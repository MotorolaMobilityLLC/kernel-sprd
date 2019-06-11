/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "sprd_dsi.h"

static struct dsi_glb_context {
	unsigned int ctrl_reg;
	unsigned int ctrl_mask;

	struct clk *clk;
	struct regmap *regmap;
} dsi_glb_ctx;


static int dsi_glb_parse_dt(struct dsi_context *ctx,
				struct device_node *np)
{
	struct dsi_glb_context *glb_ctx = &dsi_glb_ctx;
	unsigned int syscon_args[2];
	int ret;

	glb_ctx->clk =
		of_clk_get_by_name(np, "clk_aon_apb_disp_eb");
	if (IS_ERR(glb_ctx->clk)) {
		pr_warn("read clk_aon_apb_disp_eb failed\n");
		glb_ctx->clk = NULL;
	}

	glb_ctx->regmap = syscon_regmap_lookup_by_name(np, "reset");
	if (IS_ERR(glb_ctx->regmap)) {
		pr_warn("failed to map dsi glb reg\n");
		return PTR_ERR(glb_ctx->regmap);
	}

	ret = syscon_get_args_by_name(np, "reset", 2, syscon_args);
	if (ret == 2) {
		glb_ctx->ctrl_reg = syscon_args[0];
		glb_ctx->ctrl_mask = syscon_args[1];
	} else {
		pr_warn("failed to parse dsi glb reg\n");
	}

	return 0;
}

static void dsi_glb_enable(struct dsi_context *ctx)
{

}

static void dsi_glb_disable(struct dsi_context *ctx)
{
}

static void dsi_reset(struct dsi_context *ctx)
{

}

static struct dsi_glb_ops dsi_glb_ops = {
	.parse_dt = dsi_glb_parse_dt,
	.reset = dsi_reset,
	.enable = dsi_glb_enable,
	.disable = dsi_glb_disable,
};

static struct ops_entry entry = {
	.ver = "pike2",
	.ops = &dsi_glb_ops,
};

static int __init dsi_glb_register(void)
{
	return dsi_glb_ops_register(&entry);
}

subsys_initcall(dsi_glb_register);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Albert.zhang@unisoc.com");
MODULE_DESCRIPTION("sprd pike2 dsi global APB regs low-level config");
