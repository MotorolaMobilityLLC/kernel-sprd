// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
//
// Spreadtrum SC9860 platform clocks
//
// Copyright (C) 2017, Spreadtrum Communications Inc.

#ifndef _DT_BINDINGS_CLK_SC9860_H_
#define _DT_BINDINGS_CLK_SC9860_H_

#define	CLK_MPLL0_GATE		0
#define	CLK_DPLL0_GATE		1
#define	CLK_LPLL_GATE		2
#define	CLK_GPLL_GATE		3
#define	CLK_DPLL1_GATE		4
#define	CLK_MPLL1_GATE		5
#define	CLK_MPLL2_GATE		6
#define CLK_ISPPLL_GATE		7
#define	CLK_PMU_APB_NUM		(CLK_ISPPLL_GATE + 1)

#define	CLK_AUDIO_GATE		0
#define CLK_RPLL		1
#define CLK_RPLL_390M		2
#define CLK_RPLL_260M		3
#define CLK_RPLL_195M		4
#define CLK_RPLL_26M		5
#define CLK_ANLG_PHY_G5_NUM	(CLK_RPLL_26M + 1)

#define CLK_TWPLL		0
#define CLK_TWPLL_768M		1
#define CLK_TWPLL_384M		2
#define CLK_TWPLL_192M		3
#define CLK_TWPLL_96M		4
#define CLK_TWPLL_48M		5
#define CLK_TWPLL_24M		6
#define CLK_TWPLL_12M		7
#define CLK_TWPLL_512M		8
#define CLK_TWPLL_256M		9
#define CLK_TWPLL_128M		10
#define CLK_TWPLL_64M		11
#define CLK_TWPLL_307M2		12
#define CLK_TWPLL_219M4		13
#define CLK_TWPLL_170M6		14
#define CLK_TWPLL_153M6		15
#define CLK_TWPLL_76M8		16
#define CLK_TWPLL_51M2		17
#define CLK_TWPLL_38M4		18
#define CLK_TWPLL_19M2		19
#define CLK_LPLL		20
#define CLK_LPLL_409M6		21
#define CLK_LPLL_245M76		22
#define CLK_GPLL		23
#define CLK_ISPPLL		24
#define CLK_ISPPLL_468M		25
#define CLK_ANLG_PHY_G1_NUM	(CLK_ISPPLL_468M + 1)

#define CLK_DPLL0		0
#define CLK_DPLL1		1
#define CLK_DPLL0_933M		2
#define	CLK_DPLL0_622M3		3
#define CLK_DPLL0_400M		4
#define CLK_DPLL0_266M7		5
#define CLK_DPLL0_123M1		6
#define CLK_DPLL0_50M		7
#define CLK_ANLG_PHY_G7_NUM	(CLK_DPLL0_50M + 1)

#define CLK_MPLL0		0
#define CLK_MPLL1		1
#define CLK_MPLL2		2
#define CLK_MPLL2_675M		3
#define CLK_ANLG_PHY_G4_NUM	(CLK_MPLL2_675M + 1)

#define CLK_FAC_RCO25M		0
#define CLK_FAC_RCO4M		1
#define CLK_FAC_RCO2M		2
#define CLK_AON_APB		3
#define CLK_AP_AXI		4
#define CLK_SDIO0_2X		5
#define CLK_SDIO1_2X		6
#define CLK_SDIO2_2X		7
#define CLK_EMMC_2X		8
#define CLK_DPU			9
#define CLK_DPU_DPI		10
#define CLK_GPU_CORE		11
#define CLK_GPU_SOC		12
#define CLK_MM_AHB		13
#define CLK_MM_VEMC		14
#define CLK_MM_VAHB		15
#define CLK_VSP			16
#define CLK_AP_CLK_NUM		(CLK_VSP + 1)

#define CLK_OTG_EB		0
#define CLK_DMA_EB		1
#define CLK_CE_EB		2
#define CLK_NANDC_EB		3
#define CLK_SDIO0_EB		4
#define CLK_SDIO1_EB		5
#define CLK_SDIO2_EB		6
#define CLK_EMMC_EB		7
#define CLK_EMMC_32K_EB		8
#define CLK_SDIO0_32K_EB	9
#define CLK_SDIO1_32K_EB	10
#define CLK_SDIO2_32K_EB	11
#define CLK_NANDC_26M_EB	12
#define CLK_AP_AHB_GATE_NUM	(CLK_NANDC_26M_EB + 1)

#define CLK_PMU_EB		0
#define CLK_THM_EB		1
#define CLK_AUX0_EB		2
#define CLK_AUX1_EB		3
#define CLK_AUX2_EB		4
#define CLK_PROBE_EB		5
#define CLK_EMC_REF_EB		6
#define CLK_CA53_WDG_EB		7
#define CLK_AP_TMR1_EB		8
#define CLK_AP_TMR2_EB		9
#define CLK_DISP_EMC_EB		10
#define CLK_ZIP_EMC_EB		11
#define CLK_GSP_EMC_EB		12
#define CLK_MM_VSP_EB		13
#define CLK_MDAR_EB		14
#define CLK_RTC4M0_CAL_EB	15
#define CLK_RTC4M1_CAL_EB	16
#define CLK_DJTAG_EB		17
#define CLK_MBOX_EB		18
#define CLK_AON_DMA_EB		19
#define CLK_AON_APB_DEF_EB	20
#define CLK_ORP_JTAG_EB		21
#define CLK_DBG_EB		22
#define CLK_DBG_EMC_EB		23
#define CLK_CROSS_TRIG_EB	24
#define CLK_SERDES_DPHY_EB	25
#define CLK_GNU_EB		26
#define CLK_DISP_EB		27
#define CLK_MM_EMC_EB		28
#define CLK_POWER_CPU_EB	29
#define CLK_I2C_EB		30
#define CLK_MM_VSP_EMC_EB	31
#define CLK_VSP_EB		32
#define CLK_AON_APB_GATE_NUM	(CLK_VSP_EB + 1)

#define CLK_VCKG_EB		0
#define CLK_VVSP_EB		1
#define CLK_VJPG_EB		2
#define CLK_VCPP_EB		3
#define CLK_VSP_AHB_GATE_NUM	(CLK_VCPP_EB + 1)

#endif /* _DT_BINDINGS_CLK_SC9860_H_ */
