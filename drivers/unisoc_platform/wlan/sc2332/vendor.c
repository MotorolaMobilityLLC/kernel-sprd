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
#include "common/vendor.h"
#include "cmdevt.h"

#define EXTRA_LEN		500
#define SPRD_REINIT_ACS		0x35

#define MAX_CHANNELS		16
#define MAX_BUCKETS		2
#define MAX_CHANNLES_NUM	14

enum vendor_enable_gscan_status {
	VENDOR_GSCAN_STOP = 0,
	VENDOR_GSCAN_START = 1,
};

struct sc2332_wmm_ac_stat {
	u8 ac_num;
	u32 tx_mpdu;
	u32 rx_mpdu;
	u32 mpdu_lost;
	u32 retries;
} __packed;

struct llstat_data {
	u32 beacon_rx;
	u8 rssi_mgmt;
	struct sc2332_wmm_ac_stat ac[WIFI_AC_MAX];
	u32 on_time;
	u32 tx_time;
	u32 rx_time;
	u32 on_time_scan;
} __packed;

enum vendor_subcmds_index {
	VENDOR_SUBCMD_GSCAN_START_INDEX,
	VENDOR_SUBCMD_GSCAN_STOP_INDEX,
	VENDOR_SUBCMD_GSCAN_GET_CAPABILITIES_INDEX,
	VENDOR_SUBCMD_GSCAN_GET_CACHED_RESULTS_INDEX,
	VENDOR_SUBCMD_GSCAN_SCAN_RESULTS_AVAILABLE_INDEX,
	VENDOR_SUBCMD_GSCAN_FULL_SCAN_RESULT_INDEX,
	VENDOR_SUBCMD_GSCAN_SCAN_EVENT_INDEX,
	VENDOR_SUBCMD_GSCAN_HOTLIST_AP_FOUND_INDEX,
	VENDOR_SUBCMD_GSCAN_SET_BSSID_HOTLIST_INDEX,
	VENDOR_SUBCMD_GSCAN_RESET_BSSID_HOTLIST_INDEX,
	VENDOR_SUBCMD_GSCAN_SIGNIFICANT_CHANGE_INDEX,
	VENDOR_SUBCMD_GSCAN_SET_SIGNIFICANT_CHANGE_INDEX,
	VENDOR_SUBCMD_GSCAN_RESET_SIGNIFICANT_CHANGE_INDEX,
	SPRD_ACS_LTE_EVENT_INDEX,
};

struct gscan_capabilities {
	int max_scan_cache_size;
	int max_scan_buckets;
	int max_ap_cache_per_scan;
	int max_rssi_sample_size;
	int max_scan_reporting_threshold;
	int max_hotlist_bssids;
	int max_hotlist_ssids;
	int max_significant_wifi_change_aps;
	int max_bssid_history_entries;
	int max_number_epno_networks;
	int max_number_epno_networks_by_ssid;
	int max_number_of_white_listed_ssid;
};

struct gscan_channel_spec {
	int channel;
	int dwelltimems;
	int passive;
};

struct gscan_bucket_spec {
	int bucket;
	enum vendor_gscan_wifi_band band;
	int period;
	u8 report_events;
	int max_period;
	int exponent;
	int step_count;
	int num_channels;
	struct gscan_channel_spec channels[MAX_CHANNELS];
};

struct gscan_cmd_params {
	int base_period;
	int max_ap_per_scan;
	int report_threshold_percent;
	int report_threshold_num_scans;
	int num_buckets;
	struct gscan_bucket_spec buckets[MAX_BUCKETS];
};

struct cmd_gscan_set_config {
	int base_period;
	int num_buckets;
	struct gscan_bucket_spec buckets[MAX_BUCKETS];
};

struct cmd_gscan_set_scan_config {
	int max_ap_per_scan;
	int report_threshold_percent;
	int report_threshold_num_scans;
};

struct cmd_gscan_channel_list {
	int num_channels;
	int channels[MAX_CHANNLES_NUM];
};

static const u8 *vendor_wpa_scan_get_ie(u8 *res, u8 ie_len, u8 ie)
{
	const u8 *end, *pos;

	pos = res;
	end = pos + ie_len;
	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			break;
		if (pos[0] == ie)
			return pos;
		pos += 2 + pos[1];
	}
	return NULL;
}

static int vendor_parse_bucket(struct sprd_vif *vif, struct nlattr *bucket_spec,
			       struct cmd_gscan_set_config *config)
{
	int rem1, rem2, index, ret = 0, num;
	struct nlattr *attr_bucket, *channel_attr;
	struct nlattr *bucket[GSCAN_ATTR_CONFIG_MAX + 1];
	struct nlattr *channel[GSCAN_ATTR_CONFIG_MAX + 1];

	index = 0;
	nla_for_each_nested(attr_bucket, bucket_spec, rem1) {
		ret = nla_parse(bucket, GSCAN_ATTR_CONFIG_MAX,
				nla_data(attr_bucket), nla_len(attr_bucket),
				NULL, NULL);
		if (ret) {
			netdev_err(vif->ndev, "parse bucket error:%d\n", index);
			return -EINVAL;
		}

		if (!bucket[GSCAN_ATTR_CONFIG_BUCKET_BAND]) {
			netdev_err(vif->ndev, "get bucket band error\n");
			return -EINVAL;
		}
		if (!bucket[GSCAN_ATTR_CONFIG_BUCKET_PERIOD]) {
			netdev_err(vif->ndev, "get bucket period error\n");
			return -EINVAL;
		}
		if (!bucket[GSCAN_ATTR_CONFIG_BUCKET_REPORT_EVENTS]) {
			netdev_err(vif->ndev,
				   "get bucket report event error\n");
			return -EINVAL;
		}
		if (!bucket[GSCAN_ATTR_CONFIG_BUCKET_NUM_CHANNEL_SPECS]) {
			netdev_err(vif->ndev, "get bucket channel num error\n");
			return -EINVAL;
		}
		if (!bucket[GSCAN_ATTR_CONFIG_BUCKET_MAX_PERIOD]) {
			netdev_err(vif->ndev, "get bucket max period error\n");
			return -EINVAL;
		}
		if (!bucket[GSCAN_ATTR_CONFIG_BUCKET_BASE]) {
			netdev_err(vif->ndev, "get bucket base error\n");
			return -EINVAL;
		}
		if (!bucket[GSCAN_ATTR_CONFIG_BUCKET_STEP_COUNT]) {
			netdev_err(vif->ndev, "get bucket step count error\n");
			return -EINVAL;
		}
		if (!bucket[GSCAN_ATTR_CONFIG_CHAN_SPEC]) {
			netdev_err(vif->ndev,
				   "get bucket channel spec error\n");
			return -EINVAL;
		}

		config->buckets[index].band =
		    nla_get_u32(bucket[GSCAN_ATTR_CONFIG_BUCKET_BAND]);
		config->buckets[index].period =
		    nla_get_u32(bucket[GSCAN_ATTR_CONFIG_BUCKET_PERIOD]);
		config->buckets[index].report_events =
		    nla_get_u32(bucket[GSCAN_ATTR_CONFIG_BUCKET_REPORT_EVENTS]);
		config->buckets[index].num_channels =
		    nla_get_u32(bucket
				[GSCAN_ATTR_CONFIG_BUCKET_NUM_CHANNEL_SPECS]);
		config->buckets[index].max_period =
		    nla_get_u32(bucket[GSCAN_ATTR_CONFIG_BUCKET_MAX_PERIOD]);
		config->buckets[index].exponent =
		    nla_get_u32(bucket[GSCAN_ATTR_CONFIG_BUCKET_BASE]);
		config->buckets[index].step_count =
		    nla_get_u32(bucket[GSCAN_ATTR_CONFIG_BUCKET_STEP_COUNT]);

		netdev_info(vif->ndev,
			    "index: %d; bucket_band: %d; bucket_period:%d; "
			    "report_event: %d num_channels: %d "
			    "max_period: %d exponent:%d step_count: %d\n",
			    index, config->buckets[index].band,
			    config->buckets[index].period,
			    config->buckets[index].report_events,
			    config->buckets[index].num_channels,
			    config->buckets[index].max_period,
			    config->buckets[index].exponent,
			    config->buckets[index].step_count);
		num = 0;
		nla_for_each_nested(channel_attr,
				    bucket[GSCAN_ATTR_CONFIG_CHAN_SPEC],
				    rem2) {
			ret = nla_parse(channel, GSCAN_ATTR_CONFIG_MAX,
					nla_data(channel_attr),
					nla_len(channel_attr), NULL, NULL);
			if (ret) {
				netdev_err(vif->ndev, "parse channel error\n");
				return -EINVAL;
			}

			if (!channel[GSCAN_ATTR_CONFIG_CHANNEL_SPEC]) {
				netdev_err(vif->ndev, "parse channel errorn\n");
				return -EINVAL;
			}
			if (!channel[GSCAN_ATTR_CONFIG_CHANNEL_DWELL_TIME]) {
				netdev_err(vif->ndev,
					   "parse channel dwell time errorn\n");
				return -EINVAL;
			}
			if (!channel[GSCAN_ATTR_CONFIG_CHANNEL_PASSIVE]) {
				netdev_err(vif->ndev,
					   "parse channel passive errorn\n");
				return -EINVAL;
			}
			config->buckets[index].channels[num].channel =
			    nla_get_u32(channel[GSCAN_ATTR_CONFIG_CHANNEL_SPEC]);
			config->buckets[index].channels[num].dwelltimems =
			    nla_get_u32(channel
					[GSCAN_ATTR_CONFIG_CHANNEL_DWELL_TIME]);
			config->buckets[index].channels[num].passive =
			    nla_get_u8(channel
				       [GSCAN_ATTR_CONFIG_CHANNEL_PASSIVE]);
			num++;
		}
		index++;
	}
	return ret;
}

/* link layer stats */
static int vendor_link_layer_stat(struct sprd_priv *priv, struct sprd_vif *vif,
				  u8 subtype, const void *buf, u8 len, u8 *r_buf,
				  u16 *r_len)
{
	u8 *sub_cmd, *buf_pos;
	struct sprd_msg *msg;

	msg = get_cmdbuf(priv, vif, len + 1, CMD_LLSTAT);
	if (!msg)
		return -ENOMEM;
	sub_cmd = (u8 *)msg->data;
	*sub_cmd = subtype;
	buf_pos = sub_cmd + 1;
	memcpy(buf_pos, buf, len);

	if (subtype == SUBCMD_SET)
		return send_cmd_recv_rsp(priv, msg, 0, 0);
	else
		return send_cmd_recv_rsp(priv, msg, r_buf, r_len);
}

static int vendor_compose_radio_st(struct sk_buff *reply,
				   struct wifi_radio_stat *radio_st)
{
	/* 2.4G only,radio_num=1,if 5G supported radio_num=2 */
	int radio_num = 1;

