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

#define HDST_DEBUG_LOG pr_debug("%s %d\n", __func__, __LINE__)

#define LDETL_WAIT_INSERT_ALL_COMPL_MS 2000
#define INSERT_ALL_WAIT_MDET_COMPL_MS 2000
#define INSERT_ALL_WAIT_LDETL_COMPL_MS 3000
#define MAX_BUTTON_NUM 6
#define LDETL_REF_SEL_20mV 1
#define LDETL_REF_SEL_50mV 2
#define LDETL_REF_SEL_100mV 3

#define SPRD_HEADSET_JACK_MASK (SND_JACK_HEADSET)
#define SPRD_BUTTON_JACK_MASK (SND_JACK_BTN_0 | SND_JACK_BTN_1 | \
	SND_JACK_BTN_2 | SND_JACK_BTN_3 | SND_JACK_BTN_4)

#define ADC_READ_REPET 10
#define ADC_READ_BTN_COUNT 20

#define CHIP_ID_2720 0x2720
#define CHIP_ID_2730 0x2730

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

const char * const eic_name[HDST_ALL_EIC + 1] = {
	[HDST_INSERT_ALL_EIC] = TO_STRING(HDST_INSERT_ALL_EIC),
	[HDST_MDET_EIC] = TO_STRING(HDST_MDET_EIC),
	[HDST_LDETL_EIC] = TO_STRING(HDST_LDETL_EIC),
	[HDST_LDETH_EIC] = TO_STRING(HDST_LDETH_EIC),
	[HDST_GDET_EIC] = TO_STRING(HDST_GDET_EIC),
	[HDST_BDET_EIC] = TO_STRING(HDST_BDET_EIC),
	[HDST_ALL_EIC] = TO_STRING(HDST_ALL_EIC),
};

const char * const insert_name[HDST_INSERT_MAX] = {
	[HDST_INSERT_MDET] = TO_STRING(HDST_INSERT_MDET),
	[HDST_INSERT_LDETL] = TO_STRING(HDST_INSERT_LDETL),
	[HDST_INSERT_LDETH] = TO_STRING(HDST_INSERT_LDETH),
	[HDST_INSERT_GDET] = TO_STRING(HDST_INSERT_GDET),
	[HDST_INSERT_BDET] = TO_STRING(HDST_INSERT_BDET),
	[HDST_INSERT_ALL] = TO_STRING(HDST_INSERT_ALL),
};

const char * const eic_type_string[EIC_TYPE_MAX] = {
	[LDETL_PLUGIN] = TO_STRING(LDETL_PLUGIN),
	[INSERT_ALL_PLUGOUT] = TO_STRING(INSERT_ALL_PLUGOUT),
	[MDET_EIC] = TO_STRING(MDET_EIC),
	[BDET_EIC] = TO_STRING(BDET_EIC),
	[TYPE_RE_DETECT] = TO_STRING(TYPE_RE_DETECT),
	[BTN_PRESS] = TO_STRING(BTN_PRESS),
	[BTN_RELEASE] = TO_STRING(BTN_RELEASE),
	[INSERT_ALL_PLUGIN] = TO_STRING(INSERT_ALL_PLUGIN),
	[LDETL_PLUGOUT] = TO_STRING(LDETL_PLUGOUT),
};

const char * const eic_hw_state[HW_BTN_RELEASE + 1] = {
	[HW_LDETL_PLUG_OUT] = TO_STRING(HW_LDETL_PLUG_OUT),
	[HW_INSERT_ALL_PLUG_OUT] = TO_STRING(HW_INSERT_ALL_PLUG_OUT),
	[HW_LDETL_PLUG_IN] = TO_STRING(HW_LDETL_PLUG_IN),
	[HW_INSERT_ALL_PLUG_IN] = TO_STRING(HW_INSERT_ALL_PLUG_IN),
	[HW_MDET] = TO_STRING(HW_MDET),
	[HW_BTN_PRESS] = TO_STRING(HW_BTN_PRESS),
	[HW_BTN_RELEASE] = TO_STRING(HW_BTN_RELEASE),
};

char *regu_name_list[HDST_REGULATOR_COUNT] = {
	"VREG",
	"VB",
	"BG",
	"BIAS",
	"MICBIAS1",
	"MICBIAS2",
	"HEADMICBIAS",
	"DCL",
	"DIG_CLK_INTC",
	"DIG_CLK_HID",
	"CLK_DCL_32K",
};

static unsigned int sprd_read_reg_value(unsigned int reg)
{
	unsigned int ret_val;

	sci_adi_read(CODEC_REG((reg)), &ret_val);

	return ret_val;
}

#define LG "%s STS0(184) %x, INT5(214.IEV) %x, INT6(218.IE) %x, INT7(21C.RIS) %x, INT8(220.MIS) %x, INT11(22C.STS1) %x, INT32(280.INTC.EN) %x, INT34(288) %x"

#define FC __func__
#define S0 sprd_read_reg_value(ANA_STS0)
#define T5 sprd_read_reg_value(ANA_INT5)
#define T6 sprd_read_reg_value(ANA_INT6)
#define T7 sprd_read_reg_value(ANA_INT7)
#define T8 sprd_read_reg_value(ANA_INT8)
#define T11 sprd_read_reg_value(ANA_INT11)
#define T32 sprd_read_reg_value(ANA_INT32)
#define T34 sprd_read_reg_value(ANA_INT34)

int dsp_fm_mute_by_set_dg(void)
	__attribute__ ((weak, alias("__dsp_fm_mute_by_set_dg")));

static int __dsp_fm_mute_by_set_dg(void)
{
	pr_err("ERR: dsp_fm_mute_by_set_dg is not defined!\n");
	return -1;
}

static inline int sprd_get_reg_bits(unsigned int reg, int bits)
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

int vbc_close_fm_dggain(bool mute)
	__attribute__ ((weak, alias("__vbc_close_fm_dggain")));
static int __vbc_close_fm_dggain(bool mute)
{
	pr_err("ERR: vbc_close_fm_dggain is not defined!\n");
	return -1;
}

static void sprd_enable_hmicbias_polling(bool enable, bool force_disable);
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
		return -EINVAL;
	}

	pr_info("%s, %s\n", __func__, on ? "on" : "off");
	kctrl = snd_ctl_find_id(card->snd_card, &id);
	if (!kctrl) {
		pr_err("%s can't find kctrl '%s'\n", __func__, id.name);
		return -EINVAL;
	}

	return snd_soc_dapm_put_volsw(kctrl, &ucontrol);
}

static void sprd_headset_jack_report(struct sprd_headset *hdst,
	struct snd_soc_jack *jack, int status, int mask)
{
	if (mask & SND_JACK_HEADPHONE)
		dapm_jack_switch_control(hdst->codec, !!status);

	snd_soc_jack_report(jack, status, mask);
}

static enum snd_jack_types sprd_jack_type_get(int index)
{
	enum snd_jack_types jack_type_map[MAX_BUTTON_NUM] = {
		SND_JACK_BTN_0, SND_JACK_BTN_1, SND_JACK_BTN_2,
		SND_JACK_BTN_3, SND_JACK_BTN_4, SND_JACK_BTN_5
	};

	return jack_type_map[index];
}

static int sprd_headset_power_init(struct headset_power_manager *power_manager,
	struct platform_device *pdev)
{
	struct headset_power *power = power_manager->power;
	struct regulator *regulator;
	int ret = 0, i;

	if (!power) {
		pr_err("%s power(%p) is null!\n",
			__func__, power);
		return -EINVAL;
	}

	for (i = 0; i < HDST_REGULATOR_COUNT; i++) {
		if (!regu_name_list[i]) {
			regulator_put(power[i].hdst_regu);
			power[i].hdst_regu = NULL;
			power[i].name = NULL;
			power[i].index = i;
			pr_err("%s regu_name_list[%d] name NULL\n",
				__func__, i);
			continue;
		}
		regulator = regulator_get(&pdev->dev, regu_name_list[i]);
		if (IS_ERR(regulator)) {
			pr_err("%s Failed to request %ld: %s\n",
				__func__, PTR_ERR(regulator),
				regu_name_list[i]);
			ret = PTR_ERR(regulator);
			regulator = NULL;
			goto __error;
		}

		power[i].hdst_regu = regulator;
		power[i].name = regu_name_list[i];
		power[i].index = i;

		pr_debug("%s index %d, name %s\n", __func__, power[i].index,
			power[i].name);
	}

	return ret;

__error:
	sprd_headset_power_deinit();
	return ret;
}

void sprd_headset_power_deinit(void)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct headset_power *power = hdst->power_manager.power;
	struct headset_power *power_temp;
	int i = 0;

	while (i++ < HDST_REGULATOR_COUNT) {
		power_temp = &power[i];
		if (power_temp->hdst_regu) {
			regulator_put(power_temp->hdst_regu);
			power_temp->hdst_regu = NULL;
			power_temp->name = NULL;
		}
	}
}

static struct headset_power *sprd_headset_search_power(
	struct headset_power_manager *power_manager, char *name)
{
	int i = 0;
	struct headset_power *power_array = power_manager->power;

	while (i++ < HDST_REGULATOR_COUNT) {
		if (strcmp(power_array[i].name, name) == 0)
			break;
	}
	if (i >= HDST_REGULATOR_COUNT) {
		pr_err("%s: wrong regu name %s\n", __func__, name);
		return NULL;
	}
	pr_debug("%s i %d\n", __func__, i);

	return &power_array[i];
}

