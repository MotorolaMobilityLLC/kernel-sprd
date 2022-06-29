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

#include "sprdwl.h"
#include "if_sc2355.h"
#include "intf_ops.h"
#include "mm.h"
#include "tx_msg_sc2355.h"
#include "rx_msg_sc2355.h"
#include "work.h"
#include "tcp_ack.h"
#include "txrx_buf_mm.h"
#ifdef SIPC_SUPPORT
#include "sipc_txrx_mm.h"
#endif


#define INIT_INTF_SC2355(num, type, out, interval, bsize, psize, max,\
			 threshold, time, in_irq, pending, pop, push, complete, suspend) \
{ .channel = num, .hif_type = type, .inout = out, .intr_interval = interval,\
.buf_size = bsize, .pool_size = psize, .once_max_trans = max,\
.rx_threshold = threshold, .timeout = time, .cb_in_irq = in_irq, .max_pending = pending,\
.pop_link = pop, .push_link = push, .tx_complete = complete, .power_notify = suspend }

struct sprdwl_intf_sc2355 g_intf_sc2355;
struct sprdwl_intf *g_intf;

static inline struct sprdwl_intf *get_intf(void)
{
	return (struct sprdwl_intf *)g_intf_sc2355.intf;
}

int sprdwl_tx_cmd_pop_list(int channel, struct mbuf_t *head, struct mbuf_t *tail, int num);
int sprdwl_tx_data_pop_list(int channel, struct mbuf_t *head, struct mbuf_t *tail, int num);

static unsigned int chn_rx_push[8];
static unsigned int chn_rx_push_fail[8];

#define INTF_IS_PCIE \
	(get_intf()->priv->hw_type == SPRDWL_HW_SC2355_PCIE)

#define INTF_IS_SIPC \
	(get_intf()->priv->hw_type == SPRDWL_HW_SIPC)


void sprdwl_hex_dump(unsigned char *name,
		     unsigned char *data, unsigned short len)
{
	int i, p = 0, ret;
	unsigned char buf[255] = {0};

	if ((NULL == data) || (0 == len) || (NULL == name))
		return;

	sprintf(buf, "sc2355 sprdwl-wlan %s hex dump(len = %d)", name, len);
	pr_info("%s\n", buf);

	if (len > 1024)
		len = 1024;
	memset(buf, 0x00, 255);
	for (i = 0; i < len ; i++) {
		ret = sprintf((buf + p), "%02x ", *(data + i));
		if ((i != 0) && ((i + 1)%16 == 0)) {
			pr_info("%s\n", buf);
			p = 0;
			memset(buf, 0x00, 255);
		} else {
			p = p + ret;
		}
	}
	if (p != 0)
		pr_info("%s\n", buf);
}

/* FIXME: split push link & push link wait complete */
static int sprdwl_push_link(struct sprdwl_intf *intf, int chn,
			    struct mbuf_t *head, struct mbuf_t *tail, int num,
			    int (*pop)(int, struct mbuf_t *, struct mbuf_t *, int))
{
	int ret = 0;
	unsigned long time = 0;
	struct mbuf_t *pos = head;
	int i = 0;

	wl_debug("%s: start send chn %d, num %d, head %p, tail %p\n",
		 __func__, chn, num, head, tail);

	for (i = 0; i < num; i++) {
#ifndef SIPC_SUPPORT
#ifdef CONFIG_SPRD_WCN_DEBUG
		unsigned int zero_phy = 0;

		if (memcmp(&pos->phy, &zero_phy, 4) == 0) {
			wl_err("err phy address: %lx\n, err virt address: %lx\n, err port: %d\n",
				pos->phy, (long unsigned int)pos->buf, chn);
			BUG_ON(1);
		}
#endif
#endif
		if ((i == num) && (pos != tail)) {
			wl_info("num of head to tail is not match\n");
		}
		pos = pos->next;
	}

	time = jiffies;
	ret = sprdwcn_bus_push_list(chn, head, tail, num);
	time = jiffies - time;

	if (ret)
		wl_info("%s: push link fail: %d, chn: %d!\n",
			__func__, ret, chn);

	return ret;
}

#if defined(MORE_DEBUG)
void sprdwl_dump_stats(struct sprdwl_intf *intf)
{
	wl_err("++print txrx statistics++\n");
	wl_err("tx packets: %lu, tx bytes: %lu\n",  intf->stats.tx_packets,
	       intf->stats.tx_bytes);
	wl_err("tx filter num: %lu\n",  intf->stats.tx_filter_num);
	wl_err("tx errors: %lu, tx dropped: %lu\n",  intf->stats.tx_errors,
	       intf->stats.tx_dropped);
	wl_err("tx avg time: %lu\n",  intf->stats.tx_avg_time);
	wl_err("tx realloc: %lu\n",  intf->stats.tx_realloc);
	wl_err("tx arp num: %lu\n",  intf->stats.tx_arp_num);
	wl_err("rx packets: %lu, rx bytes: %lu\n",  intf->stats.rx_packets,
	       intf->stats.rx_bytes);
	wl_err("rx errors: %lu, rx dropped: %lu\n",  intf->stats.rx_errors,
	       intf->stats.rx_dropped);
	wl_err("rx multicast: %lu, tx multicast: %lu\n",
	       intf->stats.rx_multicast, intf->stats.tx_multicast);
	wl_err("--print txrx statistics--\n");
}

void sprdwl_clear_stats(struct sprdwl_intf *intf)
{
	memset(&intf->stats, 0x0, sizeof(struct txrx_stats));
}

/*calculate packets  average sent time from received
*from network stack to freed by HIF every STATS_COUNT packets
*/
void sprdwl_get_tx_avg_time(struct sprdwl_intf *intf,
			    unsigned long tx_start_time)
{
	struct timespec tx_end;

	getnstimeofday(&tx_end);
	intf->stats.tx_cost_time +=
	timespec_to_ns(&tx_end) - tx_start_time;
	if (intf->stats.gap_num >= STATS_COUNT) {
		intf->stats.tx_avg_time =
		intf->stats.tx_cost_time / intf->stats.gap_num;
		sprdwl_dump_stats(intf);
		intf->stats.gap_num = 0;
		intf->stats.tx_cost_time = 0;
		wl_info("%s:%d packets avg cost time: %lu\n",
			__func__, __LINE__, intf->stats.tx_avg_time);
	}
}
#endif

int if_tx_one(struct sprdwl_intf *intf, unsigned char *data,
	      int len, int chn)
{
	int ret = 0;
	struct mbuf_t *head = NULL, *tail = NULL, *mbuf = NULL;
	int num = 1;

	ret = sprdwcn_bus_list_alloc(chn, &head, &tail, &num);
	if (ret || head == NULL || tail == NULL) {
		wl_err("%s:%d sprdwcn_bus_list_alloc fail, chn: %d\n", __func__, __LINE__, chn);
		return -1;
	}

	mbuf = head;
	mbuf->buf = data;
	mbuf->len = len;
	mbuf->next = NULL;
	if (sprdwl_debug_level >= L_DBG)
		sprdwl_hex_dump("tx to cp2 cmd data dump", data, len);

	if (intf->priv->hw_type == SPRDWL_HW_SC2355_PCIE) {
		mbuf->phy = mm_virt_to_phys(&intf->pdev->dev, mbuf->buf,
					    mbuf->len, DMA_TO_DEVICE);
		ret = sprdwl_push_link(intf, chn, head, tail, num,
				       sprdwl_tx_cmd_pop_list);
	} else {
		ret = sprdwcn_bus_push_list(chn, head, tail, num);
	}

	if (ret) {
		mbuf = head;
		if (intf->priv->hw_type == SPRDWL_HW_SC2355_PCIE) {
			mm_phys_to_virt(&intf->pdev->dev, mbuf->phy, mbuf->len,
					DMA_TO_DEVICE, false);
			mbuf->phy = 0;
		}
		mbuf->buf = NULL;

		sprdwcn_bus_list_free(chn, head, tail, num);
	}

	return ret;
}


inline int if_tx_cmd(struct sprdwl_intf *intf, unsigned char *data, int len)
{
	return if_tx_one(intf, data, len, intf->tx_cmd_port);
}

inline int if_tx_addr_trans(struct sprdwl_intf *intf,
			    unsigned char *data, int len, bool send_now)
{
	struct sprdwl_rx_if *rx_if = (struct sprdwl_rx_if *)intf->sprdwl_rx;
	struct mbuf_t *head = NULL, *tail = NULL, *mbuf = NULL;
	int num = 1, ret = 0;

	if (data) {
		ret = sprdwcn_bus_list_alloc(intf->tx_data_port,
						&head, &tail, &num);
		if (ret || head == NULL || tail == NULL) {
			wl_err("%s:%d sprdwcn_bus_list_alloc fail, chn: %d\n",
				__func__, __LINE__, intf->tx_data_port);
		} else {
			mbuf = head;
			mbuf->buf = data;
			mbuf->len = len;
			mbuf->next = NULL;
			if (SPRDWL_HW_SC2355_PCIE == intf->priv->hw_type)
				mbuf->phy = mm_virt_to_phys(&intf->pdev->dev, mbuf->buf,
						mbuf->len, DMA_TO_DEVICE);
			if (rx_if->addr_trans_head) {
				((struct mbuf_t *)
					rx_if->addr_trans_tail)->next = head;
				rx_if->addr_trans_tail = (void *)tail;
				rx_if->addr_trans_num += num;
			} else {
				rx_if->addr_trans_head = (void *)head;
				if (!head)
					wl_err("ERROR! %s, %d, addr_trans_head set to NULL\n", __func__, __LINE__);
				rx_if->addr_trans_tail = (void *)tail;
				rx_if->addr_trans_num = num;
			}
		}
	}

	if (!ret && send_now && rx_if->addr_trans_head) {
		ret = sprdwl_push_link(intf, intf->tx_data_port,
				(struct mbuf_t *)rx_if->addr_trans_head,
				(struct mbuf_t *)rx_if->addr_trans_tail,
				rx_if->addr_trans_num, sprdwl_tx_cmd_pop_list);
		if (!ret) {
			rx_if->addr_trans_head = NULL;
			rx_if->addr_trans_tail = NULL;
			rx_if->addr_trans_num = 0;
		}
	}
	wl_debug("%s, trans rx buf, %d, cp2 buffer: %d\n", __func__, ret, skb_queue_len(&rx_if->mm_entry.buffer_list));
	return ret;
}

inline void if_tx_addr_trans_free(struct sprdwl_intf *intf)
{
	struct sprdwl_rx_if *rx_if = (struct sprdwl_rx_if *)intf->sprdwl_rx;

	sprdwl_tx_cmd_pop_list(intf->tx_data_port,
			(struct mbuf_t *)rx_if->addr_trans_head,
			(struct mbuf_t *)rx_if->addr_trans_tail,
			rx_if->addr_trans_num);

	rx_if->addr_trans_head = NULL;
	rx_if->addr_trans_tail = NULL;
	rx_if->addr_trans_num = 0;
	wl_err("%s, %d, addr_trans_num set to 0\n", __func__, __LINE__);
}

#define ADDR_OFFSET 7

static inline struct pcie_addr_buffer
*sprdwl_alloc_pcie_addr_buf(int tx_count)
{
	struct pcie_addr_buffer *addr_buffer = NULL;
//#define ADDR_OFFSET 7

	addr_buffer =
		kzalloc(sizeof(struct pcie_addr_buffer) +
		tx_count * SPRDWL_PHYS_LEN, GFP_ATOMIC);
	mb();
	if (addr_buffer == NULL) {
		wl_err("%s:%d alloc pcie addr buf fail\n", __func__, __LINE__);
		return NULL;
	}
	addr_buffer->common.type = SPRDWL_TYPE_DATA_PCIE_ADDR;
	addr_buffer->common.direction_ind = 0;
	addr_buffer->common.buffer_type = 1;
	addr_buffer->number = tx_count;
	addr_buffer->offset = ADDR_OFFSET;
	addr_buffer->buffer_ctrl.buffer_inuse = 1;

	return addr_buffer;
}

