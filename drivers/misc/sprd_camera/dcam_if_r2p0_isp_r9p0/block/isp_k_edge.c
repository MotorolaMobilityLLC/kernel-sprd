/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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
#include <video/sprd_mm.h>

#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "EDGE: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_edge_block(struct isp_io_param *param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_edge_info_v2 edge_info;
	unsigned int val = 0;
	unsigned int i   = 0;

	memset(&edge_info, 0x00, sizeof(edge_info));
	ret = copy_from_user((void *)&edge_info,
		param->property_param, sizeof(edge_info));
	if (ret != 0) {
		pr_err("fail to check param ret = 0x%x\n",
			(unsigned int)ret);
		return -1;
	}
	val = ((edge_info.ee_old_gradient_en & 0x1) << 2)|
			((edge_info.ee_new_pyramid_en & 0x1) << 1)|
			(edge_info.ee_bypass & 0x1);
	ISP_REG_MWR(idx, ISP_EE_PARAM, 0x7, val);
	if (edge_info.ee_bypass)
		return 0;

	val = ((edge_info.ee_mode & 0x1)  << 28) |
			((edge_info.ee_str_d.p & 0x7F) << 21) |
			((edge_info.ee_str_d.n & 0x7F) << 14) |
			((edge_info.ee_edge_smooth_mode & 0x3)  << 2) |
			(edge_info.ee_flat_smooth_mode & 0x3);
	ISP_REG_WR(idx, ISP_EE_CFG0, val);

	val = ((edge_info.ee_edge_thr_d.p & 0xFF) << 24) |
			((edge_info.ee_edge_thr_d.n & 0xFF) << 16) |
			((edge_info.ee_incr_d.p & 0xFF) << 8) |
			(edge_info.ee_incr_d.n & 0xFF);
	ISP_REG_WR(idx, ISP_EE_CFG1, val);

	val = ((edge_info.ee_corner_cor	   & 0x1)  << 28) |
			((edge_info.ee_corner_th.p   & 0x1F) << 23) |
			((edge_info.ee_corner_th.n   & 0x1F) << 18) |
			((edge_info.ee_corner_gain.p & 0x7F) << 11) |
			((edge_info.ee_corner_gain.n & 0x7F) << 4) |
			((edge_info.ee_corner_sm.p   & 0x3)  << 2) |
			(edge_info.ee_corner_sm.n   & 0x3);
	ISP_REG_WR(idx, ISP_EE_CFG2, val);

	val = ((edge_info.ee_cv_t[2] & 0x3FF) << 20) |
			((edge_info.ee_cv_t[1] & 0x3FF) << 10) |
			(edge_info.ee_cv_t[0] & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_ADP_CFG0, val);

	val = ((edge_info.ee_cv_clip.p & 0xFF) << 24) |
			((edge_info.ee_cv_clip.n & 0xFF) << 16) |
			(edge_info.ee_cv_t[3] & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_ADP_CFG1, val);

	val = ((edge_info.ee_cv_r[2] & 0xFF) << 16) |
			((edge_info.ee_cv_r[1] & 0xFF) << 8) |
			(edge_info.ee_cv_r[0] & 0xFF);
	ISP_REG_WR(idx, ISP_EE_ADP_CFG2, val);

	val = ((edge_info.ee_ipd_smooth_mode.n & 0x7) << 14) |
			((edge_info.ee_ipd_smooth_mode.p & 0x7) << 11) |
			((edge_info.ee_ipd_smooth_en & 0x1) << 10) |
			((edge_info.ee_ipd_less_thr.n & 0xF) << 6) |
			((edge_info.ee_ipd_less_thr.p & 0xF) << 2) |
			((edge_info.ee_ipd_mask_mode & 0x1) << 1) |
			(edge_info.ee_ipd_en & 0x1);
	ISP_REG_WR(idx, ISP_EE_IPD_CFG0, val);

	val = ((edge_info.ee_ipd_more_thr.n   & 0xF)  << 28) |
			((edge_info.ee_ipd_more_thr.p   & 0xF)  << 24) |
			((edge_info.ee_ipd_eq_thr.n & 0xF)  << 20) |
			((edge_info.ee_ipd_eq_thr.p & 0xF)  << 16) |
			((edge_info.ee_ipd_flat_thr.n & 0xFF) << 8) |
			(edge_info.ee_ipd_flat_thr.p & 0xFF);
	ISP_REG_WR(idx, ISP_EE_IPD_CFG1, val);

	val = ((edge_info.ee_ipd_smooth_edge_diff.n & 0xFF) << 24) |
			((edge_info.ee_ipd_smooth_edge_diff.p & 0xFF) << 16) |
			((edge_info.ee_ipd_smooth_edge_thr.n & 0xFF) << 8) |
			(edge_info.ee_ipd_smooth_edge_thr.p & 0xFF);
	ISP_REG_WR(idx, ISP_EE_IPD_CFG2, val);

	val = ((edge_info.ee_weight_diag2hv & 0x1F) << 27) |
			((edge_info.ee_gradient_computation_type & 0x1) << 26) |
			((edge_info.ee_weight_hv2diag	& 0x1F) << 21) |
			((edge_info.ee_ratio_diag_3 & 0x7F) << 14) |
			((edge_info.ee_ratio_hv_5 & 0x7F) << 7) |
			(edge_info.ee_ratio_hv_3 & 0x7F);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG0, val);

	for (i = 0; i < 2; i++) {
		val = ((edge_info.ee_gain_hv_t[i][2] & 0x3FF) << 20) |
				((edge_info.ee_gain_hv_t[i][1] & 0x3FF) << 10) |
				(edge_info.ee_gain_hv_t[i][0] & 0x3FF);
		ISP_REG_WR(idx, ISP_EE_LUM_CFG1 + 8 * i, val);
	}

	val = ((edge_info.ee_ratio_diag_5 & 0x7F)  << 25) |
			((edge_info.ee_gain_hv_r[0][2] & 0x1F)  << 20) |
			((edge_info.ee_gain_hv_r[0][1] & 0x1F)  << 15) |
			((edge_info.ee_gain_hv_r[0][0] & 0x1F) << 10) |
			(edge_info.ee_gain_hv_t[0][3] & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG2, val);

	val = ((edge_info.ee_gain_hv_r[1][2] & 0x1F)  << 20) |
			((edge_info.ee_gain_hv_r[1][1] & 0x1F)  << 15) |
			((edge_info.ee_gain_hv_r[1][0] & 0x1F) << 10) |
			(edge_info.ee_gain_hv_t[1][3] & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG4, val);

	for (i = 0; i < 2; i++) {
		val = ((edge_info.ee_gain_diag_t[i][2] & 0x3FF) << 20) |
			((edge_info.ee_gain_diag_t[i][1] & 0x3FF) << 10) |
			(edge_info.ee_gain_diag_t[i][0] & 0x3FF);
		ISP_REG_WR(idx, ISP_EE_LUM_CFG5 + 8*i, val);

		val = ((edge_info.ee_gain_diag_r[i][2] & 0x1F) << 20) |
			((edge_info.ee_gain_diag_r[i][1] & 0x1F) << 15) |
			((edge_info.ee_gain_diag_r[i][0] & 0x1F) << 10) |
			(edge_info.ee_gain_diag_t[i][3] & 0x3FF);
		ISP_REG_WR(idx, ISP_EE_LUM_CFG6 + 8 * i, val);
	}

	val = ((edge_info.ee_lum_t[3] & 0xFF) << 24) |
		((edge_info.ee_lum_t[2] & 0xFF) << 16) |
		((edge_info.ee_lum_t[1] & 0xFF) << 8) |
		(edge_info.ee_lum_t[0] & 0xFF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG9, val);

	val = ((edge_info.ee_lum_r[2] & 0x7F) << 14) |
		((edge_info.ee_lum_r[1] & 0x7F) << 7) |
		(edge_info.ee_lum_r[0] & 0x7F);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG10, val);

	val = ((edge_info.ee_pos_t[2]  & 0x3FF) << 20) |
		((edge_info.ee_pos_t[1]  & 0x3FF) << 10) |
		(edge_info.ee_pos_t[0]  & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG11, val);

	val = ((edge_info.ee_pos_r[2]  & 0x7F) << 24) |
		((edge_info.ee_pos_r[1]  & 0x7F) << 17) |
		((edge_info.ee_pos_r[0]  & 0x7F) << 10) |
		(edge_info.ee_pos_t[3]  & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG12, val);

	val = ((edge_info.ee_pos_c[2] & 0x7F) << 14) |
		((edge_info.ee_pos_c[1] & 0x7F) << 7) |
		(edge_info.ee_pos_c[0] & 0x7F);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG13, val);

	val = ((edge_info.ee_neg_t[2] & 0x3FF) << 20) |
		((edge_info.ee_neg_t[1]  & 0x3FF) << 10) |
		(edge_info.ee_neg_t[0]  & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG14, val);

	val = ((edge_info.ee_neg_r[2] & 0x7F) << 24) |
		((edge_info.ee_neg_r[1] & 0x7F) << 17) |
		((edge_info.ee_neg_r[0] & 0x7F) << 10) |
		(edge_info.ee_neg_t[3]  & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG15, val);

	val = ((edge_info.ee_neg_c[2] & 0xFF) << 16) |
		((edge_info.ee_neg_c[1] & 0xFF) << 8) |
		(edge_info.ee_neg_c[0] & 0xFF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG16, val);

	val = ((edge_info.ee_freq_t[2] & 0x3FF) << 20) |
		((edge_info.ee_freq_t[1] & 0x3FF) << 10) |
		(edge_info.ee_freq_t[0] & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG17, val);

	val = ((edge_info.ee_freq_t[3] & 0x3FF) << 18) |
		((edge_info.ee_freq_r[2] & 0x3F) << 12) |
		((edge_info.ee_freq_r[1] & 0x3F) << 6) |
		(edge_info.ee_freq_r[0] & 0x3F);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG18, val);

	val = ((edge_info.ee_ratio_old_gradient & 0x3F) << 6) |
			(edge_info.ee_ratio_new_pyramid & 0x3F);
	ISP_REG_WR(idx, ISP_EE_RATIO, val);

	val = ((edge_info.ee_offset_thr_layer_curve_pos[0][3] & 0xFF) << 24) |
		((edge_info.ee_offset_thr_layer_curve_pos[0][2] & 0xFF) << 16) |
		((edge_info.ee_offset_thr_layer_curve_pos[0][1] & 0xFF) << 8) |
		(edge_info.ee_offset_thr_layer_curve_pos[0][0] & 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_THR_LAYER0_POS, val);

	val = ((edge_info.ee_offset_ratio_layer_curve_pos[0][2] & 0x7F) << 16) |
			((edge_info.ee_offset_ratio_layer_curve_pos[0][1]
			& 0x7F) << 8) |
			(edge_info.ee_offset_ratio_layer_curve_pos[0][0]
			& 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER0_CURVE_POS, val);

	val = ((edge_info.ee_offset_clip_layer_curve_pos[0][2] & 0x7F) << 16) |
		((edge_info.ee_offset_clip_layer_curve_pos[0][1] & 0x7F) << 8) |
		(edge_info.ee_offset_clip_layer_curve_pos[0][0] & 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_CLIP_LAYER0_POS, val);

	val = ((edge_info.ee_offset_thr_layer_curve_neg[0][3] & 0xFF) << 24) |
		((edge_info.ee_offset_thr_layer_curve_neg[0][2] & 0xFF) << 16) |
		((edge_info.ee_offset_thr_layer_curve_neg[0][1] & 0xFF) << 8) |
		(edge_info.ee_offset_thr_layer_curve_neg[0][0] & 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_THR_LAYER0_NEG, val);

	val = ((edge_info.ee_offset_ratio_layer_curve_neg[0][2] & 0x7F) << 16) |
		((edge_info.ee_offset_ratio_layer_curve_neg[0][1]
		& 0x7F) << 8) |
		(edge_info.ee_offset_ratio_layer_curve_neg[0][0]
		& 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER0_CURVE_NEG, val);

	val = ((edge_info.ee_offset_clip_layer_curve_neg[0][2] & 0xFF) << 16) |
		((edge_info.ee_offset_clip_layer_curve_neg[0][1] & 0xFF) << 8) |
		(edge_info.ee_offset_clip_layer_curve_neg[0][0] & 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_CLIP_LAYER0_CURVE_NEG, val);

	val = ((edge_info.ee_offset_ratio_layer_lum_curve[0][2] & 0x7F) << 16) |
			((edge_info.ee_offset_ratio_layer_lum_curve[0][1]
			& 0x7F) << 8) |
			(edge_info.ee_offset_ratio_layer_lum_curve[0][0]
			& 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER0_LUM_CURVE, val);

	val = ((edge_info.ee_offset_ratio_layer_freq_curve[0][2]
			& 0x3F) << 16) |
			((edge_info.ee_offset_ratio_layer_freq_curve[0][1]
			& 0x3F) << 8) |
			(edge_info.ee_offset_ratio_layer_freq_curve[0][0]
			& 0x3F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER0_FREQ_CURVE, val);

	val = ((edge_info.ee_offset_thr_layer_curve_pos[1][3]
			& 0xFF) << 24) |
			((edge_info.ee_offset_thr_layer_curve_pos[1][2]
			& 0xFF) << 16) |
			((edge_info.ee_offset_thr_layer_curve_pos[1][1]
			& 0xFF) << 8) |
			(edge_info.ee_offset_thr_layer_curve_pos[1][0]
			& 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_THR_LAYER1_POS, val);

	val = ((edge_info.ee_offset_ratio_layer_curve_pos[1][2]
			& 0x7F) << 16) |
			((edge_info.ee_offset_ratio_layer_curve_pos[1][1]
			& 0x7F) << 8) |
			(edge_info.ee_offset_ratio_layer_curve_pos[1][0]
			& 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER1_CURVE_POS, val);

	val = ((edge_info.ee_offset_clip_layer_curve_pos[1][2] & 0x7F) << 16) |
			((edge_info.ee_offset_clip_layer_curve_pos[1][1]
			& 0x7F) << 8) |
			(edge_info.ee_offset_clip_layer_curve_pos[1][0]
			& 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_CLIP_LAYER1_POS, val);

	val = ((edge_info.ee_offset_thr_layer_curve_neg[1][3] & 0xFF) << 24) |
			((edge_info.ee_offset_thr_layer_curve_neg[1][2]
			& 0xFF) << 16) |
			((edge_info.ee_offset_thr_layer_curve_neg[1][1]
			& 0xFF) << 8) |
			(edge_info.ee_offset_thr_layer_curve_neg[1][0]
			& 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_THR_LAYER1_NEG, val);

	val = ((edge_info.ee_offset_ratio_layer_curve_neg[1][2] & 0x7F) << 16) |
			((edge_info.ee_offset_ratio_layer_curve_neg[1][1]
			& 0x7F) << 8) |
			(edge_info.ee_offset_ratio_layer_curve_neg[1][0]
			& 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER1_CURVE_NEG, val);

	val = ((edge_info.ee_offset_clip_layer_curve_neg[1][2] & 0xFF) << 16) |
			((edge_info.ee_offset_clip_layer_curve_neg[1][1]
			& 0xFF) << 8) |
			(edge_info.ee_offset_clip_layer_curve_neg[1][0]
			& 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_CLIP_LAYER1_CURVE_NEG, val);

	val = ((edge_info.ee_offset_ratio_layer_lum_curve[1][2]
			& 0x7F) << 16) |
			((edge_info.ee_offset_ratio_layer_lum_curve[1][1]
			& 0x7F) << 8) |
			(edge_info.ee_offset_ratio_layer_lum_curve[1][0]
			& 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER1_LUM_CURVE, val);

	val = ((edge_info.ee_offset_ratio_layer_freq_curve[1][2]
			& 0x3F) << 16)
			|((edge_info.ee_offset_ratio_layer_freq_curve[1][1]
			& 0x3F) << 8) |
			(edge_info.ee_offset_ratio_layer_freq_curve[1][0]
			& 0x3F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER1_FREQ_CURVE, val);

	val = ((edge_info.ee_offset_thr_layer_curve_pos[2][3]
			& 0xFF) << 24)
			| ((edge_info.ee_offset_thr_layer_curve_pos[2][2]
			& 0xFF) << 16) |
			((edge_info.ee_offset_thr_layer_curve_pos[2][1]
			& 0xFF) << 8) |
			(edge_info.ee_offset_thr_layer_curve_pos[2][0]
			& 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_THR_LAYER2_POS, val);

	val = ((edge_info.ee_offset_ratio_layer_curve_pos[2][2] & 0x7F) << 16) |
			((edge_info.ee_offset_ratio_layer_curve_pos[2][1]
			& 0x7F) << 8) |
			(edge_info.ee_offset_ratio_layer_curve_pos[2][0]
			& 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER2_CURVE_POS, val);

	val = ((edge_info.ee_offset_clip_layer_curve_pos[2][2] & 0x7F) << 16) |
			((edge_info.ee_offset_clip_layer_curve_pos[2][1]
			& 0x7F) << 8) |
			(edge_info.ee_offset_clip_layer_curve_pos[2][0]
			& 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_CLIP_LAYER2_POS, val);

	val = ((edge_info.ee_offset_thr_layer_curve_neg[2][3] & 0xFF) << 24) |
			((edge_info.ee_offset_thr_layer_curve_neg[2][2]
			& 0xFF) << 16) |
			((edge_info.ee_offset_thr_layer_curve_neg[2][1]
			& 0xFF) << 8) |
			(edge_info.ee_offset_thr_layer_curve_neg[2][0]
			& 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_THR_LAYER2_NEG, val);

	val = ((edge_info.ee_offset_ratio_layer_curve_neg[2][2] & 0x7F) << 16) |
			((edge_info.ee_offset_ratio_layer_curve_neg[2][1]
			& 0x7F) << 8) |
			(edge_info.ee_offset_ratio_layer_curve_neg[2][0]
			& 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER2_CURVE_NEG, val);

	val = ((edge_info.ee_offset_clip_layer_curve_neg[2][2] & 0xFF) << 16) |
			((edge_info.ee_offset_clip_layer_curve_neg[2][1]
			& 0xFF) << 8) |
			(edge_info.ee_offset_clip_layer_curve_neg[2][0]
			& 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_CLIP_LAYER2_CURVE_NEG, val);

	val = ((edge_info.ee_offset_ratio_layer_lum_curve[2][2] & 0x7F) << 16) |
			((edge_info.ee_offset_ratio_layer_lum_curve[2][1]
			& 0x7F) << 8) |
			(edge_info.ee_offset_ratio_layer_lum_curve[2][0]
			& 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER2_LUM_CURVE, val);

	val = ((edge_info.ee_offset_ratio_layer_freq_curve[2][2]
			& 0x3F) << 16) |
			((edge_info.ee_offset_ratio_layer_freq_curve[2][1]
			& 0x3F) << 8) |
			(edge_info.ee_offset_ratio_layer_freq_curve[2][0]
			& 0x3F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER2_FREQ_CURVE, val);

	return ret;
}

int isp_k_cfg_edge(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t com_idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_EDGE_BLOCK:
		ret = isp_k_edge_block(param, com_idx);
		break;

	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
