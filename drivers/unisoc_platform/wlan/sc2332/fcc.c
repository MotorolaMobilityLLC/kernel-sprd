/*
* SPDX-FileCopyrightText: 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
* SPDX-License-Identifier: GPL-2.0
*
* Copyright 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of version 2 of the GNU General Public License
* as published by the Free Software Foundation.
*/

#include "common/common.h"
#include "cmdevt.h"
#include "fcc.h"

static struct fcc_power_bo g_fcc_power_table[MAC_FCC_COUNTRY_NUM] = {
	{
		.country = "UY",
		.num = 4,
		.power_backoff = {
			/* subtype, value, mode, channel */
			{1, 8, 1, 11},
			{1, 7, 1, 1},
			{1, 6, 2, 11},
			{1, 5, 2, 1},
		},
	},
	{
		.country = "MX",
		.num = 4,
		.power_backoff = {
			/* subtype, value, mode, channel */
			{1, 9, 1, 11},
			{1, 8, 1, 1},
			{1, 7, 2, 11},
			{1, 6, 2, 1},
		},
	},
};

void sc2332_fcc_match_country(struct sprd_priv *priv, const char *alpha2)
{
	int index, i;
	bool found_fcc = false;

	for (i = 0; i < MAC_FCC_COUNTRY_NUM; i++) {
		if (g_fcc_power_table[i].country[0] == alpha2[0] &&
			g_fcc_power_table[i].country[1] == alpha2[1]) {
			found_fcc = true;
			pr_info("need set fcc power\n");
			for (index = 0; index < g_fcc_power_table[i].num; index++) {
				sc2332_set_power_backoff(priv, NULL,
					g_fcc_power_table[i].power_backoff[index].sub_type,
					g_fcc_power_table[i].power_backoff[index].value,
					g_fcc_power_table[i].power_backoff[index].mode,
					g_fcc_power_table[i].power_backoff[index].channel);
			}
			break;
		}
	}

	if (!found_fcc) {
		pr_info("not fcc country,need reset fcc power\n");
		sc2332_set_power_backoff(priv, NULL, 0, 0, 0, 1);
	}
}