	if (nla_put_u32(reply, ATTR_LL_STATS_NUM_RADIOS, radio_num))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_RADIO_ID, radio_st->radio))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_RADIO_ON_TIME,
			radio_st->on_time))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_RADIO_TX_TIME,
			radio_st->tx_time))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_RADIO_NUM_TX_LEVELS,
			radio_st->num_tx_levels))
		goto out_put_fail;
	if (radio_st->num_tx_levels > 0) {
		if (nla_put(reply, ATTR_LL_STATS_RADIO_TX_TIME_PER_LEVEL,
			    sizeof(u32) * radio_st->num_tx_levels,
			    radio_st->tx_time_per_levels))
			goto out_put_fail;
	}
	if (nla_put_u32(reply, ATTR_LL_STATS_RADIO_RX_TIME,
			radio_st->rx_time))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_RADIO_ON_TIME_SCAN,
			radio_st->on_time_scan))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_RADIO_ON_TIME_NBD,
			radio_st->on_time_nbd))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_RADIO_ON_TIME_GSCAN,
			radio_st->on_time_gscan))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_RADIO_ON_TIME_ROAM_SCAN,
			radio_st->on_time_roam_scan))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_RADIO_ON_TIME_PNO_SCAN,
			radio_st->on_time_pno_scan))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_RADIO_ON_TIME_HS20,
			radio_st->on_time_hs20))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_RADIO_NUM_CHANNELS,
			radio_st->num_channels))
		goto out_put_fail;
	if (radio_st->num_channels > 0) {
		struct nlattr *chan_list, *chan_info;

		chan_list = nla_nest_start(reply, ATTR_LL_STATS_CH_INFO);
		chan_info = nla_nest_start(reply, 0);

		if (!chan_list) {
			pr_err("%s %d\n", __func__, __LINE__);
			goto out_put_fail;
		}

		if (!chan_info) {
			pr_err("%s %d\n", __func__, __LINE__);
			goto out_put_fail;
		}

		if (nla_put_u32(reply, ATTR_LL_STATS_CHANNEL_INFO_WIDTH,
				radio_st->channels[0].channel.width))
			goto out_put_fail;

		if (nla_put_u32
		    (reply, ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ,
		     radio_st->channels[0].channel.center_freq))
			goto out_put_fail;

		if (nla_put_u32
		    (reply, ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ0,
		     radio_st->channels[0].channel.center_freq0))
			goto out_put_fail;

		if (nla_put_u32
		    (reply, ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ1,
		     radio_st->channels[0].channel.center_freq1))
			goto out_put_fail;

		if (nla_put_u32(reply, ATTR_LL_STATS_CHANNEL_ON_TIME,
				radio_st->channels[0].on_time))
			goto out_put_fail;

		if (nla_put_u32
		    (reply, ATTR_LL_STATS_CHANNEL_CCA_BUSY_TIME,
		     radio_st->channels[0].cca_busy_time))
			goto out_put_fail;
		nla_nest_end(reply, chan_info);
		nla_nest_end(reply, chan_list);
	}
	return 0;
out_put_fail:
	return -EMSGSIZE;
}

static int vendor_compose_iface_st(struct sk_buff *reply,
				   struct wifi_iface_stat *iface_st)
{
	int i;
	struct nlattr *nest1, *nest2;

	if (nla_put_u32(reply, ATTR_LL_STATS_IFACE_INFO_MODE,
			iface_st->info.mode))
		goto out_put_fail;
	if (nla_put(reply, ATTR_LL_STATS_IFACE_INFO_MAC_ADDR,
		    sizeof(iface_st->info.mac_addr), iface_st->info.mac_addr))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_IFACE_INFO_STATE,
			iface_st->info.state))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_IFACE_INFO_ROAMING,
			iface_st->info.roaming))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_IFACE_INFO_CAPABILITIES,
			iface_st->info.capabilities))
		goto out_put_fail;
	if (nla_put(reply, ATTR_LL_STATS_IFACE_INFO_SSID,
		    sizeof(iface_st->info.ssid), iface_st->info.ssid))
		goto out_put_fail;
	if (nla_put(reply, ATTR_LL_STATS_IFACE_INFO_BSSID,
		    sizeof(iface_st->info.bssid), iface_st->info.bssid))
		goto out_put_fail;
	if (nla_put(reply, ATTR_LL_STATS_IFACE_INFO_AP_COUNTRY_STR,
		    sizeof(iface_st->info.ap_country_str),
		    iface_st->info.ap_country_str))
		goto out_put_fail;
	if (nla_put(reply, ATTR_LL_STATS_IFACE_INFO_COUNTRY_STR,
		    sizeof(iface_st->info.country_str),
		    iface_st->info.country_str))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_IFACE_BEACON_RX,
			iface_st->beacon_rx))
		goto out_put_fail;
	if (nla_put_u64_64bit(reply,
			      ATTR_LL_STATS_IFACE_AVERAGE_TSF_OFFSET,
			      iface_st->average_tsf_offset, 0))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_IFACE_LEAKY_AP_DETECTED,
			iface_st->leaky_ap_detected))
		goto out_put_fail;
	if (nla_put_u32(reply,
			ATTR_LL_STATS_IFACE_LEAKY_AP_AVG_NUM_FRAMES_LEAKED,
			iface_st->leaky_ap_avg_num_frames_leaked))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_IFACE_LEAKY_AP_GUARD_TIME,
			iface_st->leaky_ap_guard_time))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_IFACE_MGMT_RX,
			iface_st->mgmt_rx))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_IFACE_MGMT_ACTION_RX,
			iface_st->mgmt_action_rx))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_IFACE_MGMT_ACTION_TX,
			iface_st->mgmt_action_tx))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_IFACE_RSSI_MGMT,
			iface_st->rssi_mgmt))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_IFACE_RSSI_DATA,
			iface_st->rssi_data))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_IFACE_RSSI_ACK,
			iface_st->rssi_ack))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_IFACE_RSSI_DATA,
			iface_st->rssi_data))
		goto out_put_fail;
	nest1 = nla_nest_start(reply, ATTR_LL_STATS_WMM_INFO);
	if (!nest1)
		goto out_put_fail;
	for (i = 0; i < WIFI_AC_MAX; i++) {
		nest2 = nla_nest_start(reply, ATTR_LL_STATS_WMM_AC_AC);
		if (!nest2)
			goto out_put_fail;
		if (nla_put_u32(reply, ATTR_LL_STATS_WMM_AC_AC,
				iface_st->ac[i].ac))
			goto out_put_fail;
		if (nla_put_u32(reply, ATTR_LL_STATS_WMM_AC_TX_MPDU,
				iface_st->ac[i].tx_mpdu))
			goto out_put_fail;
		if (nla_put_u32(reply, ATTR_LL_STATS_WMM_AC_RX_MPDU,
				iface_st->ac[i].rx_mpdu))
			goto out_put_fail;
		if (nla_put_u32(reply, ATTR_LL_STATS_WMM_AC_TX_MCAST,
				iface_st->ac[i].tx_mcast))
			goto out_put_fail;
		if (nla_put_u32(reply, ATTR_LL_STATS_WMM_AC_RX_MCAST,
				iface_st->ac[i].rx_mcast))
			goto out_put_fail;
		if (nla_put_u32(reply, ATTR_LL_STATS_WMM_AC_RX_AMPDU,
				iface_st->ac[i].rx_ampdu))
			goto out_put_fail;
		if (nla_put_u32(reply, ATTR_LL_STATS_WMM_AC_TX_AMPDU,
				iface_st->ac[i].tx_ampdu))
			goto out_put_fail;
		if (nla_put_u32(reply, ATTR_LL_STATS_WMM_AC_MPDU_LOST,
				iface_st->ac[i].mpdu_lost))
			goto out_put_fail;
		if (nla_put_u32(reply, ATTR_LL_STATS_WMM_AC_RETRIES,
				iface_st->ac[i].retries))
			goto out_put_fail;
		if (nla_put_u32
		    (reply, ATTR_LL_STATS_WMM_AC_RETRIES_SHORT,
		     iface_st->ac[i].retries_short))
			goto out_put_fail;
		if (nla_put_u32(reply, ATTR_LL_STATS_WMM_AC_RETRIES_LONG,
				iface_st->ac[i].retries_long))
			goto out_put_fail;
		if (nla_put_u32(reply,
				ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MIN,
				iface_st->ac[i].contention_time_min))
			goto out_put_fail;
		if (nla_put_u32(reply,
				ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MAX,
				iface_st->ac[i].contention_time_max))
			goto out_put_fail;
		if (nla_put_u32(reply,
				ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_AVG,
				iface_st->ac[i].contention_time_avg))
			goto out_put_fail;
		if (nla_put_u32(reply,
				ATTR_LL_STATS_WMM_AC_CONTENTION_NUM_SAMPLES,
				iface_st->ac[i].contention_num_samples))
			goto out_put_fail;
		nla_nest_end(reply, nest2);
	}
	nla_nest_end(reply, nest1);
	return 0;
out_put_fail:
	return -EMSGSIZE;
}

static int vendor_report_full_scan(struct sprd_vif *vif,
				   struct gscan_result *item)
{
	struct sprd_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	struct sk_buff *reply;
	int payload, rlen, ret = 0;

	rlen = sizeof(struct gscan_result) + item->ie_length;
	payload = rlen + 0x100;
	reply = cfg80211_vendor_event_alloc(wiphy, &vif->wdev, payload,
					    VENDOR_SUBCMD_GSCAN_FULL_SCAN_RESULT_INDEX,
					    GFP_KERNEL);
	if (!reply) {
		ret = -ENOMEM;
		goto out;
	}

	if (nla_put_u32(reply, ATTR_GSCAN_RESULTS_REQUEST_ID, priv->gscan_req_id) ||
	    nla_put_u64_64bit(reply,
			      ATTR_GSCAN_RESULTS_SCAN_RESULT_TIME_STAMP,
			      item->ts, 0) ||
	    nla_put(reply, ATTR_GSCAN_RESULTS_SCAN_RESULT_SSID,
		    sizeof(item->ssid),
		    item->ssid) ||
	    nla_put(reply, ATTR_GSCAN_RESULTS_SCAN_RESULT_BSSID,
		    6,
		    item->bssid) ||
	    nla_put_u32(reply,
			ATTR_GSCAN_RESULTS_SCAN_RESULT_CHANNEL,
			item->channel) ||
	    nla_put_s32(reply, ATTR_GSCAN_RESULTS_SCAN_RESULT_RSSI,
			item->rssi) ||
	    nla_put_u32(reply, ATTR_GSCAN_RESULTS_SCAN_RESULT_RTT,
			item->rtt) ||
	    nla_put_u32(reply,
			ATTR_GSCAN_RESULTS_SCAN_RESULT_RTT_SD,
			item->rtt_sd) ||
	    nla_put_u16(reply,
			ATTR_GSCAN_RESULTS_SCAN_RESULT_BEACON_PERIOD,
			item->beacon_period) ||
	    nla_put_u16(reply,
			ATTR_GSCAN_RESULTS_SCAN_RESULT_CAPABILITY,
			item->capability) ||
	    nla_put_u32(reply,
			ATTR_GSCAN_RESULTS_SCAN_RESULT_IE_LENGTH, item->ie_length)) {
		netdev_err(vif->ndev, "%s nla put fail\n", __func__);
		goto out_put_fail;
	}
	if (nla_put(reply, ATTR_GSCAN_RESULTS_SCAN_RESULT_IE_DATA,
		    item->ie_length, item->ie_data)) {
		netdev_err(vif->ndev, "%s nla put fail\n", __func__);
		goto out_put_fail;
	}
	cfg80211_vendor_event(reply, GFP_KERNEL);
out:
	return ret;

out_put_fail:
	kfree_skb(reply);
	WARN_ON(1);
	return -EMSGSIZE;
}

static int vendor_start_offload_packet(struct wiphy *wiphy, struct sprd_vif *vif,
				       struct nlattr **tb, u32 request_id)
{
	u8 src[ETH_ALEN], dest[ETH_ALEN];
	u32 period, len;
	u16 prot_type;
	u8 *data, *pos;
	int ret = 0;
	struct sprd_priv *priv = wiphy_priv(wiphy);

	if (!tb[ATTR_OFFLOADED_PACKETS_IP_PACKET_DATA] ||
	    !tb[ATTR_OFFLOADED_PACKETS_SRC_MAC_ADDR] ||
	    !tb[ATTR_OFFLOADED_PACKETS_DST_MAC_ADDR] ||
	    !tb[ATTR_OFFLOADED_PACKETS_PERIOD] ||
	    !tb[ATTR_OFFLOADED_PACKETS_ETHER_PROTO_TYPE]) {
		wiphy_err(wiphy, "check start offload para failed\n");
		return -EINVAL;
	}

	period = nla_get_u32(tb[ATTR_OFFLOADED_PACKETS_PERIOD]);
	prot_type = nla_get_u16(tb[ATTR_OFFLOADED_PACKETS_ETHER_PROTO_TYPE]);
	prot_type = htons(prot_type);
	nla_memcpy(src, tb[ATTR_OFFLOADED_PACKETS_SRC_MAC_ADDR], ETH_ALEN);
	nla_memcpy(dest, tb[ATTR_OFFLOADED_PACKETS_DST_MAC_ADDR], ETH_ALEN);
	len = nla_len(tb[ATTR_OFFLOADED_PACKETS_IP_PACKET_DATA]);

