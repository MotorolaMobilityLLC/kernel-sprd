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

#ifndef _MMSYS_COMM_DVFS_H_
#define _MMSYS_COMM_DVFS_H_

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/devfreq-event.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <sprd_mm.h>
#include <asm/memory.h>
#include <linux/version.h>
#include "mm_dvfs.h"


#define TRUE 1
#define FALSE 0
#define MM_DVFS_SUCCESS 0
#define TOP_SW_DVFS_ENABLE 0 //1:enable 0:disable
#define MM_SW_DVFS_ENABLE 0  //1:enable 0:disable
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#define DEVFREQ_NAME_LEN 16
#endif

typedef enum {
	DVFS_DCAM0_1 = 0,
	DVFS_DCAM0_1_AXI,
	DVFS_DCAM2_3,
	DVFS_DCAM2_3_AXI,
	DVFS_DEPTH,
	DVFS_CPP,
	DVFS_ISP,
	DVFS_FD,
	DVFS_JPEG,
	DVFS_DCAM_MTX,
	DVFS_MTX_DATA,
	DVFS_VDSP,
	DVFS_VDMA,
	DVFS_VDSP_MTX,
	DVFS_MODULE_MAX,
} mmsys_module;


typedef enum {
	DVFS_WORK = 0,
	DVFS_IDLE,
} set_freq_type;

typedef enum {
	MMSYS_POWER_ON = 1,
	MMSYS_POWER_OFF,
	MMSYS_POWER_ONOFF,
	MMSYS_POWER_MAX,
} mmsys_notifier_type;

#ifdef DVFS_VERSION_N6P
struct mmsys_clk_para{
	uint32_t clk_name;
	uint32_t clk_value;
	uint32_t ip_change_flag; //1=Upscaling,0=Down frequency
};
#endif
struct ip_dvfs_coffe {
	uint32_t gfree_wait_delay;
	uint32_t freq_upd_hdsk_en;
	uint32_t freq_upd_delay_en;
	uint32_t freq_upd_en_byp;
	uint32_t sw_trig_en;
	uint32_t auto_tune;
	uint32_t work_index_def;
	uint32_t idle_index_def;
};

#ifdef DVFS_VERSION_N6P
struct ip_dvfs_map_cfg {
	uint32_t map_index;
	unsigned long reg_add;
	uint32_t volt;
	uint32_t clk_freq;
	uint32_t clk;
	uint32_t fdiv_denom;
	uint32_t fdiv_num;
	uint32_t dcam_axi_index;
	uint32_t mm_mtx_index;
	char *volt_value;
};
#else
struct ip_dvfs_map_cfg {
	uint32_t map_index;
	unsigned long reg_add;
	uint32_t volt;
	uint32_t clk_freq;
	uint32_t clk;
	uint32_t mm_mtx_index;
	uint32_t dcam_mtx_index;
	uint32_t dcam_axi_index;
	char *volt_value;
};
#endif
struct ip_dvfs_status {
	uint32_t ip_req_volt;
	uint32_t ip_req_clk;
	uint32_t current_sys_volt;
	uint32_t current_ip_clk;
	uint32_t mmsys_cgm_cfg_debug_info;
	uint32_t mmsys_volt_debug_info;
	char *isp_vote_volt;
	char *dcam0_1_vote_volt;
	char *dcam0_1_axi_vote_volt;
	char *dcam2_3_vote_volt;
	char *dcam2_3_axi_vote_volt;
	char *dcam_mtx_vote_volt;
	char *fd_vote_volt;
	char *cpp_vote_volt;
	char *jpg_vote_volt;
	char *mtx_data_vote_volt;
	char *depth_vote_volt;
	char *mm_vote_volt;
	char *top_volt;

	uint32_t isp_clk;
	uint32_t dcam0_1_clk;
	uint32_t dcam0_1_axi_clk;
	uint32_t dcam2_3_clk;
	uint32_t dcam2_3_axi_clk;
	uint32_t dcam_mtx_clk;
	uint32_t fd_clk;
	uint32_t cpp_clk;
	uint32_t jpg_clk;
	uint32_t mtx_data_clk;
	uint32_t depth_clk;
};

struct mmsys_dvfs_para {
	uint32_t sys_sw_dvfs_en;
	uint32_t sys_dvfs_hold_en;
	uint32_t sys_dvfs_clk_gate_ctrl;
	uint32_t sys_dvfs_wait_window;
	uint32_t sys_dvfs_min_volt;
	uint32_t sys_dvfs_force_en;
	uint32_t sys_sw_cgb_enable;
};

struct ip_dvfs_para {
	mmsys_module ip_type;

	uint32_t u_dvfs_en;
	uint32_t u_auto_tune_en;
	uint32_t u_work_freq;
	uint32_t u_idle_freq;
	uint32_t u_work_index;
	uint32_t u_idle_index;
	uint32_t u_fix_volt;

	struct ip_dvfs_status ip_status;
	struct ip_dvfs_coffe ip_coffe;
	struct ip_dvfs_map_cfg ip_dvfs_map[8];
};

struct mmreg_map {
	unsigned long mmdvfs_ahb_regbase;
	unsigned long mmdvfs_top_regbase;
	unsigned long mm_ahb_regbase;
	unsigned long mm_power_regbase;
	unsigned long mm_power_on_regbase;
};

struct ip_dvfs_ops {
	struct list_head node;

	const char name[DEVFREQ_NAME_LEN];
	const unsigned int available;

	/* userspace  interface*/
	int (*ip_dvfs_init)(struct devfreq *devfreq);

