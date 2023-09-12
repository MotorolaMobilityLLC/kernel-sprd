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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/idr.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>

#include "sprd_vdsp_mem_core.h"
#include "sprd_vdsp_mem_core_priv.h"
#include "sprd_vdsp_iommus.h"
#include "xrp_internal.h"

#include <linux/kprobes.h>
#include <asm/traps.h>
#include "sprd_iommu_test_debug.h"
#include "vdsp_debugfs.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: [mem_core]: %d %d %s: "\
        fmt, current->pid, __LINE__, __func__

/* Maximum number of processes */
#define MAX_PROC_CTX 1000

/* Minimum page size (4KB) bits. */
#define MIN_PAGE_SIZE_BITS 12

struct mem_man {
	struct idr heaps;
	struct idr mem_ctxs;
	struct mutex mutex;

	unsigned cache_ref;
};

/* define like this, so it is easier to convert to a function argument later */
static struct mem_man mem_man_data;

struct sprd_vdsp_mmu_page {

	/**
     * @note Use ui64 instead of uintptr_t to support extended physical address
     * on 32b OS
     */
	uint64_t phys_addr;
	uint64_t virt_base;
	uintptr_t cpu_addr;
};

/* wrapper struct for imgmmu_page */
struct mmu_page {
	struct buffer *buffer;
	struct sprd_vdsp_mmu_page page;
	unsigned char type;
	bool bypass_addr_trans;
	bool use_parity;
};

static bool cache_sync = true;
module_param(cache_sync, bool, 0444);
MODULE_PARM_DESC(cache_sync,
	"cache sync mode: 0-no sync; 1-force sync (even if hw provides coherency);");

/*
 * memory heaps
 */
static char *get_heap_name(enum sprd_vdsp_mem_heap_type type)
{
	switch (type) {
	case SPRD_VDSP_MEM_HEAP_TYPE_UNIFIED:
		return "unified";
	case SPRD_VDSP_MEM_HEAP_TYPE_ION:
		return "ion";
	case SPRD_VDSP_MEM_HEAP_TYPE_DMABUF:
		return "dmabuf";
	case SPRD_VDSP_MEM_HEAP_TYPE_ANONYMOUS:
		return "anonymous";
	case SPRD_VDSP_MEM_HEAP_TYPE_CARVEOUT:
		return "carveout";

	default:
		WARN_ON(type);
		return "unknown";
	}
}

int sprd_vdsp_mem_add_heap(const struct heap_config *heap_cfg, int *heap_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct heap *heap;
	int (*init_fn) (const struct heap_config *heap_cfg, struct heap *heap);
	int ret;

	pr_debug("add heap\n");

	switch (heap_cfg->type) {
	case SPRD_VDSP_MEM_HEAP_TYPE_UNIFIED:
		init_fn = sprd_vdsp_mem_unified_init;
		break;
	case SPRD_VDSP_MEM_HEAP_TYPE_DMABUF:
	case SPRD_VDSP_MEM_HEAP_TYPE_ION:
		init_fn = sprd_vdsp_mem_dmabuf_init;
		break;
	case SPRD_VDSP_MEM_HEAP_TYPE_ANONYMOUS:
		init_fn = sprd_vdsp_mem_anonymous_init;
		break;
#ifdef CONFIG_GENERIC_ALLOCATOR
	case SPRD_VDSP_MEM_HEAP_TYPE_CARVEOUT:
		init_fn = sprd_vdsp_mem_carveout_init;
		break;
#endif
	default:
		pr_err("heap type %d unknown\n", heap_cfg->type);
		return -EINVAL;
	}

	heap = kmalloc(sizeof(struct heap), GFP_KERNEL);
	if (!heap)
		return -ENOMEM;

	ret = mutex_lock_interruptible(&mem_man->mutex);
	if (ret)
		goto lock_failed;

	ret = idr_alloc(&mem_man->heaps, heap, SPRD_VDSP_MEM_MAN_MIN_HEAP,
		SPRD_VDSP_MEM_MAN_MAX_HEAP, GFP_KERNEL);
	if (ret < 0) {
		pr_err("idr_alloc failed\n");
		goto alloc_id_failed;
	}

	heap->id = ret;
	heap->type = heap_cfg->type;
	heap->options = heap_cfg->options;
	heap->to_dev_addr = heap_cfg->to_dev_addr;
	heap->to_host_addr = heap_cfg->to_host_addr;
	heap->priv = NULL;
	heap->cache_sync = true;
	heap->alt_cache_attr = heap_cfg->cache_attr;

	ret = init_fn(heap_cfg, heap);
	if (ret) {
		pr_err("heap init failed\n");
		goto heap_init_failed;
	}

	*heap_id = heap->id;
	mutex_unlock(&mem_man->mutex);

	pr_debug("created heap %d type %d (%s)\n",
		*heap_id, heap_cfg->type, get_heap_name(heap->type));
	return 0;

heap_init_failed:
	idr_remove(&mem_man->heaps, heap->id);
alloc_id_failed:
	mutex_unlock(&mem_man->mutex);
lock_failed:
	kfree(heap);
	return ret;
}

EXPORT_SYMBOL(sprd_vdsp_mem_add_heap);

static void _sprd_vdsp_mem_del_heap(struct heap *heap)
{
	struct mem_man *mem_man = &mem_man_data;

	pr_debug("heap %d 0x%p\n", heap->id, heap);

	WARN_ON(!mutex_is_locked(&mem_man->mutex));

	if (heap->ops->destroy)
		heap->ops->destroy(heap);

	idr_remove(&mem_man->heaps, heap->id);
}

void sprd_vdsp_mem_del_heap(int heap_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct heap *heap;

	pr_debug("delete heap %d\n", heap_id);

	mutex_lock(&mem_man->mutex);

	heap = idr_find(&mem_man->heaps, heap_id);
	if (!heap) {
		pr_warn("heap %d not found!\n", heap_id);
		mutex_unlock(&mem_man->mutex);
		return;
	}

	_sprd_vdsp_mem_del_heap(heap);

	mutex_unlock(&mem_man->mutex);

	kfree(heap);
}

EXPORT_SYMBOL(sprd_vdsp_mem_del_heap);

