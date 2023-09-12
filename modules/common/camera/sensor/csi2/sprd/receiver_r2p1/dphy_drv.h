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

#ifndef _DPHY_DRIVER_H_
#define _DPHY_DRIVER_H_

struct dphy_info {
	struct regmap *anlg_phy;
	struct regmap *cam_ahb;
	u32 iso_sw_en;
	u32 iso_sw_en_msk;
	u32 ps_pd_s;
	u32 ps_pd_s_msk;
	u32 ps_pd_l;
	u32 ps_pd_l_msk;
	u32 csi_mode;
	u32 csi_mode_msk;
/*
 * 3 group: M, S, DB(dsi)
 * testclr_m,s,db(dsi)
 * IF_SEL_M,S,DB(dsi)
 */
	u32 cfg_0; /* 0x008c S,M,DB */
	u32 cfg_0_msk;
	u32 dsi_if_sel_db; /* 0x0078 */
	u32 dsi_if_sel_db_msk;
	u32 dsi_testclr_db; /* 0x007c */
	u32 dsi_testclr_db_msk;
	u32 csi_if_sel_s; /* 0x0044 */
	u32 csi_if_sel_s_msk;
	u32 csi_if_sel_m; /* 0x0034 */
	u32 csi_if_sel_m_msk;
	/* 0x00b4 */
	u32 testclr_m_sel;
	u32 testclr_m_sel_msk;
	u32 testclr_s_sel;
	u32 testclr_s_sel_msk;
	u32 testclr_m;
	u32 testclr_m_msk;
	u32 testclr_s;
	u32 testclr_s_msk;
	u32 testclr_s_en;
	u32 testclr_s_en_msk;

	unsigned int phy_id;
};

//#define REG_MM_AHB_MIPI_CSI_SEL_CTRL           0x0030
#define REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2LANE_MIPI_PHY_BIST_TEST               0x0014
#define REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_4L_BIST_TEST                 0x00b4
#define REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_REG_SEL_CFG_0                  0x008c
#define REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_DB               0x0078
#define REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_S                0x0044
#define REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_CTRL_M                0x0034
#define REG_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_2P2L_TEST_DB               0x007c

#define MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S_EN 0x80000
#define MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_DSI_IF_SEL_DB 0x20
#define MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_IF_SEL_S 0x4
#define MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_CSI_IF_SEL_M 0x4

#define MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_M 0x100000
#define MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S 0x20000

#define MASK_ANLG_PHY_G10_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_CSI_IF_SEL_S 0x2000
#define MASK_ANLG_PHY_G10_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_CSI_IF_SEL_M 0x1000000
#define MASK_ANLG_PHY_G10_DBG_SEL_ANALOG_MIPI_CSI_2P2LANE_DSI_IF_SEL_DB 0x20

#define MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_M_SEL 0x200000
#define MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_CSI_2P2L_TESTCLR_S_SEL 0x40000
#define MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2P2LANE_DSI_TESTCLR_DB 0x1

#define MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_FORCE_CSI_PHY_SHUTDOWNZ                               0x4000000
#define MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_4LANE_FORCE_CSI_S_PHY_SHUTDOWNZ                             0x1000000
#define MASK_ANLG_PHY_G10_ANALOG_MIPI_CSI_2LANE_FORCE_CSI_PHY_SHUTDOWNZ                               0x4000000

void csi_phy_power_down(unsigned int phyid, int csiId, int sensor_id, int is_eb);
int dphy_csi_path_cfg(struct csi_dt_node_info *dt_info);
void dphy_init_state(unsigned int phyid, int csi_id, int sensor_id);
int phy_parse_dt(int phyid, struct device *dev);
void dphy_2p2l_init(int phy_id, int csi_id, int sensor_id);
int dphy_csi_match(struct csi_dt_node_info *dt_info);
int reg_mwr(unsigned int reg, unsigned int msk, unsigned int value);
int reg_wr(unsigned int reg, unsigned int value);
int reg_rd(unsigned int reg);
int phy_read(int idx, int addr);
void phy_write(int idx, int addr, int data);
#endif

