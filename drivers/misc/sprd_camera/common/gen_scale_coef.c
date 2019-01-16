/*
 * Copyright (C) 2015-2016 Spreadtrum Communications Inc.
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
#include "gen_scale_coef.h"
#include "sin_cos.h"

/* Macro Definitions */

#define GSC_FIX			24
#define GSC_COUNT		256
#define GSC_ABS(_a)		((_a) < 0 ? -(_a) : (_a))
#define GSC_SIGN2(input, p) \
	{ if (p >= 0) input = 1; if (p < 0) input = -1; }
#define COEF_ARR_ROWS		9
#define COEF_ARR_COLUMNS	8
#define COEF_ARR_COL_MAX	16
#define MIN_POOL_SIZE		(6 * 1024)
#define TRUE			1
#define FALSE			0
#define SCI_MEMSET		memset
#define MAX(_x, _y)		(((_x) > (_y)) ? (_x) : (_y))

/* Structure Definitions */

struct gsc_mem_pool {
	unsigned long begin_addr;
	unsigned int total_size;
	unsigned int used_size;
};

/* Internal Function Implementation */

static unsigned char init_pool(void *buffer_ptr, unsigned int buffer_size,
			       struct gsc_mem_pool *pool_ptr)
{
	if (NULL == buffer_ptr || 0 == buffer_size || NULL == pool_ptr)
		return FALSE;

	if (buffer_size < MIN_POOL_SIZE)
		return FALSE;

	pool_ptr->begin_addr = (unsigned long) buffer_ptr;
	pool_ptr->total_size = buffer_size;
	pool_ptr->used_size = 0;

	return TRUE;
}

static void *alloc_mem(unsigned int size, unsigned int align_shift,
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

static int64_t div64_s64_s64(int64_t dividend, int64_t divisor)
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

static void normalize_inter(int64_t *data, short *int_data, unsigned char ilen)
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
			tmp_d = div64_s64_s64(tmp_data[it] * (int64_t)256,
					      tmp_sum_val);
			int_data[it] = (unsigned short)tmp_d;
		}
	}
}

static short sum_fun(short *data, signed char ilen)
{
	signed char i;
	short tmp_sum;

	tmp_sum = 0;

	for (i = 0; i < ilen; i++)
		tmp_sum += *data++;

	return tmp_sum;
}