static inline struct pcie_addr_buffer
*sprdwl_set_pcie_addr_to_mbuf(struct sprdwl_tx_msg *tx_msg, struct mbuf_t *mbuf,
				   int tx_count)
{
//#define ADDR_OFFSET 7
	struct pcie_addr_buffer *addr_buffer;
	//struct sprdwl_intf *intf = tx_msg->intf;

	addr_buffer = sprdwl_alloc_pcie_addr_buf(tx_count);
	if (addr_buffer == NULL)
		return NULL;
	mbuf->len = ADDR_OFFSET + tx_count * SPRDWL_PHYS_LEN;
	mbuf->buf = (unsigned char *)addr_buffer;
	if (sprdwl_debug_level >= L_DBG)
		sprdwl_hex_dump("tx buf:", mbuf->buf, mbuf->len);
//	mbuf->phy = mm_virt_to_phys(&intf->pdev->dev, mbuf->buf,
//				    mbuf->len, DMA_TO_DEVICE);

	return addr_buffer;
}

void sprdwl_add_tx_list_head(struct list_head *tx_fail_list,
			     struct list_head *tx_list,
			     int ac_index,
			     int tx_count)
{
	struct sprdwl_msg_buf *msg_buf = NULL;
	struct list_head *xmit_free_list;
	struct list_head *head, *tail;
	spinlock_t *lock;
	spinlock_t *free_lock;

	if (tx_fail_list == NULL)
		return;
	msg_buf = list_first_entry(tx_fail_list, struct sprdwl_msg_buf, list);
	xmit_free_list = &msg_buf->xmit_msg_list->to_free_list;
	free_lock = &msg_buf->xmit_msg_list->free_lock;
	if (msg_buf->msg_type != SPRDWL_TYPE_DATA) {
		lock = &msg_buf->msglist->busylock;
	} else {
		if (SPRDWL_AC_MAX != ac_index)
			lock = &msg_buf->data_list->p_lock;
		else
			lock = &msg_buf->xmit_msg_list->send_lock;
	}
	spin_lock_bh(free_lock);
	head = tx_fail_list->next;
	tail = tx_fail_list->prev;
	head->prev->next = tail->next;
	tail->next->prev = head->prev;
	head->prev = tx_fail_list;
	tail->next = tx_fail_list;
	atomic_sub(tx_count, &msg_buf->xmit_msg_list->free_num);
	spin_unlock_bh(free_lock);

	spin_lock_bh(lock);
	list_splice(tx_fail_list, tx_list);
	spin_unlock_bh(lock);
	INIT_LIST_HEAD(tx_fail_list);
}

void sprdwl_add_to_free_list(struct sprdwl_priv *priv,
			struct list_head *tx_list_head,
			int tx_count)
{
	struct sprdwl_intf *intf = (struct sprdwl_intf *)priv->hw_priv;
	struct sprdwl_tx_msg *tx_msg = (struct sprdwl_tx_msg *)intf->sprdwl_tx;

	spin_lock(&tx_msg->xmit_msg_list.free_lock);
	list_splice_tail(tx_list_head, &tx_msg->xmit_msg_list.to_free_list);
	spin_unlock(&tx_msg->xmit_msg_list.free_lock);
}

/*cut data list from tx data list*/
static inline int
sprdwl_list_cut_to_free_list(struct list_head *tx_list_head,
		struct list_head *tx_list, struct list_head *tail_entry,
		int tx_count)
{
	struct sprdwl_intf *intf = get_intf();
	struct sprdwl_tx_msg *tx_msg = (struct sprdwl_tx_msg *)intf->sprdwl_tx;
	int ret = 0;
	struct list_head tx_list_tmp;

	if (tail_entry == NULL) {
		wl_err("%s, %d, error tail_entry\n", __func__, __LINE__);
		return -1;
	}

	INIT_LIST_HEAD(&tx_list_tmp);
	list_cut_position(&tx_list_tmp, tx_list, tail_entry);
	atomic_add(tx_count, &tx_msg->xmit_msg_list.free_num);
	spin_lock(&tx_msg->xmit_msg_list.free_lock);
	list_splice_tail(&tx_list_tmp, &tx_msg->xmit_msg_list.to_free_list);
	spin_unlock(&tx_msg->xmit_msg_list.free_lock);

	return ret;
}

static inline void
sprdwl_list_cut_to_send_list(struct list_head *head_entry,
			     struct list_head *tail_entry,
			     int count)
{
	struct sprdwl_intf *intf = get_intf();
	struct sprdwl_tx_msg *tx_msg = (struct sprdwl_tx_msg *)intf->sprdwl_tx;
	struct list_head list_tmp;
	struct list_head *head;

	INIT_LIST_HEAD(&list_tmp);
	spin_lock(&tx_msg->xmit_msg_list.free_lock);
	head = head_entry->prev;
	list_cut_position(&list_tmp, head, tail_entry);
	atomic_sub(count, &tx_msg->xmit_msg_list.free_num);
	spin_unlock(&tx_msg->xmit_msg_list.free_lock);
	list_splice(&list_tmp, &tx_msg->xmit_msg_list.to_send_list);
}

unsigned long cp_pcie_addr[4096] = { 0 };
DEFINE_SPINLOCK(cp_pcie_addr_lock);

void save_cp_pcie_addr(unsigned long pcie_addr)
{
#if 0
	int i = 0;

	spin_lock(&cp_pcie_addr_lock);

	for (i = 0; i < 4096; i++) {
		if (cp_pcie_addr[i] == pcie_addr) {
			wl_err("%s: same addr: %lx\n", __func__, pcie_addr);
			spin_unlock(&cp_pcie_addr_lock);
		}
	}

	for (i = 0; i < 4096; i++) {
		if (cp_pcie_addr[i] == 0) {
			cp_pcie_addr[i] = pcie_addr;
			//wl_err("%s: save %lx\n", __func__, pcie_addr);
			break;
		}
	}

	if (i == 4096) {
		wl_err("%s: too much pcie addr", __func__);
		spin_unlock(&cp_pcie_addr_lock);
	}

	spin_unlock(&cp_pcie_addr_lock);
#endif
}

void clear_cp_pcie_addr(unsigned long pcie_addr)
{
#if 0
	int i = 0;

	pcie_addr &= 0xffffffffff;

	spin_lock(&cp_pcie_addr_lock);

	for (i = 0; i < 4096; i++) {
		if (cp_pcie_addr[i] == pcie_addr) {
			cp_pcie_addr[i] = 0;
			break;
		}
	}

	if (i == 4096) {
		wl_err("%s: cannot find pcie addr: %lx\n", __func__, pcie_addr);
		spin_unlock(&cp_pcie_addr_lock);
	}

	for (i = 0; i < 4096; i++) {
		if (cp_pcie_addr[i] == pcie_addr) {
			wl_err("%s: should not find same pcie addr: %lx\n",
			       __func__, pcie_addr);
			spin_unlock(&cp_pcie_addr_lock);
		}

	}

	spin_unlock(&cp_pcie_addr_lock);
#endif
}

static inline void sprdwl_mbuf_list_free(struct sprdwl_intf *dev,
					 struct mbuf_t *head,
					 struct mbuf_t *tail,
					 int count)
{
	int i;
	struct mbuf_t *mbuf_pos = head;

	for (i = 0; i < count && mbuf_pos; i++) {
		if (mbuf_pos->buf) {
			mm_phys_to_virt(&dev->pdev->dev,
					mbuf_pos->phy, mbuf_pos->len,
					DMA_TO_DEVICE, false);
			kfree(mbuf_pos->buf);
			mbuf_pos->phy = 0;
			mbuf_pos->buf = 0;
			mbuf_pos->len = 0;
		}
		mbuf_pos = mbuf_pos->next;
	}
	sprdwcn_bus_list_free(dev->tx_data_port, head, tail, count);
}

