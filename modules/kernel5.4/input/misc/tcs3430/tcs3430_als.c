/*
*****************************************************************************
* Copyright by ams AG                                                       *
* All rights are reserved.                                                  *
*                                                                           *
* IMPORTANT - PLEASE READ CAREFULLY BEFORE COPYING, INSTALLING OR USING     *
* THE SOFTWARE.                                                             *
*                                                                           *
* THIS SOFTWARE IS PROVIDED FOR USE ONLY IN CONJUNCTION WITH AMS PRODUCTS.  *
* USE OF THE SOFTWARE IN CONJUNCTION WITH NON-AMS-PRODUCTS IS EXPLICITLY    *
* EXCLUDED.                                                                 *
*                                                                           *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         *
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS         *
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT  *
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,     *
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT          *
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     *
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY     *
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT       *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE     *
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.      *
*****************************************************************************
*/
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include "ams_tcs3430.h"
#include "tcs3430_regs.h"
#include "libfixmath/fix16.h"

#define Q16_16_SCALEF 65535
#define Q16_16_SCALEF_BITS 16

int32_t const MC_N_CONST_Q16_16 = 21757;          /* 0.3320 */
int32_t const MC_D_CONST_Q16_16 = 12176;          /* 0.1858 */
int32_t const MC_1ST_COEF_Q16_16 = 29425664;      /* 449 */
int32_t const MC_2ND_COEF_Q16_16 = 231014400;     /* 3525 */
int32_t const MC_3RD_COEF_Q16_16 = 447171776;     /* 6823.3 */
int32_t const MC_4TH_CONST_Q16_16 = 361780352;    /* 5520.33 */
int32_t const NORMALIZE_COEFF_Q16_16 = 105696464; /* 16 * 100.8 */
int32_t const IR1_COEF_Q16_16 = 8519;             /* 0.13 */
int32_t const IR2_COEF_Q16_16 = 2621;             /* 0.04 */
int32_t const xgain_q16_16 = 65536;               /* Use 1 for now */
int32_t const zgain_q16_16 = 65536;               /* Use 1 for now */
int32_t const ugain_q16_16 = 65536;               /* Use 1 for now */

/* Lux Fixed Point Calibration Matrix */
int64_t lux_cal_fixed_q16_16[4] = {21782, 79737, -3116, -13824};

/* XYZ Fixed Point Calibration Matrix */
fix16_t xyz_cal_fixed_Q16_16[3][10] = {
	{ -175968, -539802, 56952, 48157, 802465, -330518, 48926, 304654, -80744, -7423},
	{ -421538, -561725, 29025, 22780, 1076864, -209260, 120506, 197764, -155715, 29637},
	{ 62184, 12445, 412759, 26014, -71242, 126745, -463753, -222147, 567051, -230388}
};


static void tcs3430_als_zero(struct tcs3430_chip *chip)
{
	memset(&chip->als_q16, 0, sizeof(struct tcs3430_als_q16));
	chip->als_inf.lux = 0;
	chip->als_inf.cct = 0;
}

int32_t q16_16_pow(int32_t value_q16_16, int32_t power)
{
	int32_t q16_16_result;
	int i;
	fix16_t result;

	q16_16_result = value_q16_16;
	for (i = 0; i < (power - 1); i++) {
		result = fix16_mul(q16_16_result, value_q16_16);
		if (result == fix16_overflow)
			printk(KERN_INFO "\n\nlibfixmath OVERFLOW!\n");

		q16_16_result = result;
	}
	return q16_16_result;
}

