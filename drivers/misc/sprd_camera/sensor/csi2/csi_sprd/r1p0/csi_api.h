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

#ifndef _CSI_API_H_
#define _CSI_API_H_

#include <linux/mfd/syscon.h>
#include <linux/mfd/syscon/sprd-glb.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>

typedef void (*csi2_handler_t) (void *);
typedef void *(*csi2_thread_t) (void *);
typedef int (*csi2_isr_func) (unsigned int err_id, unsigned int err_status,
			      void *u_data);

enum csi_event_t {
	ERR_PHY_TX_START = 0,
	ERR_FRAME_BOUNDARY_MATCH = 4,
	ERR_FRAME_SEQUENCE = 8,
	ERR_CRC_DURING_FRAME = 12,
	ERR_LINE_CRC = 16,
	ERR_DOUBLE_ECC = 20,
	ERR_PHY_ESCAPE_ENTRY = 21,
	ERR_ECC_SINGLE = 25,
	ERR_UNSUPPORTED_DATA_TYPE = 29,
	MAX_EVENT = 49
};

enum csi_line_event_t {
	ERR_LINE_BOUNDARY_MATCH = 33,
	ERR_LINE_SEQUENCE = 41
};

enum csi_api_error_t {
	ERROR_VC_LANE_OUT_OF_BOUND = 0xCA,
	ERROR_EVENT_TYPE_INVALID = 0xC9,
	ERROR_DATA_TYPE_INVALID = 0xC8,
	ERROR_SLOTS_FULL = 0xC7,
	ERROR_ALREADY_REGISTERED = 0xC6,
	ERROR_NOT_REGISTERED = 0xC5
};

enum csi_data_type_t {
	NULL_PACKET = 0x10,
	BLANKING_DATA = 0x11,
	EMBEDDED_8BIT_NON_IMAGE_DATA = 0x12,
	YUV420_8BIT = 0x18,
	YUV420_10BIT = 0x19,
	LEGACY_YUV420_8BIT = 0x1A,
	YUV420_8BIT_CHROMA_SHIFTED = 0x1C,
	YUV420_10BIT_CHROMA_SHIFTED = 0x1D,
	YUV422_8BIT = 0x1E,
	YUV422_10BIT = 0x1F,
	RGB444 = 0x20,
	RGB555 = 0x21,
	RGB565 = 0x22,
	RGB666 = 0x23,
	RGB888 = 0x24,
	RAW6 = 0x28,
	RAW7 = 0x29,
	RAW8 = 0x2A,
	RAW10 = 0x2B,
	RAW12 = 0x2C,
	RAW14 = 0x2D,
	USER_DEFINED_8BIT_DATA_TYPE_1 = 0x30,
	USER_DEFINED_8BIT_DATA_TYPE_2 = 0x31,
	USER_DEFINED_8BIT_DATA_TYPE_3 = 0x32,
	USER_DEFINED_8BIT_DATA_TYPE_4 = 0x33,
	USER_DEFINED_8BIT_DATA_TYPE_5 = 0x34,
	USER_DEFINED_8BIT_DATA_TYPE_6 = 0x35,
	USER_DEFINED_8BIT_DATA_TYPE_7 = 0x36,
	USER_DEFINED_8BIT_DATA_TYPE_8 = 0x37
};

struct mipi_phy_info {
	unsigned int phy_id;
	unsigned int phy_cfg;
	unsigned int sensor_id;
};

struct csi_phy_info {
	struct regmap *cam_ahb_syscon;
	struct regmap *ana_apb_syscon;
	unsigned int phy_id;
};

struct csi_dt_node_info {
	unsigned int id;
	unsigned long reg_base;
	struct clk *cphy_gate_clk;
	struct clk *mipi_gate_clk;
	struct clk *csi_eb_clk;
	struct clk *mipi_csi_eb_clk;
	struct csi_phy_info phy;
	unsigned int ip_version;
	unsigned int csi_irq0;
	unsigned int csi_irq1;
};

int csi_api_mipi_phy_cfg(void);
int csi_api_dt_node_init(struct device *dev, struct device_node *dn,
			 int sensor_id, unsigned int phy_id);
int csi_api_mipi_phy_cfg_init(struct device_node *phy_node, int sensor_id);
int csi_api_open(int bps_per_lane, int phy_id, int lane_num, int sensor_id);
uint8_t csi_api_close(uint32_t phy_id, int sensor_id);
enum csi_lane_state_t csi_api_get_clk_state(int sensor_id);
uint8_t csi_api_get_on_lanes(int sensor_id);
uint8_t csi_api_set_on_lanes(uint8_t no_of_lanes, int sensor_id);
enum csi_lane_state_t csi_api_get_lane_state(uint8_t lane, int sensor_id);
int csi_api_switch(int sensor_id);
#endif
