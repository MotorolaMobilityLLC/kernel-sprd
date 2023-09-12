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
#include "cpp_hw.h"
#include "cpp_block.h"

/* Macro Definition */
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "SCALE_DRV: %d %d %s: " \
	fmt, current->pid, __LINE__, __func__
#define SCALE_DRV_DEBUG 0
/* Internal Function Implementation */

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

int cpp_k_scale_dev_start(void *arg)
{
	unsigned long flags = 0;
	struct scale_drv_private *p = NULL;

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
		cfg_parm->input_size.w & CPP_SCALE_SRC_PITCH_MASK);
	return 0;
}

static int scale_k_check_sc_limit(struct scale_drv_private *p)
{
	int ret = 0;
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;

	if (!p) {
		pr_err("fail to get Input ptr\n");
		return -EINVAL;
	}
	cfg_parm = &p->cfg_parm;

	/* check for width */
	if (p->sc_full_in_size.w > (8 * cfg_parm->output_size.w)
	 || p->sc_full_in_size.w < cfg_parm->output_size.w) {
		pr_err("fail to cfg src_w %d, hor_deci %d, des_w %d\n",
			p->sc_full_in_size.w,
			p->sc_deci_val,
			cfg_parm->output_size.w);
		ret = -EINVAL;
	}

	/* check for height */
	if (cfg_parm->input_format == SCALE_YUV422
	 && cfg_parm->output_format == SCALE_YUV420) {
		if (p->sc_full_in_size.h > (4 * cfg_parm->output_size.h)) {
			pr_err("fail to cfg src_h %d >> ver_dec %d > 4 * dst_h %d\n",
				p->sc_full_in_size.h,
				p->sc_deci_val,
				cfg_parm->output_size.h);
			ret = -EINVAL;
		}
	} else {
		if (p->sc_full_in_size.h > (8 * cfg_parm->output_size.h)) {
			pr_err("fail to cfg src_h %d >> ver_dec %d > 8 * dst_h %d\n",
				p->sc_full_in_size.h,
				p->sc_deci_val,
				cfg_parm->output_size.h);
			ret = -EINVAL;
		}
	}

	if (cfg_parm->input_format == SCALE_YUV420
	 && cfg_parm->output_format == SCALE_YUV422) {
		if (p->sc_full_in_size.h < (2 * cfg_parm->output_size.h)) {
			pr_err("fail to cfg src_h %d >> ver_dec %d < 2 * dst_h %d\n",
				p->sc_full_in_size.h,
				p->sc_deci_val,
				cfg_parm->output_size.h);
			ret = -EINVAL;
		}
	} else {
		if (p->sc_full_in_size.h < cfg_parm->output_size.h) {
			pr_err("fail to cfg src_h %d >> ver_dec %d < des_h %d\n",
				p->sc_full_in_size.h,
				p->sc_deci_val,
				cfg_parm->output_size.h);
			ret = -EINVAL;
		}
	}

	return ret;
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

	if (scale_k_check_sc_limit(p)) {
		pr_err("fail to get valid src-des size\n");
		return -EINVAL;
	}

	if (cfg_parm->input_rect.x > SCALE_FRAME_WIDTH_MAX
	 || cfg_parm->input_rect.y > SCALE_FRAME_HEIGHT_MAX
	 || cfg_parm->input_rect.w > SCALE_FRAME_WIDTH_MAX
	 || cfg_parm->input_rect.h > SCALE_FRAME_HEIGHT_MAX) {
		pr_err("fail to get valid input rect %d %d %d %d\n",
			cfg_parm->input_rect.x, cfg_parm->input_rect.y,
			cfg_parm->input_rect.w, cfg_parm->input_rect.h);
		return -EINVAL;
	}

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

int cpp_k_scale_deci_set(void *arg)
{
	int i = 0;
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

	reg_mwr(p, CPP_PATH0_CFG0, CPP_SCALE_DEC_H_MASK, 0 << 4);
	reg_mwr(p, CPP_PATH0_CFG0, CPP_SCALE_DEC_V_MASK, 0 << 6);

	if (unlikely(cfg_parm->input_rect.w >
		(cfg_parm->output_size.w * SCALE_SC_COEFF_MAX *
		(1 << SCALE_DECI_FAC_MAX))
		|| cfg_parm->input_rect.h >
		(cfg_parm->output_size.h * SCALE_SC_COEFF_MAX *
		(1 << SCALE_DECI_FAC_MAX)))) {
		pr_err("fail to get input rect %d %d, output size %d %d\n",
			cfg_parm->input_rect.w, cfg_parm->input_rect.h,
			cfg_parm->output_size.w, cfg_parm->output_size.h);
		return -EINVAL;
	}

	p->sc_full_in_size.w = cfg_parm->input_rect.w;
	p->sc_full_in_size.h = cfg_parm->input_rect.h;

	/* check for decimation */
	if (cfg_parm->input_rect.w
		> (cfg_parm->output_size.w * SCALE_SC_COEFF_MAX)
		|| cfg_parm->input_rect.h
		> (cfg_parm->output_size.h * SCALE_SC_COEFF_MAX)) {

		for (i = 1; i < SCALE_DECI_FAC_MAX; i++) {
			div_factor =
			(unsigned int)(SCALE_SC_COEFF_MAX * (1 << i));
			if ((cfg_parm->input_rect.w
			<= (cfg_parm->output_size.w * div_factor))
			&& (cfg_parm->input_rect.h
			<= (cfg_parm->output_size.h * div_factor))) {
				break;
			}
		}
		deci_val = (1 << i);
		pixel_aligned_num =
			(deci_val >= SCALE_PIXEL_ALIGNED)
			? deci_val : SCALE_PIXEL_ALIGNED;

		p->sc_full_in_size.w = cfg_parm->input_rect.w >> i;
		p->sc_full_in_size.h = cfg_parm->input_rect.h >> i;

		if ((p->sc_full_in_size.w % pixel_aligned_num)
		 || (p->sc_full_in_size.h % pixel_aligned_num)) {
			p->sc_full_in_size.w =
				p->sc_full_in_size.w
			/ pixel_aligned_num
				* pixel_aligned_num;
			p->sc_full_in_size.h =
			p->sc_full_in_size.h
			/ pixel_aligned_num
			* pixel_aligned_num;

			cfg_parm->input_rect.w =
				p->sc_full_in_size.w << i;
			cfg_parm->input_rect.h =
				p->sc_full_in_size.h << i;
		}
		p->sc_deci_val = i;

		reg_mwr(p, CPP_PATH0_CFG0,
			CPP_SCALE_DEC_H_MASK, i << 4);
		reg_mwr(p, CPP_PATH0_CFG0,
			CPP_SCALE_DEC_V_MASK, i << 6);
	}

	pr_debug("sc_input_size %d %d, deci %d input_rect %d %d\n",
		p->sc_full_in_size.w,
		p->sc_full_in_size.h,
		i,
		cfg_parm->input_rect.w,
		cfg_parm->input_rect.h);

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

	if (!(cfg_parm->input_format == SCALE_YUV422
	 || cfg_parm->input_format == SCALE_YUV420)) {
		pr_err("fail to get valid input format %d\n",
			cfg_parm->input_format);
		return -EINVAL;
	}

	reg_mwr(p, CPP_PATH0_CFG0, CPP_SCALE_INPUT_FORMAT,
		(cfg_parm->input_format << 2));

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
	pr_debug("cpp iommu_dst y:0x%lx, uv:0x%lx\n",
			p->iommu_dst.iova[0], p->iommu_dst.iova[1]);

	return 0;
}

int cpp_k_scale_input_endian_set(void *arg)
{
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;
	unsigned int y_endian = 0;
	unsigned int uv_endian = 0;
	int ret = 0;
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;
	cfg_parm = &p->cfg_parm;

	if (cfg_parm->input_endian.y_endian >= SCALE_ENDIAN_MAX
	 || cfg_parm->input_endian.uv_endian >= SCALE_ENDIAN_MAX) {
		pr_err("fail to get valid input endian %d %d\n",
			cfg_parm->input_endian.y_endian,
			cfg_parm->input_endian.uv_endian);
		ret = -1;
		goto exit;
	} else {
		if (cfg_parm->input_endian.y_endian == SCALE_ENDIAN_LITTLE)
			y_endian = 0;

		if (cfg_parm->input_endian.y_endian == SCALE_ENDIAN_LITTLE
		 && cfg_parm->input_endian.uv_endian == SCALE_ENDIAN_HALFBIG)
			uv_endian = 1;

		reg_mwr(p, CPP_AXIM_CHN_SET, (CPP_SCALE_DMA_INPUT_Y_ENDIAN),
			y_endian);
		reg_mwr(p, CPP_AXIM_CHN_SET, (CPP_SCALE_DMA_INPUT_UV_ENDIAN),
			uv_endian << 3);

		pr_debug("input endian y:%d, uv:%d\n", y_endian, uv_endian);
	}

exit:
	return ret;
}

int cpp_k_scale_des_pitch_set(void *arg)
{
	struct scale_drv_private *p = NULL;
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;

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

	reg_mwr(p, CPP_PATH0_CFG2, CPP_SCALE_DES_WIDTH_MASK,
			cfg_parm->output_size.w);

	reg_mwr(p, CPP_PATH0_CFG2, CPP_SCALE_DES_HEIGHT_MASK,
			cfg_parm->output_size.h << 16);

	reg_wr(p, CPP_PATH0_CFG5, 0);

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

	if (!(cfg_parm->output_format == SCALE_YUV422
	 || cfg_parm->output_format == SCALE_YUV420)) {
		pr_err("fail to get valid output format %d\n",
			cfg_parm->output_format);
		return -EINVAL;
	}

	reg_mwr(p, CPP_PATH0_CFG0, CPP_SCALE_OUTPUT_FORMAT,
		(cfg_parm->output_format << 4));

	return 0;
}

int cpp_k_scale_output_endian_set(void *arg)
{
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;
	unsigned int y_endian = 0;
	unsigned int uv_endian = 0;
	int ret = 0;
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
		ret = -1;
		goto exit;
	} else {
		if (cfg_parm->output_endian.y_endian == SCALE_ENDIAN_LITTLE)
			y_endian = 0;

		if (cfg_parm->output_endian.y_endian == SCALE_ENDIAN_LITTLE
		 && cfg_parm->output_endian.uv_endian == SCALE_ENDIAN_HALFBIG)
			uv_endian = 1;

		reg_mwr(p, CPP_AXIM_CHN_SET, (CPP_SCALE_DMA_OUTPUT_Y_ENDIAN),
			y_endian << 4);
		reg_mwr(p, CPP_AXIM_CHN_SET, (CPP_SCALE_DMA_OUTPUT_UV_ENDIAN),
			uv_endian << 7);

		pr_debug("output endian y:%d, uv:%d\n",
			y_endian, uv_endian);
	}
exit:
	return ret;
}

