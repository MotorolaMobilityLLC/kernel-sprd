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

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>

#include "../sprd_dvfs_comm.h"
#include "../sprd_dvfs_apsys.h"

static void ap_dvfs_auto_gate_sel(u32 enable)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	if (enable)
		reg->cgm_ap_dvfs_clk_gate_ctrl |= BIT(0);
	else
		reg->cgm_ap_dvfs_clk_gate_ctrl &= ~BIT(0);
}

#if 0
static void ap_dvfs_force_en(u32 enable)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	if (enable)
		reg->cgm_ap_dvfs_clk_gate_ctrl |= BIT(1);
	else
		reg->cgm_ap_dvfs_clk_gate_ctrl &= ~BIT(1);
}
#endif

static void set_apsys_sw_dvfs_en(struct apsys_dev *apsys,
		u32 sw_dvfs_eb)
{

}

static void set_apsys_dvfs_hold_en(struct apsys_dev *apsys,
		u32 hold_en)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	reg->ap_dvfs_hold_ctrl = hold_en;
}

static void set_apsys_dvfs_clk_gate_ctrl(struct apsys_dev *apsys,
		u32 clk_gate)
{
	ap_dvfs_auto_gate_sel(clk_gate);
}

static void set_apsys_dvfs_wait_window(struct apsys_dev *apsys,
		u32 wait_window)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	reg->ap_dvfs_wait_window_cfg = wait_window;
}

static void set_apsys_dvfs_min_volt(struct apsys_dev *apsys,
		u32 min_volt)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	reg->ap_min_voltage_cfg = min_volt;
}

static void apsys_dvfs_init(struct apsys_dev *apsys)
{
#if 0
	set_apsys_dvfs_hold_en(apsys->apsys_dvfs_para.sys_dvfs_hold_en);
	ap_dvfs_force_en(apsys->apsys_dvfs_para.sys_dvfs_force_en);

	set_apsys_dvfs_min_volt(devfreq,
		apsys->dvfs_coffe.sys_dvfs_min_volt);
	set_apsys_dvfs_wait_window(devfreq,
		apsys->dvfs_coffe.sys_dvfs_wait_window);
	set_apsys_dvfs_clk_gate_ctrl(devfreq,
		apsys->dvfs_coffe.sys_dvfs_clk_gate_ctrl);
#endif
}

static struct apsys_dvfs_ops apsys_dvfs_ops = {
	.dvfs_init = apsys_dvfs_init,
	.set_dvfs_clk_gate_ctrl = set_apsys_dvfs_clk_gate_ctrl,
	.set_sw_dvfs_en = set_apsys_sw_dvfs_en,
	.set_dvfs_hold_en = set_apsys_dvfs_hold_en,
	.set_dvfs_wait_window = set_apsys_dvfs_wait_window,
	.set_dvfs_min_volt = set_apsys_dvfs_min_volt,
};

static struct ops_entry apsys_dvfs_entry = {
	.ver = "sharkl5",
	.ops = &apsys_dvfs_ops,
};

static int __init apsys_dvfs_register(void)
{
	return apsys_dvfs_ops_register(&apsys_dvfs_entry);
}

subsys_initcall(apsys_dvfs_register);
