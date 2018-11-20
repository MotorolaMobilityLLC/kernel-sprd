// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
//
// Spreadtrum Sharkl5 platform clocks
//
// Copyright (C) 2018, Spreadtrum Communications Inc.

#ifndef _DT_BINDINGS_CLK_SHARKL5PRO_H_
#define _DT_BINDINGS_CLK_SHARKL5PRO_H_

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

