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


#ifndef MM_AHB_H
#define MM_AHB_H



#define REG_MM_AHB_AHB_EB               (0x0000)
#define REG_MM_AHB_AHB_RST              (0x0004)
#define REG_MM_AHB_GEN_CKG_CFG          (0x0008)
#define REG_MM_AHB_MIPI_CSI2_CTRL       (0x000C)
#define REG_MM_AHB_QOS_THREHOLD_MM      (0x0010)
#define REG_MM_AHB_MM_LP_DISABLE        (0x0014)
#define REG_MM_AHB_MM_LPC_CTRL_DCAM     (0x0018)
#define REG_MM_AHB_MM_LPC_CTRL_ISP      (0x001C)
#define REG_MM_AHB_MM_LPC_CTRL_JPG      (0x0020)
#define REG_MM_AHB_MM_LPC_CTRL_CPP      (0x0024)
#define REG_MM_AHB_MM_LPC_CTRL_MTX_S0   (0x0028)

/* REG_MM_AHB_AHB_EB */

#define BIT_MM_AHB_CKG_EB                      BIT(6)
#define BIT_MM_AHB_JPG_EB                      BIT(5)
#define BIT_MM_AHB_CSI_S_EB                    BIT(4)
#define BIT_MM_AHB_CSI_EB                      BIT(3)
#define BIT_MM_AHB_CPP_EB                      BIT(2)
#define BIT_MM_AHB_ISP_EB                      BIT(1)
#define BIT_MM_AHB_DCAM_EB                     BIT(0)

/* REG_MM_AHB_AHB_RST */

#define BIT_MM_AHB_CPP_SOFT_RST_MASK           BIT(15)
#define BIT_MM_AHB_JPP_SOFT_RST_MASK           BIT(14)
#define BIT_MM_AHB_DCAM_SOFT_RST_MASK          BIT(13)
#define BIT_MM_AHB_CSI_S_SOFT_RST              BIT(12)
#define BIT_MM_AHB_CSI_SOFT_RST                BIT(11)
#define BIT_MM_AHB_AXI_MM_MTX_SOFT_RST         BIT(10)
#define BIT_MM_AHB_CKG_SOFT_RST                BIT(9)
#define BIT_MM_AHB_JPG_SOFT_RST                BIT(8)
#define BIT_MM_AHB_ISP_FMCU_SOFT_RST           BIT(7)
#define BIT_MM_AHB_ISP_CFG_SOFT_RST            BIT(6)
#define BIT_MM_AHB_ISP_LOG_SOFT_RST            BIT(5)
#define BIT_MM_AHB_CPP_SOFT_RST                BIT(4)
#define BIT_MM_AHB_DCAM1_SOFT_RST              BIT(3)
#define BIT_MM_AHB_DCAM0_SOFT_RST              BIT(2)
#define BIT_MM_AHB_DCAM_AXIM_SOFT_RST          BIT(1)
#define BIT_MM_AHB_DCAM_ALL_SOFT_RST           BIT(0)

/* REG_MM_AHB_GEN_CKG_CFG */

#define BIT_MM_AHB_MIPI_CSI_S_CKG_EN           BIT(5)
#define BIT_MM_AHB_MIPI_CSI_CKG_EN             BIT(4)
#define BIT_MM_AHB_ISP_AXI_CKG_EN              BIT(3)
#define BIT_MM_AHB_SENSOR1_CKG_EN              BIT(2)
#define BIT_MM_AHB_SENSOR0_CKG_EN              BIT(1)
#define BIT_MM_AHB_CPHY_CFG_CKG_EN             BIT(0)

/* REG_MM_AHB_MIPI_CSI2_CTRL */

#define BIT_MM_AHB_CSI_2P2L_TESTDOUT_MS_SEL    BIT(4)
#define BIT_MM_AHB_MIPI_CPHY_SEL1(x)           (((x) & 0x3) << 2)
#define BIT_MM_AHB_MIPI_CPHY_SEL0(x)           (((x) & 0x3))

/* REG_MM_AHB_QOS_THREHOLD_MM */

#define BIT_MM_AHB_AR_QOS_THREHOLD_MM(x)       (((x) & 0xF) << 4)
#define BIT_MM_AHB_AW_QOS_THREHOLD_MM(x)       (((x) & 0xF))

/* REG_MM_AHB_MM_LP_DISABLE */

#define BIT_MM_AHB_CGM_CLK_ISP_AUTO_GATE_EN    BIT(2)
#define BIT_MM_AHB_CGM_MM_MTX_S0_AUTO_GATE_EN  BIT(1)
#define BIT_MM_AHB_MM_LPC_DISABLE              BIT(0)

/* REG_MM_AHB_MM_LPC_CTRL_DCAM */

#define BIT_MM_AHB_LP_EB_DCAM                  BIT(16)
#define BIT_MM_AHB_LP_NUM_DCAM(x)              (((x) & 0xFFFF))

/* REG_MM_AHB_MM_LPC_CTRL_ISP */

#define BIT_MM_AHB_LP_EB_ISP                   BIT(16)
#define BIT_MM_AHB_LP_NUM_ISP(x)               (((x) & 0xFFFF))

/* REG_MM_AHB_MM_LPC_CTRL_JPG */

#define BIT_MM_AHB_LP_EB_JPG                   BIT(16)
#define BIT_MM_AHB_LP_NUM_JPG(x)               (((x) & 0xFFFF))

/* REG_MM_AHB_MM_LPC_CTRL_CPP */

#define BIT_MM_AHB_LP_EB_CPP                   BIT(16)
#define BIT_MM_AHB_LP_NUM_CPP(x)               (((x) & 0xFFFF))

/* REG_MM_AHB_MM_LPC_CTRL_MTX_S0 */

#define BIT_MM_AHB_LP_EB_MTX_S0                BIT(16)
#define BIT_MM_AHB_LP_NUM_MTX_S0(x)            (((x) & 0xFFFF))


#endif /* MM_AHB_H */

