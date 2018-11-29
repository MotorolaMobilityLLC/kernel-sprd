/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt("HDST2730")""fmt

#include <asm/div64.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/iio/consumer.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/notifier.h>
#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif
#include <linux/regulator/consumer.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/nvmem-consumer.h>
#include <linux/timer.h>
#include <linux/types.h>

#include "sprd-asoc-common.h"
#include "sprd-codec.h"
#include "sprd-headset.h"

#define DEBUG_LOG pr_debug("%s %d\n", __func__, __LINE__)

#define MAX_BUTTON_NUM 6
#define SPRD_HEADSET_JACK_MASK (SND_JACK_HEADSET)
#define SPRD_BUTTON_JACK_MASK (SND_JACK_BTN_0 | SND_JACK_BTN_1 | \
	SND_JACK_BTN_2 | SND_JACK_BTN_3 | SND_JACK_BTN_4)

#define ADC_READ_REPET (40)
#define CHIP_ID_2720 0x2720
#define CHIP_ID_2730 0x2730

#define SCI_ADC_GET_VALUE_COUNT  (10)
#define ABS(x) (((x) < (0)) ? (-(x)) : (x))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#define headset_reg_read(reg, val) \
	sci_adi_read(CODEC_REG((reg)), val)

#define headset_reg_write(reg, val, mask) \
	sci_adi_write(CODEC_REG((reg)), (val), (mask))

#define headset_reg_write_force(reg, val, mask) \
	sci_adi_write_force(CODEC_REG((reg)), (val), (mask))

#define headset_reg_clr_bits(reg, bits) \
	sci_adi_clr(CODEC_REG((reg)), (bits))

#define headset_reg_set_bits(reg, bits) \
	sci_adi_set(CODEC_REG((reg)), (bits))

unsigned int headset_reg_value_read(unsigned int reg)
{
	unsigned int ret_val;

	sci_adi_read(CODEC_REG((reg)), &ret_val);

	return ret_val;
}

#define LG "%s STS0(184) %x, INT5(214.IEV) %x, INT6(218.IE) %x, INT7(21C.RIS) %x, INT8(220.MIS) %x, INT11(22C.STS1) %x, INT32(280.INTC.EN) %x, INT34(288) %x, INT35(28C) %x"

#define FC __func__
#define S0 headset_reg_value_read(ANA_STS0)
#define T5 headset_reg_value_read(ANA_INT5)
#define T6 headset_reg_value_read(ANA_INT6)
#define T7 headset_reg_value_read(ANA_INT7)
#define T8 headset_reg_value_read(ANA_INT8)
#define T11 headset_reg_value_read(ANA_INT11)
#define T32 headset_reg_value_read(ANA_INT32)
#define T34 headset_reg_value_read(ANA_INT34)
#define T35 headset_reg_value_read(ANA_INT35)

int dsp_fm_mute_by_set_dg(void)
	__attribute__ ((weak, alias("__dsp_fm_mute_by_set_dg")));

static int __dsp_fm_mute_by_set_dg(void)
{
	pr_err("ERR: dsp_fm_mute_by_set_dg is not defined!\n");
	return -1;
}

static inline int headset_reg_get_bits(unsigned int reg, int bits)
{
	unsigned int temp;
	int ret;

	ret = sci_adi_read(CODEC_REG(reg), &temp);
	if (ret) {
		pr_err("%s: read reg#%#x failed!\n", __func__, reg);
		return ret;
	}
	temp = temp & bits;

	return temp;
}

enum sprd_headset_type {
	HEADSET_4POLE_NORMAL,
	HEADSET_NO_MIC,
	HEADSET_4POLE_NOT_NORMAL,
	HEADSET_APPLE,
	HEADSET_TYPE_ERR = -1,
};

struct sprd_headset_auxadc_cal_l {
	u32 A;
	u32 B;
	u32 E1;
	u32 E2;
	u32 cal_type;
};

#define SPRD_HEADSET_AUXADC_CAL_NO 0
#define SPRD_HEADSET_AUXADC_CAL_DO 1

static struct sprd_headset_auxadc_cal_l adc_cal_headset = {
	0, 0, 0, 0, SPRD_HEADSET_AUXADC_CAL_NO,
};

static struct sprd_headset *sprd_hdst;
static bool fast_charge_finished;

/* ========================  audio codec  ======================== */

int vbc_close_fm_dggain(bool mute)
	__attribute__ ((weak, alias("__vbc_close_fm_dggain")));
static int __vbc_close_fm_dggain(bool mute)
{
	pr_err("ERR: vbc_close_fm_dggain is not defined!\n");
	return -1;
}

/*
 * When remove headphone, disconnect the headphone
 * dapm DA path in codec driver.
 */
static int dapm_jack_switch_control(struct snd_soc_codec *codec, bool on)
{
	struct snd_kcontrol *kctrl;
	struct snd_soc_card *card = codec ? codec->component.card : NULL;
	struct snd_ctl_elem_id id = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Virt HP Jack Switch",
	};
	struct snd_ctl_elem_value ucontrol = {
		.value.integer.value[0] = on,
	};

	if (!card) {
		pr_err("%s card is NULL!\n", __func__);
		return -1;
	}

	pr_info("%s, %s\n", __func__, on ? "on" : "off");
	kctrl = snd_ctl_find_id(card->snd_card, &id);
	if (!kctrl) {
		pr_err("%s can't find kctrl '%s'\n", __func__, id.name);
		return -1;
	}

	return snd_soc_dapm_put_volsw(kctrl, &ucontrol);
}

static void headset_jack_report(struct sprd_headset *hdst,
	struct snd_soc_jack *jack, int status, int mask)
{
	if (mask & SND_JACK_HEADPHONE)
		dapm_jack_switch_control(hdst->codec, !!status);

	snd_soc_jack_report(jack, status, mask);
}

static enum snd_jack_types headset_jack_type_get(int index)
{
	enum snd_jack_types jack_type_map[MAX_BUTTON_NUM] = {
		SND_JACK_BTN_0, SND_JACK_BTN_1, SND_JACK_BTN_2,
		SND_JACK_BTN_3, SND_JACK_BTN_4, SND_JACK_BTN_5
	};

	return jack_type_map[index];
}

static int sprd_headset_power_get(struct device *dev,
				struct regulator **regu, const char *id)
{
	struct regulator *regu_ret;

	if (!*regu) {
		*regu = regulator_get(dev, id);
		if (IS_ERR(*regu)) {
			pr_err("ERR:Failed to request %ld: %s\n",
				PTR_ERR(*regu), id);
			regu_ret = *regu;
			*regu = 0;
			return PTR_ERR(regu_ret);
		}
	}

	return 0;
}

static int sprd_headset_power_init(struct sprd_headset *hdst)
{
	struct platform_device *pdev = hdst->pdev;
	struct device *dev;
	struct sprd_headset_power *power = &hdst->power;
	int ret;

	if (!pdev) {
		pr_err("%s: codec is null!\n", __func__);
		return -1;
	}
	dev = &pdev->dev;
	ret = sprd_headset_power_get(dev, &power->head_mic, "HEADMICBIAS");
	if (ret || (power->head_mic == NULL)) {
		power->head_mic = 0;
		return ret;
	}
	regulator_set_voltage(power->head_mic, 950000, 950000);

	ret = sprd_headset_power_get(dev, &power->bg, "BG");
	if (ret) {
		power->bg = 0;
		goto __err1;
	}

	ret = sprd_headset_power_get(dev, &power->bias, "BIAS");
	if (ret) {
		power->bias = 0;
		goto __err2;
	}

	goto __ok;

__err2:
	regulator_put(power->bg);
__err1:
	regulator_put(power->head_mic);
__ok:
	return ret;
}

static int sprd_headset_power_regulator_init(struct sprd_headset *hdst)
{
	struct platform_device *pdev = hdst->pdev;
	struct device *dev;
	struct sprd_headset_power *power = &hdst->power;
	int ret;

	if (!pdev) {
		pr_err("%s: codec is null!\n", __func__);
		return -1;
	}
	dev = &pdev->dev;
	ret = sprd_headset_power_get(dev, &power->vb, "VB");
	if (ret || (power->vb == NULL)) {
		power->vb = 0;
		return ret;
	}
	return sprd_headset_power_init(hdst);
}

void sprd_headset_power_deinit(void)
{
	struct sprd_headset_power *power = &sprd_hdst->power;

	regulator_put(power->head_mic);
	regulator_put(power->bg);
	regulator_put(power->bias);
}

static int sprd_headset_audio_block_is_running(struct sprd_headset *hdst)
{
	struct sprd_headset_power *power = &hdst->power;

	return (regulator_is_enabled(power->bg) &&
		regulator_is_enabled(power->bias));
}

static int sprd_headset_headmic_bias_control(
	struct sprd_headset *hdst, int on)
{
	struct sprd_headset_power *power = &hdst->power;
	int ret;

	if (!power->head_mic)
		return -1;

	if (on)
		ret = regulator_enable(power->head_mic);
	else
		ret = regulator_disable(power->head_mic);
	if (!ret) {
		/* Set HEADMIC_SLEEP when audio block closed */
		if (sprd_headset_audio_block_is_running(hdst))
			ret = regulator_set_mode(
				power->head_mic, REGULATOR_MODE_NORMAL);
		else
			ret = regulator_set_mode(
				power->head_mic, REGULATOR_MODE_STANDBY);
	}

	return ret;
}

static int sprd_headset_bias_control(
	struct sprd_headset *hdst, int on)
{
	struct sprd_headset_power *power = &hdst->power;
	int ret;

	if (!power->head_mic)
		return -1;

	pr_info("%s bias set %d\n", __func__, on);
	if (on)
		ret = regulator_enable(power->bias);
	else
		ret = regulator_disable(power->bias);

	return ret;
}
static int sprd_headset_vb_control(
	struct sprd_headset *hdst, int on)
{
	struct sprd_headset_power *power = &hdst->power;
	static int state;
	int ret = 0;

	if (!power->vb)
		return -1;

	pr_info("%s bias set %d\n", __func__, on);
	if (on) {
		if (state == 0) {
			ret = regulator_enable(power->vb);
			state = 1;
		}
	} else
		ret = regulator_disable(power->vb);

	return ret;
}


static BLOCKING_NOTIFIER_HEAD(hp_chain_list);
int headset_register_notifier(struct notifier_block *nb)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return 0;
	}

	if (pdata->jack_type == JACK_TYPE_NO) {
		nb = NULL;
		return 0;
	}

	return blocking_notifier_chain_register(&hp_chain_list, nb);
}
EXPORT_SYMBOL(headset_register_notifier);

int headset_unregister_notifier(struct notifier_block *nb)
{
	if (nb == NULL)
		return -1;

	return blocking_notifier_chain_unregister(&hp_chain_list, nb);
}
EXPORT_SYMBOL(headset_unregister_notifier);

int headset_get_plug_state(void)
{
	struct sprd_headset *hdst = sprd_hdst;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return 0;
	}

	return !!hdst->plug_state_last;
}
EXPORT_SYMBOL(headset_get_plug_state);

static int headset_wrap_sci_adc_get(struct iio_channel *chan)
{
	int count = 0, average = 0, val, ret;
	struct sprd_headset *hdst = sprd_hdst;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return 0;
	}

	if (!chan) {
		pr_err("%s: iio_channel is NULL\n", __func__);
		return 0;
	}

	while (count < SCI_ADC_GET_VALUE_COUNT) {
		ret = iio_read_channel_raw(chan, &val);
		if (ret < 0) {
			pr_err("%s: read adc raw value failed!\n", __func__);
			return 0;
		}
		average += val;
		count++;
		pr_debug("%s count %d,val %d, average %d\n",
			__func__, count, val, average);
	}

	average /= SCI_ADC_GET_VALUE_COUNT;

	pr_debug("%s: adc: %d\n", __func__, average);

	return average;
}