int sprdwl_intf_tx_list(struct sprdwl_intf *dev,
			struct list_head *tx_list,
			struct list_head *tx_list_head,
			int tx_count,
			int ac_index)
{
#ifdef SIPC_SUPPORT
#define PCIE_TX_NUM 48
#else
#define PCIE_TX_NUM 96
#endif
	int ret = 0, i = 0, j = PCIE_TX_NUM, k = 0, pcie_count = 0, cnt = 0, num = 0;
	struct sprdwl_msg_buf *msg_pos;
	struct pcie_addr_buffer *addr_buffer = NULL;
	struct sprdwl_tx_msg *tx_msg;
	struct mbuf_t *head = NULL, *tail = NULL, *mbuf_pos;
	/*struct sprdwl_data_hdr *hdr; *//*temp for test*/
	struct list_head *pos, *tx_list_tail, *tx_head = NULL;
	struct tx_msdu_dscr *dscr = NULL;
	int print_len;
#if defined(MORE_DEBUG)
	unsigned long tx_bytes = 0;
#endif

	wl_debug("%s:%d tx_count is %d\n", __func__, __LINE__, tx_count);
	tx_msg = (struct sprdwl_tx_msg *)dev->sprdwl_tx;
	if (dev->priv->hw_type == SPRDWL_HW_SC2355_PCIE ||
			SPRDWL_HW_SIPC == dev->priv->hw_type) {
		if (tx_count <= PCIE_TX_NUM) {
			pcie_count = 1;
		} else {
			cnt = tx_count;
			while (cnt > PCIE_TX_NUM) {
				++num;
				cnt -= PCIE_TX_NUM;
			}
			pcie_count = num + 1;
		}
		ret = sprdwcn_bus_list_alloc(dev->tx_data_port, &head, &tail, &pcie_count); //port is 6
	} else {
		ret = sprdwcn_bus_list_alloc(dev->tx_data_port, &head, &tail, &tx_count); //port is 9
	}
	if (ret != 0 || head == NULL || tail == NULL || pcie_count == 0) {
		wl_err("%s:%d mbuf link alloc fail\n", __func__, __LINE__);
		return -1;
	}

	mbuf_pos = head;
	if (dev->priv->hw_type == SPRDWL_HW_SC2355_PCIE ||
		SPRDWL_HW_SIPC == dev->priv->hw_type) {
		for (i = 0; i < pcie_count && mbuf_pos; i++) {
			/* To prevent the mbuf_pos->buf not NULL case */
			mbuf_pos->buf = NULL;
			mbuf_pos = mbuf_pos->next;
		}
		mbuf_pos = head;
		if (pcie_count > 1) {
			addr_buffer =
			sprdwl_set_pcie_addr_to_mbuf(tx_msg,
				mbuf_pos, PCIE_TX_NUM);
		} else {
			addr_buffer =
			sprdwl_set_pcie_addr_to_mbuf(tx_msg,
				mbuf_pos, tx_count);
		}
		if (addr_buffer == NULL) {
			wl_err("%s:%d alloc pcie addr buf fail\n",
			       __func__, __LINE__);
			sprdwl_mbuf_list_free(dev, head, tail, pcie_count);
			return -1;
		}
	}

	i = 0;
	list_for_each(pos, tx_list) {
		msg_pos = list_entry(pos, struct sprdwl_msg_buf, list);
		sprdwl_move_tcpack_msg(dev->priv, msg_pos);
		if (tx_head == NULL)
			tx_head = pos;
/*TODO*/
		if (msg_pos->len > 200)
			print_len = 200;
		else
			print_len = msg_pos->len;
		if (sprdwl_debug_level >= L_DBG)
			sprdwl_hex_dump("tx to cp2 data",
				(unsigned char *)(msg_pos->tran_data),
				print_len);
#if defined(MORE_DEBUG)
		tx_bytes += msg_pos->skb->len;
#endif
		if (SPRDWL_HW_SC2355_PCIE == dev->priv->hw_type ||
			SPRDWL_HW_SIPC == dev->priv->hw_type) {
			if (pcie_count > 1 && num > 0 && i >= j) {
				if (--num == 0) {
					if (cnt > 0) {
						if (SPRDWL_HW_SC2355_PCIE == dev->priv->hw_type)
							mbuf_pos->phy =
								mm_virt_to_phys(&dev->pdev->dev, mbuf_pos->buf,
									mbuf_pos->len, DMA_TO_DEVICE);
						if (sprdwl_debug_level >= L_DBG)
							sprdwl_hex_dump("tx to addr trans",
								(unsigned char *)(mbuf_pos->buf), mbuf_pos->len);
						mbuf_pos = mbuf_pos->next;
						addr_buffer =
						sprdwl_set_pcie_addr_to_mbuf(
						tx_msg, mbuf_pos, cnt);
						if (addr_buffer == NULL) {
							wl_err("%s:%d alloc pcie addr buf fail\n",
								__func__, __LINE__);
							sprdwl_mbuf_list_free(dev, head, tail, pcie_count);
							return -1;
						}
					} else {
						wl_err("%s: cnt %d\n", __func__, cnt);
					}
				} else {
					j += PCIE_TX_NUM;

					if (SPRDWL_HW_SC2355_PCIE == dev->priv->hw_type)
						mbuf_pos->phy = mm_virt_to_phys(&dev->pdev->dev,
							mbuf_pos->buf, mbuf_pos->len, DMA_TO_DEVICE);
					if (sprdwl_debug_level >= L_DBG)
						sprdwl_hex_dump("tx to addr trans",
							(unsigned char *)(mbuf_pos->buf), mbuf_pos->len);
					mbuf_pos = mbuf_pos->next;
					addr_buffer =
					sprdwl_set_pcie_addr_to_mbuf(
					tx_msg, mbuf_pos, PCIE_TX_NUM);
				}
				if (addr_buffer == NULL) {
					wl_err("%s:%d alloc pcie addr buf fail\n",
					       __func__, __LINE__);
					sprdwl_mbuf_list_free(dev, head, tail, pcie_count);
					return -1;
				}

				k = 0;
			}

			if (msg_pos->skb) {
				if (SPRDWL_HW_SC2355_PCIE == dev->priv->hw_type)
					sprdwl_skb_to_tx_buf(dev, msg_pos);
				else if (SPRDWL_HW_SIPC == dev->priv->hw_type) {
#ifdef SIPC_SUPPORT
					sipc_skb_to_tx_buf(dev, msg_pos);
#endif
				}
			}

			wl_debug("debug pcie addr: 0x%lx\n", msg_pos->pcie_addr);
			memcpy(&addr_buffer->pcie_addr[k],
			       &msg_pos->pcie_addr, SPRDWL_PHYS_LEN);
			save_cp_pcie_addr(msg_pos->pcie_addr);
			dscr = (struct tx_msdu_dscr *)(msg_pos->tran_data + MSDU_DSCR_RSVD);
			addr_buffer->common.interface = dscr->common.interface;
			//printk(KERN_EMERG"addr_buffer buffer_type=%d, buffer_inuse=%d\n",
			//	addr_buffer->common.buffer_type, addr_buffer->buffer_ctrl.buffer_inuse);
			k++;
		} else {
			mbuf_pos->buf = msg_pos->tran_data - dev->hif_offset;
			mbuf_pos->len = msg_pos->len;
			mbuf_pos = mbuf_pos->next;
		}
		if (++i == tx_count)
			break;
	}

	if (SPRDWL_HW_SC2355_PCIE == dev->priv->hw_type)
		mbuf_pos->phy = mm_virt_to_phys(&dev->pdev->dev,
			mbuf_pos->buf, mbuf_pos->len, DMA_TO_DEVICE);
	if (sprdwl_debug_level >= L_DBG)
		sprdwl_hex_dump("tx to addr trans",
			(unsigned char *)(mbuf_pos->buf), mbuf_pos->len);

	tx_list_tail = pos;

	if (dev->priv->hw_type == SPRDWL_HW_SC2355_PCIE ||
		SPRDWL_HW_SIPC == dev->priv->hw_type) {
		sprdwl_list_cut_to_free_list(tx_list_head,
					     tx_list, tx_list_tail,
					     tx_count);

		dev->mbuf_head = (void *)head;
		dev->mbuf_tail = (void *)tail;
		dev->mbuf_num = pcie_count;

		if (dev->mbuf_head) {
			/*ret = mchn_push_link(9, head, tail, pcie_count);*/
			/*edma sync function*/
			ret = sprdwl_push_link(dev, dev->tx_data_port,
					(struct mbuf_t *)dev->mbuf_head,
					(struct mbuf_t *)dev->mbuf_tail,
					dev->mbuf_num, sprdwl_tx_data_pop_list);
			if (ret != 0) {
				/*wl_err("%s: push link fail\n", __func__); */
				dev->pushfail_count++;
				sprdwl_list_cut_to_send_list(tx_head,
							     tx_list_tail,
							     tx_count);
				if (tx_list_tail)
					sprdwl_mbuf_list_free(dev, head, tail, pcie_count);
			} else {
#if defined(MORE_DEBUG)
				UPDATE_TX_PACKETS(dev, tx_count, tx_bytes);
#endif
				INIT_LIST_HEAD(tx_list_head);
				dev->mbuf_head = NULL;
				dev->mbuf_tail = NULL;
				dev->mbuf_num = 0;
				dev->pushfail_count = 0;
				tx_msg->tx_num += tx_count;
			}
		}
		return ret;
	}

	return ret;
}

struct sprdwl_peer_entry
*sprdwl_find_peer_entry_using_addr(struct sprdwl_vif *vif, u8 *addr)
{
	struct sprdwl_intf *intf;
	struct sprdwl_peer_entry *peer_entry = NULL;
	u8 i;

	intf = (struct sprdwl_intf *)vif->priv->hw_priv;
	for (i = 0; i < MAX_LUT_NUM; i++) {
		if (ether_addr_equal(intf->peer_entry[i].tx.da, addr)) {
			peer_entry = &intf->peer_entry[i];
			break;
		}
	}
	if (!peer_entry)
		wl_err("not find peer_entry at :%s\n", __func__);

	return peer_entry;
}

/* It is tx private function, just use in sprdwl_intf_fill_msdu_dscr()  */
unsigned char sprdwl_find_lut_index(struct sprdwl_intf *intf,
				    struct sprdwl_vif *vif)
{
	unsigned char i;

	if (intf->skb_da == NULL)/*TODO*/
		goto out;

	wl_debug("%s,bssid: %02x:%02x:%02x:%02x:%02x:%02x\n", __func__,
		 intf->skb_da[0], intf->skb_da[1], intf->skb_da[2],
		 intf->skb_da[3], intf->skb_da[4], intf->skb_da[5]);
	if (sprdwl_is_group(intf->skb_da) &&
	    (vif->mode == SPRDWL_MODE_AP || vif->mode == SPRDWL_MODE_P2P_GO)) {
		for (i = 0; i < MAX_LUT_NUM; i++) {
			if ((sprdwl_is_group(intf->peer_entry[i].tx.da)) &&
			    (intf->peer_entry[i].ctx_id == vif->ctx_id)) {
				wl_info("%s, %d, group lut_index=%d\n",
					__func__, __LINE__,
					intf->peer_entry[i].lut_index);
				return intf->peer_entry[i].lut_index;
			}
		}
		if (vif->mode == SPRDWL_MODE_AP) {
			wl_info("%s,AP mode, group bssid,\n"
				"lut not found, ctx_id:%d, return lut:4\n",
				__func__, vif->ctx_id);
			return 4;
		}
		if (vif->mode == SPRDWL_MODE_P2P_GO) {
			wl_info("%s,GO mode, group bssid,\n"
				"lut not found, ctx_id:%d, return lut:5\n",
				__func__, vif->ctx_id);
			return 5;
		}
	}

	for (i = 0; i < MAX_LUT_NUM; i++) {
		if ((0 == memcmp(intf->peer_entry[i].tx.da,
				 intf->skb_da, ETH_ALEN)) &&
		    (intf->peer_entry[i].ctx_id == vif->ctx_id)) {
			wl_debug("%s, %d, lut_index=%d\n",
				 __func__, __LINE__,
				 intf->peer_entry[i].lut_index);
			return intf->peer_entry[i].lut_index;
		}
	}

	for (i = 0; i < MAX_LUT_NUM; i++) {
		if ((vif->mode == SPRDWL_MODE_STATION ||
			vif->mode == SPRDWL_MODE_STATION_SECOND ||
		     vif->mode == SPRDWL_MODE_P2P_CLIENT) &&
		    (intf->peer_entry[i].ctx_id == vif->ctx_id)) {
			wl_debug("%s, %d, lut_index=%d\n",
				 __func__, __LINE__,
				 intf->peer_entry[i].lut_index);
			return intf->peer_entry[i].lut_index;
		}
	}

out:
	if (vif->mode == SPRDWL_MODE_STATION ||
		vif->mode == SPRDWL_MODE_STATION_SECOND ||
		vif->mode == SPRDWL_MODE_P2P_CLIENT) {
		wl_err("%s,%d,bssid not found, multicast?\n"
		       "default of STA/GC = 0,\n",
		       __func__, vif->ctx_id);
		return 0;
	}
	if (vif->mode == SPRDWL_MODE_AP) {
		wl_err("%s,%d,bssid not found, multicast?\n"
		       "default of AP = 4\n",
		       __func__, vif->ctx_id);
		return 4;
	}
	if (vif->mode == SPRDWL_MODE_P2P_GO) {
		wl_err("%s,%d,bssid not found, multicast?\n"
		       "default of GO = 5\n",
		       __func__, vif->ctx_id);
		return 5;
	}
	return 0;
}

