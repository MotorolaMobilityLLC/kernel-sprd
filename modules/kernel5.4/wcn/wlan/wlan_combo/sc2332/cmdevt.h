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

#ifndef __CMDEVT_H__
#define __CMDEVT_H__

#include <linux/math64.h>
#include "common/cmd.h"
#include "common/delay_work.h"
#include "common/msg.h"

/* Set scan timeout to 6s due to split scan
 * to several period in CP2
 */
#define CMD_SCAN_WAIT_TIMEOUT			(4000)
#define SPRD_MAX_SDIO_SEND_COUT			1024
#define SPRD_SCHED_SCAN_BUF_END			BIT(0)

#define SPRD_SEND_FLAG_IFRC			BIT(0)
#define SPRD_SEND_FLAG_SSID			BIT(1)
#define SPRD_SEND_FLAG_MSSID			BIT(2)
#define SPRD_SEND_FLAG_IE			BIT(4)

/* wnm feature */
#define SPRD_11V_BTM				BIT(0)
#define SPRD_11V_PARP				BIT(1)
#define SPRD_11V_MIPM				BIT(2)
#define SPRD_11V_DMS				BIT(3)
#define SPRD_11V_SLEEP				BIT(4)
#define SPRD_11V_TFS				BIT(5)
#define SPRD_11V_ALL_FEATURE			0xFFFF

#define SPRD_CAPA_SCAN_RANDOM_MAC_ADDR		BIT(12)
#define SPRD_CAPA_SCHED_SCAN_RANDOM_MAC_ADDR	BIT(13)
#define SPRD_EXTEND_FEATURE_SAE			BIT(0)
#define SPRD_EXTEND_FEATURE_OWE			BIT(1)
#define SPRD_EXTEND_FEATURE_DPP			BIT(2)
#define SPRD_EXTEND_8021X_SUITE_B_192		BIT(3)
#define SPRD_EXTEND_FEATURE_OCE			BIT(4)
#define SPRD_EXTEND_FEATURE_LLSTATE		BIT(5)
#define SPRD_EXTEND_SOATAP_WPA3			BIT(6)
#define SPRD_SET_SAR	0x05
#define SPRD_EARLY_RSP9_0			0x90
#define SPRD_IE_BEACON				0
#define SPRD_IE_PROBE_REQ			1
#define SPRD_IE_PROBE_RESP			2
#define SPRD_IE_ASSOC_REQ			3
#define SPRD_IE_ASSOC_RESP			4
#define SPRD_IE_P2P_REQ				5
#define SPRD_IE_BEACON_HEAD			6
#define SPRD_IE_BEACON_TAIL			7
#define SPRD_IE_SAE				8
#define SPRD_FLAGS_SCAN_RANDOM_ADDR		BIT(0)
#define SPRD_FRAME_NORMAL			1
#define SPRD_FRAME_DEAUTH			2
#define SPRD_FRAME_DISASSOC			3
#define SPRD_FRAME_SCAN				4
#define SPRD_FRAME_ROAMING			5
#define SPRD_SCAN_DONE				1
#define SPRD_SCHED_SCAN_DONE			2
#define SPRD_SCAN_ERROR				3
#define SPRD_GSCAN_DONE				4
#define SPRD_CQM_RSSI_LOW			1
#define SPRD_CQM_RSSI_HIGH			2
#define SPRD_CQM_BEACON_LOSS			3
#define SPRD_SAE_PASSPHRASE			1
#define SPRD_SAE_PASSWORD_ENTRY			2
#define SPRD_SCHED_SCAN_CHAN_LEN		16
#define SPRD_KEY_SEQ_LEN			8
#define SPRD_ACS_CHAN_NUM_MIN			3

#ifndef SPRD_MAX_CMD_TXLEN
#define SPRD_MAX_CMD_TXLEN			1396

#define CMD_SNIFFER_MODE                  "SNIFFER_MODE"
#define CMD_SNIFFER_LISTEN_CHANNEL        "LISTEN_CHANNEL"
#endif

