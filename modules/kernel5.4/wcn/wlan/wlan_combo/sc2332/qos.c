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

#include <linux/string.h>
#include <uapi/linux/if_ether.h>
#include <uapi/linux/ip.h>
#include "common/msg.h"
#include "qos.h"

/* IPV6 field decodes */
#define IPV6_TRAFFIC_CLASS(ipv6_body) \
		(((((unsigned char *)ipv6_body)[0] & 0x0f) << 4) | \
		((((unsigned char *)ipv6_body)[1] & 0xf0) >> 4))

#define SPRD_QOS_WRAP_COUNT 200

void sc2332_qos_init(struct sprd_qos_t *qos, struct sprd_msg_list *list)
{
	memset(qos, 0, sizeof(struct sprd_qos_t));
	qos->head = &list->busylist;
	qos->last_node = &list->busylist;
	qos->txlist = list;
	qos->enable = 1;
}

int sc2332_qos_map(unsigned char *frame)
{
	struct ethhdr *eh;
	struct iphdr *iph;
	unsigned char tos_tc;
	int index;

	eh = (struct ethhdr *)frame;
	if (eh->h_proto == htons(ETH_P_IP)) {
		iph = (struct iphdr *)(eh + 1);
		tos_tc = iph->tos;
	} else if (eh->h_proto == htons(ETH_P_IPV6)) {
		tos_tc = IPV6_TRAFFIC_CLASS(eh + 1);
	} else {
		return QOS_OTHER;
	}

	switch (tos_tc & 0xE0) {
	case 0x0:
	case 0x60:
		index = QOS_AC_BE;
		break;
	case 0x20:
	case 0x40:
		index = QOS_AC_BK;
		break;
	case 0x80:
	case 0xA0:
		index = QOS_AC_VI;
		break;
	default:
		index = QOS_AC_VO;
		break;
	}

	return index;
}

void sc2332_qos_reorder(struct sprd_qos_t *qos)
{
	struct qos_list *list;
	struct sprd_msg *msg;
	struct list_head *end;
	struct list_head *pos;

	spin_lock_bh(&qos->txlist->busylock);
	end = qos->head->prev;
	for (pos = qos->last_node->next; pos != qos->head; pos = pos->next) {
		msg = list_entry(pos, struct sprd_msg, list);
		qos->num[msg->index]++;
		qos->txnum++;
		qos->last_node = &msg->list;
		msg->next = NULL;
		list = &qos->list[msg->index];
		if (!list->head) {
			list->head = msg;
			list->tail = msg;
		} else {
			list->tail->next = msg;
			list->tail = msg;
		}
		if (pos == end)
			break;
	}
	spin_unlock_bh(&qos->txlist->busylock);
}

/* the queue send weight */
int qos_weight[2][SPRD_QOS_NUM] = {
	{1, 1, 1, 1, 1},
	{1, 1, 3, 12, 48}
};

/* switch_buf: If switch send queue.
 *             We do our best to send the same qos msg in one sdio trans.
 *             if -1, a new sdio trans start.
 */
struct sprd_msg *sc2332_qos_peek_msg(struct sprd_qos_t *qos, int *switch_buf)
{
	int i, j, min = 0;
	int first;
	struct sprd_msg *msg = NULL;
	struct qos_list *list = NULL;

	if (*switch_buf != -1) {
		if (qos->num[*switch_buf]) {
			j = *switch_buf;
			goto same_index;
		}
	}
	for (i = 0, first = 1, j = -1; i < SPRD_QOS_NUM; i++) {
		if (qos->num[i]) {
			if (first) {
				first = 0;
				min = qos->cur_weight[i];
				j = i;
				*switch_buf = i;
			} else if (min > qos->cur_weight[i]) {
				min = qos->cur_weight[i];
				j = i;
				*switch_buf = i;
			}
		}
	}
same_index:
	if (j != -1) {
		qos->cur_weight[j] += qos_weight[qos->enable][j];
		if (qos->num_wrap++ >= SPRD_QOS_WRAP_COUNT || qos->change) {
			memset(qos->cur_weight, 0, sizeof(qos->cur_weight));
			qos->num_wrap = 0;
			qos->change = 0;
		}
		list = &qos->list[j];
		spin_lock_bh(&qos->txlist->busylock);
		msg = list->head;
		if (msg) {
			list->head = msg->next;
			msg->next = NULL;
		}
		spin_unlock_bh(&qos->txlist->busylock);
		return msg;
	}

	return NULL;
}

void sc2332_qos_update(struct sprd_qos_t *qos, struct sprd_msg *msg,
		       struct list_head *node)
{
	spin_lock_bh(&qos->txlist->busylock);
	qos->num[msg->index]--;
	qos->txnum--;
	if (node == qos->last_node)
		qos->last_node = node->prev;
	spin_unlock_bh(&qos->txlist->busylock);
}

/* call it after sc2332_qos_update */
void sc2332_qos_need_resch(struct sprd_qos_t *qos)
{
	if (!qos->txnum)
		sc2332_qos_reorder(qos);
}
