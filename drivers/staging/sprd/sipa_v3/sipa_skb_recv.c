// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc sipa driver
 *
 * Copyright (C) 2020 Unisoc, Inc.
 * Author: Qingsheng Li <qingsheng.li@unisoc.com>
 */

#include <linux/atomic.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/percpu-defs.h>
#include <linux/skbuff.h>
#include <linux/sched.h>
#include <linux/sipa.h>
#include <linux/netdevice.h>
#include <uapi/linux/sched/types.h>

#include "sipa_dummy.h"
#include "sipa_hal.h"
#include "sipa_priv.h"

#define SIPA_RECV_BUF_LEN	1600
#define SIPA_RECV_RSVD_LEN	NET_SKB_PAD

static int sipa_init_recv_array(struct sipa_skb_receiver *receiver, u32 depth)
{
	int i;
	struct sipa_skb_array *skb_array;
	size_t size = sizeof(struct sipa_skb_dma_addr_pair);
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	for (i = 0; i < SIPA_RECV_QUEUES_MAX; i++) {
		skb_array = devm_kzalloc(ipa->dev,
					 sizeof(struct sipa_skb_array),
					 GFP_KERNEL);
		if (!skb_array)
			return -ENOMEM;

		skb_array->array = devm_kcalloc(ipa->dev, depth,
						size, GFP_KERNEL);
		if (!skb_array->array)
			return -ENOMEM;

		skb_array->rp = 0;
		skb_array->wp = 0;
		skb_array->depth = depth;

		receiver->fill_array[i] = skb_array;
	}

	return 0;
}

void sipa_reinit_recv_array(struct device *dev)
{
	int i;
	struct sipa_skb_array *skb_array;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	if (!ipa->receiver) {
		dev_err(dev, "sipa receiver is null\n");
		return;
	}

	for (i = 0; i < SIPA_RECV_QUEUES_MAX; i++) {
		skb_array = ipa->receiver->fill_array[i];
		if (!skb_array->array) {
			dev_err(dev, "sipa p->array is null\n");
			return;
		}

		skb_array->rp = 0;
		skb_array->wp = skb_array->depth;
	}
}
EXPORT_SYMBOL(sipa_reinit_recv_array);

static int sipa_put_recv_array_node(struct sipa_skb_array *p,
				    struct sk_buff *skb, dma_addr_t dma_addr)
{
	u32 pos;

	if ((p->wp - p->rp) >= p->depth)
		return -1;

	pos = p->wp & (p->depth - 1);
	p->array[pos].skb = skb;
	p->array[pos].dma_addr = dma_addr;
	/*
	 * Ensure that we put the item to the fifo before
	 * we update the fifo wp.
	 */
	smp_wmb();
	p->wp++;
	return 0;
}

static int sipa_get_recv_array_node(struct sipa_skb_array *p,
				    struct sk_buff **skb, dma_addr_t *dma_addr)
{
	u32 pos;

	if (p->rp == p->wp)
		return -1;

	pos = p->rp & (p->depth - 1);
	*skb = p->array[pos].skb;
	*dma_addr = p->array[pos].dma_addr;
	/*
	 * Ensure that we remove the item from the fifo before
	 * we update the fifo rp.
	 */
	smp_wmb();
	p->rp++;
	return 0;
}

static struct sk_buff *sipa_alloc_recv_skb(u32 req_len, u8 rsvd)
{
	u32 hr;
	struct sk_buff *skb;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	skb = __dev_alloc_skb(req_len + rsvd,
			      GFP_KERNEL | GFP_NOWAIT | GFP_ATOMIC);
	if (!skb) {
		dev_err(ipa->dev, "failed to alloc skb!\n");
		return NULL;
	}

	/* save skb ptr to skb->data */
	hr = skb_headroom(skb);
	if (hr < rsvd)
		skb_reserve(skb, rsvd - hr);

	return skb;
}

