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

#include "alg_common_calc.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ALG_COMMON_CALC: %d %d %s : "fmt, current->pid, __LINE__, __func__


void calc_scaler_phase(int32_t phase, uint16_t factor,
	int16_t *phase_int, uint16_t *phase_rmd)
{
	phase_int[0] = (uint32_t)(phase / factor);
	phase_rmd[0] = (uint32_t)(phase - factor * phase_int[0]);
}

void yuv_scaler_init_slice_info_v3(
	struct yuvscaler_param_t *frame_scaler, struct yuvscaler_param_t *slice_scaler,
	struct scaler_slice_t *slice_info, const struct scaler_slice_t *input_slice_info,
	const struct scaler_slice_t *output_slice_info)
{
	int trim_start, trim_size;
	int deci;
	int scl_en, scl_factor_in, scl_factor_out, scl_tap, init_phase;
	int input_slice_start, input_slice_size, input_pixel_align;
	int output_slice_start, output_slice_size, output_pixel_align;
	int overlap_head, overlap_tail;
	int start_col_org, start_row_org;

	memcpy(slice_scaler, frame_scaler, sizeof(struct yuvscaler_param_t));

	/* hor */
	trim_start = frame_scaler->trim0_info.trim_start_x;
	trim_size = frame_scaler->trim0_info.trim_size_x;
	deci = frame_scaler->deci_info.deci_x;
	scl_en = frame_scaler->scaler_info.scaler_en;
	scl_factor_in = frame_scaler->scaler_info.scaler_factor_in_hor;
	scl_factor_out = frame_scaler->scaler_info.scaler_factor_out_hor;
	scl_tap = frame_scaler->scaler_info.scaler_y_hor_tap;
	init_phase = frame_scaler->scaler_info.init_phase_info.scaler_init_phase[0];

	slice_scaler->src_size_x = slice_info->slice_width;

	overlap_head = input_slice_info->start_col - slice_info->start_col;
	overlap_tail = slice_info->end_col - input_slice_info->end_col;

	start_col_org = slice_info->start_col;

	slice_info->start_col += overlap_head;
	slice_info->end_col -= overlap_tail;
	slice_info->slice_width = slice_info->end_col - slice_info->start_col + 1;

	input_slice_start = slice_info->start_col;
	input_slice_size = slice_info->slice_width;
	input_pixel_align = 2;/* YUV420 */
	output_pixel_align = 2;/* YUV420 */
	if (slice_info->sliceColNo != slice_info->sliceCols - 1)
		output_pixel_align = frame_scaler->output_align_hor;

	output_slice_start = output_slice_info->start_col;
	output_slice_size = output_slice_info->end_col - output_slice_info->start_col + 1;

	if (output_slice_size == 0) {
		slice_scaler->trim0_info.trim_start_x = 0;
		slice_scaler->trim0_info.trim_size_x = 0;
	} else {
		input_slice_start = input_slice_info->start_col;
		input_slice_size  = input_slice_info->end_col - input_slice_info->start_col + 1;
		init_phase = slice_info->init_phase_hor;

		slice_scaler->trim0_info.trim_start_x = input_slice_start - start_col_org;
		slice_scaler->trim0_info.trim_size_x  = input_slice_size;

		slice_scaler->scaler_info.scaler_in_width = input_slice_size/deci;
		slice_scaler->scaler_info.scaler_out_width = output_slice_size;

		slice_scaler->scaler_info.init_phase_info.scaler_init_phase[0] = init_phase;
		calc_scaler_phase(init_phase, scl_factor_out, &slice_scaler->scaler_info.init_phase_info.scaler_init_phase_int[0][0],
			&slice_scaler->scaler_info.init_phase_info.scaler_init_phase_rmd[0][0]);
		calc_scaler_phase(init_phase / 4, scl_factor_out / 2, &slice_scaler->scaler_info.init_phase_info.scaler_init_phase_int[0][1],
			&slice_scaler->scaler_info.init_phase_info.scaler_init_phase_rmd[0][1]);
	}
	slice_scaler->dst_start_x = output_slice_start;
	slice_scaler->dst_size_x = output_slice_size;
	slice_scaler->trim1_info.trim_start_x = 0;
	slice_scaler->trim1_info.trim_size_x = output_slice_size;

	/* ver */
	trim_start = frame_scaler->trim0_info.trim_start_y;
	trim_size = frame_scaler->trim0_info.trim_size_y;
	deci = frame_scaler->deci_info.deci_y;
	scl_en = frame_scaler->scaler_info.scaler_en;
	scl_factor_in = frame_scaler->scaler_info.scaler_factor_in_ver;
	scl_factor_out = frame_scaler->scaler_info.scaler_factor_out_ver;
	/* FIXME: 420 input */
	if (frame_scaler->input_pixfmt == YUV422)
		scl_tap = MAX(frame_scaler->scaler_info.scaler_y_ver_tap, frame_scaler->scaler_info.scaler_uv_ver_tap) + 2;
	else if (frame_scaler->input_pixfmt == YUV420)
		scl_tap = MAX(frame_scaler->scaler_info.scaler_y_ver_tap, frame_scaler->scaler_info.scaler_uv_ver_tap * 2) + 2;
	init_phase = frame_scaler->scaler_info.init_phase_info.scaler_init_phase[1];

	slice_scaler->src_size_y = slice_info->slice_height;

	overlap_head = input_slice_info->start_row - slice_info->start_row;
	overlap_tail = slice_info->end_row - input_slice_info->end_row;

	start_row_org = slice_info->start_row;/* copy for trim0 */

	slice_info->start_row += overlap_head;
	slice_info->end_row -= overlap_tail;
	slice_info->slice_height = slice_info->end_row - slice_info->start_row + 1;

	input_slice_start = slice_info->start_row;
	input_slice_size = slice_info->slice_height;
	input_pixel_align = 2;/* YUV420 */
	if (frame_scaler->output_pixfmt == YUV422)
		output_pixel_align = 2;
	else if (frame_scaler->output_pixfmt == YUV420) {
		output_pixel_align = 2;
		if (slice_info->sliceRowNo != slice_info->sliceRows-1)
			output_pixel_align = frame_scaler->output_align_ver;
	} else
		pr_err("fail to get invalid fmt %d\n", frame_scaler->output_pixfmt);

	output_slice_start = output_slice_info->start_row;
	output_slice_size = output_slice_info->end_row - output_slice_info->start_row + 1;

	if (output_slice_size == 0) {
		slice_scaler->trim0_info.trim_start_y = 0;
		slice_scaler->trim0_info.trim_size_y = 0;
	} else {
		input_slice_start = input_slice_info->start_row;
		input_slice_size = input_slice_info->end_row - input_slice_info->start_row + 1;
		init_phase = slice_info->init_phase_ver;

		slice_scaler->trim0_info.trim_start_y = input_slice_start - start_row_org;
		slice_scaler->trim0_info.trim_size_y = input_slice_size;

		slice_scaler->scaler_info.scaler_in_height = input_slice_size / deci;
		slice_scaler->scaler_info.scaler_out_height = output_slice_size;
		slice_scaler->scaler_info.init_phase_info.scaler_init_phase[1] = init_phase;
		/* FIXME: need refer to input_pixfmt */
		/* luma */
		calc_scaler_phase(init_phase, scl_factor_out, &slice_scaler->scaler_info.init_phase_info.scaler_init_phase_int[1][0],
			&slice_scaler->scaler_info.init_phase_info.scaler_init_phase_rmd[1][0]);
		/* chroma */
		if (slice_scaler->scaler_info.input_pixfmt == YUV422) {
			if (slice_scaler->scaler_info.output_pixfmt == YUV420)
				calc_scaler_phase(init_phase / 2, scl_factor_out / 2, &slice_scaler->scaler_info.init_phase_info.scaler_init_phase_int[1][1],
					&slice_scaler->scaler_info.init_phase_info.scaler_init_phase_rmd[1][1]);
			else if (slice_scaler->scaler_info.output_pixfmt == YUV422)
				calc_scaler_phase(init_phase, scl_factor_out, &slice_scaler->scaler_info.init_phase_info.scaler_init_phase_int[1][1],
					&slice_scaler->scaler_info.init_phase_info.scaler_init_phase_rmd[1][1]);
		} else if (slice_scaler->scaler_info.input_pixfmt == YUV420) {
			if (slice_scaler->scaler_info.output_pixfmt == YUV420)
				calc_scaler_phase(init_phase / 4, scl_factor_out / 2, &slice_scaler->scaler_info.init_phase_info.scaler_init_phase_int[1][1],
					&slice_scaler->scaler_info.init_phase_info.scaler_init_phase_rmd[1][1]);
			else if (slice_scaler->scaler_info.output_pixfmt == YUV422)
				calc_scaler_phase(init_phase / 2, scl_factor_out, &slice_scaler->scaler_info.init_phase_info.scaler_init_phase_int[1][1],
					&slice_scaler->scaler_info.init_phase_info.scaler_init_phase_rmd[1][1]);
		}
	}
	slice_scaler->dst_start_y = output_slice_start;
	slice_scaler->dst_size_y = output_slice_size;
	slice_scaler->trim1_info.trim_start_y = 0;
	slice_scaler->trim1_info.trim_size_y = output_slice_size;
}

