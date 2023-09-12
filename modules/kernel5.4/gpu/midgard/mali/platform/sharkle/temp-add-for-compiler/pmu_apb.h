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


#ifndef PMU_APB_H
#define PMU_APB_H



#define REG_PMU_APB_PD_CA53_TOP_CFG                            (0x0000)
#define REG_PMU_APB_PD_CA53_C0_CFG                             (0x0004)
#define REG_PMU_APB_PD_CA53_C1_CFG                             (0x0008)
#define REG_PMU_APB_PD_CA53_C2_CFG                             (0x000C)
#define REG_PMU_APB_PD_CA53_C3_CFG                             (0x0010)
#define REG_PMU_APB_PD_CA53_TOP_CFG2                           (0x0014)
#define REG_PMU_APB_PD_AP_SYS_CFG                              (0x0018)
#define REG_PMU_APB_PD_MM_TOP_CFG                              (0x001C)
#define REG_PMU_APB_PD_GPU_TOP_CFG                             (0x0020)
#define REG_PMU_APB_PD_WTLCP_LTE_P1_CFG                        (0x0028)
#define REG_PMU_APB_PD_WTLCP_LTE_P2_CFG                        (0x002C)
#define REG_PMU_APB_PD_WTLCP_LDSP_CFG                          (0x0034)
#define REG_PMU_APB_PD_WTLCP_TGDSP_CFG                         (0x0038)
#define REG_PMU_APB_PD_WTLCP_HU3GE_A_CFG                       (0x003C)
#define REG_PMU_APB_PD_WTLCP_SYS_CFG                           (0x0044)
#define REG_PMU_APB_PD_PUBCP_SYS_CFG                           (0x0048)
#define REG_PMU_APB_PD_WTLCP_LTE_P3_CFG                        (0x004C)
#define REG_PMU_APB_PD_WTLCP_LTE_P4_CFG                        (0x0050)
#define REG_PMU_APB_PUBCP_FRC_STOP_REQ_FOR_WTL                 (0x0054)
#define REG_PMU_APB_PD_PUB_SYS_CFG                             (0x005C)
#define REG_PMU_APB_AP_WAKEUP_POR_CFG                          (0x0060)
#define REG_PMU_APB_XTL_WAIT_CNT                               (0x0070)
#define REG_PMU_APB_XTLBUF_WAIT_CNT                            (0x0074)
#define REG_PMU_APB_PLL_WAIT_CNT1                              (0x0078)
#define REG_PMU_APB_PLL_WAIT_CNT2                              (0x007C)
#define REG_PMU_APB_XTL0_REL_CFG                               (0x0080)
#define REG_PMU_APB_XTL1_REL_CFG                               (0x0084)
#define REG_PMU_APB_ISPPLL_REL_CFG                             (0x0088)
#define REG_PMU_APB_XTLBUF0_REL_CFG                            (0x008C)
#define REG_PMU_APB_XTLBUF1_REL_CFG                            (0x0090)
#define REG_PMU_APB_MPLL_REL_CFG                               (0x0094)
#define REG_PMU_APB_DPLL_REL_CFG                               (0x0098)
#define REG_PMU_APB_LTEPLL_REL_CFG                             (0x009C)
#define REG_PMU_APB_TWPLL_REL_CFG                              (0x00A0)
#define REG_PMU_APB_GPLL_REL_CFG                               (0x00A8)
#define REG_PMU_APB_RPLL_REL_CFG                               (0x00AC)
#define REG_PMU_APB_CP_SOFT_RST                                (0x00B0)
#define REG_PMU_APB_CP_SLP_STATUS_DBG0                         (0x00B4)
#define REG_PMU_APB_PWR_STATUS0_DBG                            (0x00BC)
#define REG_PMU_APB_PWR_STATUS1_DBG                            (0x00C0)
#define REG_PMU_APB_PWR_STATUS2_DBG                            (0x00C4)
#define REG_PMU_APB_PUB_SYS_AUTO_LIGHT_SLEEP_ENABLE            (0x00C8)
#define REG_PMU_APB_SLEEP_CTRL                                 (0x00CC)
#define REG_PMU_APB_DDR_SLEEP_CTRL                             (0x00D0)
#define REG_PMU_APB_SLEEP_STATUS                               (0x00D4)
#define REG_PMU_APB_PUB_SYS_SLEEP_BYPASS_CFG                   (0x00D8)
#define REG_PMU_APB_PUB_SYS_DEEP_SLEEP_POLL0                   (0x00DC)
#define REG_PMU_APB_PUB_SYS_DEEP_SLEEP_POLL1                   (0x00E0)
#define REG_PMU_APB_CA53_TOP_CFG                               (0x00E4)
#define REG_PMU_APB_CA53_C0_CFG                                (0x00E8)
#define REG_PMU_APB_CA53_C1_CFG                                (0x00EC)
#define REG_PMU_APB_CA53_C2_CFG                                (0x00F0)
#define REG_PMU_APB_CA53_C3_CFG                                (0x00F4)
#define REG_PMU_APB_DDR_CHN_SLEEP_CTRL0                        (0x00F8)
#define REG_PMU_APB_DDR_CHN_SLEEP_CTRL1                        (0x00FC)
#define REG_PMU_APB_PD_WCN_SYS_CFG                             (0x0100)
#define REG_PMU_APB_PD_WIFI_WRAP_CFG                           (0x0104)
#define REG_PMU_APB_PD_GNSS_WRAP_CFG                           (0x0108)
#define REG_PMU_APB_PWR_STATUS3_DBG                            (0x010C)
#define REG_PMU_APB_DDR_OP_MODE_CFG                            (0x012C)
#define REG_PMU_APB_DDR_PHY_RET_CFG                            (0x0130)
#define REG_PMU_APB_CLK26M_SEL_CFG                             (0x0134)
#define REG_PMU_APB_BISR_DONE_STATUS                           (0x0138)
#define REG_PMU_APB_BISR_BUSY_STATUS                           (0x013C)
#define REG_PMU_APB_BISR_BYP_CFG                               (0x0140)
#define REG_PMU_APB_BISR_EN_CFG                                (0x0144)
#define REG_PMU_APB_CGM_AUTO_GATE_SEL_CFG0                     (0x0148)
#define REG_PMU_APB_CGM_AUTO_GATE_SEL_CFG1                     (0x014C)
#define REG_PMU_APB_CGM_AUTO_GATE_SEL_CFG2                     (0x0150)
#define REG_PMU_APB_CGM_AUTO_GATE_SEL_CFG3                     (0x0154)
#define REG_PMU_APB_CGM_FORCE_EN_CFG0                          (0x0158)
#define REG_PMU_APB_CGM_FORCE_EN_CFG1                          (0x015C)
#define REG_PMU_APB_CGM_FORCE_EN_CFG2                          (0x0160)
#define REG_PMU_APB_CGM_FORCE_EN_CFG3                          (0x0164)
#define REG_PMU_APB_SLEEP_XTLON_CTRL                           (0x0168)
#define REG_PMU_APB_MEM_SLP_CFG                                (0x016C)
#define REG_PMU_APB_MEM_SD_CFG                                 (0x0170)
#define REG_PMU_APB_CA53_CORE_WAKEUP_EN                        (0x0174)
#define REG_PMU_APB_SP_SYS_HOLD_CGM_EN                         (0x0178)
#define REG_PMU_APB_PWR_CNT_WAIT_CFG0                          (0x017C)
#define REG_PMU_APB_PWR_CNT_WAIT_CFG1                          (0x0180)
#define REG_PMU_APB_RC0_REL_CFG                                (0x0184)
#define REG_PMU_APB_RC1_REL_CFG                                (0x0188)
#define REG_PMU_APB_RC_CNT_WAIT_CFG                            (0x018C)
#define REG_PMU_APB_MEM_AUTO_SLP_CFG                           (0x0190)
#define REG_PMU_APB_MEM_AUTO_SD_CFG                            (0x0194)
#define REG_PMU_APB_WAKEUP_LOCK_EN                             (0x01A0)
#define REG_PMU_APB_WCN_SYS_CORE_INT_DISABLE                   (0x01A4)
#define REG_PMU_APB_GNSS_WRAP_CORE_INT_DISABLE                 (0x01A8)
#define REG_PMU_APB_WTLCP_TGDSP_CORE_INT_DISABLE               (0x01B0)
#define REG_PMU_APB_WTLCP_LDSP_CORE_INT_DISABLE                (0x01B4)
#define REG_PMU_APB_PUBCP_CORE_INT_DISABLE                     (0x01B8)
#define REG_PMU_APB_CA53_C0_CORE_INT_DISABLE                   (0x01BC)
#define REG_PMU_APB_CA53_C1_CORE_INT_DISABLE                   (0x01C0)
#define REG_PMU_APB_CA53_C2_CORE_INT_DISABLE                   (0x01C4)
#define REG_PMU_APB_CA53_C3_CORE_INT_DISABLE                   (0x01C8)
#define REG_PMU_APB_WTLCP_TGDSP_DSLP_ENA                       (0x0200)
#define REG_PMU_APB_WTLCP_LDSP_DSLP_ENA                        (0x0204)
#define REG_PMU_APB_AP_DSLP_ENA                                (0x0208)
#define REG_PMU_APB_PUBCP_DSLP_ENA                             (0x020C)
#define REG_PMU_APB_WTLCP_DSLP_ENA                             (0x0210)
#define REG_PMU_APB_CA53_TOP_DSLP_ENA                          (0x0214)
#define REG_PMU_APB_SP_SYS_DSLP_ENA                            (0x0218)
#define REG_PMU_APB_LIGHT_SLEEP_ENABLE                         (0x0230)
#define REG_PMU_APB_LIGHT_SLEEP_MON                            (0x0234)
#define REG_PMU_APB_WCN_SYS_DSLP_ENA                           (0x0244)
#define REG_PMU_APB_WIFI_WRAP_DSLP_ENA                         (0x0248)
#define REG_PMU_APB_GNSS_WRAP_DSLP_ENA                         (0x024C)
#define REG_PMU_APB_PUB_ACC_RDY                                (0x0250)
#define REG_PMU_APB_PUB_CLK_RDY                                (0x0254)
#define REG_PMU_APB_EIC_SEL                                    (0x0258)
#define REG_PMU_APB_AXI_LP_CTRL_DISABLE                        (0x0260)
#define REG_PMU_APB_PMU_DEBUG                                  (0x0270)
#define REG_PMU_APB_SLEEP_CNT_CLR                              (0x0274)
#define REG_PMU_APB_LVDSRFPLL_REL_CFG                          (0x0280)
#define REG_PMU_APB_PAD_OUT_ADIE_CTRL0                         (0x0290)
#define REG_PMU_APB_PAD_OUT_ADIE_CTRL1                         (0x0294)
#define REG_PMU_APB_BISR_FORCE_SEL                             (0x0300)
#define REG_PMU_APB_AON_MEM_CTRL                               (0x0330)
#define REG_PMU_APB_PWR_DOMAIN_INT_CLR                         (0x0334)
#define REG_PMU_APB_DDR_SLP_WAIT_CNT                           (0x0338)
#define REG_PMU_APB_PMU_CLK_DIV_CFG                            (0x033C)
#define REG_PMU_APB_CGM_PMU_SEL                                (0x0340)
#define REG_PMU_APB_PWR_DGB_PARAMETER                          (0x0344)
#define REG_PMU_APB_CA53_C0_DSLP_ENA                           (0x0348)
#define REG_PMU_APB_CA53_C1_DSLP_ENA                           (0x034C)
#define REG_PMU_APB_CA53_C2_DSLP_ENA                           (0x0350)
#define REG_PMU_APB_CA53_C3_DSLP_ENA                           (0x0354)
#define REG_PMU_APB_CA53_GIC_RST_EN                            (0x0358)
#define REG_PMU_APB_ANALOG_PHY_PD_CFG                          (0x035C)
#define REG_PMU_APB_PUB_SYS_DEEP_SLEEP_SEL                     (0x0360)
#define REG_PMU_APB_PD_CA53_C0_SHUTDOWN_MARK_STATUS            (0x3000)
#define REG_PMU_APB_PD_CA53_C1_SHUTDOWN_MARK_STATUS            (0x3004)
#define REG_PMU_APB_PD_CA53_C2_SHUTDOWN_MARK_STATUS            (0x3008)
#define REG_PMU_APB_PD_CA53_C3_SHUTDOWN_MARK_STATUS            (0x300C)
#define REG_PMU_APB_PD_CA53_TOP_SHUTDOWN_MARK_STATUS           (0x3010)
#define REG_PMU_APB_PD_AP_SYS_SHUTDOWN_MARK_STATUS             (0x3014)
#define REG_PMU_APB_PD_GPU_TOP_SHUTDOWN_MARK_STATUS            (0x3018)
#define REG_PMU_APB_PD_MM_TOP_SHUTDOWN_MARK_STATUS             (0x301C)
#define REG_PMU_APB_PD_WTLCP_TD_SHUTDOWN_MARK_STATUS           (0x3020)
#define REG_PMU_APB_PD_WTLCP_LTE_P1_SHUTDOWN_MARK_STATUS       (0x3024)
#define REG_PMU_APB_PD_WTLCP_LTE_P2_SHUTDOWN_MARK_STATUS       (0x3028)
#define REG_PMU_APB_PD_WTLCP_LDSP_SHUTDOWN_MARK_STATUS         (0x3030)
#define REG_PMU_APB_PD_WTLCP_TGDSP_SHUTDOWN_MARK_STATUS        (0x3034)
#define REG_PMU_APB_PD_WTLCP_HU3GE_A_SHUTDOWN_MARK_STATUS      (0x3038)
#define REG_PMU_APB_PD_WTLCP_HU3GE_B_SHUTDOWN_MARK_STATUS      (0x303C)
#define REG_PMU_APB_PD_WTLCP_SYS_SHUTDOWN_MARK_STATUS          (0x3040)
#define REG_PMU_APB_PD_PUBCP_SYS_SHUTDOWN_MARK_STATUS          (0x3044)
#define REG_PMU_APB_PD_WTLCP_LTE_P3_SHUTDOWN_MARK_STATUS       (0x3048)
#define REG_PMU_APB_PD_WTLCP_LTE_P4_SHUTDOWN_MARK_STATUS       (0x304C)
#define REG_PMU_APB_PD_PUB_SYS_SHUTDOWN_MARK_STATUS            (0x3054)
#define REG_PMU_APB_PD_WCN_SYS_SHUTDOWN_MARK_STATUS            (0x3058)
#define REG_PMU_APB_PD_WIFI_WRAP_SHUTDOWN_MARK_STATUS          (0x305C)
#define REG_PMU_APB_PD_GNSS_WRAP_SHUTDOWN_MARK_STATUS          (0x3060)
#define REG_PMU_APB_AP_SYS_SLEEP_CNT                           (0x3064)
#define REG_PMU_APB_WTLCP_SYS_SLEEP_CNT                        (0x3068)
#define REG_PMU_APB_PUBCP_SYS_SLEEP_CNT                        (0x306C)
#define REG_PMU_APB_WCN_SYS_SLEEP_CNT                          (0x3070)
#define REG_PMU_APB_PUB_SYS_LIGHT_SLEEP_CNT                    (0x3074)
#define REG_PMU_APB_AP_DEEP_SLEEP_CNT                          (0x3078)
#define REG_PMU_APB_SP_SYS_DEEP_SLEEP_CNT                      (0x307C)
#define REG_PMU_APB_WTLCP_DEEP_SLEEP_CNT                       (0x3080)
#define REG_PMU_APB_PUBCP_DEEP_SLEEP_CNT                       (0x3084)
#define REG_PMU_APB_WCN_SYS_DEEP_SLEEP_CNT                     (0x3088)
#define REG_PMU_APB_PUB_SYS_DEEP_SLEEP_CNT                     (0x308C)
#define REG_PMU_APB_AP_LIGHT_SLEEP_CNT                         (0x3090)
#define REG_PMU_APB_WTLCP_LIGHT_SLEEP_CNT                      (0x3094)
#define REG_PMU_APB_PUBCP_LIGHT_SLEEP_CNT                      (0x3098)
#define REG_PMU_APB_WCN_SYS_LIGHT_SLEEP_CNT                    (0x309C)
#define REG_PMU_APB_AON_SYS_LIGHT_SLEEP_CNT                    (0x30A0)
#define REG_PMU_APB_SYS_SOFT_RST_BUSY                          (0x30A4)
#define REG_PMU_APB_REG_SYS_SRST_FRC_LP_ACK                    (0x30A8)
#define REG_PMU_APB_SOFT_RST_SEL                               (0x30AC)
#define REG_PMU_APB_REG_SYS_DDR_PWR_HS_ACK                     (0x30B0)
#define REG_PMU_APB_CSI_DSI_PWR_CNT_DONE                       (0x30B4)
#define REG_PMU_APB_PD_AP_SYS_DBG_SHUTDOWN__EN                 (0x30B8)
#define REG_PMU_APB_WCN_SYS_SLEEP_XTL_ON_SEL                   (0x30BC)
#define REG_PMU_APB_EIC_SYS_SEL                                (0x30C0)
#define REG_PMU_APB_DDR_SLP_CTRL_STATE                         (0x30C4)

