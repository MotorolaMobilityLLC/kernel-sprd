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

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

#include "sprd_mm.h"
#include "isp_slice.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_SLICE: %d: %d %s: "\
	fmt, current->pid, __LINE__, __func__

#define ISP_SLICE_ALIGN_SIZE		2
#define ISP_ALIGNED(size)	((size) & ~(ISP_SLICE_ALIGN_SIZE - 1))

#define SLICE_THRESHOLD			2592
#define SLICE_HEIGHT_NUM			1
#define SLICE_WIDTH_MAX			2112	/* 1408 */
#define SLICE_WIDTH_DEF			2112
#define SLICE_SCL_WIDTH_MAX			2304

#define YUV_OVERLAP_UP				46
#define YUV_OVERLAP_DOWN			66
#define YUV_OVERLAP_LEFT			90
#define YUV_OVERLAP_RIGHT			126

/*
 * Change YUVSCALER_OVERLAP_LEFT from 16 to 32 to support
 * (3264,2488) -> (352, 288) scaler.
 * And the YUV_OVERLAP_LEFT and RAW_OVERLAP_LEFT also needs
 * to be added 16 as the YUVSCALER_OVERLAP_LEFT changes.
 *
 * Note: The sum of RAW_OVERLAP_LEFT and RAW_OVERLAP_RIGHT should
 * be less than 256, due to the 8-bit limit in overlap.
 */
#define YUVSCALER_OVERLAP_UP			32
#define YUVSCALER_OVERLAP_DOWN			52
#define YUVSCALER_OVERLAP_LEFT			32
#define YUVSCALER_OVERLAP_RIGHT		68

#define RAW_OVERLAP_UP				58
#define RAW_OVERLAP_DOWN			78
#define RAW_OVERLAP_LEFT			102
#define RAW_OVERLAP_RIGHT			138

#define SCL_HOR_MAX_TAP			8
#define SCL_VER_MAX_TAP			16
#define SCL_OVERLAP_UP				(SCL_VER_MAX_TAP/2)
#define SCL_OVERLAP_DOWN			(SCL_VER_MAX_TAP/2+1)
#define SCL_OVERLAP_LEFT			(SCL_HOR_MAX_TAP/2)
#define SCL_OVERLAP_RIGHT			(SCL_HOR_MAX_TAP/2+1)

/* use to cal lsc Q value */
#define CLIP(input, top, bottom)		\
do {						\
	if (input > top)			\
		input = top;			\
	if (input < bottom)			\
		input = bottom;			\
} while (0)

#define TABLE_LEN_128	128
#define TABLE_LEN_96	96

#define MAX_TABLE_SIZE	129
struct lnc_bicubic_weight_t_64_tag {
	int16_t w0;
	int16_t w1;
	int16_t w2;
};

static struct lnc_bicubic_weight_t_64_tag lnc_bicubic_weight_t_96_simple[] = {
	{0, 1024, 0},
	{-5, 1024, 6},
	{-10, 1023, 12},
	{-15, 1022, 18},
	{-20, 1020, 25},
	{-24, 1017, 32},
	{-28, 1014, 40},
	{-32, 1011, 48},
	{-36, 1007, 56},
	{-39, 1003, 65},
	{-43, 998, 74},
	{-46, 993, 83},
	{-49, 987, 93},
	{-52, 981, 103},
	{-54, 974, 113},
	{-57, 967, 124},
	{-59, 960, 135},
	{-61, 952, 146},
	{-63, 944, 158},
	{-65, 936, 170},
	{-67, 927, 182},
	{-68, 918, 194},
	{-70, 908, 206},
	{-71, 898, 219},
	{-72, 888, 232},
	{-73, 878, 245},
	{-74, 867, 258},
	{-74, 856, 272},
	{-75, 844, 285},
	{-75, 833, 299},
	{-76, 821, 313},
	{-76, 809, 327},
	{-76, 796, 341},
	{-76, 784, 356},
	{-76, 771, 370},
	{-75, 758, 384},
	{-75, 745, 399},
	{-75, 732, 414},
	{-74, 718, 428},
	{-73, 704, 443},
	{-73, 691, 458},
	{-72, 677, 473},
	{-71, 663, 487},
	{-70, 648, 502},
	{-69, 634, 517},
	{-68, 620, 532},
	{-67, 605, 547},
	{-65, 591, 561},
	{-64, 576, 576},
};

static struct lnc_bicubic_weight_t_64_tag lnc_bicubic_weight_t_128_simple[] = {
	{0, 1024, 0},
	{-4, 1024, 4},
	{-8, 1023, 8},
	{-11, 1023, 13},
	{-15, 1022, 18},
	{-18, 1020, 23},
	{-22, 1019, 28},
	{-25, 1017, 34},
	{-28, 1014, 40},
	{-31, 1012, 46},
	{-34, 1009, 52},
	{-37, 1006, 58},
	{-39, 1003, 65},
	{-42, 999, 72},
	{-44, 995, 78},
	{-47, 991, 86},
	{-49, 987, 93},
	{-51, 982, 101},
	{-53, 978, 108},
	{-55, 973, 116},
	{-57, 967, 124},
	{-59, 962, 132},
	{-60, 956, 141},
	{-62, 950, 149},
	{-63, 944, 158},
	{-65, 938, 167},
	{-66, 931, 176},
	{-67, 925, 185},
	{-68, 918, 194},
	{-69, 910, 203},
	{-70, 903, 213},
	{-71, 896, 222},
	{-72, 888, 232},
	{-73, 880, 242},
	{-73, 872, 252},
	{-74, 864, 262},
	{-74, 856, 272},
	{-75, 847, 282},
	{-75, 839, 292},
	{-75, 830, 303},
	{-76, 821, 313},
	{-76, 812, 324},
	{-76, 803, 334},
	{-76, 793, 345},
	{-76, 784, 356},
	{-76, 774, 366},
	{-76, 765, 377},
	{-75, 755, 388},
	{-75, 745, 399},
	{-75, 735, 410},
	{-74, 725, 421},
	{-74, 715, 432},
	{-73, 704, 443},
	{-73, 694, 454},
	{-72, 684, 465},
	{-72, 673, 476},
	{-71, 663, 487},
	{-70, 652, 498},
	{-69, 641, 510},
	{-69, 631, 521},
	{-68, 620, 532},
	{-67, 609, 543},
	{-66, 598, 554},
	{-65, 587, 565},
	{-64, 576, 576},
};

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
	uint32_t *scaler_slice;
	uint32_t *scaler_yuv;
};

static int fmcu_push_back(uint32_t *p, uint32_t addr,
	uint32_t cmd, uint32_t num)
{
	p[0] = cmd;
	p[1] = addr;
	num += 2;

	return num;
}

static uint32_t noisefilter_24b_shift8(uint32_t seed, uint32_t *data_out)
{
	uint32_t bit_0, bit_1, bit_2, bit_3;
	uint32_t bit_in[8], bit_in8b;
	uint32_t out;
	uint32_t i = 0;

	for (i = 0; i < 8; i++) {
		bit_0 = (seed>>(0+i)) & 0x1;
		bit_1 = (seed>>(1+i)) & 0x1;
		bit_2 = (seed>>(2+i)) & 0x1;
		bit_3 = (seed>>(7+i)) & 0x1;
		bit_in[i] = bit_0^bit_1^bit_2^bit_3;
	}
	bit_in8b = (bit_in[7]<<7) | (bit_in[6]<<6) | (bit_in[5]<<5) |
		(bit_in[4]<<4) | (bit_in[3]<<3) | (bit_in[2]<<2) |
		(bit_in[1]<<1) | bit_in[0];

	out = seed & 0xffffff;
	out = out | (bit_in8b<<24);
	if (data_out)
		*data_out = out;

	out = out>>8;

	return out;
}
static void slice_noisefilter_seeds(uint32_t image_width,
				    uint32_t seed0, uint32_t *seed1,
				    uint32_t *seed2, uint32_t *seed3)
{
	uint32_t i = 0;

	*seed1 = noisefilter_24b_shift8(seed0, 0);
	*seed2 = seed0;

	for (i = 0; i < image_width; i++)
		*seed2 = noisefilter_24b_shift8(*seed2, 0);

	*seed3 = noisefilter_24b_shift8(*seed2, 0);
}

static void calc_scaler_phase(uint32_t phase, uint32_t factor,
			      uint32_t *phase_int, uint32_t *phase_rmd)
{
	phase_int[0] = (uint32_t)(phase/factor);
	phase_rmd[0] = (uint32_t)(phase-factor*phase_int[0]);
}

static int get_slice_size_info(struct slice_param_in *in_ptr,
			       uint32_t *h, uint32_t *w)
{
	int rtn = 0;
	struct slice_img_size *input = NULL;
	struct slice_img_size *output = NULL;

	if (!in_ptr || !h || !w) {
		pr_err("fail to get valid param, NULL\n");
		rtn = -1;
		goto exit;
	}

	input = &in_ptr->img_size;
	output = &in_ptr->store_frame[SLICE_PATH_CAP].size;

	/*
	 * Beware of scale-up scenario which size cross
	 * the threshold from source to destination.
	 * ISP blocks should perform slice process consistently.
	 *
	 */
	if ((input->width > SLICE_THRESHOLD)  ||
	    (output->width > SLICE_THRESHOLD)) {
		*w =  input->width / 2;
		if (*w > SLICE_WIDTH_MAX)
			*w = SLICE_WIDTH_DEF;
		pr_info("slice width %d\n", *w);
		pr_debug("in/out width %d %d\n", input->width, output->width);
	} else {
		*w = input->width;
	}
	*h = input->height / SLICE_HEIGHT_NUM;

	*w = ISP_ALIGNED(*w);
	*h = ISP_ALIGNED(*h);
exit:
	return rtn;
}

static int get_slice_overlap_info(struct slice_param_in *in_ptr,
				  struct slice_base_info *base_info)
{
	if (!in_ptr || !base_info) {
		pr_err("fail to get valid param, NULL");
		return -EINVAL;
	}

	switch (in_ptr->fetch_format) {
	case ISP_FETCH_YUV422_3FRAME:
	case ISP_FETCH_YUV422_2FRAME:
	case ISP_FETCH_YVU422_2FRAME:
	case ISP_FETCH_YUV420_2FRAME:
	case ISP_FETCH_YVU420_2FRAME:
	case ISP_FETCH_YUYV:
	case ISP_FETCH_UYVY:
	case ISP_FETCH_YVYU:
	case ISP_FETCH_VYUY:
		base_info->overlap_up = YUV_OVERLAP_UP;
		base_info->overlap_down = YUV_OVERLAP_DOWN;
		base_info->overlap_left = YUV_OVERLAP_LEFT;
		base_info->overlap_right = YUV_OVERLAP_RIGHT;
		break;
	case ISP_FETCH_CSI2_RAW_10:
	case ISP_FETCH_RAW_10:
		base_info->overlap_up = RAW_OVERLAP_UP;
		base_info->overlap_down = RAW_OVERLAP_DOWN;
		base_info->overlap_left = RAW_OVERLAP_LEFT;
		base_info->overlap_right = RAW_OVERLAP_RIGHT;
		break;
	default:
		break;
	}

	return 0;
}

static int set_slice_base_info(struct slice_param_in *in_ptr,
			       struct slice_base_info *base_info)
{
	int rtn = 0;
	uint32_t i = 0, j = 0;
	uint32_t img_height, img_width;
	uint32_t slice_height = 0, slice_width = 0;
	uint32_t slice_total_row, slice_total_col, slice_num;
	uint32_t overlap_up = 0;
	uint32_t overlap_down = 0;
	uint32_t overlap_left = 0;
	uint32_t overlap_right = 0;

	if (!in_ptr || !base_info) {
		pr_err("fail to get valid param, NULL\n");
		rtn = -1;
		goto exit;
	}

	rtn = get_slice_size_info(in_ptr, &slice_height, &slice_width);
	if (rtn)
		goto exit;

	rtn = get_slice_overlap_info(in_ptr, base_info);
	if (rtn)
		goto exit;