static void sipa_prepare_free_node_init(struct sipa_skb_receiver *receiver,
					u32 cnt, int cpu_num)
{
	int i, j;
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	struct sipa_node_desc_tag *item;
	struct sipa_skb_array *fill_arrays;
	u32 fail_cnt = 0, success_cnt = 0;
	enum sipa_cmn_fifo_index fifo_id = receiver->ep->recv_fifo.idx;

	for (j = 0; j < cpu_num; j++) {
		fill_arrays = receiver->fill_array[j];
		for (i = 0; i < cnt; i++) {
			item = sipa_hal_get_rx_node_wptr(receiver->dev,
							 fifo_id + j, i);
			if (!item) {
				dev_err(receiver->dev,
					"sipa fifo_id %d + j %d hal get rx node wptr err\n",
					fifo_id, j);
				fail_cnt++;
				break;
			}

			skb = sipa_alloc_recv_skb(SIPA_RECV_BUF_LEN,
						  receiver->rsvd);
			if (!skb) {
				dev_err(receiver->dev, "sipa alloc recv skb err\n");
				fail_cnt++;
				break;
			}
			skb_put(skb, SIPA_RECV_BUF_LEN);

			dma_addr = dma_map_single(receiver->dev, skb->head,
						  SIPA_RECV_BUF_LEN +
						  skb_headroom(skb),
						  DMA_FROM_DEVICE);
			if (dma_mapping_error(receiver->dev, dma_addr)) {
				dev_err(receiver->dev,
					"prepare free node dma map err\n");
				fail_cnt++;
				break;
			}

			sipa_put_recv_array_node(fill_arrays, skb, dma_addr);

			item->address = dma_addr;
			item->length = skb->len;
			item->offset = skb_headroom(skb);
			item->hash = j;

			success_cnt++;
		}

		sipa_hal_sync_node_to_rx_fifo(receiver->dev,
					      receiver->ep->recv_fifo.idx + j,
					      cnt);
	}

	if (fail_cnt)
		dev_err(receiver->dev,
			"ep->id = %d fail_cnt = %d s_cnt = %d j = %d i = %d\n",
			receiver->ep->id, fail_cnt, success_cnt, j, i);
}

/**
 * Because the ipa endpoint to the AP has the dma copy function, it it necessary
 * to call this interface to fill the new free buff after reading the data.
 */
void sipa_fill_free_fifo(u32 index)
{
	int i;
	u32 fail_cnt = 0;
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	u32 success_cnt = 0, depth;
	struct sipa_node_desc_tag *item = NULL;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	struct sipa_skb_receiver *receiver = ipa->receiver;
	struct sipa_skb_array *fill_array = receiver->fill_array[index];

	atomic_inc(&receiver->check_flag);

	if (atomic_read(&receiver->check_suspend)) {
		dev_warn(receiver->dev,
			 "encountered ipa suspend while fill free fifo cpu = %d\n",
			 smp_processor_id());
		atomic_dec(&receiver->check_flag);
		return;
	}

	depth = receiver->ep->recv_fifo.rx_fifo.fifo_depth;
	if (atomic_read(&fill_array->need_fill_cnt) > (depth - depth / 4)) {
		dev_warn(receiver->dev,
			 "ep id = %d free node is not enough,need fill %d cpu %d\n",
			 receiver->ep->id,
			 atomic_read(&fill_array->need_fill_cnt),
			 smp_processor_id());
		receiver->rx_danger_cnt++;
	}

	for (i = 0; i < atomic_read(&fill_array->need_fill_cnt); i++) {
		item = sipa_hal_get_rx_node_wptr(receiver->dev,
						 receiver->ep->recv_fifo.idx +
						 index, i);
		if (!item) {
			fail_cnt++;
			break;
		}

		skb = sipa_alloc_recv_skb(SIPA_RECV_BUF_LEN, receiver->rsvd);
		if (!skb) {
			fail_cnt++;
			break;
		}
		skb_put(skb, SIPA_RECV_BUF_LEN);

		dma_addr = dma_map_single(receiver->dev, skb->head,
					  SIPA_RECV_BUF_LEN + skb_headroom(skb),
					  DMA_FROM_DEVICE);
		if (dma_mapping_error(receiver->dev, dma_addr)) {
			dev_err(receiver->dev,
				"prepare free node dma map err\n");
			fail_cnt++;
			dev_kfree_skb_any(skb);
			break;
		}

		item->address = dma_addr;
		item->length = skb->len;
		item->offset = skb_headroom(skb);
		item->hash = index;

		sipa_put_recv_array_node(fill_array, skb, dma_addr);
		success_cnt++;
	}

	sipa_hal_sync_node_to_rx_fifo(receiver->dev,
				      receiver->ep->recv_fifo.idx + index,
				      success_cnt);

	if (success_cnt) {
		sipa_hal_add_rx_fifo_wptr(receiver->dev,
					  receiver->ep->recv_fifo.idx +
					  index,
					  success_cnt);
		if (atomic_read(&fill_array->need_fill_cnt) > 0)
			atomic_sub(success_cnt, &fill_array->need_fill_cnt);
	}
	if (fail_cnt)
		dev_err(receiver->dev, "fill free fifo fail_cnt = %d\n",
			fail_cnt);
	atomic_dec(&receiver->check_flag);
}
EXPORT_SYMBOL(sipa_fill_free_fifo);

