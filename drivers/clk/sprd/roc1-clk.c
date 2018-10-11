// SPDX-License-Identifier: GPL-2.0
//
// Spreatrum Roc1 clock driver
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

#include <dt-bindings/clock/sprd,roc1-clk.h>

#include "common.h"
#include "composite.h"
#include "div.h"
#include "gate.h"
#include "mux.h"
#include "pll.h"

/* ap ahb gates */
static SPRD_SC_GATE_CLK(dsi_eb,		"dsi-eb",	"ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dispc_eb,	"dispc-eb",	"ext-26m", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vsp_eb,		"vsp-eb",	"ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vdma_eb,	"vdma-eb",	"ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pub_dma_eb,	"pub-dma-eb",	"ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sec_dma_eb,	"sec-dma-eb",	"ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(icu_eb,		"icu-eb",	"ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apahb_ckg_eb,	"apahb-ckg-eb",	"ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(bm_clk_eb,	"bm-clk-eb",	"ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *roc1_apahb_gate[] = {
	/* address base is 0x20100000 */
	&dsi_eb.common,
	&dispc_eb.common,
	&vsp_eb.common,
	&vdma_eb.common,
	&pub_dma_eb.common,
	&sec_dma_eb.common,
	&icu_eb.common,
	&apahb_ckg_eb.common,
	&bm_clk_eb.common,
};

static struct clk_hw_onecell_data roc1_apahb_gate_hws = {
	.hws	= {
		[CLK_DSI_EB]		= &dsi_eb.common.hw,
		[CLK_DISPC_EB]		= &dispc_eb.common.hw,
		[CLK_VSP_EB]		= &vsp_eb.common.hw,
		[CLK_VDMA_EB]		= &vdma_eb.common.hw,
		[CLK_DMA_PUB_EB]	= &pub_dma_eb.common.hw,
		[CLK_DMA_SEC_EB]	= &sec_dma_eb.common.hw,
		[CLK_ICU_EB]		= &icu_eb.common.hw,
		[CLK_AP_AHB_CKG_EB]	= &apahb_ckg_eb.common.hw,
		[CLK_BUSM_CLK_EB]	= &bm_clk_eb.common.hw,
	},
	.num	= CLK_AP_AHB_GATE_NUM,
};

static const struct sprd_clk_desc roc1_apahb_gate_desc = {
	.clk_clks	= roc1_apahb_gate,
	.num_clk_clks	= ARRAY_SIZE(roc1_apahb_gate),
	.hw_clks	= &roc1_apahb_gate_hws,
};

/* aon apb gates */
static SPRD_SC_GATE_CLK(rc100m_cal_eb,	"rc100m-cal-eb",	"ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(djtag_tck_eb,	"djtag-tck-eb",		"ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(djtag_eb,	"djtag-eb",		"ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux0_eb,	"aux0-eb",	"ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux1_eb,	"aux1-eb",	"ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux2_eb,	"aux2-eb",	"ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(probe_eb,	"probe-eb",	"ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_eb,		"ipa-eb",	"ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_eb,		"mm-eb",	"ext-26m", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gpu_eb,		"gpu-eb",	"ext-26m", 0x0,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mspi_eb,	"mspi-eb",	"ext-26m", 0x0,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ai_eb,		"ai-eb",	"ext-26m", 0x0,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apcpu_dap_eb,	"apcpu-dap-eb",	"ext-26m", 0x0,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_cssys_eb,	"aon-cssys-eb",	"ext-26m", 0x0,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cssys_apb_eb,	"cssys-apb-eb",	"ext-26m", 0x0,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cssys_pub_eb,	"cssys-pub-eb",	"ext-26m", 0x0,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdphy_cfg_eb,	"sdphy-cfg-eb",	"ext-26m", 0x0,
		     0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdphy_ref_eb,	"sdphy-ref-eb",	"ext-26m", 0x0,
		     0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(efuse_eb,	"efuse-eb",	"ext-26m", 0x4,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gpio_eb,	"gpio-eb",	"ext-26m", 0x4,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mbox_eb,	"mbox-eb",	"ext-26m", 0x4,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(kpd_eb,		"kpd-eb",	"ext-26m", 0x4,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_syst_eb,	"aon-syst-eb",	"ext-26m", 0x4,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_syst_eb,	"ap-syst-eb",	"ext-26m", 0x4,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_tmr_eb,	"aon-tmr-eb",	"ext-26m", 0x4,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(otg_utmi_eb,	"otg-utmi-eb",	"ext-26m", 0x4,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(otg_phy_eb,	"otg-phy-eb",	"ext-26m", 0x4,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(splk_eb,	"splk-eb",	"ext-26m", 0x4,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pin_eb,		"pin-eb",	"ext-26m", 0x4,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ana_eb,		"ana-eb",	"ext-26m", 0x4,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aonapb_ckg_eb,	"aonapb-ckg-eb", "ext-26m", 0x4,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_i2c1_eb,	"aon-i2c1-eb",	"ext-26m", 0x4,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ufs_ao_eb,	"ufs-ao-eb",	"ext-26m", 0x4,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm3_eb,	"thm3-eb",	"ext-26m", 0x4,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apcpu_ts0_eb,	"apcpu-ts0-eb",	"ext-26m", 0x4,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(debug_filter_eb, "debug-filter-eb",	"ext-26m", 0x4,
		     0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_iis_eb, "aon-iis-eb", "ext-26m", 0x4,
		     0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(scc_eb, "scc-eb", "ext-26m", 0x4,
		     0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm0_eb,	"thm0-eb",	"ext-26m", 0x8,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm1_eb,	"thm1-eb",	"ext-26m", 0x8,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm2_eb,	"thm2-eb",	"ext-26m", 0x8,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(asim_top_eb,	"asim-top-eb",	"ext-26m", 0x8,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_i2c0_eb,	"aon-i2c0-eb",	"ext-26m", 0x8,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pmu_eb,		"pmu-eb",	"ext-26m", 0x8,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(adi_eb, "adi-eb", "ext-26m", 0x8,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_eb,		"eic-eb",	"ext-26m", 0x8,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc0_eb,	"ap-intc0-eb",	"ext-26m", 0x8,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc1_eb,	"ap-intc1-eb",	"ext-26m", 0x8,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc2_eb,	"ap-intc2-eb",	"ext-26m", 0x8,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc3_eb,	"ap-intc3-eb",	"ext-26m", 0x8,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc4_eb,	"ap-intc4-eb",	"ext-26m", 0x8,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc5_eb,	"ap-intc5-eb",	"ext-26m", 0x8,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_intc_eb,	"audcp-intc-eb",	"ext-26m", 0x8,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr0_eb,	"ap-tmr0-eb",	"ext-26m", 0x8,
			       0x1000, BIT(22), 0, 0);
static SPRD_SC_GATE_CLK(ap_tmr1_eb,	"ap-tmr1-eb",	"ext-26m", 0x8,
			       0x1000, BIT(23), 0, 0);
static SPRD_SC_GATE_CLK(ap_tmr2_eb,	"ap-tmr2-eb",	"ext-26m", 0x8,
			       0x1000, BIT(24), 0, 0);
static SPRD_SC_GATE_CLK(pwm0_eb,	"pwm0-eb",	"ext-26m", 0x8,
			       0x1000, BIT(25), 0, 0);
static SPRD_SC_GATE_CLK(pwm1_eb,	"pwm1-eb",	"ext-26m", 0x8,
			       0x1000, BIT(26), 0, 0);
static SPRD_SC_GATE_CLK(pwm2_eb,	"pwm2-eb",	"ext-26m", 0x8,
			       0x1000, BIT(27), 0, 0);
static SPRD_SC_GATE_CLK(pwm3_eb,	"pwm3-eb",	"ext-26m", 0x8,
			       0x1000, BIT(28), 0, 0);
static SPRD_SC_GATE_CLK(ap_wdg_eb,	"ap-wdg-eb",	"ext-26m", 0x8,
			       0x1000, BIT(29), 0, 0);
static SPRD_SC_GATE_CLK(apcpu_wdg_eb,	"apcpu-wdg-eb",	"ext-26m", 0x8,
			       0x1000, BIT(30), 0, 0);
static SPRD_SC_GATE_CLK(serdes_eb,	"serdes-eb",	"ext-26m", 0x8,
			       0x1000, BIT(31), 0, 0);
static SPRD_SC_GATE_CLK(arch_rtc_eb,	"arch-rtc-eb",	"ext-26m", 0x18,
			       0x1000, BIT(0), 0, 0);
static SPRD_SC_GATE_CLK(kpd_rtc_eb,	"kpd-rtc-eb",	"ext-26m", 0x18,
			       0x1000, BIT(1), 0, 0);
static SPRD_SC_GATE_CLK(aon_syst_rtc_eb, "aon-syst-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(2), 0, 0);
static SPRD_SC_GATE_CLK(ap_syst_rtc_eb,	"ap-syst-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(3), 0, 0);
static SPRD_SC_GATE_CLK(aon_tmr_rtc_eb,	"aon-tmr-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(4), 0, 0);
static SPRD_SC_GATE_CLK(eic_rtc_eb,	"eic-rtc-eb",	"ext-26m", 0x18,
			       0x1000, BIT(5), 0, 0);
static SPRD_SC_GATE_CLK(eic_rtcdv5_eb,	"eic-rtcdv5-eb", "ext-26m", 0x18,
			       0x1000, BIT(6), 0, 0);
static SPRD_SC_GATE_CLK(ap_wdg_rtc_eb,	"ap-wdg-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(7), 0, 0);
static SPRD_SC_GATE_CLK(ac_wdg_rtc_eb,	"ac-wdg-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(8), 0, 0);
static SPRD_SC_GATE_CLK(ap_tmr0_rtc_eb,	"ap-tmr0-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(9), 0, 0);
static SPRD_SC_GATE_CLK(ap_tmr1_rtc_eb,	"ap-tmr1-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(10), 0, 0);
static SPRD_SC_GATE_CLK(ap_tmr2_rtc_eb,	"ap-tmr2-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(11), 0, 0);
static SPRD_SC_GATE_CLK(dcxo_lc_rtc_eb,	"dcxo-lc-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(12), 0, 0);
static SPRD_SC_GATE_CLK(bb_cal_rtc_eb,	"bb-cal-rtc-eb", "ext-26m", 0x18,
			       0x1000, BIT(13), 0, 0);
static SPRD_SC_GATE_CLK(dsi_csi_test_eb, "dsi-csi-test-eb", "ext-26m", 0x138,
			       0x1000, BIT(8), 0, 0);
static SPRD_SC_GATE_CLK(djtag_tck_en, "djtag-tck-en", "ext-26m", 0x138,
			       0x1000, BIT(9), 0, 0);
static SPRD_SC_GATE_CLK(dphy_ref_eb, "dphy-ref-eb", "ext-26m", 0x138,
			       0x1000, BIT(10), 0, 0);
static SPRD_SC_GATE_CLK(dmc_ref_eb, "dmc-ref-eb", "ext-26m", 0x138,
			       0x1000, BIT(11), 0, 0);
static SPRD_SC_GATE_CLK(otg_ref_eb, "otg-ref-eb", "ext-26m", 0x138,
			       0x1000, BIT(12), 0, 0);
static SPRD_SC_GATE_CLK(tsen_eb, "tsen-eb", "ext-26m", 0x138,
			       0x1000, BIT(13), 0, 0);
static SPRD_SC_GATE_CLK(tmr_eb, "tmr-eb", "ext-26m", 0x138,
			       0x1000, BIT(14), 0, 0);
static SPRD_SC_GATE_CLK(rc100m_ref_eb, "rc100m-ref-eb", "ext-26m", 0x138,
			       0x1000, BIT(15), 0, 0);
static SPRD_SC_GATE_CLK(rc100m_fdk_eb, "rc100m-fdk-eb", "ext-26m", 0x138,
			       0x1000, BIT(16), 0, 0);
static SPRD_SC_GATE_CLK(debounce_eb, "debounce-eb", "ext-26m", 0x138,
			       0x1000, BIT(17), 0, 0);
static SPRD_SC_GATE_CLK(det_32k_eb, "det-32k-eb", "ext-26m", 0x138,
			       0x1000, BIT(18), 0, 0);
static SPRD_SC_GATE_CLK(usb_suspend_en, "usb_suspend_en", "ext-26m", 0x138,
			       0x1000, BIT(22), 0, 0);
static SPRD_SC_GATE_CLK(ufs_ao_en, "ufs_ao_en", "ext-26m", 0x138,
			       0x1000, BIT(23), 0, 0);
static SPRD_SC_GATE_CLK(top_cssys_en, "top-cssys-en", "ext-26m", 0x13c,
			       0x1000, BIT(0), 0, 0);
static SPRD_SC_GATE_CLK(ap_axi_en, "ap-axi-en", "ext-26m", 0x13c,
			       0x1000, BIT(1), 0, 0);
static SPRD_SC_GATE_CLK(sdio0_2x_en, "sdio0-2x-en", "ext-26m", 0x13c,
			       0x1000, BIT(2), 0, 0);
static SPRD_SC_GATE_CLK(sdio0_1x_en, "sdio0-1x-en", "ext-26m", 0x13c,
			       0x1000, BIT(3), 0, 0);
static SPRD_SC_GATE_CLK(sdio1_2x_en, "sdio1-2x-en", "ext-26m", 0x13c,
			       0x1000, BIT(4), 0, 0);
static SPRD_SC_GATE_CLK(sdio1_1x_en, "sdio1-1x-en", "ext-26m", 0x13c,
			       0x1000, BIT(5), 0, 0);
static SPRD_SC_GATE_CLK(sdio2_2x_en, "sdio2-2x-en", "ext-26m", 0x13c,
			       0x1000, BIT(6), 0, 0);
static SPRD_SC_GATE_CLK(sdio2_1x_en, "sdio2-1x-en", "ext-26m", 0x13c,
			       0x1000, BIT(7), 0, 0);
static SPRD_SC_GATE_CLK(emmc_2x_en, "emmc-2x-en", "ext-26m", 0x13c,
			       0x1000, BIT(8), 0, 0);
static SPRD_SC_GATE_CLK(emmc_1x_en, "emmc-1x-en", "ext-26m", 0x13c,
			       0x1000, BIT(9), 0, 0);
static SPRD_SC_GATE_CLK(pll_test_en, "pll-test-en", "ext-26m", 0x13c,
			       0x1000, BIT(14), 0, 0);
static SPRD_SC_GATE_CLK(cphy_cfg_en, "cphy-cfg-en", "ext-26m", 0x13c,
			       0x1000, BIT(15), 0, 0);
static SPRD_SC_GATE_CLK(aapc_clk_test_en, "aapc-clk-test-en", "ext-26m", 0x13c,
			       0x1000, BIT(16), 0, 0);
static SPRD_SC_GATE_CLK(ese_efuse_ctrl_en, "ese_efuse_ctrl_en", "ext-26m", 0x13c,
			       0x1000, BIT(17), 0, 0);
static SPRD_SC_GATE_CLK(debug_ts_en, "debug-ts-en", "ext-26m", 0x13c,
			       0x1000, BIT(18), 0, 0);

static struct sprd_clk_common *roc1_aon_gate[] = {
	/* address base is 0x327d0000 */
	&rc100m_cal_eb.common,
	&djtag_tck_eb.common,
	&djtag_eb.common,
	&aux0_eb.common,
	&aux1_eb.common,
	&aux2_eb.common,
	&probe_eb.common,
	&ipa_eb.common,
	&mm_eb.common,
	&gpu_eb.common,
	&mspi_eb.common,
	&ai_eb.common,
	&apcpu_dap_eb.common,
	&aon_cssys_eb.common,
	&cssys_apb_eb.common,
	&cssys_pub_eb.common,
	&sdphy_cfg_eb.common,
	&sdphy_ref_eb.common,
	&efuse_eb.common,
	&gpio_eb.common,
	&mbox_eb.common,
	&kpd_eb.common,
	&aon_syst_eb.common,
	&ap_syst_eb.common,
	&aon_tmr_eb.common,
	&otg_utmi_eb.common,
	&otg_phy_eb.common,
	&splk_eb.common,
	&pin_eb.common,
	&ana_eb.common,
	&aonapb_ckg_eb.common,
	&aon_i2c1_eb.common,
	&ufs_ao_eb.common,
	&thm3_eb.common,
	&apcpu_ts0_eb.common,
	&debug_filter_eb.common,
	&aon_iis_eb.common,
	&scc_eb.common,
	&thm0_eb.common,
	&thm1_eb.common,
	&thm2_eb.common,
	&asim_top_eb.common,
	&aon_i2c0_eb.common,
	&pmu_eb.common,
	&adi_eb.common,
	&eic_eb.common,
	&ap_intc0_eb.common,
	&ap_intc1_eb.common,
	&ap_intc2_eb.common,
	&ap_intc3_eb.common,
	&ap_intc4_eb.common,
	&ap_intc5_eb.common,
	&audcp_intc_eb.common,
	&ap_tmr0_eb.common,
	&ap_tmr1_eb.common,
	&ap_tmr2_eb.common,
	&pwm0_eb.common,
	&pwm1_eb.common,
	&pwm2_eb.common,
	&pwm3_eb.common,
	&ap_wdg_eb.common,
	&apcpu_wdg_eb.common,
	&serdes_eb.common,
	&arch_rtc_eb.common,
	&kpd_rtc_eb.common,
	&aon_syst_rtc_eb.common,
	&ap_syst_rtc_eb.common,
	&aon_tmr_rtc_eb.common,
	&eic_rtc_eb.common,
	&eic_rtcdv5_eb.common,
	&ap_wdg_rtc_eb.common,
	&ac_wdg_rtc_eb.common,
	&ap_tmr0_rtc_eb.common,
	&ap_tmr1_rtc_eb.common,
	&ap_tmr2_rtc_eb.common,
	&dcxo_lc_rtc_eb.common,
	&bb_cal_rtc_eb.common,
	&dsi_csi_test_eb.common,
	&djtag_tck_en.common,
	&dphy_ref_eb.common,
	&dmc_ref_eb.common,
	&otg_ref_eb.common,
	&tsen_eb.common,
	&tmr_eb.common,
	&rc100m_ref_eb.common,
	&rc100m_fdk_eb.common,
	&debounce_eb.common,
	&det_32k_eb.common,
	&usb_suspend_en.common,
	&ufs_ao_en.common,
	&top_cssys_en.common,
	&ap_axi_en.common,
	&sdio0_2x_en.common,
	&sdio0_1x_en.common,
	&sdio1_2x_en.common,
	&sdio1_1x_en.common,
	&sdio2_2x_en.common,
	&sdio2_1x_en.common,
	&emmc_2x_en.common,
	&emmc_1x_en.common,
	&pll_test_en.common,
	&cphy_cfg_en.common,
	&aapc_clk_test_en.common,
	&ese_efuse_ctrl_en.common,
	&debug_ts_en.common,
};

static struct clk_hw_onecell_data roc1_aon_gate_hws = {
	.hws	= {
		[CLK_RC100M_CAL_EB]	= &rc100m_cal_eb.common.hw,
		[CLK_DJTAG_TCK_EB]	= &djtag_tck_eb.common.hw,
		[CLK_DJTAG_EB]		= &djtag_eb.common.hw,
		[CLK_AUX0_EB]		= &aux0_eb.common.hw,
		[CLK_AUX1_EB]		= &aux1_eb.common.hw,
		[CLK_AUX2_EB]		= &aux2_eb.common.hw,
		[CLK_PROBE_EB]		= &probe_eb.common.hw,
		[CLK_IPA_EB]		= &ipa_eb.common.hw,
		[CLK_MM_EB]		= &mm_eb.common.hw,
		[CLK_GPU_EB]		= &gpu_eb.common.hw,
		[CLK_MSPI_EB]		= &mspi_eb.common.hw,
		[CLK_AI_EB]		= &ai_eb.common.hw,
		[CLK_APCPU_DAP_EB]	= &apcpu_dap_eb.common.hw,
		[CLK_AON_CSSYS_EB]	= &aon_cssys_eb.common.hw,
		[CLK_CSSYS_APB_EB]	= &cssys_apb_eb.common.hw,
		[CLK_CSSYS_PUB_EB]	= &cssys_pub_eb.common.hw,
		[CLK_SDPHY_CFG_EB]	= &sdphy_cfg_eb.common.hw,
		[CLK_SDPHY_REF_EB]	= &sdphy_ref_eb.common.hw,
		[CLK_EFUSE_EB]		= &efuse_eb.common.hw,
		[CLK_GPIO_EB]		= &gpio_eb.common.hw,
		[CLK_MBOX_EB]		= &mbox_eb.common.hw,
		[CLK_KPD_EB]		= &kpd_eb.common.hw,
		[CLK_AON_SYST_EB]	= &aon_syst_eb.common.hw,
		[CLK_AP_SYST_EB]	= &ap_syst_eb.common.hw,
		[CLK_AON_TMR_EB]	= &aon_tmr_eb.common.hw,
		[CLK_OTG_UTMI_EB]	= &otg_utmi_eb.common.hw,
		[CLK_OTG_PHY_EB]	= &otg_phy_eb.common.hw,
		[CLK_SPLK_EB]		= &splk_eb.common.hw,
		[CLK_PIN_EB]		= &pin_eb.common.hw,
		[CLK_ANA_EB]		= &ana_eb.common.hw,
		[CLK_AON_APB_CKG_EB]	= &aonapb_ckg_eb.common.hw,
		[CLK_AON_I2C1_EB]	= &aon_i2c1_eb.common.hw,
		[CLK_UFS_AO_EB]		= &ufs_ao_eb.common.hw,
		[CLK_THM3_EB]		= &thm3_eb.common.hw,
		[CLK_APCPU_TS0_EB]	= &apcpu_ts0_eb.common.hw,
		[CLK_DEBUG_FILTER_EB]	= &debug_filter_eb.common.hw,
		[CLK_AON_IIS_EB]	= &aon_iis_eb.common.hw,
		[CLK_SCC_EB]		= &scc_eb.common.hw,
		[CLK_THM0_EB]		= &thm0_eb.common.hw,
		[CLK_THM1_EB]		= &thm1_eb.common.hw,
		[CLK_THM2_EB]		= &thm2_eb.common.hw,
		[CLK_ASIM_TOP_EB]	= &asim_top_eb.common.hw,
		[CLK_AON_I2C0_EB]	= &aon_i2c0_eb.common.hw,
		[CLK_PMU_EB]		= &pmu_eb.common.hw,
		[CLK_ADI_EB]		= &adi_eb.common.hw,
		[CLK_EIC_EB]		= &eic_eb.common.hw,
		[CLK_AP_INTC0_EB]	= &ap_intc0_eb.common.hw,
		[CLK_AP_INTC1_EB]	= &ap_intc1_eb.common.hw,
		[CLK_AP_INTC2_EB]	= &ap_intc2_eb.common.hw,
		[CLK_AP_INTC3_EB]	= &ap_intc3_eb.common.hw,
		[CLK_AP_INTC4_EB]	= &ap_intc4_eb.common.hw,
		[CLK_AP_INTC5_EB]	= &ap_intc5_eb.common.hw,
		[CLK_AUDCP_INTC_EB]	= &audcp_intc_eb.common.hw,
		[CLK_AP_TMR0_EB]	= &ap_tmr0_eb.common.hw,
		[CLK_AP_TMR1_EB]	= &ap_tmr1_eb.common.hw,
		[CLK_AP_TMR2_EB]	= &ap_tmr2_eb.common.hw,
		[CLK_PWM0_EB]		= &pwm0_eb.common.hw,
		[CLK_PWM1_EB]		= &pwm1_eb.common.hw,
		[CLK_PWM2_EB]		= &pwm2_eb.common.hw,
		[CLK_PWM3_EB]		= &pwm3_eb.common.hw,
		[CLK_AP_WDG_EB]		= &ap_wdg_eb.common.hw,
		[CLK_APCPU_WDG_EB]	= &apcpu_wdg_eb.common.hw,
		[CLK_SERDES_EB]		= &serdes_eb.common.hw,
		[CLK_ARCH_RTC_EB]	= &arch_rtc_eb.common.hw,
		[CLK_KPD_RTC_EB]	= &kpd_rtc_eb.common.hw,
		[CLK_AON_SYST_RTC_EB]	= &aon_syst_rtc_eb.common.hw,
		[CLK_AP_SYST_RTC_EB]	= &ap_syst_rtc_eb.common.hw,
		[CLK_AON_TMR_RTC_EB]	= &aon_tmr_rtc_eb.common.hw,
		[CLK_EIC_RTC_EB]	= &eic_rtc_eb.common.hw,
		[CLK_EIC_RTCDV5_EB]	= &eic_rtcdv5_eb.common.hw,
		[CLK_AP_WDG_RTC_EB]	= &ap_wdg_rtc_eb.common.hw,
		[CLK_AC_WDG_RTC_EB]	= &ac_wdg_rtc_eb.common.hw,
		[CLK_AP_TMR0_RTC_EB]	= &ap_tmr0_rtc_eb.common.hw,
		[CLK_AP_TMR1_RTC_EB]	= &ap_tmr1_rtc_eb.common.hw,
		[CLK_AP_TMR2_RTC_EB]	= &ap_tmr2_rtc_eb.common.hw,
		[CLK_DCXO_LC_RTC_EB]	= &dcxo_lc_rtc_eb.common.hw,
		[CLK_BB_CAL_RTC_EB]	= &bb_cal_rtc_eb.common.hw,
		[CLK_DSI_CSI_TEST_EB]	= &dsi_csi_test_eb.common.hw,
		[CLK_DJTAG_TCK_EN]	= &djtag_tck_en.common.hw,
		[CLK_DPHY_REF_EN]	= &dphy_ref_eb.common.hw,
		[CLK_DMC_REF_EN]	= &dmc_ref_eb.common.hw,
		[CLK_OTG_REF_EN]	= &otg_ref_eb.common.hw,
		[CLK_TSEN_EN]		= &tsen_eb.common.hw,
		[CLK_TMR_EN]		= &tmr_eb.common.hw,
		[CLK_RC100M_REF_EN]	= &rc100m_ref_eb.common.hw,
		[CLK_RC100M_FDK_EN]	= &rc100m_fdk_eb.common.hw,
		[CLK_DEBOUNCE_EN]	= &debounce_eb.common.hw,
		[CLK_DET_32K_EB]	= &det_32k_eb.common.hw,
		[CLK_USB_SUSPEND_EN]	= &usb_suspend_en.common.hw,
		[CLK_UFS_AO_EN]		= &ufs_ao_en.common.hw,
		[CLK_TOP_CSSYS_EN]	= &top_cssys_en.common.hw,
		[CLK_AP_AXI_EN]		= &ap_axi_en.common.hw,
		[CLK_SDIO0_2X_EN]	= &sdio0_2x_en.common.hw,
		[CLK_SDIO0_1X_EN]	= &sdio0_1x_en.common.hw,
		[CLK_SDIO1_2X_EN]	= &sdio1_2x_en.common.hw,
		[CLK_SDIO1_1X_EN]	= &sdio1_1x_en.common.hw,
		[CLK_SDIO2_2X_EN]	= &sdio2_2x_en.common.hw,
		[CLK_SDIO2_1X_EN]	= &sdio2_1x_en.common.hw,
		[CLK_EMMC_2X_EN]	= &emmc_2x_en.common.hw,
		[CLK_EMMC_1X_EN]	= &emmc_1x_en.common.hw,
		[CLK_PLL_TEST_EN]	= &pll_test_en.common.hw,
		[CLK_CPHY_CFG_EN]	= &cphy_cfg_en.common.hw,
		[CLK_AAPC_CLK_TEST_EN]	= &aapc_clk_test_en.common.hw,
		[CLK_ESE_EFUSE_CTRL_EN]	= &ese_efuse_ctrl_en.common.hw,
		[CLK_DEBUG_TS_EN]	= &debug_ts_en.common.hw,
	},
	.num	= CLK_AON_APB_GATE_NUM,
};

static const struct sprd_clk_desc roc1_aon_gate_desc = {
	.clk_clks	= roc1_aon_gate,
	.num_clk_clks	= ARRAY_SIZE(roc1_aon_gate),
	.hw_clks	= &roc1_aon_gate_hws,
};

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
static SPRD_SC_GATE_CLK(ce_sec_eb,	"ce-sec-eb",	"ext-26m", 0x0,
		     0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ce_pub_eb,	"ce-pub-eb",	"ext-26m", 0x0,
		     0x1000, BIT(31), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *roc1_apapb_gate[] = {
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
	&ce_sec_eb.common,
	&ce_pub_eb.common,
};

static struct clk_hw_onecell_data roc1_apapb_gate_hws = {
	.hws	= {
		[CLK_SIM0_EB]		= &sim0_eb.common.hw,
		[CLK_IIS0_EB]		= &iis0_eb.common.hw,
		[CLK_IIS1_EB]		= &iis1_eb.common.hw,
		[CLK_IIS2_EB]		= &iis2_eb.common.hw,
		[CLK_AP_APB_REG_EB]	= &apb_reg_eb.common.hw,
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
		[CLK_SIM0_32K_DET]	= &sim0_32k_eb.common.hw,
		[CLK_SPI0_LF_IN_EB]	= &spi0_lfin_eb.common.hw,
		[CLK_SPI1_LF_IN_EB]	= &spi1_lfin_eb.common.hw,
		[CLK_SPI2_LF_IN_EB]	= &spi2_lfin_eb.common.hw,
		[CLK_SPI3_LF_IN_EB]	= &spi3_lfin_eb.common.hw,
		[CLK_SDIO0_EB]		= &sdio0_eb.common.hw,
		[CLK_SDIO1_EB]		= &sdio1_eb.common.hw,
		[CLK_SDIO1_EB]		= &sdio2_eb.common.hw,
		[CLK_EMMC_EB]		= &emmc_eb.common.hw,
		[CLK_CE_SEC_EB]		= &ce_sec_eb.common.hw,
		[CLK_CE_PUB_EB]		= &ce_pub_eb.common.hw,
	},
	.num	= CLK_AP_APB_GATE_NUM,
};

static const struct sprd_clk_desc roc1_apapb_gate_desc = {
	.clk_clks	= roc1_apapb_gate,
	.num_clk_clks	= ARRAY_SIZE(roc1_apapb_gate),
	.hw_clks	= &roc1_apapb_gate_hws,
};

static const struct of_device_id sprd_roc1_clk_ids[] = {
	{ .compatible = "sprd,roc1-apahb-gate",	/* 0x20100000 */
	  .data = &roc1_apahb_gate_desc },
	{ .compatible = "sprd,roc1-aon-gate",	/* 0x327d0000 */
	  .data = &roc1_aon_gate_desc },
	{ .compatible = "sprd,roc1-apapb-gate",	/* 0x71000000 */
	  .data = &roc1_apapb_gate_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_roc1_clk_ids);

static int roc1_clk_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct sprd_clk_desc *desc;

	match = of_match_node(sprd_roc1_clk_ids, pdev->dev.of_node);
	if (!match) {
		pr_err("%s: of_match_node() failed", __func__);
		return -ENODEV;
	}

	desc = match->data;
	sprd_clk_regmap_init(pdev, desc);

	return sprd_clk_probe(&pdev->dev, desc->hw_clks);
}

static struct platform_driver roc1_clk_driver = {
	.probe	= roc1_clk_probe,
	.driver	= {
		.name	= "roc1-clk",
		.of_match_table	= sprd_roc1_clk_ids,
	},
};
module_platform_driver(roc1_clk_driver);

MODULE_DESCRIPTION("Spreadtrum Roc1 Clock Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:roc1-clk");
