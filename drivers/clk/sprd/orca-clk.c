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

#include <dt-bindings/clock/sprd,orca-clk.h>

#include "common.h"
#include "composite.h"
#include "div.h"
#include "gate.h"
#include "mux.h"
#include "pll.h"

/* ap ahb gates */
static SPRD_SC_GATE_CLK(apahb_ckg_eb, "apahb-ckg-eb", "ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nandc_eb, "nandc-eb", "ext-26m", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nandc_ecc_eb, "nandc-ecc-eb", "ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nandc_26m_eb, "nandc-26m-eb", "ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dma_eb, "dma-eb", "ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dma_eb2, "dma-eb2", "ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(usb0_eb, "usb0-eb", "ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(usb0_suspend_eb, "usb0-suspend-eb", "ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(usb0_ref_eb, "usb0-ref-eb", "ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio_mst_eb, "sdio-mst-eb", "ext-26m", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio_mst_32k_eb, "sdio-mst-32k-eb", "ext-26m", 0x0,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_eb, "emmc-eb", "ext-26m", 0x0,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_32k_eb, "emmc-32k-eb", "ext-26m", 0x0,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *orca_apahb_gate[] = {
	/* address base is 0x21000000 */
	&apahb_ckg_eb.common,
	&nandc_eb.common,
	&nandc_ecc_eb.common,
	&nandc_26m_eb.common,
	&dma_eb.common,
	&dma_eb2.common,
	&usb0_eb.common,
	&usb0_suspend_eb.common,
	&usb0_ref_eb.common,
	&sdio_mst_eb.common,
	&sdio_mst_32k_eb.common,
	&emmc_eb.common,
	&emmc_32k_eb.common,
};

static struct clk_hw_onecell_data orca_apahb_gate_hws = {
	.hws	= {
		[CLK_APAHB_CKG_EB] = &apahb_ckg_eb.common.hw,
		[CLK_NANDC_EB] = &nandc_eb.common.hw,
		[CLK_NANDC_ECC_EB] = &nandc_ecc_eb.common.hw,
		[CLK_NANDC_26M_EB] = &nandc_26m_eb.common.hw,
		[CLK_DMA_EB] = &dma_eb.common.hw,
		[CLK_DMA_EB2] = &dma_eb2.common.hw,
		[CLK_USB0_EB] = &usb0_eb.common.hw,
		[CLK_USB0_SUSPEND_EB] = &usb0_suspend_eb.common.hw,
		[CLK_USB0_REF_EB] = &usb0_ref_eb.common.hw,
		[CLK_SDIO_MST_EB] = &sdio_mst_eb.common.hw,
		[CLK_SDIO_MST_32K_EB] = &sdio_mst_32k_eb.common.hw,
		[CLK_EMMC_EB] = &emmc_eb.common.hw,
		[CLK_EMMC_32K_EB] = &emmc_32k_eb.common.hw,
	},
	.num	= CLK_AP_AHB_GATE_NUM,
};

static const struct sprd_clk_desc orca_apahb_gate_desc = {
	.clk_clks	= orca_apahb_gate,
	.num_clk_clks	= ARRAY_SIZE(orca_apahb_gate),
	.hw_clks	= &orca_apahb_gate_hws,
};

/* ap apb gates */
static SPRD_SC_GATE_CLK(apapb_reg_eb, "apapb-reg-eb", "ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_uart0_eb, "ap-uart0-eb", "ext-26m", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_i2c0_eb, "ap-i2c0-eb", "ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_i2c1_eb, "ap-i2c1-eb", "ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_i2c2_eb, "ap-i2c2-eb", "ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_i2c3_eb2, "ap-i2c3-eb2", "ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_i2c4_eb, "ap-i2c4-eb", "ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_apb_spi0_eb, "ap-apb-spi0-eb", "ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi0_lf_in_eb, "spi0-lf-in-eb", "ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_apb_spi1_eb, "ap-apb-spi1-eb", "ext-26m", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi1_lf_in_eb, "spi1-lf-in-eb", "ext-26m", 0x0,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_apb_spi2_eb, "ap-apb-spi2-eb", "ext-26m", 0x0,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi2_lf_in_eb, "spi2-lf-in-eb", "ext-26m", 0x0,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm0_eb, "pwm0-eb", "ext-26m", 0x0,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm1_eb, "pwm1-eb", "ext-26m", 0x0,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm2_eb, "pwm2-eb", "ext-26m", 0x0,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm3_eb, "pwm3-eb", "ext-26m", 0x0,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sim0_eb, "sim0-eb", "ext-26m", 0x0,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sim0_32k_eb, "sim0-32k-eb", "ext-26m", 0x0,
		     0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *orca_apapb_gate[] = {
	/* address base is 0x24000000 */
	&apapb_reg_eb.common,
	&ap_uart0_eb.common,
	&ap_i2c0_eb.common,
	&ap_i2c1_eb.common,
	&ap_i2c2_eb.common,
	&ap_i2c3_eb2.common,
	&ap_i2c4_eb.common,
	&ap_apb_spi0_eb.common,
	&spi0_lf_in_eb.common,
	&ap_apb_spi1_eb.common,
	&spi1_lf_in_eb.common,
	&ap_apb_spi2_eb.common,
	&spi2_lf_in_eb.common,
	&pwm0_eb.common,
	&pwm1_eb.common,
	&pwm2_eb.common,
	&pwm3_eb.common,
	&sim0_eb.common,
	&sim0_32k_eb.common,
};

static struct clk_hw_onecell_data orca_apapb_gate_hws = {
	.hws	= {
		[CLK_APAPB_REG_EB] = &apapb_reg_eb.common.hw,
		[CLK_AP_UART0_EB] = &ap_uart0_eb.common.hw,
		[CLK_AP_I2C0_EB] = &ap_i2c0_eb.common.hw,
		[CLK_AP_I2C1_EB] = &ap_i2c1_eb.common.hw,
		[CLK_AP_I2C2_EB] = &ap_i2c2_eb.common.hw,
		[CLK_AP_I2C3_EB] = &ap_i2c3_eb2.common.hw,
		[CLK_AP_I2C4_EB] = &ap_i2c4_eb.common.hw,
		[CLK_AP_APB_SPI0_EB] = &ap_apb_spi0_eb.common.hw,
		[CLK_SPI0_LF_IN_EB] = &spi0_lf_in_eb.common.hw,
		[CLK_AP_APB_SPI1_EB] = &ap_apb_spi1_eb.common.hw,
		[CLK_SPI1_IF_IN_EB] = &spi1_lf_in_eb.common.hw,
		[CLK_AP_APB_SPI2_EB] = &ap_apb_spi2_eb.common.hw,
		[CLK_SPI2_IF_IN_EB] = &spi2_lf_in_eb.common.hw,
		[CLK_PWM0_EB] = &pwm0_eb.common.hw,
		[CLK_PWM1_EB] = &pwm1_eb.common.hw,
		[CLK_PWM2_EB] = &pwm2_eb.common.hw,
		[CLK_PWM3_EB] = &pwm3_eb.common.hw,
		[CLK_SIM0_EB] = &sim0_eb.common.hw,
		[CLK_SIM0_32K_EB] = &sim0_32k_eb.common.hw,
	},
	.num	= CLK_AP_APB_GATE_NUM,
};

static const struct sprd_clk_desc orca_apapb_gate_desc = {
	.clk_clks	= orca_apapb_gate,
	.num_clk_clks	= ARRAY_SIZE(orca_apapb_gate),
	.hw_clks	= &orca_apapb_gate_hws,
};

/* aon gates */
static SPRD_SC_GATE_CLK(rc100_cal_eb, "rc100-cal-eb", "ext-26m", 0x0,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_spi_eb, "aon-spi-eb", "ext-26m", 0x0,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(djtag_tck_eb, "djtag-tck-eb", "ext-26m", 0x0,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(djtag_eb, "djtag-eb", "ext-26m", 0x0,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux0_eb, "aux0-eb", "ext-26m", 0x0,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux1_eb, "aux1-eb", "ext-26m", 0x0,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux2_eb, "aux2-eb", "ext-26m", 0x0,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(probe_eb, "probe-eb", "ext-26m", 0x0,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(bsm_tmr_eb, "bsm-tmr-eb", "ext-26m", 0x0,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_apb_bm_eb, "aon-apb-bm-eb", "ext-26m", 0x0,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pmu_apb_bm_eb, "pmu-apb-bm-eb", "ext-26m", 0x0,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apcpu_cssys_eb, "apcpu-cssys-eb", "ext-26m", 0x0,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(debug_filter_eb, "debug-filter-eb", "ext-26m", 0x0,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apcpu_dap_eb, "apcpu-dap-eb", "ext-26m", 0x0,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cssys_eb, "cssys-eb", "ext-26m", 0x0,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cssys_apb_eb, "cssys-apb-eb", "ext-26m", 0x0,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cssys_pub_eb, "cssys-pub-eb", "ext-26m", 0x0,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sd0_cfg_eb, "sd0-cfg-eb", "ext-26m", 0x0,
		     0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sd0_ref_eb, "sd0-ref-eb", "ext-26m", 0x0,
		     0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sd1_cfg_eb, "sd1-cfg-eb", "ext-26m", 0x0,
		     0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sd1_ref_eb, "sd1-ref-eb", "ext-26m", 0x0,
		     0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sd2_cfg_eb, "sd2-cfg-eb", "ext-26m", 0x0,
		     0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sd2_ref_eb, "sd2-ref-eb", "ext-26m", 0x0,
		     0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(serdes0_eb, "serdes0-eb", "ext-26m", 0x0,
		     0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(serdes1_eb, "serdes1-eb", "ext-26m", 0x0,
		     0x1000, BIT(26), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(serdes2_eb, "serdes2-eb", "ext-26m", 0x0,
		     0x1000, BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rtm_eb, "rtm-eb", "ext-26m", 0x0,
		     0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rtm_atb_eb, "rtm-atb-eb", "ext-26m", 0x0,
		     0x1000, BIT(29), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_nr_spi_eb, "aon-nr-spi-eb", "ext-26m", 0x0,
		     0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_bm_s5_eb, "aon-bm-s5-eb", "ext-26m", 0x0,
		     0x1000, BIT(31), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK(efuse_eb, "efuse-eb", "ext-26m", 0x4,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gpio_eb, "gpio-eb", "ext-26m", 0x4,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mbox_eb, "mbox-eb", "ext-26m", 0x4,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(kpd_eb, "kpd-eb", "ext-26m", 0x4,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_syst_eb, "aon-syst-eb", "ext-26m", 0x4,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_syst_eb, "ap-syst-eb", "ext-26m", 0x4,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_tmr_eb, "aon-tmr-eb", "ext-26m", 0x4,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dvfs_top_eb, "dvfs-top-eb", "ext-26m", 0x4,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apcpu_clk_eb, "apcpu-clk-rf-eb", "ext-26m", 0x4,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(splk_eb, "splk-eb", "ext-26m", 0x4,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pin_eb, "pin-eb", "ext-26m", 0x4,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ana_eb, "ana-eb", "ext-26m", 0x4,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_ckg_eb, "aon-ckg-eb", "ext-26m", 0x4,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(djtag_ctrl_eb, "djtag-ctrl-eb", "ext-26m", 0x4,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apcpu_ts0_eb, "apcpu-ts0-eb", "ext-26m", 0x4,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nic400_aon_eb, "nic400-aon-eb", "ext-26m", 0x4,
		     0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(scc_eb, "scc-eb", "ext-26m", 0x4,
		     0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_spi0_eb, "ap-spi0-eb", "ext-26m", 0x4,
		     0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_spi1_eb, "ap-spi1-eb", "ext-26m", 0x4,
		     0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_spi2_eb, "ap-spi2-eb", "ext-26m", 0x4,
		     0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_bm_s3_eb, "aon-bm-s3-eb", "ext-26m", 0x4,
		     0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sc_cc_eb, "sc-cc-eb", "ext-26m", 0x4,
		     0x1000, BIT(31), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK(thm0_eb, "thm0-eb", "ext-26m", 0x8,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm1_eb, "thm1-eb", "ext-26m", 0x8,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_sim_eb, "ap-sim-eb", "ext-26m", 0x8,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_i2c_eb, "aon-i2c-eb", "ext-26m", 0x8,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pmu_eb, "pmu-eb", "ext-26m", 0x8,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(adi_eb, "adi-eb", "ext-26m", 0x8,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_eb, "eic-eb", "ext-26m", 0x8,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc0_eb, "ap-intc0-eb", "ext-26m", 0x8,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc1_eb, "ap-intc1-eb", "ext-26m", 0x8,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc2_eb, "ap-intc2-eb", "ext-26m", 0x8,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc3_eb, "ap-intc3-eb", "ext-26m", 0x8,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc4_eb, "ap-intc4-eb", "ext-26m", 0x8,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc5_eb, "ap-intc5-eb", "ext-26m", 0x8,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_intc_eb, "audcp-intc-eb", "ext-26m", 0x8,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr0_eb, "ap-tmr0-eb", "ext-26m", 0x8,
		     0x1000, BIT(25), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr1_eb, "ap-tmr1-eb", "ext-26m", 0x8,
		     0x1000, BIT(26), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr2_eb, "ap-tmr2-eb", "ext-26m", 0x8,
		     0x1000, BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_wdg_eb, "ap-wdg-eb", "ext-26m", 0x8,
		     0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apcpu_wdg_eb, "apcpu-wdg-eb", "ext-26m", 0x8,
		     0x1000, BIT(29), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm2_eb, "thm2-eb", "ext-26m", 0x8,
		     0x1000, BIT(30), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK(arch_rtc_eb, "arch-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(kpd_rtc_eb, "kpd-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_syst_rtc_eb, "aon-syst-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_syst_rtc_eb, "ap-syst-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_tmr_rtc_eb, "aon-tmr-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_rtc_eb, "eic-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_rtcdv5_eb, "eic-rtcdv5-eb", "ext-26m", 0xc,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_wdg_rtc_eb, "ap-wdg-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ac_wdg_rtc_eb, "ac-wdg-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr0_rtc_eb, "ap-tmr0-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr1_rtc_eb, "ap-tmr1-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr2_rtc_eb, "ap-tmr2-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dcxo_lc_rtc_eb, "dcxo-lc-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(bb_cal_rtc_eb, "bb-cal-rtc-eb", "ext-26m", 0xc,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK(dsi0_test_eb, "dsi0-test-eb", "ext-26m", 0x20,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dsi1_test_eb, "dsi1-test-eb", "ext-26m", 0x20,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dsi2_test_eb, "dsi2-test-eb", "ext-26m", 0x20,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dmc_ref_en, "dmc-ref-eb", "ext-26m", 0x20,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(tsen_en, "tsen-en", "ext-26m", 0x20,
		     0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(tmr_en, "tmr-en", "ext-26m", 0x20,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rc100_ref_en, "rc100-ref-en", "ext-26m", 0x20,
		     0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rc100_fdk_en, "rc100-fdk-en", "ext-26m", 0x20,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(debounce_en, "debounce-en", "ext-26m", 0x20,
		     0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(det_32k_eb, "det-32k-eb", "ext-26m", 0x20,
		     0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);

static SPRD_SC_GATE_CLK(cssys_en, "cssys-en", "ext-26m", 0x24,
		     0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_2x_en, "sdio0-2x-en", "ext-26m", 0x24,
		     0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_1x_en, "sdio0-1x-en", "ext-26m", 0x24,
		     0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio1_2x_en, "sdio1-2x-en", "ext-26m", 0x24,
		     0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio1_1x_en, "sdio1-1x-en", "ext-26m", 0x24,
		     0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio2_2x_en, "sdio2-2x-en", "ext-26m", 0x24,
		     0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio2_1x_en, "sdio2-1x-en", "ext-26m", 0x24,
		     0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_1x_en, "emmc-1x-en", "ext-26m", 0x24,
		     0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(emmc_2x_en, "emmc-2x-en", "ext-26m", 0x24,
		     0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nandc_1x_en, "nandc-1x-en", "ext-26m", 0x24,
		     0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nandc_2x_en, "nandc-2x-en", "ext-26m", 0x24,
		     0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(all_pll_test_eb, "all-pll-test-eb", "ext-26m", 0x24,
		     0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aapc_test_eb, "aapc-test-eb", "ext-26m", 0x24,
		     0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(debug_ts_eb, "debug-ts-eb", "ext-26m", 0x24,
		     0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *orca_aon_gate[] = {
	/* address base is 0x64020000 */
	&rc100_cal_eb.common,
	&aon_spi_eb.common,
	&djtag_tck_eb.common,
	&djtag_eb.common,
	&aux0_eb.common,
	&aux1_eb.common,
	&aux2_eb.common,
	&probe_eb.common,
	&bsm_tmr_eb.common,
	&aon_apb_bm_eb.common,
	&pmu_apb_bm_eb.common,
	&apcpu_cssys_eb.common,
	&debug_filter_eb.common,
	&apcpu_dap_eb.common,
	&cssys_eb.common,
	&cssys_apb_eb.common,
	&cssys_pub_eb.common,
	&sd0_cfg_eb.common,
	&sd0_ref_eb.common,
	&sd1_cfg_eb.common,
	&sd1_ref_eb.common,
	&sd2_cfg_eb.common,
	&sd2_ref_eb.common,
	&serdes0_eb.common,
	&serdes1_eb.common,
	&serdes2_eb.common,
	&rtm_eb.common,
	&rtm_atb_eb.common,
	&aon_nr_spi_eb.common,
	&aon_bm_s5_eb.common,
	&efuse_eb.common,
	&gpio_eb.common,
	&mbox_eb.common,
	&kpd_eb.common,
	&aon_syst_eb.common,
	&ap_syst_eb.common,
	&aon_tmr_eb.common,
	&dvfs_top_eb.common,
	&apcpu_clk_eb.common,
	&splk_eb.common,
	&pin_eb.common,
	&ana_eb.common,
	&aon_ckg_eb.common,
	&djtag_ctrl_eb.common,
	&apcpu_ts0_eb.common,
	&nic400_aon_eb.common,
	&scc_eb.common,
	&ap_spi0_eb.common,
	&ap_spi1_eb.common,
	&ap_spi2_eb.common,
	&aon_bm_s3_eb.common,
	&sc_cc_eb.common,
	&thm0_eb.common,
	&thm1_eb.common,
	&ap_sim_eb.common,
	&aon_i2c_eb.common,
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
	&ap_wdg_eb.common,
	&apcpu_wdg_eb.common,
	&thm2_eb.common,
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
	&dsi0_test_eb.common,
	&dsi1_test_eb.common,
	&dsi2_test_eb.common,
	&dmc_ref_en.common,
	&tsen_en.common,
	&tmr_en.common,
	&rc100_ref_en.common,
	&rc100_fdk_en.common,
	&debounce_en.common,
	&det_32k_eb.common,
	&cssys_en.common,
	&sdio0_2x_en.common,
	&sdio0_1x_en.common,
	&sdio1_2x_en.common,
	&sdio1_1x_en.common,
	&sdio2_2x_en.common,
	&sdio2_1x_en.common,
	&emmc_1x_en.common,
	&emmc_2x_en.common,
	&nandc_1x_en.common,
	&nandc_2x_en.common,
	&all_pll_test_eb.common,
	&aapc_test_eb.common,
	&debug_ts_eb.common,
};

static struct clk_hw_onecell_data orca_aon_gate_hws = {
	.hws	= {
		[CLK_RC100_CAL_EB] = &rc100_cal_eb.common.hw,
		[CLK_AON_SPI_EB] = &aon_spi_eb.common.hw,
		[CLK_DJTAG_TCK_EB] = &djtag_tck_eb.common.hw,
		[CLK_DJTAG_EB] = &djtag_eb.common.hw,
		[CLK_AUX0_EB] = &aux0_eb.common.hw,
		[CLK_AUX1_EB] = &aux1_eb.common.hw,
		[CLK_AUX2_EB] = &aux2_eb.common.hw,
		[CLK_PROBE_EB] = &probe_eb.common.hw,
		[CLK_BSM_TMR_EB] = &bsm_tmr_eb.common.hw,
		[CLK_AON_APB_BM_EB] = &aon_apb_bm_eb.common.hw,
		[CLK_PMU_APB_BM_EB] = &pmu_apb_bm_eb.common.hw,
		[CLK_APCPU_CSSYS_EB] = &apcpu_cssys_eb.common.hw,
		[CLK_DEBUG_FILTER_EB] = &debug_filter_eb.common.hw,
		[CLK_APCPU_DAP_EB] = &apcpu_dap_eb.common.hw,
		[CLK_CSSYS_EB] = &cssys_eb.common.hw,
		[CLK_CSSYS_APB_EB] = &cssys_apb_eb.common.hw,
		[CLK_CSSYS_PUB_EB] = &cssys_pub_eb.common.hw,
		[CLK_SD0_CFG_EB] = &sd0_cfg_eb.common.hw,
		[CLK_SD0_REF_EB] = &sd0_ref_eb.common.hw,
		[CLK_SD1_CFG_EB] = &sd1_cfg_eb.common.hw,
		[CLK_SD1_REF_EB] = &sd1_ref_eb.common.hw,
		[CLK_SD2_CFG_EB] = &sd2_cfg_eb.common.hw,
		[CLK_SD2_REF_EB] = &sd2_ref_eb.common.hw,
		[CLK_SERDES0_EB] = &serdes0_eb.common.hw,
		[CLK_SERDES1_EB] = &serdes1_eb.common.hw,
		[CLK_SERDES2_EB] = &serdes2_eb.common.hw,
		[CLK_RTM_EB] = &rtm_eb.common.hw,
		[CLK_RTM_ATB_EB] = &rtm_atb_eb.common.hw,
		[CLK_AON_NR_SPI_EB] = &aon_nr_spi_eb.common.hw,
		[CLK_AON_BM_S5_EB] = &aon_bm_s5_eb.common.hw,
		[CLK_EFUSE_EB] = &efuse_eb.common.hw,
		[CLK_GPIO_EB] = &gpio_eb.common.hw,
		[CLK_MBOX_EB] = &mbox_eb.common.hw,
		[CLK_KPD_EB] = &kpd_eb.common.hw,
		[CLK_AON_SYST_EB] = &aon_syst_eb.common.hw,
		[CLK_AP_SYST_EB] = &ap_syst_eb.common.hw,
		[CLK_AON_TMR_EB] = &aon_tmr_eb.common.hw,
		[CLK_DVFS_TOP_EB] = &dvfs_top_eb.common.hw,
		[CLK_APCPU_CLK_EB] = &apcpu_clk_eb.common.hw,
		[CLK_SPLK_EB] = &splk_eb.common.hw,
		[CLK_PIN_EB] = &pin_eb.common.hw,
		[CLK_ANA_EB] = &ana_eb.common.hw,
		[CLK_AON_CKG_EB] = &aon_ckg_eb.common.hw,
		[CLK_DJTAG_CTRL_EB] = &djtag_ctrl_eb.common.hw,
		[CLK_APCPU_TS0_EB] = &apcpu_ts0_eb.common.hw,
		[CLK_NIC400_AON_EB] = &nic400_aon_eb.common.hw,
		[CLK_SCC_EB] = &scc_eb.common.hw,
		[CLK_AP_SPI0_EB] = &ap_spi0_eb.common.hw,
		[CLK_AP_SPI1_EB] = &ap_spi1_eb.common.hw,
		[CLK_AP_SPI2_EB] = &ap_spi2_eb.common.hw,
		[CLK_AON_BM_S3_EB] = &aon_bm_s3_eb.common.hw,
		[CLK_SC_CC_EB] = &sc_cc_eb.common.hw,
		[CLK_THM0_EB] = &thm0_eb.common.hw,
		[CLK_THM1_EB] = &thm1_eb.common.hw,
		[CLK_AP_SIM_EB] = &ap_sim_eb.common.hw,
		[CLK_AON_I2C_EB] = &aon_i2c_eb.common.hw,
		[CLK_PMU_EB] = &pmu_eb.common.hw,
		[CLK_ADI_EB] = &adi_eb.common.hw,
		[CLK_EIC_EB] = &eic_eb.common.hw,
		[CLK_AP_INTC0_EB] = &ap_intc0_eb.common.hw,
		[CLK_AP_INTC1_EB] = &ap_intc1_eb.common.hw,
		[CLK_AP_INTC2_EB] = &ap_intc2_eb.common.hw,
		[CLK_AP_INTC3_EB] = &ap_intc3_eb.common.hw,
		[CLK_AP_INTC4_EB] = &ap_intc4_eb.common.hw,
		[CLK_AP_INTC5_EB] = &ap_intc5_eb.common.hw,
		[CLK_AUDCP_INTC_EB] = &audcp_intc_eb.common.hw,
		[CLK_AP_TMR0_EB] = &ap_tmr0_eb.common.hw,
		[CLK_AP_TMR1_EB] = &ap_tmr1_eb.common.hw,
		[CLK_AP_TMR2_EB] = &ap_tmr2_eb.common.hw,
		[CLK_AP_WDG_EB] = &ap_wdg_eb.common.hw,
		[CLK_APCPU_WDG_EB] = &apcpu_wdg_eb.common.hw,
		[CLK_THM2_EB] = &thm2_eb.common.hw,
		[CLK_ARCH_RTC_EB] = &arch_rtc_eb.common.hw,
		[CLK_KPD_RTC_EB] = &kpd_rtc_eb.common.hw,
		[CLK_AON_SYST_RTC_EB] = &aon_syst_rtc_eb.common.hw,
		[CLK_AP_SYST_RTC_EB] = &ap_syst_rtc_eb.common.hw,
		[CLK_AON_TMR_RTC_EB] = &aon_tmr_rtc_eb.common.hw,
		[CLK_EIC_RTC_EB] = &eic_rtc_eb.common.hw,
		[CLK_EIC_RTCDV5_EB] = &eic_rtcdv5_eb.common.hw,
		[CLK_AP_WDG_RTC_EB] = &ap_wdg_rtc_eb.common.hw,
		[CLK_AC_WDG_RTC_EB] = &ac_wdg_rtc_eb.common.hw,
		[CLK_AP_TMR0_RTC_EB] = &ap_tmr0_rtc_eb.common.hw,
		[CLK_AP_TMR1_RTC_EB] = &ap_tmr1_rtc_eb.common.hw,
		[CLK_AP_TMR2_RTC_EB] = &ap_tmr2_rtc_eb.common.hw,
		[CLK_DCXO_LC_RTC_EB] = &dcxo_lc_rtc_eb.common.hw,
		[CLK_BB_CAL_RTC_EB] = &bb_cal_rtc_eb.common.hw,
		[CLK_DSI0_TEST_EB] = &dsi0_test_eb.common.hw,
		[CLK_DSI1_TEST_EB] = &dsi1_test_eb.common.hw,
		[CLK_DSI2_TEST_EB] = &dsi2_test_eb.common.hw,
		[CLK_DMC_REF_EN] = &dmc_ref_en.common.hw,
		[CLK_TSEN_EN] = &tsen_en.common.hw,
		[CLK_TMR_EN] = &tmr_en.common.hw,
		[CLK_RC100_REF_EN] = &rc100_ref_en.common.hw,
		[CLK_RC100_FDK_EN] = &rc100_fdk_en.common.hw,
		[CLK_DEBOUNCE_EN] = &debounce_en.common.hw,
		[CLK_DET_32K_EB] = &det_32k_eb.common.hw,
		[CLK_CSSYS_EN] = &cssys_en.common.hw,
		[CLK_SDIO0_2X_EN] = &sdio0_2x_en.common.hw,
		[CLK_SDIO0_1X_EN] = &sdio0_1x_en.common.hw,
		[CLK_SDIO1_2X_EN] = &sdio1_2x_en.common.hw,
		[CLK_SDIO1_1X_EN] = &sdio1_1x_en.common.hw,
		[CLK_SDIO2_2X_EN] = &sdio2_2x_en.common.hw,
		[CLK_SDIO2_1X_EN] = &sdio2_1x_en.common.hw,
		[CLK_EMMC_1X_EN] = &emmc_1x_en.common.hw,
		[CLK_EMMC_2X_EN] = &emmc_2x_en.common.hw,
		[CLK_NANDC_1X_EN] = &nandc_1x_en.common.hw,
		[CLK_NANDC_2X_EN] = &nandc_2x_en.common.hw,
		[CLK_ALL_PLL_TEST_EB] = &all_pll_test_eb.common.hw,
		[CLK_AAPC_TEST_EB] = &aapc_test_eb.common.hw,
		[CLK_DEBUG_TS_EB] = &debug_ts_eb.common.hw,
	},
	.num	= CLK_AON_GATE_NUM,
};

static const struct sprd_clk_desc orca_aon_gate_desc = {
	.clk_clks	= orca_aon_gate,
	.num_clk_clks	= ARRAY_SIZE(orca_aon_gate),
	.hw_clks	= &orca_aon_gate_hws,
};

static const struct of_device_id sprd_orca_clk_ids[] = {
	{ .compatible = "sprd,orca-apahb-gate",	/* 0x21000000 */
	  .data = &orca_apahb_gate_desc },
	{ .compatible = "sprd,orca-apapb-gate",	/* 0x24000000 */
	  .data = &orca_apapb_gate_desc },
	{ .compatible = "sprd,orca-aon-gate",	/* 0x64020000 */
	  .data = &orca_aon_gate_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_orca_clk_ids);

static int orca_clk_probe(struct platform_device *pdev)
{
	const struct sprd_clk_desc *desc;

	desc = of_device_get_match_data(&pdev->dev);
	if (!desc) {
		pr_err("%s: Cannot find matching driver data!\n", __func__);
		return -EINVAL;
	}
	sprd_clk_regmap_init(pdev, desc);

	return sprd_clk_probe(&pdev->dev, desc->hw_clks);
}

static struct platform_driver orca_clk_driver = {
	.probe	= orca_clk_probe,
	.driver	= {
		.name	= "orca-clk",
		.of_match_table	= sprd_orca_clk_ids,
	},
};
module_platform_driver(orca_clk_driver);

MODULE_DESCRIPTION("Spreadtrum Orca Clock Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:orca-clk");
