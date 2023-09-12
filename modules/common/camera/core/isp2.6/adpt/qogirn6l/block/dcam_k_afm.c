/*
 * Copyright (C) 2020-2021 UNISOC Communications Inc.
 *
 * This	software is licensed under the terms of	the GNU	General	Public
 * License version 2, as published by the Free Software	Foundation, and
 * may be copied, distributed, and modified under those	terms.
 *
 * This	program	is distributed in the hope that	it will	be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 */

#include <linux/uaccess.h>
#include <sprd_mm.h>

#include "isp_hw.h"
#include "dcam_reg.h"
#include "dcam_path.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "AFM: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int dcam_k_afm_block(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	int i = 0;
	uint32_t val = 0;
	struct dcam_dev_afm_info *p = NULL;

	if (param == NULL)
		return -1;

	idx = param->idx;
	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;
	p = &(param->afm.af_param);

	val = (p->afm_iir_enable & 0x1) |
		((p->afm_lum_stat_chn_sel & 0x3) << 1) |
		((p->afm_done_tile_num_y & 0xF) << 8) |
		((p->afm_done_tile_num_x & 0x1F) << 12);
	DCAM_REG_MWR(idx, DCAM_AFM_PARAMETERS, 0x1FF07, val);

	/* IIR cfg */
	val = ((p->afm_iir_g1 & 0xFFF) << 16) |
		(p->afm_iir_g0 & 0xFFF);
	DCAM_REG_WR(idx, DCAM_AFM_IIR_FILTER0, val);

	for (i = 0; i < 10; i += 2) {
		val = ((p->afm_iir_c[i + 1] & 0xFFF) << 16) |
			(p->afm_iir_c[i] & 0xFFF);
		DCAM_REG_WR(idx, DCAM_AFM_IIR_FILTER1 + 4 * (i >> 1), val);
	}

	/* enhance control cfg */
	val = (p->afm_channel_sel & 0x3) |
		((p->afm_denoise_mode & 0x3) << 2) |
		((p->afm_center_weight & 0x3) << 4) |
		((p->afm_clip_en0 & 0x1) << 6) |
		((p->afm_clip_en1 & 0x1) << 7) |
		((p->afm_fv0_shift & 0x7) << 8) |
		((p->afm_fv1_shift & 0x7) << 12);
	DCAM_REG_WR(idx, DCAM_AFM_ENHANCE_CTRL, val);

	val = (p->afm_fv0_th.min & 0xFFF) |
		((p->afm_fv0_th.max & 0xFFFFF) << 12);
	DCAM_REG_WR(idx, DCAM_AFM_ENHANCE_FV0_THD, val);

	val = (p->afm_fv1_th.min & 0xFFF) |
		((p->afm_fv1_th.max & 0xFFFFF) << 12);
	DCAM_REG_WR(idx, DCAM_AFM_ENHANCE_FV1_THD, val);

	for (i = 0; i < 4; i++) {
		val = ((p->afm_fv1_coeff[i][4] & 0x3F) << 24) |
			((p->afm_fv1_coeff[i][3] & 0x3F) << 18) |
			((p->afm_fv1_coeff[i][2] & 0x3F) << 12) |
			((p->afm_fv1_coeff[i][1] & 0x3F) << 6) |
			(p->afm_fv1_coeff[i][0] & 0x3F);
		DCAM_REG_WR(idx, DCAM_AFM_ENHANCE_FV1_COEFF00 + 8 * i, val);

		val = ((p->afm_fv1_coeff[i][8] & 0x3F) << 18) |
			((p->afm_fv1_coeff[i][7] & 0x3F) << 12) |
			((p->afm_fv1_coeff[i][6] & 0x3F) << 6) |
			(p->afm_fv1_coeff[i][5] & 0x3F);
		DCAM_REG_WR(idx, DCAM_AFM_ENHANCE_FV1_COEFF01 + 8 * i, val);
	}

	val = (p->afm_enhanced_lum.afm_max_lum_num_th & 0xff) << 16 |
		(p->afm_enhanced_lum.afm_max_lum_th & 0xffff);
	DCAM_REG_WR(idx, DCAM_AFM_MAX_LUM, val);

	DCAM_REG_MWR(idx, DCAM_BUF_CTRL, BIT_5, BIT_5);
	DCAM_REG_MWR(idx, DCAM_AFM_GAMC_CTRL, BIT_0, !p->afm_enhanced_lum.afm_gamma_en);
	if (p->afm_enhanced_lum.afm_gamma_en) {
		for (i = 0; i < RGB_AFM_GAMMA_POINT_NUM - 1; i++) {
			val = ((p->afm_enhanced_lum.afm_gamma_curve[i+1] & 0x3FF) | (p->afm_enhanced_lum.afm_gamma_curve[i] & 0x3FF) << 10);
			DCAM_REG_WR(idx, DCAM_AFM_FGAMMA_TABLE + i * 4, val);
		}
		val = DCAM_REG_RD(idx, DCAM_BUF_CTRL) & BIT_21;
		DCAM_REG_MWR(idx, DCAM_BUF_CTRL, BIT_21, ~val);
	}

	DCAM_REG_MWR(idx, DCAM_AFM_ENHANCE_CTRL, BIT_16, p->afm_enhanced_lum.afm_hist_en << 16);

	DCAM_REG_MWR(idx, DCAM_AFM_BIN_PARAM, BIT_4 | BIT_0,
		(p->afm_enhanced_lum.afm_scale_hx & 1) | ((p->afm_enhanced_lum.afm_scale_vx & 1) << 4));
	DCAM_REG_MWR(idx, DCAM_AFM_BIN_CTRL, BIT_0, !p->afm_enhanced_lum.afm_scale_en);

	return ret;
}

