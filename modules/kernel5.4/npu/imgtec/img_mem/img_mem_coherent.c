/*!
 *****************************************************************************
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

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/gfp.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>

#include <img_mem_man.h>
#include "img_mem_man_priv.h"

struct buffer_data {
	void *kptr;
	dma_addr_t dma_handle; /* addr returned by dma_alloc_coherent */
	uint64_t *addrs; /* array of physical addresses, upcast to 64-bit */
	struct device *dev;
	size_t size;
	/* exporter via dmabuf */
	struct sg_table *sgt;
	bool exported;
	struct dma_buf *dma_buf;
};

static int trace_physical_pages;

/*
 * dmabuf wrapper ops
 */
static struct sg_table *coherent_map_dmabuf(struct dma_buf_attachment *attach,
		enum dma_data_direction dir)
{
	struct buffer *buffer = attach->dmabuf->priv;
	struct buffer_data *buffer_data;

	if (!buffer)
		return NULL;

	pr_debug("%s\n", __func__);

	buffer_data = buffer->priv;
	sg_dma_address(buffer_data->sgt->sgl) = buffer_data->dma_handle;
	sg_dma_len(buffer_data->sgt->sgl) = buffer_data->size;

	return buffer_data->sgt;
}

static void coherent_unmap_dmabuf(struct dma_buf_attachment *attach,
					struct sg_table *sgt,
					enum dma_data_direction dir)
{
	struct buffer *buffer = attach->dmabuf->priv;
	struct buffer_data *buffer_data;

	if (!buffer)
		return;

	pr_debug("%s\n", __func__);

	buffer_data = buffer->priv;
	sg_dma_address(buffer_data->sgt->sgl) = (~(dma_addr_t)0);
	sg_dma_len(buffer_data->sgt->sgl) = 0;
}

/* Called when when ref counter reaches zero! */
static void coherent_release_dmabuf(struct dma_buf *buf)
{
	struct buffer *buffer = buf->priv;
	struct buffer_data *buffer_data;

	if (!buffer)
		return;

	buffer_data = buffer->priv;
	pr_debug("%s %p\n", __func__, buffer_data);
	if (!buffer_data)
		return;

	buffer_data->exported = false;
}

/* Called on file descriptor mmap */
static int coherent_mmap_dmabuf(struct dma_buf *buf, struct vm_area_struct *vma)
{
	struct buffer *buffer = buf->priv;
	struct buffer_data *buffer_data;
	struct scatterlist *sgl;
	unsigned long addr;

	if (!buffer)
		return -EINVAL;

	buffer_data = buffer->priv;

	pr_debug("%s:%d buffer %d (0x%p)\n", __func__, __LINE__,
		buffer->id, buffer);
	pr_debug("%s:%d vm_start %#lx vm_end %#lx size %#lx\n",
		__func__, __LINE__,
		vma->vm_start, vma->vm_end, vma->vm_end - vma->vm_start);

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	sgl = buffer_data->sgt->sgl;
	addr = vma->vm_start;
	while (sgl) {
		dma_addr_t phys = sg_phys(sgl);
		unsigned long pfn = phys >> PAGE_SHIFT;
		unsigned int len = sgl->length;
		int ret;

		if (vma->vm_end < (addr + len)) {
			unsigned long size = vma->vm_end - addr;
			pr_debug("%s:%d buffer %d (0x%p) truncating len=%#x to size=%#lx\n",
				__func__, __LINE__,
				buffer->id, buffer, len, size);
			WARN(round_up(size, PAGE_SIZE) != size,
				"VMA size %#lx not page aligned\n", size);
			len = size;
			if (!len) /* VM space is smaller than allocation */
				break;
		}

		ret = remap_pfn_range(vma, addr, pfn, len, vma->vm_page_prot);
		if (ret)
			return ret;

		addr += len;
		sgl = sg_next(sgl);
	}

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,19,0)
static void *coherent_kmap_dmabuf(struct dma_buf *buf, unsigned long page)
{
	struct buffer *buffer = buf->priv;

	if (!buffer)
		return NULL;

	return buffer->kptr;
}
#endif

static int coherent_heap_map_km(struct heap *heap, struct buffer *buffer);
static int coherent_heap_unmap_km(struct heap *heap, struct buffer *buffer);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,11,0)
static void *coherent_vmap_dmabuf(struct dma_buf *buf)
{
	struct buffer *buffer = buf->priv;
	struct heap *heap;

	if (!buffer)
		return NULL;

	heap = buffer->heap;

	if (coherent_heap_map_km(heap, buffer))
		return NULL;

	pr_debug("%s:%d buffer %d kptr 0x%p\n", __func__, __LINE__,
		buffer->id, buffer->kptr);

	return buffer->kptr;
}

