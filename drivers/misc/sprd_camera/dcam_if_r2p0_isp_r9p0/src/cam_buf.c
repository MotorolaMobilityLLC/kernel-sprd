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
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/of.h>

#include "cam_common.h"
#include "cam_buf.h"

/*
 **************************************************************************
 * to debug iommu alloc/free map/unmap flow, enable CAM_BUF_DEBUG
 * to debug which iova is not unmap further, enable CAM_BUF_DEBUG_MAP
 **************************************************************************
 */

/*#define CAM_BUF_DEBUG*/
/*#define CAM_BUF_DEBUG_MAP*/

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "cam_buf: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define ION
#ifdef ION
#include "ion.h"
#endif

#define CAM_BUF_DEBUG
#ifdef CAM_BUF_DEBUG
#define CAM_BUF_TRACE                      pr_info
#else
#define CAM_BUF_TRACE                      pr_debug
#endif

struct fd_map_dma {
	struct list_head list;
	int type;
	int fd;
	void *dma_buf;
	int size;
};

static struct list_head dma_buffer_list[CAMERA_MAX_COUNT] = {
	{&(dma_buffer_list[0]), &(dma_buffer_list[0])},
	{&(dma_buffer_list[1]), &(dma_buffer_list[1])},
	{&(dma_buffer_list[2]), &(dma_buffer_list[2])}
};

static DEFINE_MUTEX(dma_buffer_lock);

/*
 *******************************************************************************
 * s_cambuf_allocsize[]  to found whether kenerl allocated iommu buf size for
 * one camera is free to zero when finish calling sprd_cam_buf_sg_table_put().
 * If it is not zero, there is something wrong.
 * s_cambuf_mapcnt[]  to found whether all mapping iommu buf count for one
 * camera is unmapped to zero when finish calling sprd_cam_buf_sg_table_put().
 * If it is not zero,there is something wrong.
 * s_cambuf_mapsize[][2]  to found whether all mapping iommu buf size for one
 * camera is unmapped to zero when finish calling sprd_cam_buf_sg_table_put().
 * If it is not zero,there is something wrong.
 * where [][0] is for dcam,[][1] is for isp.
 * s_cambuf_max_mapsize[][2]  to found each dcam and isp max mapping iommu size
 * when finish calling sprd_cam_buf_sg_table_put(). The whole max maping size
 * for dcam or isp mustn't be above real iommu capacity.
 * where [][0] is for dcam,[][1] is for isp.
 *******************************************************************************
 */
static atomic_t s_cambuf_allocsize[CAMERA_MAX_COUNT] = {
	ATOMIC_INIT(0), ATOMIC_INIT(0), ATOMIC_INIT(0)
};
static atomic_t s_cambuf_mapcnt[CAMERA_MAX_COUNT] = {
	ATOMIC_INIT(0), ATOMIC_INIT(0), ATOMIC_INIT(0)
};
static uint32_t s_cambuf_mapsize[CAMERA_MAX_COUNT][2] = {
	{0, 0}, {0, 0}, {0, 0}
};
static uint32_t s_cambuf_max_mapsize[CAMERA_MAX_COUNT][2] = {
	{0, 0}, {0, 0}, {0, 0}
};

#ifdef CAM_BUF_DEBUG_MAP
static unsigned long map_iova[0x100] = {0};
static DEFINE_MUTEX(map_iova_mutex);

static int sprd_cambuf_usediova_find(unsigned long iova)
{
	int i;

	for (i = 0; i < 0x100; i++) {
		if (map_iova[i] == iova)
			return i;
	}
	return -1;
}

static int sprd_cambuf_unusediova_find(unsigned long iova)
{
	int i;

	for (i = 0; i < 0x100; i++) {
		if (map_iova[i] == 0)
			return i;
	}
	return -1;
}
#endif

static void sprd_cambuf_log_iova(unsigned long iova, uint32_t idx)
{
#ifdef CAM_BUF_DEBUG_MAP
	int i;

	iova |= (1 << idx);

	mutex_lock(&map_iova_mutex);
	i = sprd_cambuf_unusediova_find(iova);
	if (i >= 0)
		map_iova[i] = iova;
	mutex_unlock(&map_iova_mutex);
#endif
}

