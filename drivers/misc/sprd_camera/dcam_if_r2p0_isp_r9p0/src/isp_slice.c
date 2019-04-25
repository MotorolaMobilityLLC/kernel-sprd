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

#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <video/sprd_mm.h>

#include "isp_slice.h"
#include "isp_3dnr_cap.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_SLICE: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define ISP_SLICE_ALIGN_SIZE           2
#define ISP_ALIGNED(size)              ((size) & ~(ISP_SLICE_ALIGN_SIZE - 1))

#define SLICE_HEIGHT_NUM               1
#define SLICE_WIDTH_MAX                (2592 - RAW_OVERLAP_RIGHT)
#define SLICE_SCL_CAP_WIDTH_MAX        2592
#define SLICE_WIDTH_MIN                160
#define RAW_OVERLAP_UP                 58
#define RAW_OVERLAP_DOWN               78
#define RAW_OVERLAP_LEFT               86
#define RAW_OVERLAP_RIGHT              138
#define YUV_OVERLAP_UP                 46
#define YUV_OVERLAP_DOWN               68
#define YUV_OVERLAP_LEFT               74
#define YUV_OVERLAP_RIGHT              126
/* overlap need increase when 1/4 deci used */
#define YUV_OVERLAP_RIGHT_PLUS         192
#define RAW_OVERLAP_RIGHT_PLUS         208

#define YUVSCALER_OVERLAP_UP           32
#define YUVSCALER_OVERLAP_DOWN         52
#define YUVSCALER_OVERLAP_LEFT         16
#define YUVSCALER_OVERLAP_RIGHT        68
/* overlap need increase when 1/4 deci used */
#define YUVSCALER_OVERLAP_RIGHT_PLUS   134

struct isp_scaler_slice_tmp {
	uint32_t cur_slice_id;
	uint32_t slice_row_num;
	uint32_t slice_col_num;
	uint32_t start_col;
	uint32_t start_row;
	uint32_t end_col;
	uint32_t end_row;
	uint32_t cur_row;
	uint32_t cur_col;
	uint32_t overlap_bad_up;
	uint32_t overlap_bad_down;
	uint32_t overlap_bad_left;
	uint32_t overlap_bad_right;
	uint32_t trim0_end_x;
	uint32_t trim0_end_y;
	uint32_t trim0_start_adjust_x;
	uint32_t trim0_start_adjust_y;
	uint32_t deci_x;
	uint32_t deci_y;
	uint32_t deci_x_align;
	uint32_t deci_y_align;
	uint32_t scaler_out_height_temp;
	uint32_t scaler_out_width_temp;
	uint32_t scaler_slice_start_x;
	uint32_t scaler_slice_end_x;
	uint32_t *scaler_slice;
	uint32_t *scaler_yuv;
};

static uint32_t sprd_ispslice_noisefilter_24b_shift8(uint32_t seed,
	uint32_t *data_out)
{
	uint32_t bit_0 = 0, bit_1 = 0;
	uint32_t bit_2 = 0, bit_3 = 0;
	uint32_t bit_in[8] = {0}, bit_in8b = 0;
	uint32_t out = 0;
	uint32_t i = 0;

	for (i = 0; i < 8; i++) {
		bit_0 = (seed >> (0 + i)) & 0x1;
		bit_1 = (seed >> (1 + i)) & 0x1;
		bit_2 = (seed >> (2 + i)) & 0x1;
		bit_3 = (seed >> (7 + i)) & 0x1;
		bit_in[i] = bit_0 ^ bit_1 ^ bit_2 ^ bit_3;
	}
	bit_in8b = (bit_in[7] << 7) | (bit_in[6] << 6) | (bit_in[5] << 5) |
		(bit_in[4] << 4) | (bit_in[3] << 3) | (bit_in[2] << 2) |
		(bit_in[1] << 1) | bit_in[0];

	out = seed & 0xffffff;
	out = out | (bit_in8b << 24);
	if (data_out)
		*data_out = out;

	out = out >> 8;

	return out;
}

static void sprd_ispslice_noisefilter_seeds(uint32_t image_width,
	uint32_t seed0, uint32_t *seed1, uint32_t *seed2, uint32_t *seed3)
{
	uint32_t i = 0;

	*seed1 = sprd_ispslice_noisefilter_24b_shift8(seed0, NULL);
	*seed2 = seed0;

	for (i = 0; i < image_width; i++)
		*seed2 = sprd_ispslice_noisefilter_24b_shift8(*seed2, NULL);

	*seed3 = sprd_ispslice_noisefilter_24b_shift8(*seed2, NULL);
}

static int sprd_ispslice_noisefliter_info_set(struct slice_context_info *cxt)
{
	int rtn = 0;
	uint32_t cur_slice_id = 0, slice_num = 0;
	uint32_t slice_width = 0;
	struct slice_base_info *base_info = NULL;
	struct slice_noisefilter_info *noisefilter_info = NULL;
	struct slice_scaler_info *scaler_info = NULL;

	if (!cxt) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	base_info = &cxt->base_info;
	cur_slice_id = base_info->cur_slice_id;
	slice_num = base_info->slice_num;
	scaler_info = cxt->scaler_info[SLICE_PATH_CAP];

	for (; cur_slice_id < slice_num; cur_slice_id++) {
		noisefilter_info = &cxt->noisefilter_info[cur_slice_id];
		slice_width = scaler_info[cur_slice_id].trim1_size_x;
		noisefilter_info->seed0 = 0xff;
		sprd_ispslice_noisefilter_seeds(slice_width,
			noisefilter_info->seed0,
			&noisefilter_info->seed1, &noisefilter_info->seed2,
			&noisefilter_info->seed3);
		noisefilter_info->seed_int = 1;
	}
exit:
	return rtn;
}

static void sprd_ispslice_scaler_phase_calc(uint32_t phase, uint32_t factor,
	uint32_t *phase_int, uint32_t *phase_rmd)
{
	phase_int[0] = (uint32_t)(phase / factor);
	phase_rmd[0] = (uint32_t)(phase - factor * phase_int[0]);
}

static int sprd_ispslice_size_info_get(struct slice_param_in *in_ptr,
	uint32_t *h, uint32_t *w, uint32_t *slice_col)
{
	int rtn = 0;
	uint32_t tempw = 0, temp_limit = 0;
	struct slice_img_size *input = NULL;
	struct slice_img_size *output = NULL;
	uint32_t max_width = 0;
	struct slice_scaler_path *scaler_info = NULL;
	uint32_t slice_start = 0, slice_end = 0;
	uint32_t trim_start = 0, trim_end = 0;
	uint32_t i = 0;

	if (!in_ptr || !h || !w) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	input = &in_ptr->img_size;
	scaler_info = &in_ptr->scaler_frame[SLICE_PATH_CAP];
	output = &in_ptr->store_frame[SLICE_PATH_CAP].size;

	max_width = (input->width > output->width) ?
		input->width : output->width;
	trim_start = scaler_info->trim0_start_x;
	trim_end = scaler_info->trim0_start_x + scaler_info->trim0_size_x;
	if (max_width > SLICE_SCL_CAP_WIDTH_MAX) {
		tempw = (max_width + SLICE_SCL_CAP_WIDTH_MAX
			- 1) / SLICE_SCL_CAP_WIDTH_MAX + 1;
		*w = input->width / tempw;
		*w = (*w > SLICE_WIDTH_MAX) ? SLICE_WIDTH_MAX : *w;
		for (i = 0; i < tempw; i++) {
			slice_start = *w * i;
			slice_end = *w * (i + 1);
			if (trim_start > slice_start &&
				trim_start < slice_end) {
				temp_limit = slice_end - trim_start;
				if (temp_limit < SLICE_WIDTH_MIN) {
					tempw -= 1;
					break;
				}
			}
			if (trim_end > slice_start &&
				trim_end < slice_end) {
				temp_limit = trim_end - slice_start;
				if (temp_limit < SLICE_WIDTH_MIN) {
					tempw -= 1;
					break;
				}
			}
		}
		*w = input->width / tempw;
		*w = (*w > SLICE_WIDTH_MAX) ? SLICE_WIDTH_MAX : *w;
	} else {
		*w = input->width;
		tempw = 1;
	}

	*h = input->height / SLICE_HEIGHT_NUM;

	*w = ISP_ALIGNED(*w);
	*h = ISP_ALIGNED(*h);
	*slice_col = tempw;

exit:
	return rtn;
}

static int sprd_ispslice_overlap_info_get(struct slice_param_in *in_ptr,
	struct slice_base_info *base_info)
{
	if (!in_ptr || !base_info) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	switch (in_ptr->fetch_format) {
	case ISP_FETCH_YUV422_2FRAME:
	case ISP_FETCH_YVU422_2FRAME:
	case ISP_FETCH_YUV420_2FRAME:
	case ISP_FETCH_YVU420_2FRAME:
	case ISP_FETCH_YUYV:
	case ISP_FETCH_UYVY:
	case ISP_FETCH_YVYU:
	case ISP_FETCH_VYUY:
		base_info->yuv_overlap_up = YUV_OVERLAP_UP;
		base_info->yuv_overlap_down = YUV_OVERLAP_DOWN;
		base_info->yuv_overlap_left = YUV_OVERLAP_LEFT;
		base_info->yuv_overlap_right = YUV_OVERLAP_RIGHT;
		if (in_ptr->scaler_frame[SLICE_PATH_CAP].deci_x > 3)
			base_info->yuv_overlap_right = YUV_OVERLAP_RIGHT_PLUS;
		break;
	case ISP_FETCH_CSI2_RAW10:
	case ISP_FETCH_NORMAL_RAW10:
		base_info->raw_overlap_up = RAW_OVERLAP_UP;
		base_info->raw_overlap_down = RAW_OVERLAP_DOWN;
		base_info->raw_overlap_left = RAW_OVERLAP_LEFT;
		base_info->raw_overlap_right = RAW_OVERLAP_RIGHT;
		if (in_ptr->scaler_frame[SLICE_PATH_CAP].deci_x > 3)
			base_info->raw_overlap_right = RAW_OVERLAP_RIGHT_PLUS;
		break;
	default:
		break;
	}

	return ISP_RTN_SUCCESS;
}

static void sprd_ispslice_fetch_pitch_get(struct slice_pitch *pitch_ptr,
	enum isp_fetch_format format, uint32_t width)
{

	switch (format) {
	case ISP_FETCH_YUV422_3FRAME:
		pitch_ptr->chn0 = width;
		pitch_ptr->chn1 = width >> 1;
		pitch_ptr->chn2 = width >> 1;
		break;
	case ISP_FETCH_YUV422_2FRAME:
	case ISP_FETCH_YVU422_2FRAME:
	case ISP_FETCH_YUV420_2FRAME:
	case ISP_FETCH_YVU420_2FRAME:
		pitch_ptr->chn0 = width;
		pitch_ptr->chn1 = width;
		break;
	case ISP_FETCH_UYVY:
	case ISP_FETCH_VYUY:
	case ISP_FETCH_YUYV:
	case ISP_FETCH_YVYU:
		pitch_ptr->chn0 = width;
		break;
	case ISP_FETCH_NORMAL_RAW10:
		pitch_ptr->chn0 = width << 1;
		break;
	case ISP_FETCH_CSI2_RAW10:
		{
			uint32_t mod16_pixel = width & 0xF;
			uint32_t mod16_bytes = (mod16_pixel + 3) / 4 * 5;
			uint32_t mod16_words = (mod16_bytes + 3) / 4;

			pitch_ptr->chn0 = width / 16 * 20 + mod16_words * 4;
		}
		break;
	default:
		break;
	}

}

static int sprd_ispslice_fetch_info_set(struct slice_param_in *in_ptr,
	struct slice_context_info *cxt)
{
	int rtn = 0;
	struct slice_base_info *base_info = NULL;
	struct slice_fetch_info *fetch_info = NULL;
	struct slice_addr *address = NULL;
	uint32_t cur_slice_id = 0, slice_num = 0;
	struct slice_pitch fetch_pitch = {0};
	uint32_t start_col = 0, end_col = 0;
	uint32_t start_row = 0, end_row = 0;
	uint32_t ch0_offset = 0;
	uint32_t ch1_offset = 0;
	uint32_t ch2_offset = 0;

	uint32_t mipi_word_num_start[16] = {
		0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5};
	uint32_t mipi_word_num_end[16] = {
		0, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5};

	if (!in_ptr || !cxt) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	base_info = &cxt->base_info;
	address = &in_ptr->fetch_addr;
	cur_slice_id = base_info->cur_slice_id;
	slice_num = base_info->slice_num;

