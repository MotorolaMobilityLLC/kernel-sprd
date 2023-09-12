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

#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include "cam_kernel_adapt.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "cam_porting: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
timeval cam_ktime_to_timeval(const ktime_t kt)
{
	timespec temp_timespec = {0};
	timeval temp_timeval = {0};

	temp_timespec = ktime_to_timespec64(kt);
	temp_timeval.tv_sec = temp_timespec.tv_sec;
	temp_timeval.tv_usec = temp_timespec.tv_nsec / 1000L;

	return temp_timeval;
}

timespec cam_timespec_sub(timespec lhs, timespec rhs)
{
	return timespec64_sub(lhs,rhs);
}

int cam_buf_map_kernel(struct camera_buf *buf_info, int i)
{
	int ret = 0;

	ret = sprd_dmabuf_map_kernel(buf_info->dmabuf_p[i], &buf_info->map);
	buf_info->addr_k[i] = (unsigned long)buf_info->map.vaddr;

	return ret;
}

int cam_buf_unmap_kernel(struct camera_buf *buf_info, int i)
{
	int ret = 0;

	ret = sprd_dmabuf_unmap_kernel(buf_info->dmabuf_p[i], &buf_info->map);
	return ret;
}

int cam_buffer_alloc(struct camera_buf *buf_info,
	size_t size, unsigned int iommu_enable, unsigned int flag)
{
	int ret = 0;
	const char *heap_name = NULL;
	struct dma_heap *dmaheap = NULL;

	if (buf_info->buf_sec)
		heap_name = "carveout_fd";
	else {
		if (iommu_enable)
			if(flag)
				heap_name = "system";
			else
				heap_name = "system-uncached";
		else
			heap_name = "carveout_mm";
	}

	dmaheap = dma_heap_find(heap_name);
	if (dmaheap == NULL) {
		pr_err("fail to get dmaheap\n");
		ret = -ENOMEM;
		return ret;
	}
	buf_info->dmabuf_p[0] = dma_heap_buffer_alloc(dmaheap, size, O_RDWR | O_CLOEXEC, 0);
	return ret;
}

void cam_buffer_free(struct dma_buf *dmabuf)
{
	return dma_heap_buffer_free(dmabuf);
}

int cam_ion_get_buffer(int fd, bool buf_sec, struct dma_buf *dmabuf,
		void **buf, size_t *size)
{
	int ret = 0;

	if (buf_sec)
		ret = sprd_dmabuf_get_carvebuffer(fd, dmabuf, buf, size);
	else
		ret = sprd_dmabuf_get_sysbuffer(fd, dmabuf, buf, size);

	return ret;
}

