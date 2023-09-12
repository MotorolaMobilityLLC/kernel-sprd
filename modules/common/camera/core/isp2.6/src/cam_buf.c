/*
 * Copyright (C) 2021-2022 UNISOC Communications Inc.
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
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>
#include <linux/dma-mapping.h>

#include "cam_types.h"
#include "cam_buf.h"
#include "cam_queue.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "cam_buf: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static struct cam_mem_dbg_info s_mem_dbg;
struct cam_mem_dbg_info *g_mem_dbg = &s_mem_dbg;
static atomic_t s_dev_cnt;
struct iommudev_info {
	enum cam_iommudev_type type;
	int32_t iommu_en;
	struct device *dev;

	/* reserved for future using. */
	void *handle;
};
static struct iommudev_info s_iommudevs[CAM_IOMMUDEV_MAX];

/*************** some static interface only for cam_buf ****************/
static int cambuf_mdbg_init(void)
{
	memset(g_mem_dbg, 0, sizeof(struct cam_mem_dbg_info));
	pr_info("reset to 0\n");
	return 0;
}

static struct iommudev_info *cambuf_iommu_dev_get(
	enum cam_iommudev_type type, struct device *dev)
{
	uint32_t i;
	struct iommudev_info *cur;

	if (dev == NULL) {
		if (type >= CAM_IOMMUDEV_MAX)
			return NULL;

		cur = &s_iommudevs[type];
		if ((cur->type == type) && (cur->dev != NULL))
			return cur;

	} else {
		for (i = 0; i < CAM_IOMMUDEV_MAX; i++) {
			cur = &s_iommudevs[i];
			if (cur->dev == dev)
				return cur;
		}
	}

	return NULL;
}

/*************** some externel interface only for camera driver ****************/
int cam_buf_mdbg_check(void)
{
	int val[13];

	val[0] = atomic_read(&g_mem_dbg->ion_alloc_cnt);
	val[1] = atomic_read(&g_mem_dbg->ion_kmap_cnt);
	val[2] = atomic_read(&g_mem_dbg->ion_dma_cnt);
	val[3] = atomic_read(&g_mem_dbg->empty_frm_cnt);
	val[4] = atomic_read(&g_mem_dbg->empty_interruption_cnt);
	val[5] = atomic_read(&g_mem_dbg->iommu_map_cnt[0]);
	val[6] = atomic_read(&g_mem_dbg->iommu_map_cnt[1]);
	val[7] = atomic_read(&g_mem_dbg->iommu_map_cnt[2]);
	val[8] = atomic_read(&g_mem_dbg->iommu_map_cnt[3]);
	val[9] = atomic_read(&g_mem_dbg->empty_state_cnt);
	val[10] = atomic_read(&g_mem_dbg->isp_sw_context_cnt);
	val[11] = atomic_read(&g_mem_dbg->empty_mv_state_cnt);
	val[12] = atomic_read(&g_mem_dbg->ion_alloc_size);

	pr_info("mdbg info: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, total_size: %d Bytes\n",
		val[0], val[1], val[2], val[3], val[4], val[5], val[6], val[7], val[8], val[9], val[10], val[11], val[12]);
	return 0;
}

int cam_buf_iommu_single_page_map(struct camera_buf *buf_info,
	enum cam_iommudev_type type)
{
	int i;
	int ret = 0;
	void *ionbuf[3];
	struct iommudev_info *dev_info;
	struct sprd_iommu_map_data iommu_data;

	dev_info = cambuf_iommu_dev_get(type, NULL);
	if (!buf_info || !dev_info) {
		pr_err("fail to get valid param %p %p\n", buf_info, dev_info);
		return -EFAULT;
	}

