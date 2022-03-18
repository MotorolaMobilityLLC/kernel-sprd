/*
* SPDX-FileCopyrightText: 2015-2022 Unisoc (Shanghai) Technologies Co., Ltd
* SPDX-License-Identifier: GPL-2.0
*
* Copyright 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of version 2 of the GNU General Public License
* as published by the Free Software Foundation.
*/

#include <linux/of_device.h>
#include <linux/sipc.h>

#include "common/chip_ops.h"
#include "common/hif.h"
#include "common/iface.h"
#include "common/msg.h"
#include "qos.h"
#include "txrx.h"

#define WLAN_CP_ID		3
#define SPRD_SBLOCK_CMD_EVENT	7
#define SPRD_SBLOCK_DATA0	8
#define SPRD_SBLOCK_DATA1	9
#define SPRD_SIPC_HEAD_RESERV	32
/* tx len less than cp len 4 byte as sdiom 4 bytes align */
#define SPRD_MAX_CMD_TXLEN	1396
#define SPRD_MAX_CMD_RXLEN	1088
#define SPRD_MAX_DATA_TXLEN	1624
#define SPRD_MAX_DATA_RXLEN	1592
#define SPRD_WAKE_TIMEOUT	240
#define SPRD_WAKE_PRE_TIMEOUT	80
#define SPRD_TX_MSG_CMD_NUM	4
#define SPRD_TX_MSG_DATA_NUM	128
#define SPRD_TX_DATA_START_NUM	(SPRD_TX_MSG_DATA_NUM - 3)
#define SPRD_SIPC_MASK_LIST0	0x1
#define SPRD_SIPC_MASK_LIST1	0x2
#define SPRD_SIPC_MASK_LIST2	0x4
/* sipc reserv more len than Mac80211 HEAD */
#define SPRD_SIPC_RESERV_LEN	32

enum sipc_port {
	SPRD_SIPC_CMD_TX,
	SPRD_SIPC_CMD_RX = 0,
	SPRD_SIPC_DATA_TX = 1,
	SPRD_SIPC_DATA_RX = 1,
};

static struct sprd_hif *sc2332_hif;

static const char *sipc_channel_tostr(int channel)
{
	switch (channel) {
	case SPRD_SBLOCK_CMD_EVENT:
		return "cmd";
	case SPRD_SBLOCK_DATA0:
		return "data0";
	case SPRD_SBLOCK_DATA1:
		return "data1";
	default:
		return "unknown channel";
	}
}

