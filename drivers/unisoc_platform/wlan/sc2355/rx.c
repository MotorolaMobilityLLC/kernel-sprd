// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright © 2021 Unisoc Technologies Co., Ltd.
 * <https://www.unisoc.com>
 */

#include <net/ip6_checksum.h>

#include "cmdevt.h"
#include "common/common.h"
#include "common/debug.h"
#include "common/delay_work.h"
#include "common/msg.h"
#include "rx.h"
#include "txrx.h"

static bool rx_mh_ipv6_ext_hdr(unsigned char nexthdr)
{
	return (nexthdr == NEXTHDR_HOP ||
		nexthdr == NEXTHDR_ROUTING || nexthdr == NEXTHDR_DEST);
}

static int rx_ipv6_csum(void *data, __wsum csum)
{
	int ret = 0;
	struct rx_msdu_desc *msdu_desc = (struct rx_msdu_desc *)data;
	struct ethhdr *eth = (struct ethhdr *)(data + msdu_desc->msdu_offset);
	struct ipv6hdr *ip6h = NULL;
	struct ipv6_opt_hdr *hp = NULL;
	unsigned short dataoff = ETH_HLEN;
	unsigned short nexthdr = 0;

	pr_debug("%s: eth_type: 0x%x\n", __func__, eth->h_proto);

	if (eth->h_proto == cpu_to_be16(ETH_P_IPV6)) {
		data += msdu_desc->msdu_offset;
		ip6h = data + dataoff;
		nexthdr = ip6h->nexthdr;
		dataoff += sizeof(*ip6h);

		while (rx_mh_ipv6_ext_hdr(nexthdr)) {
			pr_debug("%s: nexthdr: %d\n", __func__, nexthdr);
			hp = (struct ipv6_opt_hdr *)(data + dataoff);
			dataoff += ipv6_optlen(hp);
			nexthdr = hp->nexthdr;
		}

		pr_debug("%s: nexthdr: %d, dataoff: %d, len: %d\n",
			 __func__, nexthdr, dataoff,
			 (msdu_desc->msdu_len - dataoff));

		if (!csum_ipv6_magic(&ip6h->saddr, &ip6h->daddr,
				     (msdu_desc->msdu_len - dataoff),
				     nexthdr, csum)) {
			ret = 1;
		} else {
			ret = -1;
		}

		pr_debug("%s: ret: %d\n", __func__, ret);
	}

	return ret;
}

static void rx_send_cmd_process(struct sprd_priv *priv, void *data, int len,
				unsigned char id, unsigned char ctx_id)
{
	struct sprd_vif *vif;
	struct sprd_work *misc_work = NULL;

	if (unlikely(!priv)) {
		pr_err("%s priv not init.\n", __func__);
	} else if (ctx_id > STAP_MODE_P2P_DEVICE) {
		pr_err("%s [ctx_id %d]RX err\n", __func__, ctx_id);
	} else {
		vif = sc2355_ctxid_to_vif(priv, ctx_id);
		if (!vif) {
			pr_err("%s cant't get vif from ctx_id%d\n",
			       __func__, ctx_id);
		} else {
			misc_work = sprd_alloc_work(len);
			if (!misc_work) {
				pr_err("%s out of memory", __func__);
			} else {
				misc_work->vif = vif;
				misc_work->id = id;
				memcpy(misc_work->data, data, len);
				sprd_queue_work(vif->priv, misc_work);
			}
			sprd_put_vif(vif);
		}
	}
}

