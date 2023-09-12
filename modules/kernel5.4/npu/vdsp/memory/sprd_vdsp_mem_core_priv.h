
/*!
 *****************************************************************************
 *
 * @File       sprd_vdsp_mem_core_priv.h
 * ---------------------------------------------------------------------------
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

#ifndef SPRD_VDSP_MEM_CORE_PRIV_H
#define SPRD_VDSP_MEM_CORE_PRIV_H

#include <linux/list.h>
#include <linux/idr.h>
#include <linux/scatterlist.h>
#include <linux/device.h>

#include "sprd_vdsp_mem_core.h"

enum buffer_mapping_sta {
	BUFFER_NO_MAPPING = 0x00000001,
	BUFFER_MAPPING = 0x00000002,
};

/* Memory context : one per process */
struct mem_ctx {
	unsigned id;
	struct idr buffers;
	/* Used to track memory usage */
	size_t mem_usage_max;
	size_t mem_usage_curr;
	struct xvp *xvp;
};

struct map_buf {
	struct device *dev;
	int mfd;
	struct sg_table *table;
	void *buf;
	size_t size;
	unsigned long addr_k;
	unsigned long iova;
	struct dma_buf *dmabuf_p;
	unsigned int offset;
	unsigned long addr_p;
	unsigned int map_flags;
	int isfixed;                // flag for fixed map, 0: no 1:fixed_offset 2:fixed_addr
	unsigned long fixed_data;   // fixed map addr or offset
};

/* An MMU mapping of a buffer */
struct mmu_ctx_mapping {
	struct buffer *buffer;
	uint64_t virt_addr;
	uint64_t cache_offset;
	struct list_head buffer_entry;	/* Entry in <buffer:mappings> */
	struct map_buf map_buf;
};

#ifdef KERNEL_DMA_FENCE_SUPPORT
struct buffer_fence {
	struct dma_fence fence;
	spinlock_t lock;
};
#endif

/* buffer : valid in the context of a mem_ctx */
struct buffer {
	int id;			/* Generated in <mem_ctx:buffers> */
	size_t request_size;
	size_t actual_size;
	struct device *device;
	struct mem_ctx *mem_ctx;
	struct heap *heap;
	int mapping;		/*0:buffer no map, 1:buffer mapping */
	int mmu_idx;
	void *kptr;		/*kernel space virt pointer */
	void *priv;
	phys_addr_t paddr;	/*only for carveout heap */
	struct map_buf map_buf;
#ifdef KERNEL_DMA_FENCE_SUPPORT
	struct buffer_fence *fence;
#endif
};

struct heap_ops {
	int (*alloc) (struct device *device, struct heap *heap, size_t size, enum sprd_vdsp_mem_attr attr,
		struct buffer *buffer);
	int (*import) (struct device *device, struct heap *heap, size_t size, enum sprd_vdsp_mem_attr attr,
		uint64_t buf_fd, struct page **pages, struct buffer *buffer);
	int (*export) (struct device *device, struct heap *heap, size_t size, enum sprd_vdsp_mem_attr attr,
		struct buffer *buffer, uint64_t *buf_hnd);
	void (*free) (struct heap *heap, struct buffer *buffer);
	int (*map_um) (struct heap *heap, struct buffer *buffer, struct vm_area_struct *vma);
	int (*unmap_um) (struct heap *heap, struct buffer *buffer);
	int (*map_km) (struct heap *heap, struct buffer *buffer);
	int (*unmap_km) (struct heap *heap, struct buffer *buffer);
	int (*get_sg_table) (struct heap *heap, struct buffer *buffer, struct sg_table **sg_table,
		bool *use_sg_dma);
	int (*get_page_array) (struct heap *heap, struct buffer *buffer, uint64_t **addrs);
	void (*sync_cpu_to_dev) (struct heap *heap, struct buffer *buffer);
	void (*sync_dev_to_cpu) (struct heap *heap, struct buffer *buffer);
	int (*set_offset)(struct heap *heap, size_t offs);
	void (*destroy) (struct heap *heap);
};

struct heap {
	int id;			/* Generated in <mem_man:heaps> */
	enum sprd_vdsp_mem_heap_type type;
	struct heap_ops *ops;
	union heap_options options;
	phys_addr_t(*to_dev_addr) (union heap_options *opts, phys_addr_t addr);
	phys_addr_t(*to_host_addr) (union heap_options *opts, phys_addr_t addr);
	bool cache_sync;
	enum sprd_vdsp_mem_attr alt_cache_attr;
	void *priv;
};

int sprd_vdsp_mem_unified_init(const struct heap_config *config, struct heap *heap);

int sprd_vdsp_mem_dmabuf_init(const struct heap_config *config, struct heap *heap);

int sprd_vdsp_mem_ion_init(const struct heap_config *config, struct heap *heap);

int sprd_vdsp_mem_anonymous_init(const struct heap_config *config, struct heap *heap);
#ifdef CONFIG_GENERIC_ALLOCATOR
int sprd_vdsp_mem_carveout_init(const struct heap_config *config, struct heap *heap);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 0)
typedef int vm_fault_t;
#endif

#endif /* SPRD_VDSP_MEM_MAN_PRIV_H */
