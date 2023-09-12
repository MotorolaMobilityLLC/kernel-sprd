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


#ifndef AON_CLK_CORE_H
#define AON_CLK_CORE_H



#define REG_AON_CLK_CORE_CGM_AON_APB_CFG          (0x0020)
#define REG_AON_CLK_CORE_CGM_ADI_CFG              (0x0024)
#define REG_AON_CLK_CORE_CGM_AUX0_CFG             (0x0028)
#define REG_AON_CLK_CORE_CGM_AUX1_CFG             (0x002C)
#define REG_AON_CLK_CORE_CGM_AUX2_CFG             (0x0030)
#define REG_AON_CLK_CORE_CGM_PROBE_CFG            (0x0034)
#define REG_AON_CLK_CORE_CGM_PWM0_CFG             (0x0038)
#define REG_AON_CLK_CORE_CGM_PWM1_CFG             (0x003C)
#define REG_AON_CLK_CORE_CGM_PWM2_CFG             (0x0040)
#define REG_AON_CLK_CORE_CGM_PWM3_CFG             (0x0044)
#define REG_AON_CLK_CORE_CGM_EFUSE_CFG            (0x0048)
#define REG_AON_CLK_CORE_CGM_CM4_UART_CFG         (0x004C)
#define REG_AON_CLK_CORE_CGM_32K_CFG              (0x0050)
#define REG_AON_CLK_CORE_CGM_1K_CFG               (0x0054)
#define REG_AON_CLK_CORE_CGM_THM0_CFG             (0x0058)
#define REG_AON_CLK_CORE_CGM_THM1_CFG             (0x005C)
#define REG_AON_CLK_CORE_CGM_AUD_CFG              (0x0060)
#define REG_AON_CLK_CORE_CGM_AUDIF_CFG            (0x0064)
#define REG_AON_CLK_CORE_CGM_VBC_CFG              (0x0068)
#define REG_AON_CLK_CORE_CGM_AUD_IIS_DA0_CFG      (0x006C)
#define REG_AON_CLK_CORE_CGM_AUD_IIS0_AD0_CFG     (0x0070)
#define REG_AON_CLK_CORE_CGM_CA53_DAP_CFG         (0x0074)
#define REG_AON_CLK_CORE_CGM_CA53_DAP_MTCK_CFG    (0x0078)
#define REG_AON_CLK_CORE_CGM_CA53_TS_CFG          (0x007C)
#define REG_AON_CLK_CORE_CGM_DJTAG_TCK_CFG        (0x0080)
#define REG_AON_CLK_CORE_CGM_CM4_AHB_CFG          (0x0084)
#define REG_AON_CLK_CORE_CGM_FUNCDMA_CFG          (0x0088)
#define REG_AON_CLK_CORE_CGM_EMC_REF_CFG          (0x008C)
#define REG_AON_CLK_CORE_CGM_CSSYS_CFG            (0x0090)
#define REG_AON_CLK_CORE_CGM_TMR_CFG              (0x0094)
#define REG_AON_CLK_CORE_CGM_PMU_CFG              (0x0098)
#define REG_AON_CLK_CORE_CGM_WCDMA_CFG            (0x009C)
#define REG_AON_CLK_CORE_CGM_DSI_TEST_CFG         (0x00A0)
#define REG_AON_CLK_CORE_CGM_RFTI_SBI_CFG         (0x00A4)
#define REG_AON_CLK_CORE_CGM_RFTI1_XO_CFG         (0x00A8)
#define REG_AON_CLK_CORE_CGM_RFTI_LTH_CFG         (0x00AC)
#define REG_AON_CLK_CORE_CGM_RFTI2_XO_CFG         (0x00B0)
#define REG_AON_CLK_CORE_CGM_LVDSRF_CALI_CFG      (0x00B4)
#define REG_AON_CLK_CORE_CGM_SERDES_DPHY_APB_CFG  (0x00B8)
#define REG_AON_CLK_CORE_CGM_SERDES_DPHY_REF_CFG  (0x00BC)
#define REG_AON_CLK_CORE_CGM_SERDES_DPHY_CFG_CFG  (0x00C0)
#define REG_AON_CLK_CORE_CGM_ANALOG_IO_APB_CFG    (0x00C4)
#define REG_AON_CLK_CORE_CGM_DJTAG_TCK_HW_CFG     (0x00C8)
#define REG_AON_CLK_CORE_CGM_AP_MM_CFG            (0x00CC)
#define REG_AON_CLK_CORE_CGM_AP_AXI_CFG           (0x00D0)
#define REG_AON_CLK_CORE_CGM_WCN_CFG              (0x00D4)
#define REG_AON_CLK_CORE_CGM_NIC_GPU_CFG          (0x00D8)
#define REG_AON_CLK_CORE_CGM_MM_ISP_CFG           (0x00DC)
#define REG_AON_CLK_CORE_CGM_WTL_BRIDGE_CFG       (0x00E0)

/* REG_AON_CLK_CORE_CGM_AON_APB_CFG */

