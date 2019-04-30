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

#include <linux/math64.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include "cam_sin_cos.h"
#include "dcam_drv.h"

#define TOTAL_PHASE          (2 * 4)
#define FILTER_TAP_H         8/* 8->12  xing.huo*/
#define FILTER_TAP_V         4
#define FILTER_TAP_MAX       12
#define FILTER_WINDOW        (1)

#define WEIGHT_BITWIDTH      (10)/* 8->10 xing.huo */
#define WEIGHT_SUM           (1 << WEIGHT_BITWIDTH)
#define fabs(x)              ((x) >= 0 ? (x) : -(x))

#define CLIP(x, maxv, minv) \
		do { \
			if (x > maxv) \
				x = maxv; \
			else if (x < minv) \
				x = minv; \
		} while (0)

#define coeff_v(coeff, i, j)   (*(coeff + FILTER_TAP_V * i + j) & 0x7FF)
#define coeff_h(coeff, i, j)   (*(coeff + FILTER_TAP_H * i + j) & 0x7FF)

static int64_t sprd_dcamrawsizer_div64(
		int64_t dividend, int64_t divisor)
{
	signed char sign = 1;
	int64_t dividend_tmp = dividend;
	int64_t divisor_tmp = divisor;
	int64_t ret = 0;

	if (divisor == 0)
		return 0;

	if ((dividend >> 63) & 0x1) {
		sign *= -1;
		dividend_tmp = dividend * (-1);
	}

	if ((divisor >> 63) & 0x1) {
		sign *= -1;
		divisor_tmp = divisor * (-1);
	}

	ret = div64_s64(dividend_tmp, divisor_tmp);
	ret *= sign;

	return ret;
}

static int sprd_dcamrawsizer_sinc_weight_calc(
		int index, int phase, uint16_t src_size, uint16_t dst_size)
{
	int weight;
	int n = 2;
	/* c = c * 2^24 */
	int64_t c1 = (int64_t)5968797;
	int64_t c2 = (int64_t)8177148;
	int64_t c3 = (int64_t)2419811;
	int64_t c4 = (int64_t)211460;

	int64_t dividend, divisor;
	int64_t angle1, angle2, angle3, angle4;
	int64_t value1, value2, value3, value4;
	int64_t tmp;

	if (FILTER_WINDOW * fabs(index) * dst_size == 0) {
		weight = div64_s64((int64_t)dst_size * (1 << 24), src_size);
	} else if (fabs(index) * dst_size * FILTER_WINDOW
			<=
			n * phase * src_size) {
		/* (y1 <= n) */
		dividend = (int64_t)(FILTER_WINDOW * fabs(index) *
		dst_size + n * phase * src_size) *
		(int64_t)(ARC_32_COEF);

		divisor = (int64_t)(n * phase * src_size);
		angle1 = sprd_dcamrawsizer_div64(dividend, divisor);


		dividend = (int64_t)(FILTER_WINDOW * fabs(index) *
		dst_size + n * phase * src_size) *
		(int64_t)2 * (int64_t)ARC_32_COEF;

		divisor = (int64_t)(n * phase * src_size);
		angle2 = sprd_dcamrawsizer_div64(dividend, divisor);


		dividend = (int64_t)(FILTER_WINDOW * fabs(index) *
				dst_size + n * phase * src_size) *
				(int64_t)3 * (int64_t)ARC_32_COEF;

		divisor = (int64_t)(n * phase * src_size);
		angle3 = sprd_dcamrawsizer_div64(dividend, divisor);


		dividend = (int64_t)(FILTER_WINDOW * fabs(index) *
				dst_size) * (int64_t)ARC_32_COEF;

		divisor = (int64_t)(phase * src_size);
		angle4 = sprd_dcamrawsizer_div64(dividend, divisor);


		value1 = cam_cos_32((int)angle1);
		value2 = cam_cos_32((int)angle2);
		value3 = cam_cos_32((int)angle3);
		value4 = cam_sin_32((int)angle4);

		tmp = c1 -
			((c2 * value1) >> 30) +
			((c3 * value2) >> 30) -
			((c4 * value3) >> 30);

		tmp = (tmp * value4) >> 30;

		dividend = tmp * (int64_t)phase;
		divisor = (int64_t)PI_32 * (int64_t)FILTER_WINDOW *
				fabs((int64_t)index);
		divisor = divisor >> 10;
		dividend = dividend << 22;

		weight = (int)(sprd_dcamrawsizer_div64(dividend, divisor));
	} else {
		weight = 0;
	}

	return weight;
}