	sprd_ispslice_fetch_pitch_get(&fetch_pitch,
		in_ptr->fetch_format, base_info->img_width);
	for (; cur_slice_id < slice_num; cur_slice_id++) {
		fetch_info = &cxt->fetch_info[cur_slice_id];
		fetch_info->addr.chn0 = address->chn0;
		fetch_info->addr.chn1 = address->chn1;
		fetch_info->addr.chn2 = address->chn2;
		start_col = base_info->slice_raw_pos_array
			[cur_slice_id].start_col;
		end_col = base_info->slice_raw_pos_array[cur_slice_id].end_col;
		start_row = base_info->slice_raw_pos_array
			[cur_slice_id].start_row;
		end_row = base_info->slice_raw_pos_array[cur_slice_id].end_row;

		switch (in_ptr->fetch_format) {
		case ISP_FETCH_YUV422_3FRAME:
			ch0_offset = start_row * fetch_pitch.chn0
				+ start_col;
			ch1_offset = start_row * fetch_pitch.chn1
				+ ((start_col + 1) >> 1);
			ch2_offset = start_row * fetch_pitch.chn2
				+ ((start_col + 1) >> 1);
			break;
		case ISP_FETCH_NORMAL_RAW10:
		case ISP_FETCH_UYVY:
			ch0_offset = start_row * fetch_pitch.chn0
				+ start_col * 2;
			break;
		case ISP_FETCH_YUV422_2FRAME:
		case ISP_FETCH_YVU422_2FRAME:
			ch0_offset = start_row * fetch_pitch.chn0
				+ start_col;
			ch1_offset = start_row * fetch_pitch.chn1
				+ start_col;
			break;
		case ISP_FETCH_YUV420_2FRAME:
		case ISP_FETCH_YVU420_2FRAME:
			ch0_offset = start_row * fetch_pitch.chn0
				+ start_col;
			ch1_offset = ((start_row * fetch_pitch.chn1
				+ 1) >> 1) + start_col;
			break;
		case ISP_FETCH_CSI2_RAW10:
			ch0_offset = start_row * fetch_pitch.chn0
				+ (start_col >> 2) * 5
				+ (start_col & 0x3);
			break;
		default:
			break;
		}

		fetch_info->addr.chn0 += ch0_offset;
		fetch_info->addr.chn1 += ch1_offset;
		fetch_info->addr.chn2 += ch2_offset;
		fetch_info->size.height = end_row - start_row + 1;
		fetch_info->size.width = end_col - start_col + 1;

		fetch_info->mipi_byte_rel_pos = start_col & 0x0f;
		fetch_info->mipi_word_num = ((((end_col + 1) >> 4) * 5 +
			mipi_word_num_end[(end_col + 1) & 0x0f])
			- (((start_col + 1) >> 4) * 5
			+ mipi_word_num_start[(start_col + 1) & 0x0f]) + 1);
	}
exit:
	return rtn;
}

static int sprd_ispslice_base_info_set(struct slice_param_in *in_ptr,
	struct slice_base_info *base_info)
{
	int rtn = 0;
	uint32_t i = 0, j = 0;
	uint32_t img_height = 0, img_width = 0;
	uint32_t slice_height = 0, slice_width = 0;
	uint32_t slice_total_row = 0, slice_total_col = 0, slice_num = 0;

	if (!in_ptr || !base_info) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	rtn = sprd_ispslice_size_info_get(in_ptr, &slice_height,
		&slice_width, &slice_total_col);
	if (rtn) {
		pr_err("fail to get slice size info!\n");
		rtn = -EINVAL;
		goto exit;
	}

	rtn = sprd_ispslice_overlap_info_get(in_ptr, base_info);
	if (rtn)
		goto exit;

	pr_info("fmcu slice height %d, width %d\n", slice_height, slice_width);
	img_height = in_ptr->img_size.height;
	img_width = in_ptr->img_size.width;
	slice_total_row = (img_height + slice_height - 1) / slice_height;
	slice_num = slice_total_col * slice_total_row;
	base_info->cur_slice_id = 0;
	base_info->slice_num = slice_num;
	base_info->slice_col_num = slice_total_col;
	base_info->slice_row_num = slice_total_row;
	base_info->slice_height = slice_height;
	base_info->slice_width = slice_width;
	base_info->img_height = img_height;
	base_info->img_width = img_width;
	base_info->store_height =
		in_ptr->store_frame[SLICE_PATH_CAP].size.height;
	base_info->store_width =
		in_ptr->store_frame[SLICE_PATH_CAP].size.width;
	sprd_ispslice_fetch_pitch_get(&base_info->pitch,
		in_ptr->fetch_format, base_info->img_width);

	if (base_info->slice_num > SLICE_NUM_MAX)
		pr_info("slice num is too large %d\n", base_info->slice_num);

	for (i = 0; i < slice_total_row; i++) {
		for (j = 0; j < slice_total_col; j++) {
			struct slice_pos_info raw_temp_win = {0};
			struct slice_overlap_info raw_temp_overlap = {0};
			struct slice_pos_info yuv_temp_win = {0};
			struct slice_overlap_info yuv_temp_overlap = {0};

			raw_temp_win.start_col = j * slice_width;
			raw_temp_win.start_row = i * slice_height;
			yuv_temp_win.start_col = j * slice_width;
			yuv_temp_win.start_row = i * slice_height;
			if (i != 0) {
				raw_temp_win.start_row -=
					base_info->raw_overlap_up;
				raw_temp_overlap.overlap_up =
					base_info->raw_overlap_up;
				yuv_temp_win.start_row -=
					base_info->yuv_overlap_up;
				yuv_temp_overlap.overlap_up =
					base_info->yuv_overlap_up;
			}

			if (j != 0) {
				raw_temp_win.start_col -=
					base_info->raw_overlap_left;
				raw_temp_overlap.overlap_left =
					base_info->raw_overlap_left;
				yuv_temp_win.start_col -=
					base_info->yuv_overlap_left;
				yuv_temp_overlap.overlap_left =
					base_info->yuv_overlap_left;
			}

			if (i != slice_total_row - 1) {
				raw_temp_win.end_row = (i + 1) * slice_height
					- 1 + base_info->raw_overlap_down;
				raw_temp_overlap.overlap_down =
					base_info->raw_overlap_down;
				yuv_temp_win.end_row = (i + 1) * slice_height
					- 1 + base_info->yuv_overlap_down;
				yuv_temp_overlap.overlap_down =
					base_info->yuv_overlap_down;
			} else {
				raw_temp_win.end_row = img_height - 1;
				yuv_temp_win.end_row = img_height - 1;
			}

			if (j != slice_total_col - 1) {
				raw_temp_win.end_col = (j + 1) * slice_width
					- 1 + base_info->raw_overlap_right;
				raw_temp_overlap.overlap_right =
					base_info->raw_overlap_right;
				yuv_temp_win.end_col = (j + 1) * slice_width
					- 1 + base_info->yuv_overlap_right;
				yuv_temp_overlap.overlap_right =
					base_info->yuv_overlap_right;
			} else {
				raw_temp_win.end_col = img_width - 1;
				yuv_temp_win.end_col = img_width - 1;
			}

			base_info->slice_raw_pos_array[i * slice_total_col
				+ j] = raw_temp_win;
			base_info->slice_raw_overlap_array[i * slice_total_col
				+ j] = raw_temp_overlap;
			base_info->slice_yuv_pos_array[i * slice_total_col
				+ j] = yuv_temp_win;
			base_info->slice_yuv_overlap_array[i * slice_total_col
				+ j] = yuv_temp_overlap;
		}
	}

exit:
	return rtn;
}

static int sprd_ispslice_dispatch_info_set(struct slice_context_info *cxt)
{
	int rtn = 0;
	uint32_t cur_slice_id = 0, slice_num = 0;
	uint32_t start_col = 0, end_col = 0;
	uint32_t start_row = 0, end_row = 0;
	struct slice_base_info *base_info = NULL;
	struct slice_dispatch_info *dispatch_info = NULL;

	if (!cxt) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	base_info = &cxt->base_info;
	cur_slice_id = base_info->cur_slice_id;
	slice_num = base_info->slice_num;

	for (; cur_slice_id < slice_num; cur_slice_id++) {
		dispatch_info = &cxt->dispatch_info[cur_slice_id];
		start_col = base_info->slice_raw_pos_array
			[cur_slice_id].start_col;
		end_col = base_info->slice_raw_pos_array[cur_slice_id].end_col;
		start_row = base_info->slice_raw_pos_array
			[cur_slice_id].start_row;
		end_row = base_info->slice_raw_pos_array[cur_slice_id].end_row;
		dispatch_info->size.height = end_row - start_row + 1;
		dispatch_info->size.width = end_col - start_col + 1;
	}
exit:
	return rtn;
}

static int sprd_ispslice_nlm_info_set(struct slice_context_info *cxt,
	struct slice_param_in *in_ptr)
{
	int rtn = 0;
	uint32_t cur_slice_id = 0, slice_num = 0;
	uint32_t start_col = 0, end_col = 0;
	uint32_t start_row = 0, end_row = 0;
	struct slice_base_info *base_info = NULL;
	struct slice_nlm_info *nlm_info = NULL;

	if (!cxt) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	base_info = &cxt->base_info;
	cur_slice_id = base_info->cur_slice_id;
	slice_num = base_info->slice_num;

	for (; cur_slice_id < slice_num; cur_slice_id++) {
		nlm_info = &cxt->nlm_info[cur_slice_id];
		start_col = base_info->slice_raw_pos_array
			[cur_slice_id].start_col;
		end_col = base_info->slice_raw_pos_array[cur_slice_id].end_col;
		start_row = base_info->slice_raw_pos_array
			[cur_slice_id].start_row;
		end_row = base_info->slice_raw_pos_array[cur_slice_id].end_row;
		nlm_info->col_center = in_ptr->nlm_col_center - start_col;
		nlm_info->row_center = in_ptr->nlm_row_center - start_row;
	}

exit:
	return rtn;
}

static int sprd_ispslice_cfa_info_set(struct slice_param_in *in_ptr,
	struct slice_context_info *cxt)
{
	int rtn = 0;
	uint32_t cur_slice_id = 0, slice_num = 0;
	struct slice_base_info *base_info = NULL;
	struct slice_cfa_info *cfa_info = NULL;
	uint32_t start_col = 0;
	uint32_t end_col = 0;

	if (!cxt) {
		pr_err("fail to get valid input handle!\n");
		rtn = -1;
		goto exit;
	}

	base_info = &cxt->base_info;
	cur_slice_id = base_info->cur_slice_id;
	slice_num = base_info->slice_num;

	for (; cur_slice_id < slice_num; cur_slice_id++) {
		cfa_info = &cxt->cfa_info[cur_slice_id];
		start_col = base_info->slice_raw_pos_array
			[cur_slice_id].start_col;
		end_col = base_info->slice_raw_pos_array[cur_slice_id].end_col;
		cfa_info->gbuf_addr_max = (end_col - start_col + 1) / 2 + 1;
	}

exit:
	return rtn;
}

static int sprd_ispslice_postcnr_info_set(struct slice_context_info *cxt)
{
	int rtn = 0;
	uint32_t cur_slice_id = 0, slice_num = 0;
	struct slice_base_info *base_info = NULL;
	struct slice_postcnr_info *postcnr_info = NULL;

	if (!cxt) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	base_info = &cxt->base_info;
	cur_slice_id = base_info->cur_slice_id;
	slice_num = base_info->slice_num;

	for (; cur_slice_id < slice_num; cur_slice_id++) {
		postcnr_info = &cxt->postcnr_info[cur_slice_id];
		postcnr_info->start_row_mod4 =
			base_info->slice_raw_pos_array[cur_slice_id].start_row
			& 0x3;
	}
exit:
	return rtn;
}

static int sprd_ispslice_ynr_info_set(struct slice_context_info *cxt,
	struct slice_param_in *in_ptr)
{
	int rtn = 0;
	uint32_t cur_slice_id = 0, slice_num = 0;
	struct slice_base_info *base_info = NULL;
	struct slice_ynr_info *ynr_info = NULL;

	if (!cxt) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	base_info = &cxt->base_info;
	cur_slice_id = base_info->cur_slice_id;
	slice_num = base_info->slice_num;

	for (; cur_slice_id < slice_num; cur_slice_id++) {
		ynr_info = &cxt->ynr_info[cur_slice_id];
		ynr_info->start_col =
			base_info->slice_raw_pos_array[cur_slice_id].start_col;
		ynr_info->start_row =
			base_info->slice_raw_pos_array[cur_slice_id].start_row;
		ynr_info->col_center = in_ptr->ynr_center_x;
		ynr_info->row_center = in_ptr->ynr_center_y;
	}
exit:
	return rtn;
}

static void sprd_ispslice_path_trim0_info_set(
	struct isp_scaler_slice_tmp *slice)
{
	uint32_t start, end;
	struct slice_scaler_info *out =
		(struct slice_scaler_info *)slice->scaler_slice;
	struct slice_scaler_path *in =
		(struct slice_scaler_path *)slice->scaler_yuv;

