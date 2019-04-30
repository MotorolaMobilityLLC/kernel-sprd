/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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
#include <linux/mfd/syscon/sprd-glb.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <video/sprd_mm.h>

#include "cam_pw_domain.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "cam_pw_domain: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

struct cam_pw_domain_info {
	atomic_t users_pw;
	atomic_t users_clk;
	unsigned int chip_id0;
	unsigned int chip_id1;
	struct mutex client_lock;
	struct regmap *cam_ahb_gpr;
	struct regmap *aon_apb_gpr;
	struct regmap *pmu_apb_gpr;
	struct clk *cam_clk_cphy_cfg_gate_eb;
	struct clk *cam_mm_eb;
	struct clk *cam_ahb_clk;
	struct clk *cam_ahb_clk_default;
	struct clk *cam_ahb_clk_parent;
	struct clk *cam_emc_clk;
	struct clk *cam_emc_clk_default;
	struct clk *cam_emc_clk_parent;
};

static struct cam_pw_domain_info *cam_pw;

int sprd_cam_pw_domain_init(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int chip_id0 = 0, chip_id1 = 0;
	struct regmap *cam_ahb_gpr = NULL;
	struct regmap *aon_apb_gpr = NULL;
	struct regmap *pmu_apb_gpr = NULL;

	cam_pw = devm_kzalloc(&pdev->dev, sizeof(*cam_pw), GFP_KERNEL);
	if (!cam_pw)
		return -ENOMEM;

	cam_pw->cam_clk_cphy_cfg_gate_eb =
		devm_clk_get(&pdev->dev, "clk_cphy_cfg_gate_eb");
	if (IS_ERR_OR_NULL(cam_pw->cam_clk_cphy_cfg_gate_eb))
		return PTR_ERR(cam_pw->cam_clk_cphy_cfg_gate_eb);

	cam_pw->cam_mm_eb = devm_clk_get(&pdev->dev, "clk_mm_eb");
	if (IS_ERR_OR_NULL(cam_pw->cam_mm_eb))
		return PTR_ERR(cam_pw->cam_mm_eb);

	cam_pw->cam_ahb_clk = devm_clk_get(&pdev->dev, "clk_mm_ahb");
	if (IS_ERR_OR_NULL(cam_pw->cam_ahb_clk))
		return PTR_ERR(cam_pw->cam_ahb_clk);

	cam_pw->cam_ahb_clk_parent =
		devm_clk_get(&pdev->dev, "clk_mm_ahb_parent");
	if (IS_ERR_OR_NULL(cam_pw->cam_ahb_clk_parent))
		return PTR_ERR(cam_pw->cam_ahb_clk_parent);

	cam_pw->cam_ahb_clk_default = clk_get_parent(cam_pw->cam_ahb_clk);
	if (IS_ERR_OR_NULL(cam_pw->cam_ahb_clk_default))
		return PTR_ERR(cam_pw->cam_ahb_clk_default);

	/* need set cgm_mm_emc_sel :512m , DDR  matrix clk*/
	cam_pw->cam_emc_clk = devm_clk_get(&pdev->dev, "clk_mm_emc");
	if (IS_ERR_OR_NULL(cam_pw->cam_emc_clk))
		return PTR_ERR(cam_pw->cam_emc_clk);

	cam_pw->cam_emc_clk_parent =
		devm_clk_get(&pdev->dev, "clk_mm_emc_parent");
	if (IS_ERR_OR_NULL(cam_pw->cam_emc_clk_parent))
		return PTR_ERR(cam_pw->cam_emc_clk_parent);

	cam_pw->cam_emc_clk_default = clk_get_parent(cam_pw->cam_emc_clk);
	if (IS_ERR_OR_NULL(cam_pw->cam_emc_clk_default))
		return PTR_ERR(cam_pw->cam_emc_clk_default);

	cam_ahb_gpr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						"sprd,cam-ahb-syscon");
	if (IS_ERR_OR_NULL(cam_ahb_gpr))
		return PTR_ERR(cam_ahb_gpr);
	cam_pw->cam_ahb_gpr = cam_ahb_gpr;

	aon_apb_gpr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						"sprd,aon-apb-syscon");
	if (IS_ERR_OR_NULL(aon_apb_gpr))
		return PTR_ERR(aon_apb_gpr);
	cam_pw->aon_apb_gpr = aon_apb_gpr;

	pmu_apb_gpr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						"sprd,syscon-pmu-apb");
	if (IS_ERR_OR_NULL(pmu_apb_gpr))
		return PTR_ERR(pmu_apb_gpr);
	cam_pw->pmu_apb_gpr = pmu_apb_gpr;

	ret = regmap_read(aon_apb_gpr, REG_AON_APB_AON_CHIP_ID0, &chip_id0);
	if (ret) {
		cam_pw->chip_id0 = 0;
		pr_err("fail to read chip id0\n");
	} else
		cam_pw->chip_id0 = chip_id0;

	ret = regmap_read(aon_apb_gpr, REG_AON_APB_AON_CHIP_ID1, &chip_id1);
	if (ret) {
		cam_pw->chip_id1 = 0;
		pr_err("fail to read chip id1\n");
	} else
		cam_pw->chip_id1 = chip_id1;

	mutex_init(&cam_pw->client_lock);

	return 0;
}

