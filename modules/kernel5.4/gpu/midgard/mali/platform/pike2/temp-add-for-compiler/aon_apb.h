/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * This file is dual-licensed: you can use it either under the terms
 * of the GPL or the X11 license, at your option. Note that this dual
 * licensing only applies to this file, and not this project as a
 * whole.
 *
 * updated at 2017-10-12 09:49:25
 *
 */


#ifndef AON_APB_H
#define AON_APB_H



#define REG_AON_APB_APB_EB0                           (0x0000)
#define REG_AON_APB_APB_EB1                           (0x0004)
#define REG_AON_APB_APB_RST0                          (0x0008)
#define REG_AON_APB_APB_RST1                          (0x000C)
#define REG_AON_APB_APB_RTC_EB                        (0x0010)
#define REG_AON_APB_REC_26MHZ_BUF_CFG                 (0x0014)
#define REG_AON_APB_APB_SEC_EB                        (0x001C)
#define REG_AON_APB_VBC_CTRL                          (0x0020)
#define REG_AON_APB_PWR_CTRL                          (0x0024)
#define REG_AON_APB_TS_CFG                            (0x0028)
#define REG_AON_APB_BOOT_MODE                         (0x002C)
#define REG_AON_APB_AON_CFG0                          (0x0030)
#define REG_AON_APB_PLL_SOFT_CNT_DONE                 (0x0038)
#define REG_AON_APB_DCXO_LC_REG0                      (0x003C)
#define REG_AON_APB_DCXO_LC_REG1                      (0x0040)
#define REG_AON_APB_MPLL_CFG1                         (0x0044)
#define REG_AON_APB_MPLL_CFG2                         (0x0048)
#define REG_AON_APB_DPLL_CFG1                         (0x004C)
#define REG_AON_APB_DPLL_CFG2                         (0x0050)
#define REG_AON_APB_TWPLL_CFG1                        (0x0054)
#define REG_AON_APB_TWPLL_CFG2                        (0x0058)
#define REG_AON_APB_AON_REG_PROT                      (0x006C)
#define REG_AON_APB_DSI_PHY_CTRL                      (0x0070)
#define REG_AON_APB_CSI_4P2L_M_PHY_CTRL               (0x007C)
#define REG_AON_APB_CSI_4P2L_DBG_PHY_CTRL             (0x0080)
#define REG_AON_APB_CSI_4P2L_PHY_CTRL                 (0x0084)
#define REG_AON_APB_AON_CGM_CFG                       (0x0088)
#define REG_AON_APB_SOFT_DFS_CTRL                     (0x00A0)
#define REG_AON_APB_HARD_DFS_CTRL_LO                  (0x00A4)
#define REG_AON_APB_HARD_DFS_CTRL_HI                  (0x00A8)
#define REG_AON_APB_HARD_DFS_HANSHAKE                 (0x00AC)
#define REG_AON_APB_APB_EB2                           (0x00B0)
#define REG_AON_APB_WCN_BUS_REMAP                     (0x00CC)
#define REG_AON_APB_ANALOG_TSEN_ADC_CONFIG0           (0x00D0)
#define REG_AON_APB_ANALOG_TSEN_ADC_CONFIG1           (0x00D4)
#define REG_AON_APB_WCN_CONFIG0                       (0x00D8)
#define REG_AON_APB_AON_CHIP_ID0                      (0x00E0)
#define REG_AON_APB_AON_CHIP_ID1                      (0x00E4)
#define REG_AON_APB_AON_PLAT_ID0                      (0x00E8)
#define REG_AON_APB_AON_PLAT_ID1                      (0x00EC)
#define REG_AON_APB_AON_IMPL_ID                       (0x00F0)
#define REG_AON_APB_AON_MFT_ID                        (0x00F4)
#define REG_AON_APB_AON_VER_ID                        (0x00F8)
#define REG_AON_APB_AON_CHIP_ID                       (0x00FC)
#define REG_AON_APB_ANALOG_TSEN_ADC_CONFIG2           (0x0100)
#define REG_AON_APB_CM4_SYS_SOFT_RST                  (0x0114)
#define REG_AON_APB_AON_DMA_INT_EN                    (0x011C)
#define REG_AON_APB_EMC_AUTO_GATE_EN                  (0x0120)
#define REG_AON_APB_CM4_CFG_BUS                       (0x0124)
#define REG_AON_APB_APB_RST2                          (0x0130)
#define REG_AON_APB_CLK_EB0                           (0x0134)
#define REG_AON_APB_MPLL_CTRL                         (0x013C)
#define REG_AON_APB_CPLL_CFG1                         (0x0150)
#define REG_AON_APB_CPLL_CFG2                         (0x0154)
#define REG_AON_APB_GPLL_CFG1                         (0x0158)
#define REG_AON_APB_GPLL_CFG2                         (0x015C)
#define REG_AON_APB_BUSMON_DMA_CFG                    (0x0170)
#define REG_AON_APB_ANALOG_CFG0                       (0x0174)
#define REG_AON_APB_ANALOG_CFG1                       (0x0178)
#define REG_AON_APB_MPLL_BIST_CTRL                    (0x0180)
#define REG_AON_APB_DPLL_BIST_CTRL                    (0x0184)
#define REG_AON_APB_CPLL_BIST_CTRL                    (0x0188)
#define REG_AON_APB_TWPLL_BIST_CTRL                   (0x018C)
#define REG_AON_APB_GPLL_BIST_CTRL                    (0x0190)
#define REG_AON_APB_DPLL_CTRL                         (0x0194)
#define REG_AON_APB_SENSOR_CFG0                       (0x01A0)
#define REG_AON_APB_ANALOG_IOS_CFG                    (0x01A4)
#define REG_AON_APB_MPLL_CFG3                         (0x01B0)
#define REG_AON_APB_DPLL_CFG3                         (0x01B4)
#define REG_AON_APB_CPLL_CFG3                         (0x01B8)
#define REG_AON_APB_TWPLL_CFG3                        (0x01BC)
#define REG_AON_APB_GPLL_CFG3                         (0x01C0)
#define REG_AON_APB_CP_DAP_PAD_CTRL                   (0x0200)
#define REG_AON_APB_CA7_PROT_CTRL                     (0x0204)
#define REG_AON_APB_CSSYS_CFG                         (0x0208)
#define REG_AON_APB_SEC_MUX_DBG_EN                    (0x020C)
#define REG_AON_APB_CR5_PROT_CTRL                     (0x0210)
#define REG_AON_APB_DBG_DJTAG_CTRL                    (0x0214)
#define REG_AON_APB_WTLCP_CTRL                        (0x0240)
#define REG_AON_APB_WTL_WCDMA_EB                      (0x0244)
#define REG_AON_APB_WTLCP_TDSP_CTRL0                  (0x0250)
#define REG_AON_APB_WTLCP_TDSP_CTRL1                  (0x0254)
#define REG_AON_APB_CP_CONFIG0                        (0x0258)
#define REG_AON_APB_CP_CONFIG1                        (0x025C)
#define REG_AON_APB_RF_CONFIG                         (0x0260)
#define REG_AON_APB_PCP_AON_EB                        (0x0280)
#define REG_AON_APB_PCP_SOFT_RST                      (0x0284)
#define REG_AON_APB_PUBCP_CTRL                        (0x0288)
#define REG_AON_APB_SYS_DBG_SEL                       (0x02B0)
#define REG_AON_APB_SYS_DBG_SEL2                      (0x02B8)
#define REG_AON_APB_SUBSYS_DBG_CFG                    (0x02BC)
#define REG_AON_APB_GLB_WCDMA_CTRL                    (0x0300)
#define REG_AON_APB_AON_MTX_EMC_LP_CTRL               (0x03D4)
#define REG_AON_APB_AON_MTX_MAIN_LP_CTRL              (0x03D8)
#define REG_AON_APB_AON_MTX_SW0_LP_CTRL               (0x03DC)
#define REG_AON_APB_AON_MTX_SW1_LP_CTRL               (0x03E0)
#define REG_AON_APB_AON_MTX__RF_LP_CTRL               (0x03E4)
#define REG_AON_APB_AON_MTX_WCN_LP_CTRL               (0x03E8)
#define REG_AON_APB_SUBSYS_LPC_FORCE_REQ              (0x03F8)
#define REG_AON_APB_SUBSYS_LPC_FORCE_ACK              (0x03FC)
#define REG_AON_APB_DAP_DJTAG_CTRL                    (0x0430)
#define REG_AON_APB_EFUSE_CONFIG0                     (0x0450)
#define REG_AON_APB_TOP_LPC0                          (0x0500)
#define REG_AON_APB_TOP_LPC1                          (0x0504)
#define REG_AON_APB_TOP_LPC2                          (0x0508)
#define REG_AON_APB_TOP_LPC3                          (0x050C)
#define REG_AON_APB_OVERHEAT_RST_CTRL                 (0x0510)
#define REG_AON_APB_TOP_LPC4                          (0x0514)
#define REG_AON_APB_TOP_LPC5                          (0x0518)
#define REG_AON_APB_DDR_SLEEP_CTRL0                   (0x0530)
#define REG_AON_APB_DDR_SLEEP_CTRL1                   (0x0534)
#define REG_AON_APB_DDR_SLEEP_CTRL2                   (0x0538)
#define REG_AON_APB_DDR_SLEEP_CTRL3                   (0x053C)
#define REG_AON_APB_ADI_CONFIG0                       (0x0540)
#define REG_AON_APB_AP_WPROT_EN1                      (0x3004)
#define REG_AON_APB_WTLCP_WPROT_EN1                   (0x3008)
#define REG_AON_APB_PUBCP_WPROT_EN1                   (0x300C)
#define REG_AON_APB_IO_DLY_CTRL                       (0x3014)
#define REG_AON_APB_AP_WPROT_EN0                      (0x3018)
#define REG_AON_APB_WTLCP_WPROT_EN0                   (0x3020)
#define REG_AON_APB_PUBCP_WPROT_EN0                   (0x3024)
#define REG_AON_APB_PMU_RST_MONITOR                   (0x302C)
#define REG_AON_APB_THM_RST_MONITOR                   (0x3030)
#define REG_AON_APB_AP_RST_MONITOR                    (0x3034)
#define REG_AON_APB_CA7_RST_MONITOR                   (0x3038)
#define REG_AON_APB_BOND_OPT0                         (0x303C)
#define REG_AON_APB_BOND_OPT1                         (0x3040)
#define REG_AON_APB_RES_REG0                          (0x3044)
#define REG_AON_APB_RES_REG1                          (0x3048)
#define REG_AON_APB_PLL_LOCK_OUT_SEL                  (0x3064)
#define REG_AON_APB_WDG_RST_FLAG                      (0x3080)
#define REG_AON_APB_CA7_CFG                           (0x3084)
#define REG_AON_APB_RES_REG2                          (0x3090)
#define REG_AON_APB_RES_REG3                          (0x3094)
#define REG_AON_APB_RES_REG4                          (0x3098)
#define REG_AON_APB_RES_REG5                          (0x309C)
#define REG_AON_APB_RES_REG6                          (0x30A0)
#define REG_AON_APB_RES_REG7                          (0x30A4)
#define REG_AON_APB_AON_APB_RSV                       (0x30F0)
#define REG_AON_APB_FUNCTION_DMA_BOOT_ADDR            (0x3110)
#define REG_AON_APB_SIM_HOT_PLUG_CTRL_PUBCP_SIM0      (0x3114)
#define REG_AON_APB_SIM_HOT_PLUG_CTRL_PUBCP_SIM1      (0x3118)
#define REG_AON_APB_SIM_HOT_PLUG_CTRL_PUBCP_SIM2      (0x311C)
#define REG_AON_APB_SIM_HOT_PLUG_CTRL_AP_SIM0         (0x3120)
#define REG_AON_APB_DEEP_SLEEP_SOFT                   (0x3124)

