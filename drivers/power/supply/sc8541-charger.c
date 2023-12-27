// SPDX-License-Identifier: GPL-2.0:
// Copyright (c) 2021 unisoc.

/*
 * Driver for the TI Solutions BQ2597x charger.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/power/charger-manager.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include <linux/power/sc8541_reg.h>

enum {
	ADC_IBUS,
	ADC_VBUS,
	ADC_VAC1,
	ADC_VAC2,
	ADC_VOUT,
	ADC_VBAT,
	ADC_IBAT,
	ADC_TBUS,
	ADC_TBAT,
	ADC_TDIE,
	ADC_MAX_NUM,
};

static const int sc8541_adc_m[] = 
    {250, 375, 5, 5, 125, 125, 3125, 9766, 9766, 5};

static const int sc8541_adc_l[] = 
    {100, 100, 1, 1, 100, 100, 1000, 100000, 100000, 10};


#define SC8541_ROLE_STDALONE   0
#define SC8541_ROLE_SLAVE	1
#define SC8541_ROLE_MASTER	2

enum {
	SC8541_STDALONE,
	SC8541_SLAVE,
	SC8541_MASTER,
};

static int sc8541_mode_data[] = {
	[SC8541_STDALONE] = SC8541_STDALONE,
	[SC8541_MASTER] = SC8541_ROLE_MASTER,
	[SC8541_SLAVE] = SC8541_ROLE_SLAVE,
};


#define VBAT_REG_STATUS_SHIFT			0
#define IBAT_REG_STATUS_SHIFT			1

#define VBAT_REG_STATUS_MASK		(1 << VBAT_REG_STATUS_SHIFT)
#define IBAT_REG_STATUS_MASK		(1 << VBAT_REG_STATUS_SHIFT)

#define ADC_REG_BASE			0x25
#define ADC_SAMPLE_15BITS		15
#define ADC_SAMPLE_12BITS		12

struct sc8541_charger_cfg {
	bool bat_ovp_disable;
	bool bat_ocp_disable;
	bool bat_ovp_alm_disable;
	bool bat_ocp_alm_disable;

	int bat_ovp_th;
	int bat_ovp_alm_th;
	int bat_ocp_th;
	int bat_ocp_alm_th;
	int bat_delta_volt;

	bool bus_ovp_alm_disable;
	bool bus_ocp_disable;
	bool bus_ocp_alm_disable;

	int bus_ovp_th;
	int bus_ovp_alm_th;
	int bus_ocp_th;
	int bus_ocp_alm_th;

	bool bat_ucp_alm_disable;

	int bat_ucp_alm_th;
	int ac_ovp_th;

	bool bat_therm_disable;
	bool bus_therm_disable;
	bool die_therm_disable;

	/* in % */
	int bat_therm_th;
	/* in % */
	int bus_therm_th;
	/* in degC */
	int die_therm_th;

	int sense_r_mohm;

	int adc_sample_bits;

	bool regulation_disable;
	int ibat_reg_th;
	int vbat_reg_th;
	int vdrop_th;
	int vdrop_deglitch;

	int ss_timeout;
	int wdt_timer;
};

struct sc8541_charger_info {
	struct device *dev;
	struct i2c_client *client;

	int part_no;
	int revision;

	int mode;

	struct mutex data_lock;
	struct mutex i2c_rw_lock;

	bool irq_waiting;
	bool irq_disabled;
	bool irq_response;
	bool resume_completed;

	bool batt_present;
	bool vbus_present;

	bool usb_present;

	/* Register bit status */
	bool charge_enabled;

	/* ADC reading */
	int vbat_volt;
	int vbus_volt;
	int vout_volt;
	int vac_volt;

	int ibat_curr;
	int ibus_curr;

	int bat_temp;
	int bus_temp;
	int die_temp;

	/* alarm/fault status */
	bool bat_ovp_fault;
	bool bat_ocp_fault;
	bool bus_ovp_fault;
	bool bus_ocp_fault;

	bool bat_ovp_alarm;
	bool bat_ocp_alarm;
	bool bus_ovp_alarm;
	bool bus_ocp_alarm;

	bool bat_ucp_alarm;

	bool bat_therm_alarm;
	bool bus_therm_alarm;
	bool die_therm_alarm;

	bool bat_therm_fault;
	bool bus_therm_fault;
	bool die_therm_fault;

	bool bus_err_lo;
	bool bus_err_hi;

	bool therm_shutdown_flag;
	bool therm_shutdown_stat;

	bool vbat_reg;
	bool ibat_reg;

	int prev_alarm;
	int prev_present;
	int prev_fault1;
	int prev_fault2;
	int prev_fault3;

	int chg_ma;
	int chg_mv;

	struct sc8541_charger_cfg *cfg;

	int skip_writes;
	int skip_reads;

	struct sc8541_platform_data *platform_data;

	struct delayed_work monitor_work;
	struct delayed_work wdt_work;
	struct delayed_work det_init_stat_work;

	struct dentry *debug_root;

	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *sc8541_psy;

	unsigned int int_pin;
};
static void sc8541_dump_reg(struct sc8541_charger_info *bq);

