// SPDX-License-Identifier: GPL-2.0
//
// Spreatrum SHARKL3 camera pd driver
//
// Copyright (C) 2021 Spreadtrum, Inc.
// Author: Hongjian Wang <hongjian.wang@spreadtrum.com>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/pm_domain.h>
#include <dt-bindings/soc/sprd,pike2-mask.h>
#include <dt-bindings/soc/sprd,pike2-regs.h>

#include "sprd_camsys_domain.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd_campd_r4p0: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__


#define PD_MM_STAT_BIT_SHIFT 28
#define BIT_PMU_APB_PD_MM_SYS_STATE(x)	(((x) & 0xf) << PD_MM_STAT_BIT_SHIFT)
#define PD_MM_DOWN_FLAG (0x7 << PD_MM_STAT_BIT_SHIFT)

static int sprd_cam_domain_eb(struct camsys_power_info *pw_info)
{
	unsigned int rst_bit = 0;
	unsigned int eb_bit = 0;

	pr_debug("cb %pS\n", __builtin_return_address(0));

	/* mm bus enable */
	/*clk_prepare_enable(pw_info->u.pike2.cam_mm_eb);*/

	/* cam CKG enable */
	clk_prepare_enable(pw_info->u.pike2.cam_ckg_eb);

	/* config cam ahb clk */
	clk_set_parent(pw_info->u.pike2.cam_ahb_clk, pw_info->u.pike2.cam_ahb_clk_parent);
	clk_prepare_enable(pw_info->u.pike2.cam_ahb_clk);

	eb_bit = MASK_MM_AHB_DCAM_EB | MASK_MM_AHB_ISP_EB |
		MASK_MM_AHB_CSI_EB | MASK_MM_AHB_CKG_EB;
	regmap_update_bits(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_AHB_EB,
			eb_bit,
			eb_bit);

	rst_bit =
		MASK_MM_AHB_AHB_CKG_SOFT_RST |
		MASK_MM_AHB_AXI_CAM_MTX_SOFT_RST;

	regmap_update_bits(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_AHB_RST,
			rst_bit,
			rst_bit);
	udelay(1);
	regmap_update_bits(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_AHB_RST,
			rst_bit,
			~rst_bit);
	/* clock for anlg_phy_g7_controller */
	regmap_update_bits(pw_info->u.pike2.aon_apb_gpr,
		REG_AON_APB_APB_EB2,
		MASK_AON_APB_ANLG_APB_EB,
		MASK_AON_APB_ANLG_APB_EB);

	return 0;
}

static int sprd_cam_domain_disable(struct camsys_power_info *pw_info)
{
	pr_debug("cb %pS\n", __builtin_return_address(0));

	clk_set_parent(pw_info->u.pike2.cam_ahb_clk,
		       pw_info->u.pike2.cam_ahb_clk_default);
	clk_disable_unprepare(pw_info->u.pike2.cam_ahb_clk);

	clk_disable_unprepare(pw_info->u.pike2.cam_ckg_eb);

	/*clk_disable_unprepare(pw_info->u.pike2.cam_mm_eb);*/

	return 0;
}

static void sprd_mm_lpc_ctrl(struct camsys_power_info *pw_info)
{
	unsigned int val = 0;

	pr_debug("open mm lpc\n");
	/* add lpc and light sleep */
	if (regmap_read(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_CAM_MTX_LPC_CTRL, &val))
		goto err_exit;
	val |= MASK_MM_AHB_CAM_MTX_LPC_EB | 0x20;
	regmap_write(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_CAM_MTX_LPC_CTRL, val);

	if (regmap_read(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_CAM_MTX_LPC_CTRL_M0, &val))
		goto err_exit;
	val |= MASK_MM_AHB_CAM_MTX_M0_LPC_EB | 0x20;
	regmap_write(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_CAM_MTX_LPC_CTRL_M0, val);

	if (regmap_read(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_CAM_MTX_LPC_CTRL_M1, &val))
		goto err_exit;
	val |= MASK_MM_AHB_CAM_MTX_M1_LPC_EB | 0x20;
	regmap_write(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_CAM_MTX_LPC_CTRL_M1, val);

	if (regmap_read(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_VSP_MTX_LPC_CTRL_M0, &val))
		goto err_exit;
	val |= MASK_MM_AHB_VSP_MTX_M0_LPC_EB | 0x20;
	regmap_write(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_VSP_MTX_LPC_CTRL_M0, val);

	if (regmap_read(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_VSP_MTX_LPC_CTRL_M1, &val))
		goto err_exit;
	val |= MASK_MM_AHB_VSP_MTX_M1_LPC_EB | 0x20;
	regmap_write(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_VSP_MTX_LPC_CTRL_M1, val);

	if (regmap_read(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_VSP_MTX_LPC_CTRL_S0, &val))
		goto err_exit;
	val |= MASK_MM_AHB_VSP_MTX_S0_LPC_EB | 0x20;
	regmap_write(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_VSP_MTX_LPC_CTRL_S0, val);

	if (regmap_read(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_VSP_MTX_LPC_CTRL_GPV, &val))
		goto err_exit;
	val |= MASK_MM_AHB_VSP_MTX_GPV_LPC_EB | 0x20;
	regmap_write(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_VSP_MTX_LPC_CTRL_GPV, val);

	if (regmap_read(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_MM_LIGHT_SLEEP_CTRL, &val))
		goto err_exit;
	val |= MASK_MM_AHB_REG_VSP_MTX_FRC_LSLP_M1 |
			MASK_MM_AHB_REG_VSP_MTX_FRC_LSLP_M0 |
			MASK_MM_AHB_REG_CAM_MTX_FRC_LSLP_M1 |
			MASK_MM_AHB_REG_CAM_MTX_FRC_LSLP_M0;
	regmap_write(pw_info->u.pike2.cam_ahb_gpr,
			REG_MM_AHB_MM_LIGHT_SLEEP_CTRL, val);
	return;

err_exit:
	pr_err("reg config fail\n");
}


