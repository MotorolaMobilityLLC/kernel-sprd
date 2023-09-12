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

/*#include <linux/sdiom_rx_api.h>
 *#include <linux/sdiom_tx_api.h>
 */

#include <linux/of_device.h>

#include "common/chip_ops.h"
#include "common/hif.h"
#include "common/iface.h"
#include "common/msg.h"
#include "cmdevt.h"
#include "qos.h"
#include "txrx.h"

#define SPRD_MAX_FLUSH_NUM	60
/* tx len less than cp len 4 byte as sdiom 4 bytes align */
#define SPRD_MAX_CMD_TXLEN	1396
#define SPRD_MAX_CMD_RXLEN	1088
#define SPRD_MAX_DATA_TXLEN	1596
#define SPRD_MAX_DATA_RXLEN	1592

#define SPRD_WAKE_TIMEOUT	240
#define SPRD_WAKE_PRE_TIMEOUT	80

#define SPRD_SDIOM_WIFI		2
#define SPRD_TX_MSG_CMD_NUM	2
#define SPRD_TX_MSG_DATA_NUM	192
#define SPRD_TX_DATA_START_NUM	(SPRD_TX_MSG_DATA_NUM - 3)
#define SPRD_RX_MSG_NUM		128

#define SPRD_SDIO_MASK_LIST0	0x1
#define SPRD_SDIO_MASK_LIST1	0x2
#define SPRD_SDIO_MASK_LIST2	0x4

#define ALIGN_4BYTE(a)		(((a) + 3) & ~3)
#define ALIGN_512BYTE(a)	(((a) + 511) & ~511)

enum sdio_port {
	/* cp port 8 */
	SPRD_SDIOM_CMD_TX,
	/* cp port 8 */
	SPRD_SDIOM_CMD_RX = 0,
	/* cp port 9 */
	SPRD_SDIOM_DATA_TX,
	/* cp port 9 */
	SPRD_SDIOM_DATA_RX = 1,
};

struct sdio_flush_cp {
	u8 num;
	u8 resrv[3];
} __packed;

struct sdio_puh {
	unsigned int pad:7;
	unsigned int len:16;
	unsigned int eof:1;
	unsigned int subtype:4;
	unsigned int type:4;
};

static struct sprd_hif *sc2332_hif;

static int sdio_flush_num(struct sprd_priv *priv, int num)
{
	int i, ret, count = 0;
	unsigned short len, cur_len;
	unsigned char *buf;
	struct sdio_puh *puh;
	struct sprd_data_hdr hdr;
	struct wakeup_source *wake;
	const unsigned short buf_size = 2048;

	/* CP tx num is less than 80 */
	if (num > SPRD_MAX_FLUSH_NUM)
		return -EINVAL;
	/* 2k is enough to flush data msg */
	buf = kmalloc(buf_size, GFP_ATOMIC);
	if (!buf)
		return -ENOMEM;

	wake = wakeup_source_create("flush_wakelock");
	wakeup_source_add(wake);
	__pm_stay_awake(wake);
	while (!sdiom_resume_wait_status()) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
		usleep_range_state(2000, 3000, TASK_UNINTERRUPTIBLE);
#else
		usleep_range(2000, 3000);
#endif
		if (count++ >= 1000) {
			dev_err(&sc2332_hif->pdev->dev,
				"%s wait resume erro\n", __func__);
			kfree(buf);
			__pm_relax(wake);
			wakeup_source_remove(wake);
			return -1;
		}
	}
	marlin_set_sleep(MARLIN_WIFI_FLUSH, FALSE);
	ret = marlin_set_wakeup(MARLIN_WIFI_FLUSH);
	if (ret) {
		dev_err(&sc2332_hif->pdev->dev, "%s sdiom wakeup erro:%d\n",
			__func__, ret);
		goto out;
	}
	/* cp wake code */
	cur_len = 0;
	memset(&hdr, 0, sizeof(hdr));
	hdr.common.type = SPRD_TYPE_DATA;
	hdr.common.mode = SPRD_MODE_NONE;
	hdr.info1 = SPRD_DATA_TYPE_NORMAL;
	hdr.plen = cpu_to_le16(sizeof(hdr));

	len = ALIGN_4BYTE(sizeof(hdr) + sizeof(*puh));

	for (i = 0; i < num; i++) {
		puh = (struct sdio_puh *)(buf + cur_len);
		puh->type = SPRD_SDIOM_WIFI;
		puh->subtype = SPRD_SDIOM_DATA_TX;
		puh->len = sizeof(hdr);
		puh->eof = 0;
		puh->pad = 0;
		memcpy(puh + 1, &hdr, sizeof(hdr));
		cur_len += len;
	}
	puh = (struct sdio_puh *)(buf + cur_len);
	puh->type = 0;
	puh->subtype = 0;
	puh->len = 0;
	puh->eof = 1;
	puh->pad = 0;
	cur_len += sizeof(*puh);
	cur_len = ALIGN_512BYTE(cur_len);

	ret = sdiom_sdio_pt_write_raw(buf, cur_len, 0);
	if (ret < 0)
		dev_err(&sc2332_hif->pdev->dev, "%s sdiom write erro:%d\n",
			__func__, ret);