	pr_debug("enter.\n");
	for (i = 0; i < 3; i++) {
		if (buf_info->ionbuf[i] == NULL)
			continue;

		ionbuf[i] = buf_info->ionbuf[i];

		if (dev_info->iommu_en && !buf_info->buf_sec) {
			memset(&iommu_data, 0,
				sizeof(struct sprd_iommu_map_data));
			iommu_data.buf = ionbuf[i];
			iommu_data.iova_size = buf_info->size[i];
			iommu_data.ch_type = SPRD_IOMMU_FM_CH_RW;
			pr_debug("start map buf: %p, size: %d\n",
					ionbuf[i], (int)iommu_data.iova_size);
			ret = sprd_iommu_map_single_page(dev_info->dev, &iommu_data);
			if (ret) {
				pr_err("fail to get iommu kaddr %d\n", i);
				ret = -EFAULT;
				goto failed;
			}

			if (g_mem_dbg)
				atomic_inc(&g_mem_dbg->iommu_map_cnt[type]);
			buf_info->iova[i] = iommu_data.iova_addr;
			buf_info->iova[i] += buf_info->offset[i];
			pr_debug("mfd %d, kaddr %p, iova: 0x%08x, off 0x%x, size 0x%x\n",
					buf_info->mfd[i],
					(void *)buf_info->addr_k[i],
					(uint32_t)buf_info->iova[i],
					(uint32_t)buf_info->offset[i],
					(uint32_t)buf_info->size[i]);
		} else {
			ret = cam_buf_get_phys_addr(-1,
					buf_info->dmabuf_p[i],
					&buf_info->iova[i],
					&buf_info->size[i]);
			if (ret) {
				pr_err("fail to get iommu kaddr %d\n", i);
				ret = -EFAULT;
				goto failed;
			}
			buf_info->iova[i] += buf_info->offset[i];
			pr_debug("mfd %d, kaddr %p, iova: 0x%08x, off 0x%x, size 0x%x\n",
					buf_info->mfd[i],
					(void *)buf_info->addr_k[i],
					(uint32_t)buf_info->iova[i],
					(uint32_t)buf_info->offset[i],
					(uint32_t)buf_info->size[i]);
		}
	}
	buf_info->dev = dev_info->dev;
	buf_info->mapping_state |= CAM_BUF_MAPPING_DEV;
	return 0;

failed:
	for (i = 0; i < 3; i++) {
		if (buf_info->size[i] <= 0 || buf_info->iova[i] == 0)
			continue;

		if (dev_info->iommu_en) {
			struct sprd_iommu_unmap_data unmap_data;

			unmap_data.iova_addr = buf_info->iova[i] - buf_info->offset[i];
			unmap_data.iova_size = buf_info->size[i];
			unmap_data.ch_type = SPRD_IOMMU_FM_CH_RW;
			unmap_data.table = NULL;
			unmap_data.buf = NULL;
			ret = sprd_iommu_unmap(dev_info->dev, &unmap_data);
			if (ret)
				pr_err("fail to free iommu %d\n", i);
			if (g_mem_dbg)
				atomic_dec(&g_mem_dbg->iommu_map_cnt[type]);
		}
		buf_info->iova[i] = 0;
	}

	return ret;
}

int cam_buf_kmap(struct camera_buf *buf_info)
{
	int i;
	int ret = 0;

	if (!buf_info) {
		pr_err("fail to get buffer info ptr\n");
		return -EFAULT;
	}

	if ((buf_info->mapping_state & CAM_BUF_MAPPING_KERNEL)) {
		pr_warn("warning: buf type %d status: 0x%x\n",
			buf_info->type, buf_info->mapping_state);
		return -EFAULT;
	}

	for (i = 0; i < 3; i++) {
		if ((buf_info->size[i] <= 0) ||
			(buf_info->dmabuf_p[i] == NULL))
			continue;

		ret = cam_buf_map_kernel(buf_info, i);
		if (IS_ERR_OR_NULL((void *)buf_info->addr_k[i])) {
			pr_err("fail to map k_addr %p for dmabuf[%p]\n",
				(void *)buf_info->addr_k[i], buf_info->dmabuf_p[i]);
			buf_info->addr_k[i] = 0;
			ret = -EINVAL;
			goto map_fail;
		}

		buf_info->addr_k[i] += buf_info->offset[i];

		pr_debug("buf%d, addr_k %p, dmabuf[%p]\n", i,
			(void *)buf_info->addr_k[i], buf_info->dmabuf_p[i]);
		if (g_mem_dbg)
			atomic_inc(&g_mem_dbg->ion_kmap_cnt);
	}
	buf_info->mapping_state |= CAM_BUF_MAPPING_KERNEL;

	pr_debug("done: %p\n", (void *)buf_info->addr_k[0]);
	return 0;

map_fail:
	for (i = 0; i < 3; i++) {
		if ((buf_info->size[i] <= 0) ||
			(buf_info->dmabuf_p[i] == NULL))
			continue;
		ret = cam_buf_unmap_kernel(buf_info, i);
		buf_info->addr_k[i] = 0;
		if (g_mem_dbg)
			atomic_dec(&g_mem_dbg->ion_kmap_cnt);
	}
	return ret;
}

