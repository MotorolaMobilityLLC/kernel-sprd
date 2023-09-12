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

#ifndef _SPRD_VDSP_IOMMU_REG_H_
#define _SPRD_VDSP_IOMMU_REG_H_

#include <linux/io.h>

/*IOMMU register definition*/
#define CPP_INT_MSK_R1P0	0x14	/*cpp iommu interrupt */
#define CPP_INT_MSK_R2P0	0x19
#define VSP_INT_MASK		0x4
#define DCAM_INT_MASK		0X40
#define JPG_INT_EN			0x24
#define GSP_INT_EN			0x800
#define DISPC_INT_EN		0x160

#define VSP_MMU_CFG			0x140
#define DCAM_MMU_CFG		0x80
#define CPP_MMU_CFG			0x200
#define JPG_MMU_CFG			0x100
#define GSP_MMU_CFG			0x804
#define DISPC_MMU_CFG		0x800

#define MMU_EN		        0x04
#define UPDATE_OFFSET		0x08
#define FIRST_VPN_OFFSET	0x1c
#define VPN_RANGE_OFFSET	0x20
#define FIRST_PPN_OFFSET	0x10
#define DEFAULT_PPN_OFFSET	0x14
#define FIRST_VARD_OUT_ADDR_OFFSET 0x54
#define FIRST_VAWR_OUT_ADDR_OFFSET 0x58
#define FIRST_INVALID_RD_ADDR_OFFSET 0x5c
#define FIRST_INVALID_WR_ADDR_OFFSET 0x60
#define FIRST_UNSURE_RD_ADDR_OFFSET 0x64
#define FIRST_UNSURE_WR_ADDR_OFFSET 0x68
#define PT_UPDATE_QOS_OFFSET	0x34
#define MINI_PPN1_OFFSET		0x24
#define PPN1_RANGE_OFFSET		0x28
#define MINI_PPN2_OFFSET		0x2c
#define PPN2_RANGE_OFFSET		0x30
#define FIRST_PAOUT_RD_VAADDR_OFFSET 0x44
#define FIRST_PAOUT_WR_VAADDR_OFFSET 0x58
#define FIRST_PAOUT_RD_PAADDR_OFFSET 0x5c
#define FIRST_PAOUT_WR_PAADDR_OFFSET 0x50
#define REG_AUTHORITY_OFFSET	0x0c
#define MMU_INT_EN              0xa0

/*page is 4k alignment, left shift 12 bit*/
#define MMU_MAPING_PAGESIZE_SHIFFT   12

/*virt address must be 1M byte alignment, left shift 20 bit*/
#define MMU_MAPING_VIRT_ADDR_SHIFFT   20

/*page size is 4K */
#define MMU_MAPING_PAGESIZE   (1<<MMU_MAPING_PAGESIZE_SHIFFT)
#define MMU_MAPING_PAGE_MASK  (MMU_MAPING_PAGESIZE - 1)

#define VIR_TO_ENTRY_IDX(virt_addr, base_addr) \
	((virt_addr-base_addr)/MMU_MAPING_PAGESIZE)
#define MAP_SIZE_PAGE_ALIGN_UP(length) \
	((length + MMU_MAPING_PAGE_MASK) & (~MMU_MAPING_PAGE_MASK))
#define SIZE_TO_ENTRIES(size) (size/MMU_MAPING_PAGESIZE)
#define FULL_MASK 0xFFFFFFFF
#define reg_write_dword(addr, value) writel_relaxed(value, (void __iomem *)addr)
#define reg_read_dword(addr)         readl_relaxed((void __iomem *)addr)

/*-----------------------------------------------------------------------*/

/*                          FUNCTIONS HEADERS                            */

/*-----------------------------------------------------------------------*/
void mmu_vau_enable(ulong ctrl_base_addr, u32 iommu_id, u32 mmu_enable);
void mmu_vau_vaorbypass_clkgate_enable_combined(ulong ctrl_base_addr,
	u32 iommu_id);
void mmu_vau_clock_gate_enable(ulong ctrl_base_addr, u32 cg_enable);
void mmu_vau_vaout_bypass_enable(ulong ctrl_base_addr, u32 iommu_id,
	u32 iommu_type, bool vaor_bp_en);
void mmu_vau_update(ulong ctrl_base_addr, u32 iommu_id, u32 iommu_type);
void mmu_vau_first_vpn(ulong ctrl_base_addr, u32 iommu_id, u32 vp_addr);
void mmu_vau_vpn_range(ulong ctrl_base_addr, u32 iommu_id, u32 vp_range);
void mmu_vau_first_ppn(ulong ctrl_base_addr, u32 iommu_id, ulong pp_addr);
void mmu_vau_default_ppn(ulong ctrl_base_addr, u32 iommu_id, ulong pp_addr);
void mmu_vau_pt_update_arqos(ulong ctrl_base_addr, u32 arqos);
void mmu_vau_mini_ppn1(ulong ctrl_base_addr, u32 iommu_id, ulong ppn1);
void mmu_vau_ppn1_range(ulong ctrl_base_addr, u32 iommu_id, ulong ppn1_range);
void mmu_vau_mini_ppn2(ulong ctrl_base_addr, u32 iommu_id, ulong ppn2);
void mmu_vau_ppn2_range(ulong ctrl_base_addr, u32 iommu_id, ulong ppn2_range);
void mmu_vau_reg_authority(ulong ctrl_base_addr, u32 iommu_id, ulong reg_ctrl);
void mmu_vau_write_pate_totable(ulong pgt_base_addr, u32 entry_index,
	u32 ppn_addr)
	__attribute__((no_instrument_function));;
u32 mmu_vau_read_page_entry(ulong page_table_addr, u32 entry_index);
void mmu_vau_int_enable(ulong ctrl_base_addr, u32 iommu_id, u32 iommu_type);

#endif /* _SPRD_IOMMUEX_HAL_REG_H_ */
