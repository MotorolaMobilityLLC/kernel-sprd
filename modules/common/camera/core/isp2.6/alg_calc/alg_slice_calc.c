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

#include "alg_slice_calc.h"
#include "isp_dewarping.h"
#include "alg_common_calc.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ALG_SLICE_CALC: %d %d %s : "fmt, current->pid, __LINE__, __func__

#define FBC_SUPPORT                   0
#define AFBC_PADDING_W_YUV420_3dnr    32
#define AFBC_PADDING_H_YUV420_3dnr    8
#define YUV422                        0
#define YUV420                        1

static uint8_t SCALER_YUV_DECI_MAP[]={2,4,8,16};
static uint8_t SCALER_YUV_DECI_OFFSET_MAP[]={0,1,3,7};

#define CAL_OVERLAP(ctrl_bypass, module_param) \
		do { \
			if (!(ctrl_bypass)) { \
				ov_pipe.ov_left += module_param.left; \
				ov_pipe.ov_right += module_param.right; \
				ov_pipe.ov_up += module_param.up; \
				ov_pipe.ov_down += module_param.down; \
			} \
		} while (0)

#define CAL_REC_OVERLAP(ctrl_bypass,module_param) \
		do { \
			if (!(ctrl_bypass)) { \
				overlap_rec.ov_left  += module_param.left; \
				overlap_rec.ov_right += module_param.right; \
				overlap_rec.ov_up    += module_param.up; \
				overlap_rec.ov_down  += module_param.down; \
			} \
		} while (0)

struct alg_scaler_ovlap_temp {
	int image_w;
	int image_h;
	struct alg_slice_drv_overlap *param_ptr;
	struct alg_slice_regions *maxRegion;
	struct alg_slice_regions *orgRegion;
};

struct alg_pyramid_ovlap_temp {
	int layer_id;
	int layer_num;
	int slice_num;
	int layer0_padding_width;
	int layer0_padding_height;
	struct alg_overlap_info *overlap_rec;
	struct alg_overlap_info *ov_pipe_layer0;
	struct alg_overlap_info *overlap_rec_mode1;
	struct alg_slice_drv_overlap *param_ptr;
	struct alg_slice_regions add_rec_slice_out[MAX_PYR_DEC_LAYER_NUM];
};

static int check_image_resolution(uint32_t length, uint32_t layer_num)
{
	uint32_t outData = 0;
	uint32_t multiple = 1 << layer_num;

	outData = length % multiple;
	if (outData != 0)
		outData = multiple - outData;

	return outData;
}

static void core_drv_dnr_init_block(struct alg_block_calc *block_ptr)
{
	block_ptr->left = 4;
	block_ptr->right = 4;
	block_ptr->up = 4;
	block_ptr->down = 4;
}

static void core_drv_pyd_dec_offline_init_block(struct alg_block_calc *block_ptr)
{
	block_ptr->left = DEC_OVERLAP;
	block_ptr->right = 0;
	block_ptr->up = DEC_OVERLAP;
	block_ptr->down = 0;
}

void isp_drv_region_w_align(struct alg_region_info *r,
		uint32_t align_v, uint32_t min_v, uint32_t max_v)
{
	uint32_t slice_w_org, slice_w_dst, offset_v, sx, ex;

	if (align_v <= 0)
		return;

	slice_w_org = r->ex - r->sx + 1;
	if (slice_w_org % align_v != 0) {
		slice_w_dst = (slice_w_org + (align_v - 1)) / align_v * align_v;
		offset_v = slice_w_dst - slice_w_org;
		sx = r->sx - offset_v;
		if (sx >= min_v) {
			r->sx = sx;
		} else {
			ex = r->ex + offset_v;
			if (ex <= max_v)
				r->ex = ex;
		}
	}
}

uint32_t isp_drv_regions_fetch_ref(const struct alg_fetch_region *fetch_param,
	const struct alg_slice_regions *r_ref, struct alg_slice_regions *r_out)
{
	uint32_t imgW = fetch_param->image_w;
	uint32_t imgH = fetch_param->image_h;
	uint32_t overlapUp = fetch_param->overlap_up;
	uint32_t overlapDown = fetch_param->overlap_down;
	uint32_t overlapLeft = fetch_param->overlap_left;
	uint32_t overlapRight= fetch_param->overlap_right;
	uint32_t row_num = r_ref->rows;
	uint32_t col_num = r_ref->cols;
	uint32_t i ,j, index;
	struct alg_fetch_region_context context;

	r_out->rows = row_num;
	r_out->cols = col_num;
	for (i = 0;i < row_num; i++) {
		for (j = 0; j < col_num; j++) {
			index = i * col_num + j;
			context.s_row = r_ref->regions[index].sy;
			context.s_col = r_ref->regions[index].sx;
			context.e_row = r_ref->regions[index].ey;
			context.e_col = r_ref->regions[index].ex;
			context.overlap_left = overlapLeft;
			context.overlap_right = overlapRight;
			context.overlap_up = overlapUp;
			context.overlap_down = overlapDown;

			/* l-top */
			if ((0 == i) && (0 == j)) {
				context.overlap_left = 0;
				context.overlap_up = 0;
				context.s_row = 0;
				context.s_col = 0;
			}
			/* r-top */
			if ((0 == i) && (col_num - 1 == j)) {
				context.overlap_right = 0;
				context.overlap_up = 0;
				context.s_row = 0;
				context.e_col = (imgW - 1);
			}
			/* l-bottom */
			if ((row_num-1 == i) && (0 == j)) {
				context.overlap_left = 0;
				context.overlap_down = 0;
				context.s_col = 0;
				context.e_row = (imgH - 1);
			}
			/* r-bottom */
			if ((row_num - 1 == i) && (col_num - 1 == j)) {
				context.overlap_right = 0;
				context.overlap_down = 0;
				context.e_row = (imgH - 1);
				context.e_col = (imgW - 1);
			}
			/* up */
			if ((0 == i) && (0 < j && j < col_num - 1)) {
				context.overlap_up = 0;
				context.s_row = 0;
			}
			/* down */
			if ((row_num - 1 == i) && (0 < j && j < col_num-1)) {
				context.overlap_down = 0;
				context.e_row = (imgH - 1);
			}
			/* left */
			if ((0 == j) && (0 < i && i < row_num - 1)) {
				context.overlap_left = 0;
				context.s_col = 0;
			}
			/* right */
			if ((col_num - 1 == j) && (0 < i && i < row_num - 1)) {
				context.overlap_right = 0;
				context.e_col = (imgW - 1);
			}

			context.s_row -= context.overlap_up;
			context.e_row += context.overlap_down;
			context.s_col -= context.overlap_left;
			context.e_col += context.overlap_right;

			/* add overlap overflow return -1 */
			if (context.s_col < 0 || context.s_row < 0 ||
				context.e_col >= imgW || context.e_row >= imgH) {
				pr_err("context s_col %d s_row %d e_col %d e_row %d\n",
					context.s_col, context.s_row, context.e_col, context.e_row);
				return -1;
			}

			r_out->regions[index].sx = context.s_col;
			r_out->regions[index].ex = context.e_col;
			r_out->regions[index].sy = context.s_row;
			r_out->regions[index].ey = context.e_row;
		}
	}

	return 0;
}

uint32_t isp_drv_regions_alignfetch_ref(const struct alg_fetch_region *fetch_param,
	const struct alg_slice_regions *r_ref, struct alg_overlap_info *ov, struct alg_slice_regions *r_out)
{
	uint32_t imgW = fetch_param->image_w;
	uint32_t imgH = fetch_param->image_h;
	uint32_t overlapUp = fetch_param->overlap_up;
	uint32_t overlapDown = fetch_param->overlap_down;
	uint32_t overlapLeft = ov->ov_left;
	uint32_t overlapRight= ov->ov_right;
	uint32_t row_num = r_ref->rows;
	uint32_t col_num = r_ref->cols;
	uint32_t i ,j, index;
	struct alg_fetch_region_context context;

	r_out->rows = row_num;
	r_out->cols = col_num;
	for (i = 0;i < row_num; i++) {
		for (j = 0; j < col_num; j++) {
			index = i * col_num + j;
			context.s_row = r_ref->regions[index].sy;
			context.s_col = r_ref->regions[index].sx;
			context.e_row = r_ref->regions[index].ey;
			context.e_col = r_ref->regions[index].ex;
			context.overlap_left = overlapLeft;
			context.overlap_right = overlapRight;
			context.overlap_up = overlapUp;
			context.overlap_down = overlapDown;

			/* l-top */
			if ((0 == i) && (0 == j)) {
				context.overlap_left = 0;
				context.overlap_up = 0;
				context.s_row = 0;
				context.s_col = 0;
			}
			/* r-top */
			if ((0 == i) && (col_num - 1 == j)) {
				context.overlap_right = 0;
				context.overlap_up = 0;
				context.s_row = 0;
				context.e_col = (imgW - 1);
			}
			/* l-bottom */
			if ((row_num - 1 == i) && (0 == j)) {
				context.overlap_left = 0;
				context.overlap_down = 0;
				context.s_col= 0;
				context.e_row= (imgH - 1);
			}
			/* r-bottom */
			if ((row_num - 1 == i) && (col_num - 1 == j)) {
				context.overlap_right= 0;
				context.overlap_down = 0;
				context.e_row = (imgH - 1);
				context.e_col = (imgW - 1);
			}
			/* up */
			if ((0 == i) && (0<j && j < col_num - 1)) {
				context.overlap_up = 0;
				context.s_row = 0;
			}
			/* down */
			if ((row_num - 1 == i) && (0 < j && j < col_num-1)) {
				context.overlap_down = 0;
				context.e_row = (imgH - 1);
			}
			/* left */
			if ((0 == j) && (0 < i && i < row_num - 1)) {
				context.overlap_left = 0;
				context.s_col = 0;
			}
			/* right */
			if ((col_num - 1 == j) && (0 < i && i < row_num - 1)) {
				context.overlap_right = 0;
				context.e_col = (imgW - 1);
			}

			context.s_row -= context.overlap_up;
			context.e_row += context.overlap_down;
			context.s_col -= context.overlap_left;
			context.e_col += context.overlap_right;

			/* add overlap overflow return -1 */
			if (context.s_col < 0 || context.s_row < 0 ||
				context.e_col >= imgW || context.e_row >= imgH)
				return -1;

			r_out->regions[index].sx = context.s_col;
			r_out->regions[index].ex = context.e_col;
			r_out->regions[index].sy = context.s_row;
			r_out->regions[index].ey = context.e_row;
		}
	}

	return 0;
}


static uint32_t isp_drv_regions_fetch(const struct alg_fetch_region *fetch_param,
		struct alg_slice_regions *r_out)
{
	uint32_t imgW = fetch_param->image_w;
	uint32_t imgH = fetch_param->image_h;
	uint32_t sliceW = fetch_param->slice_w;
	uint32_t sliceH = fetch_param->slice_h;
	uint32_t col_num, row_num;
	uint32_t i, j, index, ret_val;
	struct alg_slice_regions region_temp;
	struct alg_fetch_region_context context;

	if (sliceW <= 0)
		sliceW = imgW;
	if(sliceH <= 0)
		sliceH = imgH;
	col_num = imgW / sliceW + (imgW % sliceW ? 1:0);
	row_num = imgH / sliceH + (imgH % sliceH ? 1:0);

	if (col_num * row_num > ALG_REGIONS_NUM) {
		pr_err("fail to get invalid param %d %d %d %d\n", col_num, row_num, imgW, sliceW);
		return -EFAULT;
	}

	region_temp.rows = row_num;
	region_temp.cols = col_num;

	pr_debug("row_num %d col_num %d imgw %d slicew %d\n", row_num, col_num, imgW, sliceW);
	for (i = 0;i < row_num; i++) {
		for (j = 0; j < col_num; j++) {
			index = i * col_num + j;
			context.s_row = i * sliceH;
			context.s_col = j * sliceW;
			context.e_row = context.s_row + sliceH - 1;
			context.e_col = context.s_col + sliceW - 1;

			region_temp.regions[index].sx = context.s_col;
			region_temp.regions[index].ex = context.e_col;
			region_temp.regions[index].sy = context.s_row;
			region_temp.regions[index].ey = context.e_row;
		}
	}

	ret_val = isp_drv_regions_fetch_ref(fetch_param,&region_temp,r_out);

	return ret_val;
}

void isp_drv_regions_w_align(struct alg_slice_regions *r, int align_v, int min_v, int max_v)
{
	uint32_t i, j, index;
	uint32_t rows = r->rows;
	uint32_t cols = r->cols;

	for (i = 0; i < rows; i++) {
		for(j = 0; j < cols; j++) {
			index = i * cols + j;
			isp_drv_region_w_align(&r->regions[index], align_v, min_v, max_v);
		}
	}
}

static void isp_drv_region_set(const struct alg_region_info *r, struct alg_region_info *r_out)
{
	r_out->sx = r->sx;
	r_out->ex = r->ex;
	r_out->sy = r->sy;
	r_out->ey = r->ey;
}

static void isp_drv_regions_set(const struct alg_slice_regions *r, struct alg_slice_regions *r_out)
{
	int i,j,index;
	int rows = r->rows;
	int cols = r->cols;

	r_out->rows = rows;
	r_out->cols = cols;

	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			index = i*cols + j;
			isp_drv_region_set(&r->regions[index],&r_out->regions[index]);
		}
	}
}

static void isp_drv_regions_3dnr(const struct alg_slice_regions *r_ref, struct alg_slice_regions *r_out,
	int ALIGN_W_V, int ALIGN_H_V, int v_flag)
{
	int index,next_index;
	int move_size_x, move_size_y;
	int rows = r_ref->rows;
	int cols = r_ref->cols;
	int row,col;
	const struct alg_region_info *src_region;
	struct alg_region_info *dst_region;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			index = row*cols + col;

			src_region = &r_ref->regions[index];
			dst_region = &r_out->regions[index];

			move_size_x = (src_region->ex - dst_region->ex) / ALIGN_W_V*ALIGN_W_V;
			if (0 == v_flag) {
				int old_w = dst_region->ex + 1;
				int new_w = (old_w + ALIGN_W_V / 2) / ALIGN_W_V*ALIGN_W_V;
				move_size_x = new_w - old_w;
			}

			if(col != cols - 1) {
				dst_region->ex += move_size_x;
				next_index = row * cols + (col + 1);
				r_out->regions[next_index].sx += move_size_x;
			}

			move_size_y = (dst_region->ey - dst_region->ey) / ALIGN_H_V*ALIGN_H_V;
			if(0 == v_flag) {
				int old_h = dst_region->ey + 1;
				int new_h = (old_h + ALIGN_H_V / 2) / ALIGN_H_V*ALIGN_H_V;
				move_size_y = new_h - old_h;
			}

			if(row != rows - 1) {
				dst_region->ey += move_size_y;
				next_index = (row + 1) * cols + col;
				r_out->regions[next_index].sy += move_size_y;
			}
		}
	}
}

void alg_slice_calc_dec_offline_overlap(struct alg_dec_offline_overlap *param_ptr)
{
	const uint32_t SLICE_W_ALIGN_V = 4;
	uint32_t layer_id = 0;
	uint32_t image_w = param_ptr->img_w;
	uint32_t image_h = param_ptr->img_h;
	uint32_t src_width = image_w;
	uint32_t src_height = image_h;
	uint32_t layer0_padding_w = 0, layer0_padding_h = 0;
	uint32_t dw = 0, dh = 0;
	uint32_t layer_slice_width[ISP_PYR_DEC_LAYER_NUM] = {0};
	uint32_t layer_slice_height[ISP_PYR_DEC_LAYER_NUM] = {0};
	uint32_t cur_layer_width[MAX_PYR_DEC_LAYER_NUM] = {0};
	uint32_t cur_layer_height[MAX_PYR_DEC_LAYER_NUM] = {0};
	struct alg_overlap_info ov_pipe = {0};
	struct alg_block_calc dct_param = {0}, dec_offline_param = {0};
	struct alg_slice_regions org_dec_slice_out[ISP_PYR_DEC_LAYER_NUM];
	struct alg_fetch_region dec_slice_in = {0}, org_dec_slice_in = {0};
	struct alg_slice_regions dec_slice_out[ISP_PYR_DEC_LAYER_NUM];
	uint32_t slice_flag[MAX_PYR_DEC_LAYER_NUM] = {0};

	for (layer_id = 0; layer_id < ISP_PYR_DEC_LAYER_NUM; layer_id++) {
		memset(&org_dec_slice_out[layer_id], 0, sizeof(struct alg_slice_regions));
		memset(&dec_slice_out[layer_id], 0, sizeof(struct alg_slice_regions));
	}

	if (param_ptr->crop_en && 0 == param_ptr->crop_mode) {
		image_w = param_ptr->crop_w;
		image_h = param_ptr->crop_h;
		src_width = image_w;
		src_height = image_h;
	}

	/* width need 4 align & high need 2 align */
	dw = check_image_resolution(image_w, param_ptr->layerNum + 1);
	dh = check_image_resolution(image_h, param_ptr->layerNum);
	layer0_padding_w = src_width + dw;
	layer0_padding_h = src_height + dh;

{
	uint32_t slice_width_org, slice_height_org, layer0_slice_num, after_padding_slice_num;
	uint32_t slice_align_shift = 3;
	for (layer_id = 0; layer_id < param_ptr->layerNum - 1; layer_id++) {
		if (!param_ptr->dec_offline_bypass) {
			slice_width_org = (param_ptr->slice_w >> layer_id);
			slice_height_org = (param_ptr->slice_h >> layer_id);
			layer0_slice_num = (src_width + param_ptr->slice_w - 1) / param_ptr->slice_w;
			after_padding_slice_num = (layer0_padding_w + param_ptr->slice_w - 1) / param_ptr->slice_w;
			if (layer0_slice_num < after_padding_slice_num) {
				layer_slice_width[layer_id] = ((layer_id ? layer0_padding_w : (src_width))
					* slice_width_org + src_width - 1 ) / src_width;
				layer_slice_height[layer_id] = ((layer_id ? layer0_padding_h : (src_height))
					* slice_height_org + src_height - 1) / src_height;
			} else {
				layer_slice_width[layer_id] = slice_width_org;
				layer_slice_height[layer_id] = slice_height_org;
			}

			layer_slice_width[layer_id] = ((layer_slice_width[layer_id] + (1 << slice_align_shift) - 1)
				>> slice_align_shift) << slice_align_shift;
			layer_slice_height[layer_id] = ((layer_slice_height[layer_id] + (1 << slice_align_shift) - 1)
				>> slice_align_shift) << slice_align_shift;
		} else {
			layer_slice_width[layer_id] = param_ptr->slice_w;
			layer_slice_height[layer_id] = param_ptr->slice_h;
		}
	}

	if (param_ptr->layerNum == 1 && param_ptr->dec_offline_bypass) {
		layer_slice_width[0] = param_ptr->slice_w;
		layer_slice_height[0] = param_ptr->slice_h;
	}

	/* if image width is too small, no slice */
	if (param_ptr->slice_mode) {
		for (layer_id = 0; layer_id < param_ptr->layerNum - 1; layer_id++) {
			const uint32_t max_slice_w = param_ptr->MaxSliceWidth;
			if (layer_id == 0) {
				cur_layer_width[layer_id] = src_width;
				cur_layer_height[layer_id] = src_height;
			} else {
				cur_layer_width[layer_id] = layer0_padding_w >> layer_id;
				cur_layer_height[layer_id] = layer0_padding_h >> layer_id;
			}

			if (layer_id == 0 && layer0_padding_w <= max_slice_w) {
				slice_flag[layer_id] = 1;
			} else if (cur_layer_width[layer_id] <= max_slice_w) {
				slice_flag[layer_id] = 1;
			}
		}

		if (param_ptr->layerNum == 1 && param_ptr->dec_offline_bypass) {
			const uint32_t max_slice_w = param_ptr->MaxSliceWidth;
			cur_layer_width[0] = src_width;
			cur_layer_height[0] = src_height;
			if (cur_layer_width[0] <= max_slice_w)
				slice_flag[0] = 1;
		}
	}
}

	/* input */
	core_drv_dnr_init_block(&dct_param);
	CAL_OVERLAP(param_ptr->dct_bypass, dct_param);
	core_drv_pyd_dec_offline_init_block(&dec_offline_param);
	CAL_OVERLAP(param_ptr->dec_offline_bypass, dec_offline_param);

	/* overlap 4 align */
	if (!param_ptr->dec_offline_bypass) {
		ov_pipe.ov_left = (ov_pipe.ov_left + 7) >> 3 << 3;
		ov_pipe.ov_right = (ov_pipe.ov_right + 7) >> 3 << 3;
		ov_pipe.ov_up = (ov_pipe.ov_up + 7) >> 3 << 3;
		ov_pipe.ov_down = (ov_pipe.ov_down + 7) >> 3 << 3;
	} else {
		ov_pipe.ov_left = (ov_pipe.ov_left + 1) >> 1 << 1;
		ov_pipe.ov_right = (ov_pipe.ov_right + 1) >> 1 << 1;
		ov_pipe.ov_up = (ov_pipe.ov_up + 1) >> 1 << 1;
		ov_pipe.ov_down = (ov_pipe.ov_down + 1) >> 1 << 1;
	}

	/* slice cfg */
	for (layer_id = 0; layer_id < param_ptr->layerNum - 1; layer_id++) {
		org_dec_slice_in.image_w = layer_id ? (layer0_padding_w >> layer_id) : (src_width);
		org_dec_slice_in.image_h = layer_id ? (layer0_padding_h >>layer_id) : (src_height);
		if (param_ptr->slice_mode && slice_flag[layer_id]) {
			org_dec_slice_in.slice_w = cur_layer_width[layer_id];
			org_dec_slice_in.slice_h = cur_layer_height[layer_id];
		} else {
			org_dec_slice_in.slice_w = layer_slice_width[layer_id];
			org_dec_slice_in.slice_h = layer_slice_height[layer_id];
		}
		org_dec_slice_in.overlap_left = 0;
		org_dec_slice_in.overlap_right = 0;
		org_dec_slice_in.overlap_up = 0;
		org_dec_slice_in.overlap_down = 0;

		/* get org slice region */
		if (-1 == isp_drv_regions_fetch(&org_dec_slice_in, &org_dec_slice_out[layer_id]))
			return;
	}

	if (param_ptr->layerNum == 1 && param_ptr->dec_offline_bypass) {
		org_dec_slice_in.image_w = src_width;
		org_dec_slice_in.image_h = src_height;
		if(param_ptr->slice_mode && slice_flag[0]) {
			org_dec_slice_in.slice_w = cur_layer_width[0];
			org_dec_slice_in.slice_h = cur_layer_height[0];
		} else {
			org_dec_slice_in.slice_w = layer_slice_width[0];
			org_dec_slice_in.slice_h = layer_slice_height[0];
		}
		org_dec_slice_in.overlap_left = 0;
		org_dec_slice_in.overlap_right = 0;
		org_dec_slice_in.overlap_up = 0;
		org_dec_slice_in.overlap_down = 0;

		/* get org slice region */
		if (-1 == isp_drv_regions_fetch(&org_dec_slice_in, &org_dec_slice_out[0]))
			return;
	}

	for (layer_id = 0; layer_id < param_ptr->layerNum - 1; layer_id++) {
		dec_slice_in.image_w = layer_id ? (layer0_padding_w >> layer_id) : (src_width);
		dec_slice_in.image_h = layer_id ? (layer0_padding_h >> layer_id) : (src_height);
		if (param_ptr->slice_mode && slice_flag[layer_id]) {
			dec_slice_in.slice_w = cur_layer_width[layer_id];
			dec_slice_in.slice_h = cur_layer_height[layer_id];
		} else {
			dec_slice_in.slice_w = layer_slice_width[layer_id];
			dec_slice_in.slice_h = layer_slice_height[layer_id];
		}
		dec_slice_in.overlap_left = ov_pipe.ov_left;
		dec_slice_in.overlap_right = ov_pipe.ov_right;
		dec_slice_in.overlap_up = ov_pipe.ov_up;
		dec_slice_in.overlap_down = ov_pipe.ov_down;

		/* get out slice region */
		if (-1 == isp_drv_regions_fetch(&dec_slice_in, &dec_slice_out[layer_id]))
			return;

		/* dec_offline slice 4 align */
		isp_drv_regions_w_align(&dec_slice_out[layer_id], SLICE_W_ALIGN_V, 0, dec_slice_in.image_w);
	}

	if (param_ptr->layerNum == 1 && param_ptr->dec_offline_bypass) {
		dec_slice_in.image_w = src_width;
		dec_slice_in.image_h = src_height;
		if (param_ptr->slice_mode && slice_flag[0]) {
			dec_slice_in.slice_w = cur_layer_width[0];
			dec_slice_in.slice_h = cur_layer_height[0];
		} else {
			dec_slice_in.slice_w = layer_slice_width[0];
			dec_slice_in.slice_h = layer_slice_height[0];
		}
		dec_slice_in.overlap_left = ov_pipe.ov_left;
		dec_slice_in.overlap_right = ov_pipe.ov_right;
		dec_slice_in.overlap_up = ov_pipe.ov_up;
		dec_slice_in.overlap_down = ov_pipe.ov_down;

		/* get out slice region */
		if (-1 == isp_drv_regions_fetch(&dec_slice_in, &dec_slice_out[0]))
			return;

		/* dec_offline slice 4 align */
		isp_drv_regions_w_align(&dec_slice_out[0], SLICE_W_ALIGN_V, 0, dec_slice_in.image_w);
	}

{
	uint32_t i, j, index, rows, cols;
	/* fecth_dec_slice */
	for (layer_id = 0; layer_id < param_ptr->layerNum - 1; layer_id++) {
		rows = dec_slice_out[layer_id].rows;
		cols = dec_slice_out[layer_id].cols;
		for (i = 0; i < rows; i++) {
			for (j=0; j < cols; j++) {
				index = i * cols + j;
				param_ptr->fecth_dec_region[layer_id][index].sx = dec_slice_out[layer_id].regions[index].sx;
				param_ptr->fecth_dec_region[layer_id][index].ex = dec_slice_out[layer_id].regions[index].ex;
				param_ptr->fecth_dec_region[layer_id][index].sy = dec_slice_out[layer_id].regions[index].sy;
				param_ptr->fecth_dec_region[layer_id][index].ey = dec_slice_out[layer_id].regions[index].ey;
				param_ptr->fecth_dec_overlap[layer_id][index].ov_left = org_dec_slice_out[layer_id].regions[index].sx - dec_slice_out[layer_id].regions[index].sx;
				param_ptr->fecth_dec_overlap[layer_id][index].ov_right = dec_slice_out[layer_id].regions[index].ex - org_dec_slice_out[layer_id].regions[index].ex;
				param_ptr->fecth_dec_overlap[layer_id][index].ov_up = org_dec_slice_out[layer_id].regions[index].sy - dec_slice_out[layer_id].regions[index].sy;
				param_ptr->fecth_dec_overlap[layer_id][index].ov_down = dec_slice_out[layer_id].regions[index].ey - org_dec_slice_out[layer_id].regions[index].ey;
				pr_debug("layer %d fetch ov_r %d\n", layer_id, param_ptr->fecth_dec_overlap[layer_id][index].ov_right);
			}
		}
	}

	if (param_ptr->layerNum == 1 && param_ptr->dec_offline_bypass) {
		rows = dec_slice_out[0].rows;
		cols = dec_slice_out[0].cols;
		for (i = 0; i < rows; i++) {
			for (j = 0; j < cols; j++) {
				index = i * cols + j;
				param_ptr->fecth_dec_region[0][index].sx = dec_slice_out[0].regions[index].sx;
				param_ptr->fecth_dec_region[0][index].ex = dec_slice_out[0].regions[index].ex;
				param_ptr->fecth_dec_region[0][index].sy = dec_slice_out[0].regions[index].sy;
				param_ptr->fecth_dec_region[0][index].ey = dec_slice_out[0].regions[index].ey;
				param_ptr->fecth_dec_overlap[0][index].ov_left = org_dec_slice_out[0].regions[index].sx - dec_slice_out[0].regions[index].sx;
				param_ptr->fecth_dec_overlap[0][index].ov_right = dec_slice_out[0].regions[index].ex - org_dec_slice_out[0].regions[index].ex;
				param_ptr->fecth_dec_overlap[0][index].ov_up = org_dec_slice_out[0].regions[index].sy - dec_slice_out[0].regions[index].sy;
				param_ptr->fecth_dec_overlap[0][index].ov_down = dec_slice_out[0].regions[index].ey - org_dec_slice_out[0].regions[index].ey;
			}
		}
	}

	/* store_dec_slice */
	for (layer_id = 0; layer_id < param_ptr->layerNum; layer_id++) {
		if (layer_id == 0) {
			rows = dec_slice_out[0].rows;
			cols = dec_slice_out[0].cols;
		} else {
			rows = dec_slice_out[layer_id - 1].rows;
			cols = dec_slice_out[layer_id - 1].cols;
		}

		for (i = 0; i < rows; i++) {
			for (j = 0; j < cols; j++) {
				index = i * cols + j;
				if (layer_id == 0) {
					/* store_dct, no overlap, before padding */
					param_ptr->store_dec_overlap[layer_id][index].ov_left = param_ptr->fecth_dec_overlap[layer_id][index].ov_left;
					param_ptr->store_dec_overlap[layer_id][index].ov_right = param_ptr->fecth_dec_overlap[layer_id][index].ov_right;
					param_ptr->store_dec_overlap[layer_id][index].ov_up = param_ptr->fecth_dec_overlap[layer_id][index].ov_up;
					param_ptr->store_dec_overlap[layer_id][index].ov_down = param_ptr->fecth_dec_overlap[layer_id][index].ov_down;
					param_ptr->store_dec_region[layer_id][index].sx = param_ptr->fecth_dec_region[layer_id][index].sx + param_ptr->fecth_dec_overlap[layer_id][index].ov_left;
					param_ptr->store_dec_region[layer_id][index].ex = param_ptr->fecth_dec_region[layer_id][index].ex - param_ptr->fecth_dec_overlap[layer_id][index].ov_right;
					param_ptr->store_dec_region[layer_id][index].sy = param_ptr->fecth_dec_region[layer_id][index].sy + param_ptr->fecth_dec_overlap[layer_id][index].ov_up;
					param_ptr->store_dec_region[layer_id][index].ey = param_ptr->fecth_dec_region[layer_id][index].ey - param_ptr->fecth_dec_overlap[layer_id][index].ov_down;
				} else if (layer_id == 1) {
					param_ptr->store_dec_overlap[layer_id][index].ov_left = param_ptr->fecth_dec_overlap[0][index].ov_left / 2 ;
					param_ptr->store_dec_overlap[layer_id][index].ov_right = param_ptr->fecth_dec_overlap[0][index].ov_right / 2;
					param_ptr->store_dec_overlap[layer_id][index].ov_up = param_ptr->fecth_dec_overlap[0][index].ov_up / 2;
					param_ptr->store_dec_overlap[layer_id][index].ov_down = param_ptr->fecth_dec_overlap[0][index].ov_down / 2 ;
					if(j < cols - 1) {
						param_ptr->store_dec_region[layer_id][index].sx = (param_ptr->fecth_dec_region[0][index].sx + param_ptr->fecth_dec_overlap[0][index].ov_left ) / 2;
						param_ptr->store_dec_region[layer_id][index].sy = (param_ptr->fecth_dec_region[0][index].sy + param_ptr->fecth_dec_overlap[0][index].ov_up) / 2;
						param_ptr->store_dec_region[layer_id][index].ex = (param_ptr->fecth_dec_region[0][index].ex + 1 - param_ptr->fecth_dec_overlap[0][index].ov_right) / 2 - 1;
						param_ptr->store_dec_region[layer_id][index].ey = (param_ptr->fecth_dec_region[0][index].ey +dh + 1 - param_ptr->fecth_dec_overlap[0][index].ov_down ) / 2 - 1;
					} else if (j == cols - 1) {
						param_ptr->store_dec_region[layer_id][index].sx = (param_ptr->fecth_dec_region[0][index].sx + param_ptr->fecth_dec_overlap[0][index].ov_left) /2;
						param_ptr->store_dec_region[layer_id][index].sy = (param_ptr->fecth_dec_region[0][index].sy + param_ptr->fecth_dec_overlap[0][index].ov_up) / 2;
						param_ptr->store_dec_region[layer_id][index].ex = (param_ptr->fecth_dec_region[0][index].ex + dw + 1 - param_ptr->fecth_dec_overlap[0][index].ov_right) / 2 - 1;
						param_ptr->store_dec_region[layer_id][index].ey = (param_ptr->fecth_dec_region[0][index].ey + dh + 1 - param_ptr->fecth_dec_overlap[0][index].ov_down ) / 2 - 1;
					}
				} else {
					param_ptr->store_dec_overlap[layer_id][index].ov_left  = param_ptr->fecth_dec_overlap[layer_id - 1][index].ov_left / 2 ;
					param_ptr->store_dec_overlap[layer_id][index].ov_right = param_ptr->fecth_dec_overlap[layer_id - 1][index].ov_right / 2;
					param_ptr->store_dec_overlap[layer_id][index].ov_up = param_ptr->fecth_dec_overlap[layer_id - 1][index].ov_up / 2;
					param_ptr->store_dec_overlap[layer_id][index].ov_down  = param_ptr->fecth_dec_overlap[layer_id - 1][index].ov_down / 2;
					param_ptr->store_dec_region[layer_id][index].sx = (param_ptr->fecth_dec_region[layer_id - 1][index].sx + param_ptr->fecth_dec_overlap[layer_id - 1][index].ov_left) / 2;
					param_ptr->store_dec_region[layer_id][index].sy = (param_ptr->fecth_dec_region[layer_id - 1][index].sy + param_ptr->fecth_dec_overlap[layer_id - 1][index].ov_up) / 2;
					param_ptr->store_dec_region[layer_id][index].ex = (param_ptr->fecth_dec_region[layer_id - 1][index].ex + 1 - param_ptr->fecth_dec_overlap[layer_id - 1][index].ov_right) / 2 - 1;
					param_ptr->store_dec_region[layer_id][index].ey = (param_ptr->fecth_dec_region[layer_id - 1][index].ey + 1 - param_ptr->fecth_dec_overlap[layer_id - 1][index].ov_down) / 2 - 1;
				}
			}
		}
	}
}
}