void est_scaler_output_slice_info(int trim_start, int trim_size, int deci,
	int scl_en, int scl_factor_in, int scl_factor_out, int scl_tap, int init_phase,
	int input_slice_start, int input_slice_size, int output_pixel_align, int *output_slice_end)
{
	int epixel;
	int input_slice_end = input_slice_start + input_slice_size;
	int trim_end = trim_start + trim_size;

	if (scl_tap % 2 != 0)
		pr_err("fail to get valid scl_tap %d\n", scl_tap);
	if (trim_size % deci != 0)
		pr_err("fail to get valid trim size %d\n", trim_size);
	/* trim */
	input_slice_end = input_slice_end > trim_end ? trim_end : input_slice_end;
	input_slice_end = input_slice_end - trim_start;

	/* deci */
	input_slice_end = input_slice_end / deci;

	/* scale */
	epixel = (input_slice_end * scl_factor_out - 1 - init_phase) / scl_factor_in + 1;

	/* align */
	epixel = ((epixel + output_pixel_align / 2) / output_pixel_align) * output_pixel_align;

	if (epixel < 0)
		epixel = 0;

	/* output */
	*output_slice_end = epixel;
}

void est_scaler_output_slice_info_v2(int trim_start, int trim_size, int deci,
	int scl_en, int scl_factor_in, int scl_factor_out, int scl_tap, int init_phase,
	int input_slice_start, int input_slice_size, int output_pixel_align, int *output_slice_end)
{
	int epixel;
	int deci_size = trim_size / deci;
	int input_slice_end = input_slice_start + input_slice_size;
	int trim_end = trim_start + trim_size;

	if (scl_tap % 2 != 0)
		pr_err("fail to get valid scl_tap %d\n", scl_tap);
	if (trim_size % deci != 0)
		pr_err("fail to get valid trim size %d\n", trim_size);
	/* trim */
	input_slice_end = input_slice_end > trim_end ? trim_end : input_slice_end;
	input_slice_end = input_slice_end - trim_start;

	/* deci */
	input_slice_end = input_slice_end / deci;
	if (input_slice_end != deci_size)
		input_slice_end -= scl_tap / 2;

	/* scale */
	epixel = (input_slice_end * scl_factor_out - 1 - init_phase) / scl_factor_in + 1;

	/* align */
	epixel = (epixel / output_pixel_align) * output_pixel_align;

	if(epixel < 0)
		epixel = 0;

	/* output */
	*output_slice_end = epixel;
}

