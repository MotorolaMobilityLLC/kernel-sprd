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

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>

#include <video/sprd_img.h>
#include "flash_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "FLASH_DRV: %d: %d " fmt, current->pid, __LINE__

#define FLASH_TEST
#define FLASH_DEVICE_NAME       "sprd_flash"

/* Structure Definitions */

struct flash_device {
	struct miscdevice md;
	const struct sprd_flash_driver_ops *ops[SPRD_FLASH_MAX];
	void *driver_data[SPRD_FLASH_MAX];
	int flashlight_status[SPRD_FLASH_MAX];
	unsigned short attr_test_value;
};

/* Static Variables Definitions */
static struct platform_device *pdev;
static struct flash_device *s_flash_dev;

/* Internal Function Implementation */

static int flash_open_torch(struct flash_device *dev,
			    uint8_t flash_idx, uint8_t led_idx)
{
	int ret = -EPERM;

	if (!dev || !dev->ops[flash_idx])
		goto exit;

	if (dev->ops[flash_idx]->open_torch)
		ret = dev->ops[flash_idx]->open_torch(
				dev->driver_data[flash_idx], led_idx);
exit:
	return ret;
}

static int flash_close_torch(struct flash_device *dev,
			    uint8_t flash_idx, uint8_t led_idx)
{
	int ret = -EPERM;

	if (!dev || !dev->ops[flash_idx])
		goto exit;

	if (dev->ops[flash_idx]->close_torch)
		ret = dev->ops[flash_idx]->close_torch(
				dev->driver_data[flash_idx], led_idx);

exit:
	return ret;
}

static int flash_open_preflash(struct flash_device *dev,
			    uint8_t flash_idx, uint8_t led_idx)
{
	int ret = -EPERM;

	if (!dev || !dev->ops[flash_idx])
		goto exit;

	if (dev->ops[flash_idx]->open_preflash)
		ret = dev->ops[flash_idx]->open_preflash(
				dev->driver_data[flash_idx], led_idx);
exit:
	return ret;
}

static int flash_close_preflash(struct flash_device *dev,
			    uint8_t flash_idx, uint8_t led_idx)
{
	int ret = -EPERM;

	if (!dev || !dev->ops[flash_idx])
		goto exit;

	if (dev->ops[flash_idx]->close_preflash)
		ret = dev->ops[flash_idx]->close_preflash(
				dev->driver_data[flash_idx], led_idx);

exit:
	return ret;
}

static int flash_open_highlight(struct flash_device *dev,
			    uint8_t flash_idx, uint8_t led_idx)
{
	int ret = -EPERM;

	if (!dev || !dev->ops[flash_idx])
		goto exit;

	if (dev->ops[flash_idx]->open_highlight)
		ret = dev->ops[flash_idx]->open_highlight(
				dev->driver_data[flash_idx], led_idx);
exit:
	return ret;
}

static int flash_close_highlight(struct flash_device *dev,
			    uint8_t flash_idx, uint8_t led_idx)
{
	int ret = -EPERM;

	if (!dev || !dev->ops[flash_idx])
		goto exit;

	if (dev->ops[flash_idx]->close_highlight)
		ret = dev->ops[flash_idx]->close_highlight(
				dev->driver_data[flash_idx], led_idx);
exit:
	return ret;
}
static int flash_cfg_value_torch(struct flash_device *dev,
				    uint8_t flash_idx, uint8_t led_idx,
				    struct sprd_flash_element *element)
{
	int ret = -EPERM;

	if (!dev || !dev->ops[flash_idx])
		goto exit;

	if (dev->ops[flash_idx]->cfg_value_torch)
		ret = dev->ops[flash_idx]->cfg_value_torch(
				dev->driver_data[flash_idx], led_idx, element);
exit:
	return ret;
}

static int flash_cfg_value_preflash(struct flash_device *dev,
				    uint8_t flash_idx, uint8_t led_idx,
				    struct sprd_flash_element *element)
{
	int ret = -EPERM;