void sipa_fill_all_free_fifo(void)
{
	int i;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	struct sipa_skb_receiver *receiver = ipa->receiver;

	for (i = 0; i < SIPA_RECV_QUEUES_MAX; i++) {
		if (atomic_read(&receiver->fill_array[i]->need_fill_cnt))
			sipa_fill_free_fifo(i);
	}
}

void sipa_recv_wake_up(void)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	wake_up(&ipa->receiver->fill_recv_waitq);
}
EXPORT_SYMBOL(sipa_recv_wake_up);

void sipa_init_free_fifo(struct sipa_skb_receiver *receiver, u32 cnt,
			 enum sipa_cmn_fifo_index id)
{
	int i;
	struct sipa_skb_array *skb_array;

	i = id - SIPA_FIFO_MAP0_OUT;
	if (i >= SIPA_RECV_QUEUES_MAX)
		return;

	skb_array = receiver->fill_array[i];
	sipa_hal_add_rx_fifo_wptr(receiver->dev,
				  receiver->ep->recv_fifo.idx + i,
				  cnt);
	if (atomic_read(&skb_array->need_fill_cnt) > 0)
		dev_info(receiver->dev,
			 "a very serious problem, mem cover may appear\n");

	atomic_set(&skb_array->need_fill_cnt, 0);
}

static void sipa_receiver_notify_cb(void *priv, enum sipa_hal_evt_type evt,
				    unsigned long data)
{
	struct sipa_skb_receiver *receiver = (struct sipa_skb_receiver *)priv;

	if (evt & SIPA_RECV_WARN_EVT) {
		dev_err(receiver->dev,
			"sipa maybe poor resources evt = 0x%x\n", evt);
		receiver->tx_danger_cnt++;
	}

	sipa_dummy_recv_trigger(smp_processor_id());
}

