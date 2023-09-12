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
#include <linux/vmalloc.h>
#include <sprd_mm.h>

#include "alg_isp_overlap.h"
#include "isp_reg.h"
#include "isp_core.h"
#include "isp_slice.h"
#include "isp_pyr_rec.h"
#include "alg_nr3_calc.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_SLICE: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

struct isp_scaler_slice_tmp {
	uint32_t slice_row_num;
	uint32_t slice_col_num;
	uint32_t start_col;
	uint32_t start_row;
	uint32_t end_col;
	uint32_t end_row;
	uint32_t start_col_orig;
	uint32_t start_row_orig;
	uint32_t end_col_orig;
	uint32_t end_row_orig;
	uint32_t x;
	uint32_t y;
	uint32_t overlap_bad_up;
	uint32_t overlap_bad_down;
	uint32_t overlap_bad_left;
	uint32_t overlap_bad_right;
	uint32_t trim0_end_x;
	uint32_t trim0_end_y;
	uint32_t trim0_start_adjust_x;
	uint32_t trim0_start_adjust_y;
	uint32_t trim1_sum_x;
	uint32_t trim1_sum_y;
	uint32_t deci_x;
	uint32_t deci_y;
	uint32_t deci_x_align;
	uint32_t deci_y_align;
	uint32_t scaler_out_height_temp;
	uint32_t scaler_out_width_temp;
};

static void ispslice_scaler_phase_calc(uint32_t phase, uint32_t factor,
	uint32_t *phase_int, uint32_t *phase_rmd)
{
	phase_int[0] = (uint32_t)(phase / factor);
	phase_rmd[0] = (uint32_t)(phase - factor * phase_int[0]);
}

static int ispslice_noisefliter_info_set(struct isp_slice_desc *slc_ctx,
		struct isp_slice_context *ctx)
{
	int rtn = 0;
	uint32_t slice_id = 0;
	uint32_t slice_num = 0;
	uint32_t slice_width = 0;
	uint32_t seed0 = 0;
	struct isp_slice_desc *cur_slc = slc_ctx;
	struct slice_noisefilter_info *noisefilter_info = NULL;
	struct slice_scaler_info *scaler_info = NULL;

	if (!ctx) {
		pr_err("fail to get valid input ptr\n");
		rtn = -EINVAL;
		goto exit;
	}

	slice_num = ctx->slice_num;
	seed0 = cur_slc->slice_noisefilter_mode.seed_for_mode1;
	pr_debug("yrandom_mode=%d,slice_num=%d\n",
		cur_slc->slice_noisefilter_mode.yrandom_mode, slice_num);
	if (cur_slc->slice_noisefilter_mode.yrandom_mode == 1) {
		for (slice_id = 0; slice_id < slice_num; slice_id++, cur_slc++) {
			scaler_info = &cur_slc->slice_scaler[0];
			noisefilter_info = &cur_slc->noisefilter_info;
			noisefilter_info->seed0 = seed0;
			slice_width = scaler_info->trim1_size_x;
			cam_block_noisefilter_seeds(slice_width,
				noisefilter_info->seed0, &noisefilter_info->seed1,
				&noisefilter_info->seed2, &noisefilter_info->seed3);
			pr_debug("seed0=%d,seed1=%d,seed2=%d,seed3=%d,slice_width=%d\n",
				noisefilter_info->seed0, noisefilter_info->seed1,
					noisefilter_info->seed2, noisefilter_info->seed3, slice_width);
		}
	} else
		return rtn;

exit:
	return rtn;
}

static void ispslice_spath_trim0_info_cfg(
		struct isp_scaler_slice_tmp *sinfo,
		struct img_trim *frm_trim0,
		struct slice_scaler_info *slc_scaler)
{
	uint32_t start, end;

	/* trim0 x */
	start = sinfo->start_col + sinfo->overlap_bad_left;
	end = sinfo->end_col + 1 - sinfo->overlap_bad_right;

	pr_debug("start end %d, %d\n", start, end);
	pr_debug("frame %d %d %d %d\n",
		frm_trim0->start_x,
		frm_trim0->start_y,
		frm_trim0->size_x,
		frm_trim0->size_y);

	if (sinfo->slice_col_num == 1) {
		slc_scaler->trim0_start_x = frm_trim0->start_x;
		slc_scaler->trim0_size_x = frm_trim0->size_x;
		pr_debug("%d, %d\n", slc_scaler->trim0_start_x, slc_scaler->trim0_size_x);
	} else {
		if ((sinfo->end_col_orig < frm_trim0->start_x)
			|| (sinfo->start_col_orig > (frm_trim0->start_x + frm_trim0->size_x - 1))) {
			slc_scaler->out_of_range = 1;
			return;
		}

		if (sinfo->x == 0) {
			/* first slice */
			slc_scaler->trim0_start_x = frm_trim0->start_x;
			if (sinfo->trim0_end_x < end)
				slc_scaler->trim0_size_x = frm_trim0->size_x;
			else
				slc_scaler->trim0_size_x = end -frm_trim0->start_x;
		} else if ((sinfo->slice_col_num - 1) == sinfo->x) {
			/* last slice */
			if (frm_trim0->start_x > start) {
				slc_scaler->trim0_start_x = frm_trim0->start_x - sinfo->start_col;
				slc_scaler->trim0_size_x = frm_trim0->size_x;
			} else {
				slc_scaler->trim0_start_x = sinfo->overlap_bad_left;
				slc_scaler->trim0_size_x = sinfo->trim0_end_x -start;
			}
		} else {
			if (frm_trim0->start_x < start) {
				slc_scaler->trim0_start_x = sinfo->overlap_bad_left;
				if (sinfo->trim0_end_x < end)
					slc_scaler->trim0_size_x = sinfo->trim0_end_x - start;
				else
					slc_scaler->trim0_size_x = end - start;
			} else {
				slc_scaler->trim0_start_x = frm_trim0->start_x - sinfo->start_col;
				if (sinfo->trim0_end_x < end)
					slc_scaler->trim0_size_x = frm_trim0->size_x;
				else
					slc_scaler->trim0_size_x = end -frm_trim0->start_x;
			}
		}
	}

	if (sinfo->slice_row_num == 1) {
		slc_scaler->trim0_start_y = frm_trim0->start_y;
		slc_scaler->trim0_size_y = frm_trim0->size_y;
	} else {
		pr_err("fail to get support vertical slices.\n");
	}
}

static void ispslice_spath_deci_info_cfg(
		struct isp_scaler_slice_tmp *sinfo,
		struct img_deci_info *frm_deci,
		struct img_trim *frm_trim0,
		struct slice_scaler_info *slc_scaler)
{
	uint32_t start;

	if (frm_deci->deci_x_eb)
		sinfo->deci_x = 1 << (frm_deci->deci_x + 1);
	else
		sinfo->deci_x = 1;

	if (frm_deci->deci_y_eb)
		sinfo->deci_y = 1 << (frm_deci->deci_y + 1);
	else
		sinfo->deci_y = 1;

	sinfo->deci_x_align = sinfo->deci_x * 2;

	start = sinfo->start_col + sinfo->overlap_bad_left;

	if ((frm_trim0->start_x >= sinfo->start_col)
		&& (frm_trim0->start_x <= (sinfo->end_col + 1))) {
		slc_scaler->trim0_size_x = slc_scaler->trim0_size_x /sinfo->deci_x_align * sinfo->deci_x_align;
	} else {
		sinfo->trim0_start_adjust_x = (start + sinfo->deci_x_align - 1) /sinfo->deci_x_align * sinfo->deci_x_align - start;
		slc_scaler->trim0_start_x += sinfo->trim0_start_adjust_x;
		slc_scaler->trim0_size_x -= sinfo->trim0_start_adjust_x;
		slc_scaler->trim0_size_x = slc_scaler->trim0_size_x /sinfo->deci_x_align * sinfo->deci_x_align;
	}

	if (slc_scaler->odata_mode == ODATA_YUV422)
		sinfo->deci_y_align = sinfo->deci_y;
	else
		sinfo->deci_y_align = sinfo->deci_y * 2;

	start = sinfo->start_row + sinfo->overlap_bad_up;

	if ((frm_trim0->start_y >= sinfo->start_row)
		&& (frm_trim0->start_y  <= (sinfo->end_row + 1))) {
		slc_scaler->trim0_size_y = slc_scaler->trim0_size_y /sinfo->deci_y_align * sinfo->deci_y_align;
	} else {
		sinfo->trim0_start_adjust_y = (start + sinfo->deci_y_align - 1) /
			sinfo->deci_y_align * sinfo->deci_y_align - start;
		slc_scaler->trim0_start_y += sinfo->trim0_start_adjust_y;
		slc_scaler->trim0_size_y -= sinfo->trim0_start_adjust_y;
		slc_scaler->trim0_size_y = slc_scaler->trim0_size_y /sinfo->deci_y_align * sinfo->deci_y_align;
	}

	slc_scaler->scaler_in_width = slc_scaler->trim0_size_x / sinfo->deci_x;
	slc_scaler->scaler_in_height = slc_scaler->trim0_size_y / sinfo->deci_y;
}

static void ispslice_spath_scaler_info_cfg(
		struct isp_scaler_slice_tmp *slice,
		struct img_trim *frm_trim0,
		struct yuv_scaler_info *in,
		struct slice_scaler_info *out)
{
	uint32_t scl_factor_in, scl_factor_out;
	uint32_t  initial_phase, last_phase, phase_in;
	uint32_t phase_tmp, scl_temp, out_tmp;
	uint32_t start, end;
	uint32_t tap_hor, tap_ver, tap_hor_uv, tap_ver_uv;
	uint32_t tmp, n;

	if (in->scaler_bypass == 0) {
		scl_factor_in = in->scaler_factor_in / 2;
		scl_factor_out = in->scaler_factor_out / 2;

		initial_phase = 0;
		last_phase = initial_phase+ scl_factor_in * (in->scaler_out_width / 2 - 1);
		tap_hor = 8;
		tap_hor_uv = tap_hor / 2;

		start = slice->start_col + slice->overlap_bad_left + slice->deci_x_align - 1;
		end = slice->end_col + 1 - slice->overlap_bad_right + slice->deci_x_align - 1;

		if (frm_trim0->start_x >= slice->start_col &&
			(frm_trim0->start_x <= slice->end_col + 1)) {
			phase_in = 0;

			if (out->scaler_in_width == frm_trim0->size_x / slice->deci_x)
				phase_tmp = last_phase;
			else
				phase_tmp = (out->scaler_in_width / 2 -tap_hor_uv / 2) * scl_factor_out -scl_factor_in / 2 - 1;
			out_tmp = (phase_tmp - phase_in) / scl_factor_in + 1;
			out->scaler_out_width = out_tmp * 2;
		} else {
			phase_in = (tap_hor_uv / 2) * scl_factor_out;

			if (slice->x == slice->slice_col_num - 1) {
				phase_tmp = last_phase -
					((frm_trim0->size_x / 2) /slice->deci_x -out->scaler_in_width /2) * scl_factor_out;
				out_tmp = (phase_tmp - phase_in) /scl_factor_in + 1;
				out->scaler_out_width = out_tmp * 2;
				phase_in = phase_tmp - (out_tmp - 1) * scl_factor_in;
			} else {
				if (slice->trim0_end_x >= slice->start_col
					&& (slice->trim0_end_x <= slice->end_col + 1 - slice->overlap_bad_right)) {
					phase_tmp = last_phase -
						((frm_trim0->size_x / 2) /slice->deci_x -out->scaler_in_width / 2) *scl_factor_out;
					out_tmp = (phase_tmp - phase_in) /scl_factor_in + 1;
					out->scaler_out_width = out_tmp * 2;
					phase_in = phase_tmp - (out_tmp - 1) *scl_factor_in;
				} else {
					initial_phase = ((((start /
					slice->deci_x_align *
						slice->deci_x_align
					- frm_trim0->start_x) / slice->deci_x) /
						2 +
					(tap_hor_uv / 2)) * (scl_factor_out) +
					(scl_factor_in - 1)) / scl_factor_in *
					scl_factor_in;

					slice->scaler_out_width_temp = ((last_phase - initial_phase) /scl_factor_in + 1) * 2;

					scl_temp = ((end / slice->deci_x_align * slice->deci_x_align -frm_trim0->start_x) /slice->deci_x) / 2;
					last_phase = ((scl_temp - tap_hor_uv / 2)*(scl_factor_out) - scl_factor_in / 2 -1) /scl_factor_in * scl_factor_in;

					out_tmp = (last_phase - initial_phase) /scl_factor_in + 1;
					out->scaler_out_width = out_tmp * 2;
					phase_in = initial_phase -
						(((start /slice->deci_x_align * slice->deci_x_align -frm_trim0->start_x) / slice->deci_x) /2) *scl_factor_out;
				}
			}
		}

		ispslice_scaler_phase_calc(phase_in * 4, scl_factor_out * 2, &out->scaler_ip_int, &out->scaler_ip_rmd);
		ispslice_scaler_phase_calc(phase_in, scl_factor_out, &out->scaler_cip_int, &out->scaler_cip_rmd);

		scl_factor_in = in->scaler_ver_factor_in;
		scl_factor_out = in->scaler_ver_factor_out;

		initial_phase = 0;

		last_phase = initial_phase + scl_factor_in * (in->scaler_out_height - 1);
		tap_ver = in->scaler_y_ver_tap > in->scaler_uv_ver_tap ? in->scaler_y_ver_tap : in->scaler_uv_ver_tap;
		tap_ver += 2;
		tap_ver_uv = tap_ver;

		start = slice->start_row + slice->overlap_bad_up + slice->deci_y_align - 1;
		end = slice->end_row + 1 - slice->overlap_bad_down + slice->deci_y_align - 1;

		if (frm_trim0->start_y >= slice->start_row
			&& (frm_trim0->start_y <= slice->end_row + 1)) {
			phase_in = 0;

			if (out->scaler_in_height == (frm_trim0->size_y / slice->deci_y))
				phase_tmp = last_phase;
			else
				phase_tmp = (out->scaler_in_height -tap_ver_uv / 2) * scl_factor_out - 1;

			out_tmp = (phase_tmp - phase_in) / scl_factor_in + 1;
			if (out_tmp % 2 == 1)
				out_tmp -= 1;
			out->scaler_out_height = out_tmp;
		} else {
			phase_in = (tap_ver_uv / 2) * scl_factor_out;
			if (slice->y == slice->slice_row_num - 1) {
				phase_tmp = last_phase-(frm_trim0->size_y / slice->deci_y -out->scaler_in_height) * scl_factor_out;

				out_tmp =(phase_tmp - phase_in) / scl_factor_in+ 1;
				if (out_tmp % 2 == 1)
					out_tmp -= 1;
				if (in->odata_mode == 1 && out_tmp % 4 != 0)
					out_tmp = out_tmp / 4 * 4;
				out->scaler_out_height = out_tmp;

				phase_in = phase_tmp - (out_tmp - 1) * scl_factor_in;
			} else {
				if (slice->trim0_end_y >= slice->start_row
					&&(slice->trim0_end_y <= slice->end_row + 1-slice->overlap_bad_down)) {
					phase_tmp = last_phase-(frm_trim0->size_y / slice->deci_y -out->scaler_in_height) * scl_factor_out;

					out_tmp = (phase_tmp - phase_in) /scl_factor_in + 1;
					if (out_tmp % 2 == 1)
						out_tmp -= 1;
					if (in->odata_mode == 1 && out_tmp % 4 != 0)
						out_tmp = out_tmp / 4 * 4;
					out->scaler_out_height = out_tmp;

					phase_in = phase_tmp - (out_tmp - 1) *scl_factor_in;
				} else {
					initial_phase = (((start /
					slice->deci_y_align *
						slice->deci_y_align
					- frm_trim0->start_y) / slice->deci_y +
					(tap_ver_uv / 2)) * (scl_factor_out) +
					(scl_factor_in - 1)) / (
						scl_factor_in * 2)
					*(scl_factor_in * 2);

					slice->scaler_out_height_temp = (last_phase - initial_phase) /scl_factor_in + 1;
					scl_temp = (end / slice->deci_y_align*slice->deci_y_align -frm_trim0->start_y) /slice->deci_y;

					last_phase = ((scl_temp - tap_ver_uv / 2)*(scl_factor_out) - 1) / scl_factor_in *scl_factor_in;

					out_tmp = (last_phase - initial_phase) /scl_factor_in + 1;
					if (out_tmp % 2 == 1)
						out_tmp -= 1;
					if (in->odata_mode == 1 && out_tmp % 4 != 0)
						out_tmp = out_tmp / 4 * 4;
					out->scaler_out_height = out_tmp;

					phase_in = initial_phase -
						(start /slice->deci_y_align *slice->deci_y_align -frm_trim0->start_y) / slice->deci_y*scl_factor_out;
				}
			}
		}

		ispslice_scaler_phase_calc(phase_in, scl_factor_out, &out->scaler_ip_int_ver, &out->scaler_ip_rmd_ver);
		if (in->odata_mode == 1) {
			phase_in /= 2;
			scl_factor_out /= 2;
		}
		ispslice_scaler_phase_calc(phase_in, scl_factor_out, &out->scaler_cip_int_ver, &out->scaler_cip_rmd_ver);

		if (out->scaler_ip_int >= 16) {
			tmp = out->scaler_ip_int;
			n = (tmp >> 3) - 1;
			out->trim0_start_x += 8 * n * slice->deci_x;
			out->trim0_size_x -= 8 * n * slice->deci_x;
			out->scaler_ip_int -= 8 * n;
			out->scaler_cip_int -= 4 * n;
		}
		if (out->scaler_ip_int >= 16)
			pr_err("fail to get horizontal slice initial phase, overflow!\n");
		if (out->scaler_ip_int_ver >= 16) {
			tmp = out->scaler_ip_int_ver;
			n = (tmp >> 3) - 1;
			out->trim0_start_y += 8 * n * slice->deci_y;
			out->trim0_size_y -= 8 * n * slice->deci_y;
			out->scaler_ip_int_ver -= 8 * n;
			out->scaler_cip_int_ver -= 8 * n;
		}
		if (out->scaler_ip_int_ver >= 16)
			pr_err("fail to get vertical slice initial phase, overflow!\n");
	} else {
		out->scaler_out_width = out->scaler_in_width;
		out->scaler_out_height = out->scaler_in_height;

		start = slice->start_col + slice->overlap_bad_left + slice->trim0_start_adjust_x + slice->deci_x_align - 1;
		slice->scaler_out_width_temp = (frm_trim0->size_x - (start /slice->deci_x_align * slice->deci_x_align -frm_trim0->start_x))
			/ slice->deci_x;
		start = slice->start_row + slice->overlap_bad_up + slice->trim0_start_adjust_y + slice->deci_y_align - 1;
		slice->scaler_out_height_temp = (frm_trim0->size_y - (start /slice->deci_y_align * slice->deci_y_align -frm_trim0->start_y))
			/ slice->deci_y;
	}
}

static void ispslice_spath_trim1_info_cfg(
		struct isp_scaler_slice_tmp *slice,
		struct img_trim *frm_trim0,
		struct yuv_scaler_info *in,
		struct slice_scaler_info *out)
{
	uint32_t trim_sum_x = slice->trim1_sum_x;
	uint32_t trim_sum_y = slice->trim1_sum_y;
	uint32_t pix_align = 8;

	if ((frm_trim0->start_x >= slice->start_col) &&
		(frm_trim0->start_x <= slice->end_col + 1)) {
		out->trim1_start_x = 0;
		if (out->scaler_in_width == frm_trim0->size_x)
			out->trim1_size_x = out->scaler_out_width;
		else
			out->trim1_size_x = out->scaler_out_width & ~(pix_align - 1);
	} else {
		if (slice->x == slice->slice_col_num - 1) {
			out->trim1_size_x = in->scaler_out_width - trim_sum_x;
			out->trim1_start_x = out->scaler_out_width - out->trim1_size_x;
		} else {
			if ((slice->trim0_end_x >= slice->start_col)
				&& (slice->trim0_end_x <= slice->end_col + 1 - slice->overlap_bad_right)) {
				out->trim1_size_x = in->scaler_out_width - trim_sum_x;
				out->trim1_start_x = out->scaler_out_width -out->trim1_size_x;
			} else {
				out->trim1_start_x = slice->scaler_out_width_temp -(in->scaler_out_width - trim_sum_x);
				out->trim1_size_x = (out->scaler_out_width -out->trim1_start_x) & ~(pix_align - 1);
			}
		}
	}

	if ((frm_trim0->start_y >= slice->start_row)
		&& (frm_trim0->start_y <= slice->end_row + 1)) {
		out->trim1_start_y = 0;
		if (out->scaler_in_height == frm_trim0->size_y)
			out->trim1_size_y = out->scaler_out_height;
		else
			out->trim1_size_y = out->scaler_out_height & ~(pix_align - 1);
	} else {
		if (slice->y == slice->slice_row_num - 1) {
			out->trim1_size_y = in->scaler_out_height - trim_sum_y;
			out->trim1_start_y = out->scaler_out_height -out->trim1_size_y;
		} else {
			if (slice->trim0_end_y >= slice->start_row
				&& (slice->trim0_end_y <= slice->end_row + 1 - slice->overlap_bad_down)) {
				out->trim1_size_y = in->scaler_out_height - trim_sum_y;
				out->trim1_start_y = out->scaler_out_height -out->trim1_size_y;
			} else {
				out->trim1_start_y = slice->scaler_out_height_temp -(in->scaler_out_height - trim_sum_y);
				out->trim1_size_y = (out->scaler_out_height -out->trim1_start_y) & ~(pix_align - 1);
			}
		}
	}
}

static void ispslice_slice_size_info_get(
			struct slice_cfg_input *in_ptr,
			uint32_t *w, uint32_t *h)
{
	uint32_t j;
	uint32_t slice_num, slice_w, slice_w_out;
	uint32_t slice_max_w, max_w;
	uint32_t linebuf_len;
	struct img_size *input = &in_ptr->frame_in_size;
	struct img_size *output;

	/* based input */
	max_w = input->w;
	slice_num = 1;
	linebuf_len = g_camctrl.isp_linebuf_len;
	slice_max_w = linebuf_len - SLICE_OVERLAP_W_MAX;
	if (max_w <= linebuf_len) {
		slice_w = input->w;
	} else {
		do {
			slice_num++;
			slice_w = (max_w + slice_num - 1) / slice_num;
		} while (slice_w >= slice_max_w);
	}
	pr_debug("input_w %d, slice_num %d, slice_w %d\n", max_w, slice_num, slice_w);

	/* based output */
	max_w = 0;
	slice_num = 1;
	slice_max_w = linebuf_len;
	for (j = 0; j < ISP_SPATH_NUM; j++) {
		output = in_ptr->frame_out_size[j];
		if (output && (output->w > max_w))
			max_w = output->w;
	}
	if (max_w > 0) {
		if (max_w > linebuf_len) {
			do {
				slice_num++;
				slice_w_out = (max_w + slice_num - 1) / slice_num;
			} while (slice_w_out >= slice_max_w);
		}
		/* set to equivalent input size, because slice size based on input. */
		slice_w_out = (input->w + slice_num - 1) / slice_num;
	} else
		slice_w_out = slice_w;
	pr_debug("max output w %d, slice_num %d, out limited slice_w %d\n",
		max_w, slice_num, slice_w_out);

	*w = (slice_w < slice_w_out) ? slice_w : slice_w_out;
	*h = input->h / SLICE_H_NUM_MAX;

	*w = (*w + ISP_SLICE_ALIGN_SIZE -1) & ~(ISP_SLICE_ALIGN_SIZE -1);
	*h = (*h + ISP_SLICE_ALIGN_SIZE -1) & ~(ISP_SLICE_ALIGN_SIZE -1);
}

