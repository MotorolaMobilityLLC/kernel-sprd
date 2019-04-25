/*
 * Copyright (C) 2019 Unisoc Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/err.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>
#include "cam_iommu.h"
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>

struct fd_map_dma {
	struct list_head list;
	int fd;
	void *dma_buf;
};
static LIST_HEAD(dma_buffer_list);
static DEFINE_MUTEX(dma_buffer_lock);

static int dma_buffer_list_add(int fd, void *buf)
{
	struct fd_map_dma *fd_dma = NULL;
	struct list_head *g_dma_buffer_list = &dma_buffer_list;

	list_for_each_entry(fd_dma, g_dma_buffer_list, list) {
		if (fd == fd_dma->fd && buf == fd_dma->dma_buf)
			return 0;
	}

	fd_dma = kzalloc(sizeof(struct fd_map_dma), GFP_KERNEL);
	if (!fd_dma)
		return -ENOMEM;

	fd_dma->fd = fd;
	fd_dma->dma_buf = buf;
	mutex_lock(&dma_buffer_lock);
	list_add_tail(&fd_dma->list, g_dma_buffer_list);
	mutex_unlock(&dma_buffer_lock);
	dma_buf_get(fd_dma->fd);
	pr_info("%s, add 0x%x 0x%p\n", __func__, fd_dma->fd,
		((struct dma_buf *)fd_dma->dma_buf));

	return 0;
}

void dma_buffer_list_clear(void)
{
	struct fd_map_dma *fd_dma = NULL;
	struct fd_map_dma *fd_dma_next = NULL;
	struct list_head *g_dma_buffer_list = &dma_buffer_list;

	list_for_each_entry_safe(fd_dma, fd_dma_next, g_dma_buffer_list, list) {
		mutex_lock(&dma_buffer_lock);
		list_del(&fd_dma->list);
		mutex_unlock(&dma_buffer_lock);
		dma_buf_put(fd_dma->dma_buf);
		pr_info("%s, del: 0x%x 0x%p\n", __func__,
			fd_dma->fd,
		((struct dma_buf *)fd_dma->dma_buf));
		kfree(fd_dma);
	}
}

int pfiommu_get_sg_table(struct pfiommu_info *pfinfo)
{
	int i, ret;

	for (i = 0; i < 2; i++) {
		if (pfinfo->mfd[i] > 0) {
			ret = sprd_ion_get_buffer(pfinfo->mfd[i],
						    NULL,
						    &pfinfo->buf[i],
						    &pfinfo->size[i]);
			if (ret) {
				pr_err("failed to get sg table %d mfd 0x%x\n",
					i, pfinfo->mfd[i]);
				return -EFAULT;
			}

			pfinfo->dmabuf_p[i] = dma_buf_get(pfinfo->mfd[i]);
			if (IS_ERR_OR_NULL(pfinfo->dmabuf_p[i])) {
				pr_err("failed to get dma buf %p\n",
				       pfinfo->dmabuf_p[i]);
				return -EFAULT;
			}
			dma_buf_put(pfinfo->dmabuf_p[i]);
			dma_buffer_list_add(pfinfo->mfd[i],
					    pfinfo->dmabuf_p[i]);
		}
	}

	return 0;
}

int  pfiommu_put_sg_table(void)
{
	dma_buffer_list_clear();
	return 0;
}

int pfiommu_get_addr(struct pfiommu_info *pfinfo)
{
	int i;
	int ret = 0;
	struct sprd_iommu_map_data iommu_data;

	pr_debug("%s, cb: %pS\n", __func__, __builtin_return_address(0));

	for (i = 0; i < 2; i++) {
		if (pfinfo->size[i] <= 0)
			continue;

		if (sprd_iommu_attach_device(pfinfo->dev) == 0) {
			memset(&iommu_data, 0x00, sizeof(iommu_data));
			iommu_data.buf = pfinfo->buf[i];
			iommu_data.iova_size = pfinfo->size[i];
			iommu_data.ch_type = SPRD_IOMMU_FM_CH_RW;
			iommu_data.sg_offset = pfinfo->offset[i];

			ret = sprd_iommu_map(pfinfo->dev, &iommu_data);
			if (ret) {
				pr_err("failed to get iommu kaddr %d\n", i);
				return -EFAULT;
			}

			pfinfo->iova[i] = iommu_data.iova_addr;
		} else {
			ret = sprd_ion_get_phys_addr(-1, pfinfo->dmabuf_p[i],
					       &pfinfo->iova[i],
					       &pfinfo->size[i]);
			pfinfo->iova[i] += pfinfo->offset[i];
		}
	}

	return ret;
}

int pfiommu_free_addr(struct pfiommu_info *pfinfo)
{
	int i, ret;
	struct sprd_iommu_unmap_data iommu_data;

	pr_debug("%s, cb: %pS, iova 0x%lx\n",
		 __func__, __builtin_return_address(0), pfinfo->iova[0]);
	for (i = 0; i < 2; i++) {
		if (pfinfo->size[i] <= 0 || pfinfo->iova[i] == 0)
			continue;

		if (sprd_iommu_attach_device(pfinfo->dev) == 0) {
			iommu_data.iova_addr = pfinfo->iova[i];
			iommu_data.table = pfinfo->table[i];
			iommu_data.iova_size = pfinfo->size[i];
			iommu_data.ch_type = SPRD_IOMMU_FM_CH_RW;
			iommu_data.buf = NULL;

			ret = sprd_iommu_unmap(pfinfo->dev, &iommu_data);
			if (!ret) {
				pfinfo->iova[i] = 0;
				pfinfo->size[i] = 0;
			} else {
				pr_err("fail to free iommu %d\n", i);
				return -EFAULT;
			}
		}
	}

	return 0;
}

int pfiommu_free_addr_with_id(struct pfiommu_info *pfinfo,
	enum sprd_iommu_chtype ctype, unsigned int cid)
{
	int i, ret;
	struct sprd_iommu_unmap_data iommu_data;

	pr_debug("%s, cb: %pS, iova 0x%lx\n",
		 __func__, __builtin_return_address(0), pfinfo->iova[0]);
	for (i = 0; i < 2; i++) {
		if (pfinfo->size[i] <= 0 || pfinfo->iova[i] == 0)
			continue;

		if (sprd_iommu_attach_device(pfinfo->dev) == 0) {
			iommu_data.iova_addr = pfinfo->iova[i];
			iommu_data.iova_size = pfinfo->size[i];
			iommu_data.ch_type = ctype;
			iommu_data.buf = NULL;
			iommu_data.channel_id = cid;

			ret = sprd_iommu_unmap(pfinfo->dev,
					&iommu_data);
			if (!ret) {
				pfinfo->iova[i] = 0;
				pfinfo->size[i] = 0;
			} else {
				pr_err("failed to free iommu %d\n", i);
				return -EFAULT;
			}
		}
	}

	return 0;
}
