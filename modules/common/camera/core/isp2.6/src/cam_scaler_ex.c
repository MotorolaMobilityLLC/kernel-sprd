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

#include <linux/math64.h>
#include <linux/mm.h>

#include "cam_scaler_ex.h"
/* Macro Definitions */

#define SCALER_COEF_TAB_LEN_HOR         192
#define SCALER_COEF_TAB_LEN_VER         512

#define GSC_FIX                         24
#define GSC_COUNT                       1024
#define GSC_ABS(_a)                     ((_a) < 0 ? -(_a) : (_a))
#define GSC_SIGN2(input, p)             \
	{ if (p >= 0) input = 1; if (p < 0) input = -1; }
#define COEF_ARR_ROWS                   32
#define COEF_ARR_COLUMNS                8
#define COEF_ARR_COL_MAX                16
#define MIN_POOL_SIZE                   (6 * 1024)
#define TRUE                            1
#define FALSE                           0
#define SCI_MEMSET                      memset

#define SINCOS_BIT_31                   (1 << 31)
#define SINCOS_BIT_30                   (1 << 30)
#define SMULL(x, y)                     ((int64_t)(x) * (int64_t)(y))

#define YUV422  0
#define YUV420  1

struct gsc_mem_pool {
	unsigned long begin_addr;
	unsigned int total_size;
	unsigned int used_size;
};


/* Static Variables */

static const unsigned int cossin_tbl[64] = {
	0x3ffec42d, 0x00c90e90, 0x3ff4e5e0, 0x025b0caf,
	0x3fe12acb, 0x03ecadcf, 0x3fc395f9, 0x057db403,
	0x3f9c2bfb, 0x070de172, 0x3f6af2e3, 0x089cf867,
	0x3f2ff24a, 0x0a2abb59, 0x3eeb3347, 0x0bb6ecef,
	0x3e9cc076, 0x0d415013, 0x3e44a5ef, 0x0ec9a7f3,
	0x3de2f148, 0x104fb80e, 0x3d77b192, 0x11d3443f,
	0x3d02f757, 0x135410c3, 0x3c84d496, 0x14d1e242,
	0x3bfd5cc4, 0x164c7ddd, 0x3b6ca4c4, 0x17c3a931,
	0x3ad2c2e8, 0x19372a64, 0x3a2fcee8, 0x1aa6c82b,
	0x3983e1e8, 0x1c1249d8, 0x38cf1669, 0x1d79775c,
	0x3811884d, 0x1edc1953, 0x374b54ce, 0x2039f90f,
	0x367c9a7e, 0x2192e09b, 0x35a5793c, 0x22e69ac8,
	0x34c61236, 0x2434f332, 0x33de87de, 0x257db64c,
	0x32eefdea, 0x26c0b162, 0x31f79948, 0x27fdb2a7,
	0x30f8801f, 0x29348937, 0x2ff1d9c7, 0x2a650525,
	0x2ee3cebe, 0x2b8ef77d, 0x2dce88aa, 0x2cb2324c
};


/* Internal Function Implementation */

/* s(x) = sin(2*PI*x*2^(-32)) =s(a) * cos(PI*(x-a)*2^(-31)) +
 * c(a) * sin(PI*(x-a)*2^(-31))
 * b = x - a, A = PI*a*2^(-31), B = PI*(x-a)*2^(-32)
 * PI / 128 <= A < PI/4,  0 <= B < PI / 128
 * sin(B) = B - B ^ 3 / 6;  cos(B)=1 - B ^ 2 / 2;
 * s(x) = sin(A) * cos(B) + cos(A) * sin(B)
 * s(x) = sin(A) - sin(A) * B^2 / 2 + cos(A) * B - cos(A) * B * B^2 / 6
 * s(x) = S1 - S2 + S3 + S4;
 * S1 = sin(A), S2 = sin(A) * B^2 / 2, S3 = cos(A) * B,
 * S4 = - cos(A) * B * B^2 / 6
 * T1 = B^2;
 */
