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

#ifndef _VSP_DVFS_H
#define _VSP_DVFS_H

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/devfreq-event.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "../governor.h"
#include "sprd_dvfs_comm.h"

struct vsp_dvfs {
	unsigned long dvfs_enable;
	struct device dev;

	unsigned long freq, target_freq;
	unsigned long volt, target_volt;
	unsigned long user_freq;

	struct devfreq *devfreq;
	struct devfreq_event_dev *edev;
	struct ip_dvfs_ops  *dvfs_ops;
	struct ip_dvfs_para vsp_dvfs_para;
	set_freq_type user_freq_type;
	struct mutex lock;
	struct notifier_block pw_nb;
	u32 max_freq_level;

};

struct ip_dvfs_ops {

	/* userspace interface */
	int (*ip_dvfs_init)(struct ip_dvfs_para *dvfs_para);
	void (*ip_hw_dvfs_en)(u32 dvfs_eb);
	void (*ip_auto_tune_en)(struct ip_dvfs_para *dvfs_para,
				unsigned long dvfs_eb);
	void (*set_work_freq)(struct ip_dvfs_para *dvfs_para,
				unsigned long work_freq);
	void (*get_work_freq)(struct ip_dvfs_para *dvfs_para,
				unsigned long work_freq);
	void (*set_idle_freq)(struct ip_dvfs_para *dvfs_para,
				unsigned long idle_freq);
	void (*get_idle_freq)(struct ip_dvfs_para *dvfs_para,
				unsigned long idle_freq);

	/* work-idle dvfs map ops */
	void (*get_ip_dvfs_table)(struct ip_dvfs_map_cfg *dvfs_table);
	void (*set_ip_dvfs_table)(struct ip_dvfs_para *dvfs_para,
				struct ip_dvfs_map_cfg *dvfs_table);
	void (*get_ip_dvfs_coffe)(struct ip_dvfs_para *dvfs_para,
				struct ip_dvfs_coffe *dvfs_coffe);
	void (*set_ip_dvfs_coffe)(struct ip_dvfs_para *dvfs_para,
				struct ip_dvfs_coffe *dvfs_coffe);
	void (*get_ip_status)(struct ip_dvfs_status *ip_status);
	void (*power_on_nb)(struct ip_dvfs_para *dvfs_para);
	void (*power_off_nb)(struct ip_dvfs_para *dvfs_para);

	/* coffe setting ops */
	void (*set_ip_gfree_wait_delay)(u32 wind_para);
	void (*set_ip_freq_upd_en_byp)(u32 on);
	void (*set_ip_freq_upd_delay_en)(u32 on);
	void (*set_ip_freq_upd_hdsk_en)(u32 on);
	void (*set_ip_dvfs_swtrig_en)(u32 en);

	/* work-idle dvfs index ops */
	void (*set_ip_dvfs_work_index)(struct ip_dvfs_para *dvfs_para,
			u32 index);
	void (*get_ip_dvfs_work_index)(u32 *index);
	void (*set_ip_dvfs_idle_index)(struct ip_dvfs_para *dvfs_para,
			u32 index);
	void (*updata_target_freq)(struct ip_dvfs_para *dvfs_para,
			unsigned long freq, set_freq_type user_freq_type);
};

typedef enum {
	VOLT70 = 0, //0.7
	VOLT75, //0.75
	VOLT80, //0.8
} voltage_level;

typedef enum {
	VSP_CLK256 = 256000000, //256
	VSP_CLK307 = 307200000, //307
	VSP_CLK384 = 384000000, //384
} vsp_clock_freq;

typedef enum {
	VSP_CLK_INDEX_256 = 0,
	VSP_CLK_INDEX_307 = 1,
	VSP_CLK_INDEX_384 = 2,
} vsp_clock_level;

#define MAX_FREQ_LEVEL 8

extern struct list_head vsp_dvfs_head;

#define vsp_dvfs_ops_register(entry) \
	dvfs_ops_register(entry, &vsp_dvfs_head)

#define vsp_dvfs_ops_attach(str) \
	dvfs_ops_attach(str, &vsp_dvfs_head)

#endif