static void rx_skb_process(struct sprd_priv *priv, struct sk_buff *skb)
{
	struct sprd_vif *vif = NULL;
	struct net_device *ndev = NULL;
	struct rx_msdu_desc *msdu_desc = NULL;
	struct sk_buff *tx_skb = NULL;
	struct sprd_hif *hif;
	struct ethhdr *eth;

	hif = &priv->hif;

	if (unlikely(!priv)) {
		pr_err("%s priv not init.\n", __func__);
		goto err;
	}

	msdu_desc = (struct rx_msdu_desc *)skb->data;
	if (msdu_desc->ctx_id >= SPRD_MAC_INDEX_MAX) {
		pr_err("%s [ctx_id %d]RX err\n", __func__, msdu_desc->ctx_id);
		goto err;
	}

	vif = sc2355_ctxid_to_vif(priv, msdu_desc->ctx_id);
	if (!vif) {
		pr_err("%s cannot get vif, ctx_id: %d\n",
		       __func__, msdu_desc->ctx_id);
		goto err;
	}

	if (!vif->ndev) {
		pr_err("%s ndev is NULL, ctx_id = %d\n",
		       __func__, msdu_desc->ctx_id);
		BUG_ON(1);
	}

	ndev = vif->ndev;
	skb_reserve(skb, msdu_desc->msdu_offset);
	skb_put(skb, msdu_desc->msdu_len);

	eth = (struct ethhdr *)skb->data;
	if (eth->h_proto == htons(ETH_P_IPV6))
		if (ether_addr_equal(skb->data, skb->data + ETH_ALEN)) {
			pr_err
			    ("%s, drop loopback pkt, macaddr:%02x:%02x:%02x:%02x:%02x:%02x\n",
			     __func__, skb->data[0], skb->data[1], skb->data[2],
			     skb->data[3], skb->data[4], skb->data[5]);
			goto err;
		}

	if (hif->tdls_flow_count_enable == 1)
		sc2355_tdls_count_flow(vif, skb->data + ETH_ALEN,
				       skb->len - ETH_ALEN);

	if ((vif->mode == SPRD_MODE_AP ||
	     vif->mode == SPRD_MODE_P2P_GO) && msdu_desc->uc_w2w_flag) {
		skb->dev = ndev;
		dev_queue_xmit(skb);
	} else {
		if ((vif->mode == SPRD_MODE_AP ||
		     vif->mode == SPRD_MODE_P2P_GO) &&
		    msdu_desc->bc_mc_w2w_flag) {
			struct ethhdr *eth = (struct ethhdr *)skb->data;

			if (eth->h_proto != ETH_P_IP &&
			    eth->h_proto != ETH_P_IPV6) {
				tx_skb = pskb_copy(skb, GFP_ATOMIC);
				if (likely(tx_skb)) {
					tx_skb->dev = ndev;
					dev_queue_xmit(tx_skb);
				}
			}
		}

		/* skb->data MUST point to ETH HDR */
		sc2355_tcp_ack_filter_rx(priv, skb->data, msdu_desc->msdu_len);

		if (hif->hw_type == SPRD_HW_SC2355_PCIE)
			sc2355_count_rx_tp(hif, msdu_desc->msdu_len);
		sprd_netif_rx(ndev, skb);
	}

	sprd_put_vif(vif);

	return;

err:
	dev_kfree_skb(skb);
#if defined(MORE_DEBUG)
	hif->stats.rx_errors++;
	hif->stats.rx_dropped++;
#endif
}

static unsigned short rx_data_process(struct sprd_priv *priv,
				      unsigned char *msg)
{
	return 0;
}

static inline void
rx_mh_data_process(struct rx_mgmt *rx_mgmt, void *data,
		   int len, int buffer_type)
{
	sc2355_mm_mh_data_process(&rx_mgmt->mm_entry, data, len, buffer_type);
}

static void
rx_mh_addr_process(struct rx_mgmt *rx_mgmt, void *data,
		   int len, int buffer_type)
{
	struct sprd_hif *hif = rx_mgmt->hif;
	struct sprd_common_hdr *hdr =
	    (struct sprd_common_hdr *)(data + hif->hif_offset);
	struct sprd_work *misc_work = NULL;
	static unsigned long time;

	pr_debug("%s: rx_data_addr=0x%lx\n", __func__, (unsigned long)data);

	if (hdr->reserv) {
		pr_debug("%s: Add RX code here\n", __func__);
		sc2355_mm_mh_data_event_process(&rx_mgmt->mm_entry, data,
						len, buffer_type);
	} else {
		pr_debug("%s: Add TX complete code here\n", __func__);

		if (time != 0 && ((jiffies - time) >= msecs_to_jiffies(1000))) {
			pr_err("%s: out of time %d\n",
			       __func__, jiffies_to_msecs(jiffies - time));
		}

		time = jiffies;

		sc2355_tx_free_pcie_data_num(hif, (unsigned char *)data);
		misc_work = sprd_alloc_work(sizeof(void *));

		if (misc_work) {
			misc_work->id = SPRD_PCIE_TX_FREE_BUF;
			memcpy(misc_work->data, &data, sizeof(void *));
			misc_work->len = buffer_type;

			sprd_queue_work(hif->priv, misc_work);
		} else {
			pr_err("%s fail\n", __func__);
		}
	}
}

