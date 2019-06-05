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
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/property.h>

#include "pcie-designware.h"
#include "pcie-sprd.h"

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

#ifdef CONFIG_SPRD_IPA_INTC
static void sprd_pcie_fix_interrupt_line(struct pci_dev *dev)
{
	struct pcie_port *pp = dev->bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct platform_device *pdev = to_platform_device(pci->dev);
	struct sprd_pcie *ctrl = platform_get_drvdata(pdev);

	if (dev->hdr_type == PCI_HEADER_TYPE_NORMAL) {
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE,
				      ctrl->interrupt_line);
		dev_info(&dev->dev,
			 "The pci legacy interrupt pin is set to: %lu\n",
			 (unsigned long)ctrl->interrupt_line);
	}
}
DECLARE_PCI_FIXUP_EARLY(PCI_ANY_ID, PCI_ANY_ID, sprd_pcie_fix_interrupt_line);
#endif

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
	dw_pcie_setup_ep(ep);
}

static int sprd_pcie_ep_raise_irq(struct dw_pcie_ep *ep,
				     enum pci_epc_irq_type type,
				     u8 interrupt_num)
{
	struct dw_pcie *pci = to_dw_pcie_from_ep(ep);

	switch (type) {
	case PCI_EPC_IRQ_LEGACY:
		/* TODO*/
		break;
	case  PCI_EPC_IRQ_MSI:
		return dw_pcie_ep_raise_msi_irq(ep, interrupt_num);
	default:
		dev_err(pci->dev, "UNKNOWN IRQ type\n");
	}

	return 0;
}

static struct dw_pcie_ep_ops pcie_ep_ops = {
	.ep_init = sprd_pcie_ep_init,
	.raise_irq = sprd_pcie_ep_raise_irq,
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

/*
 * WAKE# (low active)from endpoint to wake up AP.
 *
 * When AP is in deep state, an endpoint can wakeup AP by pulling the wake
 * signal to low. After AP is activated, the endpoint must pull the wake signal
 * to high.
 */
static irqreturn_t sprd_pcie_wakeup_irq(int irq, void *data)
{
	struct sprd_pcie *sprd_pcie = data;
	int value = gpiod_get_value(sprd_pcie->gpiod_wakeup);
	u32 irq_flags = irq_get_trigger_type(irq);

	if (!value) {
		irq_flags &= ~IRQF_TRIGGER_LOW;
		irq_flags |= IRQF_TRIGGER_HIGH;
	} else {
		irq_flags &= ~IRQF_TRIGGER_HIGH;
		irq_flags |= IRQF_TRIGGER_LOW;
	}

	irq_set_irq_type(irq, irq_flags);

	return IRQ_HANDLED;
}

static int sprd_add_pcie_port(struct dw_pcie *pci, struct platform_device *pdev)
{
	struct sprd_pcie *sprd_pcie;
	struct pcie_port *pp;
	struct device *dev = &pdev->dev;
	struct fwnode_handle *child;
	int ret;
	unsigned int irq;
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dbi");
	pci->dbi_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!pci->dbi_base)
		return -ENOMEM;

	pp = &pci->pp;
	pp->ops = &sprd_pcie_host_ops;

	dw_pcie_writel_dbi(pci, PCIE_SS_REG_BASE + PE0_GEN_CTRL_3,
			   LTSSM_EN | L1_AUXCLK_EN);

	sprd_pcie = platform_get_drvdata(to_platform_device(pci->dev));

	device_for_each_child_node(dev, child) {
		if (fwnode_property_read_string(child, "label",
						&sprd_pcie->label)) {
			dev_err(dev, "without interrupt property\n");
			fwnode_handle_put(child);
			return -EINVAL;
		}
		if (!strcmp(sprd_pcie->label, "parent_gic_intc")) {
			irq = irq_of_parse_and_map(to_of_node(child), 0);
			if (irq < 0) {
				dev_err(dev, "cannot get msi irq\n");
				return irq;
			}

			pp->msi_irq = (int)irq;
			ret = devm_request_irq(dev, pp->msi_irq,
					       sprd_pcie_msi_irq_handler,
					       IRQF_SHARED | IRQF_NO_THREAD,
					       "sprd-pcie-msi", pp);
			if (ret) {
				dev_err(dev, "cannot request msi irq\n");
				return ret;
			}
		}

#ifdef CONFIG_SPRD_IPA_INTC
		if (!strcmp(sprd_pcie->label, "parent_ipa_intc")) {
			irq = irq_of_parse_and_map(to_of_node(child), 0);
			if (irq < 0) {
				dev_err(dev, "cannot get legacy irq\n");
				return irq;
			}
			sprd_pcie->interrupt_line = irq;
		}
#endif
	}

	sprd_pcie->gpiod_wakeup =
		devm_gpiod_get_index(dev, "pcie-wakeup", 0, GPIOD_IN);
	if (IS_ERR(sprd_pcie->gpiod_wakeup)) {
		dev_warn(dev, "Please set pcie-wakeup gpio in DTS\n");
		goto no_wakeup;
	}

	sprd_pcie->wakeup_irq = gpiod_to_irq(sprd_pcie->gpiod_wakeup);
	if (sprd_pcie->wakeup_irq < 0) {
		dev_warn(dev, "cannot get wakeup irq\n");
		goto no_wakeup;
	}

