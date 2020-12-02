/*
 * License-Identifier: GPL-2.0
 * Driver for UNISOC PDM r2p0
 *
 * Copyright (c) 2020 UNISOC.
 */
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt(" pdm")""fmt

#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <sound/tlv.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/io.h>
#include <linux/iopoll.h>

#include "sprd-audio.h"
#include "sprd-asoc-common.h"
#include "sprd-pdm.h"

#define SPRD_PDM_NAME    "pdm"
#define PDM_DRIVER_VERSION  "v1.0.0"

struct sprd_pdm_priv {
	struct snd_soc_codec *codec;
	struct platform_device *pdev;
	struct device *dev;
	struct regmap *regmap_ahb;
	bool module_en;
	void __iomem *regbase;
	/* start address of resource */
	unsigned long memphys;
	/* size of resource */
	unsigned int reg_size;
	/* base address after ioremap */
	unsigned long membase;
};

static inline int pdm_ioremap_reg_read(struct sprd_pdm_priv *sprd_pdm,
				       unsigned int offset)
{
	unsigned long reg;

	reg = sprd_pdm->membase + offset;
	return readl_relaxed((void *__iomem)reg);
}

static inline void pdm_ioremap_reg_raw_write(struct sprd_pdm_priv *sprd_pdm,
					     unsigned int offset, int val)
{
	unsigned long reg;

	reg = sprd_pdm->membase + offset;
	writel_relaxed(val, (void *__iomem)reg);
}

static int pdm_ioremap_reg_update(struct sprd_pdm_priv *sprd_pdm,
				  unsigned int offset, int val, int mask)
{
	int new, old;

	old = pdm_ioremap_reg_read(sprd_pdm, offset);
	new = (old & ~mask) | (val & mask);
	pdm_ioremap_reg_raw_write(sprd_pdm, offset, new);

	return old != new;
}

void pdm_module_en(struct sprd_pdm_priv *sprd_pdm, bool en)
{
	if (en) {
		agcp_ahb_reg_set(AHB_MODULE_RST0_STS, PDM_SOFT_RST);
		udelay(10);
		agcp_ahb_reg_clr(AHB_MODULE_RST0_STS, PDM_SOFT_RST);
		agcp_ahb_reg_set(AHB_MODULE_EB0_STS, PDM_EB);
		sprd_pdm->module_en = true;
	} else {
		agcp_ahb_reg_set(AHB_MODULE_RST0_STS, PDM_SOFT_RST);
		udelay(10);
		agcp_ahb_reg_clr(AHB_MODULE_RST0_STS, PDM_SOFT_RST);
		agcp_ahb_reg_clr(AHB_MODULE_EB0_STS, PDM_EB);
		sprd_pdm->module_en = false;
	}
}

static int pdm_module_en_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_pdm_priv *sprd_pdm = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sprd_pdm->module_en;

	return 0;
}

static int pdm_module_en_set(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sprd_pdm_priv *sprd_pdm = snd_soc_codec_get_drvdata(codec);

	sprd_pdm->module_en = ucontrol->value.integer.value[0];
	pdm_module_en(sprd_pdm, !!sprd_pdm->module_en);
	dev_info(sprd_pdm->dev, "set module_en %d\n", sprd_pdm->module_en);

	return 0;
}

static const struct snd_kcontrol_new pdm_snd_controls[] = {
	SOC_SINGLE_EXT("PDM MODULE EN", 0, 0, 1, 0,
		       pdm_module_en_get, pdm_module_en_set),
};