enum CMD_LIST {
	CMD_MIN = 0,
	/* All Interface */
	CMD_GET_INFO = 1,
	CMD_SET_REGDOM,
	CMD_OPEN,
	CMD_CLOSE,
	CMD_POWER_SAVE,
	CMD_SET_PARAM,
	CMD_SET_CHANNEL,
	CMD_REQ_LTE_CONCUR,

	/* Connect */
	CMD_CONNECT = 10,

	/* Station */
	CMD_SCAN = 11,
	CMD_SCHED_SCAN,
	CMD_DISCONNECT,
	CMD_KEY,
	CMD_SET_PMKSA,
	CMD_GET_STATION,

	/* SoftAP */
	CMD_START_AP = 17,
	CMD_DEL_STATION,
	CMD_SET_BLACKLIST,
	CMD_SET_WHITELIST,

	/* P2P */
	CMD_TX_MGMT = 21,
	CMD_REGISTER_FRAME,
	CMD_REMAIN_CHAN,
	CMD_CANCEL_REMAIN_CHAN,

	/* Public/New Feature */
	CMD_SET_IE = 25,
	CMD_NOTIFY_IP_ACQUIRED,
	/* Roaming */
	CMD_SET_CQM,		/* Uplayer Roaming */
	CMD_SET_ROAM_OFFLOAD,	/* fw Roaming */
	CMD_SET_MEASUREMENT,
	CMD_SET_QOS_MAP,
	CMD_TDLS,
	CMD_11V,

	/* NPI/DEBUG/OTHER */
	CMD_NPI_MSG = 33,
	CMD_NPI_GET,

	CMD_ASSERT,
	CMD_FLUSH_SDIO,

	/* WMM Admisson Control */
	CMD_ADD_TX_TS = 37,
	CMD_DEL_TX_TS = 38,

	/* Multicast filter */
	CMD_MULTICAST_FILTER,
	CMD_LLSTAT = 56,

	/* gscan */
	CMD_GSCAN = 57,
	/* Marlin2 domain cmd */
	CMD_PRE_CLOSE = 58,
	CMD_SET_VOWIFI = 59,
	CMD_MIRACAST = 60,
	CMD_MAX_STA = 61,
	CMD_SET_SNIFFER = 62,
	CMD_RANDOM_MAC = 63,
	CMD_PACKET_OFFLOAD = 64,
	CMD_SET_SAE_PARAM = 65,
	CMD_EXTENDED_LLSTAT = 66,
	CMD_MAX
};

enum sar_mode {
	SPRD_SET_SAR_2G_11B = 0,
	SPRD_SET_SAR_2G_11G = 1,
	SPRD_SET_SAR_2G_11N = 2,
	SPRD_SET_SAR_ALL_MODE = 3,
};

enum SPRD_EXTERN_LLSTAT_SUBTYPE {
	SPRD_SUBTYPE_CHANNEL_INFO = 1,
};

/* CMD_GET_INFO
 * @SPRD_STD_11D:  The fw supports regulatory domain.
 * @SPRD_STD_11E:  The fw supports WMM/WMM-AC/WMM-PS.
 * @SPRD_STD_11K:  The fw supports Radio Resource Measurement.
 * @SPRD_STD_11R:  The fw supports FT roaming.
 * @SPRD_STD_11U:  The fw supports Interworking Network.
 * @SPRD_STD_11V:  The fw supports Wireless Network Management.
 * @SPRD_STD_11W:  The fw supports Protected Management Frame.
 *
 * @SPRD_CAPA_5G:  The fw supports dual band (2.4G/5G).
 * @SPRD_CAPA_MCC:  The fw supports Multi Channel Concurrency.
 * @SPRD_CAPA_ACL:  The fw supports ACL.
 * @SPRD_CAPA_AP_SME:  The fw integrates AP SME.
 * @SPRD_CAPA_PMK_OKC_OFFLOAD:  The fw supports PMK/OKC roaming offload.
 * @SPRD_CAPA_11R_ROAM_OFFLOAD:  The fw supports FT roaming offload.
 * @SPRD_CAPA_SCHED_SCAN:  The fw supports scheduled scans.
 * @SPRD_CAPA_TDLS:  The fw supports TDLS (802.11z) operation.
 * @SPRD_CAPA_MC_FILTER:  The fw supports multicast filter operation.
 * @SPRD_CAPA_NS_OFFLOAD:  The fw supports ipv6 NS operation.
 * @SPRD_CAPA_RA_OFFLOAD:  The fw supports ipv6 RA offload.
 * @SPRD_CAPA_LL_STATS:  The fw supports link layer stats.
 */