static void rx_net_work_queue(struct work_struct *work)
{
	struct rx_mgmt *rx_mgmt;
	struct sprd_priv *priv;
	struct sk_buff *reorder_skb = NULL, *skb = NULL;

	rx_mgmt = container_of(work, struct rx_mgmt, rx_net_work);
	priv = rx_mgmt->hif->priv;

	reorder_skb = sc2355_reorder_get_skb_list(&rx_mgmt->ba_entry);
	while (reorder_skb) {
		SPRD_GET_FIRST_SKB(skb, reorder_skb);
		skb = sc2355_defrag_data_process(&rx_mgmt->defrag_entry, skb);
		if (skb)
			rx_skb_process(priv, skb);
	}
}

static void rx_work_queue(struct work_struct *work)
{
	struct sprd_msg *msg;
	struct sprd_priv *priv;
	struct rx_mgmt *rx_mgmt;
	struct sprd_hif *hif;
	void *pos = NULL, *data = NULL, *tran_data = NULL;
	int len = 0, num = 0;
	int print_len = 100;
	struct sprd_vif *vif;
	struct sprd_cmd_hdr *hdr;

	rx_mgmt = container_of(work, struct rx_mgmt, rx_work);
	hif = rx_mgmt->hif;
	priv = hif->priv;

	if (!hif->exit && !sprd_peek_msg(&rx_mgmt->rx_list))
		sc2355_rx_process(rx_mgmt, NULL);

	while ((msg = sprd_peek_msg(&rx_mgmt->rx_list))) {
		if (hif->exit)
			goto next;

		pos = msg->tran_data;
		for (num = msg->len; num > 0; num--) {
			pos = sc2355_get_rx_data(hif, pos, &data, &tran_data,
						 &len, hif->hif_offset);

			pr_debug("%s: rx type:%d, num = %d\n",
				 __func__, SPRD_HEAD_GET_TYPE(data), num);

			/* len in mbuf_t just means buffer len in ADMA,
			 * so need to get data len in sc2355_sdiohal_puh
			 */
			if (sprd_get_debug_level() >= L_DBG) {
				sc2355_hex_dump("rx data 100B",
						(unsigned char *)data,
						print_len);
			}

			/* to check is the rsp_cnt from CP2
			 * eqaul to rsp_cnt count on driver side.
			 * if not equal, must be lost on SDIOHAL/PCIE.
			 * assert to warn CP2
			 */
			hdr = (struct sprd_cmd_hdr *)data;
			vif = sc2355_ctxid_to_vif(priv, hdr->common.mode);
			if ((SPRD_HEAD_GET_TYPE(data) == SPRD_TYPE_CMD ||
			     SPRD_HEAD_GET_TYPE(data) == SPRD_TYPE_EVENT)) {
				if (rx_mgmt->rsp_event_cnt != hdr->rsp_cnt) {
					pr_info
					    ("%s, %d, rsp_event_cnt=%d, hdr->cnt=%d\n",
					     __func__, __LINE__,
					     rx_mgmt->rsp_event_cnt,
					     hdr->rsp_cnt);

					if (hdr->rsp_cnt == 0) {
						rx_mgmt->rsp_event_cnt = 0;
						pr_info
						    ("%s reset rsp_event_cnt",
						     __func__);
					}
					/* hdr->rsp_cnt=0 means it's a
					 * old version CP2,
					 * so do not assert.
					 * vif=NULL means driver not init ok,
					 * send cmd may cause crash
					 */
					if (vif && hdr->rsp_cnt != 0)
						sc2355_assert_cmd(priv, vif,
								  hdr->cmd_id,
								  RSP_CNT_ERROR);
				}

				rx_mgmt->rsp_event_cnt++;
			}
			sprd_put_vif(vif);

			switch (SPRD_HEAD_GET_TYPE(data)) {
			case SPRD_TYPE_DATA:
				if (msg->len > SPRD_MAX_DATA_RXLEN)
					pr_err("err rx data too long:%d > %d\n",
					       len, SPRD_MAX_DATA_RXLEN);
				rx_data_process(priv, data);
				break;
			case SPRD_TYPE_CMD:
				if (msg->len > SPRD_MAX_CMD_RXLEN)
					pr_err("err rx cmd too long:%d > %d\n",
					       len, SPRD_MAX_CMD_RXLEN);
				sc2355_rx_rsp_process(priv, data);
				break;

			case SPRD_TYPE_EVENT:
				if (msg->len > SPRD_MAX_CMD_RXLEN)
					pr_err
					    ("err rx event too long:%d > %d\n",
					     len, SPRD_MAX_CMD_RXLEN);
				sc2355_rx_evt_process(priv, data);
				break;
			case SPRD_TYPE_DATA_SPECIAL:
				sprd_debug_ts_leave(RX_SDIO_PORT);
				sprd_debug_ts_enter(RX_SDIO_PORT);

				if (msg->len > SPRD_MAX_DATA_RXLEN)
					pr_err
					    ("err data trans too long:%d > %d\n",
					     len, SPRD_MAX_CMD_RXLEN);
				rx_mh_data_process(rx_mgmt, tran_data, len,
						   msg->buffer_type);
				tran_data = NULL;
				data = NULL;
				break;
			case SPRD_TYPE_DATA_PCIE_ADDR:
				if (msg->len > SPRD_MAX_CMD_RXLEN)
					pr_err
					    ("err rx mh data too long:%d > %d\n",
					     len, SPRD_MAX_DATA_RXLEN);
				rx_mh_addr_process(rx_mgmt, tran_data, len,
						   msg->buffer_type);
				tran_data = NULL;
				data = NULL;
				break;
			default:
				pr_err("rx unknown type:%d\n",
				       SPRD_HEAD_GET_TYPE(data));
				break;
			}

			/* Marlin3 should release buffer by ourself */
			if (tran_data)
				sc2355_free_data(tran_data, msg->buffer_type);

			if (!pos) {
				pr_debug("%s no mbuf\n", __func__);
				break;
			}
		}
next:
		sc2355_free_rx_data(hif, msg->fifo_id, msg->tran_data,
				    msg->data, msg->len);
		sprd_dequeue_msg(msg, &rx_mgmt->rx_list);
	}
}

