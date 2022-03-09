// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright © 2021 Unisoc Technologies Co., Ltd.
 * <https://www.unisoc.com>
 */

#ifndef __FCC_H__
#define __FCC_H__

#define MAC_FCC_COUNTRY_NUM	6
#define MAX_POWER_BACKOFF_RULE	20

struct sprd_priv;

struct sprd_power_backoff {
	u8 sub_type;
	s8 value;
	u8 mode;
	u8 channel;
} __packed;

struct fcc_power_bo {
	char country[3];
	u8 num;
	struct sprd_power_backoff power_backoff[MAX_POWER_BACKOFF_RULE];
} __packed;

void sc2332_fcc_match_country(struct sprd_priv *priv, const char *alpha2);
#endif