out:
	kfree(buf);
	marlin_set_sleep(MARLIN_WIFI_FLUSH, TRUE);
	__pm_relax(wake);
	wakeup_source_remove(wake);

	return ret;
}

static int sdio_flush_cp_data(struct sprd_priv *priv)
{
	struct sprd_msg *msg;
	struct sdio_flush_cp s_info = { 0 };
	struct sdio_flush_cp r_info;
	u16 r_len, s_len;
	int ret;

	s_len = sizeof(s_info);
	r_len = sizeof(r_info);

	msg = get_cmdbuf(priv, NULL, s_len, CMD_PRE_CLOSE);
	if (!msg)
		return -ENOMEM;
	memcpy(msg->data, (u8 *)&s_info, s_len);

	ret = send_cmd_recv_rsp(priv, msg, (u8 *)&r_info, &r_len);
	if (ret)
		return ret;

	if (r_info.num) {
		dev_info(&sc2332_hif->pdev->dev, "%s flush count:%d\n",
			 __func__, r_info.num);
		ret = sdio_flush_num(priv, r_info.num);
		if (ret)
			dev_err(&sc2332_hif->pdev->dev, "%s flush err:%d\n",
				__func__, ret);
	}

	return ret;
}

static int sdio_tx_cmd(struct sprd_hif *hif, struct sprd_msg_list *list)
{
	int ret;
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

		ret = sdiom_pt_write_skb(msg->tran_data, msg->skb,
					 msg->len, SPRD_SDIOM_WIFI, msg->type);
		if (!ret) {
			sprd_dequeue_msg(msg, list);
		} else {
			dev_err(&hif->pdev->dev, "%s err:%d\n", __func__, ret);
			/* fixme if need retry */
			dev_kfree_skb(msg->skb);
			sprd_dequeue_msg(msg, list);
		}
	}

	return 0;
}

static int sdio_tx_data(struct sprd_hif *hif,
			struct sprd_msg_list *list,
			struct sprd_qos_t *qos, atomic_t *flow)
{
	unsigned char mode;
	int ret, pkts;
	/* sendnum0 shared */
	int sendnum0, sendnum1;
	int sendcnt0 = 0;
	int sendcnt1 = 0;
	int *psend;
	unsigned int cnt;
	struct sprd_msg *msg;
	struct sprd_data_hdr *hdr;

	/* cp: self free link */
	sendnum0 = atomic_read(flow);
	/* ap: shared free link */
	sendnum1 = atomic_read(&hif->flow0);
	if (!sendnum0 && !sendnum1)
		return 0;
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
			char *pinfo;

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
		if (sendnum1) {
			sendnum1--;
			psend = &sendcnt1;
			hdr = (struct sprd_data_hdr *)msg->tran_data;
			hdr->flow3 = 0;
		} else if (sendnum0) {
			sendnum0--;
			psend = &sendcnt0;
			hdr = (struct sprd_data_hdr *)msg->tran_data;
			hdr->flow3 = 1;
		} else {
			break;
		}

		ret = sdiom_pt_write_skb(msg->tran_data, msg->skb,
					 msg->len, SPRD_SDIOM_WIFI, msg->type);
		if (!ret) {
			*psend += 1;
			mode = msg->mode;
			sc2332_qos_update(qos, msg, &msg->list);
			sprd_dequeue_msg(msg, list);
			sc2332_qos_need_resch(qos);
			sc2332_wake_queue(hif, list, mode);
		} else {
			printk_ratelimited("%s pt_write_skb err:%d\n",
					   __func__, ret);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
			usleep_range_state(800, 1000, TASK_UNINTERRUPTIBLE);
#else
			usleep_range(800, 1000);
#endif
			break;
		}
	}

	hif->ring_ap += sendcnt0 + sendcnt1;
	if (sendcnt1)
		atomic_sub(sendcnt1, &hif->flow0);
	if (sendcnt0)
		atomic_sub(sendcnt0, flow);

	return sendcnt0 + sendcnt1;
}

