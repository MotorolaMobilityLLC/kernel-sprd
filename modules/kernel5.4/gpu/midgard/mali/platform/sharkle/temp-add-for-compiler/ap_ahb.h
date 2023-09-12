/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * This file is dual-licensed: you can use it either under the terms
 * of the GPL or the X11 license, at your option. Note that this dual
 * licensing only applies to this file, and not this project as a
 * whole.
 *
 * updated at 2017-06-12 20:13:17
 *
 */


#ifndef AP_AHB_H
#define AP_AHB_H



#define REG_AP_AHB_AHB_EB                          (0x0000)
#define REG_AP_AHB_AHB_RST                         (0x0004)
#define REG_AP_AHB_CA53_RST_SET                    (0x0008)
#define REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG          (0x000C)
#define REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG           (0x0010)
#define REG_AP_AHB_HOLDING_PEN                     (0x0014)
#define REG_AP_AHB_JMP_ADDR_CA53_C0                (0x0018)
#define REG_AP_AHB_JMP_ADDR_CA53_C1                (0x001C)
#define REG_AP_AHB_JMP_ADDR_CA53_C2                (0x0020)
#define REG_AP_AHB_JMP_ADDR_CA53_C3                (0x0024)
#define REG_AP_AHB_CA53_C0_PU_LOCK                 (0x0028)
#define REG_AP_AHB_CA53_C1_PU_LOCK                 (0x002C)
#define REG_AP_AHB_CA53_C2_PU_LOCK                 (0x0030)
#define REG_AP_AHB_CA53_C3_PU_LOCK                 (0x0034)
#define REG_AP_AHB_CA53_CKG_DIV_CFG                (0x0038)
#define REG_AP_AHB_MCU_PAUSE                       (0x003C)
#define REG_AP_AHB_MISC_CKG_EN                     (0x0040)
#define REG_AP_AHB_CA53_C0_AUTO_FORCE_SHUTDOWN_EN  (0x0044)
#define REG_AP_AHB_CA53_C1_AUTO_FORCE_SHUTDOWN_EN  (0x0048)
#define REG_AP_AHB_CA53_C2_AUTO_FORCE_SHUTDOWN_EN  (0x004C)
#define REG_AP_AHB_DISP_ASYNC_BRG                  (0x0050)
#define REG_AP_AHB_CA53_CKG_SEL_CFG                (0x0054)
#define REG_AP_AHB_S5_LPC                          (0x0058)
#define REG_AP_AHB_AP_ASYNC_BRG                    (0x005C)
#define REG_AP_AHB_M0_LPC                          (0x0060)
#define REG_AP_AHB_M1_LPC                          (0x0064)
#define REG_AP_AHB_M2_LPC                          (0x0068)
#define REG_AP_AHB_M3_LPC                          (0x006C)
#define REG_AP_AHB_M4_LPC                          (0x0070)
#define REG_AP_AHB_M5_LPC                          (0x0074)
#define REG_AP_AHB_M6_LPC                          (0x0078)
#define REG_AP_AHB_M7_LPC                          (0x007C)
#define REG_AP_AHB_M8_LPC                          (0x0080)
#define REG_AP_AHB_M9_LPC                          (0x0084)
#define REG_AP_AHB_MAIN_LPC                        (0x0088)
#define REG_AP_AHB_S0_LPC                          (0x008C)
#define REG_AP_AHB_S1_LPC                          (0x0090)
#define REG_AP_AHB_S2_LPC                          (0x0094)
#define REG_AP_AHB_S3_LPC                          (0x0098)
#define REG_AP_AHB_S4_LPC                          (0x009C)
#define REG_AP_AHB_MERGE_M0_LPC                    (0x00A0)
#define REG_AP_AHB_MERGE_M1_LPC                    (0x00A4)
#define REG_AP_AHB_MERGE_M2_LPC                    (0x00A8)
#define REG_AP_AHB_MERGE_S0_LPC                    (0x00AC)
#define REG_AP_AHB_AP_QOS0                         (0x00B0)
#define REG_AP_AHB_CPU_ASYNC_BRIDGE                (0x00B4)
#define REG_AP_AHB_AP_QOS1                         (0x00B8)
#define REG_AP_AHB_AP_QOS2                         (0x00BC)
#define REG_AP_AHB_CA53_L2                         (0x00C0)
#define REG_AP_AHB_CA53_MTX_LP                     (0x00C4)
#define REG_AP_AHB_CA53_STANDBY_STATUS             (0x3008)
#define REG_AP_AHB_SYS_RST                         (0x3010)
#define REG_AP_AHB_LVDS_PLL_CFG0                   (0x3014)
#define REG_AP_AHB_LVDS_PLL_CFG1                   (0x3018)
#define REG_AP_AHB_AP_QOS_CFG                      (0x301C)
#define REG_AP_AHB_OTG_PHY_TUNE                    (0x3020)
#define REG_AP_AHB_OTG_PHY_TEST                    (0x3024)
#define REG_AP_AHB_OTG_PHY_CTRL                    (0x3028)
#define REG_AP_AHB_OTG_CTRL0                       (0x302C)
#define REG_AP_AHB_OTG_CTRL1                       (0x3030)
#define REG_AP_AHB_DSI_PHY                         (0x3034)
#define REG_AP_AHB_CE_SEC_EB                       (0x3038)
#define REG_AP_AHB_CE_SEC_SOFT_RST                 (0x303C)
#define REG_AP_AHB_DMA_SOFT_RST                    (0x3040)
#define REG_AP_AHB_DMA_SEC_EB                      (0x3044)
#define REG_AP_AHB_CHIP_ID                         (0x30FC)