/* REG_AON_APB_APB_EB0 */

#define BIT_AON_APB_I2C_EB                            BIT(31)
#define BIT_AON_APB_CA7_DAP_EB                        BIT(30)
#define BIT_AON_APB_CA7_TS1_EB                        BIT(29)
#define BIT_AON_APB_CA7_TS0_EB                        BIT(28)
#define BIT_AON_APB_GPU_EB                            BIT(27)
#define BIT_AON_APB_CKG_EB                            BIT(26)
#define BIT_AON_APB_MM_EB                             BIT(25)
#define BIT_AON_APB_AP_WDG_EB                         BIT(24)
#define BIT_AON_APB_SPLK_EB                           BIT(22)
#define BIT_AON_APB_PIN_EB                            BIT(20)
#define BIT_AON_APB_VBC_EB                            BIT(19)
#define BIT_AON_APB_AUD_EB                            BIT(18)
#define BIT_AON_APB_AUDIF_EB                          BIT(17)
#define BIT_AON_APB_ADI_EB                            BIT(16)
#define BIT_AON_APB_INTC_EB                           BIT(15)
#define BIT_AON_APB_EIC_EB                            BIT(14)
#define BIT_AON_APB_REE_EFUSE_EB                      BIT(13)
#define BIT_AON_APB_AP_TMR0_EB                        BIT(12)
#define BIT_AON_APB_AON_TMR_EB                        BIT(11)
#define BIT_AON_APB_AP_SYST_EB                        BIT(10)
#define BIT_AON_APB_AON_SYST_EB                       BIT(9)
#define BIT_AON_APB_KPD_EB                            BIT(8)
#define BIT_AON_APB_PWM3_EB                           BIT(7)
#define BIT_AON_APB_PWM2_EB                           BIT(6)
#define BIT_AON_APB_PWM1_EB                           BIT(5)
#define BIT_AON_APB_PWM0_EB                           BIT(4)
#define BIT_AON_APB_GPIO_EB                           BIT(3)

/* REG_AON_APB_APB_EB1 */

#define BIT_AON_APB_SERDES_DPHY_EB                    BIT(31)
#define BIT_AON_APB_CROSS_TRIG_EB                     BIT(30)
#define BIT_AON_APB_DBG_EMC_EB                        BIT(29)
#define BIT_AON_APB_DBG_EB                            BIT(28)
#define BIT_AON_APB_DEF_EB                            BIT(25)
#define BIT_AON_APB_WCN_EB                            BIT(24)
#define BIT_AON_APB_CM4_JTAG_EB                       BIT(23)
#define BIT_AON_APB_REE_AON_DMA_EB                    BIT(22)
#define BIT_AON_APB_MBOX_EB                           BIT(21)
#define BIT_AON_APB_DJTAG_EB                          BIT(20)
#define BIT_AON_APB_RTC4M0_CAL_EB                     BIT(18)
#define BIT_AON_APB_MDAR_EB                           BIT(17)
#define BIT_AON_APB_MM_VSP_EB                         BIT(14)
#define BIT_AON_APB_GSP_EMC_EB                        BIT(13)
#define BIT_AON_APB_DJTAG_APB_EB                      BIT(11)
#define BIT_AON_APB_AP_TMR2_EB                        BIT(10)
#define BIT_AON_APB_AP_TMR1_EB                        BIT(9)
#define BIT_AON_APB_CA7_WDG_EB                        BIT(8)
#define BIT_AON_APB_CLK_EMC_REF_EB                    BIT(7)
#define BIT_AON_APB_AVS_EB                            BIT(6)
#define BIT_AON_APB_PROBE_EB                          BIT(5)
#define BIT_AON_APB_AUX2_EB                           BIT(4)
#define BIT_AON_APB_AUX1_EB                           BIT(3)
#define BIT_AON_APB_AUX0_EB                           BIT(2)
#define BIT_AON_APB_THM_EB                            BIT(1)
#define BIT_AON_APB_PMU_EB                            BIT(0)

/* REG_AON_APB_APB_RST0 */

#define BIT_AON_APB_CA5_TS0_SOFT_RST                  BIT(31)
#define BIT_AON_APB_I2C_SOFT_RST                      BIT(30)
#define BIT_AON_APB_CA7_TS1_SOFT_RST                  BIT(29)
#define BIT_AON_APB_CA7_TS0_SOFT_RST                  BIT(28)
#define BIT_AON_APB_DAP_MTX_SOFT_RST                  BIT(27)
#define BIT_AON_APB_SPLK_SOFT_RST                     BIT(24)
#define BIT_AON_APB_IPI_SOFT_RST                      BIT(23)
#define BIT_AON_APB_CKG_SOFT_RST                      BIT(22)
#define BIT_AON_APB_PIN_SOFT_RST                      BIT(21)
#define BIT_AON_APB_VBC_SOFT_RST                      BIT(20)
#define BIT_AON_APB_AUD_SOFT_RST                      BIT(19)
#define BIT_AON_APB_AUDIF_SOFT_RST                    BIT(18)
#define BIT_AON_APB_ADI_SOFT_RST                      BIT(17)
#define BIT_AON_APB_INTC_SOFT_RST                     BIT(16)
#define BIT_AON_APB_EIC_SOFT_RST                      BIT(15)
#define BIT_AON_APB_EFUSE_SOFT_RST                    BIT(14)
#define BIT_AON_APB_AP_WDG_SOFT_RST                   BIT(13)
#define BIT_AON_APB_AP_TMR0_SOFT_RST                  BIT(12)
#define BIT_AON_APB_AON_TMR_SOFT_RST                  BIT(11)
#define BIT_AON_APB_AP_SYST_SOFT_RST                  BIT(10)
#define BIT_AON_APB_AON_SYST_SOFT_RST                 BIT(9)
#define BIT_AON_APB_KPD_SOFT_RST                      BIT(8)
#define BIT_AON_APB_PWM3_SOFT_RST                     BIT(7)
#define BIT_AON_APB_PWM2_SOFT_RST                     BIT(6)
#define BIT_AON_APB_PWM1_SOFT_RST                     BIT(5)
#define BIT_AON_APB_PWM0_SOFT_RST                     BIT(4)
#define BIT_AON_APB_GPIO_SOFT_RST                     BIT(3)
#define BIT_AON_APB_SOFT_RST                          BIT(2)
#define BIT_AON_APB_FM_SOFT_RST                       BIT(1)
#define BIT_AON_APB_ADC_SOFT_RST                      BIT(0)

/* REG_AON_APB_APB_RST1 */

#define BIT_AON_APB_RTC4M_ANA_SOFT_RST                BIT(31)
#define BIT_AON_APB_DEF_SLV_INT_SOFT_CLR              BIT(30)
#define BIT_AON_APB_DEF_SOFT_RST                      BIT(29)
#define BIT_AON_APB_ADC3_SOFT_RST                     BIT(28)
#define BIT_AON_APB_ADC2_SOFT_RST                     BIT(27)
#define BIT_AON_APB_ADC1_SOFT_RST                     BIT(26)
#define BIT_AON_APB_MBOX_SOFT_RST                     BIT(25)
#define BIT_AON_APB_ROSC_SOFT_RST                     BIT(24)
#define BIT_AON_APB_RTC4M0_CAL_SOFT_RST               BIT(22)
#define BIT_AON_APB_SIM_AON_TOP_AP_SIM0_SOFT_RST      BIT(15)
#define BIT_AON_APB_SIM_AON_TOP_PUBCP_SIM2_SOFT_RST   BIT(14)
#define BIT_AON_APB_SIM_AON_TOP_PUBCP_SIM1_SOFT_RST   BIT(13)
#define BIT_AON_APB_SIM_AON_TOP_PUBCP_SIM0_SOFT_RST   BIT(12)
#define BIT_AON_APB_BB_CAL_SOFT_RST                   BIT(11)
#define BIT_AON_APB_DCXO_LC_SOFT_RST                  BIT(10)
#define BIT_AON_APB_AP_TMR2_SOFT_RST                  BIT(9)
#define BIT_AON_APB_AP_TMR1_SOFT_RST                  BIT(8)
#define BIT_AON_APB_CA7_WDG_SOFT_RST                  BIT(7)
#define BIT_AON_APB_AON_DMA_SOFT_RST                  BIT(6)
#define BIT_AON_APB_AVS_SOFT_RST                      BIT(5)
#define BIT_AON_APB_GPU_THMA_SOFT_RST                 BIT(3)
#define BIT_AON_APB_ARM_THMA_SOFT_RST                 BIT(2)
#define BIT_AON_APB_THM_SOFT_RST                      BIT(1)
#define BIT_AON_APB_PMU_SOFT_RST                      BIT(0)

/* REG_AON_APB_APB_RTC_EB */