/* REG_PMU_APB_PD_CA53_TOP_CFG */

#define BIT_PMU_APB_PD_CA53_TOP_DBG_SHUTDOWN_EN                  BIT(28)
#define BIT_PMU_APB_PD_CA53_TOP_PD_SEL                           BIT(27)
#define BIT_PMU_APB_PD_CA53_TOP_FORCE_SHUTDOWN                   BIT(25)
#define BIT_PMU_APB_PD_CA53_TOP_AUTO_SHUTDOWN_EN                 BIT(24)
#define BIT_PMU_APB_PD_CA53_TOP_PWR_ON_SEQ_DLY(x)                (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_CA53_TOP_ISO_ON_DLY(x)                    (((x) & 0xFF))

/* REG_PMU_APB_PD_CA53_C0_CFG */

#define BIT_PMU_APB_PD_CA53_C0_WFI_SHUTDOWN_EN                   BIT(29)
#define BIT_PMU_APB_PD_CA53_C0_DBG_SHUTDOWN_EN                   BIT(28)
#define BIT_PMU_APB_PD_CA53_C0_PD_SEL                            BIT(27)
#define BIT_PMU_APB_PD_CA53_C0_FORCE_SHUTDOWN                    BIT(25)
#define BIT_PMU_APB_PD_CA53_C0_AUTO_SHUTDOWN_EN                  BIT(24)
#define BIT_PMU_APB_PD_CA53_C0_PWR_ON_DLY(x)                     (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_CA53_C0_PWR_ON_SEQ_DLY(x)                 (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_CA53_C0_ISO_ON_DLY(x)                     (((x) & 0xFF))

/* REG_PMU_APB_PD_CA53_C1_CFG */

#define BIT_PMU_APB_PD_CA53_C1_WFI_SHUTDOWN_EN                   BIT(29)
#define BIT_PMU_APB_PD_CA53_C1_DBG_SHUTDOWN_EN                   BIT(28)
#define BIT_PMU_APB_PD_CA53_C1_PD_SEL                            BIT(27)
#define BIT_PMU_APB_PD_CA53_C1_FORCE_SHUTDOWN                    BIT(25)
#define BIT_PMU_APB_PD_CA53_C1_AUTO_SHUTDOWN_EN                  BIT(24)
#define BIT_PMU_APB_PD_CA53_C1_PWR_ON_DLY(x)                     (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_CA53_C1_PWR_ON_SEQ_DLY(x)                 (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_CA53_C1_ISO_ON_DLY(x)                     (((x) & 0xFF))

/* REG_PMU_APB_PD_CA53_C2_CFG */

#define BIT_PMU_APB_PD_CA53_C2_WFI_SHUTDOWN_EN                   BIT(29)
#define BIT_PMU_APB_PD_CA53_C2_DBG_SHUTDOWN_EN                   BIT(28)
#define BIT_PMU_APB_PD_CA53_C2_PD_SEL                            BIT(27)
#define BIT_PMU_APB_PD_CA53_C2_FORCE_SHUTDOWN                    BIT(25)
#define BIT_PMU_APB_PD_CA53_C2_AUTO_SHUTDOWN_EN                  BIT(24)
#define BIT_PMU_APB_PD_CA53_C2_PWR_ON_DLY(x)                     (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_CA53_C2_PWR_ON_SEQ_DLY(x)                 (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_CA53_C2_ISO_ON_DLY(x)                     (((x) & 0xFF))

/* REG_PMU_APB_PD_CA53_C3_CFG */

#define BIT_PMU_APB_PD_CA53_C3_WFI_SHUTDOWN_EN                   BIT(29)
#define BIT_PMU_APB_PD_CA53_C3_DBG_SHUTDOWN_EN                   BIT(28)
#define BIT_PMU_APB_PD_CA53_C3_PD_SEL                            BIT(27)
#define BIT_PMU_APB_PD_CA53_C3_FORCE_SHUTDOWN                    BIT(25)
#define BIT_PMU_APB_PD_CA53_C3_AUTO_SHUTDOWN_EN                  BIT(24)
#define BIT_PMU_APB_PD_CA53_C3_PWR_ON_DLY(x)                     (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_CA53_C3_PWR_ON_SEQ_DLY(x)                 (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_CA53_C3_ISO_ON_DLY(x)                     (((x) & 0xFF))

/* REG_PMU_APB_PD_CA53_TOP_CFG2 */

#define BIT_PMU_APB_PD_CA53_TOP_DCDC_PWR_ON_DLY(x)               (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_CA53_TOP_DCDC_PWR_OFF_DLY(x)              (((x) & 0xFF))

/* REG_PMU_APB_PD_AP_SYS_CFG */

#define BIT_PMU_APB_PD_AP_SYS_FORCE_SHUTDOWN                     BIT(25)
#define BIT_PMU_APB_PD_AP_SYS_AUTO_SHUTDOWN_EN                   BIT(24)
#define BIT_PMU_APB_PD_AP_SYS_PWR_ON_DLY(x)                      (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_AP_SYS_PWR_ON_SEQ_DLY(x)                  (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_AP_SYS_ISO_ON_DLY(x)                      (((x) & 0xFF))

/* REG_PMU_APB_PD_MM_TOP_CFG */

#define BIT_PMU_APB_PD_MM_TOP_FORCE_SHUTDOWN                     BIT(25)
#define BIT_PMU_APB_PD_MM_TOP_AUTO_SHUTDOWN_EN                   BIT(24)
#define BIT_PMU_APB_PD_MM_TOP_PWR_ON_DLY(x)                      (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_MM_TOP_PWR_ON_SEQ_DLY(x)                  (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_MM_TOP_ISO_ON_DLY(x)                      (((x) & 0xFF))

/* REG_PMU_APB_PD_GPU_TOP_CFG */

#define BIT_PMU_APB_PD_GPU_TOP_FORCE_SHUTDOWN                    BIT(25)
#define BIT_PMU_APB_PD_GPU_TOP_AUTO_SHUTDOWN_EN                  BIT(24)
#define BIT_PMU_APB_PD_GPU_TOP_PWR_ON_DLY(x)                     (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_GPU_TOP_PWR_ON_SEQ_DLY(x)                 (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_GPU_TOP_ISO_ON_DLY(x)                     (((x) & 0xFF))

/* REG_PMU_APB_PD_WTLCP_LTE_P1_CFG */

#define BIT_PMU_APB_PD_WTLCP_LTE_P1_FORCE_SHUTDOWN               BIT(25)
#define BIT_PMU_APB_PD_WTLCP_LTE_P1_AUTO_SHUTDOWN_EN             BIT(24)
#define BIT_PMU_APB_PD_WTLCP_LTE_P1_PWR_ON_DLY(x)                (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_WTLCP_LTE_P1_PWR_ON_SEQ_DLY(x)            (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_WTLCP_LTE_P1_ISO_ON_DLY(x)                (((x) & 0xFF))

/* REG_PMU_APB_PD_WTLCP_LTE_P2_CFG */

#define BIT_PMU_APB_PD_WTLCP_LTE_P2_FORCE_SHUTDOWN               BIT(25)
#define BIT_PMU_APB_PD_WTLCP_LTE_P2_AUTO_SHUTDOWN_EN             BIT(24)
#define BIT_PMU_APB_PD_WTLCP_LTE_P2_PWR_ON_DLY(x)                (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_WTLCP_LTE_P2_PWR_ON_SEQ_DLY(x)            (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_WTLCP_LTE_P2_ISO_ON_DLY(x)                (((x) & 0xFF))

/* REG_PMU_APB_PD_WTLCP_LDSP_CFG */

#define BIT_PMU_APB_PD_WTLCP_LDSP_PD_SEL                         BIT(27)
#define BIT_PMU_APB_PD_WTLCP_LDSP_FORCE_SHUTDOWN                 BIT(25)
#define BIT_PMU_APB_PD_WTLCP_LDSP_AUTO_SHUTDOWN_EN               BIT(24)
#define BIT_PMU_APB_PD_WTLCP_LDSP_PWR_ON_DLY(x)                  (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_WTLCP_LDSP_PWR_ON_SEQ_DLY(x)              (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_WTLCP_LDSP_ISO_ON_DLY(x)                  (((x) & 0xFF))

/* REG_PMU_APB_PD_WTLCP_TGDSP_CFG */

#define BIT_PMU_APB_PD_WTLCP_TGDSP_PD_SEL                        BIT(27)
#define BIT_PMU_APB_PD_WTLCP_TGDSP_FORCE_SHUTDOWN                BIT(25)
#define BIT_PMU_APB_PD_WTLCP_TGDSP_AUTO_SHUTDOWN_EN              BIT(24)
#define BIT_PMU_APB_PD_WTLCP_TGDSP_PWR_ON_DLY(x)                 (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_WTLCP_TGDSP_PWR_ON_SEQ_DLY(x)             (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_WTLCP_TGDSP_ISO_ON_DLY(x)                 (((x) & 0xFF))

/* REG_PMU_APB_PD_WTLCP_HU3GE_A_CFG */

#define BIT_PMU_APB_PD_WTLCP_HU3GE_A_FORCE_SHUTDOWN              BIT(25)
#define BIT_PMU_APB_PD_WTLCP_HU3GE_A_AUTO_SHUTDOWN_EN            BIT(24)
#define BIT_PMU_APB_PD_WTLCP_HU3GE_A_PWR_ON_DLY(x)               (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_WTLCP_HU3GE_A_PWR_ON_SEQ_DLY(x)           (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_WTLCP_HU3GE_A_ISO_ON_DLY(x)               (((x) & 0xFF))

/* REG_PMU_APB_PD_WTLCP_SYS_CFG */

#define BIT_PMU_APB_PD_WTLCP_SYS_FORCE_SHUTDOWN                  BIT(25)
#define BIT_PMU_APB_PD_WTLCP_SYS_AUTO_SHUTDOWN_EN                BIT(24)
#define BIT_PMU_APB_PD_WTLCP_SYS_PWR_ON_DLY(x)                   (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_WTLCP_SYS_PWR_ON_SEQ_DLY(x)               (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_WTLCP_SYS_ISO_ON_DLY(x)                   (((x) & 0xFF))

/* REG_PMU_APB_PD_PUBCP_SYS_CFG */

