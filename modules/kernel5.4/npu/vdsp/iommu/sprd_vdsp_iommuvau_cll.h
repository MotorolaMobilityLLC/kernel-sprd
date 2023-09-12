/*
 * SPDX-FileCopyrightText: 2020-2022 Unisoc (Shanghai) Technologies Co., Ltd
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2020-2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#ifndef _SPRD_VDSP_IOMMU_CLL_H_
#define _SPRD_VDSP_IOMMU_CLL_H_
#include  <linux/types.h>
#include "sprd_vdsp_iommus.h"
#include "sprd_vdsp_iommu_dev.h"

/* General driver error definition */
enum {
	SPRD_NO_ERR = 0x100,
	SPRD_ERR_INVALID_PARAM,
	SPRD_ERR_INITIALIZED,
	SPRD_ERR_INVALID_HDL,
	SPRD_ERR_STATUS,
	SPRD_ERR_RESOURCE_BUSY,
	SPRD_ERR_ILLEGAL_PARAM,
	SPRD_ERR_MAX,
};

struct sprd_vdsp_iommu_init_param {
	enum sprd_vdsp_iommu_type iommu_type;	//get_iommuvau_type(data->iommuex_rev, &chip);
	enum VDSP_IOMMU_ID iommu_id;	//iommu_dev->id;
	ulong master_reg_addr;	//iommu_dev->pgt_base;
	ulong base_reg_addr;
	u32 pgt_size;
	ulong ctrl_reg_addr;	//iommu_dev->ctrl_reg

	ulong fm_base_addr;	//iommu_dev->iova_base
	u32 fm_ram_size;	//iommu_dev->iova_size;
	u64 faultpage_addr;	/* Enabel fault page function */// iommu_dev->fault_page;
	u8 ram_clk_div;		/*Clock divisor */
	unsigned long pagt_base_ddr;	//iommu_dev->pagt_base_ddr;
	unsigned long pagt_base_virt;	//iommu_dev->pagt_base_virt;
	unsigned int pagt_ddr_size;	//iommu_dev->pagt_ddr_size;

	/*for sharkl2/isharkl2 */
	u64 mini_ppn1;
	u64 ppn1_range;
	u64 mini_ppn2;
	u64 ppn2_range;
	int chip;		//=chip
};

struct sprd_iommu_map_param {
	u64 start_virt_addr;
	u32 total_map_size;
	struct sg_table *p_sg_table;
	u32 sg_offset;
};

struct sprd_iommu_unmap_param {
	u64 start_virt_addr;
	u32 total_map_size;
	u32 ch_id;
};

struct sprd_vdsp_iommu_widget {
	void *p_priv;
	struct sprd_vdsp_iommu_func_tbl *p_iommu_tbl;
};
struct sprd_vdsp_iommu_func_tbl {
	u32(*init) (struct sprd_vdsp_iommu_init_param *p_init_param,
		struct sprd_vdsp_iommu_widget *p_iommu_hdl);
	u32(*uninit) (struct sprd_vdsp_iommu_widget *p_iommu_hdl);

	u32(*map) (struct sprd_vdsp_iommu_widget *p_iommu_hdl,
		struct sprd_iommu_map_param *p_map_param);
	u32(*unmap) (struct sprd_vdsp_iommu_widget *p_iommu_hdl,
		struct sprd_iommu_unmap_param *p_unmap_param);

	u32(*enable) (struct sprd_vdsp_iommu_widget *p_iommu_hdl);
	u32(*disable) (struct sprd_vdsp_iommu_widget *p_iommu_hdl);

	u32(*suspend) (struct sprd_vdsp_iommu_widget *p_iommu_hdl);
	u32(*resume) (struct sprd_vdsp_iommu_widget *p_iommu_hdl);
	u32(*release) (struct sprd_vdsp_iommu_widget *p_iommu_hdl);

	u32(*reset) (struct sprd_vdsp_iommu_widget *p_iommu_hdl,
		u32 channel_num);
	u32(*set_bypass) (struct sprd_vdsp_iommu_widget *p_iommu_hdl,
		bool vaor_bp_en);
	u32(*virttophy) (struct sprd_vdsp_iommu_widget *p_iommu_hdl,
		u64 virt_addr, u64 *dest_addr);

