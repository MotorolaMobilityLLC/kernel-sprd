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

#ifndef _CSI_DRIVER_H_
#define _CSI_DRIVER_H_

#include <linux/of.h>
#include <linux/spinlock.h>
#include "csi_api.h"

#define PHY_TESTCLR 0
#define PHY_TESTCLK 1
#define PHY_TESTDIN 0
#define PHY_TESTDOUT 8
#define PHY_TESTEN 16
#define PHY_TESTDIN_W 8
#define PHY_TESTDOUT_W 8
#define CSI_PCLK_CFG_COUNTER 34

enum csi_error_t {
	ERR_NOT_INIT = 0xFE,
	ERR_ALREADY_INIT = 0xFD,
	ERR_NOT_COMPATIBLE = 0xFC,
	ERR_UNDEFINED = 0xFB,
	ERR_OUT_OF_BOUND = 0xFA,
	SUCCESS = 0
};
/* csi_register_t just for whale2 */
enum csi_registers_t {
	VERSION = 0x00,
	N_LANES = 0x04,
	PHY_SHUTDOWNZ = 0x08,
	DPHY_RSTZ = 0x0C,
	CSI2_RESETN = 0x10,
	PHY_STATE = 0x18,
	DATA_IDS_1 = 0x80,
	DATA_IDS_2 = 0x84,
	ERR1 = 0x20,
	ERR2 = 0x24,
	MASK1 = 0x28,
	MASK2 = 0x2C,
	PHY_TST_CRTL0 = 0x40,
	PHY_TST_CRTL1 = 0x44
};
enum csi_lane_state_t {
	CSI_LANE_OFF = 0,
	CSI_LANE_ON,
	CSI_LANE_ULTRA_LOW_POWER,
	CSI_LANE_STOP,
	CSI_LANE_HIGH_SPEED
};
struct csi_pclk_cfg {
	uint32_t pclk_start;
	uint32_t pclk_end;
	uint8_t hsfreqrange;
	uint8_t hsrxthssettle;
};

void csi_phy_power_down(struct csi_phy_info *phy, unsigned int csi_id,
			int is_eb);
void csi_enable(struct csi_phy_info *phy, unsigned int phy_id);
uint8_t csi_init(unsigned long base_address, uint32_t version, int32_t idx);
uint32_t csi_core_read(enum csi_registers_t address, int32_t idx);
uint8_t csi_core_write(enum csi_registers_t address, uint32_t data,
		       int32_t idx);
void dphy_init(struct csi_phy_info *phy, uint32_t bps_per_lane, uint32_t phy_id,
	       int32_t idx);
uint32_t csi_core_read_part(enum csi_registers_t address, uint8_t shift,
			    uint8_t width, int32_t idx);
uint8_t csi_set_on_lanes(uint8_t lanes, int32_t idx);
uint8_t csi_shut_down_phy(uint8_t shutdown, int32_t idx);
void csi_dump_reg(void);
uint8_t csi_close(int32_t idx);
uint8_t csi_reset_controller(int32_t idx);
uint8_t csi_reset_phy(int32_t idx);
uint32_t csi_event_get_source(uint8_t err_reg_no, int32_t idx);
uint8_t csi_event_disable(uint32_t mask, uint8_t err_reg_no, int32_t idx);
enum csi_lane_state_t csi_clk_state(int32_t idx);
uint8_t csi_get_on_lanes(int32_t idx);
enum csi_lane_state_t csi_lane_module_state(uint8_t lane, int32_t idx);
uint8_t csi_event_enable(uint32_t mask, uint32_t err_reg_no, int32_t idx);
uint8_t csi_get_registered_line_event(uint8_t offset, int32_t idx);
uint8_t csi_register_line_event(u8 virtual_channel_no,
				enum csi_data_type_t data_type, u8 offset,
				int32_t idx);
uint8_t csi_unregister_line_event(uint8_t offset, int32_t idx);
uint8_t csi_payload_bypass(uint8_t on, int32_t idx);
#endif
