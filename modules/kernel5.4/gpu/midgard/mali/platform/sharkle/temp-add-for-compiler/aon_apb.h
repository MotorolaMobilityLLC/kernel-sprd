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


#ifndef AON_APB_H
#define AON_APB_H



#define REG_AON_APB_APB_EB0                     (0x0000)
#define REG_AON_APB_APB_EB1                     (0x0004)
#define REG_AON_APB_APB_RST0                    (0x0008)
#define REG_AON_APB_APB_RST1                    (0x000C)
#define REG_AON_APB_APB_RTC_EB                  (0x0010)
#define REG_AON_APB_REC_26MHZ_BUF_CFG           (0x0014)
#define REG_AON_APB_SINDRV_CTRL                 (0x0018)
#define REG_AON_APB_ADA_SEL_CTRL                (0x001C)
#define REG_AON_APB_VBC_CTRL                    (0x0020)
#define REG_AON_APB_PWR_CTRL                    (0x0024)
#define REG_AON_APB_TS_CFG                      (0x0028)
#define REG_AON_APB_BOOT_MODE                   (0x002C)
#define REG_AON_APB_BB_BG_CTRL                  (0x0030)
#define REG_AON_APB_CP_ARM_JTAG_CTRL            (0x0034)
#define REG_AON_APB_PLL_SOFT_CNT_DONE           (0x0038)
#define REG_AON_APB_DCXO_LC_REG0                (0x003C)
#define REG_AON_APB_DCXO_LC_REG1                (0x0040)
#define REG_AON_APB_MPLL_CFG1                   (0x0044)
#define REG_AON_APB_MPLL_CFG2                   (0x0048)
#define REG_AON_APB_DPLL_CFG1                   (0x004C)
#define REG_AON_APB_DPLL_CFG2                   (0x0050)
#define REG_AON_APB_TWPLL_CFG1                  (0x0054)
#define REG_AON_APB_TWPLL_CFG2                  (0x0058)
#define REG_AON_APB_LTEPLL_CFG1                 (0x005C)
#define REG_AON_APB_LTEPLL_CFG2                 (0x0060)
#define REG_AON_APB_LVDSRFPLL_CFG1              (0x0064)
#define REG_AON_APB_LVDSRFPLL_CFG2              (0x0068)
#define REG_AON_APB_AON_REG_PROT                (0x006C)
#define REG_AON_APB_DSI_PHY_CTRL                (0x0070)
#define REG_AON_APB_CSI_2L_PHY_CTRL             (0x0074)
#define REG_AON_APB_CSI_2P2L_S_PHY_CTRL         (0x0078)
#define REG_AON_APB_CSI_2P2L_M_PHY_CTRL         (0x007C)
#define REG_AON_APB_CSI_2P2L_DBG_PHY_CTRL       (0x0080)
#define REG_AON_APB_CSI_2P2L_PHY_CTRL           (0x0084)
#define REG_AON_APB_AON_CGM_CFG                 (0x0088)
#define REG_AON_APB_DFS_CLK_GATE_CFG            (0x008C)
#define REG_AON_APB_HARD_DFS_CTRL               (0x00AC)
#define REG_AON_APB_APB_EB2                     (0x00B0)
#define REG_AON_APB_ROSC_CFG                    (0x00C0)
#define REG_AON_APB_ROSC_STATUS                 (0x00C4)
#define REG_AON_APB_AON_ANALOG_RSV              (0x00C8)
#define REG_AON_APB_AON_CHIP_ID0                (0x00E0)
#define REG_AON_APB_AON_CHIP_ID1                (0x00E4)
#define REG_AON_APB_AON_PLAT_ID0                (0x00E8)
#define REG_AON_APB_AON_PLAT_ID1                (0x00EC)
#define REG_AON_APB_AON_IMPL_ID                 (0x00F0)
#define REG_AON_APB_AON_MFT_ID                  (0x00F4)
#define REG_AON_APB_AON_VER_ID                  (0x00F8)
#define REG_AON_APB_AON_CHIP_ID                 (0x00FC)
#define REG_AON_APB_CCIR_RCVR_CFG               (0x0100)
#define REG_AON_APB_PLL_BG_CFG                  (0x0108)
#define REG_AON_APB_LVDSDIS_SEL                 (0x010C)
#define REG_AON_APB_DJTAG_MUX_SEL               (0x0110)
#define REG_AON_APB_CM4_SYS_SOFT_RST            (0x0114)
#define REG_AON_APB_PUBCP_WTLCP_ADDR_MSB        (0x0118)
#define REG_AON_APB_AON_DMA_INT_EN              (0x011C)
#define REG_AON_APB_EMC_AUTO_GATE_EN            (0x0120)
#define REG_AON_APB_CM4_CFG_BUS                 (0x0124)
#define REG_AON_APB_APB_RST2                    (0x0130)
#define REG_AON_APB_CLK_EB0                     (0x0134)
#define REG_AON_APB_AON_SDIO                    (0x0138)
#define REG_AON_APB_MPLL_CTRL                   (0x013C)
#define REG_AON_APB_AUTO_GATE_CTRL0             (0x0140)
#define REG_AON_APB_AUTO_GATE_CTRL1             (0x0144)
#define REG_AON_APB_GPLL_CFG1                   (0x0150)
#define REG_AON_APB_GPLL_CFG2                   (0x0154)
#define REG_AON_APB_RPLL_CFG1                   (0x0158)
#define REG_AON_APB_RPLL_CFG2                   (0x015C)
#define REG_AON_APB_RPLL_CFG3                   (0x0160)
#define REG_AON_APB_THM0_CTRL                   (0x0164)
#define REG_AON_APB_THM1_CTRL                   (0x0168)
#define REG_AON_APB_BUSMON_DMA_CFG              (0x0170)
#define REG_AON_APB_ANALOG_CFG0                 (0x0174)
#define REG_AON_APB_ANALOG_CFG1                 (0x0178)
#define REG_AON_APB_RPLL_BIST_CTRL              (0x017C)
#define REG_AON_APB_MPLL_BIST_CTRL              (0x0180)
#define REG_AON_APB_DPLL_BIST_CTRL              (0x0184)
#define REG_AON_APB_GPLL_BIST_CTRL              (0x0188)
#define REG_AON_APB_TWPLL_BIST_CTRL             (0x018C)
#define REG_AON_APB_LPLL_BIST_CTRL              (0x0190)
#define REG_AON_APB_DPLL_CTRL                   (0x0194)
#define REG_AON_APB_CPPLL_CFG1                  (0x0198)
#define REG_AON_APB_CPPLL_CFG2                  (0x019C)
#define REG_AON_APB_CPPLL_BIST_CTRL             (0x01A0)
#define REG_AON_APB_MPLL_CFG3                   (0x01B0)
#define REG_AON_APB_DPLL_CFG3                   (0x01B4)
#define REG_AON_APB_GPLL_CFG3                   (0x01B8)
#define REG_AON_APB_CP_DAP_PAD_CTRL             (0x0200)
#define REG_AON_APB_CA53_PROT_CTRL              (0x0204)
#define REG_AON_APB_CSSYS_CFG                   (0x0208)
#define REG_AON_APB_SEC_MUX_DBG_EN              (0x020C)
#define REG_AON_APB_CR5_PROT_CTRL               (0x0210)
#define REG_AON_APB_DBG_DJTAG_CTRL              (0x0214)
#define REG_AON_APB_WTLCP_CTRL                  (0x0240)
#define REG_AON_APB_WTL_WCDMA_EB                (0x0244)
#define REG_AON_APB_WTLCP_LDSP_CTRL0            (0x0248)
#define REG_AON_APB_WTLCP_LDSP_CTRL1            (0x024C)
#define REG_AON_APB_WTLCP_TDSP_CTRL0            (0x0250)
#define REG_AON_APB_WTLCP_TDSP_CTRL1            (0x0254)
#define REG_AON_APB_PCP_AON_EB                  (0x0280)
#define REG_AON_APB_PCP_SOFT_RST                (0x0284)
#define REG_AON_APB_PUBCP_CTRL                  (0x0288)
#define REG_AON_APB_SYS_DBG_SEL                 (0x02B0)
#define REG_AON_APB_MDAR_HSDL_CFG               (0x02B4)
#define REG_AON_APB_SYS_DBG_SEL2                (0x02B8)
#define REG_AON_APB_SUBSYS_DBG_CFG              (0x02BC)
#define REG_AON_APB_AP_LPC_CTRL                 (0x02C0)
#define REG_AON_APB_WTLCP_LPC_CTRL              (0x02C4)
#define REG_AON_APB_PUBCP_LPC_CTRL              (0x02C8)
#define REG_AON_APB_RFTI_SOFT_RST               (0x02D0)
#define REG_AON_APB_GLB_WCDMA_CTRL              (0x0300)
#define REG_AON_APB_PLL_CLKOUT_GATE             (0x0400)
#define REG_AON_APB_HM_CFG_SEL                  (0x0404)
#define REG_AON_APB_HM_PWR_CTRL                 (0x0408)
#define REG_AON_APB_HM_RST_CTRL                 (0x040C)
#define REG_AON_APB_M_AAPC_CFG                  (0x0420)
#define REG_AON_APB_DAP_DJTAG_CTRL              (0x0430)
#define REG_AON_APB_CGM_REG1                    (0x0440)
#define REG_AON_APB_CM4_INT_REQ_SRC_EN          (0x0448)
#define REG_AON_APB_CM4_INT_REQ_SRC_EN1         (0x044C)
#define REG_AON_APB_AON_MTX_MAIN_LPC_CTRL       (0x0450)
#define REG_AON_APB_AON_MTX_M0_LPC_CTRL         (0x0454)
#define REG_AON_APB_AON_MTX_M1_LPC_CTRL         (0x0458)
#define REG_AON_APB_AON_MTX_M2_LPC_CTRL         (0x045C)
#define REG_AON_APB_AON_MTX_M3_LPC_CTRL         (0x0460)
#define REG_AON_APB_AON_MTX_M4_LPC_CTRL         (0x0464)
#define REG_AON_APB_AON_MTX_M5_LPC_CTRL         (0x0468)
#define REG_AON_APB_AON_MTX_S0_LPC_CTRL         (0x046C)
#define REG_AON_APB_AON_MTX_S1_LPC_CTRL         (0x0470)
#define REG_AON_APB_AON_MTX_S2_LPC_CTRL         (0x0474)
#define REG_AON_APB_AON_MTX_S3_LPC_CTRL         (0x0478)
#define REG_AON_APB_AON_MTX_S4_LPC_CTRL         (0x047C)
#define REG_AON_APB_AON_MTX_S5_LPC_CTRL         (0x0480)
#define REG_AON_APB_AON_MTX_S6_LPC_CTRL         (0x0484)
#define REG_AON_APB_AON_MTX_S7_LPC_CTRL         (0x0488)
#define REG_AON_APB_GPU2DDR_LPC_CTRL            (0x048C)
#define REG_AON_APB_WTLCP2DDR_LPC_CTRL          (0x0490)
#define REG_AON_APB_WCN2DDR_LPC_CTRL            (0x0494)
#define REG_AON_APB_MM2DDR_LPC_CTRL             (0x0498)
#define REG_AON_APB_AON2DDR_LPC_CTRL            (0x049C)
#define REG_AON_APB_CM42AON_LPC_CTRL            (0x04A0)
#define REG_AON_APB_OVERHEAT_CTRL               (0x0510)
#define REG_AON_APB_SKLE_TMP0                   (0x0514)
#define REG_AON_APB_SKLE_TMP1                   (0x0518)
#define REG_AON_APB_SKLE_TMP2                   (0x051C)
#define REG_AON_APB_SKLE_TMP3                   (0x0520)
#define REG_AON_APB_DBG_PWRUP_SEL               (0x0524)
#define REG_AON_APB_RVBAADDR0                   (0x0528)
#define REG_AON_APB_RVBAADDR1                   (0x052C)
#define REG_AON_APB_RVBAADDR2                   (0x0530)
#define REG_AON_APB_RVBAADDR3                   (0x0534)
#define REG_AON_APB_AA64N32                     (0x0538)
#define REG_AON_APB_CA53_EDBGRQ                 (0x053C)
#define REG_AON_APB_PUBCP_SIM1_TOP_CTRL         (0x0540)
#define REG_AON_APB_PUBCP_SIM2_TOP_CTRL         (0x0544)
#define REG_AON_APB_PUBCP_SIM3_TOP_CTRL         (0x0548)
#define REG_AON_APB_AP_SIM_TOP_CTRL             (0x054C)
#define REG_AON_APB_SYS_DEBUG_BUS_SEL_CFG       (0x0550)
#define REG_AON_APB_SYS_DEBUG_BUS_SEL_CFG1      (0x0554)
#define REG_AON_APB_SYS_DEBUG_BUS_SEL_CFG2      (0x0558)
#define REG_AON_APB_PAD_DBG_BUS_SEL_CFG1        (0x055C)
#define REG_AON_APB_PAD_DBG_BUS_SEL_CFG2        (0x0560)
#define REG_AON_APB_PAD_DBG_BUS_SEL_CFG3        (0x0564)
#define REG_AON_APB_BONDING_OPTION              (0x0568)
#define REG_AON_APB_CE_LIFE_CYCLE               (0x056C)
#define REG_AON_APB_WCN_ADC_CLK_SEL             (0x0570)
#define REG_AON_APB_CHIP_TOP_PLL_CNT_DONE       (0x0574)
#define REG_AON_APB_WCN_SYS_CFG1                (0x0578)
#define REG_AON_APB_WCN_SYS_CFG2                (0x057C)
#define REG_AON_APB_AP_CORE_CFG                 (0x0580)
#define REG_AON_APB_CM4_MPU_DISABLE             (0x0584)
#define REG_AON_APB_CM4_STATUS                  (0x0588)
#define REG_AON_APB_MBIST_EFUSE_CTRL            (0x058C)
#define REG_AON_APB_AON_APB_FREQ_CTRL           (0x0590)
#define REG_AON_APB_CA53_PROT_CTRL_NON_SEC      (0x0594)
#define REG_AON_APB_CSSYS_CFG_NON_SEC           (0x0598)
#define REG_AON_APB_CR5_PROT_CTRL_NON_SEC       (0x059C)
#define REG_AON_APB_PAD_DBG_BUS_SEL_CFG4        (0x05A0)
#define REG_AON_APB_PAD_DBG_BUS_SEL_CFG5        (0x05A4)
#define REG_AON_APB_PAD_DBG_BUS_SEL_CFG6        (0x05A8)
#define REG_AON_APB_INT_REQ_PWR_UP_FLAG         (0x05AC)
#define REG_AON_APB_INT_REQ_PWR_DOWN_FLAG       (0x05B0)
#define REG_AON_APB_IO_DLY_CTRL                 (0x3014)
#define REG_AON_APB_PMU_RST_MONITOR             (0x302C)
#define REG_AON_APB_THM_RST_MONITOR             (0x3030)
#define REG_AON_APB_AP_RST_MONITOR              (0x3034)
#define REG_AON_APB_CA53_RST_MONITOR            (0x3038)
#define REG_AON_APB_BOND_OPT0                   (0x303C)
#define REG_AON_APB_RES_REG0                    (0x3044)
#define REG_AON_APB_RES_REG1                    (0x3048)
#define REG_AON_APB_AON_QOS_CFG                 (0x304C)
#define REG_AON_APB_BB_LDO_CAL_START            (0x3050)
#define REG_AON_APB_AON_MTX_PROT_CFG            (0x3058)
#define REG_AON_APB_LVDS_CFG                    (0x3060)
#define REG_AON_APB_PLL_LOCK_OUT_SEL            (0x3064)
#define REG_AON_APB_FUNCTST_CTRL_0              (0x3070)
#define REG_AON_APB_FUNCTST_CTRL_1              (0x3074)
#define REG_AON_APB_FUNCTST_CTRL_2              (0x3078)
#define REG_AON_APB_WDG_RST_FLAG                (0x3080)
#define REG_AON_APB_CA53_CFG                    (0x3084)
#define REG_AON_APB_RES_REG2                    (0x3090)
#define REG_AON_APB_RES_REG3                    (0x3094)
#define REG_AON_APB_RES_REG4                    (0x3098)
#define REG_AON_APB_RES_REG5                    (0x309C)
#define REG_AON_APB_RES_REG6                    (0x30A0)
#define REG_AON_APB_RES_REG7                    (0x30A4)
#define REG_AON_APB_AON_APB_RSV                 (0x30F0)
#define REG_AON_APB_FUNCTION_DMA_BOOT_ADDR      (0x3110)

