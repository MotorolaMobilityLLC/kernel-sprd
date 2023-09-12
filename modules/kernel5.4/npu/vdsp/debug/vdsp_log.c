/*
* SPDX-FileCopyrightText: 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd
* SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
*
* Copyright 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd.
* Licensed under the Unisoc General Software License, version 1.0 (the License);
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
* Software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* See the Unisoc General Software License, version 1.0 for more details.
*/

#include <linux/notifier.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include "xrp_internal.h"
#include "vdsp_log.h"
#include "vdsp_dump.h"
#include "sprd_vdsp_mem_xvp_init.h"

#define VDSP_LOG_BUFFER_SZIE (1024*128)
#define BANK_BUSY 0
#define BANK_READY 1

#define COREDUMP_NONE		(0)
#define COREDUMP_START		(1)
#define COREDUMP_STACK_DONE	(2)
#define COREDUMP_FINISH		(3)

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "sprd-vdsp: log %d %d : "\
        fmt, current->pid, __LINE__

struct log_header {
	volatile int32_t mode;	// 0:not wait,1:wait
	volatile int32_t bank_size;
	volatile int32_t flag[2];	//flag, write disable/enable
	volatile uint32_t addr[2];	//log bank address
	volatile uint32_t log_size[2];
	volatile uint32_t log_level;
	volatile uint32_t coredump_flag;
	volatile uint32_t reverse[6];
};

static int log_read_line(struct vdsp_log_state *s, int put, int get, char *addr)
{
	int i;
	char c = '\0';
	size_t max_to_read = min((size_t) (put - get), sizeof(s->line_buffer) - 1);

	for (i = 0; i < max_to_read && c != '\n';)
		s->line_buffer[i++] = c = addr[get++];
	s->line_buffer[i] = '\0';
	return i;
}

static void vdsp_dump_logs(struct vdsp_log_state *s)
{
	uint32_t put, get, bank;
	char *addr;
	int read_chars;
	struct log_header *log = (struct log_header *)s->log_vaddr;

	if (BANK_READY == log->flag[0]) {
		bank = 0;
		addr = (char *)s->log_vaddr + sizeof(struct log_header);
	} else if (BANK_READY == log->flag[1]) {
		bank = 1;
		addr = (char *)s->log_vaddr + sizeof(struct log_header) + log->bank_size;
	} else {
		pr_err("bank is busy\n");
		return;
	}
	get = s->get;
	put = log->log_size[bank];
	pr_debug("addr %p, log size %d\n", addr, put);
	while (put != get) {
		/* Make sure that the read of put occurs before the read of log data */
		rmb();

		/* Read a line from the log */
		read_chars = log_read_line(s, put, get, addr);

		/* Force the loads from log_read_line to complete. */
		rmb();

		pr_info("%s", s->line_buffer);
		get += read_chars;
	}
	log->flag[bank] = BANK_BUSY;
	return;
}

irqreturn_t vdsp_log_irq_handler(int irq, void *private)
{
	struct xvp *xvp = (struct xvp *)private;
	struct vdsp_log_state *s;
	struct vdsp_log_work *vlw;

	s = xvp->log_state;
	if (s) {
		preempt_disable();
		vlw = this_cpu_ptr(s->nop_works);
		queue_work(s->nop_wq, &vlw->work);
		preempt_enable();
	}

	return IRQ_HANDLED;
}

EXPORT_SYMBOL(vdsp_log_irq_handler);

static void vdsp_log_nop_work_func(struct work_struct *work)
{
	struct vdsp_log_state *s;
	struct vdsp_log_work *vlw = container_of(work, struct vdsp_log_work, work);
	unsigned long flags;

	s = vlw->vls;
	if (s) {
		spin_lock_irqsave(&s->lock, flags);
		vdsp_dump_logs(s);
		spin_unlock_irqrestore(&s->lock, flags);
	}
}

int vdsp_log_alloc_buffer(struct xvp *xvp)
{
	struct vdsp_log_state *s = xvp->log_state;
	char *name = NULL;
	uint64_t size = 0;
	uint32_t heap_type = 0;
	uint32_t attr = 0;
	struct xvp_buf *buf = NULL;

	name = "xvp log buffer";
	size = VDSP_LOG_BUFFER_SZIE;	//128k
	heap_type = SPRD_VDSP_MEM_HEAP_TYPE_UNIFIED;
	attr = SPRD_VDSP_MEM_ATTR_WRITECOMBINE;
	buf = xvp_buf_alloc(xvp, name, size, heap_type, attr);
	if (!buf) {
		pr_err("Error:xvp_buf_alloc failed\n");
		return -1;
	}
	xvp->log_buf = buf;
	xvp_buf_kmap(xvp, xvp->log_buf);
	s->log_vaddr = xvp_buf_get_vaddr(xvp->log_buf);

	pr_debug("log buffer vdsp va:%p\n", xvp_buf_get_vaddr(xvp->log_buf));

	return 0;
}

int vdsp_log_free_buffer(struct xvp *xvp)
{
	struct vdsp_log_state *s = xvp->log_state;

	xvp_buf_kunmap(xvp, xvp->log_buf);
	if (xvp_buf_free(xvp, xvp->log_buf)) {
		pr_err("Error:xvp_buf_free faild\n");
		return -1;
	}
	xvp->log_buf = NULL;
	s->log_vaddr = NULL;
	return 0;
}