#define BIT_AON_CLK_CORE_CGM_AON_APB_DIV(x)         (((x) & 0x3) << 8)
#define BIT_AON_CLK_CORE_CGM_AON_APB_SEL(x)         (((x) & 0x3))

/* REG_AON_CLK_CORE_CGM_ADI_CFG */

#define BIT_AON_CLK_CORE_CGM_ADI_SEL(x)             (((x) & 0x3))

/* REG_AON_CLK_CORE_CGM_AUX0_CFG */

#define BIT_AON_CLK_CORE_CGM_AUX0_SEL(x)            (((x) & 0xF))

/* REG_AON_CLK_CORE_CGM_AUX1_CFG */

#define BIT_AON_CLK_CORE_CGM_AUX1_SEL(x)            (((x) & 0xF))

/* REG_AON_CLK_CORE_CGM_AUX2_CFG */

#define BIT_AON_CLK_CORE_CGM_AUX2_SEL(x)            (((x) & 0xF))

/* REG_AON_CLK_CORE_CGM_PROBE_CFG */

#define BIT_AON_CLK_CORE_CGM_PROBE_SEL(x)           (((x) & 0xF))

/* REG_AON_CLK_CORE_CGM_PWM0_CFG */

#define BIT_AON_CLK_CORE_CGM_PWM0_SEL(x)            (((x) & 0x3))

/* REG_AON_CLK_CORE_CGM_PWM1_CFG */

#define BIT_AON_CLK_CORE_CGM_PWM1_SEL(x)            (((x) & 0x3))

/* REG_AON_CLK_CORE_CGM_PWM2_CFG */

#define BIT_AON_CLK_CORE_CGM_PWM2_SEL(x)            (((x) & 0x3))

/* REG_AON_CLK_CORE_CGM_PWM3_CFG */

#define BIT_AON_CLK_CORE_CGM_PWM3_SEL(x)            (((x) & 0x3))

/* REG_AON_CLK_CORE_CGM_EFUSE_CFG */

#define BIT_AON_CLK_CORE_CGM_EFUSE_SEL              BIT(0)

/* REG_AON_CLK_CORE_CGM_CM4_UART_CFG */

#define BIT_AON_CLK_CORE_CGM_CM4_UART_SEL(x)        (((x) & 0x3))

/* REG_AON_CLK_CORE_CGM_32K_CFG */

#define BIT_AON_CLK_CORE_CGM_32K_SEL                BIT(0)

/* REG_AON_CLK_CORE_CGM_1K_CFG */


/* REG_AON_CLK_CORE_CGM_THM0_CFG */

#define BIT_AON_CLK_CORE_CGM_THM0_SEL               BIT(0)

/* REG_AON_CLK_CORE_CGM_THM1_CFG */

#define BIT_AON_CLK_CORE_CGM_THM1_SEL               BIT(0)

/* REG_AON_CLK_CORE_CGM_AUD_CFG */

#define BIT_AON_CLK_CORE_CGM_AUD_SEL                BIT(0)

/* REG_AON_CLK_CORE_CGM_AUDIF_CFG */

#define BIT_AON_CLK_CORE_CGM_AUDIF_SEL(x)           (((x) & 0x3))

/* REG_AON_CLK_CORE_CGM_VBC_CFG */

#define BIT_AON_CLK_CORE_CGM_VBC_SEL                BIT(0)

/* REG_AON_CLK_CORE_CGM_AUD_IIS_DA0_CFG */

#define BIT_AON_CLK_CORE_CGM_AUD_IIS_DA0_PAD_SEL    BIT(16)

/* REG_AON_CLK_CORE_CGM_AUD_IIS0_AD0_CFG */

#define BIT_AON_CLK_CORE_CGM_AUD_IIS0_AD0_PAD_SEL   BIT(16)

/* REG_AON_CLK_CORE_CGM_CA53_DAP_CFG */

#define BIT_AON_CLK_CORE_CGM_CA53_DAP_SEL(x)        (((x) & 0x3))

/* REG_AON_CLK_CORE_CGM_CA53_DAP_MTCK_CFG */

#define BIT_AON_CLK_CORE_CGM_CA53_DAP_MTCK_PAD_SEL  BIT(16)

/* REG_AON_CLK_CORE_CGM_CA53_TS_CFG */

#define BIT_AON_CLK_CORE_CGM_CA53_TS_SEL(x)         (((x) & 0x3))

/* REG_AON_CLK_CORE_CGM_DJTAG_TCK_CFG */

#define BIT_AON_CLK_CORE_CGM_DJTAG_TCK_PAD_SEL      BIT(16)
#define BIT_AON_CLK_CORE_CGM_DJTAG_TCK_SEL          BIT(0)

/* REG_AON_CLK_CORE_CGM_CM4_AHB_CFG */

#define BIT_AON_CLK_CORE_CGM_CM4_AHB_DIV(x)         (((x) & 0x3) << 8)
#define BIT_AON_CLK_CORE_CGM_CM4_AHB_SEL(x)         (((x) & 0x3))

/* REG_AON_CLK_CORE_CGM_FUNCDMA_CFG */

#define BIT_AON_CLK_CORE_CGM_FUNCDMA_PAD_SEL        BIT(16)

