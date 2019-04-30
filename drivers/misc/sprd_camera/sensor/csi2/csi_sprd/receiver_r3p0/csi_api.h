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

#ifndef _CSI_API_H_
#define _CSI_API_H_

#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>

struct csi_phy_info {
	struct regmap *cam_ahb_syscon;
	struct regmap *aon_apb_syscon;
	struct regmap *anlg_phy_g1_syscon; /* anlg_phy control,0x4035 */
	unsigned int phy_id;
};

struct csi_dt_node_info {
	unsigned int controller_id;
	unsigned long reg_base;
	struct clk *clk_ckg_eb;
	struct clk *mipi_csi_gate_eb;
	struct clk *csi_eb_clk;
	struct clk *csi_src_eb;
	struct csi_phy_info phy;
	bool lane_switch_eb;
	uint64_t lane_seq;
};

int csi_set_dt_node_data(void *param, int sensor_id);
int csi_api_mipi_phy_cfg(void);
int csi_api_mipi_phy_cfg_init(struct device_node *phy_node, int sensor_id);
int csi_api_dt_node_init(struct device *dev, struct device_node *dn,
				int sensor_id, unsigned int phy_id);
int csi_api_open(int bps_per_lane, int phy_id, int lane_num, int sensor_id);
int csi_api_close(uint32_t phy_id, int sensor_id);
void csi_api_reg_trace(void);
int csi_api_switch(int sensor_id);
#endif