int dcam_k_afm_bypass(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;

	if (param == NULL)
		return -1;

	idx = param->idx;
	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;
	if (g_dcam_bypass[idx] & (1 << _E_AFM))
		param->afm.bypass = 1;
	DCAM_REG_MWR(idx, DCAM_AFM_FRM_CTRL, BIT_0, param->afm.bypass);

	return ret;
}

int dcam_k_afm_win(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	struct isp_img_rect *p;

	if (param == NULL)
		return -1;

	idx = param->idx;
	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;
	p = &(param->afm.win);
	DCAM_REG_WR(idx, DCAM_AFM_WIN_RANGE0S,
			(p->y & 0x1FFF) << 16 | (p->x & 0x1FFF));

	DCAM_REG_WR(idx, DCAM_AFM_WIN_RANGE0E,
			(p->h & 0x7FF) << 16 | (p->w & 0X7FF));

	return ret;
}

int dcam_k_afm_win_num(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	struct isp_img_size *p;

	if (param == NULL)
		return -1;

	idx = param->idx;
	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;
	p = &(param->afm.win_num);

	DCAM_REG_WR(idx, DCAM_AFM_WIN_RANGE1S,
			(p->height & 0xF) << 16 | (p->width & 0x1F));

	return ret;
}

int dcam_k_afm_mode(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	uint32_t mode = 0;

	if (param == NULL)
		return -1;

	idx = param->idx;
	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;
	mode = param->afm.mode;
	DCAM_REG_MWR(idx, DCAM_AFM_FRM_CTRL,
		BIT_2, mode << 2);

	/* 0 - single mode , trigger afm_sgl_start */
	/* 1 - multi mode, trigger mul mode enable */
	if (mode == 0)
		DCAM_REG_MWR(idx, DCAM_AFM_FRM_CTRL1, BIT_0, 1);
	else
		DCAM_REG_MWR(idx, DCAM_AFM_FRM_CTRL, BIT_3, 1 << 3);

	return ret;
}

int dcam_k_afm_skipnum(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	uint32_t skip_num = 0;

	if (param == NULL)
		return -1;

	idx = param->idx;
	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;
	if (param->is_high_fps)
		param->afm.skip_num = 0;

	skip_num = param->afm.skip_num;

	DCAM_REG_MWR(idx, DCAM_AFM_FRM_CTRL, 0xFF0, (skip_num & 0xFF) << 4);

	/* afm_skip_num_clr */
	DCAM_REG_MWR(idx, DCAM_AFM_FRM_CTRL1, BIT_1, 1 << 1);

	dcam_path_skip_num_set(param->dev, DCAM_PATH_AFM, param->afm.skip_num);

	pr_debug("skip num %d\n", param->afm.skip_num);

	return ret;
}