static int __sc8541_read_byte(struct sc8541_charger_info *bq, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(bq->client, reg);
	if (ret < 0) {
		dev_err(bq->dev, "i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __sc8541_write_byte(struct sc8541_charger_info *bq, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(bq->client, reg, val);
	if (ret < 0) {
		dev_err(bq->dev, "i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
		       val, reg, ret);
		return ret;
	}
	return 0;
}

static int __sc8541_read_word(struct sc8541_charger_info *bq, u8 reg, u16 *data)
{
	s32 ret;

	ret = i2c_smbus_read_word_data(bq->client, reg);
	if (ret < 0) {
		dev_err(bq->dev, "i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u16) ret;

	return 0;
}

static int sc8541_read_byte(struct sc8541_charger_info *bq, u8 reg, u8 *data)
{
	int ret;

	if (bq->skip_reads) {
		*data = 0;
		return 0;
	}

	mutex_lock(&bq->i2c_rw_lock);
	ret = __sc8541_read_byte(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int sc8541_write_byte(struct sc8541_charger_info *bq, u8 reg, u8 data)
{
	int ret;

	if (bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __sc8541_write_byte(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int sc8541_read_word(struct sc8541_charger_info *bq, u8 reg, u16 *data)
{
	int ret;

	if (bq->skip_reads) {
		*data = 0;
		return 0;
	}

	mutex_lock(&bq->i2c_rw_lock);
	ret = __sc8541_read_word(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int sc8541_update_bits(struct sc8541_charger_info *bq, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	if (bq->skip_reads || bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __sc8541_read_byte(bq, reg, &tmp);
	if (ret) {
		dev_err(bq->dev, "Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __sc8541_write_byte(bq, reg, tmp);
	if (ret)
		dev_err(bq->dev, "Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&bq->i2c_rw_lock);
	return ret;
}

static int sc8541_enable_charge(struct sc8541_charger_info *bq, bool enable)
{
	u8 val;

	if (enable)
		val = SC8541_CHG_ENABLE;
	else
		val = SC8541_CHG_DISABLE;

	val <<= SC8541_CHG_EN_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_0F, SC8541_CHG_EN_MASK, val);

}

static int sc8541_check_charge_enabled(struct sc8541_charger_info *bq, bool *enabled)
{
	int ret;
	u8 val;

	ret = sc8541_read_byte(bq, SC8541_REG_17, &val);
	if (ret < 0) {
		dev_err(bq->dev, "failed to check charge enable, ret = %d\n", ret);
		*enabled = false;
		return ret;
	}

	*enabled = !!(val & CP_SWITCHING_STAT_MASK);

	return 0;
}

static int sc8541_reset(struct sc8541_charger_info *bq, bool reset)
{
	u8 val;

	if (reset)
		val = SC8541_REG_RST_ENABLE;
	else
		val = SC8541_REG_RST_DISABLE;

	val <<= SC8541_REG_RST_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_0F, SC8541_REG_RST_MASK, val);
}

static int sc8541_enable_wdt(struct sc8541_charger_info *bq, bool enable)
{
	u8 val;

	if (enable)
		val = SC8541_WATCHDOG_ENABLE;
	else
		val = SC8541_WATCHDOG_DISABLE;

	val <<= SC8541_WATCHDOG_DIS_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_10, SC8541_WATCHDOG_DIS_MASK, val);
}

static int sc8541_set_wdt(struct sc8541_charger_info *bq, int ms)
{
	u8 val;

	if (ms == 500)
		val = SC8541_WATCHDOG_0P5S;
	else if (ms == 1000)
		val = SC8541_WATCHDOG_1S;
	else if (ms == 5000)
		val = SC8541_WATCHDOG_5S;
	else if (ms == 30000)
		val = SC8541_WATCHDOG_30S;
	else
		val = SC8541_WATCHDOG_30S;

	val <<= SC8541_WATCHDOG_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_10, SC8541_WATCHDOG_MASK, val);
}

static int sc8541_enable_batovp(struct sc8541_charger_info *bq, bool enable)
{
	u8 val;

	if (enable)
		val = SC8541_BAT_OVP_ENABLE;
	else
		val = SC8541_BAT_OVP_DISABLE;

	val <<= SC8541_BAT_OVP_DIS_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_00, SC8541_BAT_OVP_DIS_MASK, val);
}

static int sc8541_set_batovp_th(struct sc8541_charger_info *bq, int threshold)
{
	u8 val;

	if (threshold < SC8541_BAT_OVP_BASE)
		threshold = SC8541_BAT_OVP_BASE;
	else if (threshold > SC8541_BAT_OVP_MAX)
		threshold = SC8541_BAT_OVP_MAX;

	val = (threshold - SC8541_BAT_OVP_BASE) / SC8541_BAT_OVP_LSB;

	val <<= SC8541_BAT_OVP_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_00, SC8541_BAT_OVP_MASK, val);
}

static int sc8541_enable_batovp_alarm(struct sc8541_charger_info *bq, bool enable)
{
	u8 val;

	if (enable)
		val = SC8541_BAT_OVP_ALM_ENABLE;
	else
		val = SC8541_BAT_OVP_ALM_DISABLE;

	val <<= SC8541_BAT_OVP_ALM_DIS_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_01, SC8541_BAT_OVP_ALM_DIS_MASK, val);
}

static int sc8541_set_batovp_alarm_th(struct sc8541_charger_info *bq, int threshold)
{
	u8 val;

	if (threshold < SC8541_BAT_OVP_ALM_BASE)
		threshold = SC8541_BAT_OVP_ALM_BASE;

	val = (threshold - SC8541_BAT_OVP_ALM_BASE) / SC8541_BAT_OVP_ALM_LSB;

	val <<= SC8541_BAT_OVP_ALM_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_01, SC8541_BAT_OVP_ALM_MASK, val);
}

static int sc8541_enable_batocp(struct sc8541_charger_info *bq, bool enable)
{
	u8 val;

	if (enable)
		val = SC8541_BAT_OCP_ENABLE;
	else
		val = SC8541_BAT_OCP_DISABLE;

	val <<= SC8541_BAT_OCP_DIS_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_02, SC8541_BAT_OCP_DIS_MASK, val);
}

static int sc8541_set_batocp_th(struct sc8541_charger_info *bq, int threshold)
{
	u8 val;

	if (threshold < SC8541_BAT_OCP_BASE)
		threshold = SC8541_BAT_OCP_BASE;
	else if (threshold > SC8541_BAT_OCP_MAX)
		threshold = SC8541_BAT_OCP_MAX;

	val = (threshold - SC8541_BAT_OCP_BASE) / SC8541_BAT_OCP_LSB;

	val <<= SC8541_BAT_OCP_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_02, SC8541_BAT_OCP_MASK, val);
}

static int sc8541_enable_batocp_alarm(struct sc8541_charger_info *bq, bool enable)
{
	u8 val;

	if (enable)
		val = SC8541_BAT_OCP_ALM_ENABLE;
	else
		val = SC8541_BAT_OCP_ALM_DISABLE;

	val <<= SC8541_BAT_OCP_ALM_DIS_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_03,
				   SC8541_BAT_OCP_ALM_DIS_MASK, val);
}

static int sc8541_set_batocp_alarm_th(struct sc8541_charger_info *bq, int threshold)
{
	u8 val;

	if (threshold < SC8541_BAT_OCP_ALM_BASE)
		threshold = SC8541_BAT_OCP_ALM_BASE;
	else if (threshold > SC8541_BAT_OCP_ALM_MAX)
		threshold = SC8541_BAT_OCP_ALM_MAX;

	val = (threshold - SC8541_BAT_OCP_ALM_BASE) / SC8541_BAT_OCP_ALM_LSB;

	val <<= SC8541_BAT_OCP_ALM_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_03,  SC8541_BAT_OCP_ALM_MASK, val);
}

static int sc8541_set_busovp_th(struct sc8541_charger_info *bq, int threshold)
{
	u8 val;

	if (threshold < SC8541_BUS_OVP_BASE)
		threshold = SC8541_BUS_OVP_BASE;
	else if (threshold > SC8541_BUS_OVP_MAX)
		threshold = SC8541_BUS_OVP_MAX;

	val = (threshold - SC8541_BUS_OVP_BASE) / SC8541_BUS_OVP_LSB;

	val <<= SC8541_BUS_OVP_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_06, SC8541_BUS_OVP_MASK, val);
}

static int sc8541_enable_busovp_alarm(struct sc8541_charger_info *bq, bool enable)
{
	u8 val;

	if (enable)
		val = SC8541_BUS_OVP_ALM_ENABLE;
	else
		val = SC8541_BUS_OVP_ALM_DISABLE;

	val <<= SC8541_BUS_OVP_ALM_DIS_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_07,
				   SC8541_BUS_OVP_ALM_DIS_MASK, val);
}

static int sc8541_set_busovp_alarm_th(struct sc8541_charger_info *bq, int threshold)
{
	u8 val;

	if (threshold < SC8541_BUS_OVP_ALM_BASE)
		threshold = SC8541_BUS_OVP_ALM_BASE;

	val = (threshold - SC8541_BUS_OVP_ALM_BASE) / SC8541_BUS_OVP_ALM_LSB;

	val <<= SC8541_BUS_OVP_ALM_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_07, SC8541_BUS_OVP_ALM_MASK, val);
}

static int sc8541_enable_busocp(struct sc8541_charger_info *bq, bool enable)
{
	u8 val;

	if (enable)
		val = SC8541_BUS_OCP_ENABLE;
	else
		val = SC8541_BUS_OCP_DISABLE;

	val <<= SC8541_BUS_OCP_DIS_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_08, SC8541_BUS_OCP_DIS_MASK, val);
}

static int sc8541_set_busocp_th(struct sc8541_charger_info *bq, int threshold)
{
	u8 val;

	if (threshold < SC8541_BUS_OCP_BASE)
		threshold = SC8541_BUS_OCP_BASE;

	val = (threshold - SC8541_BUS_OCP_BASE) / SC8541_BUS_OCP_LSB;

	val <<= SC8541_BUS_OCP_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_08, SC8541_BUS_OCP_MASK, val);
}

static int sc8541_enable_busocp_alarm(struct sc8541_charger_info *bq, bool enable)
{
	u8 val;
	return 0;
	if (enable)
		val = SC8541_BUS_OCP_ALM_ENABLE;
	else
		val = SC8541_BUS_OCP_ALM_DISABLE;

	val <<= SC8541_BUS_OCP_ALM_DIS_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_09,
				   SC8541_BUS_OCP_ALM_DIS_MASK, val);
}

static int sc8541_set_busocp_alarm_th(struct sc8541_charger_info *bq, int threshold)
{
	u8 val;
	return 0;
	if (threshold < SC8541_BUS_OCP_ALM_BASE)
		threshold = SC8541_BUS_OCP_ALM_BASE;

	val = (threshold - SC8541_BUS_OCP_ALM_BASE) / SC8541_BUS_OCP_ALM_LSB;

	val <<= SC8541_BUS_OCP_ALM_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_09, SC8541_BUS_OCP_ALM_MASK, val);
}

static int sc8541_enable_batucp_alarm(struct sc8541_charger_info *bq, bool enable)
{
	u8 val;
	return 0;

	if (enable)
		val = SC8541_BAT_UCP_ALM_ENABLE;
	else
		val = SC8541_BAT_UCP_ALM_DISABLE;

	val <<= SC8541_BAT_UCP_ALM_DIS_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_04, SC8541_BAT_UCP_ALM_DIS_MASK, val);
}

static int sc8541_set_batucp_alarm_th(struct sc8541_charger_info *bq, int threshold)
{
	u8 val;
	return 0;

	if (threshold < SC8541_BAT_UCP_ALM_BASE)
		threshold = SC8541_BAT_UCP_ALM_BASE;
	else if (threshold > SC8541_BAT_UCP_ALM_MAX)
		threshold = SC8541_BAT_UCP_ALM_MAX;

	val = (threshold - SC8541_BAT_UCP_ALM_BASE) / SC8541_BAT_UCP_ALM_LSB;

	val <<= SC8541_BAT_UCP_ALM_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_04, SC8541_BAT_UCP_ALM_MASK, val);
}

