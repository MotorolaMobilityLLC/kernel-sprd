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

#include "sprd_dpu.h"

static struct dpu_clk_context {
	struct clk *clk_src_96m;
	struct clk *clk_src_128m;
	struct clk *clk_src_153m6;
	struct clk *clk_src_192m;
	struct clk *clk_src_256m;
	struct clk *clk_src_384m;
	struct clk *clk_dpu_core;
	struct clk *clk_dpu_dpi;
} dpu_clk_ctx;

static const u32 dpu_core_clk[] = {
	153600000,
	192000000,
	256000000,
	384000000
};

static const u32 dpi_clk_src[] = {
	96000000,
	128000000,
	153600000,
	192000000
};

static void __iomem *base1;
static void __iomem *base2;

static int dpu_clk_parse_dt(struct dpu_context *ctx,
				struct device_node *np)
{
	struct dpu_clk_context *clk_ctx = &dpu_clk_ctx;

	clk_ctx->clk_src_96m =
		of_clk_get_by_name(np, "clk_src_96m");
	clk_ctx->clk_src_128m =
		of_clk_get_by_name(np, "clk_src_128m");
	clk_ctx->clk_src_153m6 =
		of_clk_get_by_name(np, "clk_src_153m6");
	clk_ctx->clk_src_192m =
		of_clk_get_by_name(np, "clk_src_192m");
	clk_ctx->clk_src_256m =
		of_clk_get_by_name(np, "clk_src_256m");
	clk_ctx->clk_src_384m =
		of_clk_get_by_name(np, "clk_src_384m");
	clk_ctx->clk_dpu_core =
		of_clk_get_by_name(np, "clk_dpu_core");
	clk_ctx->clk_dpu_dpi =
		of_clk_get_by_name(np, "clk_dpu_dpi");

	if (IS_ERR(clk_ctx->clk_src_96m)) {
		pr_warn("read clk_src_96m failed\n");
		clk_ctx->clk_src_96m = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_128m)) {
		pr_warn("read clk_src_128m failed\n");
		clk_ctx->clk_src_128m = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_153m6)) {
		pr_warn("read clk_src_153m6 failed\n");
		clk_ctx->clk_src_153m6 = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_192m)) {
		pr_warn("read clk_src_192m failed\n");
		clk_ctx->clk_src_192m = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_256m)) {
		pr_warn("read clk_src_256m failed\n");
		clk_ctx->clk_src_256m = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_384m)) {
		pr_warn("read clk_src_384m failed\n");
		clk_ctx->clk_src_384m = NULL;
	}

	if (IS_ERR(clk_ctx->clk_dpu_core)) {
		pr_warn("read clk_dpu_core failed\n");
		clk_ctx->clk_dpu_core = NULL;
	}

	if (IS_ERR(clk_ctx->clk_dpu_dpi)) {
		pr_warn("read clk_dpu_dpi failed\n");
		clk_ctx->clk_dpu_dpi = NULL;
	}

	return 0;
}

static int dpu_clk_init(struct dpu_context *ctx)
{


	return 0;
}

static int dpu_clk_enable(struct dpu_context *ctx)
{


	return 0;
}

static int dpu_clk_disable(struct dpu_context *ctx)
{


	return 0;
}

static int dpu_glb_parse_dt(struct dpu_context *ctx,
				struct device_node *np)
{
	pr_err("dpu glb parse dt\n");
	base1 = ioremap_nocache(0x21500000, 0x10000);
	base2 = ioremap_nocache(0x20e00000, 0x10000);


	return 0;

}

static void dpu_glb_enable(struct dpu_context *ctx)
{
	unsigned int temp;

	pr_err("dpu glb enable\n");
	temp = readl(base1);
	temp = (temp | 0x2);
	writel(temp, base1);
	writel(0x4, base2 + 0x0030);
	writel(0x102, base2 + 0x0034);
}

static void dpu_glb_disable(struct dpu_context *ctx)
{
}

static void dpu_reset(struct dpu_context *ctx)
{
	unsigned int temp;

	pr_err("dpu glb reset\n");
	temp = readl(base1 + 0x0004);
	temp = temp | 0x2;
	writel(temp, base1 + 0x0004);
	udelay(10);

	temp = temp & 0xfffffffD;
	writel(temp, base1 + 0x0004);
}

static void dpu_power_domain(struct dpu_context *ctx, int enable)
{

}

static struct dpu_clk_ops dpu_clk_ops = {
	.parse_dt = dpu_clk_parse_dt,
	.init = dpu_clk_init,
	.enable = dpu_clk_enable,
	.disable = dpu_clk_disable,
};

static struct dpu_glb_ops dpu_glb_ops = {
	.parse_dt = dpu_glb_parse_dt,
	.reset = dpu_reset,
	.enable = dpu_glb_enable,
	.disable = dpu_glb_disable,
	.power = dpu_power_domain,
};

static struct ops_entry clk_entry = {
	.ver = "pike2",
	.ops = &dpu_clk_ops,
};

static struct ops_entry glb_entry = {
	.ver = "pike2",
	.ops = &dpu_glb_ops,
};

static int __init dpu_glb_register(void)
{
	dpu_clk_ops_register(&clk_entry);
	dpu_glb_ops_register(&glb_entry);
	return 0;
}

subsys_initcall(dpu_glb_register);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Albert.zhang@unisoc.com");
MODULE_DESCRIPTION("sprd pike2 dpu global and clk regs config");
