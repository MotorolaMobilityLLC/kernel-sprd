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

#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/time.h>

#include "ufshcd.h"
#include "ufshcd-pltfrm.h"
#include "ufshci.h"
#include "ufs-sprd.h"
#include "ufs_quirks.h"
#include "unipro.h"

static int ufs_sprd_get_syscon_reg(struct device_node *np,
	struct syscon_ufs *reg, const char *name)
{
	struct regmap *regmap;
	u32 syscon_args[2];
	int ret;

	regmap = syscon_regmap_lookup_by_name(np, name);
	if (IS_ERR(regmap)) {
		pr_err("read ufs syscon %s regmap fail\n", name);
		reg->regmap = NULL;
		reg->reg = 0x0;
		reg->mask = 0x0;
		return -EINVAL;
	}

	ret = syscon_get_args_by_name(np, name, 2, syscon_args);
	if (ret < 0)
		return ret;
	else if (ret != 2) {
		pr_err("read ufs syscon %s fail,ret = %d\n", name, ret);
		return -EINVAL;
	}
	reg->regmap = regmap;
	reg->reg = syscon_args[0];
	reg->mask = syscon_args[1];

	return 0;
}

void ufs_sprd_reset(struct ufs_sprd_host *host)
{
	int val = 0;

	dev_info(host->hba->dev, "ufs hardware reset!\n");

	/* TODO: Temporary codes. Ufs reset will be simple in next IP version */
	regmap_update_bits(host->anlg_mphy_ufs_rst.regmap,
			   host->anlg_mphy_ufs_rst.reg,
			   host->anlg_mphy_ufs_rst.mask,
			   0);
	msleep(100);
	regmap_update_bits(host->anlg_mphy_ufs_rst.regmap,
			   host->anlg_mphy_ufs_rst.reg,
			   host->anlg_mphy_ufs_rst.mask,
			   host->anlg_mphy_ufs_rst.mask);

	val = readl(host->unipro_reg + 0x3c);
	writel(0x35000000 | val, host->unipro_reg + 0x3c);
	msleep(100);
	writel((~0x35000000) & val, host->unipro_reg + 0x3c);

	val = readl(host->unipro_reg + 0x40);
	writel(1 | val, host->unipro_reg + 0x40);
	msleep(100);
	writel((~1) & val, host->unipro_reg + 0x40);

	val = readl(host->ufsutp_reg + 0x100);
	writel(3 | val, host->ufsutp_reg + 0x100);
	msleep(100);
	writel((~3) & val, host->ufsutp_reg + 0x100);

	val = readl(host->hba->mmio_base + 0xb0);
	writel(0x10001000 | val, host->hba->mmio_base + 0xb0);
	msleep(100);
	writel((~0x10001000) & val, host->hba->mmio_base + 0xb0);

	regmap_update_bits(host->aon_apb_ufs_rst.regmap,
			   host->aon_apb_ufs_rst.reg,
			   host->aon_apb_ufs_rst.mask,
			   host->aon_apb_ufs_rst.mask);
	msleep(100);
	regmap_update_bits(host->aon_apb_ufs_rst.regmap,
			   host->aon_apb_ufs_rst.reg,
			   host->aon_apb_ufs_rst.mask,
			   0);

	val = readl(host->unipro_reg + 0x84);
	writel(2 | val, host->unipro_reg + 0x84);
	msleep(100);
	writel((~2) & val, host->unipro_reg + 0x84);

	val = readl(host->unipro_reg + 0xc0);
	writel(0x10 | val, host->unipro_reg + 0xc0);
	msleep(100);
	writel((~0x10) & val, host->unipro_reg + 0xc0);

	val = readl(host->unipro_reg + 0xd0);
	writel(4 | val, host->unipro_reg + 0xd0);
	msleep(100);
	writel((~4) & val, host->unipro_reg + 0xd0);

	regmap_update_bits(host->ap_apb_ufs_rst.regmap,
			   host->ap_apb_ufs_rst.reg,
			   host->ap_apb_ufs_rst.mask,
			   host->ap_apb_ufs_rst.mask);
	msleep(100);
	regmap_update_bits(host->ap_apb_ufs_rst.regmap,
			   host->ap_apb_ufs_rst.reg,
			   host->ap_apb_ufs_rst.mask,
			   0);

	val = readl(host->ufs_ao_reg + 0x1c);
	writel(2 | val, host->ufs_ao_reg + 0x1c);
	msleep(100);
	writel((~2) & val, host->ufs_ao_reg + 0x1c);
}

