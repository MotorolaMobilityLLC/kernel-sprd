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
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/pm_domain.h>

#include "sprd_camsys_domain.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd_campd_r5p0: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static const char * const syscon_name[] = {
	"shutdown-en",
	"force-shutdown",
	"pwr-status0",
	"bus-status0",
	"aon-apb-mm-eb",
	"init-dis-bits"
};

enum  {
	CAMSYS_SHUTDOWN_EN = 0,
	CAMSYS_FORCE_SHUTDOWN,
	CAMSYS_PWR_STATUS0,
	CAMSYS_BUS_STATUS0,
	CAMSYS_AON_APB_MM_EB,
	CAMSYS_INIT_DIS_BITS
};

static int boot_mode_check(void)
{
	struct device_node *np;
	const char *cmd_line;
	int ret = 0;

	np = of_find_node_by_path("/chosen");
	if (!np)
		return 0;

	ret = of_property_read_string(np, "bootargs", &cmd_line);
	if (ret < 0)
		return 0;

	if (strstr(cmd_line, "androidboot.mode=cali") ||
			strstr(cmd_line, "sprdboot.mode=cali"))
		ret = 1;

	return ret;
}

static int sprd_cam_domain_eb(struct camsys_power_info *pw_info)
{
	pr_info("cb %pS\n", __builtin_return_address(0));

	/* config cam ahb clk */
	clk_set_parent(pw_info->u.l3.cam_ahb_clk,
		pw_info->u.l3.cam_ahb_clk_parent);
	clk_prepare_enable(pw_info->u.l3.cam_ahb_clk);

	/* config cam emc clk */
	clk_set_parent(pw_info->u.l3.cam_emc_clk,
		pw_info->u.l3.cam_emc_clk_parent);
	clk_prepare_enable(pw_info->u.l3.cam_emc_clk);

	/* mm bus enable */
	clk_prepare_enable(pw_info->u.l3.cam_mm_eb);

	clk_prepare_enable(pw_info->u.l3.cam_clk_cphy_cfg_gate_eb);

	return 0;
}

static int sprd_cam_domain_disable(struct camsys_power_info *pw_info)
{
	int ret = 0;
	unsigned int domain_state = 0;
	unsigned int read_count = 0;
	unsigned int val = 0;
	unsigned int pmu_mm_handshake_bit = 0;
	unsigned int pmu_mm_handshake_state = 0;
	unsigned int mm_domain_disable = 0;
	struct register_gpr *preg_gpr;

	pr_info("cb %pS\n", __builtin_return_address(0));

	pmu_mm_handshake_bit = 19;
	pmu_mm_handshake_state = 0x1;
	mm_domain_disable = 0x80000;

	clk_disable_unprepare(pw_info->u.l3.cam_clk_cphy_cfg_gate_eb);
	clk_disable_unprepare(pw_info->u.l3.cam_mm_eb);

	while (read_count++ < 10) {
		usleep_range(300, 350);
		preg_gpr = &pw_info->u.l3.syscon_regs[CAMSYS_BUS_STATUS0];

		ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
		if (ret) {
			pr_err("fail to disable cam power domain, ret %d, count %d\n",
				ret, read_count);
			return ret;
		}
		domain_state = val & (pmu_mm_handshake_state <<
			pmu_mm_handshake_bit);
		if (domain_state) {
			pr_debug("wait for done pmu mm handshake0x%x\n",
			domain_state);
			break;
		}
	}
	if (read_count == 10) {
		pr_err("fail to wait for pmu mm handshake 0x%x\n",
			domain_state);
		ret = -EIO;
		return ret;
	}

	clk_set_parent(pw_info->u.l3.cam_emc_clk,
		pw_info->u.l3.cam_emc_clk_default);
	clk_disable_unprepare(pw_info->u.l3.cam_emc_clk);

	clk_set_parent(pw_info->u.l3.cam_ahb_clk,
		pw_info->u.l3.cam_ahb_clk_default);
	clk_disable_unprepare(pw_info->u.l3.cam_ahb_clk);

	return 0;
}