	data = kzalloc(len + 14, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	pos = data;
	memcpy(pos, dest, ETH_ALEN);
	pos += ETH_ALEN;
	memcpy(pos, src, ETH_ALEN);
	pos += ETH_ALEN;
	memcpy(pos, &prot_type, 2);
	pos += 2;
	memcpy(pos, nla_data(tb[ATTR_OFFLOADED_PACKETS_IP_PACKET_DATA]), len);

	ret = sc2332_set_packet_offload(priv, vif,
					request_id, 1, period, len + 14, data);
	kfree(data);

	return ret;
}

static int vendor_stop_offload_packet(struct wiphy *wiphy, struct sprd_vif *vif,
				      u32 request_id)
{
	struct sprd_priv *priv = wiphy_priv(wiphy);

	return sc2332_set_packet_offload(priv, vif, request_id, 0, 0, 0, NULL);
}

static int vendor_parse_sae_entry(struct net_device *ndev,
				  struct sae_entry *entry,
				  const void *data, int len)
{
	int rem_len, type, data_len;
	struct nlattr *pos;

	nla_for_each_attr(pos, (void *)data, len, rem_len) {
		type = nla_type(pos);
		switch (type) {
		case VENDOR_SAE_PASSWORD:
			data_len = nla_len(pos);
			entry->passwd_len = data_len;
			nla_strlcpy(entry->password, pos, data_len + 1);
			netdev_info(ndev, "entry->passwd: %s, entry->len:%d\n",
				    entry->password, entry->passwd_len);
			break;
		case VENDOR_SAE_IDENTIFIER:
			data_len = nla_len(pos);
			entry->id_len = data_len;
			nla_strlcpy(entry->identifier, pos, data_len);
			break;
		case VENDOR_SAE_PEER_ADDR:
			nla_memcpy(entry->peer_addr, pos, ETH_ALEN);
			break;
		case VENDOR_SAE_VLAN_ID:
			entry->vlan_id = (s32)nla_get_u32(pos);
			break;
		default:
			break;
		}
	}
	return 0;
}

static int vendor_gscan_start(struct wiphy *wiphy, struct wireless_dev *wdev,
			      const void *data, int len)
{
	struct cmd_gscan_set_config config;
	struct cmd_gscan_set_scan_config scan_params;
	struct sprd_vif *vif = netdev_priv(wdev->netdev);
	struct nlattr *mattributes[GSCAN_ATTR_CONFIG_MAX + 1];
	struct cmd_gscan_rsp_header rsp;
	u16 slen, rlen;
	int ret = 0, start = 1, index;

	rlen = sizeof(struct cmd_gscan_rsp_header);
	memset(&config, 0, sizeof(config));
	memset(&scan_params, 0, sizeof(scan_params));

	ret = nla_parse(mattributes, GSCAN_ATTR_CONFIG_MAX,
			data, len, NULL, NULL);
	if (ret < 0) {
		netdev_err(vif->ndev, "%s failed to parse attribute\n",
			   __func__);
		return -EINVAL;
	}

	if (!mattributes[GSCAN_ATTR_CONFIG_REQUEST_ID]) {
		netdev_err(vif->ndev, "%s get req id error\n", __func__);
		return -EINVAL;
	}
	vif->priv->gscan_req_id =
	    nla_get_u32(mattributes[GSCAN_ATTR_CONFIG_REQUEST_ID]);

	if (!mattributes[GSCAN_ATTR_CONFIG_BASE_PERIOD]) {
		netdev_err(vif->ndev, "%s get base period error\n", __func__);
		return -EINVAL;
	}

	if (!mattributes[GSCAN_ATTR_CONFIG_MAX_AP_PER_SCAN]) {
		netdev_err(vif->ndev, "get max ap perscan error\n");
		return -EINVAL;
	}

	if (!mattributes[GSCAN_ATTR_CONFIG_REPORT_THR]) {
		netdev_err(vif->ndev, "get threshold percent error\n");
		return -EINVAL;
	}
	if (!mattributes[GSCAN_ATTR_CONFIG_REPORT_NUM_SCANS]) {
		netdev_err(vif->ndev, "get report num scans error\n");
		return -EINVAL;
	}
	if (!mattributes[GSCAN_ATTR_CONFIG_NUM_BUCKETS]) {
		netdev_err(vif->ndev, "get num buckets error\n");
		return -EINVAL;
	}
	if (!mattributes[GSCAN_ATTR_CONFIG_BUCKET_SPEC]) {
		netdev_err(vif->ndev, "get bucket spec error\n");
		return -EINVAL;
	}

	config.base_period =
	    nla_get_u32(mattributes[GSCAN_ATTR_CONFIG_BASE_PERIOD]);
	scan_params.max_ap_per_scan =
	    nla_get_u32(mattributes[GSCAN_ATTR_CONFIG_MAX_AP_PER_SCAN]);
	scan_params.report_threshold_percent =
	    nla_get_u32(mattributes[GSCAN_ATTR_CONFIG_REPORT_THR]);
	scan_params.report_threshold_num_scans =
	    nla_get_u32(mattributes[GSCAN_ATTR_CONFIG_REPORT_NUM_SCANS]);
	config.num_buckets =
	    nla_get_u32(mattributes[GSCAN_ATTR_CONFIG_NUM_BUCKETS]);
	netdev_info(vif->ndev, "%s base_period: %d; max_ap_per_scan: %d; "
		    "report_threshold_percent: %d; threshold_num_scans: %d "
		    "num_buckets: %d\n", __func__, config.base_period,
		    scan_params.max_ap_per_scan,
		    scan_params.report_threshold_percent,
		    scan_params.report_threshold_num_scans, config.num_buckets);

	vif->priv->gscan_buckets_num = config.num_buckets;
	for (index = 0; index < MAX_BUCKETS; index++) {
		vif->priv->gscan_res[index].num_results = 0;
		vif->priv->gscan_res[index].scan_id = index + 1;
	}
	vendor_parse_bucket(vif, mattributes[GSCAN_ATTR_CONFIG_BUCKET_SPEC],
			    &config);

	slen = sizeof(struct cmd_gscan_set_config);
	ret = sc2332_set_gscan_config(vif->priv, vif, (void *)(&config),
				      slen, (u8 *)&rsp, &rlen);

	if (ret) {
		netdev_err(vif->ndev, "set gscan config error ret:%d\n", ret);
		return ret;
	}

	slen = sizeof(scan_params);
	ret = sc2332_set_gscan_scan_config(vif->priv, vif,
					   (void *)(&scan_params),
					   slen, (u8 *)&rsp, &rlen);
	if (ret) {
		netdev_err(vif->ndev, "set scan para error ret:%d\n", ret);
		return ret;
	}

	ret = sc2332_enable_gscan(vif->priv, vif, (void *)(&start),
				  (u8 *)&rsp, &rlen);
	if (ret) {
		netdev_err(vif->ndev, "start gscan error ret:%d\n", ret);
		return ret;
	}
	return ret;
}

static int vendor_gscan_stop(struct wiphy *wiphy, struct wireless_dev *wdev,
			     const void *data, int len)
{
	int ret = 0, enable = VENDOR_GSCAN_STOP;
	u16 rlen;
	struct cmd_gscan_rsp_header rsp;
	struct sprd_vif *vif = netdev_priv(wdev->netdev);

	if (!vif)
		return -EINVAL;

	ret = sc2332_enable_gscan(vif->priv, vif, (void *)(&enable),
				  (u8 *)&rsp, &rlen);
	if (ret) {
		netdev_err(vif->ndev, "stop gscan error ret:%d\n", ret);
		return ret;
	}
	return ret;
}

static int vendor_set_country(struct wiphy *wiphy, struct wireless_dev *wdev,
			      const void *data, int len)
{
	struct nlattr *country_attr;
	char *country;

	if (!data) {
		wiphy_err(wiphy, "%s data is NULL!\n", __func__);
		return -EINVAL;
	}

	country_attr = (struct nlattr *)data;
	country = (char *)nla_data(country_attr);

	if (!country || strlen(country) != SPRD_COUNTRY_CODE_LEN) {
		wiphy_err(wiphy, "%s invalid country code!\n", __func__);
		return -EINVAL;
	}
	wiphy_info(wiphy, "%s %c%c\n", __func__,
		   toupper(country[0]), toupper(country[1]));
	return regulatory_hint(wiphy, country);
}

static int vendor_set_llstat_handler(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data, int len)
{
	int ret = 0, err = 0;
	struct sprd_priv *priv = wiphy_priv(wiphy);
	struct sprd_vif *vif = container_of(wdev, struct sprd_vif, wdev);
	struct wifi_link_layer_params *ll_params;
	struct nlattr *tb[ATTR_LL_STATS_SET_MAX + 1];

	if (!priv || !vif)
		return -EIO;
	if (!(priv->fw_capa & SPRD_CAPA_LL_STATS))
		return -ENOTSUPP;
	if (!data) {
		wiphy_err(wiphy, "%s llstat param check filed\n", __func__);
		return -EINVAL;
	}
	err = nla_parse(tb, ATTR_LL_STATS_SET_MAX, data,
			len, ll_stats_policy, NULL);
	if (err)
		return err;
	ll_params = kzalloc(sizeof(*ll_params), GFP_KERNEL);
	if (!ll_params)
		return -ENOMEM;
	if (tb[ATTR_LL_STATS_MPDU_THRESHOLD]) {
		ll_params->mpdu_size_threshold =
		    nla_get_u32(tb[ATTR_LL_STATS_MPDU_THRESHOLD]);
	}
	if (tb[ATTR_LL_STATS_GATHERING]) {
		ll_params->aggressive_statistics_gathering =
		    nla_get_u32(tb[ATTR_LL_STATS_GATHERING]);
	}
	wiphy_err(wiphy, "%s mpdu_size_threshold =%u,"
		  "aggressive_statistics_gathering=%u\n", __func__,
		  ll_params->mpdu_size_threshold,
		  ll_params->aggressive_statistics_gathering);
	if (ll_params->aggressive_statistics_gathering)
		ret = vendor_link_layer_stat(priv, vif, SUBCMD_SET,
					     ll_params, sizeof(*ll_params),
					     0, 0);
	kfree(ll_params);
	return ret;
}

static int vendor_get_llstat_handler(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data, int len)
{
	int ret = 0;
	struct sk_buff *reply_radio = NULL, *reply_iface = NULL;
	struct llstat_data *llst;
	struct wifi_radio_stat *radio_st;
	struct wifi_iface_stat *iface_st;
	u16 r_len = sizeof(*llst);
	u8 r_buf[sizeof(*llst)], i;
	u32 reply_radio_length, reply_iface_length;
	u16 recv_len = sizeof(struct llstat_channel_info) + 4;
	char recv_buf[50] = { 0x00 };
	u32 channel_num = 0;
	struct llstat_channel_info *info;
	char *pos;

	struct sprd_priv *priv = wiphy_priv(wiphy);
	struct sprd_vif *vif = container_of(wdev, struct sprd_vif, wdev);

	if (!priv || !vif)
		return -EIO;
	if (!(priv->fw_capa & SPRD_CAPA_LL_STATS))
		return -ENOTSUPP;
	memset(r_buf, 0, r_len);
	radio_st = kzalloc(sizeof(*radio_st), GFP_KERNEL);
	iface_st = kzalloc(sizeof(*iface_st), GFP_KERNEL);
	if (!radio_st || !iface_st)
		goto out_put_fail;
	ret = vendor_link_layer_stat(priv, vif, SUBCMD_GET, NULL, 0, r_buf, &r_len);
	llst = (struct llstat_data *)r_buf;
	iface_st->info.mode = vif->mode;
	memcpy(iface_st->info.mac_addr, vif->ndev->dev_addr, ETH_ALEN);
	iface_st->info.state = (enum vendor_wifi_connection_state)vif->sm_state;
	memcpy(iface_st->info.ssid, vif->ssid, IEEE80211_MAX_SSID_LEN);
	memcpy(iface_st->info.bssid, vif->bssid, ETH_ALEN);
	iface_st->beacon_rx = llst->beacon_rx;
	iface_st->rssi_mgmt = llst->rssi_mgmt;
	for (i = 0; i < WIFI_AC_MAX; i++) {
		iface_st->ac[i].tx_mpdu = llst->ac[i].tx_mpdu;
		iface_st->ac[i].rx_mpdu = llst->ac[i].rx_mpdu;
		iface_st->ac[i].mpdu_lost = llst->ac[i].mpdu_lost;
		iface_st->ac[i].retries = llst->ac[i].retries;
	}
	radio_st->on_time = llst->on_time;
	radio_st->tx_time = llst->tx_time;
	radio_st->rx_time = llst->rx_time;
	radio_st->on_time_scan = llst->on_time_scan;
	wiphy_err(wiphy, "beacon_rx = %d, rssi_mgmt = %d, on_time = %d, "
		  "tx_time = %d, rx_time = %d, on_time_scan = %d,\n",
		  iface_st->beacon_rx, iface_st->rssi_mgmt, radio_st->on_time,
		  radio_st->tx_time, radio_st->rx_time, radio_st->on_time_scan);
	/* androidR need get channel info to pass vts test */
	if (priv->extend_feature & SPRD_EXTEND_FEATURE_LLSTATE) {
		ret =
		    sc2332_extended_llstate(priv, vif, SUBCMD_GET,
					    SPRD_SUBTYPE_CHANNEL_INFO, NULL,
					    0, recv_buf, &recv_len);
		if (ret) {
			wiphy_err(wiphy, "set externed llstate failed\n");
			goto put_iface_fail;
		}
		pos = recv_buf;
		channel_num = *(u32 *)pos;
		pos += sizeof(u32);
		wiphy_info(wiphy, "channel num %d\n", channel_num);
		radio_st->num_channels = channel_num;

		if (channel_num) {
			info = (struct llstat_channel_info *)(pos);
			wiphy_info(wiphy, "cca busy time: %d, on time: %d\n",
				   info->cca_busy_time, info->on_time);
			wiphy_info(wiphy,
				   "center width: %d, center_freq: %d, "
				   "center_freq0: %d, center_freq1: %d\n",
				   info->channel_width, info->center_freq,
				   info->center_freq0, info->center_freq1);
			radio_st->channels[0].cca_busy_time =
			    info->cca_busy_time;
			radio_st->channels[0].on_time = info->on_time;
			radio_st->channels[0].channel.center_freq =
			    info->center_freq;
			radio_st->channels[0].channel.center_freq0 =
			    info->center_freq0;
			radio_st->channels[0].channel.center_freq1 =
			    info->center_freq1;
			radio_st->channels[0].channel.width =
			    info->channel_width;
		}
	}
	reply_radio_length = sizeof(struct wifi_radio_stat) + 1100;
	reply_iface_length = sizeof(struct wifi_iface_stat) + 1000;

	reply_radio = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
							  reply_radio_length);
	if (!reply_radio) {
		kfree(radio_st);
		kfree(iface_st);
		return -ENOMEM;
	}
	if (nla_put_u32(reply_radio, NL80211_ATTR_VENDOR_ID, OUI_SPREAD))
		goto put_radio_fail;
	if (nla_put_u32(reply_radio, NL80211_ATTR_VENDOR_SUBCMD,
			VENDOR_GET_LLSTAT))
		goto put_radio_fail;
	if (nla_put_u32(reply_radio, ATTR_LL_STATS_TYPE,
			ATTR_CMD_LL_STATS_GET_TYPE_RADIO))
		goto put_radio_fail;
	ret = vendor_compose_radio_st(reply_radio, radio_st);
	ret = cfg80211_vendor_cmd_reply(reply_radio);