static void headset_detect_clk_en(void)
{
	sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN, BIT_ANA_AUD_EN);
	/* enable EIC module */
	sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN, BIT(3));
	/* enable efuse module */
	sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN, BIT(6));
	/* enable RTC_EIC_EN, neo.hou, 20181101 */
	sci_adi_set(ANA_REG_GLB_RTC_CLK_EN0, BIT(3));
	/* bandgap, si.chen ask to remove on 20181026 */
	/* headset_reg_set_bits(ANA_PMU0, BG_EN);*/
	/* si.chen ask to add, 20181026 */
	headset_reg_clr_bits(ANA_PMU0, BG_EN);
	/* si.chen ask to add, enable this all the time after bootup */
	headset_reg_set_bits(ANA_PMU0, VB_EN);
	headset_reg_set_bits(ANA_DCL1, DCL_EN);
	headset_reg_set_bits(ANA_CLK0, CLK_DCL_32K_EN);
	headset_reg_set_bits(ANA_DCL1, DIG_CLK_INTC_EN);

	pr_debug("%s ANA_REG_GLB_ARM_MODULE_EN(glb 0008) %x, PMU0(0000) %x, DCL1(0100) %x, CLK0(0068) %x\n",
		__func__, headset_reg_value_read(ANA_REG_GLB_ARM_MODULE_EN),
		headset_reg_value_read(ANA_PMU0),
		headset_reg_value_read(ANA_DCL1),
		headset_reg_value_read(ANA_CLK0));
}

static void headset_eic_intc_clear(int clear_mark)
{
	if (clear_mark == 1) {
		/* daniel say need 2ms before and after */
		usleep_range(2000, 2500);
		/* clear bit14 of reg 0x288, reg 0x28C, clear all analog intc */
		headset_reg_write_force(ANA_INT33, 0x4000, 0xFFFF);
		usleep_range(2000, 2500);
	} else if (clear_mark == 0) {
		usleep_range(2000, 2500);
		/*set bit14 to 0, equal to enable intc */
		headset_reg_write_force(ANA_INT33, 0x0, 0xFFFF);
		usleep_range(2000, 2500);
	}
	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);
}

void headset_eic_intc_clear_test(void)
{
	headset_eic_intc_clear(0);
}
extern void headset_eic_intc_clear_test(void);

static void headset_eic_clear_irq(uint irq_bit)
{
	if (irq_bit == HDST_EIC_MAX) {
		/* clear all */
		headset_reg_write(ANA_INT9, EIC_DBNC_IC(0xFFFF),
		EIC_DBNC_IC(0xFFFF));
		pr_info("%s clear all internal eic\n", __func__);
	} else if (irq_bit != 0) {
		/* clear reg0x21c, reg 0x220 */
		headset_reg_set_bits(ANA_INT9, BIT(irq_bit));
		pr_info("%s clear irq_bit %d\n", __func__, irq_bit);
	}
	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);
}

static void headset_eic_trig_irq(uint irq_bit)
{
	headset_reg_set_bits(ANA_INT10, BIT(irq_bit));
	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);
}

static void headset_eic_enable(uint irq_bit, int enable)
{
	if ((irq_bit == HDST_EIC_MAX) && (enable == 0)) {
		headset_reg_write(ANA_INT6, EIC_DBNC_IE(0x0000),
			EIC_DBNC_IE(0xFFFF));
		pr_info("%s disable all internal eic\n", __func__);
	} else if  ((irq_bit == HDST_EIC_MAX) && (enable == 1)) {
		headset_reg_write(ANA_INT6, EIC_DBNC_IE(0xFF00),
			EIC_DBNC_IE(0xFFFF));
		pr_info("%s enable all internal eic\n", __func__);
	} else if (enable == 0) {
		pr_info("%s disable irq_bit %d\n", __func__, irq_bit);
		headset_reg_clr_bits(ANA_INT6, BIT(irq_bit));
	} else if (enable == 1) {
		pr_info("%s enable irq_bit %d\n", __func__, irq_bit);
		headset_reg_set_bits(ANA_INT6, BIT(irq_bit));
	}
	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);
}

static void headset_internal_eic_entry_init(void)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);

	/* clear reg 0x21c, reg 0x220, clear all eic irq */
	headset_eic_clear_irq(16);

	if (pdata->jack_type == JACK_TYPE_NO) {
		headset_eic_enable(12, 1);/* enalbe ldetl */
		headset_eic_trig_irq(12);/* trig */
	} else if (pdata->jack_type == JACK_TYPE_NC) {
		headset_eic_enable(10, 1);/* enalbe detect_all */
		headset_eic_trig_irq(10);/* trig */
	}
	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);
}

static unsigned int headset_eic_get_insert_status(unsigned int bit_check)
{
	unsigned int bit_status;
	/* headset insert */
	bit_status = (BIT(15) & headset_reg_value_read(ANA_STS0)) > 0;
	if (bit_check == 15) {
		pr_info("%s headphone %s insert\n", __func__,
			bit_status ? "is" : "not");
		return bit_status;
		}
	/* headphone is inserted and check other bit status */
	if ((bit_status == 1) && (bit_check != 15)) {
		bit_status =
			(BIT(bit_check) & headset_reg_value_read(ANA_STS0)) > 0;
		pr_info("%s headphone is insert, %d bit is %s\n", __func__,
			bit_check, bit_status ? "high" : "low");
		return bit_status;
	}
	pr_info("%s headphone not insert, return 0 instead\n",
		__func__);
	return 0;
}

static unsigned int headset_eic_get_irq_status(unsigned int bit_irq)
{
	unsigned int bit_status;

	bit_status =
		(BIT(bit_irq) & headset_reg_value_read(ANA_INT8)) > 0;
	pr_info("%s eic irq %d %s active\n", __func__,
		bit_irq, bit_status ? "is" : "not");
	return bit_status;
}

static void headset_eic_set_trig_level(uint irq_bit, uint trig_level)
{
	unsigned int last_trig_level;
	struct sprd_headset *hdst = sprd_hdst;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}
	if (trig_level != 1 && trig_level != 0) {
		pr_err("%s: irq trig type error %d\n", __func__, trig_level);
		return;
	}

	if (irq_bit == HDST_EIC_MAX) {
		pr_info("%s set all internal eic trig level to high\n",
			__func__);
		/* set IEV to high, the ori is high */
		headset_reg_write(ANA_INT5, EIC_DBNC_IEV(0xFFFF),
			EIC_DBNC_IEV(0xFFFF));
		return;
	}

	last_trig_level = (BIT(irq_bit) & headset_reg_value_read(ANA_INT5)) > 0;
	if (last_trig_level == trig_level) {
		pr_info("%s irq_bit %d trig level is %s already, needn't to set\n",
			__func__, irq_bit, last_trig_level ? "high" : "low");
		return;
	}
	pr_info("%s set irq_bit %d trig level to %s\n",
			__func__, irq_bit, trig_level ? "high" : "low");
	if (trig_level == 1)
		headset_reg_set_bits(ANA_INT5, BIT(irq_bit));
	else if (trig_level == 0)
		headset_reg_clr_bits(ANA_INT5, BIT(irq_bit));
}

static unsigned int headset_eic_get_trig_level(uint bit_check)
{
	unsigned int bit_status;

	bit_status =
		(BIT(bit_check) & headset_reg_value_read(ANA_INT5)) > 0;
	pr_info("%s bit %d trig level is %s\n", __func__,
		bit_check, bit_status ? "high" : "low");
	return bit_status;
}

static unsigned int headset_eic_get_irq_data(uint bit_check)
{
	unsigned int bit_status;

	bit_status =
		(BIT(bit_check) & headset_reg_value_read(ANA_INT0)) > 0;
	pr_info("%s bit %d is %s\n", __func__, bit_check,
		bit_status ? "high" : "low");
	return bit_status;
}

static void headset_detect_reg_init(void)
{
	headset_detect_clk_en();
	/* detect ref enable */
	headset_reg_set_bits(ANA_HDT0, HEDET_VREF_EN);
	headset_reg_set_bits(ANA_HDT2, HEDET_LDETL_EN);
	headset_reg_set_bits(ANA_HDT2, HEDET_LDETH_EN);
	headset_reg_set_bits(ANA_HDT0, HEDET_GDET_EN);
	headset_reg_set_bits(ANA_HDT2, HEDET_MDET_EN);
	headset_reg_set_bits(ANA_HDT1, HEDET_PLGPD_EN);

	/* enable INTC bit14(irq 14) */
	headset_reg_set_bits(ANA_INT32, ANA_INT_EN);
	/* EIC_DBNC_DATA register can be read if EIC_DBNC_DMSK set 1 */
	headset_reg_write(ANA_INT1, EIC_DBNC_DMSK(0xFFFF),
		EIC_DBNC_DMSK(0xFFFF));
	/* for polling */
	headset_reg_write(ANA_HID4, HID_TMR_T2(0x1),
		HID_TMR_T2(0xFFFF));
	pr_debug("%s HDT0(00D0) %x, HDT2(00D8) %x, HDT1(00D4) %x, INT32(0280) %x, INT1(0204) %x, INT6(0218) %x, INT0(0200) %x\n",
		__func__, headset_reg_value_read(ANA_HDT0),
		headset_reg_value_read(ANA_HDT2),
		headset_reg_value_read(ANA_HDT1),
		headset_reg_value_read(ANA_INT32),
		headset_reg_value_read(ANA_INT1),
		headset_reg_value_read(ANA_INT6),
		headset_reg_value_read(ANA_INT0));

	/* clear reg 0x21c, reg 0x220, clear all eic irq */
	headset_eic_clear_irq(HDST_EIC_MAX);
	headset_reg_write(ANA_INT26, EIC10_DBNC_CTRL(0x4002),
		EIC10_DBNC_CTRL(0xFFFF));
	headset_reg_write(ANA_INT27, EIC11_DBNC_CTRL(0x4002),
		EIC11_DBNC_CTRL(0xFFFF));
	headset_reg_write(ANA_INT28, EIC12_DBNC_CTRL(0x4002),
		EIC12_DBNC_CTRL(0xFFFF));
	headset_reg_write(ANA_INT29, EIC13_DBNC_CTRL(0x4002),
		EIC13_DBNC_CTRL(0xFFFF));
	headset_reg_write(ANA_INT30, EIC14_DBNC_CTRL(0x4002),
		EIC14_DBNC_CTRL(0xFFFF));
	headset_reg_write(ANA_INT31, EIC15_DBNC_CTRL(0x4002),
		EIC15_DBNC_CTRL(0xFFFF));

	headset_eic_set_trig_level(16, 1);
	headset_eic_enable(16, 0);
	headset_eic_intc_clear(1);
	headset_eic_intc_clear(0);

	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);

	/*
	 * according to luting, only for test, this will be
	 * removed after haps test, 20181026
	 */
	headset_reg_set_bits(ANA_PMU0, BIAS_EN);
	/*
	 * according to luting, only for test, this will be
	 * removed after haps test, 20181026
	 */
	headset_reg_set_bits(ANA_PMU1, HMIC_BIAS_EN);
}

static void headset_scale_set(int large_scale)
{
	if (large_scale)
		headset_reg_set_bits(ANA_HDT3, HEDET_V2AD_SCALE_SEL);
	else
		headset_reg_clr_bits(ANA_HDT3, HEDET_V2AD_SCALE_SEL);
}

static void headset_button_irq_threshold(int enable)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	int audio_head_sbut;
	unsigned long msk, val;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}

	audio_head_sbut = pdata->irq_threshold_button;
	msk = HEDET_BDET_REF_SEL(0x3);
	/*
	 * according to si.chen's email, it is set in initial, we don't to
	 * set or care this, (so here use default value, 0.8V)
	 */
	val = enable ? HEDET_BDET_REF_SEL(audio_head_sbut) : 0x7;
	headset_reg_write(ANA_HDT0, val, msk);
	if (enable)
		headset_reg_set_bits(ANA_HDT0, HEDET_BDET_EN);
	else
		headset_reg_clr_bits(ANA_HDT0, HEDET_BDET_EN);
}

static void headmicbias_power_on(struct sprd_headset *hdst, int on)
{
	static int current_power_state;
	struct sprd_headset_platform_data *pdata;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}

	pdata = &hdst->pdata;

	if (on == 1) {
		if (current_power_state == 0) {
			if (pdata->external_headmicbias_power_on != NULL)
				pdata->external_headmicbias_power_on(1);
			sprd_headset_headmic_bias_control(hdst, 1);
			current_power_state = 1;
		}
	} else {
		if (current_power_state == 1) {
			if (pdata->external_headmicbias_power_on != NULL)
				pdata->external_headmicbias_power_on(0);
			sprd_headset_headmic_bias_control(hdst, 0);
			current_power_state = 0;
		}
	}
}