	pr_debug("fmcu slice height %d, width %d\n", slice_height, slice_width);
	img_height = in_ptr->img_size.height;
	img_width = in_ptr->img_size.width;
	slice_total_row = DIV_ROUND_UP(img_height, slice_height);
	slice_total_col = DIV_ROUND_UP(img_width, slice_width);
	slice_num = slice_total_col * slice_total_row;
	overlap_up = base_info->overlap_up;
	overlap_down = base_info->overlap_down;
	overlap_left = base_info->overlap_left;
	overlap_right = base_info->overlap_right;

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
	base_info->isp_jpg_cowork = 0;

	if (base_info->slice_num > SLICE_NUM_MAX) {
		pr_err("fail to valid slice num %d\n", base_info->slice_num);
		rtn = -1;
		goto exit;
	}

	pr_debug("slice row %d, col %d, num %d", slice_total_row,
		slice_total_col, slice_num);
	for (i = 0; i < slice_total_row; i++) {
		for (j = 0; j < slice_total_col; j++) {
			struct slice_pos_info temp_win = {0};
			struct slice_overlap_info temp_overlap = {0};

			temp_win.start_col = j * slice_width;
			temp_win.start_row = i * slice_height;
			if (i != 0) {
				temp_win.start_row -= overlap_up;
				temp_overlap.overlap_up = overlap_up;
			}

			if (j != 0) {
				temp_win.start_col -= overlap_left;
				temp_overlap.overlap_left = overlap_left;
			}

			if (i != slice_total_row - 1) {
				temp_win.end_row = (i + 1) * slice_height
					- 1 + overlap_down;
				temp_overlap.overlap_down = overlap_down;
			} else {
				temp_win.end_row = img_height - 1;
			}

			if (j != slice_total_col - 1) {
				temp_win.end_col = (j + 1) * slice_width
					- 1 + overlap_right;
				temp_overlap.overlap_right = overlap_right;
			} else {
				temp_win.end_col = img_width - 1;
			}

			base_info->slice_pos_array[i * slice_total_col
				+ j] = temp_win;
			base_info->slice_overlap_array[i * slice_total_col
				+ j] = temp_overlap;
		}
	}

exit:
	return rtn;
}

static uint16_t cubic_1d(uint16_t a, uint16_t b, uint16_t c, uint16_t d,
			 uint16_t u, uint16_t box,
			 struct lnc_bicubic_weight_t_64_tag *simple)
{
	int32_t out_value;
	uint32_t out_value_tmp;
	uint16_t out_value_uint16_t;
	int16_t w0, w1, w2, w3;
	int16_t sub_tmp0;
	int16_t sub_tmp1;
	int16_t sub_tmp2;

	if (simple == NULL) {
		if (box == 96) {
			if (u < (TABLE_LEN_96 / 2 + 1)) {
				w0 = lnc_bicubic_weight_t_96_simple[u].w0;
				w1 = lnc_bicubic_weight_t_96_simple[u].w1;
				w2 = lnc_bicubic_weight_t_96_simple[u].w2;

				sub_tmp0 = a - d;
				sub_tmp1 = b - d;
				sub_tmp2 = c - d;
				out_value_tmp = ((uint32_t) d) << 10;
				out_value = out_value_tmp + sub_tmp0 * w0 +
						sub_tmp1 * w1 + sub_tmp2 * w2;
			} else {
				w1 = lnc_bicubic_weight_t_96_simple[
					TABLE_LEN_96 - u].w2;
				w2 = lnc_bicubic_weight_t_96_simple[
					TABLE_LEN_96 - u].w1;
				w3 = lnc_bicubic_weight_t_96_simple[
					TABLE_LEN_96 - u].w0;

				sub_tmp0 = b - a;
				sub_tmp1 = c - a;
				sub_tmp2 = d - a;
				out_value_tmp = ((uint32_t) a) << 10;
				out_value = out_value_tmp + sub_tmp0 * w1 +
						sub_tmp1 * w2 + sub_tmp2 * w3;
			}
		} else {
			u = u * (TABLE_LEN_128 / box);

			if (u < (TABLE_LEN_128 / 2 + 1)) {
				w0 = lnc_bicubic_weight_t_128_simple[u].w0;
				w1 = lnc_bicubic_weight_t_128_simple[u].w1;
				w2 = lnc_bicubic_weight_t_128_simple[u].w2;

				sub_tmp0 = a - d;
				sub_tmp1 = b - d;
				sub_tmp2 = c - d;
				out_value_tmp = ((uint32_t) d) << 10;
				out_value = out_value_tmp + sub_tmp0 * w0 +
						sub_tmp1 * w1 + sub_tmp2 * w2;
			} else {
				w1 = lnc_bicubic_weight_t_128_simple[
					TABLE_LEN_128 - u].w2;
				w2 = lnc_bicubic_weight_t_128_simple[
					TABLE_LEN_128 - u].w1;
				w3 = lnc_bicubic_weight_t_128_simple[
					TABLE_LEN_128 - u].w0;

				sub_tmp0 = b - a;
				sub_tmp1 = c - a;
				sub_tmp2 = d - a;
				out_value_tmp = ((uint32_t) a) << 10;
				out_value = out_value_tmp + sub_tmp0 * w1 +
						sub_tmp1 * w2 + sub_tmp2 * w3;
			}
		}
	} else {
		if (u < (box / 2 + 1)) {
			w0 = simple[u].w0;
			w1 = simple[u].w1;
			w2 = simple[u].w2;

			sub_tmp0 = a - d;
			sub_tmp1 = b - d;
			sub_tmp2 = c - d;
			out_value_tmp = ((uint32_t) d) << 10;
			out_value = out_value_tmp + sub_tmp0 * w0 +
					sub_tmp1 * w1 + sub_tmp2 * w2;
		} else {
			w1 = simple[box - u].w2;
			w2 = simple[box - u].w1;
			w3 = simple[box - u].w0;

			sub_tmp0 = b - a;
			sub_tmp1 = c - a;
			sub_tmp2 = d - a;
			out_value_tmp = ((uint32_t) a) << 10;
			out_value = out_value_tmp + sub_tmp0 * w1 +
					sub_tmp1 * w2 + sub_tmp2 * w3;
		}
	}
#if 0
	/*
	 * for LSC gain, 1024 = 1.0, 4095 = 4.0;
	 * 4095*2 is for boundary extension.
	 */
	CLIP(out_value, 4095*1024*2, 1024*1024);
#endif
	/*
	 * for LSC gain, 1024 = 1.0, 16383 = 16.0;
	 * 16383 is for boundary extension, 14 bit parameter is used.
	 */
	CLIP(out_value, 16383 * 1024, 1024 * 1024);

	out_value_uint16_t = (uint16_t) ((out_value + 512) >> 10);

	return out_value_uint16_t;
}

static int generate_q_values(uint32_t word_endian, uint16_t q_val[][2],
			     void *param_address, uint16_t grid_pitch,
			     uint16_t grid_width, uint16_t u,
			     struct lnc_bicubic_weight_t_64_tag *simple)
{
	uint8_t i;
	uint16_t a0 = 0, b0 = 0, c0 = 0, d0 = 0;
	uint16_t a1 = 0, b1 = 0, c1 = 0, d1 = 0;
	uint32_t *addr = (uint32_t *)param_address;

	if (IS_ERR(param_address) || grid_width == 0 || grid_pitch == 0) {
		pr_err("fail to get param_address addr=0x%p, grid_width=%d, grid_pitch=%d\n",
		       param_address, grid_width, grid_pitch);
		return -1;
	}

	for (i = 0; i < 5; i++) {
		if (word_endian == 1) { /* ABCD */
			/* AB */
			a0 = *(addr + i * 2) >> 16;
			b0 = *(addr + i * 2 + grid_pitch * 2) >> 16;
			c0 = *(addr + i * 2 + grid_pitch * 2 * 2) >> 16;
			d0 = *(addr + i * 2 + grid_pitch * 2 * 3) >> 16;
			/* CD */
			a1 = *(addr + i * 2) & 0xFFFF;
			b1 = *(addr + i * 2 + grid_pitch * 2) & 0xFFFF;
			c1 = *(addr + i * 2 + grid_pitch * 2 * 2) & 0xFFFF;
			d1 = *(addr + i * 2 + grid_pitch * 2 * 3) & 0xFFFF;
		} else if (word_endian == 2) {	/* CDAB = 1 word */
			a0 = *(addr + i * 2) & 0xFFFF;
			b0 = *(addr + i * 2 + grid_pitch * 2) & 0xFFFF;
			c0 = *(addr + i * 2 + grid_pitch * 2 * 2) & 0xFFFF;
			d0 = *(addr + i * 2 + grid_pitch * 2 * 3) & 0xFFFF;
			a1 = *(addr + i * 2) >> 16;
			b1 = *(addr + i * 2 + grid_pitch * 2) >> 16;
			c1 = *(addr + i * 2 + grid_pitch * 2 * 2) >> 16;
			d1 = *(addr + i * 2 + grid_pitch * 2 * 3) >> 16;
		} else if (word_endian == 0) {	/* DCBA = 1 word */
			a0 = ((*(addr + i * 2) << 8) & 0x0000FF00) |
				((*(addr + i * 2) >> 8) & 0x000000FF);
			b0 = ((*(addr + i * 2 + grid_pitch * 2) << 8) &
			      0x0000FF00) |
				((*(addr + i * 2 + grid_pitch * 2) >> 8) &
				 0x000000FF);
			c0 = ((*(addr + i * 2 + grid_pitch * 2 * 2) << 8) &
			      0x0000FF00) |
				((*(addr + i * 2 + grid_pitch * 2 * 2) >> 8) &
				 0x000000FF);
			d0 = ((*(addr + i * 2 + grid_pitch * 2 * 3) << 8) &
			      0x0000FF00) |
				((*(addr + i * 2 + grid_pitch * 2 * 3) >> 8) &
				 0x000000FF);
			a1 = ((*(addr + i * 2) << 8) & 0xFF000000) |
				((*(addr + i * 2) >> 8) & 0x00FF0000);
			b1 = ((*(addr + i * 2 + grid_pitch * 2) << 8) &
			      0xFF000000) |
				((*(addr + i * 2 + grid_pitch * 2) >> 8) &
				 0x00FF0000);
			c1 = ((*(addr + i * 2 + grid_pitch * 2 * 2) << 8) &
			      0xFF000000) |
				((*(addr + i * 2 + grid_pitch * 2 * 2) >> 8) &
				 0x00FF0000);
			d1 = ((*(addr + i * 2 + grid_pitch * 2 * 3) << 8) &
			      0xFF000000) |
				((*(addr + i * 2 + grid_pitch * 2 * 3) >> 8) &
				 0x00FF0000);
		} else if (word_endian == 3) {	/* BADC = 1 word */
			a0 = ((*(addr + i * 2) << 8) & 0xFF000000) |
				((*(addr + i * 2) >> 8) & 0x00FF0000);
			b0 = ((*(addr + i * 2 + grid_pitch * 2) << 8) &
			      0xFF000000) |
				((*(addr + i * 2 + grid_pitch * 2) >> 8) &
				 0x00FF0000);
			c0 = ((*(addr + i * 2 + grid_pitch * 2 * 2) << 8) &
			      0xFF000000) |
				((*(addr + i * 2 + grid_pitch * 2 * 2) >> 8) &
				 0x00FF0000);
			d0 = ((*(addr + i * 2 + grid_pitch * 2 * 3) << 8) &
			      0xFF000000) |
				((*(addr + i * 2 + grid_pitch * 2 * 3) >> 8) &
				 0x00FF0000);
			a1 = ((*(addr + i * 2) << 8) & 0x0000FF00) |
				((*(addr + i * 2) >> 8) & 0x000000FF);
			b1 = ((*(addr + i * 2 + grid_pitch * 2) << 8) &
			      0x0000FF00) |
				((*(addr + i * 2 + grid_pitch * 2) >> 8) &
				 0x000000FF);
			c1 = ((*(addr + i * 2 + grid_pitch * 2 * 2) << 8) &
			      0x0000FF00) |
				((*(addr + i * 2 + grid_pitch * 2 * 2) >> 8) &
				 0x000000FF);
			d1 = ((*(addr + i * 2 + grid_pitch * 2 * 3) << 8) &
			      0x0000FF00) |
				((*(addr + i * 2 + grid_pitch * 2 * 3) >> 8) &
				 0x000000FF);
		}

		q_val[i][0] = cubic_1d(a0, b0, c0, d0, u, grid_width, simple);
		q_val[i][1] = cubic_1d(a1, b1, c1, d1, u, grid_width, simple);
	}