/* REG_AP_AHB_AHB_EB */

#define BIT_AP_AHB_SDIO1_32K_EB                   BIT(29)
#define BIT_AP_AHB_SDIO0_32K_EB                   BIT(28)
#define BIT_AP_AHB_EMMC_32K_EB                    BIT(27)
#define BIT_AP_AHB_AHBREG_EB                      BIT(23)
#define BIT_AP_AHB_SPINLOCK_EB                    BIT(13)
#define BIT_AP_AHB_EMMC_EB                        BIT(11)
#define BIT_AP_AHB_NANDC_EB                       BIT(10)
#define BIT_AP_AHB_SDIO1_EB                       BIT(9)
#define BIT_AP_AHB_SDIO0_EB                       BIT(8)
#define BIT_AP_AHB_CKG_EB                         BIT(7)
#define BIT_AP_AHB_CE_PUB_EB                      BIT(6)
#define BIT_AP_AHB_DMA_PUB_EB                     BIT(5)
#define BIT_AP_AHB_OTG_EB                         BIT(4)
#define BIT_AP_AHB_GSP_EB                         BIT(3)
#define BIT_AP_AHB_VSP_EB                         BIT(2)
#define BIT_AP_AHB_DISPC_EB                       BIT(1)
#define BIT_AP_AHB_DSI_EB                         BIT(0)

/* REG_AP_AHB_AHB_RST */

#define BIT_AP_AHB_SPINLOCK_SOFT_RST              BIT(16)
#define BIT_AP_AHB_EMMC_SOFT_RST                  BIT(14)
#define BIT_AP_AHB_VSP_SOFT_RST                   BIT(13)
#define BIT_AP_AHB_SDIO1_SOFT_RST                 BIT(12)
#define BIT_AP_AHB_SDIO0_SOFT_RST                 BIT(11)
#define BIT_AP_AHB_CE_PUB_SOFT_RST                BIT(9)
#define BIT_AP_AHB_NANDC_SOFT_RST                 BIT(7)
#define BIT_AP_AHB_OTG_PHY_SOFT_RST               BIT(6)
#define BIT_AP_AHB_OTG_UTMI_SOFT_RST              BIT(5)
#define BIT_AP_AHB_OTG_SOFT_RST                   BIT(4)
#define BIT_AP_AHB_GSP_SOFT_RST                   BIT(3)
#define BIT_AP_AHB_DISP_MTX_SOFT_RST              BIT(2)
#define BIT_AP_AHB_DISPC_SOFT_RST                 BIT(1)
#define BIT_AP_AHB_DSI_SOFT_RST                   BIT(0)

#define BIT_OTG_PHY_SOFT_RST			BIT_AP_AHB_OTG_PHY_SOFT_RST
#define BIT_OTG_UTMI_SOFT_RST			BIT_AP_AHB_OTG_UTMI_SOFT_RST
#define BIT_OTG_SOFT_RST			BIT_AP_AHB_OTG_SOFT_RST

/* REG_AP_AHB_CA53_RST_SET */

