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

#include "dcam_buf.h"
#include "isp_statis_buf.h"
#include <linux/sprd_iommu.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "DCAM_BUF: %d %d %s : " \
	fmt, current->pid, __LINE__, __func__

int dcam_statis_queue_init(struct isp_statis_buf_queue *queue)
{
	return isp_statis_queue_init(queue);
}

void dcam_statis_frm_queue_init(struct isp_statis_frm_queue *queue)
{
	isp_statis_frm_queue_init(queue);
}

int dcam_set_next_statis_buf(struct dcam_statis_module *module,
			     enum isp_3a_block_id block_index,
			     unsigned int *addr)
{
	int rtn = 0;
	struct isp_statis_frm_queue *statis_heap = NULL;
	struct isp_statis_buf_queue *p_buf_queue = NULL;
	struct isp_statis_buf *reserved_buf = NULL;
	struct isp_statis_buf node;

	memset(&node, 0, sizeof(struct isp_statis_buf));
	if (block_index == DCAM_AEM_BLOCK) {
		p_buf_queue = &module->aem_statis_queue;
		statis_heap = &module->aem_statis_frm_queue;
		reserved_buf = &module->aem_buf_reserved;
	} else if (block_index == DCAM_PDAF_BLOCK) {
		p_buf_queue = &module->pdaf_statis_queue;
		statis_heap = &module->pdaf_statis_frm_queue;
		reserved_buf = &module->pdaf_buf_reserved;
	} else if (block_index == DCAM_EBD_BLOCK) {
		p_buf_queue = &module->ebd_statis_queue;
		statis_heap = &module->ebd_statis_frm_queue;
		reserved_buf = &module->ebd_buf_reserved;
	} else if (block_index == DCAM_RAW_BLOCK) {
		p_buf_queue = &module->raw_statis_queue;
		statis_heap = &module->raw_statis_frm_queue;
		reserved_buf = &module->raw_buf_reserved;
	} else {
		return -EFAULT;
	}

	/* read buf addr from in_buf_queue */
	if (isp_statis_queue_read(p_buf_queue, &node) != 0) {
		DCAM_TRACE("statis NO free buf\n");
		/*use reserved buffer*/
		if (reserved_buf->pfinfo.mfd[0] == 0)
			pr_info("NO need to cfg statis buf block id %d\n",
				block_index);

		memcpy(&node, reserved_buf, sizeof(struct isp_statis_buf));
	}

	if (node.pfinfo.dev == NULL)
		pr_debug("dev is NULL.\n");

	rtn = isp_statis_enqueue(statis_heap, &node);
	if (unlikely(rtn)) {
		pr_err("fail to enqueue statis buf\n");
		return rtn;
	}

	*addr = node.phy_addr;

	return rtn;
}
