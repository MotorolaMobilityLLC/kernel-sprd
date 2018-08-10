/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/bug.h>
#include <linux/delay.h>
#ifdef CONFIG_SC2342_INTEG
#include <linux/gnss.h>
#endif
#include <linux/init.h>
#include <linux/kernel.h>
#include <misc/marlin_platform.h>
#include <linux/mfd/sprd/pmic_glb_reg.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <misc/wcn_bus.h>

#include "../../include/wcn_glb_reg.h"
#include "gnss_common.h"
#include "gnss_dump.h"
#include "wcn_glb.h"

#define GNSSCOMM_INFO(format, arg...) pr_info("gnss_ctl: " format, ## arg)
#define GNSSCOMM_ERR(format, arg...) pr_err("gnss_ctl: " format, ## arg)

#define GNSS_DATA_BASE_TYPE_H  16
#define GNSS_MAX_STRING_LEN	10
/* gnss mem dump success return value is 3 */
#define GNSS_DUMP_DATA_SUCCESS	3
#define FIRMWARE_FILEPATHNAME_LENGTH_MAX 256

struct gnss_common_ctl {
	struct device *dev;
	unsigned long chip_ver;
	unsigned int gnss_status;
	unsigned int gnss_subsys;
	char firmware_path[FIRMWARE_FILEPATHNAME_LENGTH_MAX];
};

static struct gnss_common_ctl gnss_common_ctl_dev;

enum gnss_status_e {
	GNSS_STATUS_POWEROFF = 0,
	GNSS_STATUS_POWERON,
	GNSS_STATUS_ASSERT,
	GNSS_STATUS_POWEROFF_GOING,
	GNSS_STATUS_POWERON_GOING,
	GNSS_STATUS_MAX,
};

static const struct of_device_id gnss_common_ctl_of_match[] = {
	{.compatible = "sprd,gnss_common_ctl", .data = (void *)0x22},
	{},
};

#ifndef CONFIG_SC2342_INTEG
struct gnss_cali {
	bool cali_done;
	u32 *cali_data;
};
static struct gnss_cali gnss_cali_data;


#ifdef GNSSDEBUG
static void gnss_cali_done_isr(void)
{
	complete(&marlin_dev->gnss_cali_done);
	GNSSCOMM_INFO("gnss cali done");
}
#endif
static int gnss_cali_init(void)
{
	gnss_cali_data.cali_done = false;

	gnss_cali_data.cali_data = kzalloc(GNSS_CALI_DATA_SIZE, GFP_KERNEL);
	if (gnss_cali_data.cali_data == NULL) {
		GNSSCOMM_ERR("%s malloc fail.\n", __func__);
		return -ENOMEM;
	}

	#ifdef GNSSDEBUG
	init_completion(&marlin_dev.gnss_cali_done);
	sdio_pub_int_regcb(GNSS_CALI_DONE, (PUB_INT_ISR)gnss_cali_done_isr);
	#endif
	return 0;
}

static int gnss_write_cali_data(void)
{
	if (gnss_cali_data.cali_done) {
		sprdwcn_bus_direct_write(GNSS_CALI_ADDRESS,
			gnss_cali_data.cali_data, GNSS_CALI_DATA_SIZE);
	}
	GNSSCOMM_INFO("gnss write calidata successful\n");
	return 0;
}

int gnss_backup_cali(void)
{

#ifdef GNSSDEBUG
	int i = 0;
#endif

	if (!gnss_cali_data.cali_done) {
		GNSSCOMM_INFO("%s begin\n", __func__);
#ifdef GNSSDEBUG
		if (gnss_cali_data.cali_data != NULL) {
			do {
				sprdwcn_bus_direct_read(GNSS_CALI_ADDRESS,
					gnss_cali_data.cali_data,
					GNSS_CALI_DATA_SIZE);
				msleep(50);
				if (i == 10)
					break;
				i++;
				GNSSCOMM_INFO(" cali time out i = %d\n", i);
			} while (*(gnss_cali_data.cali_data) !=
				GNSS_CALI_DONE_FLAG)
		}
#endif
		gnss_cali_data.cali_done = true;
	}
	GNSSCOMM_INFO("gnss backup calidata successful\n");

	return 0;
}

#endif

