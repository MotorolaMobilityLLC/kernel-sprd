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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/slab.h>
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
#include <sprd_mm.h>
#include "cam_types.h"
#include "sprd_cpp.h"
#include "sprd_img.h"

#include "cpp_int.h"
#include "cpp_drv.h"
#include "cpp_reg.h"
#include "cpp_hw.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CPP_CORE: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define CPP_DEVICE_NAME             "sprd_cpp"
#define ROT_TIMEOUT                 5000
#define SCALE_TIMEOUT               5000
#define DMA_TIMEOUT                 5000

struct cpp_device {
	atomic_t users;
	struct platform_device *pdev;
	void __iomem *io_base;
	struct mutex lock;
	struct miscdevice md;
	struct cpp_pipe_dev *cppif;
	struct cpp_hw_info *hw_info;
	struct wakeup_source *ws;
};

static int cppcore_module_enable(struct cpp_device *dev)
{
	int ret = 0;
	struct cpp_hw_soc_info *soc_cpp = NULL;
	struct cpp_hw_info *hw = NULL;

	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}
	hw = dev->hw_info;
	if (!hw) {
		pr_err("fail to get valid cpp hw info\n");
		return -1;
	}
	soc_cpp = hw->soc_cpp;
	if (!soc_cpp) {
		pr_err("fail to get valid soc_cpp info\n");
		return -1;
	}
	mutex_lock(&dev->lock);
	ret = dev->cppif->cppdrv_ops->ioctl(CPP_DRV_CLK_EB, hw, soc_cpp);
	if (ret)
	goto fail;
	ret = dev->cppif->cppdrv_ops->ioctl(CPP_DRV_MODULE_RESET, hw, soc_cpp);
	if (ret)
	goto fail;
	ret = dev->cppif->cppdrv_ops->ioctl(CPP_DRV_QOS_SET, hw, soc_cpp);
	if (ret)
	goto fail;
	ret = dev->cppif->cppdrv_ops->ioctl(CPP_DRV_MMU_SET, hw, soc_cpp);
	if (ret)
	goto fail;

fail:
	mutex_unlock(&dev->lock);
	return ret;
}

static void cppcore_module_disable(struct cpp_device *dev)
{
	struct cpp_hw_soc_info *soc_cpp = NULL;
	int ret = 0;

	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return;
	}
	soc_cpp = dev->hw_info->soc_cpp;
	mutex_lock(&dev->lock);
	ret = dev->cppif->cppdrv_ops->ioctl(CPP_DRV_CLK_DIS, dev->hw_info, soc_cpp);
	if (ret)
	goto fail;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	sprd_cam_domain_disable();
	sprd_cam_pw_off();
#else
	pm_runtime_put_sync(&dev->pdev->dev);
	__pm_relax(dev->ws);
#endif
fail:
	mutex_unlock(&dev->lock);
}

#define CPP_IOCTL
#include "cpp_ioctl.c"
#undef CPP_IOCTL

static long cppcore_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct cpp_device *dev = NULL;
	cpp_io_func io_ctrl;

	if (!file) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	dev = file->private_data;
	if (!dev) {
		pr_err("fail to get cpp device\n");
		return -EFAULT;
	}
	io_ctrl = cppcore_ioctl_get_fun(cmd);
	if (io_ctrl != NULL) {
		ret = io_ctrl(dev, arg);
		if (ret) {
			pr_err("fail to cmd %d\n", _IOC_NR(cmd));
			goto exit;
		}
	} else {
		pr_debug("fail to get valid cmd 0x%x 0x%x %s\n", cmd,
			cppcore_ioctl_get_val(cmd),
			cppcore_ioctl_get_str(cmd));
	}

exit:
	return ret;
}

static int cppcore_open(struct inode *node, struct file *file)
{
	int ret = 0;
	struct cpp_device *dev = NULL;
	struct cpp_pipe_dev *cppif = NULL;
	struct cpp_hw_info *hw = NULL;
	struct miscdevice *md = NULL;

	CPP_TRACE("start open cpp\n");

	if (!node || !file) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	CPP_TRACE("start get miscdevice\n");
	md = (struct miscdevice *)file->private_data;
	if (!md) {
		pr_err("fail to get md device\n");
		return -EFAULT;
	}
	dev = md->this_device->platform_data;

	file->private_data = (void *)dev;
	if (atomic_inc_return(&dev->users) != 1) {
		CPP_TRACE("cpp device node has been opened %d\n",
			atomic_read(&dev->users));
		return 0;
	}
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	ret = sprd_cam_pw_on();
	if (ret) {
		pr_err("%s fail to power on cpp\n", __func__);
		goto fail;
	}
	sprd_cam_domain_eb();
#else
	ret = pm_runtime_get_sync(&dev->pdev->dev);
	if (ret) {
		pr_err("%s fail to power on cpp\n", __func__);
		goto fail;
	}
	__pm_stay_awake(dev->ws);
#endif
	hw = dev->hw_info;
	if (!hw) {
		pr_err("fail to get valid cpp hw info\n");
		goto fail;
	}
	cppif = vzalloc(sizeof(struct cpp_pipe_dev));
	if (!cppif) {
		pr_err("fail to alloc memory \n");
		goto fail;
	}
	ret = cpp_drv_get_cpp_res(cppif, hw);
	if (ret) {
		pr_err("fail to get cpp res\n");
		goto fail;
	}
	dev->cppif = cppif;
	ret = cppcore_module_enable(dev);
	if (ret) {
		pr_err("fail to enable cpp module\n");
		goto enable_fail;
	}

	ret = cpp_int_irq_request(cppif);
	if (ret < 0) {
		pr_err("fail to install IRQ %d\n", ret);
		goto int_fail;
	}

	CPP_TRACE("open sprd_cpp success\n");

	return ret;

int_fail:
enable_fail:
	cppcore_module_disable(dev);
fail:
	if (atomic_dec_return(&dev->users) != 0)
		CPP_TRACE("others is using cpp device\n");
	file->private_data = NULL;
	if (cppif)
		vfree(cppif);
	return ret;
}