/* REG_AON_APB_APB_EB0 */

#define BIT_AON_APB_CA53_DAP_EB                                  BIT(31)
#define BIT_AON_APB_PUBCP_INTC_EB                                BIT(30)
#define BIT_AON_APB_WTLCP_INTC_EB                                BIT(29)
#define BIT_AON_APB_CA53_TS0_EB                                  BIT(28)
#define BIT_AON_APB_GPU_EB                                       BIT(27)
#define BIT_AON_APB_CKG_EB                                       BIT(26)
#define BIT_AON_APB_MM_EB                                        BIT(25)
#define BIT_AON_APB_AP_WDG_EB                                    BIT(24)
#define BIT_AON_APB_SPLK_EB                                      BIT(22)
#define BIT_AON_APB_IPI_EB                                       BIT(21)
#define BIT_AON_APB_PIN_EB                                       BIT(20)
#define BIT_AON_APB_VBC_EB                                       BIT(19)
#define BIT_AON_APB_AUD_EB                                       BIT(18)
#define BIT_AON_APB_AUDIF_EB                                     BIT(17)
#define BIT_AON_APB_ADI_EB                                       BIT(16)
#define BIT_AON_APB_AON_INTC_EB                                  BIT(15)
#define BIT_AON_APB_EIC_EB                                       BIT(14)
#define BIT_AON_APB_EFUSE_NORMAL_EB                              BIT(13)
#define BIT_AON_APB_AP_TMR0_EB                                   BIT(12)
#define BIT_AON_APB_AON_TMR_EB                                   BIT(11)
#define BIT_AON_APB_AP_SYST_EB                                   BIT(10)
#define BIT_AON_APB_AON_SYST_EB                                  BIT(9)
#define BIT_AON_APB_KPD_EB                                       BIT(8)
#define BIT_AON_APB_PWM3_EB                                      BIT(7)
#define BIT_AON_APB_PWM2_EB                                      BIT(6)
#define BIT_AON_APB_PWM1_EB                                      BIT(5)
#define BIT_AON_APB_PWM0_EB                                      BIT(4)
#define BIT_AON_APB_GPIO_EB                                      BIT(3)
#define BIT_AON_APB_TPC_EB                                       BIT(2)
#define BIT_AON_APB_FM_EB                                        BIT(1)
#define BIT_AON_APB_ADC_EB                                       BIT(0)

/* REG_AON_APB_APB_EB1 */

#define BIT_AON_APB_SERDES_DPHY_EB                               BIT(31)
#define BIT_AON_APB_CROSS_TRIG_EB                                BIT(30)
#define BIT_AON_APB_DBG_EMC_EB                                   BIT(29)
#define BIT_AON_APB_DBG_EB                                       BIT(28)
#define BIT_AON_APB_ORP_JTAG_EB                                  BIT(27)
#define BIT_AON_APB_TGDSP_INTC_EB                                BIT(26)
#define BIT_AON_APB_LDSP_INTC_EB                                 BIT(25)
#define BIT_AON_APB_LVDS_PLL_DIV_EN                              BIT(24)
#define BIT_AON_APB_CM4_JTAG_EB                                  BIT(23)
#define BIT_AON_APB_AON_DMA_NORMAL_EB                            BIT(22)
#define BIT_AON_APB_MBOX_EB                                      BIT(21)
#define BIT_AON_APB_DJTAG_EB                                     BIT(20)
#define BIT_AON_APB_THM1_EB                                      BIT(19)
#define BIT_AON_APB_AON_FOR_AP_INTC_EB                           BIT(18)
#define BIT_AON_APB_MDAR_EB                                      BIT(17)
#define BIT_AON_APB_LVDS_TCXO_EB                                 BIT(16)
#define BIT_AON_APB_LVDS_TRX_EB                                  BIT(15)
#define BIT_AON_APB_MM_VSP_EB                                    BIT(14)
#define BIT_AON_APB_GSP_EMC_EB                                   BIT(13)
#define BIT_AON_APB_ZIP_EMC_EB                                   BIT(12)
#define BIT_AON_APB_DISP_EMC_EB                                  BIT(11)
#define BIT_AON_APB_AP_TMR2_EB                                   BIT(10)
#define BIT_AON_APB_AP_TMR1_EB                                   BIT(9)
#define BIT_AON_APB_CA53_WDG_EB                                  BIT(8)
#define BIT_AON_APB_CLK_EMC_REF_EB                               BIT(7)
#define BIT_AON_APB_CM4_APB_SOFT_EB                              BIT(6)
#define BIT_AON_APB_PROBE_EB                                     BIT(5)
#define BIT_AON_APB_AUX2_EB                                      BIT(4)
#define BIT_AON_APB_AUX1_EB                                      BIT(3)
#define BIT_AON_APB_AUX0_EB                                      BIT(2)
#define BIT_AON_APB_THM0_EB                                      BIT(1)
#define BIT_AON_APB_PMU_EB                                       BIT(0)

/* REG_AON_APB_APB_RST0 */

#define BIT_AON_APB_AON_FOR_AP_INTC_SOFT_RST                     BIT(31)
#define BIT_AON_APB_TGDSP_INTC_SOFT_RST                          BIT(30)
#define BIT_AON_APB_LDSP_INTC_SOFT_RST                           BIT(29)
#define BIT_AON_APB_CA53_TS0_SOFT_RST                            BIT(28)
#define BIT_AON_APB_DAP_MTX_SOFT_RST                             BIT(27)
#define BIT_AON_APB_PUBCP_INTC_SOFT_RST                          BIT(26)
#define BIT_AON_APB_WTLCP_INTC_SOFT_RST                          BIT(25)
#define BIT_AON_APB_SPLK_SOFT_RST                                BIT(24)
#define BIT_AON_APB_IPI_SOFT_RST                                 BIT(23)
#define BIT_AON_APB_CKG_SOFT_RST                                 BIT(22)
#define BIT_AON_APB_PIN_SOFT_RST                                 BIT(21)
#define BIT_AON_APB_VBC_SOFT_RST                                 BIT(20)
#define BIT_AON_APB_AUD_SOFT_RST                                 BIT(19)
#define BIT_AON_APB_AUDIF_SOFT_RST                               BIT(18)
#define BIT_AON_APB_ADI_SOFT_RST                                 BIT(17)
#define BIT_AON_APB_AON_INTC_SOFT_RST                            BIT(16)
#define BIT_AON_APB_EIC_SOFT_RST                                 BIT(15)
#define BIT_AON_APB_EFUSE_SOFT_RST                               BIT(14)
#define BIT_AON_APB_AP_WDG_SOFT_RST                              BIT(13)
#define BIT_AON_APB_AP_TMR0_SOFT_RST                             BIT(12)
#define BIT_AON_APB_AON_TMR_SOFT_RST                             BIT(11)
#define BIT_AON_APB_AP_SYST_SOFT_RST                             BIT(10)
#define BIT_AON_APB_AON_SYST_SOFT_RST                            BIT(9)
#define BIT_AON_APB_KPD_SOFT_RST                                 BIT(8)
#define BIT_AON_APB_PWM3_SOFT_RST                                BIT(7)
#define BIT_AON_APB_PWM2_SOFT_RST                                BIT(6)
#define BIT_AON_APB_PWM1_SOFT_RST                                BIT(5)
#define BIT_AON_APB_PWM0_SOFT_RST                                BIT(4)
#define BIT_AON_APB_GPIO_SOFT_RST                                BIT(3)
#define BIT_AON_APB_TPC_SOFT_RST                                 BIT(2)
#define BIT_AON_APB_FM_SOFT_RST                                  BIT(1)
#define BIT_AON_APB_ADC_SOFT_RST                                 BIT(0)

/* REG_AON_APB_APB_RST1 */

#define BIT_AON_APB_ADC3_SOFT_RST                                BIT(28)
#define BIT_AON_APB_ADC2_SOFT_RST                                BIT(27)
#define BIT_AON_APB_ADC1_SOFT_RST                                BIT(26)
#define BIT_AON_APB_MBOX_SOFT_RST                                BIT(25)
#define BIT_AON_APB_ROSC_SOFT_RST                                BIT(24)
#define BIT_AON_APB_PUBCP_SIM2_AON_TOP_SOFT_RST                  BIT(23)
#define BIT_AON_APB_PUBCP_SIM1_AON_TOP_SOFT_RST                  BIT(22)
#define BIT_AON_APB_PUBCP_SIM0_AON_TOP_SOFT_RST                  BIT(21)
#define BIT_AON_APB_THM1_SOFT_RST                                BIT(20)
#define BIT_AON_APB_DAC3_SOFT_RST                                BIT(19)
#define BIT_AON_APB_DAC2_SOFT_RST                                BIT(18)
#define BIT_AON_APB_DAC1_SOFT_RST                                BIT(17)
#define BIT_AON_APB_ADC3_CAL_SOFT_RST                            BIT(16)
#define BIT_AON_APB_ADC2_CAL_SOFT_RST                            BIT(15)
#define BIT_AON_APB_ADC1_CAL_SOFT_RST                            BIT(14)
#define BIT_AON_APB_MDAR_SOFT_RST                                BIT(13)
#define BIT_AON_APB_LVDSDIS_SOFT_RST                             BIT(12)
#define BIT_AON_APB_BB_CAL_SOFT_RST                              BIT(11)
#define BIT_AON_APB_DCXO_LC_SOFT_RST                             BIT(10)
#define BIT_AON_APB_AP_TMR2_SOFT_RST                             BIT(9)
#define BIT_AON_APB_AP_TMR1_SOFT_RST                             BIT(8)
#define BIT_AON_APB_CA53_WDG_SOFT_RST                            BIT(7)
#define BIT_AON_APB_AON_DMA_SOFT_RST                             BIT(6)
#define BIT_AON_APB_AP_SIM_AON_TOP_SOFT_RST                      BIT(5)
#define BIT_AON_APB_DMC_PHY_SOFT_RST                             BIT(4)
#define BIT_AON_APB_GPU_THMA_SOFT_RST                            BIT(3)
#define BIT_AON_APB_ARM_THMA_SOFT_RST                            BIT(2)
#define BIT_AON_APB_THM0_SOFT_RST                                BIT(1)
#define BIT_AON_APB_PMU_SOFT_RST                                 BIT(0)

/* REG_AON_APB_APB_RTC_EB */

#define BIT_AON_APB_BB_CAL_RTC_EB                                BIT(18)
#define BIT_AON_APB_DCXO_LC_RTC_EB                               BIT(17)
#define BIT_AON_APB_AP_TMR2_RTC_EB                               BIT(16)
#define BIT_AON_APB_AP_TMR1_RTC_EB                               BIT(15)
#define BIT_AON_APB_GPU_THMA_RTC_AUTO_EN                         BIT(14)
#define BIT_AON_APB_ARM_THMA_RTC_AUTO_EN                         BIT(13)
#define BIT_AON_APB_GPU_THMA_RTC_EB                              BIT(12)
#define BIT_AON_APB_ARM_THMA_RTC_EB                              BIT(11)
#define BIT_AON_APB_THM_RTC_EB                                   BIT(10)
#define BIT_AON_APB_CA53_WDG_RTC_EB                              BIT(9)
#define BIT_AON_APB_AP_WDG_RTC_EB                                BIT(8)
#define BIT_AON_APB_EIC_RTCDV5_EB                                BIT(7)
#define BIT_AON_APB_EIC_RTC_EB                                   BIT(6)
#define BIT_AON_APB_AP_TMR0_RTC_EB                               BIT(5)
#define BIT_AON_APB_AON_TMR_RTC_EB                               BIT(4)
#define BIT_AON_APB_AP_SYST_RTC_EB                               BIT(3)
#define BIT_AON_APB_AON_SYST_RTC_EB                              BIT(2)
#define BIT_AON_APB_KPD_RTC_EB                                   BIT(1)
#define BIT_AON_APB_ARCH_RTC_EB                                  BIT(0)

/* REG_AON_APB_REC_26MHZ_BUF_CFG */

#define BIT_AON_APB_PLL_PROBE_SEL(x)                             (((x) & 0x3F) << 8)
#define BIT_AON_APB_REC_26MHZ_1_CUR_SEL                          BIT(4)
#define BIT_AON_APB_REC_26MHZ_0_CUR_SEL                          BIT(0)

/* REG_AON_APB_SINDRV_CTRL */

#define BIT_AON_APB_CLK26M_TUNED_SEL                             BIT(20)
#define BIT_AON_APB_CLK26M_RESERVED(x)                           (((x) & 0x7FF) << 9)
#define BIT_AON_APB_REC_26MHZ_SR_TRIM(x)                         (((x) & 0xF) << 5)
#define BIT_AON_APB_SINDRV_LVL(x)                                (((x) & 0x3) << 3)
#define BIT_AON_APB_SINDRV_CLIP_MODE                             BIT(2)
#define BIT_AON_APB_SINDRV_ENA_SQUARE                            BIT(1)
#define BIT_AON_APB_SINDRV_ENA                                   BIT(0)

/* REG_AON_APB_ADA_SEL_CTRL */

#define BIT_AON_APB_TW_MODE_SEL                                  BIT(3)
#define BIT_AON_APB_WGADC_DIV_EN                                 BIT(2)
#define BIT_AON_APB_AFCDAC_SYS_SEL                               BIT(1)
#define BIT_AON_APB_APCDAC_SYS_SEL                               BIT(0)

/* REG_AON_APB_VBC_CTRL */

#define BIT_AON_APB_AUDIF_CKG_AUTO_EN                            BIT(20)
#define BIT_AON_APB_AUD_INT_SYS_SEL(x)                           (((x) & 0x3) << 18)
#define BIT_AON_APB_VBC_DA23_INT_SYS_SEL(x)                      (((x) & 0x3) << 16)
#define BIT_AON_APB_VBC_AD01_INT_SYS_SEL(x)                      (((x) & 0x3) << 12)
#define BIT_AON_APB_VBC_DA01_INT_SYS_SEL(x)                      (((x) & 0x3) << 10)
#define BIT_AON_APB_VBC_AD01_DMA_SYS_SEL(x)                      (((x) & 0x3) << 6)
#define BIT_AON_APB_VBC_DA01_DMA_SYS_SEL(x)                      (((x) & 0x3) << 4)
#define BIT_AON_APB_VBC_DA23_DMA_SYS_SEL(x)                      (((x) & 0x3) << 2)
#define BIT_AON_APB_VBC_DMA_WTLCP_ARM_SEL                        BIT(1)
#define BIT_AON_APB_VBC_DMA_PUBCP_ARM_SEL                        BIT(0)

