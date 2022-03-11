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

#include "cmdevt.h"
#include "common/cfg80211.h"
#include "common/chip_ops.h"
#include "common/common.h"
#include "scan.h"

static void scan_cancel(struct sprd_vif *vif)
{
	struct sprd_priv *priv = vif->priv;

	if (priv->scan_vif && priv->scan_vif == vif) {
		if (timer_pending(&priv->scan_timer))
			del_timer_sync(&priv->scan_timer);

		spin_lock_bh(&priv->scan_lock);

		if (priv->scan_request) {
			priv->scan_request = NULL;
			priv->scan_vif = NULL;
		}
		spin_unlock_bh(&priv->scan_lock);
	}
}

static void sched_scan_cancel(struct sprd_vif *vif)
{
	struct sprd_priv *priv = vif->priv;

	if (priv->sched_scan_vif && priv->sched_scan_vif == vif) {
		spin_lock_bh(&priv->sched_scan_lock);
		if (priv->sched_scan_request) {
			priv->sched_scan_request = NULL;
			priv->sched_scan_vif = NULL;
		}
		spin_unlock_bh(&priv->sched_scan_lock);
	}
}

void sc2332_report_scan_result(struct sprd_vif *vif, u16 chan, s16 rssi,
			       u8 *frame, u16 len)
{
	struct sprd_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)frame;
	struct ieee80211_channel *channel;
	struct cfg80211_bss *bss;
	struct timespec64 ts;
	u16 band, capability, beacon_interval;
	u32 freq;
	s32 signal;
	u64 tsf;
	u8 *ie;
	size_t ielen;

	if (!priv->scan_request && !priv->sched_scan_request) {
		netdev_err(vif->ndev, "%s Unexpected event\n", __func__);
		return;
	}

	band = sprd_channel_to_band(chan);
	freq = ieee80211_channel_to_frequency(chan, band);
	channel = ieee80211_get_channel(wiphy, freq);
	if (!channel) {
		netdev_err(vif->ndev, "%s invalid freq!\n", __func__);
		return;
	}

	if (!mgmt) {
		netdev_err(vif->ndev, "%s NULL frame!\n", __func__);
		return;
	}

	signal = rssi * 100;

	ie = mgmt->u.probe_resp.variable;
	if (IS_ERR_OR_NULL(ie)) {
		netdev_err(vif->ndev, "%s Invalid IE in beacon!\n", __func__);
		return;
	}
	ielen = len - offsetof(struct ieee80211_mgmt, u.probe_resp.variable);
	if (ielen > SPRD_SCAN_RESULT_MAX_IE_LEN) {
		netdev_err(vif->ndev, "%s Invalid IE length!\n", __func__);
		return;
	}
	/* framework use system bootup time */
	ktime_get_boottime_ts64(&ts);
	tsf = (u64)ts.tv_sec * 1000000 + div_u64(ts.tv_nsec, 1000);
	beacon_interval = le16_to_cpu(mgmt->u.probe_resp.beacon_int);
	capability = le16_to_cpu(mgmt->u.probe_resp.capab_info);

	netdev_dbg(vif->ndev, "%s, %pM, channel %2u, rssi %d\n",
		   ieee80211_is_probe_resp(mgmt->frame_control)
		   ? "proberesp" : "beacon", mgmt->bssid, chan, rssi);

	bss = cfg80211_inform_bss(wiphy, channel, CFG80211_BSS_FTYPE_UNKNOWN,
				  mgmt->bssid, tsf, capability, beacon_interval,
				  ie, ielen, signal, GFP_KERNEL);

	if (unlikely(!bss))
		netdev_err(vif->ndev,
			   "%s failed to inform bss frame!\n", __func__);
	cfg80211_put_bss(wiphy, bss);

	if (vif->beacon_loss) {
		bss = cfg80211_get_bss(wiphy, NULL, vif->bssid,
				       vif->ssid, vif->ssid_len,
				       IEEE80211_BSS_TYPE_ESS,
				       IEEE80211_PRIVACY_ANY);
		if (bss) {
			cfg80211_unlink_bss(wiphy, bss);
			netdev_info(vif->ndev,
				    "unlink %pM due to beacon loss\n",
				    bss->bssid);
			vif->beacon_loss = 0;
		}
	}
}

