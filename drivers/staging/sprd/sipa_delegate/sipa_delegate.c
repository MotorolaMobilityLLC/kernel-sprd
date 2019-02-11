/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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

#include <linux/sipa.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include "../sipa/sipa_hal_priv.h"
#include "sipa_delegate.h"


#define DRV_NAME "sipa_delegate"

struct sipa_delegate_plat_drv_cfg {
	phys_addr_t remote_base;
	phys_addr_t remote_end;
	phys_addr_t mapped_base;
	phys_addr_t mapped_end;
	u32 ul_fifo_depth;
	u32 dl_fifo_depth;
};

struct modem_sipa_delegator {
	struct device *pdev;

	phys_addr_t reg_ori_start;
	phys_addr_t reg_ori_end;
	phys_addr_t reg_mapped_start;
	phys_addr_t reg_mapped_end;
	u32 ul_fifo_depth;
	u32 dl_fifo_depth;

	dma_addr_t ul_free_fifo_phy;
	dma_addr_t ul_filled_fifo_phy;
	dma_addr_t dl_free_fifo_phy;
	dma_addr_t dl_filled_fifo_phy;

	u8 *ul_free_fifo_virt;
	u8 *ul_filled_fifo_virt;
	u8 *dl_free_fifo_virt;
	u8 *dl_filled_fifo_virt;
};

static struct sipa_delegate_plat_drv_cfg s_sipa_dele_cfg;
static struct modem_sipa_delegator *s_modem_sipa_delegator;

int modem_sipa_connect(struct sipa_to_pam_info *out)
{
	if (!s_modem_sipa_delegator)
		return -ENODEV;

	out->term = SIPA_TERM_PCIE0;
	out->dl_fifo.rx_fifo_base_addr = DL_RX_FIFO_BASE_ADDR;
	out->dl_fifo.tx_fifo_base_addr = DL_TX_FIFO_BASE_ADDR;
	out->dl_fifo.fifo_sts_addr = s_modem_sipa_delegator->reg_mapped_start +
				     ((SIPA_FIFO_PCIE_DL + 1) * SIPA_FIFO_REG_SIZE);

	out->ul_fifo.rx_fifo_base_addr = UL_RX_FIFO_BASE_ADDR;
	out->ul_fifo.tx_fifo_base_addr = UL_TX_FIFO_BASE_ADDR;

	out->ul_fifo.fifo_sts_addr = s_modem_sipa_delegator->reg_mapped_start +
				     ((SIPA_FIFO_PCIE_UL + 1) * SIPA_FIFO_REG_SIZE);

	return 0;
}
EXPORT_SYMBOL(modem_sipa_connect);

static int sipa_dele_parse_dts_cfg(
	struct platform_device *pdev,
	struct sipa_delegate_plat_drv_cfg *cfg)
{
	int ret;
	struct resource *resource;

	/* get modem IPA global register base  address */
	resource = platform_get_resource_byname(pdev,
						IORESOURCE_MEM,
						"remote-base");
	if (!resource) {
		dev_err(&pdev->dev, "get resource failed for remote-base!\n");
		return -ENODEV;
	}
	cfg->remote_base = resource->start;
	cfg->remote_end = resource->end;

	/* get mapped modem IPA global register base  address */
	resource = platform_get_resource_byname(pdev,
						IORESOURCE_MEM,
						"mapped-base");
	if (!resource) {
		dev_err(&pdev->dev, "get resource failed for mapped-base!\n");
		return -ENODEV;
	}
	cfg->mapped_base = resource->start;
	cfg->mapped_end = resource->end;

	/* get ul fifo depth */
	ret = of_property_read_u32(pdev->dev.of_node,
				   "sprd,ul-fifo-depth",
				   &cfg->ul_fifo_depth);
	if (ret) {
		dev_err(&pdev->dev, "get resource failed for ul_fifo_depth\n");
		return ret;
	} else
		pr_debug("%s : using ul_fifo_depth = %d", __func__,
			 cfg->ul_fifo_depth);

	/* get dl fifo depth */
	ret = of_property_read_u32(pdev->dev.of_node,
				   "sprd,dl-fifo-depth",
				   &cfg->dl_fifo_depth);
	if (ret) {
		dev_err(&pdev->dev, "get resource failed for dl_fifo_depth\n");
		return ret;
	} else
		dev_err(&pdev->dev, "using dl_fifo_depth = %d", cfg->dl_fifo_depth);

	return 0;
}

static int sipa_dele_init(struct modem_sipa_delegator **delegator_pp,
			  struct sipa_delegate_plat_drv_cfg *cfg,
			  struct device *dev)
{
	struct modem_sipa_delegator *delegator;

	delegator = devm_kzalloc(dev, sizeof(*delegator), GFP_KERNEL);
	if (!delegator) {
		pr_err("%s: alloc sipa_dele_init err.\n", __func__);
		return -ENOMEM;
	}

	delegator->pdev = dev;
	delegator->reg_ori_start = cfg->remote_base;
	delegator->reg_ori_end = cfg->remote_end;
	delegator->reg_mapped_start = cfg->mapped_base;
	delegator->reg_mapped_end = cfg->mapped_end;
	delegator->ul_fifo_depth = cfg->ul_fifo_depth;
	delegator->dl_fifo_depth = cfg->dl_fifo_depth;
	/* ul_free_fifo */
	delegator->ul_free_fifo_virt = devm_kzalloc(dev,
					       sizeof(struct sipa_node_description_tag) *
					       delegator->ul_fifo_depth,
					       GFP_KERNEL);
	if (!delegator->ul_free_fifo_virt) {
		dev_err(dev, "alloc ul_free_fifo_virt err.\n");
		return -ENOMEM;
	}
	delegator->ul_free_fifo_phy = dma_map_single(dev,
				      delegator->ul_free_fifo_virt,
				      sizeof(struct sipa_node_description_tag) *
				      delegator->ul_fifo_depth,
				      DMA_BIDIRECTIONAL);
	if (dma_mapping_error(dev, delegator->ul_free_fifo_phy)) {
		dev_err(dev, "dma map ul_free_fifo_phy err.\n");
		return -ENOMEM;
	}

