/*
 * Driver for the FAIRCHILD fan54015 charger.
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
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/usb/phy.h>

#define FAN54015_REG_0					0x0
#define FAN54015_REG_1					0x1
#define FAN54015_REG_2					0x2
#define FAN54015_REG_3					0x3
#define FAN54015_REG_4					0x4
#define FAN54015_REG_5					0x5
#define FAN54015_REG_6					0x6
#define FAN54015_REG_10					0x10

#define FAN54015_REG_SAFETY_VOL_MASK			GENMASK(3, 0)
#define FAN54015_REG_SAFETY_CUR_MASK			GENMASK(6, 4)

#define FAN54015_REG_RESET_MASK				GENMASK(7, 7)
#define FAN54015_REG_RESET				BIT(7)

#define FAN54015_REG_WEAK_VOL_THRESHOLD_MASK		GENMASK(5, 4)

#define FAN54015_REG_IO_LEVEL_MASK			GENMASK(5, 5)

#define FAN54015_REG_VSP_MASK				GENMASK(2, 0)
#define FAN54015_REG_VSP				(BIT(3) | BIT(0))

#define FAN54015_REG_TERMINAL_CURRENT_MASK		GENMASK(3, 3)
#define FAN54015_REG_TERMINAL_VOLTAGE_MASK		GENMASK(7, 2)

#define FAN54015_REG_CHARGE_CONTROL_MASK		GENMASK(2, 2)
#define FAN54015_REG_CHARGE_DISABLE			BIT(2)
#define FAN54015_REG_CHARGE_ENABLE			0

#define FAN54015_REG_CURRENT_MASK			GENMASK(6, 4)
#define FAN54015_REG_CURRENT_MASK_SHIFT			4

#define FAN54015_REG_LIMIT_CURRENT_MASK			GENMASK(7, 6)
#define FAN54015_REG_LIMIT_CURRENT_SHIFT		6

struct fan54015_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct power_supply *psy_usb;
	struct work_struct work;
	struct mutex lock;
	bool charging;
	u32 limit;
};

static int fan54015_read(struct fan54015_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int fan54015_write(struct fan54015_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int fan54015_update_bits(struct fan54015_charger_info *info, u8 reg,
		u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = fan54015_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return fan54015_write(info, reg, v);
}

static int
fan54015_charger_set_safety_vol(struct fan54015_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < 4200)
		vol = 4200;
	if (vol > 4440)
		vol = 4440;
	reg_val = (vol - 4200) / 20 + 1;

	return fan54015_update_bits(info, FAN54015_REG_6,
				    FAN54015_REG_SAFETY_VOL_MASK, reg_val);
}

static int
fan54015_charger_set_termina_vol(struct fan54015_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < 3500)
		reg_val = 0x0;
	else if (vol >= 4440)
		reg_val = 0x2e;
	else
		reg_val = (vol - 3499) / 20;

	return fan54015_update_bits(info, FAN54015_REG_2,
				    FAN54015_REG_TERMINAL_VOLTAGE_MASK,
				    reg_val);
}


static int
fan54015_charger_set_safety_cur(struct fan54015_charger_info *info, u32 cur)
{
	u8 reg_val;

	if (cur < 650)
		reg_val = 0x0;
	else if (cur >= 650 && cur < 750)
		reg_val = 0x1;
	else if (cur >= 750 && cur < 850)
		reg_val = 0x2;
	else if (cur >= 850 && cur < 1050)
		reg_val = 0x3;
	else if (cur >= 1050 && cur < 1150)
		reg_val = 0x4;
	else if (cur >= 1150 && cur < 1350)
		reg_val = 0x5;
	else if (cur >= 1350 && cur < 1450)
		reg_val = 0x6;
	else if (cur >= 1450)
		reg_val = 0x7;

	return fan54015_update_bits(info, FAN54015_REG_6,
				    FAN54015_REG_SAFETY_CUR_MASK,
				    reg_val << FAN54015_REG_CURRENT_MASK_SHIFT);
}

static int fan54015_charger_hw_init(struct fan54015_charger_info *info)
{
	struct power_supply_battery_info bat_info = { };
	int voltage_max_microvolt, current_max_ua;
	int ret;

	ret = power_supply_get_battery_info(info->psy_usb, &bat_info);
	if (ret)
		dev_warn(info->dev, "no battery information is supplied\n");

	voltage_max_microvolt = bat_info.constant_charge_voltage_max_uv / 1000;
	current_max_ua = bat_info.constant_charge_current_max_ua / 1000;
	power_supply_put_battery_info(info->psy_usb, &bat_info);

	ret = fan54015_charger_set_safety_vol(info, voltage_max_microvolt);
	if (ret) {
		dev_err(info->dev, "set fan54015 safety vol failed\n");
		return ret;
	}

	ret = fan54015_charger_set_safety_cur(info, current_max_ua);
	if (ret) {
		dev_err(info->dev, "set fan54015 safety cur failed\n");
		return ret;
	}

	ret = fan54015_update_bits(info, FAN54015_REG_4,
				   FAN54015_REG_RESET_MASK, FAN54015_REG_RESET);
	if (ret) {
		dev_err(info->dev, "reset fan54015 failed\n");
		return ret;
	}

	ret = fan54015_update_bits(info, FAN54015_REG_1,
				   FAN54015_REG_WEAK_VOL_THRESHOLD_MASK, 0);
	if (ret) {
		dev_err(info->dev, "set fan54015 weak voltage threshold failed\n");
		return ret;
	}
	ret = fan54015_update_bits(info, FAN54015_REG_5,
				   FAN54015_REG_IO_LEVEL_MASK, 0);
	if (ret) {
		dev_err(info->dev, "set fan54015 io level failed\n");
		return ret;
	}

	ret = fan54015_update_bits(info, FAN54015_REG_5,
				   FAN54015_REG_VSP_MASK, FAN54015_REG_VSP);
	if (ret) {
		dev_err(info->dev, "set fan54015 vsp failed\n");
		return ret;
	}

	ret = fan54015_update_bits(info, FAN54015_REG_1,
				   FAN54015_REG_TERMINAL_CURRENT_MASK, 0);
	if (ret) {
		dev_err(info->dev, "set fan54015 terminal cur failed\n");
		return ret;
	}

	ret = fan54015_charger_set_termina_vol(info, voltage_max_microvolt);
	if (ret)
		dev_err(info->dev, "set fan54015 terminal vol failed\n");

	return ret;
}

static int fan54015_charger_start_charge(struct fan54015_charger_info *info)
{
	int ret;

	ret = fan54015_update_bits(info, FAN54015_REG_1,
				   FAN54015_REG_CHARGE_CONTROL_MASK,
				   FAN54015_REG_CHARGE_DISABLE);
	if (ret) {
		dev_err(info->dev, "disable fan54015 charge failed\n");
		return ret;
	}

	ret = fan54015_update_bits(info, FAN54015_REG_1,
				   FAN54015_REG_CHARGE_CONTROL_MASK,
				   FAN54015_REG_CHARGE_ENABLE);
	if (ret)
		dev_err(info->dev, "enable fan54015 charge failed\n");

	return ret;
}

static void fan54015_charger_stop_charge(struct fan54015_charger_info *info)
{
	int ret;

	ret = fan54015_update_bits(info, FAN54015_REG_1,
				   FAN54015_REG_CHARGE_CONTROL_MASK,
				   FAN54015_REG_CHARGE_DISABLE);
	if (ret)
		dev_err(info->dev, "disable fan54015 charge failed\n");
}

static int fan54015_charger_set_current(struct fan54015_charger_info *info,
					u32 cur)
{
	u8 reg_val;

	if (cur < 650)
		reg_val = 0x0;
	else if (cur >= 650 && cur < 750)
		reg_val = 0x1;
	else if (cur >= 750 && cur < 850)
		reg_val = 0x2;
	else if (cur >= 850 && cur < 1050)
		reg_val = 0x3;
	else if (cur >= 1050 && cur < 1150)
		reg_val = 0x4;
	else if (cur >= 1150 && cur < 1350)
		reg_val = 0x5;
	else if (cur >= 1350 && cur < 1450)
		reg_val = 0x6;
	else if (cur >= 1450)
		reg_val = 0x7;

	return fan54015_update_bits(info, FAN54015_REG_4,
				    FAN54015_REG_CURRENT_MASK,
				    reg_val);
}

static int fan54015_charger_get_current(struct fan54015_charger_info *info,
					u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = fan54015_read(info, FAN54015_REG_4, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= FAN54015_REG_CURRENT_MASK;
	reg_val = reg_val >> FAN54015_REG_CURRENT_MASK_SHIFT;

	switch (reg_val) {
	case 0:
		*cur = 550;
		break;
	case 1:
		*cur = 650;
		break;
	case 2:
		*cur = 750;
		break;
	case 3:
		*cur = 850;
		break;
	case 4:
		*cur = 1050;
		break;
	case 5:
		*cur = 1150;
		break;
	case 6:
		*cur = 1350;
		break;
	case 7:
		*cur = 1450;
		break;
	default:
		*cur = 550;
	}
	return 0;
}

static int
fan54015_charger_set_limit_current(struct fan54015_charger_info *info,
				   u32 limit_cur)
{
	u8 reg_val;
	int ret;

	if (limit_cur <= 100)
		reg_val = 0x0;
	else if (limit_cur > 100 && limit_cur <= 500)
		reg_val = 0x1;
	else if (limit_cur > 500 && limit_cur <= 800)
		reg_val = 0x2;
	else if (limit_cur > 800)
		reg_val = 0x3;

	ret = fan54015_update_bits(info, FAN54015_REG_1,
				   FAN54015_REG_LIMIT_CURRENT_MASK,
				   reg_val);
	if (ret)
		dev_err(info->dev, "set fan54015 limit cur failed\n");

	return ret;
}

static u32
fan54015_charger_get_limit_current(struct fan54015_charger_info *info,
				   u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = fan54015_read(info, FAN54015_REG_1, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= FAN54015_REG_LIMIT_CURRENT_MASK;
	reg_val = reg_val >> FAN54015_REG_LIMIT_CURRENT_SHIFT;

	switch (reg_val) {
	case 0:
		*limit_cur = 100;
		break;
	case 1:
		*limit_cur = 500;
		break;
	case 2:
		*limit_cur = 800;
		break;
	case 3:
		*limit_cur = 2000;
		break;
	default:
		*limit_cur = 100;
	}

	return 0;
}

static int fan54015_charger_get_health(struct fan54015_charger_info *info,
				     u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int fan54015_charger_get_online(struct fan54015_charger_info *info,
				     u32 *online)
{
	if (info->charging)
		*online = true;
	else
		*online = false;

	return 0;
}

static int fan54015_charger_get_status(struct fan54015_charger_info *info)
{
	if (info->charging == true)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_DISCHARGING;
}

static int fan54015_charger_set_status(struct fan54015_charger_info *info,
				       int val)
{
	u8 chg_en;
	int ret;

	switch (val) {
	case POWER_SUPPLY_STATUS_CHARGING:
		chg_en = FAN54015_REG_CHARGE_ENABLE;
		break;
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		chg_en = FAN54015_REG_CHARGE_DISABLE;
		break;
	default:
		return -EINVAL;
	}

	ret = fan54015_update_bits(info, FAN54015_REG_1,
				   FAN54015_REG_CHARGE_CONTROL_MASK,
				   chg_en);
	if (ret)
		dev_err(info->dev, "set fan54015 charger status failed\n");

	return ret;
}

static void fan54015_charger_work(struct work_struct *data)
{
	struct fan54015_charger_info *info =
		container_of(data, struct fan54015_charger_info, work);
	int ret;

	mutex_lock(&info->lock);

	if (info->limit > 0 && !info->charging) {
		/* set current limitation and start to charge */
		ret = fan54015_charger_set_limit_current(info, info->limit);
		if (ret)
			goto out;

		ret = fan54015_charger_set_current(info, info->limit);
		if (ret)
			goto out;

		ret = fan54015_charger_start_charge(info);
		if (ret)
			goto out;

		info->charging = true;
	} else if (!info->limit && info->charging) {
		/* Stop charging */
		info->charging = false;
		fan54015_charger_stop_charge(info);
	}

