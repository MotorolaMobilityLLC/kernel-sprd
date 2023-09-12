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

#include <linux/slab.h>
#include <sprd_mm.h>

#include "cam_hw.h"
#include "cam_types.h"
#include "isp_interface.h"
#include "isp_core.h"
#include "alg_isp_overlap.h"
#include "isp_slice.h"
#include "cam_scaler_ex.h"
#include "alg_slice_calc.h"

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "ALG_ISP_OVERLAP: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define TM_BIN_NUM_BIT      7
#define TM_TILE_NUM_MIN     4
#define TM_TILE_NUM_MAX     8
#define TM_TILE_WIDTH_MIN   160
#define TM_TILE_MAX_SIZE    (1<<16)
#define FBC_SUPPORT         0
#define CAL_OVERLAP(ctrl_bypass, module_param) \
		do { \
			if (!(ctrl_bypass)) { \
				ov_pipe.ov_left += module_param.left; \
				ov_pipe.ov_right += module_param.right; \
				ov_pipe.ov_up    += module_param.up; \
				ov_pipe.ov_down  += module_param.down; \
			} \
		} while (0)

#define CLIP(x, maxv, minv) \
		do { \
			if (x > maxv) \
				x = maxv; \
			else if (x < minv) \
				x = minv; \
		} while (0)

const uint8_t YUV_DECI_MAP[] = {2,4,8,16};
static uint8_t SCALER_YUV_DECI_MAP[] = {2,4,8,16};
static uint8_t SCALER_YUV_DECI_OFFSET_MAP[] = {0,1,3,7};
static uint32_t scaler1_coeff_buf[ISP_SC_COEFF_BUF_SIZE] = {0};
static uint32_t scaler2_coeff_buf[ISP_SC_COEFF_BUF_SIZE] = {0};
int scaler_path = -1;
struct yuvscaler_param_t yuvscaler_param = {0};

void core_drv_ppe_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 64;
	block_ptr->right = 64;
	block_ptr->up = 0;
	block_ptr->down = 0;
}

void core_drv_bpc_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 3;
	block_ptr->right = 3;
	block_ptr->up = 2;
	block_ptr->down = 2;
}

void core_drv_raw_gtm_stat_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 2;
	block_ptr->right = 1;
	block_ptr->up = 1;
	block_ptr->down = 1;
}

void core_drv_raw_gtm_map_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 2;
	block_ptr->right = 1;
	block_ptr->up = 1;
	block_ptr->down = 1;
}

void core_drv_nr3d_me_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 32;
	block_ptr->right = 32;
	block_ptr->up = 32;
	block_ptr->down = 32;
}

void core_drv_af_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 100;
	block_ptr->right = 0;
	block_ptr->up = 4;
	block_ptr->down = 4;
}

void core_drv_imbalance_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 3;
	block_ptr->right = 3;
	block_ptr->up = 2;
	block_ptr->down = 2;
}

void core_drv_nlm_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 5;
	block_ptr->right = 5;
	block_ptr->up = 5;
	block_ptr->down = 5;
}

void core_drv_cfa_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 4;
	block_ptr->right = 4;
	block_ptr->up = 4;
	block_ptr->down = 4;
}

void core_drv_nr3d_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 1;
	block_ptr->right = 1;
	block_ptr->up = 1;
	block_ptr->down = 1;
}

void core_drv_ltmsta_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 1;
	block_ptr->right = 1;
	block_ptr->up = 1;
	block_ptr->down = 1;
}

void core_drv_pre_cnr_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 12;
	block_ptr->right = 12;
	block_ptr->up = 0;
	block_ptr->down = 0;
}

void core_drv_ynr_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 12;
	block_ptr->right = 12;
	block_ptr->up = 12;
	block_ptr->down = 12;;
}

void core_drv_ee_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 4;
	block_ptr->right = 4;
	block_ptr->up = 4;
	block_ptr->down = 4;
}

void core_drv_cnrnew_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 3;
	block_ptr->right = 3;
	block_ptr->up = 4;
	block_ptr->down = 4;
}

void core_drv_post_cnr_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 2;
	block_ptr->right = 2;
	block_ptr->up = 5;
	block_ptr->down = 5;
}

void core_drv_iir_cnr_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 12;
	block_ptr->right = 12;
	block_ptr->up = 1;
	block_ptr->down = 1;
}

void core_drv_yuv420to422_init_block(struct isp_block_drv_t *block_ptr)
{
	block_ptr->left = 0;
	block_ptr->right = 0;
	block_ptr->up = 0;
	block_ptr->down = 1;
}

static int core_drv_get_fetch_fmt(enum isp_fetch_format fmt)
{
	int fetch_fmt = 0;

	switch (fmt) {
		case ISP_FETCH_RAW10:
		case ISP_FETCH_CSI2_RAW10:
			fetch_fmt = 0;
			break;
		case ISP_FETCH_FULL_RGB10:
			fetch_fmt = 1;
			break;
		case ISP_FETCH_YUV422_2FRAME:
		case ISP_FETCH_YVU422_2FRAME:
			fetch_fmt = 3;
			break;
		case ISP_FETCH_YUV420_2FRAME:
		case ISP_FETCH_YVU420_2FRAME:
		case ISP_FETCH_YUV420_2FRAME_10:
		case ISP_FETCH_YVU420_2FRAME_10:
		case ISP_FETCH_YUV420_2FRAME_MIPI:
		case ISP_FETCH_YVU420_2FRAME_MIPI:
			fetch_fmt = 4;
			break;
		default:
			pr_err("fail to get fetch fmt %d\n", fmt);
			fetch_fmt = -1;
			break;
	}

	return fetch_fmt;
}

