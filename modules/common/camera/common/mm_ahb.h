/*
 * Copyright (C) 2021-2022 UNISOC Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MM_AHB_H
#define MM_AHB_H



#define REG_MM_AHB_AHB_EB                      (0x0000)
#define REG_MM_AHB_AHB_RST                     (0x0004)
#define REG_MM_AHB_GEN_CLK_CFG                 (0x0008)
#define REG_MM_AHB_MM_QOS                      (0x000C)
#define REG_MM_AHB_MM_LP_DISABLE               (0x0010)
#define REG_MM_AHB_MM_LPC_CTRL_ISP             (0x0014)
#define REG_MM_AHB_MM_LPC_CTRL_JPG             (0x0018)
#define REG_MM_AHB_MM_LPC_CTRL_CPP             (0x001C)
#define REG_MM_AHB_MM_LPC_CTRL_MM_MAIN_MTX_S0  (0x0020)
#define REG_MM_AHB_MM_LPC_CTRL_AXI2AHB_M0      (0x0024)
#define REG_MM_AHB_MM_LPC_CTRL_DCAM_ASYNC_BDG  (0x0028)
#define REG_MM_AHB_MM_LPC_CTRL_MTX_ASYNC_BDG   (0x002C)
#define REG_MM_AHB_MIPI_CSI_SEL_CTRL           (0x0030)
#define REG_MM_AHB_MM_0P5_APPEND               (0x0034)
#define REG_MM_AHB_MM_IP_BUSY                  (0x0038)
#define REG_MM_AHB_MM_AS_BDG_STATE             (0x003C)
#define REG_MM_AHB_MM_LPC_CTRL_FD              (0x0040)

/* REG_MM_AHB_AHB_EB */

#define BIT_MM_AHB_FD_EB                          BIT(10)
#define BIT_MM_AHB_DVFS_EB                        BIT(9)
#define BIT_MM_AHB_ISP_AHB_EB                     BIT(8)
#define BIT_MM_AHB_CKG_EB                         BIT(7)
#define BIT_MM_AHB_CSI0_EB                        BIT(6)
#define BIT_MM_AHB_CSI1_EB                        BIT(5)
#define BIT_MM_AHB_CSI2_EB                        BIT(4)
#define BIT_MM_AHB_ISP_EB                         BIT(3)
#define BIT_MM_AHB_DCAM_EB                        BIT(2)
#define BIT_MM_AHB_JPG_EB                         BIT(1)
#define BIT_MM_AHB_CPP_EB                         BIT(0)

/* REG_MM_AHB_AHB_RST */

#define BIT_MM_AHB_FD_SOFT_RST_MASK               BIT(27)
#define BIT_MM_AHB_FD_SOFT_RST                    BIT(26)
#define BIT_MM_AHB_DVFS_SOFT_RST                  BIT(24)
#define BIT_MM_AHB_DCAM_ALL_SOFT_RST              BIT(23)
#define BIT_MM_AHB_ISP_DCAM0_SOFT_RST             BIT(22)
#define BIT_MM_AHB_ISP_DCAM1_SOFT_RST             BIT(21)
#define BIT_MM_AHB_DCAM_SOFT_RST_MASK             BIT(20)
#define BIT_MM_AHB_JPG_SOFT_RST_MASK              BIT(19)
#define BIT_MM_AHB_CPP_SOFT_RST_MASK              BIT(18)
#define BIT_MM_AHB_MM_MAIN_SOFT_RST               BIT(17)
#define BIT_MM_AHB_CPP_SOFT_RST                   BIT(16)
#define BIT_MM_AHB_CPP_PATH0_SOFT_RST             BIT(15)
#define BIT_MM_AHB_CPP_PATH1_SOFT_RST             BIT(14)
#define BIT_MM_AHB_CPP_DMA_SOFT_RST               BIT(13)
#define BIT_MM_AHB_ISP_AHB_SOFT_RST               BIT(12)
#define BIT_MM_AHB_ISP_AXI_SOFT_RST               BIT(11)
#define BIT_MM_AHB_ISP_SOFT_RST                   BIT(10)
#define BIT_MM_AHB_MIPI_CSI0_SOFT_RST             BIT(9)
#define BIT_MM_AHB_MIPI_CSI1_SOFT_RST             BIT(8)
#define BIT_MM_AHB_MIPI_CSI2_SOFT_RST             BIT(7)
#define BIT_MM_AHB_DCAM_AXIM_SOFT_RST             BIT(6)
#define BIT_MM_AHB_DCAM0_SOFT_RST                 BIT(5)
#define BIT_MM_AHB_DCAM1_SOFT_RST                 BIT(4)
#define BIT_MM_AHB_DCAM2_SOFT_RST                 BIT(3)
#define BIT_MM_AHB_DCAM_AXI_SOFT_RST              BIT(2)
#define BIT_MM_AHB_JPG_SOFT_RST                   BIT(1)
#define BIT_MM_AHB_CKG_SOFT_RST                   BIT(0)