static int sc8541_set_acovp_th(struct sc8541_charger_info *bq, int threshold)
{
	u8 val;

	if (threshold == SC8541_AC1_OVP_6P5V) {
		dev_info(bq->dev, "%s, VAC1_OVP set default 6.5V\n", __func__);
		threshold = 0x00;
	}else if (threshold == SC8541_AC1_OVP_10P5V) {
		threshold = 0x01;
	}else if (threshold <= 12) {
		threshold = 0x02;
	}else if (threshold <= 14) {
		threshold = 0x03;
	} else if (threshold <= 16) {
		threshold = 0x04;
	}else if (threshold > SC8541_AC1_OVP_MAX) {
		threshold = 0x05;
	}

	val = threshold;
	val <<= SC8541_AC1_OVP_SHIFT;
	sc8541_update_bits(bq, SC8541_REG_0E, SC8541_AC1_OVP_MASK, val);

	val = threshold;
	val <<= SC8541_AC2_OVP_SHIFT;
	sc8541_update_bits(bq, SC8541_REG_0E, SC8541_AC2_OVP_MASK, val);

	return 0;
}

static int sc8541_set_vdrop_th(struct sc8541_charger_info *bq, int threshold)
{
	u8 val;
	return 0;
	if (threshold <= 300)
		val = SC8541_VDROP_THRESHOLD_300MV;
	else
		val = SC8541_VDROP_THRESHOLD_400MV;

	val <<= SC8541_VDROP_THRESHOLD_SET_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_05,
				   SC8541_VDROP_THRESHOLD_SET_MASK,
				   val);
}

static int sc8541_set_vdrop_deglitch(struct sc8541_charger_info *bq, int us)
{
	u8 val;
	return 0;
	if (us <= 8)
		val = SC8541_VDROP_DEGLITCH_8US;
	else
		val = SC8541_VDROP_DEGLITCH_5MS;

	val <<= SC8541_VDROP_DEGLITCH_SET_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_05,
				   SC8541_VDROP_DEGLITCH_SET_MASK, val);
}

static int sc8541_enable_bat_therm(struct sc8541_charger_info *bq, bool enable)
{
	u8 val;

	if (enable)
		val = SC8541_TSBAT_ENABLE;
	else
		val = SC8541_TSBAT_DISABLE;

	val <<= SC8541_TSBAT_DIS_SHIFT;
	
	return sc8541_update_bits(bq, SC8541_REG_0A, SC8541_TSBAT_DIS_MASK, val);
}

/*
 * the input threshold is the raw value that would write to register directly.
 */
static int sc8541_set_bat_therm_th(struct sc8541_charger_info *bq, u8 threshold)
{
	return sc8541_write_byte(bq, SC8541_REG_0D, threshold);
}

static int sc8541_enable_bus_therm(struct sc8541_charger_info *bq, bool enable)
{
	u8 val;

	if (enable)
		val = SC8541_TSBUS_ENABLE;
	else
		val = SC8541_TSBUS_DISABLE;

	val <<= SC8541_TSBUS_DIS_SHIFT;


	return sc8541_update_bits(bq, SC8541_REG_0A, SC8541_TSBUS_DIS_MASK, val);
}

/*
 * the input threshold is the raw value that would write to register directly.
 */
static int sc8541_set_bus_therm_th(struct sc8541_charger_info *bq, u8 threshold)
{
	return sc8541_write_byte(bq, SC8541_REG_0C, threshold);
}

static int sc8541_enable_die_therm(struct sc8541_charger_info *bq, bool enable)
{
	u8 val;

	if (enable)
		val = SC8541_TDIE_ENABLE;
	else
		val = SC8541_TDIE_DISABLE;

	val <<= SC8541_TDIE_DIS_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_20, SC8541_TDIE_DIS_MASK, val);
}

/*
 * please be noted that the unit here is degC
 */
static int sc8541_set_die_therm_th(struct sc8541_charger_info *bq, u8 threshold)
{
	u8 val;

	/* BE careful, LSB is here is 1/LSB, so we use multiply here */
	val = (threshold - SC8541_TDIE_ALM_BASE) * SC8541_TDIE_ALM_LSB;
	val <<= SC8541_TDIE_ALM_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_0B, SC8541_TDIE_ALM_MASK, val);
}

static int sc8541_enable_adc(struct sc8541_charger_info *bq, bool enable)
{
	u8 val;

	if (enable)
		val = SC8541_ADC_ENABLE;
	else
		val = SC8541_ADC_DISABLE;

	val <<= SC8541_ADC_EN_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_23, SC8541_ADC_EN_MASK, val);
}

static int sc8541_set_adc_average(struct sc8541_charger_info *bq, bool avg)
{
	u8 val;
	return 0;
	if (avg)
		val = SC8541_ADC_AVG_ENABLE;
	else
		val = SC8541_ADC_AVG_DISABLE;

	val <<= SC8541_ADC_AVG_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_23, SC8541_ADC_AVG_MASK, val);
}

static int sc8541_set_adc_scanrate(struct sc8541_charger_info *bq, bool oneshot)
{
	u8 val;

	if (oneshot)
		val = SC8541_ADC_RATE_ONESHOT;
	else
		val = SC8541_ADC_RATE_CONTINUOUS;

	val <<= SC8541_ADC_RATE_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_23, SC8541_ADC_RATE_MASK, val);
}

static int sc8541_set_adc_bits(struct sc8541_charger_info *bq, int bits)
{
	u8 val;
	return 0;
	if (bits > ADC_SAMPLE_15BITS)
		bits = ADC_SAMPLE_15BITS;
	if (bits < ADC_SAMPLE_12BITS)
		bits = ADC_SAMPLE_12BITS;
	val = ADC_SAMPLE_15BITS - bits;

	val <<= SC8541_ADC_SAMPLE_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_23, SC8541_ADC_SAMPLE_MASK, val);
}

static int sc8541_get_adc_data(struct sc8541_charger_info *bq, int channel,  int *result)
{
	int ret;
	u16 val;
	s16 t;

	if (channel > ADC_MAX_NUM)
		return -EINVAL;

//	sc8541_enable_adc(sc, true);
//	msleep(50);

	ret = sc8541_read_word(bq, ADC_REG_BASE + (channel << 1), &val);
	if (ret < 0)
		return ret;

	t = val & 0xF;
	t <<= 8;
	t |= (val >> 8) & 0xFF;
	 *result = t *sc8541_adc_m[channel] / sc8541_adc_l[channel];

//	sc8541_enable_adc(sc, false);

	return 0;
}

static int sc8541_set_adc_scan(struct sc8541_charger_info *bq, int channel, bool enable)
{
	u8 reg;
	u8 mask;
	u8 shift;
	u8 val;

	if (channel > ADC_MAX_NUM)
		return -EINVAL;

	if (channel == ADC_IBUS) {
		reg = SC8541_REG_23;
		shift = SC8541_IBUS_ADC_DIS_SHIFT;
		mask = SC8541_IBUS_ADC_DIS_MASK;
	}	
	else if (channel == ADC_VBUS) {
		reg = SC8541_REG_23;
		shift = SC8541_VBUS_ADC_DIS_SHIFT;
		mask = SC8541_VBUS_ADC_DIS_MASK;
	} else {
		reg = SC8541_REG_24;
		shift = 9 - channel;
		mask = 1 << shift;
	}

	if (enable)
		val = 0 << shift;
	else
		val = 1 << shift;

	return sc8541_update_bits(bq, reg, mask, val);
}

static int sc8541_set_alarm_int_mask(struct sc8541_charger_info *bq, u8 mask)
{
	int ret;
	u8 val;

	ret = sc8541_read_byte(bq, SC8541_REG_1F, &val);
	if (ret)
		return ret;
	if(mask & VBUS_INSERT)
		val |= VBUS_INSERT;
	if(mask & VBAT_INSERT)
		val |= VBAT_INSERT;
	ret = sc8541_write_byte(bq, SC8541_REG_1F, val);
		

	ret = sc8541_read_byte(bq, SC8541_REG_20, &val);
	if (ret)
		return ret;
	if(mask & ADC_DONE)
		val |= ADC_DONE;
	ret = sc8541_write_byte(bq, SC8541_REG_20, val);

	return ret;
}

static int sc8541_set_sense_resistor(struct sc8541_charger_info *bq, int r_mohm)
{
	u8 val;

	if (r_mohm == 2)
		val = SC8541_SET_IBAT_SNS_RES_2MHM;
	else if (r_mohm == 5)
		val = SC8541_SET_IBAT_SNS_RES_5MHM;
	else
		return -EINVAL;

	val <<= SC8541_SET_IBAT_SNS_RES_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_11,
				   SC8541_SET_IBAT_SNS_RES_MASK,
				   val);
}

static int sc8541_disable_regulation(struct sc8541_charger_info *bq, bool disable)
{
	u8 val;
	return 0;
	if (disable)
		val = SC8541_EN_REGULATION_DISABLE;
	else
		val = SC8541_EN_REGULATION_ENABLE;

	val <<= SC8541_EN_REGULATION_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_2B,
				SC8541_EN_REGULATION_MASK,
				val);
}

