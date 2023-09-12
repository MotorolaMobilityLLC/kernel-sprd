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


#ifndef GPU_APB_H
#define GPU_APB_H



#define REG_GPU_APB_APB_RST                       (0x0000)
#define REG_GPU_APB_APB_CLK_CTRL                  (0x0004)
#define REG_GPU_APB_APB_T820_INT_STS              (0x0008)
#define REG_GPU_APB_APB_GPU_PRB_SEL               (0x000C)
#define REG_GPU_APB_GPU_EMC_BRG_S0_LPC_CTRL       (0x0010)
#define REG_GPU_APB_GPU_EMC_BRG_M0_LPC_CTRL       (0x0014)
#define REG_GPU_APB_GPU_EMC_BRG_GPV_LPC_CTRL      (0x0018)
#define REG_GPU_APB_GPU_APB_BRG_LPC_CTRL          (0x001C)
#define REG_GPU_APB_APB_BARRIER_CTRL              (0x0020)
#define REG_GPU_APB_GPU_ENGINEER_DEBUG_RSVD       (0x0024)

/* REG_GPU_APB_APB_RST */

#define BIT_GPU_APB_GPU_SOFT_RST                    BIT(0)

/* REG_GPU_APB_APB_CLK_CTRL */

#define BIT_GPU_APB_CLK_GPU_DIV(x)                  (((x) & 0x3) << 4)
#define BIT_GPU_APB_CLK_GPU_SEL(x)                  (((x) & 0x7))

/* REG_GPU_APB_APB_T820_INT_STS */

#define BIT_GPU_APB_T820_IRQGPU                     BIT(2)
#define BIT_GPU_APB_T820_IRQMMU                     BIT(1)
#define BIT_GPU_APB_T820_IRQJOB                     BIT(0)

/* REG_GPU_APB_APB_GPU_PRB_SEL */

#define BIT_GPU_APB_GPU_PRB_SEL(x)                  (((x) & 0xF))

/* REG_GPU_APB_GPU_EMC_BRG_S0_LPC_CTRL */

#define BIT_GPU_APB_GPU_EMC_BRG_S0_LP_STAT_M        BIT(22)
#define BIT_GPU_APB_GPU_EMC_BRG_S0_MMTX_STOP_CH     BIT(21)
#define BIT_GPU_APB_GPU_EMC_BRG_S0_FRC_LSLP_MST     BIT(20)
#define BIT_GPU_APB_GPU_EMC_BRG_S0_MTX_LP_DISABLE   BIT(19)
#define BIT_GPU_APB_GPU_EMC_BRG_S0_LP_AUTO_CTRL_EN  BIT(18)
#define BIT_GPU_APB_GPU_EMC_BRG_S0_LP_FORCE_M       BIT(17)
#define BIT_GPU_APB_GPU_EMC_BRG_S0_LP_EB_M          BIT(16)
#define BIT_GPU_APB_GPU_EMC_BRG_S0_LP_NUM(x)        (((x) & 0xFFFF))

/* REG_GPU_APB_GPU_EMC_BRG_M0_LPC_CTRL */

#define BIT_GPU_APB_GPU_EMC_BRG_M0_LP_STAT_M        BIT(22)
#define BIT_GPU_APB_GPU_EMC_BRG_M0_LP_FORCE_M       BIT(17)
#define BIT_GPU_APB_GPU_EMC_BRG_M0_LP_EB_M          BIT(16)
#define BIT_GPU_APB_GPU_EMC_BRG_M0_LP_NUM(x)        (((x) & 0xFFFF))

/* REG_GPU_APB_GPU_EMC_BRG_GPV_LPC_CTRL */

#define BIT_GPU_APB_GPU_EMC_BRG_GPV_LP_STAT_M       BIT(22)
#define BIT_GPU_APB_GPU_EMC_BRG_GPV_LP_FORCE_M      BIT(17)
#define BIT_GPU_APB_GPU_EMC_BRG_GPV_LP_EB_M         BIT(16)
#define BIT_GPU_APB_GPU_EMC_BRG_GPV_LP_NUM(x)       (((x) & 0xFFFF))

/* REG_GPU_APB_GPU_APB_BRG_LPC_CTRL */

#define BIT_GPU_APB_GPU_APB_BRG_S0_LP_STAT_M        BIT(22)
#define BIT_GPU_APB_GPU_APB_BRG_S0_LP_FORCE_M       BIT(17)
#define BIT_GPU_APB_GPU_APB_BRG_S0_LP_EB_M          BIT(16)
#define BIT_GPU_APB_GPU_APB_BRG_S0_LP_NUM(x)        (((x) & 0xFFFF))

/* REG_GPU_APB_APB_BARRIER_CTRL */

#define BIT_GPU_APB_GPU_BARRIER_DISABLE             BIT(0)

/* REG_GPU_APB_GPU_ENGINEER_DEBUG_RSVD */

#define BIT_GPU_APB_GPU_ENGINEER_DBG_H(x)           (((x) & 0xFFFF) << 16)
#define BIT_GPU_APB_GPU_ENGINEER_DBG_L(x)           (((x) & 0xFFFF))


#endif /* GPU_APB_H */