#define BIT_PMU_APB_PD_PUBCP_DBG_SHUTDOWN_EN                     BIT(26)
#define BIT_PMU_APB_PD_PUBCP_SYS_FORCE_SHUTDOWN                  BIT(25)
#define BIT_PMU_APB_PD_PUBCP_SYS_AUTO_SHUTDOWN_EN                BIT(24)
#define BIT_PMU_APB_PD_PUBCP_SYS_PWR_ON_DLY(x)                   (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_PUBCP_SYS_PWR_ON_SEQ_DLY(x)               (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_PUBCP_SYS_ISO_ON_DLY(x)                   (((x) & 0xFF))

/* REG_PMU_APB_PD_WTLCP_LTE_P3_CFG */

#define BIT_PMU_APB_PD_WTLCP_LTE_P3_FORCE_SHUTDOWN               BIT(25)
#define BIT_PMU_APB_PD_WTLCP_LTE_P3_AUTO_SHUTDOWN_EN             BIT(24)
#define BIT_PMU_APB_PD_WTLCP_LTE_P3_PWR_ON_DLY(x)                (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_WTLCP_LTE_P3_PWR_ON_SEQ_DLY(x)            (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_WTLCP_LTE_P3_ISO_ON_DLY(x)                (((x) & 0xFF))

/* REG_PMU_APB_PD_WTLCP_LTE_P4_CFG */

#define BIT_PMU_APB_PD_WTLCP_LTE_P4_FORCE_SHUTDOWN               BIT(25)
#define BIT_PMU_APB_PD_WTLCP_LTE_P4_AUTO_SHUTDOWN_EN             BIT(24)
#define BIT_PMU_APB_PD_WTLCP_LTE_P4_PWR_ON_DLY(x)                (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_WTLCP_LTE_P4_PWR_ON_SEQ_DLY(x)            (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_WTLCP_LTE_P4_ISO_ON_DLY(x)                (((x) & 0xFF))

/* REG_PMU_APB_PUBCP_FRC_STOP_REQ_FOR_WTL */

#define BIT_PMU_APB_PUBCP_FRC_STOP_REQ_FOR_WTL                   BIT(0)

/* REG_PMU_APB_PD_PUB_SYS_CFG */

#define BIT_PMU_APB_PD_PUB_SYS_FORCE_SHUTDOWN                    BIT(25)
#define BIT_PMU_APB_PD_PUB_SYS_AUTO_SHUTDOWN_EN                  BIT(24)
#define BIT_PMU_APB_PD_PUB_SYS_PWR_ON_DLY(x)                     (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_PUB_SYS_PWR_ON_SEQ_DLY(x)                 (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_PUB_SYS_ISO_ON_DLY(x)                     (((x) & 0xFF))

/* REG_PMU_APB_AP_WAKEUP_POR_CFG */

#define BIT_PMU_APB_AP_WAKEUP_POR_N                              BIT(0)

/* REG_PMU_APB_XTL_WAIT_CNT */

#define BIT_PMU_APB_XTL1_WAIT_CNT(x)                             (((x) & 0xFF) << 8)
#define BIT_PMU_APB_XTL0_WAIT_CNT(x)                             (((x) & 0xFF))

/* REG_PMU_APB_XTLBUF_WAIT_CNT */

#define BIT_PMU_APB_XTLBUF1_WAIT_CNT(x)                          (((x) & 0xFF) << 8)
#define BIT_PMU_APB_XTLBUF0_WAIT_CNT(x)                          (((x) & 0xFF))

/* REG_PMU_APB_PLL_WAIT_CNT1 */

#define BIT_PMU_APB_LTEPLL_WAIT_CNT(x)                           (((x) & 0xFF) << 24)
#define BIT_PMU_APB_TWPLL_WAIT_CNT(x)                            (((x) & 0xFF) << 16)
#define BIT_PMU_APB_DPLL_WAIT_CNT(x)                             (((x) & 0xFF) << 8)
#define BIT_PMU_APB_MPLL_WAIT_CNT(x)                             (((x) & 0xFF))

/* REG_PMU_APB_PLL_WAIT_CNT2 */

#define BIT_PMU_APB_ISPPLL_WAIT_CNT(x)                           (((x) & 0xFF) << 24)
#define BIT_PMU_APB_RPLL_WAIT_CNT(x)                             (((x) & 0xFF) << 16)
#define BIT_PMU_APB_GPLL_WAIT_CNT(x)                             (((x) & 0xFF) << 8)

/* REG_PMU_APB_XTL0_REL_CFG */

#define BIT_PMU_APB_XTL0_FRC_OFF                                 BIT(7)
#define BIT_PMU_APB_XTL0_FRC_ON                                  BIT(6)
#define BIT_PMU_APB_XTL0_SP_SYS_SEL                              BIT(5)
#define BIT_PMU_APB_XTL0_PUB_SYS_SEL                             BIT(4)
#define BIT_PMU_APB_XTL0_WCN_SYS_SEL                             BIT(3)
#define BIT_PMU_APB_XTL0_PUBCP_SEL                               BIT(2)
#define BIT_PMU_APB_XTL0_WTLCP_SEL                               BIT(1)
#define BIT_PMU_APB_XTL0_AP_SEL                                  BIT(0)

/* REG_PMU_APB_XTL1_REL_CFG */

#define BIT_PMU_APB_XTL1_FRC_OFF                                 BIT(7)
#define BIT_PMU_APB_XTL1_FRC_ON                                  BIT(6)
#define BIT_PMU_APB_XTL1_SP_SYS_SEL                              BIT(5)
#define BIT_PMU_APB_XTL1_PUB_SYS_SEL                             BIT(4)
#define BIT_PMU_APB_XTL1_WCN_SYS_SEL                             BIT(3)
#define BIT_PMU_APB_XTL1_PUBCP_SEL                               BIT(2)
#define BIT_PMU_APB_XTL1_WTLCP_SEL                               BIT(1)
#define BIT_PMU_APB_XTL1_AP_SEL                                  BIT(0)

/* REG_PMU_APB_ISPPLL_REL_CFG */

#define BIT_PMU_APB_ISPPLL_REF_SEL                               BIT(8)
#define BIT_PMU_APB_ISPPLL_FRC_OFF                               BIT(7)
#define BIT_PMU_APB_ISPPLL_FRC_ON                                BIT(6)
#define BIT_PMU_APB_ISPPLL_SP_SYS_SEL                            BIT(5)
#define BIT_PMU_APB_ISPPLL_PUB_SYS_SEL                           BIT(4)
#define BIT_PMU_APB_ISPPLL_WCN_SYS_SEL                           BIT(3)
#define BIT_PMU_APB_ISPPLL_PUBCP_SEL                             BIT(2)
#define BIT_PMU_APB_ISPPLL_WTLCP_SEL                             BIT(1)
#define BIT_PMU_APB_ISPPLL_AP_SEL                                BIT(0)

/* REG_PMU_APB_XTLBUF0_REL_CFG */

#define BIT_PMU_APB_XTLBUF0_FRC_OFF                              BIT(7)
#define BIT_PMU_APB_XTLBUF0_FRC_ON                               BIT(6)
#define BIT_PMU_APB_XTLBUF0_SP_SYS_SEL                           BIT(5)
#define BIT_PMU_APB_XTLBUF0_PUB_SYS_SEL                          BIT(4)
#define BIT_PMU_APB_XTLBUF0_WCN_SYS_SEL                          BIT(3)
#define BIT_PMU_APB_XTLBUF0_PUBCP_SEL                            BIT(2)
#define BIT_PMU_APB_XTLBUF0_WTLCP_SEL                            BIT(1)
#define BIT_PMU_APB_XTLBUF0_AP_SEL                               BIT(0)

/* REG_PMU_APB_XTLBUF1_REL_CFG */

#define BIT_PMU_APB_XTLBUF1_FRC_OFF                              BIT(7)
#define BIT_PMU_APB_XTLBUF1_FRC_ON                               BIT(6)
#define BIT_PMU_APB_XTLBUF1_SP_SYS_SEL                           BIT(5)
#define BIT_PMU_APB_XTLBUF1_PUB_SYS_SEL                          BIT(4)
#define BIT_PMU_APB_XTLBUF1_WCN_SYS_SEL                          BIT(3)
#define BIT_PMU_APB_XTLBUF1_PUBCP_SEL                            BIT(2)
#define BIT_PMU_APB_XTLBUF1_WTLCP_SEL                            BIT(1)
#define BIT_PMU_APB_XTLBUF1_AP_SEL                               BIT(0)

/* REG_PMU_APB_MPLL_REL_CFG */

#define BIT_PMU_APB_MPLL_REF_SEL                                 BIT(8)
#define BIT_PMU_APB_MPLL_FRC_OFF                                 BIT(7)
#define BIT_PMU_APB_MPLL_FRC_ON                                  BIT(6)
#define BIT_PMU_APB_MPLL_SP_SYS_SEL                              BIT(5)
#define BIT_PMU_APB_MPLL_PUB_SYS_SEL                             BIT(4)
#define BIT_PMU_APB_MPLL_WCN_SYS_SEL                             BIT(3)
#define BIT_PMU_APB_MPLL_PUBCP_SEL                               BIT(2)
#define BIT_PMU_APB_MPLL_WTLCP_SEL                               BIT(1)
#define BIT_PMU_APB_MPLL_AP_SEL                                  BIT(0)

/* REG_PMU_APB_DPLL_REL_CFG */

#define BIT_PMU_APB_DPLL_REF_SEL                                 BIT(8)
#define BIT_PMU_APB_DPLL_FRC_OFF                                 BIT(7)
#define BIT_PMU_APB_DPLL_FRC_ON                                  BIT(6)
#define BIT_PMU_APB_DPLL_SP_SYS_SEL                              BIT(5)
#define BIT_PMU_APB_DPLL_PUB_SYS_SEL                             BIT(4)
#define BIT_PMU_APB_DPLL_WCN_SYS_SEL                             BIT(3)
#define BIT_PMU_APB_DPLL_PUBCP_SEL                               BIT(2)
#define BIT_PMU_APB_DPLL_WTLCP_SEL                               BIT(1)
#define BIT_PMU_APB_DPLL_AP_SEL                                  BIT(0)

/* REG_PMU_APB_LTEPLL_REL_CFG */

#define BIT_PMU_APB_LTEPLL_REF_SEL(x)                            (((x) & 0x3) << 8)
#define BIT_PMU_APB_LTEPLL_FRC_OFF                               BIT(7)
#define BIT_PMU_APB_LTEPLL_FRC_ON                                BIT(6)
#define BIT_PMU_APB_LTEPLL_SP_SYS_SEL                            BIT(5)
#define BIT_PMU_APB_LTEPLL_PUB_SYS_SEL                           BIT(4)
#define BIT_PMU_APB_LTEPLL_WCN_SYS_SEL                           BIT(3)
#define BIT_PMU_APB_LTEPLL_PUBCP_SEL                             BIT(2)
#define BIT_PMU_APB_LTEPLL_WTLCP_SEL                             BIT(1)
#define BIT_PMU_APB_LTEPLL_AP_SEL                                BIT(0)

/* REG_PMU_APB_TWPLL_REL_CFG */

#define BIT_PMU_APB_TWPLL_REF_SEL(x)                             (((x) & 0x3) << 8)
#define BIT_PMU_APB_TWPLL_FRC_OFF                                BIT(7)
#define BIT_PMU_APB_TWPLL_FRC_ON                                 BIT(6)
#define BIT_PMU_APB_TWPLL_SP_SYS_SEL                             BIT(5)
#define BIT_PMU_APB_TWPLL_PUB_SYS_SEL                            BIT(4)
#define BIT_PMU_APB_TWPLL_WCN_SYS_SEL                            BIT(3)
#define BIT_PMU_APB_TWPLL_PUBCP_SEL                              BIT(2)
#define BIT_PMU_APB_TWPLL_WTLCP_SEL                              BIT(1)
#define BIT_PMU_APB_TWPLL_AP_SEL                                 BIT(0)

/* REG_PMU_APB_GPLL_REL_CFG */

#define BIT_PMU_APB_GPLL_REF_SEL                                 BIT(8)
#define BIT_PMU_APB_GPLL_FRC_OFF                                 BIT(7)
#define BIT_PMU_APB_GPLL_FRC_ON                                  BIT(6)
#define BIT_PMU_APB_GPLL_SP_SYS_SEL                              BIT(5)
#define BIT_PMU_APB_GPLL_PUB_SYS_SEL                             BIT(4)
#define BIT_PMU_APB_GPLL_WCN_SYS_SEL                             BIT(3)
#define BIT_PMU_APB_GPLL_PUBCP_SEL                               BIT(2)
#define BIT_PMU_APB_GPLL_WTLCP_SEL                               BIT(1)
#define BIT_PMU_APB_GPLL_AP_SEL                                  BIT(0)

/* REG_PMU_APB_RPLL_REL_CFG */

#define BIT_PMU_APB_RPLL_REF_SEL(x)                              (((x) & 0x3) << 8)
#define BIT_PMU_APB_RPLL_FRC_OFF                                 BIT(7)
#define BIT_PMU_APB_RPLL_FRC_ON                                  BIT(6)
#define BIT_PMU_APB_RPLL_SP_SYS_SEL                              BIT(5)
#define BIT_PMU_APB_RPLL_PUB_SYS_SEL                             BIT(4)
#define BIT_PMU_APB_RPLL_WCN_SYS_SEL                             BIT(3)
#define BIT_PMU_APB_RPLL_PUBCP_SEL                               BIT(2)
#define BIT_PMU_APB_RPLL_WTLCP_SEL                               BIT(1)
#define BIT_PMU_APB_RPLL_AP_SEL                                  BIT(0)

/* REG_PMU_APB_CP_SOFT_RST */

