/*
 * Chip depended functions for qogirn6pro.
 *
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

#include <linux/err.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/pm_domain.h>
#include <linux/pm_wakeup.h>
#include <linux/regmap.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/regulator/consumer.h>
#include <dt-bindings/soc/sprd,qogirn6pro-regs.h>
#include <dt-bindings/soc/sprd,qogirn6pro-mask.h>

#include "vha_chipdep.h"
#include "vha_common.h"
#include "ai_sys_qos.h"
#include "vha_devfreq.h"
#include "sprd,qogirn6pro-npu-regs.h"
#include "sprd,qogirn6pro-npu-mask.h"

struct npu_regmap{
	struct regmap *ai_apb_regs;
	struct regmap *pmu_apb_regs;
};

static struct npu_regmap ai_regs;
static struct regulator *vddai = NULL;

static int get_qos_array_length(QOS_REG_T array[])
{
	int i = 0;
	while (array[i].base_addr != 0) {
		i++;
	}
	return i;
}

static void qos_parameters_set(QOS_REG_T array[])
{
	int i, length;
	void __iomem *addr;
	length = get_qos_array_length(array);
	for (i = 0; i < length; i++) {
		addr = ioremap(array[i].base_addr, 4);
		writel(readl(addr) & (~array[i].mask_value) | array[i].set_value, addr);
		iounmap(addr);
	}
}

static int vha_set_qos()
{
	qos_parameters_set(nic400_ai_main_mtx_m0_qos_list);
	qos_parameters_set(ai_apb_rf_qos_list);
	return 0;
}

static int vha_init_regs(struct device *dev)
{
	struct device_node *np = dev->of_node;

	ai_regs.ai_apb_regs = syscon_regmap_lookup_by_phandle(np, "apb_reg");
	if (IS_ERR(ai_regs.ai_apb_regs)) {
		dev_err_once(dev, "failed to get apb_reg\n");
		return -EINVAL;
	}

	ai_regs.pmu_apb_regs = syscon_regmap_lookup_by_phandle(np, "pd_ai_sys");
	if (IS_ERR(ai_regs.pmu_apb_regs)) {
		dev_err_once(dev, "failed to get pd_ai_reg\n");
		return -EINVAL;
	}

	if (!vddai) {
		vddai = devm_regulator_get(dev, "npu");
		if (IS_ERR_OR_NULL(vddai)) {
			dev_err_once(dev, "failed to get npu-supply\n");
			return PTR_ERR(vddai);
		}
	}

	return 0;
}

static int vddai_enable()
{
	int ret = 0;
	if (!vddai)
		ret = -EINVAL;
	else
		ret = regulator_enable(vddai);
	return ret;
}

static void vddai_disable()
{
	if (vddai)
		regulator_disable(vddai);
}

static void aipll_force_off(uint8_t val)
{
	if (val) {
		regmap_update_bits(ai_regs.pmu_apb_regs,
				REG_PMU_APB_AIPLL_REL_CFG,
				MASK_PMU_APB_AIPLL_FRC_OFF,
				MASK_PMU_APB_AIPLL_FRC_OFF);
		regmap_update_bits(ai_regs.pmu_apb_regs,
				REG_PMU_APB_AIPLL_REL_CFG,
				MASK_PMU_APB_AIPLL_FRC_ON,
				0);
	}
	else {
		regmap_update_bits(ai_regs.pmu_apb_regs,
				REG_PMU_APB_AIPLL_REL_CFG,
				MASK_PMU_APB_AIPLL_FRC_OFF,
				0);
		regmap_update_bits(ai_regs.pmu_apb_regs,
				REG_PMU_APB_AIPLL_REL_CFG,
				MASK_PMU_APB_AIPLL_FRC_ON,
				MASK_PMU_APB_AIPLL_FRC_ON);
	}
}

static int vha_power_on(void)
{
	uint32_t reg, mask;
	reg = REG_PMU_APB_PD_AI_CFG_0;
	mask = MASK_PMU_APB_PD_AI_FORCE_SHUTDOWN;

	regmap_update_bits(ai_regs.pmu_apb_regs, reg, mask, 0);

	return 0;
}

static int vha_power_off(void)
{
	uint32_t reg, mask;
	reg = REG_PMU_APB_PD_AI_CFG_0;
	mask = MASK_PMU_APB_PD_AI_FORCE_SHUTDOWN;

	regmap_update_bits(ai_regs.pmu_apb_regs, reg, mask, mask);

	return 0;
}

#define  LOOP_TIME  34  // (2000us - 300 us)/50us =34 times
static int vha_wait_power_ready(struct device *dev)
{
	uint32_t ret, mask, reg, val=0, loop=0;

	reg = REG_PMU_APB_PWR_STATUS_DBG_10;
	mask = MASK_PMU_APB_PD_AI_STATE;

	/* delay for setup power domain */
	udelay(300);
	/* check setup if ok*/
	if (ai_regs.pmu_apb_regs) {
		while(loop < LOOP_TIME){
			ret = regmap_read(ai_regs.pmu_apb_regs,reg, &val);
			if (ret){
				dev_err_once(dev, "read pmu_apb_regs error, regs 0x%x\n", reg);
				return ret;
			}

			if ((val & mask) == 0){
				return 0;
			}

			loop++;
			udelay(50);
		}
	}
	else {
		dev_err_once(dev, "pmu_apb_regs uninit\n");
	}

	dev_err_once(dev, "check pmu_apb_regs regs 0x%x  timeout!!!\n", reg);
	return 1;
}