static void adjust_filter_inter(short *filter, unsigned char ilen)
{
	int i, midi;
	int tmpi, tmp_S;
	int tmp_val = 0;

	tmpi = sum_fun(filter, ilen) - 256;
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

static short cal_y_model_coef(short coef_length, short *coef_data_ptr,
			      short n, short m,
			      struct gsc_mem_pool *pool_ptr)
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

	filter = alloc_mem(GSC_COUNT * sizeof(int64_t), 3, pool_ptr);
	tmp_filter = alloc_mem(GSC_COUNT * sizeof(int64_t), 3, pool_ptr);
	normal_filter = alloc_mem(GSC_COUNT * sizeof(short), 2, pool_ptr);

	if (NULL == filter || NULL == tmp_filter || NULL == normal_filter)
		return 1;

	mid_i = coef_length >> 1;
	filter[mid_i] = div64_s64_s64((int64_t)((int64_t)n << GSC_FIX),
				      (int64_t)MAX(m, n));
	for (i = 0; i < mid_i; i++) {
		dividend = (int64_t)
			((int64_t)ARC_32_COEF * (int64_t)(i + 1) * (int64_t)n);
		divisor = (int64_t)((int64_t)MAX(m, n) * (int64_t)8);
		angle_x = div64_s64_s64(dividend, divisor);

		dividend = (int64_t)
			((int64_t)ARC_32_COEF * (int64_t)(i + 1) * (int64_t)n);
		divisor = (int64_t)((int64_t)(m * n) * (int64_t)8);
		angle_y = div64_s64_s64(dividend, divisor);

		value_x = dcam_sin_32((int)angle_x);
		value_y = dcam_sin_32((int)angle_y);

		dividend = (int64_t)
			((int64_t)value_x * (int64_t)(1 << GSC_FIX));
		divisor = (int64_t)((int64_t)m * (int64_t)value_y);
		filter[mid_i + i + 1] = div64_s64_s64(dividend, divisor);
		filter[mid_i - (i + 1)] = filter[mid_i + i + 1];
	}

	for (i = -1; i < mid_i; i++) {
		dividend = (int64_t)((int64_t)2 * (int64_t)(mid_i - i - 1) *
				     (int64_t)ARC_32_COEF);
		divisor = (int64_t)coef_length;
		angle_32 = (int)div64_s64_s64(dividend, divisor);

		a = (int64_t)9059697;
		b = (int64_t)7717519;

		t = a - ((b * dcam_cos_32(angle_32)) >> 30);

		filter[mid_i + i + 1] = (t * filter[mid_i + i + 1]) >> GSC_FIX;
		filter[mid_i - (i + 1)] = filter[mid_i + i + 1];
	}

	for (i = 0; i < 8; i++) {
		mount = 0;
		for (j = i; j < coef_length; j += 8) {
			tmp_filter[mount] = filter[j];
			mount++;
		}
		normalize_inter(tmp_filter, normal_filter, (signed char) mount);
		sum_val = sum_fun(normal_filter, mount);
		if (sum_val != 256)
			adjust_filter_inter(normal_filter, mount);

		mount = 0;
		for (kk = i; kk < coef_length; kk += 8) {
			coef_data_ptr[kk] = normal_filter[mount];
			mount++;
		}
	}

	return 0;
}

static short cal_y_scaling_coef(short tap, short d, short i,
				short *y_coef_data_ptr, short dir,
				struct gsc_mem_pool *pool_ptr)
{
	unsigned short coef_length;

	coef_length = (unsigned short) (tap * 8);
	SCI_MEMSET(y_coef_data_ptr, 0, coef_length * sizeof(short));
	cal_y_model_coef(coef_length, y_coef_data_ptr, i, d, pool_ptr);

	return coef_length;
}

static short cal_uv_scaling_coef(short tap, short d, short i,
				 short *uv_coef_data_ptr, short dir,
				 struct gsc_mem_pool *pool_ptr)
{
	short uv_coef_length;

	if (dir == 1) {
		uv_coef_length = (short)(tap * 8);
		cal_y_model_coef(uv_coef_length,
				 uv_coef_data_ptr,
				 i,
				 d,
				 pool_ptr);
	} else {
		if (d > i)
			uv_coef_length = (short)(tap * 8);
		else
			uv_coef_length = (short)(2 * 8);

		cal_y_model_coef(uv_coef_length,
				 uv_coef_data_ptr,
				 i,
				 d,
				 pool_ptr);
	}

	return uv_coef_length;
}