#define BIT_PMU_APB_WTLCP_TGDSP_SOFT_RST                         BIT(15)
#define BIT_PMU_APB_WTLCP_LDSP_SOFT_RST                          BIT(14)
#define BIT_PMU_APB_WCDMA_AON_SOFT_RST                           BIT(13)
#define BIT_PMU_APB_WTLCP_AON_SOFT_RST                           BIT(12)
#define BIT_PMU_APB_GNSS_WRAP_SOFT_RST                           BIT(11)
#define BIT_PMU_APB_WIFI_WRAP_SOFT_RST                           BIT(10)
#define BIT_PMU_APB_WCN_SYS_SOFT_RST                             BIT(9)
#define BIT_PMU_APB_SP_SYS_SOFT_RST                              BIT(8)
#define BIT_PMU_APB_CA53_SOFT_RST                                BIT(7)
#define BIT_PMU_APB_PUB_SOFT_RST                                 BIT(6)
#define BIT_PMU_APB_AP_SOFT_RST                                  BIT(5)
#define BIT_PMU_APB_GPU_SOFT_RST                                 BIT(4)
#define BIT_PMU_APB_MM_SOFT_RST                                  BIT(3)
#define BIT_PMU_APB_WTLCP_DSP_SYS_SRST                           BIT(2)
#define BIT_PMU_APB_PUBCP_SOFT_RST                               BIT(1)
#define BIT_PMU_APB_WTLCP_SOFT_RST                               BIT(0)

/* REG_PMU_APB_CP_SLP_STATUS_DBG0 */

#define BIT_PMU_APB_PUBCP_DEEP_SLP_DBG(x)                        (((x) & 0xFFFF) << 16)
#define BIT_PMU_APB_WTLCP_DEEP_SLP_DBG(x)                        (((x) & 0xFFFF))

/* REG_PMU_APB_PWR_STATUS0_DBG */

#define BIT_PMU_APB_PD_AP_SYS_STATE(x)                           (((x) & 0x1F) << 25)
#define BIT_PMU_APB_PD_CA53_C3_STATE(x)                          (((x) & 0x1F) << 20)
#define BIT_PMU_APB_PD_CA53_C2_STATE(x)                          (((x) & 0x1F) << 15)
#define BIT_PMU_APB_PD_CA53_C1_STATE(x)                          (((x) & 0x1F) << 10)
#define BIT_PMU_APB_PD_CA53_C0_STATE(x)                          (((x) & 0x1F) << 5)
#define BIT_PMU_APB_PD_CA53_TOP_STATE(x)                         (((x) & 0x1F))

/* REG_PMU_APB_PWR_STATUS1_DBG */

#define BIT_PMU_APB_PD_WTLCP_LTE_P1_STATE(x)                     (((x) & 0x1F) << 25)
#define BIT_PMU_APB_PD_WTLCP_LTE_P2_STATE(x)                     (((x) & 0x1F) << 20)
#define BIT_PMU_APB_PD_WIFI_WRAP_STATE(x)                        (((x) & 0x1F) << 15)
#define BIT_PMU_APB_PD_WTLCP_LDSP_STATE(x)                       (((x) & 0x1F) << 10)
#define BIT_PMU_APB_PD_WTLCP_TGDSP_STATE(x)                      (((x) & 0x1F) << 5)
#define BIT_PMU_APB_PD_WTLCP_HU3GE_A_STATE(x)                    (((x) & 0x1F))

/* REG_PMU_APB_PWR_STATUS2_DBG */

#define BIT_PMU_APB_PD_GNSS_WRAP_STATE(x)                        (((x) & 0x1F) << 25)
#define BIT_PMU_APB_PD_PUB_SYS_STATE(x)                          (((x) & 0x1F) << 20)
#define BIT_PMU_APB_PD_WTLCP_LTE_P4_STATE(x)                     (((x) & 0x1F) << 15)
#define BIT_PMU_APB_PD_WTLCP_LTE_P3_STATE(x)                     (((x) & 0x1F) << 10)
#define BIT_PMU_APB_PD_PUBCP_SYS_STATE(x)                        (((x) & 0x1F) << 5)
#define BIT_PMU_APB_PD_WTLCP_SYS_STATE(x)                        (((x) & 0x1F))

/* REG_PMU_APB_PUB_SYS_AUTO_LIGHT_SLEEP_ENABLE */

#define BIT_PMU_APB_PUB_SYS_AUTO_LIGHT_SLEEP_ENABLE(x)           (((x) & 0xFFFFFFFF))

/* REG_PMU_APB_SLEEP_CTRL */

#define BIT_PMU_APB_PUB_SYS_FORCE_SYSTEM_SLEEP                   BIT(31)
#define BIT_PMU_APB_PUB_SYS_FORCE_LIGHT_SLEEP                    BIT(30)
#define BIT_PMU_APB_PUB_SYS_FORCE_DEEP_SLEEP                     BIT(29)
#define BIT_PMU_APB_PUB_SYS_AUTO_DEEP_SLEEP_ENABLE               BIT(28)
#define BIT_PMU_APB_AON_DMA_FORCE_LIGHT_SLEEP                    BIT(27)
#define BIT_PMU_APB_PUBCP_FORCE_LIGHT_SLEEP                      BIT(26)
#define BIT_PMU_APB_WTLCP_FORCE_LIGHT_SLEEP                      BIT(25)
#define BIT_PMU_APB_WCN_SYS_FORCE_LIGHT_SLEEP                    BIT(24)
#define BIT_PMU_APB_AP_FORCE_LIGHT_SLEEP                         BIT(23)
#define BIT_PMU_APB_AON_SYS_FORCE_LIGHT_SLEEP                    BIT(22)
#define BIT_PMU_APB_SP_SYS_FORCE_DEEP_SLEEP                      BIT(21)
#define BIT_PMU_APB_WCN_SYS_FORCE_DEEP_SLEEP                     BIT(19)
#define BIT_PMU_APB_PUBCP_FORCE_DEEP_SLEEP                       BIT(18)
#define BIT_PMU_APB_WTLCP_FORCE_DEEP_SLEEP                       BIT(17)
#define BIT_PMU_APB_AP_FORCE_DEEP_SLEEP                          BIT(16)
#define BIT_PMU_APB_WCN_SYS_FORCE_SYSTEM_SLEEP                   BIT(15)
#define BIT_PMU_APB_GPU_TOP_FORCE_SYSTEM_SLEEP                   BIT(14)
#define BIT_PMU_APB_MM_TOP_FORCE_SYSTEM_SLEEP                    BIT(13)
#define BIT_PMU_APB_PUBCP_FORCE_SYSTEM_SLEEP                     BIT(12)
#define BIT_PMU_APB_WTLCP_FORCE_SYSTEM_SLEEP                     BIT(11)
#define BIT_PMU_APB_AP_FORCE_SYSTEM_SLEEP                        BIT(10)
#define BIT_PMU_APB_SP_SYS_DEEP_SLEEP                            BIT(6)
#define BIT_PMU_APB_WCN_SYS_DEEP_SLEEP                           BIT(5)
#define BIT_PMU_APB_PUB_SYS_DEEP_SLEEP                           BIT(4)
#define BIT_PMU_APB_PUB_SYS_LIGHT_SLEEP                          BIT(3)
#define BIT_PMU_APB_PUBCP_DEEP_SLEEP                             BIT(2)
#define BIT_PMU_APB_WTLCP_DEEP_SLEEP                             BIT(1)
#define BIT_PMU_APB_AP_DEEP_SLEEP                                BIT(0)

/* REG_PMU_APB_DDR_SLEEP_CTRL */

#define BIT_PMU_APB_DDR_SLEEP_DISABLE_ACK                        BIT(19)
#define BIT_PMU_APB_DDR_SLEEP_DISABLE_ACK_BYP                    BIT(18)
#define BIT_PMU_APB_DDR_SLEEP_DISABLE                            BIT(17)
#define BIT_PMU_APB_BUSY_TRANSFER_HWDATA_SEL                     BIT(16)
#define BIT_PMU_APB_DDR_PUBL_APB_SOFT_RST                        BIT(12)
#define BIT_PMU_APB_DDR_UMCTL_APB_SOFT_RST                       BIT(11)
#define BIT_PMU_APB_DDR_PUBL_SOFT_RST                            BIT(10)
#define BIT_PMU_APB_DDR_PHY_SOFT_RST                             BIT(8)
#define BIT_PMU_APB_DDR_PHY_AUTO_GATE_EN                         BIT(6)
#define BIT_PMU_APB_DDR_PUBL_AUTO_GATE_EN                        BIT(5)
#define BIT_PMU_APB_DDR_UMCTL_AUTO_GATE_EN                       BIT(4)
#define BIT_PMU_APB_DDR_PHY_EB                                   BIT(2)
#define BIT_PMU_APB_DDR_UMCTL_EB                                 BIT(1)
#define BIT_PMU_APB_DDR_PUBL_EB                                  BIT(0)

/* REG_PMU_APB_SLEEP_STATUS */

#define BIT_PMU_APB_SP_SYS_SLP_STATUS(x)                         (((x) & 0xF) << 20)
#define BIT_PMU_APB_WCN_SYS_SLP_STATUS(x)                        (((x) & 0xF) << 12)
#define BIT_PMU_APB_PUBCP_SLP_STATUS(x)                          (((x) & 0xF) << 8)
#define BIT_PMU_APB_WTLCP_SLP_STATUS(x)                          (((x) & 0xF) << 4)
#define BIT_PMU_APB_AP_SLP_STATUS(x)                             (((x) & 0xF))

/* REG_PMU_APB_PUB_SYS_SLEEP_BYPASS_CFG */

#define BIT_PMU_APB_PUB_SYS_SELF_REFRESH_FLAG_BYPASS             BIT(2)
#define BIT_PMU_APB_PUB_SYS_PWR_PD_ACK_BYPASS                    BIT(1)
#define BIT_PMU_APB_PUB_SYS_DEEP_SLEEP_LOCK_ACK_BYPASS           BIT(0)

/* REG_PMU_APB_PUB_SYS_DEEP_SLEEP_POLL0 */

#define BIT_PMU_APB_WCN_SYS_PUB_SYS_DEEP_SLEEP_POLL(x)           (((x) & 0xFF))

/* REG_PMU_APB_PUB_SYS_DEEP_SLEEP_POLL1 */

#define BIT_PMU_APB_AON_PUB_SYS_DEEP_SLEEP_POLL(x)               (((x) & 0xFF) << 24)
#define BIT_PMU_APB_PUBCP_PUB_SYS_DEEP_SLEEP_POLL(x)             (((x) & 0xFF) << 16)
#define BIT_PMU_APB_AP_PUB_SYS_DEEP_SLEEP_POLL(x)                (((x) & 0xFF) << 8)
#define BIT_PMU_APB_WTLCP_PUB_SYS_DEEP_SLEEP_POLL(x)             (((x) & 0xFF))

/* REG_PMU_APB_CA53_TOP_CFG */

#define BIT_PMU_APB_CA53_L2RSTDISABLE                            BIT(0)

/* REG_PMU_APB_CA53_C0_CFG */

#define BIT_PMU_APB_CA53_VINITHI_C0                              BIT(0)

/* REG_PMU_APB_CA53_C1_CFG */

#define BIT_PMU_APB_CA53_VINITHI_C1                              BIT(0)

/* REG_PMU_APB_CA53_C2_CFG */

#define BIT_PMU_APB_CA53_VINITHI_C2                              BIT(0)

/* REG_PMU_APB_CA53_C3_CFG */

#define BIT_PMU_APB_CA53_VINITHI_C3                              BIT(0)

/* REG_PMU_APB_DDR_CHN_SLEEP_CTRL0 */

#define BIT_PMU_APB_DDR_CTRL_AXI_LP_EN                           BIT(31)
#define BIT_PMU_APB_DDR_CTRL_CGM_SEL                             BIT(30)
#define BIT_PMU_APB_DDR_CHN9_AXI_LP_EN                           BIT(25)
#define BIT_PMU_APB_DDR_CHN8_AXI_LP_EN                           BIT(24)
#define BIT_PMU_APB_DDR_CHN7_AXI_LP_EN                           BIT(23)
#define BIT_PMU_APB_DDR_CHN6_AXI_LP_EN                           BIT(22)
#define BIT_PMU_APB_DDR_CHN5_AXI_LP_EN                           BIT(21)
#define BIT_PMU_APB_DDR_CHN4_AXI_LP_EN                           BIT(20)
#define BIT_PMU_APB_DDR_CHN3_AXI_LP_EN                           BIT(19)
#define BIT_PMU_APB_DDR_CHN2_AXI_LP_EN                           BIT(18)
#define BIT_PMU_APB_DDR_CHN1_AXI_LP_EN                           BIT(17)
#define BIT_PMU_APB_DDR_CHN0_AXI_LP_EN                           BIT(16)
#define BIT_PMU_APB_DDR_CHN9_CGM_SEL                             BIT(9)
#define BIT_PMU_APB_DDR_CHN8_CGM_SEL                             BIT(8)
#define BIT_PMU_APB_DDR_CHN7_CGM_SEL                             BIT(7)
#define BIT_PMU_APB_DDR_CHN6_CGM_SEL                             BIT(6)
#define BIT_PMU_APB_DDR_CHN5_CGM_SEL                             BIT(5)
#define BIT_PMU_APB_DDR_CHN4_CGM_SEL                             BIT(4)
#define BIT_PMU_APB_DDR_CHN3_CGM_SEL                             BIT(3)
#define BIT_PMU_APB_DDR_CHN2_CGM_SEL                             BIT(2)
#define BIT_PMU_APB_DDR_CHN1_CGM_SEL                             BIT(1)
#define BIT_PMU_APB_DDR_CHN0_CGM_SEL                             BIT(0)

/* REG_PMU_APB_DDR_CHN_SLEEP_CTRL1 */

