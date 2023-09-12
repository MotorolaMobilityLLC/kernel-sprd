/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * Abstract : This file is an implementation for cfg80211 subsystem
 *
 * Authors:
 * Chaojie Xu <chaojie.xu@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/etherdevice.h>
#include <net/cfg80211.h>
#include <net/netlink.h>
#include "sprdwl.h"
#include "rtt.h"
#include "if_sc2355.h"
#include "vendor.h"

/* FTM session ID we use with FW */
#define FTM_ESSION_ID		1

/* fixed spare allocation we reserve in NL messages we allocate */
#define FTM_NL_EXTRA_ALLOC		32

/* approx maximum length for FTM_MEAS_RESULT NL80211 event */
#define FTM_MEAS_RESULT_MAX_LENGTH	2048

/* maximum number of allowed FTM measurements per burst */
#define FTM_MAX_MEAS_PER_BURST	31

/* initial token to use on non-secure FTM measurement */
#define FTM_DEFAULT_INITIAL_TOKEN	2

#define FTM_MAX_LCI_LENGTH		(240)
#define FTM_MAX_LCR_LENGTH		(240)

/* max rtt cmd response length */
#define RTT_RSP_LEN (128)

enum session_start_flags {
	FTM_SESSION_START_FLAG_SECURED	= 0x1,
	FTM_SESSION_START_FLAG_ASAP		= 0x2,
	FTM_SESSION_START_FLAG_LCI_REQ	= 0x4,
	FTM_SESSION_START_FLAG_LCR_REQ	= 0x8,
};

enum rtt_subcmd {
	RTT_ENABLE,
	RTT_DISABLE,
	RTT_GET_CAPABILITIES,
	RTT_RANGE_REQUEST,
	RTT_RANGE_CANCEL,
	RTT_SET_CLI,
	RTT_SET_CLR,
	RTT_GET_RESPONDER_INFO,
	RTT_ENABLE_RESPONDER,
	RTT_DISABLE_RESPONDER,
};

enum rtt_subevt {
	RTT_SESSION_END,
	RTT_PER_DEST_RES,
};

/* Responder FTM Results */
struct sprdwl_responder_ftm_res {
	u8 t1[6];
	u8 t2[6];
	u8 t3[6];
	u8 t4[6];
	__le16 tod_err;
	__le16 toa_err;
	__le16 tod_err_initiator;
	__le16 toa_err_initiator;
} __packed;

enum ftm_per_dest_res_status {
	FTM_PER_DEST_RES_NO_ERROR		= 0x00,
	FTM_PER_DEST_RES_TX_RX_FAIL		= 0x01,
	FTM_PER_DEST_RES_PARAM_DONT_MATCH	= 0x02,
};

enum ftm_per_dest_res_flags {
	FTM_PER_DEST_RES_REQ_START		= 0x01,
	FTM_PER_DEST_RES_BURST_REPORT_END	= 0x02,
	FTM_PER_DEST_RES_REQ_END		= 0x04,
	FTM_PER_DEST_RES_PARAM_UPDATE		= 0x08,
};

struct ftm_per_dest_res {
	/* FTM session ID */
	__le32 session_id;
	/* destination MAC address */
	u8 dst_mac[ETH_ALEN];
	/* wmi_tof_ftm_per_dest_res_flags_e */
	u8 flags;
	/* wmi_tof_ftm_per_dest_res_status_e */
	u8 status;
	/* responder ASAP */
	u8 responder_asap;
	/* responder number of FTM per burst */
	u8 responder_num_ftm_per_burst;
	/* responder number of FTM burst exponent */
	u8 responder_num_ftm_bursts_exp;
	/* responder burst duration ,wmi_tof_burst_duration_e */
	u8 responder_burst_duration;
	/* responder burst period, indicate interval between two consecutive
	 * burst instances, in units of 100 ms
	 */
	__le16 responder_burst_period;
	/* receive burst counter */
	__le16 bursts_cnt;
	/* tsf of responder start burst */
	__le32 tsf_sync;
	/* actual received ftm per burst */
	u8 actual_ftm_per_burst;
	u8 reserved0[7];
	struct sprdwl_responder_ftm_res responder_ftm_res[0];
} __packed;

struct ftm_dest_info {
	u8 channel;
	u8 flags;
	u8 initial_token;
	u8 num_of_ftm_per_burst;
	u8 num_of_bursts_exp;
	u8 burst_duration;
	/* Burst Period indicate interval between two consecutive burst
	 * instances, in units of 100 ms
	 */
	__le16 burst_period;
	u8 dst_mac[ETH_ALEN];
	__le16 reserved;
} __packed;

struct ftm_session_start {
	__le32 session_id;
	u8 num_of_aoa_measures;
	u8 aoa_type;
	__le16 num_of_dest;
	u8 reserved[4];
	struct ftm_dest_info dest_info[0];
} __packed;

struct sprdwl_cmd_rtt {
	u8 sub_cmd;
	__le16 len;
	u8 data[0];
} __packed;

static const struct
nla_policy sprdwl_nl80211_loc_policy[SPRDWL_VENDOR_ATTR_LOC_MAX + 1] = {
	[SPRDWL_VENDOR_ATTR_FTM_SESSION_COOKIE] = { .type = NLA_U64 },
	[SPRDWL_VENDOR_ATTR_LOC_CAPA] = { .type = NLA_NESTED },
	[SPRDWL_VENDOR_ATTR_FTM_MEAS_PEERS] = { .type = NLA_NESTED },
	[SPRDWL_VENDOR_ATTR_FTM_MEAS_PEER_RESULTS] = { .type = NLA_NESTED },
	[SPRDWL_VENDOR_ATTR_FTM_RESPONDER_ENABLE] = { .type = NLA_FLAG },
	[SPRDWL_VENDOR_ATTR_LOC_SESSION_STATUS] = { .type = NLA_U32 },
	[SPRDWL_VENDOR_ATTR_FTM_INITIAL_TOKEN] = { .type = NLA_U8 },
	[SPRDWL_VENDOR_ATTR_AOA_TYPE] = { .type = NLA_U32 },
	[SPRDWL_VENDOR_ATTR_LOC_ANTENNA_ARRAY_MASK] = { .type = NLA_U32 },
	[SPRDWL_VENDOR_ATTR_FREQ] = { .type = NLA_U32 },
};

static const struct
nla_policy sprdwl_nl80211_ftm_peer_policy[
	SPRDWL_VENDOR_ATTR_FTM_PEER_MAX + 1] = {
	[SPRDWL_VENDOR_ATTR_FTM_PEER_MAC_ADDR] = { .len = ETH_ALEN },
	[SPRDWL_VENDOR_ATTR_FTM_PEER_MEAS_FLAGS] = { .type = NLA_U32 },
	[SPRDWL_VENDOR_ATTR_FTM_PEER_MEAS_PARAMS] = { .type = NLA_NESTED },
	[SPRDWL_VENDOR_ATTR_FTM_PEER_SECURE_TOKEN_ID] = { .type = NLA_U8 },
	[SPRDWL_VENDOR_ATTR_FTM_PEER_FREQ] = { .type = NLA_U32 },
};

