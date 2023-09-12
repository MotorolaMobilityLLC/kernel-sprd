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
#include "common/channel_2g.h"
#include "common/chip_ops.h"
#include "common/common.h"
#include "qos.h"
#include "txrx.h"
#include "fcc.h"

#define SPRD_MAX_PFN_LIST_COUNT		16
#define SPRD_MAX_SCHED_PLANS		1
#define SPRD_MAX_SCHED_INTERVAL		1000	/* seconds */
#define SPRD_MAX_SCHED_ITERATION	0	/* inifinities */
#define SPRD_MAX_REMAIN_ON_DURATION_MS	5000

#define SPRD_G_RATE_NUM			28
#define G_RATES				(sprd_rates)
#define SPRD_A_RATE_NUM			24
#define A_RATES				(sprd_rates + 4)

#define SPRD_BAND_2G			(&sc2332_band_2ghz)
#define SPRD_BAND_5G			NULL
#define G_HTCAP \
{ \
	.cap = IEEE80211_HT_CAP_SM_PS | \
	       IEEE80211_HT_CAP_SGI_20, \
	.ht_supported = true, \
	.mcs = { \
		.rx_mask = { 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, }, \
		.rx_highest = cpu_to_le16(0), \
	}, \
	.ampdu_factor   = IEEE80211_HT_MAX_AMPDU_64K, \
	.ampdu_density  = IEEE80211_HT_MPDU_DENSITY_8, \
}

static struct ieee80211_supported_band sc2332_band_2ghz = {
	.n_channels = ARRAY_SIZE(sprd_2ghz_channels),
	.channels = sprd_2ghz_channels,
	.n_bitrates = SPRD_G_RATE_NUM,
	.bitrates = G_RATES,
	.ht_cap = G_HTCAP,
};

static void sc2332_reg_notify(struct wiphy *wiphy,
			      struct regulatory_request *request)
{
	struct sprd_priv *priv = wiphy_priv(wiphy);
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *chan;
	const struct ieee80211_freq_range *freq_range;
	const struct ieee80211_reg_rule *reg_rule;
	struct sprd_ieee80211_regdomain *rd = NULL;
	u32 band, channel, i;
	u32 last_start_freq;
	u32 n_rules = 0, rd_size;

	wiphy_info(wiphy, "%s %c%c initiator %d hint_type %d\n", __func__,
		   request->alpha2[0], request->alpha2[1],
		   request->initiator, request->user_reg_hint_type);

	memset(&priv->ch_2g4_info, 0, sizeof(struct sprd_channel_list));
	memset(&priv->ch_5g_without_dfs_info, 0,
	       sizeof(struct sprd_channel_list));
	memset(&priv->ch_5g_dfs_info, 0, sizeof(struct sprd_channel_list));

	/* Figure out the actual rule number */
	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		sband = wiphy->bands[band];
		if (!sband)
			continue;

		last_start_freq = 0;
		for (channel = 0; channel < sband->n_channels; channel++) {
			chan = &sband->channels[channel];

			reg_rule =
			    freq_reg_info(wiphy, MHZ_TO_KHZ(chan->center_freq));
			if (IS_ERR(reg_rule))
				continue;

			freq_range = &reg_rule->freq_range;
			if (last_start_freq != freq_range->start_freq_khz) {
				last_start_freq = freq_range->start_freq_khz;
				n_rules++;
			}
		}
	}

	rd_size = sizeof(struct sprd_ieee80211_regdomain) +
	    n_rules * sizeof(struct sprd_reg_rule);

	rd = kzalloc(rd_size, GFP_KERNEL);
	if (!rd)
		return;

	/* Fill regulatory domain */
	rd->n_reg_rules = n_rules;
	memcpy(rd->alpha2, request->alpha2, ARRAY_SIZE(rd->alpha2));
	for (band = 0, i = 0; band < NUM_NL80211_BANDS; band++) {
		sband = wiphy->bands[band];
		if (!sband)
			continue;

		last_start_freq = 0;
		for (channel = 0; channel < sband->n_channels; channel++) {
			chan = &sband->channels[channel];

			if (chan->flags & IEEE80211_CHAN_DISABLED)
				continue;

			reg_rule =
			    freq_reg_info(wiphy, MHZ_TO_KHZ(chan->center_freq));
			if (IS_ERR(reg_rule))
				continue;

			freq_range = &reg_rule->freq_range;
			if (last_start_freq != freq_range->start_freq_khz &&
			    i < n_rules) {
				last_start_freq = freq_range->start_freq_khz;
				memcpy(&rd->reg_rules[i].freq_range,
				       &reg_rule->freq_range,
				       sizeof(struct ieee80211_freq_range));
				memcpy(&rd->reg_rules[i].power_rule,
				       &reg_rule->power_rule,
				       sizeof(struct ieee80211_power_rule));
				rd->reg_rules[i].flags = reg_rule->flags;
				rd->reg_rules[i].dfs_cac_ms =
				    reg_rule->dfs_cac_ms;
				i++;

				wiphy_dbg(wiphy,
					  "   %d KHz - %d KHz @ %d KHz flags %#x\n",
					  freq_range->start_freq_khz,
					  freq_range->end_freq_khz,
					  freq_range->max_bandwidth_khz,
					  reg_rule->flags);
			}
		}
	}

	if (sc2332_set_regdom(priv, (u8 *)rd, rd_size))
		wiphy_err(wiphy, "%s failed to set regdomain!\n", __func__);

	sc2332_fcc_match_country(priv, request->alpha2);

	kfree(rd);
}

