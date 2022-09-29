/*
 * Copyright (C) 2016 Spreadtrum Communications Inc.
 *
 * Authors	:
 * star.liu <star.liu@spreadtrum.com>
 * yifei.li <yifei.li@spreadtrum.com>
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

#include <linux/platform_device.h>
#include <linux/utsname.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/ip.h>
#include "tx_msg_sc2355.h"
#include "rx_msg_sc2355.h"
#include "cfg80211.h"
#include "core_sc2355.h"
#include "sprdwl.h"
#include "if_sc2355.h"
#include "mm.h"
#include "intf_ops.h"
#include "cmdevt.h"
#include "debug.h"
#include "work.h"
#include <linux/kthread.h>
#include "txrx_buf_mm.h"
#ifdef SIPC_SUPPORT
#include "sipc_txrx_mm.h"
#endif

unsigned int g_max_fw_tx_dscr;

struct sprdwl_msg_buf *sprdwl_get_msg_buf(void *pdev,
					  enum sprdwl_head_type type,
					  enum sprdwl_mode mode,
					  u8 ctx_id)
{
	struct sprdwl_intf *dev;
	struct sprdwl_msg_buf *msg = NULL;
	struct sprdwl_msg_list *list = NULL;
	struct sprdwl_tx_msg *sprdwl_tx_dev = NULL;
#if defined(MORE_DEBUG)
	struct timespec tx_begin;
#endif

	dev = (struct sprdwl_intf *)pdev;
	sprdwl_tx_dev = (struct sprdwl_tx_msg *)dev->sprdwl_tx;
	sprdwl_tx_dev->mode = mode;

	if (unlikely(dev->exit)) {
		wl_err("%s can not get msg_buf: intf->exit\n", __func__);
		return NULL;
	}

	if (type == SPRDWL_TYPE_DATA)
		list = &sprdwl_tx_dev->tx_list_qos_pool;
	else
		list = &sprdwl_tx_dev->tx_list_cmd;

	if (!list) {
		wl_err("%s: type %d could not get list\n", __func__, type);
		return NULL;
	}

	msg = sprdwl_alloc_msg_buf(list);

	if (msg) {
#if defined(MORE_DEBUG)
		getnstimeofday(&tx_begin);
		msg->tx_start_time = timespec_to_ns(&tx_begin);
#endif
		if (type == SPRDWL_TYPE_DATA)
			msg->msg_type = SPRDWL_TYPE_DATA;
		msg->type = type;
		msg->msglist = list;
		msg->mode = mode;
		msg->ctxt_id = ctx_id;
		msg->skb = NULL;
		msg->node = NULL;
		msg->xmit_msg_list = &sprdwl_tx_dev->xmit_msg_list;
		return msg;
	}

	if (type == SPRDWL_TYPE_DATA) {
		sprdwl_tx_dev->net_stop_cnt++;
		sprdwl_net_flowcontrl(dev->priv, mode, false);
		atomic_set(&list->flow, 1);
	}
	printk_ratelimited("%s no more msgbuf for %s\n",
			   __func__, type == SPRDWL_TYPE_DATA ?
			   "data" : "cmd");

	return NULL;
}

static void sprdwl_dequeue_cmd_buf(struct sprdwl_msg_buf *msg_buf,
				    struct sprdwl_msg_list *list)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&list->busylock, flags);
	list_del(&msg_buf->list);
	spin_unlock_irqrestore(&list->busylock, flags);

	//spin_lock_irqsave(&list->complock);
	spin_lock_irqsave(&list->complock, flags);
	list_add_tail(&msg_buf->list, &list->cmd_to_free);
	spin_unlock_irqrestore(&list->complock, flags);
	//spin_unlock_irqrestore(&list->complock);
}

void sprdwl_free_cmd_buf(struct sprdwl_msg_buf *msg_buf,
			   struct sprdwl_msg_list *list)
{
	unsigned long flags = 0;

	//spin_lock_irqsave(&list->complock);
	spin_lock_irqsave(&list->complock, flags);
	list_del(&msg_buf->list);
	//spin_unlock_irqrestore(&list->complock);
	spin_unlock_irqrestore(&list->complock, flags);
	sprdwl_free_msg_buf(msg_buf, list);
}

void sprdwl_tx_free_msg_buf(void *pdev, struct sprdwl_msg_buf *msg)
{
	sprdwl_free_msg_buf(msg, msg->msglist);
}

static inline void
sprdwl_queue_data_msg_buf(struct sprdwl_msg_buf *msg_buf)
{
	spin_lock_bh(&msg_buf->data_list->p_lock);
	list_add_tail(&msg_buf->list, &msg_buf->data_list->head_list);
	atomic_inc(&msg_buf->data_list->l_num);
	spin_unlock_bh(&msg_buf->data_list->p_lock);
}

static inline void
sprdwl_dequeue_qos_buf(struct sprdwl_msg_buf *msg_buf, int ac_index)
{
	spinlock_t *lock;/*to lock qos list*/

	if (ac_index != SPRDWL_AC_MAX)
		lock = &msg_buf->data_list->p_lock;
	else
		lock = &msg_buf->xmit_msg_list->send_lock;
	spin_lock_bh(lock);
	if (msg_buf->skb)
		dev_kfree_skb(msg_buf->skb);
	if (msg_buf->node)
		sprdwl_free_tx_buf(msg_buf->node);
	list_del(&msg_buf->list);
	sprdwl_free_msg_buf(msg_buf, msg_buf->msglist);
	spin_unlock_bh(lock);
}

void sprdwl_dequeue_tofreelist_buf(struct sprdwl_tx_msg *tx_msg,
								   struct sprdwl_msg_buf *msg_buf)
{
	enum sprdwl_hw_type hw_type = tx_msg->intf->priv->hw_type;

	if (msg_buf->skb)
		dev_kfree_skb(msg_buf->skb);

	if (SPRDWL_HW_SIPC == hw_type) {
#ifdef SIPC_SUPPORT
		if (msg_buf->sipc_node) {
			sipc_free_tx_buf(tx_msg->intf, msg_buf->sipc_node);
		}
#endif
	} else if (SPRDWL_HW_SC2355_PCIE == hw_type) {
		if (msg_buf->node) {
			sprdwl_free_tx_buf(msg_buf->node);
		}
	}

	list_del(&msg_buf->list);
	sprdwl_free_msg_buf(msg_buf, msg_buf->msglist);
}

void sprdwl_flush_tx_qoslist(struct sprdwl_tx_msg *tx_msg, int mode, int ac_index, int lut_index)
{
	/*peer list lock*/
	spinlock_t *plock;
	struct sprdwl_msg_buf *pos_buf, *temp_buf;
	struct list_head *data_list;

	data_list =
	&tx_msg->tx_list[mode]->q_list[ac_index].p_list[lut_index].head_list;

	plock =
	&tx_msg->tx_list[mode]->q_list[ac_index].p_list[lut_index].p_lock;

	if (!list_empty(data_list)) {
		spin_lock_bh(plock);

		list_for_each_entry_safe(pos_buf, temp_buf,
					 data_list, list) {
			dev_kfree_skb(pos_buf->skb);
			list_del(&pos_buf->list);
			sprdwl_free_msg_buf(pos_buf, pos_buf->msglist);
		}

		spin_unlock_bh(plock);

		atomic_sub(atomic_read(&tx_msg->tx_list[mode]->q_list[ac_index].p_list[lut_index].l_num),
					&tx_msg->tx_list[mode]->mode_list_num);
		atomic_set(&tx_msg->tx_list[mode]->q_list[ac_index].p_list[lut_index].l_num, 0);
	}
}

void sprdwl_flush_mode_txlist(struct sprdwl_tx_msg *tx_msg, enum sprdwl_mode mode)
{
	int i, j;
	/*peer list lock*/
	spinlock_t *plock;
	struct sprdwl_msg_buf *pos_buf, *temp_buf;
	struct list_head *data_list;

	wl_debug("%s, mode=%d\n", __func__, mode);

	for (i = 0; i < SPRDWL_AC_MAX; i++) {
		for (j = 0; j < MAX_LUT_NUM; j++) {
			data_list =
			&tx_msg->tx_list[mode]->q_list[i].p_list[j].head_list;
			if (list_empty(data_list))
				continue;
			plock =
			&tx_msg->tx_list[mode]->q_list[i].p_list[j].p_lock;

			spin_lock_bh(plock);

			list_for_each_entry_safe(pos_buf, temp_buf,
						 data_list, list) {
				dev_kfree_skb(pos_buf->skb);
				list_del(&pos_buf->list);
				sprdwl_free_msg_buf(pos_buf, pos_buf->msglist);
			}

			spin_unlock_bh(plock);

			atomic_set(&tx_msg->tx_list[mode]->q_list[i].p_list[j].l_num, 0);
		}
	}
	atomic_set(&tx_msg->tx_list[mode]->mode_list_num, 0);
}

void sprdwl_flush_tosendlist(struct sprdwl_tx_msg *tx_msg)
{
	struct sprdwl_msg_buf *pos_buf, *temp_buf;
	struct list_head *data_list;

	wl_err("%s, %d\n", __func__, __LINE__);
	if (!list_empty(&tx_msg->xmit_msg_list.to_send_list)) {
		data_list = &tx_msg->xmit_msg_list.to_send_list;
		list_for_each_entry_safe(pos_buf, temp_buf,
			data_list, list) {
			sprdwl_dequeue_qos_buf(pos_buf, SPRDWL_AC_MAX);
		}
	}
}

static void sprdwl_flush_data_txlist(struct sprdwl_tx_msg *tx_msg)
{
	enum sprdwl_mode mode;
	struct list_head *to_free_list;
	int cnt = 0;
	struct sprdwl_priv *priv = tx_msg->intf->priv;

	for (mode = SPRDWL_MODE_STATION; mode < SPRDWL_MODE_MAX; mode++) {
		if (atomic_read(&tx_msg->tx_list[mode]->mode_list_num) == 0)
			continue;
		sprdwl_flush_mode_txlist(tx_msg, mode);
	}

	sprdwl_flush_tosendlist(tx_msg);
	to_free_list = &tx_msg->xmit_msg_list.to_free_list;
	/*wait until data list sent completely and freed by HIF*/
	wl_err("%s check if data freed complete start, freenum=%d\n",
		__func__,
		atomic_read(&tx_msg->xmit_msg_list.free_num));
	while (!list_empty(to_free_list) && (cnt < 1000)) {
		if (priv->hw_type == SPRDWL_HW_SC2355_PCIE &&
			sprdwcn_bus_get_status() == WCN_BUS_DOWN) {
			struct sprdwl_msg_buf *pos_buf, *temp_buf;
			unsigned long lockflag_txfree = 0;

			spin_lock_irqsave(&tx_msg->xmit_msg_list.free_lock,
					  lockflag_txfree);
			list_for_each_entry_safe(pos_buf, temp_buf,
				to_free_list, list)
				sprdwl_dequeue_tofreelist_buf(tx_msg, pos_buf);
			spin_unlock_irqrestore(&tx_msg->xmit_msg_list.free_lock,
					       lockflag_txfree);
			goto out;
		}
		usleep_range(2500, 3000);
		cnt++;
	}
out:
	wl_err("%s check if data freed complete end\n", __func__);
}

