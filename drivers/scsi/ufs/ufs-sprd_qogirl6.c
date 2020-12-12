/*
 * Copyright (C) 2020 Unisoc Communications Inc.
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
#include "ufs-sprd_qogirl6.h"
#include "ufs_quirks.h"
#include "unipro.h"


int syscon_get_args(struct device_node *np, struct ufs_sprd_host *host)
{
	u32 args[2];
	int ret;

	host->aon_apb_ufs_en.regmap =
			syscon_regmap_lookup_by_name(np, "aon_apb_ufs_en");
	if (IS_ERR(host->aon_apb_ufs_en.regmap)) {
		pr_warn("failed to get apb ufs aon_apb_ufs_en\n");
		return PTR_ERR(host->aon_apb_ufs_en.regmap);
	}

	pr_info("fangkuiufs host->aon_apb_ufs_en.regmap = %p",
		host->aon_apb_ufs_en.regmap);
	ret = syscon_get_args_by_name(np, "aon_apb_ufs_en", 2, args);
	if (ret == 2) {
		host->aon_apb_ufs_en.reg = args[0];
		host->aon_apb_ufs_en.mask = args[1];
	} else {
		pr_err("failed to parse aon aph ufs en reg\n");
	}

	host->ap_apb_ufs_en.regmap =
			syscon_regmap_lookup_by_name(np, "ap_apb_ufs_en");
	if (IS_ERR(host->ap_apb_ufs_en.regmap)) {
		pr_err("failed to get apb ufs ap_apb_ufs_en\n");
		return PTR_ERR(host->ap_apb_ufs_en.regmap);
	}

	ret = syscon_get_args_by_name(np, "ap_apb_ufs_en", 2, args);
	if (ret == 2) {
		host->ap_apb_ufs_en.reg = args[0];
		host->ap_apb_ufs_en.mask = args[1];
	} else {
		pr_err("failed to parse ap_apb_ufs_en\n");
	}

	host->ap_apb_ufs_rst.regmap =
			syscon_regmap_lookup_by_name(np, "ap_apb_ufs_rst");
	if (IS_ERR(host->ap_apb_ufs_rst.regmap)) {
		pr_err("failed to get ap_apb_ufs_rst\n");
		return PTR_ERR(host->ap_apb_ufs_rst.regmap);
	}

	ret = syscon_get_args_by_name(np, "ap_apb_ufs_rst", 2, args);
	if (ret == 2) {
		host->ap_apb_ufs_rst.reg = args[0];
		host->ap_apb_ufs_rst.mask = args[1];
	} else {
		pr_err("failed to parse ap_apb_ufs_rst\n");
	}

	host->ufs_refclk_on.regmap =
			syscon_regmap_lookup_by_name(np, "ufs_refclk_on");
	if (IS_ERR(host->ufs_refclk_on.regmap)) {
		pr_warn("failed to get ufs_refclk_on\n");
		return PTR_ERR(host->ufs_refclk_on.regmap);
	}

	ret = syscon_get_args_by_name(np, "ufs_refclk_on", 2, args);
	if (ret == 2) {
		host->ufs_refclk_on.reg = args[0];
		host->ufs_refclk_on.mask = args[1];
	} else {
		pr_err("failed to parse ufs_refclk_on\n");
	}

	host->ahb_ufs_lp.regmap =
			syscon_regmap_lookup_by_name(np, "ahb_ufs_lp");
	if (IS_ERR(host->ahb_ufs_lp.regmap)) {
		pr_warn("failed to get ahb_ufs_lp\n");
		return PTR_ERR(host->ahb_ufs_lp.regmap);
	}

	ret = syscon_get_args_by_name(np, "ahb_ufs_lp", 2, args);
	if (ret == 2) {
		host->ahb_ufs_lp.reg = args[0];
		host->ahb_ufs_lp.mask = args[1];
	} else {
		pr_err("failed to parse ahb_ufs_lp\n");
	}

	host->ahb_ufs_force_isol.regmap =
			syscon_regmap_lookup_by_name(np, "ahb_ufs_force_isol");
	if (IS_ERR(host->ahb_ufs_force_isol.regmap)) {
		pr_err("failed to get ahb_ufs_force_isol 1\n");
		return PTR_ERR(host->ahb_ufs_force_isol.regmap);
	}

	ret = syscon_get_args_by_name(np, "ahb_ufs_force_isol", 2, args);
	if (ret == 2) {
		host->ahb_ufs_force_isol.reg = args[0];
		host->ahb_ufs_force_isol.mask = args[1];
	} else {
		pr_err("failed to parse ahb_ufs_force_isol\n");
	}

	host->ahb_ufs_cb.regmap =
			syscon_regmap_lookup_by_name(np, "ahb_ufs_cb");
	if (IS_ERR(host->ahb_ufs_cb.regmap)) {
		pr_err("failed to get ahb_ufs_cb\n");
		return PTR_ERR(host->ahb_ufs_cb.regmap);
	}

	ret = syscon_get_args_by_name(np, "ahb_ufs_cb", 2, args);
	if (ret == 2) {
		host->ahb_ufs_cb.reg = args[0];
		host->ahb_ufs_cb.mask = args[1];
	} else {
		pr_err("failed to parse ahb_ufs_cb\n");
	}

	return 0;
}

/*
 * ufs_sprd_rmwl - read modify write into a register
 * @base - base address
 * @mask - mask to apply on read value
 * @val - actual value to write
 * @reg - register address
 */