static int headset_adc_get_ideal(u32 adc_mic, u32 coefficient);

static int headset_get_adc_value(struct iio_channel *chan)
{
	int adc_value;

	/* head buffer not swap */
	headset_reg_clr_bits(ANA_HDT3, HEDET_V2AD_SWAP);
	pr_info("%s, not swap ANA_HDT3(00DC) %x\n",
		__func__, headset_reg_value_read(ANA_HDT3));
	adc_value = headset_wrap_sci_adc_get(chan);
	/* head buffer swap input */
	headset_reg_set_bits(ANA_HDT3, HEDET_V2AD_SWAP);
	pr_info("%s,is swap ANA_HDT3(00DC) %x\n",
		__func__, headset_reg_value_read(ANA_HDT3));
	adc_value = headset_wrap_sci_adc_get(chan) + adc_value;
	pr_info("%s, adc_value is %d\n", __func__, adc_value/2);

	return adc_value/2;
}

/*
 * I hope this func called by sprd_codec_soc_suspend -- true
 * sprd_codec_soc_resume -- false
 * or called by the value of sprd_codec->
 * startup_cnt sprd_codec_pcm_hw_startup/
 * sprd_codec_pcm_hw_shutdown
 * call with false before headset plug out? after headset plug in,
 * how to set polling, and only headset need polling,
 * headphone not need???
 */
void headset_hmicbias_polling_enable(bool enable, bool force_disable)
{
	/*
	 * I am not sure how to make the init value of polling
	 *  is right here, I think set to false when probe
	 */
	static int current_polling_state = 1;
	struct sprd_headset *hdst = sprd_hdst;

	if (!hdst) {
		pr_err("%s sprd_hdset is NULL!\n", __func__);
		return;
	}
	if (force_disable) {
		headset_reg_clr_bits(ANA_HID0, HID_EN);
		headset_reg_clr_bits(ANA_DCL1, DIG_CLK_HID_EN);
		pr_info("%s force to disable polling\n", __func__);
	}
	if (hdst->plug_state_last == 0) {
		pr_err("%s no headset insert!\n", __func__);
		return;
	}
	if ((hdst->hdst_status & SND_JACK_MICROPHONE) == 0) {
		pr_err("%s no headset plugin, hdst_status 0x%x\n",
			__func__, hdst->hdst_status);
		return;
	}

	pr_info("%s set polling to %s: DCL1(0100) %x, CLK0(0068) %x, HID0(0144) %x\n",
		__func__, enable ? "enable" : "disable",
		headset_reg_value_read(ANA_DCL1),
		headset_reg_value_read(ANA_CLK0),
		headset_reg_value_read(ANA_HID0));

	mutex_lock(&hdst->hmicbias_polling_lock);
	if (enable == 1) {
		if (current_polling_state == 0) {
			headset_reg_set_bits(ANA_DCL1, DCL_EN);
			headset_reg_set_bits(ANA_CLK0, CLK_DCL_32K_EN);
			headset_reg_set_bits(ANA_DCL1, DIG_CLK_HID_EN);
			headset_reg_write(ANA_HID0, HID_DBNC_EN(0x3),
				HID_DBNC_EN(0x3));
			headset_reg_set_bits(ANA_HID0, HID_EN);
			current_polling_state = 1;
		}
	} else {
		if (current_polling_state == 1) {
			headset_reg_clr_bits(ANA_HID0, HID_EN);
			headset_reg_clr_bits(ANA_DCL1, DIG_CLK_HID_EN);
			current_polling_state = 0;
		}
	}
	mutex_unlock(&hdst->hmicbias_polling_lock);
	pr_info("%s polling: DCL1(0100) %x, CLK0(0068) %x, HID0(0144) %x\n",
		__func__, headset_reg_value_read(ANA_DCL1),
		headset_reg_value_read(ANA_CLK0),
		headset_reg_value_read(ANA_HID0));
}

static enum sprd_headset_type
headset_type_detect_through_mdet(void)
{
	struct sprd_headset *hdst = sprd_hdst;
	enum sprd_headset_type headset_type;
	/*
	 * according to si.chen's email, need 100~200ms at least to
	 * wait now for try
	 */
	/* int check_times = 20; */
	int check_times = 600;/* only used in test, haps is very slow */

	pr_info("%s enter\n", __func__);
	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);

	/* need to disable/enable irq 10 in this func? i am not sure */
	headset_eic_enable(10, 0);
	headset_eic_clear_irq(10);
	headset_eic_enable(11, 1);/* enalbe mdet */
	headset_eic_trig_irq(11);/* trig mdet */
	headset_eic_intc_clear(0);
	headset_reg_clr_bits(ANA_HDT2, HEDET_LDETL_FLT_EN);
	while (check_times > 0) {
		if (hdst->mic_irq_trigged == 1)
			break;
		check_times--;
		sprd_msleep(10);
	}
	if (check_times == 0)
		headset_type = HEADSET_NO_MIC;
	else
		headset_type = HEADSET_4POLE_NORMAL;

	headset_eic_enable(10, 1);
	headset_eic_enable(11, 0);/* disalbe mdet */
	headset_eic_clear_irq(11);
	hdst->mic_irq_trigged = 0;
	pr_info("%s, headset_type = %d (%s), check_times %d\n",
		__func__, headset_type,
		(headset_type == HEADSET_NO_MIC) ?
		"HEADSET_NO_MIC" : "HEADSET_4POLE_NORMAL", check_times);
	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);

	return headset_type;
}

static enum sprd_headset_type
headset_type_detect_all(int insert_all_val_last)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata;
	int adc_mic_average, adc_mic_ideal, adc_left_average,
		adc_left_ideal, val;
	struct iio_channel *adc_chan = hdst->adc_chan;

	DEBUG_LOG;

	if (!hdst || !adc_chan) {
		pr_err("%s: sprd_hdset(%p) or adc_chan(%p) is NULL!\n",
			__func__, hdst, adc_chan);
		return HEADSET_TYPE_ERR;
	}

	pdata = &hdst->pdata;
	if (pdata->gpio_switch != 0)
		gpio_direction_output(pdata->gpio_switch, 0);
	else
		pr_info("automatic type switch is unsupported\n");

	/*
	 *I am not sure, if need this func here, need to improve the func,
	 *  too many reg should not be set here
	 */
	headset_detect_clk_en();

	/*
	 * power up HMICBIAS, the begin of setp 5
	 * according to guideline, doc not refer,
	 * seems need this, I am not sure
	 */
	/* headset_reg_set_bits(ANA_PMU1, HMIC_BIAS_VREF_SEL); */
	headset_reg_set_bits(ANA_PMU0, VB_SLEEP_EN);
	headset_reg_set_bits(ANA_PMU0, VB_EN);
	headset_reg_set_bits(ANA_PMU1, HMIC_BIAS_SOFT_EN);
	headset_reg_set_bits(ANA_PMU1, HMIC_BIAS_SLEEP_EN);
	headset_reg_set_bits(ANA_PMU1, HMIC_BIAS_EN);
	/*
	 * changing to 4ms according to si.chen's email,
	 * make sure the whole time is in 10ms
	 */
	sprd_msleep(4);

	pr_info("%s, get adc value of headmic in little scale\n", __func__);

	headset_reg_set_bits(ANA_HDT3, HEDET_V2AD_EN);
	/* 0 little, 1 large */
	headset_reg_clr_bits(ANA_HDT3, HEDET_V2AD_SCALE_SEL);
	headset_reg_write(ANA_HDT3, HEDET_V2AD_CH_SEL(0),
		HEDET_V2AD_CH_SEL(0xF));
	adc_mic_average = headset_get_adc_value(adc_chan);
	adc_mic_ideal = headset_adc_get_ideal(adc_mic_average,
						pdata->coefficient);
	pr_info("%s, adc_value: adc_mic_average %d, ideal_value: adc_mic_ideal %d\n",
		__func__, adc_mic_average, adc_mic_ideal);
	if (adc_mic_ideal >= 0)
		adc_mic_average = adc_mic_ideal;

	if (pdata->jack_type == JACK_TYPE_NO)
		headset_reg_write(ANA_HDT3, HEDET_V2AD_CH_SEL(0x4),
			HEDET_V2AD_CH_SEL(0xF));
	else if (pdata->jack_type == JACK_TYPE_NC)
		headset_reg_write(ANA_HDT3, HEDET_V2AD_CH_SEL(0x5),
			HEDET_V2AD_CH_SEL(0xF));

	adc_left_average = headset_get_adc_value(adc_chan);
	/* si.chen say here could calculate voltage like mic */
	adc_left_ideal = headset_adc_get_ideal(adc_left_average,
			pdata->coefficient);
	pr_info("%s adc_value: adc_left_average = %d, ideal_value: adc_left_ideal %d\n",
		__func__, adc_left_average, adc_left_ideal);
	if (-1 == adc_left_ideal)
		return HEADSET_TYPE_ERR;

	pr_info("%s sprd_one_half_adc_gnd %d, adc_threshold_3pole_detect %d,sprd_adc_gnd %d\n",
		__func__, pdata->sprd_one_half_adc_gnd,
		pdata->adc_threshold_3pole_detect, pdata->sprd_adc_gnd);

	if (adc_left_ideal <= pdata->sprd_one_half_adc_gnd) {
		/* (2) */
		if (adc_mic_average <= pdata->adc_threshold_3pole_detect)
			return HEADSET_NO_MIC;
		/* (3) */
		if (adc_mic_average > pdata->adc_threshold_3pole_detect) {
			/*
			 *4 pole normal type is divided into 4 types:
			 * A: 4 pole normal headphone,
			 * B: 4 pole normal for selfie stick,
			 * C: 4 pole normal which is not totally inserted with
			 * MIC floating, can be regarded as 3 pole headphone,
			 * D: 4 pole normal for selfie stick which is not
			 * totally inserted or it is 4 pole floating.
			 */
			val =
			headset_eic_get_insert_status(HDST_INSERT_BIT_MDET);
			pr_info("%s %d, adc_left_ideal %d, adc_mic_average %d, val %d\n",
				__func__, __LINE__, adc_left_ideal,
				adc_mic_average, val);
			/* type A */
			if (val != 0 && adc_left_ideal < pdata->sprd_adc_gnd)
				return HEADSET_4POLE_NORMAL;
			/* type B */
			if (val != 0 && adc_left_ideal > pdata->sprd_adc_gnd)
				return HEADSET_4POLE_NORMAL;
			/* type C */
			if (val == 0 && adc_left_ideal < pdata->sprd_adc_gnd)
				return headset_type_detect_through_mdet();
			/* type D */
			if (val == 0 && adc_left_ideal > pdata->sprd_adc_gnd)
				return headset_type_detect_through_mdet();
		}
	} else if  (ABS(adc_mic_average - adc_left_ideal) <
	/* need using ABS? the document don't refer this, I am not sure */
						pdata->sprd_adc_gnd)
		return HEADSET_4POLE_NOT_NORMAL;/* (1) */
	else
		return HEADSET_TYPE_ERR;/* (1) */

	return HEADSET_TYPE_ERR;
}

static void headset_button_release_verify(void)
{
	struct sprd_headset *hdst = sprd_hdst;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}

	if (hdst->btn_state_last == 1) {
		headset_jack_report(hdst, &hdst->btn_jack,
			0, hdst->btns_pressed);
		hdst->btn_state_last = 0;
		pr_info("%s headset button released by force!!! current button: %#x\n",
			__func__, hdst->btns_pressed);
		hdst->btns_pressed &= ~SPRD_BUTTON_JACK_MASK;
		headset_eic_set_trig_level(15, 1);
	}
}