#define BIT_AP_AHB_GIC_SOFT_RST                   BIT(20)
#define BIT_AP_AHB_CA53_ATB_SOFT_RST(x)           (((x) & 0xF) << 16)
#define BIT_AP_AHB_CA53_AXI_SOFT_RST              BIT(15)
#define BIT_AP_AHB_CA53_CS_DBG_SOFT_RST           BIT(14)
#define BIT_AP_AHB_CA53_L2_SOFT_RST               BIT(13)
#define BIT_AP_AHB_CA53_SOCDBG_SOFT_RST           BIT(12)
#define BIT_AP_AHB_CA53_ETM_SOFT_RST(x)           (((x) & 0xF) << 8)
#define BIT_AP_AHB_CA53_DBG_SOFT_RST(x)           (((x) & 0xF) << 4)
#define BIT_AP_AHB_CA53_CORE_SOFT_RST(x)          (((x) & 0xF))

/* REG_AP_AHB_AP_SYS_FORCE_SLEEP_CFG */

#define BIT_AP_AHB_CA53_C3_AUTO_SLP_EN            BIT(15)
#define BIT_AP_AHB_CA53_C2_AUTO_SLP_EN            BIT(14)
#define BIT_AP_AHB_CA53_C1_AUTO_SLP_EN            BIT(13)
#define BIT_AP_AHB_CA53_C0_AUTO_SLP_EN            BIT(12)
#define BIT_AP_AHB_CA53_C3_WFI_SHUTDOWN_EN        BIT(11)
#define BIT_AP_AHB_CA53_C2_WFI_SHUTDOWN_EN        BIT(10)
#define BIT_AP_AHB_CA53_C1_WFI_SHUTDOWN_EN        BIT(9)
#define BIT_AP_AHB_CA53_C0_WFI_SHUTDOWN_EN        BIT(8)
#define BIT_AP_AHB_MCU_CA53_C3_SLEEP              BIT(7)
#define BIT_AP_AHB_MCU_CA53_C2_SLEEP              BIT(6)
#define BIT_AP_AHB_MCU_CA53_C1_SLEEP              BIT(5)
#define BIT_AP_AHB_MCU_CA53_C0_SLEEP              BIT(4)
#define BIT_AP_AHB_AXI_LP_CTRL_DISABLE            BIT(3)
#define BIT_AP_AHB_PERI_FORCE_ON                  BIT(2)
#define BIT_AP_AHB_PERI_FORCE_OFF                 BIT(1)
#define BIT_AP_AHB_CGM_CLK_AP_AXI_AUTO_GATE_EN    BIT(0)

/* REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG */

#define BIT_AP_AHB_CA53_ATB_AUTO_GATE_EN          BIT(12)
#define BIT_AP_AHB_CA53_AXI_AUTO_GATE_EN          BIT(11)
#define BIT_AP_AHB_CACTIVE_SLV3_WAKEUP_EN         BIT(10)
#define BIT_AP_AHB_GSP_CKG_FORCE_EN               BIT(9)
#define BIT_AP_AHB_GSP_AUTO_GATE_EN               BIT(8)
#define BIT_AP_AHB_LP_AUTO_CTRL_EN                BIT(7)
#define BIT_AP_AHB_AP_MAINMTX_LP_DISABLE          BIT(6)
#define BIT_AP_AHB_AP_AHB_AUTO_GATE_EN            BIT(5)
#define BIT_AP_AHB_AP_EMC_AUTO_GATE_EN            BIT(4)
#define BIT_AP_AHB_CA53_EMC_AUTO_GATE_EN          BIT(3)
#define BIT_AP_AHB_CA53_DBG_FORCE_SLEEP           BIT(2)
#define BIT_AP_AHB_CA53_DBG_AUTO_GATE_EN          BIT(1)
#define BIT_AP_AHB_CA53_CORE_AUTO_GATE_EN         BIT(0)

/* REG_AP_AHB_HOLDING_PEN */

#define BIT_AP_AHB_HOLDING_PEN(x)                 (((x) & 0xFFFFFFFF))

/* REG_AP_AHB_JMP_ADDR_CA53_C0 */


/* REG_AP_AHB_JMP_ADDR_CA53_C1 */


/* REG_AP_AHB_JMP_ADDR_CA53_C2 */


