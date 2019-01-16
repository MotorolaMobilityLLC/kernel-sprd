/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include "csi_access.h"

static unsigned int *access_base_addr[ADDR_COUNT];

int access_init(unsigned int *base_addr, int idx)
{
	access_base_addr[idx] = base_addr;
	return 0;
}

unsigned int access_read(unsigned short addr, int idx)
{
	return access_base_addr[idx][addr];
}

void access_write(unsigned int data, unsigned short addr, int idx)
{
	access_base_addr[idx][addr] = data;
}