static int sc2332_reset(struct sprd_hif *hif)
{
	struct sprd_priv *priv = NULL;
	struct sprd_vif *vif, *tmp;

	if (!hif) {
		pr_err("%s can not get hif!\n", __func__);
		return -1;
	}

	priv = hif->priv;
	if (!priv) {
		pr_err("%s can not get priv!\n", __func__);
		return -1;
	}

	/* need reset hif->exit flag, if wcn reset happened */
	if (unlikely(hif->exit)) {
		hif->exit = 0;
		pr_info("%s reset hif->exit flag:%d!\n", __func__, hif->exit);
	}

	/* need reset hif->cp_assert flag */
	if (unlikely(hif->cp_asserted)) {
		hif->cp_asserted = 0;
		pr_info("%s reset hif->cp_asserted flag:%d!\n", __func__,
			hif->cp_asserted);
	}

	list_for_each_entry_safe(vif, tmp, &priv->vif_list, vif_node) {
		int ciphyr_type, key_index;
		int ciphyr_type_max = 2, key_index_max = 4;
		/* check connect state */
		if (vif->mode == SPRD_MODE_STATION ||
		    vif->mode == SPRD_MODE_P2P_CLIENT) {
			if (vif->sm_state == SPRD_DISCONNECTING ||
			    vif->sm_state == SPRD_CONNECTING ||
			    vif->sm_state == SPRD_CONNECTED) {
				pr_info("%s check connection state for sta or p2p gc\n", __func__);
				pr_info("vif->mode : %d, vif->sm_state : %d\n",
					vif->mode, vif->sm_state);
				cfg80211_disconnected(vif->ndev, 0, NULL, 0,
						      false, GFP_KERNEL);
				vif->sm_state = SPRD_DISCONNECTED;
			}
		}

		if (vif->mode == SPRD_MODE_AP) {
			pr_info("softap mode, reset iftype to station, before reset:%d\n",
				vif->wdev.iftype);
			vif->wdev.iftype = NL80211_IFTYPE_STATION;
			pr_info("after reset iftype:%d\n", vif->wdev.iftype);
		}
		if (vif->mode != SPRD_MODE_NONE) {
			pr_info("need reset mode to none: %d\n", vif->mode);
			vif->state &= ~VIF_STATE_OPEN;
			vif->mode = SPRD_MODE_NONE;
		}

		/* reset ssid & bssid */
		memset(vif->bssid, 0, sizeof(vif->bssid));
		memset(vif->ssid, 0, sizeof(vif->ssid));
		vif->ssid_len = 0;
		vif->prwise_crypto = SPRD_CIPHER_NONE;
		vif->grp_crypto = SPRD_CIPHER_NONE;

		for (ciphyr_type = 0; ciphyr_type < ciphyr_type_max; ciphyr_type++) {
			vif->key_index[ciphyr_type] = 0;
			for (key_index = 0; key_index < key_index_max; key_index++) {
				memset(vif->key[ciphyr_type][key_index], 0x00,
				       WLAN_MAX_KEY_LEN);
				vif->key_len[ciphyr_type][key_index] = 0;
			}
		}
	}

	/* flush cmd and data buffer */
	pr_info("%s flust all tx list\n", __func__);
	sc2332_flush_all_txlist(hif);

	return 0;
}

static int sipc_reserv_len(struct sprd_hif *hif)
{
	return SPRD_SIPC_RESERV_LEN;
}

static int sipc_msg_send(void *xmit_data, u16 xmit_len, int channel)
{
	int ret;
	struct sblock blk;
	u8 *addr = NULL;

	/* Get a free sblock. */
	ret = sblock_get(WLAN_CP_ID, channel, &blk, 0);
	if (ret) {
		pr_err("%s Failed to get free sblock(%d)!\n",
		       sipc_channel_tostr(channel), ret);
		return -ENOMEM;
	}

	if (blk.length < xmit_len) {
		pr_err("%s The size of sblock is so tiny!\n",
		       sipc_channel_tostr(channel));
		sblock_put(WLAN_CP_ID, channel, &blk);
		WARN_ON(1);
		return 0;
	}

	addr = (u8 *)blk.addr + SPRD_SIPC_HEAD_RESERV;
	blk.length = xmit_len + SPRD_SIPC_HEAD_RESERV;
	/* memcpy(((u8 *)addr), xmit_data, xmit_len); */
	/* FIXME sharkle V8 none cache workaround, tempuse */
	{
		u8 *pos;

		pos = (u8 *)xmit_data;
		for (ret = xmit_len - 1; ret >= 0; ret--)
			addr[ret] = pos[ret];
	}

	/* FIXME if use less irq to CP, use sblock_send_prepare */
	/* ret = sblock_send_prepare(WLAN_CP_ID, channel, &blk); */
	ret = sblock_send(WLAN_CP_ID, channel, &blk);
	if (ret) {
		pr_err("%s err:%d\n", sipc_channel_tostr(channel), ret);
		sblock_put(WLAN_CP_ID, channel, &blk);
	}

	return ret;
}