static void sprd_dcamrawsizer_weight_normalize(
		int16_t *norm_weights, int *tmp_weights, int kk)
{
	/* fix_point version normalize_weight function */
	int weight_max = (1 << WEIGHT_BITWIDTH) - 1;
	/* int weight_min = -1 * (1<<WEIGHT_BITWIDTH); */
	int64_t tmp_sum = 0;
	int temp_value;
	int64_t tmp_weight64 = 0;
	u_int8_t i;
	int sum = 0;
	int diff;

	for (i = 0; i < kk; ++i)
		tmp_sum += tmp_weights[i];

	for (i = 0; i < kk; ++i) {
		tmp_weight64 = ((int64_t)tmp_weights[i] * WEIGHT_SUM);
		temp_value = div64_s64(tmp_weight64, tmp_sum);
		norm_weights[i] = (int16_t)(temp_value);
		sum += norm_weights[i];
	}

	if (norm_weights[kk >> 1] >= weight_max) {
		diff = norm_weights[kk >> 1] - weight_max;
		norm_weights[kk >> 1] = weight_max;
		norm_weights[(kk >> 1) - 1] += diff;
	}

	if (norm_weights[(kk >> 1) - 1] >= weight_max) {
		diff = norm_weights[(kk >> 1) - 1] - weight_max;
		norm_weights[(kk >> 1) - 1] = weight_max;
		norm_weights[kk >> 1] += diff;
	}

	if (sum != WEIGHT_SUM) {
		diff = WEIGHT_SUM - sum;
		if (diff > 0)
			norm_weights[kk >> 1] = norm_weights[kk >> 1] + diff;
		else
			norm_weights[0] = norm_weights[0] + diff;
	}
}

static void sprd_dcamrawsizer_scaler_weight_calc(
		uint16_t src_size, uint16_t dst_size,
		u_int8_t hor_or_ver, int16_t *filter_weight,
		u_int8_t *filter_phase, u_int8_t *filter_tap)
{
	int N = TOTAL_PHASE;
	int kw, tap;
	int i, index, phase, idx_lb, idx_ub;
	int weight_phase[FILTER_TAP_MAX] = {0};
	int (*weight_func)(int, int, uint16_t, uint16_t) = NULL;

	if (hor_or_ver) {
		kw = 4;
		tap = FILTER_TAP_H;
		weight_func = sprd_dcamrawsizer_sinc_weight_calc;
	} else {
		kw = 2;
		tap = FILTER_TAP_V;
		weight_func = sprd_dcamrawsizer_sinc_weight_calc;
	}

	idx_ub =
		(kw *
		TOTAL_PHASE * src_size + 2 * dst_size - 1) /
		(2 * dst_size);

	idx_lb = -idx_ub;

	for (phase = 0; phase < N; phase++) {
		int offset = -tap / 2 + 1;

		for (i = 0; i < tap; i++) {
			index = N * (i+offset) - phase;
			CLIP(index, idx_ub, idx_lb);
			weight_phase[i] =
				weight_func(index, N, src_size, dst_size);
		}

		sprd_dcamrawsizer_weight_normalize(
			filter_weight, weight_phase, tap);
		filter_weight += tap;
	}

	*filter_phase = N;
	*filter_tap = tap;
}

int sprd_dcam_raw_sizer_coeff_gen(
		uint16_t src_width, uint16_t src_height,
		uint16_t dst_width, uint16_t dst_height,
		uint32_t *coeff_buf, uint32_t is_zoom_min)
{
	int16_t hor_weight_table[TOTAL_PHASE * FILTER_TAP_H];
	int16_t ver_weight_table[TOTAL_PHASE * FILTER_TAP_V];
	int i = 0;
	uint8_t hor_N = 0, hor_tap = 0;
	uint8_t ver_N = 0, ver_tap = 0;

	if (!coeff_buf)
		return -1;

	if (is_zoom_min) {
		for (i = 0; i < TOTAL_PHASE*FILTER_TAP_H; i++)
			hor_weight_table[i] = 0;
		for (i = 0; i < TOTAL_PHASE*FILTER_TAP_V; i++)
			ver_weight_table[i] = 0;

		for (i = 0; i < TOTAL_PHASE; ++i) {
			int j = 0;

			for (j = 3; j <= 4; ++j)
				hor_weight_table[i*FILTER_TAP_H + j] = 512;
		}

		for (i = 0; i < TOTAL_PHASE; ++i) {
			int j = 0;

			for (j = 1; j <= 2; ++j)
				ver_weight_table[i*FILTER_TAP_V + j] = 512;
		}
	} else {
		sprd_dcamrawsizer_scaler_weight_calc(src_width, dst_width, 1,
			hor_weight_table, &hor_N, &hor_tap);

		sprd_dcamrawsizer_scaler_weight_calc(src_height, dst_height, 0,
			ver_weight_table, &ver_N, &ver_tap);
	}

	for (i = 0; i < (TOTAL_PHASE); i++) {
		*coeff_buf++ = ((uint32_t)
			coeff_v(ver_weight_table, i, 0) << 16)
			+ coeff_v(ver_weight_table, i, 1);

		*coeff_buf++ = ((uint32_t)
			coeff_v(ver_weight_table, i, 2) << 16)
			+ coeff_v(ver_weight_table, i, 3);
	}

	for (i = 0; i < (TOTAL_PHASE); i++) {
		*coeff_buf++ = ((uint32_t)
			coeff_h(hor_weight_table, i, 0) << 16)
			+ coeff_h(hor_weight_table, i, 1);

		*coeff_buf++ = ((uint32_t)
			coeff_h(hor_weight_table, i, 2) << 16)
			+ coeff_h(hor_weight_table, i, 3);

		*coeff_buf++ = ((uint32_t)
			coeff_h(hor_weight_table, i, 4) << 16)
			+ coeff_h(hor_weight_table, i, 5);

		*coeff_buf++ = ((uint32_t)
			coeff_h(hor_weight_table, i, 6) << 16)
			+ coeff_h(hor_weight_table, i, 7);
	}
	return 0;
}
