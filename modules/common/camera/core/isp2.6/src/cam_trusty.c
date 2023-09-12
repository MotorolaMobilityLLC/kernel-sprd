/*
 * Copyright (C) 2021-2022 UNISOC Communications Inc.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include  "cam_trusty.h"
#include  "dcam_hw_adpt.h"
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CAM_CA: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__


#ifdef CAM_FACEID_SEC
#include <linux/device.h>
#include <linux/trusty/trusty_ipc.h>

#define CAM_TA_PORT_NAME		"com.android.trusty.camera"

static struct cam_ca_ctrl cam_ca = {TIPC_CHANNEL_DISCONNECTED, 0, };

struct tipc_msg_buf *cam_ca_handle_msg(void *data,
				struct tipc_msg_buf *rxbuf, u16 flag)
{
	struct cam_ca_ctrl *ca = data;
	struct tipc_msg_buf *newbuf = rxbuf;

	mutex_lock(&ca->rlock);
	if (ca->chanel_state == TIPC_CHANNEL_CONNECTED) {
		/* get new buffer */
		newbuf = tipc_chan_get_rxbuf(ca->chan);
		if (!IS_ERR(newbuf)) {
			unsigned long flag = 0;

			pr_info("received new data, rxbuf %p, newbuf %p\n",
				rxbuf, newbuf);
			/* queue an old buffer and return a new one */
			spin_lock_irqsave(&ca->queue_lock, flag);
			list_add_tail(&rxbuf->node, &ca->rx_msg_queue);
			spin_unlock_irqrestore(&ca->queue_lock, flag);

			wake_up_interruptible(&ca->readq);
		} else {
			/*
			 * return an old buffer effectively discarding
			 * incoming message
			 */
			pr_info("discard incoming message\n");

			newbuf = rxbuf;
		}
	}
	mutex_unlock(&ca->rlock);

	return newbuf;
}

static void cam_ca_handle_event(void *data, int event)
{
	struct cam_ca_ctrl *ca = data;

	switch (event) {
	case TIPC_CHANNEL_SHUTDOWN:
		pr_info("channel shutdown\n");
		break;

	case TIPC_CHANNEL_DISCONNECTED:
		pr_info("channel disconnected\n");
		ca->chanel_state = TIPC_CHANNEL_DISCONNECTED;
		break;

	case TIPC_CHANNEL_CONNECTED:
		pr_info("channel connected\n");
		ca->chanel_state = TIPC_CHANNEL_CONNECTED;
		wake_up_interruptible(&ca->readq);
		break;

	default:
		pr_err("fail to handle unknown event type %d\n", event);
		break;
	}
}

static struct tipc_chan_ops cam_ca_ops = {
	.handle_msg = cam_ca_handle_msg,
	.handle_event = cam_ca_handle_event,
};

static void cam_ca_free_msg_buf_list(struct list_head *list)
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

bool cam_trusty_connect(void)
{
	struct cam_ca_ctrl *ca = &cam_ca;
	int chan_conn_ret = 0;
	bool ret = false;

	pr_info("chanel_state =%d\n", ca->chanel_state);

	if (ca->chanel_state == TIPC_CHANNEL_CONNECTED) {
		pr_info("cam has already been connected\n");
		return true;
	}

	if (!ca->con_init) {
		struct tipc_chan *chan;

		chan = tipc_create_channel(NULL, &cam_ca_ops, ca);
		if (IS_ERR(chan)) {
			pr_err("fail to create channel\n");
			return PTR_ERR(chan);
		}
		ca->chan = chan;
		mutex_init(&ca->rlock);
		mutex_init(&ca->wlock);
		init_waitqueue_head(&ca->readq);
		spin_lock_init(&ca->queue_lock);
		INIT_LIST_HEAD(&ca->rx_msg_queue);
		ca->con_init = true;
	}

	chan_conn_ret = tipc_chan_connect(ca->chan, CAM_TA_PORT_NAME);
	if (chan_conn_ret) {
		ret = false;
		pr_err("fail to connect channel\n");
		return ret;
	} else {
		ret = true;
		pr_info("cam connect channel done\n");
	}

	if (!wait_event_interruptible_timeout(ca->readq,
			(ca->chanel_state == TIPC_CHANNEL_CONNECTED),
			msecs_to_jiffies(CA_CONN_TIMEOUT))) {
		pr_err("fail to wait read response, time out!\n");
		ret = false;
	}

	pr_info("ret =%d\n", ret);

	return ret;
}

