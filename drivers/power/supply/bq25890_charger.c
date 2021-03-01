/*
 * TI BQ25890 charger driver
 *
 * Copyright (C) 2015 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/acpi.h>
#include <linux/alarmtimer.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power/charger-manager.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/types.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>

#define BQ25890_REG_0				0x0
#define BQ25890_REG_1				0x1
#define BQ25890_REG_2				0x2
#define BQ25890_REG_3				0x3
#define BQ25890_REG_4				0x4
#define BQ25890_REG_5				0x5
#define BQ25890_REG_6				0x6
#define BQ25890_REG_7				0x7
#define BQ25890_REG_8				0x8
#define BQ25890_REG_9				0x9
#define BQ25890_REG_A				0xa
#define BQ25890_REG_B				0xb
#define BQ25890_REG_C				0xc
#define BQ25890_REG_D				0xd
#define BQ25890_REG_E				0xe
#define BQ25890_REG_F				0xf
#define BQ25890_REG_10				0x10
#define BQ25890_REG_11				0x11
#define BQ25890_REG_12				0x12
#define BQ25890_REG_13				0x13
#define BQ25890_REG_14				0x14
#define BQ25890_REG_NUM				21

#define BQ25890_BATTERY_NAME			"sc27xx-fgu"
#define BIT_DP_DM_BC_ENB			BIT(0)

/* set limit current register */
#define BQ25890_REG_ICHG_MIN			0
#define BQ25890_REG_ICHG_MAX			5056
#define BQ25890_REG_ICHG_STEP			64
#define BQ25890_REG_ICHG_MASK			GENMASK(6, 0)

/* set terminal current register */
#define BQ25890_REG_TERMINAL_CUR_MIN		64
#define BQ25890_REG_TERMINAL_CUR_MAX		1024
#define BQ25890_REG_TERMINAL_CUR_STEP		64
#define BQ25890_REG_TERMINAL_CUR_OFFSET		64
#define BQ25890_REG_TERMINAL_CUR_MASK		GENMASK(3, 0)

/* set terminal voltage register*/
#define BQ25890_REG_TERMINAL_VOLTAGE_MIN	3840
#define BQ25890_REG_TERMINAL_VOLTAGE_MAX	4608
#define BQ25890_REG_TERMINAL_VOLTAGE_STEP	16
#define BQ25890_REG_TERMINAL_VOLTAGE_OFFSET	3840
#define BQ25890_REG_TERMINAL_VOLTAGE_SHIFT	2
#define BQ25890_REG_TERMINAL_VOLTAGE_MASK	GENMASK(7, 2)

/* set vindpm register */
#define BQ25890_REG_VINDPM_MIN			3900
#define BQ25890_REG_VINDPM_MAX			15300
#define BQ25890_REG_VINDPM_STEP			100
#define BQ25890_REG_VINDPM_OFFSET		2600
#define BQ25890_REG_VINDPM_FORCE_ENABLE		0x1
#define BQ25890_REG_VINDPM_FORCE_MASK		GENMASK(7, 7)
#define BQ25890_REG_VINDPM_VOLTAGE_MASK		GENMASK(6, 0)

/* set input limit current register */
#define BQ25890_REG_LIMIT_CURRENT_MIN		100
#define BQ25890_REG_LIMIT_CURRENT_MAX		3250
#define BQ25890_REG_LIMIT_CURRENT_STEP		50
#define BQ25890_REG_LIMIT_CURRENT_OFFSET	100
#define BQ25890_REG_LIMIT_CURRENT_MASK		GENMASK(5, 0)

/* enable hiz related definition */
#define BQ25890_REG_EN_HIZ_SHIFT		7
#define BQ25890_REG_EN_HIZ_MASK			GENMASK(7, 7)

/* set watchdog register */
#define BQ25890_FEED_WATCHDOG_VALID_MS		50
#define BQ25890_REG_WATCHDOG_TIMER_DISABLE	0x00
#define BQ25890_REG_WATCHDOG_TIMER_ENABLE	0x01
#define BQ25890_REG_WATCHDOG_ENABLE		0x1
#define BQ25890_REG_WATCHDOG_MASK		GENMASK(6, 6)
#define BQ25890_REG_WATCHDOG_TIMER_MASK		GENMASK(5, 4)

/* otg related definition */
#define BQ25890_OTG_ALARM_TIMER_MS		15000
#define BQ25890_OTG_VALID_MS			500
#define BQ25890_OTG_RETRY_TIMES			10
#define BQ25890_REG_OTG_DISABLE			0x0
#define BQ25890_REG_OTG_ENABLE			0x1
#define BQ25890_REG_OTG_MASK			GENMASK(5, 5)

/* reset register definition */
#define BQ25890_REG_RESET_MASK			GENMASK(7, 7)

#define BQ25890_DISABLE_PIN_MASK		BIT(0)
#define BQ25890_DISABLE_PIN_MASK_2721		BIT(15)

#define BQ25890_FAST_CHARGER_VOLTAGE_MAX	10500000
#define BQ25890_NORMAL_CHARGER_VOLTAGE_MAX	6500000

#define BQ25890_WAKE_UP_MS			2000

struct bq25890_charger_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_bq25890_dump_reg;
	struct device_attribute attr_bq25890_lookup_reg;
	struct device_attribute attr_bq25890_sel_reg_id;
	struct device_attribute attr_bq25890_reg_val;
	struct attribute *attrs[5];

	struct bq25890_charger_info *info;
};

