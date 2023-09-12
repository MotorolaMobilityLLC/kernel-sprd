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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include "sprd_scale_k.h"
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/vmalloc.h>

#include "img_scale.h"
#include "dcam_drv.h"
#include <linux/completion.h>
#include <sprd_mm.h>

#define SCALE_DEVICE_NAME "sprd_scale"
#define SCALE_TIMEOUT             msecs_to_jiffies(5000)/*ms*/
#define SCALE_MINOR MISC_DYNAMIC_MINOR
#define SC_COEFF_BUF_SIZE (24 << 10)

static void scale_k_irq(void *fd)
{
	struct scale_k_file *scale_file = (struct scale_k_file *)fd;

	if (!scale_file) {
		pr_err("scale_k_irq error: hand is null");
		return;
	}

	pr_err("sc done.\n");

	complete(&scale_file->scale_done_com);
}

static void scale_k_file_init(struct scale_k_file *fd,
		struct scale_k_private *scale_private)
{
	fd->scale_private = scale_private;

	init_completion(&fd->scale_done_com);

	fd->drv_private.scale_fd = (void *)fd;
	fd->drv_private.path_info.coeff_addr = scale_private->coeff_addr;

	spin_lock_init(&fd->drv_private.scale_drv_lock);
}

static int scale_k_open(struct inode *node, struct file *file)
{
	int ret = 0;
	struct scale_k_file *fd = NULL;
	struct scale_k_private *scale_private = NULL;
	struct miscdevice *md = file->private_data;

	if (!md) {
		ret = -EFAULT;
		pr_err("scale_k_open error: miscdevice is null\n");
		goto exit;
	}
	scale_private =
		(struct scale_k_private *)md->this_device->platform_data;
	if (!scale_private) {
		ret = -EFAULT;
		pr_err("scale_k_open error: scale_private is null\n");
		goto exit;
	}

	fd = vzalloc(sizeof(*fd));
	if (!fd) {
		ret = -ENOMEM;
		pr_err("scale_k_open error: alloc\n");
		goto exit;
	}
	fd->dn = md->this_device->of_node;
	scale_k_file_init(fd, scale_private);

	file->private_data = fd;

	SCALE_TRACE("scale_k_open fd=%p ret=%d\n", fd, ret);

exit:
	return ret;
}

ssize_t scale_k_read(struct file *file, char __user *u_data,
						size_t cnt, loff_t *cnt_ret)
{
	uint32_t rt_word[2];

	(void)file; (void)cnt; (void)cnt_ret;

	if (cnt < sizeof(uint32_t)) {
		pr_err("scale_k_read error: wrong size of u_data: %d\n", cnt);
		return -1;
	}

	rt_word[0] = SCALE_LINE_BUF_LENGTH;
	rt_word[1] = SCALE_SC_COEFF_MAX;

	return copy_to_user(u_data, (void *)rt_word,
		(uint32_t)(2 * sizeof(uint32_t)));
}

static int scale_k_release(struct inode *node, struct file *file)
{
	struct scale_k_file *fd = NULL;
	struct scale_k_private *scale_private = NULL;

	fd = file->private_data;
	if (!fd)
		goto exit;


	scale_private = fd->scale_private;
	if (!scale_private)
		goto fd_free;

	wait_for_completion(&scale_private->start_com);
	complete(&scale_private->start_com);

fd_free:
	vfree(fd);
	fd = NULL;
	file->private_data = NULL;

exit:
	SCALE_TRACE("scale_k_release\n");

	return 0;
}