static const struct
nla_policy sprdwl_nl80211_ftm_meas_param_policy[
	SPRDWL_VENDOR_ATTR_FTM_PARAM_MAX + 1] = {
	[SPRDWL_VENDOR_ATTR_FTM_PARAM_MEAS_PER_BURST] = { .type = NLA_U8 },
	[SPRDWL_VENDOR_ATTR_FTM_PARAM_NUM_BURSTS_EXP] = { .type = NLA_U8 },
	[SPRDWL_VENDOR_ATTR_FTM_PARAM_BURST_DURATION] = { .type = NLA_U8 },
	[SPRDWL_VENDOR_ATTR_FTM_PARAM_BURST_PERIOD] = { .type = NLA_U16 },
};

static const struct nla_policy
sprdwl_rtt_policy[SPRD_VENDOR_RTT_ATTRIBUTE_MAX + 1] = {
	[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_MAC] = { .len = ETH_ALEN },
	[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_TYPE] = { .type = NLA_U8 },
	[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_PEER] = { .type = NLA_U8 },
	[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_CHAN] = { .len = sizeof(struct wifi_channel_info) },
	[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_PERIOD] = { .type = NLA_U32 },
	[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_NUM_BURST] = { .type = NLA_U32 },
	[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_NUM_FTM_BURST] = { .type = NLA_U32 },
	[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTM] = { .type = NLA_U32 },
	[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTMR] = { .type = NLA_U32 },
	[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_LCI] = { .type = NLA_U8 },
	[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_LCR] = { .type = NLA_U8 },
	[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_BURST_DURATION] = { .type = NLA_U32 },
	[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_PREAMBLE] = { .type = NLA_U8 },
	[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_BW] = { .type = NLA_U8 },
	[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_RESPONDER_INFO] = { .type = NLA_U32 }

};
#if 0
static u8 sprdwl_ftm_get_channel(struct wiphy *wiphy,
				 const u8 *mac_addr, u32 freq)
{
	struct cfg80211_bss *bss;
	struct ieee80211_channel *chan;
	u8 channel;

	if (freq) {
		chan = ieee80211_get_channel(wiphy, freq);
		if (!chan) {
			wl_err("invalid freq: %d\n", freq);
			return 0;
		}
		channel = chan->hw_value;
	} else {
		bss = cfg80211_get_bss(wiphy, NULL, mac_addr,
				       NULL, 0, WLAN_CAPABILITY_ESS,
				       WLAN_CAPABILITY_ESS);
		if (!bss) {
			wl_err("Unable to find BSS\n");
			return 0;
		}
		channel = bss->channel->hw_value;
		cfg80211_put_bss(wiphy, bss);
	}

	wl_info("target %pM at channel %d\n", mac_addr, channel);
	return channel;
}


static int sprdwl_ftm_parse_meas_params(struct sprdwl_vif *vif,
					struct nlattr *attr,
					struct sprdwl_ftm_meas_params *params)
{
	struct nlattr *tb[SPRDWL_VENDOR_ATTR_FTM_PARAM_MAX + 1];
	int rc;

	if (!attr) {
		/* temporary defaults for one-shot measurement */
		params->meas_per_burst = 1;
		params->burst_period = 5; /* 500 milliseconds */
		return 0;
	}
	rc = nla_parse_nested(tb, SPRDWL_VENDOR_ATTR_FTM_PARAM_MAX,
			      attr, sprdwl_nl80211_ftm_meas_param_policy);
	if (rc) {
		netdev_err(vif->ndev,
			   "%s: invalid measurement params\n", __func__);
		return rc;
	}
	if (tb[SPRDWL_VENDOR_ATTR_FTM_PARAM_MEAS_PER_BURST])
		params->meas_per_burst = nla_get_u8(
			tb[SPRDWL_VENDOR_ATTR_FTM_PARAM_MEAS_PER_BURST]);
	if (tb[SPRDWL_VENDOR_ATTR_FTM_PARAM_NUM_BURSTS_EXP])
		params->num_of_bursts_exp = nla_get_u8(
			tb[SPRDWL_VENDOR_ATTR_FTM_PARAM_NUM_BURSTS_EXP]);
	if (tb[SPRDWL_VENDOR_ATTR_FTM_PARAM_BURST_DURATION])
		params->burst_duration = nla_get_u8(
			tb[SPRDWL_VENDOR_ATTR_FTM_PARAM_BURST_DURATION]);
	if (tb[SPRDWL_VENDOR_ATTR_FTM_PARAM_BURST_PERIOD])
		params->burst_period = nla_get_u16(
			tb[SPRDWL_VENDOR_ATTR_FTM_PARAM_BURST_PERIOD]);
	return 0;
}

