/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
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
#include <linux/device.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/sprd_sip_svc.h>
#include "sprd_debuglog_data.h"

#define AP_INTC0_BIT_NAME		\
{					\
	"NULL",				\
	"NULL",				\
	"AP_UART0",			\
	"AP_UART1",			\
	"AP_UART2",			\
	"AP_SPI0",			\
	"AP_SPI1",			\
	"AP_SPI2",			\
	"AP_SPI3",			\
	"AP_SIM",			\
	"AP_EMMC",			\
	"AP_I2C0",			\
	"AP_I2C1",			\
	"AP_I2C2",			\
	"AP_I2C3",			\
	"AP_I2C4",			\
	"AP_IIS0",			\
	"AP_IIS1",			\
	"AP_IIS2",			\
	"AP_SDIO0",			\
	"AP_SDIO1",			\
	"AP_SDIO2",			\
	"CE_SEC",			\
	"CE_PUB",			\
	"AP_DMA",			\
	"DMA_SEC_AP",			\
	"GSP",				\
	"DISPC",			\
	"SLV_FW_AP_INTERRUPT",		\
	"DSI_PLL",			\
	"DSI0",				\
	"DSI1",				\
}

#define AP_INTC1_BIT_NAME		\
{					\
	"NULL",				\
	"NULL",				\
	"VSP",				\
	"NULL",				\
	"NULL",				\
	"NULL",				\
	"NULL",				\
	"NULL",				\
	"FD_ARM_INT",			\
	"CPP_ARM_INT",			\
	"JPG_ARM_INT",			\
	"ISP_CH0",			\
	"ISP_CH1",			\
	"CSI0_CAL_FAILED",		\
	"CSI0_CAL_DONE",		\
	"CSI0_R2",			\
	"CSI0_R1",			\
	"CSI1_CAL_FAILED",		\
	"CSI1_CAL_DONE",		\
	"CSI1_R2",			\
	"CSI1_R1",			\
	"CSI2_CAL_FAILED",		\
	"CSI2_CAL_DONE",		\
	"CSI2_R2",			\
	"CSI2_R1",			\
	"DCAM0_ARM",			\
	"DCAM1_ARM",			\
	"DCAM2_ARM",			\
	"GPU",				\
	"GPIO",				\
	"THM0",				\
	"THM1",				\
}

#define AP_INTC2_BIT_NAME		\
{					\
	"NULL",				\
	"NULL",				\
	"THM2",				\
	"KPD",				\
	"AON_I2C",			\
	"OTG",				\
	"ADI",				\
	"AON_TMR",			\
	"EIC",				\
	"AP_TMR0",			\
	"AP_TMR1",			\
	"AP_TMR2",			\
	"AP_TMR3",			\
	"AP_TMR4",			\
	"AP_SYST",			\
	"APCPU_WDG",			\
	"AP_WDG",			\
	"BUSMON_AON",			\
	"MBOX_SRC_AP",			\
	"MBOX_TAR_AP",			\
	"MBOX_TAR_SIPC_AP_NOWAKEUP",	\
	"PWR_UP_AP",			\
	"PWR_UP_PUB",			\
	"PWR_UP_ALL",			\
	"PWR_DOWN_ALL",			\
	"SEC_EIC",			\
	"SEC_EIC_NON_LAT",		\
	"SEC_WDG",			\
	"SEC_RTC",			\
	"SEC_TMR",			\
	"SEC_GPIO",			\
	"SLV_FW_AON",			\
}

#define AP_INTC3_BIT_NAME		\
{					\
	"NULL",				\
	"NULL",				\
	"MEM_FW_AON",			\
	"AON_32K_DET",			\
	"SCC",				\
	"IIS",				\
	"APB_BUSMON",			\
	"EXT_RSTB_APCPU",		\
	"AON_SYST",			\
	"MEM_FW_PUB",			\
	"PUB_HDW_DFS_EXIT",		\
	"PUB_DFS_ERROR",		\
	"PUB_DFS_COMPLETE",		\
	"PUB_PTM",			\
	"DFI_BUS_MONITOR_PUB",		\
	"REG_PUB_DMC_MPU_VIO",		\
	"NPMUIRQ0",			\
	"NPMUIRQ1",			\
	"NPMUIRQ2",			\
	"NPMUIRQ3",			\
	"NULL",				\
	"NULL",				\
	"NULL",				\
	"NULL",				\
	"NCTIIRQ0",			\
	"NCTIIRQ1",			\
	"NCTIIRQ2",			\
	"NCTIIRQ3",			\
	"NULL",				\
	"NULL",				\
	"NULL",				\
	"NULL",				\
}

