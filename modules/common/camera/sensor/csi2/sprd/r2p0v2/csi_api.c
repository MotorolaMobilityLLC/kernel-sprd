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
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/semaphore.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mfd/syscon.h>

#include "sprd_mm.h"
#include "csi_api.h"
#include "csi_driver.h"
#include "sprd_sensor_core.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CSI_API: %d %s: "\
	fmt, __LINE__, __func__

#ifdef CONFIG_PHYS_ADDR_T_64BIT
#define PRIx64 "llx"
#else
#define PRIx64 "x"
#endif

static struct csi_dt_node_info *s_csi_dt_info_p[SPRD_SENSOR_ID_MAX];

static struct csi_dt_node_info *csi_get_dt_node_data(int sensor_id)
{
	return s_csi_dt_info_p[sensor_id];
}

static int csi_mipi_clk_enable(int sensor_id)
{
	struct csi_dt_node_info *dt_info = s_csi_dt_info_p[sensor_id];
	int ret = 0;

	if (!dt_info || !dt_info->mipi_gate_clk || !dt_info->csi_eb_clk ||
		!dt_info->csi_from_clk) {
		pr_err("fail to enable mipi clk\n");
		return -EINVAL;
	}

	ret = clk_prepare_enable(dt_info->csi_eb_clk);
	if (ret) {
		pr_err("fail to enable csi eb clk\n");
		return ret;
	}

	ret = clk_prepare_enable(dt_info->mipi_gate_clk);
	if (ret) {
		pr_err("fail to enable mipi csi mipi gate clk\n");
		return ret;
	}

	ret = clk_prepare_enable(dt_info->csi_from_clk);
	if (ret) {
		pr_err("fail to enable mipi csi csi from clk\n");
		return ret;
	}
	pr_debug("csi mipi clk enable OK\n");

	return ret;
}

static void csi_mipi_clk_disable(int sensor_id)
{
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);

	if (!dt_info || !dt_info->mipi_gate_clk || !dt_info->csi_eb_clk) {
		pr_err("fail to disable mipi clk\n");
		return;
	}

	clk_disable_unprepare(dt_info->mipi_gate_clk);
	clk_disable_unprepare(dt_info->csi_eb_clk);
	if (dt_info->csi_from_clk)
		clk_disable_unprepare(dt_info->csi_from_clk);
}

int csi_api_mipi_phy_cfg_init(struct device_node *phy_node, int sensor_id)
{
	unsigned int phy_id = 0;

	of_property_read_u32(phy_node, "sprd,phyid", &phy_id);
	if (phy_id >= PHY_MAX) {
		pr_err("fail to parse phyid:%u\n", phy_id);
		return -1;
	}

	return phy_id;
}

int csi_api_dt_node_init(struct device *dev, struct device_node *dn,
		int sensor_id, unsigned int phy_id)
{
	struct csi_dt_node_info *csi_info = NULL;
	struct resource res;
	void __iomem *reg_base = NULL;
	struct regmap *regmap_syscon = NULL;

	csi_info = devm_kzalloc(dev, sizeof(*csi_info), GFP_KERNEL);
	if (!csi_info)
		return -ENOMEM;

	/* read address */
	of_address_to_resource(dn, 0, &res);
	reg_base = devm_ioremap_nocache(dev, res.start, resource_size(&res));
	if (IS_ERR(reg_base))
		return PTR_ERR(reg_base);
	csi_info->reg_base = (unsigned long)reg_base;

	csi_info->phy.phy_id = phy_id;
	pr_debug("csi node name %s\n", dn->name);

	of_property_read_u32_index(dn, "sprd,csi-id", 0, &csi_info->id);

	pr_info("csi %d ,reg phy addr base 0x%"PRIx64", size 0x%"PRIx64", reg base 0x%lx\n",
			csi_info->id, res.start,
			resource_size(&res),
			csi_info->reg_base);

	/* read clocks */
	csi_info->mipi_gate_clk = of_clk_get_by_name(dn,
					"clk_mipi_csi_gate_eb");

	csi_info->csi_eb_clk = of_clk_get_by_name(dn, "clk_csi_eb");

	csi_info->csi_from_clk = of_clk_get_by_name(dn, "clk_csi_from");
	/* read version */
	if (of_property_read_u32_index(dn, "sprd,ip-version", 0,
				       &csi_info->ip_version)) {
		pr_err("fail to read version\n");
		devm_kfree(dev, csi_info);
		return -EINVAL;
	}

	/* read interrupt */
	csi_info->csi_irq0 = irq_of_parse_and_map(dn, 0);
	csi_info->csi_irq1 = irq_of_parse_and_map(dn, 1);

	regmap_syscon = syscon_regmap_lookup_by_phandle(dn,
					"sprd,cam-ahb-syscon");
	if (IS_ERR(regmap_syscon))
		return PTR_ERR(regmap_syscon);
	csi_info->phy.cam_ahb_syscon = regmap_syscon;

	regmap_syscon = syscon_regmap_lookup_by_phandle(dn,
					"sprd,ana-apb-syscon");
	if (IS_ERR(regmap_syscon))
		return PTR_ERR(regmap_syscon);
	csi_info->phy.ana_apb_syscon = regmap_syscon;

	regmap_syscon = syscon_regmap_lookup_by_phandle(dn,
					"sprd,anlg_phy_g7_controller");
	if (IS_ERR(regmap_syscon))
		return PTR_ERR(regmap_syscon);
	csi_info->phy.anlg_phy_g7_syscon = regmap_syscon;

	/*
	 * TODO: removed only for ag7_controller
	csi_phy_power_down(&csi_info->phy, csi_info->id, 1);
	 */
	s_csi_dt_info_p[sensor_id] = csi_info;
	pr_info("csi dt info:sensor_id :%d, csi_info:0x%p\n",
				sensor_id, csi_info);

	return 0;
}

