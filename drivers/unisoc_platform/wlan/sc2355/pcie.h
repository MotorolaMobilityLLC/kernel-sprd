/*
* SPDX-FileCopyrightText: 2020-2022 Unisoc (Shanghai) Technologies Co., Ltd
* SPDX-License-Identifier: GPL-2.0
*
* Copyright 2020-2022 Unisoc (Shanghai) Technologies Co., Ltd
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of version 2 of the GNU General Public License
* as published by the Free Software Foundation.
*/

#ifndef __PCIE_H__
#define __PCIE_H__

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <misc/wcn_bus.h>
#include "common/hif.h"
#include "pcie_buf.h"

#define PCIE_TX_NUM 96

#define PCIE_RX_CMD_PORT	7
#define PCIE_RX_DATA_PORT	9
#define PCIE_RX_ADDR_DATA_PORT	11
#define PCIE_TX_CMD_PORT	4
#define PCIE_TX_DATA_PORT	5
#define PCIE_TX_ADDR_DATA_PORT  10

#define USB_RX_CMD_PORT	20
#define USB_RX_PKT_LOG_PORT	21
#define USB_RX_DATA_PORT	22
#define USB_TX_CMD_PORT	4
#define USB_TX_DATA_PORT	6

#define MSDU_DSCR_RSVD	5

#define DEL_LUT_INDEX 0
#define ADD_LUT_INDEX 1
#define UPD_LUT_INDEX 2
#define SPRD_TX_MSG_CMD_NUM 128
#define SPRD_TX_QOS_POOL_SIZE 20000
#define SPRD_TX_DATA_START_NUM (SPRD_TX_QOS_POOL_SIZE - 3)
#define SPRD_RX_MSG_NUM 20000

#define MAX_FW_TX_DSCR (1024)

/* tx len less than cp len 4 byte as sdiom 4 bytes align */
/* set MAX CMD length to 1600 on firmware side*/
#define SPRD_MAX_CMD_TXLEN	1596
#define SPRD_MAX_CMD_RXLEN	1092
#define SPRD_MAX_DATA_TXLEN	1672
#define SPRD_MAX_DATA_RXLEN	1676

#define SAVE_ADDR(data, buf, len) memcpy((data - len), &buf, len)
#define RESTORE_ADDR(buf, data, len) memcpy(&buf, (data - len), len)
#define CLEAR_ADDR(data, len) memset((data - len), 0x0, len)
#define HIGHER_DDR_PRIORITY	0xAA

struct pcie_addr_buffer {
	struct {
		unsigned char type:3;
		/*direction of address buffer of cmd/event,*/
		/*0:Tx, 1:Rx*/
		unsigned char direction_ind:1;
		unsigned char buffer_type:1;
		unsigned char interface:3;
	} common;
	unsigned char offset;
	struct {
		unsigned char rsvd:7;
		unsigned char buffer_inuse:1;
	} buffer_ctrl;
	unsigned short number;
	unsigned short rsvd;
	unsigned char pcie_addr[0][5];
} __packed;

static inline void pcie_free_msg_content(struct sprd_msg *msg)
{
	if (msg->skb)
		dev_kfree_skb(msg->skb);
	if (msg->node)
		pcie_free_tx_buf(msg->node);

}

unsigned short sc2355_get_data_csum(void *entry, void *data);
int sc2355_tx_cmd_pop_list(int channel, struct mbuf_t *head,
			   struct mbuf_t *tail, int num);
int sc2355_tx_data_pop_list(int channel, struct mbuf_t *head,
			    struct mbuf_t *tail, int num);
int sc2355_tx_cmd(struct sprd_hif *hif, unsigned char *data, int len);
int sc2355_tx_addr_trans(struct sprd_hif *hif, unsigned char *data, int len);
int sc2355_hif_tx_list(struct sprd_hif *hif,
		       struct list_head *tx_list,
		       struct list_head *tx_list_head,
		       int tx_count, int ac_index, u8 coex_bt_on);
int sc2355_hif_fill_msdu_dscr(struct sprd_vif *vif,
			      struct sk_buff *skb, u8 type, u8 offset);
void sc2355_tx_free_pcie_data_num(struct sprd_hif *hif, unsigned char *data);
int sc2355_tx_free_pcie_data(struct sprd_priv *priv, unsigned char *data);
void *sc2355_get_rx_data(struct sprd_hif *hif, void *pos, void **data,
			 void **tran_data, int *len, int offset);
void sc2355_free_rx_data(struct sprd_hif *hif,
			 int chn, void *head, void *tail, int num);

void sc2355_hex_dump(unsigned char *name,
		     unsigned char *data, unsigned short len);
struct sprd_peer_entry
*sc2355_find_peer_entry_using_lut_index(struct sprd_hif *hif,
					unsigned char sta_lut_index);
void sc2355_event_sta_lut(struct sprd_vif *vif, u8 *data, u16 len);
struct sprd_peer_entry
*sc2355_find_peer_entry_using_addr(struct sprd_vif *vif, u8 *addr);
void sc2355_tx_addba(struct sprd_hif *hif,
		     struct sprd_peer_entry *peer_entry, unsigned char tid);
void sc2355_tx_delba(struct sprd_hif *hif,
		     struct sprd_peer_entry *peer_entry, unsigned int ac_index);
void sc2355_tx_send_addba(struct sprd_vif *vif, void *data, int len);
void sc2355_tx_send_delba(struct sprd_vif *vif, void *data, int len);
unsigned char sc2355_find_lut_index(struct sprd_hif *hif, struct sprd_vif *vif);
int sc2355_dis_flush_txlist(struct sprd_hif *hif, u8 lut_index);
void sc2355_handle_pop_list(void *data);
int sc2355_add_topop_list(int chn, struct mbuf_t *head,
			  struct mbuf_t *tail, int num);
void sc2355_set_coex_bt_on_off(u8 action);
int sc2355_tx_data_pop_list(int channel, struct mbuf_t *head,
			    struct mbuf_t *tail, int num);
int sc2355_push_link(struct sprd_hif *hif, int chn,
		     struct mbuf_t *head, struct mbuf_t *tail, int num,
		     int (*pop)(int, struct mbuf_t *, struct mbuf_t *, int));
enum sprd_hif_type get_hwintf_type(void);
void sc2355_tx_addr_trans_free(struct sprd_hif *hif);
int sc2355_tx_addr_trans_pcie(struct sprd_hif *hif,
			      unsigned char *data, int len, bool send_now);
void sc2355_add_to_free_list(struct sprd_priv *priv,
			     struct list_head *tx_list_head, int tx_count);
struct sprd_hif *sc2355_get_hif(void);
void sc2355_rx_work_queue(struct work_struct *work);
void sc2355_handle_tx_return(struct sprd_hif *hif,
				  struct sprd_msg_list *list,
				  int send_num, int ret);
int sc2355_fc_get_send_num(struct sprd_hif *hif,
			   enum sprd_mode mode, int data_num);
int sc2355_fc_test_send_num(struct sprd_hif *hif,
			    enum sprd_mode mode, int data_num);

#endif /* __PCIE_H__ */
