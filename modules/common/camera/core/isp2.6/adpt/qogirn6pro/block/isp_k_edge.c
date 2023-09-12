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
#include "cam_types.h"
#include "cam_block.h"
#include "isp_core.h"
#include "cam_queue.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "EDGE: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int isp_k_edge_block(struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	uint32_t i = 0, val = 0, ee_weight_diag2hv = 0;
	struct isp_dev_edge_info_v3 *edge_info = NULL;
	if (isp_k_param->edge_info_v3.isupdate == 0)
		return ret;

	edge_info = &isp_k_param->edge_info_v3;
	isp_k_param->edge_info.isupdate = 0;

	if (g_isp_bypass[idx] & (1 << _EISP_EE))
		edge_info->bypass = 1;

	if (edge_info->bypass)
		return 0;

	val = ((edge_info->ee_radial_1D_pyramid_layer_offset_en & 0x1) << 14) |
		((edge_info->ee_cal_radius_en & 0x1) << 13) |
		((edge_info->ee_radial_1D_en & 0x1) << 12) |
		((edge_info->ee_radial_1D_old_gradient_ratio_en & 0x1) << 11) |
		((edge_info->ee_radial_1D_new_pyramid_ratio_en & 0x1) << 10) |
		((edge_info->ee_offset_ratio_layer0_computation_type & 0x1) << 9) |
		((edge_info->ee_ipd_orientation_filter_mode & 0x1) << 8) |
		((edge_info->ee_ipd_direction_mode & 0x7) << 5) |
		((edge_info->ee_ipd_direction_freq_hop_control_en & 0x1) << 4) |
		((edge_info->ee_ipd_1d_en & 0x1) << 3) |
		((edge_info->ee_old_gradient_en & 0x1) << 2) |
		((edge_info->ee_new_pyramid_en & 0x1) << 1);
	ISP_REG_MWR(idx, ISP_EE_PARAM, 0x7FFE, val);

	ISP_REG_MWR(idx, ISP_EE_CFG0, BIT_28, BIT_28);

	val = ((edge_info->ipd_smooth_mode.n & 0x7) << 14) |
		((edge_info->ipd_smooth_mode.p & 0x7) << 11) |
		((edge_info->ipd_smooth_en & 0x1) << 10) |
		((edge_info->ipd_less_thr.n & 0xF) << 6) |
		((edge_info->ipd_less_thr.p & 0xF) << 2) |
		((edge_info->ipd_mask_mode & 0x1) << 1) |
		(edge_info->ipd_enable & 0x1);
	ISP_REG_WR(idx, ISP_EE_IPD_CFG0, val);

	val = ((edge_info->ipd_more_thr.n & 0xF) << 28) |
		((edge_info->ipd_more_thr.p & 0xF) << 24) |
		((edge_info->ipd_eq_thr.n & 0xF) << 20) |
		((edge_info->ipd_eq_thr.p & 0xF) << 16) |
		((edge_info->ipd_flat_thr.n & 0xFF) << 8) |
		(edge_info->ipd_flat_thr.p & 0xFF);
	ISP_REG_WR(idx, ISP_EE_IPD_CFG1, val);

	val = ((edge_info->ipd_smooth_edge_diff.n & 0xFF) << 24) |
		((edge_info->ipd_smooth_edge_diff.p & 0xFF) << 16) |
		((edge_info->ipd_smooth_edge_thr.n & 0xFF) << 8) |
		(edge_info->ipd_smooth_edge_thr.p & 0xFF);
	ISP_REG_WR(idx, ISP_EE_IPD_CFG2, val);

	ee_weight_diag2hv = 16 - edge_info->ee_weight_hv2diag;
	val = ((ee_weight_diag2hv & 0x1F) << 27) |
		((edge_info->ee_gradient_computation_type & 0x1) << 26) |
		((edge_info->ee_weight_hv2diag & 0x1F) << 21) |
		((edge_info->ee_ratio_diag_3 & 0x7F) << 14) |
		((edge_info->ee_ratio_hv_5 & 0x7F) << 7) |
		(edge_info->ee_ratio_hv_3 & 0x7F);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG0, val);

	for (i = 0; i < 2; i++) {
		val = ((edge_info->ee_gain_hv_t[i][2] & 0x3FF) << 20) |
			((edge_info->ee_gain_hv_t[i][1] & 0x3FF) << 10) |
			(edge_info->ee_gain_hv_t[i][0] & 0x3FF);
		ISP_REG_WR(idx, ISP_EE_LUM_CFG1 + 8 * i, val);
	}

	val = ((edge_info->ee_ratio_diag_5 & 0x7F) << 25) |
		((edge_info->ee_gain_hv_r[0][2] & 0x1F) << 20) |
		((edge_info->ee_gain_hv_r[0][1] & 0x1F) << 15) |
		((edge_info->ee_gain_hv_r[0][0] & 0x1F) << 10) |
		(edge_info->ee_gain_hv_t[0][3] & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG2, val);

	val = ((edge_info->ee_gain_hv_r[1][2] & 0x1F) << 20) |
		((edge_info->ee_gain_hv_r[1][1] & 0x1F) << 15) |
		((edge_info->ee_gain_hv_r[1][0] & 0x1F) << 10) |
		(edge_info->ee_gain_hv_t[1][3] & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG4, val);

	for (i = 0; i < 2; i++) {
		val = ((edge_info->ee_gain_diag_t[i][2] & 0x3FF) << 20) |
			((edge_info->ee_gain_diag_t[i][1] & 0x3FF) << 10) |
			(edge_info->ee_gain_diag_t[i][0] & 0x3FF);
		ISP_REG_WR(idx, ISP_EE_LUM_CFG5 + 8 * i, val);

		val = ((edge_info->ee_gain_diag_r[i][2] & 0x1F) << 20) |
			((edge_info->ee_gain_diag_r[i][1] & 0x1F) << 15) |
			((edge_info->ee_gain_diag_r[i][0] & 0x1F) << 10) |
			(edge_info->ee_gain_diag_t[i][3] & 0x3FF);
		ISP_REG_WR(idx, ISP_EE_LUM_CFG6 + 8 * i, val);
	}

	val = ((edge_info->ee_lum_t[3] & 0xFF) << 24) |
		((edge_info->ee_lum_t[2] & 0xFF) << 16) |
		((edge_info->ee_lum_t[1] & 0xFF) << 8) |
		(edge_info->ee_lum_t[0] & 0xFF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG9, val);

	val = ((edge_info->ee_lum_r[2] & 0x7F) << 14) |
		((edge_info->ee_lum_r[1] & 0x7F) << 7) |
		(edge_info->ee_lum_r[0] & 0x7F);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG10, val);

	val = ((edge_info->ee_pos_t[2]  & 0x3FF) << 20) |
		((edge_info->ee_pos_t[1] & 0x3FF) << 10) |
		(edge_info->ee_pos_t[0] & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG11, val);

	val = ((edge_info->ee_pos_r[2] & 0x7F) << 24) |
		((edge_info->ee_pos_r[1] & 0x7F) << 17) |
		((edge_info->ee_pos_r[0] & 0x7F) << 10) |
		(edge_info->ee_pos_t[3] & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG12, val);

	val = ((edge_info->ee_pos_c[2] & 0x7F) << 14) |
		((edge_info->ee_pos_c[1] & 0x7F) << 7) |
		(edge_info->ee_pos_c[0] & 0x7F);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG13, val);

	val = ((edge_info->ee_neg_t[2]  & 0x3FF) << 20) |
		((edge_info->ee_neg_t[1]  & 0x3FF) << 10) |
		(edge_info->ee_neg_t[0]  & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG14, val);

	val = ((edge_info->ee_neg_r[2] & 0x7F) << 24) |
		((edge_info->ee_neg_r[1] & 0x7F) << 17) |
		((edge_info->ee_neg_r[0] & 0x7F) << 10) |
		(edge_info->ee_neg_t[3] & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG15, val);

	val = ((edge_info->ee_neg_c[2] & 0xFF) << 16) |
		((edge_info->ee_neg_c[1] & 0xFF) << 8) |
		(edge_info->ee_neg_c[0] & 0xFF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG16, val);

	val = ((edge_info->ee_freq_t[2] & 0x3FF) << 20) |
		((edge_info->ee_freq_t[1] & 0x3FF) << 10) |
		(edge_info->ee_freq_t[0] & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG17, val);

	val = ((edge_info->ee_freq_t[3] & 0x3FF) << 18) |
		((edge_info->ee_freq_r[2] & 0x3F) << 12) |
		((edge_info->ee_freq_r[1] & 0x3F) << 6) |
		(edge_info->ee_freq_r[0] & 0x3F);
	ISP_REG_WR(idx, ISP_EE_LUM_CFG18, val);


	/* new added below */
	val =  ((edge_info->ee_ratio_old_gradient & 0x3F) << 6) |
		(edge_info->ee_ratio_new_pyramid & 0x3F);
	ISP_REG_WR(idx, ISP_EE_RATIO, val);

	val = ((edge_info->ee_offset_layer0_thr_layer_curve_pos[3] & 0xFF) << 24) |
		((edge_info->ee_offset_layer0_thr_layer_curve_pos[2] & 0xFF) << 16) |
		((edge_info->ee_offset_layer0_thr_layer_curve_pos[1] & 0xFF) << 8) |
		(edge_info->ee_offset_layer0_thr_layer_curve_pos[0] & 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_THR_LAYER0_POS, val);

	val = ((edge_info->ee_offset_layer0_ratio_layer_curve_pos[2] & 0x7F) << 16) |
		((edge_info->ee_offset_layer0_ratio_layer_curve_pos[1] & 0x7F) << 8) |
		(edge_info->ee_offset_layer0_ratio_layer_curve_pos[0] & 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER0_CURVE_POS, val);

	val = ((edge_info->ee_offset_layer0_clip_layer_curve_pos[2] & 0x7F) << 16) |
		((edge_info->ee_offset_layer0_clip_layer_curve_pos[1] & 0x7F) << 8) |
		(edge_info->ee_offset_layer0_clip_layer_curve_pos[0] & 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_CLIP_LAYER0_POS, val);

	val = ((edge_info->ee_offset_layer0_thr_layer_curve_neg[3] & 0xFF) << 24) |
		((edge_info->ee_offset_layer0_thr_layer_curve_neg[2] & 0xFF) << 16) |
		((edge_info->ee_offset_layer0_thr_layer_curve_neg[1] & 0xFF) << 8) |
		(edge_info->ee_offset_layer0_thr_layer_curve_neg[0] & 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_THR_LAYER0_NEG, val);

	val = ((edge_info->ee_offset_layer0_ratio_layer_curve_neg[2] & 0x7F) << 16) |
		((edge_info->ee_offset_layer0_ratio_layer_curve_neg[1] & 0x7F) << 8) |
		(edge_info->ee_offset_layer0_ratio_layer_curve_neg[0] & 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER0_CURVE_NEG, val);

	val = ((edge_info->ee_offset_layer0_clip_layer_curve_neg[2] & 0xFF) << 16) |
		((edge_info->ee_offset_layer0_clip_layer_curve_neg[1] & 0xFF) << 8) |
		(edge_info->ee_offset_layer0_clip_layer_curve_neg[0] & 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_CLIP_LAYER0_CURVE_NEG, val);

	val = ((edge_info->ee_offset_layer0_ratio_layer_lum_curve[2] & 0x7F) << 16) |
		((edge_info->ee_offset_layer0_ratio_layer_lum_curve[1] & 0x7F) << 8) |
		(edge_info->ee_offset_layer0_ratio_layer_lum_curve[0] & 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER0_LUM_CURVE, val);

	val = ((edge_info->ee_offset_layer0_ratio_layer_freq_curve[2] & 0x3F) << 16) |
		((edge_info->ee_offset_layer0_ratio_layer_freq_curve[1] & 0x3F) << 8) |
		(edge_info->ee_offset_layer0_ratio_layer_freq_curve[0] & 0x3F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER0_FREQ_CURVE, val);

	val = ((edge_info->ee_offset_thr_layer_curve_pos[0][3] & 0xFF) << 24) |
		((edge_info->ee_offset_thr_layer_curve_pos[0][2] & 0xFF) << 16) |
		((edge_info->ee_offset_thr_layer_curve_pos[0][1] & 0xFF) << 8) |
		(edge_info->ee_offset_thr_layer_curve_pos[0][0] & 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_THR_LAYER1_POS, val);

	val = ((edge_info->ee_offset_ratio_layer_curve_pos[0][2] & 0x7F) << 16) |
		((edge_info->ee_offset_ratio_layer_curve_pos[0][1] & 0x7F) << 8) |
		(edge_info->ee_offset_ratio_layer_curve_pos[0][0] & 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER1_CURVE_POS, val);

	val = ((edge_info->ee_offset_clip_layer_curve_pos[0][2] & 0x7F) << 16) |
		((edge_info->ee_offset_clip_layer_curve_pos[0][1] & 0x7F) << 8) |
		(edge_info->ee_offset_clip_layer_curve_pos[0][0] & 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_CLIP_LAYER1_POS, val);

	val = ((edge_info->ee_offset_thr_layer_curve_neg[0][3] & 0xFF) << 24) |
		((edge_info->ee_offset_thr_layer_curve_neg[0][2] & 0xFF) << 16) |
		((edge_info->ee_offset_thr_layer_curve_neg[0][1] & 0xFF) << 8) |
		(edge_info->ee_offset_thr_layer_curve_neg[0][0] & 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_THR_LAYER1_NEG, val);

	val = ((edge_info->ee_offset_ratio_layer_curve_neg[0][2] & 0x7F) << 16) |
		((edge_info->ee_offset_ratio_layer_curve_neg[0][1] & 0x7F) << 8) |
		(edge_info->ee_offset_ratio_layer_curve_neg[0][0] & 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER1_CURVE_NEG, val);

	val = ((edge_info->ee_offset_clip_layer_curve_neg[0][2] & 0xFF) << 16) |
		((edge_info->ee_offset_clip_layer_curve_neg[0][1] & 0xFF) << 8) |
		(edge_info->ee_offset_clip_layer_curve_neg[0][0] & 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_CLIP_LAYER1_CURVE_NEG, val);

	val = ((edge_info->ee_offset_ratio_layer_lum_curve[0][2] & 0x7F) << 16) |
		((edge_info->ee_offset_ratio_layer_lum_curve[0][1] & 0x7F) << 8) |
		(edge_info->ee_offset_ratio_layer_lum_curve[0][0] & 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER1_LUM_CURVE, val);

	val = ((edge_info->ee_offset_ratio_layer_freq_curve[0][2] & 0x3F) << 16) |
		((edge_info->ee_offset_ratio_layer_freq_curve[0][1] & 0x3F) << 8) |
		(edge_info->ee_offset_ratio_layer_freq_curve[0][0] & 0x3F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER1_FREQ_CURVE, val);

	val = ((edge_info->ee_offset_thr_layer_curve_pos[1][3] & 0xFF) << 24) |
		((edge_info->ee_offset_thr_layer_curve_pos[1][2] & 0xFF) << 16) |
		((edge_info->ee_offset_thr_layer_curve_pos[1][1] & 0xFF) << 8) |
		(edge_info->ee_offset_thr_layer_curve_pos[1][0] & 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_THR_LAYER2_POS, val);

	val = ((edge_info->ee_offset_ratio_layer_curve_pos[1][2] & 0x7F) << 16) |
		((edge_info->ee_offset_ratio_layer_curve_pos[1][1] & 0x7F) << 8) |
		(edge_info->ee_offset_ratio_layer_curve_pos[1][0] & 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER2_CURVE_POS, val);

	val = ((edge_info->ee_offset_clip_layer_curve_pos[1][2] & 0x7F) << 16) |
		((edge_info->ee_offset_clip_layer_curve_pos[1][1] & 0x7F) << 8) |
		(edge_info->ee_offset_clip_layer_curve_pos[1][0] & 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_CLIP_LAYER2_POS, val);

	val = ((edge_info->ee_offset_thr_layer_curve_neg[1][3] & 0xFF) << 24) |
		((edge_info->ee_offset_thr_layer_curve_neg[1][2] & 0xFF) << 16) |
		((edge_info->ee_offset_thr_layer_curve_neg[1][1] & 0xFF) << 8) |
		(edge_info->ee_offset_thr_layer_curve_neg[1][0] & 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_THR_LAYER2_NEG, val);

	val = ((edge_info->ee_offset_ratio_layer_curve_neg[1][2] & 0x7F) << 16) |
		((edge_info->ee_offset_ratio_layer_curve_neg[1][1] & 0x7F) << 8) |
		(edge_info->ee_offset_ratio_layer_curve_neg[1][0] & 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER2_CURVE_NEG, val);

	val = ((edge_info->ee_offset_clip_layer_curve_neg[1][2] & 0xFF) << 16) |
		((edge_info->ee_offset_clip_layer_curve_neg[1][1] & 0xFF) << 8) |
		(edge_info->ee_offset_clip_layer_curve_neg[1][0] & 0xFF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_CLIP_LAYER2_CURVE_NEG, val);

	val = ((edge_info->ee_offset_ratio_layer_lum_curve[1][2] & 0x7F) << 16) |
		((edge_info->ee_offset_ratio_layer_lum_curve[1][1] & 0x7F) << 8) |
		(edge_info->ee_offset_ratio_layer_lum_curve[1][0] & 0x7F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER2_LUM_CURVE, val);

	val = ((edge_info->ee_offset_ratio_layer_freq_curve[1][2] & 0x3F) << 16) |
		((edge_info->ee_offset_ratio_layer_freq_curve[1][1] & 0x3F) << 8) |
		(edge_info->ee_offset_ratio_layer_freq_curve[1][0] & 0x3F);
	ISP_REG_WR(idx, ISP_EE_OFFSET_RATIO_LAYER2_FREQ_CURVE, val);

	val = ((edge_info->direction_thresh_min & 0x1FF) << 16) |
		(edge_info->direction_thresh_diff & 0x1FF);
	ISP_REG_WR(idx, ISP_EE_IPD_THR, val);

	val = ((edge_info->direction_freq_hop_thresh & 0xFF) << 16) |
		(edge_info->freq_hop_total_num_thresh & 0x7F);
	ISP_REG_WR(idx, ISP_EE_HOP_CFG0, val);

	val = ((edge_info->direction_hop_thresh_diff & 0x1FF) << 16) |
		(edge_info->direction_hop_thresh_min & 0x1FF);
	ISP_REG_WR(idx, ISP_EE_HOP_CFG1, val);

	val = ((edge_info->ee_offset_layer0_gradient_curve_pos[0] & 0x3FF) << 16) |
		(edge_info->ee_offset_layer0_gradient_curve_pos[1] & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_GRAD_CFG0, val);

	val = ((edge_info->ee_offset_layer0_gradient_curve_pos[2] & 0x3FF) << 16) |
		(edge_info->ee_offset_layer0_gradient_curve_pos[3] & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_GRAD_CFG1, val);

	val = ((edge_info->ee_offset_layer0_gradient_curve_neg[0] & 0x3FF) << 16) |
		(edge_info->ee_offset_layer0_gradient_curve_neg[1] & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_GRAD_CFG2, val);

	val = ((edge_info->ee_offset_layer0_gradient_curve_neg[2] & 0x3FF) << 16) |
		(edge_info->ee_offset_layer0_gradient_curve_neg[3] & 0x3FF);
	ISP_REG_WR(idx, ISP_EE_OFFSET_GRAD_CFG3, val);

	val = ((edge_info->old_gradient_ratio_gain_max & 0x7FF) << 16) |
		(edge_info->old_gradient_ratio_gain_min & 0x7FF);
	ISP_REG_WR(idx, ISP_EE_GRAD_RATIO_THR, val);

	val = ((edge_info->new_pyramid_ratio_gain_max & 0x7FF) << 16) |
		(edge_info->new_pyramid_ratio_gain_min & 0x7FF);
	ISP_REG_WR(idx, ISP_EE_PYRAMID_RATIO_THR, val);

	val = ((edge_info->center_x & 0x3FFF) << 16) |
		(edge_info->center_y & 0x3FFF);
	ISP_REG_WR(idx, ISP_EE_PIXEL_POSTION, val);

	val = ((edge_info->radius_threshold_factor & 0x7FFF) << 16) |
		(edge_info->layer_pyramid_offset_coef1 & 0x3FFF);
	ISP_REG_WR(idx, ISP_EE_PYRAMID_OFFSET_COEF0, val);

	val = ((edge_info->layer_pyramid_offset_coef2 & 0x3FFF) << 16) |
		(edge_info->layer_pyramid_offset_coef3 & 0x3FFF);
	ISP_REG_WR(idx, ISP_EE_PYRAMID_OFFSET_COEF1, val);

	val = ((edge_info->old_gradient_ratio_coef & 0x3FFF) << 16) |
		(edge_info->new_pyramid_ratio_coef & 0x3FFF);
	ISP_REG_WR(idx, ISP_EE_RATIO_COEF, val);

	val = ((edge_info->layer_pyramid_offset_gain_max & 0x7FF) << 16) |
		(edge_info->layer_pyramid_offset_clip_ratio_max & 0x7FF);
	ISP_REG_WR(idx, ISP_EE_PYRAMID_OFFSET_THR, val);

	val = ((edge_info->layer_pyramid_offset_gain_min1 & 0x7FF) << 16) |
		(edge_info->layer_pyramid_offset_gain_min2 & 0x7FF);
	ISP_REG_WR(idx, ISP_EE_PYRAMID_OFFSET_GAIN_MIN0, val);

	val =(edge_info->layer_pyramid_offset_gain_min3 & 0x7FF);
	ISP_REG_WR(idx, ISP_EE_PYRAMID_OFFSET_GAIN_MIN1, val);

	return ret;
}