static int
sprdwl_ftm_validate_meas_params(struct sprdwl_vif *vif,
				struct sprdwl_ftm_meas_params *params)
{
	if (params->meas_per_burst > FTM_MAX_MEAS_PER_BURST ||
	    params->num_of_bursts_exp != 0) {
		netdev_err(vif->ndev, "%s: invalid meas per burst\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int sprdwl_ftm_append_meas_params(struct sprdwl_priv *priv,
					 struct sk_buff *msg,
					 struct sprdwl_ftm_meas_params *params)
{
	struct nlattr *nl_p;

	nl_p = nla_nest_start(
		msg, SPRDWL_VENDOR_ATTR_FTM_PEER_RES_MEAS_PARAMS);
	if (!nl_p)
		goto out_put_failure;
	if (nla_put_u8(msg, SPRDWL_VENDOR_ATTR_FTM_PARAM_MEAS_PER_BURST,
		       params->meas_per_burst) ||
	    nla_put_u8(msg, SPRDWL_VENDOR_ATTR_FTM_PARAM_NUM_BURSTS_EXP,
		       params->num_of_bursts_exp) ||
	    nla_put_u8(msg, SPRDWL_VENDOR_ATTR_FTM_PARAM_BURST_DURATION,
		       params->burst_duration) ||
	    nla_put_u16(msg, SPRDWL_VENDOR_ATTR_FTM_PARAM_BURST_PERIOD,
			params->burst_period))
		goto out_put_failure;
	nla_nest_end(msg, nl_p);
	return 0;
out_put_failure:
	return -ENOBUFS;
}

static int sprdwl_ftm_append_peer_meas_res(struct sprdwl_priv *priv,
					   struct sk_buff *msg,
					   struct sprdwl_ftm_peer_meas_res *res)
{
	struct nlattr *nl_mres, *nl_f;
	int i;

	if (nla_put(msg, SPRDWL_VENDOR_ATTR_FTM_PEER_RES_MAC_ADDR,
		    ETH_ALEN, res->mac_addr) ||
	    nla_put_u32(msg, SPRDWL_VENDOR_ATTR_FTM_PEER_RES_FLAGS,
			res->flags) ||
	    nla_put_u8(msg, SPRDWL_VENDOR_ATTR_FTM_PEER_RES_STATUS,
		       res->status))
		goto out_put_failure;
	if (res->status == SPRDWL_VENDOR_ATTR_FTM_PEER_RES_STATUS_FAILED &&
	    nla_put_u8(msg,
		       SPRDWL_VENDOR_ATTR_FTM_PEER_RES_VALUE_SECONDS,
		       res->value_seconds))
		goto out_put_failure;
	if (res->has_params &&
	    sprdwl_ftm_append_meas_params(priv, msg, &res->params))
		goto out_put_failure;
	nl_mres = nla_nest_start(msg, SPRDWL_VENDOR_ATTR_FTM_PEER_RES_MEAS);
	if (!nl_mres)
		goto out_put_failure;
	for (i = 0; i < res->n_meas; i++) {
		nl_f = nla_nest_start(msg, i);
		if (!nl_f)
			goto out_put_failure;
		if (nla_put_u64(msg, SPRDWL_VENDOR_ATTR_FTM_MEAS_T1,
				res->meas[i].t1) ||
		    nla_put_u64(msg, SPRDWL_VENDOR_ATTR_FTM_MEAS_T2,
				res->meas[i].t2) ||
		    nla_put_u64(msg, SPRDWL_VENDOR_ATTR_FTM_MEAS_T3,
				res->meas[i].t3) ||
		    nla_put_u64(msg, SPRDWL_VENDOR_ATTR_FTM_MEAS_T4,
				res->meas[i].t4))
			goto out_put_failure;
		nla_nest_end(msg, nl_f);
	}
	nla_nest_end(msg, nl_mres);
	return 0;
out_put_failure:
	wl_err("%s: fail to append peer result\n", __func__);
	return -ENOBUFS;
}

static void sprdwl_ftm_send_meas_result(struct sprdwl_priv *priv,
					struct sprdwl_ftm_peer_meas_res *res)
{
	struct sk_buff *skb = NULL;
	struct nlattr *nl_res;
	int rc = 0;

	wl_info("sending %d results for peer %pM\n",
		res->n_meas, res->mac_addr);

	skb = cfg80211_vendor_event_alloc(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 83)
				priv->wiphy, NULL,
#else
				priv->wiphy,
#endif
				FTM_MEAS_RESULT_MAX_LENGTH,
				SPRD_VENDOR_EVENT_FTM_MEAS_RESULT_INDEX,
				GFP_KERNEL);
	if (!skb) {
		wl_err("fail to allocate measurement result\n");
		rc = -ENOMEM;
		goto out;
	}

	if (nla_put_u64(
		skb, SPRDWL_VENDOR_ATTR_FTM_SESSION_COOKIE,
		priv->ftm.session_cookie)) {
		rc = -ENOBUFS;
		goto out;
	}

	nl_res = nla_nest_start(skb,
				SPRDWL_VENDOR_ATTR_FTM_MEAS_PEER_RESULTS);
	if (!nl_res) {
		rc = -ENOBUFS;
		goto out;
	}

	rc = sprdwl_ftm_append_peer_meas_res(priv, skb, res);
	if (rc)
		goto out;

	nla_nest_end(skb, nl_res);
	cfg80211_vendor_event(skb, GFP_KERNEL);
	skb = NULL;
out:
	if (skb)
		kfree_skb(skb);
	if (rc)
		wl_err("send peer result failed, err %d\n", rc);
}

static void sprdwl_ftm_send_peer_res(struct sprdwl_priv *priv)
{
	if (!priv->ftm.has_ftm_res || !priv->ftm.ftm_res)
		return;

	sprdwl_ftm_send_meas_result(priv, priv->ftm.ftm_res);
	priv->ftm.has_ftm_res = 0;
	priv->ftm.ftm_res->n_meas = 0;
}



static int
sprdwl_ftm_cfg80211_start_session(struct sprdwl_priv *priv,
				  struct sprdwl_vif *vif,
				  struct sprdwl_ftm_session_request *request)
{
	int ret = 0;
	bool has_lci = false, has_lcr = false;
	u8 max_meas = 0, channel, *ptr;
	u32 i, cmd_len;
	struct ftm_session_start *cmd;
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_rtt *rtt;

	mutex_lock(&priv->ftm.lock);
	if (priv->ftm.session_started) {
		wl_err("%s: FTM session already running\n", __func__);
		ret = -EALREADY;
		goto out;
	}

	for (i = 0; i < request->n_peers; i++) {
		if (request->peers[i].flags &
		    SPRDWL_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_LCI)
			has_lci = true;
		if (request->peers[i].flags &
		    SPRDWL_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_LCR)
			has_lcr = true;
		max_meas = max(max_meas,
			       request->peers[i].params.meas_per_burst);
	}

	priv->ftm.ftm_res = kzalloc(sizeof(*priv->ftm.ftm_res) +
		      max_meas * sizeof(struct sprdwl_ftm_peer_meas) +
		      (has_lci ? FTM_MAX_LCI_LENGTH : 0) +
		      (has_lcr ? FTM_MAX_LCR_LENGTH : 0), GFP_KERNEL);
	if (!priv->ftm.ftm_res) {
		ret = -ENOMEM;
		goto out;
	}
	ptr = (u8 *)priv->ftm.ftm_res;
	ptr += sizeof(struct sprdwl_ftm_peer_meas_res) +
	       max_meas * sizeof(struct sprdwl_ftm_peer_meas);
	if (has_lci) {
		priv->ftm.ftm_res->lci = ptr;
		ptr += FTM_MAX_LCI_LENGTH;
	}
	if (has_lcr)
		priv->ftm.ftm_res->lcr = ptr;
	priv->ftm.max_ftm_meas = max_meas;

	cmd_len = sizeof(struct ftm_session_start) +
		  request->n_peers * sizeof(struct ftm_dest_info);
	cmd = kzalloc(cmd_len, GFP_KERNEL);
	if (!cmd) {
		ret = -ENOMEM;
		goto out_ftm_res;
	}

	cmd->session_id = cpu_to_le32(FTM_ESSION_ID);
	cmd->num_of_dest = cpu_to_le16(request->n_peers);
	for (i = 0; i < request->n_peers; i++) {
		ether_addr_copy(cmd->dest_info[i].dst_mac,
				request->peers[i].mac_addr);
		channel = sprdwl_ftm_get_channel(priv->wiphy,
						 request->peers[i].mac_addr,
						 request->peers[i].freq);
		if (!channel) {
			wl_err("%s: can't find FTM target at index %d\n",
			       __func__, i);
			ret = -EINVAL;
			goto out_cmd;
		}
		cmd->dest_info[i].channel = channel - 1;
		if (request->peers[i].flags &
		    SPRDWL_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_SECURE) {
			cmd->dest_info[i].flags |=
				FTM_SESSION_START_FLAG_SECURED;
			cmd->dest_info[i].initial_token =
				request->peers[i].secure_token_id;
		} else {
			cmd->dest_info[i].initial_token =
				FTM_DEFAULT_INITIAL_TOKEN;
		}
		if (request->peers[i].flags &
		    SPRDWL_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_ASAP)
			cmd->dest_info[i].flags |=
				FTM_SESSION_START_FLAG_ASAP;
		if (request->peers[i].flags &
		    SPRDWL_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_LCI)
			cmd->dest_info[i].flags |=
				FTM_SESSION_START_FLAG_LCI_REQ;
		if (request->peers[i].flags &
		    SPRDWL_VENDOR_ATTR_FTM_PEER_MEAS_FLAG_LCR)
			cmd->dest_info[i].flags |=
				FTM_SESSION_START_FLAG_LCR_REQ;
		cmd->dest_info[i].num_of_ftm_per_burst =
			request->peers[i].params.meas_per_burst;
		cmd->dest_info[i].num_of_bursts_exp =
			request->peers[i].params.num_of_bursts_exp;
		cmd->dest_info[i].burst_duration =
			request->peers[i].params.burst_duration;
		cmd->dest_info[i].burst_period =
			cpu_to_le16(request->peers[i].params.burst_period);
	}

