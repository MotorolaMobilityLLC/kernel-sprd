/*
 * Copyright (C) 2016 Spreadtrum Communications Inc.
 *
 * Authors	:
 * ben.xu <ben.xu@unisoc.com>
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

#include "debug.h"
#include "sprdwl.h"
#include "txrx_buf_mm.h"
#include "mm.h"
#include "if_sc2355.h"

static struct sprdwl_buf_mm *g_tx_buf;

static inline void sprdwl_buf_node_init(struct sprdwl_buf_node *node)
{
	node->flag = SPRDWL_MEMORY_FREE;
	node->location = LOC_BUFF_FREE;
	node->ctxt_id = 0;
	node->next = NULL;
}

void sprdwl_buf_mm_dump(void)
{
	int i;

	if (g_tx_buf) {
		for (i = 0; i < g_tx_buf->max_num; i++)
			if (g_tx_buf->type == TXRX_BUF_SINGLE_TYPE)
				wl_err("%s:addr [%d]%p\n",
					__func__, i, g_tx_buf->addr[i]);
	}
}

struct sprdwl_buf_node *sprdwl_buf_mm_alloc(struct sprdwl_buf_mm *mm)
{
	struct sprdwl_buf_node *node = NULL;

	spin_lock_bh(&mm->freelock);
	if (likely(mm->head)) {
		node = mm->head;
		if (unlikely(mm->free_num == 1)) {
			mm->head = NULL;
			mm->tail = NULL;
		} else {
			mm->head = node->next;
		}
		mm->free_num--;
		node->next = NULL;
		node->flag = SPRDWL_MEMORY_ALLOC;
	}
	spin_unlock_bh(&mm->freelock);
	return node;
}

static inline struct sprdwl_buf_node *
sprdwl_buf_mm_single_alloc(struct sprdwl_buf_mm *mm)
{
	struct sprdwl_buf_node *node = NULL;

	if (mm->max_num >= mm->addr_count)
		return NULL;

	node = kmalloc(mm->len + sizeof(struct sprdwl_buf_node),
					GFP_DMA | GFP_KERNEL);
	if (node == NULL) {
		wl_warn("%s: alloc buf_node failed.\n", __func__);
		return NULL;
	}

	sprdwl_buf_node_init(node);
	spin_lock_bh(&mm->freelock);
	mm->addr[mm->max_num] = (void *)node;
	mm->max_num += 1;
	spin_unlock_bh(&mm->freelock);
	node->flag = SPRDWL_MEMORY_ALLOC;

	return node;
}

int sprdwl_buf_mm_free(struct sprdwl_buf_mm *mm,
							struct sprdwl_buf_node *node)
{
	if (node->flag != SPRDWL_MEMORY_ALLOC) {
		wl_err("%s:The buf[%p] is refree or bad.\n",
			__func__, node);
		sprdwl_buf_mm_dump();
		return -1;
	}

	spin_lock_bh(&mm->freelock);
	sprdwl_buf_node_init(node);
	if (mm->free_num)
		mm->tail->next = node;
	else
		mm->head = node;
	mm->tail = node;
	mm->free_num++;
	spin_unlock_bh(&mm->freelock);
	return 0;
}

struct sprdwl_buf_node *sprdwl_alloc_tx_buf(void)
{
	struct sprdwl_buf_node *node;
	if (g_tx_buf == NULL) {
		wl_err("%s:Tx_buf is not init.\n", __func__);
		return NULL;
	}
	node = sprdwl_buf_mm_alloc(g_tx_buf);
	if (node)
		return node;

	if (g_tx_buf->type == TXRX_BUF_SINGLE_TYPE)
		return sprdwl_buf_mm_single_alloc(g_tx_buf);
	return NULL;
}

void sprdwl_free_tx_buf(struct sprdwl_buf_node *node)
{
	if (g_tx_buf == NULL) {
		wl_err("%s:Tx_buf is not init.\n", __func__);
		return;
	}
	sprdwl_buf_mm_free(g_tx_buf, node);
}

int sprdwl_get_tx_buf_len(void)
{
	if (g_tx_buf == NULL) {
		return 0;
	}
	return g_tx_buf->len;
}

int sprdwl_get_tx_buf_num(void)
{
	if (g_tx_buf == NULL)
		return 0;
	if (g_tx_buf->type == TXRX_BUF_SINGLE_TYPE)
		return g_tx_buf->addr_count;
	return g_tx_buf->max_num;
}

int sprdwl_get_tx_buf_free_num(void)
{
	if (g_tx_buf == NULL)
		return 0;
	return g_tx_buf->free_num;
}

static inline int sprdwl_txrx_buf_single_init(void)
{
	int i;

	if (g_tx_buf) {
		wl_err("%s: txrx buf had been inited.\n", __func__);
		return -1;
	}

	g_tx_buf = kzalloc((sizeof(struct sprdwl_buf_mm)+
			    sizeof(unsigned long)*TXRX_TX_BUF_MAX_NUM),
			   GFP_KERNEL);
	if (!g_tx_buf) {
		wl_err("%s: alloc sprdwl_buf_mm fail", __func__);
		return -1;
	}
	spin_lock_init(&g_tx_buf->freelock);
	g_tx_buf->type = TXRX_BUF_SINGLE_TYPE;
	g_tx_buf->len = SPRDWL_MAX_DATA_TXLEN;
	g_tx_buf->addr_count = TXRX_TX_BUF_MAX_NUM;

	for (i = 0; i < g_tx_buf->addr_count; i++) {
		struct sprdwl_buf_node *node;
		node = sprdwl_buf_mm_single_alloc(g_tx_buf);
		if (node)
			sprdwl_free_tx_buf(node);
	}
	return 0;
}

static inline void
sprdwl_txrx_buf_single_deinit(struct sprdwl_buf_mm *mm)
{
	int i;

	for (i = 0; i < mm->max_num; i++) {
		kfree(mm->addr[i]);
		mm->addr[i] = NULL;
	}
}

int sprdwl_txrx_buf_init(void)
{
	return sprdwl_txrx_buf_single_init();
}

void sprdwl_txrx_buf_deinit(void)
{
	if (g_tx_buf) {
		if (g_tx_buf->type == TXRX_BUF_SINGLE_TYPE)
			sprdwl_txrx_buf_single_deinit(g_tx_buf);
		kfree(g_tx_buf);
		g_tx_buf = NULL;
	}

}

int sprdwl_skb_to_tx_buf(struct sprdwl_intf *dev,
							struct sprdwl_msg_buf *msg_pos)
{
	struct sk_buff *skb = NULL;
	unsigned long dma_addr = 0;
	struct sprdwl_buf_node *node = NULL;

	if (msg_pos == NULL) {
		wl_err("%s %d\n", __func__, __LINE__);
		return -1;
	}

	skb = msg_pos->skb;
	node = sprdwl_alloc_tx_buf();
	if (unlikely(node == NULL)) {
		wl_debug("%s: alloc tx buf fail.\n", __func__);
		return -1;
	}

	node->ctxt_id = msg_pos->ctxt_id;
	node->location = LOC_TX_INTF;
	if (node->addr == NULL) {
		wl_err("%s %d\n", __func__, __LINE__);
		return -1;
	}

	memcpy(node->addr, &msg_pos, sizeof(msg_pos));

	if (skb->len > sprdwl_get_tx_buf_len()) {
		wl_err("%s: skb->len(%d) > tx buf len(%d).\n",
			__func__, skb->len, sprdwl_get_tx_buf_len());
		sprdwl_free_tx_buf(node);
		return -1;
	}

	memcpy(node->buf, skb->data, skb->len);
	dma_addr = virt_to_phys(node->buf) | SPRDWL_MH_ADDRESS_BIT;
	memcpy(node->buf, &dma_addr, MSDU_DSCR_RSVD);
	dev_kfree_skb(msg_pos->skb);
	msg_pos->skb = NULL;
	msg_pos->node = node;
	msg_pos->tran_data = node->buf;
	msg_pos->pcie_addr = mm_virt_to_phys(&dev->pdev->dev,
					     msg_pos->tran_data,
					     msg_pos->len,
					     DMA_TO_DEVICE);

	return 0;
}

