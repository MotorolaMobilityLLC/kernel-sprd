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

#ifndef _ALG_ISP_OVERLAP_H_
#define _ALG_ISP_OVERLAP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cam_types.h"
#include "alg_common_calc.h"

#define SCL_UP_MAX                      10
#define SCL_DOWN_MAX                    10
#define PIPE_MAX_SLICE_NUM              5
#define ISP_DRV_REGIONS_NUM             5
#define FBC_PADDING_W_BAYER             128
#define FBC_PADDING_H_BAYER             4
#define FBC_PADDING_W_YUV420_scaler     32
#define FBC_PADDING_H_YUV420_scaler     8
#define FBC_PADDING_W_YUV420_3dnr       256
#define FBC_PADDING_H_YUV420_3dnr       4

#ifdef ISP_OVERLAP_DEBUG_ON
#define ISP_OVERLAP_DEBUG pr_info
#else
#define ISP_OVERLAP_DEBUG pr_debug
#endif

enum alg_isp_overlap_version {
	ALG_ISP_DYN_OVERLAP_NONE,
	ALG_ISP_OVERLAP_VER_1, /* qogirl6 */
	ALG_ISP_OVERLAP_VER_2, /* qogirN6pro */
	ALG_ISP_OVERLAP_VER_MAX,
};

struct isp_fw_slice_pos {
	uint32_t start_col;
	uint32_t start_row;
	uint32_t end_col;
	uint32_t end_row;
};

struct isp_fw_slice_overlap {
	uint32_t overlap_up;
	uint32_t overlap_down;
	uint32_t overlap_left;
	uint32_t overlap_right;
};

struct isp_fw_scaler_slice {
	uint32_t bypass;
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
	uint32_t chk_sum_clr;
};

enum SCALER_ID {
	SCALER_CAP_PRE,
	SCALER_VID,
	SCALER_NUM,
};

struct ltm_rgb_stat_param_t {
	uint8_t bypass;

	uint8_t strength;
	uint8_t ch_G_Y;
	uint8_t region_est_en;
	uint8_t text_point_thres;
	uint8_t text_proportion;
	uint8_t tile_num_auto;
	uint8_t tile_num_row;
	uint8_t tile_num_col;

	uint8_t binning_en;
	uint8_t cropUp_stat;
	uint8_t cropDown_stat;
	uint8_t cropLeft_stat;
	uint8_t cropRight_stat;

	uint16_t clipLimit;
	uint16_t clipLimit_min;
	uint16_t tile_width_stat;
	uint16_t tile_height_stat;
	uint16_t frame_width_stat;
	uint16_t frame_height_stat;
	uint16_t slice_width_stat;
	uint16_t slice_height_stat;
	uint32_t tile_size_stat;

	uint32_t ***tileHists;
	uint32_t ***tileHists_temp;
	uint8_t  ***flat_bin_flag;
	uint8_t *texture_thres_table;

	uint8_t frame_id;
	int in_bitwidth;
	int out_bitwidth;
	uint8_t frame_flag;
	char *stat_file_out;
	char *tile_file_out;
};

enum STORE_OUTPUT_FORMAT
{
	YUV_OUT_UYVY_422,
	YUV_OUT_Y_UV_422,
	YUV_OUT_Y_VU_422,
	YUV_OUT_Y_U_V_422,
	YUV_OUT_Y_UV_420,
	YUV_OUT_Y_VU_420,
	YUV_OUT_Y_U_V_420,
	YUV_OUT_Y_400 = 7,
	NORMAL_RAW10 = 7,
	FULL_RGB8,
};

struct pipe_overlap_info {
	int ov_left;
	int ov_right;
	int ov_up;
	int ov_down;
};

struct slice_drv_scaler_slice_init_context {
	int slice_index;
	int rows;
	int cols;
	int slice_row_no;
	int slice_col_no;
	int slice_w;
	int slice_h;
};

struct SliceWnd {
	int s_row;
	int e_row;
	int s_col;
	int e_col;
	int overlap_left;
	int overlap_right;
	int overlap_up;
	int overlap_down;
};

struct isp_drv_region_fetch_context {
	int s_row;
	int e_row;
	int s_col;
	int e_col;
	int overlap_left;
	int overlap_right;
	int overlap_up;
	int overlap_down;
};

struct isp_drv_region_fetch_t {
	int image_w;
	int image_h;
	int slice_w;
	int slice_h;
	int overlap_left;
	int overlap_right;
	int overlap_up;
	int overlap_down;
};

struct isp_block_drv_t {
	int left;
	int right;
	int up;
	int down;
};

