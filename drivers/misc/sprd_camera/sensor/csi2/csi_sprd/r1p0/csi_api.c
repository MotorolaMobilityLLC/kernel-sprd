/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
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
#include <linux/mfd/syscon/sprd-glb.h>
#include <linux/mfd/syscon.h>
#include "csi_api.h"
#include "csi_driver.h"
#include <video/sprd_mm.h>

static struct csi_dt_node_info *s_csi_dt_info_p[4];
static struct mipi_phy_info phy_info[3];

static int csi_ch_enable(unsigned int sensor_id);
static int csi_ch_disable(unsigned int sensor_id);

static struct csi_dt_node_info *csi_get_dt_node_data(int sensor_id)
{
	return s_csi_dt_info_p[sensor_id];
}

static int csi_mipi_clk_enable(int sensor_id)
{
	struct csi_dt_node_info *dt_info = s_csi_dt_info_p[sensor_id];
	int ret = 0;

	if (!dt_info || !dt_info->mipi_gate_clk || !dt_info->cphy_gate_clk
	    || !dt_info->csi_eb_clk) {
		pr_err("CSI: mipi clk enable err\n");
		return -EINVAL;

	}
	ret = clk_prepare_enable(dt_info->cphy_gate_clk);
	if (ret) {
		pr_err("mipi csi cphy clk err\n");
		return ret;
	}
	ret = clk_prepare_enable(dt_info->mipi_gate_clk);
	if (ret) {
		pr_err("mipi csi mipi gate clk err\n");
		return ret;
	}
	ret = clk_prepare_enable(dt_info->csi_eb_clk);
	if (ret) {
		pr_err("csi eb clk err\n");
		return ret;
	}
	ret = clk_prepare_enable(dt_info->mipi_csi_eb_clk);
	if (ret) {
		pr_err("mipi csi eb clk err\n");
		return ret;
	}
	pr_debug("csi mipi clk enable OK\n");
	return ret;
}

static void csi_mipi_clk_disable(int sensor_id)
{
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);

	if (!dt_info || !dt_info->mipi_gate_clk || !dt_info->cphy_gate_clk
	    || !dt_info->csi_eb_clk) {
		pr_err("CSI: mipi clk disable err\n");
		return;
	}
	clk_disable_unprepare(dt_info->cphy_gate_clk);
	clk_disable_unprepare(dt_info->mipi_gate_clk);
	clk_disable_unprepare(dt_info->csi_eb_clk);
	clk_disable_unprepare(dt_info->mipi_csi_eb_clk);
}

int csi_api_dt_node_init(struct device *dev, struct device_node *dn,
			 int sensor_id, unsigned int phy_id)
{
	struct csi_dt_node_info *csi_info = NULL;
	struct resource res;
	void __iomem *reg_base;
	struct regmap *cam_ahb_syscon;
	struct regmap *ana_apb_syscon;

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
	pr_info("csi node name %s\n", dn->name);
	if (strncasecmp(dn->name, "csi0", 4) == 0)
		csi_info->id = 0;
	else
		csi_info->id = 1;
	pr_info("csi %d reg base %lx\n", csi_info->id, csi_info->reg_base);

	/* read clocks */
	csi_info->cphy_gate_clk = of_clk_get_by_name(dn, "clk_cphy_gate");
	csi_info->mipi_gate_clk = of_clk_get_by_name(dn, "clk_mipi_gate");
	csi_info->csi_eb_clk = of_clk_get_by_name(dn, "clk_csi_eb");
	csi_info->mipi_csi_eb_clk = of_clk_get_by_name(dn, "clk_mipi_csi_eb");

	/* read version */
	if (of_property_read_u32_index(dn, "sprd,ip-version", 0,
				       &csi_info->ip_version)) {
		pr_err("csi2_init: error reading version\n");
		devm_kfree(dev, csi_info);
		return -EINVAL;
	}

	/* read interrupt */
	csi_info->csi_irq0 = irq_of_parse_and_map(dn, 0);
	csi_info->csi_irq1 = irq_of_parse_and_map(dn, 1);
	pr_info("csi interrupts %d %d\n", csi_info->csi_irq0,
		csi_info->csi_irq1);

	cam_ahb_syscon = syscon_regmap_lookup_by_phandle(dn,
							 "sprd,cam-ahb-syscon");
	if (IS_ERR(cam_ahb_syscon))
		return PTR_ERR(cam_ahb_syscon);
	csi_info->phy.cam_ahb_syscon = cam_ahb_syscon;

	ana_apb_syscon = syscon_regmap_lookup_by_phandle(dn,
							 "sprd,ana-apb-syscon");
	if (IS_ERR(ana_apb_syscon))
		return PTR_ERR(ana_apb_syscon);
	csi_info->phy.ana_apb_syscon = ana_apb_syscon;

	csi_phy_power_down(&csi_info->phy, csi_info->id, 1);
	/* workaround for power down csi2 */
	if (csi_info->id == 0)
		csi_phy_power_down(&csi_info->phy, 2, 1);

	s_csi_dt_info_p[sensor_id] = csi_info;
	pr_info("sensorid %d irq0:%u, irq1:%u\n", sensor_id,
		csi_info->csi_irq0, csi_info->csi_irq1);

	return 0;
}