#define BIT_AON_APB_BB_CAL_RTC_EB                     BIT(18)
#define BIT_AON_APB_DCXO_LC_RTC_EB                    BIT(17)
#define BIT_AON_APB_AP_TMR2_RTC_EB                    BIT(16)
#define BIT_AON_APB_AP_TMR1_RTC_EB                    BIT(15)
#define BIT_AON_APB_GPU_THMA_RTC_AUTO_EN              BIT(14)
#define BIT_AON_APB_ARM_THMA_RTC_AUTO_EN              BIT(13)
#define BIT_AON_APB_GPU_THMA_RTC_EB                   BIT(12)
#define BIT_AON_APB_ARM_THMA_RTC_EB                   BIT(11)
#define BIT_AON_APB_THM_RTC_EB                        BIT(10)
#define BIT_AON_APB_CA7_WDG_RTC_EB                    BIT(9)
#define BIT_AON_APB_AP_WDG_RTC_EB                     BIT(8)
#define BIT_AON_APB_EIC_RTCDV5_EB                     BIT(7)
#define BIT_AON_APB_EIC_RTC_EB                        BIT(6)
#define BIT_AON_APB_AP_TMR0_RTC_EB                    BIT(5)
#define BIT_AON_APB_AON_TMR_RTC_EB                    BIT(4)
#define BIT_AON_APB_AP_SYST_RTC_EB                    BIT(3)
#define BIT_AON_APB_AON_SYST_RTC_EB                   BIT(2)
#define BIT_AON_APB_KPD_RTC_EB                        BIT(1)
#define BIT_AON_APB_ARCH_RTC_EB                       BIT(0)

/* REG_AON_APB_REC_26MHZ_BUF_CFG */

#define BIT_AON_APB_PLL_PROBE_SEL(x)                  (((x) & 0x3F) << 8)

/* REG_AON_APB_APB_SEC_EB */

#define BIT_AON_APB_TEE_AON_DMA_EB                    BIT(1)
#define BIT_AON_APB_TEE_EFUSE_EB                      BIT(0)

/* REG_AON_APB_VBC_CTRL */

#define BIT_AON_APB_AUDIF_CKG_AUTO_EN                 BIT(20)
#define BIT_AON_APB_AUD_INT_SYS_SEL(x)                (((x) & 0x3) << 18)
#define BIT_AON_APB_VBC_DA23_INT_SYS_SEL(x)           (((x) & 0x3) << 16)
#define BIT_AON_APB_VBC_AD23_INT_SYS_SEL(x)           (((x) & 0x3) << 14)
#define BIT_AON_APB_VBC_AD01_INT_SYS_SEL(x)           (((x) & 0x3) << 12)
#define BIT_AON_APB_VBC_DA01_INT_SYS_SEL(x)           (((x) & 0x3) << 10)
#define BIT_AON_APB_VBC_AD23_DMA_SYS_SEL(x)           (((x) & 0x3) << 8)
#define BIT_AON_APB_VBC_AD01_DMA_SYS_SEL(x)           (((x) & 0x3) << 6)
#define BIT_AON_APB_VBC_DA01_DMA_SYS_SEL(x)           (((x) & 0x3) << 4)
#define BIT_AON_APB_VBC_DA23_DMA_SYS_SEL(x)           (((x) & 0x3) << 2)

/* REG_AON_APB_PWR_CTRL */

#define BIT_AON_APB_FORCE_DSI_DBG_PHY_SHUTDOWNZ       BIT(21)
#define BIT_AON_APB_FORCE_CSI_S_PHY_SHUTDOWNZ         BIT(20)
#define BIT_AON_APB_USB_PHY_PD_S                      BIT(17)
#define BIT_AON_APB_USB_PHY_PD_L                      BIT(16)
#define BIT_AON_APB_MIPI_DSI_PS_PD_S                  BIT(15)
#define BIT_AON_APB_MIPI_DSI_PS_PD_L                  BIT(14)
#define BIT_AON_APB_MIPI_CSI_4P2LANE_PS_PD_S          BIT(13)
#define BIT_AON_APB_MIPI_CSI_4P2LANE_PS_PD_L          BIT(12)
#define BIT_AON_APB_USB_REF_ATE_SEL                   BIT(8)
#define BIT_AON_APB_EFUSE_BIST_PWR_ON                 BIT(3)
#define BIT_AON_APB_FORCE_DSI_PHY_SHUTDOWNZ           BIT(2)
#define BIT_AON_APB_FORCE_CSI_PHY_SHUTDOWNZ           BIT(1)
#define BIT_AON_APB_DSI_REFCLK_SEL                    BIT(0)

/* REG_AON_APB_TS_CFG */

#define BIT_AON_APB_DBG_TRACE_CTRL_EN                 BIT(14)
#define BIT_AON_APB_EVENTACK_RESTARTREQ_TS01          BIT(4)
#define BIT_AON_APB_EVENT_RESTARTREQ_TS01             BIT(1)
#define BIT_AON_APB_EVENT_HALTREQ_TS01                BIT(0)

/* REG_AON_APB_BOOT_MODE */

#define BIT_AON_APB_PTEST_FUNC_ATSPEED_SEL            BIT(8)
#define BIT_AON_APB_PTEST_FUNC_MODE                   BIT(7)
#define BIT_AON_APB_FUNCTST_DMA_EB                    BIT(5)
#define BIT_AON_APB_USB_DLOAD_EN                      BIT(4)
#define BIT_AON_APB_ARM_BOOT_MD3                      BIT(3)
#define BIT_AON_APB_ARM_BOOT_MD2                      BIT(2)
#define BIT_AON_APB_ARM_BOOT_MD1                      BIT(1)
#define BIT_AON_APB_ARM_BOOT_MD0                      BIT(0)

/* REG_AON_APB_AON_CFG0 */

#define BIT_AON_APB_ENABLE_BBPLL_307P2M_SEL           BIT(5)
#define BIT_AON_APB_ENABLE_BBPLL_307P2M_REG           BIT(4)
#define BIT_AON_APB_ENABLE_BBPLL_416M_SEL             BIT(3)
#define BIT_AON_APB_ENABLE_BBPLL_416M_REG             BIT(2)
#define BIT_AON_APB_ENABLE_BBPLL_624M_SEL             BIT(1)
#define BIT_AON_APB_ENABLE_BBPLL_624M_REG             BIT(0)

/* REG_AON_APB_PLL_SOFT_CNT_DONE */

#define BIT_AON_APB_GPLL_SOFT_CNT_DONE                BIT(4)
#define BIT_AON_APB_CPLL_SOFT_CNT_DONE                BIT(3)
#define BIT_AON_APB_TWPLL_SOFT_CNT_DONE               BIT(2)
#define BIT_AON_APB_DPLL_SOFT_CNT_DONE                BIT(1)
#define BIT_AON_APB_MPLL_SOFT_CNT_DONE                BIT(0)

/* REG_AON_APB_DCXO_LC_REG0 */

#define BIT_AON_APB_DCXO_LC_FLAG                      BIT(8)
#define BIT_AON_APB_DCXO_LC_FLAG_CLR                  BIT(1)
#define BIT_AON_APB_DCXO_LC_CNT_CLR                   BIT(0)

/* REG_AON_APB_DCXO_LC_REG1 */

#define BIT_AON_APB_DCXO_LC_CNT(x)                    (((x) & 0xFFFFFFFF))

/* REG_AON_APB_MPLL_CFG1 */

#define BIT_AON_APB_MPLL_RESERVED(x)                  (((x) & 0xF) << 22)
#define BIT_AON_APB_MPLL_LOCK_DONE                    BIT(21)
#define BIT_AON_APB_MPLL_DIV_S                        BIT(20)
#define BIT_AON_APB_MPLL_MOD_EN                       BIT(19)
#define BIT_AON_APB_MPLL_SDM_EN                       BIT(18)
#define BIT_AON_APB_MPLL_REF_SEL                      BIT(16)
#define BIT_AON_APB_MPLL_TEST_EN                      BIT(15)
#define BIT_AON_APB_MPLL_IL_DIV2                      BIT(14)
#define BIT_AON_APB_MPLL_OL_DIV2                      BIT(13)
#define BIT_AON_APB_MPLL_IBIAS(x)                     (((x) & 0x3) << 11)
#define BIT_AON_APB_MPLL_N(x)                         (((x) & 0x7FF))

/* REG_AON_APB_MPLL_CFG2 */

#define BIT_AON_APB_MPLL_NINT(x)                      (((x) & 0x7F) << 23)
#define BIT_AON_APB_MPLL_KINT(x)                      (((x) & 0x7FFFFF))

/* REG_AON_APB_DPLL_CFG1 */

#define BIT_AON_APB_DPLL_RESERVED(x)                  (((x) & 0xF) << 22)
#define BIT_AON_APB_DPLL_LOCK_DONE                    BIT(21)
#define BIT_AON_APB_DPLL_DIV_S                        BIT(20)
#define BIT_AON_APB_DPLL_MOD_EN                       BIT(19)
#define BIT_AON_APB_DPLL_SDM_EN                       BIT(18)
#define BIT_AON_APB_DPLL_REF_SEL                      BIT(16)
#define BIT_AON_APB_DPLL_TEST_EN                      BIT(15)
#define BIT_AON_APB_DPLL_IL_DIV2                      BIT(14)
#define BIT_AON_APB_DPLL_OL_DIV2                      BIT(13)
#define BIT_AON_APB_DPLL_IBIAS(x)                     (((x) & 0x3) << 11)
#define BIT_AON_APB_DPLL_N(x)                         (((x) & 0x7FF))

/* REG_AON_APB_DPLL_CFG2 */

#define BIT_AON_APB_DPLL_NINT(x)                      (((x) & 0x7F) << 23)
#define BIT_AON_APB_DPLL_KINT(x)                      (((x) & 0x7FFFFF))

/* REG_AON_APB_TWPLL_CFG1 */

#define BIT_AON_APB_TWPLL_RESERVED(x)                 (((x) & 0xF) << 22)
#define BIT_AON_APB_TWPLL_LOCK_DONE                   BIT(21)
#define BIT_AON_APB_TWPLL_DIV_S                       BIT(20)
#define BIT_AON_APB_TWPLL_MOD_EN                      BIT(19)
#define BIT_AON_APB_TWPLL_SDM_EN                      BIT(18)
#define BIT_AON_APB_TWPLL_REF_SEL                     BIT(16)
#define BIT_AON_APB_TWPLL_TEST_EN                     BIT(15)
#define BIT_AON_APB_TWPLL_IL_DIV2                     BIT(14)
#define BIT_AON_APB_TWPLL_OL_DIV2                     BIT(13)
#define BIT_AON_APB_TWPLL_IBIAS(x)                    (((x) & 0x3) << 11)
#define BIT_AON_APB_TWPLL_N(x)                        (((x) & 0x7FF))