static int ispslice_slice_overlap_info_get(
			struct slice_cfg_input *in_ptr,
			struct isp_slice_context *slc_ctx)
{
	switch (in_ptr->frame_fetch->fetch_fmt) {
	case ISP_FETCH_RAW10:
	case ISP_FETCH_CSI2_RAW10:
		slc_ctx->overlap_up = RAW_OVERLAP_UP;
		slc_ctx->overlap_down = RAW_OVERLAP_DOWN;
		slc_ctx->overlap_left = RAW_OVERLAP_LEFT;
		slc_ctx->overlap_right = RAW_OVERLAP_RIGHT;
		break;
	default:
		slc_ctx->overlap_up = YUV_OVERLAP_UP;
		slc_ctx->overlap_down = YUV_OVERLAP_DOWN;
		slc_ctx->overlap_left = YUV_OVERLAP_LEFT;
		slc_ctx->overlap_right = YUV_OVERLAP_RIGHT;
		break;
	}

	return 0;
}

static int ispslice_slice_base_info_cfg(
			struct slice_cfg_input *in_ptr,
			struct isp_slice_context *slc_ctx)
{
	int rtn = 0;
	uint32_t i = 0, j = 0;
	uint32_t img_height, img_width;
	uint32_t slice_height = 0, slice_width = 0;
	uint32_t slice_total_row, slice_total_col, slice_num;
	uint32_t fetch_start_x, fetch_start_y;
	uint32_t fetch_end_x, fetch_end_y;
	struct isp_slice_desc *cur_slc = NULL;
	struct isp_hw_fetch_info *frame_fetch = in_ptr->frame_fetch;
	struct isp_fbd_raw_info *frame_fbd_raw = in_ptr->frame_fbd_raw;
	struct isp_fbd_yuv_info *frame_fbd_yuv = in_ptr->frame_fbd_yuv;

	ispslice_slice_size_info_get(in_ptr, &slice_width, &slice_height);
	if (slice_width == 0 || slice_height == 0)
		return -EINVAL;

	rtn = ispslice_slice_overlap_info_get(in_ptr, slc_ctx);

	img_height = in_ptr->frame_in_size.h;
	img_width = in_ptr->frame_in_size.w;

	fetch_start_x = frame_fetch->in_trim.start_x;
	fetch_start_y = frame_fetch->in_trim.start_y;
	fetch_end_x = frame_fetch->in_trim.start_x + frame_fetch->in_trim.size_x - 1;
	fetch_end_y = frame_fetch->in_trim.start_y + frame_fetch->in_trim.size_y - 1;

	if (!frame_fbd_raw->fetch_fbd_bypass) {
		fetch_start_x = frame_fbd_raw->trim.start_x;
		fetch_start_y = frame_fbd_raw->trim.start_y;
		fetch_end_x = frame_fbd_raw->trim.start_x + frame_fbd_raw->trim.size_x - 1;
		fetch_end_y = frame_fbd_raw->trim.start_y + frame_fbd_raw->trim.size_y - 1;
	}
	if (!frame_fbd_yuv->fetch_fbd_bypass) {
		fetch_start_x = frame_fbd_yuv->trim.start_x;
		fetch_start_y = frame_fbd_yuv->trim.start_y;
		fetch_end_x = frame_fbd_yuv->trim.start_x + frame_fbd_yuv->trim.size_x - 1;
		fetch_end_y = frame_fbd_yuv->trim.start_y + frame_fbd_yuv->trim.size_y - 1;
	}

	slice_total_row = (img_height + slice_height - 1) / slice_height;
	slice_total_col = (img_width + slice_width - 1) / slice_width;
	slice_num = slice_total_col * slice_total_row;

	slc_ctx->slice_num = slice_num;
	slc_ctx->slice_col_num = slice_total_col;
	slc_ctx->slice_row_num = slice_total_row;
	slc_ctx->slice_height = slice_height;
	slc_ctx->slice_width = slice_width;
	slc_ctx->img_height = img_height;
	slc_ctx->img_width = img_width;
	pr_debug("img w %d, h %d, slice w %d, h %d, slice num %d\n",
		img_width, img_height,
		slice_width, slice_height, slice_num);

	if (!frame_fbd_raw->fetch_fbd_bypass)
		pr_debug("src %d %d, fbd_raw crop %d %d %d %d\n",
			frame_fbd_raw->width, frame_fbd_raw->height,
			fetch_start_x, fetch_start_y, fetch_end_x, fetch_end_y);
	else if (!frame_fbd_yuv->fetch_fbd_bypass)
		pr_debug("src %d %d, fbd_raw crop %d %d %d %d\n",
			frame_fbd_yuv->slice_size.w, frame_fbd_yuv->slice_size.h,
			fetch_start_x, fetch_start_y, fetch_end_x, fetch_end_y);
	else
		pr_debug("src %d %d, fetch crop %d %d %d %d\n",
			frame_fetch->src.w, frame_fetch->src.h,
			fetch_start_x, fetch_start_y, fetch_end_x, fetch_end_y);

	for (i = 0; i < SLICE_NUM_MAX; i++)
		pr_debug("slice %d valid %d\n", i, slc_ctx->slices[i].valid);

	cur_slc = &slc_ctx->slices[0];
	for (i = 0; i < slice_total_row; i++) {
		for (j = 0; j < slice_total_col; j++) {
			uint32_t start_col;
			uint32_t start_row;
			uint32_t end_col;
			uint32_t end_row;

			cur_slc->valid = 1;
			cur_slc->y = i;
			cur_slc->x = j;

			start_col = j * slice_width;
			start_row = i * slice_height;
			end_col = start_col + slice_width - 1;
			end_row = start_row + slice_height - 1;

			if (i != 0)
				cur_slc->slice_overlap.overlap_up = slc_ctx->overlap_up;
			if (j != 0)
				cur_slc->slice_overlap.overlap_left = slc_ctx->overlap_left;

			if (i != (slice_total_row - 1))
				cur_slc->slice_overlap.overlap_down = slc_ctx->overlap_down;
			else
				end_row = img_height - 1;

			if (j != (slice_total_col - 1))
				cur_slc->slice_overlap.overlap_right = slc_ctx->overlap_right;
			else
				end_col = img_width - 1;

			cur_slc->slice_pos_orig.start_col = start_col;
			cur_slc->slice_pos_orig.start_row = start_row;
			cur_slc->slice_pos_orig.end_col = end_col;
			cur_slc->slice_pos_orig.end_row = end_row;

			cur_slc->slice_pos.start_col = start_col - cur_slc->slice_overlap.overlap_left;
			cur_slc->slice_pos.start_row = start_row - cur_slc->slice_overlap.overlap_up;
			cur_slc->slice_pos.end_col = end_col + cur_slc->slice_overlap.overlap_right;
			cur_slc->slice_pos.end_row = end_row + cur_slc->slice_overlap.overlap_down;

			cur_slc->slice_pos_fetch.start_col = cur_slc->slice_pos.start_col + fetch_start_x;
			cur_slc->slice_pos_fetch.start_row = cur_slc->slice_pos.start_row + fetch_start_y;
			cur_slc->slice_pos_fetch.end_col = cur_slc->slice_pos.end_col + fetch_start_x;
			cur_slc->slice_pos_fetch.end_row = cur_slc->slice_pos.end_row + fetch_start_y;

			pr_debug("slice %d %d pos_orig [%d %d %d %d]\n", i, j,
					cur_slc->slice_pos_orig.start_col,
					cur_slc->slice_pos_orig.end_col,
					cur_slc->slice_pos_orig.start_row,
					cur_slc->slice_pos_orig.end_row);
			pr_debug("slice %d %d pos [%d %d %d %d]\n", i, j,
					cur_slc->slice_pos.start_col,
					cur_slc->slice_pos.end_col,
					cur_slc->slice_pos.start_row,
					cur_slc->slice_pos.end_row);
			pr_debug("slice %d %d pos_fetch [%d %d %d %d]\n", i, j,
					cur_slc->slice_pos_fetch.start_col,
					cur_slc->slice_pos_fetch.end_col,
					cur_slc->slice_pos_fetch.start_row,
					cur_slc->slice_pos_fetch.end_row);
			pr_debug("slice %d %d ovl [%d %d %d %d]\n", i, j,
					cur_slc->slice_overlap.overlap_up,
					cur_slc->slice_overlap.overlap_down,
					cur_slc->slice_overlap.overlap_left,
					cur_slc->slice_overlap.overlap_right);

			cur_slc->slice_fbd_raw.fetch_fbd_bypass = frame_fbd_raw->fetch_fbd_bypass;
			cur_slc->slice_fbd_yuv.fetch_fbd_bypass = frame_fbd_yuv->fetch_fbd_bypass;

			cur_slc++;
		}
	}

	return rtn;
}

static int ispslice_slice_base_info_cfg_ex(struct slice_cfg_input *in_ptr,
		struct isp_slice_context*slice_ctx)
{
	int rtn = 0;
	uint32_t i = 0, j = 0;
	uint32_t img_height, img_width;
	uint32_t slice_height = 0, slice_width = 0;
	uint32_t slice_total_row, slice_total_col, slice_num;
	uint32_t fetch_start_x, fetch_start_y;
	uint32_t fetch_end_x, fetch_end_y;
	struct isp_slice_desc *cur_slc = NULL;
	struct isp_hw_fetch_info *frame_fetch = NULL;
	struct isp_fbd_raw_info *frame_fbd_raw = NULL;
	struct isp_fbd_yuv_info *frame_fbd_yuv = NULL;
	struct isp_slice_context *slc_ctx = slice_ctx;

	if (!in_ptr || !slice_ctx) {
		pr_err("fail to get input ptr\n");
		return -1;
	}

	frame_fetch = in_ptr->frame_fetch;
	frame_fbd_raw = in_ptr->frame_fbd_raw;
	frame_fbd_yuv = in_ptr->frame_fbd_yuv;
	ispslice_slice_size_info_get(in_ptr, &slice_width, &slice_height);
	if (slice_width == 0 || slice_height == 0) {
		pr_err("fail to get slice info w %d, h%\n", slice_width, slice_height);
		return -EINVAL;
	}

	img_height = in_ptr->frame_in_size.h;
	img_width = in_ptr->frame_in_size.w;

	fetch_start_x = frame_fetch->in_trim.start_x;
	fetch_start_y = frame_fetch->in_trim.start_y;
	fetch_end_x = frame_fetch->in_trim.start_x + frame_fetch->in_trim.size_x - 1;
	fetch_end_y = frame_fetch->in_trim.start_y + frame_fetch->in_trim.size_y - 1;

	if (!frame_fbd_raw->fetch_fbd_bypass) {
		fetch_start_x = frame_fbd_raw->trim.start_x;
		fetch_start_y = frame_fbd_raw->trim.start_y;
		fetch_end_x = frame_fbd_raw->trim.start_x + frame_fbd_raw->trim.size_x - 1;
		fetch_end_y = frame_fbd_raw->trim.start_y + frame_fbd_raw->trim.size_y - 1;
	}

	if (!frame_fbd_yuv->fetch_fbd_bypass) {
		fetch_start_x = frame_fbd_yuv->trim.start_x;
		fetch_start_y = frame_fbd_yuv->trim.start_y;
		fetch_end_x = frame_fbd_yuv->trim.start_x + frame_fbd_yuv->trim.size_x - 1;
		fetch_end_y = frame_fbd_yuv->trim.start_y + frame_fbd_yuv->trim.size_y - 1;
	}

	slice_total_row = (img_height + slice_height - 1) / slice_height;
	slice_total_col = (img_width + slice_width - 1) / slice_width;
	slice_num = slice_total_col * slice_total_row;

	slc_ctx->slice_num = slice_num;
	slc_ctx->slice_col_num = slice_total_col;
	slc_ctx->slice_row_num = slice_total_row;
	slc_ctx->slice_height = slice_height;
	slc_ctx->slice_width = slice_width;

	slc_ctx->img_height = img_height;
	slc_ctx->img_width = img_width;

	pr_debug("slice(num %d, col %d, row %d, w %d, h %d); img (w %d, h %d)\n",
		slc_ctx->slice_num, slc_ctx->slice_col_num, slc_ctx->slice_row_num,
		slc_ctx->slice_width, slc_ctx->slice_height, slc_ctx->img_width, slc_ctx->img_height);

	rtn = alg_isp_get_dynamic_overlap(in_ptr, slc_ctx);
	if (rtn) {
		pr_err("fail to get dynamic overlap\n");
		return -1;
	}

	cur_slc = &slc_ctx->slices[0];
	for (i = 0; i < slice_total_row; i++) {
		for (j = 0; j < slice_total_col; j++) {
			uint32_t start_col;
			uint32_t start_row;
			uint32_t end_col;
			uint32_t end_row;

			cur_slc->valid = 1;
			cur_slc->y = i;
			cur_slc->x = j;

			start_col = j * slice_width;
			start_row = i * slice_height;
			if (j == (slice_total_col -1))
				end_col = img_width -1;
			else
				end_col = start_col + slice_width - 1;

			if (i == (slice_total_row-1))
				end_row = img_height -1;
			else
				end_row = start_row + slice_height - 1;

			cur_slc->slice_pos_orig.start_col = start_col;
			cur_slc->slice_pos_orig.start_row = start_row;
			cur_slc->slice_pos_orig.end_col = end_col;
			cur_slc->slice_pos_orig.end_row = end_row;

			cur_slc->slice_overlap.overlap_left = slc_ctx->overlapParam.slice_overlap[j].ov_left;
			cur_slc->slice_overlap.overlap_right = slc_ctx->overlapParam.slice_overlap[j].ov_right;
			cur_slc->slice_overlap.overlap_up = slc_ctx->overlapParam.slice_overlap[j].ov_up;
			cur_slc->slice_overlap.overlap_down = slc_ctx->overlapParam.slice_overlap[j].ov_down;

			cur_slc->slice_pos.start_col = start_col - cur_slc->slice_overlap.overlap_left;
			cur_slc->slice_pos.start_row = start_row - cur_slc->slice_overlap.overlap_up;
			cur_slc->slice_pos.end_col = end_col + cur_slc->slice_overlap.overlap_right;
			cur_slc->slice_pos.end_row = end_row + cur_slc->slice_overlap.overlap_down;

			cur_slc->slice_pos_fetch.start_col = cur_slc->slice_pos.start_col + fetch_start_x;
			cur_slc->slice_pos_fetch.start_row = cur_slc->slice_pos.start_row + fetch_start_y;
			cur_slc->slice_pos_fetch.end_col = cur_slc->slice_pos.end_col + fetch_start_x;
			cur_slc->slice_pos_fetch.end_row = cur_slc->slice_pos.end_row + fetch_start_y;

			pr_debug("slice %d %d pos_orig [start x %d y %d end x %d y %d]\n", i, j,
				cur_slc->slice_pos_orig.start_col,
				cur_slc->slice_pos_orig.start_row,
				cur_slc->slice_pos_orig.end_col,
				cur_slc->slice_pos_orig.end_row);
			pr_debug("slice %d %d pos+ov [start x %d y %d end x %d y %d]\n", i, j,
				cur_slc->slice_pos.start_col,
				cur_slc->slice_pos.start_row,
				cur_slc->slice_pos.end_col,
				cur_slc->slice_pos.end_row);
			pr_debug("slice %d %d pos_fetch [start x %d y %d end x %d y %d]\n", i, j,
				cur_slc->slice_pos_fetch.start_col,
				cur_slc->slice_pos_fetch.start_row,
				cur_slc->slice_pos_fetch.end_col,
				cur_slc->slice_pos_fetch.end_row);
			pr_debug("slice %d %d overlap [left %d right %d up %d down %d]\n", i, j,
				cur_slc->slice_overlap.overlap_left,
				cur_slc->slice_overlap.overlap_right,
				cur_slc->slice_overlap.overlap_up,
				cur_slc->slice_overlap.overlap_down);

			cur_slc->slice_fbd_raw.fetch_fbd_bypass = frame_fbd_raw->fetch_fbd_bypass;
			cur_slc->slice_fbd_yuv.fetch_fbd_bypass = frame_fbd_yuv->fetch_fbd_bypass;
			cur_slc++;
		}
	}

	return rtn;
}

static int ispslice_base_info_calc_cfg(struct slice_cfg_input *in_ptr,
		struct isp_slice_context*slice_ctx)
{
	int rtn = 0;
	uint32_t i = 0, j = 0;
	uint32_t img_height, img_width;
	uint32_t slice_height = 0, slice_width = 0;
	uint32_t slice_total_row, slice_total_col, slice_num;
	uint32_t fetch_start_x, fetch_start_y;
	uint32_t fetch_end_x, fetch_end_y;
	struct isp_slice_desc *cur_slc = NULL;
	struct isp_hw_fetch_info *frame_fetch = NULL;
	struct isp_fbd_raw_info *frame_fbd_raw = NULL;
	struct isp_fbd_yuv_info *frame_fbd_yuv = NULL;
	struct isp_slice_context *slc_ctx = slice_ctx;

	if (!in_ptr || !slice_ctx) {
		pr_err("fail to get input ptr\n");
		return -1;
	}

	frame_fetch = in_ptr->frame_fetch;
	frame_fbd_raw = in_ptr->frame_fbd_raw;
	frame_fbd_yuv = in_ptr->frame_fbd_yuv;
	ispslice_slice_size_info_get(in_ptr, &slice_width, &slice_height);
	if (slice_width == 0 || slice_height == 0) {
		pr_err("fail to get slice info w %d, h %d\n", slice_width, slice_height);
		return -EINVAL;
	}

	img_height = in_ptr->frame_in_size.h;
	img_width = in_ptr->frame_in_size.w;

	fetch_start_x = frame_fetch->in_trim.start_x;
	fetch_start_y = frame_fetch->in_trim.start_y;
	fetch_end_x = frame_fetch->in_trim.start_x + frame_fetch->in_trim.size_x - 1;
	fetch_end_y = frame_fetch->in_trim.start_y + frame_fetch->in_trim.size_y - 1;

	/* TBD: fbd raw need delete */
	if (!frame_fbd_raw->fetch_fbd_bypass) {
		fetch_start_x = frame_fbd_raw->trim.start_x;
		fetch_start_y = frame_fbd_raw->trim.start_y;
		fetch_end_x = frame_fbd_raw->trim.start_x + frame_fbd_raw->trim.size_x - 1;
		fetch_end_y = frame_fbd_raw->trim.start_y + frame_fbd_raw->trim.size_y - 1;
	}

	if (!frame_fbd_yuv->fetch_fbd_bypass) {
		fetch_start_x = frame_fbd_yuv->trim.start_x;
		fetch_start_y = frame_fbd_yuv->trim.start_y;
		fetch_end_x = frame_fbd_yuv->trim.start_x + frame_fbd_yuv->trim.size_x - 1;
		fetch_end_y = frame_fbd_yuv->trim.start_y + frame_fbd_yuv->trim.size_y - 1;
	}

	slice_total_row = (img_height + slice_height - 1) / slice_height;
	slice_total_col = (img_width + slice_width - 1) / slice_width;
	slice_num = slice_total_col * slice_total_row;

	slc_ctx->slice_num = slice_num;
	slc_ctx->slice_col_num = slice_total_col;
	slc_ctx->slice_row_num = slice_total_row;
	slc_ctx->slice_height = slice_height;
	slc_ctx->slice_width = slice_width;
	slc_ctx->img_height = img_height;
	slc_ctx->img_width = img_width;

	pr_debug("slice(num %d, col %d, row %d, w %d, h %d); img (w %d, h %d)\n",
		slc_ctx->slice_num, slc_ctx->slice_col_num, slc_ctx->slice_row_num,
		slc_ctx->slice_width, slc_ctx->slice_height, slc_ctx->img_width, slc_ctx->img_height);

	rtn = alg_isp_get_dynamic_overlap(in_ptr, slc_ctx);
	if (rtn) {
		pr_err("fail to get dynamic overlap\n");
		return -1;
	}

	cur_slc = &slc_ctx->slices[0];
	for (i = 0; i < slice_total_row; i++) {
		for (j = 0; j < slice_total_col; j++) {
			uint32_t start_col;
			uint32_t start_row;
			uint32_t end_col;
			uint32_t end_row;

			cur_slc->valid = 1;
			cur_slc->y = i;
			cur_slc->x = j;

			start_col = j * slice_width;
			start_row = i * slice_height;
			if (j == (slice_total_col -1))
				end_col = img_width -1;
			else
				end_col = start_col + slice_width - 1;

			if (i == (slice_total_row-1))
				end_row = img_height -1;
			else
				end_row = start_row + slice_height - 1;

			cur_slc->slice_pos_orig.start_col = start_col;
			cur_slc->slice_pos_orig.start_row = start_row;
			cur_slc->slice_pos_orig.end_col = end_col;
			cur_slc->slice_pos_orig.end_row = end_row;

			cur_slc->slice_overlap.overlap_left = slc_ctx->slice_overlap.fecth0_slice_overlap[0][j].ov_left;
			cur_slc->slice_overlap.overlap_right = slc_ctx->slice_overlap.fecth0_slice_overlap[0][j].ov_right;
			cur_slc->slice_overlap.overlap_up = slc_ctx->slice_overlap.fecth0_slice_overlap[0][j].ov_up;
			cur_slc->slice_overlap.overlap_down = slc_ctx->slice_overlap.fecth0_slice_overlap[0][j].ov_down;

			cur_slc->slice_pos.start_col = slc_ctx->slice_overlap.fecth0_slice_region[0][j].sx;
			cur_slc->slice_pos.start_row = slc_ctx->slice_overlap.fecth0_slice_region[0][j].sy;
			cur_slc->slice_pos.end_col = slc_ctx->slice_overlap.fecth0_slice_region[0][j].ex;
			cur_slc->slice_pos.end_row = slc_ctx->slice_overlap.fecth0_slice_region[0][j].ey;

			cur_slc->slice_pos_fetch.start_col = cur_slc->slice_pos.start_col + fetch_start_x;
			cur_slc->slice_pos_fetch.start_row = cur_slc->slice_pos.start_row + fetch_start_y;
			cur_slc->slice_pos_fetch.end_col = cur_slc->slice_pos.end_col + fetch_start_x;
			cur_slc->slice_pos_fetch.end_row = cur_slc->slice_pos.end_row + fetch_start_y;

			cur_slc->slice_pos_fbd.start_col = slc_ctx->slice_overlap.fecth1_slice_region[0][j].sx + fetch_start_x;
			cur_slc->slice_pos_fbd.start_row = slc_ctx->slice_overlap.fecth1_slice_region[0][j].sy + fetch_start_y;
			cur_slc->slice_pos_fbd.end_col = slc_ctx->slice_overlap.fecth1_slice_region[0][j].ex + fetch_start_x;
			cur_slc->slice_pos_fbd.end_row = slc_ctx->slice_overlap.fecth1_slice_region[0][j].ey + fetch_start_y;

			pr_debug("slice %d %d pos_orig [start x %d y %d end x %d y %d]\n", i, j,
				cur_slc->slice_pos_orig.start_col,
				cur_slc->slice_pos_orig.start_row,
				cur_slc->slice_pos_orig.end_col,
				cur_slc->slice_pos_orig.end_row);
			pr_debug("slice %d %d pos+ov [start x %d y %d end x %d y %d]\n", i, j,
				cur_slc->slice_pos.start_col,
				cur_slc->slice_pos.start_row,
				cur_slc->slice_pos.end_col,
				cur_slc->slice_pos.end_row);
			pr_debug("slice %d %d pos_fetch [start x %d y %d end x %d y %d]\n", i, j,
				cur_slc->slice_pos_fetch.start_col,
				cur_slc->slice_pos_fetch.start_row,
				cur_slc->slice_pos_fetch.end_col,
				cur_slc->slice_pos_fetch.end_row);
			pr_debug("slice %d %d overlap [left %d right %d up %d down %d]\n", i, j,
				cur_slc->slice_overlap.overlap_left,
				cur_slc->slice_overlap.overlap_right,
				cur_slc->slice_overlap.overlap_up,
				cur_slc->slice_overlap.overlap_down);

			pr_debug("slice %d %d pos_fbd [start x %d y %d end x %d y %d]\n", i, j,
				cur_slc->slice_pos_fbd.start_col,
				cur_slc->slice_pos_fbd.start_row,
				cur_slc->slice_pos_fbd.end_col,
				cur_slc->slice_pos_fbd.end_row);

			cur_slc->slice_fbd_raw.fetch_fbd_bypass = frame_fbd_raw->fetch_fbd_bypass;
			cur_slc->slice_fbd_yuv.fetch_fbd_bypass = frame_fbd_yuv->fetch_fbd_bypass;
			cur_slc++;
		}
	}

