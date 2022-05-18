// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 Unisoc Co., Ltd.
 * Jinfeng.Lin <Jinfeng.Lin1@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_wakeup.h>
#include <linux/power/charger-manager.h>
#include <linux/power/sprd_vchg_detect.h>

static enum power_supply_usb_type sprd_vchg_get_bc1p2_type(struct sprd_vchg_info *info)
{
	enum power_supply_usb_type usb_type  = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	enum usb_charger_type type;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", SPRD_VCHG_TAG, __LINE__);
		return usb_type;
	}

	type = info->usb_phy->chg_type;

	switch (type) {
	case SDP_TYPE:
		usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;

	case DCP_TYPE:
		usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;

	case CDP_TYPE:
		usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;

	default:
		usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	}

	return usb_type;
}

static bool sprd_vchg_is_charger_online(struct sprd_vchg_info *info)
{
	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", SPRD_VCHG_TAG, __LINE__);
		return false;
	}

	if (info->limit)
		return true;

	return false;
}

static void sprd_vchg_work(struct work_struct *data)
{
	struct sprd_vchg_info *info = container_of(data, struct sprd_vchg_info,
						   sprd_vchg_work);

	dev_info(info->dev, "%s: charger type = %d, limit = %d\n",
		 SPRD_VCHG_TAG, info->usb_phy->chg_type, info->limit);

	cm_notify_event(info->psy, CM_EVENT_CHG_START_STOP, NULL);
}

static int sprd_vchg_change(struct notifier_block *nb, unsigned long limit, void *data)
{
	struct sprd_vchg_info *info = container_of(nb, struct sprd_vchg_info, usb_notify);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", SPRD_VCHG_TAG, __LINE__);
		return NOTIFY_OK;
	}

	if (!info->pd_hard_reset) {
		info->limit = limit;
		if (info->typec_online) {
			info->typec_online = false;
			dev_info(info->dev, "%s, typec not disconnect while pd hard reset.\n",
				 SPRD_VCHG_TAG);
		}

		__pm_wakeup_event(info->sprd_vchg_ws, SPRD_VCHG_WAKE_UP_MS);
		schedule_work(&info->sprd_vchg_work);
	} else {
		info->pd_hard_reset = false;
		if (info->usb_phy->chg_state == USB_CHARGER_PRESENT) {
			dev_err(info->dev, "%s: The adapter is not PD adapter.\n", SPRD_VCHG_TAG);
			info->limit = limit;
			__pm_wakeup_event(info->sprd_vchg_ws, SPRD_VCHG_WAKE_UP_MS);
			schedule_work(&info->sprd_vchg_work);
		} else if (!extcon_get_state(info->typec_extcon, EXTCON_USB)) {
			dev_err(info->dev, "%s: typec disconnect before pd hard reset.\n", SPRD_VCHG_TAG);
			info->limit = 0;
			__pm_wakeup_event(info->sprd_vchg_ws, SPRD_VCHG_WAKE_UP_MS);
			schedule_work(&info->sprd_vchg_work);
		} else {
			info->typec_online = true;
			dev_err(info->dev, "%s, USB PD hard reset, ignore vbus off\n", SPRD_VCHG_TAG);
			cancel_delayed_work_sync(&info->pd_hard_reset_work);
		}
	}

	return NOTIFY_OK;
}

static void sprd_vchg_detect_status(struct sprd_vchg_info *info)
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

	schedule_work(&info->sprd_vchg_work);
}

#if IS_ENABLED(CONFIG_SC27XX_PD)
static int sprd_vchg_pd_extcon_event(struct notifier_block *nb, unsigned long event, void *param)
{
	struct sprd_vchg_info *info = container_of(nb, struct sprd_vchg_info, pd_extcon_nb);
	int pd_extcon_status;


	if (info->pd_hard_reset) {
		dev_info(info->dev, "%s: Already receive USB PD hardreset\n", SPRD_VCHG_TAG);
		return NOTIFY_OK;
	}

	pd_extcon_status = extcon_get_state(info->pd_extcon, EXTCON_CHG_USB_PD);
	if (pd_extcon_status == info->pd_extcon_status)
		return NOTIFY_OK;

	info->pd_extcon_status = pd_extcon_status;

	info->pd_hard_reset = true;

	dev_info(info->dev, "%s: receive USB PD hard reset request\n", SPRD_VCHG_TAG);

	schedule_delayed_work(&info->pd_hard_reset_work,
			      msecs_to_jiffies(SPRD_VCHG_PD_HARD_RESET_MS));

	return NOTIFY_OK;
}

