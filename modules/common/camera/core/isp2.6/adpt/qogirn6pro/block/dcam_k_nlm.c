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
#include <linux/vmalloc.h>
#include <sprd_mm.h>

#include "isp_hw.h"
#include "isp_reg.h"
#include "dcam_reg.h"
#include "cam_types.h"
#include "cam_block.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "NLM: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__
#define DCAM_VST_IVST_NUM        513

static int load_vst_ivst_buf(struct dcam_dev_param *p)
{
	int ret = 0;
	uint32_t buf_len;
	uint32_t val;
	uint32_t idx = 0;
	unsigned int i = 0;
	uint32_t *vst_ivst_buf = NULL;
	struct isp_dev_nlm_info_v2 *nlm_info = NULL;

	if (p == NULL)
		return 0;

	idx = p->idx;
	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;
	nlm_info = &p->nlm_info2;

	DCAM_REG_MWR(idx, DCAM_BUF_CTRL, BIT_2 | BIT_1, 3 << 1);

	if (nlm_info->vst_bypass == 0 && nlm_info->vst_table_addr) {
		buf_len = ISP_VST_IVST_NUM2 * 4;
		if (nlm_info->vst_len < (ISP_VST_IVST_NUM2 * 4))
			buf_len = nlm_info->vst_len;

		vst_ivst_buf = p->vst_buf;

		/*recombine the vst/ivst table as requires of l5pro's vst/ivst module*/
		for (i = 0; i < DCAM_VST_IVST_NUM; i++) {
			if (i == DCAM_VST_IVST_NUM - 1) {
				val = ((vst_ivst_buf[i * 2] & 0x3fff) << 16) | (vst_ivst_buf[i * 2] & 0x3fff);
				DCAM_REG_WR(idx, DCAM_VST_TABLE + i * 4, val);
			} else {
				val = ((vst_ivst_buf[i * 2 + 1] & 0x3fff) << 16) | (vst_ivst_buf[i * 2] & 0x3fff);
				DCAM_REG_WR(idx, DCAM_VST_TABLE + i * 4, val);
			}
		}

		val = DCAM_REG_RD(idx, DCAM_BUF_CTRL);
		DCAM_REG_MWR(idx, DCAM_BUF_CTRL, BIT_17, ~(val & 0x20000));

	}
	if (nlm_info->ivst_bypass == 0 && nlm_info->ivst_table_addr) {
		buf_len = ISP_VST_IVST_NUM2 * 4;
		if (nlm_info->ivst_len < (ISP_VST_IVST_NUM2 * 4))
			buf_len = nlm_info->ivst_len;

		vst_ivst_buf = p->ivst_buf;

		/*recombine the vst/ivst table as requires of l5pro's vst/ivst module*/
		for (i = 0; i < DCAM_VST_IVST_NUM; i++) {
			if (i == DCAM_VST_IVST_NUM - 1) {
				val = ((vst_ivst_buf[i * 2] & 0x3fff) << 16) | (vst_ivst_buf[i * 2] & 0x3fff);
				DCAM_REG_WR(idx, DCAM_IVST_TABLE + i * 4, val);
			} else {
				val = ((vst_ivst_buf[i * 2 + 1] & 0x3fff) << 16) | (vst_ivst_buf[i * 2] & 0x3fff);
				DCAM_REG_WR(idx, DCAM_IVST_TABLE + i * 4, val);
			}
		}

		val = DCAM_REG_RD(idx, DCAM_BUF_CTRL);
		DCAM_REG_MWR(idx, DCAM_BUF_CTRL, BIT_18, ~(val & 0x40000));

	}

	return ret;
}