inline int sc2355_fill_skb_csum(struct sk_buff *skb, unsigned short csum)
{
	int ret = 0;

	if (csum) {
		ret = rx_ipv6_csum(skb->data, (__force __wsum)csum);
		if (!ret) {
			skb->ip_summed = CHECKSUM_COMPLETE;
			skb->csum = (__force __wsum)csum;
		} else if (ret > 0) {
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		}
	} else {
		skb->ip_summed = CHECKSUM_NONE;
	}

	return ret;
}

void sc2355_rx_send_cmd(struct sprd_hif *hif, void *data, int len,
			unsigned char id, unsigned char ctx_id)
{
	struct sprd_priv *priv = hif->priv;

	rx_send_cmd_process(priv, data, len, id, ctx_id);
}

void sc2355_queue_rx_buff_work(struct sprd_priv *priv, unsigned char id)
{
	struct sprd_work *misc_work;

	misc_work = sprd_alloc_work(0);
	switch (id) {
	case SPRD_PCIE_RX_ALLOC_BUF:
	case SPRD_PCIE_RX_FLUSH_BUF:
		misc_work->id = id;
		sprd_queue_work(priv, misc_work);
		break;
	default:
		pr_err("%s: err id: %d\n", __func__, id);
		kfree(misc_work);
		break;
	}
}

void sc2355_rx_up(struct rx_mgmt *rx_mgmt)
{
	complete(&rx_mgmt->rx_completed);
}

void sc2355_rx_process(struct rx_mgmt *rx_mgmt, struct sk_buff *pskb)
{
	sc2355_reorder_data_process(&rx_mgmt->ba_entry, pskb);

	if (!work_pending(&rx_mgmt->rx_net_work))
		queue_work(rx_mgmt->rx_net_workq, &rx_mgmt->rx_net_work);
}

int sc2355_mm_fill_buffer(void *hif)
{
	struct rx_mgmt *rx_mgmt =
	    (struct rx_mgmt *)((struct sprd_hif *)hif)->rx_mgmt;
	struct mem_mgmt *mm_entry = &rx_mgmt->mm_entry;
	unsigned int num = 0, alloc_num = atomic_xchg(&mm_entry->alloc_num, 0);

	num = sc2355_mm_buffer_alloc(&rx_mgmt->mm_entry, alloc_num);
	sc2355_tx_addr_trans_pcie(hif, NULL, 0, true);

	if (num)
		num = atomic_add_return(num, &mm_entry->alloc_num);

	if (num > SPRD_MAX_ADD_MH_BUF_ONCE || rx_mgmt->addr_trans_head)
		sc2355_queue_rx_buff_work(rx_mgmt->hif->priv,
					  SPRD_PCIE_RX_ALLOC_BUF);

	return num;
}

