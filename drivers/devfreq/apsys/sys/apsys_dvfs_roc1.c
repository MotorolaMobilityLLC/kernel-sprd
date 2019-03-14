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

#include "sprd_dvfs_apsys.h"
#include "apsys_reg_roc1.h"


static void apsys_dvfs_auto_gate_sel(u32 enable)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	if (enable)
		reg->cgm_ap_dvfs_clk_gate_ctrl |= BIT(0);
	else
		reg->cgm_ap_dvfs_clk_gate_ctrl &= ~BIT(0);
}

#if 0
static void apsys_dvfs_force_en(u32 enable)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	if (enable)
		reg->cgm_ap_dvfs_clk_gate_ctrl |= BIT(1);
	else
		reg->cgm_ap_dvfs_clk_gate_ctrl &= ~BIT(1);
}
#endif

static void apsys_dvfs_hold_en(u32 hold_en)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	reg->ap_dvfs_hold_ctrl = hold_en;
}

static void apsys_dvfs_clk_gate(u32 clk_gate)
{
	apsys_dvfs_auto_gate_sel(clk_gate);
}

static void apsys_dvfs_wait_window(u32 wait_window)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	reg->ap_dvfs_wait_window_cfg = wait_window;
}

static void apsys_dvfs_min_volt(u32 min_volt)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	reg->ap_min_voltage_cfg = min_volt;
}

static int dcdc_modem_cur_volt(void)
{
	volatile u32 rw32;

	rw32 = *(volatile u32 *)(regmap_ctx.top_base + 0x0050);

	return (rw32 >> 20) & 0x07;
}

static void apsys_dvfs_init(struct apsys_dev *apsys)
{
	void __iomem *base;

	base = ioremap_nocache(0x322a0000, 0x150);
	if (IS_ERR(base))
		pr_err("ioremap top dvfs address failed\n");

	regmap_ctx.top_base = (unsigned long)base;
#if 0
	apsys_dvfs_hold(apsys->dvfs_coffe.dvfs_hold_en);
	apsys_dvfs_force(apsys->dvfs_coffe.dvfs_force_en);

	apsys_dvfs_min_volt(devfreq,
		apsys->dvfs_coffe.dvfs_min_volt);
	apsys_dvfs_wait_window(devfreq,
		apsys->dvfs_coffe.dvfs_wait_window);
	apsys_dvfs_clk_gate(devfreq,
		apsys->dvfs_coffe.dvfs_clk_gate_ctrl);
#endif
}

static struct apsys_dvfs_ops apsys_dvfs_ops = {
	.dvfs_init = apsys_dvfs_init,
	.apsys_clk_gate = apsys_dvfs_clk_gate,
	.apsys_hold_en = apsys_dvfs_hold_en,
	.apsys_wait_window = apsys_dvfs_wait_window,
	.apsys_min_volt = apsys_dvfs_min_volt,
	.top_cur_volt = dcdc_modem_cur_volt,
};

static struct dvfs_ops_entry apsys_dvfs_entry = {
	.ver = "roc1",
	.ops = &apsys_dvfs_ops,
};

static int __init apsys_dvfs_register(void)
{
	return apsys_dvfs_ops_register(&apsys_dvfs_entry);
}

subsys_initcall(apsys_dvfs_register);