static int cppcore_release(struct inode *node,
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
	if (dev->cppif == NULL) {
		pr_err("fail to get cppif\n");
		return -EFAULT;
	}

	if (atomic_dec_return(&dev->users) != 0) {
		CPP_TRACE("others is using cpp device\n");
		return ret;
	}
	if (dev->cppif->rotif) {
		vfree(dev->cppif->rotif);
		dev->cppif->rotif = NULL;
	}
	if (dev->cppif->scif) {
		vfree(dev->cppif->scif);
		dev->cppif->scif = NULL;
	}
	if (dev->cppif->dmaif) {
		vfree(dev->cppif->dmaif);
		dev->cppif->dmaif = NULL;
	}
	cpp_int_irq_free(dev->cppif);
	cppcore_module_disable(dev);
	pr_info("cpp mdbg info:%d\n", cpp_dma_cnt);
	vfree(dev->cppif);
	dev->cppif = NULL;

	file->private_data = NULL;
	CPP_TRACE("cpp release success\n");

	return ret;
}

static const struct file_operations cpp_fops = {
	.open = cppcore_open,
	.unlocked_ioctl = cppcore_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cppcore_ioctl,
#endif
	.release = cppcore_release,
};

static int cppcore_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct cpp_device *dev = NULL;
	struct cpp_hw_info * hw_info = NULL;

	if (!pdev) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		pr_err("fail to alloc cpp dev memory\n");
		ret = -ENOMEM;
		goto fail;
	}

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
	dev->hw_info = (struct cpp_hw_info *)
		of_device_get_match_data(&pdev->dev);
	if (!dev->hw_info) {
		pr_err("fail to get cpp_hw_info\n");
		goto misc_fail;
	}
	hw_info = dev->hw_info;
	hw_info->pdev = pdev;
	hw_info->cpp_probe(pdev, hw_info);
	dev->io_base = hw_info->ip_cpp->io_base;
	mutex_init(&dev->lock);
	atomic_set(&dev->users, 0);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	pm_runtime_enable(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	dev->ws = wakeup_source_create("Camera cpp Wakeup Source");
	wakeup_source_add(dev->ws);
#endif
	platform_set_drvdata(pdev, (void *)dev);
	CPP_TRACE("cpp probe OK\n");

	return 0;

misc_fail:
	pr_err("fai to probe cpp module\n");
	misc_deregister(&dev->md);
fail:
	return ret;
}

static int cppcore_remove(struct platform_device *pdev)
{
	struct cpp_device *dev = platform_get_drvdata(pdev);
	wakeup_source_remove(dev->ws);
	wakeup_source_destroy(dev->ws);
	misc_deregister(&dev->md);
	return 0;
}

static const struct of_device_id of_match_table[] = {
	#if defined (PROJ_CPP_R3P0)
	{ .compatible = "sprd,cpp", .data = &lite_r3p0_cpp_hw_info},
	#elif defined (PROJ_CPP_R4P0)
	{ .compatible = "sprd,cpp", .data = &lite_r4p0_cpp_hw_info},
	#elif defined (PROJ_CPP_R5P0)
	{ .compatible = "sprd,cpp", .data = &lite_r5p0_cpp_hw_info},
	#elif defined (PROJ_CPP_R6P0)
	{ .compatible = "sprd,cpp", .data = &lite_r6p0_cpp_hw_info},
	#endif
	{},
};

static struct platform_driver sprd_cpp_driver = {
	.probe = cppcore_probe,
	.remove = cppcore_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = CPP_DEVICE_NAME,
		.of_match_table = of_match_ptr(of_match_table),
	},
};

module_platform_driver(sprd_cpp_driver);

MODULE_DESCRIPTION("CPP Driver");
MODULE_AUTHOR("Multimedia_Camera@unisoc");
MODULE_LICENSE("GPL");
