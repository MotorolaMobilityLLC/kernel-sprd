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

#include <linux/types.h>
#include "dvfs_phy.h"
#include "dvfs_adpt.h"

static void enable_adpt(void *hw_arg)
{
	struct dvfs_phy_desc *phy = get_dvfs_phy_desc();
	phy->ops->enable(hw_arg);
}

static void disable_adpt(void *hw_arg)
{
	struct dvfs_phy_desc *phy = get_dvfs_phy_desc();
	phy->ops->disable(hw_arg);
}

static uint32_t level_to_freq_adpt(uint32_t level)
{
	struct dvfs_phy_desc *phy = get_dvfs_phy_desc();
	return phy->ops->level_to_freq(level);
}

static void setdvfs_adpt(uint32_t level)
{
	struct dvfs_phy_desc *phy = get_dvfs_phy_desc();
	phy->ops->setdvfs(level);
}

static void strategy_adpt(uint32_t *level, uint32_t max_level,
	uint32_t percent, uint32_t last_percent)
{
	struct dvfs_phy_desc *phy = get_dvfs_phy_desc();
	phy->ops->strategy(level, max_level, percent, last_percent);
}

static struct dvfs_adpt_ops dvfs_adpt = {
	.enable_adpt = enable_adpt,
	.disable_adpt = disable_adpt,
	.level_to_freq_adpt = level_to_freq_adpt,
	.setdvfs_adpt = setdvfs_adpt,
	.strategy_adpt = strategy_adpt,
};

struct dvfs_adpt_ops *get_dvfs_adpt_ops(void)
{
	return &dvfs_adpt;
}