/* REG_AP_AHB_JMP_ADDR_CA53_C3 */


/* REG_AP_AHB_CA53_C0_PU_LOCK */

#define BIT_AP_AHB_CA53_C0_PU_LOCK                BIT(0)

/* REG_AP_AHB_CA53_C1_PU_LOCK */

#define BIT_AP_AHB_CA53_C1_PU_LOCK                BIT(0)

/* REG_AP_AHB_CA53_C2_PU_LOCK */

#define BIT_AP_AHB_CA53_C2_PU_LOCK                BIT(0)

/* REG_AP_AHB_CA53_C3_PU_LOCK */

#define BIT_AP_AHB_CA53_C3_PU_LOCK                BIT(0)

/* REG_AP_AHB_CA53_CKG_DIV_CFG */

#define BIT_AP_AHB_CA53_DBG_CKG_DIV(x)            (((x) & 0x7) << 16)
#define BIT_AP_AHB_CA53_AXI_CKG_DIV(x)            (((x) & 0x7) << 8)
#define BIT_AP_AHB_CA53_MCU_CKG_DIV(x)            (((x) & 0x7) << 4)
#define BIT_AP_AHB_CA53_ATB_DIV(x)                (((x) & 0x7))

/* REG_AP_AHB_MCU_PAUSE */

#define BIT_AP_AHB_CPU_AXI_LP_CTRL_DISABLE        BIT(15)
#define BIT_AP_AHB_CRYPTODISABLE(x)               (((x) & 0xF) << 11)
#define BIT_AP_AHB_CA53_CORE_X_STOP_FORCE(x)      (((x) & 0xF) << 7)
#define BIT_AP_AHB_CA53_TOP_DEEP_STOP_FORCE       BIT(0)

/* REG_AP_AHB_MISC_CKG_EN */

#define BIT_AP_AHB_DPHY_REF_CKG_EN                BIT(1)
#define BIT_AP_AHB_DPHY_CFG_CKG_EN                BIT(0)

/* REG_AP_AHB_CA53_C0_AUTO_FORCE_SHUTDOWN_EN */


/* REG_AP_AHB_CA53_C1_AUTO_FORCE_SHUTDOWN_EN */


/* REG_AP_AHB_CA53_C2_AUTO_FORCE_SHUTDOWN_EN */


/* REG_AP_AHB_DISP_ASYNC_BRG */

#define BIT_AP_AHB_DISP_ASYNC_BRG_LP_NUM(x)       (((x) & 0xFFFF) << 1)
#define BIT_AP_AHB_DISP_ASYNC_BRG_LP_EB           BIT(0)

/* REG_AP_AHB_CA53_CKG_SEL_CFG */

#define BIT_AP_AHB_CA53_MCU_CKG_SEL(x)            (((x) & 0x3))

/* REG_AP_AHB_S5_LPC */

#define BIT_AP_AHB_CGM_MTX_S5_AUTO_GATE_EN        BIT(17)
#define BIT_AP_AHB_MAIN_S5_LP_EB                  BIT(16)
#define BIT_AP_AHB_MAIN_S5_LP_NUM(x)              (((x) & 0xFFFF))

/* REG_AP_AHB_AP_ASYNC_BRG */

#define BIT_AP_AHB_AP_ASYNC_BRG_LP_NUM(x)         (((x) & 0xFFFF) << 1)
#define BIT_AP_AHB_AP_ASYNC_BRG_LP_EB             BIT(0)

/* REG_AP_AHB_M0_LPC */

#define BIT_AP_AHB_MAIN_M0_LP_EB                  BIT(16)
#define BIT_AP_AHB_MAIN_M0_LP_NUM(x)              (((x) & 0xFFFF))

/* REG_AP_AHB_M1_LPC */

#define BIT_AP_AHB_MAIN_M1_LP_EB                  BIT(16)
#define BIT_AP_AHB_MAIN_M1_LP_NUM(x)              (((x) & 0xFFFF))

/* REG_AP_AHB_M2_LPC */

#define BIT_AP_AHB_MAIN_M2_LP_EB                  BIT(16)
#define BIT_AP_AHB_MAIN_M2_LP_NUM(x)              (((x) & 0xFFFF))

/* REG_AP_AHB_M3_LPC */

#define BIT_AP_AHB_MAIN_M3_LP_EB                  BIT(16)
#define BIT_AP_AHB_MAIN_M3_LP_NUM(x)              (((x) & 0xFFFF))

