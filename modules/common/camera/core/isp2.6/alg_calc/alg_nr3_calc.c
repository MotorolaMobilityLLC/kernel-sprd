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

#include "alg_nr3_calc.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ALG_NR3_CALC: %d %d %s : "fmt, current->pid, __LINE__, __func__

void nr3_mv_convert_ver(struct alg_nr3_mv_cfg *param_ptr)
{
	int mv_x = 0, mv_y = 0;
	uint32_t mode_projection = 0;
	uint32_t sub_me_bypass = 0;
	int iw = 0, ow = 0;
	int ih = 0, oh = 0;
	int o_mv_x = 0, o_mv_y = 0;

	mv_x = param_ptr->mv_x;
	mv_y = param_ptr->mv_y;
	mode_projection = param_ptr->mode_projection;
	sub_me_bypass = param_ptr->sub_me_bypass;
	iw = param_ptr->iw;
	ih = param_ptr->ih;
	ow = param_ptr->ow;
	oh = param_ptr->oh;

	if (sub_me_bypass == 1) {
		if (mode_projection == 1) {
			if (mv_x > 0)
				o_mv_x = (mv_x * 2 * ow + (iw >> 1)) / iw;
			else
				o_mv_x = (mv_x * 2 * ow - (iw >> 1)) / iw;

			if (mv_y > 0)
				o_mv_y = (mv_y * 2 * oh + (ih >> 1)) / ih;
			else
				o_mv_y = (mv_y * 2 * oh - (ih >> 1)) / ih;
		} else {
			if (mv_x > 0)
				o_mv_x = (mv_x * ow + (iw >> 1)) / iw;
			else
				o_mv_x = (mv_x * ow - (iw >> 1)) / iw;

			if (mv_y > 0)
				o_mv_y = (mv_y * oh + (ih >> 1)) / ih;
			else
				o_mv_y = (mv_y * oh - (ih >> 1)) / ih;
		}
	} else {
		if (mode_projection == 1) {
			if (mv_x > 0)
				o_mv_x = (mv_x * ow + (iw >> 1)) / iw;
			else
				o_mv_x = (mv_x * ow - (iw >> 1)) / iw;

			if (mv_y > 0)
				o_mv_y = (mv_y * oh + (ih >> 1)) / ih;
			else
				o_mv_y = (mv_y * oh - (ih >> 1)) / ih;
		} else {
			if (mv_x > 0)
				o_mv_x = (mv_x * ow + iw) / (iw * 2);
			else
				o_mv_x = (mv_x * ow - iw) / (iw * 2);

			if (mv_y > 0)
				o_mv_y = (mv_y * oh + ih) / (ih * 2);
			else
				o_mv_y = (mv_y * oh - ih) / (ih * 2);
		}
	}

	param_ptr->o_mv_x = o_mv_x;
	param_ptr->o_mv_y = o_mv_y;
}

int nr3d_fetch_ref_image_position(struct ImageRegion_Info *image_region_info,
	uint32_t frame_width, uint32_t frame_height)
{
	uint32_t slice_flag = 0;//0: one slice; 1:top slice; 2:bottom slice; 3:middle slice
	uint32_t mv_flag = 0;//0: positive even; 1:positive odd; 2: negative even; 3:negative odd;
	int mv_y_adjust_start = 0, mv_y_adjust_end = 0;
	int x_start =0, x_end = 0, y_start = 0, y_end = 0;
	int x_start_uv = 0, x_end_uv = 0, y_start_uv = 0, y_end_uv = 0;
	int mv_x = 0, mv_y = 0;
	int mv_loc_adjust[16][3] = {
	{0, 0, 0}, {0, 0, 1}, {0, 1, 0}, {0, 0, 0},
	{0, 1, 0}, {0, 1, 1}, {0, 1, 0}, {0, 0, 0},
	{0, 0, 0}, {0, 0, 1}, {0, 1, 0}, {-1, 0, 1},
	{0, 1, 0}, {0, 1, 1}, {0, 1, 0}, {-1, 0, 1}
	};

	if (!image_region_info) {
		pr_err("fail to get valid image_region_info.\n");
		return -EINVAL;
	}

	mv_x = image_region_info->mv_x;
	mv_y = image_region_info->mv_y;

	slice_flag = ((image_region_info->region_start_row == 0 ? 0 : 1) << 1) + \
		(image_region_info->region_end_row == (frame_height - 1) ? 0 : 1);
	mv_flag = mv_y >= 0 ? (mv_y % 2 == 0 ? 0 : 1) : (mv_y % 2 == 0 ? 2 : 3);
	image_region_info->skip_flag = 0; //indicator whether skip one row.

	if (mv_y != 0) {
		mv_y_adjust_start = mv_loc_adjust[slice_flag * 4 + mv_flag][0];
		mv_y_adjust_end = mv_loc_adjust[slice_flag * 4 + mv_flag][1];
		image_region_info->skip_flag = mv_loc_adjust[slice_flag * 4 + mv_flag][2];
	}

	//calculate the locations of the region in the image using MV information
	x_start = image_region_info->region_start_col + mv_x;
	x_end = image_region_info->region_width - 1 + x_start;
	y_start = image_region_info->region_start_row + mv_y;
	y_end = image_region_info->region_height - 1 + y_start;

	if (x_start < 0)
		x_start = 0;
	if (x_end > (frame_width - 1))
		x_end = frame_width - 1;
	if (y_start < 0)
		y_start = 0;
	if (y_end > (frame_height - 1))
		y_end = frame_height - 1;

	x_start_uv = image_region_info->region_start_col / 2 + mv_x / 2;
	x_end_uv = image_region_info->region_width / 2 - 1 + x_start_uv;
	y_start_uv = image_region_info->region_start_row / 2 + mv_y / 2;
	y_end_uv = image_region_info->region_height / 2 - 1 + y_start_uv;

	if (x_start_uv < 0)
		x_start_uv = 0;
	if (x_end_uv > (frame_width / 2 - 1))
		x_end_uv = frame_width / 2 - 1;
	if (y_start_uv < 0)
		y_start_uv = 0;
	if (y_end_uv > (frame_height / 2 - 1))
		y_end_uv = frame_height / 2 - 1;

	//ROI location in full size image.
	image_region_info->Y_start_x = x_start;
	image_region_info->Y_end_x = x_end;
	image_region_info->Y_start_y = y_start;
	image_region_info->Y_end_y = y_end;

	image_region_info->UV_start_x = x_start_uv;
	image_region_info->UV_end_x = x_end_uv;
	image_region_info->UV_start_y = y_start_uv;
	image_region_info->UV_end_y = y_end_uv;

	return 0;
}