int cam_buf_get_phys_addr(int fd, struct dma_buf *dmabuf,
		unsigned long *phys_addr, size_t *size)
{
	int ret = 0;

	ret = sprd_dmabuf_get_phys_addr(fd, dmabuf, phys_addr, size);
	return ret;
}
#else
timeval cam_ktime_to_timeval(const ktime_t kt)
{
	return ktime_to_timeval(kt);
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
timespec cam_timespec_sub(struct timespec lhs,
			struct timespec rhs)
{
	return timespec64_to_timespec(timespec64_sub(
			timespec_to_timespec64(lhs),
			timespec_to_timespec64(rhs)));
}
#else
timespec cam_timespec_sub(timespec lhs,
					timespec rhs)
{
	return timespec_sub(lhs, rhs);
}
#endif

int cam_buf_map_kernel(struct camera_buf *buf_info, int i)
{
	int ret = 0;

	buf_info->addr_k[i] = (unsigned long)
		sprd_ion_map_kernel(buf_info->dmabuf_p[i], 0);
	return ret;
}

int cam_buf_unmap_kernel(struct camera_buf *buf_info, int i)
{
	int ret = 0;

	ret = sprd_ion_unmap_kernel(buf_info->dmabuf_p[i], 0);
	return ret;
}

int cam_buffer_alloc(struct camera_buf *buf_info,
	size_t size, unsigned int iommu_enable, unsigned int flag)
{
	int ret = 0;
	int heap_type = 0;

	if (buf_info->buf_sec)
		heap_type = ION_HEAP_ID_MASK_CAM;
	else
		heap_type = iommu_enable ?
				ION_HEAP_ID_MASK_SYSTEM :
				ION_HEAP_ID_MASK_MM;

	buf_info->dmabuf_p[0] = cam_ion_alloc(size, heap_type, flag);
	return ret;
}

void cam_buffer_free(struct dma_buf *dmabuf)
{
	cam_ion_free(dmabuf);
}

int cam_ion_get_buffer(int fd, bool buf_sec, struct dma_buf *dmabuf,
		void **buf, size_t *size)
{
	int ret = 0;

	ret = sprd_ion_get_buffer(fd, dmabuf, buf, size);
	return ret;
}

int cam_buf_get_phys_addr(int fd, struct dma_buf *dmabuf,
		unsigned long *phys_addr, size_t *size)
{
	int ret = 0;

	ret = sprd_ion_get_phys_addr(fd, dmabuf, phys_addr, size);
	return ret;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
struct dma_buf * cam_ion_alloc(size_t len, unsigned int heap_id_mask,
					unsigned int flags)
{
	return ion_alloc(len, heap_id_mask, flags);
}

void cam_ion_free(struct dma_buf *dmabuf)
{
	dma_buf_put(dmabuf);
}

struct file *cam_filp_open(const char *filename, int flags, umode_t mode)
{
#ifdef CONFIG_SPRD_DCAM_DEBUG_RAW
	return filp_open(filename, flags, mode);
#else
	return NULL;
#endif
}

int cam_filp_close(struct file *filp, fl_owner_t id)
{
#ifdef CONFIG_SPRD_DCAM_DEBUG_RAW
	return filp_close(filp, id);
#else
	return 0;
#endif
}

ssize_t cam_kernel_read(struct file *file, void *buf, size_t count, loff_t *pos)
{
#ifdef CONFIG_SPRD_DCAM_DEBUG_RAW
	return kernel_read(file, buf, count, &file->f_pos);
#else
	return 0;
#endif
}

ssize_t cam_kernel_write(struct file *file, void *buf, size_t count, loff_t *pos)
{
#ifdef CONFIG_SPRD_DCAM_DEBUG_RAW
	return kernel_write(file, buf, count, &file->f_pos);
#else
	return 0;
#endif
}

void cam_kproperty_get(const char *key, char *value,
	const char *default_value)
{
}

int cam_syscon_get_args_by_name(struct device_node *np,
				const char *name,
				int arg_count,
				unsigned int *out_args)
{
	struct regmap *reg_map = NULL;

	reg_map = syscon_regmap_lookup_by_phandle_args(np, name, arg_count, out_args);
	if (IS_ERR_OR_NULL(reg_map)) {
		pr_err("fail to get syscon %s %d, %p\n", name, arg_count, out_args);
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(cam_syscon_get_args_by_name);

struct regmap *cam_syscon_regmap_lookup_by_name(
					struct device_node *np,
					const char *name)
{
	return syscon_regmap_lookup_by_phandle(np, name);
}
EXPORT_SYMBOL(cam_syscon_regmap_lookup_by_name);
#else
extern void sprd_kproperty_get(const char *key, char *value,
	const char *default_value);

struct dma_buf * cam_ion_alloc(size_t len, unsigned int heap_id_mask,
					unsigned int flags)
{
	return ion_new_alloc(len, heap_id_mask, flags);
}

void cam_ion_free(struct dma_buf *dmabuf)
{
	ion_free(dmabuf);
}

struct file *cam_filp_open(const char *filename, int flags, umode_t mode)
{
	return filp_open(filename, flags, mode);
}

int cam_filp_close(struct file *filp, fl_owner_t id)
{
	return filp_close(filp, id);
}

ssize_t cam_kernel_read(struct file *file, void *buf, size_t count, loff_t *pos)
{
	return kernel_read(file, buf, count, &file->f_pos);
}

ssize_t cam_kernel_write(struct file *file, void *buf, size_t count, loff_t *pos)
{
	return kernel_write(file, buf, count, &file->f_pos);
}

void cam_kproperty_get(const char *key, char *value,
	const char *default_value)
{
#if defined(PROJ_SHARKL5PRO) || defined(PROJ_QOGIRL6) || defined(PROJ_QOGIRN6PRO)
	sprd_kproperty_get(key, value, default_value);
#endif
}

int cam_syscon_get_args_by_name(struct device_node *np,
				const char *name,
				int arg_count,
				unsigned int *out_args)
{
	int args_count = 0;

	args_count = syscon_get_args_by_name(np, name, arg_count, out_args);
	if (args_count != arg_count) {
		pr_err("fail to get syscon %s %d, %p\n", name, arg_count, out_args);
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(cam_syscon_get_args_by_name);

struct regmap *cam_syscon_regmap_lookup_by_name(
					struct device_node *np,
					const char *name)
{
	return syscon_regmap_lookup_by_name(np, name);
}
EXPORT_SYMBOL(cam_syscon_regmap_lookup_by_name);
#endif