void tcs3430_calc_normalized_fixed(struct tcs3430_chip *chip)
{
	int32_t ir1tmp_q16_16;
	int32_t ir2tmp_q16_16;
	int32_t ircombined_q16_16;
	int32_t itime_q16_16;
	int32_t itime;
	int32_t again;
	int32_t again_q16_16;
	int32_t xg_ug_q16_16;
	int32_t zg_ug_q16_16;
	int32_t tmp_q16_16;
	int32_t denominator_q16_16;
	int32_t div_result_q16_16;

	tcs3430_als_zero(chip);

	itime = TCS3430_ATIME_TO_ITIME_FIXED(chip->shadow[TCS3430_REG_ATIME]);
	again = als_gains[chip->shadow[TCS3430_REG_CFG1] &
			  TCS3430_ALS_GAIN_MASK];

	itime_q16_16 = fix16_from_int(itime);
	again_q16_16 = fix16_from_int(again);

	chip->als_q16.x_raw_q16_16 = fix16_from_int(chip->als_inf.x_raw);
	chip->als_q16.y_raw_q16_16 = fix16_from_int(chip->als_inf.y_raw);
	chip->als_q16.z_raw_q16_16 = fix16_from_int(chip->als_inf.z_raw);
	chip->als_q16.ir1_raw_q16_16 = fix16_from_int(chip->als_inf.ir1_raw);
	chip->als_q16.ir2_raw_q16_16 = fix16_from_int(chip->als_inf.ir2_raw);

	/* Normalize for Analog Gain and Integration Time */

	/* X'  = 16 * 101 * (x_raw / (xgain * ugain * again * itime)) */
	xg_ug_q16_16 = fix16_mul(xgain_q16_16, ugain_q16_16);
	tmp_q16_16 = fix16_mul(again_q16_16, itime_q16_16);
	denominator_q16_16 = fix16_mul(xg_ug_q16_16, tmp_q16_16);
	chip->als_q16.xp1_q16_16 = NORMALIZE_COEFF_Q16_16;
	div_result_q16_16 = fix16_div(chip->als_q16.x_raw_q16_16,
				      denominator_q16_16);
	chip->als_q16.xp1_q16_16 = fix16_mul(chip->als_q16.xp1_q16_16,
					     div_result_q16_16);

	/* Y'  = 16 * 101 * (y_raw / (ugain * again * itime)); */
	denominator_q16_16 = fix16_mul(ugain_q16_16, tmp_q16_16);
	chip->als_q16.yp1_q16_16 = NORMALIZE_COEFF_Q16_16;
	div_result_q16_16 = fix16_div(chip->als_q16.y_raw_q16_16,
				      denominator_q16_16);
	chip->als_q16.yp1_q16_16 = fix16_mul(chip->als_q16.yp1_q16_16,
					     div_result_q16_16);

	/* Z'  = 16 * 101 * (z_raw / (zgain * ugain * again * itime)); */
	zg_ug_q16_16 = fix16_mul(zgain_q16_16, ugain_q16_16);
	denominator_q16_16 = fix16_mul(zg_ug_q16_16, tmp_q16_16);
	chip->als_q16.zp1_q16_16 = NORMALIZE_COEFF_Q16_16;
	div_result_q16_16 = fix16_div(chip->als_q16.z_raw_q16_16,
				      denominator_q16_16);
	chip->als_q16.zp1_q16_16 = fix16_mul(chip->als_q16.zp1_q16_16,
					     div_result_q16_16);

	/* IR' = 16 * 101 * (((ir1_raw * IR1_coef_fixed) +
	   (ir2_raw * IR2_coef_fixed))/ (ugain * again * itime)); */
	denominator_q16_16 = fix16_mul(ugain_q16_16, tmp_q16_16);
	chip->als_q16.irp1_q16_16 = NORMALIZE_COEFF_Q16_16;
	ir1tmp_q16_16 = fix16_mul(chip->als_q16.ir1_raw_q16_16,
				  IR1_COEF_Q16_16);
	ir2tmp_q16_16 = fix16_mul(chip->als_q16.ir2_raw_q16_16,
				  IR2_COEF_Q16_16);
	ircombined_q16_16 = fix16_add(ir1tmp_q16_16, ir2tmp_q16_16);
	div_result_q16_16 = fix16_div(ircombined_q16_16,
				      denominator_q16_16);
	chip->als_q16.irp1_q16_16 = fix16_mul(chip->als_q16.irp1_q16_16,
					      div_result_q16_16);
}