struct bq25890_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct power_supply *psy_usb;
	struct power_supply_charge_current cur;
	struct work_struct work;
	struct mutex lock;
	bool charging;
	u32 limit;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	struct regmap *pmic;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	struct gpio_desc *gpiod;
	struct extcon_dev *edev;
	u32 last_limit_current;
	u32 role;
	bool need_disable_Q1;
	int termination_cur;
	int vol_max_mv;
	u32 actual_limit_current;
	bool otg_enable;
	struct alarm otg_timer;
	struct bq25890_charger_sysfs *sysfs;
	int reg_id;
};

struct bq25890_charger_reg_tab {
	int id;
	u32 addr;
	char *name;
};

static struct bq25890_charger_reg_tab reg_tab[BQ25890_REG_NUM + 1] = {
	{0, BQ25890_REG_0, "Setting Input Limit Current reg"},
	{1, BQ25890_REG_1, "Setting Vindpm_OS reg"},
	{2, BQ25890_REG_2, "Related Function Enable reg"},
	{3, BQ25890_REG_3, "Related Function Config reg"},
	{4, BQ25890_REG_4, "Setting Charge Limit Current reg"},
	{5, BQ25890_REG_5, "Setting Terminal Current reg"},
	{6, BQ25890_REG_6, "Setting Charge Limit Voltage reg"},
	{7, BQ25890_REG_7, "Related Function Config reg"},
	{8, BQ25890_REG_8, "IR Compensation Resistor Setting reg"},
	{9, BQ25890_REG_9, "Related Function Config reg"},
	{10, BQ25890_REG_A, "Boost Mode Related Setting reg"},
	{11, BQ25890_REG_B, "Status reg"},
	{12, BQ25890_REG_C, "Fault reg"},
	{13, BQ25890_REG_D, "Setting Vindpm reg"},
	{14, BQ25890_REG_E, "ADC Conversion of Battery Voltage reg"},
	{15, BQ25890_REG_F, "ADDC Conversion of System Voltage reg"},
	{16, BQ25890_REG_10, "ADC Conversion of TS Voltage as Percentage of REGN reg"},
	{17, BQ25890_REG_11, "ADC Conversion of VBUS voltage reg"},
	{18, BQ25890_REG_12, "ICHGR Setting reg"},
	{19, BQ25890_REG_13, "IDPM Limit Setting reg"},
	{20, BQ25890_REG_14, "Related Function Config reg"},
	{21, 0, "null"},
};

static int bq25890_charger_set_limit_current(struct bq25890_charger_info *info,
					     u32 limit_cur);

