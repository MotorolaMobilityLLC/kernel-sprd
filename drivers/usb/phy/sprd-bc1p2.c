// SPDX-License-Identifier: GPL-2.0+
/*
 * sprd-bc1p2.c -- USB BC1.2 handling
 *
 * Copyright (C) 2022 Chen Yongzhi <yongzhi.chen@unisoc.com>
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/power/sprd-bc1p2.h>

static const struct sprd_bc1p2_data sc2720_data = {
	.charge_status = SC2720_CHARGE_STATUS,
	.chg_det_fgu_ctrl = SC2720_CHG_DET_FGU_CTRL,
	.chg_bc1p2_ctrl2 = 0,
	.chg_int_delay_mask = SC27XX_CHG_INT_DELAY_MASK,
	.chg_int_delay_offset = SC27XX_CHG_INT_DELAY_OFFSET,
};

static const struct sprd_bc1p2_data sc2721_data = {
	.charge_status = SC2721_CHARGE_STATUS,
	.chg_det_fgu_ctrl = SC2721_CHG_DET_FGU_CTRL,
	.chg_bc1p2_ctrl2 = 0,
	.chg_int_delay_mask = SC27XX_CHG_INT_DELAY_MASK,
	.chg_int_delay_offset = SC27XX_CHG_INT_DELAY_OFFSET,
};

static const struct sprd_bc1p2_data sc2730_data = {
	.charge_status = SC2730_CHARGE_STATUS,
	.chg_det_fgu_ctrl = SC2730_CHG_DET_FGU_CTRL,
	.chg_bc1p2_ctrl2 = 0,
	.chg_int_delay_mask = SC27XX_CHG_INT_DELAY_MASK,
	.chg_int_delay_offset = SC27XX_CHG_INT_DELAY_OFFSET,
};

static const struct sprd_bc1p2_data sc2731_data = {
	.charge_status = SC2731_CHARGE_STATUS,
	.chg_det_fgu_ctrl = SC2731_CHG_DET_FGU_CTRL,
	.chg_bc1p2_ctrl2 = 0,
	.chg_int_delay_mask = SC27XX_CHG_INT_DELAY_MASK,
	.chg_int_delay_offset = SC27XX_CHG_INT_DELAY_OFFSET,
};

static const struct sprd_bc1p2_data ump9620_data = {
	.charge_status = UMP9620_CHARGE_STATUS,
	.chg_det_fgu_ctrl = UMP9620_CHG_DET_FGU_CTRL,
	.chg_bc1p2_ctrl2 = UMP9620_CHG_BC1P2_CTRL2,
	.chg_int_delay_mask = UMP96XX_CHG_INT_DELAY_MASK,
	.chg_int_delay_offset = UMP96XX_CHG_INT_DELAY_OFFSET,
};

static const struct sprd_bc1p2_data ump518_data = {
	.charge_status = SC2730_CHARGE_STATUS,
	.chg_det_fgu_ctrl = SC2730_CHG_DET_FGU_CTRL,
	.chg_bc1p2_ctrl2 = UMP518_CHG_BC1P2_CTRL2,
	.chg_int_delay_mask = UMP96XX_CHG_INT_DELAY_MASK,
	.chg_int_delay_offset = UMP96XX_CHG_INT_DELAY_OFFSET,
};

static u32 det_delay_ms;
static struct sprd_bc1p2 *bc1p2;

static int sprd_bc1p2_redetect_control(bool enable)
{
	int ret;

	if (enable)
		ret = regmap_update_bits(bc1p2->regmap, bc1p2->data->chg_bc1p2_ctrl2,
					 UMP96XX_CHG_DET_EB_MASK,
					 UMP96XX_CHG_BC1P2_REDET_ENABLE);
	else
		ret = regmap_update_bits(bc1p2->regmap, bc1p2->data->chg_bc1p2_ctrl2,
					 UMP96XX_CHG_DET_EB_MASK,
					 UMP96XX_CHG_BC1P2_REDET_DISABLE);

	if (ret)
		pr_info("fail to %s, enable = %d\n", __func__, enable);
	return ret;
}

static enum usb_charger_type sprd_bc1p2_detect(void)
{
	enum usb_charger_type type;
	u32 status = 0, val = 0;
	int ret, cnt = UMP96XX_CHG_DET_RETRY_COUNT;

	cnt += det_delay_ms / UMP96XX_CHG_DET_DELAY_MS;
	det_delay_ms = 0;

	do {
		ret = regmap_read(bc1p2->regmap, bc1p2->data->charge_status, &val);
		if (ret) {
			type = UNKNOWN_TYPE;
			goto bc1p2_detect_end;
		}

		if (!(val & BIT_CHGR_INT) && cnt < UMP96XX_CHG_DET_RETRY_COUNT) {
			type = UNKNOWN_TYPE;
			goto bc1p2_detect_end;
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

bc1p2_detect_end:
	if (val & BIT_CHGR_INT)
		bc1p2->chg_state = USB_CHARGER_PRESENT;
	else
		bc1p2->chg_state = USB_CHARGER_ABSENT;
	if (bc1p2->redetect_enable)
		sprd_bc1p2_redetect_control(false);
	return type;
}

static enum usb_charger_type sprd_bc1p2_try_once_detect(void)
{
	enum usb_charger_type type = UNKNOWN_TYPE;
	u32 status = 0, val = 0;
	int ret;

	ret = regmap_read(bc1p2->regmap, bc1p2->data->charge_status, &val);
	if (ret)
		return UNKNOWN_TYPE;

	if (!(val & BIT_CHGR_INT))
		return UNKNOWN_TYPE;

	if (val & BIT_CHG_DET_DONE) {
		status = val & (BIT_CDP_INT | BIT_DCP_INT | BIT_SDP_INT);
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
	}

	return type;
}

static int sprd_bc1p2_redetect_trigger(u32 time_ms)
{
	int ret;
	u32 reg_val;

	if (time_ms > UMP96XX_CHG_DET_DELAY_MS_MAX)
		time_ms = UMP96XX_CHG_DET_DELAY_MS_MAX;

	reg_val = time_ms / UMP96XX_CHG_DET_DELAY_STEP_MS;
	ret = regmap_update_bits(bc1p2->regmap, bc1p2->data->chg_det_fgu_ctrl,
				 UMP96XX_CHG_REDET_DELAY_MASK,
				 reg_val << UMP96XX_CHG_REDET_DELAY_OFFSET);
	if (ret)
		return UMP96XX_ERROR_REGMAP_UPDATE;

	ret = sprd_bc1p2_redetect_control(true);
	if (ret)
		return UMP96XX_ERROR_REGMAP_UPDATE;

	msleep(UMP96XX_CHG_DET_DELAY_MS);
	ret = regmap_read(bc1p2->regmap, bc1p2->data->charge_status, &reg_val);
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

static void sprd_get_bc1p2_type_work(struct kthread_work *work)
{
	struct sprd_bc1p2_priv *bc1p2_info = container_of(work, struct sprd_bc1p2_priv,
	bc1p2_kwork);

	spin_lock(&bc1p2_info->vbus_event_lock);
	while (bc1p2_info->vbus_events) {
		bc1p2_info->vbus_events = false;
		spin_unlock(&bc1p2_info->vbus_event_lock);
		sprd_bc1p2_notify_charger(bc1p2_info->phy);
		spin_lock(&bc1p2_info->vbus_event_lock);
	}

	spin_unlock(&bc1p2_info->vbus_event_lock);
}

void sprd_usb_changed(struct sprd_bc1p2_priv *bc1p2_info, enum usb_charger_state state)
{
	struct usb_phy *usb_phy = bc1p2_info->phy;

	spin_lock(&bc1p2_info->vbus_event_lock);
	bc1p2_info->vbus_events = true;

	usb_phy->chg_state = state;
	if (usb_phy->chg_state == USB_CHARGER_ABSENT)
		usb_phy->flags &= ~CHARGER_DETECT_DONE;
	spin_unlock(&bc1p2_info->vbus_event_lock);

	if (bc1p2_info->bc1p2_thread)
		kthread_queue_work(&bc1p2_info->bc1p2_kworker, &bc1p2_info->bc1p2_kwork);
}
EXPORT_SYMBOL_GPL(sprd_usb_changed);

int usb_add_bc1p2_init(struct sprd_bc1p2_priv *bc1p2_info, struct usb_phy *x)
{
	struct sched_param param = { .sched_priority = 1 };

	if (!x->dev) {
		dev_err(x->dev, "no device provided for PHY\n");
		return -EINVAL;
	}

	bc1p2_info->phy = x;
	spin_lock_init(&bc1p2_info->vbus_event_lock);
	kthread_init_worker(&bc1p2_info->bc1p2_kworker);
	kthread_init_work(&bc1p2_info->bc1p2_kwork, sprd_get_bc1p2_type_work);
	bc1p2_info->bc1p2_thread = kthread_run(kthread_worker_fn, &bc1p2_info->bc1p2_kworker,
					       "sprd_bc1p2_worker");
	if (IS_ERR(bc1p2_info->bc1p2_thread)) {
		bc1p2_info->bc1p2_thread = NULL;
		dev_err(x->dev, "failed to run bc1p2_thread\n");
		return PTR_ERR(bc1p2_info->bc1p2_thread);
	}

	sched_setscheduler(bc1p2_info->bc1p2_thread, SCHED_FIFO, &param);

	return 0;
}
EXPORT_SYMBOL_GPL(usb_add_bc1p2_init);

void usb_remove_bc1p2(struct sprd_bc1p2_priv *bc1p2_info)
{
	if (bc1p2_info->bc1p2_thread) {
		kthread_flush_worker(&bc1p2_info->bc1p2_kworker);
		kthread_stop(bc1p2_info->bc1p2_thread);
		bc1p2_info->bc1p2_thread = NULL;
	}
}
EXPORT_SYMBOL_GPL(usb_remove_bc1p2);

void usb_phy_notify_charger(struct usb_phy *x)
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
EXPORT_SYMBOL_GPL(usb_phy_notify_charger);

enum usb_charger_type sprd_bc1p2_charger_detect(struct usb_phy *x)
{
	enum usb_charger_type type = UNKNOWN_TYPE;

	if (!bc1p2) {
		pr_err("%s:line%d: bc1p2 NULL pointer!!!\n", __func__, __LINE__);
		return UNKNOWN_TYPE;
	}

	mutex_lock(&bc1p2->bc1p2_lock);

	if (x->flags & CHARGER_DETECT_DONE) {
		if (bc1p2->type == UNKNOWN_TYPE)
			bc1p2->type = sprd_bc1p2_try_once_detect();
		dev_info(x->dev, "%s:line%d: type = %d\n", __func__,  __LINE__, bc1p2->type);
		mutex_unlock(&bc1p2->bc1p2_lock);
		return bc1p2->type;
	}

	type = sprd_bc1p2_detect();
	if (bc1p2->chg_state != USB_CHARGER_PRESENT) {
		dev_info(x->dev, "%s:line%d: type = UNKNOWN_TYPE\n", __func__,  __LINE__);
		type = UNKNOWN_TYPE;
		goto bc1p2_done;
	}

	if (!bc1p2->redetect_enable) {
		x->flags |= CHARGER_2NDDETECT_ENABLE | CHARGER_DETECT_DONE;
		dev_info(x->dev, "%s:line%d: type = %d\n", __func__,  __LINE__, type);
		goto bc1p2_done;
	}

	if (type == UNKNOWN_TYPE) {
		x->chg_type = UNKNOWN_TYPE;
		dev_info(x->dev, "first_detect:type:0x%x\n", x->chg_type);
		usb_phy_notify_charger(x);
		type = sprd_bc1p2_retry_detect(x);
	}

	if (bc1p2->chg_state != USB_CHARGER_PRESENT) {
		dev_info(x->dev, "%s:line%d: type = UNKNOWN_TYPE\n", __func__,  __LINE__);
		type = UNKNOWN_TYPE;
		goto bc1p2_done;
	}

	x->flags |= CHARGER_DETECT_DONE;
	dev_info(x->dev, "%s:line%d: type = %d\n", __func__,  __LINE__, type);
bc1p2_done:
	bc1p2->type = type;
	mutex_unlock(&bc1p2->bc1p2_lock);
	return bc1p2->type;
}
EXPORT_SYMBOL_GPL(sprd_bc1p2_charger_detect);

void sprd_bc1p2_notify_charger(struct usb_phy *x)
{
	if (!bc1p2 || !x->charger_detect) {
		pr_err("%s:line%d: charger_detect NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	switch (x->chg_state) {
	case USB_CHARGER_PRESENT:
		x->chg_type = x->charger_detect(x);
		if (x->chg_state == USB_CHARGER_ABSENT) {
			x->chg_type = UNKNOWN_TYPE;
			dev_info(x->dev, "detected bc1p2 type:0x%x, absent\n", x->chg_type);
		} else {
			if (bc1p2->redetect_enable && (x->chg_type == UNKNOWN_TYPE) &&
			    (x->flags & CHARGER_DETECT_DONE))
				return;
			dev_info(x->dev, "detected bc1p2 type:0x%x\n", x->chg_type);
		}

		break;
	case USB_CHARGER_ABSENT:
		dev_info(x->dev, "usb_charger_absent\n");
		x->chg_type = UNKNOWN_TYPE;
		break;
	default:
		dev_warn(x->dev, "Unknown USB charger state: %d\n", x->chg_state);
		return;
	}

	usb_phy_notify_charger(x);
}
EXPORT_SYMBOL_GPL(sprd_bc1p2_notify_charger);

static int sprd_bc1p2_probe(struct platform_device *pdev)
{
	int err = 0;
	struct device *dev = &pdev->dev;

	bc1p2 = devm_kzalloc(dev, sizeof(struct sprd_bc1p2), GFP_KERNEL);
	if (!bc1p2)
		return -ENOMEM;

	bc1p2->data = of_device_get_match_data(dev);
	if (!bc1p2->data) {
		dev_err(dev, "no matching driver data found\n");
		return -EINVAL;
	}

	bc1p2->regmap = dev_get_regmap(dev->parent, NULL);
	if (!bc1p2->regmap) {
		dev_err(dev, "failed to get regmap\n");
		return -ENODEV;
	}

	err = regmap_update_bits(bc1p2->regmap, bc1p2->data->chg_det_fgu_ctrl,
				 bc1p2->data->chg_int_delay_mask,
				 CHG_INT_DELAY_128MS << bc1p2->data->chg_int_delay_offset);
	if (err)
		return err;

	if (bc1p2->data->chg_bc1p2_ctrl2 == 0) {
		dev_warn(dev, "no support chg_bc1p2 redetect\n");
		bc1p2->redetect_enable = false;
	} else {
		bc1p2->redetect_enable = true;
	}

	mutex_init(&bc1p2->bc1p2_lock);

	return err;
}

static const struct of_device_id sprd_bc1p2_of_match[] = {
	{ .compatible = "sprd,sc2720-bc1p2", .data = &sc2720_data},
	{ .compatible = "sprd,sc2721-bc1p2", .data = &sc2721_data},
	{ .compatible = "sprd,sc2730-bc1p2", .data = &sc2730_data},
	{ .compatible = "sprd,sc2731-bc1p2", .data = &sc2731_data},
	{ .compatible = "sprd,ump9620-bc1p2", .data = &ump9620_data},
	{ .compatible = "sprd,ump518-bc1p2", .data = &ump518_data},
	{ }
};

MODULE_DEVICE_TABLE(of, sprd_bc1p2_of_match);

static struct platform_driver sprd_bc1p2_driver = {
	.driver = {
		.name = "sprd-bc1p2",
		.of_match_table = sprd_bc1p2_of_match,
	 },
	.probe = sprd_bc1p2_probe,
};

module_platform_driver(sprd_bc1p2_driver);

MODULE_AUTHOR("Yongzhi Chen <yongzhi.chen@unisoc.com>");
MODULE_DESCRIPTION("sprd bc1p2 driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sprd_bc1p2");
