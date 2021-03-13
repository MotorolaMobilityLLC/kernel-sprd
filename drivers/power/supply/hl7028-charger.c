/*
 * Driver for the FAIRCHILD hl7028 charger.
 * Author: Mark A. Greer <mgreer@animalcreek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>


#define HL7028_BATTERY_NAME				"sc27xx-fgu"
#define BIT_DP_DM_BC_ENB				BIT(0)
#define HL7028_OTG_VALID_MS				500
#define HL7028_FEED_WATCHDOG_VALID_MS			50

#define HL7028_REG_HZ_MODE_MASK			GENMASK(1, 1)
#define HL7028_REG_OPA_MODE_MASK			GENMASK(0, 0)

#define HL7028_DISABLE_PIN_MASK_2730			BIT(0)
#define HL7028_DISABLE_PIN_MASK_2721			BIT(15)
#define HL7028_DISABLE_PIN_MASK_2720			BIT(0)

#define VENDOR_HL7028					(0x1)

/******************************************************************************
* Register addresses
******************************************************************************/

#define HL7028_CON0      0x00
#define HL7028_CON1      0x01
#define HL7028_CON2      0x02
#define HL7028_CON3      0x03
#define HL7028_CON4      0x04
#define HL7028_CON5      0x05
#define HL7028_CON6      0x06
#define HL7028_CON7      0x07
#define HL7028_CON8      0x08
#define HL7028_CON9      0x09
#define HL7028_CON10      0x0A
#define HL7028_CON11      0x0B

#define HL7028_REG_OREG (0x1)

#define HL7028_OTG_EN (0x3 << 4)
#define HL7028_OTG_EN_SHIFT (4)

/******************************************************************************
* Register bits
******************************************************************************/
#define CON0_EN_HIZ_MASK   (0x01 << 7)
#define CON0_EN_HIZ_SHIFT  (7)
#define CON0_VINDPM_MASK (0xf << 3)
#define CON0_VINDPM_SHIFT (3)
#define CON0_IINLIM_MASK (0x7)
#define CON0_IINLIM_SHIFT (0)

#define CON1_REG_RST_MASK (0x1 << 7)
#define CON1_REG_RST_SHIFT (7)
#define CON1_WDT_RST_MASK (0x1 << 6)
#define CON1_WDT_RST_SHIFT (6)
#define CON1_OTG_CONFIG_MASK (0x3 << 4)
#define CON1_OTG_CONFIG_SHIFT (4)
#define CON1_CHG_CONFIG_MASK (0x3 << 4)
#define CON1_CHG_CONFIG_SHIFT (4)
#define CON1_SYS_MIN_MASK (0x7 << 1)
#define CON1_SYS_MIN_SHIFT (1)
#define CON1_BOOST_LIM_MASK (0x1)
#define CON1_BOOST_LIM_SHIFT (0)

#define CON2_ICHG_MASK (0x3f << 2)
#define CON2_ICHG_SHIFT (2)
#define CON2_BCOLD_MASK (0x1 << 1)
#define CON2_BCOLD_SHIFT (1)
#define CON2_FORCE_20PCT_MASK (0x1)
#define CON2_FORCE_20PCT_SHIFT (0)

#define CON3_IPRECHG_MASK (0xf << 4)
#define CON3_IPRECHG_SHIFT (4)
#define CON3_ITERM_MASK (0xf)
#define CON3_ITERM_SHIFT (0)

#define CON4_VREG_MASK (0x3f << 2)
#define CON4_VREG_SHIFT (2)
#define CON4_BATLOWV_MASK (0x1 << 1)
#define CON4_BATLOWV_SHIFT (1)
#define CON4_VRECHG_MASK (0x1)
#define CON4_VRECHG_SHIFT (0)

#define CON5_EN_TERM_MASK (0x1 << 7)
#define CON5_EN_TERM_SHIFT (7)
#define CON5_WATCHDOG_MASK (0x3 << 4)
#define CON5_WATCHDOG_SHIFT (4)
#define CON5_EN_TIMER_MASK (0x1 << 3)
#define CON5_EN_TIMER_SHIFT (3)
#define CON5_CHG_TIMER_MASK (0x3 << 1)
#define CON5_CHG_TIMER_SHIFT (1)