void sprdwl_dequeue_data_buf(struct sprdwl_msg_buf *msg_buf)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&msg_buf->xmit_msg_list->free_lock, flags);
	list_del(&msg_buf->list);
	spin_unlock_irqrestore(&msg_buf->xmit_msg_list->free_lock, flags);
	sprdwl_free_msg_buf(msg_buf, msg_buf->msglist);
}

void sprdwl_dequeue_data_list(struct mbuf_t *head, int num)
{
	int i;
	struct sprdwl_msg_buf *msg_pos;
	struct mbuf_t *mbuf_pos = NULL;
	unsigned long flags = 0;

	mbuf_pos = head;
	for (i = 0; i < num; i++) {
		msg_pos = GET_MSG_BUF(mbuf_pos);
		/*TODO, check msg_buf after pop link*/
		if (msg_pos == NULL ||
		    !virt_addr_valid(msg_pos) ||
		    !virt_addr_valid(msg_pos->skb)) {
			wl_err("%s,%d, error! wrong sprdwl_msg_buf\n",
			       __func__, __LINE__);
			BUG_ON(1);
			return;
		}
		dev_kfree_skb(msg_pos->skb);
		/*delete node from to_free_list*/
		spin_lock_irqsave(&msg_pos->xmit_msg_list->free_lock, flags);
		list_del(&msg_pos->list);
		spin_unlock_irqrestore(&msg_pos->xmit_msg_list->free_lock, flags);
		/*add it to free_list*/
		//spin_lock_irqsave(&msg_pos->msglist->freelock);
		spin_lock_irqsave(&msg_pos->msglist->freelock, flags);
		list_add_tail(&msg_pos->list, &msg_pos->msglist->freelist);
		//spin_unlock_irqrestore(&msg_pos->msglist->freelock);
		spin_unlock_irqrestore(&msg_pos->msglist->freelock, flags);
		mbuf_pos = mbuf_pos->next;
	}
}

/*To clear mode assigned in flow_ctrl
 *and to flush data lit of closed mode
 */
void handle_tx_status_after_close(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;
	enum sprdwl_mode mode;
	u8 i, allmode_closed = 1;
	struct sprdwl_intf *intf;
	struct sprdwl_tx_msg *tx_msg;

	for (mode = SPRDWL_MODE_STATION; mode < SPRDWL_MODE_MAX; mode++) {
		if (priv->fw_stat[mode] != SPRDWL_INTF_CLOSE) {
			allmode_closed = 0;
			break;
		}
	}
	intf = (struct sprdwl_intf *)(vif->priv->hw_priv);
	tx_msg = (struct sprdwl_tx_msg *)intf->sprdwl_tx;
	if (allmode_closed == 1) {
		/*all modee closed,
		 *reset all credit
		*/
		wl_info("%s, %d, _fc_, delete flow num after all closed\n",
			__func__, __LINE__);
		for (i = 0; i < MAX_COLOR_BIT; i++) {
			tx_msg->flow_ctrl[i].mode = SPRDWL_MODE_NONE;
			tx_msg->flow_ctrl[i].color_bit = i;
			tx_msg->ring_cp = 0;
			tx_msg->ring_ap = 0;
			atomic_set(&tx_msg->flow_ctrl[i].flow, 0);
		}
		sprdwl_rx_flush_buffer(priv->hw_priv);
	} else {
		/*a mode closed,
		 *remove it from flow control to
		 *make it shared by other still open mode
		*/
		for (i = 0; i < MAX_COLOR_BIT; i++) {
			if (tx_msg->flow_ctrl[i].mode == vif->mode) {
				wl_info(" %s, %d, _fc_, clear mode%d because closed\n",
					__func__, __LINE__, vif->mode);
				tx_msg->flow_ctrl[i].mode = SPRDWL_MODE_NONE;
			}
		}
		/*if tx_list[mode] not empty,
		 *but mode is closed, should flush it
		*/
		if (priv->fw_stat[mode] == SPRDWL_INTF_CLOSE &&
			atomic_read(&tx_msg->tx_list[mode]->mode_list_num) != 0)
			sprdwl_flush_mode_txlist(tx_msg, mode);
	}
}

void sprdwl_init_xmit_list(struct sprdwl_tx_msg *tx_msg)
{
	INIT_LIST_HEAD(&tx_msg->xmit_msg_list.to_send_list);
	INIT_LIST_HEAD(&tx_msg->xmit_msg_list.to_free_list);
	spin_lock_init(&tx_msg->xmit_msg_list.send_lock);
	spin_lock_init(&tx_msg->xmit_msg_list.free_lock);
}

static void
add_xmit_list_tail(struct sprdwl_tx_msg *tx_msg,
		   struct peer_list *p_list,
		   int add_num)
{
	struct list_head *pos_list = NULL, *n_list;
	struct list_head temp_list;
	int num = 0;

	if (add_num == 0)
		return;
	spin_lock_bh(&p_list->p_lock);
	if (list_empty(&p_list->head_list)) {
		spin_unlock_bh(&p_list->p_lock);
		return;
	}
	list_for_each_safe(pos_list, n_list, &p_list->head_list) {
		num++;
		if (num == add_num)
			break;
	}
	if (num != add_num)
		wl_err("%s, %d, error! add_num:%d, num:%d\n",
		       __func__, __LINE__, add_num, num);
	INIT_LIST_HEAD(&temp_list);
	list_cut_position(&temp_list,
			  &p_list->head_list,
			  pos_list);
	list_splice_tail(&temp_list,
		&tx_msg->xmit_msg_list.to_send_list);
	if (list_empty(&p_list->head_list))
		INIT_LIST_HEAD(&p_list->head_list);
	spin_unlock_bh(&p_list->p_lock);
	wl_debug("%s,%d,q_num%d,tosend_num%d\n", __func__, __LINE__,
		 get_list_num(&p_list->head_list),
		 get_list_num(&tx_msg->xmit_msg_list.to_send_list));
}

unsigned int queue_is_empty(struct sprdwl_tx_msg *tx_msg, enum sprdwl_mode mode)
{
	int i, j;
	struct tx_t *tx_t_list = tx_msg->tx_list[mode];

	if (mode == SPRDWL_MODE_AP || mode == SPRDWL_MODE_P2P_GO) {
		for (i = 0;  i < SPRDWL_AC_MAX; i++) {
			for (j = 0;  j < MAX_LUT_NUM; j++) {
				if (!list_empty(&tx_t_list->q_list[i].p_list[j].head_list))
					return 0;
			}
		}
		return 1;
	}
	/*other mode, STA/GC/...*/
	j = tx_msg->tx_list[mode]->lut_id;
	for (i = 0;  i < SPRDWL_AC_MAX; i++) {
		if (!list_empty(&tx_t_list->q_list[i].p_list[j].head_list))
			return 0;
	}
	return 1;
}

void sprdwl_wake_net_ifneed(struct sprdwl_intf *dev,
			    struct sprdwl_msg_list *list,
			    enum sprdwl_mode mode)
{
	struct sprdwl_tx_msg *tx_msg = (struct sprdwl_tx_msg *)dev->sprdwl_tx;

	if (atomic_read(&list->flow)) {
		if (atomic_read(&list->ref) <= SPRDWL_TX_DATA_START_NUM) {
			atomic_set(&list->flow, 0);
			tx_msg->net_start_cnt++;
			sprdwl_net_flowcontrl(dev->priv, mode, true);
		}
	}
}

static void sprdwl_sdio_flush_txlist(struct sprdwl_msg_list *list)
{
	struct sprdwl_msg_buf *msgbuf;
	int cnt = 0;

	/*wait until cmd list sent completely and freed by HIF*/
	while (!list_empty(&list->cmd_to_free) && (cnt < 1000)) {
		wl_debug("%s cmd not yet transmited", __func__);
		usleep_range(2500, 3000);
		cnt++;
	}
	while ((msgbuf = sprdwl_peek_msg_buf(list))) {
		if (msgbuf->skb)
			dev_kfree_skb(msgbuf->skb);
		else
			kfree(msgbuf->tran_data);
		sprdwl_dequeue_msg_buf(msgbuf, list);
	}
}

static int sprdwl_tx_cmd(struct sprdwl_intf *intf, struct sprdwl_msg_list *list)
{
	int i, ret = 0;
	struct sprdwl_msg_buf *msgbuf;
	struct sprdwl_tx_msg *tx_msg;

	tx_msg = (struct sprdwl_tx_msg *)intf->sprdwl_tx;
	while ((msgbuf = sprdwl_peek_msg_buf(list))) {
		if (unlikely(intf->exit)) {
			kfree(msgbuf->tran_data);
			msgbuf->tran_data = NULL;
			sprdwl_dequeue_msg_buf(msgbuf, list);
			continue;
		}
		if (time_after(jiffies, msgbuf->timeout)) {
			tx_msg->drop_cmd_cnt++;
			wl_err("tx drop cmd msg,dropcnt:%lu\n",
			       tx_msg->drop_cmd_cnt);
			kfree(msgbuf->tran_data);
			msgbuf->tran_data = NULL;
			sprdwl_dequeue_msg_buf(msgbuf, list);
			continue;
		}
		sprdwl_dequeue_cmd_buf(msgbuf, list);
		tx_msg->cmd_send++;
		wl_info("tx_cmd cmd_send num: %d\n", tx_msg->cmd_send);

		/*TBD, temp solution: send CMD one by one*/
		ret = if_tx_cmd(intf, (unsigned char *)msgbuf->tran_data,
				msgbuf->len);
		if (ret) {
			wl_err("%s, %d, tx cmd err:%d firstly\n", __func__, __LINE__, ret);
			for (i = 0; i < 25; i++) {
				if (intf->cp_asserted) {
					wl_err("%s, %d, cp assert!\n", __func__, __LINE__);
					break;
				}
				msleep(80);
				ret = if_tx_cmd(intf, (unsigned char *)msgbuf->tran_data,
					msgbuf->len);
				if (ret)
					wl_err("%s, %d, tx cmd retry time:%d err:%d\n", __func__, __LINE__, i, ret);
				else
					break;
			}
			if (ret) {
				wl_err("%s, %d, tx cmd err:%d lastly\n", __func__, __LINE__, ret);
				/* fixme if need retry */
				kfree(msgbuf->tran_data);
				msgbuf->tran_data = NULL;
				sprdwl_free_cmd_buf(msgbuf, list);
			}
		}
	}

	return 0;
}

void sprdwl_fc_add_share_credit(struct sprdwl_vif *vif)
{
	struct sprdwl_intf *intf;
	struct sprdwl_tx_msg *tx_msg;
	u8 i;

	intf = (struct sprdwl_intf *)(vif->priv->hw_priv);
	tx_msg = (struct sprdwl_tx_msg *)intf->sprdwl_tx;
	for (i = 0; i < MAX_COLOR_BIT; i++) {
		if (tx_msg->flow_ctrl[i].mode == vif->mode) {
			wl_err("%s, %d, mode:%d closed, index:%d, share it\n",
			       __func__, __LINE__,
			       vif->mode, i);
			tx_msg->flow_ctrl[i].mode = SPRDWL_MODE_NONE;
			break;
		}
	}
}