static int bq25890_read(struct bq25890_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int bq25890_write(struct bq25890_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int bq25890_update_bits(struct bq25890_charger_info *info, u8 reg,
			       u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = bq25890_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return bq25890_write(info, reg, v);
}

static void bq25890_charger_dump_register(struct bq25890_charger_info *info)
{
	int i, ret, len, idx = 0;
	u8 reg_val;
	char buf[512];

	memset(buf, '\0', sizeof(buf));
	for (i = 0; i < BQ25890_REG_NUM; i++) {
		ret = bq25890_read(info, reg_tab[i].addr, &reg_val);
		if (ret == 0) {
			len = snprintf(buf + idx, sizeof(buf) - idx,
				       "[REG_0x%.2x]=0x%.2x; ", reg_tab[i].addr,
				       reg_val);
			idx += len;
		}
	}

	dev_info(info->dev, "%s: %s", __func__, buf);
}

static bool bq25890_charger_is_bat_present(struct bq25890_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(BQ25890_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return present;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&val);
	if (!ret && val.intval)
		present = true;
	power_supply_put(psy);

	if (ret)
		dev_err(info->dev, "Failed to get property of present:%d\n", ret);

	return present;
}

static int bq25890_charger_is_fgu_present(struct bq25890_charger_info *info)
{
	struct power_supply *psy;

	psy = power_supply_get_by_name(BQ25890_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to find psy of sc27xx_fgu\n");
		return -ENODEV;
	}
	power_supply_put(psy);

	return 0;
}

static int bq25890_charger_set_vindpm(struct bq25890_charger_info *info, u32 vol)
{
	u8 reg_val;
	int ret;

	ret = bq25890_update_bits(info, BQ25890_REG_D,
				  BQ25890_REG_VINDPM_FORCE_MASK,
				  BQ25890_REG_VINDPM_FORCE_ENABLE);
	if (ret) {
		dev_err(info->dev, "set force vindpm failed\n");
		return ret;
	}

	if (vol < BQ25890_REG_VINDPM_MIN)
		vol = BQ25890_REG_VINDPM_MIN;
	else if (vol > BQ25890_REG_VINDPM_MAX)
		vol = BQ25890_REG_VINDPM_MAX;
	reg_val = (vol - BQ25890_REG_VINDPM_OFFSET) / BQ25890_REG_VINDPM_STEP;

	return bq25890_update_bits(info, BQ25890_REG_D,
				   BQ25890_REG_VINDPM_VOLTAGE_MASK, reg_val);
}

static int bq25890_charger_set_termina_vol(struct bq25890_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < BQ25890_REG_TERMINAL_VOLTAGE_MIN)
		vol = BQ25890_REG_TERMINAL_VOLTAGE_MIN;
	else if (vol >= BQ25890_REG_TERMINAL_VOLTAGE_MAX)
		vol = BQ25890_REG_TERMINAL_VOLTAGE_MAX;
	reg_val = (vol - BQ25890_REG_TERMINAL_VOLTAGE_OFFSET) /
		BQ25890_REG_TERMINAL_VOLTAGE_STEP;

	return bq25890_update_bits(info, BQ25890_REG_6,
				   BQ25890_REG_TERMINAL_VOLTAGE_MASK,
				   reg_val << BQ25890_REG_TERMINAL_VOLTAGE_SHIFT);
}

static int bq25890_charger_set_termina_cur(struct bq25890_charger_info *info, u32 cur)
{
	u8 reg_val;

	if (cur <= BQ25890_REG_TERMINAL_CUR_MIN)
		cur = BQ25890_REG_TERMINAL_CUR_MIN;
	else if (cur >= BQ25890_REG_TERMINAL_CUR_MAX)
		cur = BQ25890_REG_TERMINAL_CUR_MAX;
	reg_val = (cur - BQ25890_REG_TERMINAL_CUR_OFFSET) /
		BQ25890_REG_TERMINAL_CUR_STEP;

	return bq25890_update_bits(info, BQ25890_REG_5,
				   BQ25890_REG_TERMINAL_CUR_MASK,
				   reg_val);
}

static int bq25890_charger_hw_init(struct bq25890_charger_info *info)
{
	struct power_supply_battery_info bat_info;
	int ret;

	ret = power_supply_get_battery_info(info->psy_usb, &bat_info, 0);
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");

		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 100 mA, and default
		 * charge termination voltage to 4.2V.
		 */
		info->cur.sdp_limit = 500000;
		info->cur.sdp_cur = 500000;
		info->cur.dcp_limit = 5000000;
		info->cur.dcp_cur = 500000;
		info->cur.cdp_limit = 5000000;
		info->cur.cdp_cur = 1500000;
		info->cur.unknown_limit = 5000000;
		info->cur.unknown_cur = 500000;
	} else {
		info->cur.sdp_limit = bat_info.cur.sdp_limit;
		info->cur.sdp_cur = bat_info.cur.sdp_cur;
		info->cur.dcp_limit = bat_info.cur.dcp_limit;
		info->cur.dcp_cur = bat_info.cur.dcp_cur;
		info->cur.cdp_limit = bat_info.cur.cdp_limit;
		info->cur.cdp_cur = bat_info.cur.cdp_cur;
		info->cur.unknown_limit = bat_info.cur.unknown_limit;
		info->cur.unknown_cur = bat_info.cur.unknown_cur;
		info->cur.fchg_limit = bat_info.cur.fchg_limit;
		info->cur.fchg_cur = bat_info.cur.fchg_cur;

		info->vol_max_mv = bat_info.constant_charge_voltage_max_uv / 1000;
		info->termination_cur = bat_info.charge_term_current_ua / 1000;
		power_supply_put_battery_info(info->psy_usb, &bat_info);

		ret = bq25890_update_bits(info, BQ25890_REG_14,
					  BQ25890_REG_RESET_MASK,
					  BQ25890_REG_RESET_MASK);
		if (ret) {
			dev_err(info->dev, "reset bq25890 failed\n");
			return ret;
		}

		ret = bq25890_charger_set_vindpm(info, info->vol_max_mv);
		if (ret) {
			dev_err(info->dev, "set bq25890 vindpm vol failed\n");
			return ret;
		}

		ret = bq25890_charger_set_termina_vol(info,
						      info->vol_max_mv);
		if (ret) {
			dev_err(info->dev, "set bq25890 terminal vol failed\n");
			return ret;
		}

		ret = bq25890_charger_set_termina_cur(info, info->termination_cur);
		if (ret) {
			dev_err(info->dev, "set bq25890 terminal cur failed\n");
			return ret;
		}

		ret = bq25890_charger_set_limit_current(info,
							info->cur.unknown_cur);
		if (ret)
			dev_err(info->dev, "set bq25890 limit current failed\n");
	}

	return ret;
}

static int bq25890_charger_get_charge_voltage(struct bq25890_charger_info *info,
					      u32 *charge_vol)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(BQ25890_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "failed to get BQ25890_BATTERY_NAME\n");
		return -ENODEV;
	}

	ret = power_supply_get_property(psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	power_supply_put(psy);
	if (ret) {
		dev_err(info->dev, "failed to get CONSTANT_CHARGE_VOLTAGE\n");
		return ret;
	}

	*charge_vol = val.intval;

	return 0;
}

static int bq25890_charger_start_charge(struct bq25890_charger_info *info)
{
	int ret;

	ret = bq25890_update_bits(info, BQ25890_REG_0,
				  BQ25890_REG_EN_HIZ_MASK, 0);
	if (ret)
		dev_err(info->dev, "disable HIZ mode failed\n");

	ret = bq25890_update_bits(info, BQ25890_REG_7,
				  BQ25890_REG_WATCHDOG_TIMER_MASK,
				  BQ25890_REG_WATCHDOG_TIMER_ENABLE);
	if (ret) {
		dev_err(info->dev, "Failed to enable bq25890 watchdog\n");
		return ret;
	}

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask, 0);
	if (ret) {
		dev_err(info->dev, "enable bq25890 charge failed\n");
			return ret;
		}

	ret = bq25890_charger_set_limit_current(info,
						info->last_limit_current);
	if (ret) {
		dev_err(info->dev, "failed to set limit current\n");
		return ret;
	}

	ret = bq25890_charger_set_termina_cur(info, info->termination_cur);
	if (ret)
		dev_err(info->dev, "set bq25890 terminal cur failed\n");

	return ret;
}