out:
	mutex_unlock(&info->lock);
}


static int fan54015_charger_usb_change(struct notifier_block *nb,
				       unsigned long limit, void *data)
{
	struct fan54015_charger_info *info =
		container_of(nb, struct fan54015_charger_info, usb_notify);

	info->limit = limit;

	schedule_work(&info->work);
	return NOTIFY_OK;
}

static int fan54015_charger_usb_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct fan54015_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, online, health;
	int ret = 0;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->charging)
			val->intval = fan54015_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = fan54015_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = fan54015_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = fan54015_charger_get_online(info, &online);
			if (ret)
				goto out;

			val->intval = online;
		}
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = fan54015_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;

	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int fan54015_charger_usb_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct fan54015_charger_info *info = power_supply_get_drvdata(psy);
	int ret;

	mutex_lock(&info->lock);

	if (!info->charging) {
		mutex_unlock(&info->lock);
		return -ENODEV;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = fan54015_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = fan54015_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_STATUS:
		ret = fan54015_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int fan54015_charger_property_is_writeable(struct power_supply *psy,
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

static enum power_supply_property fan54015_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
};

static const struct power_supply_desc fan54015_charger_desc = {
	.name			= "fan54015_charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= fan54015_usb_props,
	.num_properties		= ARRAY_SIZE(fan54015_usb_props),
	.get_property		= fan54015_charger_usb_get_property,
	.set_property		= fan54015_charger_usb_set_property,
	.property_is_writeable	= fan54015_charger_property_is_writeable,
};

static void fan54015_charger_detect_status(struct fan54015_charger_info *info)
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

static int fan54015_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct fan54015_charger_info *info;
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
	INIT_WORK(&info->work, fan54015_charger_work);

	info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(dev, "failed to find USB phy\n");
		return PTR_ERR(info->usb_phy);
	}

	info->usb_notify.notifier_call = fan54015_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		return ret;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy_usb = devm_power_supply_register(dev,
						   &fan54015_charger_desc,
						   &charger_cfg);
	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return PTR_ERR(info->psy_usb);
	}

	ret = fan54015_charger_hw_init(info);
	if (ret) {
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return ret;
	}
	fan54015_charger_detect_status(info);

	return 0;
}

static int fan54015_charger_remove(struct i2c_client *client)
{
	struct fan54015_charger_info *info = i2c_get_clientdata(client);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

static const struct i2c_device_id fan54015_i2c_id[] = {
	{"fan54015_chg", 0},
	{}
};

static const struct of_device_id fan54015_charger_of_match[] = {
	{ .compatible = "fairchild, fan54015_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, fan54015_charger_of_match);

static struct i2c_driver fan54015_charger_driver = {
	.driver = {
		.name = "fan54015_chg",
		.of_match_table = fan54015_charger_of_match,
	},
	.probe = fan54015_charger_probe,
	.remove = fan54015_charger_remove,
	.id_table = fan54015_i2c_id,
};

module_i2c_driver(fan54015_charger_driver);
MODULE_DESCRIPTION("FAN54015 Charger Driver");
MODULE_LICENSE("GPL v2");