static void sdio_tx_work_queue(struct work_struct *work)
{
	unsigned int send_list, needsleep;
	struct sprd_hif *hif;
	int send_num0, send_num1, send_num2;

	send_num0 = 0;
	send_num1 = 0;
	send_num2 = 0;
	hif = container_of(work, struct sprd_hif, tx_work);
RETRY:
	if (unlikely(hif->exit)) {
		sc2332_flush_all_txlist(hif);
		return;
	}
	send_list = 0;
	needsleep = 0;
	hif->do_tx = 0;
	if (sprd_msg_tx_pended(&hif->tx_list0))
		send_list |= SPRD_SDIO_MASK_LIST0;

	if (hif->driver_status) {
		if (sprd_msg_tx_pended(&hif->tx_list1)) {
			send_num0 = atomic_read(&hif->flow0);
			send_num1 = atomic_read(&hif->flow1);
			if (send_num1 || send_num0)
				send_list |= SPRD_SDIO_MASK_LIST1;
			else
				needsleep |= SPRD_SDIO_MASK_LIST1;
		}
		if (sprd_msg_tx_pended(&hif->tx_list2)) {
			send_num0 = atomic_read(&hif->flow0);
			send_num2 = atomic_read(&hif->flow2);
			if (send_num2 || send_num0)
				send_list |= SPRD_SDIO_MASK_LIST2;
			else
				needsleep |= SPRD_SDIO_MASK_LIST2;
		}
	}

	if (!send_list) {
		if (!needsleep) {
			sc2332_keep_wakeup(hif);
			return;
		}
		printk_ratelimited("%s need sleep  -- 0x%x %d %d %d\n",
				   __func__, needsleep, send_num0,
				   send_num1, send_num2);
		sc2332_keep_wakeup(hif);
		wait_event(hif->waitq, (hif->do_tx || hif->exit));
		goto RETRY;
	}

	if (send_list & SPRD_SDIO_MASK_LIST0)
		sdio_tx_cmd(hif, &hif->tx_list0);
	if (hif->driver_status) {
		if (send_list & SPRD_SDIO_MASK_LIST2)
			sdio_tx_data(hif, &hif->tx_list2,
				     &hif->qos2, &hif->flow2);
		if (send_list & SPRD_SDIO_MASK_LIST1)
			sdio_tx_data(hif, &hif->tx_list1,
				     &hif->qos1, &hif->flow1);
	}

	goto RETRY;
}

static int sdio_process_credit(void *data)
{
	int ret = 0;
	unsigned char *flow;
	struct sprd_common_hdr *common;

	common = (struct sprd_common_hdr *)data;
	if (common->rsp && common->type == SPRD_TYPE_DATA) {
		flow = &((struct sprd_data_hdr *)data)->flow0;
		goto out;
	} else if (common->type == SPRD_TYPE_EVENT) {
		struct sprd_cmd_hdr *cmd;

		cmd = (struct sprd_cmd_hdr *)data;
		if (cmd->cmd_id == EVT_SDIO_FLOWCON) {
			flow = cmd->paydata;
			ret = -1;
			goto out;
		}
	}
	return 0;

out:
	if (flow[0])
		atomic_add(flow[0], &sc2332_hif->flow0);
	if (flow[1])
		atomic_add(flow[1], &sc2332_hif->flow1);
	if (flow[2])
		atomic_add(flow[2], &sc2332_hif->flow2);
	if (flow[0] || flow[1] || flow[2]) {
		sc2332_hif->ring_cp += flow[0] + flow[1] + flow[2];
		sc2332_tx_wakeup(sc2332_hif);
	}

	return ret;
}

static void sdio_rx_process(unsigned char *data, unsigned int len,
			    unsigned int fifo_id)
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
	sdiom_pt_read_release(fifo_id);
}

static unsigned int sdio_rx_handle(void *data, unsigned int len,
				   unsigned int fifo_id)
{
	if (!data || !len) {
		dev_err(&sc2332_hif->pdev->dev,
			"%s param erro:%p %d\n", __func__, data, len);
		goto out;
	}

	if (unlikely(sc2332_hif->exit))
		goto out;

	if (sdio_process_credit(data))
		goto out;

	sdio_rx_process(data, len, fifo_id);

	return 0;
out:
	sdiom_pt_read_release(fifo_id);
	return 0;
}