	if (!dev || !dev->ops[flash_idx])
		goto exit;

	if (dev->ops[flash_idx]->cfg_value_preflash)
		ret = dev->ops[flash_idx]->cfg_value_preflash(
				dev->driver_data[flash_idx], led_idx, element);
exit:
	return ret;
}

static int flash_cfg_value_highlight(struct flash_device *dev,
				    uint8_t flash_idx, uint8_t led_idx,
				    struct sprd_flash_element *element)
{
	int ret = -EPERM;

	if (!dev || !dev->ops[flash_idx])
		goto exit;

	if (dev->ops[flash_idx]->cfg_value_highlight)
		ret = dev->ops[flash_idx]->cfg_value_highlight(
				dev->driver_data[flash_idx], led_idx, element);
exit:
	return ret;
}

static int flash_get_flash_info(struct flash_device *dev,
				    uint8_t flash_idx, uint8_t led_idx,
				    struct sprd_flash_capacity *element)
{
	int ret = -EPERM;

	if (!dev || !dev->ops[flash_idx])
		goto exit;

	if (dev->ops[flash_idx]->get_flash_info)
		ret = dev->ops[flash_idx]->get_flash_info(
				dev->driver_data[flash_idx], led_idx, element);
exit:
	return ret;
}

static int flash_close_all(struct flash_device *dev,
			    uint8_t flash_idx, uint8_t led_idx)
{
	int ret = 0;

	ret = flash_close_torch(dev, flash_idx, led_idx);
	ret |= flash_close_preflash(dev, flash_idx, led_idx);
	ret |= flash_close_highlight(dev, flash_idx, led_idx);

	return ret;
}

int sprd_flash_get_info(uint8_t flash_idx, uint8_t led_idx,
			struct sprd_flash_capacity *flash_capacity)
{
	int ret = 0;
	struct flash_device *flash_dev = s_flash_dev;

	pr_info("flash_idx 0x%x led_idx %d\n", flash_idx, led_idx);

	ret = flash_get_flash_info(flash_dev,
				   flash_idx,
				   led_idx, flash_capacity);

	return ret;
}
EXPORT_SYMBOL(sprd_flash_get_info);

int sprd_flash_ctrl(struct sprd_img_set_flash *set_flash)
{
	int ret = 0;
	int status = FLASH_STATUS_MAX;
	unsigned int led_index = 0;
	struct flash_device *flash_dev = s_flash_dev;

	if (set_flash->led0_ctrl) {
		led_index |= 1;
		status = set_flash->led0_status;
	}

	if (set_flash->led1_ctrl) {
		led_index |= 2;
		status = set_flash->led1_status;
	}
	pr_info("ctrl: led_index %d status 0x%x\n", led_index, status);
	switch (status) {
	case FLASH_OPEN:
		ret = flash_open_preflash(flash_dev,
					  set_flash->flash_index,
					  led_index);
		break;
	case FLASH_TORCH:
		ret = flash_open_torch(flash_dev,
				       set_flash->flash_index,
				       led_index);
		break;
	case FLASH_HIGH_LIGHT:
		ret = flash_open_highlight(flash_dev,
					   set_flash->flash_index,
					   led_index);
		break;
	case FLASH_CLOSE_AFTER_OPEN:
	case FLASH_CLOSE_AFTER_AUTOFOCUS:
	case FLASH_CLOSE:
		if (!flash_dev->flashlight_status[set_flash->flash_index])
			ret = flash_close_all(flash_dev,
					      set_flash->flash_index,
					      led_index);
		break;
	default:
		pr_info("invalid mode: 0x%x led_index %d\n", status, led_index);
		break;
	}

	return ret;
}
EXPORT_SYMBOL(sprd_flash_ctrl);

