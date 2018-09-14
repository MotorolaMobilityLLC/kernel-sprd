/* SPDX-License-Identifier: GPL-2.0
 *
 * Charger device driver for SC2703
 *
 * Copyright (c) 2018 Dialog Semiconductor.
 */

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/mfd/sc2703/registers.h>
#include <linux/usb/phy.h>

#define SC2703_BATTERY_NAME		"sc27xx-fgu"
#define SC2703_EVENT_CLR_MASK		GENMASK(7, 0)
#define SC2703_VBAT_CHG_DEFAULT		0x14
#define SC2703_VBAT_CHG_MAX		0x32
#define SC2703_IBAT_TERM_DEFAULT	0x00
#define SC2703_IBAT_TERM_MAX		0x7
#define SC2703_S_CHG_STAT_FULL		0x5
#define SC2703_S_CHG_STAT_FAULT_L1	0x6
#define SC2703_S_CHG_STAT_FAULT_L2	0x7
#define SC2703_E_VBAT_OV_SHIFT		6
#define SC2703_E_VBAT_OV_MASK		BIT(6)

#define SC2703_CHG_B_IMIN		500000
#define SC2703_CHG_B_ISTEP		50000
#define SC2703_CHG_B_IMAX		3500000

#define SC2703_CHG_B_VMIN		3800000
#define SC2703_CHG_B_VSTEP		20000
#define SC2703_CHG_B_VMAX		4800000

struct sc2703_charger_info {
	struct device *dev;
	struct regmap *regmap;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct power_supply *psy_usb;
	struct mutex lock;
	bool charging;
};

/* sc2703 input limit current, Milliamp */
static int sc2703_limit_current[] = {
	100000,
	150000,
	500000,
	900000,
	1500000,
	2000000,
	2500000,
	3000000,
};

static u32 sc2703_charger_of_prop_range(struct device *dev, u32 val, u32 min,
				       u32 max, u32 step, u32 default_val,
				       const char *name)
{
	if (val < min || val > max || val % step) {
		dev_warn(dev, "Invalid %s value\n", name);
		return default_val;
	} else {
		return (val - min) / step;
	}
}

