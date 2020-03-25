// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
//
// * Copyright (C) 2020 Unisoc Inc.
//

#include <asm/cacheflush.h>
#include <linux/compat.h>
#include <linux/dma-buf.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sprd_ion.h>
#include <linux/uaccess.h>
#include "linux/ion.h"


static struct ion_buffer *get_ion_buffer(int fd, struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer;

	if (fd < 0 && !dmabuf) {
		pr_err("%s, input fd: %d, dmabuf: %p error\n", __func__, fd,
		       dmabuf);
		return ERR_PTR(-EINVAL);
	}

	if (fd >= 0) {
		dmabuf = dma_buf_get(fd);
		if (IS_ERR_OR_NULL(dmabuf)) {
			pr_err("%s, dmabuf=%p dma_buf_get error!\n", __func__,
			       dmabuf);
			return ERR_PTR(-EBADF);
		}
		buffer = dmabuf->priv;
		dma_buf_put(dmabuf);
	} else {
		buffer = dmabuf->priv;
	}

	return buffer;
}

int sprd_ion_get_buffer(int fd, struct dma_buf *dmabuf,
			void **buf, size_t *size)
{
	struct ion_buffer *buffer;

	buffer = get_ion_buffer(fd, dmabuf);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	*buf = (void *)buffer;
	*size = buffer->size;

	return 0;
}
EXPORT_SYMBOL(sprd_ion_get_buffer);

int sprd_ion_get_phys_addr(int fd, struct dma_buf *dmabuf,
			   unsigned long *phys_addr, size_t *size)
{
	int ret = 0;
	struct ion_buffer *buffer;
	struct sg_table *table = NULL;
	struct scatterlist *sgl = NULL;

	buffer = get_ion_buffer(fd, dmabuf);
	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	if (buffer->heap->type != ION_HEAP_TYPE_SYSTEM ||
	    buffer->size == 0x1000) {
		table = buffer->sg_table;
		if (table && table->sgl) {
			sgl = table->sgl;
		} else {
			if (!table)
				pr_err("invalid table\n");
			else if (!table->sgl)
				pr_err("invalid table->sgl\n");
			return -EINVAL;
		}

		*phys_addr = sg_phys(sgl);
		*size = buffer->size;
	} else {
		pr_err("%s, buffer heap type:%d error\n", __func__,
		       buffer->heap->type);
		return -EPERM;
	}

	return ret;
}
EXPORT_SYMBOL(sprd_ion_get_phys_addr);

void *sprd_ion_map_kernel(struct dma_buf *dmabuf, unsigned long offset)
{
	void *vaddr;

	if (!dmabuf)
		return ERR_PTR(-EINVAL);

	dmabuf->ops->begin_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	vaddr = dmabuf->ops->map(dmabuf, offset);

	return vaddr;
}
EXPORT_SYMBOL(sprd_ion_map_kernel);

int sprd_ion_unmap_kernel(struct dma_buf *dmabuf, unsigned long offset)
{
	if (!dmabuf)
		return -EINVAL;

	dmabuf->ops->unmap(dmabuf, offset, NULL);
	dmabuf->ops->end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);

	return 0;
}
EXPORT_SYMBOL(sprd_ion_unmap_kernel);

int ion_debug_heap_show_printk(struct ion_heap *heap, void *data)
{
	return 0;
}

static int sprd_ion_probe(struct platform_device *pdev)
{
	return 0;
}

static int sprd_ion_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id sprd_ion_ids[] = {
	{ .compatible = "sprd,ion"},
	{},
};

static struct platform_driver sprd_ion_driver = {
	.probe = sprd_ion_probe,
	.remove = sprd_ion_remove,
	.driver = {
		.name = "ion",
		.of_match_table = of_match_ptr(sprd_ion_ids),
	},
};

module_platform_driver(sprd_ion_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Unisoc ION Driver");
MODULE_AUTHOR("Sheng Xu <sheng.xu@unisoc.com>");