	snprintf(sprd_pcie->wakeup_label, sprd_pcie->label_len,
		 "%s wakeup", dev_name(dev));
	ret = devm_request_threaded_irq(dev, sprd_pcie->wakeup_irq,
					sprd_pcie_wakeup_irq, NULL,
					IRQF_TRIGGER_LOW,
					sprd_pcie->wakeup_label, sprd_pcie);
	if (ret < 0)
		dev_warn(dev, "cannot request wakeup irq\n");

no_wakeup:

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
	size_t len = strlen(dev_name(dev)) + 10;
	const struct sprd_pcie_of_data *data;
	enum dw_pcie_device_mode mode;
	/* Dirty: must be deleted. Only for marlin3 driver temperarily */
	static int probe_defer_count;

	data = (struct sprd_pcie_of_data *)of_device_get_match_data(dev);
	mode = data->mode;

	/*
	 *  Dirty:
	 *	These codes must be delete atfer marlin3 EP power-on sequence
	 *	is okay.
	 *  There two device type of the PCIe controller: RC and EP.
	 *  -1. As RC + marlin3 PCIe EP:
	 *	Marlin3 power on and init is too late. Before establishing PCIe
	 *	link we must wait, wait... If marlin3 PCIe power on sequence is
	 *	nice, we will remove these dirty codes.
	 *  -2. As RC + ORCA PCIe EP:
	 *	Because orca EP power on in uboot, if the probe() continue, PCIe
	 *	will establish link. It's not nesseary to defer probe in this
	 *	situation. However, it's harmless to defer probe.
	 *  -3. As EP: This controller is selected to EP mode for ORCA:
	 *	It must run earlier than RC. So it can't be probed.
	 */
	if (mode == DW_PCIE_RC_TYPE) {
		if ((probe_defer_count++) < 10)
			return -EPROBE_DEFER;
		dev_info(dev, "%s: defer probe %d times to wait wcn\n",
			 __func__, probe_defer_count);
	}

	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-startup-syscons");
	if (ret < 0) {
		dev_err(dev, "get pcie syscons fail, return %d\n", ret);
		return ret;
	}

	sprd_pcie = devm_kzalloc(dev, sizeof(*sprd_pcie) + len, GFP_KERNEL);
	if (!sprd_pcie)
		return -ENOMEM;

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (!pci)
		return -ENOMEM;

	pci->dev = dev;
	pci->ops = &dw_pcie_ops;
	sprd_pcie->pci = pci;
	sprd_pcie->label_len = len;

	platform_set_drvdata(pdev, sprd_pcie);

	switch (mode) {
	case DW_PCIE_RC_TYPE:
		ret = sprd_add_pcie_port(pci, pdev);
		if (ret) {
			dev_err(dev, "cannot initialize rc host\n");
			return ret;
		}
		ret = sprd_pcie_syscon_setting(pdev,
					       "sprd,pcie-aspml1p2-syscons");
		if (ret < 0)
			dev_err(&pdev->dev, "get pcie aspml1.2 syscons fail\n");
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

static int sprd_pcie_suspend_noirq(struct device *dev)
{
	int ret;
	struct platform_device *pdev = to_platform_device(dev);
	struct sprd_pcie *sprd_pcie = platform_get_drvdata(pdev);
	struct dw_pcie *pci = sprd_pcie->pci;

	sprd_pcie_save_dwc_reg(pci);

	ret = sprd_pcie_enter_pcipm_l2(pci);
	if (ret < 0)
		dev_warn(dev, "note: RC doesn't enter l2\n");

	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-suspend-syscons");
	if (ret < 0) {
		dev_err(dev, "set pcie suspend syscons fail, return %d\n", ret);
		return ret;
	}

	return 0;
}

static int sprd_pcie_resume_noirq(struct device *dev)
{
	int ret;
	struct platform_device *pdev = to_platform_device(dev);
	struct sprd_pcie *sprd_pcie = platform_get_drvdata(pdev);
	struct dw_pcie *pci = sprd_pcie->pci;
	struct pcie_port *pp = &pci->pp;

	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-resume-syscons");
	if (ret < 0) {
		dev_err(dev, "set pcie resume syscons fail, return %d\n", ret);
		return ret;
	}

	dw_pcie_writel_dbi(pci, PCIE_SS_REG_BASE + PE0_GEN_CTRL_3,
			   LTSSM_EN | L1_AUXCLK_EN);

	dw_pcie_setup_rc(pp);
	ret = dw_pcie_wait_for_link(pci);
	if (ret < 0) {
		dev_err(dev, "resume fail, pcie can't establish link\n");
		return ret;
	}

	sprd_pcie_restore_dwc_reg(pci);

	ret = sprd_pcie_syscon_setting(pdev, "sprd,pcie-aspml1p2-syscons");
	if (ret < 0)
		dev_err(&pdev->dev, "get pcie aspml1.2 syscons fail\n");

	return 0;
}

static const struct dev_pm_ops sprd_pcie_pm_ops = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(sprd_pcie_suspend_noirq,
				      sprd_pcie_resume_noirq)
};

static struct platform_driver sprd_pcie_driver = {
	.probe = sprd_pcie_probe,
	.driver = {
		.name = "sprd-pcie",
		.suppress_bind_attrs = true,
		.of_match_table = sprd_pcie_of_match,
		.pm	= &sprd_pcie_pm_ops,
	},
};

builtin_platform_driver(sprd_pcie_driver);