int dcam_k_afm_crop_eb(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	uint32_t crop_eb = 0;

	if (param == NULL)
		return -1;

	idx = param->idx;
	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;
	crop_eb = param->afm.crop_eb;
	DCAM_REG_MWR(idx, DCAM_CROP3_CTRL, BIT_0, !crop_eb);

	return ret;
}

int dcam_k_afm_crop_size(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	struct isp_img_rect crop_size;

	if (param == NULL)
		return -1;

	idx = param->idx;
	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;
	crop_size = param->afm.crop_size;

	DCAM_REG_WR(idx, DCAM_CROP3_START,
		(crop_size.y & 0xFFFF) << 16 |
		(crop_size.x & 0XFFFF));
	DCAM_REG_WR(idx, DCAM_CROP3_SIZE,
		(crop_size.h & 0xFFFF) << 16 |
		(crop_size.w & 0XFFFF));

	return ret;
}

int dcam_k_afm_done_tilenum(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	uint32_t val = 0;
	struct isp_img_size done_tile_num;

	if (param == NULL)
		return -1;

	idx = param->idx;
	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;
	done_tile_num = param->afm.done_tile_num;

	val = ((done_tile_num.width & 0x1F) << 12) |
		((done_tile_num.height & 0x0F) << 8);
	DCAM_REG_MWR(idx, DCAM_AFM_PARAMETERS, 0x1FF00, val);

	return ret;
}

int dcam_k_cfg_afm(struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	void *pcpy;
	int size;
	FUNC_DCAM_PARAM sub_func = NULL;

	switch (param->property) {
	case DCAM_PRO_AFM_BYPASS:
		pcpy = (void *)&(p->afm.bypass);
		size = sizeof(p->afm.bypass);
		sub_func = dcam_k_afm_bypass;
		break;
	case DCAM_PRO_AFM_BLOCK:
		pcpy = (void *)&(p->afm.af_param);
		size = sizeof(p->afm.af_param);
		sub_func = dcam_k_afm_block;
		break;
	case DCAM_PRO_AFM_WIN:
		pcpy = (void *)&(p->afm.win);
		size = sizeof(p->afm.win);
		sub_func = dcam_k_afm_win;
		break;
	case DCAM_PRO_AFM_WIN_NUM:
		pcpy = (void *)&(p->afm.win_num);
		size = sizeof(p->afm.win_num);
		sub_func = dcam_k_afm_win_num;
		break;
	case DCAM_PRO_AFM_MODE:
		pcpy = (void *)&(p->afm.mode);
		size = sizeof(p->afm.mode);
		sub_func = dcam_k_afm_mode;
		break;
	case DCAM_PRO_AFM_SKIPNUM:
		pcpy = (void *)&(p->afm.skip_num);
		size = sizeof(p->afm.skip_num);
		sub_func = dcam_k_afm_skipnum;
		break;
	case DCAM_PRO_AFM_CROP_EB:
		pcpy = (void *)&(p->afm.crop_eb);
		size = sizeof(p->afm.crop_eb);
		sub_func = dcam_k_afm_crop_eb;
		break;
	case DCAM_PRO_AFM_CROP_SIZE:
		pcpy = (void *)&(p->afm.crop_size);
		size = sizeof(p->afm.crop_size);
		sub_func = dcam_k_afm_crop_size;
		break;
	case DCAM_PRO_AFM_DONE_TILENUM:
		pcpy = (void *)&(p->afm.done_tile_num);
		size = sizeof(p->afm.done_tile_num);
		sub_func = dcam_k_afm_done_tilenum;
		break;
	default:
		pr_err("fail to support property %d\n",
			param->property);
		return -EINVAL;
	}
	if (p->offline == 0) {
		ret = copy_from_user(pcpy, param->property_param, size);
		if (ret) {
			pr_err("fail to copy from user ret=0x%x\n",
				(unsigned int)ret);
			return -EPERM;
		}
		if (p->idx == DCAM_HW_CONTEXT_MAX || param->scene_id == PM_SCENE_CAP)
			return 0;
		ret = sub_func(p);
	} else {
		mutex_lock(&p->param_lock);
		ret = copy_from_user(pcpy, param->property_param, size);
		if (ret) {
			mutex_unlock(&p->param_lock);
			pr_err("fail to copy from user ret=0x%x\n",
				(unsigned int)ret);
			return -EPERM;
		}
		mutex_unlock(&p->param_lock);
	}

	return ret;
}
