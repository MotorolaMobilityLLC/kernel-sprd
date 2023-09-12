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

#ifndef SPRD_VDSP_MEM_XVP_INIT_H
#define SPRD_VDSP_MEM_XVP_INIT_H

#include "xrp_internal.h"
#include "vdsp_hw.h"

/* node for heaps list */
struct xvp_heap {
	int id;
	struct list_head list_node;	/* Entry in <struct xvp_mem_dev:heaps> */
};

/* node for heaps list */
struct xvp_mem_dev {
	struct mem_ctx *xvp_mem_ctx;
	/* Available device memory heaps. List of <struct xvp_heap> */
	struct list_head heaps;
	struct list_head buf_list;
	struct mutex buf_list_mutex;
};

/* parameters to allocate a device buffer */
struct xvp_buf {
	char name[20];		/* [IN] short name for buffer               */
	uint64_t size;		/* [IN] Size of device memory (in bytes)    */
	uint32_t heap_type;	/* [IN] Heap type of allocator */
	uint32_t attributes;	/* [IN] Attributes of buffer */
	uint32_t buf_id;	/* [OUT] Generated buffer ID                */
	void __iomem *vaddr;
	phys_addr_t paddr;
	phys_addr_t iova;
	struct list_head list_node;
	struct list_head xvp_file_list_node;
	unsigned long owner;
	uint64_t buf_hnd;			// need modify later
	int isfixed;                // flag for fixed map, 0: no 1:fixed_offset 2:fixed_addr
	unsigned long fixed_data;   // fixed map addr or offset
} __attribute__ ((aligned(8)));

int sprd_vdsp_mem_xvp_init(struct xvp *xvp);
int sprd_vdsp_mem_xvp_release(struct xvp_mem_dev *xvp_mem_dev);
int xvp_mem_check_args(struct xvp *xvp, struct xvp_buf *xvp_buf,
	struct mem_ctx **mem_ctx);

// NOTE: for internal use
struct xvp_buf *__xvp_buf_creat(struct xvp *xvp, char *name, uint64_t size,
	uint32_t type, uint32_t attr);
int __xvp_buf_destroy(struct xvp *xvp, struct xvp_buf *xvp_buf);
int __xvp_buf_alloc(struct xvp *xvp, struct xvp_buf *xvp_buf);
int __xvp_buf_free(struct xvp *xvp, struct xvp_buf *xvp_buf);
int xvp_buf_kmap(struct xvp *xvp, struct xvp_buf *xvp_buf);
int xvp_buf_kunmap(struct xvp *xvp, struct xvp_buf *xvp_buf);

struct xvp_buf *xvp_buf_alloc(struct xvp *xvp, char *name, uint64_t size,
	uint32_t type, uint32_t attr);
int xvp_buf_free(struct xvp *xvp, struct xvp_buf *buf);
int xvp_buf_iommu_map(struct xvp *xvp, struct xvp_buf *xvp_buf);
int xvp_buf_iommu_unmap(struct xvp *xvp, struct xvp_buf *xvp_buf);
struct xvp_buf *xvp_buf_alloc_with_iommu(struct xvp *xvp, char *name,
	uint64_t size, uint32_t type,
	uint32_t attr);
int xvp_buf_free_with_iommu(struct xvp *xvp, struct xvp_buf *buf);

void *xvp_buf_get_vaddr(struct xvp_buf *buf);
phys_addr_t xvp_buf_get_iova(struct xvp_buf *buf);
void *xvp_buf_get_vaddr_by_id(struct xvp *xvp, uint32_t buf_id);
phys_addr_t xvp_buf_get_iova_by_id(struct xvp *xvp, uint32_t buf_id);

//functions by buf_id
struct xvp_buf *xvp_buf_get_by_id(struct xvp *xvp, uint32_t buf_id);
int xvp_buf_free_by_id(struct xvp *xvp, uint32_t buf_id);
int xvp_buf_iommu_map_by_id(struct xvp *xvp, uint32_t buf_id);
int xvp_buf_iommu_unmap_by_id(struct xvp *xvp, uint32_t buf_id);

#endif //SPRD_VDSP_MEM_XVP_INIT_H