	/* ul_filled_fifo */
	delegator->ul_filled_fifo_virt = devm_kzalloc(dev,
			sizeof(struct sipa_node_description_tag) *
			delegator->ul_fifo_depth,
			GFP_KERNEL);
	if (!delegator->ul_filled_fifo_virt) {
		dev_err(dev, "alloc ul_filled_fifo_virt err.\n");
		return -ENOMEM;
	}
	delegator->ul_filled_fifo_phy = dma_map_single(dev,
					delegator->ul_filled_fifo_virt,
					sizeof(struct sipa_node_description_tag) *
					delegator->ul_fifo_depth,
					DMA_BIDIRECTIONAL);
	if (dma_mapping_error(dev, delegator->ul_filled_fifo_phy)) {
		dev_err(dev, "dma map ul_filled_fifo_phy err.\n");
		return -ENOMEM;
	}
	/* dl_free_fifo */
	delegator->dl_free_fifo_virt = devm_kzalloc(dev,
					       sizeof(struct sipa_node_description_tag) *
					       delegator->dl_fifo_depth,
					       GFP_KERNEL);
	if (!delegator->dl_free_fifo_virt) {
		dev_err(dev, " alloc dl_free_fifo_virt err.\n");
		return -ENOMEM;
	}
	delegator->dl_free_fifo_phy = dma_map_single(dev,
				      delegator->dl_free_fifo_virt,
				      sizeof(struct sipa_node_description_tag) *
				      delegator->dl_fifo_depth,
				      DMA_BIDIRECTIONAL);
	if (dma_mapping_error(dev, delegator->dl_free_fifo_phy)) {
		dev_err(dev, "dma map dl_free_fifo_phy err.\n");
		return -ENOMEM;
	}
	/* dl_filled_fifo */
	delegator->dl_filled_fifo_virt = devm_kzalloc(dev,
			sizeof(struct sipa_node_description_tag) *
			delegator->dl_fifo_depth,
			GFP_KERNEL);
	if (!delegator->dl_filled_fifo_virt) {
		dev_err(dev, "alloc dl_filled_fifo_virt err.\n");
		return -ENOMEM;
	}
	delegator->dl_filled_fifo_phy = dma_map_single(dev,
					delegator->dl_filled_fifo_virt,
					sizeof(struct sipa_node_description_tag) *
					delegator->dl_fifo_depth,
					DMA_BIDIRECTIONAL);
	if (dma_mapping_error(dev, delegator->dl_filled_fifo_phy)) {
		dev_err(dev, "dma map dl_filled_fifo_phy err.\n");
		return -ENOMEM;
	}

	*delegator_pp = delegator;

	return 0;
}

static int sipa_dele_plat_drv_probe(struct platform_device *pdev_p)
{
	int ret;
	struct device *dev = &pdev_p->dev;
	struct sipa_delegate_plat_drv_cfg *cfg = &s_sipa_dele_cfg;

	memset(cfg, 0, sizeof(*cfg));

	ret = sipa_dele_parse_dts_cfg(pdev_p, cfg);
	if (ret) {
		dev_err(&pdev_p->dev, "dts parsing failed\n");
		return ret;
	}

	ret = sipa_dele_init(&s_modem_sipa_delegator, cfg, dev);
	if (ret) {
		dev_err(&pdev_p->dev, "sipa_dele_init failed: %d\n", ret);
		return ret;
	}

	return ret;
}

static struct of_device_id sipa_dele_plat_drv_match[] = {
	{ .compatible = "sprd,sipa-delegate", },
	{}
};

/**
 * sipa_dele_ap_suspend() - suspend callback for runtime_pm
 * @dev: pointer to device
 *
 * This callback will be invoked by the runtime_pm framework when an AP suspend
 * operation is invoked.
 *
 * Returns -EAGAIN to runtime_pm framework in case IPA is in use by AP.
 * This will postpone the suspend operation until IPA is no longer used by AP.
 */
static int sipa_dele_ap_suspend(struct device *dev)
{
	return 0;
}

/**
 * sipa_dele_ap_resume() - resume callback for runtime_pm
 * @dev: pointer to device
 *
 * This callback will be invoked by the runtime_pm framework when an AP resume
 * operation is invoked.
 *
 * Always returns 0 since resume should always succeed.
 */
static int sipa_dele_ap_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops sipa_dele_pm_ops = {
	.suspend_noirq = sipa_dele_ap_suspend,
	.resume_noirq = sipa_dele_ap_resume,
};

static struct platform_driver sipa_dele_plat_drv = {
	.probe = sipa_dele_plat_drv_probe,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &sipa_dele_pm_ops,
		.of_match_table = sipa_dele_plat_drv_match,
	},
};

static int __init sipa_dele_module_init(void)
{
	/* Register as a platform device driver */
	return platform_driver_register(&sipa_dele_plat_drv);
}

module_init(sipa_dele_module_init);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum IPA Delegate device driver");
