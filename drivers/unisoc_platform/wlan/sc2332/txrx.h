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

#ifndef __TXRX_H__
#define __TXRX_H__

#include "common/hif.h"
#include "common/msg.h"

#define SPRD_SEND_DATA_OFFSET		(2)

struct sprd_vif;
struct sprd_priv;
struct sprd_msg;

void sc2332_tcp_ack_filter_rx(unsigned char *buf);
int sc2332_tcp_ack_filter_send(struct sprd_msg *msg, unsigned char *buf);

unsigned char sc2332_convert_msg_type(enum sprd_head_type type);
unsigned int sc2332_max_tx_len(struct sprd_hif *hif, struct sprd_msg *msg);

void sc2332_wake_queue(struct sprd_hif *hif, struct sprd_msg_list *list,
		       enum sprd_mode mode);

struct sprd_msg *sc2332_tx_get_msg(struct sprd_chip *chip,
				   enum sprd_head_type type,
				   enum sprd_mode mode);
void sc2332_tx_free_msg(struct sprd_chip *chip, struct sprd_msg *msg);
int sc2332_tx(struct sprd_chip *chip, struct sprd_msg *msg);
int sc2332_tx_force_exit(struct sprd_chip *chip);
int sc2332_tx_is_exit(struct sprd_chip *chip);
void sc2332_tx_drop_tcp_msg(struct sprd_chip *chip, struct sprd_msg *msg);
void sc2332_tx_set_qos(struct sprd_chip *chip, enum sprd_mode mode, int enable);

void sc2332_flush_all_txlist(struct sprd_hif *hif);
void sc2332_keep_wakeup(struct sprd_hif *hif);

unsigned short sc2332_rx_data_process(struct sprd_priv *priv,
				      unsigned char *msg);
unsigned short sc2332_rx_evt_process(struct sprd_priv *priv, u8 *msg);
unsigned short sc2332_rx_rsp_process(struct sprd_priv *priv, u8 *msg);
int sc2332_send_data_offset(void);
int sc2332_send_data(struct sprd_vif *vif, struct sprd_msg *msg,
		     struct sk_buff *skb, u8 type, u8 offset, bool flag);

static inline void sc2332_tx_wakeup(struct sprd_hif *hif)
{
	hif->do_tx = 1;
	wake_up(&hif->waitq);
}

#endif