int sprd_cam_pw_off(struct camsys_power_info *pw_info)
{
	int ret = 0;
	unsigned int power_state1 = 0;
	unsigned int read_count = 0;
	unsigned int val = 0;

	pr_debug("cb %pS\n", __builtin_return_address(0));

	usleep_range(300, 350);
	regmap_update_bits(pw_info->u.pike2.pmu_apb_gpr,
			   REG_PMU_APB_PD_MM_TOP_CFG,
			   MASK_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN,
			   ~(unsigned int)
			   MASK_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN);
	regmap_update_bits(pw_info->u.pike2.pmu_apb_gpr,
			   REG_PMU_APB_PD_MM_TOP_CFG,
			   MASK_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN,
			   MASK_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN);

	do {
		cpu_relax();
		usleep_range(300, 350);
		read_count++;

		ret = regmap_read(pw_info->u.pike2.pmu_apb_gpr,
				  REG_PMU_APB_PWR_STATUS0_DBG, &val);
		if (ret)
			goto err_pw_off;
		power_state1 = val & BIT_PMU_APB_PD_MM_SYS_STATE(0xf);

	} while ((power_state1 != PD_MM_DOWN_FLAG) &&
		read_count < 10);

	if (power_state1 != PD_MM_DOWN_FLAG) {
		pr_err("cam domain pw off failed 0x%x\n", power_state1);
		ret = -1;
		goto err_pw_off;
	}
	pr_info("cam_pw_domain:cam_pw_off set OK.\n");
	return 0;

err_pw_off:
	pr_err("cam domain pw off failed, ret: %d, count: %d!\n",
	       ret, read_count);
	return 0;
}

static int sprd_cam_pw_on(struct camsys_power_info *pw_info)
{
	int ret = 0;
	unsigned int power_state1;
	unsigned int read_count = 0;
	unsigned int val = 0;


	/* cam domain power on */
	regmap_update_bits(pw_info->u.pike2.pmu_apb_gpr,
			   REG_PMU_APB_PD_MM_TOP_CFG,
			   MASK_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN,
			   ~(unsigned int)
			   MASK_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN);
	regmap_update_bits(pw_info->u.pike2.pmu_apb_gpr,
			   REG_PMU_APB_PD_MM_TOP_CFG,
			   MASK_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN,
			   ~(unsigned int)
			   MASK_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN);

	do {
		cpu_relax();
		usleep_range(300, 350);
		read_count++;

		ret = regmap_read(pw_info->u.pike2.pmu_apb_gpr,
				  REG_PMU_APB_PWR_STATUS0_DBG, &val);
		if (ret)
			goto err_pw_on;
		power_state1 = val & BIT_PMU_APB_PD_MM_SYS_STATE(0xf);

	} while (power_state1 && read_count < 10);

	if (power_state1) {
		pr_err("cam domain pw on failed 0x%x\n", power_state1);
		ret = -1;
		goto err_pw_on;
	}

	/* mm bus enable */
	clk_prepare_enable(pw_info->u.pike2.cam_mm_eb);
	udelay(50);
	sprd_mm_lpc_ctrl(pw_info);
	pr_info("cam_pw_domain:cam_pw_on set OK.\n");

	return 0;
err_pw_on:
	pr_info("cam domain, failed to power on\n");

	return ret;
}

