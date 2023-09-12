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

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/semaphore.h>

#include "cpp_reg.h"
#include "cpp_drv.h"
#include "cpp_hw.h"
#include "cpp_block.h"
#define SCALE_DRV_DEBUG 1

#ifdef CPP_DEBUG
#undef CPP_DEBUG
#define CPP_DEBUG
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "SCALE_DRV: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define SCALE_WR_COEFF(addr, value) \
	CPP_REG_WR(addr, value & 0x3ffff)

int cpp_k_scale_dev_start(void *arg)
{
	struct scale_drv_private *p = NULL;
	unsigned long flags = 0;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_OWR(CPP_PATH_START, CPP_SCALE_START_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
	return 0;
}

int cpp_k_scale_dev_stop(void *arg)
{
	struct scale_drv_private *p = NULL;
	unsigned long flags = 0;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_AWR(CPP_PATH_START, (~CPP_SCALE_START_BIT));
	spin_unlock_irqrestore(p->hw_lock, flags);
	return 0;
}

int cpp_k_scale_dev_enable(void *arg)
{
	struct scale_drv_private *p = NULL;
	unsigned long flags = 0;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;
	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_OWR(CPP_PATH_EB, CPP_SCALE_PATH_EB_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
	return 0;
}

int cpp_k_scale_src_pitch_set(void *arg)
{
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	CPP_REG_MWR(CPP_PATH0_CFG4, CPP_PATH0_SRC_PITCH, p->src_pitch);

	return 0;
}

int cpp_k_scale_des_pitch_set(void *arg)
{
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	CPP_REG_MWR(CPP_PATH0_CFG5,
		CPP_PATH0_BP_DES_PITCH, p->bp_des_pitch << 16);
	CPP_REG_MWR(CPP_PATH0_CFG5, CPP_PATH0_SC_DES_PITCH, p->sc_des_pitch);

	return 0;
}

int cpp_k_scale_input_rect_set(void *arg)
{
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	CPP_REG_MWR(CPP_PATH0_CFG1, CPP_PATH0_SRC_WIDTH_MASK,
		p->src_rect.w);
	CPP_REG_MWR(CPP_PATH0_CFG1, CPP_PATH0_SRC_HEIGHT_MASK,
		p->src_rect.h << 16);
	CPP_REG_MWR(CPP_PATH0_CFG6, CPP_PATH0_SRC_OFFSET_X_MASK,
		p->src_rect.x << 16);
	CPP_REG_MWR(CPP_PATH0_CFG6, CPP_PATH0_SRC_OFFSET_Y_MASK,
		p->src_rect.y);

	return 0;
}

int cpp_k_scale_output_rect_set(void *arg)
{
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	CPP_REG_MWR(CPP_PATH0_CFG2, CPP_PATH0_SC_DES_WIDTH_MASK,
			p->sc_des_rect.w);
	CPP_REG_MWR(CPP_PATH0_CFG2, CPP_PATH0_SC_DES_HEIGHT_MASK,
			p->sc_des_rect.h << 16);
	CPP_REG_MWR(CPP_PATH0_CFG7, CPP_PATH0_SC_DES_OFFSET_X_MASK,
			p->sc_des_rect.x << 16);
	CPP_REG_MWR(CPP_PATH0_CFG7, CPP_PATH0_SC_DES_OFFSET_Y_MASK,
			p->sc_des_rect.y);
	CPP_REG_MWR(CPP_PATH0_CFG3, CPP_PATH0_BP_DES_WIDTH_MASK,
			p->bp_des_rect.w);
	CPP_REG_MWR(CPP_PATH0_CFG3, CPP_PATH0_BP_DES_HEIGHT_MASK,
			p->bp_des_rect.h << 16);
	CPP_REG_MWR(CPP_PATH0_CFG8, CPP_PATH0_BP_DES_OFFSET_X_MASK,
			p->bp_des_rect.x << 16);
	CPP_REG_MWR(CPP_PATH0_CFG8, CPP_PATH0_BP_DES_OFFSET_Y_MASK,
			p->bp_des_rect.y);

	return 0;
}

int cpp_k_scale_input_format_set(void *arg)
{
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	CPP_REG_MWR(CPP_PATH0_CFG0, CPP_PATH0_INPUT_FORMAT, p->input_fmt);

	return 0;
}

int cpp_k_scale_output_format_set(void *arg)
{
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	CPP_REG_MWR(CPP_PATH0_CFG0, CPP_PATH0_OUTPUT_FORMAT,
		(p->sc_output_fmt << 2));

	return 0;
}

int cpp_k_scale_deci_set(void *arg)
{
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	CPP_REG_MWR(CPP_PATH0_CFG0, CPP_PATH0_HOR_DECI, (p->hor_deci << 4));
	CPP_REG_MWR(CPP_PATH0_CFG0, CPP_PATH0_VER_DECI, (p->ver_deci << 6));

	return 0;
}

int cpp_k_scale_input_endian_set(void *arg)
{
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	CPP_REG_MWR(CPP_AXIM_CHN_SET, CPP_PATH0_IN_ENDIAN, p->input_endian);
	CPP_REG_MWR(CPP_AXIM_CHN_SET, CPP_PATH0_IN_UV_ENDIAN,
		p->input_uv_endian << 3);

	return 0;
}

int cpp_k_scale_output_endian_set(void *arg)
{
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	CPP_REG_MWR(CPP_AXIM_CHN_SET, CPP_PATH0_OUT_ENDIAN,
		p->output_endian << 4);
	CPP_REG_MWR(CPP_AXIM_CHN_SET, CPP_PATH0_OUT_UV_ENDIAN,
		p->output_uv_endian << 7);

	return 0;
}

int cpp_k_scale_burst_gap_set(void *arg)
{
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	CPP_REG_MWR(CPP_AXIM_BURST_GAP_PATH0, CPP_PATH0_RCH_BURST_GAP,
		p->rch_burst_gap << 16);
	CPP_REG_MWR(CPP_AXIM_BURST_GAP_PATH0, CPP_PATH0_WCH_BURST_GAP,
		p->wch_burst_gap);

	return 0;
}

int cpp_k_scale_bpen_set(void *arg)
{
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	CPP_REG_MWR(CPP_PATH0_CFG0, CPP_PATH0_BP_EB, p->bp_en << 8);

	return 0;
}

int cpp_k_scale_ini_phase_set(void *arg)
{
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	CPP_REG_MWR(CPP_PATH0_SC_Y_HOR_INI_PHASE,
		CPP_PATH0_SC_Y_HOR_INI_PHASE_INT,
		p->y_hor_ini_phase_int << 16);
	CPP_REG_MWR(CPP_PATH0_SC_Y_HOR_INI_PHASE,
		CPP_PATH0_SC_Y_HOR_INI_PHASE_FRAC,
		p->y_hor_ini_phase_frac);
	CPP_REG_MWR(CPP_PATH0_SC_UV_HOR_INI_PHASE,
		CPP_PATH0_SC_UV_HOR_INI_PHASE_INT,
		p->uv_hor_ini_phase_int << 16);
	CPP_REG_MWR(CPP_PATH0_SC_UV_HOR_INI_PHASE,
		CPP_PATH0_SC_UV_HOR_INI_PHASE_FRAC,
		p->uv_hor_ini_phase_frac);
	CPP_REG_MWR(CPP_PATH0_SC_Y_VER_INI_PHASE,
		CPP_PATH0_SC_Y_VER_INI_PHASE_INT,
		p->y_ver_ini_phase_int << 16);
	CPP_REG_MWR(CPP_PATH0_SC_Y_VER_INI_PHASE,
		CPP_PATH0_SC_Y_VER_INI_PHASE_FRAC,
		p->y_ver_ini_phase_frac);
	CPP_REG_MWR(CPP_PATH0_SC_UV_VER_INI_PHASE,
		CPP_PATH0_SC_UV_VER_INI_PHASE_INT,
		p->uv_ver_ini_phase_int << 16);
	CPP_REG_MWR(CPP_PATH0_SC_UV_VER_INI_PHASE,
		CPP_PATH0_SC_UV_VER_INI_PHASE_FRAC,
		p->uv_ver_ini_phase_frac);

	return 0;
}

int cpp_k_scale_tap_set(void *arg)
{
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	CPP_REG_MWR(CPP_PATH0_SC_TAP, CPP_PATH0_Y_VER_TAP, p->y_ver_tap << 5);
	CPP_REG_MWR(CPP_PATH0_SC_TAP, CPP_PATH0_UV_VER_TAP, p->uv_ver_tap);

	return 0;
}

int cpp_k_scale_offset_size_set(void *arg)
{
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	CPP_REG_MWR(CPP_PATH0_SC_IN_TRIM_OFFSET,
		CPP_PATH0_SC_IN_TRIM_OFFSET_X_MASK, p->sc_intrim_rect.x << 16);
	CPP_REG_MWR(CPP_PATH0_SC_IN_TRIM_OFFSET,
		CPP_PATH0_SC_IN_TRIM_OFFSET_Y_MASK, p->sc_intrim_rect.y);
	CPP_REG_MWR(CPP_PATH0_SC_IN_TRIM_SIZE,
		CPP_PATH0_SC_IN_TRIM_HEIGHT_MASK, p->sc_intrim_rect.h << 16);
	CPP_REG_MWR(CPP_PATH0_SC_IN_TRIM_SIZE,
		CPP_PATH0_SC_IN_TRIM_WIDTH_MASK, p->sc_intrim_rect.w);
	CPP_REG_MWR(CPP_PATH0_SC_OUT_TRIM_OFFSET,
		CPP_PATH0_SC_OUT_TRIM_OFFSET_X_MASK,
		p->sc_outtrim_rect.x << 16);
	CPP_REG_MWR(CPP_PATH0_SC_OUT_TRIM_OFFSET,
		CPP_PATH0_SC_OUT_TRIM_OFFSET_Y_MASK, p->sc_outtrim_rect.y);
	CPP_REG_MWR(CPP_PATH0_SC_OUT_TRIM_SIZE,
		CPP_PATH0_SC_OUT_TRIM_HEIGHT_MASK, p->sc_outtrim_rect.h << 16);
	CPP_REG_MWR(CPP_PATH0_SC_OUT_TRIM_SIZE,
		CPP_PATH0_SC_OUT_TRIM_WIDTH_MASK, p->sc_outtrim_rect.w);
	CPP_REG_MWR(CPP_PATH0_BP_TRIM_OFFSET,
		CPP_PATH0_BP_TRIM_OFFSET_X_MASK, p->bp_trim_rect.x << 16);
	CPP_REG_MWR(CPP_PATH0_BP_TRIM_OFFSET,
		CPP_PATH0_BP_TRIM_OFFSET_Y_MASK, p->bp_trim_rect.y);
	CPP_REG_MWR(CPP_PATH0_BP_TRIM_SIZE,
		CPP_PATH0_BP_TRIM_HEIGHT_MASK, p->bp_trim_rect.h << 16);
	CPP_REG_MWR(CPP_PATH0_BP_TRIM_SIZE,
		CPP_PATH0_BP_TRIM_WIDTH_MASK, p->bp_trim_rect.w);
	CPP_REG_MWR(CPP_PATH0_SC_FULL_IN_SIZE,
		CPP_PATH0_SC_FULL_IN_HEIGHT_MASK, p->sc_full_in_size.h << 16);
	CPP_REG_MWR(CPP_PATH0_SC_FULL_IN_SIZE,
		CPP_PATH0_SC_FULL_IN_WIDTH_MASK, p->sc_full_in_size.w);
	CPP_REG_MWR(CPP_PATH0_SC_FULL_OUT_SIZE,
		CPP_PATH0_SC_FULL_OUT_HEIGHT_MASK, p->sc_full_out_size.h << 16);
	CPP_REG_MWR(CPP_PATH0_SC_FULL_OUT_SIZE,
		CPP_PATH0_SC_FULL_OUT_WIDTH_MASK, p->sc_full_out_size.w);
	CPP_REG_MWR(CPP_PATH0_SC_SLICE_IN_SIZE,
		CPP_PATH0_SC_SLICE_IN_HEIGHT_MASK, p->sc_slice_in_size.h << 16);
	CPP_REG_MWR(CPP_PATH0_SC_SLICE_IN_SIZE,
		CPP_PATH0_SC_SLICE_IN_WIDTH_MASK, p->sc_slice_in_size.w);
	CPP_REG_MWR(CPP_PATH0_SC_SLICE_OUT_SIZE,
		CPP_PATH0_SC_SLICE_OUT_HEIGHT_MASK,
		p->sc_slice_out_size.h << 16);
	CPP_REG_MWR(CPP_PATH0_SC_SLICE_OUT_SIZE,
		CPP_PATH0_SC_SLICE_OUT_WIDTH_MASK, p->sc_slice_out_size.w);

	return 0;
}

int cpp_k_scale_addr_set(void *arg)
{
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	CPP_REG_WR(CPP_PATH0_SRC_ADDR_Y, p->iommu_src.iova[0]);
	CPP_REG_WR(CPP_PATH0_SRC_ADDR_UV, p->iommu_src.iova[1]);
	CPP_REG_WR(CPP_PATH0_SC_DES_ADDR_Y, p->iommu_dst.iova[0]);
	CPP_REG_WR(CPP_PATH0_SC_DES_ADDR_UV, p->iommu_dst.iova[1]);
	CPP_REG_WR(CPP_PATH0_BP_DES_ADDR_Y, p->iommu_dst_bp.iova[0]);
	CPP_REG_WR(CPP_PATH0_BP_DES_ADDR_UV, p->iommu_dst_bp.iova[1]);
	CPP_TRACE("iommu_src y:0x%lx, uv:0x%lx\n",
		p->iommu_src.iova[0], p->iommu_src.iova[1]);
	CPP_TRACE("iommu_dst y:0x%lx, uv:0x%lx\n",
		p->iommu_dst.iova[0], p->iommu_dst.iova[1]);
	CPP_TRACE("iommu_dst_bp y:0x%lx, uv:0x%lx\n",
		p->iommu_dst_bp.iova[0], p->iommu_dst_bp.iova[1]);

	return 0;
}

int cpp_k_scale_dev_disable(void *arg)
{
	struct scale_drv_private *p = NULL;
	unsigned long flags;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;
	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_AWR(CPP_PATH_EB, (~CPP_SCALE_PATH_EB_BIT));
	spin_unlock_irqrestore(p->hw_lock, flags);
	return 0;
}

int cpp_k_scale_clk_switch(void *arg)
{
	int flags =  0;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	flags = *( (int *) arg);
	if (flags == 1)
		CPP_REG_MWR(CPP_PATH0_CFG0, CPP_PATH0_CLK_SWITCH, CPP_PATH0_CLK_SWITCH);
	else if (flags == 0)
		CPP_REG_AWR(CPP_PATH0_CFG0, ~CPP_PATH0_CLK_SWITCH);
	else {
		pr_err("fail to get valid clk switch flags\n");
		return -EFAULT;
	}

	return 0;
}

int cpp_k_scale_luma_hcoeff_set(void *arg)
{
	struct scale_drv_private *p = NULL;
	unsigned long luma_h_coeff_addr = CPP_BASE;
	int coeff = 0;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;
	luma_h_coeff_addr += PATH0_LUMA_H_COEFF_BASE_ADDR;

	luma_h_coeff_addr += p->coeff_addr_offset;
	coeff = p->coeff_arg;
	SCALE_WR_COEFF(luma_h_coeff_addr, coeff);

	return 0;
}

int cpp_k_scale_chrima_hcoeff_set(void *arg)
{
	struct scale_drv_private *p = NULL;
	unsigned long chrima_h_coeff_addr = CPP_BASE;
	int coeff = 0;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	chrima_h_coeff_addr += PATH0_CHROMA_H_COEFF_BASE_ADDR;

	chrima_h_coeff_addr += p->coeff_addr_offset;
	coeff = p->coeff_arg;
	SCALE_WR_COEFF(chrima_h_coeff_addr, coeff);

	return 0;
}

int cpp_k_scale_vcoeff_set(void *arg)
{
	struct scale_drv_private *p = NULL;
	unsigned long v_coeff_addr = CPP_BASE;
	int coeff = 0;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	v_coeff_addr += PATH0_V_COEFF_BASE_ADDR;
	v_coeff_addr += p->coeff_addr_offset;
	coeff = p->coeff_arg;
	SCALE_WR_COEFF(v_coeff_addr, coeff);

	return 0;
}

int cpp_k_scale_reg_trace(void *arg)
{
#ifdef SCALE_DRV_DEBUG
	unsigned long addr = 0;
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid arg\n");
		return -EFAULT;
	}
	p= (struct scale_drv_private *) arg;

	CPP_TRACE("CPP:Scaler Register list\n");
	for (addr = CPP_PATH_EB; addr <= CPP_PATH0_BP_YUV_REGULATE_2;
		addr += 16) {
		CPP_TRACE("0x%lx: 0x%8x 0x%8x 0x%8x 0x%8x\n", addr,
			CPP_REG_RD(addr), CPP_REG_RD(addr + 4),
			CPP_REG_RD(addr + 8), CPP_REG_RD(addr + 12));
	}
#endif

	return 0;
}