static enum snd_jack_types headset_adc_to_button(int adc_mic)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	struct headset_buttons *hdst_btns =
		(pdata ? pdata->headset_buttons : NULL);
	int i, nb = (pdata ? pdata->nbuttons : 0);
	enum snd_jack_types j_type = KEY_RESERVED;

	if (!hdst || !hdst_btns) {
		pr_err("%s: sprd_hdst(%p) or hdst_btns(%p) is NULL!\n",
			__func__, sprd_hdst, hdst_btns);
		return KEY_RESERVED;
	}

	for (i = 0; i < nb; i++) {
		if (adc_mic >= hdst_btns[i].adc_min &&
			adc_mic < hdst_btns[i].adc_max) {
			j_type = headset_jack_type_get(i);
			pr_info("%s adc_mic %d, j_type 0x%x\n",
				__func__, adc_mic, j_type);
			break;
		}
	}
	pr_info("%s adc_mic %d, j_type 0x%x, i %d, nb %d\n",
		__func__, adc_mic, j_type, i, nb);

	return j_type;
}

static void headset_mic_work_func(struct work_struct *work)
{
	struct sprd_headset *hdst = sprd_hdst;
	int val, mdet_insert_status;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}
	val = headset_eic_get_irq_status(11);
	if (val == 0) {
		pr_info("%s fatal error, irq 11 invalid, INT8(220.MIS) %x\n",
			__func__, val);
		return;
	}

	pr_info("%s in, mic irq status %d\n", __func__,
		headset_eic_get_irq_status(HDST_EIC_MDET));
	mdet_insert_status =
		headset_eic_get_insert_status(HDST_INSERT_BIT_MDET);
	if (mdet_insert_status != hdst->mdet_val_last) {
		pr_info("%s check debounce failed\n", __func__);
		return;
	}
	/* 0==mic irq not triggered, 1==mic triggered */
	if (mdet_insert_status == 1)
		hdst->mic_irq_trigged = 1;
	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);

	headset_eic_enable(11, 0);
	headset_eic_clear_irq(11);
	headset_eic_intc_clear(0);

	headset_reg_read(ANA_STS0, &val);
	pr_info("%s: mic_irq_trigged %d, ANA_STS0 = 0x%x\n",
		__func__, hdst->mic_irq_trigged, val);
}

static void headset_button_work_func(struct work_struct *work)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	int button_bit_value_current, adc_mic_average, btn_irq_trig_level,
		adc_ideal, temp;
	unsigned int val;
	struct iio_channel *chan;

	if (!hdst || !pdata) {
		pr_err("%s: sprd_hdst(%p) or pdata(%p) is NULL!\n",
			__func__, sprd_hdst, pdata);
		return;
	}

	chan = hdst->adc_chan;

	down(&hdst->sem);
	val = headset_eic_get_irq_status(HDST_EIC_BDET);
	if (val == 0) {
		pr_info("%s fatal error, irq 15 invalid, INT8(220.MIS) %x\n",
			__func__, val);
		goto out;
	}

	btn_irq_trig_level = headset_eic_get_trig_level(15);
	pr_info("%s: button is %s, hdst_status 0x%x\n", __func__,
		btn_irq_trig_level ? "pressed" : "released", hdst->hdst_status);

	hdst->bdet_val_last =
		headset_eic_get_insert_status(HDST_INSERT_BIT_BDET);

	if (headset_eic_get_insert_status(15) == 0) {
		pr_info("%s: button is invalid! STS0 0x%x\n",
			__func__, headset_reg_value_read(ANA_STS0));
		goto out;
	}
	button_bit_value_current =
		headset_eic_get_insert_status(HDST_INSERT_BIT_BDET);
	if (button_bit_value_current != hdst->bdet_val_last) {
		pr_info("%s software debounce error\n", __func__);
		goto out;
	}

	if (val != headset_eic_get_irq_status(HDST_EIC_BDET)) {
		pr_info("%s check debounce failed\n", __func__);
		goto out;
	}

	pr_info("%s polling: DCL1(0100) %x, CLK0(0068) %x, HID0(0144) %x\n",
		__func__, headset_reg_value_read(ANA_DCL1),
		headset_reg_value_read(ANA_CLK0),
		headset_reg_value_read(ANA_HID0));

	headset_eic_enable(HDST_EIC_BDET, 0);
	if (btn_irq_trig_level == 1) {
		headset_eic_set_trig_level(15, 0);
		headset_hmicbias_polling_enable(false, false);
	} else if (btn_irq_trig_level == 0) {
		headset_eic_set_trig_level(15, 1);
	}

	if (btn_irq_trig_level == 1) {/* pressed! */
		if (pdata->nbuttons > 0) {
			headset_reg_set_bits(ANA_HDT3, HEDET_V2AD_EN);
			headset_reg_clr_bits(ANA_HDT3, HEDET_V2AD_SCALE_SEL);
			headset_reg_write(ANA_HDT3, HEDET_V2AD_CH_SEL(0),
				HEDET_V2AD_CH_SEL(0xF));
			for (temp = 0; temp < ADC_READ_REPET; temp++)
				adc_mic_average += headset_get_adc_value(chan);

			adc_mic_average /= ADC_READ_REPET;
			if (-1 == adc_mic_average) {
				pr_info("%s software debounce check fail\n",
					__func__);
				goto out;
			}
			pr_info("%s in button press\n", __func__);
			adc_ideal = headset_adc_get_ideal(adc_mic_average,
						pdata->coefficient);
			pr_info("adc_value: adc_mic_average=%d, ideal_value: adc_ideal=%d\n",
				adc_mic_average, adc_ideal);
			if (adc_ideal >= 0)
				adc_mic_average = adc_ideal;
			pr_info("adc_mic_average = %d\n", adc_mic_average);
			hdst->btns_pressed |=
				headset_adc_to_button(adc_mic_average);
		}

		if (hdst->btn_state_last == 0) {
			headset_jack_report(hdst, &hdst->btn_jack,
				hdst->btns_pressed, hdst->btns_pressed);
			hdst->btn_state_last = 1;
			pr_info("Reporting headset button press. button: 0x%#x\n",
				hdst->btns_pressed);
		} else {
			pr_err("Headset button has been reported already. button: 0x%#x\n",
				hdst->btns_pressed);
		}
	} else if (btn_irq_trig_level == 0) {/* released */
		pr_info("%s %d, in button release\n", __func__, __LINE__);
		if (hdst->btn_state_last == 1) {
			headset_jack_report(hdst, &hdst->btn_jack,
				0, hdst->btns_pressed);
			hdst->btn_state_last = 0;
			pr_info("Reporting headset button release. button: %#x\n",
				hdst->btns_pressed);
		} else
			pr_err("Headset button has been released already. button: %#x\n",
				hdst->btns_pressed);

		hdst->btns_pressed &= ~SPRD_BUTTON_JACK_MASK;
	}
out:
	headset_eic_intc_clear(0);
	headset_eic_clear_irq(15);
	headset_eic_enable(15, 1);
	headset_eic_trig_irq(15);
	/* wake_unlock(&hdst->btn_wakelock); */
	up(&hdst->sem);
}

static void headset_process_for_4pole(enum sprd_headset_type headset_type)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);

	if (!pdata) {
		pr_err("%s: pdata is NULL!\n", __func__);
		return;
	}

	if ((headset_type == HEADSET_4POLE_NOT_NORMAL)
	    && (pdata->gpio_switch == 0)) {
		headset_eic_enable(15, 0);
		pr_info("%s micbias power off for 4 pole abnormal\n", __func__);
		headmicbias_power_on(hdst, 0);
		pr_info("HEADSET_4POLE_NOT_NORMAL is not supported %s\n",
			"by your hardware! so disable the button irq!");
	} else {
		headset_eic_set_trig_level(15, 1);
		/* config LDETL REF for plug-out pop */
		if (pdata->jack_type == JACK_TYPE_NO)
			headset_reg_write(ANA_HDT2, HEDET_LDETL_REF_SEL(0x1),
			HEDET_LDETL_REF_SEL(0x7));
		headset_reg_clr_bits(ANA_HDT2, HEDET_LDETL_FLT_EN);
		/* set threshold and HEDET_BDET_EN */
		headset_button_irq_threshold(1);
		headset_eic_enable(15, 1);
		headset_eic_trig_irq(15);
	}

	hdst->hdst_status = SND_JACK_HEADSET;
	if (hdst->report == 0) {
		pr_debug("%s report for 4p\n", __func__);
		headset_jack_report(hdst, &hdst->hdst_jack,
			hdst->hdst_status, SND_JACK_HEADSET);
		headset_eic_enable(15, 1);/* enalbe bdet */
		headset_eic_trig_irq(15);/* trig bdet */
	}
	if (hdst->re_detect == true) {
		pr_debug("%s report for 4p re_detect\n", __func__);
		headset_jack_report(hdst, &hdst->hdst_jack,
			0, SPRD_HEADSET_JACK_MASK);

		headset_jack_report(hdst, &hdst->hdst_jack,
			hdst->hdst_status, SND_JACK_HEADSET);
	}
	hdst->report = 1;
	pr_info("%s headset plug in\n", __func__);
}

static void headset_fc_work_func(struct work_struct *work)
{
	struct sprd_headset *hdst = sprd_hdst;
	int cnt = 0;

	pr_debug("Start waiting for fast charging.\n");
	/* Wait at least 50mS before DC-CAL. */
	while ((++cnt <= 3) && hdst->plug_state_last)
		sprd_msleep(20);

	if (hdst->plug_state_last) {
		pr_info("Headphone fast charging completed. (%d ms)\n",
			 (cnt - 1) * 20);
		fast_charge_finished = true;
	} else
		pr_info("Wait for headphone fast charging aborted.\n");
}