/* REG_AP_AHB_M4_LPC */

#define BIT_AP_AHB_MAIN_M4_LP_EB                  BIT(16)
#define BIT_AP_AHB_MAIN_M4_LP_NUM(x)              (((x) & 0xFFFF))

/* REG_AP_AHB_M5_LPC */

#define BIT_AP_AHB_MAIN_M5_LP_EB                  BIT(16)
#define BIT_AP_AHB_MAIN_M5_LP_NUM(x)              (((x) & 0xFFFF))

/* REG_AP_AHB_M6_LPC */

#define BIT_AP_AHB_MAIN_M6_LP_EB                  BIT(16)
#define BIT_AP_AHB_MAIN_M6_LP_NUM(x)              (((x) & 0xFFFF))

/* REG_AP_AHB_M7_LPC */

#define BIT_AP_AHB_MAIN_M7_LP_EB                  BIT(16)
#define BIT_AP_AHB_MAIN_M7_LP_NUM(x)              (((x) & 0xFFFF))

/* REG_AP_AHB_M8_LPC */

#define BIT_AP_AHB_MAIN_M8_LP_EB                  BIT(16)
#define BIT_AP_AHB_MAIN_M8_LP_NUM(x)              (((x) & 0xFFFF))

/* REG_AP_AHB_M9_LPC */

#define BIT_AP_AHB_MAIN_M9_LP_EB                  BIT(16)
#define BIT_AP_AHB_MAIN_M9_LP_NUM(x)              (((x) & 0xFFFF))

/* REG_AP_AHB_MAIN_LPC */

#define BIT_AP_AHB_CGM_MATRIX_AUTO_GATE_EN        BIT(17)
#define BIT_AP_AHB_MAIN_LP_EB                     BIT(16)
#define BIT_AP_AHB_MAIN_LP_NUM(x)                 (((x) & 0xFFFF))

/* REG_AP_AHB_S0_LPC */

#define BIT_AP_AHB_CGM_MTX_S0_AUTO_GATE_EN        BIT(17)
#define BIT_AP_AHB_MAIN_S0_LP_EB                  BIT(16)
#define BIT_AP_AHB_MAIN_S0_LP_NUM(x)              (((x) & 0xFFFF))

/* REG_AP_AHB_S1_LPC */

#define BIT_AP_AHB_CGM_MTX_S1_AUTO_GATE_EN        BIT(17)
#define BIT_AP_AHB_MAIN_S1_LP_EB                  BIT(16)
#define BIT_AP_AHB_MAIN_S1_LP_NUM(x)              (((x) & 0xFFFF))

/* REG_AP_AHB_S2_LPC */

#define BIT_AP_AHB_CGM_MTX_S2_AUTO_GATE_EN        BIT(17)
#define BIT_AP_AHB_MAIN_S2_LP_EB                  BIT(16)
#define BIT_AP_AHB_MAIN_S2_LP_NUM(x)              (((x) & 0xFFFF))

/* REG_AP_AHB_S3_LPC */

#define BIT_AP_AHB_CGM_MTX_S3_AUTO_GATE_EN        BIT(17)
#define BIT_AP_AHB_MAIN_S3_LP_EB                  BIT(16)
#define BIT_AP_AHB_MAIN_S3_LP_NUM(x)              (((x) & 0xFFFF))

/* REG_AP_AHB_S4_LPC */

#define BIT_AP_AHB_CGM_MTX_S4_AUTO_GATE_EN        BIT(17)
#define BIT_AP_AHB_MAIN_S4_LP_EB                  BIT(16)
#define BIT_AP_AHB_MAIN_S4_LP_NUM(x)              (((x) & 0xFFFF))

/* REG_AP_AHB_MERGE_M0_LPC */

#define BIT_AP_AHB_MERGE_M0_LP_EB                 BIT(16)
#define BIT_AP_AHB_MERGE_M0_LP_NUM(x)             (((x) & 0xFFFF))

/* REG_AP_AHB_MERGE_M1_LPC */

#define BIT_AP_AHB_MERGE_M1_LP_EB                 BIT(16)
#define BIT_AP_AHB_MERGE_M1_LP_NUM(x)             (((x) & 0xFFFF))

/* REG_AP_AHB_MERGE_M2_LPC */

