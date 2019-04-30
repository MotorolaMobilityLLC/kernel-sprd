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
	uint16_t width_align_16 = (width + 16 - 1) & (~(16 - 1));

	if (!width_align_16 || !width) {
		pr_err("fail to get valid width!\n");
		return 0;
	}

	if (!isloose) {
		uint32_t fetchraw_pitch = 0;
		uint32_t mod16_pixel = width & 0xF;
		uint32_t mod16_bytes = (mod16_pixel + 3) / 4 * 5;
		uint32_t mod16_words = (mod16_bytes + 3) / 4;

		fetchraw_pitch = width / 16 * 20 + mod16_words * 4;

		pr_debug("width 0x%x, pitch 0x%x\n", width, fetchraw_pitch);
		return fetchraw_pitch;
	} else {
		return (width * 2);
	}
}