static int sipc_tx_cmd(struct sprd_hif *hif, struct sprd_msg_list *list)
{
	struct sprd_msg *msg;

	while ((msg = sprd_peek_msg(list))) {
		if (unlikely(hif->exit)) {
			dev_kfree_skb(msg->skb);
			sprd_dequeue_msg(msg, list);
			continue;
		}
		if (time_after(jiffies, msg->timeout)) {
			hif->drop_cmd_cnt++;
			dev_err(&hif->pdev->dev,
				"tx drop cmd msg,dropcnt:%u\n",
				hif->drop_cmd_cnt);
			dev_kfree_skb(msg->skb);
			sprd_dequeue_msg(msg, list);
			continue;
		}

		sipc_msg_send(msg->tran_data, msg->len, SPRD_SBLOCK_CMD_EVENT);
		dev_kfree_skb(msg->skb);
		sprd_dequeue_msg(msg, list);
	}

	return 0;
}

static int sipc_tx_data(struct sprd_hif *hif,
			struct sprd_msg_list *list,
			struct sprd_qos_t *qos, int num, int channel)
{
	unsigned char mode;
	int ret, pkts;
	unsigned int cnt;
	struct sprd_msg *msg;
	char *pinfo;

	pkts = -1;
	sc2332_qos_reorder(qos);
	while ((msg = sc2332_qos_peek_msg(qos, &pkts))) {
		if (unlikely(hif->exit)) {
			dev_kfree_skb(msg->skb);
			sc2332_qos_update(qos, msg, &msg->list);
			sprd_dequeue_msg(msg, list);
			sc2332_qos_need_resch(qos);
			continue;
		}
		if (time_after(jiffies, msg->timeout)) {
			if (list == &hif->tx_list1) {
				pinfo = "data1";
				cnt = hif->drop_data1_cnt++;
			} else {
				pinfo = "data2";
				cnt = hif->drop_data2_cnt++;
			}
			dev_err(&hif->pdev->dev,
				"tx drop %s msg,dropcnt:%u\n", pinfo, cnt);
			dev_kfree_skb(msg->skb);
			mode = msg->mode;
			sc2332_qos_update(qos, msg, &msg->list);
			sprd_dequeue_msg(msg, list);
			sc2332_qos_need_resch(qos);
			sc2332_wake_queue(hif, list, mode);
			continue;
		}
		if (num-- == 0)
			break;

		ret = sipc_msg_send(msg->tran_data, msg->len, channel);
		if (!ret) {
			dev_kfree_skb(msg->skb);
			mode = msg->mode;
			sc2332_qos_update(qos, msg, &msg->list);
			sprd_dequeue_msg(msg, list);
			sc2332_qos_need_resch(qos);
			sc2332_wake_queue(hif, list, mode);
		} else {
			break;
		}
	}

	return 0;
}