#define CON6_BOOSTV_MASK (0xf << 4)
#define CON6_BOOSTV_SHIFT (4)
#define CON6_BHOT_MASK (0x3 << 2)
#define CON6_BHOT_SHIFT (2)
#define CON6_TREG_MASK (0x3)
#define CON6_TREG_SHIFT (0)

#define CON7_TMR2X_EN_MASK (0x1 << 6)
#define CON7_TMR2X_EN_SHIFT (6)
#define CON7_BATFET_Disable_MASK (0x1 << 5)
#define CON7_BATFET_Disable_SHIFT (5)
#define CON7_INT_MASK_MASK (0x3)
#define CON7_INT_MASK_SHIFT (0)

#define CON8_VBUS_STAT_MASK (0x3 << 6)
#define CON8_VBUS_STAT_SHIFT (6)
#define CON8_CHRG_STAT_MASK (0x3 << 4)
#define CON8_CHRG_STAT_SHIFT (4)
#define CON8_VSYS_STAT_MASK (0x1)
#define CON8_VSYS_STAT_SHIFT (0)

//CON9
#define CON9_WATCHDOG_FAULT_MASK      (0x01 << 7)
#define CON9_WATCHDOG_FAULT_SHIFT     (7)

#define CON9_OTG_FAULT_MASK           (0x01 << 6)
#define CON9_OTG_FAULT_SHIFT          (6)

#define CON9_CHRG_FAULT_MASK           (0x03 << 4)
#define CON9_CHRG_FAULT_SHIFT          (4)

#define CON9_BAT_FAULT_MASK           (0x01 << 3)
#define CON9_BAT_FAULT_SHIFT          (3)

#define CON9_NTC_FAULT_MASK           (0x07)
#define CON9_NTC_FAULT_SHIFT          (0)

//CON10
#define CON10_PN_MASK      (0x07 << 5)
#define CON10_PN_SHIFT     (5)

//CON11
#define CON11_TSR_MASK      (0x07 << 3)
#define CON11_TSR_SHIFT     (3)
/******************************************************************************
* bit definitions
******************************************************************************/
/********** hl7028_REG_CON0 (0x00) **********/

// IINLIM [2:0]
#define IINLIM100 0
#define IINLIM150 1
#define IINLIM500 2
#define IINLIM900 3
#define IINLIM1000 4
#define IINLIM1500 5
#define IINLIM2000 6
#define NOLIMIT 7

// VSP [6:3]
#define VSP4P213 4
#define VSP4P293 5
#define VSP4P373 6
#define VSP4P453 7
#define VSP4P533 8
#define VSP4P613 9
#define VSP4P693 10
#define VSP4P773 11

//HZ_MODE [7]
#define NOTHIGHIMP 0
#define HIGHIMP 1

//HL7028_CON1
// SYS_MIN [3:1]
#define VLOWV3P0 0
#define VLOWV3P1 1
#define VLOWV3P2 2
#define VLOWV3P3 3
#define VLOWV3P4 4
#define VLOWV3P5 5
#define VLOWV3P6 6
#define VLOWV3P7 7

struct hl7028_charger_info {
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
};

static int
hl7028_charger_set_limit_current(struct hl7028_charger_info *info,
				   u32 limit_cur);

static bool hl7028_charger_is_bat_present(struct hl7028_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(HL7028_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return present;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&val);
	if (ret == 0 && val.intval)
		present = true;
	power_supply_put(psy);

	if (ret)
		dev_err(info->dev,
			"Failed to get property of present:%d\n", ret);

	return present;
}