void core_drv_dewarping_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 160;
	block_ptr->right = 160;
	block_ptr->up = 160;
	block_ptr->down = 160;
}

void core_drv_pyd_rec_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = REC_OVERLAP;
	block_ptr->right = REC_OVERLAP;
	block_ptr->up = REC_OVERLAP;
	block_ptr->down = REC_OVERLAP;
}

void core_drv_cnr_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 3;
	block_ptr->right = 3;
	block_ptr->up = 3;
	block_ptr->down = 3;
}

static void core_drv_ynr_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 3;
	block_ptr->right = 3;
	block_ptr->up = 3;
	block_ptr->down = 3;
}

void core_drv_yuv420_to_rgb10_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 2;
	block_ptr->right = 2;
	block_ptr->up = 0;
	block_ptr->down = 1;
}

static uint16_t cal_ratio(uint16_t iw, uint16_t ow, uint16_t outformat)
{
	uint16_t ratio = 0;

	if(ow * 2 > iw) {
		ratio = 1;
	} else if ((ow * 2 <= iw) && (ow * 8 >= iw)) {
		ratio = 2;
	} else if ((ow * 8 < iw) && (ow * 16 >= iw)) {
		ratio = 4;
	} else if ((ow * 16 < iw) && (ow * 32 >= iw)) {
		ratio = 8;
	} else if(ow * 16 < iw) {
		ratio = 16;
	} else {
		ratio = 1;
	}

	return ratio;
}

static void cal_trim_deci_info(uint16_t iw, uint16_t ow, uint16_t outformat, uint16_t* trimx, uint16_t* deci_factor_x)
{
	uint16_t tmp;

	tmp = cal_ratio(iw, ow, outformat);

	if (iw % (2 * tmp) == 0) {
		*trimx = iw;
		*deci_factor_x = tmp;
	} else {
		*trimx = (iw / (2 * tmp) * (2 * tmp));
		tmp = cal_ratio(*trimx, ow, outformat);
		*deci_factor_x = tmp;
	}
}

static uint16_t cal_rect(uint16_t Bstartcol,uint16_t Bstartrow,uint16_t Bendcol,uint16_t Bendrow,
	uint16_t Astartcol,uint16_t Astartrow,uint16_t Aendcol,uint16_t Aendrow,uint16_t* first,
	uint16_t* second, uint16_t*third,uint16_t*fourth)
{
	uint16_t num=0;
	uint16_t flag1=0,flag2=0,flag3=0,flag4=0;
	*first = 0;
	*second = 0;
	*third = 0;
	*fourth = 0;
	if(Astartcol >= Bstartcol && Astartcol <= Bendcol)
		flag1 = 1;
	if(Aendcol >= Bstartcol && Aendcol <= Bendcol)
		flag2 = 1;
	if(Astartrow >= Bstartrow && Astartrow <= Bendrow)
		flag3 = 1;
	if(Aendrow >= Bstartrow && Aendrow <= Bendrow)
		flag4 = 1;

	*first = (uint16_t)(flag1&flag3);
	*second = (uint16_t)(flag2&flag3);
	*third = (uint16_t)(flag1&flag4);
	*fourth = (uint16_t)(flag2&flag4);
	num = *first+*second+*third+*fourth;
	return num;
}

int16_t cal_phase_4(uint16_t trimy,uint16_t oh,uint16_t deci_factor_y, uint16_t real_startrow,
	uint16_t* phasey, uint16_t* realnumup,uint16_t thumbnail_phase,uint16_t thumbnailscaler_base_align)
{
	int16_t num = 0, phase = 0;
	uint16_t realnum = 0;
	int count = 0;
	int sumphase = thumbnail_phase;

	int i;
	int deci_y = trimy / deci_factor_y;
	if (real_startrow == 0) {
		*realnumup = realnum;
		*phasey = thumbnail_phase;
		return num;
	} else {
		for(i = 0; i < deci_y; i++) {
			if (sumphase < deci_y) {
				sumphase = sumphase + oh;
				count++;

				if (i == deci_y - 1)
					realnum++;
			} else {
				realnum++;
				if ((count * deci_factor_y) >= real_startrow) {
					if (realnum % thumbnailscaler_base_align == 0)
						break;
				}
				sumphase = sumphase -deci_y + oh;
				count++;
				if (i == deci_y - 1)
					realnum++;
			}
		}
		phase = sumphase - deci_y;
		*realnumup = realnum;
		*phasey = phase;
		return num = count * deci_factor_y - real_startrow;
	}
}

int16_t cal_phase_1uv(uint16_t trimy,uint16_t oh,uint16_t deci_factor_y, uint16_t real_startrow,
	uint16_t* phasey, uint16_t* realnumup,uint16_t yrealnumup,uint16_t thumbnailscaler_phase)
{
	int16_t num = 0, phase = 0;
	uint16_t realnum = 0;
	int count = 0;
	int sumphase = thumbnailscaler_phase;

	int i;
	int deci_y = trimy / deci_factor_y;

	if (real_startrow == 0) {
		*realnumup = realnum;
		*phasey = thumbnailscaler_phase;
		return num;
	} else {
		for (i = 0; i < deci_y; i++) {
			if (sumphase < deci_y) {
				sumphase = sumphase +oh;
				count++;
				if (i == deci_y - 1)
					realnum++;
			} else {
				realnum++;
				if (realnum * 2 == yrealnumup)
					break;
				if ((count * deci_factor_y) >= real_startrow) {
					if (realnum % 1 == 0 && realnum * 2 == yrealnumup)
						break;
				}
				sumphase = sumphase -deci_y + oh;
				count++;
				if (i == deci_y - 1)
					realnum++;
			}
		}
		phase = sumphase - deci_y;
		*realnumup = realnum;
		*phasey = phase;
		return num = count * deci_factor_y - real_startrow;
	}
}

void cal_trimcoordinate(struct THUMB_SLICE_PARAM_T* thumbslice_param,struct CONFIGINFO_T* alg_configinfo_t,
	struct THUMBINFO_T*  alg_thumbinfo,struct TH_infophasenum* th_infophasenum)
{
	int id =0;
	uint16_t deci_factor_x = 0, deci_factor_y = 0;
	uint16_t trimx = 0, trimy = 0;
	uint16_t cur_col = 0, cur_row = 0;
	uint16_t totalcol = 0, totalrow = 0;
	uint16_t width = 0, height = 0;
	uint16_t start_col = 0, start_row = 0, end_row = 0, end_col = 0;
	uint16_t overlap_up = 0, overlap_down = 0, overlap_left = 0, overlap_right = 0;

	uint16_t trim0startrow = 0, trim0startcol = 0, trim0endrow = 0, trim0endcol = 0;
	uint16_t trim0realiw = 0, trim0realih = 0;
	uint16_t thumbnailscaler_trimstartrow = 0;
	uint16_t thumbnailscaler_trimstartcol = 0;
	uint16_t thumbnailscaler_trimendrow = 0;
	uint16_t thumbnailscaler_trimendcol = 0;
	uint16_t trim_realstx = 0, trim_realsty = 0;

	uint16_t trim0slice_totalcol = 0, trim0slice_totalrow = 0;
	uint16_t trim0slice_col = 0, trim0slice_row = 0;
	uint16_t inrealstartrow = 0, inrealstartcol = 0;
	uint16_t inrealendrow = 0, inrealendcol = 0;
	uint16_t slicewidth = 0, sliceheight = 0;

	uint16_t phaseup = 0, phasedown = 0, phaseleft = 0, phaseright = 0;
	uint16_t numup=0, numdown=0, numleft=0, numright=0;
	uint16_t realnumup =0, realnumdown =0, realnumleft =0, realnumright =0;
	uint16_t realih = 0, realiw = 0, realoh = 0, realow = 0;

	uint16_t uvphaseup = 0, uvphasedown = 0, uvphaseleft = 0, uvphaseright = 0;
	uint16_t uvnumup=0, uvnumdown=0, uvnumleft=0, uvnumright=0;
	uint16_t uvrealnumup =0, uvrealnumdown =0, uvrealnumleft =0, uvrealnumright =0;
	uint16_t uvrealih = 0, uvrealiw = 0, uvrealoh = 0, uvrealow = 0;
	uint16_t uvtrim_realstx = 0, uvtrim_realsty = 0;

	id = thumbslice_param->id;
	trim0startrow = alg_thumbinfo->thumbsliceinfo[id].trim0startrow;
	trim0startcol = alg_thumbinfo->thumbsliceinfo[id].trim0startcol;
	trim0endrow = alg_thumbinfo->thumbsliceinfo[id].trim0endrow;
	trim0endcol = alg_thumbinfo->thumbsliceinfo[id].trim0endcol;
	trim0slice_col = alg_thumbinfo->thumbsliceinfo[id].trim0slice_col;
	trim0slice_row = alg_thumbinfo->thumbsliceinfo[id].trim0slice_row;
	slicewidth = alg_thumbinfo->thumbsliceinfo[id].slicewidth;
	sliceheight = alg_thumbinfo->thumbsliceinfo[id].sliceheight;

	trim0realiw =  alg_thumbinfo->thumbsliceinfo[id].trim0realiw;
	trim0realih =  alg_thumbinfo->thumbsliceinfo[id].trim0realih;
	deci_factor_x = alg_thumbinfo->thumbsliceinfo[id].deci_factor_x;
	deci_factor_y = alg_thumbinfo->thumbsliceinfo[id].deci_factor_y;
	trimx = alg_thumbinfo->thumbsliceinfo[id].trimx;
	trimy = alg_thumbinfo->thumbsliceinfo[id].trimy;
	cur_col =alg_thumbinfo->thumbsliceinfo[id].cur_col;
	cur_row = alg_thumbinfo->thumbsliceinfo[id].cur_row;
	totalcol = alg_thumbinfo->thumbsliceinfo[id].totalcol;
	totalrow = alg_thumbinfo->thumbsliceinfo[id].totalrow;

	thumbnailscaler_trimstartrow = alg_configinfo_t->thumbnailscaler_trimstartrow;
	thumbnailscaler_trimstartcol = alg_configinfo_t->thumbnailscaler_trimstartcol;
	thumbnailscaler_trimendrow = alg_configinfo_t->thumbnailscaler_trimendrow-1;
	thumbnailscaler_trimendcol = alg_configinfo_t->thumbnailscaler_trimendcol-1;
	trim0slice_totalcol = alg_configinfo_t->trim0slice_totalcol;
	trim0slice_totalrow = alg_configinfo_t->trim0slice_totalrow;

	inrealstartrow = trim0startrow - thumbnailscaler_trimstartrow;
	inrealstartcol = trim0startcol - thumbnailscaler_trimstartcol;
	inrealendrow = trim0endrow - thumbnailscaler_trimstartrow;
	inrealendcol = trim0endcol - thumbnailscaler_trimstartcol;

	width = thumbslice_param->width;
	height = thumbslice_param->height;
	start_col = thumbslice_param->start_col;
	end_col = thumbslice_param->end_col;
	start_row = thumbslice_param->start_row;
	end_row = thumbslice_param->end_row;
	overlap_left = thumbslice_param->overlap_left;
	overlap_right = thumbslice_param->overlap_right	;
	overlap_up = thumbslice_param->overlap_up;
	overlap_down = thumbslice_param->overlap_down;

	if ((trim0slice_row == trim0slice_totalrow - 1) && (trim0slice_col == trim0slice_totalcol - 1)) {
		inrealendcol = inrealendcol - (trim0realiw - trimx);
		inrealendrow = inrealendrow - (trim0realih - trimy);
	} else if (trim0slice_col == trim0slice_totalcol - 1)
		inrealendcol = inrealendcol - (trim0realiw - trimx);
	else if (trim0slice_row == trim0slice_totalrow - 1)
		inrealendrow = inrealendrow - (trim0realih - trimy);

	numup = cal_phase_4(trimy, alg_configinfo_t->oh, deci_factor_y, inrealstartrow, &phaseup, &realnumup, alg_configinfo_t->thumbnailscaler_phaseY, alg_configinfo_t->thumbnailscaler_base_align);
	numdown = cal_phase_4(trimy, alg_configinfo_t->oh, deci_factor_y, inrealendrow + 1, &phasedown, &realnumdown, alg_configinfo_t->thumbnailscaler_phaseY, alg_configinfo_t->thumbnailscaler_base_align);
	numleft = cal_phase_4(trimx, alg_configinfo_t->ow, deci_factor_x, inrealstartcol, &phaseleft, &realnumleft, alg_configinfo_t->thumbnailscaler_phaseX, alg_configinfo_t->thumbnailscaler_base_align);
	numright = cal_phase_4(trimx, alg_configinfo_t->ow, deci_factor_x, inrealendcol + 1, &phaseright, &realnumright, alg_configinfo_t->thumbnailscaler_phaseX, alg_configinfo_t->thumbnailscaler_base_align);

	realih = inrealendrow - inrealstartrow +1 - numup + numdown;
	realiw = inrealendcol - inrealstartcol + 1 - numleft + numright;
	realoh = realnumdown - realnumup;
	realow = realnumright - realnumleft;

	trim_realstx = numleft + (trim0startcol - start_col);
	trim_realsty = numup + (trim0startrow - start_row);

	th_infophasenum->thumbinfo_trimcoordinate_yid[id].trim_s_col = trim_realstx;
	th_infophasenum->thumbinfo_trimcoordinate_yid[id].trim_e_col = trim_realstx + realiw - 1;
	th_infophasenum->thumbinfo_trimcoordinate_yid[id].trim_width = realiw;
	th_infophasenum->thumbinfo_trimcoordinate_yid[id].trim_s_row = trim_realsty;
	th_infophasenum->thumbinfo_trimcoordinate_yid[id].trim_e_row = trim_realsty + realih - 1;
	th_infophasenum->thumbinfo_trimcoordinate_yid[id].trim_height = realih;

	if (alg_configinfo_t->outformat != 2) {
		uvnumup = cal_phase_1uv(trimy / 2, alg_configinfo_t->oh / 2, deci_factor_y, inrealstartrow / 2, &uvphaseup, &uvrealnumup, realnumup, alg_configinfo_t->thumbnailscaler_phaseY / 2);
		uvnumdown = cal_phase_1uv(trimy / 2, alg_configinfo_t->oh / 2, deci_factor_y, (inrealendrow - 1) / 2 + 1, &uvphasedown, &uvrealnumdown, realnumdown, alg_configinfo_t->thumbnailscaler_phaseY / 2);
		uvnumleft = cal_phase_1uv(trimx / 2, alg_configinfo_t->ow / 2, deci_factor_x, inrealstartcol / 2, &uvphaseleft, &uvrealnumleft, realnumleft, alg_configinfo_t->thumbnailscaler_phaseX / 2);
		uvnumright = cal_phase_1uv(trimx / 2, alg_configinfo_t->ow / 2, deci_factor_x, (inrealendcol - 1) / 2 + 1, &uvphaseright, &uvrealnumright, realnumright, alg_configinfo_t->thumbnailscaler_phaseX / 2);

		uvrealih = (inrealendrow - 1) / 2 - inrealstartrow / 2 + 1 - uvnumup + uvnumdown ;
		uvrealiw = (inrealendcol - 1) / 2 - inrealstartcol / 2 + 1 - uvnumleft + uvnumright;
		uvrealoh = uvrealnumdown - uvrealnumup;
		uvrealow = uvrealnumright - uvrealnumleft;
		uvtrim_realstx = uvnumleft + (trim0startcol - start_col) / 2;
		uvtrim_realsty = uvnumup + (trim0startrow - start_row) / 2;

		th_infophasenum->thumbinfo_trimcoordinate_uvid[id].trim_s_col = uvtrim_realstx;
		th_infophasenum->thumbinfo_trimcoordinate_uvid[id].trim_e_col = uvtrim_realstx + uvrealiw - 1;
		th_infophasenum->thumbinfo_trimcoordinate_uvid[id].trim_width = uvrealiw;
		th_infophasenum->thumbinfo_trimcoordinate_uvid[id].trim_s_row = uvtrim_realsty;
		th_infophasenum->thumbinfo_trimcoordinate_uvid[id].trim_e_row = uvtrim_realsty + uvrealih - 1;
		th_infophasenum->thumbinfo_trimcoordinate_uvid[id].trim_height = uvrealih;
	}
}