static void sipc_tx_work_queue(struct work_struct *work)
{
	unsigned int send_list, needsleep;
	struct sprd_hif *hif;
	int send_num0, send_num1, send_num2;
	int wake_state = 0;

	send_num0 = 0;
	send_num1 = 0;
	send_num2 = 0;
	hif = container_of(work, struct sprd_hif, tx_work);
RETRY:
	if (unlikely(hif->exit)) {
		if (wake_state)
			__pm_relax(hif->tx_wakelock);

		sc2332_flush_all_txlist(hif);
		return;
	}
	send_list = 0;
	needsleep = 0;
	hif->do_tx = 0;
	if (sprd_msg_tx_pended(&hif->tx_list0))
		send_list |= SPRD_SIPC_MASK_LIST0;
	if (hif->driver_status) {
		if (sprd_msg_tx_pended(&hif->tx_list1)) {
			send_num1 =
			    sblock_get_free_count(WLAN_CP_ID,
						  SPRD_SBLOCK_DATA0);
			if (send_num1 > 0)
				send_list |= SPRD_SIPC_MASK_LIST1;
			else
				needsleep |= SPRD_SIPC_MASK_LIST1;
		}
		if (sprd_msg_tx_pended(&hif->tx_list2)) {
			send_num2 =
			    sblock_get_free_count(WLAN_CP_ID,
						  SPRD_SBLOCK_DATA1);
			if (send_num2 > 0)
				send_list |= SPRD_SIPC_MASK_LIST2;
			else
				needsleep |= SPRD_SIPC_MASK_LIST2;
		}
	}

	/* send_list = 0 & needsleep = 0 means tx_list is empty!
	 * send_list = 0 & needsleep != 0 means cp hasn't enough
	 * space for new message!
	 */
	if (!send_list) {
		if (!needsleep) {
			if (wake_state)
				__pm_relax(hif->tx_wakelock);
			sc2332_keep_wakeup(hif);
			return;
		}
		printk_ratelimited("%s need sleep  -- 0x%x %d %d %d\n",
				   __func__, needsleep, send_num0,
				   send_num1, send_num2);
		if (wake_state) {
			__pm_relax(hif->tx_wakelock);
			wake_state = 0;
		}
		sc2332_keep_wakeup(hif);
		wait_event(hif->waitq, (hif->do_tx || hif->exit));
		goto RETRY;
	}

	/* send list is not empty and cp has more buff, then not permit sleep!
	 */
	if (!wake_state) {
		wake_state = 1;
		__pm_stay_awake(hif->tx_wakelock);
	}

	if (send_list & SPRD_SIPC_MASK_LIST0)
		sipc_tx_cmd(hif, &hif->tx_list0);
	if (hif->driver_status) {
		if (send_list & SPRD_SIPC_MASK_LIST1)
			sipc_tx_data(hif, &hif->tx_list1, &hif->qos1,
				     send_num1, SPRD_SBLOCK_DATA0);
		if (send_list & SPRD_SIPC_MASK_LIST2)
			sipc_tx_data(hif, &hif->tx_list2, &hif->qos2,
				     send_num2, SPRD_SBLOCK_DATA1);
	}

	goto RETRY;
}

static void sipc_rx_process(unsigned char *data, unsigned int len)
{
	struct sprd_priv *priv;
	struct sprd_hif *hif;

	hif = sc2332_hif;
	priv = hif->priv;

	switch (SPRD_HEAD_GET_TYPE(data)) {
	case SPRD_TYPE_DATA:
		if (len > SPRD_MAX_DATA_RXLEN)
			dev_err(&hif->pdev->dev,
				"err rx data too long:%d > %d\n",
				len, SPRD_MAX_DATA_RXLEN);
		sc2332_rx_data_process(priv, data);
		break;
	case SPRD_TYPE_CMD:
		if (len > SPRD_MAX_CMD_RXLEN)
			dev_err(&hif->pdev->dev,
				"err rx cmd too long:%d > %d\n",
				len, SPRD_MAX_CMD_RXLEN);
		sc2332_rx_rsp_process(priv, data);
		break;
	case SPRD_TYPE_EVENT:
		if (len > SPRD_MAX_CMD_RXLEN)
			dev_err(&hif->pdev->dev,
				"err rx event too long:%d > %d\n",
				len, SPRD_MAX_CMD_RXLEN);
		sc2332_rx_evt_process(priv, data);
		break;
	default:
		dev_err(&hif->pdev->dev, "rx unkonow type:%d\n",
			SPRD_HEAD_GET_TYPE(data));
		break;
	}
}

static int sipc_rx_handle(void *data, unsigned int len)
{
	unsigned char *rdata;

	if (!data || !len) {
		dev_err(&sc2332_hif->pdev->dev,
			"%s param erro:%p %d\n", __func__, data, len);
		return -EFAULT;
	}

	if (unlikely(sc2332_hif->exit))
		return -EFAULT;

	rdata = kmalloc(len, GFP_KERNEL);
	if (unlikely(!rdata))
		return -ENOMEM;
	memcpy(rdata, data, len);

	sipc_rx_process(rdata, len);
	kfree(rdata);

	return 0;
}

