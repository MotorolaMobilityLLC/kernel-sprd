/*
 * Copyright (C) 2020 Uniso Communications Inc.
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

#ifndef _UFS_SPRD_H_
#define _UFS_SPRD_H_

struct syscon_ufs {
	struct regmap *regmap;
	u32 reg;
	u32 mask;
};

struct ufs_sprd_host {
	struct ufs_hba *hba;
	void __iomem *ufs_analog_reg;
	struct syscon_ufs aon_apb_ufs_en;
	struct syscon_ufs ap_apb_ufs_en;
	struct syscon_ufs ap_apb_ufs_rst;
	struct syscon_ufs ufs_refclk_on;
	struct syscon_ufs ahb_ufs_lp;
	struct syscon_ufs ahb_ufs_force_isol;
	struct syscon_ufs ahb_ufs_cb;
};

/* UFS analog registers */
#define MPHY_2T2R_APB_REG1 0x68
#define MPHY_2T2R_APB_RESETN (0x1 << 3)

#define FIFO_ENABLE_MASK (0x1 << 15)

/* UFS mphy registers */
#define MPHY_LANE0_FIFO 0xc08c
#define MPHY_LANE1_FIFO 0xc88c

#define FIFO_ENABLE_MASK (0x1 << 15)

#endif/* _UFS_SPRD_H_ */