/* REG_AON_APB_PWR_CTRL */

#define BIT_AON_APB_FORCE_DSI_DBG_PHY_SHUTDOWNZ                  BIT(21)
#define BIT_AON_APB_FORCE_CSI_S_PHY_SHUTDOWNZ                    BIT(20)
#define BIT_AON_APB_HSIC_PLL_EN                                  BIT(19)
#define BIT_AON_APB_HSIC_PHY_PD                                  BIT(18)
#define BIT_AON_APB_USB_PHY_PD_S                                 BIT(17)
#define BIT_AON_APB_USB_PHY_PD_L                                 BIT(16)
#define BIT_AON_APB_MIPI_DSI_PS_PD_S                             BIT(15)
#define BIT_AON_APB_MIPI_DSI_PS_PD_L                             BIT(14)
#define BIT_AON_APB_MIPI_CSI_2P2LANE_PS_PD_S                     BIT(13)
#define BIT_AON_APB_MIPI_CSI_2P2LANE_PS_PD_L                     BIT(12)
#define BIT_AON_APB_MIPI_CSI_4LANE_PS_PD_S                       BIT(11)
#define BIT_AON_APB_MIPI_CSI_4LANE_PS_PD_L                       BIT(10)
#define BIT_AON_APB_CA53_TS0_STOP                                BIT(8)
#define BIT_AON_APB_USB20_ISO_SW_EN                              BIT(7)
#define BIT_AON_APB_CSI_ISO_SW_EN_4LANE                          BIT(5)
#define BIT_AON_APB_CSI_ISO_SW_EN_2P2LANE                        BIT(4)
#define BIT_AON_APB_EFUSE_BIST_PWR_ON                            BIT(3)
#define BIT_AON_APB_FORCE_DSI_PHY_SHUTDOWNZ                      BIT(2)
#define BIT_AON_APB_FORCE_CSI_PHY_SHUTDOWNZ                      BIT(1)

/* REG_AON_APB_TS_CFG */

#define BIT_AON_APB_DBG_TRACE_CTRL_EN                            BIT(14)
#define BIT_AON_APB_CSYSACK_TS_LP_2                              BIT(13)
#define BIT_AON_APB_CSYSREQ_TS_LP_2                              BIT(12)
#define BIT_AON_APB_CSYSACK_TS_LP_1                              BIT(11)
#define BIT_AON_APB_CSYSREQ_TS_LP_1                              BIT(10)
#define BIT_AON_APB_CSYSACK_TS_LP_0                              BIT(9)
#define BIT_AON_APB_CSYSREQ_TS_LP_0                              BIT(8)
#define BIT_AON_APB_EVENTACK_RESTARTREQ_TS01                     BIT(4)
#define BIT_AON_APB_EVENT_RESTARTREQ_TS01                        BIT(1)
#define BIT_AON_APB_EVENT_HALTREQ_TS01                           BIT(0)

/* REG_AON_APB_BOOT_MODE */

#define BIT_AON_APB_ARM_JTAG_EN                                  BIT(13)
#define BIT_AON_APB_WPLL_OVR_FREQ_SEL                            BIT(12)
#define BIT_AON_APB_PTEST_FUNC_ATSPEED_SEL                       BIT(8)
#define BIT_AON_APB_PTEST_FUNC_MODE                              BIT(7)
#define BIT_AON_APB_FUNCTST_DMA_EB                               BIT(5)
#define BIT_AON_APB_USB_DLOAD_EN                                 BIT(4)
#define BIT_AON_APB_ARM_BOOT_MD3                                 BIT(3)
#define BIT_AON_APB_ARM_BOOT_MD2                                 BIT(2)
#define BIT_AON_APB_ARM_BOOT_MD1                                 BIT(1)
#define BIT_AON_APB_ARM_BOOT_MD0                                 BIT(0)

/* REG_AON_APB_BB_BG_CTRL */

#define BIT_AON_APB_BB_CON_BG                                    BIT(22)
#define BIT_AON_APB_BB_BG_RSV(x)                                 (((x) & 0x3) << 20)
#define BIT_AON_APB_BB_LDO_V(x)                                  (((x) & 0xF) << 16)
#define BIT_AON_APB_BB_BG_RBIAS_EN                               BIT(15)
#define BIT_AON_APB_BB_BG_IEXT_IB_EN                             BIT(14)
#define BIT_AON_APB_BB_LDO_REFCTRL(x)                            (((x) & 0x3) << 12)
#define BIT_AON_APB_BB_LDO_AUTO_PD_EN                            BIT(11)
#define BIT_AON_APB_BB_LDO_SLP_PD_EN                             BIT(10)
#define BIT_AON_APB_BB_LDO_FORCE_ON                              BIT(9)
#define BIT_AON_APB_BB_LDO_FORCE_PD                              BIT(8)
#define BIT_AON_APB_BB_BG_AUTO_PD_EN                             BIT(3)
#define BIT_AON_APB_BB_BG_SLP_PD_EN                              BIT(2)
#define BIT_AON_APB_BB_BG_FORCE_ON                               BIT(1)
#define BIT_AON_APB_BB_BG_FORCE_PD                               BIT(0)

/* REG_AON_APB_CP_ARM_JTAG_CTRL */

#define BIT_AON_APB_CP_ARM_JTAG_PIN_SEL(x)                       (((x) & 0x7))

/* REG_AON_APB_PLL_SOFT_CNT_DONE */

#define BIT_AON_APB_XTLBUF1_SOFT_CNT_DONE                        BIT(9)
#define BIT_AON_APB_XTLBUF0_SOFT_CNT_DONE                        BIT(8)
#define BIT_AON_APB_LVDSPLL_SOFT_CNT_DONE                        BIT(4)
#define BIT_AON_APB_LPLL_SOFT_CNT_DONE                           BIT(3)
#define BIT_AON_APB_TWPLL_SOFT_CNT_DONE                          BIT(2)
#define BIT_AON_APB_DPLL_SOFT_CNT_DONE                           BIT(1)
#define BIT_AON_APB_MPLL_SOFT_CNT_DONE                           BIT(0)

/* REG_AON_APB_DCXO_LC_REG0 */

#define BIT_AON_APB_DCXO_LC_FLAG                                 BIT(8)
#define BIT_AON_APB_DCXO_LC_FLAG_CLR                             BIT(1)
#define BIT_AON_APB_DCXO_LC_CNT_CLR                              BIT(0)

/* REG_AON_APB_DCXO_LC_REG1 */

#define BIT_AON_APB_DCXO_LC_CNT(x)                               (((x) & 0xFFFFFFFF))

/* REG_AON_APB_MPLL_CFG1 */

#define BIT_AON_APB_MPLL_RES(x)                                  (((x) & 0xFF) << 22)
#define BIT_AON_APB_MPLL_LOCK_DONE                               BIT(21)
#define BIT_AON_APB_MPLL_DIV_S                                   BIT(20)
#define BIT_AON_APB_MPLL_MOD_EN                                  BIT(19)
#define BIT_AON_APB_MPLL_SDM_EN                                  BIT(18)
#define BIT_AON_APB_MPLL_LPF(x)                                  (((x) & 0x7) << 15)
#define BIT_AON_APB_MPLL_IBIAS(x)                                (((x) & 0x3) << 11)
#define BIT_AON_APB_MPLL_N(x)                                    (((x) & 0x7FF))

/* REG_AON_APB_MPLL_CFG2 */

#define BIT_AON_APB_MPLL_POSTDIV                                 BIT(30)
#define BIT_AON_APB_MPLL_NINT(x)                                 (((x) & 0x7F) << 23)
#define BIT_AON_APB_MPLL_KINT(x)                                 (((x) & 0x7FFFFF))

/* REG_AON_APB_DPLL_CFG1 */

#define BIT_AON_APB_DPLL_RES(x)                                  (((x) & 0xFF) << 22)
#define BIT_AON_APB_DPLL_LOCK_DONE                               BIT(21)
#define BIT_AON_APB_DPLL_DIV_S                                   BIT(20)
#define BIT_AON_APB_DPLL_MOD_EN                                  BIT(19)
#define BIT_AON_APB_DPLL_SDM_EN                                  BIT(18)
#define BIT_AON_APB_DPLL_LPF(x)                                  (((x) & 0x7) << 15)
#define BIT_AON_APB_DPLL_IBIAS(x)                                (((x) & 0x3) << 11)
#define BIT_AON_APB_DPLL_N(x)                                    (((x) & 0x7FF))

/* REG_AON_APB_DPLL_CFG2 */

#define BIT_AON_APB_DPLL_NINT(x)                                 (((x) & 0x7F) << 23)
#define BIT_AON_APB_DPLL_KINT(x)                                 (((x) & 0x7FFFFF))

/* REG_AON_APB_TWPLL_CFG1 */

#define BIT_AON_APB_TWPLL_RES(x)                                 (((x) & 0xFF) << 22)
#define BIT_AON_APB_TWPLL_LOCK_DONE                              BIT(21)
#define BIT_AON_APB_TWPLL_DIV_S                                  BIT(20)
#define BIT_AON_APB_TWPLL_MOD_EN                                 BIT(19)
#define BIT_AON_APB_TWPLL_SDM_EN                                 BIT(18)
#define BIT_AON_APB_TWPLL_LPF(x)                                 (((x) & 0x7) << 15)
#define BIT_AON_APB_TWPLL_IBIAS(x)                               (((x) & 0x3) << 11)
#define BIT_AON_APB_TWPLL_N(x)                                   (((x) & 0x7FF))

/* REG_AON_APB_TWPLL_CFG2 */

#define BIT_AON_APB_TWPLL_NINT(x)                                (((x) & 0x7F) << 23)
#define BIT_AON_APB_TWPLL_KINT(x)                                (((x) & 0x7FFFFF))

/* REG_AON_APB_LTEPLL_CFG1 */

#define BIT_AON_APB_LTEPLL_RES(x)                                (((x) & 0xFF) << 22)
#define BIT_AON_APB_LTEPLL_LOCK_DONE                             BIT(21)
#define BIT_AON_APB_LTEPLL_DIV_S                                 BIT(20)
#define BIT_AON_APB_LTEPLL_MOD_EN                                BIT(19)
#define BIT_AON_APB_LTEPLL_SDM_EN                                BIT(18)
#define BIT_AON_APB_LTEPLL_LPF(x)                                (((x) & 0x7) << 15)
#define BIT_AON_APB_LTEPLL_IBIAS(x)                              (((x) & 0x3) << 11)
#define BIT_AON_APB_LTEPLL_N(x)                                  (((x) & 0x7FF))

/* REG_AON_APB_LTEPLL_CFG2 */

#define BIT_AON_APB_LTEPLL_NINT(x)                               (((x) & 0x7F) << 23)
#define BIT_AON_APB_LTEPLL_KINT(x)                               (((x) & 0x7FFFFF))

/* REG_AON_APB_LVDSRFPLL_CFG1 */

#define BIT_AON_APB_LVDSRFPLL_RES(x)                             (((x) & 0xFF) << 22)
#define BIT_AON_APB_LVDSRFPLL_LOCK_DONE                          BIT(21)
#define BIT_AON_APB_LVDSRFPLL_DIV_S                              BIT(20)
#define BIT_AON_APB_LVDSRFPLL_MOD_EN                             BIT(19)
#define BIT_AON_APB_LVDSRFPLL_SDM_EN                             BIT(18)
#define BIT_AON_APB_LVDSRFPLL_LPF(x)                             (((x) & 0x7) << 15)
#define BIT_AON_APB_LVDSRFPLL_IBIAS(x)                           (((x) & 0x3) << 11)
#define BIT_AON_APB_LVDSRFPLL_N(x)                               (((x) & 0x7FF))

/* REG_AON_APB_LVDSRFPLL_CFG2 */

#define BIT_AON_APB_LVDSRF_PLL_CLKOUT_EN                         BIT(30)
#define BIT_AON_APB_LVDSRFPLL_NINT(x)                            (((x) & 0x3F) << 24)
#define BIT_AON_APB_LVDSRFPLL_KINT(x)                            (((x) & 0xFFFFFF))

/* REG_AON_APB_AON_REG_PROT */

#define BIT_AON_APB_LDSP_CTRL_PROT                               BIT(31)
#define BIT_AON_APB_REG_PROT_VAL(x)                              (((x) & 0xFFFF))

/* REG_AON_APB_DSI_PHY_CTRL */

#define BIT_AON_APB_DSI_IF_SEL                                   BIT(24)
#define BIT_AON_APB_DSI_TRIMBG(x)                                (((x) & 0xF) << 20)
#define BIT_AON_APB_DSI_RCTL(x)                                  (((x) & 0xF) << 16)
#define BIT_AON_APB_DSI_RES(x)                                   (((x) & 0xFFFF))

/* REG_AON_APB_CSI_2L_PHY_CTRL */

#define BIT_AON_APB_CSI_2L_IF_SEL                                BIT(20)
#define BIT_AON_APB_CSI_2L_RCTL(x)                               (((x) & 0xF) << 16)
#define BIT_AON_APB_CSI_2L_RES(x)                                (((x) & 0xFFFF))

/* REG_AON_APB_CSI_2P2L_S_PHY_CTRL */

#define BIT_AON_APB_CSI_2P2L_TESTCLR_S                           BIT(23)
#define BIT_AON_APB_CSI_2P2L_TESTCLR_S_SEL                       BIT(22)
#define BIT_AON_APB_CSI_2P2L_TESTCLK_S_EN                        BIT(21)
#define BIT_AON_APB_CSI_2P2L_S_IF_SEL                            BIT(20)

/* REG_AON_APB_CSI_2P2L_M_PHY_CTRL */

#define BIT_AON_APB_CSI_2P2L_TESTCLR_M                           BIT(23)
#define BIT_AON_APB_CSI_2P2L_TESTCLR_M_SEL                       BIT(22)
#define BIT_AON_APB_CSI_2P2L_TESTCLK_M_EN                        BIT(21)
#define BIT_AON_APB_CSI_2P2L_M_IF_SEL                            BIT(20)
#define BIT_AON_APB_CSI_2P2L_RCTL(x)                             (((x) & 0xF) << 16)
#define BIT_AON_APB_CSI_2P2L_RES(x)                              (((x) & 0xFFFF))

/* REG_AON_APB_CSI_2P2L_DBG_PHY_CTRL */

#define BIT_AON_APB_CSI_2P2L_DBG_EN                              BIT(25)
#define BIT_AON_APB_CSI_2P2L_DBG_IF_SEL                          BIT(24)
#define BIT_AON_APB_CSI_2P2L_DBG_TRIMBG(x)                       (((x) & 0xF) << 20)

/* REG_AON_APB_CSI_2P2L_PHY_CTRL */

#define BIT_AON_APB_CSI_2P2L_MODE_SEL                            BIT(0)

/* REG_AON_APB_AON_CGM_CFG */

