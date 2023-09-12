/*
 * SPDX-FileCopyrightText: 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
 * SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
 *
 * Copyright 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd.
 * Licensed under the Unisoc General Software License, version 1.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
 * Software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, either express or implied.
 * See the Unisoc General Software License, version 1.0 for more details.
 */

#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/scatterlist.h>

#include "sprd_vdsp_iommus.h"
#include "sprd_iommu_test_debug.h"
#include "sprd_vdsp_mem_test_debug.h"

#ifdef pr_fmt
#undef pr_fmt
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: [mem_debug]: %d %d %s: "\
        fmt, current->pid, __LINE__, __func__

static unsigned long debug_show_all_counut = 0;

static char *cmd_array[20] = {
	"                            ",	//0
	"XRP_IOCTL_ALLOC             ",	//1
	"XRP_IOCTL_FREE              ",	//2
	"XRP_IOCTL_QUEUE             ",	//3
	"XRP_IOCTL_QUEUE_NS          ",	//4
	"XRP_IOCTL_SET_DVFS          ",	//5
	"XRP_IOCTL_FACEID_CMD        ",	//6
	"XRP_IOCTL_SET_POWERHINT     ",	//7
	"XRP_IOCTL_MEM_QUERY         ",	//8
	"XRP_IOCTL_MEM_IMPORT        ",	//9
	"XRP_IOCTL_MEM_EXPORT        ",	//10 a
	"XRP_IOCTL_MEM_ALLOC         ",	//11 b
	"XRP_IOCTL_MEM_FREE          ",	//12 c
	"XRP_IOCTL_MEM_IOMMU_MAP     ",	//13 d
	"XRP_IOCTL_MEM_IOMMU_UNMAP   ",	//14 e
	"XRP_IOCTL_MEM_CPU_TO_DEVICE ",	//15 f
	"XRP_IOCTL_MEM_DEVICE_TO_CPU ",	//16 10
};

void debug_xvp_buf_print(struct xvp_buf *buf)
{

	pr_debug("-------------------------\n");

	pr_debug("xvp_buf->name      :%s\n", buf->name);
	pr_debug("xvp_buf->buf_id    :%d\n", buf->buf_id);
	pr_debug("xvp_buf->size      :%lld\n", buf->size);
	pr_debug("xvp_buf->heap_type :%d\n", buf->heap_type);
	pr_debug("xvp_buf->attr      :%d\n", buf->attributes);
	pr_debug("xvp_buf->vadd      :%#lx\n", (unsigned long)buf->vaddr);
	pr_debug("xvp_buf->iova      :%#llx\n", buf->iova);
	pr_debug("xvp_buf->owner     :%#lx\n", buf->owner);
	pr_debug("xvp_buf->buf_hnd   :%#llx\n", buf->buf_hnd);
}

void debug_buffer_print(struct buffer *buf)
{
	pr_debug("-------------------------\n");
	pr_debug("buffer->id    :%d\n", buf->id);
	pr_debug("buffer->request_size      :%zu\n", buf->request_size);
	pr_debug("buffer->actual_size :%zu\n", buf->actual_size);
	pr_debug("buffer->mapping      :%d\n", buf->mapping);
	pr_debug("buffer->mmu_idx      :%d\n", buf->mmu_idx);
	pr_debug("buffer->kptr      :%#lx\n", (unsigned long)buf->kptr);
	pr_debug("buffer->priv      :%#lx\n", (unsigned long)buf->priv);
	pr_debug("buffer->iova     :%#lx\n", buf->map_buf.iova);

}

void debug_xvp_buf_show_all(struct xvp *xvp)
{
	struct xvp_buf *buf = NULL;
	struct xvp_buf *temp = NULL;
	struct xvp_mem_dev *mem_dev = NULL;

	mem_dev = xvp->mem_dev;
	if (!mem_dev) {
		pr_err("Error: mem_dev is NULL");
		return;
	}
	pr_debug("=====debug_vxp_buf_show_all [start][%ld]=====\n",
		 debug_show_all_counut);
	mutex_lock(&mem_dev->buf_list_mutex);
	list_for_each_entry_safe(buf, temp, &mem_dev->buf_list, list_node) {
		debug_xvp_buf_print(buf);
	}
	mutex_unlock(&mem_dev->buf_list_mutex);
	pr_debug("=====debug_vxp_buf_show_all [end][%ld] =====\n",
		 debug_show_all_counut);
	debug_show_all_counut++;
	return;
};

void debug_xvpfile_buf_show_all(struct xvp_file *xvp_file)
{
	struct xvp_buf *buf = NULL;
	struct xvp_buf *temp = NULL;

	pr_debug("=====debug_vxpfile_buf_show_all [start]=====\n");
	list_for_each_entry_safe(buf, temp, &xvp_file->buf_list, xvp_file_list_node) {
		debug_xvp_buf_print(buf);
	}
	pr_debug("=====debug_vxpfile_buf_show_all [end] =====\n");
	return;
};

void debug_check_xvp_buf_leak(struct xvp_file *xvp_file)
{
	struct xvp_buf *buf = NULL;

	list_for_each_entry(buf, &xvp_file->buf_list, xvp_file_list_node) {
		debug_xvpfile_buf_show_all(xvp_file);
		BUG_ON(1);
	}
}

char *debug_get_ioctl_cmd_name(unsigned int cmd)
{
	unsigned int num = _IOC_NR(cmd);

	// pr_debug("IOC_NR:%d cmd name:%s\n",num,cmd_array[num]);
	return cmd_array[num];
}

void debug_print_xrp_ioctl_queue(struct xrp_ioctl_queue *q)
{

	pr_debug("================================\n");
	pr_debug("xrp_ioctl_queue->flags        =%d\n", q->flags);

	pr_debug("xrp_ioctl_queue->in_data_fd   =%d\n", q->in_data_fd);
	pr_debug("xrp_ioctl_queue->in_data_addr =%#llx\n", q->in_data_addr);
	pr_debug("xrp_ioctl_queue->in_data_size =%d\n", q->in_data_size);

	pr_debug("xrp_ioctl_queue->out_data_fd  =%d\n", q->out_data_fd);
	pr_debug("xrp_ioctl_queue->out_data_addr=%#llx\n", q->out_data_addr);
	pr_debug("xrp_ioctl_queue->out_data_size=%d\n", q->out_data_size);

	pr_debug("xrp_ioctl_queue->buffer_addr  =%#llx\n", q->buffer_addr);
	pr_debug("xrp_ioctl_queue->buffer_size  =%d\n", q->buffer_size);
	// pr_debug("xrp_ioctl_queue->nsid_addr    =%s\n"  ,(char *)q->nsid_addr);
}