static enum csi_error_t csi_api_init(unsigned int bps_per_lane,
				     unsigned int phy_id, int sensor_id)
{
	enum csi_error_t e = SUCCESS;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);

	do {
		csi_phy_power_down(&dt_info->phy, dt_info->id, 0);
		csi_ch_enable(sensor_id);
		pr_err("%s: init %d\n", __func__, sensor_id);
		csi_enable(&dt_info->phy, dt_info->id);
		e = csi_init(dt_info->reg_base, dt_info->ip_version, sensor_id);
		if (e != SUCCESS) {
			pr_err("%s: Unable to initialise driver", __func__);
			break;
		}
		dphy_init(&dt_info->phy, bps_per_lane, phy_id, sensor_id);
	} while (0);

	return e;
}

static int csi_ch_enable(unsigned int sensor_id)
{
	unsigned int value = 0;
	void __iomem *base_address = NULL;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	struct csi_phy_info *phy = &dt_info->phy;
	unsigned int csi_id = dt_info->id;

	pr_info("ch sensor_id:%d => phy_id:%d => csi_id:%d\n",
			sensor_id, phy->phy_id, dt_info->id);

	base_address = ioremap_nocache(0x40400238, 0x10);
	value = __raw_readl(base_address);

	/* bit[21:22]: phy0_sel; bit[23:24]: phy1_sel; bit[25:26]: phy2_sel */
	if (phy->phy_id == 0) {
		value &= ~(0x3 << 21);
		value |= ((0x3 & csi_id) << 21);
#ifndef CONFIG_SPRD_CAM_PIP_VIV
		value &= ~(0x3 << 23);
		value &= ~(0x3 << 25);
		if (csi_id == 0) {
			value |= ((0x3 & 1) << 23);
			value |= ((0x3 & 2) << 25);
		} else if (csi_id == 1) {
			value |= ((0x3 & 0) << 23);
			value |= ((0x3 & 2) << 25);
		}
#endif
	} else if (phy->phy_id == 1) {
		value &= ~(0x3 << 23);
		value |= ((0x3 & csi_id) << 23);
#ifndef CONFIG_SPRD_CAM_PIP_VIV
		value &= ~(0x3 << 21);
		value &= ~(0x3 << 25);
		if (csi_id == 0) {
			value |= ((0x3 & 1) << 21);
			value |= ((0x3 & 2) << 25);
		} else if (csi_id == 1) {
			value |= ((0x3 & 0) << 21);
			value |= ((0x3 & 2) << 25);
		}
#endif
	} else {
		value &= ~(0x3 << 25);
		value |= ((0x3 & csi_id) << 25);
#ifndef CONFIG_SPRD_CAM_PIP_VIV
		value &= ~(0x3 << 21);
		value &= ~(0x3 << 23);
		if (csi_id == 0) {
			value |= ((0x3 & 1) << 21);
			value |= ((0x3 & 2) << 23);
		} else if (csi_id == 1) {
			value |= ((0x3 & 0) << 21);
			value |= ((0x3 & 2) << 23);
		}
#endif
	}

	/* bit[17:18]: csi0_sel; bit[19:20]: csi1_sel */
	if (csi_id == 0) {
		value &= ~(0x03 << 17);
		value |= (0x3 & phy->phy_id) << 17;
#ifndef CONFIG_SPRD_CAM_PIP_VIV
		value &= ~(0x03 << 19);
		if (phy->phy_id == 0)
			value |= (0x3 & 1) << 19;
		else if (1 == phy->phy_id || 2 == phy->phy_id)
			value |= (0x3 & 0) << 19;
#endif
	} else {
		value &= ~(0x03 << 19);
		value |= (0x3 & phy->phy_id) << 19;
#ifndef CONFIG_SPRD_CAM_PIP_VIV
		value &= ~(0x03 << 17);
		if (phy->phy_id == 0)
			value |= (0x3 & 1) << 17;
		else if (1 == phy->phy_id || 2 == phy->phy_id)
			value |= (0x3 & 0) << 17;
#endif
	}
	pr_info("%s, phy_cfg_val: 0x%x\n", __func__, value);

	__raw_writel(value, base_address);
	iounmap(base_address);

	return SUCCESS;
}