static void sipc_msg_receive(int channel)
{
	struct sblock blk;
	u32 length = 0;
	int ret;

	while (!sblock_receive(WLAN_CP_ID, channel, &blk, 0)) {
		length = blk.length - SPRD_SIPC_HEAD_RESERV;
		sipc_rx_handle((u8 *)blk.addr + SPRD_SIPC_HEAD_RESERV, length);
		ret = sblock_release(WLAN_CP_ID, channel, &blk);
		if (ret)
			pr_err("release sblock[%d] err:%d\n", channel, ret);
	}
}

static void sipc_data0_handler(int event, void *data)
{
	switch (event) {
	case SBLOCK_NOTIFY_RECV:
		sipc_msg_receive(SPRD_SBLOCK_DATA0);
		break;
	case SBLOCK_NOTIFY_GET:
		sc2332_tx_wakeup(sc2332_hif);
		break;
	case SBLOCK_NOTIFY_STATUS:
		break;
	default:
		pr_err("Invalid data0 sblock notify:%d\n", event);
		break;
	}
}

static void sipc_data1_handler(int event, void *data)
{
	switch (event) {
	case SBLOCK_NOTIFY_GET:
		sc2332_tx_wakeup(sc2332_hif);
		break;
	case SBLOCK_NOTIFY_RECV:
	case SBLOCK_NOTIFY_STATUS:
		break;
	default:
		pr_err("Invalid data1 sblock notify:%d\n", event);
		break;
	}
}

static void sipc_event_handler(int event, void *data)
{
	switch (event) {
	case SBLOCK_NOTIFY_RECV:
		sipc_msg_receive(SPRD_SBLOCK_CMD_EVENT);
		break;
		/* SBLOCK_NOTIFY_GET cmd not need process it */
	case SBLOCK_NOTIFY_GET:
	case SBLOCK_NOTIFY_STATUS:
		break;
	default:
		pr_err("Invalid event sblock notify:%d\n", event);
		break;
	}
}

static void sipc_clean_sblock(void)
{
	int ret = 0, block_num;
	struct sblock blk;

	block_num = 0;
	do {
		ret = sblock_receive(WLAN_CP_ID,
				     SPRD_SBLOCK_CMD_EVENT, &blk, 0);
		if (!ret) {
			sblock_release(WLAN_CP_ID,
				       SPRD_SBLOCK_CMD_EVENT, &blk);
			block_num++;
		}
	} while (!ret);
	if (block_num)
		pr_err("release event rubbish num:%d\n", block_num);

	block_num = 0;
	do {
		ret = sblock_receive(WLAN_CP_ID, SPRD_SBLOCK_DATA0, &blk, 0);
		if (!ret) {
			sblock_release(WLAN_CP_ID, SPRD_SBLOCK_DATA0, &blk);
			block_num++;
		}
	} while (!ret);
	if (block_num)
		pr_err("release data0 rubbish num:%d\n", block_num);

	block_num = 0;
	do {
		ret = sblock_receive(WLAN_CP_ID, SPRD_SBLOCK_DATA1, &blk, 0);
		if (!ret) {
			sblock_release(WLAN_CP_ID, SPRD_SBLOCK_DATA1, &blk);
			block_num++;
		}
	} while (!ret);
	if (block_num)
		pr_err("release data1 rubbish num:%d\n", block_num);
}