	/* send range request data to the FW */
	msg = sprdwl_cmd_getbuf(priv, sizeof(struct sprdwl_cmd_rtt) + cmd_len,
				vif->ctx_id, SPRDWL_HEAD_RSP, WIFI_CMD_RTT);
	if (!msg) {
		ret = -ENOMEM;
		goto out_cmd;
	}
	rtt = (struct sprdwl_cmd_rtt *)msg->data;
	rtt->sub_cmd = RTT_RANGE_REQUEST;
	rtt->len = cmd_len;
	memcpy(rtt->data, cmd, cmd_len);

	ret = sprdwl_cmd_send_recv(priv, msg,
				   CMD_WAIT_TIMEOUT, NULL, 0);
	if (ret) {
		netdev_err(vif->ndev, "%s: ret=%d\n", __func__, ret);
	} else {
		priv->ftm.session_cookie = request->session_cookie;
		priv->ftm.session_started = 1;
	}
out_cmd:
	kfree(cmd);
out_ftm_res:
	if (ret) {
		kfree(priv->ftm.ftm_res);
		priv->ftm.ftm_res = NULL;
	}
out:
	mutex_unlock(&priv->ftm.lock);
	return ret;
}

static void
sprdwl_ftm_session_ended(struct sprdwl_priv *priv, u32 status)
{



	mutex_lock(&priv->ftm.lock);
/*
	if (!priv->ftm.session_started) {
		wl_err("%s: FTM session not started, ignoring\n", __func__);
		return;
	}

	wl_info("%s: finishing FTM session\n", __func__);
*/
	/* send left-over results if any */
	sprdwl_ftm_send_peer_res(priv);

	priv->ftm.session_started = 0;
	kfree(priv->ftm.ftm_res);
	priv->ftm.ftm_res = NULL;

	skb = cfg80211_vendor_event_alloc(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 83)
		priv->wiphy, NULL,
#else
		priv->wiphy,
#endif
		FTM_NL_EXTRA_ALLOC,
		SPRD_VENDOR_EVENT_FTM_SESSION_DONE_INDEX,
		GFP_KERNEL);
	if (!skb)
		goto out;

	if (nla_put_u64(skb,
			SPRDWL_VENDOR_ATTR_FTM_SESSION_COOKIE,
			priv->ftm.session_cookie) ||
	    nla_put_u32(skb,
			SPRDWL_VENDOR_ATTR_LOC_SESSION_STATUS, status)) {
		wl_err("%s: failed to fill session done event\n", __func__);
		goto out;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	skb = NULL;
out:
	kfree_skb(skb);
	mutex_unlock(&priv->ftm.lock);
}
#if 0
void sprdwl_ftm_event_per_dest_res(struct sprdwl_priv *priv,
				   struct wifi_rtt_result *res)
{
	u32 i, index;
	__le64 tmp = 0;
	u8 n_meas;

	mutex_lock(&priv->ftm.lock);

	if (!priv->ftm.session_started || !priv->ftm.ftm_res) {
		wl_err("%s: Session not running, ignoring res event\n",
		       __func__);
		goto out;
	}
	if (priv->ftm.has_ftm_res &&
	    !ether_addr_equal(res->dst_mac, priv->ftm.ftm_res->mac_addr)) {
		wl_err("%s: previous peer not properly terminated\n",
		       __func__);
		sprdwl_ftm_send_peer_res(priv);
	}

	if (!priv->ftm.has_ftm_res) {
		ether_addr_copy(priv->ftm.ftm_res->mac_addr, res->dst_mac);
		priv->ftm.has_ftm_res = 1;
	}

	n_meas = res->actual_ftm_per_burst;
	switch (res->status) {
	case FTM_PER_DEST_RES_NO_ERROR:
		priv->ftm.ftm_res->status =
			SPRDWL_VENDOR_ATTR_FTM_PEER_RES_STATUS_OK;
		break;
	case FTM_PER_DEST_RES_TX_RX_FAIL:
		/* FW reports corrupted results here, discard. */
		n_meas = 0;
		priv->ftm.ftm_res->status =
			SPRDWL_VENDOR_ATTR_FTM_PEER_RES_STATUS_OK;
		break;
	case FTM_PER_DEST_RES_PARAM_DONT_MATCH:
		priv->ftm.ftm_res->status =
			SPRDWL_VENDOR_ATTR_FTM_PEER_RES_STATUS_INVALID;
		break;
	default:
		wl_err("%s: unexpected status %d\n", __func__, res->status);
		priv->ftm.ftm_res->status =
			SPRDWL_VENDOR_ATTR_FTM_PEER_RES_STATUS_INVALID;
		break;
	}

	for (i = 0; i < n_meas; i++) {
		index = priv->ftm.ftm_res->n_meas;
		if (index >= priv->ftm.max_ftm_meas) {
			wl_info("%s: Too many measurements\n", __func__);
			break;
		}
		memcpy(&tmp, res->responder_ftm_res[i].t1,
		       sizeof(res->responder_ftm_res[i].t1));
		priv->ftm.ftm_res->meas[index].t1 = le64_to_cpu(tmp);
		memcpy(&tmp, res->responder_ftm_res[i].t2,
		       sizeof(res->responder_ftm_res[i].t2));
		priv->ftm.ftm_res->meas[index].t2 = le64_to_cpu(tmp);
		memcpy(&tmp, res->responder_ftm_res[i].t3,
		       sizeof(res->responder_ftm_res[i].t3));
		priv->ftm.ftm_res->meas[index].t3 = le64_to_cpu(tmp);
		memcpy(&tmp, res->responder_ftm_res[i].t4,
		       sizeof(res->responder_ftm_res[i].t4));
		priv->ftm.ftm_res->meas[index].t4 = le64_to_cpu(tmp);
		priv->ftm.ftm_res->n_meas++;
	}

