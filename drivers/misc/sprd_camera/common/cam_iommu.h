/*
 * Copyright (C) 2015-2016 Spreadtrum Communications Inc.
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
#ifndef _CAM_IOMMU_H_
#define _CAM_IOMMU_H_

#include <linux/types.h>
#include <linux/sprd_iommu.h>

struct pfiommu_info {
	struct device *dev;
	unsigned int mfd[3];
	struct sg_table *table[3];
	void *buf[3];
	size_t size[3];
	unsigned long iova[3];
	struct dma_buf *dmabuf_p[3];
	unsigned int offset[3];
};

int pfiommu_get_sg_table(struct pfiommu_info *pfinfo);
int  pfiommu_put_sg_table(void);
void dma_buffer_list_clear(void);
int pfiommu_get_addr(struct pfiommu_info *pfinfo);
int pfiommu_free_addr(struct pfiommu_info *pfinfo);
int pfiommu_free_addr_with_id(struct pfiommu_info *pfinfo,
	enum sprd_iommu_chtype ctype, unsigned int cid);

#endif /* _CAM_IOMMU_H_ */