static int sprd_cam_pw_off(struct camsys_power_info *pw_info)
{
	int ret = 0;
	unsigned int power_state1 = 0;
	unsigned int read_count = 0;
	unsigned int val = 0;
	unsigned int pmu_mm_bit = 0, pmu_mm_state = 0;
	unsigned int mm_off = 0;
	struct register_gpr *preg_gpr;

	pmu_mm_bit = 27;
	pmu_mm_state = 0x1f;
	mm_off = 0x38000000;

	usleep_range(300, 350);

	preg_gpr = &pw_info->u.l3.syscon_regs[CAMSYS_SHUTDOWN_EN];
	regmap_update_bits(preg_gpr->gpr,
			preg_gpr->reg,
			preg_gpr->mask,
			~preg_gpr->mask);
	preg_gpr = &pw_info->u.l3.syscon_regs[CAMSYS_FORCE_SHUTDOWN];
	regmap_update_bits(preg_gpr->gpr,
			preg_gpr->reg,
			preg_gpr->mask,
			preg_gpr->mask);

	do {
		usleep_range(300, 350);
		read_count++;
		preg_gpr = &pw_info->u.l3.syscon_regs[CAMSYS_PWR_STATUS0];

		ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
		if (ret) {
			pr_err("fail to power off cam sys, ret %d, read count %d\n",
				ret, read_count);
			return ret;
		}
		power_state1 = val & (pmu_mm_state << pmu_mm_bit);
	} while ((power_state1 != mm_off) && read_count < 10);

	if (power_state1 != mm_off) {
		pr_err("fail to get power state 0x%x\n", power_state1);
		ret = -EIO;
		return ret;
	}

	pr_info("Done, read count %d, cb: %pS\n",
		read_count, __builtin_return_address(0));
	return 0;
}

