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

#ifndef _CAM_KERNEL_ADAPT_H_
#define _CAM_KERNEL_ADAPT_H_

#include <linux/version.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <linux/pm_runtime.h>
#include <linux/sprd_ion.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#include <linux/dma-heap.h>
#include <uapi/linux/sprd_dmabuf.h>
#endif
#else
#include <video/sprd_mmsys_pw_domain.h>
#include "ion.h"
/* #include "ion_priv.h" */
#endif

#include "cam_buf.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
typedef struct timespec64 timespec;
#define ktime_get_ts ktime_get_ts64
typedef struct __kernel_old_timeval timeval;
#else
typedef struct timespec timespec;
typedef struct timeval timeval;
#endif

struct dma_buf * cam_ion_alloc(size_t len, unsigned int heap_id_mask,
					unsigned int flags);
void cam_ion_free(struct dma_buf *dmabuf);

struct file *cam_filp_open(const char *, int, umode_t);
int cam_filp_close(struct file *, fl_owner_t id);

ssize_t cam_kernel_read(struct file *, void *, size_t, loff_t *);
ssize_t cam_kernel_write(struct file *, void *, size_t, loff_t *);

void cam_kproperty_get(const char *key, char *value,
	const char *default_value);
int cam_syscon_get_args_by_name(struct device_node *np,
				const char *name,
				int arg_count,
				unsigned int *out_args);
struct regmap *cam_syscon_regmap_lookup_by_name(
					struct device_node *np,
					const char *name);

timespec cam_timespec_sub(timespec lhs, timespec rhs);
timeval cam_ktime_to_timeval(const ktime_t kt);

struct dma_heap *cam_dmaheap_find(const char *name);
struct dma_buf *cam_dma_alloc(struct dma_heap *heap, size_t len,
					unsigned int fd_flags,
					unsigned int heap_flags);
void cam_dma_free(struct dma_buf *dmabuf);

int cam_buf_map_kernel(struct camera_buf *buf_info, int i);
int cam_buf_unmap_kernel(struct camera_buf *buf_info, int i);

int cam_buffer_alloc(struct camera_buf *buf_info,
	size_t size, unsigned int iommu_enable, unsigned int flag);
void cam_buffer_free(struct dma_buf *dmabuf);

int cam_ion_get_buffer(int fd, bool buf_sec, struct dma_buf *dmabuf,
		void **buf, size_t *size);
int cam_buf_get_phys_addr(int fd, struct dma_buf *dmabuf,
		unsigned long *phys_addr, size_t *size);
#endif