struct sk_buff *sipa_recv_skb(struct sipa_skb_receiver *receiver,
			      int *netid, u32 *src_id, u32 index)
{
	dma_addr_t addr;
	int ret, retry = 10;
	enum sipa_cmn_fifo_index id;
	struct sk_buff *recv_skb = NULL;
	struct sipa_node_desc_tag *node = NULL;
	struct sipa_skb_array *fill_array =
		receiver->fill_array[smp_processor_id()];

	atomic_inc(&receiver->check_flag);

	if (atomic_read(&receiver->check_suspend)) {
		dev_warn(receiver->dev,
			 "encounter ipa suspend while reading data cpu = %d\n",
			 smp_processor_id());
		atomic_dec(&receiver->check_flag);
		return NULL;
	}

	id = receiver->ep->recv_fifo.idx + smp_processor_id();
	if (sipa_hal_get_tx_fifo_empty_status(receiver->dev, id)) {
		atomic_dec(&receiver->check_flag);
		return NULL;
	}

	node = sipa_hal_get_tx_node_rptr(receiver->dev, id, index);
	if (!node) {
		dev_err(receiver->dev, "recv node is null\n");
		sipa_hal_add_tx_fifo_rptr(receiver->dev, id, 1);
		atomic_dec(&receiver->check_flag);
		return NULL;
	}

	ret = sipa_get_recv_array_node(fill_array, &recv_skb, &addr);
	atomic_inc(&fill_array->need_fill_cnt);

check_again:
	if (ret) {
		dev_err(receiver->dev, "recv addr:0x%llx, but recv_array is empty\n",
			(u64)node->address);
		atomic_dec(&receiver->check_flag);
		return NULL;
	} else if ((addr != node->address || !node->src) && retry--) {
		sipa_hal_sync_node_from_tx_fifo(receiver->dev, id, -1);
		goto check_again;
	} else if ((addr != node->address || !node->src) && !retry) {
		dma_unmap_single(receiver->dev, addr,
				 SIPA_RECV_BUF_LEN + skb_headroom(recv_skb),
				 DMA_FROM_DEVICE);
		dev_kfree_skb_any(recv_skb);
		sipa_hal_add_tx_fifo_rptr(receiver->dev, id, 1);
		atomic_dec(&receiver->check_flag);
		dev_info(receiver->dev,
			 "recv addr:0x%llx, recv_array addr:0x%llx not equal retry = %d src = %d\n",
			 node->address, (u64)addr, retry, node->src);
		return NULL;
	}

	*netid = node->net_id;
	*src_id = node->src;

	if (node->checksum == 0xffff)
		recv_skb->ip_summed = CHECKSUM_UNNECESSARY;
	else
		recv_skb->ip_summed = CHECKSUM_NONE;

	dma_unmap_single(receiver->dev, addr,
			 SIPA_RECV_BUF_LEN + skb_headroom(recv_skb),
			 DMA_FROM_DEVICE);

	/* trim to the real length, hope it's not a fake len */
	skb_trim(recv_skb, node->length);
	atomic_dec(&receiver->check_flag);

	return recv_skb;
}

static int sipa_check_need_fill_cnt(void)
{
	int i;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	struct sipa_skb_receiver *receiver = ipa->receiver;

	for (i = 0 ; i < SIPA_RECV_QUEUES_MAX; i++) {
		if (atomic_read(&receiver->fill_array[i]->need_fill_cnt))
			return true;
	}

	return 0;
}

static int sipa_fill_recv_thread(void *data)
{
	struct sipa_skb_receiver *receiver = (struct sipa_skb_receiver *)data;
	struct sched_param param = {.sched_priority = 90};

	sched_setscheduler(current, SCHED_RR, &param);

	while (!kthread_should_stop()) {
		wait_event_interruptible(receiver->fill_recv_waitq,
					 sipa_check_need_fill_cnt());
		sipa_fill_all_free_fifo();
	}

	return 0;
}

int sipa_receiver_prepare_suspend(struct sipa_skb_receiver *receiver)
{
	int i;
	atomic_set(&receiver->check_suspend, 1);

	if (atomic_read(&receiver->check_flag)) {
		dev_err(receiver->dev,
			"task recv %d is running\n", receiver->ep->id);
		atomic_set(&receiver->check_suspend, 0);
		return -EAGAIN;
	}

	for (i = 0; i < SIPA_RECV_QUEUES_MAX; i++) {
		if (!sipa_hal_get_tx_fifo_empty_status(receiver->dev,
					receiver->ep->recv_fifo.idx + i)) {
			dev_err(receiver->dev, "sipa recv fifo %d tx fifo is not empty\n",
				receiver->ep->recv_fifo.idx);
			atomic_set(&receiver->check_suspend, 0);
			return -EAGAIN;
		}
	}

	if (sipa_check_need_fill_cnt()) {
		dev_err(receiver->dev, "task fill_recv %d\n",
			receiver->ep->id);
		atomic_set(&receiver->check_suspend, 0);
		return -EAGAIN;
	}

	return 0;
}

