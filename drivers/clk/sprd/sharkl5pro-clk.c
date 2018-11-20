// SPDX-License-Identifier: GPL-2.0
//
// Spreatrum Sharkl5Pro clock driver
//
// Copyright (C) 2018 Spreadtrum, Inc.
// Author: Xiaolong Zhang <xiaolong.zhang@unisoc.com>

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <dt-bindings/clock/sprd,sharkl5pro-clk.h>

#include "common.h"
#include "composite.h"
#include "div.h"
#include "gate.h"
#include "mux.h"
#include "pll.h"

/* ap apb gates */
static SPRD_SC_GATE_CLK(sim0_eb,	"sim0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(iis0_eb,	"iis0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(iis1_eb,	"iis1-eb",	"ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(iis2_eb,	"iis2-eb",	"ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apb_reg_eb,	"apb-reg-eb",	"ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi0_eb,	"spi0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi1_eb,	"spi1-eb",	"ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi2_eb,	"spi2-eb",	"ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi3_eb,	"spi3-eb",	"ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c0_eb,	"i2c0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c1_eb,	"i2c1-eb",	"ext-26m", 0x0,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c2_eb,	"i2c2-eb",	"ext-26m", 0x0,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c3_eb,	"i2c3-eb",	"ext-26m", 0x0,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(i2c4_eb,	"i2c4-eb",	"ext-26m", 0x0,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(uart0_eb,	"uart0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(uart1_eb,	"uart1-eb",	"ext-26m", 0x0,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(uart2_eb,	"uart2-eb",	"ext-26m", 0x0,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sim0_32k_eb,	"sim0-32k-eb",	"ext-26m", 0x0,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi0_lfin_eb,	"spi0-lfin-eb",	"ext-26m", 0x0,
		     0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi1_lfin_eb,	"spi1-lfin-eb",	"ext-26m", 0x0,
		     0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi2_lfin_eb,	"spi2-lfin-eb",	"ext-26m", 0x0,
		     0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi3_lfin_eb,	"spi3-lfin-eb",	"ext-26m", 0x0,
		     0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_eb,	"sdio0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio1_eb,	"sdio1-eb",	"ext-26m", 0x0,
		     0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio2_eb,	"sdio2-eb",	"ext-26m", 0x0,
		     0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_eb,	"emmc-eb",	"ext-26m", 0x0,
		     0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_32k_eb,	"sdio0-32k-eb",	"ext-26m", 0x0,
		     0x1000, BIT(26), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio1_32k_eb,	"sdio1-32k-eb",	"ext-26m", 0x0,
		     0x1000, BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio2_32k_eb,	"sdio2-32k-eb",	"ext-26m", 0x0,
		     0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_32k_eb,	"emmc-32k-eb",	"ext-26m", 0x0,
		     0x1000, BIT(29), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sharkl5pro_apapb_gate[] = {
	/* address base is 0x71000000 */
	&sim0_eb.common,
	&iis0_eb.common,
	&iis1_eb.common,
	&iis2_eb.common,
	&apb_reg_eb.common,
	&spi0_eb.common,
	&spi1_eb.common,
	&spi2_eb.common,
	&spi3_eb.common,
	&i2c0_eb.common,
	&i2c1_eb.common,
	&i2c2_eb.common,
	&i2c3_eb.common,
	&i2c4_eb.common,
	&uart0_eb.common,
	&uart1_eb.common,
	&uart2_eb.common,
	&sim0_32k_eb.common,
	&spi0_lfin_eb.common,
	&spi1_lfin_eb.common,
	&spi2_lfin_eb.common,
	&spi3_lfin_eb.common,
	&sdio0_eb.common,
	&sdio1_eb.common,
	&sdio2_eb.common,
	&emmc_eb.common,
	&sdio0_32k_eb.common,
	&sdio1_32k_eb.common,
	&sdio2_32k_eb.common,
	&emmc_32k_eb.common,
};

static struct clk_hw_onecell_data sharkl5pro_apapb_gate_hws = {
	.hws	= {
		[CLK_SIM0_EB]		= &sim0_eb.common.hw,
		[CLK_IIS0_EB]		= &iis0_eb.common.hw,
		[CLK_IIS1_EB]		= &iis1_eb.common.hw,
		[CLK_IIS2_EB]		= &iis2_eb.common.hw,
		[CLK_APB_REG_EB]	= &apb_reg_eb.common.hw,
		[CLK_SPI0_EB]		= &spi0_eb.common.hw,
		[CLK_SPI1_EB]		= &spi1_eb.common.hw,
		[CLK_SPI2_EB]		= &spi2_eb.common.hw,
		[CLK_SPI3_EB]		= &spi3_eb.common.hw,
		[CLK_I2C0_EB]		= &i2c0_eb.common.hw,
		[CLK_I2C1_EB]		= &i2c1_eb.common.hw,
		[CLK_I2C2_EB]		= &i2c2_eb.common.hw,
		[CLK_I2C3_EB]		= &i2c3_eb.common.hw,
		[CLK_I2C4_EB]		= &i2c4_eb.common.hw,
		[CLK_UART0_EB]		= &uart0_eb.common.hw,
		[CLK_UART1_EB]		= &uart1_eb.common.hw,
		[CLK_UART2_EB]		= &uart2_eb.common.hw,
		[CLK_SIM0_32K_EB]	= &sim0_32k_eb.common.hw,
		[CLK_SPI0_LFIN_EB]	= &spi0_lfin_eb.common.hw,
		[CLK_SPI1_LFIN_EB]	= &spi1_lfin_eb.common.hw,
		[CLK_SPI2_LFIN_EB]	= &spi2_lfin_eb.common.hw,
		[CLK_SPI3_LFIN_EB]	= &spi3_lfin_eb.common.hw,
		[CLK_SDIO0_EB]		= &sdio0_eb.common.hw,
		[CLK_SDIO1_EB]		= &sdio1_eb.common.hw,
		[CLK_SDIO1_EB]		= &sdio2_eb.common.hw,
		[CLK_EMMC_EB]		= &emmc_eb.common.hw,
		[CLK_SDIO0_32K_EB]	= &sdio0_32k_eb.common.hw,
		[CLK_SDIO1_32K_EB]	= &sdio1_32k_eb.common.hw,
		[CLK_SDIO2_32K_EB]	= &sdio2_32k_eb.common.hw,
		[CLK_EMMC_32K_EB]	= &emmc_32k_eb.common.hw,
	},
	.num	= CLK_AP_APB_GATE_NUM,
};

static struct sprd_clk_desc sharkl5pro_apapb_gate_desc = {
	.clk_clks	= sharkl5pro_apapb_gate,
	.num_clk_clks	= ARRAY_SIZE(sharkl5pro_apapb_gate),
	.hw_clks	= &sharkl5pro_apapb_gate_hws,
};

static const struct of_device_id sprd_sharkl5pro_clk_ids[] = {
	{ .compatible = "sprd,sharkl5pro-apapb-gate",	/* 0x71000000 */
	  .data = &sharkl5pro_apapb_gate_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_sharkl5pro_clk_ids);

static int sharkl5pro_clk_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct sprd_clk_desc *desc;

	match = of_match_node(sprd_sharkl5pro_clk_ids, pdev->dev.of_node);
	if (!match) {
		pr_err("%s: of_match_node() failed", __func__);
		return -ENODEV;
	}

	desc = match->data;
	sprd_clk_regmap_init(pdev, desc);

	return sprd_clk_probe(&pdev->dev, desc->hw_clks);
}

static struct platform_driver sharkl5pro_clk_driver = {
	.probe	= sharkl5pro_clk_probe,
	.driver	= {
		.name	= "sharkl5pro-clk",
		.of_match_table	= sprd_sharkl5pro_clk_ids,
	},
};
module_platform_driver(sharkl5pro_clk_driver);

MODULE_DESCRIPTION("Spreadtrum Sharkl5Pro Clock Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sharkl5pro-clk");