#define BIT_PMU_APB_DDR_CHN9_AXI_STOP_SEL                        BIT(9)
#define BIT_PMU_APB_DDR_CHN8_AXI_STOP_SEL                        BIT(8)
#define BIT_PMU_APB_DDR_CHN7_AXI_STOP_SEL                        BIT(7)
#define BIT_PMU_APB_DDR_CHN6_AXI_STOP_SEL                        BIT(6)
#define BIT_PMU_APB_DDR_CHN5_AXI_STOP_SEL                        BIT(5)
#define BIT_PMU_APB_DDR_CHN4_AXI_STOP_SEL                        BIT(4)
#define BIT_PMU_APB_DDR_CHN3_AXI_STOP_SEL                        BIT(3)
#define BIT_PMU_APB_DDR_CHN2_AXI_STOP_SEL                        BIT(2)
#define BIT_PMU_APB_DDR_CHN1_AXI_STOP_SEL                        BIT(1)
#define BIT_PMU_APB_DDR_CHN0_AXI_STOP_SEL                        BIT(0)

/* REG_PMU_APB_PD_WCN_SYS_CFG */

#define BIT_PMU_APB_PD_WCN_SYS_DBG_SHUTDOWN_EN                   BIT(28)
#define BIT_PMU_APB_PD_WCN_SYS_PD_SEL                            BIT(27)
#define BIT_PMU_APB_PD_WCN_SYS_FORCE_SHUTDOWN                    BIT(25)
#define BIT_PMU_APB_PD_WCN_SYS_AUTO_SHUTDOWN_EN                  BIT(24)
#define BIT_PMU_APB_PD_WCN_SYS_PWR_ON_DLY(x)                     (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_WCN_SYS_PWR_ON_SEQ_DLY(x)                 (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_WCN_SYS_ISO_ON_DLY(x)                     (((x) & 0xFF))

/* REG_PMU_APB_PD_WIFI_WRAP_CFG */

#define BIT_PMU_APB_PD_WIFI_WRAP_DBG_SHUTDOWN_EN                 BIT(28)
#define BIT_PMU_APB_PD_WIFI_WRAP_PD_SEL                          BIT(27)
#define BIT_PMU_APB_PD_WIFI_WRAP_FORCE_SHUTDOWN                  BIT(25)
#define BIT_PMU_APB_PD_WIFI_WRAP_AUTO_SHUTDOWN_EN                BIT(24)
#define BIT_PMU_APB_PD_WIFI_WRAP_PWR_ON_DLY(x)                   (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_WIFI_WRAP_PWR_ON_SEQ_DLY(x)               (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_WIFI_WRAP_ISO_ON_DLY(x)                   (((x) & 0xFF))

/* REG_PMU_APB_PD_GNSS_WRAP_CFG */

#define BIT_PMU_APB_PD_GNSS_WRAP_DBG_SHUTDOWN_EN                 BIT(28)
#define BIT_PMU_APB_PD_GNSS_WRAP_PD_SEL                          BIT(27)
#define BIT_PMU_APB_PD_GNSS_WRAP_FORCE_SHUTDOWN                  BIT(25)
#define BIT_PMU_APB_PD_GNSS_WRAP_AUTO_SHUTDOWN_EN                BIT(24)
#define BIT_PMU_APB_PD_GNSS_WRAP_PWR_ON_DLY(x)                   (((x) & 0xFF) << 16)
#define BIT_PMU_APB_PD_GNSS_WRAP_PWR_ON_SEQ_DLY(x)               (((x) & 0xFF) << 8)
#define BIT_PMU_APB_PD_GNSS_WRAP_ISO_ON_DLY(x)                   (((x) & 0xFF))

/* REG_PMU_APB_PWR_STATUS3_DBG */

#define BIT_PMU_APB_PD_MM_TOP_STATE(x)                           (((x) & 0x1F) << 10)
#define BIT_PMU_APB_PD_GPU_TOP_STATE(x)                          (((x) & 0x1F) << 5)
#define BIT_PMU_APB_PD_WCN_SYS_STATE(x)                          (((x) & 0x1F))

/* REG_PMU_APB_DDR_OP_MODE_CFG */

#define BIT_PMU_APB_DDR_PUBL_RET_EN                              BIT(27)
#define BIT_PMU_APB_DDR_PHY_ISO_RST_EN                           BIT(26)
#define BIT_PMU_APB_DDR_UMCTL_RET_EN                             BIT(25)
#define BIT_PMU_APB_DDR_PHY_AUTO_RET_EN                          BIT(24)

/* REG_PMU_APB_DDR_PHY_RET_CFG */

#define BIT_PMU_APB_DDR_UMCTL_SOFT_RST                           BIT(16)
#define BIT_PMU_APB_DDR_PHY_CKE_RET_EN                           BIT(0)

/* REG_PMU_APB_CLK26M_SEL_CFG */

#define BIT_PMU_APB_AON_RC_4M_SEL                                BIT(8)
#define BIT_PMU_APB_GGE_26M_SEL                                  BIT(6)
#define BIT_PMU_APB_PUB_26M_SEL                                  BIT(5)
#define BIT_PMU_APB_AON_26M_SEL                                  BIT(4)
#define BIT_PMU_APB_PUBCP_26M_SEL                                BIT(2)
#define BIT_PMU_APB_WTLCP_26M_SEL                                BIT(1)
#define BIT_PMU_APB_AP_26M_SEL                                   BIT(0)

/* REG_PMU_APB_BISR_DONE_STATUS */

#define BIT_PMU_APB_PD_GNSS_WRAP_BISR_DONE                       BIT(23)
#define BIT_PMU_APB_PD_WIFI_WRAP_BISR_DONE                       BIT(22)
#define BIT_PMU_APB_PD_WCN_SYS_BISR_DONE                         BIT(21)
#define BIT_PMU_APB_PD_WTLCP_LTE_P4_BISR_DONE                    BIT(20)
#define BIT_PMU_APB_PD_WTLCP_LTE_P3_BISR_DONE                    BIT(19)
#define BIT_PMU_APB_PD_AON_MEM_BISR_DONE                         BIT(18)
#define BIT_PMU_APB_PD_PUBCP_SYS_BISR_DONE                       BIT(17)
#define BIT_PMU_APB_PD_WTLCP_SYS_BISR_DONE                       BIT(16)
#define BIT_PMU_APB_PD_WTLCP_HU3GE_A_BISR_DONE                   BIT(14)
#define BIT_PMU_APB_PD_WTLCP_TGDSP_BISR_DONE                     BIT(13)
#define BIT_PMU_APB_PD_WTLCP_LDSP_BISR_DONE                      BIT(12)
#define BIT_PMU_APB_PD_WTLCP_LTE_P2_BISR_DONE                    BIT(10)
#define BIT_PMU_APB_PD_WTLCP_LTE_P1_BISR_DONE                    BIT(9)
#define BIT_PMU_APB_PD_MM_TOP_BISR_DONE                          BIT(7)
#define BIT_PMU_APB_PD_GPU_TOP_BISR_DONE                         BIT(6)
#define BIT_PMU_APB_PD_AP_SYS_BISR_DONE                          BIT(5)
#define BIT_PMU_APB_PD_CA53_TOP_BISR_DONE                        BIT(4)
#define BIT_PMU_APB_PD_CA53_C3_BISR_DONE                         BIT(3)
#define BIT_PMU_APB_PD_CA53_C2_BISR_DONE                         BIT(2)
#define BIT_PMU_APB_PD_CA53_C1_BISR_DONE                         BIT(1)
#define BIT_PMU_APB_PD_CA53_C0_BISR_DONE                         BIT(0)

/* REG_PMU_APB_BISR_BUSY_STATUS */

#define BIT_PMU_APB_PD_GNSS_WRAP_BISR_BUSY                       BIT(23)
#define BIT_PMU_APB_PD_WIFI_WRAP_BISR_BUSY                       BIT(22)
#define BIT_PMU_APB_PD_WCN_SYS_BISR_BUSY                         BIT(21)
#define BIT_PMU_APB_PD_WTLCP_LTE_P4_BISR_BUSY                    BIT(20)
#define BIT_PMU_APB_PD_WTLCP_LTE_P3_BISR_BUSY                    BIT(19)
#define BIT_PMU_APB_PD_AON_MEM_BISR_BUSY                         BIT(18)
#define BIT_PMU_APB_PD_PUBCP_SYS_BISR_BUSY                       BIT(17)
#define BIT_PMU_APB_PD_WTLCP_SYS_BISR_BUSY                       BIT(16)
#define BIT_PMU_APB_PD_WTLCP_HU3GE_B_BISR_BUSY                   BIT(15)
#define BIT_PMU_APB_PD_WTLCP_HU3GE_A_BISR_BUSY                   BIT(14)
#define BIT_PMU_APB_PD_WTLCP_TGDSP_BISR_BUSY                     BIT(13)
#define BIT_PMU_APB_PD_WTLCP_LDSP_BISR_BUSY                      BIT(12)
#define BIT_PMU_APB_PD_WTLCP_LTE_P2_BISR_BUSY                    BIT(10)
#define BIT_PMU_APB_PD_WTLCP_LTE_P1_BISR_BUSY                    BIT(9)
#define BIT_PMU_APB_PD_WTLCP_TD_BISR_BUSY                        BIT(8)
#define BIT_PMU_APB_PD_MM_TOP_BISR_BUSY                          BIT(7)
#define BIT_PMU_APB_PD_GPU_TOP_BISR_BUSY                         BIT(6)
#define BIT_PMU_APB_PD_AP_SYS_BISR_BUSY                          BIT(5)
#define BIT_PMU_APB_PD_CA53_TOP_BISR_BUSY                        BIT(4)
#define BIT_PMU_APB_PD_CA53_C3_BISR_BUSY                         BIT(3)
#define BIT_PMU_APB_PD_CA53_C2_BISR_BUSY                         BIT(2)
#define BIT_PMU_APB_PD_CA53_C1_BISR_BUSY                         BIT(1)
#define BIT_PMU_APB_PD_CA53_C0_BISR_BUSY                         BIT(0)

/* REG_PMU_APB_BISR_BYP_CFG */

#define BIT_PMU_APB_PD_GNSS_WRAP_BISR_FORCE_BYP                  BIT(23)
#define BIT_PMU_APB_PD_WIFI_WRAP_BISR_FORCE_BYP                  BIT(22)
#define BIT_PMU_APB_PD_WCN_SYS_BISR_FORCE_BYP                    BIT(21)
#define BIT_PMU_APB_PD_WTLCP_LTE_P4_BISR_FORCE_BYP               BIT(20)
#define BIT_PMU_APB_PD_WTLCP_LTE_P3_BISR_FORCE_BYP               BIT(19)
#define BIT_PMU_APB_PD_AON_MEM_BISR_FORCE_BYP                    BIT(18)
#define BIT_PMU_APB_PD_PUBCP_SYS_BISR_FORCE_BYP                  BIT(17)
#define BIT_PMU_APB_PD_WTLCP_SYS_BISR_FORCE_BYP                  BIT(16)
#define BIT_PMU_APB_PD_WTLCP_HU3GE_B_BISR_FORCE_BYP              BIT(15)
#define BIT_PMU_APB_PD_WTLCP_HU3GE_A_BISR_FORCE_BYP              BIT(14)
#define BIT_PMU_APB_PD_WTLCP_TGDSP_BISR_FORCE_BYP                BIT(13)
#define BIT_PMU_APB_PD_WTLCP_LDSP_BISR_FORCE_BYP                 BIT(12)
#define BIT_PMU_APB_PD_WTLCP_LTE_P2_BISR_FORCE_BYP               BIT(10)
#define BIT_PMU_APB_PD_WTLCP_LTE_P1_BISR_FORCE_BYP               BIT(9)
#define BIT_PMU_APB_PD_WTLCP_TD_BISR_FORCE_BYP                   BIT(8)
#define BIT_PMU_APB_PD_MM_TOP_BISR_FORCE_BYP                     BIT(7)
#define BIT_PMU_APB_PD_GPU_TOP_BISR_FORCE_BYP                    BIT(6)
#define BIT_PMU_APB_PD_AP_SYS_BISR_FORCE_BYP                     BIT(5)
#define BIT_PMU_APB_PD_CA53_TOP_BISR_FORCE_BYP                   BIT(4)
#define BIT_PMU_APB_PD_CA53_C3_BISR_FORCE_BYP                    BIT(3)
#define BIT_PMU_APB_PD_CA53_C2_BISR_FORCE_BYP                    BIT(2)
#define BIT_PMU_APB_PD_CA53_C1_BISR_FORCE_BYP                    BIT(1)
#define BIT_PMU_APB_PD_CA53_C0_BISR_FORCE_BYP                    BIT(0)

/* REG_PMU_APB_BISR_EN_CFG */

#define BIT_PMU_APB_PD_GNSS_WRAP_BISR_FORCE_EN                   BIT(23)
#define BIT_PMU_APB_PD_WIFI_WRAP_BISR_FORCE_EN                   BIT(22)
#define BIT_PMU_APB_PD_WCN_SYS_BISR_FORCE_EN                     BIT(21)
#define BIT_PMU_APB_PD_WTLCP_LTE_P4_BISR_FORCE_EN                BIT(20)
#define BIT_PMU_APB_PD_WTLCP_LTE_P3_BISR_FORCE_EN                BIT(19)
#define BIT_PMU_APB_PD_AON_MEM_BISR_FORCE_EN                     BIT(18)
#define BIT_PMU_APB_PD_PUBCP_SYS_BISR_FORCE_EN                   BIT(17)
#define BIT_PMU_APB_PD_WTLCP_SYS_BISR_FORCE_EN                   BIT(16)
#define BIT_PMU_APB_PD_WTLCP_HU3GE_B_BISR_FORCE_EN               BIT(15)
#define BIT_PMU_APB_PD_WTLCP_HU3GE_A_BISR_FORCE_EN               BIT(14)
#define BIT_PMU_APB_PD_WTLCP_TGDSP_BISR_FORCE_EN                 BIT(13)
#define BIT_PMU_APB_PD_WTLCP_LDSP_BISR_FORCE_EN                  BIT(12)
#define BIT_PMU_APB_PD_WTLCP_LTE_P2_BISR_FORCE_EN                BIT(10)
#define BIT_PMU_APB_PD_WTLCP_LTE_P1_BISR_FORCE_EN                BIT(9)
#define BIT_PMU_APB_PD_WTLCP_TD_BISR_FORCE_EN                    BIT(8)
#define BIT_PMU_APB_PD_MM_TOP_BISR_FORCE_EN                      BIT(7)
#define BIT_PMU_APB_PD_GPU_TOP_BISR_FORCE_EN                     BIT(6)
#define BIT_PMU_APB_PD_AP_SYS_BISR_FORCE_EN                      BIT(5)
#define BIT_PMU_APB_PD_CA53_TOP_BISR_FORCE_EN                    BIT(4)
#define BIT_PMU_APB_PD_CA53_C3_BISR_FORCE_EN                     BIT(3)
#define BIT_PMU_APB_PD_CA53_C2_BISR_FORCE_EN                     BIT(2)
#define BIT_PMU_APB_PD_CA53_C1_BISR_FORCE_EN                     BIT(1)
#define BIT_PMU_APB_PD_CA53_C0_BISR_FORCE_EN                     BIT(0)

