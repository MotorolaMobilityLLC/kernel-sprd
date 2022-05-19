// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS Host Controller driver for Unisoc specific extensions
 *
 * Copyright (C) 2022 Unisoc, Inc.
 *
 */

#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <trace/hooks/ufshcd.h>

#include "ufshcd.h"
#include "ufshcd-pltfrm.h"
#include "ufs-sprd.h"
#include "ufs-sprd-ioctl.h"

static int ufs_sprd_plat_parse_dt(struct device *dev, struct ufs_hba *hba,
				  struct ufs_sprd_host *host)
{
	if (host->comm_vops && host->comm_vops->parse_dt)
		return host->comm_vops->parse_dt(dev, hba, host);
	return 0;
}

static int ufs_sprd_plat_pre_init(struct device *dev, struct ufs_hba *hba,
				  struct ufs_sprd_host *host)
{
	if (host->comm_vops && host->comm_vops->pre_init)
		return host->comm_vops->pre_init(dev, hba, host);
	return 0;
}

static void ufs_sprd_plat_exit(struct device *dev, struct ufs_hba *hba,
			       struct ufs_sprd_host *host)
{
	if (host->comm_vops && host->comm_vops->exit_notify)
		return host->comm_vops->exit_notify(dev, hba, host);
}

static u32 ufs_sprd_get_ufs_hci_version(struct ufs_hba *hba)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	if (host->comm_vops && host->comm_vops->get_ufs_hci_ver)
		return host->comm_vops->get_ufs_hci_ver(hba);
	return 0;
}

static int ufs_sprd_plat_hce_enable_pre(struct ufs_hba *hba,
					struct ufs_sprd_host *host)
{
	if (host->comm_vops && host->comm_vops->hce_enable_pre_notify)
		return host->comm_vops->hce_enable_pre_notify(hba);
	return 0;
}

static int ufs_sprd_plat_hce_enable_post(struct ufs_hba *hba,
					 struct ufs_sprd_host *host)
{
	if (host->comm_vops && host->comm_vops->hce_enable_post_notify)
		return host->comm_vops->hce_enable_post_notify(hba);
	return 0;
}

static int ufs_sprd_plat_link_startup_pre(struct ufs_hba *hba,
					  struct ufs_sprd_host *host)
{
	if (host->comm_vops && host->comm_vops->link_startup_pre_notify)
		return host->comm_vops->link_startup_pre_notify(hba);
	return 0;
}

static void ufs_sprd_plat_h8_pre(struct ufs_hba *hba, enum uic_cmd_dme cmd,
				 struct ufs_sprd_host *host)
{
	if (host->comm_vops && host->comm_vops->hibern8_pre_notify)
		return host->comm_vops->hibern8_pre_notify(hba, cmd);
}

static void ufs_sprd_plat_h8_post(struct ufs_hba *hba, enum uic_cmd_dme cmd,
				  struct ufs_sprd_host *host)
{
	if (host->comm_vops && host->comm_vops->hibern8_post_notify)
		return host->comm_vops->hibern8_post_notify(hba, cmd);
}

static int ufs_sprd_apply_dev_quirks(struct ufs_hba *hba)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	if (host->comm_vops && host->comm_vops->apply_dev_quirks)
		return host->comm_vops->apply_dev_quirks(hba);
	return 0;
}

int ufs_sprd_get_syscon_reg(struct device_node *np, struct syscon_ufs *reg,
			    const char *name)
{
	struct regmap *regmap;
	u32 syscon_args[2];

	regmap = syscon_regmap_lookup_by_phandle_args(np, name, 2, syscon_args);
	if (IS_ERR(regmap)) {
		pr_err("read ufs syscon %s regmap fail\n", name);
		reg->regmap = NULL;
		reg->reg = 0x0;
		reg->mask = 0x0;
		return -EINVAL;
	}
	reg->regmap = regmap;
	reg->reg = syscon_args[0];
	reg->mask = syscon_args[1];

	return 0;
}

static int ufs_sprd_init(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct ufs_sprd_host *host;
	int ret = 0;

	host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
	if (!host)
		return -ENOMEM;

	host->hba = hba;
	ufshcd_set_variant(hba, host);

	ret = ufs_sprd_plat_priv_data_init(dev, hba, host);
	if (ret < 0)
		return ret;

	if (host->comm_vops) {
		hba->caps |= host->comm_vops->caps;
		hba->quirks |= host->comm_vops->quirks;
	}

	ret = ufs_sprd_plat_parse_dt(dev, hba, host);
	if (ret < 0)
		return ret;

	ret = ufs_sprd_plat_pre_init(dev, hba, host);
	if (ret < 0)
		return ret;

	hba->host->hostt->ioctl = ufshcd_sprd_ioctl;
#ifdef CONFIG_COMPAT
	hba->host->hostt->compat_ioctl = ufshcd_sprd_ioctl;
#endif

	return 0;
}

