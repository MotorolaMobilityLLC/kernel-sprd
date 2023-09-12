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

#ifndef _CAM_SCALER_EX_H_
#define _CAM_SCALER_EX_H_

#include "cam_hw.h"

#define COSSIN_Q                                30
#define pi                                      3.14159265359
/* pi * (1 << 32) */
#define PI_32                                   0x3243F6A88UL
#define ARC_32_COEF                             0x80000000
/* convert arc of double type to int32 type */

int cam_scaler_dcam_rds_coeff_gen(
		uint16_t src_width, uint16_t src_height,
		uint16_t dst_width, uint16_t dst_height,
		uint32_t *coeff_buf);

int cam_scaler_coeff_calc_ex(struct yuv_scaler_info *scaler);
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
		unsigned int temp_buf_size);

#endif