	return rtn;
}

static int ispslice_slice_nr_info_cfg(
		struct slice_cfg_input *in_ptr,
		struct isp_slice_context *slc_ctx)
{
	int i;
	uint32_t start_col, start_row;
	uint32_t end_col, end_row;
	struct isp_slice_desc *cur_slc;

	cur_slc = &slc_ctx->slices[0];
	for (i = 0; i < SLICE_NUM_MAX; i++, cur_slc++) {
		if (cur_slc->valid == 0)
			continue;

		start_col = cur_slc->slice_pos.start_col;
		start_row = cur_slc->slice_pos.start_row;
		end_col = cur_slc->slice_pos.end_col;
		end_row = cur_slc->slice_pos.end_row;

		/* NLM */
		cur_slc->slice_nlm.center_x_relative = in_ptr->nlm_center_x - start_col;
		cur_slc->slice_nlm.center_y_relative = in_ptr->nlm_center_y - start_row;

		/* Post CDN */
		cur_slc->slice_postcdn.start_row_mod4 = cur_slc->slice_pos.start_row & 0x3;

		/* YNR */
		cur_slc->slice_ynr.center_offset_x = (int32_t)in_ptr->ynr_center_x - (int32_t)start_col;
		cur_slc->slice_ynr.center_offset_y = (int32_t)in_ptr->ynr_center_y - (int32_t)start_row;
		cur_slc->slice_ynr.slice_width = end_col - start_col + 1;
		cur_slc->slice_ynr.slice_height = end_row - start_row + 1;

		/* Post CNR */
		cur_slc->slice_postcnr.st_x = start_col;
		cur_slc->slice_postcnr.st_y = start_row;

		/* EE */
		cur_slc->slice_edge.radial_1D_global_start_x = start_col;
		cur_slc->slice_edge.radial_1D_global_start_y = start_row;

		pr_debug("slice %d,  (%d %d %d %d),  ynr_off %d %d, size %d %d\n",
			i, start_row, start_col, end_row, end_col,
			cur_slc->slice_ynr.center_offset_x,
			cur_slc->slice_ynr.center_offset_y,
			cur_slc->slice_ynr.slice_width,
			cur_slc->slice_ynr.slice_height);
	}

	return 0;
}

static int ispslice_slice_thumbscaler_cfg(
		struct isp_slice_desc *cur_slc,
		struct img_trim *frm_trim0,
		struct isp_hw_thumbscaler_info *scalerFrame,
		struct slice_thumbscaler_info *scalerSlice)
{
	int ret = 0;
	uint32_t half;
	uint32_t deci_w, deci_h, trim_w, trim_h;
	uint32_t frm_start_col, frm_end_col;
	uint32_t frm_start_row, frm_end_row;
	uint32_t slc_start_col, slc_end_col;
	uint32_t slc_start_row, slc_end_row;
	struct img_size src;

	scalerSlice->scaler_bypass = scalerFrame->scaler_bypass;
	scalerSlice->odata_mode = scalerFrame->odata_mode;
	scalerSlice->out_of_range = 0;
	frm_start_col = frm_trim0->start_x;
	frm_end_col = frm_trim0->size_x + frm_trim0->start_x - 1;
	frm_start_row = frm_trim0->start_y;
	frm_end_row = frm_trim0->size_y + frm_trim0->start_y - 1;
	if ((cur_slc->slice_pos_orig.end_col < frm_start_col) ||
		(cur_slc->slice_pos_orig.start_col > frm_end_col)) {
		scalerSlice->out_of_range = 1;
		cur_slc->path_en[ISP_SPATH_FD] = 0;
		return 0;
	}
	deci_w = scalerFrame->y_deci.deci_x;
	deci_h = scalerFrame->y_deci.deci_y;
	if (scalerFrame->y_deci.deci_x_eb)
		deci_w = 1 << (deci_w + 1);
	else
		deci_w = 1;

	if (scalerFrame->y_deci.deci_y_eb)
		deci_h = 1 << (deci_h + 1);
	else
		deci_h = 1;

	src.w = cur_slc->slice_pos.end_col - cur_slc->slice_pos.start_col + 1;
	src.h = cur_slc->slice_pos.end_row - cur_slc->slice_pos.start_row + 1;
	scalerSlice->src0 = src;

	slc_start_col = MAX(cur_slc->slice_pos_orig.start_col, frm_start_col);
	slc_start_row = MAX(cur_slc->slice_pos_orig.start_row, frm_start_row);
	slc_end_col = MIN(cur_slc->slice_pos_orig.end_col, frm_end_col);
	slc_end_row = MIN(cur_slc->slice_pos_orig.end_row, frm_end_row);
	trim_w = slc_end_col - slc_start_col + 1;
	trim_h = slc_end_row - slc_start_row + 1;
	scalerSlice->y_trim.start_x = slc_start_col -
		cur_slc->slice_pos.start_col;
	scalerSlice->y_trim.start_y = slc_start_row -
		cur_slc->slice_pos.start_row;
	scalerSlice->y_trim.size_x = trim_w;
	scalerSlice->y_trim.size_y = trim_h;
	scalerSlice->uv_trim.start_x = scalerSlice->y_trim.start_x / 2;
	scalerSlice->uv_trim.start_y = scalerSlice->y_trim.start_y;
	scalerSlice->uv_trim.size_x = trim_w / 2;
	scalerSlice->uv_trim.size_y = trim_h;

	scalerSlice->y_factor_in.w = trim_w / deci_w;
	scalerSlice->y_factor_in.h = trim_h / deci_h;

	half = (scalerFrame->y_factor_out.w + 1) / 2;
	scalerSlice->y_factor_out.w =
		(scalerSlice->y_factor_in.w * scalerFrame->y_factor_out.w +
			half)
				/ scalerFrame->y_factor_in.w;

	half = (scalerFrame->y_factor_out.h + 1) / 2;
	scalerSlice->y_factor_out.h =
		(scalerSlice->y_factor_in.h * scalerFrame->y_factor_out.h +
			half)
				/ scalerFrame->y_factor_in.h;

	scalerSlice->y_src_after_deci = scalerSlice->y_factor_in;
	scalerSlice->y_dst_after_scaler = scalerSlice->y_factor_out;

	scalerSlice->uv_factor_in.w = trim_w / deci_w / 2;
	scalerSlice->uv_factor_in.h = trim_h / deci_h;

	half = (scalerFrame->uv_factor_out.w + 1) / 2;
	scalerSlice->uv_factor_out.w =
		(scalerSlice->uv_factor_in.w * scalerFrame->uv_factor_out.w +
			half)
				/ scalerFrame->uv_factor_in.w;

	half = (scalerFrame->uv_factor_out.h + 1) / 2;
	scalerSlice->uv_factor_out.h =
		(scalerSlice->uv_factor_in.h * scalerFrame->uv_factor_out.h +
			half)
				/ scalerFrame->uv_factor_in.h;

	scalerSlice->uv_src_after_deci = scalerSlice->uv_factor_in;
	scalerSlice->uv_dst_after_scaler = scalerSlice->uv_factor_out;

	pr_debug("-------------slice (%d %d),  src (%d %d)-------------\n",
		cur_slc->x, cur_slc->y,
		scalerSlice->src0.w, scalerSlice->src0.h);

	pr_debug("Y: (%d %d), (%d %d), (%d %d %d %d), (%d %d)\n",
		scalerSlice->y_factor_in.w, scalerSlice->y_factor_in.h,
		scalerSlice->y_factor_out.w, scalerSlice->y_factor_out.h,
		scalerSlice->y_trim.start_x, scalerSlice->y_trim.start_y,
		scalerSlice->y_trim.size_x, scalerSlice->y_trim.size_y,
		scalerSlice->y_init_phase.w, scalerSlice->y_init_phase.h);
	pr_debug("U: (%d %d), (%d %d), (%d %d %d %d), (%d %d)\n",
		scalerSlice->uv_factor_in.w, scalerSlice->uv_factor_in.h,
		scalerSlice->uv_factor_out.w, scalerSlice->uv_factor_out.h,
		scalerSlice->uv_trim.start_x, scalerSlice->uv_trim.start_y,
		scalerSlice->uv_trim.size_x, scalerSlice->uv_trim.size_y,
		scalerSlice->uv_init_phase.w, scalerSlice->uv_init_phase.h);

	return ret;
}

static int ispslice_slice_scaler_info_cfg(
		struct slice_cfg_input *in_ptr,
		struct isp_slice_context *slc_ctx)
{
	int i, j;
	struct yuv_scaler_info  *frm_scaler;
	struct img_deci_info *frm_deci;
	struct img_trim *frm_trim0;
	struct img_trim *frm_trim1;
	struct slice_scaler_info *slc_scaler;
	struct isp_slice_desc *cur_slc;
	struct isp_scaler_slice_tmp sinfo = {0};
	uint32_t trim1_sum_x[ISP_SPATH_NUM][SLICE_W_NUM_MAX] = { { 0 }, { 0 } };
	uint32_t trim1_sum_y[ISP_SPATH_NUM][SLICE_H_NUM_MAX] = { { 0 }, { 0 } };

	sinfo.slice_col_num = slc_ctx->slice_col_num;
	sinfo.slice_row_num = slc_ctx->slice_row_num;
	sinfo.trim1_sum_x = 0;
	sinfo.trim1_sum_y = 0;
	sinfo.overlap_bad_up = slc_ctx->overlap_up - YUVSCALER_OVERLAP_UP;
	sinfo.overlap_bad_down = slc_ctx->overlap_down - YUVSCALER_OVERLAP_DOWN;
	sinfo.overlap_bad_left = slc_ctx->overlap_left - YUVSCALER_OVERLAP_LEFT;
	sinfo.overlap_bad_right = slc_ctx->overlap_right - YUVSCALER_OVERLAP_RIGHT;

	for (i = 0; i < SLICE_NUM_MAX; i++)
		pr_debug("slice %d valid %d. xy (%d %d)  %p\n",
			i, slc_ctx->slices[i].valid, slc_ctx->slices[i].x, slc_ctx->slices[i].y, &slc_ctx->slices[i]);

	cur_slc = &slc_ctx->slices[0];
	for (i = 0; i < SLICE_NUM_MAX; i++, cur_slc++) {
		if (cur_slc->valid == 0) {
			pr_debug("slice %d not valid. %p\n", i, cur_slc);
			continue;
		}

		for (j = 0; j < ISP_SPATH_NUM; j++) {

			frm_scaler = in_ptr->frame_scaler[j];
			if (frm_scaler == NULL) {
				/* path is not valid. */
				cur_slc->path_en[j] = 0;
				pr_debug("path %d not enable.\n", j);
				continue;
			}
			cur_slc->path_en[j] = 1;
			pr_debug("path %d  enable.\n", j);

			if (j == ISP_SPATH_FD) {
				ispslice_slice_thumbscaler_cfg(cur_slc,
					in_ptr->frame_trim0[j],
					in_ptr->thumb_scaler,
					&cur_slc->slice_thumbscaler);
				continue;
			}

			frm_trim0 = in_ptr->frame_trim0[j];
			frm_deci = in_ptr->frame_deci[j];
			frm_trim1 = in_ptr->frame_trim1[j];

			slc_scaler = &cur_slc->slice_scaler[j];
			slc_scaler->scaler_bypass = frm_scaler->scaler_bypass;
			slc_scaler->odata_mode = frm_scaler->odata_mode;

			sinfo.x = cur_slc->x;
			sinfo.y = cur_slc->y;

			sinfo.start_col = cur_slc->slice_pos.start_col;
			sinfo.end_col = cur_slc->slice_pos.end_col;
			sinfo.start_row = cur_slc->slice_pos.start_row;
			sinfo.end_row = cur_slc->slice_pos.end_row;

			sinfo.start_col_orig = cur_slc->slice_pos_orig.start_col;
			sinfo.end_col_orig = cur_slc->slice_pos_orig.end_col;
			sinfo.start_row_orig = cur_slc->slice_pos_orig.start_row;
			sinfo.end_row_orig = cur_slc->slice_pos_orig.end_row;

			sinfo.trim0_end_x = frm_trim0->start_x + frm_trim0->size_x;
			sinfo.trim0_end_y = frm_trim0->start_y + frm_trim0->size_y;

			ispslice_spath_trim0_info_cfg(&sinfo, frm_trim0, slc_scaler);
			if (slc_scaler->out_of_range) {
				cur_slc->path_en[j] = 0;
				continue;
			}

			ispslice_spath_deci_info_cfg(&sinfo, frm_deci, frm_trim0, slc_scaler);
			ispslice_spath_scaler_info_cfg(&sinfo, frm_trim0, frm_scaler, slc_scaler);

			sinfo.trim1_sum_x = trim1_sum_x[j][cur_slc->x];
			sinfo.trim1_sum_y = trim1_sum_y[j][cur_slc->y];

			ispslice_spath_trim1_info_cfg(&sinfo, frm_trim0, frm_scaler, slc_scaler);

			if ((cur_slc->y == 0)
				&& ((cur_slc->x + 1) < SLICE_W_NUM_MAX))
				trim1_sum_x[j][cur_slc->x + 1] = slc_scaler->trim1_size_x + trim1_sum_x[j][cur_slc->x];

			slc_scaler->src_size_x = sinfo.end_col - sinfo.start_col +1;
			slc_scaler->src_size_y = sinfo.end_row - sinfo.start_row +1;
			slc_scaler->dst_size_x = slc_scaler->scaler_out_width;
			slc_scaler->dst_size_y = slc_scaler->scaler_out_height;
		}

		/* check if all path scaler out of range. */
		/* if yes, invalid this slice. */
		cur_slc->valid = 0;
		for (j = 0; j < ISP_SPATH_NUM; j++)
			cur_slc->valid |= cur_slc->path_en[j];
		pr_debug("final slice (%d, %d) valid = %d\n", cur_slc->x, cur_slc->y, cur_slc->valid);
	}

	return 0;
}

static int ispslice_slice_scaler_info_cfg_ex(
		struct slice_cfg_input *slc_cfg_input, struct isp_slice_context *slice_ctx)
{
	int i = 0;
	int j = 0;
	int ret = 0;
	struct isp_fw_scaler_slice slice_param[2][SLICE_NUM_MAX] = {0};

	if (!slc_cfg_input || !slice_ctx) {
		pr_err("fail to get input ptr NULL\n");
		return -1;
	}

	ret = alg_isp_init_yuvscaler_slice(slc_cfg_input, slice_ctx, slice_param);
	if (ret) {
		pr_err("fail to get yuvscaler slice\n");
		return -1;
	}

	for (j = 0; j < ISP_SPATH_FD; j++) {
		if (slc_cfg_input->calc_dyn_ov.path_en[j] == 0) {
			for (i = 0; i < slice_ctx->slice_num; i++)
				slice_ctx->slices[i].path_en[j] = 0;
		} else {
			for (i = 0; i < slice_ctx->slice_num; i++) {
				slice_ctx->slices[i].path_en[j] = 1;
				if (slc_cfg_input->calc_dyn_ov.path_scaler[j]->scaler.scaler_bypass) {
					slice_ctx->slices[i].slice_scaler[j].trim1_size_x = slice_param[j][i].trim1_size_x;
					slice_ctx->slices[i].slice_scaler[j].trim1_size_y = slice_param[j][i].trim1_size_y;

					pr_debug("scaler_bypass: path %d, slice_id %d, trim1 size_x %d, size_y %d\n",
						j, i, slice_param[j][i].trim1_size_x, slice_param[j][i].trim1_size_y);
				} else {
					slice_ctx->slices[i].slice_scaler[j].trim0_start_x = slice_param[j][i].trim0_start_x;
					slice_ctx->slices[i].slice_scaler[j].trim0_start_y = slice_param[j][i].trim0_start_y;
					slice_ctx->slices[i].slice_scaler[j].trim0_size_x = slice_param[j][i].trim0_size_x;
					slice_ctx->slices[i].slice_scaler[j].trim0_size_y = slice_param[j][i].trim0_size_y;
					slice_ctx->slices[i].slice_scaler[j].trim1_start_x = slice_param[j][i].trim1_start_x;
					slice_ctx->slices[i].slice_scaler[j].trim1_start_y = slice_param[j][i].trim1_start_y;
					slice_ctx->slices[i].slice_scaler[j].trim1_size_x = slice_param[j][i].trim1_size_x;
					slice_ctx->slices[i].slice_scaler[j].trim1_size_y = slice_param[j][i].trim1_size_y;
					slice_ctx->slices[i].slice_scaler[j].scaler_ip_int = slice_param[j][i].scaler_ip_int;
					slice_ctx->slices[i].slice_scaler[j].scaler_ip_rmd = slice_param[j][i].scaler_ip_rmd;
					slice_ctx->slices[i].slice_scaler[j].scaler_cip_int = slice_param[j][i].scaler_cip_int;
					slice_ctx->slices[i].slice_scaler[j].scaler_cip_rmd = slice_param[j][i].scaler_cip_rmd;
					slice_ctx->slices[i].slice_scaler[j].scaler_ip_int_ver = slice_param[j][i].scaler_ip_int_ver;
					slice_ctx->slices[i].slice_scaler[j].scaler_ip_rmd_ver = slice_param[j][i].scaler_ip_rmd_ver;
					slice_ctx->slices[i].slice_scaler[j].scaler_cip_int_ver = slice_param[j][i].scaler_cip_int_ver;
					slice_ctx->slices[i].slice_scaler[j].scaler_cip_rmd_ver = slice_param[j][i].scaler_cip_rmd_ver;
					slice_ctx->slices[i].slice_scaler[j].scaler_factor_in = slice_param[j][i].scaler_factor_in;
					slice_ctx->slices[i].slice_scaler[j].scaler_factor_out = slice_param[j][i].scaler_factor_out;
					slice_ctx->slices[i].slice_scaler[j].scaler_factor_in_ver = slice_param[j][i].scaler_factor_in_ver;
					slice_ctx->slices[i].slice_scaler[j].scaler_factor_out_ver = slice_param[j][i].scaler_factor_out_ver;
					slice_ctx->slices[i].slice_scaler[j].src_size_x = slice_param[j][i].src_size_x;
					slice_ctx->slices[i].slice_scaler[j].src_size_y = slice_param[j][i].src_size_y;
					slice_ctx->slices[i].slice_scaler[j].dst_size_x = slice_param[j][i].dst_size_x;
					slice_ctx->slices[i].slice_scaler[j].dst_size_y = slice_param[j][i].dst_size_y;

					pr_debug("path %d, slice_id %d, trim0( %d, %d, %d, %d), trim1( %d, %d, %d, %d)\n",
						j, i, slice_param[j][i].trim0_start_x, slice_param[j][i].trim0_start_y, slice_param[j][i].trim0_size_x, slice_param[j][i].trim0_size_y,
						slice_param[j][i].trim1_start_x, slice_param[j][i].trim1_start_y, slice_param[j][i].trim1_size_x, slice_param[j][i].trim1_size_y);
					pr_debug("path %d, slice_id %d, hor(ip_int %d, ip_rmd %d, cip_int %d, cip_rmd %d), ver(ip_int %d, ip_rmd %d, cip_int %d, cip_rmd %d),\n",
						j, i, slice_param[j][i].scaler_ip_int, slice_param[j][i].scaler_ip_rmd, slice_param[j][i].scaler_cip_int, slice_param[j][i].scaler_cip_rmd,
						 slice_param[j][i].scaler_ip_int_ver, slice_param[j][i].scaler_ip_rmd_ver, slice_param[j][i].scaler_cip_int_ver, slice_param[j][i].scaler_cip_rmd_ver);
					pr_debug("path %d, slice_id %d, hor(factor: in %d, out %d), ver(factor: in %d, out %d)\n",
						j, i, slice_param[j][i].scaler_factor_in , slice_param[j][i].scaler_factor_out,
						slice_param[j][i].scaler_factor_in_ver, slice_param[j][i].scaler_factor_out_ver);
					pr_debug("path %d, slice_id %d, src(size_x %d, size_y %d), dst(size_x %d, size_y %d)\n",
						j, i, slice_param[j][i].src_size_x, slice_param[j][i].src_size_y, slice_param[j][i].dst_size_x, slice_param[j][i].dst_size_y);
				}
			}
		}
	}

	return ret;
}

static int ispslice_scaler_info_calc_cfg(
		struct slice_cfg_input *slc_cfg_input, struct isp_slice_context *slice_ctx)
{
	int i = 0, j = 0;
	int ret = 0;
	struct alg_slice_drv_overlap *slice_info = NULL;
	struct yuvscaler_param_t *sliceParam = NULL;
	struct thumbnailscaler_slice *slcParam_t = NULL;

	if (!slc_cfg_input || !slice_ctx) {
		pr_err("fail to get input ptr NULL\n");
		return -1;
	}

