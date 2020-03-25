/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_ION_H
#define _SPRD_ION_H

#include <uapi/linux/ion.h>
#include <uapi/linux/sprd_ion.h>
#include <linux/dma-buf.h>
#include <linux/scatterlist.h>

struct ion_platform_heap {
	enum ion_heap_type type;
	unsigned int id;
	const char *name;
	phys_addr_t base;
	size_t size;
	phys_addr_t align;
	void *priv;
};

int sprd_ion_get_buffer(int fd, struct dma_buf *dmabuf,
			void **buf, size_t *size);
int sprd_ion_get_phys_addr(int fd, struct dma_buf *dmabuf,
			   unsigned long *phys_addr, size_t *size);
void *sprd_ion_map_kernel(struct dma_buf *dmabuf, unsigned long offset);
int sprd_ion_unmap_kernel(struct dma_buf *dmabuf, unsigned long offset);
struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap *heap_data);
#endif /* _SPRD_ION_H */
