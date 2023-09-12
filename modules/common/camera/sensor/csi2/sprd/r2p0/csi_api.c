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
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/semaphore.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mfd/syscon.h>
#include <sprd_mm.h>
#include "sprd_sensor_core.h"
#include "csi_api.h"
#include "csi_driver.h"
#include <dt-bindings/soc/sprd,pike2-mask.h>
#include <dt-bindings/soc/sprd,pike2-regs.h>

#ifdef CONFIG_PHYS_ADDR_T_64BIT
#define PRIx64 "llx"
#else
#define PRIx64 "x"
#endif

#define PIKE				0x50696B65
#define TWO				0x32000000
#define CSI_IPG_CLK			0x60E00038

#define EFUSE_CSI_2P2L_DSI_TRIMBG(x)	(((x) & 0xf000) >> 12)
#define EFUSE_CSI_2P2L_RCTL(x)		(((x) & 0xf00) >> 8)
#define EFUSE_CSI_2L_RCTL(x)		(((x) & 0x3c000000) >> 26)
#define CSI_2P2L_EFUSE_BLOCK_ID		7
#define CSI_2L_EFUSE_BLOCK_ID		9

static int csi_pattern_enable = 0;
#define IPG_CLK_CFG_MSK			0x3
#define IPG_CLK_48M			0
#define IPG_CLK_96M			1
#define IPG_CLK_153M6			2
#define IPG_CLK_192M			3
#define CSI_CLK_SOURCE_MSK		BIT_16
#define CSI_CLK_SOURCE			0
#define CSI_CLK_SENSOR			1

static struct csi_dt_node_info *s_csi_dt_info_p[SPRD_SENSOR_ID_MAX];

static struct csi_dt_node_info *csi_get_dt_node_data(int sensor_id)
{
	return s_csi_dt_info_p[sensor_id];
}

void csi_api_set_mode_size(uint32_t width, uint32_t height)
{
	csi_set_mode_size(width, height);
}
EXPORT_SYMBOL(csi_api_set_mode_size);

/* csi src 0 soc, 1 sensor, and csi test pattern clk */
static int csi_ipg_set_clk(int sensor_id)
{
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	int ret = 0;

	if (!dt_info || !dt_info->csi_src_eb) {
		pr_err("CSI: mipi clk enable err\n");
		return -EINVAL;
	}
	if (csi_pattern_enable) {
		clk_disable_unprepare(dt_info->csi_src_eb);
	} else {
		ret = clk_prepare_enable(dt_info->csi_src_eb);
		if (ret)
			pr_err("CSI: mipi clk src eb err\n");
	}

	return ret;
}

static int csi_mipi_clk_enable(int sensor_id)
{
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	int ret = 0;

	if (!dt_info) {
		pr_err("Input dt_info ptr is Null\n");
		return -EINVAL;
	}

	if (!dt_info->mipi_csi_gate_eb || !dt_info->csi_eb_clk ||
		!dt_info->cam_clk_cphy_cfg_gate_eb) {
		pr_err("CSI: mipi clk enable err\n");
		return -EINVAL;
	}
#ifndef CONFIG_FPGA
	ret = clk_prepare_enable(dt_info->csi_eb_clk);
	if (ret) {
		pr_err("csi eb clk err\n");
		return ret;
	}

	ret = clk_prepare_enable(dt_info->cam_clk_cphy_cfg_gate_eb);
	if (ret) {
		pr_err("csi cphy eb clk err\n");
		return ret;
	}

	ret = clk_prepare_enable(dt_info->mipi_csi_gate_eb);
	if (ret) {
		pr_err("mipi csi mipi gate clk err\n");
		return ret;
	}
#endif
	ret = csi_ipg_set_clk(sensor_id);
	if (ret) {
		pr_err("CSI_PATTERN err\n");
		return ret;
	}

	return ret;
}

static void csi_mipi_clk_disable(int sensor_id)
{
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);

	if (!dt_info) {
		pr_err("Input dt_info ptr is Null\n");
		return;
	}

	if (!dt_info->mipi_csi_gate_eb || !dt_info->csi_eb_clk ||
		!dt_info->cam_clk_cphy_cfg_gate_eb) {
		pr_err("CSI: mipi clk disable err\n");
		return;
	}