int dcam_k_nlm_block(struct dcam_dev_param *p)
{
	int ret = 0;
	uint32_t i = 0, j = 0, val = 0, idx = 0;
	uint32_t center_x = 0, center_y = 0, radius_threshold = 0;
	uint32_t radius_limit = 0, r_factor = 0, r_base = 0;
	uint32_t filter_ratio = 0, coef2 = 0, flat_thresh_coef = 0;
	uint32_t in_width = 0, re_width = 0;
	uint32_t in_height = 0, re_height = 0;
	struct isp_dev_nlm_info_v2 *nlm_info2 = NULL;

	if (p == NULL)
		return 0;

	nlm_info2 = &p->nlm_info2;
	idx = p->idx;
	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;
	in_width = p->in_size.size_x;
	in_height = p->in_size.size_y;
	re_width = nlm_info2->nlm_radial_1D_center_x << 1;
	re_height = nlm_info2->nlm_radial_1D_center_y << 1;

	if (g_dcam_bypass[idx] & (1 << _E_NLM))
		nlm_info2->bypass = 1;
	if (g_dcam_bypass[idx] & (1 << _E_VST))
		nlm_info2->vst_bypass = 1;
	if (g_dcam_bypass[idx] & (1 << _E_IVST))
		nlm_info2->ivst_bypass = 1;

	DCAM_REG_MWR(idx, DCAM_NLM_PARA, BIT_0, nlm_info2->bypass);
	DCAM_REG_MWR(idx, DCAM_VST_PARA, BIT_0, nlm_info2->vst_bypass);
	DCAM_REG_MWR(idx, DCAM_IVST_PARA, BIT_0, nlm_info2->ivst_bypass);
	if (nlm_info2->bypass || nlm_info2->vst_bypass || nlm_info2->ivst_bypass)
		return 0;

	val = ((nlm_info2->imp_opt_bypass & 0x1) << 1) |
		((nlm_info2->flat_opt_bypass & 0x1) << 2) |
		((nlm_info2->direction_mode_bypass & 0x1) << 4) |
		((nlm_info2->first_lum_byapss & 0x1) << 5) |
		((nlm_info2->simple_bpc_bypass & 0x1) << 6);
	DCAM_REG_MWR(idx, DCAM_NLM_PARA, 0x76, val);

	val = ((nlm_info2->direction_cnt_th & 0x3) << 24) |
		((nlm_info2->w_shift[2] & 0x3) << 20) |
		((nlm_info2->w_shift[1] & 0x3) << 18) |
		((nlm_info2->w_shift[0] & 0x3) << 16) |
		(nlm_info2->dist_mode & 0x3);
	DCAM_REG_WR(idx, DCAM_NLM_MODE_CNT, val);

	val = ((nlm_info2->simple_bpc_th & 0xFF) << 16) |
		(nlm_info2->simple_bpc_lum_th & 0x3FF);
	DCAM_REG_WR(idx, DCAM_NLM_SIMPLE_BPC, val);

	val = ((nlm_info2->lum_th1 & 0x3FF) << 16) |
		(nlm_info2->lum_th0 & 0x3FF);
	DCAM_REG_WR(idx, DCAM_NLM_LUM_THRESHOLD, val);

	val = ((nlm_info2->tdist_min_th & 0xFFFF)  << 16) |
		(nlm_info2->diff_th & 0xFFFF);
	DCAM_REG_WR(idx, DCAM_NLM_DIRECTION_TH, val);

	for (i = 0; i < 24; i++) {
		val = (nlm_info2->lut_w[i * 3 + 0] & 0x3FF) |
			((nlm_info2->lut_w[i * 3 + 1] & 0x3FF) << 10) |
			((nlm_info2->lut_w[i * 3 + 2] & 0x3FF) << 20);
		DCAM_REG_WR(idx, DCAM_NLM_LUT_W_0 + i * 4, val);
	}

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			val = ((nlm_info2->lum_flat[i][j].thresh & 0x3FFF) << 16) |
				((nlm_info2->lum_flat[i][j].match_count & 0x1F) << 8) |
				(nlm_info2->lum_flat[i][j].inc_strength & 0xFF);
			DCAM_REG_WR(idx, DCAM_NLM_LUM0_FLAT0_PARAM + (i * 4 + j) * 8, val);
		}
	}

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 4; j++) {
			val = ((nlm_info2->lum_flat_addback_min[i][j] & 0x7FF) << 20) |
				((nlm_info2->lum_flat_addback_max[i][j] & 0x3FF) << 8) |
				(nlm_info2->lum_flat_addback0[i][j] & 0x7F);
			DCAM_REG_WR(idx, DCAM_NLM_LUM0_FLAT0_ADDBACK + (i * 4 + j) * 8, val);
		}
	}

	for (i = 0; i < 3; i++) {
		val = ((nlm_info2->lum_flat_addback1[i][0] & 0x7F) << 22) |
			((nlm_info2->lum_flat_addback1[i][1] & 0x7F) << 15) |
			((nlm_info2->lum_flat_addback1[i][2] & 0x7F) << 8) |
			(nlm_info2->lum_flat_dec_strenth[i] & 0xFF);
		DCAM_REG_WR(idx, DCAM_NLM_LUM0_FLAT3_PARAM + i * 32, val);
	}

	val = ((nlm_info2->lum_flat_addback1[2][3] & 0x7F) << 14) |
		((nlm_info2->lum_flat_addback1[1][3] & 0x7F) << 7) |
		(nlm_info2->lum_flat_addback1[0][3] & 0x7F);
	DCAM_REG_WR(idx, DCAM_NLM_ADDBACK3, val);

	val = (nlm_info2->radius_bypass & 0x1) |
		((nlm_info2->nlm_radial_1D_bypass & 0x1) << 1) |
		((nlm_info2->nlm_direction_addback_mode_bypass & 0x1) << 2) |
		((nlm_info2->update_flat_thr_bypass & 0x1) << 3);
	DCAM_REG_MWR(idx, DCAM_NLM_RADIAL_1D_PARAM, 0xF, val);

	center_x = p->in_size.size_x >> 1;
	center_y = p->in_size.size_y >> 1;
	val = ((center_y & 0x7FFF) << 16) | (center_x & 0x7FFF);
	DCAM_REG_WR(idx, DCAM_NLM_RADIAL_1D_DIST, val);

	r_base = nlm_info2->radius_base;
	r_factor = nlm_info2->nlm_radial_1D_radius_threshold_factor;
	if (r_base == 0)
		r_base = 1024;
	radius_threshold = (in_width + in_height) * r_factor / r_base;
	DCAM_REG_MWR(idx, DCAM_NLM_RADIAL_1D_THRESHOLD, 0x7FFF, radius_threshold);

	val = nlm_info2->nlm_radial_1D_protect_gain_max & 0x1FFF;
	DCAM_REG_MWR(idx, DCAM_NLM_RADIAL_1D_GAIN_MAX, 0x1FFF, val);

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			flat_thresh_coef = nlm_info2->nlm_first_lum_flat_thresh_coef[i][j];
			flat_thresh_coef *= re_width;
			flat_thresh_coef += (in_width / 2);
			flat_thresh_coef /= in_width;
			val = ((nlm_info2->nlm_first_lum_flat_thresh_max[i][j] & 0x3FFF) << 16) |
				(flat_thresh_coef & 0x7FFF);
			DCAM_REG_WR(idx, DCAM_NLM_RADIAL_1D_THR0 + i * 12 + j * 4, val);
		}
	}

	for (i = 0; i < 3; i++) {
		uint32_t *pclip, *pratio;
		pclip = nlm_info2->nlm_first_lum_direction_addback_noise_clip[i];
		pratio = nlm_info2->nlm_radial_1D_radius_threshold_filter_ratio[i];
		for (j = 0; j < 4; j++) {
			filter_ratio = pratio[j];
			filter_ratio *= in_width;
			filter_ratio += (re_width / 2);
			filter_ratio /= re_width;
			r_factor = pratio[j];
			radius_limit = (in_width + in_height) * r_factor / r_base;
			filter_ratio = (filter_ratio < radius_limit) ? filter_ratio : radius_limit;
			val = (pclip[j] & 0x3FF) | ((filter_ratio & 0x7FFF) << 17) |
				((nlm_info2->nlm_first_lum_direction_addback[i][j] & 0x7F) << 10);
			DCAM_REG_WR(idx, DCAM_NLM_RADIAL_1D_ADDBACK00 + i * 16 + j * 4, val);

			coef2 = nlm_info2->nlm_radial_1D_coef2[i][j];
			coef2 *= re_width;
			coef2 = (coef2 + (in_width / 2)) / in_width;
			val = (nlm_info2->nlm_radial_1D_protect_gain_min[i][j] & 0x1FFF) |
				((coef2 & 0x3FFF) << 16);
			DCAM_REG_WR(idx, DCAM_NLM_RADIAL_1D_RATIO + i * 16 + j * 4, val);
		}
	}

	ret = load_vst_ivst_buf(p);

	return ret;
}

