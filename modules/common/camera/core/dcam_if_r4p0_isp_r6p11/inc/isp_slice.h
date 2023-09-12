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

#ifndef _ISP_SLICE_HEADER_
#define _ISP_SLICE_HEADER_

#include "isp_drv.h"
#include "isp_cfg.h"

#define SLICE_NUM_MAX	3

enum slice_path_index {
	SLICE_PATH_PRE,
	SLICE_PATH_VID,
	SLICE_PATH_CAP,
	SLICE_PATH_MAX,
};

struct slice_img_size {
	uint32_t width;
	uint32_t height;
};

struct slice_addr {
	uint32_t chn0;
	uint32_t chn1;
	uint32_t chn2;
};

struct slice_pitch {
	uint32_t chn0;
	uint32_t chn1;
	uint32_t chn2;
};

struct slice_border {
	uint32_t up_border;
	uint32_t down_border;
	uint32_t left_border;
	uint32_t right_border;
};

struct slice_pos_info {
	uint32_t start_col;
	uint32_t start_row;
	uint32_t end_col;
	uint32_t end_row;
};

struct slice_overlap_info {
	uint32_t overlap_up;
	uint32_t overlap_down;
	uint32_t overlap_left;
	uint32_t overlap_right;
};

struct slice_base_info {
	struct slice_pos_info slice_pos_array[SLICE_NUM_MAX];
	struct slice_overlap_info slice_overlap_array[SLICE_NUM_MAX];
	uint32_t cur_slice_id;
	uint32_t slice_row_num;
	uint32_t slice_col_num;
	uint32_t slice_num;
	uint32_t slice_height;
	uint32_t slice_width;
	uint32_t img_width;
	uint32_t img_height;
	uint32_t store_width;
	uint32_t store_height;
	uint32_t overlap_up;
	uint32_t overlap_down;
	uint32_t overlap_left;
	uint32_t overlap_right;
	uint32_t isp_jpg_cowork;
};

struct slice_lsc_2d_info {
	uint32_t start_col;
	uint32_t start_row;
	uint32_t relative_x;
	uint32_t relative_y;
	uint32_t slice_width;
	uint32_t slice_height;
	uint32_t grid_x_num;
	uint32_t grid_y_num;
	uint16_t q_val[5][2];
	uint32_t grid_num_t;
	uint16_t *grid_buf;
};

struct slice_fetch_info {
	struct slice_img_size size;
	struct slice_addr addr;
	uint32_t mipi_word_num;
	uint32_t mipi_byte_rel_pos;
};

struct slice_store_info {
	struct slice_img_size size;
	struct slice_border border;
	struct slice_addr addr;
};

struct slice_dispatch_info {
	struct slice_img_size size;
	uint32_t bayer_mode;
};

struct slice_scaler_info {
	uint32_t trim0_size_x;
	uint32_t trim0_size_y;
	uint32_t trim0_start_x;
	uint32_t trim0_start_y;
	uint32_t trim1_size_x;
	uint32_t trim1_size_y;
	uint32_t trim1_start_x;
	uint32_t trim1_start_y;
	uint32_t scaler_ip_int;
	uint32_t scaler_ip_rmd;
	uint32_t scaler_cip_int;
	uint32_t scaler_cip_rmd;
	uint32_t scaler_factor_in;
	uint32_t scaler_factor_out;
	uint32_t scaler_ip_int_ver;
	uint32_t scaler_ip_rmd_ver;
	uint32_t scaler_cip_int_ver;
	uint32_t scaler_cip_rmd_ver;
	uint32_t scaler_factor_in_ver;
	uint32_t scaler_factor_out_ver;
	uint32_t src_size_x;
	uint32_t src_size_y;
	uint32_t dst_size_x;
	uint32_t dst_size_y;
	uint32_t scaler_in_width;
	uint32_t scaler_in_height;
	uint32_t scaler_out_width;
	uint32_t scaler_out_height;
	uint32_t chk_sum_clr;
};

struct slice_yuv_param {
	uint32_t id;
	uint32_t width;
	uint32_t height;
	uint32_t start_col;
	uint32_t start_row;
	uint32_t end_col;
	uint32_t end_row;
	uint32_t overlap_hor_left;
	uint32_t overlap_hor_right;
	uint32_t overlap_ver_up;
	uint32_t overlap_ver_down;
};

struct slice_postcnr_info {
	uint32_t start_row_mod4;
};

struct slice_ynr_info {
	uint32_t start_row;
	uint32_t start_col;
};

struct slice_noisefilter_info {
	uint32_t seed0;
	uint32_t seed1;
	uint32_t seed2;
	uint32_t seed3;
	uint32_t seed_int;
};

struct slice_cfa_info {
	uint32_t gbuf_addr_max;
};

struct slice_context_info {
	struct slice_base_info base_info;
	struct slice_lsc_2d_info lsc_2d_info[SLICE_NUM_MAX];
	struct slice_fetch_info fetch_info[SLICE_NUM_MAX];
	struct slice_store_info store_info[SLICE_PATH_MAX][SLICE_NUM_MAX];
	struct slice_dispatch_info dispatch_info[SLICE_NUM_MAX];
	struct slice_scaler_info scaler_info[SLICE_PATH_MAX][SLICE_NUM_MAX];
	struct slice_postcnr_info postcnr_info[SLICE_NUM_MAX];
	struct slice_ynr_info ynr_info[SLICE_NUM_MAX];
	struct slice_noisefilter_info noisefilter_info[SLICE_NUM_MAX];
	struct slice_cfa_info cfa_info[SLICE_NUM_MAX];
};

struct slice_store_path {
	uint32_t format;
	struct slice_addr addr;
	struct slice_img_size size;
};

struct slice_scaler_path {
	uint32_t trim0_size_x;
	uint32_t trim0_size_y;
	uint32_t trim0_start_x;
	uint32_t trim0_start_y;
	uint32_t deci_x;
	uint32_t deci_y;
	uint32_t odata_mode;
	uint32_t scaler_bypass;
	uint32_t scaler_factor_in;
	uint32_t scaler_factor_out;
	uint32_t scaler_out_width;
	uint32_t scaler_out_height;
	uint32_t scaler_ver_factor_in;
	uint32_t scaler_ver_factor_out;
	uint32_t scaler_y_ver_tap;
	uint32_t scaler_uv_ver_tap;
};

struct slice_param_in {
	enum isp_id iid;
	enum isp_scene_id sid;
	enum isp_work_mode mid;
	uint32_t is_raw_capture;
	uint32_t fetch_format;
	uint32_t bayer_mode;
	uint32_t pre_slice_need;
	uint32_t vid_slice_need;
	uint32_t cap_slice_need;
	uint32_t *fmcu_addr_vir;
	struct isp_pipe_dev *isp_dev;
	struct slice_addr fetch_addr;
	struct slice_img_size img_size;
	struct slice_scaler_path scaler_frame[SLICE_PATH_MAX];
	struct slice_store_path store_frame[SLICE_PATH_MAX];
};

int isp_fmcu_slice_cfg(void *fmcu_handler,
		       struct slice_param_in *in_ptr, uint32_t *fmcu_num);
int isp_fmcu_slice_init(void **fmcu_handler);
int isp_fmcu_slice_deinit(void *fmcu_handler);

#endif