void sc2332_setup_wiphy(struct wiphy *wiphy, struct sprd_priv *priv)
{
	wiphy->mgmt_stypes = sprd_mgmt_stypes;
	wiphy->interface_modes =
	    BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP) |
	    BIT(NL80211_IFTYPE_P2P_GO) | BIT(NL80211_IFTYPE_P2P_CLIENT) |
	    BIT(NL80211_IFTYPE_P2P_DEVICE);

	wiphy->features |= NL80211_FEATURE_CELL_BASE_REG_HINTS;
	wiphy->flags &= ~WIPHY_FLAG_PS_ON_BY_DEFAULT;
	wiphy->flags |= WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL;
	wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;
	wiphy->max_scan_ssids = SPRD_MAX_SCAN_SSIDS;
	wiphy->max_scan_ie_len = SPRD_MAX_SCAN_IE_LEN;
	wiphy->cipher_suites = sprd_cipher_suites;
	wiphy->n_cipher_suites = ARRAY_SIZE(sprd_cipher_suites);
	wiphy->bands[NL80211_BAND_2GHZ] = SPRD_BAND_2G;
	wiphy->max_ap_assoc_sta = priv->max_ap_assoc_sta;
#ifdef CONFIG_PM
	/* Set WoWLAN flags */
	wiphy->wowlan = &sprd_wowlan_support;