int cam_buf_kunmap(struct camera_buf *buf_info)
{
	int i;
	int ret = 0;

	if (!buf_info) {
		pr_err("fail to get buffer info ptr\n");
		return -EFAULT;
	}

	if (!(buf_info->mapping_state & CAM_BUF_MAPPING_KERNEL)) {
		pr_warn("warning: buf type %d status: 0x%x\n",
			buf_info->type, buf_info->mapping_state);
		return -EFAULT;
	}

	for (i = 0; i < 3; i++) {
		if ((buf_info->size[i] <= 0) ||
			(buf_info->dmabuf_p[i] == NULL))
			continue;

		pr_debug("buf%d, addr_k %p, dmabuf[%p]\n", i,
			(void *)buf_info->addr_k[i], buf_info->dmabuf_p[i]);

		ret = cam_buf_unmap_kernel(buf_info, i);
		buf_info->addr_k[i] = 0;
		if (g_mem_dbg)
			atomic_dec(&g_mem_dbg->ion_kmap_cnt);
	}

	buf_info->mapping_state &= ~CAM_BUF_MAPPING_KERNEL;
	return ret;
}

int cam_buf_alloc(struct camera_buf *buf_info,
		size_t size, unsigned int iommu_enable)
{
	int ret = 0;
	unsigned int flag = 0;
	char name[64];

	if (!buf_info) {
		pr_err("fail to get buffer info ptr\n");
		return -EFAULT;
	}

	snprintf(name, 16+CAM_BUF_NAME_LEN,
			"camera-buf-%s", buf_info->name);

	if (iommu_enable & CAM_BUF_CAHCED)
		flag = ION_FLAG_CACHED;
	iommu_enable &= ~CAM_BUF_CAHCED;

#ifdef TEST_ON_HAPS
	/* force reserved memory during bringup. */
	iommu_enable = 0;
#endif
	ret = cam_buffer_alloc(buf_info, size, iommu_enable, flag);
	if (IS_ERR_OR_NULL(buf_info->dmabuf_p[0])) {
		pr_err("fail to alloc ion buf size = 0x%x\n", (int)size);
		ret = -ENOMEM;
		return ret;
	}

	ret = cam_ion_get_buffer(-1, buf_info->buf_sec,
			buf_info->dmabuf_p[0],
			&buf_info->ionbuf[0],
			&buf_info->size[0]);

	if (ret) {
		pr_err("fail to get ionbuf for kernel buffer %p\n",
				buf_info->dmabuf_p[0]);
		ret = -EFAULT;
		goto failed;
	}

	pr_debug("dmabuf_p[%p], ionbuf[%p], size %d\n",
		buf_info->dmabuf_p[0],
		buf_info->ionbuf[0], (int)buf_info->size[0]);

	buf_info->type = CAM_BUF_KERNEL;
	if (g_mem_dbg) {
		atomic_inc(&g_mem_dbg->ion_alloc_cnt);
		atomic_add((int)buf_info->size[0], &g_mem_dbg->ion_alloc_size);
	}
	pr_debug("alloc done. %p\n", buf_info);
	return 0;

failed:
	cam_buffer_free(buf_info->dmabuf_p[0]);
	buf_info->dmabuf_p[0] = NULL;
	buf_info->ionbuf[0] = NULL;
	buf_info->size[0] = 0;
	return ret;
}