static void sprd_cambuf_unlog_iova(unsigned long iova, uint32_t idx)
{
#ifdef CAM_BUF_DEBUG_MAP
	int i;

	iova |= (1 << idx);

	mutex_lock(&map_iova_mutex);
	i = sprd_cambuf_usediova_find(iova);
	if (i >= 0)
		map_iova[i] = 0;
	mutex_unlock(&map_iova_mutex);
#endif
}

static void sprd_cambuf_iova_print(uint32_t idx)
{
#ifdef CAM_BUF_DEBUG_MAP
	int i;

	mutex_lock(&map_iova_mutex);
	for (i = 0; i < 0x100; i++) {
		if ((map_iova[i] & 0x0f) == (1 << idx))
			pr_info("isp%d iova 0x%x\n",
			idx,
			(uint32_t) map_iova[i] & (~0x0f));
	}
	mutex_unlock(&map_iova_mutex);
#endif
}

static int sprd_cambuf_buffer_list_add(int type, int fd,
			void *buf, int size, int idx)
{
	struct fd_map_dma *fd_dma = NULL;
	struct list_head *g_dma_buffer_list = NULL;

	g_dma_buffer_list = &dma_buffer_list[idx];

	list_for_each_entry(fd_dma, g_dma_buffer_list, list)
		if (fd_dma->type == type && fd_dma->dma_buf == buf)
			return 0;

	fd_dma = kzalloc(sizeof(struct fd_map_dma), GFP_KERNEL);
	if (!fd_dma)
		return -ENOMEM;

	fd_dma->type = type;
	fd_dma->fd = fd;
	fd_dma->dma_buf = buf;
	fd_dma->size = size;
	mutex_lock(&dma_buffer_lock);
	list_add_tail(&fd_dma->list, g_dma_buffer_list);
	mutex_unlock(&dma_buffer_lock);

	if (type == CAM_BUF_USER_TYPE)
		dma_buf_get(fd_dma->fd);

	CAM_BUF_TRACE("add type %d 0x%p to list\n", type, fd_dma->dma_buf);

	return 0;
}

static int sprd_cambuf_free(int type, struct dma_buf *dmabuf,
			int size, int idx)
{
	if (!dmabuf || size <= 0)
		return -EFAULT;

	if (type == CAM_BUF_USER_TYPE)
		return -EPERM;

	ion_free(dmabuf);
	atomic_sub(size, &s_cambuf_allocsize[idx]);
	CAM_BUF_TRACE("free buf ok, dmabuf %p total %d\n",
		dmabuf,
		atomic_read(&s_cambuf_allocsize[idx]));

	return 0;
}

