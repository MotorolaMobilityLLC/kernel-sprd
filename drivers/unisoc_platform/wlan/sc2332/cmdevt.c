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
#include "common/acs.h"
#include "common/cfg80211.h"
#include "common/chip_ops.h"
#include "common/common.h"
#include "common/delay_work.h"
#include "common/hif.h"
#include "common/msg.h"
#include "common/report.h"
#include "common/tdls.h"
#include "scan.h"
#include "txrx.h"

unsigned int wfa_cap;
module_param(wfa_cap, uint, 0644);
MODULE_PARM_DESC(wfa_cap, "set capability for WFA test");
unsigned int dump_data;
module_param(dump_data, uint, 0644);
MODULE_PARM_DESC(dump_data, "dump data packet");

static int bss_count;

static const char *cmdevt_cmd2str(u8 cmd)
{
	switch (cmd) {
	case CMD_GET_INFO:
		return "CMD_GET_INFO";
	case CMD_SET_REGDOM:
		return "CMD_SET_REGDOM";
	case CMD_OPEN:
		return "CMD_OPEN";
	case CMD_CLOSE:
		return "CMD_CLOSE";
	case CMD_POWER_SAVE:
		return "CMD_POWER_SAVE";
	case CMD_SET_PARAM:
		return "CMD_SET_PARAM";
	case CMD_REQ_LTE_CONCUR:
		return "CMD_REQ_LTE_CONCUR";

	case CMD_CONNECT:
		return "CMD_CONNECT";

	case CMD_SCAN:
		return "CMD_SCAN";
	case CMD_SCHED_SCAN:
		return "CMD_SCHED_SCAN";
	case CMD_DISCONNECT:
		return "CMD_DISCONNECT";
	case CMD_KEY:
		return "CMD_KEY";
	case CMD_SET_PMKSA:
		return "CMD_SET_PMKSA";
	case CMD_GET_STATION:
		return "CMD_GET_STATION";
	case CMD_SET_CHANNEL:
		return "CMD_SET_CHANNEL";

	case CMD_START_AP:
		return "CMD_START_AP";
	case CMD_DEL_STATION:
		return "CMD_DEL_STATION";
	case CMD_SET_BLACKLIST:
		return "CMD_SET_BLACKLIST";
	case CMD_SET_WHITELIST:
		return "CMD_SET_WHITELIST";
	case CMD_MULTICAST_FILTER:
		return "CMD_MULTICAST_FILTER";

	case CMD_TX_MGMT:
		return "CMD_TX_MGMT";
	case CMD_REGISTER_FRAME:
		return "CMD_REGISTER_FRAME";
	case CMD_REMAIN_CHAN:
		return "CMD_REMAIN_CHAN";
	case CMD_CANCEL_REMAIN_CHAN:
		return "CMD_CANCEL_REMAIN_CHAN";

	case CMD_SET_IE:
		return "CMD_SET_IE";
	case CMD_NOTIFY_IP_ACQUIRED:
		return "CMD_NOTIFY_IP_ACQUIRED";

	case CMD_SET_CQM:
		return "CMD_SET_CQM";
	case CMD_SET_ROAM_OFFLOAD:
		return "CMD_SET_ROAM_OFFLOAD";
	case CMD_SET_MEASUREMENT:
		return "CMD_SET_MEASUREMENT";
	case CMD_SET_QOS_MAP:
		return "CMD_SET_QOS_MAP";
	case CMD_TDLS:
		return "CMD_TDLS";
	case CMD_11V:
		return "CMD_11V";
	case CMD_NPI_MSG:
		return "CMD_NPI_MSG";
	case CMD_NPI_GET:
		return "CMD_NPI_GET";

	case CMD_ASSERT:
		return "CMD_ASSERT";
	case CMD_FLUSH_SDIO:
		return "CMD_FLUSH_SDIO";
	case CMD_ADD_TX_TS:
		return "CMD_ADD_TX_TS";
	case CMD_DEL_TX_TS:
		return "CMD_DEL_TX_TS";
	case CMD_LLSTAT:
		return "CMD_LLSTAT";

	case CMD_GSCAN:
		return "CMD_GSCAN";
	case CMD_PRE_CLOSE:
		return "CMD_PRE_CLOSE";

	case CMD_SET_VOWIFI:
		return "CMD_SET_VOWIFI";
	case CMD_MIRACAST:
		return "CMD_MIRACAST";
	case CMD_MAX_STA:
		return "CMD_MAX_STA";
	case CMD_RANDOM_MAC:
		return "CMD_RANDOM_MAC";
	case CMD_PACKET_OFFLOAD:
		return "CMD_PACKET_OFFLOAD";
	case CMD_SET_SAE_PARAM:
		return "CMD_SET_SAE_PARAM";
	case CMD_EXTENDED_LLSTAT:
		return "CMD_EXTENDED_LLSTAT";
	default:
		return "CMD_UNKNOWN";
	}
}

static const char *cmdevt_err2str(s8 error)
{
	char *str = NULL;

	switch (error) {
	case SPRD_CMD_STATUS_ARG_ERROR:
		str = "SPRD_CMD_STATUS_ARG_ERROR";
		break;
	case SPRD_CMD_STATUS_GET_RESULT_ERROR:
		str = "SPRD_CMD_STATUS_GET_RESULT_ERROR";
		break;
	case SPRD_CMD_STATUS_EXEC_ERROR:
		str = "SPRD_CMD_STATUS_EXEC_ERROR";
		break;
	case SPRD_CMD_STATUS_MALLOC_ERROR:
		str = "SPRD_CMD_STATUS_MALLOC_ERROR";
		break;
	case SPRD_CMD_STATUS_WIFIMODE_ERROR:
		str = "SPRD_CMD_STATUS_WIFIMODE_ERROR";
		break;
	case SPRD_CMD_STATUS_ERROR:
		str = "SPRD_CMD_STATUS_ERROR";
		break;
	case SPRD_CMD_STATUS_CONNOT_EXEC_ERROR:
		str = "SPRD_CMD_STATUS_CONNOT_EXEC_ERROR";
		break;
	case SPRD_CMD_STATUS_NOT_SUPPORT_ERROR:
		str = "SPRD_CMD_STATUS_NOT_SUPPORT_ERROR";
		break;
	case SPRD_CMD_STATUS_OTHER_ERROR:
		str = "SPRD_CMD_STATUS_OTHER_ERROR";
		break;
	case SPRD_CMD_STATUS_OK:
		str = "CMD STATUS OK";
		break;
	default:
		str = "SPRD_CMD_STATUS_UNKNOWN_ERROR";
		break;
	}
	return str;
}

static const char *cmdevt_evt2str(u8 evt)
{
	switch (evt) {
	case EVT_CONNECT:
		return "EVT_CONNECT";
	case EVT_DISCONNECT:
		return "EVT_DISCONNECT";
	case EVT_SCAN_DONE:
		return "EVT_SCAN_DONE";
	case EVT_MGMT_FRAME:
		return "EVT_MGMT_FRAME";
	case EVT_MGMT_TX_STATUS:
		return "EVT_MGMT_TX_STATUS";
	case EVT_REMAIN_CHAN_EXPIRED:
		return "EVT_REMAIN_CHAN_EXPIRED";
	case EVT_MIC_FAIL:
		return "EVT_MIC_FAIL";
	case EVT_NEW_STATION:
		return "EVT_NEW_STATION";
	case EVT_CQM:
		return "EVT_CQM";
	case EVT_MEASUREMENT:
		return "EVT_MEASUREMENT";
	case EVT_TDLS:
		return "EVT_TDLS";
	case EVT_SDIO_SEQ_NUM:
		return "EVT_SDIO_SEQ_NUM";
	case EVT_SDIO_FLOWCON:
		return "EVT_SDIO_FLOWCON";
	case EVT_WMM_REPORT:
		return "EVT_WMM_REPORT";
	case EVT_GSCAN_FRAME:
		return "EVT_GSCAN_FRAME";
	case EVT_ACS_REPORT:
		return "EVT_ACS_REPORT";
	case EVT_ACS_LTE_CONFLICT_EVENT:
		return "EVT_ACS_LTE_CONFLICT_EVENT";
	default:
		return "WIFI_EVENT_UNKNOWN";
	}
}

static void cmdevt_set_cmd(struct sprd_cmd *cmd, struct sprd_cmd_hdr *hdr)
{
	u32 msec;
	ktime_t kt;

	kt = ktime_get();
	msec = (u32)div_u64(kt, NSEC_PER_MSEC);
	hdr->mstime = cpu_to_le32(msec);
	spin_lock_bh(&cmd->lock);
	kfree(cmd->data);
	cmd->data = NULL;
	cmd->mstime = msec;
	cmd->cmd_id = hdr->cmd_id;
	spin_unlock_bh(&cmd->lock);
}

static void cmdevt_clean_cmd(struct sprd_cmd *cmd)
{
	spin_lock_bh(&cmd->lock);
	kfree(cmd->data);
	cmd->data = NULL;
	cmd->mstime = 0;
	cmd->cmd_id = 0;
	spin_unlock_bh(&cmd->lock);
}

static int cmdevt_lock_cmd(struct sprd_cmd *cmd)
{
	if (atomic_inc_return(&cmd->refcnt) >= SPRD_CMD_EXIT_VAL) {
		atomic_dec(&cmd->refcnt);
		pr_err("%s failed\n", __func__);
		return -1;
	}
	mutex_lock(&cmd->cmd_lock);

	return 0;
}

static void cmdevt_unlock_cmd(struct sprd_cmd *cmd)
{
	mutex_unlock(&cmd->cmd_lock);
	atomic_dec(&cmd->refcnt);
}

/* if erro, data is released in this function
 * if OK, data is released
 */
