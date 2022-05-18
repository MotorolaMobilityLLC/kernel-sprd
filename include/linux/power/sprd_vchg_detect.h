/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2011 Unisoc Co., Ltd.
 * Jinfeng.lin <Jinfeng.Lin1@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _SPRD_VCHG_DETECT_H
#define _SPRD_VCHG_DETECT_H

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>

#define SPRD_VCHG_TAG	"srpd_vchg"
#define SPRD_VCHG_PD_HARD_RESET_MS		500
#define SPRD_VCHG_PD_RECONNECT_MS		3000
#define SPRD_VCHG_WAKE_UP_MS			1000

struct sprd_vchg_info {
	struct device *dev;
	/* receive vchg event */
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct extcon_dev *pd_extcon;
	struct notifier_block pd_extcon_nb;
	struct extcon_dev *typec_extcon;
	struct notifier_block typec_extcon_nb;
	struct delayed_work pd_hard_reset_work;
	struct delayed_work typec_extcon_work;
	struct work_struct sprd_vchg_work;
	struct wakeup_source *sprd_vchg_ws;
	struct power_supply *psy;
	struct sprd_vchg_ops *ops;
	bool pd_extcon_enable;
	bool pd_hard_reset;
	bool typec_online;
	u32 limit;
	int pd_extcon_status;
};

struct sprd_vchg_ops {
	int (*parse_dts)(struct sprd_vchg_info *info);
	int (*init)(struct sprd_vchg_info *info, struct power_supply *psy);
	bool (*is_charger_online)(struct sprd_vchg_info *info);
	enum power_supply_usb_type (*get_bc1p2_type)(struct sprd_vchg_info *info);
	void (*suspend)(struct sprd_vchg_info *info);
	void (*resume)(struct sprd_vchg_info *info);
	void (*remove)(struct sprd_vchg_info *info);
	void (*shutdown)(struct sprd_vchg_info *info);
};

struct sprd_vchg_info *sprd_vchg_info_register(struct device *dev);

#endif /* _SPRD_VCHG_DETECT_H */