void thumbInitSliceInfo_forhw(struct thumbscaler_info* thumbnail_info, int col, int row,
	int totalcol, int totalrow, struct THUMB_SLICE_PARAM_T *thumbframeInfo)
{
	int id = thumbframeInfo->id;
	uint16_t deci_factor_x,deci_factor_y;
	uint16_t trimx,trimy;
	uint16_t num1,num2;
	uint16_t first1,second1,third1,fourth1;
	uint16_t first2,second2,third2,fourth2;
	int flagsum ;
	int a[4] = {1,2,3,4};
	uint16_t *aa = (uint16_t*)vzalloc(16 * sizeof(uint16_t));
	int coltemp=0,rowtemp=0;
	int i;

	if (thumbnail_info->configinfo.thumbnailscaler_trim0_en == 0) {
		thumbnail_info->configinfo.thumbnailscaler_trimstartcol = 0;
		thumbnail_info->configinfo.thumbnailscaler_trimstartrow = 0;
		thumbnail_info->configinfo.thumbnailscaler_trimendcol = thumbnail_info->configinfo.iw;
		thumbnail_info->configinfo.thumbnailscaler_trimendrow = thumbnail_info->configinfo.ih;
	} else {
		thumbnail_info->configinfo.thumbnailscaler_trimendcol =
			thumbnail_info->configinfo.thumbnailscaler_trimsizeX + thumbnail_info->configinfo.thumbnailscaler_trimstartcol;
		thumbnail_info->configinfo.thumbnailscaler_trimendrow =
			thumbnail_info->configinfo.thumbnailscaler_trimsizeY + thumbnail_info->configinfo.thumbnailscaler_trimstartrow;
	}

	thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0realiw =
		thumbnail_info->configinfo.thumbnailscaler_trimendcol-1 -
		thumbnail_info->configinfo.thumbnailscaler_trimstartcol +1;
	thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0realih =
		thumbnail_info->configinfo.thumbnailscaler_trimendrow-1 -
		thumbnail_info->configinfo.thumbnailscaler_trimstartrow +1;

	cal_trim_deci_info(thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0realiw,
		thumbnail_info->configinfo.ow,  thumbnail_info->configinfo.outformat, &trimx, &deci_factor_x);
	cal_trim_deci_info(thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0realih,
		thumbnail_info->configinfo.oh,  thumbnail_info->configinfo.outformat, &trimy, &deci_factor_y);

	if (thumbframeInfo->start_col > thumbnail_info->configinfo.thumbnailscaler_trimendcol - 1 ||
		thumbframeInfo->start_row > thumbnail_info->configinfo.thumbnailscaler_trimendrow-1 ||
		thumbframeInfo->end_col < thumbnail_info->configinfo.thumbnailscaler_trimstartcol ||
		thumbframeInfo->end_row < thumbnail_info->configinfo.thumbnailscaler_trimstartrow)
		thumbnail_info->thumbinfo.thumbsliceinfo[id].scalerswitch = 0;
	else
		thumbnail_info->thumbinfo.thumbsliceinfo[id].scalerswitch = 1;

	if (thumbnail_info->thumbinfo.thumbsliceinfo[id].scalerswitch == 1) {
		num1 = cal_rect(thumbframeInfo->start_col,
			thumbframeInfo->start_row,
			thumbframeInfo->end_col,
			thumbframeInfo->end_row,
			thumbnail_info->configinfo.thumbnailscaler_trimstartcol,
			thumbnail_info->configinfo.thumbnailscaler_trimstartrow,
			thumbnail_info->configinfo.thumbnailscaler_trimendcol - 1,
			thumbnail_info->configinfo.thumbnailscaler_trimendrow - 1,
			&first1,&second1,&third1,&fourth1);

		num2 = cal_rect(
			thumbnail_info->configinfo.thumbnailscaler_trimstartcol,
			thumbnail_info->configinfo.thumbnailscaler_trimstartrow,
			thumbnail_info->configinfo.thumbnailscaler_trimendcol - 1,
			thumbnail_info->configinfo.thumbnailscaler_trimendrow - 1,
			thumbframeInfo->start_col,
			thumbframeInfo->start_row,
			thumbframeInfo->end_col,
			thumbframeInfo->end_row,
			&first2,&second2,&third2,&fourth2);

		if (num1== 0 && num2 == 0) {
			thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = MAX(thumbframeInfo->start_col,thumbnail_info->configinfo.thumbnailscaler_trimstartcol);
			thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = MAX(thumbframeInfo->start_row,thumbnail_info->configinfo.thumbnailscaler_trimstartrow);
			thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = MIN(thumbframeInfo->end_col,thumbnail_info->configinfo.thumbnailscaler_trimendcol-1);
			thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = MIN(thumbframeInfo->end_row,thumbnail_info->configinfo.thumbnailscaler_trimendrow-1);
		} else if (num1 == 4 && num2 == 4) {
			thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbnail_info->configinfo.thumbnailscaler_trimstartcol;//hong
			thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbnail_info->configinfo.thumbnailscaler_trimstartrow;
			thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbnail_info->configinfo.thumbnailscaler_trimendcol-1;
			thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbnail_info->configinfo.thumbnailscaler_trimendrow-1;
		} else if (num1 == 4 &&(num2 ==0 || num2== 1||num2 == 2)) {
			thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbnail_info->configinfo.thumbnailscaler_trimstartcol;//hong
			thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbnail_info->configinfo.thumbnailscaler_trimstartrow;
			thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbnail_info->configinfo.thumbnailscaler_trimendcol-1;
			thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbnail_info->configinfo.thumbnailscaler_trimendrow-1;
		} else if ((num1 == 0 || num1== 1||num1 == 2) && num2 == 4) {
			thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbframeInfo->start_col;//hei
			thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbframeInfo->start_row;
			thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbframeInfo->end_col;
			thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbframeInfo->end_row;
		} else if ((num1 == 0 || num1 ==1) && num2 == 2) {

			flagsum = a[0] * first2 +a[1] * second2 + a[2] * third2 + a[3] * fourth2;
			if (flagsum == 6) {
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbnail_info->configinfo.thumbnailscaler_trimstartcol;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbframeInfo->start_row;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbframeInfo->end_col;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbframeInfo->end_row;
			} else if (flagsum == 7) {
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbframeInfo->start_col;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbnail_info->configinfo.thumbnailscaler_trimstartrow;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbframeInfo->end_col;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbframeInfo->end_row;
			} else if (flagsum == 4) {
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbframeInfo->start_col;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbframeInfo->start_row;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbnail_info->configinfo.thumbnailscaler_trimendcol-1;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbframeInfo->end_row;
			} else if (flagsum == 3) {
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbframeInfo->start_col;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbframeInfo->start_row;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbframeInfo->end_col;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbnail_info->configinfo.thumbnailscaler_trimendrow-1;
			}
		} else if (num1 == 2 && (num2 == 0 || num2 == 1)) {

			int flagsum = a[0] * first1 + a[1] * second1 + a[2] * third1 + a[3] * fourth1;
			if (flagsum == 6) {
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbframeInfo->start_col;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbnail_info->configinfo.thumbnailscaler_trimstartrow;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbnail_info->configinfo.thumbnailscaler_trimendcol-1;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbnail_info->configinfo.thumbnailscaler_trimendrow-1;
			} else if (flagsum == 7) {
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbnail_info->configinfo.thumbnailscaler_trimstartcol;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbframeInfo->start_row;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbnail_info->configinfo.thumbnailscaler_trimendcol-1;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbnail_info->configinfo.thumbnailscaler_trimendrow-1;
			} else if (flagsum == 4) {
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbnail_info->configinfo.thumbnailscaler_trimstartcol;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbnail_info->configinfo.thumbnailscaler_trimstartrow;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbframeInfo->end_col;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbnail_info->configinfo.thumbnailscaler_trimendrow-1;
			} else if (flagsum == 3) {
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbnail_info->configinfo.thumbnailscaler_trimstartcol;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbnail_info->configinfo.thumbnailscaler_trimstartrow;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbnail_info->configinfo.thumbnailscaler_trimendcol-1;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbframeInfo->end_row;
			}
		}else if (num1 ==1 && num2 ==1) {
			if (first1==1) {
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbnail_info->configinfo.thumbnailscaler_trimstartcol;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbnail_info->configinfo.thumbnailscaler_trimstartrow;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbframeInfo->end_col;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbframeInfo->end_row;
			} else if (second1 ==1) {
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbframeInfo->start_col;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbnail_info->configinfo.thumbnailscaler_trimstartrow;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbnail_info->configinfo.thumbnailscaler_trimendcol-1;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbframeInfo->end_row;
			} else if (third1 == 1) {
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbnail_info->configinfo.thumbnailscaler_trimstartcol;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbframeInfo->start_row;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbframeInfo->end_col;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbnail_info->configinfo.thumbnailscaler_trimendrow-1;
			} else if (fourth1 == 1) {
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbframeInfo->start_col;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbframeInfo->start_row;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbnail_info->configinfo.thumbnailscaler_trimendcol-1;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbnail_info->configinfo.thumbnailscaler_trimendrow-1;
			}
		}else if (num1 ==2 && num2 == 2) {

			flagsum = a[0]*first1 +a[1]*second1 + a[2]*third1 + a[3]*fourth1;
			if (flagsum == 4) {
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbnail_info->configinfo.thumbnailscaler_trimstartcol;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbnail_info->configinfo.thumbnailscaler_trimstartrow;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbframeInfo->end_col;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbnail_info->configinfo.thumbnailscaler_trimendrow-1;
			} else if (flagsum == 7) {
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbnail_info->configinfo.thumbnailscaler_trimstartcol;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbframeInfo->start_row;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbnail_info->configinfo.thumbnailscaler_trimendcol-1;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbnail_info->configinfo.thumbnailscaler_trimendrow-1;
			} else if (flagsum == 6) {
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbframeInfo->start_col;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbnail_info->configinfo.thumbnailscaler_trimstartrow;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbnail_info->configinfo.thumbnailscaler_trimendcol-1;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbnail_info->configinfo.thumbnailscaler_trimendrow-1;
			} else if (flagsum == 3) {
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startcol = thumbnail_info->configinfo.thumbnailscaler_trimstartcol;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0startrow = thumbnail_info->configinfo.thumbnailscaler_trimstartrow;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol = thumbnail_info->configinfo.thumbnailscaler_trimendcol-1;
				thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow = thumbframeInfo->end_row;
			}
		} for ( i = 0;i < totalcol; i++) {
			if ((i + 1) * thumbframeInfo->width <= thumbnail_info->configinfo.iw)
				aa[i] = (i + 1) * thumbframeInfo->width - 1;
			else
				aa[i] = thumbnail_info->configinfo.iw - 1;

			if (aa[i] >= thumbnail_info->configinfo.thumbnailscaler_trimstartcol
				&& aa[i]<thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol)
				coltemp = coltemp + 1;
		} for ( i = 0;i < totalrow;i++) {
			if ((i + 1) * thumbframeInfo->height <= thumbnail_info->configinfo.ih)
				aa[i] = (i + 1) * thumbframeInfo->height - 1;
			else
				aa[i] = thumbnail_info->configinfo.ih - 1;

			if (aa[i] >= thumbnail_info->configinfo.thumbnailscaler_trimstartrow &&
				aa[i] < thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow)
				rowtemp = rowtemp + 1;
		}
		thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0slice_col = coltemp;
		thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0slice_row = rowtemp;

		if (thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endcol == thumbnail_info->configinfo.thumbnailscaler_trimendcol - 1 &&
			thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0endrow == thumbnail_info->configinfo.thumbnailscaler_trimendrow - 1) {
			thumbnail_info->configinfo.trim0slice_totalcol = thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0slice_col + 1;
			thumbnail_info->configinfo.trim0slice_totalrow = thumbnail_info->thumbinfo.thumbsliceinfo[id].trim0slice_row + 1;
		}
	}
	vfree(aa);

	thumbnail_info->thumbinfo.thumbsliceinfo[id].totalcol = totalcol;
	thumbnail_info->thumbinfo.thumbsliceinfo[id].totalrow = totalrow;
	thumbnail_info->thumbinfo.thumbsliceinfo[id].cur_col = col;
	thumbnail_info->thumbinfo.thumbsliceinfo[id].cur_row = row;
	thumbnail_info->thumbinfo.thumbsliceinfo[id].slicewidth = thumbframeInfo->width;
	thumbnail_info->thumbinfo.thumbsliceinfo[id].sliceheight = thumbframeInfo->height;
	thumbnail_info->thumbinfo.thumbsliceinfo[id].trimx = trimx;
	thumbnail_info->thumbinfo.thumbsliceinfo[id].trimy = trimy;
	thumbnail_info->thumbinfo.thumbsliceinfo[id].deci_factor_x = deci_factor_x;
	thumbnail_info->thumbinfo.thumbsliceinfo[id].deci_factor_y = deci_factor_y;
}


void cal_TH_infophasenum_v2(struct TH_infophasenum* th_infophasenum, struct CONFIGINFO_T* configinfo,
		struct THUMBINFO_T* thumbinfo,int slicecols,int slicerows, struct THUMB_SLICE_PARAM_T *sliceInfo)
{
	uint16_t needup_v = 0;
	uint16_t needdown_v = 0;
	uint16_t needleft_v = 0;
	uint16_t needright_v = 0;
	uint16_t *needup = &needup_v;
	uint16_t *needdown = &needdown_v;
	uint16_t *needleft = &needleft_v;
	uint16_t *needright = &needright_v;

	int id;
	int ii, jj;
	uint16_t deci_factor_x,deci_factor_y;
	uint16_t trimx, trimy;
	uint16_t cur_col,cur_row;
	uint16_t totalcol,totalrow;

	uint16_t trim0startrow;
	uint16_t trim0startcol;
	uint16_t trim0endrow;
	uint16_t trim0endcol;
	uint16_t trim0realiw;
	uint16_t trim0realih;
	uint16_t thumbnailscaler_trimstartrow;
	uint16_t thumbnailscaler_trimstartcol;
	uint16_t thumbnailscaler_trimendrow;
	uint16_t thumbnailscaler_trimendcol;
	uint16_t trim0slice_totalcol;
	uint16_t trim0slice_totalrow;
	uint16_t trim0slice_col;
	uint16_t trim0slice_row;
	uint16_t inrealstartrow;
	uint16_t inrealstartcol;
	uint16_t inrealendrow;
	uint16_t inrealendcol;
	uint16_t phaseup ,phasedown ,phaseleft ,phaseright ;
	int16_t numup,numdown,numleft,numright;
	uint16_t realnumup ,realnumdown ,realnumleft ,realnumright ;
	uint16_t uvphaseup ,uvphasedown ,uvphaseleft ,uvphaseright;
	int16_t uvnumup,uvnumdown,uvnumleft,uvnumright;
	uint16_t uvrealnumup ,uvrealnumdown ,uvrealnumleft ,uvrealnumright ;

	id = 0;
	for (ii = 0; ii < slicerows; ii++) {
		for (jj = 0; jj < slicecols; jj++,id++) {
			needup_v = 0;
			needdown_v = 0;
			needleft_v = 0;
			needright_v = 0;

			if (thumbinfo->thumbsliceinfo[id].scalerswitch == 0) {
				thumbinfo->thumbsliceinfo[id].realih = 0 ;
				thumbinfo->thumbsliceinfo[id].realiw = 0;
				thumbinfo->thumbsliceinfo[id].realoh = 0;
				thumbinfo->thumbsliceinfo[id].realow = 0;
				continue;
			}


			thumbnailscaler_trimstartrow = configinfo->thumbnailscaler_trimstartrow;
			thumbnailscaler_trimstartcol = configinfo->thumbnailscaler_trimstartcol;
			thumbnailscaler_trimendrow = configinfo->thumbnailscaler_trimendrow-1;
			thumbnailscaler_trimendcol = configinfo->thumbnailscaler_trimendcol-1;

			trim0slice_totalcol = configinfo->trim0slice_totalcol;
			trim0slice_totalrow = configinfo->trim0slice_totalrow;
			trim0slice_col = thumbinfo->thumbsliceinfo[id].trim0slice_col;
			trim0slice_row = thumbinfo->thumbsliceinfo[id].trim0slice_row;

			trim0startrow = thumbinfo->thumbsliceinfo[id].trim0startrow;
			trim0startcol = thumbinfo->thumbsliceinfo[id].trim0startcol;
			trim0endrow = thumbinfo->thumbsliceinfo[id].trim0endrow;
			trim0endcol = thumbinfo->thumbsliceinfo[id].trim0endcol;

			trim0realiw = thumbinfo->thumbsliceinfo[id].trim0realiw;
			trim0realih = thumbinfo->thumbsliceinfo[id].trim0realih;

			inrealstartrow = trim0startrow - thumbnailscaler_trimstartrow;
			inrealstartcol = trim0startcol - thumbnailscaler_trimstartcol;
			inrealendrow = trim0endrow - thumbnailscaler_trimstartrow;
			inrealendcol = trim0endcol - thumbnailscaler_trimstartcol;

			deci_factor_x = thumbinfo->thumbsliceinfo[id].deci_factor_x;
			deci_factor_y = thumbinfo->thumbsliceinfo[id].deci_factor_y;
			trimx = thumbinfo->thumbsliceinfo[id].trimx;
			trimy = thumbinfo->thumbsliceinfo[id].trimy;
			cur_col = thumbinfo->thumbsliceinfo[id].cur_col;
			cur_row = thumbinfo->thumbsliceinfo[id].cur_row;
			totalcol = thumbinfo->thumbsliceinfo[id].totalcol;
			totalrow = thumbinfo->thumbsliceinfo[id].totalrow;

			if ((trim0slice_row == trim0slice_totalrow - 1) && (trim0slice_col == trim0slice_totalcol - 1)) {

				inrealendcol = inrealendcol - (trim0realiw - trimx);
				inrealendrow = inrealendrow - (trim0realih - trimy);
			}
			else if (trim0slice_col == trim0slice_totalcol - 1) {
				inrealendcol = inrealendcol - (trim0realiw - trimx);
			}
			else if (trim0slice_row == trim0slice_totalrow - 1) {
				inrealendrow = inrealendrow - (trim0realih - trimy);
			}

			phaseup = 0, phasedown = 0, phaseleft = 0, phaseright = 0;
			numup= 0, numdown= 0, numleft= 0, numright= 0;
			realnumup = 0, realnumdown = 0, realnumleft = 0, realnumright = 0;

			numup = cal_phase_4(trimy, configinfo->oh, deci_factor_y, inrealstartrow, &phaseup, &realnumup, configinfo->thumbnailscaler_phaseY, configinfo->thumbnailscaler_base_align);//jueduichangdu//num是deci前的多余数，算出的phase是在全frame尺寸下的值，这个是deci后iw与ow

			numdown = cal_phase_4(trimy, configinfo->oh, deci_factor_y, inrealendrow + 1, &phasedown, &realnumdown, configinfo->thumbnailscaler_phaseY, configinfo->thumbnailscaler_base_align);//

			numleft = cal_phase_4(trimx,configinfo->ow,deci_factor_x,inrealstartcol, &phaseleft, &realnumleft,configinfo->thumbnailscaler_phaseX,configinfo->thumbnailscaler_base_align);

			numright = cal_phase_4(trimx, configinfo->ow, deci_factor_x, inrealendcol + 1, &phaseright, &realnumright, configinfo->thumbnailscaler_phaseX, configinfo->thumbnailscaler_base_align);


			uvphaseup = 0, uvphasedown = 0, uvphaseleft = 0, uvphaseright = 0;
			uvnumup= 0, uvnumdown= 0, uvnumleft= 0, uvnumright = 0;
			uvrealnumup = 0, uvrealnumdown = 0, uvrealnumleft = 0, uvrealnumright = 0;

			uvnumup = cal_phase_1uv(trimy / 2, configinfo->oh / 2, deci_factor_y, inrealstartrow / 2, &uvphaseup, &uvrealnumup, realnumup, configinfo->thumbnailscaler_phaseY / 2);
			uvnumdown = cal_phase_1uv(trimy / 2, configinfo->oh / 2, deci_factor_y, (inrealendrow - 1) / 2 + 1, &uvphasedown, &uvrealnumdown, realnumdown, configinfo->thumbnailscaler_phaseY / 2);
			uvnumleft = cal_phase_1uv(trimx / 2, configinfo->ow / 2, deci_factor_x, inrealstartcol / 2, &uvphaseleft, &uvrealnumleft, realnumleft, configinfo->thumbnailscaler_phaseX / 2);
			uvnumright = cal_phase_1uv(trimx / 2, configinfo->ow / 2, deci_factor_x, (inrealendcol - 1) / 2 + 1, &uvphaseright, &uvrealnumright, realnumright, configinfo->thumbnailscaler_phaseX / 2);


			th_infophasenum->thumbinfo_phasenum_yid[id].numup = numup;
			th_infophasenum->thumbinfo_phasenum_yid[id].numdown = numdown;
			th_infophasenum->thumbinfo_phasenum_yid[id].numleft = numleft;
			th_infophasenum->thumbinfo_phasenum_yid[id].numright = numright;
			th_infophasenum->thumbinfo_phasenum_yid[id].phaseup = phaseup;
			th_infophasenum->thumbinfo_phasenum_yid[id].phasedown = phasedown;
			th_infophasenum->thumbinfo_phasenum_yid[id].phaseleft = phaseleft;
			th_infophasenum->thumbinfo_phasenum_yid[id].phaseright = phaseright;

			th_infophasenum->thumbinfo_phasenum_uvid[id].numup = uvnumup;
			th_infophasenum->thumbinfo_phasenum_uvid[id].numdown = uvnumdown;
			th_infophasenum->thumbinfo_phasenum_uvid[id].numleft = uvnumleft;
			th_infophasenum->thumbinfo_phasenum_uvid[id].numright = uvnumright;
			th_infophasenum->thumbinfo_phasenum_uvid[id].phaseup = uvphaseup;
			th_infophasenum->thumbinfo_phasenum_uvid[id].phasedown = uvphasedown;
			th_infophasenum->thumbinfo_phasenum_uvid[id].phaseleft = uvphaseleft;
			th_infophasenum->thumbinfo_phasenum_uvid[id].phaseright = uvphaseright;

			if (abs(numup) > *needup) {
				*needup = abs(numup);
			}
			if (abs(numdown) > *needdown) {
				*needdown = abs(numdown);
			}
			if (abs(numleft) > *needleft) {
				*needleft = abs(numleft);
			}
			if (abs(numright) > *needright) {
				*needright = abs(numright);
			}

			if (abs(uvnumup * 2) > *needup) {
				*needup = abs(uvnumup * 2);
			}
			if (abs(uvnumdown * 2) > *needdown) {
				*needdown = abs(uvnumdown * 2);
			}

			if (abs(uvnumleft * 2) > *needleft) {
				*needleft = abs(uvnumleft * 2);
			}
			if (abs(uvnumright * 2) > *needright) {
				*needright = abs(uvnumright * 2);
			}

			sliceInfo[id].overlap_up = needup_v;
			sliceInfo[id].overlap_down = needdown_v;
			sliceInfo[id].overlap_left = needleft_v;
			sliceInfo[id].overlap_right = needright_v;

			sliceInfo[id].overlap_up = (sliceInfo[id].overlap_up + 1)>>1<<1;
			sliceInfo[id].overlap_down = (sliceInfo[id].overlap_down + 1)>>1<<1;
			sliceInfo[id].overlap_left = (sliceInfo[id].overlap_left + 1)>>1<<1;
			sliceInfo[id].overlap_right = (sliceInfo[id].overlap_right + 1)>>1<<1;

			thumbinfo->thumbsliceinfo[id].realih = inrealendrow - inrealstartrow +1 - numup + numdown;
			thumbinfo->thumbsliceinfo[id].realiw  = inrealendcol - inrealstartcol + 1 - numleft + numright;
			thumbinfo->thumbsliceinfo[id].realoh = realnumdown - realnumup;
			thumbinfo->thumbsliceinfo[id].realow = realnumright - realnumleft;
		}
	}
}

void cal_TH_infophasenum_v2_forhw(struct TH_infophasenum* th_infophasenum, struct CONFIGINFO_T* configinfo,
		struct THUMBINFO_T* thumbinfo,int slicecols,int slicerows)
{
	uint16_t needup_v = 0;
	uint16_t needdown_v = 0;
	uint16_t needleft_v = 0;
	uint16_t needright_v = 0;

	int id = 0;
	int ii = 0, jj = 0;
	uint16_t deci_factor_x = 0, deci_factor_y = 0;
	uint16_t trimx = 0, trimy = 0;
	uint16_t cur_col = 0, cur_row = 0;
	uint16_t totalcol = 0, totalrow = 0;

	uint16_t trim0startrow = 0;
	uint16_t trim0startcol = 0;
	uint16_t trim0endrow = 0;
	uint16_t trim0endcol = 0;
	uint16_t trim0realiw = 0;
	uint16_t trim0realih = 0;
	uint16_t thumbnailscaler_trimstartrow = 0;
	uint16_t thumbnailscaler_trimstartcol = 0;
	uint16_t thumbnailscaler_trimendrow = 0;
	uint16_t thumbnailscaler_trimendcol = 0;
	uint16_t trim0slice_totalcol = 0;
	uint16_t trim0slice_totalrow = 0;
	uint16_t trim0slice_col = 0;
	uint16_t trim0slice_row = 0;
	uint16_t inrealstartrow = 0;
	uint16_t inrealstartcol = 0;
	uint16_t inrealendrow = 0;
	uint16_t inrealendcol = 0;
	uint16_t phaseup = 0, phasedown = 0, phaseleft = 0, phaseright = 0;
	int16_t numup = 0, numdown = 0, numleft = 0, numright = 0;
	uint16_t realnumup = 0, realnumdown = 0, realnumleft = 0, realnumright = 0;
	uint16_t uvphaseup = 0, uvphasedown = 0, uvphaseleft = 0, uvphaseright = 0;
	int16_t uvnumup = 0, uvnumdown = 0, uvnumleft = 0, uvnumright = 0;
	uint16_t uvrealnumup = 0, uvrealnumdown = 0, uvrealnumleft = 0, uvrealnumright = 0;

