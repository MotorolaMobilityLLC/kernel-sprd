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

#include "common/chip_ops.h"
#include "common/common.h"
#include "common/msg.h"
#include "common/tcp_ack.h"

#define SPRD_TCP_ACK_DROP_TIME		10
#define SPRD_U32_BEFORE(a, b)		((__s32)((__u32)(a) - (__u32)(b)) < 0)

unsigned int tcp_ack_drop_cnt = SPRD_TCP_ACK_DROP_CNT;
module_param(tcp_ack_drop_cnt, uint, 0644);
MODULE_PARM_DESC(tcp_ack_drop_cnt, "valid values: [1, 13]");

static struct sprd_tcp_ack_manage ack_manage;

static void tcp_ack_timeout(struct timer_list *t_list)
{
	struct tcp_ack_info *ack_info = from_timer(ack_info, t_list, timer);
	struct sprd_msg *msg;

	spin_lock_bh(&ack_info->lock);
	msg = ack_info->msg;
	if (msg) {
		ack_info->msg = NULL;
		ack_info->drop_cnt = 0;
		spin_unlock_bh(&ack_info->lock);
		if (sprd_chip_tx(&ack_manage.priv->chip, msg))
			pr_err("%s TX data error\n", __func__);
		return;
	}
	spin_unlock_bh(&ack_info->lock);
}

static int tcp_ack_check_quick(unsigned char *buf, struct tcp_ack_msg *ack_msg)
{
	int ip_hdr_len;
	unsigned char *temp;
	struct ethhdr *ethhdr;
	struct iphdr *iphdr;
	struct tcphdr *tcphdr;

	ethhdr = (struct ethhdr *)buf;
	if (ethhdr->h_proto != htons(ETH_P_IP))
		return 0;
	iphdr = (struct iphdr *)(ethhdr + 1);
	if (iphdr->version != 4 || iphdr->protocol != IPPROTO_TCP)
		return 0;
	ip_hdr_len = iphdr->ihl * 4;
	temp = (unsigned char *)(iphdr) + ip_hdr_len;
	tcphdr = (struct tcphdr *)temp;
	/* TCP_FLAG_ACK */
	if (!(temp[13] & 0x10))
		return 0;
	/* TCP_FLAG_PUSH */
	if (temp[13] & 0x8) {
		ack_msg->saddr = iphdr->daddr;
		ack_msg->daddr = iphdr->saddr;
		ack_msg->source = tcphdr->dest;
		ack_msg->dest = tcphdr->source;
		ack_msg->seq = ntohl(tcphdr->seq);
		return 1;
	}

	return 0;
}

/* return val:
 * 0 for not tcp ack
 * 1 for ack which can be dropped
 * 2 for other ack whith more info
 */
static int tcp_ack_check(unsigned char *buf, struct tcp_ack_msg *ack_msg)
{
	int ret;
	int ip_hdr_len;
	int iptotal_len;
	unsigned char *temp;
	struct ethhdr *ethhdr;
	struct iphdr *iphdr;
	struct tcphdr *tcphdr;

	ethhdr = (struct ethhdr *)buf;
	if (ethhdr->h_proto != htons(ETH_P_IP))
		return 0;
	iphdr = (struct iphdr *)(ethhdr + 1);
	if (iphdr->version != 4 || iphdr->protocol != IPPROTO_TCP)
		return 0;
	ip_hdr_len = iphdr->ihl * 4;
	temp = (unsigned char *)(iphdr) + ip_hdr_len;
	tcphdr = (struct tcphdr *)temp;
	/* TCP_FLAG_ACK */
	if (!(temp[13] & 0x10))
		return 0;
	if (temp[13] != 0x10) {
		ret = 2;
		goto out;
	}
	iptotal_len = ntohs(iphdr->tot_len);
	ret = (iptotal_len == 40) ? 1 : 2;

out:
	ack_msg->saddr = iphdr->saddr;
	ack_msg->daddr = iphdr->daddr;
	ack_msg->source = tcphdr->source;
	ack_msg->dest = tcphdr->dest;
	ack_msg->seq = ntohl(tcphdr->ack_seq);

	return ret;
}

/* return val: -1 for not match, others for match */
static int tcp_ack_match(struct sprd_tcp_ack_manage *ack_m,
			 struct tcp_ack_msg *ack_msg)
{
	int i;
	struct tcp_ack_info *ack_info;
	struct tcp_ack_msg *ack = NULL;

