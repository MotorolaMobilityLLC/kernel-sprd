//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 */
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>

struct pmic_refout {
	unsigned int regsw;
	unsigned int refnum;
	struct regmap *regmap;
};

static struct pmic_refout *sc27xx_refout = NULL;

static ssize_t pmic_refout_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned int value, mask;
	int ret;

	if (sc27xx_refout == NULL)
		return sprintf(buf, "The pmic refout driver is not init\n");

	ret = regmap_read(sc27xx_refout->regmap, sc27xx_refout->regsw, &value);
	if (ret) {
		pr_err("Unable to get refout\n");
		return ret;
	}

	mask = (1 << sc27xx_refout->refnum) - 1;

	return sprintf(buf, "0x%x\n", value & mask);
}
static DEVICE_ATTR_RO(pmic_refout);

static struct attribute *pmic_refout_attrs[] = {
	&dev_attr_pmic_refout.attr,
	NULL
};
ATTRIBUTE_GROUPS(pmic_refout);

int pmic_refout_update(unsigned int refout_num, int refout_state)
{
	int ret;

	if ((sc27xx_refout == NULL) || (refout_num >= sc27xx_refout->refnum)) {
		pr_warn("Pmic driver is not init yet or ref num is out\n");
		return -EINVAL;
	}

	if (!refout_state)
		ret = regmap_update_bits(sc27xx_refout->regmap, sc27xx_refout->regsw,
				 1 << refout_num, 0);
	else if (refout_state == 1)
		ret = regmap_update_bits(sc27xx_refout->regmap, sc27xx_refout->regsw,
				 1 << refout_num, 1 << refout_num);
	else {
		pr_err("Invalid state(%d)\n", refout_state);
		ret = -EINVAL;
	}

	return ret;
}
EXPORT_SYMBOL(pmic_refout_update);

static int sprd_pmic_refout_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	int ret;

	sc27xx_refout = devm_kzalloc(dev, sizeof(struct pmic_refout), GFP_KERNEL);
	if (!sc27xx_refout)
		return -ENOMEM;

	sc27xx_refout->regmap = dev_get_regmap(dev->parent, NULL);
	if (!sc27xx_refout->regmap) {
		pr_err("Get regmap fail\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(np, "regsw", 0, &sc27xx_refout->regsw);
	if (ret) {
		pr_err("Get base register failed\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(np, "refnum", 0, &sc27xx_refout->refnum);
	if (ret) {
		pr_err("Get refout num failed\n");
		return -EINVAL;
	}

	ret = sysfs_create_groups(&dev->kobj, pmic_refout_groups);
	if (ret)
		pr_warn("Failed to create pmic_syscon attributes\n");

	return 0;
}

static int sprd_pmic_refout_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	sysfs_remove_groups(&dev->kobj, pmic_refout_groups);

	return 0;
}

static const struct of_device_id sprd_pmic_refout_match[] = {
	{.compatible = "sprd,sc27xx-refout"},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_pmic_refout_match);

static struct platform_driver sprd_pmic_refout_driver = {
	.probe = sprd_pmic_refout_probe,
	.remove = sprd_pmic_refout_remove,
	.driver = {
		.name = "sprd-pmic-refout",
		.of_match_table = sprd_pmic_refout_match,
	},
};
module_platform_driver(sprd_pmic_refout_driver);

MODULE_DESCRIPTION("UNISOC pmic refout driver");
MODULE_LICENSE("GPL v2");
