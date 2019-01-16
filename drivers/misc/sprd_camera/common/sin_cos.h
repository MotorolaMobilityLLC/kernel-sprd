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
#ifndef _SIN_COS_H_
#define _SIN_COS_H_

#include <linux/types.h>

/* Macro Definitions */

#define COSSIN_Q				30
#define pi					3.14159265359
/* pi * (1 << 32) */
#define PI_32					0x3243F6A88
#define ARC_32_COEF				0x80000000
/* convert arc of double type to int32 type */

int dcam_sin_32(int n);
int dcam_cos_32(int n);

#endif /* _SIN_COS_H_ */