static int cmdevt_send_cmd(struct sprd_priv *priv, struct sprd_msg *msg)
{
	struct sprd_cmd_hdr *hdr;
	struct sk_buff *skb;
	u8 mode;
	int ret;

	skb = msg->skb;
	hdr = (struct sprd_cmd_hdr *)skb->data;
	mode = hdr->common.mode;
	if (hdr->common.rsp)
		cmdevt_set_cmd(&priv->cmd, hdr);

	wiphy_info(priv->wiphy, "[%u]mode %d send[%s]\n",
		   le32_to_cpu(hdr->mstime), mode, cmdevt_cmd2str(hdr->cmd_id));

	if (dump_data)
		print_hex_dump_debug("CMD: ", DUMP_PREFIX_OFFSET, 16, 1,
				     ((u8 *)hdr + sizeof(*hdr)),
				     hdr->plen - sizeof(*hdr), 0);

	ret = sprd_chip_tx(&priv->chip, msg);
	if (ret) {
		pr_err("%s TX cmd Err: %d\n", __func__, ret);
		/* now cmd msg dropped */
		dev_kfree_skb(skb);
	}

	return ret;
}

static int cmdevt_recv_rsp_timeout(struct sprd_priv *priv, unsigned int timeout)
{
	int ret;
	struct sprd_cmd *cmd = &priv->cmd;

	ret = wait_for_completion_timeout(&cmd->completed,
					  msecs_to_jiffies(timeout));
	if (!ret) {
		wiphy_err(priv->wiphy, "[%s]timeout\n", cmdevt_cmd2str(cmd->cmd_id));
		return -1;
	} else if (sprd_chip_is_exit(&priv->chip) ||
		   atomic_read(&cmd->refcnt) >= SPRD_CMD_EXIT_VAL)
		return -1;

	spin_lock_bh(&cmd->lock);
	ret = cmd->data ? 0 : -1;
	spin_unlock_bh(&cmd->lock);

	return ret;
}

struct sprd_msg *sc2332_get_cmdbuf(struct sprd_priv *priv, struct sprd_vif *vif,
				   u16 len, u8 cmd_id, enum sprd_head_rsp rsp)
{
	struct sprd_msg *msg;
	struct sprd_cmd_hdr *hdr;
	u16 plen = sizeof(*hdr) + len;
	u8 mode = SPRD_MODE_NONE;

	if (!sprd_hif_is_on(&priv->hif)) {
		pr_err("%s Drop command %s in case of power off\n",
		       __func__, cmdevt_cmd2str(cmd_id));

		return NULL;
	}

	if (vif)
		mode = vif->mode;

	msg = sprd_chip_get_msg(&priv->chip, SPRD_TYPE_CMD, mode);
	if (!msg)
		return NULL;

	msg->skb = dev_alloc_skb(plen);
	if (msg->skb) {
		memset(msg->skb->data, 0, plen);
		hdr = (struct sprd_cmd_hdr *)msg->skb->data;
		hdr->common.type = SPRD_TYPE_CMD;
		hdr->common.reserv = 0;
		hdr->common.rsp = rsp;
		hdr->common.mode = mode;
		hdr->plen = cpu_to_le16(plen);
		hdr->cmd_id = cmd_id;
		sprd_fill_msg(msg, msg->skb, msg->skb->data, plen);
		msg->data = hdr + 1;
	} else {
		pr_err("%s failed to allocate skb\n", __func__);
		sprd_chip_free_msg(&priv->chip, msg);
		return NULL;
	}

	return msg;
}

/* msg is released in this function or the realy driver
 * rbuf: the msg after sprd_cmd_hdr
 * rlen: input the length of rbuf
 *       output the length of the msg,if *rlen == 0, rbuf get nothing
 */
int sc2332_send_cmd_recv_rsp(struct sprd_priv *priv, struct sprd_msg *msg,
			     u8 *rbuf, u16 *rlen, unsigned int timeout)
{
	u8 cmd_id;
	u16 plen;
	int ret = 0;
	struct sprd_cmd *cmd = &priv->cmd;
	struct sprd_cmd_hdr *hdr;
	struct sprd_hif *hif;

	hif = &priv->hif;
	if (hif->cp_asserted == 1) {
		pr_info("%s CP2 assert\n", __func__);
		sprd_chip_free_msg(&priv->chip, msg);
		return -EIO;
	}

	if (cmdevt_lock_cmd(cmd)) {
		sprd_chip_free_msg(&priv->chip, msg);
		dev_kfree_skb(msg->skb);
		if (rlen)
			*rlen = 0;

		goto out;
	}
	hdr = (struct sprd_cmd_hdr *)msg->skb->data;
	cmd_id = hdr->cmd_id;

	if (atomic_read(&priv->hif.block_cmd_after_close) == 1) {
		if (cmd_id != CMD_CLOSE) {
			pr_info("%s need block cmd after close : %s\n",
				__func__, cmdevt_cmd2str(cmd_id));
			sprd_chip_free_msg(&priv->chip, msg);
			cmdevt_unlock_cmd(cmd);
			goto out;
		}
	}

	if (atomic_read(&priv->hif.change_iface_block_cmd) == 1) {
		if (cmd_id != CMD_CLOSE && cmd_id != CMD_OPEN) {
			pr_info("%s need block cmd while change iface : %s\n",
				__func__, cmdevt_cmd2str(cmd_id));
			sprd_chip_free_msg(&priv->chip, msg);
			cmdevt_unlock_cmd(cmd);
			goto out;
		}
	}

	ret = cmdevt_send_cmd(priv, msg);
	if (ret) {
		cmdevt_unlock_cmd(cmd);

		return -1;
	}

	ret = cmdevt_recv_rsp_timeout(priv, timeout);
	if (ret != -1) {
		if (rbuf && rlen && *rlen) {
			hdr = (struct sprd_cmd_hdr *)cmd->data;
			plen = le16_to_cpu(hdr->plen) - sizeof(*hdr);
			*rlen = min(*rlen, plen);
			memcpy(rbuf, hdr->paydata, *rlen);
		}
	} else {
		wiphy_err(priv->wiphy, "mode %d [%s]rsp timeout\n",
			  hdr->common.mode, cmdevt_cmd2str(cmd_id));
	}

	cmdevt_unlock_cmd(cmd);

out:
	return ret;
}

int sc2332_cmd_scan(struct sprd_priv *priv, struct sprd_vif *vif, u32 channels,
		    int ssid_len, const u8 *ssid_list, u8 *mac_addr, u32 flags)
{
	struct sprd_msg *msg;
	struct cmd_scan *p;
	struct cmd_rsp_state_code state;
	u16 rlen, total_len = ssid_len;
	u8 *mac;

	if (flags & NL80211_SCAN_FLAG_RANDOM_ADDR)
		total_len += ETH_ALEN;
	msg = get_cmdbuf(priv, vif, sizeof(*p) + total_len, CMD_SCAN);
	if (!msg)
		return -ENOMEM;

	p = (struct cmd_scan *)msg->data;
	p->channels = channels;
	mac = p->param + ssid_len;

	if (ssid_len > 0) {
		memcpy(p->param, ssid_list, ssid_len);
		p->ssid_len = cpu_to_le16(ssid_len);
	}

	if (flags & NL80211_SCAN_FLAG_RANDOM_ADDR) {
		p->flags |= SPRD_FLAGS_SCAN_RANDOM_ADDR;
		ether_addr_copy(mac, mac_addr);
	}
	rlen = sizeof(state);
	/* FIXME, cp may return err state here */
	sc2332_send_cmd_recv_rsp(priv, msg, (u8 *)&state, &rlen,
				 CMD_SCAN_WAIT_TIMEOUT);

	return 0;
}

int sc2332_cmd_sched_scan_start(struct sprd_priv *priv, struct sprd_vif *vif,
				struct sprd_sched_scan *buf)
{
	struct sprd_msg *msg;
	struct cmd_sched_scan_hd *sscan_head = NULL;
	struct cmd_sched_scan_ie_hd *ie_head = NULL;
	struct cmd_sched_scan_ifrc *sscan_ifrc = NULL;
	u16 datalen;
	u8 *p = NULL;
	int len = 0, i, hd_len;

	datalen = sizeof(*sscan_head) + sizeof(*ie_head) + sizeof(*sscan_ifrc)
	    + buf->n_ssids * IEEE80211_MAX_SSID_LEN
	    + buf->n_match_ssids * IEEE80211_MAX_SSID_LEN + buf->ie_len;
	hd_len = sizeof(*ie_head);
	datalen = datalen + (buf->n_ssids ? hd_len : 0)
	    + (buf->n_match_ssids ? hd_len : 0)
	    + (buf->ie_len ? hd_len : 0);

	msg = get_cmdbuf(priv, vif, datalen, CMD_SCHED_SCAN);
	if (!msg)
		return -ENOMEM;

	p = msg->data;

	sscan_head = (struct cmd_sched_scan_hd *)(p + len);
	sscan_head->started = 1;
	sscan_head->buf_flags = SPRD_SCHED_SCAN_BUF_END;
	len += sizeof(*sscan_head);

	ie_head = (struct cmd_sched_scan_ie_hd *)(p + len);
	ie_head->ie_flag = SPRD_SEND_FLAG_IFRC;
	ie_head->ie_len = sizeof(*sscan_ifrc);
	len += sizeof(*ie_head);

	sscan_ifrc = (struct cmd_sched_scan_ifrc *)(p + len);

	sscan_ifrc->interval = buf->interval;
	sscan_ifrc->flags = buf->flags;
	sscan_ifrc->rssi_thold = buf->rssi_thold;
	memcpy(sscan_ifrc->chan, buf->channel, SPRD_SCHED_SCAN_CHAN_LEN);
	len += ie_head->ie_len;