#define BIT_AON_APB_PROBE_CKG_DIV(x)                             (((x) & 0xF) << 28)
#define BIT_AON_APB_AUX2_CKG_DIV(x)                              (((x) & 0xF) << 24)
#define BIT_AON_APB_AUX1_CKG_DIV(x)                              (((x) & 0xF) << 20)
#define BIT_AON_APB_AUX0_CKG_DIV(x)                              (((x) & 0xF) << 16)
#define BIT_AON_APB_PROBE_CKG_SEL(x)                             (((x) & 0xF) << 12)
#define BIT_AON_APB_AUX2_CKG_SEL(x)                              (((x) & 0xF) << 8)
#define BIT_AON_APB_AUX1_CKG_SEL(x)                              (((x) & 0xF) << 4)
#define BIT_AON_APB_AUX0_CKG_SEL(x)                              (((x) & 0xF))

/* REG_AON_APB_DFS_CLK_GATE_CFG */

#define BIT_AON_APB_DMC_CKG_SEL_CURRENT(x)                       (((x) & 0xFF) << 19)
#define BIT_AON_APB_DMC_CKG_SEL_DEFAULT(x)                       (((x) & 0xFF) << 11)
#define BIT_AON_APB_PUB_DMC_2X_CGM_SEL                           BIT(10)
#define BIT_AON_APB_PUB_DMC_1X_CGM_SEL                           BIT(9)
#define BIT_AON_APB_PUB_DFS_SWITCH_WAIT_TIME(x)                  (((x) & 0xFF) << 1)
#define BIT_AON_APB_DMC_CKG_SEL_LOAD                             BIT(0)

/* REG_AON_APB_HARD_DFS_CTRL */

#define BIT_AON_APB_CGM_DMC_SEL(x)                               (((x) & 0x7) << 6)
#define BIT_AON_APB_CGM_DMC_2X_DIV(x)                            (((x) & 0xF) << 2)
#define BIT_AON_APB_CGM_DMC_1X_DIV                               BIT(1)
#define BIT_AON_APB_CGM_DMC_SEL_HW_EN                            BIT(0)

/* REG_AON_APB_APB_EB2 */

#define BIT_AON_APB_PUBCP_SIM2_AON_TOP_EB                        BIT(19)
#define BIT_AON_APB_PUBCP_SIM1_AON_TOP_EB                        BIT(18)
#define BIT_AON_APB_PUBCP_SIM0_AON_TOP_EB                        BIT(17)
#define BIT_AON_APB_AP_SIM_AON_TOP_EB                            BIT(16)
#define BIT_AON_APB_AP_DAP_EB                                    BIT(15)
#define BIT_AON_APB_BSMTMR_EB                                    BIT(14)
#define BIT_AON_APB_ANLG_APB_EB                                  BIT(13)
#define BIT_AON_APB_PIN_APB_EB                                   BIT(12)
#define BIT_AON_APB_ANLG_EB                                      BIT(11)
#define BIT_AON_APB_BUSMON_DMA_EB                                BIT(10)
#define BIT_AON_APB_SERDES_DPHY_REF_EB                           BIT(9)
#define BIT_AON_APB_SERDES_DPHY_CFG_EB                           BIT(8)
#define BIT_AON_APB_ROSC_EB                                      BIT(7)
#define BIT_AON_APB_PUB_REG_EB                                   BIT(6)
#define BIT_AON_APB_DMC_EB                                       BIT(5)
#define BIT_AON_APB_CSSYS_EB                                     BIT(4)
#define BIT_AON_APB_RFTI_RX_EB                                   BIT(3)
#define BIT_AON_APB_RFTI_TX_EB                                   BIT(2)
#define BIT_AON_APB_WCDMA_ICI_EB                                 BIT(1)
#define BIT_AON_APB_WCDMA_EB                                     BIT(0)

/* REG_AON_APB_ROSC_CFG */

#define BIT_AON_APB_ROSC_NUM(x)                                  (((x) & 0xFFFF) << 12)
#define BIT_AON_APB_ROSC_EN                                      BIT(0)

/* REG_AON_APB_ROSC_STATUS */

#define BIT_AON_APB_ROSC_CNT(x)                                  (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_ROSC_VALID                                   BIT(0)

/* REG_AON_APB_AON_ANALOG_RSV */

#define BIT_AON_APB_ANALOG_MPLL_DPLL_TOP_RSV(x)                  (((x) & 0x7FF))

/* REG_AON_APB_AON_CHIP_ID0 */

#define BIT_AON_APB_AON_CHIP_ID0(x)                              (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AON_CHIP_ID1 */

#define BIT_AON_APB_AON_CHIP_ID1(x)                              (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AON_PLAT_ID0 */

#define BIT_AON_APB_AON_PLAT_ID0(x)                              (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AON_PLAT_ID1 */

#define BIT_AON_APB_AON_PLAT_ID1(x)                              (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AON_IMPL_ID */

#define BIT_AON_APB_AON_IMPL_ID(x)                               (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AON_MFT_ID */

#define BIT_AON_APB_AON_MFT_ID(x)                                (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AON_VER_ID */

#define BIT_AON_APB_AON_VER_ID(x)                                (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AON_CHIP_ID */

#define BIT_AON_APB_AON_CHIP_ID(x)                               (((x) & 0xFFFFFFFF))

/* REG_AON_APB_CCIR_RCVR_CFG */

#define BIT_AON_APB_CCIR_SE                                      BIT(1)
#define BIT_AON_APB_CCIR_IE                                      BIT(0)

/* REG_AON_APB_PLL_BG_CFG */

#define BIT_AON_APB_PLL_BG_RSV(x)                                (((x) & 0x3) << 4)
#define BIT_AON_APB_PLL_BG_RBIAS_EN                              BIT(3)
#define BIT_AON_APB_PLL_BG_PD                                    BIT(2)
#define BIT_AON_APB_PLL_BG_IEXT_IBEN                             BIT(1)
#define BIT_AON_APB_PLL_CON_BG                                   BIT(0)

/* REG_AON_APB_LVDSDIS_SEL */

#define BIT_AON_APB_LVDSDIS_LOG_SEL(x)                           (((x) & 0x3) << 1)
#define BIT_AON_APB_LVDSDIS_DBG_SEL                              BIT(0)

/* REG_AON_APB_DJTAG_MUX_SEL */

#define BIT_AON_APB_DJTAG_AON_SEL                                BIT(6)
#define BIT_AON_APB_DJTAG_PUB_SEL                                BIT(5)
#define BIT_AON_APB_DJTAG_PUBCP_SEL                              BIT(4)
#define BIT_AON_APB_DJTAG_WTLCP_SEL                              BIT(3)
#define BIT_AON_APB_DJTAG_GPU_SEL                                BIT(2)
#define BIT_AON_APB_DJTAG_MM_SEL                                 BIT(1)
#define BIT_AON_APB_DJTAG_AP_SEL                                 BIT(0)

/* REG_AON_APB_CM4_SYS_SOFT_RST */

#define BIT_AON_APB_CM4_SYS_SOFT_RST                             BIT(4)
#define BIT_AON_APB_CM4_CORE_SOFT_RST                            BIT(0)

/* REG_AON_APB_PUBCP_WTLCP_ADDR_MSB */

#define BIT_AON_APB_PUBCP_WTLCP_ADDR_MSB(x)                      (((x) & 0xF))

/* REG_AON_APB_AON_DMA_INT_EN */

#define BIT_AON_APB_AON_DMA_INT_CM4_EN                           BIT(6)
#define BIT_AON_APB_AON_DMA_INT_AP_EN                            BIT(0)

/* REG_AON_APB_EMC_AUTO_GATE_EN */

#define BIT_AON_APB_PUBCP_PUB_AUTO_GATE_EN                       BIT(19)
#define BIT_AON_APB_WTLCP_PUB_AUTO_GATE_EN                       BIT(18)
#define BIT_AON_APB_AP_PUB_AUTO_GATE_EN                          BIT(17)
#define BIT_AON_APB_AON_APB_PUB_AUTO_GATE_EN                     BIT(16)
#define BIT_AON_APB_PUBCP_EMC_AUTO_GATE_EN                       BIT(3)
#define BIT_AON_APB_WTLCP_EMC_AUTO_GATE_EN                       BIT(2)
#define BIT_AON_APB_AP_EMC_AUTO_GATE_EN                          BIT(1)
#define BIT_AON_APB_CA53_EMC_AUTO_GATE_EN                        BIT(0)

/* REG_AON_APB_CM4_CFG_BUS */

#define BIT_AON_APB_CM4_CFG_BUS_SLEEP                            BIT(0)

/* REG_AON_APB_APB_RST2 */

#define BIT_AON_APB_BSMTMR_SOFT_RST                              BIT(18)
#define BIT_AON_APB_WTLCP_TDSP_CORE_SRST                         BIT(17)
#define BIT_AON_APB_WTLCP_LDSP_CORE_SRST                         BIT(16)
#define BIT_AON_APB_WCN_DJTAG_SOFT_RST                           BIT(15)
#define BIT_AON_APB_ANLG_SOFT_RST                                BIT(14)
#define BIT_AON_APB_SERDES_DPHY_APB_SOFT_RST                     BIT(13)
#define BIT_AON_APB_BUSMON_DMA_SOFT_RST                          BIT(12)
#define BIT_AON_APB_SERDES_DPHY_SOFT_RST                         BIT(11)
#define BIT_AON_APB_CROSS_TRIG_SOFT_RST                          BIT(10)
#define BIT_AON_APB_SERDES_SOFT_RST                              BIT(9)
#define BIT_AON_APB_DBG_SOFT_RST                                 BIT(8)
#define BIT_AON_APB_DJTAG_SOFT_RST                               BIT(7)
#define BIT_AON_APB_AON_DJTAG_SOFT_RST                           BIT(6)
#define BIT_AON_APB_PUB_DJTAG_SOFT_RST                           BIT(5)
#define BIT_AON_APB_GPU_DJTAG_SOFT_RST                           BIT(4)
#define BIT_AON_APB_MM_DJTAG_SOFT_RST                            BIT(3)
#define BIT_AON_APB_PUBCP_DJTAG_SOFT_RST                         BIT(2)
#define BIT_AON_APB_WTLCP_DJTAG_SOFT_RST                         BIT(1)
#define BIT_AON_APB_AP_DJTAG_SOFT_RST                            BIT(0)

/* REG_AON_APB_CLK_EB0 */

#define BIT_AON_APB_ALL_PLL_TEST_EB                              BIT(18)
#define BIT_AON_APB_LVDSRF_CALI_EB                               BIT(17)
#define BIT_AON_APB_RFTI2_LTH_EB                                 BIT(16)
#define BIT_AON_APB_RFTI2_XO_EB                                  BIT(15)
#define BIT_AON_APB_RFTI1_LTH_EB                                 BIT(14)
#define BIT_AON_APB_RFTI1_XO_EB                                  BIT(13)
#define BIT_AON_APB_RFTI_SBI_EB                                  BIT(12)
#define BIT_AON_APB_TMR_EB                                       BIT(11)
#define BIT_AON_APB_DET_32K_EB                                   BIT(10)
#define BIT_AON_APB_AP_HS_SPI_EB                                 BIT(9)
#define BIT_AON_APB_CSSYS_CA53_EB                                BIT(8)
#define BIT_AON_APB_CGM_TSEN_EN                                  BIT(0)

/* REG_AON_APB_AON_SDIO */

#define BIT_AON_APB_CP_SDIO_ENABLE                               BIT(2)
#define BIT_AON_APB_AP_SDIO_ENABLE                               BIT(1)
#define BIT_AON_APB_SDIO_MODULE_SEL                              BIT(0)

/* REG_AON_APB_MPLL_CTRL */

#define BIT_AON_APB_CGM_MPLL_CA53_FORCE_EN                       BIT(9)
#define BIT_AON_APB_CGM_MPLL_CA53_AUTO_GATE_SEL                  BIT(8)
#define BIT_AON_APB_MPLL_WAIT_FORCE_EN                           BIT(2)
#define BIT_AON_APB_MPLL_WAIT_AUTO_GATE_SEL                      BIT(1)

/* REG_AON_APB_AUTO_GATE_CTRL0 */

#define BIT_AON_APB_MBOX_AUTO_GATE_SEL                           BIT(30)
#define BIT_AON_APB_CGM_MM_ISP_FORCE_EN                          BIT(29)
#define BIT_AON_APB_CGM_MM_ISP_AUTO_GATE_SEL                     BIT(28)
#define BIT_AON_APB_CGM_AON_APB_PUB_FORCE_EN                     BIT(27)
#define BIT_AON_APB_CGM_AON_APB_PUB_AUTO_GATE_SEL                BIT(26)
#define BIT_AON_APB_CGM_AON_APB_WCN_FORCE_EN                     BIT(25)
#define BIT_AON_APB_CGM_AON_APB_WCN_AUTO_GATE_SEL                BIT(24)
#define BIT_AON_APB_CGM_AON_APB_PUBCP_FORCE_EN                   BIT(23)
#define BIT_AON_APB_CGM_AON_APB_PUBCP_AUTO_GATE_SEL              BIT(22)
#define BIT_AON_APB_CGM_AON_APB_WTLCP_FORCE_EN                   BIT(21)
#define BIT_AON_APB_CGM_AON_APB_WTLCP_AUTO_GATE_SEL              BIT(20)
#define BIT_AON_APB_CGM_AON_APB_AP_FORCE_EN                      BIT(19)
#define BIT_AON_APB_CGM_AON_APB_AP_AUTO_GATE_SEL                 BIT(18)
#define BIT_AON_APB_CGM_AP_AXI_FORCE_EN                          BIT(17)
#define BIT_AON_APB_CGM_AP_AXI_AUTO_GATE_SEL                     BIT(16)
#define BIT_AON_APB_CGM_AP_MM_ROOT_FORCE_EN                      BIT(15)
#define BIT_AON_APB_CGM_AP_MM_ROOT_AUTO_GATE_SEL                 BIT(14)
#define BIT_AON_APB_CGM_AP_MM_AP_FORCE_EN                        BIT(13)
#define BIT_AON_APB_CGM_AP_MM_AP_AUTO_GATE_SEL                   BIT(12)
#define BIT_AON_APB_CGM_AP_MM_MM_FORCE_EN                        BIT(11)
#define BIT_AON_APB_CGM_AP_MM_MM_AUTO_GATE_SEL                   BIT(10)
#define BIT_AON_APB_CGM_AP_MM_GPU_FORCE_EN                       BIT(9)
#define BIT_AON_APB_CGM_AP_MM_GPU_AUTO_GATE_SEL                  BIT(8)
#define BIT_AON_APB_CGM_WTL_BRIDGE_FORCE_EN                      BIT(7)
#define BIT_AON_APB_CGM_WTL_BRIDGE_AUTO_GATE_SEL                 BIT(6)
#define BIT_AON_APB_CGM_NIC_GPU_FORCE_EN                         BIT(5)
#define BIT_AON_APB_CGM_NIC_GPU_AUTO_GATE_SEL                    BIT(4)
#define BIT_AON_APB_CGM_WCN_ROOT_FORCE_EN                        BIT(3)
#define BIT_AON_APB_CGM_WCN_ROOT_AUTO_GATE_SEL                   BIT(2)
#define BIT_AON_APB_CGM_WCN_SYS_312M_AON_FORCE_EN                BIT(1)
#define BIT_AON_APB_CGM_WCN_SYS_312M_AON_AUTO_GATE_SEL           BIT(0)