static void headset_detect_all_work_func(struct work_struct *work)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	struct sprd_headset_power *power = (hdst ? &hdst->power : NULL);
	enum sprd_headset_type headset_type;
	int plug_state_current, insert_all_data_last, insert_all_data_current,
		val;
	bool trig_level, insert_status, detect_value = false;
	static int times, times_1;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}
	pr_info("%s enter\n", __func__);

	down(&hdst->sem);

	if ((power->head_mic == NULL) || (power->bias == NULL)) {
		pr_info("sprd_headset_power_init fail 0\n");
		goto out;
	}
	val = headset_eic_get_irq_status(10);
	if (val == 0) {
		pr_info("%s fatal error, irq 10 invalid, INT8(220.MIS) %x\n",
			__func__, val);
		headset_eic_clear_irq(10);
		headset_eic_intc_clear(1);
		headset_eic_intc_clear(0);
		headset_eic_trig_irq(10);
		goto out;
	}
	insert_all_data_last = headset_eic_get_irq_data(10);
	if (hdst->plug_state_last == 0)
		sprd_msleep(40);
	else
		sprd_msleep(20);

	insert_all_data_current = headset_eic_get_irq_data(10);
	if (insert_all_data_last != insert_all_data_current) {
		pr_info("%s check debounce failed\n", __func__);
		headset_eic_clear_irq(10);
		headset_eic_intc_clear(1);
		headset_eic_intc_clear(0);
		headset_eic_trig_irq(10);
		goto out;
	}

	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);
	pr_info("%s plug_state_last %d\n", __func__, hdst->plug_state_last);

	if (hdst->plug_state_last == 0) {
		sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN, BIT_ANA_AUD_EN);
		if (pdata->jack_type == JACK_TYPE_NO) {
			headset_reg_set_bits(ANA_HDT1, HEDET_PLGPD_EN);
			headset_reg_set_bits(ANA_HDT2, HEDET_LDETL_FLT_EN);
			pr_info("filter detect_l ANA_HDT2 0x%04x\n",
				headset_reg_value_read(ANA_HDT2));
		} else if (pdata->jack_type == JACK_TYPE_NC) {
			/* step 3 */
			headset_reg_set_bits(ANA_HDT2, HEDET_LDETL_FLT_EN);
			/*
			 * step 4, following two line will be set back
			 * after headset
			 * type detection over, see more on step 7
			 * turn the current from HEADSET_L to HP_L to
			 * detect the resistor of HP_L
			 * in spec, HEDET_LDET_CMP_SEL need to be handled before
			 * insert, so I dout is it right in here? I am not sure
			 */
			headset_reg_set_bits(ANA_HDT1, HEDET_LDET_CMP_SEL);
			headset_reg_clr_bits(ANA_HDT0, HEDET_JACK_TYPE);
		}

		pr_info("%s micbias power on\n", __func__);
		sprd_headset_bias_control(hdst, 1);
		/* check if this need improve or need, I am not sure */
		headmicbias_power_on(hdst, 1);
		sprd_msleep(10);
	}

	pr_info("%s insert_all_val_last = %d, plug_state_last = %d\n",
			__func__, hdst->insert_all_val_last,
			hdst->plug_state_last);

	trig_level = headset_eic_get_trig_level(10);
	insert_status =
		headset_eic_get_insert_status(HDST_INSERT_BIT_INSERT_ALL);
	pr_info("%s trig_level %s, insert_status %s\n", __func__,
		trig_level ? "high" : "low", insert_status ? "high" : "low");
	if (trig_level && insert_status) {
		pr_info("%s headphone is plugin???\n", __func__);
		plug_state_current = 1;
	} else if (!trig_level && !insert_status) {
		pr_info("%s headphone is plugout???\n", __func__);
		plug_state_current = 0;
	} else {
		pr_info("%s fatal error, re-trig insert_all irq again\n",
			__func__);
		headset_eic_clear_irq(10);
		headset_eic_intc_clear(1);
		headset_eic_intc_clear(0);
		headset_eic_trig_irq(10);
		goto out;
	}

	if (hdst->re_detect == true)
		detect_value =	insert_status;

	/*
	 * 4pole detect as 3 pole if polling is enabled,
	 * and need to disable polling after plugout
	 */
	headset_hmicbias_polling_enable(false, true);
	pr_info("%s plug_state_last %d, plug_state_current %d, detect_value %d\n",
			__func__, hdst->plug_state_last,
			plug_state_current, detect_value);

	if ((1 == plug_state_current && 0 == hdst->plug_state_last) ||
	  (hdst->re_detect == true && detect_value == true)) {
		/* need move this to a better place? */
		headset_eic_set_trig_level(10, 0);
		headset_type =
			headset_type_detect_all(hdst->insert_all_val_last);
		pr_info("%s headset_type  %d\n", __func__, headset_type);
		switch (headset_type) {
		case HEADSET_TYPE_ERR:
			hdst->det_err_cnt++;
			pr_info("headset_type = %d detect_err_count = %d(HEADSET_TYPE_ERR) times %d\n",
				headset_type, hdst->det_err_cnt, times);
			if (times < 10)
				queue_delayed_work(hdst->det_all_work_q,
				&hdst->det_all_work, msecs_to_jiffies(2000));
			times++;
			goto out;
		case HEADSET_4POLE_NORMAL:
			pr_info("headset_type = %d (HEADSET_4POLE_NORMAL)\n",
				headset_type);
			if (pdata->gpio_switch != 0)
				gpio_direction_output(pdata->gpio_switch, 0);
			break;
		case HEADSET_4POLE_NOT_NORMAL:
			pr_info("headset_type = %d (HEADSET_4POLE_NOT_NORMAL)\n",
				headset_type);
			if (pdata->gpio_switch != 0)
				gpio_direction_output(pdata->gpio_switch, 1);
			/* Repeated detection 5 times when 3P is detected */
		case HEADSET_NO_MIC:
			pr_info("headset_type = %d (HEADSET_NO_MIC)\n",
				headset_type);
			/*
			 * following is ok, when in early test,
			 * adc can't work, no mic
			 * headphone is detected every time,
			 * so I remove this for test
			 */

			/* if (times_1 < 5) {
			 *	queue_delayed_work(hdst->det_all_work_q,
			 *	  &hdst->det_all_work, msecs_to_jiffies(1000));
			 *	times_1++;
			 *	hdst->re_detect = true;
			 * } else {
			 *	hdst->re_detect = false;
			 * }
			 */
			if (pdata->gpio_switch != 0)
				gpio_direction_output(pdata->gpio_switch, 0);
			break;
		case HEADSET_APPLE:
			pr_info("headset_type = %d (HEADSET_APPLE)\n",
				headset_type);
			pr_info("we have not yet implemented this in the code\n");
			break;
		default:
			pr_info("headset_type = %d (HEADSET_UNKNOWN)\n",
				headset_type);
			break;
		}

		times = 0;
		hdst->det_err_cnt = 0;
		if (headset_type == HEADSET_NO_MIC ||
				headset_type == HEADSET_4POLE_NOT_NORMAL)
			hdst->headphone = 1;
		else
			hdst->headphone = 0;

		hdst->plug_state_last = 1;

		if (hdst->headphone) {
			/*
			 * if it is 3pole, run
			 * headset_reg_clr_bits(ANA_PMU1, HMIC_BIAS_EN) in here
			 */
			headset_button_irq_threshold(0);
			headset_eic_enable(15, 0);

			hdst->hdst_status = SND_JACK_HEADPHONE;
			if (hdst->report == 0) {
				pr_debug("report for 3p\n");
				headset_jack_report(hdst, &hdst->hdst_jack,
					hdst->hdst_status, SND_JACK_HEADPHONE);
			}

			hdst->report = 1;

			pr_info("micbias power off for 3pole\n");
			if (hdst->re_detect == false)
				headmicbias_power_on(hdst, 0);
			pr_info("%s headphone plug in\n", __func__);
		} else
			headset_process_for_4pole(headset_type);

		pr_info("%s %d,\n",	__func__, __LINE__);

		/*something after headset type detection over*/
		if (pdata->jack_type == JACK_TYPE_NC) {
			headset_reg_write(ANA_HDT3, HEDET_V2AD_CH_SEL(0x4),
				HEDET_V2AD_CH_SEL(0xF));
			headset_reg_clr_bits(ANA_HDT1, HEDET_PLGPD_EN);
			headset_reg_clr_bits(ANA_HDT1, HEDET_LDET_CMP_SEL);
			headset_reg_set_bits(ANA_HDT0, HEDET_JACK_TYPE);
			usleep_range(50, 60); /* Wait for 50us */
			headset_reg_set_bits(ANA_HDT1, HEDET_PLGPD_EN);
		}
		pr_info("%s %d,\n",	__func__, __LINE__);

		if (pdata->do_fm_mute)
			vbc_close_fm_dggain(false);
		pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);

		headset_eic_enable(10, 0);
		headset_eic_clear_irq(10);
		headset_eic_set_trig_level(10, 0);
		headset_eic_intc_clear(1);
		headset_eic_intc_clear(0);
		headset_eic_enable(10, 1);
		pr_info("STS0(184) %x\n", headset_reg_value_read(ANA_STS0));
		headset_eic_trig_irq(10);
		pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);
	} else if (0 == plug_state_current && 1 == hdst->plug_state_last) {
		headmicbias_power_on(hdst, 0);
		times = 0;
		times_1 = 0;
		pr_info("%s micbias power off for plug out  times %d\n",
			__func__, times);

		headset_eic_enable(10, 0);
		headset_eic_enable(11, 0);
		headset_eic_enable(15, 0);
		headmicbias_power_on(hdst, 0);
		headset_eic_set_trig_level(10, 1);

		headset_reg_clr_bits(ANA_PMU1, HMIC_BIAS_EN);
		headset_button_release_verify();

		hdst->hdst_status &= ~SPRD_HEADSET_JACK_MASK;
		headset_jack_report(hdst, &hdst->hdst_jack,
			0, SPRD_HEADSET_JACK_MASK);
		/* must be called before set hdst->plug_state_last = 0 */
		headset_hmicbias_polling_enable(false, true);
		hdst->plug_state_last = 0;
		hdst->report = 0;
		hdst->re_detect = false;

		if (hdst->headphone)
			pr_info("%s headphone plug out\n", __func__);
		else
			pr_info("%s headset plug out\n", __func__);

		/*delay 10ms*/
		sprd_msleep(10);

		/*
		 * asic's email, this reg needn't to disable, Power
		 * Consumption is very little
		 * headset_reg_clr_bits(ANA_HDT2, HEDET_MDET_EN);
		 * Close the fm in advance because of the noise when playing fm
		 * in speaker mode plugging out headset.
		 */
		if (pdata->do_fm_mute)
			vbc_close_fm_dggain(true);

		/*
		 * following this, check all work queue if need to be
		 * canceled in appropriate time, I am not sure
		 */
		cancel_delayed_work(&hdst->fc_work);
		fast_charge_finished = false;
		headset_eic_clear_irq(10);
		headset_eic_set_trig_level(16, 1);
		headset_eic_enable(16, 0);
		headset_eic_intc_clear(0);

		headset_eic_clear_irq(16);
		if (pdata->jack_type == JACK_TYPE_NO) {
			headset_eic_set_trig_level(12, 0);
			headset_eic_enable(12, 1);
			headset_eic_trig_irq(12);
			headset_reg_write(ANA_HDT2, HEDET_LDETL_REF_SEL(0x3),
				HEDET_LDETL_REF_SEL(0x7));
		} else if (pdata->jack_type == JACK_TYPE_NC) {
			headset_eic_enable(10, 1);
			headset_eic_trig_irq(10);
		}

		pr_info("%s ANA_HDT2 0x%04x\n", __func__,
			headset_reg_value_read(ANA_HDT2));

		msleep(20);
	} else {
		times = 0;
		times_1 = 0;
		hdst->re_detect = false;
		hdst->report = 0;
		pr_info("%s times %d irq_detect must be enabled anyway!!\n",
			__func__, times);
		headmicbias_power_on(hdst, 0);
		headset_eic_set_trig_level(16, 1);
		headset_eic_enable(16, 0);
		headset_eic_clear_irq(16);
		headset_eic_intc_clear(0);
		headset_internal_eic_entry_init();
		pr_info("%s micbias power off for irq_error\n", __func__);
		goto out;
	}
out:
	headset_reg_clr_bits(ANA_HDT2, HEDET_LDETL_FLT_EN);
	pr_info("%s HDT0(00D0) 0x%x,plug_state_last %d\n", __func__,
		headset_reg_value_read(ANA_HDT0), hdst->plug_state_last);
	/*
	 * I think if in hdst->re_detect == true and times < 10,
	 * we dont need to run following
	 */
	if (hdst->plug_state_last == 0) {
		headset_scale_set(0);/* default value is 0, doc don't refer */
		headmicbias_power_on(hdst, 0);/* doc don't refer */
		pr_info("%s micbias power off end\n", __func__);
		sprd_headset_bias_control(hdst, 0);/* doc don't refer */
		sprd_headset_vb_control(hdst, 1);/* doc don't refer */
		/*
		 * if it is type error, run
		 *  headset_reg_clr_bits(ANA_PMU1, HMIC_BIAS_EN) in here
		 */
		headset_button_irq_threshold(0);
		hdst->mic_irq_trigged = 0;
	}
	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);

	pr_info("%s out\n", __func__);
	up(&hdst->sem);
}

