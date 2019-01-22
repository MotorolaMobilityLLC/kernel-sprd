/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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

#ifndef __SPRD_DVFS_APSYS_H__
#define __SPRD_DVFS_APSYS_H__

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/types.h>

struct apsys_dev {
	struct device dev;
	unsigned long base;
	const char *version;

	struct mutex lock;
	struct apsys_dvfs_coffe dvfs_coffe;
	struct apsys_dvfs_ops *dvfs_ops;
};

struct apsys_dvfs_ops {
	/* apsys common ops */
	void (*dvfs_init)(struct apsys_dev *apsys);
	void (*set_sw_dvfs_en)(struct apsys_dev *apsys, u32 sw_dvfs_eb);
	void (*set_dvfs_hold_en)(struct apsys_dev *apsys, u32 hold_en);
	void (*set_dvfs_clk_gate_ctrl)(struct apsys_dev *apsys, u32 clk_gate);
	void (*set_dvfs_wait_window)(struct apsys_dev *apsys, u32 wait_window);
	void (*set_dvfs_min_volt)(struct apsys_dev *apsys, u32 min_volt);
};

extern struct list_head apsys_dvfs_head;

#define apsys_dvfs_ops_register(entry) \
	dvfs_ops_register(entry, &apsys_dvfs_head)

#define apsys_dvfs_ops_attach(str) \
	dvfs_ops_attach(str, &apsys_dvfs_head)

#endif /* __SPRD_DVFS_APSYS_H__ */