int sprd_cam_pw_off(void)
{
	int ret = 0;
	unsigned int power_state1 = 0;
	unsigned int power_state2 = 0;
	unsigned int power_state3 = 0;
	unsigned int read_count = 0;
	unsigned int val = 0;
	unsigned int pmu_mm_bit = 0, pmu_mm_state = 0;
	unsigned int mm_off = 0;

	pr_debug("%s, count:%d, cb: %pS\n", __func__,
			atomic_read(&cam_pw->users_pw),
			__builtin_return_address(0));

	pmu_mm_bit = 27;
	pmu_mm_state = 0x1f;
	mm_off = 0x38000000;

	mutex_lock(&cam_pw->client_lock);
	if (atomic_dec_return(&cam_pw->users_pw) == 0) {

		usleep_range(300, 350);

		regmap_update_bits(cam_pw->pmu_apb_gpr,
				REG_PMU_APB_PD_MM_TOP_CFG,
				BIT_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN,
				~(unsigned int)
				BIT_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN);
		regmap_update_bits(cam_pw->pmu_apb_gpr,
				REG_PMU_APB_PD_MM_TOP_CFG,
				BIT_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN,
				BIT_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN);

		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;

			ret = regmap_read(cam_pw->pmu_apb_gpr,
					REG_PMU_APB_PWR_STATUS0_DBG, &val);
			if (ret)
				goto err_pw_off;
			power_state1 = val & (pmu_mm_state << pmu_mm_bit);

			ret = regmap_read(cam_pw->pmu_apb_gpr,
					REG_PMU_APB_PWR_STATUS0_DBG, &val);
			if (ret)
				goto err_pw_off;
			power_state2 = val & (pmu_mm_state << pmu_mm_bit);

			ret = regmap_read(cam_pw->pmu_apb_gpr,
					REG_PMU_APB_PWR_STATUS0_DBG, &val);
			if (ret)
				goto err_pw_off;
			power_state3 = val & (pmu_mm_state << pmu_mm_bit);
		} while (((power_state1 != mm_off) && read_count < 10) ||
			(power_state1 != power_state2) ||
			(power_state2 != power_state3));

		if (power_state1 != mm_off) {
			pr_err("fail to pw off camera 0x%x\n",
				power_state1);
			ret = -1;
			goto err_pw_off;
		}
	}
	mutex_unlock(&cam_pw->client_lock);
	return 0;

err_pw_off:
	pr_err("fail to pw off camera, ret: %d, count: %d!\n",
		ret, read_count);
	mutex_unlock(&cam_pw->client_lock);

	return 0;
}
EXPORT_SYMBOL(sprd_cam_pw_off);

int sprd_cam_pw_on(void)
{
	int ret = 0;
	unsigned int power_state1 = 0;
	unsigned int power_state2 = 0;
	unsigned int power_state3 = 0;
	unsigned int read_count = 0;
	unsigned int val = 0;
	unsigned int pmu_mm_bit = 0, pmu_mm_state = 0;
	unsigned int disabled_bit = 0;

	pr_debug("%s, count:%d, cb: %pS\n", __func__,
			atomic_read(&cam_pw->users_pw),
			__builtin_return_address(0));

	pmu_mm_bit = 27;
	pmu_mm_state = 0x1f;

	mutex_lock(&cam_pw->client_lock);
	if (atomic_inc_return(&cam_pw->users_pw) == 1) {
		disabled_bit = BIT_AON_APB_CLK_MM_EMC_EB |
					BIT_AON_APB_CLK_MM_AHB_EB |
					BIT_AON_APB_CLK_SENSOR2_EB |
					BIT_AON_APB_CLK_DCAM_IF_EB |
					BIT_AON_APB_CLK_ISP_EB |
					BIT_AON_APB_CLK_JPG_EB |
					BIT_AON_APB_CLK_CPP_EB |
					BIT_AON_APB_CLK_SENSOR0_EB |
					BIT_AON_APB_CLK_SENSOR1_EB |
					BIT_AON_APB_CLK_MM_VSP_EMC_EB |
					BIT_AON_APB_CLK_MM_VSP_AHB_EB |
					BIT_AON_APB_CLK_VSP_EB;
		regmap_update_bits(cam_pw->aon_apb_gpr,
			REG_AON_APB_AON_CLK_TOP_CFG,
			disabled_bit,
			~disabled_bit);

		/* cam domain power on */
		regmap_update_bits(cam_pw->pmu_apb_gpr,
				REG_PMU_APB_PD_MM_TOP_CFG,
				BIT_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN,
				~(unsigned int)
				BIT_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN);
		regmap_update_bits(cam_pw->pmu_apb_gpr,
				REG_PMU_APB_PD_MM_TOP_CFG,
				BIT_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN,
				~(unsigned int)
				BIT_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN);

		do {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;

			ret = regmap_read(cam_pw->pmu_apb_gpr,
					REG_PMU_APB_PWR_STATUS0_DBG, &val);
			if (ret)
				goto err_pw_on;
			power_state1 = val & (pmu_mm_state << pmu_mm_bit);

			ret = regmap_read(cam_pw->pmu_apb_gpr,
					REG_PMU_APB_PWR_STATUS0_DBG, &val);
			if (ret)
				goto err_pw_on;
			power_state2 = val & (pmu_mm_state << pmu_mm_bit);

			ret = regmap_read(cam_pw->pmu_apb_gpr,
					REG_PMU_APB_PWR_STATUS0_DBG, &val);
			if (ret)
				goto err_pw_on;
			power_state3 = val & (pmu_mm_state << pmu_mm_bit);

		} while ((power_state1 && read_count < 10) ||
			(power_state1 != power_state2) ||
			(power_state2 != power_state3));

		if (power_state1) {
			pr_err("fail to pw on camera 0x%x\n",
				power_state1);
			ret = -1;
			goto err_pw_on;
		}
	}
	mutex_unlock(&cam_pw->client_lock);

	return 0;

err_pw_on:
	atomic_dec_return(&cam_pw->users_pw);
	pr_err("fail to power on camera\n");
	mutex_unlock(&cam_pw->client_lock);

	return ret;
}
EXPORT_SYMBOL(sprd_cam_pw_on);