static int isp_drv_regions_fetch_ref(const struct isp_drv_region_fetch_t *fetch_param,
		const struct isp_drv_regions_t *r_ref, struct isp_drv_regions_t *r_out)
{
	int imgW = fetch_param->image_w;
	int imgH = fetch_param->image_h;

	int overlapUp = fetch_param->overlap_up;
	int overlapDown = fetch_param->overlap_down;
	int overlapLeft = fetch_param->overlap_left;
	int overlapRight= fetch_param->overlap_right;

	int row_num = r_ref->rows;
	int col_num = r_ref->cols;

	int i,j,index;
	struct isp_drv_region_fetch_context context;

	pr_debug("overlap left %d, right %d, up %d, down %d\n",
		overlapLeft, overlapRight, overlapUp, overlapDown);

	r_out->rows = row_num;
	r_out->cols = col_num;

	for (i = 0;i < row_num; i++) {
		for (j = 0; j < col_num; j++) {
			index = i*col_num + j;

			context.s_row = r_ref->regions[index].sy;
			context.s_col = r_ref->regions[index].sx;
			context.e_row = r_ref->regions[index].ey;
			context.e_col = r_ref->regions[index].ex;

			context.overlap_left = overlapLeft;
			context.overlap_right = overlapRight;
			context.overlap_up = overlapUp;
			context.overlap_down = overlapDown;

			pr_debug("row %d, col %d, context: s_row %d, s_col %d, e_row %d, e_col %d; overlap(%d, %d, %d, %d)\n",
				i, j, context.s_row, context.s_col, context.e_row, context.e_col,
				context.overlap_left, context.overlap_right, context.overlap_up, context.overlap_down);

			{
				/* l-top */
				if ((0 == i) && (0 == j)) {
					context.overlap_left = 0;
					context.overlap_up = 0;

					context.s_row = 0;
					context.s_col = 0;
				}
				/* r-top */
				if ((0 == i) && (col_num-1 == j)) {
					context.overlap_right = 0;
					context.overlap_up = 0;

					context.s_row = 0;
					context.e_col = (imgW -1);
				}
				/* l-bottom */
				if ((row_num-1 == i) && (0 == j)) {
					context.overlap_left = 0;
					context.overlap_down = 0;

					context.s_col = 0;
					context.e_row = (imgH - 1);
				}
				/* r-bottom */
				if ((row_num-1 == i) && (col_num-1 == j)) {
					context.overlap_right = 0;
					context.overlap_down = 0;

					context.e_row = (imgH -1);
					context.e_col = (imgW -1);
				}
				/* up */
				if ((0 == i) && (0<j && j<col_num-1)) {
					context.overlap_up = 0;
					context.s_row = 0;
				}
				/* down */
				if ((row_num-1 == i) && (0<j && j<col_num-1)) {
					context.overlap_down = 0;
					context.e_row = (imgH - 1);
				}
				/* left */
				if ((0 == j) && (0<i && i<row_num-1)) {
					context.overlap_left = 0;
					context.s_col = 0;
				}
				/* right */
				if ((col_num-1 == j) && (0<i && i<row_num-1)) {
					context.overlap_right = 0;
					context.e_col = (imgW - 1);
				}
			}

			pr_debug("context overlap: left %d, right %d, up %d, down%d\n",
				context.overlap_left, context.overlap_right, context.overlap_up, context.overlap_down);
			pr_debug("context s_row %d, s_col %d, e_row %d, e_col %d\n",
				context.s_row, context.s_col, context.e_row, context.e_col);

			context.s_row -= context.overlap_up;
			context.e_row += context.overlap_down;
			context.s_col -= context.overlap_left;
			context.e_col += context.overlap_right;

			/* add overlap overflow return -1 */
			if (context.s_col < 0) {
				pr_err("context.s_col %d", context.s_col);
				return -1;
			}
			if (context.s_row < 0) {
				pr_err("context.s_col %d\n", context.s_row);
				return -1;
			}
			if (context.e_col >= imgW) {
				pr_err("context.e_col %d, imgW %d\n", context.e_col, imgW);
				return -1;
			}
			if (context.e_row >= imgH) {
				pr_err("context.e_row %d, imgH %d\n", context.e_row, imgH);
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

static int isp_drv_regions_fetch(const struct isp_drv_region_fetch_t *fetch_param, struct isp_drv_regions_t *r_out)
{
	int imgW = fetch_param->image_w;
	int imgH = fetch_param->image_h;

	int sliceW = fetch_param->slice_w;
	int sliceH = fetch_param->slice_h;

	int col_num;
	int row_num;
	int i,j,index,ret_val;
	struct isp_drv_regions_t region_temp;
	struct isp_drv_region_fetch_context context;

	if (sliceW <= 0) {
		sliceW = imgW;
	}
	if (sliceH <= 0) {
		sliceH = imgH;
	}
	col_num = imgW / sliceW + (imgW % sliceW ? 1 : 0);
	row_num = imgH / sliceH + (imgH % sliceH ? 1 : 0);

	region_temp.rows = row_num;
	region_temp.cols = col_num;

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

	ret_val = isp_drv_regions_fetch_ref(fetch_param, &region_temp, r_out);

	return ret_val;
}

void isp_drv_regions_reset(struct isp_drv_regions_t *r)
{
	memset(r,0,sizeof(struct isp_drv_regions_t));
}

static void isp_drv_region_set(const struct isp_drv_region_t *r, struct isp_drv_region_t *r_out)
{
	r_out->sx = r->sx;
	r_out->ex = r->ex;
	r_out->sy = r->sy;
	r_out->ey = r->ey;
}

static void isp_drv_region_max(const struct isp_drv_region_t *r1, const struct isp_drv_region_t *r2,
		struct isp_drv_region_t *r_out)
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

static void isp_drv_regions_set(const struct isp_drv_regions_t *r, struct isp_drv_regions_t *r_out)
{
	int i,j,index;
	int rows = r->rows;
	int cols = r->cols;

	r_out->rows = rows;
	r_out->cols = cols;

	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			index = i*cols + j;
			isp_drv_region_set(&r->regions[index], &r_out->regions[index]);
		}
	}
}

void isp_drv_regions_3dnr(const struct isp_drv_regions_t *r_ref, struct isp_drv_regions_t *r_out,
		int ALIGN_W_V, int ALIGN_H_V, int v_flag)
{
	int index,next_index;
	int move_size_x, move_size_y;
	int rows = r_ref->rows;
	int cols = r_ref->cols;
	int row,col;
	const struct isp_drv_region_t * src_region;
	struct isp_drv_region_t * dst_region;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			index = row * cols + col;

			src_region = &r_ref->regions[index];
			dst_region = &r_out->regions[index];

			move_size_x = (src_region->ex - dst_region->ex) / ALIGN_W_V * ALIGN_W_V;
			if (0 == v_flag) {
				int old_w = dst_region->ex + 1;
				int new_w = (old_w + ALIGN_W_V/2) / ALIGN_W_V * ALIGN_W_V;
				move_size_x = new_w - old_w;
			}

			if(col != cols-1) {
				dst_region->ex += move_size_x;
				next_index = row*cols + (col + 1);
				r_out->regions[next_index].sx += move_size_x;
			}

			move_size_y = (dst_region->ey - dst_region->ey) / ALIGN_H_V * ALIGN_H_V;
			if(0 == v_flag) {
				int old_h = dst_region->ey + 1;
				int new_h = (old_h + ALIGN_H_V / 2) / ALIGN_H_V * ALIGN_H_V;
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

static void scaler_calculate_region(
	const struct isp_drv_regions_t *r_ref,
	struct isp_drv_regions_t *r_out,
	struct yuvscaler_param_t *core_param,
	int v_flag,
	struct slice_drv_overlap_scaler_param *scaler_param_ptr)
{
	int i,j,index;
	int output_slice_end;
	uint16_t prev_row_end, prev_col_end;
	int rows = r_ref->rows;
	int cols = r_ref->cols;
	struct SliceWnd wndInTemp;
	struct SliceWnd wndOutTemp;
	struct slice_drv_scaler_phase_info phaseTemp;

	phaseTemp.init_phase_hor = 0;
	phaseTemp.init_phase_ver = 0;

	prev_row_end = 0;
	for ( i = 0;i < rows; i++) {
		prev_col_end = 0;
		for ( j = 0;j < cols;j++) {
			index = i * cols + j;

			/* hor */
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

				int output_slice_start;
				int output_slice_size;

				if (j == cols - 1) {
					output_pixel_align = 2;
				}

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

				output_slice_start = prev_col_end;
				output_slice_size = output_slice_end - output_slice_start;

				wndOutTemp.s_col = output_slice_start;
				wndOutTemp.e_col = output_slice_end - 1;

				if (output_slice_size > 0) {
					int input_pixel_align = 2;
					calc_scaler_input_slice_info(trim_start, trim_size, deci, scl_en, scl_factor_in, scl_factor_out, scl_tap, init_phase,
					output_slice_start, output_slice_size, input_pixel_align, &input_slice_start, &input_slice_size, &init_phase);

					phaseTemp.init_phase_hor = init_phase;

					wndInTemp.s_col = input_slice_start;
					wndInTemp.e_col = wndInTemp.s_col + input_slice_size - 1;
				} else {
					wndInTemp.s_col = r_ref->regions[index].sx;
					wndInTemp.e_col = r_ref->regions[index].ex;
				}

				prev_col_end = output_slice_end;
			}

			/* ver */
			{
				int scl_tap = 0;
				int trim_start  = core_param->trim0_info.trim_start_y;
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
					if (core_param->input_pixfmt == YUV422)
						scl_tap = MAX(core_param->scaler_info.scaler_y_ver_tap, core_param->scaler_info.scaler_uv_ver_tap) + 2;
					else if (core_param->input_pixfmt == YUV420)
						scl_tap = MAX(core_param->scaler_info.scaler_y_ver_tap, core_param->scaler_info.scaler_uv_ver_tap*2) + 2;
				}

				if (core_param->output_pixfmt == YUV420 && i == rows - 1)
					output_pixel_align = 2;

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

			r_out->regions[index].sx = wndInTemp.s_col;
			r_out->regions[index].ex = wndInTemp.e_col;
			r_out->regions[index].sy = wndInTemp.s_row;
			r_out->regions[index].ey = wndInTemp.e_row;

			scaler_param_ptr->phase[index].init_phase_hor = phaseTemp.init_phase_hor;
			scaler_param_ptr->phase[index].init_phase_ver = phaseTemp.init_phase_ver;

			scaler_param_ptr->region_input[index].sx = wndInTemp.s_col;
			scaler_param_ptr->region_input[index].ex = wndInTemp.e_col;
			scaler_param_ptr->region_input[index].sy = wndInTemp.s_row;
			scaler_param_ptr->region_input[index].ey = wndInTemp.e_row;

			scaler_param_ptr->region_output[index].sx = wndOutTemp.s_col;
			scaler_param_ptr->region_output[index].ex = wndOutTemp.e_col;
			scaler_param_ptr->region_output[index].sy = wndOutTemp.s_row;
			scaler_param_ptr->region_output[index].ey = wndOutTemp.e_row;

			pr_debug("rows %d, cols %d, region_input(%d, %d, %d, %d), region_output(%d, %d, %d, %d)\n",
				i, j, wndInTemp.s_row, wndInTemp.s_col, wndInTemp.e_row, wndInTemp.e_col,
				wndOutTemp.s_row, wndOutTemp.s_col, wndOutTemp.e_row, wndOutTemp.e_col);
		}
		prev_row_end = wndOutTemp.e_row + 1;
	}

	r_out->rows = rows;
	r_out->cols = cols;
}

static int isp_drv_regions_empty(const struct isp_drv_regions_t *r)
{
	if(0 == r->rows * r->cols)
		return 1;
	return 0;
}

static void isp_drv_regions_max(const struct isp_drv_regions_t *r1, const struct isp_drv_regions_t *r2,
		struct isp_drv_regions_t *r_out)
{
	int i, j, index;
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

static void isp_drv_regions_arr_max(struct isp_drv_regions_t* arr[],int num,struct isp_drv_regions_t *r_out)
{
	int i;

	for (i = 0; i< num; i++) {
		if (!isp_drv_regions_empty(arr[i])){
			isp_drv_regions_set(arr[i],r_out);
			break;
		}
	}

	for (i = 0; i< num; i++) {
		if (isp_drv_regions_empty(arr[i]))
			continue;
		isp_drv_regions_max(r_out,arr[i],r_out);
	}
}

static void isp_drv_region_w_align(struct isp_drv_region_t *r, int align_v, int min_v, int max_v)
{
	int slice_w_org,slice_w_dst,offset_v,sx,ex;

	if (align_v <= 0)
		return;

	slice_w_org = r->ex - r->sx + 1;
	if (slice_w_org % align_v != 0) {
		slice_w_dst = (slice_w_org + (align_v - 1)) / align_v * align_v;
		offset_v = slice_w_dst - slice_w_org;

		sx = r->sx - offset_v;
		if(sx >= min_v) {
			r->sx = sx;
		} else {
			ex = r->ex + offset_v;
			if (ex <= max_v) {
				r->ex = ex;
			}
		}
	}
}

static void isp_drv_regions_w_align(struct isp_drv_regions_t *r, int align_v, int min_v, int max_v)
{
	int i, j, index;
	int rows = r->rows;
	int cols = r->cols;
	for (i = 0; i < rows; i++) {
		for(j = 0; j < cols; j++) {
			index = i * cols + j;
			isp_drv_region_w_align(&r->regions[index], align_v, min_v,max_v);
		}
	}
}

void slice_drv_calculate_overlap_init(struct slice_drv_overlap_param_t *param_ptr)
{
	memset(param_ptr, 0, sizeof(struct slice_drv_overlap_param_t));

	param_ptr->scaler1.frameParam = &param_ptr->scaler1.frameParamObj;
	param_ptr->scaler2.frameParam = &param_ptr->scaler2.frameParamObj;
}

static void scaler_slice_init(
	const struct isp_drv_region_t *input_slice_region,
	struct slice_drv_scaler_slice_init_context *context,
	struct slice_drv_overlap_scaler_param *scaler_param_ptr)
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

int trim0_resize(uint8_t deci_factor, uint16_t trim_start, uint16_t trim_size, uint8_t trim0_align, uint16_t src_size)
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

	trim_size_new = trim_size;

	return trim_size_new;
}

void ltm_rgb_stat_param_init(uint16_t frame_width, uint16_t frame_height,
		struct ltm_rgb_stat_param_t *param_stat)
{
	uint8_t min_tile_num, binning_factor,max_tile_col,min_tile_row, tile_num_x, tile_num_y;
	uint8_t cropRows, cropCols, cropUp, cropDown, cropLeft, cropRight;
	uint16_t min_tile_width, max_tile_height, tile_width, tile_height;
	uint16_t clipLimit_min, clipLimit;
	uint8_t pow_factor = 0;
	uint16_t ceil_temp;
	uint8_t strength = param_stat->strength;
	uint8_t tile_num = param_stat->tile_num_col * param_stat->tile_num_row;
	uint16_t tile_size_temp = (1<<16) -1;
	uint32_t tile_size, frame_size;
	uint8_t calculate_times = 0;

	frame_size = frame_width * frame_height;
	min_tile_num = (frame_size + (TM_TILE_MAX_SIZE -1)) / TM_TILE_MAX_SIZE;

	if (min_tile_num <= tile_num) {
		binning_factor = 0;
		pow_factor = 1;
	} else if (min_tile_num <= tile_num *4) {
		binning_factor = 1;
		pow_factor = 2;
	} else {
		pr_debug("warning!frame w %d, h %d, size %d, min_tile_num %d, tile_num %d\n",
			frame_width, frame_height, frame_size, min_tile_num, tile_num);
	}

	frame_width = (uint16_t)(frame_width / (2 * pow_factor) *2);
	frame_height = (uint16_t)(frame_height / (2 * pow_factor) *2);

	if (param_stat->tile_num_auto) {
		max_tile_col = MAX(MIN(frame_width / (TM_TILE_WIDTH_MIN * 2) * 2, TM_TILE_NUM_MAX), TM_TILE_NUM_MIN);
		min_tile_width = frame_width / (max_tile_col * 4) * 4;
		max_tile_height = TM_TILE_MAX_SIZE / (min_tile_width * 2) * 2;
		ceil_temp = (frame_height + (max_tile_height -1)) / max_tile_height;
		min_tile_row = (uint8_t) MAX(MIN(ceil_temp, TM_TILE_NUM_MAX), TM_TILE_NUM_MIN);

		tile_num_y = (min_tile_row / 2) * 2;
		tile_num_x = MIN(MAX(((tile_num_y * frame_width / frame_height) / 2) * 2, TM_TILE_NUM_MIN), max_tile_col);

		tile_width = frame_width / (4 * tile_num_x) * 4;
		tile_height = frame_height / (2 * tile_num_y) * 2;
		while (tile_width*tile_height >= TM_TILE_MAX_SIZE) {
			tile_num_y = MIN(MAX(tile_num_y + 2, TM_TILE_NUM_MIN), TM_TILE_NUM_MAX);
			tile_num_x = MIN(MAX(((tile_num_y * frame_width / frame_height)/2)*2, TM_TILE_NUM_MIN), max_tile_col);
			tile_width = frame_width / (4 * tile_num_x) * 4;
			tile_height = frame_height / (2 * tile_num_y) * 2;
			calculate_times++;
			if (calculate_times > 2) {
				tile_num_y = 8;
				tile_num_x = 8;
				tile_width = frame_width / (4 * tile_num_x) * 4;
				tile_height = frame_height / (2 * tile_num_y) * 2;
				break;
			}
		}
	} else {
		tile_num_x = param_stat->tile_num_col;
		tile_num_y = param_stat->tile_num_row;
		tile_width = frame_width / (4 * tile_num_x) * 4;
		tile_height = frame_height / (2 * tile_num_y) * 2;
	}

	cropRows = frame_height - tile_height * tile_num_y;
	cropCols = frame_width - tile_width * tile_num_x;
	cropUp = cropRows >> 1;
	cropDown = cropRows >> 1;
	cropLeft = cropCols >> 1;
	cropRight = cropCols >> 1;

	tile_size= tile_width * tile_height;
	CLIP(tile_size, tile_size_temp, 0);

	clipLimit_min = tile_size >> TM_BIN_NUM_BIT;
	clipLimit = clipLimit_min + ((clipLimit_min * strength)>>3);

	/* update parameters */
	param_stat->cropUp_stat = cropUp;
	param_stat->cropDown_stat = cropDown;
	param_stat->cropLeft_stat = cropLeft;
	param_stat->cropRight_stat = cropRight;
	param_stat->tile_width_stat = tile_width;
	param_stat->tile_height_stat = tile_height;
	param_stat->frame_width_stat = frame_width;
	param_stat->frame_height_stat = frame_height;
	param_stat->clipLimit = clipLimit;
	param_stat->clipLimit_min = clipLimit_min;
	param_stat->tile_num_row = tile_num_y;
	param_stat->tile_num_col = tile_num_x;
	param_stat->binning_en = binning_factor;
	param_stat->tile_size_stat = tile_width * tile_height;
}

void yuv_scaler_init_frame_info(struct yuvscaler_param_t *pYuvScaler)
{
	struct scaler_info_t *pScalerInfo = NULL;
	uint16_t new_width = pYuvScaler->src_size_x;
	uint16_t new_height = pYuvScaler->src_size_y;
	int adj_hor = 1;
	int adj_ver = 1;
	uint8_t trim0_align;

	if (!pYuvScaler->bypass) {
		/* init deci info */
		if (pYuvScaler->deci_info.deci_x_en == 0) {
			pYuvScaler->deci_info.deci_x = 1;
			pYuvScaler->deci_info.deciPhase_X = 0;
		}
		if(pYuvScaler->deci_info.deci_y_en == 0) {
			pYuvScaler->deci_info.deci_y = 1;
			pYuvScaler->deci_info.deciPhase_Y = 0;
		}

		/* trim0 align */
		/* FW/DRV code need to sync */
		trim0_align = 2;/* algorithm requirement, scaler needs 2 aligned input size */

		pYuvScaler->trim0_info.trim_size_x =
			trim0_resize(pYuvScaler->deci_info.deci_x, pYuvScaler->trim0_info.trim_start_x, pYuvScaler->trim0_info.trim_size_x, trim0_align, pYuvScaler->src_size_x);
		pYuvScaler->trim0_info.trim_size_y =
			trim0_resize(pYuvScaler->deci_info.deci_y, pYuvScaler->trim0_info.trim_start_y, pYuvScaler->trim0_info.trim_size_y, trim0_align, pYuvScaler->src_size_y);

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
		pScalerInfo->scaler_in_height = new_height;

		if (pScalerInfo->scaler_en) {
			int32_t  scl_init_phase_hor, scl_init_phase_ver;
			uint16_t scl_factor_in_hor, scl_factor_out_hor;
			uint16_t scl_factor_in_ver, scl_factor_out_ver;
			uint16_t i_w, o_w, i_h, o_h;

			i_w = pScalerInfo->scaler_in_width;
			o_w = pScalerInfo->scaler_out_width;
			i_h = pScalerInfo->scaler_in_height;
			o_h = pScalerInfo->scaler_out_height;

			scl_factor_in_hor = (uint16_t)(i_w*adj_hor);
			scl_factor_out_hor = (uint16_t)(o_w*adj_hor);
			scl_factor_in_ver  = (uint16_t)(i_h*adj_ver);
			scl_factor_out_ver = (uint16_t)(o_h*adj_ver);

			pScalerInfo->scaler_factor_in_hor = scl_factor_in_hor;
			pScalerInfo->scaler_factor_out_hor = scl_factor_out_hor;
			pScalerInfo->scaler_factor_in_ver = scl_factor_in_ver;
			pScalerInfo->scaler_factor_out_ver = scl_factor_out_ver;

			scl_init_phase_hor = pScalerInfo->scaler_init_phase_hor;
			scl_init_phase_ver = pScalerInfo->scaler_init_phase_ver;
			pScalerInfo->init_phase_info.scaler_init_phase[0] = scl_init_phase_hor;
			pScalerInfo->init_phase_info.scaler_init_phase[1] = scl_init_phase_ver;

			/* hor */
			calc_scaler_phase(scl_init_phase_hor, scl_factor_out_hor, &pScalerInfo->init_phase_info.scaler_init_phase_int[0][0],
				&pScalerInfo->init_phase_info.scaler_init_phase_rmd[0][0]);/* luma */
			calc_scaler_phase(scl_init_phase_hor / 4, scl_factor_out_hor / 2, &pScalerInfo->init_phase_info.scaler_init_phase_int[0][1],
				&pScalerInfo->init_phase_info.scaler_init_phase_rmd[0][1]);/*chroma*/

			/* ver*/
			/* luma */
			calc_scaler_phase(scl_init_phase_ver, scl_factor_out_ver, &pScalerInfo->init_phase_info.scaler_init_phase_int[1][0],
				&pScalerInfo->init_phase_info.scaler_init_phase_rmd[1][0]);
			/* FIXME: need refer to input_pixfmt */
			/* chroma */
			if (pYuvScaler->input_pixfmt == YUV422) {
				if (pYuvScaler->output_pixfmt == YUV422)
					calc_scaler_phase(scl_init_phase_ver, scl_factor_out_ver, &pScalerInfo->init_phase_info.scaler_init_phase_int[1][1],
						&pScalerInfo->init_phase_info.scaler_init_phase_rmd[1][1]);
				else if (pYuvScaler->output_pixfmt == YUV420)
					calc_scaler_phase(scl_init_phase_ver / 2, scl_factor_out_ver / 2, &pScalerInfo->init_phase_info.scaler_init_phase_int[1][1],
						&pScalerInfo->init_phase_info.scaler_init_phase_rmd[1][1]);
			} else if (pYuvScaler->input_pixfmt == YUV420) {
				if (pYuvScaler->output_pixfmt == YUV422)
					calc_scaler_phase(scl_init_phase_ver / 2,  scl_factor_out_ver, &pScalerInfo->init_phase_info.scaler_init_phase_int[1][1],
						&pScalerInfo->init_phase_info.scaler_init_phase_rmd[1][1]);
				else if (pYuvScaler->output_pixfmt == YUV420)
					calc_scaler_phase(scl_init_phase_ver / 4,  scl_factor_out_ver / 2, &pScalerInfo->init_phase_info.scaler_init_phase_int[1][1],
						&pScalerInfo->init_phase_info.scaler_init_phase_rmd[1][1]);
			}

			{
				uint32_t *tmp_buf = NULL;
				uint32_t *h_coeff = NULL;
				uint32_t *h_chroma_coeff = NULL;
				uint32_t *v_coeff = NULL;
				uint32_t *v_chroma_coeff = NULL;
				uint8_t y_hor_tap = 0;
				uint8_t uv_hor_tap = 0;
				uint8_t y_ver_tap = 0;
				uint8_t uv_ver_tap = 0;

				if (scaler_path == SCALER_CAP_PRE)
					tmp_buf = scaler1_coeff_buf;
				else if (scaler_path == SCALER_VID)
					tmp_buf = scaler2_coeff_buf;
				else
					pr_err("fail to get scaler path %d", scaler_path);
				h_coeff = tmp_buf;
				h_chroma_coeff = tmp_buf + (ISP_SC_COEFF_COEF_SIZE / 4);
				v_coeff = tmp_buf + (ISP_SC_COEFF_COEF_SIZE * 2 / 4);
				v_chroma_coeff = tmp_buf + (ISP_SC_COEFF_COEF_SIZE * 3 / 4);

				cam_scaler_isp_scale_coeff_gen_ex((short)scl_factor_in_hor,
					(short)scl_factor_in_ver,
					(short)scl_factor_out_hor,
					(short)scl_factor_out_ver,
					1,/* in format: 00:YUV422; 1:YUV420; */
					1,/* out format: 00:YUV422; 1:YUV420; */
					h_coeff,
					h_chroma_coeff,
					v_coeff,
					v_chroma_coeff,
					&y_hor_tap,
					&uv_hor_tap,
					&y_ver_tap,
					&uv_ver_tap,
					tmp_buf + (ISP_SC_COEFF_COEF_SIZE),
					ISP_SC_COEFF_TMP_SIZE);

				pScalerInfo->scaler_y_hor_tap = y_hor_tap;
				pScalerInfo->scaler_uv_hor_tap = uv_hor_tap;
				pScalerInfo->scaler_y_ver_tap = y_ver_tap;
				pScalerInfo->scaler_uv_ver_tap = uv_ver_tap;
				pr_debug("tap %d %d %d %d\n", y_hor_tap, uv_hor_tap, y_ver_tap, uv_ver_tap);
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

			pScalerInfo->scaler_factor_in_hor = pScalerInfo->scaler_in_width;
			pScalerInfo->scaler_factor_out_hor = pScalerInfo->scaler_out_width;
			pScalerInfo->scaler_factor_in_ver = pScalerInfo->scaler_in_height;
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

static void scaler_init(struct yuvscaler_param_t *core_param, struct slice_drv_overlap_scaler_param *in_param_ptr,
		struct pipe_overlap_context *context)
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
		if(in_param_ptr->FBC_enable) {
			core_param->output_align_hor = FBC_PADDING_W_YUV420_scaler;
			core_param->output_align_ver = FBC_PADDING_H_YUV420_scaler;
		}
	}

	if (context->pixelFormat == 3)/* PIX_FMT_YUV422 */
		core_param->input_pixfmt = YUV422;
	else if(context->pixelFormat == 4)/*PIX_FMT_YUV420*/
		core_param->input_pixfmt = YUV420;

	core_param->src_size_x = context->frameWidth;
	core_param->src_size_y = context->frameHeight;
	yuv_scaler_init_frame_info(core_param);
	core_param->scaler_info.input_pixfmt = core_param->input_pixfmt;
	core_param->scaler_info.output_pixfmt = core_param->output_pixfmt;
}

void slice_drv_calculate_overlap(struct slice_drv_overlap_param_t *param_ptr)
{
	int image_w = param_ptr->img_w;
	int image_h = param_ptr->img_h;

	struct pipe_overlap_context contextScaler;
	struct yuvscaler_param_t *scaler1_frame_p = (struct yuvscaler_param_t*)(param_ptr->scaler1.frameParam);
	struct yuvscaler_param_t *scaler2_frame_p = (struct yuvscaler_param_t*)(param_ptr->scaler2.frameParam);

	struct slice_drv_overlap_info ov_pipe;
	struct slice_drv_overlap_info ov_Y = {0};
	struct slice_drv_overlap_info ov_UV = {0};

	struct isp_block_drv_t nlm_param;
	struct isp_block_drv_t imbalance_param;
	struct isp_block_drv_t raw_gtm_stat_param;
	struct isp_block_drv_t raw_gtm_map_param;
	struct isp_block_drv_t cfa_param;

	struct isp_block_drv_t ltmsta_rgb_param;

	struct isp_block_drv_t nr3d_param;
	struct isp_block_drv_t pre_cnr_param;
	struct isp_block_drv_t ynr_param;
	struct isp_block_drv_t ee_param;
	struct isp_block_drv_t cnr_new_param;
	struct isp_block_drv_t post_cnr_param;
	struct isp_block_drv_t iir_cnr_param;
	struct isp_block_drv_t yuv420to422_param;

	struct isp_drv_regions_t orgRegion;
	struct isp_drv_regions_t maxRegion;
	struct isp_drv_region_fetch_t slice_in;
	struct isp_drv_regions_t slice_out;
	struct isp_drv_regions_t final_slice_regions;

#if !(FBC_SUPPORT)
	param_ptr->nr3d_bd_FBC_en = 0;
	param_ptr->scaler1.FBC_enable = 0;
	param_ptr->scaler2.FBC_enable = 0;
#endif

	if (param_ptr->crop_en && 0 == param_ptr->crop_mode) {
		image_w = param_ptr->crop_w;
		image_h = param_ptr->crop_h;
	}
	ov_pipe.ov_left = 0;
	ov_pipe.ov_right = 0;
	ov_pipe.ov_up = 0;
	ov_pipe.ov_down = 0;

	/* bayer input */
	if( 0 == param_ptr->img_type || 6 == param_ptr->img_type) {
		core_drv_nlm_init_block(&nlm_param);
		CAL_OVERLAP(param_ptr->nlm_bypass, nlm_param);

		core_drv_imbalance_init_block(&imbalance_param);
		CAL_OVERLAP(param_ptr->imbalance_bypass, imbalance_param);

		core_drv_raw_gtm_stat_init_block(&raw_gtm_stat_param);
		CAL_OVERLAP(param_ptr->raw_gtm_stat_bypass, raw_gtm_stat_param);

		core_drv_raw_gtm_map_init_block(&raw_gtm_map_param);
		CAL_OVERLAP(param_ptr->raw_gtm_map_bypass, raw_gtm_map_param);

		core_drv_cfa_init_block(&cfa_param);
		CAL_OVERLAP(param_ptr->cfa_bypass, cfa_param);
	}

	/* rgb */
	core_drv_ltmsta_init_block(&ltmsta_rgb_param);
	CAL_OVERLAP(param_ptr->ltmsta_rgb_bypass, ltmsta_rgb_param);

	/* yuv */
	/* yuv420 */
	if (4 == param_ptr->img_type) {
		if (!param_ptr->yuv420to422_bypass) {
			core_drv_yuv420to422_init_block(&yuv420to422_param);
			CAL_OVERLAP(param_ptr->yuv420to422_bypass, yuv420to422_param);
		}
	}

	core_drv_nr3d_init_block(&nr3d_param);
	CAL_OVERLAP(param_ptr->nr3d_bd_bypass, nr3d_param);

	core_drv_pre_cnr_init_block(&pre_cnr_param);
	if (0 == param_ptr->pre_cnr_bypass) {
		ov_pipe.ov_left += (pre_cnr_param.left << 1);
		ov_pipe.ov_right += (pre_cnr_param.right << 1);
		ov_pipe.ov_up += pre_cnr_param.up;
		ov_pipe.ov_down += pre_cnr_param.down;
	}

	/* Y domain */
	{
		core_drv_ynr_init_block(&ynr_param);
		if (0 == param_ptr->ynr_bypass) {
			ov_Y.ov_left += ynr_param.left;
			ov_Y.ov_right += ynr_param.right;
			ov_Y.ov_up += ynr_param.up;
			ov_Y.ov_down += ynr_param.down;
		}

		core_drv_ee_init_block(&ee_param);
		if (0 == param_ptr->ee_bypass) {
			ov_Y.ov_left += ee_param.left;
			ov_Y.ov_right += ee_param.right;
			ov_Y.ov_up += ee_param.up;
			ov_Y.ov_down += ee_param.down;
		}
	}

	/* UV domain */
	{
		core_drv_cnrnew_init_block(&cnr_new_param);
		if (0 == param_ptr->cnr_new_bypass) {
			ov_UV.ov_left += (cnr_new_param.left << 1);
			ov_UV.ov_right += (cnr_new_param.right << 1);
			ov_UV.ov_up += cnr_new_param.up;
			ov_UV.ov_down += cnr_new_param.down;
		}

		core_drv_post_cnr_init_block(&post_cnr_param);
		if (0 == param_ptr->post_cnr_bypass) {
			ov_UV.ov_left += (post_cnr_param.left << 1);
			ov_UV.ov_right += (post_cnr_param.right << 1);
			ov_UV.ov_up += post_cnr_param.up;
			ov_UV.ov_down += post_cnr_param.down;
		}
	}

	/* YUV4 domain */
	core_drv_iir_cnr_init_block(&iir_cnr_param);
	if (0 == param_ptr->iir_cnr_bypass) {
		ov_Y.ov_left += (iir_cnr_param.left << 1);
		ov_Y.ov_right += (iir_cnr_param.right << 1);
		ov_Y.ov_up += iir_cnr_param.up;
		ov_Y.ov_down += iir_cnr_param.down;
	}

	/* Y and UV MAX */
	{
		ov_pipe.ov_left += (ov_Y.ov_left > ov_UV.ov_left) ? ov_Y.ov_left : ov_UV.ov_left;
		ov_pipe.ov_right += (ov_Y.ov_right > ov_UV.ov_right) ? ov_Y.ov_right : ov_UV.ov_right;
		ov_pipe.ov_up += (ov_Y.ov_up > ov_UV.ov_up) ? ov_Y.ov_up : ov_UV.ov_up;
		ov_pipe.ov_down += (ov_Y.ov_down > ov_UV.ov_down) ? ov_Y.ov_down : ov_UV.ov_down;
	}

	/* set user overlap */
	if (param_ptr->offlineCfgOverlap_en) {
		ov_pipe.ov_left = param_ptr->offlineCfgOverlap_left;
		ov_pipe.ov_right = param_ptr->offlineCfgOverlap_right;
		ov_pipe.ov_up = param_ptr->offlineCfgOverlap_up;
		ov_pipe.ov_down = param_ptr->offlineCfgOverlap_down;
	}

	/* overlap 2 align */
	ov_pipe.ov_left = (ov_pipe.ov_left + 1)>>1<<1;
	ov_pipe.ov_right = (ov_pipe.ov_right + 1)>>1<<1;
	ov_pipe.ov_up = (ov_pipe.ov_up + 1)>>1<<1;
	ov_pipe.ov_down = (ov_pipe.ov_down + 1)>>1<<1;
	pr_debug("submod overlap(left %d, right %d, up %d, down %d), img_w %d, img_h %d, \n",
		ov_pipe.ov_left, ov_pipe.ov_right, ov_pipe.ov_up,
		ov_pipe.ov_down, param_ptr->img_w, param_ptr->img_h);

	/* set scaler context */
	contextScaler.frameWidth = image_w;
	contextScaler.frameHeight = image_h;
	contextScaler.pixelFormat = param_ptr->scaler_input_format;

	slice_in.image_w = image_w;
	slice_in.image_h = image_h;
	slice_in.slice_w = param_ptr->slice_w;
	slice_in.slice_h = param_ptr->slice_h;
	slice_in.overlap_left = 0;
	slice_in.overlap_right = 0;
	slice_in.overlap_up = 0;
	slice_in.overlap_down = 0;

	pr_debug("slice_in:  w %d, h %d,  image_w %d, image_h %d\n",
	slice_in.slice_w, slice_in.slice_h, slice_in.image_w, slice_in.image_h);
	if (-1 == isp_drv_regions_fetch(&slice_in, &slice_out))
		return;

	isp_drv_regions_set(&slice_out, &maxRegion);
	isp_drv_regions_set(&slice_out, &orgRegion);
	pr_debug("slice_out: rows %d , cols %d, slice0 regions %d, %d, %d, %d; slice1 regions %d, %d, %d, %d\n",
		slice_out.rows, slice_out.cols,
		slice_out.regions[0].sx, slice_out.regions[0].sy, slice_out.regions[0].ex, slice_out.regions[0].ey,
		slice_out.regions[1].sx, slice_out.regions[1].sy, slice_out.regions[1].ex, slice_out.regions[1].ey);
	pr_debug("maxRegion: rows %d , cols %d, slice0 regions %d, %d, %d, %d; slice1 regions %d, %d, %d, %d\n",
		maxRegion.rows, maxRegion.cols,
		maxRegion.regions[0].sx, maxRegion.regions[0].sy, maxRegion.regions[0].ex, maxRegion.regions[0].ey,
		maxRegion.regions[1].sx, maxRegion.regions[1].sy, maxRegion.regions[1].ex, maxRegion.regions[1].ey);
	pr_debug("orgRegion: rows %d , cols %d, slice0 regions %d, %d, %d, %d; slice1 regions %d, %d, %d, %d\n",
		orgRegion.rows, orgRegion.cols,
		orgRegion.regions[0].sx, orgRegion.regions[0].sy, orgRegion.regions[0].ex, orgRegion.regions[0].ey,
		orgRegion.regions[1].sx, orgRegion.regions[1].sy, orgRegion.regions[1].ex, orgRegion.regions[1].ey);

	/* step1 */
	do {
		struct isp_drv_regions_t refRegion;
		struct isp_drv_regions_t nr3d_slice_region_out;
		struct isp_drv_regions_t scaler1_slice_region_out;
		struct isp_drv_regions_t scaler2_slice_region_out;
		struct isp_drv_regions_t scaler3_slice_region_out;
		struct isp_drv_regions_t maxRegionTemp;
		struct isp_drv_regions_t* regions_arr[4];

		memset(&refRegion, 0, sizeof(struct isp_drv_regions_t));
		memset(&nr3d_slice_region_out, 0, sizeof(struct isp_drv_regions_t));
		memset(&scaler1_slice_region_out, 0, sizeof(struct isp_drv_regions_t));
		memset(&scaler2_slice_region_out, 0, sizeof(struct isp_drv_regions_t));
		memset(&scaler3_slice_region_out, 0, sizeof(struct isp_drv_regions_t));
		isp_drv_regions_reset(&maxRegionTemp);

		regions_arr[0] = &nr3d_slice_region_out;
		regions_arr[1] = &scaler1_slice_region_out;
		regions_arr[2] = &scaler2_slice_region_out;
		regions_arr[3] = &scaler3_slice_region_out;

		isp_drv_regions_set(&maxRegion, &refRegion);

		if (!param_ptr->nr3d_bd_bypass && param_ptr->nr3d_bd_FBC_en) {
			isp_drv_regions_set(&orgRegion, &nr3d_slice_region_out);
			isp_drv_regions_3dnr(&refRegion, &nr3d_slice_region_out, FBC_PADDING_W_YUV420_3dnr, FBC_PADDING_H_YUV420_3dnr, 0);
			isp_drv_regions_set(&nr3d_slice_region_out, &orgRegion);
		}

		if (!param_ptr->scaler1.bypass) {
			scaler_path = SCALER_CAP_PRE;
			scaler_init(scaler1_frame_p, &param_ptr->scaler1, &contextScaler);

			pr_debug("scaler_step1: bypass %d, input(pixfmt %d), output (pixfmt %d, align_hor %d, align_ver %d)\n",
				scaler1_frame_p->bypass, scaler1_frame_p->input_pixfmt, scaler1_frame_p->output_pixfmt,
				scaler1_frame_p->output_align_hor, scaler1_frame_p->output_align_ver);
			pr_debug("scaler_step1: src(size_x %d, size_y %d), dst(start_x %d, start_y %d, size_x %d, size_y %d)\n",
				scaler1_frame_p->src_size_x, scaler1_frame_p->src_size_y,
				scaler1_frame_p->dst_start_x, scaler1_frame_p->dst_start_y,
				scaler1_frame_p->dst_size_x, scaler1_frame_p->dst_size_y);
			pr_debug("scaler_step1: trim0_info:  en %d, start_x %d, start_y %d, size_x %d, size_y %d\n",
				scaler1_frame_p->trim0_info.trim_en,
				scaler1_frame_p->trim0_info.trim_start_x, scaler1_frame_p->trim0_info.trim_start_y,
				scaler1_frame_p->trim0_info.trim_size_x, scaler1_frame_p->trim0_info.trim_size_y);
			pr_debug("scaler_step1: deci_info: x_en %d, y_en %d, deci_x %d, deci_y %d, Phase_X %d, Phase_Y %d, cut_first_y %d, option %d\n",
				scaler1_frame_p->deci_info.deci_x_en, scaler1_frame_p->deci_info.deci_y_en,
				scaler1_frame_p->deci_info.deci_x, scaler1_frame_p->deci_info.deci_y,
				scaler1_frame_p->deci_info.deciPhase_X, scaler1_frame_p->deci_info.deciPhase_Y,
				scaler1_frame_p->deci_info.deci_cut_first_y, scaler1_frame_p->deci_info.deci_option);
			pr_debug("scaler_step1: scaler_info: scaler_en %d, in_w %d, in_h %d, out_w %d, out_h %d, input_pixfmt %d, output_pixfmt %d, \n",
				scaler1_frame_p->scaler_info.scaler_en,
				scaler1_frame_p->scaler_info.scaler_in_width, scaler1_frame_p->scaler_info.scaler_in_height,
				scaler1_frame_p->scaler_info.scaler_out_width, scaler1_frame_p->scaler_info.scaler_out_height,
				scaler1_frame_p->scaler_info.input_pixfmt, scaler1_frame_p->scaler_info.output_pixfmt);
			pr_debug("scaler_step1: scaler_factor_info: in_hor %d, in_ver %d, out_hor %d, out_ver %d\n",
				scaler1_frame_p->scaler_info.scaler_factor_in_hor, scaler1_frame_p->scaler_info.scaler_factor_in_ver,
				scaler1_frame_p->scaler_info.scaler_factor_out_hor, scaler1_frame_p->scaler_info.scaler_factor_out_ver);
			pr_debug("scaler_step1: scaler_init_phase: hor %d, ver %d; scaler_tap_info: y_hor_tap %d, y_ver_tap %d, uv_hor_tap %d, uv_ver_tap %d\n",
				scaler1_frame_p->scaler_info.scaler_init_phase_hor, scaler1_frame_p->scaler_info.scaler_init_phase_ver,
				scaler1_frame_p->scaler_info.scaler_y_hor_tap, scaler1_frame_p->scaler_info.scaler_y_ver_tap,
				scaler1_frame_p->scaler_info.scaler_uv_hor_tap, scaler1_frame_p->scaler_info.scaler_uv_ver_tap);
			pr_debug("scaler_step1: refRegion: slice0(%d, %d, %d, %d), slice1(%d, %d, %d, %d), rows %d, cols %d\n",
				refRegion.regions[0].sx, refRegion.regions[0].sy, refRegion.regions[0].ex, refRegion.regions[0].ey,
				refRegion.regions[1].sx, refRegion.regions[1].sy, refRegion.regions[1].ex, refRegion.regions[1].ey,
				refRegion.rows, refRegion.cols);

			scaler_calculate_region(&refRegion, &scaler1_slice_region_out, scaler1_frame_p, 0, &param_ptr->scaler1);

			pr_debug("scaler_step1: slice_region_out:  slice0(%d, %d, %d, %d), slice1(%d, %d, %d, %d),  rows %d , cols %d\n",
				scaler1_slice_region_out.regions[0].sx, scaler1_slice_region_out.regions[0].sy,
				scaler1_slice_region_out.regions[0].ex, scaler1_slice_region_out.regions[0].ey,
				scaler1_slice_region_out.regions[1].sx, scaler1_slice_region_out.regions[1].sy,
				scaler1_slice_region_out.regions[1].ex, scaler1_slice_region_out.regions[1].ey,
				scaler1_slice_region_out.rows, scaler1_slice_region_out.cols);
		}

		if (!param_ptr->scaler2.bypass) {
			scaler_path = SCALER_VID;
			scaler_init(scaler2_frame_p, &param_ptr->scaler2, &contextScaler);
			scaler_calculate_region(&refRegion, &scaler2_slice_region_out, scaler2_frame_p, 0, &param_ptr->scaler2);
		}

		/* get max */
		isp_drv_regions_arr_max(regions_arr, 4, &maxRegionTemp);
		if (!isp_drv_regions_empty(&maxRegionTemp)) {
			isp_drv_regions_set(&maxRegionTemp, &maxRegion);

			pr_debug("step1: maxRegion slice0(%d, %d, %d, %d), slice1(%d, %d, %d, %d), rows %d , cols%d, \n",
				maxRegion.regions[0].sx, maxRegion.regions[0].sy, maxRegion.regions[0].ex, maxRegion.regions[0].ey,
				maxRegion.regions[1].sx, maxRegion.regions[1].sy, maxRegion.regions[1].ex, maxRegion.regions[1].ey,
				maxRegion.rows, maxRegion.cols);
		}

	} while (0);

	/*step2*/
	do {
		struct isp_drv_regions_t refRegion;
		struct isp_drv_regions_t nr3d_slice_region_out;
		struct isp_drv_regions_t scaler1_slice_region_out;
		struct isp_drv_regions_t scaler2_slice_region_out;
		struct isp_drv_regions_t scaler3_slice_region_out;
		struct isp_drv_regions_t maxRegionTemp;
		struct isp_drv_regions_t* regions_arr[4];

		memset(&refRegion, 0, sizeof(struct isp_drv_regions_t));
		memset(&nr3d_slice_region_out, 0, sizeof(struct isp_drv_regions_t));
		memset(&scaler1_slice_region_out, 0, sizeof(struct isp_drv_regions_t));
		memset(&scaler2_slice_region_out, 0, sizeof(struct isp_drv_regions_t));
		memset(&scaler3_slice_region_out, 0, sizeof(struct isp_drv_regions_t));
		isp_drv_regions_reset(&maxRegionTemp);

		regions_arr[0] = &nr3d_slice_region_out;
		regions_arr[1] = &scaler1_slice_region_out;
		regions_arr[2] = &scaler2_slice_region_out;
		regions_arr[3] = &scaler3_slice_region_out;

		isp_drv_regions_set(&maxRegion, &refRegion);

		if (!param_ptr->nr3d_bd_bypass && param_ptr->nr3d_bd_FBC_en) {
			isp_drv_regions_set(&orgRegion, &nr3d_slice_region_out);
			isp_drv_regions_3dnr(&refRegion, &nr3d_slice_region_out, FBC_PADDING_W_YUV420_3dnr,FBC_PADDING_H_YUV420_3dnr, 1);
			isp_drv_regions_set(&nr3d_slice_region_out, &orgRegion);
		}

		if (!param_ptr->scaler1.bypass) {
			pr_debug("scaler_step2: refRegion: slice0(%d, %d, %d, %d), slice1(%d, %d, %d, %d), rows %d, cols %d\n",
				refRegion.regions[0].sx, refRegion.regions[0].sy, refRegion.regions[0].ex, refRegion.regions[0].ey,
				refRegion.regions[1].sx, refRegion.regions[1].sy, refRegion.regions[1].ex, refRegion.regions[1].ey,
				refRegion.rows, refRegion.cols);

			scaler_calculate_region(&refRegion, &scaler1_slice_region_out, scaler1_frame_p, 1, &param_ptr->scaler1);

			pr_debug("scaler_step2: slice_region_out: slice0(%d, %d, %d, %d), slice1(%d, %d, %d, %d),  rows %d , cols %d\n",
				scaler1_slice_region_out.regions[0].sx, scaler1_slice_region_out.regions[0].sy,
				scaler1_slice_region_out.regions[0].ex, scaler1_slice_region_out.regions[0].ey,
				scaler1_slice_region_out.regions[1].sx, scaler1_slice_region_out.regions[1].sy,
				scaler1_slice_region_out.regions[1].ex, scaler1_slice_region_out.regions[1].ey,
				scaler1_slice_region_out.rows, scaler1_slice_region_out.cols);

		}

		if (!param_ptr->scaler2.bypass)
			scaler_calculate_region(&refRegion, &scaler2_slice_region_out, scaler2_frame_p, 1, &param_ptr->scaler2);

		/* get max */
		isp_drv_regions_arr_max(regions_arr, 4, &maxRegionTemp);
		if (!isp_drv_regions_empty(&maxRegionTemp)) {
			isp_drv_regions_set(&maxRegionTemp, &maxRegion);

			pr_debug("scaler_step2: maxRegion: slice0(%d, %d, %d, %d), slice1(%d, %d, %d, %d), rows %d , cols%d, \n",
				maxRegion.regions[0].sx, maxRegion.regions[0].sy, maxRegion.regions[0].ex, maxRegion.regions[0].ey,
				maxRegion.regions[1].sx, maxRegion.regions[1].sy, maxRegion.regions[1].ex, maxRegion.regions[1].ey,
				maxRegion.rows, maxRegion.cols);
		}

	} while (0);

	/* get max */
	isp_drv_regions_max(&maxRegion, &orgRegion, &maxRegion);

	/*slice cfg*/
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

	do {
		struct pipe_overlap_info ov_temp;
		if (param_ptr->crop_en && 1 == param_ptr->crop_mode) {
			orgRegion.cols = 1;
			orgRegion.rows = 1;;
			final_slice_regions.rows = 1;
			final_slice_regions.cols = 1;

			orgRegion.regions[0].sx = param_ptr->crop_sx;
			orgRegion.regions[0].sy = param_ptr->crop_sy;
			orgRegion.regions[0].ex = param_ptr->crop_sx + param_ptr->crop_w - 1;
			orgRegion.regions[0].ey = param_ptr->crop_sy + param_ptr->crop_h - 1;

			ov_temp.ov_left = ov_pipe.ov_left;
			ov_temp.ov_right = ov_pipe.ov_right;
			ov_temp.ov_down = ov_pipe.ov_down;
			ov_temp.ov_up = ov_pipe.ov_up;

			if (param_ptr->crop_sx == 0)
				ov_temp.ov_left = 0;
			if ((param_ptr->crop_sx + param_ptr->crop_w) == param_ptr->img_w)
				ov_temp.ov_right = 0;
			if (param_ptr->crop_sy == 0)
				ov_temp.ov_up = 0;
			if ((param_ptr->crop_sy + param_ptr->crop_h) == param_ptr->img_h)
				ov_temp.ov_down = 0;

			final_slice_regions.regions[0].sx = orgRegion.regions[0].sx - ov_temp.ov_left;
			final_slice_regions.regions[0].ex = orgRegion.regions[0].ex + ov_temp.ov_right;
			final_slice_regions.regions[0].sy = orgRegion.regions[0].sy - ov_temp.ov_up;
			final_slice_regions.regions[0].ey = orgRegion.regions[0].ey + ov_temp.ov_down;
		}
	} while (0);

	if (!param_ptr->ltmsta_rgb_bypass && param_ptr->ltmsta_rgb_binning_en) {
		const int SLICE_W_ALIGN_V = 4;
		isp_drv_regions_w_align(&final_slice_regions, SLICE_W_ALIGN_V, 0, image_w);
	}

	param_ptr->slice_rows = final_slice_regions.rows;
	param_ptr->slice_cols = final_slice_regions.cols;
	do {
		int i, j, index, rows, cols;
		rows = param_ptr->slice_rows;
		cols = param_ptr->slice_cols;
		pr_debug("rows  %d, cols %d\n", rows, cols);
		for ( i = 0; i < rows; i++) {
			for ( j = 0; j < cols; j++) {
				index = i * cols + j;

				param_ptr->slice_region[index].sx = final_slice_regions.regions[index].sx;
				param_ptr->slice_region[index].ex = final_slice_regions.regions[index].ex;
				param_ptr->slice_region[index].sy = final_slice_regions.regions[index].sy;
				param_ptr->slice_region[index].ey = final_slice_regions.regions[index].ey;

				param_ptr->slice_overlap[index].ov_left  = orgRegion.regions[index].sx - final_slice_regions.regions[index].sx;
				param_ptr->slice_overlap[index].ov_right = final_slice_regions.regions[index].ex - orgRegion.regions[index].ex;
				param_ptr->slice_overlap[index].ov_up    = orgRegion.regions[index].sy - final_slice_regions.regions[index].sy;
				param_ptr->slice_overlap[index].ov_down  = final_slice_regions.regions[index].ey - orgRegion.regions[index].ey;
				pr_debug("slice_id %d, slice_region(%d, %d, %d, %d), slice_overlap(left %d, right %d, up %d,  down %d)\n",
					index,param_ptr->slice_region[index].sx, param_ptr->slice_region[index].ex,
					param_ptr->slice_region[index].sy, param_ptr->slice_region[index].ey,
					param_ptr->slice_overlap[index].ov_left, param_ptr->slice_overlap[index].ov_right,
					param_ptr->slice_overlap[index].ov_up, param_ptr->slice_overlap[index].ov_down);
			}
		}
	}while(0);

	/* calculation scaler slice param */
	do {
		int i, j, index, rows, cols;
		struct slice_drv_scaler_slice_init_context slice_context;

		rows = param_ptr->slice_rows;
		cols = param_ptr->slice_cols;

		for ( i = 0; i < rows; i++) {
			for ( j = 0; j < cols; j++) {
				index = i * cols + j;

				slice_context.slice_index = index;
				slice_context.rows = rows;
				slice_context.cols = cols;
				slice_context.slice_row_no = i;
				slice_context.slice_col_no = j;
				slice_context.slice_w = final_slice_regions.regions[index].ex - final_slice_regions.regions[index].sx + 1;
				slice_context.slice_h = final_slice_regions.regions[index].ey - final_slice_regions.regions[index].sy + 1;

				if (!param_ptr->scaler1.bypass)
					scaler_slice_init(&final_slice_regions.regions[index], &slice_context, &param_ptr->scaler1);

				if (!param_ptr->scaler2.bypass)
					scaler_slice_init(&final_slice_regions.regions[index], &slice_context, &param_ptr->scaler2);
			}
		}
	}while (0);
}

static int isp_init_param_for_yuvscaler_slice(void *slc_cfg_input, void *slc_ctx,
		struct isp_fw_scaler_slice (*slice_param)[PIPE_MAX_SLICE_NUM])
{
	int i = 0;
	int id = 0;
	uint32_t r = 0;
	uint32_t c = 0;
	uint32_t slice_id = 0;
	struct slice_cfg_input *slc_input = NULL;
	struct isp_slice_context *slice_ctx = NULL;
	//isp_fw_scaler_slice *slice_param = NULL;
	struct isp_fw_slice_pos slice_pos_array[PIPE_MAX_SLICE_NUM] = {0};
	struct isp_fw_slice_overlap slice_overlap_array[PIPE_MAX_SLICE_NUM] = {0};
	//isp_fw_scaler_slice slice_param[2][4] = {0};
	//slc_param = &slice_param[0];

	if (!slc_cfg_input || !slc_ctx) {
		pr_err("fail to get input param\n");
		return -1;
	}

	slc_input = (struct slice_cfg_input *)slc_cfg_input;
	slice_ctx = (struct isp_slice_context *)slc_ctx;
	//slice_param = (struct _scaler_slice *)slc_out_info;

	memset(&slice_ctx->yuvscaler_slice_param, 0, sizeof(struct yuvscaler_param_t));

	for (i = 0; i < slice_ctx->slice_num; i++) {
		slice_pos_array[i].start_row = slice_ctx->slices[i].slice_pos.start_row;
		slice_pos_array[i].start_col = slice_ctx->slices[i].slice_pos.start_col;
		slice_pos_array[i].end_row = slice_ctx->slices[i].slice_pos.end_row;
		slice_pos_array[i].end_col = slice_ctx->slices[i].slice_pos.end_col;
		pr_debug("slice_cnt %d, slice_pos_array: start_row %d,  start_col %d, end_row %d, end_col %d\n",
			i, slice_pos_array[i].start_row, slice_pos_array[i].start_col,
			slice_pos_array[i].end_row, slice_pos_array[i].end_col);

		slice_overlap_array[i].overlap_up = slice_ctx->slices[i].slice_overlap.overlap_up;
		slice_overlap_array[i].overlap_down = slice_ctx->slices[i].slice_overlap.overlap_down;
		slice_overlap_array[i].overlap_left = slice_ctx->slices[i].slice_overlap.overlap_left;
		slice_overlap_array[i].overlap_right = slice_ctx->slices[i].slice_overlap.overlap_right;
		pr_debug("slice_cnt %d, slice_pos_array: overlap_up %d, overlap_down %d, overlap_left %d, overlap_right %d\n",
			i, slice_overlap_array[i].overlap_up, slice_overlap_array[i].overlap_down,
			slice_overlap_array[i].overlap_left, slice_overlap_array[i].overlap_right);
	}

	for (id = 0; id <= SCALER_VID; id++) {
		uint32_t FBC_enable = 0;
		uint32_t yuv422to420_bypass = 0;
		uint32_t config_output_align_hor = 2;
		uint32_t yuvFormat = slc_input->calc_dyn_ov.store[id]->color_fmt;
		uint32_t slice_array_height = slice_ctx->slice_row_num;
		uint32_t slice_array_width = slice_ctx->slice_col_num;
		uint32_t slice_num = slice_ctx->slice_num;

		if (0 == slc_input->calc_dyn_ov.path_scaler[id]->scaler.scaler_bypass) {
			memset(&yuvscaler_param, 0, sizeof(struct yuvscaler_param_t));

			yuvscaler_param.bypass = slc_input->calc_dyn_ov.path_scaler[id]->scaler.scaler_bypass;

			yuvscaler_param.trim0_info.trim_en = 1;
			yuvscaler_param.trim0_info.trim_size_x = slc_input->calc_dyn_ov.path_scaler[id]->in_trim.size_x;
			yuvscaler_param.trim0_info.trim_size_y = slc_input->calc_dyn_ov.path_scaler[id]->in_trim.size_y;
			yuvscaler_param.trim0_info.trim_start_x = slc_input->calc_dyn_ov.path_scaler[id]->in_trim.start_x;
			yuvscaler_param.trim0_info.trim_start_y = slc_input->calc_dyn_ov.path_scaler[id]->in_trim.start_y;

			yuvscaler_param.deci_info.deci_x = YUV_DECI_MAP[slc_input->calc_dyn_ov.path_scaler[id]->deci.deci_x];
			yuvscaler_param.deci_info.deci_x_en = slc_input->calc_dyn_ov.path_scaler[id]->deci.deci_x_eb;
			yuvscaler_param.deci_info.deci_y = YUV_DECI_MAP[slc_input->calc_dyn_ov.path_scaler[id]->deci.deci_y];
			yuvscaler_param.deci_info.deci_y_en = slc_input->calc_dyn_ov.path_scaler[id]->deci.deci_y_eb;

			if (yuvscaler_param.deci_info.deci_x_en)
				yuvscaler_param.scaler_info.scaler_in_width = yuvscaler_param.trim0_info.trim_size_x / yuvscaler_param.deci_info.deci_x;
			else
				yuvscaler_param.scaler_info.scaler_in_width = yuvscaler_param.trim0_info.trim_size_x;
			if (yuvscaler_param.deci_info.deci_y_en)
				yuvscaler_param.scaler_info.scaler_in_height = yuvscaler_param.trim0_info.trim_start_y / yuvscaler_param.deci_info.deci_y;
			else
				yuvscaler_param.scaler_info.scaler_in_height = yuvscaler_param.trim0_info.trim_start_y;

			yuvscaler_param.scaler_info.scaler_out_width = slc_input->calc_dyn_ov.path_scaler[id]->dst.w;
			yuvscaler_param.scaler_info.scaler_out_height = slc_input->calc_dyn_ov.path_scaler[id]->dst.h;
			if ((yuvscaler_param.scaler_info.scaler_in_height == yuvscaler_param.scaler_info.scaler_out_height)
				&& (yuvscaler_param.scaler_info.scaler_in_width == yuvscaler_param.scaler_info.scaler_out_width))
				yuvscaler_param.scaler_info.scaler_en = 0;
			else
				yuvscaler_param.scaler_info.scaler_en = 1;

			yuvscaler_param.src_size_x = slc_input->calc_dyn_ov.path_scaler[id]->src.w;
			yuvscaler_param.src_size_y = slc_input->calc_dyn_ov.path_scaler[id]->src.h;

			if (YUV_OUT_UYVY_422 == yuvFormat) {
				config_output_align_hor = 2;
			} else if ((YUV_OUT_Y_UV_422 == yuvFormat)
					|| (YUV_OUT_Y_VU_422 == yuvFormat)
					|| (YUV_OUT_Y_UV_420 == yuvFormat)
					|| (YUV_OUT_Y_VU_420 == yuvFormat)) {
				config_output_align_hor = 4;
			} else if ((YUV_OUT_Y_U_V_422 == yuvFormat)
					||(YUV_OUT_Y_U_V_420 == yuvFormat)) {
				config_output_align_hor = 8;
			}

			yuvscaler_param.output_pixfmt = YUV420;
			if (yuvscaler_param.output_pixfmt == YUV422) {
				yuvscaler_param.output_align_hor = config_output_align_hor;
				yuvscaler_param.output_align_ver = 2;
			}
			else if(yuvscaler_param.output_pixfmt == YUV420) {
				yuvscaler_param.output_align_hor = config_output_align_hor;
				yuvscaler_param.output_align_ver = 4;
				if (FBC_enable) {
					yuvscaler_param.output_align_hor = 2;/* FBC_PADDING_W_YUV420_scaler */
					yuvscaler_param.output_align_ver = FBC_PADDING_H_YUV420_scaler;
				}
			}

			if (yuv422to420_bypass == 0) {
				yuvscaler_param.input_pixfmt = YUV420;
				yuvscaler_param.scaler_info.input_pixfmt = YUV420;
			} else{
				yuvscaler_param.input_pixfmt = YUV422;
				yuvscaler_param.scaler_info.input_pixfmt = YUV422;
			}
			yuvscaler_param.scaler_info.output_pixfmt = yuvscaler_param.output_pixfmt;
			pr_debug("path %d, scaler frame:  : bypass %d, input_pixfmt %d, output_pixfmt %d, output_align_hor %d, output_align_ver %d\n",
				id, yuvscaler_param.bypass, yuvscaler_param.input_pixfmt, yuvscaler_param.output_pixfmt,
				yuvscaler_param.output_align_hor, yuvscaler_param.output_align_ver);
			pr_debug("path %d, scaler frame:  : src_size_x %d, src_size_y %d, dst_start_x %d, dst_start_y %d, dst_size_x %d, dst_size_y %d\n",
				id, yuvscaler_param.src_size_x, yuvscaler_param.src_size_y,
				yuvscaler_param.dst_start_x, yuvscaler_param.dst_start_y,
				yuvscaler_param.dst_size_x, yuvscaler_param.dst_size_y);
			pr_debug("path %d, scaler frame:  : trim0_info: en %d, start_x %d, start_y %d, size_x %d, size_y %d\n",
				id, yuvscaler_param.trim0_info.trim_en,
				yuvscaler_param.trim0_info.trim_start_x, yuvscaler_param.trim0_info.trim_start_y,
				yuvscaler_param.trim0_info.trim_size_x, yuvscaler_param.trim0_info.trim_size_y);
			pr_debug("path %d, scaler frame:  : deci_info: x_en %d, y_en %d, deci_x %d, deci_y %d, Phase_X %d, Phase_Y %d, cut_first_y %d, option %d\n",
				id, yuvscaler_param.deci_info.deci_x_en, yuvscaler_param.deci_info.deci_y_en,
				yuvscaler_param.deci_info.deci_x, yuvscaler_param.deci_info.deci_y,
				yuvscaler_param.deci_info.deciPhase_X, yuvscaler_param.deci_info.deciPhase_Y,
				yuvscaler_param.deci_info.deci_cut_first_y, yuvscaler_param.deci_info.deci_option);
			pr_debug("path %d, scaler frame:  : scaler_info: en %d, input_pixfmt %d, output_pixfmt %d, in_width %d, in_height %d, out_width %d, out_height %d\n",
				id, yuvscaler_param.scaler_info.scaler_en,
				yuvscaler_param.scaler_info.input_pixfmt, yuvscaler_param.scaler_info.output_pixfmt,
				yuvscaler_param.scaler_info.scaler_in_width, yuvscaler_param.scaler_info.scaler_in_height,
				yuvscaler_param.scaler_info.scaler_out_width, yuvscaler_param.scaler_info.scaler_out_height);
			pr_debug("path %d, scaler frame:  : scaler_factor_info: in_hor %d, out_hor %d, in_ver %d, out_ver %d\n",
				id, yuvscaler_param.scaler_info.scaler_factor_in_hor, yuvscaler_param.scaler_info.scaler_factor_out_hor,
				yuvscaler_param.scaler_info.scaler_factor_in_ver, yuvscaler_param.scaler_info.scaler_factor_out_ver);
			pr_debug("path %d, scaler frame:  : scaler_init_phase: hor %d, ver %d; scaler_tap_info: y_hor_tap %d, y_ver_tap %d, uv_hor_tap %d, uv_ver_tap %d\n",
				id, yuvscaler_param.scaler_info.scaler_init_phase_hor, yuvscaler_param.scaler_info.scaler_init_phase_ver,
				yuvscaler_param.scaler_info.scaler_y_hor_tap, yuvscaler_param.scaler_info.scaler_y_ver_tap,
				yuvscaler_param.scaler_info.scaler_uv_hor_tap, yuvscaler_param.scaler_info.scaler_uv_ver_tap);

			scaler_path = id;

			yuv_scaler_init_frame_info(&yuvscaler_param);

			for (r = 0; r < slice_array_height; r++) {
				for (c = 0; c < slice_array_width; c++) {
					uint32_t start_col = 0;
					uint32_t start_row = 0;
					uint32_t end_col = 0;
					uint32_t end_row = 0;
					uint32_t slice_id = 0;

					struct scaler_slice_t scaler_slice;
					struct slice_drv_overlap_scaler_param *scaler_overlap_param;
					struct slice_drv_region_info *wndInputOrg;
					struct slice_drv_region_info *wndOutputOrg;
					struct slice_drv_scaler_phase_info *phaseInfo;
					struct scaler_slice_t input_slice_info  = {0};
					struct scaler_slice_t output_slice_info = {0};

					slice_id = r * slice_array_width + c;
					scaler_slice.slice_id = slice_id;
					start_col = slice_pos_array[slice_id].start_col;
					start_row = slice_pos_array[slice_id].start_row;
					end_col = slice_pos_array[slice_id].end_col;
					end_row = slice_pos_array[slice_id].end_row;

					scaler_slice.start_col = start_col;
					scaler_slice.start_row = start_row;
					scaler_slice.end_col = end_col;
					scaler_slice.end_row = end_row;
					scaler_slice.sliceRows = slice_array_height;
					scaler_slice.sliceCols = slice_array_width;
					scaler_slice.sliceRowNo = r;
					scaler_slice.sliceColNo = c;
					scaler_slice.slice_width = end_col - start_col + 1;
					scaler_slice.slice_height = end_row - start_row + 1;
					scaler_slice.overlap_left = slice_overlap_array[slice_id].overlap_left;
					scaler_slice.overlap_right = slice_overlap_array[slice_id].overlap_right;
					scaler_slice.overlap_up = slice_overlap_array[slice_id].overlap_up;
					scaler_slice.overlap_down = slice_overlap_array[slice_id].overlap_down;

					if (id == 0)
						scaler_overlap_param = &slice_ctx->overlapParam.scaler1;
					if (id == 1)
						scaler_overlap_param = &slice_ctx->overlapParam.scaler2;

					wndInputOrg = &scaler_overlap_param->region_input[slice_id];
					wndOutputOrg = &scaler_overlap_param->region_output[slice_id];
					phaseInfo = &scaler_overlap_param->phase[slice_id];

					scaler_slice.init_phase_hor = phaseInfo->init_phase_hor;
					scaler_slice.init_phase_ver = phaseInfo->init_phase_ver;

					input_slice_info.start_col = wndInputOrg->sx;
					input_slice_info.end_col = wndInputOrg->ex;
					input_slice_info.start_row = wndInputOrg->sy;
					input_slice_info.end_row = wndInputOrg->ey;

					output_slice_info.start_col = wndOutputOrg->sx;
					output_slice_info.end_col = wndOutputOrg->ex;
					output_slice_info.start_row = wndOutputOrg->sy;
					output_slice_info.end_row = wndOutputOrg->ey;

					yuv_scaler_init_slice_info_v3(&yuvscaler_param, &slice_ctx->yuvscaler_slice_param,
						&scaler_slice, &input_slice_info, &output_slice_info);

					slice_param[id][slice_id].trim0_size_x = slice_ctx->yuvscaler_slice_param.trim0_info.trim_size_x;
					slice_param[id][slice_id].trim0_size_y = slice_ctx->yuvscaler_slice_param.trim0_info.trim_size_y;
					slice_param[id][slice_id].trim0_start_x = slice_ctx->yuvscaler_slice_param.trim0_info.trim_start_x;
					slice_param[id][slice_id].trim0_start_y = slice_ctx->yuvscaler_slice_param.trim0_info.trim_start_y;
					slice_param[id][slice_id].trim1_size_x = slice_ctx->yuvscaler_slice_param.trim1_info.trim_size_x;
					slice_param[id][slice_id].trim1_size_y = slice_ctx->yuvscaler_slice_param.trim1_info.trim_size_y;
					slice_param[id][slice_id].trim1_start_x = slice_ctx->yuvscaler_slice_param.trim1_info.trim_start_x;
					slice_param[id][slice_id].trim1_start_y = slice_ctx->yuvscaler_slice_param.trim1_info.trim_start_y;
					slice_param[id][slice_id].scaler_ip_int = slice_ctx->yuvscaler_slice_param.scaler_info.init_phase_info.scaler_init_phase_int[0][0];
					slice_param[id][slice_id].scaler_ip_rmd = slice_ctx->yuvscaler_slice_param.scaler_info.init_phase_info.scaler_init_phase_rmd[0][0];
					slice_param[id][slice_id].scaler_cip_int = slice_ctx->yuvscaler_slice_param.scaler_info.init_phase_info.scaler_init_phase_int[0][1];
					slice_param[id][slice_id].scaler_cip_rmd = slice_ctx->yuvscaler_slice_param.scaler_info.init_phase_info.scaler_init_phase_rmd[0][1];
					slice_param[id][slice_id].scaler_ip_int_ver = slice_ctx->yuvscaler_slice_param.scaler_info.init_phase_info.scaler_init_phase_int[1][0];
					slice_param[id][slice_id].scaler_ip_rmd_ver = slice_ctx->yuvscaler_slice_param.scaler_info.init_phase_info.scaler_init_phase_rmd[1][0];
					slice_param[id][slice_id].scaler_cip_int_ver = slice_ctx->yuvscaler_slice_param.scaler_info.init_phase_info.scaler_init_phase_int[1][1];
					slice_param[id][slice_id].scaler_cip_rmd_ver = slice_ctx->yuvscaler_slice_param.scaler_info.init_phase_info.scaler_init_phase_rmd[1][1];
					slice_param[id][slice_id].scaler_factor_in = yuvscaler_param.scaler_info.scaler_factor_in_hor;
					slice_param[id][slice_id].scaler_factor_out = yuvscaler_param.scaler_info.scaler_factor_out_hor;
					slice_param[id][slice_id].scaler_factor_in_ver = yuvscaler_param.scaler_info.scaler_factor_in_ver;
					slice_param[id][slice_id].scaler_factor_out_ver = yuvscaler_param.scaler_info.scaler_factor_out_ver;
					slice_param[id][slice_id].src_size_x = slice_ctx->yuvscaler_slice_param.src_size_x;
					slice_param[id][slice_id].src_size_y = slice_ctx->yuvscaler_slice_param.src_size_y;
					slice_param[id][slice_id].dst_size_x = slice_ctx->yuvscaler_slice_param.dst_size_x;
					slice_param[id][slice_id].dst_size_y = slice_ctx->yuvscaler_slice_param.dst_size_y;

					pr_debug("path %d, slice_id %d, trim0_start_x %d, trim0_start_y %d, trim0_size_x %d, trim0_size_y %d\n",
						id, slice_id, slice_param[id][slice_id].trim0_start_x, slice_param[id][slice_id].trim0_start_y,
						slice_param[id][slice_id].trim0_size_x, slice_param[id][slice_id].trim0_size_y);
					pr_debug("path %d, slice_id %d, trim1_start_x %d, trim1_start_y %d, trim1_size_x %d, trim1_size_y %d\n",
						id, slice_id, slice_param[id][slice_id].trim1_start_x, slice_param[id][slice_id].trim1_start_y,
						slice_param[id][slice_id].trim1_size_x, slice_param[id][slice_id].trim1_size_y);
					pr_debug("path %d, slice_id %d, scaler_ip_int %d, scaler_ip_rmd %d, scaler_cip_int %d, scaler_cip_rmd %d\n",
						id, slice_id, slice_param[id][slice_id].scaler_ip_int, slice_param[id][slice_id].scaler_ip_rmd,
						slice_param[id][slice_id].scaler_cip_int, slice_param[id][slice_id].scaler_cip_rmd);
					pr_debug("path %d, slice_id %d, scaler_ip_int_ver %d, scaler_ip_rmd_ver %d, scaler_cip_int_ver %d, scaler_cip_rmd_ver %d\n",
						id, slice_id,  slice_param[id][slice_id].scaler_ip_int_ver, slice_param[id][slice_id].scaler_ip_rmd_ver,
						slice_param[id][slice_id].scaler_cip_int_ver, slice_param[id][slice_id].scaler_cip_rmd_ver);
					pr_debug("path %d, slice_id %d, scaler_factor_in %d, scaler_factor_in_ver %d, scaler_factor_out %d, scaler_factor_out_ver %d\n",
						id, slice_id, slice_param[id][slice_id].scaler_factor_in, slice_param[id][slice_id].scaler_factor_in_ver,
						slice_param[id][slice_id].scaler_factor_out, slice_param[id][slice_id].scaler_factor_out_ver);
					pr_debug("path %d, slice_id %d, src_size_x %d, src_size_y %d, dst_size_x %d, dst_size_y %d\n",
						id, slice_id, slice_param[id][slice_id].src_size_x, slice_param[id][slice_id].src_size_y,
						slice_param[id][slice_id].dst_size_x, slice_param[id][slice_id].dst_size_y);

					if(0 == slc_input->calc_dyn_ov.path_scaler[id]->scaler.scaler_bypass) {
						if (0 == slice_ctx->yuvscaler_slice_param.trim1_info.trim_size_x)
							slice_param[id][slice_id].bypass = 1;
						else
							slice_param[id][slice_id].bypass = 0;
					}
				}
			}
		} else {
			for (slice_id = 0; slice_id < slice_num; slice_id++) {
				slice_param[id][slice_id].trim1_size_x = slice_pos_array[slice_id].end_col - slice_pos_array[slice_id].start_col + 1;
				slice_param[id][slice_id].trim1_size_y = slice_pos_array[slice_id].end_row - slice_pos_array[slice_id].start_row + 1;
				pr_debug("path %d, slice_id %d, trim1_size_x %d, trim1_size_y %d\n",
					id, slice_id, slice_param[id][slice_id].trim1_size_x, slice_param[id][slice_id].trim1_size_y);
			}
		}
	}

	return 0;
}

static int isp_init_param_for_overlap_v1(
		struct slice_cfg_input *slice_input, struct isp_slice_context *slice_ctx)
{
	int i = 0;
	uint32_t config_output_align_hor = 2;
	uint32_t yuvFormat = 0;
	uint32_t crop_region_w = 0;
	uint32_t crop_region_h = 0;
	uint32_t scaler1_in_width = 0;
	uint32_t scaler1_in_height = 0;
	uint32_t scaler2_in_width = 0;
	uint32_t scaler2_in_height = 0;
	struct ltm_rgb_stat_param_t ltm_param;
	struct isp_slice_context *slc_ctx = NULL;
	struct slice_drv_overlap_param_t *overlapParam = NULL;

	if (!slice_input || !slice_ctx) {
		pr_err("fail to get input prt NULL");
		return -1;
	}

	slc_ctx = slice_ctx;
	overlapParam = &slice_ctx->overlapParam;
	scaler_path = -1;
	slice_drv_calculate_overlap_init(overlapParam);

	/************************************************************************/
	/* img_type:  */
	/* 0:bayer 1:rgb 2:yuv444 3:yuv422 4:yuv420 5:yuv400  */
	/* 6:FBC bayer 7:FBC yuv420   */
	/************************************************************************/
	overlapParam->img_type = core_drv_get_fetch_fmt(slice_input->frame_fetch->fetch_fmt);
	if (overlapParam->img_type < 0)
		return -1;

	overlapParam->img_w = slc_ctx->img_width;
	overlapParam->img_h = slc_ctx->img_height;

	/************************************************************************/
	/* if has crop,  pipleine input size = crop.w, crop.h */
	/************************************************************************/
	crop_region_w = slice_input->calc_dyn_ov.crop.start_x + slice_input->calc_dyn_ov.crop.size_x;
	crop_region_h = slice_input->calc_dyn_ov.crop.start_y + slice_input->calc_dyn_ov.crop.size_y;
	if ((crop_region_w == slice_input->calc_dyn_ov.src.w) && (crop_region_h == slice_input->calc_dyn_ov.src.h))
		overlapParam->crop_en = 0;
	else
		overlapParam->crop_en = 1;

	overlapParam->crop_mode = 0;
	overlapParam->crop_sx = slice_input->calc_dyn_ov.crop.start_x;
	overlapParam->crop_sy = slice_input->calc_dyn_ov.crop.start_y;
	overlapParam->crop_w = slice_input->calc_dyn_ov.crop.size_x;
	overlapParam->crop_h = slice_input->calc_dyn_ov.crop.size_y;
	/************************************************************************/
	/* on whaleK slice_h >= img_h or slice_h >= crop_h */
	/************************************************************************/
	overlapParam->slice_w = slc_ctx->slice_width;
	overlapParam->slice_h = slc_ctx->slice_height;

	/************************************************************************/
	/* user define overlap */
	/************************************************************************/
	overlapParam->offlineCfgOverlap_en = 0;
	overlapParam->offlineCfgOverlap_left = 0;
	overlapParam->offlineCfgOverlap_right = 0;
	overlapParam->offlineCfgOverlap_up = 0;
	overlapParam->offlineCfgOverlap_down = 0;

	/* bayer */
	overlapParam->nlm_bypass = slice_input->nofilter_ctx->nlm_info_base.bypass;
	overlapParam->imbalance_bypass = slice_input->nofilter_ctx->imbalance_info_base.nlm_imblance_bypass;
	overlapParam->raw_gtm_stat_bypass = slice_input->nofilter_ctx->gtm_rgb_info.bypass_info.gtm_hist_stat_bypass;
	overlapParam->raw_gtm_map_bypass = slice_input->nofilter_ctx->gtm_rgb_info.bypass_info.gtm_map_bypass;
	overlapParam->cfa_bypass = slice_input->nofilter_ctx->cfa_info.bypass;

	/* rgb */
	overlapParam->ltmsta_rgb_bypass = slice_input->nofilter_ctx->ltm_rgb_info.ltm_stat.bypass;
	if (0 == slice_input->nofilter_ctx->ltm_rgb_info.ltm_stat.bypass) {
		uint32_t frame_width = slc_ctx->img_width;
		uint32_t frame_height = slc_ctx->img_height;
		struct ltm_rgb_stat_param_t *param_stat = &ltm_param;

		param_stat->strength = slice_input->nofilter_ctx->ltm_rgb_info.ltm_stat.strength;
		param_stat->region_est_en = slice_input->nofilter_ctx->ltm_rgb_info.ltm_stat.region_est_en;
		param_stat->text_point_thres = slice_input->nofilter_ctx->ltm_rgb_info.ltm_stat.text_point_thres;
		param_stat->text_proportion = slice_input->nofilter_ctx->ltm_rgb_info.ltm_stat.ltm_text.textture_proporion;
		param_stat->tile_num_auto = slice_input->nofilter_ctx->ltm_rgb_info.ltm_stat.tile_num_auto;
		param_stat->tile_num_col = slice_input->nofilter_ctx->ltm_rgb_info.ltm_stat.tile_num.tile_num_x;
		param_stat->tile_num_row = slice_input->nofilter_ctx->ltm_rgb_info.ltm_stat.tile_num.tile_num_y;

		ltm_rgb_stat_param_init(frame_width, frame_height, param_stat);
		overlapParam->ltmsta_rgb_binning_en = param_stat->binning_en;
	} else {
		overlapParam->ltmsta_rgb_binning_en = 0;
	}

	/* yuv */
	overlapParam->yuv420to422_bypass = 1;
	overlapParam->nr3d_bd_bypass = slice_input->nofilter_ctx->nr3d_info.blend.bypass;
	overlapParam->nr3d_bd_FBC_en = 0;
	overlapParam->ee_bypass = slice_input->nofilter_ctx->edge_info.bypass;
	overlapParam->ynr_bypass = slice_input->nofilter_ctx->ynr_info_v2.bypass;
	overlapParam->pre_cnr_bypass = slice_input->nofilter_ctx->pre_cdn_info.bypass;
	overlapParam->cnr_new_bypass = slice_input->nofilter_ctx->cdn_info.bypass;
	overlapParam->post_cnr_bypass = slice_input->nofilter_ctx->post_cdn_info.bypass;
	overlapParam->iir_cnr_bypass = slice_input->nofilter_ctx->iircnr_info.bypass;

	/* scaler submod */
	overlapParam->scaler_input_format = 4; /* 3:422 4:420 */
	/* scaler1 */
	overlapParam->scaler1.bypass = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->scaler.scaler_bypass;
	overlapParam->scaler1.FBC_enable = 0;
	overlapParam->scaler1.trim_eb = 1;
	overlapParam->scaler1.trim_start_x = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->in_trim.start_x;
	overlapParam->scaler1.trim_start_y = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->in_trim.start_y;
	overlapParam->scaler1.trim_size_x = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->in_trim.size_x;
	overlapParam->scaler1.trim_size_y = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->in_trim.size_y;
	overlapParam->scaler1.deci_x_eb = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->deci.deci_x_eb;
	overlapParam->scaler1.deci_y_eb = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->deci.deci_y_eb;
	overlapParam->scaler1.deci_x = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->deci.deci_x;
	overlapParam->scaler1.deci_y = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->deci.deci_y;
	overlapParam->scaler1.scl_init_phase_hor =
		slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->scaler.init_phase_info.scaler_init_phase[0];
	overlapParam->scaler1.scl_init_phase_ver =
		slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->scaler.init_phase_info.scaler_init_phase[1];
	overlapParam->scaler1.des_size_x = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->dst.w;
	overlapParam->scaler1.des_size_y = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->dst.h;
	if (overlapParam->scaler1.deci_x_eb)
		scaler1_in_width = overlapParam->scaler1.trim_size_x / overlapParam->scaler1.deci_x;
	else
		scaler1_in_width = overlapParam->scaler1.trim_size_x;
	if (overlapParam->scaler1.deci_y_eb)
		scaler1_in_height = overlapParam->scaler1.trim_size_y / overlapParam->scaler1.deci_y;
	else
		scaler1_in_height = overlapParam->scaler1.trim_size_y;

	if ((scaler1_in_width == overlapParam->scaler1.des_size_x)
		&& (scaler1_in_height == overlapParam->scaler1.des_size_y))
		overlapParam->scaler1.scaler_en = 0;
	else
		overlapParam->scaler1.scaler_en = 1;
	overlapParam->scaler1.yuv_output_format = 1;/* 0: 422 1: 420 */

	yuvFormat = slice_input->calc_dyn_ov.store[ISP_SPATH_CP]->color_fmt;
	if (YUV_OUT_UYVY_422 == yuvFormat) {
		config_output_align_hor = 2;
	} else if ((YUV_OUT_Y_UV_422 == yuvFormat)
			|| (YUV_OUT_Y_VU_422 == yuvFormat)
			|| (YUV_OUT_Y_UV_420 == yuvFormat)
			|| (YUV_OUT_Y_VU_420 == yuvFormat)) {
		config_output_align_hor = 4;
	} else if ((YUV_OUT_Y_U_V_422 == yuvFormat)
			||(YUV_OUT_Y_U_V_420 == yuvFormat)) {
		config_output_align_hor = 8;
	}
	overlapParam->scaler1.output_align_hor = config_output_align_hor;
	memcpy(scaler1_coeff_buf,
		slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->scaler.coeff_buf, sizeof(uint32_t) *ISP_SC_COEFF_BUF_SIZE);

	/* scaler2 */
	overlapParam->scaler2.bypass = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->scaler.scaler_bypass;
	overlapParam->scaler2.FBC_enable = 0;
	overlapParam->scaler2.trim_eb = 1;
	overlapParam->scaler2.trim_start_x = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->in_trim.start_x;
	overlapParam->scaler2.trim_start_y = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->in_trim.start_y;
	overlapParam->scaler2.trim_size_x = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->in_trim.size_x;
	overlapParam->scaler2.trim_size_y = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->in_trim.size_y;
	overlapParam->scaler2.deci_x_eb = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->deci.deci_x_eb;
	overlapParam->scaler2.deci_y_eb = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->deci.deci_y_eb;
	overlapParam->scaler2.deci_x = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->deci.deci_x;
	overlapParam->scaler2.deci_y = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->deci.deci_y;
	overlapParam->scaler2.scl_init_phase_hor =
		slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->scaler.init_phase_info.scaler_init_phase[0];
	overlapParam->scaler2.scl_init_phase_ver =
		slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->scaler.init_phase_info.scaler_init_phase[1];
	overlapParam->scaler2.des_size_x = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->dst.w;
	overlapParam->scaler2.des_size_y = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->dst.h;
	if (overlapParam->scaler2.deci_x_eb)
		scaler2_in_width = overlapParam->scaler2.trim_size_x / overlapParam->scaler2.deci_x;
	else
		scaler2_in_width = overlapParam->scaler2.trim_size_x;
	if (overlapParam->scaler2.deci_y_eb)
		scaler2_in_height = overlapParam->scaler2.trim_size_y / overlapParam->scaler2.deci_y;
	else
		scaler2_in_height = overlapParam->scaler2.trim_size_y;

	if ((scaler2_in_width == overlapParam->scaler2.des_size_x)
		&& (scaler2_in_height == overlapParam->scaler2.des_size_y))
		 overlapParam->scaler2.scaler_en = 0;
	else
		 overlapParam->scaler2.scaler_en = 1;
	overlapParam->scaler2.yuv_output_format = 1;/* 0: 422 1: 420 */

	config_output_align_hor = 2;
	yuvFormat = slice_input->calc_dyn_ov.store[ISP_SPATH_VID]->color_fmt;
	if (YUV_OUT_UYVY_422 == yuvFormat) {
		config_output_align_hor = 2;
	} else if ((YUV_OUT_Y_UV_422 == yuvFormat)
			|| (YUV_OUT_Y_VU_422 == yuvFormat)
			|| (YUV_OUT_Y_UV_420 == yuvFormat)
			|| (YUV_OUT_Y_VU_420 == yuvFormat)) {
		config_output_align_hor = 4;
	} else if ((YUV_OUT_Y_U_V_422 == yuvFormat)
			||(YUV_OUT_Y_U_V_420 == yuvFormat)) {
		config_output_align_hor = 8;
	}
	overlapParam->scaler2.output_align_hor = config_output_align_hor;
	memcpy(scaler2_coeff_buf,
		slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->scaler.coeff_buf, sizeof(uint32_t) *ISP_SC_COEFF_BUF_SIZE);

	pr_debug("raw : nlm %d, imbalance %d, gtm stat %d, gtm map %d, cfa %d\n",
			overlapParam->nlm_bypass, overlapParam->imbalance_bypass,
			overlapParam->raw_gtm_stat_bypass, overlapParam->raw_gtm_map_bypass, overlapParam->cfa_bypass);

	pr_debug("rgb : ltmsta %d\n",
			overlapParam->ltmsta_rgb_bypass);

	pr_debug("yuv : nr3d_bd %d, ee %d, ynr %d, pre_cnr %d, cnr_new %d, post_cnr %d, iir_cnr %d\n",
			overlapParam->nr3d_bd_bypass, overlapParam->ee_bypass, overlapParam->ynr_bypass,
			overlapParam->pre_cnr_bypass, overlapParam->cnr_new_bypass,
			overlapParam->post_cnr_bypass, overlapParam->iir_cnr_bypass);

	pr_debug("img_type %d, img_w %d, img_h %d, slice_w %d, slice_h %d\n",
		overlapParam->img_type, overlapParam->img_w, overlapParam->img_h,
		overlapParam->slice_w, overlapParam->slice_h);
	pr_debug("crop_info: en %d, mode %d, sx %d, sy %d, w %d, h %d\n",
		overlapParam->crop_en, overlapParam->crop_mode,
		overlapParam->crop_sx, overlapParam->crop_sy, overlapParam->crop_w,overlapParam->crop_h);

	pr_debug("scaler1_info: bypass %d, output_align_hor %d\n",
		overlapParam->scaler1.bypass, overlapParam->scaler1.output_align_hor);
	pr_debug("scaler1_info: trim_info: en %d, start_x %d, start_y %d, size_x %d, size_y %d\n",
		overlapParam->scaler1.trim_eb,
		overlapParam->scaler1.trim_start_x, overlapParam->scaler1.trim_start_y,
		overlapParam->scaler1.trim_size_x, overlapParam->scaler1.trim_size_y);
	pr_debug("scaler1_info: deci_info: x_eb %d, y_eb %d, deci_x %d, deci_y %d\n",
		overlapParam->scaler1.deci_x_eb, overlapParam->scaler1.deci_y_eb,
		overlapParam->scaler1.deci_x, overlapParam->scaler1.deci_y);
	pr_debug("scaler1_info: scl_init_phase_hor %d, scl_init_phase_ver %d\n",
		overlapParam->scaler1.scl_init_phase_hor, overlapParam->scaler1.scl_init_phase_ver);
	pr_debug("scaler1_info: scaler_en %d, in_width %d, in_height %d, out_width %d, out_height %d\n",
		overlapParam->scaler1.scaler_en, scaler1_in_width, scaler1_in_height,
		overlapParam->scaler1.des_size_x, overlapParam->scaler1.des_size_y);

	pr_debug("scaler2_info: bypass %d, output_align_hor %d\n",
		overlapParam->scaler2.bypass, overlapParam->scaler2.output_align_hor);
	pr_debug("scaler2_info: trim_info: en %d, start_x %d, start_y %d, size_x %d, size_y %d\n",
		overlapParam->scaler2.trim_eb,
		overlapParam->scaler2.trim_start_x, overlapParam->scaler2.trim_start_y,
		overlapParam->scaler2.trim_size_x, overlapParam->scaler2.trim_size_y);
	pr_debug("scaler2_info: deci_info: x_eb %d, y_eb %d, deci_x %d, deci_y %d\n",
		overlapParam->scaler2.deci_x_eb, overlapParam->scaler2.deci_y_eb,
		overlapParam->scaler2.deci_x, overlapParam->scaler2.deci_y);
	pr_debug("scaler2_info: scl_init_phase_hor %d, scl_init_phase_ver %d\n",
		overlapParam->scaler2.scl_init_phase_hor, overlapParam->scaler2.scl_init_phase_ver);
	pr_debug("scaler2_info: scaler_en %d, in_width %d, in_height %d, out_width %d, out_height %d\n",
		overlapParam->scaler2.scaler_en, scaler2_in_width, scaler2_in_height,
		overlapParam->scaler2.des_size_x, overlapParam->scaler2.des_size_y);

	/* calc overlap */
	slice_drv_calculate_overlap(overlapParam);

	for (i = 0 ; i < slc_ctx->slice_num; i++) {
		pr_debug("get calc result: slice id %d, region (%d, %d, %d, %d)\n",
			i, overlapParam->slice_region[i].sx, overlapParam->slice_region[i].sy,
			overlapParam->slice_region[i].ex, overlapParam->slice_region[i].ey);
		pr_debug("get calc result: slice id %d, overlap (left %d, right %d , up %d, down %d)\n",
			i, overlapParam->slice_overlap[i].ov_left, overlapParam->slice_overlap[i].ov_right,
			overlapParam->slice_overlap[i].ov_up, overlapParam->slice_overlap[i].ov_down);
	}

	return 0;
}

int isp_init_param_for_overlap_v2(
		struct slice_cfg_input *slice_input, struct isp_slice_context *slice_ctx)
{
	int i = 0, j = 0;
	uint32_t config_output_align_hor = 2;
	uint32_t yuvFormat = 0;
	uint32_t crop_region_w = 0;
	uint32_t crop_region_h = 0;
	uint32_t scaler1_in_width = 0;
	uint32_t scaler1_in_height = 0;
	uint32_t scaler2_in_width = 0;
	uint32_t scaler2_in_height = 0;
	uint32_t layer_num = 0;
	uint32_t slice_align_size = 0;
	struct isp_slice_context *slc_ctx = NULL;
	struct alg_slice_drv_overlap *slice_overlap = NULL;

	if (!slice_input || !slice_ctx) {
		pr_err("fail to get input prt NULL");
		return -1;
	}

	slc_ctx = slice_ctx;
	slice_overlap = &slice_ctx->slice_overlap;
	scaler_path = -1;
	memset(slice_overlap,0,sizeof(struct alg_slice_drv_overlap));
	slice_overlap->scaler1.frameParam = &slice_overlap->scaler1.frameParamObj;
	slice_overlap->scaler2.frameParam = &slice_overlap->scaler2.frameParamObj;

	/************************************************************************/
	/* img_type:  */
	/* 0:bayer 1:rgb 2:yuv444 3:yuv422 4:yuv420 5:yuv400  */
	/* 6:FBC bayer 7:FBC yuv420   */
	/************************************************************************/
	layer_num = slice_input->calc_dyn_ov.pyr_layer_num;
	slice_overlap->img_type = core_drv_get_fetch_fmt(slice_input->frame_fetch->fetch_fmt);
	if (slice_overlap->img_type < 0)
		return -1;
	slice_overlap->scaler_input_format = 4;/*3:422 4:420*/
	slice_overlap->img_w = slc_ctx->img_width;
	slice_overlap->img_h = slc_ctx->img_height;

	ISP_OVERLAP_DEBUG("src %d %d layer num %d img_type %d\n", slice_overlap->img_w, slice_overlap->img_h, layer_num,
			slice_overlap->img_type);
	/* update layer num based on img size */
	while (isp_rec_small_layer_w(slice_overlap->img_w, layer_num) < MIN_PYR_WIDTH ||
		isp_rec_small_layer_h(slice_overlap->img_h, layer_num) < MIN_PYR_HEIGHT) {
		ISP_OVERLAP_DEBUG("layer num need decrease based on small input %d %d\n",
				slice_overlap->img_w, slice_overlap->img_h);
		if (--layer_num == 0)
			break;
	}
	slice_input->calc_dyn_ov.pyr_layer_num = layer_num;

	if (layer_num != 0) {
		slice_overlap->input_layer_w = isp_rec_small_layer_w(slice_overlap->img_w, layer_num);
		slice_overlap->input_layer_h = isp_rec_small_layer_h(slice_overlap->img_h, layer_num);
	} else {
		slice_overlap->input_layer_w = slice_overlap->img_w;
		slice_overlap->input_layer_h = slice_overlap->img_h;
	}

	ISP_OVERLAP_DEBUG("layer w %d h %d\n", slice_overlap->input_layer_w, slice_overlap->input_layer_h);
	ISP_OVERLAP_DEBUG("image w %d h %d\n", slice_overlap->img_w, slice_overlap->img_h);
	slice_overlap->input_layer_id = layer_num;
	slice_overlap->img_src_w = slice_overlap->img_w;
	slice_overlap->img_src_h = slice_overlap->img_h;
	slice_overlap->uw_sensor = slice_input->calc_dyn_ov.need_dewarping;
	ISP_OVERLAP_DEBUG("uw_sensor %d\n", slice_overlap->uw_sensor);

	crop_region_w = slice_input->calc_dyn_ov.crop.start_x + slice_input->calc_dyn_ov.crop.size_x;
	crop_region_h = slice_input->calc_dyn_ov.crop.start_y + slice_input->calc_dyn_ov.crop.size_y;
	if ((crop_region_w == slice_input->calc_dyn_ov.src.w) && (crop_region_h == slice_input->calc_dyn_ov.src.h))
		slice_overlap->crop_en = 0;
	else
		slice_overlap->crop_en = 1;

	slice_overlap->crop_mode = 0;
	slice_overlap->crop_sx = slice_input->calc_dyn_ov.crop.start_x;
	slice_overlap->crop_sy = slice_input->calc_dyn_ov.crop.start_y;
	slice_overlap->crop_w = slice_input->calc_dyn_ov.crop.size_x;
	slice_overlap->crop_h = slice_input->calc_dyn_ov.crop.size_y;
	ISP_OVERLAP_DEBUG("crop_en %d, crop_mode %d\n", slice_overlap->crop_en, slice_overlap->crop_mode);
	ISP_OVERLAP_DEBUG("crop sx %d, sy %d\n", slice_overlap->crop_sx, slice_overlap->crop_sy);
	ISP_OVERLAP_DEBUG("crop w %d, h %d\n", slice_overlap->crop_w, slice_overlap->crop_h);
	ISP_OVERLAP_DEBUG("src w %d, h %d\n", slice_input->calc_dyn_ov.src.w, slice_input->calc_dyn_ov.src.h);

	slice_align_size = PYR_DEC_WIDTH_ALIGN << layer_num;
	slice_overlap->slice_w = (slc_ctx->slice_width + slice_align_size - 1) & ~( slice_align_size - 1);
	ISP_OVERLAP_DEBUG("slice w %d\n", slice_overlap->slice_w);
	slice_overlap->slice_h = slc_ctx->slice_height;
	slice_overlap->offline_slice_mode = 1;

	slice_overlap->nr3d_bd_bypass = 1;
	slice_overlap->nr3d_bd_FBC_en = 0;
	slice_overlap->yuv420_to_rgb10_bypass = 0;
	slice_overlap->ltm_sat.bypass = slice_input->nofilter_ctx->ltm_rgb_info.ltm_stat.bypass;
	slice_overlap->ynr_bypass = slice_input->nofilter_ctx->ynr_info_v3.bypass;
	slice_overlap->ee_bypass = slice_input->nofilter_ctx->edge_info_v3.bypass;
	slice_overlap->cnr_new_bypass = slice_input->nofilter_ctx->cdn_info.bypass;
	slice_overlap->post_cnr_bypass = slice_input->nofilter_ctx->post_cnr_h_info.bypass;
	slice_overlap->cnr_bypass = slice_input->nofilter_ctx->cnr_info.bypass;

	/* current close ltm, after ltm param normal, open it again */
	slice_overlap->ltm_sat.bypass = 1;
	if (0 == slice_overlap->ltm_sat.bypass) {
		uint32_t frame_width = slc_ctx->img_width;
		uint32_t frame_height = slc_ctx->img_height;

		slice_overlap->ltm_sat.strength = slice_input->nofilter_ctx->ltm_rgb_info.ltm_stat.strength;
		slice_overlap->ltm_sat.region_est_en = slice_input->nofilter_ctx->ltm_rgb_info.ltm_stat.region_est_en;
		slice_overlap->ltm_sat.text_point_thres = slice_input->nofilter_ctx->ltm_rgb_info.ltm_stat.text_point_thres;
		slice_overlap->ltm_sat.text_proportion = slice_input->nofilter_ctx->ltm_rgb_info.ltm_stat.ltm_text.textture_proporion;
		slice_overlap->ltm_sat.tile_num_auto = slice_input->nofilter_ctx->ltm_rgb_info.ltm_stat.tile_num_auto;
		slice_overlap->ltm_sat.tile_num_col = slice_input->nofilter_ctx->ltm_rgb_info.ltm_stat.tile_num.tile_num_x;
		slice_overlap->ltm_sat.tile_num_row = slice_input->nofilter_ctx->ltm_rgb_info.ltm_stat.tile_num.tile_num_y;
		ISP_OVERLAP_DEBUG("tile_num_col %d, tile_num_row %d", slice_overlap->ltm_sat.tile_num_col, slice_overlap->ltm_sat.tile_num_row);

		ltm_rgb_stat_param_init(frame_width, frame_height, &slice_overlap->ltm_sat);
	}

	/*yuv rec*/
	slice_overlap->pyramid_rec_bypass = !layer_num;
	slice_overlap->layerNum = layer_num + 1;
	ISP_OVERLAP_DEBUG("isp pyr rec bypass %d layer num %d\n", slice_overlap->pyramid_rec_bypass,
		slice_overlap->layerNum);
	/* yuv */
	slice_overlap->dewarping_bypass = !slice_input->calc_dyn_ov.need_dewarping;
	if (slice_overlap->pyramid_rec_bypass) {
		slice_overlap->dewarping_width = slice_overlap->img_w;
		slice_overlap->dewarping_height = slice_overlap->img_h;
		slice_overlap->ynr_bypass = 1;
		slice_overlap->cnr_bypass = 1;
	} else {
		slice_overlap->dewarping_width = slice_overlap->img_src_w;
		slice_overlap->dewarping_height = slice_overlap->img_src_h;
	}

	ISP_OVERLAP_DEBUG("ltm %d ynr %d ee %d cnr %d dewarp %d cnr_new %d post-cnr %d\n",
		slice_overlap->ltm_sat.bypass, slice_overlap->ynr_bypass, slice_overlap->ee_bypass, slice_overlap->cnr_bypass,
		slice_overlap->dewarping_bypass, slice_overlap->cnr_new_bypass, slice_overlap->post_cnr_bypass);
	ISP_OVERLAP_DEBUG("dewarping w %d h %d\n", slice_overlap->dewarping_width, slice_overlap->dewarping_height);

	/* TBD: 3dnr fbc just temp close */
	slice_overlap->nr3d_bd_FBC_en = 0;

	/* scaler1 */
	slice_overlap->scaler1.bypass = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->scaler.scaler_bypass;
	slice_overlap->scaler1.scaler_id = 1;
	slice_overlap->scaler1.FBC_enable = 0;
	slice_overlap->scaler1.trim_eb = 1;
	slice_overlap->scaler1.trim_start_x = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->in_trim.start_x;
	slice_overlap->scaler1.trim_start_y = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->in_trim.start_y;
	slice_overlap->scaler1.trim_size_x = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->in_trim.size_x;
	slice_overlap->scaler1.trim_size_y = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->in_trim.size_y;
	slice_overlap->scaler1.deci_x_eb = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->deci.deci_x_eb;
	slice_overlap->scaler1.deci_y_eb = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->deci.deci_y_eb;
	slice_overlap->scaler1.deci_x = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->deci.deci_x;
	slice_overlap->scaler1.deci_y = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->deci.deci_y;

	ISP_OVERLAP_DEBUG("trim start x%d y%d size_x %d size_y %d deci_x %d deci_y %d deci_x_eb %d y_eb %d\n",
		slice_overlap->scaler1.trim_start_x, slice_overlap->scaler1.trim_start_y, slice_overlap->scaler1.trim_size_x,
		slice_overlap->scaler1.trim_size_y, slice_overlap->scaler1.deci_x, slice_overlap->scaler1.deci_y,
		slice_overlap->scaler1.deci_x_eb, slice_overlap->scaler1.deci_y_eb);
	slice_overlap->scaler1.scl_init_phase_hor =
		slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->scaler.init_phase_info.scaler_init_phase[0];
	slice_overlap->scaler1.scl_init_phase_ver =
		slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->scaler.init_phase_info.scaler_init_phase[1];
	slice_overlap->scaler1.des_size_x = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->dst.w;
	slice_overlap->scaler1.des_size_y = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->dst.h;
	if (slice_overlap->scaler1.deci_x_eb)
		scaler1_in_width = slice_overlap->scaler1.trim_size_x / slice_overlap->scaler1.deci_x;
	else
		scaler1_in_width = slice_overlap->scaler1.trim_size_x;
	if (slice_overlap->scaler1.deci_y_eb)
		scaler1_in_height = slice_overlap->scaler1.trim_size_y / slice_overlap->scaler1.deci_y;
	else
		scaler1_in_height = slice_overlap->scaler1.trim_size_y;

	if ((scaler1_in_width == slice_overlap->scaler1.des_size_x)
		&& (scaler1_in_height == slice_overlap->scaler1.des_size_y))
		slice_overlap->scaler1.scaler_en = 0;
	else
		slice_overlap->scaler1.scaler_en = 1;
	yuvFormat = slice_input->calc_dyn_ov.store[ISP_SPATH_CP]->color_fmt;
	if (YUV_OUT_UYVY_422 == yuvFormat) {
		config_output_align_hor = 2;
	} else if ((YUV_OUT_Y_UV_422 == yuvFormat)
			|| (YUV_OUT_Y_VU_422 == yuvFormat)
			|| (YUV_OUT_Y_UV_420 == yuvFormat)
			|| (YUV_OUT_Y_VU_420 == yuvFormat)) {
		config_output_align_hor = 4;
	} else if ((YUV_OUT_Y_U_V_422 == yuvFormat)
			||(YUV_OUT_Y_U_V_420 == yuvFormat)) {
		config_output_align_hor = 8;
	}
	slice_overlap->scaler1.yuv_output_format = 1;/* 0: 422 1: 420 */
	slice_overlap->scaler1.output_align_hor = config_output_align_hor;
	ISP_OVERLAP_DEBUG("phase_hor %d ver %d dstw %d dsth %d scaler_eb %d format %d align %d\n",
		slice_overlap->scaler1.scl_init_phase_hor, slice_overlap->scaler1.scl_init_phase_ver,
		slice_overlap->scaler1.des_size_x, slice_overlap->scaler1.des_size_y,
		slice_overlap->scaler1.scaler_en, slice_overlap->scaler1.yuv_output_format,
		slice_overlap->scaler1.output_align_hor);
	slice_overlap->scaler1.scaler_y_hor_tap = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->scaler.scaler_y_hor_tap;
	slice_overlap->scaler1.scaler_uv_hor_tap = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->scaler.scaler_uv_hor_tap;
	slice_overlap->scaler1.scaler_y_ver_tap = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->scaler.scaler_y_ver_tap;
	slice_overlap->scaler1.scaler_uv_ver_tap = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_CP]->scaler.scaler_uv_ver_tap;
	ISP_OVERLAP_DEBUG("scaler tap %d %d %d %d\n", slice_overlap->scaler1.scaler_y_hor_tap, slice_overlap->scaler1.scaler_uv_hor_tap,
		slice_overlap->scaler1.scaler_y_ver_tap, slice_overlap->scaler1.scaler_uv_ver_tap);

	/* scaler2 */
	slice_overlap->scaler2.bypass = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->scaler.scaler_bypass;
	slice_overlap->scaler2.scaler_id = 2;
	slice_overlap->scaler2.FBC_enable = 0;
	slice_overlap->scaler2.trim_eb = 1;
	slice_overlap->scaler2.trim_start_x = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->in_trim.start_x;
	slice_overlap->scaler2.trim_start_y = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->in_trim.start_y;
	slice_overlap->scaler2.trim_size_x = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->in_trim.size_x;
	slice_overlap->scaler2.trim_size_y = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->in_trim.size_y;
	slice_overlap->scaler2.deci_x_eb = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->deci.deci_x_eb;
	slice_overlap->scaler2.deci_y_eb = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->deci.deci_y_eb;
	slice_overlap->scaler2.deci_x = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->deci.deci_x;
	slice_overlap->scaler2.deci_y = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->deci.deci_y;
	slice_overlap->scaler2.scl_init_phase_hor =
		slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->scaler.init_phase_info.scaler_init_phase[0];
	slice_overlap->scaler2.scl_init_phase_ver =
		slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->scaler.init_phase_info.scaler_init_phase[1];
	slice_overlap->scaler2.des_size_x = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->dst.w;
	slice_overlap->scaler2.des_size_y = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->dst.h;
	if (slice_overlap->scaler2.deci_x_eb)
		scaler2_in_width = slice_overlap->scaler2.trim_size_x / slice_overlap->scaler2.deci_x;
	else
		scaler2_in_width = slice_overlap->scaler2.trim_size_x;
	if (slice_overlap->scaler2.deci_y_eb)
		scaler2_in_height = slice_overlap->scaler2.trim_size_y / slice_overlap->scaler2.deci_y;
	else
		scaler2_in_height = slice_overlap->scaler2.trim_size_y;

	if ((scaler2_in_width == slice_overlap->scaler2.des_size_x)
		&& (scaler2_in_height == slice_overlap->scaler2.des_size_y))
		slice_overlap->scaler2.scaler_en = 0;
	else
		slice_overlap->scaler2.scaler_en = 1;
	slice_overlap->scaler2.yuv_output_format = 1; /* 0: 422 1: 420 */

	config_output_align_hor = 2;
	yuvFormat = slice_input->calc_dyn_ov.store[ISP_SPATH_VID]->color_fmt;
	if (YUV_OUT_UYVY_422 == yuvFormat) {
		config_output_align_hor = 2;
	} else if ((YUV_OUT_Y_UV_422 == yuvFormat)
			|| (YUV_OUT_Y_VU_422 == yuvFormat)
			|| (YUV_OUT_Y_UV_420 == yuvFormat)
			|| (YUV_OUT_Y_VU_420 == yuvFormat)) {
		config_output_align_hor = 4;
	} else if ((YUV_OUT_Y_U_V_422 == yuvFormat)
			||(YUV_OUT_Y_U_V_420 == yuvFormat)) {
		config_output_align_hor = 8;
	}
	slice_overlap->scaler2.output_align_hor = config_output_align_hor;
	slice_overlap->scaler2.scaler_y_hor_tap = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->scaler.scaler_y_hor_tap;
	slice_overlap->scaler2.scaler_uv_hor_tap = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->scaler.scaler_uv_hor_tap;
	slice_overlap->scaler2.scaler_y_ver_tap = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->scaler.scaler_y_ver_tap;
	slice_overlap->scaler2.scaler_uv_ver_tap = slice_input->calc_dyn_ov.path_scaler[ISP_SPATH_VID]->scaler.scaler_uv_ver_tap;

	/* TBD: thumbnail scaler need to debug */
	slice_overlap->thumbnailscaler.bypass = slice_input->calc_dyn_ov.thumb_scaler->scaler_bypass;
	slice_overlap->thumbnailscaler.trim0_en = 1;
	slice_overlap->thumbnailscaler.trim0_start_x = slice_input->calc_dyn_ov.thumb_scaler->y_trim.start_x;
	slice_overlap->thumbnailscaler.trim0_start_y = slice_input->calc_dyn_ov.thumb_scaler->y_trim.start_y;
	slice_overlap->thumbnailscaler.trim0_size_x = slice_input->calc_dyn_ov.thumb_scaler->y_trim.size_x;
	slice_overlap->thumbnailscaler.trim0_size_y = slice_input->calc_dyn_ov.thumb_scaler->y_trim.size_y;
	slice_overlap->thumbnailscaler.phase_x = slice_input->calc_dyn_ov.thumb_scaler->y_init_phase.w;
	slice_overlap->thumbnailscaler.phase_y = slice_input->calc_dyn_ov.thumb_scaler->y_init_phase.h;
	slice_overlap->thumbnailscaler.base_align = 4;/* TBD: just set temp 4 */
	slice_overlap->thumbnailscaler.out_w = slice_input->calc_dyn_ov.thumb_scaler->y_dst_after_scaler.w;
	slice_overlap->thumbnailscaler.out_h = slice_input->calc_dyn_ov.thumb_scaler->y_dst_after_scaler.h;
	slice_overlap->thumbnailscaler.out_format = 1;
	slice_overlap->thumbnailscaler.slice_num = slc_ctx->slice_num;

	ISP_OVERLAP_DEBUG("start x %d y %d size x %d y %d phase x %d y %d out w %d h %d\n",
		slice_overlap->thumbnailscaler.trim0_start_x, slice_overlap->thumbnailscaler.trim0_start_y,
		slice_overlap->thumbnailscaler.trim0_size_x, slice_overlap->thumbnailscaler.trim0_size_y,
		slice_overlap->thumbnailscaler.phase_x, slice_overlap->thumbnailscaler.phase_y,
		slice_overlap->thumbnailscaler.out_w, slice_overlap->thumbnailscaler.out_h);

	/* user define ovlap only for debug use */
	slice_overlap->offlineCfgOverlap_en = 0;
	slice_overlap->offlineCfgOverlap_left = 0;
	slice_overlap->offlineCfgOverlap_right = 0;
	slice_overlap->offlineCfgOverlap_up = 0;
	slice_overlap->offlineCfgOverlap_down = 0;

	/* calc overlap */
	alg_slice_calc_drv_overlap(slice_overlap);

	/* update thumb_scaler_cfg param */
	slice_input->calc_dyn_ov.thumb_scaler->y_deci.deci_x = slice_overlap->thumbnailscaler.y_deci_hor_par;
	slice_input->calc_dyn_ov.thumb_scaler->y_deci.deci_x_eb = slice_overlap->thumbnailscaler.y_deci_hor_en;
	slice_input->calc_dyn_ov.thumb_scaler->y_deci.deci_y = slice_overlap->thumbnailscaler.y_deci_ver_par;
	slice_input->calc_dyn_ov.thumb_scaler->y_deci.deci_y_eb = slice_overlap->thumbnailscaler.y_deci_ver_en;
	slice_input->calc_dyn_ov.thumb_scaler->uv_deci.deci_x = slice_overlap->thumbnailscaler.uv_deci_hor_par;
	slice_input->calc_dyn_ov.thumb_scaler->uv_deci.deci_x_eb = slice_overlap->thumbnailscaler.uv_deci_hor_en;
	slice_input->calc_dyn_ov.thumb_scaler->uv_deci.deci_y = slice_overlap->thumbnailscaler.uv_deci_ver_par;
	slice_input->calc_dyn_ov.thumb_scaler->uv_deci.deci_y_eb = slice_overlap->thumbnailscaler.uv_deci_ver_en;
	slice_input->calc_dyn_ov.thumb_scaler->y_src_after_deci.w = slice_overlap->thumbnailscaler.y_frame_src_size_hor;
	slice_input->calc_dyn_ov.thumb_scaler->y_src_after_deci.h = slice_overlap->thumbnailscaler.y_frame_src_size_ver;
	slice_input->calc_dyn_ov.thumb_scaler->y_dst_after_scaler.w = slice_overlap->thumbnailscaler.y_frame_des_size_hor;
	slice_input->calc_dyn_ov.thumb_scaler->y_dst_after_scaler.h = slice_overlap->thumbnailscaler.y_frame_des_size_ver;
	slice_input->calc_dyn_ov.thumb_scaler->uv_src_after_deci.w = slice_overlap->thumbnailscaler.uv_frame_src_size_hor;
	slice_input->calc_dyn_ov.thumb_scaler->uv_src_after_deci.h = slice_overlap->thumbnailscaler.uv_frame_src_size_ver;
	slice_input->calc_dyn_ov.thumb_scaler->uv_dst_after_scaler.w = slice_overlap->thumbnailscaler.uv_frame_des_size_hor;
	slice_input->calc_dyn_ov.thumb_scaler->uv_dst_after_scaler.h = slice_overlap->thumbnailscaler.uv_frame_des_size_ver;

	for (i = 0 ; i < slc_ctx->slice_num; i++) {
		ISP_OVERLAP_DEBUG("get calc result: slice id %d, region (%d, %d, %d, %d)\n",
			i, slice_overlap->slice_region[i].sx, slice_overlap->slice_region[i].sy,
			slice_overlap->slice_region[i].ex, slice_overlap->slice_region[i].ey);
		ISP_OVERLAP_DEBUG("get calc result: slice id %d, overlap (left %d, right %d )\n",
			i, slice_overlap->slice_overlap[i].ov_left, slice_overlap->slice_overlap[i].ov_right);
	}
	for (j = 0; j < layer_num + 1; j++) {
		ISP_OVERLAP_DEBUG("cur pyr layer %d slice_num %d\n", j, slice_overlap->slice_number[j]);
		for (i = 0; i < slice_overlap->slice_number[j]; i++) {
			ISP_OVERLAP_DEBUG("get calc result: slice id %d, fetch0 region (%d, %d, %d, %d)\n",
				i, slice_overlap->fecth0_slice_region[j][i].sx, slice_overlap->fecth0_slice_region[j][i].sy,
				slice_overlap->fecth0_slice_region[j][i].ex, slice_overlap->fecth0_slice_region[j][i].ey);
			ISP_OVERLAP_DEBUG("get calc result: slice id %d, fetch1 region (%d, %d, %d, %d)\n",
				i, slice_overlap->fecth1_slice_region[j][i].sx, slice_overlap->fecth1_slice_region[j][i].sy,
				slice_overlap->fecth1_slice_region[j][i].ex, slice_overlap->fecth1_slice_region[j][i].ey);
			ISP_OVERLAP_DEBUG("get calc result: slice id %d, fetch0 overlap (left %d, right %d)\n",
				i, slice_overlap->fecth0_slice_overlap[j][i].ov_left, slice_overlap->fecth0_slice_overlap[j][i].ov_right);
			ISP_OVERLAP_DEBUG("get calc result: slice id %d, store region (%d, %d, %d, %d)\n",
				i, slice_overlap->store_rec_slice_region[j][i].sx, slice_overlap->store_rec_slice_region[j][i].sy,
				slice_overlap->store_rec_slice_region[j][i].ex, slice_overlap->store_rec_slice_region[j][i].ey);
			ISP_OVERLAP_DEBUG("get calc result: slice id %d, store rec overlap (left %d, right %d)\n",
				i, slice_overlap->store_rec_slice_overlap[j][i].ov_left, slice_overlap->store_rec_slice_overlap[j][i].ov_right);
			ISP_OVERLAP_DEBUG("get calc result: slice id %d, store_crop overlap (left %d, right %d )\n",
				i, slice_overlap->store_rec_slice_crop_overlap[j][i].ov_left, slice_overlap->store_rec_slice_crop_overlap[j][i].ov_right);
		}
	}

	return 0;
}

int alg_isp_init_yuvscaler_slice(void *slc_cfg_input, void *slc_ctx, struct isp_fw_scaler_slice (*slice_param)[PIPE_MAX_SLICE_NUM])
{
	int ret = 0;
	uint32_t dyn_ov_version = 0;
	struct slice_cfg_input *slc_input = NULL;

	if (!slc_cfg_input || !slc_ctx || !slice_param) {
		pr_err("fail to get input ptr NULL\n");
		return -1;
	}

	slc_input = (struct slice_cfg_input *)slc_cfg_input;
	dyn_ov_version = slc_input->calc_dyn_ov.verison;

	switch (dyn_ov_version) {
		case ALG_ISP_OVERLAP_VER_1:
			ret = isp_init_param_for_yuvscaler_slice(slc_cfg_input, slc_ctx, slice_param);
			break;
		default:
			pr_err("fail to get dyn_ov version %d\n", dyn_ov_version);
			ret = -1;
			break;
	}

	if (ret) {
		pr_err("fail to init overlap param ret %d\n", ret);
		return -1;
	}
	return ret;
}

int alg_isp_get_dynamic_overlap(void *cfg_slice_in, void*slc_ctx)
{
	int ret = 0;
	uint32_t dyn_ov_version = 0;
	struct slice_cfg_input *slice_input = NULL;
	struct isp_slice_context *slice_ctx = NULL;

	if (!cfg_slice_in || !slc_ctx) {
		pr_err("fail to get input ptr NULL\n");
		return -1;
	}

	slice_input = (struct slice_cfg_input *)cfg_slice_in;
	slice_ctx = (struct isp_slice_context *)slc_ctx;

	dyn_ov_version = slice_input->calc_dyn_ov.verison;
	switch (dyn_ov_version) {
		case ALG_ISP_OVERLAP_VER_1:
			ret = isp_init_param_for_overlap_v1(slice_input, slice_ctx);
			break;
		case ALG_ISP_OVERLAP_VER_2:
			ret = isp_init_param_for_overlap_v2(slice_input, slice_ctx);
			break;
		default:
			pr_err("fail to get dyn_ov version %d\n", dyn_ov_version);
			ret = -1;
			break;
	}

	if (ret) {
		pr_err("fail to init overlap param  ret %d\n", ret);
		return -1;
	}

	return ret;
}