/* lint --e{647} */
static int camscaler_sin_core(int arc_q33, int sign)
{
	int a = 0, b = 0, B = 0;
	int sin_a = 0, cos_a = 0;
	int S1 = 0, S2 = 0, S3 = 0, S4 = 0, T1 = 0;
	int R = 0;

	/* round(2 * PI * 2 ^ 28) */
	int C1 = 0x6487ed51;
	/* 0xd5555556, round(-2^32/6) */
	int C2 = -715827882;

	a = arc_q33 >> 25;
	if (a >= 0 && (2 * a) < 64)
		cos_a = cossin_tbl[2 * a];
 	if (((2 * a) + 1) >= 0 && ((2 * a) + 1) < 64)
 		sin_a = cossin_tbl[2 * a + 1];
 	cos_a ^= (sign >> 31);
 	sin_a ^= (sign >> 31);

 	a = a << 25;
 	b = arc_q33 - a;
 	b -= (1 << 24);

 	/* B at Q32 */
 	B = SMULL((b << 3), C1) >> 32;

 	/* B^2 at Q32 */
 	T1 = SMULL(B, B) >> 32;

 	S1 = sin_a;
 	/* S2 at Q30 */
 	S2 = SMULL(sin_a, T1 >> 1) >> 32;
 	/* S3 at Q30 */
 	S3 = SMULL(cos_a, B) >> 32;
 	/* -B^2 / 6 at Q32 */
 	S4 = SMULL(T1, C2) >> 32;
 	/* S4 at Q30 */
 	S4 = SMULL(S3, S4) >> 32;

 	R = S1 - S2 + S3 + S4;

 	return R;
 }

 /* c(x) = cos(2*PI*x*2^(-32)) = c(a) * cos(PI*(x-a)*2^(-31)) +
  * s(a) * sin(PI*(x-a)*2^(-31))
  * b = x - a, A = PI*a*2^(-31), B = PI*(x-a)*2^(-32)
  * PI / 128 <= A < PI/4,  0 <= B < PI / 128
  * sin(B) = B - B ^ 3 / 6;  cos(B)=1 - B ^ 2 / 2;
  * s(x) = cos(A) * cos(B) - sin(A) * sin(B)
  * s(x) = cos(A)  - cos(A) * B^2 / 2 - sin(A) * B + sin(A) * B * B^2 / 6;
  * s(x) = S1 - S2 - S3 + S4;
  * S1 = cos(A), S2 = cos(A) * B^2 / 2, S3 = sin(A) * B,
  * S4 = sin(A) * B * B^2 / 6
  * T1 = B^2;
  */
 /* lint --e{647} */
 static int camscaler_cos_core(int arc_q33, int sign)
 {
 	int a = 0, b = 0, B = 0;
 	int sin_a = 0, cos_a = 0;
 	int S1 = 0, S2 = 0, S3 = 0, S4 = 0, T1 = 0;
 	int R = 0;

 	/* round(2 * PI * 2 ^ 28) */
 	int C1 = 0x6487ed51;
 	/* round(2^32/6) */
 	int C2 = 0x2aaaaaab;

 	a = arc_q33 >> 25;
 	if (a >= 0 && (2 * a) < 64)
 		cos_a = cossin_tbl[a * 2];
 	if (((2 * a) + 1) >= 0 && ((2 * a) + 1) < 64)
 		sin_a = cossin_tbl[a * 2 + 1];
 	/* correct the sign */
 	cos_a ^= (sign >> 31);
 	/* correct the sign */
 	sin_a ^= (sign >> 31);

 	a = a << 25;
 	b = arc_q33 - a;
 	b -= (1 << 24);

 	/* B at Q32 */
 	B = SMULL((b << 3), C1) >> 32;

 	/* B^2 at Q32 */
 	T1 = SMULL(B, B) >> 32;

 	S1 = cos_a;
 	/* S2 at Q30 */
 	S2 = SMULL(cos_a, T1 >> 1) >> 32;
 	/* S3 at Q30 */
 	S3 = SMULL(sin_a, B) >> 32;
 	/* B^2 / 6 at Q32 */
 	S4 = SMULL(T1, C2) >> 32;
 	/* S4 at Q30 */
 	S4 = SMULL(S3, S4) >> 32;

 	R = S1 - S2 - S3 + S4;

 	return R;
}

 /* API Function Implementation */
 /****************************************************************************/
 /* Purpose: get the sin value of an arc at Q32                              */
 /* Author:                                                                  */
 /* Input:   arc at Q32                                                      */
 /* Output:  none                                                            */
 /* Return:  sin value at Q30                                                */
 /* Note:    arc at Q32 = arc * (2 ^ 32)                                     */
 /*          value at Q30 = value * (2 ^ 30)                                 */
 /****************************************************************************/
 /* lint --e{648} */
 static int camscaler_cam_sin_32(int n)
 {
 	/* if s equal to 1, the sin value is negative */
 	int s = n & SINCOS_BIT_31;

 	/* angle in revolutions at Q33, the BIT_31 only indicates the sign */
 	n = n << 1;

 	/* == pi, 0 */
 	if (n == 0)
 		return 0;

 	/* >= pi/2 */
 	if (SINCOS_BIT_31 == (n & SINCOS_BIT_31)) {
 		/* -= pi/2 */
 		n &= ~SINCOS_BIT_31;

 		if (n < SINCOS_BIT_30) {
 			/* < pi/4 */
 			return camscaler_cos_core(n, s);
 		} else if (n == SINCOS_BIT_30) {
 			/* == pi/4 */
 			n -= 1;
 		} else if (n > SINCOS_BIT_30) {
 			/* > pi/4, pi/2 - n */
 			n = SINCOS_BIT_31 - n;
 		}

 		return camscaler_sin_core(n, s);
 	}
 	if (n < SINCOS_BIT_30) {
 		/* < pi/4 */
 		return camscaler_sin_core(n, s);
 	} else if (n == SINCOS_BIT_30) {
 		/* == pi/4 */
 		n -= 1;
 	} else if (n > SINCOS_BIT_30) {
 		/* > pi/4, pi/2 - n */
 		n = SINCOS_BIT_31 - n;
 	}

 	return camscaler_cos_core(n, s);
 }

 /****************************************************************************/
 /* Purpose: get the cos value of an arc at Q32                              */
 /* Author:                                                                  */
 /* Input:   arc at Q32                                                      */
 /* Output:  none                                                            */
 /* Return:  cos value at Q30                                                */
 /* Note:    arc at Q32 = arc * (2 ^ 32)                                     */
 /*          value at Q30 = value * (2 ^ 30)                                 */
 /****************************************************************************/
 /* lint --e{648} */
 static int camscaler_cam_cos_32(int n)
 {
 	/* if s equal to 1, the sin value is negative */
 	int s = n & SINCOS_BIT_31;

 	/* angle in revolutions at Q33, the BIT_31 only indicates the sign */
 	n = n << 1;

 	if (n == SINCOS_BIT_31)
 		return 0;

 	/* >= pi/2 */
 	if (SINCOS_BIT_31 == (n & SINCOS_BIT_31)) {
 		/* -= pi/2 */
 		n &= ~SINCOS_BIT_31;

 		if (n < SINCOS_BIT_30) {
 			/* < pi/4 */
 			return -camscaler_sin_core(n, s);
 		} else if (n == SINCOS_BIT_30) {
 			/* == pi/4 */
 			n -= 1;
 		} else if (n > SINCOS_BIT_30) {
 			/* > pi/4, pi/2 - n */
 			n = SINCOS_BIT_31 - n;
 		}

 		return -camscaler_cos_core(n, s);
 	}
 	if (n < SINCOS_BIT_30) {
 		/* < pi/4 */
 		return camscaler_cos_core(n, s);
 	} else if (n == SINCOS_BIT_30) {
 		/* == pi/4 */
 		n -= 1;
 	} else if (n > SINCOS_BIT_30) {
 		/* > pi/4, pi/2 - n */
 		n = SINCOS_BIT_31 - n;
 	}

 	return camscaler_sin_core(n, s);
 }

 static unsigned char camscaler_init_pool(void *buffer_ptr, unsigned int buffer_size,
 	struct gsc_mem_pool *pool_ptr)
 {
 	if (NULL == buffer_ptr || 0 == buffer_size
 		|| NULL == pool_ptr)
 		return FALSE;

 	if (buffer_size < MIN_POOL_SIZE)
		return FALSE;

 	pool_ptr->begin_addr = (unsigned long) buffer_ptr;
 	pool_ptr->total_size = buffer_size;
 	pool_ptr->used_size = 0;

 	return TRUE;
 }

 static void *camscaler_mem_alloc(unsigned int size, unsigned int align_shift,
 	struct gsc_mem_pool *pool_ptr)
 {
 	unsigned long begin_addr = 0;
 	unsigned long temp_addr = 0;

 	if (pool_ptr == NULL)
		return NULL;

 	begin_addr = pool_ptr->begin_addr;
 	temp_addr = begin_addr + pool_ptr->used_size;
 	temp_addr = ((temp_addr + (1 << align_shift) - 1) >> align_shift) <<
		align_shift;
	if (temp_addr + size > begin_addr + pool_ptr->total_size)
		return NULL;

	pool_ptr->used_size = (temp_addr + size) - begin_addr;
	SCI_MEMSET((void *)temp_addr, 0, size);

 	return (void *)temp_addr;
 }

 static int64_t camscaler_div64_s64_s64(int64_t dividend, int64_t divisor)
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

 static void camscaler_normalize_inter(int64_t *data, short *int_data,
 	unsigned char ilen)
 {
 	unsigned char it;
 	int64_t tmp_d = 0;
 	int64_t *tmp_data = NULL;
 	int64_t tmp_sum_val = 0;

 	tmp_data = data;
 	tmp_sum_val = 0;

 	for (it = 0; it < ilen; it++)
 		tmp_sum_val += tmp_data[it];

 	if (tmp_sum_val == 0) {
 		unsigned char value = 256 / ilen;

 		for (it = 0; it < ilen; it++) {
 			tmp_d = value;
 			int_data[it] = (short)tmp_d;
 		}
 	} else {
 		for (it = 0; it < ilen; it++) {
 			tmp_d = camscaler_div64_s64_s64(tmp_data[it] * (int64_t)256,
 				tmp_sum_val);
 			int_data[it] = (unsigned short)tmp_d;
 		}
 	}
 }

 static short camscaler_sum_fun(short *data, signed char ilen)
 {
 	signed char i;
 	short tmp_sum;

 	tmp_sum = 0;

 	for (i = 0; i < ilen; i++)
 		tmp_sum += *data++;

 	return tmp_sum;
 }

 static void camscaler_filter_inter_adjust(short *filter, unsigned char ilen)
 {
 	int i, midi;
 	int tmpi, tmp_S;
 	int tmp_val = 0;

 	tmpi = camscaler_sum_fun(filter, ilen) - 256;
 	midi = ilen >> 1;
 	GSC_SIGN2(tmp_val, tmpi);

 	if ((tmpi & 1) == 1) {
 		filter[midi] = filter[midi] - tmp_val;
 		tmpi -= tmp_val;
 	}

 	tmp_S = GSC_ABS(tmpi / 2);
 	if ((ilen & 1) == 1) {
 		for (i = 0; i < tmp_S; i++) {
 			filter[midi - (i + 1)] =
 				filter[midi - (i + 1)] - tmp_val;
 			filter[midi + (i + 1)] =
 				filter[midi + (i + 1)] - tmp_val;
 		}
 	} else {
 		for (i = 0; i < tmp_S; i++) {
 			filter[midi - (i + 1)] =
 				filter[midi - (i + 1)] - tmp_val;
 			filter[midi + i] = filter[midi + i] - tmp_val;
 		}
 	}

 	if (filter[midi] > 255) {
 		tmp_val = filter[midi];
 		filter[midi] = 255;
 		filter[midi - 1] = filter[midi - 1] + tmp_val - 255;
 	}
 }

 static short camscaler_y_model_coef_cal(short coef_length, short *coef_data_ptr,
 	short n, short m, struct gsc_mem_pool *pool_ptr)
 {
 	signed char mount;
 	short i, mid_i, kk, j, sum_val;
 	short *normal_filter;
 	int value_x, value_y, angle_32;
 	int64_t *filter;
 	int64_t *tmp_filter;
 	int64_t dividend, divisor;
 	int64_t angle_x, angle_y;
 	int64_t a, b, t;

 	filter = camscaler_mem_alloc(GSC_COUNT * sizeof(int64_t),
 		3, pool_ptr);
 	tmp_filter = camscaler_mem_alloc(GSC_COUNT * sizeof(int64_t),
 		3, pool_ptr);
 	normal_filter = camscaler_mem_alloc(GSC_COUNT * sizeof(short),
 		2, pool_ptr);

 	if (NULL == filter || NULL == tmp_filter
 		|| NULL == normal_filter)
 		return 1;

 	mid_i = coef_length >> 1;
 	filter[mid_i] = camscaler_div64_s64_s64((int64_t)((int64_t)n << GSC_FIX),
 		(int64_t)MAX(m, n));
 	for (i = 0; i < mid_i; i++) {
 		dividend = (int64_t)
 			((int64_t)ARC_32_COEF * (int64_t)(i + 1) * (int64_t)n);
 		divisor = (int64_t)((int64_t)MAX(m, n) * (int64_t)COEF_ARR_ROWS);
 		angle_x = camscaler_div64_s64_s64(dividend, divisor);

 		dividend = (int64_t)
 			((int64_t)ARC_32_COEF * (int64_t)(i + 1) * (int64_t)n);
 		divisor = (int64_t)((int64_t)(m * n) * (int64_t)COEF_ARR_ROWS);
 		angle_y = camscaler_div64_s64_s64(dividend, divisor);

 		value_x = camscaler_cam_sin_32((int)angle_x);
 		value_y = camscaler_cam_sin_32((int)angle_y);

 		dividend = (int64_t)
 			((int64_t)value_x * (int64_t)(1 << GSC_FIX));
 		divisor = (int64_t)((int64_t)m * (int64_t)value_y);
 		filter[mid_i + i + 1] = camscaler_div64_s64_s64(dividend, divisor);
 		filter[mid_i - (i + 1)] = filter[mid_i + i + 1];
 	}

 	for (i = -1; i < mid_i; i++) {
 		dividend = (int64_t)((int64_t)2 * (int64_t)(mid_i - i - 1) *
 			(int64_t)ARC_32_COEF);
 		divisor = (int64_t)coef_length;
 		angle_32 = (int)camscaler_div64_s64_s64(dividend, divisor);

 		a = (int64_t)9059697;
 		b = (int64_t)7717519;

 		t = a - ((b * camscaler_cam_cos_32(angle_32)) >> 30);

 		filter[mid_i + i + 1] = (t * filter[mid_i + i + 1]) >> GSC_FIX;
 		filter[mid_i - (i + 1)] = filter[mid_i + i + 1];
 	}

 	for (i = 0; i < COEF_ARR_ROWS; i++) {
 		mount = 0;
 		for (j = i; j < coef_length; j += COEF_ARR_ROWS) {
 			tmp_filter[mount] = filter[j];
 			mount++;
 		}
 		camscaler_normalize_inter(tmp_filter, normal_filter, (signed char) mount);
 		sum_val = camscaler_sum_fun(normal_filter, mount);
 		if (sum_val != 256)
 			camscaler_filter_inter_adjust(normal_filter, mount);

 		mount = 0;
 		for (kk = i; kk < coef_length; kk += COEF_ARR_ROWS) {
 			coef_data_ptr[kk] = normal_filter[mount];
 			mount++;
 		}
 	}

 	return 0;
 }

 static short camscaler_y_scaling_coef_cal(short tap, short d, short i,
 	short *y_coef_data_ptr, short dir, struct gsc_mem_pool *pool_ptr)
 {
 	short coef_length;

 	if (dir == 1) {
 		coef_length = (short)(tap * COEF_ARR_ROWS);
 		SCI_MEMSET(y_coef_data_ptr, 0, coef_length * sizeof(short));
 		camscaler_y_model_coef_cal(coef_length, y_coef_data_ptr, i, d, pool_ptr);
 	} else {
 		if (d > i)
 			coef_length = (short)(tap * COEF_ARR_ROWS);
 		else
 			coef_length = (short)(4 * COEF_ARR_ROWS);

        SCI_MEMSET(y_coef_data_ptr, 0, coef_length * sizeof(short));
 		camscaler_y_model_coef_cal(coef_length, y_coef_data_ptr, i, d, pool_ptr);
 	}

 	return coef_length;
 }

 static short camscaler_uv_scaling_coef_cal(short tap, short d, short i,
 				short *uv_coef_data_ptr, short dir,
 				struct gsc_mem_pool *pool_ptr)
 {
 	short uv_coef_length;

 	if (dir == 1) {
 		uv_coef_length = (short)(tap * COEF_ARR_ROWS);
 		camscaler_y_model_coef_cal(uv_coef_length,
 			uv_coef_data_ptr,
 			i, d, pool_ptr);
 	} else {
 		if (d > i)
 			uv_coef_length = (short)(tap * COEF_ARR_ROWS);
 		else
 			uv_coef_length = (short)(4 * COEF_ARR_ROWS);

 		camscaler_y_model_coef_cal(uv_coef_length,
 			uv_coef_data_ptr,
 			i, d, pool_ptr);
 	}

 	return uv_coef_length;
 }

 static void camscaler_filter_get(short *coef_data_ptr, short *out_filter,
 	short iI_hor, short coef_len, short *filter_len)
 {
 	short i, pos_start;

 	pos_start = coef_len / 2;
 	while (pos_start >= iI_hor)
 		pos_start -= iI_hor;

 	for (i = 0; i < iI_hor; i++) {
 		short len = 0;
 		short j;
 		short pos = pos_start + i;

 		while (pos >= iI_hor)
 			pos -= iI_hor;

 		for (j = 0; j < coef_len; j += iI_hor) {
 			*out_filter++ = coef_data_ptr[j + pos];
 			len++;
 		}
 		*filter_len++ = len;
 	}
 }

 static void camscaler_scaler_coef_write(short *dst_coef_ptr,
 	short *coef_ptr, short dst_pitch, short src_pitch)
 {
 	int i, j;

 	for (i = 0; i < COEF_ARR_ROWS; i++) {
 		for (j = 0; j < src_pitch; j++)
 			*(dst_coef_ptr + j) =
 				*(coef_ptr + i * src_pitch + src_pitch - 1 - j);

 		dst_coef_ptr += dst_pitch;
 	}
 }

 static void camscaler_hor_register_coef_set(unsigned int *reg_coef_lum_ptr, unsigned int *reg_coef_ch_ptr,
 	short *y_coef_ptr, short *uv_coef_ptr)
 {
 	int i = 0;
 	int j = 0;
 	int k = 0;
 	unsigned int val = 0;
 	short y_coef_arr[32][8];
	short uv_coef_arr[32][8];
	if (y_coef_ptr == NULL || uv_coef_ptr == NULL) {
		pr_debug("coef_ptr is NULL!\n");
		return;
	}

 	for (i = 0; i < 32; i++) {
		for (j = 0; j < 8; j++)
 			y_coef_arr[i][j] = *(y_coef_ptr + i * 8 + j);
 	}

	for (i = 0;i < 32; i++) {
		for (j = 0; j < 8; j++)
			uv_coef_arr[i][j] = *(uv_coef_ptr + i * 8 + j);
	}

	for (j = 0; j < 4; j++) {
		for (k = 0; k < 8; k++) {
			val = (((unsigned int)y_coef_arr[k + 8*j][1] & 0x1FF) << 16) | ((unsigned int)y_coef_arr[k + 8*j][0] & 0x1FF);
			*reg_coef_lum_ptr++ = val;
			val = (((unsigned int)y_coef_arr[k + 8*j][3] & 0x1FF) << 16) | ((unsigned int)y_coef_arr[k + 8*j][2] & 0x1FF);
			*reg_coef_lum_ptr++ = val;
			val= (((unsigned int)y_coef_arr[k + 8*j][5] & 0x1FF) << 16) | ((unsigned int)y_coef_arr[k + 8*j][4] & 0x1FF);
			*reg_coef_lum_ptr++ = val;
			val = (((unsigned int)y_coef_arr[k + 8*j][7] & 0x1FF) << 16) | ((unsigned int)y_coef_arr[k + 8*j][6] & 0x1FF);
			*reg_coef_lum_ptr++ = val;
		}

		for (k = 0; k < 8; k++) {
			val = (((unsigned int)uv_coef_arr[k + 8*j][1] & 0x1FF) << 16) | ((unsigned int)uv_coef_arr[k + 8*j][0] & 0x1FF);
			*reg_coef_ch_ptr = val;
			reg_coef_ch_ptr++;
			val = (((unsigned int)uv_coef_arr[k + 8*j][3] & 0x1FF) << 16) | ((unsigned int)uv_coef_arr[k + 8*j][2] & 0x1FF);
			*reg_coef_ch_ptr = val;
			reg_coef_ch_ptr++;
		}
	}
 }

 static void camscaler_ver_register_coef_set(unsigned int *reg_coef_lum_ptr,
 				unsigned int *reg_coef_ch_ptr,
 				short *y_coef_ptr,
 				short *uv_coef_ptr,
 				short i_h,
 				short o_h,
 				unsigned char i_pixfmt,
 				unsigned char o_pixfmt)
 {
 	unsigned int i = 0, j = 0;

	for (i = 0; i < 32; i++) {
		for (j = 0; j < 8; j++)
			reg_coef_lum_ptr[i * 8 + j] = *(y_coef_ptr + i * 16 + j);
	}

	for (i = 0; i < 32; i++) {
		for (j = 0; j < 8; j++)
			reg_coef_ch_ptr[i * 8+j] = *(uv_coef_ptr + i * 16 + j);

	}
 }

 static void camscaler_coef_range_check(short *coef_ptr, short rows,
 	short columns, short pitch)
 {
 	short i, j;
 	short value, diff, sign;
 	short *coef_arr[COEF_ARR_ROWS] = { NULL };

 	for (i = 0; i < COEF_ARR_ROWS; i++) {
 		coef_arr[i] = coef_ptr;
 		coef_ptr += pitch;
 	}

 	for (i = 0; i < rows; i++) {
 		for (j = 0; j < columns; j++) {
 			value = coef_arr[i][j];
 			if (value > 255) {
 				diff = value - 255;
 				coef_arr[i][j] = 255;
 				sign = GSC_ABS(diff);
 				if ((sign & 1) == 1) {
 					/* ilen is odd */
 					coef_arr[i][j + 1] += (diff + 1) / 2;
 					coef_arr[i][j - 1] += (diff - 1) / 2;
 				} else {
 					/* ilen is even */
 					coef_arr[i][j + 1] += (diff) / 2;
 					coef_arr[i][j - 1] += (diff) / 2;
 				}
 			}
 		}
 	}
 	pr_debug("%s\n", __func__);
 	for (i = 0; i < rows; i++) {
 		for (j = 0; j < columns; j++)
 			pr_debug("%d ", coef_arr[i][j]);
 		pr_debug("\n");
 	}
 }

 static void camscaler_ver_edge_coef_cal(short *coeff_ptr, short d, short i,
 	short tap, short pitch)
 {
 	int phase_temp[9];
 	int acc = 0;
 	int i_sample_cnt = 0;
 	unsigned char phase = 0;
 	unsigned char spec_tap = 0;
 	int l;
 	short j, k;
 	short *coeff_arr[COEF_ARR_ROWS];

 	for (k = 0; k < COEF_ARR_ROWS; k++) {
 		coeff_arr[k] = coeff_ptr;
 		coeff_ptr += pitch;
 	}

 	for (j = 0; j <= 8; j++)
 		phase_temp[j] = j * i / 8;

 	for (k = 0; k < i; k++) {
 		spec_tap = k & 1;
 		while (acc >= i) {
 			acc -= i;
 			i_sample_cnt++;
 		}

 		for (j = 0; j < 8; j++) {
 			if (acc >= phase_temp[j] && acc < phase_temp[j + 1]) {
 				phase = (unsigned char)j;
 				break;
 			}
 		}

 		for (j = (1 - tap / 2); j <= (tap / 2); j++) {
 			l = i_sample_cnt + j;
 			if (l <= 0)
 				coeff_arr[8][spec_tap] +=
 					coeff_arr[phase][j + tap / 2 - 1];
 			else if (l >= d - 1)
 				coeff_arr[8][spec_tap + 2] +=
 					coeff_arr[phase][j + tap / 2 - 1];
 		}
 		acc += d;
 	}
 }

 /****************************************************************************/
 /* Purpose: generate scale factor                                           */
 /* Author:                                                                  */
 /* Input:                                                                   */
 /*          i_w:    source image width                                      */
 /*          i_h:    source image height                                     */
 /*          o_w:    target image width                                      */
 /*          o_h:    target image height                                     */
 /* Output:                                                                  */
 /*          coeff_h_ptr: pointer of horizontal coefficient buffer,          */
 /*                       the size of which must be at least                 */
 /*                       SCALER_COEF_TAP_NUM_HOR * 4 bytes                  */
 /*                       the output coefficient will be located in          */
 /*                       coeff_h_ptr[0], ......,                            */
 /*                       coeff_h_ptr[SCALER_COEF_TAP_NUM_HOR-1]             */
 /*          coeff_v_ptr: pointer of vertical coefficient buffer,            */
 /*                       the size of which must be at least                 */
 /*                       (SCALER_COEF_TAP_NUM_VER + 1) * 4 bytes            */
 /*                       the output coefficient will be located in          */
 /*                       coeff_v_ptr[0], ......,                            */
 /*                       coeff_h_ptr[SCALER_COEF_TAP_NUM_VER-1] and         */
 /*                       the tap number will be located in                  */
 /*                       coeff_h_ptr[SCALER_COEF_TAP_NUM_VER]               */
 /*          temp_buf_ptr: temp buffer used while generate the coefficient   */
 /*          temp_buf_ptr: temp buffer size, (21 << 12) is the suggest size  */
 /* Return:                                                                  */
 /* Note:                                                                    */
 /****************************************************************************/

