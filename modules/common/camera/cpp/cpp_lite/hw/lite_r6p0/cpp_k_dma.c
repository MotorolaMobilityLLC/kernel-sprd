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

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>

#include "cpp_reg.h"
#include "cpp_block.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "DMA_DRV: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int cpp_k_dma_dev_enable(void *arg)
{
	struct dma_drv_private *p = NULL;
	unsigned long flags = 0;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct dma_drv_private *) arg;

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_OWR(CPP_PATH_EB, CPP_DMA_EB_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
	return 0;
}

int cpp_k_dma_dev_disable(void *arg)
{
	struct dma_drv_private *p = NULL;
	unsigned long flags = 0;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct dma_drv_private *) arg;

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_AWR(CPP_PATH_EB, ~CPP_DMA_EB_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
	return 0;
}

int cpp_k_dma_dev_cfg(void *arg)
{
	struct dma_drv_private *p = NULL;
	unsigned long flags = 0;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct dma_drv_private *) arg;

	CPP_TRACE("src addr 0x%x, endian 0x%x, dst addr 0x%x, endian 0x%x, img_size %d\n",
		p->dma_src_addr, p->cfg_parm.src_endian,
		p->dma_dst_addr, p->cfg_parm.dst_endian, p->cfg_parm.total_num);

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_WR(CPP_DMA_SRC_ADDR, p->dma_src_addr);
	CPP_REG_WR(CPP_DMA_DES_ADDR, p->dma_dst_addr);
	CPP_REG_MWR(CPP_DMA_CFG, CPP_DMA_TOTAL_NUM_MASK, p->cfg_parm.total_num);

	CPP_REG_MWR(CPP_AXIM_CHN_SET, CPP_PATH2_IN_ENDIAN, p->cfg_parm.src_endian << 16);
	CPP_REG_MWR(CPP_AXIM_CHN_SET, CPP_PATH2_OUT_ENDIAN, p->cfg_parm.dst_endian << 19);
	CPP_REG_MWR(CPP_AXIM_CHN_SET, CPP_AXIM_CHN_SET_QOS_MASK, (0x1 << 24));
	spin_unlock_irqrestore(p->hw_lock, flags);
	return 0;
}

int cpp_k_dma_dev_start(void *arg)
{
	struct dma_drv_private *p = NULL;
	unsigned long flags = 0;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct dma_drv_private *) arg;

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_OWR(CPP_PATH_START, CPP_DMA_START_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);

	return 0;
}

int cpp_k_dma_dev_stop(void *arg)
{
	struct dma_drv_private *p = NULL;
	unsigned long flags = 0;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct dma_drv_private *) arg;

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_AWR(CPP_PATH_START, (~CPP_DMA_START_BIT));
	spin_unlock_irqrestore(p->hw_lock, flags);

	return 0;
}

int cpp_k_dma_reg_trace(void *arg)
{
#ifdef DMA_DRV_DEBUG
	unsigned long addr = 0;
	struct dma_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid arg\n");
		return -EFAULT;
	}
	p = (struct dma_drv_private *) arg;

	CPP_TRACE("CPP:DMA Register list");
	for (addr = CPP_DMA_SRC_ADDR; addr <= CPP_DMA_PATH_CFG;
		addr += 16) {
		CPP_TRACE("0x%lx: 0x%8x 0x%8x 0x%8x 0x%8x\n", addr,
			CPP_REG_RD(addr), CPP_REG_RD(addr + 4),
			CPP_REG_RD(addr + 8), CPP_REG_RD(addr + 12));
	}
#endif
	return 0;
}