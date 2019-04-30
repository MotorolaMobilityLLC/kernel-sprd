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

#ifndef _CPP_CORE_H_
#define _CPP_CORE_H_

#include <linux/dma-buf.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/scatterlist.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>
#include <linux/types.h>
#include <video/sprd_mm.h>

struct cpp_iommu_info {
	struct device *dev;
	unsigned int mfd[3];
	struct sg_table *table[3];
	void *buf[3];
	size_t size[3];
	unsigned long iova[3];
	struct dma_buf *dmabuf_p[3];
	unsigned int offset[3];
};

struct cpp_addr {
	void __iomem *io_base;
	long reserved;
};

int sprd_cpp_core_get_sg_table(struct cpp_iommu_info *pfinfo);
int sprd_cpp_core_get_addr(struct cpp_iommu_info *pfinfo);
int sprd_cpp_core_free_addr(struct cpp_iommu_info *pfinfo);
#endif