/* REG_PMU_APB_CGM_AUTO_GATE_SEL_CFG0 */

#define BIT_PMU_APB_CGM_AUTO_GATE_SEL_CFG0(x)                    (((x) & 0xFFFFFFFF))

/* REG_PMU_APB_CGM_AUTO_GATE_SEL_CFG1 */

#define BIT_PMU_APB_CGM_AUTO_GATE_SEL_CFG1(x)                    (((x) & 0xFFFFFFFF))

/* REG_PMU_APB_CGM_AUTO_GATE_SEL_CFG2 */

#define BIT_PMU_APB_CGM_AUTO_GATE_SEL_CFG2(x)                    (((x) & 0xFFFFFFFF))

/* REG_PMU_APB_CGM_AUTO_GATE_SEL_CFG3 */

#define BIT_PMU_APB_CGM_AUTO_GATE_SEL_CFG3(x)                    (((x) & 0xFFFFFFFF))

/* REG_PMU_APB_CGM_FORCE_EN_CFG0 */

#define BIT_PMU_APB_CGM_FORCE_EN_CFG0(x)                         (((x) & 0xFFFFFFFF))

/* REG_PMU_APB_CGM_FORCE_EN_CFG1 */

#define BIT_PMU_APB_CGM_FORCE_EN_CFG1(x)                         (((x) & 0xFFFFFFFF))

/* REG_PMU_APB_CGM_FORCE_EN_CFG2 */

#define BIT_PMU_APB_CGM_FORCE_EN_CFG2(x)                         (((x) & 0xFFFFFFFF))

/* REG_PMU_APB_CGM_FORCE_EN_CFG3 */

#define BIT_PMU_APB_CGM_FORCE_EN_CFG3(x)                         (((x) & 0xFFFFFFFF))

/* REG_PMU_APB_SLEEP_XTLON_CTRL */

#define BIT_PMU_APB_SP_SYS_SLEEP_XTL_ON                          BIT(5)
#define BIT_PMU_APB_WCN_SYS_SLEEP_XTL_ON                         BIT(3)
#define BIT_PMU_APB_PUBCP_SLEEP_XTL_ON                           BIT(2)
#define BIT_PMU_APB_WTLCP_SLEEP_XTL_ON                           BIT(1)
#define BIT_PMU_APB_AP_SLEEP_XTL_ON                              BIT(0)

/* REG_PMU_APB_MEM_SLP_CFG */

#define BIT_PMU_APB_MEM_SLP_CFG(x)                               (((x) & 0xFFFFFFFF))

/* REG_PMU_APB_MEM_SD_CFG */

#define BIT_PMU_APB_MEM_SD_CFG(x)                                (((x) & 0xFFFFFFFF))

/* REG_PMU_APB_CA53_CORE_WAKEUP_EN */

#define BIT_PMU_APB_CA53_C3_WAKEUP_EN                            BIT(11)
#define BIT_PMU_APB_CA53_C2_WAKEUP_EN                            BIT(10)
#define BIT_PMU_APB_CA53_C1_WAKEUP_EN                            BIT(9)
#define BIT_PMU_APB_CA53_C0_WAKEUP_EN                            BIT(8)

/* REG_PMU_APB_SP_SYS_HOLD_CGM_EN */

#define BIT_PMU_APB_PD_CA53_TOP_CMG_HOLD_EN                      BIT(4)
#define BIT_PMU_APB_PD_CA53_C3_CMG_HOLD_EN                       BIT(3)
#define BIT_PMU_APB_PD_CA53_C2_CMG_HOLD_EN                       BIT(2)
#define BIT_PMU_APB_PD_CA53_C1_CMG_HOLD_EN                       BIT(1)
#define BIT_PMU_APB_PD_CA53_C0_CMG_HOLD_EN                       BIT(0)

/* REG_PMU_APB_PWR_CNT_WAIT_CFG0 */

#define BIT_PMU_APB_PUBCP_PWR_WAIT_CNT(x)                        (((x) & 0xFF) << 16)
#define BIT_PMU_APB_WTLCP_PWR_WAIT_CNT(x)                        (((x) & 0xFF) << 8)
#define BIT_PMU_APB_AP_PWR_WAIT_CNT(x)                           (((x) & 0xFF))

/* REG_PMU_APB_PWR_CNT_WAIT_CFG1 */

#define BIT_PMU_APB_SP_SYS_PWR_WAIT_CNT(x)                       (((x) & 0xFF) << 8)
#define BIT_PMU_APB_WCN_SYS_PWR_WAIT_CNT(x)                      (((x) & 0xFF))

/* REG_PMU_APB_RC0_REL_CFG */

#define BIT_PMU_APB_RC0_SP_SYS_SEL                               BIT(5)
#define BIT_PMU_APB_RC0_PUBCP_SEL                                BIT(2)
#define BIT_PMU_APB_RC0_WTLCP_SEL                                BIT(1)
#define BIT_PMU_APB_RC0_AP_SEL                                   BIT(0)

/* REG_PMU_APB_RC1_REL_CFG */

#define BIT_PMU_APB_RC1_SP_SYS_SEL                               BIT(5)
#define BIT_PMU_APB_RC1_PUBCP_SEL                                BIT(2)
#define BIT_PMU_APB_RC1_WTLCP_SEL                                BIT(1)
#define BIT_PMU_APB_RC1_AP_SEL                                   BIT(0)

/* REG_PMU_APB_RC_CNT_WAIT_CFG */

#define BIT_PMU_APB_RC1_WAIT_CNT(x)                              (((x) & 0xFF) << 8)
#define BIT_PMU_APB_RC0_WAIT_CNT(x)                              (((x) & 0xFF))

/* REG_PMU_APB_MEM_AUTO_SLP_CFG */

#define BIT_PMU_APB_MEM_AUTO_SLP_EN(x)                           (((x) & 0xFFFFFFFF))

/* REG_PMU_APB_MEM_AUTO_SD_CFG */

#define BIT_PMU_APB_MEM_AUTO_SD_EN(x)                            (((x) & 0xFFFFFFFF))

/* REG_PMU_APB_WAKEUP_LOCK_EN */

#define BIT_PMU_APB_PD_GNSS_WRAP_WAKEUP_LOCK_EN                  BIT(27)
#define BIT_PMU_APB_PD_WIFI_WRAP_WAKEUP_LOCK_EN                  BIT(26)
#define BIT_PMU_APB_PD_WCN_SYS_WAKEUP_LOCK_EN                    BIT(25)
#define BIT_PMU_APB_PUBCP_SYS_WAKEUP_LOCK_EN                     BIT(24)
#define BIT_PMU_APB_WTLCP_SYS_WAKEUP_LOCK_EN                     BIT(23)
#define BIT_PMU_APB_AP_SYS_WAKEUP_LOCK_EN                        BIT(22)
#define BIT_PMU_APB_PD_PUB_SYS_WAKEUP_LOCK_EN                    BIT(21)
#define BIT_PMU_APB_PD_WTLCP_LTE_P4_WAKEUP_LOCK_EN               BIT(19)
#define BIT_PMU_APB_PD_WTLCP_LTE_P3_WAKEUP_LOCK_EN               BIT(18)
#define BIT_PMU_APB_PD_PUBCP_SYS_WAKEUP_LOCK_EN                  BIT(17)
#define BIT_PMU_APB_PD_WTLCP_SYS_WAKEUP_LOCK_EN                  BIT(16)
#define BIT_PMU_APB_PD_WTLCP_HU3GE_B_WAKEUP_LOCK_EN              BIT(15)
#define BIT_PMU_APB_PD_WTLCP_HU3GE_A_WAKEUP_LOCK_EN              BIT(14)
#define BIT_PMU_APB_PD_WTLCP_TGDSP_WAKEUP_LOCK_EN                BIT(13)
#define BIT_PMU_APB_PD_WTLCP_LDSP_WAKEUP_LOCK_EN                 BIT(12)
#define BIT_PMU_APB_PD_WTLCP_LTE_P2_WAKEUP_LOCK_EN               BIT(10)
#define BIT_PMU_APB_PD_WTLCP_LTE_P1_WAKEUP_LOCK_EN               BIT(9)
#define BIT_PMU_APB_PD_WTLCP_TD_WAKEUP_LOCK_EN                   BIT(8)
#define BIT_PMU_APB_PD_MM_TOP_WAKEUP_LOCK_EN                     BIT(7)
#define BIT_PMU_APB_PD_GPU_TOP_WAKEUP_LOCK_EN                    BIT(6)
#define BIT_PMU_APB_PD_AP_SYS_WAKEUP_LOCK_EN                     BIT(5)
#define BIT_PMU_APB_PD_CA53_TOP_WAKEUP_LOCK_EN                   BIT(4)
#define BIT_PMU_APB_PD_CA53_C3_WAKEUP_LOCK_EN                    BIT(3)
#define BIT_PMU_APB_PD_CA53_C2_WAKEUP_LOCK_EN                    BIT(2)
#define BIT_PMU_APB_PD_CA53_C1_WAKEUP_LOCK_EN                    BIT(1)
#define BIT_PMU_APB_PD_CA53_C0_WAKEUP_LOCK_EN                    BIT(0)

/* REG_PMU_APB_WCN_SYS_CORE_INT_DISABLE */

#define BIT_PMU_APB_WCN_SYS_CORE_INT_DISABLE                     BIT(0)

/* REG_PMU_APB_GNSS_WRAP_CORE_INT_DISABLE */

#define BIT_PMU_APB_GNSS_WRAP_TGDSP_CORE_INT_DISABLE             BIT(0)

/* REG_PMU_APB_WTLCP_TGDSP_CORE_INT_DISABLE */

#define BIT_PMU_APB_WTLCP_TGDSP_CORE_INT_DISABLE                 BIT(0)

/* REG_PMU_APB_WTLCP_LDSP_CORE_INT_DISABLE */

#define BIT_PMU_APB_WTLCP_LDSP_CORE_INT_DISABLE                  BIT(0)

/* REG_PMU_APB_PUBCP_CORE_INT_DISABLE */

#define BIT_PMU_APB_PUBCP_CORE_INT_DISABLE                       BIT(0)

/* REG_PMU_APB_CA53_C0_CORE_INT_DISABLE */

#define BIT_PMU_APB_CA53_C0_CORE_INT_DISABLE                     BIT(0)

/* REG_PMU_APB_CA53_C1_CORE_INT_DISABLE */

#define BIT_PMU_APB_CA53_C1_CORE_INT_DISABLE                     BIT(0)

/* REG_PMU_APB_CA53_C2_CORE_INT_DISABLE */

#define BIT_PMU_APB_CA53_C2_CORE_INT_DISABLE                     BIT(0)

/* REG_PMU_APB_CA53_C3_CORE_INT_DISABLE */

#define BIT_PMU_APB_CA53_C3_CORE_INT_DISABLE                     BIT(0)

/* REG_PMU_APB_WTLCP_TGDSP_DSLP_ENA */

#define BIT_PMU_APB_WTLCP_TGDSP_DSLP_ENA                         BIT(0)

/* REG_PMU_APB_WTLCP_LDSP_DSLP_ENA */

#define BIT_PMU_APB_WTLCP_LDSP_DSLP_ENA                          BIT(0)

/* REG_PMU_APB_AP_DSLP_ENA */

#define BIT_PMU_APB_AP_DSLP_ENA                                  BIT(0)

/* REG_PMU_APB_PUBCP_DSLP_ENA */

#define BIT_PMU_APB_PUBCP_DSLP_ENA                               BIT(0)

/* REG_PMU_APB_WTLCP_DSLP_ENA */

#define BIT_PMU_APB_WTLCP_DSLP_ENA                               BIT(0)

/* REG_PMU_APB_CA53_TOP_DSLP_ENA */

#define BIT_PMU_APB_CA53_TOP_DSLP_ENA                            BIT(0)

/* REG_PMU_APB_SP_SYS_DSLP_ENA */

#define BIT_PMU_APB_SP_SYS_DSLP_ENA                              BIT(0)

/* REG_PMU_APB_LIGHT_SLEEP_ENABLE */

#define BIT_PMU_APB_DMA_CHN2_LSLP_ENA                            BIT(20)
#define BIT_PMU_APB_DMA_CHN0_LSLP_ENA                            BIT(19)
#define BIT_PMU_APB_DMA_CHN3_LSLP_ENA                            BIT(18)
#define BIT_PMU_APB_DMA_CHN1_LSLP_ENA                            BIT(17)
#define BIT_PMU_APB_DMA_CHNALL_LSLP_ENA                          BIT(16)
#define BIT_PMU_APB_PUB_SYS_SMART_LSLP_ENA                       BIT(10)
#define BIT_PMU_APB_MM_LSLP_ENA                                  BIT(9)
#define BIT_PMU_APB_GPU_LSLP_ENA                                 BIT(8)
#define BIT_PMU_APB_WCN_SYS_LSLP_ENA                             BIT(4)
#define BIT_PMU_APB_AON_DMA_LSLP_ENA                             BIT(3)
#define BIT_PMU_APB_WTLCP_LSLP_ENA                               BIT(2)
#define BIT_PMU_APB_PUBCP_LSLP_ENA                               BIT(1)
#define BIT_PMU_APB_AP_LSLP_ENA                                  BIT(0)

