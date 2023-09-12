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

#ifndef _ALG_SLICE_CALC_H
#define _ALG_SLICE_CALC_H

#include "isp_interface.h"
#include "alg_isp_overlap.h"

#define ALG_REGIONS_NUM               5
#define DEC_OVERLAP                   4
#define REC_OVERLAP                   2
#define AFBC_PADDING_W_YUV420_scaler  32
#define AFBC_PADDING_H_YUV420_scaler  8
#define AFBC_PADDING_W_YUV420_3dnr    32
#define AFBC_PADDING_H_YUV420_3dnr    8
#define AFBC_PADDING_W_BAYER          32
#define AFBC_PADDING_H_BAYER          8

enum {
	SCALER_BASE = -1,
	SCALER_DCAM_PRV = 0,
	SCALER_ISP_PRV_CAP = 1,
	SCALER_ISP_VID = 2,
	SCALER_DCAM_CAP = 3
};

struct alg_block_calc {
	uint32_t left;
	uint32_t right;
	uint32_t up;
	uint32_t down;
};

struct alg_overlap_info {
	uint32_t ov_left;
	uint32_t ov_right;
	uint32_t ov_up;
	uint32_t ov_down;
};

struct alg_region_info {
	uint32_t sx;
	uint32_t ex;
	uint32_t sy;
	uint32_t ey;
};

struct alg_slice_regions {
	uint32_t rows;
	uint32_t cols;
	struct alg_region_info regions[ALG_REGIONS_NUM];
};

struct alg_fetch_region {
	uint32_t image_w;
	uint32_t image_h;
	uint32_t slice_w;
	uint32_t slice_h;
	uint32_t overlap_left;
	uint32_t overlap_right;
	uint32_t overlap_up;
	uint32_t overlap_down;
};

struct alg_fetch_region_context {
	uint32_t s_row;
	uint32_t e_row;
	uint32_t s_col;
	uint32_t e_col;
	uint32_t overlap_left;
	uint32_t overlap_right;
	uint32_t overlap_up;
	uint32_t overlap_down;
};

struct scaler_overlap_t {
	uint16_t overlap_up;
	uint16_t overlap_down;
	uint16_t overlap_left;
	uint16_t overlap_right;
};

struct alg_slice_scaler_overlap {
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
	int8_t scaler_id;

	int slice_overlap_after_sclaer;
	uint16_t slice_overlapleft_after_sclaer;
	uint16_t slice_overlapright_after_sclaer;
	int flag;

	int dec_online_bypass;
	int layerNum;

	/* driver add for scaler tap */
	uint8_t scaler_y_hor_tap;
	uint8_t scaler_y_ver_tap; /*Y Vertical tap of scaling*/
	uint8_t scaler_uv_hor_tap;
	uint8_t scaler_uv_ver_tap;

	struct scaler_overlap_t input_scaler_overlap;
	struct scaler_overlap_t output_scaler_overlap[PIPE_MAX_SLICE_NUM];
	struct slice_drv_scaler_phase_info phase[PIPE_MAX_SLICE_NUM];

	struct alg_region_info region_input[PIPE_MAX_SLICE_NUM];
	struct alg_region_info region_output[PIPE_MAX_SLICE_NUM];
	struct yuvscaler_param_t *frameParam;
	struct yuvscaler_param_t frameParamObj;
	struct yuvscaler_param_t sliceParam[PIPE_MAX_SLICE_NUM];
};

struct THUMBSLICEINFO_T {
	uint16_t totalcol;
	uint16_t totalrow;
	uint16_t cur_col;
	uint16_t cur_row;
	uint16_t slicewidth;
	uint16_t sliceheight;

	uint16_t trimx;
	uint16_t trimy;
	uint16_t deci_factor_x;
	uint16_t deci_factor_y;

	uint16_t scalerswitch;

	uint16_t trim0startrow;
	uint16_t trim0startcol;
	uint16_t trim0endrow;
	uint16_t trim0endcol;