static void get_filter(short *coef_data_ptr, short *out_filter,
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

static void write_scaler_coef(short *dst_coef_ptr, short *coef_ptr,
			      short dst_pitch, short src_pitch)
{
	int i, j;

	for (i = 0; i < 8; i++) {
		for (j = 0; j < src_pitch; j++)
			*(dst_coef_ptr + j) =
				*(coef_ptr + i * src_pitch + src_pitch - 1 - j);

		dst_coef_ptr += dst_pitch;
	}
}

static void set_hor_register_coef(unsigned int *reg_coef_ptr, short *y_coef_ptr,
				  short *uv_coef_ptr)
{
	int i = 0;
	short *y_coef_arr[COEF_ARR_ROWS] = { NULL };
	short *uv_coef_arr[COEF_ARR_ROWS] = { NULL };

	for (i = 0; i < COEF_ARR_ROWS; i++) {
		y_coef_arr[i] = y_coef_ptr;
		uv_coef_arr[i] = uv_coef_ptr;
		y_coef_ptr += COEF_ARR_COLUMNS;
		uv_coef_ptr += COEF_ARR_COLUMNS;
	}

	/* horizontal Y Scaling Coef Config register */
	for (i = 0; i < 8; i++) {
		unsigned short p0, p1;
		unsigned int reg;

		p0 = (unsigned short) y_coef_arr[i][7];
		p1 = (unsigned short) y_coef_arr[i][6];
		reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 9);
		*reg_coef_ptr++ = reg;
		p0 = (unsigned short) y_coef_arr[i][5];
		p1 = (unsigned short) y_coef_arr[i][4];
		reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 9);
		*reg_coef_ptr++ = reg;
		p0 = (unsigned short) y_coef_arr[i][3];
		p1 = (unsigned short) y_coef_arr[i][2];
		reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 9);
		*reg_coef_ptr++ = reg;
		p0 = (unsigned short) y_coef_arr[i][1];
		p1 = (unsigned short) y_coef_arr[i][0];
		reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 9);
		*reg_coef_ptr++ = reg;
	}

	/* horizontal UV Scaling Coef Config register */
	for (i = 0; i < 8; i++) {
		unsigned short p0, p1;
		unsigned int reg;

		p0 = (unsigned short) uv_coef_arr[i][3];
		p1 = (unsigned short) uv_coef_arr[i][2];
		reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 9);
		*reg_coef_ptr++ = reg;
		p0 = (unsigned short) uv_coef_arr[i][1];
		p1 = (unsigned short) uv_coef_arr[i][0];
		reg = ((p0 & 0x1ff)) | ((p1 & 0x1ff) << 9);
		*reg_coef_ptr++ = reg;
	}
}

static void set_ver_register_coef(unsigned int *reg_coef_lum_ptr,
				  unsigned int *reg_coef_ch_ptr,
				  short *y_coef_ptr,
				  short *uv_coef_ptr,
				  short i_h,
				  short o_h,
				  unsigned char is_scaling2yuv420)
{
	unsigned int cnts = 0;
	unsigned int i = 0, j = 0;

	if (2 * o_h <= i_h) {
		for (i = 0; i < 9; i++) {
			for (j = 0; j < 16; j++) {
				reg_coef_lum_ptr[cnts++] =
					*(y_coef_ptr + i * 16 + j);
				if (cnts == SCALER_COEF_TAB_LEN_VER)
					break;
			}
		}

		cnts = 0;
		for (i = 0; i < 9; i++) {
			for (j = 0; j < 16; j++) {
				reg_coef_ch_ptr[cnts++] =
					*(uv_coef_ptr + i * 16 + j);
				if (cnts == SCALER_COEF_TAB_LEN_VER)
					break;
			}
		}
	} else {
		for (i = 0; i < 8; i++) {
			for (j = 0; j < 4; j++)
				reg_coef_lum_ptr[cnts++] =
					*(y_coef_ptr + i * 16 + j);
		}

		cnts = 0;
		if ((o_h <= i_h) && is_scaling2yuv420) {
			for (i = 0; i < 9; i++) {
				for (j = 0; j < 16; j++) {
					reg_coef_ch_ptr[cnts++] =
						*(uv_coef_ptr + i * 16 + j);
					if (cnts == SCALER_COEF_TAB_LEN_VER)
						break;
				}
			}
		} else {
			for (i = 0; i < 8; i++) {
				for (j = 0; j < 4; j++)
					reg_coef_ch_ptr[cnts++] =
						*(uv_coef_ptr + i * 16 + j);
			}
		}
	}
}

static void check_coef_range(short *coef_ptr, short rows, short columns,
			     short pitch)
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
	pr_debug("check_coef_range\n");
	for (i = 0; i < rows; i++) {
		for (j = 0; j < columns; j++)
			pr_debug("%d ", coef_arr[i][j]);
		pr_debug("\n");
	}
}

