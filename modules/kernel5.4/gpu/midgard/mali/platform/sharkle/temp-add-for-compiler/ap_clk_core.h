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


#ifndef AP_CLK_CORE_H
#define AP_CLK_CORE_H



#define REG_AP_CLK_CORE_CGM_AP_APB_CFG             (0x0020)
#define REG_AP_CLK_CORE_CGM_NANDC_ECC_CFG          (0x0024)
#define REG_AP_CLK_CORE_CGM_OTG_REF_CFG            (0x0028)
#define REG_AP_CLK_CORE_CGM_OTG_UTMI_CFG           (0x002C)
#define REG_AP_CLK_CORE_CGM_UART1_CFG              (0x0030)
#define REG_AP_CLK_CORE_CGM_I2C0_CFG               (0x0034)
#define REG_AP_CLK_CORE_CGM_I2C1_CFG               (0x0038)
#define REG_AP_CLK_CORE_CGM_I2C2_CFG               (0x003C)
#define REG_AP_CLK_CORE_CGM_I2C3_CFG               (0x0040)
#define REG_AP_CLK_CORE_CGM_I2C4_CFG               (0x0044)
#define REG_AP_CLK_CORE_CGM_SPI0_CFG               (0x0048)
#define REG_AP_CLK_CORE_CGM_SPI2_CFG               (0x004C)
#define REG_AP_CLK_CORE_CGM_SPI_HS_CFG             (0x0050)
#define REG_AP_CLK_CORE_CGM_IIS0_CFG               (0x0054)
#define REG_AP_CLK_CORE_CGM_CE_CFG                 (0x0058)
#define REG_AP_CLK_CORE_CGM_26M_NANDC_CFG          (0x005C)
#define REG_AP_CLK_CORE_CGM_AP_EMMC_32K_CFG        (0x0060)
#define REG_AP_CLK_CORE_CGM_AP_SDIO0_32K_CFG       (0x0064)
#define REG_AP_CLK_CORE_CGM_AP_SDIO1_32K_CFG       (0x0068)
#define REG_AP_CLK_CORE_CGM_AP_SIM_32K_CFG         (0x006C)
#define REG_AP_CLK_CORE_CGM_DSI_PHY_SCAN_ONLY_CFG  (0x0070)
#define REG_AP_CLK_CORE_CGM_USB20_SCAN_ONLY_CFG    (0x0074)
#define REG_AP_CLK_CORE_CGM_NANDC_2X_CFG           (0x0078)
#define REG_AP_CLK_CORE_CGM_NANDC_1X_CFG           (0x007C)
#define REG_AP_CLK_CORE_CGM_SDIO0_2X_CFG           (0x0080)
#define REG_AP_CLK_CORE_CGM_SDIO0_1X_CFG           (0x0084)
#define REG_AP_CLK_CORE_CGM_SDIO1_2X_CFG           (0x0088)
#define REG_AP_CLK_CORE_CGM_SDIO1_1X_CFG           (0x008C)
#define REG_AP_CLK_CORE_CGM_EMMC_2X_CFG            (0x0090)
#define REG_AP_CLK_CORE_CGM_EMMC_1X_CFG            (0x0094)
#define REG_AP_CLK_CORE_CGM_VSP_CFG                (0x0098)
#define REG_AP_CLK_CORE_CGM_GSP_CFG                (0x009C)
#define REG_AP_CLK_CORE_CGM_DISPC0_CFG             (0x00A0)
#define REG_AP_CLK_CORE_CGM_DISPC0_DPI_CFG         (0x00A4)
#define REG_AP_CLK_CORE_CGM_DSI_RXESC_CFG          (0x00A8)
#define REG_AP_CLK_CORE_CGM_DSI_LANEBYTE_CFG       (0x00AC)
#define REG_AP_CLK_CORE_CGM_DPHY_REF_CFG           (0x00B0)
#define REG_AP_CLK_CORE_CGM_DPHY_CFG_CFG           (0x00B4)

/* REG_AP_CLK_CORE_CGM_AP_APB_CFG */

#define BIT_AP_CLK_CORE_CGM_AP_APB_SEL(x)          (((x) & 0x3))

/* REG_AP_CLK_CORE_CGM_NANDC_ECC_CFG */