	slice_info = &slice_ctx->slice_overlap;
	for (i = 0; i < slice_ctx->slice_num; i++) {
		for (j = 0; j < ISP_SPATH_NUM; j++) {
			slice_ctx->slices[i].path_en[j] = slc_cfg_input->calc_dyn_ov.path_en[j];
			if (j != ISP_SPATH_FD) {
				if (j == ISP_SPATH_CP)
					sliceParam = &slice_info->scaler1.sliceParam[i];
				else
					sliceParam = &slice_info->scaler2.sliceParam[i];
				if (slc_cfg_input->calc_dyn_ov.path_scaler[j]->scaler.scaler_bypass) {
					slice_ctx->slices[i].slice_scaler[j].scaler_bypass = 1;
					slice_ctx->slices[i].slice_scaler[j].trim1_size_x = sliceParam->trim1_info.trim_size_x;
					slice_ctx->slices[i].slice_scaler[j].trim1_size_y = sliceParam->trim1_info.trim_size_y;

					pr_debug("scaler_bypass: path %d, slice_id %d, trim1 size_x %d, size_y %d\n",
						j, i, sliceParam->trim1_info.trim_size_x, sliceParam->trim1_info.trim_size_y);
				} else {
					slice_ctx->slices[i].slice_scaler[j].sub_scaler_bypass = !sliceParam->scaler_info.scaler_en;
					slice_ctx->slices[i].slice_scaler[j].trim0_start_x = sliceParam->trim0_info.trim_start_x;
					slice_ctx->slices[i].slice_scaler[j].trim0_start_y = sliceParam->trim0_info.trim_start_y;
					slice_ctx->slices[i].slice_scaler[j].trim0_size_x = sliceParam->trim0_info.trim_size_x;
					slice_ctx->slices[i].slice_scaler[j].trim0_size_y = sliceParam->trim0_info.trim_size_y;
					slice_ctx->slices[i].slice_scaler[j].trim1_start_x = sliceParam->trim1_info.trim_start_x;
					slice_ctx->slices[i].slice_scaler[j].trim1_start_y = sliceParam->trim1_info.trim_start_y;
					slice_ctx->slices[i].slice_scaler[j].trim1_size_x = sliceParam->trim1_info.trim_size_x;
					slice_ctx->slices[i].slice_scaler[j].trim1_size_y = sliceParam->trim1_info.trim_size_y;
					slice_ctx->slices[i].slice_scaler[j].scaler_ip_int = sliceParam->scaler_info.init_phase_info.scaler_init_phase_int[0][0];
					slice_ctx->slices[i].slice_scaler[j].scaler_ip_rmd = sliceParam->scaler_info.init_phase_info.scaler_init_phase_rmd[0][0];;
					slice_ctx->slices[i].slice_scaler[j].scaler_cip_int = sliceParam->scaler_info.init_phase_info.scaler_init_phase_int[0][1];
					slice_ctx->slices[i].slice_scaler[j].scaler_cip_rmd = sliceParam->scaler_info.init_phase_info.scaler_init_phase_rmd[0][1];
					slice_ctx->slices[i].slice_scaler[j].scaler_ip_int_ver = sliceParam->scaler_info.init_phase_info.scaler_init_phase_int[1][0];
					slice_ctx->slices[i].slice_scaler[j].scaler_ip_rmd_ver = sliceParam->scaler_info.init_phase_info.scaler_init_phase_rmd[1][0];
					slice_ctx->slices[i].slice_scaler[j].scaler_cip_int_ver = sliceParam->scaler_info.init_phase_info.scaler_init_phase_int[1][1];
					slice_ctx->slices[i].slice_scaler[j].scaler_cip_rmd_ver = sliceParam->scaler_info.init_phase_info.scaler_init_phase_rmd[1][1];
					slice_ctx->slices[i].slice_scaler[j].scaler_factor_in = sliceParam->scaler_info.scaler_factor_in_hor;
					slice_ctx->slices[i].slice_scaler[j].scaler_factor_out = sliceParam->scaler_info.scaler_factor_out_hor;
					slice_ctx->slices[i].slice_scaler[j].scaler_factor_in_ver = sliceParam->scaler_info.scaler_factor_in_ver;
					slice_ctx->slices[i].slice_scaler[j].scaler_factor_out_ver = sliceParam->scaler_info.scaler_factor_out_ver;
					slice_ctx->slices[i].slice_scaler[j].src_size_x = sliceParam->src_size_x;
					slice_ctx->slices[i].slice_scaler[j].src_size_y = sliceParam->src_size_y;
					slice_ctx->slices[i].slice_scaler[j].dst_size_x = sliceParam->dst_size_x;
					slice_ctx->slices[i].slice_scaler[j].dst_size_y = sliceParam->dst_size_y;

					pr_debug("path %d, slice_id %d, trim0( %d, %d, %d, %d), trim1( %d, %d, %d, %d)\n",
						j, i, sliceParam->trim0_info.trim_start_x, sliceParam->trim0_info.trim_start_y,
						sliceParam->trim0_info.trim_size_x, sliceParam->trim0_info.trim_size_y,
						sliceParam->trim1_info.trim_start_x, sliceParam->trim1_info.trim_start_y,
						sliceParam->trim1_info.trim_size_x, sliceParam->trim1_info.trim_size_y);
					pr_debug("path %d, slice_id %d, hor(factor: in %d, out %d), ver(factor: in %d, out %d)\n",
						j, i, sliceParam->scaler_info.scaler_factor_in_hor , sliceParam->scaler_info.scaler_factor_out_hor,
						sliceParam->scaler_info.scaler_factor_in_ver, sliceParam->scaler_info.scaler_factor_out_ver);
					pr_debug("path %d, slice_id %d, src(size_x %d, size_y %d), dst(size_x %d, size_y %d)\n",
						j, i, sliceParam->src_size_x, sliceParam->src_size_y, sliceParam->dst_size_x, sliceParam->dst_size_y);
				}
			} else {
				slcParam_t = &slice_info->thumbnail_scaler.sliceParam[i];
				if (slc_cfg_input->calc_dyn_ov.thumb_scaler->scaler_bypass) {
					slice_ctx->slices[i].slice_thumbscaler.scaler_bypass = 1;
					slice_ctx->slices[i].slice_thumbscaler.y_dst_after_scaler.w = slcParam_t->y_slice_des_size_hor;
					slice_ctx->slices[i].slice_thumbscaler.y_dst_after_scaler.h = slcParam_t->y_slice_des_size_ver;
					pr_debug("scaler_bypass: path %d, slice_id %d, y dst(hor %d ver %d)\n",
						j, i, slcParam_t->y_slice_des_size_hor, slcParam_t->y_slice_des_size_ver);
				} else {
					slice_ctx->slices[i].slice_thumbscaler.src0.w = slcParam_t->slice_size_before_trim_hor;
					slice_ctx->slices[i].slice_thumbscaler.src0.h = slcParam_t->slice_size_before_trim_ver;
					slice_ctx->slices[i].slice_thumbscaler.y_src_after_deci.w = slcParam_t->y_slice_src_size_hor;
					slice_ctx->slices[i].slice_thumbscaler.y_src_after_deci.h = slcParam_t->y_slice_src_size_ver;
					slice_ctx->slices[i].slice_thumbscaler.y_dst_after_scaler.w = slcParam_t->y_slice_des_size_hor;
					slice_ctx->slices[i].slice_thumbscaler.y_dst_after_scaler.h = slcParam_t->y_slice_des_size_ver;
					slice_ctx->slices[i].slice_thumbscaler.y_trim.start_x = slcParam_t->y_trim0_start_hor;
					slice_ctx->slices[i].slice_thumbscaler.y_trim.start_y = slcParam_t->y_trim0_start_ver;
					slice_ctx->slices[i].slice_thumbscaler.y_trim.size_x = slcParam_t->y_trim0_size_hor;
					slice_ctx->slices[i].slice_thumbscaler.y_trim.size_y = slcParam_t->y_trim0_size_ver;
					slice_ctx->slices[i].slice_thumbscaler.y_init_phase.w = slcParam_t->y_init_phase_hor;
					slice_ctx->slices[i].slice_thumbscaler.y_init_phase.h = slcParam_t->y_init_phase_ver;
					slice_ctx->slices[i].slice_thumbscaler.uv_src_after_deci.w = slcParam_t->uv_slice_src_size_hor;
					slice_ctx->slices[i].slice_thumbscaler.uv_src_after_deci.h = slcParam_t->uv_slice_src_size_ver;
					slice_ctx->slices[i].slice_thumbscaler.uv_dst_after_scaler.w = slcParam_t->uv_slice_des_size_hor;
					slice_ctx->slices[i].slice_thumbscaler.uv_dst_after_scaler.h = slcParam_t->uv_slice_des_size_ver;
					slice_ctx->slices[i].slice_thumbscaler.uv_trim.start_x = slcParam_t->uv_trim0_start_hor;
					slice_ctx->slices[i].slice_thumbscaler.uv_trim.start_y = slcParam_t->uv_trim0_start_ver;
					slice_ctx->slices[i].slice_thumbscaler.uv_trim.size_x = slcParam_t->uv_trim0_size_hor;
					slice_ctx->slices[i].slice_thumbscaler.uv_trim.size_y = slcParam_t->uv_trim0_size_ver;
					slice_ctx->slices[i].slice_thumbscaler.uv_init_phase.w = slcParam_t->uv_init_phase_hor;
					slice_ctx->slices[i].slice_thumbscaler.uv_init_phase.h = slcParam_t->uv_init_phase_ver;

					pr_debug("path %d, slice_id %d, before_trim(hor %d, ver %d)\n",
						j, i, slcParam_t->slice_size_before_trim_hor, slcParam_t->slice_size_before_trim_ver);
					pr_debug("path %d, slice_id %d, y src(hor %d, ver %d), dst(hor %d, ver %d)\n",
						j, i, slcParam_t->y_slice_src_size_hor, slcParam_t->y_slice_src_size_ver,
						slcParam_t->y_slice_des_size_hor, slcParam_t->y_slice_des_size_ver);
					pr_debug("path %d, slice_id %d, y trim0( %d, %d, %d, %d), phase(hor %d, ver %d)\n",
						j, i, slcParam_t->y_trim0_start_hor, slcParam_t->y_trim0_start_ver,
						slcParam_t->y_trim0_size_hor, slcParam_t->y_trim0_size_ver,
						slcParam_t->y_init_phase_hor, slcParam_t->y_init_phase_ver);
					pr_debug("path %d, slice_id %d, uv src(hor %d, ver %d), dst(hor %d, ver %d)\n",
						j, i, slcParam_t->uv_slice_src_size_hor, slcParam_t->uv_slice_src_size_ver,
						slcParam_t->uv_slice_des_size_hor, slcParam_t->uv_slice_des_size_ver);
					pr_debug("path %d, slice_id %d, uv trim0( %d, %d, %d, %d), phase(hor %d, ver %d)\n",
						j, i, slcParam_t->uv_trim0_start_hor, slcParam_t->uv_trim0_start_ver,
						slcParam_t->uv_trim0_size_hor, slcParam_t->uv_trim0_size_ver,
						slcParam_t->uv_init_phase_hor, slcParam_t->uv_init_phase_ver);
				}
			}
		}
	}
	return ret;
}

static void ispslice_slice_fetch_cfg(struct isp_hw_fetch_info *frm_fetch,
		struct isp_slice_desc *cur_slc)
{
	uint32_t start_col, start_row;
	uint32_t end_col, end_row;
	uint32_t ch_offset[3] = { 0 };
	uint32_t mipi_word_num_start[16] = {
		0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5
	};
	uint32_t mipi_word_num_end[16] = {
		0, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5
	};
	struct slice_fetch_info *slc_fetch;
	struct img_pitch *pitch;

	start_col = cur_slc->slice_pos_fetch.start_col;
	start_row = cur_slc->slice_pos_fetch.start_row;
	end_col = cur_slc->slice_pos_fetch.end_col;
	end_row = cur_slc->slice_pos_fetch.end_row;

	slc_fetch = &cur_slc->slice_fetch;
	pitch = &frm_fetch->pitch;

	switch (frm_fetch->fetch_fmt) {
	case ISP_FETCH_YUV422_3FRAME:
		ch_offset[0] = start_row * pitch->pitch_ch0 + start_col;
		ch_offset[1] = start_row * pitch->pitch_ch1 + (start_col >> 1);
		ch_offset[2] = start_row * pitch->pitch_ch2 + (start_col >> 1);
		break;

	case ISP_FETCH_YUV422_2FRAME:
	case ISP_FETCH_YVU422_2FRAME:
		ch_offset[0] = start_row * pitch->pitch_ch0 + start_col;
		ch_offset[1] = start_row * pitch->pitch_ch1 + start_col;
		break;

	case ISP_FETCH_YUV420_2FRAME:
	case ISP_FETCH_YVU420_2FRAME:
		slc_fetch->is_pack = 0;
		ch_offset[0] = start_row * pitch->pitch_ch0 + start_col;
		ch_offset[1] = (start_row >> 1) * pitch->pitch_ch1 + start_col;
		pr_debug("(%d %d %d %d), pitch %d %d, offset %d %d, mipi %d %d\n",
			start_row, start_col, end_row, end_col,
			pitch->pitch_ch0, pitch->pitch_ch1, ch_offset[0], ch_offset[1],
			slc_fetch->mipi_byte_rel_pos, slc_fetch->mipi_word_num);
		break;
	case ISP_FETCH_YUV420_2FRAME_10:
	case ISP_FETCH_YVU420_2FRAME_10:
		ch_offset[0] = start_row * pitch->pitch_ch0 + start_col * 2;
		ch_offset[1] = (start_row >> 1) * pitch->pitch_ch1 + start_col * 2;
		pr_debug("(%d %d %d %d), pitch %d %d, offset %d %d, mipi %d %d\n",
			start_row, start_col, end_row, end_col,
			pitch->pitch_ch0, pitch->pitch_ch1, ch_offset[0], ch_offset[1],
			slc_fetch->mipi_byte_rel_pos, slc_fetch->mipi_word_num);
		break;
	case ISP_FETCH_YUV420_2FRAME_MIPI:
	case ISP_FETCH_YVU420_2FRAME_MIPI:
		ch_offset[0] = start_row * pitch->pitch_ch0 + (start_col >> 2) * 5 + (start_col & 0x3);
		ch_offset[1] = (start_row >> 1) * pitch->pitch_ch1 + (start_col >> 2) * 5 + (start_col & 0x3);
		slc_fetch->mipi_byte_rel_pos = start_col & 0xF;
		slc_fetch->mipi_word_num = ((end_col + 1) >> 4) * 5
			+ mipi_word_num_end[(end_col + 1) & 0xF]
			- ((start_col + 1) >> 4) * 5
			- mipi_word_num_start[(start_col + 1) & 0xF] + 1;
		slc_fetch->mipi_byte_rel_pos_uv = slc_fetch->mipi_byte_rel_pos;
		slc_fetch->mipi_word_num_uv = slc_fetch->mipi_word_num;
		slc_fetch->is_pack = 1;
		pr_debug("(%d %d %d %d), pitch %d %d, offset %d %d, mipi %d %d\n",
			start_row, start_col, end_row, end_col,
			pitch->pitch_ch0, pitch->pitch_ch1, ch_offset[0], ch_offset[1],
			slc_fetch->mipi_byte_rel_pos, slc_fetch->mipi_word_num);
		break;
	case ISP_FETCH_CSI2_RAW10:
		ch_offset[0] = start_row * pitch->pitch_ch0
			+ (start_col >> 2) * 5 + (start_col & 0x3);

		slc_fetch->mipi_byte_rel_pos = start_col & 0x0f;
		slc_fetch->mipi_word_num =
			((((end_col + 1) >> 4) * 5
			+ mipi_word_num_end[(end_col + 1) & 0x0f])
			-(((start_col + 1) >> 4) * 5
			+ mipi_word_num_start[(start_col + 1)
			& 0x0f]) + 1);
		pr_debug("(%d %d %d %d), pitch %d, offset %d, mipi %d %d\n",
			start_row, start_col, end_row, end_col,
			pitch->pitch_ch0, ch_offset[0],
			slc_fetch->mipi_byte_rel_pos, slc_fetch->mipi_word_num);
		break;
	case ISP_FETCH_FULL_RGB10:
		ch_offset[0] = start_row * pitch->pitch_ch0 + start_col * 8;
		pr_debug("isp full rgb pitch %d offset %d\n", pitch->pitch_ch0, ch_offset[0]);
		break;
	default:
		ch_offset[0] = start_row * pitch->pitch_ch0 + start_col * 2;
		break;
	}

	slc_fetch->addr.addr_ch0 = frm_fetch->addr.addr_ch0 + ch_offset[0];
	slc_fetch->addr.addr_ch1 = frm_fetch->addr.addr_ch1 + ch_offset[1];
	slc_fetch->addr.addr_ch2 = frm_fetch->addr.addr_ch2 + ch_offset[2];
	slc_fetch->size.h = end_row - start_row + 1;
	slc_fetch->size.w = end_col - start_col + 1;

	pr_debug("slice fetch size %d, %d\n", slc_fetch->size.w, slc_fetch->size.h);
}

static void ispslice_slice_fbd_raw_cfg(struct isp_fbd_raw_info *frame_fbd_raw,
		struct isp_slice_desc *cur_slc)
{
	uint32_t left_tiles_num = 0,
			right_tiles_num = 0,
			hor_middle_tiles_num = 0,
			left_size, right_size = 0;
	uint32_t up_tiles_num = 0,
			down_tiles_num = 0,
			vertical_middle_tiles_num = 0,
			up_size = 0, down_size = 0;
	uint32_t left_offset_tiles_num = 0,
			up_offset_tiles_num = 0;
	uint32_t global_img_width  = frame_fbd_raw->size.w;
	uint32_t tiles_num_pitch = frame_fbd_raw->tiles_num_pitch;
	uint32_t start_row = cur_slc->slice_pos_fetch.start_row;
	uint32_t end_row = cur_slc->slice_pos_fetch.end_row;
	uint32_t start_col = cur_slc->slice_pos_fetch.start_col;
	uint32_t end_col = cur_slc->slice_pos_fetch.end_col;
	uint32_t slice_width = end_col - start_col + 1;
	uint32_t slice_height = end_row - start_row + 1;

	struct slice_fbd_raw_info *slc_fbd_raw
		= &cur_slc->slice_fbd_raw;

	slc_fbd_raw->width = slice_width;
	slc_fbd_raw->height = slice_height;

	left_offset_tiles_num = start_col / ISP_FBD_TILE_WIDTH;
	if (start_col % ISP_FBD_TILE_WIDTH == 0) {
		left_tiles_num = 0;
		left_size = 0;
	} else {
		left_tiles_num = 1;
		left_size = ISP_FBD_TILE_WIDTH - start_col % ISP_FBD_TILE_WIDTH;
	}
	if ((end_col + 1) % ISP_FBD_TILE_WIDTH == 0) {
		right_tiles_num = 0;
		right_size = 0;
	} else {
		right_tiles_num = 1;
		right_size = (end_col + 1) % ISP_FBD_TILE_WIDTH;
	}
	hor_middle_tiles_num = (slice_width - left_size - right_size) / ISP_FBD_TILE_WIDTH;

	up_offset_tiles_num = start_row / ISP_FBD_TILE_HEIGHT;
	if (start_row %  ISP_FBD_TILE_HEIGHT == 0) {
		up_tiles_num = 0;
		up_size = 0;
	} else {
		up_tiles_num = 1;
		up_size = ISP_FBD_TILE_HEIGHT - start_row % ISP_FBD_TILE_HEIGHT;
	}
	if ((end_row + 1) % ISP_FBD_TILE_HEIGHT == 0) {
		down_tiles_num = 0;
		down_size = 0;
	} else {
		down_tiles_num = 1;
		down_size = (end_row + 1) % ISP_FBD_TILE_HEIGHT;
	}
	vertical_middle_tiles_num = (slice_height - up_size - down_size) / ISP_FBD_TILE_HEIGHT;

	slc_fbd_raw->pixel_start_in_hor = start_col % ISP_FBD_TILE_WIDTH;
	slc_fbd_raw->pixel_start_in_ver = start_row % ISP_FBD_TILE_HEIGHT;
	slc_fbd_raw->tiles_num_in_hor = left_tiles_num + right_tiles_num + hor_middle_tiles_num;
	slc_fbd_raw->tiles_num_in_ver = up_tiles_num + down_tiles_num + vertical_middle_tiles_num;
	pr_debug("left_tiles_num %d right_tiles_num %d middle_tiles_num %d\n",
		left_tiles_num, right_tiles_num, hor_middle_tiles_num);
	pr_debug("up_tiles_num %d down_tiles_num %d vertical_middle_tiles_num %d\n",
		up_tiles_num, down_tiles_num, vertical_middle_tiles_num);
	pr_debug("slice_tiles_num_in_ver %d slice_tiles_num_in_hor %d\n",
		slc_fbd_raw->tiles_num_in_ver, slc_fbd_raw->tiles_num_in_hor);
	slc_fbd_raw->tiles_start_odd = left_offset_tiles_num % 2;
	slc_fbd_raw->header_addr_init = frame_fbd_raw->header_addr_init
		- (left_offset_tiles_num + up_offset_tiles_num * tiles_num_pitch) / 2;
	slc_fbd_raw->tile_addr_init_x256 = frame_fbd_raw->tile_addr_init_x256
		+ (left_offset_tiles_num + up_offset_tiles_num * tiles_num_pitch) * ISP_FBD_BASE_ALIGN;
	slc_fbd_raw->low_bit_addr_init = frame_fbd_raw->low_bit_addr_init
		+ (start_col + start_row * global_img_width) / 2;

	/* have to copy these here for fmcu cmd queue, sad */
	slc_fbd_raw->tiles_num_pitch = frame_fbd_raw->tiles_num_pitch;
	slc_fbd_raw->low_bit_pitch = frame_fbd_raw->low_bit_pitch;
	slc_fbd_raw->low_4bit_pitch = frame_fbd_raw->low_4bit_pitch;
	slc_fbd_raw->fetch_fbd_bypass = 0;

	if (frame_fbd_raw->fetch_fbd_4bit_bypass == 0)
		slc_fbd_raw->low_4bit_addr_init = frame_fbd_raw->low_4bit_addr_init
		+ (start_col + start_row * global_img_width);

	if (frame_fbd_raw->fetch_fbd_4bit_bypass == 0)
		slc_fbd_raw->fetch_fbd_4bit_bypass = 0;
	else
		slc_fbd_raw->fetch_fbd_4bit_bypass = 1;

	pr_debug("head %x, tile %x, low2 %x, low4 %x\n",
		 slc_fbd_raw->header_addr_init,
		 slc_fbd_raw->tile_addr_init_x256,
		 slc_fbd_raw->low_bit_addr_init,
		 slc_fbd_raw->low_4bit_addr_init);
}

static void ispslice_slice_fbd_yuv_cfg(struct isp_fbd_yuv_info *frame_fbd_yuv,
		struct isp_slice_desc *cur_slc)
{
	uint32_t tiles_num_pitch = frame_fbd_yuv->tile_num_pitch;
	uint32_t start_row = 0, end_row = 0;
	uint32_t start_col = 0, end_col = 0;
	struct slice_fbd_yuv_info *slc_fbd_yuv = &cur_slc->slice_fbd_yuv;

	if (cur_slc->pyr_rec_eb) {
		start_row = cur_slc->slice_pos_fbd.start_row;
		end_row = cur_slc->slice_pos_fbd.end_row;
		start_col = cur_slc->slice_pos_fbd.start_col;
		end_col = cur_slc->slice_pos_fbd.end_col;
	} else {
		start_row = cur_slc->slice_pos_fetch.start_row;
		end_row = cur_slc->slice_pos_fetch.end_row;
		start_col = cur_slc->slice_pos_fetch.start_col;
		end_col = cur_slc->slice_pos_fetch.end_col;
	}

	if (frame_fbd_yuv->trim.start_x || frame_fbd_yuv->trim.start_y) {
		start_col = start_col + frame_fbd_yuv->trim.start_x;
		start_row = start_row + frame_fbd_yuv->trim.start_y;
		end_col = frame_fbd_yuv->trim.start_x + end_col;
		end_row = frame_fbd_yuv->trim.start_y + end_row;
	} else {
		slc_fbd_yuv->slice_start_pxl_xpt = start_col;
		slc_fbd_yuv->slice_start_pxl_ypt = start_row;
	}
	slc_fbd_yuv->slice_start_pxl_ypt = start_row;
	slc_fbd_yuv->slice_start_pxl_xpt = start_col;

	slc_fbd_yuv->slice_size.w = ((end_col - start_col + 1) >> 1) << 1;
	slc_fbd_yuv->slice_size.h = ((end_row - start_row + 1) >> 1) << 1;
	slc_fbd_yuv->slice_start_header_addr = frame_fbd_yuv->slice_start_header_addr
		+ ((start_row / ISP_FBD_TILE_HEIGHT) * tiles_num_pitch + start_col / ISP_FBD_TILE_WIDTH) * 16 ;

	/* have to copy these here for fmcu cmd queue, sad */
	slc_fbd_yuv->tile_num_pitch = frame_fbd_yuv->tile_num_pitch;
	slc_fbd_yuv->fetch_fbd_bypass = frame_fbd_yuv->fetch_fbd_bypass;
	slc_fbd_yuv->hblank_num = frame_fbd_yuv->hblank_num;
	slc_fbd_yuv->frame_header_base_addr = frame_fbd_yuv->frame_header_base_addr;

	pr_debug("head %x\n", slc_fbd_yuv->frame_header_base_addr);
}