int sprdwl_fc_find_color_per_mode(struct sprdwl_tx_msg *tx_msg,
				enum sprdwl_mode mode,
				u8 *index)
{
	u8 i = 0, found = 0;
	struct sprdwl_priv *priv = tx_msg->intf->priv;

	for (i = 0; i < MAX_COLOR_BIT; i++) {
		if (tx_msg->flow_ctrl[i].mode == mode) {
			found = 1;
			wl_debug("%s, %d, mode:%d found, index:%d\n",
				 __func__, __LINE__,
				 mode, i);
			break;
		}
	}
	if (found == 0) {
		/*a new mode. sould assign new color to this mode*/
		for (i = 0; i < MAX_COLOR_BIT; i++) {
			if ((tx_msg->flow_ctrl[i].mode != SPRDWL_MODE_NONE) &&
				(priv->fw_stat[tx_msg->flow_ctrl[i].mode]
				== SPRDWL_INTF_CLOSE))
				tx_msg->flow_ctrl[i].mode = SPRDWL_MODE_NONE;
		}
		for (i = 0; i < MAX_COLOR_BIT; i++) {
			if (tx_msg->flow_ctrl[i].mode == SPRDWL_MODE_NONE) {
				found = 1;
				tx_msg->flow_ctrl[i].mode = mode;
				tx_msg->flow_ctrl[i].color_bit = i;
				wl_info("%s, %d, new mode:%d, assign color:%d\n",
					__func__, __LINE__,
					mode, i);
				break;
			}
		}
	}
	if (found == 1)
		*index = i;
	return found;
}

int sprdwl_fc_get_shared_num(struct sprdwl_tx_msg *tx_msg, u8 num)
{
	u8 i;
	int shared_flow_num = 0;
	unsigned int color_flow;

	for (i = 0; i < MAX_COLOR_BIT; i++) {
		color_flow = atomic_read(&tx_msg->flow_ctrl[i].flow);
		if ((tx_msg->flow_ctrl[i].mode == SPRDWL_MODE_NONE) &&
			(0 != color_flow)) {
			if ((num - shared_flow_num) <= color_flow) {
				/*one shared color is enough?*/
				tx_msg->color_num[i] = num - shared_flow_num;
				shared_flow_num += num - shared_flow_num;
				break;
			} else {
				/*need one more shared color*/
				tx_msg->color_num[i] = color_flow;
				shared_flow_num += color_flow;
			}
		}
	}
	return shared_flow_num;
}

int sprdwl_fc_get_send_num(struct sprdwl_tx_msg *tx_msg,
			     enum sprdwl_mode mode,
			     int data_num)
{
	int excusive_flow_num = 0, shared_flow_num = 0;
	int send_num = 0, free_num = 0;
	u8 i = 0;
	struct sprdwl_priv *priv = tx_msg->intf->priv;
	static unsigned long caller_jiffies;
	unsigned int tx_buf_max;

	if (data_num <= 0)
		return 0;
	/*send all data in buff with PCIe interface*/
	if (priv->hw_type == SPRDWL_HW_SC2355_PCIE ||
		priv->hw_type == SPRDWL_HW_SIPC) {
		if (priv->hw_type == SPRDWL_HW_SC2355_PCIE)
			tx_buf_max = g_max_fw_tx_dscr >
					  sprdwl_get_tx_buf_num() ?
					  sprdwl_get_tx_buf_num() :
					  g_max_fw_tx_dscr;
		else if (priv->hw_type == SPRDWL_HW_SIPC) {
#ifdef SIPC_SUPPORT
			tx_buf_max = g_max_fw_tx_dscr >
					  sipc_get_tx_buf_num(tx_msg->intf) ?
					  sipc_get_tx_buf_num(tx_msg->intf) :
					  g_max_fw_tx_dscr;
#endif
			}

		free_num = atomic_read(&tx_msg->xmit_msg_list.free_num);
		if (printk_timed_ratelimit(&caller_jiffies, 1000)) {
			wl_info("%s, free_num=%d, data_num=%d\n", __func__,
				free_num, data_num);
			if (list_empty(&tx_msg->xmit_msg_list.to_free_list))
				wl_info("%s: to free list empty\n", __func__);
		}

		if (priv->hw_type == SPRDWL_HW_SIPC) {
			if (data_num >= tx_buf_max) {
				wl_err_ratelimited("%s, %d, tx_buf_max=%d, data_num=%d\n", __func__,
						   __LINE__, tx_buf_max, data_num);
				return tx_buf_max;
			} else {
				return data_num;
			}
		} else {
			if ((free_num + data_num) >= tx_buf_max) {
				wl_err_ratelimited("%s, %d, free_num=%d, data_num=%d\n", __func__,
						   __LINE__, free_num, data_num);
				return (tx_buf_max - free_num);
			} else {
				return data_num;
			}
		}
	}
	/*TODO. CP2 can not hanle more than 32 packets at one time*/
	if (data_num > 64)
		data_num = 64;
	if (priv->credit_capa == TX_NO_CREDIT)
		return data_num;

	memset(tx_msg->color_num, 0x00, MAX_COLOR_BIT);

	if (sprdwl_fc_find_color_per_mode(tx_msg, mode, &i) == 1) {
		excusive_flow_num  = atomic_read(&tx_msg->flow_ctrl[i].flow);
		if (excusive_flow_num >= data_num) {
			/*excusive flow is enough, do not need shared flow*/
			send_num = tx_msg->color_num[i] = data_num;
		} else {
			/*excusive flow not enough, need shared flow
			 *total give num =  excusive + shared
			 *(may be more than one color)*/
			u8 num_need = data_num - excusive_flow_num;

			shared_flow_num =
				sprdwl_fc_get_shared_num(tx_msg, num_need);
			tx_msg->color_num[i] = excusive_flow_num;
			send_num = excusive_flow_num + shared_flow_num;
		}

		if (send_num <= 0) {
			wl_err("%s, %d, mode:%d, e_num:%d, s_num:%d, d_num:%d\n",
			       __func__, __LINE__,
			       (u8)mode, excusive_flow_num,
			       shared_flow_num, data_num);
			return -ENOMEM;
		}
		wl_info("%s, %d, mode:%d, e_num:%d, s_num:%d, d_num:%d\n"
			"color_num = {%d, %d, %d, %d}\n",
			__func__, __LINE__, mode, excusive_flow_num,
			shared_flow_num, data_num,
			tx_msg->color_num[0], tx_msg->color_num[1],
			tx_msg->color_num[2], tx_msg->color_num[3]);
	} else {
		wl_err("%s, %d, wrong mode:%d?\n",
		       __func__, __LINE__,
		       (u8)mode);
		for (i = 0; i < MAX_COLOR_BIT; i++)
			wl_err("color[%d] assigned mode%d\n",
			       i, (u8)tx_msg->flow_ctrl[i].mode);
		return -ENOMEM;
	}

	return send_num;
}

/*to see there is shared flow or not*/
int sprdwl_fc_test_shared_num(struct sprdwl_tx_msg *tx_msg)
{
	u8 i;
	int shared_flow_num = 0;
	unsigned int color_flow;

	for (i = 0; i < MAX_COLOR_BIT; i++) {
		color_flow = atomic_read(&tx_msg->flow_ctrl[i].flow);
		if ((tx_msg->flow_ctrl[i].mode == SPRDWL_MODE_NONE) &&
			(0 != color_flow)) {
			shared_flow_num += color_flow;
		}
	}
	return shared_flow_num;
}
/*to check flow number, no flow number, no send*/
int sprdwl_fc_test_send_num(struct sprdwl_tx_msg *tx_msg,
			     enum sprdwl_mode mode,
			     int data_num)
{
	int excusive_flow_num = 0, shared_flow_num = 0;
	int send_num = 0, free_num = 0;
	u8 i = 0;
	struct sprdwl_priv *priv = tx_msg->intf->priv;
	static unsigned long caller_jiffies;
	unsigned int tx_buf_max;

	if (data_num <= 0 || mode == SPRDWL_MODE_NONE)
		return 0;

	/*send all data in buff with PCIe interface, TODO*/
	if (priv->hw_type == SPRDWL_HW_SC2355_PCIE ||
		priv->hw_type == SPRDWL_HW_SIPC) {
		if (priv->hw_type == SPRDWL_HW_SC2355_PCIE)
			tx_buf_max = g_max_fw_tx_dscr >
				sprdwl_get_tx_buf_num() ?
				sprdwl_get_tx_buf_num() :
				g_max_fw_tx_dscr;
		else if (priv->hw_type == SPRDWL_HW_SIPC) {
#ifdef SIPC_SUPPORT
			tx_buf_max = g_max_fw_tx_dscr >
				sipc_get_tx_buf_num(tx_msg->intf) ?
				sipc_get_tx_buf_num(tx_msg->intf) :
				g_max_fw_tx_dscr;
#endif
		}

		free_num = atomic_read(&tx_msg->xmit_msg_list.free_num);
		if (printk_timed_ratelimit(&caller_jiffies, 1000)) {
			wl_info("%s, free_num=%d, data_num=%d\n", __func__,
				free_num, data_num);
			if (list_empty(&tx_msg->xmit_msg_list.to_free_list)) {
				wl_info("%s: to free list empty\n", __func__);
			}
		}

		if (priv->hw_type == SPRDWL_HW_SIPC) {
			if (data_num >= tx_buf_max) {
				wl_err_ratelimited("%s, %d, tx_buf_max=%d, data_num=%d\n",
						   __func__, __LINE__, tx_buf_max, data_num);
				return tx_buf_max;
			} else {
				return data_num;
			}
		} else {
			if ((free_num + data_num) >= tx_buf_max) {
				wl_err_ratelimited("%s, %d, free_num=%d, data_num=%d\n",
						   __func__, __LINE__, free_num, data_num);
				return (tx_buf_max - free_num);
			} else {
				return data_num;
			}
		}
	}

	if (data_num > 64)
		data_num = 64;
	if (priv->credit_capa == TX_NO_CREDIT)
		return data_num;

	if (sprdwl_fc_find_color_per_mode(tx_msg, mode, &i) == 1) {
		excusive_flow_num = atomic_read(&tx_msg->flow_ctrl[i].flow);
		shared_flow_num =
				sprdwl_fc_test_shared_num(tx_msg);
		send_num = excusive_flow_num + shared_flow_num;

		if (send_num <= 0) {
			wl_debug("%s, %d, err, mode:%d, e_num:%d, s_num:%d, d_num=%d\n",
				 __func__, __LINE__, (u8)mode,
				 excusive_flow_num, shared_flow_num, data_num);
			return -ENOMEM;
		}
		wl_debug("%s, %d, e_num=%d, s_num=%d, d_num=%d\n",
			 __func__, __LINE__, excusive_flow_num,
			 shared_flow_num, data_num);
	} else {
		wl_err("%s, %d, wrong mode:%d?\n",
		       __func__, __LINE__,
		       (u8)mode);
		for (i = 0; i < MAX_COLOR_BIT; i++)
			printk_ratelimited("color[%d] assigned mode%d\n",
			       i, (u8)tx_msg->flow_ctrl[i].mode);
		return -ENOMEM;
	}

	return min(send_num, data_num);
}

