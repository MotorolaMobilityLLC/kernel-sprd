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

#ifndef SPRD_VDSP_MEM_XVPFILE_H
#define SPRD_VDSP_MEM_XVPFILE_H

#include "xrp_internal.h"
#include "xvp_main.h"
#include "sprd_vdsp_mem_xvp_init.h"

int xvpfile_buf_init(struct xvp_file *xvp_file);
int xvpfile_buf_deinit(struct xvp_file *xvp_file);

struct xvp_buf *xvpfile_buf_alloc(struct xvp_file *xvp_file, char *name,
	uint64_t size, uint32_t type, uint32_t attr);
int xvpfile_buf_free(struct xvp_file *xvp_file, struct xvp_buf *buf);
int xvpfile_buf_kmap(struct xvp_file *xvp_file, struct xvp_buf *buf);
int xvpfile_buf_kunmap(struct xvp_file *xvp_file, struct xvp_buf *buf);
int xvpfile_buf_iommu_map(struct xvp_file *xvp_file, struct xvp_buf *xvp_buf);
int xvpfile_buf_iommu_unmap(struct xvp_file *xvp_file, struct xvp_buf *xvp_buf);
struct xvp_buf *xvpfile_buf_alloc_with_iommu(struct xvp_file *xvp_file,
	char *name, uint64_t size,
	uint32_t type, uint32_t attr);
int xvpfile_buf_free_with_iommu(struct xvp_file *xvp_file, struct xvp_buf *buf);

struct xvp_buf *xvpfile_buf_get(struct xvp_file *xvp_file, uint32_t buf_id);
void *xvpfile_buf_get_vaddr(struct xvp_buf *buf);
phys_addr_t xvpfile_buf_get_iova(struct xvp_buf *buf);

struct xvp_buf *xvpfile_buf_import(struct xvp_file *xvp_file, char *name,
	uint64_t size, uint32_t heap_id, uint32_t attr, uint64_t buf_fd, uint64_t cpu_ptr);
int xvpfile_buf_export(struct xvp_file *xvp_file, struct xvp_buf *buf);

#endif //SPRD_VDSP_MEM_XVPFILE_H