static int sprd_headset_power_set(struct headset_power_manager *power_manager,
	char *name, bool power_on)
{
	struct headset_power *power;
	int ret;

	power = sprd_headset_search_power(power_manager, name);
	if (!power) {
		pr_err("%s  power NULL\n", __func__);
		return -EINVAL;
	}

	pr_info("%s name %s, power_on %d, en %d\n",
		__func__, power->name, power_on,
		power->en);

	if (power_on && !power->en) {
		ret = regulator_enable(power->hdst_regu);
		if (!ret)
			power->en = true;
	} else if (!power_on && power->en) {
		ret = regulator_disable(power->hdst_regu);
		if (!ret)
			power->en = false;
	}

	return ret;
}

static void sprd_hmicbias_mode_set(struct sprd_headset *hdst,
	unsigned int mode)
{
	struct headset_power *power_micbias;
	int ret;

	power_micbias = sprd_headset_search_power(&hdst->power_manager,
		"HEADMICBIAS");
	ret = regulator_is_enabled(power_micbias->hdst_regu);
	if (ret > 0)
		regulator_set_mode(power_micbias->hdst_regu, mode);
}

static BLOCKING_NOTIFIER_HEAD(hp_chain_list);
int headset_register_notifier(struct notifier_block *nb)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);

	if (!hdst) {
		pr_err("%s sprd_hdset is NULL!\n", __func__);
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
	if (nb == NULL) {
		pr_err("%s nb NULL error\n", __func__);
		return -1;
	}

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

static int sprd_headset_adc_get(struct iio_channel *chan)
{
	int val, ret;

	ret = iio_read_channel_raw(chan, &val);
	if (ret < 0) {
		pr_err("%s: read adc raw value failed!\n", __func__);
		return ret;
	}
	pr_debug("%s: adc: %d\n", __func__, val);

	return val;
}

static void sprd_intc_force_clear(int force_clear)
{
	if (force_clear == 1) {
		/* clear bit14 of reg 0x288, reg 0x28c, clear all analog intc */
		headset_reg_write_force(ANA_INT33, 0x4000, 0xffff);
		pr_info("%s INT8(220.MIS) %x\n", __func__,
			sprd_read_reg_value(ANA_INT8));
	} else if (force_clear == 0) {
		/*set bit14 to 0, equal to enable intc */
		headset_reg_write_force(ANA_INT33, 0x0, 0xffff);
	}
}

static inline int sprd_intc_irq_status(void)
{
	return sprd_read_reg_value(ANA_INT34);
}

static inline bool sprd_intc_type_status(enum intc_type intc_type)
{
	return (BIT(intc_type) & sprd_read_reg_value(ANA_INT34)) > 0;
}

/* clear one bit of reg0x21c and reg 0x220 */
static void sprd_headset_eic_clear(enum hdst_eic_type eic_type)
{
	headset_reg_set_bits(ANA_INT9, BIT(eic_type));
	pr_debug("%s clear %s\n", __func__, eic_name[eic_type]);
}

/* clear all bits of reg 0x21c, reg 0x220, clear all eic irq */
static void sprd_headset_clear_all_eic(void)
{
	headset_reg_write(ANA_INT9, EIC_DBNC_IC(0xfc00),
	EIC_DBNC_IC(0xffff));
	pr_info("%s clear all internal eic\n", __func__);
}

static void sprd_headset_eic_trig(enum hdst_eic_type eic_type)
{
	headset_reg_set_bits(ANA_INT10, BIT(eic_type));
	pr_debug(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34);
}

static void sprd_hmicbias_hw_control_enable(bool enable)
{
	if (enable)
		headset_reg_set_bits(ANA_HDT1, HEDET_PLGPD_EN);
	else
		headset_reg_clr_bits(ANA_HDT1, HEDET_PLGPD_EN);
}

static void sprd_headset_eic_enable(enum hdst_eic_type eic_type, bool enable)
{
	if (enable)
		headset_reg_set_bits(ANA_INT6, BIT(eic_type));
	else
		headset_reg_clr_bits(ANA_INT6, BIT(eic_type));

	pr_debug("%s %s %s\n", __func__,
		enable ? "enable" : "disable", eic_name[eic_type]);
}

static void sprd_headset_all_eic_enable(bool enable)
{
	if (enable)
		headset_reg_write(ANA_INT6, EIC_DBNC_IE(0xfc00),
			EIC_DBNC_IE(0xffff));
	else
		headset_reg_write(ANA_INT6, EIC_DBNC_IE(0x0000),
			EIC_DBNC_IE(0xffff));

	pr_info("%s %s all internal eic\n", __func__,
		enable ? "enable" : "disable");
}

static void sprd_ldetl_filter_enable(bool enable)
{
	if (enable)
		headset_reg_set_bits(ANA_HDT2, HEDET_LDETL_FLT_EN);
	else
		headset_reg_clr_bits(ANA_HDT2, HEDET_LDETL_FLT_EN);
}

static void sprd_headset_eic_plugin_enable(void)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);

	if (pdata->jack_type == JACK_TYPE_NO) {
		/* enalbe ldetl eic */
		sprd_headset_eic_enable(HDST_LDETL_EIC, true);
		sprd_headset_eic_trig(HDST_LDETL_EIC);
	} else if (pdata->jack_type == JACK_TYPE_NC) {
		/* enalbe detect_all eic */
		sprd_headset_eic_enable(HDST_INSERT_ALL_EIC, true);
		sprd_headset_eic_trig(HDST_INSERT_ALL_EIC);
	}
	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34);
}

static inline bool sprd_headset_part_is_inserted(
	enum hdst_insert_signal insert_part)
{
	return (BIT(insert_part) & sprd_read_reg_value(ANA_STS0)) > 0;
}

static inline unsigned int sprd_get_eic_mis_status(
	enum hdst_eic_type eic_type)
{
	return (BIT(eic_type) & sprd_read_reg_value(ANA_INT8)) > 0;
}

static inline int sprd_get_all_eic_mis_status(void)
{
	return sprd_read_reg_value(ANA_INT8);
}

static void sprd_set_eic_trig_level(enum hdst_eic_type eic_type,
	bool trig_level)
{
	if (trig_level)
		headset_reg_set_bits(ANA_INT5, BIT(eic_type));
	else
		headset_reg_clr_bits(ANA_INT5, BIT(eic_type));
	pr_debug("%s set %s trig level to %s\n",
			__func__, eic_name[eic_type],
			trig_level ? "high" : "low");
}

static void sprd_set_all_eic_trig_level(bool trig_level)
{
	if (trig_level)
		headset_reg_write(ANA_INT5, EIC_DBNC_IEV(0xffff),
			EIC_DBNC_IEV(0xffff));
	else
		headset_reg_write(ANA_INT5, EIC_DBNC_IEV(0x0),
			EIC_DBNC_IEV(0xffff));
	pr_info("%s set all internal eic trig level to %s\n",
		__func__, trig_level ? "high" : "low");
}

static inline bool sprd_get_eic_trig_level(enum hdst_eic_type eic_type)
{
	return (BIT(eic_type) & sprd_read_reg_value(ANA_INT5)) > 0;
}

static inline bool sprd_headset_eic_get_data(enum hdst_eic_type eic_type)
{
	return (BIT(eic_type) & sprd_read_reg_value(ANA_INT0)) > 0;
}

static int sprd_headset_regulator_init(struct sprd_headset *hdst)
{
	int ret;

	/*
	 * si.chen ask to enable these all the time after bootup,
	 * VB is supply of BG,
	 * DCL is supply of CLK_DCL_32K DIG_CLK_INTC DIG_CLK_HID
	 * so do not enable VB and DCL separately
	 */
	ret = sprd_headset_power_set(&hdst->power_manager, "CLK_DCL_32K",
		true);
	if (ret) {
		pr_err("%s regulator CLK_DCL_32K set failed\n", __func__);
		return ret;
	}
	ret = sprd_headset_power_set(&hdst->power_manager, "DIG_CLK_INTC",
		true);
	if (ret) {
		pr_err("%s regulator DIG_CLK_INTC set failed\n", __func__);
		return ret;
	}

	pr_info("%s PMU0(0000) %x, PMU1(0004) %x, DCL1(0100) %x, CLK0(0068) %x\n",
		__func__, sprd_read_reg_value(ANA_PMU0),
		sprd_read_reg_value(ANA_PMU1), sprd_read_reg_value(ANA_DCL1),
		sprd_read_reg_value(ANA_CLK0));

	return 0;
}

static void sprd_hmicbias_polling_init(struct sprd_headset *hdst)
{
	headset_reg_write(ANA_HID0, HID_DBNC_EN(0x3),
		HID_DBNC_EN(0x3));
	headset_reg_write(ANA_HID0, HID_TMR_CLK_SEL(0x2),
		HID_TMR_CLK_SEL(0x3));
	headset_reg_write(ANA_HID1, HID_HIGH_DBNC_THD0(0x01),
		HID_HIGH_DBNC_THD0(0xff));
	headset_reg_write(ANA_HID1, HID_LOW_DBNC_THD0(0x01),
		HID_LOW_DBNC_THD0(0xff));
	headset_reg_write(ANA_HID2, HID_HIGH_DBNC_THD1(0x80),
		HID_HIGH_DBNC_THD1(0xff));
	headset_reg_write(ANA_HID2, HID_LOW_DBNC_THD1(0x80),
		HID_LOW_DBNC_THD1(0xff));
	headset_reg_write(ANA_HID3, HID_TMR_T1(0x8),
		HID_TMR_T1(0xffff));
	headset_reg_write(ANA_HID4, HID_TMR_T2(0x20),
		HID_TMR_T2(0xffff));
	hdst->current_polling_state = true;
	pr_info("%s HID0(144) %x, HID1(148) %x, HID2(14C) %x, HID3(150) %x, HID4(154) %x",
		__func__, sprd_read_reg_value(ANA_HID0),
		sprd_read_reg_value(ANA_HID1), sprd_read_reg_value(ANA_HID2),
		sprd_read_reg_value(ANA_HID3), sprd_read_reg_value(ANA_HID4));
}

