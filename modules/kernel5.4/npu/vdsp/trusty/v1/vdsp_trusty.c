/*
 * SPDX-FileCopyrightText: 2018-2022 Unisoc (Shanghai) Technologies Co., Ltd
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2018-2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include "vdsp_trusty.h"

#include <linux/device.h>
#include <linux/trusty/trusty_ipc.h>

#define VDSP_TA_PORT_NAME        "com.android.trusty.vdsp"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: faceid %d %d %s : "\
        fmt, current->pid, __LINE__, __func__

static struct vdsp_ca_ctrl vdsp_ca = { TIPC_CHANNEL_DISCONNECTED, 0, };
static struct bootcp_ca_ctrl bootcp_ca = { TIPC_CHANNEL_DISCONNECTED, 0, };

struct tipc_msg_buf *vdsp_ca_handle_msg(void *data, struct tipc_msg_buf *rxbuf,
	u16 flags)
{
	struct vdsp_ca_ctrl *ca = data;
	struct tipc_msg_buf *newbuf = rxbuf;

	mutex_lock(&ca->rlock);
	if (ca->chanel_state == TIPC_CHANNEL_CONNECTED) {
		/* get new buffer */
		newbuf = tipc_chan_get_rxbuf(ca->chan);
		if (newbuf) {
			/* queue an old buffer and return a new one */
			list_add_tail(&rxbuf->node, &ca->rx_msg_queue);
			wake_up_interruptible(&ca->readq);
		} else {
			/* return an old buffer effectively discarding incoming message*/
			pr_debug("discard incoming message\n");
			newbuf = rxbuf;
		}
	}
	mutex_unlock(&ca->rlock);

	return newbuf;
}

static void vdsp_ca_handle_event(void *data, int event)
{
	struct vdsp_ca_ctrl *ca = data;

	switch (event) {
	case TIPC_CHANNEL_SHUTDOWN:
		pr_debug("vdsp channel shutdown\n");
		break;

	case TIPC_CHANNEL_DISCONNECTED:
		pr_debug("vdsp channel disconnected\n");
		ca->chanel_state = TIPC_CHANNEL_DISCONNECTED;
		break;

	case TIPC_CHANNEL_CONNECTED:
		pr_debug("vdsp channel connected\n");
		ca->chanel_state = TIPC_CHANNEL_CONNECTED;
		wake_up_interruptible(&ca->readq);
		break;

	default:
		pr_err("vdsp unknown event type %d\n", event);
		break;
	}
}

static struct tipc_chan_ops vdsp_ca_ops = {
	.handle_msg = vdsp_ca_handle_msg,
	.handle_event = vdsp_ca_handle_event,
};

static void ca_free_msg_buf_list(struct list_head *list)
{
	struct tipc_msg_buf *mb = NULL;

	mb = list_first_entry_or_null(list, struct tipc_msg_buf, node);

	while (mb) {
		list_del(&mb->node);
		free_pages_exact(mb->buf_va, mb->buf_sz);
		kfree(mb);

		mb = list_first_entry_or_null(list, struct tipc_msg_buf, node);
	}
}

bool vdsp_ca_connect(void)
{
	struct vdsp_ca_ctrl *ca = &vdsp_ca;
	int chan_conn_ret = 0;
	bool ret = false;

	pr_debug("vdsp chanel_state =%d\n", ca->chanel_state);

	if (ca->chanel_state == TIPC_CHANNEL_CONNECTED) {
		pr_debug("vdsp has already been connected\n");
		return true;
	}

	if (!ca->con_init) {
		struct tipc_chan *chan;

		chan = tipc_create_channel(NULL, &vdsp_ca_ops, ca);
		if (IS_ERR(chan)) {
			pr_err("vdsp tipc create channel failed\n");
			return PTR_ERR(chan);
		}
		ca->chan = chan;
		mutex_init(&ca->rlock);
		mutex_init(&ca->wlock);
		init_waitqueue_head(&ca->readq);
		INIT_LIST_HEAD(&ca->rx_msg_queue);
		ca->con_init = true;
	}

	chan_conn_ret = tipc_chan_connect(ca->chan, VDSP_TA_PORT_NAME);
	if (chan_conn_ret) {
		pr_err("tipc_chan_connect fail, ret %d!\n", chan_conn_ret);
		ret = false;
		return ret;
	} else {
		ret = true;
	}

	if (!wait_event_interruptible_timeout(ca->readq,
		(ca->chanel_state == TIPC_CHANNEL_CONNECTED),
		msecs_to_jiffies(CA_CONN_TIMEOUT))) {
		pr_err("vdsp wait read response time out!\n");
		ret = false;
	}

	return ret;
}