#define BIT_AP_AHB_MERGE_M2_LP_EB                 BIT(16)
#define BIT_AP_AHB_MERGE_M2_LP_NUM(x)             (((x) & 0xFFFF))

/* REG_AP_AHB_MERGE_S0_LPC */

#define BIT_AP_AHB_CGM_MERGE_S0_AUTO_GATE_EN      BIT(17)
#define BIT_AP_AHB_MERGE_S0_LP_EB                 BIT(16)
#define BIT_AP_AHB_MERGE_S0_LP_NUM(x)             (((x) & 0xFFFF))

/* REG_AP_AHB_AP_QOS0 */

#define BIT_AP_AHB_ARQOS_USB(x)                   (((x) & 0xF) << 28)
#define BIT_AP_AHB_AWQOS_USB(x)                   (((x) & 0xF) << 24)
#define BIT_AP_AHB_ARQOS_CE(x)                    (((x) & 0xF) << 20)
#define BIT_AP_AHB_AWQOS_CE(x)                    (((x) & 0xF) << 16)
#define BIT_AP_AHB_ARQOS_DMA(x)                   (((x) & 0xF) << 12)
#define BIT_AP_AHB_AWQOS_DMA(x)                   (((x) & 0xF) << 8)
#define BIT_AP_AHB_ARQOS_CA53(x)                  (((x) & 0xF) << 4)
#define BIT_AP_AHB_AWQOS_CA53(x)                  (((x) & 0xF))

/* REG_AP_AHB_CPU_ASYNC_BRIDGE */

#define BIT_AP_AHB_CPU_SYS_LP_NUM(x)              (((x) & 0xFFFF) << 3)
#define BIT_AP_AHB_CPU_ASYNC_BRG_ACTIVE_SYNC_SEL  BIT(2)
#define BIT_AP_AHB_CPU_ASYNC_BRG_LP_FORCE         BIT(1)
#define BIT_AP_AHB_CPU_ASYNC_BRG_LP_EB            BIT(0)

/* REG_AP_AHB_AP_QOS1 */

#define BIT_AP_AHB_ARQOS_SDIO0(x)                 (((x) & 0xF) << 28)
#define BIT_AP_AHB_AWQOS_SDIO0(x)                 (((x) & 0xF) << 24)
#define BIT_AP_AHB_ARQOS_SDIO1(x)                 (((x) & 0xF) << 20)
#define BIT_AP_AHB_AWQOS_SDIO1(x)                 (((x) & 0xF) << 16)
#define BIT_AP_AHB_ARQOS_EMMC(x)                  (((x) & 0xF) << 12)
#define BIT_AP_AHB_AWQOS_EMMC(x)                  (((x) & 0xF) << 8)
#define BIT_AP_AHB_ARQOS_NANDC(x)                 (((x) & 0xF) << 4)
#define BIT_AP_AHB_AWQOS_NANDC(x)                 (((x) & 0xF))

/* REG_AP_AHB_AP_QOS2 */

#define BIT_AP_AHB_ARQOS_THRESHHOLD_CPU(x)        (((x) & 0xF) << 28)
#define BIT_AP_AHB_AWQOS_THRESHHOLD_CPU(x)        (((x) & 0xF) << 24)
#define BIT_AP_AHB_ARQOS_THRESHHOLD_MAIN(x)       (((x) & 0xF) << 20)
#define BIT_AP_AHB_AWQOS_THRESHHOLD_MAIN(x)       (((x) & 0xF) << 16)
#define BIT_AP_AHB_ARQOS_THRESHHOLD_MERGE(x)      (((x) & 0xF) << 12)
#define BIT_AP_AHB_AWQOS_THRESHHOLD_MERGE(x)      (((x) & 0xF) << 8)
#define BIT_AP_AHB_ARQOS_THRESHHOLD_DISP(x)       (((x) & 0xF) << 4)
#define BIT_AP_AHB_AWQOS_THRESHHOLD_DISP(x)       (((x) & 0xF))

/* REG_AP_AHB_CA53_L2 */

#define BIT_AP_AHB_L2FLUSHDONE                    BIT(5)
#define BIT_AP_AHB_L2FLUSHREQ                     BIT(4)
#define BIT_AP_AHB_L2QDENY                        BIT(3)
#define BIT_AP_AHB_L2QACCEPTN                     BIT(2)
#define BIT_AP_AHB_L2QREQN                        BIT(1)
#define BIT_AP_AHB_L2QACTIVE                      BIT(0)