static int sipc_sblock_init(struct sprd_hif *hif)
{
	int ret = 0;
	int channel[3];
	int i = 0;
	unsigned long timeout;

	channel[0] = SPRD_SBLOCK_CMD_EVENT;
	channel[1] = SPRD_SBLOCK_DATA0;
	channel[2] = SPRD_SBLOCK_DATA1;
	timeout = jiffies + msecs_to_jiffies(1000);
	while (time_before(jiffies, timeout)) {
		ret = sblock_query(WLAN_CP_ID, channel[i]);
		if (ret) {
			usleep_range(8000, 10000);
			continue;
		} else {
			i++;
			if (i == 3)
				break;
		}
	}
	if (i != 3) {
		pr_err("cp sblock not ready (%d %d)!\n", i, ret);
		return ret;
	}
	sipc_clean_sblock();

	ret = sblock_register_notifier(WLAN_CP_ID, SPRD_SBLOCK_CMD_EVENT,
				       sipc_event_handler, hif->priv);
	if (ret) {
		pr_err("Failed to regitster event sblock notifier (%d)!\n",
		       ret);
		return ret;
	}

	ret = sblock_register_notifier(WLAN_CP_ID, SPRD_SBLOCK_DATA0,
				       sipc_data0_handler, hif->priv);
	if (ret) {
		pr_err("Failed to regitster data0 sblock notifier(%d)!\n",
		       ret);
		goto err_data0;
	}

	ret = sblock_register_notifier(WLAN_CP_ID, SPRD_SBLOCK_DATA1,
				       sipc_data1_handler, hif->priv);
	if (ret) {
		pr_err("Failed to regitster data1 sblock notifier(%d)!\n",
		       ret);
		goto err_data1;
	}

	return 0;

err_data1:
	sblock_register_notifier(WLAN_CP_ID, SPRD_SBLOCK_DATA0, NULL, NULL);
err_data0:
	sblock_register_notifier(WLAN_CP_ID, SPRD_SBLOCK_CMD_EVENT,
				 NULL, NULL);

	return ret;
}

unsigned char sc2332_convert_msg_type(enum sprd_head_type type)
{
	if (type == SPRD_TYPE_DATA)
		return SPRD_SIPC_DATA_TX;

	return SPRD_SIPC_CMD_TX;
}

unsigned int sc2332_max_tx_len(struct sprd_hif *hif, struct sprd_msg *msg)
{
	if (msg->msglist == &hif->tx_list0)
		return SPRD_MAX_CMD_TXLEN;

	return SPRD_MAX_DATA_TXLEN;
}

void sc2332_wake_queue(struct sprd_hif *hif, struct sprd_msg_list *list,
		       enum sprd_mode mode)
{
	if (atomic_read(&list->flow)) {
		if (atomic_read(&list->ref) <= SPRD_TX_DATA_START_NUM) {
			atomic_set(&list->flow, 0);
			hif->net_start_cnt++;
			if (hif->driver_status)
				sprd_net_flowcontrl(hif->priv, mode, true);
		}
	}
}

int sipc_init(struct sprd_hif *hif)
{
	int ret;

	hif->hw_type = SPRD_HW_SC2332_SIPC;
	hif->driver_status = 1;
	sc2332_hif = hif;

	spin_lock_init(&hif->lock);
	init_waitqueue_head(&hif->waitq);
	hif->cmd_timeout = msecs_to_jiffies(SPRD_TX_CMD_TIMEOUT);
	hif->data_timeout = msecs_to_jiffies(SPRD_TX_DATA_TIMEOUT);
	hif->wake_timeout = msecs_to_jiffies(SPRD_WAKE_TIMEOUT);
	hif->wake_pre_timeout = msecs_to_jiffies(SPRD_WAKE_PRE_TIMEOUT);
	hif->wake_last_time = jiffies;
	hif->keep_wake = wakeup_source_create("keep_wakelock");
	wakeup_source_add(hif->keep_wake);

	ret = sprd_init_msg(SPRD_TX_MSG_CMD_NUM, &hif->tx_list0);
	if (ret) {
		dev_err(&hif->pdev->dev, "%s no tx_list0\n", __func__);
		goto err_tx_list0;
	}

	ret = sprd_init_msg(SPRD_TX_MSG_DATA_NUM, &hif->tx_list1);
	if (ret) {
		dev_err(&hif->pdev->dev, "%s no tx_list1\n", __func__);
		goto err_tx_list1;
	}

	ret = sprd_init_msg(SPRD_TX_MSG_DATA_NUM, &hif->tx_list2);
	if (ret) {
		dev_err(&hif->pdev->dev, "%s no tx_list2\n", __func__);
		goto err_tx_list2;
	}

	sc2332_qos_init(&hif->qos0, &hif->tx_list0);
	sc2332_qos_init(&hif->qos1, &hif->tx_list1);
	sc2332_qos_init(&hif->qos2, &hif->tx_list2);

	hif->tx_wakelock = wakeup_source_create("tx_wakelock");
	wakeup_source_add(hif->tx_wakelock);
	hif->tx_queue = alloc_ordered_workqueue("SPRD_TX_QUEUE",
						WQ_MEM_RECLAIM |
						WQ_HIGHPRI | WQ_CPU_INTENSIVE);
	if (!hif->tx_queue) {
		dev_err(&hif->pdev->dev,
			"%s SPRD_TX_QUEUE create failed", __func__);
		ret = -ENOMEM;
		goto err_tx_work;
	}
	INIT_WORK(&hif->tx_work, sipc_tx_work_queue);

	ret = sipc_sblock_init(hif);
	if (ret)
		goto err_sblock_init;

	hif->cp_asserted = 0;
	hif->exit = 0;
	hif->remove_flag = 0;

	return 0;

err_sblock_init:
	destroy_workqueue(hif->tx_queue);
err_tx_work:
	wakeup_source_remove(hif->tx_wakelock);
	sprd_deinit_msg(&hif->tx_list2);
err_tx_list2:
	sprd_deinit_msg(&hif->tx_list1);
err_tx_list1:
	sprd_deinit_msg(&hif->tx_list0);
err_tx_list0:
	wakeup_source_remove(hif->keep_wake);
	return ret;
}

