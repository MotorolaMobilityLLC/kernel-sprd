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
#include <linux/sched.h>

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon/sprd-glb.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include "csi_api.h"
#include "csi_driver.h"

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "[csi api]: %d: %s line=%d: " fmt, \
	current->pid, __func__, __LINE__

static struct csi_dt_node_info *s_csi_dt_info_p[4];

static struct csi_dt_node_info *csi_get_dt_node_data(int sensor_id)
{
	return s_csi_dt_info_p[sensor_id];
}

static int csi_ch_enable(unsigned int sensor_id)
{
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	struct csi_phy_info *phy = &dt_info->phy;
	unsigned int csi_id = dt_info->id;
	unsigned int val;

	pr_info("%s sensor id %d => phy id %d => csi id %d\n",
		__func__, sensor_id, phy->phy_id, dt_info->id);

	/* bit[1:2]: csi0_sel; bit[3:4]: csi1_sel */
	if (csi_id == 0) {
		regmap_update_bits(phy->aon_apb_gpr,
				   REG_AON_APB_ANA_PHY_CTRL0,
				   BIT_AON_APB_R2G_CSI0_SEL(phy->phy_id),
				   BIT_AON_APB_R2G_CSI0_SEL(phy->phy_id));

		/* make sure one csi sel value is non-zero */
		regmap_read(phy->aon_apb_gpr,
			    REG_AON_APB_ANA_PHY_CTRL0, &val);
		if ((val & 0x1e) == 0)
			regmap_update_bits(phy->aon_apb_gpr,
					   REG_AON_APB_ANA_PHY_CTRL0,
					   BIT_AON_APB_R2G_CSI1_SEL(1),
					   BIT_AON_APB_R2G_CSI1_SEL(1));
	} else {
		regmap_update_bits(phy->aon_apb_gpr,
				   REG_AON_APB_ANA_PHY_CTRL0,
				   BIT_AON_APB_R2G_CSI1_SEL(phy->phy_id),
				   BIT_AON_APB_R2G_CSI1_SEL(phy->phy_id));

		/* make sure one csi sel value is non-zero */
		regmap_read(phy->aon_apb_gpr,
			    REG_AON_APB_ANA_PHY_CTRL0, &val);
		if ((val & 0x1e) == 0)
			regmap_update_bits(phy->aon_apb_gpr,
					   REG_AON_APB_ANA_PHY_CTRL0,
					   BIT_AON_APB_R2G_CSI0_SEL(1),
					   BIT_AON_APB_R2G_CSI0_SEL(1));
	}

	return SUCCESS;
}

static int csi_ch_disable(unsigned int sensor_id)
{
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	struct csi_phy_info *phy = &dt_info->phy;
	unsigned int csi_id = dt_info->id;

	pr_info("%s sensor id %d => phy id %d => csi id %d\n",
		__func__, sensor_id, phy->phy_id, dt_info->id);

	/* bit[1:2]: csi0_sel; bit[3:4]: csi1_sel */
	if (csi_id == 0) {
		regmap_update_bits(phy->aon_apb_gpr,
				   REG_AON_APB_ANA_PHY_CTRL0,
				   BIT_AON_APB_R2G_CSI0_SEL(phy->phy_id),
				   ~(unsigned int)
				   BIT_AON_APB_R2G_CSI0_SEL(phy->phy_id));
	} else {
		regmap_update_bits(phy->aon_apb_gpr,
				   REG_AON_APB_ANA_PHY_CTRL0,
				   BIT_AON_APB_R2G_CSI1_SEL(phy->phy_id),
				   ~(unsigned int)
				   BIT_AON_APB_R2G_CSI1_SEL(phy->phy_id));
	}

	return SUCCESS;
}

