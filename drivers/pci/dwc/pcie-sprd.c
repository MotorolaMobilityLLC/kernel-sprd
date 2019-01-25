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
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "pcie-designware.h"
#include "pcie-sprd.h"

static int sprd_pcie_get_syscon_reg(struct platform_device *pdev,
	struct syscon_pcie *reg, const char *name)
{
	struct regmap *regmap;
	u32 syscon_args[2];
	int ret;
	struct device_node *np = pdev->dev.of_node;

	regmap = syscon_regmap_lookup_by_name(pdev->dev.of_node, name);
	if (IS_ERR(regmap)) {
		dev_err(&pdev->dev, "%s lookup %s failed\n", __func__, name);
		reg->regmap = NULL;
		reg->reg = 0x0;
		reg->mask = 0x0;
		return -EINVAL;
	}

	ret = syscon_get_args_by_name(np, name, 2, syscon_args);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s get args %s failed\n", __func__, name);
		return ret;
	} else if (ret != 2) {
		dev_err(&pdev->dev, "%s the args numbers of %s is not 2\n",
			__func__, name);
		return -EINVAL;
	}
	reg->regmap = regmap;
	reg->reg = syscon_args[0];
	reg->mask = syscon_args[1];

	return 0;
}

void sprd_pcie_get_syscon_info(struct platform_device *pdev,
			      struct sprd_pcie *ctrl)
{
	sprd_pcie_get_syscon_reg(pdev, &ctrl->pcie2_eb,
				 "pcie2_eb");
	sprd_pcie_get_syscon_reg(pdev, &ctrl->pcieh_frc_on,
				 "pcieh_frc_on");
	sprd_pcie_get_syscon_reg(pdev, &ctrl->pciev_frc_on,
				 "pciev_frc_on");
	sprd_pcie_get_syscon_reg(pdev, &ctrl->pcie2_frc_wakeup,
				 "pcie2_frc_wakeup");
	sprd_pcie_get_syscon_reg(pdev, &ctrl->pcie2_perst,
				 "pcie2_perst");
	sprd_pcie_get_syscon_reg(pdev, &ctrl->pcie2_phy_pwron,
				 "pcie2_phy_pwron");
	sprd_pcie_get_syscon_reg(pdev, &ctrl->ipa_sys_dly,
				 "ipa_sys_dly");
	sprd_pcie_get_syscon_reg(pdev, &ctrl->pciepllh_pd,
				 "pciepllh_pd");
	sprd_pcie_get_syscon_reg(pdev, &ctrl->pciepllh_divn,
				 "pciepllh_divn");
	sprd_pcie_get_syscon_reg(pdev, &ctrl->pciepllh_diff_or_sign_sel,
				 "pciepllh_diff_or_sign_sel");
	sprd_pcie_get_syscon_reg(pdev, &ctrl->pciepllh_kdelta,
				 "pciepllh_kdelta");
	sprd_pcie_get_syscon_reg(pdev, &ctrl->pciepllh_reserved,
				 "pciepllh_reserved");
	sprd_pcie_get_syscon_reg(pdev, &ctrl->pciepllh_cp_en,
				 "pciepllh_cp_en");
	sprd_pcie_get_syscon_reg(pdev, &ctrl->pciepllh_icp,
				 "pciepllh_icp");
	sprd_pcie_get_syscon_reg(pdev, &ctrl->pciepllh_ldo_trim,
				 "pciepllh_ldo_trim");
	sprd_pcie_get_syscon_reg(pdev, &ctrl->pcie2_phy_sw_en,
				 "pcie2_phy_sw_en");
}

/* Dirty: must be improved. Only for bringup test */
void sprd_pcie_init_syscon_reg(struct platform_device *pdev,
			       struct sprd_pcie *ctrl)
{
	unsigned int val;
	struct device *dev = &pdev->dev;

	sprd_pcie_get_syscon_info(pdev, ctrl);