static void gnss_power_on(bool enable)
{
	int ret;

	GNSSCOMM_INFO("%s:enable=%d,current gnss_status=%d\n", __func__,
			enable, gnss_common_ctl_dev.gnss_status);
	if (enable && gnss_common_ctl_dev.gnss_status == GNSS_STATUS_POWEROFF) {
#ifndef CONFIG_SC2342_INTEG
		gnss_write_cali_data();
#endif
		gnss_common_ctl_dev.gnss_status = GNSS_STATUS_POWERON_GOING;
		ret = start_marlin(gnss_common_ctl_dev.gnss_subsys);
		if (ret != 0)
			GNSSCOMM_INFO("%s: start marlin failed ret=%d\n",
					__func__, ret);
		else
			gnss_common_ctl_dev.gnss_status = GNSS_STATUS_POWERON;
	} else if (!enable && gnss_common_ctl_dev.gnss_status
			== GNSS_STATUS_POWERON) {
		gnss_common_ctl_dev.gnss_status = GNSS_STATUS_POWEROFF_GOING;
		ret = stop_marlin(gnss_common_ctl_dev.gnss_subsys);
		if (ret != 0)
			GNSSCOMM_INFO("%s: stop marlin failed ret=%d\n",
				 __func__, ret);
		else
			gnss_common_ctl_dev.gnss_status = GNSS_STATUS_POWEROFF;
	} else {
		GNSSCOMM_INFO("%s: status is not match\n", __func__);
	}
}

static ssize_t gnss_power_enable_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long set_value;

	if (kstrtoul(buf, GNSS_MAX_STRING_LEN, &set_value)) {
		GNSSCOMM_ERR("%s, Maybe store string is too long\n", __func__);
		return -EINVAL;
	}
	GNSSCOMM_INFO("%s,%lu\n", __func__, set_value);
	if (set_value == 1)
		gnss_power_on(1);
	else if (set_value == 0)
		gnss_power_on(0);
	else {
		count = -EINVAL;
		GNSSCOMM_INFO("%s,unknown control\n", __func__);
	}

	return count;
}

static DEVICE_ATTR_WO(gnss_power_enable);

static ssize_t gnss_subsys_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long set_value;

	if (kstrtoul(buf, GNSS_MAX_STRING_LEN, &set_value))
		return -EINVAL;

	GNSSCOMM_INFO("%s,%lu\n", __func__, set_value);
#ifndef CONFIG_SC2342_INTEG
	gnss_common_ctl_dev.gnss_subsys = MARLIN_GNSS;
#else
	if (set_value == WCN_GNSS)
		gnss_common_ctl_dev.gnss_subsys = WCN_GNSS;
	else if (set_value == WCN_GNSS_BD)
		gnss_common_ctl_dev.gnss_subsys  = WCN_GNSS_BD;
	else
		count = -EINVAL;
#endif
	return count;
}

void gnss_file_path_set(char *buf)
{
	strcpy(&gnss_common_ctl_dev.firmware_path[0], buf);
}

static ssize_t gnss_subsys_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int i = 0;

	GNSSCOMM_INFO("%s\n", __func__);
	if (gnss_common_ctl_dev.gnss_status == GNSS_STATUS_POWERON) {
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d:%s\n",
				gnss_common_ctl_dev.gnss_subsys,
				&gnss_common_ctl_dev.firmware_path[0]);
	} else {
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				gnss_common_ctl_dev.gnss_subsys);
	}

	return i;
}

static DEVICE_ATTR_RW(gnss_subsys);

static ssize_t gnss_dump_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long set_value;
	int ret = 0;

	if (kstrtoul(buf, GNSS_MAX_STRING_LEN, &set_value)) {
		GNSSCOMM_ERR("%s, store string is too long\n", __func__);
		return -EINVAL;
	}
	GNSSCOMM_INFO("%s,%lu\n", __func__, set_value);
	if (set_value == 1) {
		ret = gnss_dump_mem();
		gnss_common_ctl_dev.gnss_status = GNSS_STATUS_ASSERT;
		if (!ret)
			ret = GNSS_DUMP_DATA_SUCCESS;
		else
			ret = -1;
	} else
		count = -EINVAL;

	return ret;
}

static DEVICE_ATTR_WO(gnss_dump);

