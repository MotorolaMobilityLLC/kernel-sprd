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

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "SCALE_DRV: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

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
	reg_owr(p, CPP_PATH_START, CPP_SCALE_START_BIT);
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
	reg_awr(p, CPP_PATH_START, (~CPP_SCALE_START_BIT));
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
	reg_owr(p, CPP_PATH_EB, CPP_SCALE_PATH_EB_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
	return 0;
}

int cpp_k_scale_src_pitch_set(void *arg)
{
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	cfg_parm = &p->cfg_parm;

	reg_mwr(p, CPP_PATH0_CFG3, CPP_SCALE_SRC_PITCH_MASK,
		cfg_parm->input_size.w);
	return 0;
}

int cpp_k_scale_des_pitch_set(void *arg)
{
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;
	cfg_parm = &p->cfg_parm;

	reg_mwr(p, CPP_PATH0_CFG3, CPP_SCALE_DES_PITCH_MASK,
		cfg_parm->output_size.w << 16);

	return 0;
}

int cpp_k_scale_input_rect_set(void *arg)
{
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;
	cfg_parm = &p->cfg_parm;

	reg_mwr(p, CPP_PATH0_CFG1, CPP_SCALE_SRC_HEIGHT_MASK,
		cfg_parm->input_rect.h << 16);
	reg_mwr(p, CPP_PATH0_CFG1, CPP_SCALE_SRC_WIDTH_MASK,
		cfg_parm->input_rect.w);
	reg_mwr(p, CPP_PATH0_CFG4, CPP_SCALE_SRC_OFFSET_X_MASK,
		cfg_parm->input_rect.x << 16);
	reg_mwr(p, CPP_PATH0_CFG4, CPP_SCALE_SRC_OFFSET_Y_MASK,
		cfg_parm->input_rect.y);

	return 0;
}

int cpp_k_scale_output_rect_set(void *arg)
{
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;
	cfg_parm = &p->cfg_parm;

	reg_mwr(p, CPP_PATH0_CFG2, CPP_SCALE_DES_HEIGHT_MASK,
			cfg_parm->output_size.h << 16);
	reg_mwr(p, CPP_PATH0_CFG2, CPP_SCALE_DES_WIDTH_MASK,
			cfg_parm->output_size.w);

	reg_wr(p, CPP_PATH0_CFG5, 0);

	return 0;
}

int cpp_k_scale_input_format_set(void *arg)
{
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;
	cfg_parm = &p->cfg_parm;

	reg_mwr(p, CPP_PATH0_CFG0, CPP_SCALE_INPUT_FORMAT,
		(cfg_parm->input_format << 2));

	return 0;
}

int cpp_k_scale_output_format_set(void *arg)
{
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;
	cfg_parm = &p->cfg_parm;

	reg_mwr(p, CPP_PATH0_CFG0, CPP_SCALE_OUTPUT_FORMAT,
		(cfg_parm->output_format << 4));

	return 0;
}

int cpp_k_scale_input_endian_set(void *arg)
{
	unsigned int y_endian = 0;
	unsigned int uv_endian = 0;
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;
	cfg_parm = &p->cfg_parm;

	if (cfg_parm->input_endian.y_endian >= SCALE_ENDIAN_MAX
	 || cfg_parm->input_endian.uv_endian >= SCALE_ENDIAN_MAX) {
		pr_err("invalid input endian %d %d\n",
			cfg_parm->input_endian.y_endian,
			cfg_parm->input_endian.uv_endian);
		return -EINVAL;
	}

	if (cfg_parm->input_endian.y_endian == SCALE_ENDIAN_LITTLE)
		y_endian = 0;

	if (cfg_parm->input_endian.y_endian == SCALE_ENDIAN_LITTLE
	 && cfg_parm->input_endian.uv_endian == SCALE_ENDIAN_HALFBIG)
		uv_endian = 1;

	reg_mwr(p, CPP_AXIM_CHN_SET, (CPP_SCALE_DMA_INPUT_Y_ENDIAN),
		y_endian);
	reg_mwr(p, CPP_AXIM_CHN_SET, (CPP_SCALE_DMA_INPUT_UV_ENDIAN),
		uv_endian << 3);

	pr_debug("CPP:input endian y:%d, uv:%d\n", y_endian, uv_endian);


	return 0;
}

