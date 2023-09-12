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

#include <linux/uaccess.h>
#include <sprd_mm.h>

#include "isp_hw.h"
#include "cam_types.h"
#include "cam_block.h"
#include "isp_core.h"
#include "isp_dewarping.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_DEWARPING: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define AXIS_NONE           (0)
#define AXIS_RMASK          (1 << 0)
#define AXIS_LMASK          (1 << 1)

#define AXIS_SCHEME         (AXIS_LMASK | AXIS_RMASK)
#define XY_OFFSET           ((AXIS_SCHEME == AXIS_RMASK) ? 1 : 0)

#define SINCOS_BIT_31       (1 << 31)
#define SINCOS_BIT_30       (1 << 30)
#define SMULL(x, y)         ((int64_t)(x) * (int64_t)(y))

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

static void Matrix3x3_multiply(const int64_t first[3][3],
		const int64_t second[3][3], int64_t multiply[3][3])
{
	int i = 0, j = 0, k = 0;
	int64_t sum = 0;

	/* row */
	for (i = 0; i < 3; i++)
	{
		/* col */
		for (j = 0; j < 3; j++)
		{
			sum = 0;
			for (k = 0; k < 3; k++)
			{
				sum = sum + ((first[i][k] * second[k][j]
					+ (1 << 15)) >> 30);
			}
			multiply[i][j] = sum;
		}
	}
}

static void get_xy_delta(int64_t x, int64_t y, int64_t xs, int64_t ys, int32_t *dx, int32_t *dy)
{
	int64_t dx_t = 0;
	int64_t dy_t = 0;

	dx_t = (xs - x * ((int64_t)1<<CAMERA_K_PREC)) >> (CAMERA_K_PREC-6);
	dy_t = (ys - y * ((int64_t)1<<CAMERA_K_PREC)) >> (CAMERA_K_PREC-6);

	dx_t = WARP_CLIP2(dx_t, -((int64_t)1 << 31), ((int64_t)1 << 31)-1);
	dy_t = WARP_CLIP2(dy_t, -((int64_t)1 << 31), ((int64_t)1 << 31)-1);

	*dx = (int32_t)dx_t;
	*dy = (int32_t)dy_t;
}

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

static void matrix_mulv(const int64_t A[3][3], const int64_t V[3], int64_t V_out[3])
{
	int i, j;
	for (i = 0; i < 3; i++ )
	{
		V_out[i] = 0;
		for (j = 0; j < 3; j++)
		{
			V_out[i] += (((A[i][j] * V[j]) + (1 << 15)) >> 30);
		}
	}
}

static void computeTiltProjectionMatrix(int32_t tauX, int32_t tauY, int64_t matTilt[3][3])
{
	int64_t cTauX = (int64_t)camscaler_cam_cos_32(tauX);
	int64_t sTauX = (int64_t)camscaler_cam_sin_32(tauX);
	int64_t cTauY = (int64_t)camscaler_cam_cos_32(tauY);
	int64_t sTauY = (int64_t)camscaler_cam_sin_32(tauY);

	int64_t matRotX[3][3] = {{(1 << 30), 0, 0}, {0, cTauX, sTauX}, {0, -sTauX, cTauX}};
	int64_t matRotY[3][3] = {{cTauY, 0, -sTauY}, {0, (1 << 30), 0}, {sTauY , 0, cTauY}};
	int64_t matRotXY[3][3] = {0};
	int64_t matProjZ[3][3] = {0};

	Matrix3x3_multiply(matRotY, matRotX, matRotXY);

	matProjZ[0][0] = matRotXY[2][2];
	matProjZ[0][1] = 0;
	matProjZ[0][2] = -matRotXY[0][2];
	matProjZ[1][0] = 0;
	matProjZ[1][1] = matRotXY[2][2];
	matProjZ[1][2] = -matRotXY[1][2];
	matProjZ[2][0] = 0;
	matProjZ[2][1] = 0;
	matProjZ[2][2] = 1 << 30;

	Matrix3x3_multiply(matProjZ, matRotXY, matTilt);
}