	/* trim0 x */
	start = slice->start_col + slice->overlap_bad_left;
	end = slice->end_col + 1 - slice->overlap_bad_right;
	if (slice->slice_col_num == 1) {
		out->trim0_start_x = in->trim0_start_x;
		out->trim0_size_x = in->trim0_size_x;
	} else {
		if (slice->cur_col == 0) {
			out->trim0_start_x = in->trim0_start_x;
			if (slice->trim0_end_x < end)
				out->trim0_size_x = in->trim0_size_x;
			else
				out->trim0_size_x = end - in->trim0_start_x;
		} else if ((slice->slice_col_num - 1) == slice->cur_col) {
			if (in->trim0_start_x > start) {
				out->trim0_start_x =
					in->trim0_start_x - slice->start_col;
				out->trim0_size_x = in->trim0_size_x;
			} else {
				out->trim0_start_x = slice->overlap_bad_left;
				out->trim0_size_x = slice->trim0_end_x - start;
			}
		} else {
			if (in->trim0_start_x < start) {
				out->trim0_start_x = slice->overlap_bad_left;
				if (slice->trim0_end_x < end)
					out->trim0_size_x =
					slice->trim0_end_x - start;
				else
					out->trim0_size_x = end - start;
			} else {
				out->trim0_start_x =
					in->trim0_start_x - slice->start_col;
				if (slice->trim0_end_x < end)
					out->trim0_size_x = in->trim0_size_x;
				else
					out->trim0_size_x =
					end - in->trim0_start_x;
			}
		}
	}

	/* trim0 y */
	start = slice->start_row + slice->overlap_bad_up;
	end = slice->end_row + 1 - slice->overlap_bad_down;
	if (slice->slice_row_num == 1) {
		out->trim0_start_y = in->trim0_start_y;
		out->trim0_size_y = in->trim0_size_y;
	} else {
		if (slice->cur_row == 0) {
			out->trim0_start_y = in->trim0_start_y;
			if (slice->trim0_end_y < end)
				out->trim0_size_y = in->trim0_size_y;
			else
				out->trim0_size_y = end - in->trim0_start_y;
		} else if ((slice->slice_row_num - 1) == slice->cur_row) {
			if (in->trim0_start_y < start) {
				out->trim0_start_y = slice->overlap_bad_up;
				out->trim0_size_y = slice->trim0_end_y - start;
			} else {
				out->trim0_start_y =
					in->trim0_start_y - slice->start_row;
				out->trim0_size_y = in->trim0_size_y;
			}
		} else {
			if (in->trim0_start_y < start) {
				out->trim0_start_y = slice->overlap_bad_up;
				if (slice->trim0_end_y < end)
					out->trim0_size_y =
					slice->trim0_end_y - start;
				else
					out->trim0_size_y = end - start;
			} else {
				out->trim0_start_y =
					in->trim0_start_y - slice->start_row;
				if (slice->trim0_end_y < end)
					out->trim0_size_y = in->trim0_size_y;
				else
					out->trim0_size_y =
					end - in->trim0_start_y;
			}
		}
	}

}

static void sprd_ispslice_path_trim1_info_set(
	struct isp_scaler_slice_tmp *slice,
	struct slice_scaler_info *scaler_info)
{
	struct slice_scaler_info *out =
		(struct slice_scaler_info *)slice->scaler_slice;
	struct slice_scaler_path *in =
		(struct slice_scaler_path *)slice->scaler_yuv;
	uint32_t trim_sum_x = 0;
	uint32_t trim_sum_y = 0;
	uint32_t pix_align = 32;
	uint32_t i = 0;

	if (in->trim0_start_x >= slice->scaler_slice_start_x &&
		(in->trim0_start_x <= slice->scaler_slice_end_x)) {
		out->trim1_start_x = 0;
		if (out->scaler_in_width == in->trim0_size_x)
			out->trim1_size_x = out->scaler_out_width;
		else
			out->trim1_size_x = out->scaler_out_width /
			pix_align * pix_align;
	} else {
		for (i = 1; i < slice->cur_col + 1; i++)
			trim_sum_x +=
			scaler_info[slice->cur_slice_id - i].trim1_size_x;

		if (slice->cur_col == slice->slice_col_num - 1) {
			out->trim1_size_x = in->scaler_out_width - trim_sum_x;
			out->trim1_start_x = out->scaler_out_width -
				out->trim1_size_x;
		} else {
			if (slice->trim0_end_x >= slice->start_col &&
				(slice->trim0_end_x <= slice->end_col + 1
				- slice->overlap_bad_right)) {
				out->trim1_size_x = in->scaler_out_width
					- trim_sum_x;
				out->trim1_start_x = out->scaler_out_width -
					out->trim1_size_x;
			} else {
				out->trim1_start_x =
					slice->scaler_out_width_temp -
					(in->scaler_out_width - trim_sum_x);
				out->trim1_size_x = (out->scaler_out_width -
					out->trim1_start_x) /
					pix_align * pix_align;
			}
		}
	}

	if (in->trim0_start_y >= slice->start_row &&
		(in->trim0_start_y <= slice->end_row + 1)) {
		out->trim1_start_y = 0;
		if (out->scaler_in_height == in->trim0_size_y)
			out->trim1_size_y = out->scaler_out_height;
		else
			out->trim1_size_y = out->scaler_out_height /
			pix_align * pix_align;
	} else {
		for (i = 1; i < slice->cur_row + 1; i++)
			trim_sum_y += scaler_info[slice->cur_slice_id -
			i * slice->slice_col_num].trim1_size_y;

		if (slice->cur_row == slice->slice_row_num - 1) {
			out->trim1_size_y = in->scaler_out_height - trim_sum_y;
			out->trim1_start_y = out->scaler_out_height -
				out->trim1_size_y;
		} else {
			if (slice->trim0_end_y >= slice->start_row &&
				(slice->trim0_end_y <= slice->end_row + 1
				- slice->overlap_bad_down)) {
				out->trim1_size_y = in->scaler_out_height
					- trim_sum_y;
				out->trim1_start_y = out->scaler_out_height -
					out->trim1_size_y;
			} else {
				out->trim1_start_y =
					slice->scaler_out_height_temp -
					(in->scaler_out_height - trim_sum_y);
				out->trim1_size_y = (out->scaler_out_height -
					out->trim1_start_y) /
					pix_align * pix_align;
			}
		}
	}
}

static void sprd_ispslice_path_deci_info_set(struct isp_scaler_slice_tmp *slice)
{
	uint32_t start = 0;
	struct slice_scaler_info *out =
		(struct slice_scaler_info *)slice->scaler_slice;
	struct slice_scaler_path *in =
		(struct slice_scaler_path *)slice->scaler_yuv;

	slice->deci_x = in->deci_x;
	slice->deci_y = in->deci_y;
	slice->deci_x_align = slice->deci_x * 2;

	start = slice->start_col + slice->overlap_bad_left;
	if (in->trim0_start_x >= slice->start_col &&
		(in->trim0_start_x <= slice->end_col + 1)) {
		out->trim0_size_x = out->trim0_size_x /
			slice->deci_x_align * slice->deci_x_align;
	} else {
		slice->trim0_start_adjust_x =
			(start + slice->deci_x_align - 1) /
			slice->deci_x_align * slice->deci_x_align - start;
		out->trim0_start_x += slice->trim0_start_adjust_x;
		out->trim0_size_x -= slice->trim0_start_adjust_x;
		out->trim0_size_x = out->trim0_size_x /
			slice->deci_x_align * slice->deci_x_align;
	}

	if (in->odata_mode == 0)
		slice->deci_y_align = slice->deci_y;
	else
		slice->deci_y_align = slice->deci_y * 2;

	start = slice->start_row + slice->overlap_bad_up;
	if (in->trim0_start_y >= slice->start_row &&
		(in->trim0_start_y <= slice->end_row + 1)) {
		out->trim0_size_y = out->trim0_size_y /
			slice->deci_y_align * slice->deci_y_align;
	} else {
		slice->trim0_start_adjust_y =
			(start + slice->deci_y_align - 1) /
			slice->deci_y_align * slice->deci_y_align - start;
		out->trim0_start_y += slice->trim0_start_adjust_y;
		out->trim0_size_y -= slice->trim0_start_adjust_y;
		out->trim0_size_y = out->trim0_size_y /
			slice->deci_y_align * slice->deci_y_align;
	}

	out->scaler_in_width = out->trim0_size_x / slice->deci_x;
	out->scaler_in_height = out->trim0_size_y / slice->deci_y;
}

static void sprd_ispslice_path_scaler_info_set(
	struct isp_scaler_slice_tmp *slice)
{
	struct slice_scaler_info *out =
		(struct slice_scaler_info *)slice->scaler_slice;
	struct slice_scaler_path *in =
		(struct slice_scaler_path *)slice->scaler_yuv;
	uint32_t scl_factor_in = 0, scl_factor_out = 0;
	uint32_t  initial_phase = 0, last_phase = 0, phase_in = 0;
	uint32_t phase_tmp = 0, scl_temp = 0, out_tmp = 0;
	uint32_t start, end = 0;
	uint32_t tap_hor = 0, tap_ver = 0;
	uint32_t tap_hor_uv = 0, tap_ver_uv = 0;
	uint32_t tmp, n = 0;