static long sprd_cam_pw_domain_init(struct platform_device *pdev, struct camsys_power_info *pw_info)
{
	int ret = 0;
	struct regmap *cam_ahb_gpr = NULL;
	struct regmap *aon_apb_gpr = NULL;
	struct regmap *pmu_apb_gpr = NULL;
	unsigned int chip_id0 = 0, chip_id1 = 0;

	pr_info("cam_pw_domain: cam_pw_domain_init.\n");

	pw_info->u.pike2.cam_ckg_eb = devm_clk_get(&pdev->dev, "clk_gate_eb");
	if (IS_ERR(pw_info->u.pike2.cam_ckg_eb)) {
		pr_err("cam pw domain init fail, cam_ckg_eb\n");
		return PTR_ERR(pw_info->u.pike2.cam_ckg_eb);
	}

	pw_info->u.pike2.cam_mm_eb = devm_clk_get(&pdev->dev, "clk_mm_eb");
	if (IS_ERR(pw_info->u.pike2.cam_mm_eb)) {
		pr_err("cam pw domain init fail, cam_mm_eb\n");
		return PTR_ERR(pw_info->u.pike2.cam_mm_eb);
	}

	pw_info->u.pike2.cam_ahb_clk = devm_clk_get(&pdev->dev, "clk_mm_ahb");
	if (IS_ERR(pw_info->u.pike2.cam_ahb_clk)) {
		pr_err("cam pw domain init fail, cam_ahb_clk\n");
		return PTR_ERR(pw_info->u.pike2.cam_ahb_clk);
	}

	pw_info->u.pike2.cam_ahb_clk_parent =
		devm_clk_get(&pdev->dev, "clk_mm_ahb_parent");
	if (IS_ERR(pw_info->u.pike2.cam_ahb_clk_parent)) {
		pr_err("cam pw domain init fail, cam_ahb_clk_parent\n");
		return PTR_ERR(pw_info->u.pike2.cam_ahb_clk_parent);
	}

	pw_info->u.pike2.cam_ahb_clk_default = clk_get_parent(pw_info->u.pike2.cam_ahb_clk);
	if (IS_ERR(pw_info->u.pike2.cam_ahb_clk_default)) {
		pr_err("cam pw domain init fail, cam_ahb_clk_default\n");
		return PTR_ERR(pw_info->u.pike2.cam_ahb_clk_default);
	}

	cam_ahb_gpr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						      "sprd,cam-ahb-syscon");
	if (IS_ERR(cam_ahb_gpr)) {
		pr_err("cam pw domain init fail, cam_ahb_gpr\n");
		return PTR_ERR(cam_ahb_gpr);
	}
	pw_info->u.pike2.cam_ahb_gpr = cam_ahb_gpr;

	aon_apb_gpr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						      "sprd,aon-apb-syscon");
	if (IS_ERR(aon_apb_gpr)) {
		pr_err("cam pw domain init fail, aon_apb_gpr\n");
		return PTR_ERR(aon_apb_gpr);
	}
	pw_info->u.pike2.aon_apb_gpr = aon_apb_gpr;

	pmu_apb_gpr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						      "sprd,syscon-pmu-apb");
	if (IS_ERR(pmu_apb_gpr)) {
		pr_err("cam pw domain init fail, pmu_apb_gpr\n");
		return PTR_ERR(pmu_apb_gpr);
	}
	pw_info->u.pike2.pmu_apb_gpr = pmu_apb_gpr;

	ret = regmap_read(aon_apb_gpr, REG_AON_APB_AON_CHIP_ID0, &chip_id0);
	if (ret) {
		pw_info->u.pike2.chip_id0 = 0;
		pr_err("Read chip id0 error\n");
	} else
		pw_info->u.pike2.chip_id0 = chip_id0;

	ret = regmap_read(aon_apb_gpr, REG_AON_APB_AON_CHIP_ID1, &chip_id1);
	if (ret) {
		pw_info->u.pike2.chip_id1 = 0;
		pr_err("Read chip id1 error\n");
	} else
		pw_info->u.pike2.chip_id1 = chip_id1;

	pr_info("chip_id0 %x, chip_id1 %x\n", chip_id0, chip_id1);

	return 0;
}

struct camsys_power_ops camsys_power_ops_pike2 = {
	.sprd_campw_init = sprd_cam_pw_domain_init,
	.sprd_cam_pw_on = sprd_cam_pw_on,
	.sprd_cam_pw_off = sprd_cam_pw_off,
	.sprd_cam_domain_eb = sprd_cam_domain_eb,
	.sprd_cam_domain_disable = sprd_cam_domain_disable,
};