int sprd_flash_cfg(struct sprd_flash_cfg_param *parm)
{
	int ret = 0;
	int status = FLASH_TYPE_MAX;
	uint16_t index = 0;
	uint16_t value = 0;
	uint8_t led_idx = 0;
	uint8_t flash_idx = 0;
	struct flash_device *flash_dev = s_flash_dev;
	struct sprd_flash_element element;

	if (!parm || !flash_dev) {
		pr_info("flash_cfg NULL parm %p flash_dev %p\n",
		       parm, flash_dev);
		return -EPERM;
	}
	index = parm->real_cell.element[0].index;
	value = parm->real_cell.element[0].val;
	status = parm->real_cell.type;
	led_idx = parm->real_cell.led_idx;
	flash_idx = parm->flash_idx;
	element.index = index;
	element.val = value;

	pr_info("flash value: %d index %d status %d\n",
		element.val, index, status);

	switch (status) {
	case FLASH_TYPE_TORCH:
		ret = flash_cfg_value_torch(flash_dev,
						flash_idx, led_idx, &element);
		break;
	case FLASH_TYPE_PREFLASH:
		ret = flash_cfg_value_preflash(flash_dev,
					       flash_idx, led_idx, &element);
		break;
	case FLASH_TYPE_MAIN:
		ret = flash_cfg_value_highlight(flash_dev,
						flash_idx, led_idx, &element);
		break;
	default:
		pr_info("invalid mode: 0x%x index %d\n", status, index);
		break;
	}

	return ret;
}
EXPORT_SYMBOL(sprd_flash_cfg);

static int sprd_flash_open(struct inode *node, struct file *file)
{
	struct flash_device *flash_dev;
	struct miscdevice *md = file->private_data;

	if (!md)
		return -EFAULT;

	flash_dev = md->this_device->platform_data;

	/* TODO */

	file->private_data = (void *)flash_dev;

	return 0;
}

static int sprd_flash_release(struct inode *node, struct file *file)
{
	struct flash_device *flash_dev;

	flash_dev = (struct flash_device *)file->private_data;
	if (!flash_dev)
		return -EFAULT;

	/* TODO */

	file->private_data = NULL;

	return 0;
}

static long sprd_flash_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	struct flash_device *flash_dev;

	flash_dev = file->private_data;
	if (!flash_dev)
		return -EFAULT;

	return 0;
}

#ifdef FLASH_TEST
static ssize_t flash_sysfs_test(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	int ret = 0;
	unsigned int val, cmd, led_idx;
	unsigned int flash_idx = 0;
	struct flash_device *flash_dev = NULL;
	struct sprd_flash_element element;

	flash_dev = dev->platform_data;
	if (!flash_dev) {
		pr_err("flash device is null\n");
		return 0;
	}

	ret = kstrtouint(buf, 16, &val);
	if (ret)
		goto exit;

	flash_dev->attr_test_value = val;

	cmd = val & 0x0f;
	led_idx = (val & 0xf0) >> 4;
	element.index = (val & 0x1f00) >> 8;
	flash_idx = (val & 0x8000) >> 15;

	pr_info("cmd:%d flash:%d element.index %d flash_idx %d\n",
			cmd, led_idx, element.index, flash_idx);

	switch (cmd) {
	case 0:
		flash_open_preflash(flash_dev, flash_idx, led_idx);
		break;
	case 1:
		flash_close_preflash(flash_dev, flash_idx, led_idx);
		break;
	case 2:
		flash_dev->flashlight_status[0] = 1;
		flash_open_torch(flash_dev, flash_idx, led_idx);
		break;
	case 3:
		flash_dev->flashlight_status[0] = 0;
		flash_close_torch(flash_dev, flash_idx, led_idx);
		break;
	case 4:
		flash_open_highlight(flash_dev, flash_idx, led_idx);
		break;
	case 5:
		flash_close_highlight(flash_dev, flash_idx, led_idx);
		break;
	case 6:
		flash_cfg_value_preflash(flash_dev, flash_idx,
					 led_idx, &element);
		break;
	case 7:
		flash_cfg_value_torch(flash_dev, flash_idx, led_idx, &element);
		break;
	case 8:
		flash_cfg_value_highlight(flash_dev, flash_idx,
					 led_idx, &element);
		break;
	default:
		break;
	}

	return size;

exit:
	return ret;
}