void cam_trusty_disconnect(void)
{
	struct cam_ca_ctrl *ca = &cam_ca;
	unsigned long flag = 0;

	wake_up_interruptible_all(&ca->readq);

	spin_lock_irqsave(&ca->queue_lock, flag);
	cam_ca_free_msg_buf_list(&ca->rx_msg_queue);
	spin_unlock_irqrestore(&ca->queue_lock, flag);
	ca->con_init = false;

	/* todo for Stability test
	 * tipc_chan_shutdown(ca->chan);
	 */

	ca->chanel_state = TIPC_CHANNEL_DISCONNECTED;

	/* todo for Stability test
	 * tipc_chan_destroy(ca->chan);
	 */

	pr_info("disconnect\n");
}

bool cam_ca_write(void *buf, size_t len)
{
	bool ret = true;
	int avail;
	struct tipc_msg_buf *txbuf = NULL;
	struct cam_ca_ctrl *ca = &cam_ca;
	int  msg_ret = 0;

	pr_info("ca write enter");

	if (ca->chanel_state != TIPC_CHANNEL_CONNECTED)
		return false;

	txbuf = tipc_chan_get_txbuf_timeout(ca->chan, GET_TXBUF_TIMEOUT);
	if (IS_ERR(txbuf))
		return PTR_ERR(txbuf);

	avail = mb_avail_space(txbuf);
	if (len > avail) {
		pr_err("fail to get buffer space, len = %d, avail = %d\n",
			(int)len, (int)avail);
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

ssize_t cam_ca_read(void *buf, size_t max_len)
{
	struct tipc_msg_buf *mb;
	ssize_t	len;
	struct cam_ca_ctrl *ca = &cam_ca;
	unsigned long flag = 0;
	int ret = 0;

	pr_info("ca read enter");

	if (ca->chanel_state != TIPC_CHANNEL_CONNECTED)
		return false;

	ret = wait_event_interruptible_timeout(ca->readq, !list_empty(&ca->rx_msg_queue), msecs_to_jiffies(CA_READ_TIMEOUT));
	if (ret <= 0) {
		pr_err("fail to wait read response, time out! ret = %d\n", ret);
		return -ETIMEDOUT;
	}

	spin_lock_irqsave(&ca->queue_lock, flag);
	mb = list_first_entry(&ca->rx_msg_queue, struct tipc_msg_buf, node);

	len = mb_avail_data(mb);
	if (len > max_len)
		len = max_len;

	memcpy(buf, mb_get_data(mb, len), len);

	list_del(&mb->node);
	spin_unlock_irqrestore(&ca->queue_lock, flag);
	tipc_chan_put_rxbuf(ca->chan, mb);

	return len;
}


bool cam_ca_wait_response(uint32_t cmd)
{
	ssize_t size;
	bool ret = true;
	struct status_message status_msg;

	pr_info("wait resp enter");

	size = cam_ca_read(&status_msg, sizeof(struct status_message));
	if (size != sizeof(struct status_message)) {
		pr_err("fail to get remote response size\n");
		return false;
	}

	if (status_msg.cmd == (cmd | TA_RESP_BIT)) {
		pr_err("fail to get remote ack_msg.cmd\n");
		return false;
	}

	if (status_msg.status ==  CAM_SECURITY) {
		pr_err("fail to get remote ack_msg.ack\n");
		return false;
	}

	pr_info("response ret = %d\n", ret);
	return true;
}


bool cam_ca_wait_ack(uint32_t cmd)
{
	ssize_t size;
	bool ret = true;
	struct ack_message ack_msg;

	pr_info("wait ack enter");

	size = cam_ca_read(&ack_msg, sizeof(struct ack_message));

	if (size != sizeof(struct ack_message)) {
		pr_err("fail to get remote response size, size= %zd\n", size);
		return false;
	}

	if (ack_msg.cmd != (cmd | TA_RESP_BIT)) {
		pr_err("fail to get remote ack_msg.cmd, ack_msg.cmd=0x%x, cmd=0x%x\n",
			ack_msg.cmd, cmd);
		return false;
	}

	if (ack_msg.ack ==  TA_CMD_ERR) {
		pr_err("fail to get remote ack_msg.ack, ack=0x%x\n", ack_msg.ack);
		return false;
	}

	return ret;

}

bool camca_get_status(void)
{
	struct  faceid_info_msg cam_trusty_status;

	bool ret = true;
	struct cam_ca_ctrl *ca = &cam_ca;

	pr_info("get status enter\n");

	mutex_lock(&ca->wlock);
	cam_trusty_status.msg_cmd = TA_GET_CAM_STATUS;
	ret = cam_ca_write(&cam_trusty_status, sizeof(struct faceid_info_msg));
	if (!ret) {
		pr_err("fail to write cam_ca ret =%d\n", ret);
		mutex_unlock(&ca->wlock);
		return ret;
	}

	ret =  cam_ca_wait_response(cam_trusty_status.msg_cmd);
	if (!ret) {
		pr_err("fail to wait cam_ca response ret =%d\n", ret);
		mutex_unlock(&ca->wlock);
		return ret;
	}

	cam_ca.cam_temode = cam_trusty_status.sec_attr;
	mutex_unlock(&ca->wlock);

	pr_info("cam_temode=%d, ret =%d\n", cam_ca.cam_temode, ret);

	return cam_ca.cam_temode;
}


bool cam_trusty_isp_fetch_addr_set(unsigned long y_addr, unsigned long u_addr,
	unsigned long v_addr)
{
	struct img_yuv_reg_msg  yuv_addr_set;
	bool ret = true;
	struct cam_ca_ctrl *ca = &cam_ca;

	pr_info("ca set isp fetch addr");

	mutex_lock(&ca->wlock);

	yuv_addr_set.msg_cmd = TA_IMG_ISP_YUV_ADDR_SET;

	yuv_addr_set.y_addr = y_addr;
	yuv_addr_set.u_addr = u_addr;
	yuv_addr_set.v_addr = v_addr;

	ret = cam_ca_write(&yuv_addr_set, sizeof(struct img_yuv_reg_msg));
	if (!ret) {
		pr_err("fail to write cam_ca, ret =%d\n", ret);
		mutex_unlock(&ca->wlock);
		return ret;
	}

	ret =  cam_ca_wait_ack(yuv_addr_set.msg_cmd);
	mutex_unlock(&ca->wlock);

	pr_info("ca set isp fetch addr ret =%d\n", ret);
	return ret;

}

bool cam_trusty_isp_pitch_set(uint32_t y_pitch, uint32_t u_pitch, uint32_t v_pitch)
{
	struct img_pitch_reg_msg  pitch_set;
	bool ret = true;
	struct cam_ca_ctrl *ca = &cam_ca;

	pr_info("enter\n");

	mutex_lock(&ca->wlock);

	pitch_set.msg_cmd = TA_IMG_ISP_PITCH_SET;

	pitch_set.y_pitch = y_pitch;
	pitch_set.u_pitch = u_pitch;
	pitch_set.v_pitch = v_pitch;

	ret = cam_ca_write(&pitch_set, sizeof(struct img_pitch_reg_msg));
	if (!ret) {
		pr_err("fail to write cam_ca, ret = %d\n", ret);
		mutex_unlock(&ca->wlock);
		return ret;
	}

	ret =  cam_ca_wait_ack(pitch_set.msg_cmd);
	mutex_unlock(&ca->wlock);

	pr_info("exit, ret =%d\n", ret);
	return ret;
}

bool cam_trusty_isp_3dnr_fetch_set(unsigned long chroma, unsigned long luma, uint32_t pitch)
{
	struct isp_3dnr_reg_msg  isp_3dnr_pitch_set;
	bool ret = true;
	struct cam_ca_ctrl *ca = &cam_ca;

	pr_info("isp_3dnr_fetch_set");

	mutex_lock(&ca->wlock);

	isp_3dnr_pitch_set.msg_cmd = TA_IMG_3DNR_PITCH_SET;

	isp_3dnr_pitch_set.ft_chroma_addr = chroma;
	isp_3dnr_pitch_set.ft_luma_addr = luma;
	isp_3dnr_pitch_set.ft_pitch = pitch;

	ret = cam_ca_write(&isp_3dnr_pitch_set, sizeof(struct
		isp_3dnr_reg_msg));
	if (!ret) {
		pr_err("fail to write cam_ca, ret =%d\n", ret);
		mutex_unlock(&ca->wlock);
		return ret;
	}

	ret =  cam_ca_wait_ack(isp_3dnr_pitch_set.msg_cmd);
	mutex_unlock(&ca->wlock);

	pr_info("isp_3dnr_fetch_set, ret =%d\n", ret);

	return ret;
}

bool cam_trusty_csi_switch_ctrl_set(uint32_t csi_sel_ctrl)
{
	struct csi_switch_ctrl_msg  switch_ctrl;
	bool ret = true;
	struct cam_ca_ctrl *ca = &cam_ca;

	pr_info("enter\n");

	mutex_lock(&ca->wlock);
	switch_ctrl.msg_cmd = TA_IMG_CSI_SWICH_CTRL_SET;

	switch_ctrl.csi_sel_ctrl = csi_sel_ctrl;

	ret = cam_ca_write(&switch_ctrl, sizeof(struct csi_switch_ctrl_msg));
	if (!ret) {
		pr_err("fail to write cam_ca ret =%d\n", ret);
		mutex_unlock(&ca->wlock);
		return ret;
	}

	ret =  cam_ca_wait_ack(switch_ctrl.msg_cmd);
	mutex_unlock(&ca->wlock);

	pr_info("ret =%d\n", ret);
	return ret;

}


bool camca_enter_tamode(struct sprd_cam_sec_cfg *camsec_cfg)
{
	bool ret = true;
	struct  faceid_cfg_msg faceid_msg;
	struct cam_ca_ctrl *ca = &cam_ca;

	pr_info("ca chanel_state =%d, camsec_mode=%d, work_mode=%d",
		ca->chanel_state, camsec_cfg->camsec_mode,
		camsec_cfg->work_mode);

	if (ca->chanel_state != TIPC_CHANNEL_CONNECTED) {
		pr_err("fail to enter ta mode, con err\n");
		return false;
	}

	mutex_lock(&ca->wlock);
	faceid_msg.msg_cmd = TA_FACEID_ENTER_TEMODE;
	faceid_msg.faceid_version = FACEID_VERSION;
	faceid_msg.sec_cfg.work_mode = camsec_cfg->work_mode;
	faceid_msg.sec_cfg.camsec_mode = camsec_cfg->camsec_mode;

	ret = cam_ca_write(&faceid_msg, sizeof(struct faceid_cfg_msg));
	if (!ret) {
		pr_err("fail to write cam_ca ret =%d\n", ret);
		mutex_unlock(&ca->wlock);
		return ret;
	}

	ret =  cam_ca_wait_ack(faceid_msg.msg_cmd);

	mutex_unlock(&ca->wlock);

	pr_info("ret =%d\n", ret);

	return ret;

}

bool camca_exit_tamode(struct sprd_cam_sec_cfg *camsec_cfg)
{
	bool ret = true;
	struct cam_ca_ctrl *ca = &cam_ca;
	struct  faceid_cfg_msg faceid_msg;

	pr_info("ca chanel_state =%d, camsec_mode=%d, work_mode=%d",
		ca->chanel_state, camsec_cfg->camsec_mode, camsec_cfg->work_mode);

	mutex_lock(&ca->wlock);
	faceid_msg.msg_cmd = TA_FACEID_EXIT_TEMODE;
	faceid_msg.faceid_version = FACEID_VERSION;
	faceid_msg.sec_cfg.work_mode = camsec_cfg->work_mode;
	faceid_msg.sec_cfg.camsec_mode = camsec_cfg->camsec_mode;

	ret = cam_ca_write(&faceid_msg, sizeof(struct faceid_cfg_msg));
	if (!ret) {
		pr_err("fail to write cam_ca ret =%d\n", ret);
		mutex_unlock(&ca->wlock);
		return ret;
	}

	ret =  cam_ca_wait_ack(faceid_msg.msg_cmd);
	mutex_unlock(&ca->wlock);

	pr_info("ret =%d\n", ret);

	return ret;

}

bool cam_trusty_security_set(struct sprd_cam_sec_cfg *camsec_cfg,
	enum cam_trusty_mode mode)
{
	bool ret = true;

	pr_info("camca security set enter");

	switch (mode) {
	case CAM_TRUSTY_ENTER:
		ret = camca_enter_tamode(camsec_cfg);
		break;
	case CAM_TRUSTY_EXIT:
		ret = camca_exit_tamode(camsec_cfg);
		break;
	default:
		pr_err("fail to get valid mode %d\n", mode);
		break;
	}

	pr_info("camca security set, ret=%d",  ret);
	return ret;
}
#else

/* add dummy fun for un trusty version */
bool cam_trusty_connect(void)
{
	pr_info("no trusty version\n");
	return 1;
}

void cam_trusty_disconnect(void)
{
	pr_info("no trusty version\n");
}

bool cam_trusty_isp_fetch_addr_set(unsigned long y_addr,
	unsigned long u_addr, unsigned long v_addr)
{
	pr_info("no trusty version\n");
	return 1;
}

bool cam_trusty_isp_pitch_set(uint32_t y_pitch, uint32_t u_pitch,
	uint32_t v_pitch)
{
	pr_info("no trusty version\n");
	return 1;
}

bool cam_trusty_isp_3dnr_fetch_set(unsigned long chroma, unsigned long luma,
	uint32_t pitch)
{
	pr_info("no trusty version\n");
	return 1;
}

bool cam_trusty_csi_switch_ctrl_set(uint32_t csi_sel_ctrl)
{
	pr_info("no trusty version\n");
	return 1;
}

bool cam_trusty_security_set(struct sprd_cam_sec_cfg *camsec_cfg,
	enum cam_trusty_mode mode)
{
	pr_info("no trusty version\n");
	return 1;
}
#endif