u8 sprdwl_fc_set_clor_bit(struct sprdwl_tx_msg *tx_msg, int num)
{
	u8 i = 0;
	int count_num = 0;
	struct sprdwl_priv *priv = tx_msg->intf->priv;

	if (priv->credit_capa == TX_NO_CREDIT)
		return 0;

	for (i = 0; i < MAX_COLOR_BIT; i++) {
		count_num += tx_msg->color_num[i];
		if (num <= count_num)
			break;
	}
	wl_debug("%s, %d, color bit =%d\n", __func__, __LINE__, i);
	return i;
}

void sprdwl_handle_tx_return(struct sprdwl_tx_msg *tx_msg,
				  struct sprdwl_msg_list *list,
				  int send_num, int ret)
{
//	u8 i;

	if (ret == -2) {
		//printk_ratelimited(
		//"%s sprdwl_intf_tx_list err:%d\n",
		//       __func__, ret);
		atomic_sub(send_num, &list->ref);
		wl_info("%s,%d,debug: %d\n", __func__, __LINE__, atomic_read(&list->ref));
		usleep_range(100, 200);
		return;
	} else if (ret < 0) {
		usleep_range(100, 200);
		return;
	} else {
		//atomic_sub(send_num, &list->ref);
		wl_info("%s,%d,debug: %d\n", __func__, __LINE__, atomic_read(&list->ref));
	}
#if 0
	for (i = 0; i < MAX_COLOR_BIT; i++) {
		if (tx_msg->color_num[i] == 0)
			continue;
		atomic_sub(tx_msg->color_num[i],
			&tx_msg->flow_ctrl[i].flow);
		wl_info("%s, _fc_, color bit:%d, flow num-%d=%d, seq_num=%d\n",
			 __func__, i, tx_msg->color_num[i],
			 atomic_read(&tx_msg->flow_ctrl[i].flow),
			 tx_msg->seq_num);
	}
	tx_msg->ring_ap += send_num;
	sprdwl_wake_net_ifneed(tx_msg->intf, list, tx_msg->mode);
#endif
}

int handle_tx_timeout(struct sprdwl_tx_msg *tx_msg,
		      struct sprdwl_msg_list *msg_list,
		      struct peer_list *p_list, int ac_index)
{
	u8 mode;
	char *pinfo;
	spinlock_t *lock;
	int cnt, i, del_list_num;
	struct list_head *tx_list;
	struct sprdwl_msg_buf *pos_buf, *temp_buf, *tailbuf;

	if (SPRDWL_AC_MAX == ac_index)
		return 0;

	tx_list = &p_list->head_list;
	lock = &p_list->p_lock;
	spin_lock_bh(lock);
	if (list_empty(tx_list)) {
		spin_unlock_bh(lock);
		return 0;
	}
	tailbuf = list_first_entry(tx_list, struct sprdwl_msg_buf, list);
	if (time_after(jiffies, tailbuf->timeout)) {
		mode = tailbuf->mode;
		i = 0;
		del_list_num = atomic_read(&p_list->l_num) * atomic_read(&p_list->l_num) /
						msg_list->maxnum;
		if (del_list_num >= atomic_read(&p_list->l_num))
			del_list_num = atomic_read(&p_list->l_num);
		wl_info("tx timeout drop num:%d, l_num:%d, maxnum:%d",
			del_list_num, atomic_read(&p_list->l_num), msg_list->maxnum);
		list_for_each_entry_safe(pos_buf,
					 temp_buf, tx_list, list) {
			if (i >= del_list_num)
				break;
			wl_debug("%s:%d buf->timeout\n",
			       __func__, __LINE__);
			if (pos_buf->mode <= SPRDWL_MODE_AP) {
				pinfo = "STA/AP mode";
				cnt = tx_msg->drop_data1_cnt++;
			} else {
				pinfo = "P2P mode";
				cnt = tx_msg->drop_data2_cnt++;
			}
			wl_warn("tx drop %s, dropcnt:%u\n",
				pinfo, cnt);
			if (pos_buf->skb)
				dev_kfree_skb(pos_buf->skb);
			if (pos_buf->node)
				sprdwl_free_tx_buf(pos_buf->node);
			list_del(&pos_buf->list);
			sprdwl_free_msg_buf(pos_buf, pos_buf->msglist);
			atomic_dec(&tx_msg->tx_list[mode]->mode_list_num);
#if defined(MORE_DEBUG)
			tx_msg->intf->stats.tx_dropped++;
#endif
			i++;
		}
		atomic_sub(del_list_num, &p_list->l_num);
		spin_unlock_bh(lock);
		return -ENOMEM;
	}
	spin_unlock_bh(lock);

	return 0;
}

static int sprdwl_handle_to_send_list(struct sprdwl_intf *intf,
				      enum sprdwl_mode mode)
{
	struct sprdwl_tx_msg *tx_msg = (struct sprdwl_tx_msg *)intf->sprdwl_tx;
	struct list_head *to_send_list, tx_list_head;
	int tosendnum = 0, credit = 0, ret = 0;
	struct sprdwl_msg_list *list = &tx_msg->tx_list_qos_pool;
	INIT_LIST_HEAD(&tx_list_head);

	if (!list_empty(&tx_msg->xmit_msg_list.to_send_list)) {
		to_send_list = &tx_msg->xmit_msg_list.to_send_list;
		tosendnum = get_list_num(to_send_list);
		credit = sprdwl_fc_get_send_num(tx_msg, mode, tosendnum);
		if (credit < tosendnum)
			wl_debug("%s, %d,error! credit:%d,tosendnum:%d\n",
				 __func__, __LINE__,
				 credit, tosendnum);
		if (credit <= 0) {
#if 0
			wl_debug("%s, %d\n", __func__, __LINE__);
			if (intf->priv->hw_type == SPRDWL_HW_SC2355_PCIE) {
				sprdwl_flush_tosendlist(tx_msg);
				sprdwl_net_flowcontrl(intf->priv, SPRDWL_MODE_NONE, false);
				tx_msg->net_stoped = 1;
			}
#endif
			return -ENOMEM;
		}

		ret = sprdwl_intf_tx_list(intf, to_send_list, &tx_list_head,
					  credit, SPRDWL_AC_MAX);
		sprdwl_handle_tx_return(tx_msg, list, credit, ret);
		if (0 != ret) {
			wl_debug("%s, %d: tx return err!\n",
			       __func__, __LINE__);
			tx_up(tx_msg);
			return -ENOMEM;
		}
	}
	return 0;
}

unsigned int vo_ratio = 87;
unsigned int vi_ratio = 90;
unsigned int be_ratio = 81;
static int sprdwl_tx_eachmode_data(struct sprdwl_intf *intf,
				   enum sprdwl_mode mode)
{
	struct sprdwl_tx_msg *tx_msg = (struct sprdwl_tx_msg *)intf->sprdwl_tx;
	int ret, i, j;
	struct list_head tx_list_head;
	struct qos_list *q_list;
	struct peer_list *p_list;
	struct tx_t *tx_list = tx_msg->tx_list[mode];
	int send_num = 0, total = 0, min_num = 0, round_num = 0;
	int q_list_num[SPRDWL_AC_MAX] = {0, 0, 0, 0};
	int p_list_num[SPRDWL_AC_MAX][MAX_LUT_NUM] = {{0}, {0}, {0}, {0} };

	INIT_LIST_HEAD(&tx_list_head);
	/* first, go through all list, handle timeout msg
	 * and count each TID's tx_num and total tx_num
	 */
	for (i = 0; i < SPRDWL_AC_MAX; i++) {
		for (j = 0; j < MAX_LUT_NUM; j++) {
			p_list = &tx_list->q_list[i].p_list[j];
			if (atomic_read(&p_list->l_num) > 0) {
				p_list_num[i][j] = atomic_read(&p_list->l_num);
				/*wl_info("TID=%d,PEER=%d,l_num=%d\n",i,j,p_list_num[i][j]);*/
				q_list_num[i] += p_list_num[i][j];
			}
		}
		total += q_list_num[i];
		if (q_list_num[i] != 0)
			wl_info("TID%s%s%s%snum=%d, total=%d\n",
			       (i == SPRDWL_AC_VO) ? "VO" : "",
			       (i == SPRDWL_AC_VI) ? "VI" : "",
			       (i == SPRDWL_AC_BE) ? "BE" : "",
			       (i == SPRDWL_AC_BK) ? "BK" : "",
			       q_list_num[i], total);
	}
	send_num = sprdwl_fc_test_send_num(tx_msg, mode, total);
	if (total != 0 && send_num <= 0) {
		wl_err("%s, %d: _fc_ no credit!\n",
		       __func__, __LINE__);
		return -ENOMEM;
	}

	/* merge qos queues to to_send_list
	 * to best use of HIF interrupt
	 */
	/* case1: send_num >= total
	 * remained _fc_ num is more than remained qos data,
	 * just add all remained qos list to xmit list
	 * and send all xmit to_send_list
	 */
	if (send_num >= total) {
		for (i = 0; i < SPRDWL_AC_MAX; i++) {
			q_list = &tx_list->q_list[i];
			if (q_list_num[i] <= 0)
				continue;
			for (j = 0; j < MAX_LUT_NUM; j++) {
				p_list = &q_list->p_list[j];
				if (p_list_num[i][j] ==  0)
					continue;
				add_xmit_list_tail(tx_msg, p_list, p_list_num[i][j]);
				atomic_sub(p_list_num[i][j], &p_list->l_num);
				atomic_sub(p_list_num[i][j], &tx_list->mode_list_num);
				wl_debug("%s, %d, mode=%d, TID=%d, lut=%d, %d add to xmit_list, then l_num=%d, mode_list_num=%d\n",
					 __func__, __LINE__, mode, i, j,
					 p_list_num[i][j],
					 atomic_read(&p_list->l_num),
					 atomic_read(&tx_msg->tx_list[mode]->mode_list_num));
			}
		}
		ret = sprdwl_handle_to_send_list(intf, mode);
		return ret;
	}