/* REG_AON_APB_TWPLL_CFG2 */

#define BIT_AON_APB_TWPLL_NINT(x)                     (((x) & 0x7F) << 23)
#define BIT_AON_APB_TWPLL_KINT(x)                     (((x) & 0x7FFFFF))

/* REG_AON_APB_AON_REG_PROT */

#define BIT_AON_APB_TDSP_CTRL_PROT                    BIT(31)
#define BIT_AON_APB_REG_PROT_REG(x)                   (((x) & 0xFFFF))

/* REG_AON_APB_DSI_PHY_CTRL */

#define BIT_AON_APB_DSI_IF_SEL                        BIT(24)
#define BIT_AON_APB_DSI_TRIMBG(x)                     (((x) & 0xF) << 20)
#define BIT_AON_APB_DSI_RCTL(x)                       (((x) & 0xF) << 16)
#define BIT_AON_APB_DSI_RES(x)                        (((x) & 0xFFFF))

/* REG_AON_APB_CSI_4P2L_M_PHY_CTRL */

#define BIT_AON_APB_CSI_4P2L_CFGCLK_M_EN              BIT(24)
#define BIT_AON_APB_CSI_4P2L_TESTCLR_M                BIT(23)
#define BIT_AON_APB_CSI_4P2L_TESTCLR_M_SEL            BIT(22)
#define BIT_AON_APB_CSI_4P2L_TESTCLK_M_EN             BIT(21)
#define BIT_AON_APB_CSI_4P2L_M_IF_SEL                 BIT(20)
#define BIT_AON_APB_CSI_4P2L_RCTL(x)                  (((x) & 0xF) << 16)
#define BIT_AON_APB_CSI_4P2L_RES(x)                   (((x) & 0xFFFF))

/* REG_AON_APB_CSI_4P2L_DBG_PHY_CTRL */

#define BIT_AON_APB_CSI_4P2L_DBG_EN                   BIT(25)
#define BIT_AON_APB_CSI_4P2L_DBG_IF_SEL               BIT(24)
#define BIT_AON_APB_CSI_4P2L_DBG_TRIMBG(x)            (((x) & 0xF) << 20)

/* REG_AON_APB_CSI_4P2L_PHY_CTRL */

#define BIT_AON_APB_CSI_4P2L_MODE_SEL                 BIT(0)

/* REG_AON_APB_AON_CGM_CFG */

#define BIT_AON_APB_PROBE_CKG_DIV(x)                  (((x) & 0xF) << 28)
#define BIT_AON_APB_AUX2_CKG_DIV(x)                   (((x) & 0xF) << 24)
#define BIT_AON_APB_AUX1_CKG_DIV(x)                   (((x) & 0xF) << 20)
#define BIT_AON_APB_AUX0_CKG_DIV(x)                   (((x) & 0xF) << 16)
#define BIT_AON_APB_PROBE_CKG_SEL(x)                  (((x) & 0xF) << 12)
#define BIT_AON_APB_AUX2_CKG_SEL(x)                   (((x) & 0xF) << 8)
#define BIT_AON_APB_AUX1_CKG_SEL(x)                   (((x) & 0xF) << 4)
#define BIT_AON_APB_AUX0_CKG_SEL(x)                   (((x) & 0xF))

/* REG_AON_APB_SOFT_DFS_CTRL */

#define BIT_AON_APB_PUB_DFS_SW_SWITCH_PERIOD(x)       (((x) & 0xFF) << 16)
#define BIT_AON_APB_PUB_DFS_SW_RATIO(x)               (((x) & 0x1F) << 6)
#define BIT_AON_APB_PUB_DFS_SW_FRQ_SEL(x)             (((x) & 0x3) << 4)
#define BIT_AON_APB_PUB_DFS_SW_RESP                   BIT(3)
#define BIT_AON_APB_PUB_DFS_SW_ACK                    BIT(2)
#define BIT_AON_APB_PUB_DFS_SW_REQ                    BIT(1)
#define BIT_AON_APB_PUB_DFS_SW_ENABLE                 BIT(0)

/* REG_AON_APB_HARD_DFS_CTRL_LO */

#define BIT_AON_APB_PUB_DFS_HW_INITIAL_FREQ(x)        (((x) & 0x3) << 3)
#define BIT_AON_APB_PUB_DFS_HW_STOP                   BIT(2)
#define BIT_AON_APB_PUB_DFS_HW_START                  BIT(1)
#define BIT_AON_APB_PUB_DFS_HW_ENABLE                 BIT(0)

/* REG_AON_APB_HARD_DFS_CTRL_HI */

#define BIT_AON_APB_PUB_DFS_HW_SWITCH_PERIOD(x)       (((x) & 0xFF) << 20)
#define BIT_AON_APB_PUB_DFS_HW_F3_RATIO(x)            (((x) & 0x1F) << 15)
#define BIT_AON_APB_PUB_DFS_HW_F2_RATIO(x)            (((x) & 0x1F) << 10)
#define BIT_AON_APB_PUB_DFS_HW_F1_RATIO(x)            (((x) & 0x1F) << 5)
#define BIT_AON_APB_PUB_DFS_HW_F0_RATIO(x)            (((x) & 0x1F))

/* REG_AON_APB_HARD_DFS_HANSHAKE */

#define BIT_AON_APB_WCN_WIFI_DFS_REQ_TO_PUB_EB        BIT(5)
#define BIT_AON_APB_WCN_WIFI_DFS_ACK_INT_CLR          BIT(4)
#define BIT_AON_APB_WCN_WIFI_DFS_ACK_INT_EN           BIT(3)
#define BIT_AON_APB_WCN_WIFI_DFS_ACK_INT_STAT         BIT(2)
#define BIT_AON_APB_WCN_WIFI_DFS_ACK                  BIT(1)
#define BIT_AON_APB_WCN_WIFI_DFS_REQ                  BIT(0)

/* REG_AON_APB_APB_EB2 */

#define BIT_AON_APB_AP_DAP_EB                         BIT(15)
#define BIT_AON_APB_BSMTMR_EB                         BIT(14)
#define BIT_AON_APB_ANLG_APB_EB                       BIT(13)
#define BIT_AON_APB_PIN_APB_EB                        BIT(12)
#define BIT_AON_APB_ANLG_EB                           BIT(11)
#define BIT_AON_APB_BUSMON_DMA_EB                     BIT(10)
#define BIT_AON_APB_SERDES_DPHY_REF_EB                BIT(9)
#define BIT_AON_APB_SERDES_DPHY_CFG_EB                BIT(8)
#define BIT_AON_APB_ROSC_EB                           BIT(7)
#define BIT_AON_APB_PUB_REG_EB                        BIT(6)
#define BIT_AON_APB_DMC_EB                            BIT(5)
#define BIT_AON_APB_CSSYS_EB                          BIT(4)
#define BIT_AON_APB_WCDMA_ICI_EB                      BIT(1)
#define BIT_AON_APB_WCDMA_EB                          BIT(0)

/* REG_AON_APB_WCN_BUS_REMAP */

#define BIT_AON_APB_WCNTODDR_ADDR_OFFSET(x)           (((x) & 0xFF) << 8)
#define BIT_AON_APB_WCNTOAON_ADDR_OFFSET(x)           (((x) & 0xF))

/* REG_AON_APB_ANALOG_TSEN_ADC_CONFIG0 */

#define BIT_AON_APB_RG_TSEN_CHOP_CLKSEL(x)            (((x) & 0x3) << 24)
#define BIT_AON_APB_RG_TSEN_CLKSEL(x)                 (((x) & 0x3) << 20)
#define BIT_AON_APB_RG_TSEN_SDADC_BIAS(x)             (((x) & 0x3) << 16)
#define BIT_AON_APB_RG_TSEN_UGBUF_BIAS(x)             (((x) & 0x3) << 12)
#define BIT_AON_APB_RG_TSEN_SDADC_VCMI(x)             (((x) & 0x3) << 8)
#define BIT_AON_APB_RG_TSEN_SDADC_VCMO(x)             (((x) & 0x3) << 4)
#define BIT_AON_APB_RG_UGBUF_CTRL(x)                  (((x) & 0x3))

/* REG_AON_APB_ANALOG_TSEN_ADC_CONFIG1 */

#define BIT_AON_APB_RG_TSEN_ADCLDO_EN                 BIT(26)
#define BIT_AON_APB_RG_TSEN_SDADC_CAPCHOP_EN          BIT(25)
#define BIT_AON_APB_RG_TSEN_SDADC_CHOP_EN             BIT(24)
#define BIT_AON_APB_RG_TSEN_UGBUF_CHOP_EN             BIT(23)
#define BIT_AON_APB_RG_TSEN_SDADC_EN                  BIT(22)
#define BIT_AON_APB_RG_TSEN_SDADC_OFFSET_EN           BIT(21)
#define BIT_AON_APB_RG_TSEN_INPUT_EN                  BIT(20)
#define BIT_AON_APB_RG_TSEN_UGBUF_EN                  BIT(19)
#define BIT_AON_APB_RG_TSEN_SDADC_DATA_EDGE_SEL       BIT(18)
#define BIT_AON_APB_RG_TSEN_SDADC_RST                 BIT(17)
#define BIT_AON_APB_RG_TSEN_ADCLDOREF(x)              (((x) & 0x1F) << 8)
#define BIT_AON_APB_RG_TSEN_ADCLDO_V(x)               (((x) & 0xF))

/* REG_AON_APB_WCN_CONFIG0 */

#define BIT_AON_APB_WCN_GNSS_CM4_ADDR_OFFSET(x)       (((x) & 0xFFFFFF) << 8)
#define BIT_AON_APB_WCN_OFFCHIP_XTRL_EN               BIT(5)
#define BIT_AON_APB_WCN_XTAL_DIF_SEL_H                BIT(4)
#define BIT_AON_APB_WCN_CM4_ADDR_REMAP_SEL(x)         (((x) & 0x3))

/* REG_AON_APB_AON_CHIP_ID0 */

#define BIT_AON_APB_AON_CHIP_ID0(x)                   (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AON_CHIP_ID1 */

#define BIT_AON_APB_AON_CHIP_ID1(x)                   (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AON_PLAT_ID0 */

#define BIT_AON_APB_AON_PLAT_ID0(x)                   (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AON_PLAT_ID1 */

#define BIT_AON_APB_AON_PLAT_ID1(x)                   (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AON_IMPL_ID */

#define BIT_AON_APB_AON_IMPL_ID(x)                    (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AON_MFT_ID */

