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

#ifndef _ISP_SLICE_H_
#define _ISP_SLICE_H_

#include "isp_fmcu.h"
#include "isp_core.h"
#include "alg_isp_overlap.h"
#include "alg_slice_calc.h"

#define SLICE_NUM_MAX                   5
#define SLICE_W_NUM_MAX                 5
#define SLICE_H_NUM_MAX                 1
#define SLICE_OVERLAP_W_MAX             64

#define YUV_OVERLAP_UP                  46
#define YUV_OVERLAP_DOWN                68
#define YUV_OVERLAP_LEFT                112
#define YUV_OVERLAP_RIGHT               126

#define YUVSCALER_OVERLAP_UP            32
#define YUVSCALER_OVERLAP_DOWN          52
#define YUVSCALER_OVERLAP_LEFT          48
#define YUVSCALER_OVERLAP_RIGHT         68

#define ISP_SLICE_ALIGN_SIZE            2
#define ISP_ALIGNED(size)               (((size) + ISP_SLICE_ALIGN_SIZE - 1) & ~(ISP_SLICE_ALIGN_SIZE - 1))
#define FMCU_PUSH(fmcu, addr, cmd) \
		fmcu->ops->push_cmdq(fmcu, addr, cmd)

struct slice_scaler_info {
	uint32_t out_of_range;
	uint32_t scaler_bypass;
	uint32_t sub_scaler_bypass;
	uint32_t odata_mode;
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
	uint32_t scaler_in_width;
	uint32_t scaler_in_height;
	uint32_t scaler_out_width;
	uint32_t scaler_out_height;
	uint32_t src_size_x;
	uint32_t src_size_y;
	uint32_t dst_size_x;
	uint32_t dst_size_y;
	uint32_t chk_sum_clr;
};

struct slice_thumbscaler_info {
	uint32_t out_of_range;
	uint32_t scaler_bypass;
	uint32_t odata_mode;
	struct img_deci_info y_deci;
	struct img_deci_info uv_deci;
	struct img_size y_factor_in;
	struct img_size y_factor_out;
	struct img_size uv_factor_in;
	struct img_size uv_factor_out;
	struct img_size src0;/* input image/slice size */
	struct img_trim y_trim;
	struct img_size y_src_after_deci;
	struct img_size y_dst_after_scaler;
	struct img_size y_init_phase;
	struct img_trim uv_trim;
	struct img_size uv_src_after_deci;
	struct img_size uv_dst_after_scaler;
	struct img_size uv_init_phase;
};

struct slice_nlm_info {
	uint32_t center_x_relative;
	uint32_t center_y_relative;
};

struct slice_postcdn_info {
	uint32_t start_row_mod4;
};