static void sprd_headset_intc_enable(bool enable)
{
	if (enable)
		headset_reg_set_bits(ANA_INT32, ANA_INT_EN);
	else
		headset_reg_clr_bits(ANA_INT32, ANA_INT_EN);
}

/*
 * Si.chen ask to set val like this:
 * 0x3 for 3 pole and selfie stick,
 * 0x1 for 4 pole normal
 */
static void sprd_headset_ldetl_ref_sel(unsigned int val)
{
	headset_reg_write(ANA_HDT2, val, HEDET_LDETL_REF_SEL(0x7));
}

static void sprd_eic_hardware_debounce_set(unsigned int reg, unsigned int ms)
{
	unsigned int val;

	val = ms + 0x4000;
	headset_reg_write(reg, val, EIC10_DBNC_CTRL(0xffff));
	pr_debug("%s reg 0x%x, ms %d, val %x\n", __func__,
		reg - CTL_BASE_AUD_CFGA_RF, ms, val);
}

static void sprd_headset_eic_init(void)
{
	/* detect ref enable */
	headset_reg_set_bits(ANA_HDT0, HEDET_VREF_EN);
	headset_reg_set_bits(ANA_HDT2, HEDET_LDETL_EN);
	headset_reg_set_bits(ANA_HDT2, HEDET_LDETH_EN);
	headset_reg_set_bits(ANA_HDT0, HEDET_GDET_EN);

	headset_reg_set_bits(ANA_HDT2, HEDET_MDET_EN);
	headset_reg_set_bits(ANA_PMU2, BIT(12));/* BIAS_RSV1 */

	/* EIC_DBNC_DATA register can be read if EIC_DBNC_DMSK set 1 */
	headset_reg_write(ANA_INT1, EIC_DBNC_DMSK(0xffff),
		EIC_DBNC_DMSK(0xffff));
	pr_info("%s INT0(0200) %x, INT1(0204) %x, INT6(0218) %x\n",
		__func__, sprd_read_reg_value(ANA_INT0),
		sprd_read_reg_value(ANA_INT1), sprd_read_reg_value(ANA_INT6));

	/* clear reg 0x21c, reg 0x220, clear all eic irq */
	sprd_headset_clear_all_eic();

	/* set hardware debouce for internal EIC */
	sprd_eic_hardware_debounce_set(ANA_INT26, 2);
	sprd_eic_hardware_debounce_set(ANA_INT27, 50);
	sprd_eic_hardware_debounce_set(ANA_INT28, 2);
	sprd_eic_hardware_debounce_set(ANA_INT29, 2);
	sprd_eic_hardware_debounce_set(ANA_INT30, 2);
	sprd_eic_hardware_debounce_set(ANA_INT31, 6);

	/* init internal EIC */
	sprd_set_all_eic_trig_level(true);
	sprd_headset_all_eic_enable(false);
	sprd_intc_force_clear(1);
	sprd_intc_force_clear(0);

	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34);
}

static int sprd_detect_reg_init(void)
{
	int ret;
	struct sprd_headset *hdst = sprd_hdst;

	ret = sprd_headset_regulator_init(hdst);
	if (ret) {
		pr_err("%s regulator init failed\n", __func__);
		return ret;
	}
	sprd_hmicbias_polling_init(hdst);
	sprd_headset_intc_enable(true);
	sprd_hmicbias_hw_control_enable(true);
	sprd_headset_eic_init();
	return 0;
}

static void sprd_headset_scale_set(int large_scale)
{
	if (large_scale)
		headset_reg_set_bits(ANA_HDT3, HEDET_V2AD_SCALE_SEL);
	else
		headset_reg_clr_bits(ANA_HDT3, HEDET_V2AD_SCALE_SEL);
}

static void sprd_button_irq_threshold(int enable)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = hdst ? &hdst->pdata : NULL;
	int audio_head_sbut;
	unsigned long msk, val;

	audio_head_sbut = pdata->irq_threshold_button;
	msk = HEDET_BDET_REF_SEL(0x7);
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

static int sprd_adc_to_ideal(u32 adc_mic, u32 coefficient);

static int sprd_get_adc_value(struct iio_channel *chan)
{
	int adc_value, i = 0, avrage = 0;

	while (i++ < ADC_READ_REPET) {
		/* head buffer not swap */
		headset_reg_clr_bits(ANA_HDT3, HEDET_V2AD_SWAP);
		adc_value = sprd_headset_adc_get(chan);
		if (adc_value < 0)
			return adc_value;
		/* head buffer swap input */
		headset_reg_set_bits(ANA_HDT3, HEDET_V2AD_SWAP);
		adc_value = sprd_headset_adc_get(chan) + adc_value;
		if (adc_value < 0)
			return adc_value;
		avrage += adc_value / 2;
	}
	return avrage / ADC_READ_REPET;
}

static int sprd_button_ideal_adc(struct sprd_headset *hdst)
{
	struct sprd_headset_platform_data *pdata = &hdst->pdata;
	struct iio_channel *chan = hdst->adc_chan;
	int adc_mic_average = 0, adc_ideal, adc_value,
		i = 0, insert_state, did_times;

	headset_reg_set_bits(ANA_HDT3, HEDET_V2AD_EN);
	sprd_headset_scale_set(0);
	headset_reg_write(ANA_HDT3, HEDET_V2AD_CH_SEL(0),
		HEDET_V2AD_CH_SEL(0xf));

	while (i < ADC_READ_BTN_COUNT) {
		/* head buffer not swap */
		headset_reg_clr_bits(ANA_HDT3, HEDET_V2AD_SWAP);
		adc_value = sprd_headset_adc_get(chan);
		if (adc_value < 0) {
			adc_mic_average = 4095;
			goto read_adc_err;
		}
		/* head buffer swap input */
		headset_reg_set_bits(ANA_HDT3, HEDET_V2AD_SWAP);
		adc_value = sprd_headset_adc_get(chan) + adc_value;
		if (adc_value < 0) {
			adc_mic_average = 4095;
			goto read_adc_err;
		}
		insert_state = sprd_headset_eic_get_data(HDST_BDET_EIC);
		if (insert_state == 0) {
			pr_err("%s key released! i %d, adc_value %d, insert_state %d\n",
				__func__, i, adc_value / 2, insert_state);
			did_times = i;
			break;
		}
		adc_mic_average += adc_value / 2;
		i++;
	}
	if (i == 0) {
		adc_mic_average = 4095;
		goto read_adc_err;
	}

	adc_mic_average /= i;

	if (adc_mic_average < 0) {
		pr_err("%s adc error, adc_mic_average %d\n",
			__func__, adc_mic_average);
		/*
		 * When adc value is negative, it is invalid,
		 * set a useless value to it, like 4095 in
		 * buttons.
		 */
		adc_mic_average = 4095;
	}
read_adc_err:
	adc_ideal = sprd_adc_to_ideal(adc_mic_average,
				pdata->coefficient);
	pr_info("%s adc_mic_average %d, adc_ideal=%d, V_ideal %s %d mV\n",
		__func__, adc_mic_average, adc_ideal,
		(adc_mic_average >= 4095) ? "outrange!" : "",
		adc_ideal * 1250 / 4095);
	if (adc_ideal >= 0)
		adc_mic_average = adc_ideal;

	return adc_mic_average;
}

static void sprd_enable_hmicbias_polling(bool enable, bool force_disable)
{
	struct sprd_headset *hdst = sprd_hdst;

	if (!force_disable) {
		if (hdst->plug_state_last == 0) {
			pr_err("%s no headset insert!\n", __func__);
			return;
		}
		mutex_lock(&hdst->btn_detecting_lock);
		if ((hdst->hdst_type_status & SND_JACK_MICROPHONE) == 0 ||
			hdst->btn_detecting) {
			pr_err("%s no headset plugin or button pressing 0x%x\n",
				__func__, hdst->hdst_type_status);
			mutex_unlock(&hdst->btn_detecting_lock);
			return;
		}
		mutex_unlock(&hdst->btn_detecting_lock);
	}

	pr_info("%s %s set polling to %s, current_polling_state %d\n",
		__func__, force_disable ? "force" : "",
		enable ? "enable" : "disable", hdst->current_polling_state);

	mutex_lock(&hdst->hmicbias_polling_lock);
	if (enable) {
		if (hdst->current_polling_state == false) {
			sprd_hmicbias_mode_set(hdst, REGULATOR_MODE_STANDBY);
			sprd_headset_power_set(&hdst->power_manager,
				"DIG_CLK_HID", true);
			headset_reg_set_bits(ANA_HID0, HID_EN);
			hdst->current_polling_state = true;
		}
	} else {
		if (hdst->current_polling_state == true) {
			headset_reg_clr_bits(ANA_HID0, HID_EN);
			sprd_headset_power_set(&hdst->power_manager,
				"DIG_CLK_HID", false);
			sprd_hmicbias_mode_set(hdst, REGULATOR_MODE_NORMAL);
			hdst->current_polling_state = false;
		}
	}
	mutex_unlock(&hdst->hmicbias_polling_lock);
}

