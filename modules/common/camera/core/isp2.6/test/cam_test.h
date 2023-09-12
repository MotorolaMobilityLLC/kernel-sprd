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

#ifndef _CAM_TEST_H_
#define _CAM_TEST_H_

#include "sprd_cam_test.h"
#include "cam_hw.h"

int camt_test(struct cam_hw_info *hw, unsigned long arg);

int dcamt_init(struct cam_hw_info *hw, struct camt_info *info);
int dcamt_start(struct camt_info *info);
int dcamt_stop(void);
int dcamt_deinit(void);

int ispt_init(struct cam_hw_info *hw, struct camt_info *info);
int ispt_start(struct camt_info *info);
int ispt_stop(void);
int ispt_deinit(void);

void read_image_from_file(unsigned char *buffer,
	unsigned int size, const char *file);
void camt_write_image_to_file(unsigned char *buffer,
	unsigned int size, const char *file);

#endif