unsigned char cam_scaler_isp_scale_coeff_gen_ex(short i_w, short i_h,
 				short o_w, short o_h,
 				unsigned char i_pixfmt,
 				unsigned char o_pixfmt,
 				unsigned int *coeff_h_lum_ptr,
 				unsigned int *coeff_h_ch_ptr,
 				unsigned int *coeff_v_lum_ptr,
 				unsigned int *coeff_v_ch_ptr,
 				unsigned char *scaler_tap_hor,
 				unsigned char *chroma_tap_hor,
 				unsigned char *scaler_tap_ver,
 				unsigned char *chroma_tap_ver,
 				void *temp_buf_ptr,
 				unsigned int temp_buf_size)/* #define ISP_SC_COEFF_TMP_SIZE (21 << 12)*/
 {
 //   short i, j;
 	/* decimition at horizontal */
 //	short d_hor = i_w;
 	short d_hor_bak = i_w;
 	/* decimition at vertical */
 	short d_ver = i_h;
 	short d_ver_bak = i_h;
 	/* interpolation at horizontal */
 //	short i_hor = o_w;
 	short i_hor_bak = o_w;
 	/* interpolation at vertical */
 	short i_ver = o_h;
 	short i_ver_bak = o_h;
 	short d_ver_bak_uv = i_h;
 	short i_ver_bak_uv = o_h;

 	short *cong_y_com_hor = NULL;
 	short *cong_uv_com_hor = NULL;
 	short *cong_y_com_ver = NULL;
 	short *cong_uv_com_ver = NULL;

 	unsigned short luma_ver_tap, chroma_ver_tap;
 	unsigned short luma_ver_maxtap = 8, chroma_ver_maxtap = 8;

 	unsigned int coef_buf_size = 0;
 	short *temp_filter_ptr = NULL;
 	short *filter_ptr = NULL;
 	unsigned int filter_buf_size = GSC_COUNT * sizeof(short);
 	short filter_len[COEF_ARR_ROWS] = { 0 };
 	short coef_len = 0;
 	struct gsc_mem_pool pool = { 0 };

 	if (0 == i_w || 0 == i_h || 0 == o_w || 0 == o_h ||
 		NULL == coeff_h_lum_ptr || NULL == coeff_h_ch_ptr||NULL == coeff_v_lum_ptr ||
 		NULL == coeff_v_ch_ptr || NULL == scaler_tap_hor ||
 		NULL == chroma_tap_hor || NULL == scaler_tap_ver ||
 		NULL == chroma_tap_ver || NULL == temp_buf_ptr) {
 		pr_info("GenScaleCoeff: i_w: %d, i_h: %d, o_w:%d, o_h: %d\n",
 			i_w, i_h, o_w, o_h);
 		pr_info("coef_h:%p, coef_h_chr: %p, coef_v_lum:%p, coef_v_chr: %p\n",
 			coeff_h_lum_ptr, coeff_h_ch_ptr, coeff_v_lum_ptr, coeff_v_ch_ptr);
 		pr_info("y_tap_hor: %p, ch_tap_hor: %p, y_tap_ver: %p, ch_tap_ver: %p, tmp_buf:%p\n",
 			scaler_tap_hor, chroma_tap_hor, scaler_tap_ver, chroma_tap_ver, temp_buf_ptr);
 		return FALSE;
 	}

 	/* init pool and allocate static array */
 	if (!camscaler_init_pool(temp_buf_ptr, temp_buf_size, &pool))
 		return FALSE;

 	coef_buf_size = COEF_ARR_ROWS * COEF_ARR_COL_MAX * sizeof(short);
 	cong_y_com_hor = (short *)camscaler_mem_alloc(coef_buf_size, 2, &pool);
 	cong_uv_com_hor = (short *)camscaler_mem_alloc(coef_buf_size, 2, &pool);
 	cong_y_com_ver = (short *)camscaler_mem_alloc(coef_buf_size, 2, &pool);
 	cong_uv_com_ver =  (short *)camscaler_mem_alloc(coef_buf_size, 2, &pool);

 	if (NULL == cong_y_com_hor || NULL == cong_uv_com_hor ||
 		NULL == cong_y_com_ver || NULL == cong_uv_com_ver)
 		return FALSE;

 	temp_filter_ptr = camscaler_mem_alloc(filter_buf_size, 2, &pool);
 	filter_ptr = camscaler_mem_alloc(filter_buf_size, 2, &pool);
 	if (NULL == temp_filter_ptr || NULL == filter_ptr)
 		return FALSE;

 	/* calculate coefficients of Y component in horizontal direction */
 	coef_len = camscaler_y_scaling_coef_cal(8,
 				d_hor_bak,
 				i_hor_bak,
 				temp_filter_ptr,
 				1,
 				&pool);
 	camscaler_filter_get(temp_filter_ptr, filter_ptr, COEF_ARR_ROWS, coef_len, filter_len);
 	pr_debug("scale y coef hor\n");
 	camscaler_scaler_coef_write(cong_y_com_hor, filter_ptr, COEF_ARR_COLUMNS, 8);
 	camscaler_coef_range_check(cong_y_com_hor, COEF_ARR_ROWS, 8, COEF_ARR_COLUMNS);

 	/* calculate coefficients of UV component in horizontal direction */
 	coef_len = camscaler_uv_scaling_coef_cal(4,
 				d_hor_bak,
 				i_hor_bak,
 				temp_filter_ptr,
 				1,
 				&pool);
 	camscaler_filter_get(temp_filter_ptr, filter_ptr, COEF_ARR_ROWS, coef_len, filter_len);
 	//pr_debug("scale uv coef hor\n");
 	camscaler_scaler_coef_write(cong_uv_com_hor, filter_ptr, COEF_ARR_COLUMNS, 4);
 	camscaler_coef_range_check(cong_uv_com_hor, COEF_ARR_ROWS, 4, COEF_ARR_COLUMNS);
 	/* write the coefficient to register format */
 	camscaler_hor_register_coef_set(coeff_h_lum_ptr, coeff_h_ch_ptr, cong_y_com_hor, cong_uv_com_hor);
	*scaler_tap_hor = 8;
	*chroma_tap_hor = 4;

 	luma_ver_tap = ((unsigned char)(d_ver / i_ver)) * 2;
 	if(i_pixfmt == YUV420)
 		d_ver /= 2;
 	if(i_pixfmt == YUV420 && o_pixfmt == YUV420)
 		i_ver /= 2;
 	chroma_ver_tap = ((unsigned char)(d_ver / i_ver)) * 2;

 	if (luma_ver_tap > luma_ver_maxtap)
 		/* modified by Hongbo, max_tap 8-->16 */
 		luma_ver_tap = luma_ver_maxtap;

 	if (luma_ver_tap <= 2)
 		luma_ver_tap = 4;

 	i_ver_bak_uv = i_ver_bak;

 	*scaler_tap_ver = (unsigned char)luma_ver_tap;

 	/* calculate coefficients of Y component in vertical direction */
 	coef_len = camscaler_y_scaling_coef_cal(luma_ver_tap,
 				d_ver_bak,
 				i_ver_bak,
 				temp_filter_ptr,
 				0,
 				&pool);
 	camscaler_filter_get(temp_filter_ptr, filter_ptr, COEF_ARR_ROWS, coef_len, filter_len);
 	pr_debug("scale y coef ver\n");
 	camscaler_scaler_coef_write(cong_y_com_ver, filter_ptr, COEF_ARR_COL_MAX, filter_len[0]);
 	camscaler_coef_range_check(cong_y_com_ver, COEF_ARR_ROWS, luma_ver_tap, COEF_ARR_COL_MAX);

 	/* calculate coefficients of UV component in vertical direction */
 	if (i_pixfmt == 0/*YUV422*/ && o_pixfmt == 1/*YUV420*/) {
 		//i_ver_bak_uv /= 2;
 		chroma_ver_tap *= 2;
 		chroma_ver_maxtap = 16;
 	}

 	if (chroma_ver_tap > chroma_ver_maxtap)
 		/* modified by Hongbo, max_tap 8-->16 */
 		chroma_ver_tap = chroma_ver_maxtap;

 	if (chroma_ver_tap <= 2)
 		chroma_ver_tap = 4;

 	*chroma_tap_ver = (unsigned char)chroma_ver_tap;

    if(i_pixfmt == YUV420)
 		d_ver_bak_uv /= 2;
 	if(o_pixfmt == YUV420)
 		i_ver_bak_uv /= 2;

 	coef_len = camscaler_uv_scaling_coef_cal((short) (chroma_ver_tap),
 				d_ver_bak_uv,
 				i_ver_bak_uv,
 				temp_filter_ptr,
 				0,
 				&pool);
 	camscaler_filter_get(temp_filter_ptr, filter_ptr, COEF_ARR_ROWS, coef_len, filter_len);
 	pr_debug("scale uv coef ver\n");
 	camscaler_scaler_coef_write(cong_uv_com_ver, filter_ptr, COEF_ARR_COL_MAX, filter_len[0]);
 	camscaler_coef_range_check(cong_uv_com_ver, COEF_ARR_ROWS, chroma_ver_tap, COEF_ARR_COL_MAX);

 	/* calculate edge coefficients of Y component in vertical direction */
 	if (0/*2 * i_ver_bak_y <= d_ver*/) {
 		/* only scale down */
 		//pr_debug("scale y coef ver down\n");
 		camscaler_ver_edge_coef_cal(cong_y_com_ver,
 				d_ver_bak,
 				i_ver_bak,
 				luma_ver_tap,
 				16);
    }

 	/* calculate edge coefficients of UV component in vertical direction */
	if (0/*2 * i_ver_bak_uv <= d_ver*/) {
 		/* only scale down */
 		//pr_debug("scale uv coef ver down\n");
 		camscaler_ver_edge_coef_cal(cong_uv_com_ver,
 				d_ver_bak,
 				i_ver_bak_uv,
 				chroma_ver_tap,
 				16);
    }

 	/* write the coefficient to register format */
 	camscaler_ver_register_coef_set(coeff_v_lum_ptr, coeff_v_ch_ptr,
 			cong_y_com_ver, cong_uv_com_ver, i_ver, d_ver, i_pixfmt, o_pixfmt);

  	return TRUE;
  }