int cam_buf_free(struct camera_buf *buf_info)
{
	int rtn = 0;
	int size;
	struct dma_buf *dmabuf = NULL;

	if (!buf_info) {
		pr_err("fail to get buffer info ptr\n");
		return -EFAULT;
	}

	if (buf_info->type != CAM_BUF_KERNEL) {
		pr_err("fail to get correct buffer type: %d\n", buf_info->type);
		return -EPERM;
	}

	if (buf_info->mapping_state != CAM_BUF_MAPPING_NULL) {
		pr_err("fail to get correct mapping state %x. maybe addr leak.\n",
				buf_info->mapping_state);
	}

	dmabuf = buf_info->dmabuf_p[0];
	if (dmabuf) {
		cam_buffer_free(dmabuf);
		size = (int)buf_info->size[0];
		buf_info->dmabuf_p[0] = NULL;
		buf_info->mfd[0] = 0;
		buf_info->size[0] = 0;
		buf_info->ionbuf[0] = NULL;
		if (g_mem_dbg) {
			atomic_dec(&g_mem_dbg->ion_alloc_cnt);
			atomic_sub(size, &g_mem_dbg->ion_alloc_size);
		}
	}

	pr_debug("free done: %p, dmabuf[%p]\n", buf_info, dmabuf);
	return rtn;
}

/*************** some externel interface not only for camera driver ****************/
int cam_buf_iommudev_reg(struct device *dev,
	enum cam_iommudev_type type)
{
	if (type < CAM_IOMMUDEV_MAX) {

		s_iommudevs[type].type = type;
		s_iommudevs[type].dev = dev;
		s_iommudevs[type].handle = NULL;
		s_iommudevs[type].iommu_en = 0;

		if (atomic_inc_return(&s_dev_cnt) == 1)
			cambuf_mdbg_init();

		/* change mode when camera open */
		if (type == CAM_IOMMUDEV_DCAM)
			g_dbg_iommu_mode = g_dbg_set_iommu_mode;

		if (g_dbg_iommu_mode == IOMMU_AUTO) {
			if (sprd_iommu_attach_device(dev) == 0)
				s_iommudevs[type].iommu_en = 1;
		} else if (g_dbg_iommu_mode == IOMMU_OFF) {
			s_iommudevs[type].iommu_en = 0;
		} else {
			s_iommudevs[type].iommu_en = 1;
		}
		pr_info("dev %d, iommu_mode %d iommu_hw_en %d\n",
			type, g_dbg_iommu_mode, s_iommudevs[type].iommu_en);
	}

	return 0;
}
EXPORT_SYMBOL(cam_buf_iommudev_reg);

int cam_buf_iommudev_unreg(enum cam_iommudev_type type)
{
	if (type < CAM_IOMMUDEV_MAX) {
		if (!s_iommudevs[type].dev)
			return 0;
		atomic_dec(&s_dev_cnt);
		s_iommudevs[type].type = CAM_IOMMUDEV_MAX;
		s_iommudevs[type].dev = NULL;
		s_iommudevs[type].handle = NULL;
		s_iommudevs[type].iommu_en = 0;
	}
	return 0;
}
EXPORT_SYMBOL(cam_buf_iommudev_unreg);

int cam_buf_iommu_status_get(enum cam_iommudev_type type)
{
	int ret = -ENODEV;
	struct iommudev_info *cur;

	if (type >= CAM_IOMMUDEV_MAX)
		return ret;

	cur = &s_iommudevs[type];
	if ((cur->type == type) && (cur->dev != NULL)) {
		int enable;

		if (g_dbg_iommu_mode == IOMMU_AUTO)
			ret = cur->iommu_en ? 0 : -1;
		else if (g_dbg_iommu_mode == IOMMU_ON)
			ret = 0;
		else
			ret = -1;
		enable = (ret == 0) ? 1 : 0;
		pr_info("dev %d, iommu_mode %d en %d\n",
			type, g_dbg_iommu_mode, enable);
	}

	return ret;
}
EXPORT_SYMBOL(cam_buf_iommu_status_get);