#define AP_INTC4_BIT_NAME		\
{					\
	"NULL",				\
	"NULL",				\
	"NERRIRQ1",			\
	"NERRIRQ2",			\
	"NERRIRQ3",			\
	"NERRIRQ4",			\
	"NERRIRQ5",			\
	"NERRIRQ6",			\
	"NERRIRQ7",			\
	"NERRIRQ8",			\
	"NFAULTIRQ1",			\
	"NFAULTIRQ2",			\
	"NFAULTIRQ3",			\
	"NFAULTIRQ4",			\
	"NFAULTIRQ5",			\
	"NFAULTIRQ6",			\
	"NFAULTIRQ7",			\
	"NFAULTIRQ8",			\
	"NCNTPNSIRQ0",			\
	"NCNTPNSIRQ1",			\
	"NCNTPNSIRQ2",			\
	"NCNTPNSIRQ3",			\
	"NULL",				\
	"NULL",				\
	"NULL",				\
	"NULL",				\
	"NCNTPSIRQ0",			\
	"NCNTPSIRQ1",			\
	"NCNTPSIRQ2",			\
	"NCNTPSIRQ3",			\
	"NULL",				\
	"NULL",				\
}

#define AP_INTC5_BIT_NAME		\
{					\
	"NULL",				\
	"NULL",				\
	"NULL",				\
	"NULL",				\
	"NERRIRQ0",			\
	"NFAULTIRQ0",			\
	"APCPU_PMU",			\
	"APCPU_ERR",			\
	"APCPU_FAULTY",			\
	"NULL",				\
	"NULL",				\
	"APCPU_MODE_ST",		\
	"NCLUSTERPMUIRQ",		\
	"ANA",				\
	"WTLCP_TG_WDG_RST",		\
	"WTLCP_LTE_WDG_RST",		\
	"MDAR",				\
	"AUDCP_CHN_START_CHN0",		\
	"AUDCP_CHN_START_CHN1",		\
	"AUDCP_CHN_START_CHN2",		\
	"AUDCP_CHN_START_CHN3",		\
	"AUDCP_DMA",			\
	"AUDCP_MCDT",			\
	"AUDCP_VBC_AUDRCD",		\
	"AUDCP_VBC_AUDPLY",		\
	"AUDCP_WDG_RST",		\
	"ACC_PROT_PMU",			\
	"ACC_PROT_AON_APB_REG",		\
	"EIC_NON_LAT",			\
	"PUB_DFS_GIVEUP",		\
	"PUB_DFS_DENY",			\
	"PUB_DFS_VOTE_DONE",		\
}

/* Int anlyise */
static int gpio_int_handler(char *buff, u32 second, u32 thrid);
static int eic_int_handler(char *buff, u32 second, u32 thrid);
static int mbox_int_handler(char *buff, u32 second, u32 thrid);
static int ana_int_handler(char *buff, u32 second, u32 thrid);

/* AP INTC */
static struct intc_info ap_intc_set[] = {
	INTC_INFO_INIT("sprd,sys-ap-intc0", AP_INTC0_BIT_NAME),
	INTC_INFO_INIT("sprd,sys-ap-intc1", AP_INTC1_BIT_NAME),
	INTC_INFO_INIT("sprd,sys-ap-intc2", AP_INTC2_BIT_NAME),
	INTC_INFO_INIT("sprd,sys-ap-intc3", AP_INTC3_BIT_NAME),
	INTC_INFO_INIT("sprd,sys-ap-intc4", AP_INTC4_BIT_NAME),
	INTC_INFO_INIT("sprd,sys-ap-intc5", AP_INTC5_BIT_NAME),
};

static struct intc_handler ap_intc1_handler[] = {
	INTC_HANDLER_INIT(29, gpio_int_handler),
};

static struct intc_handler ap_intc2_handler[] = {
	INTC_HANDLER_INIT(19, mbox_int_handler),
	INTC_HANDLER_INIT(25, eic_int_handler),
};

static struct intc_handler ap_intc5_handler[] = {
	INTC_HANDLER_INIT(13, ana_int_handler),
};