	return 0;
}

static void free_lsc_2d_slice_grid_buf(struct slice_context_info *cxt)
{
	struct slice_base_info *base_info = NULL;
	struct slice_lsc_2d_info *lsc_2d_info = NULL;
	uint32_t cur_slice_id, slice_num;

	if (!cxt) {
		pr_err("fail to get valid param, NULL\n");
		return;
	}

	base_info = &cxt->base_info;
	cur_slice_id = base_info->cur_slice_id;
	slice_num = base_info->slice_num;

	for (; cur_slice_id < slice_num; cur_slice_id++) {
		lsc_2d_info = &cxt->lsc_2d_info[cur_slice_id];

		kfree(lsc_2d_info->grid_buf);
		lsc_2d_info->grid_buf = NULL;
	}
}

static int set_slice_lsc_2d_info(struct slice_param_in *in_ptr,
				 struct slice_context_info *cxt)
{
	int rtn = 0;
	struct isp_pipe_dev *dev = NULL;
	struct isp_k_block *isp_k_bp = NULL;
	struct slice_base_info *base_info = NULL;
	struct slice_lsc_2d_info *lsc_2d_info = NULL;
	uint32_t cur_slice_id, slice_num;
	uint32_t start_col, end_col, start_row, end_row;
	uint32_t start_grid_col, end_grid_col, start_grid_row, end_grid_row;
	uint32_t grid_width, grid_pitch, grid_num, grid_x, grid_y;
	uint32_t g_r, g_c, addr_off, tmp_off;
	uint16_t *grid_buf = NULL;
	uint32_t *slice_grid_buf = NULL;
	uint16_t tmp_grid_0, tmp_grid_1, tmp_grid_2, tmp_grid_3;

	if (!in_ptr || !cxt) {
		pr_err("fail to get valid param, NULL\n");
		return -1;
	}

	if (in_ptr->mid == ISP_AP_MODE) {
		pr_info("not support in AP mode yet!");
		return -1;
	}

	if (in_ptr->isp_dev) {
		dev = in_ptr->isp_dev;
		isp_k_bp = &dev->isp_k_param;
	} else {
		pr_err("fail to get isp dev null\n");
		return -1;
	}

	base_info = &cxt->base_info;
	cur_slice_id = base_info->cur_slice_id;
	slice_num = base_info->slice_num;

	grid_width = isp_k_bp->lsc_cap_grid_width;
	grid_pitch = isp_k_bp->lsc_cap_grid_pitch;
	grid_buf = isp_k_bp->lsc_buf_info.sw_addr;

	if (isp_k_bp->lsc_bypass) {
		pr_debug("lsc_2d has been bypassed!\n");
		return 0;
	} else if (IS_ERR(grid_buf) || grid_width == 0 || grid_pitch == 0) {
		pr_err("fail to get valid param,grid_width:%d, grid_pitch:%d,grid_buf:0x%p,bypassing 2dlsc\n",
		       grid_width, grid_pitch, grid_buf);
		free_lsc_2d_slice_grid_buf(cxt);
		ISP_REG_MWR(0, ISP_LENS_PARAM, BIT_0, 1);
		return 0;
	}

	pr_debug("grid_width:%d, grid_pitch:%d, grid_buf:0x%p\n",
		 grid_width, grid_pitch, grid_buf);

	for (; cur_slice_id < slice_num; cur_slice_id++) {
		lsc_2d_info = &cxt->lsc_2d_info[cur_slice_id];
		start_col = base_info->slice_pos_array[cur_slice_id].start_col;
		end_col = base_info->slice_pos_array[cur_slice_id].end_col;
		start_row = base_info->slice_pos_array[cur_slice_id].start_row;
		end_row = base_info->slice_pos_array[cur_slice_id].end_row;

		start_grid_col = (start_col / 2) / grid_width;
		start_grid_row = (start_row / 2) / grid_width;
		end_grid_col = (end_col / 2) / grid_width + 1;
		end_grid_row = (end_row / 2) / grid_width + 1;
		grid_x = end_grid_col - start_grid_col + 1;
		grid_y = end_grid_row - start_grid_row + 1;

		lsc_2d_info->start_col = start_col;
		lsc_2d_info->start_row = start_row;
		lsc_2d_info->slice_width = end_col - start_col + 1;
		lsc_2d_info->slice_height = end_row - start_row + 1;
		lsc_2d_info->grid_x_num = grid_x;
		lsc_2d_info->grid_y_num = grid_y;
		lsc_2d_info->grid_num_t = (grid_x + 2) * (grid_y + 2);
		lsc_2d_info->relative_x = start_col % (grid_width * 2);
		lsc_2d_info->relative_y = start_row % (grid_width * 2);

		/*
		 * get addr_off using (row * pitch + col)*4*2, means 4 channel
		 * and 2 word space for each channel.
		 */
		addr_off = (start_grid_row * grid_pitch + start_grid_col) << 3;
		rtn = generate_q_values(1, lsc_2d_info->q_val,
			(void *)((uint8_t *)grid_buf + addr_off),
			(uint16_t)(grid_pitch & 0xFF),
			(uint16_t)(grid_width & 0xFF),
			lsc_2d_info->relative_y >> 1,
			NULL);
		if (rtn) {
			pr_err("fail to generate_q_values, ret=%d\n", rtn);
			return rtn;
		}

		grid_num = lsc_2d_info->grid_num_t;
		slice_grid_buf = kmalloc(grid_num << 3,
					 GFP_ATOMIC | __GFP_ZERO);
		if (!slice_grid_buf)
			return -ENOMEM;

		lsc_2d_info->grid_buf = (uint16_t *)slice_grid_buf;

		for (g_r = 0; g_r < grid_y + 2; g_r++) {
			for (g_c = 0; g_c < grid_x + 2; g_c++) {
				/*
				 * tmp_off unit is half-word(16bit),
				 * each cell(64bit) has 4 channel,
				 * each channel needs 16-bit in external memory.
				 */
				tmp_off = ((start_grid_row + g_r) * grid_pitch +
					   start_grid_col + g_c) << 2;
				tmp_grid_3 = *(grid_buf + tmp_off + 3);
				tmp_grid_2 = *(grid_buf + tmp_off + 2);
				*slice_grid_buf++ =
					((tmp_grid_3 & 0x3FFF) << 14) |
					(tmp_grid_2 & 0x3FFF);
				tmp_grid_1 = *(grid_buf + tmp_off + 1);
				tmp_grid_0 = *(grid_buf + tmp_off);
				*slice_grid_buf++ =
					((tmp_grid_1 & 0x3FFF) << 14) |
					(tmp_grid_0 & 0x3FFF);
			}
		}

	}

	return 0;
}

static void get_slice_fetch_pitch(struct slice_pitch *pitch_ptr,
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
	case ISP_FETCH_YUYV:
	case ISP_FETCH_UYVY:
	case ISP_FETCH_YVYU:
	case ISP_FETCH_VYUY:
	case ISP_FETCH_RAW_10:
		pitch_ptr->chn0 = width << 1;
		break;
	case ISP_FETCH_CSI2_RAW_10:
		pitch_ptr->chn0 = round_up(
			(((width >> 2) + ((width & 0x3) & 0x1)) * 5), 4);
		break;
	default:
		break;
	}
}

static int set_slice_fetch_info(struct slice_param_in *in_ptr,
				struct slice_context_info *cxt)
{
	int rtn = 0;
	struct slice_base_info *base_info = NULL;
	struct slice_fetch_info *fetch_info = NULL;
	struct slice_addr *address = NULL;
	uint32_t cur_slice_id, slice_num;
	struct slice_pitch fetch_pitch = {0};
	uint32_t start_col, end_col, start_row, end_row;
	uint32_t ch0_offset = 0;
	uint32_t ch1_offset = 0;
	uint32_t ch2_offset = 0;
	uint32_t mipi_word_num_start[16] = {
		0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5};
	uint32_t mipi_word_num_end[16] = {
		0, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5};

	if (!in_ptr || !cxt) {
		pr_err("fail to get input handle is NULL!\n");
		rtn = -1;
		goto exit;
	}

	base_info = &cxt->base_info;
	address = &in_ptr->fetch_addr;
	cur_slice_id = base_info->cur_slice_id;
	slice_num = base_info->slice_num;

	get_slice_fetch_pitch(&fetch_pitch,
		in_ptr->fetch_format, base_info->img_width);
	for (; cur_slice_id < slice_num; cur_slice_id++) {
		fetch_info = &cxt->fetch_info[cur_slice_id];
		fetch_info->addr.chn0 = address->chn0;
		fetch_info->addr.chn1 = address->chn1;
		fetch_info->addr.chn2 = address->chn2;
		start_col = base_info->slice_pos_array[cur_slice_id].start_col;
		end_col = base_info->slice_pos_array[cur_slice_id].end_col;
		start_row = base_info->slice_pos_array[cur_slice_id].start_row;
		end_row = base_info->slice_pos_array[cur_slice_id].end_row;
		switch (in_ptr->fetch_format) {
		case ISP_FETCH_YUV422_3FRAME:
			ch0_offset = start_row * fetch_pitch.chn0
				+ start_col;
			ch1_offset = start_row * fetch_pitch.chn1
				+ ((start_col + 1) >> 1);
			ch2_offset = start_row * fetch_pitch.chn2
				+ ((start_col + 1) >> 1);
			break;
		case ISP_FETCH_YUYV:
		case ISP_FETCH_UYVY:
		case ISP_FETCH_YVYU:
		case ISP_FETCH_VYUY:
		case ISP_FETCH_RAW_10:
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
		case ISP_FETCH_CSI2_RAW_10:
			ch0_offset = start_row * fetch_pitch.chn0
				+ (start_col >> 2) * 5
				+ (start_col & 0x3);

			fetch_info->mipi_byte_rel_pos = start_col & 0x0f;
			fetch_info->mipi_word_num = (
				(((end_col + 1) >> 4) * 5 +
				mipi_word_num_end[(end_col + 1) & 0x0f])
				-(((start_col + 1) >> 4) * 5 +
				mipi_word_num_start[(start_col + 1) & 0x0f])
				+ 1);
			break;
		default:
			break;
		}

		fetch_info->addr.chn0 += ch0_offset;
		fetch_info->addr.chn1 += ch1_offset;
		fetch_info->addr.chn2 += ch2_offset;
		fetch_info->size.height = end_row - start_row + 1;
		fetch_info->size.width = end_col - start_col + 1;

	}
exit:
	return rtn;
}

static int set_slice_dispatch_info(struct slice_param_in *in_ptr,
				   struct slice_context_info *cxt)
{
	int rtn = 0;
	uint32_t cur_slice_id, slice_num;
	uint32_t start_col, end_col, start_row, end_row;
	struct slice_base_info *base_info = NULL;
	struct slice_dispatch_info *dispath_info = NULL;
	uint32_t bayer_mode = 0;

	if (!in_ptr || !cxt) {
		pr_err("fail to get input handle is NULL!\n");
		rtn = -1;
		goto exit;
	}

	bayer_mode = in_ptr->bayer_mode;
	base_info = &cxt->base_info;
	cur_slice_id = base_info->cur_slice_id;
	slice_num = base_info->slice_num;

