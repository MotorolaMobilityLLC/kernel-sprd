/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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

#include <dt-bindings/soc/sprd,sharkl5pro-regs.h>
#include <dt-bindings/soc/sprd,sharkl5pro-mask.h>
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
#include <video/sprd_mm.h>
#include "sprd_sensor_core.h"
#include "csi_api.h"
#include "csi_driver.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "csi_api: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

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

#define CSI_PATTERN_ENABLE		1
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
/* csi src 0 soc, 1 sensor, and csi test pattern clk */
static int csi_ipg_set_clk(int sensor_id)
{
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	int ret = 0;

	if (!dt_info || !dt_info->csi_src_eb) {
		pr_err("fail to csi mipi clk enable\n");
		return -EINVAL;
	}
	if (CSI_PATTERN_ENABLE) {
		clk_disable_unprepare(dt_info->csi_src_eb);
	} else {
		ret = clk_prepare_enable(dt_info->csi_src_eb);
		if (ret)
			pr_err("fail to csi mipi clk src eb\n");
	}

	return ret;
}

static int csi_mipi_clk_enable(int sensor_id)
{
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	int ret = 0;

	if (!dt_info) {
		pr_err("fail to get valid dt_info ptr\n");
		return -EINVAL;
	}


	if (!dt_info->clk_ckg_eb || !dt_info->mipi_csi_gate_eb
		|| !dt_info->csi_eb_clk) {
		pr_err("fail to csi mipi clk enable\n");
		return -EINVAL;
	}

	ret = clk_prepare_enable(dt_info->clk_ckg_eb);
	if (ret) {
		pr_err("fail to csi eb clk\n");
		return ret;
	}

	ret = clk_prepare_enable(dt_info->csi_eb_clk);
	if (ret) {
		pr_err("fail to csi eb clk\n");
		return ret;
	}

	ret = clk_prepare_enable(dt_info->mipi_csi_gate_eb);
	if (ret) {
		pr_err("fail to mipi csi mipi gate clk\n");
		return ret;
	}


	ret = csi_ipg_set_clk(sensor_id);
	if (ret) {
		pr_err("fail to set ipg clk\n");
		return ret;
	}
	return ret;
}

static void csi_mipi_clk_disable(int sensor_id)
{
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);

	if (!dt_info) {
		pr_err("fail to get valid dt_info ptr\n");
		return;
	}


	if (!dt_info->mipi_csi_gate_eb || !dt_info->csi_eb_clk) {
		pr_err("fail to csi mipi clk disable\n");
		return;
	}
	clk_disable_unprepare(dt_info->mipi_csi_gate_eb);
	clk_disable_unprepare(dt_info->csi_eb_clk);
	clk_disable_unprepare(dt_info->csi_src_eb);
	clk_disable_unprepare(dt_info->clk_ckg_eb);

}

int csi_set_dt_node_data(void *param, int sensor_id)
{
	struct csi_dt_node_info *csi_dt_node_data = NULL;

	if (!param) {
		pr_err("fail to get the csi dt info\n");
		return -1;
	}

	csi_dt_node_data = (struct csi_dt_node_info *)param;
	s_csi_dt_info_p[sensor_id]->lane_switch_eb =
		csi_dt_node_data->lane_switch_eb;
	s_csi_dt_info_p[sensor_id]->lane_seq =
		csi_dt_node_data->lane_seq;

	return 0;
}