/* REG_AP_AHB_CA53_MTX_LP */

#define BIT_AP_AHB_CA53_MTX_LP_NUM(x)             (((x) & 0xFFFF) << 4)
#define BIT_AP_AHB_CA53_MTX_S2_LP_EB              BIT(3)
#define BIT_AP_AHB_CA53_MTX_S1_LP_EB              BIT(2)
#define BIT_AP_AHB_CA53_MTX_S0_LP_EB              BIT(1)
#define BIT_AP_AHB_CA53_MTX_M0_LP_EB              BIT(0)

/* REG_AP_AHB_CA53_STANDBY_STATUS */

#define BIT_AP_AHB_CA53_STANDBYWFIL2              BIT(12)
#define BIT_AP_AHB_CA53_ETMSTANDBYWFX(x)          (((x) & 0xF) << 8)
#define BIT_AP_AHB_CA53_STANDBYWFE(x)             (((x) & 0xF) << 4)
#define BIT_AP_AHB_CA53_STANDBYWFI(x)             (((x) & 0xF))

/* REG_AP_AHB_SYS_RST */

#define BIT_AP_AHB_SYS_SOFT_RST_REQ_GSP           BIT(2)
#define BIT_AP_AHB_SYS_SOFT_RST_REQ_DISP          BIT(1)
#define BIT_AP_AHB_SYS_SOFT_RST_REQ_VSP           BIT(0)

/* REG_AP_AHB_LVDS_PLL_CFG0 */


/* REG_AP_AHB_LVDS_PLL_CFG1 */


/* REG_AP_AHB_AP_QOS_CFG */

#define BIT_AP_AHB_QOS_R_TMC(x)                   (((x) & 0xF) << 20)
#define BIT_AP_AHB_QOS_W_TMC(x)                   (((x) & 0xF) << 16)
#define BIT_AP_AHB_QOS_R_DISPC(x)                 (((x) & 0xF) << 4)
#define BIT_AP_AHB_QOS_W_DISPC(x)                 (((x) & 0xF))

/* REG_AP_AHB_OTG_PHY_TUNE */

#define BIT_AP_AHB_OTG_TXPREEMPPULSETUNE          BIT(20)
#define BIT_AP_AHB_OTG_TXRESTUNE(x)               (((x) & 0x3) << 18)
#define BIT_AP_AHB_OTG_TXHSXVTUNE(x)              (((x) & 0x3) << 16)
#define BIT_AP_AHB_OTG_TXVREFTUNE(x)              (((x) & 0xF) << 12)
#define BIT_AP_AHB_OTG_TXPREEMPAMPTUNE(x)         (((x) & 0x3) << 10)
#define BIT_AP_AHB_OTG_TXRISETUNE(x)              (((x) & 0x3) << 8)
#define BIT_AP_AHB_OTG_TXFSLSTUNE(x)              (((x) & 0xF) << 4)
#define BIT_AP_AHB_OTG_SQRXTUNE(x)                (((x) & 0x7))

/* REG_AP_AHB_OTG_PHY_TEST */

#define BIT_AP_AHB_OTG_ATERESET                   BIT(31)
#define BIT_AP_AHB_OTG_VBUS_VALID_PHYREG          BIT(24)
#define BIT_AP_AHB_OTG_VBUS_VALID_PHYREG_SEL      BIT(23)
#define BIT_AP_AHB_OTG_VBUS_VALID_EXT             BIT(22)
#define BIT_AP_AHB_OTG_TESTBURNIN                 BIT(21)
#define BIT_AP_AHB_OTG_LOOPBACKENB                BIT(20)
#define BIT_AP_AHB_OTG_TESTDATAOUT(x)             (((x) & 0xF) << 16)
#define BIT_AP_AHB_OTG_VATESTENB(x)               (((x) & 0x3) << 14)
#define BIT_AP_AHB_OTG_TESTCLK                    BIT(13)
#define BIT_AP_AHB_OTG_TESTDATAOUTSEL             BIT(12)
#define BIT_AP_AHB_OTG_TESTADDR(x)                (((x) & 0xF) << 8)
#define BIT_AP_AHB_OTG_TESTDATAIN(x)              (((x) & 0xFF))