	if (in->scaler_bypass == 0) {
		scl_factor_in = in->scaler_factor_in / 2;
		scl_factor_out = in->scaler_factor_out / 2;
		initial_phase = 0;
		last_phase = initial_phase +
			scl_factor_in * (in->scaler_out_width / 2 - 1);
		tap_hor = 8;
		tap_hor_uv = tap_hor / 2;

		start = slice->start_col + slice->overlap_bad_left +
			slice->deci_x_align - 1;
		end = slice->end_col + 1 - slice->overlap_bad_right +
			slice->deci_x_align - 1;
		if (in->trim0_start_x >= slice->start_col &&
			(in->trim0_start_x <= slice->end_col + 1)) {
			phase_in = 0;
			if (out->scaler_in_width ==
				in->trim0_size_x / slice->deci_x)
				phase_tmp = last_phase;
			else
				phase_tmp = (out->scaler_in_width / 2 -
				tap_hor_uv / 2) * scl_factor_out -
				scl_factor_in / 2 - 1;
			out_tmp = (phase_tmp - phase_in) / scl_factor_in + 1;
			out->scaler_out_width = out_tmp * 2;
		} else {
			phase_in = (tap_hor_uv / 2) * scl_factor_out;
			if (slice->cur_col == slice->slice_col_num - 1) {
				phase_tmp = last_phase -
					((in->trim0_size_x / 2) /
					slice->deci_x -
					out->scaler_in_width / 2) *
					scl_factor_out;
				out_tmp = (phase_tmp-phase_in) /
				scl_factor_in + 1;
				out->scaler_out_width = out_tmp * 2;
				phase_in = phase_tmp -
				(out_tmp - 1) * scl_factor_in;
			} else {
				if (slice->trim0_end_x >= slice->start_col
					&& (slice->trim0_end_x <= slice->end_col
					+ 1 - slice->overlap_bad_right)) {
					phase_tmp = last_phase -
					((in->trim0_size_x / 2) /
					slice->deci_x -
					out->scaler_in_width / 2) *
					scl_factor_out;
					out_tmp = (phase_tmp - phase_in) /
						scl_factor_in + 1;
					out->scaler_out_width = out_tmp * 2;
					phase_in = phase_tmp - (out_tmp - 1)
						* scl_factor_in;
				} else {
					initial_phase = ((((start /
					slice->deci_x_align *
					slice->deci_x_align
					- in->trim0_start_x) /
					slice->deci_x) / 2 +
					(tap_hor_uv / 2)) * (scl_factor_out) +
					(scl_factor_in - 1)) / scl_factor_in *
					scl_factor_in;
					slice->scaler_out_width_temp =
					((last_phase - initial_phase) /
					scl_factor_in + 1) * 2;

					scl_temp = ((end / slice->deci_x_align *
					slice->deci_x_align -
					in->trim0_start_x) /
					slice->deci_x) / 2;
					last_phase =
					((scl_temp - tap_hor_uv / 2) *
					(scl_factor_out) -
					scl_factor_in / 2 - 1) /
					scl_factor_in * scl_factor_in;

					out_tmp = (last_phase - initial_phase)
					/ scl_factor_in + 1;
					out->scaler_out_width = out_tmp * 2;
					phase_in = initial_phase - (((start /
					slice->deci_x_align *
					slice->deci_x_align -
					in->trim0_start_x) /
					slice->deci_x) / 2) *
					scl_factor_out;
				}
			}
		}

		sprd_ispslice_scaler_phase_calc(phase_in * 4,
			scl_factor_out * 2,
			&out->scaler_ip_int, &out->scaler_ip_rmd);
		sprd_ispslice_scaler_phase_calc(phase_in, scl_factor_out,
			&out->scaler_cip_int, &out->scaler_cip_rmd);

		scl_factor_in = in->scaler_ver_factor_in;
		scl_factor_out = in->scaler_ver_factor_out;
		initial_phase = 0;
		last_phase = initial_phase +
			scl_factor_in * (in->scaler_out_height - 1);
		tap_ver = in->scaler_y_ver_tap > in->scaler_uv_ver_tap ?
			in->scaler_y_ver_tap : in->scaler_uv_ver_tap;
		tap_ver += 2;
		tap_ver_uv = tap_ver;

		start = slice->start_row + slice->overlap_bad_up +
			slice->deci_y_align - 1;
		end = slice->end_row + 1 - slice->overlap_bad_down +
			slice->deci_y_align - 1;
		if (in->trim0_start_y >= slice->start_row &&
			(in->trim0_start_y <= slice->end_row + 1)) {
			phase_in = 0;
			if (out->scaler_in_height ==
				in->trim0_size_y / slice->deci_y)
				phase_tmp = last_phase;
			else
				phase_tmp = (out->scaler_in_height -
				tap_ver_uv / 2) * scl_factor_out - 1;
			out_tmp = (phase_tmp - phase_in) / scl_factor_in + 1;
			if (out_tmp % 2 == 1)
				out_tmp -= 1;
			out->scaler_out_height = out_tmp;
		} else {
			phase_in = (tap_ver_uv / 2) * scl_factor_out;
			if (slice->cur_row == slice->slice_row_num - 1) {
				phase_tmp = last_phase -
					(in->trim0_size_y / slice->deci_y -
					out->scaler_in_height) * scl_factor_out;
				out_tmp = (phase_tmp - phase_in) /
				scl_factor_in + 1;
				if (out_tmp % 2 == 1)
					out_tmp -= 1;
				if (in->odata_mode == 1 && out_tmp % 4 != 0)
					out_tmp = out_tmp / 4 * 4;
				out->scaler_out_height = out_tmp;
				phase_in = phase_tmp -
				(out_tmp - 1) * scl_factor_in;
			} else {
				if (slice->trim0_end_y >= slice->start_row &&
					(slice->trim0_end_y <=
					slice->end_row + 1
					- slice->overlap_bad_down)) {
					phase_tmp = last_phase -
					(in->trim0_size_y / slice->deci_y -
					out->scaler_in_height) * scl_factor_out;
					out_tmp = (phase_tmp - phase_in) /
						scl_factor_in + 1;
					if (out_tmp % 2 == 1)
						out_tmp -= 1;
					if (in->odata_mode == 1 &&
						out_tmp % 4 != 0)
						out_tmp = out_tmp / 4 * 4;
					out->scaler_out_height = out_tmp;
					phase_in = phase_tmp - (out_tmp - 1)
						* scl_factor_in;
				} else {
					initial_phase = (((start /
					slice->deci_y_align *
					slice->deci_y_align
					- in->trim0_start_y) / slice->deci_y +
					(tap_ver_uv / 2)) * (scl_factor_out) +
					(scl_factor_in - 1)) /
					(scl_factor_in * 2)
					* (scl_factor_in * 2);
					slice->scaler_out_height_temp =
						(last_phase - initial_phase) /
						scl_factor_in + 1;
					scl_temp = (end / slice->deci_y_align *
					slice->deci_y_align -
					in->trim0_start_y) /
					slice->deci_y;
					last_phase = ((scl_temp -
					tap_ver_uv / 2) *
					(scl_factor_out) - 1) / scl_factor_in
					* scl_factor_in;
					out_tmp = (last_phase - initial_phase) /
						scl_factor_in + 1;
					if (out_tmp % 2 == 1)
						out_tmp -= 1;
					if (in->odata_mode == 1 &&
						out_tmp % 4 != 0)
						out_tmp = out_tmp / 4 * 4;
					out->scaler_out_height = out_tmp;
					phase_in = initial_phase - (start /
					slice->deci_y_align *
					slice->deci_y_align -
					in->trim0_start_y) / slice->deci_y
					* scl_factor_out;
				}
			}
		}

		sprd_ispslice_scaler_phase_calc(phase_in, scl_factor_out,
			&out->scaler_ip_int_ver, &out->scaler_ip_rmd_ver);
		if (in->odata_mode == 1) {
			phase_in /= 2;
			scl_factor_out /= 2;
		}
		sprd_ispslice_scaler_phase_calc(phase_in, scl_factor_out,
			&out->scaler_cip_int_ver, &out->scaler_cip_rmd_ver);

		if (out->scaler_ip_int >= 16) {
			tmp = out->scaler_ip_int;
			n = (tmp >> 3) - 1;
			out->trim0_start_x += 8 * n * slice->deci_x;
			out->trim0_size_x -= 8 * n * slice->deci_x;
			out->scaler_ip_int -= 8 * n;
			out->scaler_cip_int -= 4 * n;
		}
		if (out->scaler_ip_int >= 16)
			pr_err("fail to get hor slice initial phase: overflowed!\n");
		if (out->scaler_ip_int_ver >= 16) {
			tmp = out->scaler_ip_int_ver;
			n = (tmp >> 3) - 1;
			out->trim0_start_y += 8 * n * slice->deci_y;
			out->trim0_size_y -= 8 * n * slice->deci_y;
			out->scaler_ip_int_ver -= 8 * n;
			out->scaler_cip_int_ver -= 8 * n;
		}
		if (out->scaler_ip_int_ver >= 16)
			pr_err("fail to get ver slice initial phase: overflowed!\n");
	} else {
		out->scaler_out_width = out->scaler_in_width;
		out->scaler_out_height = out->scaler_in_height;
		start = slice->start_col + slice->overlap_bad_left +
			slice->trim0_start_adjust_x + slice->deci_x_align - 1;
		slice->scaler_out_width_temp = (in->trim0_size_x - (start /
			slice->deci_x_align * slice->deci_x_align -
			in->trim0_start_x)) / slice->deci_x;
		start = slice->start_row + slice->overlap_bad_up +
			slice->trim0_start_adjust_y + slice->deci_y_align - 1;
		slice->scaler_out_height_temp = (in->trim0_size_y - (start /
			slice->deci_y_align * slice->deci_y_align -
			in->trim0_start_y)) / slice->deci_y;
	}
}

static int sprd_ispslice_path_info_set(struct slice_scaler_info *scaler_info,
	struct slice_scaler_path *scaler_frame,
	struct slice_base_info *base_info,
	uint32_t row, uint32_t col)
{
	int rtn = 0;
	uint32_t cur_slice_id = 0;
	struct isp_scaler_slice_tmp slice = {0};
	uint32_t start_limit_x = 0, start_limit_y = 0;
	uint32_t end_limit_x = 0, end_limit_y = 0;

	if (!scaler_info || !scaler_frame || !base_info) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	cur_slice_id = base_info->cur_slice_id;
	slice.cur_slice_id = cur_slice_id;
	slice.cur_col = col;
	slice.cur_row = row;
	slice.slice_col_num = base_info->slice_col_num;
	slice.slice_row_num = base_info->slice_row_num;
	slice.start_col = base_info->slice_raw_pos_array
		[cur_slice_id].start_col;
	slice.end_col = base_info->slice_raw_pos_array[cur_slice_id].end_col;
	slice.start_row = base_info->slice_raw_pos_array
		[cur_slice_id].start_row;
	slice.end_row = base_info->slice_raw_pos_array[cur_slice_id].end_row;
	slice.trim0_end_x = scaler_frame->trim0_start_x +
		scaler_frame->trim0_size_x;
	slice.trim0_end_y = scaler_frame->trim0_start_y +
		scaler_frame->trim0_size_y;
	slice.overlap_bad_up = base_info->raw_overlap_up -
		YUVSCALER_OVERLAP_UP;
	slice.overlap_bad_down = base_info->raw_overlap_down -
		YUVSCALER_OVERLAP_DOWN;
	slice.overlap_bad_left = base_info->raw_overlap_left -
		YUVSCALER_OVERLAP_LEFT;
	slice.overlap_bad_right = base_info->raw_overlap_right -
		YUVSCALER_OVERLAP_RIGHT;
	if (scaler_frame->deci_x > 3)
		slice.overlap_bad_right = base_info->raw_overlap_right -
		YUVSCALER_OVERLAP_RIGHT_PLUS;
	slice.scaler_slice = (uint32_t *)&scaler_info[cur_slice_id];
	slice.scaler_yuv = (uint32_t *)scaler_frame;

	start_limit_x = slice.end_col + 1 - base_info->raw_overlap_right
		- YUVSCALER_OVERLAP_LEFT;
	end_limit_x = slice.start_col + 1 + base_info->raw_overlap_left
		+ YUVSCALER_OVERLAP_RIGHT;
	start_limit_y = slice.end_row + 1 - base_info->raw_overlap_up
		- YUVSCALER_OVERLAP_DOWN;
	end_limit_y = slice.start_row + 1 + base_info->raw_overlap_down
		+ YUVSCALER_OVERLAP_UP;

	pr_debug("slice limit start x%d y%d end x%d y%d\n",
		start_limit_x, start_limit_y, end_limit_x, end_limit_y);
	if (slice.cur_col == 0)
		slice.scaler_slice_start_x = 0;
	else
		slice.scaler_slice_start_x = slice.start_col
			+ slice.overlap_bad_left;

	if (slice.cur_col == slice.slice_col_num - 1)
		slice.scaler_slice_end_x = slice.end_col + 1;
	else
		slice.scaler_slice_end_x = slice.end_col
			+ 1 - slice.overlap_bad_right;

	if (scaler_frame->trim0_start_y > start_limit_y
		|| scaler_frame->trim0_start_x >= start_limit_x
		|| slice.trim0_end_y < end_limit_y
		|| slice.trim0_end_x <= end_limit_x) {
		memset(slice.scaler_slice, 0, sizeof(struct slice_scaler_info));
	} else {
		sprd_ispslice_path_trim0_info_set(&slice);
		sprd_ispslice_path_deci_info_set(&slice);
		sprd_ispslice_path_scaler_info_set(&slice);
		sprd_ispslice_path_trim1_info_set(&slice, scaler_info);
	}

	scaler_info[cur_slice_id].src_size_x = slice.end_col -
		slice.start_col + 1;
	scaler_info[cur_slice_id].src_size_y = slice.end_row -
		slice.start_row + 1;
	scaler_info[cur_slice_id].dst_size_x =
		scaler_info[cur_slice_id].scaler_out_width;
	scaler_info[cur_slice_id].dst_size_y =
		scaler_info[cur_slice_id].scaler_out_height;

exit:
	return rtn;
}

static int sprd_ispslice_scaler_info_set(struct slice_param_in *in_ptr,
	struct slice_context_info *cxt)
{
	int rtn = 0;
	struct slice_base_info *base_info = NULL;
	struct slice_scaler_info *scaler_info = NULL;
	struct slice_scaler_path *scaler_frame = NULL;
	uint32_t slice_col_num = 0, slice_row_num = 0;
	uint32_t r = 0, c = 0;

	if (!in_ptr || !cxt) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	base_info = &cxt->base_info;
	slice_col_num = base_info->slice_col_num;
	slice_row_num = base_info->slice_row_num;

	for (r = 0; r < slice_row_num; r++) {
		for (c = 0; c < slice_col_num; c++) {
			base_info->cur_slice_id = r * slice_col_num + c;
			if (in_ptr->cap_slice_need == 1) {
				scaler_info = cxt->scaler_info[SLICE_PATH_CAP];
				scaler_frame =
					&in_ptr->scaler_frame[SLICE_PATH_CAP];
				sprd_ispslice_path_info_set(scaler_info,
					scaler_frame, base_info, r, c);
			}
		}
	}

	base_info->cur_slice_id = 0;

exit:
	return rtn;
}

static int sprd_ispslice_store_pitch_get(struct slice_pitch *pitch_ptr,
	enum isp_store_format format, uint32_t width)
{
	int rtn = 0;

	switch (format) {
	case ISP_STORE_YUV422_3FRAME:
		pitch_ptr->chn0 = width;
		pitch_ptr->chn1 = width >> 1;
		pitch_ptr->chn2 = width >> 1;
		break;
	case ISP_STORE_YUV422_2FRAME:
	case ISP_STORE_YVU422_2FRAME:
	case ISP_STORE_YUV420_2FRAME:
	case ISP_STORE_YVU420_2FRAME:
		pitch_ptr->chn0 = width;
		pitch_ptr->chn1 = width;
		break;
	case ISP_STORE_UYVY:
		pitch_ptr->chn0 = width << 1;
		break;
	case ISP_STORE_RAW10:
		pitch_ptr->chn0 = width << 1;
		break;
	default:
		pr_err("fail to get valid store format %d\n", format);
		rtn = -1;
		break;
	}

	return rtn;
}

static int sprd_ispslice_store_info_set(struct slice_store_info *store_info,
	struct slice_store_path *store_frame,
	struct slice_base_info *base_info,
	struct slice_scaler_info *scaler_info, uint32_t scl_bypass)
{
	int rtn = 0;
	uint32_t cur_slice_id = 0, cur_slice_row = 0;
	uint32_t scl_out_width = 0, scl_out_height = 0;
	uint32_t overlap_left = 0, overlap_up = 0,
		overlap_right = 0, overlap_down = 0;
	uint32_t start_col = 0, end_col = 0,
		start_row = 0, end_row = 0;
	uint32_t start_row_out = 0, start_col_out = 0;
	uint32_t tmp_slice_id = 0;
	uint32_t ch0_offset = 0;
	uint32_t ch1_offset = 0;
	uint32_t ch2_offset = 0;
	struct slice_pitch store_pitch = {0};
	struct slice_addr *address = NULL;

