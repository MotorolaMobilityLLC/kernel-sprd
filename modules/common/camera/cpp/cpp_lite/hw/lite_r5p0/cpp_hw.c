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

#include <linux/clk.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>

#include "cpp_drv.h"
#include "cpp_reg.h"
#include "cpp_block.h"
#include "cpp_hw.h"


/* Macro Definitions */
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CPP_HW: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int cpphw_bypass_path_support(void *cpp_handle)
{
	struct cpp_hw_info * hw_info = NULL;
	int cpp_bp_support = 0;

	if (!cpp_handle) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	hw_info = (struct cpp_hw_info *)cpp_handle;
	if (CPP_BP_SUPPORT == 1)
		cpp_bp_support = 1;
	else if (CPP_BP_SUPPORT == 0)
		cpp_bp_support = 0;
	else
		cpp_bp_support = 0;

	return cpp_bp_support;
}

static int cpphw_slice_support(void *cpp_handle)
{
	struct cpp_hw_info * hw_info = NULL;
	int cpp_slice_support = 0;

	if (!cpp_handle) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	hw_info = (struct cpp_hw_info *)cpp_handle;
	if (CPP_SLICE_SUPPORT == 1)
		cpp_slice_support = 1;
	else if (CPP_SLICE_SUPPORT == 0)
		cpp_slice_support = 0;
	else
		cpp_slice_support = 0;

	return cpp_slice_support;
}

static int cpphw_zoom_up_support(void *cpp_handle)
{
	struct cpp_hw_info * hw_info = NULL;
	int cpp_zoom_up_support = 0;

	if (!cpp_handle) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	hw_info = (struct cpp_hw_info *)cpp_handle;
	if (CPP_ZOOM_UP_SUPPORT == 1)
		cpp_zoom_up_support = 1;
	else if (CPP_ZOOM_UP_SUPPORT == 0)
		cpp_zoom_up_support = 0;
	else
		cpp_zoom_up_support = 0;

	return cpp_zoom_up_support;
}

static int cpphw_qos_set(void *cpp_soc_handle)
{
	uint32_t qos = 0;
	struct cpp_hw_soc_info *soc_cpp = NULL;

	if (!cpp_soc_handle) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	soc_cpp = (struct cpp_hw_soc_info *)cpp_soc_handle;
	qos = soc_cpp->awrqos;
	CPP_REG_MWR(CPP_AXIM_CHN_SET,
			CPP_AXIM_CHN_SET_QOS_MASK,
			qos);
	return 0;
}

static int cpphw_mmu_set(void *cpp_soc_handle)
{
	struct cpp_hw_soc_info *soc_cpp = NULL;

	if (!cpp_soc_handle) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	soc_cpp = (struct cpp_hw_soc_info *)cpp_soc_handle;
	CPP_REG_AWR(CPP_MMU_EN, (0xfffffffe));
	CPP_REG_OWR(MMU_PPN_RANGE1, (0xfff));
	CPP_REG_OWR(MMU_PPN_RANGE2, (0xfff));
	return 0;
}

static int cpphw_module_reset(void *cpp_soc_handle)
{
	int ret = 0;
	struct cpp_hw_soc_info *soc_cpp = NULL;

	if (!cpp_soc_handle) {
		pr_err("fail to get valid input ptr\n");
		ret = -EINVAL;
		return ret;
	}
	soc_cpp = (struct cpp_hw_soc_info *)cpp_soc_handle;

	regmap_update_bits(soc_cpp->mm_ahb_gpr, MM_AHB_RESET,
			CPP_PATH_RESET_MASK,
			(unsigned int)CPP_PATH_RESET_MASK);
	udelay(2);
	regmap_update_bits(soc_cpp->mm_ahb_gpr, MM_AHB_RESET,
			CPP_PATH_RESET_MASK,
			~(unsigned int)CPP_PATH_RESET_MASK);
	return ret;
}

