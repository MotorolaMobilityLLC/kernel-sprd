// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
//
// Spreadtrum Sharkl5 platform clocks
//
// Copyright (C) 2018, Spreadtrum Communications Inc.

#ifndef _DT_BINDINGS_CLK_SHARKL5PRO_H_
#define _DT_BINDINGS_CLK_SHARKL5PRO_H_

#define CLK_DSI_EB			0
#define CLK_DISPC_EB			1
#define CLK_VSP_EB			2
#define CLK_VDMA_EB			3
#define CLK_DMA_PUB_EB			4
#define CLK_DMA_SEC_EB			5
#define CLK_IPI_EB			6
#define CLK_AHB_CKG_EB			7
#define CLK_BM_CLK_EB			8
#define CLK_AP_AHB_GATE_NUM		(CLK_BM_CLK_EB + 1)

#define CLK_RC100M_CAL_EB		0
#define CLK_DJTAG_TCK_EB		1
#define CLK_DJTAG_EB			2
#define CLK_AUX0_EB			3
#define CLK_AUX1_EB			4
#define CLK_AUX2_EB			5
#define CLK_PROBE_EB			6
#define CLK_MM_EB			7
#define CLK_GPU_EB			8
#define CLK_MSPI_EB			9
#define CLK_APCPU_DAP_EB		10
#define CLK_AON_CSSYS_EB		11
#define CLK_CSSYS_APB_EB		12
#define CLK_CSSYS_PUB_EB		13
#define CLK_SDPHY_CFG_EB		14
#define CLK_SDPHY_REF_EB		15
#define CLK_EFUSE_EB			16
#define CLK_GPIO_EB			17
#define CLK_MBOX_EB			18
#define CLK_KPD_EB			19
#define CLK_AON_SYST_EB			20
#define CLK_AP_SYST_EB			21
#define CLK_AON_TMR_EB			22
#define CLK_OTG_UTMI_EB			23
#define CLK_OTG_PHY_EB			24
#define CLK_SPLK_EB			25
#define CLK_PIN_EB			26
#define CLK_ANA_EB			27
#define CLK_APCPU_TS0_EB		28
#define CLK_APB_BUSMON_EB		29
#define CLK_AON_IIS_EB			30
#define CLK_SCC_EB			31
#define CLK_THM0_EB			32
#define CLK_THM1_EB			33
#define CLK_THM2_EB			34
#define CLK_ASIM_TOP_EB			35
#define CLK_I2C_EB			36
#define CLK_PMU_EB			37
#define CLK_ADI_EB			38
#define CLK_EIC_EB			39
#define CLK_AP_INTC0_EB			40
#define CLK_AP_INTC1_EB			41
#define CLK_AP_INTC2_EB			42
#define CLK_AP_INTC3_EB			43
#define CLK_AP_INTC4_EB			44
#define CLK_AP_INTC5_EB			45
#define CLK_AUDCP_INTC_EB		46
#define CLK_AP_TMR0_EB			47
#define CLK_AP_TMR1_EB			48
#define CLK_AP_TMR2_EB			49
#define CLK_PWM0_EB			50
#define CLK_PWM1_EB			51
#define CLK_PWM2_EB			52
#define CLK_PWM3_EB			53
#define CLK_AP_WDG_EB			54
#define CLK_APCPU_WDG_EB		55
#define CLK_SERDES_EB			56
#define CLK_ARCH_RTC_EB			57
#define CLK_KPD_RTC_EB			58
#define CLK_AON_SYST_RTC_EB		59
#define CLK_AP_SYST_RTC_EB		60
#define CLK_AON_TMR_RTC_EB		61
#define CLK_EIC_RTC_EB			62
#define CLK_EIC_RTCDV5_EB		63
#define CLK_AP_WDG_RTC_EB		64
#define CLK_AC_WDG_RTC_EB		65
#define CLK_AP_TMR0_RTC_EB		66
#define CLK_AP_TMR1_RTC_EB		67
#define CLK_AP_TMR2_RTC_EB		68
#define CLK_DCXO_LC_RTC_EB		69
#define CLK_BB_CAL_RTC_EB		70
#define CLK_AP_EMMC_RTC_EB		71
#define CLK_AP_SDIO0_RTC_EB		72
#define CLK_AP_SDIO1_RTC_EB		73
#define CLK_AP_SDIO2_RTC_EB		74
#define CLK_DSI_CSI_TEST_EB		75
#define CLK_DJTAG_TCK_EN		76
#define CLK_DPHY_REF_EB			77
#define CLK_DMC_REF_EB			78
#define CLK_OTG_REF_EB			79
#define CLK_TSEN_EB			80
#define CLK_TMR_EB			81
#define CLK_RC100M_REF_EB		82
#define CLK_RC100M_FDK_EB		83
#define CLK_DEBOUNCE_EB			84
#define CLK_DET_32K_EB			85
#define CLK_TOP_CSSYS_EB		86
#define CLK_AP_AXI_EN			87
#define CLK_SDIO0_2X_EN			88
#define CLK_SDIO0_1X_EN			89
#define CLK_SDIO1_2X_EN			90
#define CLK_SDIO1_1X_EN			91
#define CLK_SDIO2_2X_EN			92
#define CLK_SDIO2_1X_EN			93
#define CLK_EMMC_2X_EN			94
#define CLK_EMMC_1X_EN			95
#define CLK_PLL_TEST_EN			96
#define CLK_CPHY_CFG_EN			97
#define CLK_DEBUG_TS_EN			98
#define CLK_AON_APB_GATE_NUM		(CLK_DEBUG_TS_EN + 1)

#define CLK_SIM0_EB			0
#define CLK_IIS0_EB			1
#define CLK_IIS1_EB			2
#define CLK_IIS2_EB			3
#define CLK_APB_REG_EB			4
#define CLK_SPI0_EB			5
#define CLK_SPI1_EB			6
#define CLK_SPI2_EB			7
#define CLK_SPI3_EB			8
#define CLK_I2C0_EB			9
#define CLK_I2C1_EB			10
#define CLK_I2C2_EB			11
#define CLK_I2C3_EB			12
#define CLK_I2C4_EB			13
#define CLK_UART0_EB			14
#define CLK_UART1_EB			15
#define CLK_UART2_EB			16
#define CLK_SIM0_32K_EB			17
#define CLK_SPI0_LFIN_EB		18
#define CLK_SPI1_LFIN_EB		19
#define CLK_SPI2_LFIN_EB		20
#define CLK_SPI3_LFIN_EB		21
#define CLK_SDIO0_EB			22
#define CLK_SDIO1_EB			23
#define CLK_SDIO2_EB			24
#define CLK_EMMC_EB			25
#define CLK_SDIO0_32K_EB		26
#define CLK_SDIO1_32K_EB		27
#define CLK_SDIO2_32K_EB		28
#define CLK_EMMC_32K_EB			29
#define CLK_AP_APB_GATE_NUM		(CLK_EMMC_32K_EB + 1)

#endif /* _DT_BINDINGS_CLK_SHARKL5PRO_H_ */