static int sprd_vchg_typec_extcon_event(struct notifier_block *nb, unsigned long event, void *param)
{
	struct sprd_vchg_info *info = container_of(nb, struct sprd_vchg_info, typec_extcon_nb);

	if (info->typec_online) {
		dev_info(info->dev, "%s: typec status change.\n", SPRD_VCHG_TAG);
		schedule_delayed_work(&info->typec_extcon_work, 0);
	}
	return NOTIFY_OK;
}

static void sprd_vchg_pd_hard_reset_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sprd_vchg_info *info = container_of(dwork, struct sprd_vchg_info,
						   pd_hard_reset_work);

	if (!info->pd_hard_reset) {
		if (info->usb_phy->chg_state == USB_CHARGER_PRESENT)
			return;

		dev_info(info->dev, "%s: Not USB PD hard reset, charger out\n", SPRD_VCHG_TAG);

		info->limit = 0;
		schedule_work(&info->sprd_vchg_work);
	}

	info->pd_hard_reset = false;
}

static void sprd_vchg_typec_extcon_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sprd_vchg_info *info = container_of(dwork, struct sprd_vchg_info,
						   typec_extcon_work);

	if (!extcon_get_state(info->typec_extcon, EXTCON_USB)) {
		info->limit = 0;
		info->typec_online = false;
		__pm_wakeup_event(info->sprd_vchg_ws, SPRD_VCHG_WAKE_UP_MS);
		dev_info(info->dev, "%s: typec disconnect while pd hard reset.\n", SPRD_VCHG_TAG);
		schedule_work(&info->sprd_vchg_work);
	}
}

static void sprd_vchg_detect_pd_extcon_status(struct sprd_vchg_info *info)
{
	if (!info->pd_extcon_enable)
		return;

	info->pd_extcon_status = extcon_get_state(info->pd_extcon, EXTCON_CHG_USB_PD);
	if (info->pd_extcon_status) {
		info->pd_hard_reset = true;
		dev_info(info->dev, "%s: Detect USB PD hard reset request\n", SPRD_VCHG_TAG);
		schedule_delayed_work(&info->pd_hard_reset_work,
				      msecs_to_jiffies(SPRD_VCHG_PD_HARD_RESET_MS));
	}
}

#else
static void sprd_vchg_detect_pd_extcon_status(struct sprd_vchg_info *info)
{
}
#endif
static int sprd_vchg_detect_parse_dts(struct sprd_vchg_info *info)
{
	if (!info) {
		pr_err("%s:line%d: cm NULL pointer!!!\n", SPRD_VCHG_TAG, __LINE__);
		return -ENOMEM;
	}

	info->usb_phy = devm_usb_get_phy_by_phandle(info->dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(info->dev, "%s: failed to find USB phy\n", SPRD_VCHG_TAG);
		return -EINVAL;
	}

	info->pd_extcon_enable = device_property_read_bool(info->dev, "pd-extcon-enable");

	return 0;
}

static int sprd_vchg_detect_init(struct sprd_vchg_info *info, struct power_supply *psy)
{
	struct device *dev;
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", SPRD_VCHG_TAG, __LINE__);
		return -ENOMEM;
	}

	dev = info->dev;
	info->sprd_vchg_ws = wakeup_source_create("sprd_vchg_wakelock");
	wakeup_source_add(info->sprd_vchg_ws);

	info->psy = psy;
	INIT_WORK(&info->sprd_vchg_work, sprd_vchg_work);

	info->usb_notify.notifier_call = sprd_vchg_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "%s:failed to register notifier:%d\n", SPRD_VCHG_TAG, ret);
		ret = -EINVAL;
		goto remove_wakeup;
	}