void headset_set_audio_state(bool on)
{
	struct sprd_headset *hdst = sprd_hdst;

	mutex_lock(&hdst->audio_on_lock);
	hdst->audio_on = on;
	mutex_unlock(&hdst->audio_on_lock);
	sprd_enable_hmicbias_polling(!on, false);
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
	int check_times = 300;

	pr_info("%s enter\n", __func__);
	pr_debug(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34);

	/* need to disable/enable irq 10 in this func? i am not sure */
	sprd_headset_eic_enable(10, 0);
	sprd_headset_eic_clear(10);
	sprd_headset_eic_enable(11, 1);/* enalbe mdet */
	sprd_headset_eic_trig(11);/* trig mdet */
	sprd_intc_force_clear(0);
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

	sprd_headset_eic_enable(10, 1);
	sprd_headset_eic_enable(11, 0);/* disalbe mdet */
	sprd_headset_eic_clear(11);
	hdst->mic_irq_trigged = 0;
	pr_info("%s, headset_type = %d (%s), check_times %d\n",
		__func__, headset_type,
		(headset_type == HEADSET_NO_MIC) ?
		"HEADSET_NO_MIC" : "HEADSET_4POLE_NORMAL", check_times);
	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34);

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
	bool adc_value_err = false;
	enum sprd_headset_type headset_type;

	HDST_DEBUG_LOG;

	if (!hdst || !adc_chan) {
		pr_err("%s: sprd_hdset(%p) or adc_chan(%p) is NULL!\n",
			__func__, hdst, adc_chan);
		return HEADSET_TYPE_ERR;
	}

	pdata = &hdst->pdata;
	if (pdata->eu_us_switch != 0)
		gpio_direction_output(pdata->eu_us_switch, 0);
	else
		pr_info("automatic type switch is unsupported\n");

	pr_debug("%s PMU0(0000) %x\n", __func__,
		sprd_read_reg_value(ANA_PMU0));
	/*
	 * changing to 4ms according to si.chen's email,
	 * make sure the whole time is in 10ms
	 */
	sprd_msleep(4);

	pr_debug("%s, get adc value of headmic in little scale\n", __func__);

	headset_reg_set_bits(ANA_HDT3, HEDET_V2AD_EN);
	/* 0 little, 1 large */
	sprd_headset_scale_set(0);
	headset_reg_write(ANA_HDT3, HEDET_V2AD_CH_SEL(0),
		HEDET_V2AD_CH_SEL(0xf));
	adc_mic_average = sprd_get_adc_value(adc_chan);
	if (adc_mic_average < 0) {
		pr_err("%s adc error, adc_mic_average %d\n",
			__func__, adc_mic_average);
		/*
		 * When adc value is negative, it is invalid, set a useless
		 * value to it, like 0 in headset type identification.
		 */
		adc_value_err = true;
		adc_mic_average = 0;
	}

	adc_mic_ideal = sprd_adc_to_ideal(adc_mic_average,
						pdata->coefficient);
	pr_info("%s, adc_mic_average %d, adc_mic_ideal %d, V_ideal %s %d mV\n",
		__func__, adc_mic_average, adc_mic_ideal,
		(adc_mic_average >= 4095 || adc_value_err) ? "outrange!" : "",
		adc_mic_ideal * 1250 / 4095);
	if (adc_mic_ideal < 0)
		adc_mic_ideal = 0;

	if (pdata->jack_type == JACK_TYPE_NO)
		headset_reg_write(ANA_HDT3, HEDET_V2AD_CH_SEL(0x4),
			HEDET_V2AD_CH_SEL(0xf));
	else if (pdata->jack_type == JACK_TYPE_NC)
		headset_reg_write(ANA_HDT3, HEDET_V2AD_CH_SEL(0x5),
			HEDET_V2AD_CH_SEL(0xf));

	adc_left_average = sprd_get_adc_value(adc_chan);
	if (adc_left_average < 0) {
		pr_err("%s adc error, adc_left_average %d\n",
			__func__, adc_left_average);
		/*
		 * When adc value is negative, it is invalid, set a useless
		 * value to it, like 0 in headset type identification.
		 */
		adc_value_err = true;
		adc_left_average = 0;
	}

	adc_left_ideal = sprd_adc_to_ideal(adc_left_average,
			pdata->coefficient);
	pr_info("%s adc_left_average %d, adc_left_ideal %d, V_ideal %s %d mV\n",
		__func__, adc_left_average, adc_left_ideal,
		(adc_left_average >= 4095 || adc_value_err) ? "outrange!" : "",
		adc_left_ideal * 1250 / 4095);

	pr_info("%s sprd_half_adc_gnd %d, sprd_adc_gnd %d,sprd_one_half_adc_gnd %d, threshold_3pole %d\n",
		__func__, pdata->sprd_half_adc_gnd,
		pdata->sprd_adc_gnd, pdata->sprd_one_half_adc_gnd,
		pdata->threshold_3pole);

	if (adc_left_ideal > pdata->sprd_adc_gnd &&
		ABS(adc_mic_ideal - adc_left_ideal) < pdata->sprd_adc_gnd) {
		sprd_headset_ldetl_ref_sel(LDETL_REF_SEL_20mV);
		return HEADSET_4POLE_NOT_NORMAL;
	} else if (adc_left_ideal > pdata->sprd_adc_gnd &&
		ABS(adc_mic_ideal - adc_left_ideal) >= pdata->sprd_adc_gnd)
		return HEADSET_TYPE_ERR;
	else if (adc_left_ideal < pdata->sprd_adc_gnd &&
		adc_mic_ideal < pdata->threshold_3pole) {
		sprd_headset_ldetl_ref_sel(LDETL_REF_SEL_100mV);
		return HEADSET_NO_MIC;
	} else if (adc_left_ideal < pdata->sprd_adc_gnd &&
		adc_mic_ideal >= pdata->threshold_3pole) {
		val = sprd_headset_part_is_inserted(HDST_INSERT_MDET);
		pr_debug("%s val %d\n", __func__, val);
		if (val != 0 && adc_left_ideal < pdata->sprd_half_adc_gnd) {
			sprd_headset_ldetl_ref_sel(LDETL_REF_SEL_20mV);
			return HEADSET_4POLE_NORMAL;
		} else if (val != 0 &&
			adc_left_ideal >= pdata->sprd_half_adc_gnd) {
			/* selfie stick */
			sprd_headset_ldetl_ref_sel(LDETL_REF_SEL_100mV);
			return HEADSET_4POLE_NORMAL;
		} else if (val == 0 &&
			adc_left_ideal < pdata->sprd_half_adc_gnd) {
			/*
			 * 4 pole normal type with mic floating
			 * can be treated as 3 pole headphone
			 */
			headset_type = headset_type_detect_through_mdet();
			if (headset_type == HEADSET_4POLE_NORMAL)
				sprd_headset_ldetl_ref_sel(LDETL_REF_SEL_20mV);
			else
				sprd_headset_ldetl_ref_sel(LDETL_REF_SEL_100mV);
			return headset_type;
		} else if (val == 0 &&
			adc_left_ideal >= pdata->sprd_half_adc_gnd) {
			/*
			 * 4 pole normal which is not totally inserted.
			 * 4 pole normal for selfie stick which is not
			 * totally inserted or it is 4 pole floating.
			 */
			headset_type = headset_type_detect_through_mdet();
			sprd_headset_ldetl_ref_sel(LDETL_REF_SEL_100mV);
			return headset_type;
		}
	}

	return HEADSET_TYPE_ERR;
}

static void headset_button_release_verify(void)
{
	struct sprd_headset *hdst = sprd_hdst;

	if (hdst->btn_state_last == 1) {
		sprd_headset_jack_report(hdst, &hdst->btn_jack,
			0, hdst->btns_pressed);
		hdst->btn_state_last = 0;
		pr_info("%s headset button released by force! current button: %#x\n",
			__func__, hdst->btns_pressed);
		hdst->btns_pressed &= ~SPRD_BUTTON_JACK_MASK;
		sprd_set_eic_trig_level(HDST_BDET_EIC, true);
	}
}

static void headset_removed_verify(void)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = hdst ? &hdst->pdata : NULL;
	int plug_state_current = 0;
	bool trig_level, hw_insert_status;

	trig_level = sprd_get_eic_trig_level(HDST_INSERT_ALL_EIC);
	hw_insert_status =
		sprd_headset_part_is_inserted(HDST_INSERT_ALL);
	if (trig_level && hw_insert_status)
		plug_state_current = 1;
	/* Hardware is plugging in, but software was plugged, clean SW status */
	if ((plug_state_current && 1 == hdst->plug_state_last) ||
		(trig_level && hdst->report == 1)) {
		pr_warn("%s headset removed but not reported, do %s plug out\n",
			__func__, hdst->headphone ? "headphone" : "headset");
		sprd_headset_eic_enable(HDST_MDET_EIC, 0);
		sprd_headset_eic_enable(HDST_BDET_EIC, 0);
		headset_button_release_verify();
		hdst->hdst_type_status &= ~SPRD_HEADSET_JACK_MASK;
		sprd_headset_jack_report(hdst, &hdst->hdst_jack,
			0, SPRD_HEADSET_JACK_MASK);
		sprd_enable_hmicbias_polling(false, true);
		sprd_headset_power_set(&hdst->power_manager, "HEADMICBIAS",
			false);
		hdst->plug_state_last = 0;
		hdst->report = 0;
		hdst->re_detect = false;

		if (pdata->jack_type == JACK_TYPE_NO)
			sprd_set_eic_trig_level(HDST_LDETL_EIC, 0);
		sprd_headset_ldetl_ref_sel(3);
	}
}

static enum snd_jack_types sprd_adc_to_button(int adc_mic)
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
			j_type = sprd_jack_type_get(i);
			break;
		}
	}
	pr_info("%s key %d\n", __func__, i);

	return j_type;
}