/* REG_AON_APB_AUTO_GATE_CTRL1 */

#define BIT_AON_APB_CGM_WCDMA_ROOT_FORCE_EN                      BIT(5)
#define BIT_AON_APB_CGM_WCDMA_ROOT_AUTO_GATE_SEL                 BIT(4)
#define BIT_AON_APB_CGM_WCDMA_PUBCP_FORCE_EN                     BIT(3)
#define BIT_AON_APB_CGM_WCDMA_PUBCP_AUTO_GATE_SEL                BIT(2)
#define BIT_AON_APB_CGM_WCDMA_WTLCP_FORCE_EN                     BIT(1)
#define BIT_AON_APB_CGM_WCDMA_WTLCP_AUTO_GATE_SEL                BIT(0)

/* REG_AON_APB_GPLL_CFG1 */

#define BIT_AON_APB_GPLL_RES(x)                                  (((x) & 0xFF) << 22)
#define BIT_AON_APB_GPLL_LOCK_DONE                               BIT(21)
#define BIT_AON_APB_GPLL_DIV_S                                   BIT(20)
#define BIT_AON_APB_GPLL_MOD_EN                                  BIT(19)
#define BIT_AON_APB_GPLL_SDM_EN                                  BIT(18)
#define BIT_AON_APB_GPLL_LPF(x)                                  (((x) & 0x7) << 15)
#define BIT_AON_APB_GPLL_IBIAS(x)                                (((x) & 0x3) << 11)
#define BIT_AON_APB_GPLL_N(x)                                    (((x) & 0x7FF))

/* REG_AON_APB_GPLL_CFG2 */

#define BIT_AON_APB_GPLL_POSTDIV                                 BIT(30)
#define BIT_AON_APB_GPLL_NINT(x)                                 (((x) & 0x7F) << 23)
#define BIT_AON_APB_GPLL_KINT(x)                                 (((x) & 0x7FFFFF))

/* REG_AON_APB_RPLL_CFG1 */

#define BIT_AON_APB_RPLL_LOCK_DONE                               BIT(21)
#define BIT_AON_APB_RPLL_DIV_S                                   BIT(20)
#define BIT_AON_APB_RPLL_MOD_EN                                  BIT(19)
#define BIT_AON_APB_RPLL_SDM_EN                                  BIT(18)
#define BIT_AON_APB_RPLL_LPF(x)                                  (((x) & 0x7) << 15)
#define BIT_AON_APB_RPLL_REFIN(x)                                (((x) & 0x3) << 13)
#define BIT_AON_APB_RPLL_IBIAS(x)                                (((x) & 0x3) << 11)
#define BIT_AON_APB_RPLL_N(x)                                    (((x) & 0x7FF))

/* REG_AON_APB_RPLL_CFG2 */

#define BIT_AON_APB_RPLL_NINT(x)                                 (((x) & 0x7F) << 23)
#define BIT_AON_APB_RPLL_KINT(x)                                 (((x) & 0x7FFFFF))

/* REG_AON_APB_RPLL_CFG3 */

#define BIT_AON_APB_RPLL_RES(x)                                  (((x) & 0x7FFF) << 16)
#define BIT_AON_APB_RPLL_26M_SEL                                 BIT(7)
#define BIT_AON_APB_RPLL_DIV1_EN                                 BIT(6)
#define BIT_AON_APB_RPLL_26M_DIV(x)                              (((x) & 0x3F))

/* REG_AON_APB_THM0_CTRL */

#define BIT_AON_APB_THM0_CALI_RSVD(x)                            (((x) & 0xFF))

/* REG_AON_APB_THM1_CTRL */

#define BIT_AON_APB_THM1_CALI_RSVD(x)                            (((x) & 0xFF))

/* REG_AON_APB_BUSMON_DMA_CFG */

#define BIT_AON_APB_BUSMON_DMA_CNT_START                         BIT(0)

/* REG_AON_APB_ANALOG_CFG0 */

#define BIT_AON_APB_ANALOG_PLL_RSV(x)                            (((x) & 0xFFFF) << 16)
#define BIT_AON_APB_ANALOG_TESTMUX(x)                            (((x) & 0xFFFF))

/* REG_AON_APB_ANALOG_CFG1 */

#define BIT_AON_APB_ANALOG_BB_RSV(x)                             (((x) & 0xFFFF))

/* REG_AON_APB_RPLL_BIST_CTRL */

#define BIT_AON_APB_RPLL_BIST_CNT(x)                             (((x) & 0xFFFF) << 16)
#define BIT_AON_APB_RPLL_BIST_CTRL(x)                            (((x) & 0x1FF) << 1)
#define BIT_AON_APB_RPLL_BIST_EN                                 BIT(0)

/* REG_AON_APB_MPLL_BIST_CTRL */

#define BIT_AON_APB_MPLL_BIST_CNT(x)                             (((x) & 0xFFFF) << 16)
#define BIT_AON_APB_MPLL_BIST_CTRL(x)                            (((x) & 0x1FF) << 1)
#define BIT_AON_APB_MPLL_BIST_EN                                 BIT(0)

/* REG_AON_APB_DPLL_BIST_CTRL */

#define BIT_AON_APB_DPLL_BIST_CNT(x)                             (((x) & 0xFFFF) << 16)
#define BIT_AON_APB_DPLL_BIST_CTRL(x)                            (((x) & 0x1FF) << 1)
#define BIT_AON_APB_DPLL_BIST_EN                                 BIT(0)

/* REG_AON_APB_GPLL_BIST_CTRL */

#define BIT_AON_APB_GPLL_BIST_CNT(x)                             (((x) & 0xFFFF) << 16)
#define BIT_AON_APB_GPLL_BIST_CTRL(x)                            (((x) & 0x1FF) << 1)
#define BIT_AON_APB_GPLL_BIST_EN                                 BIT(0)

/* REG_AON_APB_TWPLL_BIST_CTRL */

#define BIT_AON_APB_TWPLL_BIST_CNT(x)                            (((x) & 0xFFFF) << 16)
#define BIT_AON_APB_TWPLL_BIST_CTRL(x)                           (((x) & 0x1FF) << 1)
#define BIT_AON_APB_TWPLL_BIST_EN                                BIT(0)

/* REG_AON_APB_LPLL_BIST_CTRL */

#define BIT_AON_APB_LPLL_BIST_CNT(x)                             (((x) & 0xFFFF) << 16)
#define BIT_AON_APB_LPLL_BIST_CTRL(x)                            (((x) & 0x1FF) << 1)
#define BIT_AON_APB_LPLL_BIST_EN                                 BIT(0)

/* REG_AON_APB_DPLL_CTRL */

#define BIT_AON_APB_DPLL_PRE_DIV_MONITOR_GATE_AUTO_EN_STATUS(x)  (((x) & 0x3) << 13)
#define BIT_AON_APB_DPLL_PRE_DIV_MONITOR_DIV_AUTO_EN_STATUS      BIT(12)
#define BIT_AON_APB_DPLL_PRE_DIV_MONITOR_WAIT_EN_STATUS          BIT(11)
#define BIT_AON_APB_CGM_DPLL_40M_AON_FORCE_EN                    BIT(10)
#define BIT_AON_APB_CGM_DPLL_40M_AON_AUTO_GATE_SEL               BIT(9)
#define BIT_AON_APB_CGM_DPLL_AON_FORCE_EN                        BIT(8)
#define BIT_AON_APB_CGM_DPLL_AON_AUTO_GATE_SEL                   BIT(7)
#define BIT_AON_APB_DPLL_DIV_40M_FORCE_EN                        BIT(4)
#define BIT_AON_APB_DPLL_DIV_40M_AUTO_GATE_SEL                   BIT(3)
#define BIT_AON_APB_DPLL_WAIT_FORCE_EN                           BIT(2)
#define BIT_AON_APB_DPLL_WAIT_AUTO_GATE_SEL                      BIT(1)

/* REG_AON_APB_CPPLL_CFG1 */

#define BIT_AON_APB_CPPLL_RES(x)                                 (((x) & 0xFF) << 22)
#define BIT_AON_APB_CPPLL_LOCK_DONE                              BIT(21)
#define BIT_AON_APB_CPPLL_DIV_S                                  BIT(20)
#define BIT_AON_APB_CPPLL_MOD_EN                                 BIT(19)
#define BIT_AON_APB_CPPLL_SDM_EN                                 BIT(18)
#define BIT_AON_APB_CPPLL_LPF(x)                                 (((x) & 0x7) << 15)
#define BIT_AON_APB_CPPLL_IBIAS(x)                               (((x) & 0x3) << 11)
#define BIT_AON_APB_CPPLL_N(x)                                   (((x) & 0x7FF))

/* REG_AON_APB_CPPLL_CFG2 */

#define BIT_AON_APB_CPPLL_POSTDIV                                BIT(30)
#define BIT_AON_APB_CPPLL_NINT(x)                                (((x) & 0x7F) << 23)
#define BIT_AON_APB_CPPLL_KINT(x)                                (((x) & 0x7FFFFF))

/* REG_AON_APB_CPPLL_BIST_CTRL */

#define BIT_AON_APB_CPPLL_BIST_CNT(x)                            (((x) & 0xFFFF) << 16)
#define BIT_AON_APB_CPPLL_BIST_CTRL(x)                           (((x) & 0x1FF) << 1)
#define BIT_AON_APB_CPPLL_BIST_EN                                BIT(0)

/* REG_AON_APB_MPLL_CFG3 */

#define BIT_AON_APB_MPLL_CCS_CTRL(x)                             (((x) & 0xFF))

/* REG_AON_APB_DPLL_CFG3 */

#define BIT_AON_APB_DPLL_CCS_CTRL(x)                             (((x) & 0xFF))

/* REG_AON_APB_GPLL_CFG3 */

#define BIT_AON_APB_GPLL_CCS_CTRL(x)                             (((x) & 0xFF))

/* REG_AON_APB_CP_DAP_PAD_CTRL */

#define BIT_AON_APB_CP_DAP_PAD_SEL(x)                            (((x) & 0x3))

/* REG_AON_APB_CA53_PROT_CTRL */

#define BIT_AON_APB_CA53_SPNIDEN(x)                              (((x) & 0xF) << 12)
#define BIT_AON_APB_CA53_SPIDEN(x)                               (((x) & 0xF) << 8)
#define BIT_AON_APB_CA53_NIDEN(x)                                (((x) & 0xF) << 4)
#define BIT_AON_APB_CA53_DBGEN(x)                                (((x) & 0xF))

/* REG_AON_APB_CSSYS_CFG */

#define BIT_AON_APB_DAP_DEVICEEN                                 BIT(31)
#define BIT_AON_APB_DAP_DBGEN                                    BIT(30)
#define BIT_AON_APB_DAP_SPIDBGEN                                 BIT(29)
#define BIT_AON_APB_GNSS_CM4_DBGEN                               BIT(11)
#define BIT_AON_APB_BTWF_CM4_DBGEN                               BIT(10)
#define BIT_AON_APB_TG_JTAG_EN                                   BIT(9)
#define BIT_AON_APB_LTE_JTAG_EN                                  BIT(8)
#define BIT_AON_APB_AON_CM4_DBGEN                                BIT(7)
#define BIT_AON_APB_DJTAG_EN                                     BIT(6)
#define BIT_AON_APB_AG_JTAG_EN                                   BIT(5)
#define BIT_AON_APB_MJTAG_EN                                     BIT(4)
#define BIT_AON_APB_CSSYS_NIDEN                                  BIT(3)
#define BIT_AON_APB_CSSYS_SPNIDEN                                BIT(2)
#define BIT_AON_APB_CSSYS_SPIDEN                                 BIT(1)
#define BIT_AON_APB_CSSYS_DBGEN                                  BIT(0)

/* REG_AON_APB_SEC_MUX_DBG_EN */

#define BIT_AON_APB_DAP_DEVICEEN_S                               BIT(25)
#define BIT_AON_APB_DAP_DBGEN_S                                  BIT(24)
#define BIT_AON_APB_DAP_SPIDBGEN_S                               BIT(23)
#define BIT_AON_APB_GNSS_CM4_DBGEN_S                             BIT(19)
#define BIT_AON_APB_BTWF_CM4_DBGEN_S                             BIT(18)
#define BIT_AON_APB_AON_CM4_DBGEN_S                              BIT(17)
#define BIT_AON_APB_CR5_DBGEN_S                                  BIT(16)
#define BIT_AON_APB_CR5_NIDEN_S                                  BIT(15)
#define BIT_AON_APB_CSSYS_DBGEN_S                                BIT(14)
#define BIT_AON_APB_CSSYS_NIDEN_S                                BIT(13)
#define BIT_AON_APB_CSSYS_SPIDEN_S                               BIT(12)
#define BIT_AON_APB_CSSYS_SPNIDEN_S                              BIT(11)
#define BIT_AON_APB_CA53_DBGEN_S                                 BIT(10)
#define BIT_AON_APB_CA53_NIDEN_S                                 BIT(9)
#define BIT_AON_APB_CA53_SPIDEN_S                                BIT(8)
#define BIT_AON_APB_CA53_SPNIDEN_S                               BIT(7)
#define BIT_AON_APB_DJTAG_EN_S                                   BIT(2)
#define BIT_AON_APB_AG_JTAG_EN_S                                 BIT(1)
#define BIT_AON_APB_MJTAG_EN_S                                   BIT(0)

/* REG_AON_APB_CR5_PROT_CTRL */

#define BIT_AON_APB_CR5_NIDEN                                    BIT(1)
#define BIT_AON_APB_CR5_DBGEN                                    BIT(0)

/* REG_AON_APB_DBG_DJTAG_CTRL */

#define BIT_AON_APB_DBGSYS_CSSYS_STM_NSGUAREN                    BIT(0)

/* REG_AON_APB_WTLCP_CTRL */

#define BIT_AON_APB_WTLCP_AON_FRC_WSYS_LT_STOP                   BIT(4)
#define BIT_AON_APB_WTLCP_AON_FRC_WSYS_STOP                      BIT(3)

/* REG_AON_APB_WTL_WCDMA_EB */

#define BIT_AON_APB_WTLCP_WCMDA_EB                               BIT(16)
#define BIT_AON_APB_WCDMA_AUTO_GATE_EN                           BIT(8)

/* REG_AON_APB_WTLCP_LDSP_CTRL0 */

#define BIT_AON_APB_WTLCP_LDSP_BOOT_VECTOR(x)                    (((x) & 0xFFFFFFFF))

/* REG_AON_APB_WTLCP_LDSP_CTRL1 */

#define BIT_AON_APB_WTLCP_STCK_LDSP                              BIT(13)
#define BIT_AON_APB_WTLCP_STMS_LDSP                              BIT(12)
#define BIT_AON_APB_WTLCP_STDO_LDSP                              BIT(11)
#define BIT_AON_APB_WTLCP_STDI_LDSP                              BIT(10)
#define BIT_AON_APB_WTLCP_STRTCK_LDSP                            BIT(9)
#define BIT_AON_APB_WTLCP_SW_JTAG_ENA_LDSP                       BIT(8)
#define BIT_AON_APB_WTLCP_LDSP_EXTERNAL_WAIT                     BIT(1)
#define BIT_AON_APB_WTLCP_LDSP_BOOT                              BIT(0)

