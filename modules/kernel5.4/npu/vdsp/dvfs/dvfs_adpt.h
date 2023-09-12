/*
 * SPDX-FileCopyrightText: 2022 Unisoc (Shanghai) Technologies Co., Ltd
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#ifndef _VDSP_ADPT_H_
#define _VDSP_ADPT_H_

struct dvfs_adpt_ops {
	void (*enable_adpt)(void *hw_arg);
	void (*disable_adpt)(void *hw_arg);
	uint32_t (*level_to_freq_adpt)(uint32_t level);
	void (*setdvfs_adpt)(uint32_t level);
	void (*strategy_adpt)(uint32_t *level, uint32_t max_level, uint32_t percent, uint32_t last_percent);
};

struct dvfs_adpt_ops *get_dvfs_adpt_ops(void);

#endif