int dcam_k_nlm_imblance(struct dcam_dev_param *p)
{
	int ret = 0, idx = 0;
	uint32_t center_x = 0, center_y = 0;
	uint32_t radius = 0;
	uint32_t in_width = 0, in_height = 0;
	uint32_t re_width = 0, re_height = 0;
	struct isp_dev_nlm_imblance_v2 *imblance_info = NULL;

	if (p == NULL)
		return 0;

	imblance_info = &p->nlm_imblance2;
	idx = p->idx;
	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;
	in_width = p->in_size.size_x;
	in_height = p->in_size.size_y;
	re_height = imblance_info->imblance_radial_1D_center_y << 1;
	re_width = imblance_info->imblance_radial_1D_center_x << 1;

	/* new added below */
	if (g_dcam_bypass[idx] & (1 << _E_GRGB))
		imblance_info->nlm_imblance_bypass = 1;

	DCAM_REG_MWR(idx, DCAM_NLM_IMBLANCE_CTRL, BIT_0,
			imblance_info->nlm_imblance_bypass);
	if (imblance_info->nlm_imblance_bypass == 1)
		return 0;

	DCAM_REG_MWR(idx, DCAM_NLM_IMBLANCE_CTRL, BIT_1,
			imblance_info->imblance_radial_1D_en << 1);
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA1,
		(imblance_info->nlm_imblance_slash_edge_thr[0] & 0xff) |
		((imblance_info->nlm_imblance_hv_edge_thr[0] & 0xff) << 8) |
		((imblance_info->nlm_imblance_S_baohedu[0][1] & 0xff) << 16) |
		((imblance_info->nlm_imblance_S_baohedu[0][0] & 0xff) << 24));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA2,
		(imblance_info->nlm_imblance_slash_flat_thr[0] & 0x3ff) |
		((imblance_info->nlm_imblance_hv_flat_thr[0] & 0x3ff) << 10));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA3,
		(imblance_info->nlm_imblance_flag3_frez & 0x3ff) |
		((imblance_info->nlm_imblance_flag3_lum & 0x3ff) << 10) |
		((imblance_info->nlm_imblance_flag3_grid & 0x3ff) << 20));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA4,
		(imblance_info->nlm_imblance_lumth2 & 0xffff) |
		((imblance_info->nlm_imblance_lumth1 & 0xffff) << 16));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA5,
		(imblance_info->nlm_imblance_lum1_flag4_r & 0x7ff) |
		((imblance_info->nlm_imblance_lum1_flag2_r & 0x7ff) << 11) |
		((imblance_info->nlm_imblance_flag12_frezthr & 0x3ff) << 22));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA6,
		(imblance_info->nlm_imblance_lum1_flag0_r & 0x7ff) |
		((imblance_info->nlm_imblance_lum1_flag0_rs & 0x7ff) << 11));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA7,
		(imblance_info->nlm_imblance_lum2_flag2_r & 0x7ff) |
		((imblance_info->nlm_imblance_lum1_flag1_r & 0x7ff) << 11));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA8,
		(imblance_info->nlm_imblance_lum2_flag0_rs & 0x7ff) |
		((imblance_info->nlm_imblance_lum2_flag4_r & 0x7ff) << 11));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA9,
		(imblance_info->nlm_imblance_lum2_flag1_r & 0x7ff) |
		((imblance_info->nlm_imblance_lum2_flag0_r & 0x7ff) << 11));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA10,
		(imblance_info->nlm_imblance_lum3_flag4_r & 0x7ff) |
		((imblance_info->nlm_imblance_lum3_flag2_r & 0x7ff) << 11));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA11,
		(imblance_info->nlm_imblance_lum3_flag0_r & 0x7ff) |
		((imblance_info->nlm_imblance_lum3_flag0_rs & 0x7ff) << 11));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA12,
		(imblance_info->nlm_imblance_diff[0] & 0x3ff) |
		((imblance_info->nlm_imblance_lum3_flag1_r & 0x7ff) << 10));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA13,
		(imblance_info->nlm_imblance_faceRmax & 0xffff) |
		((imblance_info->nlm_imblance_faceRmin & 0xffff) << 16));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA14,
		(imblance_info->nlm_imblance_faceBmax & 0xffff) |
		((imblance_info->nlm_imblance_faceBmin & 0xffff) << 16));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA15,
		(imblance_info->nlm_imblance_faceGmax & 0xffff) |
		((imblance_info->nlm_imblance_faceGmin & 0xffff) << 16));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA16,
		(imblance_info->nlm_imblance_hv_edge_thr[1] & 0xff) |
		((imblance_info->nlm_imblance_hv_edge_thr[2] & 0xff) << 8) |
		((imblance_info->nlm_imblance_slash_edge_thr[2] & 0xff) << 16) |
		((imblance_info->nlm_imblance_slash_edge_thr[1] & 0xff) << 24));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA17,
		((imblance_info->nlm_imblance_hv_flat_thr[2] & 0x3ff) << 16) |
		(imblance_info->nlm_imblance_hv_flat_thr[1] & 0x3ff));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA18,
		((imblance_info->nlm_imblance_slash_flat_thr[2] & 0x3ff) << 16) |
		(imblance_info->nlm_imblance_slash_flat_thr[1] & 0x3ff));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA19,
		(imblance_info->nlm_imblance_S_baohedu[1][0] & 0xff) |
		((imblance_info->nlm_imblance_S_baohedu[2][0] & 0xff) << 8) |
		((imblance_info->nlm_imblance_S_baohedu[1][1] & 0xff) << 16) |
		((imblance_info->nlm_imblance_S_baohedu[2][1] & 0xff) << 24));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA20,
		((imblance_info->nlm_imblance_lum2_flag3_r & 0x7ff) << 16) |
		(imblance_info->nlm_imblance_lum1_flag3_r & 0x7ff));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA21,
		((imblance_info->imblance_sat_lumth & 0x3ff) << 16) |
		(imblance_info->nlm_imblance_lum3_flag3_r & 0x7ff));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA22,
		((imblance_info->nlm_imblance_diff[2] & 0x3ff) << 16) |
		(imblance_info->nlm_imblance_diff[1] & 0x3ff));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA23,
		((imblance_info->nlm_imblance_ff_wt1 & 0x3ff) << 16) |
		(imblance_info->nlm_imblance_ff_wt0 & 0x3ff));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA24,
		((imblance_info->nlm_imblance_ff_wt3 & 0x3ff) << 16) |
		(imblance_info->nlm_imblance_ff_wt2 & 0x3ff));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA25,
		(imblance_info->nlm_imblance_ff_wr0 & 0xff) |
		((imblance_info->nlm_imblance_ff_wr1 & 0xff) << 8) |
		((imblance_info->nlm_imblance_ff_wr2 & 0xff) << 16) |
		((imblance_info->nlm_imblance_ff_wr3 & 0xff) << 24));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA26,
		(imblance_info->nlm_imblance_ff_wr4 & 0xff) |
		((imblance_info->imblance_radial_1D_coef_r0 & 0xff) << 8) |
		((imblance_info->imblance_radial_1D_coef_r1 & 0xff) << 16) |
		((imblance_info->imblance_radial_1D_coef_r2 & 0xff) << 24));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA27,
		(imblance_info->imblance_radial_1D_coef_r3 & 0xff) |
		((imblance_info->imblance_radial_1D_coef_r4 & 0xff) << 8) |
		((imblance_info->imblance_radial_1D_protect_ratio_max & 0x7ff) << 16));

	center_x = in_width >> 1;
	center_y = in_height >> 1;
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA28,
		((center_x & 0xffff) << 16) | (center_y & 0xffff));

	if (imblance_info->radius_base == 0)
		imblance_info->radius_base = 1024;
	radius = (in_height + in_width)
		* imblance_info->imblance_radial_1D_radius_thr_factor
		/ imblance_info->radius_base;
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA29, (radius & 0xffff));

	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA31,
		(imblance_info->med_diff[0] & 0x3ff ) |
		((imblance_info->med_diff[1] & 0x3ff) << 10) |
		((imblance_info->med_diff[2] & 0x3ff) << 20) |
		((imblance_info->grgb_mode & 0x1) << 30));
	DCAM_REG_WR(idx, DCAM_NLM_IMBLANCE_PARA32,
		((imblance_info->gb_ratio & 0xffff) << 16) |
		(imblance_info->gr_ratio & 0xffff));

	return ret;
}