static void headset_ldetl_work_func(struct work_struct *work)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_power *power = (hdst ? &hdst->power : NULL);
	unsigned int val, ldetl_data_last, ldetl_data_current;
	bool insert_status;
	int check_times = 100;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}

	down(&hdst->sem);

	pr_info("%s enter\n", __func__);
	if ((power->head_mic == NULL) || (power->bias == NULL)) {
		pr_info("sprd_headset_power_init fail 0\n");
		goto out;
	}
	val = headset_eic_get_irq_status(12);
	if (val == 0) {
		pr_info("%s fatal error, irq 12 invalid, INT8(220.MIS) %x\n",
			__func__, val);
		headset_eic_clear_irq(12);
		headset_eic_intc_clear(1);
		headset_eic_intc_clear(0);
		headset_eic_trig_irq(12);
		goto out;
	}
	ldetl_data_last = headset_eic_get_irq_data(12);
	sprd_msleep(20);
	ldetl_data_current = headset_eic_get_irq_data(12);
	if (ldetl_data_last != ldetl_data_current) {
		pr_info("%s check debounce failed\n", __func__);
		headset_eic_clear_irq(12);
		headset_eic_intc_clear(1);
		headset_eic_intc_clear(0);
		headset_eic_trig_irq(12);
		goto out;
	}
	hdst->ldetl_trig_val_last = headset_eic_get_trig_level(12);
	insert_status = headset_eic_get_insert_status(HDST_INSERT_BIT_LDETL);
	pr_info("%s ldetl_trig_val_last %s, insert_status %s\n", __func__,
		hdst->ldetl_trig_val_last ? "high" : "low",
		insert_status ? "high" : "low");

	if (hdst->ldetl_trig_val_last && insert_status) {
		hdst->ldetl_plug_in = 1;
		pr_info("%s ldetl trig level is high, plugin?\n", __func__);
	} else if (hdst->ldetl_trig_val_last == 0 && insert_status == 0) {
		hdst->ldetl_plug_in = 0;
		pr_info("%s ldetl trig level is low, plugout?\n", __func__);
	} else {
		pr_info("%s fatal error, re-trig ldetl irq again\n", __func__);
		headset_eic_clear_irq(12);
		headset_eic_intc_clear(1);
		headset_eic_intc_clear(0);
		headset_eic_trig_irq(12);
		goto out;
	}
	headset_reg_clr_bits(ANA_HDT1, HEDET_PLGPD_EN);
	headset_eic_enable(12, 0);
	headset_eic_set_trig_level(12, 0);
	headset_eic_intc_clear(1);
	pr_info("%s %d ldetl_trig_val_last %d, plug_state_last %d, ldetl_plug_in %d\n",
		__func__, __LINE__, hdst->ldetl_trig_val_last,
		hdst->plug_state_last, hdst->ldetl_plug_in);

	if ((0 == hdst->ldetl_trig_val_last && 0 == hdst->plug_state_last) ||
		(hdst->ldetl_plug_in == 0)) {
		headset_reg_set_bits(ANA_HDT1, HEDET_PLGPD_EN);
		headset_eic_set_trig_level(12, 1);
		headset_eic_clear_irq(12);
		headset_eic_intc_clear(0);
		headset_eic_enable(12, 1);
		headset_eic_trig_irq(12);
		sprd_msleep(20);
	} else if ((hdst->ldetl_trig_val_last == 1) ||
	(hdst->ldetl_plug_in == 1)) {
		hdst->insert_all_irq_trigged = 0;
		headset_reg_clr_bits(ANA_HDT1, HEDET_PLGPD_EN);
		headset_eic_enable(12, 0);
		headset_eic_clear_irq(12);
		headset_eic_intc_clear(1);
		headset_eic_set_trig_level(12, 0);
		usleep_range(3000, 3500);
		headset_eic_intc_clear(0);
		headset_eic_set_trig_level(10, 1);
		headset_eic_enable(10, 1);
		headset_eic_trig_irq(10);
		while (check_times > 0) {
			if (hdst->insert_all_irq_trigged == 1)
				break;
			check_times--;
			pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);
			sprd_msleep(240);
		}
		pr_info("%s check_times %d\n", __func__, check_times);
		if (check_times == 0) {
			pr_info("%s failed to wait insert_all irq, trig ldetl again\n",
				__func__);
			headset_eic_clear_irq(12);
			headset_eic_intc_clear(1);
			headset_eic_intc_clear(0);
			headset_eic_enable(12, 1);
			headset_eic_set_trig_level(12, 1);
			headset_eic_trig_irq(12);
		}
		hdst->insert_all_irq_trigged = 0;
	}
out:
	pr_info("%s out\n", __func__);
	up(&hdst->sem);
}

static void headset_reg_dump_func(struct work_struct *work)
{

	int adc_mic, gpio_insert_all, ana_sts0, ana_pmu0, ana_pmu1,
		ana_hdt0, ana_hdt1, ana_hdt2, ana_dcl0, ana_cdc2,
		ana_cdc3, arm_module_en, arm_clk_en;
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}
	pdata = (hdst ? &hdst->pdata : NULL);
	if (!pdata) {
		pr_err("%s: pdata is NULL!\n", __func__);
		return;
	}

	adc_mic = headset_wrap_sci_adc_get(hdst->adc_chan);
	gpio_insert_all =
		gpio_get_value(pdata->gpios[HDST_GPIO_AUD_DET_INT_ALL]);
	pr_info("adc_mic %d, gpio_detect_all %d\n", adc_mic, gpio_insert_all);

	sci_adi_write(ANA_REG_GLB_ARM_MODULE_EN,
		BIT_ANA_AUD_EN, BIT_ANA_AUD_EN);
	headset_reg_read(ANA_PMU0, &ana_pmu0);
	headset_reg_read(ANA_HDT0, &ana_hdt0);
	headset_reg_read(ANA_HDT1, &ana_hdt1);
	headset_reg_read(ANA_HDT2, &ana_hdt2);
	headset_reg_read(ANA_STS0, &ana_sts0);
	headset_reg_read(ANA_DCL0, &ana_dcl0);
	headset_reg_read(ANA_CDC2, &ana_cdc2);
	headset_reg_read(ANA_CDC3, &ana_cdc3);
	headset_reg_read(ANA_PMU1, &ana_pmu1);

	sci_adi_read(ANA_REG_GLB_ARM_MODULE_EN, &arm_module_en);
	sci_adi_read(ANA_REG_GLB_ARM_CLK_EN, &arm_clk_en);

	pr_info("ana_pmu0  | ana_hdt0 | ana_hdt1 | ana_hdt2 | ana_sts0 | ana_dcl0\n");
	pr_info("0x%08X|0x%08X|0x%08X|0x%08X|0x%08X|0x%08X\n",
		 ana_pmu0, ana_hdt0, ana_hdt1,
		ana_hdt2, ana_sts0, ana_dcl0);

	pr_info("ana_cdc2 | ana_cdc3 | ana_pmu1\n");
	pr_info("0x%08X|0x%08X|0x%08X\n", ana_cdc2, ana_cdc3, ana_pmu1);
	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);

	if (hdst->debug_level >= 2)
		queue_delayed_work(hdst->reg_dump_work_q,
			&hdst->reg_dump_work, msecs_to_jiffies(500));
}

static irqreturn_t headset_detect_top_eic_handler(int irq, void *dev)
{
	struct sprd_headset *hdst = dev;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	struct sprd_headset_power *power;
	unsigned int val;
	bool ret;

	pr_info("%s enter\n", __func__);
	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		goto out;
	}
	power = (hdst ? &hdst->power : NULL);
	if (!power) {
		pr_err("%s: power is NULL!\n", __func__);
		goto out;
	}
	if ((power->head_mic == NULL) || (power->bias == NULL)) {
		pr_info("sprd_headset_power_init fail 0\n");
		goto out;
	}

	hdst->gpio_detect_int_all_last =
		gpio_get_value(pdata->gpios[HDST_GPIO_AUD_DET_INT_ALL]);

	headset_reg_read(ANA_STS0, &val);
	/* the gpio value is changed at later, I don't know why */
	pr_info("%s: detect_int_all, IRQ_%d(GPIO_%d) %d, STS0(184) %x\n",
		__func__, hdst->irq_detect_int_all,
		pdata->gpios[HDST_GPIO_AUD_DET_INT_ALL],
		hdst->gpio_detect_int_all_last, val);

	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);

	if ((BIT(14) & headset_reg_value_read(ANA_INT34)) == 0) {
		pr_err("%s: error, intc is not active\n", __func__);
		return IRQ_HANDLED;
	}

	if ((val & 0xFC00) == 0) {
		pr_err("%s no headset alert signal at all! headphone is plugout?\n",
			__func__);
		pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);
	}

	val = headset_reg_value_read(ANA_INT8);
	if ((val & 0xFC00) == 0) {
		pr_err("%s: no active interrupt at all!\n", __func__);
		pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);

		headset_eic_clear_irq(HDST_EIC_MAX);
		headset_eic_intc_clear(1);
		headset_eic_intc_clear(0);
		headset_internal_eic_entry_init();
		pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);

		return IRQ_HANDLED;
	}
	/* I think this need place at the begain of this func to clear intc */
	headset_eic_intc_clear(1);
	irq_set_irq_type(hdst->irq_detect_int_all, IRQF_TRIGGER_HIGH);

	if (val & BIT(10)) {/* insert_all */
		pr_info("%s in insert_all\n", __func__);
		/* I am not sure the new func is right or not */
		__pm_wakeup_event(&hdst->det_all_wakelock,
			msecs_to_jiffies(2000));
		/* I think this is useless, only used to check debounce */
		hdst->insert_all_val_last = headset_eic_get_irq_status(10);
		hdst->insert_all_irq_trigged = 1;
		pr_info("%s insert_all_val_last %d\n", __func__,
			hdst->insert_all_val_last);

		ret = cancel_delayed_work(&hdst->det_all_work);
		queue_delayed_work(hdst->det_all_work_q,
			&hdst->det_all_work, msecs_to_jiffies(0));
		pr_info("%s insert_all irq active, exit, ret %d\n",
			__func__, ret);
	}
	if (val & BIT(11)) {/* mdet */
		pr_info("%s in mdet\n", __func__);
		/* I am not sure the new func is right or not */
		__pm_wakeup_event(&hdst->mic_wakelock, msecs_to_jiffies(2000));
		/* I think this is useless, only used to check debounce */
		hdst->mdet_val_last = headset_eic_get_irq_status(11);

		ret = cancel_delayed_work(&hdst->det_mic_work);
		queue_delayed_work(hdst->det_mic_work_q,
			&hdst->det_mic_work, msecs_to_jiffies(5));
		pr_info("%s mdet irq active, ret %d\n", __func__, ret);
	}
	if (val & BIT(12)) {/* ldetl */
		pr_info("%s in ldetl\n", __func__);
		if (pdata->jack_type == JACK_TYPE_NC) {
			pr_err("%s: don't need ldetl_irq in JACK_TYPE_NC!\n",
				__func__);
			goto out;
		}
		/* I am not sure the new func is right or not */
		__pm_wakeup_event(&hdst->ldetl_wakelock,
		msecs_to_jiffies(2000));

		ret = cancel_delayed_work(&hdst->ldetl_work);
		queue_delayed_work(hdst->ldetl_work_q,
			&hdst->ldetl_work, msecs_to_jiffies(0));
		pr_info("%s ldetl irq active, ldetl_trig_val_last %d,plug_state_last %d, ldetl_plug_in %d\n",
			__func__, hdst->ldetl_trig_val_last,
			hdst->plug_state_last, hdst->ldetl_plug_in);
	}
	if (val & BIT(15)) {/* bdet */
		pr_info("%s in bdet\n", __func__);

		/* I am not sure the new func is right or not */
		__pm_wakeup_event(&hdst->btn_wakelock, msecs_to_jiffies(2000));
		ret = cancel_delayed_work(&hdst->btn_work);
		queue_delayed_work(hdst->btn_work_q,
			&hdst->btn_work, msecs_to_jiffies(0));
		pr_info("%s bdet irq active, ret %d\n", __func__, ret);
	}

out:
	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);
	pr_info("%s exit\n", __func__);

	return IRQ_HANDLED;
}

/* ================= create sys fs for debug =================== */
 /* Used for getting headset type in sysfs. */
static ssize_t headset_state_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buff)
{
	struct sprd_headset *hdst = sprd_hdst;
	int type;

	switch (hdst->hdst_status) {
	case SND_JACK_HEADSET:
		type = 1;
		break;
	case SND_JACK_HEADPHONE:
		type = 2;
		break;
	default:
		type = 0;
		break;
	}

	pr_debug("%s status: %#x, headset_state = %d\n",
		__func__, hdst->hdst_status, type);

	return sprintf(buff, "%d\n", type);
}

static ssize_t headset_state_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buff, size_t len)
{
	return len;
}
/* ============= /sys/kernel/headset/debug_level =============== */

static ssize_t headset_debug_level_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buff)
{
	struct sprd_headset *hdst = sprd_hdst;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return -1;
	}

	pr_info("%s debug_level = %d\n", __func__, hdst->debug_level);

	return sprintf(buff, "%d\n", hdst->debug_level);
}

static ssize_t headset_debug_level_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buff, size_t len)
{
	struct sprd_headset *hdst = sprd_hdst;
	unsigned long level;
	int ret;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return -1;
	}

	ret = kstrtoul(buff, 10, &level);
	if (ret) {
		pr_err("%s kstrtoul failed!(%d)\n", __func__, ret);
		return len;
	}
	hdst->debug_level = level;
	pr_info("%s debug_level = %d\n", __func__, hdst->debug_level);
	if (hdst->debug_level >= 2)
		queue_delayed_work(hdst->reg_dump_work_q,
			&hdst->reg_dump_work, msecs_to_jiffies(500));

	return len;
}

