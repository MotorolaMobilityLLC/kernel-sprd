/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, Unisoc. All rights reserved.
 */

#ifndef _UFS_SPRD_QOGIRN6PRO_H_
#define _UFS_SPRD_QOGIRN6PRO_H_

struct ufs_sprd_ums9620_data {
	struct regulator *vdd_mphy;
	struct syscon_ufs ap_ahb_ufs_rst;
	struct syscon_ufs aon_apb_ufs_rst;
	struct syscon_ufs phy_sram_ext_ld_done;
	struct syscon_ufs phy_sram_bypass;
	struct syscon_ufs phy_sram_init_done;
	struct syscon_ufs aon_apb_ufs_clk_en;
	struct syscon_ufs ufsdev_refclk_en;
	struct syscon_ufs usb31pllv_ref2mphy_en;

	struct clk *hclk_source;
	struct clk *hclk;
	uint32_t ufs_lane_calib_data0;
	uint32_t ufs_lane_calib_data1;
};

/*
 * Synopsys common M-PHY Attributes
 */
#define CBRATESEL				0x8114
#define CBCREGADDRLSB				0x8116
#define CBCREGADDRMSB				0x8117
#define CBCREGWRLSB				0x8118
#define CBCREGWRMSB				0x8119
#define CBCREGRDWRSEL				0x811C
#define CBCRCTRL				0x811F
#define CBREFCLKCTRL2				0x8132

/*
 * Synopsys RX implementation specific M-PHY Attributes
 */
#define RXSQCONTROL				0x8009

#define VS_MPHYDISABLE		0xD0C1

#endif/* _UFS_SPRD_QOGIRN6PRO_H_ */