	id = 0;
	for (ii = 0; ii < slicerows; ii++) {
		for (jj = 0; jj < slicecols; jj++, id++) {
			needup_v = 0;
			needdown_v = 0;
			needleft_v = 0;
			needright_v = 0;

			if (thumbinfo->thumbsliceinfo[id].scalerswitch == 0) {
				thumbinfo->thumbsliceinfo[id].realih = 0 ;
				thumbinfo->thumbsliceinfo[id].realiw = 0;
				thumbinfo->thumbsliceinfo[id].realoh = 0;
				thumbinfo->thumbsliceinfo[id].realow = 0;
				continue;
			}

			thumbnailscaler_trimstartrow = configinfo->thumbnailscaler_trimstartrow;
			thumbnailscaler_trimstartcol = configinfo->thumbnailscaler_trimstartcol;
			thumbnailscaler_trimendrow = configinfo->thumbnailscaler_trimendrow - 1;
			thumbnailscaler_trimendcol = configinfo->thumbnailscaler_trimendcol - 1;

			trim0slice_totalcol = configinfo->trim0slice_totalcol;
			trim0slice_totalrow = configinfo->trim0slice_totalrow;
			trim0slice_col = thumbinfo->thumbsliceinfo[id].trim0slice_col;
			trim0slice_row = thumbinfo->thumbsliceinfo[id].trim0slice_row;

			trim0startrow = thumbinfo->thumbsliceinfo[id].trim0startrow;
			trim0startcol = thumbinfo->thumbsliceinfo[id].trim0startcol;
			trim0endrow = thumbinfo->thumbsliceinfo[id].trim0endrow;
			trim0endcol = thumbinfo->thumbsliceinfo[id].trim0endcol;

			trim0realiw = thumbinfo->thumbsliceinfo[id].trim0realiw;
			trim0realih = thumbinfo->thumbsliceinfo[id].trim0realih;

			inrealstartrow = trim0startrow - thumbnailscaler_trimstartrow;
			inrealstartcol = trim0startcol - thumbnailscaler_trimstartcol;
			inrealendrow = trim0endrow - thumbnailscaler_trimstartrow;
			inrealendcol = trim0endcol - thumbnailscaler_trimstartcol;

			deci_factor_x = thumbinfo->thumbsliceinfo[id].deci_factor_x;
			deci_factor_y = thumbinfo->thumbsliceinfo[id].deci_factor_y;
			trimx = thumbinfo->thumbsliceinfo[id].trimx;
			trimy = thumbinfo->thumbsliceinfo[id].trimy;
			cur_col = thumbinfo->thumbsliceinfo[id].cur_col;
			cur_row = thumbinfo->thumbsliceinfo[id].cur_row;
			totalcol = thumbinfo->thumbsliceinfo[id].totalcol;
			totalrow = thumbinfo->thumbsliceinfo[id].totalrow;

			if ((trim0slice_row == trim0slice_totalrow - 1) && (trim0slice_col == trim0slice_totalcol - 1)) {
				inrealendcol = inrealendcol - (trim0realiw - trimx);
				inrealendrow = inrealendrow - (trim0realih - trimy);
			} else if (trim0slice_col == trim0slice_totalcol - 1)
				inrealendcol = inrealendcol - (trim0realiw - trimx);
			else if (trim0slice_row == trim0slice_totalrow - 1)
				inrealendrow = inrealendrow - (trim0realih - trimy);

			numup = cal_phase_4(trimy, configinfo->oh, deci_factor_y, inrealstartrow, &phaseup, &realnumup, configinfo->thumbnailscaler_phaseY, configinfo->thumbnailscaler_base_align);
			numdown = cal_phase_4(trimy, configinfo->oh, deci_factor_y, inrealendrow + 1, &phasedown, &realnumdown, configinfo->thumbnailscaler_phaseY, configinfo->thumbnailscaler_base_align);
			numleft = cal_phase_4(trimx, configinfo->ow, deci_factor_x, inrealstartcol, &phaseleft, &realnumleft, configinfo->thumbnailscaler_phaseX, configinfo->thumbnailscaler_base_align);
			numright = cal_phase_4(trimx, configinfo->ow, deci_factor_x, inrealendcol + 1, &phaseright, &realnumright, configinfo->thumbnailscaler_phaseX, configinfo->thumbnailscaler_base_align);

			uvnumup = cal_phase_1uv(trimy / 2, configinfo->oh / 2, deci_factor_y, inrealstartrow / 2, &uvphaseup, &uvrealnumup, realnumup, configinfo->thumbnailscaler_phaseY / 2);
			uvnumdown = cal_phase_1uv(trimy / 2, configinfo->oh / 2, deci_factor_y, (inrealendrow - 1) / 2 + 1, &uvphasedown, &uvrealnumdown, realnumdown, configinfo->thumbnailscaler_phaseY / 2);
			uvnumleft = cal_phase_1uv(trimx / 2, configinfo->ow / 2, deci_factor_x, inrealstartcol / 2, &uvphaseleft, &uvrealnumleft, realnumleft, configinfo->thumbnailscaler_phaseX / 2);
			uvnumright = cal_phase_1uv(trimx / 2, configinfo->ow / 2, deci_factor_x, (inrealendcol - 1) / 2 + 1, &uvphaseright, &uvrealnumright, realnumright, configinfo->thumbnailscaler_phaseX / 2);

			th_infophasenum->thumbinfo_phasenum_yid[id].numup = numup;
			th_infophasenum->thumbinfo_phasenum_yid[id].numdown = numdown;
			th_infophasenum->thumbinfo_phasenum_yid[id].numleft = numleft;
			th_infophasenum->thumbinfo_phasenum_yid[id].numright = numright;
			th_infophasenum->thumbinfo_phasenum_yid[id].phaseup = phaseup;
			th_infophasenum->thumbinfo_phasenum_yid[id].phasedown = phasedown;
			th_infophasenum->thumbinfo_phasenum_yid[id].phaseleft = phaseleft;
			th_infophasenum->thumbinfo_phasenum_yid[id].phaseright = phaseright;
			pr_debug("left %d, right %d\n", phaseleft, phaseright);

			th_infophasenum->thumbinfo_phasenum_uvid[id].numup = uvnumup;
			th_infophasenum->thumbinfo_phasenum_uvid[id].numdown = uvnumdown;
			th_infophasenum->thumbinfo_phasenum_uvid[id].numleft = uvnumleft;
			th_infophasenum->thumbinfo_phasenum_uvid[id].numright = uvnumright;
			th_infophasenum->thumbinfo_phasenum_uvid[id].phaseup = uvphaseup;
			th_infophasenum->thumbinfo_phasenum_uvid[id].phasedown = uvphasedown;
			th_infophasenum->thumbinfo_phasenum_uvid[id].phaseleft = uvphaseleft;
			th_infophasenum->thumbinfo_phasenum_uvid[id].phaseright = uvphaseright;
			pr_debug("left %d, right %d\n", uvphaseleft, uvphaseright);

			thumbinfo->thumbsliceinfo[id].realih = inrealendrow - inrealstartrow + 1 - numup + numdown;
			thumbinfo->thumbsliceinfo[id].realiw = inrealendcol - inrealstartcol + 1 - numleft + numright;
			thumbinfo->thumbsliceinfo[id].realoh = realnumdown - realnumup;
			thumbinfo->thumbsliceinfo[id].realow = realnumright - realnumleft;
		}
	}
}

void thumbnailscaler_calculate_region(
	const struct alg_slice_regions *r_ref,
	struct alg_slice_regions *r_out,
	struct thumbnailscaler_param_t *thumbnailscaler_param_ptr,
	struct thumbnailscaler_context *pipe_context_ptr,
	int v_flag)
{
	int ii,jj,rows,cols;
	int frameW = pipe_context_ptr->src_width;
	int FrameH = pipe_context_ptr->src_height;
	int slicecols = r_ref->cols;
	int slicerows = r_ref->rows;
	struct THUMB_SLICE_PARAM_T thumbslice_param_obj;
	struct THUMB_SLICE_PARAM_T *thumbslice_param;
	int id =0;
	struct thumbscaler_info thumbscaler_param_temp = {0};
	struct THUMB_SLICE_PARAM_T *sliceInfo = (struct THUMB_SLICE_PARAM_T *)vzalloc(sizeof(struct THUMB_SLICE_PARAM_T) * slicerows * slicecols);

	isp_drv_regions_set(r_ref,r_out);

	memset(&thumbslice_param_obj,0,sizeof(struct THUMB_SLICE_PARAM_T));
	thumbslice_param = &thumbslice_param_obj;
	rows = slicerows;
	cols = slicecols;
	thumbnailscaler_param_ptr->configinfo.iw = frameW;
	thumbnailscaler_param_ptr->configinfo.ih = FrameH;

	//set input
	memcpy(&thumbscaler_param_temp.configinfo, &thumbnailscaler_param_ptr->configinfo, sizeof(struct CONFIGINFO_T));

	for (ii = 0; ii < rows; ii++) {
		for ( jj = 0; jj < cols; jj++, id++) {
			thumbslice_param->id = id;
			thumbslice_param->width = pipe_context_ptr->offlineSliceWidth;
			thumbslice_param->height = pipe_context_ptr->offlineSliceHeight;
			thumbslice_param->start_col = r_ref->regions[id].sx;
			thumbslice_param->end_col = r_ref->regions[id].ex;
			thumbslice_param->start_row = r_ref->regions[id].sy;
			thumbslice_param->end_row = r_ref->regions[id].ey;
			thumbslice_param->overlap_left = 0;
			thumbslice_param->overlap_right = 0;
			thumbslice_param->overlap_up = 0;
			thumbslice_param->overlap_down = 0;

			/* thumbInitSliceInfo(thumbnailscaler_param_ptr,jj,ii,cols,rows,thumbslice_param); */
			thumbInitSliceInfo_forhw(&thumbscaler_param_temp, jj, ii, cols, rows, thumbslice_param);
		}
	}

	/* get output */
	/* memcpy(&thumbnailscaler_param_ptr->th_infophasenum, &thumbscaler_param_temp.th_infophasenum, sizeof(TH_infophasenum)); */
	memcpy(&thumbnailscaler_param_ptr->configinfo, &thumbscaler_param_temp.configinfo, sizeof(struct CONFIGINFO_T));
	memcpy(&thumbnailscaler_param_ptr->thumbinfo, &thumbscaler_param_temp.thumbinfo, sizeof(struct THUMBINFO_T));

	cal_TH_infophasenum_v2(
		&thumbnailscaler_param_ptr->th_infophasenum,
		&thumbnailscaler_param_ptr->configinfo,
		&thumbnailscaler_param_ptr->thumbinfo,
		slicecols,
		slicerows,
		sliceInfo);

	id = 0;
	for ( ii = 0; ii < rows; ii++) {
		for ( jj = 0; jj < cols; jj++,id++) {
			r_out->regions[id].sy -= sliceInfo[id].overlap_up;
			r_out->regions[id].ey += sliceInfo[id].overlap_down;
			r_out->regions[id].sx -= sliceInfo[id].overlap_left;
			r_out->regions[id].ex += sliceInfo[id].overlap_right;

			if (r_out->regions[id].sy < 0)
				r_out->regions[id].sy = 0;

			if (r_out->regions[id].ey > FrameH-1)
				r_out->regions[id].ey = FrameH-1;

			if (r_out->regions[id].sx < 0)
				r_out->regions[id].sx = 0;

			if (r_out->regions[id].ex > frameW-1)
				r_out->regions[id].ex = frameW-1;
		}
	}

	r_out->rows = rows;
	r_out->cols = cols;

	vfree(sliceInfo);
}

void method_firmware_for_driver(struct thumbscaler_this *thumbnail_scaler,
	struct thumbscaler_info *thumbnail_scaler_lite)
{

	struct THUMB_SLICE_PARAM_T thumbslice_param_obj = {0};
	struct THUMB_SLICE_PARAM_T *thumbslice_param = NULL;
	int id = 0, ii = 0, jj = 0, i = 0;
	int slicecols = 0, slicerows = 0, sumslice = 0;
	struct THUMB_SLICE_PARAM_T sliceInfo_obj = {0}, *sliceInfo = NULL;

	thumbslice_param = &thumbslice_param_obj;
	sliceInfo = &sliceInfo_obj;
	slicecols = thumbnail_scaler->inputSliceList.cols;
	slicerows = thumbnail_scaler->inputSliceList.rows;
	sumslice = thumbnail_scaler->sumslice;

	for (ii = 0; ii < thumbnail_scaler->inputSliceList.rows; ii++)
	{
		for (jj = 0; jj < thumbnail_scaler->inputSliceList.cols; jj++,id++)
		{
			thumbslice_param->id = id;
			thumbslice_param->width = thumbnail_scaler->inputSliceList.offlineSliceWidth;
			thumbslice_param->height = thumbnail_scaler->inputSliceList.offlineSliceHeight;
			thumbslice_param->start_col = thumbnail_scaler->inputSliceList.slices[id].s_col;
			thumbslice_param->end_col = thumbnail_scaler->inputSliceList.slices[id].e_col;
			thumbslice_param->start_row = thumbnail_scaler->inputSliceList.slices[id].s_row;
			thumbslice_param->end_row = thumbnail_scaler->inputSliceList.slices[id].e_row;
			thumbslice_param->overlap_left = 0;
			thumbslice_param->overlap_right = 0;
			thumbslice_param->overlap_up = 0;
			thumbslice_param->overlap_down = 0;
			thumbInitSliceInfo_forhw(thumbnail_scaler_lite, jj, ii, thumbnail_scaler->inputSliceList.cols, thumbnail_scaler->inputSliceList.rows, thumbslice_param);
		}
	}

	cal_TH_infophasenum_v2_forhw(&thumbnail_scaler->th_infophasenum, &thumbnail_scaler_lite->configinfo, &thumbnail_scaler_lite->thumbinfo, slicecols, slicerows);

	id = 0;
	for (i = 0; i < sumslice; id++, i++) {
		if (thumbnail_scaler_lite->thumbinfo.thumbsliceinfo[id].scalerswitch == 0)
			continue;
		thumbslice_param->id = id;
		thumbslice_param->width = thumbnail_scaler->inputSliceList_overlap.slices[id].e_col - thumbnail_scaler->inputSliceList_overlap.slices[id].s_col + 1;
		thumbslice_param->height = thumbnail_scaler->inputSliceList_overlap.slices[id].e_row - thumbnail_scaler->inputSliceList_overlap.slices[id].s_row + 1;
		thumbslice_param->start_col = thumbnail_scaler->inputSliceList_overlap.slices[id].s_col;
		thumbslice_param->end_col = thumbnail_scaler->inputSliceList_overlap.slices[id].e_col;
		thumbslice_param->start_row = thumbnail_scaler->inputSliceList_overlap.slices[id].s_row;
		thumbslice_param->end_row = thumbnail_scaler->inputSliceList_overlap.slices[id].e_row;
		thumbslice_param->overlap_left = thumbnail_scaler->inputSliceList_overlap.slices[id].overlap_left;
		thumbslice_param->overlap_right = thumbnail_scaler->inputSliceList_overlap.slices[id].overlap_right;
		thumbslice_param->overlap_up = thumbnail_scaler->inputSliceList_overlap.slices[id].overlap_up;
		thumbslice_param->overlap_down = thumbnail_scaler->inputSliceList_overlap.slices[id].overlap_down;

		cal_trimcoordinate(thumbslice_param, &thumbnail_scaler_lite->configinfo, &thumbnail_scaler_lite->thumbinfo, &thumbnail_scaler->th_infophasenum);
	}
}

void thumbnailscaler_slice_init(struct alg_slice_drv_overlap *param_ptr)
{
	struct thumbscaler_this *thumbnail_scaler = NULL;
	struct thumbscaler_info *thumbnail_scaler_lite = NULL;
	struct CONFIGINFO_T *configinfo = NULL;
	struct Sliceinfo *slice = NULL, *slice_overlap = NULL;
	struct alg_region_info *slice_pos_array = NULL;
	struct alg_overlap_info *slice_overlap_array = NULL;
	struct slice_drv_overlap_thumbnail_scaler_param* thumbnailscaler = NULL;
	uint32_t deci_factor_x = 0, deci_factor_y = 0;
	uint32_t start_col = 0, start_row = 0, end_col = 0, end_row = 0;
	uint32_t overlap_left = 0, overlap_right = 0, overlap_up = 0, overlap_down = 0;
	uint32_t trimx = 0, trimy = 0;
	uint32_t slice_id = 0, c= 0, r = 0, sumslice = 0;
	struct thumbnailscaler_slice slice_param[PIPE_MAX_SLICE_NUM] = {0};

	thumbnail_scaler = &param_ptr->thumbnail_scaler;
	thumbnail_scaler_lite = &param_ptr->thumbnail_scaler_lite;
	thumbnailscaler = &param_ptr->thumbnailscaler;
	configinfo = &thumbnail_scaler_lite->configinfo;
	slice = &thumbnail_scaler->inputSliceList;
	slice_overlap = &thumbnail_scaler->inputSliceList_overlap;
	slice_pos_array = param_ptr->slice_region;
	slice_overlap_array = param_ptr->slice_overlap;

	sumslice = thumbnailscaler->slice_num;
	thumbnail_scaler->sumslice = sumslice;

	configinfo->thumbnailscaler_trim0_en = thumbnailscaler->trim0_en;
	configinfo->thumbnailscaler_trimstartcol = thumbnailscaler->trim0_start_x;
	configinfo->thumbnailscaler_trimstartrow = thumbnailscaler->trim0_start_y;
	configinfo->thumbnailscaler_trimsizeX = thumbnailscaler->trim0_size_x;
	configinfo->thumbnailscaler_trimsizeY = thumbnailscaler->trim0_size_y;
	configinfo->thumbnailscaler_phaseX = thumbnailscaler->phase_x;
	configinfo->thumbnailscaler_phaseY = thumbnailscaler->phase_y;
	configinfo->thumbnailscaler_base_align = thumbnailscaler->base_align;
	configinfo->ow = thumbnailscaler->out_w;
	configinfo->oh = thumbnailscaler->out_h;
	configinfo->iw = param_ptr->img_w;
	configinfo->ih = param_ptr->img_h;
	configinfo->outformat = thumbnailscaler->out_format;

	slice->rows = param_ptr->slice_rows;
	slice->cols = param_ptr->slice_cols;
	slice->offlineSliceWidth = param_ptr->slice_w;
	slice->offlineSliceHeight = param_ptr->slice_h;
	slice_overlap->rows = param_ptr->slice_rows;
	slice_overlap->cols = param_ptr->slice_cols;
	slice_overlap->offlineSliceWidth = param_ptr->slice_w;
	slice_overlap->offlineSliceHeight = param_ptr->slice_h;

	if (param_ptr->slice_h > param_ptr->img_h)
		param_ptr->slice_h = param_ptr->img_h;
	if (param_ptr->slice_w > param_ptr->img_w)
		param_ptr->slice_w = param_ptr->img_w;

	for (r = 0; r < slice->rows; r++) {
		for (c = 0; c < slice->cols; c++, slice_id++) {
			start_col = slice_pos_array[slice_id].sx;
			start_row = slice_pos_array[slice_id].sy;
			end_col = slice_pos_array[slice_id].ex;
			end_row = slice_pos_array[slice_id].ey;
			overlap_left = slice_overlap_array[slice_id].ov_left;
			overlap_up = slice_overlap_array[slice_id].ov_up;
			overlap_right = slice_overlap_array[slice_id].ov_right;
			overlap_down = slice_overlap_array[slice_id].ov_down;

			slice_param[slice_id].slice_size_before_trim_hor = end_col - start_col + 1;
			slice_param[slice_id].slice_size_before_trim_ver = end_row - start_row + 1;

			slice_overlap->slices[slice_id].s_row = start_row;
			slice_overlap->slices[slice_id].e_row = end_row;
			slice_overlap->slices[slice_id].s_col = start_col;
			slice_overlap->slices[slice_id].e_col = end_col;
			slice_overlap->slices[slice_id].overlap_left = overlap_left;
			slice_overlap->slices[slice_id].overlap_up = overlap_up;
			slice_overlap->slices[slice_id].overlap_right = overlap_right;
			slice_overlap->slices[slice_id].overlap_down = overlap_down;

			slice->slices[slice_id].s_col = start_col + overlap_left;
			slice->slices[slice_id].s_row = start_row + overlap_up;
			slice->slices[slice_id].e_col = end_col - overlap_right;
			slice->slices[slice_id].e_row = end_row - overlap_down;
			slice->slices[slice_id].overlap_left = 0;
			slice->slices[slice_id].overlap_right = 0;
			slice->slices[slice_id].overlap_up = 0;
			slice->slices[slice_id].overlap_down = 0;
		}
	}

	method_firmware_for_driver(thumbnail_scaler,thumbnail_scaler_lite);

	deci_factor_x = thumbnail_scaler_lite->thumbinfo.thumbsliceinfo[0].deci_factor_x;
	deci_factor_y = thumbnail_scaler_lite->thumbinfo.thumbsliceinfo[0].deci_factor_y;
	trimx = thumbnail_scaler_lite->thumbinfo.thumbsliceinfo[0].trimx;
	trimy = thumbnail_scaler_lite->thumbinfo.thumbsliceinfo[0].trimy;
	for (slice_id = 0; slice_id< thumbnail_scaler->sumslice; slice_id++) {
		slice_param[slice_id].bypass = 1 - thumbnail_scaler_lite->thumbinfo.thumbsliceinfo[slice_id].scalerswitch;
		slice_param[slice_id].y_trim0_start_hor = thumbnail_scaler->th_infophasenum.thumbinfo_trimcoordinate_yid[slice_id].trim_s_col;
		slice_param[slice_id].y_trim0_start_ver = thumbnail_scaler->th_infophasenum.thumbinfo_trimcoordinate_yid[slice_id].trim_s_row;
		slice_param[slice_id].y_trim0_size_hor = thumbnail_scaler->th_infophasenum.thumbinfo_trimcoordinate_yid[slice_id].trim_width;
		slice_param[slice_id].y_trim0_size_ver = thumbnail_scaler->th_infophasenum.thumbinfo_trimcoordinate_yid[slice_id].trim_height;
		slice_param[slice_id].uv_trim0_start_hor = thumbnail_scaler->th_infophasenum.thumbinfo_trimcoordinate_uvid[slice_id].trim_s_col;
		slice_param[slice_id].uv_trim0_start_ver = thumbnail_scaler->th_infophasenum.thumbinfo_trimcoordinate_uvid[slice_id].trim_s_row;
		slice_param[slice_id].uv_trim0_size_hor = thumbnail_scaler->th_infophasenum.thumbinfo_trimcoordinate_uvid[slice_id].trim_width;
		slice_param[slice_id].uv_trim0_size_ver = thumbnail_scaler->th_infophasenum.thumbinfo_trimcoordinate_uvid[slice_id].trim_height;
		slice_param[slice_id].y_init_phase_hor = thumbnail_scaler->th_infophasenum.thumbinfo_phasenum_yid[slice_id].phaseleft;
		slice_param[slice_id].y_init_phase_ver = thumbnail_scaler->th_infophasenum.thumbinfo_phasenum_yid[slice_id].phaseup;
		slice_param[slice_id].uv_init_phase_hor = thumbnail_scaler->th_infophasenum.thumbinfo_phasenum_uvid[slice_id].phaseleft;
		slice_param[slice_id].uv_init_phase_ver = thumbnail_scaler->th_infophasenum.thumbinfo_phasenum_uvid[slice_id].phaseup;
		slice_param[slice_id].y_slice_src_size_hor = thumbnail_scaler_lite->thumbinfo.thumbsliceinfo[slice_id].realiw / deci_factor_x;
		slice_param[slice_id].y_slice_src_size_ver = thumbnail_scaler_lite->thumbinfo.thumbsliceinfo[slice_id].realih / deci_factor_y;
		slice_param[slice_id].y_slice_des_size_hor = thumbnail_scaler_lite->thumbinfo.thumbsliceinfo[slice_id].realow;
		slice_param[slice_id].y_slice_des_size_ver = thumbnail_scaler_lite->thumbinfo.thumbsliceinfo[slice_id].realoh;
		slice_param[slice_id].uv_slice_src_size_hor = thumbnail_scaler_lite->thumbinfo.thumbsliceinfo[slice_id].realiw / (deci_factor_x * 2);
		slice_param[slice_id].uv_slice_src_size_ver = thumbnail_scaler_lite->thumbinfo.thumbsliceinfo[slice_id].realih / deci_factor_y;
		if (thumbnailscaler->out_format == 0) {
			slice_param[slice_id].uv_slice_des_size_hor = thumbnail_scaler_lite->thumbinfo.thumbsliceinfo[slice_id].realow / 2;
			slice_param[slice_id].uv_slice_des_size_ver = thumbnail_scaler_lite->thumbinfo.thumbsliceinfo[slice_id].realoh;
		} else if (thumbnailscaler->out_format == 1) {
			slice_param[slice_id].uv_slice_des_size_hor = thumbnail_scaler_lite->thumbinfo.thumbsliceinfo[slice_id].realow / 2;
			slice_param[slice_id].uv_slice_des_size_ver = thumbnail_scaler_lite->thumbinfo.thumbsliceinfo[slice_id].realoh / 2;
		} else {
			slice_param[slice_id].uv_slice_des_size_hor = 0;
			slice_param[slice_id].uv_slice_des_size_ver = 0;
		}

		pr_debug("slice_id %d bypass %d  y trim0(%d %d %d %d) uv trim0(%d %d %d %d) y init(%d %d) uv init(%d %d)\n",
			slice_id, slice_param[slice_id].bypass, slice_param[slice_id].y_trim0_start_hor, slice_param[slice_id].y_trim0_start_ver,
			slice_param[slice_id].y_trim0_size_hor, slice_param[slice_id].y_trim0_size_ver, slice_param[slice_id].uv_trim0_start_hor,
			slice_param[slice_id].uv_trim0_start_ver, slice_param[slice_id].uv_trim0_size_hor, slice_param[slice_id].uv_trim0_size_ver,
			slice_param[slice_id].y_init_phase_hor, slice_param[slice_id].y_init_phase_ver, slice_param[slice_id].uv_init_phase_hor,
			slice_param[slice_id].uv_init_phase_ver);
		pr_debug("y src(%d %d) dst(%d %d) uv src(%d %d) dst(%d %d)\n",
			slice_param[slice_id].y_slice_src_size_hor,slice_param[slice_id].y_slice_src_size_ver, slice_param[slice_id].y_slice_des_size_hor,
			slice_param[slice_id].y_slice_des_size_ver,slice_param[slice_id].uv_slice_src_size_hor, slice_param[slice_id].uv_slice_src_size_ver,
			slice_param[slice_id].uv_slice_des_size_hor, slice_param[slice_id].uv_slice_des_size_ver);

		thumbnail_scaler->sliceParam[slice_id] = slice_param[slice_id];
	}

	if (deci_factor_x == 1) {
		thumbnailscaler->y_deci_hor_en = 0;
		thumbnailscaler->uv_deci_hor_en = 0;
		thumbnailscaler->y_deci_hor_par = 0;
		thumbnailscaler->uv_deci_hor_par = 0;
	} else {
		thumbnailscaler->y_deci_hor_en = 1;
		thumbnailscaler->uv_deci_hor_en = 1;
		if (deci_factor_x == 2)
			thumbnailscaler->y_deci_hor_par = 0;
		if (deci_factor_x == 4)
			thumbnailscaler->y_deci_hor_par = 1;
		if (deci_factor_x == 8)
			thumbnailscaler->y_deci_hor_par = 2;
		if (deci_factor_x == 16)
			thumbnailscaler->y_deci_hor_par = 3;
		thumbnailscaler->uv_deci_hor_par = thumbnailscaler->y_deci_hor_par;
	}
	if (deci_factor_y == 1) {
		thumbnailscaler->y_deci_ver_en = 0;
		thumbnailscaler->uv_deci_ver_en = 0;
		thumbnailscaler->y_deci_ver_par = 0;
		thumbnailscaler->uv_deci_ver_par = 0;
	} else {
		thumbnailscaler->y_deci_ver_en = 1;
		thumbnailscaler->uv_deci_ver_en = 1;
		if (deci_factor_y == 2)
			thumbnailscaler->y_deci_ver_par = 0;
		if (deci_factor_y == 4)
			thumbnailscaler->y_deci_ver_par = 1;
		if (deci_factor_y == 8)
			thumbnailscaler->y_deci_ver_par = 2;
		if (deci_factor_y == 16)
			thumbnailscaler->y_deci_ver_par = 3;
		thumbnailscaler->uv_deci_ver_par = thumbnailscaler->y_deci_ver_par;
	}

	thumbnailscaler->y_frame_src_size_hor = trimx / deci_factor_x;
	thumbnailscaler->y_frame_src_size_ver = trimy / deci_factor_y;
	thumbnailscaler->uv_frame_src_size_hor = trimx / (deci_factor_x * 2);
	thumbnailscaler->uv_frame_src_size_ver = trimy / (deci_factor_y * 2);
	thumbnailscaler->y_frame_des_size_hor = thumbnailscaler->out_w;
	thumbnailscaler->y_frame_des_size_ver = thumbnailscaler->out_h;

	if (thumbnailscaler->out_format == 0) {
		thumbnailscaler->uv_frame_des_size_hor = thumbnailscaler->out_w / 2;
		thumbnailscaler->uv_frame_des_size_ver = thumbnailscaler->out_h;
	} else if (thumbnailscaler->out_format == 1) {
		thumbnailscaler->uv_frame_des_size_hor = thumbnailscaler->out_w / 2;
		thumbnailscaler->uv_frame_des_size_ver = thumbnailscaler->out_h / 2;
	} else {
		thumbnailscaler->uv_frame_des_size_hor = 0;
		thumbnailscaler->uv_frame_des_size_ver = 0;
	}
}

