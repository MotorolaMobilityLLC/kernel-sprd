// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc SC7731E clock driver
 *
 * Copyright (C) 2020 Unisoc, Inc.
 * Author: Xiongpeng Wu <xiongpeng.wu@unisoc.com>
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <dt-bindings/clock/sprd,sc7731e-clk.h>

#include "common.h"
#include "composite.h"
#include "div.h"
#include "gate.h"
#include "mux.h"
#include "pll.h"

#define SC7731E_MUX_FLAG	\
	(CLK_GET_RATE_NOCACHE | CLK_SET_RATE_NO_REPARENT)

/* pmu apb pll gates clock */
static CLK_FIXED_FACTOR_FW_NAME(clk_26m_aud, "clk-26m-aud", "ext-26m", 1, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_13m, "clk-13m", "ext-26m", 2, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_6m5, "clk-6m5", "ext-26m", 4, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_4m3, "clk-4m3", "ext-26m", 6, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_2m, "clk-2m", "ext-26m", 13, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_1m, "clk-1m", "ext-26m", 26, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(clk_250k, "clk-250k", "ext-26m", 104, 1, 0);

static SPRD_PLL_SC_GATE_CLK_FW_NAME(cpll_gate, "cpll-gate", "ext-26m", 0x88,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(gpll_gate, "gpll-gate", "ext-26m", 0x90,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(mpll_gate, "mpll-gate", "ext-26m", 0x94,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(dpll_gate, "dpll-gate", "ext-26m", 0x98,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK_FW_NAME(bbpll_gate, "bbpll-gate", "ext-26m", 0x344,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);

static struct sprd_clk_common *sc7731e_pmu_gate_clks[] = {
	/* address base is 0x402b0000 */
	&cpll_gate.common,
	&gpll_gate.common,
	&mpll_gate.common,
	&dpll_gate.common,
	&bbpll_gate.common,
};

static struct clk_hw_onecell_data sc7731e_pmu_gate_hws = {
	.hws	= {
		[CLK_26M_AUD]		= &clk_26m_aud.hw,
		[CLK_13M]		= &clk_13m.hw,
		[CLK_6M5]		= &clk_6m5.hw,
		[CLK_4M3]		= &clk_4m3.hw,
		[CLK_2M]		= &clk_2m.hw,
		[CLK_1M]		= &clk_1m.hw,
		[CLK_250K]		= &clk_250k.hw,
		[CLK_CPLL_GATE]		= &cpll_gate.common.hw,
		[CLK_GPLL_GATE]		= &gpll_gate.common.hw,
		[CLK_MPLL_GATE]		= &mpll_gate.common.hw,
		[CLK_DPLL_GATE]		= &dpll_gate.common.hw,
		[CLK_BBPLL_GATE]	= &bbpll_gate.common.hw,
	},
	.num	= CLK_PMU_APB_NUM,
};

static const struct sprd_clk_desc sc7731e_pmu_gate_desc = {
	.clk_clks	= sc7731e_pmu_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_pmu_gate_clks),
	.hw_clks        = &sc7731e_pmu_gate_hws,
};

/* aon apb pll register clock */
static const u64 itable[5] = {4,
			951000000ULL, 1131000000ULL,
			1145000000ULL, 1600000000ULL};

static const struct clk_bit_field f_twpll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 20,	.width = 1 },	/* div_s	*/
	{ .shift = 19,	.width = 1 },	/* mod_en	*/
	{ .shift = 18,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 11,	.width = 2 },	/* ibias	*/
	{ .shift = 0,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 0,	.width = 0 },	/* postdiv	*/
};

#define f_mpll f_twpll
static SPRD_PLL_HW(mpll, "mpll", &mpll_gate.common.hw, 0x44,
		   2, itable, f_mpll, 240,
		   1000, 1000, 0, 0);
#define f_dpll f_twpll
static SPRD_PLL_HW(dpll, "dpll", &dpll_gate.common.hw, 0x4c,
		   2, itable, f_dpll, 240,
		   1000, 1000, 0, 0);

static SPRD_PLL_FW_NAME(twpll, "twpll", "ext-26m", 0x54,
			2, itable, f_twpll, 240,
			1000, 1000, 0, 0);

static CLK_FIXED_FACTOR_FW_NAME(twpll_768m, "twpll-768m", "twpll", 2, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_384m, "twpll-384m", "twpll", 4, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_192m, "twpll-192m", "twpll", 8, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_96m, "twpll-96m", "twpll", 16, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_48m, "twpll-48m", "twpll", 32, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_24m, "twpll-24m", "twpll", 64, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_12m, "twpll-12m", "twpll", 128, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_512m, "twpll-512m", "twpll", 3, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_256m, "twpll-256m", "twpll", 6, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_128m, "twpll-128m", "twpll", 12, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_64m, "twpll-64m", "twpll", 24, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_307m2, "twpll-307m2", "twpll", 5, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_219m4, "twpll-219m4", "twpll", 7, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_170m6, "twpll-170m6", "twpll", 9, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_153m6, "twpll-153m6", "twpll", 10, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_76m8, "twpll-76m8", "twpll", 20, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_51m2, "twpll-51m2", "twpll", 30, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_38m4, "twpll-38m4", "twpll", 40, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(twpll_19m2, "twpll-19m2", "twpll", 80, 1, 0);

#define f_cpll f_twpll
static SPRD_PLL_HW(cpll, "cpll", &cpll_gate.common.hw, 0x150,
		   2, itable, f_cpll, 240,
		   1000, 1000, 0, 0);

static CLK_FIXED_FACTOR_FW_NAME(cpll_800m, "cpll-800m", "cpll", 2, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(cpll_533m, "cpll-533m", "cpll", 3, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(cpll_400m, "cpll-400m", "cpll", 4, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(cpll_320m, "cpll-320m", "cpll", 5, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(cpll_266m67, "cpll-266m67", "cpll", 6, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(cpll_228m57, "cpll-228m57", "cpll", 7, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(cpll_200m, "cpll-200m", "cpll", 8, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(cpll_160m, "cpll-160m", "cpll", 10, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(cpll_133m34, "cpll-133m34", "cpll", 12, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(cpll_100m, "cpll-100m", "cpll", 16, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(cpll_50m, "cpll-50m", "cpll", 32, 1, 0);
static CLK_FIXED_FACTOR_FW_NAME(cpll_40m, "cpll-40m", "cpll", 40, 1, 0);

static const struct clk_bit_field f_gpll[PLL_FACT_MAX] = {
	{ .shift = 0,	.width = 0 },	/* lock_done	*/
	{ .shift = 20,	.width = 1 },	/* div_s	*/
	{ .shift = 19,	.width = 1 },	/* mod_en	*/
	{ .shift = 18,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 11,	.width = 2 },	/* ibias	*/
	{ .shift = 0,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 13,	.width = 1 },	/* postdiv	*/
};

static SPRD_PLL_HW(gpll, "gpll", &gpll_gate.common.hw, 0x158,
		   2, itable, f_gpll, 240,
		   1000, 1000, 1, 600000000);

static CLK_FIXED_FACTOR_HW(bbpll_416m, "bbpll-416m", &bbpll_gate.common.hw,
			   3, 1, 0);

static struct sprd_clk_common *sc7731e_pll_clks[] = {
	/* address base is 0x402e0000 */
	&twpll.common,
	&cpll.common,
	&mpll.common,
	&dpll.common,
	&gpll.common,
};

static struct clk_hw_onecell_data sc7731e_pll_hws = {
	.hws	= {
		[CLK_TWPLL]		= &twpll.common.hw,
		[CLK_TWPLL_768M]	= &twpll_768m.hw,
		[CLK_TWPLL_384M]	= &twpll_384m.hw,
		[CLK_TWPLL_192M]	= &twpll_192m.hw,
		[CLK_TWPLL_96M]		= &twpll_96m.hw,
		[CLK_TWPLL_48M]		= &twpll_48m.hw,
		[CLK_TWPLL_24M]		= &twpll_24m.hw,
		[CLK_TWPLL_12M]		= &twpll_12m.hw,
		[CLK_TWPLL_512M]	= &twpll_512m.hw,
		[CLK_TWPLL_256M]	= &twpll_256m.hw,
		[CLK_TWPLL_128M]	= &twpll_128m.hw,
		[CLK_TWPLL_64M]		= &twpll_64m.hw,
		[CLK_TWPLL_307M2]	= &twpll_307m2.hw,
		[CLK_TWPLL_219M4]	= &twpll_219m4.hw,
		[CLK_TWPLL_170M6]	= &twpll_170m6.hw,
		[CLK_TWPLL_153M6]	= &twpll_153m6.hw,
		[CLK_TWPLL_76M8]	= &twpll_76m8.hw,
		[CLK_TWPLL_51M2]	= &twpll_51m2.hw,
		[CLK_TWPLL_38M4]	= &twpll_38m4.hw,
		[CLK_TWPLL_19M2]	= &twpll_19m2.hw,
		[CLK_CPLL]		= &cpll.common.hw,
		[CLK_CPPLL_800M]	= &cpll_800m.hw,
		[CLK_CPPLL_533M]	= &cpll_533m.hw,
		[CLK_CPPLL_400M]	= &cpll_400m.hw,
		[CLK_CPPLL_320M]	= &cpll_320m.hw,
		[CLK_CPPLL_266M]	= &cpll_266m67.hw,
		[CLK_CPPLL_228M]	= &cpll_228m57.hw,
		[CLK_CPPLL_200M]	= &cpll_200m.hw,
		[CLK_CPPLL_160M]	= &cpll_160m.hw,
		[CLK_CPPLL_133M]	= &cpll_133m34.hw,
		[CLK_CPPLL_100M]	= &cpll_100m.hw,
		[CLK_CPPLL_50M]		= &cpll_50m.hw,
		[CLK_CPPLL_40M]		= &cpll_40m.hw,
		[CLK_MPLL]		= &mpll.common.hw,
		[CLK_DPLL]		= &dpll.common.hw,
		[CLK_GPLL]		= &gpll.common.hw,
		[CLK_BBPLL_416M]	= &bbpll_416m.hw,

	},
	.num	= CLK_AON_PLL_NUM,
};

static const struct sprd_clk_desc sc7731e_pll_desc = {
	.clk_clks	= sc7731e_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(sc7731e_pll_clks),
	.hw_clks        = &sc7731e_pll_hws,
};