int sprdwl_intf_fill_msdu_dscr(struct sprdwl_vif *vif,
			       struct sk_buff *skb,
			       u8 type, u8 offset)
{
	u8 protocol;
	struct tx_msdu_dscr *dscr;
	struct sprdwl_intf *dev;
	u8 lut_index;
	struct sk_buff *temp_skb;
	unsigned char dscr_rsvd = 0;
	unsigned long dma_addr = 0;
	struct ethhdr *ethhdr = (struct ethhdr *)skb->data;
	u8 is_special_data = 0;
	bool is_vowifi2cmd = false;
#define MSG_PTR_LEN 8

	if (ethhdr->h_proto == htons(ETH_P_ARP) ||
		ethhdr->h_proto == htons(ETH_P_TDLS) ||
		ethhdr->h_proto == htons(ETH_P_PREAUTH))
		is_special_data = 1;
	else if ((type == SPRDWL_TYPE_CMD) &&
		 is_vowifi_pkt(skb, &is_vowifi2cmd))
		is_special_data = 1;

	dev = (struct sprdwl_intf *)(vif->priv->hw_priv);
	dscr_rsvd = (INTF_IS_PCIE || INTF_IS_SIPC) ? MSDU_DSCR_RSVD : 0;
	if (skb_headroom(skb) < (DSCR_LEN + dev->hif_offset +
		MSG_PTR_LEN + dscr_rsvd)) {
		temp_skb = skb;

		skb = skb_realloc_headroom(skb, (DSCR_LEN + dev->hif_offset +
						 MSG_PTR_LEN + dscr_rsvd));
		kfree_skb(temp_skb);
		if (skb == NULL) {
			wl_err("%s:%d failed to unshare skbuff: NULL\n",
			       __func__, __LINE__);
			return -EPERM;
		}
#if defined(MORE_DEBUG)
		dev->stats.tx_realloc++;
#endif
	}

	dev->skb_da = skb->data;

	lut_index = sprdwl_find_lut_index(dev, vif);

	if ((lut_index < 6) && (!sprdwl_is_group(dev->skb_da))) {
		wl_err("%s, %d, sta disconn, no data tx!", __func__, __LINE__);
		return -EPERM;
	}
	skb_push(skb, DSCR_LEN + offset);

	dscr = (struct tx_msdu_dscr *)(skb->data);
	memset(dscr, 0x00, DSCR_LEN);
	dscr->common.type = (type == SPRDWL_TYPE_CMD ?
		SPRDWL_TYPE_CMD : SPRDWL_TYPE_DATA);
	dscr->common.direction_ind = 0;
	dscr->common.need_rsp = 0;/*TODO*/
	dscr->common.interface = vif->ctx_id;
	dscr->pkt_len = cpu_to_le16(skb->len - DSCR_LEN);
	dscr->offset = DSCR_LEN;
	/*TODO*/
	dscr->tx_ctrl.sw_rate = (is_special_data == 1 ? 1 : 0);
	dscr->tx_ctrl.wds = 0; /*TBD*/
	dscr->tx_ctrl.swq_flag = 0; /*TBD*/
	dscr->tx_ctrl.rsvd = 0; /*TBD*/
	dscr->tx_ctrl.next_buffer_type = 0;
	dscr->tx_ctrl.pcie_mh_readcomp = 0;
	dscr->buffer_info.msdu_tid = 0;
	dscr->buffer_info.mac_data_offset = 0;
	dscr->sta_lut_index = lut_index;

	/* For MH to get phys addr */
	if (INTF_IS_PCIE || INTF_IS_SIPC) {
		skb_push(skb, dscr_rsvd);
		dma_addr = virt_to_phys(skb->data) | SPRDWL_MH_ADDRESS_BIT;
		memcpy(skb->data, &dma_addr, dscr_rsvd);
	}

	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		dscr->tx_ctrl.checksum_offload = 1;
		if (ethhdr->h_proto == htons(ETH_P_IPV6))
			protocol = ipv6_hdr(skb)->nexthdr;
		else
			protocol = ip_hdr(skb)->protocol;

		dscr->tx_ctrl.checksum_type =
			protocol == IPPROTO_TCP ? 1 : 0;
		dscr->tcp_udp_header_offset =
			skb->transport_header - skb->mac_header;
		wl_debug("%s: offload: offset: %d, protocol: %d\n",
			 __func__, dscr->tcp_udp_header_offset, protocol);
	}

	return 0;
}

#if defined FPGA_LOOPBACK_TEST
unsigned char tx_ipv4_udp[] = {
0x02, 0x04, 0x00, 0x01, 0x00, 0x06, 0x40, 0x45,
0xda, 0xf0, 0xff, 0x7e, 0x08, 0x00, 0x45, 0x00,
0x00, 0xe4, 0xc8, 0xc0, 0x40, 0x00, 0x40, 0x11,
0x8d, 0xb2, 0xc0, 0xa8, 0x31, 0x01, 0xc0, 0xa8,
0x31, 0x44, 0x67, 0x62, 0x3c, 0xbe, 0x00, 0xd0,
0xa0, 0x26, 0x80, 0x21, 0x81, 0x4b, 0x03, 0x68,
0x2b, 0x37, 0xde, 0xad, 0xbe, 0xef, 0x47, 0x10,
0x11, 0x35, 0xb1, 0x00, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00,
0x00, 0x00, 0xAA, 0xBB};

unsigned char tx_arp[] = {
0x02, 0x00, 0x00, 0x01, 0x00, 0x06, 0x74, 0x27,
0xea, 0xc8, 0x3e, 0x69, 0x08, 0x06, 0x00, 0x01,
0x08, 0x00, 0x06, 0x04, 0x00, 0x01, 0x74, 0x27,
0xea, 0xc8, 0x3e, 0x69, 0xc0, 0xa8, 0x01, 0x14,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xa8,
0x01, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0xaa, 0xbb};

int sprdwl_intf_tx_data_fpga_test(struct sprdwl_intf *intf,
				  unsigned char *data, int len)
{
	struct sprdwl_msg_buf *msg;
	struct sk_buff *skb;
	int ret;

	wl_debug("%s: #%d start\n", __func__,  intf->loopback_n);
	msg = sprdwl_get_msg_buf(intf, SPRDWL_TYPE_DATA,
				 SPRDWL_MODE_STATION, 1);
	if (!msg) {
		wl_err("%s:%d get msg buf failed\n", __func__, __LINE__);
		return -1;
	}
	if (data == NULL) {
		skb = dev_alloc_skb(244 + NET_IP_ALIGN);
		skb_reserve(skb, NET_IP_ALIGN);
		memcpy(skb->data, tx_ipv4_udp, 244);
		skb_put(skb, 244);
	} else {
		skb = dev_alloc_skb(len + NET_IP_ALIGN);
		skb_reserve(skb, NET_IP_ALIGN);
		memcpy(skb->data, data, len);
		skb_put(skb, len);
	}
	ret = sprdwl_send_data_fpga_test(intf->priv,
					 msg, skb, SPRDWL_TYPE_DATA, 0);
	wl_debug("%s:%d loopback_n#%d end ret=%d\n", __func__, __LINE__,
		 intf->loopback_n, ret);
	intf->loopback_n++;
	return ret;
}

int sprdwl_intf_fill_msdu_dscr_test(struct sprdwl_priv *priv,
				    struct sk_buff *skb,
				    u8 type,
				    u8 offset)
{
	struct tx_msdu_dscr *dscr;
	struct sprdwl_intf *intf;
	u8 lut_index;
	struct sk_buff *temp_skb;
#define DSCR_LEN	11
#define MSG_PTR_LEN 8

	intf = (struct sprdwl_intf *)(priv->hw_priv);
	if (skb_headroom(skb) < (DSCR_LEN + intf->hif_offset + MSG_PTR_LEN)) {
		temp_skb = skb;

		skb = skb_realloc_headroom(skb, (DSCR_LEN + intf->hif_offset +
						 MSG_PTR_LEN));
		kfree_skb(temp_skb);
		if (skb == NULL) {
			wl_err("%s:%d failed to realloc skbuff: NULL\n",
			       __func__, __LINE__);
			return 0;
		}
	}
	intf->skb_da = skb->data;
	lut_index = sprdwl_find_index_using_addr(intf);
	skb_push(skb, sizeof(struct tx_msdu_dscr) + offset);
	dscr = (struct tx_msdu_dscr *)(skb->data);
	dscr->common.type = type;
	dscr->pkt_len = cpu_to_le16(skb->len - (DSCR_LEN));
	dscr->offset = DSCR_LEN;
	dscr->tx_ctrl.checksum_offload = 1;
	dscr->tx_ctrl.checksum_type =
		ip_hdr(skb)->protocol == IPPROTO_TCP ? 1 : 0;
	dscr->tx_ctrl.sw_rate = (type == SPRDWL_TYPE_DATA ? 0 : 1);
	dscr->tx_ctrl.wds = 0;
	dscr->sta_lut_index = lut_index;
	dscr->tcp_udp_header_offset = 34;
	return 1;
}
#endif

int sprdwl_rx_fill_mbuf(struct mbuf_t *head, struct mbuf_t *tail, int num, int len)
{
	struct sprdwl_intf *intf = get_intf();
	int ret = 0, count = 0;
	struct mbuf_t *pos = NULL;

	for (pos = head, count = 0; count < num; count++) {
		wl_debug("%s: pos: %p\n", __func__, pos);
		pos->len = ALIGN(len, SMP_CACHE_BYTES);
		pos->buf = netdev_alloc_frag(pos->len);
		if (unlikely(!pos->buf)) {
			wl_err("%s: buffer error\n", __func__);
			ret = -ENOMEM;
			break;
		}

		if (SPRDWL_HW_SC2355_PCIE == intf->priv->hw_type)
			pos->phy = mm_virt_to_phys(&intf->pdev->dev, pos->buf,
					   pos->len, DMA_FROM_DEVICE);
		else if (SPRDWL_HW_SIPC == intf->priv->hw_type)
			pos->phy =  virt_to_phys(pos->buf) | SPRDWL_MH_ADDRESS_BIT;

		if (unlikely(!pos->phy)) {
			wl_err("%s: buffer error\n", __func__);
			ret = -ENOMEM;
			break;
		}
		pos = pos->next;
	}

	if (ret) {
		pos = head;
		while (count--) {
			sprdwl_free_data(pos->buf, SPRDWL_DEFRAG_MEM);
			pos = pos->next;
		}
	}

	return ret;
}

int sprdwl_rx_common_push(int chn, struct mbuf_t **head, struct mbuf_t **tail,
			  int *num, int len)
{
	int ret = 0;

	wl_err("%s: channel:%d head:%p tail:%p num:%d\n",
		 __func__, chn, head, tail, *num);

	if (0 == (*num))
		goto out;

	chn_rx_push[chn/2] += *num;

	ret = sprdwcn_bus_list_alloc(chn, head, tail, num);
	if (ret || head == NULL || tail == NULL || *head == NULL || *tail == NULL) {
		wl_err("%s:%d sprdwcn_bus_list_alloc fail\n", __func__, chn);
		ret = -ENOMEM;
		chn_rx_push_fail[chn/2] += *num;
	} else {
		ret = sprdwl_rx_fill_mbuf(*head, *tail, *num, len);
		if (ret) {
			wl_err("%s: %d alloc buf fail\n", __func__, chn);
			sprdwcn_bus_list_free(chn, *head, *tail, *num);
			*head = NULL;
			*tail = NULL;
			*num = 0;
			chn_rx_push_fail[chn/2] += *num;
		}
	}

out:
	return ret;
}

#ifdef PCIE_DEBUG
int sprdwl_mh_addr_seq = -1;
int sprdwl_mh_addr_hook(int chn, struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	int i, cur_seq;
	struct mbuf_t *mbuf;
	struct sprdwl_common_hdr *hdr;
	struct sprdwl_addr_trans *rxc_addrs;
	struct txc_addr_buff *txc_addrs;
	if (chn != 7)
		return 0;
	wl_info("pop:%d\n", num);
	for (i = 0, mbuf = head; i < num; i++) {
		hdr = (struct sprdwl_common_hdr *)(mbuf->buf);
		if (hdr->reserv) {
			rxc_addrs = (struct sprdwl_addr_trans *)(hdr+1);
			cur_seq = rxc_addrs->timestamp;
			printk(KERN_ERR "rxc:%d\n", cur_seq);
		} else {
			txc_addrs = (struct txc_addr_buff *)(mbuf->buf);
			cur_seq = txc_addrs->offset;
			printk(KERN_ERR "txc:%d\n", cur_seq);
		}

		mbuf = mbuf->next;
	}
	return 0;
}
#endif

