//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 UNISOC Communications Inc.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include "../../base/base.h"

struct pmic_glb {
	u32 reg;
	u32 base;
	struct kobject *kobj;
	struct regmap *regmap;
	struct device *dev;
	struct list_head list;
};

static LIST_HEAD(sc27xx_head);
static struct platform_driver sprd_pmic_glb_driver;

static ssize_t pmic_reg_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	ssize_t ret = -EINVAL;
	struct pmic_glb *sc27xx_glb;

	list_for_each_entry(sc27xx_glb, &sc27xx_head, list) {
		if (sc27xx_glb->kobj == kobj)
			return sprintf(buf, "0x%x", sc27xx_glb->reg);
	}
	return ret;
}

static ssize_t pmic_reg_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	int ret = -EINVAL;
	struct pmic_glb *sc27xx_glb;

	list_for_each_entry(sc27xx_glb, &sc27xx_head, list) {
		if (sc27xx_glb->kobj == kobj) {
			ret = sscanf(buf, "%x", &sc27xx_glb->reg);
			if (ret != 1)
				return -EINVAL;

			return strnlen(buf, count);
		}
	}
	return ret;
}

static ssize_t pmic_value_show(struct kobject *kobj, struct kobj_attribute
			       *attr, char *buf)
{
	int ret = -EINVAL;
	u32 value;
	struct pmic_glb *sc27xx_glb;

	list_for_each_entry(sc27xx_glb, &sc27xx_head, list) {
		if (sc27xx_glb->kobj == kobj) {
			if (sc27xx_glb->reg < sc27xx_glb->base)
				return ret;

			ret = regmap_read(sc27xx_glb->regmap, sc27xx_glb->reg, &value);
			if (ret)
				return ret;

			return sprintf(buf, "%x", value);
		}
	}
	return ret;
}

static ssize_t pmic_value_store(struct kobject *kobj, struct kobj_attribute
				*attr, const char *buf, size_t count)
{
	int ret = -EINVAL;
	u32 value;
	struct pmic_glb *sc27xx_glb;

	list_for_each_entry(sc27xx_glb, &sc27xx_head, list) {
		if (sc27xx_glb->kobj == kobj) {
			ret = sscanf(buf, "%x", &value);
			if (ret != 1)
				return -EINVAL;

			if (sc27xx_glb->reg < sc27xx_glb->base)
				return -EINVAL;

			ret = regmap_write(sc27xx_glb->regmap, sc27xx_glb->reg, value);
			if (ret)
				return ret;

			return count;
		}
	}

	return ret;
}

static struct kobj_attribute pmic_reg_attr =
__ATTR(pmic_reg, 0644, pmic_reg_show, pmic_reg_store);
static struct kobj_attribute pmic_value_attr =
__ATTR(pmic_value, 0644, pmic_value_show, pmic_value_store);

static struct attribute *pmic_syscon_attrs[] = {
	&pmic_reg_attr.attr,
	&pmic_value_attr.attr,
	NULL
};

static const struct attribute_group pmic_syscon_group = {
	.attrs = pmic_syscon_attrs,
};

static int sprd_pmic_glb_probe(struct platform_device *pdev)
{
	int ret;
	struct pmic_glb *sc27xx_glb;
	struct kobject *sprd_pmic_glb_kobj;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct device_driver *drv = &sprd_pmic_glb_driver.driver;
	const struct of_device_id *match;

	sc27xx_glb = devm_kzalloc(dev, sizeof(struct pmic_glb), GFP_KERNEL);
	if (!sc27xx_glb)
		return -ENOMEM;

	sc27xx_glb->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!sc27xx_glb->regmap) {
		dev_err(dev, "get regmap fail\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(np, "reg", 0, &sc27xx_glb->base);
	if (ret) {
		dev_err(dev, "get base register failed\n");
		return -EINVAL;
	}

	sc27xx_glb->dev = &pdev->dev;

	match = of_match_device(pdev->dev.driver->of_match_table, dev);
	if (!match)
		return -EINVAL;

	sprd_pmic_glb_kobj = kobject_create_and_add(match->compatible, &drv->p->kobj);
	if (sprd_pmic_glb_kobj == NULL) {
		ret = -ENOMEM;
		pr_err("%s register sysfs failed. ret %d\n", __func__, ret);
		return ret;
	}
	ret = sysfs_create_group(sprd_pmic_glb_kobj, &pmic_syscon_group);
	if (ret) {
		dev_warn(dev, "failed to create pmic_syscon attributes\n");
		kobject_put(sprd_pmic_glb_kobj);
	}

	sc27xx_glb->kobj = sprd_pmic_glb_kobj;

	list_add(&sc27xx_glb->list, &sc27xx_head);

	dev_set_drvdata(dev, sc27xx_glb);

	return 0;
}

static int sprd_pmic_glb_remove(struct platform_device *pdev)
{
	struct pmic_glb *sc27xx_glb;

	list_for_each_entry(sc27xx_glb, &sc27xx_head, list) {
		list_del(&sc27xx_glb->list);
		sysfs_remove_group(&sc27xx_glb->dev->kobj, &pmic_syscon_group);
	}
	return 0;
}
static const struct of_device_id sprd_pmic_glb_match[] = {
	{ .compatible = "sprd,sc27xx-syscon"},
	{ .compatible = "sprd,ump962x-syscon"},
	{ .compatible = "sprd,ump9621-syscon"},
	{ .compatible = "sprd,ump9622-syscon"},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_pmic_glb_match);

static struct platform_driver sprd_pmic_glb_driver = {
	.probe = sprd_pmic_glb_probe,
	.remove = sprd_pmic_glb_remove,
	.driver = {
		.name = "sprd-pmic-glb",
		.of_match_table = sprd_pmic_glb_match,
	},
};

module_platform_driver(sprd_pmic_glb_driver);

MODULE_AUTHOR("Luyao Wu <luyao.wu@unisoc.com>");
MODULE_DESCRIPTION("UNISOC pmic glob driver");
MODULE_LICENSE("GPL v2");