static struct intc_handler_set ap_intc_handler_set[] = {
	INTC_HANDLER_SET_INIT(0, NULL),
	INTC_HANDLER_SET_INIT(1, ap_intc1_handler),
	INTC_HANDLER_SET_INIT(2, ap_intc2_handler),
	INTC_HANDLER_SET_INIT(0, NULL),
	INTC_HANDLER_SET_INIT(0, NULL),
	INTC_HANDLER_SET_INIT(1, ap_intc5_handler),
};

/* AP_AHB */
static struct reg_bit ahb_eb[] = {
	REG_BIT_INIT("DSI_EB",     0x01, 0, 0),
	REG_BIT_INIT("DISPC_EB",   0x01, 1, 0),
	REG_BIT_INIT("VSP_EB",     0x01, 2, 0),
	REG_BIT_INIT("DMA_ENABLE", 0x01, 4, 0),
	REG_BIT_INIT("DMA_EB",     0x01, 5, 0),
	REG_BIT_INIT("IPI_EB",     0x01, 6, 0),
};

static struct reg_bit ap_sys_force_sleep_cfg[] = {
	REG_BIT_INIT("PERI_FORCE_OFF",      0x01, 1, 0),
	REG_BIT_INIT("PERI_FORCE_ON",       0x01, 2, 0),
	REG_BIT_INIT("AXI_LP_CTRL_DISABLE", 0x01, 3, 0),
};