	regmap_update_bits(ctrl->pcie2_eb.regmap,
			   ctrl->pcie2_eb.reg,
			   ctrl->pcie2_eb.mask,
			   ctrl->pcie2_eb.mask);
	regmap_read(ctrl->pcie2_eb.regmap, ctrl->pcie2_eb.reg, &val);
	dev_info(dev, "billows: pcie2_eb: [0x21040004]: 0x%x\n", val);

	regmap_update_bits(ctrl->pcie2_frc_wakeup.regmap,
			   ctrl->pcie2_frc_wakeup.reg,
			   ctrl->pcie2_frc_wakeup.mask,
			   ctrl->pcie2_frc_wakeup.mask);
	regmap_read(ctrl->pcie2_frc_wakeup.regmap,
		    ctrl->pcie2_frc_wakeup.reg, &val);
	dev_info(dev, "billows: pcie2_frc_wakeup: [0x3228025c]: 0x%x\n", val);

	regmap_update_bits(ctrl->ipa_sys_dly.regmap,
			   ctrl->ipa_sys_dly.reg,
			   ctrl->ipa_sys_dly.mask,
			   0xc6006);
	regmap_read(ctrl->ipa_sys_dly.regmap, ctrl->ipa_sys_dly.reg, &val);
	dev_info(dev, "billows: ipa_sys_dly: [0x32280538]: 0x%x\n",
		 val);

	regmap_update_bits(ctrl->pciepllh_diff_or_sign_sel.regmap,
			   ctrl->pciepllh_diff_or_sign_sel.reg,
			   ctrl->pciepllh_diff_or_sign_sel.mask,
			   0);
	regmap_read(ctrl->pciepllh_diff_or_sign_sel.regmap,
		    ctrl->pciepllh_diff_or_sign_sel.reg, &val);
	dev_info(dev, "billows: pciepllh_diff_or_sign_sel: [0x32404000]:0x%x\n",
		 val);

	regmap_update_bits(ctrl->pcie2_phy_pwron.regmap,
			   ctrl->pcie2_phy_pwron.reg,
			   ctrl->pcie2_phy_pwron.mask,
			   ctrl->pcie2_phy_pwron.mask);
	regmap_read(ctrl->pcie2_phy_pwron.regmap,
		    ctrl->pcie2_phy_pwron.reg, &val);
	dev_info(dev, "billows: pcie2_phy_pwron: [0x322804bc]: 0x%x\n", val);

	regmap_read(ctrl->pcie2_phy_sw_en.regmap,
		    ctrl->pcie2_phy_sw_en.reg, &val);
	val &= (~(0x1 << 2));
	regmap_write(ctrl->pcie2_phy_sw_en.regmap,
		     ctrl->pcie2_phy_sw_en.reg, val);

	regmap_read(ctrl->pcie2_phy_sw_en.regmap,
		    ctrl->pcie2_phy_sw_en.reg, &val);
	dev_info(dev, "billows5: pcie2_phy_sw_en2: [0x23424004]: 0x%x\n", val);

	regmap_update_bits(ctrl->pcie2_eb.regmap,
			   ctrl->pcie2_eb.reg,
			   ctrl->pcie2_eb.mask,
			   ctrl->pcie2_eb.mask);
	regmap_read(ctrl->pcie2_eb.regmap, ctrl->pcie2_eb.reg, &val);
	dev_info(dev, "billows6: pcie2_eb: [0x21040004]: 0x%x\n", val);

	regmap_update_bits(ctrl->pcieh_frc_on.regmap,
			   ctrl->pcieh_frc_on.reg,
			   ctrl->pcieh_frc_on.mask,
			   ctrl->pcieh_frc_on.mask);
	regmap_read(ctrl->pcieh_frc_on.regmap, ctrl->pcieh_frc_on.reg, &val);
	dev_info(dev, "billows: pcieh_frc_on11: [0x322800f0]: 0x%x\n", val);

	regmap_update_bits(ctrl->pciev_frc_on.regmap,
			   ctrl->pciev_frc_on.reg,
			   ctrl->pciev_frc_on.mask,
			   ctrl->pciev_frc_on.mask);
	regmap_read(ctrl->pciev_frc_on.regmap, ctrl->pciev_frc_on.reg, &val);
	dev_info(dev, "billows: pciev_frc_on12: [0x322800f4]: 0x%x\n", val);