/* REG_MM_AHB_GEN_CLK_CFG */

#define BIT_MM_AHB_CPHY_CFG_CKG_EN                BIT(8)
#define BIT_MM_AHB_ISP_AXI_CKG_EN                 BIT(7)
#define BIT_MM_AHB_DCAM_AXI_CKG_EN                BIT(6)
#define BIT_MM_AHB_MIPI_CSI0_CKG_EN               BIT(5)
#define BIT_MM_AHB_MIPI_CSI1_CKG_EN               BIT(4)
#define BIT_MM_AHB_MIPI_CSI2_CKG_EN               BIT(3)
#define BIT_MM_AHB_SENSOR0_CKG_EN                 BIT(2)
#define BIT_MM_AHB_SENSOR1_CKG_EN                 BIT(1)
#define BIT_MM_AHB_SENSOR2_CKG_EN                 BIT(0)

/* REG_MM_AHB_MM_QOS */

#define BIT_MM_AHB_AR_QOS_THRESHOLD_MM(x)         (((x) & 0xF) << 4)
#define BIT_MM_AHB_AW_QOS_THRESHOLD_MM(x)         (((x) & 0xF))

/* REG_MM_AHB_MM_LP_DISABLE */

#define BIT_MM_AHB_CGM_ISP_AUTO_GATE_SEL          BIT(3)
#define BIT_MM_AHB_CGM_DCAM_AXI_AUTO_GATE_SEL     BIT(2)
#define BIT_MM_AHB_CGM_MM_MTX_S0_AUTO_GATE_EN     BIT(1)
#define BIT_MM_AHB_MM_LPC_DISABLE                 BIT(0)

/* REG_MM_AHB_MM_LPC_CTRL_ISP */

#define BIT_MM_AHB_PU_NUM_ISP(x)                  (((x) & 0xFF) << 24)
#define BIT_MM_AHB_LP_EB_ISP                      BIT(16)
#define BIT_MM_AHB_LP_NUM_ISP(x)                  (((x) & 0xFFFF))

/* REG_MM_AHB_MM_LPC_CTRL_JPG */

#define BIT_MM_AHB_PU_NUM_JPG(x)                  (((x) & 0xFF) << 24)
#define BIT_MM_AHB_LP_EB_JPG                      BIT(16)
#define BIT_MM_AHB_LP_NUM_JPG(x)                  (((x) & 0xFFFF))

/* REG_MM_AHB_MM_LPC_CTRL_CPP */

#define BIT_MM_AHB_PU_NUM_CPP(x)                  (((x) & 0xFF) << 24)
#define BIT_MM_AHB_LP_EB_CPP                      BIT(16)
#define BIT_MM_AHB_LP_NUM_CPP(x)                  (((x) & 0xFFFF))

/* REG_MM_AHB_MM_LPC_CTRL_MM_MAIN_MTX_S0 */

#define BIT_MM_AHB_PU_NUM_MM_MAIN_MTX_S0(x)       (((x) & 0xFF) << 24)
#define BIT_MM_AHB_LP_EB_MM_MAIN_MTX_S0           BIT(16)
#define BIT_MM_AHB_LP_NUM_MM_MAIN_MTX_S0(x)       (((x) & 0xFFFF))

/* REG_MM_AHB_MM_LPC_CTRL_AXI2AHB_M0 */

#define BIT_MM_AHB_PU_NUM_AXI2AHB_M0(x)           (((x) & 0xFF) << 24)
#define BIT_MM_AHB_LP_EB_AXI2AHB_M0               BIT(16)
#define BIT_MM_AHB_LP_NUM_AXI2AHB_M0(x)           (((x) & 0xFFFF))