/* REG_AON_APB_WTLCP_TDSP_CTRL0 */

#define BIT_AON_APB_WTLCP_TDSP_BOOT_VECTOR(x)                    (((x) & 0xFFFFFFFF))

/* REG_AON_APB_WTLCP_TDSP_CTRL1 */

#define BIT_AON_APB_WTLCP_STCK_TDSP                              BIT(13)
#define BIT_AON_APB_WTLCP_STMS_TDSP                              BIT(12)
#define BIT_AON_APB_WTLCP_STDO_TDSP                              BIT(11)
#define BIT_AON_APB_WTLCP_STDI_TDSP                              BIT(10)
#define BIT_AON_APB_WTLCP_STRTCK_TDSP                            BIT(9)
#define BIT_AON_APB_WTLCP_SW_JTAG_ENA_TDSP                       BIT(8)
#define BIT_AON_APB_WTLCP_TDSP_EXTERNAL_WAIT                     BIT(1)
#define BIT_AON_APB_WTLCP_TDSP_BOOT                              BIT(0)

/* REG_AON_APB_PCP_AON_EB */

#define BIT_AON_APB_PUBCP_SYST_RTC_EB                            BIT(11)
#define BIT_AON_APB_PUBCP_TMR_EB                                 BIT(10)
#define BIT_AON_APB_PUBCP_TMR_RTC_EB                             BIT(9)
#define BIT_AON_APB_PUBCP_SYST_EB                                BIT(8)
#define BIT_AON_APB_PUBCP_WDG_EB                                 BIT(7)
#define BIT_AON_APB_PUBCP_WDG_RTC_EB                             BIT(6)
#define BIT_AON_APB_PUBCP_ARCH_RTC_EB                            BIT(5)
#define BIT_AON_APB_PUBCP_EIC_EB                                 BIT(4)
#define BIT_AON_APB_PUBCP_EIC_RTCDV5_EB                          BIT(3)
#define BIT_AON_APB_PUBCP_EIC_RTC_EB                             BIT(2)

/* REG_AON_APB_PCP_SOFT_RST */

#define BIT_AON_APB_PUBCP_CR5_CORE_SOFT_RST                      BIT(10)
#define BIT_AON_APB_PUBCP_CR5_DBG_SOFT_RST                       BIT(9)
#define BIT_AON_APB_PUBCP_CR5_ETM_SOFT_RST                       BIT(8)
#define BIT_AON_APB_PUBCP_CR5_MP_SOFT_RST                        BIT(7)
#define BIT_AON_APB_PUBCP_CR5_CS_DBG_SOFT_RST                    BIT(6)
#define BIT_AON_APB_PUBCP_TMR_SOFT_RST                           BIT(5)
#define BIT_AON_APB_PUBCP_SYST_SOFT_RST                          BIT(4)
#define BIT_AON_APB_PUBCP_WDG_SOFT_RST                           BIT(3)
#define BIT_AON_APB_PUBCP_EIC_SOFT_RST                           BIT(2)

/* REG_AON_APB_PUBCP_CTRL */

#define BIT_AON_APB_AON_ACCESS_PUBCP                             BIT(13)
#define BIT_AON_APB_PUBCP_CR5_STANDBYWFI_N                       BIT(12)
#define BIT_AON_APB_PUBCP_CR5_STANDBYWFE_N                       BIT(11)
#define BIT_AON_APB_PUBCP_CR5_CLKSTOPPED0_N                      BIT(10)
#define BIT_AON_APB_PUBCP_CR5_L2IDLE                             BIT(9)
#define BIT_AON_APB_PUBCP_CR5_VALIRQ0_N                          BIT(8)
#define BIT_AON_APB_PUBCP_CR5_VALFIQ0_N                          BIT(7)
#define BIT_AON_APB_PUBCP_CR5_STOP                               BIT(6)
#define BIT_AON_APB_PUBCP_CR5_CSYSACK_ATB                        BIT(5)
#define BIT_AON_APB_PUBCP_CR5_CACTIVE_ATB                        BIT(4)
#define BIT_AON_APB_PUBCP_CR5_CSSYNC_REQ                         BIT(3)
#define BIT_AON_APB_PUBCP_CR5_CSYSREQ_ATB                        BIT(2)
#define BIT_AON_APB_PUBCP_CR5_NODBGCLK                           BIT(1)
#define BIT_AON_APB_PUBCP_CR5_CFGEE                              BIT(0)

/* REG_AON_APB_SYS_DBG_SEL */

#define BIT_AON_APB_MDAR_DBG_MOD_SEL(x)                          (((x) & 0xFF) << 24)
#define BIT_AON_APB_MDAR_DBG_SIG_SEL(x)                          (((x) & 0xFF) << 16)

/* REG_AON_APB_MDAR_HSDL_CFG */

#define BIT_AON_APB_MDAR_HSDL_CFG(x)                             (((x) & 0xFFFFFFFF))

/* REG_AON_APB_SYS_DBG_SEL2 */

#define BIT_AON_APB_AON_DBG_MOD_SEL                              BIT(8)
#define BIT_AON_APB_AON_DBG_SIG_SEL(x)                           (((x) & 0xFF))

/* REG_AON_APB_SUBSYS_DBG_CFG */

#define BIT_AON_APB_SUBSYS_DBG_SEL(x)                            (((x) & 0x7))

/* REG_AON_APB_AP_LPC_CTRL */

#define BIT_AON_APB_AP_FRC_STOP_ACK                              BIT(8)
#define BIT_AON_APB_AP_FRC_STOP_REQ                              BIT(0)

/* REG_AON_APB_WTLCP_LPC_CTRL */

#define BIT_AON_APB_WTLCP_FRC_STOP_ACK                           BIT(8)
#define BIT_AON_APB_WTLCP_FRC_STOP_REQ                           BIT(0)

/* REG_AON_APB_PUBCP_LPC_CTRL */

#define BIT_AON_APB_PUBCP_FRC_STOP_ACK                           BIT(8)
#define BIT_AON_APB_PUBCP_FRC_STOP_REQ                           BIT(0)

/* REG_AON_APB_RFTI_SOFT_RST */

#define BIT_AON_APB_RFTI_SOFT_RST                                BIT(4)
#define BIT_AON_APB_LVDSRF_CALI_SOFT_RST                         BIT(3)
#define BIT_AON_APB_RFTI2_LTH_SOFT_RST                           BIT(2)
#define BIT_AON_APB_RFTI1_LTH_SOFT_RST                           BIT(1)
#define BIT_AON_APB_RFTI_SBI_SOFT_RST                            BIT(0)

/* REG_AON_APB_GLB_WCDMA_CTRL */

#define BIT_AON_APB_WTLCP_WCDMA_AUTO_GATE_EN                     BIT(3)
#define BIT_AON_APB_WTLCP_WCDMA_SOFT_GATE_DIS                    BIT(2)
#define BIT_AON_APB_PUBCP_WCDMA_AUTO_GATE_EN                     BIT(1)
#define BIT_AON_APB_PUBCP_WCDMA_SOFT_GATE_DIS                    BIT(0)

/* REG_AON_APB_PLL_CLKOUT_GATE */

#define BIT_AON_APB_LPLL_CLKOUT_EN                               BIT(29)
#define BIT_AON_APB_LPLL_DIV1_EN                                 BIT(28)
#define BIT_AON_APB_LPLL_DIV2_EN                                 BIT(27)
#define BIT_AON_APB_LPLL_DIV3_EN                                 BIT(26)
#define BIT_AON_APB_LPLL_DIV5_EN                                 BIT(25)
#define BIT_AON_APB_TWPLL_CLKOUT_EN                              BIT(24)
#define BIT_AON_APB_TWPLL_DIV1_EN                                BIT(23)
#define BIT_AON_APB_TWPLL_DIV2_EN                                BIT(22)
#define BIT_AON_APB_TWPLL_DIV3_EN                                BIT(21)
#define BIT_AON_APB_TWPLL_DIV5_EN                                BIT(20)
#define BIT_AON_APB_TWPLL_DIV7_EN                                BIT(19)
#define BIT_AON_APB_GPLL_CLKOUT_EN                               BIT(18)
#define BIT_AON_APB_DPLL_CLKOUT_EN                               BIT(17)
#define BIT_AON_APB_MPLL_CLKOUT_EN                               BIT(16)
#define BIT_AON_APB_LPLL_CLKOUT_AUTO_GATE_DIS                    BIT(13)
#define BIT_AON_APB_LPLL_DIV1_AUTO_GATE_DIS                      BIT(12)
#define BIT_AON_APB_LPLL_DIV2_AUTO_GATE_DIS                      BIT(11)
#define BIT_AON_APB_LPLL_DIV3_AUTO_GATE_DIS                      BIT(10)
#define BIT_AON_APB_LPLL_DIV5_AUTO_GATE_DIS                      BIT(9)
#define BIT_AON_APB_TWPLL_CLKOUT_AUTO_GATE_DIS                   BIT(8)
#define BIT_AON_APB_TWPLL_DIV1_AUTO_GATE_DIS                     BIT(7)
#define BIT_AON_APB_TWPLL_DIV2_AUTO_GATE_DIS                     BIT(6)
#define BIT_AON_APB_TWPLL_DIV3_AUTO_GATE_DIS                     BIT(5)
#define BIT_AON_APB_TWPLL_DIV5_AUTO_GATE_DIS                     BIT(4)
#define BIT_AON_APB_TWPLL_DIV7_AUTO_GATE_DIS                     BIT(3)
#define BIT_AON_APB_GPLL_CLKOUT_AUTO_GATE_DIS                    BIT(2)
#define BIT_AON_APB_DPLL_CLKOUT_AUTO_GATE_DIS                    BIT(1)
#define BIT_AON_APB_MPLL_CLKOUT_AUTO_GATE_DIS                    BIT(0)

/* REG_AON_APB_HM_CFG_SEL */

#define BIT_AON_APB_MDAR_PMU_RPLL_CFG_SEL                        BIT(12)
#define BIT_AON_APB_LVDSRF_CFG_SEL                               BIT(11)
#define BIT_AON_APB_LVDSRF_PLL_CFG_SEL                           BIT(10)
#define BIT_AON_APB_RPLL_CFG_SEL                                 BIT(9)
#define BIT_AON_APB_LPLL_CFG_SEL                                 BIT(8)
#define BIT_AON_APB_TWPLL_CFG_SEL                                BIT(7)
#define BIT_AON_APB_GPLL_CFG_SEL                                 BIT(6)
#define BIT_AON_APB_DPLL_CFG_SEL                                 BIT(5)
#define BIT_AON_APB_MPLL_CFG_SEL                                 BIT(4)
#define BIT_AON_APB_AAPC_CFG_SEL                                 BIT(2)
#define BIT_AON_APB_BB_BG_CFG_SEL                                BIT(1)
#define BIT_AON_APB_XTAL_SIN_CFG_SEL                             BIT(0)

/* REG_AON_APB_HM_PWR_CTRL */

#define BIT_AON_APB_CPPLL_PD                                     BIT(12)
#define BIT_AON_APB_LVDSRF_PD                                    BIT(11)
#define BIT_AON_APB_LVDSRF_PLL_PD                                BIT(10)
#define BIT_AON_APB_RPLL_PD                                      BIT(9)
#define BIT_AON_APB_LPLL_PD                                      BIT(8)
#define BIT_AON_APB_TWPLL_PD                                     BIT(7)
#define BIT_AON_APB_GPLL_PD                                      BIT(6)
#define BIT_AON_APB_DPLL_PD                                      BIT(5)
#define BIT_AON_APB_MPLL_PD                                      BIT(4)
#define BIT_AON_APB_AAPC_PD                                      BIT(3)
#define BIT_AON_APB_BB_BG_PD                                     BIT(2)
#define BIT_AON_APB_XTAL_32M_BUF_PD                              BIT(1)
#define BIT_AON_APB_XTAL_26M_BUF_PD                              BIT(0)

/* REG_AON_APB_HM_RST_CTRL */

#define BIT_AON_APB_CPPLL_RST                                    BIT(12)
#define BIT_AON_APB_LVDSRF_RST                                   BIT(11)
#define BIT_AON_APB_LVDSRF_PLL_RST                               BIT(10)
#define BIT_AON_APB_RPLL_RST                                     BIT(9)
#define BIT_AON_APB_LPLL_RST                                     BIT(8)
#define BIT_AON_APB_TWPLL_RST                                    BIT(7)
#define BIT_AON_APB_GPLL_RST                                     BIT(6)
#define BIT_AON_APB_DPLL_RST                                     BIT(5)
#define BIT_AON_APB_MPLL_RST                                     BIT(4)
#define BIT_AON_APB_BB_BG_RST                                    BIT(1)

/* REG_AON_APB_M_AAPC_CFG */

#define BIT_AON_APB_AAPC_G1(x)                                   (((x) & 0x3) << 24)
#define BIT_AON_APB_AAPC_G0(x)                                   (((x) & 0x3) << 22)
#define BIT_AON_APB_AAPC_SEL                                     BIT(21)
#define BIT_AON_APB_AAPC_BPRES                                   BIT(20)
#define BIT_AON_APB_APCOUT_SEL                                   BIT(15)
#define BIT_AON_APB_AAPC_LOW_V_CON                               BIT(14)
#define BIT_AON_APB_AAPC_D(x)                                    (((x) & 0x3FFF))

/* REG_AON_APB_DAP_DJTAG_CTRL */

#define BIT_AON_APB_DAP_DJTAG_EN                                 BIT(0)

/* REG_AON_APB_CGM_REG1 */

#define BIT_AON_APB_LTE_PCCSCC_RFTI_CLK_SW_CFG(x)                (((x) & 0xFF) << 24)
#define BIT_AON_APB_LPLL1_CLKOUT_SW                              BIT(23)
#define BIT_AON_APB_LPLL0_CLKOUT_SW                              BIT(22)

/* REG_AON_APB_CM4_INT_REQ_SRC_EN */

