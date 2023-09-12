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

#ifndef __WAPI_H__
#define __WAPI_H__

#include "cmdevt.h"

#define ETH_PKT_TYPE_OFFSET       12
#define WAPI_TYPE                 0x88B4
#define IPV6_TYPE                 0x86DD
#define IP_TYPE                   0x0800
#define ARP_TYPE                  0x0806
#define ONE_X_TYPE                0x888E
#define VLAN_TYPE                 0x8100
#define LLTD_TYPE                 0x88D9
#define UDP_TYPE                  0x11
#define TCP_TYPE                  0x06
#define SNAP_HDR_LEN		  8
#define ETHERNET_HDR_LEN          14
#define IP_HDR_OFFSET             ETHERNET_HDR_LEN
#define IP_HDR_LEN                20
#define IP_PROT_OFFSET            23
#define UDP_HDR_OFFSET            (IP_HDR_LEN + IP_HDR_OFFSET)
#define UDP_HDR_LEN               8
#define UDP_DATA_OFFSET           (UDP_HDR_OFFSET + UDP_HDR_LEN)
#define UDP_SRC_PORT_OFFSET       UDP_HDR_OFFSET
#define UDP_DST_PORT_OFFSET       (UDP_HDR_OFFSET + 2)
#define VLAN_HDR_LEN              18
#define TOS_FIELD_OFFSET          15
#define VLAN_TID_FIELD_OFFSET     14
#define MAC_UDP_DATA_LEN          1472
#define MAX_UDP_IP_PKT_LEN        (MAC_UDP_DATA_LEN + UDP_DATA_OFFSET)
#define SPRD_WAPI_ATTACH_LEN	  18

static inline int is_wapi(struct sprd_vif *vif, unsigned char *data)
{
	return (vif->prwise_crypto == SPRD_CIPHER_WAPI &&
		vif->key_len[SPRD_PAIRWISE]
			    [vif->key_index[SPRD_PAIRWISE]] != 0 &&
		(*(u16 *)(data + ETH_PKT_TYPE_OFFSET) != 0xb488));
}

unsigned short sc2332_wapi_enc(struct sprd_vif *vif,
			       unsigned char *data,
			       unsigned short data_len,
			       unsigned char *output_buf);

unsigned short sc2332_wapi_dec(struct sprd_vif *vif,
			       unsigned char *input_ptk,
			       unsigned short header_len,
			       unsigned short data_len,
			       unsigned char *output_buf);

int sc2332_rebuild_wapi_skb(struct sprd_vif *vif, struct sk_buff **skb);

/* This function compares the address with the (last bit on air) BIT24 to    */
/* determine if the address is a group address.                              */
/* Returns true if the input address has the group bit set.                  */
static inline bool is_group(unsigned char *addr)
{
	if ((addr[0] & BIT(0)) != 0)
		return true;

	return false;
}

static inline unsigned char *inc_wapi_pairwise_key_txrsc(struct sprd_vif *vif)
{
	int i;

	vif->key_txrsc[1][15] += 2;

	if (vif->key_txrsc[1][15] == 0x00) {
		for (i = 14; i >= 0; i--) {
			vif->key_txrsc[1][i] += 1;
			if ((vif->key_txrsc[1][i]) != 0x00)
				break;
		}
	}

	return vif->key_txrsc[1];
}

static inline unsigned char *mget_wapi_group_pkt_key(struct sprd_vif *vif,
						     int index)
{
	return (index >= 3) ? NULL : vif->key[0][index];
}

static inline unsigned char *mget_wapi_pairwise_pkt_key(struct sprd_vif *vif,
							int index)
{
	return (index >= 3) ? NULL : vif->key[1][index];
}

static inline unsigned char *mget_wapi_group_mic_key(struct sprd_vif *vif,
						     int index)
{
	return (index >= 3) ? NULL : ((u8 *)vif->key[0][index] + 16);
}

static inline unsigned char *mget_wapi_pairwise_mic_key(struct sprd_vif *vif,
							int index)
{
	return (index >= 3) ? NULL : ((u8 *)vif->key[1][index] + 16);
}

#endif /* __WAPI_H__ */