	for (; cur_slice_id < slice_num; cur_slice_id++) {
		dispath_info = &cxt->dispatch_info[cur_slice_id];
		start_col = base_info->slice_pos_array[cur_slice_id].start_col;
		end_col = base_info->slice_pos_array[cur_slice_id].end_col;
		start_row = base_info->slice_pos_array[cur_slice_id].start_row;
		end_row = base_info->slice_pos_array[cur_slice_id].end_row;
		dispath_info->size.height = end_row - start_row + 1;
		dispath_info->size.width = end_col - start_col + 1;

		if (start_row & 0x1)
			bayer_mode ^= 0x2;
		if (start_col & 0x1)
			bayer_mode ^= 0x1;

		dispath_info->bayer_mode = bayer_mode;
	}
exit:
	return rtn;
}

static int set_slice_postcnr_info(struct slice_context_info *cxt)
{
	int rtn = 0;
	uint32_t cur_slice_id, slice_num;
	struct slice_base_info *base_info = NULL;
	struct slice_postcnr_info *postcnr_info = NULL;

	if (!cxt) {
		pr_err("fail to get input handle is NULL!\n");
		rtn = -1;
		goto exit;
	}

	base_info = &cxt->base_info;
	cur_slice_id = base_info->cur_slice_id;
	slice_num = base_info->slice_num;

	for (; cur_slice_id < slice_num; cur_slice_id++) {
		postcnr_info = &cxt->postcnr_info[cur_slice_id];
		postcnr_info->start_row_mod4 =
			base_info->slice_pos_array[cur_slice_id].start_row
			& 0x3;
	}
exit:
	return rtn;
}

static int set_slice_ynr_info(struct slice_context_info *cxt)
{
	int rtn = 0;
	uint32_t cur_slice_id, slice_num;
	struct slice_base_info *base_info = NULL;
	struct slice_ynr_info *ynr_info = NULL;

	if (!cxt) {
		pr_err("fail to get input handle is NULL!\n");
		rtn = -1;
		goto exit;
	}

	base_info = &cxt->base_info;
	cur_slice_id = base_info->cur_slice_id;
	slice_num = base_info->slice_num;

	for (; cur_slice_id < slice_num; cur_slice_id++) {
		ynr_info = &cxt->ynr_info[cur_slice_id];
		ynr_info->start_col =
			base_info->slice_pos_array[cur_slice_id].start_col;
		ynr_info->start_row =
			base_info->slice_pos_array[cur_slice_id].start_row;
	}
exit:
	return rtn;
}

