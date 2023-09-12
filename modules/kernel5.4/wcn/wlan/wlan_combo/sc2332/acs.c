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

#include <linux/platform_device.h>
#include "common/acs.h"
#include "common/cfg80211.h"
#include "common/iface.h"

int sc2332_dump_survey(struct wiphy *wiphy, struct net_device *ndev,
		       int idx, struct survey_info *s_info)
{
	struct sprd_vif *vif = netdev_priv(ndev);
	struct survey_info_node *info;
	u16 band;
	unsigned int freq;
	static int survey_count;
	int err = 0;

	if (!list_empty(&vif->survey_info_list)) {
		mutex_lock(&vif->survey_lock);
		info = list_first_entry(&vif->survey_info_list,
					struct survey_info_node, survey_list);
		list_del(&info->survey_list);
		mutex_unlock(&vif->survey_lock);
		if (info->channel_num) {
			band = sprd_channel_to_band(info->channel_num);
			freq =
			    ieee80211_channel_to_frequency(info->channel_num, band);
			s_info->channel = ieee80211_get_channel(wiphy, freq);
			s_info->noise = -80;
			s_info->time = info->duration;
			s_info->time_busy = info->busy;
			s_info->filled = (SURVEY_INFO_NOISE_DBM |
					  SURVEY_INFO_TIME |
					  SURVEY_INFO_TIME_BUSY);
			survey_count++;
		}
		kfree(info);
	} else {
		/* There are no more survey info in list */
		err = -ENOENT;
		netdev_info(vif->ndev, "%s report %d surveys\n",
			    __func__, survey_count);
		survey_count = 0;
	}
	return err;
}