static int sprd_cam_pw_on(struct camsys_power_info *pw_info)
{
	int ret = 0;
	unsigned int power_state1 = 0;
	unsigned int read_count = 0;
	unsigned int val = 0;
	unsigned int pmu_mm_bit = 0, pmu_mm_state = 0;
	struct register_gpr *preg_gpr;

	pmu_mm_bit = 27;
	pmu_mm_state = 0x1f;

	preg_gpr = &pw_info->u.l3.syscon_regs[CAMSYS_INIT_DIS_BITS];
	regmap_update_bits(preg_gpr->gpr,
			preg_gpr->reg,
			preg_gpr->mask,
			~preg_gpr->mask);

	/* cam domain power on */
	preg_gpr = &pw_info->u.l3.syscon_regs[CAMSYS_SHUTDOWN_EN];
	regmap_update_bits(preg_gpr->gpr,
			preg_gpr->reg,
			preg_gpr->mask,
			~preg_gpr->mask);
	preg_gpr = &pw_info->u.l3.syscon_regs[CAMSYS_FORCE_SHUTDOWN];
	regmap_update_bits(preg_gpr->gpr,
			preg_gpr->reg,
			preg_gpr->mask,
			~preg_gpr->mask);

	do {
		usleep_range(300, 350);
		read_count++;
		preg_gpr = &pw_info->u.l3.syscon_regs[CAMSYS_PWR_STATUS0];

		ret = regmap_read(preg_gpr->gpr, preg_gpr->reg, &val);
		if (ret) {
			pr_err("fail to read state reg\n");
			return ret;
		}
		power_state1 = val & (pmu_mm_state << pmu_mm_bit);
	} while ((power_state1 && read_count < 10));

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
	struct register_gpr *preg_gpr;
	uint32_t i, args[2];

	pw_info->u.l3.cam_clk_cphy_cfg_gate_eb =
		devm_clk_get(&pdev->dev, "clk_cphy_cfg_gate_eb");
	if (IS_ERR_OR_NULL(pw_info->u.l3.cam_clk_cphy_cfg_gate_eb))
		return PTR_ERR(pw_info->u.l3.cam_clk_cphy_cfg_gate_eb);

	pw_info->u.l3.cam_mm_eb = devm_clk_get(&pdev->dev, "clk_mm_eb");
	if (IS_ERR_OR_NULL(pw_info->u.l3.cam_mm_eb))
		return PTR_ERR(pw_info->u.l3.cam_mm_eb);

	pw_info->u.l3.cam_ahb_clk = devm_clk_get(&pdev->dev, "clk_mm_ahb");
	if (IS_ERR_OR_NULL(pw_info->u.l3.cam_ahb_clk))
		return PTR_ERR(pw_info->u.l3.cam_ahb_clk);

	pw_info->u.l3.cam_ahb_clk_parent =
		devm_clk_get(&pdev->dev, "clk_mm_ahb_parent");
	if (IS_ERR_OR_NULL(pw_info->u.l3.cam_ahb_clk_parent))
		return PTR_ERR(pw_info->u.l3.cam_ahb_clk_parent);

	pw_info->u.l3.cam_ahb_clk_default = clk_get_parent(pw_info->u.l3.cam_ahb_clk);
	if (IS_ERR_OR_NULL(pw_info->u.l3.cam_ahb_clk_default))
		return PTR_ERR(pw_info->u.l3.cam_ahb_clk_default);

	/* need set cgm_mm_emc_sel :512m , DDR  matrix clk*/
	pw_info->u.l3.cam_emc_clk = devm_clk_get(&pdev->dev, "clk_mm_emc");
	if (IS_ERR_OR_NULL(pw_info->u.l3.cam_emc_clk))
		return PTR_ERR(pw_info->u.l3.cam_emc_clk);

	pw_info->u.l3.cam_emc_clk_parent =
		devm_clk_get(&pdev->dev, "clk_mm_emc_parent");
	if (IS_ERR_OR_NULL(pw_info->u.l3.cam_emc_clk_parent))
		return PTR_ERR(pw_info->u.l3.cam_emc_clk_parent);

	pw_info->u.l3.cam_emc_clk_default = clk_get_parent(pw_info->u.l3.cam_emc_clk);
	if (IS_ERR_OR_NULL(pw_info->u.l3.cam_emc_clk_default))
		return PTR_ERR(pw_info->u.l3.cam_emc_clk_default);

	/* read global register */
	for (i = 0; i < ARRAY_SIZE(syscon_name); i++) {
		pname = syscon_name[i];
		tregmap =  syscon_regmap_lookup_by_phandle_args(np, pname, 2, args);
		if (IS_ERR_OR_NULL(tregmap)) {
			pr_err("fail to read %s regmap\n", pname);
			continue;
		}
		pw_info->u.l3.syscon_regs[i].gpr = tregmap;
		pw_info->u.l3.syscon_regs[i].reg = args[0];
		pw_info->u.l3.syscon_regs[i].mask = args[1];
		pr_info("dts[%s] 0x%x 0x%x\n", pname,
			pw_info->u.l3.syscon_regs[i].reg,
			pw_info->u.l3.syscon_regs[i].mask);
	}

	//fix bug 1707122
	if (boot_mode_check()) {
		preg_gpr = &pw_info->u.l3.syscon_regs[CAMSYS_AON_APB_MM_EB];
		regmap_update_bits(preg_gpr->gpr,
					preg_gpr->reg,
					preg_gpr->mask,
					~preg_gpr->mask);

		preg_gpr = &pw_info->u.l3.syscon_regs[CAMSYS_SHUTDOWN_EN];
		regmap_update_bits(preg_gpr->gpr,
					preg_gpr->reg,
					preg_gpr->mask,
					~preg_gpr->mask);

		preg_gpr = &pw_info->u.l3.syscon_regs[CAMSYS_FORCE_SHUTDOWN];
		regmap_update_bits(preg_gpr->gpr,
					preg_gpr->reg,
					preg_gpr->mask,
					preg_gpr->mask);
		pr_info("calibration mode MM SHUTDOWN");
	}

	return 0;
}

struct camsys_power_ops camsys_power_ops_l3 = {
	.sprd_campw_init = sprd_campw_init,
	.sprd_cam_pw_on = sprd_cam_pw_on,
	.sprd_cam_pw_off = sprd_cam_pw_off,
	.sprd_cam_domain_eb = sprd_cam_domain_eb,
	.sprd_cam_domain_disable = sprd_cam_domain_disable,
};