static int csi_mipi_clk_enable(int sensor_id)
{
	int ret = 0;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);

	if (!dt_info || !dt_info->mipi_gate_clk ||
	    !dt_info->cphy_gate_clk || !dt_info->csi_eb_clk) {
		pr_err("%s invalid parm\n", __func__);
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

	if (!dt_info || !dt_info->mipi_gate_clk ||
	    !dt_info->cphy_gate_clk || !dt_info->csi_eb_clk) {
		pr_err("%s invalid parm\n", __func__);
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
	struct regmap *syscon_gpr;

	csi_info = devm_kzalloc(dev, sizeof(*csi_info), GFP_KERNEL);
	if (!csi_info)
		return -ENOMEM;

	/* get reg base address */
	of_address_to_resource(dn, 0, &res);
	reg_base = devm_ioremap_nocache(dev, res.start, resource_size(&res));
	if (IS_ERR(reg_base))
		return PTR_ERR(reg_base);
	csi_info->reg_base = (unsigned long)reg_base;

	csi_info->phy.phy_id = phy_id;
	pr_info("csi node name %s\n", dn->name);
	if (strncasecmp(dn->name, "csi0", 4) == 0)
		csi_info->id = 0;
	else if (strncasecmp(dn->name, "csi1", 4) == 0)
		csi_info->id = 1;
	else
		pr_info("warning: not support currently\n");
	pr_info("csi %d reg base %lx\n", csi_info->id, csi_info->reg_base);

	/* get clk */
	csi_info->cphy_gate_clk = of_clk_get_by_name(dn, "clk_cphy_gate");
	csi_info->mipi_gate_clk = of_clk_get_by_name(dn, "clk_mipi_gate");
	csi_info->csi_eb_clk = of_clk_get_by_name(dn, "clk_csi_eb");
	csi_info->mipi_csi_eb_clk = of_clk_get_by_name(dn, "clk_mipi_csi_eb");

	/* read version */
	if (of_property_read_u32_index(dn, "sprd,ip-version", 0,
				       &csi_info->ip_version)) {
		pr_err("failed to get csi ip version\n");
		devm_kfree(dev, csi_info);
		return -EINVAL;
	}

	syscon_gpr = syscon_regmap_lookup_by_compatible("sprd,iwhale2-lpc-ahb");
	if (IS_ERR(syscon_gpr))
		return PTR_ERR(syscon_gpr);
	csi_info->phy.cam_ahb_gpr = syscon_gpr;

	syscon_gpr = syscon_regmap_lookup_by_compatible("sprd,iwhale2-aon-apb");
	if (IS_ERR(syscon_gpr))
		return PTR_ERR(syscon_gpr);
	csi_info->phy.aon_apb_gpr = syscon_gpr;

	syscon_gpr = syscon_regmap_lookup_by_compatible("sprd,anlg_phy_g8");
	if (IS_ERR(syscon_gpr))
		return PTR_ERR(syscon_gpr);
	csi_info->phy.anlg_phy_g8_gpr = syscon_gpr;

	/* workaround for csi phy leakage */
	if (csi_info->id == 0)
		csi_phy_init(&csi_info->phy);

	csi_phy_power_down(&csi_info->phy, phy_id, 1);

	/* workaround for power down csi2, for 2 sensors phone */
	if (csi_info->id == 0)
		csi_phy_power_down(&csi_info->phy, 2, 1);

	s_csi_dt_info_p[sensor_id] = csi_info;

	return 0;
}

int csi_api_mipi_phy_cfg_init(struct device_node *phy_node, int sensor_id)
{
	unsigned int phy_id = 0;

	of_property_read_u32(phy_node, "sprd,phyid", &phy_id);
	if (phy_id > 2) {
		pr_err("%s: get phy id error:%d\n", __func__, phy_id);
		return -1;
	}
	pr_info("sensor id %d phy id %d\n", sensor_id, phy_id);

	return phy_id;
}

int csi_api_mipi_phy_cfg(void)
{
	return 0;
}

static int csi_api_init(unsigned int bps_per_lane, int sensor_id)
{
	int ret = SUCCESS;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	unsigned int csi_id = dt_info->id;

	do {
		csi_ch_enable(sensor_id);
		csi_phy_power_down(&dt_info->phy, dt_info->phy.phy_id, 0);
		pr_info("%s sensor id %d\n", __func__, sensor_id);
		ret = csi_init(dt_info->reg_base, dt_info->ip_version,
			       csi_id);
		if (ret != SUCCESS) {
			pr_err("%s unable to initialise driver", __func__);
			break;
		}
		dphy_init(bps_per_lane, csi_id);
	} while (0);

	return ret;
}

static int csi_api_start(int sensor_id)
{
	int ret = SUCCESS;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	unsigned int csi_id = dt_info->id;

	pr_debug("start csi\n");

	do {
		/* set only one lane (lane 0) as active (on) */
		ret = csi_set_on_lanes(1, csi_id);
		if (ret != SUCCESS) {
			pr_err("%s unable to set lanes\n", __func__);
			break;
		}
		ret = csi_shut_down_phy(0, csi_id);
		if (ret != SUCCESS) {
			pr_err("%s unable to bring up PHY\n", __func__);
			break;
		}
		ret = csi_reset_phy(csi_id);
		if (ret != SUCCESS) {
			pr_err("%s unable to reset PHY\n", __func__);
			break;
		}
		ret = csi_reset_controller(csi_id);
		if (ret != SUCCESS) {
			pr_err("%s unable to reset controller\n", __func__);
			break;
		}

		/* mask all interrupts */
		csi_event_disable(0xffffffff, 1, csi_id);
		csi_event_disable(0xffffffff, 2, csi_id);
		csi_event_disable(0xffffffff, 3, csi_id);
		csi_event_disable(0xffffffff, 4, csi_id);
		csi_event_disable(0xffffffff, 5, csi_id);
		csi_event_disable(0xffffffff, 6, csi_id);
		csi_event_disable(0xffffffff, 7, csi_id);
	} while (0);

	return ret;
}

int csi_api_open(int bps_per_lane, int phy_id, int lane_num, int sensor_id)
{
	int ret = 0;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	unsigned int csi_id = dt_info->id;

	ret = csi_mipi_clk_enable(sensor_id);
	if (ret) {
		pr_err("csi: failed enable mipi clk\n");
		csi_mipi_clk_disable(sensor_id);
		goto exit;
	}

	udelay(1);
	ret = csi_api_init(bps_per_lane, sensor_id);
	if (ret)
		goto exit;

	ret = csi_api_start(sensor_id);
	if (ret)
		goto exit;

	ret = csi_set_on_lanes(lane_num, csi_id);
	if (ret)
		goto exit;

	return ret;

exit:
	pr_err("csi: failed open csi %d\n", ret);
	csi_api_close(phy_id, sensor_id);
	return ret;
}

int csi_api_close(unsigned int phy_id, int sensor_id)
{
	int ret = 0;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	unsigned int csi_id = dt_info->id;

	csi_shut_down_phy(1, csi_id);
	ret = csi_close(csi_id);
	csi_mipi_clk_disable(sensor_id);
	csi_ch_disable(sensor_id);
	csi_phy_power_down(&dt_info->phy, dt_info->phy.phy_id, 1);
	pr_info("close csi, ret %d\n", ret);

	return ret;
}

int csi_api_switch(int sensor_id)
{
	return 0;
}