int dcam_k_save_vst_ivst(struct dcam_dev_param *p)
{
	int ret = 0;
	uint32_t buf_len = 0;
	unsigned long utab_addr = 0;
	uint32_t *vst_ivst_buf = NULL;
	struct isp_dev_nlm_info_v2 *nlm_info2 = NULL;

	if (p == NULL)
		return 0;

	nlm_info2 = &p->nlm_info2;
	if (nlm_info2->vst_bypass == 0 && nlm_info2->vst_table_addr) {
		buf_len = ISP_VST_IVST_NUM2 * 4;
		if (nlm_info2->vst_len < (ISP_VST_IVST_NUM2 * 4))
			buf_len = nlm_info2->vst_len;

		vst_ivst_buf = p->vst_buf;
		utab_addr = (unsigned long)nlm_info2->vst_table_addr;
		pr_debug("vst table addr 0x%lx\n", utab_addr);
		ret = copy_from_user((void *)vst_ivst_buf, (void __user *)utab_addr, buf_len);
	}

	if (nlm_info2->ivst_bypass == 0 && nlm_info2->ivst_table_addr) {
		buf_len = ISP_VST_IVST_NUM2 * 4;
		if (nlm_info2->ivst_len < (ISP_VST_IVST_NUM2 * 4))
			buf_len = nlm_info2->ivst_len;

		vst_ivst_buf = p->ivst_buf;
		utab_addr = (unsigned long)nlm_info2->ivst_table_addr;
		pr_debug("vst table addr 0x%lx\n", utab_addr);
		ret = copy_from_user((void *)vst_ivst_buf, (void __user *)utab_addr, buf_len);
	}
	return ret;
}