	/* case2: send_num < total
	 * vo get 87%,vi get 90%,be get remain 81%
	 */
	for (i = 0; i < SPRDWL_AC_MAX; i++) {
		int fp_num = 0;/*assigned _fc_ num to qoslist*/

		q_list = &tx_list->q_list[i];
		if (q_list_num[i] <= 0)
			continue;
		if (send_num <= 0)
			break;

		if ((i == SPRDWL_AC_VO) && (total > q_list_num[i])) {
			round_num = send_num * vo_ratio / 100;
			fp_num = min(round_num, q_list_num[i]);
		} else if ((i == SPRDWL_AC_VI) && (total > q_list_num[i])) {
			round_num = send_num * vi_ratio / 100;
			fp_num = min(round_num, q_list_num[i]);
		} else if ((i == SPRDWL_AC_BE) && (total > q_list_num[i])) {
			round_num = send_num * be_ratio / 100;
			fp_num = min(round_num, q_list_num[i]);
		} else {
			fp_num = send_num * q_list_num[i] / total;
		}
		if (((total - q_list_num[i]) < (send_num - fp_num)) &&
			((total - q_list_num[i]) > 0))
			fp_num += (send_num - fp_num - (total - q_list_num[i]));

		total -= q_list_num[i];

		wl_info("TID%s%s%s%s, credit=%d, fp_num=%d, remain=%d\n",
			(i == SPRDWL_AC_VO) ? "VO" : "",
			(i == SPRDWL_AC_VI) ? "VI" : "",
			(i == SPRDWL_AC_BE) ? "BE" : "",
			(i == SPRDWL_AC_BK) ? "BK" : "",
			send_num, fp_num, total);

		send_num -= fp_num;
		for (j = 0; j < MAX_LUT_NUM; j++) {
			if (p_list_num[i][j] == 0)
				continue;
			round_num = p_list_num[i][j] * fp_num / q_list_num[i];
			if (fp_num > 0 && round_num == 0)
				round_num = 1;/*round_num = 0.1~0.9*/
			min_num = min(round_num, fp_num);
			wl_debug("TID=%d,PEER=%d,%d,%d,%d,%d,%d\n",
				 i, j, p_list_num[i][j], q_list_num[i], round_num, fp_num, min_num);
			if (min_num <= 0)
				break;
			q_list_num[i] -= p_list_num[i][j];
			fp_num -= min_num;
			add_xmit_list_tail(tx_msg,
					   &q_list->p_list[j],
					   min_num);
			atomic_sub(min_num, &q_list->p_list[j].l_num);
			atomic_sub(min_num, &tx_list->mode_list_num);
			wl_debug("%s, %d, mode=%d, TID=%d, lut=%d, %d add to xmit_list, then l_num=%d, mode_list_num=%d\n",
				 __func__, __LINE__, mode, i, j,
				 min_num,
				 atomic_read(&p_list->l_num),
				 atomic_read(&tx_msg->tx_list[mode]->mode_list_num));
			if (fp_num <= 0)
				break;
		}
	}
	ret = sprdwl_handle_to_send_list(intf, mode);
	return ret;
}

static void sprdwl_flush_all_txlist(struct sprdwl_tx_msg *sprdwl_tx_dev)
{
	sprdwl_sdio_flush_txlist(&sprdwl_tx_dev->tx_list_cmd);
	sprdwl_flush_data_txlist(sprdwl_tx_dev);
}

int sprdwl_sdio_process_credit(void *pdev, void *data)
{
	int ret = 0, i;
	unsigned char *flow;
	struct sprdwl_common_hdr *common;
	struct sprdwl_intf *intf;
	struct sprdwl_tx_msg *tx_msg;
	ktime_t kt;
	int in_count = 0;

	intf = (struct sprdwl_intf *)pdev;
	tx_msg = (struct sprdwl_tx_msg *)intf->sprdwl_tx;
	common = (struct sprdwl_common_hdr *)data;

	if (common->type == SPRDWL_TYPE_DATA_SPECIAL) {
		int offset = (size_t)&((struct rx_msdu_desc *)0)->rsvd5;

		flow = data + offset;
		goto out;
	}

	if (common->type == SPRDWL_TYPE_EVENT) {
		struct sprdwl_cmd_hdr *cmd;

		cmd = (struct sprdwl_cmd_hdr *)data;
		if (cmd->cmd_id == WIFI_EVENT_SDIO_FLOWCON) {
			flow = cmd->paydata;
			ret = -1;
			goto out;
		}
	}
	return 0;

out:
	if (flow[0])
		atomic_add(flow[0], &tx_msg->flow_ctrl[0].flow);
	if (flow[1])
		atomic_add(flow[1], &tx_msg->flow_ctrl[1].flow);
	if (flow[2])
		atomic_add(flow[2], &tx_msg->flow_ctrl[2].flow);
	if (flow[3])
		atomic_add(flow[3], &tx_msg->flow_ctrl[3].flow);
	if (flow[0] || flow[1] || flow[2] || flow[3]) {
		in_count = flow[0] + flow[1] + flow[2] + flow[3];
		tx_msg->ring_cp += in_count;
		if (intf->fw_awake == 1)
			tx_up(tx_msg);
	}
	/* Firmware want to reset credit, will send us
	 * a credit event with all 4 parameters set to zero
	 */
	if (in_count == 0) {
		/*in_count==0: reset credit event or a data without credit
		 *ret == -1:reset credit event
		 *for a data without credit:just return,donot print log
		*/
		if (ret == -1) {
			wl_info("%s, %d, _fc_ reset credit\n", __func__, __LINE__);
			for (i = 0; i < MAX_COLOR_BIT; i++) {
				if (tx_msg->ring_cp != 0)
					tx_msg->ring_cp -= atomic_read(&tx_msg->flow_ctrl[i].flow);
				atomic_set(&tx_msg->flow_ctrl[i].flow, 0);
				tx_msg->color_num[i] = 0;
			}
		}
		goto exit;
	}
	kt = ktime_get();
	/*1.(tx_msg->kt.tv64 == 0) means 1st event
	2.add (in_count == 0) to avoid
	division by expression in_count which
	may be zero has undefined behavior*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	if ((tx_msg->kt == 0) || (in_count == 0)) {
		tx_msg->kt = kt;
	} else {
		wl_info("%s, %d, %s, %dadded, %lld usec per flow\n",
		__func__, __LINE__,
		(ret == -1) ? "event" : "data",
		in_count,
		div_u64(div_u64(kt - tx_msg->kt, NSEC_PER_USEC), in_count));

		debug_record_add(TX_CREDIT_ADD, in_count);
		debug_record_add(TX_CREDIT_PER_ADD,
			div_u64(div_u64(kt - tx_msg->kt,
				 NSEC_PER_USEC), in_count));
		debug_record_add(TX_CREDIT_RECORD, jiffies_to_usecs(jiffies));
		debug_record_add(TX_CREDIT_TIME_DIFF,
			div_u64(kt - tx_msg->kt, NSEC_PER_USEC));
	}
#else
	if ((tx_msg->kt.tv64 == 0) || (in_count == 0)) {
		tx_msg->kt = kt;
	} else {
		wl_info("%s, %d, %s, %dadded, %lld usec per flow\n",
		__func__, __LINE__,
		(ret == -1) ? "event" : "data",
		in_count,
		div_u64(div_u64(kt.tv64 - tx_msg->kt.tv64, NSEC_PER_USEC), in_count));

		debug_record_add(TX_CREDIT_ADD, in_count);
		debug_record_add(TX_CREDIT_PER_ADD,
			div_u64(div_u64(kt.tv64 - tx_msg->kt.tv64,
				NSEC_PER_USEC), in_count));
		debug_record_add(TX_CREDIT_RECORD, jiffies_to_usecs(jiffies));
		debug_record_add(TX_CREDIT_TIME_DIFF,
			div_u64(kt.tv64 - tx_msg->kt.tv64, NSEC_PER_USEC));
	}
#endif
	tx_msg->kt = ktime_get();

	wl_info("%s, _fc_,R+%d=%d,G+%d=%d,B+%d=%d,W+%d=%d,cp=%lu,ap=%lu\n",
		__func__,
		flow[0], atomic_read(&tx_msg->flow_ctrl[0].flow),
		flow[1], atomic_read(&tx_msg->flow_ctrl[1].flow),
		flow[2], atomic_read(&tx_msg->flow_ctrl[2].flow),
		flow[3], atomic_read(&tx_msg->flow_ctrl[3].flow),
		tx_msg->ring_cp, tx_msg->ring_ap);
exit:
	return ret;
}

void prepare_addba(struct sprdwl_intf *intf, unsigned char lut_index,
		    struct sprdwl_peer_entry *peer_entry, unsigned char tid)
{
	if (intf->tx_num[lut_index] > 9 &&
#ifdef WMMAC_WFA_CERTIFICATION
		intf->wmm_special_flag == 0 &&
#endif
		peer_entry &&
		peer_entry->ht_enable &&
		peer_entry->vowifi_enabled != 1 &&
		!test_bit(tid, &peer_entry->ba_tx_done_map)) {
		struct timespec time;
		struct sprdwl_vif *vif;

		vif = ctx_id_to_vif(intf->priv, peer_entry->ctx_id);
		if (!vif) {
			wl_err("can not get vif base peer_entry ctx id\n");
			return;
		}

		if (vif->mode == SPRDWL_MODE_STATION ||
			vif->mode == SPRDWL_MODE_STATION_SECOND ||
			vif->mode == SPRDWL_MODE_P2P_CLIENT) {
			if (!peer_entry->ip_acquired)
				return;
		}

		getnstimeofday(&time);
		/*need to delay 3s if priv addba failed*/
		if (((timespec_to_ns(&time) - timespec_to_ns(&peer_entry->time[tid]))/1000000) > 3000 ||
			peer_entry->time[tid].tv_nsec == 0) {
			wl_info("%s, %d, tx_addba, tid=%d\n",
				__func__, __LINE__, tid);
			getnstimeofday(&peer_entry->time[tid]);
			test_and_set_bit(tid, &peer_entry->ba_tx_done_map);
			sprdwl_tx_addba(intf, peer_entry, tid);
		}
	}
}

