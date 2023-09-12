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

#include <dt-bindings/soc/sprd,qogirn6pro-regs.h>
#include <dt-bindings/soc/sprd,qogirn6pro-mask.h>
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
#include <sprd_mm.h>
#include "sprd_sensor_core.h"
#include "csi_api.h"
#include "csi_driver.h"
#include "sprd_sensor_drv.h"
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
/* csi src 0 soc, 1 sensor, and csi test pattern clk */

int cnt = 0;
static int csi_mipi_clk_enable(int sensor_id)
{
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	int ret = 0;
	void __iomem *reg_base = NULL;
	pr_debug("cnt 0x%x 0x%x\n", cnt, reg_rd(0x3000000c));

	if (!dt_info) {
		pr_err("fail to get valid dt_info ptr\n");
		return -EINVAL;
	}

	if (!dt_info->csi_eb_clk) {
		pr_err("fail to csi mipi clk enable\n");
		return -EINVAL;
	}
	if((BIT_15 | BIT_14 | BIT_12)!=(reg_rd(0x3000000c)&(BIT_15 | BIT_14 | BIT_12))){
		reg_base = ioremap(0x3000000c, 1);
		if (!reg_base) {
			pr_info("0x%x: ioremap failed\n", 0x3000000c);
			return -1;
		}
		REG_MWR(reg_base, BIT_15 | BIT_14 | BIT_12, BIT_15 | BIT_14 | BIT_12);
		iounmap(reg_base);
/*		reg_base = ioremap(0x3001004c, 9);
		if (!reg_base) {
			pr_info("0x%x: ioremap failed\n", 0x3001004c);
			return -1;
		}
		REG_MWR(reg_base, BIT_16, ~BIT_16);
		REG_MWR(reg_base + 4, BIT_16, ~BIT_16);
		REG_MWR(reg_base + 8, BIT_16, ~BIT_16);
		iounmap(reg_base)*/;
		//csi_api_reg_trace();
	}
	ret = clk_prepare_enable(dt_info->csi_eb_clk);
	if (ret) {
		pr_err("fail to csi eb clk\n");
		return ret;
	}
	reg_mwr(0x3000000c,BIT_2, BIT_2);//TODO


	regmap_update_bits(dt_info->phy.aon_apb_syscon,
		REG_AON_APB_CGM_CLK_TOP_REG1,//0x013c not used
		MASK_AON_APB_CGM_CPHY_CFG_EN,//bit15
		MASK_AON_APB_CGM_CPHY_CFG_EN);
	regmap_update_bits(dt_info->phy.aon_apb_syscon,
		0x0c, 0x04, 0x04);

	//ret = clk_prepare_enable(dt_info->csi_src_eb);

	if (ret){
		pr_err("fail to csi mipi clk src eb\n");
		return ret;
	}
	if(csi_pattern_enable){
		clk_prepare_enable(dt_info->csi_src_eb);//don't need enable in ipg mode
		//reg_mwr(0x3000000c,BIT_16, BIT_16);
    }else{
		pr_info("don't need enable ipg mode\n");
		//	reg_mwr(0x3000000c,BIT_16, ~BIT_16);
	//	reg_mwr(0x6491009c,0x1f, 0x1f);
	//	reg_mwr(0x6491009c,0x1f, 0x00);
		//reg_mwr(0x64804004, BIT_12, BIT_12);//0x0c bit12//0x30 bit12?? can't access
		reg_mwr(0x649100a0,0x1f, 0x1f);

		//reg_mwr(0x64900004, BIT_12, BIT_12);//
    }
	return ret;
}