int cam_buf_ionbuf_get(struct camera_buf *buf_info)
{
	int i;
	int ret = 0;
	void *ionbuf[3];
	enum cam_iommudev_type type;
	struct iommudev_info *dev_info = NULL;

	if (!buf_info) {
		pr_err("fail to get buffer info ptr\n");
		return -EFAULT;
	}
	if (buf_info->type != CAM_BUF_USER) {
		pr_err("fail to get correct buffer type: %d\n", buf_info->type);
		return -EFAULT;
	}

	pr_debug("enter.\n");
	for (i = 0; i < 3; i++) {
		if (buf_info->mfd[i] <= 0)
			continue;
		pr_debug("user buf:  %d,  %d\n", i, buf_info->mfd[i]);

		ret = cam_ion_get_buffer(buf_info->mfd[i], buf_info->buf_sec,
				NULL,
				&ionbuf[i],
				&buf_info->size[i]);
		if (ret) {
			pr_err("fail to get ionbuf for user buffer %d\n",
					buf_info->mfd[i]);
			goto failed;
		}
		buf_info->ionbuf[i] = ionbuf[i];
		pr_debug("size %d, ionbuf %p\n",
			(int)buf_info->size[i], ionbuf[i]);

		/*
		 * get dambuf to avoid ion buf is freed during
		 * hardware access it.
		 * In fact, this situation will happen when application exit
		 * exceptionally and for user buffer only.
		 * Just get dmabuf when hardware ready to access it
		 * for user buffer can avoid previous situation.
		 * We get dmabuf here because if the buffer is passed to kernel,
		 * there is potential hardware access.
		 * And it must be put before return to user space,
		 * or else it may cause ion memory leak.
		 */
		if (buf_info->dmabuf_p[i] == NULL) {
			buf_info->dmabuf_p[i] =
					dma_buf_get(buf_info->mfd[i]);
			if (IS_ERR_OR_NULL(buf_info->dmabuf_p[i])) {
				pr_err("fail to get dma buf %p\n",
						buf_info->dmabuf_p[i]);
				ret = -EINVAL;
				goto failed;
			}
			if (g_mem_dbg)
				atomic_inc(&g_mem_dbg->ion_dma_cnt);
			pr_debug("dmabuf %p\n", buf_info->dmabuf_p[i]);
		}

	}
	if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) {
		for (type = CAM_IOMMUDEV_ISP; type < CAM_IOMMUDEV_MAX; type++) {
			dev_info = cambuf_iommu_dev_get(type, NULL);
			if (!dev_info)
				continue;
			if (dma_set_mask(dev_info->dev, DMA_BIT_MASK(64))) {
				dev_warn(dev_info->dev, "mydev: No suitable DMA available\n");
				goto failed;
			}
			for (i = 0; i < 3; i++) {
				if (buf_info->mfd[i] <= 0)
					continue;
				buf_info->attachment[i][type] = dma_buf_attach(buf_info->dmabuf_p[i], dev_info->dev);
				if (IS_ERR_OR_NULL(buf_info->attachment[i][type])) {
					pr_err("fail to attach dmabuf %px\n", (void *)buf_info->dmabuf_p[i]);
					ret = -EINVAL;
					goto failed;
				}
				buf_info->table[i][type] = dma_buf_map_attachment(buf_info->attachment[i][type], DMA_BIDIRECTIONAL);
				if (IS_ERR_OR_NULL(buf_info->table[i][type])) {
					pr_err("fail to map attachment %px\n", (void *)buf_info->attachment[i][type]);
					ret = -EINVAL;
					goto map_attachment_failed;
				}
			}
		}
	}
	return 0;

map_attachment_failed:
	for (type = CAM_IOMMUDEV_ISP; type < CAM_IOMMUDEV_MAX; type++) {
		for (i = 0; i < 3; i++) {
			if (buf_info->mfd[i] <= 0)
				continue;
			if (!IS_ERR_OR_NULL(buf_info->attachment[i][type]))
				dma_buf_detach(buf_info->dmabuf_p[i], buf_info->attachment[i][type]);
		}
	}
failed:
	for (i = 0; i < 3; i++) {
		if (buf_info->mfd[i] <= 0)
			continue;
		if (!IS_ERR_OR_NULL(buf_info->dmabuf_p[i])) {
			dma_buf_put(buf_info->dmabuf_p[i]);
			buf_info->dmabuf_p[i] = NULL;
			if (g_mem_dbg)
				atomic_dec(&g_mem_dbg->ion_dma_cnt);
		}
		buf_info->ionbuf[i] = NULL;
	}
	return ret;
}
EXPORT_SYMBOL(cam_buf_ionbuf_get);