#if IS_ENABLED(CONFIG_SC27XX_PD)
	if (!info->pd_extcon_enable)
		return 0;

	if (!of_property_read_bool(dev->of_node, "extcon"))
		return 0;

	INIT_DELAYED_WORK(&info->pd_hard_reset_work, sprd_vchg_pd_hard_reset_work);

	info->pd_extcon = extcon_get_edev_by_phandle(dev, 1);
	if (IS_ERR(info->pd_extcon)) {
		dev_err(dev, "%s: failed to find pd extcon device.\n", SPRD_VCHG_TAG);
		ret = -EINVAL;
		goto err_reg_usb;
	}

	info->pd_extcon_nb.notifier_call = sprd_vchg_pd_extcon_event;
	ret = devm_extcon_register_notifier_all(dev, info->pd_extcon, &info->pd_extcon_nb);
	if (ret) {
		dev_err(dev, "%s:Can't register pd extcon\n", SPRD_VCHG_TAG);
		ret = -EINVAL;
		goto err_reg_usb;
	}

	INIT_DELAYED_WORK(&info->typec_extcon_work, sprd_vchg_typec_extcon_work);
	info->typec_extcon = extcon_get_edev_by_phandle(dev, 0);
	if (IS_ERR(info->typec_extcon)) {
		dev_err(dev, "%s: failed to find typec extcon device.\n", SPRD_VCHG_TAG);
		ret = -EINVAL;
		goto err_reg_usb;
	}

	info->typec_extcon_nb.notifier_call = sprd_vchg_typec_extcon_event;
	ret = devm_extcon_register_notifier_all(dev, info->typec_extcon, &info->typec_extcon_nb);
	if (ret) {
		dev_err(dev, "%s:Can't register typec extcon\n", SPRD_VCHG_TAG);
		ret = -EINVAL;
		goto err_reg_usb;
	}
#endif

	sprd_vchg_detect_status(info);
	sprd_vchg_detect_pd_extcon_status(info);

	return 0;

#if IS_ENABLED(CONFIG_SC27XX_PD)
err_reg_usb:
	usb_unregister_notifier(info->usb_phy, &info->usb_notify);
#endif

remove_wakeup:
	wakeup_source_remove(info->sprd_vchg_ws);

	return ret;
}

static void sprd_vchg_suspend(struct sprd_vchg_info *info)
{
}

static void sprd_vchg_resume(struct sprd_vchg_info *info)
{
	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", SPRD_VCHG_TAG, __LINE__);
		return;
	}

}

static void sprd_vchg_remove(struct sprd_vchg_info *info)
{
	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", SPRD_VCHG_TAG, __LINE__);
		return;
	}

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);
	wakeup_source_remove(info->sprd_vchg_ws);
}

static void sprd_vchg_shutdown(struct sprd_vchg_info *info)
{
	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", SPRD_VCHG_TAG, __LINE__);
		return;
	}
}

struct sprd_vchg_info *sprd_vchg_info_register(struct device *dev)
{
	struct sprd_vchg_info *info = NULL;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(dev, "%s: Fail to malloc memory for sprd_vchg_info\n", SPRD_VCHG_TAG);
		return ERR_PTR(-ENOMEM);
	}

	info->ops = devm_kzalloc(dev, sizeof(*info->ops), GFP_KERNEL);
	if (!info->ops) {
		dev_err(dev, "%s: Fail to malloc memory for ops\n", SPRD_VCHG_TAG);
		return ERR_PTR(-ENOMEM);
	}

	info->ops->parse_dts = sprd_vchg_detect_parse_dts;
	info->ops->init = sprd_vchg_detect_init;
	info->ops->is_charger_online = sprd_vchg_is_charger_online;
	info->ops->get_bc1p2_type = sprd_vchg_get_bc1p2_type;
	info->ops->suspend = sprd_vchg_suspend;
	info->ops->resume = sprd_vchg_resume;
	info->ops->remove = sprd_vchg_remove;
	info->ops->shutdown = sprd_vchg_shutdown;
	info->dev = dev;

	return info;
}
EXPORT_SYMBOL_GPL(sprd_vchg_info_register);
