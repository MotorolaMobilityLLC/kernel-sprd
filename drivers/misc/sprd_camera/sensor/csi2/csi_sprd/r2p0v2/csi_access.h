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

#ifndef _CSI_ACCESS_H_
#define _CSI_ACCESS_H_

#include <linux/types.h>
#include "sprd_sensor_core.h"

#define ADDR_COUNT SPRD_SENSOR_ID_MAX

int access_init(uint32_t *base_addr, int32_t idx);
uint32_t access_read(uint16_t addr, int32_t idx);
void access_write(uint32_t data, uint16_t addr, int32_t idx);

#endif