int cam_scaler_coeff_calc_ex(struct yuv_scaler_info *scaler)
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

	 tmp_buf = scaler->coeff_buf;
	 h_coeff = tmp_buf;
	 h_chroma_coeff = tmp_buf + (ISP_SC_COEFF_COEF_SIZE / 4);
	 v_coeff = tmp_buf + (ISP_SC_COEFF_COEF_SIZE * 2 / 4);
	 v_chroma_coeff = tmp_buf + (ISP_SC_COEFF_COEF_SIZE * 3 / 4);

	 pr_debug("factor_in, ver_factor_in, factor_out, ver_factor_out %d %d %d %d\n",
		 (short)scaler->scaler_factor_in, (short)scaler->scaler_ver_factor_in,
		 (short)scaler->scaler_factor_out, (short)scaler->scaler_ver_factor_out);
	 if (!(cam_scaler_isp_scale_coeff_gen_ex((short)scaler->scaler_factor_in,
				 (short)scaler->scaler_ver_factor_in,
				 (short)scaler->scaler_factor_out,
				 (short)scaler->scaler_ver_factor_out,
				 1,/*in format: 00:YUV422; 1:YUV420;*/
				 1,/*out format: 00:YUV422; 1:YUV420;*/
				 h_coeff,
				 h_chroma_coeff,
				 v_coeff,
				 v_chroma_coeff,
				 &y_hor_tap,
				 &uv_hor_tap,
				 &y_ver_tap,
				 &uv_ver_tap,
				 tmp_buf + (ISP_SC_COEFF_COEF_SIZE),
				 ISP_SC_COEFF_TMP_SIZE))) {
		 pr_err("fail to calc scale_coeff_gen_ex !\n");
		 return -EINVAL;
	}

	scaler->scaler_y_hor_tap = y_hor_tap;
	scaler->scaler_uv_hor_tap = uv_hor_tap;
	scaler->scaler_y_ver_tap = y_ver_tap;
	scaler->scaler_uv_ver_tap = uv_ver_tap;

	pr_debug("hor: y %d uv %d; ver: y %d uv %d\n",
		 scaler->scaler_y_hor_tap,
		 scaler->scaler_uv_hor_tap,
		 scaler->scaler_y_ver_tap,
		 scaler->scaler_uv_ver_tap);

	 return 0;
}