static ssize_t show_help(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct flash_device *flash_dev = NULL;

	flash_dev = dev->platform_data;
	if (!flash_dev) {
		pr_err("flash device is null\n");
		return 0;
	}

	return sprintf(buf, "%x\n", flash_dev->attr_test_value);
}

static DEVICE_ATTR(test, 0600, show_help, flash_sysfs_test);
#endif

static const struct file_operations flash_fops = {
	.owner = THIS_MODULE,
	.open = sprd_flash_open,
	.unlocked_ioctl = sprd_flash_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = sprd_flash_ioctl,
#endif
	.release = sprd_flash_release,
};

int sprd_flash_register(const struct sprd_flash_driver_ops *ops,
			void *drvd,
			uint8_t flash_idx)
{
	if (!s_flash_dev)
		return -EPROBE_DEFER;

	s_flash_dev->ops[flash_idx] = ops;
	s_flash_dev->driver_data[flash_idx] = drvd;

	return 0;
}

static int sprd_flash_probe(struct platform_device *pdev)
{
	int ret;
	struct flash_device *flash_dev;

	flash_dev = devm_kzalloc(&pdev->dev, sizeof(*flash_dev), GFP_KERNEL);
	if (!flash_dev)
		return -ENOMEM;

	flash_dev->md.minor = MISC_DYNAMIC_MINOR;
	flash_dev->md.name = FLASH_DEVICE_NAME;
	flash_dev->md.fops = &flash_fops;
	flash_dev->md.parent = NULL;
	ret = misc_register(&flash_dev->md);
	if (ret) {
		pr_err("failed to register misc devices\n");
		goto exit;
	}

	flash_dev->md.this_device->platform_data = (void *)flash_dev;
	platform_set_drvdata(pdev, (void *)flash_dev);

	s_flash_dev = flash_dev;

#ifdef FLASH_TEST
	ret = device_create_file(flash_dev->md.this_device, &dev_attr_test);
	if (ret < 0) {
		pr_err("failed to create flash test file");
		goto fail;
	}
#endif

	return 0;

fail:
	misc_deregister(&flash_dev->md);

exit:
	pr_err("failed to probe flash driver\n");
	return ret;
}

static int sprd_flash_remove(struct platform_device *pdev)
{
	struct flash_device *flash_dev = platform_get_drvdata(pdev);

#ifdef FLASH_TEST
	device_remove_file(flash_dev->md.this_device, &dev_attr_test);
#endif

	misc_deregister(&flash_dev->md);
	platform_set_drvdata(pdev, NULL);
	s_flash_dev = NULL;

	return 0;
}

static struct platform_driver sprd_flash_drvier = {
	.probe = sprd_flash_probe,
	.remove = sprd_flash_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = FLASH_DEVICE_NAME,
		   },
};

static int __init flash_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&sprd_flash_drvier);
	if (ret != 0) {
		pr_err("failed to register flash driver\n");
		goto exit;
	}

	pdev = platform_device_register_simple(FLASH_DEVICE_NAME, -1, NULL, 0);
	if (IS_ERR_OR_NULL(pdev)) {
		ret = PTR_ERR(pdev);
		platform_driver_unregister(&sprd_flash_drvier);
		goto exit;
	}

exit:
	return ret;
}

static void __exit flash_exit(void)
{
	platform_device_unregister(pdev);
	platform_driver_unregister(&sprd_flash_drvier);
}

subsys_initcall(flash_init);
module_exit(flash_exit);
MODULE_DESCRIPTION("Sprd Flash Driver");
MODULE_LICENSE("GPL");