int isp_k_cfg_edge(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_edge_info_v3 *edge_info = NULL;

	edge_info = &isp_k_param->edge_info_v3;

	switch (param->property) {
	case ISP_PRO_EDGE_BLOCK:
		ret = copy_from_user((void *)edge_info, param->property_param, sizeof(struct isp_dev_edge_info_v3));
		if (ret != 0) {
			pr_err("fail to copy from user, ret = %d\n", ret);
			return ret;
		}
		isp_k_param->edge_info_v3.isupdate = 1;

		break;

	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}

int isp_k_update_edge(void *handle)
{
	int ret = 0;
	uint32_t val = 0, center_x = 0, center_y = 0;
	uint32_t radius = 0, radius_limit = 0;
	uint32_t idx = 0, new_width = 0, old_width = 0, sensor_width = 0;
	uint32_t new_height = 0, old_height = 0, sensor_height = 0;
	struct isp_dev_edge_info_v3 *edge_info = NULL;
	struct isp_sw_context *pctx = NULL;

	if (!handle) {
		pr_err("fail to get invalid in ptr\n");
		return -EFAULT;
	}

	pctx = (struct isp_sw_context *)handle;
	idx = pctx->ctx_id;
	new_width = pctx->isp_using_param->blkparam_info.new_width;
	new_height = pctx->isp_using_param->blkparam_info.new_height;
	old_width = pctx->isp_using_param->blkparam_info.old_width;
	old_height = pctx->isp_using_param->blkparam_info.old_height;
	sensor_width = pctx->uinfo.sn_size.w;
	sensor_height = pctx->uinfo.sn_size.h;

	edge_info = &pctx->isp_using_param->edge_info_v3;

	if (edge_info->bypass)
		return 0;

	center_x = new_width >> 1;
	center_y = new_height >> 1;
	val = ((center_x & 0x3FFF) << 16) | (center_y & 0x3FFF);
	ISP_REG_WR(idx, ISP_EE_PIXEL_POSTION, val);

	edge_info->center_x = center_x;
	edge_info->center_y = center_y;

	if (edge_info->radius_base == 0)
		edge_info->radius_base = 1024;

	radius = (sensor_width + sensor_height ) * edge_info->radius_threshold_factor / edge_info->radius_base;
	radius_limit = new_width+new_height;
	radius = (radius < radius_limit) ? radius : radius_limit;
	radius = new_height * radius / old_height;

	edge_info->radius = radius;
	val = ((edge_info->radius & 0x7FFF) << 16) |
		(edge_info->layer_pyramid_offset_coef1 & 0x3FFF);
	ISP_REG_WR(idx, ISP_EE_PYRAMID_OFFSET_COEF0, val);
	pr_debug("cen %d %d, base %d, factor %d, new radius %d\n",
		center_x, center_y, edge_info->radius_base, edge_info->radius_threshold_factor, radius);

	return ret;
}

int isp_k_cpy_edge(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if (isp_k_param->edge_info_v3.isupdate == 1) {
		memcpy(&param_block->edge_info_v3, &isp_k_param->edge_info_v3, sizeof(struct isp_dev_edge_info_v3));
		isp_k_param->edge_info_v3.isupdate = 0;
		param_block->edge_info_v3.isupdate = 1;
	}

	return ret;
}