static int headset_debug_sysfs_init(void)
{
	int ret, i;
	static struct kobject *headset_debug_kobj;
	static struct kobj_attribute headset_debug_attr[] = {
		__ATTR(debug_level, 0644,
		headset_debug_level_show,
		headset_debug_level_store),
		__ATTR(state, 0644,
		headset_state_show,
		headset_state_store),
	};

	headset_debug_kobj = kobject_create_and_add("headset", kernel_kobj);
	if (headset_debug_kobj == NULL) {
		ret = -ENOMEM;
		pr_err("%s register sysfs failed. ret = %d\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < sizeof(headset_debug_attr) /
	     sizeof(headset_debug_attr[0]); i++) {
		ret = sysfs_create_file(headset_debug_kobj,
					&headset_debug_attr[i].attr);
		if (ret) {
			pr_err("%s create sysfs '%s' failed. ret = %d\n",
			       __func__, headset_debug_attr[i].attr.name, ret);
			return ret;
		}
	}

	pr_info("%s success\n", __func__);

	return ret;
}

void sprd_headset_set_global_variables(
	struct sprd_headset_global_vars *glb)
{
	arch_audio_codec_set_regmap(glb->regmap);
	arch_audio_codec_set_reg_offset(glb->codec_reg_offset);
}

static int headset_adc_cal_from_efuse(struct platform_device *pdev);

/*
 * note: after sprd_headset_probe() run to its end
 * (be called in sprd-asoc-card-utils.c), this probe be called.
 *  note: sprd_headset_probe --> asoc_sprd_card_parse_sprd_headset
 * --> asoc_sprd_card_parse_of  --> asoc_sprd_card_probe -->
 * all in sprd-asoc-card-utils.c
 * note: this probe is called by sprd_codec_soc_probe() in sprd-codec.c
 */
int sprd_headset_soc_probe(struct snd_soc_codec *codec)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	struct device *dev = codec->dev; /* digiatal part device */
	struct gpio_desc *gpio_desc_test;
	struct snd_soc_card *card = codec->component.card;
	unsigned int adie_chip_id;
	int ret, i;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return -1;
	}

	hdst->codec = codec;

	adie_chip_id = sci_get_ana_chip_id() >> 16;
	pr_info("%s adie_chip_id 0x%x\n", __func__, adie_chip_id & 0xFFFF);

	headset_detect_reg_init();
	/* try to close polling, I am not sure whether need this here */
	headset_hmicbias_polling_enable(false, true);

	pr_debug("%s ANA_HID0(0144) %x, ANA_DCL1(0100) %x\n",
		__func__, headset_reg_value_read(ANA_HID0),
		headset_reg_value_read(ANA_DCL1));

	ret = sprd_headset_power_init(hdst);
	if (ret) {
		pr_err("sprd_headset_power_init failed\n");
		return ret;
	}

	ret = snd_soc_card_jack_new(card, "Headset Jack",
		SPRD_HEADSET_JACK_MASK, &hdst->hdst_jack, NULL, 0);
	if (ret) {
		pr_err("Failed to create headset jack\n");
		return ret;
	}

	ret = snd_soc_card_jack_new(card, "Headset Keyboard",
		SPRD_BUTTON_JACK_MASK, &hdst->btn_jack, NULL, 0);
	if (ret) {
		pr_err("Failed to create button jack\n");
		return ret;
	}

	if (pdata->nbuttons > MAX_BUTTON_NUM) {
		pr_warn("button number in dts is more than %d!\n",
			MAX_BUTTON_NUM);
		pdata->nbuttons = MAX_BUTTON_NUM;
	}
	for (i = 0; i < pdata->nbuttons; i++) {
		struct headset_buttons *buttons =
			&pdata->headset_buttons[i];

		ret = snd_jack_set_key(hdst->btn_jack.jack,
			headset_jack_type_get(i), buttons->code);
		if (ret) {
			pr_err("%s: Failed to set code for btn-%d\n",
				__func__, i);
			return ret;
		}
	}

	if (pdata->gpio_switch != 0)
		gpio_direction_output(pdata->gpio_switch, 0);
	gpio_direction_input(pdata->gpios[HDST_GPIO_AUD_DET_INT_ALL]);

	hdst->irq_detect_int_all =
		gpio_to_irq(pdata->gpios[HDST_GPIO_AUD_DET_INT_ALL]);
	pr_debug("%s irq_detect_int_all %d, GPIO %d",
		 __func__, hdst->irq_detect_int_all,
		 pdata->gpios[HDST_GPIO_AUD_DET_INT_ALL]);
	gpio_desc_test = gpio_to_desc(pdata->gpios[HDST_GPIO_AUD_DET_INT_ALL]);
	/* used to test, we can operate this gpio with command */
	gpiod_export(gpio_desc_test, true);

	sema_init(&hdst->sem, 1);

	INIT_DELAYED_WORK(&hdst->det_mic_work, headset_mic_work_func);
	hdst->det_mic_work_q = create_singlethread_workqueue("headset_mic");
	if (hdst->det_mic_work_q == NULL) {
		pr_err("create_singlethread_workqueue for headset_mic failed!\n");
		goto failed_to_headset_mic;
	}

	INIT_DELAYED_WORK(&hdst->btn_work, headset_button_work_func);
	hdst->btn_work_q = create_singlethread_workqueue("headset_button");
	if (hdst->btn_work_q == NULL) {
		pr_err("create_singlethread_workqueue for headset_button failed!\n");
		goto failed_to_headset_button;
	}
	hdst->bdet_val_last = 0;
	hdst->btn_state_last = 0;
	hdst->re_detect = false;
	hdst->mic_irq_trigged = 0;
	hdst->insert_all_irq_trigged = 0;

	INIT_DELAYED_WORK(&hdst->det_all_work, headset_detect_all_work_func);
	hdst->det_all_work_q =
		create_singlethread_workqueue("headset_detect_all");
	if (hdst->det_all_work_q == NULL) {
		pr_err("create_singlethread_workqueue for headset_detect failed!\n");
		goto failed_to_headset_detect_all;
	}

	INIT_DELAYED_WORK(&hdst->ldetl_work, headset_ldetl_work_func);
	hdst->ldetl_work_q =
		create_singlethread_workqueue("headset_ldetl");
	if (hdst->ldetl_work_q == NULL) {
		pr_err("create_singlethread_workqueue for headset_ldetl failed!\n");
		goto failed_to_headset_ldetl;
	}

	INIT_DELAYED_WORK(&hdst->reg_dump_work, headset_reg_dump_func);
	hdst->reg_dump_work_q =
		create_singlethread_workqueue("headset_reg_dump");
	if (hdst->reg_dump_work_q == NULL) {
		pr_err("create_singlethread_workqueue for headset_reg_dump failed!\n");
		goto failed_to_headset_reg_dump;
	}
	if (hdst->debug_level >= 2)
		queue_delayed_work(hdst->reg_dump_work_q,
			&hdst->reg_dump_work, msecs_to_jiffies(500));

	INIT_DELAYED_WORK(&hdst->fc_work, headset_fc_work_func);

	wakeup_source_init(&hdst->ldetl_wakelock,
		"headset_ldetl_wakelock");
	wakeup_source_init(&hdst->det_all_wakelock,
		"headset_detect_all_wakelock");
	wakeup_source_init(&hdst->btn_wakelock,
		"headset_button_wakelock");
	wakeup_source_init(&hdst->mic_wakelock,
		"headset_mic_wakelock");

	mutex_init(&hdst->irq_det_ldetl_lock);
	mutex_init(&hdst->irq_det_all_lock);
	mutex_init(&hdst->irq_btn_lock);
	mutex_init(&hdst->irq_det_mic_lock);
	mutex_init(&hdst->hmicbias_polling_lock);

	for (i = 0; i < HDST_GPIO_AUD_MAX; i++)
		gpio_set_debounce(pdata->gpios[i], pdata->dbnc_times[i] * 1000);

	headset_debug_sysfs_init();
	headset_adc_cal_from_efuse(hdst->pdev);
	sprd_headset_power_regulator_init(hdst);
	ret = devm_request_threaded_irq(
		dev, hdst->irq_detect_int_all, NULL,
		headset_detect_top_eic_handler,
		IRQF_TRIGGER_HIGH | IRQF_NO_SUSPEND | IRQF_ONESHOT,
		"head_aud_det_int_all", hdst);
	if (ret < 0) {
		pr_err("failed to request IRQ_%d(GPIO_%d)\n",
			hdst->irq_detect_int_all,
			pdata->gpios[HDST_GPIO_AUD_DET_INT_ALL]);
		goto failed_to_request_int_all_irq;
	}
	pr_info("%s devm_request_threaded_irq successful\n", __func__);
	/*can't call this after request irq */
	/*enable_irq(hdst->irq_detect_int_all);*/

	headset_reg_set_bits(ANA_HDT1, HEDET_PLGPD_EN);
	headset_reg_set_bits(ANA_HDT2, HEDET_MDET_EN);

	pr_debug("%s ANA_HDT1(00D4) %x, ANA_HDT2(00D8) %x\n",
		__func__, headset_reg_value_read(ANA_HDT1),
		headset_reg_value_read(ANA_HDT2));
	usleep_range(3000, 3500);
	headset_internal_eic_entry_init();

	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34, T35);

	return 0;
failed_to_request_int_all_irq:
	devm_free_irq(dev, hdst->irq_detect_int_all, hdst);
failed_to_headset_reg_dump:
	cancel_delayed_work_sync(&hdst->reg_dump_work);
	destroy_workqueue(hdst->reg_dump_work_q);
failed_to_headset_ldetl:
	destroy_workqueue(hdst->ldetl_work_q);
failed_to_headset_detect_all:
	destroy_workqueue(hdst->det_all_work_q);
failed_to_headset_button:
	destroy_workqueue(hdst->btn_work_q);
failed_to_headset_mic:
	destroy_workqueue(hdst->det_mic_work_q);

	return ret;
}

static struct gpio_map {
	int type;
	const char *name;
} gpio_map[] = {
	{HDST_GPIO_AUD_DET_INT_ALL, "aud_int_all"},
	{0, NULL},
};

