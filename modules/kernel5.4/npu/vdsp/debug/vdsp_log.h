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

#ifndef _VDSP_LOG_H_
#define _VDSP_LOG_H_

#include <linux/types.h>

#define VDSP_LINE_BUFFER_SIZE 256

struct xvp;

/*
 * Ring buffer that supports one secure producer thread and one
 * linux side consumer thread.
 */
struct log_rb {
	volatile uint32_t alloc;
	volatile uint32_t put;
	uint32_t sz;
	volatile char data[0];
} __packed;


#define LOG_OVERFLOW_MODE	0
#define LOG_WAIT_MODE		1

struct vdsp_log_work;

struct vdsp_log_state {
	/*
	 * This lock is here to ensure only one consumer will read
	 * from the log ring buffer at a time.
	 */
	spinlock_t lock;
	uint32_t get;

	void *log_vaddr;
	uint32_t log_ion_vdsp_addr;	// this is no use now
	struct workqueue_struct *nop_wq;
	struct vdsp_log_work __percpu *nop_works;
	struct list_head nop_queue;					// no use
	spinlock_t nop_lock;	/* protects nop_queue */ // no use now
	char line_buffer[VDSP_LINE_BUFFER_SIZE];
};

struct vdsp_log_work {
	struct vdsp_log_state *vls;
	struct work_struct work;
};

int vdsp_log_init(struct xvp *xvp);
int vdsp_log_deinit(struct xvp *xvp);
int vdsp_log_coredump(struct xvp *xvp);
int vdsp_log_buf_iommu_map(struct xvp *xvp);
int vdsp_log_buf_iommu_unmap(struct xvp *xvp);
#endif