/* REG_AON_CLK_CORE_CGM_EMC_REF_CFG */

#define BIT_AON_CLK_CORE_CGM_EMC_REF_SEL(x)         (((x) & 0x3))

/* REG_AON_CLK_CORE_CGM_CSSYS_CFG */

#define BIT_AON_CLK_CORE_CGM_CSSYS_DIV(x)           (((x) & 0x3) << 8)
#define BIT_AON_CLK_CORE_CGM_CSSYS_SEL(x)           (((x) & 0x7))

/* REG_AON_CLK_CORE_CGM_TMR_CFG */

#define BIT_AON_CLK_CORE_CGM_TMR_SEL                BIT(0)

/* REG_AON_CLK_CORE_CGM_PMU_CFG */

#define BIT_AON_CLK_CORE_CGM_PMU_SEL                BIT(0)

/* REG_AON_CLK_CORE_CGM_WCDMA_CFG */

#define BIT_AON_CLK_CORE_CGM_WCDMA_SEL              BIT(0)

/* REG_AON_CLK_CORE_CGM_DSI_TEST_CFG */

#define BIT_AON_CLK_CORE_CGM_DSI_TEST_PAD_SEL       BIT(16)

/* REG_AON_CLK_CORE_CGM_RFTI_SBI_CFG */

#define BIT_AON_CLK_CORE_CGM_RFTI_SBI_SEL(x)        (((x) & 0x7))

/* REG_AON_CLK_CORE_CGM_RFTI1_XO_CFG */

#define BIT_AON_CLK_CORE_CGM_RFTI1_XO_SEL           BIT(0)

/* REG_AON_CLK_CORE_CGM_RFTI_LTH_CFG */

#define BIT_AON_CLK_CORE_CGM_RFTI_LTH_SEL           BIT(0)

/* REG_AON_CLK_CORE_CGM_RFTI2_XO_CFG */

#define BIT_AON_CLK_CORE_CGM_RFTI2_XO_SEL           BIT(0)

/* REG_AON_CLK_CORE_CGM_LVDSRF_CALI_CFG */

#define BIT_AON_CLK_CORE_CGM_LVDSRF_CALI_SEL        BIT(0)

/* REG_AON_CLK_CORE_CGM_SERDES_DPHY_APB_CFG */

#define BIT_AON_CLK_CORE_CGM_SERDES_DPHY_APB_SEL    BIT(0)

/* REG_AON_CLK_CORE_CGM_SERDES_DPHY_REF_CFG */

#define BIT_AON_CLK_CORE_CGM_SERDES_DPHY_REF_SEL    BIT(0)

/* REG_AON_CLK_CORE_CGM_SERDES_DPHY_CFG_CFG */

#define BIT_AON_CLK_CORE_CGM_SERDES_DPHY_CFG_SEL    BIT(0)

/* REG_AON_CLK_CORE_CGM_ANALOG_IO_APB_CFG */

#define BIT_AON_CLK_CORE_CGM_ANALOG_IO_APB_DIV(x)   (((x) & 0x3) << 8)
#define BIT_AON_CLK_CORE_CGM_ANALOG_IO_APB_SEL      BIT(0)

/* REG_AON_CLK_CORE_CGM_DJTAG_TCK_HW_CFG */

#define BIT_AON_CLK_CORE_CGM_DJTAG_TCK_HW_PAD_SEL   BIT(16)

/* REG_AON_CLK_CORE_CGM_AP_MM_CFG */

#define BIT_AON_CLK_CORE_CGM_AP_MM_DIV(x)           (((x) & 0x3) << 8)
#define BIT_AON_CLK_CORE_CGM_AP_MM_SEL(x)           (((x) & 0x3))

/* REG_AON_CLK_CORE_CGM_AP_AXI_CFG */

#define BIT_AON_CLK_CORE_CGM_AP_AXI_SEL(x)          (((x) & 0x3))

/* REG_AON_CLK_CORE_CGM_WCN_CFG */

#define BIT_AON_CLK_CORE_CGM_WCN_DIV(x)             (((x) & 0x7) << 8)
#define BIT_AON_CLK_CORE_CGM_WCN_SEL(x)             (((x) & 0x3))

/* REG_AON_CLK_CORE_CGM_NIC_GPU_CFG */

#define BIT_AON_CLK_CORE_CGM_NIC_GPU_DIV(x)         (((x) & 0x7) << 8)
#define BIT_AON_CLK_CORE_CGM_NIC_GPU_SEL(x)         (((x) & 0x7))

/* REG_AON_CLK_CORE_CGM_MM_ISP_CFG */

#define BIT_AON_CLK_CORE_CGM_MM_ISP_SEL(x)          (((x) & 0x7))

/* REG_AON_CLK_CORE_CGM_WTL_BRIDGE_CFG */

#define BIT_AON_CLK_CORE_CGM_WTL_BRIDGE_DIV(x)      (((x) & 0x3) << 8)
#define BIT_AON_CLK_CORE_CGM_WTL_BRIDGE_SEL(x)      (((x) & 0x3))


#endif /* AON_CLK_CORE_H */