#ifndef SC2355_RX_NO_LOOP
#ifdef PCIE_DEBUG
static int sprdwl_sc2355_rx_handle_for_debug(int chn, struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	struct sprdwl_intf *intf = get_intf();
	struct sprdwl_rx_if *rx_if = (struct sprdwl_rx_if *)intf->sprdwl_rx;
	struct sprdwl_msg_buf *msg = NULL;
	int buf_num = 0;//, ret = 0;
	struct mbuf_t *pos = head;

	wl_err("%s: channel:%d head:%p tail:%p num:%d\n",
		 __func__, chn, head, tail, num);

	sprdwl_mh_addr_hook(chn, head, tail, num);
	for (buf_num = num; buf_num > 0; buf_num--, pos = pos->next) {
		if (unlikely(!pos)) {
			wl_err("%s: NULL mbuf\n", __func__);
			break;
		}

		if (intf->priv->hw_type == SPRDWL_HW_SC2355_PCIE) {
			mm_phys_to_virt(&intf->pdev->dev, pos->phy, pos->len,
					DMA_FROM_DEVICE, false);
			pos->phy = 0;
		}

		/*Need FIX this in Marlin3*/
		if (sprdwl_sdio_process_credit(intf, pos->buf + 4))
			continue;

		msg = sprdwl_alloc_msg_buf(&rx_if->rx_list);
		if (!msg) {
			wl_err("%s: no msgbuf\n", __func__);
			sprdwl_free_data(pos->buf, SPRDWL_DEFRAG_MEM);
			pos->buf = NULL;
			continue;
		}

		sprdwl_fill_msg(msg, NULL, pos->buf, pos->len);

		/* SDIO would send public header to us because
		 * data is save in a buffer.
		 */
		msg->fifo_id = chn;
		msg->buffer_type = SPRDWL_DEFRAG_MEM;
		msg->data = msg->tran_data + intf->hif_offset;
		pos->buf = NULL;

		sprdwl_queue_msg_buf(msg, &rx_if->rx_list);
	}

	sprdwcn_bus_list_free(chn, head, tail, num);
	head = NULL;
	tail = NULL;
	num = 0;

#if 0
	/* We should refill mbuf in pcie mode */
	if (intf->priv->hw_type == SPRDWL_HW_SC2355_PCIE) {
		int len = 0;

		if (intf->rx_cmd_port == chn)
			len = SPRDWL_MAX_CMD_RXLEN;
		else
			len = SPRDWL_MAX_DATA_RXLEN;

		ret = sprdwl_rx_fill_mbuf(head, tail, num, len);
		if (ret) {
			wl_err("%s: alloc buf fail\n", __func__);
			sprdwcn_bus_list_free(chn, head, tail, num);
			head = NULL;
			tail = NULL;
			num = 0;
		}
	}

	if (!ret)
		mchn_push_link(chn, head, tail, num);
#endif

	rx_up(rx_if);

	return 0;
}
#endif

static int sprdwl_sc2355_rx_handle(int chn, struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	struct sprdwl_intf *intf = get_intf();
	struct sprdwl_rx_if *rx_if = (struct sprdwl_rx_if *)intf->sprdwl_rx;
	struct sprdwl_msg_buf *msg = NULL;
	int buf_num = 0, ret = 0;
	struct mbuf_t *pos = head;

	if (chn == PCIE_RX_ADDR_DATA_PORT &&
		(intf->priv->hw_type == SPRDWL_HW_SC2355_PCIE) &&
		intf->pushfail_count > 5)
		wl_warn("%s: channel:%d head:%p tail:%p num:%d\n",
			__func__, chn, head, tail, num);
	else
		wl_debug("%s: channel:%d head:%p tail:%p num:%d\n",
			__func__, chn, head, tail, num);

	for (buf_num = num; buf_num > 0; buf_num--, pos = pos->next) {
		if (unlikely(!pos)) {
			wl_err("%s: NULL mbuf\n", __func__);
			break;
		}

		if (intf->priv->hw_type == SPRDWL_HW_SC2355_PCIE) {
			mm_phys_to_virt(&intf->pdev->dev, pos->phy, pos->len,
					DMA_FROM_DEVICE, false);
			pos->phy = 0;
		} else if (intf->priv->hw_type == SPRDWL_HW_SIPC)
			pos->phy = 0;


		/*Need FIX this in Marlin3*/
		if (intf->priv->hw_type == SPRDWL_HW_SC2355_SDIO &&
			sprdwl_sdio_process_credit(intf, pos->buf + intf->hif_offset))
				continue;

		msg = sprdwl_alloc_msg_buf(&rx_if->rx_list);
		if (!msg) {
			wl_err("%s: no msgbuf\n", __func__);
			sprdwl_free_data(pos->buf, SPRDWL_DEFRAG_MEM);
			pos->buf = NULL;
			continue;
		}

		sprdwl_fill_msg(msg, NULL, pos->buf, pos->len);

		/* SDIO would send public header to us because
		 * data is save in a buffer.
		 */
		msg->fifo_id = chn;
		msg->buffer_type = SPRDWL_DEFRAG_MEM;
		msg->data = msg->tran_data + intf->hif_offset;
		//pos->buf = NULL;

		if (intf->priv->hw_type == SPRDWL_HW_SIPC) {
#ifdef SIPC_SUPPORT
			msg->buffer_type = SPRDWL_RSERVE_MEM;
			msg->tran_data = sipc_fill_mbuf(pos->buf, pos->len);
			msg->data = msg->tran_data;
#endif
		}
		sprdwl_queue_msg_buf(msg, &rx_if->rx_list);

		/*add this for debugging cmd timeout*/
#ifdef SIPC_SUPPORT
		if (SIPC_WIFI_CMD_RX == chn &&
			SPRDWL_TYPE_CMD == SPRDWL_HEAD_GET_TYPE(msg->data)) {
			wl_info("%s: channel:%d head:%p tail:%p num:%d\n",
			__func__, chn, head->buf, tail->buf, num);
			sprdwl_hex_dump("CMD", (unsigned char *)(msg->data), 12);
			wl_info("%s rsp_event_cnt %d\n", __func__, rx_if->rsp_event_cnt);
		}
#endif
	}

#if 0
	sprdwcn_bus_list_free(chn, head, tail, num);
	head = NULL;
	tail = NULL;
	num = 0;
#else
	/* We should refill mbuf in pcie mode */
	if (intf->priv->hw_type == SPRDWL_HW_SC2355_PCIE) {
		int len = 0;

		if (intf->rx_cmd_port == chn)
			len = SPRDWL_MAX_CMD_RXLEN;
		else
			len = SPRDWL_MAX_DATA_RXLEN;

		ret = sprdwl_rx_fill_mbuf(head, tail, num, len);
		if (ret) {
			wl_err("%s: alloc buf fail\n", __func__);
			sprdwcn_bus_list_free(chn, head, tail, num);
			head = NULL;
			tail = NULL;
			num = 0;
		}
	}

	if (!ret)
		sprdwcn_bus_push_list(chn, head, tail, num);
#endif

	rx_up(rx_if);

	return 0;
}
#else
inline void *sprdwl_get_rx_data(struct sprdwl_intf *intf,
				void *pos, void **data,
				void **tran_data, int *len, int offset)
{
	struct mbuf_t *mbuf = (struct mbuf_t *)pos;

	if (intf->priv->hw_type == SPRDWL_HW_SC2355_PCIE) {
		mm_phys_to_virt(&intf->pdev->dev, mbuf->phy, mbuf->len,
				DMA_FROM_DEVICE, false);
		mbuf->phy = 0;
	} else if (intf->priv->hw_type == SPRDWL_HW_SIPC)
		mbuf->phy = 0;

	*tran_data = mbuf->buf;
	*data = (*tran_data) + offset;
	*len = mbuf->len;
	mbuf->buf = NULL;

	return (void *)mbuf->next;
}

void sprdwl_free_rx_data(struct sprdwl_intf *intf,
			int chn, void *head, void *tail, int num)
{
	int len = 0, ret = 0;

	/* We should refill mbuf in pcie mode */
	if (intf->priv->hw_type == SPRDWL_HW_SC2355_PCIE ||
		intf->priv->hw_type == SPRDWL_HW_SIPC) {
		if (intf->rx_cmd_port == chn)
			len = SPRDWL_MAX_CMD_RXLEN;
		else
			len = SPRDWL_MAX_DATA_RXLEN;

		ret = sprdwl_rx_fill_mbuf(head, tail, num, len);
		if (ret) {
			wl_err("%s: alloc buf fail\n", __func__);
			sprdwcn_bus_list_free(chn, (struct mbuf_t *)head,
				       (struct mbuf_t *)tail, num);
			head = NULL;
			tail = NULL;
			num = 0;
		}
	}

	if (!ret)
		sprdwcn_bus_push_list(chn, (struct mbuf_t *)head, (struct mbuf_t *)tail, num);
		//mchn_push_link(chn, (struct mbuf_t *)head, (struct mbuf_t *)tail, num);
}

static int sprdwl_sc2355_rx_handle_no_loop(int chn, struct mbuf_t *head, struct mbuf_t *tail,
					   int num)
{
	struct sprdwl_intf *intf = get_intf();
	struct sprdwl_rx_if *rx_if = (struct sprdwl_rx_if *)intf->sprdwl_rx;
	struct sprdwl_msg_buf *msg = NULL;

	wl_err("%s: channel:%d head:%p tail:%p num:%d\n",
		 __func__, chn, head, tail, num);

	chn_rx_num[chn/2] += num;

	/* FIXME: Should we use replace msg? */
	msg = sprdwl_alloc_msg_buf(&rx_if->rx_list);
	if (!msg) {
		wl_err("%s: no msgbuf\n", __func__);
		//mchn_push_link(chn, head, tail, num);
		sprdwcn_bus_push_list(chn, head, tail, num);
		return 0;
	}

	sprdwl_fill_msg(msg, NULL, (void *)head, num);
	msg->fifo_id = chn;
	msg->buffer_type = SPRDWL_DEFRAG_MEM;
	msg->data = (void *)tail;

	sprdwl_queue_msg_buf(msg, &rx_if->rx_list);
	rx_up(rx_if);

	return 0;
}
#endif

#ifdef SC2355_RX_NAPI
static int sprdwl_sc2355_data_rx_handle(int chn, struct mbuf_t *head,
					struct mbuf_t *tail, int num)
{
	struct sprdwl_intf *intf = get_intf();
	struct sprdwl_rx_if *rx_if = (struct sprdwl_rx_if *)intf->sprdwl_rx;
	struct sprdwl_msg_buf *msg = NULL;

	wl_info("%s: channel:%d head:%p tail:%p num:%d\n",
		__func__, chn, head, tail, num);

	/* FIXME: Should we use replace msg? */
	msg = sprdwl_alloc_msg_buf(&rx_if->rx_data_list);
	if (!msg) {
		wl_err("%s: no msgbuf\n", __func__);
		sprdwcn_bus_push_list(chn, head, tail, num);
		return 0;
	}

	sprdwl_fill_msg(msg, NULL, (void *)head, num);
	msg->fifo_id = chn;
	msg->buffer_type = SPRDWL_DEFRAG_MEM;
	msg->data = (void *)tail;

	sprdwl_queue_msg_buf(msg, &rx_if->rx_data_list);
	napi_schedule(&rx_if->napi_rx);

	return 0;
}
#endif /* SC2355_RX_NO_LOOP */
#if 0
void sprdwl_handle_pop_list(void *data)
{
	int i;
	struct sprdwl_msg_buf *msg_pos;
	struct mbuf_t *mbuf_pos = NULL;
	struct sprdwl_pop_work *pop = (struct sprdwl_pop_work *)data;

	struct mbuf_pos = (struct mbuf_t *)pop->head;
	for (i = 0; i < pop->num; i++) {
		msg_pos = GET_MSG_BUF(mbuf_pos);
		/*TODO, check msg_buf after pop link*/
		if (msg_pos == NULL ||
		    !virt_addr_valid(msg_pos) ||
		    !virt_addr_valid(msg_pos->skb)) {
			wl_err("%s,%d, error! wrong sprdwl_msg_buf\n",
			       __func__, __LINE__);
			BUG_ON(1);
		}
		dev_kfree_skb(msg_pos->skb);
		/*delete node from to_free_list*/
		spin_lock_bh(&msg_pos->xmit_msg_list->free_lock);
		list_del(&msg_pos->list);
		spin_unlock_bh(&msg_pos->xmit_msg_list->free_lock);
		/*add it to free_list*/
		spin_lock_bh(&msg_pos->msglist->freelock);
		list_add_tail(&msg_pos->list, &msg_pos->msglist->freelist);
		spin_unlock_bh(&msg_pos->msglist->freelock);
		mbuf_pos = mbuf_pos->next;
	}
	sprdwcn_bus_list_free(pop->chn,
			      pop->head,
			      pop->tail,
			      pop->num);
}
#endif