	if (buf->n_ssids > 0) {
		ie_head = (struct cmd_sched_scan_ie_hd *)(p + len);
		ie_head->ie_flag = SPRD_SEND_FLAG_SSID;
		ie_head->ie_len = buf->n_ssids * IEEE80211_MAX_SSID_LEN;
		len += sizeof(*ie_head);
		for (i = 0; i < buf->n_ssids; i++) {
			memcpy((p + len + i * IEEE80211_MAX_SSID_LEN),
			       buf->ssid[i], IEEE80211_MAX_SSID_LEN);
		}
		len += ie_head->ie_len;
	}

	if (buf->n_match_ssids > 0) {
		ie_head = (struct cmd_sched_scan_ie_hd *)(p + len);
		ie_head->ie_flag = SPRD_SEND_FLAG_MSSID;
		ie_head->ie_len = buf->n_match_ssids * IEEE80211_MAX_SSID_LEN;
		len += sizeof(*ie_head);
		for (i = 0; i < buf->n_match_ssids; i++) {
			memcpy((p + len + i * IEEE80211_MAX_SSID_LEN),
			       buf->mssid[i], IEEE80211_MAX_SSID_LEN);
		}
		len += ie_head->ie_len;
	}

	if (buf->ie_len > 0) {
		ie_head = (struct cmd_sched_scan_ie_hd *)(p + len);
		ie_head->ie_flag = SPRD_SEND_FLAG_IE;
		ie_head->ie_len = buf->ie_len;
		len += sizeof(*ie_head);

		wiphy_info(priv->wiphy, "%s: ie len is %zu\n",
			   __func__, buf->ie_len);
		wiphy_info(priv->wiphy, "ie:%s", buf->ie);
		memcpy((p + len), buf->ie, buf->ie_len);
		len += ie_head->ie_len;
	}

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_cmd_sched_scan_stop(struct sprd_priv *priv, struct sprd_vif *vif)
{
	struct sprd_msg *msg;
	struct cmd_sched_scan_hd *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_SCHED_SCAN);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_sched_scan_hd *)msg->data;
	p->started = 0;
	p->buf_flags = SPRD_SCHED_SCAN_BUF_END;

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_set_gscan_config(struct sprd_priv *priv, struct sprd_vif *vif,
			    void *data, u16 len, u8 *r_buf, u16 *r_len)
{
	struct sprd_msg *msg;
	struct cmd_gscan_header *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p) + len, CMD_GSCAN);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_gscan_header *)
	    (msg->skb->data + sizeof(struct sprd_cmd_hdr));
	p->subcmd = SPRD_GSCAN_SUBCMD_SET_CONFIG;
	p->data_len = len;
	memcpy(p->data, data, len);
	return send_cmd_recv_rsp(priv, msg, r_buf, r_len);
}

int sc2332_set_gscan_scan_config(struct sprd_priv *priv, struct sprd_vif *vif,
				 void *data, u16 len, u8 *r_buf, u16 *r_len)
{
	struct sprd_msg *msg;
	struct cmd_gscan_header *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p) + len, CMD_GSCAN);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_gscan_header *)(msg->skb->data +
					sizeof(struct sprd_cmd_hdr));
	p->subcmd = SPRD_GSCAN_SUBCMD_SET_SCAN_CONFIG;
	p->data_len = len;
	memcpy(p->data, data, len);
	return send_cmd_recv_rsp(priv, msg, r_buf, r_len);
}

int sc2332_set_max_sta(struct sprd_priv *priv,
		       struct sprd_vif *vif, unsigned char max_sta)
{
	struct sprd_msg *msg;
	struct cmd_max_sta *p = NULL;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_MAX_STA);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_max_sta *)msg->data;
	p->max_sta = max_sta;

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_enable_miracast(struct sprd_priv *priv, struct sprd_vif *vif,
			   int val)
{
	struct sprd_msg *msg;
	struct cmd_miracast *p = NULL;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_MIRACAST);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_miracast *)msg->data;
	p->value = val;

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_enable_gscan(struct sprd_priv *priv, struct sprd_vif *vif,
			void *data, u8 *r_buf, u16 *r_len)
{
	struct sprd_msg *msg;
	struct cmd_gscan_header *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p) + sizeof(int), CMD_GSCAN);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_gscan_header *)
	    (msg->skb->data + sizeof(struct sprd_cmd_hdr));
	p->subcmd = SPRD_GSCAN_SUBCMD_ENABLE_GSCAN;
	p->data_len = sizeof(int);
	memcpy(p->data, data, p->data_len);
	return send_cmd_recv_rsp(priv, msg, r_buf, r_len);
}

int sc2332_get_gscan_capabilities(struct sprd_priv *priv, struct sprd_vif *vif,
				  u8 *r_buf, u16 *r_len)
{
	struct sprd_msg *msg;
	struct cmd_gscan_header *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_GSCAN);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_gscan_header *)
	    (msg->skb->data + sizeof(struct sprd_cmd_hdr));
	p->subcmd = SPRD_GSCAN_SUBCMD_GET_CAPABILITIES;
	p->data_len = 0;

	return send_cmd_recv_rsp(priv, msg, r_buf, r_len);
}

int sc2332_get_gscan_channel_list(struct sprd_priv *priv, struct sprd_vif *vif,
				  void *data, u8 *r_buf, u16 *r_len)
{
	struct sprd_msg *msg;
	int *band;
	struct cmd_gscan_header *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p) + sizeof(*band), CMD_GSCAN);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_gscan_header *)
	    (msg->skb->data + sizeof(struct sprd_cmd_hdr));
	p->subcmd = SPRD_GSCAN_SUBCMD_GET_CHANNEL_LIST;
	p->data_len = sizeof(*band);

	band = (int *)(msg->skb->data + sizeof(struct sprd_cmd_hdr) +
		       sizeof(struct cmd_gscan_header));

	*band = *((int *)data);
	return send_cmd_recv_rsp(priv, msg, r_buf, r_len);
}

int sc2332_set_packet_offload(struct sprd_priv *priv, struct sprd_vif *vif,
			      u32 req, u8 enable, u32 interval,
			      u32 len, u8 *data)
{
	struct sprd_msg *msg;
	struct cmd_packet_offload *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p) + len, CMD_PACKET_OFFLOAD);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_packet_offload *)msg->data;

	p->enable = enable;
	p->req_id = req;
	if (enable) {
		p->period = interval;
		p->len = len;
		memcpy(p->data, data, len);
	}

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_extended_llstate(struct sprd_priv *priv, struct sprd_vif *vif,
			    u8 type, u8 subtype, void *buf, u8 len, u8 *r_buf,
			    u16 *r_len)
{
	struct sprd_msg *msg;
	struct cmd_extended_llstate *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_EXTENDED_LLSTAT);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_extended_llstate *)msg->data;
	p->type = type;
	p->subtype = subtype;
	p->len = len;
	memcpy(p->data, buf, len);

	if (type == SUBCMD_SET)
		return send_cmd_recv_rsp(priv, msg, 0, 0);
	else
		return send_cmd_recv_rsp(priv, msg, r_buf, r_len);
}

void sc2332_cmd_init(struct sprd_cmd *cmd)
{
	/* memset(cmd, 0, sizeof(*cmd)); */
	cmd->data = NULL;
	spin_lock_init(&cmd->lock);
	mutex_init(&cmd->cmd_lock);
	init_completion(&cmd->completed);
	cmd->init_ok = 1;
}

void sc2332_cmd_deinit(struct sprd_cmd *cmd)
{
	unsigned long timeout;

	atomic_add(SPRD_CMD_EXIT_VAL, &cmd->refcnt);
	if (!completion_done(&cmd->completed))
		complete_all(&cmd->completed);

	timeout = jiffies + msecs_to_jiffies(1000);
	while (atomic_read(&cmd->refcnt) > SPRD_CMD_EXIT_VAL) {
		if (time_after(jiffies, timeout)) {
			pr_err("%s cmd lock timeout\n", __func__);
			break;
		}
		usleep_range(2000, 2500);
	}
	cmdevt_clean_cmd(cmd);
	mutex_destroy(&cmd->cmd_lock);
}

int sc2332_get_fw_info(struct sprd_priv *priv)
{
	int ret;
	struct sprd_msg *msg;
	struct cmd_fw_info *p;
	struct cmd_get_fw_info *pcmd;
	u16 r_len = sizeof(*p);
	u8 r_buf[sizeof(*p)];

	msg = get_cmdbuf(priv, NULL, sizeof(*pcmd), CMD_GET_INFO);
	if (!msg)
		return -ENOMEM;

	pcmd = (struct cmd_get_fw_info *)msg->data;
	pcmd->early_rsp = SPRD_EARLY_RSP9_0;
	wiphy_info(priv->wiphy, "send CMD_GET_INFO, early_rsp = %d",
		   SPRD_EARLY_RSP9_0);
	ret = send_cmd_recv_rsp(priv, msg, r_buf, &r_len);
	if (!ret && r_len) {
		p = (struct cmd_fw_info *)r_buf;
		priv->chip_model = p->chip_model;
		priv->chip_ver = p->chip_version;
		priv->fw_ver = p->fw_version;
		priv->fw_capa = p->fw_capa;
		priv->fw_std = p->fw_std;
		priv->extend_feature = p->extend_feature;
		priv->max_ap_assoc_sta = p->max_ap_assoc_sta;
		priv->max_acl_mac_addrs = p->max_acl_mac_addrs;
		priv->max_mc_mac_addrs = p->max_mc_mac_addrs;
		priv->wnm_ft_support = p->wnm_ft_support;
		wiphy_info(priv->wiphy, "chip_model:0x%x, chip_ver:0x%x\n",
			   priv->chip_model, priv->chip_ver);
		wiphy_info(priv->wiphy,
			   "fw_ver:%d, fw_std:0x%x, fw_capa:0x%x\n",
			   priv->fw_ver, priv->fw_std, priv->fw_capa);
	}

	return ret;
}