static int ispslice_fetch_info_cfg(void *cfg_in, struct isp_slice_context *slc_ctx)
{
	int i;
	struct slice_cfg_input *in_ptr = (struct slice_cfg_input *)cfg_in;
	struct isp_slice_desc *cur_slc = &slc_ctx->slices[0];

	for (i = 0; i < SLICE_NUM_MAX; i++, cur_slc++) {
		if (cur_slc->valid == 0)
			continue;

		cur_slc->pyr_rec_eb = slc_ctx->pyr_rec_eb;
		if (!in_ptr->frame_fbd_raw->fetch_fbd_bypass)
			ispslice_slice_fbd_raw_cfg(in_ptr->frame_fbd_raw, cur_slc);
		else if (!in_ptr->frame_fbd_yuv->fetch_fbd_bypass)
			ispslice_slice_fbd_yuv_cfg(in_ptr->frame_fbd_yuv, cur_slc);
		else
			ispslice_slice_fetch_cfg(in_ptr->frame_fetch, cur_slc);
	}

	return 0;
}

static int ispslice_store_info_cfg(
		void *cfg_in, struct isp_slice_context *slc_ctx)
{
	int i, j;
	struct slice_cfg_input *in_ptr = (struct slice_cfg_input *)cfg_in;
	struct isp_store_info *frm_store;
	struct isp_slice_desc *cur_slc;
	struct slice_store_info *slc_store;
	struct slice_scaler_info *slc_scaler;
	struct slice_thumbscaler_info *slc_thumbscaler;

	uint32_t overlap_left = 0, overlap_up = 0;
	uint32_t overlap_right = 0, overlap_down = 0;

	uint32_t start_col = 0, end_col = 0, start_row = 0, end_row = 0;
	uint32_t start_row_out[ISP_SPATH_NUM][SLICE_W_NUM_MAX] = { { 0 }, { 0 } };
	uint32_t start_col_out[ISP_SPATH_NUM][SLICE_W_NUM_MAX] = { { 0 }, { 0 } };
	uint32_t ch0_offset = 0;
	uint32_t ch1_offset = 0;
	uint32_t ch2_offset = 0;

	cur_slc = &slc_ctx->slices[0];
	for (i = 0; i < SLICE_NUM_MAX; i++, cur_slc++) {
		if (cur_slc->valid == 0)
			continue;
		for (j = 0; j < ISP_SPATH_NUM; j++) {
			frm_store = in_ptr->frame_store[j];
			if (frm_store == NULL || cur_slc->path_en[j] == 0)
				/* path is not valid. */
				continue;

			slc_store = &cur_slc->slice_store[j];
			slc_scaler = &cur_slc->slice_scaler[j];

			if (j == ISP_SPATH_FD) {
				slc_thumbscaler = &cur_slc->slice_thumbscaler;
				slc_store->size.w = slc_thumbscaler->y_dst_after_scaler.w;
				slc_store->size.h = slc_thumbscaler->y_dst_after_scaler.h;
				if (cur_slc->y == 0 &&
					(cur_slc->x + 1) < SLICE_W_NUM_MAX)
					start_col_out[j][cur_slc->x + 1] = slc_store->size.w + start_col_out[j][cur_slc->x];
			} else if (slc_scaler->scaler_bypass == 0) {
				if (in_ptr->calc_dyn_ov.verison == ALG_ISP_DYN_OVERLAP_NONE) {
					slc_store->size.w = slc_scaler->trim1_size_x;
					slc_store->size.h = slc_scaler->trim1_size_y;
				} else {
					if ((slc_scaler->scaler_factor_in == slc_scaler->scaler_factor_out)
						&& (slc_scaler->scaler_factor_in_ver == slc_scaler->scaler_factor_out_ver)) {
						overlap_up = slc_ctx->overlapParam.slice_overlap[i].ov_up;
						overlap_down = slc_ctx->overlapParam.slice_overlap[i].ov_down;
						overlap_left = slc_ctx->overlapParam.slice_overlap[i].ov_left;
						overlap_right = slc_ctx->overlapParam.slice_overlap[i].ov_right;

						slc_store->size.w = slc_scaler->trim1_size_x - overlap_left - overlap_right;
						slc_store->size.h = slc_scaler->trim1_size_y - overlap_up - overlap_down;
						slc_store->border.up_border = overlap_up;
						slc_store->border.down_border = overlap_down;
						slc_store->border.left_border = overlap_left;
						slc_store->border.right_border = overlap_right;
					} else {
						slc_store->size.w = slc_scaler->trim1_size_x;
						slc_store->size.h = slc_scaler->trim1_size_y;
					}
				}

				if ((cur_slc->y == 0) && ((cur_slc->x + 1) < SLICE_W_NUM_MAX))
					start_col_out[j][cur_slc->x + 1] = slc_store->size.w + start_col_out[j][cur_slc->x];

				pr_debug("slice %d, path %d,  size(w %d, h %d), overlap: (%d %d %d %d), cur_slc(x %d, y %d)  start_col_out[j][cur_slc->x] %d\n",
					i, j, slc_store->size.w, slc_store->size.h,
					overlap_up, overlap_down, overlap_left, overlap_right,
					cur_slc->x, cur_slc->y, start_col_out[j][cur_slc->x]);
			} else {
				start_col = cur_slc->slice_pos.start_col;
				start_row = cur_slc->slice_pos.start_row;
				end_col = cur_slc->slice_pos.end_col;
				end_row = cur_slc->slice_pos.end_row;
				overlap_left = cur_slc->slice_overlap.overlap_left;
				overlap_right = cur_slc->slice_overlap.overlap_right;
				overlap_up = cur_slc->slice_overlap.overlap_up;
				overlap_down = cur_slc->slice_overlap.overlap_down;

				pr_debug("slice %d. pos: %d %d %d %d, ovl: %d %d %d %d\n",
					i, start_col, end_col, start_row, end_row,
					overlap_up, overlap_down, overlap_left, overlap_right);

				slc_store->size.w = end_col - start_col + 1 -overlap_left - overlap_right;
				slc_store->size.h = end_row - start_row + 1 -overlap_up - overlap_down;
				slc_store->border.up_border = overlap_up;
				slc_store->border.down_border = overlap_down;
				slc_store->border.left_border = overlap_left;
				slc_store->border.right_border = overlap_right;

				if (cur_slc->y == 0)
					slc_store->border.up_border = 0;
				else if (cur_slc->y == (slc_ctx->slice_row_num - 1))
					slc_store->border.down_border = 0;

				if (cur_slc->x == 0)
					slc_store->border.left_border =  0;
				else if (cur_slc->x == (slc_ctx->slice_col_num - 1))
					slc_store->border.right_border = 0;

				if (cur_slc->x != 0)
					start_col_out[j][cur_slc->x] = start_col + slc_store->border.left_border;
				if (cur_slc->y != 0)
					start_row_out[j][cur_slc->y] = start_row + slc_store->border.up_border;

				pr_debug("scaler bypass .  %d   %d", start_row_out[j][cur_slc->y], start_col_out[j][cur_slc->x]);
			}

			switch (frm_store->color_fmt) {
			case ISP_STORE_UYVY:
				ch0_offset = start_col_out[j][cur_slc->x] * 2 + start_row_out[j][cur_slc->y] * frm_store->pitch.pitch_ch0;
				break;
			case ISP_STORE_YUV422_2FRAME:
			case ISP_STORE_YVU422_2FRAME:
				ch0_offset = start_col_out[j][cur_slc->x] + start_row_out[j][cur_slc->y] * frm_store->pitch.pitch_ch0;
				ch1_offset = start_col_out[j][cur_slc->x] + start_row_out[j][cur_slc->y] * frm_store->pitch.pitch_ch1;
				break;
			case ISP_STORE_YUV422_3FRAME:
				ch0_offset = start_col_out[j][cur_slc->x] + start_row_out[j][cur_slc->y] * frm_store->pitch.pitch_ch0;
				ch1_offset = (start_col_out[j][cur_slc->x] >> 1) + start_row_out[j][cur_slc->y] * frm_store->pitch.pitch_ch1;
				ch2_offset = (start_col_out[j][cur_slc->x] >> 1) + start_row_out[j][cur_slc->y] * frm_store->pitch.pitch_ch2;
				break;
			case ISP_STORE_YUV420_2FRAME:
			case ISP_STORE_YVU420_2FRAME:
				ch0_offset = start_col_out[j][cur_slc->x] + start_row_out[j][cur_slc->y] * frm_store->pitch.pitch_ch0;
				ch1_offset = start_col_out[j][cur_slc->x] + start_row_out[j][cur_slc->y] * frm_store->pitch.pitch_ch1 / 2;
				pr_debug("offset %d %d\n", ch0_offset, ch1_offset);
				break;
			case ISP_STORE_YUV420_2FRAME_10:
			case ISP_STORE_YVU420_2FRAME_10:
				ch0_offset = (start_col_out[j][cur_slc->x] + start_row_out[j][cur_slc->y] * frm_store->pitch.pitch_ch0) * 2;
				ch1_offset = (start_col_out[j][cur_slc->x] + start_row_out[j][cur_slc->y] * frm_store->pitch.pitch_ch1 / 2) * 2;
				break;
			case ISP_STORE_YUV420_2FRAME_MIPI:
			case ISP_STORE_YVU420_2FRAME_MIPI:
				ch0_offset = (start_col_out[j][cur_slc->x] + start_row_out[j][cur_slc->y] * frm_store->pitch.pitch_ch0) * 10 / 8;
				ch1_offset = (start_col_out[j][cur_slc->x] + start_row_out[j][cur_slc->y] * frm_store->pitch.pitch_ch1 / 2) * 10 / 8;
				break;
			case ISP_STORE_YUV420_3FRAME:
				ch0_offset = start_col_out[j][cur_slc->x] + start_row_out[j][cur_slc->y] * frm_store->pitch.pitch_ch0;
				ch1_offset = (start_col_out[j][cur_slc->x] >> 1) + start_row_out[j][cur_slc->y] * frm_store->pitch.pitch_ch1 / 2;
				ch2_offset = (start_col_out[j][cur_slc->x] >> 1) + start_row_out[j][cur_slc->y] * frm_store->pitch.pitch_ch2 / 2;
				break;
			case ISP_STORE_FULL_RGB:
				ch0_offset = start_col_out[j][cur_slc->x] * 8 + start_row_out[j][cur_slc->y] * frm_store->pitch.pitch_ch0;
				break;
			default:
				pr_err("fail to support store format %d\n", frm_store->color_fmt);
				break;
			}
			slc_store->addr.addr_ch0 = frm_store->addr.addr_ch0 + ch0_offset;
			slc_store->addr.addr_ch1 = frm_store->addr.addr_ch1 + ch1_offset;
			slc_store->addr.addr_ch2 = frm_store->addr.addr_ch2 + ch2_offset;
			pr_debug("addr_ch0 %x, addr_ch1 %x\n", slc_store->addr.addr_ch0, slc_store->addr.addr_ch1);
		}
	}

	return 0;
}

static int ispslice_afbc_store_info_cfg(
		void *cfg_in, struct isp_slice_context *slc_ctx)
{
	int i = 0, j = 0, slice_id = 0;
	uint32_t slice_col_num = 0;
	uint32_t slice_start_col = 0;
	uint32_t cur_slice_row = 0;
	uint32_t cur_slice_col = 0;
	uint32_t overlap_left = 0;
	uint32_t overlap_up = 0;
	uint32_t overlap_down = 0;
	uint32_t overlap_right = 0;
	uint32_t start_row = 0, end_row = 0,
		start_col = 0, end_col = 0;
	uint32_t scl_out_width = 0, scl_out_height = 0;
	uint32_t tmp_slice_id = 0;
	uint32_t slice_start_col_tile_num = 0;

	struct slice_cfg_input *in_ptr = (struct slice_cfg_input *)cfg_in;
	struct isp_afbc_store_info *frm_afbc_store = NULL;
	struct isp_slice_desc *cur_slc = NULL;
	struct isp_slice_desc *cur_slc_temp = NULL;
	struct slice_afbc_store_info *slc_afbc_store = NULL;
	struct slice_scaler_info *slc_scaler = NULL;

	slice_col_num = slc_ctx->slice_col_num;
	cur_slc = &slc_ctx->slices[0];
	cur_slc_temp = &slc_ctx->slices[0];
	for (i = 0; i < SLICE_NUM_MAX; i++, cur_slc++) {
		if (cur_slc->valid == 0)
			continue;
		for (j = 0; j < AFBC_PATH_NUM; j++) {
			frm_afbc_store = in_ptr->frame_afbc_store[j];
			if (frm_afbc_store == NULL || cur_slc->path_en[j] == 0
				|| frm_afbc_store->bypass)
				/* path is not valid. */
				continue;
			slc_afbc_store = &cur_slc->slice_afbc_store[j];
			slc_scaler = &cur_slc->slice_scaler[j];
			slice_id = i;
			cur_slice_row = slice_id / slice_col_num;
			cur_slice_col = slice_id % slice_col_num;
			if (slc_scaler->scaler_bypass == 1) {
				overlap_left = cur_slc->slice_overlap.overlap_left;
				overlap_up = cur_slc->slice_overlap.overlap_up;
				overlap_right = cur_slc->slice_overlap.overlap_right;
				overlap_down = cur_slc->slice_overlap.overlap_down;

				slc_afbc_store->border.up_border = overlap_up;
				slc_afbc_store->border.left_border = overlap_left;
				slc_afbc_store->border.down_border = overlap_down;
				slc_afbc_store->border.right_border = overlap_right;

				start_row = cur_slc->slice_pos.start_row;
				end_row = cur_slc->slice_pos.end_row;
				start_col = cur_slc->slice_pos.start_col;
				end_col = cur_slc->slice_pos.end_col;

				slc_afbc_store->size.w = end_col - start_col + 1
					- overlap_left - overlap_right;
				slc_afbc_store->size.h = end_row - start_row + 1
					- overlap_up - overlap_down;
				slice_start_col = start_col + overlap_left;
			} else {
				overlap_up = 0;
				overlap_left = 0;
				overlap_down = 0;
				overlap_right = 0;
				scl_out_width = slc_scaler->trim1_size_x;
				scl_out_height = slc_scaler->trim1_size_y;

				slc_afbc_store->border.up_border = overlap_up;
				slc_afbc_store->border.left_border = overlap_left;
				slc_afbc_store->border.down_border = overlap_down;
				slc_afbc_store->border.right_border = overlap_right;

				slc_afbc_store->size.h =
					scl_out_height - overlap_up - overlap_down;
				slc_afbc_store->size.w =
					scl_out_width - overlap_left - overlap_right;

				tmp_slice_id = slice_id;
				slice_start_col = 0;
				while ((int)(tmp_slice_id - 1) >=
					(int)(cur_slice_row * slice_col_num)) {
					tmp_slice_id--;
					slice_start_col +=
						(cur_slc_temp[tmp_slice_id].slice_afbc_store[j].size.w +
							AFBC_PADDING_W_YUV420 - 1) /
							AFBC_PADDING_W_YUV420 *
							AFBC_PADDING_W_YUV420;
				}
			}
			slice_start_col_tile_num = slice_start_col /
					AFBC_PADDING_W_YUV420;
			slc_afbc_store->yheader_addr =
				frm_afbc_store->yheader +
					slice_start_col_tile_num * AFBC_HEADER_SIZE;
			slc_afbc_store->yaddr =
				frm_afbc_store->yaddr +
					slice_start_col_tile_num * AFBC_PAYLOAD_SIZE;
			slc_afbc_store->slice_offset =
				frm_afbc_store->header_offset +
					slice_start_col_tile_num * AFBC_PAYLOAD_SIZE;
			slc_afbc_store->slc_afbc_on = 1;
			pr_debug("slice afbc yheader = %x\n",
				slc_afbc_store->yheader_addr);
			pr_debug("slice afbc yaddr = %x\n",
				slc_afbc_store->yaddr);
			pr_debug("slice afbc offset = %x\n",
				slc_afbc_store->slice_offset);
			pr_debug("slice afbc on = %x\n",
				slc_afbc_store->slc_afbc_on);
		}
	}

	return 0;
}

static int ispslice_3dnr_memctrl_info_cfg(
		void *cfg_in, struct isp_slice_context *slc_ctx)
{
	int ret = 0, idx = 0;
	uint32_t start_col = 0, end_col = 0;
	uint32_t start_row = 0, end_row = 0;
	uint32_t pitch_y = 0, pitch_u = 0;
	uint32_t ch0_offset = 0, ch1_offset = 0;
	struct slice_cfg_input *in_ptr = (struct slice_cfg_input *)cfg_in;
	struct isp_3dnr_ctx_desc *nr3_ctx = in_ptr->nr3_ctx;
	struct isp_3dnr_mem_ctrl *mem_ctrl = &nr3_ctx->mem_ctrl;
	struct isp_slice_desc *cur_slc = NULL;
	struct slice_3dnr_memctrl_info *slc_3dnr_memctrl = NULL;
	struct slice_3dnr_store_info *slc_3dnr_store = NULL;

	pitch_y = in_ptr->frame_in_size.w;
	pitch_u = in_ptr->frame_in_size.w;
	cur_slc = &slc_ctx->slices[0];
	for (idx = 0; idx < SLICE_NUM_MAX; idx++, cur_slc++) {
		if (cur_slc->valid == 0)
			continue;
		slc_3dnr_memctrl = &cur_slc->slice_3dnr_memctrl;
		slc_3dnr_store = &cur_slc->slice_3dnr_store;

		start_col = cur_slc->slice_pos.start_col;
		start_row = cur_slc->slice_pos.start_row;
		end_col = cur_slc->slice_pos.end_col;
		end_row = cur_slc->slice_pos.end_row;

		slc_3dnr_memctrl->first_line_mode = 0;
		slc_3dnr_memctrl->last_line_mode = 0;

		if (slc_ctx->slice_num > 1) {
			if (idx == 0)
				slc_3dnr_memctrl->slice_info = 1;
			else if (idx == slc_ctx->slice_num - 1)
				slc_3dnr_memctrl->slice_info = 2;
			else
				slc_3dnr_memctrl->slice_info = 3;
		}

		slc_3dnr_memctrl->roi_mode = 0;
		slc_3dnr_memctrl->retain_num = 32;
		slc_3dnr_memctrl->ft_max_len_sel = 1;
		slc_3dnr_memctrl->data_toyuv_en = 1;
		slc_3dnr_memctrl->back_toddr_en = 1;
		slc_3dnr_memctrl->chk_sum_clr_en = 1;
		slc_3dnr_memctrl->bypass = mem_ctrl->bypass;
		slc_3dnr_memctrl->ref_pic_flag = mem_ctrl->ref_pic_flag;
		slc_3dnr_memctrl->yuv_8bits_flag = mem_ctrl->yuv_8bits_flag;
		slc_3dnr_memctrl->nr3_ft_path_sel = mem_ctrl->nr3_ft_path_sel ;
		slc_3dnr_memctrl->nr3_done_mode = mem_ctrl->nr3_done_mode;
		if (nr3_ctx->mem_ctrl.bypass)
			slc_3dnr_memctrl->nr3_ft_path_sel = 1;

		/* blending cnt reset 0 than ++ by nr3 frame cfg */
		if (nr3_ctx->blending_cnt == 1)
			slc_3dnr_memctrl->ref_pic_flag = 0;
		else
			slc_3dnr_memctrl->ref_pic_flag = 1;

		slc_3dnr_memctrl->start_row = start_row;
		slc_3dnr_memctrl->start_col = start_col;
		slc_3dnr_memctrl->src.h = end_row - start_row + 1;
		slc_3dnr_memctrl->src.w = end_col - start_col + 1;
		slc_3dnr_memctrl->ft_y.h = end_row - start_row + 1;
		slc_3dnr_memctrl->ft_y.w = end_col - start_col + 1;
		slc_3dnr_memctrl->ft_uv.h = (end_row - start_row + 1) >> 1;
		slc_3dnr_memctrl->ft_uv.w = end_col - start_col + 1;

		if (nr3_ctx->type == NR3_FUNC_OFF)
			slc_3dnr_memctrl->bypass = 1;
		else {
			/* YUV420_2FRAME */
			ch0_offset = start_row * pitch_y + start_col;
			ch1_offset = ((start_row * pitch_u + 1) >> 1) + start_col;
			if (nr3_ctx->nr3_mv_version == ALG_NR3_MV_VER_0) {
				slc_3dnr_memctrl->addr.addr_ch0 = mem_ctrl->frame_addr.addr_ch0 + ch0_offset;
				slc_3dnr_memctrl->addr.addr_ch1 = mem_ctrl->frame_addr.addr_ch1 + ch1_offset;
			} else {
				slc_3dnr_memctrl->addr.addr_ch0 = mem_ctrl->frame_addr.addr_ch0;
				slc_3dnr_memctrl->addr.addr_ch1 = mem_ctrl->frame_addr.addr_ch1;
				slc_3dnr_store->addr.addr_ch0 = nr3_ctx->nr3_store.st_luma_addr;
				slc_3dnr_store->addr.addr_ch1 = nr3_ctx->nr3_store.st_chroma_addr;
			}
			slc_3dnr_memctrl->bypass = mem_ctrl->bypass;
		}
		pr_debug("memctrl param w[%d], h[%d] bypass %d\n",
			slc_3dnr_memctrl->src.w,
			slc_3dnr_memctrl->src.h,
			slc_3dnr_memctrl->bypass);
	}

	return ret;
}