void pdm_dmic2_on(struct sprd_pdm_priv *sprd_pdm, bool on_off)
{
	unsigned int val;

	pr_info("%s on_off %d\n", __func__, on_off);
	if (on_off) {
		pdm_module_en(sprd_pdm, true);
		pdm_ioremap_reg_update(sprd_pdm, TX_CFG3, TX2_BCK_DIV_NUM(0x3),
				       TX2_BCK_DIV_NUM(0xffff));
		pdm_ioremap_reg_update(sprd_pdm, TX_CFG3,
				       TX2_LRCK_DIV_NUM(0xff),
				       TX2_LRCK_DIV_NUM(0xffff));
		/* down */
		pdm_ioremap_reg_update(sprd_pdm, ADC2_FIRST_DECIM_CFG,
				       ADC2_DECIM_1ST_DOWN_NUM(0x5),
				       ADC2_DECIM_1ST_DOWN_NUM(0x7));
		/* up, 2M */
		pdm_ioremap_reg_update(sprd_pdm, ADC2_FIRST_DECIM_CFG,
				       ADC2_DECIM_1ST_UP_NUM(0x3),
				       ADC2_DECIM_1ST_UP_NUM(0x7));

		/* 26/13 = 2M */
		pdm_ioremap_reg_update(sprd_pdm, PDM_DMIC2_DIVIDER,
				       BIT_PDM_DMIC2_DIV(0xc),
				       BIT_PDM_DMIC2_DIV(0xff));
		pdm_ioremap_reg_update(sprd_pdm, PDM_CLR, BIT_PDM_CLR,
				       BIT_PDM_CLR);

		val = IIS2_MSB | IIS2_EN | IIS2_SLOT_WIDTH(0x1);
		pdm_ioremap_reg_raw_write(sprd_pdm, IIS2_CTRL, 0x148);

		val = ADC2_DMIC_EN_R | ADC2_DMIC_EN_L;
		pdm_ioremap_reg_update(sprd_pdm, ADC0_CTRL, val, val);
		pdm_ioremap_reg_update(sprd_pdm, ADC0_CTRL, ADC2_DP_EN,
				       ADC2_DP_EN);

		val = ADC2_DECIM_FIRST_EN_R | ADC2_DECIM_FIRST_EN_L |
			ADC2_D5_TAPS25_EN_R | ADC2_D5_TAPS25_EN_L |
			ADC2_DECIM_3RD_EN_R | ADC2_DECIM_3RD_EN_L;
		pdm_ioremap_reg_update(sprd_pdm, ADC2_CTRL, val, val);
	} else {
		pdm_module_en(sprd_pdm, false);
	}
}

static const char *pdm_get_event_name(int event)
{
	const char *ev_name;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ev_name = "PRE_PMU";
		break;
	case SND_SOC_DAPM_POST_PMU:
		ev_name = "POST_PMU";
		break;
	case SND_SOC_DAPM_PRE_PMD:
		ev_name = "PRE_PMD";
		break;
	case SND_SOC_DAPM_POST_PMD:
		ev_name = "POST_PMD";
		break;
	default:
		pr_err("%s fail, invalid event 0x%x\n", __func__, event);
		return NULL;
	}

	return ev_name;
}

static int pdm_widget_event(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;

	pr_info("wname %s %s\n", w->name, pdm_get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_POST_PMD:
		break;
	default:
		pr_err("%s fail, invalid event 0x%x\n", __func__, event);
		ret = -EINVAL;
	}

	return ret;
}

static int pdm_dmic1_power_on(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *k, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_pdm_priv *sprd_pdm = snd_soc_codec_get_drvdata(codec);

	dev_info(sprd_pdm->dev, "%s wname %s, %s\n",
		__func__, w->name, pdm_get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(sprd_pdm->dev, "%s\n", __func__);
		break;
	case SND_SOC_DAPM_PRE_PMD:
	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(sprd_pdm->dev, "%s\n", __func__);
		break;
	default:
		dev_err(sprd_pdm->dev, "unknown widget event type %d\n", event);
	}

	return 0;
}