struct cmd_fw_info {
	__le32 chip_model;
	__le32 chip_version;
	__le32 fw_version;
	__le32 fw_std;
	__le32 fw_capa;
	u8 max_ap_assoc_sta;
	u8 max_acl_mac_addrs;
	u8 max_mc_mac_addrs;
	u8 wnm_ft_support;
	__le32 extend_feature;
} __packed;

/* CMD_OPEN */
struct cmd_open {
	u8 mode;
	u8 reserved;
	u8 mac[ETH_ALEN];
} __packed;

/* CMD_GET_FW_INFO */
/* start from andorid9.0 for resolving the wifi scan rsp problem
 * and compatibility issue
 */

struct cmd_get_fw_info {
	u8 early_rsp;
	u8 reserved[3];
} __packed;

/* CMD_CLOSE */
struct cmd_close {
	u8 mode;
} __packed;

struct cmd_power_save {
	u8 sub_type;
	u8 value;
} __packed;

/**
 * cmd_set_sar: this struct used to describe set sar parameters.
 *
 * @power_save_type:power save command type, we send sar para through
 *  power save command,so need provide power_save_sub_type,in this case
 *  this value always set to SPRD_SET_SAR, other sub type please ref
 *  sprdwl_cmd_power_save struct.
 * @sub_type: Please refer sar_subtype struct.
 * @value: The value we set.
 * @mode: 802.11mode, please refer sar_mode struct.
 */
struct cmd_set_sar {
	u8 power_save_type;
	u8 sub_type;
	s8 value;
	u8 mode;
} __packed;

/**
 * sprdwl_cmd_set_power_backoff: this struct used to describe set power bo parameters.
 *
 * @power_save_type:power save command type, we send sar para through
 *  power save command,so need provide power_save_sub_type,in this case
 *  this value always set to SPRDWL_SET_POWER_BACKOFF, other sub type please ref
 *  sprdwl_cmd_power_save struct.
 * @sub_type: Please refer sprdwl_sar_subtype struct.
 * @value: The value we set.
 * @mode: 802.11mode, please refer sprdwl_sar_mode struct.
 * @channel:channel num.
 * @bw: bandwidth info.
 */
struct cmd_set_power_backoff {
#define SPRD_SET_POWER_BACKOFF	0x07
	u8 power_save_type;
	u8 sub_type;
	s8 value;
	u8 mode;
	u8 channel;
} __packed;

struct cmd_vowifi {
	u8 value;
} __packed;

struct cmd_add_key {
	u8 key_index;
	u8 pairwise;
	u8 mac[ETH_ALEN];
	u8 keyseq[8];
	u8 cypher_type;
	u8 key_len;
	u8 value[0];
} __packed;

struct cmd_del_key {
	u8 key_index;
	u8 pairwise;		/* pairwise or group */
	u8 mac[ETH_ALEN];
} __packed;

struct cmd_set_def_key {
	u8 key_index;
} __packed;

/* CMD_SET_IE */
struct cmd_set_ie {
	u8 type;
	__le16 len;
	u8 data[0];
} __packed;

/* CMD_START_AP */
struct cmd_start_ap {
	__le16 len;
	u8 value[0];
} __packed;

/* CMD_DEL_STATION */
struct cmd_del_station {
	u8 mac[ETH_ALEN];
	__le16 reason_code;
} __packed;