#define BIT_AON_APB_AON_MFT_ID(x)                     (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AON_VER_ID */

#define BIT_AON_APB_AON_VER_ID(x)                     (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AON_CHIP_ID */

#define BIT_AON_APB_AON_CHIP_ID(x)                    (((x) & 0xFFFFFFFF))

/* REG_AON_APB_ANALOG_TSEN_ADC_CONFIG2 */

#define BIT_AON_APB_RG_TSEN_BIST_CODE(x)              (((x) & 0x7) << 12)
#define BIT_AON_APB_RG_TSEN_TEST_CLK_SEL              BIT(9)
#define BIT_AON_APB_RG_TSEN_BIST_EN                   BIT(8)
#define BIT_AON_APB_RG_TSEN_RESERVED(x)               (((x) & 0xFF))

/* REG_AON_APB_CM4_SYS_SOFT_RST */

#define BIT_AON_APB_CM4_SYS_SOFT_RST                  BIT(4)
#define BIT_AON_APB_CM4_CORE_SOFT_RST                 BIT(0)

/* REG_AON_APB_AON_DMA_INT_EN */

#define BIT_AON_APB_AON_DMA_INT_CM4_EN                BIT(6)
#define BIT_AON_APB_AON_DMA_INT_AP_EN                 BIT(0)

/* REG_AON_APB_EMC_AUTO_GATE_EN */

#define BIT_AON_APB_MAILBOX_PCLK_AUTO_GATE_EN         BIT(19)
#define BIT_AON_APB_WTLCP_PUB_AUTO_GATE_EN            BIT(18)
#define BIT_AON_APB_AP_PUB_AUTO_GATE_EN               BIT(17)
#define BIT_AON_APB_AON_APB_PUB_AUTO_GATE_EN          BIT(16)
#define BIT_AON_APB_PUBCP_EMC_AUTO_GATE_EN            BIT(3)
#define BIT_AON_APB_WTLCP_EMC_AUTO_GATE_EN            BIT(2)
#define BIT_AON_APB_AP_EMC_AUTO_GATE_EN               BIT(1)
#define BIT_AON_APB_CA7_EMC_AUTO_GATE_EN              BIT(0)

/* REG_AON_APB_CM4_CFG_BUS */

#define BIT_AON_APB_CM4_CFG_BUS_SLEEP                 BIT(0)

/* REG_AON_APB_APB_RST2 */

#define BIT_AON_APB_TOP_REG_SLICE_SOFT_RST(x)         (((x) & 0xFF) << 20)
#define BIT_AON_APB_THM1_SOFT_RST                     BIT(19)
#define BIT_AON_APB_BSMTMR_SOFT_RST                   BIT(18)
#define BIT_AON_APB_WTLCP_TDSP_CORE_SRST              BIT(17)
#define BIT_AON_APB_WCN_DJTAG_SOFT_RST                BIT(16)
#define BIT_AON_APB_DIG_DJTAG_SOFT_RST                BIT(15)
#define BIT_AON_APB_ANLG_SOFT_RST                     BIT(14)
#define BIT_AON_APB_SERDES_DPHY_APB_SOFT_RST          BIT(13)
#define BIT_AON_APB_BUSMON_DMA_SOFT_RST               BIT(12)
#define BIT_AON_APB_SERDES_DPHY_SOFT_RST              BIT(11)
#define BIT_AON_APB_CROSS_TRIG_SOFT_RST               BIT(10)
#define BIT_AON_APB_SERDES_SOFT_RST                   BIT(9)
#define BIT_AON_APB_DBG_SOFT_RST                      BIT(8)
#define BIT_AON_APB_DJTAG_SOFT_RST                    BIT(7)
#define BIT_AON_APB_AON_DJTAG_SOFT_RST                BIT(6)
#define BIT_AON_APB_PUB_DJTAG_SOFT_RST                BIT(5)
#define BIT_AON_APB_GPU_DJTAG_SOFT_RST                BIT(4)
#define BIT_AON_APB_MM_DJTAG_SOFT_RST                 BIT(3)
#define BIT_AON_APB_PUBCP_DJTAG_SOFT_RST              BIT(2)
#define BIT_AON_APB_WTLCP_DJTAG_SOFT_RST              BIT(1)
#define BIT_AON_APB_AP_DJTAG_SOFT_RST                 BIT(0)

/* REG_AON_APB_CLK_EB0 */

#define BIT_AON_APB_ALL_PLL_TEST_EB                   BIT(18)
#define BIT_AON_APB_CLK_26M_AUDIO_EB                  BIT(14)
#define BIT_AON_APB_CLK_26M_NFC_EB                    BIT(13)
#define BIT_AON_APB_CLK_26M_DTV_EB                    BIT(12)
#define BIT_AON_APB_TMR_EB                            BIT(11)
#define BIT_AON_APB_DET_32K_EB                        BIT(10)
#define BIT_AON_APB_AP_HS_SPI_EB                      BIT(9)
#define BIT_AON_APB_CSSYS_CA7_EB                      BIT(8)
#define BIT_AON_APB_NANDC_2X_EB                       BIT(7)
#define BIT_AON_APB_NANDC_1X_EB                       BIT(6)
#define BIT_AON_APB_SDIO1_2X_EB                       BIT(5)
#define BIT_AON_APB_SDIO1_1X_EB                       BIT(4)
#define BIT_AON_APB_SDIO0_2X_EB                       BIT(3)
#define BIT_AON_APB_SDIO0_1X_EB                       BIT(2)
#define BIT_AON_APB_EMMC_2X_EB                        BIT(1)
#define BIT_AON_APB_EMMC_1X_EB                        BIT(0)

/* REG_AON_APB_MPLL_CTRL */

#define BIT_AON_APB_CGM_MPLL_CA7_FORCE_EN             BIT(9)
#define BIT_AON_APB_CGM_MPLL_CA7_AUTO_GATE_SEL        BIT(8)
#define BIT_AON_APB_MPLL_WAIT_FORCE_EN                BIT(2)
#define BIT_AON_APB_MPLL_WAIT_AUTO_GATE_SEL           BIT(1)

/* REG_AON_APB_CPLL_CFG1 */

#define BIT_AON_APB_CPLL_RESERVED(x)                  (((x) & 0xF) << 22)
#define BIT_AON_APB_CPLL_LOCK_DONE                    BIT(21)
#define BIT_AON_APB_CPLL_DIV_S                        BIT(20)
#define BIT_AON_APB_CPLL_MOD_EN                       BIT(19)
#define BIT_AON_APB_CPLL_SDM_EN                       BIT(18)
#define BIT_AON_APB_CPLL_REF_SEL                      BIT(16)
#define BIT_AON_APB_CPLL_TEST_EN                      BIT(15)
#define BIT_AON_APB_CPLL_IL_DIV2                      BIT(14)
#define BIT_AON_APB_CPLL_OL_DIV2                      BIT(13)
#define BIT_AON_APB_CPLL_IBIAS(x)                     (((x) & 0x3) << 11)
#define BIT_AON_APB_CPLL_N(x)                         (((x) & 0x7FF))

/* REG_AON_APB_CPLL_CFG2 */

#define BIT_AON_APB_CPLL_NINT(x)                      (((x) & 0x7F) << 23)
#define BIT_AON_APB_CPLL_KINT(x)                      (((x) & 0x7FFFFF))

/* REG_AON_APB_GPLL_CFG1 */

#define BIT_AON_APB_GPLL_RESERVED(x)                  (((x) & 0xF) << 22)
#define BIT_AON_APB_GPLL_LOCK_DONE                    BIT(21)
#define BIT_AON_APB_GPLL_DIV_S                        BIT(20)
#define BIT_AON_APB_GPLL_MOD_EN                       BIT(19)
#define BIT_AON_APB_GPLL_SDM_EN                       BIT(18)
#define BIT_AON_APB_GPLL_REF_SEL                      BIT(16)
#define BIT_AON_APB_GPLL_TEST_EN                      BIT(15)
#define BIT_AON_APB_GPLL_IL_DIV2                      BIT(14)
#define BIT_AON_APB_GPLL_OL_DIV2                      BIT(13)
#define BIT_AON_APB_GPLL_IBIAS(x)                     (((x) & 0x3) << 11)
#define BIT_AON_APB_GPLL_N(x)                         (((x) & 0x7FF))

/* REG_AON_APB_GPLL_CFG2 */

#define BIT_AON_APB_GPLL_NINT(x)                      (((x) & 0x7F) << 23)
#define BIT_AON_APB_GPLL_KINT(x)                      (((x) & 0x7FFFFF))

/* REG_AON_APB_BUSMON_DMA_CFG */

#define BIT_AON_APB_BUSMON_DMA_CNT_START              BIT(0)

/* REG_AON_APB_ANALOG_CFG0 */

#define BIT_AON_APB_ANALOG_PLL_RSV(x)                 (((x) & 0xFF) << 16)

/* REG_AON_APB_ANALOG_CFG1 */

#define BIT_AON_APB_ANA_BB_RSV(x)                     (((x) & 0xFF))

/* REG_AON_APB_MPLL_BIST_CTRL */

#define BIT_AON_APB_MPLL_BIST_CNT(x)                  (((x) & 0xFFFF) << 16)
#define BIT_AON_APB_MPLL_BIST_CTRL(x)                 (((x) & 0x1FF) << 1)
#define BIT_AON_APB_MPLL_BIST_EN                      BIT(0)

/* REG_AON_APB_DPLL_BIST_CTRL */

#define BIT_AON_APB_DPLL_BIST_CNT(x)                  (((x) & 0xFFFF) << 16)
#define BIT_AON_APB_DPLL_BIST_CTRL(x)                 (((x) & 0x1FF) << 1)
#define BIT_AON_APB_DPLL_BIST_EN                      BIT(0)

/* REG_AON_APB_CPLL_BIST_CTRL */

#define BIT_AON_APB_CPLL_BIST_CNT(x)                  (((x) & 0xFFFF) << 16)
#define BIT_AON_APB_CPLL_BIST_CTRL(x)                 (((x) & 0x1FF) << 1)
#define BIT_AON_APB_CPLL_BIST_EN                      BIT(0)

/* REG_AON_APB_TWPLL_BIST_CTRL */

#define BIT_AON_APB_TWPLL_BIST_CNT(x)                 (((x) & 0xFFFF) << 16)
#define BIT_AON_APB_TWPLL_BIST_CTRL(x)                (((x) & 0x1FF) << 1)
#define BIT_AON_APB_TWPLL_BIST_EN                     BIT(0)