struct isp_drv_region_t {
	int sx;
	int ex;
	int sy;
	int ey;
};

struct isp_drv_regions_t {
	int rows;
	int cols;
	struct isp_drv_region_t regions[ISP_DRV_REGIONS_NUM];
};

struct pipe_overlap_context {
	int frameWidth;
	int frameHeight;
	int pixelFormat;
};

struct slice_drv_overlap_info {
	int ov_left;
	int ov_right;
	int ov_up;
	int ov_down;
};

struct slice_drv_scaler_phase_info {
	int init_phase_hor;
	int init_phase_ver;
};

struct slice_drv_region_info {
	int sx;
	int ex;
	int sy;
	int ey;
};

struct slice_drv_overlap_scaler_param {
	/*in*/
	int bypass;
	int trim_eb;
	int trim_start_x;
	int trim_start_y;
	int trim_size_x;
	int trim_size_y;
	int deci_x_eb;
	int deci_y_eb;
	int deci_x;
	int deci_y;
	int scaler_en;
	int32_t scl_init_phase_hor;
	int32_t scl_init_phase_ver;
	int des_size_x;
	int des_size_y;
	int yuv_output_format;
	int FBC_enable;
	int output_align_hor;

	/*out*/
	struct slice_drv_scaler_phase_info phase[PIPE_MAX_SLICE_NUM];
	struct slice_drv_region_info region_input[PIPE_MAX_SLICE_NUM];
	struct slice_drv_region_info region_output[PIPE_MAX_SLICE_NUM];
	struct yuvscaler_param_t *frameParam;
	struct yuvscaler_param_t frameParamObj;
	struct yuvscaler_param_t sliceParam[PIPE_MAX_SLICE_NUM];
};

struct slice_drv_overlap_param_t {
	/************************************************************************/
	/* img_type:                                                            */
	/* 0:bayer 1:rgb 2:yuv444 3:yuv422 4:yuv420 5:yuv400                    */
	/* 6:FBC bayer 7:FBC yuv420                                             */
	/************************************************************************/
	int img_w;
	int img_h;
	int img_type;

	/************************************************************************/
	/* 如果有crop行为，则输入pipeline的图像大小为crop_w,crop_h              */
	/************************************************************************/
	int crop_en;
	int crop_mode;
	int crop_sx;
	int crop_sy;
	int crop_w;
	int crop_h;

	/************************************************************************/
	/* on whaleK slice_h >= img_h or slice_h >= crop_h                      */
	/************************************************************************/
	int slice_w;
	int slice_h;
	/************************************************************************/
	/* user define overlap                                                  */
	/************************************************************************/
	int offlineCfgOverlap_en;
	int offlineCfgOverlap_left;
	int offlineCfgOverlap_right;
	int offlineCfgOverlap_up;
	int offlineCfgOverlap_down;

	/*bayer*/
	int nlm_bypass;
	int imbalance_bypass;
	int raw_gtm_stat_bypass;
	int raw_gtm_map_bypass;
	int cfa_bypass;

	/*rgb*/
	int ltmsta_rgb_bypass;
	int ltmsta_rgb_binning_en;

	/*yuv*/
	int yuv420to422_bypass;
	int nr3d_bd_bypass;
	int nr3d_bd_FBC_en;
	int pre_cnr_bypass;
	int ynr_bypass;
	int ee_bypass;
	int cnr_new_bypass;
	int post_cnr_bypass;
	int iir_cnr_bypass;

	/*scaler*/
	int scaler_input_format; /*3:422 4:420*/
	struct slice_drv_overlap_scaler_param scaler1;
	struct slice_drv_overlap_scaler_param scaler2;

	/************************************************************************/
	/*  output                                                              */
	/************************************************************************/
	int slice_rows;
	int slice_cols;
	struct slice_drv_region_info slice_region[PIPE_MAX_SLICE_NUM];
	struct slice_drv_overlap_info slice_overlap[PIPE_MAX_SLICE_NUM];
};

int alg_isp_get_dynamic_overlap(void *cfg_slice_in, void*slc_ctx);
int alg_isp_init_yuvscaler_slice(void *slc_cfg_input, void *slc_ctx, struct isp_fw_scaler_slice (*slice_param)[PIPE_MAX_SLICE_NUM]);

void core_drv_nr3d_init_block(struct isp_block_drv_t *block_ptr);
void core_drv_ltmsta_init_block(struct isp_block_drv_t *block_ptr);
void core_drv_ee_init_block(struct isp_block_drv_t *block_ptr);
void core_drv_cnrnew_init_block(struct isp_block_drv_t *block_ptr);

#endif
