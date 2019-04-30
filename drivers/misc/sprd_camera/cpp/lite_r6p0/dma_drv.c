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

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <uapi/video/sprd_cpp.h>

#include "cpp_common.h"
#include "cpp_reg.h"
#include "dma_drv.h"

#define DMA_ADDR_ALIGN 0x07

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "DMA_DRV: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static void sprd_dmadrv_dev_enable(struct dma_drv_private *p)
{
	unsigned long flags = 0;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_OWR(CPP_PATH_EB, CPP_DMA_EB_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
}

static void sprd_dmadrv_dev_disable(struct dma_drv_private *p)
{
	unsigned long flags = 0;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_AWR(CPP_PATH_EB, ~CPP_DMA_EB_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
}

static void sprd_dmadrv_dev_start(struct dma_drv_private *p)
{
	unsigned long flags = 0;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_OWR(CPP_PATH_START, CPP_DMA_START_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
}

static void sprd_dmadrv_dev_stop(struct dma_drv_private *p)
{
	unsigned long flags = 0;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_AWR(CPP_PATH_START, ~CPP_DMA_START_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
}

static int sprd_dmadrv_parm_check(struct sprd_cpp_dma_cfg_parm *parm)
{
	if (!parm) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
#ifdef DMA_DRV_DEBUG
	pr_info("DMA:\n");
	pr_info("dma total num %d\n", parm->total_num);
	pr_info("src mfd %u y:u:v 0x%x 0x%x 0x%x\n",
		parm->input_addr.mfd[0],
		parm->input_addr.y, parm->input_addr.u, parm->input_addr.v);
	pr_info("dst mfd %u y:u:v 0x%x 0x%x 0x%x\n",
		parm->output_addr.mfd[0],
		parm->output_addr.y, parm->output_addr.u, parm->output_addr.v);
#endif
	if ((parm->input_addr.y & DMA_ADDR_ALIGN) ||
		(parm->input_addr.u & DMA_ADDR_ALIGN) ||
		(parm->input_addr.v & DMA_ADDR_ALIGN) ||
		(parm->output_addr.y & DMA_ADDR_ALIGN) ||
		(parm->output_addr.u & DMA_ADDR_ALIGN) ||
		(parm->output_addr.v & DMA_ADDR_ALIGN)) {
		pr_err("fail to get valid align addr\n");
		return -EINVAL;
	}

	if (parm->total_num % 8 != 0) {
		pr_err("fail to get align total num:%d",
			parm->total_num);
		return -EINVAL;
	}

	return 0;
}

static int sprd_dmadrv_parm_cfg(
	struct sprd_cpp_dma_cfg_parm *parm, struct dma_drv_private *p)
{
	int ret = 0;

	if (!parm || !p) {
		pr_err("fail to get valid input parm %p p %p\n", parm, p);
		return -EINVAL;
	}

	memcpy((void *)&p->cfg_parm, (void *)parm,
		sizeof(struct sprd_cpp_dma_cfg_parm));

	memcpy(p->iommu_src.mfd, parm->input_addr.mfd,
		sizeof(parm->input_addr.mfd));
	memcpy(p->iommu_dst.mfd, parm->output_addr.mfd,
		sizeof(parm->output_addr.mfd));

	ret = sprd_cpp_core_get_sg_table(&p->iommu_src);
	if (ret) {
		pr_err("fail to get cpp sg table\n");
		return -1;
	}
	p->iommu_src.offset[0] = p->cfg_parm.input_addr.y;
	p->iommu_src.offset[1] = p->cfg_parm.input_addr.u;
	ret = sprd_cpp_core_get_addr(&p->iommu_src);
	if (ret) {
		pr_err("fail to get src addr\n");
		return -EFAULT;
	}
	p->dma_src_addr = p->iommu_src.iova[0];

	ret = sprd_cpp_core_get_sg_table(&p->iommu_dst);
	if (ret) {
		sprd_cpp_core_free_addr(&p->iommu_src);
		pr_err("fail to get cpp sg table\n");
		return -1;
	}
	p->iommu_dst.offset[0] = p->cfg_parm.output_addr.y;
	p->iommu_dst.offset[1] = p->cfg_parm.output_addr.u;
	ret = sprd_cpp_core_get_addr(&p->iommu_dst);
	if (ret) {
		sprd_cpp_core_free_addr(&p->iommu_src);
		pr_err("fail to get src addr\n");
		return -EFAULT;
	}
	p->dma_dst_addr = p->iommu_dst.iova[0];

	p->total_num = parm->total_num;

	pr_debug("iommu_dma_src:0x%x\n", p->dma_dst_addr);
	pr_debug("iommu_dma_dst:0x%x\n", p->dma_src_addr);

	return 0;
}

static int sprd_dmadrv_parm_set(struct dma_drv_private *p)
{
	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	if ((p->dma_src_addr & DMA_ADDR_ALIGN) ||
		(p->dma_dst_addr & DMA_ADDR_ALIGN)) {
		pr_err("fail to get align src or dst addr\n");
		return -EINVAL;
	}

	CPP_REG_WR(CPP_DMA_SRC_ADDR, p->dma_src_addr);
	CPP_REG_WR(CPP_DMA_DES_ADDR, p->dma_dst_addr);
	CPP_REG_WR(CPP_DMA_CFG, p->total_num & CPP_DMA_TOTAL_NUM_MASK);

	return 0;
}

int sprd_dma_drv_start(struct sprd_cpp_dma_cfg_parm *parm,
		   struct dma_drv_private *p)
{
	int ret = 0;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	sprd_dmadrv_dev_stop(p);
	sprd_dmadrv_dev_enable(p);

	ret = sprd_dmadrv_parm_check(parm);
	if (ret) {
		pr_err("fail to check dma parm\n");
		return -EINVAL;
	}

	ret = sprd_dmadrv_parm_cfg(parm, p);
	if (ret) {
		pr_err("fail to cfg dma parm\n");
		return -EINVAL;
	}

	ret = sprd_dmadrv_parm_set(p);
	if (ret) {
		pr_err("fail to set dma parm\n");
		return -EINVAL;
	}

	sprd_dmadrv_dev_start(p);

	return ret;
}

void sprd_dma_drv_stop(struct dma_drv_private *p)
{
	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	sprd_dmadrv_dev_stop(p);
	udelay(1);
	sprd_dmadrv_dev_disable(p);
	sprd_cpp_core_free_addr(&p->iommu_src);
	sprd_cpp_core_free_addr(&p->iommu_dst);
}