static int sprd_cambuf_map(struct cam_buf_info *buf_info)
{
	int ret = 0;
	int i = 0;
	const char *name = NULL;
	int dev_index = 0;
	struct sprd_iommu_map_data iommu_data;
	int iova_i = 0;
	int idx = 0;

	if (!buf_info || !buf_info->dev ||
		buf_info->num > 3 || buf_info->num == 0 ||
		(buf_info->type == CAM_BUF_SWAP_TYPE && buf_info->num > 1)) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	name = buf_info->dev->of_node->name;
	if (strstr(name, "dcam"))
		dev_index = 0;
	else if (strstr(name, "isp"))
		dev_index = 1;
	idx = buf_info->idx;

	CAM_BUF_TRACE("enter... num %d\n", buf_info->num);
	for (i = 0; i < buf_info->num; i++) {
		if (buf_info->size[i] <= 0 || buf_info->dmabuf_p[i] == NULL)
			continue;

		iova_i = i;
		if (sprd_iommu_attach_device(buf_info->dev) == 0) {
			memset(&iommu_data, 0x00, sizeof(iommu_data));

			iommu_data.buf = buf_info->buf[i];
			iommu_data.iova_size = buf_info->size[i];
			iommu_data.ch_type = SPRD_IOMMU_FM_CH_RW;
			iommu_data.sg_offset = 0;
			CAM_BUF_TRACE("start map buf %p size 0x%x\n",
				buf_info->buf[i],
				(int)buf_info->size[i]);
			ret = sprd_iommu_map(buf_info->dev, &iommu_data);
			if (ret) {
				pr_err("fail to get iommu kaddr\n");
				return -EFAULT;
			}

			if (buf_info->type == CAM_BUF_KERNEL_TYPE) {
				buf_info->kaddr[i] = (uint32_t *)
					sprd_ion_map_kernel(
						buf_info->dmabuf_p[i], 0);
				if (CAM_ADDR_INVALID(buf_info->kaddr[i])) {
					pr_err("fail to map kernel vir_addr\n");
					return -EPERM;
				}
				buf_info->state |= CAM_BUF_STATE_MAPPING;
			} else if (buf_info->type == CAM_BUF_SWAP_TYPE) {
				if (strstr(name, "dcam")) {
					iova_i = 0;
					buf_info->state |=
						CAM_BUF_STATE_MAPPING_DCAM;
				} else if (strstr(name, "isp")) {
					iova_i = 1;
					buf_info->state |=
						CAM_BUF_STATE_MAPPING_ISP;
				}
			}
			buf_info->iova[iova_i] = iommu_data.iova_addr +
					buf_info->offset[i];
		} else {
			unsigned long phys_addr = 0;

			ret = sprd_ion_get_phys_addr_by_db(
					buf_info->dmabuf_p[i],
					&phys_addr,
					&buf_info->size[i]);
			if (ret) {
				pr_err("fail to get phys addr\n");
				return -EFAULT;
			}

			if (buf_info->type == CAM_BUF_KERNEL_TYPE) {
				buf_info->state |= CAM_BUF_STATE_MAPPING;
				buf_info->kaddr[i] = phys_to_virt(
					(unsigned long)phys_addr);
			} else if (buf_info->type == CAM_BUF_SWAP_TYPE) {
				if (strstr(name, "dcam")) {
					iova_i = 0;
					buf_info->state |=
						CAM_BUF_STATE_MAPPING_DCAM;
				} else if (strstr(name, "isp")) {
					iova_i = 1;
					buf_info->state |=
						CAM_BUF_STATE_MAPPING_ISP;
				}
			}
			buf_info->iova[iova_i] = phys_addr +
					buf_info->offset[i];
		}

		sprd_cambuf_buffer_list_add((int)buf_info->type,
			0, (void *)buf_info->dmabuf_p[i],
			(int)buf_info->size[i], (int)buf_info->idx);
		s_cambuf_mapsize[idx][dev_index] += buf_info->size[i];
		s_cambuf_max_mapsize[idx][dev_index] =
			max(s_cambuf_mapsize[idx][dev_index],
			s_cambuf_max_mapsize[idx][dev_index]);
		atomic_inc(&s_cambuf_mapcnt[idx]);
		if (iova_i)
			CAM_BUF_TRACE("%s%d, 0x%x => 0x%x,0x%x total %d 0x%x\n",
				name,
				idx,
				(int)buf_info->iova[0],
				(int)buf_info->iova[1],
				(int)buf_info->size[i],
				atomic_read(&s_cambuf_mapcnt[idx]),
				s_cambuf_mapsize[idx][dev_index]);
		else
			CAM_BUF_TRACE("%s%d, 0x%x,0x%x total %d 0x%x\n",
				name,
				idx,
				(int)buf_info->iova[iova_i],
				(int)buf_info->size[i],
				atomic_read(&s_cambuf_mapcnt[idx]),
				s_cambuf_mapsize[idx][dev_index]);
		sprd_cambuf_log_iova(buf_info->iova[iova_i], idx);
	}

	return ret;
}