static void cal_ver_edge_coef(short *coeff_ptr, short d, short i,
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
/*          temp_buf_ptr: temp buffer size, 6k is the suggest size          */
/* Return:                                                                  */
/* Note:                                                                    */
/****************************************************************************/
unsigned char dcam_gen_scale_coeff(short i_w,
				   short i_h,
				   short o_w,
				   short o_h,
				   unsigned int *coeff_h_ptr,
				   unsigned int *coeff_v_lum_ptr,
				   unsigned int *coeff_v_ch_ptr,
				   unsigned char scaling2yuv420,
				   unsigned char *scaler_tap,
				   unsigned char *chrome_tap,
				   void *temp_buf_ptr,
				   unsigned int temp_buf_size)
{
	/* decimition at horizontal */
	short d_hor = i_w;
	/* decimition at vertical */
	short d_ver = i_h;
	/* interpolation at horizontal */
	short i_hor = o_w;
	/* interpolation at vertical */
	short i_ver = o_h;
	short i_ver_bak_uv = o_h;
	short i_ver_bak_y = o_h;
	short *cong_y_com_hor = 0;
	short *cong_uv_com_hor = 0;
	short *cong_y_com_ver = 0;
	short *cong_uv_com_ver = 0;

	unsigned short luma_ver_tap, chrome_ver_tap;
	unsigned short luma_ver_maxtap = 16, chrome_ver_maxtap = 16;

	unsigned int coef_buf_size = 0;
	short *temp_filter_ptr = NULL;
	short *filter_ptr = NULL;
	unsigned int filter_buf_size = GSC_COUNT * sizeof(short);
	short filter_len[COEF_ARR_ROWS] = { 0 };
	short coef_len = 0;
	struct gsc_mem_pool pool = { 0 };

	if (0 == i_w || 0 == i_h || 0 == o_w || 0 == o_h ||
	    NULL == coeff_h_ptr || NULL == coeff_v_lum_ptr ||
	    NULL == coeff_v_ch_ptr || NULL == scaler_tap ||
	    NULL == chrome_tap || NULL == temp_buf_ptr) {
		pr_info("GenScaleCoeff: i_w: %d, i_h: %d, o_w:%d, o_h: %d\n",
			i_w, i_h, o_w, o_h);
		pr_info("coef_h:%p, coef_v_lum:%p, coef_v_chr: %p\n",
			coeff_h_ptr, coeff_v_lum_ptr, coeff_v_ch_ptr);
		pr_info("y_tap: %p, ch_tap: %p, tmp_buf:%p\n",
			scaler_tap, chrome_tap, temp_buf_ptr);
		return FALSE;
	}

	/* init pool and allocate static array */
	if (!init_pool(temp_buf_ptr, temp_buf_size, &pool))
		return FALSE;

	coef_buf_size = COEF_ARR_ROWS * COEF_ARR_COL_MAX * sizeof(short);
	cong_y_com_hor = (short *)alloc_mem(coef_buf_size, 2, &pool);
	cong_uv_com_hor = (short *)alloc_mem(coef_buf_size, 2, &pool);
	cong_y_com_ver = (short *)alloc_mem(coef_buf_size, 2, &pool);
	cong_uv_com_ver =  (short *)alloc_mem(coef_buf_size, 2, &pool);

	if (NULL == cong_y_com_hor || NULL == cong_uv_com_hor ||
	    NULL == cong_y_com_ver || NULL == cong_uv_com_ver)
		return FALSE;

	temp_filter_ptr = alloc_mem(filter_buf_size, 2, &pool);
	filter_ptr = alloc_mem(filter_buf_size, 2, &pool);
	if (NULL == temp_filter_ptr || NULL == filter_ptr)
		return FALSE;

	/* calculate coefficients of Y component in horizontal direction */
	coef_len = cal_y_scaling_coef(8,
				      d_hor,
				      i_hor,
				      temp_filter_ptr,
				      1,
				      &pool);
	get_filter(temp_filter_ptr, filter_ptr, 8, coef_len, filter_len);
	pr_debug("scale y coef hor\n");
	write_scaler_coef(cong_y_com_hor, filter_ptr, 8, 8);
	check_coef_range(cong_y_com_hor, 8, 8, 8);

	/* calculate coefficients of UV component in horizontal direction */
	coef_len = cal_uv_scaling_coef(4,
				       d_hor,
				       i_hor,
				       temp_filter_ptr,
				       1,
				       &pool);
	get_filter(temp_filter_ptr, filter_ptr, 8, coef_len, filter_len);
	pr_debug("scale uv coef hor\n");
	write_scaler_coef(cong_uv_com_hor, filter_ptr, 8, 4);
	check_coef_range(cong_uv_com_hor, 8, 4, 8);
	/* write the coefficient to register format */
	set_hor_register_coef(coeff_h_ptr, cong_y_com_hor, cong_uv_com_hor);

	luma_ver_tap = ((unsigned char)(d_ver / i_ver)) * 2;
	chrome_ver_tap = luma_ver_tap;

	if (luma_ver_tap > luma_ver_maxtap)
		/* modified by Hongbo, max_tap 8-->16 */
		luma_ver_tap = luma_ver_maxtap;

	if (luma_ver_tap <= 2)
		luma_ver_tap = 4;

	*scaler_tap = (unsigned char)luma_ver_tap;

	/* calculate coefficients of Y component in vertical direction */
	coef_len = cal_y_scaling_coef(luma_ver_tap,
				      d_ver,
				      i_ver,
				      temp_filter_ptr,
				      0,
				      &pool);
	get_filter(temp_filter_ptr, filter_ptr, 8, coef_len, filter_len);
	pr_debug("scale y coef ver\n");
	write_scaler_coef(cong_y_com_ver, filter_ptr, 16, filter_len[0]);
	check_coef_range(cong_y_com_ver, 8, luma_ver_tap, 16);

	/* calculate coefficients of UV component in vertical direction */
	if (scaling2yuv420) {
		i_ver_bak_uv /= 2;
		chrome_ver_tap *= 2;
		chrome_ver_maxtap = 16;
	}

	if (chrome_ver_tap > chrome_ver_maxtap)
		/* modified by Hongbo, max_tap 8-->16 */
		chrome_ver_tap = chrome_ver_maxtap;

	if (chrome_ver_tap <= 2)
		chrome_ver_tap = 4;

	*chrome_tap = (unsigned char)chrome_ver_tap;

	coef_len = cal_uv_scaling_coef((short) (chrome_ver_tap),
				       d_ver,
				       i_ver_bak_uv,
				       temp_filter_ptr,
				       0,
				       &pool);
	get_filter(temp_filter_ptr, filter_ptr, 8, coef_len, filter_len);
	pr_debug("scale uv coef ver\n");
	write_scaler_coef(cong_uv_com_ver, filter_ptr, 16, filter_len[0]);
	check_coef_range(cong_uv_com_ver, 8, chrome_ver_tap, 16);

	/* calculate edge coefficients of Y component in vertical direction */
	if (2 * i_ver_bak_y <= d_ver)
		/* only scale down */
		pr_debug("scale y coef ver down\n");
		cal_ver_edge_coef(cong_y_com_ver,
				  d_ver,
				  i_ver_bak_y,
				  luma_ver_tap,
				  16);

	/* calculate edge coefficients of UV component in vertical direction */
	if (2 * i_ver_bak_uv <= d_ver)
		/* only scale down */
		pr_debug("scale uv coef ver down\n");
		cal_ver_edge_coef(cong_uv_com_ver,
				  d_ver,
				  i_ver_bak_uv,
				  chrome_ver_tap,
				  16);

	/* write the coefficient to register format */
	set_ver_register_coef(coeff_v_lum_ptr, coeff_v_ch_ptr,
			      cong_y_com_ver, cong_uv_com_ver, d_ver,
			      i_ver, scaling2yuv420);

	return TRUE;
}