int sprdwl_add_topop_list(int chn, struct mbuf_t *head,
			   struct mbuf_t *tail, int num)
{
	struct sprdwl_intf *intf = get_intf();
	struct sprdwl_work *misc_work;
	struct sprdwl_pop_work pop_work;

	pop_work.chn = chn;
	pop_work.head = (void *)head;
	pop_work.tail = (void *)tail;
	pop_work.num = num;

	misc_work = sprdwl_alloc_work(sizeof(struct sprdwl_pop_work));
	if (!misc_work) {
		wl_err("%s out of memory\n", __func__);
		return -1;
	}
	misc_work->vif = NULL;
	misc_work->id = SPRDWL_POP_MBUF;
	memcpy(misc_work->data, &pop_work, sizeof(struct sprdwl_pop_work));

	sprdwl_queue_work(intf->priv, misc_work);
	return 0;
}

/*call back func for HIF pop_link*/
int sprdwl_tx_data_pop_list(int channel, struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	struct mbuf_t *mbuf_pos = NULL;
#if defined(MORE_DEBUG)
	struct sprdwl_msg_buf *msg_head;
#endif
	struct sprdwl_intf *intf = get_intf();
	int tmp_num = num;
	struct sprdwl_tx_msg *tx_msg;

	tx_msg = (struct sprdwl_tx_msg *)intf->sprdwl_tx;

	wl_debug("%s channel: %d, head: %p, tail: %p num: %d\n",
		 __func__, channel, head, tail, num);

	if (intf->priv->hw_type == SPRDWL_HW_SC2355_PCIE) {
		/* FIXME: Temp solution, addr node pos hard to sync dma */
		for (mbuf_pos = head; mbuf_pos != NULL;
		mbuf_pos = mbuf_pos->next) {
			mm_phys_to_virt(&intf->pdev->dev, mbuf_pos->phy,
					mbuf_pos->len, DMA_TO_DEVICE, false);
			mbuf_pos->phy = 0;
			if (mbuf_pos->buf != NULL) {
				kfree(mbuf_pos->buf);
				mbuf_pos->buf = NULL;
			} else
				wl_err("%s:error! mbuf_pos->buf is NULL!\n", __func__);
			if (--tmp_num == 0)
				break;
		}
		sprdwcn_bus_list_free(channel, head, tail, num);
		/*if (channel == 6)
			atomic_dec(&send_flag);*/
		wl_debug("%s:%d free : %d msg buf\n", __func__, __LINE__, num);
		return 0;
	} else if (intf->priv->hw_type == SPRDWL_HW_SIPC) {
		for (mbuf_pos = head; mbuf_pos != NULL; mbuf_pos = mbuf_pos->next) {
			mbuf_pos->phy = 0;
			if (mbuf_pos->buf != NULL) {
				kfree(mbuf_pos->buf);
				mbuf_pos->buf = NULL;
			} else
				wl_err("%s:error! mbuf_pos->buf is NULL!\n", __func__);
			if (--tmp_num == 0)
				break;
		}
		sprdwcn_bus_list_free(channel, head, tail, num);
		wl_debug("%s:%d free : %d msg buf\n", __func__, __LINE__, num);
		return 0;
	}

#if defined(MORE_DEBUG)
	msg_head = GET_MSG_BUF(head);
	/*show packet average sent time, unit: ns*/
	sprdwl_get_tx_avg_time(intf, msg_head->tx_start_time);
#endif
	sprdwl_dequeue_data_list(head, num);
	wl_debug("%s:%d free : %d msg buf\n", __func__, __LINE__, num);
	sprdwcn_bus_list_free(channel, head, tail, num);

	return 0;
}

void sprdwl_tx_free_pcie_data_num(struct sprdwl_intf *dev, unsigned char *data)
{
	struct sprdwl_tx_msg *tx_msg;
	unsigned short data_num;
	struct txc_addr_buff *txc_addr;

	tx_msg = (struct sprdwl_tx_msg *)dev->sprdwl_tx;
	txc_addr = (struct txc_addr_buff *)data;

	data_num = txc_addr->number;
	atomic_sub(data_num, &tx_msg->xmit_msg_list.free_num);

	tx_up(tx_msg);
}

static inline int sprdwl_tx_free_txc_msg(struct sprdwl_tx_msg *tx_msg,
					 struct sprdwl_msg_buf *msg_buf)
{
	struct sprdwl_msg_buf *pos_msg = NULL;
	unsigned long lockflag_txc = 0;
	int found = 0;
	enum sprdwl_hw_type hw_type = tx_msg->intf->priv->hw_type;

	spin_lock_irqsave(&tx_msg->xmit_msg_list.free_lock, lockflag_txc);
	list_for_each_entry(pos_msg, &tx_msg->xmit_msg_list.to_free_list,
			    list) {
		if (pos_msg == msg_buf) {
			found = 1;
			break;
		}
	}

	if (found != 1) {
		wl_err("%s: msg_buf %lx not in to free list\n",
			__func__, (unsigned long int)msg_buf);
		spin_unlock_irqrestore(&tx_msg->xmit_msg_list.free_lock,
				       lockflag_txc);
		return -1;
	}
	list_del(&msg_buf->list);
	spin_unlock_irqrestore(&tx_msg->xmit_msg_list.free_lock, lockflag_txc);

	if (SPRDWL_HW_SIPC == hw_type) {
#ifdef SIPC_SUPPORT
		if (msg_buf->sipc_node) {
			sipc_free_tx_buf(tx_msg->intf, msg_buf->sipc_node);
			msg_buf->sipc_node = NULL;
		}
#endif
	} else if (SPRDWL_HW_SC2355_PCIE == hw_type) {
		if (msg_buf->node) {
			sprdwl_free_tx_buf(msg_buf->node);
			msg_buf->node = NULL;
		}
	}
	if (msg_buf->skb)
		dev_kfree_skb(msg_buf->skb);

	msg_buf->skb = NULL;
	msg_buf->data = NULL;
	msg_buf->tran_data = NULL;
	msg_buf->len = 0;

	sprdwl_free_msg_buf(msg_buf, msg_buf->msglist);
	return 0;
}

int sprdwl_tx_free_pcie_data(struct sprdwl_priv *priv, unsigned char *data)
{
	int i;
	struct sprdwl_intf *dev = (struct sprdwl_intf *)priv->hw_priv;
	struct sprdwl_tx_msg *tx_msg = (struct sprdwl_tx_msg *)dev->sprdwl_tx;
	void *data_addr_ptr = NULL;
	unsigned long long pcie_addr = 0;
	unsigned short	data_num;
	struct txc_addr_buff *txc_addr;
	unsigned char (*pos)[5];
	struct sprdwl_msg_buf *msg_buf, *last_msg_buf = NULL;
#if defined(MORE_DEBUG)
	unsigned long tx_start_time = 0;
#endif
#ifdef SIPC_SUPPORT
	unsigned long phy_addr;
	struct sipc_buf_node *node = NULL;
#endif

	static unsigned long caller_jiffies;

	if (tx_msg->net_stoped == 1) {
		sprdwl_net_flowcontrl(priv, SPRDWL_MODE_NONE, true);
		tx_msg->net_stoped = 0;
	}

	txc_addr = (struct txc_addr_buff *)data;
	data_num = txc_addr->number;
	tx_msg->txc_num += data_num;
	wl_debug("%s: seq_num=0x%x", __func__, txc_addr->offset);
	wl_info("%s, total free num:%lu; total tx_num:%lu\n", __func__,
		 tx_msg->txc_num, tx_msg->tx_num);
	if (printk_timed_ratelimit(&caller_jiffies, 1000)) {
		wl_info("%s, free_num: %d, to_free_list num: %d\n",
			__func__, data_num,
			atomic_read(&tx_msg->xmit_msg_list.free_num));
	}

	pos = (unsigned char (*)[5])(txc_addr + 1);
	for (i = 0; i < data_num; i++, pos++) {
		memcpy(&pcie_addr, pos, SPRDWL_PHYS_LEN);
		pcie_addr -= 0x10; //Workaround for HW issue
		if (SPRDWL_HW_SC2355_PCIE == dev->priv->hw_type) {

		clear_cp_pcie_addr(pcie_addr);
		wl_debug("%s: pcie_addr=0x%llx", __func__, pcie_addr);
		data_addr_ptr = mm_phys_to_virt(&dev->pdev->dev, pcie_addr,
						SPRDWL_MAX_DATA_TXLEN,
						DMA_TO_DEVICE, false);
		} else if (SPRDWL_HW_SIPC == dev->priv->hw_type) {
#ifdef SIPC_SUPPORT
			phy_addr = pcie_addr & (~(SPRDWL_MH_ADDRESS_BIT) & SPRDWL_PHYS_MASK);
			phy_addr = phy_addr | SPRDWL_MH_SIPC_ADDRESS_BASE;
			data_addr_ptr = (void *)(phy_addr + dev->sipc_mm->tx_buf->offset);
#endif
		}
		msg_buf = NULL;
		if (SPRDWL_HW_SC2355_PCIE == dev->priv->hw_type)
			RESTORE_ADDR(msg_buf, data_addr_ptr, sizeof(struct sprdwl_msg_buf *));
		else if (SPRDWL_HW_SIPC == dev->priv->hw_type) {
#ifdef SIPC_SUPPORT
			memcpy_fromio(&node, (char *)data_addr_ptr + SPRDWL_MAX_DATA_TXLEN, sizeof(node));
			if (node != NULL) {
				msg_buf = node->priv;
			} else {
				wl_err("%s, node already is NULL\n", __func__);
				continue;

			}
#endif
		}
		if (last_msg_buf == msg_buf) {
			wl_info("%s: same msg buf: %lx, %lx\n", __func__,
				 (unsigned long)msg_buf, (unsigned long)last_msg_buf);
		}
		wl_debug("data_addr_ptr: 0x%lx, msg_buf: 0x:%lx\n",
			  (unsigned long int)data_addr_ptr, (unsigned long int)msg_buf);
#if defined(MORE_DEBUG)
		if (i == 0)
			tx_start_time = msg_buf->tx_start_time;
#endif

		if (!sprdwl_tx_free_txc_msg(tx_msg, msg_buf))
			last_msg_buf = msg_buf;
	}

#if defined(MORE_DEBUG)
	sprdwl_get_tx_avg_time(dev, tx_start_time);
#endif

	return 0;
}

int sprdwl_tx_cmd_pop_list(int channel, struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	int count = 0;
	struct mbuf_t *pos = NULL;
	struct sprdwl_intf *intf = get_intf();
	struct sprdwl_tx_msg *tx_msg;
	struct sprdwl_msg_buf *pos_buf, *temp_buf;

	tx_msg = (struct sprdwl_tx_msg *)intf->sprdwl_tx;
	tx_msg->cmd_poped += num;

	wl_info("%s,head:%p,tail:%p,num:%d,cmd_poped%d,cmd_send%d\n",
		 __func__, head, tail, num,
		 tx_msg->cmd_poped, tx_msg->cmd_send);

	pos = head;

	list_for_each_entry_safe(pos_buf, temp_buf,
				 &tx_msg->tx_list_cmd.cmd_to_free, list) {
		if (pos_buf->tran_data == pos->buf) {
			wl_debug("move CMD node from to_free to free list\n");
			/*list msg_buf from to_free list  to free list*/
			sprdwl_free_cmd_buf(pos_buf, &tx_msg->tx_list_cmd);

			if (intf->priv->hw_type == SPRDWL_HW_SC2355_PCIE) {
				mm_phys_to_virt(&intf->pdev->dev, pos->phy,
						pos->len, DMA_TO_DEVICE, false);
				pos->phy = 0;
			}
			/*free it*/
			kfree(pos->buf);
			pos->buf = NULL;
			pos = pos->next;
			count++;
		}
		if (count == num)
			break;
	}

	sprdwcn_bus_list_free(channel, head, tail, num);

	return 0;
}