	if (res->flags & FTM_PER_DEST_RES_BURST_REPORT_END)
		sprdwl_ftm_send_peer_res(priv);
out:
	mutex_unlock(&priv->ftm.lock);
}
#endif
#endif
void sprdwl_ftm_event_end(struct sprdwl_priv *priv)
{
	struct sk_buff *reply;
	struct wiphy *wiphy = priv->wiphy;
	struct sprdwl_vif *vif = mode_to_vif(priv, SPRDWL_MODE_STATION);
	int i;
	struct nlattr *nl_res;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 83)
	reply = cfg80211_vendor_event_alloc(wiphy, &vif->wdev,
#else
	reply = cfg80211_vendor_event_alloc(wiphy,
#endif
					    priv->rtt_results.peer_num * sizeof(struct wifi_hal_rtt_result) + NLMSG_HDRLEN,
		SPRD_RTT_EVENT_COMPLETE_INDEX,
		GFP_KERNEL);
	if (!reply) {
		printk("%s, %d\n", __func__, __LINE__);
		return;
	}

	for (i = 0; i < priv->rtt_results.peer_num; i++) {
		wl_err("%s, %d\n", __func__, i);
		nl_res = nla_nest_start(reply, SPRD_VENDOR_RTT_ATTRIBUTE_RESULTS_PER_TARGET);
		if (!nl_res) {
			wl_err("%s, %d\n", __func__, __LINE__);
			goto out;
		}
		if (nla_put(reply, SPRD_VENDOR_RTT_ATTRIBUTE_RESULT_MAC,
			6, priv->rtt_results.peer_rtt_result[i]->mac_addr) ||
		nla_put_u32(reply, SPRD_VENDOR_RTT_ATTRIBUTE_RESULT_CNT_CNT, i + 1) ||
		nla_put(reply, SPRD_VENDOR_RTT_ATTRIBUTE_RESULT,
			2 * sizeof(struct wifi_hal_rtt_result), priv->rtt_results.peer_rtt_result[i])) {
			wl_info("%s, %d\n", __func__, __LINE__);
			goto out;
		}

		nla_nest_end(reply, nl_res);
	}
	nla_put_u32(reply, SPRD_VENDOR_RTT_ATTRIBUTE_RESULTS_COMPLETE, 1);
	cfg80211_vendor_event(reply, GFP_KERNEL);
	reply = NULL;
	wl_info("report rtt result\n");
	priv->ftm.session_started = 0;

	priv->rtt_results.peer_num  = 0;
out:
	if (reply)
		kfree_skb(reply);
}

#ifndef BITS_PER_LONG
#define BITS_PER_LONG 64
#endif
unsigned long int_sqrt(unsigned long x)
{
	unsigned long op, res, one;
	op = x;
	res = 0;
	one = 1UL << (BITS_PER_LONG - 2);
	while (one > op)
		one >>= 2;

	while (one != 0) {
		if (op >= res + one) {
			op = op - (res + one);
			res = res +  2 * one;
		}
		res /= 2;
		one /= 4;
	}
	return res;
}

int power(int base, int exponent)
{
	int result = 1;
	int i = 0;

	for (i = 0; i < exponent; i++) {
		result *= base;
	}

	return result;
}

void sprdwl_ftm_event_per_dest_res(struct sprdwl_priv *priv,
				   struct wifi_rtt_result *res)
{
	s64 rtt_time_tmp = 0, avg_rtt_time = 0, *rtt_time,
		max_rtt_time = 0, min_rtt_time = 0, pow_rtt_time = 0, sqrt_rtt_time = 0;
	int i, j, rtt_distance_tmp = 0, avg_rtt_distance = 0, *rtt_distance,
		max_rtt_distance = 0, min_rtt_distance = 0, pow_rtt_distance = 0, sqrt_rtt_distance = 0;

	mutex_lock(&priv->ftm.lock);
	i = priv->rtt_results.peer_num;
	priv->rtt_results.peer_num++;
	memcpy(priv->rtt_results.peer_rtt_result[i]->mac_addr, res->mac_addr, 6);
	priv->rtt_results.peer_rtt_result[i]->burst_num =  res->params.meas_per_burst;
	priv->rtt_results.peer_rtt_result[i]->measurement_number = res->measurement_number;
	priv->rtt_results.peer_rtt_result[i]->success_number = res->success_number;
	priv->rtt_results.peer_rtt_result[i]->number_per_burst_peer = res->params.num_of_bursts_exp;
	priv->rtt_results.peer_rtt_result[i]->status = res->status;
	priv->rtt_results.peer_rtt_result[i]->retry_after_duration = res->retry_after_duration;
	priv->rtt_results.peer_rtt_result[i]->type = res->type;
	priv->rtt_results.peer_rtt_result[i]->rssi = res->rssi;
	priv->rtt_results.peer_rtt_result[i]->rssi_spread = res->rssi_spread;
	priv->rtt_results.peer_rtt_result[i]->tx_rate.preamble = res->tx_rate.preamble;
	priv->rtt_results.peer_rtt_result[i]->tx_rate.nss = res->tx_rate.nss;
	priv->rtt_results.peer_rtt_result[i]->tx_rate.bw = res->tx_rate.bw;
	priv->rtt_results.peer_rtt_result[i]->tx_rate.ratemcsidx = res->tx_rate.ratemcsidx;
	priv->rtt_results.peer_rtt_result[i]->tx_rate.reserved = res->tx_rate.reserved;
	priv->rtt_results.peer_rtt_result[i]->tx_rate.bitrate = res->tx_rate.bitrate;
	priv->rtt_results.peer_rtt_result[i]->rx_rate.preamble = res->rx_rate.preamble;
	priv->rtt_results.peer_rtt_result[i]->rx_rate.nss = res->rx_rate.nss;
	priv->rtt_results.peer_rtt_result[i]->rx_rate.bw = res->rx_rate.bw;
	priv->rtt_results.peer_rtt_result[i]->rx_rate.ratemcsidx = res->rx_rate.ratemcsidx;
	priv->rtt_results.peer_rtt_result[i]->rx_rate.reserved = res->rx_rate.reserved;
	priv->rtt_results.peer_rtt_result[i]->rx_rate.bitrate = res->rx_rate.bitrate;
	wl_info("%s, burst_num: %u, measurement_number:%u, success_number:%u, number_per_burst_peer:%u, status:%d\n",
			__func__, priv->rtt_results.peer_rtt_result[i]->burst_num, priv->rtt_results.peer_rtt_result[i]->measurement_number,
			priv->rtt_results.peer_rtt_result[i]->success_number, priv->rtt_results.peer_rtt_result[i]->number_per_burst_peer,
			priv->rtt_results.peer_rtt_result[i]->status);
	if (res->n_meas > 0) {
		rtt_time = kmalloc(res->n_meas*sizeof(s64), GFP_KERNEL);
		rtt_distance = kmalloc(res->n_meas*sizeof(int), GFP_KERNEL);
		for (j = 0; j < res->n_meas; j++) {
			if ((res->meas[j].t3 < res->meas[j].t2) && (res->meas[j].t4 > res->meas[j].t1))
				rtt_time_tmp = ((res->meas[j].t4 - res->meas[j].t1) - (power(2, 24)*25/8 + res->meas[j].t3 - res->meas[j].t2))/2;
			else if ((res->meas[j].t3 > res->meas[j].t2) && (res->meas[j].t4 < res->meas[j].t1))
				rtt_time_tmp = ((power(2, 24)*25/8 + res->meas[j].t4 - res->meas[j].t1) - (res->meas[j].t3 - res->meas[j].t2))/2;
			else
				rtt_time_tmp = ((res->meas[j].t4 - res->meas[j].t1) - (res->meas[j].t3 - res->meas[j].t2))/2;
			avg_rtt_time += rtt_time_tmp;
			rtt_time[j] =  div_s64(rtt_time_tmp, 1000);
			rtt_distance_tmp = (int)(div_s64(rtt_time_tmp*3, 10));
			avg_rtt_distance += rtt_distance_tmp;
			rtt_distance[j] = rtt_distance_tmp;

			if (j == 0) {
				max_rtt_time = min_rtt_time = rtt_time_tmp;
				max_rtt_distance = min_rtt_distance = rtt_distance_tmp;
			} else {
				if (max_rtt_time < rtt_time_tmp)
					max_rtt_time = rtt_time_tmp;
				if (min_rtt_time > rtt_time_tmp)
					min_rtt_time = rtt_time_tmp;
				if (max_rtt_distance < rtt_distance_tmp)
					max_rtt_distance = rtt_distance_tmp;
				if (min_rtt_distance > rtt_distance_tmp)
					min_rtt_distance = rtt_distance_tmp;
			}
			wl_info("%s,t1:%llu, t2:%llu, t3:%llu, t4s:%llu, rtt time:%lld, rtt distance:%d!\n", __func__, res->meas[j].t1, res->meas[j].t2,
					res->meas[j].t3, res->meas[j].t4, rtt_time[j], rtt_distance[j]);
		}

		avg_rtt_time = div_s64(avg_rtt_time, res->n_meas);
		avg_rtt_distance = div_s64(avg_rtt_distance, res->n_meas);

		for (j = 0; j < res->n_meas; j++) {
			pow_rtt_time += (rtt_time[j] - avg_rtt_time) * (rtt_time[j] - avg_rtt_time);
			pow_rtt_distance += (rtt_distance[j] - avg_rtt_distance) * (rtt_distance[j] - avg_rtt_distance);
		}

		pow_rtt_time = div_s64(pow_rtt_time, res->n_meas);
		pow_rtt_distance = div_s64(pow_rtt_distance, res->n_meas);
		sqrt_rtt_time = (u64)int_sqrt((unsigned long)pow_rtt_time);
		sqrt_rtt_distance = (int)int_sqrt((unsigned long)pow_rtt_distance);
		kfree(rtt_time);
		kfree(rtt_distance);
	}