static int sc2703_charger_hw_init(struct sc2703_charger_info *info)
{
	struct power_supply_battery_info bat_info = { };
	u32 cur_val, vol_val;
	int ret;

	ret = power_supply_get_battery_info(info->psy_usb, &bat_info);
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");

		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 100 mA, and default
		 * charge termination voltage to 4.2V.
		 */
		cur_val = 0x0;
		vol_val = 0x14;
	} else {
		cur_val = sc2703_charger_of_prop_range(info->dev,
					bat_info.charge_term_current_ua,
					100000, 450000, 50000,
					SC2703_IBAT_TERM_DEFAULT,
					"ibat-term");

		/* Set charge termination voltage */
		vol_val = sc2703_charger_of_prop_range(info->dev,
					bat_info.constant_charge_voltage_max_uv,
					SC2703_CHG_B_VMIN,
					SC2703_CHG_B_VMAX,
					SC2703_CHG_B_VSTEP,
					SC2703_VBAT_CHG_DEFAULT,
					"vbat-chg");
	}

	/* Set charge termination current */
	if (cur_val > SC2703_IBAT_TERM_MAX)
		cur_val = SC2703_IBAT_TERM_MAX;

	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_D,
				 SC2703_IBAT_TERM_MASK,
				 cur_val << SC2703_IBAT_TERM_SHIFT);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set terminal current:%d\n", ret);
		return ret;
	}

	/* Set charge termination voltage */
	if (vol_val > SC2703_VBAT_CHG_MAX)
		vol_val = SC2703_VBAT_CHG_MAX

	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_A,
				 SC2703_VBAT_CHG_MASK, vol_val);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set terminal voltage:%d\n", ret);
		return ret;
	}

	/* Set vindrop threshold 4300mv */
	ret = regmap_update_bits(info->regmap, SC2703_VIN_CTRL_A,
					 SC2703_VIN_DROP_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set vindrop threshold:%d\n", ret);
		return ret;
	}

	/* Set pre-charging current 100ma */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_D,
				 SC2703_IBAT_PRE_MASK, 0x01);
	if (ret) {
		dev_err(info->dev,
			 "Failed to write pre-charging current:%d\n", ret);
		return ret;
	}

	/* Set end of charge detection time 60min */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_D,
				 SC2703_T_EOC_MASK, 0x07 << SC2703_T_EOC_SHIFT);
	if (ret) {
		dev_err(info->dev, "Failed to set detection time:%d\n", ret);
		return ret;
	}

	/* Set battery voltage re-charge threshold 100ma */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_A,
				 SC2703_VBAT_RECHG_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set voltage re-charge threshold:%d\n", ret);
		return ret;
	}

	/* Set dc-dc peak current limit 6000ma */
	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_B,
				 SC2703_DCDC_PEAK_ILIM_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set dc-dc peak current limit:%d\n", ret);
		return ret;
	}

	/* Set vbat ovp 4600mv */
	ret = regmap_update_bits(info->regmap, SC2703_VBAT_CTRL_A,
				 SC2703_VBAT_OV_MASK,
				 0x06 << SC2703_VBAT_OV_SHIFT);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set voltage ovp value:%d\n", ret);
		return ret;
	}

	/* Set vbat uvp 2800mv */
	ret = regmap_update_bits(info->regmap, SC2703_VBAT_CTRL_A,
				 SC2703_VBAT_UV_MASK, 0x0c);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set voltage uvp value:%d\n", ret);
		return ret;
	}

	/* Set minimum system voltage 3420mv */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_C,
				 SC2703_VSYS_MIN_MASK, 0x07);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set minimum system voltage:%d\n", ret);
		return ret;
	}

	/* Set pre-charge timeout 900s */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_TIMER_CTRL_A,
				 SC2703_TIMEOUT_PRE_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set pre-charge timeout:%d\n", ret);
		return ret;
	}

	/* Set constant current/constant voltage charging timeout 10hour */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_TIMER_CTRL_A,
				 SC2703_TIMEOUT_CCCV_MASK, 0x04);
	if (ret) {
		dev_err(info->dev, "Failed to set cc timeout:%d\n", ret);
		return ret;
	}

	/* enable the watchdog timer */
	ret = regmap_update_bits(info->regmap, SC2703_CONF_A,
				 SC2703_WD_EN_MASK, SC2703_WD_EN_MASK);
	if (ret) {
		dev_err(info->dev,
			 "Failed to enable the watchdog timer:%d\n", ret);
		return ret;
	}

	/* Set dc-dc output current limit in reverse boost mode */
	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_B,
				 SC2703_IIN_REV_LIM_MASK,
				 0x05 << SC2703_IIN_REV_LIM_SHIFT);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set dc-dc output current limit:%d\n", ret);
		return ret;
	}

	/* Set dc-dc peak current limit in reverse boost mode */
	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_B,
				 SC2703_DCDC_REV_PEAK_ILIM_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set dc-dc peak current limit:%d\n", ret);
		return ret;
	}

	/* Selects the sources for battery detection */
	ret = regmap_update_bits(info->regmap, SC2703_CONF_A,
				 SC2703_BAT_DET_SRC_MASK,
				 0x01 << SC2703_BAT_DET_SRC_SHIFT);
	if (ret) {
		dev_err(info->dev,
			 "Failed to selects the sources detection:%d\n", ret);
		return ret;
	}

	/* Set T3 temperature threshold */
	ret = regmap_update_bits(info->regmap, SC2703_TBAT_CTRL_A,
				 SC2703_TBAT_T3_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set T3 temperature threshold:%d\n", ret);
		return ret;
	}

	/* Set T4 temperature threshold */
	ret = regmap_update_bits(info->regmap, SC2703_TBAT_CTRL_A,
				 SC2703_TBAT_T4_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set T4 temperature threshold:%d\n", ret);
		return ret;
	}

	/* Set charge voltage reduction in TBAT_T1-TBAT_T2 */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_E,
				 SC2703_VBAT_COLD_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set voltage reduction in T1-T2:%d\n", ret);
		return ret;
	}

	/* Set charge current reduction in TBAT_T1-TBAT_T2 */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_E,
				 SC2703_IBAT_COLD_MASK, SC2703_IBAT_COLD_MASK);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set current reduction in T1-T2:%d\n", ret);
		return ret;
	}

	/* Set charge voltage reduction in TBAT_T3-TBAT_T4 */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_E,
				 SC2703_VBAT_WARM_MASK, 0x00);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set voltage reduction in T3-T4:%d\n", ret);
		return ret;
	}

	/* Set charge current reduction in TBAT_T3-TBAT_T4 */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_E,
				 SC2703_IBAT_WARM_MASK, SC2703_IBAT_WARM_MASK);
	if (ret) {
		dev_err(info->dev,
			 "Failed to set current reduction in T3-T4:%d\n", ret);
		return ret;
	}

	/* Unlock 2703 test mode */
	ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_REG_UNLOCK_VAL);
	if (ret) {
		dev_err(info->dev,
			 "Failed to unlock 2703 test mode:%d\n", ret);
		return ret;
	}

	/* Disable vsys_uv */
	ret = regmap_update_bits(info->regmap, SC2703_VSYS_UV_SWITCH,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_VSYS_UV_SWITCH_VAL);
	if (ret) {
		dev_err(info->dev, "Failed to disable vsys_uv:%d\n", ret);
		return ret;
	}

	/* Enable charge buck */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_BUCK_SWITCH,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_CHG_REG_DISABLE_VAL);
	if (ret) {
		dev_err(info->dev, "Failed to enable charge buck:%d\n", ret);
		return ret;
	}

	/* Lock 2703 test mode */
	ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_CHG_REG_DISABLE_VAL);
	if (ret) {
		dev_err(info->dev, "Failed to lock 2703 test mode:%d\n", ret);
		return ret;
	}

	return 0;
}