int sprdwl_rx_cmd_push(int chn, struct mbuf_t **head, struct mbuf_t **tail, int *num)
{
	return sprdwl_rx_common_push(chn, head, tail,
				     num, SPRDWL_MAX_CMD_RXLEN);
}

int sprdwl_rx_data_push(int chn, struct mbuf_t **head, struct mbuf_t **tail, int *num)
{
	return sprdwl_rx_common_push(chn, head, tail,
				     num, SPRDWL_MAX_DATA_RXLEN);
}

/*
 * mode:
 * 0 - suspend
 * 1 - resume
 */
int sprdwl_suspend_resume_handle(int chn, int mode)
{
	struct sprdwl_intf *intf = get_intf();
	struct sprdwl_priv *priv = intf->priv;
	struct sprdwl_tx_msg *tx_msg = (struct sprdwl_tx_msg *)intf->sprdwl_tx;
	int ret;
	struct sprdwl_vif *vif;
	struct timespec time;
	enum sprdwl_mode sprdwl_mode = SPRDWL_MODE_STATION;
	u8 mode_found = 0;

	wl_info("%s, %d, mode:%d\n", __func__, __LINE__, mode);

	for (sprdwl_mode = SPRDWL_MODE_STATION; sprdwl_mode < SPRDWL_MODE_MAX; sprdwl_mode++) {
		if (priv->fw_stat[sprdwl_mode] == SPRDWL_INTF_OPEN) {
			mode_found = 1;
			break;
		}
	}

	if (0 == mode_found)
		return -EBUSY;

	vif = mode_to_vif(priv, sprdwl_mode);

	if (vif == NULL || intf->cp_asserted) {
		wl_err("%s, %d, error! NULL vif or assert\n", __func__, __LINE__);
		sprdwl_put_vif(vif);
		return -EBUSY;
	}

	if (mode == 0) {
		if (atomic_read(&tx_msg->tx_list_qos_pool.ref) > 0 ||
			atomic_read(&tx_msg->tx_list_cmd.ref) > 0 ||
			!list_empty(&tx_msg->xmit_msg_list.to_send_list) ||
			!list_empty(&tx_msg->xmit_msg_list.to_free_list)) {
			wl_info("%s, %d,Q not empty suspend not allowed\n",
				__func__, __LINE__);
			sprdwl_put_vif(vif);
			return -EBUSY;
		}
		priv->wakeup_tracer.resume_flag = 0;
		intf->suspend_mode = SPRDWL_PS_SUSPENDING;
		getnstimeofday(&time);
		intf->sleep_time = timespec_to_ns(&time);
		priv->is_suspending = 1;
		ret = sprdwl_power_save(priv,
					vif->ctx_id,
					SPRDWL_SUSPEND_RESUME,
					0);
		if (ret == 0)
			intf->suspend_mode = SPRDWL_PS_SUSPENDED;
		else
			intf->suspend_mode = SPRDWL_PS_RESUMED;
		sprdwl_put_vif(vif);
		return ret;
	} else if (mode == 1) {
		intf->suspend_mode = SPRDWL_PS_RESUMING;
		priv->wakeup_tracer.resume_flag = 1;
		getnstimeofday(&time);
		intf->sleep_time = timespec_to_ns(&time) - intf->sleep_time;
		ret = sprdwl_power_save(priv,
					vif->ctx_id,
					SPRDWL_SUSPEND_RESUME,
					1);
		wl_info("%s, %d,resume ret=%d, resume after %lu ms\n",
			__func__, __LINE__,
			ret, intf->sleep_time/1000000);
		sprdwl_put_vif(vif);
		return ret;
	}
	sprdwl_put_vif(vif);
	return -EBUSY;
}

/*  SDIO TX:
 *  Type 3:WIFI
 *  Subtype 0  --> port 8
 *  Subtype 1  --> port 9
 *  Subtype 2  --> port 10(fifolen=8)
 *  Subtype 3  --> port 11(fifolen=8)
 *  SDIO RX:
 *  Type 3:WIFI
 *  Subtype 0  --> port 10
 *  Subtype 1  --> port 11
 *  Subtype 2  --> port 12(fifolen=8)
 *  Subtype 3  --> port 13(fifolen=8)
 */
struct mchn_ops_t pcie_hif_ops[] = {
	/* ADD HIF ops config here */
	/* NOTE: Requested by SDIO team, pool_size MUST be 1 in RX */
	/* RX channels 1 3 5 7 9 11 13 15 */
#ifndef SC2355_RX_NO_LOOP
	INIT_INTF_SC2355(PCIE_RX_CMD_PORT, 1, 0, 0, SPRDWL_MAX_CMD_RXLEN,
			10, 0, 0, 0, 1, 32, sprdwl_sc2355_rx_handle,
			sprdwl_rx_cmd_push, NULL, NULL),
	INIT_INTF_SC2355(PCIE_RX_DATA_PORT, 1, 0, 0, SPRDWL_MAX_CMD_RXLEN,
			100, 0, 0, 0, 1, 32, sprdwl_sc2355_rx_handle,
			sprdwl_rx_data_push, NULL, NULL),
#ifdef PCIE_DEBUG
	INIT_INTF_SC2355(PCIE_RX_ADDR_DATA_PORT, 1, 0, 0, SPRDWL_MAX_CMD_RXLEN,
			100, 0, 0, 0, 1, 32, sprdwl_sc2355_rx_handle_for_debug,
			sprdwl_rx_data_push, NULL, NULL),
#else
	INIT_INTF_SC2355(PCIE_RX_ADDR_DATA_PORT, 1, 0, 0, SPRDWL_MAX_CMD_RXLEN,
			100, 0, 0, 0, 1, 32, sprdwl_sc2355_rx_handle,
			sprdwl_rx_data_push, NULL, NULL),
#endif
#else
	INIT_INTF_SC2355(PCIE_RX_CMD_PORT, 1, 0, 0, SPRDWL_MAX_CMD_RXLEN,
			10, 0, 0, 0, 1, 32, sprdwl_sc2355_rx_handle_no_loop,
			sprdwl_rx_cmd_push, NULL, NULL),
	INIT_INTF_SC2355(PCIE_RX_DATA_PORT, 1, 0, 0, SPRDWL_MAX_CMD_RXLEN,
			100, 0, 0, 0, 1, 32, sprdwl_sc2355_rx_handle_no_loop,
			sprdwl_rx_data_push, NULL, NULL),
	INIT_INTF_SC2355(PCIE_RX_ADDR_DATA_PORT, 1, 0, 0, SPRDWL_MAX_CMD_RXLEN,
			100, 0, 0, 0, 1, 32, sprdwl_sc2355_rx_handle_no_loop,
			sprdwl_rx_data_push, NULL, NULL),
#endif
	/* TX channels 0 2 4 6 8 10 12 14 */
	INIT_INTF_SC2355(PCIE_TX_CMD_PORT, 1, 1, 0, SPRDWL_MAX_CMD_TXLEN,
			10, 0, 0, 0, 1, 32, sprdwl_tx_cmd_pop_list,
			NULL, NULL, sprdwl_suspend_resume_handle),
	INIT_INTF_SC2355(PCIE_TX_DATA_PORT, 1, 1, 0, SPRDWL_MAX_CMD_TXLEN,
			300, 0, 0, 0, 1, 32, sprdwl_tx_data_pop_list,
			NULL, NULL, NULL),
	INIT_INTF_SC2355(PCIE_TX_ADDR_DATA_PORT, 1, 1, 0, SPRDWL_MAX_CMD_TXLEN,
			300, 0, 0, 0, 1, 4, sprdwl_tx_data_pop_list,
			NULL, NULL, NULL),
};

#ifdef SIPC_SUPPORT
struct mchn_ops_t sipc_hif_ops[] = {
	/* RX channels */
	INIT_INTF_SC2355(SIPC_WIFI_CMD_RX, 2, 0, 0, SPRDWL_MAX_CMD_RXLEN,
		64, 0, 0, 0, 1, 32, sprdwl_sc2355_rx_handle,
			sprdwl_rx_cmd_push, NULL, NULL),
	INIT_INTF_SC2355(SIPC_WIFI_DATA0_RX, 2, 0, 0, SPRDWL_MAX_CMD_RXLEN,
		64, 0, 0, 0, 1, 32, sprdwl_sc2355_rx_handle,
		sprdwl_rx_data_push, NULL, NULL),

	INIT_INTF_SC2355(SIPC_WIFI_DATA1_RX, 2, 0, 0, SPRDWL_MAX_CMD_RXLEN,
		64, 0, 0, 0, 1, 32, sprdwl_sc2355_rx_handle,
		sprdwl_rx_data_push, NULL, NULL),

	/* TX channels */
	INIT_INTF_SC2355(SIPC_WIFI_CMD_TX, 2, 1, 0, SPRDWL_MAX_CMD_TXLEN,
			64, 0, 0, 0, 1, 32, sprdwl_tx_cmd_pop_list,
			NULL, NULL, sprdwl_suspend_resume_handle),
	INIT_INTF_SC2355(SIPC_WIFI_DATA0_TX, 1, 1, 0, SPRDWL_MAX_CMD_TXLEN,
			64, 0, 0, 0, 1, 32, sprdwl_tx_data_pop_list,
			NULL, NULL, NULL),
	INIT_INTF_SC2355(SIPC_WIFI_DATA1_TX, 1, 1, 0, SPRDWL_MAX_CMD_TXLEN,
			64, 0, 0, 0, 1, 4, sprdwl_tx_data_pop_list,
			NULL, NULL, NULL),
};
#endif

struct sprdwl_peer_entry
*sprdwl_find_peer_entry_using_lut_index(struct sprdwl_intf *intf,
					unsigned char sta_lut_index)
{
	int i = 0;
	struct sprdwl_peer_entry *peer_entry = NULL;

	for (i = 0; i < MAX_LUT_NUM; i++) {
		if (sta_lut_index == intf->peer_entry[i].lut_index) {
			peer_entry = &intf->peer_entry[i];
			break;
		}
	}

	return peer_entry;
}

/* update lut-inidex if event_sta_lut received
 * at CP side, lut_index range 0-31
 * but 0-3 were used to send non-assoc frame(only used by CP)
 * so for Ap-CP interface, there is only 4-31
*/
void sprdwl_event_sta_lut(struct sprdwl_vif *vif, u8 *data, u16 len)
{
	struct sprdwl_intf *intf;
	struct sprdwl_sta_lut_ind *sta_lut = NULL;
	u8 i;

	if (len < sizeof(*sta_lut)) {
		wl_err("%s, len:%d too short!\n", __func__, len);
		return;
	}
	intf = (struct sprdwl_intf *)vif->priv->hw_priv;
	sta_lut = (struct sprdwl_sta_lut_ind *)data;
	if (intf != get_intf()) {
		wl_err("%s, wrong intf!\n", __func__);
		return;
	}
	if (sta_lut == NULL) {
		wl_err("%s, NULL input data!\n", __func__);
		return;
	}

	i = sta_lut->sta_lut_index;

	wl_info("ctx_id:%d,action:%d,lut:%d\n", sta_lut->ctx_id,
		sta_lut->action, sta_lut->sta_lut_index);
	switch (sta_lut->action) {
	case DEL_LUT_INDEX:
		if (intf->peer_entry[i].ba_tx_done_map != 0) {
			intf->peer_entry[i].ht_enable = 0;
			intf->peer_entry[i].ip_acquired = 0;
			intf->peer_entry[i].ba_tx_done_map = 0;
			/*sprdwl_tx_delba(intf, intf->peer_entry + i);*/
		}
		peer_entry_delba((void *)intf, i);
		memset(&intf->peer_entry[i], 0x00,
		       sizeof(struct sprdwl_peer_entry));
		intf->peer_entry[i].ctx_id = 0xFF;
		intf->tx_num[i] = 0;
		sprdwl_dis_flush_txlist(intf, i);
		break;
	case UPD_LUT_INDEX:
		peer_entry_delba((void *)intf, i);
		sprdwl_dis_flush_txlist(intf, i);
		fallthrough;
	case ADD_LUT_INDEX:
		intf->peer_entry[i].lut_index = i;
		intf->peer_entry[i].ctx_id = sta_lut->ctx_id;
		intf->peer_entry[i].ht_enable = sta_lut->is_ht_enable;
		intf->peer_entry[i].vht_enable = sta_lut->is_vht_enable;
		intf->peer_entry[i].ba_tx_done_map = 0;
		intf->tx_num[i] = 0;

		wl_info("ctx_id%d,action%d,lut%d,%x:%x:%x:%x:%x:%x\n",
			sta_lut->ctx_id, sta_lut->action,
			sta_lut->sta_lut_index,
			sta_lut->ra[0], sta_lut->ra[1], sta_lut->ra[2],
			sta_lut->ra[3], sta_lut->ra[4], sta_lut->ra[5]);
		ether_addr_copy(intf->peer_entry[i].tx.da, sta_lut->ra);
		break;
	default:
		break;
	}
}