struct slice_ynr_info {
	int32_t center_offset_x;
	int32_t center_offset_y;
	uint32_t slice_width;
	uint32_t slice_height;
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

struct slice_border_info {
	uint32_t up_border;
	uint32_t down_border;
	uint32_t left_border;
	uint32_t right_border;
};

struct slice_store_info {
	uint32_t store_bypass;
	struct img_size size;
	struct img_addr addr;
	struct img_pitch pitch;
	struct slice_border_info border;
};

struct slice_afbc_store_info {
	uint32_t slc_afbc_on;
	uint32_t yheader_addr;
	uint32_t yaddr;
	struct img_size size;
	struct slice_border_info border;
	uint32_t slice_offset;
};

struct slice_fetch_info {
	uint32_t cur_layer;
	uint32_t is_pack;
	struct img_size size;
	struct img_addr addr;
	uint32_t mipi10_en;
	uint32_t mipi_byte_rel_pos;
	uint32_t mipi_word_num;
	uint32_t mipi_byte_rel_pos_uv;
	uint32_t mipi_word_num_uv;
	struct isp_fbd_yuv_info fetch_fbd;
	enum isp_fetch_path_select fetch_path_sel;
};

struct slice_fbd_raw_info {
	/* ISP_FBD_RAW_SEL */
	uint32_t pixel_start_in_hor:6;
	uint32_t pixel_start_in_ver:2;
	uint32_t fetch_fbd_bypass:1;
	uint32_t fetch_fbd_4bit_bypass:1;
	/* ISP_FBD_RAW_SLICE_SIZE */
	uint32_t height;
	uint32_t width;
	/* ISP_FBD_RAW_PARAM0 */
	uint32_t tiles_num_in_ver:11;
	uint32_t tiles_num_in_hor:6;
	/* ISP_FBD_RAW_PARAM1 */
	uint32_t tiles_start_odd:1;
	uint32_t tiles_num_pitch:8;
	/* ISP_FBD_RAW_PARAM2 */
	uint32_t header_addr_init;
	/* ISP_FBD_RAW_PARAM3 */
	uint32_t tile_addr_init_x256;
	/* ISP_FBD_RAW_PARAM4 */
	uint32_t fbd_cr_ch0123_val0;
	/* ISP_FBD_RAW_PARAM5 */
	uint32_t fbd_cr_ch0123_val1;
	/* ISP_FBD_RAW_PARAM6 */
	uint32_t fbd_cr_uv_val1:8;
	uint32_t fbd_cr_y_val1:8;
	uint32_t fbd_cr_uv_val0:8;
	uint32_t fbd_cr_y_val0:8;
	/* ISP_FBD_RAW_LOW_PARAM0 */
	uint32_t low_bit_addr_init;
	/* ISP_FBD_RAW_LOW_PARAM1 */
	uint32_t low_bit_pitch:16;
	/* ISP_FBD_RAW_LOW_4BIT_PARAM0 */
	uint32_t low_4bit_addr_init;
	/* ISP_FBD_RAW_LOW_4BIT_PARAM1 */
	uint32_t low_4bit_pitch:16;
};

struct slice_fbd_yuv_info {
	/*1.1.58.1 fdb fetch sel*/
	uint32_t fetch_fbd_bypass;
	uint32_t chk_sum_auto_clr;
	uint32_t hblank_en;
	uint32_t dout_req_signal_type;
	uint32_t afbc_mode;
	uint32_t start_isp_afbd;
	uint32_t hblank_num;
	uint32_t tile_num_pitch;
	struct img_size slice_size;
	uint32_t slice_start_pxl_xpt;
	uint32_t slice_start_pxl_ypt;
	uint32_t frame_header_base_addr;
	uint32_t slice_start_header_addr;
};

struct slice_3dnr_memctrl_info {
	uint32_t bypass;
	uint32_t start_col;
	uint32_t start_row;
	uint32_t first_line_mode;
	uint32_t last_line_mode;
	uint32_t slice_info;
	uint32_t chk_sum_clr_en;
	uint32_t roi_mode;
	uint32_t retain_num;
	uint32_t ft_max_len_sel;
	uint32_t ref_pic_flag;
	uint32_t data_toyuv_en;
	uint32_t nr3_done_mode;
	uint32_t nr3_ft_path_sel;
	uint32_t yuv_8bits_flag;
	uint32_t back_toddr_en;
	struct img_size src;
	struct img_size ft_y;
	struct img_size ft_uv;
	struct img_addr addr;
};

struct slice_3dnr_store_info {
	uint32_t bypass;
	struct img_size size;
	struct img_addr addr;
};

struct slice_3dnr_fbc_store_info {
	uint32_t bypass;
	uint32_t slice_mode_en;
	uint32_t fbc_tile_number;
	uint32_t fbc_size_in_ver;
	uint32_t fbc_size_in_hor;
	uint32_t fbc_y_tile_addr_init_x256;
	uint32_t fbc_c_tile_addr_init_x256;
	uint32_t fbc_y_header_addr_init;
	uint32_t fbc_c_header_addr_init;
	/*This is for N6pro*/
	uint32_t ctx_id;
	uint32_t endian;
	uint32_t mirror_en;
	uint32_t up_border;
	uint32_t afbc_mode;
	uint32_t left_border;
	uint32_t color_format;
	uint32_t tile_num_pitch;
	uint32_t c_nearly_full_level;
	uint32_t y_nearly_full_level;
	unsigned long slice_header_base_addr;
	unsigned long slice_payload_base_addr;
	unsigned long slice_payload_offset_addr_init;
};

struct slice_3dnr_fbd_fetch_info {
	uint32_t fbd_y_tiles_num_pitch;
	uint32_t fbd_y_pixel_size_in_hor;
	uint32_t fbd_y_pixel_size_in_ver;
	uint32_t fbd_c_pixel_size_in_hor;
	uint32_t fbd_c_pixel_size_in_ver;
	uint32_t fbd_y_pixel_start_in_ver;
	uint32_t fbd_c_pixel_start_in_ver;
	uint32_t fbd_y_pixel_start_in_hor;
	uint32_t fbd_c_pixel_start_in_hor;
	uint32_t fbd_y_tiles_num_in_hor;
	uint32_t fbd_y_tiles_start_odd;
	uint32_t fbd_c_tiles_num_in_hor;
	uint32_t fbd_c_tiles_start_odd;
	uint32_t fbd_y_tiles_num_in_ver;
	uint32_t fbd_y_header_addr_init;
	uint32_t fbd_y_tile_addr_init_x256;
	uint32_t fbd_c_tiles_num_in_ver;
	uint32_t fbd_c_header_addr_init;
	uint32_t fbd_c_tile_addr_init_x256;