#ifndef CONFIG_FPGA
	clk_disable_unprepare(dt_info->mipi_csi_gate_eb);
	clk_disable_unprepare(dt_info->csi_eb_clk);
	clk_disable_unprepare(dt_info->cam_clk_cphy_cfg_gate_eb);
	clk_disable_unprepare(dt_info->csi_src_eb);
#endif
}

int csi_api_mipi_phy_cfg_init(struct device_node *phy_node, int sensor_id)
{
	unsigned int phy_id = 0;

	if (of_property_read_u32(phy_node, "sprd,phyid", &phy_id)) {
		pr_err("Failed to get the sprd_phyid\n");
		return -1;
	}

	if (phy_id > 2) {
		pr_err("%s: parse phyid error : phyid:%u\n", __func__, phy_id);
		return -1;
	}

	pr_info("sensor_id %d mipi_phy:phy_id:%u\n", sensor_id, phy_id);

	return phy_id;
}

int csi_api_dt_node_init(struct device *dev, struct device_node *dn,
					int sensor_id, unsigned int phy_id)
{
	struct csi_dt_node_info *csi_info = NULL;
	struct resource res;
	void __iomem *reg_base = NULL;
	struct regmap *cam_ahb_syscon = NULL;
	struct regmap *aon_apb_syscon = NULL;

	csi_info = devm_kzalloc(dev, sizeof(*csi_info), GFP_KERNEL);
	if (!csi_info) {
		pr_err("Input csi info is NULL\n");
		return -EINVAL;
	}

	/* read address */
	if (of_address_to_resource(dn, 0, &res)) {
		pr_err("csi2_init: get address info error\n");
		return -EINVAL;
	}

	reg_base = devm_ioremap_nocache(dev, res.start, resource_size(&res));
	if (IS_ERR(reg_base)) {
		pr_err("csi_dt_init: get csi regbase error\n");
		return PTR_ERR(reg_base);
	}
	csi_info->reg_base = (unsigned long)reg_base;

	csi_info->phy.phy_id = phy_id;
	pr_info("csi node name %s\n", dn->name);

	if (of_property_read_u32_index(dn, "sprd,csi-id", 0,
		&csi_info->controller_id)) {
		pr_err("csi2_init: get csi-id error\n");
		return -EINVAL;
	}

	pr_info("csi %d ,phy addr:0x%"PRIx64" ,size:0x%"PRIx64" , reg %lx\n",
			csi_info->controller_id, res.start,
			resource_size(&res),
			csi_info->reg_base);

	/* read clocks */
	csi_info->mipi_csi_gate_eb = of_clk_get_by_name(dn,
					"clk_mipi_csi_gate_eb");

	csi_info->csi_eb_clk = of_clk_get_by_name(dn, "clk_csi_eb");
	/* csi src flag from sensor or csi */
	csi_info->cam_clk_cphy_cfg_gate_eb = of_clk_get_by_name(dn,
					"clk_cphy_cfg_gate_eb");

	csi_info->csi_src_eb = of_clk_get_by_name(dn, "mipi_csi_src_eb");

	cam_ahb_syscon = syscon_regmap_lookup_by_phandle(dn,
					"sprd,cam-ahb-syscon");
	if (IS_ERR(cam_ahb_syscon)) {
		pr_err("csi_dt_init: get cam-ahb-syscon error\n");
		return PTR_ERR(cam_ahb_syscon);
	}

	csi_info->phy.cam_ahb_syscon = cam_ahb_syscon;

	aon_apb_syscon = syscon_regmap_lookup_by_phandle(dn,
					"sprd,ana-apb-syscon");
	if (IS_ERR(aon_apb_syscon)) {
		pr_err("csi_dt_init: get aon-apb-syscon error\n");
		return PTR_ERR(aon_apb_syscon);
	}

	csi_info->phy.ana_apb_syscon = aon_apb_syscon;

	csi_phy_power_down(&csi_info->phy, sensor_id, 1);

	s_csi_dt_info_p[sensor_id] = csi_info;
	pr_info("csi dt info:sensor_id :%d, csi_info:0x%p\n",
				sensor_id, csi_info);

	return 0;
}