int cpp_k_scale_dev_enable(void *arg)
{
	unsigned long flags = 0;
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;
	cfg_parm = &p->cfg_parm;
	spin_lock_irqsave(p->hw_lock, flags);
	reg_owr(p, CPP_PATH_EB, CPP_SCALE_PATH_EB_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
	return 0;
}

int cpp_k_scale_dev_disable(void *arg)
{
	unsigned long flags = 0;
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;
	struct scale_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;
	cfg_parm = &p->cfg_parm;

	spin_lock_irqsave(p->hw_lock, flags);
	reg_awr(p, CPP_PATH_EB, (~CPP_SCALE_PATH_EB_BIT));
	spin_unlock_irqrestore(p->hw_lock, flags);
	return 0;
}

int cpp_k_scale_reg_trace(void *arg)
{
#ifdef SCALE_DRV_DEBUG
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;
	struct scale_drv_private *p = NULL;
	unsigned long addr = 0;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct scale_drv_private *) arg;
	cfg_parm = &p->cfg_parm;

	pr_info("CPP: Register list\n");
	for (addr = CPP_BASE; addr <= CPP_END; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			reg_rd(p, addr), reg_rd(p, addr + 4),
			reg_rd(p, addr + 8), reg_rd(p, addr + 12));
	}
#endif
	return 0;
}