/* REG_PMU_APB_LIGHT_SLEEP_MON */

#define BIT_PMU_APB_AP_LIGHT_SLEEP                               BIT(4)
#define BIT_PMU_APB_WTLCP_LIGHT_SLEEP                            BIT(3)
#define BIT_PMU_APB_PUBCP_LIGHT_SLEEP                            BIT(2)
#define BIT_PMU_APB_WCN_SYS_LIGHT_SLEEP                          BIT(1)
#define BIT_PMU_APB_AON_SYS_LIGHT_SLEEP                          BIT(0)

/* REG_PMU_APB_WCN_SYS_DSLP_ENA */

#define BIT_PMU_APB_WCN_SYS_XTL_STOP_BYPASS                      BIT(1)
#define BIT_PMU_APB_WCN_SYS_DSLP_ENA                             BIT(0)

/* REG_PMU_APB_WIFI_WRAP_DSLP_ENA */

#define BIT_PMU_APB_WIFI_WRAP_DSLP_ENA                           BIT(0)

/* REG_PMU_APB_GNSS_WRAP_DSLP_ENA */

#define BIT_PMU_APB_GNSS_WRAP_DSLP_ENA                           BIT(0)

/* REG_PMU_APB_PUB_ACC_RDY */

#define BIT_PMU_APB_PUB_ACC_RDY                                  BIT(0)

/* REG_PMU_APB_PUB_CLK_RDY */

#define BIT_PMU_APB_PUB_CLK_RDY                                  BIT(0)

/* REG_PMU_APB_EIC_SEL */

#define BIT_PMU_APB_EIC_LIGHT_SLEEP_SEL                          BIT(1)
#define BIT_PMU_APB_EIC_DEEP_SLEEP_SEL                           BIT(0)

/* REG_PMU_APB_AXI_LP_CTRL_DISABLE */


/* REG_PMU_APB_PMU_DEBUG */

#define BIT_PMU_APB_PMU_DEBUG(x)                                 (((x) & 0xFFFFFFFF))

/* REG_PMU_APB_SLEEP_CNT_CLR */

#define BIT_PMU_APB_AON_SYS_LIGHT_SLEEP_CNT_CLR                  BIT(19)
#define BIT_PMU_APB_PUBCP_SYS_SLEEP_CNT_CLR                      BIT(17)
#define BIT_PMU_APB_WTLCP_SYS_SLEEP_CNT_CLR                      BIT(16)
#define BIT_PMU_APB_AP_SYS_SLEEP_CNT_CLR                         BIT(15)
#define BIT_PMU_APB_WCN_SYS_SLEEP_CNT_CLR                        BIT(14)
#define BIT_PMU_APB_PUBCP_LIGHT_SLEEP_CNT_CLR                    BIT(13)
#define BIT_PMU_APB_WTLCP_LIGHT_SLEEP_CNT_CLR                    BIT(12)
#define BIT_PMU_APB_AP_LIGHT_SLEEP_CNT_CLR                       BIT(11)
#define BIT_PMU_APB_PUB_SYS_LIGHT_SLEEP_CNT_CLR                  BIT(10)
#define BIT_PMU_APB_WCN_SYS_LIGHT_SLEEP_CNT_CLR                  BIT(9)
#define BIT_PMU_APB_SP_SYS_DEEP_SLEEP_CNT_CLR                    BIT(8)
#define BIT_PMU_APB_PUBCP_DEEP_SLEEP_CNT_CLR                     BIT(6)
#define BIT_PMU_APB_WTLCP_DEEP_SLEEP_CNT_CLR                     BIT(5)
#define BIT_PMU_APB_AP_DEEP_SLEEP_CNT_CLR                        BIT(4)
#define BIT_PMU_APB_PUB_DEEP_SLEEP_CNT_CLR                       BIT(3)
#define BIT_PMU_APB_WCN_SYS_DEEP_SLEEP_CNT_CLR                   BIT(2)

/* REG_PMU_APB_LVDSRFPLL_REL_CFG */

#define BIT_PMU_APB_LVDSRFPLL_REF_SEL(x)                         (((x) & 0x3) << 8)

/* REG_PMU_APB_PAD_OUT_ADIE_CTRL0 */

#define BIT_PMU_APB_DCXO_LC_DEEP_SLEEP_WCN_SYS_DEEP_SLEEP_MASK   BIT(30)
#define BIT_PMU_APB_PAD_OUT_XTL_EN_WCN_SYS_DEEP_SLEEP_MASK       BIT(29)
#define BIT_PMU_APB_PAD_OUT_CHIP_SLEEP_WCN_SYS_DEEP_SLEEP_MASK   BIT(28)
#define BIT_PMU_APB_PAD_OUT_CHIP_SLEEP_PUB_SYS_DEEP_SLEEP_MASK   BIT(27)
#define BIT_PMU_APB_DCXO_LC_DEEP_SLEEP_POL_SEL                   BIT(26)
#define BIT_PMU_APB_PAD_OUT_XTL_EN_POL_SEL                       BIT(25)
#define BIT_PMU_APB_PAD_OUT_CHIP_SLEEP_POL_SEL                   BIT(24)
#define BIT_PMU_APB_PAD_OUT_XTL_EN_PUB_SYS_DEEP_SLEEP_MASK       BIT(23)
#define BIT_PMU_APB_DCXO_LC_DEEP_SLEEP_PUB_SYS_DEEP_SLEEP_MASK   BIT(22)
#define BIT_PMU_APB_DCXO_LC_DEEP_SLEEP_EXT_XTL_PD_MASK           BIT(21)
#define BIT_PMU_APB_DCXO_LC_DEEP_SLEEP_SP_SYS_DEEP_SLEEP_MASK    BIT(20)
#define BIT_PMU_APB_DCXO_LC_DEEP_SLEEP_CA53_TOP_PD_MASK          BIT(19)
#define BIT_PMU_APB_DCXO_LC_DEEP_SLEEP_WTLCP_DEEP_SLEEP_MASK     BIT(18)
#define BIT_PMU_APB_DCXO_LC_DEEP_SLEEP_PUBCP_DEEP_SLEEP_MASK     BIT(17)
#define BIT_PMU_APB_DCXO_LC_DEEP_SLEEP_AP_DEEP_SLEEP_MASK        BIT(16)
#define BIT_PMU_APB_PAD_OUT_XTL_EN_EXT_XTL_PD_MASK               BIT(15)
#define BIT_PMU_APB_PAD_OUT_XTL_EN_SP_SYS_DEEP_SLEEP_MASK        BIT(14)
#define BIT_PMU_APB_PAD_OUT_XTL_EN_CA53_TOP_PD_MASK              BIT(13)
#define BIT_PMU_APB_PAD_OUT_XTL_EN_WTLCP_DEEP_SLEEP_MASK         BIT(12)
#define BIT_PMU_APB_PAD_OUT_XTL_EN_PUBCP_DEEP_SLEEP_MASK         BIT(11)
#define BIT_PMU_APB_PAD_OUT_XTL_EN_AP_DEEP_SLEEP_MASK            BIT(10)
#define BIT_PMU_APB_PAD_OUT_CHIP_SLEEP_EXT_XTL_PD_MASK           BIT(9)
#define BIT_PMU_APB_PAD_OUT_CHIP_SLEEP_SP_SYS_DEEP_SLEEP_MASK    BIT(8)
#define BIT_PMU_APB_PAD_OUT_CHIP_SLEEP_CA53_TOP_PD_MASK          BIT(7)
#define BIT_PMU_APB_PAD_OUT_CHIP_SLEEP_WTLCP_DEEP_SLEEP_MASK     BIT(6)
#define BIT_PMU_APB_PAD_OUT_CHIP_SLEEP_PUBCP_DEEP_SLEEP_MASK     BIT(5)
#define BIT_PMU_APB_PAD_OUT_CHIP_SLEEP_AP_DEEP_SLEEP_MASK        BIT(4)
#define BIT_PMU_APB_EXT_XTL3_COMB_EN                             BIT(3)
#define BIT_PMU_APB_EXT_XTL2_COMB_EN                             BIT(2)
#define BIT_PMU_APB_EXT_XTL1_COMB_EN                             BIT(1)
#define BIT_PMU_APB_EXT_XTL0_COMB_EN                             BIT(0)

/* REG_PMU_APB_PAD_OUT_ADIE_CTRL1 */

#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN1_WCN_SYS_DEEP_SLEEP_MASK  BIT(25)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN0_WCN_SYS_DEEP_SLEEP_MASK  BIT(24)
#define BIT_PMU_APB_PAD_OUT_XTL_EN_PLL_PD_MASK                   BIT(23)
#define BIT_PMU_APB_DCXO_LC_DEEP_SLEEP_PLL_PD_MASK               BIT(22)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN1_PLL_PD_MASK              BIT(21)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN0_PLL_PD_MASK              BIT(20)
#define BIT_PMU_APB_PAD_OUT_CHIP_SLEEP_PLL_PD_MASK               BIT(19)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN1_POL_SEL                  BIT(18)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN0_POL_SEL                  BIT(17)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN1_PUB_SYS_DEEP_SLEEP_MASK  BIT(15)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN1_EXT_XTL_PD_MASK          BIT(14)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN1_SP_SYS_DEEP_SLEEP_MASK   BIT(13)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN1_CA53_TOP_PD_MASK         BIT(12)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN1_WTLCP_DEEP_SLEEP_MASK    BIT(11)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN1_PUBCP_DEEP_SLEEP_MASK    BIT(10)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN1_AP_DEEP_SLEEP_MASK       BIT(9)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN0_PUB_SYS_DEEP_SLEEP_MASK  BIT(7)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN0_EXT_XTL_PD_MASK          BIT(6)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN0_SP_SYS_DEEP_SLEEP_MASK   BIT(5)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN0_CA53_TOP_PD_MASK         BIT(4)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN0_WTLCP_DEEP_SLEEP_MASK    BIT(3)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN0_PUBCP_DEEP_SLEEP_MASK    BIT(2)
#define BIT_PMU_APB_PAD_OUT_XTL_BUF_EN0_AP_DEEP_SLEEP_MASK       BIT(1)

/* REG_PMU_APB_BISR_FORCE_SEL */

#define BIT_PMU_APB_PD_GNSS_WRAP_BISR_FORCE_SEL                  BIT(23)
#define BIT_PMU_APB_PD_WIFI_WRAP_BISR_FORCE_SEL                  BIT(22)
#define BIT_PMU_APB_PD_WCN_SYS_BISR_FORCE_SEL                    BIT(21)
#define BIT_PMU_APB_PD_WTLCP_LTE_P4_BISR_FORCE_SEL               BIT(20)
#define BIT_PMU_APB_PD_WTLCP_LTE_P3_BISR_FORCE_SEL               BIT(19)
#define BIT_PMU_APB_PD_AON_MEM_BISR_FORCE_SEL                    BIT(18)
#define BIT_PMU_APB_PD_PUBCP_SYS_BISR_FORCE_SEL                  BIT(17)
#define BIT_PMU_APB_PD_WTLCP_SYS_BISR_FORCE_SEL                  BIT(16)
#define BIT_PMU_APB_PD_WTLCP_HU3GE_B_BISR_FORCE_SEL              BIT(15)
#define BIT_PMU_APB_PD_WTLCP_HU3GE_A_BISR_FORCE_SEL              BIT(14)
#define BIT_PMU_APB_PD_WTLCP_TGDSP_BISR_FORCE_SEL                BIT(13)
#define BIT_PMU_APB_PD_WTLCP_LDSP_BISR_FORCE_SEL                 BIT(12)
#define BIT_PMU_APB_PD_WTLCP_LTE_P2_BISR_FORCE_SEL               BIT(10)
#define BIT_PMU_APB_PD_WTLCP_LTE_P1_BISR_FORCE_SEL               BIT(9)
#define BIT_PMU_APB_PD_WTLCP_TD_BISR_FORCE_SEL                   BIT(8)
#define BIT_PMU_APB_PD_MM_TOP_BISR_FORCE_SEL                     BIT(7)
#define BIT_PMU_APB_PD_GPU_TOP_BISR_FORCE_SEL                    BIT(6)
#define BIT_PMU_APB_PD_AP_SYS_BISR_FORCE_SEL                     BIT(5)
#define BIT_PMU_APB_PD_CA53_TOP_BISR_FORCE_SEL                   BIT(4)
#define BIT_PMU_APB_PD_CA53_C3_BISR_FORCE_SEL                    BIT(3)
#define BIT_PMU_APB_PD_CA53_C2_BISR_FORCE_SEL                    BIT(2)
#define BIT_PMU_APB_PD_CA53_C1_BISR_FORCE_SEL                    BIT(1)
#define BIT_PMU_APB_PD_CA53_C0_BISR_FORCE_SEL                    BIT(0)

/* REG_PMU_APB_AON_MEM_CTRL */

#define BIT_PMU_APB_SP_SYS_MEM_ALL_SEL                           BIT(1)
#define BIT_PMU_APB_AON_MEM_SP_SYS_SEL                           BIT(0)

/* REG_PMU_APB_PWR_DOMAIN_INT_CLR */

#define BIT_PMU_APB_INT_REQ_PWR_DOWN_CLR(x)                      (((x) & 0x3FF) << 16)
#define BIT_PMU_APB_INT_REQ_PWR_UP_CLR(x)                        (((x) & 0x3FF))

/* REG_PMU_APB_DDR_SLP_WAIT_CNT */