static void csi_mipi_clk_disable(int sensor_id)
{
	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	void __iomem *reg_base = NULL;

	if (!dt_info) {
		pr_err("fail to get valid dt_info ptr\n");
		return;
	}


	if (!dt_info->csi_eb_clk) {
		pr_err("fail to csi mipi clk disable\n");
		return;
	}


	regmap_update_bits(dt_info->phy.aon_apb_syscon,
		REG_AON_APB_CGM_CLK_TOP_REG1,
		MASK_AON_APB_CGM_CPHY_CFG_EN,
		~MASK_AON_APB_CGM_CPHY_CFG_EN);
	regmap_update_bits(dt_info->phy.aon_apb_syscon,
		0x0c, 0x04, ~0x04);
	cnt--;
	pr_debug("cnt 0x%x 0x%x\n", cnt, reg_rd(0x3000000c));
	if(!cnt) {
		reg_base = ioremap(0x3000000c, 1);
		if (!reg_base) {
			pr_info("0x%x: ioremap failed\n", 0x3000000c);
		}
		REG_MWR(reg_base, BIT_15 | BIT_14 | BIT_12, 0);
		iounmap(reg_base);
	}

	//clk_disable_unprepare(dt_info->mipi_csi_gate_eb);
	clk_disable_unprepare(dt_info->csi_eb_clk);
	if(csi_pattern_enable)
		clk_disable_unprepare(dt_info->csi_src_eb);//don't need enable in ipg mode
    else
		pr_info("don't need disable ipg mode\n");

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

int csi_api_get_dcam_id(struct device_node *phy_node, int sensor_id, unsigned int phy_id)
{
	unsigned int dcam_id = 255;

	if (phy_id >= PHY_MAX) {
		pr_err("%s:fail to parse dcam_id : dcam_id:%u\n", __func__, dcam_id);
		return -1;
	}else if (phy_id == PHY_4LANE || phy_id == PHY_CPHY){
	        dcam_id = 0;
	}else if (phy_id == PHY_2P2 || phy_id == PHY_2P2_M){
	        dcam_id = 2;
	}else if (phy_id == PHY_2P2RO|| phy_id == PHY_2P2RO_M){
	        dcam_id = 4;
	}else if (phy_id == PHY_2P2_S){
	        dcam_id = 3;
	}else if (phy_id == PHY_2P2RO_S)
	        dcam_id = 5;

	pr_info("sensor_id %d dcam_id:%u\n", sensor_id, dcam_id);

	return dcam_id;
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

	reg_base = devm_ioremap(dev, res.start, resource_size(&res));
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
/*
	csi_info->mipi_csi_gate_eb = of_clk_get_by_name(dn,
					"clk_mipi_csi_gate_eb");
*/
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
					"sprd,anlg_phy_g4_controller");
	if (IS_ERR_OR_NULL(regmap_syscon)) {
		pr_err("csi_dt_init:fail to get anlg_phy_g4_controller\n");
		return PTR_ERR(regmap_syscon);
	}

	csi_info->phy.anlg_phy_g4_syscon = regmap_syscon;

	regmap_syscon = syscon_regmap_lookup_by_phandle(dn,
					"sprd,anlg_phy_g4l_controller");
	if (IS_ERR_OR_NULL(regmap_syscon)) {
		pr_err("csi_dt_init:fail to get anlg_phy_g4l_controller\n");
		return PTR_ERR(regmap_syscon);
	}

	csi_info->phy.anlg_phy_g4l_syscon = regmap_syscon;

	csi_reg_base_save(csi_info, csi_info->controller_id);

	s_csi_dt_info_p[sensor_id] = csi_info;
	pr_info("csi dt info:sensor_id :%d, csi_info:0x%p\n",
				sensor_id, csi_info);

	return 0;
}

void csi_start(int lane_num, int sensor_id)
{
	csi_set_on_lanes(lane_num, sensor_id);
	csi_event_enable(sensor_id);
}

void csi_start_s(int lane_num, int sensor_id)
{
	csi_set_on_lanes_s(lane_num, sensor_id);
	csi_event_enable(sensor_id);
}
void csi_start_m(int lane_num, int sensor_id)
{
	csi_set_on_lanes_m(lane_num, sensor_id);
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

int csi_api_open(int bps_per_lane, int phy_id, int lane_num, int sensor_id, int is_pattern,
	int is_cphy, uint64_t lane_seq)
{
	int ret = 0;

	struct csi_dt_node_info *dt_info = csi_get_dt_node_data(sensor_id);
	csi_pattern_enable = is_pattern;

	if (!dt_info) {
		pr_err("fail to get valid phy ptr\n");
		return -EINVAL;
	}
	if (is_cphy){
		pr_info("orign phy_id: %d\n", phy_id);
		if (sensor_id == 0)
			phy_id = PHY_CPHY;
		dt_info->phy.phy_id = phy_id;
	}
/*	phy_id = (phy_id == 2) ? 4 : phy_id;
	dt_info->phy.phy_id = phy_id;
*/


	pr_info("csi open phy_id: %d\n", phy_id);

	dt_info->lane_seq = lane_seq;
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
	dt_info->bps_per_lane = bps_per_lane;
	phy_csi_path_cfg(dt_info, sensor_id);
	csi_phy_power_down(dt_info, sensor_id, 0);
	csi_phy_testclr_init(&dt_info->phy);
	csi_controller_enable(dt_info);
//	csi_phy_testclr(dt_info->controller_id, &dt_info->phy);
	csi_phy_init(dt_info, dt_info->controller_id);
	if(dt_info->phy.phy_id == PHY_2P2_S || dt_info->phy.phy_id == PHY_2P2RO_S)
		csi_start_s(lane_num, dt_info->controller_id);
	else if(dt_info->phy.phy_id == PHY_2P2_M || dt_info->phy.phy_id == PHY_2P2RO_M)
		csi_start_m(lane_num, dt_info->controller_id);
	else
		csi_start(lane_num, dt_info->controller_id);

	if (csi_pattern_enable)
		csi_ipg_mode_cfg(dt_info->controller_id, 1);

	return ret;

EXIT:
	pr_err("fail to open csi api %d\n", ret);
	csi_api_close(dt_info->phy.phy_id, sensor_id);
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

	if (csi_pattern_enable)
		csi_ipg_mode_cfg(dt_info->controller_id, 0);
	csi_close(dt_info->controller_id);
	csi_controller_disable(dt_info, dt_info->controller_id);
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
EXPORT_SYMBOL(csi_api_reg_trace);

int csi_api_switch(int sensor_id)
{
	return 0;
}