static int sprd_cambuf_unmap(struct cam_buf_info *buf_info)
{
	int ret = 0;
	int i = 0;
	const char *name = NULL;
	int dev_index = 0;
	struct sprd_iommu_unmap_data unmap_data;
	int iova_i = 0;
	int idx = 0;

	if (!buf_info || !buf_info->dev ||
		buf_info->num > 3 || buf_info->num == 0 ||
		(buf_info->type == CAM_BUF_SWAP_TYPE && buf_info->num > 1)) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	name = buf_info->dev->of_node->name;
	if (strstr(name, "dcam"))
		dev_index = 0;
	else if (strstr(name, "isp"))
		dev_index = 1;
	idx = buf_info->idx;

	CAM_BUF_TRACE("enter... num %d\n", buf_info->num);
	for (i = 0; i < buf_info->num; i++) {
		if (buf_info->size[i] <= 0 || buf_info->dmabuf_p[i] == NULL)
			continue;

		iova_i = i;
		memset(&unmap_data, 0x00, sizeof(unmap_data));
		if (sprd_iommu_attach_device(buf_info->dev) == 0) {
			if (buf_info->type == CAM_BUF_KERNEL_TYPE) {
				buf_info->kaddr[i] = NULL;
				buf_info->state &= ~CAM_BUF_STATE_MAPPING;
			} else if (buf_info->type == CAM_BUF_SWAP_TYPE) {
				if (strstr(name, "dcam")) {
					iova_i = 0;
					buf_info->state &=
						~CAM_BUF_STATE_MAPPING_DCAM;
				} else if (strstr(name, "isp")) {
					iova_i = 1;
					buf_info->state &=
						~CAM_BUF_STATE_MAPPING_ISP;
				}
			}

			if (buf_info->iova[iova_i] <= 0)
				continue;

			unmap_data.iova_addr = buf_info->iova[iova_i] -
					buf_info->offset[i];
			unmap_data.iova_size = buf_info->size[i];
			unmap_data.ch_type = SPRD_IOMMU_FM_CH_RW;
			unmap_data.table = NULL;
			unmap_data.buf = NULL;
			CAM_BUF_TRACE("upmap buf addr: 0x%lx\n",
				unmap_data.iova_addr);
			ret = sprd_iommu_unmap(buf_info->dev, &unmap_data);
			if (ret) {
				pr_err("fail to unmap iommu kaddr\n");
				return -EFAULT;
			}
			buf_info->iova[iova_i] = 0;
		} else {
			if (buf_info->type == CAM_BUF_KERNEL_TYPE) {
				buf_info->kaddr[i] = NULL;
				buf_info->state &= ~CAM_BUF_STATE_MAPPING;
			} else if (buf_info->type == CAM_BUF_SWAP_TYPE) {
				if (strstr(name, "dcam")) {
					iova_i = 0;
					buf_info->state &=
						~CAM_BUF_STATE_MAPPING_DCAM;
				} else if (strstr(name, "isp")) {
					iova_i = 1;
					buf_info->state &=
						~CAM_BUF_STATE_MAPPING_ISP;
				}
			}
			if (buf_info->iova[iova_i] > 0) {
				unmap_data.iova_addr = buf_info->iova[iova_i];
				buf_info->iova[iova_i] = 0;
			}
		}

		s_cambuf_mapsize[idx][dev_index] -= buf_info->size[i];
		atomic_dec(&s_cambuf_mapcnt[idx]);
		CAM_BUF_TRACE("%s%d, iova 0x%x,0x%x total %d 0x%x\n",
			name,
			idx,
			(int)unmap_data.iova_addr,
			(int)buf_info->size[i],
			atomic_read(&s_cambuf_mapcnt[idx]),
				s_cambuf_mapsize[idx][dev_index]);
		sprd_cambuf_unlog_iova(unmap_data.iova_addr, idx);
	}

	return ret;
}