	reply_iface = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
							  reply_iface_length);
	if (!reply_iface) {
		kfree(radio_st);
		kfree(iface_st);
		return -ENOMEM;
	}
	if (nla_put_u32(reply_iface, NL80211_ATTR_VENDOR_ID, OUI_SPREAD))
		goto put_iface_fail;
	if (nla_put_u32(reply_iface, NL80211_ATTR_VENDOR_SUBCMD,
			VENDOR_GET_LLSTAT))
		goto put_iface_fail;
	if (nla_put_u32(reply_iface, ATTR_LL_STATS_TYPE,
			ATTR_CMD_LL_STATS_GET_TYPE_IFACE))
		goto put_iface_fail;
	ret = vendor_compose_iface_st(reply_iface, iface_st);
	ret = cfg80211_vendor_cmd_reply(reply_iface);

	kfree(radio_st);
	kfree(iface_st);
	return ret;
put_radio_fail:
	kfree_skb(reply_radio);
put_iface_fail:
	if (reply_iface)
		kfree_skb(reply_iface);
out_put_fail:
	kfree(radio_st);
	kfree(iface_st);
	return -EMSGSIZE;
}

static int vendor_clr_llstat_handler(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data, int len)
{
	int ret = 0;
	struct sk_buff *reply;
	struct wifi_clr_llstat_rsp clr_rsp;
	struct nlattr *tb[ATTR_LL_STATS_CLR_MAX + 1];
	u32 *stats_clear_rsp_mask, stats_clear_req_mask = 0;
	u16 r_len = sizeof(*stats_clear_rsp_mask);
	u8 r_buf[sizeof(*stats_clear_rsp_mask)];
	u32 reply_length, err;

	struct sprd_priv *priv = wiphy_priv(wiphy);
	struct sprd_vif *vif = container_of(wdev, struct sprd_vif, wdev);

	if (!(priv->fw_capa & SPRD_CAPA_LL_STATS))
		return -ENOTSUPP;
	memset(r_buf, 0, r_len);
	if (!data) {
		wiphy_err(wiphy, "%s wrong llstat clear req mask\n", __func__);
		return -EINVAL;
	}
	err = nla_parse(tb, ATTR_LL_STATS_CLR_MAX, data, len, NULL, NULL);
	if (err)
		return err;
	if (tb[ATTR_LL_STATS_CLR_CONFIG_REQ_MASK]) {
		stats_clear_req_mask =
		    nla_get_u32(tb[ATTR_LL_STATS_CLR_CONFIG_REQ_MASK]);
	}
	wiphy_info(wiphy, "stats_clear_req_mask = %u\n", stats_clear_req_mask);
	ret = vendor_link_layer_stat(priv, vif, SUBCMD_DEL,
				     &stats_clear_req_mask, r_len, r_buf,
				     &r_len);
	stats_clear_rsp_mask = (u32 *)r_buf;
	clr_rsp.stats_clear_rsp_mask = *stats_clear_rsp_mask;
	clr_rsp.stop_rsp = 1;

	reply_length = sizeof(clr_rsp) + 100;
	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, reply_length);
	if (!reply)
		return -ENOMEM;
	if (nla_put_u32(reply, NL80211_ATTR_VENDOR_ID, OUI_SPREAD))
		goto out_put_fail;
	if (nla_put_u32(reply, NL80211_ATTR_VENDOR_SUBCMD,
			VENDOR_CLR_LLSTAT))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_CLR_CONFIG_RSP_MASK,
			clr_rsp.stats_clear_rsp_mask))
		goto out_put_fail;
	if (nla_put_u32(reply, ATTR_LL_STATS_CLR_CONFIG_STOP_RSP,
			clr_rsp.stop_rsp))
		goto out_put_fail;
	ret = cfg80211_vendor_cmd_reply(reply);

	return ret;
out_put_fail:
	kfree_skb(reply);
	pr_err("%s out put fail\n", __func__);
	return -EMSGSIZE;
}

static int vendor_get_gscan_capabilities(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data, int len)
{
	u16 rlen;
	struct sk_buff *reply;
	int ret = 0, payload;
	struct sprd_vif *vif = netdev_priv(wdev->netdev);
	struct cmd_gscan_rsp_header *hdr;
	struct gscan_capabilities *p = NULL;
	void *rbuf;

	rlen = sizeof(struct gscan_capabilities) +
	    sizeof(struct cmd_gscan_rsp_header);
	rbuf = kmalloc(rlen, GFP_KERNEL);
	if (!rbuf)
		return -ENOMEM;

	ret = sc2332_get_gscan_capabilities(vif->priv, vif, (u8 *)rbuf, &rlen);
	if (ret < 0) {
		netdev_err(vif->ndev, "%s failed to get capabilities!\n",
			   __func__);
		goto out;
	}
	hdr = (struct cmd_gscan_rsp_header *)rbuf;
	p = (struct gscan_capabilities *)
	    (rbuf + sizeof(struct cmd_gscan_rsp_header));

	payload = rlen + EXTRA_LEN;
	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, payload);
	if (!reply) {
		ret = -ENOMEM;
		goto out;
	}

	if (nla_put_u32(reply, ATTR_GSCAN_SCAN_CACHE_SIZE, p->max_scan_cache_size) ||
	    nla_put_u32(reply, ATTR_GSCAN_MAX_SCAN_BUCKETS, p->max_scan_buckets) ||
	    nla_put_u32(reply, ATTR_GSCAN_MAX_AP_CACHE_PER_SCAN,
			p->max_ap_cache_per_scan) ||
	    nla_put_u32(reply, ATTR_GSCAN_MAX_RSSI_SAMPLE_SIZE,
			p->max_rssi_sample_size) ||
	    nla_put_s32(reply, ATTR_GSCAN_MAX_SCAN_REPORTING_THRESHOLD,
			p->max_scan_reporting_threshold) ||
	    nla_put_u32(reply, ATTR_GSCAN_MAX_HOTLIST_BSSIDS,
			p->max_hotlist_bssids) ||
	    nla_put_u32(reply, ATTR_GSCAN_MAX_SIGNIFICANT_WIFI_CHANGE_APS,
			p->max_significant_wifi_change_aps) ||
	    nla_put_u32(reply, ATTR_GSCAN_MAX_BSSID_HISTORY_ENTRIES,
			p->max_bssid_history_entries) ||
	    nla_put_u32(reply, ATTR_GSCAN_MAX_HOTLIST_SSIDS,
			p->max_hotlist_bssids) ||
	    nla_put_u32(reply, ATTR_GSCAN_MAX_NUM_EPNO_NETS,
			p->max_number_epno_networks) ||
	    nla_put_u32(reply, ATTR_GSCAN_MAX_NUM_EPNO_NETS_BY_SSID,
			p->max_number_epno_networks_by_ssid) ||
	    nla_put_u32(reply, ATTR_GSCAN_MAX_NUM_WHITELISTED_SSID,
			p->max_number_of_white_listed_ssid)) {
		netdev_err(vif->ndev, "failed to put channel number\n");
		goto out_put_fail;
	}
	ret = cfg80211_vendor_cmd_reply(reply);
	if (ret)
		netdev_err(vif->ndev, "%s failed to reply skb!\n", __func__);
out:
	kfree(rbuf);
	return ret;
out_put_fail:
	kfree_skb(reply);
	kfree(rbuf);
	pr_err("%s out put fail\n", __func__);
	return -EMSGSIZE;
}