#endif
	wiphy->max_remain_on_channel_duration = SPRD_MAX_REMAIN_ON_DURATION_MS;
	wiphy->max_num_pmkids = SPRD_MAX_NUM_PMKIDS;

	if (priv->fw_std & SPRD_STD_11D) {
		pr_info("\tIEEE802.11d supported\n");
		wiphy->reg_notifier = sc2332_reg_notify;
	}

	if (priv->fw_std & SPRD_STD_11E) {
		pr_info("\tIEEE802.11e supported\n");
		wiphy->features |= NL80211_FEATURE_SUPPORTS_WMM_ADMISSION;
		wiphy->flags |= WIPHY_FLAG_AP_UAPSD;
	}

	if (priv->fw_std & SPRD_STD_11K)
		pr_info("\tIEEE802.11k supported\n");

	if (priv->fw_std & SPRD_STD_11R)
		pr_info("\tIEEE802.11r supported\n");

	if (priv->fw_std & SPRD_STD_11U)
		pr_info("\tIEEE802.11u supported\n");

	if (priv->fw_std & SPRD_STD_11V)
		pr_info("\tIEEE802.11v supported\n");

	if (priv->fw_std & SPRD_STD_11W)
		pr_info("\tIEEE802.11w supported\n");

	if (priv->fw_capa & SPRD_CAPA_5G) {
		pr_info("\tDual band supported\n");
		wiphy->bands[NL80211_BAND_5GHZ] = SPRD_BAND_5G;
	}

	if (priv->fw_capa & SPRD_CAPA_MCC) {
		pr_info("\tMCC supported\n");
		wiphy->n_iface_combinations = ARRAY_SIZE(sprd_iface_combos);
		wiphy->iface_combinations = sprd_iface_combos;
	} else {
		pr_info("\tSCC supported\n");
		wiphy->software_iftypes =
		    BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP) |
		    BIT(NL80211_IFTYPE_P2P_CLIENT) |
		    BIT(NL80211_IFTYPE_P2P_GO) | BIT(NL80211_IFTYPE_P2P_DEVICE);
	}

	if (priv->fw_capa & SPRD_CAPA_ACL) {
		pr_info("\tACL supported (%d)\n", priv->max_acl_mac_addrs);
		wiphy->max_acl_mac_addrs = priv->max_acl_mac_addrs;
	}

	if (priv->fw_capa & SPRD_CAPA_AP_SME) {
		pr_info("\tAP SME enabled\n");
		wiphy->flags |= WIPHY_FLAG_HAVE_AP_SME;
		wiphy->ap_sme_capa = 1;
	}

	if (priv->fw_capa & SPRD_CAPA_PMK_OKC_OFFLOAD &&
	    priv->fw_capa & SPRD_CAPA_11R_ROAM_OFFLOAD) {
		pr_info("\tRoaming offload supported\n");
		wiphy->flags |= WIPHY_FLAG_SUPPORTS_FW_ROAM;
	}

	/* Random MAC addr is enabled by default. And needs to be
	 * disabled to pass WFA Certification.
	 */
	if (priv->fw_capa & SPRD_CAPA_SCAN_RANDOM_MAC_ADDR &&
	    (!(wfa_cap & SPRD_WFA_CAP_NON_RAN_MAC))) {
		pr_info("\tRandom MAC address scan default supported\n");
		wiphy->features |= NL80211_FEATURE_SCAN_RANDOM_MAC_ADDR;
	}

	if (priv->fw_capa & SPRD_CAPA_SCHED_SCAN) {
		pr_info("\tScheduled scan supported\n");
		wiphy->max_sched_scan_reqs = 1;
		if (priv->random_mac_support)
			wiphy->features |=
			    NL80211_FEATURE_SCHED_SCAN_RANDOM_MAC_ADDR;

		wiphy->max_sched_scan_ssids = SPRD_MAX_PFN_LIST_COUNT;
		wiphy->max_match_sets = SPRD_MAX_PFN_LIST_COUNT;
		wiphy->max_sched_scan_ie_len = SPRD_MAX_SCAN_IE_LEN;
		wiphy->max_sched_scan_plans = SPRD_MAX_SCHED_PLANS;
		wiphy->max_sched_scan_plan_interval = SPRD_MAX_SCHED_INTERVAL;
		wiphy->max_sched_scan_plan_iterations =
		    SPRD_MAX_SCHED_ITERATION;
	}

	if (priv->fw_capa & SPRD_CAPA_TDLS) {
		pr_info("\tTDLS supported\n");
		wiphy->flags |= WIPHY_FLAG_SUPPORTS_TDLS;
		wiphy->flags |= WIPHY_FLAG_TDLS_EXTERNAL_SETUP;
		wiphy->features |= NL80211_FEATURE_TDLS_CHANNEL_SWITCH;
	}

	if (priv->fw_capa & SPRD_CAPA_LL_STATS)
		pr_info("\tLink layer stats supported\n");

	wiphy->features |= NL80211_FEATURE_SAE;

	if (priv->extend_feature & SPRD_EXTEND_FEATURE_OCE) {
		pr_info("\tOCE supported\n");
		wiphy_ext_feature_set(wiphy,
				      NL80211_EXT_FEATURE_ACCEPT_BCAST_PROBE_RESP);
		wiphy_ext_feature_set(wiphy,
				      NL80211_EXT_FEATURE_FILS_MAX_CHANNEL_TIME);
		wiphy_ext_feature_set(wiphy,
				      NL80211_EXT_FEATURE_OCE_PROBE_REQ_DEFERRAL_SUPPRESSION);
		wiphy_ext_feature_set(wiphy,
				      NL80211_EXT_FEATURE_OCE_PROBE_REQ_HIGH_TX_RATE);
	}
}

