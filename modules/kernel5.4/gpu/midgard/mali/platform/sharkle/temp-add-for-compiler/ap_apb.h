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


#ifndef AP_APB_H
#define AP_APB_H



#define REG_AP_APB_APB_EB         (0x0000)
#define REG_AP_APB_APB_RST        (0x0004)
#define REG_AP_APB_APB_MISC_CTRL  (0x0008)

/* REG_AP_APB_APB_EB */

#define BIT_AP_APB_INTC3_EB            BIT(22)
#define BIT_AP_APB_INTC2_EB            BIT(21)
#define BIT_AP_APB_INTC1_EB            BIT(20)
#define BIT_AP_APB_INTC0_EB            BIT(19)
#define BIT_AP_APB_SIM0_32K_EB         BIT(18)
#define BIT_AP_APB_UART1_EB            BIT(14)
#define BIT_AP_APB_I2C4_EB             BIT(12)
#define BIT_AP_APB_I2C3_EB             BIT(11)
#define BIT_AP_APB_I2C2_EB             BIT(10)
#define BIT_AP_APB_I2C1_EB             BIT(9)
#define BIT_AP_APB_I2C0_EB             BIT(8)
#define BIT_AP_APB_SPI2_EB             BIT(7)
#define BIT_AP_APB_SPI0_EB             BIT(5)
#define BIT_AP_APB_APB_REG_EB          BIT(2)
#define BIT_AP_APB_IIS0_EB             BIT(1)
#define BIT_AP_APB_SIM0_EB             BIT(0)

/* REG_AP_APB_APB_RST */

#define BIT_AP_APB_INTC3_SOFT_RST      BIT(22)
#define BIT_AP_APB_INTC2_SOFT_RST      BIT(21)
#define BIT_AP_APB_INTC1_SOFT_RST      BIT(20)
#define BIT_AP_APB_INTC0_SOFT_RST      BIT(19)
#define BIT_AP_APB_UART1_SOFT_RST      BIT(14)
#define BIT_AP_APB_I2C4_SOFT_RST       BIT(12)
#define BIT_AP_APB_I2C3_SOFT_RST       BIT(11)
#define BIT_AP_APB_I2C2_SOFT_RST       BIT(10)
#define BIT_AP_APB_I2C1_SOFT_RST       BIT(9)
#define BIT_AP_APB_I2C0_SOFT_RST       BIT(8)
#define BIT_AP_APB_SPI2_SOFT_RST       BIT(7)
#define BIT_AP_APB_SPI1_SOFT_RST       BIT(6)
#define BIT_AP_APB_SPI0_SOFT_RST       BIT(5)
#define BIT_AP_APB_IIS0_SOFT_RST       BIT(1)
#define BIT_AP_APB_SIM0_SOFT_RST       BIT(0)

/* REG_AP_APB_APB_MISC_CTRL */

#define BIT_AP_APB_SIM_CLK_POLARITY    BIT(1)
#define BIT_AP_APB_FMARK_POLARITY_INV  BIT(0)


#endif /* AP_APB_H */