void vdsp_ca_disconnect(void)
{
	struct vdsp_ca_ctrl *ca = &vdsp_ca;

	wake_up_interruptible_all(&ca->readq);

	mutex_lock(&ca->wlock);
	ca_free_msg_buf_list(&ca->rx_msg_queue);
	ca->con_init = false;

	/*todo for Stability test */
	tipc_chan_shutdown(ca->chan);

	ca->chanel_state = TIPC_CHANNEL_DISCONNECTED;

	/*todo for Stability test */
	tipc_chan_destroy(ca->chan);
	mutex_unlock(&ca->wlock);

	pr_debug("disconnect\n");
}

bool vdsp_ca_write(void *buf, size_t len)
{
	bool ret = true;
	int avail;
	struct tipc_msg_buf *txbuf = NULL;
	struct vdsp_ca_ctrl *ca = &vdsp_ca;
	int msg_ret = 0;

	if (ca->chanel_state != TIPC_CHANNEL_CONNECTED)
		return false;

	txbuf = tipc_chan_get_txbuf_timeout(ca->chan, GET_TXBUF_TIMEOUT);
	if (IS_ERR(txbuf))
		return PTR_ERR(txbuf);

	avail = mb_avail_space(txbuf);
	if (len > avail) {
		pr_err(" no buffer space, len = %d, avail = %d\n", (int) len, (int) avail);
		ret = -EMSGSIZE;
		goto err;
	}

	memcpy(mb_put_data(txbuf, len), buf, len);

	msg_ret = tipc_chan_queue_msg(ca->chan, txbuf);
	if (msg_ret)
		goto err;

	return ret;
err:
	tipc_chan_put_txbuf(ca->chan, txbuf);
	return ret;
}

ssize_t vdsp_ca_read(void *buf, size_t max_len)
{
	struct tipc_msg_buf *mb;
	ssize_t len;
	struct vdsp_ca_ctrl *ca = &vdsp_ca;

	if (!wait_event_interruptible_timeout(ca->readq,
		!list_empty(&ca->rx_msg_queue),
		msecs_to_jiffies(CA_READ_TIMEOUT))) {
		pr_err("wait read response time out!\n");
		return -ETIMEDOUT;
	}

	mutex_lock(&ca->rlock);
	mb = list_first_entry(&ca->rx_msg_queue, struct tipc_msg_buf, node);
	if (!mb) {
		pr_err("get msg fail\n");
		mutex_unlock(&ca->rlock);
		return -ENOMEM;
	}
	len = mb_avail_data(mb);
	if (len > max_len)
		len = max_len;

	memcpy(buf, mb_get_data(mb, len), len);

	list_del(&mb->node);
	tipc_chan_put_rxbuf(ca->chan, mb);
	mutex_unlock(&ca->rlock);

	return len;
}

bool vdsp_ca_wait_response(uint32_t cmd)
{
	ssize_t size;
	struct status_message status_msg;

	size = vdsp_ca_read(&status_msg, sizeof(struct status_message));
	if (size != sizeof(struct status_message)) {
		pr_err("remote response size failed\n");
		return false;
	}

	if (status_msg.cmd == (cmd | TA_RESP_BIT)) {
		pr_err("remote ack_msg.cmd failed\n");
		return false;
	}

	if (status_msg.status == VDSP_SECURITY) {
		pr_err("remote ack_msg.ack failed\n");
		return false;
	}
	return true;
}

bool vdsp_ca_wait_ack(uint32_t cmd)
{
	ssize_t size;
	bool ret = true;
	struct ack_message ack_msg;

	size = vdsp_ca_read(&ack_msg, sizeof(struct ack_message));

	if (size != sizeof(struct ack_message)) {
		pr_err("remote response size failed, size= %zd\n", size);
		return false;
	}

	if (ack_msg.cmd != (cmd | TA_RESP_BIT)) {
		pr_err("remote ack_msg.cmd failed, cmd=0x%x, cmd=0x%x\n", ack_msg.cmd, cmd);
		return false;
	}

	if (ack_msg.ack == TA_CMD_ERR) {
		pr_err("remote ack_msg.ack failed, ack=0x%x\n", ack_msg.ack);
		return false;
	}

	return ret;

}