int sc2332_set_regdom(struct sprd_priv *priv, u8 *regdom, u32 len)
{
	struct sprd_msg *msg;
	struct sprd_ieee80211_regdomain *p;

	msg = get_cmdbuf(priv, NULL, len, CMD_SET_REGDOM);
	if (!msg)
		return -ENOMEM;
	p = (struct sprd_ieee80211_regdomain *)msg->data;
	memcpy(p, regdom, len);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_open_fw(struct sprd_priv *priv, struct sprd_vif *vif, u8 *mac_addr)
{
	struct sprd_msg *msg;
	struct cmd_open *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_OPEN);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_open *)msg->data;
	p->mode = vif->mode;
	if (mac_addr)
		memcpy(&p->mac[0], mac_addr, sizeof(p->mac));

	if (vif->mode == SPRD_MODE_STATION) {
		p->reserved = wfa_cap;
		pr_info("%s wfa_cap = %d\n", __func__, wfa_cap);
	} else {
		p->reserved = 0;
	}

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_close_fw(struct sprd_priv *priv, struct sprd_vif *vif)
{
	struct sprd_msg *msg;
	struct cmd_close *p;

	/* workaround for this case: the time is too short between del_station
	 * and close command, driver need delay 100ms. more info please
	 * refer Bug:1362522
	 */
	if (vif->mode == SPRD_MODE_AP || vif->mode == SPRD_MODE_P2P_GO)
		msleep(100);
	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_CLOSE);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_close *)msg->data;
	p->mode = vif->mode;

	send_cmd_recv_rsp(priv, msg, NULL, NULL);
	/* FIXME - in case of close failure */
	return 0;
}

