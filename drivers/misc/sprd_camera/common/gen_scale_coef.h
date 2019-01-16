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
#ifndef _GEN_SCALE_COEF_H_
#define _GEN_SCALE_COEF_H_

#define SCALER_COEF_TAB_LEN_HOR			48
#define SCALER_COEF_TAB_LEN_VER			132

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
				   unsigned int temp_buf_size);
#endif