static int hl7028_read(struct hl7028_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int hl7028_write(struct hl7028_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int hl7028_update_bits(struct hl7028_charger_info *info, u8 reg,
		u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = hl7028_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return hl7028_write(info, reg, v);
}

static int
hl7028_charger_set_safety_vol(struct hl7028_charger_info *info, u32 vol)
{
	u8 reg_val;

	if(vol <= 3880 ) {
		reg_val = 0x0;
	}else if(vol >= 5080) {
		reg_val = 0xf;
	}else {
		reg_val = (vol - 3880) / 80 + 1; //+1 vindpm
	}

	return hl7028_update_bits(info, HL7028_CON0,
				    CON0_VINDPM_MASK, reg_val << CON0_VINDPM_SHIFT);
}

static int
hl7028_charger_set_termina_vol(struct hl7028_charger_info *info, u32 vol)
{
	u8 reg_val;

	if(vol <= 3504){
		reg_val = 0x0;
	}else if( vol >= 4400){
		reg_val = 0x38;
	}else{
		reg_val = (vol - 3504) / 16;
	}

	return hl7028_update_bits(info, HL7028_CON4,
				    CON4_VREG_MASK,
				    reg_val << CON4_VREG_SHIFT);
}

static int
hl7028_charger_set_safety_cur(struct hl7028_charger_info *info, u32 cur)
{
	u8 reg_val;

	if (cur < 110000)
		reg_val = 0x0;
	else if (cur >= 110000 && cur < 160000)
		reg_val = 0x1;
	else if (cur >= 160000 && cur < 510000)
		reg_val = 0x2;
	else if (cur >= 510000 && cur < 910000)
		reg_val = 0x3;
	else if (cur >= 910000 && cur < 1210000)
		reg_val = 0x4;
	else if (cur >= 1210000 && cur < 2100000)
		reg_val = 0x5;
	else if (cur >= 2100000)
		reg_val = 0x7;

	return hl7028_update_bits(info, HL7028_CON0,
				    CON0_IINLIM_MASK,
				    reg_val << CON0_IINLIM_SHIFT);
}

static int hl7028_charger_hw_init(struct hl7028_charger_info *info)
{
	struct power_supply_battery_info bat_info = { };
	int voltage_max_microvolt, current_max_ua;
	int ret;

	ret = power_supply_get_battery_info(info->psy_usb, &bat_info);
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

		voltage_max_microvolt =
			bat_info.constant_charge_voltage_max_uv / 1000;
		current_max_ua = bat_info.constant_charge_current_max_ua / 1000;
		power_supply_put_battery_info(info->psy_usb, &bat_info);

		ret = hl7028_charger_set_safety_vol(info, voltage_max_microvolt);
		if (ret) {
			dev_err(info->dev, "set hl7028 safety vol failed\n");
			return ret;
		}

		ret = hl7028_charger_set_safety_cur(info, info->cur.dcp_cur);
		if (ret) {
			dev_err(info->dev, "set hl7028 safety cur failed\n");
			return ret;
		}
#if 0 //
		ret = hl7028_update_bits(info, HL7028_CON1,
					   CON1_REG_RST_MASK,
					   1 << CON1_REG_RST_SHIFT);
		if (ret) {
			dev_err(info->dev, "reset hl7028 failed\n");
			return ret;
		}
#endif
		ret = hl7028_update_bits(info, 0x0c, 0xff, 0xfc); //hl7059

		ret = hl7028_update_bits(info, HL7028_CON5,
					   CON5_EN_TERM_MASK, 1 << CON5_EN_TERM_SHIFT); //FROM_EXT_IC
		if (ret) {
			dev_err(info->dev, "set hl7028 terminal cur failed\n");
			return ret;
		}

		ret = hl7028_update_bits(info,
					   HL7028_CON1,
					   CON1_WDT_RST_MASK,
					   1 << CON1_WDT_RST_SHIFT);
		ret = hl7028_update_bits(info,
					   HL7028_CON5,
					   CON5_WATCHDOG_MASK,
					   0 << CON5_WATCHDOG_SHIFT); //disable watchdog
		if (ret) {
			dev_err(info->dev, "feed hl7028 watchdog failed\n");
			return ret;
		}
		ret = hl7028_charger_set_termina_vol(info, voltage_max_microvolt);
		if (ret) {
			dev_err(info->dev, "set hl7028 terminal vol failed\n");
			return ret;
		}

		ret = hl7028_charger_set_limit_current(info,
							 info->cur.unknown_cur);
		if (ret)
			dev_err(info->dev, "set hl7028 limit current failed\n");
	}

	return ret;
}

static int hl7028_charger_start_charge(struct hl7028_charger_info *info)
{
	int ret;

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask, 0);
	if (ret)
		dev_err(info->dev, "enable hl7028 charge failed\n");

	return ret;
}