void sc2332_scan_timeout(struct timer_list *t)
{
	struct sprd_priv *priv = from_timer(priv, t, scan_timer);
	struct cfg80211_scan_info info;

	pr_info("%s\n", __func__);

	spin_lock_bh(&priv->scan_lock);
	if (priv->scan_request) {
		info.aborted = true;
		cfg80211_scan_done(priv->scan_request, &info);
		priv->scan_vif = NULL;
		priv->scan_request = NULL;
	}
	spin_unlock_bh(&priv->scan_lock);
}

int sc2332_scan(struct wiphy *wiphy,
		struct cfg80211_scan_request *request)
{
	struct sprd_priv *priv = wiphy_priv(wiphy);
	struct sprd_vif *vif =
	    container_of(request->wdev, struct sprd_vif, wdev);
	struct cfg80211_ssid *ssids = request->ssids;
	struct sprd_scan_ssid *scan_ssids;
	u8 *ssids_ptr = NULL;
	const u32 ssids_bufsize = 512;
	int ssids_len = 0;
	u32 channels = 0;
	unsigned int i, n;
	int ret;
	u8 mac_addr[ETH_ALEN] = { 0x0 };

	netdev_info(vif->ndev, "%s n_channels %u\n", __func__,
		    request->n_channels);
	if (priv->scan_request)
		netdev_err(vif->ndev, "%s error scan %p running [%p, %p]\n",
			   __func__, priv->scan_request, priv->scan_vif, vif);

	/* set WPS ie */
	if (request->ie_len > 0) {
		if (request->ie_len > SPRD_MAX_SCAN_REQ_IE_LEN) {
			netdev_err(vif->ndev, "%s invalid len: %zu\n", __func__,
				   request->ie_len);
			ret = -EOPNOTSUPP;
			goto err;
		}

		ret = sprd_set_probereq_ie(priv, vif, request->ie,
					   request->ie_len);
		if (ret)
			goto err;
	}
	if ((request->flags & NL80211_SCAN_FLAG_RANDOM_ADDR) &&
	    (vif->mode == SPRD_MODE_STATION && priv->rand_mac_flag == 0)) {
			get_random_mask_addr(mac_addr, request->mac_addr,
				     request->mac_addr_mask);
			netdev_info(vif->ndev, "%s random mac address is %pM\n", __func__, mac_addr);
		}
	for (i = 0; i < request->n_channels; i++)
		channels |= (1 << (request->channels[i]->hw_value - 1));

	n = min(request->n_ssids, SPRD_TOTAL_SSID_NR);
	if (n) {
		ssids_ptr = kzalloc(ssids_bufsize, GFP_KERNEL);
		if (!ssids_ptr) {
			ret = -ENOMEM;
			goto err;
		}

		scan_ssids = (struct sprd_scan_ssid *)ssids_ptr;
		for (i = 0; i < n; i++) {
			if (!ssids[i].ssid_len)
				continue;
			scan_ssids->len = ssids[i].ssid_len;
			strncpy(scan_ssids->ssid, ssids[i].ssid,
				ssids[i].ssid_len);
			ssids_len += (ssids[i].ssid_len
				      + sizeof(scan_ssids->len));
			scan_ssids = (struct sprd_scan_ssid *)
			    (ssids_ptr + ssids_len);
		}
	} else {
		netdev_info(vif->ndev, "%s n_ssids is 0\n", __func__);
	}

	spin_lock_bh(&priv->scan_lock);
	priv->scan_request = request;
	priv->scan_vif = vif;
	spin_unlock_bh(&priv->scan_lock);
	mod_timer(&priv->scan_timer,
		  jiffies + SPRD_SCAN_TIMEOUT_MS * HZ / 1000);

	ret = sc2332_cmd_scan(vif->priv, vif, channels, ssids_len, ssids_ptr,
			      mac_addr, request->flags);
	kfree(ssids_ptr);
	if (ret) {
		scan_cancel(vif);
		goto err;
	}

	return 0;
err:
	netdev_err(vif->ndev, "%s failed (%d)\n", __func__, ret);
	return ret;
}

