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
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/version.h>

#include <linux/mfd/syscon.h>
#include <linux/pm_domain.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <linux/pm_runtime.h>
#include <linux/ion.h>
#else
#include <video/sprd_mmsys_pw_domain.h>
#include "ion.h"
#endif
#include "sprd_camsys_domain.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd_campd_r5p1: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static const char * const syscon_name[] = {
	"shutdown_en",
	"force_shutdown",
	"pd_mm_state",
	"anlg_apb_eb",
	"qos_threshold_mm"
};

enum  {
	CAMSYS_SHUTDOWN_EN = 0,
	CAMSYS_FORCE_SHUTDOWN,
	CAMSYS_PD_MM_STATE,
	CAMSYS_ANLG_APB_EB,
	CAMSYS_QOS_THRESHOLD_MM
};

static int sprd_cam_domain_eb(struct camsys_power_info *pw_info)
{
	unsigned int rst_bit;
	struct register_gpr *preg_gpr;

	pr_info("cb %pS\n", __builtin_return_address(0));
	/* mm bus enable */
	clk_prepare_enable(pw_info->u.le.cam_mm_eb);

	/* cam CKG enable */
	clk_prepare_enable(pw_info->u.le.cam_ckg_eb);

	clk_prepare_enable(pw_info->u.le.cam_clk_cphy_cfg_gate_eb);

	/* config cam ahb clk */
	clk_set_parent(pw_info->u.le.cam_ahb_clk,
		pw_info->u.le.cam_ahb_clk_parent);
	clk_prepare_enable(pw_info->u.le.cam_ahb_clk);

	/* clock for anlg_phy_g7_controller */
	preg_gpr = &pw_info->u.le.syscon_regs[CAMSYS_ANLG_APB_EB];
	regmap_update_bits(preg_gpr->gpr,
		preg_gpr->reg,
		preg_gpr->mask,
		preg_gpr->mask);

	rst_bit = (0xd << 4) | 0xd;
	preg_gpr = &pw_info->u.le.syscon_regs[CAMSYS_QOS_THRESHOLD_MM];
	regmap_update_bits(preg_gpr->gpr,
		preg_gpr->reg,
		preg_gpr->mask,
		rst_bit);
	return 0;
}

static int sprd_cam_domain_disable(struct camsys_power_info *pw_info)
{
	pr_info("cb %pS\n", __builtin_return_address(0));

	clk_set_parent(pw_info->u.le.cam_ahb_clk,
	       pw_info->u.le.cam_ahb_clk_default);
	clk_disable_unprepare(pw_info->u.le.cam_ahb_clk);
	clk_disable_unprepare(pw_info->u.le.cam_clk_cphy_cfg_gate_eb);
	clk_disable_unprepare(pw_info->u.le.cam_ckg_eb);
	clk_disable_unprepare(pw_info->u.le.cam_mm_eb);
	return 0;
}

static int sprd_cam_pw_off(struct camsys_power_info *pw_info)
{
	int ret = 0;
	unsigned int power_state1 = 0;
	unsigned int power_state2 = 0;
	unsigned int power_state3 = 0;
	unsigned int read_count = 0;
	unsigned int val = 0;
	unsigned int pd_off_state = 0;
	struct register_gpr *preg_gpr;

	pd_off_state = 0x7 << 10;

	usleep_range(300, 350);

	preg_gpr = &pw_info->u.le.syscon_regs[CAMSYS_SHUTDOWN_EN];
	regmap_update_bits(preg_gpr->gpr,
		preg_gpr->reg,
		preg_gpr->mask,
		~preg_gpr->mask);
	preg_gpr = &pw_info->u.le.syscon_regs[CAMSYS_FORCE_SHUTDOWN];
	regmap_update_bits(preg_gpr->gpr,
		preg_gpr->reg,
		preg_gpr->mask,
		preg_gpr->mask);

	do {
		cpu_relax();
		usleep_range(300, 350);
		read_count++;
		preg_gpr = &pw_info->u.le.syscon_regs[CAMSYS_PD_MM_STATE];

		ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
		if (ret)
			pr_err("fail to power off cam sys, ret %d, read count %d\n",
				ret, read_count);
		power_state1 = val & preg_gpr->mask;

		ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
		if (ret)
			pr_err("fail to power off cam sys, ret %d, read count %d\n",
				ret, read_count);
		power_state2 = val & preg_gpr->mask;

		ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
		if (ret)
			pr_err("fail to power off cam sys, ret %d, read count %d\n",
				ret, read_count);
		power_state3 = val & preg_gpr->mask;
	} while (((power_state1 != pd_off_state) && read_count < 10) ||
		(power_state1 != power_state2) ||
		(power_state2 != power_state3));

	if (power_state1 != pd_off_state) {
		pr_err("fail to get power state 0x%x\n", power_state1);
		ret = -1;
		return ret;
	}

	return 0;
}