static inline void ufs_sprd_rmwl(void __iomem *base, u32 mask, u32 val, u32 reg)
{
	u32 tmp;

	tmp = readl((base) + (reg));
	tmp &= ~mask;
	tmp |= (val & mask);
	writel(tmp, (base) + (reg));
}

void ufs_sprd_reset(struct ufs_sprd_host *host)
{

	unsigned int value = 0;

	dev_info(host->hba->dev, "ufs hardware reset!\n");
	/* TODO: HW reset will be simple in next version. */
	/* Configs need strict squence. */

	regmap_read(host->ap_apb_ufs_rst.regmap,
		    host->ap_apb_ufs_rst.reg, &value);
	value = value | host->ap_apb_ufs_en.mask;
	regmap_write(host->ap_apb_ufs_rst.regmap,
		    host->ap_apb_ufs_rst.reg, value);
	mdelay(10);
	regmap_read(host->ap_apb_ufs_rst.regmap,
		    host->ap_apb_ufs_rst.reg, &value);
	value = value & (~host->ap_apb_ufs_en.mask);
	regmap_write(host->ap_apb_ufs_rst.regmap,
		     host->ap_apb_ufs_rst.reg, value);

	ufs_sprd_rmwl(host->ufs_analog_reg, FIFO_ENABLE_MASK,
		      FIFO_ENABLE_MASK, MPHY_LANE0_FIFO);

	ufs_sprd_rmwl(host->ufs_analog_reg, FIFO_ENABLE_MASK,
		      FIFO_ENABLE_MASK, MPHY_LANE1_FIFO);

	regmap_read(host->ufs_refclk_on.regmap,
		    host->ufs_refclk_on.reg, &value);
	value = value | host->ufs_refclk_on.mask;
	regmap_write(host->ufs_refclk_on.regmap,
		     host->ufs_refclk_on.reg, value);

	regmap_read(host->ahb_ufs_lp.regmap, host->ahb_ufs_lp.reg, &value);
	value = value & (~host->ahb_ufs_lp.mask);
	regmap_write(host->ahb_ufs_lp.regmap, host->ahb_ufs_lp.reg, value);

	regmap_read(host->ahb_ufs_cb.regmap, host->ahb_ufs_cb.reg, &value);
	value = value | host->ahb_ufs_cb.mask;
	regmap_write(host->ahb_ufs_cb.regmap, host->ahb_ufs_cb.reg, value);

	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_2T2R_APB_RESETN,
		      0, MPHY_2T2R_APB_REG1);
	mdelay(1);
	ufs_sprd_rmwl(host->ufs_analog_reg, MPHY_2T2R_APB_RESETN,
		      MPHY_2T2R_APB_RESETN, MPHY_2T2R_APB_REG1);

	regmap_read(host->ahb_ufs_cb.regmap, host->ahb_ufs_cb.reg, &value);
	value = value & (~host->ahb_ufs_cb.mask);
	regmap_write(host->ahb_ufs_cb.regmap, host->ahb_ufs_cb.reg, value);
}