	if (!store_info || !store_frame || !base_info || !scaler_info) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	cur_slice_id = base_info->cur_slice_id;
	address = &store_frame->addr;

	if (sprd_ispslice_store_pitch_get(&store_pitch, store_frame->format,
		store_frame->size.width)) {
		pr_err("fail to get slice store pitch\n");
		rtn = -EINVAL;
		goto exit;
	}

	for (; cur_slice_id < base_info->slice_num; cur_slice_id++) {
		cur_slice_row = cur_slice_id/base_info->slice_col_num;
		store_info[cur_slice_id].addr.chn0 = address->chn0;
		store_info[cur_slice_id].addr.chn1 = address->chn1;
		store_info[cur_slice_id].addr.chn2 = address->chn2;
		if (scl_bypass == 0) {
			scl_out_width = scaler_info[cur_slice_id].trim1_size_x;
			scl_out_height = scaler_info[cur_slice_id].trim1_size_y;
			overlap_left = 0;
			overlap_up = 0;
			overlap_down = 0;
			overlap_right = 0;

			store_info[cur_slice_id].size.width =
				scl_out_width-overlap_left-overlap_right;
			store_info[cur_slice_id].size.height =
				scl_out_height-overlap_up-overlap_down;

			tmp_slice_id = cur_slice_id;
			start_col_out = 0;
			while ((int)(tmp_slice_id - 1) >=
				(int)(cur_slice_row*base_info->slice_col_num)) {
				tmp_slice_id--;
				start_col_out +=
					store_info[tmp_slice_id].size.width;
			}

			tmp_slice_id = cur_slice_id;
			start_row_out = 0;
			while ((int)(tmp_slice_id-
				base_info->slice_col_num) >= 0) {
				tmp_slice_id -= base_info->slice_col_num;
				start_row_out +=
					store_info[tmp_slice_id].size.height;
			}

			store_info[cur_slice_id].border.left_border = 0;
			store_info[cur_slice_id].border.right_border = 0;
			store_info[cur_slice_id].border.up_border = 0;
			store_info[cur_slice_id].border.down_border = 0;
		} else {
			start_col = base_info->slice_raw_pos_array
				[cur_slice_id].start_col;
			end_col = base_info->slice_raw_pos_array
				[cur_slice_id].end_col;
			start_row = base_info->slice_raw_pos_array
				[cur_slice_id].start_row;
			end_row = base_info->slice_raw_pos_array
				[cur_slice_id].end_row;
			overlap_left = base_info->slice_raw_overlap_array
				[cur_slice_id].overlap_left;
			overlap_up = base_info->slice_raw_overlap_array
				[cur_slice_id].overlap_up;
			overlap_right = base_info->slice_raw_overlap_array
				[cur_slice_id].overlap_right;
			overlap_down = base_info->slice_raw_overlap_array
				[cur_slice_id].overlap_down;
			start_row_out = start_row + overlap_up;
			start_col_out = start_col + overlap_left;
			store_info[cur_slice_id].size.height =
				end_row - start_row + 1 -
				overlap_up - overlap_down;
			store_info[cur_slice_id].size.width =
				end_col - start_col + 1 -
				overlap_left - overlap_right;
			store_info[cur_slice_id].border.left_border =
				overlap_left;
			store_info[cur_slice_id].border.right_border =
				overlap_right;
			store_info[cur_slice_id].border.up_border =
				overlap_up;
			store_info[cur_slice_id].border.down_border =
				overlap_down;
		}

		switch (store_frame->format) {
		case ISP_STORE_YUV422_3FRAME:
			ch0_offset = start_row_out * store_pitch.chn0 +
				start_col_out;
			ch1_offset = start_row_out * store_pitch.chn1 +
				((start_col_out + 1) >> 1);
			ch2_offset = start_row_out * store_pitch.chn2 +
				((start_col_out + 1) >> 1);
			break;
		case ISP_STORE_UYVY:
			ch0_offset = start_row_out * store_pitch.chn0 +
				start_col_out * 2;
			break;
		case ISP_STORE_YUV422_2FRAME:
		case ISP_STORE_YVU422_2FRAME:
			ch0_offset = start_row_out * store_pitch.chn0 +
				start_col_out;
			ch1_offset = start_row_out * store_pitch.chn1 +
				start_col_out;
			break;
		case ISP_STORE_YUV420_2FRAME:
		case ISP_STORE_YVU420_2FRAME:
			ch0_offset = start_row_out * store_pitch.chn0 +
				start_col_out;
			ch1_offset = ((start_row_out *
				store_pitch.chn1 + 1) >> 1) +
				start_col_out;
			break;
		case ISP_STORE_YUV420_3FRAME:
			ch0_offset = start_row_out * store_pitch.chn0 +
				start_col_out;
			ch1_offset = ((start_row_out *
				store_pitch.chn1 + 1) >> 1) +
				((start_col_out + 1) >> 1);
			ch2_offset = ((start_row_out *
				store_pitch.chn2 + 1) >> 1) +
				((start_col_out + 1) >> 1);
			break;
		default:
			break;
		}

		store_info[cur_slice_id].addr.chn0 += ch0_offset;
		store_info[cur_slice_id].addr.chn1 += ch1_offset;
		store_info[cur_slice_id].addr.chn2 += ch2_offset;
	}

exit:
	return rtn;
}

static int sprd_ispslice_slice_store_info_set(struct slice_param_in *in_ptr,
	struct slice_context_info *cxt)
{
	int rtn = 0;
	uint32_t scl_bypass = 0;
	struct slice_base_info *base_info = NULL;
	struct slice_store_info *store_info = NULL;
	struct slice_store_path *store_frame = NULL;
	struct slice_scaler_info *scaler_info = NULL;

	if (!in_ptr || !cxt) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	base_info = &cxt->base_info;
	if (in_ptr->cap_slice_need == 1) {
		store_info = cxt->store_info[SLICE_PATH_CAP];
		scaler_info = cxt->scaler_info[SLICE_PATH_CAP];
		store_frame = &in_ptr->store_frame[SLICE_PATH_CAP];
		scl_bypass = in_ptr->scaler_frame[SLICE_PATH_CAP].scaler_bypass;
		sprd_ispslice_store_info_set(store_info, store_frame, base_info,
			scaler_info, scl_bypass);
	}

exit:
	return rtn;
}

static int sprd_ispslice_store_fbc_info_set(
	struct slice_store_fbc_info *store_fbc_info,
	struct slice_store_fbc_path *store_fbc_frame,
	struct slice_base_info *base_info,
	struct slice_scaler_info *scaler_info, uint32_t scl_bypass)
{
	int rtn = 0;
	uint32_t slice_col_num = base_info->slice_col_num;
	uint32_t slice_start_col = 0;
	uint32_t slice_id = 0;
	uint32_t cur_slice_row = 0;
	uint32_t cur_slice_col = 0;
	uint32_t overlap_left = 0;
	uint32_t overlap_up = 0;
	uint32_t overlap_down = 0;
	uint32_t overlap_right = 0;
	uint32_t scl_out_width = 0, scl_out_height = 0;
	uint32_t tmp_slice_id = 0;
	uint32_t slice_start_col_tile_num = 0;

	struct slice_store_fbc_info *slice_param = store_fbc_info;

	if (!store_fbc_info || !store_fbc_frame || !base_info || !scaler_info) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	slice_id = base_info->cur_slice_id;

	for (; slice_id < base_info->slice_num; slice_id++) {
		cur_slice_row = slice_id / slice_col_num;
		cur_slice_col = slice_id % slice_col_num;

		overlap_up = 0;
		overlap_left = 0;
		overlap_down = 0;
		overlap_right = 0;
		scl_out_width = scaler_info[slice_id].trim1_size_x;
		scl_out_height = scaler_info[slice_id].trim1_size_y;

		slice_param[slice_id].overlap_up = overlap_up;
		slice_param[slice_id].overlap_left = overlap_left;
		slice_param[slice_id].overlap_down = overlap_down;
		slice_param[slice_id].overlap_right = overlap_right;

		slice_param[slice_id].slice_out_height =
			scl_out_height - overlap_up - overlap_down;
		slice_param[slice_id].slice_out_width =
			scl_out_width - overlap_left - overlap_right;

		tmp_slice_id = slice_id;
		slice_start_col = 0;
		while ((int)(tmp_slice_id - 1) >=
			(int)(cur_slice_row * slice_col_num)) {
			tmp_slice_id--;
			slice_start_col +=
				(slice_param[tmp_slice_id].slice_out_width +
				32 - 1) / 32 * 32;
		}

		slice_start_col_tile_num = slice_start_col / 32;

		slice_param[slice_id].yheader_addr =
			store_fbc_frame->yheader_addr +
				slice_start_col_tile_num * 16;
		slice_param[slice_id].yaddr =
			store_fbc_frame->yaddr +
				slice_start_col_tile_num * 384;
		slice_param[slice_id].slice_offset =
			store_fbc_frame->header_offset +
				slice_start_col_tile_num * 384;
		pr_info("slice afbc yheader = %x\n",
			slice_param[slice_id].yheader_addr);
		pr_info("slice afbc yaddr = %x\n",
			slice_param[slice_id].yaddr);
		pr_info("slice afbc offset = %x\n",
			slice_param[slice_id].slice_offset);
	}

exit:
	return rtn;
}

static int sprd_ispslice_slice_store_fbc_info_set(struct slice_param_in *in_ptr,
	struct slice_context_info *cxt)
{
	int rtn = 0;
	uint32_t scl_bypass = 0;
	struct slice_base_info *base_info = NULL;
	struct slice_store_fbc_info *store_fbc_info = NULL;
	struct slice_store_fbc_path *store_fbc_frame = NULL;
	struct slice_scaler_info *scaler_info = NULL;

	if (!in_ptr || !cxt) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	base_info = &cxt->base_info;
	if (in_ptr->cap_slice_need == 1) {
		store_fbc_info = cxt->store_fbc_info[SLICE_PATH_CAP];
		scaler_info = cxt->scaler_info[SLICE_PATH_CAP];
		store_fbc_frame = &in_ptr->store_fbc_frame[SLICE_PATH_CAP];
		scl_bypass =
			in_ptr->scaler_frame[SLICE_PATH_CAP].scaler_bypass;
		sprd_ispslice_store_fbc_info_set(store_fbc_info,
			store_fbc_frame, base_info, scaler_info, scl_bypass);
	}

exit:
	return rtn;
}

static int sprd_ispslice_3dnr_memctrl_set(struct slice_store_path *in_ptr,
	struct slice_context_info *cxt)
{
	int rtn = 0;
	struct slice_base_info *base_info = NULL;
	struct slice_3dnr_mem_ctrl *nr3_mem_ctrl = NULL;
	struct slice_addr *address = NULL;
	uint32_t cur_slice_id, slice_num;
	struct slice_pitch fetch_pitch = {0};
	uint32_t start_col = 0, end_col = 0;
	uint32_t start_row = 0, end_row = 0;
	uint32_t ch0_offset = 0;
	uint32_t ch1_offset = 0;
	uint32_t ch2_offset = 0;

	if (!in_ptr || !cxt) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	base_info = &cxt->base_info;
	address = &in_ptr->addr;
	cur_slice_id = base_info->cur_slice_id;
	slice_num = base_info->slice_num;