static int sprd_cam_pw_on(struct camsys_power_info *pw_info)
{
	int ret = 0;
	unsigned int power_state1 = 0;
	unsigned int power_state2 = 0;
	unsigned int power_state3 = 0;
	unsigned int read_count = 0;
	unsigned int val = 0;
	struct register_gpr *preg_gpr;

	usleep_range(300, 350);

	preg_gpr = &pw_info->u.le.syscon_regs[CAMSYS_SHUTDOWN_EN];
	regmap_update_bits(preg_gpr->gpr,
		preg_gpr->reg,
		preg_gpr->mask,
		~preg_gpr->mask);
	preg_gpr = &pw_info->u.le.syscon_regs[CAMSYS_FORCE_SHUTDOWN];
	regmap_update_bits(preg_gpr->gpr,
		preg_gpr->reg,
		preg_gpr->mask,
		~preg_gpr->mask);


	do {
		usleep_range(300, 350);
		read_count++;
		preg_gpr = &pw_info->u.le.syscon_regs[CAMSYS_PD_MM_STATE];

		ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
		if (ret)
			pr_err("fail to power on cam sys\n");
		power_state1 = val & preg_gpr->mask;

		ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
		if (ret)
			pr_err("fail to power on cam sys\n");
		power_state2 = val & preg_gpr->mask;

		ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
		if (ret)
			pr_err("fail to power on cam sys\n");
		power_state3 = val & preg_gpr->mask;

	} while ((power_state1 && read_count < 10) ||
		(power_state1 != power_state2) ||
		(power_state2 != power_state3));

	if (power_state1) {
		pr_err("fail to get power state 0x%x\n", power_state1);
		ret = -1;
		return ret;
	}

	pr_info("Done, read count %d, cb: %pS\n",
		read_count, __builtin_return_address(0));

	return 0;
}

static long sprd_campw_init(struct platform_device *pdev, struct camsys_power_info *pw_info)
{
	struct device_node *np = pdev->dev.of_node;
	const char *pname;
	struct regmap *tregmap;
	uint32_t i, args[2];

	pw_info->u.le.cam_clk_cphy_cfg_gate_eb =
		devm_clk_get(&pdev->dev, "clk_cphy_cfg_gate_eb");
	if (IS_ERR_OR_NULL(pw_info->u.le.cam_clk_cphy_cfg_gate_eb))
		return PTR_ERR(pw_info->u.le.cam_clk_cphy_cfg_gate_eb);

	pw_info->u.le.cam_ckg_eb = devm_clk_get(&pdev->dev, "clk_gate_eb");
	if (IS_ERR(pw_info->u.le.cam_ckg_eb))
		return PTR_ERR(pw_info->u.le.cam_ckg_eb);

	pw_info->u.le.cam_mm_eb = devm_clk_get(&pdev->dev, "clk_mm_eb");
	if (IS_ERR_OR_NULL(pw_info->u.le.cam_mm_eb))
		return PTR_ERR(pw_info->u.le.cam_mm_eb);

	pw_info->u.le.cam_ahb_clk = devm_clk_get(&pdev->dev, "clk_mm_ahb");
	if (IS_ERR_OR_NULL(pw_info->u.le.cam_ahb_clk))
		return PTR_ERR(pw_info->u.le.cam_ahb_clk);

	pw_info->u.le.cam_ahb_clk_parent =
		devm_clk_get(&pdev->dev, "clk_mm_ahb_parent");
	if (IS_ERR_OR_NULL(pw_info->u.le.cam_ahb_clk_parent))
		return PTR_ERR(pw_info->u.le.cam_ahb_clk_parent);

	pw_info->u.le.cam_ahb_clk_default = clk_get_parent(pw_info->u.le.cam_ahb_clk);
	if (IS_ERR_OR_NULL(pw_info->u.le.cam_ahb_clk_default))
		return PTR_ERR(pw_info->u.le.cam_ahb_clk_default);

	/* read global register */
	for (i = 0; i < ARRAY_SIZE(syscon_name); i++) {
		pname = syscon_name[i];
		tregmap =  syscon_regmap_lookup_by_phandle_args(np, pname, 2, args);
		if (IS_ERR_OR_NULL(tregmap)) {
			pr_err("fail to read %s regmap\n", pname);
			continue;
		}

		pw_info->u.le.syscon_regs[i].gpr = tregmap;
		pw_info->u.le.syscon_regs[i].reg = args[0];
		pw_info->u.le.syscon_regs[i].mask = args[1];
		pr_info("dts[%s] 0x%x 0x%x\n", pname,
			pw_info->u.le.syscon_regs[i].reg,
			pw_info->u.le.syscon_regs[i].mask);
	}
	return 0;
}

struct camsys_power_ops camsys_power_ops_le = {
	.sprd_campw_init = sprd_campw_init,
	.sprd_cam_pw_on = sprd_cam_pw_on,
	.sprd_cam_pw_off = sprd_cam_pw_off,
	.sprd_cam_domain_eb = sprd_cam_domain_eb,
	.sprd_cam_domain_disable = sprd_cam_domain_disable,
};