int sc2332_sched_scan_start(struct wiphy *wiphy, struct net_device *ndev,
			    struct cfg80211_sched_scan_request *request)
{
	struct sprd_priv *priv = wiphy_priv(wiphy);
	struct cfg80211_sched_scan_plan *scan_plans = NULL;
	struct sprd_sched_scan *sscan_buf = NULL;
	struct sprd_vif *vif = NULL;
	struct cfg80211_ssid *ssid_tmp = NULL;
	struct cfg80211_match_set *match_ssid_tmp = NULL;
	int ret = 0;
	int i = 0, j = 0;
	int ch = request->channels[i]->hw_value;

	if (!ndev) {
		netdev_err(ndev, "%s NULL ndev\n", __func__);
		return ret;
	}
	vif = netdev_priv(ndev);

	if (vif->priv->sched_scan_request) {
		netdev_err(ndev, "%s  schedule scan is running\n", __func__);
		return 0;
	}
	sscan_buf = kzalloc(sizeof(*sscan_buf), GFP_KERNEL);
	if (!sscan_buf)
		return -ENOMEM;

	scan_plans = request->scan_plans;
	sscan_buf->interval = scan_plans->interval * 1000;
	sscan_buf->flags = request->flags;

	if (request->min_rssi_thold <= NL80211_SCAN_RSSI_THOLD_OFF)
		sscan_buf->rssi_thold = 0;
	else if (request->min_rssi_thold < SPRD_MIN_RSSI_THOLD)
		sscan_buf->rssi_thold = SPRD_MIN_RSSI_THOLD;
	else
		sscan_buf->rssi_thold = request->min_rssi_thold;

	for (i = 0, j = 0; i < request->n_channels; i++) {
		if (ch == 0) {
			netdev_info(ndev, "%s  unknown frequency %dMhz\n",
				    __func__,
				    request->channels[i]->center_freq);
			continue;
		}

		netdev_info(ndev, "%s: channel is %d\n", __func__, ch);
		sscan_buf->channel[j + 1] = ch;
		j++;
	}
	sscan_buf->channel[0] = j;

	if (request->ssids && request->n_ssids > 0) {
		sscan_buf->n_ssids = request->n_ssids;

		for (i = 0; i < request->n_ssids; i++) {
			ssid_tmp = request->ssids + i;
			sscan_buf->ssid[i] = ssid_tmp->ssid;
		}
	}

	if (request->match_sets && request->n_match_sets > 0) {
		sscan_buf->n_match_ssids = request->n_match_sets;

		for (i = 0; i < request->n_match_sets; i++) {
			match_ssid_tmp = request->match_sets + i;
			sscan_buf->mssid[i] = match_ssid_tmp->ssid.ssid;
		}
	}
	sscan_buf->ie_len = request->ie_len;
	sscan_buf->ie = request->ie;

	spin_lock_bh(&priv->sched_scan_lock);
	vif->priv->sched_scan_request = request;
	vif->priv->sched_scan_vif = vif;
	spin_unlock_bh(&priv->sched_scan_lock);

	ret = sc2332_cmd_sched_scan_start(priv, vif, sscan_buf);
	if (ret)
		sched_scan_cancel(vif);

	kfree(sscan_buf);
	return ret;
}

int sc2332_sched_scan_stop(struct wiphy *wiphy, struct net_device *ndev,
			   u64 reqid)
{
	struct sprd_priv *priv = wiphy_priv(wiphy);
	struct sprd_vif *vif = NULL;
	int ret = 0;

	if (!ndev) {
		netdev_err(ndev, "%s NULL ndev\n", __func__);
		return ret;
	}
	vif = netdev_priv(ndev);
	ret = sc2332_cmd_sched_scan_stop(priv, vif);
	if (!ret) {
		spin_lock_bh(&priv->sched_scan_lock);
		vif->priv->sched_scan_request = NULL;
		vif->priv->sched_scan_vif = NULL;
		spin_unlock_bh(&priv->sched_scan_lock);
	} else {
		netdev_err(ndev, "%s  scan stop failed\n", __func__);
	}
	return ret;
}
