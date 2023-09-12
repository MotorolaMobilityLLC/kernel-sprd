/*
 * Chip depended functions.
 *
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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
#ifndef _VHA_CHIPDEP_H
#define _VHA_CHIPDEP_H

#include <linux/device.h>
#include <linux/regmap.h>
#include <img_mem_man.h>

int vha_chip_init(struct device *dev);
int vha_chip_deinit(struct device *dev);
int vha_chip_runtime_resume(struct device *dev);
int vha_chip_runtime_suspend(struct device *dev);
u64 vha_get_chip_dmamask(void);

#endif /* _VHA_CHIPDEP_H */
