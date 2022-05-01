/*
 * sound/soc/sprd/codec/sprd/sc2730/sprd-audio-power.c
 *
 * SPRD-AUDIO-POWER -- SpreadTrum intergrated audio power supply.
 *
 * Copyright (C) 2016 SpreadTrum Ltd.
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
#define pr_fmt(fmt) pr_sprd_fmt("POWER")""fmt

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>

#include "sprd-audio.h"
#include "sprd-audio-power.h"
#include "sprd-asoc-common.h"

#define SPRD_AUDIO_POWER_LDO_LOW(label, r_supply, d_id, \
e_reg, e_bit, p_enable, p_disable, s_ctrl, \
reg, mask, shift, table, table_size, d_on_delay, d_off_delay) \
static struct regulator_consumer_supply label[] = { \
	{ .supply =	#label },\
}; \
static struct regulator_init_data label##_data = { \
	.supply_regulator = r_supply, \
	.constraints = { \
		.name = "SRG_" #label, \
	}, \
	.num_consumer_supplies = ARRAY_SIZE(label), \
	.consumer_supplies = label, \
}

#define SPRD_AUDIO_POWER_LDO(label, r_supply, id, en_reg, en_bit, sleep_ctrl, \
		v_reg, v_mask, v_shift, v_table, on_delay, off_delay) \
SPRD_AUDIO_POWER_LDO_LOW(label, r_supply, id, en_reg, en_bit, 0, 0, \
		sleep_ctrl, v_reg, v_mask, v_shift, v_table, \
		ARRAY_SIZE(v_table), on_delay, off_delay)

#define SPRD_AUDIO_POWER_SIMPLE_LDO(label, r_supply, id, en_reg, en_bit, \
		sleep_ctrl, on_delay, off_delay) \
SPRD_AUDIO_POWER_LDO_LOW(label, r_supply, id, en_reg, en_bit, 0, 0, \
		sleep_ctrl, 0, 0, 0, 0, 0, on_delay, off_delay)

#define SPRD_AUDIO_POWER_REG_LDO(label, id, power_enable, power_disable) \
SPRD_AUDIO_POWER_LDO_LOW(label, 0, id, 0, 0, power_enable, power_disable, \
		0, 0, 0, 0, 0, 0, 0, 0)

/*
 * We list regulators here if systems need some level of
 * software control over them after boot.
 */
SPRD_AUDIO_POWER_REG_LDO(VREG, 1, vreg_enable, vreg_disable);
SPRD_AUDIO_POWER_SIMPLE_LDO(VB, "VREG", 2, ANA_PMU0,
			VB_EN, power_sleep, 0, 0);
SPRD_AUDIO_POWER_SIMPLE_LDO(BG, "VB", 3, ANA_PMU0,
			BG_EN, NULL, 0, 0);
SPRD_AUDIO_POWER_SIMPLE_LDO(BIAS, "VB", 4, ANA_PMU0,
			BIAS_EN, NULL, 0, 0);
SPRD_AUDIO_POWER_LDO_LOW(MICBIAS1, "BIAS", 5, ANA_PMU1,
			MIC1_BIAS_EN, NULL, NULL, power_sleep,
			 0, 0, 0, 0, 0, 0, 0);
SPRD_AUDIO_POWER_LDO_LOW(MICBIAS2, "BIAS", 6, ANA_PMU1,
			MIC2_BIAS_EN, NULL, NULL, power_sleep,
			 0, 0, 0, 0, 0, 0, 0);
SPRD_AUDIO_POWER_LDO_LOW(HEADMICBIAS, "BG", 7, 0, 0,
			 power_enable, power_disable, power_sleep,
			 0, 0, 0, 0, 0, 0, 0);
SPRD_AUDIO_POWER_SIMPLE_LDO(DCL, "VB", 8, ANA_DCL1,
			DCL_EN, NULL, 0, 0);
SPRD_AUDIO_POWER_SIMPLE_LDO(DIG_CLK_INTC, "DCL", 9, ANA_DCL1,
			DIG_CLK_INTC_EN, NULL, 0, 0);
SPRD_AUDIO_POWER_SIMPLE_LDO(DIG_CLK_HID, "DCL", 10, ANA_DCL1,
			DIG_CLK_HID_EN, NULL, 0, 0);
SPRD_AUDIO_POWER_SIMPLE_LDO(CLK_DCL_32K, "DCL", 11, ANA_CLK0,
			CLK_DCL_32K_EN, NULL, 0, 0);

#define SPRD_AUDIO_DEVICE(label) \
{ \
	.name = "sprd-audio-power", \
	.id = SPRD_AUDIO_POWER_##label, \
	.dev = { \
		.platform_data = &label##_data, \
	}, \
}

static struct platform_device sprd_audio_regulator_devices[] = {
	SPRD_AUDIO_DEVICE(VREG),
	SPRD_AUDIO_DEVICE(VB),
	SPRD_AUDIO_DEVICE(BG),
	SPRD_AUDIO_DEVICE(BIAS),
	SPRD_AUDIO_DEVICE(MICBIAS1),
	SPRD_AUDIO_DEVICE(MICBIAS2),
	SPRD_AUDIO_DEVICE(HEADMICBIAS),
	SPRD_AUDIO_DEVICE(DCL),
	SPRD_AUDIO_DEVICE(DIG_CLK_INTC),
	SPRD_AUDIO_DEVICE(DIG_CLK_HID),
	SPRD_AUDIO_DEVICE(CLK_DCL_32K),
};

static struct device_node *sprd_audio_power_get_ana_node(void)
{
	int i;
	struct device_node *np;
	const char * const comp[] = {
		"unisoc,sc2730-audio-codec",
	};

	/* Check if sc272x codec is available. */
	for (i = 0; i < ARRAY_SIZE(comp); i++) {
		np = of_find_compatible_node(
			NULL, NULL, comp[i]);
		if (np)
			return np;
	}

	return NULL;
}

static int sprd_audio_power_add_devices(void)
{
	int ret = 0;
	int i;
	struct device_node *np;

	np = sprd_audio_power_get_ana_node();
	if (!np) {
		pr_info("there is no analog node!\n");
		return 0;
	}
	if (!of_device_is_available(np)) {
		pr_info(" node 'unisoc,sc2730-audio-codec' is disabled.\n");
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(sprd_audio_regulator_devices); i++) {
		ret = platform_device_register(
			&sprd_audio_regulator_devices[i]);
		if (ret < 0) {
			pr_err("ERR:Add Regulator %d Failed %d\n",
				sprd_audio_regulator_devices[i].id, ret);
			return ret;
		}
	}

	return ret;
}

static int __init sprd_audio_power_init(void)
{
	return sprd_audio_power_add_devices();
}

postcore_initcall(sprd_audio_power_init);

MODULE_DESCRIPTION("SPRD audio power regulator device");
MODULE_AUTHOR("Jian Chen <jian.chen@spreadtrum.com>");
MODULE_ALIAS("platform:sprd-audio-power");
MODULE_LICENSE("GPL");
