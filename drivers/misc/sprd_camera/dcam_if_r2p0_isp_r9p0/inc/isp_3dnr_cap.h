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

#ifndef _ISP_3DNR_CAP_H_
#define _ISP_3DNR_CAP_H_

#ifdef __cplusplus
extern "C" {
#endif

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
	uint32_t cur_frame_width;
	uint32_t cur_frame_height;
	uint32_t src_lum_addr;
	uint32_t src_chr_addr;
	uint32_t img_id;
	int mv_x;
	int mv_y;
};

struct nr3_slice_for_blending {
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

int sprd_isp_3dnr_cap_frame_proc(void *isp_handle, void *data);
int sprd_isp_3dnr_blending_calc(struct nr3_slice *in,
	struct nr3_slice_for_blending *out);

#ifdef __cplusplus
}
#endif

#endif
