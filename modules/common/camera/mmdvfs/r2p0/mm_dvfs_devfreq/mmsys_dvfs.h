/*
 * Copyright (C) 2021-2022 UNISOC Communications Inc.
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

#ifndef _MMSYS_DVFS_H
#define _MMSYS_DVFS_H

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/devfreq-event.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include "governor.h"
#include "mmsys_dvfs_comm.h"

struct mmsys_dvfs {

    unsigned long power_ctrl;
    struct clk *clk_mmsys_core;
    struct devfreq *devfreq;
    struct devfreq_event_dev *edev;
    struct ip_dvfs_ops *dvfs_ops;
    struct regmap *top_apb;
    struct mmsys_dvfs_para mmsys_dvfs_para;
    struct mutex lock;
    struct notifier_block pw_nb;
};

extern struct ip_dvfs_ops mmsys_dvfs_ops;
extern struct devfreq_governor mmsys_dvfs_gov;
extern int cpp_dvfs_init(void);
extern int isp_dvfs_init(void);
extern int jpg_dvfs_init(void);
extern int dcam0_1_dvfs_init(void);
extern int dcam0_1_axi_dvfs_init(void);
extern int dcam2_3_dvfs_init(void);
extern int dcam2_3_axi_dvfs_init(void);
extern int dcam_mtx_dvfs_init(void);
extern int mtx_data_dvfs_init(void);

#ifdef DVFS_VERSION_N6P
extern int fd_dvfs_init(void);
extern int depth_dvfs_init(void);
extern int vdsp_dvfs_init(void);
extern int vdma_dvfs_init(void);
extern int vdsp_mtx_dvfs_init(void);

extern int fd_dvfs_exit(void);
extern int depth_dvfs_exit(void);
extern int vdsp_dvfs_exit(void);
extern int vdma_dvfs_exit(void);
extern int vdsp_mtx_dvfs_exit(void);
#endif

extern int cpp_dvfs_exit(void);
extern int isp_dvfs_exit(void);
extern int jpg_dvfs_exit(void);
extern int dcam0_1_dvfs_exit(void);
extern int dcam0_1_axi_dvfs_exit(void);
extern int dcam2_3_dvfs_exit(void);
extern int dcam2_3_axi_dvfs_exit(void);
extern int dcam_mtx_dvfs_exit(void);
extern int mtx_data_dvfs_exit(void);
#endif