	int (*ip_hw_dvfs_en)(struct devfreq *devfreq, unsigned int dvfs_eb);

	int (*ip_auto_tune_en)(struct devfreq *devfreq, unsigned long dvfs_eb);

	int (*set_work_freq)(struct devfreq *devfreq, unsigned long work_freq);
	int (*get_work_freq)(struct devfreq *devfreq, unsigned long work_freq);

	int (*set_idle_freq)(struct devfreq *devfreq, unsigned long idle_freq);
	int (*get_idle_freq)(struct devfreq *devfreq, unsigned long idle_freq);

	/*work-idle dvfs map  ops*/
	int (*get_ip_dvfs_table)(struct devfreq *devfreq, struct ip_dvfs_map_cfg *dvfs_table);
	int (*set_ip_dvfs_table)(struct devfreq *devfreq, struct ip_dvfs_map_cfg *dvfs_table);

	int (*get_ip_dvfs_coffe)(struct devfreq *devfreq, struct ip_dvfs_coffe *dvfs_coffe);
	int (*set_ip_dvfs_coffe)(struct devfreq *devfreq,
	struct ip_dvfs_coffe *dvfs_coffe);

	int (*get_ip_status)(struct devfreq *devfreq, struct ip_dvfs_status *ip_status);

	void (*power_on_nb)(struct devfreq *devfreq);
	void (*power_off_nb)(struct devfreq *devfreq);

	/*coffe setting ops*/
	void (*set_ip_gfree_wait_delay)(unsigned int wind_para);
	void (*set_ip_freq_upd_en_byp)(unsigned int on);
	void (*set_ip_freq_upd_delay_en)(unsigned int on);
	void (*set_ip_freq_upd_hdsk_en)(unsigned int on);
	void (*set_ip_dvfs_swtrig_en)(unsigned int en);

	/*work-idle dvfs index ops*/
	void (*set_ip_dvfs_work_index)(struct devfreq *devfreq, unsigned int index);
	void (*get_ip_dvfs_work_index)(struct devfreq *devfreq, unsigned int *index);

	void (*set_ip_dvfs_idle_index)(struct devfreq *devfreq, unsigned int index);
	void (*get_ip_work_index_from_table)(struct ip_dvfs_map_cfg *dvfs_cfg,
		unsigned long work_freq, unsigned int *index);
	void (*get_ip_idle_index_from_table)(struct ip_dvfs_map_cfg *dvfs_cfg,
		unsigned long work_freq,unsigned int *index);

	int (*updata_target_freq)(struct devfreq *devfreq, unsigned long volt,
		unsigned long freq, unsigned int enable_sysgov);
	int (*event_handler)(struct devfreq *devfreq, unsigned int event, void *data);
	int (*set_fix_dvfs_value)(struct devfreq *devfreq, unsigned long freq);

	/*mmsys ops*/
	int (*mmsys_sw_dvfs_en)(struct devfreq *devfreq, unsigned int sw_en);
	int (*mmsys_dvfs_hold_en)(struct devfreq *devfreq, unsigned int hold_en);
	int (*mmsys_dvfs_clk_gate_ctrl)(struct devfreq *devfreq, unsigned int clk_gate);
	int (*mmsys_dvfs_wait_window)(struct devfreq *devfreq, unsigned int wait_window);
	int (*mmsys_dvfs_min_volt)(struct devfreq *devfreq, unsigned int min_volt);
	void (*mmsys_power_ctrl)(struct devfreq *devfreq, unsigned int on);
	int (*top_current_volt)(struct devfreq *devfreq, unsigned int *top_volt);
	int (*mm_current_volt)(struct devfreq *devfreq, unsigned int *mm_volt);
};

extern struct ip_dvfs_ops *get_ip_dvfs_ops(const char *name);
extern int register_ip_dvfs_ops(struct ip_dvfs_ops *dvfs_ops);
extern int unregister_ip_dvfs_ops(struct ip_dvfs_ops *dvfs_ops);
extern int mmsys_adjust_target_freq(mmsys_module md_type, unsigned long volt,
unsigned long freq, unsigned int en_sysgov);
extern int mmsys_register_notifier(struct notifier_block *nb);
extern int mmsys_unregister_notifier(struct notifier_block *nb);
extern int mmsys_notify_call_chain(mmsys_notifier_type notifier_type);
extern int mmsys_set_fix_dvfs_value(unsigned long fix_volt);
extern int top_mm_dvfs_current_volt(struct devfreq *devfreq);
extern int mm_dvfs_read_current_volt(struct devfreq *devfreq);
extern int mmsys_dvfs_pw_on(void);
extern int mmsys_dvfs_pw_off(void);
extern int set_mtx_data_work_freq(unsigned int index);
extern int set_dcam01_axi_work_freq(unsigned int index);
extern int set_dcam_mtx_work_freq(unsigned int index);


#ifdef DVFS_VERSION_N6P
extern int set_vdsp_mtx_work_freq(unsigned int index);
extern struct ip_dvfs_ops *get_vdma_dvfs_ops(void);
extern struct ip_dvfs_ops *get_vdsp_dvfs_ops(void);
extern struct ip_dvfs_ops *get_vdsp_mtx_dvfs_ops(void);
#endif

extern struct mutex mmsys_glob_reg_lock;
extern struct mmreg_map g_mmreg_map;

#define DVFS_DEBUG
#ifdef DVFS_DEBUG
#define DVFS_TRACE pr_info
#else
#define DVFS_TRACE pr_debug
#endif

//extern void dvfs_test_case(u32 tc_id);

#endif