static void set_path_trim0_info(struct isp_scaler_slice_tmp *slice)
{
	uint32_t start, end;
	struct slice_scaler_info *out =
		(struct slice_scaler_info *)slice->scaler_slice;
	struct slice_scaler_path *in =
		(struct slice_scaler_path *)slice->scaler_yuv;

	/* trim0 x */
	start = slice->start_col+slice->overlap_bad_left;
	end = slice->end_col+1-slice->overlap_bad_right;
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
				out->trim0_size_x = slice->trim0_end_x-start;
			}
		} else {
			if (in->trim0_start_x < start) {
				out->trim0_start_x = slice->overlap_bad_left;
				if (slice->trim0_end_x < end)
					out->trim0_size_x =
					slice->trim0_end_x-start;
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
	end = slice->end_row+1 - slice->overlap_bad_down;
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
					slice->trim0_end_y-start;
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

static void set_path_deci_info(struct isp_scaler_slice_tmp *slice)
{
	uint32_t start;
	struct slice_scaler_info *out =
		(struct slice_scaler_info *)slice->scaler_slice;
	struct slice_scaler_path *in =
		(struct slice_scaler_path *)slice->scaler_yuv;

	slice->deci_x = in->deci_x;
	slice->deci_y = in->deci_y;
	slice->deci_x_align = slice->deci_x * 2;

	start = slice->start_col + slice->overlap_bad_left;
	if (in->trim0_start_x >= slice->start_col &&
		(in->trim0_start_x <= slice->end_col+1)) {
		out->trim0_size_x = out->trim0_size_x/
			slice->deci_x_align*slice->deci_x_align;
	} else {
		slice->trim0_start_adjust_x = (start+slice->deci_x_align-1)/
			slice->deci_x_align*slice->deci_x_align-start;
		out->trim0_start_x += slice->trim0_start_adjust_x;
		out->trim0_size_x -= slice->trim0_start_adjust_x;
		out->trim0_size_x = out->trim0_size_x/
			slice->deci_x_align*slice->deci_x_align;
	}

	if (in->odata_mode == 0)
		slice->deci_y_align = slice->deci_y;		/* 422 */
	else
		slice->deci_y_align = slice->deci_y * 2;	/* 420 */

	start = slice->start_row + slice->overlap_bad_up;
	if (in->trim0_start_y >= slice->start_row &&
		(in->trim0_start_y <= slice->end_row+1)) {
		out->trim0_size_y = out->trim0_size_y/
			slice->deci_y_align*slice->deci_y_align;
	} else {
		slice->trim0_start_adjust_y = (start+slice->deci_y_align-1)/
			slice->deci_y_align*slice->deci_y_align-start;
		out->trim0_start_y += slice->trim0_start_adjust_y;
		out->trim0_size_y -= slice->trim0_start_adjust_y;
		out->trim0_size_y = out->trim0_size_y/
			slice->deci_y_align*slice->deci_y_align;
	}

	out->scaler_in_width = out->trim0_size_x/slice->deci_x;
	out->scaler_in_height = out->trim0_size_y/slice->deci_y;
}

static void set_path_scaler_info(struct isp_scaler_slice_tmp *slice)
{
	struct slice_scaler_info *out =
		(struct slice_scaler_info *)slice->scaler_slice;
	struct slice_scaler_path *in =
		(struct slice_scaler_path *)slice->scaler_yuv;
	uint32_t scl_factor_in, scl_factor_out;
	uint32_t  initial_phase, last_phase, phase_in;
	uint32_t phase_tmp, scl_temp, out_tmp;
	uint32_t start, end;
	uint32_t tap_hor, tap_ver, tap_hor_uv, tap_ver_uv;
	uint32_t tmp, n;

	if (in->scaler_bypass == 0) {
		scl_factor_in = in->scaler_factor_in/2;
		scl_factor_out = in->scaler_factor_out/2;
		initial_phase = 0;
		last_phase = initial_phase+
			scl_factor_in*(in->scaler_out_width/2-1);
		tap_hor = 8;
		tap_hor_uv = tap_hor / 2;

		start = slice->start_col+slice->overlap_bad_left+
			slice->deci_x_align-1;
		end = slice->end_col+1-slice->overlap_bad_right+
			slice->deci_x_align-1;
		if (in->trim0_start_x >= slice->start_col &&
			(in->trim0_start_x <= slice->end_col+1)) {
			phase_in = 0;
			if (out->scaler_in_width ==
				in->trim0_size_x/slice->deci_x)
				phase_tmp = last_phase;
			else
				phase_tmp = (out->scaler_in_width/2-
				tap_hor_uv/2)*scl_factor_out -
				scl_factor_in/2 - 1;
			out_tmp = (phase_tmp - phase_in)/scl_factor_in+1;
			out->scaler_out_width = out_tmp*2;
		} else {
			phase_in = (tap_hor_uv/2)*scl_factor_out;
			if (slice->cur_col == slice->slice_col_num - 1) {
				phase_tmp = last_phase-
					((in->trim0_size_x/2)/slice->deci_x-
					out->scaler_in_width/2)*scl_factor_out;
				out_tmp = (phase_tmp-phase_in)/scl_factor_in+1;
				out->scaler_out_width = out_tmp*2;
				phase_in = phase_tmp-(out_tmp-1)*scl_factor_in;
			} else {
				if (slice->trim0_end_x >= slice->start_col
					&& (slice->trim0_end_x <= slice->end_col
					+1-slice->overlap_bad_right)) {
					phase_tmp = last_phase-
					((in->trim0_size_x/2)/slice->deci_x-
					out->scaler_in_width/2)*scl_factor_out;
					out_tmp = (phase_tmp-phase_in)/
						scl_factor_in+1;
					out->scaler_out_width = out_tmp*2;
					phase_in = phase_tmp-(out_tmp-1)
						*scl_factor_in;
				} else {
					initial_phase = ((((start/
					slice->deci_x_align*slice->deci_x_align
					-in->trim0_start_x)/slice->deci_x)/2+
					(tap_hor_uv/2))*(scl_factor_out)+
					(scl_factor_in-1))/scl_factor_in*
					scl_factor_in;
					slice->scaler_out_width_temp =
					((last_phase-initial_phase)/
					scl_factor_in+1)*2;

					scl_temp = ((end/slice->deci_x_align*
					slice->deci_x_align-in->trim0_start_x)/
					slice->deci_x)/2;
					last_phase = ((scl_temp-tap_hor_uv/2)*
					(scl_factor_out)-scl_factor_in/2-1)/
					scl_factor_in*scl_factor_in;

					out_tmp = (last_phase-initial_phase)/
						scl_factor_in+1;
					out->scaler_out_width = out_tmp*2;
					phase_in = initial_phase-(((start/
					slice->deci_x_align*slice->deci_x_align-
					in->trim0_start_x)/slice->deci_x)/2)*
					scl_factor_out;
				}
			}
		}

		calc_scaler_phase(phase_in*4, scl_factor_out*2,
			&out->scaler_ip_int, &out->scaler_ip_rmd);
		calc_scaler_phase(phase_in, scl_factor_out,
			&out->scaler_cip_int, &out->scaler_cip_rmd);

		scl_factor_in = in->scaler_ver_factor_in;
		scl_factor_out = in->scaler_ver_factor_out;
		initial_phase = 0;
		last_phase = initial_phase+
			scl_factor_in*(in->scaler_out_height-1);
		tap_ver = in->scaler_y_ver_tap > in->scaler_uv_ver_tap ?
			in->scaler_y_ver_tap : in->scaler_uv_ver_tap;
		tap_ver += 2;
		tap_ver_uv = tap_ver;

		start = slice->start_row+slice->overlap_bad_up+
			slice->deci_y_align-1;
		end = slice->end_row+1-slice->overlap_bad_down+
			slice->deci_y_align-1;
		if (in->trim0_start_y >= slice->start_row &&
			(in->trim0_start_y <= slice->end_row+1)) {
			phase_in = 0;
			if (out->scaler_in_height ==
				in->trim0_size_y/slice->deci_y)
				phase_tmp = last_phase;
			else
				phase_tmp = (out->scaler_in_height-
				tap_ver_uv/2)*scl_factor_out-1;
			out_tmp = (phase_tmp-phase_in)/scl_factor_in+1;
			if (out_tmp%2 == 1)
				out_tmp -= 1;
			out->scaler_out_height = out_tmp;
		} else {
			phase_in = (tap_ver_uv/2)*scl_factor_out;
			if (slice->cur_row == slice->slice_row_num-1) {
				phase_tmp = last_phase-
					(in->trim0_size_y/slice->deci_y-
					out->scaler_in_height)*scl_factor_out;
				out_tmp = (phase_tmp-phase_in)/scl_factor_in+1;
				if (out_tmp%2 == 1)
					out_tmp -= 1;
				if (in->odata_mode == 1 && out_tmp%4 != 0)
					out_tmp = out_tmp/4*4;
				out->scaler_out_height = out_tmp;
				phase_in = phase_tmp-(out_tmp-1)*scl_factor_in;
			} else {
				if (slice->trim0_end_y >= slice->start_row &&
					(slice->trim0_end_y <= slice->end_row+1
					-slice->overlap_bad_down)) {
					phase_tmp = last_phase-
					(in->trim0_size_y/slice->deci_y-
					out->scaler_in_height)*scl_factor_out;
					out_tmp = (phase_tmp-phase_in)/
						scl_factor_in+1;
					if (out_tmp%2 == 1)
						out_tmp -= 1;
					if (in->odata_mode == 1 &&
						out_tmp % 4 != 0)
						out_tmp = out_tmp/4*4;
					out->scaler_out_height = out_tmp;
					phase_in = phase_tmp-(out_tmp-1)
						*scl_factor_in;
				} else {
					initial_phase = (((start/
					slice->deci_y_align*slice->deci_y_align
					-in->trim0_start_y)/slice->deci_y+
					(tap_ver_uv/2))*(scl_factor_out)+
					(scl_factor_in-1))/(scl_factor_in*2)
					*(scl_factor_in*2);
					slice->scaler_out_height_temp =
						(last_phase-initial_phase)/
						scl_factor_in+1;
					scl_temp = (end/slice->deci_y_align*
					slice->deci_y_align-in->trim0_start_y)/
					slice->deci_y;
					last_phase = ((scl_temp-tap_ver_uv/2)*
					(scl_factor_out)-1)/scl_factor_in
					*scl_factor_in;
					out_tmp = (last_phase-initial_phase)/
						scl_factor_in+1;
					if (out_tmp%2 == 1)
						out_tmp -= 1;
					if (in->odata_mode == 1 &&
						out_tmp%4 != 0)
						out_tmp = out_tmp/4*4;
					out->scaler_out_height = out_tmp;
					phase_in = initial_phase-(start/
					slice->deci_y_align*slice->deci_y_align-
					in->trim0_start_y)/slice->deci_y
					*scl_factor_out;
				}
			}
		}

		calc_scaler_phase(phase_in, scl_factor_out,
			&out->scaler_ip_int_ver, &out->scaler_ip_rmd_ver);
		if (in->odata_mode == 1) {
			phase_in /= 2;
			scl_factor_out /= 2;
		}
		calc_scaler_phase(phase_in, scl_factor_out,
			&out->scaler_cip_int_ver, &out->scaler_cip_rmd_ver);

		if (out->scaler_ip_int >= 16) {
			tmp = out->scaler_ip_int;
			n = (tmp >> 3) - 1;
			out->trim0_start_x += 8*n*slice->deci_x;
			out->trim0_size_x -= 8*n*slice->deci_x;
			out->scaler_ip_int -= 8*n;
			out->scaler_cip_int -= 4*n;
		}
		if (out->scaler_ip_int >= 16)
			pr_err("fail to initial Horizontal slice phase overflowed!\n");
		if (out->scaler_ip_int_ver >= 16) {
			tmp = out->scaler_ip_int_ver;
			n = (tmp >> 3) - 1;
			out->trim0_start_y += 8*n*slice->deci_y;
			out->trim0_size_y -= 8*n*slice->deci_y;
			out->scaler_ip_int_ver -= 8*n;
			out->scaler_cip_int_ver -= 8*n;
		}
		if (out->scaler_ip_int_ver >= 16)
			pr_err("fail to initial Vertical slice phase overflowed!\n");
	} else {
		out->scaler_out_width = out->scaler_in_width;
		out->scaler_out_height = out->scaler_in_height;
		start = slice->start_col+slice->overlap_bad_left+
			slice->trim0_start_adjust_x+slice->deci_x_align-1;
		slice->scaler_out_width_temp = (in->trim0_size_x-(start/
			slice->deci_x_align*slice->deci_x_align-
			in->trim0_start_x))/slice->deci_x;
		start = slice->start_row+slice->overlap_bad_up+
			slice->trim0_start_adjust_y+slice->deci_y_align-1;
		slice->scaler_out_height_temp = (in->trim0_size_y-(start/
			slice->deci_y_align*slice->deci_y_align-
			in->trim0_start_y))/slice->deci_y;
	}
}

static void set_path_trim1_info(struct isp_scaler_slice_tmp *slice,
				struct slice_scaler_info *scaler_info)
{
	struct slice_scaler_info *out =
		(struct slice_scaler_info *)slice->scaler_slice;
	struct slice_scaler_path *in =
		(struct slice_scaler_path *)slice->scaler_yuv;
	uint32_t trim_sum_x = 0;
	uint32_t trim_sum_y = 0;
	uint32_t pix_align = 8;
	uint32_t i = 0;

	if (in->trim0_start_x >= slice->start_col &&
		(in->trim0_start_x <= slice->end_col+1)) {
		out->trim1_start_x = 0;
		if (out->scaler_in_width == in->trim0_size_x)
			out->trim1_size_x = out->scaler_out_width;
		else
			out->trim1_size_x = out->scaler_out_width/
			pix_align*pix_align;
	} else {
		for (i = 1; i < slice->cur_col+1; i++)
			trim_sum_x +=
			scaler_info[slice->cur_slice_id-i].trim1_size_x;

		if (slice->cur_col == slice->slice_col_num - 1) {
			out->trim1_size_x = in->scaler_out_width-trim_sum_x;
			out->trim1_start_x = out->scaler_out_width-
				out->trim1_size_x;
		} else {
			if (slice->trim0_end_x >= slice->start_col &&
				(slice->trim0_end_x <= slice->end_col+1
				-slice->overlap_bad_right)) {
				out->trim1_size_x = in->scaler_out_width
					-trim_sum_x;
				out->trim1_start_x = out->scaler_out_width-
					out->trim1_size_x;
			} else {
				out->trim1_start_x =
					slice->scaler_out_width_temp-
					(in->scaler_out_width-trim_sum_x);
				out->trim1_size_x = (out->scaler_out_width-
					out->trim1_start_x)/pix_align*pix_align;
			}
		}
	}

	if (in->trim0_start_y >= slice->start_row &&
		(in->trim0_start_y <= slice->end_row+1)) {
		out->trim1_start_y = 0;
		if (out->scaler_in_height == in->trim0_size_y)
			out->trim1_size_y = out->scaler_out_height;
		else
			out->trim1_size_y = out->scaler_out_height/
			pix_align*pix_align;
	} else {
		for (i = 1; i < slice->cur_row+1; i++)
			trim_sum_y += scaler_info[slice->cur_slice_id-
			i*slice->slice_col_num].trim1_size_y;

		if (slice->cur_row == slice->slice_row_num - 1) {
			out->trim1_size_y = in->scaler_out_height-trim_sum_y;
			out->trim1_start_y = out->scaler_out_height-
				out->trim1_size_y;
		} else {
			if (slice->trim0_end_y >= slice->start_row &&
				(slice->trim0_end_y <= slice->end_row+1
				-slice->overlap_bad_down)) {
				out->trim1_size_y = in->scaler_out_height
					-trim_sum_y;
				out->trim1_start_y = out->scaler_out_height-
					out->trim1_size_y;
			} else {
				out->trim1_start_y =
					slice->scaler_out_height_temp-
					(in->scaler_out_height-trim_sum_y);
				out->trim1_size_y = (out->scaler_out_height-
					out->trim1_start_y)/pix_align*pix_align;
			}
		}
	}
}

static int set_path_info(struct slice_scaler_info *scaler_info,
			 struct slice_scaler_path *scaler_frame,
			 struct slice_base_info *base_info,
			 uint32_t row, uint32_t col)
{
	int rtn = 0;
	uint32_t cur_slice_id;
	struct isp_scaler_slice_tmp slice = {0};

	if (!scaler_info || !scaler_frame || !base_info) {
		pr_err("fail to get valid param, NULL\n");
		rtn = -1;
		goto exit;
	}

	cur_slice_id = base_info->cur_slice_id;
	slice.cur_slice_id = cur_slice_id;
	slice.cur_col = col;
	slice.cur_row = row;
	slice.slice_col_num = base_info->slice_col_num;
	slice.slice_row_num = base_info->slice_row_num;
	slice.start_col = base_info->slice_pos_array[cur_slice_id].start_col;
	slice.end_col = base_info->slice_pos_array[cur_slice_id].end_col;
	slice.start_row = base_info->slice_pos_array[cur_slice_id].start_row;
	slice.end_row = base_info->slice_pos_array[cur_slice_id].end_row;
	slice.trim0_end_x = scaler_frame->trim0_start_x +
		scaler_frame->trim0_size_x;
	slice.trim0_end_y = scaler_frame->trim0_start_y +
		scaler_frame->trim0_size_y;
	slice.overlap_bad_up = base_info->overlap_up -
		YUVSCALER_OVERLAP_UP;
	slice.overlap_bad_down = base_info->overlap_down -
		YUVSCALER_OVERLAP_DOWN;
	slice.overlap_bad_left = base_info->overlap_left -
		YUVSCALER_OVERLAP_LEFT;
	slice.overlap_bad_right = base_info->overlap_right -
		YUVSCALER_OVERLAP_RIGHT;
	slice.scaler_slice = (uint32_t *)&scaler_info[cur_slice_id];
	slice.scaler_yuv = (uint32_t *)scaler_frame;

	set_path_trim0_info(&slice);
	set_path_deci_info(&slice);
	set_path_scaler_info(&slice);
	set_path_trim1_info(&slice, scaler_info);

	scaler_info[cur_slice_id].src_size_x = slice.end_col -
		slice.start_col + 1;
	scaler_info[cur_slice_id].src_size_y = slice.end_row -
		slice.start_row + 1;
	scaler_info[cur_slice_id].dst_size_x =
		scaler_info[cur_slice_id].scaler_out_width;
	scaler_info[cur_slice_id].dst_size_y =
		scaler_info[cur_slice_id].scaler_out_height;

	scaler_info[cur_slice_id].scaler_factor_in =
		scaler_frame->scaler_factor_in;
	scaler_info[cur_slice_id].scaler_factor_out =
		scaler_frame->scaler_factor_out;
	scaler_info[cur_slice_id].scaler_factor_in_ver =
		scaler_frame->scaler_ver_factor_in;
	scaler_info[cur_slice_id].scaler_factor_out_ver =
		scaler_frame->scaler_ver_factor_out;

exit:
	return rtn;
}

static int set_slice_scaler_info(struct slice_param_in *in_ptr,
				 struct slice_context_info *cxt)
{
	int rtn = 0;
	struct slice_base_info *base_info = NULL;
	struct slice_scaler_info *scaler_info = NULL;
	struct slice_scaler_path *scaler_frame = NULL;
	uint32_t cur_slice_id;
	uint32_t start_col, end_col, start_row, end_row;
	uint32_t slice_col_num, slice_row_num;
	uint32_t r = 0, c = 0;

	if (!in_ptr || !cxt) {
		pr_err("fail to get valid param, NULL\n");
		rtn = -1;
		goto exit;
	}

	base_info = &cxt->base_info;
	slice_col_num = base_info->slice_col_num;
	slice_row_num = base_info->slice_row_num;
	cur_slice_id = base_info->cur_slice_id;

	/* Currently, keep only capture slice scaler setting here. */
	scaler_frame =
		&in_ptr->scaler_frame[SLICE_PATH_CAP];
	scaler_info = cxt->scaler_info[SLICE_PATH_CAP];

	if (!scaler_frame->scaler_bypass) {
		for (r = 0; r < slice_row_num; r++) {
			for (c = 0; c < slice_col_num; c++) {
				base_info->cur_slice_id = r * slice_col_num + c;
#if 0
				if (in_ptr->pre_slice_need == 1) {
					scaler_info = cxt->
						scaler_info[SLICE_PATH_PRE];
					scaler_frame = &in_ptr->
						scaler_frame[SLICE_PATH_PRE];
					set_path_info(scaler_info,
						scaler_frame, base_info, r, c);
					if (rtn)
						goto exit;
				}

				if (in_ptr->vid_slice_need == 1) {
					scaler_info = cxt->
						scaler_info[SLICE_PATH_VID];
					scaler_frame = &in_ptr->
						scaler_frame[SLICE_PATH_VID];
					set_path_info(scaler_info,
						scaler_frame, base_info, r, c);
					if (rtn)
						goto exit;
				}
#endif
				if (in_ptr->cap_slice_need == 1) {
					scaler_info = cxt->
						scaler_info[SLICE_PATH_CAP];
					scaler_frame = &in_ptr->
						scaler_frame[SLICE_PATH_CAP];
					set_path_info(scaler_info,
						scaler_frame, base_info, r, c);
					/* rtn init to 0, and not changed
					 * if (rtn)
					 *	goto exit;
					 */
				}

				if (in_ptr->vid_slice_need == 1) {
					scaler_info = cxt->
						scaler_info[SLICE_PATH_VID];
					scaler_frame = &in_ptr->
						scaler_frame[SLICE_PATH_VID];
					set_path_info(scaler_info,
						scaler_frame, base_info, r, c);
					/* rtn init to 0, and not changed
					 * if (rtn)
					 *	goto exit;
					 */
				}
			}
		}
	} else {
		for (; cur_slice_id < base_info->slice_num; cur_slice_id++) {
			start_col = base_info->
				slice_pos_array[cur_slice_id].start_col;
			end_col = base_info->
				slice_pos_array[cur_slice_id].end_col;
			start_row = base_info->
				slice_pos_array[cur_slice_id].start_row;
			end_row = base_info->
				slice_pos_array[cur_slice_id].end_row;

			scaler_info[cur_slice_id].trim1_size_x = end_col -
				start_col + 1;
			scaler_info[cur_slice_id].trim1_size_y = end_row -
				start_row + 1;
		}
	}

	base_info->cur_slice_id = 0;

exit:
	return rtn;
}

static int set_slice_noisefliter_info(struct slice_context_info *cxt)
{
	int rtn = 0;
	uint32_t cur_slice_id, slice_num;
	uint32_t slice_width;
	struct slice_base_info *base_info = NULL;
	struct slice_noisefilter_info *noisefilter_info = NULL;
	struct slice_scaler_info *scaler_info = NULL;

	if (!cxt) {
		pr_err("fail to get valid param, NULL\n");
		rtn = -1;
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
		if ((int)slice_width < 0) {
			pr_err("fail to slice_width,id=%d width=%d\n",
				cur_slice_id, slice_width);
			rtn = -1;
			goto exit;
		}
		slice_noisefilter_seeds(slice_width, noisefilter_info->seed0,
			&noisefilter_info->seed1, &noisefilter_info->seed2,
			&noisefilter_info->seed3);
		noisefilter_info->seed_int = 1;
	}
exit:
	return rtn;
}

static int get_slice_store_pitch(struct slice_pitch *pitch_ptr,
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
		break;
	}

	return rtn;
}

void isp_slice_store_addr_init(struct slice_pitch *sf_pitch,
			       struct slice_store_info *store_info,
			       struct slice_store_path *store_frame,
			       uint32_t start_row_out,
			       uint32_t start_col_out,
			       uint32_t cs_id)
{
	uint32_t ch0_offset = 0;
	uint32_t ch1_offset = 0;
	uint32_t ch2_offset = 0;
	struct slice_addr *address = &store_frame->addr;

	store_info[cs_id].addr.chn0 = address->chn0;
	store_info[cs_id].addr.chn1 = address->chn1;
	store_info[cs_id].addr.chn2 = address->chn2;

	switch (store_frame->format) {
	case ISP_STORE_YUV422_3FRAME:
		ch0_offset = start_row_out*sf_pitch->chn0+
			start_col_out;
		ch1_offset = start_row_out*sf_pitch->chn1+
			((start_col_out+1)>>1);
		ch2_offset = start_row_out*sf_pitch->chn2+
			((start_col_out+1)>>1);
		break;
	case ISP_STORE_UYVY:
		ch0_offset = start_row_out*sf_pitch->chn0+
			start_col_out*2;
		break;
	case ISP_STORE_YUV422_2FRAME:
	case ISP_STORE_YVU422_2FRAME:
		ch0_offset = start_row_out*sf_pitch->chn0+
			start_col_out;
		ch1_offset = start_row_out*sf_pitch->chn1+
			start_col_out;
		break;
	case ISP_STORE_YUV420_2FRAME:
	case ISP_STORE_YVU420_2FRAME:
		ch0_offset = start_row_out*sf_pitch->chn0+
			start_col_out;
		ch1_offset = ((start_row_out*sf_pitch->chn1+1)>>1)+
			start_col_out;
		break;
	case ISP_STORE_YUV420_3FRAME:
		ch0_offset = start_row_out*sf_pitch->chn0+
			start_col_out;
		ch1_offset = ((start_row_out*sf_pitch->chn1+1)>>1)+
			((start_col_out+1)>>1);
		ch2_offset = ((start_row_out*sf_pitch->chn2+1)>>1)+
			((start_col_out+1)>>1);
		break;
	default:
		break;
	}

	store_info[cs_id].addr.chn0 += ch0_offset;
	store_info[cs_id].addr.chn1 += ch1_offset;
	store_info[cs_id].addr.chn2 += ch2_offset;

}

static int set_store_info(struct slice_store_info *store_info,
			  struct slice_store_path *store_frame,
			  struct slice_base_info *base_info,
			  struct slice_scaler_info *scaler_info,
			  uint32_t scl_bypass)
{
	int rtn = 0;
	uint32_t cur_slice_id, cur_slice_row;
	uint32_t scl_out_width, scl_out_height;
	uint32_t overlap_left, overlap_up,
		overlap_right, overlap_down;
	uint32_t start_col, end_col, start_row, end_row;
	uint32_t start_row_out, start_col_out;
	uint32_t tmp_slice_id;

	struct slice_pitch store_pitch = {0};

	if (!store_info || !store_frame || !base_info || !scaler_info) {
		pr_err("fail to get valid param, NULL\n");
		rtn = -1;
		goto exit;
	}

	cur_slice_id = base_info->cur_slice_id;
	rtn = get_slice_store_pitch(&store_pitch, store_frame->format,
		store_frame->size.width);

	if (scl_bypass == 0) {
		for (; cur_slice_id < base_info->slice_num; cur_slice_id++) {
			cur_slice_row = cur_slice_id/base_info->slice_col_num;
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

			isp_slice_store_addr_init(&store_pitch, store_info,
						  store_frame, start_row_out,
						  start_col_out, cur_slice_id);

		}
	} else {
		for (; cur_slice_id < base_info->slice_num; cur_slice_id++) {
			start_col = base_info->slice_pos_array
				[cur_slice_id].start_col;
			end_col = base_info->slice_pos_array
				[cur_slice_id].end_col;
			start_row = base_info->slice_pos_array
				[cur_slice_id].start_row;
			end_row = base_info->slice_pos_array
				[cur_slice_id].end_row;
			overlap_left = base_info->slice_overlap_array
				[cur_slice_id].overlap_left;
			overlap_up = base_info->slice_overlap_array
				[cur_slice_id].overlap_up;
			overlap_right = base_info->slice_overlap_array
				[cur_slice_id].overlap_right;
			overlap_down = base_info->slice_overlap_array
				[cur_slice_id].overlap_down;
			start_row_out = start_row + overlap_up;
			start_col_out = start_col + overlap_left;
			store_info[cur_slice_id].size.height =
				end_row-start_row+1-overlap_up-overlap_down;
			store_info[cur_slice_id].size.width =
				end_col-start_col+1-overlap_left-overlap_right;
			store_info[cur_slice_id].border.left_border =
				overlap_left;
			store_info[cur_slice_id].border.right_border =
				overlap_right;
			store_info[cur_slice_id].border.up_border =
				overlap_up;
			store_info[cur_slice_id].border.down_border =
				overlap_down;

			isp_slice_store_addr_init(&store_pitch,
				store_info, store_frame,
				start_row_out,
				start_col_out,
				cur_slice_id);
		}
	}

exit:
	return rtn;
}

static int set_slice_store_info(struct slice_param_in *in_ptr,
				struct slice_context_info *cxt)
{
	int rtn = 0;
	uint32_t scl_bypass;
	struct slice_base_info *base_info = NULL;
	struct slice_store_info *store_info = NULL;
	struct slice_store_path *store_frame = NULL;
	struct slice_scaler_info *scaler_info = NULL;

	if (!in_ptr || !cxt) {
		pr_err("fail to get valid param, NULL\n");
		rtn = -1;
		goto exit;
	}

	base_info = &cxt->base_info;
	if (in_ptr->pre_slice_need == 1) {
		store_info = cxt->store_info[SLICE_PATH_PRE];
		scaler_info = cxt->scaler_info[SLICE_PATH_PRE];
		store_frame = &in_ptr->store_frame[SLICE_PATH_PRE];
		scl_bypass = in_ptr->scaler_frame[SLICE_PATH_PRE].scaler_bypass;
		set_store_info(store_info, store_frame, base_info, scaler_info,
			       scl_bypass);
		/* rtn init to 0, and not changed
		 * if (rtn)
		 *	goto exit;
		 */
	}

	if (in_ptr->vid_slice_need == 1) {
		store_info = cxt->store_info[SLICE_PATH_VID];
		scaler_info = cxt->scaler_info[SLICE_PATH_VID];
		store_frame = &in_ptr->store_frame[SLICE_PATH_VID];
		scl_bypass = in_ptr->scaler_frame[SLICE_PATH_VID].scaler_bypass;
		set_store_info(store_info, store_frame, base_info, scaler_info,
			       scl_bypass);
		/* rtn init to 0, and not changed
		 * if (rtn)
		 *	goto exit;
		 */
	}

	if (in_ptr->cap_slice_need == 1) {
		store_info = cxt->store_info[SLICE_PATH_CAP];
		scaler_info = cxt->scaler_info[SLICE_PATH_CAP];
		store_frame = &in_ptr->store_frame[SLICE_PATH_CAP];
		scl_bypass = in_ptr->scaler_frame[SLICE_PATH_CAP].scaler_bypass;
		set_store_info(store_info, store_frame, base_info, scaler_info,
			       scl_bypass);
		/* rtn init to 0, and not changed
		 * if (rtn)
		 *	goto exit;
		 */
	}

exit:
	return rtn;
}

static int set_slice_cfa_info(struct slice_param_in *in_ptr,
			      struct slice_context_info *cxt)
{
	int rtn = 0;
	uint32_t cur_slice_id, slice_num;
	struct slice_base_info *base_info = NULL;
	struct slice_cfa_info *cfa_info = NULL;
	uint32_t start_col = 0;
	uint32_t end_col = 0;

	if (!cxt) {
		pr_err("fail to get valid param, NULL\n");
		rtn = -1;
		goto exit;
	}

	base_info = &cxt->base_info;
	cur_slice_id = base_info->cur_slice_id;
	slice_num = base_info->slice_num;

	for (; cur_slice_id < slice_num; cur_slice_id++) {
		cfa_info = &cxt->cfa_info[cur_slice_id];
		start_col = base_info->slice_pos_array[cur_slice_id].start_col;
		end_col = base_info->slice_pos_array[cur_slice_id].end_col;
		cfa_info->gbuf_addr_max =
			DIV_ROUND_UP((end_col - start_col), 2) + 1;
	}

exit:
	return rtn;
}

static int set_fmcu_clr_int(uint32_t *fmcu_buf, uint32_t num,
			    enum isp_scene_id sid, enum isp_id iid)
{
	uint32_t addr = 0, cmd = 0;
	uint32_t int_base = int_reg_base[iid][sid];

	/* clear interrupt */
	addr = ISP_GET_REG(iid, int_base + ISP_INT_CLR0);
	cmd = 0xFFFFFFFFUL;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, int_base + ISP_INT_CLR1);
	cmd = 0xFFFFFFFFUL;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, int_base + ISP_INT_CLR2);
	cmd = 0xFFFFFFFFUL;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, int_base + ISP_INT_CLR3);
	cmd = 0xFFFFFFFFUL;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int set_fmcu_cfg(uint32_t *fmcu_buf,
			uint32_t num,
			enum isp_scene_id sid,
			enum isp_id iid)
{
	uint32_t addr = 0, cmd = 0;
	uint32_t cfg_start_addr[ISP_ID_MAX][ISP_SCENE_NUM] = {
		{
			ISP_CFG_PRE0_START,
			ISP_CFG_CAP0_START
		},
		{
			ISP_CFG_PRE1_START,
			ISP_CFG_CAP1_START
		}
	};