/* CMD_GET_STATION */
struct cmd_get_station {
	u8 txrate;
	u8 signal;
	u8 noise;
	u8 reserved;
	__le32 txfailed;
	struct sprd_rate_info tx_rate;
	struct sprd_rate_info rx_rate;
} __packed;

/* CMD_SET_CHANNEL */
struct cmd_set_channel {
	u8 channel;
} __packed;

/* CMD_SCAN */
struct cmd_scan {
	__le32 channels;	/* One bit for one channel */
	__le32 flags;
	u16 ssid_len;
	u8 param[0];
} __packed;

/* CMD_SCHED_SCAN */
struct cmd_sched_scan_hd {
	u16 started;
	u16 buf_flags;
} __packed;

struct cmd_sched_scan_ie_hd {
	u16 ie_flag;
	u16 ie_len;
} __packed;

struct cmd_sched_scan_ifrc {
	u32 interval;
	u32 flags;
	s32 rssi_thold;
	u8 chan[16];
} __packed;

/* CMD_DISCONNECT */
struct cmd_disconnect {
	__le16 reason_code;
} __packed;

/* CMD_SET_PARAM */
struct cmd_set_param {
	__le16 rts;
	__le16 frag;
} __packed;

struct cmd_pmkid {
	u8 bssid[ETH_ALEN];
	u8 pmkid[WLAN_PMKID_LEN];
} __packed;

struct cmd_dscp_exception {
	u8 dscp;
	u8 up;
} __packed;

struct cmd_dscp_range {
	u8 low;
	u8 high;
} __packed;

struct cmd_qos_map {
	u8 num_des;
	struct cmd_dscp_exception dscp_exception[21];
	struct cmd_dscp_range up[8];
} __packed;

struct cmd_tx_ts {
	u8 tsid;
	u8 peer[ETH_ALEN];
	u8 user_prio;
	__le16 admitted_time;
} __packed;

/* CMD_REMAIN_CHAN */
struct cmd_remain_chan {
	u8 chan;
	u8 chan_type;
	__le32 duraion;
	__le64 cookie;
} __packed;

/* CMD_CANCEL_REMAIN_CHAN */
struct cmd_cancel_remain_chan {
	__le64 cookie;		/* cookie */
} __packed;

/* CMD_TX_MGMT */
struct cmd_mgmt_tx {
	u8 chan;		/* send channel */
	u8 dont_wait_for_ack;	/*don't wait for ack */
	__le32 wait;		/* wait time */
	__le64 cookie;		/* cookie */
	__le16 len;		/* mac length */
	u8 value[0];		/* mac */
} __packed;

/* CMD_REGISTER_FRAME */
struct cmd_register_frame {
	__le16 type;
	u8 reg;
} __packed;

/* CMD_SET_CQM */
struct cmd_cqm_rssi {
	__le32 rssih;
	__le32 rssil;
} __packed;

struct cmd_roam_offload_data {
	u8 type;
	u8 len;
	u8 value[0];
} __packed;

struct cmd_tdls {
	u8 tdls_sub_cmd_mgmt;
	u8 da[ETH_ALEN];
	u8 initiator;
	u8 paylen;
	u8 payload[0];
} __packed;

struct cmd_blacklist {
	u8 sub_type;
	u8 num;
	u8 mac[0];
} __packed;

struct cmd_tdls_channel_switch {
	u8 primary_chan;
	u8 second_chan_offset;
	u8 band;
} __packed;

struct cmd_set_mac_addr {
	u8 sub_type;
	u8 num;
	u8 mac[0];
} __packed;

struct cmd_max_sta {
	unsigned char max_sta;
} __packed;

struct cmd_miracast {
	u8 value;
} __packed;

struct cmd_rsp_state_code {
	__le32 code;
} __packed;

/* 11v cmd struct */
struct cmd_11v {
	u16 cmd;
	u16 len;
	union {
		u32 value;
		u8 buf[0];
	};
} __packed;

/* packet offload struct */
struct cmd_packet_offload {
	u32 req_id;
	u8 enable;
	u32 period;
	u16 len;
	u8 data[0];
} __packed;