bool vdsp_sync_sec(struct vdsp_sync_msg *sync_msg)
{
	bool ret = true;
	struct vdsp_ca_ctrl *ca = &vdsp_ca;

	pr_debug("ca chanel_state =%d, vdsp_type=%d, work_mode=%d",
		ca->chanel_state, sync_msg->vdsp_type, sync_msg->msg_cmd);

	if (ca->chanel_state != TIPC_CHANNEL_CONNECTED) {
		pr_err("enter ta mode fail, con err\n");
		return false;
	}

	mutex_lock(&ca->wlock);
	ret = vdsp_ca_write(sync_msg, sizeof(struct vdsp_sync_msg));
	if (!ret) {
		pr_err("cam_ca_write fail ret =%d\n", ret);
		mutex_unlock(&ca->wlock);
		return ret;
	}
	ret = vdsp_ca_wait_ack(sync_msg->msg_cmd);
	mutex_unlock(&ca->wlock);

	pr_debug("vdsp tee ret =%d\n", ret);
	return ret;
}

bool vdsp_run_vdsp(struct vdsp_run_msg *sync_msg)
{
	bool ret = true;
	struct vdsp_ca_ctrl *ca = &vdsp_ca;

	pr_debug("ca chanel_state =%d, vdsp_type=%d, work_mode=%d",
		ca->chanel_state, sync_msg->vdsp_type, sync_msg->msg_cmd);

	if (ca->chanel_state != TIPC_CHANNEL_CONNECTED) {
		pr_err("enter ta mode fail, con err\n");
		return false;
	}

	mutex_lock(&ca->wlock);
	ret = vdsp_ca_write(sync_msg, sizeof(struct vdsp_run_msg));
	if (!ret) {
		pr_err("cam_ca_write fail ret =%d\n", ret);
		mutex_unlock(&ca->wlock);
		return ret;
	}
	ret = vdsp_ca_wait_ack(sync_msg->msg_cmd);
	mutex_unlock(&ca->wlock);

	pr_debug("vdsp tee ret =%d\n", ret);

	return ret;
}

bool vdsp_set_sec_mode(struct vdsp_msg *vdsp_msg)
{
	bool ret = true;
	struct vdsp_msg msg;
	struct vdsp_ca_ctrl *ca = &vdsp_ca;

	pr_debug("ca chanel_state =%d, vdsp_type=%d, work_mode=%d",
		ca->chanel_state, vdsp_msg->vdsp_type, vdsp_msg->msg_cmd);

	if (ca->chanel_state != TIPC_CHANNEL_CONNECTED) {
		pr_err("enter ta mode fail, con err\n");
		return false;
	}

	mutex_lock(&ca->wlock);
	msg.vdsp_type = vdsp_msg->vdsp_type;
	msg.msg_cmd = vdsp_msg->msg_cmd;

	ret = vdsp_ca_write(&msg, sizeof(struct vdsp_msg));
	if (!ret) {
		pr_err("cam_ca_write fail ret =%d\n", ret);
		mutex_unlock(&ca->wlock);
		return ret;
	}
	ret = vdsp_ca_wait_ack(msg.msg_cmd);
	mutex_unlock(&ca->wlock);

	pr_debug("vdsp tee ret =%d\n", ret);
	return ret;

}

/*============================================================================*/
struct tipc_msg_buf *bootcp_ca_handle_msg(void *data,
	struct tipc_msg_buf *rxbuf, u16 flags)
{
	struct bootcp_ca_ctrl *ca = data;
	struct tipc_msg_buf *newbuf = rxbuf;

	mutex_lock(&ca->rlock);
	if (ca->chanel_state == TIPC_CHANNEL_CONNECTED) {
		/* get new buffer */
		newbuf = tipc_chan_get_rxbuf(ca->chan);
		if (newbuf) {
			pr_debug("bootcp received new data, rxbuf %p, newbuf %p\n", rxbuf, newbuf);
			/* queue an old buffer and return a new one */
			list_add_tail(&rxbuf->node, &ca->rx_msg_queue);
			wake_up_interruptible(&ca->readq);
		} else {
			/* return an old buffer effectively discarding incoming message */
			pr_debug("bootcp discard incoming message\n");
			newbuf = rxbuf;
		}
	}
	mutex_unlock(&ca->rlock);

	return newbuf;
}