	addr = ISP_GET_REG(iid, cfg_start_addr[iid][sid]);
	cmd = 1;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	/*
	 * When setting CFG_TRIGGER_PULSE cmd, fmcu will wait
	 * until CFG module configs isp registers done.
	 */
	addr = ISP_GET_REG(iid, ISP_FMCU_CMD);
	cmd = CFG_TRIGGER_PULSE;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int set_fmcu_lsc_2d(uint32_t *fmcu_buf,
			   struct slice_lsc_2d_info *lsc_2d_info,
			   uint32_t num, enum isp_work_mode mid,
			   enum isp_id iid)
{
	uint32_t addr = 0, cmd = 0;
	uint32_t i = 0;

	addr = ISP_GET_REG(iid, ISP_LENS_SLICE_POS);
	cmd = ((lsc_2d_info->start_row & 0xFFFF) << 16) |
		(lsc_2d_info->start_col & 0xFFFF);
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, ISP_LENS_GRID_SIZE);
	cmd = ((lsc_2d_info->grid_num_t & 0xFFFF) << 16) |
		((lsc_2d_info->grid_y_num & 0xFF) << 8) |
		(lsc_2d_info->grid_x_num & 0xFF);
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, ISP_LENS_SLICE_SIZE);
	cmd = ((lsc_2d_info->slice_height & 0xFFFF) << 16) |
		(lsc_2d_info->slice_width & 0xFFFF);
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, ISP_LENS_INIT_COOR);
	cmd = ((lsc_2d_info->relative_y & 0x3FF) << 16) |
		(lsc_2d_info->relative_x & 0x3FF);
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	for (i = 0; i < 5; i++) {
		addr = ISP_GET_REG(iid, ISP_LENS_Q0_VALUE + i * 4);
		cmd = ((lsc_2d_info->q_val[i][0] & 0x3FFF) << 16) |
			(lsc_2d_info->q_val[i][1] & 0x3FFF);
		num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	}

	if (mid == ISP_CFG_MODE) {
		for (i = 0; i < lsc_2d_info->grid_num_t * 4; i += 2) {
			addr = ISP_GET_REG(iid, ISP_LEN_BUF0_CH0 + i * 2);
			cmd = lsc_2d_info->grid_buf[i+1] << 16 |
				lsc_2d_info->grid_buf[i];
			num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
		}
		kfree(lsc_2d_info->grid_buf);
		lsc_2d_info->grid_buf = NULL;
	} else if (mid == ISP_AP_MODE)
		pr_info("not support yet!\n");
	else
		pr_err("fail to get invalid mid:%d\n", mid);

	return num;
}