int sprd_cam_domain_eb(void)
{
	pr_debug("%s, count:%d, cb: %pS\n", __func__,
			atomic_read(&cam_pw->users_clk),
			__builtin_return_address(0));

	mutex_lock(&cam_pw->client_lock);
	if (atomic_inc_return(&cam_pw->users_clk) == 1) {
		/* config cam ahb clk */
		clk_set_parent(cam_pw->cam_ahb_clk, cam_pw->cam_ahb_clk_parent);
		clk_prepare_enable(cam_pw->cam_ahb_clk);

		/* config cam emc clk */
		clk_set_parent(cam_pw->cam_emc_clk, cam_pw->cam_emc_clk_parent);
		clk_prepare_enable(cam_pw->cam_emc_clk);

		/* mm bus enable */
		clk_prepare_enable(cam_pw->cam_mm_eb);

		clk_prepare_enable(cam_pw->cam_clk_cphy_cfg_gate_eb);
	}
	mutex_unlock(&cam_pw->client_lock);

	return 0;
}
EXPORT_SYMBOL(sprd_cam_domain_eb);

int sprd_cam_domain_disable(void)
{
	int ret = 0;
	unsigned int domain_state = 0;
	unsigned int read_count = 0;
	unsigned int val = 0;
	unsigned int pmu_mm_handshake_bit = 0;
	unsigned int pmu_mm_handshake_state = 0;
	unsigned int mm_domain_disable = 0;

	pr_debug("%s, count:%d, cb: %pS\n", __func__,
			atomic_read(&cam_pw->users_clk),
			__builtin_return_address(0));

	pmu_mm_handshake_bit = 19;
	pmu_mm_handshake_state = 0x1;
	mm_domain_disable = 0x80000;

	mutex_lock(&cam_pw->client_lock);
	if (atomic_dec_return(&cam_pw->users_clk) == 0) {

		clk_disable_unprepare(cam_pw->cam_clk_cphy_cfg_gate_eb);
		clk_disable_unprepare(cam_pw->cam_mm_eb);

		while (read_count < 10) {
			cpu_relax();
			usleep_range(300, 350);
			read_count++;

			ret = regmap_read(cam_pw->pmu_apb_gpr,
					REG_PMU_APB_BUS_STATUS0, &val);
			if (ret) {
				pr_err("fail to read mm handshake %d\n", ret);
				goto err_domain_disable;
			}
			domain_state = val & (pmu_mm_handshake_state <<
				pmu_mm_handshake_bit);
			if (domain_state) {
				pr_debug("wait for done pmu mm handshake0x%x\n",
				domain_state);
				break;
			}
		};
		if (read_count == 10) {
			pr_err("fail to wait for pmu mm handshake 0x%x\n",
				domain_state);
			ret = -1;
			goto err_domain_disable;
		}

		clk_set_parent(cam_pw->cam_emc_clk,
			cam_pw->cam_emc_clk_default);
		clk_disable_unprepare(cam_pw->cam_emc_clk);

		clk_set_parent(cam_pw->cam_ahb_clk,
			cam_pw->cam_ahb_clk_default);
		clk_disable_unprepare(cam_pw->cam_ahb_clk);
	}
	mutex_unlock(&cam_pw->client_lock);

	return 0;

err_domain_disable:
	pr_err("fail to disable cam, ret: %d, count: %d!\n",
		ret, read_count);
	mutex_unlock(&cam_pw->client_lock);

	return 0;
}
EXPORT_SYMBOL(sprd_cam_domain_disable);