int cam_buf_ionbuf_put(struct camera_buf *buf_info)
{
	int i;
	int ret = 0;
	enum cam_iommudev_type type;

	if (!buf_info) {
		pr_err("fail to get buffer info ptr\n");
		return -EFAULT;
	}
	if (buf_info->type != CAM_BUF_USER) {
		pr_debug("buffer type: %d is not user buf.\n", buf_info->type);
		return 0;
	}

	pr_debug("enter.\n");
	for (i = 0; i < 3; i++) {
		if (buf_info->mfd[i] <= 0)
			continue;
		if (IS_ERR_OR_NULL(buf_info->dmabuf_p[i]))
			continue;
		for (type = CAM_IOMMUDEV_ISP; type < CAM_IOMMUDEV_MAX; type++) {
			if (!IS_ERR_OR_NULL(buf_info->table[i][type]))
				dma_buf_unmap_attachment(buf_info->attachment[i][type], buf_info->table[i][type], DMA_BIDIRECTIONAL);
			if (!IS_ERR_OR_NULL(buf_info->attachment[i][type]))
				dma_buf_detach(buf_info->dmabuf_p[i], buf_info->attachment[i][type]);
		}

		if (!IS_ERR_OR_NULL(buf_info->dmabuf_p[i])) {
			if (!IS_ERR_OR_NULL(buf_info->dmabuf_p[i]->file) &&
				virt_addr_valid(buf_info->dmabuf_p[i]->file))
				dma_buf_put(buf_info->dmabuf_p[i]);
			buf_info->dmabuf_p[i] = NULL;
			if (g_mem_dbg)
				atomic_dec(&g_mem_dbg->ion_dma_cnt);
		}
		buf_info->ionbuf[i] = NULL;
	}
	return ret;
}
EXPORT_SYMBOL(cam_buf_ionbuf_put);

int cam_buf_iommu_restore(enum cam_iommudev_type type)
{
	struct iommudev_info *dev_info;
	uint32_t ret;
	dev_info = cambuf_iommu_dev_get(type, NULL);
	ret = sprd_iommu_restore(dev_info->dev);
	if (ret != 0)
		pr_info("fail to iommu enable\n");
	return ret;
}

int cam_buf_iommu_map(struct camera_buf *buf_info,
	enum cam_iommudev_type type)
{
	int i;
	int ret = 0;
	void *ionbuf[3];
	struct iommudev_info *dev_info;
	struct sprd_iommu_map_data iommu_data;

	dev_info = cambuf_iommu_dev_get(type, NULL);
	if (!buf_info || !dev_info) {
		pr_err("fail to get valid param %p %p\n", buf_info, dev_info);
		return -EFAULT;
	}