static int csi_init(unsigned int bps_per_lane,
				     unsigned int phy_id, int sensor_id)
{
	int ret = 0;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	/*struct regmap *csi_apb = NULL;*/
	struct regmap *aon_apb = NULL;
	unsigned int chip_id0 = 0, chip_id1 = 0;

	/*csi_apb = syscon_regmap_lookup_by_compatible(
	*					"sprd,anlg_phy_g5");
	*/
	aon_apb = dt_info->phy.ana_apb_syscon;

	ret = regmap_read(aon_apb, REG_AON_APB_AON_CHIP_ID0, &chip_id0);
	if (unlikely(ret))
		goto chipid_exit;
	ret = regmap_read(aon_apb, REG_AON_APB_AON_CHIP_ID1, &chip_id1);
	if (unlikely(ret))
		goto chipid_exit;

	pr_info("%s: sensor_id %d\n", __func__, sensor_id);
	if (!(chip_id0 == TWO && chip_id1 == PIKE))
		pr_info("chip error, need to check\n");

	csi_phy_power_down(&dt_info->phy, sensor_id, 0);
	csi_controller_enable(dt_info, sensor_id);
	dphy_init(&dt_info->phy, sensor_id);

	return ret;

chipid_exit:
	pr_err("Read chip id error\n");
	return ret;
}

static void csi_start(int sensor_id)
{
	csi_set_on_lanes(1, sensor_id);
	csi_shut_down_phy(0, sensor_id);
	csi_reset_phy(sensor_id);
	csi_reset_controller(sensor_id);
	csi_event_enable(sensor_id);
}

#if 0
static int csi_efuse_cfg(void)
{
	unsigned int csi_2p2l_block = 0, csi_2l_block = 0;
	unsigned int csi_2p2l_dbg_trimbg = 0;
	unsigned int csi_2p2l_rctl = 0;
	unsigned int csi_2l_rctl = 0;
	struct csi_dt_node_info *dt_info = NULL;
	struct regmap *aon_apb_syscon = NULL;

	dt_info = csi_get_dt_node_data(SPRD_SENSOR_MAIN_ID_E);
	if (dt_info == NULL) {
		dt_info = csi_get_dt_node_data(SPRD_SENSOR_SUB_ID_E);
		if (dt_info == NULL) {
			pr_err("csi_efuse: dt_info is NULL\n");
			return -EINVAL;
		}
	}

	aon_apb_syscon = dt_info->phy.ana_apb_syscon;
	if (IS_ERR(aon_apb_syscon)) {
		pr_err("csi_efuse_config: get aon-apb-syscon error\n");
		return PTR_ERR(aon_apb_syscon);
	}
	/*when Efuse is not default value(NULL),get csi adjust param.*/
	csi_2p2l_block = sprd_ap_efuse_read(CSI_2P2L_EFUSE_BLOCK_ID);
	if (csi_2p2l_block) {
		/*dsi debug csi*/
		csi_2p2l_dbg_trimbg = EFUSE_CSI_2P2L_DSI_TRIMBG(csi_2p2l_block);
		csi_2p2l_rctl = EFUSE_CSI_2P2L_RCTL(csi_2p2l_block);
		regmap_update_bits(aon_apb_syscon,
			REG_AON_APB_CSI_2P2L_DBG_PHY_CTRL,
			BIT_AON_APB_CSI_2P2L_DBG_TRIMBG(0xf),
			BIT_AON_APB_CSI_2P2L_DBG_TRIMBG(csi_2p2l_dbg_trimbg));
		regmap_update_bits(aon_apb_syscon,
			REG_AON_APB_CSI_2P2L_M_PHY_CTRL,
			BIT_AON_APB_CSI_2P2L_RCTL(0xf),
			BIT_AON_APB_CSI_2P2L_RCTL(csi_2p2l_rctl));
	}
	csi_2l_block = sprd_ap_efuse_read(CSI_2L_EFUSE_BLOCK_ID);
	if (csi_2l_block) {
		csi_2l_rctl = EFUSE_CSI_2P2L_RCTL(csi_2l_block);
		regmap_update_bits(aon_apb_syscon,
			REG_AON_APB_CSI_2L_PHY_CTRL,
			BIT_AON_APB_CSI_2L_RCTL(0xf),
			BIT_AON_APB_CSI_2L_RCTL(csi_2l_rctl));
	}

	return 0;
}
#endif

