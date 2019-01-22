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

#ifndef __SPRD_DVFS_COMM_H__
#define __SPRD_DVFS_COMM_H__

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/types.h>

struct ops_entry {
	const char *ver;
	void *ops;
};

struct ops_list {
	struct list_head head;
	struct ops_entry *entry;
};

struct apsys_regmap {
	unsigned long apsys_base;
	unsigned long top_base;
};

struct apsys_dvfs_coffe {
	u32 sys_sw_dvfs_en;
	u32 sys_dvfs_hold_en;
	u32 sys_dvfs_clk_gate_ctrl;
	u32 sys_dvfs_wait_window;
	u32 sys_dvfs_min_volt;
	u32 sys_dvfs_force_en;
	u32 sys_sw_cgb_enable;
};

struct ip_dvfs_coffe {
	u32 gfree_wait_delay;
	u32 freq_upd_hdsk_en;
	u32 freq_upd_delay_en;
	u32 freq_upd_en_byp;
	u32 sw_trig_en;
	u32 hw_dvfs_en;
	u32 work_index_def;
	u32 idle_index_def;
};

struct ip_dvfs_map_cfg {
	u32 map_index;
	u32 volt_level;
	u32 clk_level;
	u32 clk_rate;
};

struct ip_dvfs_status {
	u32 ip_req_volt;
	u32 ip_req_clk;
	u32 current_sys_volt;
	u32 current_ip_clk;
	u32 apsys_cgm_cfg_debug_info;
	u32 apsys_volt_debug_info;
	u32 ap_volt;
	u32 vsp_vote;
	u32 dpu_vote;
};

struct ip_dvfs_para {
	u32 u_dvfs_en;
	u32 u_work_freq;
	u32 u_idle_freq;
	u32 u_work_index;
	u32 u_idle_index;
	u32 u_fix_volt;

	struct ip_dvfs_status ip_status;
	struct ip_dvfs_coffe ip_coffe;
};

struct apsys_dvfs_reg {
	u32 ap_dvfs_hold_ctrl;
	u32 ap_dvfs_wait_window_cfg;
	u32 ap_dfs_en_ctrl;
	u32 ap_sw_trig_ctrl;
	u32 ap_min_voltage_cfg;
	u32 reserved_0x0014_0x0030[8];
	u32 ap_sw_dvfs_ctrl;
	u32 ap_freq_update_bypass;
	u32 cgm_ap_dvfs_clk_gate_ctrl;
	u32 ap_dvfs_voltage_dbg;
	u32 reserved_0x0044_0x0048[2];
	u32 ap_dvfs_cgm_cfg_dbg;
	u32 ap_dvfs_state_dbg;
	u32 reserved_0x0054_0x0070[8];
	u32 vsp_index0_map;
	u32 vsp_index1_map;
	u32 vsp_index2_map;
	u32 vsp_index3_map;
	u32 vsp_index4_map;
	u32 vsp_index5_map;
	u32 vsp_index6_map;
	u32 vsp_index7_map;
	u32 dispc_index0_map;
	u32 dispc_index1_map;
	u32 dispc_index2_map;
	u32 reserved_0x00a0_0x00fc[24];
	u32 dispc_index3_map;
	u32 dispc_index4_map;
	u32 dispc_index5_map;
	u32 dispc_index6_map;
	u32 dispc_index7_map;
	u32 reserved_0x0114_0x0118[2];
	u32 vsp_dvfs_index_cfg;
	u32 vsp_dvfs_index_idle_cfg;
	u32 dispc_dvfs_index_cfg;
	u32 dispc_dvfs_index_idle_cfg;
	u32 ap_freq_upd_state;
	u32 ap_gfree_wait_delay_cfg;
	u32 ap_freq_upd_type_cfg;
	u32 ap_dvfs_reserved_reg_cfg0;
	u32 ap_dvfs_reserved_reg_cfg1;
	u32 ap_dvfs_reserved_reg_cfg2;
	u32 ap_dvfs_reserved_reg_cfg3;
};

typedef enum {
	DVFS_WORK = 0,
	DVFS_IDLE,
} set_freq_type;

extern struct class *dvfs_class;
extern struct apsys_regmap regmap_ctx;
extern struct mutex apsys_glb_reg_lock;

void *dvfs_ops_attach(const char *str, struct list_head *head);
int dvfs_ops_register(struct ops_entry *entry, struct list_head *head);

#endif /* __SPRD_DVFS_COMM_H__ */
