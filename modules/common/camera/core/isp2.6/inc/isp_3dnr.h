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

#ifndef _ISP_3DNR_H_
#define _ISP_3DNR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cam_queue.h"
#include "dcam_interface.h"
#include "cam_types.h"

#define FBC_NR3_Y_PAD_WIDTH            256
#define FBC_NR3_Y_PAD_HEIGHT           4
#define FBC_NR3_Y_WIDTH                128
#define FBC_NR3_Y_HEIGHT               2
#define FBC_NR3_HEADER_SIZE            16
#define FBC_NR3_BASE_ALIGN             256
#define FBC_NR3_PAYLOAD_YUV10_SIZE     512
#define FBD_NR3_Y_PAD_WIDTH            256
#define FBD_NR3_Y_PAD_HEIGHT           4
#define FBD_NR3_Y_WIDTH                128
#define FBD_NR3_Y_HEIGHT               2
#define FBD_NR3_BASE_ALIGN             256
#define FBD_BAYER_HEIGHT               4
#define NR3_BLEND_CNT                  5
#define MAX_SLICE_NUM                  16

enum nr3_func_type {
	NR3_FUNC_OFF,
	NR3_FUNC_PRE,
	NR3_FUNC_VID,
	NR3_FUNC_CAP,
	NR3_FUNC_MAX
};

struct isp_3dnr_mem_ctrl {
	uint32_t bypass;
	uint32_t nr3_done_mode;
	uint32_t nr3_ft_path_sel;
	uint32_t yuv_8bits_flag;
	uint32_t slice_info;
	uint32_t back_toddr_en;
	uint32_t chk_sum_clr_en;
	uint32_t data_toyuv_en;
	uint32_t roi_mode;
	uint32_t retain_num;
	uint32_t ref_pic_flag;
	uint32_t ft_max_len_sel;
	uint32_t first_line_mode;
	uint32_t last_line_mode;
	uint32_t start_row;
	uint32_t start_col;
	uint32_t global_img_width;
	uint32_t global_img_height;
	uint32_t img_width;
	uint32_t img_height;
	uint32_t ft_y_width;
	uint32_t ft_y_height;
	uint32_t ft_uv_width;
	uint32_t ft_uv_height;
	int mv_y;
	int mv_x;
	unsigned long ft_luma_addr;
	unsigned long ft_chroma_addr;
	uint32_t ft_pitch;
	uint32_t blend_y_en_start_row;
	uint32_t blend_y_en_start_col;
	uint32_t blend_y_en_end_row;
	uint32_t blend_y_en_end_col;
	uint32_t blend_uv_en_start_row;
	uint32_t blend_uv_en_start_col;
	uint32_t blend_uv_en_end_row;
	uint32_t blend_uv_en_end_col;
	uint32_t ft_hblank_num;
	uint32_t empty_thrd;
	uint32_t pipe_hblank_num;
	uint32_t pipe_flush_line_num;
	uint32_t pipe_nfull_num;
	uint32_t ft_fifo_nfull_num;
	struct img_addr frame_addr;
};

struct isp_3dnr_store {
	uint32_t chk_sum_clr_en;
	uint32_t shadow_clr_sel;
	uint32_t st_max_len_sel;
	uint32_t speed_2x;
	uint32_t color_format;
	uint32_t mirror_en;
	uint32_t endian;
	uint32_t mono_en;
	uint32_t data_10b;
	uint32_t mipi_en;
	uint32_t flip_en;
	uint32_t last_frm_en;
	uint32_t st_bypass;
	uint32_t img_width;
	uint32_t img_height;
	uint32_t right_border;
	uint32_t left_border;
	uint32_t down_border;
	uint32_t up_border;
	uint32_t y_pitch_mem;
	uint32_t u_pitch_mem;
	uint32_t v_pitch_mem;
	uint32_t store_res;
	uint32_t rd_ctrl;
	unsigned long st_luma_addr;
	unsigned long st_chroma_addr;
	unsigned long y_addr_mem;
	unsigned long u_addr_mem;
	uint32_t st_pitch;
	uint32_t shadow_clr;
};

struct isp_3dnr_fbd_fetch {
	uint32_t bypass;
	uint32_t fbdc_cr_ch0123_val0;
	uint32_t fbdc_cr_ch0123_val1;
	uint32_t fbdc_cr_y_val0;
	uint32_t fbdc_cr_y_val1;
	uint32_t fbdc_cr_uv_val0;
	uint32_t fbdc_cr_uv_val1;
	uint32_t y_tile_addr_init_x256;
	uint32_t y_tiles_num_pitch;
	uint32_t y_header_addr_init;
	uint32_t c_tile_addr_init_x256;
	uint32_t c_tiles_num_pitch;
	uint32_t c_header_addr_init;
	uint32_t y_pixel_size_in_hor;
	uint32_t y_pixel_size_in_ver;
	uint32_t c_pixel_size_in_hor;
	uint32_t c_pixel_size_in_ver;
	uint32_t y_pixel_start_in_hor;
	uint32_t y_pixel_start_in_ver;
	uint32_t c_pixel_start_in_hor;
	uint32_t c_pixel_start_in_ver;
	uint32_t y_tiles_num_in_hor;
	uint32_t y_tiles_num_in_ver;
	uint32_t c_tiles_num_in_hor;
	uint32_t c_tiles_num_in_ver;
	uint32_t c_tiles_start_odd;
	uint32_t y_tiles_start_odd;
	uint32_t y_rd_one_more_en;
	uint32_t rd_time_out_th;