static void bq25890_charger_stop_charge(struct bq25890_charger_info *info)
{
	int ret;
	bool present = bq25890_charger_is_bat_present(info);

	if (!present || info->need_disable_Q1) {
		ret = bq25890_update_bits(info, BQ25890_REG_0,
					  BQ25890_REG_EN_HIZ_MASK,
					  0x01 << BQ25890_REG_EN_HIZ_SHIFT);
		if (ret)
			dev_err(info->dev, "enable HIZ mode failed\n");
		info->need_disable_Q1 = false;
	}

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask,
				 info->charger_pd_mask);
	if (ret)
		dev_err(info->dev, "disable bq25890 charge failed\n");

	ret = bq25890_update_bits(info, BQ25890_REG_7,
				  BQ25890_REG_WATCHDOG_TIMER_MASK,
				  BQ25890_REG_WATCHDOG_TIMER_DISABLE);
	if (ret)
		dev_err(info->dev, "Failed to disable bq25890 watchdog\n");

}

static int bq25890_charger_set_current(struct bq25890_charger_info *info,
				       u32 cur)
{
	u8 reg_val;
	int ret;

	cur = cur / 1000;
	if (cur <= BQ25890_REG_ICHG_MIN)
		cur = BQ25890_REG_ICHG_MIN;
	else if (cur >= BQ25890_REG_ICHG_MAX)
		cur = BQ25890_REG_ICHG_MAX;
	reg_val = cur / BQ25890_REG_ICHG_STEP;

	ret = bq25890_update_bits(info, BQ25890_REG_4,
				  BQ25890_REG_ICHG_MASK,
				  reg_val);

	return ret;
}