static void hl7028_charger_stop_charge(struct hl7028_charger_info *info)
{
	int ret;

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask,
				 info->charger_pd_mask);
	if (ret)
		dev_err(info->dev, "disable hl7028 charge failed\n");
}

static int hl7028_charger_set_current(struct hl7028_charger_info *info,
					u32 cur)
{
	u8 reg_val;

	if (cur <= 512000) 
	{
		reg_val = 0;
	}else if (cur >= 3008000) {
		reg_val = 0x27;
	}else {
		reg_val = (cur - 512000) / 64000;
	}

	return hl7028_update_bits(info, HL7028_CON2,
				    CON2_ICHG_MASK,
				    reg_val << CON2_ICHG_SHIFT);
}

static int hl7028_charger_get_current(struct hl7028_charger_info *info,
					u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = hl7028_read(info, HL7028_CON2, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= CON2_ICHG_MASK;
	reg_val = reg_val >> CON2_ICHG_SHIFT;

	*cur = (reg_val * 512000) + 64000;
	return 0;
}

static int
hl7028_charger_set_limit_current(struct hl7028_charger_info *info,
				   u32 cur)
{
	u8 reg_val;
	int ret;

	if (cur < 110000)
		reg_val = 0x0;
	else if (cur >= 110000 && cur < 160000)
		reg_val = 0x1;
	else if (cur >= 160000 && cur < 510000)
		reg_val = 0x2;
	else if (cur >= 510000 && cur < 910000)
		reg_val = 0x3;
	else if (cur >= 910000 && cur < 1210000)
		reg_val = 0x4;
	else if (cur >= 1210000 && cur < 2100000)
		reg_val = 0x5;
	else if (cur >= 2100000)
		reg_val = 0x7;

	ret = hl7028_update_bits(info, HL7028_CON0,
				    CON0_IINLIM_MASK,
				    reg_val << CON0_IINLIM_SHIFT);
	if (ret)
		dev_err(info->dev, "set hl7028 limit cur failed\n");

	return ret;
}

static u32
hl7028_charger_get_limit_current(struct hl7028_charger_info *info,
				   u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = hl7028_read(info, HL7028_CON0, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= CON0_IINLIM_MASK;
	reg_val = reg_val >> CON0_IINLIM_SHIFT;

	switch (reg_val) {
	case 0:
		*limit_cur = 100000;
		break;
	case 1:
		*limit_cur = 150000;
		break;
	case 2:
		*limit_cur = 500000;
		break;
	case 3:
		*limit_cur = 900000;
		break;
	case 4:
		*limit_cur = 1200000;
		break;
	case 5:
		*limit_cur = 1500000;
		break;
	case 6:
		*limit_cur = 2000000;
		break;
	case 7:
		*limit_cur = 3000000;
		break;
	default:
		*limit_cur = 120000;
	}

	return 0;
}

static int hl7028_charger_get_health(struct hl7028_charger_info *info,
				     u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int hl7028_charger_get_online(struct hl7028_charger_info *info,
				     u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}

static int hl7028_charger_feed_watchdog(struct hl7028_charger_info *info,
					  u32 val)
{
	int ret;

	ret = hl7028_update_bits(info, HL7028_CON1,
				   CON1_WDT_RST_MASK, 1 << CON1_WDT_RST_SHIFT);
	if (ret)
		dev_err(info->dev, "reset hl7028 failed\n");

	return ret;
}

static int hl7028_charger_get_status(struct hl7028_charger_info *info)
{
	if (info->charging == true)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int hl7028_charger_set_status(struct hl7028_charger_info *info,
				       int val)
{
	int ret = 0;

	if (!val && info->charging) {
		hl7028_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = hl7028_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static void hl7028_charger_work(struct work_struct *data)
{
	struct hl7028_charger_info *info =
		container_of(data, struct hl7028_charger_info, work);
	int limit_cur, cur, ret;
	bool present = hl7028_charger_is_bat_present(info);

	mutex_lock(&info->lock);

	if (info->limit > 0 && !info->charging && present) {
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

		ret = hl7028_charger_set_limit_current(info, limit_cur);
		if (ret)
			goto out;

		ret = hl7028_charger_set_current(info, cur);
		if (ret)
			goto out;

		ret = hl7028_charger_start_charge(info);
		if (ret)
			goto out;

		info->charging = true;
	} else if ((!info->limit && info->charging) || !present) {
		/* Stop charging */
		info->charging = false;
		hl7028_charger_stop_charge(info);
	}

out:
	mutex_unlock(&info->lock);
	dev_info(info->dev, "battery present = %d, charger type = %d\n",
		 present, info->usb_phy->chg_type);
	cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}


static int hl7028_charger_usb_change(struct notifier_block *nb,
				       unsigned long limit, void *data)
{
	struct hl7028_charger_info *info =
		container_of(nb, struct hl7028_charger_info, usb_notify);

	info->limit = limit;

	schedule_work(&info->work);
	return NOTIFY_OK;
}

static int hl7028_charger_usb_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct hl7028_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, online, health;
	enum usb_charger_type type;
	int ret = 0;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->limit)
			val->intval = hl7028_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = hl7028_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = hl7028_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = hl7028_charger_get_online(info, &online);
		if (ret)
			goto out;

		val->intval = online;

		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = hl7028_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;

	case POWER_SUPPLY_PROP_USB_TYPE:
		type = info->usb_phy->charger_detect(info->usb_phy);

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

	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int hl7028_charger_usb_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct hl7028_charger_info *info = power_supply_get_drvdata(psy);
	int ret;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = hl7028_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = hl7028_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_STATUS:
		ret = hl7028_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;

	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		ret = hl7028_charger_feed_watchdog(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "feed charger watchdog failed\n");
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = hl7028_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int hl7028_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_STATUS:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_usb_type hl7028_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static enum power_supply_property hl7028_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_USB_TYPE,
};

static const struct power_supply_desc hl7028_charger_desc = {
	.name			= "hl7028_charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= hl7028_usb_props,
	.num_properties		= ARRAY_SIZE(hl7028_usb_props),
	.get_property		= hl7028_charger_usb_get_property,
	.set_property		= hl7028_charger_usb_set_property,
	.property_is_writeable	= hl7028_charger_property_is_writeable,
	.usb_types		= hl7028_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(hl7028_charger_usb_types),
};

static void hl7028_charger_detect_status(struct hl7028_charger_info *info)
{
	int min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;

	usb_phy_get_charger_current(info->usb_phy, &min, &max);
	info->limit = min;
	schedule_work(&info->work);
}

static void
hl7028_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct hl7028_charger_info *info = container_of(dwork,
							  struct hl7028_charger_info,
							  wdt_work);
	int ret;

	ret = hl7028_update_bits(info, HL7028_CON1,
				   CON1_WDT_RST_MASK,
				   1 << CON1_WDT_RST_SHIFT);
	if (ret) {
		dev_err(info->dev, "reset hl7028 failed\n");
		return;
	}
	schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#ifdef CONFIG_REGULATOR
static void hl7028_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct hl7028_charger_info *info = container_of(dwork,
			struct hl7028_charger_info, otg_work);
	int ret;

	if (!extcon_get_state(info->edev, EXTCON_USB)) {
		ret = hl7028_update_bits(info, HL7028_CON1,
					   CON1_OTG_CONFIG_MASK,
					   2 << CON1_OTG_CONFIG_SHIFT);
		if (ret)
			dev_err(info->dev, "restart hl7028 charger otg failed\n");
	}

	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(500));
}

static int hl7028_charger_enable_otg(struct regulator_dev *dev)
{
	struct hl7028_charger_info *info = rdev_get_drvdata(dev);
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

	ret = hl7028_update_bits(info, HL7028_CON1,
				   CON1_OTG_CONFIG_MASK,
				   2 << CON1_OTG_CONFIG_SHIFT);
	if (ret) {
		dev_err(info->dev, "enable hl7028 otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect,
				   BIT_DP_DM_BC_ENB, 0);
		return ret;
	}

	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(HL7028_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(HL7028_OTG_VALID_MS));

	return 0;
}

static int hl7028_charger_disable_otg(struct regulator_dev *dev)
{
	struct hl7028_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	ret = hl7028_update_bits(info, HL7028_CON1,
				   CON1_OTG_CONFIG_MASK,
				   0 << CON1_OTG_CONFIG_SHIFT);
	if (ret) {
		dev_err(info->dev, "disable hl7028 otg failed\n");
		return ret;
	}

	/* Enable charger detection function to identify the charger type */
	return regmap_update_bits(info->pmic, info->charger_detect,
				  BIT_DP_DM_BC_ENB, 0);
}

static int hl7028_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct hl7028_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	ret = hl7028_read(info, HL7028_CON1, &val);
	val &= CON1_OTG_CONFIG_MASK;
	val = (val >> CON1_OTG_CONFIG_SHIFT) & 0x0c;
	if (ret) {
		dev_err(info->dev, "failed to get hl7028 otg status\n");
		return ret;
	}

//	val &= HL7028_REG_OPA_MODE_MASK;

	return val;
}

static const struct regulator_ops hl7028_charger_vbus_ops = {
	.enable = hl7028_charger_enable_otg,
	.disable = hl7028_charger_disable_otg,
	.is_enabled = hl7028_charger_vbus_is_enabled,
};

static const struct regulator_desc hl7028_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &hl7028_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int
hl7028_charger_register_vbus_regulator(struct hl7028_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &hl7028_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int
hl7028_charger_register_vbus_regulator(struct hl7028_charger_info *info)
{
	return 0;
}
#endif

static int hl7028_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct hl7028_charger_info *info;
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
	mutex_init(&info->lock);
	INIT_WORK(&info->work, hl7028_charger_work);

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

	ret = hl7028_charger_register_vbus_regulator(info);
	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
		return ret;
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
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

	if (of_device_is_compatible(regmap_np->parent, "sprd,sc2730"))
		info->charger_pd_mask = HL7028_DISABLE_PIN_MASK_2730;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
		info->charger_pd_mask = HL7028_DISABLE_PIN_MASK_2721;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2720"))
		info->charger_pd_mask = HL7028_DISABLE_PIN_MASK_2720;
	else {
		dev_err(dev, "failed to get charger_pd mask\n");
		return -EINVAL;
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

	info->usb_notify.notifier_call = hl7028_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		return ret;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy_usb = devm_power_supply_register(dev,
						   &hl7028_charger_desc,
						   &charger_cfg);
	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return PTR_ERR(info->psy_usb);
	}

	ret = hl7028_charger_hw_init(info);
	if (ret) {
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return ret;
	}
	hl7028_charger_detect_status(info);
	INIT_DELAYED_WORK(&info->otg_work, hl7028_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work,
			  hl7028_charger_feed_watchdog_work);
	dev_err(dev, "hl7028 ok to register\n");
	return 0;
}

static int hl7028_charger_remove(struct i2c_client *client)
{
	struct hl7028_charger_info *info = i2c_get_clientdata(client);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

static const struct i2c_device_id hl7028_i2c_id[] = {
	{"hl7028_chg", 0},
	{}
};

static const struct of_device_id hl7028_charger_of_match[] = {
	{ .compatible = "dfsl,hl7028_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, hl7028_charger_of_match);

static struct i2c_driver hl7028_charger_driver = {
	.driver = {
		.name = "hl7028_chg",
		.of_match_table = hl7028_charger_of_match,
	},
	.probe = hl7028_charger_probe,
	.remove = hl7028_charger_remove,
	.id_table = hl7028_i2c_id,
};

module_i2c_driver(hl7028_charger_driver);
MODULE_DESCRIPTION("HL7028 Charger Driver");
MODULE_LICENSE("GPL v2");
