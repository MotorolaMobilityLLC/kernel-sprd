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

#ifndef SPRD_VDSP_MEM_MAN_UAPI_H
#define SPRD_VDSP_MEM_MAN_UAPI_H

/* memory attributes */
enum sprd_vdsp_mem_attr {
	SPRD_VDSP_MEM_ATTR_CACHED = 0x00000001,
	SPRD_VDSP_MEM_ATTR_UNCACHED = 0x00000002,
	SPRD_VDSP_MEM_ATTR_WRITECOMBINE = 0x00000004,

	/* Special */
	SPRD_VDSP_MEM_ATTR_SECURE = 0x00000010,
	SPRD_VDSP_MEM_ATTR_NOMAP = 0x00000020,
	SPRD_VDSP_MEM_ATTR_NOSYNC = 0x00000040,

	/* Internal */
	SPRD_VDSP_MEM_ATTR_MMU = 0x10000000,
};

/* Cache attributes mask */
#define SPRD_VDSP_MEM_ATTR_CACHE_MASK 0xf

/* Supported heap types */
enum sprd_vdsp_mem_heap_type {
	SPRD_VDSP_MEM_HEAP_TYPE_UNKNOWN = 0,
	SPRD_VDSP_MEM_HEAP_TYPE_UNIFIED,
	SPRD_VDSP_MEM_HEAP_TYPE_ION,
	SPRD_VDSP_MEM_HEAP_TYPE_DMABUF,
	SPRD_VDSP_MEM_HEAP_TYPE_ANONYMOUS,
	SPRD_VDSP_MEM_HEAP_TYPE_CARVEOUT,
	SPRD_VDSP_MEM_HEAP_TYPE_MAX,
};

/* Heap attributes */
enum sprd_vdsp_mem_heap_attrs {
	SPRD_VDSP_MEM_HEAP_ATTR_INTERNAL = 0x01,
	SPRD_VDSP_MEM_HEAP_ATTR_IMPORT = 0x02,
	SPRD_VDSP_MEM_HEAP_ATTR_EXPORT = 0x04,
	SPRD_VDSP_MEM_HEAP_ATTR_SEALED = 0x08,
	/* Not used */
	SPRD_VDSP_MEM_HEAP_ATTR_USER = 0x10,
};

/* heaps ids */
#define SPRD_VDSP_MEM_MAN_HEAP_ID_INVALID 0
#define SPRD_VDSP_MEM_MAN_MIN_HEAP 1
#define SPRD_VDSP_MEM_MAN_MAX_HEAP 16

/* buffer ids (per memory context) */
#define SPRD_VDSP_MEM_MAN_BUF_ID_INVALID 0
#define SPRD_VDSP_MEM_MAN_MIN_BUFFER 1
#define SPRD_VDSP_MEM_MAN_MAX_BUFFER 2000

/* Virtual memory space for buffers allocated
 * in the kernel - device debug buffers only */
#define SPRD_VDSP_MEM_VA_HEAP1_BASE 0x0ULL
#define SPRD_VDSP_MEM_VA_HEAP1_SIZE 0x40000000ULL

/* Virtual memory space for buffers allocated in the user space */
#define SPRD_VDSP_MEM_VA_HEAP2_BASE (SPRD_VDSP_MEM_VA_HEAP1_BASE + SPRD_VDSP_MEM_VA_HEAP1_SIZE)
#define SPRD_VDSP_MEM_VA_HEAP2_SIZE 0x3C0000000ULL

#endif /* SPRD_VDSP_MEM_MAN_UAPI_H */