static int ufs_sprd_init(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct ufs_sprd_host *host;
	struct resource *res;
	int ret = 0;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	host->hba = hba;
	ufshcd_set_variant(hba, host);

	/* map ufsutp_reg */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ufsutp_reg");
	if (!res) {
		dev_err(dev, "Missing ufs utp register resource\n");
		return -ENODEV;
	}
	host->ufsutp_reg = devm_ioremap_nocache(dev, res->start,
						resource_size(res));
	if (IS_ERR(host->ufsutp_reg)) {
		dev_err(dev, "%s: could not map ufsutp_reg, err %ld\n",
			__func__, PTR_ERR(host->ufsutp_reg));
		host->ufsutp_reg = NULL;
		return -ENODEV;
	}
	pr_info("ufsutp_reg vit=0x%llx, phy=0x%llx, len=0x%llx\n",
		(u64) res->start,
		(u64) host->ufsutp_reg,
		(u64) resource_size(res));

	/* map unipro_reg */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "unipro_reg");
	if (!res) {
		dev_err(dev, "Missing unipro register resource\n");
		return -ENODEV;
	}
	host->unipro_reg = devm_ioremap_nocache(dev, res->start,
						resource_size(res));
	if (IS_ERR(host->unipro_reg)) {
		dev_err(dev, "%s: could not map unipro_reg, err %ld\n",
			__func__, PTR_ERR(host->unipro_reg));
		host->unipro_reg = NULL;
		return -ENODEV;
	}
	pr_info("unipro_reg vit=0x%llx, phy=0x%llx, len=0x%llx\n",
		(u64) res->start,
		(u64) host->unipro_reg,
		(u64) resource_size(res));

	/* map ufs_ao_reg */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ufs_ao_reg");
	if (!res) {
		dev_err(dev, "Missing ufs_ao_reg register resource\n");
		return -ENODEV;
	}
	host->ufs_ao_reg = devm_ioremap_nocache(dev, res->start,
						resource_size(res));
	if (IS_ERR(host->ufs_ao_reg)) {
		dev_err(dev, "%s: could not map ufs_ao_reg, err %ld\n",
			__func__, PTR_ERR(host->ufs_ao_reg));
		host->ufs_ao_reg = NULL;
		return -ENODEV;
	}
	pr_info("ufs_ao_reg vit=0x%llx, phy=0x%llx, len=0x%llx\n",
		(u64) res->start,
		(u64) host->ufs_ao_reg,
		(u64) resource_size(res));

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->aon_apb_ufs_en,
				      "aon_apb_ufs_en");
	if (ret < 0)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->ap_apb_ufs_en,
				      "ap_apb_ufs_en");
	if (ret < 0)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->ap_apb_ufs_rst,
				      "ap_apb_ufs_rst");
	if (ret < 0)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->anlg_mphy_ufs_rst,
				      "anlg_mphy_ufs_rst");
	if (ret < 0)
		return -ENODEV;

	ret = ufs_sprd_get_syscon_reg(dev->of_node, &host->aon_apb_ufs_rst,
				      "aon_apb_ufs_rst");
	if (ret < 0)
		return -ENODEV;

	hba->quirks |= UFSHCD_QUIRK_BROKEN_UFS_HCI_VERSION;
	hba->quirks |= UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS;

	regmap_update_bits(host->ap_apb_ufs_en.regmap,
			   host->ap_apb_ufs_en.reg,
			   host->ap_apb_ufs_en.mask,
			   host->ap_apb_ufs_en.mask);

	regmap_update_bits(host->aon_apb_ufs_en.regmap,
			   host->aon_apb_ufs_en.reg,
			   host->aon_apb_ufs_en.mask,
			   host->aon_apb_ufs_en.mask);

	ufs_sprd_reset(host);

	return 0;
}

static void ufs_sprd_exit(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	devm_kfree(dev, host);
}

static u32 ufs_sprd_get_ufs_hci_version(struct ufs_hba *hba)
{
	return UFSHCI_VERSION_21;
}

static int ufs_sprd_hce_enable_notify(struct ufs_hba *hba,
				      enum ufs_notify_change_status status)
{
	int err = 0;

	switch (status) {
	case PRE_CHANGE:
		break;
	case POST_CHANGE:
		break;
	default:
		dev_err(hba->dev, "%s: invalid status %d\n", __func__, status);
		err = -EINVAL;
		break;
	}

	return err;
}