	sprd_ispslice_fetch_pitch_get(&fetch_pitch, in_ptr->format,
		base_info->img_width);
	for (; cur_slice_id < slice_num; cur_slice_id++) {
		nr3_mem_ctrl = &cxt->nr3_mem_ctrl[cur_slice_id];
		nr3_mem_ctrl->addr.chn0 = address->chn0;
		nr3_mem_ctrl->addr.chn1 = address->chn1;
		nr3_mem_ctrl->addr.chn2 = address->chn2;
		start_col = base_info->slice_raw_pos_array
			[cur_slice_id].start_col;
		end_col = base_info->slice_raw_pos_array[cur_slice_id].end_col;
		start_row = base_info->slice_raw_pos_array
			[cur_slice_id].start_row;
		end_row = base_info->slice_raw_pos_array[cur_slice_id].end_row;
		switch (in_ptr->format) {
		case ISP_FETCH_YUV422_3FRAME:
			ch0_offset = start_row * fetch_pitch.chn0
				+ start_col;
			ch1_offset = start_row * fetch_pitch.chn1
				+ ((start_col + 1) >> 1);
			ch2_offset = start_row * fetch_pitch.chn2
				+ ((start_col + 1) >> 1);
			break;
		case ISP_FETCH_NORMAL_RAW10:
		case ISP_FETCH_UYVY:
			ch0_offset = start_row * fetch_pitch.chn0
				+ start_col * 2;
			break;
		case ISP_FETCH_YUV422_2FRAME:
		case ISP_FETCH_YVU422_2FRAME:
			ch0_offset = start_row * fetch_pitch.chn0
				+ start_col;
			ch1_offset = start_row * fetch_pitch.chn1
				+ start_col;
			break;
		case ISP_FETCH_YUV420_2FRAME:
		case ISP_FETCH_YVU420_2FRAME:
			ch0_offset = start_row * fetch_pitch.chn0
				+ start_col;
			ch1_offset = ((start_row * fetch_pitch.chn1
				+ 1) >> 1) + start_col;
			break;
		case ISP_FETCH_CSI2_RAW10:
			ch0_offset = start_row * fetch_pitch.chn0
				+ (start_col >> 2) * 5
				+ (start_col & 0x3);
			break;
		default:
			break;
		}

		nr3_mem_ctrl->first_line_mode = 0;
		nr3_mem_ctrl->last_line_mode = 0;
		nr3_mem_ctrl->start_row = start_row;
		nr3_mem_ctrl->start_col = start_col;
		nr3_mem_ctrl->addr.chn0 += ch0_offset;
		nr3_mem_ctrl->addr.chn1 += ch1_offset;
		nr3_mem_ctrl->addr.chn2 += ch2_offset;
		nr3_mem_ctrl->src.height = end_row - start_row + 1;
		nr3_mem_ctrl->src.width = end_col - start_col + 1;
		nr3_mem_ctrl->ft_y.height = end_row - start_row + 1;
		nr3_mem_ctrl->ft_y.width = end_col - start_col + 1;
		nr3_mem_ctrl->ft_uv.height = (end_row - start_row + 1) >> 1;
		nr3_mem_ctrl->ft_uv.width = end_col - start_col + 1;
	}
exit:
	return rtn;
}

static int sprd_ispslice_3dnr_store_info_set(struct slice_store_path *in_ptr,
	struct slice_context_info *cxt)
{
	int rtn = 0;
	struct slice_base_info *base_info = NULL;
	struct slice_3dnr_store_info *store_info = NULL;
	struct slice_addr *address = NULL;
	uint32_t cur_slice_id = 0;
	uint32_t overlap_left = 0, overlap_up = 0,
		overlap_right = 0, overlap_down = 0;
	uint32_t start_col = 0, end_col = 0,
		start_row = 0, end_row = 0;
	uint32_t start_row_out = 0, start_col_out = 0;
	uint32_t ch0_offset = 0;
	uint32_t ch1_offset = 0;
	uint32_t ch2_offset = 0;
	struct slice_pitch store_pitch = {0};

	if (!in_ptr || !cxt) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	base_info = &cxt->base_info;
	cur_slice_id = base_info->cur_slice_id;
	address = &in_ptr->addr;

	if (sprd_ispslice_store_pitch_get(&store_pitch, in_ptr->format,
		base_info->img_width)) {
		pr_err("fail to get slice store pitch\n");
		rtn = -EINVAL;
		goto exit;
	}

	for (; cur_slice_id < base_info->slice_num; cur_slice_id++) {
		store_info = &cxt->store_3dnr_info[cur_slice_id];
		store_info->addr.chn0 = address->chn0;
		store_info->addr.chn1 = address->chn1;
		store_info->addr.chn2 = address->chn2;
		start_col = base_info->slice_raw_pos_array
			[cur_slice_id].start_col;
		end_col = base_info->slice_raw_pos_array
			[cur_slice_id].end_col;
		start_row = base_info->slice_raw_pos_array
			[cur_slice_id].start_row;
		end_row = base_info->slice_raw_pos_array
			[cur_slice_id].end_row;
		overlap_left = base_info->slice_raw_overlap_array
			[cur_slice_id].overlap_left;
		overlap_up = base_info->slice_raw_overlap_array
			[cur_slice_id].overlap_up;
		overlap_right = base_info->slice_raw_overlap_array
			[cur_slice_id].overlap_right;
		overlap_down = base_info->slice_raw_overlap_array
			[cur_slice_id].overlap_down;
		start_row_out = start_row + overlap_up;
		start_col_out = start_col + overlap_left;
		store_info->size.height =
			end_row - start_row + 1 - overlap_up - overlap_down;
		store_info->size.width =
			end_col - start_col + 1 - overlap_left - overlap_right;

		switch (in_ptr->format) {
		case ISP_STORE_YUV422_3FRAME:
			ch0_offset = start_row_out * store_pitch.chn0 +
				start_col_out;
			ch1_offset = start_row_out * store_pitch.chn1 +
				((start_col_out + 1) >> 1);
			ch2_offset = start_row_out * store_pitch.chn2 +
				((start_col_out + 1) >> 1);
			break;
		case ISP_STORE_UYVY:
			ch0_offset = start_row_out * store_pitch.chn0 +
				start_col_out * 2;
			break;
		case ISP_STORE_YUV422_2FRAME:
		case ISP_STORE_YVU422_2FRAME:
			ch0_offset = start_row_out * store_pitch.chn0 +
				start_col_out;
			ch1_offset = start_row_out * store_pitch.chn1 +
				start_col_out;
			break;
		case ISP_STORE_YUV420_2FRAME:
		case ISP_STORE_YVU420_2FRAME:
			ch0_offset = start_row_out * store_pitch.chn0 +
				start_col_out;
			ch1_offset = ((start_row_out *
				store_pitch.chn1 + 1) >> 1)
				+ start_col_out;
			break;
		case ISP_STORE_YUV420_3FRAME:
			ch0_offset = start_row_out * store_pitch.chn0 +
				start_col_out;
			ch1_offset = ((start_row_out *
				store_pitch.chn1 + 1) >> 1) +
				((start_col_out + 1) >> 1);
			ch2_offset = ((start_row_out *
				store_pitch.chn2 + 1) >> 1) +
				((start_col_out + 1) >> 1);
			break;
		default:
			break;
		}

		store_info->addr.chn0 += ch0_offset;
		store_info->addr.chn1 += ch1_offset;
		store_info->addr.chn2 += ch2_offset;
	}

exit:
	return rtn;
}

static int sprd_ispslice_3dnr_crop_info_set(struct slice_context_info *cxt)
{
	int rtn = 0;
	struct slice_base_info *base_info = NULL;
	struct slice_3dnr_mem_ctrl *nr3_mem_ctrl = NULL;
	struct slice_3dnr_store_info *store_info = NULL;
	struct slice_3dnr_crop_info *crop_info = NULL;
	uint32_t cur_slice_id = 0;
	uint32_t overlap_left = 0, overlap_up = 0,
		overlap_right = 0, overlap_down = 0;

	if (!cxt) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	base_info = &cxt->base_info;
	cur_slice_id = base_info->cur_slice_id;

	for (; cur_slice_id < base_info->slice_num; cur_slice_id++) {
		nr3_mem_ctrl = &cxt->nr3_mem_ctrl[cur_slice_id];
		store_info = &cxt->store_3dnr_info[cur_slice_id];
		crop_info = &cxt->crop_3dnr_info[cur_slice_id];
		overlap_left = base_info->slice_raw_overlap_array
			[cur_slice_id].overlap_left;
		overlap_up = base_info->slice_raw_overlap_array
			[cur_slice_id].overlap_up;
		overlap_right = base_info->slice_raw_overlap_array
			[cur_slice_id].overlap_right;
		overlap_down = base_info->slice_raw_overlap_array
			[cur_slice_id].overlap_down;

		if (!overlap_left && !overlap_up &&
			!overlap_right && !overlap_down)
			crop_info->bypass = 1;
		else
			crop_info->bypass = 0;

		crop_info->src.width = nr3_mem_ctrl->src.width;
		crop_info->src.height = nr3_mem_ctrl->src.height;
		crop_info->dst.width = store_info->size.width;
		crop_info->dst.height = store_info->size.height;
		crop_info->start_x = overlap_left;
		crop_info->start_y = overlap_up;
	}

exit:
	return rtn;
}

static int sprd_ispslice_3dnr_memctrl_update(struct slice_param_in *in_ptr,
	struct slice_context_info *cxt)
{
	int rtn = 0;
	struct slice_base_info *base_info = NULL;
	struct slice_3dnr_mem_ctrl *nr3_mem_ctrl = NULL;
	struct nr3_slice nr3_fw_in;
	struct nr3_slice_for_blending nr3_fw_out;
	uint32_t cur_slice_id = 0, slice_num = 0;
	uint32_t end_col = 0, end_row = 0;

	if (!in_ptr || !cxt) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	memset((void *)&nr3_fw_in, 0, sizeof(nr3_fw_in));
	memset((void *)&nr3_fw_out, 0, sizeof(nr3_fw_out));
	base_info = &cxt->base_info;
	cur_slice_id = base_info->cur_slice_id;
	slice_num = base_info->slice_num;

	nr3_fw_in.cur_frame_width = in_ptr->img_size.width;
	nr3_fw_in.cur_frame_height = in_ptr->img_size.height;
	nr3_fw_in.mv_x = in_ptr->nr3_info.mv_x;
	nr3_fw_in.mv_y = in_ptr->nr3_info.mv_y;
	nr3_fw_in.slice_num = slice_num;

	for (; cur_slice_id < slice_num; cur_slice_id++) {
		nr3_mem_ctrl = &cxt->nr3_mem_ctrl[cur_slice_id];
		end_col = base_info->slice_raw_pos_array[cur_slice_id].end_col;
		end_row = base_info->slice_raw_pos_array[cur_slice_id].end_row;

		nr3_fw_in.end_col = end_col;
		nr3_fw_in.end_row = end_row;
		nr3_fw_out.first_line_mode = nr3_mem_ctrl->first_line_mode;
		nr3_fw_out.last_line_mode = nr3_mem_ctrl->last_line_mode;
		nr3_fw_out.src_lum_addr = nr3_mem_ctrl->addr.chn0;
		nr3_fw_out.src_chr_addr = nr3_mem_ctrl->addr.chn1;
		nr3_fw_out.ft_y_width = nr3_mem_ctrl->ft_y.width;
		nr3_fw_out.ft_y_height = nr3_mem_ctrl->ft_y.height;
		nr3_fw_out.ft_uv_width = nr3_mem_ctrl->ft_uv.width;
		nr3_fw_out.ft_uv_height = nr3_mem_ctrl->ft_uv.height;
		nr3_fw_out.start_col = nr3_mem_ctrl->start_col;
		nr3_fw_out.start_row = nr3_mem_ctrl->start_row;

		rtn = sprd_isp_3dnr_blending_calc(&nr3_fw_in, &nr3_fw_out);
		if (rtn) {
			pr_err("fail to set 3dnr calculate for blending\n");
			goto exit;
		}

		nr3_mem_ctrl->first_line_mode = nr3_fw_out.first_line_mode;
		nr3_mem_ctrl->last_line_mode = nr3_fw_out.last_line_mode;
		nr3_mem_ctrl->addr.chn0 = nr3_fw_out.src_lum_addr;
		nr3_mem_ctrl->addr.chn1 = nr3_fw_out.src_chr_addr;
		nr3_mem_ctrl->ft_y.width = nr3_fw_out.ft_y_width;
		nr3_mem_ctrl->ft_y.height = nr3_fw_out.ft_y_height;
		nr3_mem_ctrl->ft_uv.width = nr3_fw_out.ft_uv_width;
		nr3_mem_ctrl->ft_uv.height = nr3_fw_out.ft_uv_height;
	}
exit:
	return rtn;
}

static int sprd_ispslice_3dnr_info_set(struct slice_param_in *in_ptr,
	struct slice_context_info *cxt)
{
	int rtn = 0;
	struct slice_3dnr_info *slice_in = NULL;

	if (!in_ptr || !cxt) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	slice_in = &in_ptr->nr3_info;
	rtn = sprd_ispslice_3dnr_memctrl_set(
		&slice_in->fetch_3dnr_frame, cxt);
	if (rtn) {
		pr_err("fail to set slice 3dnr mem ctrl!\n");
		goto exit;
	}

	rtn = sprd_ispslice_3dnr_memctrl_update(in_ptr, cxt);
	if (rtn) {
		pr_err("fail to update slice 3dnr mem ctrl!\n");
		goto exit;
	}

	rtn = sprd_ispslice_3dnr_store_info_set(
		&slice_in->store_3dnr_frame, cxt);
	if (rtn) {
		pr_err("fail to set slice 3dnr store info!\n");
		goto exit;
	}

	rtn = sprd_ispslice_3dnr_crop_info_set(cxt);
	if (rtn) {
		pr_err("fail to set slice 3dnr crop info!\n");
		goto exit;
	}

exit:
	return rtn;
}

static int sprd_ispslice_fmcu_push_back(uint32_t *p, uint32_t addr,
	uint32_t cmd, uint32_t num)
{
	p[0] = cmd;
	p[1] = addr;

	num += 2;

	return num;
}

static int sprd_ispslice_fmcu_cfg_set(uint32_t *fmcu_buf, uint32_t num,
			enum isp_scene_id scene_id, enum isp_id idx)
{
	uint32_t addr = 0, cmd = 0;
	uint32_t cfg_start_addr[ISP_ID_MAX][ISP_SCENE_MAX] = {
		{
			ISP_CFG_PRE0_START,
			ISP_CFG_CAP0_START
		},
		{
			ISP_CFG_PRE1_START,
			ISP_CFG_CAP1_START
		}
	};