unsigned char sc2332_convert_msg_type(enum sprd_head_type type)
{
	if (type == SPRD_TYPE_DATA)
		return SPRD_SDIOM_DATA_TX;

	return SPRD_SDIOM_CMD_TX;
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

int sdio_init(struct sprd_hif *hif)
{
	int ret;

	hif->hw_type = SPRD_HW_SC2332_SDIO;
	hif->driver_status = 1;
	sc2332_hif = hif;

	spin_lock_init(&hif->lock);
	init_waitqueue_head(&hif->waitq);
	atomic_set(&hif->flow0, 0);
	atomic_set(&hif->flow1, 0);
	atomic_set(&hif->flow2, 0);
	hif->cmd_timeout = msecs_to_jiffies(SPRD_TX_CMD_TIMEOUT);
	hif->data_timeout = msecs_to_jiffies(SPRD_TX_DATA_TIMEOUT);
	hif->wake_timeout = msecs_to_jiffies(SPRD_WAKE_TIMEOUT);
	hif->wake_pre_timeout = msecs_to_jiffies(SPRD_WAKE_PRE_TIMEOUT);
	hif->wake_last_time = jiffies;
	hif->keep_wake = wakeup_source_create("keep_wakelock");
	wakeup_source_add(hif->keep_wake);

	ret = sprd_init_msg(SPRD_RX_MSG_NUM, &hif->tx_list0);
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

	hif->tx_queue = alloc_ordered_workqueue("SPRD_TX_QUEUE",
						WQ_MEM_RECLAIM |
						WQ_HIGHPRI | WQ_CPU_INTENSIVE);
	if (!hif->tx_queue) {
		dev_err(&hif->pdev->dev,
			"%s SPRD_TX_QUEUE create failed", __func__);
		ret = -ENOMEM;
		goto err_tx_work;
	}
	INIT_WORK(&hif->tx_work, sdio_tx_work_queue);

	sdiom_register_pt_tx_release(SPRD_SDIOM_WIFI,
				     SPRD_SDIOM_CMD_TX, consume_skb);
	sdiom_register_pt_tx_release(SPRD_SDIOM_WIFI,
				     SPRD_SDIOM_DATA_TX, consume_skb);

	sdiom_register_pt_rx_process(SPRD_SDIOM_WIFI,
				     SPRD_SDIOM_CMD_RX, sdio_rx_handle);
	sdiom_register_pt_rx_process(SPRD_SDIOM_WIFI,
				     SPRD_SDIOM_DATA_RX, sdio_rx_handle);
	return 0;

err_tx_work:
	sprd_deinit_msg(&hif->tx_list2);
err_tx_list2:
	sprd_deinit_msg(&hif->tx_list1);
err_tx_list1:
	sprd_deinit_msg(&hif->tx_list0);
err_tx_list0:
	wakeup_source_remove(hif->keep_wake);
	return ret;
}

void sdio_deinit(struct sprd_hif *hif)
{
	hif->exit = 1;

	sdiom_register_pt_rx_process(SPRD_SDIOM_WIFI, SPRD_SDIOM_CMD_RX, NULL);
	sdiom_register_pt_rx_process(SPRD_SDIOM_WIFI, SPRD_SDIOM_DATA_RX, NULL);

	wake_up_all(&hif->waitq);
	flush_workqueue(hif->tx_queue);
	destroy_workqueue(hif->tx_queue);

	sc2332_flush_all_txlist(hif);

	wakeup_source_remove(hif->tx_wakelock);
	sprd_deinit_msg(&hif->tx_list0);
	sprd_deinit_msg(&hif->tx_list1);
	sprd_deinit_msg(&hif->tx_list2);

	pr_info("%s\t"
		"net: stop %u, start %u\t"
		"drop cnt: cmd %u, sta %u, p2p %u\t"
		"ring_ap:%u ring_cp:%u common:%u sta:%u p2p:%u\n",
		__func__,
		hif->net_stop_cnt, hif->net_start_cnt,
		hif->drop_cmd_cnt, hif->drop_data1_cnt,
		hif->drop_data2_cnt,
		hif->ring_ap, hif->ring_cp,
		atomic_read(&hif->flow0),
		atomic_read(&hif->flow1), atomic_read(&hif->flow2));
}

struct sprd_hif_ops sc2332_sdio_ops = {
	.init = sdio_init,
	.deinit = sdio_deinit,
};

extern struct sprd_chip_ops sc2332_chip_ops;
static int sdio_probe(struct platform_device *pdev)
{
	return sprd_iface_probe(pdev, &sc2332_sdio_ops, &sc2332_chip_ops);
}

static int sdio_remove(struct platform_device *pdev)
{
	return sprd_iface_remove(pdev);
}

static const struct of_device_id sc2332_sdio_of_match[] = {
	{.compatible = "sprd,sc2332-sdio-wifi",},
	{},
};

MODULE_DEVICE_TABLE(of, sc2332_sdio_of_match);

static struct platform_driver sc2332_sdio_driver = {
	.probe = sdio_probe,
	.remove = sdio_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "wlan",
		   .of_match_table = sc2332_sdio_of_match,
	},
};

module_platform_driver(sc2332_sdio_driver);

MODULE_DESCRIPTION("Spreadtrum SC2332 SDIO Initialization");
MODULE_AUTHOR("Spreadtrum WCN Division");
MODULE_LICENSE("GPL");
