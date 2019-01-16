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
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include "csi_api.h"

#define PHY_TESTCLR 0
#define PHY_TESTCLK 1
#define PHY_TESTDIN 0
#define PHY_TESTDOUT 8
#define PHY_TESTEN 16
#define PHY_TESTDIN_W 8
#define PHY_TESTDOUT_W 8
#define CSI_PCLK_CFG_COUNTER 34
#define __INT_CSI__ (1)

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
	CSI2_RESETN = 0x08,
	INT_ST_MAIN = 0x0C,
	DATA_IDS_1 = 0x10,
	DATA_IDS_2 = 0x14,
	PHY_SHUTDOWNZ = 0x40,
	DPHY_RSTZ = 0x44,
	PHY_RX = 0x48,
	PHY_STOPSTATE = 0x4C,
	PHY_TST_CRTL0 = 0x50,
	PHY_TST_CRTL1 = 0x54,
	PHY2_TST_CRTL0 = 0x58,
	PHY2_TST_CRTL1 = 0x5C,
	IPI_MODE = 0x80,
	IPI_VCID = 0x84,
	IPI_DATA_TYPE = 0x88,
	IPI_MEM_FLUSH = 0x8C,
	IPI_HSA_TIME = 0x90,
	IPI_HBP_TIME = 0x94,
	IPI_HSD_TIME = 0x98,
	IPI_HLINE_TIME = 0x9C,
	IPI_VSA_LINES = 0xB0,
	IPI_VBP_LINES = 0xB4,
	IPI_VFP_LINES = 0xB8,
	IPI_VACTIVE_LINES = 0xBC,
	PHY_CAL = 0xCC,
	INT_ST_PHY_FATAL = 0xE0,
	INT_MSK_PHY_FATAL = 0xE4,
	INT_FORCE_PHY_FATAL = 0xE8,
	INT_ST_PKT_FATAL = 0xF0,
	INT_MSK_PKT_FATAL = 0xF4,
	INT_FORCE_PKT_FATAL = 0xF8,
	INT_ST_FRAME_FATAL = 0x100,
	INT_MSK_FRAME_FATAL = 0x104,
	INT_FORCE_FRAME_FATAL = 0x108,
	INT_ST_PHY = 0x110,
	INT_MSK_PHY = 0x114,
	INT_FORCE_PHY = 0x118,
	INT_ST_PKT = 0x120,
	INT_MSK_PKT = 0x124,
	INT_FORCE_PKT = 0x128,
	INT_ST_LINE = 0x130,
	INT_MSK_LINE = 0x134,
	INT_FORCE_LINE = 0x138,
	INT_ST_IPI = 0x140,
	INT_MSK_IPI = 0x144,
	INT_FORCE_IPI = 0x148,
};

#if __INT_CSI__
enum csi_int_src {
	INT_PHY_FATAL = (0x00000001<<0),
	INT_PKT_FATAL = (0x00000001<<1),
	INT_FRAME_FATAL = (0x00000001<<2),
	INT_PHY = (0x00000001<<16),
	INT_PKT = (0x00000001<<17),
	INT_LINE = (0x00000001<<18),
	INT_IPI = (0x00000001<<19),
	INT_END
};
irqreturn_t csi_api_event_handler(int irq, void *handle);
#endif

void csi_phy_power_down(struct csi_phy_info *phy, unsigned int csi_id,
			int is_eb);
void csi_phy_init(struct csi_phy_info *phy);
void csi_enable(struct csi_phy_info *phy, unsigned int phy_id);
int csi_init(unsigned long base_address, unsigned int version, int idx);
unsigned int csi_core_read(enum csi_registers_t address, int idx);
int csi_core_write(enum csi_registers_t address, unsigned int data, int idx);
void dphy_init(struct csi_phy_info *phy, unsigned int bps_per_lane,
	       unsigned int phy_id, int idx);
unsigned int csi_core_read_part(enum csi_registers_t address,
				unsigned int shift, unsigned int width,
				int idx);
int csi_set_on_lanes(unsigned int lanes, int idx);
int csi_shut_down_phy(unsigned int shutdown, int idx);
void csi_dump_reg(void);
int csi_close(int idx);
int csi_reset_controller(int idx);
int csi_reset_phy(int idx);
int csi_event_disable(unsigned int mask, unsigned int err_reg_no, int idx);
int csi_event_enable(unsigned int mask, unsigned int err_reg_no, int idx);
#endif