int sprdwl_tx_msg_func(void *pdev, struct sprdwl_msg_buf *msg)
{
	u16 len;
	unsigned char *info;
	struct sprdwl_intf *intf = (struct sprdwl_intf *)pdev;
	struct sprdwl_tx_msg *tx_msg = (struct sprdwl_tx_msg *)intf->sprdwl_tx;
	unsigned int qos_index = 0;
	struct sprdwl_peer_entry *peer_entry = NULL;
	unsigned char tid = 0, tos = 0;
	struct tx_msdu_dscr *dscr = NULL;

	if (msg->msglist == &tx_msg->tx_list_cmd) {
		len = SPRDWL_MAX_CMD_TXLEN;
		info = "cmd";
		msg->timeout = jiffies + tx_msg->cmd_timeout;
	} else {
		len = SPRDWL_MAX_DATA_TXLEN;
		info = "data";
		msg->timeout = jiffies + tx_msg->data_timeout;
	}

	if (msg->len > len) {
		wl_err("%s err:%s too long:%d > %d,drop it\n",
				   __func__, info, msg->len, len);
#if defined(MORE_DEBUG)
		intf->stats.tx_dropped++;
#endif
		INIT_LIST_HEAD(&msg->list);
		return -EPERM;
	}

	if (msg->msglist == &tx_msg->tx_list_qos_pool) {
		struct peer_list *data_list;

		if (intf->priv->hw_type == SPRDWL_HW_SC2355_PCIE ||
			intf->priv->hw_type == SPRDWL_HW_SIPC) {
			dscr = (struct tx_msdu_dscr *)(msg->tran_data + MSDU_DSCR_RSVD);
			qos_index = get_tid_qosindex(msg->skb, MSDU_DSCR_RSVD + DSCR_LEN, &tid, &tos);
		} else {
			dscr = (struct tx_msdu_dscr *)msg->tran_data;
			qos_index = get_tid_qosindex(msg->skb, DSCR_LEN, &tid, &tos);
		}

#ifdef WMMAC_WFA_CERTIFICATION
		qos_index = change_priority_if(intf->priv, &tid, &tos, msg->len);
		wl_debug("%s qos_index: %d tid: %d, tos:%d\n", __func__, qos_index, tid, tos);
		if (SPRDWL_AC_MAX == qos_index) {
			INIT_LIST_HEAD(&msg->list);
			return -EPERM;
		}
		tx_msg->wmm_tx_count[qos_index]++;
		/* wl_info("%s qos_index: %d wmm_tx_count: %lu\n", __func__, qos_index, tx_msg->wmm_tx_count[qos_index]); */

		if (intf->priv->hw_type == SPRDWL_HW_SC2355_PCIE &&
		    intf->wmm_special_flag == 1 &&
		    check_wmm_tx_flows(intf, msg, qos_index)) {
			tx_msg->wmm_tx_droped[qos_index]++;
			/* wl_info("%s qos_index: %d wmm_tx_droped: %lu\n",
				   __func__, qos_index, tx_msg->wmm_tx_droped[qos_index]); */
			INIT_LIST_HEAD(&msg->list);
			return -EPERM;
		}
#endif
		/*send group in BK to avoid FW hang*/
		if ((msg->mode == SPRDWL_MODE_AP ||
			msg->mode == SPRDWL_MODE_P2P_GO) &&
			(dscr->sta_lut_index < 6)) {
			qos_index = SPRDWL_AC_BK;
			tid = prio_1;
			wl_info("%s, %d, SOFTAP/GO group go as BK\n", __func__, __LINE__);
		} else {
			intf->tx_num[dscr->sta_lut_index]++;
		}
		dscr->buffer_info.msdu_tid = tid;
		peer_entry = &intf->peer_entry[dscr->sta_lut_index];
		prepare_addba(intf, dscr->sta_lut_index, peer_entry, tid);
/*TODO. temp for MARLIN2 test*/
#if 0
		qos_index = qos_match_q(&tx_msg->tx_list_data,
					msg->skb, 10);/*temp for test*/
#endif
		data_list =
			&tx_msg->tx_list[msg->mode]->q_list[qos_index].p_list[dscr->sta_lut_index];
		tx_msg->tx_list[msg->mode]->lut_id = dscr->sta_lut_index;
		/*if ((qos_index == SPRDWL_AC_VO) ||
			(qos_index == SPRDWL_AC_VI) ||
			(qos_index == SPRDWL_AC_BE && data_list->l_num <= BE_TOTAL_QUOTA) ||
			(qos_index == SPRDWL_AC_BK && data_list->l_num <= BK_TOTAL_QUOTA)
			) {*/
			msg->data_list = data_list;

		if (intf->priv->hw_type == SPRDWL_HW_SC2355_PCIE) {
			msg->pcie_addr = mm_virt_to_phys(&intf->pdev->dev,
							 msg->tran_data, msg->len,
							 DMA_TO_DEVICE);
			SAVE_ADDR(msg->tran_data, msg, sizeof(struct sprdwl_msg_buf *));
		} else if (intf->priv->hw_type == SPRDWL_HW_SIPC) {
#ifdef SIPC_SUPPORT
			SAVE_ADDR(msg->tran_data, msg, sizeof(struct sprdwl_msg_buf *));
#endif
		}
		sprdwl_queue_data_msg_buf(msg);
		atomic_inc(&tx_msg->tx_list[msg->mode]->mode_list_num);
		wl_err_ratelimited("lut=%d,qos_index=%d,tid=%d,lnum=%d,tx data num:%d\n",
					   tx_msg->tx_list[msg->mode]->lut_id,
					   qos_index, tid,
					   atomic_read(&msg->data_list->l_num),
					   atomic_read(&tx_msg->tx_list[msg->mode]->mode_list_num));
	}

	if (msg->msg_type != SPRDWL_TYPE_DATA)
		sprdwl_queue_msg_buf(msg, msg->msglist);


	if (msg->msg_type == SPRDWL_TYPE_CMD)
		tx_up(tx_msg);
	if (msg->msg_type == SPRDWL_TYPE_DATA &&
		((intf->fw_awake == 0 &&
		intf->fw_power_down == 1) ||
		intf->fw_awake == 1))
		tx_up(tx_msg);

	return 0;
}

/* seam for tx_thread */
void tx_down(struct sprdwl_tx_msg *tx_msg)
{
	wait_for_completion(&tx_msg->tx_completed);
}

void tx_up(struct sprdwl_tx_msg *tx_msg)
{
	complete(&tx_msg->tx_completed);
}

static int sprdwl_tx_work_queue(void *data)
{
	unsigned long need_polling;
	unsigned int polling_times = 0;
	struct sprdwl_intf *intf;
	struct sprdwl_tx_msg *tx_msg;
	enum sprdwl_mode mode = SPRDWL_MODE_NONE;
	int send_num = 0;
	struct sprdwl_priv *priv;

	tx_msg = (struct sprdwl_tx_msg *)data;
	intf = tx_msg->intf;
	priv = intf->priv;
	set_user_nice(current, -20);

	while (1) {
		if (intf->exit) {
			if (kthread_should_stop())
				return 0;
			usleep_range(50, 100);
			continue;
		} else
			tx_down(tx_msg);

		need_polling = 0;
		polling_times = 0;

		/*During hang recovery, send data is not allowed.
		* but we still need to send cmd to cp2
		*/
		if (tx_msg->hang_recovery_status != HANG_RECOVERY_END) {
			printk_ratelimited("sc2355, %s, hang happened\n", __func__);
			if (sprdwl_msg_tx_pended(&tx_msg->tx_list_cmd))
				sprdwl_tx_cmd(intf, &tx_msg->tx_list_cmd);
			usleep_range(50, 100);
			continue;
		}

		if (tx_msg->thermal_status == THERMAL_WIFI_DOWN) {
			printk_ratelimited("sc2355, %s, THERMAL_WIFI_DOWN\n", __func__);
			if (sprdwl_msg_tx_pended(&tx_msg->tx_list_cmd))
				sprdwl_tx_cmd(intf, &tx_msg->tx_list_cmd);
			usleep_range(50, 100);
			continue;
		}
		if (tx_msg->thermal_status == THERMAL_TX_STOP) {
			printk_ratelimited("sc2355, %s, THERMAL_TX_STOP\n", __func__);
			if (sprdwl_msg_tx_pended(&tx_msg->tx_list_cmd))
				sprdwl_tx_cmd(intf, &tx_msg->tx_list_cmd);
			usleep_range(50, 100);
			continue;
		}

		if (sprdwl_msg_tx_pended(&tx_msg->tx_list_cmd))
			sprdwl_tx_cmd(intf, &tx_msg->tx_list_cmd);

		if (intf->cp_asserted == 1) {
			wl_err("%s, cp2 assert, flush data\n", __func__);
			for (mode = SPRDWL_MODE_STATION; mode < SPRDWL_MODE_MAX; mode++) {
				if (atomic_read(&tx_msg->tx_list[mode]->mode_list_num) == 0)
					continue;
				sprdwl_flush_mode_txlist(tx_msg, mode);
			}
				sprdwl_flush_tosendlist(tx_msg);
				continue;
		}

		/* if tx list, send wakeup firstly */
		if (intf->fw_power_down == 1 &&
		    (atomic_read(&tx_msg->tx_list_qos_pool.ref) > 0 ||
		     !list_empty(&tx_msg->xmit_msg_list.to_send_list) ||
		     !list_empty(&tx_msg->xmit_msg_list.to_free_list))) {
				struct sprdwl_vif *vif;
				enum sprdwl_mode sprdwl_mode = SPRDWL_MODE_STATION;
				u8 mode_found = 0;

				for (sprdwl_mode = SPRDWL_MODE_STATION; sprdwl_mode < SPRDWL_MODE_MAX;
				     sprdwl_mode++) {
					if (priv->fw_stat[sprdwl_mode] == SPRDWL_INTF_OPEN) {
						mode_found = 1;
						break;
					}
				}

				if (0 == mode_found)
					continue;

				wl_info("%s, host wakeup fw!\n", __func__);
				vif = mode_to_vif(priv, sprdwl_mode);
				intf->fw_power_down = 0;
				sprdwl_work_host_wakeup_fw(vif);
				sprdwl_put_vif(vif);
				continue;
		}

		if (intf->fw_awake == 0) {
			printk_ratelimited("sc2355, %s, fw_awake = 0\n", __func__);
			usleep_range(50, 100);
			continue;
		}

		if (intf->suspend_mode != SPRDWL_PS_RESUMED) {
			printk_ratelimited("sc2355, %s, suspend_mode != RESUMED\n", __func__);
			usleep_range(50, 100);
			continue;
		}

		if (!list_empty(&tx_msg->xmit_msg_list.to_send_list)) {
			if (sprdwl_handle_to_send_list(intf, tx_msg->xmit_msg_list.mode)) {
				usleep_range(590, 610);
				continue;
			}
		}

		if (intf->pushfail_count > 100)
			sprdwl_flush_tosendlist(tx_msg);

		for (mode = SPRDWL_MODE_NONE; mode < SPRDWL_MODE_MAX; mode++) {
			int num = atomic_read(&tx_msg->tx_list[mode]->mode_list_num);

			if (num <= 0)
				continue;
			if (num > 0 && priv->fw_stat[mode] != SPRDWL_INTF_OPEN) {
				sprdwl_flush_mode_txlist(tx_msg, mode);
				continue;
			}

			send_num = sprdwl_fc_test_send_num(tx_msg, mode, num);
			if (send_num > 0)
				sprdwl_tx_eachmode_data(intf, mode);
			else
				need_polling |= (1 << (u8)mode);
		}
		/*sleep more time if screen off*/
		if (priv->is_screen_off == 1 &&
			(priv->hw_type == SPRDWL_HW_SC2355_PCIE ||
			priv->hw_type == SPRDWL_HW_SIPC)) {
			usleep_range(590, 610);
			continue;
		}
		if (need_polling) {
			/* retry to wait credit */
			usleep_range(10, 15);
			polling_times = 0;
			while (polling_times < TX_MAX_POLLING) {
				/* do not go to sleep immidiately */
				polling_times++;
				usleep_range(30, 50);
			}
		}
	}
	return 0;
}