static enum csi_error_t csi_api_init(unsigned int bps_per_lane,
				     unsigned int phy_id, int sensor_id)
{
	enum csi_error_t e = SUCCESS;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);

	do {
		pr_info("sensor id %d\n", sensor_id);
		csi_phy_power_down(&dt_info->phy, dt_info->id, 0);
		csi_enable(&dt_info->phy, dt_info->id, sensor_id);
		e = csi_init(dt_info->reg_base, dt_info->ip_version, sensor_id);
		if (e != SUCCESS) {
			pr_err("fail to initialise driver\n");
			break;
		}
		dphy_init(&dt_info->phy, bps_per_lane,
			dt_info->phy.phy_id, sensor_id);
	} while (0);

	return e;
}

static uint8_t csi_api_start(int sensor_id)
{
	enum csi_error_t e = SUCCESS;

	do {
		/* set only one lane (lane 0) as active (ON) */
		e = csi_set_on_lanes(1, sensor_id);
		if (e != SUCCESS) {
			pr_err("fail to set lanes\n");
			break;
		}
		e = csi_shut_down_phy(0, sensor_id);
		if (e != SUCCESS) {
			pr_err("fail to bring up PHY\n");
			break;
		}
		e = csi_reset_phy(sensor_id);
		if (e != SUCCESS) {
			pr_err("fail to reset PHY\n");
			break;
		}
		e = csi_reset_controller(sensor_id);
		if (e != SUCCESS) {
			pr_err("fail to reset controller\n");
			break;
		}
		/* MASK all interrupts */
		csi_event_disable(0xffffffff, 1, sensor_id);
		csi_event_disable(0xffffffff, 2, sensor_id);
	} while (0);
	pr_debug("X\n");
	return e;
}

int csi_api_mipi_phy_cfg(void)
{
	pr_debug("no need to make mipi_phy cfg\n");
	return 0;
}

int csi_api_open(int bps_per_lane, int phy_id, int lane_num, int sensor_id, int is_pattern,
	int is_cphy, uint64_t lane_seq)
{
	int ret = 0;

	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);

	ret = csi_ahb_reset(&dt_info->phy, dt_info->id);
	if (unlikely(ret))
		goto EXIT;

	ret = csi_mipi_clk_enable(sensor_id);
	if (ret) {
		pr_err("fail to eanbale clk\n");
		csi_mipi_clk_disable(sensor_id);
		goto EXIT;
	}
	udelay(1);
	ret = csi_api_init(bps_per_lane, phy_id, sensor_id);
	if (ret)
		goto EXIT;
	ret = csi_api_start(sensor_id);
	if (ret)
		goto EXIT;

	ret = csi_set_on_lanes(lane_num, sensor_id);
	if (ret)
		goto EXIT;
	return ret;
EXIT:
	pr_err("fail to open api %d\n", ret);
	csi_api_close(phy_id, sensor_id);
	return ret;
}

uint8_t csi_api_close(uint32_t phy_id, int sensor_id)
{
	int ret = 0;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);

	ret = csi_close(sensor_id);
	csi_phy_power_down(&dt_info->phy, dt_info->id, 1);
	csi_mipi_clk_disable(sensor_id);
	pr_debug("close api %d\n", ret);

	return ret;
}

void csi_api_reg_trace(void)
{
	csi_dump_reg();
}
EXPORT_SYMBOL(csi_api_reg_trace);

int csi_api_switch(int sensor_id)
{
	return 0;
}