int sprd_cam_buf_sg_table_get(struct cam_buf_info *buf_info)
{
	int i = 0, ret = 0;
	int cnt = 2;
	void *ionbuf[3] = {NULL};

	if (!buf_info) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	if (buf_info->type != CAM_BUF_USER_TYPE)
		return 0;

	if (buf_info->offset[0] == buf_info->offset[1])
		cnt = 1;

	CAM_BUF_TRACE("cb: %pS\n", __builtin_return_address(0));
	for (i = 0; i < cnt; i++) {
		if (buf_info->mfd[i] <= 0)
			continue;

		pr_info("user buf: %d 0x%x 0x%x\n",
			i, buf_info->mfd[i], buf_info->offset[i]);
		ret = sprd_ion_get_buffer(buf_info->mfd[i],
				NULL,
				&ionbuf[i],
				&buf_info->size[i]);
		if (ret) {
			pr_err("fail to get sg table %d mfd 0x%x\n",
				i, buf_info->mfd[i]);
			return -EFAULT;
		}
		buf_info->buf[i] = ionbuf[i];
		pr_info("size 0x%x, ionbuf %p\n",
			(int)buf_info->size[i],
			ionbuf[i]);

		buf_info->dmabuf_p[i] = dma_buf_get(buf_info->mfd[i]);
		if (IS_ERR_OR_NULL(buf_info->dmabuf_p[i])) {
			pr_err("fail to get dma buf %p\n",
				buf_info->dmabuf_p[i]);
			return -EFAULT;
		}
		dma_buf_put(buf_info->dmabuf_p[i]);

		sprd_cambuf_buffer_list_add(CAM_BUF_USER_TYPE,
				(int)buf_info->mfd[i],
				(void *)buf_info->dmabuf_p[i],
				(int)buf_info->size[i],
				(int)buf_info->idx);
	}

	return 0;
}

int sprd_cam_buf_sg_table_put(uint32_t idx)
{
	struct fd_map_dma *fd_dma = NULL;
	struct fd_map_dma *fd_dma_next = NULL;
	struct list_head *buffer_list = NULL;

	buffer_list = &dma_buffer_list[idx];

	list_for_each_entry_safe(fd_dma, fd_dma_next, buffer_list, list) {
		mutex_lock(&dma_buffer_lock);
		list_del(&fd_dma->list);
		mutex_unlock(&dma_buffer_lock);
		if (fd_dma->type == CAM_BUF_USER_TYPE) {
			dma_buf_put(fd_dma->dma_buf);
			CAM_BUF_TRACE("del user buf: 0x%x 0x%p\n",
				fd_dma->fd,
				fd_dma->dma_buf);
		} else {
			sprd_cambuf_free(fd_dma->type,
				fd_dma->dma_buf,
				fd_dma->size,
				(int)idx);
			pr_info("del kernel buf: %p size 0x%x\n",
				fd_dma->dma_buf,
				fd_dma->size);
		}
		kfree(fd_dma);
	}

	pr_info("cam%d total size 0x%x count %d\n",
		idx,
		atomic_read(&s_cambuf_allocsize[idx]),
		atomic_read(&s_cambuf_mapcnt[idx]));
	pr_info("cam%d map size 0x%x 0x%x 0x%x x%x\n",
		idx,
		s_cambuf_mapsize[idx][0],
		s_cambuf_mapsize[idx][1],
		s_cambuf_max_mapsize[idx][0],
		s_cambuf_max_mapsize[idx][1]);
	sprd_cambuf_iova_print(idx);
	atomic_set(&s_cambuf_allocsize[idx], 0);
	atomic_set(&s_cambuf_mapcnt[idx], 0);
	s_cambuf_mapsize[idx][0] = 0;
	s_cambuf_mapsize[idx][1] = 0;
	s_cambuf_max_mapsize[idx][0] = 0;
	s_cambuf_max_mapsize[idx][1] = 0;

	return 0;
}

