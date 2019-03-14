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

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/slab.h>

#include "sprd_dvfs_apsys.h"
#include "sprd_dvfs_dpu.h"
#include "apsys_reg_roc1.h"

static struct ip_dvfs_map_cfg map_table[] = {
	{0, VOLT70, DPU_CLK_INDEX_153M6, DPU_CLK153M6},
	{1, VOLT70, DPU_CLK_INDEX_192M, DPU_CLK192M},
	{2, VOLT70, DPU_CLK_INDEX_256M, DPU_CLK256M},
	{3, VOLT70, DPU_CLK_INDEX_307M2, DPU_CLK307M2},
	{4, VOLT75, DPU_CLK_INDEX_384M, DPU_CLK384M},
	{5, VOLT75, DPU_CLK_INDEX_468M, DPU_CLK468M},
};

static int ip_dvfs_parse_dt(struct dpu_dvfs *dpu,
				struct device_node *np)
{
	return 0;
}

static void ip_hw_dfs_en(bool dfs_en)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	mutex_lock(&apsys_glb_reg_lock);
	if (dfs_en)
		reg->ap_dfs_en_ctrl |= BIT(0);
	else
		reg->ap_dfs_en_ctrl &= ~BIT(0);
	mutex_unlock(&apsys_glb_reg_lock);
}

static void ip_dvfs_map_cfg(void)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	reg->dispc_index0_map = map_table[0].clk_level |
		map_table[0].volt_level << 3;
	reg->dispc_index1_map = map_table[1].clk_level |
		map_table[1].volt_level << 3;
	reg->dispc_index2_map = map_table[2].clk_level |
		map_table[2].volt_level << 3;
	reg->dispc_index3_map = map_table[3].clk_level |
		map_table[3].volt_level << 3;
	reg->dispc_index4_map = map_table[4].clk_level |
		map_table[4].volt_level << 3;
	reg->dispc_index5_map = map_table[5].clk_level |
		map_table[5].volt_level << 3;
}

static void set_ip_work_freq(u32 freq)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		if (map_table[i].clk_rate == freq) {
			reg->dispc_dvfs_index_cfg = i;
			break;
		}
	}
	udelay(100);
}

static u32 get_ip_work_freq(void)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;
	u32 freq;
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		if (map_table[i].map_index ==
			reg->dispc_dvfs_index_cfg) {
			freq = map_table[i].clk_rate;
			break;
		}
	}

	return freq;
}

static void set_ip_idle_freq(u32 freq)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		if (map_table[i].clk_rate == freq) {
			reg->dispc_dvfs_index_idle_cfg = i;
			break;
		}
	}
}

static u32 get_ip_idle_freq(void)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;
	u32 freq;
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		if (map_table[i].map_index ==
			reg->dispc_dvfs_index_idle_cfg) {
			freq = map_table[i].clk_rate;
			break;
		}
	}

	return freq;
}

static void set_ip_work_index(int index)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	reg->dispc_dvfs_index_cfg = index;
	udelay(100);

	//top_apsys_dvfs_current_volt(dpu);
	//apsys_dvfs_read_current_volt(dpu);
}

static int get_ip_work_index(void)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	return reg->dispc_dvfs_index_cfg;
}

static void set_ip_idle_index(int index)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	reg->dispc_dvfs_index_idle_cfg = index;

	//top_apsys_dvfs_current_volt(devfreq);
	//apsys_dvfs_read_current_volt(devfreq);
}

static int get_ip_idle_index(void)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	return reg->dispc_dvfs_index_idle_cfg;
}

static void set_ip_gfree_wait_delay(u32 para)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	mutex_lock(&apsys_glb_reg_lock);
	reg->ap_gfree_wait_delay_cfg |= para << 10;
	mutex_unlock(&apsys_glb_reg_lock);
}

static void set_ip_freq_upd_en_byp(bool enable)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	mutex_lock(&apsys_glb_reg_lock);
	if (enable)
		reg->ap_freq_update_bypass |= BIT(0);
	else
		reg->ap_freq_update_bypass &= ~BIT(0);
	mutex_unlock(&apsys_glb_reg_lock);
}

static void set_ip_freq_upd_delay_en(bool enable)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	mutex_lock(&apsys_glb_reg_lock);
	if (enable)
		reg->ap_freq_upd_type_cfg |= BIT(5);
	else
		reg->ap_freq_upd_type_cfg &= ~BIT(5);
	mutex_unlock(&apsys_glb_reg_lock);
}