static int vendor_get_channel_list(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data, int len)
{
	struct sk_buff *reply;
	int payload, ret = 0, band, max_channels;
	u16 rlen;
	void *rbuf;
	struct sprd_vif *vif = netdev_priv(wdev->netdev);
	struct cmd_gscan_channel_list channel_list;
	struct cmd_gscan_rsp_header *hdr;
	struct cmd_gscan_channel_list *p = NULL;
	struct nlattr *mattributes[GSCAN_ATTR_CONFIG_MAX + 1];

	ret = nla_parse(mattributes, GSCAN_ATTR_CONFIG_MAX,
			data, len, NULL, NULL);
	if (ret < 0) {
		netdev_err(vif->ndev, "failed to parse attribute\n");
		return -EINVAL;
	}

	if (!mattributes[GSCAN_ATTR_CONFIG_WIFI_BAND]) {
		netdev_err(vif->ndev, "%s get band error\n", __func__);
		return -EINVAL;
	}
	band = nla_get_u32(mattributes[GSCAN_ATTR_CONFIG_WIFI_BAND]);
	netdev_info(vif->ndev, "%s  band is : %d\n", __func__, band);

	if (!mattributes[GSCAN_ATTR_CONFIG_MAX_CHANNELS]) {
		netdev_err(vif->ndev, "%s get max channel error\n", __func__);
		return -EINVAL;
	}
	max_channels = nla_get_u32(mattributes[GSCAN_ATTR_CONFIG_MAX_CHANNELS]);
	netdev_info(vif->ndev, "maxchannels is : %d\n", max_channels);

	rlen = sizeof(struct cmd_gscan_channel_list) +
	    sizeof(struct cmd_gscan_rsp_header);
	rbuf = kmalloc(rlen, GFP_KERNEL);
	if (!rbuf)
		return -ENOMEM;
	memset(rbuf, 0x0, rlen);

	ret = sc2332_get_gscan_channel_list(vif->priv, vif,
					    (void *)&band, (u8 *)rbuf, &rlen);
	if (ret < 0) {
		netdev_err(vif->ndev, "%s failed to get channel!\n", __func__);
		goto out;
	}

	hdr = (struct cmd_gscan_rsp_header *)rbuf;
	p = (struct cmd_gscan_channel_list *)
	    (rbuf + sizeof(struct cmd_gscan_rsp_header));

	payload = sizeof(channel_list);
	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, payload);
	if (!reply) {
		netdev_err(vif->ndev, "failed to alloc reply skb\n");
		goto out;
	}

	if (nla_put_u32(reply, ATTR_GSCAN_NUM_CHANNELS,
			p->num_channels)) {
		netdev_err(vif->ndev, "failed to put channel number\n");
		goto out_put_fail;
	}

	if (nla_put(reply, ATTR_GSCAN_CHANNELS,
		    sizeof(channel_list.channels), (void *)p->channels)) {
		netdev_err(vif->ndev, "%s failed to put channels\n", __func__);
		goto out_put_fail;
	}
	ret = cfg80211_vendor_cmd_reply(reply);
	if (ret)
		netdev_err(vif->ndev, "%s reply vendor error\n", __func__);

out:
	kfree(rbuf);
	return ret;
out_put_fail:
	kfree_skb(reply);
	kfree(rbuf);
	pr_err("%s out put fail\n", __func__);
	return -EMSGSIZE;
}

static int vendor_get_cached_gscan_results(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data, int len)
{
	int ret = 0, i, j, rlen, payload, request_id = 0, moredata = 0;
	int rem_len, type, flush = 0, max_param = 0, n, buckets_scanned = 1;
	struct sprd_vif *vif = netdev_priv(wdev->netdev);
	struct sk_buff *reply;
	struct nlattr *pos, *scan_res, *cached_list, *res_list;
	struct nlattr *ap;
	struct sprd_gscan_cached_results *p;

	nla_for_each_attr(pos, (void *)data, len, rem_len) {
		type = nla_type(pos);
		switch (type) {
		case GSCAN_ATTR_CONFIG_REQUEST_ID:
			request_id = nla_get_u32(pos);
			break;
		case GSCAN_ATTR_CONFIG_CACHED_PARAM_FLUSH:
			flush = nla_get_u32(pos);
			break;
		case GSCAN_ATTR_CONFIG_CACHED_PARAM_MAX:
			max_param = nla_get_u32(pos);
			break;
		default:
			netdev_err(vif->ndev,
				   "nla gscan result 0x%x not support\n", type);
			ret = -EINVAL;
			break;
		}
		if (ret < 0)
			break;
	}

	rlen = vif->priv->gscan_buckets_num
	    * sizeof(struct sprd_gscan_cached_results);
	payload = rlen + 0x100;
	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, payload);
	if (!reply)
		return -ENOMEM;
	for (i = 0; i < vif->priv->gscan_buckets_num; i++) {
		if (!(vif->priv->gscan_res + i)->num_results)
			continue;

		for (j = 0; j <= (vif->priv->gscan_res + i)->num_results; j++) {
			if (time_after(jiffies - VENDOR_SCAN_RESULT_EXPIRE,
				       (unsigned long)
				       (vif->priv->gscan_res +
					i)->results[j].ts)) {
				memcpy((void *)
				       (&(vif->priv->gscan_res + i)->results
					[j]), (void *)
				       (&(vif->priv->gscan_res + i)->results
					[j + 1]), sizeof(struct gscan_result)
				       * ((vif->priv->gscan_res +
					 i)->num_results - j - 1));
				(vif->priv->gscan_res + i)->num_results--;
				j = 0;
			}
		}

		if (nla_put_u32(reply, ATTR_GSCAN_RESULTS_REQUEST_ID,
				request_id) ||
		    nla_put_u32(reply,
				ATTR_GSCAN_RESULTS_NUM_RESULTS_AVAILABLE,
				(vif->priv->gscan_res + i)->num_results)) {
			netdev_err(vif->ndev, "failed to put!\n");
			goto out_put_fail;
		}

		if (nla_put_u8(reply,
			       ATTR_GSCAN_RESULTS_SCAN_RESULT_MORE_DATA, moredata)) {
			netdev_err(vif->ndev, "failed to put!\n");
			goto out_put_fail;
		}

		if (nla_put_u32(reply,
				ATTR_GSCAN_CACHED_RESULTS_SCAN_ID,
				(vif->priv->gscan_res + i)->scan_id)) {
			netdev_err(vif->ndev, "failed to put!\n");
			goto out_put_fail;
		}

		if ((vif->priv->gscan_res + i)->num_results == 0)
			break;

		cached_list = nla_nest_start(reply, ATTR_GSCAN_CACHED_RESULTS_LIST);
		for (n = 0; n < vif->priv->gscan_buckets_num; n++) {
			res_list = nla_nest_start(reply, n);

			if (!res_list)
				goto out_put_fail;

			if (nla_put_u32(reply,
					ATTR_GSCAN_CACHED_RESULTS_SCAN_ID,
					(vif->priv->gscan_res + i)->scan_id)) {
				netdev_err(vif->ndev, "failed to put!\n");
				goto out_put_fail;
			}

			if (nla_put_u32(reply,
					ATTR_GSCAN_CACHED_RESULTS_FLAGS,
					(vif->priv->gscan_res + i)->flags)) {
				netdev_err(vif->ndev, "failed to put!\n");
				goto out_put_fail;
			}

			if (nla_put_u32(reply,
					ATTR_GSCAN_RESULTS_BUCKETS_SCANNED,
					buckets_scanned)) {
				netdev_err(vif->ndev, "failed to put!\n");
				goto out_put_fail;
			}

			if (nla_put_u32(reply,
					ATTR_GSCAN_RESULTS_NUM_RESULTS_AVAILABLE,
					(vif->priv->gscan_res +
					 i)->num_results)) {
				netdev_err(vif->ndev, "failed to put!\n");
				goto out_put_fail;
			}

			scan_res = nla_nest_start(reply, ATTR_GSCAN_RESULTS_LIST);
			if (!scan_res)
				goto out_put_fail;

			for (j = 0;
			     j < (vif->priv->gscan_res + i)->num_results; j++) {
				p = vif->priv->gscan_res + i;
				netdev_info(vif->ndev,
					    "[index = %d] Timestamp(%lu) Ssid (%s) Bssid: %pM "
					    "Channel (%d) Rssi (%d) RTT (%lu) RTT_SD (%lu)\n",
					    j, (unsigned long)p->results[j].ts,
					    p->results[j].ssid,
					    p->results[j].bssid,
					    p->results[j].channel,
					    p->results[j].rssi,
					    (unsigned long)p->results[j].rtt,
					    (unsigned long)
					    p->results[j].rtt_sd);
				ap = nla_nest_start(reply, j + 1);
				if (!ap) {
					netdev_err(vif->ndev,
						   "failed to put!\n");
					goto out_put_fail;
				}
				if (nla_put_u64_64bit(reply,
						      ATTR_GSCAN_RESULTS_SCAN_RESULT_TIME_STAMP,
						      p->results[j].ts, 0)) {
					netdev_err(vif->ndev,
						   "failed to put!\n");
					goto out_put_fail;
				}
				if (nla_put(reply,
					    ATTR_GSCAN_RESULTS_SCAN_RESULT_SSID,
					    sizeof(p->results[j].ssid),
					    p->results[j].ssid)) {
					netdev_err(vif->ndev,
						   "failed to put!\n");
					goto out_put_fail;
				}
				if (nla_put(reply,
					    ATTR_GSCAN_RESULTS_SCAN_RESULT_BSSID,
					    sizeof(p->results[j].bssid),
					    p->results[j].bssid)) {
					netdev_err(vif->ndev,
						   "failed to put!\n");
					goto out_put_fail;
				}
				if (nla_put_u32(reply,
						ATTR_GSCAN_RESULTS_SCAN_RESULT_CHANNEL,
						p->results[j].channel)) {
					netdev_err(vif->ndev,
						   "failed to put!\n");
					goto out_put_fail;
				}
				if (nla_put_s32(reply,
						ATTR_GSCAN_RESULTS_SCAN_RESULT_RSSI,
						p->results[j].rssi)) {
					netdev_err(vif->ndev,
						   "failed to put!\n");
					goto out_put_fail;
				}
				if (nla_put_u32(reply,
						ATTR_GSCAN_RESULTS_SCAN_RESULT_RTT,
						p->results[j].rtt)) {
					netdev_err(vif->ndev,
						   "failed to put!\n");
					goto out_put_fail;
				}
				if (nla_put_u32(reply,
						ATTR_GSCAN_RESULTS_SCAN_RESULT_RTT_SD,
						p->results[j].rtt_sd)) {
					netdev_err(vif->ndev,
						   "failed to put!\n");
					goto out_put_fail;
				}
				nla_nest_end(reply, ap);
			}
			nla_nest_end(reply, scan_res);
			nla_nest_end(reply, res_list);
		}
		nla_nest_end(reply, cached_list);
	}

	ret = cfg80211_vendor_cmd_reply(reply);
	if (ret < 0)
		netdev_err(vif->ndev, "%s failed to reply skb!\n", __func__);

	return ret;

out_put_fail:
	kfree_skb(reply);
	pr_err("%s out put fail\n", __func__);
	return -EMSGSIZE;
}

static int vendor_cached_scan_result(struct sprd_vif *vif, u8 bucket_id,
				     struct gscan_result *item)
{
	struct sprd_priv *priv = vif->priv;
	u32 i;
	struct sprd_gscan_cached_results *p;

	if (IS_ERR_OR_NULL(priv->gscan_res))
		netdev_err(vif->ndev, "%s gscan res invalid!\n", __func__);

	if (bucket_id >= priv->gscan_buckets_num || !priv->gscan_res ||
	    bucket_id > MAX_BUCKETS) {
		netdev_err(vif->ndev, "%s the gscan buffer invalid!\n",
			   __func__);
		return -EINVAL;
	}
	p = priv->gscan_res + bucket_id;

	if (p->num_results >= MAX_AP_CACHE_PER_SCAN) {
		netdev_err(vif->ndev, "%s the scan result reach the MAX num.\n",
			   __func__);
		return -EINVAL;
	}
	netdev_info(vif->ndev, "%s buketid: %d ,num_results:%d !\n",
		    __func__, bucket_id, p->num_results);
	for (i = 0; i < p->num_results; i++) {
		if (time_after(jiffies - VENDOR_SCAN_RESULT_EXPIRE,
			       (unsigned long)p->results[i].ts)) {
			memcpy((void *)(&p->results[i]),
			       (void *)(&p->results[i + 1]),
			       sizeof(struct gscan_result)
			       * (p->num_results - i - 1));

			p->num_results--;
		}
		if (!memcmp(p->results[i].bssid, item->bssid, ETH_ALEN) &&
		    strlen(p->results[i].ssid) == strlen(item->ssid) &&
		    !memcmp(p->results[i].ssid, item->ssid,
			    strlen(item->ssid))) {
			netdev_err(vif->ndev, "%s BSS : %s  %pM exist.\n",
				   __func__, item->ssid, item->bssid);
			memcpy((void *)(&p->results[i]),
			       (void *)item, sizeof(struct gscan_result));
			return 0;
		}
	}

