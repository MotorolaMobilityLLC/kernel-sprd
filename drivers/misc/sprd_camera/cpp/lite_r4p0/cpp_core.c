/*
 * Copyright (C) 2017-2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <dt-bindings/soc/sprd,sharkl3-regs.h>
#include <dt-bindings/soc/sprd,sharkl3-mask.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <video/sprd_cpp.h>
#include <video/sprd_img.h>
#include <video/sprd_mm.h>
#include <uapi/video/sprd_vsp_pw_domain.h>

#include "cpp_common.h"
#include "cpp_reg.h"
#include "dma_drv.h"
#include "rot_drv.h"
#include "scale_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CPP_CORE: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define CPP_DEVICE_NAME             "sprd_cpp"
#define ROT_TIMEOUT                 5000
#define SCALE_TIMEOUT               5000
#define DMA_TIMEOUT                 5000
#define CPP_IRQ_LINE_MASK           CPP_PATH_DONE
#define CPP_MMU_IRQ_LINE_MASK       CPP_MMU_ERROR_INT

enum cpp_irq_id {
	CPP_SCALE_DONE = 0,
	CPP_ROT_DONE,
	CPP_DMA_DONE,
	CPP_IRQ_NUMBER
};

struct dmaif_device {
	atomic_t count;
	struct mutex dma_mutex;
	struct completion done_com;
	struct dma_drv_private drv_priv;
};

struct rotif_device {
	atomic_t count;
	struct mutex rot_mutex;
	struct completion done_com;
	struct rot_drv_private drv_priv;
};

struct scif_device {
	atomic_t count;
	struct mutex sc_mutex;
	struct completion done_com;
	struct scale_drv_private drv_priv;
};

typedef void (*cpp_isr_func) (void *);

struct cpp_device {
	atomic_t users;
	atomic_t users_clk;
	spinlock_t hw_lock;
	struct mutex lock;
	spinlock_t slock;

	struct rotif_device *rotif;
	struct scif_device *scif;
	struct dmaif_device *dmaif;
	struct miscdevice md;

	unsigned int irq;
	cpp_isr_func isr_func[CPP_IRQ_NUMBER];
	void *isr_data[CPP_IRQ_NUMBER];

	void __iomem *io_base;

	struct platform_device *pdev;

	struct clk *cpp_clk;
	struct clk *cpp_clk_parent;
	struct clk *cpp_clk_default;
	struct clk *cpp_eb;

	struct clk *cpp_emc_clk;
	struct clk *cpp_emc_clk_parent;
	struct clk *cpp_emc_clk_default;

	struct clk *clk_mm_vsp_eb;

	struct regmap *cam_ahb_gpr;
	struct regmap *vsp_ahb_gpr;
	struct regmap *pmu_apb_gpr;
	struct regmap *aon_apb_gpr;
};

typedef void (*cpp_isr) (struct cpp_device *dev);

static void sprd_cppcore_scale_done(struct cpp_device *dev)
{
	cpp_isr_func user_func = dev->isr_func[CPP_SCALE_DONE];
	void *priv = dev->isr_data[CPP_SCALE_DONE];

	if (user_func)
		(*user_func) (priv);
}

static void sprd_cppcore_rot_done(struct cpp_device *dev)
{
	cpp_isr_func user_func = dev->isr_func[CPP_ROT_DONE];
	void *priv = dev->isr_data[CPP_ROT_DONE];

	if (user_func)
		(*user_func) (priv);
}

static void sprd_cppcore_dma_done(struct cpp_device *dev)
{
	cpp_isr_func user_func = dev->isr_func[CPP_DMA_DONE];
	void *priv = dev->isr_data[CPP_DMA_DONE];

	if (user_func)
		(*user_func) (priv);
}

static const cpp_isr cpp_isr_list[CPP_IRQ_NUMBER] = {
	sprd_cppcore_scale_done,
	sprd_cppcore_rot_done,
	sprd_cppcore_dma_done,
};

static void sprd_cppcore_module_reset(
	struct cpp_device *dev)
{
	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	regmap_update_bits(dev->vsp_ahb_gpr, MM_VSP_AHB_RESET,
			CPP_PATH_RESET_MASK,
			(unsigned int)CPP_PATH_RESET_MASK);
	udelay(2);
	regmap_update_bits(dev->vsp_ahb_gpr, MM_VSP_AHB_RESET,
			CPP_PATH_RESET_MASK,
			~(unsigned int)CPP_PATH_RESET_MASK);
}

static void sprd_cppcore_scale_reset(
	struct cpp_device *dev)
{
	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return;
	}
	regmap_update_bits(dev->vsp_ahb_gpr, (unsigned int)MM_VSP_AHB_RESET,
			(unsigned int)CPP_PATH0_AHB_RESET_BIT,
			(unsigned int)CPP_PATH0_AHB_RESET_BIT);
	udelay(2);
	regmap_update_bits(dev->vsp_ahb_gpr, (unsigned int)MM_VSP_AHB_RESET,
			(unsigned int)CPP_PATH0_AHB_RESET_BIT,
			~(unsigned int)CPP_PATH0_AHB_RESET_BIT);
}

static void sprd_cppcore_rot_reset(struct cpp_device *dev)
{
	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return;
	}
	regmap_update_bits(dev->vsp_ahb_gpr, MM_VSP_AHB_RESET,
			CPP_PATH1_AHB_RESET_BIT,
			(unsigned int)CPP_PATH1_AHB_RESET_BIT);
	udelay(2);
	regmap_update_bits(dev->vsp_ahb_gpr, MM_VSP_AHB_RESET,
			CPP_PATH1_AHB_RESET_BIT,
			~(unsigned int)CPP_PATH1_AHB_RESET_BIT);
}

static void sprd_cppcore_dma_reset(
	struct cpp_device *dev)
{
	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return;
	}
	regmap_update_bits(dev->vsp_ahb_gpr, MM_VSP_AHB_RESET,
			CPP_DMA_AHB_RESET_BIT,
			(unsigned int)CPP_DMA_AHB_RESET_BIT);
	udelay(2);
	regmap_update_bits(dev->vsp_ahb_gpr, MM_VSP_AHB_RESET,
			CPP_DMA_AHB_RESET_BIT,
			~(unsigned int)CPP_DMA_AHB_RESET_BIT);
}

static void sprd_cppcore_iommu_reg_trace(
	struct cpp_device *dev)
{
	unsigned long addr = 0;

	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	pr_info("CPP IOMMU INT ERROR:register list\n");
	for (addr = 0x200; addr <= 0x264 ; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			reg_rd(dev, addr),
			reg_rd(dev, addr + 4),
			reg_rd(dev, addr + 8),
			reg_rd(dev, addr + 12));
	}
}

static int sprd_cppcore_iommu_err_pre_proc(
	struct cpp_device *dev)
{
	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	sprd_cppcore_iommu_reg_trace(dev);

	return 0;
}

static irqreturn_t sprd_cppcore_isr_root(int irq, void *priv)
{
	int i = 0;
	unsigned int path_irq_line = 0;
	unsigned int status = 0;
	unsigned int mmu_irq_line = 0;
	unsigned long flag = 0;
	struct cpp_device *dev = NULL;

	if (!priv) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	dev = (struct cpp_device *)priv;

	status = reg_rd(dev, CPP_INT_STS);

	mmu_irq_line = status & CPP_MMU_IRQ_LINE_MASK;
	if (unlikely(mmu_irq_line != 0)) {
		pr_err("fail to run iommu, int 0x%x\n", mmu_irq_line);
		if (sprd_cppcore_iommu_err_pre_proc(dev))
			return IRQ_HANDLED;
	}
	path_irq_line = status & CPP_IRQ_LINE_MASK;
	if (unlikely(path_irq_line == 0))
		return IRQ_NONE;

	reg_wr(dev, CPP_INT_CLR, status);

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

static void sprd_cppcore_sc_reg_trace(
	struct scale_drv_private *p)
{
#ifdef SCALE_DRV_DEBUG
	unsigned long addr = 0;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	pr_info("CPP:Scaler Register list\n");
	for (addr = CPP_PATH0_SRC_ADDR_Y; addr <= CPP_PATH0_CFG5;
		addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			reg_rd(p, addr), reg_rd(p, addr + 4),
			reg_rd(p, addr + 8), reg_rd(p, addr + 12));
	}
#endif
}

static void sprd_cppcore_rot_reg_trace(struct rot_drv_private *p)
{
#ifdef SCALE_DRV_DEBUG
	unsigned long addr = 0;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	pr_info("CPP:Rotation Register list\n");
	for (addr = CPP_ROTATION_SRC_ADDR; addr <= CPP_ROTATION_PATH_CFG;
		addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			reg_rd(p, addr), reg_rd(p, addr + 4),
			reg_rd(p, addr + 8), reg_rd(p, addr + 12));
	}
#endif
}

static void sprd_cppcore_dma_reg_trace(
	struct dma_drv_private *p)
{
#ifdef SCALE_DRV_DEBUG
	unsigned long addr = 0;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	pr_info("CPP:Dma Register list\n");
	for (addr = CPP_DMA_SRC_ADDR; addr <= CPP_DMA_CFG4; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			reg_rd(p, addr), reg_rd(p, addr + 4),
			reg_rd(p, addr + 8), reg_rd(p, addr + 12));
	}
#endif
}

static int sprd_cppcore_module_enable(struct cpp_device *dev)
{
	int ret = 0;

	if (!dev) {
		pr_info("fail to get valid input ptr\n");
		return -1;
	}

	mutex_lock(&dev->lock);

	{
		ret = clk_prepare_enable(dev->clk_mm_vsp_eb);
		if (ret) {
			pr_err("fail to enable clk mm vsp eb\n");
			goto fail;
		}

		ret = clk_set_parent(dev->cpp_emc_clk,
			dev->cpp_emc_clk_parent);
		if (ret) {
			pr_err("fail to set cpp emc clk\n");
			clk_disable_unprepare(dev->clk_mm_vsp_eb);
			goto fail;
		}

		ret = clk_prepare_enable(dev->cpp_emc_clk);
		if (ret) {
			pr_err("fail to enable cpp emc clk\n");
			clk_disable_unprepare(dev->clk_mm_vsp_eb);
			goto fail;
		}

		ret = clk_prepare_enable(dev->cpp_eb);
		if (ret) {
			pr_err("fail to enable cpp eb\n");
			clk_disable_unprepare(dev->clk_mm_vsp_eb);
			goto fail;
		}

		ret = clk_set_parent(dev->cpp_clk,
			dev->cpp_clk_parent);
		if (ret) {
			pr_err("fail to set cpp clk\n");
			clk_disable_unprepare(dev->cpp_eb);
			clk_disable_unprepare(dev->clk_mm_vsp_eb);
			goto fail;
		}

		ret = clk_prepare_enable(dev->cpp_clk);
		if (ret) {
			pr_err("fail to enable cpp clk\n");
			clk_disable_unprepare(dev->cpp_eb);
			clk_disable_unprepare(dev->clk_mm_vsp_eb);
			goto fail;
		}

		sprd_cppcore_module_reset(dev);

		reg_awr(dev, CPP_MMU_EN, (0xfffffffe));
		reg_owr(dev, MMU_PPN_RANGE1, (0xfff));
		reg_owr(dev, MMU_PPN_RANGE2, (0xfff));
	}
	mutex_unlock(&dev->lock);

	pr_info("sprd_cppcore_module_enable ok\n");

	return ret;

fail:
	mutex_unlock(&dev->lock);

	return ret;
}

static void sprd_cppcore_module_disable(struct cpp_device *dev)
{
	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	mutex_lock(&dev->lock);
	clk_set_parent(dev->cpp_clk, dev->cpp_clk_default);
	clk_disable_unprepare(dev->cpp_clk);
	clk_disable_unprepare(dev->cpp_eb);
	clk_set_parent(dev->cpp_emc_clk, dev->cpp_emc_clk_default);
	clk_disable_unprepare(dev->cpp_emc_clk);
	clk_disable_unprepare(dev->clk_mm_vsp_eb);

	mutex_unlock(&dev->lock);
}

static void sprd_cppcore_register_isr(struct cpp_device *dev,
	enum cpp_irq_id id, cpp_isr_func user_func, void *priv)
{
	unsigned long flag = 0;

	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return;
	}
	if (id < CPP_IRQ_NUMBER) {
		spin_lock_irqsave(&dev->slock, flag);
		dev->isr_func[id] = user_func;
		dev->isr_data[id] = priv;
		if (user_func)
			reg_mwr(dev, CPP_INT_MASK, (1 << id), ~(1 << id));
		else
			reg_mwr(dev, CPP_INT_MASK, (1 << id), (1 << id));
		spin_unlock_irqrestore(&dev->slock, flag);
	} else {
		pr_err("fail to get valid cpp isr irq\n");
	}
}

static void sprd_cppcore_rot_isr(void *priv)
{
	struct rotif_device *rotif = (struct rotif_device *)priv;

	if (!rotif) {
		pr_err("fail to get valid input ptr\n");
		return;
	}
	complete(&rotif->done_com);
}

static void sprd_cppcore_scale_isr(void *priv)
{
	struct scif_device *scif = (struct scif_device *)priv;

	if (!scif) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	complete(&scif->done_com);
}

static void sprd_cppcore_dma_isr(void *priv)
{
	struct dmaif_device *dmaif = (struct dmaif_device *)priv;

	if (!dmaif) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	complete(&dmaif->done_com);
}

int sprd_cpp_core_get_sg_table(
	struct cpp_iommu_info *pfinfo)
{
	int i = 0, ret = 0;

	for (i = 0; i < 2; i++) {
		if (pfinfo->mfd[i] > 0) {
			ret = sprd_ion_get_buffer(pfinfo->mfd[i],
						NULL,
						&pfinfo->buf[i],
						&pfinfo->size[i]);
			if (ret) {
				pr_err("fail to get sg table\n");
				return -EFAULT;
			}
		}
	}
	return 0;
}

int sprd_cpp_core_get_addr(
	struct cpp_iommu_info *pfinfo)
{
	int i = 0, ret = 0;
	struct sprd_iommu_map_data iommu_data;

	memset(&iommu_data, 0x00, sizeof(iommu_data));

	for (i = 0; i < 2; i++) {
		if (pfinfo->size[i] <= 0)
			continue;

		if (sprd_iommu_attach_device(pfinfo->dev) == 0) {
			memset(&iommu_data, 0x00, sizeof(iommu_data));
			iommu_data.buf = pfinfo->buf[i];
			iommu_data.iova_size = pfinfo->size[i];
			iommu_data.ch_type = SPRD_IOMMU_FM_CH_RW;
			iommu_data.sg_offset = pfinfo->offset[i];

			ret = sprd_iommu_map(pfinfo->dev, &iommu_data);
			if (ret) {
				pr_err("fail to get iommu kaddr %d\n", i);
				return -EFAULT;
			}

			pfinfo->iova[i] = iommu_data.iova_addr
					+ pfinfo->offset[i];
		} else {
			ret = sprd_ion_get_phys_addr(pfinfo->mfd[i],
					NULL,
					&pfinfo->iova[i],
					&pfinfo->size[i]);
			if (ret) {
				pr_err("fail to get iommu phy addr %d mfd 0x%x\n",
				       i, pfinfo->mfd[i]);
				return -EFAULT;
			}
			pfinfo->iova[i] += pfinfo->offset[i];
		}
	}

	return 0;
}

int sprd_cpp_core_free_addr(
	struct cpp_iommu_info *pfinfo)
{
	int i, ret;
	struct sprd_iommu_unmap_data iommu_data;

	memset(&iommu_data, 0x00, sizeof(iommu_data));

	for (i = 0; i < 2; i++) {
		if (pfinfo->size[i] <= 0)
			continue;

		if (sprd_iommu_attach_device(pfinfo->dev) == 0) {
			iommu_data.iova_addr = pfinfo->iova[i]
					- pfinfo->offset[i];
			iommu_data.iova_size = pfinfo->size[i];
			iommu_data.ch_type = SPRD_IOMMU_FM_CH_RW;
			iommu_data.buf = NULL;

			ret = sprd_iommu_unmap(pfinfo->dev,
				&iommu_data);
			if (ret) {
				pr_err("failed to free iommu %d\n", i);
				return -EFAULT;
			}
		}
	}

	return 0;
}

static long sprd_cppcore_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned long rtn = 0;
	struct cpp_device *dev = NULL;
	struct rotif_device *rotif = NULL;
	struct scif_device *scif = NULL;
	struct dmaif_device *dmaif = NULL;
	struct sprd_cpp_rot_cfg_parm rot_parm;
	struct sprd_cpp_scale_cfg_parm sc_parm;
	struct sprd_cpp_dma_cfg_parm dma_parm;
	struct sprd_cpp_scale_capability sc_cap_param;
	struct sprd_cpp_size s_sc_cap;
	int cpp_dimension = 0;

	if (!file) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	dev = file->private_data;
	if (!dev) {
		pr_err("fail to get cpp device\n");
		return -EFAULT;
	}

	memset(&rot_parm, 0x00, sizeof(rot_parm));
	memset(&sc_parm, 0x00, sizeof(sc_parm));
	memset(&dma_parm, 0x00, sizeof(dma_parm));
	memset(&sc_cap_param, 0x00, sizeof(sc_cap_param));
	memset(&s_sc_cap, 0x00, sizeof(s_sc_cap));

	switch (cmd) {
	case SPRD_CPP_IO_OPEN_ROT:
		break;
	case SPRD_CPP_IO_START_ROT:
	{
		pr_debug("start rot path\n");
		rotif = dev->rotif;
		if (!rotif) {
			pr_err("fail to get invalid rotif!\n");
			return -EINVAL;
		}

		mutex_lock(&rotif->rot_mutex);

		ret = copy_from_user(&rot_parm,
				(void __user *)arg, sizeof(rot_parm));
		if (ret) {
			pr_err("fail to get rot param form user\n");
			mutex_unlock(&rotif->rot_mutex);
			return -EFAULT;
		}

		ret = sprd_rot_drv_parm_check(&rot_parm);
		if (ret) {
			pr_err("fail to check rot parm\n");
			mutex_unlock(&rotif->rot_mutex);
			return -EINVAL;
		}

		rotif->drv_priv.iommu_src.dev = &dev->pdev->dev;
		rotif->drv_priv.iommu_dst.dev = &dev->pdev->dev;

		ret = sprd_rot_drv_y_parm_set(&rot_parm,
			&rotif->drv_priv);
		if (ret) {
			pr_err("fail to set rot y parm\n");
			mutex_unlock(&rotif->rot_mutex);
			return -EINVAL;
		}
		sprd_rot_drv_start(&rotif->drv_priv);

		if (!sprd_rot_drv_is_end(&rot_parm)) {
			rtn = wait_for_completion_timeout
				(&rotif->done_com,
				msecs_to_jiffies(ROT_TIMEOUT));
			if (rtn == 0) {
				sprd_cppcore_rot_reg_trace(&rotif->drv_priv);
				pr_err("fail to wait for rot path y int\n");
				sprd_rot_drv_stop(&rotif->drv_priv);
				sprd_cppcore_rot_reset(dev);
				mutex_unlock(&rotif->rot_mutex);
				return -EBUSY;
			}

			sprd_rot_drv_uv_parm_set(&rotif->drv_priv);
			sprd_rot_drv_start(&rotif->drv_priv);
		}

		rtn = wait_for_completion_timeout
			(&rotif->done_com,
			msecs_to_jiffies(ROT_TIMEOUT));
		if (rtn == 0) {
			sprd_cppcore_rot_reg_trace(&rotif->drv_priv);
			sprd_rot_drv_stop(&rotif->drv_priv);
			pr_err("fail to wait for rot path uv int\n");
			sprd_cppcore_rot_reset(dev);
			mutex_unlock(&rotif->rot_mutex);
			return -EBUSY;
		}

		sprd_rot_drv_stop(&rotif->drv_priv);
		sprd_cppcore_rot_reset(dev);
		mutex_unlock(&rotif->rot_mutex);
		pr_info("cpp rotation over\n");
		break;
	}
	case SPRD_CPP_IO_OPEN_SCALE:
	{
			sprd_scale_drv_max_size_get(&s_sc_cap.w, &s_sc_cap.h);
			cpp_dimension = (s_sc_cap.h << 16) | s_sc_cap.w;
			if (arg)
				ret = copy_to_user((int *)arg, &cpp_dimension,
				sizeof(cpp_dimension));
			if (ret) {
				pr_err("fail to get max size form user\n");
				return -EFAULT;
			}
			break;
	}
	case SPRD_CPP_IO_SCALE_CAPABILITY:
	{
		if (!arg) {
			pr_err("%s: param is null error.\n", __func__);
			return -EFAULT;
		}

		ret = copy_from_user(&sc_cap_param, (void __user *)arg,
				sizeof(sc_cap_param));
		if (ret != 0) {
			pr_err("fail to copy from user, ret = %d\n", ret);
			return -1;
		}
		ret = sprd_scale_drv_capability_get(&sc_cap_param);
		if (ret != 0)
			sc_cap_param.is_supported = 0;

		ret = copy_to_user((void  __user *)arg,
				&sc_cap_param, sizeof(sc_cap_param));
		if (ret)
			return -EFAULT;

		pr_info("cpp scale_capability done ret = %d\n", ret);
		break;
	}
	case SPRD_CPP_IO_START_SCALE:
	{
		scif = dev->scif;
		if (!scif) {
			pr_err("fail to get valid scif!\n");
			return -EFAULT;
		}

		mutex_lock(&scif->sc_mutex);

		ret = copy_from_user(&sc_parm,
					(void __user *)arg, sizeof(sc_parm));
		if (ret) {
			pr_err("fail to get parm form user\n");
			mutex_unlock(&scif->sc_mutex);
			return -EFAULT;
		}

		scif->drv_priv.iommu_src.dev = &dev->pdev->dev;
		scif->drv_priv.iommu_dst.dev = &dev->pdev->dev;

		ret = sprd_scale_drv_start(&sc_parm, &scif->drv_priv);
		if (ret) {
			pr_err("fail to start scaler\n");
			mutex_unlock(&scif->sc_mutex);
			return -EFAULT;
		}

		rtn = wait_for_completion_timeout(&scif->done_com,
						msecs_to_jiffies
						(SCALE_TIMEOUT));
		if (rtn == 0) {
			sprd_cppcore_sc_reg_trace(&scif->drv_priv);
			sprd_scale_drv_stop(&scif->drv_priv);
			sprd_cppcore_scale_reset(dev);
			mutex_unlock(&scif->sc_mutex);
			pr_err("fail to get scaling done com\n");
			return -EBUSY;
		}

		sprd_scale_drv_stop(&scif->drv_priv);
		sprd_cppcore_scale_reset(dev);
		mutex_unlock(&scif->sc_mutex);
		pr_info("cpp scale over\n");
		break;
	}
	case SPRD_CPP_IO_OPEN_DMA:
		break;
	case SPRD_CPP_IO_START_DMA:
	{
		dmaif = dev->dmaif;
		if (!dmaif) {
			pr_err("fail to get valid dmaif!\n");
			return -EFAULT;
		}

		mutex_lock(&dmaif->dma_mutex);

		ret = copy_from_user(&dma_parm,
				(void __user *)arg, sizeof(dma_parm));
		if (ret) {
			pr_err("fail to get param form user\n");
			mutex_unlock(&dmaif->dma_mutex);
			return -EFAULT;
		}

		dmaif->drv_priv.iommu_src.dev = &dev->pdev->dev;
		dmaif->drv_priv.iommu_dst.dev = &dev->pdev->dev;

		ret = sprd_dma_drv_start(&dma_parm, &dmaif->drv_priv);
		if (ret) {
			pr_err("fail to start dma\n");
			mutex_unlock(&dmaif->dma_mutex);
			return -EFAULT;
		}

		rtn = wait_for_completion_timeout(&dmaif->done_com,
					msecs_to_jiffies(DMA_TIMEOUT));
		if (rtn == 0) {
			sprd_cppcore_dma_reg_trace(&dmaif->drv_priv);
			sprd_dma_drv_stop(&dmaif->drv_priv);
			sprd_cppcore_dma_reset(dev);
			mutex_unlock(&dmaif->dma_mutex);
			pr_err("failed to get dma done com\n");
			return -EBUSY;
		}

		sprd_dma_drv_stop(&dmaif->drv_priv);
		sprd_cppcore_dma_reset(dev);
		mutex_unlock(&dmaif->dma_mutex);
		pr_info("cpp dma over\n");
		break;
	}
	case SPRD_CPP_IO_STOP_SCALE:
		break;
	default:
		pr_err("fail to get valid cpp cmd %d\n",
			_IOC_NR(cmd));
		break;
	}

	return ret;
}

static int sprd_cppcore_open(struct inode *node, struct file *file)
{
	int ret = 0;
	struct cpp_device *dev = NULL;
	struct miscdevice *md = NULL;
	struct rotif_device *rotif = NULL;
	struct scif_device *scif = NULL;
	struct dmaif_device *dmaif = NULL;

	pr_info("start open cpp\n");

	if (!node || !file) {
		pr_info("fail to get valid input ptr\n");
		return -EINVAL;
	}
	pr_info("start get miscdevice\n");
	md = (struct miscdevice *)file->private_data;
	if (!md) {
		pr_info("fail to get md device\n");
		return -EFAULT;
	}
	dev = md->this_device->platform_data;

	file->private_data = (void *)dev;
	if (atomic_inc_return(&dev->users) != 1) {
		pr_info("cpp device node has been opened %d\n",
			atomic_read(&dev->users));
		return 0;
	}
	ret = vsp_pw_on(VSP_PW_DOMAIN_VSP_CPP);
	if (ret) {
		ret = -EFAULT;
		pr_err("fail to vsp power on\n");
		goto exit;
	}
	ret = sprd_cppcore_module_enable(dev);
	if (ret) {
		ret = -EFAULT;
		pr_err("fail to enable cpp module\n");
		goto en_cpp_exit;
	}

	rotif = vzalloc(sizeof(*rotif));
	if (unlikely(!rotif)) {
		ret = -EFAULT;
		pr_err("fail to vzalloc rotif\n");
		goto vzalloc_exit;
	}
	memset(rotif, 0, sizeof(*rotif));
	rotif->drv_priv.io_base = dev->io_base;
	rotif->drv_priv.priv = (void *)rotif;
	dev->rotif = rotif;
	rotif->drv_priv.hw_lock = &dev->hw_lock;
	sprd_cppcore_register_isr(dev, CPP_ROT_DONE,
		sprd_cppcore_rot_isr, (void *)rotif);
	init_completion(&rotif->done_com);
	mutex_init(&rotif->rot_mutex);

	scif = vzalloc(sizeof(*scif));
	if (unlikely(!scif)) {
		ret = -EFAULT;
		pr_err("fail to vzalloc scif\n");
		goto vzalloc_exit;
	}
	memset(scif, 0, sizeof(*scif));
	scif->drv_priv.io_base = dev->io_base;
	scif->drv_priv.pdev = dev->pdev;
	scif->drv_priv.hw_lock = &dev->hw_lock;
	scif->drv_priv.priv = (void *)scif;
	dev->scif = scif;
	init_completion(&scif->done_com);
	mutex_init(&scif->sc_mutex);
	sprd_cppcore_register_isr(dev, CPP_SCALE_DONE,
		sprd_cppcore_scale_isr, (void *)scif);

	dmaif = vzalloc(sizeof(*dmaif));
	if (unlikely(!dmaif)) {
		ret = -EFAULT;
		pr_err("fail to vzalloc scif\n");
		goto vzalloc_exit;
	}
	memset(dmaif, 0, sizeof(*dmaif));
	dmaif->drv_priv.io_base = dev->io_base;
	dmaif->drv_priv.pdev = dev->pdev;
	dmaif->drv_priv.hw_lock = &dev->hw_lock;
	dmaif->drv_priv.priv = (void *)dmaif;
	dev->dmaif = dmaif;
	init_completion(&dmaif->done_com);
	mutex_init(&dmaif->dma_mutex);
	spin_lock_init(&dev->hw_lock);
	sprd_cppcore_register_isr(dev, CPP_DMA_DONE,
		sprd_cppcore_dma_isr, (void *)dmaif);

	ret = devm_request_irq(&dev->pdev->dev, dev->irq,
		sprd_cppcore_isr_root,
		IRQF_SHARED, "CPP", (void *)dev);
	if (ret < 0) {
		pr_info("fail to install IRQ %d\n", ret);
		goto exit;
	}

	pr_info("open sprd_cpp success\n");

	return ret;

vzalloc_exit:
	if (scif) {
		vfree(scif);
		dev->scif = NULL;
	}
	if (rotif) {
		vfree(rotif);
		dev->rotif = NULL;
	}
	if (dmaif) {
		vfree(dmaif);
		dev->dmaif = NULL;
	}
	sprd_cppcore_module_disable(dev);
en_cpp_exit:
	ret = vsp_pw_off(VSP_PW_DOMAIN_VSP_CPP);
	if (ret) {
		pr_info("fail to camera power off\n");
		return ret;
	}
exit:
	if (atomic_dec_return(&dev->users) != 0)
		pr_info("others is using cpp device\n");
	file->private_data = NULL;

	return ret;
}

static int sprd_cppcore_release(struct inode *node,
	struct file *file)
{
	int ret = 0;
	struct cpp_device *dev = NULL;

	if (!node || !file) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	dev = file->private_data;
	if (dev == NULL) {
		pr_err("fail to close cpp device\n");
		return -EFAULT;
	}

	if (atomic_dec_return(&dev->users) != 0) {
		pr_err("others is using cpp device\n");
		return ret;
	}
	if (dev->rotif) {
		vfree(dev->rotif);
		dev->rotif = NULL;
	}
	if (dev->scif) {
		vfree(dev->scif);
		dev->scif = NULL;
	}

	if (dev->dmaif) {
		vfree(dev->dmaif);
		dev->dmaif = NULL;
	}

	devm_free_irq(&dev->pdev->dev, dev->irq, (void *)dev);
	sprd_cppcore_module_disable(dev);
	ret = vsp_pw_off(VSP_PW_DOMAIN_VSP_CPP);
	if (ret) {
		pr_err("fail to camera power off\n");
		return ret;
	}
	file->private_data = NULL;
	pr_info("cpp release success\n");

	return ret;
}

static const struct file_operations cpp_fops = {
	.open = sprd_cppcore_open,
	.unlocked_ioctl = sprd_cppcore_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = sprd_cppcore_ioctl,
#endif
	.release = sprd_cppcore_release,
};

static int sprd_cppcore_probe(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int irq = 0;
	struct cpp_device *dev = NULL;
	void __iomem *reg_base = NULL;
	struct regmap *cam_ahb_gpr = NULL;
	struct regmap *vsp_ahb_gpr = NULL;
	struct regmap *pmu_apb_gpr = NULL;
	struct regmap *aon_apb_gpr = NULL;

	if (!pdev) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		goto fail;

	dev->md.minor = MISC_DYNAMIC_MINOR;
	dev->md.name = CPP_DEVICE_NAME;
	dev->md.fops = &cpp_fops;
	dev->md.parent = NULL;
	ret = misc_register(&dev->md);
	if (ret) {
		pr_err("fail to register misc devices\n");
		return ret;
	}
	dev->pdev = pdev;
	dev->md.this_device->of_node = pdev->dev.of_node;
	dev->md.this_device->platform_data = (void *)dev;

	mutex_init(&dev->lock);
	spin_lock_init(&dev->slock);
	atomic_set(&dev->users, 0);

	pr_info("sprd cpp probe pdev name %s\n", pdev->name);

	dev->clk_mm_vsp_eb = devm_clk_get(&pdev->dev, "clk_mm_vsp_eb");
	if (IS_ERR_OR_NULL(dev->clk_mm_vsp_eb)) {
		ret =  PTR_ERR(dev->clk_mm_vsp_eb);
		goto misc_fail;
	}

	dev->cpp_eb = devm_clk_get(&pdev->dev, "cpp_eb");
	if (IS_ERR_OR_NULL(dev->cpp_eb)) {
		ret =  PTR_ERR(dev->cpp_eb);
		goto misc_fail;
	}

	dev->cpp_clk = devm_clk_get(&pdev->dev, "cpp_clk");
	if (IS_ERR_OR_NULL(dev->cpp_clk)) {
		ret = PTR_ERR(dev->cpp_clk);
		goto misc_fail;
	}

	dev->cpp_clk_parent = devm_clk_get(&pdev->dev, "cpp_clk_parent");
	if (IS_ERR_OR_NULL(dev->cpp_clk_parent)) {
		ret = PTR_ERR(dev->cpp_clk_parent);
		goto misc_fail;
	}

	dev->cpp_clk_default = clk_get_parent(dev->cpp_clk);
	if (IS_ERR_OR_NULL(dev->cpp_clk_default)) {
		ret = PTR_ERR(dev->cpp_clk_default);
		goto misc_fail;
	}

	dev->cpp_emc_clk = devm_clk_get(&pdev->dev, "clk_mm_vsp_emc");
	if (IS_ERR_OR_NULL(dev->cpp_emc_clk)) {
		ret = PTR_ERR(dev->cpp_emc_clk);
		goto misc_fail;
	}

	dev->cpp_emc_clk_parent = devm_clk_get(&pdev->dev,
		"clk_mm_vsp_emc_parent");
	if (IS_ERR_OR_NULL(dev->cpp_emc_clk_parent)) {
		ret = PTR_ERR(dev->cpp_emc_clk_parent);
		goto misc_fail;
	}

	dev->cpp_emc_clk_default = clk_get_parent(dev->cpp_emc_clk);
	if (IS_ERR_OR_NULL(dev->cpp_emc_clk_default)) {
		ret = PTR_ERR(dev->cpp_emc_clk_default);
		goto misc_fail;
	}

	cam_ahb_gpr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
			"sprd,cam-ahb-syscon");
	if (IS_ERR_OR_NULL(cam_ahb_gpr)) {
		ret = PTR_ERR(cam_ahb_gpr);
		pr_err("fail to get cam_ahb_gpr %d\n", ret);
		goto misc_fail;
	}
	dev->cam_ahb_gpr = cam_ahb_gpr;

	vsp_ahb_gpr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
			"sprd,vsp-ahb-syscon");
	if (IS_ERR_OR_NULL(vsp_ahb_gpr)) {
		pr_err("fail to get vsp_ahb_gpr\n");
		ret = PTR_ERR(vsp_ahb_gpr);
		goto misc_fail;
	}
	dev->vsp_ahb_gpr = vsp_ahb_gpr;

	pmu_apb_gpr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
			"sprd,pmu-apb-syscon");
	if (IS_ERR_OR_NULL(pmu_apb_gpr)) {
		pr_err("fail to get pmu_ahb_gpr\n");
		ret = PTR_ERR(pmu_apb_gpr);
		goto misc_fail;
	}
	dev->pmu_apb_gpr = pmu_apb_gpr;

	aon_apb_gpr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
			"sprd,aon-apb-syscon");
	if (IS_ERR_OR_NULL(aon_apb_gpr)) {
		pr_err("fail to get aon_apb_gpr\n");
		ret = PTR_ERR(aon_apb_gpr);
		goto misc_fail;
	}
	dev->aon_apb_gpr = aon_apb_gpr;

	reg_base = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR_OR_NULL(reg_base)) {
		pr_err("fail to get dcam axim_base\n");
		ret = PTR_ERR(reg_base);
		goto misc_fail;
	}
	dev->io_base = reg_base;

	irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (irq <= 0) {
		pr_err("fail to get dcam irq %d\n", irq);
		goto misc_fail;
	}
	dev->irq = irq;

	pr_info("cpp probe OK\n");

	return 0;

misc_fail:
	pr_info("cpp probe fail\n");
	misc_deregister(&dev->md);
fail:
	return ret;
}

static int sprd_cppcore_remove(struct platform_device *pdev)
{
	struct cpp_device *dev = platform_get_drvdata(pdev);

	misc_deregister(&dev->md);
	return 0;
}

static const struct of_device_id of_match_table[] = {
	{ .compatible = "sprd,cpp", },
	{},
};

static struct platform_driver sprd_cpp_driver = {
	.probe = sprd_cppcore_probe,
	.remove = sprd_cppcore_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = CPP_DEVICE_NAME,
		.of_match_table = of_match_ptr(of_match_table),
	},
};

module_platform_driver(sprd_cpp_driver);

MODULE_DESCRIPTION("R4P0 Lite CPP Driver");
MODULE_AUTHOR("Multimedia_Camera@Spreadtrum");
MODULE_LICENSE("GPL");