static int csi_ch_disable(unsigned int sensor_id)
{
	unsigned int value = 0;
	void __iomem *base_address = NULL;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	struct csi_phy_info *phy = &dt_info->phy;
	unsigned int csi_id = dt_info->id;

	pr_info("ch sensor_id:%d => phy_id:%d => csi_id:%d\n",
			sensor_id, phy->phy_id, dt_info->id);
	base_address = ioremap_nocache(0x40400238, 0x10);
	value = __raw_readl(base_address);

	/* bit[21:22]: phy0_sel; bit[23:24]: phy1_sel; bit[25:26]: phy2_sel */
	if (phy->phy_id == 0) {
		value &= ~(0x3 << 21);
		value |= 2 << 21;
	} else if (phy->phy_id == 1) {
		value &= ~(0x3 << 23);
		value |= 2 << 23;
	} else {
		value &= ~(0x3 << 25);
		value |= 2 << 25;
	}

	/* bit[17:18]: csi0_sel; bit[19:20]: csi1_sel */
	if (csi_id == 0) {
		value &= ~(0x03 << 17);
		value |= 0x03 << 17;
	} else {
		value &= ~(0x03 << 19);
		value |= 0x03 << 19;
	}
	pr_info("%s, phy_cfg_val: 0x%x\n", __func__, value);

	__raw_writel(value, base_address);
	iounmap(base_address);

	return SUCCESS;
}

static uint8_t csi_api_start(int sensor_id)
{
	enum csi_error_t e = SUCCESS;

	do {
		/* set only one lane (lane 0) as active (ON) */
		e = csi_set_on_lanes(1, sensor_id);
		if (e != SUCCESS) {
			pr_err("%s:Unable to set lanes\n", __func__);
			break;
		}
		e = csi_shut_down_phy(0, sensor_id);
		if (e != SUCCESS) {
			pr_err("%s Unable to bring up PHY\n", __func__);
			break;
		}
		e = csi_reset_phy(sensor_id);
		if (e != SUCCESS) {
			pr_err("%s Unable to reset PHY\n", __func__);
			break;
		}
		e = csi_reset_controller(sensor_id);
		if (e != SUCCESS) {
			pr_err("%s: Unable to reset controller\n", __func__);
			break;
		}
		/* MASK all interrupts */
		csi_event_disable(0xffffffff, 1, sensor_id);
		csi_event_disable(0xffffffff, 2, sensor_id);
	} while (0);
	pr_debug("%s\n", __func__);
	return e;
}