/* REG_AON_APB_GPLL_BIST_CTRL */

#define BIT_AON_APB_GPLL_BIST_CNT(x)                  (((x) & 0xFFFF) << 16)
#define BIT_AON_APB_GPLL_BIST_EN                      BIT(0)

/* REG_AON_APB_DPLL_CTRL */

#define BIT_AON_APB_CGM_DPLL_40M_AON_FORCE_EN         BIT(10)
#define BIT_AON_APB_CGM_DPLL_40M_AON_AUTO_GATE_SEL    BIT(9)
#define BIT_AON_APB_CGM_DPLL_AON_FORCE_EN             BIT(8)
#define BIT_AON_APB_CGM_DPLL_AON_AUTO_GATE_SEL        BIT(7)
#define BIT_AON_APB_CGM_DPLL_AP_FORCE_EN              BIT(6)
#define BIT_AON_APB_CGM_DPLL_AP_AUTO_GATE_SEL         BIT(5)
#define BIT_AON_APB_DPLL_DIV_40M_FORCE_EN             BIT(4)
#define BIT_AON_APB_DPLL_DIV_40M_AUTO_GATE_SEL        BIT(3)
#define BIT_AON_APB_DPLL_WAIT_FORCE_EN                BIT(2)
#define BIT_AON_APB_DPLL_WAIT_AUTO_GATE_SEL           BIT(1)

/* REG_AON_APB_SENSOR_CFG0 */

#define BIT_AON_APB_SENSOR_PD_POL(x)                  (((x) & 0x3) << 4)
#define BIT_AON_APB_SENSOR_RST_POL(x)                 (((x) & 0x3))

/* REG_AON_APB_ANALOG_IOS_CFG */

#define BIT_AON_APB_DSI_ISO_EN                        BIT(2)
#define BIT_AON_APB_EFS2_ISO_EN                       BIT(1)
#define BIT_AON_APB_EFS01_ISO_EN                      BIT(0)

/* REG_AON_APB_MPLL_CFG3 */

#define BIT_AON_APB_MPLL_CCS_CTRL(x)                  (((x) & 0xFF))

/* REG_AON_APB_DPLL_CFG3 */

#define BIT_AON_APB_DPLL_CCS_CTRL(x)                  (((x) & 0xFF))

/* REG_AON_APB_CPLL_CFG3 */

#define BIT_AON_APB_CPLL_CCS_CTRL(x)                  (((x) & 0xFF))

/* REG_AON_APB_TWPLL_CFG3 */

#define BIT_AON_APB_TWPLL_CCS_CTRL(x)                 (((x) & 0xFF))

/* REG_AON_APB_GPLL_CFG3 */

#define BIT_AON_APB_GPLL_CCS_CTRL(x)                  (((x) & 0xFF))

/* REG_AON_APB_CP_DAP_PAD_CTRL */

#define BIT_AON_APB_CP_DAP_PAD_SEL(x)                 (((x) & 0x3))

/* REG_AON_APB_CA7_PROT_CTRL */

#define BIT_AON_APB_CA7_SPNIDEN(x)                    (((x) & 0xF) << 12)
#define BIT_AON_APB_CA7_SPIDEN(x)                     (((x) & 0xF) << 8)
#define BIT_AON_APB_CA7_NIDEN(x)                      (((x) & 0xF) << 4)
#define BIT_AON_APB_CA7_DBGEN(x)                      (((x) & 0xF))

/* REG_AON_APB_CSSYS_CFG */

#define BIT_AON_APB_DAP_DEVICEEN                      BIT(31)
#define BIT_AON_APB_DAP_DBGEN                         BIT(30)
#define BIT_AON_APB_DAP_SPIDBGEN                      BIT(29)
#define BIT_AON_APB_TG_JTAG_EN                        BIT(9)
#define BIT_AON_APB_CM4_DBGEN                         BIT(8)
#define BIT_AON_APB_CM4_JTAG_EN                       BIT(7)
#define BIT_AON_APB_DJTAG_EN                          BIT(6)
#define BIT_AON_APB_MJTAG_EN                          BIT(4)
#define BIT_AON_APB_CSSYS_NIDEN                       BIT(3)
#define BIT_AON_APB_CSSYS_SPNIDEN                     BIT(2)
#define BIT_AON_APB_CSSYS_SPIDEN                      BIT(1)
#define BIT_AON_APB_CSSYS_DBGEN                       BIT(0)

/* REG_AON_APB_SEC_MUX_DBG_EN */

#define BIT_AON_APB_DAP_DEVICEEN_S                    BIT(25)
#define BIT_AON_APB_DAP_DBGEN_S                       BIT(24)
#define BIT_AON_APB_DAP_SPIDBGEN_S                    BIT(23)
#define BIT_AON_APB_CR5_DBGEN_S                       BIT(16)
#define BIT_AON_APB_CR5_NIDEN_S                       BIT(15)
#define BIT_AON_APB_CSSYS_DBGEN_S                     BIT(14)
#define BIT_AON_APB_CSSYS_NIDEN_S                     BIT(13)
#define BIT_AON_APB_CSSYS_SPIDEN_S                    BIT(12)
#define BIT_AON_APB_CSSYS_SPNIDEN_S                   BIT(11)
#define BIT_AON_APB_CA7_DBGEN_S                       BIT(10)
#define BIT_AON_APB_CA7_NIDEN_S                       BIT(9)
#define BIT_AON_APB_CA7_SPIDEN_S                      BIT(8)
#define BIT_AON_APB_CA7_SPNIDEN_S                     BIT(7)
#define BIT_AON_APB_DJTAG_EN_S                        BIT(2)
#define BIT_AON_APB_CM4_DBGEN_S                       BIT(1)
#define BIT_AON_APB_MJTAG_EN_S                        BIT(0)

/* REG_AON_APB_CR5_PROT_CTRL */

#define BIT_AON_APB_CR5_NIDEN                         BIT(1)
#define BIT_AON_APB_CR5_DBGEN                         BIT(0)

/* REG_AON_APB_DBG_DJTAG_CTRL */

#define BIT_AON_APB_DBGSYS_CSSYS_STM_NSGUAREN         BIT(0)

/* REG_AON_APB_WTLCP_CTRL */

#define BIT_AON_APB_WTLCP_AON_FRC_WSYS_LT_STOP        BIT(4)
#define BIT_AON_APB_WTLCP_AON_FRC_WSYS_STOP           BIT(3)

/* REG_AON_APB_WTL_WCDMA_EB */

#define BIT_AON_APB_WTLCP_WCMDA_EB                    BIT(16)
#define BIT_AON_APB_WCDMA_AUTO_GATE_EN                BIT(8)

/* REG_AON_APB_WTLCP_TDSP_CTRL0 */

#define BIT_AON_APB_WTLCP_TDSP_BOOT_VECTOR(x)         (((x) & 0xFFFFFFFF))

/* REG_AON_APB_WTLCP_TDSP_CTRL1 */

#define BIT_AON_APB_WTLCP_STCK_TDSP                   BIT(13)
#define BIT_AON_APB_WTLCP_STMS_TDSP                   BIT(12)
#define BIT_AON_APB_WTLCP_STDO_TDSP                   BIT(11)
#define BIT_AON_APB_WTLCP_STDI_TDSP                   BIT(10)
#define BIT_AON_APB_WTLCP_STRTCK_TDSP                 BIT(9)
#define BIT_AON_APB_WTLCP_SW_JTAG_ENA_TDSP            BIT(8)
#define BIT_AON_APB_WTLCP_TDSP_EXTERNAL_WAIT          BIT(1)
#define BIT_AON_APB_WTLCP_TDSP_BOOT                   BIT(0)

/* REG_AON_APB_CP_CONFIG0 */

#define BIT_AON_APB_CP_AXI_VP_BASE(x)                 (((x) & 0xFFFFF) << 8)
#define BIT_AON_APB_CP_AXI_VP_SIZE(x)                 (((x) & 0x1F))

/* REG_AON_APB_CP_CONFIG1 */

#define BIT_AON_APB_CP_AXI_PP_BASE(x)                 (((x) & 0xFFFFF) << 8)
#define BIT_AON_APB_CP_AXI_PP_SIZE(x)                 (((x) & 0x1F))

/* REG_AON_APB_RF_CONFIG */

#define BIT_AON_APB_IDLE_ENABLE                       BIT(0)

/* REG_AON_APB_PCP_AON_EB */

#define BIT_AON_APB_PUBCP_SYST_RTC_EB                 BIT(11)
#define BIT_AON_APB_PUBCP_TMR_EB                      BIT(10)
#define BIT_AON_APB_PUBCP_TMR_RTC_EB                  BIT(9)
#define BIT_AON_APB_PUBCP_SYST_EB                     BIT(8)
#define BIT_AON_APB_PUBCP_WDG_EB                      BIT(7)
#define BIT_AON_APB_PUBCP_WDG_RTC_EB                  BIT(6)
#define BIT_AON_APB_PUBCP_ARCH_RTC_EB                 BIT(5)
#define BIT_AON_APB_PUBCP_EIC_EB                      BIT(4)
#define BIT_AON_APB_PUBCP_EIC_RTCDV5_EB               BIT(3)
#define BIT_AON_APB_PUBCP_EIC_RTC_EB                  BIT(2)

/* REG_AON_APB_PCP_SOFT_RST */

#define BIT_AON_APB_PUBCP_CR5_CORE_SOFT_RST           BIT(10)
#define BIT_AON_APB_PUBCP_CR5_DBG_SOFT_RST            BIT(9)
#define BIT_AON_APB_PUBCP_CR5_ETM_SOFT_RST            BIT(8)
#define BIT_AON_APB_PUBCP_CR5_MP_SOFT_RST             BIT(7)
#define BIT_AON_APB_PUBCP_CR5_CS_DBG_SOFT_RST         BIT(6)
#define BIT_AON_APB_PUBCP_TMR_SOFT_RST                BIT(5)
#define BIT_AON_APB_PUBCP_SYST_SOFT_RST               BIT(4)
#define BIT_AON_APB_PUBCP_WDG_SOFT_RST                BIT(3)
#define BIT_AON_APB_PUBCP_EIC_SOFT_RST                BIT(2)

/* REG_AON_APB_PUBCP_CTRL */

