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

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/soc/sprd/hwfeature.h>
#include "dvfs_phy.h"
#include "sprd_dvfs_vdsp.h"
#include "vdsp_debugfs.h"
#include "vdsp_hw.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: dvfs-phy %d: %d %s:" \
	fmt, current->pid, __LINE__, __func__

#define vmin(a, b) ((a) < (b) ? (a) :(b))
#define vmax(a, b) ((a) > (b) ? (a) :(b))

#define 	VDSP_CLK256M		256000000	//SHARKL5PRO_VDSP_CLK256M
#define 	VDSP_CLK384M		384000000	//SHARKL5PRO_VDSP_CLK384M
#define 	VDSP_CLK512M		512000000	//SHARKL5PRO_VDSP_CLK512M
#define 	VDSP_CLK614M4		614400000	//SHARKL5PRO_VDSP_CLK614M4
#define 	VDSP_CLK768M		768000000	//SHARKL5PRO_VDSP_CLK768M
#define 	VDSP_CLK936M		936000000	//SHARKL5PRO_VDSP_CLK936M

static void enable_phy(void *hw_arg)
{
	(void)hw_arg;
}

static void disable_phy(void *hw_arg)
{
	(void)hw_arg;
}

static uint32_t level_to_freq(uint32_t level)
{
	switch (level) {
	case 1:	//VDSP_PWR_MIN
		return VDSP_CLK256M;
	case 2:
		return VDSP_CLK384M;
	case 3:
		return VDSP_CLK512M;
	case 4:
		return VDSP_CLK614M4;
	case 5:
		return VDSP_CLK768M;
	case 6:	//VDSP_PWR_MAX
		return VDSP_CLK936M;
	default:
		return VDSP_CLK256M;
	}
}

#define CHIP_T610	0
#define CHIP_T618	1
static uint32_t soc_ver_id_check(void)
{
	uint32_t chip_id = CHIP_T610;
	const char *chip_name;
	struct device_node *node;

	node = of_find_node_by_path("/hwfeature/auto");
	if (!node) {
		pr_err("no efuse, default chip is T610\n");
		goto out;
	}

	chip_name = of_get_property(node, "efuse", NULL);
	if (!strcmp(chip_name, "T618")) {
		chip_id = CHIP_T618;
	} else {
		chip_id = CHIP_T610;
	}

out:
	pr_debug("chip name :%s, chip_id:%d\n", chip_name, chip_id);
	return chip_id;
}

static void strategy(uint32_t *level, uint32_t max_level,
	uint32_t percent, uint32_t last_percent)
{
	if ((last_percent > 50)) {
		if (percent > 50)
			*level = max_level;
		else if ((percent <= 50) && (percent > 20))
			*level = max_level - 2;
		else
			*level = max_level - 3;
	} else if ((last_percent <= 50) && (last_percent > 20)) {
		if (percent > 50)
			*level = max_level;
		else if ((percent <= 50) && (percent > 20))
			*level = max_level - 2;
		else
			*level = max_level - 3;
	} else {
		if (percent > 50)
			*level = max_level;
		else if ((percent <= 50) && (percent > 20))
			*level = max_level - 3;
		else
			*level = max_level - 3;
	}
}

static void setdvfs_hw(uint32_t level)
{
	uint32_t freq;
	unsigned int debugfs_dvfs_level;

	/* chip check, t610 is max 768M support */
	if (soc_ver_id_check() == CHIP_T610)
		level = vmin(level, 5);	// 5: 768M

	/* debugfs for dvfs debug */
	debugfs_dvfs_level = vdsp_debugfs_dvfs_level();
	if (debugfs_dvfs_level > 0)
	{
		level = debugfs_dvfs_level;
		pr_info("debugfs force dvfs, level:%d, freq:%d\n", level, level_to_freq(level));
	}

	freq = level_to_freq(level);
	pr_info("set vdsp dvfs, level:%d, freq:%d\n", level, freq);

	vdsp_dvfs_notifier_call_chain(&freq);
}

static struct dvfs_phy_ops vdsp_dvfs_ops = {
	.enable = enable_phy,
	.disable = disable_phy,
	.level_to_freq = level_to_freq,
	.setdvfs = setdvfs_hw,
	.strategy = strategy,
};

static struct dvfs_phy_desc sub_dvfs_phy_desc = {
	.ops = &vdsp_dvfs_ops,
};

struct dvfs_phy_desc *get_dvfs_phy_desc(void)
{
	return &sub_dvfs_phy_desc;
}