	memcpy((void *)(&p->results[p->num_results]),
	       (void *)item, sizeof(struct gscan_result));
	p->results[p->num_results].ie_length = 0;
	p->results[p->num_results].ie_data[0] = 0;
	p->num_results++;
	return 0;
}

static int vendor_get_logger_feature(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data, int len)
{
	int ret = 0;
	struct sk_buff *reply;
	int feature, payload;

	payload = sizeof(feature);
	feature = 0;
	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, payload);

	if (!reply)
		return -ENOMEM;

	feature |= WIFI_LOGGER_CONNECT_EVENT_SUPPORTED;

	/* vts will test wake reason state function */
	feature |= WIFI_LOGGER_WAKE_LOCK_SUPPORTED;

	if (nla_put_u32(reply, ATTR_FEATURE_SET, feature)) {
		wiphy_err(wiphy, "%s put skb u32 failed\n", __func__);
		goto out_put_fail;
	}

	ret = cfg80211_vendor_cmd_reply(reply);
	if (ret)
		wiphy_err(wiphy, "%s reply cmd error\n", __func__);
	return ret;

out_put_fail:
	kfree_skb(reply);
	return -EMSGSIZE;
}

static int vendor_get_feature(struct wiphy *wiphy, struct wireless_dev *wdev,
			      const void *data, int len)
{
	int ret = 0;
	struct sk_buff *reply;
	int feature = 0, payload;
	struct sprd_priv *priv = wiphy_priv(wiphy);

	wiphy_info(wiphy, "%s\n", __func__);
	payload = sizeof(feature);

	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, payload);

	feature |= WIFI_FEATURE_INFRA;
	feature |= WIFI_FEATURE_HOTSPOT;
	feature |= WIFI_FEATURE_P2P;
	feature |= WIFI_FEATURE_SOFT_AP;
	/* must support feature logger, see bug 908664 */
	feature |= WIFI_FEATURE_LOGGER;

	if (priv->fw_capa & SPRD_CAPA_SCAN_RANDOM_MAC_ADDR)
		feature |= WIFI_FEATURE_SCAN_RAND;

	if (priv->fw_capa & SPRD_CAPA_TDLS)
		feature |= WIFI_FEATURE_TDLS;
	if (priv->fw_capa & SPRD_CAPA_LL_STATS)
		feature |= WIFI_FEATURE_LINK_LAYER_STATS;
	if (!reply)
		return -ENOMEM;

	if (nla_put_u32(reply, ATTR_FEATURE_SET, feature)) {
		wiphy_err(wiphy, "%s put u32 error\n", __func__);
		goto out_put_fail;
	}

	ret = cfg80211_vendor_cmd_reply(reply);
	if (ret)
		wiphy_err(wiphy, "%s reply cmd error\n", __func__);
	return ret;

out_put_fail:
	kfree_skb(reply);
	return -EMSGSIZE;
}

static int vendor_get_wake_state(struct wiphy *wiphy, struct wireless_dev *wdev,
				 const void *data, int len)
{
	int ret = 0;
	struct sk_buff *skb;
	int payload;

	wiphy_info(wiphy, "%s\n", __func__);
	payload = NLMSG_HDRLEN;
	payload += ATTR_WAKE_MAX * (NLMSG_HDRLEN + sizeof(unsigned int));

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, payload);

	if (!skb)
		return -ENOMEM;

	if (nla_put_u32(skb, ATTR_WAKE_TOTAL_CMD_EVT_WAKE, 0) ||
	    nla_put_u32(skb, ATTR_WAKE_CMD_EVT_WAKE_CNT_PTR, 0) ||
	    nla_put_u32(skb, ATTR_WAKE_CMD_EVT_WAKE_CNT_SZ, 0) ||
	    nla_put_u32(skb, ATTR_WAKE_TOTAL_DRV_FW_LOCAL_WAKE, 0) ||
	    nla_put_u32(skb, ATTR_WAKE_DRV_FW_LOCAL_WAKE_CNT_PTR, 0) ||
	    nla_put_u32(skb, ATTR_WAKE_DRV_FW_LOCAL_WAKE_CNT_SZ, 0) ||
	    nla_put_u32(skb, ATTR_WAKE_TOTAL_RX_DATA_WAKE, 0) ||
	    nla_put_u32(skb, ATTR_WAKE_RX_UNICAST_CNT, 0) ||
	    nla_put_u32(skb, ATTR_WAKE_RX_MULTICAST_CNT, 0) ||
	    nla_put_u32(skb, ATTR_WAKE_RX_BROADCAST_CNT, 0) ||
	    nla_put_u32(skb, ATTR_WAKE_ICMP_PKT, 0) ||
	    nla_put_u32(skb, ATTR_WAKE_ICMP6_PKT, 0) ||
	    nla_put_u32(skb, ATTR_WAKE_ICMP6_RA, 0) ||
	    nla_put_u32(skb, ATTR_WAKE_ICMP6_NA, 0) ||
	    nla_put_u32(skb, ATTR_WAKE_ICMP6_NS, 0) ||
	    nla_put_u32(skb, ATTR_WAKE_ICMP4_RX_MULTICAST_CNT, 0) ||
	    nla_put_u32(skb, ATTR_WAKE_ICMP6_RX_MULTICAST_CNT, 0) ||
	    nla_put_u32(skb, ATTR_WAKE_OTHER_RX_MULTICAST_CNT, 0)) {
		wiphy_err(wiphy, "%s nla put error\n", __func__);
		goto out_put_fail;
	}

	ret = cfg80211_vendor_cmd_reply(skb);
	if (ret)
		wiphy_err(wiphy, "%s reply cmd error\n", __func__);
	return ret;

out_put_fail:
	kfree_skb(skb);
	return -EMSGSIZE;
}

static int vendor_enable_nd_offload(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int len)
{
	wiphy_info(wiphy, "%s\n", __func__);

	return VENDOR_WIFI_SUCCESS;
}

static int vendor_set_mac_oui(struct wiphy *wiphy, struct wireless_dev *wdev,
			      const void *data, int len)
{
	wiphy_info(wiphy, "%s\n", __func__);

	return VENDOR_WIFI_SUCCESS;
}

static int vendor_start_logging(struct wiphy *wiphy, struct wireless_dev *wdev,
				const void *data, int len)
{
	wiphy_info(wiphy, "%s\n", __func__);

	return VENDOR_WIFI_SUCCESS;
}

static int vendor_get_ring_data(struct wiphy *wiphy, struct wireless_dev *wdev,
				const void *data, int len)
{
	wiphy_info(wiphy, "%s\n", __func__);

	return VENDOR_WIFI_SUCCESS;
}

static int vendor_memory_dump(struct wiphy *wiphy, struct wireless_dev *wdev,
			      const void *data, int len)
{
	wiphy_info(wiphy, "%s\n", __func__);

	return -EOPNOTSUPP;
}

static int vendor_get_driver_info(struct wiphy *wiphy,
				  struct wireless_dev *wdev,
				  const void *data, int len)
{
	int ret = 0, payload;
	struct sk_buff *reply;
	char *version = "1.0";

	wiphy_info(wiphy, "%s\n", __func__);

	payload = strlen(version);
	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, payload);

	if (!reply)
		return -ENOMEM;

	if (nla_put(reply, ATTR_WIFI_INFO_DRIVER_VERSION,
		    payload, version)) {
		wiphy_err(wiphy, "%s put version error\n", __func__);
		goto out_put_fail;
	}

	ret = cfg80211_vendor_cmd_reply(reply);
	if (ret)
		wiphy_err(wiphy, "%s reply cmd error\n", __func__);
	return ret;

out_put_fail:
	kfree_skb(reply);
	return -EMSGSIZE;
}

static int vendor_get_akm_suite(struct wiphy *wiphy, struct wireless_dev *wdev,
				const void *data, int len)
{
	int ret = 0, akm_len;
	struct sprd_priv *priv;
	struct sk_buff *reply;
	int index = 0;
	int akm[8] = { 0 };

	priv = wiphy_priv(wiphy);
	if (priv->extend_feature & SPRD_EXTEND_FEATURE_SAE)
		akm[index++] = WLAN_AKM_SUITE_SAE;
	if (priv->extend_feature & SPRD_EXTEND_FEATURE_OWE)
		akm[index++] = WLAN_AKM_SUITE_OWE;
	if (priv->extend_feature & SPRD_EXTEND_FEATURE_DPP)
		akm[index++] = WLAN_CIPHER_SUITE_DPP;
	if (priv->extend_feature & SPRD_EXTEND_8021X_SUITE_B_192)
		akm[index++] = WLAN_CIPHER_SUITE_BIP_GMAC_256;

	akm_len = index * sizeof(akm[0]);
	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, len);
	if (!reply)
		return -ENOMEM;
	ret = nla_put(reply, NL80211_ATTR_AKM_SUITES, akm_len, akm);
	if (ret) {
		wiphy_err(wiphy, "put akm suite error\n");
		kfree_skb(reply);
		return ret;
	}
	ret = cfg80211_vendor_cmd_reply(reply);
	if (ret) {
		wiphy_err(wiphy, "reply cmd error\n");
		return ret;
	}
	return 0;
}

static int vendor_set_offload_packet(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data, int len)
{
	int err;
	u8 control;
	u32 req;
	struct sprd_vif *vif = container_of(wdev, struct sprd_vif, wdev);
	struct nlattr *tb[ATTR_OFFLOADED_PACKETS_MAX + 1];

	if (!data) {
		wiphy_err(wiphy, "%s offload failed\n", __func__);
		return -EINVAL;
	}

	err = nla_parse(tb, ATTR_OFFLOADED_PACKETS_MAX, data, len, NULL, NULL);
	if (err) {
		wiphy_err(wiphy, "%s parse attr failed", __func__);
		return err;
	}

	if (!tb[ATTR_OFFLOADED_PACKETS_REQUEST_ID] ||
	    !tb[ATTR_OFFLOADED_PACKETS_SENDING_CONTROL]) {
		wiphy_err(wiphy, "check request id or control failed\n");
		return -EINVAL;
	}

	req = nla_get_u32(tb[ATTR_OFFLOADED_PACKETS_REQUEST_ID]);
	control = nla_get_u32(tb[ATTR_OFFLOADED_PACKETS_SENDING_CONTROL]);

	switch (control) {
	case VENDOR_OFFLOADED_PACKETS_SENDING_STOP:
		return vendor_stop_offload_packet(wiphy, vif, req);
	case VENDOR_OFFLOADED_PACKETS_SENDING_START:
		return vendor_start_offload_packet(wiphy, vif, tb, req);
	default:
		wiphy_err(wiphy, "control value is invalid\n");
		return -EINVAL;
	}
}

static int vendor_softap_set_sae_para(struct sprd_vif *vif,
				      struct softap_sae_setting *setting)
{
	char *pos, *data;
	int len, header_len, index, data_len, ret, *d;
	struct sae_param *param;
	struct sae_entry *tmp;
	struct sprd_msg *msg;
	struct tlv_data *tlv;