int vdsp_log_buf_iommu_map(struct xvp *xvp)
{
	struct log_header *log;
	unsigned long vdsp_log_iova;

	log = (struct log_header *)xvp->log_state->log_vaddr;
	if (xvp_buf_iommu_map(xvp, xvp->log_buf)) {
		return -1;
	}
	vdsp_log_iova = xvp_buf_get_iova(xvp->log_buf);
	log->addr[0] = vdsp_log_iova + sizeof(struct log_header);
	log->addr[1] = vdsp_log_iova + sizeof(struct log_header) + log->bank_size;
	log->flag[0] = 0;	//init log flag, BANK_BUSY
	log->flag[1] = 0;
	log->log_size[0] = 0;	//vdsp write log size
	log->log_size[1] = 0;
	pr_debug("log addr map, 0x%x-0x%x", log->addr[0], log->addr[1]);
	return 0;
}

int vdsp_log_buf_iommu_unmap(struct xvp *xvp)
{
	struct log_header *log;

	if (xvp_buf_iommu_unmap(xvp, xvp->log_buf)) {
		return -1;
	}
	log = (struct log_header *)xvp->log_state->log_vaddr;
	log->addr[0] = 0;
	log->addr[1] = 0;
	return 0;
}

int vdsp_log_init(struct xvp *xvp)
{
	struct vdsp_log_state *s;
	int result;
	struct log_header *log;
	unsigned int cpu;

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s) {
		result = -ENOMEM;
		goto error_alloc_state;
	}
	xvp->log_state = s;

	spin_lock_init(&s->lock);
	s->get = 0;
	s->nop_wq = alloc_workqueue("vdsplog-nop-wq", WQ_CPU_INTENSIVE, 0);
	if (!s->nop_wq) {
		result = -ENODEV;
		pr_err("Failed create vdsplog-nop-wq\n");
		goto err_create_nop_wq;
	}

	s->nop_works = alloc_percpu(struct vdsp_log_work);

	if (!s->nop_works) {
		result = -ENOMEM;
		pr_err("Failed to allocate works\n");
		goto err_alloc_works;
	}

	for_each_possible_cpu(cpu) {
		struct vdsp_log_work *vlw = per_cpu_ptr(s->nop_works, cpu);

		vlw->vls = s;
		INIT_WORK(&vlw->work, vdsp_log_nop_work_func);
	}

	if (vdsp_log_alloc_buffer(xvp) != 0) {
		result = -ENOMEM;
		goto error_alloc_log;
	}

	log = (struct log_header *)s->log_vaddr;
	log->bank_size = (VDSP_LOG_BUFFER_SZIE - sizeof(struct log_header)) / 2;

	log->mode = LOG_OVERFLOW_MODE;	//wait or ignore when log full
	log->flag[0] = 0;	//init log flag, BANK_BUSY
	log->flag[1] = 0;
	log->addr[0] = 0;
	log->addr[1] = 0;
	log->log_size[0] = 0;	//vdsp write log size
	log->log_size[1] = 0;
	log->log_level = 5;	//vdsp log level
	log->coredump_flag = COREDUMP_NONE;

	pr_debug("log header addr:%p, bank addr:%x,%x, bank_size %d\n",
		 s->log_vaddr, log->addr[0], log->addr[1], log->bank_size);
	return 0;

error_alloc_log:
	for_each_possible_cpu(cpu) {
		struct vdsp_log_work *vlw = per_cpu_ptr(s->nop_works, cpu);

		flush_work(&vlw->work);
	}
	free_percpu(s->nop_works);
err_alloc_works:
	destroy_workqueue(s->nop_wq);
err_create_nop_wq:
	kfree(s);
error_alloc_state:
	return result;
}

int vdsp_log_deinit(struct xvp *xvp)
{
	struct vdsp_log_state *s;
	unsigned int cpu;

	s = xvp->log_state;
	if (s) {
		for_each_possible_cpu(cpu) {
			struct vdsp_log_work *vlw = per_cpu_ptr(s->nop_works, cpu);

			flush_work(&vlw->work);
		}
		free_percpu(s->nop_works);
		destroy_workqueue(s->nop_wq);
		vdsp_log_free_buffer(xvp);
		kfree(s);
		xvp->log_state = NULL;
	}

	return 0;
}

int vdsp_log_coredump(struct xvp *xvp)
{
	int res = -1;
	int dump_res = -1;
	struct log_header *log = (struct log_header *)xvp->log_state->log_vaddr;
	unsigned long deadline = jiffies + 5 * HZ;	//5s

	pr_debug("dump start,flag:%d \n", log->coredump_flag);
	if (log->coredump_flag == COREDUMP_START) {
		dump_res = xrp_dump_libraries(xvp);
		log->coredump_flag = COREDUMP_STACK_DONE;
		do {
			if (log->coredump_flag == COREDUMP_FINISH) {
				res = 0;
				break;
			}
			schedule();
		}
		while (time_before(jiffies, deadline));
	}
	log->coredump_flag = COREDUMP_NONE;
	pr_debug("dump end, process(-1/0):%d, data(-1/0):%d\n", res, dump_res);
	return res;
}
