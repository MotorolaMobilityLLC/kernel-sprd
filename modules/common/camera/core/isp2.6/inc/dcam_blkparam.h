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

#ifndef _DCAM_BLKPARAM_H_
#define _DCAM_BLKPARAM_H_

#include "isp_hw.h"

enum dcam_gtm_param_type {
	DCAM_GTM_PARAM_PRE,
	DCAM_GTM_PARAM_CAP,
	DCAM_GTM_PARAM_MAX,
};

struct dcam_dev_lsc_param {
	uint32_t update;
	uint32_t load_trigger;
	uint32_t weight_tab_size;
	uint32_t weight_tab_size_x;
	uint32_t weight_tab_size_y;
	void *weight_tab;
	void *weight_tab_x;
	void *weight_tab_y;
	struct mutex lsc_lock;
	struct camera_buf buf;
	struct dcam_dev_lsc_info lens_info;
};

struct dcam_dev_gamma_param_v1 {
	uint32_t buf_sel;
	struct isp_dev_gamma_info_v1 gamma_info;
};

struct dcam_dev_blc_param {
	struct dcam_dev_blc_info blc_info;
};

struct dcam_dev_rgb_param {
	struct dcam_dev_rgb_gain_info gain_info;
	struct dcam_dev_rgb_dither_info rgb_dither;
};

struct dcam_dev_hist_param {
	uint32_t update;
	struct dcam_dev_hist_info bayerHist_info;
};

struct dcam_dev_hist_roi_param {
	uint32_t update;
	struct isp_dev_hist2_info hist_roi_info;
};

struct dcam_dev_aem_param {
	uint32_t mode;
	uint32_t bypass;
	uint32_t update;
	struct dcam_dev_aem_win win_info;
	uint32_t skip_num;
	struct dcam_dev_aem_thr aem_info;
};

struct dcam_dev_afl_param {
	struct isp_dev_anti_flicker_new_info afl_info;
};

struct dcam_dev_awbc_param {
	struct dcam_dev_awbc_info awbc_info;
};

struct dcam_dev_bpc_param {
	struct dcam_bpc_ppi_info bpc_ppi_info;
	union {
		struct dcam_dev_bpc_info bpc_info;
		struct dcam_dev_bpc_info_l3 bpc_info_l3;
	} bpc_param;
};

struct dcam_dev_n6pro_bpc_param {
	struct dcam_bpc_ppi_info ppi_info;
	union {
		struct dcam_dev_bpc_info_v1 bpc_info;
		struct dcam_dev_bpc_info_l3 bpc_info_l3;
	} bpc_param_n6pro;
};

struct dcam_dev_grgb_param {
	struct isp_dev_grgb_info grgb_info;
};

struct dcam_dev_3dnr_param {
	struct dcam_dev_3dnr_me nr3_me;
};

struct dcam_dev_afm_param {
	struct dcam_dev_afm_info af_param;
	struct isp_img_rect win;
	struct isp_img_size win_num;
	uint32_t mode;
	uint32_t bypass;
	uint32_t skip_num;
	uint32_t crop_eb;
	struct isp_img_rect crop_size;
	struct isp_img_size done_tile_num;
};

struct dcam_dev_gtm_param {
	uint32_t update_en;
	uint32_t gtm_calc_mode;
	struct cam_gtm_mapping mapping_info;
	struct dcam_dev_raw_gtm_block_info gtm_info;
};

struct dcam_dev_rgb_gtm_param {
	uint32_t update_en;
	uint32_t gtm_calc_mode;
	struct cam_gtm_mapping mapping_info;
	struct dcam_dev_rgb_gtm_block_info rgb_gtm_info;
};

struct isp_dev_pdaf_info {
	uint32_t bypass;
	uint32_t corrector_bypass;
	uint32_t phase_map_corr_en;
	struct isp_img_size block_size;
	uint32_t grid_mode;
	struct isp_coord win;
	struct isp_coord block;
	struct isp_rrgb gain_upperbound;
	uint32_t phase_txt_smooth;
	uint32_t phase_gfilter;
	uint32_t phase_flat_smoother;
	uint32_t hot_pixel_th[3];
	uint32_t dead_pixel_th[3];
	uint32_t flat_th;
	uint32_t edge_ratio_hv;
	uint32_t edge_ratio_rd;
	uint32_t edge_ratio_hv_rd;
	uint32_t phase_left_addr;
	uint32_t phase_right_addr;
	uint32_t phase_pitch;
	uint32_t pattern_pixel_is_right[PDAF_PPI_NUM];
	uint32_t pattern_pixel_row[PDAF_PPI_NUM];
	uint32_t pattern_pixel_col[PDAF_PPI_NUM];
	uint32_t gain_ori_left[2];
	uint32_t gain_ori_right[2];
	uint32_t extractor_bypass;
	uint32_t mode_sel;
	uint32_t skip_num;
	uint32_t phase_data_dword_num;
	struct isp_rrgb pdaf_blc;
	uint32_t data_ptr_left[2];
	uint32_t data_ptr_right[2];
};

struct dcam_dev_pdaf_param {
	uint32_t bypass;
	uint32_t mode;
	uint32_t skip_num;
	uint32_t pdaf_type;
	struct isp_dev_pdaf_info pdaf_info;
	struct dev_dcam_vc2_control vch2_info;
	struct pdaf_ppi_info ppi_info;
	struct pdaf_roi_info roi_info;
	struct dcam_ppe_ppc_info ppe_ppc_info;
};

struct dcam_dev_param {
	struct mutex param_lock;
	struct img_trim in_size;
	uint32_t idx;/* dcam dev idx */
	void *dev;/* dcam_sw_ctx */
	uint32_t dcam_slice_mode;
	uint32_t offline;
	uint32_t frm_idx;
	uint32_t is_high_fps;
	uint32_t non_zsl_cap;

	struct dcam_dev_lsc_param lsc;
	struct dcam_dev_blc_param blc;
	struct dcam_dev_rgb_param rgb;
	struct dcam_dev_hist_param hist;
	struct dcam_dev_hist_roi_param hist_roi;
	struct dcam_dev_aem_param aem;
	struct dcam_dev_afl_param afl;
	struct dcam_dev_awbc_param awbc;
	struct dcam_dev_bpc_param bpc;
	struct dcam_dev_grgb_param grgb;
	struct dcam_dev_3dnr_param nr3;
	struct dcam_dev_afm_param afm;
	struct dcam_dev_gamma_param_v1 gamma_info_v1;
	struct isp_dev_nlm_imblance_v2 nlm_imblance2;
	struct isp_dev_nlm_info_v2 nlm_info2;
	struct isp_dev_cmc10_info cmc10_info;
	struct isp_dev_cfa_info_v1 cfa_info_v1;
	struct isp_dev_cce_info cce_info;
	struct dcam_dev_gtm_param gtm[DCAM_GTM_PARAM_MAX];
	/* qogirn6pro rgb_gtm blocks*/
	struct dcam_dev_rgb_gtm_param rgb_gtm[DCAM_GTM_PARAM_MAX];
	/* qogirn6pro bpc blocks*/
	struct dcam_dev_n6pro_bpc_param bpc_n6pro;
	struct dcam_dev_lscm_param lscm;
	struct dcam_dev_pdaf_param pdaf;

	uint32_t vst_buf[ISP_VST_IVST_NUM2];
	uint32_t ivst_buf[ISP_VST_IVST_NUM2];
};

typedef int (*FUNC_DCAM_PARAM)(struct dcam_dev_param *param);

#endif