int sprdwl_sc2355_reset(struct sprdwl_intf *intf)
{
	struct sprdwl_priv *priv = NULL;
	struct sprdwl_tx_msg *tx_msg = NULL;
	struct sprdwl_vif *vif, *tmp;
	int i;

	if (!intf) {
		wl_err("%s can not get intf!\n", __func__);
		return -1;
	}

	priv = intf->priv;
	if (!priv) {
		wl_err("%s can not get priv!\n", __func__);
		return -1;
	}

	tx_msg = (struct sprdwl_tx_msg *)intf->sprdwl_tx;
	if (!tx_msg) {
		wl_err("%s can not get tx_msg!\n", __func__);
		return -1;
	}

	/* need rest intf->exit flag, if wcn reset happened */
	if (unlikely(intf->exit)) {
		intf->exit = 0;
		wl_info("%s reset intf->exit flag: %d!\n",
			__func__, intf->exit);
	}

	/* need reset intf->cp_asserted flag */
	if (unlikely(intf->cp_asserted)) {
		intf->cp_asserted = 0;
		wl_info("%s reset intf->cp_asserted flag: %d!\n",
			__func__, intf->cp_asserted);
	}

	intf->fw_awake = 1;
	intf->fw_power_down = 0;

	list_for_each_entry_safe(vif, tmp, &priv->vif_list, vif_node) {
		int ciphyr_type, key_index;
		int ciphyr_type_max = 2, key_index_max = 6;
		/* check connect state */
		if (vif->mode == SPRDWL_MODE_STATION ||
			vif->mode == SPRDWL_MODE_P2P_CLIENT) {
			if (vif->sm_state == SPRDWL_DISCONNECTING ||
				vif->sm_state == SPRDWL_CONNECTING ||
				vif->sm_state == SPRDWL_CONNECTED) {
				wl_info("%s check connection state for sta or p2p\n", __func__);
				wl_info("vif->mode : %d, vif->sm_state: %d\n",
					vif->mode, vif->sm_state);
				cfg80211_disconnected(vif->ndev, 0, NULL, 0,
							false, GFP_KERNEL);
				vif->sm_state = SPRDWL_DISCONNECTED;
			}
		}

		if (vif->mode == SPRDWL_MODE_AP) {
			wl_info("softap mode, reset iftype to station, before reset: %d\n",
				vif->wdev.iftype);
			vif->wdev.iftype = NL80211_IFTYPE_STATION;
			wl_info("after reset iftype: %d\n", vif->wdev.iftype);
		}
		if (vif->mode != SPRDWL_MODE_NONE) {
			wl_info("need reset mode to none: %d\n", vif->mode);
			priv->fw_stat[vif->mode] = SPRDWL_INTF_CLOSE;
			vif->mode = SPRDWL_MODE_NONE;
			vif->ctx_id = 0;
			handle_tx_status_after_close(vif);
		}

		/* rest ssid & bssid */
		memset(vif->bssid, 0, sizeof(vif->bssid));
		memset(vif->ssid, 0, sizeof(vif->ssid));
		vif->ssid_len = 0;
		vif->prwise_crypto = SPRDWL_CIPHER_NONE;
		vif->grp_crypto = SPRDWL_CIPHER_NONE;

		for (ciphyr_type = 0; ciphyr_type < ciphyr_type_max; ciphyr_type++) {
			vif->key_index[ciphyr_type] = 0;
			for (key_index = 0; key_index < key_index_max; key_index++) {
				memset(vif->key[ciphyr_type][key_index], 0x00,
					WLAN_MAX_KEY_LEN);
				vif->key_len[ciphyr_type][key_index] = 0;
			}
		}
	}

	for (i = 0; i < 32; i++) {
		if (intf->peer_entry[i].ba_tx_done_map != 0) {
			intf->peer_entry[i].ht_enable = 0;
			intf->peer_entry[i].ip_acquired = 0;
			intf->peer_entry[i].ba_tx_done_map = 0;
		}
		peer_entry_delba((void *)intf, i);
		memset(&intf->peer_entry[i], 0x00,
			sizeof(struct sprdwl_peer_entry));
		intf->peer_entry[i].ctx_id = 0xFF;
		sprdwl_dis_flush_txlist(intf, i);
	}

	/* flush cmd and data buffer */
	wl_info("%s flush all tx list\n", __func__);
	sprdwl_flush_all_txlist(tx_msg);
	sprdwl_rx_flush_buffer((void *)intf);

	/* when cp2 hang and reset, claer hang_recvery_status */
	wl_info("%s set hang recovery status to END, %d\n", __func__, __LINE__);
	tx_msg->hang_recovery_status = HANG_RECOVERY_END;

	return 0;
}

int sprdwl_tx_init(struct sprdwl_intf *dev)
{
	int ret = 0;
	u8 i, j;
	struct sprdwl_tx_msg *tx_msg = NULL;

	g_max_fw_tx_dscr = 1024;

	tx_msg = kzalloc(sizeof(struct sprdwl_tx_msg), GFP_KERNEL);
	if (!tx_msg) {
		ret = -ENOMEM;
		wl_err("%s kzalloc failed!\n", __func__);
		goto exit;
	}

	spin_lock_init(&tx_msg->lock);/*useless?*/
	tx_msg->cmd_timeout = msecs_to_jiffies(SPRDWL_TX_CMD_TIMEOUT);
	tx_msg->data_timeout = msecs_to_jiffies(SPRDWL_TX_DATA_TIMEOUT);
	atomic_set(&tx_msg->flow0, 0);
	atomic_set(&tx_msg->flow1, 0);
	atomic_set(&tx_msg->flow2, 0);

	ret = sprdwl_msg_init(SPRDWL_TX_MSG_CMD_NUM, &tx_msg->tx_list_cmd);
	if (ret) {
		wl_err("%s tx_list_cmd alloc failed\n", __func__);
		goto err_tx_work;
	}

	ret = sprdwl_msg_init(SPRDWL_TX_QOS_POOL_SIZE,
			      &tx_msg->tx_list_qos_pool);
	if (ret) {
		wl_err("%s tx_list_qos_pool alloc failed\n", __func__);
		goto err_tx_list_cmd;
	}

	for (i = 0; i < SPRDWL_MODE_MAX; i++) {
		tx_msg->tx_list[i] = kzalloc(sizeof(struct tx_t), GFP_KERNEL);
		if (!tx_msg->tx_list[i])
			goto err_txlist;
		qos_init(tx_msg->tx_list[i]);
	}
	sprdwl_init_xmit_list(tx_msg);

	tx_msg->tx_thread = kthread_create(sprdwl_tx_work_queue,
			       (void *)tx_msg, "SPRDWL_TX_THREAD");
	if (!tx_msg->tx_thread) {
		wl_err("%s SPRDWL_TX_THREAD create failed", __func__);
		ret = -ENOMEM;
		/* temp debug */
		goto err_txlist;
	}

	dev->sprdwl_tx = (void *)tx_msg;
	tx_msg->intf = dev;

#ifdef WMMAC_WFA_CERTIFICATION
	reset_wmmac_parameters(tx_msg->intf->priv);
	reset_wmmac_ts_info();
#endif

	for (i = 0; i < MAX_COLOR_BIT; i++) {
		tx_msg->flow_ctrl[i].mode = SPRDWL_MODE_NONE;
		tx_msg->flow_ctrl[i].color_bit = i;
		atomic_set(&tx_msg->flow_ctrl[i].flow, 0);
	}

	tx_msg->hang_recovery_status = HANG_RECOVERY_END;
	dev->remove_flag = 0;
	init_completion(&tx_msg->tx_completed);
	wake_up_process(tx_msg->tx_thread);

	return ret;

err_txlist:
	for (j = 0; j < i; j++)
		kfree(tx_msg->tx_list[j]);

	sprdwl_msg_deinit(&tx_msg->tx_list_qos_pool);
err_tx_list_cmd:
	sprdwl_msg_deinit(&tx_msg->tx_list_cmd);
err_tx_work:
	kfree(tx_msg);
exit:
	return ret;
}

void sprdwl_tx_deinit(struct sprdwl_intf *intf)
{
	struct sprdwl_tx_msg *tx_msg = NULL;
	u8 i;

	wl_err("%s, %d\n", __func__, __LINE__);
	tx_msg = (void *)intf->sprdwl_tx;

	/*let tx work queue exit*/
	intf->exit = 1;
	intf->remove_flag = 1;

	if (tx_msg->tx_thread) {
		tx_up(tx_msg);
		kthread_stop(tx_msg->tx_thread);
		tx_msg->tx_thread = NULL;
	}

	/*need to check if there is some data and cmdpending
	*or sending by HIF, and wait until tx complete and freed
	*/
	if (!list_empty(&tx_msg->tx_list_cmd.cmd_to_free))
		wl_err("%s cmd not yet transmited, cmd_send:%d, cmd_poped:%d\n",
		       __func__, tx_msg->cmd_send, tx_msg->cmd_poped);

	sprdwl_flush_all_txlist(tx_msg);

	sprdwl_msg_deinit(&tx_msg->tx_list_cmd);
	sprdwl_msg_deinit(&tx_msg->tx_list_qos_pool);
	for (i = 0; i < SPRDWL_MODE_MAX; i++)
		kfree(tx_msg->tx_list[i]);
	kfree(tx_msg);
	intf->sprdwl_tx = NULL;
}

static inline unsigned short from32to16(unsigned int x)
{
	/* add up 16-bit and 16-bit for 16+c bit */
	x = (x & 0xffff) + (x >> 16);
	/* add up carry.. */
	x = (x & 0xffff) + (x >> 16);
	return x;
}

unsigned int do_csum(const unsigned char *buff, int len)
{
	int odd;
	unsigned int result = 0;

	if (len <= 0)
		goto out;
	odd = 1 & (unsigned long) buff;
	if (odd) {
#ifdef __LITTLE_ENDIAN
		result += (*buff << 8);
#else
		result = *buff;
#endif
		len--;
		buff++;
	}
	if (len >= 2) {
		if (2 & (unsigned long) buff) {
			result += *(unsigned short *) buff;
			len -= 2;
			buff += 2;
		}
		if (len >= 4) {
			const unsigned char *end = buff + ((unsigned)len & ~3);
			unsigned int carry = 0;
			do {
				unsigned int w = *(unsigned int *) buff;
				buff += 4;
				result += carry;
				result += w;
				carry = (w > result);
			} while (buff < end);
			result += carry;
			result = (result & 0xffff) + (result >> 16);
		}
		if (len & 2) {
			result += *(unsigned short *) buff;
			buff += 2;
		}
	}
	if (len & 1)
#ifdef __LITTLE_ENDIAN
		result += *buff;
#else
		result += (*buff << 8);
#endif
	result = from32to16(result);
	if (odd)
		result = ((result >> 8) & 0xff) | ((result & 0xff) << 8);
out:
	return result;
}

static int is_multicast_mac_addr(const u8 *addr)
{
	return ((addr[0] != 0xff) && (0x01 & addr[0]));
}

static int sprdwl_mc_pkt_checksum(struct sk_buff *skb, struct net_device *ndev)
{
	struct udphdr *udphdr;
	struct tcphdr *tcphdr;
	struct ipv6hdr *ipv6hdr;
	__sum16 checksum = 0;
	unsigned char iphdrlen = 0;
	struct sprdwl_vif *vif;
	struct sprdwl_intf *intf;

	vif = netdev_priv(ndev);
	intf = (struct sprdwl_intf *)vif->priv->hw_priv;
	ipv6hdr = (struct ipv6hdr *)(skb->data + ETHER_HDR_LEN);
	iphdrlen = sizeof(*ipv6hdr);

	udphdr = (struct udphdr *)(skb->data + ETHER_HDR_LEN + iphdrlen);
	tcphdr = (struct tcphdr *)(skb->data + ETHER_HDR_LEN + iphdrlen);

	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		checksum =
			(__force __sum16)do_csum(
			skb->data + ETHER_HDR_LEN + iphdrlen,
			skb->len - ETHER_HDR_LEN - iphdrlen);
		if (ipv6hdr->nexthdr == IPPROTO_UDP) {
			udphdr->check = ~checksum;
			wl_info("csum:%x,udp check:%x\n",
				checksum, udphdr->check);
		} else if (ipv6hdr->nexthdr == IPPROTO_TCP) {
			tcphdr->check = ~checksum;
			wl_info("csum:%x,tcp check:%x\n",
				checksum, tcphdr->check);
		} else
			return 1;
		skb->ip_summed = CHECKSUM_NONE;
		return 0;
	}
	return 1;
}

