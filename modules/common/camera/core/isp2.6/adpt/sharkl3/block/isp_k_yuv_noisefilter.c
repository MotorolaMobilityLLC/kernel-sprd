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

#include <linux/uaccess.h>
#include <sprd_mm.h>

#include "isp_hw.h"
#include "isp_reg.h"
#include "isp_core.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "NOISEFILTER: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_noisefilter_block(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	uint32_t val = 0;
	struct isp_dev_noise_filter_info *nf_info;

	nf_info = &isp_k_param->nf_info;

	ret = copy_from_user((void *)nf_info,
				param->property_param,
				sizeof(struct isp_dev_noise_filter_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return ret;
	}

	if (g_isp_bypass[idx] & (1 << _EISP_YUVNF))
		nf_info->yrandom_bypass = 1;
	if (nf_info->yrandom_bypass)
		return 0;

	pr_debug("yrandom_mode=%d,shape_mode=%d,index=%d\n",
		nf_info->yrandom_mode, nf_info->shape_mode, idx);
	val = ((nf_info->shape_mode & 1) << 1) |
		((nf_info->filter_thr_mode & 0x3) << 2) |
		((nf_info->yrandom_mode & 0x1) << 5);
	ISP_REG_MWR(idx, ISP_YUV_NF_CTRL, 0x2E, val);
	ISP_REG_WR(idx, ISP_YUV_NF_SEED_INIT, 1);

	if (nf_info->shape_mode != 0) {
		isp_k_param->seed0_for_mode1 = nf_info->yrandom_seed[0];
		isp_k_param->yrandom_mode = nf_info->yrandom_mode;
		pr_debug("seed0_for_mode1=%d\n", isp_k_param->seed0_for_mode1);
		cam_block_noisefilter_seeds(isp_k_param->src_w,
			nf_info->yrandom_seed[0], &nf_info->yrandom_seed[1],
			&nf_info->yrandom_seed[2], &nf_info->yrandom_seed[3]);
	}
	ISP_REG_WR(idx, ISP_YUV_NF_SEED0, nf_info->yrandom_seed[0]);
	ISP_REG_WR(idx, ISP_YUV_NF_SEED1, nf_info->yrandom_seed[1]);
	ISP_REG_WR(idx, ISP_YUV_NF_SEED2, nf_info->yrandom_seed[2]);
	ISP_REG_WR(idx, ISP_YUV_NF_SEED3, nf_info->yrandom_seed[3]);

	val = (nf_info->takebit[0]  & 0xF) |
			((nf_info->takebit[1] & 0xF) << 4) |
			((nf_info->takebit[2] & 0xF) << 8) |
			((nf_info->takebit[3] & 0xF) << 12) |
			((nf_info->takebit[4] & 0xF) << 16) |
			((nf_info->takebit[5] & 0xF) << 20) |
			((nf_info->takebit[6] & 0xF) << 24) |
			((nf_info->takebit[7] & 0xF) << 28);
	ISP_REG_WR(idx, ISP_YUV_NF_TB4, val);

	val = ((nf_info->r_shift & 0xF) << 16) |
			(nf_info->r_offset & 0x7FF);
	ISP_REG_WR(idx, ISP_YUV_NF_SF, val);

	ISP_REG_MWR(idx, ISP_YUV_NF_THR, 0xFF, nf_info->filter_thr);

	val = ((nf_info->cv_t[1] & 0x3FF) << 16)
		| (nf_info->cv_t[0] & 0x3FF);
	ISP_REG_WR(idx, ISP_YUV_NF_CV_T12, val);

	val = ((nf_info->cv_r[2] & 0xFF) << 16) |
		((nf_info->cv_r[1] & 0xFF) << 8) |
		(nf_info->cv_r[0] & 0xFF);
	ISP_REG_WR(idx, ISP_YUV_NF_CV_R, val);

	val = ((nf_info->noise_clip.p & 0xFF) << 8)
		| (nf_info->noise_clip.n & 0xFF);
	ISP_REG_WR(idx, ISP_YUV_NF_CLIP, val);

	val = ((nf_info->cv_t[3] & 0x3FF) << 16)
		| (nf_info->cv_t[2] & 0x3FF);
	ISP_REG_WR(idx, ISP_YUV_NF_CV_T34, val);

	return ret;
}

int isp_k_cfg_yuv_noisefilter(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;

	switch (param->property) {
	case ISP_PRO_NOISE_FILTER_BLOCK:
		ret = isp_k_noisefilter_block(param, isp_k_param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