void sipc_deinit(struct sprd_hif *hif)
{
	hif->exit = 1;
	hif->remove_flag = 1;

	sblock_register_notifier(WLAN_CP_ID, SPRD_SBLOCK_DATA1, NULL, NULL);
	sblock_register_notifier(WLAN_CP_ID, SPRD_SBLOCK_DATA0, NULL, NULL);
	sblock_register_notifier(WLAN_CP_ID, SPRD_SBLOCK_CMD_EVENT,
				 NULL, NULL);

	wake_up_all(&hif->waitq);
	flush_workqueue(hif->tx_queue);
	destroy_workqueue(hif->tx_queue);

	sc2332_flush_all_txlist(hif);

	wakeup_source_remove(hif->tx_wakelock);
	wakeup_source_remove(hif->keep_wake);
	sprd_deinit_msg(&hif->tx_list0);
	sprd_deinit_msg(&hif->tx_list1);
	sprd_deinit_msg(&hif->tx_list2);

	pr_info("%s\t"
		"net: stop %u, start %u\t"
		"drop cnt: cmd %u, sta %u, p2p %u\t",
		__func__,
		hif->net_stop_cnt, hif->net_start_cnt,
		hif->drop_cmd_cnt, hif->drop_data1_cnt, hif->drop_data2_cnt);
}

struct sprd_hif_ops sc2332_sipc_ops = {
	.init = sipc_init,
	.deinit = sipc_deinit,
	.reserv_len = sipc_reserv_len,
	.reset = sc2332_reset,
};

extern struct sprd_chip_ops sc2332_chip_ops;
static int sipc_probe(struct platform_device *pdev)
{
	return sprd_iface_probe(pdev, &sc2332_sipc_ops, &sc2332_chip_ops);
}

static int sipc_remove(struct platform_device *pdev)
{
	return sprd_iface_remove(pdev);
}

static const struct of_device_id sc2332_sipc_of_match[] = {
	{.compatible = "sprd,sc2332-sipc-wifi",},
	{},
};

MODULE_DEVICE_TABLE(of, sc2332_sipc_of_match);

static struct platform_driver sc2332_sipc_driver = {
	.probe = sipc_probe,
	.remove = sipc_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "wlan",
		   .of_match_table = sc2332_sipc_of_match,
	},
};

module_platform_driver(sc2332_sipc_driver);

MODULE_DESCRIPTION("Spreadtrum SC2332 SIPC Initialization");
MODULE_AUTHOR("Spreadtrum WCN Division");
MODULE_LICENSE("GPL");