static int pdm_dmic2_power_on(struct snd_soc_dapm_widget *w,
			    struct snd_kcontrol *k, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sprd_pdm_priv *sprd_pdm = snd_soc_codec_get_drvdata(codec);

	dev_info(sprd_pdm->dev, "%s wname %s, %s\n",
		__func__, w->name, pdm_get_event_name(event));

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
	case SND_SOC_DAPM_POST_PMU:
		pdm_dmic2_on(sprd_pdm, true);
		break;
	case SND_SOC_DAPM_PRE_PMD:
	case SND_SOC_DAPM_POST_PMD:
		pdm_dmic2_on(sprd_pdm, false);
		break;
	default:
		dev_err(sprd_pdm->dev, "unknown widget event type %d\n", event);
	}

	return 0;
}

static const struct snd_kcontrol_new pdm_dmic_switch[] = {
	SOC_DAPM_SINGLE_VIRT("Switch", 1),
	SOC_DAPM_SINGLE_VIRT("Switch", 1),
	SOC_DAPM_SINGLE_VIRT("Switch", 1),
};

static const struct snd_soc_dapm_widget pdm_dapm_widgets[] = {
	SND_SOC_DAPM_ADC_E("ADC PDM DMIC", "PDM-DMIC", SND_SOC_NOPM,
		0, 0, pdm_widget_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("PDM DMIC1 ON", SND_SOC_NOPM, 0, 0, NULL, 0,
		pdm_dmic1_power_on,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH("PDM_DMIC1", SND_SOC_NOPM, 0, 1,
		&pdm_dmic_switch[1]),
	/* wsubseq should be larger than ucp1301 */
	SND_SOC_DAPM_PGA_S("PDM DMIC2 ON", 201, SND_SOC_NOPM, 0, 0,
		pdm_dmic2_power_on,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH("PDM_DMIC2", SND_SOC_NOPM, 0, 1,
		&pdm_dmic_switch[2]),
	SND_SOC_DAPM_INPUT("PDM DMIC Pin"),
};

static int pdm_soc_probe(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	struct sprd_pdm_priv *sprd_pdm = snd_soc_codec_get_drvdata(codec);

	if (!dapm) {
		dev_err(sprd_pdm->dev, "spk dapm %p, sprd_pdm %p, NULL error\n",
			dapm, sprd_pdm);
		return -EINVAL;
	}

	snd_soc_dapm_ignore_suspend(dapm, "PDM-DMIC");
	snd_soc_dapm_ignore_suspend(dapm, "PDM DMIC Pin");
	dev_info(sprd_pdm->dev, "%s\n", __func__);

	return 0;
}

static const struct snd_soc_dapm_route pdm_intercon[] = {
	/*
	{"PDM DMIC1 ON", NULL, "PDM DMIC Pin"},
	{"PDM DMIC2 ON", NULL, "PDM DMIC Pin"},
	*/

	{"PDM_DMIC1", "Switch", "PDM DMIC1 ON"},
	{"PDM_DMIC2", "Switch", "PDM DMIC2 ON"},

	/*
	{"ADC PDM DMIC", NULL, "PDM_DMIC1"},
	{"ADC PDM DMIC", NULL, "PDM_DMIC2"},
	*/
};

static struct snd_soc_codec_driver soc_codec_dev_pdm = {
	.probe = pdm_soc_probe,
	.idle_bias_off = true,
	.component_driver = {
		.controls = pdm_snd_controls,
		.num_controls = ARRAY_SIZE(pdm_snd_controls),
		.dapm_widgets = pdm_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(pdm_dapm_widgets),
		.dapm_routes = pdm_intercon,
		.num_dapm_routes = ARRAY_SIZE(pdm_intercon),
	}
};

static int pdm_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	pr_debug("%s\n", __func__);

	return 0;
}

static int pdm_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
				  unsigned int freq, int dir)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int pdm_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int pdm_trigger(struct snd_pcm_substream *substream, int cmd,
			   struct snd_soc_dai *dai)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int pdm_set_dai_mute(struct snd_soc_dai *dai, int mute)
{
	pr_debug("%s\n", __func__);
	return 0;
}

#define UNSUPPORTED_AD_RATE SNDRV_PCM_RATE_44100

#define SPRD_CODEC_PCM_RATES \
	(SNDRV_PCM_RATE_8000 |  \
	 SNDRV_PCM_RATE_11025 | \
	 SNDRV_PCM_RATE_16000 | \
	 SNDRV_PCM_RATE_22050 | \
	 SNDRV_PCM_RATE_32000 | \
	 SNDRV_PCM_RATE_44100 | \
	 SNDRV_PCM_RATE_48000 | \
	 SNDRV_PCM_RATE_96000)

#define SPRD_PDM_PCM_AD_RATES \
	(SNDRV_PCM_RATE_8000 |  \
	 SNDRV_PCM_RATE_16000 | \
	 SNDRV_PCM_RATE_32000 | \
	 UNSUPPORTED_AD_RATE | \
	 SNDRV_PCM_RATE_48000)

#define SPRD_PDM_PCM_FATMATS (SNDRV_PCM_FMTBIT_S16_LE|SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_ops pdm_dai_ops = {
	.hw_params = pdm_hw_params,
	.set_sysclk = pdm_set_dai_sysclk,
	.set_fmt = pdm_set_dai_fmt,
	.trigger = pdm_trigger,
	.digital_mute = pdm_set_dai_mute,
};