	data = kzalloc(1024, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	pos = data;
	data_len = 0;
	header_len = sizeof(struct tlv_data);

	for (index = 0; index < SPRD_SAE_MAX_NUM; index++) {
		if (setting->entry[index].used == 0)
			break;

		tmp = &setting->entry[index];
		/* add sae entry tlv first */
		tlv = (struct tlv_data *)pos;
		tlv->type = VENDOR_SAE_ENTRY - 1;
		tlv->len = 0;
		pos += header_len;
		data_len += header_len;

		/* PASSWORD ELEMENT */
		if (tmp->passwd_len > 0) {
			tlv = (struct tlv_data *)pos;
			tlv->type = VENDOR_SAE_PASSWORD - 1;
			tlv->len = tmp->passwd_len;

			memcpy(tlv->data, tmp->password, tmp->passwd_len);
			netdev_info(vif->ndev, "%s password: %s, len:%d\n",
				    __func__, tmp->password, tmp->passwd_len);
			pos += (header_len + tmp->passwd_len);
			data_len += (header_len + tmp->passwd_len);
		}

		/* IDENTIFIER ELEMENT */
		tmp = &setting->entry[index];
		if (tmp->id_len > 0) {
			tlv = (struct tlv_data *)pos;
			tlv->type = VENDOR_SAE_IDENTIFIER - 1;
			tlv->len = tmp->id_len;
			memcpy(tlv->data, tmp->identifier, tmp->id_len);
			netdev_info(vif->ndev, "%s id: %s, len:%d\n", __func__,
				    tmp->identifier, tmp->id_len);
			pos += (header_len + tmp->id_len);
			data_len += (header_len + tmp->id_len);
		}
		/* PEER_ADDRESS ELEMENT */
		if (!is_zero_ether_addr(tmp->peer_addr)) {
			tlv = (struct tlv_data *)pos;
			tlv->type = VENDOR_SAE_PEER_ADDR - 1;
			tlv->len = ETH_ALEN;

			memcpy(tlv->data, tmp->peer_addr, ETH_ALEN);
			pos += (header_len + ETH_ALEN);
			data_len += (header_len + ETH_ALEN);
		}
		/* VLAN_ID ELEMENT */
		if (tmp->vlan_id != -1) {
			tlv = (struct tlv_data *)pos;
			tlv->type = VENDOR_SAE_VLAN_ID - 1;
			tlv->len = sizeof(tmp->vlan_id);
			d = (u32 *)tlv->data;
			*d = (u32)tmp->vlan_id;
			pos += (header_len + sizeof(tmp->vlan_id));
			data_len += (header_len + sizeof(tmp->vlan_id));
		}
	}

	if (setting->passphrase_len) {
		/* add entry */
		tlv = (struct tlv_data *)pos;
		tlv->type = VENDOR_SAE_ENTRY - 1;
		tlv->len = 0;
		pos += header_len;
		data_len += header_len;

		/* PASSWORD ELEMENT */
		tlv = (struct tlv_data *)pos;
		tlv->type = VENDOR_SAE_PWD - 1;
		tlv->len = setting->passphrase_len;
		memcpy(tlv->data, setting->passphrase, setting->passphrase_len);
		netdev_info(vif->ndev, "%s passphrase: %s, len:%d\n", __func__,
			    setting->passphrase, setting->passphrase_len);
		pos += (header_len + setting->passphrase_len);
		data_len += (header_len + setting->passphrase_len);
	}

	/*GROUP*/
	if (setting->group_count) {
		tlv = (struct tlv_data *)pos;
		tlv->type = VENDOR_SAE_GROUP_ID - 1;
		tlv->len = setting->group_count;
		pos = tlv->data;
		for (index = 0; index < setting->group_count; index++) {
			*pos = (unsigned char)(setting->groups[index]);
			pos++;
		}
		data_len += (header_len + setting->group_count);
	}

	/* ACT */
	if (setting->act != -1) {
		tlv = (struct tlv_data *)pos;
		tlv->type = VENDOR_SAE_ACT - 1;
		tlv->len = sizeof(u32);
		d = (u32 *)tlv->data;
		*d = (setting->act);
		pos += header_len + sizeof(u32);
		data_len += header_len + sizeof(u32);
	}

	/* End */
	tlv = (struct tlv_data *)pos;
	tlv->type = VENDOR_SAE_END;
	tlv->len = 0;
	data_len += header_len;

	len = sizeof(*param) + data_len;
	netdev_info(vif->ndev, "total len is : %d\n", data_len);

	msg = get_cmdbuf(vif->priv, vif, len, CMD_SET_SAE_PARAM);
	if (!msg) {
		kfree(data);
		return -ENOMEM;
	}

	param = (struct sae_param *)msg->data;
	param->request_type = SPRD_SAE_PASSWORD_ENTRY;
	memcpy(param->data, data, data_len);

	ret = send_cmd_recv_rsp(vif->priv, msg, NULL, NULL);
	if (ret)
		netdev_info(vif->ndev, "set sae para failed, ret=%d\n", ret);

	kfree(data);
	return ret;
}

static int vendor_set_sae_password(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data, int len)
{
	int group_index = 0, sae_entry_index = 0, rem_len, type;
	struct nlattr *pos;
	struct softap_sae_setting sae_para;
	struct sprd_vif *vif = netdev_priv(wdev->netdev);
	struct sprd_priv *priv;

	priv = wiphy_priv(wiphy);
	if (!(priv->extend_feature & SPRD_EXTEND_SOATAP_WPA3)) {
		netdev_info(vif->ndev, "firmware not support softap wpa3\n");
		return -ENOTSUPP;
	}

	memset(&sae_para, 0x00, sizeof(sae_para));

	nla_for_each_attr(pos, (void *)data, len, rem_len) {
		type = nla_type(pos);
		netdev_info(vif->ndev, "type is : %d\n", type);
		switch (type) {
		case VENDOR_SAE_ENTRY:
			sae_para.entry[sae_entry_index].vlan_id =
			    SPRD_SAE_NOT_SET;
			sae_para.entry[sae_entry_index].used = 1;
			vendor_parse_sae_entry(vif->ndev,
					       &sae_para.entry[sae_entry_index],
					       nla_data(pos), nla_len(pos));
			sae_entry_index++;
			if (sae_entry_index >= SPRD_SAE_MAX_NUM)
				return -EINVAL;
			break;
		case VENDOR_SAE_GROUP_ID:
			if (sae_para.group_count >= 31)
				return 0;
			sae_para.groups[group_index] = nla_get_u32(pos);
			group_index++;
			break;

		case VENDOR_SAE_ACT:
			sae_para.act = nla_get_u32(pos);
			break;

		case VENDOR_SAE_PWD:
			sae_para.passphrase_len = nla_len(pos);
			nla_strlcpy(sae_para.passphrase, pos,
				    sae_para.passphrase_len + 1);
			netdev_info(vif->ndev, "pwd is :%s, len :%d\n",
				    sae_para.passphrase, sae_para.passphrase_len);
			break;
		default:
			break;
		}
	}
	vendor_softap_set_sae_para(vif, &sae_para);
	return 0;
}

static int vendor_ftm_get_capabilities(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data, int len)
{
	/* Marlin2 not support RTT */
	return VENDOR_WIFI_ERROR_NOT_SUPPORTED;
}

static void vendor_report_gscan_result(struct sprd_vif *vif, u32 report_event,
				       u8 bucket_id, u16 chan, s16 rssi,
				       const u8 *frame, u16 len)
{
	struct sprd_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)frame;
	struct ieee80211_channel *channel;
	struct gscan_result *gscan_res = NULL;
	u16 band, capability, beacon_interval;
	u32 freq;
	s32 signal;
	u64 tsf;
	u8 *ie;
	size_t ielen;
	const u8 *ssid;

	band = sprd_channel_to_band(chan);
	freq = ieee80211_channel_to_frequency(chan, band);
	channel = ieee80211_get_channel(wiphy, freq);
	if (!channel) {
		netdev_err(vif->ndev, "%s invalid freq!\n", __func__);
		return;
	}
	signal = rssi;
	if (!mgmt) {
		netdev_err(vif->ndev, "%s NULL frame!\n", __func__);
		return;
	}
	ie = mgmt->u.probe_resp.variable;
	if (IS_ERR_OR_NULL(ie)) {
		netdev_err(vif->ndev, "%s Invalid IE in frame!\n", __func__);
		return;
	}
	ielen = len - offsetof(struct ieee80211_mgmt, u.probe_resp.variable);
	tsf = jiffies;
	beacon_interval = le16_to_cpu(mgmt->u.probe_resp.beacon_int);
	capability = le16_to_cpu(mgmt->u.probe_resp.capab_info);
	netdev_info(vif->ndev, "   %s, %pM, channel %2u, signal %d\n",
		    ieee80211_is_probe_resp(mgmt->frame_control)
		    ? "proberesp" : "beacon   ", mgmt->bssid, chan, rssi);

	gscan_res = kmalloc(sizeof(*gscan_res) + ielen, GFP_KERNEL);
	if (!gscan_res)
		return;
	memset(gscan_res, 0x0, sizeof(struct gscan_result) + ielen);
	gscan_res->channel = freq;
	gscan_res->beacon_period = beacon_interval;
	gscan_res->ts = tsf;
	gscan_res->rssi = signal;
	gscan_res->ie_length = ielen;
	memcpy(gscan_res->bssid, mgmt->bssid, 6);
	memcpy(gscan_res->ie_data, ie, ielen);

	ssid = vendor_wpa_scan_get_ie(ie, ielen, WLAN_EID_SSID);
	if (!ssid) {
		netdev_err(vif->ndev, "%s BSS: No SSID IE included for %pM!\n",
			   __func__, mgmt->bssid);
		goto out;
	}
	if (ssid[1] > 32) {
		netdev_err(vif->ndev, "%s BSS: Too long SSID IE for %pM!\n",
			   __func__, mgmt->bssid);
		goto out;
	}
	memcpy(gscan_res->ssid, ssid + 2, ssid[1]);
	netdev_err(vif->ndev, "%s %pM : %s !\n", __func__,
		   mgmt->bssid, gscan_res->ssid);

	vendor_cached_scan_result(vif, bucket_id, gscan_res);
	if (report_event & REPORT_EVENTS_FULL_RESULTS)
		vendor_report_full_scan(vif, gscan_res);
out:
	kfree(gscan_res);
}

static int vendor_report_available_event(struct sprd_vif *vif)
{
	struct sprd_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	struct sk_buff *reply;
	struct sprd_gscan_cached_results *p = NULL;
	int ret = 0, payload, rlen, i, num = 0;
	unsigned char *tmp;

	rlen = sizeof(enum vendor_gscan_event) + sizeof(u32);
	payload = rlen + 0x100;
	reply = cfg80211_vendor_event_alloc(wiphy, &vif->wdev, payload, 1,
					    GFP_KERNEL);
	if (!reply) {
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < priv->gscan_buckets_num; i++) {
		p = priv->gscan_res + i;
		num += p->num_results;
	}
	netdev_info(vif->ndev, "%s num:%d!\n", __func__, num);
	tmp = skb_tail_pointer(reply);
	memcpy(tmp, &num, sizeof(num));
	skb_put(reply, sizeof(num));
	cfg80211_vendor_event(reply, GFP_KERNEL);
out:
	return ret;
}

static int vendor_report_buffer_full_event(struct sprd_vif *vif)
{
	struct sprd_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	struct sk_buff *reply;
	int payload, rlen, ret = 0;

	rlen = sizeof(enum vendor_gscan_event) + sizeof(u32);
	payload = rlen + 0x100;
	reply = cfg80211_vendor_event_alloc(wiphy, &vif->wdev, payload, 0,
					    GFP_KERNEL);
	if (!reply) {
		ret = -ENOMEM;
		goto out;
	}
	if (nla_put_u32(reply, NL80211_ATTR_VENDOR_DATA, VENDOR_GSCAN_EVT_BUFFER_FULL))
		goto out_put_fail;
	cfg80211_vendor_event(reply, GFP_KERNEL);
out:
	return ret;
out_put_fail:
	kfree_skb(reply);
	WARN_ON(1);
	return -EMSGSIZE;
}

