// SPDX-License-Identifier: GPL-2.0
//
// Spreatrum SC9863a clock driver
//
// Copyright (C) 2018 Spreadtrum, Inc.
// Author: Chunyan Zhang <chunyan.zhang@spreadtrum.com>

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <dt-bindings/clock/sprd,sc9863a-clk.h>

#include "common.h"
#include "composite.h"
#include "div.h"
#include "gate.h"
#include "mux.h"
#include "pll.h"

SPRD_PLL_SC_GATE_CLK(mpll0_gate, "mpll0-gate", "ext-26m", 0x94,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(dpll0_gate, "dpll0-gate", "ext-26m", 0x98,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(lpll_gate, "lpll-gate", "ext-26m", 0x9c,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(gpll_gate, "gpll-gate", "ext-26m", 0xa8,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(dpll1_gate, "dpll1-gate", "ext-26m", 0x1dc,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(mpll1_gate, "mpll1-gate", "ext-26m", 0x1e0,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(mpll2_gate, "mpll2-gate", "ext-26m", 0x1e4,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);
SPRD_PLL_SC_GATE_CLK(isppll_gate, "isppll-gate", "ext-26m", 0x1e8,
				0x1000, BIT(0), CLK_IGNORE_UNUSED, 0, 240);

static struct sprd_clk_common *sc9863a_pmu_gate_clks[] = {
	/* address base is 0x402b0000 */
	&mpll0_gate.common,
	&dpll0_gate.common,
	&lpll_gate.common,
	&gpll_gate.common,
	&dpll1_gate.common,
	&mpll1_gate.common,
	&mpll2_gate.common,
	&isppll_gate.common,
};

static struct clk_hw_onecell_data sc9863a_pmu_gate_hws = {
	.hws	= {
		[CLK_MPLL0_GATE]	= &mpll0_gate.common.hw,
		[CLK_DPLL0_GATE]	= &dpll0_gate.common.hw,
		[CLK_LPLL_GATE]		= &lpll_gate.common.hw,
		[CLK_GPLL_GATE]		= &gpll_gate.common.hw,
		[CLK_DPLL1_GATE]	= &dpll1_gate.common.hw,
		[CLK_MPLL1_GATE]	= &mpll1_gate.common.hw,
		[CLK_MPLL2_GATE]	= &mpll2_gate.common.hw,
		[CLK_ISPPLL_GATE]	= &isppll_gate.common.hw,
	},
	.num	= CLK_PMU_APB_NUM,
};

static const struct sprd_clk_desc sc9863a_pmu_gate_desc = {
	.clk_clks	= sc9863a_pmu_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_pmu_gate_clks),
	.hw_clks        = &sc9863a_pmu_gate_hws,
};

static const u64 itable[5] = {4, 1000000000, 1200000000,
			      1400000000, 1600000000};

static const struct clk_bit_field f_twpll[PLL_FACT_MAX] = {
	{ .shift = 95,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 1,	.width = 1 },	/* mod_en	*/
	{ .shift = 2,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 3,	.width = 3 },	/* ibias	*/
	{ .shift = 8,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 0,	.width = 0 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_1K(twpll_clk, "twpll", "ext-26m", 0x4,
				   3, itable, f_twpll, 240);
static CLK_FIXED_FACTOR(twpll_768m, "twpll-768m", "twpll", 2, 1, 0);
static CLK_FIXED_FACTOR(twpll_384m, "twpll-384m", "twpll", 4, 1, 0);
static CLK_FIXED_FACTOR(twpll_192m, "twpll-192m", "twpll", 8, 1, 0);
static CLK_FIXED_FACTOR(twpll_96m, "twpll-96m", "twpll", 16, 1, 0);
static CLK_FIXED_FACTOR(twpll_48m, "twpll-48m", "twpll", 32, 1, 0);
static CLK_FIXED_FACTOR(twpll_24m, "twpll-24m", "twpll", 64, 1, 0);
static CLK_FIXED_FACTOR(twpll_12m, "twpll-12m", "twpll", 128, 1, 0);
static CLK_FIXED_FACTOR(twpll_512m, "twpll-512m", "twpll", 3, 1, 0);
static CLK_FIXED_FACTOR(twpll_256m, "twpll-256m", "twpll", 6, 1, 0);
static CLK_FIXED_FACTOR(twpll_128m, "twpll-128m", "twpll", 12, 1, 0);
static CLK_FIXED_FACTOR(twpll_64m, "twpll-64m", "twpll", 24, 1, 0);
static CLK_FIXED_FACTOR(twpll_307m2, "twpll-307m2", "twpll", 5, 1, 0);
static CLK_FIXED_FACTOR(twpll_219m4, "twpll-219m4", "twpll", 7, 1, 0);
static CLK_FIXED_FACTOR(twpll_170m6, "twpll-170m6", "twpll", 9, 1, 0);
static CLK_FIXED_FACTOR(twpll_153m6, "twpll-153m6", "twpll", 10, 1, 0);
static CLK_FIXED_FACTOR(twpll_76m8, "twpll-76m8", "twpll", 20, 1, 0);
static CLK_FIXED_FACTOR(twpll_51m2, "twpll-51m2", "twpll", 30, 1, 0);
static CLK_FIXED_FACTOR(twpll_38m4, "twpll-38m4", "twpll", 40, 1, 0);
static CLK_FIXED_FACTOR(twpll_19m2, "twpll-19m2", "twpll", 80, 1, 0);

static const struct clk_bit_field f_lpll[PLL_FACT_MAX] = {
	{ .shift = 95,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 1,	.width = 1 },	/* mod_en	*/
	{ .shift = 2,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 6,	.width = 2 },	/* ibias	*/
	{ .shift = 8,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 0,	.width = 0 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_1K(lpll_clk, "lpll", "lpll-gate", 0x20,
				   3, itable, f_lpll, 240);
static CLK_FIXED_FACTOR(lpll_409m6, "lpll-409m6", "lpll", 3, 1, 0);
static CLK_FIXED_FACTOR(lpll_245m76, "lpll-245m76", "lpll", 5, 1, 0);

static const struct clk_bit_field f_gpll[PLL_FACT_MAX] = {
	{ .shift = 95,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 1,	.width = 1 },	/* mod_en	*/
	{ .shift = 2,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 6,	.width = 2 },	/* ibias	*/
	{ .shift = 8,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 48,	.width = 1 },	/* postdiv	*/
};
static SPRD_PLL_WITH_ITABLE_K_FVCO(gpll_clk, "gpll", "gpll-gate", 0x38,
				   3, itable, f_gpll, 240,
				   1000, 1000, 1, 400000000);

#define f_isppll f_gpll
static SPRD_PLL_WITH_ITABLE_1K(isppll_clk, "isppll", "isppll-gate", 0x50,
				   3, itable, f_isppll, 240);
static CLK_FIXED_FACTOR(isppll_468m, "isppll-468m", "isppll", 2, 1, 0);

static struct sprd_clk_common *sc9863a_pll_clks[] = {
	/* address base is 0x40353000 */
	&twpll_clk.common,
	&lpll_clk.common,
	&gpll_clk.common,
	&isppll_clk.common,
};

static struct clk_hw_onecell_data sc9863a_pll_hws = {
	.hws	= {
		[CLK_TWPLL]		= &twpll_clk.common.hw,
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
		[CLK_LPLL]		= &lpll_clk.common.hw,
		[CLK_LPLL_409M6]	= &lpll_409m6.hw,
		[CLK_LPLL_245M76]	= &lpll_245m76.hw,
		[CLK_GPLL]		= &gpll_clk.common.hw,
		[CLK_ISPPLL]		= &isppll_clk.common.hw,
		[CLK_ISPPLL_468M]	= &isppll_468m.hw,

	},
	.num	= CLK_ANLG_PHY_G1_NUM,
};

static const struct sprd_clk_desc sc9863a_pll_desc = {
	.clk_clks	= sc9863a_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_pll_clks),
	.hw_clks        = &sc9863a_pll_hws,
};

#define f_mpll f_gpll
static const u64 itable_mpll[6] = {5, 1000000000, 1200000000, 1400000000,
				   1600000000, 1800000000};
static SPRD_PLL_WITH_ITABLE_K_FVCO(mpll0_clk, "mpll0", "mpll0-gate", 0x0,
				   3, itable_mpll, f_mpll, 240,
				   1000, 1000, 1, 1000000000);
static SPRD_PLL_WITH_ITABLE_K_FVCO(mpll1_clk, "mpll1", "mpll1-gate", 0x18,
				   3, itable_mpll, f_mpll, 240,
				   1000, 1000, 1, 1000000000);
static SPRD_PLL_WITH_ITABLE_K_FVCO(mpll2_clk, "mpll2", "mpll2-gate", 0x30,
				   3, itable_mpll, f_mpll, 240,
				   1000, 1000, 1, 1000000000);
static CLK_FIXED_FACTOR(mpll2_675m, "mpll2-675m", "mpll2", 2, 1, 0);

static struct sprd_clk_common *sc9863a_mpll_clks[] = {
	/* address base is 0x40359000 */
	&mpll0_clk.common,
	&mpll1_clk.common,
	&mpll2_clk.common,
};

static struct clk_hw_onecell_data sc9863a_mpll_hws = {
	.hws	= {
		[CLK_MPLL0]		= &mpll0_clk.common.hw,
		[CLK_MPLL1]		= &mpll1_clk.common.hw,
		[CLK_MPLL2]		= &mpll2_clk.common.hw,
		[CLK_MPLL2_675M]	= &mpll2_675m.hw,

	},
	.num	= CLK_ANLG_PHY_G4_NUM,
};

static const struct sprd_clk_desc sc9863a_mpll_desc = {
	.clk_clks	= sc9863a_mpll_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_mpll_clks),
	.hw_clks        = &sc9863a_mpll_hws,
};

static SPRD_SC_GATE_CLK(audio_gate,	"audio-gate",	"ext-26m", 0x4,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);

#define f_rpll f_lpll
static SPRD_PLL_WITH_ITABLE_1K(rpll_clk, "rpll", "ext-26m", 0x10,
				   3, itable, f_rpll, 240);

static CLK_FIXED_FACTOR(rpll_390m, "rpll-390m", "rpll", 2, 1, 0);
static CLK_FIXED_FACTOR(rpll_260m, "rpll-260m", "rpll", 3, 1, 0);
static CLK_FIXED_FACTOR(rpll_195m, "rpll-195m", "rpll", 4, 1, 0);
static CLK_FIXED_FACTOR(rpll_26m, "rpll-26m", "rpll", 30, 1, 0);

static struct sprd_clk_common *sc9863a_rpll_clks[] = {
	/* address base is 0x4035c000 */
	&audio_gate.common,
	&rpll_clk.common,
};

static struct clk_hw_onecell_data sc9863a_rpll_hws = {
	.hws	= {
		[CLK_AUDIO_GATE]	= &audio_gate.common.hw,
		[CLK_RPLL]		= &rpll_clk.common.hw,
		[CLK_RPLL_390M]		= &rpll_390m.hw,
		[CLK_RPLL_260M]		= &rpll_260m.hw,
		[CLK_RPLL_195M]		= &rpll_195m.hw,
		[CLK_RPLL_26M]		= &rpll_26m.hw,
	},
	.num	= CLK_ANLG_PHY_G5_NUM,
};

static const struct sprd_clk_desc sc9863a_rpll_desc = {
	.clk_clks	= sc9863a_rpll_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_rpll_clks),
	.hw_clks        = &sc9863a_rpll_hws,
};

#define f_dpll f_lpll
static const u64 itable_dpll[5] = {4, 1211000000, 1320000000, 1570000000,
				   1866000000};
static SPRD_PLL_WITH_ITABLE_1K(dpll0_clk, "dpll0", "dpll0-gate", 0x0,
				   3, itable_dpll, f_dpll, 240);
static SPRD_PLL_WITH_ITABLE_1K(dpll1_clk, "dpll1", "dpll1-gate", 0x18,
				   3, itable_dpll, f_dpll, 240);

static CLK_FIXED_FACTOR(dpll0_933m, "dpll0-933m", "dpll0", 2, 1, 0);
static CLK_FIXED_FACTOR(dpll0_622m3, "dpll0-622m", "dpll0", 3, 1, 0);
static CLK_FIXED_FACTOR(dpll1_400m, "dpll1-400m", "dpll1", 4, 1, 0);
static CLK_FIXED_FACTOR(dpll1_266m7, "dpll1-266m7", "dpll1", 6, 1, 0);
static CLK_FIXED_FACTOR(dpll1_123m1, "dpll1-123m1", "dpll1", 13, 1, 0);
static CLK_FIXED_FACTOR(dpll1_50m, "dpll1-50m", "dpll1", 32, 1, 0);

static struct sprd_clk_common *sc9863a_dpll_clks[] = {
	/* address base is 0x40363000 */
	&dpll0_clk.common,
	&dpll1_clk.common,
};

static struct clk_hw_onecell_data sc9863a_dpll_hws = {
	.hws	= {
		[CLK_DPLL0]		= &dpll0_clk.common.hw,
		[CLK_DPLL1]		= &dpll1_clk.common.hw,
		[CLK_DPLL0_933M]	= &dpll0_933m.hw,
		[CLK_DPLL0_622M3]	= &dpll0_622m3.hw,
		[CLK_DPLL0_400M]	= &dpll1_400m.hw,
		[CLK_DPLL0_266M7]	= &dpll1_266m7.hw,
		[CLK_DPLL0_123M1]	= &dpll1_123m1.hw,
		[CLK_DPLL0_50M]		= &dpll1_50m.hw,

	},
	.num	= CLK_ANLG_PHY_G7_NUM,
};

static const struct sprd_clk_desc sc9863a_dpll_desc = {
	.clk_clks	= sc9863a_dpll_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_dpll_clks),
	.hw_clks        = &sc9863a_dpll_hws,
};

#define SC9863A_MUX_FLAG	\
	(CLK_GET_RATE_NOCACHE | CLK_SET_RATE_NO_REPARENT)

static CLK_FIXED_FACTOR(fac_rco_25m,	"rco-25m",	"ext-rco-100m",
			4, 1, 0);
static CLK_FIXED_FACTOR(fac_rco_4m,	"rco-4m",	"ext-rco-100m",
			25, 1, 0);
static CLK_FIXED_FACTOR(fac_rco_2m,	"rco-2m",	"ext-rco-100m",
			50, 1, 0);

static const char * const aon_apb_parents[] = { "rco-4m",	"rco-25m",
						"ext-26m",	"twpll-96m",
						"ext-rco-100m",	"twpll-128m" };
static SPRD_COMP_CLK(aon_apb, "aon-apb", aon_apb_parents, 0x224,
		     0, 3, 8, 2, 0);

static const char * const ap_axi_parents[] = { "ext-26m", "twpll-76m8",
					       "twpll-128m", "twpll-256m" };
static SPRD_MUX_CLK(ap_axi, "ap-axi", ap_axi_parents, 0x2c8,
		    0, 2, SC9863A_MUX_FLAG);

static const char * const sdio_parents[] = { "ext-26m", "twpll-307m2",
					     "twpll-384m", "rpll-390m",
					     "dpll1-400m", "lpll-409m6" };
static SPRD_MUX_CLK(sdio0_2x, "sdio0-2x", sdio_parents, 0x2cc,
			0, 3, SC9863A_MUX_FLAG);
static SPRD_MUX_CLK(sdio1_2x, "sdio1-2x", sdio_parents, 0x2d4,
			0, 3, SC9863A_MUX_FLAG);
static SPRD_MUX_CLK(sdio2_2x, "sdio2-2x", sdio_parents, 0x2dc,
			0, 3, SC9863A_MUX_FLAG);
static SPRD_MUX_CLK(emmc_2x, "emmc-2x", sdio_parents, 0x2e4,
			0, 3, SC9863A_MUX_FLAG);

static const char * const dpu_parents[] = { "twpll-153m6", "twpll-192m",
					    "twpll-256m", "twpll-384m"};
static SPRD_MUX_CLK(dpu_clk, "dpu", dpu_parents, 0x2f4,
			0, 2, SC9863A_MUX_FLAG);

static const char * const dpu_dpi_parents[] = { "twpll-128m", "twpll-153m6",
					    "twpll-192m" };
static SPRD_COMP_CLK(dpu_dpi,	"dpu-dpi", dpu_dpi_parents, 0x2f8,
		     0, 2, 8, 4, 0);

static const char * const gpu_parents[] = { "twpll-153m6", "twpll-192m",
						 "twpll-256m", "twpll-307m2",
						 "twpll-384m", "twpll-512m",
						 "gpll" };
static SPRD_COMP_CLK(gpu_core, "gpu-core", gpu_parents, 0x344,
		     0, 3, 8, 2, 0);
static SPRD_COMP_CLK(gpu_soc, "gpu-soc", gpu_parents, 0x348,
		     0, 3, 8, 2, 0);

static const char * const mm_ahb_parents[] = { "ext-26m", "twpll-96m",
					    "twpll-128m", "twpll-153m6"};
static SPRD_MUX_CLK(mm_ahb, "mm-ahb", mm_ahb_parents, 0x354,
			0, 2, SC9863A_MUX_FLAG);

static const char * const mm_vemc_parents[] = { "ext-26m", "twpll-307m2",
					    "twpll-384m", "isppll-468m"};
static SPRD_MUX_CLK(mm_vemc, "mm-vemc", mm_vemc_parents, 0x378,
			0, 2, SC9863A_MUX_FLAG);

static SPRD_MUX_CLK(mm_vahb, "mm-vahb", mm_ahb_parents, 0x37c,
			0, 2, SC9863A_MUX_FLAG);

static const char * const vsp_parents[] = { "twpll-76m8", "twpll-128m",
					    "twpll-256m", "twpll-307m2",
					    "twpll-384m"};
static SPRD_MUX_CLK(clk_vsp, "vsp", vsp_parents, 0x380,
			0, 3, SC9863A_MUX_FLAG);

static struct sprd_clk_common *sc9863a_ap_clks[] = {
	/* address base is 0x402d0000 */
	&aon_apb.common,
	&ap_axi.common,
	&sdio0_2x.common,
	&sdio1_2x.common,
	&sdio2_2x.common,
	&emmc_2x.common,
	&dpu_clk.common,
	&dpu_dpi.common,
	&gpu_core.common,
	&gpu_soc.common,
	&mm_ahb.common,
	&mm_vemc.common,
	&mm_vahb.common,
	&clk_vsp.common,
};

static struct clk_hw_onecell_data sc9863a_ap_clk_hws = {
	.hws	= {
		[CLK_FAC_RCO25M]	= &fac_rco_25m.hw,
		[CLK_FAC_RCO4M]		= &fac_rco_4m.hw,
		[CLK_FAC_RCO2M]		= &fac_rco_2m.hw,
		[CLK_AON_APB]		= &aon_apb.common.hw,
		[CLK_AP_AXI]		= &ap_axi.common.hw,
		[CLK_SDIO0_2X]		= &sdio0_2x.common.hw,
		[CLK_SDIO1_2X]		= &sdio1_2x.common.hw,
		[CLK_SDIO2_2X]		= &sdio2_2x.common.hw,
		[CLK_EMMC_2X]		= &emmc_2x.common.hw,
		[CLK_DPU]		= &dpu_clk.common.hw,
		[CLK_DPU_DPI]		= &dpu_dpi.common.hw,
		[CLK_GPU_CORE]		= &gpu_core.common.hw,
		[CLK_GPU_SOC]		= &gpu_soc.common.hw,
		[CLK_MM_AHB]		= &mm_ahb.common.hw,
		[CLK_MM_VEMC]		= &mm_vemc.common.hw,
		[CLK_MM_VAHB]		= &mm_vahb.common.hw,
		[CLK_VSP]		= &clk_vsp.common.hw,
	},
	.num	= CLK_AP_CLK_NUM,
};

static const struct sprd_clk_desc sc9863a_ap_clk_desc = {
	.clk_clks	= sc9863a_ap_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_ap_clks),
	.hw_clks	= &sc9863a_ap_clk_hws,
};

static SPRD_SC_GATE_CLK(otg_eb, "otg-eb", "ap-axi", 0x0, 0x1000,
			BIT(4), 0, 0);
static SPRD_SC_GATE_CLK(dma_eb, "dma-eb", "ap-axi", 0x0, 0x1000,
			BIT(5), 0, 0);
static SPRD_SC_GATE_CLK(ce_eb, "ce-eb", "ap-axi", 0x0, 0x1000,
			BIT(6), 0, 0);
static SPRD_SC_GATE_CLK(nandc_eb, "nandc-eb", "ap-axi", 0x0, 0x1000,
			BIT(7), 0, 0);
static SPRD_SC_GATE_CLK(sdio0_eb, "sdio0-eb", "ap-axi", 0x0, 0x1000,
			BIT(8), 0, 0);
static SPRD_SC_GATE_CLK(sdio1_eb, "sdio1-eb", "ap-axi", 0x0, 0x1000,
			BIT(9), 0, 0);
static SPRD_SC_GATE_CLK(sdio2_eb, "sdio2-eb", "ap-axi", 0x0, 0x1000,
			BIT(10), 0, 0);
static SPRD_SC_GATE_CLK(emmc_eb, "emmc-eb", "ap-axi", 0x0, 0x1000,
			BIT(11), 0, 0);
static SPRD_SC_GATE_CLK(emmc_32k_eb, "emmc-32k-eb", "ap-axi", 0x0, 0x1000,
			BIT(27), 0, 0);
static SPRD_SC_GATE_CLK(sdio0_32k_eb, "sdio0-32k-eb", "ap-axi", 0x0, 0x1000,
			BIT(28), 0, 0);
static SPRD_SC_GATE_CLK(sdio1_32k_eb, "sdio1-32k-eb", "ap-axi", 0x0, 0x1000,
			BIT(29), 0, 0);
static SPRD_SC_GATE_CLK(sdio2_32k_eb, "sdio2-32k-eb", "ap-axi", 0x0, 0x1000,
			BIT(30), 0, 0);
static SPRD_SC_GATE_CLK(nandc_26m_eb, "nandc-26m-eb", "ap-axi", 0x0, 0x1000,
			BIT(31), 0, 0);

static struct sprd_clk_common *sc9863a_apahb_gate_clks[] = {
	/* address base is 0x20e00000 */
	&otg_eb.common,
	&dma_eb.common,
	&ce_eb.common,
	&nandc_eb.common,
	&sdio0_eb.common,
	&sdio1_eb.common,
	&sdio2_eb.common,
	&emmc_eb.common,
	&emmc_32k_eb.common,
	&sdio0_32k_eb.common,
	&sdio1_32k_eb.common,
	&sdio2_32k_eb.common,
	&nandc_26m_eb.common
};

static struct clk_hw_onecell_data sc9863a_apahb_gate_hws = {
	.hws	= {
		[CLK_OTG_EB]		= &otg_eb.common.hw,
		[CLK_DMA_EB]		= &dma_eb.common.hw,
		[CLK_CE_EB]		= &ce_eb.common.hw,
		[CLK_NANDC_EB]		= &nandc_eb.common.hw,
		[CLK_SDIO0_EB]		= &sdio0_eb.common.hw,
		[CLK_SDIO1_EB]		= &sdio1_eb.common.hw,
		[CLK_SDIO2_EB]		= &sdio2_eb.common.hw,
		[CLK_EMMC_EB]		= &emmc_eb.common.hw,
		[CLK_EMMC_32K_EB]	= &emmc_32k_eb.common.hw,
		[CLK_SDIO0_32K_EB]	= &sdio0_32k_eb.common.hw,
		[CLK_SDIO1_32K_EB]	= &sdio1_32k_eb.common.hw,
		[CLK_SDIO2_32K_EB]	= &sdio2_32k_eb.common.hw,
		[CLK_NANDC_26M_EB]	= &nandc_26m_eb.common.hw,
	},
	.num	= CLK_AP_AHB_GATE_NUM,
};

static const struct sprd_clk_desc sc9863a_apahb_gate_desc = {
	.clk_clks	= sc9863a_apahb_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_apahb_gate_clks),
	.hw_clks	= &sc9863a_apahb_gate_hws,
};

static SPRD_SC_GATE_CLK(pmu_eb,		"pmu-eb",		"aon-apb",
			0x4, 0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm_eb,		"thm-eb",		"aon-apb",
			0x4, 0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux0_eb,	"aux0-eb",		"aon-apb",
			0x4, 0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux1_eb,	"aux1-eb",		"aon-apb",
			0x4, 0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux2_eb,	"aux2-eb",		"aon-apb",
			0x4, 0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(probe_eb,	"probe-eb",		"aon-apb",
			0x4, 0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emc_ref_eb,	"emc-ref-eb",		"aon-apb",
			0x4, 0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ca53_wdg_eb,	"ca53-wdg-eb",		"aon-apb",
			0x4, 0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr1_eb,	"ap-tmr1-eb",		"aon-apb",
			0x4, 0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr2_eb,	"ap-tmr2-eb",		"aon-apb",
			0x4, 0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(disp_emc_eb,	"disp-emc-eb",		"aon-apb",
			0x4, 0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(zip_emc_eb,	"zip-emc-eb",		"aon-apb",
			0x4, 0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gsp_emc_eb,	"gsp-emc-eb",		"aon-apb",
			0x4, 0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_vsp_eb,	"mm-vsp-eb",		"aon-apb",
			0x4, 0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mdar_eb,	"mdar-eb",		"aon-apb",
			0x4, 0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rtc4m0_cal_eb,	"rtc4m0-cal-eb",	"aon-apb",
			0x4, 0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rtc4m1_cal_eb,	"rtc4m1-cal-eb",	"aon-apb",
			0x4, 0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(djtag_eb,	"djtag-eb",		"aon-apb",
			0x4, 0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mbox_eb,	"mbox-eb",		"aon-apb",
			0x4, 0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_dma_eb,	"aon-dma-eb",		"aon-apb",
			0x4, 0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_apb_def_eb,	"aon-apb-def-eb",	"aon-apb",
			0x4, 0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(orp_jtag_eb,	"orp-jtag-eb",		"aon-apb",
			0x4, 0x1000, BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dbg_eb,		"dbg-eb",		"aon-apb",
			0x4, 0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dbg_emc_eb,	"dbg-emc-eb",		"aon-apb",
			0x4, 0x1000, BIT(29), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cross_trig_eb,	"cross-trig-eb",	"aon-apb",
			0x4, 0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(serdes_dphy_eb,	"serdes-dphy-eb",	"aon-apb",
			0x4, 0x1000, BIT(31), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gpu_eb,		"gpu-eb",	"aon-apb", 0x50,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(disp_eb,		"disp-eb",	"aon-apb", 0x50,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_emc_eb,		"mm-emc-eb",	"aon-apb", 0x50,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(power_cpu_eb,	"power-cpu-eb",	"aon-apb", 0x50,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(hw_i2c_eb,		"hw-i2c-eb",	"aon-apb", 0x50,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_vsp_emc_eb, "mm-vsp-emc-eb",	"aon-apb", 0x50,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vsp_eb,		"vsp-eb",	"aon-apb", 0x50,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *sc9863a_aonapb_gate_clks[] = {
	/* address base is 0x402e0000 */
	&pmu_eb.common,
	&thm_eb.common,
	&aux0_eb.common,
	&aux1_eb.common,
	&aux2_eb.common,
	&probe_eb.common,
	&emc_ref_eb.common,
	&ca53_wdg_eb.common,
	&ap_tmr1_eb.common,
	&ap_tmr2_eb.common,
	&disp_emc_eb.common,
	&zip_emc_eb.common,
	&gsp_emc_eb.common,
	&mm_vsp_eb.common,
	&mdar_eb.common,
	&rtc4m0_cal_eb.common,
	&rtc4m1_cal_eb.common,
	&djtag_eb.common,
	&mbox_eb.common,
	&aon_dma_eb.common,
	&aon_apb_def_eb.common,
	&orp_jtag_eb.common,
	&dbg_eb.common,
	&dbg_emc_eb.common,
	&cross_trig_eb.common,
	&serdes_dphy_eb.common,
	&gpu_eb.common,
	&disp_eb.common,
	&mm_emc_eb.common,
	&power_cpu_eb.common,
	&hw_i2c_eb.common,
	&mm_vsp_emc_eb.common,
	&vsp_eb.common,
};

static struct clk_hw_onecell_data sc9863a_aonapb_gate_hws = {
	.hws	= {
		[CLK_PMU_EB]		= &pmu_eb.common.hw,
		[CLK_THM_EB]		= &thm_eb.common.hw,
		[CLK_AUX0_EB]		= &aux0_eb.common.hw,
		[CLK_AUX1_EB]		= &aux1_eb.common.hw,
		[CLK_AUX2_EB]		= &aux2_eb.common.hw,
		[CLK_PROBE_EB]		= &probe_eb.common.hw,
		[CLK_EMC_REF_EB]	= &emc_ref_eb.common.hw,
		[CLK_CA53_WDG_EB]	= &ca53_wdg_eb.common.hw,
		[CLK_AP_TMR1_EB]	= &ap_tmr1_eb.common.hw,
		[CLK_AP_TMR2_EB]	= &ap_tmr2_eb.common.hw,
		[CLK_DISP_EMC_EB]	= &disp_emc_eb.common.hw,
		[CLK_ZIP_EMC_EB]	= &zip_emc_eb.common.hw,
		[CLK_GSP_EMC_EB]	= &gsp_emc_eb.common.hw,
		[CLK_MM_VSP_EB]		= &mm_vsp_eb.common.hw,
		[CLK_MDAR_EB]		= &mdar_eb.common.hw,
		[CLK_RTC4M0_CAL_EB]	= &rtc4m0_cal_eb.common.hw,
		[CLK_RTC4M1_CAL_EB]	= &rtc4m1_cal_eb.common.hw,
		[CLK_DJTAG_EB]		= &djtag_eb.common.hw,
		[CLK_MBOX_EB]		= &mbox_eb.common.hw,
		[CLK_AON_DMA_EB]	= &aon_dma_eb.common.hw,
		[CLK_AON_APB_DEF_EB]	= &aon_apb_def_eb.common.hw,
		[CLK_ORP_JTAG_EB]	= &orp_jtag_eb.common.hw,
		[CLK_DBG_EB]		= &dbg_eb.common.hw,
		[CLK_DBG_EMC_EB]	= &dbg_emc_eb.common.hw,
		[CLK_CROSS_TRIG_EB]	= &cross_trig_eb.common.hw,
		[CLK_SERDES_DPHY_EB]	= &serdes_dphy_eb.common.hw,
		[CLK_GNU_EB]		= &gpu_eb.common.hw,
		[CLK_DISP_EB]		= &disp_eb.common.hw,
		[CLK_MM_EMC_EB]		= &mm_emc_eb.common.hw,
		[CLK_POWER_CPU_EB]	= &power_cpu_eb.common.hw,
		[CLK_I2C_EB]		= &hw_i2c_eb.common.hw,
		[CLK_MM_VSP_EMC_EB]	= &mm_vsp_emc_eb.common.hw,
		[CLK_VSP_EB]		= &vsp_eb.common.hw,
	},
	.num	= CLK_AON_APB_GATE_NUM,
};

static const struct sprd_clk_desc sc9863a_aonapb_gate_desc = {
	.clk_clks	= sc9863a_aonapb_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_aonapb_gate_clks),
	.hw_clks	= &sc9863a_aonapb_gate_hws,
};

static SPRD_SC_GATE_CLK(vckg_eb, "vckg-eb", "mm-ahb", 0x0, 0x1000,
			BIT(0), 0, 0);
static SPRD_SC_GATE_CLK(vvsp_eb, "vvsp-eb", "mm-ahb", 0x0, 0x1000,
			BIT(1), 0, 0);
static SPRD_SC_GATE_CLK(vjpg_eb, "vjpg-eb", "mm-ahb", 0x0, 0x1000,
			BIT(2), 0, 0);
static SPRD_SC_GATE_CLK(vcpp_eb, "vcpp-eb", "mm-ahb", 0x0, 0x1000,
			BIT(3), 0, 0);

static struct sprd_clk_common *sc9863a_vspahb_gate_clks[] = {
	/* address base is 0x62000000 */
	&vckg_eb.common,
	&vvsp_eb.common,
	&vjpg_eb.common,
	&vcpp_eb.common,
};

static struct clk_hw_onecell_data sc9863a_vspahb_gate_hws = {
	.hws	= {
		[CLK_VCKG_EB]		= &vckg_eb.common.hw,
		[CLK_VVSP_EB]		= &vvsp_eb.common.hw,
		[CLK_VJPG_EB]		= &vjpg_eb.common.hw,
		[CLK_VCPP_EB]		= &vcpp_eb.common.hw,
	},
	.num	= CLK_VSP_AHB_GATE_NUM,
};

static const struct sprd_clk_desc sc9863a_vspahb_gate_desc = {
	.clk_clks	= sc9863a_vspahb_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(sc9863a_vspahb_gate_clks),
	.hw_clks	= &sc9863a_vspahb_gate_hws,
};

static const struct of_device_id sprd_sc9863a_clk_ids[] = {
	{ .compatible = "sprd,sc9863a-pmu-gate",	/* 0x402b0000 */
	  .data = &sc9863a_pmu_gate_desc },
	{ .compatible = "sprd,sc9863a-pll",	/* 0x40353000 */
	  .data = &sc9863a_pll_desc },
	{ .compatible = "sprd,sc9863a-mpll",	/* 0x40359000 */
	  .data = &sc9863a_mpll_desc },
	{ .compatible = "sprd,sc9863a-rpll",	/* 0x4035c000 */
	  .data = &sc9863a_rpll_desc },
	{ .compatible = "sprd,sc9863a-dpll",	/* 0x40363000 */
	  .data = &sc9863a_dpll_desc },
	{ .compatible = "sprd,sc9863a-ap-clk",	/* 0x402d0000 */
	  .data = &sc9863a_ap_clk_desc },
	{ .compatible = "sprd,sc9863a-apahb-gate",	/* 0x20e00000 */
	  .data = &sc9863a_apahb_gate_desc },
	{ .compatible = "sprd,sc9863a-aonapb-gate",	/* 0x402e0000 */
	  .data = &sc9863a_aonapb_gate_desc },
	{ .compatible = "sprd,sc9863a-vspahb-gate",	/* 0x62000000 */
	  .data = &sc9863a_vspahb_gate_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_sc9863a_clk_ids);

static int sc9863a_clk_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct sprd_clk_desc *desc;

	match = of_match_node(sprd_sc9863a_clk_ids, pdev->dev.of_node);
	if (!match) {
		pr_err("%s: of_match_node() failed", __func__);
		return -ENODEV;
	}

	desc = match->data;
	sprd_clk_regmap_init(pdev, desc);

	return sprd_clk_probe(&pdev->dev, desc->hw_clks);
}

static struct platform_driver sc9863a_clk_driver = {
	.probe	= sc9863a_clk_probe,
	.driver	= {
		.name	= "sc9863a-clk",
		.of_match_table	= sprd_sc9863a_clk_ids,
	},
};
module_platform_driver(sc9863a_clk_driver);

MODULE_DESCRIPTION("Spreadtrum SC9863A Clock Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sc9863a-clk");
