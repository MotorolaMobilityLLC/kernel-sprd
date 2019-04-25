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

#include "cam_common.h"

int sprd_cam_com_timestamp(struct timeval *tv)
{
	struct timespec ts;

	ktime_get_ts(&ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / NSEC_PER_USEC;

	return 0;
}

int sprd_cam_com_raw_pitch_calc(uint16_t isloose, uint16_t width)
{
	uint16_t width_pitch = 0;

	if (!width) {
		pr_err("fail to get valid width!\n");
		return 0;
	}

	if (!isloose) {
		width_pitch = (width * 10 + 127) / 128 * 128  / 8;
		pr_err("fail to width 0x%x, width_pitch 0x%x\n",
			width, width_pitch);

	} else
		width_pitch = (width * 16 + 127) / 128 * 128 / 8;
	return width_pitch;
}