void calc_scaler_input_slice_info(int trim_start, int trim_size, int deci,
	int scl_en, int scl_factor_in, int scl_factor_out, int scl_tap, int init_phase,
	int output_slice_start, int output_slice_size, int input_pixel_align,
	int *input_slice_start, int *input_slice_size, int *input_slice_phase)
{
	int sphase, ephase;
	int spixel, epixel;
	int deci_size = trim_size / deci;

	if (scl_tap % 2 != 0)
		pr_err("fail to get valid scl_tap %d\n", scl_tap);
	if (trim_start % input_pixel_align != 0)
		pr_err("fail to get valid trim start %d\n", trim_start);
	if (trim_size % deci != 0)
		pr_err("fail to get valid trim size %d\n", trim_size);
	if (deci_size % input_pixel_align != 0)
		pr_err("fail to get valid deci size %d\n", deci_size);

	sphase = init_phase + output_slice_start * scl_factor_in;/* start phase */
	ephase = sphase + (output_slice_size - 1) * scl_factor_in;/* end phase (interior) */

	spixel = sphase / scl_factor_out - scl_tap / 2 + 1;/* start pixel index */
	epixel = ephase / scl_factor_out + scl_tap / 2;/* end pixel index (interior) */

	/* adjust the input slice position for alignment */
	spixel = (spixel / input_pixel_align) * input_pixel_align;/* start pixel (aligned) */
	epixel = (epixel / input_pixel_align + 1) * input_pixel_align;

	spixel = spixel < 0 ? 0 : spixel;
	epixel = epixel > deci_size ? deci_size : epixel;

	/* calculate the initial phase of current slice (local phase) */
	sphase -= spixel * scl_factor_out;

	/* calculate the input slice position of deci */
	spixel *= deci;
	epixel *= deci;

	*input_slice_start = spixel + trim_start;
	*input_slice_size = epixel - spixel;
	*input_slice_phase = sphase;
}