static ssize_t gnss_status_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int i = 0;

	GNSSCOMM_INFO("%s\n", __func__);

	i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			gnss_common_ctl_dev.gnss_status);

	return i;
}
static DEVICE_ATTR_RO(gnss_status);
#ifndef CONFIG_SC2342_INTEG
static uint gnss_op_reg;
static ssize_t gnss_regvalue_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int i = 0;
	unsigned char *buffer;

	buffer = kzalloc(4, GFP_KERNEL);
	if (buffer == NULL) {
		GNSSCOMM_ERR("%s malloc fail.\n", __func__);
		return -ENOMEM;
	}
	GNSSCOMM_INFO("%s, register is 0x%x\n", __func__, gnss_op_reg);
	sprdwcn_bus_direct_read(gnss_op_reg, buffer, 4);
	GNSSCOMM_INFO("%s,temp value is 0x%x\n", __func__,
		*(u32 *)buffer);

	i += scnprintf(buf + i, PAGE_SIZE - i,
		"show: 0x%x\n", *(u32 *)buffer);
	kfree(buffer);

	return i;
}
static DEVICE_ATTR_RO(gnss_regvalue);

static ssize_t gnss_regwrite_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long set_value;

	if (kstrtoul(buf, GNSS_DATA_BASE_TYPE_H, &set_value)) {
		GNSSCOMM_ERR("%s, input error\n", __func__);
		return -EINVAL;
	}
	GNSSCOMM_INFO("%s,0x%lx\n", __func__, set_value);
	gnss_op_reg = (uint)set_value;

	return count;
}
static DEVICE_ATTR_WO(gnss_regwrite);
#endif

static struct attribute *gnss_common_ctl_attrs[] = {
	&dev_attr_gnss_power_enable.attr,
	&dev_attr_gnss_dump.attr,
	&dev_attr_gnss_status.attr,
	&dev_attr_gnss_subsys.attr,
#ifndef CONFIG_SC2342_INTEG
	&dev_attr_gnss_regvalue.attr,
	&dev_attr_gnss_regwrite.attr,
#endif
	NULL,
};

static struct attribute_group gnss_common_ctl_group = {
	.name = NULL,
	.attrs = gnss_common_ctl_attrs,
};

static struct miscdevice gnss_common_ctl_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gnss_common_ctl",
	.fops = NULL,
};

static int gnss_common_ctl_probe(struct platform_device *pdev)
{
	int ret;
	const struct of_device_id *of_id;

	GNSSCOMM_INFO("%s enter", __func__);
	gnss_common_ctl_dev.dev = &pdev->dev;

	gnss_common_ctl_dev.gnss_status = GNSS_STATUS_POWEROFF;
#ifndef CONFIG_SC2342_INTEG
	gnss_common_ctl_dev.gnss_subsys = MARLIN_GNSS;
	gnss_cali_init();
#else
	gnss_common_ctl_dev.gnss_subsys = WCN_GNSS;
#endif
	/* considering backward compatibility, it's not use now  start */
	of_id = of_match_node(gnss_common_ctl_of_match,
		pdev->dev.of_node);
	if (!of_id) {
		dev_err(&pdev->dev,
			"get gnss_common_ctl of device id failed!\n");
		return -ENODEV;
	}
	gnss_common_ctl_dev.chip_ver = (unsigned long)(of_id->data);
	/* considering backward compatibility, it's not use now  end */

	platform_set_drvdata(pdev, &gnss_common_ctl_dev);
	ret = misc_register(&gnss_common_ctl_miscdev);
	if (ret) {
		GNSSCOMM_ERR("%s failed to register gnss_common_ctl.\n",
			__func__);
		return ret;
	}

	ret = sysfs_create_group(&gnss_common_ctl_miscdev.this_device->kobj,
			&gnss_common_ctl_group);
	if (ret) {
		GNSSCOMM_ERR("%s failed to create device attributes.\n",
			__func__);
		goto err_attr_failed;
	}

	return 0;

err_attr_failed:
	misc_deregister(&gnss_common_ctl_miscdev);
	return ret;
}

static int gnss_common_ctl_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&gnss_common_ctl_miscdev.this_device->kobj,
				&gnss_common_ctl_group);

	misc_deregister(&gnss_common_ctl_miscdev);
	return 0;
}
static struct platform_driver gnss_common_ctl_drv = {
	.driver = {
		   .name = "gnss_common_ctl",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(gnss_common_ctl_of_match),
		   },
	.probe = gnss_common_ctl_probe,
	.remove = gnss_common_ctl_remove
};

static int __init gnss_common_ctl_init(void)
{
	return platform_driver_register(&gnss_common_ctl_drv);
}

static void __exit gnss_common_ctl_exit(void)
{
	platform_driver_unregister(&gnss_common_ctl_drv);
}

module_init(gnss_common_ctl_init);
module_exit(gnss_common_ctl_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum Gnss Driver");
MODULE_AUTHOR("Jun.an<jun.an@spreadtrum.com>");
