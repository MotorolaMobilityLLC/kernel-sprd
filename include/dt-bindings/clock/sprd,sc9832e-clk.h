// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
//
// Spreadtrum SC9860 platform clocks
//
// Copyright (C) 2017, Spreadtrum Communications Inc.

#ifndef _DT_BINDINGS_CLK_SC9860_H_
#define _DT_BINDINGS_CLK_SC9860_H_

#define CLK_ISPPLL_GATE		0
#define	CLK_MPLL_GATE		1
#define	CLK_DPLL_GATE		2
#define	CLK_LPLL_GATE		3
#define	CLK_GPLL_GATE		4
#define	CLK_PMU_APB_NUM		(CLK_GPLL_GATE + 1)

#define	CLK_AUDIO_GATE		0
#define CLK_RPLL		1
#define CLK_RPLL_D1_EN		2
#define CLK_RPLL_390M		3
#define CLK_RPLL_260M		4
#define CLK_RPLL_195M		5
#define CLK_RPLL_26M		6
#define CLK_ANLG_PHY_G6_NUM	(CLK_RPLL_26M + 1)

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

#define CLK_DPLL		0
#define CLK_ANLG_PHY_G2_NUM	(CLK_DPLL + 1)

#define CLK_MPLL		0
#define CLK_ANLG_PHY_G4_NUM	(CLK_MPLL + 1)

#define	CLK_AP_APB		0
#define	CLK_NANDC_ECC		1
#define	CLK_OTG_REF		2
#define CLK_OTG_UTMI		3
#define	CLK_UART1		4
#define	CLK_I2C0		5
#define	CLK_I2C1		6
#define	CLK_I2C2		7
#define	CLK_I2C3		8
#define	CLK_I2C4		9
#define	CLK_SPI0		10
#define	CLK_SPI2		11
#define	CLK_HS_SPI		12
#define	CLK_IIS0		13
#define	CLK_CE			14
#define CLK_NANDC_2X		15
#define CLK_SDIO0_2X		16
#define CLK_SDIO1_2X		17
#define CLK_EMMC_2X		18
#define CLK_VSP			19
#define CLK_GSP			20
#define CLK_DISPC0		21
#define CLK_DISPC0_DPI		22
#define CLK_DSI_RXESC		23
#define CLK_DSI_LANEBYTE	24
#define CLK_AP_CLK_NUM		(CLK_DSI_LANEBYTE + 1)

#define	CLK_AON_APB		0
#define	CLK_ADI			1
#define	CLK_PWM0		2
#define	CLK_PWM1		3
#define	CLK_PWM2		4
#define	CLK_PWM3		5
#define CLK_THM0		6
#define CLK_THM1		7
#define	CLK_AUDIF		8
#define CLK_AUD_IIS_DA0		9
#define CLK_AUD_IIS_AD0		10
#define	CLK_CA53_DAP		11
#define CLK_CA53_DMTCK		12
#define	CLK_CA53_TS		13
#define	CLK_DJTAG_TCK		14
#define	CLK_EMC_REF		15
#define CLK_CSSYS		16
#define	CLK_TMR			17
#define	CLK_DSI_TEST		18
#define	CLK_SDPHY_APB		19
#define	CLK_AIO_APB		20
#define	CLK_DTCK_HW		21
#define	CLK_AP_MM		22
#define	CLK_AP_AXI		23
#define	CLK_NIC_GPU		24
#define	CLK_MM_ISP		25
#define CLK_AON_PREDIV_NUM	(CLK_MM_ISP + 1)

#define CLK_DSI_EB		0
#define DISPC_EB		1
#define VSP_EB			2
#define GSP_EB			3
#define OTG_EB			4
#define DMA_PUB_EB		5
#define CE_PUB_EB		6
#define AHB_CKG_EB		7
#define SDIO0_EB		8
#define SDIO1_EB		9
#define NANDC_EB		10
#define EMMC_EB			11
#define SPINLOCK_EB		12
#define CE_EFUSE_EB		13
#define EMMC_32K_EB		14
#define SDIO0_32K_EB		15
#define SDIO1_32K_EB		16
#define SDIO1_MCU		17
#define CLK_AP_AHB_GATE_NUM	(SDIO1_MCU + 1)

#endif /* _DT_BINDINGS_CLK_SC9860_H_ */