void sc2355_mm_fill_all_buffer(void *hif)
{
	struct rx_mgmt *rx_mgmt =
	    (struct rx_mgmt *)((struct sprd_hif *)hif)->rx_mgmt;
	struct mem_mgmt *mm_entry = &rx_mgmt->mm_entry;
	int num = SPRD_MAX_MH_BUF - skb_queue_len(&mm_entry->buffer_list);

	if (num >= 0) {
		atomic_add(num, &mm_entry->alloc_num);
		sc2355_mm_fill_buffer(hif);
	}
}

void sc2355_rx_flush_buffer(void *hif)
{
	struct rx_mgmt *rx_mgmt =
	    (struct rx_mgmt *)((struct sprd_hif *)hif)->rx_mgmt;
	struct mem_mgmt *mm_entry = &rx_mgmt->mm_entry;

	if (rx_mgmt->addr_trans_head)
		sc2355_tx_addr_trans_free(hif);

	sc2355_mm_flush_buffer(mm_entry);
}

int sc2355_rx_init(struct sprd_hif *hif)
{
	int ret = 0;
	struct rx_mgmt *rx_mgmt = NULL;

	rx_mgmt = kzalloc(sizeof(*rx_mgmt), GFP_KERNEL);
	if (!rx_mgmt) {
		ret = -ENOMEM;
		goto err_rx_mgmt;
	}

	/* init rx_list */
	ret = sprd_init_msg(SPRD_RX_MSG_NUM, &rx_mgmt->rx_list);
	if (ret) {
		pr_err("%s tx_buf create failed: %d\n", __func__, ret);
		goto err_rx_list;
	}

	/* init rx_work */
	rx_mgmt->rx_queue =
	    alloc_ordered_workqueue("SPRD_RX_QUEUE", WQ_MEM_RECLAIM |
				    WQ_HIGHPRI | WQ_CPU_INTENSIVE);
	if (!rx_mgmt->rx_queue) {
		pr_err("%s SPRD_RX_QUEUE create failed\n", __func__);
		ret = -ENOMEM;
		goto err_rx_work;
	}

	/*init rx_queue*/
	INIT_WORK(&rx_mgmt->rx_work, rx_work_queue);

	rx_mgmt->rx_net_workq = alloc_ordered_workqueue("SPRD_RX_NET_QUEUE",
							WQ_HIGHPRI |
							WQ_CPU_INTENSIVE |
							WQ_MEM_RECLAIM);
	if (!rx_mgmt->rx_net_workq) {
		pr_err("%s SPRD_RX_NET_QUEUE create failed\n", __func__);
		ret = -ENOMEM;
		goto err_rx_net_work;
	}

	/*init rx_queue*/
	INIT_WORK(&rx_mgmt->rx_net_work, rx_net_work_queue);

	ret = sc2355_defrag_init(&rx_mgmt->defrag_entry);
	if (ret) {
		pr_err("%s init defrag fail: %d\n", __func__, ret);
		goto err_rx_defrag;
	}

	ret = sc2355_mm_init(&rx_mgmt->mm_entry, (void *)hif);
	if (ret) {
		pr_err("%s init mm fail: %d\n", __func__, ret);
		goto err_rx_mm;
	}

	sc2355_reorder_init(&rx_mgmt->ba_entry);

	hif->lp = 0;
	hif->rx_mgmt = (void *)rx_mgmt;
	rx_mgmt->hif = hif;

	return ret;

err_rx_mm:
	sc2355_mm_deinit(&rx_mgmt->mm_entry, hif);
err_rx_defrag:
	destroy_workqueue(rx_mgmt->rx_net_workq);
err_rx_net_work:
	destroy_workqueue(rx_mgmt->rx_queue);
err_rx_work:
	sprd_deinit_msg(&rx_mgmt->rx_list);
err_rx_list:
	kfree(rx_mgmt);
err_rx_mgmt:
	return ret;
}

int sc2355_rx_deinit(struct sprd_hif *hif)
{
	struct rx_mgmt *rx_mgmt = (struct rx_mgmt *)hif->rx_mgmt;

	flush_workqueue(rx_mgmt->rx_queue);
	destroy_workqueue(rx_mgmt->rx_queue);

	flush_workqueue(rx_mgmt->rx_net_workq);
	destroy_workqueue(rx_mgmt->rx_net_workq);

	sprd_deinit_msg(&rx_mgmt->rx_list);

	sc2355_defrag_deinit(&rx_mgmt->defrag_entry);
	sc2355_mm_deinit(&rx_mgmt->mm_entry, hif);
	sc2355_reorder_deinit(&rx_mgmt->ba_entry);

	kfree(rx_mgmt);
	hif->rx_mgmt = NULL;

	return 0;
}
