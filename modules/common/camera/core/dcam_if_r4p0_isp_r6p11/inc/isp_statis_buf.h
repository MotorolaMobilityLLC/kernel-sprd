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

#ifndef _ISP_STATIS_BUF_HEADER_
#define _ISP_STATIS_BUF_HEADER_

#include <linux/kfifo.h>
#include "isp_drv.h"
#include "isp_cfg.h"
#include "sprd_isp_hw.h"
#include "dcam_buf.h"

struct isp_queue_param {
	struct isp_statis_buf *frm_statis;
	struct isp_statis_buf_queue *statis_queue;
	struct isp_statis_buf *buf_reserved;
};

extern struct platform_device *s_isp_pdev;
int isp_statis_queue_init(struct isp_statis_buf_queue *queue);
int isp_statis_queue_read(struct isp_statis_buf_queue *queue,
	struct isp_statis_buf *buf);
void isp_statis_frm_queue_init(struct isp_statis_frm_queue *queue);
int sprd_isp_cfg_statis_buf(struct isp_pipe_dev *dev,
	struct dcam_statis_module *dcam_module,
	struct isp_statis_buf_input *parm);
int isp_statis_queue_write(struct isp_statis_buf_queue *queue,
	struct isp_statis_buf *buf);
int sprd_isp_set_statis_addr(struct isp_pipe_dev *dev,
	struct dcam_statis_module *dcam_module,
	struct isp_statis_buf_input *parm);
int isp_statis_enqueue(struct isp_statis_frm_queue *queue,
	struct isp_statis_buf *frame);
int isp_statis_dequeue(struct isp_statis_frm_queue *queue,
	struct isp_statis_buf *frame);
#endif
