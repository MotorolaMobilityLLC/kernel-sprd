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

#ifndef _ISP_SLICE_HEADER_
#define _ISP_SLICE_HEADER_

#include "isp_drv.h"

#define SLICE_NUM_MAX                  4

enum slice_path_index {
	SLICE_PATH_PRE,
	SLICE_PATH_VID,
	SLICE_PATH_CAP,
	SLICE_PATH_MAX,
};

enum isp_fetchYUV_format {
	ISP_FETCHYUV_YUV422_3FRAME = 0,
	ISP_FETCHYUV_YUYV,
	ISP_FETCHYUV_UYVY,
	ISP_FETCHYUV_YVYU,
	ISP_FETCHYUV_VYUY,
	ISP_FETCHYUV_YUV422_2FRAME,
	ISP_FETCHYUV_YVU422_2FRAME,
	ISP_FETCHYUV_YUV420_2FRAME = 10,
	ISP_FETCHYUV_YVU420_2FRAME,
	ISP_FETCHYUV_FORMAT_MAX
};

enum isp_store_format {
	ISP_STORE_UYVY = 0x00,
	ISP_STORE_YUV422_2FRAME,
	ISP_STORE_YVU422_2FRAME,
	ISP_STORE_YUV422_3FRAME,
	ISP_STORE_YUV420_2FRAME,
	ISP_STORE_YVU420_2FRAME,
	ISP_STORE_YUV420_3FRAME,
	ISP_STORE_RAW10,
	ISP_STORE_FULL_RGB8,
	ISP_STORE_FORMAT_MAX
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
	struct slice_pos_info slice_raw_pos_array[SLICE_NUM_MAX];
	struct slice_overlap_info slice_raw_overlap_array[SLICE_NUM_MAX];
	struct slice_pos_info slice_yuv_pos_array[SLICE_NUM_MAX];
	struct slice_overlap_info slice_yuv_overlap_array[SLICE_NUM_MAX];
	struct slice_pitch pitch;
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
	uint32_t raw_overlap_up;
	uint32_t raw_overlap_down;
	uint32_t raw_overlap_left;
	uint32_t raw_overlap_right;
	uint32_t yuv_overlap_up;
	uint32_t yuv_overlap_down;
	uint32_t yuv_overlap_left;
	uint32_t yuv_overlap_right;
};

struct slice_fetch_info {
	struct slice_img_size size;
	struct slice_addr addr;
	uint32_t mipi_byte_rel_pos;
	uint32_t mipi_word_num;
};

struct slice_store_info {
	struct slice_img_size size;
	struct slice_border border;
	struct slice_addr addr;
};

struct slice_3dnr_mem_ctrl {
	uint32_t start_col;
	uint32_t start_row;
	uint32_t first_line_mode;
	uint32_t last_line_mode;
	struct slice_img_size src;
	struct slice_img_size ft_y;
	struct slice_img_size ft_uv;
	struct slice_addr addr;
};

struct slice_3dnr_store_info {
	struct slice_img_size size;
	struct slice_addr addr;
};

struct slice_3dnr_crop_info {
	uint32_t bypass;
	uint32_t start_x;
	uint32_t start_y;
	struct slice_img_size src;
	struct slice_img_size dst;
};

struct slice_dispatch_info {
	struct slice_img_size size;
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

struct slice_nlm_info {
	int row_center;
	int col_center;
};

struct slice_cfa_info {
	uint32_t gbuf_addr_max;
};

struct slice_postcnr_info {
	uint32_t start_row_mod4;
};

struct slice_ynr_info {
	uint32_t start_row;
	uint32_t start_col;
	uint32_t row_center;
	uint32_t col_center;
};

struct slice_noisefilter_info {
	uint32_t seed0;
	uint32_t seed1;
	uint32_t seed2;
	uint32_t seed3;
	uint32_t seed_int;
};

struct slice_context_info {
	struct slice_base_info base_info;
	struct slice_fetch_info fetch_info[SLICE_NUM_MAX];
	struct slice_3dnr_mem_ctrl nr3_mem_ctrl[SLICE_NUM_MAX];
	struct slice_3dnr_store_info store_3dnr_info[SLICE_NUM_MAX];
	struct slice_3dnr_crop_info crop_3dnr_info[SLICE_NUM_MAX];
	struct slice_store_info store_info[SLICE_PATH_MAX][SLICE_NUM_MAX];
	struct slice_dispatch_info dispatch_info[SLICE_NUM_MAX];
	struct slice_scaler_info scaler_info[SLICE_PATH_MAX][SLICE_NUM_MAX];
	struct slice_postcnr_info postcnr_info[SLICE_NUM_MAX];
	struct slice_ynr_info ynr_info[SLICE_NUM_MAX];
	struct slice_noisefilter_info noisefilter_info[SLICE_NUM_MAX];
	struct slice_nlm_info nlm_info[SLICE_NUM_MAX];
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

struct slice_3dnr_info {
	uint32_t need_slice;
	uint32_t cur_frame_num;
	int mv_x;
	int mv_y;
	struct slice_store_path fetch_3dnr_frame;
	struct slice_store_path store_3dnr_frame;
};

struct slice_param_in {
	uint32_t com_idx;
	uint32_t is_raw_capture;
	uint32_t fetch_format;
	uint32_t cap_slice_need;
	uint32_t *fmcu_addr_vir;
	struct slice_addr fetch_addr;
	struct slice_img_size img_size;
	struct slice_scaler_path scaler_frame[SLICE_PATH_MAX];
	struct slice_store_path store_frame[SLICE_PATH_MAX];
	struct slice_3dnr_info nr3_info;
	uint32_t nlm_col_center;
	uint32_t nlm_row_center;
	uint32_t ynr_center_x;
	uint32_t ynr_center_y;
};

int sprd_isp_slice_fmcu_slice_cfg(void *fmcu_handler,
struct slice_param_in *in_ptr, uint32_t *fmcu_num);
int sprd_isp_slice_fmcu_init(void **fmcu_handler);
int sprd_isp_slice_fmcu_deinit(void *fmcu_handler);

#endif