static void sc2703_clear_event(struct sc2703_charger_info *info)
{
	int ret;

	ret = regmap_update_bits(info->regmap, SC2703_EVENT_A,
				 SC2703_EVENT_CLR_MASK, SC2703_EVENT_CLR_MASK);
	if (ret)
		dev_warn(info->dev, "Failed to clear charge event_a:%d\n", ret);

	ret = regmap_update_bits(info->regmap, SC2703_EVENT_B,
				 SC2703_EVENT_CLR_MASK, SC2703_EVENT_CLR_MASK);
	if (ret)
		dev_warn(info->dev, "Failed to clear charge event_b:%d\n", ret);

	ret = regmap_update_bits(info->regmap, SC2703_EVENT_C,
				 SC2703_EVENT_CLR_MASK,
				 SC2703_EVENT_CLR_MASK &
				 (~SC2703_E_ADC_DONE_MASK));
	if (ret)
		dev_warn(info->dev, "Failed to clear charge event_c:%d\n", ret);

	ret = regmap_update_bits(info->regmap, SC2703_EVENT_D,
				 SC2703_EVENT_CLR_MASK, SC2703_EVENT_CLR_MASK);
	if (ret)
		dev_warn(info->dev, "Failed to clear charge event_d:%d\n", ret);
}

