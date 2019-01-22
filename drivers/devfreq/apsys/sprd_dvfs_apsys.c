/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "sprd_dvfs_comm.h"
#include "sprd_dvfs_apsys.h"

LIST_HEAD(apsys_dvfs_head);

struct class *dvfs_class;
struct apsys_regmap regmap_ctx;

#define to_apsys(DEV)	container_of((DEV), struct apsys_dev, dev)

static ssize_t cur_volt_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	//struct apsys_dev *apsys = to_apsys(dev);
	//u32 cur_volt;

	//if (apsys->dvfs_ops && apsys->dvfs_ops->get_cur_volt)
	//	apsys->dvfs_ops->get_cur_volt(apsys, &cur_volt);

	//sprintf(buf, "0x%x\n", cur_volt);

	return 0;
}
static DEVICE_ATTR_RO(cur_volt);

static struct attribute *apsys_attrs[] = {
	&dev_attr_cur_volt.attr,
	NULL,
};

static const struct attribute_group apsys_group = {
	.attrs = apsys_attrs,
};

static int apsys_dvfs_class_init(void)
{
	pr_info("apsys dvfs class init\n");

	dvfs_class = class_create(THIS_MODULE, "dvfs");
	if (IS_ERR(dvfs_class)) {
		pr_err("Unable to create apsys dvfs class\n");
		return PTR_ERR(dvfs_class);
	}

	return 0;
}

static int apsys_dvfs_device_create(struct apsys_dev *apsys,
				struct device *parent)
{
	int ret;

	apsys->dev.class = dvfs_class;
	apsys->dev.parent = parent;
	apsys->dev.of_node = parent->of_node;
	dev_set_name(&apsys->dev, "apsys");
	dev_set_drvdata(&apsys->dev, apsys);

	ret = device_register(&apsys->dev);
	if (ret)
		pr_err("apsys dvfs device register failed\n");

	return ret;
}

static int apsys_dvfs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct apsys_dev *apsys;
	const char *str = NULL;
	void __iomem *base;
	struct resource r;
	int ret;

	pr_info("apsys-dvfs initialized\n");

	apsys = devm_kzalloc(dev, sizeof(*apsys), GFP_KERNEL);
	if (!apsys)
		return -ENOMEM;

	str = (char *)of_device_get_match_data(dev);

	apsys->dvfs_ops = apsys_dvfs_ops_attach(str);
	if (!apsys->dvfs_ops) {
		pr_err("attach apsys dvfs ops %s failed\n", str);
		return -EINVAL;
	}

	of_property_read_u32(np, "sprd,sys-sw-dvfs-en",
		&apsys->dvfs_coffe.sys_sw_dvfs_en);
	of_property_read_u32(np, "sprd,sys-dvfs-hold-en",
		&apsys->dvfs_coffe.sys_dvfs_hold_en);
	of_property_read_u32(np, "sprd,sys-dvfs-clk-gate-ctrl",
		&apsys->dvfs_coffe.sys_dvfs_clk_gate_ctrl);
	of_property_read_u32(np, "sprd,sys-dvfs-wait_window",
		&apsys->dvfs_coffe.sys_dvfs_wait_window);
	of_property_read_u32(np, "sprd,sys-dvfs-min_volt",
		&apsys->dvfs_coffe.sys_dvfs_min_volt);

	if (of_address_to_resource(np, 0, &r)) {
		pr_err("parse apsys base address failed\n");
		return -ENODEV;
	}

	base = ioremap_nocache(r.start, resource_size(&r));
	if (IS_ERR(base)) {
		pr_err("ioremap apsys dvfs address failed\n");
		return -EFAULT;
	}
	regmap_ctx.apsys_base = (unsigned long)base;

	base = ioremap_nocache(0x322a0000, 0x150);
	if (IS_ERR(base)) {
		pr_err("ioremap top dvfs address failed\n");
		return -EFAULT;
	}
	regmap_ctx.top_base = (unsigned long)base;

	apsys_dvfs_class_init();
	apsys_dvfs_device_create(apsys, dev);

	ret = sysfs_create_group(&(apsys->dev.kobj), &apsys_group);
	if (ret) {
		dev_err(dev, "apsys create sysfs class failed, ret=%d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, apsys);

	if (apsys->dvfs_ops && apsys->dvfs_ops->dvfs_init)
		apsys->dvfs_ops->dvfs_init(apsys);

	return 0;
}
static int apsys_dvfs_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id apsys_dvfs_of_match[] = {
	{ .compatible = "sprd,hwdvfs-apsys-sharkl5",
	  .data = (void *)"sharkl5" },
	{ },
};

MODULE_DEVICE_TABLE(of, apsys_dvfs_of_match);

static struct platform_driver apsys_dvfs_driver = {
	.probe	= apsys_dvfs_probe,
	.remove	= apsys_dvfs_remove,
	.driver = {
		.name = "apsys-dvfs",
		.of_match_table = apsys_dvfs_of_match,
	},
};

static int __init apsys_dvfs_register(void)
{
	return platform_driver_register(&apsys_dvfs_driver);
}

static void __exit apsys_dvfs_unregister(void)
{
	platform_driver_unregister(&apsys_dvfs_driver);
}

subsys_initcall(apsys_dvfs_register);
module_exit(apsys_dvfs_unregister);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("sprd apsys dvfs driver");
MODULE_AUTHOR("Kevin Tang <kevin.tang@unisoc.com>");