static void coherent_vunmap_dmabuf(struct dma_buf *buf, void *kptr)
{
	struct buffer *buffer = buf->priv;
	struct heap *heap;

	if (!buffer)
		return;

	heap = buffer->heap;

	pr_debug("%s:%d buffer %d kptr 0x%p (0x%p)\n", __func__, __LINE__,
		buffer->id, buffer->kptr, kptr);

	if (buffer->kptr == kptr)
		coherent_heap_unmap_km(heap, buffer);
}
#else
static int coherent_vmap_dmabuf(struct dma_buf *buf, struct dma_buf_map *map)
{
	struct buffer *buffer = buf->priv;
	struct heap *heap;
	int ret = 0;

	if (!buffer || !map)
		return -EINVAL;

	heap = buffer->heap;
	ret = coherent_heap_map_km(heap, buffer);
	if (ret)
		return ret;

	pr_debug("%s:%d buffer %d kptr 0x%p\n", __func__, __LINE__,
		buffer->id, buffer->kptr);

	dma_buf_map_set_vaddr(map, buffer->kptr);

	return ret;
}

static void coherent_vunmap_dmabuf(struct dma_buf *buf, struct dma_buf_map *map)
{
	struct buffer *buffer = buf->priv;
	struct heap *heap;

	if (!buffer || !map)
		return;

	heap = buffer->heap;

	pr_debug("%s:%d buffer %d kptr 0x%p (0x%p)\n", __func__, __LINE__,
		buffer->id, buffer->kptr, map->vaddr);

	if (buffer->kptr == map->vaddr)
		coherent_heap_unmap_km(heap, buffer);
	dma_buf_map_clear(map);
}
#endif

static const struct dma_buf_ops coherent_dmabuf_ops = {
	.map_dma_buf = coherent_map_dmabuf,
	.unmap_dma_buf = coherent_unmap_dmabuf,
	.release = coherent_release_dmabuf,
	.mmap = coherent_mmap_dmabuf,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,12,0)
	.kmap_atomic = coherent_kmap_dmabuf,
	.kmap = coherent_kmap_dmabuf,
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,19,0)
	.map_atomic = coherent_kmap_dmabuf,
	.map = coherent_kmap_dmabuf,
#endif
#endif
	.vmap = coherent_vmap_dmabuf,
	.vunmap = coherent_vunmap_dmabuf,
};

static int coherent_heap_export(struct device *device, struct heap *heap,
						 size_t size, enum img_mem_attr attr,
						 struct buffer *buffer, uint64_t* buf_hnd)
{
	struct buffer_data *buffer_data = buffer->priv;
	struct dma_buf *dma_buf;
	int ret, fd;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0)
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
#endif
	pr_debug("%s:%d buffer %d (0x%p)\n", __func__, __LINE__,
		buffer->id, buffer);

	if (!buffer_data)
		/* Nothing to export ? */
		return -ENOMEM;

	if (buffer_data->exported) {
		pr_err("%s: already exported!\n", __func__);
		return -EBUSY;
	}

	if (!buffer_data->sgt) {
		/* Create for the very first time */
		buffer_data->sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
		if (!buffer_data->sgt) {
			pr_err("%s: failed to allocate sg_table\n", __func__);
			return -ENOMEM;
		}

		ret = sg_alloc_table(buffer_data->sgt, 1, GFP_KERNEL);
		if (ret) {
			pr_err("%s: sg_alloc_table failed\n", __func__);
			goto free_sgt_mem;
		}
		sg_set_page(buffer_data->sgt->sgl,
				pfn_to_page(PFN_DOWN(buffer_data->dma_handle)), /*virt_to_page ?*/
				PAGE_ALIGN(size), 0);
		/* No mapping yet */
		sg_dma_address(buffer_data->sgt->sgl) = (~(dma_addr_t)0);
		sg_dma_len(buffer_data->sgt->sgl) = 0;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,17,0)
	dma_buf = dma_buf_export(buffer_data, &coherent_dmabuf_ops,
			size, O_RDWR);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4,1,0)
	dma_buf = dma_buf_export(buffer_data, &coherent_dmabuf_ops,
			size, O_RDWR, NULL);
#else
	exp_info.ops = &coherent_dmabuf_ops;
	exp_info.size = size;
	exp_info.flags = O_RDWR;
	exp_info.priv = buffer;
	exp_info.resv = NULL;
	dma_buf = dma_buf_export(&exp_info);
#endif
	if (IS_ERR(dma_buf)) {
		pr_err("%s:dma_buf_export failed\n", __func__);
		ret = PTR_ERR(dma_buf);
		return ret;
	}

	get_dma_buf(dma_buf);
	fd = dma_buf_fd(dma_buf, 0);
	if (fd < 0) {
		pr_err("%s: dma_buf_fd failed\n", __func__);
		dma_buf_put(dma_buf);
		return -EFAULT;
	}
	buffer_data->dma_buf = dma_buf;
	buffer_data->exported = true;
	*buf_hnd = (uint64_t)fd;

	return 0;

free_sgt_mem:
	kfree(buffer_data->sgt);
	buffer_data->sgt = NULL;

	return ret;
}

static int coherent_heap_alloc(struct device *device, struct heap *heap,
			       size_t size, enum img_mem_attr attr,
			       struct buffer *buffer)
{
	struct buffer_data *buffer_data;
	phys_addr_t phys_addr;
	size_t pages, page;

	pr_debug("%s:%d buffer %d (0x%p) size:%zu attr:%x\n", __func__, __LINE__,
		buffer->id, buffer, size, attr);