int tcs3430_calc_lux_fixed(struct tcs3430_chip *chip)
{
	int i;
	int lux = 0;
	fix16_t luxtmp_q16_16 = 0;
	fix16_t normalized_matrix[4];

	/* Calculate normalized data */
	tcs3430_calc_normalized_fixed(chip);

	normalized_matrix[0] = chip->als_q16.xp1_q16_16;
	normalized_matrix[1] = chip->als_q16.yp1_q16_16;
	normalized_matrix[2] = chip->als_q16.zp1_q16_16;
	normalized_matrix[3] = chip->als_q16.irp1_q16_16;

	for (i = 0; i < 4; i++)	{
		luxtmp_q16_16 += fix16_mul(normalized_matrix[i],
					   lux_cal_fixed_q16_16[i]);
	}
	lux = luxtmp_q16_16 >> Q16_16_SCALEF_BITS;

	return lux;
}

int tcs3430_calc_cct_fixed(struct tcs3430_chip *chip)
{
	int32_t cct;
	int32_t cct_q16_16;
	int64_t xs_q16_16;         /* X' squared */
	int64_t ys_q16_16;         /* Y' squared */
	int64_t zs_q16_16;         /* Z' squared */
	int64_t irs_q16_16;        /* IR' squared */
	int64_t sum_q16_16;
	int64_t dtmp;
	int32_t i;
	int32_t j;
	int64_t lr_terms_q16_16[10];
	int64_t xyz_q16_16[3] = {0};
	int32_t sum_sqrt_q16_16 = 0;
	int32_t n_q16_16;
	int32_t product_q16_16;
	int32_t n_numerator_q16_16;
	int32_t n_denominator_q16_16;
	int32_t n3_q16_16;
	int32_t n2_q16_16;
	int32_t mc_1st_term_q16_16;
	int32_t mc_2nd_term_q16_16;
	int32_t mc_3rd_term_q16_16;
	int32_t mc_4th_term_q16_16;

	/*----------------------------------------------------------------
	 * Calculate Linear Regression Terms
	 *----------------------------------------------------------------*/

	/* Calculate squares of the normalized values */
	xs_q16_16 = fix16_mul(chip->als_q16.xp1_q16_16,
			      chip->als_q16.xp1_q16_16);
	ys_q16_16 = fix16_mul(chip->als_q16.yp1_q16_16,
			      chip->als_q16.yp1_q16_16);
	zs_q16_16 = fix16_mul(chip->als_q16.zp1_q16_16,
			      chip->als_q16.zp1_q16_16);
	irs_q16_16 = fix16_mul(chip->als_q16.irp1_q16_16,
			       chip->als_q16.irp1_q16_16);

	/* Sum the squares */
	sum_q16_16 = xs_q16_16 + ys_q16_16 + zs_q16_16 + irs_q16_16;

	/* Take the square root of the sum of squares */
	sum_sqrt_q16_16 = fix16_sqrt(sum_q16_16);

	/* X'n */
	chip->als_q16.xp1n_q16_16 = fix16_div(chip->als_q16.xp1_q16_16,
					      sum_sqrt_q16_16);
	lr_terms_q16_16[0] = chip->als_q16.xp1n_q16_16;


	/* Y'n */
	chip->als_q16.yp1n_q16_16 = fix16_div(chip->als_q16.yp1_q16_16,
					      sum_sqrt_q16_16);
	lr_terms_q16_16[1] = chip->als_q16.yp1n_q16_16;


	/* Z'n */
	chip->als_q16.zp1n_q16_16 = fix16_div(chip->als_q16.zp1_q16_16,
					      sum_sqrt_q16_16);
	lr_terms_q16_16[2] = chip->als_q16.zp1n_q16_16;


	/* IR'n */
	chip->als_q16.irp1n_q16_16 = fix16_div(chip->als_q16.irp1_q16_16,
					       sum_sqrt_q16_16);
	lr_terms_q16_16[3] = chip->als_q16.irp1n_q16_16;

	/* XY */
	product_q16_16 = fix16_mul(chip->als_q16.xp1n_q16_16,
				   chip->als_q16.yp1n_q16_16);
	chip->als_q16.xy_q16_16 = fix16_sqrt(product_q16_16);
	lr_terms_q16_16[4] = chip->als_q16.xy_q16_16;

	/* XZ */
	product_q16_16 = fix16_mul(chip->als_q16.xp1n_q16_16,
				   chip->als_q16.zp1n_q16_16);
	chip->als_q16.xz_q16_16 = fix16_sqrt(product_q16_16);
	lr_terms_q16_16[5] = chip->als_q16.xz_q16_16;

	/* XIR */
	product_q16_16 = fix16_mul(chip->als_q16.xp1n_q16_16,
				   chip->als_q16.irp1n_q16_16);
	chip->als_q16.xir_q16_16 = fix16_sqrt(product_q16_16);
	lr_terms_q16_16[6] = chip->als_q16.xir_q16_16;

	/* YZ */
	product_q16_16 = fix16_mul(chip->als_q16.yp1n_q16_16,
				   chip->als_q16.zp1n_q16_16);
	chip->als_q16.yz_q16_16 = fix16_sqrt(product_q16_16);
	lr_terms_q16_16[7] = chip->als_q16.yz_q16_16;

	/* YIR */
	product_q16_16 = fix16_mul(chip->als_q16.yp1n_q16_16,
				   chip->als_q16.irp1n_q16_16);
	chip->als_q16.yir_q16_16 = fix16_sqrt(product_q16_16);
	lr_terms_q16_16[8] = chip->als_q16.yir_q16_16;

	/* ZIR */
	product_q16_16 = fix16_mul(chip->als_q16.zp1n_q16_16,
				   chip->als_q16.irp1n_q16_16);
	chip->als_q16.zir_q16_16 = fix16_sqrt(product_q16_16);
	lr_terms_q16_16[9] = chip->als_q16.zir_q16_16;

	/*-----------------------------------------------------------------
	 * Calculate XYZ from 3x10 Calibration Matrix
	 *-----------------------------------------------------------------*/
	for (i = 0; i < 3; i++)	{
		for (j = 0; j < 10; j++) {
			dtmp = fix16_mul(xyz_cal_fixed_Q16_16[i][j],
					 lr_terms_q16_16[j]);
			xyz_q16_16[i] += dtmp;
		}
	}

	/* X'' */
	chip->als_q16.xp2_q16_16 = xyz_q16_16[0];

	/* Y'' */
	chip->als_q16.yp2_q16_16 = xyz_q16_16[1];

	/* Z'' */
	chip->als_q16.zp2_q16_16 = xyz_q16_16[2];

	/*----------------------------------------------------------------
	 * Calculate Chromaticity Coordinates x, y
	 *----------------------------------------------------------------*/
	sum_q16_16 = 0;
	sum_q16_16 = fix16_add(sum_q16_16, chip->als_q16.xp2_q16_16);
	sum_q16_16 = fix16_add(sum_q16_16, chip->als_q16.yp2_q16_16);
	sum_q16_16 = fix16_add(sum_q16_16, chip->als_q16.zp2_q16_16);

	chip->als_q16.x_q16_16 = fix16_div(chip->als_q16.xp2_q16_16,
					   sum_q16_16);
	chip->als_q16.y_q16_16 = fix16_div(chip->als_q16.yp2_q16_16,
					   sum_q16_16);

	/*----------------------------------------------------------------
	 * McCamy's Formula
	 *----------------------------------------------------------------*/
	n_numerator_q16_16 = fix16_sub(chip->als_q16.x_q16_16,
				       MC_N_CONST_Q16_16);
	n_denominator_q16_16 = fix16_sub(MC_D_CONST_Q16_16,
					 chip->als_q16.y_q16_16);

	n_q16_16 = fix16_div(n_numerator_q16_16, n_denominator_q16_16);

	n3_q16_16 = q16_16_pow(n_q16_16, 3);
	n2_q16_16 = q16_16_pow(n_q16_16, 2);
	mc_1st_term_q16_16 = fix16_mul(MC_1ST_COEF_Q16_16, n3_q16_16);
	mc_2nd_term_q16_16 = fix16_mul(MC_2ND_COEF_Q16_16, n2_q16_16);
	mc_3rd_term_q16_16 = fix16_mul(MC_3RD_COEF_Q16_16, n_q16_16);
	mc_4th_term_q16_16 = MC_4TH_CONST_Q16_16;

	/* cct = (449 * pow(n,3)) + (3525 * pow(n,2)) + (6823.3 * n) + 5520; */
	cct_q16_16 = mc_1st_term_q16_16 +
		mc_2nd_term_q16_16 +
		mc_3rd_term_q16_16 +
		mc_4th_term_q16_16;

	cct = fix16_to_int(cct_q16_16);

	return cct;
}