int dcam_k_cfg_nlm(struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	uint32_t size = 0;
	void *pcpy;
	FUNC_DCAM_PARAM sub_func = NULL;

	switch (param->property) {
	case ISP_PRO_NLM_BLOCK:
		pcpy = (void *)&p->nlm_info2;
		size = sizeof(struct isp_dev_nlm_info_v2);
		sub_func = dcam_k_nlm_block;
		break;
	case ISP_PRO_NLM_IMBLANCE:
		pcpy = (void *)&p->nlm_imblance2;
		size = sizeof(struct isp_dev_nlm_imblance_v2);
		sub_func = dcam_k_nlm_imblance;
		break;
	default:
		pr_err("fail to support property %d\n", param->property);
		return -EINVAL;
	}

	if (p->offline == 0) {
		ret = copy_from_user(pcpy, param->property_param, size);
		if (ret) {
			pr_err("fail to copy from user ret=0x%x\n", (unsigned int)ret);
			return -EPERM;
		}

		if (ISP_PRO_NLM_BLOCK == param->property) {
			ret = dcam_k_save_vst_ivst(p);
			if (ret) {
				pr_err("fail to copy from user ret=0x%x\n", (unsigned int)ret);
				return -EPERM;
			}
		}

		if (p->idx == DCAM_HW_CONTEXT_MAX || param->scene_id == PM_SCENE_CAP)
			return 0;
		if (g_dcam_bypass[p->idx] & (1 << _E_NLM)) {
			p->nlm_info2.bypass = 1;
			p->nlm_info2.vst_bypass = 1;
			p->nlm_info2.ivst_bypass = 1;
		}

		if (g_dcam_bypass[p->idx] & (1 << _E_GRGB))
			p->nlm_imblance2.nlm_imblance_bypass = 1;
		ret = sub_func(p);
	} else {
		mutex_lock(&p->param_lock);
		ret = copy_from_user(pcpy, param->property_param, size);
		if (ret) {
			mutex_unlock(&p->param_lock);
			pr_err("fail to copy from user ret=0x%x\n", (unsigned int)ret);
			return -EPERM;
		}
		if (ISP_PRO_NLM_BLOCK == param->property) {
			ret = dcam_k_save_vst_ivst(p);
			if (ret) {
				pr_err("fail to copy from user ret=0x%x\n", (unsigned int)ret);
				return -EPERM;
			}
		}

		mutex_unlock(&p->param_lock);
	}

	return ret;

}