static int cpphw_scale_reset(void *cpp_soc_handle)
{
	int ret = 0;
	struct cpp_hw_soc_info *soc_cpp = NULL;

	if (!cpp_soc_handle) {
		pr_err("fail to get valid input ptr\n");
		ret = -EINVAL;
		return ret;
	}
	soc_cpp = (struct cpp_hw_soc_info *)cpp_soc_handle;

	regmap_update_bits(soc_cpp->mm_ahb_gpr, (unsigned int)MM_AHB_RESET,
			(unsigned int)CPP_PATH0_AHB_RESET_BIT,
			(unsigned int)CPP_PATH0_AHB_RESET_BIT);
	udelay(2);
	regmap_update_bits(soc_cpp->mm_ahb_gpr, (unsigned int)MM_AHB_RESET,
			(unsigned int)CPP_PATH0_AHB_RESET_BIT,
			~(unsigned int)CPP_PATH0_AHB_RESET_BIT);
	return ret;
}

static int cpphw_rot_reset(void *cpp_soc_handle)
{
	int ret = 0;
	struct cpp_hw_soc_info *soc_cpp = NULL;

	if (!cpp_soc_handle) {
		pr_err("fail to get valid input ptr\n");
		ret = -EINVAL;
		return ret;
	}
	soc_cpp = (struct cpp_hw_soc_info *)cpp_soc_handle;

	regmap_update_bits(soc_cpp->mm_ahb_gpr, MM_AHB_RESET,
			CPP_PATH1_AHB_RESET_BIT,
			(unsigned int)CPP_PATH1_AHB_RESET_BIT);
	udelay(2);
	regmap_update_bits(soc_cpp->mm_ahb_gpr, MM_AHB_RESET,
			CPP_PATH1_AHB_RESET_BIT,
			~(unsigned int)CPP_PATH1_AHB_RESET_BIT);
	return ret;
}

static int cpphw_clk_enable(void *cpp_soc_handle)
{
	int ret = 0;
	struct cpp_hw_soc_info *soc_cpp = NULL;

	if (!cpp_soc_handle) {
		pr_err("fail to get valid input ptr\n");
		ret = -1;
		return ret;
	}
	soc_cpp = (struct cpp_hw_soc_info *)cpp_soc_handle;

	ret = clk_prepare_enable(soc_cpp->cpp_eb);
	if (ret) {
		pr_err("fail to enable cpp eb\n");
		return ret;
	}
	ret = clk_set_parent(soc_cpp->cpp_clk, soc_cpp->cpp_clk_parent);
	if (ret) {
		pr_err("fail to set cpp clk parent\n");
		clk_disable_unprepare(soc_cpp->cpp_eb);
		return ret;
	}

	ret = clk_prepare_enable(soc_cpp->cpp_clk);
	if (ret) {
		pr_err("fail to enable cpp clk\n");
		clk_disable_unprepare(soc_cpp->cpp_eb);
		return ret;
	}

	return ret;
}

static int cpphw_clk_disable(void *cpp_soc_handle)
{
	int ret = 0;
	struct cpp_hw_soc_info *soc_cpp = NULL;

	if (!cpp_soc_handle) {
		pr_err("fail to get valid input ptr\n");
		ret = -1;
		return ret;
	}
	soc_cpp = (struct cpp_hw_soc_info *)cpp_soc_handle;

	clk_set_parent(soc_cpp->cpp_clk, soc_cpp->cpp_clk_default);
	clk_disable_unprepare(soc_cpp->cpp_clk);
	clk_disable_unprepare(soc_cpp->cpp_eb);
	return ret;
}