void sprdwl_tx_ba_mgmt(struct sprdwl_priv *priv, void *data, int len,
		       unsigned char cmd_id, unsigned char ctx_id)
{
	struct sprdwl_msg_buf *msg;
	unsigned char *data_ptr;
	u8 *rbuf;
	u16 rlen = (1 + sizeof(struct host_addba_param));

	msg = sprdwl_cmd_getbuf(priv, len, ctx_id, SPRDWL_HEAD_RSP,
				cmd_id);
	if (!msg) {
		wl_err("%s, %d, get msg err\n", __func__, __LINE__);
		return;
	}
	rbuf = kzalloc(rlen, GFP_KERNEL);
	if (!rbuf) {
		wl_err("%s, %d, alloc rbuf err\n", __func__, __LINE__);
		return;
	}
	memcpy(msg->data, data, len);
	data_ptr = (unsigned char *)data;

	if (sprdwl_debug_level >= L_INFO)
		sprdwl_hex_dump("sprdwl_tx_ba_mgmt", data_ptr, len);

	if (sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, rbuf, &rlen))
		goto out;
	/*if tx ba req failed, need to clear txba map*/
	if (cmd_id == WIFI_CMD_ADDBA_REQ &&
		rbuf[0] != ADDBA_REQ_RESULT_SUCCESS) {
		struct host_addba_param *addba;
		struct sprdwl_peer_entry *peer_entry = NULL;
		struct sprdwl_intf *intf = get_intf();
		u16 tid = 0;

		addba = (struct host_addba_param *)(rbuf + 1);
		peer_entry = &intf->peer_entry[addba->lut_index];
		tid = addba->addba_param.tid;
		if (!test_and_clear_bit(tid, &peer_entry->ba_tx_done_map))
			goto out;
		wl_err("%s, %d, tx_addba failed, reason=%d, lut_index=%d, tid=%d, map=%lu\n",
		       __func__, __LINE__,
		       rbuf[0],
		       addba->lut_index,
		       tid,
		       peer_entry->ba_tx_done_map);
	}
out:
	kfree(rbuf);
}

void sprdwl_tx_send_addba(struct sprdwl_vif *vif, void *data, int len)
{
	sprdwl_tx_ba_mgmt(vif->priv, data, len, WIFI_CMD_ADDBA_REQ,
			  vif->ctx_id);
}

void sprdwl_tx_send_delba(struct sprdwl_vif *vif, void *data, int len)
{
	struct host_delba_param *delba;

	delba = (struct host_delba_param *)data;
	sprdwl_tx_ba_mgmt(vif->priv, delba, sizeof(struct host_delba_param),
			  WIFI_CMD_DELBA_REQ, vif->ctx_id);
}

void sprdwl_tx_addba(struct sprdwl_intf *intf,
		     struct sprdwl_peer_entry *peer_entry, unsigned char tid)
{
#define WIN_SIZE 64
	struct host_addba_param addba;
	struct sprdwl_work *misc_work;
	struct sprdwl_vif *vif;

	vif = ctx_id_to_vif(intf->priv, peer_entry->ctx_id);
	if (!vif)
		return;
	memset(&addba, 0x0, sizeof(struct host_addba_param));

	addba.lut_index = peer_entry->lut_index;
	ether_addr_copy(addba.perr_mac_addr, peer_entry->tx.da);
	wl_info("%s, lut=%d, tid=%d\n", __func__, peer_entry->lut_index, tid);
	addba.dialog_token = 1;
	addba.addba_param.amsdu_permit = 0;
	addba.addba_param.ba_policy = DOT11_ADDBA_POLICY_IMMEDIATE;
	addba.addba_param.tid = tid;
	addba.addba_param.buffer_size = WIN_SIZE;
	misc_work = sprdwl_alloc_work(sizeof(struct host_addba_param));
	if (!misc_work) {
		wl_err("%s out of memory\n", __func__);
		sprdwl_put_vif(vif);
		return;
	}
	misc_work->vif = vif;
	misc_work->id = SPRDWL_WORK_ADDBA;
	memcpy(misc_work->data, &addba, sizeof(struct host_addba_param));

	sprdwl_queue_work(vif->priv, misc_work);
	sprdwl_put_vif(vif);
}

void sprdwl_tx_delba(struct sprdwl_intf *intf,
		     struct sprdwl_peer_entry *peer_entry,  unsigned int ac_index)
{
	struct host_delba_param delba;
	struct sprdwl_work *misc_work;
	struct sprdwl_vif *vif;

	vif = ctx_id_to_vif(intf->priv, peer_entry->ctx_id);
	if (!vif)
		return;
	memset(&delba, 0x0, sizeof(delba));

	wl_debug("enter--at %s\n", __func__);
	ether_addr_copy(delba.perr_mac_addr, peer_entry->tx.da);
	delba.lut_index = peer_entry->lut_index;
	delba.delba_param.initiator = 1;
	delba.delba_param.tid = qos_index_2_tid(ac_index);
	delba.reason_code = 0;


	misc_work =
	sprdwl_alloc_work(sizeof(struct host_delba_param));
	if (!misc_work) {
		wl_err("%s out of memory\n", __func__);
		sprdwl_put_vif(vif);
		return;
	}
	misc_work->vif = vif;
	misc_work->id = SPRDWL_WORK_DELBA;
	memcpy(misc_work->data, &delba, sizeof(struct host_delba_param));
	clear_bit(qos_index_2_tid(ac_index), &peer_entry->ba_tx_done_map);

	sprdwl_queue_work(vif->priv, misc_work);
	sprdwl_put_vif(vif);
}

void sprdwl_count_rx_tp(struct sprdwl_intf *intf, int len)
{
	unsigned long long timeus = 0;
	struct sprdwl_rx_if *rx_if = (struct sprdwl_rx_if *)intf->sprdwl_rx;

	rx_if->rx_total_len += len;
	if (rx_if->rx_total_len == len) {
		rx_if->rxtimebegin = ktime_get();
		return;
	}

	rx_if->rxtimeend = ktime_get();
	timeus = div_u64(rx_if->rxtimeend - rx_if->rxtimebegin, NSEC_PER_USEC);
	if (div_u64((rx_if->rx_total_len * 8), timeus) >= intf->tcpack_delay_th_in_mb &&
		timeus > intf->tcpack_time_in_ms * USEC_PER_MSEC) {
		rx_if->rx_total_len = 0;
		enable_tcp_ack_delay("tcpack_delay_en=1", strlen("tcpack_delay_en="));
	} else if (div_u64((rx_if->rx_total_len * 8), timeus) < intf->tcpack_delay_th_in_mb &&
		timeus > intf->tcpack_time_in_ms * USEC_PER_MSEC) {
		rx_if->rx_total_len = 0;
		enable_tcp_ack_delay("tcpack_delay_en=0", strlen("tcpack_delay_en="));
	}
}

void sprdwl_sipc_init(struct sprdwl_priv *priv, struct sprdwl_intf *intf)
{

#ifdef RTT_SUPPORT
	int i;
#endif

	if (SPRDWL_HW_SC2355_PCIE == priv->hw_type) {
		g_intf_sc2355.hif_ops = pcie_hif_ops;
		g_intf_sc2355.max_num = sizeof(pcie_hif_ops)/sizeof(struct mchn_ops_t);
	} else if (SPRDWL_HW_SIPC == priv->hw_type) {
#ifdef SIPC_SUPPORT
		g_intf_sc2355.hif_ops = sipc_hif_ops;
		g_intf_sc2355.max_num = sizeof(sipc_hif_ops)/sizeof(struct mchn_ops_t);
#endif
	}
	g_intf_sc2355.intf = (void *)intf;
	g_intf = intf;
	/* TODO: Need we reserve g_intf_sc2355? */
	intf->hw_intf = (void *)&g_intf_sc2355;
	priv->hw_priv = intf;
	priv->hw_offset = intf->hif_offset;
	intf->priv = priv;
	spin_lock_init(&intf->l1ss_lock);
	intf->tsq_shift = 7;
	intf->tcpack_time_in_ms = RX_TP_COUNT_IN_MS;
	intf->tcpack_delay_th_in_mb = DROPACK_TP_TH_IN_M;

#ifdef RTT_SUPPORT
	for (i = 0; i < 10; i++)
		priv->rtt_results.peer_rtt_result[i] =  kzalloc(2 * sizeof(struct wifi_hal_rtt_result), GFP_KERNEL);
#endif

}


void sprdwl_sipc_deinit(struct sprdwl_intf *intf)
{
#ifdef RTT_SUPPORT
	int i;

	for (i = 0; i < 10; i++)
		kfree(intf->priv->rtt_results.peer_rtt_result[i]);
#endif

	g_intf_sc2355.intf = NULL;
	g_intf_sc2355.max_num = 0;
	intf->hw_intf = NULL;
	g_intf = NULL;
}

int sprdwl_intf_init(struct sprdwl_intf *intf)
{
	int ret = -EINVAL, chn = 0;

	if (g_intf_sc2355.max_num < MAX_CHN_NUM) {
		for (chn = 0; chn < g_intf_sc2355.max_num; chn++) {
			ret = sprdwcn_bus_chn_init(&g_intf_sc2355.hif_ops[chn]);
			if (ret < 0)
				goto err;
		}

		intf->fw_awake = 1;
		intf->fw_power_down = 0;
	} else {
err:
		for (; chn > 0; chn--)
			sprdwcn_bus_chn_deinit(&g_intf_sc2355.hif_ops[chn]);

		g_intf_sc2355.hif_ops = NULL;
		g_intf_sc2355.max_num = 0;
	}

	return ret;
}

void sprdwl_intf_deinit(void)
{
	int chn = 0;

	for (chn = 0; chn < g_intf_sc2355.max_num; chn++)
		sprdwcn_bus_chn_deinit(&g_intf_sc2355.hif_ops[chn]);
}

int sprdwl_dis_flush_txlist(struct sprdwl_intf *intf, u8 lut_index)
{
	struct sprdwl_tx_msg *tx_msg;
	int i, j;

	if (lut_index <= 5) {
		wl_err("err lut_index:%d, %s, %d\n",
				lut_index, __func__, __LINE__);
		return -1;
	}
	wl_err("disconnect, flush qoslist, %s, %d\n", __func__, __LINE__);
	tx_msg = (struct sprdwl_tx_msg *)intf->sprdwl_tx;
	for (i = 0; i < SPRDWL_MODE_MAX; i++)
		for (j = 0; j < SPRDWL_AC_MAX; j++)
				sprdwl_flush_tx_qoslist(tx_msg, i, j, lut_index);
	return 0;
}

