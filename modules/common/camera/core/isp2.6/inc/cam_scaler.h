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

#ifndef _CAM_SCALER_H_
#define _CAM_SCALER_H_

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

int cam_scaler_coeff_calc(struct yuv_scaler_info *scaler, uint32_t scale2yuv420);

#endif
