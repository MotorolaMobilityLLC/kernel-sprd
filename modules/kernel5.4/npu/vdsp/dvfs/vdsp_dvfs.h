/*
 * SPDX-FileCopyrightText: 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#ifndef _SPRD_VDSP_DRIVER_DVFS_H_
#define _SPRD_VDSP_DRIVER_DVFS_H_
#include "vdsp_hw.h"

typedef enum {
	POWERHINT_ENABLE = 0,
	POWERHINT_DISABLE,
	POWERHINT_INVALID,
}vdsp_powerhint_flag;

typedef enum {
	VDSP_PWR_DVFS = 0,
	VDSP_PWR_MIN,
	VDSP_PWR_2,
	VDSP_PWR_3,
	VDSP_PWR_4,
	VDSP_PWR_5,
	VDSP_PWR_MAX,
	VDSP_PWR_NUM,	// total power level numbers
}vdsp_power_level;	// power level

struct vdsp_dvfs_info {/*hww check all used*/
	struct mutex dvfs_lock;
	struct mutex timepiece_lock;
	struct mutex powerhint_lock;
	ktime_t starttime;
	ktime_t totaltime;
	ktime_t piece_starttime;

	uint32_t count;
	uint32_t last_powerhint_level;
	uint32_t last_dvfs_level;
	uint32_t dvfs_init;
	wait_queue_head_t wait_q;
	struct task_struct *dvfs_thread;
};

struct vdsp_powerhint {
	uint32_t record[VDSP_PWR_NUM];
};

struct vdsp_dvfs_ops {
	int (*init)(void *data);
	void (*deinit)(void *data);
	int32_t (*set_powerhint)(void *data, int32_t level, uint32_t flag);
	void (*preprocess)(void *data);
	void (*postprocess)(void *data);
	void (*release_powerhint)(void *filp);
	void (*setdvfs)(void *hw_arg, uint32_t level);
	void (*enable)(void *hw_arg);
	void (*disable)(void *hw_arg);
};

struct vdsp_dvfs_desc {
	struct vdsp_dvfs_ops *ops;
};

struct vdsp_dvfs_desc *get_vdsp_dvfs_desc(void);

#endif