#define BIT_PMU_APB_PUB_SYS_DEEP_SLEEP_WAIT_CNT(x)               (((x) & 0xFFFF) << 16)
#define BIT_PMU_APB_PUB_SYS_SLEEP_WAIT_CNT(x)                    (((x) & 0xFFFF))

/* REG_PMU_APB_PMU_CLK_DIV_CFG */

#define BIT_PMU_APB_PWR_ST_CLK_DIV_CFG(x)                        (((x) & 0xFFFF) << 16)
#define BIT_PMU_APB_SLP_CTRL_CLK_DIV_CFG(x)                      (((x) & 0xFFFF))

/* REG_PMU_APB_CGM_PMU_SEL */

#define BIT_PMU_APB_CGM_PMU_SEL_REG(x)                           (((x) & 0x3))

/* REG_PMU_APB_PWR_DGB_PARAMETER */

#define BIT_PMU_APB_ISO_OFF_DLY(x)                               (((x) & 0xFF) << 16)
#define BIT_PMU_APB_CGM_ON_DLY(x)                                (((x) & 0xFF) << 8)
#define BIT_PMU_APB_RST_ASSERT_DLY(x)                            (((x) & 0xFF))

/* REG_PMU_APB_CA53_C0_DSLP_ENA */

#define BIT_PMU_APB_CA53_C0_DSLP_ENA                             BIT(0)

/* REG_PMU_APB_CA53_C1_DSLP_ENA */

#define BIT_PMU_APB_CA53_C1_DSLP_ENA                             BIT(0)

/* REG_PMU_APB_CA53_C2_DSLP_ENA */

#define BIT_PMU_APB_CA53_C2_DSLP_ENA                             BIT(0)

/* REG_PMU_APB_CA53_C3_DSLP_ENA */

#define BIT_PMU_APB_CA53_C3_DSLP_ENA                             BIT(0)

/* REG_PMU_APB_CA53_GIC_RST_EN */

#define BIT_PMU_APB_CA53_C3_GIC_RST_EN                           BIT(4)
#define BIT_PMU_APB_CA53_C2_GIC_RST_EN                           BIT(3)
#define BIT_PMU_APB_CA53_C1_GIC_RST_EN                           BIT(2)
#define BIT_PMU_APB_CA53_C0_GIC_RST_EN                           BIT(1)
#define BIT_PMU_APB_CA53_TOP_GIC_RST_EN                          BIT(0)

/* REG_PMU_APB_ANALOG_PHY_PD_CFG */

#define BIT_PMU_APB_PHY_PWR_DLY(x)                               (((x) & 0xFF) << 4)
#define BIT_PMU_APB_DSI_PD_REG                                   BIT(3)
#define BIT_PMU_APB_USB2PHY_PD_REG                               BIT(2)
#define BIT_PMU_APB_CSI_4LANE_PD_REG                             BIT(1)
#define BIT_PMU_APB_CSI_2LANE_PD_REG                             BIT(0)

/* REG_PMU_APB_PUB_SYS_DEEP_SLEEP_SEL */

#define BIT_PMU_APB_PUB_SYS_DEEP_SLEEP_SEL                       BIT(0)

/* REG_PMU_APB_PD_CA53_C0_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_CA53_C0_SHUTDOWN_MARK(x)                  (((x) & 0xF))

/* REG_PMU_APB_PD_CA53_C1_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_CA53_C1_SHUTDOWN_MARK(x)                  (((x) & 0xF))

/* REG_PMU_APB_PD_CA53_C2_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_CA53_C2_SHUTDOWN_MARK(x)                  (((x) & 0xF))

/* REG_PMU_APB_PD_CA53_C3_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_CA53_C3_SHUTDOWN_MARK(x)                  (((x) & 0xF))

/* REG_PMU_APB_PD_CA53_TOP_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_CA53_TOP_SHUTDOWN_MARK(x)                 (((x) & 0xF))

/* REG_PMU_APB_PD_AP_SYS_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_AP_SYS_SHUTDOWN_MARK(x)                   (((x) & 0xF))

/* REG_PMU_APB_PD_GPU_TOP_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_GPU_TOP_SHUTDOWN_MARK(x)                  (((x) & 0xF))

/* REG_PMU_APB_PD_MM_TOP_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_MM_TOP_SHUTDOWN_MARK(x)                   (((x) & 0xF))

/* REG_PMU_APB_PD_WTLCP_TD_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_WTLCP_TD_SHUTDOWN_MARK(x)                 (((x) & 0xF))

/* REG_PMU_APB_PD_WTLCP_LTE_P1_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_WTLCP_LTE_P1_SHUTDOWN_MARK(x)             (((x) & 0xF))

/* REG_PMU_APB_PD_WTLCP_LTE_P2_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_WTLCP_LTE_P2_SHUTDOWN_MARK(x)             (((x) & 0xF))

/* REG_PMU_APB_PD_WTLCP_LDSP_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_WTLCP_LDSP_SHUTDOWN_MARK(x)               (((x) & 0xF))

/* REG_PMU_APB_PD_WTLCP_TGDSP_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_WTLCP_TGDSP_SHUTDOWN_MARK(x)              (((x) & 0xF))

/* REG_PMU_APB_PD_WTLCP_HU3GE_A_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_WTLCP_HU3GE_A_SHUTDOWN_MARK(x)            (((x) & 0xF))

/* REG_PMU_APB_PD_WTLCP_HU3GE_B_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_WTLCP_HU3GE_B_SHUTDOWN_MARK(x)            (((x) & 0xF))

/* REG_PMU_APB_PD_WTLCP_SYS_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_WTLCP_SYS_SHUTDOWN_MARK(x)                (((x) & 0xF))

/* REG_PMU_APB_PD_PUBCP_SYS_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_PUBCP_SYS_SHUTDOWN_MARK(x)                (((x) & 0xF))

/* REG_PMU_APB_PD_WTLCP_LTE_P3_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_WTLCP_LTE_P3_SHUTDOWN_MARK(x)             (((x) & 0xF))

/* REG_PMU_APB_PD_WTLCP_LTE_P4_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_WTLCP_LTE_P4_SHUTDOWN_MARK(x)             (((x) & 0xF))

/* REG_PMU_APB_PD_PUB_SYS_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_PUB_SYS_SHUTDOWN_MARK(x)                  (((x) & 0xF))

/* REG_PMU_APB_PD_WCN_SYS_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_WCN_SYS_SHUTDOWN_MARK(x)                  (((x) & 0xF))

/* REG_PMU_APB_PD_WIFI_WRAP_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_WIFI_WRAP_SHUTDOWN_MARK(x)                (((x) & 0xF))

/* REG_PMU_APB_PD_GNSS_WRAP_SHUTDOWN_MARK_STATUS */

#define BIT_PMU_APB_PD_GNSS_WRAP_SHUTDOWN_MARK(x)                (((x) & 0xF))

/* REG_PMU_APB_AP_SYS_SLEEP_CNT */

#define BIT_PMU_APB_AP_SYS_SLEEP_CNT(x)                          (((x) & 0xFF))

/* REG_PMU_APB_WTLCP_SYS_SLEEP_CNT */

#define BIT_PMU_APB_WTLCP_SYS_SLEEP_CNT(x)                       (((x) & 0xFF))

/* REG_PMU_APB_PUBCP_SYS_SLEEP_CNT */

#define BIT_PMU_APB_PUBCP_SYS_SLEEP_CNT(x)                       (((x) & 0xFF))

/* REG_PMU_APB_WCN_SYS_SLEEP_CNT */

#define BIT_PMU_APB_WCN_SYS_SLEEP_CNT(x)                         (((x) & 0xFF))

/* REG_PMU_APB_PUB_SYS_LIGHT_SLEEP_CNT */

#define BIT_PMU_APB_PUB_SYS_LIGHT_SLEEP_CNT(x)                   (((x) & 0xFF))

/* REG_PMU_APB_AP_DEEP_SLEEP_CNT */

#define BIT_PMU_APB_AP_DEEP_SLEEP_CNT(x)                         (((x) & 0xFF))

/* REG_PMU_APB_SP_SYS_DEEP_SLEEP_CNT */

#define BIT_PMU_APB_SP_SYS_DEEP_SLEEP_CNT(x)                     (((x) & 0xFF))

/* REG_PMU_APB_WTLCP_DEEP_SLEEP_CNT */

#define BIT_PMU_APB_WTLCP_DEEP_SLEEP_CNT(x)                      (((x) & 0xFF))

/* REG_PMU_APB_PUBCP_DEEP_SLEEP_CNT */

#define BIT_PMU_APB_PUBCP_DEEP_SLEEP_CNT(x)                      (((x) & 0xFF))

/* REG_PMU_APB_WCN_SYS_DEEP_SLEEP_CNT */

#define BIT_PMU_APB_WCN_SYS_DEEP_SLEEP_CNT(x)                    (((x) & 0xFF))

/* REG_PMU_APB_PUB_SYS_DEEP_SLEEP_CNT */

#define BIT_PMU_APB_PUB_SYS_DEEP_SLEEP_CNT(x)                    (((x) & 0xFF))

/* REG_PMU_APB_AP_LIGHT_SLEEP_CNT */

#define BIT_PMU_APB_AP_LIGHT_SLEEP_CNT(x)                        (((x) & 0xFF))

/* REG_PMU_APB_WTLCP_LIGHT_SLEEP_CNT */

#define BIT_PMU_APB_WTLCP_LIGHT_SLEEP_CNT(x)                     (((x) & 0xFF))

/* REG_PMU_APB_PUBCP_LIGHT_SLEEP_CNT */

#define BIT_PMU_APB_PUBCP_LIGHT_SLEEP_CNT(x)                     (((x) & 0xFF))

/* REG_PMU_APB_WCN_SYS_LIGHT_SLEEP_CNT */

#define BIT_PMU_APB_WCN_LIGHT_SLEEP_CNT(x)                       (((x) & 0xFF))

/* REG_PMU_APB_AON_SYS_LIGHT_SLEEP_CNT */

#define BIT_PMU_APB_AON_LIGHT_SLEEP_CNT(x)                       (((x) & 0xFF))

/* REG_PMU_APB_SYS_SOFT_RST_BUSY */

#define BIT_PMU_APB_WCN_SYS_SRST_BUSY                            BIT(6)
#define BIT_PMU_APB_AP_SYS_SRST_BUSY                             BIT(5)
#define BIT_PMU_APB_CA53_SYS_SRST_BUSY                           BIT(4)
#define BIT_PMU_APB_GPU_SYS_SRST_BUSY                            BIT(3)
#define BIT_PMU_APB_MM_SYS_SRST_BUSY                             BIT(2)
#define BIT_PMU_APB_WTLCP_SYS_SRST_BUSY                          BIT(1)
#define BIT_PMU_APB_PUBCP_SYS_SRST_BUSY                          BIT(0)

/* REG_PMU_APB_REG_SYS_SRST_FRC_LP_ACK */

#define BIT_PMU_APB_REG_WCN_SRST_FRC_LP_ACK                      BIT(6)
#define BIT_PMU_APB_REG_AP_SRST_FRC_LP_ACK                       BIT(5)
#define BIT_PMU_APB_REG_CA53_SRST_FRC_LP_ACK                     BIT(4)
#define BIT_PMU_APB_REG_GPU_SRST_FRC_LP_ACK                      BIT(3)
#define BIT_PMU_APB_REG_MM_SRST_FRC_LP_ACK                       BIT(2)
#define BIT_PMU_APB_REG_WTLCP_SRST_FRC_LP_ACK                    BIT(1)
#define BIT_PMU_APB_REG_PUBCP_SRST_FRC_LP_ACK                    BIT(0)

/* REG_PMU_APB_SOFT_RST_SEL */

#define BIT_PMU_APB_SOFT_RST_SEL(x)                              (((x) & 0x7F))

/* REG_PMU_APB_REG_SYS_DDR_PWR_HS_ACK */

#define BIT_PMU_APB_REG_WCN_SYS_DDR_PWR_HS_ACK                   BIT(7)
#define BIT_PMU_APB_REG_PUBCP_SYS_DDR_PWR_HS_ACK                 BIT(6)
#define BIT_PMU_APB_REG_WTLCP_SYS_DDR_PWR_HS_ACK                 BIT(5)
#define BIT_PMU_APB_REG_CA53_SYS_DDR_PWR_HS_ACK                  BIT(4)
#define BIT_PMU_APB_REG_AP_SYS_DDR_PWR_HS_ACK                    BIT(3)
#define BIT_PMU_APB_REG_MM_SYS_DDR_PWR_HS_ACK                    BIT(2)
#define BIT_PMU_APB_REG_GPU_SYS_DDR_PWR_HS_ACK                   BIT(1)
#define BIT_PMU_APB_REG_AON_SYS_DDR_PWR_HS_ACK                   BIT(0)

/* REG_PMU_APB_CSI_DSI_PWR_CNT_DONE */

#define BIT_PMU_APB_CSI_4LANE_PWR_CNT_DONE                       BIT(2)
#define BIT_PMU_APB_CSI_2LANE_PWR_CNT_DONE                       BIT(1)
#define BIT_PMU_APB_DSI_PWR_CNT_DONE                             BIT(0)

/* REG_PMU_APB_PD_AP_SYS_DBG_SHUTDOWN__EN */

#define BIT_PMU_APB_PD_AP_SYS_DBG_SHUTDOWN_EN                    BIT(0)

/* REG_PMU_APB_WCN_SYS_SLEEP_XTL_ON_SEL */

#define BIT_PMU_APB_WCN_SYS_SLEEP_XTL_ON_SEL                     BIT(0)

/* REG_PMU_APB_EIC_SYS_SEL */

#define BIT_PMU_APB_EIC_SYS_SEL(x)                               (((x) & 0x3))

/* REG_PMU_APB_DDR_SLP_CTRL_STATE */

#define BIT_PMU_APB_DDR_SLP_CTRL_STATE(x)                        (((x) & 0xF))


#endif /* PMU_APB_H */

