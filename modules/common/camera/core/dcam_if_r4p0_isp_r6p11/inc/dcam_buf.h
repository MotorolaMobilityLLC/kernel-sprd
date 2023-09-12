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

#ifndef _DCAM_BUF_HEADER_
#define _DCAM_BUF_HEADER_

#include "isp_drv.h"
#include "isp_buf.h"

extern struct platform_device *s_dcam_pdev;

struct dcam_statis_module {
	struct isp_statis_frm_queue aem_statis_frm_queue;
	uint32_t aem_statis_cnt;
	struct isp_statis_buf aem_buf_reserved;
	struct isp_statis_buf_queue aem_statis_queue; /*for irq read*/

	struct isp_statis_frm_queue pdaf_statis_frm_queue;
	uint32_t pdaf_statis_cnt;
	struct isp_statis_buf pdaf_buf_reserved;
	struct isp_statis_buf_queue pdaf_statis_queue; /*for irq read*/

	struct isp_statis_frm_queue ebd_statis_frm_queue;
	uint32_t ebd_statis_cnt;
	struct isp_statis_buf ebd_buf_reserved;
	struct isp_statis_buf_queue ebd_statis_queue; /*for irq read*/

	struct isp_statis_frm_queue raw_statis_frm_queue;
	uint32_t raw_statis_cnt;
	struct isp_statis_buf raw_buf_reserved;
	struct isp_statis_buf_queue raw_statis_queue; /*for irq read*/
	struct isp_statis_buf img_statis_buf;
};

int dcam_statis_queue_init(struct isp_statis_buf_queue *queue);
void dcam_statis_frm_queue_init(struct isp_statis_frm_queue *queue);
int dcam_set_next_statis_buf(struct dcam_statis_module *module,
			     enum isp_3a_block_id block_index,
			     uint32_t *addr);
#endif