static void scaler_slice_init(const struct alg_region_info *input_slice_region, struct slice_drv_scaler_slice_init_context *context,
		struct alg_slice_scaler_overlap *scaler_param_ptr)
{
	struct scaler_slice_t scaler_slice;
	struct scaler_slice_t input_slice_info  = {0};
	struct scaler_slice_t output_slice_info = {0};
	int slice_index = context->slice_index;
	int rows = context->rows;
	int cols = context->cols;
	int slice_row_no = context->slice_row_no;
	int slice_col_no = context->slice_col_no;
	struct yuvscaler_param_t *frame_param;
	struct yuvscaler_param_t *slice_param;

	frame_param = (struct yuvscaler_param_t*)scaler_param_ptr->frameParam;
	slice_param = (struct yuvscaler_param_t *)scaler_param_ptr->sliceParam + slice_index;

	scaler_slice.slice_id = slice_index;
	scaler_slice.start_col = input_slice_region->sx;
	scaler_slice.start_row = input_slice_region->sy;
	scaler_slice.end_col = input_slice_region->ex;
	scaler_slice.end_row = input_slice_region->ey;
	scaler_slice.sliceRows = rows;
	scaler_slice.sliceCols = cols;
	scaler_slice.sliceRowNo = slice_row_no;
	scaler_slice.sliceColNo = slice_col_no;
	scaler_slice.slice_width = context->slice_w;
	scaler_slice.slice_height = context->slice_h;
	scaler_slice.overlap_left = 0;
	scaler_slice.overlap_right = 0;
	scaler_slice.overlap_up = 0;
	scaler_slice.overlap_down = 0;

	scaler_slice.init_phase_hor = scaler_param_ptr->phase[slice_index].init_phase_hor;
	scaler_slice.init_phase_ver = scaler_param_ptr->phase[slice_index].init_phase_ver;

	input_slice_info.start_col = scaler_param_ptr->region_input[slice_index].sx;
	input_slice_info.end_col = scaler_param_ptr->region_input[slice_index].ex;
	input_slice_info.start_row = scaler_param_ptr->region_input[slice_index].sy;
	input_slice_info.end_row = scaler_param_ptr->region_input[slice_index].ey;

	output_slice_info.start_col = scaler_param_ptr->region_output[slice_index].sx;
	output_slice_info.end_col = scaler_param_ptr->region_output[slice_index].ex;
	output_slice_info.start_row = scaler_param_ptr->region_output[slice_index].sy;
	output_slice_info.end_row = scaler_param_ptr->region_output[slice_index].ey;

	yuv_scaler_init_slice_info_v3(
		frame_param,
		slice_param,
		&scaler_slice,
		&input_slice_info,
		&output_slice_info);
}

//input:    输入图像(frame)的pixel_alignment, trim_start, trim_size, decimation, scaler setting;
//          输入图像中的slice位置, 输出图像的pixel对齐要求
//output:   输出图像中的slice位置(如果输入图像slice位置与trim窗口没有重叠区域，则没有对应的输出，此时返回的output_slice_size = 0)
void calc_scaler_output_slice_info(int trim_start, int trim_size, int deci,
	int scl_en, int scl_factor_in, int scl_factor_out, int scl_tap, int init_phase,
	int input_slice_start, int input_slice_size, int output_pixel_align,
	int *output_slice_start, int *output_slice_size)
{
	int spixel, epixel;
	int deci_size = trim_size / deci;
	int input_slice_end = input_slice_start + input_slice_size;
	int trim_end = trim_start + trim_size;

	if (scl_tap % 2 != 0)
		pr_warn("warning: get valid scl_tap %d\n", scl_tap);
	if (trim_size % deci != 0)
		pr_warn("warning: get valid trim_size %d\n", trim_size);

	//trim
	input_slice_start = input_slice_start < trim_start ? trim_start : input_slice_start;
	input_slice_start = input_slice_start - trim_start;

	//deci
	input_slice_start = (input_slice_start + deci - 1) / deci;
	if (input_slice_start != 0)
	input_slice_start += scl_tap/2 - 1;

	//trim
	input_slice_end = input_slice_end > trim_end ? trim_end : input_slice_end;
	input_slice_end = input_slice_end - trim_start;

	//deci
	input_slice_end = input_slice_end / deci;
	if (input_slice_end != deci_size)
	input_slice_end -= scl_tap / 2;

	//scale
	spixel = (input_slice_start * scl_factor_out - init_phase + scl_factor_in - 1) / scl_factor_in;
	epixel = (input_slice_end * scl_factor_out - 1 - init_phase) / scl_factor_in + 1;

	//align
	spixel = ((spixel + output_pixel_align - 1) / output_pixel_align) * output_pixel_align;
	epixel = (epixel / output_pixel_align) * output_pixel_align;

	//output
	*output_slice_start = spixel;
	*output_slice_size = epixel > spixel ? epixel - spixel : 0;
}

void calc_scaler_output_slice_info_v2(int trim_start, int trim_size, int deci,
	int scl_en, int scl_factor_in, int scl_factor_out, int scl_tap, int init_phase,
    int input_slice_start, int input_slice_size, int output_pixel_align,
    int *output_slice_start, int *output_slice_size)
{
	int spixel, epixel;
	int input_slice_end = input_slice_start + input_slice_size;
	int trim_end = trim_start + trim_size;

	if (scl_tap % 2 != 0)
		pr_warn("warning: get valid scl_tap %d\n", scl_tap);
	if (trim_size % deci != 0)
		pr_warn("warning: get valid trim_size %d\n", trim_size);

	//trim
	input_slice_start = input_slice_start < trim_start ? trim_start : input_slice_start;
	input_slice_start = input_slice_start - trim_start;

	//deci
	input_slice_start = (input_slice_start + deci - 1) / deci;

	//trim
	input_slice_end = input_slice_end > trim_end ? trim_end : input_slice_end;
	input_slice_end = input_slice_end - trim_start;

	//deci
	input_slice_end = input_slice_end / deci;

	//scale
	spixel = (input_slice_start * scl_factor_out - init_phase) / scl_factor_in;
	//epixel = ((input_slice_end - 1)*scl_factor_out - init_phase)/scl_factor_in + 1;
	epixel = (input_slice_end * scl_factor_out - 1 - init_phase) / scl_factor_in + 1;

	//align
	spixel = ((spixel + output_pixel_align - 1) / output_pixel_align) * output_pixel_align;
	epixel = ((epixel + output_pixel_align - 1) / output_pixel_align) * output_pixel_align;

	//output
	*output_slice_start = spixel;
	*output_slice_size = epixel > spixel ? epixel - spixel : 0;
}

static int trim0_resize(uint8_t deci_factor, uint16_t trim_start,
	uint16_t trim_size, uint8_t trim0_align, uint16_t src_size)
{
	uint16_t trim_size_temp, trim_size_new;
	int align_rmd;

	trim_size_temp = trim_size;
	align_rmd = trim_size_temp % (deci_factor * trim0_align);

	if (align_rmd != 0) {
		trim_size += deci_factor * trim0_align - align_rmd;

		if ((trim_start + trim_size) > src_size)
			trim_size = trim_size_temp - align_rmd;
	}

	if (trim_size % (deci_factor * trim0_align) != 0)
		pr_err("fail to get valid trim size %d\n", trim_size);
	trim_size_new = trim_size;

	return trim_size_new;
}


static void yuv_scaler_init_frame_info(struct yuvscaler_param_t *pYuvScaler)
{
	struct scaler_info_t *pScalerInfo = NULL;
	uint16_t new_width = pYuvScaler->src_size_x;
	uint16_t new_height = pYuvScaler->src_size_y;
	int adj_hor = 1;
	int adj_ver = 1;
	uint8_t trim0_align;

	if (!pYuvScaler->bypass) {
		//init deci info
		if (pYuvScaler->deci_info.deci_x_en == 0) {
			pYuvScaler->deci_info.deci_x = 1;
			pYuvScaler->deci_info.deciPhase_X = 0;
		}

		if (pYuvScaler->deci_info.deci_y_en == 0) {
			pYuvScaler->deci_info.deci_y = 1;
			pYuvScaler->deci_info.deciPhase_Y = 0;
		}

		/* //////////////////////////////// trim0 align ////////////////////////////////////// */
		trim0_align = 2;/*algorithm requirement, scaler needs 2 aligned input size */
		if (pYuvScaler->trim0_info.trim_start_x % 2 != 0)
			pr_err("fail to align trim start x%d\n", pYuvScaler->trim0_info.trim_start_x);
		if (pYuvScaler->trim0_info.trim_start_y % 2 != 0)
			pr_err("fail to align trim start y%d\n", pYuvScaler->trim0_info.trim_start_y);

		pYuvScaler->trim0_info.trim_size_x = trim0_resize(pYuvScaler->deci_info.deci_x,
			pYuvScaler->trim0_info.trim_start_x, pYuvScaler->trim0_info.trim_size_x,
			trim0_align, pYuvScaler->src_size_x);
		pYuvScaler->trim0_info.trim_size_y = trim0_resize(pYuvScaler->deci_info.deci_y,
			pYuvScaler->trim0_info.trim_start_y, pYuvScaler->trim0_info.trim_size_y,
			trim0_align, pYuvScaler->src_size_y);

		if (pYuvScaler->deci_info.deci_x_en)
			new_width = pYuvScaler->trim0_info.trim_size_x / pYuvScaler->deci_info.deci_x;
		else
			new_width = pYuvScaler->trim0_info.trim_size_x;

		if (pYuvScaler->deci_info.deci_y_en)
			new_height = pYuvScaler->trim0_info.trim_size_y / pYuvScaler->deci_info.deci_y;
		else
			new_height = pYuvScaler->trim0_info.trim_size_y;

		pScalerInfo = &pYuvScaler->scaler_info;
		pScalerInfo->scaler_in_width = new_width;
		pScalerInfo->scaler_in_height= new_height;

		if (pScalerInfo->scaler_en) {
			int32_t scl_init_phase_hor, scl_init_phase_ver;
			uint16_t scl_factor_in_hor, scl_factor_out_hor;
			uint16_t scl_factor_in_ver, scl_factor_out_ver;
			uint16_t i_w,o_w,i_h,o_h;

			i_w = pScalerInfo->scaler_in_width;
			o_w = pScalerInfo->scaler_out_width;
			i_h = pScalerInfo->scaler_in_height;
			o_h = pScalerInfo->scaler_out_height;

			if (i_w % 2 != 0 || o_w % 2 !=0 || i_w > o_w * SCL_DOWN_MAX || o_w > i_w * SCL_UP_MAX)
				pr_err("fail to get vaild iw %d ow %d\n", i_w, o_w);
			if (i_h % 2 != 0 || o_h % 2 != 0 || i_h > o_h * SCL_DOWN_MAX || o_h > i_h * SCL_UP_MAX)
				pr_err("fail to get vaild iw %d ow %d\n", i_h, o_h);

			scl_factor_in_hor = (uint16_t)(i_w * adj_hor);
			scl_factor_out_hor = (uint16_t)(o_w * adj_hor);
			scl_factor_in_ver = (uint16_t)(i_h * adj_ver);
			scl_factor_out_ver = (uint16_t)(o_h * adj_ver);

			pScalerInfo->scaler_factor_in_hor = scl_factor_in_hor;
			pScalerInfo->scaler_factor_out_hor = scl_factor_out_hor;
			pScalerInfo->scaler_factor_in_ver = scl_factor_in_ver;
			pScalerInfo->scaler_factor_out_ver = scl_factor_out_ver;

			scl_init_phase_hor = pScalerInfo->scaler_init_phase_hor;
			scl_init_phase_ver = pScalerInfo->scaler_init_phase_ver;
			pScalerInfo->init_phase_info.scaler_init_phase[0] = scl_init_phase_hor;
			pScalerInfo->init_phase_info.scaler_init_phase[1] = scl_init_phase_ver;

			// hor
			calc_scaler_phase(scl_init_phase_hor, scl_factor_out_hor,
				&pScalerInfo->init_phase_info.scaler_init_phase_int[0][0], &pScalerInfo->init_phase_info.scaler_init_phase_rmd[0][0]);
			calc_scaler_phase(scl_init_phase_hor / 4, scl_factor_out_hor / 2,
				&pScalerInfo->init_phase_info.scaler_init_phase_int[0][1], &pScalerInfo->init_phase_info.scaler_init_phase_rmd[0][1]);

			// ver
			calc_scaler_phase(scl_init_phase_ver, scl_factor_out_ver, &pScalerInfo->init_phase_info.scaler_init_phase_int[1][0],
				&pScalerInfo->init_phase_info.scaler_init_phase_rmd[1][0]);
			//FIXME: need refer to input_pixfmt
			//chroma
			if(pYuvScaler->input_pixfmt == YUV422) {
				if(pYuvScaler->output_pixfmt == YUV422)
					calc_scaler_phase(scl_init_phase_ver, scl_factor_out_ver,
						&pScalerInfo->init_phase_info.scaler_init_phase_int[1][1],
						&pScalerInfo->init_phase_info.scaler_init_phase_rmd[1][1]);
				else if(pYuvScaler->output_pixfmt == YUV420)
					calc_scaler_phase(scl_init_phase_ver / 2, scl_factor_out_ver / 2,
						&pScalerInfo->init_phase_info.scaler_init_phase_int[1][1],
						&pScalerInfo->init_phase_info.scaler_init_phase_rmd[1][1]);
			} else if (pYuvScaler->input_pixfmt == YUV420) {
				if (pYuvScaler->output_pixfmt == YUV422)
					calc_scaler_phase(scl_init_phase_ver / 2, scl_factor_out_ver,
						&pScalerInfo->init_phase_info.scaler_init_phase_int[1][1],
						&pScalerInfo->init_phase_info.scaler_init_phase_rmd[1][1]);
				else if (pYuvScaler->output_pixfmt == YUV420)
					calc_scaler_phase(scl_init_phase_ver / 4, scl_factor_out_ver / 2,
						&pScalerInfo->init_phase_info.scaler_init_phase_int[1][1],
						&pScalerInfo->init_phase_info.scaler_init_phase_rmd[1][1]);
			}
		} else {
			pScalerInfo->init_phase_info.scaler_init_phase[0] = 0;
			pScalerInfo->init_phase_info.scaler_init_phase[1] = 0;

			pScalerInfo->scaler_y_hor_tap = 0;
			pScalerInfo->scaler_uv_hor_tap = 0;
			pScalerInfo->scaler_y_ver_tap = 0;
			pScalerInfo->scaler_uv_ver_tap = 0;

			pScalerInfo->scaler_out_width = pScalerInfo->scaler_in_width;
			pScalerInfo->scaler_out_height = pScalerInfo->scaler_in_height;

			pScalerInfo->scaler_factor_in_hor  = pScalerInfo->scaler_in_width;
			pScalerInfo->scaler_factor_out_hor = pScalerInfo->scaler_out_width;
			pScalerInfo->scaler_factor_in_ver  = pScalerInfo->scaler_in_height;
			pScalerInfo->scaler_factor_out_ver = pScalerInfo->scaler_out_height;
		}
		new_width = pScalerInfo->scaler_out_width;
		new_height = pScalerInfo->scaler_out_height;

		pYuvScaler->trim1_info.trim_en = 0;
		pYuvScaler->trim1_info.trim_start_x = 0;
		pYuvScaler->trim1_info.trim_start_y = 0;
		pYuvScaler->trim1_info.trim_size_x = new_width;
		pYuvScaler->trim1_info.trim_size_y = new_height;
	}

	pYuvScaler->dst_size_x = new_width;
	pYuvScaler->dst_size_y = new_height;
}

static void scaler_init(struct yuvscaler_param_t *core_param, struct alg_slice_scaler_overlap *in_param_ptr, struct pipe_overlap_context *context)
{
	core_param->bypass = in_param_ptr->bypass;
	core_param->trim0_info.trim_en = in_param_ptr->trim_eb;
	core_param->trim0_info.trim_start_x = in_param_ptr->trim_start_x;
	core_param->trim0_info.trim_start_y = in_param_ptr->trim_start_y;
	core_param->trim0_info.trim_size_x = in_param_ptr->trim_size_x;
	core_param->trim0_info.trim_size_y = in_param_ptr->trim_size_y;

	if (0 == core_param->trim0_info.trim_en) {
		core_param->trim0_info.trim_start_x = 0;
		core_param->trim0_info.trim_start_y = 0;
		core_param->trim0_info.trim_size_x = context->frameWidth;
		core_param->trim0_info.trim_size_y = context->frameHeight;
	}

	core_param->deci_info.deci_x_en = in_param_ptr->deci_x_eb;
	core_param->deci_info.deci_y_en = in_param_ptr->deci_y_eb;
	core_param->deci_info.deci_x = SCALER_YUV_DECI_MAP[in_param_ptr->deci_x];
	core_param->deci_info.deci_y = SCALER_YUV_DECI_MAP[in_param_ptr->deci_y];
	core_param->deci_info.deciPhase_X = SCALER_YUV_DECI_OFFSET_MAP[in_param_ptr->deci_x];
	core_param->deci_info.deciPhase_Y = SCALER_YUV_DECI_OFFSET_MAP[in_param_ptr->deci_y];

	core_param->scaler_info.scaler_en = in_param_ptr->scaler_en;
	core_param->scaler_info.scaler_init_phase_hor = in_param_ptr->scl_init_phase_hor;
	core_param->scaler_info.scaler_init_phase_ver = in_param_ptr->scl_init_phase_ver;
	core_param->scaler_info.scaler_out_width = in_param_ptr->des_size_x;
	core_param->scaler_info.scaler_out_height = in_param_ptr->des_size_y;

	core_param->output_pixfmt = in_param_ptr->yuv_output_format;
	if (core_param->output_pixfmt == YUV422) {
		core_param->output_align_hor = in_param_ptr->output_align_hor;
		core_param->output_align_ver = 2;
	} else if (core_param->output_pixfmt == YUV420) {
		core_param->output_align_hor = in_param_ptr->output_align_hor;
		core_param->output_align_ver = 2;
		if (in_param_ptr->scaler_id == SCALER_DCAM_PRV) {
			if (in_param_ptr->FBC_enable && in_param_ptr->dec_online_bypass) {
				core_param->output_align_hor = AFBC_PADDING_W_YUV420_scaler;
				core_param->output_align_ver = AFBC_PADDING_H_YUV420_scaler;
			} else if (!in_param_ptr->FBC_enable &&!in_param_ptr->dec_online_bypass) {
				core_param->output_align_hor = (core_param->output_align_hor >
					(1<<(in_param_ptr->layerNum + 1))) ? core_param->output_align_hor : (1<<(in_param_ptr->layerNum + 1));
				core_param->output_align_ver = (core_param->output_align_ver >
					(1<<in_param_ptr->layerNum)) ? core_param->output_align_ver : (1<<in_param_ptr->layerNum);
			} else if (in_param_ptr->FBC_enable && !in_param_ptr->dec_online_bypass) {
				core_param->output_align_hor = (AFBC_PADDING_W_YUV420_scaler >
					(1<<(in_param_ptr->layerNum + 1)) ) ? AFBC_PADDING_W_YUV420_scaler : (1<<(in_param_ptr->layerNum + 1));
				core_param->output_align_ver = (AFBC_PADDING_H_YUV420_scaler >
					(1<<in_param_ptr->layerNum ) ) ? AFBC_PADDING_H_YUV420_scaler : (1<<in_param_ptr->layerNum);
			}
		} else {
			if (in_param_ptr->FBC_enable) {
				core_param->output_align_hor = AFBC_PADDING_W_YUV420_scaler;
				core_param->output_align_ver = AFBC_PADDING_H_YUV420_scaler;
			}
		}
	}
	if(context->pixelFormat == 3)/* PIX_FMT_YUV422 */
		core_param->input_pixfmt = YUV422;
	else if (context->pixelFormat == 4)/* PIX_FMT_YUV420 */
		core_param->input_pixfmt = YUV420;

	core_param->src_size_x = context->frameWidth;
	core_param->src_size_y = context->frameHeight;
	yuv_scaler_init_frame_info(core_param);
	core_param->scaler_info.input_pixfmt = core_param->input_pixfmt;
	core_param->scaler_info.output_pixfmt = core_param->output_pixfmt;
	/* driver add for scaler tap */
	if (!core_param->bypass && core_param->scaler_info.scaler_en) {
		core_param->scaler_info.scaler_y_hor_tap = in_param_ptr->scaler_y_hor_tap;
		core_param->scaler_info.scaler_uv_hor_tap = in_param_ptr->scaler_uv_hor_tap;
		core_param->scaler_info.scaler_y_ver_tap = in_param_ptr->scaler_y_ver_tap;
		core_param->scaler_info.scaler_uv_ver_tap = in_param_ptr->scaler_uv_ver_tap;
	}
}

