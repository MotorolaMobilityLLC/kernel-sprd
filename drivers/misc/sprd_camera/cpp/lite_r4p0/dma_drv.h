/*
 * Copyright (C) 2017-2018 Spreadtrum Communications Inc.
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

#ifndef _DMA_DRV_H_
#define _DMA_DRV_H_

#include "cpp_core.h"

struct dma_drv_private {
	struct sprd_cpp_dma_cfg_parm cfg_parm;
	spinlock_t *hw_lock;
	void *priv;
	unsigned int dma_src_addr;
	unsigned int dma_dst_addr;

	unsigned int total_num;

	struct cpp_iommu_info iommu_src;
	struct cpp_iommu_info iommu_dst;
	void __iomem *io_base;

	struct platform_device *pdev;
};

int sprd_dma_drv_start(
	struct sprd_cpp_dma_cfg_parm *parm, struct dma_drv_private *p);
void sprd_dma_drv_stop(struct dma_drv_private *p);
#endif