static int sc2703_charger_start_charge(struct sc2703_charger_info *info)
{
	int ret;

	sc2703_clear_event(info);
	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_A,
				 SC2703_CHG_EN_MASK, SC2703_CHG_EN_MASK);
	if (ret) {
		dev_err(info->dev, "Failed to enable sc2703 charge:%d\n", ret);
		return ret;
	}
	ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_REG_UNLOCK_VAL);
	if (ret) {
		dev_err(info->dev,
			"Failed to unlock sc2703 test mode:%d\n", ret);
		return ret;
	}
	ret = regmap_update_bits(info->regmap, SC2703_VSYS_UV_SWITCH,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_VSYS_UV_SWITCH_VAL);
	if (ret) {
		dev_err(info->dev,
			"Failed to disable sc2703 vys_uv mode:%d\n", ret);
		return ret;
	}
	ret = regmap_update_bits(info->regmap, SC2703_CHG_BUCK_SWITCH,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_CHG_REG_DISABLE_VAL);
	if (ret) {
		dev_err(info->dev,
			"Failed to enable sc2703 charge buck:%d\n", ret);
		return ret;
	}
	ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_CHG_REG_DISABLE_VAL);
	if (ret) {
		dev_err(info->dev, "Failed to lock sc2703 test mode:%d\n", ret);
		return ret;
	}

	return 0;
}

static void sc2703_charger_stop_charge(struct sc2703_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval data;
	int bat_present, ret;

	psy = power_supply_get_by_name(SC2703_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu:%d\n", ret);
		return;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&data);
	power_supply_put(psy);
	if (ret) {
		dev_err(info->dev,
			"Failed to get property of present:%d\n", ret);
		return;
	}

	bat_present = data.intval;
	if (bat_present) {
		ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_A,
					 SC2703_CHG_EN_MASK |
					 SC2703_DCDC_EN_MASK, 0);
		if (ret)
			dev_err(info->dev,
				"Failed to set chg_en and dcdc_en:%d\n", ret);
	} else {
		ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_A,
					 SC2703_CHG_EN_MASK, 0);
		if (ret) {
			dev_err(info->dev, "Failed to set chg_en:%d\n", ret);
			return;
		}
		ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
					 SC2703_CHG_REG_MASK_ALL,
					 SC2703_REG_UNLOCK_VAL);
		if (ret) {
			dev_err(info->dev,
				"Failed to unlock sc2703 test mode:%d\n", ret);
			return;
		}
		ret = regmap_update_bits(info->regmap, SC2703_VSYS_UV_SWITCH,
					 SC2703_CHG_REG_MASK_ALL,
					 SC2703_VSYS_UV_SWITCH_VAL);
		if (ret) {
			dev_err(info->dev,
				"Failed to disable sc2703 vys_uv:%d\n", ret);
			return;
		}
		ret = regmap_update_bits(info->regmap, SC2703_CHG_BUCK_SWITCH,
					 SC2703_CHG_REG_MASK_ALL,
					 SC2703_CHG_BUCK_SWITCH_VAL);
		if (ret) {
			dev_err(info->dev,
				"Failed to enable  charge buck:%d\n", ret);
			return;
		}
		ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
					 SC2703_CHG_REG_MASK_ALL,
					 SC2703_CHG_REG_DISABLE_VAL);
		if (ret)
			dev_err(info->dev,
				"Failed to lock sc2703 test mode:%d\n", ret);
	}
}

static int
sc2703_charger_set_current(struct sc2703_charger_info *info, u32 val)
{
	u32 reg;

	/* mA to uA */
	val *= 1000;

	if (val < SC2703_CHG_B_IMIN)
		val = SC2703_CHG_B_IMIN;
	else if (val > SC2703_CHG_B_IMAX)
		val = SC2703_CHG_B_IMAX;

	reg = (val - SC2703_CHG_B_IMIN) / SC2703_CHG_B_ISTEP;
	return regmap_update_bits(info->regmap, SC2703_CHG_CTRL_B,
			   SC2703_IBAT_CHG_MASK,
			   reg << SC2703_IBAT_CHG_SHIFT);
}

static u32
sc2703_charger_get_current(struct sc2703_charger_info *info, u32 *cur)
{
	u32 val;
	int ret;

	ret = regmap_read(info->regmap, SC2703_CHG_CTRL_B, &val);
	if (ret)
		return ret;

	/* The value of the SC2703_CHG_CTRL_B register is converted
	 * to the charge current value.
	 * Calculation formula:(x * 50) + 500 ma
	 */
	 *cur = ((val & SC2703_IBAT_CHG_MASK) * 50) + 500;

	return 0;
}

