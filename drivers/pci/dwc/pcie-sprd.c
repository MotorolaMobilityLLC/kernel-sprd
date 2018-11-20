/*
 * PCIe host controller driver for Spreadtrum SoCs
 *
 * Copyright (C) 2018-2019 Spreadtrum corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include "pcie-designware.h"

struct sprd_pcie {
	struct dw_pcie *pci;
	struct pcie_port pp;
	struct clk *pcie_eb;
};

struct sprd_pcie_of_data {
	enum dw_pcie_device_mode mode;
};

static void sprd_pcie_fix_class(struct pci_dev *dev)
{
	struct pcie_port *pp = dev->bus->sysdata;

	if (dev->class != PCI_CLASS_NOT_DEFINED)
		return;

	if (dev->bus->number == pp->root_bus_nr)
		dev->class = 0x0604 << 8;
	else
		dev->class = 0x080d << 8;

	dev_info(&dev->dev,
		 "%s: The class of device %04x:%04x is changed to: 0x%06x\n",
		 __func__, dev->device, dev->vendor, dev->class);
}
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_SYNOPSYS, 0xabcd, sprd_pcie_fix_class);

static irqreturn_t sprd_pcie_msi_irq_handler(int irq, void *arg)
{
	struct pcie_port *pp = arg;

	return dw_handle_msi_irq(pp);
}

static void sprd_pcie_assert_reset(struct pcie_port *pp)
{
	/* TODO */
}

static int sprd_pcie_host_init(struct pcie_port *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);

	sprd_pcie_assert_reset(pp);
	dw_pcie_setup_rc(pp);

	if (IS_ENABLED(CONFIG_PCI_MSI))
		dw_pcie_msi_init(pp);

	return dw_pcie_wait_for_link(pci);
}

static const struct dw_pcie_host_ops sprd_pcie_host_ops = {
	.host_init = sprd_pcie_host_init,
};

static void sprd_pcie_ep_init(struct dw_pcie_ep *ep)
{
	/* TODO*/
}

static struct dw_pcie_ep_ops pcie_ep_ops = {
	.ep_init = sprd_pcie_ep_init,
};

static int sprd_add_pcie_ep(struct sprd_pcie *sprd_pcie,
			    struct platform_device *pdev)
{
	int ret;
	struct dw_pcie_ep *ep;
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct dw_pcie *pci = sprd_pcie->pci;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	pci->dbi_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!pci->dbi_base)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi2");
	pci->dbi_base2 = devm_ioremap(dev, res->start, resource_size(res));
	if (!pci->dbi_base2)
		return -ENOMEM;

	ep = &pci->ep;
	ep->ops = &pcie_ep_ops;
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "addr_space");
	if (!res) {
		dev_err(dev, "pci can't get addr space\n");
		return -EINVAL;
	}

	ep->phys_base = res->start;
	ep->addr_size = resource_size(res);

	ret = dw_pcie_ep_init(ep);
	if (ret) {
		dev_err(dev, "failed to initialize endpoint\n");
		return ret;
	}

	return 0;
}

static int sprd_add_pcie_port(struct dw_pcie *pci, struct platform_device *pdev)
{
	struct pcie_port *pp;
	struct device *dev = &pdev->dev;
	int ret;
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	pci->dbi_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!pci->dbi_base)
		return -ENOMEM;

	pp = &pci->pp;
	pp->ops = &sprd_pcie_host_ops;

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		pp->msi_irq = platform_get_irq(pdev, 0);
		if (pp->msi_irq < 0) {
			dev_err(dev, "cannot get msi irq\n");
			return pp->msi_irq;
		}

		ret = devm_request_irq(dev, pp->msi_irq,
				       sprd_pcie_msi_irq_handler,
				       IRQF_SHARED | IRQF_NO_THREAD,
				       "sprd-pcie-msi", pp);
		if (ret) {
			dev_err(dev, "cannot request msi irq\n");
			return ret;
		}
	}

	return dw_pcie_host_init(&pci->pp);
}

static const struct sprd_pcie_of_data sprd_pcie_rc_of_data = {
	.mode = DW_PCIE_RC_TYPE,
};

static const struct sprd_pcie_of_data sprd_pcie_ep_of_data = {
	.mode = DW_PCIE_EP_TYPE,
};

static const struct of_device_id sprd_pcie_of_match[] = {
	{
		.compatible = "sprd,pcie",
		.data = &sprd_pcie_rc_of_data,
	},
	{
		.compatible = "sprd,pcie-ep",
		.data = &sprd_pcie_ep_of_data,
	},
	{},
};

static int sprd_pcie_establish_link(struct dw_pcie *pci)
{
	/* TODO */
	return 0;
}

static void sprd_pcie_stop_link(struct dw_pcie *pci)
{
	/* TODO */
}

static const struct dw_pcie_ops dw_pcie_ops = {
	.start_link = sprd_pcie_establish_link,
	.stop_link = sprd_pcie_stop_link,
};

static int sprd_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dw_pcie *pci;
	struct sprd_pcie *sprd_pcie;
	int ret;
	const struct sprd_pcie_of_data *data;
	enum dw_pcie_device_mode mode;

	data = (struct sprd_pcie_of_data *)of_device_get_match_data(dev);
	mode = data->mode;

	sprd_pcie = devm_kzalloc(dev, sizeof(*sprd_pcie), GFP_KERNEL);
	if (!sprd_pcie)
		return -ENOMEM;

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (!pci)
		return -ENOMEM;

	pci->dev = dev;
	pci->ops = &dw_pcie_ops;
	sprd_pcie->pci = pci;

	platform_set_drvdata(pdev, sprd_pcie);

	switch (mode) {
	case DW_PCIE_RC_TYPE:
		ret = sprd_add_pcie_port(pci, pdev);
		if (ret) {
			dev_err(dev, "cannot initialize rc host\n");
			return ret;
		}
		break;
	case DW_PCIE_EP_TYPE:
		ret = sprd_add_pcie_ep(sprd_pcie, pdev);
		if (ret) {
			dev_err(dev, "cannot initialize ep host\n");
			return ret;
		}
		break;
	default:
		dev_err(dev, "INVALID device type %d\n", mode);
		return -EINVAL;
	}

	return 0;
}

static struct platform_driver sprd_pcie_driver = {
	.probe = sprd_pcie_probe,
	.driver = {
		.name = "sprd-pcie",
		.suppress_bind_attrs = true,
		.of_match_table = sprd_pcie_of_match,
	},
};
builtin_platform_driver(sprd_pcie_driver);