int sprd_vdsp_mem_get_heap_info(int heap_id, uint8_t *type, uint32_t *attrs)
{
	struct mem_man *mem_man = &mem_man_data;
	struct heap *heap;

	pr_debug("get heap %d\n", heap_id);

	if (heap_id < SPRD_VDSP_MEM_MAN_MIN_HEAP
		|| heap_id > SPRD_VDSP_MEM_MAN_MAX_HEAP) {
		pr_err("heap %d does not match internal constraints <%u - %u>!\n",
			heap_id, SPRD_VDSP_MEM_MAN_MIN_HEAP, SPRD_VDSP_MEM_MAN_MAX_HEAP);
		return -EINVAL;
	}
	mutex_lock(&mem_man->mutex);

	heap = idr_find(&mem_man->heaps, heap_id);
	if (!heap) {
		pr_debug("heap %d not found!\n", heap_id);
		mutex_unlock(&mem_man->mutex);
		return -ENOENT;
	}

	*type = heap->type;

	*attrs = 0;
	if (heap->ops->import)
		*attrs |= SPRD_VDSP_MEM_HEAP_ATTR_IMPORT;
	if (heap->ops->export)
		*attrs |= SPRD_VDSP_MEM_HEAP_ATTR_EXPORT;
	if (heap->ops->alloc && !heap->ops->import)
		*attrs |= SPRD_VDSP_MEM_HEAP_ATTR_INTERNAL;

	mutex_unlock(&mem_man->mutex);

	return 0;
}

EXPORT_SYMBOL(sprd_vdsp_mem_get_heap_info);

int sprd_vdsp_mem_get_heap_id(enum sprd_vdsp_mem_heap_type type)
{
	struct mem_man *mem_man = &mem_man_data;
	struct heap *heap;
	int id;

	if (type < SPRD_VDSP_MEM_HEAP_TYPE_UNIFIED
	    || type > SPRD_VDSP_MEM_HEAP_TYPE_CARVEOUT) {
		pr_err(" Error heap type %d does not match internal constraints <%u - %u>!\n",
			type, SPRD_VDSP_MEM_HEAP_TYPE_UNIFIED, SPRD_VDSP_MEM_HEAP_TYPE_CARVEOUT);
		return -1;
	}
	mutex_lock(&mem_man->mutex);

	idr_for_each_entry(&mem_man->heaps, heap, id) {
		if (heap->type == type) {
			mutex_unlock(&mem_man->mutex);
			return heap->id;
		}
	}

	mutex_unlock(&mem_man->mutex);
	return 0;
}

EXPORT_SYMBOL(sprd_vdsp_mem_get_heap_id);

/*
 * related to process context (contains SYSMEM heap's functionality in general)
 */