static void sprd_img_print_reg(void)
{

	unsigned long                addr = 0;

	pr_info("DCAM: Register list");
	for (addr = DCAM_CFG; addr <= DCAM_IP_REVISION; addr += 16) {
		pr_info("DCAM: 0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			REG_RD(addr),
			REG_RD(addr + 4),
			REG_RD(addr + 8),
			REG_RD(addr + 12));
	}

#if 0
	uint32_t *reg_buf = NULL;
	uint32_t                 reg_buf_len = 0x400;
	int                      ret;
	uint32_t                 print_len = 0, print_cnt = 0;

	reg_buf = vzalloc(reg_buf_len);
	if (reg_buf == NULL)
		return;
	ret = dcam_read_registers(reg_buf, &reg_buf_len);
	if (ret) {
		vfree(reg_buf);
		return;
	}
	pr_err("dcam registers\n");
	while (print_len < reg_buf_len) {
		pr_err("offset 0x%03x : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			print_len,
			reg_buf[print_cnt],
			reg_buf[print_cnt+1],
			reg_buf[print_cnt+2],
			reg_buf[print_cnt+3]);
		print_cnt += 4;
		print_len += 16;
	}
	udelay(1);
	vfree(reg_buf);
#endif
}

static long scale_k_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct scale_k_private *scale_private;
	struct scale_k_file *fd;
	struct scale_frame_param_t frame_params;
	struct scale_slice_param_t slice_params;

	fd = file->private_data;
	if (!fd) {
		ret = -EFAULT;
		pr_err("scale_k_ioctl error:  fd null\n");
		goto ioctl_exit;
	}

	scale_private = fd->scale_private;
	if (!scale_private) {
		ret = -EFAULT;
		pr_err("scale_k_ioctl erro: scale private null\n");
		goto ioctl_exit;
	}

	switch (cmd) {
	case SCALE_IO_START:

		wait_for_completion(&scale_private->start_com);
		ret = scale_k_module_en(fd->dn);
		if (unlikely(ret)) {
			pr_err("rot_k_ioctl erro: scale_module_en\n");
			complete(&scale_private->start_com);
			goto ioctl_exit;
		}

		ret = scale_k_isr_reg(scale_k_irq, &fd->drv_private);
		if (unlikely(ret)) {
			pr_err("rot_k_ioctl error:  scale_k_isr_reg\n");
			scale_k_module_dis(fd->dn);
			complete(&scale_private->start_com);
			goto ioctl_exit;

		}

		ret = copy_from_user(&frame_params,
			(struct scale_frame_param_t *)arg,
			sizeof(frame_params));
		if (ret) {
			pr_err("rot_k_ioctl error: get frame param info\n");
			scale_k_module_dis(fd->dn);
			complete(&scale_private->start_com);
			goto ioctl_exit;
		}
		pr_debug("scale src_fd =0x%x, dst_fd = 0x%x",
			frame_params.input_addr.mfd[0],
			frame_params.output_addr.mfd[0]);
		fd->scale_done_com.done = 0;
		ret = scale_k_start(&frame_params, &fd->drv_private.path_info);
		if (ret) {
			pr_err("rot_k_ioctl error: frame start\n");
			pfiommu_free_addr(&fd->
				drv_private.path_info.scale_iommu_src);
			pfiommu_free_addr(&fd->
				drv_private.path_info.scale_iommu_dst);
			scale_k_module_dis(fd->dn);
			complete(&scale_private->start_com);
			goto ioctl_exit;
		}

		break;
	case SCALE_IO_DONE:
		ret = wait_for_completion_timeout(&fd->scale_done_com,
					SCALE_TIMEOUT);
		if (ret <= 0) {
			pr_err("scale_k_ioctl error:  interruptible time out\n");
			ret = -EBUSY;
			sprd_img_print_reg();
			goto ioctl_out;
		} else
			ret = 0;
		dcam_resize_end();
		scale_k_stop();
		pfiommu_free_addr(&fd->drv_private.path_info.scale_iommu_src);
		pfiommu_free_addr(&fd->drv_private.path_info.scale_iommu_dst);
		scale_k_module_dis(fd->dn);
		complete(&scale_private->start_com);
		break;
	case SCALE_IO_CONTINUE:
/*Caution: slice scale is not supported by current driver.Please do not use it*/
		ret = copy_from_user(&slice_params,
		(struct scale_slice_param_t *)arg, sizeof(slice_params));
		if (ret) {
			pr_err("rot_k_ioctl error: get slice param info\n");
			goto ioctl_exit;
		}

		ret = scale_k_continue(&slice_params,
			&fd->drv_private.path_info);
		if (ret)
			pr_err("rot_k_ioctl error: continue\n");

		break;

	default:
		break;
	}

