/*
 * Copyright(C) 2018-2019 Spreadtrum Communications	Inc.
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
#include <video/sprd_mm.h>

#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "AFM: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_raw_afm_iir_nr_cfg(enum isp_id idx,
	struct isp_dev_rgb_afm_info *rafm_info)
{
	int ret = 0;
	unsigned int val = 0;
	int i = 0;

	DCAM_REG_MWR(idx, ISP_AFM_PARAMETERS,
			BIT_2, rafm_info->iir_eb << 2);

	val = ((rafm_info->iir_g1 & 0xFFF) << 16) |
		(rafm_info->iir_g0 & 0xFFF);
	DCAM_REG_WR(idx, ISP_AFM_IIR_FILTER0, val);

	for (i = 0; i < 10; i += 2) {
		val = ((rafm_info->iir_c[i+1] & 0xFFF) << 16) |
			(rafm_info->iir_c[i] & 0xFFF);
		DCAM_REG_WR(idx, ISP_AFM_IIR_FILTER1 + 4 * (i>>1), val);
	}

	return ret;
}

static int isp_k_raw_afm_enhance_cfg(enum isp_id idx,
	struct isp_dev_rgb_afm_info *rafm_info)
{
	int ret = 0, i = 0;
	unsigned int val = 0;

	val = ((rafm_info->fv1_shift & 0x7) << 12) |
		((rafm_info->fv0_shift & 0x7) << 8) |
		((rafm_info->clip_en1 & 0x1) << 7) |
		((rafm_info->clip_en0 & 0x1) << 6) |
		((rafm_info->center_weight & 0x3) << 4) |
		((rafm_info->denoise_mode & 0x3) << 2) |
		(rafm_info->channel_sel & 0x3);
	DCAM_REG_WR(idx, ISP_AFM_ENHANCE_CTRL, val);

	val = ((rafm_info->fv0_th.max & 0xFFFFF) << 12) |
		(rafm_info->fv0_th.min & 0xFFF);
	DCAM_REG_WR(idx, ISP_AFM_ENHANCE_FV0_THD, val);

	val = ((rafm_info->fv1_th.max & 0xFFFFF) << 12) |
		(rafm_info->fv1_th.min & 0xFFF);
	DCAM_REG_WR(idx, ISP_AFM_ENHANCE_FV1_THD, val);

	for (i = 0; i < 4; i++) {
		val = ((rafm_info->fv1_coeff[i][4] & 0x3F) << 24) |
			((rafm_info->fv1_coeff[i][3] & 0x3F) << 18) |
			((rafm_info->fv1_coeff[i][2] & 0x3F) << 12) |
			((rafm_info->fv1_coeff[i][1] & 0x3F) << 6) |
			(rafm_info->fv1_coeff[i][0] & 0x3F);
		DCAM_REG_WR(idx, ISP_AFM_ENHANCE_FV1_COEFF00 + 8 * i, val);

		val = ((rafm_info->fv1_coeff[i][8] & 0x3F) << 18) |
			((rafm_info->fv1_coeff[i][7] & 0x3F) << 12) |
			((rafm_info->fv1_coeff[i][6] & 0x3F) << 6) |
			(rafm_info->fv1_coeff[i][5] & 0x3F);
		DCAM_REG_WR(idx, ISP_AFM_ENHANCE_FV1_COEFF01 + 8 * i, val);
	}

	return ret;
}

int isp_k_raw_afm_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	struct isp_dev_rgb_afm_info rafm_info;

	memset(&rafm_info, 0x00, sizeof(rafm_info));
	ret = copy_from_user((void *)&rafm_info, param->property_param,
			sizeof(struct isp_dev_rgb_afm_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		ret = -1;
		goto exit;
	}

	DCAM_REG_MWR(idx, ISP_AFM_FRM_CTRL, BIT_0, rafm_info.bypass);
	if (rafm_info.bypass) {
		ret = 0;
		goto exit;
	}

	DCAM_REG_MWR(idx, ISP_AFM_PARAMETERS, 0x3 << 4,
		rafm_info.lum_stat_chn_sel << 4);
	DCAM_REG_MWR(idx, ISP_AFM_PARAMETERS, BIT_0, rafm_info.clk_gate_dis);

	ret = isp_k_raw_afm_iir_nr_cfg(idx, &rafm_info);
	if (ret != 0) {
		pr_err("fail to cfg afm iir nr, ret = %d\n", ret);
		ret = -1;
		goto exit;
	}

	ret = isp_k_raw_afm_enhance_cfg(idx, &rafm_info);
	if (ret != 0) {
		pr_err("fail to cfg afm enhance, ret = %d\n", ret);
		ret = -1;
		goto exit;
	}

exit:
	return ret;
}

static int isp_k_raw_afm_bypass(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int bypass = 0;

	ret = copy_from_user((void *)&bypass,
			param->property_param, sizeof(bypass));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	DCAM_REG_MWR(idx, ISP_AFM_FRM_CTRL, BIT_0, bypass);

	return ret;
}

static int32_t isp_k_raw_afm_win(struct isp_io_param *param, enum isp_id idx)
{
	unsigned int ret = 0;
	struct isp_img_rect win;

	memset(&win, 0, sizeof(win));
	ret = copy_from_user((void *)&win,
		param->property_param, sizeof(win));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	DCAM_REG_WR(idx, ISP_AFM_WIN_RANGE0S,
			(win.y & 0x1FFF) << 16 | (win.x & 0x1FFF));

	DCAM_REG_WR(idx, ISP_AFM_WIN_RANGE0E,
			(win.h & 0x7FF) << 16 | (win.w & 0X7FF));

	return ret;
}

static int isp_k_raw_afm_win_num(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	struct img_offset win_num;

	memset(&win_num, 0x00, sizeof(win_num));
	ret = copy_from_user((void *)&win_num, param->property_param,
				sizeof(win_num));
	if (ret != 0) {
		ret = -1;
		pr_err("fail to copy from user, ret = %d\n", ret);
	}

	DCAM_REG_WR(idx, ISP_AFM_WIN_RANGE1S,
			(win_num.y & 0xF) << 16 | (win_num.x & 0x1F));

	return ret;
}

static int isp_k_raw_afm_mode(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	uint32_t mode = 0;

	ret = copy_from_user((void *)&mode,
			param->property_param, sizeof(mode));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	DCAM_REG_MWR(idx, ISP_AFM_FRM_CTRL, BIT_2,
		mode << 2);

	if (mode)
		DCAM_REG_MWR(idx, ISP_AFM_FRM_CTRL, BIT_3,
			(0x1 << 3));
	else
		DCAM_REG_MWR(idx, ISP_AFM_FRM_CTRL1, BIT_0, 0x1);

	return ret;
}

static int isp_k_raw_afm_skip_num(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int skip_num = 0;

	ret = copy_from_user((void *)&skip_num,
			param->property_param, sizeof(skip_num));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	DCAM_REG_MWR(idx, ISP_AFM_FRM_CTRL, 0xF0, (skip_num & 0xF) << 4);

	return ret;
}

static int32_t isp_k_raw_afm_skip_num_clr(struct isp_io_param *param,
							enum isp_id idx)
{
	int32_t ret = 0;
	unsigned int is_clear = 0;

	ret = copy_from_user((void *)&is_clear,
		param->property_param, sizeof(is_clear));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	DCAM_REG_MWR(idx, ISP_AFM_FRM_CTRL1, BIT_1, is_clear << 1);

	return ret;
}

static int isp_k_raw_afm_crop_eb(struct isp_io_param *param,
	enum isp_id idx)
{
	int ret = 0;
	unsigned int crop_eb = 0;

	ret = copy_from_user((void *)&crop_eb,
		param->property_param, sizeof(crop_eb));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	DCAM_REG_MWR(idx, ISP_AFM_PARAMETERS, BIT_1, crop_eb << 1);

	return ret;
}

static int isp_k_raw_afm_crop_size(struct isp_io_param *param,
	enum isp_id idx)
{
	int ret = 0;
	struct isp_img_rect crop_size;

	memset(&crop_size, 0x00, sizeof(crop_size));
	ret = copy_from_user((void *)&crop_size,
		param->property_param, sizeof(crop_size));
	if (ret != 0) {
		ret = -1;
		pr_err("fail to copy from user, ret = %d\n", ret);
	}

	DCAM_REG_WR(idx, ISP_AFM_CROP_START,
		(crop_size.y & 0x1FFF) << 16 |
		(crop_size.x & 0X1FFF));
	DCAM_REG_WR(idx, ISP_AFM_CROP_SIZE,
		(crop_size.h & 0x1FFF) << 16 |
		(crop_size.w & 0X1FFF));

	return ret;
}

static int isp_k_raw_afm_done_tile_num(struct isp_io_param *param,
	enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct img_offset done_tile_num;

	memset(&done_tile_num, 0x00, sizeof(done_tile_num));
	ret = copy_from_user((void *)&done_tile_num,
			param->property_param, sizeof(done_tile_num));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	val = (done_tile_num.x & 0x1F) << 4 |
		(done_tile_num.y & 0xF);
	DCAM_REG_WR(idx, ISP_AFM_TILE_CNT_OUT, val);

	return ret;
}


int isp_k_raw_afm_pitch(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int pitch = 0;

	ret = copy_from_user((void *)&pitch,
			param->property_param, sizeof(pitch));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	DCAM_REG_MWR(idx, ISP_AFM_FRM_CTRL, 0x1F << 16, pitch << 16);

	return ret;
}

int isp_k_cfg_rgb_afm(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t com_idx)
{
	int ret = 0;
	enum isp_id idx = ISP_ID_0;

	if (!param) {
		pr_err("fail to get param\n");
		return -1;
	}

	if (!param->property_param) {
		pr_err("fail to get property_param\n");
		return -1;
	}

	idx = ISP_GET_ISP_ID(com_idx);
	switch (param->property) {
	case ISP_PRO_RGB_AFM_BYPASS:
		ret = isp_k_raw_afm_bypass(param, idx);
		break;
	case ISP_PRO_RGB_AFM_BLOCK:
		ret = isp_k_raw_afm_block(param, idx);
		break;
	case ISP_PRO_RGB_AFM_WIN:
		ret = isp_k_raw_afm_win(param, idx);
		break;
	case ISP_PRO_RGB_AFM_WIN_NUM:
		ret = isp_k_raw_afm_win_num(param, idx);
		break;
	case ISP_PRO_RGB_AFM_MODE:
		ret = isp_k_raw_afm_mode(param, idx);
		break;
	case ISP_PRO_RGB_AFM_SKIP_NUM:
		ret = isp_k_raw_afm_skip_num(param, idx);
		break;
	case ISP_PRO_RGB_AFM_SKIP_NUM_CLR:
		ret = isp_k_raw_afm_skip_num_clr(param, idx);
		break;
	case ISP_PRO_RGB_AFM_CROP_EB:
		ret = isp_k_raw_afm_crop_eb(param, idx);
		break;
	case ISP_PRO_RGB_AFM_CROP_SIZE:
		ret = isp_k_raw_afm_crop_size(param, idx);
		break;
	case ISP_PRO_RGB_AFM_DONE_TILE_NUM:
		ret = isp_k_raw_afm_done_tile_num(param, idx);
		break;

	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