int cpp_k_scale_output_endian_set(void *arg)
{
	unsigned int y_endian = 0;
	unsigned int uv_endian = 0;
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;
	cfg_parm = &p->cfg_parm;

	if (cfg_parm->output_endian.y_endian >= SCALE_ENDIAN_MAX
	 || cfg_parm->output_endian.uv_endian >= SCALE_ENDIAN_MAX) {
		pr_err("fail to get valid output endian %d %d\n",
			cfg_parm->output_endian.y_endian,
			cfg_parm->output_endian.uv_endian);
		return -EINVAL;
	}
	if (cfg_parm->output_endian.y_endian == SCALE_ENDIAN_LITTLE)
		y_endian = 0;

	if (cfg_parm->output_endian.y_endian == SCALE_ENDIAN_LITTLE
	 && cfg_parm->output_endian.uv_endian == SCALE_ENDIAN_HALFBIG)
		uv_endian = 1;

	reg_mwr(p, CPP_AXIM_CHN_SET, (CPP_SCALE_DMA_OUTPUT_Y_ENDIAN),
		y_endian << 4);
	reg_mwr(p, CPP_AXIM_CHN_SET, (CPP_SCALE_DMA_OUTPUT_UV_ENDIAN),
		uv_endian << 7);

	pr_debug("CPP:output endian y:%d, uv:%d\n",
		y_endian, uv_endian);

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

	reg_wr(p, CPP_PATH0_SRC_ADDR_Y,
		p->iommu_src.iova[0]);
	reg_wr(p, CPP_PATH0_SRC_ADDR_UV,
		p->iommu_src.iova[1]);
	reg_wr(p, CPP_PATH0_DES_ADDR_Y,
		p->iommu_dst.iova[0]);
	reg_wr(p, CPP_PATH0_DES_ADDR_UV,
		p->iommu_dst.iova[1]);
	pr_debug("iommu_src y:0x%lx, uv:0x%lx\n",
		p->iommu_src.iova[0], p->iommu_src.iova[1]);
	pr_debug("iommu_dst y:0x%lx, uv:0x%lx\n",
		p->iommu_dst.iova[0], p->iommu_dst.iova[1]);

	return 0;
}

int cpp_k_scale_dev_disable(void *arg)
{
	unsigned long flags= 0;
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;
	spin_lock_irqsave(p->hw_lock, flags);
	reg_awr(p, CPP_PATH_EB, (~CPP_SCALE_PATH_EB_BIT));
	spin_unlock_irqrestore(p->hw_lock, flags);
	return 0;
}