static void get_xy_undistort(int64_t m_k_inv[3][3], int64_t m_k[3][3], int64_t dist_coefs[14],
     uint16_t iw, uint16_t ih, int64_t x, int64_t y, int64_t *xs, int64_t *ys, uint64_t fov_scale)
{
	int64_t _x, _y, _w;
	int64_t xo, yo;
	int64_t x2, y2, r2, _2xy, kr, xd, yd;
	int64_t k1, k2, k3, p1, p2, k4, k5, k6, s1, s2, s3, s4;
	int32_t tauX, tauY;
	int64_t x_t, y_t;
	int64_t matTilt[3][3] = {0};
	int64_t vec3_d[3] = {0};
	int64_t vecTilt[3];
	int64_t invProj;

	k1 = dist_coefs[0];
	k2 = dist_coefs[1];
	p1 = dist_coefs[2];
	p2 = dist_coefs[3];
	k3 = dist_coefs[4];
	k4 = dist_coefs[5];
	k5 = dist_coefs[6];
	k6 = dist_coefs[7];
	s1 = dist_coefs[8];
	s2 = dist_coefs[9];
	s3 = dist_coefs[10];
	s4 = dist_coefs[11];
	tauX = (int32_t)dist_coefs[12];
	tauY = (int32_t)dist_coefs[13];

	x_t = div_s64((x - (int64_t)iw), 2);
	y_t = div_s64((y - (int64_t)ih), 2);

	xo = (x_t * (int64_t)fov_scale);//29bit
	yo = (y_t * (int64_t)fov_scale);
	/* for affine transformation, w always equals to 1 left shift 17bit*/
	_w = (((m_k_inv[2][0] * xo) >> 29) + ((m_k_inv[2][1] * yo) >> 29) + m_k_inv[2][2]);
	_x = div64_s64((((m_k_inv[0][0] * xo) >> 29) + ((m_k_inv[0][1] * yo) >> 29) + m_k_inv[0][2]), _w);
	_y = div64_s64((((m_k_inv[1][0] * xo) >> 29) + ((m_k_inv[1][1] * yo) >> 29) + m_k_inv[1][2]), _w);
	/* 19bit */
	x2 = (_x * _x) >> CAMERA_K_PREC;
	y2 = (_y * _y) >> CAMERA_K_PREC;
	_2xy = (2 * _x * _y) >> CAMERA_K_PREC;
	r2 = x2 + y2;
	/*0.007 error*/
	computeTiltProjectionMatrix(tauX, tauY, matTilt);

	//kr = (1 + ((k3*r2 + k2)*r2 + k1)*r2)/(1 + ((k6*r2 + k5)*r2 + k4)*r2);
	//xd = _x*kr + p1*_2xy + p2*(r2 + 2*x2) + s1*r2+s2*r2*r2;
	//yd = _y*kr + p1*(r2 + 2*y2) + p2*_2xy + s3*r2+s4*r2*r2;
	//kr = ((1<<24) + ((k3*r2/(1<<24) + k2)*r2/(1<<24) + k1)*r2/(1<<24))*(1<<19)/((1<<24) + ((k6*r2/(1<<24) + k5)*r2/(1<<24) + k4)*r2/(1<<24));
	kr = div64_s64(((1 << 19) + ((div_s64(k3 * r2, (1 << CAMERA_K_PREC)) + k2) * div_s64(r2, (1 << CAMERA_K_PREC)) + k1) * div_s64(r2, (1 << CAMERA_K_PREC))) * (1 << 19), ((1 << 19)
		+ div_s64((div_s64((div_s64(k6 * r2, (1 << CAMERA_K_PREC)) + k5) * r2, (1 << CAMERA_K_PREC)) + k4) * r2, (1 << CAMERA_K_PREC))));
	//kr = (((int64)1<<55) + ((k3*r2/(1<<5) + k2*(1<<12))*r2/(1<<5) + k1*(1<<24))*r2/(1<<5))/((1<<19) + ((k6*r2/(1<<17) + k5)*r2/(1<<17) + k4)*r2/(1<<17));
	xd = ((_x * kr) >> 19) + ((p1 * _2xy) >> 19) + ((p2 * (r2 + 2 * x2)) >> 19) + ((s1 * r2) >> 19)
			+ ((s2 * r2 * r2) >> 19 >> CAMERA_K_PREC);
	yd = ((_y * kr) >> 19) + ((p1 * (r2 + 2 * y2)) >> 19) + ((p2 * _2xy) >> 19)
		+ ((s3 * r2) >> 19) + ((s4 * r2 * r2) >> 19 >> CAMERA_K_PREC);

	vec3_d[0] = xd;
	vec3_d[1] = yd;
	vec3_d[2] = 1 << CAMERA_K_PREC;

	matrix_mulv(matTilt, vec3_d, vecTilt);
	invProj = vecTilt[2] ? (div64_s64(((int64_t)1 << (CAMERA_K_PREC + CAMERA_K_PREC)), vecTilt[2]) >
		(1 << CAMERA_K_PREC)) ? (1 << CAMERA_K_PREC) : div_s64(((int64_t)1 << (CAMERA_K_PREC+CAMERA_K_PREC)), vecTilt[2]) : (1 << CAMERA_K_PREC);

	vecTilt[0] = vecTilt[0] >> 8;
	vecTilt[1] = vecTilt[1] >> 8;
	invProj = invProj >> 8;

	*xs = ((((m_k[0][0] * invProj) >> 20) * vecTilt[0]) >> 20) + m_k[0][2];
	*ys = ((((m_k[1][1] * invProj) >> 20) * vecTilt[1]) >> 20) + m_k[1][2];

	*xs += div_s64(iw * ((int64_t)1 << CAMERA_K_PREC), 2);
	*ys += div_s64(ih * ((int64_t)1 << CAMERA_K_PREC), 2);
}