static int
sc2703_charger_set_limit_current(struct sc2703_charger_info *info, u32 val)
{
	u32 limit_index;
	int i;

	val *= 1000;
	for (i = 1; i < ARRAY_SIZE(sc2703_limit_current); ++i)
		if (val < sc2703_limit_current[i])
			break;

	limit_index = i - 1;
	regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_D,
			   SC2703_IIN_LIM0_MASK | SC2703_IIN_LIM1_MASK,
			   limit_index |
			   (limit_index << SC2703_IIN_LIM1_SHIFT));

	return 0;
}

static int
sc2703_charger_get_limit_current(struct sc2703_charger_info *info, u32 *cur)
{

	u32 val;
	int ret;

	ret = regmap_read(info->regmap, SC2703_DCDC_CTRL_D, &val);
	if (ret)
		return ret;

	*cur = sc2703_limit_current[val & SC2703_IIN_LIM0_MASK];

	return 0;
}

static int sc2703_charger_get_status(struct sc2703_charger_info *info)
{
	u32 status[4];
	int ret;

	ret = regmap_bulk_read(info->regmap, SC2703_STATUS_A, status,
			       ARRAY_SIZE(status));
	if (ret)
		return POWER_SUPPLY_STATUS_UNKNOWN;

	/* If wall charger not connected, we're definitely discharging */
	if (!(status[0] & SC2703_S_ADP_DET_MASK))
		return POWER_SUPPLY_STATUS_DISCHARGING;

	switch (status[3] & SC2703_S_CHG_STAT_MASK) {
	case SC2703_S_CHG_STAT_DCDC_OFF:
	case SC2703_S_CHG_STAT_FAULT_L2:
		ret = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case SC2703_S_CHG_STAT_FAULT_L1:
		ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case SC2703_S_CHG_STAT_PRE:
	case SC2703_S_CHG_STAT_CC:
	case SC2703_S_CHG_STAT_CV:
	case SC2703_S_CHG_STAT_TOP_OFF:
		ret = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case SC2703_S_CHG_STAT_FULL:
		ret = POWER_SUPPLY_STATUS_FULL;
		break;
	default:
		ret = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	return ret;
}

static int sc2703_charger_usb_change(struct notifier_block *nb,
				     unsigned long limit, void *data)
{
	struct sc2703_charger_info *info =
		container_of(nb, struct sc2703_charger_info, usb_notify);
	int ret = 0;

	mutex_lock(&info->lock);

	if (limit > 0) {
		/* set current limitation and start to charge */
		ret = sc2703_charger_set_limit_current(info, limit);
		if (ret)
			goto out;

		ret = sc2703_charger_set_current(info, limit);
		if (ret)
			goto out;

		ret = sc2703_charger_start_charge(info);
		if (ret)
			goto out;

		info->charging = true;
	} else {
		/* Stop charging */
		info->charging = false;
		sc2703_charger_stop_charge(info);
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int sc2703_charger_usb_get_property(struct power_supply *psy,
					   enum power_supply_property psp,
					   union power_supply_propval *val)
{
	struct sc2703_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;
	u32 cur;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->charging)
			val->intval = sc2703_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sc2703_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sc2703_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int
sc2703_charger_usb_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct sc2703_charger_info *info = power_supply_get_drvdata(psy);
	int ret;

	mutex_lock(&info->lock);

	if (!info->charging) {
		mutex_unlock(&info->lock);
		return -ENODEV;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sc2703_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sc2703_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sc2703_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_property sc2703_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
};

static const struct power_supply_desc sc2703_charger_desc = {
	.name			= "sc2703_charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= sc2703_usb_props,
	.num_properties		= ARRAY_SIZE(sc2703_usb_props),
	.get_property		= sc2703_charger_usb_get_property,
	.set_property		= sc2703_charger_usb_set_property,
	.property_is_writeable	= sc2703_charger_property_is_writeable,
};

static int sc2703_charger_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct sc2703_charger_info *info;
	struct power_supply_config charger_cfg = { };
	int ret;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	mutex_init(&info->lock);
	info->dev = &pdev->dev;

	info->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!info->regmap) {
		dev_err(&pdev->dev, "failed to get charger regmap\n");
		return -ENODEV;
	}
	charger_cfg.drv_data = info;
	charger_cfg.of_node = np;
	info->psy_usb = devm_power_supply_register(&pdev->dev,
						   &sc2703_charger_desc,
						   &charger_cfg);
	if (IS_ERR(info->psy_usb)) {
		dev_err(&pdev->dev, "failed to register power supply\n");
		return PTR_ERR(info->psy_usb);
	}

	ret = sc2703_charger_hw_init(info);
	if (ret)
		return ret;

	info->usb_phy = devm_usb_get_phy_by_phandle(&pdev->dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(&pdev->dev, "failed to find USB phy\n");
		return PTR_ERR(info->usb_phy);
	}
	info->usb_notify.notifier_call = sc2703_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(&pdev->dev, "failed to register notifier:%d\n", ret);
		return ret;
	}

	return 0;
}

