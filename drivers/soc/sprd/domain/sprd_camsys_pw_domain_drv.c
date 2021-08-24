// SPDX-License-Identifier: GPL-2.0
//
// Spreatrum UMS512 camera pd driver
//
// Copyright (C) 2018 Spreadtrum, Inc.
// Author: Hongjian Wang <hongjian.wang@spreadtrum.com>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/notifier.h>

#include "sprd_camsys_domain.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd_campd: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__


static BLOCKING_NOTIFIER_HEAD(mmsys_chain);

int sprd_mm_pw_notify_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&mmsys_chain, nb);
}
EXPORT_SYMBOL(sprd_mm_pw_notify_register);

int sprd_mm_pw_notify_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&mmsys_chain, nb);
}
EXPORT_SYMBOL(sprd_mm_pw_notify_unregister);

static int mmsys_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&mmsys_chain, val, v);
}

static int check_drv_init(struct camsys_power_info *pw_info)
{
	int ret = 0;

	if (!pw_info)
		ret = -1;
	if (atomic_read(&pw_info->inited) == 0)
		ret = -2;

	return ret;
}

static int sprd_cam_pw_on(struct generic_pm_domain *domain)
{
	int ret = 0;
	struct camsys_power_info *pw_info;

	pw_info = container_of(domain, struct camsys_power_info, pd);
	ret = check_drv_init(pw_info);
	if (ret) {
		pr_err("fail to check drv init. cb: %p, ret %d\n",
			__builtin_return_address(0), ret);
		return -ENODEV;
	}
	mutex_lock(&pw_info->mlock);
	ret = pw_info->ops->sprd_cam_pw_on(pw_info);
	if (ret) {
		mutex_unlock(&pw_info->mlock);
		return ret;
	}

	ret  = pw_info->ops->sprd_cam_domain_eb(pw_info);
	if (ret) {
		mutex_unlock(&pw_info->mlock);
		return ret;
	}

	mmsys_notifier_call_chain(_E_PW_ON, NULL);
	mutex_unlock(&pw_info->mlock);
	return ret;
}

static int sprd_cam_pw_off(struct generic_pm_domain *domain)
{
	int ret = 0;
	struct camsys_power_info *pw_info;

	pw_info = container_of(domain, struct camsys_power_info, pd);
	ret = check_drv_init(pw_info);
	if (ret) {
		pr_err("fail to check drv init. cb: %p, ret %d\n",
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	mutex_lock(&pw_info->mlock);
	mmsys_notifier_call_chain(_E_PW_OFF, NULL);
	ret = pw_info->ops->sprd_cam_domain_disable(pw_info);
	if (ret) {
		mutex_unlock(&pw_info->mlock);
		return ret;
	}

	ret = pw_info->ops->sprd_cam_pw_off(pw_info);
	if (ret) {
		mutex_unlock(&pw_info->mlock);
		return ret;
	}
	mutex_unlock(&pw_info->mlock);
	return ret;
}

static const struct of_device_id sprd_campw_match_table[] = {
	{ .compatible = "sprd,pike2-camsys-domain",
	   .data = (void *)(&camsys_power_ops_pike2)},

	{ .compatible = "sprd,sharkl3-camsys-domain",
	   .data = (void *)(&camsys_power_ops_l3)},

	{ .compatible = "sprd,sharkl5pro-camsys-domain",
	   .data = (void *)(&camsys_power_ops_l5pro)},

	{ .compatible = "sprd,sharkle-camsys-domain",
	   .data = (void *)(&camsys_power_ops_le)},

	{ .compatible = "sprd,qogirl6-camsys-domain",
	   .data = (void *)(&camsys_power_ops_qogirl6)},

	{},
};

static int sprd_campw_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct camsys_power_info *pw_info;
	struct camsys_power_ops *ops = NULL;

	pw_info = devm_kzalloc(&pdev->dev, sizeof(*pw_info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(pw_info)) {
		pr_err("fail to alloc pw_info\n");
		return -ENOMEM;
	}

	ops = (struct camsys_power_ops *)
		((of_match_node(sprd_campw_match_table, np))->data);
	if (IS_ERR_OR_NULL(ops)) {
		pr_err("fail to parse sprd_campw_match_table item\n");
		return -ENOENT;
	}

	pw_info->ops = ops;
	pw_info->ops->sprd_campw_init(pdev, pw_info);
	pw_info->pd.name = kstrdup(np->name, GFP_KERNEL);
	pw_info->pd.power_off = sprd_cam_pw_off;
	pw_info->pd.power_on = sprd_cam_pw_on;
	pdev->dev.platform_data = (void *)pw_info;
	pm_genpd_init(&pw_info->pd, NULL, true);
	of_genpd_add_provider_simple(np, &pw_info->pd);
	mutex_init(&pw_info->mlock);
	atomic_set(&pw_info->inited, 1);
	return 0;
}

static int sprd_campw_remove(struct platform_device *pdev)
{
	struct camsys_power_info *pw_info;
	int ret = 0;

	pw_info = pdev->dev.platform_data;
	ret = check_drv_init(pw_info);
	if (ret) {
		pr_err("fail to check drv init: cb: %p, ret %d\n",
			__builtin_return_address(0), ret);
		return -ENODEV;
	}

	pm_genpd_remove(&pw_info->pd);
	devm_kfree(&pdev->dev, pw_info);

	return 0;
}

static struct platform_driver sprd_campw_driver = {
	.probe = sprd_campw_probe,
	.remove = sprd_campw_remove,
	.driver = {
		.name = "sprd_campd",
		.of_match_table = sprd_campw_match_table,
	},
};
module_platform_driver(sprd_campw_driver);

MODULE_DESCRIPTION("Camsys Power Domain Driver");
MODULE_AUTHOR("hongjian.wang@unisoc.com");
MODULE_LICENSE("GPL v2");
