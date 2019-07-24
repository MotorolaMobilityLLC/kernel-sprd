/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/sipa.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include "sipa_priv.h"
#include "sipa_hal.h"

#define SIPA_CP_SRC ((1 << SIPA_TERM_VAP0) | (1 << SIPA_TERM_VAP1) |\
		(1 << SIPA_TERM_VAP2) | (1 << SIPA_TERM_CP0) | \
		(1 << SIPA_TERM_CP1) | (1 << SIPA_TERM_VCP))

#define SIPA_NIC_RM_INACTIVE_TIMER	1000

struct sipa_nic_statics_info {
	enum sipa_ep_id send_ep;
	enum sipa_xfer_pkt_type pkt_type;
	u32 src_mask;
	int netid;
	enum sipa_rm_res_id cons;
};

static struct sipa_nic_statics_info s_spia_nic_statics[SIPA_NIC_MAX] = {
	{
		.send_ep = SIPA_EP_AP_ETH,
		.pkt_type = SIPA_PKT_ETH,
		.src_mask = (1 << SIPA_TERM_USB),
		.netid = -1,
		.cons = SIPA_RM_RES_CONS_USB,
	},
	{
		.send_ep = SIPA_EP_AP_ETH,
		.pkt_type = SIPA_PKT_ETH,
		.src_mask = (1 << SIPA_TERM_WIFI),
		.netid = -1,
		.cons = SIPA_RM_RES_CONS_WLAN,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 0,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 1,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 2,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 3,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 4,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 5,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 6,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 7,
		.cons = SIPA_RM_RES_CONS_WWAN_UL,
	},
};

int sipa_nic_open(enum sipa_term_type src, int netid,
		  sipa_notify_cb cb, void *priv)
{
	int i;
	struct sipa_nic *nic = NULL;
	struct sk_buff *skb;
	enum sipa_nic_id nic_id = SIPA_NIC_MAX;
	struct sipa_skb_receiver *receiver;
	struct sipa_skb_sender *sender;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	if (!ctrl) {
		pr_err("sipa driver may not register\n");
		return -EINVAL;
	}
	for (i = 0; i < SIPA_NIC_MAX; i++) {
		if ((s_spia_nic_statics[i].src_mask & (1 << src)) &&
			netid == s_spia_nic_statics[i].netid) {
			nic_id = i;
			break;
		}
	}
	dev_info(ctrl->ctx->pdev, "%s nic_id = %d\n", __func__, nic_id);
	if (nic_id == SIPA_NIC_MAX)
		return -EINVAL;

	if (ctrl->nic[nic_id]) {
		nic = ctrl->nic[nic_id];
		if  (atomic_read(&nic->status) == NIC_OPEN)
			return -EBUSY;
		while ((skb = skb_dequeue(&nic->rx_skb_q)) != NULL)
			dev_kfree_skb_any(skb);
	} else {
		nic = kzalloc(sizeof(*nic), GFP_KERNEL);
		if (!nic)
			return -ENOMEM;
		ctrl->nic[nic_id] = nic;
		skb_queue_head_init(&nic->rx_skb_q);
	}

	sender = ctrl->sender[s_spia_nic_statics[nic_id].pkt_type];

	atomic_set(&nic->status, NIC_OPEN);
	nic->nic_id = nic_id;
	nic->send_ep = ctrl->eps[s_spia_nic_statics[nic_id].send_ep];
	nic->need_notify = 0;
	nic->src_mask = s_spia_nic_statics[i].src_mask;
	nic->netid = netid;
	nic->cb = cb;
	nic->cb_priv = priv;

	/* every receiver may receive cp packets */
	receiver = ctrl->receiver[s_spia_nic_statics[nic_id].pkt_type];
	sipa_receiver_add_nic(receiver, nic);

	if (SIPA_PKT_IP == s_spia_nic_statics[nic_id].pkt_type) {
		receiver = ctrl->receiver[SIPA_PKT_ETH];
		sipa_receiver_add_nic(receiver, nic);
	}

	sipa_skb_sender_add_nic(sender, nic);

	return nic_id;
}
EXPORT_SYMBOL(sipa_nic_open);