static int set_fmcu_fetch(uint32_t *fmcu_buf,
			  struct slice_fetch_info *fetch_info,
			  uint32_t num, enum isp_id iid)
{
	uint32_t addr = 0, cmd = 0;

	addr = ISP_GET_REG(iid, ISP_FETCH_MEM_SLICE_SIZE);
	cmd = ((fetch_info->size.height & 0xFFFF) << 16) |
		(fetch_info->size.width & 0xFFFF);
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, ISP_FETCH_SLICE_Y_ADDR);
	cmd = fetch_info->addr.chn0;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, ISP_FETCH_SLICE_U_ADDR);
	cmd = fetch_info->addr.chn1;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, ISP_FETCH_SLICE_V_ADDR);
	cmd = fetch_info->addr.chn2;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, ISP_FETCH_MIPI_PARAM);
	cmd = ((fetch_info->mipi_byte_rel_pos & 0xF) << 16) |
		(fetch_info->mipi_word_num & 0xFFFF);
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int set_fmcu_dispatch(uint32_t *fmcu_buf,
			     struct slice_dispatch_info *dispatch_info,
			     uint32_t num, enum isp_id iid)
{
	uint32_t addr = 0, cmd = 0;

	/*
	 * Once the slice width/height 2-align be ensured,
	 * dispatch bayer mode won't be affect during slicing.
	 * Improper configure bayer mode could cause color defect.
	 */

#if 0
	addr = ISP_GET_REG(iid, ISP_DISPATCH_CH0_BAYER);
	cmd = dispatch_info->bayer_mode & 0x3;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
#endif

	addr = ISP_GET_REG(iid, ISP_DISPATCH_CH0_SIZE);
	cmd = ((dispatch_info->size.height & 0xFFFF) << 16) |
		(dispatch_info->size.width & 0xFFFF);
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int set_fmcu_postcnr(uint32_t *fmcu_buf,
			    struct slice_postcnr_info *postcnr_info,
			    uint32_t num, enum isp_id iid)
{
	uint32_t addr = 0, cmd = 0;

	addr = ISP_GET_REG(iid, ISP_POSTCDN_START_ROW_MOD4);
	cmd = postcnr_info->start_row_mod4 & 0x3;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int set_fmcu_ynr(uint32_t *fmcu_buf, struct slice_ynr_info *ynr_info,
			uint32_t num, enum isp_id iid)
{
	uint32_t addr = 0, cmd = 0;

	addr = ISP_GET_REG(iid, ISP_YNR_CFG11);
	cmd = ((ynr_info->start_col & 0xFFFF) << 16) |
		(ynr_info->start_row & 0xFFFF);
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int set_fmcu_noisefilter(uint32_t *fmcu_buf,
				struct slice_noisefilter_info *noisefilter_info,
				uint32_t num, enum isp_id iid)
{
	uint32_t addr = 0, cmd = 0;

	addr = ISP_GET_REG(iid, ISP_YUV_NF_SEED0);
	cmd = noisefilter_info->seed0 & 0xFFFFFF;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, ISP_YUV_NF_SEED1);
	cmd = noisefilter_info->seed1 & 0xFFFFFF;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, ISP_YUV_NF_SEED2);
	cmd = noisefilter_info->seed2 & 0xFFFFFF;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, ISP_YUV_NF_SEED3);
	cmd = noisefilter_info->seed3 & 0xFFFFFF;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, ISP_YUV_NF_SEED_INIT);
	cmd = noisefilter_info->seed_int;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int set_fmcu_scaler(uint32_t *fmcu_buf,
			   struct slice_scaler_info *scaler_info,
			   uint32_t num, enum isp_id iid,
			   uint32_t base, uint32_t scl_bypass)
{
	uint32_t addr = 0, cmd = 0;

	if (!scl_bypass) {
		addr = ISP_GET_REG(iid, ISP_SCALER_SRC_SIZE) + base;
		cmd = (scaler_info->src_size_x & 0x3FFF) |
			((scaler_info->src_size_y & 0x3FFF) << 16);
		num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

		addr = ISP_GET_REG(iid, ISP_SCALER_DES_SIZE) + base;
		cmd = (scaler_info->dst_size_x & 0x3FFF) |
			((scaler_info->dst_size_y & 0x3FFF) << 16);
		num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

		addr = ISP_GET_REG(iid, ISP_SCALER_TRIM0_START) + base;
		cmd = (scaler_info->trim0_start_x & 0x1FFF) |
			((scaler_info->trim0_start_y & 0x1FFF) << 16);
		num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

		/* trim0 size must <= src_size */
		if (scaler_info->trim0_size_x > scaler_info->src_size_x ||
			scaler_info->trim0_size_y > scaler_info->src_size_y) {
			pr_warn("trim0 size > src size, used src size\n");
			scaler_info->trim0_size_x = scaler_info->src_size_x;
			scaler_info->trim0_size_y = scaler_info->src_size_y;
		}
		addr = ISP_GET_REG(iid, ISP_SCALER_TRIM0_SIZE) + base;
		cmd = (scaler_info->trim0_size_x & 0x1FFF) |
			((scaler_info->trim0_size_y & 0x1FFF) << 16);
		num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

		addr = ISP_GET_REG(iid, ISP_SCALER_IP) + base;
		cmd = (scaler_info->scaler_ip_rmd & 0x1FFF) |
			((scaler_info->scaler_ip_int & 0xF) << 16);
		num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

		addr = ISP_GET_REG(iid, ISP_SCALER_CIP) + base;
		cmd = (scaler_info->scaler_cip_rmd & 0x1FFF) |
			((scaler_info->scaler_cip_int & 0xF) << 16);
		num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

		addr = ISP_GET_REG(iid, ISP_SCALER_FACTOR) + base;
		cmd = (scaler_info->scaler_factor_out & 0x1FFF) |
			((scaler_info->scaler_factor_in & 0x1FFF) << 16);
		num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

		addr = ISP_GET_REG(iid, ISP_SCALER_TRIM1_START) + base;
		cmd = (scaler_info->trim1_start_x & 0x1FFF) |
			((scaler_info->trim1_start_y & 0x1FFF) << 16);
		num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

		addr = ISP_GET_REG(iid, ISP_SCALER_TRIM1_SIZE) + base;
		cmd = (scaler_info->trim1_size_x & 0x1FFF) |
			((scaler_info->trim1_size_y & 0x1FFF) << 16);
		num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

		addr = ISP_GET_REG(iid, ISP_SCALER_VER_IP) + base;
		cmd = (scaler_info->scaler_ip_rmd_ver & 0x1FFF) |
			((scaler_info->scaler_ip_int_ver & 0xF) << 16);
		num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

		addr = ISP_GET_REG(iid, ISP_SCALER_VER_CIP) + base;
		cmd = (scaler_info->scaler_cip_rmd_ver & 0x1FFF) |
			((scaler_info->scaler_cip_int_ver & 0xF) << 16);
		num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

		addr = ISP_GET_REG(iid, ISP_SCALER_VER_FACTOR) + base;
		cmd = (scaler_info->scaler_factor_out_ver & 0x3FFF) |
			((scaler_info->scaler_factor_in_ver & 0x3FFF) << 16);
		num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	} else {
		/* bypass scaler*/
		addr = ISP_GET_REG(iid, ISP_SCALER_TRIM1_SIZE) + base;
		cmd = (scaler_info->trim1_size_x & 0x1FFF) |
			((scaler_info->trim1_size_y & 0x1FFF) << 16);
		num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);
	}

	return num;
}

static int set_fmcu_store(uint32_t *fmcu_buf,
			  struct slice_store_info *store_info,
			  uint32_t num, enum isp_id iid, uint32_t base)
{
	uint32_t addr = 0, cmd = 0;

	addr = ISP_GET_REG(iid, ISP_STORE_SLICE_SIZE) + base;
	cmd = ((store_info->size.height & 0xFFFF) << 16) |
		(store_info->size.width & 0xFFFF);
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, ISP_STORE_BORDER) + base;
	cmd = (store_info->border.up_border & 0xFF) |
		((store_info->border.down_border & 0xFF) << 8)
		| ((store_info->border.left_border & 0xFF) << 16) |
		((store_info->border.right_border & 0xFF) << 24);
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, ISP_STORE_SLICE_Y_ADDR) + base;
	cmd = store_info->addr.chn0;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, ISP_STORE_SLICE_U_ADDR) + base;
	cmd = store_info->addr.chn1;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, ISP_STORE_SLICE_V_ADDR) + base;
	cmd = store_info->addr.chn2;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	addr = ISP_GET_REG(iid, ISP_STORE_SHADOW_CLR) + base;
	cmd = 1;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int set_fmcu_cfa(uint32_t *fmcu_buf,
			struct slice_cfa_info *cfa_info,
			uint32_t num, enum isp_id iid)
{
	uint32_t addr = 0, cmd = 0;

	addr = ISP_GET_REG(iid, ISP_CFAE_GBUF_CFG);
	cmd = cfa_info->gbuf_addr_max & 0xfff;
	num = fmcu_push_back(&fmcu_buf[num], addr, cmd, num);

	return num;
}