int cpp_k_scale_deci_set(void *arg)
{
	int i = 0;
	unsigned int deci_tmp_w = 0;
	unsigned int deci_tmp_h = 0;
	unsigned int div_factor = 1;
	unsigned int deci_val = 0;
	unsigned int pixel_aligned_num = 0;
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;
	cfg_parm = &p->cfg_parm;

	if (cfg_parm->input_rect.w <=
		(cfg_parm->output_size.w * SCALE_SC_COEFF_MID))
		deci_tmp_w = 0;
	else if (cfg_parm->input_rect.w >
		(cfg_parm->output_size.w * SCALE_SC_COEFF_MID) &&
		(cfg_parm->input_rect.w <=
		(cfg_parm->output_size.w * SCALE_SC_COEFF_MID *
		(1 << SCALE_DECI_FAC_MAX)))) {
		for (i = 1; i <= SCALE_DECI_FAC_MAX; i++) {
			div_factor =
			(unsigned int)(SCALE_SC_COEFF_MID * (1 << i));
			if (cfg_parm->input_rect.w <=
				(cfg_parm->output_size.w * div_factor)) {
				break;
			}
		}
		deci_tmp_w = i;
	} else
		deci_tmp_w = SCALE_DECI_FAC_MAX;

	deci_val = (1 << deci_tmp_w);
	pixel_aligned_num =
		(deci_val >= SCALE_PIXEL_ALIGNED)
		? deci_val : SCALE_PIXEL_ALIGNED;

	p->sc_full_in_size.w = cfg_parm->input_rect.w >> deci_tmp_w;

	if (pixel_aligned_num > 0 &&
		(p->sc_full_in_size.w % pixel_aligned_num)) {
		p->sc_full_in_size.w =
			p->sc_full_in_size.w / pixel_aligned_num
			* pixel_aligned_num;

		cfg_parm->input_rect.w = p->sc_full_in_size.w << deci_tmp_w;
	}
	p->ver_deci = deci_tmp_w;

	if (cfg_parm->input_rect.h <=
		(cfg_parm->output_size.h * SCALE_SC_COEFF_MID))
		deci_tmp_h = 0;
	else if ((cfg_parm->input_rect.h >
		(cfg_parm->output_size.h * SCALE_SC_COEFF_MID)) &&
		(cfg_parm->input_rect.h <=
		(cfg_parm->output_size.h * SCALE_SC_COEFF_MID) *
		(1 << SCALE_DECI_FAC_MAX))) {
		for (i = 1; i <= SCALE_DECI_FAC_MAX; i++) {
			div_factor =
			(unsigned int)(SCALE_SC_COEFF_MID * (1 << i));
			if (cfg_parm->input_rect.h <=
				(cfg_parm->output_size.h * div_factor)) {
				break;
			}
		}
		deci_tmp_h = i;
	} else
		deci_tmp_h = SCALE_DECI_FAC_MAX;

	deci_val = (1 << deci_tmp_h);
	pixel_aligned_num =
		(deci_val >= SCALE_PIXEL_ALIGNED)
		? deci_val : SCALE_PIXEL_ALIGNED;

	p->sc_full_in_size.h = cfg_parm->input_rect.h >> deci_tmp_h;

	if (pixel_aligned_num > 0 &&
		(p->sc_full_in_size.h % pixel_aligned_num)) {
		p->sc_full_in_size.h =
			p->sc_full_in_size.h / pixel_aligned_num
			* pixel_aligned_num;

		cfg_parm->input_rect.h = p->sc_full_in_size.h << deci_tmp_h;
	}
	p->hor_deci = deci_tmp_h;

	reg_mwr(p, CPP_PATH0_CFG0,
		CPP_SCALE_DEC_H_MASK, deci_tmp_w << 4);
	reg_mwr(p, CPP_PATH0_CFG0,
		CPP_SCALE_DEC_V_MASK, deci_tmp_h << 6);

	pr_debug("sc_input_size %d %d, deci(w,h) %d, %d input_rect %d %d\n",
		p->sc_full_in_size.w,
		p->sc_full_in_size.h,
		deci_tmp_w, deci_tmp_h,
		cfg_parm->input_rect.w,
		cfg_parm->input_rect.h);

	return 0;
}

int cpp_k_scale_reg_trace(void *arg)
{
#ifdef SCALE_DRV_DEBUG
	unsigned long addr = 0;
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;

	pr_info("CPP:Scaler Register list\n");
	for (addr = CPP_PATH0_SRC_ADDR_Y; addr <= CPP_PATH0_CFG5;
		addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			CPP_REG_RD(p, addr), CPP_REG_RD(p, addr + 4),
			CPP_REG_RD(p, addr + 8), CPP_REG_RD(p, addr + 12));
	}
#endif
	return 0;
}
