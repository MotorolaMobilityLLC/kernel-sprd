// SPDX-License-Identifier: GPL-2.0
/*
 * PCIe host controller driver for Spreadtrum SoCs
 *
 * Copyright (C) 2020 Spreadtrum corporation. http://www.unisoc.com
 *
 * Author: Billows Wu <Billows.Wu@unisoc.com>
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/property.h>
#include <linux/regmap.h>

#include "pcie-designware.h"

#define NUM_OF_ARGS 5

struct sprd_pcie {
	struct dw_pcie *pci;
};

struct sprd_pcie_of_data {
	enum dw_pcie_device_mode mode;
};

static int sprd_pcie_establish_link(struct dw_pcie *pci)
{
	return 0;
}

static const struct dw_pcie_ops sprd_pcie_ops = {
	.start_link = sprd_pcie_establish_link,
};

int sprd_pcie_syscon_setting(struct platform_device *pdev, char *env)
{
	struct device_node *np = pdev->dev.of_node;
	int i, count, err;
	u32 type, delay, reg, mask, val, tmp_val;
	struct of_phandle_args out_args;
	struct regmap *iomap;
	struct device *dev = &pdev->dev;

	if (!of_find_property(np, env, NULL)) {
		dev_info(dev, "There isn't property %s in dts\n", env);
		return 0;
	}

	count = of_property_count_elems_of_size(np, env,
				(NUM_OF_ARGS + 1) * sizeof(u32));
	dev_info(dev, "Property (%s) reg count is %d :\n", env, count);

	for (i = 0; i < count; i++) {
		err = of_parse_phandle_with_fixed_args(np, env, NUM_OF_ARGS,
						       i, &out_args);
		if (err < 0)
			return err;

		type = out_args.args[0];
		delay = out_args.args[1];
		reg = out_args.args[2];
		mask = out_args.args[3];
		val = out_args.args[4];

		iomap = syscon_node_to_regmap(out_args.np);

		switch (type) {
		case 0:
			regmap_update_bits(iomap, reg, mask, val);
			break;

		case 1:
			regmap_read(iomap, reg, &tmp_val);
			tmp_val &= (~mask);
			tmp_val |= (val & mask);
			regmap_write(iomap, reg, tmp_val);
			break;
		default:
			break;
		}

		if (delay)
			usleep_range(delay, delay + 10);

		regmap_read(iomap, reg, &tmp_val);
		dev_dbg(&pdev->dev,
			"%2d:reg[0x%8x] mask[0x%8x] val[0x%8x] result[0x%8x]\n",
			i, reg, mask, val, tmp_val);
	}

	return i;
}

static void sprd_pcie_perst_assert(struct platform_device *pdev)
{
	sprd_pcie_syscon_setting(pdev, "sprd,pcie-perst-assert");
}

static void sprd_pcie_perst_deassert(struct platform_device *pdev)
{
	sprd_pcie_syscon_setting(pdev, "sprd,pcie-perst-deassert");
}

static int sprd_pcie_host_shutdown(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;

	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-shutdown-syscons");
	if (ret < 0)
		dev_err(dev,
			"Failed to set pcie shutdown syscons, return %d\n",
			ret);

	sprd_pcie_perst_assert(pdev);

	ret = pm_runtime_put(&pdev->dev);
	if (ret < 0)
		dev_warn(&pdev->dev,
			 "Failed to put runtime,return %d\n", ret);

	return ret;
}

static int sprd_pcie_host_init(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct platform_device *pdev = to_platform_device(pci->dev);

	sprd_pcie_perst_deassert(pdev);

	dw_pcie_setup_rc(pp);

	if (IS_ENABLED(CONFIG_PCI_MSI))
		dw_pcie_msi_init(pp);

	if (dw_pcie_wait_for_link(pci)) {
		dev_warn(pci->dev,
			 "pcie ep may has not been powered on yet\n");
		sprd_pcie_host_shutdown(pdev);
	}

	return 0;
}

static const struct dw_pcie_host_ops sprd_pcie_host_ops = {
	.host_init = sprd_pcie_host_init,
};

static int sprd_add_pcie_port(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);
	struct dw_pcie *pci = ctrl->pci;
	struct pcie_port *pp = &pci->pp;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	if (!res)
		return -EINVAL;

	pci->dbi_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!pci->dbi_base)
		return -ENOMEM;

	pp->ops = &sprd_pcie_host_ops;

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		pp->msi_irq = platform_get_irq_byname(pdev, "msi");
		if (pp->msi_irq < 0) {
			dev_err(dev, "Failed to get msi, return %d\n",
				pp->msi_irq);
			return pp->msi_irq;
		}
	}

	return dw_pcie_host_init(pp);
}

static int sprd_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dw_pcie *pci;
	struct sprd_pcie *ctrl;
	int ret;

	ctrl = devm_kzalloc(dev, sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (!pci)
		return -ENOMEM;

	pci->dev = dev;
	pci->ops = &sprd_pcie_ops;
	ctrl->pci = pci;

	platform_set_drvdata(pdev, ctrl);

	pm_runtime_enable(dev);
	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		dev_err(dev, "Fialed to get runtime sync, return %d\n", ret);
		goto err_get_sync;
	}

	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-startup-syscons");
	if (ret < 0) {
		dev_err(dev, "Failed to get pcie syscons, return %d\n", ret);
		goto err_power_off;
	}

	ret = sprd_add_pcie_port(pdev);
	if (ret)
		dev_warn(dev, "Failed to initialize RC controller\n");

	return 0;

err_power_off:
	sprd_pcie_syscon_setting(pdev, "sprd,pcie-shutdown-syscons");

err_get_sync:
	pm_runtime_put(&pdev->dev);
	pm_runtime_disable(dev);

	return ret;
}

static const struct of_device_id sprd_pcie_of_match[] = {
	{
		.compatible = "sprd,pcie",
	},
	{},
};

static struct platform_driver sprd_pcie_driver = {
	.probe = sprd_pcie_probe,
	.driver = {
		.name = "sprd-pcie",
		.of_match_table = sprd_pcie_of_match,
	},
};

module_platform_driver(sprd_pcie_driver);

MODULE_DESCRIPTION("Spreadtrum PCIe host controller driver");
MODULE_LICENSE("GPL v2");