static void headset_mic_work_func(struct work_struct *work)
{
	struct sprd_headset *hdst = sprd_hdst;
	int val, mdet_insert_status;

	mdet_insert_status =
		sprd_headset_part_is_inserted(HDST_INSERT_MDET);

	/* 0==mic irq not triggered, 1==mic triggered */
	if (mdet_insert_status == 1)
		hdst->mic_irq_trigged = 1;
	else
		goto out;

	sprd_headset_eic_enable(HDST_MDET_EIC, 0);
	sprd_headset_eic_clear(HDST_MDET_EIC);
	sprd_intc_force_clear(0);

	headset_reg_read(ANA_STS0, &val);
	pr_info("%s: mic_irq_trigged %d, ANA_STS0 = 0x%x\n",
		__func__, hdst->mic_irq_trigged, val);
	return;

out:
	pr_err("%s invalid, mdet_insert_status %d\n",
		__func__, mdet_insert_status);
	/* enable and trig MDET irq again */
	sprd_headset_eic_clear(HDST_MDET_EIC);
	sprd_headset_eic_enable(HDST_MDET_EIC, 1);
	sprd_intc_force_clear(0);
	sprd_headset_eic_trig(HDST_MDET_EIC);
}

static void headset_button_work_func(struct work_struct *work)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
	int button_bit_value_current, adc_mic_average = 0, btn_irq_trig_level;
	unsigned int val;
	struct iio_channel *chan;

	if (!hdst || !pdata) {
		pr_err("%s: sprd_hdst(%p) or pdata(%p) is NULL!\n",
			__func__, sprd_hdst, pdata);
		return;
	}

	chan = hdst->adc_chan;

	down(&hdst->sem);
	val = sprd_get_eic_mis_status(HDST_BDET_EIC);
	if (val == 0) {
		pr_info("%s fatal error, irq 15 invalid, INT8(220.MIS) %x\n",
			__func__, val);
		goto out;
	}

	btn_irq_trig_level = sprd_get_eic_trig_level(15);
	hdst->bdet_val_last =
		sprd_headset_part_is_inserted(HDST_INSERT_BDET);

	if (sprd_headset_part_is_inserted(15) == 0) {
		pr_err("%s: button is invalid! STS0 0x%x\n",
			__func__, sprd_read_reg_value(ANA_STS0));
		goto out;
	}
	button_bit_value_current =
		sprd_headset_part_is_inserted(HDST_INSERT_BDET);
	if (button_bit_value_current != hdst->bdet_val_last) {
		pr_err("%s software debounce error\n", __func__);
		goto out;
	}

	if (val != sprd_get_eic_mis_status(HDST_BDET_EIC)) {
		pr_err("%s check debounce failed\n", __func__);
		goto out;
	}

	pr_info("%s polling: DCL1(0100) %x, CLK0(0068) %x, HID0(0144) %x\n",
		__func__, sprd_read_reg_value(ANA_DCL1),
		sprd_read_reg_value(ANA_CLK0),
		sprd_read_reg_value(ANA_HID0));

	sprd_headset_eic_enable(HDST_BDET_EIC, 0);
	if (btn_irq_trig_level == 1) {
		sprd_set_eic_trig_level(15, 0);
		sprd_enable_hmicbias_polling(false, false);
		mutex_lock(&hdst->btn_detecting_lock);
		hdst->btn_detecting = true;
		mutex_unlock(&hdst->btn_detecting_lock);
	} else if (btn_irq_trig_level == 0) {
		sprd_set_eic_trig_level(15, 1);
		mutex_lock(&hdst->btn_detecting_lock);
		hdst->btn_detecting = false;
		mutex_unlock(&hdst->btn_detecting_lock);

		mutex_lock(&hdst->audio_on_lock);
		if (!hdst->audio_on)
			sprd_enable_hmicbias_polling(true, false);
		mutex_unlock(&hdst->audio_on_lock);
	}

	if (btn_irq_trig_level == 1) {/* pressed! */
		if (pdata->nbuttons > 0) {
			adc_mic_average = sprd_button_ideal_adc(hdst);
			hdst->btns_pressed |=
				sprd_adc_to_button(adc_mic_average);
		}

		if (hdst->btn_state_last == 0) {
			sprd_headset_jack_report(hdst, &hdst->btn_jack,
				hdst->btns_pressed, hdst->btns_pressed);
			hdst->btn_state_last = 1;
			pr_info("Reporting headset button press. button: 0x%#x\n",
				hdst->btns_pressed);
		} else {
			pr_err("Headset button has been reported already. button: 0x%#x\n",
				hdst->btns_pressed);
		}
	} else if (btn_irq_trig_level == 0) {/* released */
		if (hdst->btn_state_last == 1) {
			sprd_headset_jack_report(hdst, &hdst->btn_jack,
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
	sprd_headset_eic_clear(15);
	sprd_intc_force_clear(0);
	sprd_headset_eic_enable(15, 1);
	sprd_headset_eic_trig(15);
	/* wake_unlock(&hdst->btn_wakelock); */
	up(&hdst->sem);
}

static void sprd_process_4pole_type(enum sprd_headset_type headset_type)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);

	if (!pdata) {
		pr_err("%s: pdata is NULL!\n", __func__);
		return;
	}

	if ((headset_type == HEADSET_4POLE_NOT_NORMAL)
	    && (pdata->gpio_switch == 0)) {
		sprd_headset_eic_enable(15, 0);
		sprd_headset_power_set(&hdst->power_manager, "HEADMICBIAS",
			false);
		hdst->hdst_type_status = SND_JACK_HEADPHONE;
		pr_info("HEADSET_4POLE_NOT_NORMAL is not supported %s\n",
			"by your hardware! so disable the button irq!");
	} else {
		sprd_set_eic_trig_level(15, 1);
		headset_reg_clr_bits(ANA_HDT2, HEDET_LDETL_FLT_EN);
		/* set threshold and HEDET_BDET_EN */
		sprd_button_irq_threshold(1);
		sprd_headset_eic_enable(15, 1);
		sprd_headset_eic_trig(15);
		hdst->hdst_type_status = SND_JACK_HEADSET;
		if (!hdst->audio_on)
			sprd_enable_hmicbias_polling(true, false);
	}

	if (hdst->report == 0) {
		sprd_headset_jack_report(hdst, &hdst->hdst_jack,
			hdst->hdst_type_status, SND_JACK_HEADSET);
		sprd_headset_eic_enable(HDST_BDET_EIC, true);
		sprd_headset_eic_trig(HDST_BDET_EIC);
	}
	if (hdst->re_detect) {
		pr_err("%s report for 4p re_detect\n", __func__);
		sprd_headset_jack_report(hdst, &hdst->hdst_jack,
			0, SPRD_HEADSET_JACK_MASK);

		sprd_headset_jack_report(hdst, &hdst->hdst_jack,
			hdst->hdst_type_status, SND_JACK_HEADSET);
	}
	hdst->report = 1;
	pr_info("%s headset plug in\n", __func__);
}

static void headset_reinit_all_eic(void)
{
	sprd_headset_clear_all_eic();
	sprd_set_all_eic_trig_level(true);
	sprd_headset_all_eic_enable(false);
	sprd_intc_force_clear(0);
}

