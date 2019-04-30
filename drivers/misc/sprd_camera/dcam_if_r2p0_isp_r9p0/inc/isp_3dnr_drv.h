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

#ifndef _ISP_3DNR_DEV_H_
#define _ISP_3DNR_DEV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/sprd_ion.h>
#include "cam_common.h"

#define ISP_3DNR_BLEND_NUM  5
#define ISP_NR3_BUFFER_NUM  2

enum isp_operate_type {
	ISP_OPERATE_PRE,
	ISP_OPERATE_VID,
	ISP_OPERATE_CAP,
};

struct isp_mem_ctrl {
	uint32_t ft_max_len_sel;
	uint32_t ref_pic_flag;
	uint32_t retain_num;
	uint32_t roi_mode;
	uint32_t data_toyuv_en;
	uint32_t chk_sum_clr_en;
	uint32_t back_toddr_en;
	uint32_t nr3_done_mode;
	uint32_t bypass;
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
	uint32_t pipe_hblank_num;
	uint32_t pipe_flush_line_num;
	uint32_t pipe_nfull_num;
	uint32_t ft_fifo_nfull_num;
};

struct isp_3dnr_store {
	uint32_t chk_sum_clr_en;
	uint32_t shadow_clr_sel;
	uint32_t st_max_len_sel;
	uint32_t st_bypass;
	uint32_t img_width;
	uint32_t img_height;
	unsigned long st_luma_addr;
	unsigned long st_chroma_addr;
	uint32_t st_pitch;
	uint32_t shadow_clr;
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

struct frame_info {
	uint32_t global_width;
	uint32_t global_height;
};

struct fetch_frame_info {
	uint32_t ft_y_width;
	uint32_t ft_y_height;
	uint32_t ft_uv_width;
	uint32_t ft_uv_height;
	uint32_t ft_luma_addr;
	uint32_t ft_chroma_addr;
	uint32_t ft_pitch;
};

struct  frame_mv {
	int mv_x;
	int mv_y;
};

struct me_conversion {
	uint32_t nr3_channel_sel;
	uint32_t project_mode;
	int roi_start_x;
	int roi_start_y;
	uint32_t roi_width;
	uint32_t roi_height;
	uint32_t input_width;
	uint32_t input_height;
	uint32_t output_width;
	uint32_t output_height;
};

struct isp_nr3_init_param {
	uint32_t need_3dnr;
	uint32_t proj_mode;
	uint32_t prev_width;
	uint32_t prev_height;
	uint32_t cap_width;
	uint32_t cap_height;
	uint32_t input_width;
	uint32_t input_height;
};

struct frame_size {
	uint32_t width;
	uint32_t height;
};

struct isp_nr3_param {
	uint32_t blending_cnt;
	uint32_t need_3dnr;
	uint32_t isp_3dnr_bypass;
	uint32_t fetch_format;
	uint32_t store_format;
	uint32_t cur_cap_frame;
	struct frame_size sns_max_size;/*for capture*/
	struct frame_size prev_size;/*for prevew*/
	struct frame_mv mv;
	struct me_conversion me_conv;
	struct frame_info curr_frame_yuv;
	struct fetch_frame_info ft_ref;
	struct isp_mem_ctrl mem_ctrl;
	struct isp_3dnr_store nr3_store;
	struct  isp_3dnr_crop crop;
	struct cam_buf_info buf_info;
};

int sprd_isp_3dnr_release(void *isp_handle);
int sprd_isp_3dnr_conversion_mv(struct isp_nr3_param *nr3_param, void *pdata);
int sprd_isp_3dnr_param_get(struct isp_nr3_param *param,
	void *frame);
void isp_3dnr_config_param(uint32_t idx, enum isp_operate_type type_id,
	struct isp_nr3_param *param);
void isp_3dnr_default_param(uint32_t idx);

#ifdef __cplusplus
}
#endif

#endif