struct sprd_chip_ops sc2332_chip_ops = {
	.get_msg = sc2332_tx_get_msg,
	.free_msg = sc2332_tx_free_msg,
	.tx = sc2332_tx,
	.force_exit = sc2332_tx_force_exit,
	.is_exit = sc2332_tx_is_exit,
	.drop_tcp_msg = sc2332_tx_drop_tcp_msg,
	.set_qos = sc2332_tx_set_qos,

	.setup_wiphy = sc2332_setup_wiphy,
	.cmd_init = sc2332_cmd_init,
	.cmd_deinit = sc2332_cmd_deinit,
	.get_fw_info = sc2332_get_fw_info,
	.open_fw = sc2332_open_fw,
	.close_fw = sc2332_close_fw,
	.power_save = sc2332_power_save,
	.set_sar = sc2332_set_sar,
	.add_key = sc2332_add_key,
	.del_key = sc2332_del_key,
	.set_def_key = sc2332_set_def_key,
	.set_beacon_ie = sc2332_set_beacon_ie,
	.set_probereq_ie = sc2332_set_probereq_ie,
	.set_proberesp_ie = sc2332_set_proberesp_ie,
	.set_assocreq_ie = sc2332_set_assocreq_ie,
	.set_assocresp_ie = sc2332_set_assocresp_ie,
	.set_sae_ie = sc2332_set_sae_ie,
	.start_ap = sc2332_start_ap,
	.del_station = sc2332_del_station,
	.get_station = sc2332_get_station,
	.set_channel = sc2332_set_channel,
	.connect = sc2332_connect,
	.disconnect = sc2332_disconnect,
	.pmksa = sc2332_pmksa,
	.remain_chan = sc2332_remain_chan,
	.cancel_remain_chan = sc2332_cancel_remain_chan,
	.tx_mgmt = sc2332_tx_mgmt,
	.register_frame = sc2332_register_frame,
	.tdls_mgmt = sc2332_tdls_mgmt,
	.tdls_oper = sc2332_tdls_oper,
	.start_tdls_channel_switch = sc2332_start_tdls_channel_switch,
	.cancel_tdls_channel_switch = sc2332_cancel_tdls_channel_switch,
	.set_cqm_rssi = sc2332_set_cqm_rssi,
	.set_roam_offload = sc2332_set_roam_offload,
	.set_blacklist = sc2332_set_blacklist,
	.set_whitelist = sc2332_set_whitelist,
	.set_param = sc2332_set_param,
	.set_qos_map = sc2332_set_qos_map,
	.add_tx_ts = sc2332_add_tx_ts,
	.del_tx_ts = sc2332_del_tx_ts,
	.set_mc_filter = sc2332_set_mc_filter,
	.set_11v_feature_support = sc2332_set_11v_feature_support,
	.set_11v_sleep_mode = sc2332_set_11v_sleep_mode,
	.xmit_data2cmd = sc2332_xmit_data2cmd,
	.set_random_mac = sc2332_set_random_mac,
	.do_delay_work = sc2332_do_delay_work,
	.notify_ip = sc2332_notify_ip,
	.set_vowifi = sc2332_set_vowifi,
	.dump_survey = sc2332_dump_survey,
	.npi_send_recv = sc2332_npi_send_recv,
	.scan = sc2332_scan,
	.sched_scan_start = sc2332_sched_scan_start,
	.sched_scan_stop = sc2332_sched_scan_stop,
	.scan_timeout = sc2332_scan_timeout,
	.tcp_ack_init = sc2332_tcp_ack_init,
	.tcp_ack_deinit = sc2332_tcp_ack_deinit,
#ifdef CONFIG_SPRD_WLAN_VENDOR_SPECIFIC
	.vendor_init = sc2332_vendor_init,
	.vendor_deinit = sc2332_vendor_deinit,
#endif /* CONFIG_SPRD_WLAN_VENDOR_SPECIFIC */
	.send_data = sc2332_send_data,
	.send_data_offset = sc2332_send_data_offset,
	.set_sniffer = sc2332_set_sniffer,
};

MODULE_DESCRIPTION("Spreadtrum SC2332 WLAN Driver");
MODULE_AUTHOR("Spreadtrum WCN Division");
MODULE_LICENSE("GPL");