static int ispslice_3dnr_fbd_fetch_info_cfg(
		void *cfg_in, struct isp_slice_context *slc_ctx)
{
	int ret = 0, idx = 0;
	uint32_t start_col, end_col, start_row, end_row;
	uint32_t overlap_up, overlap_down, overlap_left, overlap_right;

	uint32_t fetch_start_x, fetch_start_y, uv_fetch_start_y;
	uint32_t left_tiles_num, right_tiles_num, middle_tiles_num;
	uint32_t left_size, right_size, up_size, down_size;
	uint32_t up_tiles_num, down_tiles_num, vertical_middle_tiles_num;
	uint32_t left_offset_tiles_num, up_offset_tiles_num;

	int Y_start_x, Y_end_x, Y_start_y, Y_end_y;
	int UV_start_x, UV_end_x, UV_start_y, UV_end_y;
	int mv_x, mv_y;

	uint32_t slice_width, slice_height;
	uint32_t pad_slice_width, pad_slice_height;
	uint32_t tile_col, tile_row;
	uint32_t img_width, fbd_y_tiles_num_pitch;

	struct slice_cfg_input *in_ptr = (struct slice_cfg_input *)cfg_in;
	struct isp_3dnr_ctx_desc *nr3_ctx = in_ptr->nr3_ctx;
	struct isp_3dnr_fbd_fetch *nr3_fbd_fetch = &nr3_ctx->nr3_fbd_fetch;
	struct isp_slice_desc *cur_slc;
	struct slice_3dnr_fbd_fetch_info *slc_3dnr_fbd_fetch;

	img_width = nr3_ctx->mem_ctrl.img_width;
	fbd_y_tiles_num_pitch = (img_width + FBD_NR3_Y_WIDTH - 1) / FBD_NR3_Y_WIDTH;

	mv_x = nr3_ctx->mem_ctrl.mv_x;
	mv_y = nr3_ctx->mem_ctrl.mv_y;

	cur_slc = &slc_ctx->slices[0];
	for (idx = 0; idx < SLICE_NUM_MAX; idx++, cur_slc++) {
		if (cur_slc->valid == 0)
			continue;
		slc_3dnr_fbd_fetch = &cur_slc->slice_3dnr_fbd_fetch;

		start_col = cur_slc->slice_pos.start_col;
		end_col = cur_slc->slice_pos.end_col;
		start_row = cur_slc->slice_pos.start_row;
		end_row = cur_slc->slice_pos.end_row;
		overlap_up = cur_slc->slice_overlap.overlap_up;
		overlap_down = cur_slc->slice_overlap.overlap_down;
		overlap_left = cur_slc->slice_overlap.overlap_left;
		overlap_right = cur_slc->slice_overlap.overlap_right;

		slice_width = end_col - start_col + 1;
		slice_height = end_row - start_row + 1;
		pad_slice_width = (slice_width + FBD_NR3_Y_WIDTH - 1) /
			FBD_NR3_Y_WIDTH * FBD_NR3_Y_WIDTH;
		pad_slice_height = (slice_height + FBD_NR3_Y_PAD_HEIGHT - 1) /
			FBD_NR3_Y_PAD_HEIGHT * FBD_NR3_Y_PAD_HEIGHT;
		tile_col = pad_slice_width / FBD_NR3_Y_WIDTH;
		tile_row = pad_slice_height / FBD_NR3_Y_HEIGHT;

		slc_3dnr_fbd_fetch->fbd_y_pixel_size_in_hor = slice_width;
		slc_3dnr_fbd_fetch->fbd_y_pixel_size_in_ver = slice_height;
		slc_3dnr_fbd_fetch->fbd_c_pixel_size_in_hor = slice_width;
		slc_3dnr_fbd_fetch->fbd_c_pixel_size_in_ver = slice_height / 2;
		slc_3dnr_fbd_fetch->fbd_y_pixel_start_in_ver = mv_y & 0x1;
		slc_3dnr_fbd_fetch->fbd_c_pixel_start_in_ver = (mv_y / 2) & 0x1;

		fetch_start_x = (mv_x < 0) ? start_col : (start_col + mv_x);
		fetch_start_y = (mv_y < 0) ? start_row : (start_row + mv_y);
		uv_fetch_start_y = (mv_y < 0) ? start_row : (start_row + mv_y / 2);

		Y_start_x = fetch_start_x;
		UV_start_x = fetch_start_x;
		Y_start_y = fetch_start_y;
		UV_start_y = uv_fetch_start_y;
		if (mv_x < 0) {
			if ((start_col == 0) && (mv_x & 1)) {
				Y_start_x = fetch_start_x;
				UV_start_x = fetch_start_x + 2;
			} else if ((start_col != 0) && (mv_x & 1)) {
				Y_start_x = fetch_start_x + mv_x;
				UV_start_x = fetch_start_x + mv_x + 1;
			} else if (start_col != 0) {
				Y_start_x = fetch_start_x + mv_x;
				UV_start_x = fetch_start_x + mv_x;
			} else {
				Y_start_x = fetch_start_x;
				UV_start_x = fetch_start_x;
			}
		} else if (mv_x > 0) {
			if ((start_col == 0) && (mv_x & 1)) {
				Y_start_x = fetch_start_x;
				UV_start_x = fetch_start_x - 1;
			} else if ((start_col != 0) && (mv_x & 1)) {
				Y_start_x = fetch_start_x;
				UV_start_x = fetch_start_x - 1;
			} else {
				Y_start_x = fetch_start_x;
				UV_start_x = fetch_start_x;
			}
		}
		Y_end_x = slice_width + Y_start_x - 1;
		UV_end_x = slice_width + UV_start_x - 1;
		left_offset_tiles_num = Y_start_x / FBD_NR3_Y_WIDTH;
		if (Y_start_x % FBD_NR3_Y_WIDTH == 0) {
			left_tiles_num = 0;
			left_size = 0;
		} else {
			left_tiles_num = 1;
			left_size = FBD_NR3_Y_WIDTH - Y_start_x % FBD_NR3_Y_WIDTH;
		}
		if (((Y_end_x + 1) % FBD_NR3_Y_WIDTH == 0)
			|| (((Y_end_x + 1) > img_width)
			&& ((Y_end_x + 1) % FBD_NR3_Y_WIDTH == 1)))
			right_tiles_num = 0;
		else
			right_tiles_num = 1;
		right_size = (Y_end_x + 1) % FBD_NR3_Y_WIDTH;

		middle_tiles_num = (slice_width - left_size - right_size) / FBD_NR3_Y_WIDTH;
		slc_3dnr_fbd_fetch->fbd_y_pixel_start_in_hor = Y_start_x % FBD_NR3_Y_WIDTH;
		slc_3dnr_fbd_fetch->fbd_y_tiles_num_in_hor = left_tiles_num + right_tiles_num + middle_tiles_num;
		slc_3dnr_fbd_fetch->fbd_y_tiles_start_odd = left_offset_tiles_num % 2;
		left_offset_tiles_num = UV_start_x / FBD_NR3_Y_WIDTH;
		if (UV_start_x % FBD_NR3_Y_WIDTH == 0) {
			left_tiles_num = 0;
			left_size = 0;
		} else {
			left_tiles_num = 1;
			left_size = FBD_NR3_Y_WIDTH - UV_start_x % FBD_NR3_Y_WIDTH;
		}
		if ((UV_end_x + 1) % FBD_NR3_Y_WIDTH == 0)
			right_tiles_num = 0;
		else
			right_tiles_num = 1;
		right_size = (UV_end_x + 1) % FBD_NR3_Y_WIDTH;
		middle_tiles_num = (slice_width - left_size - right_size) / FBD_NR3_Y_WIDTH;
		slc_3dnr_fbd_fetch->fbd_c_pixel_start_in_hor = UV_start_x % FBD_NR3_Y_WIDTH;
		slc_3dnr_fbd_fetch->fbd_c_tiles_num_in_hor = left_tiles_num + right_tiles_num + middle_tiles_num;
		slc_3dnr_fbd_fetch->fbd_c_tiles_start_odd = left_offset_tiles_num % 2;
		if (mv_y < 0) {
			Y_start_y = fetch_start_y;
			UV_start_y = fetch_start_y;
		} else if (mv_y > 0) {
			Y_start_y = fetch_start_y;
			UV_start_y = uv_fetch_start_y;
		}
		Y_end_y = slice_height + Y_start_y - 1;
		UV_end_y = slice_height + UV_start_y - 1;
		up_offset_tiles_num = Y_start_y / FBD_NR3_Y_HEIGHT;
		if (Y_start_y % FBD_NR3_Y_HEIGHT == 0) {
			up_tiles_num = 0;
			up_size = 0;
		} else {
			up_tiles_num = 1;
			up_size = FBD_NR3_Y_HEIGHT - Y_start_y % FBD_NR3_Y_HEIGHT;
		}
		if ((Y_end_y + 1) % FBD_NR3_Y_HEIGHT == 0)
			down_tiles_num = 0;
		else
			down_tiles_num = 1;
		down_size = (Y_end_y + 1) % FBD_NR3_Y_HEIGHT;
		vertical_middle_tiles_num = (slice_height - up_size - down_size) / FBD_NR3_Y_HEIGHT;
		slc_3dnr_fbd_fetch->fbd_y_pixel_start_in_ver = Y_start_y % FBD_NR3_Y_HEIGHT;
		slc_3dnr_fbd_fetch->fbd_y_tiles_num_in_ver = up_tiles_num + down_tiles_num + vertical_middle_tiles_num;
		slc_3dnr_fbd_fetch->fbd_y_header_addr_init = nr3_fbd_fetch->y_header_addr_init
			- (left_offset_tiles_num + up_offset_tiles_num * fbd_y_tiles_num_pitch) / 2;
		slc_3dnr_fbd_fetch->fbd_y_tile_addr_init_x256 = nr3_fbd_fetch->y_tile_addr_init_x256
			+ (left_offset_tiles_num + up_offset_tiles_num * fbd_y_tiles_num_pitch) * FBC_NR3_BASE_ALIGN;
		up_offset_tiles_num = UV_start_y / FBD_NR3_Y_HEIGHT;
		if (UV_start_y % FBD_BAYER_HEIGHT == 0) {
			up_tiles_num = 0;
			up_size = 0;
		} else {
			up_tiles_num = 1;
			up_size = FBD_NR3_Y_HEIGHT - UV_start_y % FBD_NR3_Y_HEIGHT;
		}
		if ((UV_end_y + 1) % FBD_NR3_Y_HEIGHT == 0)
			down_tiles_num = 0;
		else
			down_tiles_num = 1;
		down_size = (UV_end_y + 1) % FBD_NR3_Y_HEIGHT;
		vertical_middle_tiles_num = (slice_height / 2 - up_size - down_size) / FBD_NR3_Y_HEIGHT;
		slc_3dnr_fbd_fetch->fbd_c_pixel_start_in_ver = UV_start_y % FBD_NR3_Y_WIDTH;
		slc_3dnr_fbd_fetch->fbd_c_tiles_num_in_ver = up_tiles_num + down_tiles_num
			+ vertical_middle_tiles_num;
		slc_3dnr_fbd_fetch->fbd_c_header_addr_init = nr3_fbd_fetch->c_header_addr_init
			- (left_offset_tiles_num + up_offset_tiles_num * fbd_y_tiles_num_pitch) / 2;
		slc_3dnr_fbd_fetch->fbd_c_tile_addr_init_x256 = nr3_fbd_fetch->c_tile_addr_init_x256
			+ (left_offset_tiles_num + up_offset_tiles_num * fbd_y_tiles_num_pitch) * FBC_NR3_BASE_ALIGN;

		slc_3dnr_fbd_fetch->fbd_y_tiles_num_pitch = nr3_fbd_fetch->y_tiles_num_pitch;

		/*This is for N6pro*/
		slc_3dnr_fbd_fetch->bypass = nr3_fbd_fetch->bypass;
		slc_3dnr_fbd_fetch->afbc_mode = nr3_fbd_fetch->afbc_mode;
		slc_3dnr_fbd_fetch->color_fmt = nr3_fbd_fetch->color_fmt;
		slc_3dnr_fbd_fetch->hblank_en = nr3_fbd_fetch->hblank_en;
		slc_3dnr_fbd_fetch->slice_width = end_col - start_col + 1;
		slc_3dnr_fbd_fetch->slice_height = end_row - start_row + 1;
		slc_3dnr_fbd_fetch->hblank_num = nr3_fbd_fetch->hblank_num;
		slc_3dnr_fbd_fetch->tile_num_pitch = nr3_fbd_fetch->tile_num_pitch;
		slc_3dnr_fbd_fetch->start_3dnr_afbd = nr3_fbd_fetch->start_3dnr_afbd;
		slc_3dnr_fbd_fetch->chk_sum_auto_clr = nr3_fbd_fetch->chk_sum_auto_clr;
		slc_3dnr_fbd_fetch->slice_start_pxl_xpt = start_col;
		slc_3dnr_fbd_fetch->slice_start_pxl_ypt = start_row;
		slc_3dnr_fbd_fetch->dout_req_signal_type = nr3_fbd_fetch->dout_req_signal_type;
		slc_3dnr_fbd_fetch->slice_start_header_addr = nr3_fbd_fetch->slice_start_header_addr
			+ ((start_row / ISP_FBD_TILE_HEIGHT) * nr3_fbd_fetch->tile_num_pitch + start_col / ISP_FBD_TILE_WIDTH) * FBC_NR3_HEADER_SIZE ;
		slc_3dnr_fbd_fetch->frame_header_base_addr = nr3_fbd_fetch->frame_header_base_addr;

	}

	return ret;
}

static int ispslice_3dnr_store_info_cfg(
		void *cfg_in, struct isp_slice_context *slc_ctx)
{
	int ret = 0, idx = 0;

	/* struct slice_overlap_info */
	uint32_t overlap_left  = 0;
	uint32_t overlap_up    = 0;
	uint32_t overlap_right = 0;
	uint32_t overlap_down  = 0;
	/* struct slice_pos_info */
	uint32_t start_col     = 0;
	uint32_t end_col       = 0;
	uint32_t start_row     = 0;
	uint32_t end_row       = 0;

	/* struct slice_pos_info */
	uint32_t orig_s_col    = 0;
	uint32_t orig_s_row    = 0;
	uint32_t orig_e_col    = 0;
	uint32_t orig_e_row    = 0;

	uint32_t pitch_y       = 0;
	uint32_t pitch_u       = 0;
	uint32_t ch0_offset    = 0;
	uint32_t ch1_offset    = 0;

	struct slice_cfg_input *in_ptr = (struct slice_cfg_input *)cfg_in;
	struct isp_3dnr_ctx_desc *nr3_ctx = in_ptr->nr3_ctx;
	struct isp_3dnr_store *nr3_store = &nr3_ctx->nr3_store;
	struct isp_slice_desc *cur_slc = NULL;

	struct slice_3dnr_store_info *slc_3dnr_store = NULL;

	pitch_y = in_ptr->frame_in_size.w;
	pitch_u = in_ptr->frame_in_size.w;

	cur_slc = &slc_ctx->slices[0];
	if (nr3_ctx->type == NR3_FUNC_OFF) {
		for (idx = 0; idx < SLICE_NUM_MAX; idx++, cur_slc++) {
			if (cur_slc->valid == 0)
				continue;
			slc_3dnr_store = &cur_slc->slice_3dnr_store;
			slc_3dnr_store->bypass = 1;
		}

		return 0;
	}

	for (idx = 0; idx < SLICE_NUM_MAX; idx++, cur_slc++) {
		if (cur_slc->valid == 0)
			continue;
		slc_3dnr_store = &cur_slc->slice_3dnr_store;

		start_col = cur_slc->slice_pos.start_col;
		start_row = cur_slc->slice_pos.start_row;
		end_col = cur_slc->slice_pos.end_col;
		end_row = cur_slc->slice_pos.end_row;

		orig_s_col = cur_slc->slice_pos_orig.start_col;
		orig_s_row = cur_slc->slice_pos_orig.start_row;
		orig_e_col = cur_slc->slice_pos_orig.end_col;
		orig_e_row = cur_slc->slice_pos_orig.end_row;

		overlap_left = cur_slc->slice_overlap.overlap_left;
		overlap_up = cur_slc->slice_overlap.overlap_up;
		overlap_right = cur_slc->slice_overlap.overlap_right;
		overlap_down = cur_slc->slice_overlap.overlap_down;

		slc_3dnr_store->bypass = nr3_store->st_bypass;
		if (nr3_ctx->nr3_mv_version == ALG_NR3_MV_VER_0) {
			/* YUV420_2FRAME */
			ch0_offset = orig_s_row * pitch_y + orig_s_col;
			ch1_offset = ((orig_s_row * pitch_u + 1) >> 1) + orig_s_col;

			slc_3dnr_store->addr.addr_ch0 = nr3_store->st_luma_addr +
				ch0_offset;
			slc_3dnr_store->addr.addr_ch1 = nr3_store->st_chroma_addr +
				ch1_offset;

			slc_3dnr_store->size.w = orig_e_col - orig_s_col + 1;
			slc_3dnr_store->size.h = orig_e_row - orig_s_row + 1;
		}
		pr_debug("store w[%d], h[%d] bypass %d\n", slc_3dnr_store->size.w,
			slc_3dnr_store->size.h, slc_3dnr_store->bypass);
	}

	return ret;
}

static int ispslice_3dnr_fbc_store_info_cfg(
		void *cfg_in, struct isp_slice_context *slc_ctx)
{
	int ret = 0, idx = 0;

	uint32_t slice_width, slice_height;
	uint32_t store_slice_width, store_slice_height;
	uint32_t uv_tile_w_num, uv_tile_h_num;
	uint32_t y_tile_w_num, y_tile_h_num;
	uint32_t store_left_offset_tiles_num;
	uint32_t start_col, end_col, start_row, end_row;
	uint32_t overlap_up, overlap_down, overlap_left, overlap_right;
	struct slice_cfg_input *in_ptr = (struct slice_cfg_input *)cfg_in;
	struct isp_3dnr_ctx_desc *nr3_ctx = in_ptr->nr3_ctx;
	struct isp_3dnr_fbc_store *nr3_fbc_store = &nr3_ctx->nr3_fbc_store;
	struct isp_slice_desc *cur_slc;
	struct slice_3dnr_fbc_store_info *slc_3dnr_fbc_store;

	cur_slc = &slc_ctx->slices[0];

	if (nr3_ctx->type == NR3_FUNC_OFF) {
		for (idx = 0; idx < SLICE_NUM_MAX; idx++, cur_slc++) {
			if (cur_slc->valid == 0)
				continue;
			slc_3dnr_fbc_store = &cur_slc->slice_3dnr_fbc_store;
			slc_3dnr_fbc_store->bypass = 1;
		}

		return 0;
	}

	for (idx = 0; idx < SLICE_NUM_MAX; idx++, cur_slc++) {
		if (cur_slc->valid == 0)
			continue;
		slc_3dnr_fbc_store = &cur_slc->slice_3dnr_fbc_store;

		start_col = cur_slc->slice_pos.start_col;
		end_col = cur_slc->slice_pos.end_col;
		start_row = cur_slc->slice_pos.start_row;
		end_row = cur_slc->slice_pos.end_row;
		overlap_up = cur_slc->slice_overlap.overlap_up;
		overlap_down = cur_slc->slice_overlap.overlap_down;
		overlap_left = cur_slc->slice_overlap.overlap_left;
		overlap_right = cur_slc->slice_overlap.overlap_right;

		slice_width = end_col - start_col + 1;
		slice_height = end_row - start_row + 1;
		store_slice_width = slice_width - overlap_left - overlap_right;
		store_slice_height = slice_height - overlap_up - overlap_down;

		store_left_offset_tiles_num = (start_col + overlap_left) / FBC_NR3_Y_WIDTH;

		uv_tile_w_num = (store_slice_width + FBC_NR3_Y_WIDTH - 1) / FBC_NR3_Y_WIDTH;
		uv_tile_w_num = (uv_tile_w_num + 2 - 1) / 2 * 2;
		uv_tile_h_num = (store_slice_height / 2 + FBC_NR3_Y_HEIGHT - 1) / FBC_NR3_Y_HEIGHT;
		y_tile_w_num = uv_tile_w_num;
		y_tile_h_num = 2 * uv_tile_h_num;

		slc_3dnr_fbc_store->fbc_tile_number = uv_tile_w_num * uv_tile_h_num +
			y_tile_w_num * y_tile_h_num;
		slc_3dnr_fbc_store->fbc_size_in_ver = store_slice_height;
		slc_3dnr_fbc_store->fbc_size_in_hor = store_slice_width;
		slc_3dnr_fbc_store->fbc_y_tile_addr_init_x256 = (uint32_t)nr3_fbc_store->y_tile_addr_init_x256
			+ store_left_offset_tiles_num * FBC_NR3_BASE_ALIGN;
		slc_3dnr_fbc_store->fbc_c_tile_addr_init_x256 = (uint32_t)nr3_fbc_store->c_tile_addr_init_x256
			+ store_left_offset_tiles_num * FBC_NR3_BASE_ALIGN;
		slc_3dnr_fbc_store->fbc_y_header_addr_init = nr3_fbc_store->y_header_addr_init
			- store_left_offset_tiles_num / 2;
		slc_3dnr_fbc_store->fbc_c_header_addr_init = nr3_fbc_store->c_header_addr_init
			- store_left_offset_tiles_num / 2;
		slc_3dnr_fbc_store->slice_mode_en = 1;
		slc_3dnr_fbc_store->bypass = nr3_fbc_store->bypass;

		/*This is for N6pro*/
		slc_3dnr_fbc_store->slice_payload_base_addr = nr3_fbc_store->slice_payload_base_addr + start_col * FBC_NR3_PAYLOAD_YUV10_SIZE;
		slc_3dnr_fbc_store->slice_header_base_addr = nr3_fbc_store->slice_header_base_addr + start_col * FBC_NR3_HEADER_SIZE;
		slc_3dnr_fbc_store->slice_payload_offset_addr_init = slc_3dnr_fbc_store->slice_payload_base_addr -
			slc_3dnr_fbc_store->slice_header_base_addr + start_col * FBC_NR3_PAYLOAD_YUV10_SIZE;
		slc_3dnr_fbc_store->y_nearly_full_level = nr3_fbc_store->y_nearly_full_level;
		slc_3dnr_fbc_store->c_nearly_full_level = nr3_fbc_store->c_nearly_full_level;
		slc_3dnr_fbc_store->tile_num_pitch = nr3_fbc_store->tile_number_pitch;
		slc_3dnr_fbc_store->color_format = nr3_fbc_store->color_format;
		slc_3dnr_fbc_store->fbc_size_in_ver = end_row - start_row + 1;
		slc_3dnr_fbc_store->fbc_size_in_hor = end_col - start_col + 1;
		slc_3dnr_fbc_store->afbc_mode = nr3_fbc_store->afbc_mode;
		slc_3dnr_fbc_store->mirror_en = nr3_fbc_store->mirror_en;
		slc_3dnr_fbc_store->endian = nr3_fbc_store->endian;
		slc_3dnr_fbc_store->left_border = nr3_fbc_store->left_border;
		slc_3dnr_fbc_store->up_border = nr3_fbc_store->up_border;

		pr_debug("[%s] [slice id %d] tile_number %d, size:(%d, %d)\n", __func__,
			idx, slc_3dnr_fbc_store->fbc_tile_number, slc_3dnr_fbc_store->fbc_size_in_ver, slc_3dnr_fbc_store->fbc_size_in_hor);
	}

	return ret;
}

static int ispslice_3dnr_crop_info_cfg(
		void *cfg_in, struct isp_slice_context *slc_ctx)
{
	int ret = 0;
	int idx = 0;

	struct slice_cfg_input *in_ptr = (struct slice_cfg_input *)cfg_in;
	struct isp_3dnr_ctx_desc *nr3_ctx  = in_ptr->nr3_ctx;

	/* struct slice_overlap_info */
	uint32_t overlap_left  = 0;
	uint32_t overlap_up    = 0;
	uint32_t overlap_right = 0;
	uint32_t overlap_down  = 0;

	struct isp_slice_desc *cur_slc = NULL;

	struct slice_3dnr_memctrl_info *slc_3dnr_memctrl = NULL;
	struct slice_3dnr_store_info *slc_3dnr_store = NULL;
	struct slice_3dnr_crop_info *slc_3dnr_crop = NULL;

	cur_slc = &slc_ctx->slices[0];

	if (nr3_ctx->type == NR3_FUNC_OFF) {
		for (idx = 0; idx < SLICE_NUM_MAX; idx++, cur_slc++) {
			if (cur_slc->valid == 0)
				continue;
			slc_3dnr_crop = &cur_slc->slice_3dnr_crop;
			slc_3dnr_crop->bypass = 1;
		}

		return 0;
	}

	for (idx = 0; idx < SLICE_NUM_MAX; idx++, cur_slc++) {
		if (cur_slc->valid == 0)
			continue;
		slc_3dnr_memctrl = &cur_slc->slice_3dnr_memctrl;
		slc_3dnr_store = &cur_slc->slice_3dnr_store;
		slc_3dnr_crop = &cur_slc->slice_3dnr_crop;

		overlap_left = cur_slc->slice_overlap.overlap_left;
		overlap_up = cur_slc->slice_overlap.overlap_up;
		overlap_right = cur_slc->slice_overlap.overlap_right;
		overlap_down = cur_slc->slice_overlap.overlap_down;

		slc_3dnr_crop->src.w = slc_3dnr_memctrl->src.w;
		slc_3dnr_crop->src.h = slc_3dnr_memctrl->src.h;
		slc_3dnr_crop->dst.w = slc_3dnr_store->size.w;
		slc_3dnr_crop->dst.h = slc_3dnr_store->size.h;

		if (nr3_ctx->nr3_mv_version == ALG_NR3_MV_VER_0) {
			slc_3dnr_crop->bypass = !(overlap_left || overlap_up ||
				overlap_right || overlap_down);
			slc_3dnr_crop->start_x = overlap_left;
			slc_3dnr_crop->start_y = overlap_up;
		}

		pr_debug("src.w[%d], src.h[%d], des.w[%d], des.h[%d]",
			slc_3dnr_crop->src.w, slc_3dnr_crop->src.h,
			slc_3dnr_crop->dst.w, slc_3dnr_crop->dst.h);
		pr_debug("ovx[%d], ovx[%d] bypass %d\n",
			slc_3dnr_crop->start_x, slc_3dnr_crop->start_y,
			slc_3dnr_crop->bypass);
	}

	return ret;
}