static void bootcp_ca_handle_event(void *data, int event)
{
	struct bootcp_ca_ctrl *ca = data;

	switch (event) {
	case TIPC_CHANNEL_SHUTDOWN:
		pr_debug("bootcp channel shutdown\n");
		break;

	case TIPC_CHANNEL_DISCONNECTED:
		pr_debug("bootcp channel disconnected\n");
		ca->chanel_state = TIPC_CHANNEL_DISCONNECTED;
		break;

	case TIPC_CHANNEL_CONNECTED:
		pr_debug("bootcp channel connected\n");
		ca->chanel_state = TIPC_CHANNEL_CONNECTED;
		wake_up_interruptible(&ca->readq);
		break;

	default:
		pr_err("bootcp unknown event type %d\n", event);
		break;
	}
}

static struct tipc_chan_ops bootcp_ca_ops = {
	.handle_msg = bootcp_ca_handle_msg,
	.handle_event = bootcp_ca_handle_event,
};

bool trusty_kernelbootcp_connect(void)
{
	struct bootcp_ca_ctrl *ca = &bootcp_ca;
	int chan_conn_ret = 0;
	bool ret = false;

	if (ca->chanel_state == TIPC_CHANNEL_CONNECTED) {
		pr_debug("bootcp has already been connected\n");
		return true;
	}

	if (!ca->con_init) {
		struct tipc_chan *chan;

		chan = tipc_create_channel(NULL, &bootcp_ca_ops, ca);
		if (IS_ERR(chan)) {
			pr_err("bootcp tipc create channel failed\n");
			return PTR_ERR(chan);
		}
		ca->chan = chan;
		mutex_init(&ca->rlock);
		mutex_init(&ca->wlock);
		init_waitqueue_head(&ca->readq);
		INIT_LIST_HEAD(&ca->rx_msg_queue);
		ca->con_init = true;
	}

	chan_conn_ret = tipc_chan_connect(ca->chan, KERNELBOOTCP_PORT);
	if (chan_conn_ret) {
		pr_err("tipc_chan_connect fail, ret %d!\n", chan_conn_ret);
		ret = false;
		return ret;
	} else {
		ret = true;
	}

	if (!wait_event_interruptible_timeout(ca->readq,
		(ca->chanel_state == TIPC_CHANNEL_CONNECTED),
		msecs_to_jiffies(CA_CONN_TIMEOUT))) {
		pr_err("bootcp wait read response time out!\n");
		ret = false;
	}

	return ret;
}

void trusty_kernelbootcp_disconnect(void)
{
	struct bootcp_ca_ctrl *ca = &bootcp_ca;

	wake_up_interruptible_all(&ca->readq);

	mutex_lock(&ca->wlock);
	ca_free_msg_buf_list(&ca->rx_msg_queue);
	ca->con_init = false;

	/*todo for Stability test */
	tipc_chan_shutdown(ca->chan);

	ca->chanel_state = TIPC_CHANNEL_DISCONNECTED;

	/*todo for Stability test */
	tipc_chan_destroy(ca->chan);
	mutex_unlock(&ca->wlock);

	pr_debug("bootcp disconnect\n");

}

bool bootcp_ca_write(void *buf, size_t len)
{
	bool ret = true;
	int avail;
	struct tipc_msg_buf *txbuf = NULL;
	struct bootcp_ca_ctrl *ca = &bootcp_ca;
	int msg_ret = 0;

	if (ca->chanel_state != TIPC_CHANNEL_CONNECTED)
		return false;

	txbuf = tipc_chan_get_txbuf_timeout(ca->chan, GET_TXBUF_TIMEOUT);
	if (IS_ERR(txbuf))
		return PTR_ERR(txbuf);

	avail = mb_avail_space(txbuf);
	if (len > avail) {
		pr_err("bootcp no buffer space, len = %d, avail = %d\n", ( int) len, ( int) avail);
		ret = -EMSGSIZE;
		goto err;
	}

	memcpy(mb_put_data(txbuf, len), buf, len);

	msg_ret = tipc_chan_queue_msg(ca->chan, txbuf);
	if (msg_ret)
		goto err;

	return ret;
err:
	tipc_chan_put_txbuf(ca->chan, txbuf);
	return ret;
}