static int sc8541_set_ss_timeout(struct sc8541_charger_info *bq, int timeout)
{
	u8 val;

	switch (timeout) {
	case 0:
		val = SC8541_SS_TIMEOUT_6P25MS;
		break;
	case 12:
		val = SC8541_SS_TIMEOUT_12P5MS;
		break;
	case 25:
		val = SC8541_SS_TIMEOUT_25MS;
		break;
	case 50:
		val = SC8541_SS_TIMEOUT_50MS;
		break;
	case 100:
		val = SC8541_SS_TIMEOUT_100MS;
		break;
	case 400:
		val = SC8541_SS_TIMEOUT_400MS;
		break;
	case 1500:
		val = SC8541_SS_TIMEOUT_1500MS;
		break;
	case 100000:
		val = SC8541_SS_TIMEOUT_100000MS;
		break;
	default:
		val = SC8541_SS_TIMEOUT_6P25MS;
		break;
	}

	val <<= SC8541_SS_TIMEOUT_SET_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_11,
				   SC8541_SS_TIMEOUT_SET_MASK,
				   val);
}

static int sc8541_set_ibat_reg_th(struct sc8541_charger_info *bq, int th_ma)
{
	u8 val;
	return 0;

	if (th_ma == 200)
		val = SC8541_IBAT_REG_200MA;
	else if (th_ma == 300)
		val = SC8541_IBAT_REG_300MA;
	else if (th_ma == 400)
		val = SC8541_IBAT_REG_400MA;
	else if (th_ma == 500)
		val = SC8541_IBAT_REG_500MA;
	else
		val = SC8541_IBAT_REG_500MA;

	val <<= SC8541_IBAT_REG_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_2C, SC8541_IBAT_REG_MASK, val);
}

static int sc8541_set_vbat_reg_th(struct sc8541_charger_info *bq, int th_mv)
{
	u8 val;
	return 0;

	if (th_mv == 50)
		val = SC8541_VBAT_REG_50MV;
	else if (th_mv == 100)
		val = SC8541_VBAT_REG_100MV;
	else if (th_mv == 150)
		val = SC8541_VBAT_REG_150MV;
	else
		val = SC8541_VBAT_REG_200MV;

	val <<= SC8541_VBAT_REG_SHIFT;

	return sc8541_update_bits(bq, SC8541_REG_2C, SC8541_VBAT_REG_MASK, val);
}

static int sc8541_check_vbus_error_status(struct sc8541_charger_info *bq)
{
	int ret;
	u8 data;

	bq->bus_err_lo = false;
	bq->bus_err_hi = false;

	ret = sc8541_read_byte(bq, SC8541_REG_17, &data);
	if (ret == 0) {
		dev_err(bq->dev, "vbus error >>>>%02x\n", data);
		bq->bus_err_lo = !!(data & SC8541_VBUS_ERRORLO_STAT_MASK);
		bq->bus_err_hi = !!(data & SC8541_VBUS_ERRORHI_STAT_MASK);
	}

	return ret;
}

static int sc8541_get_work_mode(struct sc8541_charger_info *bq, int *mode)
{
	int ret;
	u8 val;

	ret = sc8541_read_byte(bq, SC8541_REG_12, &val);

	if (ret) {
		dev_err(bq->dev, "Failed to read operation mode register\n");
		return ret;
	}

	val = (val & SC8541_MS_MASK) >> SC8541_MS_SHIFT;
	if (val == SC8541_MS_MASTER)
		*mode = SC8541_ROLE_MASTER;
	else if (val == SC8541_MS_SLAVE)
		*mode = SC8541_ROLE_SLAVE;
	else
		*mode = SC8541_ROLE_STDALONE;

	dev_info(bq->dev, "work mode:%s\n", *mode == SC8541_ROLE_STDALONE ? "Standalone" :
		 (*mode == SC8541_ROLE_SLAVE ? "Slave" : "Master"));
	return ret;
}

static int sc8541_detect_device(struct sc8541_charger_info *bq)
{
	int ret;
	u8 data;

	ret = sc8541_read_byte(bq, SC8541_REG_22, &data);
	if (ret == 0) {
		bq->part_no = (data & SC8541_DEV_ID_MASK);
		bq->part_no >>= SC8541_DEV_ID_SHIFT;
	}
	dev_err(bq->dev, "%s;%x;\n",__func__,bq->part_no);
	if(bq->part_no != 0x41)
		ret = -ENODEV;
	return ret;
}

