/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 * Authors	:
 * baojie.cai <baojie.cai@unisoc.com>
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

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include "debug.h"
#include "sprdwl.h"
#include "sipc_txrx_mm.h"
#include "mm.h"
#include "if_sc2355.h"

#include <linux/debugfs.h>
#include <linux/genalloc.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

int sipc_buf_mm_init(int num, struct sipc_buf_mm *buf_mm)
{
	int i;
	struct sprdwl_msg_list *list = &buf_mm->nlist;
	struct sipc_buf_node *node = NULL;
	struct sipc_buf_node *pos = NULL;
	void *ptr = buf_mm->virt_start;

	if (!list)
		return -EPERM;
	INIT_LIST_HEAD(&list->freelist);
	INIT_LIST_HEAD(&list->busylist);
	INIT_LIST_HEAD(&list->cmd_to_free);
	list->maxnum = num;
	spin_lock_init(&list->freelock);
	spin_lock_init(&list->busylock);
	spin_lock_init(&list->complock);
	atomic_set(&list->ref, 0);
	atomic_set(&list->flow, 0);

	for (i = 0; i < num; i++) {
		node = kzalloc(sizeof(*node), GFP_KERNEL);
		if (!node) {
			wl_err("%s: alloc rx node failed.\n", __func__);
			goto err_alloc;
		}

		/*must not out of virt_end*/
		if (ptr > buf_mm->virt_end) {
			wl_err("%s out of memory, alloc buf node num %d!\n",
				__func__, num);
			list->maxnum = num;
			kfree(node);
			return 0;
		}

		INIT_LIST_HEAD(&node->list);
		node->buf = ptr;
		node->priv = NULL;
		node->flag = SIPC_MEMORY_FREE;
		node->location = SIPC_LOC_BUFF_FREE;
		node->ctxt_id = 0;
		memset_io(node->buf, 0x0, buf_mm->len);
		list_add_tail(&node->list, &list->freelist);
		atomic_inc(&list->ref);
		ptr += buf_mm->len;
	}

	wl_err("%s: init mem ptr %p, phy %llx.",
			__func__, ptr, virt_to_phys(ptr));
	return 0;

err_alloc:
		list_for_each_entry_safe(node, pos, &list->freelist, list) {
			list_del(&node->list);
			kfree(node);
		}
	return -1;

}

#define SPRDWL_NODE_EXIT_VAL 0x8000
void sipc_buf_mm_deinit(struct sprdwl_msg_list *list)
{
	struct sipc_buf_node *node;
	struct sipc_buf_node *pos;
	struct timespec txmsgftime1, txmsgftime2;
	memset(&txmsgftime1, 0, sizeof(struct timespec));
	memset(&txmsgftime2, 0, sizeof(struct timespec));
	atomic_add(SPRDWL_NODE_EXIT_VAL, &list->ref);
	if (atomic_read(&list->ref) > SPRDWL_NODE_EXIT_VAL)
		wl_err("%s ref not ok! wait for pop!\n", __func__);

	getnstimeofday(&txmsgftime1);
	while (atomic_read(&list->ref) > SPRDWL_NODE_EXIT_VAL) {
		getnstimeofday(&txmsgftime2);
		if (((unsigned long)(timespec_to_ns(&txmsgftime2) -
			timespec_to_ns(&txmsgftime1))/1000000) > 3000)
			break;
		usleep_range(2000, 2500);
	}

	wl_info("%s list->ref ok!\n", __func__);

	list_for_each_entry_safe(node, pos, &list->busylist, list) {
		list_del(&node->list);
		kfree(node);
		atomic_dec(&list->flow);
	}

	list_for_each_entry_safe(node, pos, &list->freelist, list) {
		list_del(&node->list);
		kfree(node);
		atomic_dec(&list->ref);
	}
}

struct sipc_buf_node *sipc_alloc_node_buf(struct sprdwl_msg_list *list)
{
	struct sipc_buf_node *node = NULL;
	unsigned long flags = 0;

	if (!list)
		return NULL;

	spin_lock_irqsave(&list->freelock, flags);
	if (!list_empty(&list->freelist)) {
		node = list_first_entry(&list->freelist,
					   struct sipc_buf_node, list);
		list_del(&node->list);
		atomic_dec(&list->ref);
	}
	spin_unlock_irqrestore(&list->freelock, flags);

	return node;
}

void sipc_free_node_buf(struct sipc_buf_node *node,
			 struct sprdwl_msg_list *list)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&list->freelock, flags);
	list_add_tail(&node->list, &list->freelist);
	atomic_inc(&list->ref);
	spin_unlock_irqrestore(&list->freelock, flags);
}