static void headset_detect_all_work_func(struct work_struct *work)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = (hdst ? &hdst->pdata : NULL);
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

	val = sprd_get_eic_mis_status(10);
	if (val == 0) {
		pr_info("%s fatal error, irq 10 invalid, INT8(220.MIS) %x\n",
			__func__, val);
		sprd_headset_eic_clear(10);
		sprd_intc_force_clear(1);
		sprd_intc_force_clear(0);
		sprd_headset_eic_trig(10);
		goto out;
	}
	insert_all_data_last = sprd_headset_eic_get_data(10);
	if (hdst->plug_state_last == 0)
		sprd_msleep(40);
	else
		sprd_msleep(20);

	insert_all_data_current = sprd_headset_eic_get_data(10);
	if (insert_all_data_last != insert_all_data_current) {
		pr_info("%s check debounce failed\n", __func__);
		sprd_headset_eic_clear(10);
		sprd_intc_force_clear(1);
		sprd_intc_force_clear(0);
		sprd_headset_eic_trig(10);
		goto out;
	}

	pr_debug(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34);
	pr_info("%s plug_state_last %d\n", __func__, hdst->plug_state_last);

	if (hdst->plug_state_last == 0) {
		sci_adi_set(ANA_REG_GLB_ARM_MODULE_EN, BIT_ANA_AUD_EN);
		if (pdata->jack_type == JACK_TYPE_NO) {
			sprd_hmicbias_hw_control_enable(true);
			headset_reg_set_bits(ANA_HDT2, HEDET_LDETL_FLT_EN);
			pr_debug("filter detect_l HDT2 0x%04x\n",
				sprd_read_reg_value(ANA_HDT2));
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
		sprd_headset_power_set(&hdst->power_manager, "BIAS", true);
		pr_debug("%s PMU0(0000) %x\n", __func__,
			sprd_read_reg_value(ANA_PMU0));
		/* check if this need improve or need, I am not sure */
		sprd_headset_power_set(&hdst->power_manager, "HEADMICBIAS",
			true);
		sprd_msleep(10);
	}

	pr_info("%s insert_all_val_last = %d, plug_state_last = %d\n",
			__func__, hdst->insert_all_val_last,
			hdst->plug_state_last);

	trig_level = sprd_get_eic_trig_level(10);
	insert_status =
		sprd_headset_part_is_inserted(HDST_INSERT_ALL);
	pr_info("%s trig_level %s, insert_status %s\n", __func__,
		trig_level ? "high" : "low", insert_status ? "high" : "low");
	if (trig_level && insert_status) {
		pr_info("%s headphone is plugin???\n", __func__);
		plug_state_current = 1;
	} else if (!trig_level && !insert_status) {
		pr_info("%s headphone is plugout???\n", __func__);
		plug_state_current = 0;
	} else {
		pr_err("%s fatal error, re-trig insert_all irq again\n",
			__func__);
		sprd_headset_eic_clear(10);
		sprd_intc_force_clear(1);
		sprd_intc_force_clear(0);
		sprd_headset_eic_trig(10);
		goto out;
	}

	if (hdst->re_detect == true)
		detect_value =	insert_status;

	/*
	 * 4pole detect as 3 pole if polling is enabled,
	 * and need to disable polling after plugout
	 */
	sprd_enable_hmicbias_polling(false, true);
	pr_info("%s plug_state_last %d, hdst_hw_status %s\n",
			__func__, hdst->plug_state_last,
			eic_hw_state[hdst->hdst_hw_status]);

	if ((1 == plug_state_current && 0 == hdst->plug_state_last) ||
	  (hdst->re_detect == true && detect_value == true)) {
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
			pr_info("%s headset_type %d (HEADSET_4POLE_NORMAL)\n",
				__func__, headset_type);
			if (pdata->eu_us_switch != 0)
				gpio_direction_output(pdata->eu_us_switch, 0);
			break;
		case HEADSET_4POLE_NOT_NORMAL:
			pr_info("%s headset_type %d (HEADSET_4POLE_NOT_NORMAL)\n",
				__func__, headset_type);
			if (pdata->eu_us_switch != 0)
				gpio_direction_output(pdata->eu_us_switch, 1);
			/* Repeated detection 5 times when 3P is detected */
		case HEADSET_NO_MIC:
			pr_info("%s headset_type %d (HEADSET_NO_MIC)\n",
				__func__, headset_type);
				/*
				 * following is ok, when in early test,
				 * adc can't work, no mic
				 * headphone is detected every time,
				 * so I remove this for test
				 */

				/* if (times_1 < 5) {
				 *	queue_delayed_work(hdst->det_all_work_q,
				 *	&hdst->det_all_work,
				 *	msecs_to_jiffies(1000));
				 *	times_1++;
				 *	hdst->re_detect = true;
				 * } else {
				 *	hdst->re_detect = false;
				 * }
				 */
			if (pdata->eu_us_switch != 0)
				gpio_direction_output(pdata->eu_us_switch, 0);
			break;
		case HEADSET_APPLE:
			pr_info("%s headset_type %d (HEADSET_APPLE)\n",
				__func__, headset_type);
			pr_info("we have not yet implemented this in the code\n");
			break;
		default:
			pr_info("%s headset_type %d (HEADSET_UNKNOWN)\n",
				__func__, headset_type);
			break;
		}

		/*
		 * invert trig level after type detect over, because it
		 * may need redetect at that time.
		 */
		sprd_set_eic_trig_level(HDST_INSERT_ALL_EIC, false);
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
			sprd_button_irq_threshold(0);
			sprd_headset_eic_enable(15, 0);

			hdst->hdst_type_status = SND_JACK_HEADPHONE;
			if (hdst->report == 0) {
				pr_debug("report for 3p\n");
				sprd_headset_jack_report(hdst, &hdst->hdst_jack,
					hdst->hdst_type_status,
					SND_JACK_HEADPHONE);
			}

			hdst->report = 1;
			if (hdst->re_detect == false)
				sprd_headset_power_set(&hdst->power_manager,
				"HEADMICBIAS", false);
			pr_info("%s headphone plug in\n", __func__);
		} else
			sprd_process_4pole_type(headset_type);

		/*something after headset type detection over*/
		if (pdata->jack_type == JACK_TYPE_NC) {
			headset_reg_write(ANA_HDT3, HEDET_V2AD_CH_SEL(0x4),
				HEDET_V2AD_CH_SEL(0xF));
			sprd_hmicbias_hw_control_enable(false);
			headset_reg_clr_bits(ANA_HDT1, HEDET_LDET_CMP_SEL);
			headset_reg_set_bits(ANA_HDT0, HEDET_JACK_TYPE);
			usleep_range(50, 60); /* Wait for 50us */
			sprd_hmicbias_hw_control_enable(true);
		}

		if (pdata->do_fm_mute)
			vbc_close_fm_dggain(false);
		pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34);

		sprd_headset_eic_enable(10, 0);
		sprd_headset_eic_clear(10);
		sprd_set_eic_trig_level(10, 0);
		sprd_intc_force_clear(1);
		sprd_intc_force_clear(0);
		sprd_headset_eic_enable(10, 1);
		pr_info("STS0(184) %x\n", sprd_read_reg_value(ANA_STS0));
		sprd_headset_eic_trig(10);
		pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34);
	} else if (0 == plug_state_current && 1 == hdst->plug_state_last) {
		times = 0;
		times_1 = 0;
		pr_info("%s micbias power off for plug out, times %d\n",
			__func__, times);

		sprd_headset_eic_enable(10, 0);
		sprd_headset_eic_enable(11, 0);
		sprd_headset_eic_enable(15, 0);
		sprd_set_eic_trig_level(10, 1);

		headset_button_release_verify();

		hdst->hdst_type_status &= ~SPRD_HEADSET_JACK_MASK;
		sprd_headset_jack_report(hdst, &hdst->hdst_jack,
			0, SPRD_HEADSET_JACK_MASK);
		/* must be called before set hdst->plug_state_last = 0 */
		sprd_enable_hmicbias_polling(false, true);
		sprd_headset_power_set(&hdst->power_manager, "HEADMICBIAS",
			false);
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
		sprd_headset_eic_clear(10);
		sprd_set_all_eic_trig_level(true);
		sprd_headset_all_eic_enable(false);
		sprd_intc_force_clear(0);

		sprd_headset_clear_all_eic();
		if (pdata->jack_type == JACK_TYPE_NO) {
			sprd_set_eic_trig_level(12, 0);
			sprd_headset_eic_enable(12, 1);
			sprd_headset_eic_trig(12);
		} else if (pdata->jack_type == JACK_TYPE_NC) {
			sprd_headset_eic_enable(10, 1);
			sprd_headset_eic_trig(10);
		}
		sprd_headset_ldetl_ref_sel(3);

		pr_info("%s ANA_HDT2 0x%04x\n", __func__,
			sprd_read_reg_value(ANA_HDT2));

		msleep(20);
	} else {
		times = 0;
		times_1 = 0;
		hdst->re_detect = false;
		hdst->report = 0;
		pr_info("%s times %d irq_detect must be enabled anyway! micbias power off\n",
			__func__, times);
		/*
		 * When status is wrong, checking Whether the hardware state
		 * is corresponds to the software. If status conflict, software
		 * status should be clean.
		 */
		headset_removed_verify();
		sprd_headset_power_set(&hdst->power_manager, "HEADMICBIAS",
			false);
		sprd_set_all_eic_trig_level(true);
		sprd_headset_all_eic_enable(false);
		sprd_headset_clear_all_eic();
		sprd_intc_force_clear(0);
		sprd_headset_eic_plugin_enable();
		goto out;
	}
out:
	headset_reg_clr_bits(ANA_HDT2, HEDET_LDETL_FLT_EN);
	pr_info("%s HDT2(00D8) 0x%x,plug_state_last %d\n", __func__,
		sprd_read_reg_value(ANA_HDT2), hdst->plug_state_last);
	/*
	 * I think if in hdst->re_detect == true and times < 10,
	 * we dont need to run following
	 */
	if (hdst->plug_state_last == 0) {
		sprd_headset_scale_set(0);
		sprd_headset_power_set(&hdst->power_manager,
			"HEADMICBIAS", false);
		/* doc don't refer */
		sprd_headset_power_set(&hdst->power_manager, "BIAS", false);
		pr_debug("%s PMU0(0000) %x\n", __func__,
			sprd_read_reg_value(ANA_PMU0));
		/*
		 * if it is type error, run
		 *  headset_reg_clr_bits(ANA_PMU1, HMIC_BIAS_EN) in here
		 */
		sprd_button_irq_threshold(0);
		hdst->mic_irq_trigged = 0;
	}
	if (times >= 10) {
		times = 0;
		sprd_headset_all_eic_enable(false);
		sprd_headset_clear_all_eic();
		sprd_set_all_eic_trig_level(true);
		sprd_intc_force_clear(0);
		sprd_headset_eic_plugin_enable();
	}
	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34);

	pr_info("%s out\n", __func__);
	up(&hdst->sem);
}