int sc2332_power_save(struct sprd_priv *priv, struct sprd_vif *vif,
		      u8 sub_type, u8 status)
{
	struct sprd_msg *msg;
	struct cmd_power_save *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_POWER_SAVE);
	if (!msg)
		return -ENOMEM;

	p = (struct cmd_power_save *)msg->data;
	p->sub_type = sub_type;
	p->value = status;

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_set_sar(struct sprd_priv *priv, struct sprd_vif *vif,
		 u8 sub_type, s8 value)
{
	struct sprd_msg *msg;
	struct cmd_set_sar *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_POWER_SAVE);
	if (!msg)
		return -ENOMEM;

	p = (struct cmd_set_sar *)msg->data;
	p->power_save_type = SPRD_SET_SAR;
	p->sub_type = sub_type;
	p->value = value;
	p->mode = SPRD_SET_SAR_ALL_MODE;
	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_set_power_backoff(struct sprd_priv *priv, struct sprd_vif *vif,
			     u8 sub_type, s8 value, u8 mode, u8 channel)
{
	struct sprd_msg *msg;
	struct cmd_set_power_backoff *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_POWER_SAVE);
	if (!msg)
		return -ENOMEM;

	p = (struct cmd_set_power_backoff *)msg->data;
	p->power_save_type = SPRD_SET_POWER_BACKOFF;
	p->sub_type = sub_type;
	p->value = value;
	p->mode = mode;
	p->channel = channel;
	pr_err("sub_type:%d, value : %d, mode : %d, channel:%d\n",
		sub_type, value, mode, channel);
	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_add_key(struct sprd_priv *priv, struct sprd_vif *vif,
		   const u8 *key_data, u8 key_len, bool pairwise, u8 key_index,
		   const u8 *key_seq, u8 cypher_type, const u8 *mac_addr)
{
	struct sprd_msg *msg;
	struct cmd_add_key *p;
	u8 *sub_cmd;
	int datalen = sizeof(*p) + sizeof(*sub_cmd) + key_len;

	msg = get_cmdbuf(priv, vif, datalen, CMD_KEY);
	if (!msg)
		return -ENOMEM;

	sub_cmd = (u8 *)msg->data;
	*sub_cmd = SUBCMD_ADD;
	p = (struct cmd_add_key *)(++sub_cmd);

	p->key_index = key_index;
	p->pairwise = (u8)pairwise;
	p->cypher_type = cypher_type;
	p->key_len = key_len;
	if (key_seq)
		memcpy(p->keyseq, key_seq, SPRD_KEY_SEQ_LEN);
	if (mac_addr)
		ether_addr_copy(p->mac, mac_addr);
	if (key_data)
		memcpy(p->value, key_data, key_len);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_del_key(struct sprd_priv *priv, struct sprd_vif *vif, u8 key_index,
		   bool pairwise, const u8 *mac_addr)
{
	struct sprd_msg *msg;
	struct cmd_del_key *p;
	u8 *sub_cmd;

	msg = get_cmdbuf(priv, vif, sizeof(*p) + sizeof(*sub_cmd), CMD_KEY);
	if (!msg)
		return -ENOMEM;

	sub_cmd = (u8 *)msg->data;
	*sub_cmd = SUBCMD_DEL;
	p = (struct cmd_del_key *)(++sub_cmd);

	p->key_index = key_index;
	p->pairwise = (u8)pairwise;
	if (mac_addr)
		ether_addr_copy(p->mac, mac_addr);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_set_def_key(struct sprd_priv *priv, struct sprd_vif *vif,
		       u8 key_index)
{
	struct sprd_msg *msg;
	struct cmd_set_def_key *p;
	u8 *sub_cmd;

	msg = get_cmdbuf(priv, vif, sizeof(*p) + sizeof(*sub_cmd), CMD_KEY);
	if (!msg)
		return -ENOMEM;

	sub_cmd = (u8 *)msg->data;
	*sub_cmd = SUBCMD_SET;
	p = (struct cmd_set_def_key *)(++sub_cmd);

	p->key_index = key_index;

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

static int cmdevt_set_ie(struct sprd_priv *priv, struct sprd_vif *vif, u8 type,
			 const u8 *ie, u16 len)
{
	struct sprd_msg *msg;
	struct cmd_set_ie *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p) + len, CMD_SET_IE);
	if (!msg)
		return -ENOMEM;

	p = (struct cmd_set_ie *)msg->data;
	p->type = type;
	p->len = len;
	memcpy(p->data, ie, len);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_set_beacon_ie(struct sprd_priv *priv, struct sprd_vif *vif,
			 const u8 *ie, u16 len)
{
	return cmdevt_set_ie(priv, vif, SPRD_IE_BEACON, ie, len);
}

int sc2332_set_probereq_ie(struct sprd_priv *priv, struct sprd_vif *vif,
			   const u8 *ie, u16 len)
{
	return cmdevt_set_ie(priv, vif, SPRD_IE_PROBE_REQ, ie, len);
}

int sc2332_set_proberesp_ie(struct sprd_priv *priv, struct sprd_vif *vif,
			    const u8 *ie, u16 len)
{
	return cmdevt_set_ie(priv, vif, SPRD_IE_PROBE_RESP, ie, len);
}

int sc2332_set_assocreq_ie(struct sprd_priv *priv, struct sprd_vif *vif,
			   const u8 *ie, u16 len)
{
	return cmdevt_set_ie(priv, vif, SPRD_IE_ASSOC_REQ, ie, len);
}

int sc2332_set_assocresp_ie(struct sprd_priv *priv, struct sprd_vif *vif,
			    const u8 *ie, u16 len)
{
	return cmdevt_set_ie(priv, vif, SPRD_IE_ASSOC_RESP, ie, len);
}

int sc2332_set_sae_ie(struct sprd_priv *priv, struct sprd_vif *vif,
		      const u8 *ie, u16 len)
{
	return cmdevt_set_ie(priv, vif, SPRD_IE_SAE, ie, len);
}

int sc2332_start_ap(struct sprd_priv *priv, struct sprd_vif *vif, u8 *beacon,
		    u16 len, struct cfg80211_ap_settings *settings)
{
	struct sprd_msg *msg;
	struct cmd_start_ap *p;
	u16 datalen = sizeof(*p) + len;
	struct ieee80211_channel *ch;
	struct cfg80211_chan_def chandef;
	u16 freq = 0;
	int ret;

	if (settings->chandef.chan) {
		freq = settings->chandef.chan->center_freq;
		netdev_info(vif->ndev, "%s freq : %d\n", __func__, freq);
	} else {
		netdev_err(vif->ndev, "%s can not get channel info\n", __func__);
	}

	msg = get_cmdbuf(priv, vif, datalen, CMD_START_AP);
	if (!msg)
		return -ENOMEM;

	p = (struct cmd_start_ap *)msg->data;
	p->len = cpu_to_le16(len);
	memcpy(p->value, beacon, len);

	ret = send_cmd_recv_rsp(priv, msg, NULL, NULL);

	/* need report channel info to uplayer to pass cts test */
	ch = ieee80211_get_channel(priv->wiphy, freq);
	if (ch) {
		cfg80211_chandef_create(&chandef, ch, NL80211_CHAN_HT20);
		cfg80211_ch_switch_notify(vif->ndev, &chandef);
	} else {
		netdev_err(vif->ndev, "%s, ch is null!\n", __func__);
	}

	return ret;
}

int sc2332_del_station(struct sprd_priv *priv, struct sprd_vif *vif,
		       const u8 *mac_addr, u16 reason_code)
{
	struct sprd_msg *msg;
	struct cmd_del_station *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_DEL_STATION);
	if (!msg)
		return -ENOMEM;

	p = (struct cmd_del_station *)msg->data;
	if (mac_addr)
		memcpy(&p->mac[0], mac_addr, sizeof(p->mac));
	p->reason_code = cpu_to_le16(reason_code);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_get_station(struct sprd_priv *priv, struct sprd_vif *vif,
		       struct sprd_sta_info *sta)
{
	struct sprd_msg *msg;
	struct cmd_get_station get_sta;
	u8 *r_buf = (u8 *)&get_sta;
	u16 r_len = sizeof(get_sta);
	int ret;

	msg = get_cmdbuf(priv, vif, 0, CMD_GET_STATION);
	if (!msg)
		return -ENOMEM;

	ret = send_cmd_recv_rsp(priv, msg, r_buf, &r_len);

	sta->tx_rate = get_sta.tx_rate;
	sta->rx_rate = get_sta.rx_rate;
	sta->signal = get_sta.signal;
	sta->noise = get_sta.noise;
	sta->txfailed = get_sta.txfailed;

	return ret;
}

int sc2332_set_channel(struct sprd_priv *priv, struct sprd_vif *vif, u8 channel)
{
	struct sprd_msg *msg;
	struct cmd_set_channel *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_SET_CHANNEL);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_set_channel *)msg->data;
	p->channel = channel;

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_connect(struct sprd_priv *priv, struct sprd_vif *vif,
		   struct cmd_connect *p)
{
	struct sprd_msg *msg;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_CONNECT);
	if (!msg)
		return -ENOMEM;

	memcpy(msg->data, p, sizeof(*p));

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_disconnect(struct sprd_priv *priv, struct sprd_vif *vif,
		      u16 reason_code)
{
	struct sprd_msg *msg;
	struct cmd_disconnect *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_DISCONNECT);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_disconnect *)msg->data;
	p->reason_code = cpu_to_le16(reason_code);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_set_param(struct sprd_priv *priv, u32 rts, u32 frag)
{
	struct sprd_msg *msg;
	struct cmd_set_param *p;

	msg = get_cmdbuf(priv, NULL, sizeof(*p), CMD_SET_PARAM);
	if (!msg)
		return -ENOMEM;

	p = (struct cmd_set_param *)msg->data;
	p->rts = cpu_to_le16((u16)rts);
	p->frag = cpu_to_le16((u16)frag);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_pmksa(struct sprd_priv *priv, struct sprd_vif *vif, const u8 *bssid,
		 const u8 *pmkid, u8 type)
{
	struct sprd_msg *msg;
	struct cmd_pmkid *p;
	u8 *sub_cmd;

	msg = get_cmdbuf(priv, vif, sizeof(*p) + sizeof(*sub_cmd),
			 CMD_SET_PMKSA);
	if (!msg)
		return -ENOMEM;

	sub_cmd = (u8 *)msg->data;
	*sub_cmd = type;
	p = (struct cmd_pmkid *)(++sub_cmd);

	if (bssid)
		memcpy(p->bssid, bssid, sizeof(p->bssid));
	if (pmkid)
		memcpy(p->pmkid, pmkid, sizeof(p->pmkid));

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_set_qos_map(struct sprd_priv *priv, struct sprd_vif *vif, void *map)
{
	struct sprd_msg *msg;
	struct cmd_qos_map *p;

	if (!map)
		return 0;
	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_SET_QOS_MAP);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_qos_map *)
	    (msg->skb->data + sizeof(struct sprd_cmd_hdr));
	memset((u8 *)p, 0, sizeof(*p));
	memcpy((u8 *)p, map, sizeof(*p));

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_add_tx_ts(struct sprd_priv *priv, struct sprd_vif *vif, u8 tsid,
		     const u8 *peer, u8 user_prio, u16 admitted_time)
{
	struct sprd_msg *msg;
	struct cmd_tx_ts *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_ADD_TX_TS);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_tx_ts *)
	    (msg->skb->data + sizeof(struct sprd_cmd_hdr));
	memset((u8 *)p, 0, sizeof(*p));

	p->tsid = tsid;
	memcpy(p->peer, peer, ETH_ALEN);
	p->user_prio = user_prio;
	p->admitted_time = cpu_to_le16(admitted_time);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_del_tx_ts(struct sprd_priv *priv, struct sprd_vif *vif, u8 tsid,
		     const u8 *peer)
{
	struct sprd_msg *msg;
	struct cmd_tx_ts *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_DEL_TX_TS);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_tx_ts *)
	    (msg->skb->data + sizeof(struct sprd_cmd_hdr));
	memset((u8 *)p, 0, sizeof(*p));

	p->tsid = tsid;
	memcpy(p->peer, peer, ETH_ALEN);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_remain_chan(struct sprd_priv *priv, struct sprd_vif *vif,
		       struct ieee80211_channel *channel,
		       enum nl80211_channel_type channel_type,
		       u32 duration, u64 *cookie)
{
	struct sprd_msg *msg;
	struct cmd_remain_chan *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_REMAIN_CHAN);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_remain_chan *)msg->data;
	p->chan = ieee80211_frequency_to_channel(channel->center_freq);
	p->chan_type = channel_type;
	p->duraion = cpu_to_le32(duration);
	p->cookie = cpu_to_le64(*cookie);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_cancel_remain_chan(struct sprd_priv *priv, struct sprd_vif *vif,
			      u64 cookie)
{
	struct sprd_msg *msg;
	struct cmd_cancel_remain_chan *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_CANCEL_REMAIN_CHAN);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_cancel_remain_chan *)msg->data;
	p->cookie = cpu_to_le64(cookie);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_tx_mgmt(struct sprd_priv *priv, struct sprd_vif *vif, u8 channel,
		   u8 dont_wait_for_ack, u32 wait, u64 *cookie,
		   const u8 *buf, size_t len)
{
	struct sprd_msg *msg;
	struct cmd_mgmt_tx *p;
	u16 datalen = sizeof(*p) + len;

	msg = get_cmdbuf(priv, vif, datalen, CMD_TX_MGMT);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_mgmt_tx *)msg->data;

	p->chan = channel;
	p->dont_wait_for_ack = dont_wait_for_ack;
	p->wait = cpu_to_le32(wait);
	if (cookie)
		p->cookie = cpu_to_le64(*cookie);
	p->len = cpu_to_le16(len);
	memcpy(p->value, buf, len);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_register_frame(struct sprd_priv *priv, struct sprd_vif *vif,
			  u16 type, u8 reg)
{
	struct sprd_msg *msg;
	struct cmd_register_frame *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_REGISTER_FRAME);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_register_frame *)msg->data;
	p->type = type;
	p->reg = reg;

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_set_cqm_rssi(struct sprd_priv *priv, struct sprd_vif *vif,
			s32 rssi_thold, u32 rssi_hyst)
{
	struct sprd_msg *msg;
	struct cmd_cqm_rssi *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_SET_CQM);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_cqm_rssi *)msg->data;
	p->rssih = cpu_to_le32(rssi_thold);
	p->rssil = cpu_to_le32(rssi_hyst);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_set_roam_offload(struct sprd_priv *priv, struct sprd_vif *vif,
			    u8 sub_type, const u8 *data, u8 len)
{
	struct sprd_msg *msg;
	struct cmd_roam_offload_data *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p) + len, CMD_SET_ROAM_OFFLOAD);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_roam_offload_data *)msg->data;
	p->type = sub_type;
	p->len = len;
	memcpy(p->value, data, len);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_tdls_mgmt(struct sprd_vif *vif, struct sk_buff *skb)
{
	struct sprd_msg *msg;
	u8 type;
	u8 ret;
	unsigned int len;

	msg = sprd_chip_get_msg(&vif->priv->chip, SPRD_TYPE_DATA, vif->mode);
	if (!msg) {
		vif->ndev->stats.tx_fifo_errors++;
		return -NETDEV_TX_BUSY;
	}
	type = SPRD_DATA_TYPE_NORMAL;
	/* temp debug use */
	if (skb_headroom(skb) < vif->ndev->needed_headroom)
		wiphy_err(vif->priv->wiphy, "%s skb head len err:%d %d\n",
			  __func__, skb_headroom(skb),
			  vif->ndev->needed_headroom);
	len = skb->len;
	if (dump_data)
		print_hex_dump_debug("TX packet: ", DUMP_PREFIX_OFFSET,
				     16, 1, skb->data, len, 0);
	/* sprd_send_data: offset use 2 for cp bytes align */
	ret = sprd_send_data(vif->priv, vif, msg, skb, type, SPRD_DATA_OFFSET,
			     false);
	if (ret) {
		wiphy_err(vif->priv->wiphy, "%s drop msg due to TX Err\n",
			  __func__);
		goto out;
	}

	vif->ndev->stats.tx_bytes += len;
	vif->ndev->stats.tx_packets++;
out:
	return ret;
}