/* packet offload struct */
struct cmd_extended_llstate {
	u8 type;
	u8 subtype;
	u16 len;
	u8 data[0];
} __packed;

/* sniffer mode para */
struct cmd_sniffer_para {
	u8 value;
	u8 filter;
} __packed;

enum EVT_LIST {
	EVT_MIN = 0x80,
	/* Station/P2P */
	EVT_CONNECT = EVT_MIN,
	EVT_DISCONNECT,
	EVT_SCAN_DONE,
	EVT_MGMT_FRAME,
	EVT_MGMT_TX_STATUS,
	EVT_REMAIN_CHAN_EXPIRED,
	EVT_MIC_FAIL,
	EVT_GSCAN_FRAME = 0X88,

	/* SoftAP */
	EVT_NEW_STATION = 0xA0,

	/* New Feature */
	/* Uplayer Roaming */
	EVT_CQM = 0xB0,
	EVT_MEASUREMENT,
	EVT_TDLS,
	EVT_SDIO_FLOWCON = 0xB3,
	EVT_WMM_REPORT = 0xB4,
	EVT_ACS_REPORT = 0xB5,
	EVT_ACS_LTE_CONFLICT_EVENT = 0xB6,

	/* DEBUG/OTHER */
	EVT_SDIO_SEQ_NUM = 0xE0,

	EVT_MAX
};

/* EVT_CONNECT */
struct evt_connect {
	u8 status_code;
	u8 bssid[ETH_ALEN];
	u8 channel_num;
	s8 signal;
	/* include beacon ie, req ie, resp ie */
	u8 ie[0];
} __packed;

/* EVT_DISCONNECT */
struct evt_disconnect {
	u16 reason_code;
} __packed;

/* EVT_MGMT_FRAME */
struct evt_mgmt_frame {
	u8 type;
	u8 channel;
	s8 signal;		/* signal should be signed */
	u8 reserved;
	u8 bssid[ETH_ALEN];	/* roaming frame */
	__le16 len;
	u8 data[0];
} __packed;

/* WIFI_EVENT_SCAN_COMP */
struct evt_scan_done {
	u8 type;
} __packed;

/* EVT_GSCAN_COMP */
struct evt_gscan_done {
	struct evt_scan_done evt;
	u8 bucket_id;
} __packed;

/* WIFI_EVENT_MLME_TX_STATUS */
struct evt_mgmt_tx_status {
	__le64 cookie;		/* cookie */
	u8 ack;			/* status */
	__le16 len;		/* frame len */
	u8 buf[0];		/* mgmt frame */
} __packed;

/* EVT_NEW_STATION  */
struct evt_new_station {
	u8 is_connect;
	u8 mac[ETH_ALEN];
	__le16 ie_len;
	u8 ie[0];
} __packed;

/* EVT_MIC_FAIL */
struct evt_mic_failure {
	u8 key_id;
	u8 is_mcast;
} __packed;

/* EVT_CQM */
struct evt_cqm {
	u8 status;
} __packed;

struct evt_tdls {
	u8 tdls_sub_cmd_mgmt;
	u8 mac[ETH_ALEN];
	u8 payload_len;
	u8 rcpi;
} __packed;

struct cmd_gscan_header {
	u16 subcmd;
	u16 data_len;
	u8 data[0];
} __packed;

struct llc_hdr {
	u8 dsap;
	u8 ssap;
	u8 cntl;
	u8 org_code[3];
	__be16 eth_type;
} __packed;

struct acs_channel {
	u8 channel;
	u8 duration;
	u8 busy;
} __packed;

struct sae_param {
	u16 request_type;
	u8 data[0];
};

struct tlv_data {
	u16 type;
	u16 len;
	u8 data[0];
} __packed;

enum IOCTL_TLV_TYPE_LIST {
	IOCTL_TLV_TP_VOWIFI_INFO = 6,
	IOCTL_TLV_TP_ADD_VOWIFI_PAIR = 7,
	IOCTL_TLV_TP_DEL_VOWIFI_PAIR = 8,
	IOCTL_TLV_TP_FLUSH_VOWIFI_PAIR = 9
};