static int csi_phy_cfg(int phy0_sel, int phy1_sel, int phy2_sel)
{
	unsigned int value = 0;

	void __iomem *base_address = NULL;

	pr_info("phy_cfg:%d, %d, %d\n", phy0_sel, phy1_sel, phy2_sel);
	base_address = ioremap_nocache(0x40400238, 0x10);
	value = __raw_readl(base_address);

	/* bit[21:22]: phy0_sel; bit[23:24]: phy1_sel; bit[25:26]: phy2_sel */
	value &= ~(0x3 << 21);
	value |= ((0x3 & phy0_sel) << 21);

	value &= ~(0x3 << 23);
	value |= ((0x3 & phy1_sel) << 23);

	value &= ~(0x3 << 25);
	value |= ((0x3 & phy2_sel) << 25);

	/*  phy2_sel is 0 means we use phy2 */
	if (phy2_sel == 0) {
		value &= ~(0x0f << 17);
		value |= (0x2 << 17);
		value |= (0x1 << 19);
	} else {
		/* bit[17:18]: csi0_sel; bit[19:20]: csi1_sel */
		value &= ~(0x0f << 17);
		value |= (phy0_sel << 17);
		value |= (phy1_sel << 19);
	}
	pr_info("%s, phy_cfg_val: 0x%x\n", __func__, value);
#ifndef CONFIG_SPRD_CAM_PIP_VIV
	__raw_writel(value, base_address);
#endif
	iounmap(base_address);

	return SUCCESS;
}

uint8_t csi_api_set_on_lanes(uint8_t no_of_lanes, int sensor_id)
{
	return csi_set_on_lanes(no_of_lanes, sensor_id);
}

uint8_t csi_api_get_on_lanes(int sensor_id)
{
	return csi_get_on_lanes(sensor_id);
}

enum csi_lane_state_t csi_api_get_clk_state(int sensor_id)
{
	return csi_clk_state(sensor_id);
}

enum csi_lane_state_t csi_api_get_lane_state(uint8_t lane, int sensor_id)
{
	return csi_lane_module_state(lane, sensor_id);
}

int csi_api_mipi_phy_cfg(void)
{
	int ret = 0;

	ret = csi_phy_cfg(phy_info[0].phy_cfg, phy_info[1].phy_cfg,
			  phy_info[2].phy_cfg);
	return ret;
}

int csi_api_mipi_phy_cfg_init(struct device_node *phy_node, int sensor_id)
{
	unsigned int phy_id = 0;
	unsigned int phy_cfg = 0;

	of_property_read_u32(phy_node, "sprd,phyid", &phy_id);
	if (phy_id > 2) {
		pr_err("%s: parse phyid error : phyid:%u\n", __func__, phy_id);
		return -1;
	}
	of_property_read_u32(phy_node, "sprd,phycfg", &phy_cfg);

	phy_info[phy_id].sensor_id = sensor_id;
	phy_info[phy_id].phy_id = phy_id;
	phy_info[phy_id].phy_cfg = phy_cfg;
	pr_info("sensor_id %d mipi_phy:phy_id:%u, phy_cfg:%u\n", sensor_id,
		phy_id, phy_cfg);

	return phy_id;
}

int csi_api_open(int bps_per_lane, int phy_id, int lane_num, int sensor_id)
{
	int ret = 0;

	ret = csi_mipi_clk_enable(sensor_id);
	if (ret) {
		pr_err("csi: clk enable fail\n");
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
	pr_err("csi%d: api open err %d\n", sensor_id, ret);
	csi_api_close(phy_id, sensor_id);
	return ret;
}

uint8_t csi_api_close(uint32_t phy_id, int sensor_id)
{
	int ret = 0;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);

	csi_shut_down_phy(1, sensor_id);
	ret = csi_close(sensor_id);
	csi_mipi_clk_disable(sensor_id);
	csi_ch_disable(sensor_id);
	csi_phy_power_down(&dt_info->phy, dt_info->id, 1);
	pr_err("csi%d: api close ret%d\n", sensor_id, ret);

	return ret;
}

int csi_api_switch(int sensor_id)
{
	return 0;
}