#define BIT_AON_APB_INT_PUB_HARDWARE_DFS_EXIT_EN                 BIT(31)
#define BIT_AON_APB_INT_PUB_DFS_COMPLET_EN                       BIT(30)
#define BIT_AON_APB_INT_PUB_DFS_ERROR_EN                         BIT(29)
#define BIT_AON_APB_INT_SLV_FW_AP_EN                             BIT(28)
#define BIT_AON_APB_INT_MEM_FW_PUB_EN                            BIT(27)
#define BIT_AON_APB_INT_SLV_FW_AON_EN                            BIT(26)
#define BIT_AON_APB_INT_MEM_FW_AON_EN                            BIT(25)
#define BIT_AON_APB_INT_BUSMON_PUB_EN                            BIT(24)
#define BIT_AON_APB_INT_BUSMON_AP_EN                             BIT(23)
#define BIT_AON_APB_INT_BUSMON_WTLCP_EN                          BIT(22)
#define BIT_AON_APB_INT_BUSMON_PUBCP_EN                          BIT(21)
#define BIT_AON_APB_INT_BUSMON_WCN_EN                            BIT(20)
#define BIT_AON_APB_INT_WTLCP_LTE_WDG_RST_EN                     BIT(19)
#define BIT_AON_APB_INT_WTLCP_TG_WDG_RST_EN                      BIT(18)
#define BIT_AON_APB_INT_PCP_WDG_EN                               BIT(17)
#define BIT_AON_APB_INT_WCN_BTWF_WDG_EN                          BIT(16)
#define BIT_AON_APB_INT_WCN_GNSS_WDG_EN                          BIT(15)
#define BIT_AON_APB_INT_PWR_DOWN_ALL_EN                          BIT(14)
#define BIT_AON_APB_INT_PWR_UP_ALL_EN                            BIT(13)
#define BIT_AON_APB_INT_AP_WDG_EN                                BIT(12)
#define BIT_AON_APB_INT_CA53_WDG_EN                              BIT(11)
#define BIT_AON_APB_INT_SEC_TMR_EN                               BIT(10)
#define BIT_AON_APB_INT_SEC_WDG_EN                               BIT(9)
#define BIT_AON_APB_INT_SEC_RTC_EN                               BIT(8)
#define BIT_AON_APB_INT_SEC_REQ_DMA_EN                           BIT(7)
#define BIT_AON_APB_INT_CHN_START_CHN3_EN                        BIT(6)
#define BIT_AON_APB_INT_CHN_START_CHN2_EN                        BIT(5)
#define BIT_AON_APB_INT_CHN_START_CHN1_EN                        BIT(4)
#define BIT_AON_APB_INT_CHN_START_CHN0_EN                        BIT(3)
#define BIT_AON_APB_INT_EIC_NON_LAT_EN                           BIT(2)
#define BIT_AON_APB_INT_SEC_GPIO_EN                              BIT(1)
#define BIT_AON_APB_INT_ADI_EN                                   BIT(0)

/* REG_AON_APB_CM4_INT_REQ_SRC_EN1 */

#define BIT_AON_APB_INT_REQ_SEC_EIC_EN                           BIT(15)
#define BIT_AON_APB_INT_REQ_MDAR_AP_EN                           BIT(14)
#define BIT_AON_APB_INT_REQ_DISPC_EN                             BIT(13)
#define BIT_AON_APB_INT_REQ_GSP_EN                               BIT(12)
#define BIT_AON_APB_INT_ANA_EN                                   BIT(11)
#define BIT_AON_APB_INT_DFI_BUS_MONITOR_EN                       BIT(10)
#define BIT_AON_APB_INT_CM4_GPIO_EN                              BIT(9)
#define BIT_AON_APB_INT_AON_DMA_CM4_EN                           BIT(8)
#define BIT_AON_APB_INT_MBOX_SRC_CM4_EN                          BIT(7)
#define BIT_AON_APB_INT_MBOX_TAR_CM4_EN                          BIT(6)
#define BIT_AON_APB_INT_CM4_WDG_EN                               BIT(5)
#define BIT_AON_APB_INT_CM4_UART_EN                              BIT(4)
#define BIT_AON_APB_INT_CM4_SYST_EN                              BIT(3)
#define BIT_AON_APB_INT_CM4_TMR_EN                               BIT(2)
#define BIT_AON_APB_INT_PWR_UP_PUB_EN                            BIT(1)
#define BIT_AON_APB_INT_PWR_UP_AP_EN                             BIT(0)

/* REG_AON_APB_AON_MTX_MAIN_LPC_CTRL */

#define BIT_AON_APB_AON_MTX_MAIN_LP_NUM(x)                       (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_AON_MTX_MAIN_LP_EB                           BIT(0)

/* REG_AON_APB_AON_MTX_M0_LPC_CTRL */

#define BIT_AON_APB_AON_MTX_M0_LP_NUM(x)                         (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_AON_MTX_M0_LP_EB                             BIT(0)

/* REG_AON_APB_AON_MTX_M1_LPC_CTRL */

#define BIT_AON_APB_AON_MTX_M1_LP_NUM(x)                         (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_AON_MTX_M1_LP_EB                             BIT(0)

/* REG_AON_APB_AON_MTX_M2_LPC_CTRL */

#define BIT_AON_APB_AON_MTX_M2_LP_NUM(x)                         (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_AON_MTX_M2_LP_EB                             BIT(0)

/* REG_AON_APB_AON_MTX_M3_LPC_CTRL */

#define BIT_AON_APB_AON_MTX_M3_LP_NUM(x)                         (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_AON_MTX_M3_LP_EB                             BIT(0)

/* REG_AON_APB_AON_MTX_M4_LPC_CTRL */

#define BIT_AON_APB_AON_MTX_M4_LP_NUM(x)                         (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_AON_MTX_M4_LP_EB                             BIT(0)

/* REG_AON_APB_AON_MTX_M5_LPC_CTRL */

#define BIT_AON_APB_AON_MTX_M5_LP_NUM(x)                         (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_AON_MTX_M5_LP_EB                             BIT(0)

/* REG_AON_APB_AON_MTX_S0_LPC_CTRL */

#define BIT_AON_APB_AON_MTX_S0_LP_NUM(x)                         (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_AON_MTX_S0_LP_EB                             BIT(0)

/* REG_AON_APB_AON_MTX_S1_LPC_CTRL */

#define BIT_AON_APB_AON_MTX_S1_LP_NUM(x)                         (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_AON_MTX_S1_LP_EB                             BIT(0)

/* REG_AON_APB_AON_MTX_S2_LPC_CTRL */

#define BIT_AON_APB_AON_MTX_S2_LP_NUM(x)                         (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_AON_MTX_S2_LP_EB                             BIT(0)

/* REG_AON_APB_AON_MTX_S3_LPC_CTRL */

#define BIT_AON_APB_AON_MTX_S3_LP_NUM(x)                         (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_AON_MTX_S3_LP_EB                             BIT(0)

/* REG_AON_APB_AON_MTX_S4_LPC_CTRL */

#define BIT_AON_APB_AON_MTX_S4_LP_NUM(x)                         (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_AON_MTX_S4_LP_EB                             BIT(0)

/* REG_AON_APB_AON_MTX_S5_LPC_CTRL */

#define BIT_AON_APB_AON_MTX_S5_LP_NUM(x)                         (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_AON_MTX_S5_LP_EB                             BIT(0)

/* REG_AON_APB_AON_MTX_S6_LPC_CTRL */

#define BIT_AON_APB_AON_MTX_S6_LP_NUM(x)                         (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_AON_MTX_S6_LP_EB                             BIT(0)

/* REG_AON_APB_AON_MTX_S7_LPC_CTRL */

#define BIT_AON_APB_AON_MTX_S7_LP_NUM(x)                         (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_AON_MTX_S7_LP_EB                             BIT(0)

/* REG_AON_APB_GPU2DDR_LPC_CTRL */

#define BIT_AON_APB_GPU2DDR_ASYNC_BRIDGE_LP_NUM(x)               (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_GPU2DDR_ASYNC_BRIDGE_LP_EB                   BIT(0)

/* REG_AON_APB_WTLCP2DDR_LPC_CTRL */

#define BIT_AON_APB_WTLCP2DDR_ASYNC_BRIDGE_LP_NUM(x)             (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_WTLCP2DDR_ASYNC_BRIDGE_LP_EB                 BIT(0)

/* REG_AON_APB_WCN2DDR_LPC_CTRL */

#define BIT_AON_APB_WCN2DDR_ASYNC_BRIDGE_LP_NUM(x)               (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_WCN2DDR_ASYNC_BRIDGE_LP_EB                   BIT(0)

/* REG_AON_APB_MM2DDR_LPC_CTRL */

#define BIT_AON_APB_MM2DDR_ASYNC_BRIDGE_LP_NUM(x)                (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_MM2DDR_ASYNC_BRIDGE_LP_EB                    BIT(0)

/* REG_AON_APB_AON2DDR_LPC_CTRL */

#define BIT_AON_APB_AON2DDR_ASYNC_BRIDGE_LP_NUM(x)               (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_AON2DDR_ASYNC_BRIDGE_LP_EB                   BIT(0)

/* REG_AON_APB_CM42AON_LPC_CTRL */

#define BIT_AON_APB_CM4_TO_AON_AXI_LP_NUM(x)                     (((x) & 0xFFFF) << 1)
#define BIT_AON_APB_CM4_TO_AON_AXI_LP_EB                         BIT(0)

/* REG_AON_APB_OVERHEAT_CTRL */

#define BIT_AON_APB_THM1_INT_ADIE_EN                             BIT(5)
#define BIT_AON_APB_THM0_INT_ADIE_EN                             BIT(4)
#define BIT_AON_APB_THM1_OVERHEAT_ALARM_ADIE_EN                  BIT(3)
#define BIT_AON_APB_THM0_OVERHEAT_ALARM_ADIE_EN                  BIT(2)
#define BIT_AON_APB_THM1_OVERHEAT_RST_DDIE_EN                    BIT(1)
#define BIT_AON_APB_THM0_OVERHEAT_RST_DDIE_EN                    BIT(0)

/* REG_AON_APB_SKLE_TMP0 */

#define BIT_AON_APB_SKLE_TMP0(x)                                 (((x) & 0xFFFFFFFF))

/* REG_AON_APB_SKLE_TMP1 */

#define BIT_AON_APB_SKLE_TMP1(x)                                 (((x) & 0xFFFFFFFF))

/* REG_AON_APB_SKLE_TMP2 */

#define BIT_AON_APB_SKLE_TMP2(x)                                 (((x) & 0xFFFFFFFF))

/* REG_AON_APB_SKLE_TMP3 */

#define BIT_AON_APB_SKLE_TMP3(x)                                 (((x) & 0xFFFFFFFF))

/* REG_AON_APB_DBG_PWRUP_SEL */

#define BIT_AON_APB_DBG_PWRUP_SEL(x)                             (((x) & 0x3))

/* REG_AON_APB_RVBAADDR0 */

#define BIT_AON_APB_RVBAADDR0(x)                                 (((x) & 0x7FFFFFFF))

/* REG_AON_APB_RVBAADDR1 */

#define BIT_AON_APB_RVBAADDR1(x)                                 (((x) & 0x7FFFFFFF))

/* REG_AON_APB_RVBAADDR2 */

#define BIT_AON_APB_RVBAADDR2(x)                                 (((x) & 0x7FFFFFFF))

/* REG_AON_APB_RVBAADDR3 */

#define BIT_AON_APB_RVBAADDR3(x)                                 (((x) & 0x7FFFFFFF))

/* REG_AON_APB_AA64N32 */

#define BIT_AON_APB_AA64NAA32(x)                                 (((x) & 0xF))

/* REG_AON_APB_CA53_EDBGRQ */

#define BIT_AON_APB_CA53_EDBGRQ(x)                               (((x) & 0xF))

/* REG_AON_APB_PUBCP_SIM1_TOP_CTRL */

#define BIT_AON_APB_CP_SIM1_CLK_PL                               BIT(5)
#define BIT_AON_APB_CP_SIM1_DETECT_EN                            BIT(4)
#define BIT_AON_APB_CP_BAT1_DETECT_EN                            BIT(3)
#define BIT_AON_APB_CP_SIM1_DETECT_POL                           BIT(2)
#define BIT_AON_APB_CP_BAT1_DETECT_POL                           BIT(1)
#define BIT_AON_APB_CP_SIM1_OFF_PD_EN                            BIT(0)

/* REG_AON_APB_PUBCP_SIM2_TOP_CTRL */

#define BIT_AON_APB_CP_SIM2_CLK_PL                               BIT(5)
#define BIT_AON_APB_CP_SIM2_DETECT_EN                            BIT(4)
#define BIT_AON_APB_CP_BAT2_DETECT_EN                            BIT(3)
#define BIT_AON_APB_CP_SIM2_DETECT_POL                           BIT(2)
#define BIT_AON_APB_CP_BAT2_DETECT_POL                           BIT(1)
#define BIT_AON_APB_CP_SIM2_OFF_PD_EN                            BIT(0)

/* REG_AON_APB_PUBCP_SIM3_TOP_CTRL */

#define BIT_AON_APB_CP_SIM3_CLK_PL                               BIT(5)
#define BIT_AON_APB_CP_SIM3_DETECT_EN                            BIT(4)
#define BIT_AON_APB_CP_BAT3_DETECT_EN                            BIT(3)
#define BIT_AON_APB_CP_SIM3_DETECT_POL                           BIT(2)
#define BIT_AON_APB_CP_BAT3_DETECT_POL                           BIT(1)
#define BIT_AON_APB_CP_SIM3_OFF_PD_EN                            BIT(0)

/* REG_AON_APB_AP_SIM_TOP_CTRL */

#define BIT_AON_APB_AP_SIM_CLK_PL                                BIT(5)
#define BIT_AON_APB_AP_SIM_DETECT_EN                             BIT(4)
#define BIT_AON_APB_AP_BAT_DETECT_EN                             BIT(3)
#define BIT_AON_APB_AP_SIM_DETECT_POL                            BIT(2)
#define BIT_AON_APB_AP_BAT_DETECT_POL                            BIT(1)
#define BIT_AON_APB_AP_SIM_OFF_PD_EN                             BIT(0)

/* REG_AON_APB_SYS_DEBUG_BUS_SEL_CFG */

#define BIT_AON_APB_REG_DBG_BUS_SEL_AP(x)                        (((x) & 0xFF) << 24)
#define BIT_AON_APB_REG_DBG_BUS_SEL_PUBCP(x)                     (((x) & 0xFF) << 16)
#define BIT_AON_APB_REG_DBG_BUS_SEL_WTLCP(x)                     (((x) & 0xFF) << 8)
#define BIT_AON_APB_REG_DBG_BUS_SEL_WCN(x)                       (((x) & 0xFF))

/* REG_AON_APB_SYS_DEBUG_BUS_SEL_CFG1 */

#define BIT_AON_APB_REG_DBG_BUS_SEL_PUB(x)                       (((x) & 0xFF) << 24)
#define BIT_AON_APB_REG_DBG_BUS_SEL_MM(x)                        (((x) & 0xFF) << 16)
#define BIT_AON_APB_REG_DBG_BUS_SEL_AON(x)                       (((x) & 0xFF) << 8)
#define BIT_AON_APB_REG_DBG_BUS_SEL_MDAR(x)                      (((x) & 0xFF))

/* REG_AON_APB_SYS_DEBUG_BUS_SEL_CFG2 */

#define BIT_AON_APB_REG_DBG_MOD_SEL_WTLCP(x)                     (((x) & 0xFF) << 16)
#define BIT_AON_APB_REG_DBG_MOD_SEL_PUBCP(x)                     (((x) & 0xFF) << 8)
#define BIT_AON_APB_REG_DBG_SYS_SEL(x)                           (((x) & 0xFF))

/* REG_AON_APB_PAD_DBG_BUS_SEL_CFG1 */

#define BIT_AON_APB_DBG_BUS5_SEL(x)                              (((x) & 0x1F) << 25)
#define BIT_AON_APB_DBG_BUS4_SEL(x)                              (((x) & 0x1F) << 20)
#define BIT_AON_APB_DBG_BUS3_SEL(x)                              (((x) & 0x1F) << 15)
#define BIT_AON_APB_DBG_BUS2_SEL(x)                              (((x) & 0x1F) << 10)
#define BIT_AON_APB_DBG_BUS1_SEL(x)                              (((x) & 0x1F) << 5)
#define BIT_AON_APB_DBG_BUS0_SEL(x)                              (((x) & 0x1F))

/* REG_AON_APB_PAD_DBG_BUS_SEL_CFG2 */

