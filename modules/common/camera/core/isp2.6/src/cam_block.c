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

#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/completion.h>

#include "cam_types.h"

static uint32_t camblock_noisefilter_24b_shift8(uint32_t seed,
	uint32_t *data_out)
{
	uint32_t bit_0 = 0, bit_1 = 0;
	uint32_t bit_2 = 0, bit_3 = 0;
	uint32_t bit_in[8] = {0}, bit_in8b = 0;
	uint32_t out = 0;
	uint32_t i = 0;

	for (i = 0; i < 8; i++) {
		bit_0 = (seed >> (0 + i)) & 0x1;
		bit_1 = (seed >> (1 + i)) & 0x1;
		bit_2 = (seed >> (2 + i)) & 0x1;
		bit_3 = (seed >> (7 + i)) & 0x1;
		bit_in[i] = bit_0 ^ bit_1 ^ bit_2 ^ bit_3;
	}
	bit_in8b = (bit_in[7] << 7) | (bit_in[6] << 6) | (bit_in[5] << 5) |
		(bit_in[4] << 4) | (bit_in[3] << 3) | (bit_in[2] << 2) |
		(bit_in[1] << 1) | bit_in[0];

	out = seed & 0xffffff;
	out = out | (bit_in8b << 24);
	if (data_out)
		*data_out = out;

	out = out >> 8;

	return out;
}

void cam_block_noisefilter_seeds(uint32_t image_width,
	uint32_t seed0, uint32_t *seed1, uint32_t *seed2, uint32_t *seed3)
{
	uint32_t i = 0;

	*seed1 = camblock_noisefilter_24b_shift8(seed0, NULL);
	*seed2 = seed0;

	for (i = 0; i < image_width; i++)
		*seed2 = camblock_noisefilter_24b_shift8(*seed2, NULL);

	*seed3 = camblock_noisefilter_24b_shift8(*seed2, NULL);
}