	uint16_t trim0slice_col;
	uint16_t trim0slice_row;
	uint16_t trim0realiw;
	uint16_t trim0realih;
	uint16_t realih;
	uint16_t realiw;
	uint16_t realoh;
	uint16_t realow;
};

struct THUMBINFO_T {
	struct THUMBSLICEINFO_T thumbsliceinfo[PIPE_MAX_SLICE_NUM];
};

struct CONFIGINFO_T {
	uint16_t iw,ih,ow,oh,outformat;
	uint8_t thumbnailscaler_pipeline_pos;
	uint16_t thumbnailscaler_phaseX,thumbnailscaler_phaseY;

	uint16_t thumbnailscaler_base_align;

	uint8_t thumbnailscaler_trim0_en;
	uint16_t thumbnailscaler_trimstartrow;
	uint16_t thumbnailscaler_trimstartcol;
	uint16_t thumbnailscaler_trimsizeX;
	uint16_t thumbnailscaler_trimsizeY;
	uint8_t scaler_frame_deci;

	uint16_t thumbnailscaler_trimendrow;
	uint16_t thumbnailscaler_trimendcol;
	uint16_t trim0slice_totalcol;
	uint16_t trim0slice_totalrow;
};

struct thumbinfo_phasenum_y {
	uint16_t phaseup;
	uint16_t phasedown;
	uint16_t phaseleft;
	uint16_t phaseright;
	int16_t numup;
	int16_t numdown;
	int16_t numleft;
	int16_t numright;
};

struct thumbinfo_phasenum_uv {
	uint16_t phaseup;
	uint16_t phasedown;
	uint16_t phaseleft;
	uint16_t phaseright;
	int16_t numup;
	int16_t numdown;
	int16_t numleft;
	int16_t numright;
};

struct thumbinfo_trimcoordinate_uv {
	uint16_t trim_s_col;
	uint16_t trim_e_col;
	uint16_t trim_width;
	uint16_t trim_s_row;
	uint16_t trim_e_row ;
	uint16_t trim_height;
};

struct thumbinfo_trimcoordinate_y {
	uint16_t trim_s_col;
	uint16_t trim_e_col;
	uint16_t trim_width;
	uint16_t trim_s_row;
	uint16_t trim_e_row ;
	uint16_t trim_height;
};

struct TH_infophasenum {
	struct thumbinfo_phasenum_y thumbinfo_phasenum_yid[PIPE_MAX_SLICE_NUM];
	struct thumbinfo_phasenum_uv thumbinfo_phasenum_uvid[PIPE_MAX_SLICE_NUM];
	struct thumbinfo_trimcoordinate_y thumbinfo_trimcoordinate_yid[PIPE_MAX_SLICE_NUM];
	struct thumbinfo_trimcoordinate_uv thumbinfo_trimcoordinate_uvid[PIPE_MAX_SLICE_NUM];
};

struct thumbnailscaler_param_t {
	struct THUMBINFO_T thumbinfo;
	struct CONFIGINFO_T configinfo;
	struct TH_infophasenum th_infophasenum;
};

struct thumbnailscaler_context {
	int src_width;
	int src_height;
	int offlineSliceWidth;
	int offlineSliceHeight;
};