struct vowifi_data {
	u16 type;
	u16 len;
	u8 data[0];
};

extern unsigned int dump_data;

struct sprd_priv;

int sc2332_cmd_scan(struct sprd_priv *priv, struct sprd_vif *vif, u32 channels,
		    int ssid_len, const u8 *ssid_list, u8 *mac_addr, u32 flags);
int sc2332_cmd_sched_scan_start(struct sprd_priv *priv, struct sprd_vif *vif,
				struct sprd_sched_scan *buf);
int sc2332_cmd_sched_scan_stop(struct sprd_priv *priv, struct sprd_vif *vif);
int sc2332_set_max_sta(struct sprd_priv *priv, struct sprd_vif *vif,
		       unsigned char max_sta);

int sc2332_enable_miracast(struct sprd_priv *priv, struct sprd_vif *vif,
			   int val);
int sc2332_set_gscan_config(struct sprd_priv *priv, struct sprd_vif *vif,
			    void *data, u16 len, u8 *r_buf, u16 *r_len);
int sc2332_set_gscan_scan_config(struct sprd_priv *priv, struct sprd_vif *vif,
				 void *data, u16 len, u8 *r_buf, u16 *r_len);
int sc2332_enable_gscan(struct sprd_priv *priv, struct sprd_vif *vif,
			void *data, u8 *r_buf, u16 *r_len);
struct sprd_msg *sc2332_cmd_getbuf(struct sprd_priv *priv, struct sprd_vif *vif,
				   u16 len, enum sprd_head_rsp rsp, u8 cmd_id);
int sc2332_get_gscan_capabilities(struct sprd_priv *priv, struct sprd_vif *vif,
				  u8 *r_buf, u16 *r_len);
int sc2332_get_gscan_channel_list(struct sprd_priv *priv, struct sprd_vif *vif,
				  void *data, u8 *r_buf, u16 *r_len);
int sc2332_cmd_send_recv(struct sprd_priv *priv,
			 struct sprd_msg *msg,
			 unsigned int timeout, u8 *rbuf, u16 *rlen);
int sc2332_extended_llstate(struct sprd_priv *priv, struct sprd_vif *vif,
			    u8 type, u8 subtype, void *buf, u8 len, u8 *r_buf,
			    u16 *r_len);
int sc2332_set_packet_offload(struct sprd_priv *priv, struct sprd_vif *vif,
			      u32 req, u8 enable, u32 interval, u32 len,
			      u8 *data);

void sc2332_report_gscan_frame_evt(struct sprd_vif *vif, u8 *data, u16 len);
int sc2332_gscan_done(struct sprd_vif *vif, u8 bucket_id);
int sc2332_report_acs_lte_event(struct sprd_vif *vif);

int sc2332_set_regdom(struct sprd_priv *priv, u8 *regdom, u32 len);

void sc2332_setup_wiphy(struct wiphy *wiphy, struct sprd_priv *priv);
void sc2332_cmd_init(struct sprd_cmd *cmd);
void sc2332_cmd_deinit(struct sprd_cmd *cmd);
int sc2332_get_fw_info(struct sprd_priv *priv);
int sc2332_open_fw(struct sprd_priv *priv, struct sprd_vif *vif, u8 *mac_addr);
int sc2332_close_fw(struct sprd_priv *priv, struct sprd_vif *vif);
int sc2332_power_save(struct sprd_priv *priv, struct sprd_vif *vif,
		      u8 sub_type, u8 status);
int sc2332_set_sar(struct sprd_priv *priv, struct sprd_vif *vif,
		   u8 sub_type, s8 value);
int sc2332_set_power_backoff(struct sprd_priv *priv, struct sprd_vif *vif,
			     u8 sub_type, s8 value, u8 mode, u8 channel);

struct sprd_msg *sc2332_get_cmdbuf(struct sprd_priv *priv, struct sprd_vif *vif,
				   u16 len, u8 cmd_id, enum sprd_head_rsp rsp);