static void headset_ldetl_work_func(struct work_struct *work)
{
	struct sprd_headset *hdst = sprd_hdst;
	unsigned int val, ldetl_data_last, ldetl_data_current;
	bool insert_status;
	int check_times = 100;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return;
	}

	down(&hdst->sem);

	pr_info("%s enter\n", __func__);
	val = sprd_get_eic_mis_status(12);
	if (val == 0) {
		pr_err("%s fatal error, irq 12 invalid, INT8(220.MIS) %x\n",
			__func__, val);
		sprd_headset_eic_clear(12);
		sprd_intc_force_clear(1);
		sprd_intc_force_clear(0);
		sprd_headset_eic_trig(12);
		goto out;
	}
	ldetl_data_last = sprd_headset_eic_get_data(12);
	sprd_msleep(20);
	ldetl_data_current = sprd_headset_eic_get_data(12);
	if (ldetl_data_last != ldetl_data_current) {
		pr_err("%s check debounce failed\n", __func__);
		sprd_headset_eic_clear(12);
		sprd_intc_force_clear(1);
		sprd_intc_force_clear(0);
		sprd_headset_eic_trig(12);
		goto out;
	}
	hdst->ldetl_trig_val_last = sprd_get_eic_trig_level(12);
	insert_status = sprd_headset_part_is_inserted(HDST_INSERT_LDETL);
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
		pr_err("%s fatal error, re-trig ldetl irq again\n", __func__);
		sprd_headset_eic_clear(12);
		sprd_intc_force_clear(1);
		sprd_intc_force_clear(0);
		sprd_headset_eic_trig(12);
		goto out;
	}
	sprd_hmicbias_hw_control_enable(false);
	sprd_headset_eic_enable(12, 0);
	sprd_set_eic_trig_level(12, 0);
	sprd_intc_force_clear(1);
	pr_info("%s %d ldetl_trig_val_last %d, plug_state_last %d, ldetl_plug_in %d\n",
		__func__, __LINE__, hdst->ldetl_trig_val_last,
		hdst->plug_state_last, hdst->ldetl_plug_in);

	if ((0 == hdst->ldetl_trig_val_last && 0 == hdst->plug_state_last) ||
		(hdst->ldetl_plug_in == 0)) {
		sprd_hmicbias_hw_control_enable(true);
		sprd_set_eic_trig_level(12, 1);
		sprd_headset_eic_clear(12);
		sprd_intc_force_clear(0);
		sprd_headset_eic_enable(12, 1);
		sprd_headset_eic_trig(12);
		sprd_msleep(20);
	} else if ((hdst->ldetl_trig_val_last == 1) ||
	(hdst->ldetl_plug_in == 1)) {
		hdst->insert_all_irq_trigged = 0;
		sprd_hmicbias_hw_control_enable(false);
		sprd_headset_eic_enable(12, 0);
		sprd_headset_eic_clear(12);
		sprd_intc_force_clear(1);
		sprd_set_eic_trig_level(12, 0);
		usleep_range(3000, 3500);
		sprd_intc_force_clear(0);
		sprd_set_eic_trig_level(10, 1);
		sprd_headset_eic_enable(10, 1);
		sprd_headset_eic_trig(10);
		while (check_times > 0) {
			if (hdst->insert_all_irq_trigged == 1)
				break;
			check_times--;
			pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34);
			sprd_msleep(240);
		}
		pr_info("%s check_times %d\n", __func__, check_times);
		if (check_times == 0) {
			pr_err("%s failed to wait insert_all irq, trig ldetl again\n",
				__func__);
			sprd_headset_eic_clear(12);
			sprd_intc_force_clear(1);
			sprd_intc_force_clear(0);
			sprd_headset_eic_enable(12, 1);
			sprd_set_eic_trig_level(12, 1);
			sprd_headset_eic_trig(12);
		}
		hdst->insert_all_irq_trigged = 0;
	}
out:
	pr_info("%s out\n", __func__);
	up(&hdst->sem);
}

static void sprd_dump_reg_work(struct work_struct *work)
{

	int adc_mic, gpio_insert_all, ana_sts0, ana_pmu0, ana_pmu1,
		ana_hdt0, ana_hdt1, ana_hdt2, ana_dcl0, ana_cdc2,
		ana_cdc3, arm_module_en, arm_clk_en;
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata;

	pdata = (hdst ? &hdst->pdata : NULL);
	if (!pdata) {
		pr_err("%s: pdata is NULL!\n", __func__);
		return;
	}

	adc_mic = sprd_headset_adc_get(hdst->adc_chan);
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
	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34);

	if (hdst->debug_level >= 2)
		queue_delayed_work(hdst->reg_dump_work_q,
			&hdst->reg_dump_work, msecs_to_jiffies(500));
}

/*
 * Check whether exsit software status of headset pluged in and button,
 * pressed. If exsit, clean them.
 */
static void headset_force_remove_sw_plugin_status(void)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct sprd_headset_platform_data *pdata = &hdst->pdata;

	if (hdst->plug_state_last == 1) {
		pr_info("%s headset removed but not reported, do %s plug out\n",
			__func__, hdst->headphone ? "headphone" : "headset");
		sprd_headset_eic_enable(HDST_MDET_EIC, false);
		sprd_headset_eic_enable(HDST_BDET_EIC, false);
		sprd_headset_power_set(&hdst->power_manager, "HEADMICBIAS",
			false);
		headset_button_release_verify();
		hdst->hdst_type_status &= ~SPRD_HEADSET_JACK_MASK;
		sprd_headset_jack_report(hdst, &hdst->hdst_jack,
			0, SPRD_HEADSET_JACK_MASK);
		sprd_enable_hmicbias_polling(false, true);
		hdst->plug_state_last = 0;
		hdst->report = 0;
		hdst->re_detect = false;

		if (pdata->jack_type == JACK_TYPE_NO)
			sprd_set_eic_trig_level(HDST_LDETL_EIC, false);
		sprd_headset_ldetl_ref_sel(3);
		headset_reinit_all_eic();
	}

	sprd_ldetl_filter_enable(false);

	if (hdst->plug_state_last == 0) {
		sprd_headset_scale_set(0);
		sprd_headset_power_set(&hdst->power_manager, "HEADMICBIAS",
			false);
		if (pdata->jack_type == JACK_TYPE_NO)
			sprd_headset_power_set(&hdst->power_manager, "BIAS",
				false);
		sprd_button_irq_threshold(0);
	}
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

	hdst->gpio_detect_int_all_last =
		gpio_get_value(pdata->gpios[HDST_GPIO_AUD_DET_INT_ALL]);

	headset_reg_read(ANA_STS0, &val);

	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34);

	if ((BIT(14) & sprd_read_reg_value(ANA_INT34)) == 0) {
		pr_err("%s: error, intc is not active\n", __func__);
		sprd_headset_eic_plugin_enable();
		irq_set_irq_type(hdst->irq_detect_int_all, IRQF_TRIGGER_HIGH);
		return IRQ_HANDLED;
	}

	if ((val & 0xFC00) == 0) {
		pr_err("%s no headset alert signal at all! headphone is plugout?\n",
			__func__);
		pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34);
	}

	val = sprd_read_reg_value(ANA_INT8);
	if ((val & 0xFC00) == 0) {
		pr_err("%s: no active interrupt at all!\n", __func__);
		pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34);

		sprd_intc_force_clear(1);
		sprd_intc_force_clear(0);
		headset_force_remove_sw_plugin_status();
		irq_set_irq_type(hdst->irq_detect_int_all, IRQF_TRIGGER_HIGH);
		sprd_headset_eic_plugin_enable();
		pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34);
		return IRQ_HANDLED;
	}
	/* I think this need place at the begain of this func to clear intc */
	sprd_intc_force_clear(1);
	irq_set_irq_type(hdst->irq_detect_int_all, IRQF_TRIGGER_HIGH);

	if (val & BIT(10)) {/* insert_all */
		pr_info("%s in insert_all\n", __func__);
		/* I am not sure the new func is right or not */
		__pm_wakeup_event(&hdst->det_all_wakelock,
			msecs_to_jiffies(2000));
		/* I think this is useless, only used to check debounce */
		hdst->insert_all_val_last = sprd_get_eic_mis_status(10);
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
		hdst->mdet_val_last = sprd_get_eic_mis_status(11);

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
		__pm_wakeup_event(&hdst->btn_wakelock, msecs_to_jiffies(2000));
		ret = cancel_delayed_work(&hdst->btn_work);
		queue_delayed_work(hdst->btn_work_q,
			&hdst->btn_work, msecs_to_jiffies(0));
		pr_info("%s bdet irq active, ret %d\n", __func__, ret);
	}

out:
	pr_debug(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34);

	return IRQ_HANDLED;
}

/* ================= create sys fs for debug =================== */
 /* Used for getting headset type in sysfs. */
static ssize_t sprd_headset_state_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buff)
{
	struct sprd_headset *hdst = sprd_hdst;
	int type;

	switch (hdst->hdst_type_status) {
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

	pr_debug("%s status: %#x, headset_state %d\n",
		__func__, hdst->hdst_type_status, type);

	return sprintf(buff, "%d\n", type);
}

static ssize_t sprd_headset_state_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buff, size_t len)
{
	return len;
}

static ssize_t sprd_headset_debug_level_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buff)
{
	struct sprd_headset *hdst = sprd_hdst;

	if (!hdst) {
		pr_err("%s: sprd_hdset is NULL!\n", __func__);
		return -1;
	}

	pr_info("%s debug_level %d\n", __func__, hdst->debug_level);

	return sprintf(buff, "%d\n", hdst->debug_level);
}

static ssize_t sprd_headset_debug_level_store(struct kobject *kobj,
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
	pr_info("%s debug_level %d\n", __func__, hdst->debug_level);
	if (hdst->debug_level >= 2)
		queue_delayed_work(hdst->reg_dump_work_q,
			&hdst->reg_dump_work, msecs_to_jiffies(500));

	return len;
}

