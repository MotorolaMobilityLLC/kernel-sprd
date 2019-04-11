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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "sprd_dpu.h"

static LIST_HEAD(dpu_clk_list);
static LIST_HEAD(dpi_clk_list);

static struct clk *clk_dpu_core;
static struct clk *clk_dpu_dpi;
static struct clk *clk_ap_ahb_disp_eb;


struct dpu_clk_context {
	struct list_head head;
	const char *name;
	struct clk *source;
	unsigned int rate;
};

/* Must be sorted in ascending order */
static char *dpu_clk_src[] = {
	"clk_src_153m6",
	"clk_src_192m",
	"clk_src_256m",
	"clk_src_307m2",
	"clk_src_384m",
};

/* Must be sorted in ascending order */
static char *dpi_clk_src[] = {
	"clk_src_96m",
	"clk_src_128m",
	"clk_src_153m6",
};


static void __iomem *base1;
static void __iomem *base2;

static int dpu_clk_parse_dt(struct dpu_context *ctx,
				struct device_node *np)
{
	struct dpu_clk_context *clk_ctx = NULL;
	int index;

	for (index = 0; index < ARRAY_SIZE(dpu_clk_src); index++) {
		clk_ctx = kzalloc(sizeof(*clk_ctx), GFP_KERNEL);
		if (!clk_ctx)
			return -ENOMEM;

		clk_ctx->source =
			of_clk_get_by_name(np, dpu_clk_src[index]);

		if (IS_ERR(clk_ctx->source)) {
			pr_warn("read dpu %s failed\n", dpu_clk_src[index]);
			clk_ctx->source = NULL;
		} else {
			clk_ctx->name = dpu_clk_src[index];
			clk_ctx->rate = clk_get_rate(clk_ctx->source);
		}

		list_add_tail(&clk_ctx->head, &dpu_clk_list);
	}

	for (index = 0; index < ARRAY_SIZE(dpi_clk_src); index++) {
		clk_ctx = kzalloc(sizeof(*clk_ctx), GFP_KERNEL);
		if (!clk_ctx)
			return -ENOMEM;

		clk_ctx->source =
			of_clk_get_by_name(np, dpi_clk_src[index]);

		if (IS_ERR(clk_ctx->source)) {
			pr_warn("read dpi %s failed\n", dpi_clk_src[index]);
			clk_ctx->source = NULL;
		} else {
			clk_ctx->name = dpi_clk_src[index];
			clk_ctx->rate = clk_get_rate(clk_ctx->source);
		}

		list_add_tail(&clk_ctx->head, &dpi_clk_list);
	}


	clk_dpu_core =
		of_clk_get_by_name(np, "clk_dpu_core");
	clk_dpu_dpi =
		of_clk_get_by_name(np, "clk_dpu_dpi");
	clk_ap_ahb_disp_eb =
		of_clk_get_by_name(np, "clk_ap_ahb_disp_eb");

	if (IS_ERR(clk_dpu_core)) {
		clk_dpu_core = NULL;
		pr_warn("read clk_dpu_core failed\n");
	}

	if (IS_ERR(clk_dpu_dpi)) {
		clk_dpu_dpi = NULL;
		pr_warn("read clk_dpu_dpi failed\n");
	}

	if (IS_ERR(clk_ap_ahb_disp_eb)) {
		clk_ap_ahb_disp_eb = NULL;
		pr_warn("read clk_ap_ahb_disp_eb failed\n");
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
	base1 = ioremap_nocache(0x20100000, 0x10000);
	base2 = ioremap_nocache(0x20200000, 0x10000);


	return 0;

}

static void dpu_glb_enable(struct dpu_context *ctx)
{
	unsigned int temp;
	pr_err("dpu glb enable\n");
	temp = readl(base1);
	temp = (temp | 0x2);
	writel(temp, base1);
	writel(0x4, base2 + 0x009c);
	writel(0x3, base2 + 0x00A0);
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
	.ver = "sharkl5Pro",
	.ops = &dpu_clk_ops,
};

static struct ops_entry glb_entry = {
	.ver = "sharkl5Pro",
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
MODULE_DESCRIPTION("sprd sharkl5Pro dpu global and clk regs config");