static int ufs_sprd_link_startup_notify(struct ufs_hba *hba,
					enum ufs_notify_change_status status)
{
	int err = 0;

	switch (status) {
	case PRE_CHANGE:
		/*
		 * Some UFS devices (and may be host) have issues if LCC is
		 * enabled. So we are setting PA_Local_TX_LCC_Enable to 0
		 * before link startup which will make sure that both host
		 * and device TX LCC are disabled once link startup is
		 * completed.
		 */
		if (ufshcd_get_local_unipro_ver(hba) != UFS_UNIPRO_VER_1_41)
			err = ufshcd_dme_set(hba,
					UIC_ARG_MIB(PA_LOCAL_TX_LCC_ENABLE),
					0);

		break;
	case POST_CHANGE:
		break;
	default:
		break;
	}

	return err;
}

static int ufs_sprd_pwr_change_notify(struct ufs_hba *hba,
				enum ufs_notify_change_status status,
				struct ufs_pa_layer_attr *dev_max_params,
				struct ufs_pa_layer_attr *dev_req_params)
{
	int err = 0;

	if (!dev_req_params) {
		pr_err("%s: incoming dev_req_params is NULL\n", __func__);
		err = -EINVAL;
		goto out;
	}

	dev_req_params->gear_rx = UFS_PWM_G3;
	dev_req_params->gear_tx = UFS_PWM_G3;
	dev_req_params->lane_rx = 1;
	dev_req_params->lane_tx = 1;
	dev_req_params->pwr_rx = FASTAUTO_MODE;
	dev_req_params->pwr_tx = FASTAUTO_MODE;
	dev_req_params->hs_rate = 0;

	switch (status) {
	case PRE_CHANGE:
		break;
	case POST_CHANGE:
		break;
	default:
		err = -EINVAL;
		break;
	}

out:
	return err;
}

/**
 * struct ufs_hba_sprd_vops - UFS sprd specific variant operations
 *
 * The variant operations configure the necessary controller and PHY
 * handshake during initialization.
 */
static struct ufs_hba_variant_ops ufs_hba_sprd_vops = {
	.name = "sprd",
	.init = ufs_sprd_init,
	.exit = ufs_sprd_exit,
	.get_ufs_hci_version = ufs_sprd_get_ufs_hci_version,
	.hce_enable_notify = ufs_sprd_hce_enable_notify,
	.link_startup_notify = ufs_sprd_link_startup_notify,
	.pwr_change_notify = ufs_sprd_pwr_change_notify,
};

/**
 * ufs_sprd_probe - probe routine of the driver
 * @pdev: pointer to Platform device handle
 *
 * Return zero for success and non-zero for failure
 */
static int ufs_sprd_probe(struct platform_device *pdev)
{
	int err;
	struct device *dev = &pdev->dev;

	/* Perform generic probe */
	err = ufshcd_pltfrm_init(pdev, &ufs_hba_sprd_vops);
	if (err)
		dev_err(dev, "ufshcd_pltfrm_init() failed %d\n", err);

	return err;
}

/**
 * ufs_sprd_remove - set driver_data of the device to NULL
 * @pdev: pointer to platform device handle
 *
 * Always returns 0
 */
static int ufs_sprd_remove(struct platform_device *pdev)
{
	struct ufs_hba *hba =  platform_get_drvdata(pdev);

	pm_runtime_get_sync(&(pdev)->dev);
	ufshcd_remove(hba);
	return 0;
}

static const struct of_device_id ufs_sprd_of_match[] = {
	{ .compatible = "sprd,ufshc"},
	{},
};

static const struct dev_pm_ops ufs_sprd_pm_ops = {
	.suspend = ufshcd_pltfrm_suspend,
	.resume = ufshcd_pltfrm_resume,
	.runtime_suspend = ufshcd_pltfrm_runtime_suspend,
	.runtime_resume = ufshcd_pltfrm_runtime_resume,
	.runtime_idle = ufshcd_pltfrm_runtime_idle,
};

static struct platform_driver ufs_sprd_pltform = {
	.probe = ufs_sprd_probe,
	.remove = ufs_sprd_remove,
	.shutdown = ufshcd_pltfrm_shutdown,
	.driver = {
		.name = "ufshcd-sprd",
		.pm = &ufs_sprd_pm_ops,
		.of_match_table = of_match_ptr(ufs_sprd_of_match),
	},
};
module_platform_driver(ufs_sprd_pltform);

MODULE_DESCRIPTION("SPRD Specific UFSHCI driver");
MODULE_LICENSE("GPL v2");