#define BIT_AON_APB_AON_ACCESS_PUBCP                  BIT(13)
#define BIT_AON_APB_PUBCP_CR5_STANDBYWFI_N            BIT(12)
#define BIT_AON_APB_PUBCP_CR5_STANDBYWFE_N            BIT(11)
#define BIT_AON_APB_PUBCP_CR5_CLKSTOPPED0_N           BIT(10)
#define BIT_AON_APB_PUBCP_CR5_L2IDLE                  BIT(9)
#define BIT_AON_APB_PUBCP_CR5_VALIRQ0_N               BIT(8)
#define BIT_AON_APB_PUBCP_CR5_VALFIQ0_N               BIT(7)
#define BIT_AON_APB_PUBCP_CR5_STOP                    BIT(6)
#define BIT_AON_APB_PUBCP_CR5_CSYSACK_ATB             BIT(5)
#define BIT_AON_APB_PUBCP_CR5_CACTIVE_ATB             BIT(4)
#define BIT_AON_APB_PUBCP_CR5_CSSYNC_REQ              BIT(3)
#define BIT_AON_APB_PUBCP_CR5_CSYSREQ_ATB             BIT(2)
#define BIT_AON_APB_PUBCP_CR5_NODBGCLK                BIT(1)
#define BIT_AON_APB_PUBCP_CR5_CFGEE                   BIT(0)

/* REG_AON_APB_SYS_DBG_SEL */

#define BIT_AON_APB_AP_DBG_SIG_SEL(x)                 (((x) & 0xFF) << 16)
#define BIT_AON_APB_CP_DBG_MOD_SEL(x)                 (((x) & 0xFF) << 8)

/* REG_AON_APB_SYS_DBG_SEL2 */

#define BIT_AON_APB_AON_DBG_MOD_SEL                   BIT(8)
#define BIT_AON_APB_AON_DBG_SIG_SEL(x)                (((x) & 0xFF))

/* REG_AON_APB_SUBSYS_DBG_CFG */

#define BIT_AON_APB_SUBSYS_DBG_SEL(x)                 (((x) & 0x7))

/* REG_AON_APB_GLB_WCDMA_CTRL */

#define BIT_AON_APB_WTLCP_WCDMA_AUTO_GATE_EN          BIT(3)
#define BIT_AON_APB_WTLCP_WCDMA_SOFT_GATE_DIS         BIT(2)
#define BIT_AON_APB_PUBCP_WCDMA_AUTO_GATE_EN          BIT(1)
#define BIT_AON_APB_PUBCP_WCDMA_SOFT_GATE_DIS         BIT(0)

/* REG_AON_APB_AON_MTX_EMC_LP_CTRL */

#define BIT_AON_APB_LP_FORCE_ACK_EMC                  BIT(19)
#define BIT_AON_APB_LP_FORCE_EMC                      BIT(18)
#define BIT_AON_APB_LP_STAT_EMC                       BIT(17)
#define BIT_AON_APB_LP_EB_EMC                         BIT(16)
#define BIT_AON_APB_LP_NUM_EMC(x)                     (((x) & 0xFFFF))

/* REG_AON_APB_AON_MTX_MAIN_LP_CTRL */

#define BIT_AON_APB_LP_STAT_MAIN                      BIT(17)
#define BIT_AON_APB_LP_EB_MAIN                        BIT(16)
#define BIT_AON_APB_LP_NUM_MAIN(x)                    (((x) & 0xFFFF))

/* REG_AON_APB_AON_MTX_SW0_LP_CTRL */

#define BIT_AON_APB_LP_STAT_SW0                       BIT(17)
#define BIT_AON_APB_LP_EB_SW0                         BIT(16)
#define BIT_AON_APB_LP_NUM_SW0(x)                     (((x) & 0xFFFF))

/* REG_AON_APB_AON_MTX_SW1_LP_CTRL */

#define BIT_AON_APB_LP_STAT_SW1                       BIT(17)
#define BIT_AON_APB_LP_EB_SW1                         BIT(16)
#define BIT_AON_APB_LP_NUM_SW1(x)                     (((x) & 0xFFFF))

/* REG_AON_APB_AON_MTX__RF_LP_CTRL */

#define BIT_AON_APB_LP_STAT_RF                        BIT(17)
#define BIT_AON_APB_LP_EB_RF                          BIT(16)
#define BIT_AON_APB_LP_NUM_RF(x)                      (((x) & 0xFFFF))

/* REG_AON_APB_AON_MTX_WCN_LP_CTRL */

#define BIT_AON_APB_LP_STAT_WCN                       BIT(17)
#define BIT_AON_APB_LP_EB_WCN                         BIT(16)
#define BIT_AON_APB_LP_NUM_WCN(x)                     (((x) & 0xFFFF))

/* REG_AON_APB_SUBSYS_LPC_FORCE_REQ */

#define BIT_AON_APB_WCN_FORCE_STOP_REQ                BIT(13)
#define BIT_AON_APB_CA7_FORCE_STOP_REQ                BIT(12)
#define BIT_AON_APB_PUBCP_ACC_FORCE_STOP_REQ          BIT(11)
#define BIT_AON_APB_WTLCP_DDR_FORCE_STOP_REQ          BIT(10)
#define BIT_AON_APB_PUBCP_DDR_FORCE_STOP_REQ          BIT(9)
#define BIT_AON_APB_CP_FORCE_STOP_REQ                 BIT(8)
#define BIT_AON_APB_WTLCP_AON_FORCE_STOP_REQ          BIT(7)
#define BIT_AON_APB_PUBCP_AON_FORCE_STOP_REQ          BIT(6)
#define BIT_AON_APB_GPU_FORCE_STOP_REQ                BIT(5)
#define BIT_AON_APB_MM_FORCE_STOP_REQ                 BIT(4)
#define BIT_AON_APB_AP_DISP_FORCE_STOP_REQ            BIT(3)
#define BIT_AON_APB_AP_IMC_FORCE_STOP_REQ             BIT(2)
#define BIT_AON_APB_AP_DAP_FORCE_STOP_REQ             BIT(1)
#define BIT_AON_APB_AP_GSP_FORCE_STOP_REQ             BIT(0)

/* REG_AON_APB_SUBSYS_LPC_FORCE_ACK */

#define BIT_AON_APB_WCN_FORCE_STOP_ACK                BIT(13)
#define BIT_AON_APB_CA7_FORCE_STOP_ACK                BIT(12)
#define BIT_AON_APB_PUBCP_ACC_FORCE_STOP_ACK          BIT(11)
#define BIT_AON_APB_WTLCP_DDR_FORCE_STOP_ACK          BIT(10)
#define BIT_AON_APB_PUBCP_DDR_FORCE_STOP_ACK          BIT(9)
#define BIT_AON_APB_CP_FORCE_STOP_ACK                 BIT(8)
#define BIT_AON_APB_WTLCP_AON_FORCE_STOP_ACK          BIT(7)
#define BIT_AON_APB_PUBCP_AON_FORCE_STOP_ACK          BIT(6)
#define BIT_AON_APB_GPU_FORCE_STOP_ACK                BIT(5)
#define BIT_AON_APB_MM_FORCE_STOP_ACK                 BIT(4)
#define BIT_AON_APB_AP_DISP_FORCE_STOP_ACK            BIT(3)
#define BIT_AON_APB_AP_IMC_FORCE_STOP_ACK             BIT(2)
#define BIT_AON_APB_AP_DAP_FORCE_STOP_ACK             BIT(1)
#define BIT_AON_APB_AP_GSP_FORCE_STOP_ACK             BIT(0)

/* REG_AON_APB_DAP_DJTAG_CTRL */

#define BIT_AON_APB_DAP_DJTAG_EN                      BIT(0)

/* REG_AON_APB_EFUSE_CONFIG0 */

#define BIT_AON_APB_S_EFS_BOUNDARY(x)                 (((x) & 0xFF))

/* REG_AON_APB_TOP_LPC0 */

#define BIT_AON_APB_TOP_LPC0_LP_FORCE_ACK             BIT(19)
#define BIT_AON_APB_TOP_LPC0_ACTIVE_SYNC_SEL          BIT(18)
#define BIT_AON_APB_TOP_LPC0_LP_REQ                   BIT(17)
#define BIT_AON_APB_TOP_LPC0_EB                       BIT(16)
#define BIT_AON_APB_TOP_LPC0_NUM(x)                   (((x) & 0xFFFF))

/* REG_AON_APB_TOP_LPC1 */

#define BIT_AON_APB_TOP_LPC1_LP_FORCE_ACK             BIT(19)
#define BIT_AON_APB_TOP_LPC1_ACTIVE_SYNC_SEL          BIT(18)
#define BIT_AON_APB_TOP_LPC1_LP_REQ                   BIT(17)
#define BIT_AON_APB_TOP_LPC1_EB                       BIT(16)
#define BIT_AON_APB_TOP_LPC1_NUM(x)                   (((x) & 0xFFFF))

/* REG_AON_APB_TOP_LPC2 */

#define BIT_AON_APB_TOP_LPC2_LP_FORCE_ACK             BIT(19)
#define BIT_AON_APB_TOP_LPC2_ACTIVE_SYNC_SEL          BIT(18)
#define BIT_AON_APB_TOP_LPC2_LP_REQ                   BIT(17)
#define BIT_AON_APB_TOP_LPC2_EB                       BIT(16)
#define BIT_AON_APB_TOP_LPC2_NUM(x)                   (((x) & 0xFFFF))

/* REG_AON_APB_TOP_LPC3 */

#define BIT_AON_APB_TOP_LPC3_LP_FORCE_ACK             BIT(19)
#define BIT_AON_APB_TOP_LPC3_ACTIVE_SYNC_SEL          BIT(18)
#define BIT_AON_APB_TOP_LPC3_LP_REQ                   BIT(17)
#define BIT_AON_APB_TOP_LPC3_EB                       BIT(16)
#define BIT_AON_APB_TOP_LPC3_NUM(x)                   (((x) & 0xFFFF))

/* REG_AON_APB_OVERHEAT_RST_CTRL */

#define BIT_AON_APB_OVERHEAT_RST_DDIE_EN              BIT(0)

/* REG_AON_APB_TOP_LPC4 */

#define BIT_AON_APB_TOP_LPC4_LP_FORCE_ACK             BIT(19)
#define BIT_AON_APB_TOP_LPC4_ACTIVE_SYNC_SEL          BIT(18)
#define BIT_AON_APB_TOP_LPC4_LP_REQ                   BIT(17)
#define BIT_AON_APB_TOP_LPC4_EB                       BIT(16)
#define BIT_AON_APB_TOP_LPC4_NUM(x)                   (((x) & 0xFFFF))

/* REG_AON_APB_TOP_LPC5 */