/* REG_MM_AHB_MM_LPC_CTRL_DCAM_ASYNC_BDG */

#define BIT_MM_AHB_PU_NUM_DCAM_ASYNC_BDG(x)       (((x) & 0xFF) << 24)
#define BIT_MM_AHB_LP_EB_DCAM_ASYNC_BDG           BIT(16)
#define BIT_MM_AHB_LP_NUM_DCAM_ASYNC_BDG(x)       (((x) & 0xFFFF))

/* REG_MM_AHB_MM_LPC_CTRL_MTX_ASYNC_BDG */

#define BIT_MM_AHB_PU_NUM_MTX_ASYNC_BDG(x)        (((x) & 0xFF) << 24)
#define BIT_MM_AHB_LP_EB_MTX_ASYNC_BDG            BIT(16)
#define BIT_MM_AHB_LP_NUM_MTX_ASYNC_BDG(x)        (((x) & 0xFFFF))

/* REG_MM_AHB_MIPI_CSI_SEL_CTRL */

#define BIT_MM_AHB_ISP_INT_1_MASK                 BIT(27)
#define BIT_MM_AHB_ISP_INT_0_MASK                 BIT(26)
#define BIT_MM_AHB_DCAM_INT_2_MASK                BIT(25)
#define BIT_MM_AHB_DCAM_INT_1_MASK                BIT(24)
#define BIT_MM_AHB_DCAM_INT_0_MASK                BIT(23)
#define BIT_MM_AHB_CGM_DCAM_IF_FDIV_NUM(x)        (((x) & 0xF) << 19)
#define BIT_MM_AHB_CGM_DCAM_IF_FDIV_DENOM(x)      (((x) & 0xF) << 15)
#define BIT_MM_AHB_MIPI_CSI_DPHY_C2_SEL0(x)       (((x) & 0x7) << 12)
#define BIT_MM_AHB_MIPI_CSI_DPHY_C1_SEL1(x)       (((x) & 0x7) << 9)
#define BIT_MM_AHB_MIPI_CSI_DPHY_C1_SEL0(x)       (((x) & 0x7) << 6)
#define BIT_MM_AHB_MIPI_CSI_DPHY_C0_SEL1(x)       (((x) & 0x7) << 3)
#define BIT_MM_AHB_MIPI_CSI_DPHY_C0_SEL0(x)       (((x) & 0x7))

/* REG_MM_AHB_MM_0P5_APPEND */

#define BIT_MM_AHB_CSYSACK_SYNC_SEL_DCAM_AS       BIT(2)
#define BIT_MM_AHB_CACTIVE_SYNC_SEL_DCAM_AS       BIT(1)
#define BIT_MM_AHB_ISP_BUSY_LSLP_EN               BIT(0)

/* REG_MM_AHB_MM_IP_BUSY */

#define BIT_MM_AHB_FD_BUSY                        BIT(4)
#define BIT_MM_AHB_DCAM_BUSY                      BIT(3)
#define BIT_MM_AHB_ISP_BUSY                       BIT(2)
#define BIT_MM_AHB_JPG_BUSY                       BIT(1)
#define BIT_MM_AHB_CPP_BUSY                       BIT(0)

/* REG_MM_AHB_MM_AS_BDG_STATE */

#define BIT_MM_AHB_AXI_DETECTOR_OVERFLOW_MTX_AS   BIT(3)
#define BIT_MM_AHB_AXI_DETECTOR_OVERFLOW_DCAM_AS  BIT(2)
#define BIT_MM_AHB_BRIDGE_TRANS_IDLE_MTX_AS       BIT(1)
#define BIT_MM_AHB_BRIDGE_TRANS_IDLE_DCAM_AS      BIT(0)

/* REG_MM_AHB_MM_LPC_CTRL_FD */

#define BIT_MM_AHB_PU_NUM_FD(x)                   (((x) & 0xFF) << 24)
#define BIT_MM_AHB_LP_EB_FD                       BIT(16)
#define BIT_MM_AHB_LP_NUM_FD(x)                   (((x) & 0xFFFF))


#endif /* MM_AHB_H */


