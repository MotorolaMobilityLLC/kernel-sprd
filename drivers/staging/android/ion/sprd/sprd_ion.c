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
#include <../ion_private.h>
#include <uapi/linux/sprd_ion.h>
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

static long sprd_ion_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct ion_phy_data data;

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	/*
	 * The copy_from_user is unconditional here for both read and write
	 * to do the validate. If there is no write for the ioctl, the
	 * buffer is cleared
	 */
	if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	if (!(_IOC_DIR(cmd) & _IOC_WRITE))
		memset(&data, 0, sizeof(data));

	switch (cmd) {
	case ION_IOC_PHY:
	{
		int fd = data.fd;

		ret = sprd_ion_get_phys_addr(fd, NULL, (unsigned long *)&data.addr,
			      (size_t *)&data.len);
		break;
	}
	default:
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
	}
	return ret;
}
static const struct file_operations sprd_ion_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = sprd_ion_ioctl,
	.compat_ioctl = sprd_ion_ioctl,
};

static int sprd_ion_device_create(void)
{
	struct ion_device *sprd_ion_dev;
	int ret;

	sprd_ion_dev = kzalloc(sizeof(*sprd_ion_dev), GFP_KERNEL);
	if (!sprd_ion_dev)
		return -ENOMEM;

	sprd_ion_dev->dev.minor = MISC_DYNAMIC_MINOR;
	sprd_ion_dev->dev.name = "sprd_ion";
	sprd_ion_dev->dev.fops = &sprd_ion_fops;
	sprd_ion_dev->dev.parent = NULL;
	ret = misc_register(&sprd_ion_dev->dev);
	if (ret) {
		pr_err("ion: failed to register misc device.\n");
		goto err_reg;
	}

	init_rwsem(&sprd_ion_dev->lock);
	plist_head_init(&sprd_ion_dev->heaps);
	return 0;

err_reg:
	kfree(sprd_ion_dev);
	return ret;
}

static int sprd_ion_probe(struct platform_device *pdev)
{
	return sprd_ion_device_create();
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