static void calc_grid_data_undistort(struct isp_dewarp_input_info input_info, struct isp_dewarp_calib_info *calib_info, int grid_size,
                    int grid_x, int grid_y, int pos_x, int pos_y, int32_t *grid_data_x, int32_t *grid_data_y)
{
	int64_t r,c,x,y;
	int64_t camera_k_inv[3][3] = {{1,0,0}, {0, 1, 0}, {0, 0, 1}};
	int64_t camera_k[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
	int64_t x_undist, y_undist, xs, ys;
	uint64_t fov_scale;
	uint64_t scale_s_f,scale_f_c,scale_c_f,scale_f_s;

	scale_s_f = div_s64(input_info.crop_width * ((int64_t)1 << SCALE_PREC), input_info.input_width);
	scale_f_c = div64_u64(calib_info->calib_size.calib_width * (1 << SCALE_PREC), calib_info->calib_size.crop_width);
	scale_c_f = div_s64(calib_info->calib_size.crop_width * ((int64_t)1 << SCALE_PREC), calib_info->calib_size.calib_width);
	scale_f_s = div_s64(input_info.input_width * ((int64_t)1 << SCALE_PREC), input_info.crop_width);

	fov_scale = div_s64(((int64_t)1 << 58), (calib_info->calib_size.fov_scale));
	x_undist = y_undist = xs = ys = 0;

	camera_k[0][0] = div64_s64(calib_info->camera_k[0][0], (int64_t)((scale_s_f * scale_f_c) >> SCALE_PREC));
	camera_k[0][2] = div64_s64((calib_info->calib_size.crop_start_x - input_info.crop_start_x) * ((int64_t)1 << SCALE_PREC) * ((int64_t)1 << CAMERA_K_PREC), scale_s_f)
					+ div64_s64(calib_info->camera_k[0][2], (int64_t)((scale_s_f * scale_f_c) >> SCALE_PREC));
	camera_k[1][1] = div_s64(calib_info->camera_k[1][1], ((scale_s_f * scale_f_c) >> SCALE_PREC));
	camera_k[1][2] = div_s64((calib_info->calib_size.crop_start_y - input_info.crop_start_y) * ((int64_t)1 << SCALE_PREC) * ((int64_t)1 << CAMERA_K_PREC), scale_s_f)
					+ div_s64(calib_info->camera_k[1][2], ((scale_s_f * scale_f_c) >> SCALE_PREC));

	camera_k[0][2] = camera_k[0][2] - div_s64(input_info.input_width * ((int64_t)1 << CAMERA_K_PREC), 2);
	camera_k[1][2] = camera_k[1][2] - div_s64(input_info.input_height * ((int64_t)1 << CAMERA_K_PREC), 2);
	/* 19bit */
	camera_k_inv[0][0] = div64_s64(((int64_t)1 << (CAMERA_K_PREC + CAMERA_K_PREC)), camera_k[0][0]);
	camera_k_inv[0][2] = div_s64(-camera_k[0][2] * ((int64_t)1 << CAMERA_K_PREC), camera_k[0][0]);
	camera_k_inv[1][1] = div_s64(((int64_t)1 << (CAMERA_K_PREC + CAMERA_K_PREC)), camera_k[1][1]);
	camera_k_inv[1][2] = div_s64(-camera_k[1][2]*((int64_t)1 << CAMERA_K_PREC), camera_k[1][1]);

	for (r = 0; r < grid_y; r++)
	{
	y = r * grid_size + pos_y - grid_size;
	for (c = 0; c < grid_x; c++)
	{
		x = c * grid_size + pos_x - grid_size;
		get_xy_undistort(camera_k_inv, camera_k, calib_info->dist_coefs, input_info.input_width, input_info.input_height, x, y, &xs, &ys, fov_scale);
		get_xy_delta(x, y, xs, ys, &grid_data_x[r * grid_x + c], &grid_data_y[r * grid_x + c]);
		}
	}
}

static int calc_grid_num(int img_size, int grid_size)
{
	return (img_size + grid_size - 1) / grid_size + 3;
}

static void calc_bicubic_coef(uint32_t *bicubic_coef_i, uint32_t grid_size)
{
	int64_t bicubic_coef[4];
	int64_t bicubic_coef_tmp;
	int64_t u;
	int64_t i, j;

	for (i = 0; i < grid_size; i++)
	{
		u = div_s64((i<<24), grid_size);
		bicubic_coef[0] = ((-u + ((2 * u * u) >> 24) - ((((u * u) >> 24) * u) >> 24)) << 23) >> 24;
		bicubic_coef[1] = ((((int64_t)1 << 25) - ((5 * u * u)>>24) + ((((3 * u * u) >> 24) * u) >> 24)) << 23) >> 24;
		bicubic_coef[2] = ((u + ((4 * u * u) >> 24) - ((((3 * u * u) >> 24) * u) >> 24)) << 23) >> 24;
		bicubic_coef[3] = ((((-u * u) >> 24) + ((((u * u) >> 24) * u) >> 24)) << 23) >> 24;

		for (j = 0; j < 3; j++)
		{
			bicubic_coef_tmp = (bicubic_coef[j] + (1 << 11)) >> 12;
			bicubic_coef_tmp = WARP_CLIP2(bicubic_coef_tmp, -BICUBIC_COEF_MULT, BICUBIC_COEF_MULT);
			bicubic_coef_i[i + j] = (int)bicubic_coef_tmp;
		}
	}
}

static void calc_pixel_bicubic_coef(int *pixel_interp_coef)
{
	int64_t pixel_bicubic_coef[4];
	int64_t pixel_bicubic_coef_tmp;
	int64_t u;
	int64_t i, j;

	for(i = 0; i < LXY_MULT; i++)
	{
		u = (i << 24) / LXY_MULT;
		pixel_bicubic_coef[0] = ((-u + ((2 * u * u) >> 24) - ((((u * u) >> 24) * u) >> 24)) << 23) >> 24;
		pixel_bicubic_coef[1] = ((((int64_t)1 << 25) - ((5 * u * u) >> 24) + ((((3 * u * u) >> 24) * u) >> 24)) << 23) >> 24;
		pixel_bicubic_coef[2] = ((u + ((4 * u * u) >> 24) - ((((3 * u * u) >> 24) * u) >> 24)) << 23) >> 24;
		pixel_bicubic_coef[3] = ((((-u * u) >> 24) + ((((u * u) >> 24) * u) >> 24)) << 23) >> 24;
		for (j = 0; j < 3; j++)
		{
			pixel_bicubic_coef_tmp = (pixel_bicubic_coef[j] + (1 << 13)) >> 14;
			pixel_bicubic_coef_tmp = WARP_CLIP2(pixel_bicubic_coef_tmp, -PIXEL_INTERP_PREC_MULT, PIXEL_INTERP_PREC_MULT);
			pixel_interp_coef[i + j] = (int)pixel_bicubic_coef_tmp;
		}
	}

}

static int ispdewarping_calc_slice_info(int s_col, int e_col, int s_row, int e_row,
	    uint8_t dst_mblk_size, int *crop_info_w, int *crop_info_h,
            int *crop_info_x, int *crop_info_y, uint16_t *mb_row_start,
	    uint16_t *mb_col_start, uint16_t *width, uint16_t *height,
	    uint16_t *init_start_col, uint16_t *init_start_row)
{
	int inter_slice_w_align = dst_mblk_size * 2;

	*crop_info_w = (e_col - s_col + 1);
	*crop_info_h = (e_row - s_row + 1);

	*mb_row_start = s_row / dst_mblk_size;
	*mb_col_start = s_col / dst_mblk_size;

	*crop_info_y = s_row - (*mb_row_start) * dst_mblk_size;
	*crop_info_x = s_col - (*mb_col_start) * dst_mblk_size;

	*width  = *crop_info_w + *crop_info_x;
	*height = *crop_info_h + *crop_info_y;

	*height = (*height + dst_mblk_size - 1) / dst_mblk_size * dst_mblk_size;

	*init_start_col = s_col;
	*init_start_row = s_row;

	*width  = (*width + inter_slice_w_align - 1)
			/ inter_slice_w_align * inter_slice_w_align;

	return 1;
}

static int ispdewarping_slice_calculate(struct slice_pos_info *pos,
			      struct dewarping_slice_out *out)
{
	uint8_t dst_mblk_size = 8;
	ispdewarping_calc_slice_info(pos->start_col, pos->end_col,
	     pos->start_row, pos->end_row, dst_mblk_size,
	     &out->crop_info_w, &out->crop_info_h,
             &out->crop_info_x, &out->crop_info_y, &out->mb_row_start,
	     &out->mb_col_start, &out->width, &out->height,
	     &out->init_start_col, &out->init_start_row);

	return 0;
}

static int ispdewarping_slice_param_get(struct isp_dewarp_ctx_desc *ctx)
{
	struct dewarping_slice_out dewarping_out = {0};
	struct slice_pos_info dewarping_pos =  {0};
	uint32_t slice_num = 0;
	uint32_t slice_id = 0;
	uint8_t dst_mblk_size = 8;
	uint32_t mb_row, mb_col, init_st_col, init_st_row;

	if (!ctx) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	slice_num = ctx->slice_num;
	if (slice_num > 1)
	{
		for (slice_id = 0; slice_id < slice_num; slice_id++) {
			dewarping_pos = ctx->slice_info[slice_id].slice_pos;
			ispdewarping_slice_calculate(&dewarping_pos, &dewarping_out);
			mb_row = ((dewarping_out.crop_info_w + 15) / 16 * 16) / dst_mblk_size;
			mb_col = ((dewarping_out.crop_info_h + 7) / 8 * 8) / dst_mblk_size;
			init_st_col = dewarping_out.init_start_col;
			init_st_row = dewarping_out.init_start_row;
			ctx->slice_info[slice_id].dewarp_slice.start_mb_x = dewarping_out.mb_col_start;
			ctx->slice_info[slice_id].dewarp_slice.start_mb_y = dewarping_out.mb_row_start;
			ctx->slice_info[slice_id].dewarp_slice.mb_x_num = mb_row;
			ctx->slice_info[slice_id].dewarp_slice.mb_y_num = mb_col;
			ctx->slice_info[slice_id].dewarp_slice.init_start_col = init_st_col;
			ctx->slice_info[slice_id].dewarp_slice.init_start_row = init_st_row;
			ctx->slice_info[slice_id].dewarp_slice.slice_width = dewarping_out.crop_info_w;
			ctx->slice_info[slice_id].dewarp_slice.slice_height = dewarping_out.crop_info_h;
			ctx->slice_info[slice_id].dewarp_slice.dst_height = dewarping_out.crop_info_h;
			ctx->slice_info[slice_id].dewarp_slice.dst_width = dewarping_out.crop_info_w;
			ctx->slice_info[slice_id].dewarp_slice.crop_start_x = dewarping_out.crop_info_x;
			ctx->slice_info[slice_id].dewarp_slice.crop_start_y = dewarping_out.crop_info_y;
		}
	}

	   return 0;
}

static int ispdewarping_dewarp_cache_get(struct isp_dewarp_ctx_desc *ctx)
{
	int ret = 0;
	struct isp_dewarp_cache_info *dewarp_fetch = NULL;
	struct img_size *src = NULL;

	if (!ctx) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	dewarp_fetch = &(ctx->dewarp_cache_info);
	dewarp_fetch->dewarp_cache_bypass = 0;
	dewarp_fetch->yuv_format = ctx->in_fmt;
	dewarp_fetch->fetch_path_sel = ISP_FETCH_PATH_DEWARP;
	dewarp_fetch->dewarp_cache_mipi = 0;
	dewarp_fetch->dewarp_cache_endian = 0;
	dewarp_fetch->dewarp_cache_prefetch_len = 3;

	dewarp_fetch->addr.addr_ch0 = ctx->fetch_addr.addr_ch0;
	dewarp_fetch->addr.addr_ch1 = ctx->fetch_addr.addr_ch1;
	src = &(ctx->src_size);
	if (0 == dewarp_fetch->dewarp_cache_mipi) {
		dewarp_fetch->frame_pitch = ((src->w * 2 + 15) / 16) * 16;
		dewarp_fetch->addr.addr_ch1 =
			dewarp_fetch->addr.addr_ch0 + dewarp_fetch->frame_pitch * src->h;
	} else if (1 == dewarp_fetch->dewarp_cache_mipi) {
		dewarp_fetch->frame_pitch = (src->w * 10 + 127) / 128 * 128 / 8;
		dewarp_fetch->addr.addr_ch1 =
			dewarp_fetch->addr.addr_ch0 + dewarp_fetch->frame_pitch * src->h;
	} else {
		pr_err("fail to get support dewarp_cache_mipi 0x%x\n", dewarp_fetch->dewarp_cache_mipi);
	}
	return ret;
}

static uint32_t ispdewarping_grid_size_cal(struct isp_dewarp_ctx_desc *ctx)
{
	uint32_t grid_size = 16;
	uint32_t input_w = 0;

	input_w = ctx->src_size.w;
	if ((input_w > MIN_DEWARP_INPUT_WIDTH) && input_w < 640) {
		grid_size = 16;
	} else if (input_w < MAX_DEWARP_INPUT_WIDTH) {
		grid_size = input_w * MAX_GRID_SIZE_COEF1 / MAX_DEWARP_INPUT_WIDTH;
	} else {
		pr_err("fail to get valid dewarp input size %d\n", input_w);
		return grid_size;
	}

	return grid_size;
}

static int ispdewarping_dewarp_calib_parse(struct isp_dewarp_ctx_desc *ctx, struct isp_dewarp_otp_info *otp_info)
{
	int32_t rtn = 0;
	int32_t i = 0;
	struct isp_dewarp_calib_info *dewarping_calib_info = NULL;

	if (ctx == NULL || otp_info == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	dewarping_calib_info = &ctx->dewarping_calib_info;
	/* get camera_k[3][3] */
	dewarping_calib_info->camera_k[0][0] = (int64_t)otp_info->fx;
	dewarping_calib_info->camera_k[0][1] = (int64_t)otp_info->cx;
	dewarping_calib_info->camera_k[0][2] = (int64_t)0;
	dewarping_calib_info->camera_k[1][0] = (int64_t)0;
	dewarping_calib_info->camera_k[1][1] = (int64_t)otp_info->fy;
	dewarping_calib_info->camera_k[1][2] = (int64_t)otp_info->cy;
	dewarping_calib_info->camera_k[2][0] = (int64_t)0;
	dewarping_calib_info->camera_k[2][1] = (int64_t)0;
	dewarping_calib_info->camera_k[2][2] = (int64_t)1;
	/* get dist_coefs[14] */
	for (i = 0; i < 5; i++)
		dewarping_calib_info->dist_coefs[i] = (int64_t)otp_info->dist_org_coef[i];
	for (i = 0; i < 9; i++)
		dewarping_calib_info->dist_coefs[i + 5] = (int64_t)otp_info->dist_new_coef[i];
	/* get calib size info */
	dewarping_calib_info->calib_size.calib_width = (int64_t)otp_info->calib_width;
	dewarping_calib_info->calib_size.calib_height = (int64_t)otp_info->calib_height;
	dewarping_calib_info->calib_size.crop_start_x = (int64_t)otp_info->crop_start_x;
	dewarping_calib_info->calib_size.crop_start_y = (int64_t)otp_info->crop_start_y;
	dewarping_calib_info->calib_size.crop_height = (int64_t)otp_info->crop_h;
	dewarping_calib_info->calib_size.crop_width = (int64_t)otp_info->crop_w;
	dewarping_calib_info->calib_size.fov_scale = (int64_t)otp_info->fov_scale;

	/* change precision: calib_w/2^19 calib_h/2^19; fov_scale*2^10; cx*2^29l cy*2^29 fx*2^29 fy*2^29 */
	dewarping_calib_info->calib_size.calib_width = dewarping_calib_info->calib_size.calib_width >> 19;
	dewarping_calib_info->calib_size.calib_height = dewarping_calib_info->calib_size.calib_width >> 19;
	dewarping_calib_info->calib_size.fov_scale = dewarping_calib_info->calib_size.fov_scale << 10;
	dewarping_calib_info->camera_k[0][0] = dewarping_calib_info->camera_k[0][0] << 29;
	dewarping_calib_info->camera_k[0][1] = dewarping_calib_info->camera_k[0][1] << 29;
	dewarping_calib_info->camera_k[1][1] = dewarping_calib_info->camera_k[1][1] << 29;
	dewarping_calib_info->camera_k[1][2] = dewarping_calib_info->camera_k[1][2] << 29;

	return rtn;
}

static int ispdewarping_dewarp_config_get(struct isp_dewarp_ctx_desc *ctx)
{
	int32_t rtn = 0;
	struct isp_dewarping_blk_info *dewarping_info = NULL;
	uint32_t grid_size = 0;
	uint32_t pad_width = 0;
	uint32_t pad_height = 0;
	uint32_t grid_x = 0;
	uint32_t grid_y = 0;
	uint32_t grid_data_size = 0;
	struct isp_dewarp_calib_info *dewarping_calib_info  = NULL;
	struct isp_dewarp_input_info dewarp_input_info = {0};

	if (ctx == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	dewarping_info = &(ctx->dewarp_blk_info);
	dewarping_calib_info = &(ctx->dewarping_calib_info);
	dewarping_info->src_width = ctx->src_size.w;
	dewarping_info->src_height = ctx->src_size.h;
	dewarping_info->dst_width = ctx->dst_size.w;
	dewarping_info->dst_height = ctx->dst_size.h;
	grid_size = ispdewarping_grid_size_cal(ctx);
	dewarping_info->grid_size = grid_size;
	dewarping_info->pos_x = (dewarping_info->src_width - dewarping_info->dst_width) >> 1;
	dewarping_info->pos_y = (dewarping_info->src_height - dewarping_info->dst_height) >> 1;
	pad_width  = (dewarping_info->dst_width + DEWARPING_DST_MBLK_SIZE - 1) /
		DEWARPING_DST_MBLK_SIZE * DEWARPING_DST_MBLK_SIZE;
	pad_height = (dewarping_info->dst_height + DEWARPING_DST_MBLK_SIZE - 1) /
		DEWARPING_DST_MBLK_SIZE * DEWARPING_DST_MBLK_SIZE;
	grid_x = calc_grid_num(pad_width, grid_size);
	grid_y = calc_grid_num(pad_height, grid_size);
	grid_data_size = grid_x * grid_y;
	dewarping_info->grid_num_x= grid_x;
	dewarping_info->grid_num_y= grid_y;
	dewarping_info->grid_data_size = grid_data_size;

	dewarping_info->start_mb_x = 0;
	dewarping_info->mb_x_num = ((dewarping_info->src_width + 15) / 16 * 16) / 8;
	dewarping_info->mb_y_num = ((dewarping_info->src_height + 7) / 8 * 8) / 8;
	dewarping_info->dewarping_lbuf_ctrl_nfull_size = 0x4;
	dewarping_info->chk_clr_mode = 0;
	dewarping_info->chk_wrk_mode = 0;
	dewarping_info->init_start_col = 0;
	dewarping_info->init_start_row = 0;
	dewarping_info->crop_start_x = 0;
	dewarping_info->crop_start_y = 0;
	dewarping_info->crop_width = dewarping_info->src_width;
	dewarping_info->crop_height = dewarping_info->src_height;
	/* to be wait algo */
	/* calc ISP_DEWARPING_CORD_COEF_CH0 */
	calc_bicubic_coef(dewarping_info->bicubic_coef_i, grid_size);
	/* calc ISP_DEWARPING_PXL_COEF_CH0 */
	calc_pixel_bicubic_coef(dewarping_info->pixel_interp_coef);

	/* calib param */
	calc_grid_data_undistort(dewarp_input_info, dewarping_calib_info, grid_size, grid_x,
					grid_y, dewarping_info->pos_x, dewarping_info->pos_y,
					dewarping_info->grid_x_ch0_buf, dewarping_info->grid_y_ch0_buf);
	ispdewarping_slice_param_get(ctx);
	return rtn;
}

static int ispdewarping_cfg_param(void *handle,
		enum isp_dewarp_cfg_cmd cmd, void *param)
{
	int ret = 0;
	struct isp_dewarp_ctx_desc *dewarping_ctx = NULL;
	struct isp_dewarp_otp_info *otp_info = NULL;

	if (!handle || !param) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	dewarping_ctx = (struct isp_dewarp_ctx_desc *)handle;
	switch (cmd) {
	case ISP_DEWARPING_CALIB_COEFF:
		otp_info = (struct isp_dewarp_otp_info *)param;
		ispdewarping_dewarp_calib_parse(dewarping_ctx, otp_info);
		break;
	case ISP_DEWARPING_MODE:
		break;
	default:
		pr_err("fail to get known cmd: %d\n", cmd);
		ret = -EFAULT;
		break;
	}
	return ret;
}

static int ispdewarping_proc(void *dewarp_handle, void *param)
{
	int32_t rtn = 0;
	struct isp_dewarp_ctx_desc *dewarping_desc = NULL;
	struct isp_dewarp_in *dewarp_in = NULL;

	if (param == NULL || dewarp_handle == NULL){
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	dewarping_desc = (struct isp_dewarp_ctx_desc *)dewarp_handle;
	dewarp_in = (struct isp_dewarp_in *)param;
	dewarping_desc->dst_size.w = dewarp_in->in_w;
	dewarping_desc->dst_size.h = dewarp_in->in_h;
	dewarping_desc->src_size.w = dewarping_desc->dst_size.w;
	dewarping_desc->src_size.h= dewarping_desc->dst_size.h;
	dewarping_desc->fetch_addr = dewarp_in->addr;
	/* to be add dewarping_calib_info get */
	rtn = ispdewarping_dewarp_config_get(dewarping_desc);
	if (rtn != 0){
		pr_err("fail to gen dewarp\n");
		return -EFAULT;
	}
	rtn = ispdewarping_dewarp_cache_get(dewarping_desc);
	if (rtn != 0){
		pr_err("fail to get dewarp cache\n");
		return -EFAULT;
	}
	return rtn;
}

void *isp_dewarping_ctx_get(uint32_t idx, void *hw)
{
	struct isp_dewarp_ctx_desc *dewarping_desc = NULL;

	dewarping_desc = vzalloc(sizeof(struct isp_dewarp_ctx_desc));
	if (!dewarping_desc)
		return NULL;

	dewarping_desc->idx = idx;
	dewarping_desc->ops.cfg_param = ispdewarping_cfg_param;
	dewarping_desc->ops.pipe_proc = ispdewarping_proc;

	return dewarping_desc;
}

void isp_dewarping_ctx_put(void *dewarp_handle)
{
	struct isp_dewarp_ctx_desc *dewarping_ctx = NULL;

	if (!dewarp_handle) {
		pr_err("fail to get valid dewarp handle\n");
		return;
	}

	dewarping_ctx = (struct isp_dewarp_ctx_desc *)dewarp_handle;

	if (dewarping_ctx)
		vfree(dewarping_ctx);
	dewarping_ctx = NULL;
}