static void scaler_calculate_region(const struct alg_slice_regions *r_ref,
	struct alg_slice_regions *r_out, struct yuvscaler_param_t *core_param,
	int v_flag, struct alg_slice_scaler_overlap *scaler_param_ptr, int slice_w)
{
	int i,j,index;
	int output_slice_end;
	uint16_t prev_row_end, prev_col_end;
	int rows = r_ref->rows;
	int cols = r_ref->cols;
	struct SliceWnd wndInTemp;
	struct SliceWnd wndOutTemp;
	struct slice_drv_scaler_phase_info phaseTemp;

	static struct alg_slice_regions r_old;

	phaseTemp.init_phase_hor = 0;
	phaseTemp.init_phase_ver = 0;

	prev_row_end = 0;
	for (i = 0;i < rows; i++) {
		prev_col_end = 0;
		for (j = 0;j < cols;j++) {
			index = i * cols + j;
			if (v_flag == 0)
				isp_drv_regions_set(r_ref, &r_old);
			//hor
			{
				int trim_start = core_param->trim0_info.trim_start_x;
				int trim_size = core_param->trim0_info.trim_size_x;
				int deci = core_param->deci_info.deci_x;
				int scl_en = core_param->scaler_info.scaler_en;
				int scl_factor_in = core_param->scaler_info.scaler_factor_in_hor;
				int scl_factor_out = core_param->scaler_info.scaler_factor_out_hor;
				int scl_tap = core_param->scaler_info.scaler_y_hor_tap;
				int init_phase = core_param->scaler_info.init_phase_info.scaler_init_phase[0];
				int input_slice_start = r_ref->regions[index].sx;
				int input_slice_size = r_ref->regions[index].ex - r_ref->regions[index].sx + 1;
				int output_pixel_align = core_param->output_align_hor;

				int output_slice_start = 0;
				int output_slice_size = 0;

				int trim_end = trim_start + trim_size;
				int last_trim_slice_id;
				last_trim_slice_id = (trim_end + slice_w - 1)/slice_w - 1;

				//if(j == cols - 1)
				if(j >= last_trim_slice_id)
					output_pixel_align = 2;

				if(v_flag == 0) {
					est_scaler_output_slice_info(
						trim_start,
						trim_size,
						deci,
						scl_en,
						scl_factor_in,
						scl_factor_out,
						scl_tap,
						init_phase,
						input_slice_start,
						input_slice_size,
						output_pixel_align,
						&output_slice_end);
				} else {
					est_scaler_output_slice_info_v2(
						trim_start,
						trim_size,
						deci,
						scl_en,
						scl_factor_in,
						scl_factor_out,
						scl_tap,
						init_phase,
						input_slice_start,
						input_slice_size,
						output_pixel_align,
						&output_slice_end);
				}

				if (j >= last_trim_slice_id) {
					if(output_slice_end > core_param->scaler_info.scaler_out_width || output_slice_end < core_param->scaler_info.scaler_out_width)
						output_slice_end = core_param->scaler_info.scaler_out_width;
				}

				if ((scaler_param_ptr->scaler_id == SCALER_DCAM_PRV) && (rows * cols > 1) && (v_flag == 1))
				{
					if (index > 1)
					pr_err("fail to get vaild index %d\n", index);
					if (0 == index) {
						if (output_slice_end > 0)
							output_slice_end = output_slice_end + scaler_param_ptr->slice_overlap_after_sclaer;
						else
							scaler_param_ptr->flag = 2;
						if (output_slice_end > scl_factor_out) {
							output_slice_end = scl_factor_out;
							scaler_param_ptr->slice_overlapright_after_sclaer = 0;
							scaler_param_ptr->slice_overlapleft_after_sclaer = 0;
							scaler_param_ptr->flag = 1;
						} else if(scaler_param_ptr->flag == 2) {
							scaler_param_ptr->slice_overlapright_after_sclaer = 0;
							scaler_param_ptr->slice_overlapleft_after_sclaer = 0;
						} else {
							scaler_param_ptr->slice_overlapright_after_sclaer = scaler_param_ptr->slice_overlap_after_sclaer;
							scaler_param_ptr->slice_overlapleft_after_sclaer = scaler_param_ptr->slice_overlap_after_sclaer;
						}
							output_slice_start = prev_col_end;
							output_slice_size = output_slice_end - output_slice_start;
					} else if (1 == index) {
						output_slice_start = prev_col_end - scaler_param_ptr->slice_overlapleft_after_sclaer;
						output_slice_size = output_slice_end - output_slice_start;
						if (output_slice_size > scl_factor_out) {
							output_slice_start = 0;
							output_slice_size = scl_factor_out;
							output_slice_end = scl_factor_out;
							scaler_param_ptr->slice_overlapleft_after_sclaer = 0;
						} else {
							if (scaler_param_ptr->flag == 0)
								scaler_param_ptr->slice_overlapleft_after_sclaer = scaler_param_ptr->slice_overlap_after_sclaer;
						}
					}
				} else {
					output_slice_start = prev_col_end;
					output_slice_size = output_slice_end - output_slice_start;
				}

				wndOutTemp.s_col = output_slice_start;
				wndOutTemp.e_col = output_slice_end - 1;

				if (output_slice_size > 0) {
					int input_pixel_align = 2;
					pr_debug("start %d size %d deci %d en %d fac_in %d out %d tap %d phase %d start %d slice %d align %d\n",
					trim_start, trim_size, deci, scl_en, scl_factor_in, scl_factor_out, scl_tap, init_phase, output_slice_start,
					output_slice_size, input_pixel_align);
					calc_scaler_input_slice_info(trim_start, trim_size, deci, scl_en, scl_factor_in, scl_factor_out, scl_tap, init_phase,
					output_slice_start, output_slice_size, input_pixel_align, &input_slice_start, &input_slice_size, &init_phase);

					phaseTemp.init_phase_hor = init_phase;

					wndInTemp.s_col = input_slice_start;
					wndInTemp.e_col = wndInTemp.s_col + input_slice_size - 1;
				} else {
					wndInTemp.s_col = r_ref->regions[index].sx;
					wndInTemp.e_col = r_ref->regions[index].ex;
				}

				/* dcam scaler0 only two slice, out slice must be more overlap for next module(dec1) */
				if((scaler_param_ptr->scaler_id == SCALER_DCAM_PRV) && (rows*cols > 1) && (v_flag == 1)) {
					if(0 == index)
						prev_col_end = output_slice_end - scaler_param_ptr->slice_overlapright_after_sclaer;
					if(1 == index)
						prev_col_end = output_slice_end;
				} else {
						prev_col_end = output_slice_end;
				}

				//prev_col_end = output_slice_end;
			}

			//ver
			{
				int scl_tap = 0;
				int trim_start = core_param->trim0_info.trim_start_y;
				int trim_size = core_param->trim0_info.trim_size_y;
				int deci = core_param->deci_info.deci_y;
				int scl_en = core_param->scaler_info.scaler_en;
				int scl_factor_in = core_param->scaler_info.scaler_factor_in_ver;
				int scl_factor_out = core_param->scaler_info.scaler_factor_out_ver;

				int init_phase = core_param->scaler_info.init_phase_info.scaler_init_phase[1];
				int input_slice_start = r_ref->regions[index].sy;
				int input_slice_size = r_ref->regions[index].ey - r_ref->regions[index].sy + 1;
				int output_pixel_align = core_param->output_align_ver;

				int output_slice_start;
				int output_slice_size;

				if (scl_en) {
					if(core_param->input_pixfmt == YUV422)
						scl_tap = MAX(core_param->scaler_info.scaler_y_ver_tap, core_param->scaler_info.scaler_uv_ver_tap) + 2;
					else if(core_param->input_pixfmt == YUV420)
						scl_tap = MAX(core_param->scaler_info.scaler_y_ver_tap, core_param->scaler_info.scaler_uv_ver_tap * 2) + 2;
				}

				if (core_param->output_pixfmt == YUV420 && i == rows - 1)
					output_pixel_align = 4;

				if ((i == rows - 1) && (core_param->output_pixfmt == YUV420))
					output_pixel_align = 4;

				if (v_flag == 0) {
					est_scaler_output_slice_info(
						trim_start,
						trim_size,
						deci,
						scl_en,
						scl_factor_in,
						scl_factor_out,
						scl_tap,
						init_phase,input_slice_start,
						input_slice_size,
						output_pixel_align,
						&output_slice_end);
				} else {
					est_scaler_output_slice_info_v2(
						trim_start,
						trim_size,
						deci,
						scl_en,
						scl_factor_in,
						scl_factor_out,
						scl_tap,
						init_phase,input_slice_start,
						input_slice_size,
						output_pixel_align,
						&output_slice_end);

					if (i == rows - 1) {
						if (output_slice_end > core_param->scaler_info.scaler_out_height || output_slice_end < core_param->scaler_info.scaler_out_height)
							output_slice_end = core_param->scaler_info.scaler_out_height;
					}
				}

				output_slice_start = prev_row_end;
				output_slice_size = output_slice_end - output_slice_start;

				wndOutTemp.s_row = output_slice_start;
				wndOutTemp.e_row = output_slice_end - 1;

				if (output_slice_size > 0) {
					int input_pixel_align = 2;
					calc_scaler_input_slice_info(trim_start, trim_size, deci, scl_en, scl_factor_in, scl_factor_out, scl_tap, init_phase,
						output_slice_start, output_slice_size, input_pixel_align, &input_slice_start, &input_slice_size, &init_phase);

					phaseTemp.init_phase_ver = init_phase;

					wndInTemp.s_row = input_slice_start;
					wndInTemp.e_row = wndInTemp.s_row + input_slice_size - 1;
				} else {
					wndInTemp.s_row = r_ref->regions[index].sy;
					wndInTemp.e_row = r_ref->regions[index].ey;
				}
			}

			//
			r_out->regions[index].sx = wndInTemp.s_col;
			r_out->regions[index].ex = wndInTemp.e_col;
			r_out->regions[index].sy = wndInTemp.s_row;
			r_out->regions[index].ey = wndInTemp.e_row;
			//
			scaler_param_ptr->phase[index].init_phase_hor = phaseTemp.init_phase_hor;
			scaler_param_ptr->phase[index].init_phase_ver = phaseTemp.init_phase_ver;
			//
			scaler_param_ptr->region_input[index].sx = wndInTemp.s_col;
			scaler_param_ptr->region_input[index].ex = wndInTemp.e_col;
			scaler_param_ptr->region_input[index].sy = wndInTemp.s_row;
			scaler_param_ptr->region_input[index].ey = wndInTemp.e_row;
			//
			scaler_param_ptr->region_output[index].sx = wndOutTemp.s_col;
			scaler_param_ptr->region_output[index].ex = wndOutTemp.e_col;
			scaler_param_ptr->region_output[index].sy = wndOutTemp.s_row;
			scaler_param_ptr->region_output[index].ey = wndOutTemp.e_row;
		}
		prev_row_end = wndOutTemp.e_row + 1;
	}

	r_out->rows = rows;
	r_out->cols = cols;

	if (v_flag == 1 && (scaler_param_ptr->scaler_id == SCALER_DCAM_PRV || scaler_param_ptr->scaler_id == SCALER_DCAM_CAP)) {
		int overlap_left_max = 0;
		int overlap_right_max = 0;
		int overlap_up_max = 0;
		int overlap_down_max = 0;

		for (i=0; i < rows; ++i) {
			for (j=0; j < cols; ++j) {
				int index = i * cols + j;
				int overlap_left_temp = r_old.regions[index].sx - scaler_param_ptr->region_input[index].sx;
				int overlap_right_temp = scaler_param_ptr->region_input[index].ex - r_old.regions[index].ex;
				int overlap_up_temp = r_old.regions[index].sy - scaler_param_ptr->region_input[index].sy;
				int overlap_down_temp = scaler_param_ptr->region_input[index].ey - r_old.regions[index].ey;

				if (overlap_left_temp > overlap_left_max)
					overlap_left_max = overlap_left_temp;

				if (overlap_right_temp > overlap_right_max)
					overlap_right_max = overlap_right_temp;

				if (overlap_up_temp > overlap_up_max)
					overlap_up_max = overlap_up_temp;

				if (overlap_down_temp > overlap_down_max)
					overlap_down_max = overlap_down_temp;
			}
		}

		scaler_param_ptr->input_scaler_overlap.overlap_left = overlap_left_max;
		scaler_param_ptr->input_scaler_overlap.overlap_right = overlap_right_max;
		scaler_param_ptr->input_scaler_overlap.overlap_up = overlap_up_max;
		scaler_param_ptr->input_scaler_overlap.overlap_down = overlap_down_max;
	}
}

static void isp_drv_region_max(const struct alg_region_info *r1,
	const struct alg_region_info *r2, struct alg_region_info *r_out)
{
	isp_drv_region_set(r1,r_out);

	if(r2->sx < r1->sx)
		r_out->sx = r2->sx;
	if(r2->ex > r1->ex)
		r_out->ex = r2->ex;
	if(r2->sy < r1->sy)
		r_out->sy = r2->sy;
	if(r2->ey > r1->ey)
		r_out->ey = r2->ey;
}

static void isp_drv_regions_max(const struct alg_slice_regions *r1,
	const struct alg_slice_regions *r2, struct alg_slice_regions *r_out)
{
	int i,j,index;
	int rows = r1->rows;
	int cols = r1->cols;

	if (0 == rows * cols)
		return;

	r_out->rows = rows;
	r_out->cols = cols;

	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			index = i * cols + j;
			isp_drv_region_max(&r1->regions[index], &r2->regions[index], &r_out->regions[index]);
		}
	}
}

static int isp_drv_regions_empty(const struct alg_slice_regions *r)
{
	if(0 == r->rows * r->cols)
		return 1;
	return 0;
}

static void isp_drv_regions_arr_max(struct alg_slice_regions* arr[],int num,struct alg_slice_regions *r_out)
{
	int i;

	for (i = 0; i< num; i++) {
		if(!isp_drv_regions_empty(arr[i])) {
			isp_drv_regions_set(arr[i],r_out);
			break;
		}
	}

	for (i = 0; i< num; i++) {
		if(isp_drv_regions_empty(arr[i]))
			continue;
		isp_drv_regions_max(r_out,arr[i],r_out);
	}
}

static struct slice_drv_overlap_info ltm_rgb_sta_getOverlap(const struct alg_slice_regions *refSliceList,
	struct ltm_rgb_stat_param_t *tm_stat_param)
{
	int start_x;
	int start_y;
	int end_x;
	int end_y;
	int index;
	int i, j;
	uint16_t tile_num_y_t, tile_num_x_t;

	int rows = refSliceList->rows;
	int cols = refSliceList->cols;

	uint8_t frame_cropUp = tm_stat_param->cropUp_stat << tm_stat_param->binning_en;
	uint8_t frame_cropDown = tm_stat_param->cropDown_stat << tm_stat_param->binning_en;
	uint8_t frame_cropLeft = tm_stat_param->cropLeft_stat << tm_stat_param->binning_en;
	uint8_t frame_cropRight = tm_stat_param->cropRight_stat << tm_stat_param->binning_en;
	uint16_t frame_tile_width = tm_stat_param->tile_width_stat << tm_stat_param->binning_en;
	uint16_t frame_tile_height = tm_stat_param->tile_height_stat << tm_stat_param->binning_en;
	int frame_img_w = tm_stat_param->frame_width_stat << tm_stat_param->binning_en;
	int frame_img_h = tm_stat_param->frame_height_stat << tm_stat_param->binning_en;

	struct slice_drv_overlap_info overlap;
	memset(&overlap, 0 , sizeof(struct slice_drv_overlap_info));
	if (cols > 2 || rows != 1)
		pr_err("fail to get valid col %d row %d\n", cols, rows);

	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			index = i * cols + j;

			start_x = refSliceList->regions[index].sx;
			start_y= refSliceList->regions[index].sy;
			end_x = refSliceList->regions[index].ex;
			end_y = refSliceList->regions[index].ey;

			start_y = MAX(start_y, frame_cropUp);
			start_x = MAX(start_x, frame_cropLeft);
			end_y = MIN(end_y, frame_img_h - frame_cropDown - 1);
			end_x = MIN(end_x, frame_img_w - frame_cropRight - 1);

			tile_num_y_t = (end_y - start_y + 1) % frame_tile_height;
			tile_num_x_t = (end_x - start_x + 1) % frame_tile_width;
			if (index == 0) {
				overlap.ov_right = (tile_num_x_t >= frame_tile_width / 2 ? frame_tile_width - tile_num_x_t : 0);
			} else if (index == 1) {
				overlap.ov_left = (tile_num_x_t >= frame_tile_width / 2 ? frame_tile_width - tile_num_x_t : 0);
			}
		}
	}

	return overlap;
}

void alg_slice_scaler_overlap_temp(struct alg_scaler_ovlap_temp *in_ptr)
{
	struct alg_slice_drv_overlap *param_ptr =NULL;
	struct yuvscaler_param_t *scaler1_frame_p = NULL;
	struct yuvscaler_param_t *scaler2_frame_p = NULL;
	struct alg_slice_regions *maxRegion = NULL;
	struct alg_slice_regions *orgRegion = NULL;
	struct thumbnailscaler_context contextThumbnailScaler;
	struct thumbnailscaler_param_t thumbnailscaler_param;
	struct pipe_overlap_context contextScaler;

	if (!in_ptr) {
		pr_err("fail to get invalid ptr\n");
		return;
	}

	param_ptr = in_ptr->param_ptr;
	scaler1_frame_p = (struct yuvscaler_param_t*)(param_ptr->scaler1.frameParam);
	scaler2_frame_p = (struct yuvscaler_param_t*)(param_ptr->scaler2.frameParam);
	maxRegion = in_ptr->maxRegion;
	orgRegion = in_ptr->orgRegion;

	//set scaler context
	contextScaler.frameWidth = in_ptr->image_w;
	contextScaler.frameHeight = in_ptr->image_h;
	contextScaler.pixelFormat = param_ptr->scaler_input_format;

	//set thumbnail context
	contextThumbnailScaler.src_width = in_ptr->image_w;
	contextThumbnailScaler.src_height = in_ptr->image_h;
	contextThumbnailScaler.offlineSliceWidth = param_ptr->slice_w;
	contextThumbnailScaler.offlineSliceHeight = param_ptr->slice_h;

	//set thumbnail scaler param
	thumbnailscaler_param.configinfo.thumbnailscaler_trim0_en = param_ptr->thumbnailscaler.trim0_en;
	thumbnailscaler_param.configinfo.thumbnailscaler_trimstartcol = param_ptr->thumbnailscaler.trim0_start_x;
	thumbnailscaler_param.configinfo.thumbnailscaler_trimstartrow = param_ptr->thumbnailscaler.trim0_start_y;
	thumbnailscaler_param.configinfo.thumbnailscaler_trimsizeX = param_ptr->thumbnailscaler.trim0_size_x;
	thumbnailscaler_param.configinfo.thumbnailscaler_trimsizeY = param_ptr->thumbnailscaler.trim0_size_y;
	thumbnailscaler_param.configinfo.thumbnailscaler_phaseX = param_ptr->thumbnailscaler.phase_x;
	thumbnailscaler_param.configinfo.thumbnailscaler_phaseY = param_ptr->thumbnailscaler.phase_y;
	thumbnailscaler_param.configinfo.thumbnailscaler_base_align = param_ptr->thumbnailscaler.base_align;
	thumbnailscaler_param.configinfo.ow = param_ptr->thumbnailscaler.out_w;
	thumbnailscaler_param.configinfo.oh = param_ptr->thumbnailscaler.out_h;
	thumbnailscaler_param.configinfo.outformat = param_ptr->thumbnailscaler.out_format;

	//step1
	do {
		struct alg_slice_regions refRegion;
		struct alg_slice_regions nr3d_slice_region_out;
		struct alg_slice_regions scaler1_slice_region_out;
		struct alg_slice_regions scaler2_slice_region_out;
		struct alg_slice_regions scaler3_slice_region_out;
		struct alg_slice_regions maxRegionTemp;
		struct alg_slice_regions* regions_arr[4];

		memset(&refRegion, 0, sizeof(struct alg_slice_regions));
		memset(&nr3d_slice_region_out, 0, sizeof(struct alg_slice_regions));
		memset(&scaler1_slice_region_out, 0, sizeof(struct alg_slice_regions));
		memset(&scaler2_slice_region_out, 0, sizeof(struct alg_slice_regions));
		memset(&scaler3_slice_region_out, 0, sizeof(struct alg_slice_regions));
		memset(&maxRegionTemp, 0, sizeof(struct alg_slice_regions));

		regions_arr[0] = &nr3d_slice_region_out;
		regions_arr[1] = &scaler1_slice_region_out;
		regions_arr[2] = &scaler2_slice_region_out;
		regions_arr[3] = &scaler3_slice_region_out;

		isp_drv_regions_set(maxRegion, &refRegion);

		if(!param_ptr->nr3d_bd_bypass && param_ptr->nr3d_bd_FBC_en) {
			isp_drv_regions_set(orgRegion, &nr3d_slice_region_out);
			isp_drv_regions_3dnr(&refRegion, &nr3d_slice_region_out,
				AFBC_PADDING_W_YUV420_3dnr, AFBC_PADDING_H_YUV420_3dnr,0);
			isp_drv_regions_set(&nr3d_slice_region_out, orgRegion);
		}

		if (!param_ptr->scaler1.bypass) {
			param_ptr->scaler1.slice_overlap_after_sclaer = 0;
			param_ptr->scaler1.slice_overlapleft_after_sclaer = 0;
			param_ptr->scaler1.slice_overlapright_after_sclaer = 0;
			param_ptr->scaler1.flag = 0;

			scaler_init(scaler1_frame_p, &param_ptr->scaler1, &contextScaler);
			pr_debug("regin sx %d ex %d sy %d ey %d\n", param_ptr->scaler1.region_input[0].sx,
				param_ptr->scaler1.region_input[0].ex, param_ptr->scaler1.region_input[0].sy,
				param_ptr->scaler1.region_input[0].ey);
			pr_debug("ref regin sx %d ex %d sy %d ey %d\n", refRegion.regions[0].sx,
				refRegion.regions[0].ex, refRegion.regions[0].sy,
				refRegion.regions[0].ey);
			scaler_calculate_region(&refRegion, &scaler1_slice_region_out, scaler1_frame_p, 0, &param_ptr->scaler1, param_ptr->slice_w);
		}

		if (!param_ptr->scaler2.bypass) {
			param_ptr->scaler1.slice_overlap_after_sclaer = 0;
			param_ptr->scaler1.slice_overlapleft_after_sclaer = 0;
			param_ptr->scaler1.slice_overlapright_after_sclaer = 0;
			param_ptr->scaler1.flag = 0;

			scaler_init(scaler2_frame_p, &param_ptr->scaler2, &contextScaler);
			scaler_calculate_region(&refRegion, &scaler2_slice_region_out, scaler2_frame_p, 0, &param_ptr->scaler2, param_ptr->slice_w);
		}

		if (!param_ptr->thumbnailscaler.bypass)
			thumbnailscaler_calculate_region(&refRegion,&scaler3_slice_region_out,&thumbnailscaler_param,&contextThumbnailScaler,0);

		//get max
		isp_drv_regions_arr_max(regions_arr, 4, &maxRegionTemp);
		if (!isp_drv_regions_empty(&maxRegionTemp))
			isp_drv_regions_set(&maxRegionTemp, maxRegion);
	} while (0);

	//step2
	do {
		struct alg_slice_regions refRegion;
		struct alg_slice_regions nr3d_slice_region_out;
		struct alg_slice_regions scaler1_slice_region_out;
		struct alg_slice_regions scaler2_slice_region_out;
		struct alg_slice_regions scaler3_slice_region_out;
		struct alg_slice_regions maxRegionTemp;
		struct alg_slice_regions* regions_arr[4];

		memset(&refRegion, 0, sizeof(struct alg_slice_regions));
		memset(&nr3d_slice_region_out, 0, sizeof(struct alg_slice_regions));
		memset(&scaler1_slice_region_out, 0, sizeof(struct alg_slice_regions));
		memset(&scaler2_slice_region_out, 0, sizeof(struct alg_slice_regions));
		memset(&scaler3_slice_region_out, 0, sizeof(struct alg_slice_regions));
		memset(&maxRegionTemp, 0, sizeof(struct alg_slice_regions));

		regions_arr[0] = &nr3d_slice_region_out;
		regions_arr[1] = &scaler1_slice_region_out;
		regions_arr[2] = &scaler2_slice_region_out;
		regions_arr[3] = &scaler3_slice_region_out;

		isp_drv_regions_set(maxRegion, &refRegion);

		if (!param_ptr->nr3d_bd_bypass && param_ptr->nr3d_bd_FBC_en) {
			isp_drv_regions_set(orgRegion, &nr3d_slice_region_out);
			isp_drv_regions_3dnr(&refRegion, &nr3d_slice_region_out, AFBC_PADDING_W_YUV420_3dnr,AFBC_PADDING_H_YUV420_3dnr, 1);
			isp_drv_regions_set(&nr3d_slice_region_out, orgRegion);
		}

		if (!param_ptr->scaler1.bypass) {
			param_ptr->scaler1.slice_overlap_after_sclaer = 0;
			param_ptr->scaler1.slice_overlapleft_after_sclaer = 0;
			param_ptr->scaler1.slice_overlapright_after_sclaer = 0;
			param_ptr->scaler1.flag = 0;
			scaler_calculate_region(&refRegion, &scaler1_slice_region_out, scaler1_frame_p, 1, &param_ptr->scaler1, param_ptr->slice_w);
		}

		if (!param_ptr->scaler2.bypass) {
			param_ptr->scaler2.slice_overlap_after_sclaer = 0;
			param_ptr->scaler2.slice_overlapleft_after_sclaer = 0;
			param_ptr->scaler2.slice_overlapright_after_sclaer = 0;
			param_ptr->scaler2.flag = 0;
			scaler_calculate_region(&refRegion, &scaler2_slice_region_out, scaler2_frame_p, 1, &param_ptr->scaler2, param_ptr->slice_w);
		}

		if (!param_ptr->thumbnailscaler.bypass)
			isp_drv_regions_set(&refRegion, &scaler3_slice_region_out);
		//get max
		isp_drv_regions_arr_max(regions_arr, 4, &maxRegionTemp);
		if (!isp_drv_regions_empty(&maxRegionTemp))
			isp_drv_regions_set(&maxRegionTemp, maxRegion);
	} while (0);
}