	for (i = 0; i < SPRD_TCP_ACK_NUM; i++) {
		ack_info = &ack_m->ack_info[i];
		ack = &ack_info->ack_msg;
		if (ack_info->busy) {
			if (ack->dest == ack_msg->dest &&
			    ack->source == ack_msg->source &&
			    ack->saddr == ack_msg->saddr &&
			    ack->daddr == ack_msg->daddr)
				return i;
		}
	}

	return -1;
}

static void tcp_ack_updata(struct sprd_tcp_ack_manage *ack_m)
{
	int i;
	struct tcp_ack_info *ack_info;

	if (time_after(jiffies, ack_m->last_time + ack_m->timeout)) {
		spin_lock_bh(&ack_m->lock);
		ack_m->last_time = jiffies;
		for (i = SPRD_TCP_ACK_NUM - 1; i >= 0; i--) {
			ack_info = &ack_m->ack_info[i];
			if (ack_info->busy &&
			    time_after(jiffies, ack_info->last_time +
				       ack_info->timeout)) {
				ack_m->free_index = i;
				ack_m->max_num--;
				ack_info->busy = 0;
			}
		}
		spin_unlock_bh(&ack_m->lock);
	}
}

/* return val: -1 for no index, others for index */
static int tcp_ack_alloc_index(struct sprd_tcp_ack_manage *ack_m)
{
	int i;
	struct tcp_ack_info *ack_info;

	if (ack_m->max_num == SPRD_TCP_ACK_NUM)
		return -1;
	spin_lock_bh(&ack_m->lock);
	if (ack_m->free_index >= 0) {
		i = ack_m->free_index;
		ack_m->free_index = -1;
		ack_m->max_num++;
		ack_m->ack_info[i].busy = 1;
		ack_m->ack_info[i].psh_flag = 0;
		ack_m->ack_info[i].last_time = jiffies;
		spin_unlock_bh(&ack_m->lock);
		return i;
	}
	for (i = 0; i < SPRD_TCP_ACK_NUM; i++) {
		ack_info = &ack_m->ack_info[i];
		if (ack_info->busy) {
			continue;
		} else {
			ack_m->free_index = -1;
			ack_m->max_num++;
			ack_info->busy = 1;
			ack_info->psh_flag = 0;
			ack_info->last_time = jiffies;
			spin_unlock_bh(&ack_m->lock);
			return i;
		}
	}

	spin_unlock_bh(&ack_m->lock);
	return -1;
}

/* return val: 0 for handle tx, 1 for not handle tx
 * type: 2 new_msg must be send
 * 1 new_msg is ACK and can be dropped,
 * unless some conditions matched.
 */
static int tcp_ack_handle(struct sprd_msg *new_msg,
			  struct sprd_tcp_ack_manage *ack_m,
			  struct tcp_ack_info *ack_info,
			  struct tcp_ack_msg *ack_msg, int type)
{
	int quick_ack = 0;
	struct tcp_ack_msg *ack = NULL;

	ack = &ack_info->ack_msg;
	if (type == 2) {
		if (SPRD_U32_BEFORE(ack->seq, ack_msg->seq)) {
			spin_lock_bh(&ack_info->lock);
			ack->seq = ack_msg->seq;
			if (unlikely(!ack_info->msg)) {
				spin_unlock_bh(&ack_info->lock);
				return 0;
			}
			sprd_chip_drop_tcp_msg(&ack_m->priv->chip, ack_info->msg);
			ack_info->msg = NULL;
			ack_info->drop_cnt = 0;
			spin_unlock_bh(&ack_info->lock);
			if (timer_pending(&ack_info->timer))
				del_timer_sync(&ack_info->timer);
		}
		return 0;
	}
	if (SPRD_U32_BEFORE(ack->seq, ack_msg->seq)) {
		spin_lock_bh(&ack_info->lock);
		if (ack_info->msg) {
			sprd_chip_drop_tcp_msg(&ack_m->priv->chip, ack_info->msg);
			ack_info->msg = NULL;
		}
		if (ack_info->psh_flag &&
		    !SPRD_U32_BEFORE(ack_msg->seq, ack_info->psh_seq)) {
			ack_info->drop_cnt = 0;
			ack_info->psh_flag = 0;
			quick_ack = 1;
		} else {
			ack_info->drop_cnt++;
		}
		ack->seq = ack_msg->seq;
		spin_unlock_bh(&ack_info->lock);
		if (quick_ack || ack_info->drop_cnt > tcp_ack_drop_cnt) {
			if (timer_pending(&ack_info->timer))
				del_timer_sync(&ack_info->timer);
			ack_info->drop_cnt = 0;
			return 0;
		}
		spin_lock_bh(&ack_info->lock);
		ack_info->msg = new_msg;
		spin_unlock_bh(&ack_info->lock);
		if (!timer_pending(&ack_info->timer))
			mod_timer(&ack_info->timer, jiffies + ack_m->drop_time);
		return 1;
	}

