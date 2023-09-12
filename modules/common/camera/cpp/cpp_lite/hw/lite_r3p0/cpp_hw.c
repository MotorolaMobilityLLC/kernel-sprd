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
#include "cam_kernel_adapt.h"

/* Macro Definitions */
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CPP_HW: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__
#ifndef TEST_ON_HAPS

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
	struct cpp_hw_soc_info *soc_cpp = NULL;

	if (!cpp_soc_handle) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	soc_cpp = (struct cpp_hw_soc_info *)cpp_soc_handle;

	CPP_REG_MWR(CPP_AXIM_CHN_SET, CPP_AXIM_CHN_SET_QOS_MASK, (0x1 << 16));
	CPP_REG_MWR(CPP_MMU_PT_UPDATE_QOS, CPP_MMU_PT_UPDATE_QOS_MASK, 0x1);
	return 0;
}
#endif

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

	regmap_update_bits(soc_cpp->syscon_regs[0].gpr,
			soc_cpp->syscon_regs[0].reg,
			soc_cpp->syscon_regs[0].mask,
			soc_cpp->syscon_regs[0].mask);
		udelay(2);
		regmap_update_bits(soc_cpp->syscon_regs[0].gpr,
			soc_cpp->syscon_regs[0].reg,
			soc_cpp->syscon_regs[0].mask,
			~soc_cpp->syscon_regs[0].mask);
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
		if (ret)
			return ret;

		ret = clk_prepare_enable(soc_cpp->cpp_axi_eb);
		if (ret)
			return ret;

		ret = clk_set_parent(soc_cpp->cpp_clk, soc_cpp->cpp_clk_parent);
		if (ret) {
			clk_disable_unprepare(soc_cpp->cpp_eb);
			return ret;
		}

		ret = clk_prepare_enable(soc_cpp->cpp_clk);
		if (ret) {
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
	clk_disable_unprepare(soc_cpp->cpp_axi_eb);
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
	{CPP_HW_CFG_SCL_CLK_SWITCH,             NULL},
	{CPP_HW_CFG_SCL_DES_PITCH_SET,          cpp_k_scale_des_pitch_set},
	{CPP_HW_CFG_SCL_SRC_PITCH_SET,          cpp_k_scale_src_pitch_set},
	{CPP_HW_CFG_SCL_IN_RECT_SET,            cpp_k_scale_input_rect_set},
	{CPP_HW_CFG_SCL_OUT_RECT_SET,           cpp_k_scale_output_rect_set},
	{CPP_HW_CFG_SCL_IN_FORMAT_SET,          cpp_k_scale_input_format_set},
	{CPP_HW_CFG_SCL_OUT_FORMAT_SET,         cpp_k_scale_output_format_set},
	{CPP_HW_CFG_SCL_DECI_SET,               cpp_k_scale_deci_set},
	{CPP_HW_CFG_SCL_IN_ENDIAN_SET,          cpp_k_scale_input_endian_set},
	{CPP_HW_CFG_SCL_OUT_ENDIAN_SET,         cpp_k_scale_output_endian_set},
	{CPP_HW_CFG_SCL_BURST_GAP_SET,          NULL},
	{CPP_HW_CFG_SCL_BPEN_SET,               NULL},
	{CPP_HW_CFG_SCL_INI_PHASE_SET,          NULL},
	{CPP_HW_CFG_SCL_TAP_SET,                NULL},
	{CPP_HW_CFG_SCL_OFFSET_SIZE_SET,        NULL},
	{CPP_HW_CFG_SCL_ADDR_SET,               cpp_k_scale_addr_set},
	{CPP_HW_CFG_SCL_LUMA_HCOEFF_SET,        NULL},
	{CPP_HW_CFG_SCL_CHRIMA_HCOEF_SET,       NULL},
	{CPP_HW_CFG_SCL_VCOEF_SET,              NULL}
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

static int cpphw_parse_dts(struct cpp_hw_soc_info *soc_cpp,struct platform_device *pdev)
{
    struct device_node *np_isp;
    if (!soc_cpp|| !pdev) {
        pr_err("soc_cpp or pdev is null\n");
        return -EFAULT;
    }
    soc_cpp->cpp_eb = devm_clk_get(&pdev->dev, "cpp_eb");
    if (IS_ERR(soc_cpp->cpp_eb))
        return PTR_ERR(soc_cpp->cpp_eb);

    soc_cpp->cpp_axi_eb = devm_clk_get(&pdev->dev, "cpp_axi_eb");
    if (IS_ERR(soc_cpp->cpp_axi_eb))
        return PTR_ERR(soc_cpp->cpp_axi_eb);

	/* hw: cpp,isp use the same clk
	 * so read isp_clk, isp_clk_parent from isp node
	 * If change clk, need change isp_clk_parent
	 */
    np_isp = of_parse_phandle(pdev->dev.of_node, "ref-node", 0);
    if (IS_ERR(np_isp)) {
        pr_err("fail to get isp node\n");
        return PTR_ERR(np_isp);
    }

    soc_cpp->cpp_clk = of_clk_get_by_name(np_isp, "isp_clk");
    if (IS_ERR(soc_cpp->cpp_clk)) {
        pr_err("fail to get isp_clk, %p\n", soc_cpp->cpp_clk);
        return PTR_ERR(soc_cpp->cpp_clk);
    }

    soc_cpp->cpp_clk_parent = of_clk_get_by_name(np_isp, "isp_clk_parent");
    if (IS_ERR(soc_cpp->cpp_clk_parent)) {
        pr_err("fail to get isp_clk_parent %p\n", soc_cpp->cpp_clk_parent);
        return PTR_ERR(soc_cpp->cpp_clk_parent);
    }

    soc_cpp->cpp_clk_default = clk_get_parent(soc_cpp->cpp_clk);
    if (IS_ERR(soc_cpp->cpp_clk_default))
        return PTR_ERR(soc_cpp->cpp_clk_default);
    return 0;
}

int cpphw_probe(struct platform_device *pdev, struct cpp_hw_info * hw_info)
{
    int i,ret = 0;
	unsigned int irq = 0;
	struct cpp_hw_soc_info *soc_cpp = NULL;
	struct cpp_hw_ip_info *ip_cpp = NULL;
	struct device_node *np = NULL;
	const char *pname = NULL;
	struct regmap *tregmap = NULL;
	uint32_t args[2];
	struct resource *res = NULL;

	if (!pdev|| !hw_info) {
		pr_err("fail to get pdev %p, hw_info %p\n", pdev, hw_info);
		return -EFAULT;
	}
	pr_info("sprd cpp probe pdev name %s\n", pdev->name);
	np = pdev->dev.of_node;
	soc_cpp = hw_info->soc_cpp;
	soc_cpp->pdev = pdev;
	ip_cpp = hw_info->ip_cpp;
	ip_cpp->pdev = pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	ip_cpp->io_base = devm_ioremap_nocache(&pdev->dev, res->start,
				resource_size(res));
	if (IS_ERR(ip_cpp->io_base))
		return PTR_ERR(ip_cpp->io_base);
	g_cpp_base = (unsigned long)ip_cpp->io_base;

    ret = cpphw_parse_dts(soc_cpp,pdev);
    if (ret != 0){
        return ret;
    }

	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		pr_err("fail to get IRQ\n");
		return -ENXIO;
	}

	ip_cpp->irq = irq;
	np = pdev->dev.of_node;
	/* read global register */
	for (i = 0; i < ARRAY_SIZE(syscon_name); i++) {
		pname = syscon_name[i];
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
		tregmap =  syscon_regmap_lookup_by_name(np, pname);
		if (IS_ERR_OR_NULL(tregmap)) {
			pr_err("fail to read %s regmap\n", pname);
			continue;
		}
		ret = syscon_get_args_by_name(np, pname, 2, args);
		if (ret != 2) {
			pr_err("fail to read %s args, ret %d\n",
				pname, ret);
			continue;
		}
#else
		tregmap = syscon_regmap_lookup_by_phandle_args(np, pname, 2, args);
		if (IS_ERR_OR_NULL(tregmap)) {
			pr_err("fail to read %s reg_map 0x%lx %d\n",
			pname, (uintptr_t)tregmap, IS_ERR_OR_NULL(tregmap));
			continue;
		}
#endif
		soc_cpp->syscon_regs[i].gpr = tregmap;
		soc_cpp->syscon_regs[i].reg = args[0];
		soc_cpp->syscon_regs[i].mask = args[1];
		pr_info("dts[%s] 0x%x 0x%x\n", pname,
			soc_cpp->syscon_regs[i].reg, soc_cpp->syscon_regs[i].mask);
	}
	return 0;
}

static struct cpp_hw_soc_info cpp_soc_info;
static struct cpp_hw_ip_info cpp_ip_info;

struct cpp_hw_info lite_r3p0_cpp_hw_info = {
	.prj_id = CPP_R3P0,
	.pdev = NULL,
	.soc_cpp = &cpp_soc_info,
	.ip_cpp = &cpp_ip_info,
	.cpp_hw_ioctl = cpphw_ioctl,
	.cpp_probe = cpphw_probe,
};