static void sc2703_shutdown(struct platform_device *pdev)
{
	struct sc2703_charger_info *info = platform_get_drvdata(pdev);
	int ret;

	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_A,
				 SC2703_OTG_EN_MASK, 0);
	if (ret)
		dev_warn(&pdev->dev, "failed to disable otg_en:%d\n", ret);

	/* Unlock 2703 test mode */
	ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_REG_UNLOCK_VAL);
	if (ret)
		dev_warn(&pdev->dev,
			"failed to unlock sc2703 test mode:%d\n", ret);

	/* Enable charge buck */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_BUCK_SWITCH,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_CHG_REG_DISABLE_VAL);
	if (ret)
		dev_warn(&pdev->dev,
			"failed to disable sc2703 charge buck:%d\n", ret);

	/* Lock 2703 test mode */
	ret = regmap_update_bits(info->regmap, SC2703_REG_UNLOCK,
				 SC2703_CHG_REG_MASK_ALL,
				 SC2703_CHG_REG_DISABLE_VAL);
	if (ret)
		dev_warn(&pdev->dev,
			"failed to lock sc2703 test mode:%d\n", ret);

	/* Set target battery voltage 3800mv */
	ret = regmap_update_bits(info->regmap, SC2703_CHG_CTRL_A,
				 SC2703_VBAT_CHG_MASK, 0x00);
	if (ret)
		dev_warn(&pdev->dev,
			"failed to set sc2703 target vbat:%d\n", ret);
}

static int sc2703_charger_remove(struct platform_device *pdev)
{
	struct sc2703_charger_info *info = platform_get_drvdata(pdev);
	int ret;

	/* Disable charging/boost */
	ret = regmap_update_bits(info->regmap, SC2703_DCDC_CTRL_A,
				 SC2703_CHG_EN_MASK |
				 SC2703_OTG_EN_MASK, 0);
	if (ret) {
		dev_err(info->dev,
			 "Failed to disable charging/boost=%d\n", ret);
	}

	return ret;
}

static const struct of_device_id sc2703_charger_of_match[] = {
	{ .compatible = "sprd,sc2703-charger", },
	{ }
};

MODULE_DEVICE_TABLE(of, sc2703_charger_of_match);

static struct platform_driver sc2703_charger_driver = {
	.driver = {
		.name = "sc2703-charger",
		.of_match_table = sc2703_charger_of_match,
	},
	.probe = sc2703_charger_probe,
	.shutdown = sc2703_shutdown,
	.remove = sc2703_charger_remove,
};

module_platform_driver(sc2703_charger_driver);
MODULE_DESCRIPTION("Dialog SC2703 Charger Driver");
MODULE_LICENSE("GPL v2");