#define BIT_AON_APB_TOP_LPC5_LP_FORCE_ACK             BIT(19)
#define BIT_AON_APB_TOP_LPC5_ACTIVE_SYNC_SEL          BIT(18)
#define BIT_AON_APB_TOP_LPC5_LP_REQ                   BIT(17)
#define BIT_AON_APB_TOP_LPC5_EB                       BIT(16)
#define BIT_AON_APB_TOP_LPC5_NUM(x)                   (((x) & 0xFFFF))

/* REG_AON_APB_DDR_SLEEP_CTRL0 */

#define BIT_AON_APB_CONF_DMC_STOP_CH(x)               (((x) & 0x7F) << 16)
#define BIT_AON_APB_CONF_DMC_SLEEP_CH(x)              (((x) & 0x7F) << 8)
#define BIT_AON_APB_CONF_MODE_SEL                     BIT(5)
#define BIT_AON_APB_CONF_DMC_SLEEP                    BIT(4)
#define BIT_AON_APB_CONF_DMC_STOP                     BIT(3)

/* REG_AON_APB_DDR_SLEEP_CTRL1 */

#define BIT_AON_APB_CONF_SUBSYS_HW_EN(x)              (((x) & 0x1FF) << 16)
#define BIT_AON_APB_CONF_WAIT_NUM(x)                  (((x) & 0xFFFF))

/* REG_AON_APB_DDR_SLEEP_CTRL2 */

#define BIT_AON_APB_DDR_SLP_CTRL_DEEP_SLEEP_FORCE_EN  BIT(16)
#define BIT_AON_APB_DDR_SLP_CTRL_FSM_STATE(x)         (((x) & 0xF) << 12)
#define BIT_AON_APB_DDR_SLP_CTRL_STAT_INTEN(x)        (((x) & 0x7) << 8)
#define BIT_AON_APB_DDR_SLP_CTRL_STAT_CLR(x)          (((x) & 0x7) << 4)
#define BIT_AON_APB_DDR_SLP_CTRL_STAT(x)              (((x) & 0x7))

/* REG_AON_APB_DDR_SLEEP_CTRL3 */

#define BIT_AON_APB_CONF_DDR_SLP_REQ(x)               (((x) & 0x3FF) << 16)
#define BIT_AON_APB_CONF_DDR_SLP_ACK(x)               (((x) & 0x1FF))

/* REG_AON_APB_ADI_CONFIG0 */

#define BIT_AON_APB_AP_EMMC_POWER_OFF_EN              BIT(1)
#define BIT_AON_APB_AP_EMMC_POWER_OFF_CTRL_BY_HW      BIT(0)

/* REG_AON_APB_AP_WPROT_EN1 */

#define BIT_AON_APB_AP_AWADDR_WPROT_EN1(x)            (((x) & 0xFFFFFFFF))

/* REG_AON_APB_WTLCP_WPROT_EN1 */

#define BIT_AON_APB_WTLCP_AWADDR_WPROT_EN1(x)         (((x) & 0xFFFFFFFF))

/* REG_AON_APB_PUBCP_WPROT_EN1 */

#define BIT_AON_APB_PUBCP_AWADDR_WPROT_EN1(x)         (((x) & 0xFFFFFFFF))

/* REG_AON_APB_IO_DLY_CTRL */


/* REG_AON_APB_AP_WPROT_EN0 */

#define BIT_AON_APB_AP_AWADDR_WPROT_EN0(x)            (((x) & 0xFFFFFFFF))

/* REG_AON_APB_WTLCP_WPROT_EN0 */

#define BIT_AON_APB_WTLCP_AWADDR_WPROT_EN0(x)         (((x) & 0xFFFFFFFF))

/* REG_AON_APB_PUBCP_WPROT_EN0 */

#define BIT_AON_APB_PUBCP_AWADDR_WPROT_EN0(x)         (((x) & 0xFFFFFFFF))

/* REG_AON_APB_PMU_RST_MONITOR */

#define BIT_AON_APB_PMU_RST_MONITOR(x)                (((x) & 0xFFFFFFFF))

/* REG_AON_APB_THM_RST_MONITOR */

#define BIT_AON_APB_THM_RST_MONITOR(x)                (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AP_RST_MONITOR */

#define BIT_AON_APB_AP_RST_MONITOR(x)                 (((x) & 0xFFFFFFFF))

/* REG_AON_APB_CA7_RST_MONITOR */

#define BIT_AON_APB_CA7_RST_MONITOR(x)                (((x) & 0xFFFFFFFF))

/* REG_AON_APB_BOND_OPT0 */

#define BIT_AON_APB_BOND_OPTION0(x)                   (((x) & 0xFFFFFFFF))

/* REG_AON_APB_BOND_OPT1 */

#define BIT_AON_APB_BOND_OPTION1(x)                   (((x) & 0xFFFFFFFF))

/* REG_AON_APB_RES_REG0 */

#define BIT_AON_APB_RES_REG0(x)                       (((x) & 0xFFFFFFFF))

/* REG_AON_APB_RES_REG1 */

#define BIT_AON_APB_RES_REG1(x)                       (((x) & 0xFFFFFFFF))

/* REG_AON_APB_PLL_LOCK_OUT_SEL */

#define BIT_AON_APB_SLEEP_PLLLOCK_SEL                 BIT(7)
#define BIT_AON_APB_PLL_LOCK_SEL(x)                   (((x) & 0x7) << 4)
#define BIT_AON_APB_SLEEP_DBG_SEL(x)                  (((x) & 0xF))

/* REG_AON_APB_WDG_RST_FLAG */

#define BIT_AON_APB_PCP_WDG_RST_FLAG                  BIT(5)
#define BIT_AON_APB_WTLCP_TG_WDG_RST_FLAG             BIT(3)
#define BIT_AON_APB_AP_WDG_RST_FLAG                   BIT(2)
#define BIT_AON_APB_CA7_WDG_RST_FLAG                  BIT(1)
#define BIT_AON_APB_SEC_WDG_RST_FLAG                  BIT(0)

/* REG_AON_APB_CA7_CFG */

#define BIT_AON_APB_READ_ALLOC_MODE_SPRD(x)           (((x) & 0xF))

/* REG_AON_APB_RES_REG2 */

#define BIT_AON_APB_RES_REG2(x)                       (((x) & 0xFFFFFFFF))

/* REG_AON_APB_RES_REG3 */

#define BIT_AON_APB_RES_REG3(x)                       (((x) & 0xFFFFFFFF))

/* REG_AON_APB_RES_REG4 */

#define BIT_AON_APB_RES_REG4(x)                       (((x) & 0xFFFFFFFF))

/* REG_AON_APB_RES_REG5 */

#define BIT_AON_APB_RES_REG5(x)                       (((x) & 0xFFFFFFFF))

/* REG_AON_APB_RES_REG6 */

#define BIT_AON_APB_RES_REG6(x)                       (((x) & 0xFFFFFFFF))

/* REG_AON_APB_RES_REG7 */

#define BIT_AON_APB_RES_REG7(x)                       (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AON_APB_RSV */

#define BIT_AON_APB_AON_APB_RSV(x)                    (((x) & 0xFFFFFFFF))

/* REG_AON_APB_FUNCTION_DMA_BOOT_ADDR */

#define BIT_AON_APB_FUNCTION_DMA_BOOT_ADDR(x)         (((x) & 0xFFFFFFFF))

/* REG_AON_APB_SIM_HOT_PLUG_CTRL_PUBCP_SIM0 */

#define BIT_AON_APB_SIM_OFF_PD_EN_PUBCP_SIM0          BIT(5)
#define BIT_AON_APB_BAT_DETECT_POL_PUBCP_SIM0         BIT(4)
#define BIT_AON_APB_SIM_DETECT_POL_PUBCP_SIM0         BIT(3)
#define BIT_AON_APB_BAT_DETECT_EN_PUBCP_SIM0          BIT(2)
#define BIT_AON_APB_SIM_DETECT_EN_PUBCP_SIM0          BIT(1)
#define BIT_AON_APB_SIM_CLK_PL_PUBCP_SIM0             BIT(0)

/* REG_AON_APB_SIM_HOT_PLUG_CTRL_PUBCP_SIM1 */

#define BIT_AON_APB_SIM_OFF_PD_EN_PUBCP_SIM1          BIT(5)
#define BIT_AON_APB_BAT_DETECT_POL_PUBCP_SIM1         BIT(4)
#define BIT_AON_APB_SIM_DETECT_POL_PUBCP_SIM1         BIT(3)
#define BIT_AON_APB_BAT_DETECT_EN_PUBCP_SIM1          BIT(2)
#define BIT_AON_APB_SIM_DETECT_EN_PUBCP_SIM1          BIT(1)
#define BIT_AON_APB_SIM_CLK_PL_PUBCP_SIM1             BIT(0)

/* REG_AON_APB_SIM_HOT_PLUG_CTRL_PUBCP_SIM2 */

#define BIT_AON_APB_SIM_OFF_PD_EN_PUBCP_SIM2          BIT(5)
#define BIT_AON_APB_BAT_DETECT_POL_PUBCP_SIM2         BIT(4)
#define BIT_AON_APB_SIM_DETECT_POL_PUBCP_SIM2         BIT(3)
#define BIT_AON_APB_BAT_DETECT_EN_PUBCP_SIM2          BIT(2)
#define BIT_AON_APB_SIM_DETECT_EN_PUBCP_SIM2          BIT(1)
#define BIT_AON_APB_SIM_CLK_PL_PUBCP_SIM2             BIT(0)

/* REG_AON_APB_SIM_HOT_PLUG_CTRL_AP_SIM0 */

#define BIT_AON_APB_SIM_OFF_PD_EN_AP_SIM0             BIT(5)
#define BIT_AON_APB_BAT_DETECT_POL_AP_SIM0            BIT(4)
#define BIT_AON_APB_SIM_DETECT_POL_AP_SIM0            BIT(3)
#define BIT_AON_APB_BAT_DETECT_EN_AP_SIM0             BIT(2)
#define BIT_AON_APB_SIM_DETECT_EN_AP_SIM0             BIT(1)
#define BIT_AON_APB_SIM_CLK_PL_AP_SIM0                BIT(0)

/* REG_AON_APB_DEEP_SLEEP_SOFT */

#define BIT_AON_APB_MM_DEEP_SLEEP_SOFT                BIT(1)
#define BIT_AON_APB_GPU_DEEP_SLEEP_SOFT               BIT(0)


#endif /* AON_APB_H */