#ifdef CONFIG_OF
static int sprd_headset_parse_dt(struct sprd_headset *hdst)
{
	struct sprd_headset_platform_data *pdata;
	struct device_node *np, *buttons_np = NULL;
	struct headset_buttons *buttons_data;
	u32 val;
	int index, ret, i;

	if (!hdst) {
		pr_err("%s sprd_hdst is NULL!\n", __func__);
		return -EINVAL;
	}

	np = hdst->pdev->dev.of_node;
	if (!np) {
		pr_err("%s No device node for headset!\n", __func__);
		return -ENODEV;
	}

	/* Parse configs for headset & button detecting. */
	pdata = &hdst->pdata;
	ret = of_property_read_u32(np, "sprd,jack-type", &val);
	if (ret < 0) {
		pr_err("%s: parse 'jack-type' failed!\n", __func__);
		pdata->jack_type = JACK_TYPE_NO;
	}
	pdata->jack_type = val ? JACK_TYPE_NC : JACK_TYPE_NO;
	pr_debug("%s jack_type %d\n", __func__, pdata->jack_type);
	if (pdata->jack_type == JACK_TYPE_NO) {
		/* 0 normal open(Tie High), 1 normal close(Tie low) */
		headset_reg_clr_bits(ANA_HDT0, HEDET_JACK_TYPE);
	} else if (pdata->jack_type == JACK_TYPE_NO) {
		headset_reg_set_bits(ANA_HDT0, HEDET_JACK_TYPE);
	}

	/* Parse gpios. */
	/* Parse for the gpio of EU/US jack type switch. */
	index = of_property_match_string(np, "gpio-names", "switch");
	if (index < 0) {
		pr_info("%s :no match found for switch gpio.\n", __func__);
		pdata->gpio_switch = 0;
	} else {
		ret = of_get_gpio_flags(np, index, NULL);
		if (ret < 0) {
			pr_err("%s :get gpio for 'switch' failed!\n", __func__);
			return -ENXIO;
		}
		pdata->gpio_switch = (u32)ret;
	}
	/* Parse for detecting gpios. */
	for (i = 0; gpio_map[i].name; i++) {
		const char *name = gpio_map[i].name;
		int type = gpio_map[i].type;

		index = of_property_match_string(np, "gpio-names", name);
		if (index < 0) {
			pr_err("%s :no match found for '%s' gpio\n",
			       __func__, name);
			return -ENXIO;
		}

		ret = of_get_gpio_flags(np, index, NULL);
		if (ret < 0) {
			pr_err("%s :get gpio for '%s' failed!\n",
			       __func__, name);
			return -ENXIO;
		}
		pdata->gpios[type] = (u32)ret;

		ret = of_property_read_u32_index(
			np, "sprd,debounce-interval", index, &val);
		if (ret < 0) {
			pr_err("%s :get debounce inteval for '%s' failed!\n",
				__func__, name);
			return ret;
		}
		pdata->dbnc_times[type] = val;

		pr_info("use GPIO_%u for '%s', debounce: %u\n",
			pdata->gpios[type],
			name,
			pdata->dbnc_times[type]);
	}

	ret = of_property_read_u32(np, "sprd,adc-threshold-3pole-detect",
		&pdata->adc_threshold_3pole_detect);
	if (ret) {
		pr_err("%s: fail to get adc-threshold-3pole-detect\n",
			__func__);
		return -ENXIO;
	}

	ret = of_property_read_u32(np, "sprd,adc-gnd",
		&pdata->sprd_adc_gnd);
	if (ret) {
		pr_err("%s: fail to get sprd-adc-gnd\n",
			__func__);
		return -ENXIO;
	}

	ret = of_property_read_u32(np, "sprd,stable-value",
		&pdata->sprd_stable_value);
	if (ret) {
		pr_err("%s: fail to get stable-value\n",
			__func__);
		return -ENXIO;
	}

	ret = of_property_read_u32(np, "sprd,coefficient",
		&pdata->coefficient);
	if (ret) {
		pr_err("%s: fail to get sprd-coefficient\n",
			__func__);
		return -ENXIO;
	}

	ret = of_property_read_u32(
		np, "sprd,irq-threshold-button", &pdata->irq_threshold_button);
	if (ret) {
		pr_err("%s: fail to get irq-threshold-button\n", __func__);
		return -ENXIO;
	}

	pdata->sprd_half_adc_gnd = pdata->sprd_adc_gnd >> 1;
	pdata->sprd_one_half_adc_gnd = pdata->sprd_adc_gnd +
					pdata->sprd_half_adc_gnd;
	pr_info("half_adc_gnd=%u, one_half_adc_gnd=%u, sprd_adc_gnd=%u",
		pdata->sprd_half_adc_gnd, pdata->sprd_one_half_adc_gnd,
		pdata->sprd_adc_gnd);
	pr_info("adc_threshold_3pole_detect=%u, sprd_stable_value=%u, coefficient %u, irq_threshold_button %u",
		pdata->adc_threshold_3pole_detect, pdata->sprd_stable_value,
		pdata->coefficient, pdata->irq_threshold_button);

	pdata->do_fm_mute = !of_property_read_bool(np, "sprd,no-fm-mute");

	/* Parse for buttons */
	pdata->nbuttons = of_get_child_count(np);
	buttons_data = devm_kzalloc(&hdst->pdev->dev,
		pdata->nbuttons*sizeof(*buttons_data), GFP_KERNEL);
	if (!buttons_data)
		return -ENOMEM;
	pdata->headset_buttons = buttons_data;

	for_each_child_of_node(np, buttons_np) {
		ret = of_property_read_u32(
			buttons_np, "adc-min", &buttons_data->adc_min);
		if (ret) {
			pr_err("%s: fail to get adc-min\n", __func__);
			return ret;
		}
		ret = of_property_read_u32(
			buttons_np, "adc-max", &buttons_data->adc_max);
		if (ret) {
			pr_err("%s: fail to get adc-min\n", __func__);
			return ret;
		}
		ret = of_property_read_u32(
			buttons_np, "code", &buttons_data->code);
		if (ret) {
			pr_err("%s: fail to get code\n", __func__);
			return ret;
		}
		pr_info("device tree data: adc_min = %d adc_max = %d code = %d\n",
			buttons_data->adc_min,
			buttons_data->adc_max, buttons_data->code);
		buttons_data++;
	};

	return 0;
}
#endif

/* Note: @pdev is the platform_device of headset node in dts. */
int sprd_headset_probe(struct platform_device *pdev)
{
	struct sprd_headset *hdst;
	struct sprd_headset_platform_data *pdata;
	struct device *dev = (pdev ? &pdev->dev : NULL);
	int i, ret;

#ifndef CONFIG_OF
	pr_err("%s: Only OF configurations are supported yet!\n", __func__);
	return -1;
#endif
	if (!pdev) {
		pr_err("%s: platform device is NULL!\n", __func__);
		return -1;
	}

	hdst = devm_kzalloc(dev, sizeof(*hdst), GFP_KERNEL);
	if (!hdst)
		return -ENOMEM;

	hdst->pdev = pdev;
	ret = sprd_headset_parse_dt(hdst);
	if (ret < 0) {
		pr_err("Failed to parse dt for headset.\n");
		return ret;
	}
	pdata = &hdst->pdata;

	if (pdata->gpio_switch != 0) {
		ret = devm_gpio_request(dev,
			pdata->gpio_switch, "headset_switch");
		if (ret < 0) {
			pr_err("failed to request GPIO_%d(headset_switch)\n",
				pdata->gpio_switch);
			return ret;
		}
	} else
		pr_info("automatic EU/US type switch is unsupported\n");

	for (i = 0; gpio_map[i].name; i++) {
		const char *name = gpio_map[i].name;
		int type = gpio_map[i].type;

		ret = devm_gpio_request(dev, pdata->gpios[type], name);
		if (ret < 0) {
			pr_err("failed to request GPIO_%d(%s)\n",
				pdata->gpios[type], name);
			return ret;
		}
		pr_debug("%s name %s, gpio %d\n", __func__,
			name, pdata->gpios[type]);
	}

	/* Get the adc channels of headset. */
	hdst->adc_chan = iio_channel_get(dev, "headmic_in_little");
	if (IS_ERR(hdst->adc_chan)) {
		pr_err("failed to get headmic in adc channel!\n");
		return PTR_ERR(hdst->adc_chan);
	}
	hdst->report = 0;
	sprd_hdst = hdst;

	pr_info("headset_detect_probe success\n");

	return 0;
}
EXPORT_SYMBOL(sprd_headset_probe);

static int headset_adc_get_ideal(u32 adc_mic, u32 coefficient)
{
	int64_t numerator, denominator, exp1, exp2, exp3, exp4;
	u32 divisor, adc_ideal, a, b, e1, e2;
	u64 dividend;

	if (adc_cal_headset.cal_type != SPRD_HEADSET_AUXADC_CAL_DO) {
		pr_warn("%s efuse A,B,E hasn't been calculated!\n", __func__);
		return adc_mic;
	}

	a = adc_cal_headset.A;
	b = adc_cal_headset.B;
	e1 = adc_cal_headset.E1;
	e2 = adc_cal_headset.E2;

	if (9*adc_mic + b < 10*a)
		return adc_mic;

	/*1.22v new calibration need*/
	exp1 =  ((int64_t)e1 - (int64_t)e2);
	exp2 = (9 * (int64_t)adc_mic - 10 * (int64_t)a + (int64_t)b);
	exp3 = 2400 * (int64_t)e1 * ((int64_t)b - (int64_t)a);
	exp4 = 100 * exp2 * ((int64_t)e1 - (int64_t)e2 - 1200);

	pr_debug("exp1=%lld, exp2=%lld, exp3=%lld, exp4=%lld\n",
		exp1, exp2, exp3, exp4);
	denominator = exp3 + exp4;
	numerator = coefficient * (exp1 + 1200) * exp2;
	pr_debug("denominator=%lld, numerator=%lld\n",
			denominator, numerator);

	/*
	 * enable the denominator * 0.01
	 * numerator * 0.01 at the same time
	 * for do_div() argument divisor is u32
	 * and dividend is u64 but divsor is over 32bit
	 */
	do_div(denominator, 100);
	do_div(numerator, 100);
	pr_debug("%s denominator=%lld, numerator=%lld\n",
			__func__, denominator, numerator);

	divisor = (u32)(denominator);
	dividend = (u64)(numerator);
	pr_info("%s divisor=%u, dividend=%llu\n", __func__, divisor, dividend);

	do_div(dividend, divisor);
	adc_ideal = (u32)dividend;
	pr_info("%s adc_mic=%d, adc_ideal=%d\n", __func__, adc_mic, adc_ideal);

	return adc_ideal;
}

static int headset_adc_efuse_bits_read(struct platform_device *pdev,
					const char *cell_name, u32 *data)
{
	struct nvmem_cell *cell;
	u32 calib_data;
	void *buf;
	size_t len;

	cell = nvmem_cell_get(&pdev->dev, cell_name);
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(&calib_data, buf, min(len, sizeof(u32)));
	*data = calib_data;
	kfree(buf);

	return 0;
}

static int headset_adc_cal_from_efuse(struct platform_device *pdev)
{
	u8 delta[4];
	u32 test[2], ret, data;
	unsigned int adie_chip_id;

	pr_info("%s enter\n", __func__);
	if (adc_cal_headset.cal_type != SPRD_HEADSET_AUXADC_CAL_NO) {
		pr_info("%s efuse A,B,E has been calculated already!\n",
			__func__);
		return -EINVAL;
	}

	ret = headset_adc_efuse_bits_read(pdev, "hp_adc_fir_calib", &data);
	if (ret)
		goto adc_cali_error;
	test[0] = data;
	ret = headset_adc_efuse_bits_read(pdev, "hp_adc_sec_calib", &data);
	if (ret)
		goto adc_cali_error;
	test[1] = data;

	delta[0] = test[0] & 0xFF;
	delta[1] = (test[0] & 0xFF00) >> 8;
	delta[2] = test[1] & 0xFF;
	delta[3] = (test[1] & 0xFF00) >> 8;

	pr_info("%s test[0] 0x%x %d, test[1] 0x%x %d\n",
		__func__, test[0], test[0], test[1], test[1]);

	pr_info("%s d[0] %#x %d d[1] %#x %d d[2] %#x %d d[3] %#x %d\n",
			__func__, delta[0], delta[0], delta[1], delta[1],
			delta[2], delta[2],  delta[3], delta[3]);

	adc_cal_headset.cal_type = SPRD_HEADSET_AUXADC_CAL_DO;
	adie_chip_id  = sci_get_ana_chip_id();
	adie_chip_id = (adie_chip_id >> 16) & 0xffff;
	pr_info("%s adie_chip_id 0x%x\n", __func__, adie_chip_id);
	if (adie_chip_id == CHIP_ID_2720) {
		adc_cal_headset.A = (delta[0] - 128 + 80) * 4;
		adc_cal_headset.B =  (delta[1] - 128 + 833) * 4;
		adc_cal_headset.E1 = delta[2] * 2 + 2500;
		adc_cal_headset.E2 = delta[3] * 4 + 1300;
	} else if (adie_chip_id == CHIP_ID_2730) {
		adc_cal_headset.A = (delta[0] - 128) * 4 + 336;
		adc_cal_headset.B =  (delta[1] - 128) * 4 + 3357;
		adc_cal_headset.E1 = delta[2] * 2 + 2500;
		adc_cal_headset.E2 = delta[3] * 4 + 1300;
	} else {
		adc_cal_headset.A = (delta[0] - 128) * 4 + 336;
		adc_cal_headset.B =  (delta[1] - 128) * 4 + 3357;
		adc_cal_headset.E1 = delta[2] * 2 + 2500;
		adc_cal_headset.E2 = delta[3] * 4 + 1300;
	}
	pr_info("%s A %d, B %d E1 %d E2 %d\n",
		__func__, adc_cal_headset.A, adc_cal_headset.B,
		adc_cal_headset.E1, adc_cal_headset.E2);
	return 0;

adc_cali_error:
	adc_cal_headset.cal_type = SPRD_HEADSET_AUXADC_CAL_NO;
	pr_err("%s, error: headset adc calibration fail %d\n", __func__, ret);
	return ret;
}

MODULE_DESCRIPTION("headset & button detect driver v2");
MODULE_AUTHOR("Yaochuan Li <yaochuan.li@spreadtrum.com>");
MODULE_LICENSE("GPL");