void sc2332_report_gscan_frame_evt(struct sprd_vif *vif, u8 *data, u16 len)
{
	u32 report_event;
	u8 *pos = data;
	s32 avail_len = len;
	struct evt_mgmt_frame *frame;
	u16 buf_len;
	u8 *buf = NULL;
	u8 channel, type, bucket_id;

	report_event = *(u32 *)pos;
	avail_len -= sizeof(u32);
	pos += sizeof(u32);
	while (avail_len > 0) {
		if (avail_len < sizeof(struct evt_mgmt_frame)) {
			netdev_err(vif->ndev,
				   "%s invalid available length: %d!\n",
				   __func__, avail_len);
			break;
		}
		frame = (struct evt_mgmt_frame *)pos;
		channel = frame->channel;
		type = frame->type;
		bucket_id = frame->reserved;
		buf = frame->data;
		buf_len = SPRD_GET_LE16(frame->len);
		sprd_dump_frame_prot_info(0, 0, buf, buf_len);
		vendor_report_gscan_result(vif, report_event, bucket_id,
					   channel, frame->signal, buf,
					   buf_len);
		avail_len -= sizeof(struct evt_mgmt_frame) + buf_len;
		pos += sizeof(struct evt_mgmt_frame) + buf_len;
		netdev_info(vif->ndev, "%s ch:%d ty:%d id:%d len:%d aval:%d\n",
			    __func__, channel, type, bucket_id, buf_len,
			    avail_len);
	}
	if (report_event & REPORT_EVENTS_EACH_SCAN)
		vendor_report_available_event(vif);

	if (report_event == REPORT_EVENTS_BUFFER_FULL)
		vendor_report_buffer_full_event(vif);
}

int sc2332_gscan_done(struct sprd_vif *vif, u8 bucketid)
{
	struct sprd_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	struct sk_buff *reply;
	int payload, rlen, ret = 0;
	int value;
	unsigned char *tmp;

	rlen = sizeof(enum vendor_gscan_event);
	payload = rlen + 0x100;
	reply = cfg80211_vendor_event_alloc(wiphy, &vif->wdev, payload, 0,
					    GFP_KERNEL);
	if (!reply) {
		ret = -ENOMEM;
		goto out;
	}
	value = VENDOR_GSCAN_EVT_COMPLETE;
	tmp = skb_tail_pointer(reply);
	memcpy(tmp, &value, sizeof(value));
	skb_put(reply, sizeof(value));
	cfg80211_vendor_event(reply, GFP_KERNEL);
out:
	return ret;
}

int sc2332_report_acs_lte_event(struct sprd_vif *vif)
{
	struct sprd_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	struct sk_buff *reply;
	int payload = 4;

	reply = cfg80211_vendor_event_alloc(wiphy, &vif->wdev, payload,
					    SPRD_ACS_LTE_EVENT_INDEX,
					    GFP_KERNEL);
	if (!reply)
		return -ENOMEM;

	cfg80211_vendor_event(reply, GFP_KERNEL);
	return 0;
}

static const struct wiphy_vendor_command vendor_cmd[] = {
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = VENDOR_SET_COUNTRY_CODE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_set_country,
		.policy = VENDOR_CMD_RAW_DATA,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = VENDOR_SET_LLSTAT
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_set_llstat_handler,
		.policy = ll_stats_policy,
		.maxattr = ATTR_LL_STATS_SET_MAX,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = VENDOR_GET_LLSTAT
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_get_llstat_handler,
		.policy = ll_stats_get_policy,
		.maxattr = ATTR_CMD_LL_STATS_TYPE_MAX,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = VENDOR_CLR_LLSTAT
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_clr_llstat_handler,
		.policy = ll_stats_clr_policy,
		.maxattr = ATTR_LL_STATS_CLR_MAX,
	},
	{
		{
		.vendor_id = OUI_SPREAD,
		.subcmd = VENDOR_CMD_GSCAN_GET_CAPABILITIES,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_get_gscan_capabilities,
		.policy = wlan_gscan_config_policy,
		.maxattr = GSCAN_ATTR_CONFIG_MAX,
	},
	{
		{
		.vendor_id = OUI_SPREAD,
		.subcmd = VENDOR_CMD_GSCAN_GET_CHANNEL,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_get_channel_list,
		.policy = wlan_gscan_config_policy,
		.maxattr = GSCAN_ATTR_CONFIG_MAX,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = VENDOR_CMD_GSCAN_START,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_gscan_start,
		.policy = wlan_gscan_config_policy,
		.maxattr = GSCAN_ATTR_CONFIG_MAX,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = VENDOR_CMD_GSCAN_STOP,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_gscan_stop,
		.policy = wlan_gscan_config_policy,
		.maxattr = GSCAN_ATTR_CONFIG_MAX,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = VENDOR_CMD_GSCAN_GET_CACHED_RESULTS,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_get_cached_gscan_results,
		.policy = wlan_gscan_config_policy,
		.maxattr = GSCAN_ATTR_CONFIG_MAX,
	},

			{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = VENDOR_CMD_GET_LOGGER_FEATURE_SET,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_get_logger_feature,
		.policy = get_logger_features_policy,
		.maxattr = ATTR_LOGGER_MAX,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = VENDOR_CMD_GET_WAKE_REASON_STATS,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_get_wake_state,
		.policy = wake_stats_policy,
		.maxattr = ATTR_WAKE_MAX,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = VENDOR_CMD_GET_SUPPORT_FEATURE,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_get_feature,
		.policy = VENDOR_CMD_RAW_DATA,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = VENDOR_CMD_ENABLE_ND_OFFLOAD,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_enable_nd_offload,
		.policy = nd_offload_policy,
		.maxattr = ATTR_ND_OFFLOAD_MAX,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = VENDOR_CMD_SET_MAC_OUI,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_set_mac_oui,
		.policy = mac_oui_policy,
		.maxattr = ATTR_SET_SCANNING_MAC_OUI_MAX,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = VENDOR_CMD_START_LOGGING,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_start_logging,
		.policy = wifi_logger_start_policy,
		.maxattr = ATTR_WIFI_LOGGER_START_GET_MAX,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = VENDOR_CMD_GET_WIFI_INFO,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_get_driver_info,
		.policy = get_wifi_info_policy,
		.maxattr = ATTR_WIFI_INFO_GET_MAX,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = VENDOR_CMD_GET_RING_DATA,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_get_ring_data,
		.policy = wifi_logger_start_policy,
		.maxattr = ATTR_WIFI_LOGGER_START_GET_MAX,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = VENDOR_CMD_WIFI_LOGGER_MEMORY_DUMP,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = vendor_memory_dump,
		.policy = wifi_logger_start_policy,
		.maxattr = ATTR_WIFI_LOGGER_START_GET_MAX,
	},
	{
		{
		    .vendor_id = OUI_SPREAD,
		    .subcmd = VENDOR_CMD_RTT_GET_CAPA,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = vendor_ftm_get_capabilities,
		.policy = VENDOR_CMD_RAW_DATA,
	},
	{
		{
		    .vendor_id = OUI_SPREAD,
		    .subcmd = VENDOR_CMD_GET_AKM_SUITE,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = vendor_get_akm_suite,
		.policy = VENDOR_CMD_RAW_DATA,
	},
	{
		{
		    .vendor_id = OUI_SPREAD,
		    .subcmd = VENDOR_CMD_OFFLOADED_PACKETS,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = vendor_set_offload_packet,
		.policy = offloaded_packets_policy,
		.maxattr = ATTR_OFFLOADED_PACKETS_MAX,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = VENDOR_CMD_SET_SAE_PASSWORD,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = vendor_set_sae_password,
		.policy = vendor_sae_policy,
		.maxattr = VENDOR_SAE_MAX,
	},
};

static const struct nl80211_vendor_cmd_info vendor_events[] = {
	[VENDOR_SUBCMD_GSCAN_START_INDEX] = {
		.vendor_id = OUI_SPREAD,
		.subcmd = VENDOR_CMD_GSCAN_START
	},
	[VENDOR_SUBCMD_GSCAN_STOP_INDEX] = {
		.vendor_id = OUI_SPREAD,
		.subcmd = VENDOR_CMD_GSCAN_STOP
	},
	[VENDOR_SUBCMD_GSCAN_GET_CAPABILITIES_INDEX] = {
		.vendor_id = OUI_SPREAD,
		.subcmd = VENDOR_CMD_GSCAN_GET_CAPABILITIES
	},
	[VENDOR_SUBCMD_GSCAN_GET_CACHED_RESULTS_INDEX] = {
		.vendor_id = OUI_SPREAD,
		.subcmd = VENDOR_CMD_GSCAN_GET_CACHED_RESULTS
	},
	[VENDOR_SUBCMD_GSCAN_SCAN_RESULTS_AVAILABLE_INDEX] = {
		.vendor_id = OUI_SPREAD,
		.subcmd = VENDOR_CMD_GSCAN_SCAN_RESULTS_AVAILABLE
	},
	[VENDOR_SUBCMD_GSCAN_FULL_SCAN_RESULT_INDEX] = {
		.vendor_id = OUI_SPREAD,
		.subcmd = VENDOR_CMD_GSCAN_FULL_SCAN_RESULT
	},
	[VENDOR_SUBCMD_GSCAN_SCAN_EVENT_INDEX] = {
		.vendor_id = OUI_SPREAD,
		.subcmd = VENDOR_CMD_GSCAN_SCAN_EVENT
	},
	[VENDOR_SUBCMD_GSCAN_HOTLIST_AP_FOUND_INDEX] = {
		.vendor_id = OUI_SPREAD,
		.subcmd = VENDOR_CMD_GSCAN_HOTLIST_AP_FOUND
	},
	[VENDOR_SUBCMD_GSCAN_SET_BSSID_HOTLIST_INDEX] = {
		.vendor_id = OUI_SPREAD,
		.subcmd = VENDOR_CMD_GSCAN_SET_BSSID_HOTLIST
	},
	[VENDOR_SUBCMD_GSCAN_RESET_BSSID_HOTLIST_INDEX] = {
		.vendor_id = OUI_SPREAD,
		.subcmd = VENDOR_CMD_GSCAN_RESET_BSSID_HOTLIST
	},
	[VENDOR_SUBCMD_GSCAN_SIGNIFICANT_CHANGE_INDEX] = {
		.vendor_id = OUI_SPREAD,
		.subcmd = VENDOR_CMD_GSCAN_SIGNIFICANT_CHANGE
	},
	[VENDOR_SUBCMD_GSCAN_SET_SIGNIFICANT_CHANGE_INDEX] = {
		.vendor_id = OUI_SPREAD,
		.subcmd = VENDOR_CMD_GSCAN_SET_SIGNIFICANT_CHANGE
	},
	[VENDOR_SUBCMD_GSCAN_RESET_SIGNIFICANT_CHANGE_INDEX] = {
		.vendor_id = OUI_SPREAD,
		.subcmd = VENDOR_CMD_GSCAN_RESET_SIGNIFICANT_CHANGE
	},
	/* report acs lte event to uplayer */
	[SPRD_ACS_LTE_EVENT_INDEX] = {
		.vendor_id = OUI_SPREAD,
		.subcmd = SPRD_REINIT_ACS,
	},
};

int sc2332_vendor_init(struct wiphy *wiphy)
{
	int buf_size;
	struct sprd_priv *priv = wiphy_priv(wiphy);

	wiphy->vendor_commands = vendor_cmd;
	wiphy->n_vendor_commands = ARRAY_SIZE(vendor_cmd);
	wiphy->vendor_events = vendor_events;
	wiphy->n_vendor_events = ARRAY_SIZE(vendor_events);
	buf_size = MAX_BUCKETS * sizeof(struct sprd_gscan_cached_results);

	priv->gscan_res = kmalloc(buf_size, GFP_KERNEL);

	if (!priv->gscan_res)
		return -ENOMEM;

	memset(priv->gscan_res, 0, sizeof(buf_size));
	return 0;
}

int sc2332_vendor_deinit(struct wiphy *wiphy)
{
	struct sprd_priv *priv = wiphy_priv(wiphy);

	wiphy->vendor_commands = NULL;
	wiphy->n_vendor_commands = 0;
	kfree(priv->gscan_res);
	return 0;
}