	return 0;
}

void sc2332_tcp_ack_filter_rx(unsigned char *buf)
{
	int index;
	struct tcp_ack_msg ack_msg;
	struct sprd_tcp_ack_manage *ack_m;
	struct tcp_ack_info *ack_info;

	ack_m = &ack_manage;

	if (!tcp_ack_check_quick(buf, &ack_msg))
		return;

	index = tcp_ack_match(ack_m, &ack_msg);
	if (index >= 0) {
		ack_info = ack_m->ack_info + index;
		spin_lock_bh(&ack_info->lock);
		ack_info->psh_flag = 1;
		ack_info->psh_seq = ack_msg.seq;
		spin_unlock_bh(&ack_info->lock);
	}
}

/* return val: 0 for not fileter, 1 for fileter */
int sc2332_tcp_ack_filter_send(struct sprd_msg *msg, unsigned char *buf)
{
	int ret = 0;
	int index, drop;
	struct tcp_ack_msg ack_msg;
	struct tcp_ack_msg *ack = NULL;
	struct tcp_ack_info *ack_info;
	struct sprd_tcp_ack_manage *ack_m;

	ack_m = &ack_manage;
	tcp_ack_updata(ack_m);
	drop = tcp_ack_check(buf, &ack_msg);
	if (!drop)
		return 0;
	if (unlikely(atomic_inc_return(&ack_m->ref) >= SPRD_TCP_ACK_EXIT_VAL))
		goto out;

	index = tcp_ack_match(ack_m, &ack_msg);
	if (index >= 0) {
		ack_info = ack_m->ack_info + index;
		ack_info->last_time = jiffies;
		ret = tcp_ack_handle(msg, ack_m, ack_info, &ack_msg, drop);
		goto out;
	}

	index = tcp_ack_alloc_index(ack_m);
	if (index >= 0) {
		ack = &ack_m->ack_info[index].ack_msg;
		ack->dest = ack_msg.dest;
		ack->source = ack_msg.source;
		ack->saddr = ack_msg.saddr;
		ack->daddr = ack_msg.daddr;
		ack->seq = ack_msg.seq;
	}

out:
	atomic_dec(&ack_m->ref);

	return ret;
}

void sc2332_tcp_ack_init(struct sprd_priv *priv)
{
	int i;
	struct tcp_ack_info *ack_info;
	struct sprd_tcp_ack_manage *ack_m = &ack_manage;

	memset(ack_m, 0, sizeof(struct sprd_tcp_ack_manage));
	if (tcp_ack_drop_cnt == 0 || tcp_ack_drop_cnt >= 14)
		tcp_ack_drop_cnt = SPRD_TCP_ACK_DROP_CNT;
	ack_m->priv = priv;
	atomic_set(&ack_m->ref, 0);
	spin_lock_init(&ack_m->lock);
	ack_m->drop_time = SPRD_TCP_ACK_DROP_TIME * HZ / 1000;
	ack_m->last_time = jiffies;
	ack_m->timeout = msecs_to_jiffies(SPRD_ACK_OLD_TIME);
	for (i = 0; i < SPRD_TCP_ACK_NUM; i++) {
		ack_info = &ack_m->ack_info[i];
		spin_lock_init(&ack_info->lock);
		ack_info->last_time = jiffies;
		ack_info->timeout = msecs_to_jiffies(SPRD_ACK_OLD_TIME);
		timer_setup(&ack_info->timer, tcp_ack_timeout, 0);
	}
}

void sc2332_tcp_ack_deinit(struct sprd_priv *priv)
{
	int i;
	unsigned long timeout;
	struct sprd_tcp_ack_manage *ack_m = &ack_manage;

	atomic_add(SPRD_TCP_ACK_EXIT_VAL, &ack_m->ref);
	timeout = jiffies + msecs_to_jiffies(1000);
	while (atomic_read(&ack_m->ref) > SPRD_TCP_ACK_EXIT_VAL) {
		if (time_after(jiffies, timeout)) {
			pr_err("%s cmd lock timeout!\n", __func__);
			WARN_ON(1);
		}
		usleep_range(2000, 2500);
	}
	for (i = 0; i < SPRD_TCP_ACK_NUM; i++)
		del_timer_sync(&ack_m->ack_info[i].timer);
}
