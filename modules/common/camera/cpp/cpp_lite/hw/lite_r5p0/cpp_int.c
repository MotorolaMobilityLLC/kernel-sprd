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
#include <linux/err.h>
#include <linux/interrupt.h>

#include "cpp_reg.h"
#include "cpp_drv.h"
#include "cpp_int.h"

/* Macro Definitions */
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CPP_INT: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define CPP_IRQ_LINE_MASK           CPP_PATH_DONE
#define CPP_MMU_IRQ_LINE_MASK       CPP_MMU_ERROR_INT
typedef void (*cpp_isr) (struct cpp_pipe_dev *dev);
typedef void (*cpp_isr_func) (void *);
static cpp_isr_func isr_func[CPP_IRQ_NUMBER];
static void *isr_data[CPP_IRQ_NUMBER];

static void cppint_scale_done(struct cpp_pipe_dev *dev)
{
	cpp_isr_func user_func = isr_func[CPP_SCALE_DONE];
	void *priv = isr_data[CPP_SCALE_DONE];

	if (user_func)
		(*user_func) (priv);
}

static void cppint_rot_done(struct cpp_pipe_dev *dev)
{
	cpp_isr_func user_func = isr_func[CPP_ROT_DONE];
	void *priv = isr_data[CPP_ROT_DONE];

	if (user_func)
		(*user_func) (priv);
}

static const cpp_isr cpp_isr_list[CPP_IRQ_NUMBER] = {
	cppint_scale_done,
	cppint_rot_done,
};

static void cppint_iommu_reg_trace(struct cpp_pipe_dev *dev)
{
	unsigned long addr = 0;

	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	pr_err("fail to CPP IOMMU INT ERROR:register list\n");
	for (addr = 0x200; addr <= 0x264 ; addr += 16) {
		pr_err("0x%lx: 0x%x 0x%x 0x%x 0x%x\n", addr,
			CPP_REG_RD(addr), CPP_REG_RD(addr + 4),
			CPP_REG_RD(addr + 8), CPP_REG_RD(addr + 12));
	}
}

static int cppint_iommu_err_pre_proc(struct cpp_pipe_dev *dev)
{
	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	cppint_iommu_reg_trace(dev);

	return 0;
}

static irqreturn_t cppint_isr_root(int irq, void *priv)
{
	int i = 0;
	unsigned int path_irq_line = 0;
	unsigned int status = 0;
	unsigned int mmu_irq_line = 0;
	unsigned long flag = 0;
	struct cpp_pipe_dev *dev = NULL;

	if (!priv) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	dev = (struct cpp_pipe_dev *)priv;
	status = CPP_REG_RD(CPP_INT_STS);
	mmu_irq_line = status & CPP_MMU_IRQ_LINE_MASK;
	CPP_REG_WR(CPP_INT_CLR, status);
	if (unlikely(mmu_irq_line != 0)) {
		pr_err("fail to run iommu, int 0x%x\n", mmu_irq_line);
		if (cppint_iommu_err_pre_proc(dev))
			return IRQ_HANDLED;
	}
	path_irq_line = status & CPP_IRQ_LINE_MASK;
	if (unlikely(path_irq_line == 0))
		return IRQ_NONE;

	spin_lock_irqsave(&dev->slock, flag);
	for (i = 0; i < CPP_IRQ_NUMBER; i++) {
		if (path_irq_line & (1 << (unsigned int)i)) {
			if (cpp_isr_list[i])
				cpp_isr_list[i](dev);
		}
		path_irq_line &= ~(unsigned int)(1 << (unsigned int)i);
		if (!path_irq_line)
			break;
	}
	spin_unlock_irqrestore(&dev->slock, flag);

	return IRQ_HANDLED;
}

static void cppint_register_isr(struct cpp_pipe_dev *dev,
	enum cpp_irq_id id, cpp_isr_func user_func, void *priv)
{
	unsigned long flag = 0;

	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	if (id < CPP_IRQ_NUMBER) {
		spin_lock_irqsave(&dev->slock, flag);
		isr_func[id] = user_func;
		isr_data[id] = priv;
		if (user_func)
			CPP_REG_MWR(CPP_INT_MASK, (1 << id), ~(1 << id));
		else
			CPP_REG_MWR(CPP_INT_MASK, (1 << id), (1 << id));
		spin_unlock_irqrestore(&dev->slock, flag);
	} else {
		pr_err("fail to get valid cpp isr irq\n");
	}
}

static void cppint_rot_isr(void *priv)
{
	struct rotif_device *rotif = (struct rotif_device *)priv;

	if (!rotif) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	complete(&rotif->done_com);
}

static void cppint_scale_isr(void *priv)
{
	struct scif_device *scif = (struct scif_device *)priv;

	if (!scif) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	complete(&scif->done_com);
}

int cpp_int_irq_request(void *cpp_handle)
{
	int ret = 0;
	struct rotif_device *rotif = NULL;
	struct scif_device *scif = NULL;
	struct cpp_pipe_dev *dev;

	if (!cpp_handle) {
		pr_err("fail to get valid input ptr cpp_handle %p\n",
			cpp_handle);
		return -EFAULT;
	}

	dev = (struct cpp_pipe_dev *)cpp_handle;
	rotif = vzalloc(sizeof(*rotif));
	if (unlikely(!rotif)) {
		ret = -EFAULT;
		pr_err("fail to vzalloc rotif\n");
		goto fail;
	}
	rotif->drv_priv.io_base = dev->io_base;
	rotif->drv_priv.priv = (void *)rotif;
	dev->rotif = rotif;
	rotif->drv_priv.hw_lock = &dev->hw_lock;
	spin_lock_init(&dev->hw_lock);
	cppint_register_isr(dev, CPP_ROT_DONE,
		cppint_rot_isr, (void *)rotif);
	init_completion(&rotif->done_com);
	mutex_init(&rotif->rot_mutex);
	scif = vzalloc(sizeof(*scif));
	if (unlikely(!scif)) {
		ret = -EFAULT;
		pr_err("fail to vzalloc scif\n");
		goto fail;
	}
	scif->drv_priv.io_base = dev->io_base;
	scif->drv_priv.pdev = dev->pdev;
	scif->drv_priv.hw_lock = &dev->hw_lock;
	scif->drv_priv.priv = (void *)scif;
	dev->scif = scif;
	init_completion(&scif->done_com);
	mutex_init(&scif->sc_mutex);
	cppint_register_isr(dev, CPP_SCALE_DONE,
		cppint_scale_isr, (void *)scif);

	ret = devm_request_irq(&dev->pdev->dev, dev->irq,
		cppint_isr_root, IRQF_SHARED, "CPP", (void *)dev);
	if (ret < 0) {
		pr_err("fail to install IRQ %d\n", ret);
		goto fail;
	}
	return ret;
fail:
	if (scif) {
		vfree(scif);
		dev->scif = NULL;
	}
	if (rotif) {
		vfree(rotif);
		dev->rotif = NULL;
	}
	return ret;
}

int cpp_int_irq_free(void *cpp_handle)
{
	int ret = 0;
	struct cpp_pipe_dev *dev;

	if (!cpp_handle) {
		pr_err("fail to get valid input ptr cpp_handle %p\n",
			cpp_handle);
		return -EFAULT;
	}

	dev = (struct cpp_pipe_dev *)cpp_handle;
	devm_free_irq(&dev->pdev->dev, dev->irq, (void *)dev);
	return ret;
}