int csi_api_mipi_phy_cfg_init(struct device_node *phy_node, int sensor_id)
{
	unsigned int phy_id = 0;

	if (of_property_read_u32(phy_node, "sprd,phyid", &phy_id)) {
		pr_err("fail to get the sprd_phyid\n");
		return -1;
	}

	if (phy_id >= PHY_MAX) {
		pr_err("%s:fail to parse phyid : phyid:%u\n", __func__, phy_id);
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
	struct regmap *regmap_syscon = NULL;

	csi_info = devm_kzalloc(dev, sizeof(*csi_info), GFP_KERNEL);
	if (!csi_info)
		return -EINVAL;

	/* read address */
	if (of_address_to_resource(dn, 0, &res)) {
		pr_err("csi2_init:fail to get address info\n");
		return -EINVAL;
	}

	reg_base = devm_ioremap_nocache(dev, res.start, resource_size(&res));
	if (IS_ERR_OR_NULL(reg_base)) {
		pr_err("csi_dt_init:fail to get csi regbase\n");
		return PTR_ERR(reg_base);
	}
	csi_info->reg_base = (unsigned long)reg_base;

	csi_info->phy.phy_id = phy_id;
	pr_info("csi node name %s\n", dn->name);

	if (of_property_read_u32_index(dn, "sprd,csi-id", 0,
		&csi_info->controller_id)) {
		pr_err("csi2_init:fail to get csi-id\n");
		return -EINVAL;
	}

	pr_info("csi %d ,phy addr:0x%"PRIx64" ,size:0x%"PRIx64" , reg %lx\n",
			csi_info->controller_id, res.start,
			resource_size(&res),
			csi_info->reg_base);

	/* read clocks */
	csi_info->clk_ckg_eb = of_clk_get_by_name(dn,
					"clk_gate_eb");

	csi_info->mipi_csi_gate_eb = of_clk_get_by_name(dn,
					"clk_mipi_csi_gate_eb");

	csi_info->csi_eb_clk = of_clk_get_by_name(dn, "clk_csi_eb");

	/* csi src flag from sensor or csi */
	csi_info->csi_src_eb = of_clk_get_by_name(dn, "mipi_csi_src_eb");

	regmap_syscon = syscon_regmap_lookup_by_phandle(dn,
					"sprd,cam-ahb-syscon");
	if (IS_ERR_OR_NULL(regmap_syscon)) {
		pr_err("csi_dt_init:fail to get cam-ahb-syscon\n");
		return PTR_ERR(regmap_syscon);
	}
	csi_info->phy.cam_ahb_syscon = regmap_syscon;

	regmap_syscon = syscon_regmap_lookup_by_phandle(dn,
					"sprd,aon-apb-syscon");
	if (IS_ERR_OR_NULL(regmap_syscon)) {
		pr_err("csi_dt_init:fail to get aon-apb-syscon\n");
		return PTR_ERR(regmap_syscon);
	}
	csi_info->phy.aon_apb_syscon = regmap_syscon;

	regmap_syscon = syscon_regmap_lookup_by_phandle(dn,
					"sprd,anlg_phy_g1_controller");
	if (IS_ERR_OR_NULL(regmap_syscon)) {
		pr_err("csi_dt_init:fail to get anlg_phy_g1_controller\n");
		return PTR_ERR(regmap_syscon);
	}
	csi_info->phy.anlg_phy_g1_syscon = regmap_syscon;

	csi_reg_base_save(csi_info, sensor_id);
	csi_phy_power_down(csi_info, sensor_id, 1);

	s_csi_dt_info_p[sensor_id] = csi_info;
	pr_info("csi dt info:sensor_id :%d, csi_info:0x%p\n",
				sensor_id, csi_info);

	return 0;
}

static int csi_init(unsigned int bps_per_lane, unsigned int phy_id,
	int sensor_id)
{
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);

	csi_phy_power_down(dt_info, sensor_id, 0);
	csi_controller_enable(dt_info, sensor_id);
	dphy_init(dt_info, sensor_id);

	return 0;
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
	if (IS_ERR_OR_NULL(aon_apb_syscon)) {
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
		pr_err("%s,fail to cfg csi api mipi phy %d\n", __func__, ret);

	return ret;
}

static void csi_api_reg_owr(unsigned int addr, unsigned int val)
{
	void __iomem *io_tmp = NULL;
	u32 tmp;

	io_tmp = ioremap_nocache(addr, 0x4);
	tmp = __raw_readl(io_tmp);
	__raw_writel(tmp|val, io_tmp);
	mb(); /* asm/barrier.h */
	val = __raw_readl(io_tmp);
	iounmap(io_tmp);
}

static void csi_api_reg_awr(unsigned int addr, unsigned int val)
{
	void __iomem *io_tmp = NULL;
	unsigned int tmp;

	io_tmp = ioremap_nocache(addr, 0x4);
	tmp = __raw_readl(io_tmp);
	__raw_writel(tmp&val, io_tmp);
	mb(); /* asm/barrier.h */
	val = __raw_readl(io_tmp);
	iounmap(io_tmp);
}

int csi_api_open(int bps_per_lane, int phy_id, int lane_num, int sensor_id)
{
	int ret = 0;
#ifdef FPAG_BRINGUP
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);

	if (!dt_info) {
		pr_err("fail to get valid phy ptr\n");
		return -EINVAL;
	}
	ret = csi_ahb_reset(&dt_info->phy, dt_info->controller_id);
	if (unlikely(ret))
		goto EXIT;
	ret = csi_mipi_clk_enable(sensor_id);
	if (unlikely(ret)) {
		pr_err("fail to csi mipi clk enable\n");
		csi_mipi_clk_disable(sensor_id);
		goto EXIT;
	}
	udelay(1);
#endif
	csi_api_reg_owr(0x62200004, BIT_9);
	udelay(10);
	csi_api_reg_awr(0x62200004, ~BIT_9);
	udelay(10);
	bps_per_lane = 56;
	phy_id = 0;
	/* sensor_id = 0; */
	lane_num = 2;
	ret = csi_init(bps_per_lane, phy_id, sensor_id);
	if (unlikely(ret))
		goto EXIT;
	csi_api_close(0, 0);
	csi_start(sensor_id);
	csi_set_on_lanes(lane_num, sensor_id);
	if (CSI_PATTERN_ENABLE)
		csi_ipg_mode_cfg(sensor_id, 1);

	return ret;
EXIT:
	pr_err("fail to open csi api %d\n", ret);
	csi_api_close(phy_id, sensor_id);
	return ret;
}

int csi_api_close(uint32_t phy_id, int sensor_id)
{
	int ret = 0;
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);

	if (!dt_info) {
		pr_err("fail to get valid phy ptr\n");
		return -EINVAL;
	}

	if (CSI_PATTERN_ENABLE)
		csi_ipg_mode_cfg(sensor_id, 0);
	csi_close(sensor_id);
	csi_controller_disable(dt_info, sensor_id);
	csi_phy_power_down(dt_info, sensor_id, 1);
	csi_mipi_clk_disable(sensor_id);
	pr_info("csi api close ret: %d\n", ret);

	return ret;
}

void csi_api_reg_trace(void)
{
	int i = 0;

	for (i = 0; i < CSI_MAX_COUNT; i++)
		csi_reg_trace(i);
}

int csi_api_switch(int sensor_id)
{
	return 0;
}