	pr_debug("enter.\n");
	for (i = 0; i < 3; i++) {
		if (buf_info->ionbuf[i] == NULL)
			continue;
		ionbuf[i] = buf_info->ionbuf[i];

		if (dev_info->iommu_en && !buf_info->buf_sec) {
			memset(&iommu_data, 0,
				sizeof(struct sprd_iommu_map_data));
			iommu_data.buf = ionbuf[i];
			iommu_data.iova_size = buf_info->size[i];
			iommu_data.ch_type = SPRD_IOMMU_FM_CH_RW;
			pr_debug("start map buf: %p, size: %d\n",
					ionbuf[i], (int)iommu_data.iova_size);
			ret = sprd_iommu_map(dev_info->dev, &iommu_data);
			if (ret) {
				pr_err("fail to get iommu kaddr %d\n", i);
				ret = -EFAULT;
				goto failed;
			}

			if (g_mem_dbg)
				atomic_inc(&g_mem_dbg->iommu_map_cnt[type]);
			buf_info->iova[i] = iommu_data.iova_addr;
			buf_info->iova[i] += buf_info->offset[i];
			pr_debug("mfd %d, dmap %px, addr_k %px, iova: 0x%08x, off 0x%x, size 0x%x\n",
					buf_info->mfd[i],
					(void *)buf_info->dmabuf_p[i],
					(void *)buf_info->addr_k[i],
					(uint32_t)buf_info->iova[i],
					(uint32_t)buf_info->offset[i],
					(uint32_t)buf_info->size[i]);
		} else {
			ret = cam_buf_get_phys_addr(-1,
					buf_info->dmabuf_p[i],
					&buf_info->iova[i],
					&buf_info->size[i]);
			if (ret) {
				pr_err("fail to get iommu kaddr %d\n", i);
				ret = -EFAULT;
				goto failed;
			}
			buf_info->iova[i] += buf_info->offset[i];
			pr_debug("mfd %d, kaddr %p, iova: 0x%08x, off 0x%x, size 0x%x\n",
					buf_info->mfd[i],
					(void *)buf_info->addr_k[i],
					(uint32_t)buf_info->iova[i],
					(uint32_t)buf_info->offset[i],
					(uint32_t)buf_info->size[i]);
		}
	}
	buf_info->dev = dev_info->dev;
	buf_info->mapping_state |= CAM_BUF_MAPPING_DEV;
	return 0;

failed:
	for (i = 0; i < 3; i++) {
		if (buf_info->size[i] <= 0 || buf_info->iova[i] == 0)
			continue;

		if (dev_info->iommu_en) {
			struct sprd_iommu_unmap_data unmap_data;

			unmap_data.iova_addr = buf_info->iova[i] - buf_info->offset[i];
			unmap_data.iova_size = buf_info->size[i];
			unmap_data.ch_type = SPRD_IOMMU_FM_CH_RW;
			unmap_data.table = NULL;
			unmap_data.buf = NULL;
			ret = sprd_iommu_unmap(dev_info->dev, &unmap_data);
			if (ret)
				pr_err("fail to free iommu %d\n", i);
			if (g_mem_dbg)
				atomic_dec(&g_mem_dbg->iommu_map_cnt[type]);
		}
		buf_info->iova[i] = 0;
	}
	return ret;
}
EXPORT_SYMBOL(cam_buf_iommu_map);

int cam_buf_iommu_unmap(struct camera_buf *buf_info)
{
	int i;
	int ret = 0;
	struct iommudev_info *dev_info;
	struct sprd_iommu_unmap_data unmap_data;

	if (!buf_info) {
		pr_err("fail to get buffer info ptr\n");
		return -EFAULT;
	}

	if (!buf_info->dev ||
		((buf_info->mapping_state & CAM_BUF_MAPPING_DEV) == 0)) {
		pr_info("buf dev %p, may not be mapping %d\n",
			buf_info->dev, buf_info->mapping_state);
		return ret;
	}

	dev_info = cambuf_iommu_dev_get(CAM_IOMMUDEV_MAX, buf_info->dev);
	if (!dev_info) {
		pr_err("fail to get matched iommu dev.\n");
		return -EFAULT;
	}

	for (i = 0; i < 3; i++) {
		if (buf_info->size[i] <= 0 || buf_info->iova[i] == 0)
			continue;

		if (dev_info->iommu_en && !buf_info->buf_sec) {
			unmap_data.iova_addr = buf_info->iova[i] - buf_info->offset[i];
			unmap_data.iova_size = buf_info->size[i];
			unmap_data.ch_type = SPRD_IOMMU_FM_CH_RW;
			unmap_data.table = NULL;
			unmap_data.buf = NULL;
			pr_debug("upmap buf addr: %lx\n", unmap_data.iova_addr);
			ret = sprd_iommu_unmap(buf_info->dev, &unmap_data);
			if (ret) {
				pr_err("fail to free iommu %d\n", i);
				goto exit;
			}
			if (g_mem_dbg && !ret)
				atomic_dec(&g_mem_dbg->iommu_map_cnt[dev_info->type]);
		}
		buf_info->iova[i] = 0;
	}

exit:
	buf_info->dev = NULL;
	buf_info->mapping_state &= ~(CAM_BUF_MAPPING_DEV);
	pr_debug("unmap done.\n");
	return ret;
}
EXPORT_SYMBOL(cam_buf_iommu_unmap);