#define BIT_AP_CLK_CORE_CGM_NANDC_ECC_DIV(x)       (((x) & 0x7) << 8)
#define BIT_AP_CLK_CORE_CGM_NANDC_ECC_SEL(x)       (((x) & 0x3))

/* REG_AP_CLK_CORE_CGM_OTG_REF_CFG */

#define BIT_AP_CLK_CORE_CGM_OTG_REF_SEL            BIT(0)

/* REG_AP_CLK_CORE_CGM_OTG_UTMI_CFG */

#define BIT_AP_CLK_CORE_CGM_OTG_UTMI_PAD_SEL       BIT(16)

/* REG_AP_CLK_CORE_CGM_UART1_CFG */

#define BIT_AP_CLK_CORE_CGM_UART1_DIV(x)           (((x) & 0x7) << 8)
#define BIT_AP_CLK_CORE_CGM_UART1_SEL(x)           (((x) & 0x3))

/* REG_AP_CLK_CORE_CGM_I2C0_CFG */

#define BIT_AP_CLK_CORE_CGM_I2C0_DIV(x)            (((x) & 0x7) << 8)
#define BIT_AP_CLK_CORE_CGM_I2C0_SEL(x)            (((x) & 0x3))

/* REG_AP_CLK_CORE_CGM_I2C1_CFG */

#define BIT_AP_CLK_CORE_CGM_I2C1_DIV(x)            (((x) & 0x7) << 8)
#define BIT_AP_CLK_CORE_CGM_I2C1_SEL(x)            (((x) & 0x3))

/* REG_AP_CLK_CORE_CGM_I2C2_CFG */

#define BIT_AP_CLK_CORE_CGM_I2C2_DIV(x)            (((x) & 0x7) << 8)
#define BIT_AP_CLK_CORE_CGM_I2C2_SEL(x)            (((x) & 0x3))

/* REG_AP_CLK_CORE_CGM_I2C3_CFG */

#define BIT_AP_CLK_CORE_CGM_I2C3_DIV(x)            (((x) & 0x7) << 8)
#define BIT_AP_CLK_CORE_CGM_I2C3_SEL(x)            (((x) & 0x3))

/* REG_AP_CLK_CORE_CGM_I2C4_CFG */

#define BIT_AP_CLK_CORE_CGM_I2C4_DIV(x)            (((x) & 0x7) << 8)
#define BIT_AP_CLK_CORE_CGM_I2C4_SEL(x)            (((x) & 0x3))

/* REG_AP_CLK_CORE_CGM_SPI0_CFG */

#define BIT_AP_CLK_CORE_CGM_SPI0_PAD_SEL           BIT(16)
#define BIT_AP_CLK_CORE_CGM_SPI0_DIV(x)            (((x) & 0x7) << 8)
#define BIT_AP_CLK_CORE_CGM_SPI0_SEL(x)            (((x) & 0x3))

/* REG_AP_CLK_CORE_CGM_SPI2_CFG */

#define BIT_AP_CLK_CORE_CGM_SPI2_PAD_SEL           BIT(16)
#define BIT_AP_CLK_CORE_CGM_SPI2_DIV(x)            (((x) & 0x7) << 8)
#define BIT_AP_CLK_CORE_CGM_SPI2_SEL(x)            (((x) & 0x3))

/* REG_AP_CLK_CORE_CGM_SPI_HS_CFG */

#define BIT_AP_CLK_CORE_CGM_SPI_HS_PAD_SEL         BIT(16)
#define BIT_AP_CLK_CORE_CGM_SPI_HS_DIV(x)          (((x) & 0x7) << 8)
#define BIT_AP_CLK_CORE_CGM_SPI_HS_SEL(x)          (((x) & 0x3))

/* REG_AP_CLK_CORE_CGM_IIS0_CFG */

#define BIT_AP_CLK_CORE_CGM_IIS0_DIV(x)            (((x) & 0x7) << 8)
#define BIT_AP_CLK_CORE_CGM_IIS0_SEL(x)            (((x) & 0x3))

/* REG_AP_CLK_CORE_CGM_CE_CFG */

#define BIT_AP_CLK_CORE_CGM_CE_SEL(x)              (((x) & 0x3))

/* REG_AP_CLK_CORE_CGM_26M_NANDC_CFG */

#define BIT_AP_CLK_CORE_CGM_26M_NANDC_SEL          BIT(0)