static struct hw_ioctrl_fun cpphw_ioctl_fun_tab[] = {
	{CPP_HW_CFG_BP_SUPPORT,                 cpphw_bypass_path_support},
	{CPP_HW_CFG_SLICE_SUPPORT,              cpphw_slice_support},
	{CPP_HW_CFG_ZOOMUP_SUPPORT,             cpphw_zoom_up_support},
	{CPP_HW_CFG_QOS_SET,                    cpphw_qos_set},
	{CPP_HW_CFG_MMU_SET,                    cpphw_mmu_set},
	{CPP_HW_CFG_MODULE_RESET,               cpphw_module_reset},
	{CPP_HW_CFG_SCALE_RESET,                cpphw_scale_reset},
	{CPP_HW_CFG_ROT_RESET,                  cpphw_rot_reset},
	{CPP_HW_CFG_CLK_EB,                     cpphw_clk_enable},
	{CPP_HW_CFG_CLK_DIS,                    cpphw_clk_disable},
	{CPP_HW_CFG_SC_REG_TRACE,               cpp_k_scale_reg_trace},
	{CPP_HW_CFG_ROT_REG_TRACE,              cpp_k_rot_reg_trace},
	{CPP_HW_CFG_ROT_EB,                     cpp_k_rot_dev_enable},
	{CPP_HW_CFG_ROT_DISABLE,                cpp_k_rot_dev_disable},
	{CPP_HW_CFG_ROT_START,                  cpp_k_rot_dev_start},
	{CPP_HW_CFG_ROT_STOP,                   cpp_k_rot_dev_stop},
	{CPP_HW_CFG_ROT_PARM_SET,               cpp_k_rot_parm_set},
	{CPP_HW_CFG_SCL_EB,                     cpp_k_scale_dev_enable},
	{CPP_HW_CFG_SCL_DISABLE,                cpp_k_scale_dev_disable},
	{CPP_HW_CFG_SCL_START,                  cpp_k_scale_dev_start},
	{CPP_HW_CFG_SCL_STOP,                   cpp_k_scale_dev_stop},
	{CPP_HW_CFG_SCL_CLK_SWITCH,             cpp_k_scale_clk_switch},
	{CPP_HW_CFG_SCL_DES_PITCH_SET,          cpp_k_scale_des_pitch_set},
	{CPP_HW_CFG_SCL_SRC_PITCH_SET,          cpp_k_scale_src_pitch_set},
	{CPP_HW_CFG_SCL_IN_RECT_SET,            cpp_k_scale_input_rect_set},
	{CPP_HW_CFG_SCL_OUT_RECT_SET,           cpp_k_scale_output_rect_set},
	{CPP_HW_CFG_SCL_IN_FORMAT_SET,          cpp_k_scale_input_format_set},
	{CPP_HW_CFG_SCL_OUT_FORMAT_SET,         cpp_k_scale_output_format_set},
	{CPP_HW_CFG_SCL_DECI_SET,               cpp_k_scale_deci_set},
	{CPP_HW_CFG_SCL_IN_ENDIAN_SET,          cpp_k_scale_input_endian_set},
	{CPP_HW_CFG_SCL_OUT_ENDIAN_SET,         cpp_k_scale_output_endian_set},
	{CPP_HW_CFG_SCL_BURST_GAP_SET,          cpp_k_scale_burst_gap_set},
	{CPP_HW_CFG_SCL_BPEN_SET,               cpp_k_scale_bpen_set},
	{CPP_HW_CFG_SCL_INI_PHASE_SET,          cpp_k_scale_ini_phase_set},
	{CPP_HW_CFG_SCL_TAP_SET,                cpp_k_scale_tap_set},
	{CPP_HW_CFG_SCL_OFFSET_SIZE_SET,        cpp_k_scale_offset_size_set},
	{CPP_HW_CFG_SCL_ADDR_SET,               cpp_k_scale_addr_set},
	{CPP_HW_CFG_SCL_LUMA_HCOEFF_SET,        cpp_k_scale_luma_hcoeff_set},
	{CPP_HW_CFG_SCL_CHRIMA_HCOEF_SET,       cpp_k_scale_chrima_hcoeff_set},
	{CPP_HW_CFG_SCL_VCOEF_SET,              cpp_k_scale_vcoeff_set}
};

static cpp_hw_ioctl_fun cpphw_ioctl_fun_get(
	enum cpp_hw_cfg_cmd cmd)
{
	cpp_hw_ioctl_fun hw_ctrl = NULL;
	uint32_t total_num = 0;
	uint32_t i = 0;

	total_num = sizeof(cpphw_ioctl_fun_tab) / sizeof(struct hw_ioctrl_fun);
	for (i = 0; i < total_num; i++) {
		if (cmd == cpphw_ioctl_fun_tab[i].cmd) {
			hw_ctrl = cpphw_ioctl_fun_tab[i].hw_ctrl;
			break;
		}
	}

	return hw_ctrl;
}