static int sc8541_parse_dt(struct sc8541_charger_info *bq, struct device *dev)
{
	int ret;
	struct device_node *np = dev->of_node;

	bq->cfg = devm_kzalloc(dev, sizeof(struct sc8541_charger_info),
					GFP_KERNEL);

	if (!bq->cfg)
		return -ENOMEM;

	bq->cfg->bat_ovp_disable =
		of_property_read_bool(np, "sc,sc8541,bat-ovp-disable");
	bq->cfg->bat_ocp_disable =
		of_property_read_bool(np, "sc,sc8541,bat-ocp-disable");
	bq->cfg->bat_ovp_alm_disable =
		of_property_read_bool(np, "sc,sc8541,bat-ovp-alarm-disable");
	bq->cfg->bat_ocp_alm_disable =
		of_property_read_bool(np, "sc,sc8541,bat-ocp-alarm-disable");
	bq->cfg->bus_ocp_disable =
		of_property_read_bool(np, "sc,sc8541,bus-ocp-disable");
	bq->cfg->bus_ovp_alm_disable =
		of_property_read_bool(np, "sc,sc8541,bus-ovp-alarm-disable");
	bq->cfg->bus_ocp_alm_disable
		= of_property_read_bool(np, "sc,sc8541,bus-ocp-alarm-disable");
	bq->cfg->bat_ucp_alm_disable
		= of_property_read_bool(np, "sc,sc8541,bat-ucp-alarm-disable");
	bq->cfg->bat_therm_disable
		= of_property_read_bool(np, "sc,sc8541,bat-therm-disable");
	bq->cfg->bus_therm_disable
		= of_property_read_bool(np, "sc,sc8541,bus-therm-disable");
	bq->cfg->die_therm_disable
		= of_property_read_bool(np, "sc,sc8541,die-therm-disable");
	bq->cfg->regulation_disable
		= of_property_read_bool(np, "sc,sc8541,regulation-disable");
	bq->int_pin = of_get_named_gpio(np, "irq-gpio", 0);

	if (!gpio_is_valid(bq->int_pin))
		dev_info(bq->dev, "no irq pin provided\n");

	ret = of_property_read_u32(np, "sc,sc8541,bat-ovp-threshold",
				   &bq->cfg->bat_ovp_th);
	if (ret) {
		dev_err(bq->dev, "failed to read bat-ovp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,bat-ovp-alarm-threshold",
				   &bq->cfg->bat_ovp_alm_th);
	if (ret) {
		dev_err(bq->dev, "failed to read bat-ovp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,bat-ocp-threshold",
				   &bq->cfg->bat_ocp_th);
	if (ret) {
		dev_err(bq->dev, "failed to read bat-ocp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,bat-ocp-alarm-threshold",
				   &bq->cfg->bat_ocp_alm_th);
	if (ret) {
		dev_err(bq->dev, "failed to read bat-ocp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,bus-ovp-threshold",
				   &bq->cfg->bus_ovp_th);
	if (ret) {
		dev_err(bq->dev, "failed to read bus-ovp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,bus-ovp-alarm-threshold",
				   &bq->cfg->bus_ovp_alm_th);
	if (ret) {
		dev_err(bq->dev, "failed to read bus-ovp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,bus-ocp-threshold",
				   &bq->cfg->bus_ocp_th);
	if (ret) {
		dev_err(bq->dev, "failed to read bus-ocp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,bus-ocp-alarm-threshold",
				   &bq->cfg->bus_ocp_alm_th);
	if (ret) {
		dev_err(bq->dev, "failed to read bus-ocp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,bat-ucp-alarm-threshold",
				   &bq->cfg->bat_ucp_alm_th);
	if (ret) {
		dev_err(bq->dev, "failed to read bat-ucp-alarm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,bat-therm-threshold",
				   &bq->cfg->bat_therm_th);
	if (ret) {
		dev_err(bq->dev, "failed to read bat-therm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,bus-therm-threshold",
				   &bq->cfg->bus_therm_th);
	if (ret) {
		dev_err(bq->dev, "failed to read bus-therm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,die-therm-threshold",
				   &bq->cfg->die_therm_th);
	if (ret) {
		dev_err(bq->dev, "failed to read die-therm-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,ac-ovp-threshold",
				   &bq->cfg->ac_ovp_th);
	if (ret) {
		dev_err(bq->dev, "failed to read ac-ovp-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,sense-resistor-mohm",
				   &bq->cfg->sense_r_mohm);
	if (ret) {
		dev_err(bq->dev, "failed to read sense-resistor-mohm\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,adc-sample-bits",
				   &bq->cfg->adc_sample_bits);
	if (ret) {
		dev_err(bq->dev, "failed to read adc-sample-bits\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,ibat-regulation-threshold",
				   &bq->cfg->ibat_reg_th);
	if (ret) {
		dev_err(bq->dev, "failed to read ibat-regulation-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,vbat-regulation-threshold",
				   &bq->cfg->vbat_reg_th);
	if (ret) {
		dev_err(bq->dev, "failed to read vbat-regulation-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,vdrop-threshold",
				   &bq->cfg->vdrop_th);
	if (ret) {
		dev_err(bq->dev, "failed to read vdrop-threshold\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,vdrop-deglitch",
				   &bq->cfg->vdrop_deglitch);
	if (ret) {
		dev_err(bq->dev, "failed to read vdrop-deglitch\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,ss-timeout",
				   &bq->cfg->ss_timeout);
	if (ret) {
		dev_err(bq->dev, "failed to read ss-timeout\n");
		return ret;
	}

	ret = of_property_read_u32(np, "sc,sc8541,watchdog-timer",
				   &bq->cfg->wdt_timer);
	if (ret) {
		dev_err(bq->dev, "failed to read watchdog-timer\n");
		return ret;
	}

	if (bq->cfg->bat_ovp_th && bq->cfg->bat_ovp_alm_th) {
		bq->cfg->bat_delta_volt = bq->cfg->bat_ovp_th - bq->cfg->bat_ovp_alm_th;
		if (bq->cfg->bat_delta_volt < 0)
			bq->cfg->bat_delta_volt = 0;
	}

	return 0;
}

static int sc8541_init_protection(struct sc8541_charger_info *bq)
{
	int ret;

	ret = sc8541_enable_batovp(bq, !bq->cfg->bat_ovp_disable);
	if (ret)
		dev_err(bq->dev, "%s, failed to %s bat ovp, ret = %d\n",
			__func__, bq->cfg->bat_ovp_disable ? "disable" : "enable", ret);

	ret = sc8541_enable_batocp(bq, !bq->cfg->bat_ocp_disable);
	if (ret)
		dev_err(bq->dev, "%s, failed to %s bat ocp, ret = %d\n",
			__func__, bq->cfg->bat_ocp_disable ? "disable" : "enable", ret);

	ret = sc8541_enable_batovp_alarm(bq, !bq->cfg->bat_ovp_alm_disable);
	if (ret)
		dev_err(bq->dev, "%s, failed to %s bat ovp alarm, ret = %d\n",
			__func__, bq->cfg->bat_ovp_alm_disable ? "disable" : "enable", ret);

	ret = sc8541_enable_batocp_alarm(bq, !bq->cfg->bat_ocp_alm_disable);
	if (ret)
		dev_err(bq->dev, "%s, failed to %s bat ocp alarm, ret = %d\n",
			__func__, bq->cfg->bat_ocp_alm_disable ? "disable" : "enable", ret);

	ret = sc8541_enable_batucp_alarm(bq, !bq->cfg->bat_ucp_alm_disable);
	if (ret)
		dev_err(bq->dev, "%s, failed to %s bat ocp alarm, ret = %d\n",
			__func__, bq->cfg->bat_ucp_alm_disable ? "disable" : "enable", ret);

	ret = sc8541_enable_busovp_alarm(bq, !bq->cfg->bus_ovp_alm_disable);
	if (ret)
		dev_err(bq->dev, "%s, failed to %s bus ovp alarm, ret = %d\n",
			__func__, bq->cfg->bus_ovp_alm_disable ? "disable" : "enable", ret);

	ret = sc8541_enable_busocp(bq, !bq->cfg->bus_ocp_disable);
	if (ret)
		dev_err(bq->dev, "%s, failed to %s bus ocp, ret = %d\n",
			__func__, bq->cfg->bus_ocp_disable ? "disable" : "enable", ret);

	ret = sc8541_enable_busocp_alarm(bq, !bq->cfg->bus_ocp_alm_disable);
	if (ret)
		dev_err(bq->dev, "%s, failed to %s bus ocp alarm, ret = %d\n",
			__func__, bq->cfg->bus_ocp_alm_disable ? "disable" : "enable", ret);

	ret = sc8541_enable_bat_therm(bq, !bq->cfg->bat_therm_disable);
	if (ret)
		dev_err(bq->dev, "%s, failed to %s bat therm, ret = %d\n",
			__func__, bq->cfg->bat_therm_disable ? "disable" : "enable", ret);

	ret = sc8541_enable_bus_therm(bq, !bq->cfg->bus_therm_disable);
	if (ret)
		dev_err(bq->dev, "%s, failed to %s bus therm, ret = %d\n",
			__func__, bq->cfg->bus_therm_disable ? "disable" : "enable", ret);

	ret = sc8541_enable_die_therm(bq, !bq->cfg->die_therm_disable);
	if (ret)
		dev_err(bq->dev, "%s, failed to %s die therm, ret = %d\n",
			__func__, bq->cfg->die_therm_disable ? "disable" : "enable", ret);

	ret = sc8541_set_batovp_th(bq, bq->cfg->bat_ovp_th);
	if (ret)
		dev_err(bq->dev, "%s, failed to set bat ovp th %d, ret = %d\n",
			__func__, bq->cfg->bat_ovp_th, ret);

	ret = sc8541_set_batovp_alarm_th(bq, bq->cfg->bat_ovp_alm_th);
	if (ret)
		dev_err(bq->dev, "%s, failed to set bat ovp alarm th %d, ret = %d\n",
			__func__, bq->cfg->bat_ovp_alm_th, ret);

	ret = sc8541_set_batocp_th(bq, bq->cfg->bat_ocp_th);
	if (ret)
		dev_err(bq->dev, "%s, failed to set bat ocp th %d, ret = %d\n",
			__func__, bq->cfg->bat_ocp_th, ret);

	ret = sc8541_set_batocp_alarm_th(bq, bq->cfg->bat_ocp_alm_th);
	if (ret)
		dev_err(bq->dev, "%s, failed to set bat ocp alarm th %d, ret = %d\n",
			__func__, bq->cfg->bat_ocp_alm_th, ret);

	ret = sc8541_set_busovp_th(bq, bq->cfg->bus_ovp_th);
	if (ret)
		dev_err(bq->dev, "%s, failed to set bus ovp th %d, ret = %d\n",
			__func__, bq->cfg->bus_ovp_th, ret);

	ret = sc8541_set_busovp_alarm_th(bq, bq->cfg->bus_ovp_alm_th);
	if (ret)
		dev_err(bq->dev, "%s, failed to set bus ovp alarm th %d, ret = %d\n",
			__func__, bq->cfg->bus_ovp_alm_th, ret);

	ret = sc8541_set_busocp_th(bq, bq->cfg->bus_ocp_th);
	if (ret)
		dev_err(bq->dev, "%s, failed to set bus ocp th %d, ret = %d\n",
			__func__, bq->cfg->bus_ocp_th, ret);

	ret = sc8541_set_busocp_alarm_th(bq, bq->cfg->bus_ocp_alm_th);
	if (ret)
		dev_err(bq->dev, "%s, failed to set bus ocp alarm th %d, ret = %d\n",
			__func__, bq->cfg->bus_ocp_alm_th, ret);

	ret = sc8541_set_batucp_alarm_th(bq, bq->cfg->bat_ucp_alm_th);
	if (ret)
		dev_err(bq->dev, "%s, failed to set bat ucp th %d, ret = %d\n",
			__func__, bq->cfg->bat_ucp_alm_th, ret);

	ret = sc8541_set_bat_therm_th(bq, bq->cfg->bat_therm_th);
	if (ret)
		dev_err(bq->dev, "%s, failed to set die therm th %d, ret = %d\n",
			__func__, bq->cfg->bat_therm_th, ret);

	ret = sc8541_set_bus_therm_th(bq, bq->cfg->bus_therm_th);
	if (ret)
		dev_err(bq->dev, "%s, failed to set bus therm th %d, ret = %d\n",
			__func__, bq->cfg->bus_therm_th, ret);

	ret = sc8541_set_die_therm_th(bq, bq->cfg->die_therm_th);
	if (ret)
		dev_err(bq->dev, "%s, failed to set die therm th %d, ret = %d\n",
			__func__, bq->cfg->die_therm_th, ret);

	ret = sc8541_set_acovp_th(bq, bq->cfg->ac_ovp_th);
	if (ret)
		dev_err(bq->dev, "%s, failed to set ac ovp th %d, ret = %d\n",
			__func__, bq->cfg->ac_ovp_th, ret);

	return 0;
}

static int sc8541_init_adc(struct sc8541_charger_info *bq)
{

	sc8541_set_adc_scanrate(bq, false);
	sc8541_set_adc_bits(bq, bq->cfg->adc_sample_bits);
	sc8541_set_adc_average(bq, true);
	sc8541_set_adc_scan(bq, ADC_IBUS, true);
	sc8541_set_adc_scan(bq, ADC_VBUS, true);
	sc8541_set_adc_scan(bq, ADC_VOUT, false);
	sc8541_set_adc_scan(bq, ADC_VBAT, true);
	sc8541_set_adc_scan(bq, ADC_IBAT, false);
	sc8541_set_adc_scan(bq, ADC_TBUS, true);
	sc8541_set_adc_scan(bq, ADC_TBAT, true);
	sc8541_set_adc_scan(bq, ADC_TDIE, true);
	sc8541_set_adc_scan(bq, ADC_VAC1, true);
	sc8541_set_adc_scan(bq, ADC_VAC2, true);

	sc8541_enable_adc(bq, true);

	return 0;
}

static int sc8541_init_int_src(struct sc8541_charger_info *bq)
{
	int ret;
	/*
	 * TODO:be careful ts bus and ts bat alarm bit mask is in
	 *	fault mask register, so you need call
	 *	sc8541_set_fault_int_mask for tsbus and tsbat alarm
	 */
	ret = sc8541_set_alarm_int_mask(bq, ADC_DONE |
					VBUS_INSERT |
					VBAT_INSERT);
	if (ret) {
		dev_err(bq->dev, "failed to set alarm mask:%d\n", ret);
		return ret;
	}

	return ret;
}

static int sc8541_init_regulation(struct sc8541_charger_info *bq)
{
	sc8541_set_ibat_reg_th(bq, bq->cfg->ibat_reg_th);
	sc8541_set_vbat_reg_th(bq, bq->cfg->vbat_reg_th);

	sc8541_set_vdrop_deglitch(bq, bq->cfg->vdrop_deglitch);
	sc8541_set_vdrop_th(bq, bq->cfg->vdrop_th);

	sc8541_disable_regulation(bq, bq->cfg->regulation_disable);

	return 0;
}

static int sc8541_init_device(struct sc8541_charger_info *bq)
{
	sc8541_reset(bq, false);
	sc8541_enable_wdt(bq, false);

	sc8541_set_ss_timeout(bq, bq->cfg->ss_timeout);
	sc8541_set_sense_resistor(bq, bq->cfg->sense_r_mohm);

	sc8541_init_protection(bq);
	sc8541_init_adc(bq);
	sc8541_init_int_src(bq);

	sc8541_init_regulation(bq);

	return 0;
}

static int sc8541_set_present(struct sc8541_charger_info *bq, bool present)
{
	bq->usb_present = present;

	if (present) {
		sc8541_init_device(bq);
		sc8541_enable_wdt(bq, true);
		sc8541_set_wdt(bq, bq->cfg->wdt_timer);
		schedule_delayed_work(&bq->wdt_work, 0);
	}
	return 0;
}


static ssize_t sc8541_show_registers(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sc8541_charger_info *bq = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8541");
	for (addr = 0x0; addr <= 0x2A; addr++) {
		ret = sc8541_read_byte(bq, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
				       "Reg[%.2X] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t sc8541_store_register(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sc8541_charger_info *bq = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= 0x2A)
		sc8541_write_byte(bq, (unsigned char)reg, (unsigned char)val);

	return count;
}

static DEVICE_ATTR(registers, 0644, sc8541_show_registers, sc8541_store_register);

static struct attribute *sc8541_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group sc8541_attr_group = {
	.attrs = sc8541_attributes,
};

static enum power_supply_property sc8541_charger_props[] = {
	POWER_SUPPLY_PROP_CALIBRATE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
};

static void sc8541_check_alarm_status(struct sc8541_charger_info *bq);
static void sc8541_check_fault_status(struct sc8541_charger_info *bq);

static int sc8541_get_present_status(struct sc8541_charger_info *bq, int *intval)
{
	int ret = 0;
	u8 reg_val;
	bool result = false;

	if (*intval == CM_USB_PRESENT_CMD) {
		result = bq->usb_present;
	} else if (*intval == CM_BATTERY_PRESENT_CMD) {
		ret = sc8541_read_byte(bq, SC8541_REG_15, &reg_val);
		if (!ret)
			bq->batt_present = !!(reg_val & VBAT_INSERT);
		result = bq->batt_present;
	} else if (*intval == CM_VBUS_PRESENT_CMD) {
		ret = sc8541_read_byte(bq, SC8541_REG_15, &reg_val);
		if (!ret)
			bq->vbus_present  = !!(reg_val & VBUS_INSERT);
		result = bq->vbus_present;
	} else {
		dev_err(bq->dev, "get present cmd = %d is error\n", *intval);
	}

	*intval = result;

	return ret;
}

static int sc8541_get_temperature(struct sc8541_charger_info *bq, int *intval)
{
	int ret = 0;
	int result = 0;

	if (*intval == CMD_BATT_TEMP_CMD) {
		ret = sc8541_get_adc_data(bq, ADC_TBAT, &result);
		if (!ret)
			bq->bat_temp = result;
	} else if (*intval == CM_BUS_TEMP_CMD) {
		ret = sc8541_get_adc_data(bq, ADC_TBUS, &result);
		if (!ret)
			bq->bus_temp = result;
	} else if (*intval == CM_DIE_TEMP_CMD) {
		ret = sc8541_get_adc_data(bq, ADC_TDIE, &result);
		if (!ret)
			bq->die_temp = result;
	} else {
		dev_err(bq->dev, "get temperature cmd = %d is error\n", *intval);
	}

	*intval = result;

	return ret;
}

static void sc8541_charger_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sc8541_charger_info *bq = container_of(dwork,
						       struct sc8541_charger_info,
						       wdt_work);

	if (sc8541_set_wdt(bq, bq->cfg->wdt_timer) < 0)
		dev_err(bq->dev, "Fail to feed watchdog\n");
	sc8541_dump_reg(bq);
	schedule_delayed_work(&bq->wdt_work, HZ * 15);
}

static int sc8541_charger_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sc8541_charger_info *bq = power_supply_get_drvdata(psy);
	int result = 0;
	int ret, cmd;
	u8 reg_val;

	if (!bq) {
		pr_err("%s[%d], NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_CALIBRATE:
		sc8541_check_charge_enabled(bq, &bq->charge_enabled);
		val->intval = bq->charge_enabled;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		cmd = val->intval;
		if (!sc8541_get_present_status(bq, &val->intval))
			dev_err(bq->dev, "fail to get present status, cmd = %d\n", cmd);

		break;
	case POWER_SUPPLY_PROP_ONLINE:
		ret = sc8541_read_byte(bq, SC8541_REG_15, &reg_val);
		if (!ret)
			bq->vbus_present  = !!(reg_val & VBUS_INSERT);
		val->intval = bq->vbus_present;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = sc8541_get_adc_data(bq, ADC_VBAT, &result);
		if (!ret)
			bq->vbat_volt = result;

		val->intval = bq->vbat_volt * 1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (val->intval == CM_IBAT_CURRENT_NOW_CMD) {
			ret = sc8541_get_adc_data(bq, ADC_IBAT, &result);
			if (!ret)
				bq->ibat_curr = result;

			val->intval = bq->ibat_curr * 1000;
			break;
		}

		sc8541_check_charge_enabled(bq, &bq->charge_enabled);
		if (!bq->charge_enabled) {
			val->intval = 0;
		} else {
			ret = sc8541_get_adc_data(bq, ADC_IBUS, &result);
			if (!ret)
				bq->ibus_curr = result;
			val->intval = bq->ibus_curr * 1000;
		}
		break;
	case POWER_SUPPLY_PROP_TEMP:
		cmd = val->intval;
		if (sc8541_get_temperature(bq, &val->intval))
			dev_err(bq->dev, "fail to get temperature, cmd = %d\n", cmd);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sc8541_get_adc_data(bq, ADC_VBUS, &result);
		if (!ret)
			bq->vbus_volt = result;

		val->intval = bq->vbus_volt * 1000;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (val->intval == CM_SOFT_ALARM_HEALTH_CMD) {
			val->intval = 0;
			break;
		}

		if (val->intval == CM_BUS_ERR_HEALTH_CMD) {
			sc8541_check_vbus_error_status(bq);
			val->intval = (bq->bus_err_lo  << CM_CHARGER_BUS_ERR_LO_SHIFT);
			val->intval |= (bq->bus_err_hi  << CM_CHARGER_BUS_ERR_HI_SHIFT);
			break;
		}

		sc8541_check_fault_status(bq);
		val->intval = ((bq->bat_ovp_fault << CM_CHARGER_BAT_OVP_FAULT_SHIFT)
			| (bq->bat_ocp_fault << CM_CHARGER_BAT_OCP_FAULT_SHIFT)
			| (bq->bus_ovp_fault << CM_CHARGER_BUS_OVP_FAULT_SHIFT)
			| (bq->bus_ocp_fault << CM_CHARGER_BUS_OCP_FAULT_SHIFT)
			| (bq->bat_therm_fault << CM_CHARGER_BAT_THERM_FAULT_SHIFT)
			| (bq->bus_therm_fault << CM_CHARGER_BUS_THERM_FAULT_SHIFT)
			| (bq->die_therm_fault << CM_CHARGER_DIE_THERM_FAULT_SHIFT));

		sc8541_check_alarm_status(bq);
		val->intval |= ((bq->bat_ovp_alarm << CM_CHARGER_BAT_OVP_ALARM_SHIFT)
			| (bq->bat_ocp_alarm << CM_CHARGER_BAT_OCP_ALARM_SHIFT)
			| (bq->bat_ucp_alarm << CM_CHARGER_BAT_UCP_ALARM_SHIFT)
			| (bq->bus_ovp_alarm << CM_CHARGER_BUS_OVP_ALARM_SHIFT)
			| (bq->bus_ocp_alarm << CM_CHARGER_BUS_OCP_ALARM_SHIFT)
			| (bq->bat_therm_alarm << CM_CHARGER_BAT_THERM_ALARM_SHIFT)
			| (bq->bus_therm_alarm << CM_CHARGER_BUS_THERM_ALARM_SHIFT)
			| (bq->die_therm_alarm << CM_CHARGER_DIE_THERM_ALARM_SHIFT));
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		sc8541_check_charge_enabled(bq, &bq->charge_enabled);
		if (!bq->charge_enabled)
			val->intval = 0;
		else
			val->intval = bq->cfg->bus_ocp_alm_th  * 1000;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		sc8541_check_charge_enabled(bq, &bq->charge_enabled);
		if (!bq->charge_enabled)
			val->intval = 0;
		else
			val->intval = bq->cfg->bat_ocp_alm_th * 1000;
		break;
	default:
		return -EINVAL;

	}

	return 0;
}

static int sc8541_charger_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	struct sc8541_charger_info *bq = power_supply_get_drvdata(psy);
	int ret=0, value;

	if (!bq) {
		pr_err("%s[%d], NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	switch (prop) {
	case POWER_SUPPLY_PROP_CALIBRATE:
		bq->irq_response = !!val->intval;
		if (!val->intval) {
			sc8541_enable_adc(bq, false);
			cancel_delayed_work_sync(&bq->wdt_work);
		}

		ret = sc8541_enable_charge(bq, val->intval);
		if (ret)
			dev_err(bq->dev, "%s, failed to %s charge\n",
				__func__, val->intval ? "enable" : "disable");

		if (sc8541_check_charge_enabled(bq, &bq->charge_enabled))
			dev_err(bq->dev, "%s, failed to check charge enabled\n", __func__);

		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (val->intval == CM_USB_PRESENT_CMD)
			sc8541_set_present(bq, true);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		//ret = sc8541_set_batovp_th(bq, val->intval / 1000);
		if (ret)
			dev_err(bq->dev, "%s, failed to set bat ovp th %d mv, ret = %d\n",
				__func__, val->intval / 1000, ret);

		value = val->intval / 1000 - bq->cfg->bat_delta_volt;
		//ret = sc8541_set_batovp_alarm_th(bq, value);
		if (ret)
			dev_err(bq->dev, "%s, failed to set bat ovp alm th %d mv, ret = %d\n",
				__func__, value, ret);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sc8541_charger_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_CALIBRATE:
	case POWER_SUPPLY_PROP_PRESENT:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int sc8541_psy_register(struct sc8541_charger_info *bq)
{
	bq->psy_cfg.drv_data = bq;
	bq->psy_cfg.of_node = bq->dev->of_node;

	if (bq->mode == SC8541_ROLE_MASTER)
		bq->psy_desc.name = "bq2597x-master";
	else if (bq->mode == SC8541_ROLE_SLAVE)
		bq->psy_desc.name = "bq2597x-slave";
	else
		bq->psy_desc.name = "bq2597x-standalone";

	bq->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	bq->psy_desc.properties = sc8541_charger_props;
	bq->psy_desc.num_properties = ARRAY_SIZE(sc8541_charger_props);
	bq->psy_desc.get_property = sc8541_charger_get_property;
	bq->psy_desc.set_property = sc8541_charger_set_property;
	bq->psy_desc.property_is_writeable = sc8541_charger_is_writeable;


	bq->sc8541_psy = devm_power_supply_register(bq->dev,
						     &bq->psy_desc, &bq->psy_cfg);
	if (IS_ERR(bq->sc8541_psy)) {
		dev_err(bq->dev, "failed to register sc8541_psy\n");
		return PTR_ERR(bq->sc8541_psy);
	}

	dev_info(bq->dev, "%s power supply register successfully\n", bq->psy_desc.name);

	return 0;
}

static void sc8541_dump_reg(struct sc8541_charger_info *bq)
{

	int ret;
	u8 val;
	u8 addr;

	for (addr = 0x00; addr < 0x2F; addr++) {
		ret = sc8541_read_byte(bq, addr, &val);
		if (!ret)
			dev_err(bq->dev, "Reg[%02X] = 0x%02X\n", addr, val);
	}
}

static void sc8541_check_alarm_status(struct sc8541_charger_info *bq)
{
	int ret=0;
	u8 flag[5];
	int s=0;
	u8 stat = 0;
	u8 stat1 = 0;

	mutex_lock(&bq->data_lock);

	/* read to clear alarm flag */
	ret |= sc8541_read_byte(bq, SC8541_REG_1A, &flag[2]);
	s += flag[2];
	ret |= sc8541_read_byte(bq, SC8541_REG_1C, &flag[4]);
	s += flag[4];
		
	if (!ret && s)
		dev_err(bq->dev, "INT_FLAG[%02X] =0x%02X,[%02X] =0x%02X\n", SC8541_REG_1A, flag[2],SC8541_REG_1C,flag[4]);

	ret = sc8541_read_byte(bq, SC8541_REG_13, &stat);
	if (!ret && stat != bq->prev_alarm) {
		dev_err(bq->dev, "INT_STAT[%02X] = 0X%02x\n", SC8541_REG_13, stat);
		bq->prev_alarm = stat;
		bq->bat_ovp_alarm = !!(stat & BAT_OVP_ALARM);
		bq->bat_ocp_alarm = !!(stat & BAT_OCP_ALARM);
		bq->bus_ovp_alarm = !!(stat & BUS_OVP_ALARM);
//		bq->bus_ocp_alarm = !!(stat & BUS_OCP_ALARM);
//		bq->bat_ucp_alarm = !!(stat & BAT_UCP_ALARM);
	}

	ret = sc8541_read_byte(bq, SC8541_REG_15, &stat);
	if (!ret && stat != bq->prev_present) {
		dev_err(bq->dev, "INT_STAT[%s02X] = 0X%02x\n", SC8541_REG_15, stat);
		bq->prev_present = stat;
		bq->batt_present  = !!(stat & VBAT_INSERT);
		bq->vbus_present  = !!(stat & VBUS_INSERT);
	}

	ret = sc8541_read_byte(bq, SC8541_REG_19, &stat);
	ret = sc8541_read_byte(bq, SC8541_REG_40, &stat1);
	if (!ret && ((stat & SC8541_IBUS_UCP_FALL_FLAG_MASK) || (stat1 & SC8541_IBUS_UCP_RISE_FLAG_MASK)))
		dev_err(bq->dev, "Ibus ucp rise or fall event, IBUS_OCP_UCP[%02x] = 0x%02X;[%02x] = 0x%02X;\n",
			SC8541_REG_19, stat,
			SC8541_REG_40, stat1);


	sc8541_dump_reg(bq);
	mutex_unlock(&bq->data_lock);
}

static void sc8541_check_fault_status(struct sc8541_charger_info *bq)
{
	int ret;
	u8 flag = 0;
	u8 stat = 0;

	mutex_lock(&bq->data_lock);
	ret = sc8541_read_byte(bq, SC8541_REG_13, &stat);
	if (!ret && stat)
		dev_err(bq->dev, "FAULT_STAT[%02X] = 0x%02X\n", SC8541_REG_10, stat);

	ret = sc8541_read_byte(bq, SC8541_REG_18, &flag);
	if (!ret && flag)
		dev_err(bq->dev, "FAULT_FLAG[%02X] = 0x%02X\n", SC8541_REG_18, flag);

	if (!ret && flag != bq->prev_fault1) {
		bq->prev_fault1 = flag;
		bq->bat_ovp_fault = !!(flag & BAT_OVP_FAULT);
		bq->bat_ocp_fault = !!(flag & BAT_OCP_FAULT);
		bq->bus_ovp_fault = !!(flag & BUS_OVP_FAULT);
	}

	ret = sc8541_read_byte(bq, SC8541_REG_19, &flag);
	if (!ret && flag)
		dev_err(bq->dev, "FAULT_FLAG[%02X] = 0x%02X\n", SC8541_REG_19, flag);

	if (!ret && flag != bq->prev_fault2) {
		bq->prev_fault2 = flag;
		bq->bus_ocp_fault = !!(flag & BUS_OCP_FAULT);
	}

	ret = sc8541_read_byte(bq, SC8541_REG_1B, &flag);
	if (!ret && flag)
		dev_err(bq->dev, "FAULT_FLAG[%02X] = 0x%02X\n", SC8541_REG_1B, flag);

	if (!ret && flag != bq->prev_fault3) {
		bq->prev_fault3 = flag;
		bq->bat_therm_fault = !!(flag & TS_BAT_FAULT);
		bq->bus_therm_fault = !!(flag & TS_BUS_FAULT);

	//	bq->bat_therm_alarm = !!(flag & TBUS_TBAT_ALARM);
	//	bq->bus_therm_alarm = !!(flag & TBUS_TBAT_ALARM);
	}

	mutex_unlock(&bq->data_lock);
}

/*
 * interrupt does nothing, just info event chagne, other module could get info
 * through power supply interface
 */
static irqreturn_t sc8541_charger_interrupt(int irq, void *dev_id)
{
	struct sc8541_charger_info *bq = dev_id;
	u8 flag = 0;

	if (bq->irq_response) {
		dev_info(bq->dev, "INT OCCURRED\n");
		cm_notify_event(bq->sc8541_psy, CM_EVENT_INT, NULL);
	} else {
		/* purpose: clear interrupt */
		if (sc8541_read_byte(bq, SC8541_REG_18, &flag))
			dev_err(bq->dev, "%s, failed to clear interrupt\n", __func__);
	}

	return IRQ_HANDLED;
}

static void sc8541_determine_initial_status_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sc8541_charger_info *bq = container_of(dwork,
						       struct sc8541_charger_info,
						       det_init_stat_work);

	sc8541_dump_reg(bq);
}

static int show_registers(struct seq_file *m, void *data)
{
	struct sc8541_charger_info *bq = m->private;
	u8 addr;
	int ret;
	u8 val;

	for (addr = 0x0; addr <= 0x2B; addr++) {
		ret = sc8541_read_byte(bq, addr, &val);
		if (!ret)
			seq_printf(m, "Reg[%02X] = 0x%02X\n", addr, val);
	}
	return 0;
}

static int reg_debugfs_open(struct inode *inode, struct file *file)
{
	struct sc8541_charger_info *bq = inode->i_private;

	return single_open(file, show_registers, bq);
}

static const struct file_operations reg_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= reg_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void create_debugfs_entry(struct sc8541_charger_info *bq)
{
	if (bq->mode == SC8541_ROLE_MASTER)
		bq->debug_root = debugfs_create_dir("sc8541-master", NULL);
	else if (bq->mode == SC8541_ROLE_SLAVE)
		bq->debug_root = debugfs_create_dir("sc8541-slave", NULL);
	else
		bq->debug_root = debugfs_create_dir("sc8541-standalone", NULL);

	if (!bq->debug_root)
		dev_err(bq->dev, "Failed to create debug dir\n");

	if (bq->debug_root) {
		debugfs_create_file("registers", 0444, bq->debug_root, bq, &reg_debugfs_ops);

		debugfs_create_x32("skip_reads", 0644, bq->debug_root, &(bq->skip_reads));
		debugfs_create_x32("skip_writes", 0644, bq->debug_root, &(bq->skip_writes));
	}
}

static const struct of_device_id sc8541_charger_match_table[] = {
	{
		.compatible = "sc,sc8541-standalone",
		.data = &sc8541_mode_data[SC8541_STDALONE],
	},
	{
		.compatible = "sc,sc8541-master",
		.data = &sc8541_mode_data[SC8541_MASTER],
	},

	{
		.compatible = "sc,sc8541-slave",
		.data = &sc8541_mode_data[SC8541_SLAVE],
	},
	{},
};

static int sc8541_charger_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct sc8541_charger_info *bq;
	const struct of_device_id *match;
	struct device *dev = &client->dev;
	struct device_node *node = client->dev.of_node;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	bq = devm_kzalloc(dev, sizeof(struct sc8541_charger_info), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	bq->dev = &client->dev;

	bq->client = client;
	i2c_set_clientdata(client, bq);

	mutex_init(&bq->i2c_rw_lock);
	mutex_init(&bq->data_lock);

	bq->resume_completed = true;
	bq->irq_waiting = false;

	ret = sc8541_detect_device(bq);
	if (ret) {
		dev_err(bq->dev, "No sc8541 device found!\n");
		return -ENODEV;
	}

	match = of_match_node(sc8541_charger_match_table, node);
	if (match == NULL) {
		dev_err(bq->dev, "device tree match not found!\n");
		return -ENODEV;
	}

	sc8541_get_work_mode(bq, &bq->mode);

	if (bq->mode !=  *(int *)match->data) {
		dev_err(bq->dev, "device operation mode mismatch with dts configuration\n");
		return -EINVAL;
	}

	ret = sc8541_parse_dt(bq, &client->dev);
	if (ret)
		return -EIO;

	ret = sc8541_init_device(bq);
	if (ret) {
		dev_err(bq->dev, "Failed to init device\n");
		return ret;
	}

	INIT_DELAYED_WORK(&bq->wdt_work, sc8541_charger_watchdog_work);
	INIT_DELAYED_WORK(&bq->det_init_stat_work, sc8541_determine_initial_status_work);
	ret = sc8541_psy_register(bq);
	if (ret)
		return ret;

	if (gpio_is_valid(bq->int_pin)) {
		ret = devm_gpio_request_one(bq->dev, bq->int_pin, GPIOF_DIR_IN, "sc8541_int");
		if (ret) {
			dev_err(bq->dev, "int request failed\n");
			return ret;
		}

		client->irq = gpio_to_irq(bq->int_pin);
	}

	if (client->irq) {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
						NULL, sc8541_charger_interrupt,
						IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						"sc8541 charger irq", bq);
		if (ret < 0) {
			dev_err(bq->dev, "request irq for irq=%d failed, ret =%d\n",
				client->irq, ret);
			return ret;
		}
		enable_irq_wake(client->irq);
	}

	device_init_wakeup(bq->dev, 1);
	create_debugfs_entry(bq);

	ret = sysfs_create_group(&bq->dev->kobj, &sc8541_attr_group);
	if (ret) {
		dev_err(bq->dev, "failed to register sysfs. err: %d\n", ret);
		return ret;
	}

	schedule_delayed_work(&bq->det_init_stat_work, msecs_to_jiffies(100));
	dev_info(bq->dev, "sc8541 probe successfully, Part Num:%d\n!", bq->part_no);

	return 0;
}

static int sc8541_charger_remove(struct i2c_client *client)
{
	struct sc8541_charger_info *bq = i2c_get_clientdata(client);


	sc8541_enable_adc(bq, false);
	cancel_delayed_work_sync(&bq->wdt_work);

	mutex_destroy(&bq->data_lock);
	mutex_destroy(&bq->i2c_rw_lock);

	debugfs_remove_recursive(bq->debug_root);

	sysfs_remove_group(&bq->dev->kobj, &sc8541_attr_group);

	return 0;
}

static void sc8541_charger_shutdown(struct i2c_client *client)
{
	struct sc8541_charger_info *bq = i2c_get_clientdata(client);

	sc8541_enable_adc(bq, false);
	sc8541_enable_charge(bq, false);
	cancel_delayed_work_sync(&bq->wdt_work);
}

static const struct i2c_device_id sc8541_charger_id[] = {
	{"sc8541-standalone", SC8541_ROLE_STDALONE},
	{"sc8541-master", SC8541_ROLE_MASTER},
	{"sc8541-slave", SC8541_ROLE_SLAVE},
	{},
};

static struct i2c_driver sc8541_charger_driver = {
	.driver		= {
		.name	= "sc8541-charger",
		.owner	= THIS_MODULE,
		.of_match_table = sc8541_charger_match_table,
	},
	.id_table	= sc8541_charger_id,
	.probe		= sc8541_charger_probe,
	.remove		= sc8541_charger_remove,
	.shutdown	= sc8541_charger_shutdown,
};

module_i2c_driver(sc8541_charger_driver);

MODULE_DESCRIPTION("TI BQ2597x Charger Driver");
MODULE_LICENSE("GPL v2");