/* REG_AP_AHB_OTG_PHY_CTRL */

#define BIT_AP_AHB_USB2_CON_TESTMODE              BIT(31)
#define BIT_AP_AHB_UTMI_WIDTH_SEL                 BIT(30)
#define BIT_AP_AHB_USB2_DATABUS16_8               BIT(29)
#define BIT_AP_AHB_OTG_SS_SCALEDOWNMODE(x)        (((x) & 0x3) << 25)
#define BIT_AP_AHB_TEST_ANALOG_MODE1_USBPHY       BIT(24)
#define BIT_AP_AHB_OTG_TXBITSTUFFENH              BIT(23)
#define BIT_AP_AHB_OTG_TXBITSTUFFEN               BIT(22)
#define BIT_AP_AHB_OTG_DMPULLDOWN                 BIT(21)
#define BIT_AP_AHB_OTG_DPPULLDOWN                 BIT(20)
#define BIT_AP_AHB_OTG_DMPULLUP                   BIT(9)
#define BIT_AP_AHB_OTG_COMMONONN                  BIT(8)
#define BIT_AP_AHB_USB2_PHY_IDDIG                 BIT(3)
#define BIT_AP_AHB_OTG_FSEL(x)                    (((x) & 0x7))

/* REG_AP_AHB_OTG_CTRL0 */

#define BIT_AP_AHB_USB20_TUNEHSAMP(x)             (((x) & 0x3) << 30)
#define BIT_AP_AHB_USB20_TUNEPLLS(x)              (((x) & 0x3) << 28)
#define BIT_AP_AHB_USB20_TUNERISE(x)              (((x) & 0x3) << 26)
#define BIT_AP_AHB_USB20_TUNEDSC(x)               (((x) & 0x3) << 24)
#define BIT_AP_AHB_USB20_TUNEOTG(x)               (((x) & 0x7) << 21)
#define BIT_AP_AHB_USB20_TUNESQ(x)                (((x) & 0xF) << 17)
#define BIT_AP_AHB_USB20_RESERVED(x)              (((x) & 0xFFFF))

/* REG_AP_AHB_OTG_CTRL1 */

#define BIT_AP_AHB_USB20_BIST_MODE(x)             (((x) & 0x1F) << 23)
#define BIT_AP_AHB_USB20_BYPASS_DRV_DM            BIT(22)
#define BIT_AP_AHB_USB20_BYPASS_DRV_DP            BIT(21)
#define BIT_AP_AHB_USB20_SAMPLER_SEL              BIT(20)
#define BIT_AP_AHB_HSIC_PLLON                     BIT(16)
#define BIT_AP_AHB_USB20_REXTENABLE               BIT(15)
#define BIT_AP_AHB_USB20_S_ID                     BIT(14)
#define BIT_AP_AHB_USB20_TFREGRES(x)              (((x) & 0x3F) << 8)
#define BIT_AP_AHB_USB20_TFHSRES(x)               (((x) & 0x1F) << 3)
#define BIT_AP_AHB_USB20_TUNEEQ(x)                (((x) & 0x7))

/* REG_AP_AHB_DSI_PHY */

#define BIT_AP_AHB_MIPI_DSI_RESERVED(x)           (((x) & 0xFFFF) << 9)
#define BIT_AP_AHB_MIPI_DSI_TX_RCTL(x)            (((x) & 0xF) << 5)
#define BIT_AP_AHB_MIPI_DSI_TRIMBG(x)             (((x) & 0xF) << 1)
#define BIT_AP_AHB_MIPI_DSI_IF_SEL                BIT(0)

/* REG_AP_AHB_CE_SEC_EB */

#define BIT_AP_AHB_CE_SEC_EB                      BIT(0)

/* REG_AP_AHB_CE_SEC_SOFT_RST */

#define BIT_AP_AHB_CE_SEC_SOFT_RST                BIT(0)

/* REG_AP_AHB_DMA_SOFT_RST */

#define BIT_AP_AHB_DMA_SOFT_RST                   BIT(0)

/* REG_AP_AHB_DMA_SEC_EB */

#define BIT_AP_AHB_DMA_SEC_EB                     BIT(0)

/* REG_AP_AHB_CHIP_ID */

#define BIT_AP_AHB_CHIP_ID(x)                     (((x) & 0xFFFFFFFF))


#endif /* AP_AHB_H */