static int set_slice_fmcu_info(struct slice_param_in *in_ptr,
			       struct slice_context_info *cxt,
			       uint32_t *fmcu_num)
{
	int rtn = 0;
	struct isp_pipe_dev *dev = NULL;
	struct isp_k_block *isp_k_bp = NULL;
	struct slice_lsc_2d_info *lsc_2d_info = NULL;
	struct slice_fetch_info *fetch_info = NULL;
	struct slice_dispatch_info *dispatch_info = NULL;
	struct slice_postcnr_info *postcnr_info = NULL;
	struct slice_ynr_info *ynr_info = NULL;
	struct slice_noisefilter_info *noisefilter_info = NULL;
	struct slice_cfa_info *cfa_info = NULL;
	struct slice_base_info *base_info = NULL;
	struct slice_scaler_info *scaler_info = NULL;
	struct slice_store_info *store_info = NULL;
	uint32_t shadow_done_cmd[ISP_ID_MAX][ISP_SCENE_NUM] = {
		{P0_SDW_DONE, C0_SDW_DONE},
		{P1_SDW_DONE, C1_SDW_DONE}
	};

	uint32_t all_done_cmd[ISP_ID_MAX][ISP_SCENE_NUM] = {
		{P0_ALL_DONE, C0_ALL_DONE},
		{P1_ALL_DONE, C1_ALL_DONE}
	};

	uint32_t num = 0, addr = 0, cmd = 0;
	uint32_t slice_id = 0;
	uint32_t *fmcu_buf;
	enum isp_id iid = 0;
	enum isp_scene_id sid = 0;
	enum isp_work_mode mid = 0;
	uint32_t scl_base, store_base;
	uint32_t scl_bypass;

	if (!in_ptr || !cxt) {
		pr_err("fail to get valid param, NULL\n");
		rtn = -1;
		goto exit;
	}

	base_info = &cxt->base_info;
	fmcu_buf = in_ptr->fmcu_addr_vir;
	iid = in_ptr->iid;
	sid = in_ptr->sid;
	mid = in_ptr->mid;

	if (!CHECK_ID_VALID(iid) ||
	    !CHECK_ID_VALID(sid) ||
	    !CHECK_ID_VALID(mid)) {
		pr_err("fail to get invalid id: iid%d, sid%d, mid%d\n",
		       iid, sid, mid);
		rtn = -1;
		goto exit;
	}

	if (!fmcu_buf) {
		pr_err("fail to get fmcu_addr_vir erro\n");
		rtn = -1;
		goto exit;
	}

	if (in_ptr->isp_dev) {
		dev = in_ptr->isp_dev;
		isp_k_bp = &dev->isp_k_param;
	} else {
		pr_err("fail to get isp dev null\n");
		rtn = -1;
		goto exit;
	}

	pr_debug("sid:%d, mid:%d, iid:%d\n", sid, mid, iid);

	for (slice_id = 0; slice_id < base_info->slice_num; slice_id++) {
		lsc_2d_info = &cxt->lsc_2d_info[slice_id];
		fetch_info = &cxt->fetch_info[slice_id];
		dispatch_info = &cxt->dispatch_info[slice_id];
		postcnr_info = &cxt->postcnr_info[slice_id];
		ynr_info = &cxt->ynr_info[slice_id];
		noisefilter_info = &cxt->noisefilter_info[slice_id];
		cfa_info = &cxt->cfa_info[slice_id];

		/*
		 * Set common scaler path select while initialization.
		 * don't need to push into fmcu cmd queue.
		 */
		num = set_fmcu_clr_int(fmcu_buf, num, sid, iid);
		if (mid == ISP_CFG_MODE)
			num = set_fmcu_cfg(fmcu_buf, num, sid, iid);

		if (dev->is_3dnr) {
			pr_debug("3dnr set fmcu cmd: open vid path\n");
			/* open vid path */
			addr = ISP_GET_REG(iid, ISP_COMMON_SCL_PATH_SEL);
			cmd = ISP_HREG_RD(iid, ISP_COMMON_SCL_PATH_SEL) &
				~(BIT_3 | BIT_2);
			num = fmcu_push_back(&fmcu_buf[num & (~0x1)], addr,
					     cmd, num);
		}

		if (!isp_k_bp->lsc_bypass)
			num = set_fmcu_lsc_2d(fmcu_buf, lsc_2d_info, num,
					      mid, iid);
		num = set_fmcu_fetch(fmcu_buf, fetch_info, num, iid);
		num = set_fmcu_dispatch(fmcu_buf, dispatch_info, num, iid);
		num = set_fmcu_postcnr(fmcu_buf, postcnr_info, num, iid);
		num = set_fmcu_ynr(fmcu_buf, ynr_info, num, iid);
		num = set_fmcu_noisefilter(fmcu_buf,
			noisefilter_info, num, iid);
		num = set_fmcu_cfa(fmcu_buf, cfa_info, num, iid);

		if (in_ptr->pre_slice_need == 1) {
			pr_debug("set pre fmcu param\n");
			scaler_info = cxt->scaler_info[SLICE_PATH_PRE];
			store_info = cxt->store_info[SLICE_PATH_PRE];
			scl_base = ISP_SCALER_PRE_CAP_BASE;
			store_base = ISP_STORE_PRE_CAP_BASE;
			scl_bypass = in_ptr->
				scaler_frame[SLICE_PATH_PRE].scaler_bypass;
			num = set_fmcu_scaler(fmcu_buf, &scaler_info[slice_id],
				num, iid, scl_base, scl_bypass);

			/*
			 * Already set store shadow clear select while
			 * initialization, no need to push into fmcu cmd queue
			 */
			num = set_fmcu_store(fmcu_buf,
				&store_info[slice_id], num, iid, store_base);
		}

		if (in_ptr->vid_slice_need == 1) {
			pr_debug("set video fmcu param\n");
			scaler_info = cxt->scaler_info[SLICE_PATH_VID];
			store_info = cxt->store_info[SLICE_PATH_VID];
			scl_base = ISP_SCALER_VID_BASE;
			store_base = ISP_STORE_VID_BASE;
			scl_bypass = in_ptr->
				scaler_frame[SLICE_PATH_VID].scaler_bypass;
			num = set_fmcu_scaler(fmcu_buf, &scaler_info[slice_id],
				num, iid, scl_base, scl_bypass);

			/*
			 * Already set store shadow clear select while
			 * initialization, no need to push into fmcu cmd queue
			 */
			num = set_fmcu_store(fmcu_buf,
				&store_info[slice_id], num, iid, store_base);
		}

		if (in_ptr->cap_slice_need == 1) {
			pr_debug("set cap fmcu param\n");
			scaler_info = cxt->scaler_info[SLICE_PATH_CAP];
			store_info = cxt->store_info[SLICE_PATH_CAP];
			scl_base = ISP_SCALER_PRE_CAP_BASE;
			store_base = ISP_STORE_PRE_CAP_BASE;
			scl_bypass = in_ptr->
				scaler_frame[SLICE_PATH_CAP].scaler_bypass;
			num = set_fmcu_scaler(fmcu_buf, &scaler_info[slice_id],
				num, iid, scl_base, scl_bypass);

			/*
			 * Already set store shadow clear select while
			 * initialization, no need to push into fmcu cmd queue
			 */
			num = set_fmcu_store(fmcu_buf,
				&store_info[slice_id], num, iid, store_base);
		}

		/*
		 * in CFG mode, FMCU send a signal to cfg or isp core,
		 * depending on the isp work mode.
		 *
		 * @CFG mode: FMCU will set fmcu cmd ready to CFG module,
		 * after configuring the current slice's registers done.
		 * If CFG module get this signal, CFG will start isp pipeline.
		 * Before this operation, cap0/1_cmd_ready_mode should be 1.
		 *
		 * @AP mode: FMCU will start isp pipeline directly.
		 *
		 */
		if (mid == ISP_CFG_MODE)
			addr = ISP_GET_REG(iid, ISP_CFG_CAP_FMCU_RDY);
		else
			addr = ISP_GET_REG(iid, ISP_FETCH_START);
		cmd = 1;
		num = fmcu_push_back(&fmcu_buf[num & (~0x1)], addr, cmd, num);

		addr = ISP_GET_REG(iid, ISP_FMCU_CMD);
		cmd = shadow_done_cmd[iid][sid];
		num = fmcu_push_back(&fmcu_buf[num & (~0x1)], addr,
			cmd, num);
		addr = ISP_GET_REG(iid, ISP_FMCU_CMD);
		cmd = all_done_cmd[iid][sid];
		num = fmcu_push_back(&fmcu_buf[num & (~0x1)], addr,
			cmd, num);

		if (dev->is_3dnr) {
			pr_debug("3dnr set fmcu cmd: close vid path\n");
			/* close vid path */
			addr = ISP_GET_REG(iid, ISP_COMMON_SCL_PATH_SEL);
			cmd = (ISP_HREG_RD(iid, ISP_COMMON_SCL_PATH_SEL) &
			       ~(BIT_3 | BIT_2)) | (BIT_3 | BIT_2);
			num = fmcu_push_back(&fmcu_buf[num & (~0x1)], addr,
					     cmd, num);
		}
	}
	*fmcu_num = num / 2;
exit:
	return rtn;
}

int isp_fmcu_slice_cfg(void *fmcu_handler, struct slice_param_in *in_ptr,
		       uint32_t *fmcu_num)
{
	int rtn = 0;
	struct slice_context_info *cxt = NULL;

	if (!fmcu_handler || !in_ptr || !fmcu_num) {
		pr_err("fail to get input handle is NULL!\n");
		rtn = -1;
		goto exit;
	}
	cxt = (struct slice_context_info *)fmcu_handler;

	rtn = set_slice_base_info(in_ptr, &cxt->base_info);
	if (rtn) {
		pr_err("fail to set slice base info!\n");
		goto exit;
	}

	if (in_ptr->fetch_format == ISP_FETCH_CSI2_RAW_10 ||
		in_ptr->fetch_format == ISP_FETCH_RAW_10) {
		rtn = set_slice_lsc_2d_info(in_ptr, cxt);
		if (rtn) {
			pr_err("fail to set slice lsc_2d info!\n");
			goto exit;
		}
	} else
		pr_debug("skip LSC for YUV format %d\n", in_ptr->fetch_format);

	rtn = set_slice_fetch_info(in_ptr, cxt);
	if (rtn) {
		pr_err("fail to set slice fetch info!\n");
		goto exit;
	}

	rtn = set_slice_dispatch_info(in_ptr, cxt);
	if (rtn) {
		pr_err("fail to set slice dispatch info!\n");
		goto exit;
	}

	rtn = set_slice_postcnr_info(cxt);
	if (rtn) {
		pr_err("fail to set slice postcnr info!\n");
		goto exit;
	}

	rtn = set_slice_ynr_info(cxt);
	if (rtn) {
		pr_err("fail to set slice ynr info!\n");
		goto exit;
	}

	rtn = set_slice_scaler_info(in_ptr, cxt);
	if (rtn) {
		pr_err("fail to set slice scaler info!\n");
		goto exit;
	}

	rtn = set_slice_noisefliter_info(cxt);
	if (rtn) {
		pr_err("fail to set slice noise fliter info!\n");
		goto exit;
	}

	rtn = set_slice_store_info(in_ptr, cxt);
	if (rtn) {
		pr_err("fail to set slice store info!\n");
		goto exit;
	}

	rtn = set_slice_cfa_info(in_ptr, cxt);
	if (rtn) {
		pr_err("fail to set slice cfa` info!\n");
		goto exit;
	}

	rtn = set_slice_fmcu_info(in_ptr, cxt, fmcu_num);
	if (rtn) {
		pr_err("fail to set slice fmcu info!\n");
		goto exit;
	}

	return 0;
exit:
	free_lsc_2d_slice_grid_buf(cxt);
	return rtn;
}

int isp_fmcu_slice_init(void **fmcu_handler)
{
	int rtn = 0;
	struct slice_context_info *cxt = NULL;

	if (!fmcu_handler) {
		pr_err("fail to get fmcu_handle is NULL!\n");
		rtn = -1;
		goto exit;
	}
	*fmcu_handler = NULL;

	cxt = vzalloc(sizeof(struct slice_context_info));
	*fmcu_handler = (void *)cxt;
exit:
	return rtn;
}

int isp_fmcu_slice_deinit(void *fmcu_handler)
{
	int rtn = 0;
	struct slice_context_info *cxt = NULL;

	if (!fmcu_handler) {
		pr_err("fail to get fmcu_handle is NULL!\n");
		return -1;
	}
	cxt = (struct slice_context_info *)fmcu_handler;

	if (cxt != NULL)
		vfree(cxt);

	return rtn;
}