static int vha_powerdomain_setup(struct device *dev)
{
	vha_power_on();
	vha_wait_power_ready(dev);

	return 0;
}

static int vha_powerdomain_unsetup(void)
{
	vha_power_off();

	return 0;
}

static int vha_auto_ckg_setup(struct device *dev)
{
	unsigned int mask;

	mask =	MASK_AI_APB_POWERVR_NNA_AUTO_GATE_EN |
		MASK_AI_APB_POWERVR_BUSMON_AUTO_GATE_EN |
		MASK_AI_APB_MAIN_MTX_FR_AUTO_GATE_EN |
		MASK_AI_APB_CFG_MTX_FR_AUTO_GATE_EN |
		MASK_AI_APB_OCM_FR_AUTO_GATE_EN |
		MASK_AI_APB_DVFS_FR_AUTO_GATE_EN;

	regmap_update_bits(ai_regs.ai_apb_regs, REG_AI_APB_USER_GATE_AUTO_GATE_EN,
				mask, mask);

	return 0;
}

static struct clk_bulk_data clks[] = {
	[0].id = "ai_apll",
	[1].id = "ai_eb",
	[2].id = "powervr_eb",
	[3].id = "mtx_eb",
	[4].id = "dvfs_eb",
	[5].id = "ocm_eb",
	/* pmon_eb: performance monitor enable, default is disabled*/
	/* aon_to_ocm_eb: aon to ocm path enable, default is disabled*/
};

static int vha_clockdomain_init(struct device *dev)
{
	int num_clks = ARRAY_SIZE(clks);
	int ret;

	ret = clk_bulk_get(dev, num_clks, &clks[0]);
	if (ret) {
		dev_err(dev, "clk_bulk_get failed, ret %d\n", ret);
		return ret;
	}

	return 0;
}

static int vha_clockdomain_setup(struct device *dev)
{
	int num_clks = ARRAY_SIZE(clks);
	int ret = 0;

	ret = clk_bulk_prepare_enable(num_clks, clks);
	if (ret) {
		dev_err(dev, "clk_bulk_prepare_enable failed, ret %d\n", ret);
		return ret;
	}

	vha_auto_ckg_setup(dev);

	return 0;
}

static int vha_clockdomain_unsetup(struct device *dev)
{
	int num_clks = ARRAY_SIZE(clks);

	clk_bulk_disable_unprepare(num_clks, clks);

	return 0;
}

int vha_chip_init(struct device *dev)
{
	device_set_wakeup_capable(dev, true);
	device_wakeup_enable(dev);
	vha_init_regs(dev);
	vha_powerdomain_setup(dev);
	vha_clockdomain_init(dev);
#ifdef VHA_DEVFREQ
	vha_dvfs_ctx_init(dev);
#endif

	return 0;
}

int vha_chip_deinit(struct device *dev)
{
#ifdef VHA_DEVFREQ
	vha_dvfs_ctx_deinit(dev);
#endif
	device_wakeup_disable(dev);

	return 0;
}

int vha_chip_runtime_resume(struct device *dev)
{
	int ret;
	aipll_force_off(0);
	ret = vddai_enable();
	if (ret) {
		dev_err(dev, "failed to enable vddai:%d\n", ret);
		return ret;
	}
	udelay(400);
	vha_powerdomain_setup(dev);
	vha_clockdomain_setup(dev);
	vha_set_qos();
#ifdef VHA_DEVFREQ
	vha_devfreq_resume(dev);
#endif

	return 0;
}

int vha_chip_runtime_suspend(struct device *dev)
{
#ifdef VHA_DEVFREQ
	vha_devfreq_suspend(dev);
#endif
	vha_clockdomain_unsetup(dev);
	vha_powerdomain_unsetup();
	vddai_disable();
	aipll_force_off(1);

	return 0;
}

#define VHA_N6PRO_DMAMASK DMA_BIT_MASK(35)
u64 vha_get_chip_dmamask(void)
{
	return VHA_N6PRO_DMAMASK;
}