static int cpphw_ioctl(enum cpp_hw_cfg_cmd cmd, void *arg)
{
	int ret = 0;
	cpp_hw_ioctl_fun hw_ctrl = NULL;

	hw_ctrl = cpphw_ioctl_fun_get(cmd);
	if (hw_ctrl != NULL)
		ret = hw_ctrl(arg);
	else
		pr_err("cpp_hw_core_ctrl_fun is null, cmd %d", cmd);

	return ret;
}

unsigned long g_cpp_base;

int cpphw_probe(struct platform_device *pdev, struct cpp_hw_info * hw_info)
{
	int ret = 0;
	unsigned int irq = 0;
	void __iomem *reg_base = NULL;
	struct regmap *mm_ahb_gpr = NULL;
	struct cpp_hw_soc_info *soc_cpp = NULL;
	struct cpp_hw_ip_info *ip_cpp = NULL;
	struct device_node *qos_node = NULL;

	if (!pdev|| !hw_info) {
		pr_err("fail to get pdev %p, hw_info %p\n", pdev, hw_info);
		return -EFAULT;
	}

	CPP_TRACE("sprd cpp probe pdev name %s\n", pdev->name);
	/* Start cpp soc related dt parse */
	soc_cpp = hw_info->soc_cpp;
	soc_cpp->pdev = pdev;
	/* read cpp clk */
	soc_cpp->cpp_eb = devm_clk_get(&pdev->dev, "cpp_eb");
	if (IS_ERR_OR_NULL(soc_cpp->cpp_eb)) {
		ret =  PTR_ERR(soc_cpp->cpp_eb);
		return ret;
	}

	soc_cpp->cpp_clk = devm_clk_get(&pdev->dev, "cpp_clk");
	if (IS_ERR_OR_NULL(soc_cpp->cpp_clk)) {
		ret = PTR_ERR(soc_cpp->cpp_clk);
		return ret;
	}

	soc_cpp->cpp_clk_parent = devm_clk_get(&pdev->dev, "cpp_clk_parent");
	if (IS_ERR_OR_NULL(soc_cpp->cpp_clk_parent)) {
		ret = PTR_ERR(soc_cpp->cpp_clk_parent);
		return ret;
	}

	soc_cpp->cpp_clk_default = clk_get_parent(soc_cpp->cpp_clk);
	if (IS_ERR_OR_NULL(soc_cpp->cpp_clk_default)) {
		ret = PTR_ERR(soc_cpp->cpp_clk_default);
		return ret;
	}
	/* get mm ahb */
	mm_ahb_gpr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
			"sprd,cam-ahb-syscon");
	if (IS_ERR_OR_NULL(mm_ahb_gpr)) {
		pr_err("fail to get mm_ahb_gpr\n");
		ret = PTR_ERR(mm_ahb_gpr);
		return ret;
	}
	soc_cpp->mm_ahb_gpr = mm_ahb_gpr;

	reg_base = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR_OR_NULL(reg_base)) {
		pr_err("fail to get cpp base\n");
		ret = PTR_ERR(reg_base);
		return ret;
	}

	g_cpp_base = (unsigned long)reg_base;

	qos_node = of_parse_phandle(pdev->dev.of_node, "cpp_qos", 0);
	if (qos_node) {
		uint8_t val;

		if (of_property_read_u8(qos_node, "awrqos", &val)) {
			pr_err("fail to get cpp qos.\n");
			val = 0x1;
		}
	soc_cpp->awrqos = (uint32_t)val;
	} else {
		soc_cpp->awrqos = 0x1;
	}
	/* get irq */
	ip_cpp = hw_info->ip_cpp;
	ip_cpp->pdev = pdev;
	ip_cpp->io_base = reg_base;
	irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (irq <= 0) {
		pr_err("fail to get cpp irq %d\n", irq);
		ret = -EINVAL;
	}
	ip_cpp->irq = irq;
	return ret;
}

static struct cpp_hw_soc_info cpp_soc_info;
static struct cpp_hw_ip_info cpp_ip_info;

struct cpp_hw_info lite_r5p0_cpp_hw_info = {
	.prj_id = CPP_R5P0,
	.pdev = NULL,
	.soc_cpp = &cpp_soc_info,
	.ip_cpp = &cpp_ip_info,
	.cpp_hw_ioctl = cpphw_ioctl,
	.cpp_probe = cpphw_probe,
};