	u32(*unmap_orphaned) (struct sprd_vdsp_iommu_widget *p_iommu_hdl,
		struct sprd_iommu_unmap_param *p_unmap_param);
};

struct sprd_vdsp_iommuvau_interrupt {
	u8 pa_out_range_r_en;
	u8 pa_out_range_w_en;
	u8 va_out_range_r_en;
	u8 va_out_range_w_en;
	u8 invalid_r_en;
	u8 invalid_w_en;
	u8 unsecure_r_en;
	u8 unsecure_w_en;
};

struct sprd_vdsp_iommuvau_priv {
	ulong master_reg_addr;	/*master reg base address */
	ulong mmu_reg_addr;	/*mmu register offset from master base addr */
	u32 pgt_size;

	u8 va_out_bypass_en;	/*va out of range bypass,1 default */
	ulong vpn_base_addr;
	u32 vpn_range;
	ulong ppn_base_addr;	/*pagetable base addr in ddr */
	ulong default_addr;
	ulong mini_ppn1;
	ulong ppn1_range;
	ulong mini_ppn2;
	ulong ppn2_range;
	/*iommu reserved memory of pf page table */
	unsigned long pagt_base_ddr;
	unsigned int pagt_ddr_size;
	unsigned long pagt_base_phy_ddr;

	u8 ram_clk_div;		/*Clock divisor */

	u32 map_cnt;
	enum sprd_vdsp_iommu_type iommu_type;
	enum VDSP_IOMMU_ID iommu_id;
	int chip;
	struct sprd_vdsp_iommuvau_interrupt st_interrupt;
};

u32 sprd_vdsp_iommuvau_cll_init(struct sprd_vdsp_iommu_init_param *p_init_param,
	struct sprd_vdsp_iommu_widget *p_iommu_hdl);
u32 sprd_vdsp_iommuvau_cll_uninit(struct sprd_vdsp_iommu_widget *p_iommu_hdl);
u32 sprd_vdsp_iommuvau_cll_map(struct sprd_vdsp_iommu_widget *p_iommu_hdl,
	struct sprd_iommu_map_param *p_map_param);
u32 sprd_vdsp_iommuvau_cll_unmap(struct sprd_vdsp_iommu_widget *p_iommu_hdl,
	struct sprd_iommu_unmap_param *p_unmap_param);
u32 sprd_vdsp_iommuvau_cll_unmap_orphaned(struct sprd_vdsp_iommu_widget *p_iommu_hdl,
	struct sprd_iommu_unmap_param *p_unmap_param);
u32 sprd_vdsp_iommuvau_cll_enable(struct sprd_vdsp_iommu_widget *p_iommu_hdl);
u32 sprd_vdsp_iommuvau_cll_disable(struct sprd_vdsp_iommu_widget *p_iommu_hdl);
u32 sprd_vdsp_iommuvau_cll_suspend(struct sprd_vdsp_iommu_widget *p_iommu_hdl);
u32 sprd_vdsp_iommuvau_cll_resume(struct sprd_vdsp_iommu_widget *p_iommu_hdl);
u32 sprd_vdsp_iommuvau_cll_release(struct sprd_vdsp_iommu_widget *p_iommu_hdl);
u32 sprd_vdsp_iommuvau_cll_reset(struct sprd_vdsp_iommu_widget *p_iommu_hdl,
	u32 channel_num);
u32 sprd_vdsp_iommuvau_cll_set_bypass(struct sprd_vdsp_iommu_widget *p_iommu_hdl,
	bool vaor_bp_en);
u32 sprd_vdsp_iommuvau_cll_virt_to_phy(struct sprd_vdsp_iommu_widget *p_iommu_hdl,
	u64 virt_addr, u64 *dest_addr);
u32 sprd_vdsp_iommuvau_reg_authority(struct sprd_vdsp_iommu_widget *p_iommu_hdl,
	u8 authority);
void sprd_vdsp_iommuvau_flush_pgt(ulong ppn_base, u32 start_entry, u32 end_entry);

extern struct sprd_vdsp_iommu_func_tbl iommuvau_func_tbl;
#endif /* _SPRD_IOMMU_CLL_H_ */
