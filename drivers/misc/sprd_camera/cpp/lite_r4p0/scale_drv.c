/*
 * Copyright (C) 2017-2018 Spreadtrum Communications Inc.
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

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/semaphore.h>
#include <uapi/video/sprd_cpp.h>

#include "cpp_common.h"
#include "cpp_reg.h"
#include "cpp_core.h"
#include "scale_drv.h"

#define PATH0_ADDR_ALIGN                    0x07
#define SCALE_LOWEST_ADDR                   0x800
#define SCALE_ADDR_INVALID(addr) \
	((unsigned long)(addr) < SCALE_LOWEST_ADDR)
#define SCALE_YUV_ADDR_INVALID(y, u, v) \
	(SCALE_ADDR_INVALID(y) && \
	SCALE_ADDR_INVALID(u) && \
	SCALE_ADDR_INVALID(v))

#define SCALE_FRAME_WIDTH_MAX               8192
#define SCALE_FRAME_HEIGHT_MAX              8192
#define SCALE_FRAME_OUT_WIDTH_MAX           768
#define SCALE_SC_COEFF_MAX                  8
#define SCALE_SC_COEFF_MID                  4
#define SCALE_DECI_FAC_MAX                  3
#define SCALE_PIXEL_ALIGNED                 4
#define ALIGNED_DOWN_2(w)                  ((w) & ~(2 - 1))
#define ALIGNED_DOWN_4(w)                  ((w) & ~(4 - 1))
#define ALIGNED_DOWN_8(w)                  ((w) & ~(8 - 1))

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "SCALE_DRV: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static void sprd_scaledrv_stop(
	struct scale_drv_private *p)
{
	unsigned long flags = 0;

	spin_lock_irqsave(p->hw_lock, flags);
	reg_awr(p, CPP_PATH_START, (~CPP_SCALE_START_BIT));
	spin_unlock_irqrestore(p->hw_lock, flags);
}

static void sprd_scaledrv_start(
	struct scale_drv_private *p)
{
	unsigned long flags = 0;

	spin_lock_irqsave(p->hw_lock, flags);
	reg_owr(p, CPP_PATH_START, CPP_SCALE_START_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
}

static int sprd_scaledrv_get_params(
	struct scale_drv_private *p,
	struct sprd_cpp_scale_capability *scale_param)
{
	struct sprd_cpp_scale_cfg_parm *scale_cfg_parm = NULL;

	if (!p || !scale_param) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}
	scale_cfg_parm = &p->cfg_parm;

	scale_param->src_size = scale_cfg_parm->input_size;
	scale_param->rect_size = scale_cfg_parm->input_rect;
	scale_param->src_format = scale_cfg_parm->input_format;
	scale_param->dst_size = scale_cfg_parm->output_size;
	scale_param->dst_format = scale_cfg_parm->output_format;

	return 0;
}

static int sprd_scaledrv_param_check(
	struct sprd_cpp_scale_capability *scale_param)
{
	int ret = 0;

	if ((scale_param->src_format != SCALE_YUV420 &&
		scale_param->src_format != SCALE_YUV422) ||
		(scale_param->dst_format != SCALE_YUV420 &&
		scale_param->dst_format != SCALE_YUV422)) {
		pr_info("get invalid format src %d dst %d\n",
			scale_param->src_format, scale_param->dst_format);
		return -1;
	}

	if (scale_param->rect_size.w < scale_param->dst_size.w ||
		scale_param->rect_size.h < scale_param->dst_size.h) {
		pr_info("get invalid zoom ratio dst %d src %d\n",
			scale_param->dst_size.w, scale_param->rect_size.w);
		return -1;
	}
	if (scale_param->src_format == scale_param->dst_format) {
		if (scale_param->rect_size.w >
			(scale_param->dst_size.w * 64)
			|| scale_param->rect_size.h >
			(scale_param->dst_size.h * 64)) {
			pr_info("get invalid src %d %d, dst%d %d\n",
				scale_param->rect_size.w,
				scale_param->rect_size.h,
				scale_param->dst_size.w,
				scale_param->dst_size.h);
			return -1;
		}
	} else {
		if (scale_param->rect_size.w >
			(scale_param->dst_size.w * 32)
			|| scale_param->rect_size.h >
			(scale_param->dst_size.h * 32)) {
			pr_info("get invalid src %d %d, dst%d %d\n",
				scale_param->rect_size.w,
				scale_param->rect_size.h,
				scale_param->dst_size.w,
				scale_param->dst_size.h);
			return -1;
		}
	}

	if (scale_param->rect_size.w > SCALE_FRAME_WIDTH_MAX ||
		scale_param->rect_size.h > SCALE_FRAME_HEIGHT_MAX) {
		pr_info("get invalid input size %d %d\n",
			scale_param->rect_size.w, scale_param->rect_size.h);
		return -1;
	}
	if (scale_param->src_size.w % 8 != 0) {
		pr_info("get invalid org src pitch %d\n",
			scale_param->rect_size.w);
		return -1;
	}

	if (scale_param->src_format == SCALE_YUV420) {
		if (scale_param->rect_size.h % 2 != 0) {
			pr_info("get insrc height align 2: %d\n",
				scale_param->rect_size.y);
			return -1;
		}
		if (scale_param->rect_size.y % 2 != 0) {
			pr_info("get invalid src offset y align 2: %d\n",
				scale_param->rect_size.y);
			return -1;
		}
	}
	if (scale_param->rect_size.w % 4 != 0) {
		pr_info("get invalid src width align 4: %d\n",
			scale_param->rect_size.y);
		return -1;
	}
	if (scale_param->rect_size.x % 2 != 0) {
		pr_info("get invalid src offset x align 2: %d\n",
			scale_param->rect_size.x);
		return -1;
	}

	if (scale_param->dst_size.w > SCALE_FRAME_OUT_WIDTH_MAX
		|| scale_param->dst_size.h > SCALE_FRAME_HEIGHT_MAX) {
		pr_info("get invalid output size %d %d over 768\n",
			scale_param->dst_size.w, scale_param->dst_size.h);
		return -1;
	}
	if (scale_param->dst_size.w % 8 != 0) {
		pr_info("get invalid dst width %d\n",
			scale_param->dst_size.w);
		return -1;
	}

	if ((scale_param->dst_format == SCALE_YUV420)
		&& (scale_param->dst_size.h % 2 != 0)) {
		pr_info("get invalid scale output height %d\n",
			scale_param->dst_size.h);
		return -1;
	}

	return ret;
}


static void sprd_scaledrv_src_pitch_set(
	struct scale_drv_private *p)
{
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return;
	}
	cfg_parm = &p->cfg_parm;

	reg_mwr(p, CPP_PATH0_CFG3, CPP_SCALE_SRC_PITCH_MASK,
		cfg_parm->input_size.w);
}

static int sprd_scaledrv_input_rect_set(
	struct scale_drv_private *p)
{
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}
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

static int sprd_scaledrv_sc_size_calc(
	struct scale_drv_private *p)
{
	int i = 0;
	unsigned int deci_tmp_w = 0;
	unsigned int deci_tmp_h = 0;
	unsigned int div_factor = 1;
	unsigned int deci_val = 0;
	unsigned int pixel_aligned_num = 0;
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
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

	p->sc_input_size.w = cfg_parm->input_rect.w >> deci_tmp_w;

	if (pixel_aligned_num > 0 &&
		(p->sc_input_size.w % pixel_aligned_num)) {
		p->sc_input_size.w =
			p->sc_input_size.w / pixel_aligned_num
			* pixel_aligned_num;

		cfg_parm->input_rect.w = p->sc_input_size.w << deci_tmp_w;
	}
	p->sc_deci_val_w = deci_tmp_w;

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

	p->sc_input_size.h = cfg_parm->input_rect.h >> deci_tmp_h;

	if (pixel_aligned_num > 0 &&
		(p->sc_input_size.w % pixel_aligned_num)) {
		p->sc_input_size.h =
			p->sc_input_size.h / pixel_aligned_num
			* pixel_aligned_num;

		cfg_parm->input_rect.h = p->sc_input_size.h << deci_tmp_h;
	}
	p->sc_deci_val_h = deci_tmp_h;

	reg_mwr(p, CPP_PATH0_CFG0,
		CPP_SCALE_DEC_H_MASK, deci_tmp_w << 4);
	reg_mwr(p, CPP_PATH0_CFG0,
		CPP_SCALE_DEC_V_MASK, deci_tmp_h << 6);

	pr_debug("sc_input_size %d %d, deci(w,h) %d, %d input_rect %d %d\n",
		p->sc_input_size.w,
		p->sc_input_size.h,
		deci_tmp_w, deci_tmp_h,
		cfg_parm->input_rect.w,
		cfg_parm->input_rect.h);

	return 0;
}

static int sprd_scaledrv_input_size_set(
	struct scale_drv_private *p)
{
	int ret = 0;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	ret = sprd_scaledrv_sc_size_calc(p);
	if (ret) {
		pr_err("fail to configure scaler\n");
		return ret;
	}
	ret = sprd_scaledrv_input_rect_set(p);
	if (ret) {
		pr_err("fail to set input rect size\n");
		return ret;
	}

	return ret;
}

static int sprd_scaledrv_input_format_set(
	struct scale_drv_private *p)
{
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	cfg_parm = &p->cfg_parm;

	reg_mwr(p, CPP_PATH0_CFG0, CPP_SCALE_INPUT_FORMAT,
		(cfg_parm->input_format << 2));

	return 0;
}

static int sprd_scaledrv_input_endian_set(
	struct scale_drv_private *p)
{
	unsigned int y_endian = 0;
	unsigned int uv_endian = 0;
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
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

static int sprd_scaledrv_des_pitch_set(
	struct scale_drv_private *p)
{
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	cfg_parm = &p->cfg_parm;

	reg_mwr(p, CPP_PATH0_CFG3, CPP_SCALE_DES_PITCH_MASK,
		cfg_parm->output_size.w << 16);

	return 0;
}

static int sprd_scaledrv_output_size_set(
	struct scale_drv_private *p)
{
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	cfg_parm = &p->cfg_parm;

	reg_mwr(p, CPP_PATH0_CFG2, CPP_SCALE_DES_HEIGHT_MASK,
			cfg_parm->output_size.h << 16);
	reg_mwr(p, CPP_PATH0_CFG2, CPP_SCALE_DES_WIDTH_MASK,
			cfg_parm->output_size.w);

	reg_wr(p, CPP_PATH0_CFG5, 0);

	return 0;
}

static int sprd_scaledrv_output_format_set(
	struct scale_drv_private *p)
{
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	cfg_parm = &p->cfg_parm;

	reg_mwr(p, CPP_PATH0_CFG0, CPP_SCALE_OUTPUT_FORMAT,
		(cfg_parm->output_format << 4));

	return 0;
}

static int sprd_scaledrv_output_endian_set(
	struct scale_drv_private *p)
{
	unsigned int y_endian = 0;
	unsigned int uv_endian = 0;
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
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

static int sprd_scaledrv_addr_set(
	struct scale_drv_private *p)
{
	int ret = 0;
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	cfg_parm = &p->cfg_parm;

	if (cfg_parm->input_addr.mfd[0] == 0 ||
		cfg_parm->input_addr.mfd[1] == 0 ||
		cfg_parm->output_addr.mfd[0] == 0 ||
		cfg_parm->output_addr.mfd[1] == 0) {
		pr_err("fail to get valid in or out mfd\n");
		return -1;
	}
	memcpy(p->iommu_src.mfd, cfg_parm->input_addr.mfd,
		sizeof(cfg_parm->input_addr.mfd));
	memcpy(p->iommu_dst.mfd, cfg_parm->output_addr.mfd,
		sizeof(cfg_parm->output_addr.mfd));

	ret = sprd_cpp_core_get_sg_table(&p->iommu_src);
	if (ret) {
		pr_err("fail to get cpp src sg table\n");
		return -1;
	}
	p->iommu_src.offset[0] = cfg_parm->input_addr.y;
	p->iommu_src.offset[1] = cfg_parm->input_addr.u;
	p->iommu_src.offset[2] = cfg_parm->input_addr.v;
	ret = sprd_cpp_core_get_addr(&p->iommu_src);
	if (ret) {
		pr_err("fail to get cpp src addr\n");
		return -1;
	}

	ret = sprd_cpp_core_get_sg_table(&p->iommu_dst);
	if (ret) {
		pr_err("fail to get cpp dst sg table\n");
		sprd_cpp_core_free_addr(&p->iommu_src);
		return ret;
	}
	p->iommu_dst.offset[0] = cfg_parm->output_addr.y;
	p->iommu_dst.offset[1] = cfg_parm->output_addr.u;
	p->iommu_dst.offset[2] = cfg_parm->output_addr.v;
	ret = sprd_cpp_core_get_addr(&p->iommu_dst);
	if (ret) {
		pr_err("fail to get cpp dst addr\n");
		sprd_cpp_core_free_addr(&p->iommu_src);
		return ret;
	}

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

	return ret;
}

static void sprd_scaledrv_dev_enable(
	struct scale_drv_private *p)
{
	unsigned long flags;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return;
	}
	spin_lock_irqsave(p->hw_lock, flags);
	reg_owr(p, CPP_PATH_EB, CPP_SCALE_PATH_EB_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
}

static void sprd_scaledrv_dev_disable(struct scale_drv_private *p)
{
	unsigned long flags;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return;
	}
	spin_lock_irqsave(p->hw_lock, flags);
	reg_awr(p, CPP_PATH_EB, (~CPP_SCALE_PATH_EB_BIT));
	spin_unlock_irqrestore(p->hw_lock, flags);
}

void sprd_scale_drv_max_size_get(unsigned int *max_width,
	unsigned int *max_height)
{
	*max_width = SCALE_FRAME_WIDTH_MAX;
	*max_height = SCALE_FRAME_HEIGHT_MAX;
}

int sprd_scale_drv_start(struct sprd_cpp_scale_cfg_parm *parm,
			struct scale_drv_private *p)
{
	int ret = 0;
	struct sprd_cpp_scale_capability scale_cap_aram;

	if (!parm || !p) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	memset(&scale_cap_aram, 0x00, sizeof(scale_cap_aram));
	memset(&p->sc_input_size, 0x00, sizeof(p->sc_input_size));
	p->sc_deci_val_w = 0;
	p->sc_deci_val_h = 0;

	memcpy((void *)&p->cfg_parm, (void *)parm,
		sizeof(struct sprd_cpp_scale_cfg_parm));

	sprd_scaledrv_stop(p);
	sprd_scaledrv_dev_enable(p);
	sprd_scaledrv_get_params(p, &scale_cap_aram);
	ret = sprd_scaledrv_param_check(&scale_cap_aram);
	if (ret) {
		pr_info("Please use sofeware to scale\n");
		return -1;
	}
	sprd_scaledrv_src_pitch_set(p);
	sprd_scaledrv_des_pitch_set(p);
	ret = sprd_scaledrv_input_size_set(p);
	if (ret) {
		pr_err("fail to set input size\n");
		return -1;
	}
	sprd_scaledrv_output_size_set(p);

	ret = sprd_scaledrv_input_format_set(p);
	if (ret) {
		pr_err("fail to get valid input format\n");
		return -1;
	}
	ret = sprd_scaledrv_output_format_set(p);
	if (ret) {
		pr_err("fail to get valid output format\n");
		return -1;
	}
	ret = sprd_scaledrv_input_endian_set(p);
	if (ret) {
		pr_err("fail to get valid input endian\n");
		return -1;
	}
	ret = sprd_scaledrv_output_endian_set(p);
	if (ret) {
		pr_err("fail to get valid output endian\n");
		return -1;
	}
	ret = sprd_scaledrv_addr_set(p);
	if (ret) {
		pr_err("fail to get valid output format\n");
		return -1;
	}

	pr_info("in_size %d %d in_rect %d %d %d %d out_size %d %d\n",
		p->cfg_parm.input_size.w, p->cfg_parm.input_size.h,
		p->cfg_parm.input_rect.x, p->cfg_parm.input_rect.y,
		p->cfg_parm.input_rect.w, p->cfg_parm.input_rect.h,
		p->cfg_parm.output_size.w, p->cfg_parm.output_size.h);
	pr_info("w_deci %d  h_deci %d\n",
		p->sc_deci_val_w, p->sc_deci_val_h);

	sprd_scaledrv_start(p);

	return ret;
}

void sprd_scale_drv_stop(struct scale_drv_private *p)
{
	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return;
	}
	sprd_scaledrv_stop(p);
	sprd_scaledrv_dev_disable(p);
	sprd_cpp_core_free_addr(&p->iommu_src);
	sprd_cpp_core_free_addr(&p->iommu_dst);
}

int sprd_scale_drv_capability_get(
	struct sprd_cpp_scale_capability *scale_param)
{
	int ret = 0;

	scale_param->is_supported = 0;

	ret = sprd_scaledrv_param_check(scale_param);
	if (ret) {
		pr_info("Please use sofeware to scale\n");
		return -1;
	}

	scale_param->is_supported = 1;

	return ret;
}
