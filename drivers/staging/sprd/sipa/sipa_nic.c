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

struct sipa_nic_statics_info {
	enum sipa_ep_id send_ep;
	enum sipa_xfer_pkt_type pkt_type;
	u32 src_mask;
	int netid;
};

static struct sipa_nic_statics_info s_spia_nic_statics[SIPA_NIC_MAX] = {
	{
		.send_ep = SIPA_EP_AP_ETH,
		.pkt_type = SIPA_PKT_ETH,
		.src_mask = (1 << SIPA_TERM_USB),
		.netid = -1,
	},
	{
		.send_ep = SIPA_EP_AP_ETH,
		.pkt_type = SIPA_PKT_ETH,
		.src_mask = (1 << SIPA_TERM_WIFI),
		.netid = -1,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 0,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 1,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 2,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 3,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 4,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 5,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 6,
	},
	{
		.send_ep = SIPA_EP_AP_IP,
		.pkt_type = SIPA_PKT_IP,
		.src_mask = SIPA_CP_SRC,
		.netid = 7,
	},
};

int sipa_nic_open(enum sipa_term_type src, int netid,
		  sipa_notify_cb cb, void *priv)
{
	int i;
	struct sipa_nic *nic = NULL;
	enum sipa_nic_id nic_id = SIPA_NIC_MAX;
	struct sipa_skb_receiver *receiver;
	struct sipa_skb_sender *sender;

	for (i = 0; i < SIPA_NIC_MAX; i++) {
		if ((s_spia_nic_statics[i].src_mask & (1 << src)) &&
			netid == s_spia_nic_statics[i].netid) {
			nic_id = i;
			break;
		}
	}
	pr_info("%s nic_id = %d\n", __func__, nic_id);
	if (nic_id == SIPA_NIC_MAX)
		return -EINVAL;

	if (s_sipa_ctrl.nic[nic_id])
		return -EINVAL;

	nic = kzalloc(sizeof(*nic), GFP_KERNEL);
	if (!nic) {
		pr_err("sipa_nic: kzalloc err.\n");
		return -ENOMEM;
	}
	nic->send_ep = s_sipa_ctrl.eps[s_spia_nic_statics[nic_id].send_ep];
	skb_queue_head_init(&nic->rx_skb_q);
	nic->need_notify = 0;
	nic->src_mask = s_spia_nic_statics[i].src_mask;
	nic->netid = netid;
	spin_lock_init(&nic->lock);
	nic->cb = cb;
	nic->cb_priv = priv;
	s_sipa_ctrl.nic[nic_id] = nic;

	/* every receiver may receive cp packets */
	receiver = s_sipa_ctrl.receiver[s_spia_nic_statics[nic_id].pkt_type];
	sipa_receiver_add_nic(receiver, nic);

	if (SIPA_PKT_IP == s_spia_nic_statics[nic_id].pkt_type) {
		receiver = s_sipa_ctrl.receiver[SIPA_PKT_ETH];
		sipa_receiver_add_nic(receiver, nic);
	}

	sender = s_sipa_ctrl.sender[s_spia_nic_statics[nic_id].pkt_type];
	sipa_skb_sender_add_nic(sender, nic);

	return nic_id;
}

void sipa_nic_close(enum sipa_nic_id nic_id)
{
	struct sipa_nic *nic = NULL;
	struct sk_buff *skb;
	unsigned long flags;

	if (nic_id == SIPA_NIC_MAX)
		return;

	if (!s_sipa_ctrl.nic[nic_id])
		return;

	nic = s_sipa_ctrl.nic[nic_id];

	/* free all  pending skbs */
	spin_lock_irqsave(&nic->lock, flags);
	while ((skb = skb_dequeue(&nic->rx_skb_q)) != NULL)
		dev_kfree_skb_any(skb);

	spin_unlock_irqrestore(&nic->lock, flags);

	kfree(nic);
	s_sipa_ctrl.nic[nic_id] = NULL;
}

void sipa_nic_notify_evt(struct sipa_nic *nic, enum sipa_evt_type evt)
{
	if (nic->cb)
		nic->cb(nic->cb_priv, evt, 0);

}
EXPORT_SYMBOL(sipa_nic_notify_evt);

void sipa_nic_try_notify_recv(struct sipa_nic *nic)
{
	unsigned long flags;
	int need_notify = 0;

	spin_lock_irqsave(&nic->lock, flags);
	if (nic->need_notify) {
		nic->need_notify = 0;
		need_notify = 1;
	}
	spin_unlock_irqrestore(&nic->lock, flags);

	if (need_notify)
		nic->cb(nic->cb_priv, SIPA_RECEIVE, 0);
}
EXPORT_SYMBOL(sipa_nic_try_notify_recv);

void sipa_nic_push_skb(struct sipa_nic *nic, struct sk_buff *skb)
{
	unsigned long flags;

	spin_lock_irqsave(&nic->lock, flags);
	skb_queue_tail(&nic->rx_skb_q, skb);
	if (nic->rx_skb_q.qlen == 1)
		nic->need_notify = 1;
	spin_unlock_irqrestore(&nic->lock, flags);
}
EXPORT_SYMBOL(sipa_nic_push_skb);

int sipa_nic_tx(enum sipa_nic_id nic_id, enum sipa_term_type dst,
		int netid, struct sk_buff *skb)
{
	struct sipa_skb_sender *sender;

	sender = s_sipa_ctrl.sender[s_spia_nic_statics[nic_id].pkt_type];
	if (!sender)
		return -ENODEV;

	return sipa_skb_sender_send_data(sender, skb, dst, netid);
}
EXPORT_SYMBOL(sipa_nic_tx);

int sipa_nic_rx(enum sipa_nic_id nic_id, struct sk_buff **out_skb)
{
	unsigned long flags;
	struct sk_buff *skb;
	struct sipa_nic *nic;

	if (!s_sipa_ctrl.nic[nic_id])
		return -ENODEV;

	nic = s_sipa_ctrl.nic[nic_id];
	spin_lock_irqsave(&nic->lock, flags);
	skb = skb_dequeue(&nic->rx_skb_q);
	spin_unlock_irqrestore(&nic->lock, flags);

	*out_skb = skb;

	return (skb) ? 0 : -ENODATA;
}
EXPORT_SYMBOL(sipa_nic_rx);

int sipa_nic_rx_has_data(enum sipa_nic_id nic_id)
{
	struct sipa_nic *nic;

	if (!s_sipa_ctrl.nic[nic_id])
		return 0;

	nic = s_sipa_ctrl.nic[nic_id];

	return (!!nic->rx_skb_q.qlen);
}
EXPORT_SYMBOL(sipa_nic_rx_has_data);