	regmap_update_bits(ctrl->pcie2_perst.regmap,
			   ctrl->pcie2_perst.reg,
			   ctrl->pcie2_perst.mask,
			   0x1);
	regmap_read(ctrl->pcie2_perst.regmap, ctrl->pcie2_perst.reg, &val);
	dev_info(dev, "billows: pcie2_perst: [0x322803cc]: 0x%x\n", val);

	regmap_update_bits(ctrl->pcie2_frc_wakeup.regmap,
			   ctrl->pcie2_frc_wakeup.reg,
			   ctrl->pcie2_frc_wakeup.mask,
			   ctrl->pcie2_frc_wakeup.mask);
	regmap_read(ctrl->pcie2_frc_wakeup.regmap,
		    ctrl->pcie2_frc_wakeup.reg, &val);
	dev_info(dev, "billows: pcie2_frc_wakeup: [0x3228025c]: 0x%x\n", val);

	regmap_update_bits(ctrl->pciepllh_diff_or_sign_sel.regmap,
			   ctrl->pciepllh_diff_or_sign_sel.reg,
			   ctrl->pciepllh_diff_or_sign_sel.mask,
			   ctrl->pciepllh_diff_or_sign_sel.mask);
	regmap_read(ctrl->pciepllh_diff_or_sign_sel.regmap,
		    ctrl->pciepllh_diff_or_sign_sel.reg, &val);
	val |= (0x1f << 9);
	regmap_write(ctrl->pciepllh_divn.regmap, ctrl->pciepllh_divn.reg, val);
	regmap_read(ctrl->pciepllh_diff_or_sign_sel.regmap,
		    ctrl->pciepllh_diff_or_sign_sel.reg, &val);
	dev_info(dev, "billows: pciepllh_diff_or_sign_sel: [0x32404000]:0x%x\n",
		 val);

	regmap_update_bits(ctrl->pciepllh_pd.regmap,
			   ctrl->pciepllh_pd.reg,
			   ctrl->pciepllh_pd.mask,
			   0);
	regmap_read(ctrl->pciepllh_pd.regmap, ctrl->pciepllh_pd.reg, &val);
	dev_info(dev, "billows: pciepllh_pd: [0x32404000]:0x%x\n", val);

	regmap_update_bits(ctrl->pcie2_perst.regmap,
			   ctrl->pcie2_perst.reg,
			   ctrl->pcie2_perst.mask,
			   0x2);
	regmap_read(ctrl->pcie2_perst.regmap, ctrl->pcie2_perst.reg, &val);
	dev_info(dev, "billows17: pcie2_perst: [0x322803cc]: 0x%x\n", val);
	msleep(20);
	regmap_update_bits(ctrl->pcie2_perst.regmap,
			   ctrl->pcie2_perst.reg,
			   ctrl->pcie2_perst.mask,
			   0x1);
	regmap_read(ctrl->pcie2_perst.regmap, ctrl->pcie2_perst.reg, &val);
	dev_info(dev, "billows18: pcie2_perst: [0x322803cc]: 0x%x\n", val);
}

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

	dw_pcie_writel_dbi(pci, PCIE_SS_REG_BASE + PE0_GEN_CTRL_3,
			   LTSSM_EN | L1_AUXCLK_EN);

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
	/* Dirty: must be deleted. Only for wcn driver temperarily */
	static int probe_defer_count;

	if ((probe_defer_count++) < 10)
		return -EPROBE_DEFER;

	dev_info(dev, "%s: defer probe %d times to wait wcn\n",
		 __func__, probe_defer_count);

	data = (struct sprd_pcie_of_data *)of_device_get_match_data(dev);
	mode = data->mode;

	sprd_pcie = devm_kzalloc(dev, sizeof(*sprd_pcie), GFP_KERNEL);
	if (!sprd_pcie)
		return -ENOMEM;

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (!pci)
		return -ENOMEM;

	sprd_pcie_init_syscon_reg(pdev, sprd_pcie);

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