#define BIT_AON_APB_DBG_BUS11_SEL(x)                             (((x) & 0x1F) << 25)
#define BIT_AON_APB_DBG_BUS10_SEL(x)                             (((x) & 0x1F) << 20)
#define BIT_AON_APB_DBG_BUS9_SEL(x)                              (((x) & 0x1F) << 15)
#define BIT_AON_APB_DBG_BUS8_SEL(x)                              (((x) & 0x1F) << 10)
#define BIT_AON_APB_DBG_BUS7_SEL(x)                              (((x) & 0x1F) << 5)
#define BIT_AON_APB_DBG_BUS6_SEL(x)                              (((x) & 0x1F))

/* REG_AON_APB_PAD_DBG_BUS_SEL_CFG3 */

#define BIT_AON_APB_DBG_BUS15_SEL(x)                             (((x) & 0x1F) << 15)
#define BIT_AON_APB_DBG_BUS14_SEL(x)                             (((x) & 0x1F) << 10)
#define BIT_AON_APB_DBG_BUS13_SEL(x)                             (((x) & 0x1F) << 5)
#define BIT_AON_APB_DBG_BUS12_SEL(x)                             (((x) & 0x1F))

/* REG_AON_APB_BONDING_OPTION */

#define BIT_AON_APB_BONDING_OPTION(x)                            (((x) & 0xFFFFFFFF))

/* REG_AON_APB_CE_LIFE_CYCLE */

#define BIT_AON_APB_CE_LIFE_CYCLE(x)                             (((x) & 0xFFFFFFFF))

/* REG_AON_APB_WCN_ADC_CLK_SEL */

#define BIT_AON_APB_WCN_OSCADC_CLK_SEL                           BIT(1)
#define BIT_AON_APB_WCN_TSEADC_CLK_SEL                           BIT(0)

/* REG_AON_APB_CHIP_TOP_PLL_CNT_DONE */

#define BIT_AON_APB_CHIP_TOP_PLL_CNT_DONE                        BIT(0)

/* REG_AON_APB_WCN_SYS_CFG1 */

#define BIT_AON_APB_WCN_CM4_ADDR_REMAP(x)                        (((x) & 0x3) << 24)
#define BIT_AON_APB_WCN_GNSS_CM4_ADDR_OFFSET(x)                  (((x) & 0xFFFFFF))

/* REG_AON_APB_WCN_SYS_CFG2 */

#define BIT_AON_APB_WCN_RD_XTAL_REQ_SEL(x)                       (((x) & 0x3) << 8)
#define BIT_AON_APB_WCN_BTWF_SYS_EN                              BIT(7)
#define BIT_AON_APB_WCN_GNSS_SYS_EN                              BIT(6)
#define BIT_AON_APB_WCN_BTWF_CM4_DBGRESTART                      BIT(5)
#define BIT_AON_APB_WCN_BTWF_CM4_EDBGREQ                         BIT(4)
#define BIT_AON_APB_WCN_OFFCHIP_XTRL_EN                          BIT(3)
#define BIT_AON_APB_WCN_XTAL_DIF_SEL_H                           BIT(2)
#define BIT_AON_APB_WCN_GNSS_CM4_DBGRESTART                      BIT(1)
#define BIT_AON_APB_WCN_GNSS_CM4_EDBGREQ                         BIT(0)

/* REG_AON_APB_AP_CORE_CFG */

#define BIT_AON_APB_DBGL1RSTDISABLE                              BIT(1)
#define BIT_AON_APB_CA53_L2RSTDISABLE                            BIT(0)

/* REG_AON_APB_CM4_MPU_DISABLE */

#define BIT_AON_APB_MPUDISABLE                                   BIT(0)

/* REG_AON_APB_CM4_STATUS */

#define BIT_AON_APB_CM4_SLEEPING_STAT                            BIT(1)
#define BIT_AON_APB_CM4_LOCKUP_STAT                              BIT(0)

/* REG_AON_APB_MBIST_EFUSE_CTRL */

#define BIT_AON_APB_FUSEBOX_SELECT_BUFFER_SW                     BIT(3)
#define BIT_AON_APB_EFUSE_MUX_SEL_SW_DEFUALT0                    BIT(2)
#define BIT_AON_APB_EFUSE_MUX_SEL_SW_DEFUALT1                    BIT(1)
#define BIT_AON_APB_EFUSE_MUX_SEL_SW                             BIT(0)

/* REG_AON_APB_AON_APB_FREQ_CTRL */

#define BIT_AON_APB_AON_APB_MASTER_BUSY(x)                       (((x) & 0x3F) << 2)
#define BIT_AON_APB_AON_APB_FREQ_CTRL_EN                         BIT(1)
#define BIT_AON_APB_AON_APB_IDLE_EN                              BIT(0)

/* REG_AON_APB_CA53_PROT_CTRL_NON_SEC */

#define BIT_AON_APB_CA53_SPNIDEN_NON_SEC(x)                      (((x) & 0xF) << 12)
#define BIT_AON_APB_CA53_SPIDEN_NON_SEC(x)                       (((x) & 0xF) << 8)
#define BIT_AON_APB_CA53_NIDEN_NON_SEC(x)                        (((x) & 0xF) << 4)
#define BIT_AON_APB_CA53_DBGEN_NON_SEC(x)                        (((x) & 0xF))

/* REG_AON_APB_CSSYS_CFG_NON_SEC */

#define BIT_AON_APB_DAP_DEVICEEN_NON_SEC                         BIT(31)
#define BIT_AON_APB_DAP_DBGEN_NON_SEC                            BIT(30)
#define BIT_AON_APB_DAP_SPIDBGEN_NON_SEC                         BIT(29)
#define BIT_AON_APB_GNSS_CM4_DBGEN_NON_SEC                       BIT(11)
#define BIT_AON_APB_BTWF_CM4_DBGEN_NON_SEC                       BIT(10)
#define BIT_AON_APB_TG_JTAG_EN_NON_SEC                           BIT(9)
#define BIT_AON_APB_LTE_JTAG_EN_NON_SEC                          BIT(8)
#define BIT_AON_APB_AON_CM4_DBGEN_NON_SEC                        BIT(7)
#define BIT_AON_APB_DJTAG_EN_NON_SEC                             BIT(6)
#define BIT_AON_APB_AG_JTAG_EN_NON_SEC                           BIT(5)
#define BIT_AON_APB_MJTAG_EN_NON_SEC                             BIT(4)
#define BIT_AON_APB_CSSYS_NIDEN_NON_SEC                          BIT(3)
#define BIT_AON_APB_CSSYS_SPNIDEN_NON_SEC                        BIT(2)
#define BIT_AON_APB_CSSYS_SPIDEN_NON_SEC                         BIT(1)
#define BIT_AON_APB_CSSYS_DBGEN_NON_SEC                          BIT(0)

/* REG_AON_APB_CR5_PROT_CTRL_NON_SEC */

#define BIT_AON_APB_CR5_NIDEN_NON_SEC                            BIT(1)
#define BIT_AON_APB_CR5_DBGEN_NON_SEC                            BIT(0)

/* REG_AON_APB_PAD_DBG_BUS_SEL_CFG4 */

#define BIT_AON_APB_DBG_BUS21_SEL(x)                             (((x) & 0x1F) << 25)
#define BIT_AON_APB_DBG_BUS20_SEL(x)                             (((x) & 0x1F) << 20)
#define BIT_AON_APB_DBG_BUS19_SEL(x)                             (((x) & 0x1F) << 15)
#define BIT_AON_APB_DBG_BUS18_SEL(x)                             (((x) & 0x1F) << 10)
#define BIT_AON_APB_DBG_BUS17_SEL(x)                             (((x) & 0x1F) << 5)
#define BIT_AON_APB_DBG_BUS16_SEL(x)                             (((x) & 0x1F))

/* REG_AON_APB_PAD_DBG_BUS_SEL_CFG5 */

#define BIT_AON_APB_DBG_BUS27_SEL(x)                             (((x) & 0x1F) << 25)
#define BIT_AON_APB_DBG_BUS26_SEL(x)                             (((x) & 0x1F) << 20)
#define BIT_AON_APB_DBG_BUS25_SEL(x)                             (((x) & 0x1F) << 15)
#define BIT_AON_APB_DBG_BUS24_SEL(x)                             (((x) & 0x1F) << 10)
#define BIT_AON_APB_DBG_BUS23_SEL(x)                             (((x) & 0x1F) << 5)
#define BIT_AON_APB_DBG_BUS22_SEL(x)                             (((x) & 0x1F))

/* REG_AON_APB_PAD_DBG_BUS_SEL_CFG6 */

#define BIT_AON_APB_DBG_BUS31_SEL(x)                             (((x) & 0x1F) << 15)
#define BIT_AON_APB_DBG_BUS30_SEL(x)                             (((x) & 0x1F) << 10)
#define BIT_AON_APB_DBG_BUS29_SEL(x)                             (((x) & 0x1F) << 5)
#define BIT_AON_APB_DBG_BUS28_SEL(x)                             (((x) & 0x1F))

/* REG_AON_APB_INT_REQ_PWR_UP_FLAG */

#define BIT_AON_APB_INT_REQ_PWR_UP(x)                            (((x) & 0x3FF))

/* REG_AON_APB_INT_REQ_PWR_DOWN_FLAG */

#define BIT_AON_APB_INT_REQ_PWR_DOWN(x)                          (((x) & 0x3FF))

/* REG_AON_APB_IO_DLY_CTRL */

#define BIT_AON_APB_CLK_CCIR_DLY_SEL(x)                          (((x) & 0xF) << 8)
#define BIT_AON_APB_CLK_PUBCPDSP_DLY_SEL(x)                      (((x) & 0xF) << 4)
#define BIT_AON_APB_CLK_WTLCPDSP_DLY_SEL(x)                      (((x) & 0xF))

/* REG_AON_APB_PMU_RST_MONITOR */

#define BIT_AON_APB_PMU_RST_MONITOR(x)                           (((x) & 0xFFFFFFFF))

/* REG_AON_APB_THM_RST_MONITOR */

#define BIT_AON_APB_THM_RST_MONITOR(x)                           (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AP_RST_MONITOR */

#define BIT_AON_APB_AP_RST_MONITOR(x)                            (((x) & 0xFFFFFFFF))

/* REG_AON_APB_CA53_RST_MONITOR */

#define BIT_AON_APB_CA53_RST_MONITOR(x)                          (((x) & 0xFFFFFFFF))

/* REG_AON_APB_BOND_OPT0 */

#define BIT_AON_APB_BOND_OPTION0(x)                              (((x) & 0xFFFFFFFF))

/* REG_AON_APB_RES_REG0 */

#define BIT_AON_APB_DJTAG_TCK_EB                                 BIT(10)

/* REG_AON_APB_RES_REG1 */

#define BIT_AON_APB_RES_REG1(x)                                  (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AON_QOS_CFG */

#define BIT_AON_APB_QOS_R_GPU(x)                                 (((x) & 0xF) << 12)
#define BIT_AON_APB_QOS_W_GPU(x)                                 (((x) & 0xF) << 8)
#define BIT_AON_APB_QOS_R_GSP(x)                                 (((x) & 0xF) << 4)
#define BIT_AON_APB_QOS_W_GSP(x)                                 (((x) & 0xF))

/* REG_AON_APB_BB_LDO_CAL_START */

#define BIT_AON_APB_BB_LDO_CAL_START                             BIT(0)

/* REG_AON_APB_AON_MTX_PROT_CFG */

#define BIT_AON_APB_HPROT_DMAW(x)                                (((x) & 0xF) << 4)
#define BIT_AON_APB_HPROT_DMAR(x)                                (((x) & 0xF))

/* REG_AON_APB_LVDS_CFG */

#define BIT_AON_APB_LVDSDIS_TXCLKDATA(x)                         (((x) & 0x7F) << 16)
#define BIT_AON_APB_LVDSDIS_TXCOM(x)                             (((x) & 0x3) << 12)
#define BIT_AON_APB_LVDSDIS_TXSLEW(x)                            (((x) & 0x3) << 10)
#define BIT_AON_APB_LVDSDIS_TXSW(x)                              (((x) & 0x3) << 8)
#define BIT_AON_APB_LVDSDIS_TXRERSER(x)                          (((x) & 0x1F) << 3)
#define BIT_AON_APB_LVDSDIS_PRE_EMP(x)                           (((x) & 0x3) << 1)
#define BIT_AON_APB_LVDSDIS_TXPD                                 BIT(0)

/* REG_AON_APB_PLL_LOCK_OUT_SEL */

#define BIT_AON_APB_SLEEP_PLLLOCK_SEL                            BIT(7)
#define BIT_AON_APB_PLL_LOCK_SEL(x)                              (((x) & 0x7) << 4)
#define BIT_AON_APB_SLEEP_DBG_SEL(x)                             (((x) & 0xF))

/* REG_AON_APB_FUNCTST_CTRL_0 */

#define BIT_AON_APB_FUNCTST_CTRL_0(x)                            (((x) & 0xFFFFFFFF))

/* REG_AON_APB_FUNCTST_CTRL_1 */

#define BIT_AON_APB_FUNCTST_CTRL_1(x)                            (((x) & 0xFFFFFFFF))

/* REG_AON_APB_FUNCTST_CTRL_2 */

#define BIT_AON_APB_FUNCTST_CTRL_2(x)                            (((x) & 0xFFFFFFFF))

/* REG_AON_APB_WDG_RST_FLAG */

#define BIT_AON_APB_PCP_WDG_RST_FLAG                             BIT(5)
#define BIT_AON_APB_WTLCP_LTE_WDG_RST_FLAG                       BIT(4)
#define BIT_AON_APB_WTLCP_TG_WDG_RST_FLAG                        BIT(3)
#define BIT_AON_APB_CA53_WDG_RST_FLAG                            BIT(1)
#define BIT_AON_APB_SEC_WDG_RST_FLAG                             BIT(0)

/* REG_AON_APB_CA53_CFG */

#define BIT_AON_APB_READ_ALLOC_MODE_SPRD(x)                      (((x) & 0xF))

/* REG_AON_APB_RES_REG2 */

#define BIT_AON_APB_RES_REG2(x)                                  (((x) & 0xFFFFFFFF))

/* REG_AON_APB_RES_REG3 */

#define BIT_AON_APB_RES_REG3(x)                                  (((x) & 0xFFFFFFFF))

/* REG_AON_APB_RES_REG4 */

#define BIT_AON_APB_RES_REG4(x)                                  (((x) & 0xFFFFFFFF))

/* REG_AON_APB_RES_REG5 */

#define BIT_AON_APB_RES_REG5(x)                                  (((x) & 0xFFFFFFFF))

/* REG_AON_APB_RES_REG6 */

#define BIT_AON_APB_RES_REG6(x)                                  (((x) & 0xFFFFFFFF))

/* REG_AON_APB_RES_REG7 */

#define BIT_AON_APB_RES_REG7(x)                                  (((x) & 0xFFFFFFFF))

/* REG_AON_APB_AON_APB_RSV */

#define BIT_AON_APB_AON_APB_RSV(x)                               (((x) & 0xFFFFFFFF))

/* REG_AON_APB_FUNCTION_DMA_BOOT_ADDR */

#define BIT_AON_APB_FUNCTION_DMA_BOOT_ADDR(x)                    (((x) & 0xFFFFFFFF))


#endif /* AON_APB_H */

