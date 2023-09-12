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

#ifndef _CSI_API_H_
#define _CSI_API_H_

struct csi_phy_info {
	struct regmap *cam_ahb_syscon; /* 0x60d0 */
	struct regmap *ana_apb_syscon; /* 0x402e */
	struct regmap *anlg_phy_g7_syscon; /* anlg_phy control,0x4042 */
	unsigned int phy_id;
};

struct csi_dt_node_info {
	unsigned int id;
	unsigned long reg_base;
	struct clk *cphy_gate_clk;
	struct clk *mipi_gate_clk;
	struct clk *csi_eb_clk;
	struct clk *mipi_csi_eb_clk;
	struct clk *csi_from_clk;
	struct csi_phy_info phy;
	unsigned int ip_version;
	unsigned int csi_irq0;
	unsigned int csi_irq1;
};

int csi_api_mipi_phy_cfg(void);
int csi_api_mipi_phy_cfg_init(struct device_node *phy_node, int sensor_id);
int csi_api_dt_node_init(struct device *dev, struct device_node *dn,
		int sensor_id, unsigned int phy_id);
int csi_api_open(int bps_per_lane, int phy_id, int lane_num, int sensor_id, int is_pattern,
	int is_cphy, uint64_t lane_seq);
uint8_t csi_api_close(uint32_t phy_id, int sensor_id);
void csi_api_reg_trace(void);
int csi_api_switch(int sensor_id);
#endif