static void set_ip_freq_upd_hdsk_en(bool enable)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	mutex_lock(&apsys_glb_reg_lock);
	if (enable)
		reg->ap_freq_upd_type_cfg |= BIT(4);
	else
		reg->ap_freq_upd_type_cfg &= ~BIT(4);
	mutex_unlock(&apsys_glb_reg_lock);
}

static void set_ip_dvfs_swtrig_en(bool enable)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	mutex_lock(&apsys_glb_reg_lock);
	if (enable)
		reg->ap_sw_trig_ctrl |= BIT(2);
	else
		reg->ap_sw_trig_ctrl &= ~BIT(2);
	mutex_unlock(&apsys_glb_reg_lock);
}

static void set_ip_dvfs_table(struct ip_dvfs_map_cfg *dvfs_table)
{
	memcpy(map_table, dvfs_table, sizeof(struct
		ip_dvfs_map_cfg));
}

static int get_ip_dvfs_table(struct ip_dvfs_map_cfg *dvfs_table)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(map_table); i++) {
		dvfs_table[i].map_index = map_table[i].map_index;
		dvfs_table[i].volt_level = map_table[i].volt_level;
		dvfs_table[i].clk_level = map_table[i].clk_level;
		dvfs_table[i].clk_rate = map_table[i].clk_rate;
	}

	return i;
}

static void get_ip_status(struct ip_dvfs_status *dvfs_status)
{
	struct apsys_dvfs_reg *reg =
		(struct apsys_dvfs_reg *)regmap_ctx.apsys_base;

	mutex_lock(&apsys_glb_reg_lock);
	dvfs_status->vsp_vote = (reg->ap_dvfs_voltage_dbg >> 6) & (0x7);
	dvfs_status->dpu_vote = (reg->ap_dvfs_voltage_dbg >> 3) & (0x7);
	dvfs_status->ap_volt = (reg->ap_dvfs_voltage_dbg >> 12) & (0x7);
	dvfs_status->vsp_clk = (reg->ap_dvfs_cgm_cfg_dbg >> 3) & (0x3);
	dvfs_status->dpu_clk = (reg->ap_dvfs_cgm_cfg_dbg) & (0x7);
	mutex_unlock(&apsys_glb_reg_lock);
}

static int ip_dvfs_init(struct dpu_dvfs *dpu)
{
	ip_dvfs_map_cfg();

#if 0
	set_ip_freq_upd_en_byp(dpu->dvfs_coffe.freq_upd_en_byp);
	set_ip_freq_upd_delay_en(dpu->dvfs_coffe.freq_upd_delay_en);
	set_ip_freq_upd_hdsk_en(dpu->dvfs_coffe.freq_upd_hdsk_en);
	set_ip_gfree_wait_delay(dpu->dvfs_coffe.gfree_wait_delay);
	set_ip_dvfs_swtrig_en(dpu->dvfs_coffe.sw_trig_en);
#endif

	set_ip_work_index(dpu->dvfs_coffe.work_index_def);
	set_ip_idle_index(dpu->dvfs_coffe.idle_index_def);
	ip_hw_dfs_en(dpu->dvfs_coffe.hw_dfs_en);

	return 0;
}

static struct dpu_dvfs_ops dpu_dvfs_ops = {
	.parse_dt = ip_dvfs_parse_dt,
	.dvfs_init = ip_dvfs_init,
	.hw_dfs_en = ip_hw_dfs_en,
	.set_work_freq = set_ip_work_freq,
	.get_work_freq = get_ip_work_freq,
	.set_idle_freq = set_ip_idle_freq,
	.get_idle_freq = get_ip_idle_freq,
	.set_work_index = set_ip_work_index,
	.get_work_index = get_ip_work_index,
	.set_idle_index = set_ip_idle_index,
	.get_idle_index = get_ip_idle_index,
	.set_dvfs_table = set_ip_dvfs_table,
	.get_dvfs_table = get_ip_dvfs_table,
	.get_status = get_ip_status,

	.set_gfree_wait_delay = set_ip_gfree_wait_delay,
	.set_freq_upd_en_byp = set_ip_freq_upd_en_byp,
	.set_freq_upd_delay_en = set_ip_freq_upd_delay_en,
	.set_freq_upd_hdsk_en = set_ip_freq_upd_hdsk_en,
	.set_dvfs_swtrig_en = set_ip_dvfs_swtrig_en,
};

static struct dvfs_ops_entry dpu_dvfs_entry = {
	.ver = "roc1",
	.ops = &dpu_dvfs_ops,
};

static int __init dpu_dvfs_register(void)
{
	return dpu_dvfs_ops_register(&dpu_dvfs_entry);
}

subsys_initcall(dpu_dvfs_register);