struct slice_drv_overlap_thumbnail_scaler_param {
	int bypass;
	int trim0_en;
	int trim0_start_x;
	int trim0_start_y;
	int trim0_size_x;
	int trim0_size_y;
	int phase_x;
	int phase_y;
	int base_align;
	int out_w;
	int out_h;
	int out_format;
	/* for update thumb_scaler_cfg param */
	int slice_num;
	int pipeline_pos;
	int path_frame_skip;
	int uv_sync_y;
	int y_frame_src_size_hor;
	int y_frame_src_size_ver;
	int y_frame_des_size_hor;
	int y_frame_des_size_ver;
	int uv_frame_src_size_hor;
	int uv_frame_src_size_ver;
	int uv_frame_des_size_hor;
	int uv_frame_des_size_ver;
	int y_deci_hor_en;
	int y_deci_hor_par;
	int y_deci_ver_en;
	int y_deci_ver_par;
	int uv_deci_hor_en;
	int uv_deci_hor_par;
	int uv_deci_ver_en;
	int uv_deci_ver_par;
	int regular_bypass;
	int regular_mode;
	int regular_shrink_y_range;
	int regular_shrink_uv_range;
	int regular_shrink_y_offset;
	int regular_shrink_uv_offset;
	int regular_shrink_uv_dn_th;
	int regular_shrink_uv_up_th;
	int regular_shrink_y_dn_th;
	int regular_shrink_y_up_th;
	int regular_effect_v_th;
	int regular_effect_u_th;
	int regular_effect_y_th;
};

struct THUMB_SLICE_PARAM_T {
	int id;
	int width;
	int height;

	int start_col;
	int start_row;
	int end_col;
	int end_row;

	int overlap_left;
	int overlap_right;
	int overlap_up;
	int overlap_down;
};

struct SliceWndinfo {
	int s_row;
	int e_row;
	int s_col;
	int e_col;
	int overlap_left;
	int overlap_right;
	int overlap_up;
	int overlap_down;
};

struct Sliceinfo {
	int rows;
	int cols;
	struct SliceWndinfo slices[16];
	int offlineSliceWidth;
	int offlineSliceHeight;
};

struct thumbnailscaler_slice {
	uint32_t bypass;
	uint32_t slice_size_before_trim_hor;
	uint32_t slice_size_before_trim_ver;
	uint32_t y_trim0_start_hor;
	uint32_t y_trim0_start_ver;
	uint32_t y_trim0_size_hor;
	uint32_t y_trim0_size_ver;
	uint32_t uv_trim0_start_hor;
	uint32_t uv_trim0_start_ver;
	uint32_t uv_trim0_size_hor;
	uint32_t uv_trim0_size_ver;
	uint32_t y_slice_src_size_hor;
	uint32_t y_slice_src_size_ver;
	uint32_t y_slice_des_size_hor;
	uint32_t y_slice_des_size_ver;
	uint32_t uv_slice_src_size_hor;
	uint32_t uv_slice_src_size_ver;
	uint32_t uv_slice_des_size_hor;
	uint32_t uv_slice_des_size_ver;
	uint32_t y_init_phase_hor;
	uint32_t y_init_phase_ver;
	uint32_t uv_init_phase_hor;
	uint32_t uv_init_phase_ver;
	uint32_t chk_sum_clr;
};

struct thumbscaler_info {
	struct THUMBINFO_T  thumbinfo;
	struct CONFIGINFO_T configinfo;
};

struct thumbscaler_this {
	struct TH_infophasenum th_infophasenum;
	struct Sliceinfo inputSliceList;
	struct Sliceinfo inputSliceList_overlap;
	int sumslice;
	struct thumbnailscaler_slice sliceParam[PIPE_MAX_SLICE_NUM];
};

struct alg_slice_drv_overlap {
	/************************************************************************/
	/* img_type:                                                            */
	/* 0:bayer 1:rgb 2:yuv444 3:yuv422 4:yuv420 5:yuv400                    */
	/* 6:FBC bayer 7:FBC yuv420                                             */
	/************************************************************************/
	int img_w;
	int img_h;
	int img_type;

	int input_layer_w;
	int input_layer_h;
	int input_layer_id;
	int img_src_w;
	int img_src_h;
	int uw_sensor;

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

	int offline_slice_mode;
	/************************************************************************/
	/* user define overlap                                                  */
	/************************************************************************/
	int offlineCfgOverlap_en;
	int offlineCfgOverlap_left;
	int offlineCfgOverlap_right;
	int offlineCfgOverlap_up;
	int offlineCfgOverlap_down;

	//rgb
	struct ltm_rgb_stat_param_t ltm_sat;

