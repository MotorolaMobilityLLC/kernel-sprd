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

#ifndef _ALG_COMMON_CALC_H
#define _ALG_COMMON_CALC_H

#include "cam_types.h"

#define PHASE_1                         32
#define YUV422                          0
#define YUV420                          1

struct trim_info_t {
	uint8_t trim_en;
	uint16_t trim_start_x;
	uint16_t trim_start_y;
	uint16_t trim_size_x;
	uint16_t trim_size_y;
};

struct deci_info_t {
	uint8_t deci_x_en;/* 0: disable; 1:enable */
	uint8_t deci_y_en;/* 0: disable; 1:enable */
	uint8_t deci_x;/* deci factor:1,2,4,8,16 */
	uint8_t deci_y;/* deci factor:1,2,4,8,16 */
	uint8_t deciPhase_X;/* deci phase:0,1,3,7 */
	uint8_t deciPhase_Y;/* deci phase:0,1,3,7 */
	uint8_t deci_cut_first_y;
	uint8_t deci_option;/* 0:direct deci; 1:average deci; 2:only average for luma */
};

struct scaler_phase_info_t {
	int32_t scaler_init_phase[2];
	int16_t scaler_init_phase_int[2][2];/* [hor/ver][luma/chroma] */
	uint16_t scaler_init_phase_rmd[2][2];
};

struct scaler_coef_info_t {
	int16_t y_hor_coef[PHASE_1][8];/* Luma horizontal coefficients table */
	int16_t c_hor_coef[PHASE_1][8];/* Chroma horizontal coefficients table */
	int16_t y_ver_coef[PHASE_1][16];/* Luma vertical down coefficients table */
	int16_t c_ver_coef[PHASE_1][16];/* Chroma veritical down coefficients table */
};

struct scaler_info_t {
	uint8_t scaler_en;/* 0: disable; 1:enable */

	uint8_t input_pixfmt;/* input yuv format: 0=yuv422 or 1=yuv420; */
	uint8_t output_pixfmt;

	uint16_t scaler_in_width;
	uint16_t scaler_in_height;
	uint16_t scaler_out_width;
	uint16_t scaler_out_height;

	uint16_t scaler_factor_in_hor;
	uint16_t scaler_factor_out_hor;
	uint16_t scaler_factor_in_ver;
	uint16_t scaler_factor_out_ver;

	int32_t scaler_init_phase_hor;
	int32_t scaler_init_phase_ver;

	uint8_t scaler_y_hor_tap;
	uint8_t scaler_y_ver_tap;/* Y Vertical tap of scaling */
	uint8_t scaler_uv_hor_tap;
	uint8_t scaler_uv_ver_tap;

	struct scaler_phase_info_t init_phase_info;
	struct scaler_coef_info_t scaler_coef_info;
};

struct yuvscaler_param_t {
	uint8_t bypass;

	uint8_t input_pixfmt;/* 00:YUV422; 1:YUV420 */
	uint8_t output_pixfmt;/* 00:YUV422; 1:YUV420 */
	uint8_t output_align_hor;
	uint8_t output_align_ver;

	uint16_t src_size_x;
	uint16_t src_size_y;
	uint16_t dst_start_x;
	uint16_t dst_start_y;
	uint16_t dst_size_x;
	uint16_t dst_size_y;

	struct trim_info_t trim0_info;
	struct deci_info_t deci_info;
	struct scaler_info_t scaler_info;
	struct trim_info_t trim1_info;
};

struct scaler_slice_t {
	int slice_id;
	int slice_width;
	int slice_height;

	int sliceRows;
	int sliceCols;
	int sliceRowNo;
	int sliceColNo;

	int start_col;
	int start_row;
	int end_col;
	int end_row;

	int overlap_left;
	int overlap_right;
	int overlap_up;
	int overlap_down;

	int init_phase_hor;
	int init_phase_ver;
};

void calc_scaler_phase(int32_t phase, uint16_t factor,
	int16_t *phase_int, uint16_t *phase_rmd);
void yuv_scaler_init_slice_info_v3(
	struct yuvscaler_param_t *frame_scaler, struct yuvscaler_param_t *slice_scaler,
	struct scaler_slice_t *slice_info, const struct scaler_slice_t *input_slice_info,
	const struct scaler_slice_t *output_slice_info);
void est_scaler_output_slice_info(int trim_start, int trim_size, int deci,
	int scl_en, int scl_factor_in, int scl_factor_out, int scl_tap, int init_phase,
	int input_slice_start, int input_slice_size, int output_pixel_align, int *output_slice_end);
void est_scaler_output_slice_info_v2(int trim_start, int trim_size, int deci,
	int scl_en, int scl_factor_in, int scl_factor_out, int scl_tap, int init_phase,
	int input_slice_start, int input_slice_size, int output_pixel_align, int *output_slice_end);
void calc_scaler_input_slice_info(int trim_start, int trim_size, int deci,
	int scl_en, int scl_factor_in, int scl_factor_out, int scl_tap, int init_phase,
	int output_slice_start, int output_slice_size, int input_pixel_align,
	int *input_slice_start, int *input_slice_size, int *input_slice_phase);

#endif