static int bq25890_charger_get_current(struct bq25890_charger_info *info,
				       u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = bq25890_read(info, BQ25890_REG_4, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= BQ25890_REG_ICHG_MASK;
	*cur = reg_val * BQ25890_REG_ICHG_STEP * 1000;

	return 0;
}

static int bq25890_charger_set_limit_current(struct bq25890_charger_info *info,
					     u32 limit_cur)
{
	u8 reg_val;
	int ret;

	limit_cur = limit_cur / 1000;
	if (limit_cur >= BQ25890_REG_LIMIT_CURRENT_MAX)
		limit_cur = BQ25890_REG_LIMIT_CURRENT_MAX;
	if (limit_cur <= BQ25890_REG_LIMIT_CURRENT_MIN)
		limit_cur = BQ25890_REG_LIMIT_CURRENT_MIN;

	info->last_limit_current = limit_cur * 1000;
	reg_val = (limit_cur - BQ25890_REG_LIMIT_CURRENT_OFFSET) /
		BQ25890_REG_LIMIT_CURRENT_STEP;

	ret = bq25890_update_bits(info, BQ25890_REG_0,
				  BQ25890_REG_LIMIT_CURRENT_MASK,
				  reg_val);
	if (ret)
		dev_err(info->dev, "set bq25890 limit cur failed\n");

	info->actual_limit_current =
		(reg_val * BQ25890_REG_LIMIT_CURRENT_STEP +
		 BQ25890_REG_LIMIT_CURRENT_OFFSET) * 1000;

	return ret;
}

static u32 bq25890_charger_get_limit_current(struct bq25890_charger_info *info,
					     u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = bq25890_read(info, BQ25890_REG_0, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= BQ25890_REG_LIMIT_CURRENT_MASK;
	*limit_cur = reg_val * BQ25890_REG_LIMIT_CURRENT_STEP +
		BQ25890_REG_LIMIT_CURRENT_OFFSET;
	if (*limit_cur >= BQ25890_REG_LIMIT_CURRENT_MAX)
		*limit_cur = BQ25890_REG_LIMIT_CURRENT_MAX * 1000;
	else
		*limit_cur = *limit_cur * 1000;

	return 0;
}

static inline int bq25890_charger_get_health(struct bq25890_charger_info *info,
				      u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static inline int bq25890_charger_get_online(struct bq25890_charger_info *info,
				      u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}

static int bq25890_charger_feed_watchdog(struct bq25890_charger_info *info,
					 u32 val)
{
	int ret;
	u32 limit_cur = 0;

	ret = bq25890_update_bits(info, BQ25890_REG_3,
				  BQ25890_REG_WATCHDOG_MASK,
				  BQ25890_REG_WATCHDOG_ENABLE);
	if (ret) {
		dev_err(info->dev, "reset bq25890 failed\n");
		return ret;
	}

	ret = bq25890_charger_get_limit_current(info, &limit_cur);
	if (ret) {
		dev_err(info->dev, "get limit cur failed\n");
		return ret;
	}

	if (info->actual_limit_current == limit_cur)
		return 0;

	ret = bq25890_charger_set_limit_current(info, info->actual_limit_current);
	if (ret) {
		dev_err(info->dev, "set limit cur failed\n");
		return ret;
	}

	return 0;
}

static int bq25890_charger_set_fchg_current(struct bq25890_charger_info *info,
					    u32 val)
{
	int ret, limit_cur, cur;

	if (val == CM_FAST_CHARGE_ENABLE_CMD) {
		limit_cur = info->cur.fchg_limit;
		cur = info->cur.fchg_cur;
	} else if (val == CM_FAST_CHARGE_DISABLE_CMD) {
		limit_cur = info->cur.dcp_limit;
		cur = info->cur.dcp_cur;
	} else {
		return 0;
	}

	ret = bq25890_charger_set_limit_current(info, limit_cur);
	if (ret) {
		dev_err(info->dev, "failed to set fchg limit current\n");
		return ret;
	}

	ret = bq25890_charger_set_current(info, cur);
	if (ret) {
		dev_err(info->dev, "failed to set fchg current\n");
		return ret;
	}

	return 0;
}

static inline int bq25890_charger_get_status(struct bq25890_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int bq25890_charger_set_status(struct bq25890_charger_info *info,
				      int val)
{
	int ret = 0;
	u32 input_vol;

	if (val == CM_FAST_CHARGE_ENABLE_CMD) {
		ret = bq25890_charger_set_fchg_current(info, val);
		if (ret) {
			dev_err(info->dev, "failed to set 9V fast charge current\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_DISABLE_CMD) {
		ret = bq25890_charger_set_fchg_current(info, val);
		if (ret) {
			dev_err(info->dev, "failed to set 5V normal charge current\n");
			return ret;
		}
		ret = bq25890_charger_get_charge_voltage(info, &input_vol);
		if (ret) {
			dev_err(info->dev, "failed to get 9V charge voltage\n");
			return ret;
		}
		if (input_vol > BQ25890_FAST_CHARGER_VOLTAGE_MAX)
			info->need_disable_Q1 = true;
	} else if (val == false) {
		ret = bq25890_charger_get_charge_voltage(info, &input_vol);
		if (ret) {
			dev_err(info->dev, "failed to get 5V charge voltage\n");
			return ret;
		}
		if (input_vol > BQ25890_NORMAL_CHARGER_VOLTAGE_MAX)
			info->need_disable_Q1 = true;
	}

	if (val > CM_FAST_CHARGE_NORMAL_CMD)
		return 0;

	if (!val && info->charging) {
		bq25890_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = bq25890_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static void bq25890_charger_work(struct work_struct *data)
{
	struct bq25890_charger_info *info =
		container_of(data, struct bq25890_charger_info, work);
	int limit_cur, cur, ret;
	bool present;

	present = bq25890_charger_is_bat_present(info);

	mutex_lock(&info->lock);

	if (info->limit > 0 && present) {
		/* set current limitation and start to charge */
		switch (info->usb_phy->chg_type) {
		case SDP_TYPE:
			limit_cur = info->cur.sdp_limit;
			cur = info->cur.sdp_cur;
			break;
		case DCP_TYPE:
			limit_cur = info->cur.dcp_limit;
			cur = info->cur.dcp_cur;
			break;
		case CDP_TYPE:
			limit_cur = info->cur.cdp_limit;
			cur = info->cur.cdp_cur;
			break;
		default:
			limit_cur = info->cur.unknown_limit;
			cur = info->cur.unknown_cur;
		}

		ret = bq25890_charger_set_limit_current(info, limit_cur);
		if (ret)
			goto out;

		ret = bq25890_charger_set_current(info, cur);
		if (ret)
			goto out;

	} else if ((!info->limit && info->charging) || !present) {
		/* Stop charging */
		info->charging = false;
		bq25890_charger_stop_charge(info);
	}

out:
	mutex_unlock(&info->lock);
	dev_info(info->dev, "battery present = %d, charger type = %d\n",
		 present, info->usb_phy->chg_type);
	cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}

static ssize_t bq25890_reg_val_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct bq25890_charger_sysfs *bq25890_sysfs =
		container_of(attr, struct bq25890_charger_sysfs,
			     attr_bq25890_reg_val);
	struct bq25890_charger_info *info = bq25890_sysfs->info;
	u8 val;
	int ret;

	if (!info)
		return sprintf(buf, "%s bq25890_sysfs->info is null\n", __func__);

	ret = bq25890_read(info, reg_tab[info->reg_id].addr, &val);
	if (ret) {
		dev_err(info->dev, "fail to get bq25890_REG_0x%.2x value, ret = %d\n",
			reg_tab[info->reg_id].addr, ret);
		return sprintf(buf, "fail to get bq25890_REG_0x%.2x value\n",
			       reg_tab[info->reg_id].addr);
	}

	return sprintf(buf, "bq25890_REG_0x%.2x = 0x%.2x\n",
		       reg_tab[info->reg_id].addr, val);
}

static ssize_t bq25890_reg_val_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct bq25890_charger_sysfs *bq25890_sysfs =
		container_of(attr, struct bq25890_charger_sysfs,
			     attr_bq25890_reg_val);
	struct bq25890_charger_info *info = bq25890_sysfs->info;
	u8 val;
	int ret;

	if (!info) {
		dev_err(dev, "%s bq25890_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtou8(buf, 16, &val);
	if (ret) {
		dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	ret = bq25890_write(info, reg_tab[info->reg_id].addr, val);
	if (ret) {
		dev_err(info->dev, "fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
				val, reg_tab[info->reg_id].addr, ret);
		return count;
	}

	dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n", val,
		 reg_tab[info->reg_id].addr);
	return count;
}

static ssize_t bq25890_reg_id_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct bq25890_charger_sysfs *bq25890_sysfs =
		container_of(attr, struct bq25890_charger_sysfs,
			     attr_bq25890_sel_reg_id);
	struct bq25890_charger_info *info = bq25890_sysfs->info;
	int ret, id;

	if (!info) {
		dev_err(dev, "%s bq25890_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtoint(buf, 10, &id);
	if (ret) {
		dev_err(info->dev, "%s store register id fail\n", bq25890_sysfs->name);
		return count;
	}

	if (id < 0 || id >= BQ25890_REG_NUM) {
		dev_err(info->dev, "%s store register id fail, id = %d is out of range\n",
			bq25890_sysfs->name, id);
		return count;
	}

	info->reg_id = id;

	dev_info(info->dev, "%s store register id = %d success\n", bq25890_sysfs->name, id);
	return count;
}

static ssize_t bq25890_reg_id_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct bq25890_charger_sysfs *bq25890_sysfs =
		container_of(attr, struct bq25890_charger_sysfs,
			     attr_bq25890_sel_reg_id);
	struct bq25890_charger_info *info = bq25890_sysfs->info;

	if (!info)
		return sprintf(buf, "%s bq25890_sysfs->info is null\n", __func__);

	return sprintf(buf, "Cuurent register id = %d\n", info->reg_id);
}

static ssize_t bq25890_reg_table_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct bq25890_charger_sysfs *bq25890_sysfs =
		container_of(attr, struct bq25890_charger_sysfs,
			     attr_bq25890_lookup_reg);
	struct bq25890_charger_info *info = bq25890_sysfs->info;
	int i, len, idx = 0;
	char reg_tab_buf[2048];

	if (!info)
		return sprintf(buf, "%s bq25890_sysfs->info is null\n", __func__);

	memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
	len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
		       "Format: [id] [addr] [desc]\n");
	idx += len;

	for (i = 0; i < BQ25890_REG_NUM; i++) {
		len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
			       "[%d] [REG_0x%.2x] [%s]; \n",
			       reg_tab[i].id, reg_tab[i].addr, reg_tab[i].name);
		idx += len;
	}

	return sprintf(buf, "%s\n", reg_tab_buf);
}

static ssize_t bq25890_dump_reg_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct bq25890_charger_sysfs *bq25890_sysfs =
		container_of(attr, struct bq25890_charger_sysfs,
			     attr_bq25890_dump_reg);
	struct bq25890_charger_info *info = bq25890_sysfs->info;

	if (!info)
		return sprintf(buf, "%s bq25890_sysfs->info is null\n", __func__);

	bq25890_charger_dump_register(info);

	return sprintf(buf, "%s\n", bq25890_sysfs->name);
}

static int bq25890_register_sysfs(struct bq25890_charger_info *info)
{
	struct bq25890_charger_sysfs *bq25890_sysfs;
	int ret;

	bq25890_sysfs = devm_kzalloc(info->dev, sizeof(*bq25890_sysfs), GFP_KERNEL);
	if (!bq25890_sysfs)
		return -ENOMEM;

	info->sysfs = bq25890_sysfs;
	bq25890_sysfs->name = "bq25890_sysfs";
	bq25890_sysfs->info = info;
	bq25890_sysfs->attrs[0] = &bq25890_sysfs->attr_bq25890_dump_reg.attr;
	bq25890_sysfs->attrs[1] = &bq25890_sysfs->attr_bq25890_lookup_reg.attr;
	bq25890_sysfs->attrs[2] = &bq25890_sysfs->attr_bq25890_sel_reg_id.attr;
	bq25890_sysfs->attrs[3] = &bq25890_sysfs->attr_bq25890_reg_val.attr;
	bq25890_sysfs->attrs[4] = NULL;
	bq25890_sysfs->attr_g.name = "debug";
	bq25890_sysfs->attr_g.attrs = bq25890_sysfs->attrs;

	sysfs_attr_init(&bq25890_sysfs->attr_bq25890_dump_reg.attr);
	bq25890_sysfs->attr_bq25890_dump_reg.attr.name = "bq25890_dump_reg";
	bq25890_sysfs->attr_bq25890_dump_reg.attr.mode = 0444;
	bq25890_sysfs->attr_bq25890_dump_reg.show = bq25890_dump_reg_show;

	sysfs_attr_init(&bq25890_sysfs->attr_bq25890_lookup_reg.attr);
	bq25890_sysfs->attr_bq25890_lookup_reg.attr.name = "bq25890_lookup_reg";
	bq25890_sysfs->attr_bq25890_lookup_reg.attr.mode = 0444;
	bq25890_sysfs->attr_bq25890_lookup_reg.show = bq25890_reg_table_show;

	sysfs_attr_init(&bq25890_sysfs->attr_bq25890_sel_reg_id.attr);
	bq25890_sysfs->attr_bq25890_sel_reg_id.attr.name = "bq25890_sel_reg_id";
	bq25890_sysfs->attr_bq25890_sel_reg_id.attr.mode = 0644;
	bq25890_sysfs->attr_bq25890_sel_reg_id.show = bq25890_reg_id_show;
	bq25890_sysfs->attr_bq25890_sel_reg_id.store = bq25890_reg_id_store;

	sysfs_attr_init(&bq25890_sysfs->attr_bq25890_reg_val.attr);
	bq25890_sysfs->attr_bq25890_reg_val.attr.name = "bq25890_reg_val";
	bq25890_sysfs->attr_bq25890_reg_val.attr.mode = 0644;
	bq25890_sysfs->attr_bq25890_reg_val.show = bq25890_reg_val_show;
	bq25890_sysfs->attr_bq25890_reg_val.store = bq25890_reg_val_store;

	ret = sysfs_create_group(&info->psy_usb->dev.kobj, &bq25890_sysfs->attr_g);
	if (ret < 0)
		dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static int bq25890_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct bq25890_charger_info *info =
		container_of(nb, struct bq25890_charger_info, usb_notify);

	info->limit = limit;

	pm_wakeup_event(info->dev, BQ25890_WAKE_UP_MS);

	schedule_work(&info->work);
	return NOTIFY_OK;
}

static int bq25890_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{
	struct bq25890_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, online, health, enabled = 0;
	enum usb_charger_type type;
	int ret = 0;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->limit)
			val->intval = bq25890_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = bq25890_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = bq25890_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = bq25890_charger_get_online(info, &online);
		if (ret)
			goto out;

		val->intval = online;

		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = bq25890_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;

	case POWER_SUPPLY_PROP_USB_TYPE:
		type = info->usb_phy->chg_type;

		switch (type) {
		case SDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_SDP;
			break;

		case DCP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
			break;

		case CDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_CDP;
			break;

		default:
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}

		break;

	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		ret = regmap_read(info->pmic, info->charger_pd, &enabled);
		if (ret) {
			dev_err(info->dev, "get bq25890 charge status failed\n");
			goto out;
		}

		val->intval = !enabled;
		break;
	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int bq25890_charger_usb_set_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    const union power_supply_propval *val)
{
	struct bq25890_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = bq25890_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = bq25890_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_STATUS:
		ret = bq25890_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;

	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		ret = bq25890_charger_feed_watchdog(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "feed charger watchdog failed\n");
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = bq25890_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;

	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		if (val->intval == true) {
			ret = bq25890_charger_start_charge(info);
			if (ret)
				dev_err(info->dev, "start charge failed\n");
		} else if (val->intval == false) {
			bq25890_charger_stop_charge(info);
		}
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int bq25890_charger_property_is_writeable(struct power_supply *psy,
						 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_usb_type bq25890_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static enum power_supply_property bq25890_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CHARGE_ENABLED,
};

static const struct power_supply_desc bq25890_charger_desc = {
	.name			= "bq25890_charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= bq25890_usb_props,
	.num_properties		= ARRAY_SIZE(bq25890_usb_props),
	.get_property		= bq25890_charger_usb_get_property,
	.set_property		= bq25890_charger_usb_set_property,
	.property_is_writeable	= bq25890_charger_property_is_writeable,
	.usb_types		= bq25890_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(bq25890_charger_usb_types),
};

static void bq25890_charger_detect_status(struct bq25890_charger_info *info)
{
	unsigned int min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;

	usb_phy_get_charger_current(info->usb_phy, &min, &max);
	info->limit = min;

	/*
	 * slave no need to start charge when vbus change.
	 * due to charging in shut down will check each psy
	 * whether online or not, so let info->limit = min.
	 */
	schedule_work(&info->work);
}

static void bq25890_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct bq25890_charger_info *info = container_of(dwork,
							 struct bq25890_charger_info,
							 wdt_work);
	int ret;

	ret = bq25890_update_bits(info, BQ25890_REG_3,
				  BQ25890_REG_WATCHDOG_MASK,
				  BQ25890_REG_WATCHDOG_ENABLE);
	if (ret) {
		dev_err(info->dev, "reset bq25890 failed\n");
		return;
	}
	schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#ifdef CONFIG_REGULATOR
static void bq25890_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct bq25890_charger_info *info = container_of(dwork,
			struct bq25890_charger_info, otg_work);
	bool otg_valid = extcon_get_state(info->edev, EXTCON_USB);
	int ret, retry = 0;

	if (otg_valid)
		goto out;

	do {
		ret = bq25890_update_bits(info, BQ25890_REG_3,
					  BQ25890_REG_OTG_MASK,
					  BQ25890_REG_OTG_ENABLE);
		if (ret)
			dev_err(info->dev, "restart bq25890 charger otg failed\n");

		otg_valid = extcon_get_state(info->edev, EXTCON_USB);
	} while (!otg_valid && retry++ < BQ25890_OTG_RETRY_TIMES);

	if (retry >= BQ25890_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int bq25890_charger_enable_otg(struct regulator_dev *dev)
{
	struct bq25890_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
	if (ret) {
		dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
		return ret;
	}

	ret = bq25890_update_bits(info, BQ25890_REG_3,
				  BQ25890_REG_OTG_MASK,
				  BQ25890_REG_OTG_ENABLE);

	if (ret) {
		dev_err(info->dev, "enable bq25890 otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect,
				   BIT_DP_DM_BC_ENB, 0);
		return ret;
	}

	info->otg_enable = true;
	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(BQ25890_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(BQ25890_OTG_VALID_MS));

	return 0;
}

static int bq25890_charger_disable_otg(struct regulator_dev *dev)
{
	struct bq25890_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	info->otg_enable = false;
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	ret = bq25890_update_bits(info, BQ25890_REG_3,
				  BQ25890_REG_OTG_MASK,
				  BQ25890_REG_OTG_DISABLE);
	if (ret) {
		dev_err(info->dev, "disable bq25890 otg failed\n");
		return ret;
	}

	/* Enable charger detection function to identify the charger type */
	return regmap_update_bits(info->pmic, info->charger_detect,
				  BIT_DP_DM_BC_ENB, 0);
}

static int bq25890_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct bq25890_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	ret = bq25890_read(info, BQ25890_REG_3, &val);
	if (ret) {
		dev_err(info->dev, "failed to get bq25890 otg status\n");
		return ret;
	}

	val &= BQ25890_REG_OTG_MASK;

	return val;
}

static const struct regulator_ops bq25890_charger_vbus_ops = {
	.enable = bq25890_charger_enable_otg,
	.disable = bq25890_charger_disable_otg,
	.is_enabled = bq25890_charger_vbus_is_enabled,
};

static const struct regulator_desc bq25890_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &bq25890_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int bq25890_charger_register_vbus_regulator(struct bq25890_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &bq25890_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int bq25890_charger_register_vbus_regulator(struct bq25890_charger_info *info)
{
	return 0;
}
#endif

static int bq25890_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct bq25890_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->client = client;
	info->dev = dev;

	alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);

	mutex_init(&info->lock);
	INIT_WORK(&info->work, bq25890_charger_work);

	i2c_set_clientdata(client, info);

	info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(dev, "failed to find USB phy\n");
		return PTR_ERR(info->usb_phy);
	}

	info->edev = extcon_get_edev_by_phandle(info->dev, 0);
	if (IS_ERR(info->edev)) {
		dev_err(dev, "failed to find vbus extcon device.\n");
		return PTR_ERR(info->edev);
	}

	ret = bq25890_charger_is_fgu_present(info);
	if (ret) {
		dev_err(dev, "sc27xx_fgu not ready.\n");
		return -EPROBE_DEFER;
	}

	ret = bq25890_charger_register_vbus_regulator(info);
	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
		return ret;
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np)
		regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");

	if (regmap_np) {
		if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
			info->charger_pd_mask = BQ25890_DISABLE_PIN_MASK_2721;
		else
			info->charger_pd_mask = BQ25890_DISABLE_PIN_MASK;
	} else {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &info->charger_detect);
	if (ret) {
		dev_err(dev, "failed to get charger_detect\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &info->charger_pd);
	if (ret) {
		dev_err(dev, "failed to get charger_pd reg\n");
		return ret;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon device\n");
		return -ENODEV;
	}

	of_node_put(regmap_np);
	info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!info->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		return -ENODEV;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy_usb = devm_power_supply_register(dev,
						   &bq25890_charger_desc,
						   &charger_cfg);

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto err_mutex_lock;
	}

	ret = bq25890_charger_hw_init(info);
	if (ret) {
		dev_err(dev, "failed to bq25890_charger_hw_init\n");
		goto err_mutex_lock;
	}

	device_init_wakeup(info->dev, true);
	info->usb_notify.notifier_call = bq25890_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		goto err_psy_usb;
	}

	ret = bq25890_register_sysfs(info);
	if (ret) {
		dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
		goto err_sysfs;
	}

	bq25890_charger_detect_status(info);

	ret = bq25890_update_bits(info, BQ25890_REG_7,
				  BQ25890_REG_WATCHDOG_TIMER_MASK,
				  BQ25890_REG_WATCHDOG_TIMER_ENABLE);
	if (ret) {
		dev_err(info->dev, "Failed to enable bq25890 watchdog\n");
		return ret;
	}

	INIT_DELAYED_WORK(&info->otg_work, bq25890_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work,
			  bq25890_charger_feed_watchdog_work);

	return 0;

err_sysfs:
	sysfs_remove_group(&info->psy_usb->dev.kobj, &info->sysfs->attr_g);
	usb_unregister_notifier(info->usb_phy, &info->usb_notify);
err_psy_usb:
	power_supply_unregister(info->psy_usb);
err_mutex_lock:
	mutex_destroy(&info->lock);

	return ret;
}

static int bq25890_charger_remove(struct i2c_client *client)
{
	struct bq25890_charger_info *info = i2c_get_clientdata(client);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int bq25890_charger_suspend(struct device *dev)
{
	struct bq25890_charger_info *info = dev_get_drvdata(dev);
	ktime_t now, add;
	unsigned int wakeup_ms = BQ25890_OTG_ALARM_TIMER_MS;
	int ret;

	if (!info->otg_enable)
		return 0;

	cancel_delayed_work_sync(&info->wdt_work);

	/* feed watchdog first before suspend */
	ret = bq25890_update_bits(info, BQ25890_REG_3,
				  BQ25890_REG_WATCHDOG_MASK,
				  BQ25890_REG_WATCHDOG_ENABLE);
	if (ret)
		dev_warn(info->dev, "reset bq25890 failed before suspend\n");

	now = ktime_get_boottime();
	add = ktime_set(wakeup_ms / MSEC_PER_SEC,
		       (wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
	alarm_start(&info->otg_timer, ktime_add(now, add));

	return 0;
}

static int bq25890_charger_resume(struct device *dev)
{
	struct bq25890_charger_info *info = dev_get_drvdata(dev);
	int ret;

	if (!info->otg_enable)
		return 0;

	alarm_cancel(&info->otg_timer);

	/* feed watchdog first after resume */
	ret = bq25890_update_bits(info, BQ25890_REG_3,
				  BQ25890_REG_WATCHDOG_MASK,
				  BQ25890_REG_WATCHDOG_ENABLE);
	if (ret)
		dev_warn(info->dev, "reset bq25890 failed after resume\n");

	schedule_delayed_work(&info->wdt_work, HZ * 15);

	return 0;
}
#endif

static const struct dev_pm_ops bq25890_charger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(bq25890_charger_suspend,
				bq25890_charger_resume)
};

static const struct i2c_device_id bq25890_i2c_id[] = {
	{"bq25890_chg", 0},
	{}
};

static const struct of_device_id bq25890_charger_of_match[] = {
	{ .compatible = "ti,bq25890_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, bq2560x_charger_of_match);

static struct i2c_driver bq25890_charger_driver = {
	.driver = {
		.name = "bq25890_chg",
		.of_match_table = bq25890_charger_of_match,
		.pm = &bq25890_charger_pm_ops,
	},
	.probe = bq25890_charger_probe,
	.remove = bq25890_charger_remove,
	.id_table = bq25890_i2c_id,
};

module_i2c_driver(bq25890_charger_driver);

MODULE_AUTHOR("Changhua Zhang <Changhua.Zhang@unisoc.com>");
MODULE_DESCRIPTION("BQ25890 Charger Driver");
MODULE_LICENSE("GPL");