	/*This is for N6pro*/
	uint32_t ctx_id;
	uint32_t bypass;
	uint32_t data_bits;
	uint32_t color_fmt;
	uint32_t hblank_en;
	uint32_t afbc_mode;
	uint32_t slice_width;
	uint32_t slice_height;
	uint32_t hblank_num;
	uint32_t tile_num_pitch;
	uint32_t start_3dnr_afbd;
	uint32_t chk_sum_auto_clr;
	uint32_t slice_start_pxl_xpt;
	uint32_t slice_start_pxl_ypt;
	uint32_t dout_req_signal_type;
	unsigned long slice_start_header_addr;
	unsigned long frame_header_base_addr;
	struct compressed_addr hw_addr;
};

struct slice_3dnr_crop_info {
	uint32_t bypass;
	uint32_t start_x;
	uint32_t start_y;
	struct img_size src;
	struct img_size dst;
};

struct slice_ltm_map_info {
	uint32_t bypass;
	uint32_t tile_width;
	uint32_t tile_height;
	uint32_t tile_num_x;
	uint32_t tile_num_y;
	uint32_t tile_right_flag;
	uint32_t tile_left_flag;
	uint32_t tile_start_x;
	uint32_t tile_start_y;
	unsigned long mem_addr;
};

struct slice_noisefilter_info {
	uint32_t seed0;
	uint32_t seed1;
	uint32_t seed2;
	uint32_t seed3;
	uint32_t seed_int;
};

struct slice_noisefilter_mode_info {
	uint32_t seed_for_mode1;
	uint32_t yrandom_mode;
};

struct slice_gtm_info {
	uint32_t gtm_mode_en;
	uint32_t gtm_map_bypass;
	uint32_t gtm_hist_stat_bypass;
	uint32_t gtm_tm_param_calc_by_hw;
	uint32_t gtm_cur_is_first_frame;
	uint32_t gtm_tm_luma_est_mode;
	uint32_t last_slice;
	uint32_t first_slice;
	uint32_t gtm_stat_slice_en;
	uint32_t gtm_tm_in_bit_depth;
	uint32_t gtm_tm_out_bit_depth;
	uint32_t line_startpos;
	uint32_t line_endpos;
};

struct slice_postcnr_info {
	uint32_t st_x;
	uint32_t st_y;
};

struct slice_edge_info {
	uint32_t radial_1D_global_start_x;
	uint32_t radial_1D_global_start_y;
};

struct slice_pyr_rec_info {
	uint32_t rec_path_sel;
	struct img_size out;
	struct img_size pre_layer;
	struct img_size cur_layer;
	uint32_t hor_padding_en;
	uint32_t hor_padding_num;
	uint32_t ver_padding_en;
	uint32_t ver_padding_num;
	uint32_t reduce_flt_vblank;
	uint32_t reduce_flt_hblank;
	uint32_t dispatch_dly_width_num;
	uint32_t dispatch_dly_height_num;
	uint32_t dispatch_pipe_full_num;
	uint32_t width_flash_mode;
	uint32_t dispatch_mode;
	uint32_t yuv_start_row_num;
	uint32_t width_dly_num_flash;
};

struct slice_dewarping_info {
	uint32_t mb_x_num;
	uint32_t mb_y_num;
	uint32_t init_start_col;
	uint32_t init_start_row;
	uint32_t start_mb_x;
	uint32_t start_mb_y;
	uint32_t slice_width;
	uint32_t slice_height;
	uint32_t dst_width;
	uint32_t dst_height;
	uint32_t crop_start_x;
	uint32_t crop_start_y;
};

struct isp_slice_desc {
	uint32_t valid;
	uint32_t x;
	uint32_t y;
	uint32_t path_en[ISP_SPATH_NUM];
	uint32_t pyr_rec_eb;
	/* slice position in src buffer, for fetch */
	struct slice_pos_info slice_pos_fetch;
	/* slice position (in fetched image) without overlap*/
	struct slice_pos_info slice_pos_orig;
	/* slice position (in fetched image) with overlap*/
	struct slice_pos_info slice_pos;
	/* slice position for fbd*/
	struct slice_pos_info slice_pos_fbd;
	struct slice_overlap_info slice_overlap;
	struct slice_fetch_info slice_fetch;
	struct slice_fbd_raw_info slice_fbd_raw;
	struct slice_fbd_yuv_info slice_fbd_yuv;
	struct slice_store_info slice_store[ISP_SPATH_NUM];
	struct slice_afbc_store_info slice_afbc_store[AFBC_PATH_NUM];
	struct slice_scaler_info slice_scaler[ISP_SPATH_NUM];
	struct slice_thumbscaler_info slice_thumbscaler;
	struct slice_nlm_info slice_nlm;
	struct slice_postcdn_info slice_postcdn;
	struct slice_ynr_info slice_ynr;
	struct slice_3dnr_memctrl_info slice_3dnr_memctrl;
	struct slice_3dnr_store_info slice_3dnr_store;
	struct slice_3dnr_fbd_fetch_info slice_3dnr_fbd_fetch;
	struct slice_3dnr_fbc_store_info slice_3dnr_fbc_store;
	struct slice_3dnr_crop_info slice_3dnr_crop;
	struct slice_ltm_map_info slice_ltm_map[LTM_MAX];
	struct slice_noisefilter_info noisefilter_info;
	struct slice_noisefilter_mode_info slice_noisefilter_mode;
	struct slice_gtm_info slice_gtm;
	struct slice_postcnr_info slice_postcnr;
	struct slice_edge_info slice_edge;
};

struct isp_slice_context {
	struct isp_slice_desc slices[SLICE_NUM_MAX];
	struct slice_drv_overlap_param_t overlapParam;
	struct yuvscaler_param_t yuvscaler_slice_param;
	struct alg_slice_drv_overlap slice_overlap;
	uint32_t slice_row_num;
	uint32_t slice_col_num;
	uint32_t slice_num;
	uint32_t slice_height;
	uint32_t slice_width;
	uint32_t img_width;
	uint32_t img_height;
	uint32_t overlap_up;
	uint32_t overlap_down;
	uint32_t overlap_left;
	uint32_t overlap_right;
	uint32_t pyr_rec_eb;
};


int isp_slice_info_cfg(void *cfg_in, struct isp_slice_context *slc_ctx);
int isp_slice_base_cfg(void *cfg_in, void *slice_ctx, uint32_t *valid_slc_num);
void *isp_slice_ctx_get(void);
int isp_slice_ctx_put(void **slc_ctx);
int isp_slice_fmcu_cmds_set(void *fmcu_handle,  void *ctx);
int isp_slice_update(void *pctx_handle, uint32_t ctx_idx, uint32_t slice_id);

#endif