static struct snd_soc_dai_driver pdm_dai[] = {
	{
		.name = "SPRD-PDM-DMIC",
		.capture = {
			.stream_name = "PDM-DMIC",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SPRD_PDM_PCM_AD_RATES,
			.formats = SPRD_PDM_PCM_FATMATS,
		},
		.ops = &pdm_dai_ops,
	},
};

static ssize_t pdm_get_module_en(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sprd_pdm_priv *sprd_pdm = dev_get_drvdata(dev);
	ssize_t len;

	len = sprintf(buf, "get_io %d\n", sprd_pdm->module_en);

	pr_info("%s module_en %d\n", __func__, sprd_pdm->module_en);

	return len;
}

static ssize_t pdm_set_module_en(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	struct sprd_pdm_priv *sprd_pdm = dev_get_drvdata(dev);
	u32 module_en;
	int ret;

	ret = kstrtouint(buf, 10, &module_en);
	if (ret) {
		pr_err("%s fail %d, buf %s\n", __func__, ret, buf);
		return ret;
	}

	pdm_module_en(sprd_pdm, !!module_en);

	pr_info("%s module_en %d\n", __func__, sprd_pdm->module_en);

	return len;
}

static ssize_t pdm_get_read_regs(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sprd_pdm_priv *sprd_pdm = dev_get_drvdata(dev);
	u32 reg;
	ssize_t len = 0;

	for (reg = ADC0_CTRL; reg < PDM_REG_MAX; reg += 0x10) {
		len += sprintf(buf + len, "0x%04x | 0x%04x 0x%04x 0x%04x 0x%04x\n",
			(unsigned int)(reg + 0x56401000)
			, pdm_ioremap_reg_read(sprd_pdm, reg + 0x00)
			, pdm_ioremap_reg_read(sprd_pdm, reg + 0x04)
			, pdm_ioremap_reg_read(sprd_pdm, reg + 0x08)
			, pdm_ioremap_reg_read(sprd_pdm, reg + 0x0C)
		);
	}

	return len;
}

static ssize_t pdm_set_read_regs(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	struct sprd_pdm_priv *sprd_pdm = dev_get_drvdata(dev);
	u32 databuf[2];

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) != 2) {
		pr_err("%s set reg, parse data fail\n", __func__);
		return -EINVAL;
	}

	pr_info("%s set reg, reg 0x%x --> val 0x%x\n", __func__, databuf[0],
		databuf[1]);
	pdm_ioremap_reg_raw_write(sprd_pdm, databuf[0], databuf[1]);

	return len;
}

static DEVICE_ATTR(pdm_modu_en, 0660, pdm_get_module_en,
		   pdm_set_module_en);
/* do not operate registers when module_en is false */
static DEVICE_ATTR(pdm_read_regs, 0660, pdm_get_read_regs,
		   pdm_set_read_regs);

