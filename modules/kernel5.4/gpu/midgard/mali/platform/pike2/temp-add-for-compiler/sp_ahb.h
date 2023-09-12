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


#ifndef SP_AHB_H
#define SP_AHB_H



#define REG_SP_AHB_CM4_EB                 (0x0000)
#define REG_SP_AHB_CM4_SOFT_RST           (0x0004)
#define REG_SP_AHB_SLP_CTL                (0x0008)
#define REG_SP_AHB_CM4_CORE_CFG           (0x000C)
#define REG_SP_AHB_CM4_CORE_AUXFAULT_CFG  (0x0010)
#define REG_SP_AHB_CM4_MAIN_STATOUT       (0x0014)
#define REG_SP_AHB_CM4_HWDATAD_STAT       (0x0018)
#define REG_SP_AHB_CM4_INT_STAT           (0x001C)
#define REG_SP_AHB_CM4_CLK_EMC_CTRL       (0x0020)
#define REG_SP_AHB_CM4_MTX_LP_CTRL        (0x0024)
#define REG_SP_AHB_DUMMY                  (0x0040)

/* REG_SP_AHB_CM4_EB */

#define BIT_SP_AHB_CM4_GPIO_EB            BIT(10)
#define BIT_SP_AHB_CM4_UART_EB            BIT(9)
#define BIT_SP_AHB_CM4_TMR_EB             BIT(8)
#define BIT_SP_AHB_CM4_SYST_EB            BIT(7)
#define BIT_SP_AHB_CM4_WDG_EB             BIT(6)
#define BIT_SP_AHB_CM4_EIC_EB             BIT(5)
#define BIT_SP_AHB_CM4_INTC_EB            BIT(4)
#define BIT_SP_AHB_CM4_IMC_EB             BIT(2)

/* REG_SP_AHB_CM4_SOFT_RST */

#define BIT_SP_AHB_CM4_GPIO_SOFT_RST      BIT(10)
#define BIT_SP_AHB_CM4_UART_SOFT_RST      BIT(9)
#define BIT_SP_AHB_CM4_TMR_SOFT_RST       BIT(8)
#define BIT_SP_AHB_CM4_SYST_SOFT_RST      BIT(7)
#define BIT_SP_AHB_CM4_WDG_SOFT_RST       BIT(6)
#define BIT_SP_AHB_CM4_EIC_SOFT_RST       BIT(5)
#define BIT_SP_AHB_CM4_INTC_SOFT_RST      BIT(4)
#define BIT_SP_AHB_CM4_IMC_SOFT_RST       BIT(2)
#define BIT_SP_AHB_CM4_ARCH_SOFT_RST      BIT(0)

/* REG_SP_AHB_SLP_CTL */

#define BIT_SP_AHB_SYS_AUTO_GATE_EN       BIT(2)
#define BIT_SP_AHB_CORE_AUTO_GATE_EN      BIT(0)

/* REG_SP_AHB_CM4_CORE_CFG */

#define BIT_SP_AHB_SOFT_EXRESPD           BIT(9)
#define BIT_SP_AHB_SOFT_TSCLK_CHG         BIT(7)
#define BIT_SP_AHB_SOFT_FIX_MSTTYPE       BIT(6)
#define BIT_SP_AHB_SOFT_DBG_RESTART       BIT(5)
#define BIT_SP_AHB_SOFT_EDBGRQ            BIT(4)
#define BIT_SP_AHB_SOFT_SLEEP_HOLD_REQ_N  BIT(3)
#define BIT_SP_AHB_SOFT_EXRESPS           BIT(2)
#define BIT_SP_AHB_SOFT_CGBYPASS          BIT(1)
#define BIT_SP_AHB_SOFT_RSTBYPASS         BIT(0)

/* REG_SP_AHB_CM4_CORE_AUXFAULT_CFG */

#define BIT_SP_AHB_SOFT_AUXFAULT(x)       (((x) & 0xFFFFFFFF))

/* REG_SP_AHB_CM4_MAIN_STATOUT */

#define BIT_SP_AHB_WICENACK_STAT          BIT(31)
#define BIT_SP_AHB_GATEHCLK_STAT          BIT(30)
#define BIT_SP_AHB_WAKEUP_STAT            BIT(29)
#define BIT_SP_AHB_SLEEPHOLDACKN_STAT     BIT(28)
#define BIT_SP_AHB_SLEEPINGDEEP_STAT      BIT(27)
#define BIT_SP_AHB_SLEEPING_STAT          BIT(26)
#define BIT_SP_AHB_DBGRESTARTED_STAT      BIT(24)
#define BIT_SP_AHB_HALTED_STAT            BIT(23)
#define BIT_SP_AHB_BRCHSTAT_STAT(x)       (((x) & 0xF) << 19)
#define BIT_SP_AHB_EXREQS_STAT            BIT(18)
#define BIT_SP_AHB_MEMATTRS_STAT(x)       (((x) & 0x3) << 16)
#define BIT_SP_AHB_HMASTLOCKS_STAT        BIT(15)
#define BIT_SP_AHB_HMASTERS_STAT(x)       (((x) & 0x3) << 13)
#define BIT_SP_AHB_HWRITED_STAT           BIT(12)
#define BIT_SP_AHB_EXREQD_STAT            BIT(11)
#define BIT_SP_AHB_MEMATTRD_STAT(x)       (((x) & 0x3) << 9)
#define BIT_SP_AHB_HMASTERD_STAT(x)       (((x) & 0x3) << 7)
#define BIT_SP_AHB_MEMATTRI_STAT(x)       (((x) & 0x3) << 5)
#define BIT_SP_AHB_SWV_STAT               BIT(4)
#define BIT_SP_AHB_CDBGPWRUPREQ_STAT      BIT(0)

/* REG_SP_AHB_CM4_HWDATAD_STAT */

#define BIT_SP_AHB_HWDATAD_STAT(x)        (((x) & 0xFFFFFFFF))

/* REG_SP_AHB_CM4_INT_STAT */

#define BIT_SP_AHB_CURRPRI_STAT(x)        (((x) & 0xFF))

/* REG_SP_AHB_CM4_CLK_EMC_CTRL */

#define BIT_SP_AHB_CM4_CLK_EMC_SEL(x)     (((x) & 0x7) << 2)
#define BIT_SP_AHB_CM4_CLK_EMC_DIV(x)     (((x) & 0x3))

/* REG_SP_AHB_CM4_MTX_LP_CTRL */

#define BIT_SP_AHB_LP_FORCE_ACK           BIT(19)
#define BIT_SP_AHB_LP_FORCE               BIT(18)
#define BIT_SP_AHB_LP_STAT                BIT(17)
#define BIT_SP_AHB_LP_EB                  BIT(16)
#define BIT_SP_AHB_LP_NUM(x)              (((x) & 0xFFFF))

/* REG_SP_AHB_DUMMY */

#define BIT_SP_AHB_DUMMY_DEF1(x)          (((x) & 0xFFFF) << 16)
#define BIT_SP_AHB_DUMMY_DEF0(x)          (((x) & 0xFFFF))


#endif /* SP_AHB_H */

