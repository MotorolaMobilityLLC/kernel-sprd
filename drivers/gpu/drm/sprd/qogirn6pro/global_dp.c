// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "sprd_dp.h"

static struct clk *clk_ipa_apb_dptx_eb;

static int dp_glb_parse_dt(struct dp_context *ctx,
				struct device_node *np)
{
	clk_ipa_apb_dptx_eb =
		of_clk_get_by_name(np, "clk_ipa_apb_dptx_eb");
	if (IS_ERR(clk_ipa_apb_dptx_eb)) {
		pr_warn("read clk_ipa_apb_dptx_eb failed\n");
		clk_ipa_apb_dptx_eb = NULL;
	}

	return 0;
}

static void dp_glb_enable(struct dp_context *ctx)
{
	int ret;

	ret = clk_prepare_enable(clk_ipa_apb_dptx_eb);
	if (ret)
		pr_err("enable clk_ipa_apb_dptx_eb failed!\n");
}

static void dp_glb_disable(struct dp_context *ctx)
{
	clk_disable_unprepare(clk_ipa_apb_dptx_eb);
}

static void dp_reset(struct dp_context *ctx)
{
}

static struct dp_glb_ops dp_glb_ops = {
	.parse_dt = dp_glb_parse_dt,
	.reset = dp_reset,
	.enable = dp_glb_enable,
	.disable = dp_glb_disable,
};

static struct ops_entry entry = {
	.ver = "qogirn6pro1",
	.ops = &dp_glb_ops,
};

static int __init dp_glb_register(void)
{
	return dp_glb_ops_register(&entry);
}

subsys_initcall(dp_glb_register);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("chen.he@unisoc.com");
MODULE_DESCRIPTION("sprd sharkl6Pro dp global APB regs low-level config");