int sprd_cam_buf_addr_map(struct cam_buf_info *buf_info)
{
	int i = 0, ret = 0;
	int cnt = 2;
	struct sprd_iommu_map_data iommu_data;
	const char *name;
	int dev_index = 0;
	uint32_t idx = 0;

	CAM_BUF_TRACE("cb: %pS\n", __builtin_return_address(0));
	if (!buf_info || !buf_info->dev
		|| buf_info->num > 3 || buf_info->num == 0) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	if (buf_info->type != CAM_BUF_USER_TYPE)
		return sprd_cambuf_map(buf_info);

	name = buf_info->dev->of_node->name;
	if (strstr(name, "dcam"))
		dev_index = 0;
	else if (strstr(name, "isp"))
		dev_index = 1;
	idx = buf_info->idx;

	if (buf_info->offset[0] == buf_info->offset[1])
		cnt = 1;

	pr_info("enter... num %d cnt %d\n", buf_info->num, cnt);
	for (i = 0; i < cnt; i++) {
		if (!buf_info->buf[i])
			continue;

		if (sprd_iommu_attach_device(buf_info->dev) == 0) {
			memset(&iommu_data, 0x00, sizeof(iommu_data));
			iommu_data.buf = buf_info->buf[i];
			iommu_data.iova_size = buf_info->size[i];
			iommu_data.ch_type = SPRD_IOMMU_FM_CH_RW;
			pr_info("start map buf %p size 0x%x\n",
				buf_info->buf[i],
				(int)iommu_data.iova_size);

			ret = sprd_iommu_map(buf_info->dev, &iommu_data);
			if (ret) {
				pr_err("fail to get iommu kaddr %d\n", i);
				ret = -EFAULT;
				goto failed;
			}
			buf_info->iova[i] = iommu_data.iova_addr +
					buf_info->offset[i];
		} else {
			ret = sprd_ion_get_phys_addr(-1,
					buf_info->dmabuf_p[i],
					&buf_info->iova[i],
					&buf_info->size[i]);
			if (ret) {
				pr_err("fail to get iommu kaddr %d\n", i);
				return -EFAULT;
			}
			buf_info->iova[i] += buf_info->offset[i];
		}
		s_cambuf_mapsize[idx][dev_index] += buf_info->size[i];
		s_cambuf_max_mapsize[idx][dev_index] =
			max(s_cambuf_mapsize[idx][dev_index],
				s_cambuf_max_mapsize[idx][dev_index]);
		atomic_inc(&s_cambuf_mapcnt[idx]);
		CAM_BUF_TRACE("%s%d, iova 0x%08x size 0x%x total %d 0x%x\n",
			name,
			idx,
			(uint32_t)buf_info->iova[i],
			(uint32_t)buf_info->size[i],
			atomic_read(&s_cambuf_mapcnt[idx]),
			s_cambuf_mapsize[idx][dev_index]);
		sprd_cambuf_log_iova(buf_info->iova[i], idx);
	}
	buf_info->state |= CAM_BUF_STATE_MAPPING;
	return 0;

failed:
	for (i = 0; i < cnt; i++) {
		if (buf_info->size[i] <= 0 || buf_info->iova[i] == 0)
			continue;

		if (sprd_iommu_attach_device(buf_info->dev) == 0) {
			struct sprd_iommu_unmap_data unmap_data;

			unmap_data.iova_addr = buf_info->iova[i];
			unmap_data.iova_size = buf_info->size[i];
			unmap_data.ch_type = SPRD_IOMMU_FM_CH_RW;
			unmap_data.table = NULL;
			unmap_data.buf = NULL;
			ret = sprd_iommu_unmap(buf_info->dev, &unmap_data);
			if (ret) {
				pr_err("failed to free iommu %d\n", i);
				continue;
			}
		}
		s_cambuf_mapsize[idx][dev_index] -= buf_info->size[i];
		atomic_dec(&s_cambuf_mapcnt[idx]);
		buf_info->iova[i] = 0;
	}

	return ret;
}

int sprd_cam_buf_addr_unmap(struct cam_buf_info *buf_info)
{
	int i = 0, ret = 0;
	int cnt = 2;
	struct sprd_iommu_unmap_data unmap_data;
	const char *name;
	int dev_index = 0;
	size_t size = 0;
	int idx = 0;

	CAM_BUF_TRACE("cb: %pS\n", __builtin_return_address(0));
	if (!buf_info || !buf_info->dev ||
		buf_info->num > 3 || buf_info->num == 0) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	if (buf_info->type != CAM_BUF_USER_TYPE)
		return sprd_cambuf_unmap(buf_info);

	name = buf_info->dev->of_node->name;
	if (strstr(name, "dcam"))
		dev_index = 0;
	else if (strstr(name, "isp"))
		dev_index = 1;
	idx = buf_info->idx;

	if (buf_info->offset[0] == buf_info->offset[1])
		cnt = 1;

	CAM_BUF_TRACE("num %d cnt %d\n", buf_info->num, cnt);
	for (i = 0; i < cnt; i++) {
		if (buf_info->size[i] <= 0 || buf_info->iova[i] == 0)
			continue;

		if (sprd_iommu_attach_device(buf_info->dev) == 0) {
			unmap_data.iova_addr = buf_info->iova[i] -
					buf_info->offset[i];
			unmap_data.iova_size = buf_info->size[i];
			unmap_data.ch_type = SPRD_IOMMU_FM_CH_RW;
			unmap_data.table = NULL;
			unmap_data.buf = NULL;
			ret = sprd_iommu_unmap(buf_info->dev, &unmap_data);
			if (ret) {
				pr_err("failed to free iommu %d\n", i);
				continue;
			}
		}
		buf_info->iova[i] = 0;
		s_cambuf_mapsize[idx][dev_index] -= buf_info->size[i];
		atomic_dec(&s_cambuf_mapcnt[idx]);
		CAM_BUF_TRACE("%s%d iova 0x%08x size 0x%x total %d 0x%x\n",
			name,
			idx,
			(uint32_t)unmap_data.iova_addr,
			(uint32_t)size,
			atomic_read(&s_cambuf_mapcnt[idx]),
			s_cambuf_mapsize[idx][dev_index]);
		sprd_cambuf_unlog_iova(unmap_data.iova_addr, idx);
	}
	buf_info->state &= ~CAM_BUF_STATE_MAPPING;

	return 0;
}