int sc2332_tdls_oper(struct sprd_priv *priv, struct sprd_vif *vif,
		     const u8 *peer, int oper)
{
	struct sprd_msg *msg;
	struct cmd_tdls *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_TDLS);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_tdls *)msg->data;
	if (peer)
		ether_addr_copy(p->da, peer);
	p->tdls_sub_cmd_mgmt = oper;

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_start_tdls_channel_switch(struct sprd_priv *priv,
				     struct sprd_vif *vif, const u8 *peer_mac,
				     u8 primary_chan, u8 second_chan_offset,
				     u8 band)
{
	struct sprd_msg *msg;
	struct cmd_tdls *p;
	struct cmd_tdls_channel_switch chan_switch;

	msg = get_cmdbuf(priv, vif, sizeof(*p) + sizeof(chan_switch), CMD_TDLS);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_tdls *)msg->data;
	p->tdls_sub_cmd_mgmt = SPRD_TDLS_START_CHANNEL_SWITCH;
	if (peer_mac)
		ether_addr_copy(p->da, peer_mac);
	p->initiator = 1;
	chan_switch.primary_chan = primary_chan;
	chan_switch.second_chan_offset = second_chan_offset;
	chan_switch.band = band;
	p->paylen = sizeof(chan_switch);
	memcpy(p->payload, &chan_switch, p->paylen);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_cancel_tdls_channel_switch(struct sprd_priv *priv,
				      struct sprd_vif *vif, const u8 *peer_mac)
{
	struct sprd_msg *msg;
	struct cmd_tdls *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_TDLS);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_tdls *)msg->data;
	p->tdls_sub_cmd_mgmt = SPRD_TDLS_CANCEL_CHANNEL_SWITCH;
	if (peer_mac)
		ether_addr_copy(p->da, peer_mac);
	p->initiator = 1;

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_notify_ip(struct sprd_priv *priv, struct sprd_vif *vif, u8 ip_type,
		     u8 *ip_addr)
{
	struct sprd_msg *msg;
	u8 *ip_value;
	u8 ip_len;

	if (ip_type != SPRD_IPV4 && ip_type != SPRD_IPV6)
		return -EINVAL;
	ip_len = (ip_type == SPRD_IPV4) ?
	    SPRD_IPV4_ADDR_LEN : SPRD_IPV6_ADDR_LEN;
	msg = get_cmdbuf(priv, vif, ip_len, CMD_NOTIFY_IP_ACQUIRED);
	if (!msg)
		return -ENOMEM;
	ip_value = (unsigned char *)msg->data;
	memcpy(ip_value, ip_addr, ip_len);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_set_blacklist(struct sprd_priv *priv,
			 struct sprd_vif *vif, u8 sub_type, u8 num,
			 u8 *mac_addr)
{
	struct sprd_msg *msg;
	struct cmd_blacklist *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p) + num * ETH_ALEN,
			 CMD_SET_BLACKLIST);
	if (!msg)
		return -ENOMEM;

	p = (struct cmd_blacklist *)msg->data;
	p->sub_type = sub_type;
	p->num = num;
	if (mac_addr)
		memcpy(p->mac, mac_addr, num * ETH_ALEN);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_set_whitelist(struct sprd_priv *priv, struct sprd_vif *vif,
			 u8 sub_type, u8 num, u8 *mac_addr)
{
	struct sprd_msg *msg;
	struct cmd_set_mac_addr *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p) + num * ETH_ALEN,
			 CMD_SET_WHITELIST);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_set_mac_addr *)msg->data;
	p->sub_type = sub_type;
	p->num = num;
	if (mac_addr)
		memcpy(p->mac, mac_addr, num * ETH_ALEN);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_set_mc_filter(struct sprd_priv *priv, struct sprd_vif *vif,
			 u8 sub_type, u8 num, u8 *mac_addr)
{
	struct sprd_msg *msg;
	struct cmd_set_mac_addr *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p) + num * ETH_ALEN,
			 CMD_MULTICAST_FILTER);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_set_mac_addr *)msg->data;
	p->sub_type = sub_type;
	p->num = num;
	if (num && mac_addr)
		memcpy(p->mac, mac_addr, num * ETH_ALEN);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_npi_send_recv(struct sprd_priv *priv, struct sprd_vif *vif,
			 u8 *s_buf, u16 s_len, u8 *r_buf, u16 *r_len)
{
	struct sprd_msg *msg;

	msg = get_cmdbuf(priv, vif, s_len, CMD_NPI_MSG);
	if (!msg)
		return -ENOMEM;
	memcpy(msg->data, s_buf, s_len);

	return send_cmd_recv_rsp(priv, msg, r_buf, r_len);
}

int sc2332_set_11v_feature_support(struct sprd_priv *priv, struct sprd_vif *vif,
				   u16 val)
{
	struct sprd_msg *msg = NULL;
	struct cmd_rsp_state_code state;
	struct cmd_11v *p = NULL;
	u16 rlen = sizeof(state);

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_11V);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_11v *)msg->data;

	p->cmd = SUBCMD_SET;
	p->value = (val << 16) | val;
	/* len  only 8 =  cmd(2) + len(2) +value(4) */
	p->len = 8;

	return send_cmd_recv_rsp(priv, msg, (u8 *)&state, &rlen);
}

int sc2332_set_11v_sleep_mode(struct sprd_priv *priv, struct sprd_vif *vif,
			      u8 status, u16 interval)
{
	struct sprd_msg *msg = NULL;
	struct cmd_rsp_state_code state;
	struct cmd_11v *p = NULL;
	u16 rlen = sizeof(state);
	u32 value = 0;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_11V);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_11v *)msg->data;

	p->cmd = SUBCMD_ENABLE;
	/* 24-31 feature 16-23 status 0-15 interval */
	value = SPRD_11V_SLEEP << 8;
	value = (value | status) << 16;
	value = value | interval;
	p->value = value;
	/* len =  cmd(2) + len(2) +value(4) = 8 */
	p->len = 8;

	return send_cmd_recv_rsp(priv, msg, (u8 *)&state, &rlen);
}

static int cmdevt_send_data_by_cmd(struct sprd_priv *priv, struct sprd_vif *vif,
				   u8 channel, u8 dont_wait_for_ack, u32 wait,
				   u64 *cookie, const u8 *buf, size_t len)
{
	struct sprd_msg *msg;
	struct cmd_mgmt_tx *p;
	u16 datalen = sizeof(*p) + len;

	msg = sc2332_get_cmdbuf(priv, vif, datalen, CMD_TX_MGMT,
				SPRD_HEAD_NORSP);
	if (!msg)
		return -ENOMEM;
	p = (struct cmd_mgmt_tx *)msg->data;

	p->chan = channel;
	p->dont_wait_for_ack = dont_wait_for_ack;
	p->wait = cpu_to_le32(wait);
	if (cookie)
		p->cookie = cpu_to_le64(*cookie);
	p->len = cpu_to_le16(len);
	memcpy(p->value, buf, len);

	return cmdevt_send_cmd(priv, msg);
}

int sc2332_xmit_data2cmd(struct sk_buff *skb, struct net_device *ndev)
{
	/* default set channel to 0
	 * frequency information:
	 * GC/STA: wdev->current_bss->pub.channel
	 * GO/SotfAP: wdev->channel
	 */
	u8 channel = 0;
	int ret;
	struct ethhdr ehdr;
	struct ieee80211_hdr_3addr *hdr;
	struct llc_hdr *llc;
	struct sprd_vif *vif = netdev_priv(ndev);
	struct sk_buff *tmp = skb;
	unsigned int extra =
	    sizeof(struct ieee80211_hdr_3addr) +
	    sizeof(struct llc_hdr) - sizeof(struct ethhdr);

	if (!ndev || !ndev->ieee80211_ptr) {
		pr_err("%s can not get channel\n", __func__);
		return -EINVAL;
	}
	if (vif->mode == SPRD_MODE_P2P_GO || vif->mode == SPRD_MODE_AP)
		channel = ndev->ieee80211_ptr->chandef.chan->hw_value;

	memcpy(&ehdr, skb->data, sizeof(struct ethhdr));
	/* 802.3 to 802.11 */
	skb = skb_realloc_headroom(tmp, extra);
	dev_kfree_skb(tmp);
	if (!skb) {
		netdev_err(ndev, "%s realloc failed\n", __func__);
		return NETDEV_TX_BUSY;
	}
	skb_push(skb, extra);

	hdr = (struct ieee80211_hdr_3addr *)skb->data;
	/* data type:to ds */
	hdr->frame_control = 0x0208;
	hdr->duration_id = 0x00;
	memcpy(hdr->addr1, ehdr.h_dest, ETH_ALEN);
	memcpy(hdr->addr2, ehdr.h_source, ETH_ALEN);
	memcpy(hdr->addr3, ehdr.h_source, ETH_ALEN);
	hdr->seq_ctrl = 0x00;

	llc = (struct llc_hdr *)
	    ((u8 *)skb->data + sizeof(struct ieee80211_hdr_3addr));
	llc->dsap = 0xAA;
	llc->ssap = 0xAA;
	llc->cntl = 0x03;
	memset(llc->org_code, 0x0, sizeof(llc->org_code));
	llc->eth_type = ehdr.h_proto;
	/* send 80211 Eap failure frame */
	ret = cmdevt_send_data_by_cmd(vif->priv, vif, channel, 1, 0, NULL,
				      skb->data, skb->len);
	if (ret) {
		dev_kfree_skb(skb);
		netdev_err(ndev, "%s send failed\n", __func__);
		return NETDEV_TX_BUSY;
	}
	vif->ndev->stats.tx_bytes += skb->len;
	vif->ndev->stats.tx_packets++;
	dev_kfree_skb(skb);

	netdev_info(ndev, "%s send successfully\n", __func__);
	if (dump_data)
		print_hex_dump_debug("TX packet: ", DUMP_PREFIX_OFFSET,
				     16, 1, skb->data, skb->len, 0);

	return NETDEV_TX_OK;
}