int sprdwl_tx_mc_pkt(struct sk_buff *skb, struct net_device *ndev)
{
	struct sprdwl_vif *vif;
	struct sprdwl_intf *intf;

	vif = netdev_priv(ndev);
	intf = (struct sprdwl_intf *)vif->priv->hw_priv;

	intf->skb_da = skb->data;
	if (intf->skb_da == NULL)/*TODO*/
		return 1;

	if (is_multicast_mac_addr(intf->skb_da) && vif->mode == SPRDWL_MODE_AP) {
		wl_info("%s,AP mode, multicast bssid: %02x:%02x:%02x:%02x:%02x:%02x\n", __func__,
			 intf->skb_da[0], intf->skb_da[1], intf->skb_da[2],
			 intf->skb_da[3], intf->skb_da[4], intf->skb_da[5]);
		sprdwl_mc_pkt_checksum(skb, ndev);
		sprdwl_xmit_data2cmd_wq(skb, ndev);
		return NETDEV_TX_OK;
	}
	return 1;

}

bool is_vowifi_pkt(struct sk_buff *skb, bool *b_cmd_path)
{
	bool ret = false;
	u8 dscp = 0;
	struct ethhdr *ethhdr = (struct ethhdr *)skb->data;
	unsigned char iphdrlen = 0;
	struct iphdr *iphdr;
	struct udphdr *udphdr;
	u32 mark;

	mark = skb->mark & DUAL_VOWIFI_MASK_MARK;
	switch (mark) {
	case DUAL_VOWIFI_NOT_SUPPORT:
		break;
	case DUAL_VOWIFI_SIP_MARK:
	case DUAL_VOWIFI_IKE_MARK:
		ret = true;
		(*b_cmd_path) = true;
		return ret;
	case DUAL_VOWIFI_VOICE_MARK:
	case DUAL_VOWIFI_VIDEO_MARK:
		ret = true;
		(*b_cmd_path) = false;
		return ret;

	default:
		wl_info("Dual vowifi: unexpect mark bits 0x%x\n", skb->mark);
		break;
	}

	if (ethhdr->h_proto != htons(ETH_P_IP))
		return false;

	iphdr = (struct iphdr *)(skb->data + ETHER_HDR_LEN);

	if (iphdr->protocol != IPPROTO_UDP)
		return false;

	iphdrlen = ip_hdrlen(skb);
	udphdr = (struct udphdr *)(skb->data + ETHER_HDR_LEN + iphdrlen);
	dscp = (iphdr->tos >> 2);
	switch (dscp) {
	case VOWIFI_IKE_DSCP:
		if ((udphdr->dest == htons(VOWIFI_IKE_SIP_PORT)) ||
		    (udphdr->dest == htons(VOWIFI_IKE_ONLY_PORT))) {
			ret = true;
			(*b_cmd_path) = true;
		}
		break;
	case VOWIFI_SIP_DSCP:
		if (udphdr->dest == htons(VOWIFI_IKE_SIP_PORT)) {
			ret = true;
			(*b_cmd_path) = true;
		}
		break;
	case VOWIFI_VIDEO_DSCP:
	case VOWIFI_AUDIO_DSCP:
		ret = true;
		(*b_cmd_path) = false;
		break;
	default:
		ret = false;
		(*b_cmd_path) = false;
		break;
	}

	return ret;
}

int sprdwl_tx_filter_ip_pkt(struct sk_buff *skb, struct net_device *ndev)
{
	bool is_data2cmd;
	bool is_ipv4_dhcp, is_ipv6_dhcp;
	bool is_vowifi2cmd;
	unsigned char *dhcpdata = NULL;
	struct udphdr *udphdr;
	struct iphdr *iphdr = NULL;
	struct ipv6hdr *ipv6hdr;
	__sum16 checksum = 0;
	struct ethhdr *ethhdr = (struct ethhdr *)skb->data;
	unsigned char iphdrlen = 0;
	unsigned char lut_index;
	struct sprdwl_vif *vif;
	struct sprdwl_intf *intf;

	vif = netdev_priv(ndev);
	intf = (struct sprdwl_intf *)vif->priv->hw_priv;

	if (ethhdr->h_proto == htons(ETH_P_IPV6)) {
		ipv6hdr = (struct ipv6hdr *)(skb->data + ETHER_HDR_LEN);
		/* check for udp header */
		if (ipv6hdr->nexthdr != IPPROTO_UDP)
			return 1;
		iphdrlen = sizeof(*ipv6hdr);
	} else if (ethhdr->h_proto == htons(ETH_P_IP)) {
		iphdr = (struct iphdr *)(skb->data + ETHER_HDR_LEN);
		/* check for udp header */
		if (iphdr->protocol != IPPROTO_UDP)
			return 1;
		iphdrlen = ip_hdrlen(skb);
	} else {
		return 1;
	}

	udphdr = (struct udphdr *)(skb->data + ETHER_HDR_LEN + iphdrlen);

	is_ipv4_dhcp =
	((ethhdr->h_proto == htons(ETH_P_IP)) &&
	((udphdr->source == htons(DHCP_SERVER_PORT)) ||
	(udphdr->source == htons(DHCP_CLIENT_PORT))));
	is_ipv6_dhcp =
	((ethhdr->h_proto == htons(ETH_P_IPV6)) &&
	((udphdr->source == htons(DHCP_SERVER_PORT_IPV6)) ||
	(udphdr->source == htons(DHCP_CLIENT_PORT_IPV6))));

	if (is_vowifi_pkt(skb, &is_vowifi2cmd)) {
		if (is_vowifi2cmd == false) {
			struct sprdwl_peer_entry *peer_entry = NULL;

			lut_index = sprdwl_find_lut_index(intf, vif);
			peer_entry = &intf->peer_entry[lut_index];
			if (peer_entry->vowifi_enabled == 1) {
				if (peer_entry->vowifi_pkt_cnt < 11)
					peer_entry->vowifi_pkt_cnt++;
				if (peer_entry->vowifi_pkt_cnt == 10)
					sprdwl_vowifi_data_protection(vif);
			}
		}
	} else {
		is_vowifi2cmd = false;
	}

	is_data2cmd = (is_ipv4_dhcp || is_ipv6_dhcp || is_vowifi2cmd);

	if (is_ipv4_dhcp) {
		wl_info("filter DHCP packet\n");
		intf->skb_da = skb->data;
		lut_index = sprdwl_find_lut_index(intf, vif);
		dhcpdata = skb->data + ETHER_HDR_LEN + iphdrlen + 250;
		if (*dhcpdata == 0x01) {
			wl_info("DHCP: TX DISCOVER\n");
		} else if (*dhcpdata == 0x02) {
			wl_info("DHCP: TX OFFER\n");
		} else if (*dhcpdata == 0x03) {
			wl_info("DHCP: TX REQUEST\n");
			intf->peer_entry[lut_index].ip_acquired = 1;
			if (sprdwl_is_group(skb->data))
				intf->peer_entry[lut_index].ba_tx_done_map = 0;
			wl_info("lut_index is %d, line:%d\n", lut_index,
				__LINE__);
		} else if (*dhcpdata == 0x04) {
			wl_info("DHCP: TX DECLINE\n");
		} else if (*dhcpdata == 0x05) {
			wl_info("DHCP: TX ACK\n");
			intf->peer_entry[lut_index].ip_acquired = 1;
			wl_info("lut_index is %d, line:%d\n", lut_index,
				__LINE__);
		} else if (*dhcpdata == 0x06) {
			wl_info("DHCP: TX NACK\n");
		}
	}

	/*as CP request, send data with CMD*/
	if (is_data2cmd) {
		if (is_ipv4_dhcp || is_ipv6_dhcp)
			wl_info("dhcp,check:%x,skb->ip_summed:%d\n",
				udphdr->check, skb->ip_summed);
		if (is_vowifi2cmd && iphdr)
			wl_info("vowifi, proto=0x%x, tos=0x%x, dest=0x%x\n",
				ethhdr->h_proto, iphdr->tos, udphdr->dest);

		if (skb->ip_summed == CHECKSUM_PARTIAL) {
			checksum =
				(__force __sum16)do_csum(
				skb->data + ETHER_HDR_LEN + iphdrlen,
				skb->len - ETHER_HDR_LEN - iphdrlen);
			udphdr->check = ~checksum;
			wl_info("csum:%x,check:%x\n",
				checksum, udphdr->check);
			skb->ip_summed = CHECKSUM_NONE;
		}

		sprdwl_xmit_data2cmd_wq(skb, ndev);
		return NETDEV_TX_OK;
	}

	return 1;
}

int sprdwl_tx_filter_packet(struct sk_buff *skb, struct net_device *ndev)
{
	struct sprdwl_vif *vif;
	struct sprdwl_intf *intf;
	struct ethhdr *ethhdr = (struct ethhdr *)skb->data;
	unsigned char lut_index;

	vif = netdev_priv(ndev);
	intf = (struct sprdwl_intf *)vif->priv->hw_priv;

#if defined(MORE_DEBUG)
	if (ethhdr->h_proto == htons(ETH_P_ARP))
		intf->stats.tx_arp_num++;
	if (sprdwl_is_group(skb->data))
		intf->stats.tx_multicast++;
#endif

	if (ethhdr->h_proto == htons(ETH_P_ARP)) {
		wl_info("incoming ARP packet\n");
		sprdwl_xmit_data2cmd_wq(skb, ndev);
		return NETDEV_TX_OK;
	}
	if (ethhdr->h_proto == htons(ETH_P_TDLS))
		wl_info("incoming TDLS packet\n");
	if (ethhdr->h_proto == htons(ETH_P_PREAUTH))
		wl_info("incoming PREAUTH packet\n");

	intf->skb_da = skb->data;
	if (ethhdr->h_proto == htons(ETH_P_IPV6)) {
		lut_index = sprdwl_find_lut_index(intf, vif);
		if ((vif->mode == SPRDWL_MODE_AP || vif->mode == SPRDWL_MODE_P2P_GO) &&
			(lut_index != 4) && intf->peer_entry[lut_index].ip_acquired == 0) {
			wl_info("ipv6 ethhdr->h_proto=%x\n", ethhdr->h_proto);
			dev_kfree_skb(skb);
			return 0;
		}
	}

	if ((ethhdr->h_proto == htons(ETH_P_IPV6)) &&
		!sprdwl_tx_mc_pkt(skb, ndev))
		return NETDEV_TX_OK;

	if (ethhdr->h_proto == htons(ETH_P_IP) ||
		ethhdr->h_proto == htons(ETH_P_IPV6))
		return sprdwl_tx_filter_ip_pkt(skb, ndev);
	return 1;
}