void sipc_dequeue_node_to_freelist(struct sipc_buf_node *node,
			 struct sprdwl_msg_list *list)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&list->busylock, flags);
	list_del(&node->list);
	atomic_dec(&list->flow);
	spin_unlock_irqrestore(&list->busylock, flags);

	spin_lock_irqsave(&list->freelock, flags);
	list_add_tail(&node->list, &list->freelist);
	atomic_inc(&list->ref);
	spin_unlock_irqrestore(&list->freelock, flags);

}

void sipc_queue_node_buf(struct sipc_buf_node *node,
			  struct sprdwl_msg_list *list)
{
	spin_lock_bh(&list->busylock);
	list_add_tail(&node->list, &list->busylist);
	atomic_inc(&list->flow);
	spin_unlock_bh(&list->busylock);
}

void sipc_free_tx_buf(struct sprdwl_intf *intf,
						struct sipc_buf_node *node)
{
	struct sipc_buf_mm *tx_buf = intf->sipc_mm->tx_buf;

	if (tx_buf == NULL) {
		wl_err("%s:Tx_buf is not init.\n", __func__);
		return;
	}

	memset_io(node->buf, 0, tx_buf->len);
	sipc_dequeue_node_to_freelist(node, &tx_buf->nlist);

}

int sipc_get_tx_buf_num(struct sprdwl_intf *intf)
{
	int num;
	struct sipc_buf_mm *tx_buf = intf->sipc_mm->tx_buf;

	num = atomic_read(&tx_buf->nlist.ref);

	return num;
}

static inline void
sipc_txrx_buf_single_deinit(struct sipc_buf_mm *mm)
{
	struct sprdwl_msg_list *list = &mm->nlist;
	struct sipc_buf_node *node = NULL, *pos_node = NULL;
	unsigned long flags = 0;

	spin_lock_irqsave(&list->busylock, flags);
	list_for_each_entry_safe(node, pos_node, &list->busylist, list) {
		node->priv = NULL;
		list_del(&node->list);
		memset_io(node->buf, 0, mm->len);
		kfree(node);
		atomic_dec(&list->flow);
	}
	spin_unlock_irqrestore(&list->busylock, flags);

	flags = 0;
	spin_lock_irqsave(&list->freelock, flags);
	list_for_each_entry_safe(node, pos_node, &list->freelist, list) {
		node->priv = NULL;
		list_del(&node->list);
		memset_io(node->buf, 0, mm->len);
		kfree(node);
		atomic_dec(&list->ref);
	}
	spin_unlock_irqrestore(&list->freelock, flags);
	atomic_set(&list->ref, 0);

	kfree(mm);

}

void *sipc_pkt_txrx_mm(phys_addr_t start, size_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;
	phys_addr_t addr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);
	prot = pgprot_noncached(PAGE_KERNEL);

	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		wl_err("%s kmalloc_array erroy\n", "sprd-wlan");
		return NULL;
	}

	for (i = 0; i < page_count; i++) {
		addr = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}

	vaddr = vm_map_ram(pages, page_count, -1, prot) + offset_in_page(start);
	kfree(pages);
	return vaddr;
}

static inline int
sipc_pkt_mem_map(struct platform_device *pdev,
				struct sipc_mem_region *smem)
{
	int ret = 0;
/*	struct device_node *np = pdev->dev.of_node;
	struct resource res;

	np = of_find_node_by_path("/sprd-wlan");
	if (!np) {
		wl_err("No %s specified\n", "sprd-wlan");
		return -1;
	}

	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		wl_err("No memory address assigned to the region\n");
		return ret;
	}

	smem->phy_base = res.start;
	smem->size = resource_size(&res);
*/
	smem->phy_base = 0x87380000;
	smem->size = 0x00280000;
	smem->page_count = smem->phy_base - offset_in_page(smem->phy_base);
	smem->virt_base = sipc_pkt_txrx_mm(smem->phy_base, smem->size);
	if (unlikely(smem->virt_base == NULL)) {
		wl_err("Failed mapping mem\n");
		return -ENOMEM;
	}

	wl_info("Allocated reserved memory, vaddr: 0x%llx, paddr: 0x%llx\n",
			(u64)smem->virt_base, smem->phy_base);

	return ret;
}

