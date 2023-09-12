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

#include <linux/types.h>
#include "sprd_vdsp_iommuvau_register.h"

void putbit(ulong reg_addr, u32 dst_value, u8 pos)
{
	u32 org_value = 0;
	u32 new_value = 0;
	u32 mask = 1U << pos;

	org_value = reg_read_dword(reg_addr);
	new_value = (org_value & ~mask) | ((dst_value << pos) & mask);

	reg_write_dword(reg_addr, new_value);
}

void putbits(ulong reg_addr, u32 dst_value, u8 highbitoffset, u8 lowbitoffset)
{
	u32 org_value = 0;
	u32 new_value = 0;
	u32 mask = (FULL_MASK >> (32 - (highbitoffset - lowbitoffset + 1))) << lowbitoffset;

	org_value = reg_read_dword(reg_addr);
	new_value = (org_value & ~mask) | ((dst_value << lowbitoffset) & mask);

	reg_write_dword(reg_addr, new_value);
}

void mmu_vau_enable(ulong ctrl_base_addr, u32 iommu_id, u32 mmu_enable)
{
	ulong reg_addr = ctrl_base_addr;

	(void)iommu_id;

	putbit(reg_addr, mmu_enable, 0);
}

/*
 * sharkl3 dpu register is shadowed to internal ram, so we have to set
 * vaorbypass, clkgate and enable in a single function.
 */
void mmu_vau_vaorbypass_clkgate_enable_combined(ulong ctrl_base_addr,
	u32 iommu_id)
{
	ulong reg_addr = ctrl_base_addr + MMU_EN;
	u32 reg_value = 0;

	(void)iommu_id;

	reg_value = reg_read_dword(reg_addr);
	reg_write_dword(reg_addr, reg_value | 0x13);
}

void mmu_vau_clock_gate_enable(ulong ctrl_base_addr, u32 cg_enable)
{
	ulong reg_addr = ctrl_base_addr;

	putbit(reg_addr, cg_enable, 1);
}

void mmu_vau_vaout_bypass_enable(ulong ctrl_base_addr, u32 iommu_id,
	u32 iommu_type, bool vaor_bp_en)
{
	ulong reg_addr = ctrl_base_addr + MMU_EN;

	(void)iommu_id;

	if (vaor_bp_en)
		putbit(reg_addr, 1, 4);
	else
		putbit(reg_addr, 0, 4);
}

void mmu_vau_update(ulong ctrl_base_addr, u32 iommu_id, u32 iommu_type)
{
	ulong reg_addr = ctrl_base_addr + UPDATE_OFFSET;

	reg_write_dword(reg_addr, 0xffffffff);
}

void mmu_vau_first_vpn(ulong ctrl_base_addr, u32 iommu_id, u32 vp_addr)
{
	ulong reg_addr = ctrl_base_addr;

	(void)iommu_id;

	reg_addr += FIRST_VPN_OFFSET;
	reg_write_dword(reg_addr, (vp_addr >> MMU_MAPING_PAGESIZE_SHIFFT));
}

void mmu_vau_vpn_range(ulong ctrl_base_addr, u32 iommu_id, u32 vp_range)
{
	ulong reg_addr = ctrl_base_addr + VPN_RANGE_OFFSET;

	(void)iommu_id;

	reg_write_dword(reg_addr, vp_range);
}

void mmu_vau_first_ppn(ulong ctrl_base_addr, u32 iommu_id, ulong pp_addr)
{
	ulong reg_addr = ctrl_base_addr;

	(void)iommu_id;

	reg_addr += FIRST_PPN_OFFSET;
	reg_write_dword(reg_addr, (pp_addr >> MMU_MAPING_PAGESIZE_SHIFFT));
}

void mmu_vau_default_ppn(ulong ctrl_base_addr, u32 iommu_id, ulong pp_addr)
{
	ulong reg_addr = ctrl_base_addr;

	(void)iommu_id;

	reg_addr += DEFAULT_PPN_OFFSET;
	reg_write_dword(reg_addr, (pp_addr >> MMU_MAPING_PAGESIZE_SHIFFT));
}

void mmu_vau_pt_update_arqos(ulong ctrl_base_addr, u32 arqos)
{
	ulong reg_addr = ctrl_base_addr + PT_UPDATE_QOS_OFFSET;

	reg_write_dword(reg_addr, (arqos & 0xf));
}

/*1M align*/
void mmu_vau_mini_ppn1(ulong ctrl_base_addr, u32 iommu_id, ulong ppn1)
{
	ulong reg_addr = 0;

	(void)iommu_id;

	reg_addr = ctrl_base_addr + MINI_PPN1_OFFSET;
	reg_write_dword(reg_addr, (ppn1 >> 20));
}

void mmu_vau_ppn1_range(ulong ctrl_base_addr, u32 iommu_id, ulong ppn1_range)
{
	ulong reg_addr = 0;

	(void)iommu_id;

	reg_addr = ctrl_base_addr + PPN1_RANGE_OFFSET;
	reg_write_dword(reg_addr, (ppn1_range >> 20));
}

void mmu_vau_mini_ppn2(ulong ctrl_base_addr, u32 iommu_id, ulong ppn2)
{
	ulong reg_addr = 0;

	(void)iommu_id;

	reg_addr = ctrl_base_addr + MINI_PPN2_OFFSET;
	reg_write_dword(reg_addr, (ppn2 >> 20));
}

void mmu_vau_ppn2_range(ulong ctrl_base_addr, u32 iommu_id, ulong ppn2_range)
{
	ulong reg_addr = 0;

	(void)iommu_id;

	reg_addr = ctrl_base_addr + PPN2_RANGE_OFFSET;
	reg_write_dword(reg_addr, (ppn2_range >> 20));
}

void mmu_vau_reg_authority(ulong ctrl_base_addr, u32 iommu_id, ulong reg_ctrl)
{
	ulong reg_addr = ctrl_base_addr;

	(void)iommu_id;

	reg_addr += REG_AUTHORITY_OFFSET;
	putbit(reg_addr, reg_ctrl, 0);
}

void mmu_vau_write_pate_totable(ulong pgt_base_addr,
	u32 entry_index, u32 ppn_addr)
{
	ulong pgt_addr = pgt_base_addr + entry_index * 4;

	reg_write_dword(pgt_addr, ppn_addr);
}

u32 mmu_vau_read_page_entry(ulong page_table_addr, u32 entry_index)
{
	ulong reg_addr = page_table_addr + entry_index * 4;
	u32 phy_addr = 0;

	phy_addr = reg_read_dword(reg_addr);
	return phy_addr;
}

void mmu_vau_int_enable(ulong ctrl_base_addr, u32 iommu_id, u32 iommu_type)
{
	ulong reg_addr = ctrl_base_addr + MMU_INT_EN;

	reg_write_dword(reg_addr, 0xff);
}