static inline struct sprd_msg *get_cmdbuf(struct sprd_priv *priv,
					  struct sprd_vif *vif, u16 len,
					  u8 cmd_id)
{
	return sc2332_get_cmdbuf(priv, vif, len, cmd_id, SPRD_HEAD_RSP);
}

int sc2332_send_cmd_recv_rsp(struct sprd_priv *priv, struct sprd_msg *msg,
			     u8 *rbuf, u16 *rlen, unsigned int timeout);
static inline int send_cmd_recv_rsp(struct sprd_priv *priv,
				    struct sprd_msg *msg, u8 *rbuf, u16 *rlen)
{
	return sc2332_send_cmd_recv_rsp(priv, msg, rbuf, rlen,
					CMD_WAIT_TIMEOUT);
}

int sc2332_add_key(struct sprd_priv *priv, struct sprd_vif *vif,
		   const u8 *key_data, u8 key_len, bool pairwise, u8 key_index,
		   const u8 *key_seq, u8 cypher_type, const u8 *mac_addr);
int sc2332_del_key(struct sprd_priv *priv, struct sprd_vif *vif, u8 key_index,
		   bool pairwise, const u8 *mac_addr);
int sc2332_set_def_key(struct sprd_priv *priv, struct sprd_vif *vif,
		       u8 key_index);
int sc2332_set_rekey_data(struct sprd_priv *priv, struct sprd_vif *vif,
			  struct cfg80211_gtk_rekey_data *data);
int sc2332_set_beacon_ie(struct sprd_priv *priv, struct sprd_vif *vif,
			 const u8 *ie, u16 len);
int sc2332_set_probereq_ie(struct sprd_priv *priv, struct sprd_vif *vif,
			   const u8 *ie, u16 len);
int sc2332_set_proberesp_ie(struct sprd_priv *priv, struct sprd_vif *vif,
			    const u8 *ie, u16 len);
int sc2332_set_assocreq_ie(struct sprd_priv *priv, struct sprd_vif *vif,
			   const u8 *ie, u16 len);
int sc2332_set_assocresp_ie(struct sprd_priv *priv, struct sprd_vif *vif,
			    const u8 *ie, u16 len);
int sc2332_set_sae_ie(struct sprd_priv *priv, struct sprd_vif *vif,
		      const u8 *ie, u16 len);
int sc2332_start_ap(struct sprd_priv *priv, struct sprd_vif *vif, u8 *beacon,
		    u16 len, struct cfg80211_ap_settings *settings);
int sc2332_del_station(struct sprd_priv *priv, struct sprd_vif *vif,
		       const u8 *mac_addr, u16 reason_code);
int sc2332_get_station(struct sprd_priv *priv, struct sprd_vif *vif,
		       struct sprd_sta_info *sta);
int sc2332_set_channel(struct sprd_priv *priv, struct sprd_vif *vif,
		       u8 channel);
int sc2332_connect(struct sprd_priv *priv, struct sprd_vif *vif,
		   struct cmd_connect *p);
int sc2332_disconnect(struct sprd_priv *priv, struct sprd_vif *vif,
		      u16 reason_code);
int sc2332_set_param(struct sprd_priv *priv, u32 rts, u32 frag);
int sc2332_pmksa(struct sprd_priv *priv, struct sprd_vif *vif, const u8 *bssid,
		 const u8 *pmkid, u8 type);
int sc2332_set_qos_map(struct sprd_priv *priv, struct sprd_vif *vif, void *map);
int sc2332_add_tx_ts(struct sprd_priv *priv, struct sprd_vif *vif, u8 tsid,
		     const u8 *peer, u8 user_prio, u16 admitted_time);
int sc2332_del_tx_ts(struct sprd_priv *priv, struct sprd_vif *vif, u8 tsid,
		     const u8 *peer);
int sc2332_remain_chan(struct sprd_priv *priv, struct sprd_vif *vif,
		       struct ieee80211_channel *channel,
		       enum nl80211_channel_type channel_type,
		       u32 duration, u64 *cookie);