static int sipc_init_tx_mm(struct sipc_txrx_mm *txrx_buf)
{
	struct sipc_buf_mm *tx_buf = NULL;

	 txrx_buf->tx_buf =  kzalloc(sizeof(struct sipc_buf_mm), GFP_KERNEL);
	if (!txrx_buf->tx_buf) {
		wl_err("%s: alloc tx buf fail", __func__);
		return -1;
	}

	tx_buf = txrx_buf->tx_buf;
	tx_buf->type = SIPC_TXRX_BUF_SINGLE_TYPE;
	tx_buf->padding = sizeof(unsigned long);
	tx_buf->len =  ALIGN(SPRDWL_MAX_DATA_TXLEN + tx_buf->padding, ARCH_DMA_MINALIGN);
	tx_buf->buf_count = SIPC_TXRX_TX_BUF_MAX_NUM;
	tx_buf->virt_start = txrx_buf->smem.virt_base;
	tx_buf->offset = (unsigned long)txrx_buf->smem.virt_base - txrx_buf->smem.phy_base;
	tx_buf->virt_end = (char *)tx_buf->virt_start + SPRDWL_SIPC_MEM_RX_OFFSET;

	if (sipc_buf_mm_init(tx_buf->buf_count, tx_buf)) {
		wl_err("%s: sprdwl_buf_mm_init fail", __func__);
		goto err_mm_init;
	}

	return 0;

err_mm_init:
	kfree(tx_buf);
	tx_buf = NULL;
	return -1;
}

static int sipc_init_rx_mm(struct sipc_txrx_mm *txrx_buf)
{
	struct sipc_buf_mm *rx_buf = NULL;

	txrx_buf->rx_buf =  kzalloc(sizeof(struct sipc_buf_mm), GFP_KERNEL);
	if (!txrx_buf->rx_buf) {
		wl_err("%s: alloc rxbuf fail", __func__);
		return -1;
	}

	rx_buf = txrx_buf->rx_buf;
	rx_buf->type = SIPC_TXRX_BUF_SINGLE_TYPE;
	rx_buf->padding = sizeof(unsigned long);
	rx_buf->len = ALIGN(SPRDWL_MAX_DATA_RXLEN + rx_buf->padding, ARCH_DMA_MINALIGN);
	rx_buf->buf_count = SIPC_TXRX_RX_BUF_MAX_NUM;
	rx_buf->virt_start = (char *)txrx_buf->smem.virt_base
		+ SPRDWL_SIPC_MEM_RX_OFFSET;
	rx_buf->virt_end = (char *)txrx_buf->smem.virt_base
		+ SPRDWL_SIPC_MEM_TXRX_TOTAL;
	rx_buf->offset = (unsigned long)txrx_buf->smem.virt_base - txrx_buf->smem.phy_base;

	if (sipc_buf_mm_init(rx_buf->buf_count, rx_buf)) {
		wl_err("%s: sprdwl_buf_mm_init fail", __func__);
		goto err_mm_init;
	}

	wl_err("%s: alloc rxbuf len %d", __func__, rx_buf->len);
	return 0;

err_mm_init:
	kfree(rx_buf);
	rx_buf = NULL;
	return -1;
}

int sipc_txrx_buf_init(struct platform_device *pdev, struct sprdwl_intf *intf)
{
	int ret;
	struct sipc_txrx_mm  *txrx_mm = NULL;

	intf->sipc_mm = kzalloc(sizeof(struct sipc_txrx_mm), GFP_KERNEL);
	if (!intf->sipc_mm) {
		wl_err("%s: alloc sprdwl_txrx_mm fail", __func__);
		return -1;
	}

	txrx_mm = intf->sipc_mm;
	ret = sipc_pkt_mem_map(pdev, &txrx_mm->smem);
	if (ret) {
		wl_err("%s:pkt mem map fail", __func__);
		return ret;
	}

	ret = sipc_init_tx_mm(txrx_mm);
	if (ret) {
		wl_err("%s: init_tx_mm fail", __func__);
		goto err_memunmap;
	}
	wl_err("%s: init_tx_mm success", __func__);

	ret = sipc_init_rx_mm(txrx_mm);
	if (ret) {
		wl_err("%s: init_rx_mm fail", __func__);
		goto err_init_rx;
	}
	wl_err("%s: init_rx_mm success", __func__);

	return 0;

err_init_rx:
//	kfree(txrx_mm->tx_buf);
	sipc_buf_mm_deinit(&txrx_mm->tx_buf->nlist);

err_memunmap:
	vm_unmap_ram(txrx_mm->smem.virt_base, txrx_mm->smem.page_count);
	kfree(txrx_mm);
	intf->sipc_mm = NULL;
	txrx_mm = NULL;

	return ret;
}

void sipc_mm_rx_buf_deinit(struct sprdwl_intf *intf)
{
	struct sipc_buf_mm *buf_mm = intf->sipc_mm->rx_buf;

	if (buf_mm)
		sipc_txrx_buf_single_deinit(buf_mm);
}