int sprd_vdsp_mem_create_proc_ctx(struct mem_ctx **new_ctx)
{
	struct mem_man *mem_man = &mem_man_data;
	struct mem_ctx *ctx;
	int ret = 0;

	pr_debug("create mem proc ctx\n");

	ctx = kzalloc(sizeof(struct mem_ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	idr_init(&ctx->buffers);

	mutex_lock(&mem_man->mutex);
	ret = idr_alloc(&mem_man->mem_ctxs, ctx, 0, MAX_PROC_CTX, GFP_KERNEL);
	if (ret < 0) {
		mutex_unlock(&mem_man->mutex);
		pr_err("Error: idr_alloc failed\n");
		goto idr_alloc_failed;
	}
	/* Assign id to the newly created context. */
	ctx->id = ret;
	mutex_unlock(&mem_man->mutex);

	pr_debug("proc ctx id:%d\n", ctx->id);

	*new_ctx = ctx;
	return 0;

idr_alloc_failed:
	kfree(ctx);
	return ret;
}

EXPORT_SYMBOL(sprd_vdsp_mem_create_proc_ctx);

static void _sprd_vdsp_mem_free(struct buffer *buffer);

static void _sprd_vdsp_mem_destroy_proc_ctx(struct mem_ctx *ctx)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;
	int buf_id;

	pr_debug("destroy proc ctx id:%d\n", ctx->id);

	WARN_ON(!mutex_is_locked(&mem_man->mutex));

	/* free derelict buffers */
	buf_id = SPRD_VDSP_MEM_MAN_MIN_BUFFER;
	buffer = idr_get_next(&ctx->buffers, &buf_id);
	while (buffer) {
		pr_warn("found derelict buffer %d\n", buf_id);
		_sprd_vdsp_mem_free(buffer);
		buf_id = SPRD_VDSP_MEM_MAN_MIN_BUFFER;
		buffer = idr_get_next(&ctx->buffers, &buf_id);
	}

	idr_destroy(&ctx->buffers);
	idr_remove(&mem_man->mem_ctxs, ctx->id);
}

void sprd_vdsp_mem_destroy_proc_ctx(struct mem_ctx *ctx)
{
	struct mem_man *mem_man = &mem_man_data;

	pr_debug("%s:%d\n", __func__, __LINE__);

	mutex_lock(&mem_man->mutex);
	_sprd_vdsp_mem_destroy_proc_ctx(ctx);
	mutex_unlock(&mem_man->mutex);

	kfree(ctx);
}

EXPORT_SYMBOL(sprd_vdsp_mem_destroy_proc_ctx);

static int _sprd_vdsp_mem_alloc(struct device *device, struct mem_ctx *ctx,
	struct heap *heap, size_t size, enum sprd_vdsp_mem_attr attr, struct buffer **buffer_new)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;
	int ret;

	/* Allocations for MMU pages are still 4k so CPU page size is enough */
	size_t align = attr & SPRD_VDSP_MEM_ATTR_MMU ? PAGE_SIZE : PAGE_SIZE;

	pr_debug("heap %p '%s' ctx %p size %zu\n", heap, get_heap_name(heap->type), ctx, size);

	WARN_ON(!mutex_is_locked(&mem_man->mutex));

	if (size == 0) {
		pr_err("buffer size is zero\n");
		return -EINVAL;
	}

	if (heap->ops == NULL || heap->ops->alloc == NULL) {
		pr_err("no alloc function in heap %d!\n", heap->id);
		return -EINVAL;
	}

	buffer = kzalloc(sizeof(struct buffer), GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	ret = idr_alloc(&ctx->buffers, buffer,
		(SPRD_VDSP_MEM_MAN_MAX_BUFFER * ctx->id) + SPRD_VDSP_MEM_MAN_MIN_BUFFER,
		(SPRD_VDSP_MEM_MAN_MAX_BUFFER * ctx->id) + SPRD_VDSP_MEM_MAN_MAX_BUFFER,
		GFP_KERNEL);
	if (ret < 0) {
		pr_err("idr_alloc failed\n");
		goto idr_alloc_failed;
	}

	buffer->id = ret;
	buffer->request_size = size;
	buffer->actual_size = ((size + align - 1) / align) * align;
	buffer->device = device;
	buffer->mem_ctx = ctx;
	buffer->heap = heap;
	buffer->mapping = BUFFER_NO_MAPPING;
	buffer->kptr = NULL;
	buffer->priv = NULL;

	/* Check if heap has been registered using an alternative cache attributes */
	if (heap->alt_cache_attr &&
		(heap->alt_cache_attr != (attr & SPRD_VDSP_MEM_ATTR_CACHE_MASK))) {

		pr_debug("heap %d changing cache attributes from %x to %x\n",
			heap->id, attr & SPRD_VDSP_MEM_ATTR_CACHE_MASK,
			heap->alt_cache_attr);

		attr &= ~SPRD_VDSP_MEM_ATTR_CACHE_MASK;
		attr |= heap->alt_cache_attr;
	}

	ret = heap->ops->alloc(device, heap, buffer->actual_size, attr, buffer);
	if (ret) {
		pr_err("heap %d alloc failed\n", heap->id);
		goto heap_alloc_failed;
	}
	pr_debug("-- Allocating zeroed buffer id:%d  size:%zu\n",
		buffer->id, buffer->actual_size);

	ctx->mem_usage_curr += buffer->actual_size;
	if (ctx->mem_usage_curr > ctx->mem_usage_max)
		ctx->mem_usage_max = ctx->mem_usage_curr;

	*buffer_new = buffer;

	pr_debug("heap %p ctx %p created buffer %d (%p) actual_size %zu\n",
		heap, ctx, buffer->id, buffer, buffer->actual_size);

	return 0;

heap_alloc_failed:
	idr_remove(&ctx->buffers, buffer->id);
idr_alloc_failed:
	kfree(buffer);
	return ret;
}

int sprd_vdsp_mem_alloc(struct device *device, struct mem_ctx *ctx, int heap_id,
	size_t size, enum sprd_vdsp_mem_attr attr, int *buf_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct heap *heap;
	struct buffer *buffer;
	int ret;

	pr_debug("heap %d ctx %p size %zu\n", heap_id, ctx, size);

	ret = mutex_lock_interruptible(&mem_man->mutex);
	if (ret)
		return ret;

	heap = idr_find(&mem_man->heaps, heap_id);
	if (!heap) {
		pr_err("heap id %d not found\n", heap_id);
		mutex_unlock(&mem_man->mutex);
		return -EINVAL;
	}

	ret = _sprd_vdsp_mem_alloc(device, ctx, heap, size, attr, &buffer);
	if (ret) {
		mutex_unlock(&mem_man->mutex);
		return ret;
	}

	*buf_id = buffer->id;
	mutex_unlock(&mem_man->mutex);

	pr_debug("heap %s ctx %p created buffer %d (0x%lx) size %zu\n",
		get_heap_name(heap->type), ctx, *buf_id, ( unsigned long) buffer, size);

	return ret;
}

EXPORT_SYMBOL(sprd_vdsp_mem_alloc);

static int _sprd_vdsp_mem_import(struct device *device, struct mem_ctx *ctx,
	struct heap *heap, size_t size, enum sprd_vdsp_mem_attr attr, uint64_t buf_fd,
	struct page **pages, struct buffer **buffer_new)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;
	int ret;
	size_t align = PAGE_SIZE;

	WARN_ON(!mutex_is_locked(&mem_man->mutex));

	if (size == 0) {
		pr_err("buffer size is zero\n");
		return -EINVAL;
	}

	if (heap->ops == NULL || heap->ops->import == NULL) {
		pr_err("no import function in heap %d!\n", heap->id);
		return -EINVAL;
	}

	buffer = kzalloc(sizeof(struct buffer), GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	ret = idr_alloc(&ctx->buffers, buffer,
		(SPRD_VDSP_MEM_MAN_MAX_BUFFER * ctx->id) + SPRD_VDSP_MEM_MAN_MIN_BUFFER,
		(SPRD_VDSP_MEM_MAN_MAX_BUFFER * ctx->id) + SPRD_VDSP_MEM_MAN_MAX_BUFFER,
		GFP_KERNEL);
	if (ret < 0) {
		pr_err("idr_alloc failed\n");
		goto idr_alloc_failed;
	}

	buffer->id = ret;
	buffer->request_size = size;
	buffer->actual_size = ((size + align - 1) / align) * align;
	buffer->device = device;
	buffer->mem_ctx = ctx;
	buffer->heap = heap;
	buffer->mapping = BUFFER_NO_MAPPING;
	buffer->kptr = NULL;
	buffer->priv = NULL;

	/* If MMU page size is bigger than CPU page size
	 * we need an extra check against requested size
	 * The aligned size comparing to requested size
	 * can't be bigger than CPU page!
	 * otherwise it can cause troubles when
	 * HW tries to access non existing pages */
	if (buffer->actual_size - buffer->request_size > PAGE_SIZE) {
		pr_err("original buffer size is not MMU page size aligned!\n");
		ret = -EINVAL;
		goto heap_import_failed;
	}

	/* Check if heap has been registered using an alternative cache attributes */
	if (heap->alt_cache_attr &&
		(heap->alt_cache_attr != (attr & SPRD_VDSP_MEM_ATTR_CACHE_MASK))) {

		pr_debug("heap %d changing cache attributes from %x to %x\n",
			heap->id, attr & SPRD_VDSP_MEM_ATTR_CACHE_MASK, heap->alt_cache_attr);

		attr &= ~SPRD_VDSP_MEM_ATTR_CACHE_MASK;
		attr |= heap->alt_cache_attr;
	}

	ret = heap->ops->import(device, heap, buffer->actual_size, attr,
				buf_fd, pages, buffer);
	if (ret) {
		pr_err("heap %d import failed\n", heap->id);
		goto heap_import_failed;
	}

	pr_debug("-- Allocating zeroed buffer id:%d size:%zu for imported data\n",
		buffer->id, buffer->actual_size);
	pr_debug("CALLOC :BLOCK_%d %#zx %#zx 0x0\n",
		buffer->id, buffer->actual_size, align);
	ctx->mem_usage_curr += buffer->actual_size;
	if (ctx->mem_usage_curr > ctx->mem_usage_max)
		ctx->mem_usage_max = ctx->mem_usage_curr;

	*buffer_new = buffer;
	return 0;

heap_import_failed:
	idr_remove(&ctx->buffers, buffer->id);
idr_alloc_failed:
	kfree(buffer);
	return ret;
}

static void _sprd_vdsp_mem_put_pages(size_t size, struct page **pages)
{
	int num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	int i;

	for (i = 0; i < num_pages; i++)
		if (pages[i])
			put_page(pages[i]);
	kfree(pages);
}

static int _sprd_vdsp_mem_get_user_pages(size_t size, uint64_t cpu_ptr,
				struct page **pages[])
{
	struct page **tmp_pages = NULL;
	int num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	int ret;

	/* Check alignment */
	if (cpu_ptr & (PAGE_SIZE-1)) {
		pr_err("wrong alignment of %#llx address!\n", cpu_ptr);
		return -EFAULT;
	}

	tmp_pages = kmalloc_array(num_pages, sizeof(struct page *),
			GFP_KERNEL | __GFP_ZERO);
	if (!tmp_pages) {
		pr_err("failed to allocate memory for pages\n");
		return -ENOMEM;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
	down_read(&current->mm->mmap_sem);
#else
	mmap_read_lock(current->mm);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
	ret = get_user_pages(
			cpu_ptr, num_pages,
			0,
			tmp_pages, NULL);
#else
	pr_err("get_user_pages not supported for this kernel version\n");
	ret = -1;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0)
	up_read(&current->mm->mmap_sem);
#else
	mmap_read_unlock(current->mm);
#endif
	if (ret != num_pages) {
		pr_err("failed to get_user_pages count:%d for %#llx address\n", num_pages, cpu_ptr);
		ret = -ENOMEM;
		goto out_get_user_pages;
	}

	*pages = tmp_pages;

	return 0;

out_get_user_pages:
	_sprd_vdsp_mem_put_pages(size, tmp_pages);

	return ret;
}

int sprd_vdsp_mem_import(struct device *device, struct mem_ctx *ctx,
	int heap_id, size_t size, enum sprd_vdsp_mem_attr attr, uint64_t buf_fd,
	uint64_t cpu_ptr, int *buf_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct heap *heap;
	struct buffer *buffer;
	struct page **pages = NULL;
	int ret;

	pr_debug("heap %d ctx %p hnd %#llx\n", heap_id, ctx, buf_fd);

	if (cpu_ptr) {
		ret = _sprd_vdsp_mem_get_user_pages(size, cpu_ptr, &pages);
		if (ret) {
			pr_err("getting user pages failed\n");
			return ret;
		}
	}

	ret = mutex_lock_interruptible(&mem_man->mutex);
	if (ret) {
		pr_err("lock interrupted: mem_man->mutex\n");
		goto lock_interrupted;
	}

	heap = idr_find(&mem_man->heaps, heap_id);
	if (!heap) {
		pr_err("heap id %d not found\n", heap_id);
		ret = -EINVAL;
		goto idr_find_failed;
	}

	ret = _sprd_vdsp_mem_import(device, ctx, heap, size, attr, buf_fd, pages, &buffer);
	if (ret)
		goto sprd_vdsp_import_failed;

	*buf_id = buffer->id;
	mutex_unlock(&mem_man->mutex);

	if (cpu_ptr)
		_sprd_vdsp_mem_put_pages(size, pages);

	pr_debug("buf_fd %#llx heap %d (%s) buffer %d size %zu\n",
		buf_fd, heap_id, get_heap_name(heap->type), *buf_id, size);
	pr_debug("heap %d ctx %p created buffer %d (%p) size %zu\n",
		heap_id, ctx, *buf_id, buffer, size);

	return 0;

sprd_vdsp_import_failed:
idr_find_failed:
	mutex_unlock(&mem_man->mutex);
lock_interrupted:
	if (cpu_ptr)
		_sprd_vdsp_mem_put_pages(size, pages);

	return ret;
}

EXPORT_SYMBOL(sprd_vdsp_mem_import);

static int _sprd_vdsp_mem_export(struct device *device,
	struct mem_ctx *ctx, struct heap *heap,
	size_t size, enum sprd_vdsp_mem_attr attr,
	struct buffer *buffer, uint64_t *buf_hnd)
{
	struct mem_man *mem_man = &mem_man_data;
	int ret;

	WARN_ON(!mutex_is_locked(&mem_man->mutex));

	if (size > buffer->actual_size) {
		pr_err("buffer size (%zu) bigger than actual size (%zu)\n",
			size, buffer->actual_size);
		return -EINVAL;
	}

	if (heap->ops == NULL || heap->ops->export == NULL) {
		pr_err("no export function in heap %d!\n", heap->id);
		return -EINVAL;
	}

	ret = heap->ops->export(device, heap, buffer->actual_size, attr, buffer, buf_hnd);
	if (ret) {
		pr_err("heap %d export failed\n", heap->id);
		return -EFAULT;
	}

	return ret;
}

int sprd_vdsp_mem_export(struct device *device, struct mem_ctx *ctx, int buf_id,
	size_t size, enum sprd_vdsp_mem_attr attr, uint64_t *buf_hnd)
{
	struct mem_man *mem_man = &mem_man_data;
	struct heap *heap;
	struct buffer *buffer;
	int ret;

	pr_debug("ctx %p buffer id %d\n", ctx, buf_id);

	ret = mutex_lock_interruptible(&mem_man->mutex);
	if (ret)
		return ret;

	buffer = idr_find(&ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("buffer id %d not found\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return -EINVAL;
	}

	heap = buffer->heap;

	ret = _sprd_vdsp_mem_export(device, ctx, heap, size, attr, buffer, buf_hnd);
	if (ret) {
		mutex_unlock(&mem_man->mutex);
		return ret;
	}

	mutex_unlock(&mem_man->mutex);

	pr_debug("buf_hnd %#llx heap %d (%s) buffer %d size %zu\n",
		*buf_hnd, heap->id, get_heap_name(heap->type), buf_id, size);
	pr_debug("heap %d ctx %p exported buffer %d (%p) size %zu\n",
		heap->id, ctx, buf_id, buffer, size);

	return ret;
}

EXPORT_SYMBOL(sprd_vdsp_mem_export);

static int _sprd_vdsp_mem_unmap_iova(struct sprd_vdsp_iommus *iommus,
	struct map_buf *pfinfo);
static void _sprd_vdsp_mem_free(struct buffer *buffer)
{
	struct mem_man *mem_man = &mem_man_data;
	struct heap *heap = buffer->heap;
	struct mem_ctx *ctx = buffer->mem_ctx;
	struct sprd_vdsp_iommus *iommus = ctx->xvp->iommus;

	pr_debug("buffer 0x%p\n", buffer);

	WARN_ON(!mutex_is_locked(&mem_man->mutex));

	if (heap->ops == NULL || heap->ops->free == NULL) {
		pr_err("no free function in heap %d!\n", heap->id);
		return;
	}
	pr_debug("mapping %d, mmu_idx %d\n", buffer->mapping, buffer->mmu_idx);
	if ((BUFFER_MAPPING == buffer->mapping)
		&& (buffer->mmu_idx < SPRD_VDSP_IOMMU_MAX)
		&& (buffer->mmu_idx >= SPRD_VDSP_IOMMU_EPP)) {
		pr_warn("found mapping for buffer %d (size %zu)\n", buffer->id, buffer->actual_size);
		_sprd_vdsp_mem_unmap_iova(iommus, &buffer->map_buf);
		buffer->mapping = BUFFER_NO_MAPPING;
	}

	heap->ops->free(heap, buffer);
	if (ctx->mem_usage_curr >= buffer->actual_size)
		ctx->mem_usage_curr -= buffer->actual_size;
	else
		WARN_ON(1);

	idr_remove(&ctx->buffers, buffer->id);
	pr_debug("-- Freeing buffer id:%d  size:%zu\n", buffer->id, buffer->actual_size);

	kfree(buffer);
}

void sprd_vdsp_mem_free(struct mem_ctx *ctx, int buf_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;

	pr_debug("buffer %d\n", buf_id);

	mutex_lock(&mem_man->mutex);

	buffer = idr_find(&ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("Error: buffer id %d not found\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return;
	}

	_sprd_vdsp_mem_free(buffer);

	mutex_unlock(&mem_man->mutex);
}

EXPORT_SYMBOL(sprd_vdsp_mem_free);

#ifdef KERNEL_DMA_FENCE_SUPPORT

/*
 * dma_fence ops
 */
static const char *_sprd_vdsp_mem_sync_get_driver_name(struct dma_fence *f)
{
	return "buf_sync";
}

static const char *_sprd_vdsp_mem_sync_get_timeline_name(struct dma_fence *f)
{
	return "buf_timeline";
}

static bool _sprd_vdsp_mem_sync_enable_signaling(struct dma_fence *f)
{
	return true;
}

static void _sprd_vdsp_mem_sync_release(struct dma_fence *fence)
{
	dma_fence_free(fence);
}

static struct dma_fence_ops dma_fence_ops = {
	.get_driver_name = _sprd_vdsp_mem_sync_get_driver_name,
	.get_timeline_name = _sprd_vdsp_mem_sync_get_timeline_name,
	.enable_signaling = _sprd_vdsp_mem_sync_enable_signaling,
	.release = _sprd_vdsp_mem_sync_release,
	.wait = dma_fence_default_wait
};

struct dma_fence *sprd_vdsp_mem_add_fence(struct mem_ctx *ctx, int buf_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;

	pr_debug("buffer %d\n", buf_id);

	mutex_lock(&mem_man->mutex);

	buffer = idr_find(&ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("buffer id %d not found\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return NULL;
	}

	if (buffer->fence) {
		pr_err("fence for buffer id %d already allocated and not freed \n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return NULL;
	}

	buffer->fence = kmalloc(sizeof(struct buffer_fence), GFP_KERNEL);
	if (!buffer->fence) {
		pr_err("cannot allocate fence for buffer id %d\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return NULL;
	}

	spin_lock_init(&buffer->fence->lock);
	dma_fence_init(&buffer->fence->fence, &dma_fence_ops, &buffer->fence->lock,
		dma_fence_context_alloc(1), 1);

	mutex_unlock(&mem_man->mutex);

	return &buffer->fence->fence;
}

EXPORT_SYMBOL(sprd_vdsp_mem_add_fence);

void sprd_vdsp_mem_remove_fence(struct mem_ctx *ctx, int buf_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;
	struct dma_fence *fence = NULL;

	pr_debug("buffer %d\n", buf_id);

	mutex_lock(&mem_man->mutex);

	buffer = idr_find(&ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("buffer id %d not found\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return;
	}

	if (buffer->fence) {
		fence = &buffer->fence->fence;
		buffer->fence = NULL;
	}

	mutex_unlock(&mem_man->mutex);

	if (fence)
		dma_fence_signal(fence);
}

EXPORT_SYMBOL(sprd_vdsp_mem_remove_fence);

int sprd_vdsp_mem_signal_fence(struct mem_ctx *ctx, int buf_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;
	struct dma_fence *fence = NULL;
	int ret = -1;

	pr_debug("buffer %d\n", buf_id);

	mutex_lock(&mem_man->mutex);

	buffer = idr_find(&ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("buffer id %d not found\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return -1;
	}
	if (buffer->fence) {
		fence = &buffer->fence->fence;
		buffer->fence = NULL;
	}

	mutex_unlock(&mem_man->mutex);

	if (fence)
		ret = dma_fence_signal(fence);

	return ret;
}

EXPORT_SYMBOL(sprd_vdsp_mem_signal_fence);
#endif

static void _sprd_vdsp_mem_sync_device_to_cpu(struct buffer *buffer, bool force);

int sprd_vdsp_mem_map_um(struct mem_ctx *ctx, int buf_id,
	struct vm_area_struct *vma)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;
	struct heap *heap;
	int ret;

	pr_debug("buffer %d\n", buf_id);

	mutex_lock(&mem_man->mutex);
	buffer = idr_find(&ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("buffer id %d not found\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return -EINVAL;
	}
	pr_debug("buffer 0x%p\n", buffer);

	heap = buffer->heap;
	if (heap->ops == NULL || heap->ops->map_um == NULL) {
		pr_err("no map_um in heap %d!\n", heap->id);
		mutex_unlock(&mem_man->mutex);
		return -EINVAL;
	}

	ret = heap->ops->map_um(heap, buffer, vma);
	/* Always invalidate the buffer when it is mapped into UM */ //why remove VM_WRITE
	if (!ret && (vma->vm_flags & VM_READ)) //&& !(vma->vm_flags & VM_WRITE))
		_sprd_vdsp_mem_sync_device_to_cpu(buffer, false);

	mutex_unlock(&mem_man->mutex);

	return ret;
}

EXPORT_SYMBOL(sprd_vdsp_mem_map_um);

int sprd_vdsp_mem_unmap_um(struct mem_ctx *ctx, int buf_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;
	struct heap *heap;
	int ret;

	pr_debug("buffer %d\n", buf_id);

	mutex_lock(&mem_man->mutex);
	buffer = idr_find(&ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("buffer id %d not found\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return -EINVAL;
	}
	pr_debug("buffer 0x%p\n", buffer);

	heap = buffer->heap;
	if (heap->ops == NULL || heap->ops->unmap_um == NULL) {
		pr_err("no map_um in heap %d!\n", heap->id);
		mutex_unlock(&mem_man->mutex);
		return -EINVAL;
	}

	ret = heap->ops->unmap_um(heap, buffer);

	mutex_unlock(&mem_man->mutex);

	return ret;
}

EXPORT_SYMBOL(sprd_vdsp_mem_unmap_um);

static int _sprd_vdsp_mem_map_km(struct buffer *buffer)
{
	struct mem_man *mem_man = &mem_man_data;
	struct heap *heap = buffer->heap;

	pr_debug("buffer 0x%p, heap %d\n", buffer, heap->id);

	WARN_ON(!mutex_is_locked(&mem_man->mutex));

	if (heap->ops == NULL || heap->ops->map_km == NULL) {
		pr_err("no map_km in heap %d!\n", heap->id);
		return -EINVAL;
	}

	return heap->ops->map_km(heap, buffer);
}

int sprd_vdsp_mem_map_km(struct mem_ctx *ctx, int buf_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;
	int ret;

	pr_debug("buffer %d\n", buf_id);

	mutex_lock(&mem_man->mutex);
	buffer = idr_find(&ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("buffer id %d not found\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return -EINVAL;
	}

	ret = _sprd_vdsp_mem_map_km(buffer);

	mutex_unlock(&mem_man->mutex);

	return ret;
}

EXPORT_SYMBOL(sprd_vdsp_mem_map_km);

static int _sprd_vdsp_mem_unmap_km(struct buffer *buffer)
{
	struct mem_man *mem_man = &mem_man_data;
	struct heap *heap = buffer->heap;

	pr_debug("buffer 0x%p\n", buffer);

	WARN_ON(!mutex_is_locked(&mem_man->mutex));

	if (heap->ops == NULL || heap->ops->unmap_km == NULL) {
		pr_err("no unmap_km in heap %d!\n", heap->id);
		return -EINVAL;
	}

	return heap->ops->unmap_km(heap, buffer);
}

int sprd_vdsp_mem_unmap_km(struct mem_ctx *ctx, int buf_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;
	int ret;

	pr_debug("buffer %d\n", buf_id);

	mutex_lock(&mem_man->mutex);
	buffer = idr_find(&ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("buffer id %d not found\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return -EINVAL;
	}

	ret = _sprd_vdsp_mem_unmap_km(buffer);

	mutex_unlock(&mem_man->mutex);

	return ret;
}

EXPORT_SYMBOL(sprd_vdsp_mem_unmap_km);

uint64_t *sprd_vdsp_mem_get_page_array(struct mem_ctx *mem_ctx, int buf_id)
{
	struct buffer *buffer;
	struct heap *heap;
	struct mem_man *mem_man = &mem_man_data;
	uint64_t *addrs = NULL;
	int ret;

	mutex_lock(&mem_man->mutex);
	buffer = idr_find(&mem_ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("buffer id %d not found\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return NULL;
	}

	heap = buffer->heap;
	if (heap && heap->ops && heap->ops->get_page_array) {
		ret = heap->ops->get_page_array(heap, buffer, &addrs);
		if (ret || addrs == NULL) {
			pr_err("no page array for heap %d buffer %d\n", heap->id, buffer->id);
		}
	} else
		pr_err("heap %d does not support page arrays\n", heap->id);
	mutex_unlock(&mem_man->mutex);
	return addrs;
}

EXPORT_SYMBOL(sprd_vdsp_mem_get_page_array);

/* gets physical address of a single page at given offset */
uint64_t sprd_vdsp_mem_get_single_page(struct mem_ctx *mem_ctx, int buf_id,
	unsigned int offset)
{
	struct buffer *buffer;
	struct heap *heap;
	struct mem_man *mem_man = &mem_man_data;
	int ret;
	uint64_t addr = 0;

	mutex_lock(&mem_man->mutex);
	buffer = idr_find(&mem_ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("buffer id %d not found\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return -1;
	}

	heap = buffer->heap;
	if (!heap) {
		pr_err("buffer %d does not point any heap it belongs to!\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return -1;
	}

	if (heap->ops && heap->ops->get_sg_table) {
		struct sg_table *sgt;
		struct scatterlist *sgl;
		int offs = offset;
		bool use_sg_dma = false;

		ret = heap->ops->get_sg_table(heap, buffer, &sgt, &use_sg_dma);
		if (ret) {
			pr_err("heap %d buffer %d no sg_table!\n", heap->id, buffer->id);
			return -1;
		}
		sgl = sgt->sgl;
		while (sgl) {
			if (use_sg_dma)
				offs -= sg_dma_len(sgl);
			else
				offs -= sgl->length;

			if (offs <= 0)
				break;
			sgl = sg_next(sgl);
		}
		if (!sgl) {
			pr_err("heap %d buffer %d wrong offset %d!\n", heap->id, buffer->id, offset);
			return -1;
		}

		if (use_sg_dma)
			addr = sg_dma_address(sgl);
		else
			addr = sg_phys(sgl);

	} else if (heap->ops && heap->ops->get_page_array) {
		uint64_t *addrs;
		int page_idx = offset / PAGE_SIZE;

		ret = heap->ops->get_page_array(heap, buffer, &addrs);
		if (ret) {
			pr_err("heap %d buffer %d no page array!\n", heap->id, buffer->id);
			return -1;
		}

		if (offset > buffer->actual_size) {
			pr_err("heap %d buffer %d wrong offset %d!\n", heap->id, buffer->id, offset);
			return -1;
		}
		addr = addrs[page_idx];
	}

	mutex_unlock(&mem_man->mutex);
	return addr;
}

EXPORT_SYMBOL(sprd_vdsp_mem_get_single_page);

void *sprd_vdsp_mem_get_kptr(struct mem_ctx *ctx, int buf_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;
	void *kptr;

	mutex_lock(&mem_man->mutex);
	buffer = idr_find(&ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("buffer id %d not found\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return NULL;
	}
	kptr = buffer->kptr;

	mutex_unlock(&mem_man->mutex);
	return kptr;
}

EXPORT_SYMBOL(sprd_vdsp_mem_get_kptr);

phys_addr_t sprd_vdsp_mem_get_dev_addr(struct mem_ctx *mem_ctx, int buf_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;

	mutex_lock(&mem_man->mutex);
	buffer = idr_find(&mem_ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("buffer id %d not found\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		dump_stack();
		return 0;
	}

	mutex_unlock(&mem_man->mutex);
	return buffer->map_buf.iova;
}

EXPORT_SYMBOL(sprd_vdsp_mem_get_dev_addr);

phys_addr_t sprd_vdsp_mem_get_phy_addr(struct mem_ctx *mem_ctx, int buf_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;

	mutex_lock(&mem_man->mutex);
	buffer = idr_find(&mem_ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("buffer id %d not found\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		dump_stack();
		return 0;
	}

	mutex_unlock(&mem_man->mutex);
	return buffer->paddr;
}

EXPORT_SYMBOL(sprd_vdsp_mem_get_phy_addr);

size_t sprd_vdsp_mem_get_size(struct mem_ctx *mem_ctx, int buf_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;

	mutex_lock(&mem_man->mutex);
	buffer = idr_find(&mem_ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("buffer id %d not found\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return 0;
	}

	mutex_unlock(&mem_man->mutex);
	return buffer->request_size;
}

EXPORT_SYMBOL(sprd_vdsp_mem_get_size);

static void _sprd_vdsp_mem_sync_cpu_to_device(struct buffer *buffer, bool force)
{
	struct mem_man *mem_man = &mem_man_data;
	struct heap *heap = buffer->heap;

	if (!cache_sync) {
		pr_warn("buffer %d size %zu cache synchronization disabled!\n",
			buffer->id, buffer->actual_size);
		return;
	}
	pr_debug("buffer %d size %zu kptr %p cache(%d:%d)\n",
		buffer->id, buffer->actual_size, buffer->kptr, force, heap->cache_sync);

	WARN_ON(!mutex_is_locked(&mem_man->mutex));

	if (heap->ops && heap->ops->sync_cpu_to_dev
		&& (force || heap->cache_sync))
		heap->ops->sync_cpu_to_dev(heap, buffer);

#ifdef CONFIG_ARM
	dmb();
#else
	/* Put memory barrier */
	mb();
#endif
}

int sprd_vdsp_mem_sync_cpu_to_device(struct mem_ctx *ctx, int buf_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;

	pr_debug("buffer %d\n", buf_id);

	mutex_lock(&mem_man->mutex);
	buffer = idr_find(&ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("buffer id %d not found\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return -EINVAL;
	}

	_sprd_vdsp_mem_sync_cpu_to_device(buffer, false);

	mutex_unlock(&mem_man->mutex);
	return 0;
}

EXPORT_SYMBOL(sprd_vdsp_mem_sync_cpu_to_device);

static void _sprd_vdsp_mem_sync_device_to_cpu(struct buffer *buffer, bool force)
{
	struct mem_man *mem_man = &mem_man_data;
	struct heap *heap = buffer->heap;

	if (!cache_sync) {
		pr_warn("buffer %d size %zu cache synchronization disabled!\n",
			buffer->id, buffer->actual_size);
		return;
	}
	pr_debug("buffer %d size %zu kptr %p cache(%d:%d)\n",
		buffer->id, buffer->actual_size, buffer->kptr, force, heap->cache_sync);

	WARN_ON(!mutex_is_locked(&mem_man->mutex));

	if (heap->ops && heap->ops->sync_dev_to_cpu && (force || heap->cache_sync))
		heap->ops->sync_dev_to_cpu(heap, buffer);
}

int sprd_vdsp_mem_sync_device_to_cpu(struct mem_ctx *ctx, int buf_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;

	pr_debug("buffer %d\n", buf_id);

	mutex_lock(&mem_man->mutex);
	buffer = idr_find(&ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("buffer id %d not found\n", buf_id);
		mutex_unlock(&mem_man->mutex);
		return -EINVAL;
	}

	_sprd_vdsp_mem_sync_device_to_cpu(buffer, false);

	mutex_unlock(&mem_man->mutex);
	return 0;
}

EXPORT_SYMBOL(sprd_vdsp_mem_sync_device_to_cpu);

int sprd_vdsp_mem_get_usage(const struct mem_ctx *ctx, size_t *max, size_t *curr)
{
	struct mem_man *mem_man = &mem_man_data;

	mutex_lock(&mem_man->mutex);
	if (max)
		*max = ctx->mem_usage_max;
	if (curr)
		*curr = ctx->mem_usage_curr;
	mutex_unlock(&mem_man->mutex);

	return 0;
}
EXPORT_SYMBOL(sprd_vdsp_mem_get_usage);


int _sprd_vdsp_mem_map_iova(struct sprd_vdsp_iommus *iommus,
	struct map_buf *pfinfo)
{
	int ret = 0;
	struct sprd_vdsp_iommu_map_conf map_conf = {0};

	if (!pfinfo || !pfinfo->dev) {
		pr_err("invalid input ptr\n");
		return -EINVAL;
	}

	if (unlikely(!iommus)) {
		pr_err("Error iommus is NULL\n");
		return -EINVAL;
	}

	map_conf.buf_addr = ( unsigned long) pfinfo->buf;
	map_conf.iova_size = pfinfo->size;
	map_conf.table = pfinfo->table;
	map_conf.isfixed = pfinfo->isfixed;
	map_conf.fixed_data = pfinfo->fixed_data;

	ret = iommus->ops->map_all(iommus, &map_conf);
	if (ret) {
		pr_err("map fail to get iommu kaddr\n");
		return ret;
	}
	pfinfo->iova = map_conf.iova_addr + pfinfo->offset;

	return ret;
}

int sprd_vdsp_mem_map_iova(struct mem_ctx *mem_ctx, int buf_id, int isfixed, unsigned long fixed_data)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;
	struct heap *heap;
	struct sprd_vdsp_iommus *iommus = NULL;
	int ret;

	pr_debug("map_iova buffer id=%d,\n", buf_id);

	mutex_lock(&mem_man->mutex);
	buffer = idr_find(&mem_ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("buffer id %d not found\n", buf_id);
		ret = -EINVAL;
		goto error;
	}

	heap = buffer->heap;

	pr_debug("map_iova buffer id =%d addr=0x%lx size=%zu, from heap %d\n",
		buf_id, ( unsigned long) buffer, buffer->request_size, heap->id);

	if (heap->ops && heap->ops->get_sg_table) {
		struct sg_table *sgt;
		bool use_sg_dma = false;

	//tyc modify
		ret = heap->ops->get_sg_table(heap, buffer, &sgt, &use_sg_dma);
		if (ret) {
			pr_err("heap %d buffer %d no sg_table!\n", heap->id, buffer->id);
			goto error;
		}

		buffer->map_buf.dev = buffer->device;
		buffer->map_buf.table = sgt;
		buffer->map_buf.buf = buffer;
		buffer->map_buf.size = buffer->request_size;
		buffer->map_buf.offset = 0;
		buffer->map_buf.isfixed = isfixed;
		buffer->map_buf.fixed_data = fixed_data;

		pr_debug("sgt =%p, buffer =%p, size =%zu, isfixed = %d,fixed_data = %ld\n",
			buffer->map_buf.table, buffer->map_buf.buf,
			buffer->map_buf.size, buffer->map_buf.isfixed,
			buffer->map_buf.fixed_data);
		iommus = mem_ctx->xvp->iommus;

		ret = _sprd_vdsp_mem_map_iova(iommus, &buffer->map_buf);
		if (ret) {
			pr_err("Error: _sprd_vdsp_mem_map_iova failed\n");
			goto error;
		}

	} else if (heap->ops && heap->ops->get_page_array) {
		uint64_t *addrs;

		ret = heap->ops->get_page_array(heap, buffer, &addrs);
		if (ret) {
			pr_err("Error: heap %d buffer %d no page array!\n", heap->id, buffer->id);
			goto error;
		}
	} else {
		pr_err("Error: heap %d buffer %d no get_sg or get_page_array!\n",
			heap->id, buffer->id);
		ret = -EINVAL;
		goto error;
	}
	buffer->mapping = BUFFER_MAPPING;
	mutex_unlock(&mem_man->mutex);
	return 0;

error:
	mutex_unlock(&mem_man->mutex);
	return ret;
}

EXPORT_SYMBOL(sprd_vdsp_mem_map_iova);

static int _sprd_vdsp_mem_unmap_iova(struct sprd_vdsp_iommus *iommus,
	struct map_buf *pfinfo)
{

	int ret;
	struct sprd_vdsp_iommu_unmap_conf unmap_conf = {0};

	if (!pfinfo || !pfinfo->dev) {
		pr_err("Error: invalid input ptr\n");
		return -EINVAL;
	}
	if (unlikely(!iommus)) {
		pr_err("Error iommus is NULL\n");
		return -EINVAL;
	}

	unmap_conf.iova_addr = pfinfo->iova - pfinfo->offset;
	unmap_conf.iova_size = pfinfo->size;
	unmap_conf.buf_addr = ( unsigned long) pfinfo->buf;

	ret = iommus->ops->unmap_all(iommus, &unmap_conf);
	if (ret) {
		pr_err("Error: unmap failed to free iommu, ret %d\n", ret);
		return ret;
	}

	return ret;

}

int sprd_vdsp_mem_unmap_iova(struct mem_ctx *mem_ctx, int buf_id)
{
	struct mem_man *mem_man = &mem_man_data;
	struct buffer *buffer;
	struct sprd_vdsp_iommus *iommus = NULL;
	int ret = 0;

	pr_debug("unmap_iova buffer id=%d,\n", buf_id);

	mutex_lock(&mem_man->mutex);

	buffer = idr_find(&mem_ctx->buffers, buf_id);
	if (!buffer) {
		pr_err("Error: buffer id %d not found\n", buf_id);
		ret = -EINVAL;
		goto error;
	}
	pr_debug("unmap_iova buffer id =%d addr=0x%lx size=%zu\n",
		buf_id, ( unsigned long) buffer, buffer->request_size);

	iommus = mem_ctx->xvp->iommus;
	ret = _sprd_vdsp_mem_unmap_iova(iommus, &buffer->map_buf);
	if (ret)
		goto error;

	buffer->mapping = BUFFER_NO_MAPPING;
	buffer->map_buf.iova = 0;
error:
	mutex_unlock(&mem_man->mutex);

	return ret;
}

EXPORT_SYMBOL(sprd_vdsp_mem_unmap_iova);

/*
 * Initialisation
 */
int sprd_vdsp_mem_core_init(void)
{
	struct mem_man *mem_man = &mem_man_data;

	idr_init(&mem_man->heaps);
	idr_init(&mem_man->mem_ctxs);
	mutex_init(&mem_man->mutex);
	mem_man->cache_ref = 0;

	return 0;
}

void sprd_vdsp_mem_core_exit(void)
{
	struct mem_man *mem_man = &mem_man_data;
	struct heap *heap;
	struct mem_ctx *ctx;
	int heap_id;
	int ctx_id;

	/* keeps mutex checks (WARN_ON) happy, this will never actually wait */
	mutex_lock(&mem_man->mutex);

	ctx_id = 0;
	ctx = idr_get_next(&mem_man->mem_ctxs, &ctx_id);
	while (ctx) {
		pr_warn("derelict memory context %p!\n", ctx);
		_sprd_vdsp_mem_destroy_proc_ctx(ctx);
		kfree(ctx);
		ctx_id = 0;
		ctx = idr_get_next(&mem_man->mem_ctxs, &ctx_id);
	}

	heap_id = SPRD_VDSP_MEM_MAN_MIN_HEAP;
	heap = idr_get_next(&mem_man->heaps, &heap_id);
	while (heap) {
		pr_warn("derelict heap %d!\n", heap_id);
		_sprd_vdsp_mem_del_heap(heap);
		kfree(heap);
		heap_id = SPRD_VDSP_MEM_MAN_MIN_HEAP;
		heap = idr_get_next(&mem_man->heaps, &heap_id);
	}
	idr_destroy(&mem_man->heaps);
	idr_destroy(&mem_man->mem_ctxs);

	mutex_unlock(&mem_man->mutex);
	mutex_destroy(&mem_man->mutex);
}