/*
 * ufs_sprd_init - find other essential mmio bases
 * @hba: host controller instance
 * Returns 0 on success, non-zero value on failure
 */
static int ufs_sprd_init(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct ufs_sprd_host *host;
	struct resource *res;
	unsigned int value = 0;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	host->hba = hba;
	ufshcd_set_variant(hba, host);

	syscon_get_args(dev->of_node, host);

	hba->quirks |= UFSHCD_QUIRK_BROKEN_UFS_HCI_VERSION |
		       UFSHCD_QUIRK_DELAY_BEFORE_DME_CMDS;

	res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "ufs_analog_reg");
	if (!res) {
		dev_err(dev, "Missing ufs_analog_reg register resource\n");
		return -ENODEV;
	}
	host->ufs_analog_reg = devm_ioremap_nocache(dev, res->start,
	resource_size(res));
	if (IS_ERR(host->ufs_analog_reg)) {
		dev_err(dev, "%s: could not map ufs_analog_reg, err %ld\n",
			__func__, PTR_ERR(host->ufs_analog_reg));
		host->ufs_analog_reg = NULL;
		return -ENODEV;
	}

	regmap_read(host->ap_apb_ufs_en.regmap,
		    host->ap_apb_ufs_en.reg, &value);
	value =	value | host->ap_apb_ufs_en.mask;
	regmap_write(host->ap_apb_ufs_en.regmap,
		     host->ap_apb_ufs_en.reg, value);

	regmap_update_bits(host->aon_apb_ufs_en.regmap,
			   host->aon_apb_ufs_en.reg,
			   host->aon_apb_ufs_en.mask,
			   host->aon_apb_ufs_en.mask);

	ufs_sprd_reset(host);

	return 0;
}

/*
 * ufs_sprd_hw_init - controller enable and reset
 * @hba: host controller instance
 */
void ufs_sprd_hw_init(struct ufs_hba *hba)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	ufs_sprd_reset(host);
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
		/* Do hardware reset before host controller enable. */
		ufs_sprd_hw_init(hba);
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
		ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TACTIVATE), 0x10);
		break;
	default:
		err = -EINVAL;
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

	switch (status) {
	case PRE_CHANGE:
		dev_req_params->gear_rx = UFS_HS_G3;
		dev_req_params->gear_tx = UFS_HS_G3;
		dev_req_params->lane_rx = 2;
		dev_req_params->lane_tx = 2;
		dev_req_params->pwr_rx = FAST_MODE;
		dev_req_params->pwr_tx = FAST_MODE;
		dev_req_params->hs_rate = PA_HS_MODE_B;
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

static int ufs_sprd_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	hba->rpm_lvl = UFS_PM_LVL_1;
	hba->spm_lvl = UFS_PM_LVL_5;
	hba->uic_link_state = UIC_LINK_OFF_STATE;
	return 0;
}

static int ufs_sprd_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	return 0;
}

/*
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
	.suspend = ufs_sprd_suspend,
	.resume = ufs_sprd_resume,
};

/*
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
/*
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

MODULE_DESCRIPTION("Unisoc Specific UFSHCI driver");
MODULE_LICENSE("GPL v2");