	buffer_data = kzalloc(sizeof(struct buffer_data), GFP_KERNEL);
	if (!buffer_data)
		return -ENOMEM;

	pages = size / PAGE_SIZE;
	/* Check if buffer is not too big. */
	if (get_order(pages * sizeof(uint64_t)) >= MAX_ORDER) {
		pr_err("%s: buffer size is too big (%zu bytes)\n", __func__, size);
		kfree(buffer_data);
		return -ENOMEM;
	}
	buffer_data->addrs = kmalloc_array(pages, sizeof(uint64_t), GFP_KERNEL);
	if (!buffer_data->addrs) {
		kfree(buffer_data);
		return -ENOMEM;
	}

	buffer_data->dev = device;
	buffer_data->size = size;
	buffer_data->kptr = dma_alloc_coherent(device,
				size,
				&buffer_data->dma_handle,
				heap->options.coherent.gfp_flags);
	if (!buffer_data->kptr) {
		pr_err("%s dma_alloc_coherent failed!\n", __func__);
		kfree(buffer_data->addrs);
		kfree(buffer_data);
		return -ENOMEM;
	}
	buffer->kptr = (void *)buffer_data->kptr;

	page = 0;
	phys_addr = buffer_data->dma_handle;
	while (page < pages) {
		if (trace_physical_pages)
			pr_info("%s phys %llx\n",
				 __func__, (unsigned long long)phys_addr);
		buffer_data->addrs[page++] = phys_addr;
		phys_addr += PAGE_SIZE;
	};

	buffer->priv = buffer_data;

	pr_debug("%s buffer %d kptr %p phys %#llx size %zu\n", __func__,
		 buffer->id, buffer->kptr,
		 (unsigned long long)buffer_data->addrs[0], size);
	return 0;
}

static void coherent_heap_free(struct heap *heap, struct buffer *buffer)
{
	struct buffer_data *buffer_data = buffer->priv;

	pr_debug("%s:%d buffer %d (0x%p)\n", __func__, __LINE__,
		 buffer->id, buffer);

	if (buffer_data->dma_buf) {
		dma_buf_put(buffer_data->dma_buf);
		buffer_data->dma_buf->priv = NULL;
	}

	if (buffer_data->sgt) {
		sg_free_table(buffer_data->sgt);
		kfree(buffer_data->sgt);
		buffer_data->sgt = NULL;
	}

	dma_free_coherent(buffer_data->dev, buffer_data->size,
			  buffer_data->kptr, buffer_data->dma_handle);
	kfree(buffer_data->addrs);
	kfree(buffer_data);
}

static int coherent_heap_map_um(struct heap *heap, struct buffer *buffer,
			       struct vm_area_struct *vma)
{
	struct buffer_data *buffer_data = buffer->priv;
	unsigned long pfn = *buffer_data->addrs >> PAGE_SHIFT;

	pr_debug("%s:%d buffer %d (0x%p)\n", __func__, __LINE__,
		 buffer->id, buffer);
	pr_debug("%s:%d vm_start %#lx vm_end %#lx size %ld\n",
		 __func__, __LINE__,
		 vma->vm_start, vma->vm_end, vma->vm_end - vma->vm_start);

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	return remap_pfn_range(vma, vma->vm_start, pfn,
			       vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

static int coherent_heap_map_km(struct heap *heap, struct buffer *buffer)
{
	pr_debug("%s:%d buffer %d (0x%p) kptr 0x%p\n", __func__, __LINE__,
		 buffer->id, buffer, buffer->kptr);

	return 0;
}

static int coherent_heap_unmap_km(struct heap *heap, struct buffer *buffer)
{
	pr_debug("%s:%d buffer %d (0x%p) kptr 0x%p\n", __func__, __LINE__,
		 buffer->id, buffer, buffer->kptr);

	return 0;
}

static int coherent_heap_get_page_array(struct heap *heap,
					struct buffer *buffer,
					uint64_t **addrs)
{
	struct buffer_data *buffer_data = buffer->priv;

	*addrs = buffer_data->addrs;
	return 0;
}

static void coherent_heap_destroy(struct heap *heap)
{
	pr_debug("%s:%d\n", __func__, __LINE__);
}

static struct heap_ops coherent_heap_ops = {
	.export = coherent_heap_export,
	.alloc = coherent_heap_alloc,
	.import = NULL,
	.free = coherent_heap_free,
	.map_um = coherent_heap_map_um,
	.unmap_um = NULL,
	.map_km = coherent_heap_map_km,
	.unmap_km = coherent_heap_unmap_km,
	.get_sg_table = NULL,
	.get_page_array = coherent_heap_get_page_array,
	.sync_cpu_to_dev = NULL,
	.sync_dev_to_cpu = NULL,
	.set_offset = NULL,
	.destroy = coherent_heap_destroy,
};

int img_mem_coherent_init(const struct heap_config *config, struct heap *heap)
{
	pr_debug("%s gfp:%x\n", __func__,
		 config->options.coherent.gfp_flags);

	heap->ops = &coherent_heap_ops;

	return 0;
}