int sc2332_set_random_mac(struct sprd_priv *priv, struct sprd_vif *vif,
			  u8 random_mac_flag, u8 *addr)
{
	struct sprd_msg *msg;
	u8 *mac;

	msg = get_cmdbuf(priv, vif, ETH_ALEN, CMD_RANDOM_MAC);
	if (!msg)
		return -ENOMEM;
	mac = (unsigned char *)msg->data;
	memcpy(mac, addr, ETH_ALEN);

	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

static int cmdevt_set_vowifi_state(struct sprd_priv *priv, struct sprd_vif *vif,
				   u8 status)
{
	struct sprd_msg *msg;
	struct cmd_vowifi *p;

	msg = get_cmdbuf(priv, vif, sizeof(*p), CMD_SET_VOWIFI);
	if (!msg)
		return -ENOMEM;

	p = (struct cmd_vowifi *)msg->data;
	p->value = status;
	return send_cmd_recv_rsp(priv, msg, NULL, NULL);
}

int sc2332_set_vowifi(struct net_device *ndev, struct ifreq *ifr)
{
	struct sprd_vif *vif = netdev_priv(ndev);
	struct sprd_priv *priv = vif->priv;
	struct android_wifi_priv_cmd priv_cmd;
	struct vowifi_data *vowifi;
	char *command = NULL;
	int ret, value;

	if (!ifr->ifr_data)
		return -EINVAL;
	if (copy_from_user(&priv_cmd, ifr->ifr_data, sizeof(priv_cmd)))
		return -EFAULT;

	/* bug1745380, add length check to avoid invalid NULL ptr */
	if ((priv_cmd.total_len < sizeof(*vowifi)) ||
	    (priv_cmd.total_len > SPRD_MAX_CMD_TXLEN)) {
		netdev_info(ndev, "%s: priv cmd total len is invalid: %d\n",
			    __func__, priv_cmd.total_len);
		return -EINVAL;
	}

	command = kmalloc(priv_cmd.total_len, GFP_KERNEL);
	if (!command)
		return -ENOMEM;
	if (copy_from_user(command, priv_cmd.buf, priv_cmd.total_len)) {
		ret = -EFAULT;
		goto out;
	}

	vowifi = (struct vowifi_data *)command;
	value = *vowifi->data;
	netdev_info(ndev, "%s value:%d\n", __func__, value);
	if (value != 0 && value != 1) {
		ret = -EINVAL;
		goto out;
	}
	ret = cmdevt_set_vowifi_state(priv, vif, value);

	if (ret)
		netdev_err(ndev, "%s set vowifi cmd error\n", __func__);
out:
	kfree(command);
	return ret;
}

bool sc2332_do_delay_work(struct sprd_work *work)
{
	u8 mac_addr[ETH_ALEN];
	u16 reason_code;
	struct sprd_vif *vif;

	if (!work)
		return false;
	vif = work->vif;
	netdev_dbg(vif->ndev, "process delayed work: %d\n",
				work->id);
	switch (work->id) {
	case SPRD_P2P_GO_DEL_STATION:
		memcpy(mac_addr, (u8 *)work->data, ETH_ALEN);
		memcpy(&reason_code, (u16 *)(work->data + ETH_ALEN), sizeof(u16));
		sc2332_del_station(vif->priv, vif, mac_addr, reason_code);
		break;
	default:
		break;
	}
	return true;
}

/* Events */
static void cmdevt_report_connect_evt(struct sprd_vif *vif, u8 *data, u16 len)
{
	struct sprd_connect_info conn_info = { 0 };
	u8 status_code;
	u8 *pos = data;
	unsigned int left = len;

	/* the first byte is status code */
	memcpy(&status_code, pos, sizeof(status_code));
	if (status_code != SPRD_CONNECT_SUCCESS &&
	    status_code != SPRD_ROAM_SUCCESS)
		goto out;
	pos += sizeof(status_code);
	left -= sizeof(status_code);

	/* parse BSSID */
	if (left < ETH_ALEN)
		goto out;
	conn_info.bssid = pos;
	pos += ETH_ALEN;
	left -= ETH_ALEN;

	/* get channel */
	if (left < sizeof(conn_info.chan))
		goto out;
	memcpy(&conn_info.chan, pos, sizeof(conn_info.chan));
	pos += sizeof(conn_info.chan);
	left -= sizeof(conn_info.chan);

	/* get signal */
	if (left < sizeof(conn_info.signal))
		goto out;
	memcpy(&conn_info.signal, pos, sizeof(conn_info.signal));
	pos += sizeof(conn_info.signal);
	left -= sizeof(conn_info.signal);

	/* parse REQ IE */
	if (!left)
		goto out;
	memcpy(&conn_info.req_ie_len, pos, sizeof(conn_info.req_ie_len));
	pos += sizeof(conn_info.req_ie_len);
	left -= sizeof(conn_info.req_ie_len);
	conn_info.req_ie = pos;
	pos += conn_info.req_ie_len;
	left -= conn_info.req_ie_len;

	/* parse RESP IE */
	if (!left)
		goto out;
	memcpy(&conn_info.resp_ie_len, pos, sizeof(conn_info.resp_ie_len));
	pos += sizeof(conn_info.resp_ie_len);
	left -= sizeof(conn_info.resp_ie_len);
	conn_info.resp_ie = pos;
	pos += conn_info.resp_ie_len;
	left -= conn_info.resp_ie_len;

	/* parse BEA IE */
	if (!left)
		goto out;
	memcpy(&conn_info.bea_ie_len, pos, sizeof(conn_info.bea_ie_len));
	pos += sizeof(conn_info.bea_ie_len);
	left -= sizeof(conn_info.bea_ie_len);
	conn_info.bea_ie = pos;
out:
	sprd_report_connection(vif, &conn_info, status_code);
}

static void cmdevt_report_disconnect_evt(struct sprd_vif *vif, u8 *data,
					 u16 len)
{
	u16 reason_code;

	memcpy(&reason_code, data, sizeof(reason_code));
	sprd_report_disconnection(vif, reason_code);
}

static void cmdevt_report_remain_on_channel_evt(struct sprd_vif *vif, u8 *data,
						u16 len)
{
	sprd_report_remain_on_channel_expired(vif);
}

static void cmdevt_report_new_station_evt(struct sprd_vif *vif, u8 *data,
					  u16 len)
{
	struct evt_new_station *sta = (struct evt_new_station *)data;

	sprd_report_softap(vif, sta->is_connect, sta->mac, sta->ie,
			   sta->ie_len);
}

static void cmdevt_report_scan_done_evt(struct sprd_vif *vif, u8 *data, u16 len)
{
	struct evt_scan_done *p = (struct evt_scan_done *)data;
#ifdef CONFIG_SPRD_WLAN_VENDOR_SPECIFIC
	u8 bucket_id = 0;
#endif /* CONFIG_SPRD_WLAN_VENDOR_SPECIFIC */

	switch (p->type) {
	case SPRD_SCAN_DONE:
		sprd_report_scan_done(vif, false);
		netdev_info(vif->ndev, "%s got %d BSSes\n", __func__,
			    bss_count);
		break;
	case SPRD_SCHED_SCAN_DONE:
		sprd_report_sched_scan_done(vif, false);
		netdev_info(vif->ndev, "%s schedule scan got %d BSSes\n",
			    __func__, bss_count);
		break;
#ifdef CONFIG_SPRD_WLAN_VENDOR_SPECIFIC
	case SPRD_GSCAN_DONE:
		bucket_id = ((struct evt_gscan_done *)data)->bucket_id;
		sc2332_gscan_done(vif, bucket_id);
		netdev_info(vif->ndev, "%s gscan got %d bucketid done\n",
			    __func__, bucket_id);
		break;
#endif /* CONFIG_SPRD_WLAN_VENDOR_SPECIFIC */
	case SPRD_SCAN_ERROR:
	default:
		sprd_report_scan_done(vif, true);
		sprd_report_sched_scan_done(vif, false);
		if (p->type == SPRD_SCAN_ERROR)
			netdev_err(vif->ndev, "%s error!\n", __func__);
		else
			netdev_err(vif->ndev, "%s invalid scan done type: %d\n",
				   __func__, p->type);
		break;
	}
	bss_count = 0;
}

static void cmdevt_report_mic_failure_evt(struct sprd_vif *vif, u8 *data, u16 len)
{
	struct evt_mic_failure *mic_failure = (struct evt_mic_failure *)data;

	sprd_report_mic_failure(vif, mic_failure->is_mcast,
				mic_failure->key_id);
}

static void cmdevt_report_cqm_evt(struct sprd_vif *vif, u8 *data, u16 len)
{
	struct evt_cqm *p;
	u8 rssi_event;

	p = (struct evt_cqm *)data;
	switch (p->status) {
	case SPRD_CQM_RSSI_LOW:
		rssi_event = NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW;
		break;
	case SPRD_CQM_RSSI_HIGH:
		rssi_event = NL80211_CQM_RSSI_THRESHOLD_EVENT_HIGH;
		break;
	case SPRD_CQM_BEACON_LOSS:
		/* TODO wpa_supplicant not support the event ,
		 * so we workaround this issue
		 */
		rssi_event = NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW;
		vif->beacon_loss = 1;
		break;
	default:
		netdev_err(vif->ndev, "%s invalid event!\n", __func__);
		return;
	}

	sprd_report_cqm(vif, rssi_event);
}

static void cmdevt_report_mlme_tx_status_evt(struct sprd_vif *vif, u8 *data, u16 len)
{
	struct evt_mgmt_tx_status *tx_status =
	    (struct evt_mgmt_tx_status *)data;

	sprd_report_mgmt_tx_status(vif, SPRD_GET_LE64(tx_status->cookie),
				   tx_status->buf,
				   SPRD_GET_LE16(tx_status->len),
				   tx_status->ack);
}

static void cmdevt_report_tdls_evt(struct sprd_vif *vif, u8 *data, u16 len)
{
	unsigned char peer[ETH_ALEN];
	u8 oper;
	u16 reason_code;
	struct evt_tdls *report_tdls = NULL;

	report_tdls = (struct evt_tdls *)data;
	memcpy(&peer[0], &report_tdls->mac[0], ETH_ALEN);
	oper = report_tdls->tdls_sub_cmd_mgmt;
	if (oper == SPRD_TDLS_TEARDOWN)
		oper = NL80211_TDLS_TEARDOWN;
	else
		oper = NL80211_TDLS_SETUP;
	reason_code = 0;
	sprd_report_tdls(vif, peer, oper, reason_code);
}

static void cmdevt_report_wmm_evt(struct sprd_vif *vif, u8 *data, u16 len)
{
	sprd_chip_set_qos(&vif->priv->chip, vif->mode, data[0]);
}

static int cmdevt_report_acs_evt(struct sprd_vif *vif, u8 *data, u16 len)
{
	int index;
	u8 chan_num;
	u8 *pos = data;
	struct survey_info_node *acs;
	struct acs_channel *acs_channel;

	/* the first element is num of channel */
	chan_num = *pos;
	pos += 1;
	len -= 1;
	netdev_info(vif->ndev, "%s chan_num:%d\n", __func__, chan_num);

	/* acs result */
	if (len < chan_num * SPRD_ACS_CHAN_NUM_MIN)
		netdev_info(vif->ndev, "%s invalid data report\n", __func__);

	acs_channel = (struct acs_channel *)pos;
	for (index = 0; index < chan_num; index++) {
		acs = kzalloc(sizeof(*acs), GFP_KERNEL);
		if (!acs)
			return -ENOMEM;
		netdev_info(vif->ndev,
			    "channel : %d, duration : %d, busy : %d\n",
			    acs_channel->channel, acs_channel->duration,
			    acs_channel->busy);
		acs->channel_num = acs_channel->channel;
		acs->duration = acs_channel->duration;
		acs->busy = acs_channel->busy;
		mutex_lock(&vif->survey_lock);
		list_add_tail(&acs->survey_list, &vif->survey_info_list);
		mutex_unlock(&vif->survey_lock);
		acs_channel += 1;
	}
	return 0;
}

void sc2332_report_frame_evt(struct sprd_vif *vif, u8 *data, u16 len, bool flag)
{
	struct evt_mgmt_frame *frame;
	u16 buf_len;
	u8 *buf = NULL;
	u8 channel, type;

	if (flag) {
		/* here frame maybe not 4 bytes align */
		frame = (struct evt_mgmt_frame *)
		    (data - sizeof(*frame) + len);
		buf = data - sizeof(*frame);
	} else {
		frame = (struct evt_mgmt_frame *)data;
		buf = frame->data;
	}
	channel = frame->channel;
	type = frame->type;
	buf_len = SPRD_GET_LE16(frame->len);

	sprd_dump_frame_prot_info(0, 0, buf, buf_len);

	switch (type) {
	case SPRD_FRAME_NORMAL:
		sprd_report_mgmt(vif, channel, buf, buf_len);
		break;
	case SPRD_FRAME_DEAUTH:
		sprd_report_mgmt_deauth(vif, buf, buf_len);
		break;
	case SPRD_FRAME_DISASSOC:
		sprd_report_mgmt_disassoc(vif, buf, buf_len);
		break;
	case SPRD_FRAME_SCAN:
		sc2332_report_scan_result(vif, channel, frame->signal,
					  buf, buf_len);
		++bss_count;
		break;
	default:
		netdev_err(vif->ndev, "%s invalid frame type: %d!\n",
			   __func__, type);
		break;
	}
}

/* return the msg length or 0 */
unsigned short sc2332_rx_evt_process(struct sprd_priv *priv, u8 *msg)
{
	struct sprd_cmd_hdr *hdr = (struct sprd_cmd_hdr *)msg;
	struct sprd_vif *vif;
	u8 mode;
	u16 len, plen;
	u8 *data;

	mode = hdr->common.mode;
	if (mode > SPRD_MODE_MAX) {
		wiphy_info(priv->wiphy, "%s invalid mode: %d\n", __func__,
			   mode);
		return 0;
	}

	plen = SPRD_GET_LE16(hdr->plen);
	if (!priv) {
		pr_err("%s priv is NULL [%u]mode %d recv[%s]len: %d\n",
		       __func__, le32_to_cpu(hdr->mstime), mode,
		       cmdevt_evt2str(hdr->cmd_id), hdr->plen);
		return plen;
	}

	wiphy_info(priv->wiphy, "[%u]mode %d recv[%s]len: %d\n",
		   le32_to_cpu(hdr->mstime), mode, cmdevt_evt2str(hdr->cmd_id), plen);

	if (dump_data)
		print_hex_dump_debug("EVENT: ", DUMP_PREFIX_OFFSET, 16, 1,
				     ((u8 *)hdr + sizeof(*hdr)),
				     hdr->plen - sizeof(*hdr), 0);

	len = plen - sizeof(*hdr);
	vif = sprd_mode_to_vif(priv, mode);
	if (!vif) {
		wiphy_info(priv->wiphy, "%s NULL vif for mode: %d, len:%d\n",
			   __func__, mode, plen);
		return plen;
	}

	if (!((long)msg & 0x3)) {
		data = (u8 *)msg;
		data += sizeof(*hdr);
	} else {
		/* never into here when the dev is BA or MARLIN2,
		 * temply used as debug and safe
		 */
		WARN_ON(1);
		data = kmalloc(len, GFP_KERNEL);
		if (!data) {
			sprd_put_vif(vif);
			return plen;
		}
		memcpy(data, msg + sizeof(*hdr), len);
	}

	switch (hdr->cmd_id) {
	case EVT_CONNECT:
		cmdevt_report_connect_evt(vif, data, len);
		break;
	case EVT_DISCONNECT:
		cmdevt_report_disconnect_evt(vif, data, len);
		break;
	case EVT_REMAIN_CHAN_EXPIRED:
		cmdevt_report_remain_on_channel_evt(vif, data, len);
		break;
	case EVT_NEW_STATION:
		cmdevt_report_new_station_evt(vif, data, len);
		break;
	case EVT_MGMT_FRAME:
		sc2332_report_frame_evt(vif, data, len, false);
		break;
#ifdef CONFIG_SPRD_WLAN_VENDOR_SPECIFIC
	case EVT_GSCAN_FRAME:
		sc2332_report_gscan_frame_evt(vif, data, len);
		break;
#endif /* CONFIG_SPRD_WLAN_VENDOR_SPECIFIC */
	case EVT_SCAN_DONE:
		cmdevt_report_scan_done_evt(vif, data, len);
		break;
	case EVT_SDIO_SEQ_NUM:
		break;
	case EVT_MIC_FAIL:
		cmdevt_report_mic_failure_evt(vif, data, len);
		break;
	case EVT_CQM:
		cmdevt_report_cqm_evt(vif, data, len);
		break;
	case EVT_MGMT_TX_STATUS:
		cmdevt_report_mlme_tx_status_evt(vif, data, len);
		break;
	case EVT_TDLS:
		cmdevt_report_tdls_evt(vif, data, len);
		break;
	case EVT_WMM_REPORT:
		cmdevt_report_wmm_evt(vif, data, len);
		break;
	case EVT_ACS_REPORT:
		cmdevt_report_acs_evt(vif, data, len);
		break;
#ifdef CONFIG_SPRD_WLAN_VENDOR_SPECIFIC
	case EVT_ACS_LTE_CONFLICT_EVENT:
		sc2332_report_acs_lte_event(vif);
		break;
#endif /* CONFIG_SPRD_WLAN_VENDOR_SPECIFIC */
	default:
		wiphy_info(priv->wiphy, "unsupported event: %d\n", hdr->cmd_id);
		break;
	}

	sprd_put_vif(vif);

	if ((long)msg & 0x3)
		kfree(data);

	return plen;
}

unsigned short sc2332_rx_rsp_process(struct sprd_priv *priv, u8 *msg)
{
	u8 mode;
	u16 plen;
	void *data;
	struct sprd_cmd *cmd = &priv->cmd;
	struct sprd_cmd_hdr *hdr;

	if (unlikely(!cmd->init_ok)) {
		pr_info("%s cmd coming too early, drop it\n", __func__);
		return 0;
	}

	hdr = (struct sprd_cmd_hdr *)msg;
	mode = hdr->common.mode;
	plen = SPRD_GET_LE16(hdr->plen);

#ifdef DUMP_COMMAND_RESPONSE
	print_hex_dump(KERN_DEBUG, "CMD RSP: ", DUMP_PREFIX_OFFSET, 16, 1,
		       ((u8 *)hdr + sizeof(*hdr)), hdr->plen - sizeof(*hdr), 0);
#endif
	/* 2048 use mac */
	if (mode > SPRD_MODE_MAX || hdr->cmd_id > CMD_MAX || plen > 2048) {
		pr_err("%s wrong CMD_RSP: %d\n", __func__, (int)hdr->cmd_id);
		return 0;
	}
	if (atomic_inc_return(&cmd->refcnt) >= SPRD_CMD_EXIT_VAL) {
		atomic_dec(&cmd->refcnt);
		return 0;
	}
	data = kmalloc(plen, GFP_KERNEL);
	if (!data) {
		atomic_dec(&cmd->refcnt);
		return plen;
	}
	memcpy(data, (void *)hdr, plen);

	spin_lock_bh(&cmd->lock);
	if (!cmd->data && SPRD_GET_LE32(hdr->mstime) == cmd->mstime &&
	    hdr->cmd_id == cmd->cmd_id) {
		wiphy_info(priv->wiphy, "mode %d recv rsp[%s]\n",
			   (int)mode, cmdevt_cmd2str(hdr->cmd_id));
		if (unlikely(hdr->status != 0)) {
			pr_err("%s mode %d recv rsp[%s] status[%s]\n",
			       __func__, (int)mode, cmdevt_cmd2str(hdr->cmd_id),
			       cmdevt_err2str(hdr->status));
			if (cmd->cmd_id == CMD_TX_MGMT) {
				pr_err("tx mgmt status : %d\n", hdr->status);
				priv->tx_mgmt_status = hdr->status;
			}
		}
		cmd->data = data;
		complete(&cmd->completed);
	} else {
		kfree(data);
		pr_err
		    ("%s mode %d recv mismatched rsp[%s] status[%s] mstime:[%u %u]\n",
		     __func__, (int)mode, cmdevt_cmd2str(hdr->cmd_id),
		     cmdevt_err2str(hdr->status), SPRD_GET_LE32(hdr->mstime),
		     cmd->mstime);
	}
	spin_unlock_bh(&cmd->lock);
	atomic_dec(&cmd->refcnt);

	return plen;
}