int sc2332_cancel_remain_chan(struct sprd_priv *priv, struct sprd_vif *vif,
			      u64 cookie);
int sc2332_tx_mgmt(struct sprd_priv *priv, struct sprd_vif *vif, u8 channel,
		   u8 dont_wait_for_ack, u32 wait, u64 *cookie,
		   const u8 *buf, size_t len);
int sc2332_register_frame(struct sprd_priv *priv, struct sprd_vif *vif,
			  u16 type, u8 reg);
int sc2332_set_cqm_rssi(struct sprd_priv *priv, struct sprd_vif *vif,
			s32 rssi_thold, u32 rssi_hyst);
int sc2332_set_roam_offload(struct sprd_priv *priv, struct sprd_vif *vif,
			    u8 sub_type, const u8 *data, u8 len);
int sc2332_tdls_mgmt(struct sprd_vif *vif, struct sk_buff *skb);
int sc2332_tdls_oper(struct sprd_priv *priv, struct sprd_vif *vif,
		     const u8 *peer, int oper);
int sc2332_start_tdls_channel_switch(struct sprd_priv *priv,
				     struct sprd_vif *vif, const u8 *peer_mac,
				     u8 primary_chan, u8 second_chan_offset,
				     u8 band);
int sc2332_cancel_tdls_channel_switch(struct sprd_priv *priv,
				      struct sprd_vif *vif, const u8 *peer_mac);
int sc2332_notify_ip(struct sprd_priv *priv, struct sprd_vif *vif, u8 ip_type,
		     u8 *ip_addr);
int sc2332_set_blacklist(struct sprd_priv *priv,
			 struct sprd_vif *vif, u8 sub_type, u8 num,
			 u8 *mac_addr);
int sc2332_set_whitelist(struct sprd_priv *priv, struct sprd_vif *vif,
			 u8 sub_type, u8 num, u8 *mac_addr);
int sc2332_set_mc_filter(struct sprd_priv *priv, struct sprd_vif *vif,
			 u8 sub_type, u8 num, u8 *mac_addr);
int sc2332_npi_send_recv(struct sprd_priv *priv, struct sprd_vif *vif,
			 u8 *s_buf, u16 s_len, u8 *r_buf, u16 *r_len);
int sc2332_set_11v_feature_support(struct sprd_priv *priv, struct sprd_vif *vif,
				   u16 val);
int sc2332_set_11v_sleep_mode(struct sprd_priv *priv, struct sprd_vif *vif,
			      u8 status, u16 interval);
int sc2332_xmit_data2cmd(struct sk_buff *skb, struct net_device *ndev);
int sc2332_set_random_mac(struct sprd_priv *priv, struct sprd_vif *vif,
			  u8 random_mac_flag, u8 *addr);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
int sc2332_set_vowifi(struct net_device *ndev, void __user *data);
#else
int sc2332_set_vowifi(struct net_device *ndev, struct ifreq *ifr);
#endif
bool sc2332_do_delay_work(struct sprd_work *work);
void sc2332_scan_timeout(struct timer_list *t);
int sc2332_scan(struct wiphy *wiphy, struct cfg80211_scan_request *request);
int sc2332_sched_scan_start(struct wiphy *wiphy, struct net_device *ndev,
			    struct cfg80211_sched_scan_request *request);
int sc2332_sched_scan_stop(struct wiphy *wiphy, struct net_device *ndev,
			   u64 reqid);
void sc2332_tcp_ack_init(struct sprd_priv *priv);
void sc2332_tcp_ack_deinit(struct sprd_priv *priv);
int sc2332_vendor_init(struct wiphy *wiphy);
int sc2332_vendor_deinit(struct wiphy *wiphy);
int sc2332_dump_survey(struct wiphy *wiphy, struct net_device *ndev,
		       int idx, struct survey_info *s_info);

void sc2332_report_frame_evt(struct sprd_vif *vif, u8 *data, u16 len, bool flag);
int sc2332_set_sniffer(struct net_device *ndev, struct ifreq *ifr);
#endif
