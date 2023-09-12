/*
* SPDX-FileCopyrightText: 2020-2022 Unisoc (Shanghai) Technologies Co., Ltd
* SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
*
* Copyright 2020-2022 Unisoc (Shanghai) Technologies Co., Ltd.
* Licensed under the Unisoc General Software License, version 1.0 (the License);
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
* Software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* See the Unisoc General Software License, version 1.0 for more details.
*/

#ifndef _VDSP_DRIVER_DVFS_PHY_H_
#define _VDSP_DRIVER_DVFS_PHY_H_

#define MM_SYS_EN		(0x0)

/*MM_SYS_EN		(0x0)*/
#define DVFS_EN		BIT(3) //GENMASK(23, 16)

struct dvfs_phy_ops {
	void (*enable)(void *hw_arg);
	void (*disable)(void *hw_arg);
	uint32_t (*level_to_freq)(uint32_t level);
	void (*setdvfs)(uint32_t level);
	void (*strategy)(uint32_t *level, uint32_t max_level,
		uint32_t percent, uint32_t last_percent);
};

struct dvfs_phy_desc {
	struct dvfs_phy_ops *ops;
};

struct dvfs_phy_desc *get_dvfs_phy_desc(void);



#endif


