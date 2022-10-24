// SPDX-License-Identifier: GPL-2.0+
/*
 * ump96xx-usb-charger.c -- USB BC1.2 handling
 *
 * Copyright (C) 2022 Chen Yongzhi <yongzhi.chen@unisoc.com>
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/power/sprd-ump96xx-bc1p2.h>

static u32 det_delay_ms;
static struct ump96xx_bc1p2 *bc1p2;

static int sprd_bc1p2_redetect_control(bool enable)
{
	int ret;

	if (enable)
		ret = regmap_update_bits(bc1p2->regmap, bc1p2->chg_bc1p2_ctrl2,
					 UMP96XX_CHG_DET_EB_MASK,
					 UMP96XX_CHG_BC1P2_REDET_ENABLE);
	else
		ret = regmap_update_bits(bc1p2->regmap, bc1p2->chg_bc1p2_ctrl2,
					 UMP96XX_CHG_DET_EB_MASK,
					 UMP96XX_CHG_BC1P2_REDET_DISABLE);

	if (ret)
		pr_info("fail to %s, enable = %d\n", __func__, enable);
	return ret;
}

static enum usb_charger_type sprd_bc1p2_detect(void)
{
	enum usb_charger_type type;
	u32 status = 0, val;
	int ret, cnt = UMP96XX_CHG_DET_RETRY_COUNT;

	cnt += det_delay_ms / UMP96XX_CHG_DET_DELAY_MS;
	det_delay_ms = 0;

	do {
		ret = regmap_read(bc1p2->regmap, bc1p2->charge_status, &val);
		if (ret) {
			sprd_bc1p2_redetect_control(false);
			return UNKNOWN_TYPE;
		}

		if (!(val & BIT_CHGR_INT) && cnt < UMP96XX_CHG_DET_RETRY_COUNT) {
			sprd_bc1p2_redetect_control(false);
			return UNKNOWN_TYPE;
		}

		if (val & BIT_CHG_DET_DONE) {
			status = val & (BIT_CDP_INT | BIT_DCP_INT | BIT_SDP_INT);
			break;
		}

		msleep(UMP96XX_CHG_DET_DELAY_MS);
	} while (--cnt > 0);

	switch (status) {
	case BIT_CDP_INT:
		type = CDP_TYPE;
		break;
	case BIT_DCP_INT:
		type = DCP_TYPE;
		break;
	case BIT_SDP_INT:
		type = SDP_TYPE;
		break;
	default:
		type = UNKNOWN_TYPE;
	}

	sprd_bc1p2_redetect_control(false);
	return type;
}

static int sprd_bc1p2_redetect_trigger(u32 time_ms)
{
	int ret;
	u32 reg_val;

	if (time_ms > UMP96XX_CHG_DET_DELAY_MS_MAX)
		time_ms = UMP96XX_CHG_DET_DELAY_MS_MAX;

	reg_val = time_ms / UMP96XX_CHG_DET_DELAY_STEP_MS;
	ret = regmap_update_bits(bc1p2->regmap, bc1p2->chg_det_fgu_ctrl,
				 UMP96XX_CHG_DET_DELAY_MASK,
				 reg_val << UMP96XX_CHG_DET_DELAY_OFFSET);
	if (ret)
		return UMP96XX_ERROR_REGMAP_UPDATE;

	ret = sprd_bc1p2_redetect_control(true);
	if (ret)
		return UMP96XX_ERROR_REGMAP_UPDATE;

	msleep(UMP96XX_CHG_DET_DELAY_MS);
	ret = regmap_read(bc1p2->regmap, bc1p2->charge_status, &reg_val);
	if (ret)
		return UMP96XX_ERROR_REGMAP_READ;

	if (!(reg_val & BIT_CHGR_INT))
		return UMP96XX_ERROR_CHARGER_INIT;

	if (reg_val & BIT_CHG_DET_DONE)
		return UMP96XX_ERROR_CHARGER_DETDONE;

	det_delay_ms = time_ms - UMP96XX_CHG_DET_DELAY_MS;

	return UMP96XX_ERROR_NO_ERROR;
}

static enum usb_charger_type sprd_bc1p2_retry_detect(struct usb_phy *x)
{
	enum usb_charger_type type = UNKNOWN_TYPE;
	int ret = 0;

	if (x->chg_state != USB_CHARGER_PRESENT) {
		dev_warn(x->dev, "unplug usb during redetect\n");
		return UNKNOWN_TYPE;
	}

	ret = sprd_bc1p2_redetect_trigger(UMP96XX_CHG_REDET_DELAY_MS);
	if (ret) {
		sprd_bc1p2_redetect_control(false);
		if (ret == UMP96XX_ERROR_CHARGER_INIT)
			dev_warn(x->dev, "USB connection is unstable during redetect bc1p2\n");
		else
			dev_err(x->dev, "trigger redetect bc1p2 failed, error= %d\n", ret);
		return UNKNOWN_TYPE;
	}

	type = sprd_bc1p2_detect();

	return type;
}

static void usb_phy_set_default_current(struct usb_phy *x)
{
	x->chg_cur.sdp_min = DEFAULT_SDP_CUR_MIN;
	x->chg_cur.sdp_max = DEFAULT_SDP_CUR_MAX;
	x->chg_cur.dcp_min = DEFAULT_DCP_CUR_MIN;
	x->chg_cur.dcp_max = DEFAULT_DCP_CUR_MAX;
	x->chg_cur.cdp_min = DEFAULT_CDP_CUR_MIN;
	x->chg_cur.cdp_max = DEFAULT_CDP_CUR_MAX;
	x->chg_cur.aca_min = DEFAULT_ACA_CUR_MIN;
	x->chg_cur.aca_max = DEFAULT_ACA_CUR_MAX;
}

static void usb_phy_notify_charger(struct usb_phy *x)
{
	unsigned int min, max;

	switch (x->chg_state) {
	case USB_CHARGER_PRESENT:
		usb_phy_get_charger_current(x, &min, &max);

		atomic_notifier_call_chain(&x->notifier, max, x);
		break;
	case USB_CHARGER_ABSENT:
		usb_phy_set_default_current(x);

		atomic_notifier_call_chain(&x->notifier, 0, x);
		break;
	default:
		dev_warn(x->dev, "Unknown USB charger state: %d\n",
			 x->chg_state);
		return;
	}

	kobject_uevent(&x->dev->kobj, KOBJ_CHANGE);
}

enum usb_charger_type sprd_bc1p2_charger_detect(struct usb_phy *x)
{
	enum usb_charger_type type = UNKNOWN_TYPE;

	if (!bc1p2) {
		pr_err("%s:line%d: phy NULL pointer!!!\n", __func__, __LINE__);
		return UNKNOWN_TYPE;
	}

	mutex_lock(&bc1p2->bc1p2_lock);
	if (x->chg_state != USB_CHARGER_PRESENT) {
		mutex_unlock(&bc1p2->bc1p2_lock);
		return UNKNOWN_TYPE;
	}

	type = sprd_bc1p2_detect();
	if (x->chg_state != USB_CHARGER_PRESENT) {
		mutex_unlock(&bc1p2->bc1p2_lock);
		return UNKNOWN_TYPE;
	}

	if (type == UNKNOWN_TYPE) {
		x->chg_type = UNKNOWN_TYPE;
		dev_info(x->dev, "first_detect:type:0x%x\n", x->chg_type);
		usb_phy_notify_charger(x);
		type = sprd_bc1p2_retry_detect(x);
		dev_info(x->dev, "retry detected charger type:0x%x\n", type);
	}

	mutex_unlock(&bc1p2->bc1p2_lock);
	return type;
}
EXPORT_SYMBOL_GPL(sprd_bc1p2_charger_detect);

void sprd_bc1p2_notify_charger(struct usb_phy *x)
{
	if (!x->charger_detect)
		return;

	switch (x->chg_state) {
	case USB_CHARGER_PRESENT:
		x->chg_type = x->charger_detect(x);
		if (x->chg_state == USB_CHARGER_ABSENT) {
			x->chg_type = UNKNOWN_TYPE;
			dev_info(x->dev, "detected bc1p2 type:0x%x, absent\n", x->chg_type);
		} else if (x->chg_type != UNKNOWN_TYPE) {
			dev_info(x->dev, "detected bc1p2 type:0x%x\n", x->chg_type);
		}

		break;
	case USB_CHARGER_ABSENT:
		x->chg_type = UNKNOWN_TYPE;
		break;
	default:
		dev_warn(x->dev, "Unknown USB charger state: %d\n", x->chg_state);
		return;
	}
	usb_phy_notify_charger(x);
}
EXPORT_SYMBOL_GPL(sprd_bc1p2_notify_charger);

static int ump96xx_bc1p2_probe(struct platform_device *pdev)
{
	int err;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	bc1p2 = devm_kzalloc(dev, sizeof(struct ump96xx_bc1p2), GFP_KERNEL);
	if (!bc1p2)
		return -ENOMEM;

	err = of_property_read_u32_index(np, "reg", 0, &bc1p2->charge_status);
	if (err) {
		dev_err(dev, "failed to get charge_status\n");
		bc1p2->charge_status = 0;
		return err;
	}

	err = of_property_read_u32_index(np, "reg", 1, &bc1p2->chg_det_fgu_ctrl);
	if (err) {
		dev_err(dev, "no chg_det_fgu_ctrl setting\n");
		bc1p2->chg_det_fgu_ctrl = 0;
		return err;
	}

	err = of_property_read_u32_index(np, "reg", 2, &bc1p2->chg_bc1p2_ctrl2);
	if (err) {
		dev_err(dev, "no chg_bc1p2_ctrl2 setting\n");
		bc1p2->chg_bc1p2_ctrl2 = 0;
		return err;
	}

	mutex_init(&bc1p2->bc1p2_lock);
	bc1p2->regmap = dev_get_regmap(dev->parent, NULL);
	if (!bc1p2->regmap) {
		dev_err(dev, "failed to get regmap\n");
		mutex_destroy(&bc1p2->bc1p2_lock);
		return -ENODEV;
	}

	return err;
}

static const struct of_device_id ump96xx_bc1p2_of_match[] = {
	{ .compatible = "sprd,ump9620-bc1p2", },
	{ }
};

MODULE_DEVICE_TABLE(of, ump96xx_bc1p2_of_match);

static struct platform_driver ump96xx_bc1p2_driver = {
	.driver = {
		.name = "ump96xx-bc1p2",
		.of_match_table = ump96xx_bc1p2_of_match,
	 },
	.probe = ump96xx_bc1p2_probe,
};

module_platform_driver(ump96xx_bc1p2_driver);

MODULE_AUTHOR("Yongzhi Chen <yongzhi.chen@unisoc.com>");
MODULE_DESCRIPTION("ump96xx bc1p2 driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:ump96xx_bc1p2");