static int sprd_headset_debug_sysfs_init(void)
{
	int ret, i;
	static struct kobject *headset_debug_kobj;
	static struct kobj_attribute headset_debug_attr[] = {
		__ATTR(debug_level, 0644,
		sprd_headset_debug_level_show,
		sprd_headset_debug_level_store),
		__ATTR(state, 0644,
		sprd_headset_state_show,
		sprd_headset_state_store),
	};

	headset_debug_kobj = kobject_create_and_add("headset", kernel_kobj);
	if (headset_debug_kobj == NULL) {
		ret = -ENOMEM;
		pr_err("%s register sysfs failed. ret %d\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < sizeof(headset_debug_attr) /
	     sizeof(headset_debug_attr[0]); i++) {
		ret = sysfs_create_file(headset_debug_kobj,
					&headset_debug_attr[i].attr);
		if (ret) {
			pr_err("%s create sysfs '%s' failed. ret %d\n",
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

static int sprd_get_adc_cal_from_efuse(struct platform_device *pdev);

int sprd_headset_soc_probe(struct snd_soc_codec *codec)
{
	struct sprd_headset *hdst = sprd_hdst;
	struct platform_device *pdev;
	struct sprd_headset_platform_data *pdata;
	struct device *dev = codec->dev; /* digiatal part device */
	struct gpio_desc *gpio_desc_test;
	struct snd_soc_card *card = codec->component.card;
	unsigned int adie_chip_id;
	int ret, i;

	if (!hdst) {
		pr_err("%s hdst NULL\n", __func__);
		return -EINVAL;
	}
	pdev = hdst->pdev;
	pdata = &hdst->pdata;
	if (!pdev || !pdata) {
		pr_err("%s pdev %p, pdata %p\n", __func__, pdev, pdata);
		return -EINVAL;
	}
	if (!hdst->adc_chan) {
		pr_err("%s adc_chan %p\n", __func__, hdst->adc_chan);
		return -EINVAL;
	}

	hdst->codec = codec;
	adie_chip_id = sci_get_ana_chip_id() >> 16;
	pr_info("%s adie_chip_id 0x%x\n", __func__, adie_chip_id & 0xffff);
	ret = sprd_headset_power_init(&hdst->power_manager, pdev);
	if (ret) {
		pr_err("%s power regulator init failed\n", __func__);
		return ret;
	}

	ret = sprd_detect_reg_init();
	if (ret) {
		pr_err("%s headset detect reg init failed\n", __func__);
		return ret;
	}

	/* 0 normal open(Tie High), 1 normal close(Tie low) */
	if (pdata->jack_type == JACK_TYPE_NO)
		headset_reg_clr_bits(ANA_HDT0, HEDET_JACK_TYPE);
	else if (pdata->jack_type == JACK_TYPE_NO)
		headset_reg_set_bits(ANA_HDT0, HEDET_JACK_TYPE);

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
			sprd_jack_type_get(i), buttons->code);
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
	hdst->report = 0;
	hdst->det_err_cnt = 0;
	hdst->hdst_hw_status = HW_LDETL_PLUG_OUT;
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

	INIT_DELAYED_WORK(&hdst->reg_dump_work, sprd_dump_reg_work);
	hdst->reg_dump_work_q =
		create_singlethread_workqueue("headset_reg_dump");
	if (hdst->reg_dump_work_q == NULL) {
		pr_err("create_singlethread_workqueue for headset_reg_dump failed!\n");
		goto failed_to_headset_reg_dump;
	}
	if (hdst->debug_level >= 2)
		queue_delayed_work(hdst->reg_dump_work_q,
			&hdst->reg_dump_work, msecs_to_jiffies(500));

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
	mutex_init(&hdst->audio_on_lock);
	mutex_init(&hdst->btn_detecting_lock);

	for (i = 0; i < HDST_GPIO_AUD_MAX; i++)
		gpio_set_debounce(pdata->gpios[i], pdata->dbnc_times[i] * 1000);

	sprd_headset_debug_sysfs_init();
	sprd_get_adc_cal_from_efuse(hdst->pdev);
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

	sprd_hmicbias_hw_control_enable(true);
	headset_reg_set_bits(ANA_HDT2, HEDET_MDET_EN);

	pr_debug("%s ANA_HDT1(00D4) %x, ANA_HDT2(00D8) %x\n",
		__func__, sprd_read_reg_value(ANA_HDT1),
		sprd_read_reg_value(ANA_HDT2));
	usleep_range(3000, 3500);
	sprd_headset_eic_plugin_enable();

	pr_info(LG, FC, S0, T5, T6, T7, T8, T11, T32, T34);

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
	struct platform_device *pdev = hdst->pdev;
	struct device *dev = &pdev->dev;
	u32 val;
	int index, ret, i;

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

	/* Parse for the gpio of EU/US jack type switch. */
	index = of_property_match_string(np, "gpio-names", "switch");
	if (index < 0) {
		pr_info("%s :no match found for switch gpio.\n", __func__);
		pdata->eu_us_switch = 0;
	} else {
		ret = of_get_gpio_flags(np, index, NULL);
		if (ret < 0) {
			pr_err("%s :get gpio for 'switch' failed!\n", __func__);
			return -ENXIO;
		}
		pdata->eu_us_switch = (u32)ret;
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
			pdata->gpios[type], name,
			pdata->dbnc_times[type]);
	}

	ret = of_property_read_u32(np, "sprd,3pole-adc-threshold",
		&pdata->threshold_3pole);
	if (ret) {
		pr_err("%s: fail to get 3pole-adc-threshold\n",
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

	ret = of_property_read_u32(np, "sprd,half-adc-gnd",
		&pdata->sprd_half_adc_gnd);
	if (ret) {
		pr_err("%s: fail to get half-adc-gnd\n",
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

	pdata->sprd_one_half_adc_gnd = pdata->sprd_adc_gnd +
					pdata->sprd_half_adc_gnd;
	pr_info("half_adc_gnd %u, one_half_adc_gnd %u, sprd_adc_gnd %u",
		pdata->sprd_half_adc_gnd, pdata->sprd_one_half_adc_gnd,
		pdata->sprd_adc_gnd);
	pr_info("threshold_3pole %u, sprd_half_adc_gnd %u, coefficient %u, irq_threshold_button %u",
		pdata->threshold_3pole, pdata->sprd_half_adc_gnd,
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
		pr_info("device tree data: adc_min %d adc_max %d code %d\n",
			buttons_data->adc_min,
			buttons_data->adc_max, buttons_data->code);
		buttons_data++;
	};

	/* Get the adc channels of headset. */
	hdst->adc_chan = iio_channel_get(dev, "headmic_in_little");
	if (IS_ERR(hdst->adc_chan)) {
		pr_err("%s failed to get headmic in adc channel!\n", __func__);
		return PTR_ERR(hdst->adc_chan);
	}

	return 0;
}
#endif

/* Note: @pdev is the platform_device of headset node in dts. */
int sprd_headset_probe(struct platform_device *pdev)
{
	struct sprd_headset *hdst;
	struct sprd_headset_platform_data *pdata;
	struct device *dev;
	int i, ret;

#ifndef CONFIG_OF
	pr_err("%s: Only OF configurations are supported yet!\n", __func__);
	return -EINVAL;
#endif
	if (!pdev) {
		pr_err("%s: platform device is NULL!\n", __func__);
		return -EINVAL;
	}
	dev = &pdev->dev;
	if (!dev) {
		pr_err("%s: dev of platform_device is NULL!\n", __func__);
		return -EINVAL;
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

	if (pdata->eu_us_switch != 0) {
		ret = devm_gpio_request(dev,
			pdata->eu_us_switch, "headset_eu_us_switch");
		if (ret < 0) {
			pr_err("%s failed to request GPIO_%d(headset_switch)\n",
				__func__, pdata->eu_us_switch);
			return ret;
		}
	} else
		pr_info("automatic EU/US type switch is unsupported\n");

	for (i = 0; gpio_map[i].name; i++) {
		const char *name = gpio_map[i].name;
		int type = gpio_map[i].type;

		ret = devm_gpio_request(dev, pdata->gpios[type], name);
		if (ret < 0) {
			pr_err("%s failed to request GPIO_%d(%s)\n",
				__func__, pdata->gpios[type], name);
			return ret;
		}
		pr_debug("%s name %s, gpio %d\n", __func__,
			name, pdata->gpios[type]);
	}

	sprd_hdst = hdst;

	pr_info("headset_detect_probe success\n");

	return 0;
}
EXPORT_SYMBOL(sprd_headset_probe);

static int sprd_adc_to_ideal(u32 adc_mic, u32 coefficient)
{
	s64 numerator, denominator, exp1, exp2, exp3, exp4;
	int adc_ideal, a, b, e1, e2;

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

	pr_debug("exp1 %lld, exp2 %lld, exp3 %lld, exp4 %lld\n",
		exp1, exp2, exp3, exp4);
	denominator = exp3 + exp4;
	numerator = coefficient * (exp1 + 1200) * exp2;
	pr_debug("denominator %lld, numerator %lld\n",
			denominator, numerator);
	adc_ideal = div64_s64(numerator, denominator);
	pr_debug("%s adc_mic %d, adc_ideal %d\n", __func__, adc_mic, adc_ideal);

	return adc_ideal;
}

static int sprd_headset_read_efuse(struct platform_device *pdev,
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

static int sprd_get_adc_cal_from_efuse(struct platform_device *pdev)
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

	ret = sprd_headset_read_efuse(pdev, "hp_adc_fir_calib", &data);
	if (ret)
		goto adc_cali_error;
	test[0] = data;
	ret = sprd_headset_read_efuse(pdev, "hp_adc_sec_calib", &data);
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
	if (adie_chip_id == CHIP_ID_2730) {
		adc_cal_headset.A = (delta[0] - 128) * 4 + 336;
		adc_cal_headset.B =  (delta[1] - 128) * 4 + 3357;
		adc_cal_headset.E1 = delta[2] * 2 + 2400;
		adc_cal_headset.E2 = delta[3] * 4 + 1500;
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