int csi_api_mipi_phy_cfg(void)
{
	int ret = 0;

	/* ret = csi_efuse_cfg(); */
	if (ret)
		pr_err("%s,csi api mipi phy error %d\n", __func__, ret);

	return ret;
}

int csi_api_open(int bps_per_lane, int phy_id, int lane_num, int sensor_id, int is_pattern,
	int is_cphy, uint64_t lane_seq)
{
	int ret = 0;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);

	ret = csi_ahb_reset(&dt_info->phy, dt_info->controller_id);
	if (unlikely(ret))
		goto EXIT;
	ret = csi_mipi_clk_enable(sensor_id);
	if (unlikely(ret)) {
		pr_err("csi: clk enable fail\n");
		csi_mipi_clk_disable(sensor_id);
		goto EXIT;
	}
	udelay(1);
	ret = csi_init(bps_per_lane, phy_id, sensor_id);
	if (unlikely(ret))
		goto EXIT;
	csi_start(sensor_id);
	csi_set_on_lanes(lane_num, sensor_id);
	csi_pattern_enable = is_pattern;
	if (csi_pattern_enable)
		csi_ipg_mode_cfg(sensor_id);

	return ret;
EXIT:
	pr_err("csi: api open err %d\n", ret);
	csi_api_close(phy_id, sensor_id);
	return ret;
}

int csi_api_close(uint32_t phy_id, int sensor_id)
{
	int ret = 0;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	/*struct regmap *csi_apb = NULL;*/
	struct regmap *aon_apb = NULL;
	unsigned int chip_id0 = 0, chip_id1 = 0;

	/*csi_apb = syscon_regmap_lookup_by_compatible(
	*					"sprd,anlg_phy_g5");
	*/
	aon_apb = dt_info->phy.ana_apb_syscon;

	ret = regmap_read(aon_apb, REG_AON_APB_AON_CHIP_ID0, &chip_id0);
	if (unlikely(ret))
		goto chipid_exit;
	ret = regmap_read(aon_apb, REG_AON_APB_AON_CHIP_ID1, &chip_id1);
	if (unlikely(ret))
		goto chipid_exit;

	if (chip_id0 == TWO && chip_id1 == PIKE)
		pr_info("csi close pike2\n");


	csi_shut_down_phy(1, sensor_id);
	csi_close(sensor_id);
	csi_phy_power_down(&dt_info->phy, sensor_id, 1);
	csi_mipi_clk_disable(sensor_id);
	pr_info("csi api close ret: %d\n", ret);

	return ret;

chipid_exit:
	pr_err("Read chip id error\n");
	return ret;
}

void csi_api_reg_trace(void)
{
	int ret = 0;
	unsigned int csi_en = 0, val = 0;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(0);
	struct csi_phy_info *phy = NULL;
	struct regmap *cam_ahb_syscon = NULL;

	if (!dt_info) {
		pr_err("Input dt_info ptr is Null\n");
		return;
	}

	phy = &dt_info->phy;
	if (!phy) {
		pr_err("Input phy ptr is Null\n");
		return;
	}

	cam_ahb_syscon = phy->cam_ahb_syscon;
	ret = regmap_read(cam_ahb_syscon, REG_MM_AHB_EB, &val);
	if (unlikely(ret)) {
		pr_err("Read mm ahb error\n");
		return;
	}

	csi_en = val & BIT_MM_AHB_CSI_EB;
	if (csi_en)
		csi_reg_trace(0);

}
EXPORT_SYMBOL(csi_api_reg_trace);

int csi_api_switch(int sensor_id)
{
	return 0;
}
