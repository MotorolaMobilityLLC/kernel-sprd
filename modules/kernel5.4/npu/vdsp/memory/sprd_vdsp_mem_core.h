/**
 * Copyright (C) 2021-2022 UNISOC (Shanghai) Technologies Co.,Ltd.
 */
/*****************************************************************************
 *
 * Copyright (c) Imagination Technologies Ltd.
 *
 * The contents of this file are subject to the MIT license as set out below.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the terms of the
 * GNU General Public License Version 2 ("GPL")in which case the provisions of
 * GPL are applicable instead of those above.
 *
 * If you wish to allow use of your version of this file only under the terms
 * of GPL, and not to allow others to use your version of this file under the
 * terms of the MIT license, indicate your decision by deleting the provisions
 * above and replace them with the notice and other provisions required by GPL
 * as set out in the file called "GPLHEADER" included in this distribution. If
 * you do not delete the provisions above, a recipient may use your version of
 * this file under the terms of either the MIT license or GPL.
 *
 * This License is also included in this distribution in the file called
 * "MIT_COPYING".
 *
 *****************************************************************************/
/*
 * This file has been modified by UNISOC to adapt vdsp driver to call.
 */

#ifndef SPRD_VDSP_MEM_MAN_H
#define SPRD_VDSP_MEM_MAN_H

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
#define KERNEL_DMA_FENCE_SUPPORT
#endif

#include <linux/mm.h>
#include <linux/types.h>
#include <linux/device.h>
#ifdef KERNEL_DMA_FENCE_SUPPORT
#include <linux/dma-fence.h>
#endif
#include "sprd_vdsp_mem_core_uapi.h"

union heap_options {
	struct {
		gfp_t gfp_type;	/* pool and flags for buffer allocations */
		int min_order;	/* minimum page allocation order */
		int max_order;	/* maximum page allocation order */
	} unified;

#ifdef CONFIG_ION
	struct {
		struct ion_client *client;	/* must be provided by platform */
	} ion;
#endif
#ifdef CONFIG_GENERIC_ALLOCATOR
	struct {
		void *kptr; /* static pointer to kernel mapping of memory */
		/* Optional hooks to obtain kernel mapping dynamically */
		void *(*get_kptr) (phys_addr_t addr, size_t size, enum sprd_vdsp_mem_attr mattr);
		int (*put_kptr) (void *);
		phys_addr_t phys;		/* physical address start of memory */
		size_t size;			/* size of memory */
		unsigned long offs;	/* optional offset of the start of memory as seen from device, zero by default */
		int pool_order;		/* allocation order */
	} carveout;
#endif
	struct {
		bool use_sg_dma;  /* Forces sg_dma physical address instead of CPU physical address*/
	} dmabuf;

	struct {
		gfp_t gfp_flags;		/* for buffer allocations */
	} coherent;

	struct {
		phys_addr_t phys;		/* physical address start of memory */
		size_t size;			/* size of memory */
	} ocm;
};

struct heap_config {
	enum sprd_vdsp_mem_heap_type type;
	union heap_options options;
	/* (optional) functions to convert a physical address as seen from
	   the CPU to the physical address as seen from the vha device and
	   vice versa. When not implemented,
	   it is assumed that physical addresses are the
	   same regardless of viewpoint */
	phys_addr_t(*to_dev_addr) (union heap_options *opts, phys_addr_t addr);
	phys_addr_t(*to_host_addr) (union heap_options *opts, phys_addr_t addr);
	/* Cache attribute,
	 * could be platform specific if provided - overwrites the global cache policy */
	enum sprd_vdsp_mem_attr cache_attr;
};

struct mem_ctx;

int sprd_vdsp_mem_core_init(void);
void sprd_vdsp_mem_core_exit(void);

int sprd_vdsp_mem_add_heap(const struct heap_config *heap_cfg, int *heap_id);
void sprd_vdsp_mem_del_heap(int heap_id);
int sprd_vdsp_mem_get_heap_info(int heap_id, uint8_t *type, uint32_t *attrs);
int sprd_vdsp_mem_get_heap_id(enum sprd_vdsp_mem_heap_type type);

/*
*  related to process context (contains SYSMEM heap's functionality in general)
*/

int sprd_vdsp_mem_create_proc_ctx(struct mem_ctx **ctx);
void sprd_vdsp_mem_destroy_proc_ctx(struct mem_ctx *ctx);

int sprd_vdsp_mem_alloc(struct device *device, struct mem_ctx *ctx, int heap_id,
	size_t size, enum sprd_vdsp_mem_attr attributes, int *buf_id);
int sprd_vdsp_mem_import(struct device *device, struct mem_ctx *ctx,
	int heap_id, size_t size, enum sprd_vdsp_mem_attr attr, uint64_t buf_fd,
	uint64_t cpu_ptr, int *buf_id);
int sprd_vdsp_mem_export(struct device *device, struct mem_ctx *ctx, int buf_id,
	size_t size, enum sprd_vdsp_mem_attr attributes,
	uint64_t *buf_hnd);
void sprd_vdsp_mem_free(struct mem_ctx *ctx, int buf_id);

int sprd_vdsp_mem_map_um(struct mem_ctx *ctx, int buf_id,
	struct vm_area_struct *vma);
int sprd_vdsp_mem_unmap_um(struct mem_ctx *ctx, int buf_id);
int sprd_vdsp_mem_map_km(struct mem_ctx *ctx, int buf_id);
int sprd_vdsp_mem_unmap_km(struct mem_ctx *ctx, int buf_id);
void *sprd_vdsp_mem_get_kptr(struct mem_ctx *ctx, int buf_id);
uint64_t *sprd_vdsp_mem_get_page_array(struct mem_ctx *mem_ctx, int buf_id);
uint64_t sprd_vdsp_mem_get_single_page(struct mem_ctx *mem_ctx, int buf_id,
	unsigned int offset);
phys_addr_t sprd_vdsp_mem_get_dev_addr(struct mem_ctx *mem_ctx, int buf_id);
size_t sprd_vdsp_mem_get_size(struct mem_ctx *mem_ctx, int buf_id);
phys_addr_t sprd_vdsp_mem_get_phy_addr(struct mem_ctx *mem_ctx, int buf_id);

int sprd_vdsp_mem_sync_cpu_to_device(struct mem_ctx *ctx, int buf_id);
int sprd_vdsp_mem_sync_device_to_cpu(struct mem_ctx *ctx, int buf_id);

size_t sprd_vdsp_mem_get_usage_max(const struct mem_ctx *ctx);
size_t sprd_vdsp_mem_get_usage_current(const struct mem_ctx *ctx);

#ifdef KERNEL_DMA_FENCE_SUPPORT
struct dma_fence *sprd_vdsp_mem_add_fence(struct mem_ctx *ctx, int buf_id);
void sprd_vdsp_mem_remove_fence(struct mem_ctx *ctx, int buf_id);
int sprd_vdsp_mem_signal_fence(struct mem_ctx *ctx, int buf_id);
#endif

/*
* related to stream MMU context (constains IMGMMU functionality in general)
*/
int sprd_vdsp_mem_map_iova(struct mem_ctx *mem_ctx, int buf_id, int isfixed, unsigned long fixed_data);
int sprd_vdsp_mem_unmap_iova(struct mem_ctx *mem_ctx, int buf_id);
#endif /* SPRD_VDSP_MEM_MAN_H */