void alg_slice_pyr_get_slice_region
	(struct alg_slice_drv_overlap *param_ptr,
	struct alg_pyramid_ovlap_temp *in_ptr)
{
	int i = 0, j = 0, index = 0, layer_id = 0;
	int rows = 0, cols = 0, layer_num = 0, slice_num = 0;
	int layer0_padding_width = 0, layer0_padding_height = 0;
	struct alg_slice_regions org_add_rec_slice_out[MAX_PYR_DEC_LAYER_NUM];
	struct alg_fetch_region org_add_rec_slice_in;

	layer_num = in_ptr->layer_num;
	slice_num = in_ptr->slice_num;
	layer0_padding_width = in_ptr->layer0_padding_width;
	layer0_padding_height = in_ptr->layer0_padding_height;
	for (layer_id = 0; layer_id < MAX_PYR_DEC_LAYER_NUM; layer_id++)
		memset(&org_add_rec_slice_out[layer_id], 0, sizeof(struct alg_slice_regions));

	for (layer_id = 0; layer_id < layer_num; layer_id++) {
		org_add_rec_slice_in.image_w = layer_id ? (layer0_padding_width >> layer_id) : (param_ptr->img_src_w);
		org_add_rec_slice_in.image_h = layer_id ? (layer0_padding_height >> layer_id) : (param_ptr->img_src_h);
		org_add_rec_slice_in.slice_w = (slice_num == 1) ? org_add_rec_slice_in.image_w : param_ptr->slice_w >> layer_id;
		org_add_rec_slice_in.slice_h = layer0_padding_height >> layer_id;
		org_add_rec_slice_in.overlap_left = 0;
		org_add_rec_slice_in.overlap_right = 0;
		org_add_rec_slice_in.overlap_up = 0;
		org_add_rec_slice_in.overlap_down = 0;

		//get slice region
		if (-1 == isp_drv_regions_fetch(&org_add_rec_slice_in, &org_add_rec_slice_out[layer_id]))
			return;
	}
	rows = in_ptr->add_rec_slice_out[0].rows;
	cols = in_ptr->add_rec_slice_out[0].cols;
	//get fetch0
	for (layer_id = 0; layer_id < layer_num; layer_id++) {
		for (i = 0; i < rows; i++) {
			for (j = 0; j < cols; j++) {
				index = i * cols + j;
				//
				param_ptr->fecth0_slice_region[layer_id][index].sx = in_ptr->add_rec_slice_out[layer_id].regions[index].sx;
				param_ptr->fecth0_slice_region[layer_id][index].ex = in_ptr->add_rec_slice_out[layer_id].regions[index].ex;
				param_ptr->fecth0_slice_region[layer_id][index].sy = in_ptr->add_rec_slice_out[layer_id].regions[index].sy;
				param_ptr->fecth0_slice_region[layer_id][index].ey = in_ptr->add_rec_slice_out[layer_id].regions[index].ey;
				pr_debug("layer %d slice %d fetch 0 sx%d sy%d ex%d ey%d\n", layer_id, index,
					param_ptr->fecth0_slice_region[layer_id][index].sx, param_ptr->fecth0_slice_region[layer_id][index].sy,
					param_ptr->fecth0_slice_region[layer_id][index].ex, param_ptr->fecth0_slice_region[layer_id][index].ey);
				//
				param_ptr->fecth0_slice_overlap[layer_id][index].ov_left = org_add_rec_slice_out[layer_id].regions[index].sx
					- in_ptr->add_rec_slice_out[layer_id].regions[index].sx;
				param_ptr->fecth0_slice_overlap[layer_id][index].ov_right = in_ptr->add_rec_slice_out[layer_id].regions[index].ex
					- org_add_rec_slice_out[layer_id].regions[index].ex;
				param_ptr->fecth0_slice_overlap[layer_id][index].ov_up = org_add_rec_slice_out[layer_id].regions[index].sy
					- in_ptr->add_rec_slice_out[layer_id].regions[index].sy;
				param_ptr->fecth0_slice_overlap[layer_id][index].ov_down = in_ptr->add_rec_slice_out[layer_id].regions[index].ey
					- org_add_rec_slice_out[layer_id].regions[index].ey;
			}
		}
	}
}

void alg_slice_pyramid_ovlap_temp(struct alg_pyramid_ovlap_temp *in_ptr)
{
	int i = 0, layer_slice_num = 0, index = 0;
	int layer_slice_width = 0, layer_slice_height = 0;
	int layer_id = 0, layer_num = 0, slice_num = 0;
	int layer0_padding_width = 0, layer0_padding_height = 0;
	int next_layer_width= 0;
	int SLICE_W_ALIGN_V = 4;
	uint32_t slice_flag = 1;
	struct alg_slice_drv_overlap *param_ptr =NULL;
	struct alg_overlap_info *overlap_rec = NULL;
	struct alg_overlap_info *ov_pipe_layer0 = NULL;
	struct alg_overlap_info *overlap_rec_mode1 = NULL;
	struct alg_slice_regions rec_slice_out[MAX_PYR_DEC_LAYER_NUM];
	struct alg_slice_regions org_rec_slice_out[MAX_PYR_DEC_LAYER_NUM];
	struct alg_overlap_info ov_layer_rec[MAX_PYR_DEC_LAYER_NUM] = {0};
	struct alg_fetch_region rec_slice_in = {0};
	struct alg_fetch_region org_rec_slice_in = {0};
	struct alg_overlap_info rec_slice_overlap[MAX_PYR_DEC_LAYER_NUM][PIPE_MAX_SLICE_NUM] = {{0}};
	struct alg_overlap_info ov_layer[MAX_PYR_DEC_LAYER_NUM] = {0};
	struct alg_fetch_region add_rec_slice_in = {0};

	if (!in_ptr) {
		pr_err("fail to get invalid ptr\n");
		return;
	}

	for (layer_id = 0; layer_id < MAX_PYR_DEC_LAYER_NUM; layer_id++) {
		memset(&rec_slice_out[layer_id],0, sizeof(struct alg_slice_regions));
		memset(&org_rec_slice_out[layer_id], 0, sizeof(struct alg_slice_regions));
	}

	param_ptr = in_ptr->param_ptr;
	layer_id = in_ptr->layer_id;
	layer_num = in_ptr->layer_num;
	slice_num = in_ptr->slice_num;
	layer0_padding_width = in_ptr->layer0_padding_width;
	layer0_padding_height = in_ptr->layer0_padding_height;
	overlap_rec = in_ptr->overlap_rec;
	ov_pipe_layer0 = in_ptr->ov_pipe_layer0;
	overlap_rec_mode1 = in_ptr->overlap_rec_mode1;
	// rec region
	do {
		int i, j, index, rows, cols;
		rows = param_ptr->slice_rows;
		cols = param_ptr->slice_cols;
		for (layer_id = 0; layer_id < layer_num; layer_id++) {
			ov_layer_rec[layer_id].ov_left = overlap_rec->ov_left ;
			ov_layer_rec[layer_id].ov_right = overlap_rec->ov_right;
			ov_layer_rec[layer_id].ov_up = overlap_rec->ov_up;
			ov_layer_rec[layer_id].ov_down = overlap_rec->ov_down ;
			if (!param_ptr->uw_sensor && layer_id == 0) {
				ov_layer_rec[layer_id].ov_left = overlap_rec->ov_left << 1;
				ov_layer_rec[layer_id].ov_right = overlap_rec->ov_right << 1;
				ov_layer_rec[layer_id].ov_up = overlap_rec->ov_up << 1;
				ov_layer_rec[layer_id].ov_down = overlap_rec->ov_down << 1;
			}

			rec_slice_in.image_w = layer_id ? (layer0_padding_width >> layer_id) : (param_ptr->img_src_w);
			rec_slice_in.image_h = layer_id ? (layer0_padding_height >> layer_id) : (param_ptr->img_src_h);
			rec_slice_in.slice_w = (slice_num == 1) ? rec_slice_in.image_w : param_ptr->slice_w >> layer_id;
			rec_slice_in.slice_h = layer0_padding_height >> layer_id;
			rec_slice_in.overlap_left = ov_layer_rec[layer_id].ov_left;
			rec_slice_in.overlap_right = ov_layer_rec[layer_id].ov_right;
			rec_slice_in.overlap_up = ov_layer_rec[layer_id].ov_up;
			rec_slice_in.overlap_down = ov_layer_rec[layer_id].ov_down;

			pr_debug("img w%d h%d slice w%d h%d ov L%d R %d\n", rec_slice_in.image_w, rec_slice_in.image_h,
				rec_slice_in.slice_w, rec_slice_in.slice_h, rec_slice_in.overlap_left, rec_slice_in.overlap_right);
			//get slice region
			if (-1 == isp_drv_regions_fetch(&rec_slice_in, &rec_slice_out[layer_id]))
				return;
		}

		pr_debug("lay num %d input laynum %d\n", layer_num, param_ptr->layerNum);
		for (layer_id = 0; layer_id < layer_num; layer_id++) {
			org_rec_slice_in.image_w = layer_id ? (layer0_padding_width >> layer_id) : (param_ptr->img_src_w);
			org_rec_slice_in.image_h = layer_id ? (layer0_padding_height >> layer_id) : (param_ptr->img_src_h);
			org_rec_slice_in.slice_w = (slice_num == 1) ? org_rec_slice_in.image_w : param_ptr->slice_w>>layer_id;
			org_rec_slice_in.slice_h = layer0_padding_height >> layer_id;
			org_rec_slice_in.overlap_left  = 0;
			org_rec_slice_in.overlap_right = 0;
			org_rec_slice_in.overlap_up = 0;
			org_rec_slice_in.overlap_down = 0;

			pr_debug("img w%d h%d slice w%d h%d ov L%d R %d\n", org_rec_slice_in.image_w, org_rec_slice_in.image_h,
				org_rec_slice_in.slice_w, org_rec_slice_in.slice_h, org_rec_slice_in.overlap_left, org_rec_slice_in.overlap_right);
			//get slice region
			if (-1 == isp_drv_regions_fetch(&org_rec_slice_in, &org_rec_slice_out[layer_id])) {
				return;
			}
		}
		//get rec overlap
		//fetch
		for (layer_id = 0; layer_id < layer_num; layer_id++) {
			for (i=0; i < rows; i++) {
				for (j=0; j < cols; j++) {
					index = i * cols + j;
					//
					rec_slice_overlap[layer_id][index].ov_left = org_rec_slice_out[layer_id].regions[index].sx - rec_slice_out[layer_id].regions[index].sx;
					rec_slice_overlap[layer_id][index].ov_right = rec_slice_out[layer_id].regions[index].ex - org_rec_slice_out[layer_id].regions[index].ex;
					rec_slice_overlap[layer_id][index].ov_up = org_rec_slice_out[layer_id].regions[index].sy - rec_slice_out[layer_id].regions[index].sy;
					rec_slice_overlap[layer_id][index].ov_down = rec_slice_out[layer_id].regions[index].ey - org_rec_slice_out[layer_id].regions[index].ey;
				}
			}
		}
	} while(0);

	//////////////////////////////////////////////////////////////////////////
	/* (ISP_overlap >> layer_id) + rec_overlap */
	for (layer_id = 0; layer_id < layer_num; layer_id++) {
		ov_layer[layer_id].ov_left = (ov_pipe_layer0->ov_left >> layer_id) + ov_layer_rec[layer_id].ov_left;
		ov_layer[layer_id].ov_right = (ov_pipe_layer0->ov_right >> layer_id) + ov_layer_rec[layer_id].ov_right;
		ov_layer[layer_id].ov_up = (ov_pipe_layer0->ov_up >> layer_id) + ov_layer_rec[layer_id].ov_up;
		ov_layer[layer_id].ov_down = (ov_pipe_layer0->ov_down >> layer_id) + ov_layer_rec[layer_id].ov_down;

		add_rec_slice_in.image_w = layer_id ? (layer0_padding_width >> layer_id) : (param_ptr->img_src_w);
		add_rec_slice_in.image_h = layer_id ? (layer0_padding_height >> layer_id) : (param_ptr->img_src_h);
		add_rec_slice_in.slice_w = (slice_num == 1) ? add_rec_slice_in.image_w : param_ptr->slice_w >> layer_id;
		add_rec_slice_in.slice_h = layer0_padding_height >> layer_id;
		add_rec_slice_in.overlap_left = ov_layer[layer_id].ov_left;
		add_rec_slice_in.overlap_right = ov_layer[layer_id].ov_right;
		add_rec_slice_in.overlap_up = ov_layer[layer_id].ov_up;
		add_rec_slice_in.overlap_down = ov_layer[layer_id].ov_down;
		pr_debug("img w%d h%d slice w%d h%d ov L%d R %d\n", add_rec_slice_in.image_w, add_rec_slice_in.image_h,
			add_rec_slice_in.slice_w, add_rec_slice_in.slice_h, add_rec_slice_in.overlap_left, add_rec_slice_in.overlap_right);
		//get slice region
		if (-1 == isp_drv_regions_fetch(&add_rec_slice_in, &in_ptr->add_rec_slice_out[layer_id]))
			return;
	}
	//////////////////////////////////////////////////////////////////////////
	param_ptr->slice_rows = in_ptr->add_rec_slice_out[0].rows;
	param_ptr->slice_cols = in_ptr->add_rec_slice_out[0].cols;

	do {
		int i, j, index, rows, cols;
		rows = param_ptr->slice_rows;
		cols = param_ptr->slice_cols;
		alg_slice_pyr_get_slice_region(param_ptr, in_ptr);
		for (layer_id = 0; layer_id < layer_num - 1; layer_id++) {
			for (i = 0; i < rows; i++) {
				for (j = 0; j < cols; j++) {
					index = i * cols + j;
					//fetch1
					param_ptr->fecth1_slice_region[layer_id][index].sx = param_ptr->fecth0_slice_region[layer_id + 1][index].sx << 1;
					param_ptr->fecth1_slice_region[layer_id][index].ex = ((param_ptr->fecth0_slice_region[layer_id + 1][index].ex + 1) << 1) - 1;
					param_ptr->fecth1_slice_region[layer_id][index].sy = param_ptr->fecth0_slice_region[layer_id + 1][index].sy << 1;
					param_ptr->fecth1_slice_region[layer_id][index].ey = ((param_ptr->fecth0_slice_region[layer_id + 1][index].ey + 1) << 1) - 1;
					if (layer_id == 0) {
						param_ptr->fecth1_slice_region[layer_id][index].ex = MIN(param_ptr->fecth1_slice_region[layer_id][index].ex, param_ptr->img_src_w - 1);
						param_ptr->fecth1_slice_region[layer_id][index].ey = MIN(param_ptr->fecth1_slice_region[layer_id][index].ey, param_ptr->img_src_h - 1);
					} else {
						param_ptr->fecth1_slice_region[layer_id][index].ex = MIN(param_ptr->fecth1_slice_region[layer_id][index].ex, (layer0_padding_width >> layer_id) - 1);
						param_ptr->fecth1_slice_region[layer_id][index].ey = MIN(param_ptr->fecth1_slice_region[layer_id][index].ey, (layer0_padding_height >> layer_id) - 1);
					}
					pr_debug("layer %d slice %d fetch 0 sx%d sy%d ex%d ey%d\n", layer_id, index, param_ptr->fecth0_slice_region[layer_id][index].sx,
						param_ptr->fecth0_slice_region[layer_id][index].sy, param_ptr->fecth0_slice_region[layer_id][index].ex, param_ptr->fecth0_slice_region[layer_id][index].ey);
					pr_debug("layer %d slice %d fetch 1 sx%d sy%d ex%d ey%d\n", layer_id, index, param_ptr->fecth1_slice_region[layer_id][index].sx,
						param_ptr->fecth1_slice_region[layer_id][index].sy, param_ptr->fecth1_slice_region[layer_id][index].ex, param_ptr->fecth1_slice_region[layer_id][index].ey);
					//store rec
					if (layer_id == 0) {
						param_ptr->store_rec_slice_region[layer_id][index].sx = param_ptr->fecth0_slice_region[layer_id][index].sx;
						param_ptr->store_rec_slice_region[layer_id][index].ex = param_ptr->fecth0_slice_region[layer_id][index].ex;
						param_ptr->store_rec_slice_region[layer_id][index].sy = param_ptr->fecth0_slice_region[layer_id][index].sy;
						param_ptr->store_rec_slice_region[layer_id][index].ey = param_ptr->fecth0_slice_region[layer_id][index].ey;

						param_ptr->store_rec_slice_overlap[layer_id][index].ov_left = param_ptr->fecth0_slice_overlap[layer_id][index].ov_left;
						param_ptr->store_rec_slice_overlap[layer_id][index].ov_right = param_ptr->fecth0_slice_overlap[layer_id][index].ov_right;
						param_ptr->store_rec_slice_overlap[layer_id][index].ov_up = param_ptr->fecth0_slice_overlap[layer_id][index].ov_up;
						param_ptr->store_rec_slice_overlap[layer_id][index].ov_down = param_ptr->fecth0_slice_overlap[layer_id][index].ov_down;

						if (!param_ptr->uw_sensor) {
							param_ptr->store_rec_slice_crop_overlap[layer_id][index].ov_left = 0;
							param_ptr->store_rec_slice_crop_overlap[layer_id][index].ov_right = 0;
							param_ptr->store_rec_slice_crop_overlap[layer_id][index].ov_up = 0;
							param_ptr->store_rec_slice_crop_overlap[layer_id][index].ov_down = 0;
					} else {
							param_ptr->store_rec_slice_crop_overlap[layer_id][index].ov_left = rec_slice_overlap[layer_id][index].ov_left;
							param_ptr->store_rec_slice_crop_overlap[layer_id][index].ov_right = rec_slice_overlap[layer_id][index].ov_right;
							param_ptr->store_rec_slice_crop_overlap[layer_id][index].ov_up = rec_slice_overlap[layer_id][index].ov_up;
							param_ptr->store_rec_slice_crop_overlap[layer_id][index].ov_down = rec_slice_overlap[layer_id][index].ov_down;
						}
					} else {
						param_ptr->store_rec_slice_crop_overlap[layer_id][index].ov_left = rec_slice_overlap[layer_id][index].ov_left;
						param_ptr->store_rec_slice_crop_overlap[layer_id][index].ov_right = rec_slice_overlap[layer_id][index].ov_right;
						param_ptr->store_rec_slice_crop_overlap[layer_id][index].ov_up = rec_slice_overlap[layer_id][index].ov_up;
						param_ptr->store_rec_slice_crop_overlap[layer_id][index].ov_down = rec_slice_overlap[layer_id][index].ov_down;

						param_ptr->store_rec_slice_overlap[layer_id][index].ov_left = (param_ptr->fecth0_slice_overlap[layer_id+1][index].ov_left << 1) - rec_slice_overlap[layer_id][index].ov_left ;
						param_ptr->store_rec_slice_overlap[layer_id][index].ov_right = (param_ptr->fecth0_slice_overlap[layer_id+1][index].ov_right<< 1) - rec_slice_overlap[layer_id][index].ov_right;
						param_ptr->store_rec_slice_overlap[layer_id][index].ov_up = (param_ptr->fecth0_slice_overlap[layer_id+1][index].ov_up << 1) - rec_slice_overlap[layer_id][index].ov_up;
						param_ptr->store_rec_slice_overlap[layer_id][index].ov_down = (param_ptr->fecth0_slice_overlap[layer_id+1][index].ov_down <<1) - rec_slice_overlap[layer_id][index].ov_down ;

						param_ptr->store_rec_slice_region[layer_id][index].sx = (param_ptr->fecth0_slice_region[layer_id + 1][index].sx << 1) + rec_slice_overlap[layer_id][index].ov_left ;
						param_ptr->store_rec_slice_region[layer_id][index].ex = (((param_ptr->fecth0_slice_region[layer_id + 1][index].ex + 1) << 1) - 1) - rec_slice_overlap[layer_id][index].ov_right;
						param_ptr->store_rec_slice_region[layer_id][index].sy = (param_ptr->fecth0_slice_region[layer_id + 1][index].sy << 1) + rec_slice_overlap[layer_id][index].ov_up;
						param_ptr->store_rec_slice_region[layer_id][index].ey = (((param_ptr->fecth0_slice_region[layer_id + 1][index].ey + 1) << 1) - 1) - rec_slice_overlap[layer_id][index].ov_down ;
					}
				}
			}
		}
	} while(0);

	/* offline_slice_mode = 1,update layer5～layer2：auto slice */
	if (param_ptr->offline_slice_mode == 1 && param_ptr->layerNum >= 3) {
		for (layer_id = 2; layer_id < param_ptr->layerNum; layer_id++) {
			for (i = 0; i < slice_num; i++) {
				rec_slice_overlap[layer_id][i].ov_left = 0;
				rec_slice_overlap[layer_id][i].ov_right = 0;
				rec_slice_overlap[layer_id][i].ov_up = 0;
				rec_slice_overlap[layer_id][i].ov_down = 0;
			}
		}

		for (layer_id = 2; layer_id < param_ptr->layerNum; layer_id++) {
			rec_slice_in.image_w = layer_id ? (layer0_padding_width >> layer_id) : (param_ptr->img_src_w);
			rec_slice_in.image_h = layer_id ? (layer0_padding_height >> layer_id) : (param_ptr->img_src_h);
			next_layer_width = rec_slice_in.image_w << 1;
			pr_debug("next layer w %d im_src w %d layer0 p_w %d\n", next_layer_width, param_ptr->img_src_w, layer0_padding_width);
			if (next_layer_width <= 2592)
				slice_flag = 0;

			if (!slice_flag) {
				layer_slice_num = 1;
				layer_slice_width = rec_slice_in.image_w;
				layer_slice_height = rec_slice_in.image_h;
			} else {
				layer_slice_num = (rec_slice_in.image_w + 2592 / 2 - 1) / (2592 / 2);
				layer_slice_width = (rec_slice_in.image_w / layer_slice_num + SLICE_W_ALIGN_V - 1) >> 2 << 2; //均匀切slice
				layer_slice_height = rec_slice_in.image_h;
			}

			param_ptr->slice_number[layer_id] = layer_slice_num;
			rec_slice_in.slice_w = layer_slice_width;
			rec_slice_in.slice_h = layer0_padding_height >> layer_id;
			rec_slice_in.overlap_left = overlap_rec_mode1->ov_left;
			rec_slice_in.overlap_right = overlap_rec_mode1->ov_right;
			rec_slice_in.overlap_up = overlap_rec_mode1->ov_up;
			rec_slice_in.overlap_down = overlap_rec_mode1->ov_down ;
			pr_debug("rec slice in L %d R %d\n", rec_slice_in.overlap_left, rec_slice_in.overlap_right);

			if (-1 == isp_drv_regions_fetch(&rec_slice_in, &rec_slice_out[layer_id]))
				return;
			pr_debug("rec layer 4 slice out %d\n", rec_slice_out[layer_id].regions[0].ex);
		}

		slice_flag = 1;
		for (layer_id = 2; layer_id < param_ptr->layerNum; layer_id++) {
			org_rec_slice_in.image_w = layer_id ? (layer0_padding_width >> layer_id) : (param_ptr->img_src_w);
			org_rec_slice_in.image_h = layer_id ? (layer0_padding_height >> layer_id) : (param_ptr->img_src_h);
			next_layer_width = org_rec_slice_in.image_w << 1;
			if (next_layer_width <= 2592)
				slice_flag = 0;

			if (!slice_flag) {
				layer_slice_num = 1;
				layer_slice_width  = org_rec_slice_in.image_w;
				layer_slice_height = org_rec_slice_in.image_h;
			} else {
				layer_slice_num = (org_rec_slice_in.image_w + 2592 / 2 - 1) / (2592 / 2);
				layer_slice_width = (org_rec_slice_in.image_w / layer_slice_num + SLICE_W_ALIGN_V - 1) >> 2 << 2; //均匀切slice
				layer_slice_height = org_rec_slice_in.image_h;
			}

			org_rec_slice_in.slice_w = layer_slice_width;
			org_rec_slice_in.slice_h = layer0_padding_height >> layer_id;
			org_rec_slice_in.overlap_left = 0;
			org_rec_slice_in.overlap_right = 0;
			org_rec_slice_in.overlap_up = 0;
			org_rec_slice_in.overlap_down = 0;

			//get slice region
			if (-1 == isp_drv_regions_fetch(&org_rec_slice_in, &org_rec_slice_out[layer_id]))
				return;
		}

		//get rec layer5～layer2 overlap
		//fetch0
		for (layer_id = 2; layer_id < param_ptr->layerNum; layer_id++) {
			for (i = 0; i < param_ptr->slice_number[layer_id]; i++) {
				index = i;
				rec_slice_overlap[layer_id][index].ov_left = org_rec_slice_out[layer_id].regions[index].sx - rec_slice_out[layer_id].regions[index].sx;
				rec_slice_overlap[layer_id][index].ov_right = rec_slice_out[layer_id].regions[index].ex - org_rec_slice_out[layer_id].regions[index].ex;
				rec_slice_overlap[layer_id][index].ov_up = org_rec_slice_out[layer_id].regions[index].sy - rec_slice_out[layer_id].regions[index].sy;
				rec_slice_overlap[layer_id][index].ov_down = rec_slice_out[layer_id].regions[index].ey - org_rec_slice_out[layer_id].regions[index].ey;

				param_ptr->fecth0_slice_region[layer_id][index].sx = rec_slice_out[layer_id].regions[index].sx;
				param_ptr->fecth0_slice_region[layer_id][index].ex = rec_slice_out[layer_id].regions[index].ex;
				param_ptr->fecth0_slice_region[layer_id][index].sy = rec_slice_out[layer_id].regions[index].sy;
				param_ptr->fecth0_slice_region[layer_id][index].ey = rec_slice_out[layer_id].regions[index].ey;
				param_ptr->fecth0_slice_overlap[layer_id][index].ov_left  = rec_slice_overlap[layer_id][index].ov_left ;
				param_ptr->fecth0_slice_overlap[layer_id][index].ov_right = rec_slice_overlap[layer_id][index].ov_right;
				param_ptr->fecth0_slice_overlap[layer_id][index].ov_up    = rec_slice_overlap[layer_id][index].ov_up;
				param_ptr->fecth0_slice_overlap[layer_id][index].ov_down  = rec_slice_overlap[layer_id][index].ov_down ;
			}
		}

		//fetch1 layer1~layer4
		for (layer_id = 1; layer_id < param_ptr->layerNum - 1; layer_id++) {
			for (i=0; i < param_ptr->slice_number[layer_id + 1]; i++) {
				index = i;

				//fetch1
				param_ptr->fecth1_slice_region[layer_id][index].sx = param_ptr->fecth0_slice_region[layer_id + 1][index].sx << 1;
				param_ptr->fecth1_slice_region[layer_id][index].ex = ((param_ptr->fecth0_slice_region[layer_id + 1][index].ex + 1) << 1) - 1;
				param_ptr->fecth1_slice_region[layer_id][index].sy = param_ptr->fecth0_slice_region[layer_id + 1][index].sy << 1;
				param_ptr->fecth1_slice_region[layer_id][index].ey = ((param_ptr->fecth0_slice_region[layer_id + 1][index].ey + 1) << 1) - 1;

				//store rec
				param_ptr->store_rec_slice_crop_overlap[layer_id][index].ov_left = (param_ptr->fecth0_slice_overlap[layer_id + 1][index].ov_left << 1);
				param_ptr->store_rec_slice_crop_overlap[layer_id][index].ov_right = (param_ptr->fecth0_slice_overlap[layer_id + 1][index].ov_right << 1);
				param_ptr->store_rec_slice_crop_overlap[layer_id][index].ov_up = (param_ptr->fecth0_slice_overlap[layer_id + 1][index].ov_up << 1);
				param_ptr->store_rec_slice_crop_overlap[layer_id][index].ov_down = (param_ptr->fecth0_slice_overlap[layer_id + 1][index].ov_down << 1);

				param_ptr->store_rec_slice_overlap[layer_id][index].ov_left = 0;
				param_ptr->store_rec_slice_overlap[layer_id][index].ov_right = 0;
				param_ptr->store_rec_slice_overlap[layer_id][index].ov_up = 0;
				param_ptr->store_rec_slice_overlap[layer_id][index].ov_down = 0;

				param_ptr->store_rec_slice_region[layer_id][index].sx = (param_ptr->fecth0_slice_region[layer_id + 1][index].sx << 1) + (rec_slice_overlap[layer_id + 1][index].ov_left << 1);
				param_ptr->store_rec_slice_region[layer_id][index].ex = (((param_ptr->fecth0_slice_region[layer_id + 1][index].ex + 1) << 1) - 1) - (rec_slice_overlap[layer_id + 1][index].ov_right << 1);
				param_ptr->store_rec_slice_region[layer_id][index].sy = (param_ptr->fecth0_slice_region[layer_id + 1][index].sy << 1) + (rec_slice_overlap[layer_id + 1][index].ov_up << 1);
				param_ptr->store_rec_slice_region[layer_id][index].ey = (((param_ptr->fecth0_slice_region[layer_id + 1][index].ey + 1) << 1) - 1) - (rec_slice_overlap[layer_id + 1][index].ov_down << 1);
			}
		}
	}
}