void sipa_nic_close(enum sipa_nic_id nic_id)
{
	struct sipa_nic *nic = NULL;
	struct sk_buff *skb;
	struct sipa_skb_sender *sender;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	if (!ctrl) {
		pr_err("sipa driver may not register\n");
		return;
	}
	if (nic_id == SIPA_NIC_MAX || !ctrl->nic[nic_id])
		return;

	nic = ctrl->nic[nic_id];

	atomic_set(&nic->status, NIC_CLOSE);
	/* free all  pending skbs */
	while ((skb = skb_dequeue(&nic->rx_skb_q)) != NULL)
		dev_kfree_skb_any(skb);

	sender = ctrl->sender[s_spia_nic_statics[nic_id].pkt_type];
	sipa_skb_sender_remove_nic(sender, nic);
}
EXPORT_SYMBOL(sipa_nic_close);

void sipa_nic_notify_evt(struct sipa_nic *nic, enum sipa_evt_type evt)
{
	if (nic->cb)
		nic->cb(nic->cb_priv, evt, 0);

}
EXPORT_SYMBOL(sipa_nic_notify_evt);

void sipa_nic_try_notify_recv(struct sipa_nic *nic)
{
	int need_notify = 0;

	if (atomic_read(&nic->status) == NIC_CLOSE)
		return;

	if (nic->need_notify) {
		nic->need_notify = 0;
		need_notify = 1;
	}

	if (need_notify && nic->cb)
		nic->cb(nic->cb_priv, SIPA_RECEIVE, 0);
}
EXPORT_SYMBOL(sipa_nic_try_notify_recv);

void sipa_nic_push_skb(struct sipa_nic *nic, struct sk_buff *skb)
{
	skb_queue_tail(&nic->rx_skb_q, skb);
	if (nic->rx_skb_q.qlen == 1)
		nic->need_notify = 1;
}
EXPORT_SYMBOL(sipa_nic_push_skb);

int sipa_nic_tx(enum sipa_nic_id nic_id, enum sipa_term_type dst,
		int netid, struct sk_buff *skb)
{
	int ret;
	struct sipa_skb_sender *sender;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	if (!ctrl) {
		pr_err("sipa driver may not register\n");
		return -EINVAL;
	}
	sender = ctrl->sender[s_spia_nic_statics[nic_id].pkt_type];
	if (!sender)
		return -ENODEV;

	ret = sipa_skb_sender_send_data(sender, skb, dst, netid);
	if (ret == -EAGAIN)
		ctrl->nic[nic_id]->flow_ctrl_status = true;

	return ret;
}
EXPORT_SYMBOL(sipa_nic_tx);

int sipa_nic_rx(enum sipa_nic_id nic_id, struct sk_buff **out_skb)
{
	struct sk_buff *skb;
	struct sipa_nic *nic;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	if (!ctrl) {
		pr_err("sipa driver may not register\n");
		return -EINVAL;
	}
	if (!ctrl->nic[nic_id] ||
	    atomic_read(&ctrl->nic[nic_id]->status) == NIC_CLOSE)
		return -ENODEV;

	nic = ctrl->nic[nic_id];
	skb = skb_dequeue(&nic->rx_skb_q);

	*out_skb = skb;

	return (skb) ? 0 : -ENODATA;
}
EXPORT_SYMBOL(sipa_nic_rx);

int sipa_nic_rx_has_data(enum sipa_nic_id nic_id)
{
	struct sipa_nic *nic;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	if (!ctrl) {
		pr_err("sipa driver may not register\n");
		return -EINVAL;
	}
	if (!ctrl->nic[nic_id] ||
	    atomic_read(&ctrl->nic[nic_id]->status) == NIC_CLOSE)
		return 0;

	nic = ctrl->nic[nic_id];

	return (!!nic->rx_skb_q.qlen);
}
EXPORT_SYMBOL(sipa_nic_rx_has_data);

int sipa_nic_trigger_flow_ctrl_work(enum sipa_nic_id nic_id, int err)
{
	struct sipa_skb_sender *sender;
	struct sipa_control *ctrl = sipa_get_ctrl_pointer();

	if (!ctrl) {
		pr_err("sipa driver may not register\n");
		return -EINVAL;
	}
	sender = ctrl->sender[s_spia_nic_statics[nic_id].pkt_type];
	if (!sender)
		return -ENODEV;

	switch (err) {
	case -EAGAIN:
		sender->free_notify_net = true;
		schedule_work(&ctrl->flow_ctrl_work);
		break;
	default:
		dev_warn(ctrl->ctx->pdev,
			 "don't have this flow ctrl err type\n");
		break;
	}

	return 0;
}
EXPORT_SYMBOL(sipa_nic_trigger_flow_ctrl_work);