int alg_nr3_memctrl_slice_info_update_ver0(struct nr3_slice *in,
		struct nr3_slice_for_blending *out)
{
	uint32_t end_row = 0, end_col = 0, ft_pitch = 0;
	int mv_x = 0, mv_y = 0;
	uint32_t global_img_width = 0, global_img_height = 0;

	if (!in || !out) {
		pr_err("fail to get valid input ptr in %p, out %p\n", in, out);
		return -EFAULT;
	}

	end_row = in->end_row;
	end_col = in->end_col;
	ft_pitch = in->cur_frame_width;
	mv_x = in->mv_x;
	mv_y = in->mv_y;
	global_img_width = in->cur_frame_width;
	global_img_height = in->cur_frame_height;

	if (in->slice_num == 1) {
		if (mv_x < 0) {
			if ((mv_x) & 0x1) {
				out->ft_y_width = global_img_width + mv_x + 1;
				out->ft_uv_width = global_img_width + mv_x - 1;
				out->src_chr_addr += 2;
			} else {
				out->ft_y_width = global_img_width + mv_x;
				out->ft_uv_width = global_img_width + mv_x;
			}
		} else if (mv_x > 0) {
			if ((mv_x) & 0x1) {
				out->ft_y_width = global_img_width - mv_x + 1;
				out->ft_uv_width = global_img_width - mv_x + 1;
				out->src_lum_addr += mv_x;
				out->src_chr_addr += mv_x - 1;
			} else {
				out->ft_y_width = global_img_width - mv_x;
				out->ft_uv_width = global_img_width - mv_x;
				out->src_lum_addr += mv_x;
				out->src_chr_addr += mv_x;
			}
		}
	} else { /* slice > 1 */
		if (out->start_col == 0) {
			if (mv_x < 0) {
				if ((mv_x) & 0x1) {
					out->src_chr_addr =
						out->src_chr_addr + 2;
				}
			} else if (mv_x > 0) {
				if ((mv_x) & 0x1) {
					out->src_lum_addr = out->src_lum_addr +
						mv_x;
					out->src_chr_addr = out->src_chr_addr +
						mv_x - 1;
				} else {
					out->src_lum_addr = out->src_lum_addr +
						mv_x;
					out->src_chr_addr = out->src_chr_addr +
						mv_x;
				}
			}
		} else {
			if ((mv_x < 0) && ((mv_x) & 0x1)) {
				out->src_lum_addr = out->src_lum_addr + mv_x;
				out->src_chr_addr = out->src_chr_addr + (
					mv_x / 2) * 2;
			} else if ((mv_x > 0) && ((mv_x) & 0x1)) {
				out->src_lum_addr = out->src_lum_addr + mv_x;
				out->src_chr_addr = out->src_chr_addr + mv_x -
					1;
			} else {
				out->src_lum_addr = out->src_lum_addr + mv_x;
				out->src_chr_addr = out->src_chr_addr + mv_x;
			}
		}
		if (out->start_col == 0) {
			if (mv_x < 0) {
				if ((mv_x) & 0x1) {
					out->ft_y_width = out->ft_y_width + mv_x
						+ 1;
					out->ft_uv_width = out->ft_uv_width +
						mv_x - 1;
				} else {
					out->ft_y_width = out->ft_y_width +
						mv_x;
					out->ft_uv_width = out->ft_uv_width +
						mv_x;
				}
			}
		}
		if ((global_img_width - 1) == end_col) {
			if (mv_x > 0) {
				if ((mv_x) & 0x1) {
					out->ft_y_width = out->ft_y_width -
							mv_x + 1;
					out->ft_uv_width = out->ft_uv_width -
						mv_x + 1;
				} else {
					out->ft_y_width = out->ft_y_width -
						mv_x;
					out->ft_uv_width = out->ft_uv_width -
						mv_x;
				}
			}
		}
	} /* slice_num > 1 */

	if (mv_y < 0) {
		if ((mv_y) & 0x1) {
			out->last_line_mode = 0;
			out->ft_uv_height = global_img_height / 2 + mv_y / 2;
		} else{
			out->last_line_mode = 1;
			out->ft_uv_height = global_img_height / 2 +
				mv_y / 2 + 1;
		}
		out->first_line_mode = 0;
		out->ft_y_height = global_img_height + mv_y;
	} else if (mv_y > 0) {
		if ((mv_y) & 0x1) {
			out->first_line_mode = 1;
			out->last_line_mode = 0;
			out->ft_y_height = global_img_height - mv_y;
			out->ft_uv_height = global_img_height / 2 - (mv_y / 2);
			out->src_lum_addr += ft_pitch * mv_y;
			out->src_chr_addr += ft_pitch * (mv_y / 2);
		} else {
			out->ft_y_height = global_img_height - mv_y;
			out->ft_uv_height = global_img_height / 2 - (mv_y / 2);
			out->src_lum_addr += ft_pitch * mv_y;
			out->src_chr_addr += ft_pitch * (mv_y / 2);
		}
	}

	return 0;
}

int alg_nr3_memctrl_slice_info_update_ver1(struct nr3_slice *in,
	struct nr3_slice_for_blending *out)
{
	uint32_t global_img_width = 0, global_img_height = 0, ft_pitch = 0;
	struct ImageRegion_Info *image_region_info = NULL;

	if (!in || !out) {
		pr_err("fail to get valid input ptr in %p, out %p\n", in, out);
		return -EFAULT;
	}

	out->start_col = in->start_col;
	out->start_row = in->start_row;
	out->src_width = in->end_col -  in->start_col + 1;
	out->src_height = in->end_row - in->start_row + 1;

	image_region_info = in->image_region_info;
	image_region_info->mv_x = in->mv_x;
	image_region_info->mv_y = in->mv_y;
	image_region_info->region_start_row = in->start_row;
	image_region_info->region_end_row = in->end_row;
	image_region_info->region_start_col = in->start_col;
	image_region_info->region_end_col = in->end_col;
	image_region_info->region_width = image_region_info->region_end_col -
		image_region_info->region_start_col + 1;
	image_region_info->region_height= image_region_info->region_end_row -
		image_region_info->region_start_row + 1;

	global_img_width = in->cur_frame_width;
	global_img_height = in->cur_frame_height;
	ft_pitch = in->ft_pitch;

	nr3d_fetch_ref_image_position(image_region_info, global_img_width,
		global_img_height);//fetch position in full size image according to MV.

	out->Y_start_x = image_region_info->Y_start_x;
	out->Y_end_x = image_region_info->Y_end_x;
	out->Y_start_y = image_region_info->Y_start_y;
	out->Y_end_y = image_region_info->Y_end_y;

	if (in->slice_num > 1 && image_region_info->mv_x > 0 && image_region_info->mv_x % 2 != 0) {
		if (in->slice_num == 2 && in->slice_id == 0) {
			out->Y_start_x = image_region_info->Y_start_x - 1;
			out->Y_end_x = image_region_info->Y_end_x + 1;
		} else if (in->slice_num == 2 && in->slice_id == 1) {
			out->Y_start_x = image_region_info->Y_start_x - 1;
			out->Y_end_x = image_region_info->Y_end_x;
		} else if (in->slice_num > 2 && in->slice_id == in->slice_num - 1) {
			out->Y_start_x = image_region_info->Y_start_x - 1;
			out->Y_end_x = image_region_info->Y_end_x;
		} else {
			out->Y_start_x = image_region_info->Y_start_x - 1;
			out->Y_end_x = image_region_info->Y_end_x + 1;
		}
	} else if (in->slice_num > 1 && image_region_info->mv_x < 0 && image_region_info->mv_x % 2 != 0) {
		if (in->slice_num == 2 && in->slice_id == 0) {
			out->Y_start_x = image_region_info->Y_start_x;
			out->Y_end_x = image_region_info->Y_end_x + 1;
		} else if (in->slice_num == 2 && in->slice_id == 1) {
			out->Y_start_x = image_region_info->Y_start_x - 1;
			out->Y_end_x = image_region_info->Y_end_x + 1;
		} else if (in->slice_num > 2 && in->slice_id == 0) {
			out->Y_start_x = image_region_info->Y_start_x;
			out->Y_end_x = image_region_info->Y_end_x + 1;
		} else {
			out->Y_start_x = image_region_info->Y_start_x - 1;
			out->Y_end_x = image_region_info->Y_end_x + 1;
		}
	} else {
		out->Y_start_x = in->image_region_info->Y_start_x;
		out->Y_end_x = in->image_region_info->Y_end_x;
		out->Y_start_y = in->image_region_info->Y_start_y;
		out->Y_end_y = in->image_region_info->Y_end_y;
	}

	if (in->slice_num == 1) {
		if (image_region_info->mv_x < 0) {
			out->UV_start_x = 0;
		} else if (image_region_info->mv_x % 2 == 0) {
			out->UV_start_x = image_region_info->mv_x >> 1;
		} else {
			out->UV_start_x = image_region_info->mv_x >> 1;
		}
		out->UV_end_x = image_region_info->UV_end_x;

		if (image_region_info->mv_y < 0 && image_region_info->mv_y % 2 != 0) {
			out->UV_start_y = 1;
		} else if(image_region_info->mv_y < 0 && image_region_info->mv_y % 2 == 0) {
			out->UV_start_y = 0;
		} else {
			out->UV_start_y = image_region_info->mv_y >> 1;
		}
		out->UV_end_y = image_region_info->UV_end_y;
	} else if (in->slice_num > 1) {
		if (image_region_info->mv_y < 0 && image_region_info->mv_y % 2 != 0) {
			out->UV_start_x = image_region_info->UV_start_x;
			out->UV_end_x = image_region_info->UV_end_x;
			out->UV_start_y = image_region_info->UV_start_y + 1;
			out->UV_end_y = image_region_info->UV_end_y;
		} else {
			out->UV_start_x = image_region_info->UV_start_x;
			out->UV_end_x = image_region_info->UV_end_x;
			out->UV_start_y = image_region_info->UV_start_y;
			out->UV_end_y = image_region_info->UV_end_y;
		}
	}

	if (image_region_info->mv_x < 0 && image_region_info->mv_x % 2 != 0 &&
		in->slice_id == 0)
		out->UV_start_x = image_region_info->UV_start_x + 1;

	if (in->slice_num == 1 && image_region_info->mv_x > 0 && image_region_info->mv_x & 0x1)
		out->Y_start_x -=1;

	out->ft_y_height = out->Y_end_y - out->Y_start_y + 1;
	out->ft_uv_height = out->UV_end_y - out->UV_start_y + 1;
	out->ft_y_width = (out->Y_end_x - out->Y_start_x + 2) / 2 * 2;
	out->ft_uv_width = (out->UV_end_x - out->UV_start_x + 1) * 2;

	if (in->yuv_8bits_flag == 0) {
		out->src_lum_addr += out->Y_start_y * ft_pitch +
			out->Y_start_x * 2;
		out->src_chr_addr += (out->UV_start_y * ft_pitch) +
			out->UV_start_x * 2 * 2;
		out->dst_lum_addr += (in->start_row + in->overlap_up) *
			ft_pitch + (in->start_col + in->overlap_left) * 2;
		out->dst_chr_addr += (((in->start_row + in->overlap_up) * ft_pitch) >> 1) +
			(in->start_col + in->overlap_left) * 2;
	} else {
		out->src_lum_addr += out->Y_start_y * ft_pitch +
			out->Y_start_x;
		out->src_chr_addr += (out->UV_start_y * ft_pitch) +
			out->UV_start_x * 2;
		out->dst_lum_addr += (in->start_row + in->overlap_up) * ft_pitch +
			(in->start_col + in->overlap_left) ;
		out->dst_chr_addr += (((in->start_row + in->overlap_up) * ft_pitch) >> 1) +
			(in->start_col + in->overlap_left) ;
	}

	out->dst_width = in->end_col - in->start_col +1 - in->overlap_left - in->overlap_right;
	out->dst_height = in->end_row - in->start_row + 1 - in->overlap_up - in->overlap_down;
	out->offset_start_x = in->overlap_left;
	out->offset_start_y = in->overlap_up;

	if ((in->overlap_left == 0) && (in->overlap_right == 0) &&
		(in->overlap_up == 0) && (in->overlap_down == 0))
		out->crop_bypass = 1;
	else
		out->crop_bypass = 0;

	return 0;
}

static int alg_nr3_memctrl_slice_info_update(struct nr3_slice *nr3_fw_in,
	struct nr3_slice_for_blending *nr3_fw_out, uint32_t nr3_mv_version)
{
	if (!nr3_fw_in || !nr3_fw_out) {
		pr_err("fail to get valid in ptr\n");
		return 0;
	}

	switch (nr3_mv_version) {
		case ALG_NR3_MV_VER_0:
			alg_nr3_memctrl_slice_info_update_ver0(nr3_fw_in, nr3_fw_out);
			break;
		case ALG_NR3_MV_VER_1:
			alg_nr3_memctrl_slice_info_update_ver1(nr3_fw_in, nr3_fw_out);
			break;
		default:
			pr_err("fail to get invalid version %d\n", nr3_mv_version);
			break;
	}
	return 0;
}

static int ispslice_3dnr_memctrl_update_info_cfg(
		void *cfg_in, struct isp_slice_context *slc_ctx)
{
	int ret = 0, idx = 0;
	struct nr3_slice nr3_fw_in = {0};
	struct nr3_slice_for_blending nr3_fw_out = {0};
	struct slice_3dnr_memctrl_info *slc_3dnr_memctrl = NULL;
	struct slice_3dnr_store_info *slc_3dnr_store = NULL;
	struct slice_3dnr_crop_info *slc_3dnr_crop = NULL;
	struct isp_slice_desc *cur_slc = NULL;
	struct slice_cfg_input *in_ptr = (struct slice_cfg_input *)cfg_in;
	struct isp_3dnr_ctx_desc *nr3_ctx = in_ptr->nr3_ctx;
	uint32_t nr3_mv_version = nr3_ctx->nr3_mv_version;

	memset((void *)&nr3_fw_in, 0, sizeof(nr3_fw_in));
	memset((void *)&nr3_fw_out, 0, sizeof(nr3_fw_out));

	nr3_fw_in.cur_frame_width = in_ptr->frame_in_size.w;
	nr3_fw_in.cur_frame_height = in_ptr->frame_in_size.h;
	nr3_fw_in.mv_x = nr3_ctx->mv.mv_x;
	nr3_fw_in.mv_y = nr3_ctx->mv.mv_y;
	nr3_fw_in.ft_pitch = nr3_ctx->mem_ctrl.ft_pitch;
	nr3_fw_in.yuv_8bits_flag = nr3_ctx->mem_ctrl.yuv_8bits_flag;

	/* slice_num != 1, just pass current slice info */
	nr3_fw_in.slice_num = slc_ctx->slice_num;

	cur_slc = &slc_ctx->slices[0];

	if (nr3_ctx->type == NR3_FUNC_OFF)
		return 0;

	for (idx = 0; idx < SLICE_NUM_MAX; idx++, cur_slc++) {
		if (cur_slc->valid == 0)
			continue;

		slc_3dnr_memctrl = &cur_slc->slice_3dnr_memctrl;
		slc_3dnr_store = &cur_slc->slice_3dnr_store;
		slc_3dnr_crop = &cur_slc->slice_3dnr_crop;

		nr3_fw_in.slice_id = idx;
		nr3_fw_in.end_col = cur_slc->slice_pos.end_col;
		nr3_fw_in.end_row = cur_slc->slice_pos.end_row;
		nr3_fw_in.start_row = cur_slc->slice_pos.start_row;
		nr3_fw_in.start_col = cur_slc->slice_pos.start_col;
		nr3_fw_in.overlap_up = cur_slc->slice_overlap.overlap_up;
		nr3_fw_in.overlap_left = cur_slc->slice_overlap.overlap_left;
		nr3_fw_in.overlap_down = cur_slc->slice_overlap.overlap_down;
		nr3_fw_in.overlap_right = cur_slc->slice_overlap.overlap_right;
		nr3_fw_out.first_line_mode = slc_3dnr_memctrl->first_line_mode;
		nr3_fw_out.last_line_mode = slc_3dnr_memctrl->last_line_mode;
		nr3_fw_out.src_lum_addr = slc_3dnr_memctrl->addr.addr_ch0;
		nr3_fw_out.src_chr_addr = slc_3dnr_memctrl->addr.addr_ch1;
		nr3_fw_out.dst_lum_addr = slc_3dnr_store->addr.addr_ch0;
		nr3_fw_out.dst_chr_addr = slc_3dnr_store->addr.addr_ch1;
		nr3_fw_out.ft_y_width = slc_3dnr_memctrl->ft_y.w;
		nr3_fw_out.ft_y_height = slc_3dnr_memctrl->ft_y.h;
		nr3_fw_out.ft_uv_width = slc_3dnr_memctrl->ft_uv.w;
		nr3_fw_out.ft_uv_height = slc_3dnr_memctrl->ft_uv.h;
		nr3_fw_out.start_col = slc_3dnr_memctrl->start_col;
		nr3_fw_out.start_row = slc_3dnr_memctrl->start_row;

		pr_debug("slice_num[%d], frame_width[%d], frame_height[%d]",
			nr3_fw_in.slice_num, nr3_fw_in.cur_frame_width,
			nr3_fw_in.cur_frame_height);
		pr_debug("start_col[%d], start_row[%d]",
			nr3_fw_out.start_col, nr3_fw_out.start_row);
		pr_debug("src_lum_addr[0x%x], src_chr_addr[0x%x]\n",
			nr3_fw_out.src_lum_addr, nr3_fw_out.src_chr_addr);
		pr_debug("dst_lum_addr[0x%x], dst_chr_addr[0x%x]\n",
			nr3_fw_out.dst_lum_addr, nr3_fw_out.dst_chr_addr);
		pr_debug("ft_y_width[%d], ft_y_height[%d], ft_uv_width[%d]",
			nr3_fw_out.ft_y_width, nr3_fw_out.ft_y_height,
			nr3_fw_out.ft_uv_width);
		pr_debug("ft_uv_height[%d], last_line_mode[%d]",
			nr3_fw_out.ft_uv_height, nr3_fw_out.last_line_mode);
		pr_debug("first_line_mode[%d]\n", nr3_fw_out.first_line_mode);

		alg_nr3_memctrl_slice_info_update(&nr3_fw_in, &nr3_fw_out, nr3_mv_version);

		slc_3dnr_memctrl->first_line_mode = nr3_fw_out.first_line_mode;
		slc_3dnr_memctrl->last_line_mode = nr3_fw_out.last_line_mode;
		slc_3dnr_memctrl->addr.addr_ch0 = nr3_fw_out.src_lum_addr;
		slc_3dnr_memctrl->addr.addr_ch1 = nr3_fw_out.src_chr_addr;
		slc_3dnr_memctrl->ft_y.w = nr3_fw_out.ft_y_width;
		slc_3dnr_memctrl->ft_y.h = nr3_fw_out.ft_y_height;
		slc_3dnr_memctrl->ft_uv.w = nr3_fw_out.ft_uv_width;
		slc_3dnr_memctrl->ft_uv.h = nr3_fw_out.ft_uv_height;

		if (nr3_ctx->nr3_mv_version == ALG_NR3_MV_VER_1) {
			slc_3dnr_store->addr.addr_ch0 = nr3_fw_out.dst_lum_addr;
			slc_3dnr_store->addr.addr_ch1 = nr3_fw_out.dst_chr_addr;
			slc_3dnr_store->size.w = nr3_fw_out.dst_width;
			slc_3dnr_store->size.h = nr3_fw_out.dst_height;
			slc_3dnr_crop->start_x = nr3_fw_out.offset_start_x;
			slc_3dnr_crop->start_y = nr3_fw_out.offset_start_y;
			slc_3dnr_crop->bypass = nr3_fw_out.crop_bypass;
		}

		pr_debug("slice_num[%d], frame_width[%d], frame_height[%d]\n",
			nr3_fw_in.slice_num, nr3_fw_in.cur_frame_width,
			nr3_fw_in.cur_frame_height);
		pr_debug("start_col[%d], start_row[%d]\n",
			nr3_fw_out.start_col, nr3_fw_out.start_row);
		pr_debug("src_lum_addr[0x%x], src_chr_addr[0x%x]\n",
			nr3_fw_out.src_lum_addr, nr3_fw_out.src_chr_addr);
		pr_debug("dst_lum_addr[0x%x], dst_chr_addr[0x%x]\n",
			nr3_fw_out.dst_lum_addr, nr3_fw_out.dst_chr_addr);
		pr_debug("ft_y_width[%d], ft_y_height[%d], ft_uv_width[%d]\n",
			nr3_fw_out.ft_y_width, nr3_fw_out.ft_y_height,
			nr3_fw_out.ft_uv_width);
		pr_debug("ft_uv_height[%d], last_line_mode[%d]\n",
			nr3_fw_out.ft_uv_height, nr3_fw_out.last_line_mode);
		pr_debug("first_line_mode[%d]\n", nr3_fw_out.first_line_mode);
	}

	return ret;
}

static int ispslice_3dnr_info_cfg(
		void *cfg_in, struct isp_slice_context *slc_ctx)
{
	int ret = 0;
	struct slice_cfg_input *in_ptr = (struct slice_cfg_input *)cfg_in;

	ret = ispslice_3dnr_memctrl_info_cfg(cfg_in, slc_ctx);
	if (ret) {
		pr_err("fail to set slice 3dnr mem ctrl!\n");
		goto exit;
	}

	ret = ispslice_3dnr_memctrl_update_info_cfg(cfg_in, slc_ctx);
	if (ret) {
		pr_err("fail to update slice 3dnr mem ctrl!\n");
		goto exit;
	}

	ret = ispslice_3dnr_store_info_cfg(cfg_in, slc_ctx);
	if (ret) {
		pr_err("fail to set slice 3dnr store info!\n");
		goto exit;
	}

	if (!in_ptr->nr3_ctx->nr3_fbc_store.bypass) {
		ret = ispslice_3dnr_fbc_store_info_cfg(cfg_in, slc_ctx);
		if (ret) {
			pr_err("fail to set slice 3dnr fbc store info!\n");
			goto exit;
		}
	}

	if (!in_ptr->nr3_ctx->nr3_fbd_fetch.bypass) {
		ret = ispslice_3dnr_fbd_fetch_info_cfg(cfg_in, slc_ctx);
		if (ret) {
			pr_err("fail to set slice 3dnr fbd fetch ctrl!\n");
			goto exit;
		}
	}

	ret = ispslice_3dnr_crop_info_cfg(cfg_in, slc_ctx);
	if (ret) {
		pr_err("fail to set slice 3dnr crop info!\n");
		goto exit;
	}

exit:
	return ret;
}

static int ispslice_3dnr_set(
		struct isp_fmcu_ctx_desc *fmcu,
		struct isp_slice_desc *cur_slc,
		struct cam_hw_info *hw,
		struct isp_sw_context *pctx)
{
	struct isp_hw_nr3_fbc_slice nr3_fbc_slice;
	struct isp_hw_nr3_fbd_slice fbd;
	struct isp_hw_slice_3dnr_crop croparg;
	struct isp_hw_slice_3dnr_store storearg;
	struct isp_hw_slice_3dnr_memctrl memarg;