int sipa_receiver_prepare_resume(struct sipa_skb_receiver *receiver)
{
	atomic_set(&receiver->check_suspend, 0);

	wake_up_process(receiver->fill_recv_thread);

	return sipa_hal_cmn_fifo_stop_recv(receiver->dev,
					   receiver->ep->recv_fifo.idx,
					   false);
}

static void sipa_receiver_init(struct sipa_skb_receiver *receiver, u32 rsvd)
{
	int i;
	u32 depth;
	struct sipa_comm_fifo_params attr;

	/* timeout = 1 / ipa_sys_clk * 1024 * value */
	attr.tx_intr_delay_us = 0x64;
	attr.tx_intr_threshold = 0x30;
	attr.flowctrl_in_tx_full = true;
	attr.flow_ctrl_cfg = flow_ctrl_rx_empty;
	attr.flow_ctrl_irq_mode = enter_exit_flow_ctrl;
	attr.rx_enter_flowctrl_watermark =
		receiver->ep->recv_fifo.rx_fifo.fifo_depth / 4;
	attr.rx_leave_flowctrl_watermark =
		receiver->ep->recv_fifo.rx_fifo.fifo_depth / 2;
	attr.tx_enter_flowctrl_watermark = 0;
	attr.tx_leave_flowctrl_watermark = 0;

	dev_info(receiver->dev,
		 "ep_id = %d fifo_id = %d rx_fifo depth = 0x%x queues = %d\n",
		 receiver->ep->id,
		 receiver->ep->recv_fifo.idx,
		 receiver->ep->recv_fifo.rx_fifo.fifo_depth,
		 SIPA_RECV_QUEUES_MAX);
	for (i = 0; i < SIPA_RECV_CMN_FIFO_NUM; i++)
		sipa_hal_open_cmn_fifo(receiver->dev,
				       receiver->ep->recv_fifo.idx + i,
				       &attr, NULL, true,
				       sipa_receiver_notify_cb,
				       receiver);

	/* reserve space for dma flushing cache issue */
	receiver->rsvd = rsvd;
	receiver->init_flag = true;

	atomic_set(&receiver->check_suspend, 0);
	atomic_set(&receiver->check_flag, 0);

	depth = receiver->ep->recv_fifo.rx_fifo.fifo_depth;
	sipa_prepare_free_node_init(receiver, depth, SIPA_RECV_QUEUES_MAX);
}

int sipa_create_skb_receiver(struct sipa_plat_drv_cfg *ipa,
			     struct sipa_endpoint *ep,
			     struct sipa_skb_receiver **receiver_pp)
{
	struct sipa_skb_receiver *receiver = NULL;

	dev_info(ipa->dev, "ep->id = %d start\n", ep->id);
	receiver = devm_kzalloc(ipa->dev,
				sizeof(struct sipa_skb_receiver), GFP_KERNEL);
	if (!receiver)
		return -ENOMEM;

	receiver->dev = ipa->dev;
	receiver->ep = ep;
	receiver->rsvd = SIPA_RECV_RSVD_LEN;

	sipa_init_recv_array(receiver,
			     receiver->ep->recv_fifo.rx_fifo.fifo_depth);

	spin_lock_init(&receiver->lock);

	init_waitqueue_head(&receiver->fill_recv_waitq);
	sipa_receiver_init(receiver, SIPA_RECV_RSVD_LEN);

	receiver->fill_recv_thread = kthread_create(sipa_fill_recv_thread,
						    receiver,
						    "sipa-fill-recv-%d",
						    ep->id);
	if (IS_ERR(receiver->fill_recv_thread)) {
		dev_err(receiver->dev,
			"Failed to create kthread: sipa-fill-recv-%d\n",
			ep->id);
		return PTR_ERR(receiver->fill_recv_thread);
	}

	*receiver_pp = receiver;
	return 0;
}