static struct attribute *pdm_attributes[] = {
	&dev_attr_pdm_modu_en.attr,
	&dev_attr_pdm_read_regs.attr,
	NULL
};

static struct attribute_group pdm_attribute_group = {
	.attrs = pdm_attributes
};

static int pdm_debug_sysfs_init(struct sprd_pdm_priv *sprd_pdm)
{
	int ret;

	ret = sysfs_create_group(&sprd_pdm->dev->kobj, &pdm_attribute_group);
	if (ret < 0)
		dev_err(sprd_pdm->dev, "fail to create sysfs attr files\n");

	return ret;
}

static int pdm_probe(struct platform_device *pdev)
{
	struct sprd_pdm_priv *sprd_pdm;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct regmap *agcp_ahb_gpr;
	struct resource *res;
	int ret = 0;

	pr_info("%s\n", __func__);
	sprd_pdm = devm_kzalloc(&pdev->dev, sizeof(struct sprd_pdm_priv),
			       GFP_KERNEL);
	if (!sprd_pdm)
		return -ENOMEM;

	agcp_ahb_gpr =
		syscon_regmap_lookup_by_phandle(np, "sprd,syscon-agcp-ahb");
	if (IS_ERR(agcp_ahb_gpr)) {
		pr_err("ERR: [%s] Get the codec aon apb syscon failed!(%ld)\n",
			__func__, PTR_ERR(agcp_ahb_gpr));
		agcp_ahb_gpr = NULL;
		return -EPROBE_DEFER;
	}

	sprd_pdm->regmap_ahb = agcp_ahb_gpr;
	arch_audio_set_agcp_ahb_gpr(agcp_ahb_gpr);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("%s pdm reg parse error!\n", __func__);
		return -EINVAL;
	}

	sprd_pdm->memphys = res->start;
	sprd_pdm->reg_size = (unsigned int)(res->end - res->start) + 1;
	sprd_pdm->membase = (unsigned long)devm_ioremap_resource(&pdev->dev,
								 res);
	if (IS_ERR_VALUE(sprd_pdm->membase)) {
		pr_err("%s ERR: pdm reg address ioremap_nocache error!\n",
		       __func__);
		return -ENOMEM;
	}

	pr_info("%s membase 0x%lx, memphys 0x%lx, end 0x%lx, reg_size 0x%x\n",
		__func__, sprd_pdm->membase, sprd_pdm->memphys,
		(unsigned long)res->end,
		sprd_pdm->reg_size);

	sprd_pdm->dev = dev;
	sprd_pdm->pdev = pdev;
	platform_set_drvdata(pdev, sprd_pdm);
	pdm_debug_sysfs_init(sprd_pdm);

	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_pdm,
				     &pdm_dai[0],
				     ARRAY_SIZE(pdm_dai));
	if (ret)
		dev_err(&pdev->dev, " %s register codec fail, %d\n", __func__,
			ret);
	else
		pr_info("%s successful\n", __func__);

	return ret;
}

static int pdm_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	snd_soc_unregister_codec(&pdev->dev);
	sysfs_remove_group(&dev->kobj, &pdm_attribute_group);

	return 0;
}

static const struct platform_device_id pdm_id[] = {
	{ SPRD_PDM_NAME, 0 },
	{ }
};

static const struct of_device_id pdm_of_match[] = {
	{ .compatible = "sprd,pdm", },
	{},
};

MODULE_DEVICE_TABLE(of, pdm_of_match);

static struct platform_driver pdm_driver = {
	.driver = {
		.name = SPRD_PDM_NAME,
		.owner = THIS_MODULE,
		.of_match_table = pdm_of_match,
	},
	.probe = pdm_probe,
	.remove = pdm_remove,
	.id_table    = pdm_id,
};

module_platform_driver(pdm_driver);

MODULE_DESCRIPTION("SPRD PDM driver");
MODULE_AUTHOR("SPRD");
MODULE_LICENSE("GPL");