/* REG_AP_CLK_CORE_CGM_AP_EMMC_32K_CFG */

#define BIT_AP_CLK_CORE_CGM_AP_EMMC_32K_SEL        BIT(0)

/* REG_AP_CLK_CORE_CGM_AP_SDIO0_32K_CFG */

#define BIT_AP_CLK_CORE_CGM_AP_SDIO0_32K_SEL       BIT(0)

/* REG_AP_CLK_CORE_CGM_AP_SDIO1_32K_CFG */

#define BIT_AP_CLK_CORE_CGM_AP_SDIO1_32K_SEL       BIT(0)

/* REG_AP_CLK_CORE_CGM_AP_SIM_32K_CFG */

#define BIT_AP_CLK_CORE_CGM_AP_SIM_32K_SEL         BIT(0)

/* REG_AP_CLK_CORE_CGM_DSI_PHY_SCAN_ONLY_CFG */

#define BIT_AP_CLK_CORE_CGM_DSI_PHY_SCAN_ONLY_SEL  BIT(0)

/* REG_AP_CLK_CORE_CGM_USB20_SCAN_ONLY_CFG */

#define BIT_AP_CLK_CORE_CGM_USB20_SCAN_ONLY_SEL    BIT(0)

/* REG_AP_CLK_CORE_CGM_NANDC_2X_CFG */

#define BIT_AP_CLK_CORE_CGM_NANDC_2X_DIV(x)        (((x) & 0xF) << 8)
#define BIT_AP_CLK_CORE_CGM_NANDC_2X_SEL(x)        (((x) & 0xF))

/* REG_AP_CLK_CORE_CGM_NANDC_1X_CFG */

#define BIT_AP_CLK_CORE_CGM_NANDC_1X_DIV           BIT(8)

/* REG_AP_CLK_CORE_CGM_SDIO0_2X_CFG */

#define BIT_AP_CLK_CORE_CGM_SDIO0_2X_SEL(x)        (((x) & 0x7))

/* REG_AP_CLK_CORE_CGM_SDIO0_1X_CFG */


/* REG_AP_CLK_CORE_CGM_SDIO1_2X_CFG */

#define BIT_AP_CLK_CORE_CGM_SDIO1_2X_SEL(x)        (((x) & 0x7))

/* REG_AP_CLK_CORE_CGM_SDIO1_1X_CFG */


/* REG_AP_CLK_CORE_CGM_EMMC_2X_CFG */

#define BIT_AP_CLK_CORE_CGM_EMMC_2X_SEL(x)         (((x) & 0x7))

/* REG_AP_CLK_CORE_CGM_EMMC_1X_CFG */


/* REG_AP_CLK_CORE_CGM_VSP_CFG */

#define BIT_AP_CLK_CORE_CGM_VSP_SEL(x)             (((x) & 0x3))

/* REG_AP_CLK_CORE_CGM_GSP_CFG */

#define BIT_AP_CLK_CORE_CGM_GSP_SEL(x)             (((x) & 0x3))

/* REG_AP_CLK_CORE_CGM_DISPC0_CFG */

#define BIT_AP_CLK_CORE_CGM_DISPC0_SEL(x)          (((x) & 0x3))

/* REG_AP_CLK_CORE_CGM_DISPC0_DPI_CFG */

#define BIT_AP_CLK_CORE_CGM_DISPC0_DPI_DIV(x)      (((x) & 0xF) << 8)
#define BIT_AP_CLK_CORE_CGM_DISPC0_DPI_SEL(x)      (((x) & 0x3))

/* REG_AP_CLK_CORE_CGM_DSI_RXESC_CFG */

#define BIT_AP_CLK_CORE_CGM_DSI_RXESC_PAD_SEL      BIT(16)

/* REG_AP_CLK_CORE_CGM_DSI_LANEBYTE_CFG */

#define BIT_AP_CLK_CORE_CGM_DSI_LANEBYTE_PAD_SEL   BIT(16)

/* REG_AP_CLK_CORE_CGM_DPHY_REF_CFG */

#define BIT_AP_CLK_CORE_CGM_DPHY_REF_SEL           BIT(0)

/* REG_AP_CLK_CORE_CGM_DPHY_CFG_CFG */

#define BIT_AP_CLK_CORE_CGM_DPHY_CFG_SEL           BIT(0)


#endif /* AP_CLK_CORE_H */