static struct reg_bit ap_m0_lpc[] = {
	REG_BIT_INIT("MAIN_M0_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_m1_lpc[] = {
	REG_BIT_INIT("MAIN_M1_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_m2_lpc[] = {
	REG_BIT_INIT("MAIN_M2_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_m3_lpc[] = {
	REG_BIT_INIT("MAIN_M3_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_m4_lpc[] = {
	REG_BIT_INIT("MAIN_M4_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_m5_lpc[] = {
	REG_BIT_INIT("MAIN_M5_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_m6_lpc[] = {
	REG_BIT_INIT("MAIN_M6_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_m7_lpc[] = {
	REG_BIT_INIT("MAIN_M7_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_s0_lpc[] = {
	REG_BIT_INIT("MAIN_S0_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MTX_S0_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit ap_s1_lpc[] = {
	REG_BIT_INIT("MAIN_S1_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MTX_S1_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit ap_s2_lpc[] = {
	REG_BIT_INIT("MAIN_S2_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MTX_S2_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit ap_s3_lpc[] = {
	REG_BIT_INIT("MAIN_S3_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MTX_S3_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit ap_s4_lpc[] = {
	REG_BIT_INIT("MAIN_S4_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MTX_S4_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit ap_s5_lpc[] = {
	REG_BIT_INIT("MAIN_S5_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MTX_S5_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit ap_s6_lpc[] = {
	REG_BIT_INIT("MAIN_S6_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MTX_S6_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit ap_s7_lpc[] = {
	REG_BIT_INIT("MAIN_S7_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MTX_S7_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit ap_main_lpc[] = {
	REG_BIT_INIT("MAIN_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MATRIX_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit ap_merge_m1_lpc[] = {
	REG_BIT_INIT("MERGE_M1_LP_EB", 0x01, 16, 1),
};

static struct reg_bit ap_merge_s0_lpc[] = {
	REG_BIT_INIT("MERGE_S0_LP_EB", 0x01, 16, 1),
	REG_BIT_INIT("CGM_MERGE_S0_AUTO_GATE_EN", 0x01, 17, 1),
};

static struct reg_bit disp_async_brg[] = {
	REG_BIT_INIT("DISP_ASYNC_BRG_LP_EB", 0x01, 0, 1),
};

static struct reg_bit ap_async_brg[] = {
	REG_BIT_INIT("AP_ASYNC_BRG_LP_EB", 0x01, 0, 1),
};

static struct reg_bit vdsp_lp_ctrl[] = {
	REG_BIT_INIT("VDSPO_STOP_EN", 0x01, 2, 1),
};

static struct reg_info ap_ahb[] = {
	REG_INFO_INIT("AHB_EB", 0x0000, ahb_eb),

	REG_INFO_INIT("M0_LPC", 0x0060, ap_m0_lpc),
	REG_INFO_INIT("M1_LPC", 0x0064, ap_m1_lpc),
	REG_INFO_INIT("M2_LPC", 0x0068, ap_m2_lpc),
	REG_INFO_INIT("M3_LPC", 0x006C, ap_m3_lpc),
	REG_INFO_INIT("M4_LPC", 0x0070, ap_m4_lpc),
	REG_INFO_INIT("M5_LPC", 0x0074, ap_m5_lpc),
	REG_INFO_INIT("M6_LPC", 0x0078, ap_m6_lpc),
	REG_INFO_INIT("M7_LPC", 0x007C, ap_m7_lpc),

	REG_INFO_INIT("MAIN_LPC", 0x0088, ap_main_lpc),

	REG_INFO_INIT("S0_LPC", 0x008C, ap_s0_lpc),
	REG_INFO_INIT("S1_LPC", 0x0090, ap_s1_lpc),
	REG_INFO_INIT("S2_LPC", 0x0094, ap_s2_lpc),
	REG_INFO_INIT("S3_LPC", 0x0098, ap_s3_lpc),
	REG_INFO_INIT("S4_LPC", 0x009C, ap_s4_lpc),
	REG_INFO_INIT("S5_LPC", 0x0058, ap_s5_lpc),
	REG_INFO_INIT("S6_LPC", 0x0054, ap_s6_lpc),
	REG_INFO_INIT("S7_LPC", 0x00A8, ap_s7_lpc),

	REG_INFO_INIT("MERGE_M1_LPC", 0x00A4, ap_merge_m1_lpc),
	REG_INFO_INIT("MERGE_S0_LPC", 0x00AC, ap_merge_s0_lpc),

	REG_INFO_INIT("DISP_ASYNC_BRG", 0x0050, disp_async_brg),
	REG_INFO_INIT("AP_ASYNC_BRG", 0x005C, ap_async_brg),

	REG_INFO_INIT("VDSP_LP_CTRL", 0x3090, vdsp_lp_ctrl),

	REG_INFO_INIT("AP_SYS_FORCE_SLEEP_CFG", 0x000C, ap_sys_force_sleep_cfg),
};

/* AP_APB */
static struct reg_bit apb_eb[] = {
	REG_BIT_INIT("SIM0_EB",   0x01,  0, 0),
	REG_BIT_INIT("IIS0_EB",   0x01,  1, 0),
	REG_BIT_INIT("IIS1_EB",   0x01,  2, 0),
	REG_BIT_INIT("IIS2_EB",   0x01,  3, 0),
	REG_BIT_INIT("SPI0_EB",   0x01,  5, 0),
	REG_BIT_INIT("SPI1_EB",   0x01,  6, 0),
	REG_BIT_INIT("SPI2_EB",   0x01,  7, 0),
	REG_BIT_INIT("SPI3_EB",   0x01,  8, 0),
	REG_BIT_INIT("I2C0_EB",   0x01,  9, 0),
	REG_BIT_INIT("I2C1_EB",   0x01, 10, 0),
	REG_BIT_INIT("I2C2_EB",   0x01, 11, 0),
	REG_BIT_INIT("I2C3_EB",   0x01, 12, 0),
	REG_BIT_INIT("I2C4_EB",   0x01, 13, 0),
	REG_BIT_INIT("UART0_EB",  0x01, 14, 0),
	REG_BIT_INIT("UART1_EB",  0x01, 15, 0),
	REG_BIT_INIT("UART2_EB",  0x01, 16, 0),
	REG_BIT_INIT("SDIO0_EB",  0x01, 22, 0),
	REG_BIT_INIT("SDIO1_EB",  0x01, 23, 0),
	REG_BIT_INIT("SDIO2_EB",  0x01, 24, 0),
	REG_BIT_INIT("EMMC_EB",   0x01, 25, 0),
	REG_BIT_INIT("CE_SEC_EB", 0x01, 30, 0),
	REG_BIT_INIT("CE_PUB_EB", 0x01, 31, 0),
};

static struct reg_bit apb_misc_ctrl[] = {
	REG_BIT_INIT("SPI0_SEC_EB", 0x01,  6, 0),
	REG_BIT_INIT("SPI1_SEC_EB", 0x01,  7, 0),
	REG_BIT_INIT("SPI2_SEC_EB", 0x01,  8, 0),
	REG_BIT_INIT("SPI3_SEC_EB", 0x01,  9, 0),
	REG_BIT_INIT("I2C0_SEC_EB", 0x01, 10, 0),
	REG_BIT_INIT("I2C1_SEC_EB", 0x01, 11, 0),
	REG_BIT_INIT("I2C2_SEC_EB", 0x01, 12, 0),
	REG_BIT_INIT("I2C3_SEC_EB", 0x01, 13, 0),
	REG_BIT_INIT("I2C4_SEC_EB", 0x01, 14, 0),
};

static struct reg_info ap_apb[] = {
	REG_INFO_INIT("APB_EB", 0x0000, apb_eb),
	REG_INFO_INIT("APB_MISC_CTRL", 0x0008, apb_misc_ctrl),
};

/* AON APB */
static struct reg_bit sp_cfg_bus[] = {
	REG_BIT_INIT("SP_CFG_BUS_SLEEP", 0x01, 0, 1),
};

static struct reg_bit apcpu_top_mtx_lpc_ctrl[] = {
	REG_BIT_INIT("APCPU_TOP_MTX_M0_LP_EB",   0x01, 0, 1),
	REG_BIT_INIT("APCPU_TOP_MTX_M1_LP_EB",   0x01, 1, 1),
	REG_BIT_INIT("APCPU_TOP_MTX_M2_LP_EB",   0x01, 2, 1),
	REG_BIT_INIT("APCPU_TOP_MTX_M3_LP_EB",   0x01, 3, 1),
	REG_BIT_INIT("APCPU_TOP_MTX_S0_LP_EB",   0x01, 4, 1),
	REG_BIT_INIT("APCPU_TOP_MTX_S1_LP_EB",   0x01, 5, 1),
	REG_BIT_INIT("APCPU_TOP_MTX_S2_LP_EB",   0x01, 6, 1),
	REG_BIT_INIT("APCPU_TOP_MTX_S3_LP_EB",   0x01, 7, 1),
	REG_BIT_INIT("APCPU_TOP_MTX_MAIN_LP_EB", 0x01, 8, 1),
};

static struct reg_bit apcpu_ddr_ab_lpc_ctrl[] = {
	REG_BIT_INIT("APCPU_DDR_AB_LP_EB", 0x01, 0, 1),
};

static struct reg_bit apcpu_dbg_blk_lpc_ctrl[] = {
	REG_BIT_INIT("APCPU_DBG_BLK_LP_EB", 0x01, 0, 1),
};

static struct reg_bit apcpu_gic600_gic_lpc_ctrl[] = {
	REG_BIT_INIT("APCPU_GIC600_GIC_LP_EB", 0x01, 0, 1),
};

static struct reg_bit apcpu_cluster_scu_lpc_ctrl[] = {
	REG_BIT_INIT("APCPU_CLUSTER_SCU_LP_EB", 0x01, 0, 1),
};

static struct reg_bit apcpu_cluster_gic_lpc_ctrl[] = {
	REG_BIT_INIT("APCPU_CLUSTER_GIC_LP_EB", 0x01, 0, 1),
};

static struct reg_bit apcpu_cluster_apb_lpc_ctrl[] = {
	REG_BIT_INIT("APCPU_CLUSTER_APB_LP_EB", 0x01, 0, 1),
};

static struct reg_bit apcpu_cluster_atb_lpc_ctrl[] = {
	REG_BIT_INIT("APCPU_CLUSTER_ATB_LP_EB", 0x01, 0, 1),
};

static struct reg_info aon_apb[] = {
	REG_INFO_INIT("SP_CFG_BUS",                 0x0124, sp_cfg_bus),
	REG_INFO_INIT("APCPU_TOP_MTX_M0_LPC_CTRL",  0x0300, apcpu_top_mtx_lpc_ctrl),
	REG_INFO_INIT("APCPU_DDR_AB_LPC_CTRL",      0x0324, apcpu_ddr_ab_lpc_ctrl),
	REG_INFO_INIT("APCPU_DBG_BLK_LPC_CTRL",     0x029C, apcpu_dbg_blk_lpc_ctrl),
	REG_INFO_INIT("APCPU_GIC600_GIC_LPC_CTRL",  0x0298, apcpu_gic600_gic_lpc_ctrl),
	REG_INFO_INIT("APCPU_CLUSTER_SCU_LPC_CTRL", 0x0320, apcpu_cluster_scu_lpc_ctrl),
	REG_INFO_INIT("APCPU_CLUSTER_GIC_LPC_CTRL", 0x0294, apcpu_cluster_gic_lpc_ctrl),
	REG_INFO_INIT("APCPU_CLUSTER_APB_LPC_CTRL", 0x0290, apcpu_cluster_apb_lpc_ctrl),
	REG_INFO_INIT("APCPU_CLUSTER_ATB_LPC_CTRL", 0x028C, apcpu_cluster_atb_lpc_ctrl),
};

/* PMU APB */
static struct reg_bit apcpu_mode_state0[] = {
	REG_BIT_INIT("APCPU_CORE0_LOW_POWER_STATE", 0xFF,  0, 0x07),
	REG_BIT_INIT("APCPU_CORE1_LOW_POWER_STATE", 0xFF,  8, 0x07),
	REG_BIT_INIT("APCPU_CORE2_LOW_POWER_STATE", 0xFF, 16, 0x07),
};

static struct reg_bit apcpu_mode_state_fig[] = {
	REG_BIT_INIT("APCPU_CORE3_LOW_POWER_STATE", 0xFF,  0, 0x07),
	REG_BIT_INIT("APCPU_CORE4_LOW_POWER_STATE", 0xFF,  8, 0x07),
	REG_BIT_INIT("APCPU_CORE5_LOW_POWER_STATE", 0xFF, 16, 0x07),
	REG_BIT_INIT("APCPU_CORE6_LOW_POWER_STATE", 0xFF, 24, 0x07),
};

static struct reg_bit apcpu_mode_state1[] = {
	REG_BIT_INIT("APCPU_CORE7_LOW_POWER_STATE",   0xFF, 0, 0x07),
	REG_BIT_INIT("APCPU_CORINTH_LOW_POWER_STATE", 0xFF, 8, 0x00),
};

static struct reg_bit apcpu_pwr_state0[] = {
	REG_BIT_INIT("PD_APCPU_TOP_STATE", 0xFF,  0, 0x07),
	REG_BIT_INIT("PD_APCPU_C0_STATE",  0xFF,  8, 0x07),
	REG_BIT_INIT("PD_APCPU_C1_STATE",  0xFF, 16, 0x07),
	REG_BIT_INIT("PD_APCPU_C2_STATE",  0xFF, 24, 0x07),
};

static struct reg_bit apcpu_pwr_state_fig[] = {
	REG_BIT_INIT("PD_APCPU_C6_STATE", 0xFF,  0, 0x07),
	REG_BIT_INIT("PD_APCPU_C5_STATE", 0xFF,  8, 0x07),
	REG_BIT_INIT("PD_APCPU_C4_STATE", 0xFF, 16, 0x07),
	REG_BIT_INIT("PD_APCPU_C3_STATE", 0xFF, 24, 0x07),
};

static struct reg_bit apcpu_pwr_state1[] = {
	REG_BIT_INIT("PD_APCPU_C7_STATE", 0xFF,  0, 0x07),
};

static struct reg_bit pwr_status0_dbg[] = {
	REG_BIT_INIT("PD_AP_SYS_STATE",   0xFF,  0, 0x07),
	REG_BIT_INIT("PD_AP_VSP_STATE",   0xFF,  8, 0x07),
	REG_BIT_INIT("PD_CDMA_SYS_STATE", 0xFF, 16, 0x07),
	REG_BIT_INIT("PD_AP_VDSP_STATE",  0xFF, 24, 0x07),
};

static struct reg_bit pwr_status1_dbg[] = {
	REG_BIT_INIT("PD_WTLCP_HU3GE_A_STATE", 0xFF,  0, 0x07),
	REG_BIT_INIT("PD_WTLCP_TGDSP_STATE",   0xFF,  8, 0x07),
	REG_BIT_INIT("PD_WTLCP_LDSP_STATE",    0xFF, 16, 0x07),
	REG_BIT_INIT("PD_WTLCP_HU3GE_B_STATE", 0xFF, 24, 0x07),
};

static struct reg_bit pwr_status2_dbg[] = {
	REG_BIT_INIT("PD_WTLCP_SYS_STATE",      0xFF,  0, 0x07),
	REG_BIT_INIT("PD_PUBCP_SYS_STATE",      0xFF,  8, 0x07),
	REG_BIT_INIT("PD_WTLCP_LTE_PROC_STATE", 0xFF, 16, 0x07),
	REG_BIT_INIT("PD_WTLCP_TD_PROC_STATE",  0xFF, 24, 0x07),
};

static struct reg_bit pwr_status3_dbg[] = {
	REG_BIT_INIT("PD_AUDCP_DSP_STATE", 0xFF,  0, 0x07),
	REG_BIT_INIT("PD_GPU_TOP_STATE",   0xFF,  8, 0x07),
	REG_BIT_INIT("PD_MM_TOP_STATE",    0xFF, 16, 0x07),
};

static struct reg_bit pwr_status4_dbg[] = {
	REG_BIT_INIT("PD_WTLCP_LTE_DPFEC_STATE", 0xFF,  0, 0x07),
	REG_BIT_INIT("PD_WTLCP_LTE_CE_STATE",    0xFF,  8, 0x07),
	REG_BIT_INIT("PD_PUB_SYS_STATE",         0xFF, 16, 0x07),
	REG_BIT_INIT("PD_AUDCP_SYS_STATE",       0xFF, 24, 0x07),
};

static struct reg_bit sleep_ctrl[] = {
	REG_BIT_INIT("AP_DEEP_SLEEP",      0x01, 0, 1),
	REG_BIT_INIT("WTLCP_DEEP_SLEEP",   0x01, 1, 1),
	REG_BIT_INIT("PUBCP_DEEP_SLEEP",   0x01, 2, 1),
	REG_BIT_INIT("CDMA_DEEP_SLEEP",    0x01, 3, 1),
	REG_BIT_INIT("PUB_SYS_DEEP_SLEEP", 0x01, 4, 1),
	REG_BIT_INIT("AUDCP_DEEP_SLEEP",   0x01, 5, 1),
	REG_BIT_INIT("SP_SYS_DEEP_SLEEP",  0x01, 6, 1),
};

static struct reg_bit light_sleep_mon[] = {
	REG_BIT_INIT("AON_SYS_LIGHT_SLEEP", 0x01, 0, 1),
	REG_BIT_INIT("AUDCP_LIGHT_SLEEP",   0x01, 1, 1),
	REG_BIT_INIT("PUBCP_LIGHT_SLEEP",   0x01, 2, 1),
	REG_BIT_INIT("WTLCP_LIGHT_SLEEP",   0x01, 3, 1),
	REG_BIT_INIT("AP_LIGHT_SLEEP",      0x01, 4, 1),
	REG_BIT_INIT("PUB_SYS_LIGHT_SLEEP", 0x01, 5, 1),
};

static struct reg_info pmu_apb[] = {
	REG_INFO_INIT("APCPU_MODE_STATE0",    0x03D8, apcpu_mode_state0),
	REG_INFO_INIT("APCPU_MODE_STATE_FIG", 0x0840, apcpu_mode_state_fig),
	REG_INFO_INIT("APCPU_MODE_STATE1",    0x03DC, apcpu_mode_state1),
	REG_INFO_INIT("APCPU_PWR_STATE0",     0x0378, apcpu_pwr_state0),
	REG_INFO_INIT("APCPU_PWR_STATE_FIG",  0x0820, apcpu_pwr_state_fig),
	REG_INFO_INIT("APCPU_PWR_STATE1",     0x037C, apcpu_pwr_state1),
	REG_INFO_INIT("PWR_STATUS0_DBG",      0x00BC, pwr_status0_dbg),
	REG_INFO_INIT("PWR_STATUS1_DBG",      0x00C0, pwr_status1_dbg),
	REG_INFO_INIT("PWR_STATUS2_DBG",      0x00C4, pwr_status2_dbg),
	REG_INFO_INIT("PWR_STATUS3_DBG",      0x010C, pwr_status3_dbg),
	REG_INFO_INIT("PWR_STATUS4_DBG",      0x00B8, pwr_status4_dbg),
	REG_INFO_INIT("SLEEP_CTRL",           0x00CC, sleep_ctrl),
	REG_INFO_INIT("LIGHT_SLEEP_MON",      0x0234, light_sleep_mon),
};

// Register table
static struct reg_table reg_table_check[] = {
	REG_TABLE_INIT("AP_AHB",  "sprd,sys-ap-ahb",  ap_ahb),
	REG_TABLE_INIT("AP_APB",  "sprd,sys-ap-apb",  ap_apb),
	REG_TABLE_INIT("PMU_APB", "sprd,sys-pmu-apb", pmu_apb),
	REG_TABLE_INIT("AON_APB", "sprd,sys-aon-apb", aon_apb),
};

static struct reg_table reg_table_monitor[] = {
	REG_TABLE_INIT("PMU_APB", "sprd,sys-pmu-apb", pmu_apb),
	REG_TABLE_INIT("AON_APB", "sprd,sys-aon-apb", aon_apb),
};

/**
 * GPIO int anlyise
 */
static int gpio_int_handler(char *buff, u32 second, u32 thrid)
{
	#define GPIO_BIT_SHIFT	4

	u32 grp, bit;

	grp = (second >> 16) & 0xFFFF;
	bit = second & 0xFFFF;

	sprintf(buff, "%u", (grp << GPIO_BIT_SHIFT) + bit);

	return 0;
}

/**
 * AP EIC int anlyise
 */
static int eic_int_handler(char *buff, u32 second, u32 thrid)
{
	/* EIC int list */
	#define EIC_EXT_NUM	6
	#define EIC_TYPE_NUM	4
	#define EIC_NUM		8

	static const char *eic_index[] = {
		"AON_EIC_EXT0", "AON_EIC_EXT1", "AON_EIC_EXT2",
		"AON_EIC_EXT3", "AON_EIC_EXT4", "AON_EIC_EXT5",
	};

	static const char *eic_type[] = {
		"DBNC", "LATCH", "ASYNC", "SYNC",
	};

	u32 grp, bit, num;

	grp = (second >> 16) & 0xFFFF;
	bit = second & 0xFFFF;
	num = thrid & 0xFFFF;

	if (grp >= EIC_EXT_NUM || bit >= EIC_TYPE_NUM || num >= EIC_NUM)
		return -EINVAL;

	sprintf(buff, "%s-%s-%u", eic_index[grp], eic_type[bit], num);

	return 0;
}

/**
 * Mailbox int anlyise
 */
static int mbox_int_handler(char *buff, u32 second, u32 thrid)
{
	#define MBOX_SRC_NUM	6

	static const char *mbox_src[] = {
		"AP", "CM4", "CR5", "TGDSP",
		"LDSP", "ADSP", "AP",
	};

	u32 bit;

	bit = second & 0xFFFF;

	if (bit >= MBOX_SRC_NUM)
		return -EINVAL;

	sprintf(buff, "%s", mbox_src[bit]);

	return 0;
}

/**
 * PMIC int anlyise
 */
static int ana_int_handler(char *buff, u32 second, u32 thrid)
{
	/* PMIC ANA int list */
	#define ANA_INT_NUM		9
	#define ANA_EIC_NUM		7

	static const char *ana_int[] = {
		"ADC_INT", "RTC_INT", "WDG_INT",
		"FGU_INT", "EIC_INT", "FAST_CHG_INT",
		"TMR_INT", "CAL_INT", "TYPEC_INT",
		"USB_PD_INT",
	};

	static const char *ana_eic[] = {
		"CHGR_INT", "PBINT", "PBINT2", "BATDET_OK",
		"KEY2_7S_EXT_RSTN", "EXT_XTL0_EN",
		"AUD_INT_ALL", "ENDURA_OPTION",
	};

	u32 bit, num;
	int pos;

	bit = second & 0xFFFF;
	num = thrid & 0xFFFF;

	if (bit >= ANA_INT_NUM)
		return -EINVAL;

	pos = sprintf(buff, "%s", ana_int[bit]);

	if (bit == 4 && num < ANA_EIC_NUM)
		sprintf(buff + pos, ".%s", ana_eic[num]);

	return 0;
}

/* Wakeup source match */
static int intc_match(char *buff, u32 intc, u32 second, u32 thrid)
{
	struct intc_handler_set *pset;
	u32 inum, ibit;
	int i;

	inum = (intc >> 16) & 0xFFFF;
	ibit = intc & 0xFFFF;

	if (inum >= ARRAY_SIZE(ap_intc_handler_set) || ibit >= 32)
		return -EINVAL;

	pset = &ap_intc_handler_set[inum];

	for (i = 0; i < pset->num; ++i)
		if ((ibit == pset->set[i].bit) && pset->set[i].ph)
			return pset->set[i].ph(buff, second, thrid);

	return 0;
}

/* Platform debug power data */
struct debug_data sprd_sharkl5pro_debug_data = {
	/* debug check */
	.check = {ARRAY_SIZE(reg_table_check), reg_table_check},

	/* debug monitor */
	.monitor = {ARRAY_SIZE(reg_table_monitor), reg_table_monitor},

	/* intc set */
	.intc = {ARRAY_SIZE(ap_intc_set), ap_intc_set},

	/* wakeup source match callback */
	.wakeup_source_match = intc_match,
};