	//yuv rec
	int ynr_bypass;
	int cnr_bypass;
	int pyramid_rec_bypass;
	int layerNum;

	//yuv
	int dewarping_bypass;
	int dewarping_width;
	int dewarping_height;
	int post_cnr_bypass;
	int nr3d_bd_bypass;
	int nr3d_bd_FBC_en;
	int yuv420_to_rgb10_bypass;
	int ee_bypass;
	int cnr_new_bypass;

	//scaler
	int scaler_input_format;//3:422 4:420
	struct alg_slice_scaler_overlap scaler1;
	struct alg_slice_scaler_overlap scaler2;
	struct slice_drv_overlap_thumbnail_scaler_param thumbnailscaler;
	struct thumbscaler_this thumbnail_scaler;
	struct thumbscaler_info thumbnail_scaler_lite;

	/************************************************************************/
	/*  output                                                              */
	/************************************************************************/
	int slice_rows;
	int slice_cols;
	struct alg_region_info slice_region[PIPE_MAX_SLICE_NUM];
	struct alg_overlap_info slice_overlap[PIPE_MAX_SLICE_NUM];

	//fetch0, add overlap
	struct alg_region_info fecth0_slice_region[MAX_PYR_DEC_LAYER_NUM][PIPE_MAX_SLICE_NUM];
	struct alg_overlap_info fecth0_slice_overlap[MAX_PYR_DEC_LAYER_NUM][PIPE_MAX_SLICE_NUM];

	//fetch1, add overlap
	struct alg_region_info fecth1_slice_region[ISP_PYR_DEC_LAYER_NUM][PIPE_MAX_SLICE_NUM];

	//store rec, add overlap
	struct alg_region_info store_rec_slice_region[ISP_PYR_DEC_LAYER_NUM][PIPE_MAX_SLICE_NUM];
	struct alg_overlap_info store_rec_slice_overlap[ISP_PYR_DEC_LAYER_NUM][PIPE_MAX_SLICE_NUM];

	//store rec, crop overlap
	struct alg_overlap_info store_rec_slice_crop_overlap[ISP_PYR_DEC_LAYER_NUM][PIPE_MAX_SLICE_NUM];

	//slice number
	int slice_number[MAX_PYR_DEC_LAYER_NUM];
};

/*
* img_type: 0:bayer 1:rgb 2:yuv444 3:yuv422 4:yuv420 5:yuv400 6:FBC bayer 7:FBC yuv420
*/
struct alg_dec_offline_overlap {
	/*    input param    */
	uint32_t img_w;
	uint32_t img_h;
	uint32_t img_type;
	uint32_t crop_en;
	uint32_t crop_mode;
	uint32_t crop_sx;
	uint32_t crop_sy;
	uint32_t crop_w;
	uint32_t crop_h;
	uint32_t slice_w;
	uint32_t slice_h;
	uint32_t dct_bypass;
	uint32_t dec_offline_bypass;
	uint32_t layerNum;
	uint32_t MaxSliceWidth;
	uint32_t slice_mode;

	/*    output param    */
	uint32_t slice_rows;
	uint32_t slice_cols;

	struct alg_region_info fecth_dec_region[ISP_PYR_DEC_LAYER_NUM][PIPE_MAX_SLICE_NUM];
	struct alg_overlap_info fecth_dec_overlap[ISP_PYR_DEC_LAYER_NUM][PIPE_MAX_SLICE_NUM];
	struct alg_region_info store_dec_region[MAX_PYR_DEC_LAYER_NUM][PIPE_MAX_SLICE_NUM];
	struct alg_overlap_info store_dec_overlap[MAX_PYR_DEC_LAYER_NUM][PIPE_MAX_SLICE_NUM];
};

void alg_slice_calc_drv_overlap(struct alg_slice_drv_overlap *param_ptr);
void alg_slice_calc_dec_offline_overlap(struct alg_dec_offline_overlap *param_ptr);

#endif
