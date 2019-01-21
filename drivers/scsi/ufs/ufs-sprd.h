 /*
 * Copyright (C) 2015-2018 Spreadtrum Communications Inc.
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
	void __iomem *ufsutp_reg;
	void __iomem *unipro_reg;
	void __iomem *ufs_ao_reg;
	struct syscon_ufs aon_apb_ufs_en;
	struct syscon_ufs ap_apb_ufs_en;
	struct syscon_ufs ap_apb_ufs_rst;
	struct syscon_ufs anlg_mphy_ufs_rst;
	struct syscon_ufs aon_apb_ufs_rst;
};

#endif/* _UFS_SPRD_H_ */