static void ufs_sprd_exit(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	ufs_sprd_plat_exit(dev, hba, host);

	devm_kfree(dev, host);
	hba->priv = NULL;
}

static int ufs_sprd_hce_enable_notify(struct ufs_hba *hba,
				      enum ufs_notify_change_status status)
{
	int err = 0;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	switch (status) {
	case PRE_CHANGE:
		err = ufs_sprd_plat_hce_enable_pre(hba, host);
		break;
	case POST_CHANGE:
		err = ufs_sprd_plat_hce_enable_post(hba, host);
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
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	switch (status) {
	case PRE_CHANGE:
		err = ufs_sprd_plat_link_startup_pre(hba, host);
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

	switch (status) {
	case PRE_CHANGE:
		memcpy(dev_req_params, dev_max_params,
		       sizeof(struct ufs_pa_layer_attr));
		break;
	case POST_CHANGE:
		if (ufshcd_is_auto_hibern8_supported(hba))
			hba->ahit = AUTO_H8_IDLE_TIME_10MS;
		break;
	default:
		err = -EINVAL;
		break;
	}

out:
	return err;
}

static void ufs_sprd_hibern8_notify(struct ufs_hba *hba,
				    enum uic_cmd_dme cmd,
				    enum ufs_notify_change_status status)
{
	unsigned long flags;
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	switch (status) {
	case PRE_CHANGE:
		if (cmd == UIC_CMD_DME_HIBER_ENTER &&
			ufshcd_is_auto_hibern8_supported(hba)) {
			spin_lock_irqsave(hba->host->host_lock, flags);
			ufshcd_writel(hba, 0, REG_AUTO_HIBERNATE_IDLE_TIMER);
			spin_unlock_irqrestore(hba->host->host_lock, flags);
		}

		ufs_sprd_plat_h8_pre(hba, cmd, host);
		break;
	case POST_CHANGE:
		if (cmd == UIC_CMD_DME_HIBER_EXIT &&
			ufshcd_is_auto_hibern8_supported(hba)) {
			spin_lock_irqsave(hba->host->host_lock, flags);
			ufshcd_writel(hba, AUTO_H8_IDLE_TIME_10MS,
				REG_AUTO_HIBERNATE_IDLE_TIMER);
			spin_unlock_irqrestore(hba->host->host_lock, flags);
		}

		ufs_sprd_plat_h8_post(hba, cmd, host);
		break;
	default:
		break;
	}
}

static int ufs_sprd_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op,
			    enum ufs_notify_change_status status)
{
	int err = 0;

	switch (status) {
	case PRE_CHANGE:
		break;
	case POST_CHANGE:
		break;
	default:
		break;
	}

	return err;
}

static struct ufs_hba_variant_ops ufs_hba_sprd_vops = {
	.name = "sprd",
	.init = ufs_sprd_init,
	.exit = ufs_sprd_exit,
	.get_ufs_hci_version = ufs_sprd_get_ufs_hci_version,
	.hce_enable_notify = ufs_sprd_hce_enable_notify,
	.link_startup_notify = ufs_sprd_link_startup_notify,
	.pwr_change_notify = ufs_sprd_pwr_change_notify,
	.hibern8_notify = ufs_sprd_hibern8_notify,
	.apply_dev_quirks = ufs_sprd_apply_dev_quirks,
	.suspend = ufs_sprd_suspend,
};

static void ufs_sprd_vh_prepare_command(void *data, struct ufs_hba *hba,
					struct request *rq,
					struct ufshcd_lrb *lrbp,
					int *err)
{
	struct ufs_sprd_host *host = ufshcd_get_variant(hba);

	if (unlikely(host->ffu_is_process == TRUE))
		prepare_command_send_in_ffu_state(hba, lrbp, err);

	return;
}

static int ufs_sprd_probe(struct platform_device *pdev)
{
	int err;
	struct device *dev = &pdev->dev;

	register_trace_android_vh_ufs_prepare_command(ufs_sprd_vh_prepare_command, NULL);

	/* Perform generic probe */
	err = ufshcd_pltfrm_init(pdev, &ufs_hba_sprd_vops);
	if (err)
		dev_err(dev, "ufshcd_pltfrm_init() failed %d\n", err);

	return err;
}

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
	SET_SYSTEM_SLEEP_PM_OPS(ufshcd_system_suspend, ufshcd_system_resume)
	SET_RUNTIME_PM_OPS(ufshcd_runtime_suspend, ufshcd_runtime_resume, NULL)
	.prepare	 = ufshcd_suspend_prepare,
	.complete	 = ufshcd_resume_complete,
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