	memarg.fmcu = fmcu;
	memarg.mem_ctrl = &cur_slc->slice_3dnr_memctrl;
	hw->isp_ioctl(fmcu, ISP_HW_CFG_SLICE_3DNR_MEMCTRL, &memarg);
	storearg.fmcu = fmcu;
	storearg.store = &cur_slc->slice_3dnr_store;
	hw->isp_ioctl(fmcu, ISP_HW_CFG_SLICE_3DNR_STORE, &storearg);
	if (pctx->pipe_src.nr3_fbc_fbd) {
		nr3_fbc_slice.fmcu_handle = fmcu;
		nr3_fbc_slice.fbc_store = &cur_slc->slice_3dnr_fbc_store;
		fbd.fmcu_handle = fmcu;
		fbd.fbd_fetch = &cur_slc->slice_3dnr_fbd_fetch;
		fbd.mem_ctrl = &cur_slc->slice_3dnr_memctrl;
		nr3_fbc_slice.fbc_store->ctx_id = pctx->ctx_id;
		fbd.fbd_fetch->ctx_id = pctx->ctx_id;
		hw->isp_ioctl(hw, ISP_HW_CFG_NR3_FBC_SLICE_SET, &nr3_fbc_slice);
		hw->isp_ioctl(hw, ISP_HW_CFG_NR3_FBD_SLICE_SET, &fbd);
	}
	croparg.fmcu = fmcu;
	croparg.crop = &cur_slc->slice_3dnr_crop;
	hw->isp_ioctl(hw, ISP_HW_CFG_SLICE_3DNR_CROP, &croparg);

	return 0;
}

static int ispslice_ltm_info_cfg(struct isp_ltm_ctx_desc *ltm_ctx,
		struct isp_slice_context *slc_ctx, enum isp_ltm_region ltm_id)
{
	int ret = 0, idx = 0;
	struct isp_ltm_rtl_param rtl_param;
	struct isp_ltm_rtl_param *prtl = &rtl_param;

	struct isp_slice_desc *cur_slc = NULL;
	struct slice_ltm_map_info *slc_ltm_map;
	uint32_t slice_info[4];
	struct isp_ltm_map *map = NULL;

	if (!ltm_ctx || !slc_ctx) {
		pr_err("fail to get invalid in_ptr\n");
		return -1;
	}

	map = &ltm_ctx->map;
	if (map->bypass)
		return 0;

	if (ltm_ctx->mode == MODE_LTM_OFF) {
		for (idx = 0; idx < SLICE_NUM_MAX; idx++) {
			cur_slc = &slc_ctx->slices[idx];
			if (cur_slc->valid == 0)
				continue;
			slc_ltm_map = &cur_slc->slice_ltm_map[ltm_id];
			slc_ltm_map->bypass = 1;
		}

		return 0;
	}

	for (idx = 0; idx < SLICE_NUM_MAX; idx++) {
		cur_slc = &slc_ctx->slices[idx];
		if (cur_slc->valid == 0)
			continue;
		slc_ltm_map = &cur_slc->slice_ltm_map[ltm_id];

		slice_info[0] = cur_slc->slice_pos.start_col;
		slice_info[1] = cur_slc->slice_pos.start_row;
		slice_info[2] = cur_slc->slice_pos.end_col;
		slice_info[3] = cur_slc->slice_pos.end_row;

		isp_ltm_map_slice_config_gen(ltm_ctx, prtl, slice_info);

		slc_ltm_map->tile_width = map->tile_width;
		slc_ltm_map->tile_height = map->tile_height;

		slc_ltm_map->tile_num_x = prtl->tile_x_num_rtl;
		slc_ltm_map->tile_num_y = prtl->tile_y_num_rtl;
		slc_ltm_map->tile_right_flag = prtl->tile_right_flag_rtl;
		slc_ltm_map->tile_left_flag = prtl->tile_left_flag_rtl;
		slc_ltm_map->tile_start_x = prtl->tile_start_x_rtl;
		slc_ltm_map->tile_start_y = prtl->tile_start_y_rtl;
		slc_ltm_map->mem_addr = map->mem_init_addr +
			prtl->tile_index_xs * 128 * 2;
		pr_debug("ltm slice info: tile_num_x[%d], tile_num_y[%d], tile_right_flag[%d] \
			tile_left_flag[%d], tile_start_x[%d], tile_start_y[%d], mem_addr[0x%x]\n",
			slc_ltm_map->tile_num_x, slc_ltm_map->tile_num_y,
			slc_ltm_map->tile_right_flag, slc_ltm_map->tile_left_flag,
			slc_ltm_map->tile_start_x, slc_ltm_map->tile_start_y,
			slc_ltm_map->mem_addr);
	}

	return ret;
}

static int ispslice_gtm_info_cfg(struct isp_gtm_ctx_desc *gtm_ctx,
		struct isp_slice_context *slc_ctx)
{
	int i = 0;
	uint32_t overlap_left = 0, overlap_right = 0;
	struct isp_slice_desc *cur_slc =NULL;

	if (!gtm_ctx || !slc_ctx) {
		pr_err("fail to get invalid in_ptr\n");
		return -1;
	}

	if (gtm_ctx->mode == MODE_GTM_OFF
		|| (slc_ctx->slice_num  < 2)) {
		pr_debug("slice gtm off\n");
		return 0;
	}

	for(; i < SLICE_NUM_MAX; i++) {
		cur_slc = &slc_ctx->slices[i];
		if (cur_slc->valid ==0)
			continue;
		if (i == 0) {
			cur_slc->slice_gtm.first_slice = 1;
			cur_slc->slice_gtm.last_slice = 0;
		} else if (i == (slc_ctx->slice_num -1)) {
			cur_slc->slice_gtm.first_slice = 0;
			cur_slc->slice_gtm.last_slice = 1;
		}

		cur_slc->slice_gtm.gtm_stat_slice_en = 1;
		cur_slc->slice_gtm.gtm_mode_en = gtm_ctx->gtm_mode_en;
		cur_slc->slice_gtm.gtm_map_bypass = gtm_ctx->gtm_map_bypass;
		cur_slc->slice_gtm.gtm_hist_stat_bypass = gtm_ctx->gtm_hist_stat_bypass;
		if (gtm_ctx->gtm_hist_stat_bypass || (gtm_ctx->calc_mode == GTM_SW_CALC))
			cur_slc->slice_gtm.gtm_tm_param_calc_by_hw = 0;
		else
			cur_slc->slice_gtm.gtm_tm_param_calc_by_hw = 1;

		cur_slc->slice_gtm.gtm_cur_is_first_frame = (gtm_ctx->calc_mode == GTM_SW_CALC) ? 1 : 0;
		cur_slc->slice_gtm.gtm_tm_luma_est_mode = gtm_ctx->gtm_tm_luma_est_mode;
		cur_slc->slice_gtm.gtm_tm_in_bit_depth = 14;
		cur_slc->slice_gtm.gtm_tm_out_bit_depth = 14;

		overlap_left = cur_slc->slice_overlap.overlap_left;
		overlap_right = cur_slc->slice_overlap.overlap_right;
		cur_slc->slice_gtm.line_startpos = overlap_left;
		cur_slc->slice_gtm.line_endpos = cur_slc->slice_fetch.size.w - overlap_right;
	}

	return 0;
}

static int ispslice_noisefilter_info_cfg(void *cfg_in, struct isp_slice_context *slc_ctx)
{
	int ret = 0, rtn = 0;
	struct isp_slice_desc *cur_slc;
	struct slice_cfg_input *in_ptr = (struct slice_cfg_input *)cfg_in;
	struct isp_k_block  *isp_k_param = in_ptr->nofilter_ctx;

	if (!slc_ctx) {
		pr_err("fail to get input ptr, null.\n");
		return -EFAULT;
	}

	cur_slc = &slc_ctx->slices[0];
	cur_slc->slice_noisefilter_mode.seed_for_mode1 = isp_k_param->seed0_for_mode1;
	cur_slc->slice_noisefilter_mode.yrandom_mode = isp_k_param->yrandom_mode;

	rtn = ispslice_noisefliter_info_set(cur_slc, slc_ctx);

	return ret;

}

int isp_slice_info_cfg(void *cfg_in, struct isp_slice_context *slc_ctx)
{
	struct slice_cfg_input *in_ptr = NULL;

	if (!cfg_in || !slc_ctx) {
		pr_err("fail to get input ptr, null.\n");
		return -EFAULT;
	}

	in_ptr = (struct slice_cfg_input *)cfg_in;
	slc_ctx->pyr_rec_eb = in_ptr->pyr_rec_eb;
	ispslice_fetch_info_cfg(cfg_in, slc_ctx);
	ispslice_store_info_cfg(cfg_in, slc_ctx);
	ispslice_afbc_store_info_cfg(cfg_in, slc_ctx);
	ispslice_3dnr_info_cfg(cfg_in, slc_ctx);
	if (in_ptr->ltm_rgb_eb)
		ispslice_ltm_info_cfg(in_ptr->rgb_ltm, slc_ctx, LTM_RGB);
	if (in_ptr->ltm_yuv_eb)
		ispslice_ltm_info_cfg(in_ptr->yuv_ltm, slc_ctx, LTM_YUV);
	if (in_ptr->gtm_rgb_eb)
		ispslice_gtm_info_cfg(in_ptr->rgb_gtm, slc_ctx);
	ispslice_noisefilter_info_cfg(cfg_in, slc_ctx);

	return 0;
}

int isp_slice_base_cfg(void *cfg_in, void *slice_ctx,
		uint32_t *valid_slc_num)
{
	int i = 0;
	int ret = 0;
	struct slice_cfg_input *in_ptr = (struct slice_cfg_input *)cfg_in;
	struct isp_slice_context *slc_ctx = (struct isp_slice_context *)slice_ctx;

	if (!in_ptr || !slc_ctx || !valid_slc_num) {
		pr_err("fail to get input ptr, null.\n");
		return -EFAULT;
	}

	memset(slc_ctx, 0, sizeof(struct isp_slice_context));

	switch (in_ptr->calc_dyn_ov.verison) {
		case ALG_ISP_DYN_OVERLAP_NONE:
			ispslice_slice_base_info_cfg(in_ptr, slc_ctx);
			ispslice_slice_scaler_info_cfg(in_ptr, slc_ctx);
			break;
		case ALG_ISP_OVERLAP_VER_1:
			ispslice_slice_base_info_cfg_ex(in_ptr, slc_ctx);
			ispslice_slice_scaler_info_cfg_ex(in_ptr, slc_ctx);
			break;
		case ALG_ISP_OVERLAP_VER_2:
			ispslice_base_info_calc_cfg(in_ptr, slc_ctx);
			ispslice_scaler_info_calc_cfg(in_ptr, slc_ctx);
			break;
		default:
			pr_err("fail to get overlap version\n");
			ret = -1;
			break;
	}

	ispslice_slice_nr_info_cfg(in_ptr, slc_ctx);

	*valid_slc_num = 0;
	for (i = 0; i < SLICE_NUM_MAX; i++) {
		if (slc_ctx->slices[i].valid)
			*valid_slc_num = (*valid_slc_num) + 1;
	}

	return ret;
}

void *isp_slice_ctx_get()
{
	struct isp_slice_context *ptr;

	ptr = vzalloc(sizeof(struct isp_slice_context));
	if (IS_ERR_OR_NULL(ptr))
		return NULL;

	return ptr;
}

int isp_slice_ctx_put(void **slc_ctx)
{
	if (*slc_ctx)
		vfree(*slc_ctx);
	*slc_ctx = NULL;
	return 0;
}

int isp_slice_fmcu_cmds_set(void *fmcu_handle, void *ctx)
{
	int i, j;
	int sw_ctx_id = 0;
	int hw_ctx_id = 0;
	uint32_t yrandom_mode = 0;
	enum isp_work_mode wmode;
	struct isp_slice_desc *cur_slc;
	struct slice_store_info *slc_store;
	struct slice_afbc_store_info *slc_afbc_store;
	struct slice_scaler_info *slc_scaler;
	struct isp_slice_context *slc_ctx;
	struct isp_fmcu_ctx_desc *fmcu;
	struct isp_sw_context *pctx = NULL;
	struct cam_hw_info *hw = NULL;
	struct isp_rec_ctx_desc *rec_ctx = NULL;
	struct isp_hw_fbd_slice fbd_slice;
	struct isp_hw_afbc_path_slice afbc_slice;
	struct isp_hw_ltm_slice ltm;
	struct isp_hw_gtm_slice gtm;
	struct isp_hw_fmcu_cfg fmcu_cfg;
	struct isp_hw_slices_fmcu_cmds parg;
	struct isp_hw_slice_spath spath_sotre;
	struct isp_hw_slice_spath spath_scaler;
	struct isp_hw_slice_spath_thumbscaler thumbscaler;
	struct isp_hw_slice_nofilter slicearg;
	struct isp_hw_set_slice_fetch fetcharg;
	struct isp_hw_set_slice_nr_info nrarg;
	struct isp_hw_gtm_func gtm_func;
	struct isp_hw_k_blk_func rec_slice_func;

	if (!fmcu_handle || !ctx) {
		pr_err("fail to get valid input ptr, fmcu_handle %p, ctx %p\n",
			fmcu_handle, ctx);
		return -EFAULT;
	}

	pctx = (struct isp_sw_context *)ctx;
	rec_ctx = (struct isp_rec_ctx_desc *)pctx->rec_handle;
	hw = pctx->hw;
	sw_ctx_id = pctx->ctx_id;
	hw_ctx_id = isp_core_hw_context_id_get(pctx);
	pr_debug("get hw context id=%d\n", hw_ctx_id);
	wmode = pctx->dev->wmode;
	slc_ctx = (struct isp_slice_context *)pctx->slice_ctx;
	if (slc_ctx->slice_num < 1) {
		pr_err("fail to use slices, not support here.\n");
		return -EINVAL;
	}

	fmcu = (struct isp_fmcu_ctx_desc *)fmcu_handle;
	cur_slc = &slc_ctx->slices[0];
	yrandom_mode = cur_slc->slice_noisefilter_mode.yrandom_mode;
	for (i = 0; i < SLICE_NUM_MAX; i++, cur_slc++) {
		if (cur_slc->valid == 0)
			continue;
		if (wmode != ISP_CFG_MODE || (slc_ctx->pyr_rec_eb && i == 0)) {
			pr_debug("no need to cfg\n");
		} else {
			fmcu_cfg.fmcu = fmcu;
			fmcu_cfg.ctx_id = hw_ctx_id;
			hw->isp_ioctl(hw, ISP_HW_CFG_FMCU_CFG, &fmcu_cfg);
		}

		nrarg.fmcu = fmcu;
		nrarg.slice_ynr = &cur_slc->slice_ynr;
		nrarg.slice_nlm = &cur_slc->slice_nlm;
		nrarg.start_row_mod4 = cur_slc->slice_postcdn.start_row_mod4;
		nrarg.slice_postcnr_info = &cur_slc->slice_postcnr;
		nrarg.slice_edge = &cur_slc->slice_edge;
		hw->isp_ioctl(hw, ISP_HW_CFG_SET_SLICE_NR_INFO, &nrarg);

		if (!slc_ctx->pyr_rec_eb || rec_ctx->fetch_path_sel != 1) {
			if (!cur_slc->slice_fbd_raw.fetch_fbd_bypass) {
				fbd_slice.fmcu_handle = fmcu;
				fbd_slice.info = &cur_slc->slice_fbd_raw;
				hw->isp_ioctl(hw, ISP_HW_CFG_FBD_SLICE_SET, &fbd_slice);
			} else if (!cur_slc->slice_fbd_yuv.fetch_fbd_bypass){
				fbd_slice.fmcu_handle = fmcu;
				fbd_slice.yuv_info = &cur_slc->slice_fbd_yuv;
				hw->isp_ioctl(hw, ISP_HW_CFG_FBD_SLICE_SET, &fbd_slice);
			} else {
				fetcharg.fmcu = fmcu;
				fetcharg.fetch_info = &cur_slc->slice_fetch;
				hw->isp_ioctl(fmcu, ISP_HW_CFG_SET_SLICE_FETCH, &fetcharg);
			}
		}

		if (rec_ctx && rec_ctx->layer_num) {
			if (slc_ctx->pyr_rec_eb) {
				rec_slice_func.index = ISP_K_BLK_PYR_REC_SLICE_COMMON;
				hw->isp_ioctl(hw, ISP_HW_CFG_K_BLK_FUNC_GET, &rec_slice_func);
				rec_ctx->fetch_fbd.slice_size = cur_slc->slice_fbd_yuv.slice_size;
				rec_ctx->fetch_fbd.slice_start_pxl_xpt = cur_slc->slice_fbd_yuv.slice_start_pxl_xpt;
				rec_ctx->fetch_fbd.slice_start_pxl_ypt = cur_slc->slice_fbd_yuv.slice_start_pxl_ypt;
				rec_ctx->fetch_fbd.slice_start_header_addr = cur_slc->slice_fbd_yuv.slice_start_header_addr;
				rec_ctx->cur_slice_id = i;
			} else {
				rec_slice_func.index = ISP_K_BLK_PYR_REC_BYPASS;
				hw->isp_ioctl(hw, ISP_HW_CFG_K_BLK_FUNC_GET, &rec_slice_func);
			}
			if (rec_slice_func.k_blk_func)
				rec_slice_func.k_blk_func(rec_ctx);
		}

		if (pctx->pipe_src.ltm_rgb){
			ltm.fmcu_handle = fmcu;
			ltm.map = &cur_slc->slice_ltm_map[LTM_RGB];
			ltm.ltm_id = LTM_RGB;
			hw->isp_ioctl(hw, ISP_HW_CFG_LTM_SLICE_SET, &ltm);
		}
		if (pctx->pipe_src.ltm_yuv){
			ltm.fmcu_handle = fmcu;
			ltm.map = &cur_slc->slice_ltm_map[LTM_YUV];
			ltm.ltm_id = LTM_YUV;
			hw->isp_ioctl(hw, ISP_HW_CFG_LTM_SLICE_SET, &ltm);
		}
		if (pctx->pipe_src.gtm_rgb) {
			gtm.idx = sw_ctx_id;
			gtm.fmcu_handle = fmcu;
			gtm.slice_param = &cur_slc->slice_gtm;
			gtm_func.index = ISP_K_GTM_SLICE_SET;
			hw->isp_ioctl(hw, ISP_HW_CFG_GTM_FUNC_GET, &gtm_func);
			gtm_func.k_blk_func(&gtm);
		}
		if (yrandom_mode == 1) {
			slicearg.fmcu = fmcu;
			slicearg.noisefilter_info = &cur_slc->noisefilter_info;
			hw->isp_ioctl(hw, ISP_HW_CFG_SLICE_NOFILTER, &slicearg);
		}

		for (j = 0; j < ISP_SPATH_NUM; j++) {
			slc_store = &cur_slc->slice_store[j];
			if (j == ISP_SPATH_FD) {
				thumbscaler.fmcu = fmcu;
				thumbscaler.path_en = cur_slc->path_en[j];
				thumbscaler.ctx_idx = sw_ctx_id;
				thumbscaler.slc_scaler = &cur_slc->slice_thumbscaler;
				hw->isp_ioctl(hw, ISP_HW_CFG_SLICE_SPATH_THUMBSCALER, &thumbscaler);
			} else {
				slc_scaler = &cur_slc->slice_scaler[j];
				spath_scaler.fmcu = fmcu;
				spath_scaler.path_en = cur_slc->path_en[j];
				spath_scaler.ctx_idx = sw_ctx_id;
				spath_scaler.spath_id = j;
				spath_scaler.slc_scaler = slc_scaler;

				hw->isp_ioctl(hw, ISP_HW_CFG_SLICE_SPATH_SCALER, &spath_scaler);
			}
			spath_sotre.fmcu = fmcu;
			spath_sotre.path_en = cur_slc->path_en[j];
			spath_sotre.ctx_idx = sw_ctx_id;
			spath_sotre.spath_id = j;
			spath_sotre.slc_store = slc_store;
			hw->isp_ioctl(hw, ISP_HW_CFG_SLICE_SPATH_STORE, &spath_sotre);

			if (j < AFBC_PATH_NUM) {
				slc_afbc_store = &cur_slc->slice_afbc_store[j];
				if (slc_afbc_store->slc_afbc_on) {
					afbc_slice.fmcu_handle = fmcu;
					afbc_slice.path_en = cur_slc->path_en[j];
					afbc_slice.ctx_idx = sw_ctx_id;
					afbc_slice.spath_id = j;
					afbc_slice.slc_afbc_store = slc_afbc_store;
					hw->isp_ioctl(hw, ISP_HW_CFG_AFBC_PATH_SLICE_SET, &afbc_slice);
				}
			}
		}

		ispslice_3dnr_set(fmcu, cur_slc, hw, pctx);

		parg.wmode = wmode;
		parg.hw_ctx_id = hw_ctx_id;
		parg.fmcu = fmcu;
		hw->isp_ioctl(hw, ISP_HW_CFG_SLICE_FMCU_CMD, &parg);

		hw->isp_ioctl(hw, ISP_HW_CFG_FMCU_CMD_ALIGN, fmcu);
	}

	return 0;
}

int isp_slice_update(void *pctx_handle, uint32_t ctx_id, uint32_t slice_id)
{
	int j;
	struct isp_slice_desc *cur_slc;
	struct slice_store_info *slc_store;
	struct slice_afbc_store_info *slc_afbc_store;
	struct slice_scaler_info *slc_scaler;
	struct isp_slice_context *slc_ctx;
	struct isp_sw_context *pctx = NULL;
	struct cam_hw_info * hw = NULL;
	struct isp_hw_afbc_path_slice afbc_slice;
	struct isp_hw_slice_scaler update;
	struct isp_hw_slice_store store;
	struct isp_hw_slice_fetch fetch;
	struct isp_hw_slice_nr_info info;

	if (!pctx_handle) {
		pr_err("fail to get input ptr, null.\n");
		return -EFAULT;
	}

	pctx = (struct isp_sw_context *)pctx_handle;
	hw = pctx->hw;
	slc_ctx = (struct isp_slice_context *)pctx->slice_ctx;
	if (slc_ctx->slice_num < 1) {
		pr_err("fail to use slices, not support here.\n");
		return -EINVAL;
	}

	if ((slice_id >= SLICE_NUM_MAX) ||
		(slc_ctx->slices[slice_id].valid == 0)) {
		pr_err("fail to get valid slice id %d\n", slice_id);
		return -EINVAL;
	}

	cur_slc = &slc_ctx->slices[slice_id];
	info.ctx_id = ctx_id;
	info.cur_slc = cur_slc;
	hw->isp_ioctl(hw, ISP_HW_CFG_SLICE_NR_INFO, &info);
	fetch.ctx_id = ctx_id;
	fetch.fetch_info = &cur_slc->slice_fetch;
	hw->isp_ioctl(hw, ISP_HW_CFG_SLICE_FETCH, &fetch);
	for (j = 0; j < ISP_SPATH_NUM; j++) {
		slc_store = &cur_slc->slice_store[j];
		if (j != ISP_SPATH_FD) {
			slc_scaler = &cur_slc->slice_scaler[j];
			update.ctx_id = ctx_id;
			update.path_en = cur_slc->path_en[j];
			update.slc_scaler = slc_scaler;
			update.spath_id = j;
			hw->isp_ioctl(hw, ISP_HW_CFG_SLICE_SCALER, &update);
		}
		store.path_en = cur_slc->path_en[j];
		store.ctx_id = ctx_id;
		store.spath_id = j;
		store.slc_store = slc_store;
		hw->isp_ioctl(hw, ISP_HW_CFG_SLICE_STORE, &store);
		if (j < AFBC_PATH_NUM) {
			slc_afbc_store = &cur_slc->slice_afbc_store[j];
			if (slc_afbc_store->slc_afbc_on) {
				afbc_slice.fmcu_handle = NULL;
				afbc_slice.path_en = cur_slc->path_en[j];
				afbc_slice.ctx_idx = ctx_id;
				afbc_slice.spath_id = j;
				afbc_slice.slc_afbc_store = slc_afbc_store;
				hw->isp_ioctl(hw, ISP_HW_CFG_AFBC_PATH_SLICE_SET, &afbc_slice);
			}
		}
	}
	return 0;
}