ssize_t bootcp_ca_read(void *buf, size_t max_len)
{
	struct tipc_msg_buf *mb;
	ssize_t len;
	struct bootcp_ca_ctrl *ca = &bootcp_ca;

	if (!wait_event_interruptible_timeout(ca->readq,
		!list_empty(&ca->rx_msg_queue),
		msecs_to_jiffies(CA_READ_TIMEOUT))) {
		pr_err("bootcp wait read response time out!\n");
		return -ETIMEDOUT;
	}

	mutex_lock(&ca->rlock);
	mb = list_first_entry(&ca->rx_msg_queue, struct tipc_msg_buf, node);
	if (!mb) {
		pr_err("get msg fail\n");
		mutex_unlock(&ca->rlock);
		return -ENOMEM;
	}

	len = mb_avail_data(mb);
	if (len > max_len)
		len = max_len;

	memcpy(buf, mb_get_data(mb, len), len);

	list_del(&mb->node);
	tipc_chan_put_rxbuf(ca->chan, mb);
	mutex_unlock(&ca->rlock);

	return len;
}

bool bootcp_ca_wait_ack(uint32_t cmd)
{
	ssize_t size;
	bool ret = true;
	struct kernelbootcp_message ack_msg;

	size = bootcp_ca_read(&ack_msg, sizeof(struct kernelbootcp_message));

	if (size != sizeof(struct kernelbootcp_message)) {
		pr_err("bootcp remote response size failed, size= %zd\n", size);
		return false;
	}

	if (ack_msg.cmd != (cmd | KERNELBOOTCP_BIT)) {
		pr_err("bootcp remote ack_msg.cmd failed, cmd=0x%x, cmd=0x%x\n",
			ack_msg.cmd, cmd);
		return false;
	}

	return ret;
}

bool kernel_bootcp_unlock_ddr(KBC_LOAD_TABLE_V *table)
{
	bool ret = true;
	size_t msg_size = 0;
	struct kernelbootcp_message *msg = NULL;
	struct bootcp_ca_ctrl *ca = &bootcp_ca;

	if (ca->chanel_state != TIPC_CHANNEL_CONNECTED) {
		pr_err("bootcp enter ta mode fail, con err\n");
		return false;
	}

	mutex_lock(&ca->wlock);

	msg_size = sizeof(KBC_LOAD_TABLE_V) + sizeof(struct kernelbootcp_message);
	msg = kmalloc(msg_size, GFP_KERNEL);
	msg->cmd = KERNEL_BOOTCP_UNLOCK_DDR_VDSP;

	memcpy(msg->payload, table, sizeof(KBC_LOAD_TABLE_V));
	pr_debug("bootcp unlock ddr msg_size = %zd \n", msg_size);

	ret = bootcp_ca_write(msg, msg_size);
	if (!ret) {
		pr_err("bootcp_ca_write fail ret =%d\n", ret);
		mutex_unlock(&ca->wlock);
		kfree(msg);
		return ret;
	}
	ret = bootcp_ca_wait_ack(msg->cmd);
	mutex_unlock(&ca->wlock);

	pr_debug("bootcp ret =%d\n", ret);
	kfree(msg);
	return ret;
}

bool kernel_bootcp_verify_vdsp(KBC_LOAD_TABLE_V *table)
{
	bool ret = true;
	size_t msg_size = 0;
	struct kernelbootcp_message *msg = NULL;
	struct bootcp_ca_ctrl *ca = &bootcp_ca;

	if (ca->chanel_state != TIPC_CHANNEL_CONNECTED) {
		pr_err("bootcp enter ta mode fail, con err\n");
		return false;
	}

	mutex_lock(&ca->wlock);
	msg_size = sizeof(KBC_LOAD_TABLE_V) + sizeof(struct kernelbootcp_message);
	msg = kmalloc(msg_size, GFP_KERNEL);
	msg->cmd = KERNEL_BOOTCP_VERIFY_VDSP;

	memcpy(msg->payload, table, sizeof(KBC_LOAD_TABLE_V));
	pr_debug("bootcp verify msg_size = %zd \n", msg_size);

	ret = bootcp_ca_write(msg, msg_size);
	if (!ret) {
		pr_err("bootcp_ca_write fail ret =%d\n", ret);
		mutex_unlock(&ca->wlock);
		kfree(msg);
		return ret;
	}

	ret = bootcp_ca_wait_ack(msg->cmd);

	mutex_unlock(&ca->wlock);

	pr_debug("bootcp ret =%d\n", ret);
	kfree(msg);
	return ret;

}