	priv->rtt_results.peer_rtt_result[i]->rtt = avg_rtt_time;
	priv->rtt_results.peer_rtt_result[i]->rtt_sd = sqrt_rtt_time;
	priv->rtt_results.peer_rtt_result[i]->rtt_spread = max_rtt_time - min_rtt_time;
	priv->rtt_results.peer_rtt_result[i]->distance_mm = avg_rtt_distance;
	priv->rtt_results.peer_rtt_result[i]->distance_sd_mm = sqrt_rtt_distance;
	priv->rtt_results.peer_rtt_result[i]->distance_spread_mm = max_rtt_distance - min_rtt_distance;

	wl_info("%s, rtt:%llu, rtt_sd:%llu, rtt_spread:%llu, distance_mm:%d, distance_sd_mm:%d, distance_spread_mm:%d\n",
		__func__, priv->rtt_results.peer_rtt_result[i]->rtt, priv->rtt_results.peer_rtt_result[i]->rtt_sd,
		priv->rtt_results.peer_rtt_result[i]->rtt_spread, priv->rtt_results.peer_rtt_result[i]->distance_mm,
		priv->rtt_results.peer_rtt_result[i]->distance_sd_mm, priv->rtt_results.peer_rtt_result[i]->distance_spread_mm);

	priv->rtt_results.peer_rtt_result[i]->ts = res->ts;
	priv->rtt_results.peer_rtt_result[i]->burst_duration = res->params.burst_duration;
	priv->rtt_results.peer_rtt_result[i]->negotiated_burst_num = 1; //TODO

#define DOT11_HDR_LEN 2
#define DOT11_RM_IE_LEN       5
#define DOT11_MNG_MEASURE_REQUEST_ID		38	/* 11H MeasurementRequest */
#define DOT11_MEASURE_TYPE_LCI		8   /* d11 measurement LCI type */
#define DOT11_MEASURE_TYPE_CIVICLOC	11  /* d11 measurement location civic */
	priv->rtt_results.ele1[i] = (struct sprdwl_dot11_rm_ie *)(priv->rtt_results.peer_rtt_result[i] + 1);
	priv->rtt_results.ele1[i]->id = DOT11_MNG_MEASURE_REQUEST_ID;
	priv->rtt_results.ele1[i]->len = 3;
	priv->rtt_results.ele1[i]->type = DOT11_MEASURE_TYPE_LCI;
	priv->rtt_results.ele1[i]->mode = 0x01;
	priv->rtt_results.ele1[i]->token = 0x01;

	priv->rtt_results.ele2[i] = (struct sprdwl_dot11_rm_ie *)((char *)(priv->rtt_results.ele1[i]) + (priv->rtt_results.ele1[i]->len + DOT11_HDR_LEN));
	priv->rtt_results.ele2[i]->id = DOT11_MNG_MEASURE_REQUEST_ID;
	priv->rtt_results.ele2[i]->len = 3;
	priv->rtt_results.ele2[i]->type = DOT11_MEASURE_TYPE_CIVICLOC;
	priv->rtt_results.ele2[i]->mode = 0x02;
	priv->rtt_results.ele2[i]->token = 0x02;
	mutex_unlock(&priv->ftm.lock);

}

int sprdwl_event_ftm(struct sprdwl_vif *vif, u8 *data, u16 len)
{
	struct sprdwl_priv *priv = vif->priv;
	u8 sub_event;
	struct wifi_rtt_result *res;

	if (len > 100)
		sprdwl_hex_dump("rtt result debug:", data, 100);
	else
		sprdwl_hex_dump("rtt result debug:", data, len);

	memcpy(&sub_event, data, sizeof(sub_event));
	data += sizeof(sub_event);
	len -= sizeof(sub_event);

	switch (sub_event) {
	case RTT_SESSION_END:
	wl_err("rec rtt result completed!\n");
		sprdwl_ftm_event_end(priv);
		break;
	case RTT_PER_DEST_RES:
		wl_err("rec rtt result!\n");
		res = (struct wifi_rtt_result *)data;
		sprdwl_ftm_event_per_dest_res(priv, res);
		break;
	default:
		netdev_err(vif->ndev, "%s: unknown FTM event\n", __func__);
		break;
	}
	return 0;
#if 0
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 83)
	reply = cfg80211_vendor_event_alloc(wiphy, &vif->wdev,
#else
	reply = cfg80211_vendor_event_alloc(wiphy,
#endif
					    /*priv->rtt_results->peer_num * sizeof(struct wifi_rtt_result1)*/20,
		SPRD_RTT_EVENT_COMPLETE_INDEX,
		GFP_KERNEL);
	if (!reply) {
		netdev_info(vif->ndev, "skb alloc failed");
		return -ENOMEM;
	}
	nla_put_u8(reply, GSCAN_RESULTS_SCAN_RESULT_MORE_DATA, 0);
	cfg80211_vendor_event(reply, GFP_KERNEL);
	reply = NULL;

	if (reply)
		kfree_skb(reply);

	return 0;


	memcpy(&sub_event, data, sizeof(sub_event));
	data += sizeof(sub_event);
	len -= sizeof(sub_event);

	switch (sub_event) {
	case RTT_SESSION_END:
		wl_err("rec rtt result completed!\n");
		/*
		memcpy(&status, data, sizeof(status));
		sprdwl_ftm_session_ended(priv, status);*/
	{
		struct sprdwl_priv *priv = vif->priv;
		struct wiphy *wiphy = priv->wiphy;
		struct sk_buff *reply;
		int payload, rlen;
		int ret = 0;
		struct sk_buff *skb = NULL;

		mutex_lock(&priv->ftm.lock);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 83)
		reply = cfg80211_vendor_event_alloc(wiphy, &vif->wdev,
#else
		reply = cfg80211_vendor_event_alloc(wiphy,
#endif
						    priv->rtt_results->peer_num * sizeof(struct wifi_rtt_result1),
			RTT_EVENT_COMPLETE,
			GFP_KERNEL);
		for (int i = 0; i < priv->rtt_results->peer_num; i++) {
			nl_put_
		}

		mutex_unlock(&priv->ftm.lock);
	}
		break;
	case RTT_PER_DEST_RES:
		wl_err("rec rtt result!\n");
		res = (struct wifi_rtt_result *)data;
		sprdwl_ftm_event_per_dest_res(priv, res);
		break;
	default:
		netdev_err(vif->ndev, "%s: unknown FTM event\n", __func__);
		break;
	}
	return 0;
#endif
}

int sprdwl_ftm_get_capabilities(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				const void *data, int len)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_rtt *cmd;
	struct sprdwl_vif *vif = netdev_priv(wdev->netdev);
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	u8 rsp[RTT_RSP_LEN] = {0x0};
	u16 rsp_len = sizeof(struct sprdwl_cmd_rtt) + sizeof(struct sprdwl_rtt_capabilities);
	int ret = 0;
	struct sk_buff *skb;
	struct sprdwl_rtt_capabilities *rtt_cap;