	/*This is for N6pro*/
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

struct isp_3dnr_fbc_store {
	uint32_t bypass;
	uint32_t slice_mode_en;
	uint32_t size_in_ver;
	uint32_t size_in_hor;
	unsigned long y_tile_addr_init_x256;
	unsigned long c_tile_addr_init_x256;
	uint32_t tile_number_pitch;
	unsigned long y_header_addr_init;
	unsigned long c_header_addr_init;
	uint32_t fbc_constant_yuv;
	uint32_t later_bits;
	uint32_t tile_number;
	uint32_t mirror_en;
	uint32_t color_format;
	uint32_t endian;
	uint32_t afbc_mode;
	uint32_t left_border;
	uint32_t up_border;
	unsigned long slice_payload_offset_addr_init;
	unsigned long slice_payload_base_addr;
	unsigned long slice_header_base_addr;
	uint32_t c_nearly_full_level;
	uint32_t y_nearly_full_level;

	struct compressed_addr hw_addr;
};

struct isp_3dnr_crop {
	uint32_t crop_bypass;
	uint32_t src_width;
	uint32_t src_height;
	uint32_t dst_width;
	uint32_t dst_height;
	int start_x;
	int start_y;
};

struct  fast_mv {
	int mv_x;
	int mv_y;
};

struct frame_size {
	uint32_t width;
	uint32_t height;
};

/* capture */
struct nr3_slice {
	uint32_t start_row;
	uint32_t end_row;
	uint32_t start_col;
	uint32_t end_col;
	uint32_t overlap_left;
	uint32_t overlap_right;
	uint32_t overlap_up;
	uint32_t overlap_down;
	uint32_t slice_num;
	uint32_t slice_id;
	uint32_t cur_frame_width;
	uint32_t cur_frame_height;
	uint32_t src_lum_addr;
	uint32_t src_chr_addr;
	uint32_t yuv_8bits_flag;
	uint32_t ft_pitch;
	int mv_x;
	int mv_y;
	struct ImageRegion_Info image_region_info[MAX_SLICE_NUM];
};

struct nr3_slice_for_blending {
	int Y_start_x;
	int Y_end_x;
	int Y_start_y;
	int Y_end_y;
	int UV_start_x;
	int UV_end_x;
	int UV_start_y;
	int UV_end_y;
	uint32_t start_col;
	uint32_t start_row;
	uint32_t src_width;
	uint32_t src_height;
	uint32_t ft_y_width;
	uint32_t ft_y_height;
	uint32_t ft_uv_width;
	uint32_t ft_uv_height;
	uint32_t src_lum_addr;
	uint32_t src_chr_addr;
	uint32_t first_line_mode;
	uint32_t last_line_mode;
	uint32_t dst_width;
	uint32_t dst_height;
	uint32_t dst_lum_addr;
	uint32_t dst_chr_addr;
	uint32_t offset_start_x;
	uint32_t offset_start_y;
	uint32_t crop_bypass;
};

enum isp_3dnr_cfg_cmd {
	ISP_3DNR_CFG_BUF,
	ISP_3DNR_CFG_MODE,
	ISP_3DNR_CFG_BLEND_CNT,
	ISP_3DNR_CFG_FBC_FBD_INFO,
	ISP_3DNR_CFG_SIZE_INFO,
	ISP_3DNR_CFG_BLEND_INFO,
	ISP_3DNR_CFG_MEMCTL_STORE_INFO,
	ISP_3DNR_CFG_MV_VERSION,
	ISP_3DNR_CFG_MAX,
};

struct isp_3dnr_ops {
	int (*cfg_param)(void *nr3_handle, enum isp_3dnr_cfg_cmd cmd, void *param);
	int (*pipe_proc)(void *nr3_handle, void *param, uint32_t mode);
};

struct isp_3dnr_ctx_desc {
	uint32_t type;
	uint32_t mode;
	uint32_t width;
	uint32_t ctx_id;
	uint32_t height;
	uint32_t blending_cnt;
	uint32_t nr3_mv_version;
	uint32_t pyr_rec_eb;
	/*used to determine wheather is the first frame after hdr*/
	uint32_t preblend_bypass;

	struct fast_mv mv;
	struct isp_3dnr_ops ops;
	struct isp_3dnr_crop crop;
	struct nr3_me_data *mvinfo;
	struct isp_k_block *isp_block;
	struct isp_3dnr_store nr3_store;
	struct isp_3dnr_mem_ctrl mem_ctrl;
	struct dcam_compress_info fbc_info;
	struct isp_3dnr_fbc_store nr3_fbc_store;
	struct isp_3dnr_fbd_fetch nr3_fbd_fetch;
	struct camera_buf *buf_info[ISP_NR3_BUF_NUM];
	struct ImageRegion_Info image_region_info[MAX_SLICE_NUM];

	enum sprd_cam_sec_mode nr3_sec_mode;
};

void isp_3dnr_config_param(struct isp_3dnr_ctx_desc *ctx);
void isp_3dnr_bypass_config(uint32_t idx);
void *isp_3dnr_ctx_get(uint32_t idx);
void isp_3dnr_ctx_put(void *ctx);

#ifdef __cplusplus
}
#endif

#endif/* _ISP_3DNR_H_ */