void alg_slice_calc_drv_overlap(struct alg_slice_drv_overlap *param_ptr)
{
	int image_w;
	int image_h;
	int SLICE_W_ALIGN_V;
	int layer_num;

	struct ltm_rgb_stat_param_t *ltm_sta_p = &param_ptr->ltm_sat;
	struct alg_overlap_info ov_pipe;
	struct alg_overlap_info ov_Y = {0};
	struct alg_overlap_info ov_UV = {0};
	struct alg_overlap_info ov_pipe_layer0 = {0};
	struct alg_overlap_info overlap_rec = {0};
	struct alg_overlap_info overlap_rec_mode1 = {0};

	struct isp_block_drv_t ynr_param;
	struct isp_block_drv_t cnr_param;
	struct isp_block_drv_t pyramid_rec_param;
	struct isp_block_drv_t dewarping_param;
	struct isp_block_drv_t post_cnr_param;
	struct isp_block_drv_t nr3d_param;
	struct isp_block_drv_t yuv420_to_rgb10_param;
	struct isp_block_drv_t ee_param;
	struct isp_block_drv_t cnr_new_param;

	struct alg_slice_regions orgRegion;
	struct alg_slice_regions maxRegion;
	struct alg_fetch_region slice_in;
	struct alg_slice_regions slice_out;
	struct alg_slice_regions final_slice_regions;
	struct alg_slice_regions *add_rec_slice_out = NULL;

	int overlap_left_max;
	int overlap_right_max;
	int overlap_up_max;
	int overlap_down_max;

	int layer0_padding_width;
	int layer0_padding_height;
	int slice_num;
	int slice_id;
	int layer_id;
	uint32_t chk_sum_clr_flag = 0;

	struct alg_pyramid_ovlap_temp pyramid_ovlap_temp;
	memset(&pyramid_ovlap_temp, 0, sizeof(struct alg_pyramid_ovlap_temp));
	overlap_left_max = 0;
	overlap_right_max = 0;
	overlap_up_max = 0;
	overlap_down_max = 0;
	SLICE_W_ALIGN_V = 4;

	if (!param_ptr->pyramid_rec_bypass) {
		image_w = param_ptr->img_src_w;
		image_h = param_ptr->img_src_h;
	} else {
		image_w = param_ptr->img_w;
		image_h = param_ptr->img_h;
	}

	param_ptr->layerNum = param_ptr->input_layer_id + 1;
	if (param_ptr->pyramid_rec_bypass) {
		param_ptr->layerNum = 1;
		param_ptr->input_layer_id = 0;
	}
	layer_num = param_ptr->layerNum;
	param_ptr->scaler1.layerNum = param_ptr->layerNum;
	param_ptr->scaler2.layerNum = param_ptr->layerNum;

#if !(FBC_SUPPORT)
	param_ptr->nr3d_bd_FBC_en = 0;
	param_ptr->scaler1.FBC_enable = 0;
	param_ptr->scaler2.FBC_enable = 0;
#endif

	if (param_ptr->crop_en && 0 == param_ptr->crop_mode) {
		image_w = param_ptr->crop_w;
		image_h = param_ptr->crop_h;
		param_ptr->img_src_w = param_ptr->crop_w;
		param_ptr->img_src_h = param_ptr->crop_h;
	}

	ov_pipe.ov_left = 0;
	ov_pipe.ov_right = 0;
	ov_pipe.ov_up = 0;
	ov_pipe.ov_down = 0;

	core_drv_dewarping_init_block(&dewarping_param);//dewarping
	if(0 == param_ptr->dewarping_bypass) {
		ov_pipe.ov_left = (dewarping_param.left * param_ptr->dewarping_width + 5183) / 5184; // 20M width：5184
		ov_pipe.ov_right = (dewarping_param.right * param_ptr->dewarping_width + 5183) / 5184;// 20M width：5184
		ov_pipe.ov_up = (dewarping_param.up * param_ptr->dewarping_height + 5183) / 5184; // 20M width：5184
		ov_pipe.ov_down = (dewarping_param.down * param_ptr->dewarping_height + 5183) / 5184;// 20M width：5184
	}

	core_drv_cnr_init_block(&post_cnr_param);//post_cnr
	CAL_OVERLAP(param_ptr->post_cnr_bypass, post_cnr_param);

	core_drv_cnr_init_block(&post_cnr_param);//post_cnr
	CAL_OVERLAP(param_ptr->post_cnr_bypass, post_cnr_param);

	core_drv_nr3d_init_block(&nr3d_param);
	CAL_OVERLAP(param_ptr->nr3d_bd_bypass, nr3d_param);

	core_drv_yuv420_to_rgb10_init_block(&yuv420_to_rgb10_param);
	CAL_OVERLAP(param_ptr->yuv420_to_rgb10_bypass, yuv420_to_rgb10_param);

	slice_in.image_w = image_w;
	slice_in.image_h = image_h;
	slice_in.slice_w = param_ptr->slice_w;
	slice_in.slice_h = param_ptr->slice_h;
	slice_in.overlap_left = 0;
	slice_in.overlap_right = 0;
	slice_in.overlap_up = 0;
	slice_in.overlap_down = 0;

	if (-1 == isp_drv_regions_fetch(&slice_in, &slice_out))
		return;

	if (!ltm_sta_p->bypass) {
		struct slice_drv_overlap_info ltm_sta_overlap;
		memset(&ltm_sta_overlap, 0, sizeof(struct slice_drv_overlap_info));
		ltm_sta_overlap = ltm_rgb_sta_getOverlap(&slice_out, ltm_sta_p);
		ov_pipe.ov_left += ltm_sta_overlap.ov_left;
		ov_pipe.ov_right += ltm_sta_overlap.ov_right;
		ov_pipe.ov_up += ltm_sta_overlap.ov_up;
		ov_pipe.ov_down += ltm_sta_overlap.ov_down;
	}

	// Y domain
	{
		core_drv_ee_init_block(&ee_param);
		if (0 == param_ptr->ee_bypass) {
			ov_Y.ov_left += ee_param.left;
			ov_Y.ov_right += ee_param.right;
			ov_Y.ov_up += ee_param.up;
			ov_Y.ov_down += ee_param.down;
		}
	}

	// UV domain
	{
		core_drv_cnrnew_init_block(&cnr_new_param);
		if (0 == param_ptr->cnr_new_bypass) {
			ov_UV.ov_left += (cnr_new_param.left << 1);
			ov_UV.ov_right += (cnr_new_param.right << 1);
			ov_UV.ov_up += cnr_new_param.up;
			ov_UV.ov_down += cnr_new_param.down;
		}
	}

	// Y and UV MAX
	{
		ov_pipe.ov_left += (ov_Y.ov_left > ov_UV.ov_left) ? ov_Y.ov_left : ov_UV.ov_left;
		ov_pipe.ov_right += (ov_Y.ov_right > ov_UV.ov_right) ? ov_Y.ov_right : ov_UV.ov_right;
		ov_pipe.ov_up += (ov_Y.ov_up > ov_UV.ov_up) ? ov_Y.ov_up : ov_UV.ov_up;
		ov_pipe.ov_down += (ov_Y.ov_down > ov_UV.ov_down) ? ov_Y.ov_down : ov_UV.ov_down;
	}

	//set user overlap
	if (param_ptr->offlineCfgOverlap_en) {
		ov_pipe.ov_left = param_ptr->offlineCfgOverlap_left;
		ov_pipe.ov_right = param_ptr->offlineCfgOverlap_right;
		ov_pipe.ov_up = param_ptr->offlineCfgOverlap_up;
		ov_pipe.ov_down = param_ptr->offlineCfgOverlap_down;
	}

	//overlap 2 align
	ov_pipe.ov_left = (ov_pipe.ov_left + 1) >> 1 << 1;
	ov_pipe.ov_right = (ov_pipe.ov_right + 1) >> 1 << 1;
	ov_pipe.ov_up = (ov_pipe.ov_up + 1) >> 1 << 1;
	ov_pipe.ov_down = (ov_pipe.ov_down + 1) >> 1 << 1;

	isp_drv_regions_set(&slice_out, &maxRegion);
	isp_drv_regions_set(&slice_out, &orgRegion);

	/* Add temp sub function to avoid stack size overflow */
	{
		struct alg_scaler_ovlap_temp scl_ovlap_temp = {0};
		scl_ovlap_temp.image_w = image_w;
		scl_ovlap_temp.image_h = image_h;
		scl_ovlap_temp.param_ptr = param_ptr;
		scl_ovlap_temp.maxRegion = &maxRegion;
		scl_ovlap_temp.orgRegion = &orgRegion;
		alg_slice_scaler_overlap_temp(&scl_ovlap_temp);
	}

	isp_drv_regions_max(&maxRegion, &orgRegion, &maxRegion);
	slice_in.image_w = image_w;
	slice_in.image_h = image_h;
	slice_in.slice_w = param_ptr->slice_w;
	slice_in.slice_h = param_ptr->slice_h;
	slice_in.overlap_left = ov_pipe.ov_left;
	slice_in.overlap_right = ov_pipe.ov_right;
	slice_in.overlap_up = ov_pipe.ov_up;
	slice_in.overlap_down = ov_pipe.ov_down;

	if (-1 == isp_drv_regions_fetch_ref(&slice_in, &maxRegion, &final_slice_regions))
		return;

	param_ptr->slice_rows = final_slice_regions.rows;
	param_ptr->slice_cols = final_slice_regions.cols;

	do {
		int i,j,index,rows,cols;
		rows = param_ptr->slice_rows;
		cols = param_ptr->slice_cols;

		for (i = 0; i < rows; i++) {
			for (j = 0; j < cols; j++) {
				index = i * cols + j;
				param_ptr->slice_region[index].sx = final_slice_regions.regions[index].sx;
				param_ptr->slice_region[index].ex = final_slice_regions.regions[index].ex;
				param_ptr->slice_region[index].sy = final_slice_regions.regions[index].sy;
				param_ptr->slice_region[index].ey = final_slice_regions.regions[index].ey;
				param_ptr->slice_overlap[index].ov_left = orgRegion.regions[index].sx - final_slice_regions.regions[index].sx;
				param_ptr->slice_overlap[index].ov_right = final_slice_regions.regions[index].ex - orgRegion.regions[index].ex;
				param_ptr->slice_overlap[index].ov_up = orgRegion.regions[index].sy - final_slice_regions.regions[index].sy;
				param_ptr->slice_overlap[index].ov_down = final_slice_regions.regions[index].ey - orgRegion.regions[index].ey;

				if (param_ptr->slice_overlap[index].ov_left > overlap_left_max)
					overlap_left_max = param_ptr->slice_overlap[index].ov_left;
				if (param_ptr->slice_overlap[index].ov_right > overlap_right_max)
					overlap_right_max = param_ptr->slice_overlap[index].ov_right;
				if (param_ptr->slice_overlap[index].ov_up > overlap_up_max)
					overlap_up_max = param_ptr->slice_overlap[index].ov_up;
				if (param_ptr->slice_overlap[index].ov_down > overlap_down_max)
					overlap_down_max = param_ptr->slice_overlap[index].ov_down;
			}
		}

		ov_pipe_layer0.ov_left = overlap_left_max;
		ov_pipe_layer0.ov_right = overlap_right_max;
		ov_pipe_layer0.ov_up = overlap_up_max;
		ov_pipe_layer0.ov_down = overlap_down_max;
	}while(0);

{
	const int SLICE_W_ALIGN_V = 4;
	int slice_width_align;
	int slice_height_align;
	int dewarping_blk_align = DST_MBLK_SIZE;
	if (param_ptr->offline_slice_mode == 0) {
		layer_num = param_ptr->layerNum;
	} else if (param_ptr->offline_slice_mode == 1) {
		layer_num = 2;
	}
	slice_width_align = (1 << (layer_num + 1));
	slice_height_align = (1 << layer_num);

	if (!param_ptr->pyramid_rec_bypass) {
		if ((param_ptr->slice_w % slice_width_align != 0) && (param_ptr->slice_w < param_ptr->img_src_w))
			return;
	} else {
		if ((param_ptr->slice_w % SLICE_W_ALIGN_V != 0) && (param_ptr->slice_w < param_ptr->img_src_w))
			return;
	}

	if (!param_ptr->dewarping_bypass) {
		if ((param_ptr->slice_w % dewarping_blk_align != 0) && (param_ptr->slice_w < param_ptr->img_src_w))
			return;
	}

	// 4 align
	if (!param_ptr->pyramid_rec_bypass) {
		ov_pipe_layer0.ov_left = (ov_pipe_layer0.ov_left + slice_width_align - 1) >> (layer_num + 1) << (layer_num + 1);
		ov_pipe_layer0.ov_right = (ov_pipe_layer0.ov_right + slice_width_align - 1)>> (layer_num + 1) << (layer_num + 1);
		ov_pipe_layer0.ov_up = (ov_pipe_layer0.ov_up + slice_height_align - 1) >> layer_num << layer_num;
		ov_pipe_layer0.ov_down = (ov_pipe_layer0.ov_down + slice_height_align - 1) >> layer_num << layer_num;
	} else {
		ov_pipe_layer0.ov_left = (ov_pipe_layer0.ov_left + SLICE_W_ALIGN_V - 1) >> 2 << 2;
		ov_pipe_layer0.ov_right = (ov_pipe_layer0.ov_right + SLICE_W_ALIGN_V - 1) >> 2 << 2;
		ov_pipe_layer0.ov_up = (ov_pipe_layer0.ov_up + 1) >> 1 << 1;
		ov_pipe_layer0.ov_down = (ov_pipe_layer0.ov_down + 1) >> 1 << 1;
	}

	// 8 align for dewarping
	if (!param_ptr->dewarping_bypass)
		ov_pipe_layer0.ov_left = (ov_pipe_layer0.ov_left + dewarping_blk_align - 1) >> 3 << 3;

	if (-1 == isp_drv_regions_alignfetch_ref(&slice_in, &slice_out, &ov_pipe_layer0, &final_slice_regions))
		return;
	do {
		int i, j, index, rows, cols;
		rows = param_ptr->slice_rows;
		cols = param_ptr->slice_cols;
		for (i = 0; i < rows; i++) {
			for (j = 0; j < cols; j++) {
				index = i * cols +j;
				param_ptr->slice_region[index].sx = final_slice_regions.regions[index].sx;
				param_ptr->slice_region[index].ex = final_slice_regions.regions[index].ex;
				param_ptr->slice_region[index].sy = final_slice_regions.regions[index].sy;
				param_ptr->slice_region[index].ey = final_slice_regions.regions[index].ey;
				param_ptr->slice_overlap[index].ov_left = orgRegion.regions[index].sx - final_slice_regions.regions[index].sx;
				param_ptr->slice_overlap[index].ov_right = final_slice_regions.regions[index].ex - orgRegion.regions[index].ex;
				param_ptr->slice_overlap[index].ov_up = orgRegion.regions[index].sy - final_slice_regions.regions[index].sy;
				param_ptr->slice_overlap[index].ov_down = final_slice_regions.regions[index].ey - orgRegion.regions[index].ey;
			}
		}
	} while (0);
	//////////////////////////////////////////////////////////////////////////
	//rec overlap
	core_drv_ynr_init_block(&ynr_param);
	CAL_REC_OVERLAP(param_ptr->ynr_bypass, ynr_param);

	core_drv_cnr_init_block(&cnr_param);
	CAL_REC_OVERLAP(param_ptr->cnr_bypass, cnr_param);
	CAL_REC_OVERLAP(param_ptr->cnr_bypass, cnr_param);

	core_drv_pyd_rec_init_block(&pyramid_rec_param);
	CAL_REC_OVERLAP(param_ptr->pyramid_rec_bypass, pyramid_rec_param);

	if (param_ptr->offline_slice_mode == 1) {
		overlap_rec_mode1.ov_left = overlap_rec.ov_left;
		overlap_rec_mode1.ov_right = overlap_rec.ov_right;
		overlap_rec_mode1.ov_up = overlap_rec.ov_up;
		overlap_rec_mode1.ov_down = overlap_rec.ov_down;
		overlap_rec_mode1.ov_left = (overlap_rec_mode1.ov_left + SLICE_W_ALIGN_V - 1) >> 2 << 2;
		overlap_rec_mode1.ov_right = (overlap_rec_mode1.ov_right + SLICE_W_ALIGN_V - 1) >> 2 << 2;
		overlap_rec_mode1.ov_up = (overlap_rec_mode1.ov_up + 1) >> 1 << 1;
		overlap_rec_mode1.ov_down = (overlap_rec_mode1.ov_down + 1) >> 1 << 1;
	}

	//4 align
	overlap_rec.ov_left = ((overlap_rec.ov_left << 1) + SLICE_W_ALIGN_V - 1) >> 2 << 2;
	overlap_rec.ov_right = ((overlap_rec.ov_right << 1) + SLICE_W_ALIGN_V - 1) >> 2 << 2;
	overlap_rec.ov_up = ((overlap_rec.ov_up << 1)+ 1) >> 1 << 1;
	overlap_rec.ov_down = ((overlap_rec.ov_down << 1) + 1) >> 1 << 1;

	// 8 align for dewarping
	if (!param_ptr->dewarping_bypass)
		overlap_rec.ov_left = ((overlap_rec.ov_left << 1) + dewarping_blk_align - 1) >> 3 << 3;
}

	// layer0 after padding
	if (param_ptr->crop_en && param_ptr->crop_mode == 0) {
		layer0_padding_width = image_w;
		layer0_padding_height = image_h;
	} else {
		layer0_padding_width = (param_ptr->input_layer_w << param_ptr->input_layer_id);
		layer0_padding_height = (param_ptr->input_layer_h << param_ptr->input_layer_id);
	}

	slice_num = param_ptr->slice_rows * param_ptr->slice_cols;

	/////////////////////////////////////////////////////初始化每层的slice个数
	for(layer_id = 0; layer_id < param_ptr->layerNum; layer_id++)
		param_ptr->slice_number[layer_id] = slice_num;

	/* Add temp sub function to avoid stack size overflow */
	{
		pyramid_ovlap_temp.param_ptr = param_ptr;
		pyramid_ovlap_temp.layer_id = layer_id;
		pyramid_ovlap_temp.layer_num = layer_num;
		pyramid_ovlap_temp.overlap_rec = &overlap_rec;
		pyramid_ovlap_temp.slice_num = slice_num;
		pyramid_ovlap_temp.layer0_padding_height = layer0_padding_height;
		pyramid_ovlap_temp.layer0_padding_width = layer0_padding_width;
		pyramid_ovlap_temp.ov_pipe_layer0 = &ov_pipe_layer0;
		pyramid_ovlap_temp.overlap_rec_mode1 = &overlap_rec_mode1;
		add_rec_slice_out = &pyramid_ovlap_temp.add_rec_slice_out[0];
		alg_slice_pyramid_ovlap_temp(&pyramid_ovlap_temp);
	}

	//calculation scaler slice param
	do {
		int i, j, index, rows, cols;
		struct slice_drv_scaler_slice_init_context slice_context;
		rows = param_ptr->slice_rows;
		cols = param_ptr->slice_cols;

		for (i = 0; i<rows; i++) {
			for (j = 0; j<cols; j++) {
				index = i * cols + j;
				slice_context.slice_index = index;
				slice_context.rows = rows;
				slice_context.cols = cols;
				slice_context.slice_row_no = i;
				slice_context.slice_col_no = j;
				slice_context.slice_w = add_rec_slice_out->regions[index].ex - add_rec_slice_out->regions[index].sx + 1;
				slice_context.slice_h = add_rec_slice_out->regions[index].ey - add_rec_slice_out->regions[index].sy + 1;

				pr_debug("regin sx %d ex %d sy %d ey %d\n", param_ptr->scaler1.region_input[0].sx,
					param_ptr->scaler1.region_input[0].ex, param_ptr->scaler1.region_input[0].sy,
					param_ptr->scaler1.region_input[0].ey);
				if (!param_ptr->scaler1.bypass)
					scaler_slice_init(&add_rec_slice_out->regions[index], &slice_context, &param_ptr->scaler1);

				if (!param_ptr->scaler2.bypass)
					scaler_slice_init(&add_rec_slice_out->regions[index], &slice_context, &param_ptr->scaler2);
			}
		}
	}while(0);

	/*calculation thumbnailScaler slice param*/
	if (!param_ptr->thumbnailscaler.bypass)
		thumbnailscaler_slice_init(param_ptr);
	else {
		for (slice_id = 0; slice_id < slice_num; slice_id++) {
			param_ptr->thumbnail_scaler.sliceParam[slice_id].y_slice_des_size_hor =
				param_ptr->slice_region[slice_id].ex - param_ptr->slice_region[slice_id].sx + 1;
			param_ptr->thumbnail_scaler.sliceParam[slice_id].y_slice_des_size_ver =
				param_ptr->slice_region[slice_id].ey - param_ptr->slice_region[slice_id].sy + 1;
		}
	}

	for (slice_id = 0; slice_id < slice_num; slice_id++) {
		if (param_ptr->thumbnail_scaler.sliceParam[slice_id].bypass == 0 && chk_sum_clr_flag == 0) {
			param_ptr->thumbnail_scaler.sliceParam[slice_id].chk_sum_clr = 1;
			chk_sum_clr_flag = 1;
			continue;
		}
		if (chk_sum_clr_flag)
			param_ptr->thumbnail_scaler.sliceParam[slice_id].chk_sum_clr = 0;
		if (slice_id == 0)
			param_ptr->thumbnail_scaler.sliceParam[slice_id].chk_sum_clr = 1;
	}
}
