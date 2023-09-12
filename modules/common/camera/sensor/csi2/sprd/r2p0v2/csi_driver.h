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

#ifndef _CSI_DRIVER_H_
#define _CSI_DRIVER_H_

#define PHY_TESTCLR                                     (0)
#define PHY_TESTCLK                                     (1)
#define PHY_TESTDIN                                     (0)
#define PHY_TESTDOUT                                    (8)
#define PHY_TESTEN                                      (16)
#define PHY_TESTDIN_W                                   (8)
#define PHY_TESTDOUT_W                                  (8)
#define CSI_PCLK_CFG_COUNTER                            (34)

/* ANALOG_MIPI_CSI_4LANE_MIPI_PHY_BIST_TEST 0x40420000*/
#define REG_ANALOG_CPHY_BIST_TEST                       (0x0074)
#define BIT_ANALOG_FORCE_PHY_CSI_SHUTDOWN               BIT(26)
#define BIT_ANALOG_FORCE_PHY_CSI_S_SHUTDOWN             BIT(24)
/* ANALOG_MIPI_CSI_2P2LANE_CTRL_CSI_2P2L */
#define REG_ANALOG_2P2LANE_CTRL_CSI_2P2L                (0x000C)
#define BIT_ANALOG_2P2LANE_MODE_SEL                     BIT(4)

/* phy test clr setting */
#define REG_ANALOG_CPHY_TEST_CTRL                       (0x0050)
#define BIT_ANALOG_CPHY_DBG_TEST_CLR                    BIT(0)

#define REG_ANALOG_2P2LANE_SEL_CFG_0                    (0x0058)
#define BIT_ANALOG_2P2LANE_DSI_IF_SEL_DB                BIT(2)
/* 0x0074 */
#define BIT_ANALOG_2P2L_TESTCLR_S                       BIT(17)
#define BIT_ANALOG_2P2L_TESTCLR_S_SEL                   BIT(18)
#define BIT_ANALOG_2P2L_TESTCLR_M                       BIT(20)
#define BIT_ANALOG_2P2L_TESTCLR_M_SEL                   BIT(21)

#define REG_AON_APB_APB_EB1                             (0x0004)
#define BIT_AON_APB_SERDES_DPHY_EB                      BIT(31)

#define REG_AON_APB_PWR_CTRL                            (0x0024)
#define BIT_AON_APB_CSI_ISO_SW_EN_2P2LANE               BIT(4)
#define BIT_AON_APB_CSI_ISO_SW_EN_4LANE                 BIT(5)
#define BIT_AON_APB_FORCE_CSI_2P2LANE_PHY_SHUTDOWNZ     BIT(1)
#define BIT_AON_APB_MIPI_CSI_4LANE_PS_PD_L              BIT(10)
#define BIT_AON_APB_MIPI_CSI_4LANE_PS_PD_S              BIT(11)
#define BIT_AON_APB_MIPI_CSI_2P2LANE_PS_PD_L            BIT(12)
#define BIT_AON_APB_MIPI_CSI_2P2LANE_PS_PD_S            BIT(13)
#define BIT_AON_APB_FORCE_4LANE_PHY_SHUTDOWNZ           BIT(20)

#define REG_AON_APB_2P2L_PHY_CTRL                       (0x0084)
#define BIT_AON_APB_4LANE_PHY_MODE                      BIT(0)

#define REG_MM_AHB_EB                                   (0x0000)
#define BIT_MM_AHB_CSI0_EB                              BIT(3)
#define BIT_MM_AHB_CSI1_EB                              BIT(4)

#define REG_MM_AHB_RST                                  (0x0004)
#define BIT_MM_AHB_CSI0_SOFT_RST                        BIT(11)
#define BIT_MM_AHB_CSI1_SOFT_RST                        BIT(12)

#define REG_MM_AHB_MIPI_CSI2_CTRL                       (0x000c)

enum csi_error_t {
	ERR_NOT_INIT = 0xFE,
	ERR_ALREADY_INIT = 0xFD,
	ERR_NOT_COMPATIBLE = 0xFC,
	ERR_UNDEFINED = 0xFB,
	ERR_OUT_OF_BOUND = 0xFA,
	SUCCESS = 0
};

enum csi_registers_t {
	VERSION = 0x00,
	N_LANES = 0x04,
	PHY_SHUTDOWNZ = 0x08,
	DPHY_RSTZ = 0x0C,
	CSI2_RESETN = 0x10,
	MODE_CFG = 0x14,
	PHY_STATE = 0x18,
	DATA_IDS_1 = 0x18,
	DATA_IDS_2 = 0x1c,
	ERR1 = 0x20,
	ERR2 = 0x24,
	MASK1 = 0x28,
	MASK2 = 0x2C,
	PHY_TST_CRTL0 = 0x48,
	PHY_TST_CRTL1 = 0x4c
};

enum csi_lane_state_t {
	CSI_LANE_OFF = 0,
	CSI_LANE_ON,
	CSI_LANE_ULTRA_LOW_POWER,
	CSI_LANE_STOP,
	CSI_LANE_HIGH_SPEED
};

enum csi_controller_t {
	CSI_RX0 = 0,
	CSI_RX1,
	CSI_RX_MAX,
};

enum csi_phy_t {
	PHY_2P2 = 0,
	PHY_4LANE,
	PHY_2P2_M,
	PHY_2P2_S,
	PHY_MAX,
};

void csi_phy_power_down(struct csi_phy_info *phy, unsigned int csi_id,
		int is_eb);
int csi_ahb_reset(struct csi_phy_info *phy, unsigned int csi_id);
void csi_enable(struct csi_phy_info *phy, unsigned int phy_id,
		unsigned int sensor_id);
uint8_t csi_init(unsigned long base_address, uint32_t version, int32_t idx);
void dphy_init(struct csi_phy_info *phy, uint32_t bps_per_lane,
		uint32_t phy_id, int32_t idx);
uint8_t csi_set_on_lanes(uint8_t lanes, int32_t idx);
uint8_t csi_get_on_lanes(int32_t idx);
uint8_t csi_shut_down_phy(uint8_t shutdown, int32_t idx);
void csi_dump_reg(void);
uint8_t csi_close(int32_t idx);
uint8_t csi_reset_controller(int32_t idx);
uint8_t csi_reset_phy(int32_t idx);
uint8_t csi_event_disable(uint32_t mask, uint8_t err_reg_no, int32_t idx);
#endif