	addr = ISP_GET_REG(idx, cfg_start_addr[idx][scene_id]);
	cmd = 1;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	/*
	 * When setting CFG_TRIGGER_PULSE cmd, fmcu will wait
	 * until CFG module configs isp registers done.
	 */
	addr = ISP_GET_REG(idx, ISP_FMCU_CMD);
	cmd = CFG_TRIGGER_PULSE;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int sprd_ispslice_fmcu_fetch_set(uint32_t *fmcu_buf,
	struct slice_fetch_info *fetch_info,
	uint32_t num, enum isp_id idx)
{
	uint32_t addr = 0, cmd = 0;

	if (!fmcu_buf || !fetch_info) {
		pr_err("fail to get valid input ptr\n");
		return 0;
	}

	addr = ISP_GET_REG(idx, ISP_FETCH_MEM_SLICE_SIZE);
	cmd = ((fetch_info->size.height & 0xFFFF) << 16) |
		(fetch_info->size.width & 0xFFFF);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_FETCH_SLICE_Y_ADDR);
	cmd = fetch_info->addr.chn0;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_FETCH_SLICE_U_ADDR);
	cmd = fetch_info->addr.chn1;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_FETCH_SLICE_V_ADDR);
	cmd = fetch_info->addr.chn2;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_FETCH_MIPI_PARAM);
	cmd = fetch_info->mipi_word_num | (fetch_info->mipi_byte_rel_pos << 16);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int sprd_ispslice_fmcu_dispatch_set(uint32_t *fmcu_buf,
	struct slice_dispatch_info *dispatch_info,
	uint32_t num, enum isp_id idx)
{
	uint32_t addr = 0, cmd = 0;

	if (!fmcu_buf || !dispatch_info) {
		pr_err("fail to get valid input ptr\n");
		return 0;
	}

	addr = ISP_GET_REG(idx, ISP_DISPATCH_CH0_SIZE);
	cmd = ((dispatch_info->size.height & 0xFFFF) << 16) |
		(dispatch_info->size.width & 0xFFFF);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int sprd_ispslice_fmcu_nlm_set(uint32_t *fmcu_buf,
	struct slice_nlm_info *nlm_info,
	uint32_t num, enum isp_id id)
{
	uint32_t addr = 0, cmd = 0;

	addr = ISP_GET_REG(id, ISP_NLM_RADIAL_1D_DIST);
	cmd = (nlm_info->col_center & 0x3FFF) |
			((nlm_info->row_center & 0x3FFF) << 16);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int sprd_ispslice_fmcu_ynr_set(uint32_t *fmcu_buf,
	struct slice_ynr_info *ynr_info, uint32_t num, enum isp_id idx)
{
	uint32_t addr = 0, cmd = 0;

	if (!fmcu_buf || !ynr_info) {
		pr_err("fail to get valid input ptr\n");
		return 0;
	}

	addr = ISP_GET_REG(idx, ISP_YNR_CFG11);
	cmd = ((ynr_info->start_col & 0xFFFF) << 16) |
		(ynr_info->start_row & 0xFFFF);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(idx, ISP_YNR_CFG12);
	cmd = ((ynr_info->col_center & 0xFFFF) << 16) |
		(ynr_info->row_center & 0xFFFF);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

#if 0
static int sprd_ispslice_fmcu_postcnr_set(uint32_t *fmcu_buf,
	struct slice_postcnr_info *postcnr_info,
	uint32_t num, enum isp_id idx)
{
	uint32_t addr = 0, cmd = 0;

	if (!fmcu_buf || !postcnr_info) {
		pr_err("fail to get valid input ptr\n");
		return 0;
	}

	addr = ISP_GET_REG(idx, ISP_POSTCDN_START_ROW_MOD4);
	cmd = postcnr_info->start_row_mod4 & 0x3;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int sprd_ispslice_fmcu_noisefilter_set(uint32_t *fmcu_buf,
	struct slice_noisefilter_info *noisefilter_info,
	uint32_t num, enum isp_id idx)
{
	uint32_t addr = 0, cmd = 0;

	if (!fmcu_buf || !noisefilter_info) {
		pr_err("fail to get valid input ptr\n");
		return 0;
	}

	addr = ISP_GET_REG(idx, ISP_YUV_NF_SEED0);
	cmd = noisefilter_info->seed0 & 0xFFFFFF;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_YUV_NF_SEED1);
	cmd = noisefilter_info->seed1 & 0xFFFFFF;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_YUV_NF_SEED2);
	cmd = noisefilter_info->seed2 & 0xFFFFFF;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_YUV_NF_SEED3);
	cmd = noisefilter_info->seed3 & 0xFFFFFF;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_YUV_NF_SEED_INIT);
	cmd = noisefilter_info->seed_int;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int sprd_ispslice_fmcu_cfa_set(uint32_t *fmcu_buf,
	struct slice_cfa_info *cfa_info,
	uint32_t num, enum isp_id id)
{
	uint32_t addr = 0, cmd = 0;

	addr = ISP_GET_REG(id, ISP_CFAE_GBUF_CFG);
	cmd = cfa_info->gbuf_addr_max & 0xfff;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}
#endif

static int sprd_ispslice_fmcu_scaler_set(uint32_t *fmcu_buf,
	struct slice_scaler_info *scaler_info,
	uint32_t num, enum isp_id idx, uint32_t base)
{
	uint32_t addr = 0, cmd = 0;

	if (!fmcu_buf || !scaler_info) {
		pr_err("fail to get valid input ptr\n");
		return 0;
	}

	if (scaler_info->trim1_size_x == 0 || scaler_info->trim1_size_y == 0) {
		addr = ISP_GET_REG(idx, ISP_SCALER_CFG) + base;
		cmd = ISP_REG_RD(idx, ISP_SCALER_CFG + base) | BIT_9;
		num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num],
			addr, cmd, num);
		addr = ISP_GET_REG(idx, ISP_STORE_PRE_CAP_BASE
			+ ISP_STORE_PARAM);
		cmd = ISP_REG_RD(idx, ISP_STORE_PRE_CAP_BASE
			+ ISP_STORE_PARAM) | BIT_0;
		num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num],
			addr, cmd, num);
	}
	addr = ISP_GET_REG(idx, ISP_SCALER_SRC_SIZE) + base;
	cmd = (scaler_info->src_size_x & 0x3FFF) |
		((scaler_info->src_size_y & 0x3FFF) << 16);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_SCALER_DES_SIZE) + base;
	cmd = (scaler_info->dst_size_x & 0x3FFF) |
		((scaler_info->dst_size_y & 0x3FFF) << 16);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_SCALER_TRIM0_START) + base;
	cmd = (scaler_info->trim0_start_x & 0x1FFF) |
		((scaler_info->trim0_start_y & 0x1FFF) << 16);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_SCALER_TRIM0_SIZE) + base;
	cmd = (scaler_info->trim0_size_x & 0x1FFF) |
		((scaler_info->trim0_size_y & 0x1FFF) << 16);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_SCALER_IP) + base;
	cmd = (scaler_info->scaler_ip_rmd & 0x1FFF) |
		((scaler_info->scaler_ip_int & 0xF) << 16);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_SCALER_CIP) + base;
	cmd = (scaler_info->scaler_cip_rmd & 0x1FFF) |
		((scaler_info->scaler_cip_int & 0xF) << 16);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_SCALER_TRIM1_START) + base;
	cmd = (scaler_info->trim1_start_x & 0x1FFF) |
		((scaler_info->trim1_start_y & 0x1FFF) << 16);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_SCALER_TRIM1_SIZE) + base;
	cmd = (scaler_info->trim1_size_x & 0x1FFF) |
		((scaler_info->trim1_size_y & 0x1FFF) << 16);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_SCALER_VER_IP) + base;
	cmd = (scaler_info->scaler_ip_rmd_ver & 0x1FFF) |
		((scaler_info->scaler_ip_int_ver & 0xF) << 16);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_SCALER_VER_CIP) + base;
	cmd = (scaler_info->scaler_cip_rmd_ver & 0x1FFF) |
		((scaler_info->scaler_cip_int_ver & 0xF) << 16);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int sprd_ispslice_fmcu_store_set(uint32_t *fmcu_buf,
	struct slice_store_info *store_info,
	uint32_t num, enum isp_id idx, uint32_t base)
{
	uint32_t addr = 0, cmd = 0;

	if (!fmcu_buf || !store_info) {
		pr_err("fail to get valid input ptr\n");
		return 0;
	}

	addr = ISP_GET_REG(idx, ISP_STORE_SLICE_SIZE) + base;
	cmd = ((store_info->size.height & 0xFFFF) << 16) |
		(store_info->size.width & 0xFFFF);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_STORE_BORDER) + base;
	cmd = (store_info->border.up_border & 0xFF) |
		((store_info->border.down_border & 0xFF) << 8)
		| ((store_info->border.left_border & 0xFF) << 16) |
		((store_info->border.right_border & 0xFF) << 24);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_STORE_SLICE_Y_ADDR) + base;
	cmd = store_info->addr.chn0;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_STORE_SLICE_U_ADDR) + base;
	cmd = store_info->addr.chn1;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_STORE_SLICE_V_ADDR) + base;
	cmd = store_info->addr.chn2;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_STORE_SHADOW_CLR) + base;
	cmd = 1;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int sprd_ispslice_fmcu_store_fbc_set(uint32_t *fmcu_buf,
	struct slice_store_fbc_info *store_fbc_info,
	uint32_t num, enum isp_id idx, uint32_t base)
{
	uint32_t addr = 0, cmd = 0;

	if (!fmcu_buf || !store_fbc_info) {
		pr_err("fail to get valid input ptr\n");
		return 0;
	}

	addr = ISP_GET_REG(idx, ISP_FBC_STORE_SLICE_SIZE) + base;
	cmd = (store_fbc_info->slice_out_width & 0x1FFF) |
		((store_fbc_info->slice_out_height & 0x1FFF) << 16);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(idx, ISP_FBC_STORE_BORDER) + base;
	cmd = (store_fbc_info->overlap_up & 0xFF) |
		((store_fbc_info->overlap_down & 0xFF) << 8)|
		((store_fbc_info->overlap_left & 0xFF) << 16) |
		((store_fbc_info->overlap_right & 0xFF) << 24);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	cmd = store_fbc_info->slice_offset;
	addr = ISP_GET_REG(idx, ISP_FBC_STORE_SLICE_HEADER_OFFSET_ADDR) + base;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	cmd = store_fbc_info->yheader_addr;
	addr = ISP_GET_REG(idx, ISP_FBC_STORE_SLICE_Y_HEADER) + base;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	cmd = store_fbc_info->yaddr;
	addr = ISP_GET_REG(idx, ISP_FBC_STORE_SLICE_Y_ADDR) + base;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int sprd_ispslice_fmcu_3dnr_memctrl_set(uint32_t *fmcu_buf,
	struct slice_3dnr_mem_ctrl *mem_ctrl,
	uint32_t num, enum isp_id idx)
{
	uint32_t addr = 0, cmd = 0;

	if (!fmcu_buf || !mem_ctrl) {
		pr_err("fail to get valid input ptr\n");
		return 0;
	}

	addr = ISP_GET_REG(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM1);
	cmd = ((mem_ctrl->start_col & 0x1FFF) << 16) |
		(mem_ctrl->start_row & 0x1FFF);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM3);
	cmd = ((mem_ctrl->src.height & 0xFFF) << 16) |
		(mem_ctrl->src.width & 0xFFF);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM4);
	cmd = ((mem_ctrl->ft_y.height & 0xFFF) << 16) |
		(mem_ctrl->ft_y.width & 0xFFF);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM5);
	cmd = ((mem_ctrl->ft_uv.height & 0xFFF) << 16) |
		(mem_ctrl->ft_uv.width & 0xFFF);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_YUV_3DNR_MEM_CTRL_FT_CUR_LUMA_ADDR);
	cmd = mem_ctrl->addr.chn0;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_YUV_3DNR_MEM_CTRL_FT_CUR_CHROMA_ADDR);
	cmd = mem_ctrl->addr.chn1;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_YUV_3DNR_MEM_CTRL_LINE_MODE);
	cmd = (mem_ctrl->first_line_mode & 0x1) |
		((mem_ctrl->last_line_mode & 0x1) << 1);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int sprd_ispslice_fmcu_3dnr_store_set(uint32_t *fmcu_buf,
	struct slice_3dnr_store_info *store_info,
	uint32_t num, enum isp_id idx)
{
	uint32_t addr = 0, cmd = 0;

	if (!fmcu_buf || !store_info) {
		pr_err("fail to get valid input ptr\n");
		return 0;
	}

	addr = ISP_GET_REG(idx, ISP_STORE_LITE_SIZE);
	cmd = ((store_info->size.height & 0xFFFF) << 16) |
		(store_info->size.width & 0xFFFF);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_STORE_LITE_ADDR0);
	cmd = store_info->addr.chn0;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_STORE_LITE_ADDR1);
	cmd = store_info->addr.chn1;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int sprd_ispslice_fmcu_3dnr_crop_set(uint32_t *fmcu_buf,
	struct slice_3dnr_crop_info *crop_info,
	uint32_t num, enum isp_id idx)
{
	uint32_t addr = 0, cmd = 0;

	if (!fmcu_buf || !crop_info) {
		pr_err("fail to get valid input ptr\n");
		return 0;
	}

	addr = ISP_GET_REG(idx, ISP_YUV_3DNR_MEM_CTRL_PRE_PARAM0);
	cmd = crop_info->bypass & 0x1;
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_YUV_3DNR_MEM_CTRL_PRE_PARAM1);
	cmd = ((crop_info->src.height & 0xFFFF) << 16)
		| (crop_info->src.width & 0xFFFF);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_YUV_3DNR_MEM_CTRL_PRE_PARAM2);
	cmd = ((crop_info->dst.height & 0xFFFF) << 16)
		| (crop_info->dst.width & 0xFFFF);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	addr = ISP_GET_REG(idx, ISP_YUV_3DNR_MEM_CTRL_PRE_PARAM3);
	cmd = ((crop_info->start_x & 0xFFFF) << 16) |
		(crop_info->start_y & 0xFFFF);
	num = sprd_ispslice_fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int sprd_ispslice_fmcu_info_set(struct slice_param_in *in_ptr,
	struct slice_context_info *cxt, uint32_t *fmcu_num)
{
	int rtn = 0;
	struct slice_fetch_info *fetch_info = NULL;
	struct slice_dispatch_info *dispatch_info = NULL;
	struct slice_nlm_info *nlm_info = NULL;
	struct slice_cfa_info *cfa_info = NULL;
	struct slice_postcnr_info *postcnr_info = NULL;
	struct slice_ynr_info *ynr_info = NULL;
	struct slice_noisefilter_info *noisefilter_info = NULL;
	struct slice_base_info *base_info = NULL;
	struct slice_scaler_info *scaler_info = NULL;
	struct slice_store_info *store_info = NULL;
	struct slice_store_fbc_info *store_fbc_info = NULL;
	struct slice_3dnr_mem_ctrl *nr3_mem_ctrl = NULL;
	struct slice_3dnr_store_info *store_3dnr_info = NULL;
	struct slice_3dnr_crop_info *crop_3dnr_info = NULL;
	uint32_t num = 0, addr = 0, cmd = 0;
	uint32_t slice_id = 0;
	uint32_t *fmcu_buf = NULL;
	enum isp_id id = 0;
	enum isp_scene_id scene_id = 0;
	enum isp_work_mode work_mode = ISP_CFG_MODE;
	uint32_t scl_base = 0, store_base = 0, store_fbc_base = 0;