	wl_err("rtt get capability\n");
	/* get the capabilities from the FW */
	mutex_lock(&priv->ftm.lock);
	msg = sprdwl_cmd_getbuf(vif->priv, rsp_len,
				vif->ctx_id, SPRDWL_HEAD_RSP, WIFI_CMD_RTT);
	if (!msg)
		return -ENOMEM;
	cmd = (struct sprdwl_cmd_rtt *)msg->data;
	cmd->sub_cmd = RTT_GET_CAPABILITIES;
	cmd->len = len;
	memcpy(cmd->data, data, len);

	ret = sprdwl_cmd_send_recv(vif->priv, msg,
				   CMD_WAIT_TIMEOUT, rsp, &rsp_len);
	if (ret) {
		netdev_err(vif->ndev,
			   "%s: ret=%d, rsp_len=%d\n", __func__, ret, rsp_len);
	}
	sprdwl_hex_dump("rtt cap_ori:", rsp, rsp_len);
	rtt_cap = (struct sprdwl_rtt_capabilities *)(rsp + sizeof(struct sprdwl_cmd_rtt));
	wl_err("rtt cap:%d %d %d %d %d %d %d %d\n", rtt_cap->rtt_one_sided_supported,
		rtt_cap->rtt_ftm_supported, rtt_cap->lci_support,
		rtt_cap->lcr_support, rtt_cap->preamble_support,
		rtt_cap->bw_support, rtt_cap->responder_supported,
		rtt_cap->mc_version);

	/* report capabilities */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, rsp_len/*sizeof(struct sprdwl_rtt_capabilities)*/);
	if (!skb)
		goto nla_put_failure;
	if (nla_put_nohdr(skb, sizeof(struct sprdwl_rtt_capabilities), rtt_cap))
		goto nla_put_failure;
	if (nla_put_u32(skb, NL80211_ATTR_VENDOR_ID, OUI_SPREAD))
		goto nla_put_failure;
	if (nla_put_u32(skb, NL80211_ATTR_VENDOR_SUBCMD,
			SPRD_NL80211_VENDOR_SUBCMD_LOC_GET_CAPA))
		goto nla_put_failure;
	mutex_unlock(&priv->ftm.lock);
	return cfg80211_vendor_cmd_reply(skb);
nla_put_failure:
	kfree_skb(skb);
	return -ENOMEM;
}

int sprdwl_ftm_start_session(struct wiphy *wiphy,
			     struct wireless_dev *wdev,
			     const void *data, int data_len)
{
	struct sprdwl_ftm_session_request *request;
	int rem, rem1, type, index = 0, err = 0, ret = 0;
	const struct nlattr *iter, *iter1;
	struct nlattr *cfg[SPRD_VENDOR_RTT_ATTRIBUTE_MAX + 1];
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_rtt *cmd;
	struct sprdwl_vif *vif = netdev_priv(wdev->netdev);
	u8 rsp[RTT_RSP_LEN] = {0x0};
	u16 rsp_len = RTT_RSP_LEN, cmd_data_len;
	struct sprdwl_priv *priv = wiphy_priv(wiphy);

	wl_err("rtt start\n");
	print_hex_dump(KERN_DEBUG, "conf:", DUMP_PREFIX_OFFSET, 16, 1, data, data_len, true);
	mutex_lock(&priv->ftm.lock);
	if (priv->ftm.session_started) {
		wl_err("%s, rtt already started!\n", __func__);
		return 0;
	}
	request = kzalloc(sizeof(*request) +
						RTT_MAX_CONFIG * sizeof(struct sprdwl_ftm_meas_peer_info),
							GFP_KERNEL);
	if (!request) {
		wl_err("%s, request alloc failed!\n", __func__);
		return -EAGAIN;
	}
	nla_for_each_attr(iter, data, data_len, rem) {
		type = nla_type(iter);
		switch (type) {
		case SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_CNT:
			request->n_peers = nla_get_u8(iter);
			wl_err("%s, peer num:%d, type: %d, len: %d\n", __func__, nla_get_u8(iter), nla_type(iter), nla_len(iter));
			break;
		case SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_INFO:
			nla_for_each_nested(iter1, iter, rem1) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
				err = nla_parse_nested(cfg, SPRD_VENDOR_RTT_ATTRIBUTE_MAX, iter1, sprdwl_rtt_policy, NULL);
#else
				err = nla_parse_nested(cfg, SPRD_VENDOR_RTT_ATTRIBUTE_MAX, iter1, sprdwl_rtt_policy);
#endif
				if (err) {
					kfree(request);
					return err;
				}

				if (!cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_MAC]) {
					wl_err("%s SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_MAC not found\n", __func__);
					goto cfg_found_failure;
				}
				memcpy(request->peers[index].mac_addr, nla_data(cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_MAC]), ETH_ALEN);
				printk("mac: %pM\n", request->peers[index].mac_addr);

				if (!cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_TYPE]) {
					wl_err("%sSPRD_VENDOR_ RTT_ATTRIBUTE_TARGET_TYPE not found\n", __func__);
					goto cfg_found_failure;
				}

				request->peers[index].wifi_rtt_type = nla_get_u8(cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_TYPE]);
				printk("rtt type = %d\n", request->peers[index].wifi_rtt_type);

				if (!cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_PEER]) {
					wl_err("%s SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_PEER not found\n", __func__);
					goto cfg_found_failure;
				}

				request->peers[index].rtt_peer_type = nla_get_u8(cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_PEER]);
				printk("peer_type = %d\n", request->peers[index].rtt_peer_type);

				if (!cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_CHAN]) {
					wl_err("%s SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_CHAN not found\n", __func__);
					goto cfg_found_failure;
				}

				memcpy(&request->peers[index].channel, nla_data(cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_CHAN]), sizeof(struct wifi_channel_info));
				printk("channel.width = %d\n", request->peers[index].channel.width);
				printk("channel.freq = %d\n", request->peers[index].channel.center_freq);
				printk("channel.freq0 = %d\n", request->peers[index].channel.center_freq0);
				printk("channel.freq1 = %d\n", request->peers[index].channel.center_freq1);

				if (!cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_PERIOD]) {
					wl_err("%s SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_PERIOD not found\n", __func__);
					goto cfg_found_failure;
				}
				request->peers[index].burst_period = nla_get_u32(cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_PERIOD]);
				printk("burst_period = %d\n", request->peers[index].burst_period);

				if (!cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_NUM_BURST]) {
					wl_err("%s SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_NUM_BURST not found\n", __func__);
					goto cfg_found_failure;
				}

				request->peers[index].num_burst = nla_get_u32(cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_NUM_BURST]);
				printk("num_burst = %d\n", request->peers[index].num_burst);

				if (!cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_NUM_FTM_BURST]) {
					wl_err("%s SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_NUM_FTM_BURST not found\n", __func__);
					goto cfg_found_failure;
				}
				request->peers[index].num_frames_per_burst = nla_get_u32(cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_NUM_FTM_BURST]);
				printk("num_frames_per_burst = %d\n", request->peers[index].num_frames_per_burst);

				if (!cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTM]) {
					wl_err("%s SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTM not found\n", __func__);
					goto cfg_found_failure;
				}
				request->peers[index].num_retries_per_rtt_frame = nla_get_u32(cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTM]);
				printk("num_retries_per_rtt_frame = %d\n", request->peers[index].num_retries_per_rtt_frame);

				if (!cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTMR]) {
					wl_err("%s SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTMR not found\n", __func__);
					goto cfg_found_failure;
				}
				request->peers[index].num_retries_per_ftmr = nla_get_u32(cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_NUM_RETRY_FTMR]);
				printk("num_retries_per_ftmr = %d\n", request->peers[index].num_retries_per_ftmr);

				if (!cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_LCI]) {
					wl_err("%s SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_LCI not found\n", __func__);
					goto cfg_found_failure;
				}
				request->peers[index].LCI_request = nla_get_u8(cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_LCI]);
				printk("lci_request = %d\n", request->peers[index].LCI_request);

				if (!cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_LCR]) {
					wl_err("%s SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_LCR not found\n", __func__);
					goto cfg_found_failure;
				}
				request->peers[index].LCR_request = nla_get_u8(cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_LCR]);
				printk("lcr_request = %d\n", request->peers[index].LCR_request);

				if (!cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_BURST_DURATION]) {
					wl_err("%s SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_BURST_DURATION not found\n", __func__);
					goto cfg_found_failure;
				}
				request->peers[index].burst_duration = nla_get_u32(cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_BURST_DURATION]);
				printk("burst_duration = %d\n", request->peers[index].burst_duration);

				if (!cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_PREAMBLE]) {
					wl_err("%s SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_PREAMBLE not found\n", __func__);
					goto cfg_found_failure;
				}
				request->peers[index].preamble = nla_get_u8(cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_PREAMBLE]);
				printk("rtt_preamble = %d\n", request->peers[index].preamble);

				if (!cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_BW]) {
					wl_err("%s SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_BW not found\n", __func__);
					goto cfg_found_failure;
				}
				request->peers[index].bw = nla_get_u8(cfg[SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_BW]);
				printk("rtt_bw = %d\n", request->peers[index].bw);

				index++;
			}
		}
	}

	cmd_data_len = 1 + index * sizeof(struct sprdwl_ftm_meas_peer_info);
	msg = sprdwl_cmd_getbuf(vif->priv, cmd_data_len + sizeof(struct sprdwl_cmd_rtt),
				vif->ctx_id, SPRDWL_HEAD_RSP, WIFI_CMD_RTT);
	if (!msg) {
		kfree(request);
		return -ENOMEM;
	}
	cmd = (struct sprdwl_cmd_rtt *)msg->data;
	cmd->sub_cmd = RTT_RANGE_REQUEST;
	cmd->len = cmd_data_len;
	memcpy(cmd->data, request, cmd_data_len);
	sprdwl_hex_dump("dump rtt:", cmd->data, cmd_data_len);
	kfree(request);
	request = NULL;
	ret = sprdwl_cmd_send_recv(vif->priv, msg,
				   CMD_WAIT_TIMEOUT, rsp, &rsp_len);
	if (ret) {
		netdev_err(vif->ndev,
			   "%s: ret=%d, rsp_len=%d\n", __func__, ret, rsp_len);
	}
	if (!ret) {
		priv->ftm.session_started = 1;
	}
	mutex_unlock(&priv->ftm.lock);
	return ret;