void sipc_mm_rx_buf_flush(struct sprdwl_intf *intf)
{
	struct sipc_buf_mm *buf_mm = intf->sipc_mm->rx_buf;
	struct sprdwl_msg_list *list;
	struct sipc_buf_node *node = NULL, *pos_node = NULL;
	unsigned long flags = 0;

	if (!buf_mm) {
		wl_err("%s:get rx mm buffer failed.\n", __func__);
		return;
	}

	list =  &buf_mm->nlist;
	spin_lock_irqsave(&list->busylock, flags);
	list_for_each_entry_safe(node, pos_node, &list->busylist, list) {
		list_del(&node->list);
		atomic_dec(&list->flow);
		memset_io(node->buf, 0, buf_mm->len);
		node->priv = NULL;
		sipc_free_node_buf(node, list);
	}
	spin_unlock_irqrestore(&list->busylock, flags);
}

void sipc_txrx_buf_deinit(struct sprdwl_intf *intf)
{
	struct sipc_txrx_mm *sipc_mm = intf->sipc_mm;
	struct sipc_buf_mm *buf_mm = NULL;

	if (sipc_mm) {
		buf_mm = sipc_mm->tx_buf;
		if (buf_mm->type == SIPC_TXRX_BUF_SINGLE_TYPE)
			sipc_txrx_buf_single_deinit(buf_mm);

		buf_mm = sipc_mm->rx_buf;
		if (buf_mm->type == SIPC_TXRX_BUF_SINGLE_TYPE)
			sipc_txrx_buf_single_deinit(buf_mm);

		if (sipc_mm->smem.virt_base)
			vm_unmap_ram(sipc_mm->smem.virt_base, sipc_mm->smem.page_count);

		kfree(sipc_mm);
		intf->sipc_mm = NULL;
		sipc_mm = NULL;
	}

}

int sipc_skb_to_tx_buf(struct sprdwl_intf *intf,
				struct sprdwl_msg_buf *msg_pos)
{
	struct sk_buff *skb = msg_pos->skb;
	unsigned long phy_addr = 0;
	struct sipc_buf_node *node = NULL;
	struct sipc_buf_mm *tx_buf = NULL;
	void *pad = NULL;

	tx_buf = intf->sipc_mm->tx_buf;
	if (!tx_buf)
		return -1;

	node = sipc_alloc_node_buf(&tx_buf->nlist);
	if (!node)
		return -1;

	sipc_queue_node_buf(node, &tx_buf->nlist);
	node->ctxt_id = msg_pos->ctxt_id;
	node->location = SIPC_LOC_TX_INTF;
	node->priv = (void *)msg_pos;
	pad = (char *)node->buf + SPRDWL_MAX_DATA_TXLEN;
	memcpy_toio(pad, &node, sizeof(node));
	memcpy_toio(node->buf, skb->data, skb->len);
	phy_addr = ((unsigned long)node->buf - tx_buf->offset) | SPRDWL_MH_ADDRESS_BIT;
	phy_addr &= SPRDWL_MH_SIPC_ADDRESS_BIT;
	memcpy_toio(node->buf, &phy_addr, MSDU_DSCR_RSVD);
	dev_kfree_skb(msg_pos->skb);
	msg_pos->skb = NULL;
	msg_pos->sipc_node = node;
	msg_pos->tran_data = node->buf;
	msg_pos->pcie_addr = phy_addr;

	return 0;
}

int sipc_rx_mm_buf_to_skb(struct sprdwl_intf *intf,
				struct sk_buff *skb)
{
	struct sipc_buf_mm *rx_buf = NULL;
	struct sipc_buf_node *node = NULL;

	rx_buf = intf->sipc_mm->rx_buf;
	memcpy_toio(&node, skb->data, sizeof(node));
	memcpy_fromio(skb->data, node->buf, SPRDWL_MAX_DATA_RXLEN);
	memset_io(node->buf, 0, rx_buf->len);
	node->priv = NULL;
	sipc_dequeue_node_to_freelist(node, &rx_buf->nlist);

	return 0;
}

struct sipc_buf_node *sipc_rx_alloc_node_buf(struct sprdwl_intf *intf)
{
	struct sipc_buf_mm *rx_buf = intf->sipc_mm->rx_buf;

	if (!rx_buf) {
		wl_err("%s:rx buf is NULL.\n", __func__);
		return NULL;
	}

	return sipc_alloc_node_buf(&rx_buf->nlist);
}


void sprdwl_sipc_txrx_buf_deinit(struct sprdwl_intf *intf)
{
	enum sprdwl_hw_type hw_type = intf->priv->hw_type;

	if (SPRDWL_HW_SIPC == hw_type)
		sipc_txrx_buf_deinit(intf);
}

void *sipc_fill_mbuf(void *data, unsigned int len)
{
	void *buf = kmalloc(len, GFP_KERNEL);
	if (buf)
		memcpy(buf, data, len);

	return buf;
}

