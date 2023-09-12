/*
* SPDX-FileCopyrightText: 2020-2022 Unisoc (Shanghai) Technologies Co., Ltd
* SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
*
* Copyright 2020-2022 Unisoc (Shanghai) Technologies Co., Ltd.
* Licensed under the Unisoc General Software License, version 1.0 (the License);
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
* Software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* See the Unisoc General Software License, version 1.0 for more details.
*/
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/regmap.h>
#include <linux/sprd_soc_id.h>
#include "dvfs_phy.h"
#include "mm_dvfs.h"
#include "mmsys_dvfs_comm.h"
#include "vdsp_debugfs.h"
#include "vdsp_hw.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: dvfs-phy %d: %d %s:" \
	fmt, current->pid, __LINE__, __func__

#define vmin(a, b) ((a) < (b) ? (a) :(b))
#define vmax(a, b) ((a) > (b) ? (a) :(b))

static void __iomem *mm_clk_config;

struct vdsp_dvfs_table {
	uint32_t power_level;
	uint32_t voltage;
	uint32_t voltage_index;
	uint32_t clk;
	uint32_t clk_index;
};

static struct vdsp_dvfs_table vdsp_table[7] = {
	{0, 600, 1, 26000000, 0},	//power level dvfs no use.
	{1, 600, 1, 26000000, 0},	//power level min
	{2, 600, 1, 307200000, 1},
	{3, 600, 1, 512000000, 2},
	{4, 650, 2, 614400000, 3},
	{5, 700, 3, 819200000, 4},
	{6, 750, 4, 1014000000, 5},	//power level max
};

static void enable_phy(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	if (!(hw->mm_ahb)) {
		pr_err("Invalid argument\n");
		return;
	}
	if (vdsp_regmap_update_bits(hw->mm_ahb, MM_SYS_EN, DVFS_EN, DVFS_EN, RT_MMSYS))
		pr_err("error enable dvfs\n");
}

static void disable_phy(void *hw_arg)
{
	struct vdsp_hw *hw = (struct vdsp_hw *)hw_arg;

	if (!(hw->mm_ahb)) {
		pr_err("Invalid argument\n");
		return;
	}
	if (vdsp_regmap_update_bits(hw->mm_ahb, MM_SYS_EN, DVFS_EN, 0, RT_MMSYS))
		pr_err("error disable dvfs\n");
}

static uint32_t level_to_freq(uint32_t level)
{
	switch (level) {
	case 1:	//VDSP_PWR_MIN
		return VDSP_CLK260;
	case 2:
		return VDSP_CLK3072;
	case 3:
		return VDSP_CLK5120;
	case 4:
		return VDSP_CLK6144;
	case 5:
		return VDSP_CLK8192;
	case 6:	//VDSP_PWR_MAX
		return VDSP_CLK10140;
	default:
		return VDSP_CLK260;
	}
}

static uint32_t soc_ver_id_check(void)
{
	int ret;
	uint32_t ver_id = 0;

	ret = sprd_get_soc_id(AON_VER_ID, &ver_id, 1);
	if (ret) {
		void __iomem *aon_reg;
		volatile uint32_t value;

		aon_reg = ioremap(0x64900000, 0x1000);
		value = __raw_readl(aon_reg + 0xF0);
		ver_id = (value & 0x1F);
		iounmap(aon_reg);
		pr_debug("ver_id is %d\n", ver_id);
		goto end;
	}
	if (ver_id == 0) {
		pr_warn("soc is AA\n");
	}
	else if (ver_id == 1) {
		pr_debug("soc is AB\n");
	}
	else {
		pr_warn("unkowned soc\n");
		ver_id = 0;
	}
end:
	return ver_id;
}

static void strategy(uint32_t *level, uint32_t max_level,
	uint32_t percent, uint32_t last_percent)
{
	if ((last_percent > 50)) {
		if (percent > 50)
			*level = max_level;
		else if ((percent <= 50) && (percent > 20))
			*level = max_level - 2;
		else
			*level = max_level - 3;
	} else if ((last_percent <= 50) && (last_percent > 20)) {
		if (percent > 50)
			*level = max_level;
		else if ((percent <= 50) && (percent > 20))
			*level = max_level - 2;
		else
			*level = max_level - 3;
	} else {
		if (percent > 50)
			*level = max_level;
		else if ((percent <= 50) && (percent > 20))
			*level = max_level - 3;
		else
			*level = max_level - 3;
	}
}

#if 1 /* software config */
static inline void reg_write32_setbit_h(void *addr, u32 v)
{
	volatile u32 value;
	value = __raw_readl(addr);
	value = v | value;
	__raw_writel(value, addr);
}

static inline void reg_write32_clearbit_h(void *addr, u32 v)
{
	volatile u32 value;
	value = __raw_readl(addr);
	value = v & value;
	__raw_writel(value, addr);
}

static int set_clk_index(uint32_t clk_index)
{
	reg_write32_setbit_h((void *)(mm_clk_config + 0x28), clk_index);
	return 0;
}

static int set_work_freq_sw(uint32_t clk_index)
{
	pr_debug("[sw workaround] dvfs clk index:%d, -> register [0x30010028]\n", clk_index);

	mm_clk_config = ioremap(0x30010000, 0x1000);
	set_clk_index(clk_index);
	iounmap(mm_clk_config);
	return 0;
}
#endif

static void setdvfs_hw(uint32_t freq)
{
	struct ip_dvfs_ops *vdsp_dvfs_ops_ptr = get_vdsp_dvfs_ops();

	vdsp_dvfs_ops_ptr->set_work_freq(NULL, freq);
}

static void setdvfs(uint32_t level)
{
	unsigned int debugfs_dvfs_level;

	debugfs_dvfs_level = vdsp_debugfs_dvfs_level();
	if (debugfs_dvfs_level > 0)
	{
		level = debugfs_dvfs_level;
		pr_info("debugfs force dvfs, level:%d, freq:%d\n", level, vdsp_table[level].clk);
	}

	level = vmin(level, 6);		//vdsp table index max, 6-1014M
//	level = vmin(level, 5);		//vdsp table index max, 5-8192M (power requirements)

#ifdef DVFS_HIGH_FREQ_RANGE		// APOLLO version, high performance, no low freq
	level = vmax(level, 5);		// 5-819.2M
	pr_info("APOLLO version, level[%d]\n", level);
#endif

	pr_info("level:%d, freq:%d\n", level, vdsp_table[level].clk);
	// AB chip
	if (1 == soc_ver_id_check()) {
		setdvfs_hw(vdsp_table[level].clk);
	} else {
		set_work_freq_sw(vdsp_table[level].clk_index);
	}
}

static struct dvfs_phy_ops vdsp_dvfs_ops = {
	.enable = enable_phy,
	.disable = disable_phy,
	.level_to_freq = level_to_freq,
	.setdvfs = setdvfs,
	.strategy = strategy,
};

static struct dvfs_phy_desc sub_dvfs_phy_desc = {
	.ops = &vdsp_dvfs_ops,
};

struct dvfs_phy_desc *get_dvfs_phy_desc(void)
{
	return &sub_dvfs_phy_desc;
}

