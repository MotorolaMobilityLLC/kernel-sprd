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

#ifndef __SPRD_DVFS_DPU_H__
#define __SPRD_DVFS_DPU_H__

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/types.h>

typedef enum {
	VOLT70 = 0, //0.7v
	VOLT75,	//0.75v
} voltage_level;;

typedef enum {
	DPU_CLK_INDEX_153M = 0,
	DPU_CLK_INDEX_192M,
	DPU_CLK_INDEX_256M,
	DPU_CLK_INDEX_307M2,
	DPU_CLK_INDEX_384M,
} clock_level;

typedef enum {
	DPU_CLK153M6 = 153600000,
	DPU_CLK192M = 192000000,
	DPU_CLK256M = 256000000,
	DPU_CLK307M2 = 307200000,
	DPU_CLK384M = 384000000,
} clock_rate;

struct dpu_dvfs {
	int dvfs_enable;
	struct device dev;

	struct devfreq *devfreq;
	struct opp_table *opp_table;
	struct devfreq_event_dev *edev;
	struct mutex lock;

	u32 work_freq;
	u32 idle_freq;
	set_freq_type freq_type;

	int hwlayer;
	struct ip_dvfs_coffe dvfs_coffe;
	struct ip_dvfs_status dvfs_status;
	struct dpu_dvfs_ops *dvfs_ops;
};

struct dpu_dvfs_ops {
	/* initialization interface */
	int (*parse_dt)(struct dpu_dvfs *dpu, struct device_node *np);
	int (*dvfs_init)(struct dpu_dvfs *dpu);
	void (*hw_dvfs_en)(bool dvfs_en);

	/* work-idle dvfs index ops */
	void  (*set_work_index)(int index);
	int  (*get_work_index)(void);
	void  (*set_idle_index)(int index);
	int  (*get_idle_index)(void);

	/* work-idle dvfs freq ops */
	void (*set_work_freq)(u32 freq);
	u32 (*get_work_freq)(void);
	void (*set_idle_freq)(u32 freq);
	u32 (*get_idle_freq)(void);

	/* work-idle dvfs map ops */
	void (*get_dvfs_table)(struct ip_dvfs_map_cfg *dvfs_table);
	void (*set_dvfs_table)(struct ip_dvfs_map_cfg *dvfs_table);
	void (*get_dvfs_coffe)(struct ip_dvfs_coffe *dvfs_coffe);
	void (*set_dvfs_coffe)(struct ip_dvfs_coffe *dvfs_coffe);
	void (*get_status)(struct ip_dvfs_status *dvfs_status);

	/* coffe setting ops */
	void (*set_gfree_wait_delay)(u32 para);
	void (*set_freq_upd_en_byp)(bool enable);
	void (*set_freq_upd_delay_en)(bool enable);
	void (*set_freq_upd_hdsk_en)(bool enable);
	void (*set_dvfs_swtrig_en)(bool enable);
};

extern struct list_head dpu_dvfs_head;
extern struct blocking_notifier_head dpu_dvfs_chain;

#define dpu_dvfs_ops_register(entry) \
	dvfs_ops_register(entry, &dpu_dvfs_head)

#define dpu_dvfs_ops_attach(str) \
	dvfs_ops_attach(str, &dpu_dvfs_head)

#define dpu_dvfs_notifier_register(nb) \
	blocking_notifier_chain_register(&dpu_dvfs_chain, nb);

#define dpu_dvfs_notifier_unregister(nb) \
	blocking_notifier_chain_unregister(&dpu_dvfs_chain, nb);

#define dpu_dvfs_notifier_call_chain(data) \
	blocking_notifier_call_chain(&dpu_dvfs_chain, data);

#endif /* __SPRD_DVFS_APSYS_H__ */