cfg_found_failure:
	kfree(request);
	return -EINVAL;
}

int sprdwl_ftm_abort_session(struct wiphy *wiphy,
			     struct wireless_dev *wdev,
			     const void *data, int len)
{
	struct sprdwl_msg_buf *msg;
	struct sprdwl_cmd_rtt *cmd;
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif = netdev_priv(wdev->netdev);
	struct sprdwl_ftm_meas_cancel_peer_info *request;
	const struct nlattr *iter;
	int ret, rem, type, index = 0, cmd_data_len;

	wl_err("rtt stop\n");

	print_hex_dump(KERN_DEBUG, "conf:", DUMP_PREFIX_OFFSET, 16, 1, data, len, true);

	mutex_lock(&priv->ftm.lock);
	if (!priv->ftm.session_started) {
		netdev_err(vif->ndev,
			   "%s: FTM session not started\n", __func__);
		return -EAGAIN;
	}

	request = kzalloc(sizeof(*request) + RTT_MAX_CONFIG * ETH_ALEN,
					GFP_KERNEL);
	nla_for_each_attr(iter, data, len, rem) {
		type = nla_type(iter);
		switch (type) {
		case SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_CNT:
			request->n_peers = nla_get_u8(iter);
			wl_err("%s, peer num:%d, type: %d, len: %d\n", __func__, nla_get_u8(iter), nla_type(iter), nla_len(iter));
			break;
		case SPRD_VENDOR_RTT_ATTRIBUTE_TARGET_MAC:
			memcpy(request->mac_addr[index], nla_data(iter), ETH_ALEN);
			printk("mac: %pM\n", request->mac_addr[index]);
			index++;
			break;
		}
	}

	/* send cancel range request */
	cmd_data_len = 1 + index * ETH_ALEN;
	msg = sprdwl_cmd_getbuf(priv, sizeof(struct sprdwl_cmd_rtt) + cmd_data_len,
				vif->ctx_id, SPRDWL_HEAD_RSP, WIFI_CMD_RTT);
	if (!msg) {
		kfree(request);
		request = NULL;
		return -ENOMEM;
	}
	cmd = (struct sprdwl_cmd_rtt *)msg->data;
	cmd->sub_cmd = RTT_RANGE_CANCEL;
	cmd->len = cmd_data_len;
	memcpy(cmd->data, request, cmd_data_len);
	kfree(request);
	request = NULL;
	ret = sprdwl_cmd_send_recv(priv, msg,
				   CMD_WAIT_TIMEOUT, NULL, 0);
	if (ret)
		netdev_err(vif->ndev, "%s: ret=%d\n", __func__, ret);
	if (!ret) {
		priv->ftm.session_started = 0;
	}
	mutex_unlock(&priv->ftm.lock);

	return ret;
	}

int sprdwl_ftm_get_responder_info(struct wiphy *wiphy,
				  struct wireless_dev *wdev,
				  const void *data, int len)
{
	struct sprdwl_vif *vif = netdev_priv(wdev->netdev);

	/* get responder info */
	netdev_info(vif->ndev, "%s: not implemented yet\n", __func__);
	return -ENOTSUPP;
}

int sprdwl_ftm_configure_responder(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data, int data_len)
{
	struct sprdwl_vif *vif = netdev_priv(wdev->netdev);

	/* enable or disable responder */
	netdev_info(vif->ndev, "%s: not implemented yet\n", __func__);
	return -ENOTSUPP;
}

void sprdwl_ftm_init(struct sprdwl_priv *priv)
{
	priv->ftm.session_started = 0;
	mutex_init(&priv->ftm.lock);
}

void sprdwl_ftm_deinit(struct sprdwl_priv *priv)
{
	kfree(priv->ftm.ftm_res);
}

#if 0
void sprdwl_ftm_stop_operations(struct sprdwl_priv *priv)
{
	sprdwl_ftm_session_ended(
		priv, SPRDWL_VENDOR_ATTR_LOC_SESSION_STATUS_ABORTED);
}
#endif