	uint32_t shadow_done_cmd[ISP_ID_MAX][ISP_SCENE_MAX] = {
		{PRE0_SHADOW_DONE, CAP0_SHADOW_DONE},
		{PRE1_SHADOW_DONE, CAP1_SHADOW_DONE}
	};
	uint32_t all_done_cmd[ISP_ID_MAX][ISP_SCENE_MAX] = {
		{PRE0_ALL_DONE, CAP0_ALL_DONE},
		{PRE1_ALL_DONE, CAP1_ALL_DONE}
	};

	if (!in_ptr || !cxt) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	base_info = &cxt->base_info;
	fmcu_buf = in_ptr->fmcu_addr_vir;
	id = ISP_GET_ISP_ID(in_ptr->com_idx);
	scene_id = ISP_GET_SCENE_ID(in_ptr->com_idx);
	work_mode = ISP_GET_MODE_ID(in_ptr->com_idx);

	for (slice_id = 0; slice_id < base_info->slice_num; slice_id++) {
		fetch_info = &cxt->fetch_info[slice_id];
		dispatch_info = &cxt->dispatch_info[slice_id];
		nlm_info = &cxt->nlm_info[slice_id];
		cfa_info = &cxt->cfa_info[slice_id];
		postcnr_info = &cxt->postcnr_info[slice_id];
		ynr_info = &cxt->ynr_info[slice_id];
		noisefilter_info = &cxt->noisefilter_info[slice_id];
		nr3_mem_ctrl = &cxt->nr3_mem_ctrl[slice_id];
		store_3dnr_info = &cxt->store_3dnr_info[slice_id];
		crop_3dnr_info = &cxt->crop_3dnr_info[slice_id];

		if (work_mode == ISP_CFG_MODE)
			num = sprd_ispslice_fmcu_cfg_set(fmcu_buf,
				num, scene_id, id);
		num = sprd_ispslice_fmcu_fetch_set(fmcu_buf,
			fetch_info, num, id);
		num = sprd_ispslice_fmcu_dispatch_set(fmcu_buf,
			dispatch_info, num, id);
#if 0
		num = sprd_ispslice_fmcu_postcnr_set(fmcu_buf,
			postcnr_info, num, id);
		num = sprd_ispslice_fmcu_noisefilter_set(fmcu_buf,
			noisefilter_info, num, id);
		num = sprd_ispslice_fmcu_cfa_set(fmcu_buf, cfa_info, num, id);
#endif
		num = sprd_ispslice_fmcu_nlm_set(fmcu_buf, nlm_info, num, id);
		num = sprd_ispslice_fmcu_ynr_set(fmcu_buf, ynr_info, num, id);
		if (in_ptr->nr3_info.need_slice) {
			num = sprd_ispslice_fmcu_3dnr_memctrl_set(fmcu_buf,
				nr3_mem_ctrl, num, id);
			num = sprd_ispslice_fmcu_3dnr_store_set(fmcu_buf,
				store_3dnr_info, num, id);
			num = sprd_ispslice_fmcu_3dnr_crop_set(fmcu_buf,
				crop_3dnr_info, num, id);
			if (in_ptr->nr3_info.cur_frame_num == 1) {
				addr = ISP_GET_REG(id,
					ISP_YUV_3DNR_MEM_CTRL_PARAM0);
				cmd = ISP_REG_CFG_RD(in_ptr->com_idx,
					ISP_YUV_3DNR_MEM_CTRL_PARAM0);
				cmd &= (~BIT_23);
				cmd |= (BIT_6 | BIT_12);
				num = sprd_ispslice_fmcu_push_back(
					&fmcu_buf[num], addr, cmd, num);
				addr = ISP_GET_REG(id,
					ISP_YUV_3DNR_MEM_CTRL_PARAM7);
				cmd = 0;
				num = sprd_ispslice_fmcu_push_back(
					&fmcu_buf[num], addr, cmd, num);
			} else {
				addr = ISP_GET_REG(id,
					ISP_YUV_3DNR_MEM_CTRL_PARAM0);
				cmd = ISP_REG_CFG_RD(in_ptr->com_idx,
					ISP_YUV_3DNR_MEM_CTRL_PARAM0);
				cmd |= (BIT_6 | BIT_12 | BIT_23);
				num = sprd_ispslice_fmcu_push_back(
					&fmcu_buf[num], addr, cmd, num);
			}
		}

		if (in_ptr->cap_slice_need == 1) {
			scaler_info = cxt->scaler_info[SLICE_PATH_CAP];
			store_info = cxt->store_info[SLICE_PATH_CAP];
			store_fbc_info = cxt->store_fbc_info[SLICE_PATH_CAP];
			scl_base = ISP_SCALER_PRE_CAP_BASE;
			store_base = ISP_STORE_PRE_CAP_BASE;
			store_fbc_base = ISP_CAP_FBC_STORE_BASE;
			num = sprd_ispslice_fmcu_scaler_set(fmcu_buf,
				&scaler_info[slice_id], num, id, scl_base);
			num = sprd_ispslice_fmcu_store_set(fmcu_buf,
				&store_info[slice_id], num, id, store_base);
			num = sprd_ispslice_fmcu_store_fbc_set(fmcu_buf,
				&store_fbc_info[slice_id], num, id,
					store_fbc_base);
		}

		if (work_mode == ISP_CFG_MODE)
			addr = ISP_GET_REG(id, ISP_CFG_CAP_FMCU_RDY);
		else
			addr = ISP_GET_REG(id, ISP_FETCH_START);
		cmd = 1;
		num = sprd_ispslice_fmcu_push_back(
			&fmcu_buf[num], addr, cmd, num);
		addr = ISP_GET_REG(id, ISP_FMCU_CMD);
		if (work_mode == ISP_CFG_MODE)
			cmd = shadow_done_cmd[id][scene_id];
		else
			cmd = shadow_done_cmd[0][ISP_SCENE_PRE];
		num = sprd_ispslice_fmcu_push_back(
			&fmcu_buf[num], addr, cmd, num);
		addr = ISP_GET_REG(id, ISP_FMCU_CMD);
		if (work_mode == ISP_CFG_MODE)
			cmd = all_done_cmd[id][scene_id];
		else
			cmd = all_done_cmd[0][ISP_SCENE_PRE];
		num = sprd_ispslice_fmcu_push_back(
			&fmcu_buf[num], addr, cmd, num);
	}
	*fmcu_num = num / 2;
exit:
	return rtn;
}

int sprd_isp_slice_fmcu_slice_cfg(void *fmcu_handler,
	struct slice_param_in *in_ptr, uint32_t *fmcu_num)
{
	int rtn = 0;
	struct slice_context_info *cxt = NULL;

	if (!fmcu_handler || !in_ptr || !fmcu_num) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}
	cxt = (struct slice_context_info *)fmcu_handler;

	rtn = sprd_ispslice_base_info_set(in_ptr, &cxt->base_info);
	if (rtn) {
		pr_err("fail to set slice base info!\n");
		goto exit;
	}

	rtn = sprd_ispslice_fetch_info_set(in_ptr, cxt);
	if (rtn) {
		pr_err("fail to set slice fetchYUV info!\n");
		goto exit;
	}

	rtn = sprd_ispslice_dispatch_info_set(cxt);
	if (rtn) {
		pr_err("fail to set slice dispatchYUV info!\n");
		goto exit;
	}

	rtn = sprd_ispslice_nlm_info_set(cxt, in_ptr);
	if (rtn) {
		pr_err("fail to set slice dispatchYUV info!\n");
		goto exit;
	}

	rtn = sprd_ispslice_cfa_info_set(in_ptr, cxt);
	if (rtn) {
		pr_err("fail to set slice cfa` info!\n");
		goto exit;
	}

	rtn = sprd_ispslice_postcnr_info_set(cxt);
	if (rtn) {
		pr_err("fail to set slice postcnr info!\n");
		goto exit;
	}

	rtn = sprd_ispslice_ynr_info_set(cxt, in_ptr);
	if (rtn) {
		pr_err("fail to set slice ynr info!\n");
		goto exit;
	}

	rtn = sprd_ispslice_scaler_info_set(in_ptr, cxt);
	if (rtn) {
		pr_err("fail to set slice scaler info!\n");
		goto exit;
	}

	rtn = sprd_ispslice_noisefliter_info_set(cxt);
	if (rtn) {
		pr_err("fail to set slice noise fliter info!\n");
		goto exit;
	}

	rtn = sprd_ispslice_slice_store_info_set(in_ptr, cxt);
	if (rtn) {
		pr_err("fail to set slice store info!\n");
		goto exit;
	}

	rtn = sprd_ispslice_slice_store_fbc_info_set(in_ptr, cxt);
	if (rtn) {
		pr_err("fail to set slice fbc store info!\n");
		goto exit;
	}

	if (in_ptr->nr3_info.need_slice) {
		rtn = sprd_ispslice_3dnr_info_set(in_ptr, cxt);
		if (rtn) {
			pr_err("fail to set slice 3dnr info!\n");
			goto exit;
		}
	}

	rtn = sprd_ispslice_fmcu_info_set(in_ptr, cxt, fmcu_num);
	if (rtn) {
		pr_err("fail to set slice fmcu info!\n");
		goto exit;
	}

exit:
	return rtn;
}

int sprd_isp_slice_fmcu_init(void **fmcu_handler)
{
	int rtn = 0;
	struct slice_context_info *cxt = NULL;

	if (!fmcu_handler) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}
	*fmcu_handler = NULL;

	cxt = vzalloc(sizeof(struct slice_context_info));
	if (cxt == NULL) {
		rtn = -ENOMEM;
		goto vzalloc_exit;
	}

	*fmcu_handler = (void *)cxt;
exit:
	return rtn;
vzalloc_exit:
	pr_err("fail to alloc memory for fmcu slice\n");
	return rtn;
}

int sprd_isp_slice_fmcu_deinit(void *fmcu_handler)
{
	int rtn = 0;
	struct slice_context_info *cxt = NULL;

	if (!fmcu_handler) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	cxt = (struct slice_context_info *)fmcu_handler;

	if (cxt != NULL)
		vfree(cxt);

	return rtn;
}