int sprd_cam_buf_alloc(struct cam_buf_info *buf_info, uint32_t idx,
	struct device *dev, size_t size, uint32_t num, uint32_t type)
{
	int ret = 0;
	int heap_type = 0;
	int i = 0;

	if (!buf_info || !dev || num > 3 || num == 0
		|| (type == CAM_BUF_SWAP_TYPE && num > 1)) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	memset(buf_info, 0x00, sizeof(*buf_info));

	CAM_BUF_TRACE("cb: %pS\n", __builtin_return_address(0));
	for (i = 0; i < num; i++) {
		heap_type = sprd_iommu_attach_device(dev) ?
			ION_HEAP_ID_MASK_MM :
			ION_HEAP_ID_MASK_SYSTEM;

		buf_info->dmabuf_p[i] = ion_new_alloc(size, heap_type, 0);
		if (IS_ERR_OR_NULL(buf_info->dmabuf_p[i])) {
			pr_err("failed to alloc ion buf size = 0x%x %ld\n",
				(int)size,
				(unsigned long)buf_info->dmabuf_p[i]);
			return -ENOMEM;
		}

		ret = sprd_ion_get_buffer(-1,
				buf_info->dmabuf_p[i],
				&buf_info->buf[i],
				&buf_info->size[i]);
		if (ret) {
			pr_err("failed to get ion buf for kernel buffer %p\n",
				buf_info->dmabuf_p[i]);
			ret = -EFAULT;
			goto failed;
		}

		pr_info("dmabuf_p[%p], buf[%p], size 0x%x, heap %d\n",
			buf_info->dmabuf_p[i],
			buf_info->buf[i],
			(int)buf_info->size[i],
			heap_type);
	}
	buf_info->idx = idx;
	buf_info->dev = dev;
	buf_info->type = type;
	buf_info->num = num;
	for (i = 0; i < num; i++)
		atomic_add(buf_info->size[i], &s_cambuf_allocsize[idx]);
	CAM_BUF_TRACE("alloc %d buffer ok, idx %d total 0x%x\n",
		num, idx, atomic_read(&s_cambuf_allocsize[idx]));
	return 0;

failed:
	for (i = 0; i < num; i++) {
		if (buf_info->dmabuf_p[i]) {
			ion_free(buf_info->dmabuf_p[i]);
			buf_info->dmabuf_p[i] = NULL;
			buf_info->buf[i] = NULL;
			buf_info->size[i] = 0;
		}
	}
	return ret;
}

unsigned long sprd_cam_buf_get_kaddr(int fd)
{
	struct dma_buf *dmabuf_p;
	unsigned long kaddr = 0;

	if (fd <= 0)
		return 0;

	dmabuf_p = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf_p)) {
		pr_err("fail to get dma buf %p\n", dmabuf_p);
		return 0;
	}

	kaddr = (unsigned long)sprd_ion_map_kernel(dmabuf_p, 0);
	if (CAM_ADDR_INVALID(kaddr))
		pr_err("fail to map kernel vir_addr\n");

	dma_buf_put(dmabuf_p);

	return kaddr;
}