ioctl_exit:
	return ret;

ioctl_out:
	dcam_resize_end();
	scale_k_stop();
	pfiommu_free_addr(&fd->drv_private.path_info.scale_iommu_src);
	pfiommu_free_addr(&fd->drv_private.path_info.scale_iommu_dst);
	scale_k_module_dis(fd->dn);
	complete(&scale_private->start_com);
	return ret;
}
/* for kernel formate static change to static const */
static const struct file_operations scale_fops = {
	.owner = THIS_MODULE,
	.open = scale_k_open,
	.read = scale_k_read,
	.unlocked_ioctl = scale_k_ioctl,
	.compat_ioctl = NULL,
	.release = scale_k_release,
};

static struct miscdevice scale_dev = {
	.minor = SCALE_MINOR,
	.name = SCALE_DEVICE_NAME,
	.fops = &scale_fops,
};

int scale_k_probe(struct platform_device *pdev)
{
	int ret;
	struct scale_k_private *scale_private;

	pr_info("scale_k_probe call in!");

	scale_private =
		devm_kzalloc(&pdev->dev, sizeof(*scale_private), GFP_KERNEL);
	if (!scale_private)
		return -ENOMEM;

	scale_private->coeff_addr = vzalloc(SC_COEFF_BUF_SIZE);
	if (!scale_private->coeff_addr) {
		devm_kfree(&pdev->dev, scale_private);
		return -ENOMEM;
	}
	init_completion(&scale_private->start_com);
	complete(&scale_private->start_com);

	platform_set_drvdata(pdev, scale_private);

	ret = misc_register(&scale_dev);
	if (ret) {
		pr_err("scale_k_probe error: ret=%d\n", ret);
		ret = -EACCES;
		goto probe_out;
	}

	scale_dev.this_device->of_node = pdev->dev.of_node;
	scale_dev.this_device->platform_data = (void *)scale_private;
	sprd_scale_drv_init(pdev);
	pr_info("scale_k_probe success!");
	goto exit;

probe_out:
	misc_deregister(&scale_dev);
	vfree(scale_private->coeff_addr);
	devm_kfree(&pdev->dev, scale_private);
	platform_set_drvdata(pdev, NULL);
exit:
	return 0;
}

static int scale_k_remove(struct platform_device *pdev)
{
	struct scale_k_private *scale_private;

	scale_private = platform_get_drvdata(pdev);

	if (!scale_private)
		goto remove_exit;

	misc_deregister(&scale_dev);
	if (scale_private->coeff_addr)
		vfree(scale_private->coeff_addr);

	devm_kfree(&pdev->dev, scale_private);
	platform_set_drvdata(pdev, NULL);

remove_exit:
	return 0;
}

static const struct of_device_id of_match_table_scale[] = {
	{ .compatible = "sprd,sprd_scale", },
	{ },
};

static struct platform_driver scale_driver = {
	.probe = scale_k_probe,
	.remove = scale_k_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = SCALE_DEVICE_NAME,
		.of_match_table = of_match_ptr(of_match_table_scale),
	}
};

int scale_k_init(void)
{
	if (platform_driver_register(&scale_driver) != 0) {
		pr_err("platform scale device register Failed\n");
		return -1;
	}

	return 0;
}

void scale_k_exit(void)
{
	platform_driver_unregister(&scale_driver);
}

#if 0
module_init(scale_k_init);
module_exit(scale_k_exit);
MODULE_DESCRIPTION("Sprd Scale Driver");
MODULE_LICENSE("GPL");
#endif
