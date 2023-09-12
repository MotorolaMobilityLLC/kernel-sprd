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

#ifndef __QOGIRN6PRO_MM_DVFS_REG_H____
#define __QOGIRN6PRO_MM_DVFS_REG_H____

/* registers definitions for controller REGS_MM_DVFS_AHB, 0x30014000 */
#define REG_MM_DVFS_AHB_CAMERA_DVFS_HOLD_CTRL              0x0000UL
#define REG_MM_DVFS_AHB_CAMERA_DVFS_WAIT_WINDOW_CFG0       0x0004UL
#define REG_MM_DVFS_AHB_MM_MIN_VOLTAGE_CFG                 0x0024UL
#define REG_MM_DVFS_AHB_MM_SW_DVFS_CTRL                    0x0038UL
#define REG_MM_DVFS_AHB_MM_FREQ_UPDATE_BYPASS              0x0048UL
#define REG_MM_DVFS_AHB_CGM_MM_DVFS_CLK_GATE_CTRL          0x0050UL
#define REG_MM_DVFS_AHB_MM_DVFS_VOLTAGE_DBG0               0x0054UL
#define REG_MM_DVFS_AHB_MM_DVFS_VOLTAGE_DBG1               0x0058UL
#define REG_MM_DVFS_AHB_MM_VDSP_DVFS_CGM_CFG_DBG           0x0068UL
#define REG_MM_DVFS_AHB_MM_VDMA_DVFS_CGM_CFG_DBG           0x006CUL
#define REG_MM_DVFS_AHB_MM_VDSP_MTX_DATA_DVFS_CGM_CFG_DBG  0x0070UL
#define REG_MM_DVFS_AHB_MM_ISP_DVFS_CGM_CFG_DBG            0x0074UL
#define REG_MM_DVFS_AHB_MM_CPP_DVFS_CGM_CFG_DBG            0x0078UL
#define REG_MM_DVFS_AHB_MM_DEPTH_DVFS_CGM_CFG_DBG          0x007CUL
#define REG_MM_DVFS_AHB_MM_FD_DVFS_CGM_CFG_DBG             0x0080UL
#define REG_MM_DVFS_AHB_MM_DCAM0_1_DVFS_CGM_CFG_DBG        0x0084UL
#define REG_MM_DVFS_AHB_MM_DCAM0_1_AXI_DVFS_CGM_CFG_DBG    0x0088UL
#define REG_MM_DVFS_AHB_MM_DCAM2_3_DVFS_CGM_CFG_DBG        0x008CUL
#define REG_MM_DVFS_AHB_MM_DCAM2_3_AXI_DVFS_CGM_CFG_DBG    0x0090UL
#define REG_MM_DVFS_AHB_MM_DCAM_MTX_DVFS_CGM_CFG_DBG       0x0094UL
#define REG_MM_DVFS_AHB_MM_MTX_DATA_DVFS_CGM_CFG_DBG       0x0098UL
#define REG_MM_DVFS_AHB_MM_JPG_DVFS_CGM_CFG_DBG            0x009CUL
#define REG_MM_DVFS_AHB_MM_DVFS_STATE_DBG                  0x0130UL

#define REG_MM_DVFS_AHB_VDSP_INDEX0_MAP    0x0154UL
#define REG_MM_DVFS_AHB_VDSP_INDEX1_MAP    0x0158UL
#define REG_MM_DVFS_AHB_VDSP_INDEX2_MAP    0x015CUL
#define REG_MM_DVFS_AHB_VDSP_INDEX3_MAP    0x0160UL
#define REG_MM_DVFS_AHB_VDSP_INDEX4_MAP    0x0164UL
#define REG_MM_DVFS_AHB_VDSP_INDEX5_MAP    0x0168UL
#define REG_MM_DVFS_AHB_VDSP_INDEX6_MAP    0x016CUL
#define REG_MM_DVFS_AHB_VDSP_INDEX7_MAP    0x0170UL

#define REG_MM_DVFS_AHB_VDMA_INDEX0_MAP    0x0174UL
#define REG_MM_DVFS_AHB_VDMA_INDEX1_MAP    0x0178UL
#define REG_MM_DVFS_AHB_VDMA_INDEX2_MAP    0x017CUL
#define REG_MM_DVFS_AHB_VDMA_INDEX3_MAP    0x0180UL
#define REG_MM_DVFS_AHB_VDMA_INDEX4_MAP    0x0184UL
#define REG_MM_DVFS_AHB_VDMA_INDEX5_MAP    0x0188UL
#define REG_MM_DVFS_AHB_VDMA_INDEX6_MAP    0x018CUL
#define REG_MM_DVFS_AHB_VDMA_INDEX7_MAP    0x0190UL

#define REG_MM_DVFS_AHB_VDSP_MTX_INDEX0_MAP    0x0194UL
#define REG_MM_DVFS_AHB_VDSP_MTX_INDEX1_MAP    0x0198UL
#define REG_MM_DVFS_AHB_VDSP_MTX_INDEX2_MAP    0x019CUL
#define REG_MM_DVFS_AHB_VDSP_MTX_INDEX3_MAP    0x01A0UL
#define REG_MM_DVFS_AHB_VDSP_MTX_INDEX4_MAP    0x01A4UL
#define REG_MM_DVFS_AHB_VDSP_MTX_INDEX5_MAP    0x01A8UL
#define REG_MM_DVFS_AHB_VDSP_MTX_INDEX6_MAP    0x01ACUL
#define REG_MM_DVFS_AHB_VDSP_MTX_INDEX7_MAP    0x01B0UL

#define REG_MM_DVFS_AHB_ISP_INDEX0_MAP     0x01B4UL
#define REG_MM_DVFS_AHB_ISP_INDEX1_MAP     0x01B8UL
#define REG_MM_DVFS_AHB_ISP_INDEX2_MAP     0x01BCUL
#define REG_MM_DVFS_AHB_ISP_INDEX3_MAP     0x01C0UL
#define REG_MM_DVFS_AHB_ISP_INDEX4_MAP     0x01C4UL
#define REG_MM_DVFS_AHB_ISP_INDEX5_MAP     0x01C8UL
#define REG_MM_DVFS_AHB_ISP_INDEX6_MAP     0x01CCUL
#define REG_MM_DVFS_AHB_ISP_INDEX7_MAP     0x01D0UL

#define REG_MM_DVFS_AHB_CPP_INDEX0_MAP     0x01D4UL
#define REG_MM_DVFS_AHB_CPP_INDEX1_MAP     0x01D8UL
#define REG_MM_DVFS_AHB_CPP_INDEX2_MAP     0x01DCUL
#define REG_MM_DVFS_AHB_CPP_INDEX3_MAP     0x01E0UL
#define REG_MM_DVFS_AHB_CPP_INDEX4_MAP     0x01E4UL
#define REG_MM_DVFS_AHB_CPP_INDEX5_MAP     0x01E8UL
#define REG_MM_DVFS_AHB_CPP_INDEX6_MAP     0x01ECUL
#define REG_MM_DVFS_AHB_CPP_INDEX7_MAP     0x01F0UL

#define REG_MM_DVFS_AHB_DEPTH_INDEX0_MAP   0x01F4UL
#define REG_MM_DVFS_AHB_DEPTH_INDEX1_MAP   0x01F8UL
#define REG_MM_DVFS_AHB_DEPTH_INDEX2_MAP   0x01FCUL
#define REG_MM_DVFS_AHB_DEPTH_INDEX3_MAP   0x0200UL
#define REG_MM_DVFS_AHB_DEPTH_INDEX4_MAP   0x0204UL
#define REG_MM_DVFS_AHB_DEPTH_INDEX5_MAP   0x0208UL
#define REG_MM_DVFS_AHB_DEPTH_INDEX6_MAP   0x020CUL
#define REG_MM_DVFS_AHB_DEPTH_INDEX7_MAP   0x0210UL

#define REG_MM_DVFS_AHB_FD_INDEX0_MAP      0x0214UL
#define REG_MM_DVFS_AHB_FD_INDEX1_MAP      0x0218UL
#define REG_MM_DVFS_AHB_FD_INDEX2_MAP      0x021CUL
#define REG_MM_DVFS_AHB_FD_INDEX3_MAP      0x0220UL
#define REG_MM_DVFS_AHB_FD_INDEX4_MAP      0x0224UL
#define REG_MM_DVFS_AHB_FD_INDEX5_MAP      0x0228UL
#define REG_MM_DVFS_AHB_FD_INDEX6_MAP      0x022CUL
#define REG_MM_DVFS_AHB_FD_INDEX7_MAP      0x0230UL

#define REG_MM_DVFS_AHB_DCAM0_1_INDEX0_MAP    0x0234UL
#define REG_MM_DVFS_AHB_DCAM0_1_INDEX1_MAP    0x0238UL
#define REG_MM_DVFS_AHB_DCAM0_1_INDEX2_MAP    0x023CUL
#define REG_MM_DVFS_AHB_DCAM0_1_INDEX3_MAP    0x0240UL
#define REG_MM_DVFS_AHB_DCAM0_1_INDEX4_MAP    0x0244UL
#define REG_MM_DVFS_AHB_DCAM0_1_INDEX5_MAP    0x0248UL
#define REG_MM_DVFS_AHB_DCAM0_1_INDEX6_MAP    0x024CUL
#define REG_MM_DVFS_AHB_DCAM0_1_INDEX7_MAP    0x0250UL
#define REG_MM_DVFS_AHB_DCAM0_1_AXI_INDEX0_MAP   0x0254UL
#define REG_MM_DVFS_AHB_DCAM0_1_AXI_INDEX1_MAP   0x0258UL
#define REG_MM_DVFS_AHB_DCAM0_1_AXI_INDEX2_MAP   0x025CUL
#define REG_MM_DVFS_AHB_DCAM0_1_AXI_INDEX3_MAP   0x0260UL
#define REG_MM_DVFS_AHB_DCAM0_1_AXI_INDEX4_MAP   0x0264UL
#define REG_MM_DVFS_AHB_DCAM0_1_AXI_INDEX5_MAP   0x0268UL
#define REG_MM_DVFS_AHB_DCAM0_1_AXI_INDEX6_MAP   0x026CUL
#define REG_MM_DVFS_AHB_DCAM0_1_AXI_INDEX7_MAP   0x0270UL

#define REG_MM_DVFS_AHB_DCAM2_3_INDEX0_MAP    0x0274UL
#define REG_MM_DVFS_AHB_DCAM2_3_INDEX1_MAP    0x0278UL
#define REG_MM_DVFS_AHB_DCAM2_3_INDEX2_MAP    0x027CUL
#define REG_MM_DVFS_AHB_DCAM2_3_INDEX3_MAP    0x0280UL
#define REG_MM_DVFS_AHB_DCAM2_3_INDEX4_MAP    0x0284UL
#define REG_MM_DVFS_AHB_DCAM2_3_INDEX5_MAP    0x0288UL
#define REG_MM_DVFS_AHB_DCAM2_3_INDEX6_MAP    0x028CUL
#define REG_MM_DVFS_AHB_DCAM2_3_INDEX7_MAP    0x0290UL
#define REG_MM_DVFS_AHB_DCAM2_3_AXI_INDEX0_MAP   0x0294UL
#define REG_MM_DVFS_AHB_DCAM2_3_AXI_INDEX1_MAP   0x0298UL
#define REG_MM_DVFS_AHB_DCAM2_3_AXI_INDEX2_MAP   0x029CUL
#define REG_MM_DVFS_AHB_DCAM2_3_AXI_INDEX3_MAP   0x02A0UL
#define REG_MM_DVFS_AHB_DCAM2_3_AXI_INDEX4_MAP   0x02A4UL
#define REG_MM_DVFS_AHB_DCAM2_3_AXI_INDEX5_MAP   0x02A8UL
#define REG_MM_DVFS_AHB_DCAM2_3_AXI_INDEX6_MAP   0x02ACUL
#define REG_MM_DVFS_AHB_DCAM2_3_AXI_INDEX7_MAP   0x02B0UL

#define REG_MM_DVFS_AHB_DCAM_MTX_INDEX0_MAP      0x02B4UL
#define REG_MM_DVFS_AHB_DCAM_MTX_INDEX1_MAP      0x02B8UL
#define REG_MM_DVFS_AHB_DCAM_MTX_INDEX2_MAP      0x02BCUL
#define REG_MM_DVFS_AHB_DCAM_MTX_INDEX3_MAP      0x02C0UL
#define REG_MM_DVFS_AHB_DCAM_MTX_INDEX4_MAP      0x02C4UL
#define REG_MM_DVFS_AHB_DCAM_MTX_INDEX5_MAP      0x02C8UL
#define REG_MM_DVFS_AHB_DCAM_MTX_INDEX6_MAP      0x02CCUL
#define REG_MM_DVFS_AHB_DCAM_MTX_INDEX7_MAP      0x02D0UL

#define REG_MM_DVFS_AHB_MM_MTX_DATA_INDEX0_MAP        0x02D4UL
#define REG_MM_DVFS_AHB_MM_MTX_DATA_INDEX1_MAP        0x02D8UL
#define REG_MM_DVFS_AHB_MM_MTX_DATA_INDEX2_MAP        0x02DCUL
#define REG_MM_DVFS_AHB_MM_MTX_DATA_INDEX3_MAP        0x02E0UL
#define REG_MM_DVFS_AHB_MM_MTX_DATA_INDEX4_MAP        0x02E4UL
#define REG_MM_DVFS_AHB_MM_MTX_DATA_INDEX5_MAP        0x02E8UL
#define REG_MM_DVFS_AHB_MM_MTX_DATA_INDEX6_MAP        0x02ECUL
#define REG_MM_DVFS_AHB_MM_MTX_DATA_INDEX7_MAP        0x02F0UL

#define REG_MM_DVFS_AHB_JPG_INDEX0_MAP           0x02F4UL
#define REG_MM_DVFS_AHB_JPG_INDEX1_MAP           0x02F8UL
#define REG_MM_DVFS_AHB_JPG_INDEX2_MAP           0x02FCUL
#define REG_MM_DVFS_AHB_JPG_INDEX3_MAP           0x0300UL
#define REG_MM_DVFS_AHB_JPG_INDEX4_MAP           0x0304UL
#define REG_MM_DVFS_AHB_JPG_INDEX5_MAP           0x0308UL
#define REG_MM_DVFS_AHB_JPG_INDEX6_MAP           0x030CUL
#define REG_MM_DVFS_AHB_JPG_INDEX7_MAP           0x0310UL

#define REG_MM_DVFS_AHB_VDSP_DVFS_INDEX_CFG               0x0794UL
#define REG_MM_DVFS_AHB_VDSP_DVFS_INDEX_IDLE_CFG          0x0798UL
#define REG_MM_DVFS_AHB_VDMA_DVFS_INDEX_CFG               0x079CUL
#define REG_MM_DVFS_AHB_VDMA_DVFS_INDEX_IDLE_CFG          0x07A0UL
#define REG_MM_DVFS_AHB_VDSP_MTX_DATA_DVFS_INDEX_CFG      0x07A4UL
#define REG_MM_DVFS_AHB_VDSP_MTX_DATA_DVFS_INDEX_IDLE_CFG 0x07A8UL


#define REG_MM_DVFS_AHB_ISP_DVFS_INDEX_CFG              0x07ACUL
#define REG_MM_DVFS_AHB_ISP_DVFS_INDEX_IDLE_CFG         0x07B0UL
#define REG_MM_DVFS_AHB_CPP_DVFS_INDEX_CFG              0x07B4UL
#define REG_MM_DVFS_AHB_CPP_DVFS_INDEX_IDLE_CFG         0x07B8UL
#define REG_MM_DVFS_AHB_DEPTH_DVFS_INDEX_CFG            0x07BCUL
#define REG_MM_DVFS_AHB_DEPTH_DVFS_INDEX_IDLE_CFG       0x07C0UL
#define REG_MM_DVFS_AHB_FD_DVFS_INDEX_CFG               0x07C4UL
#define REG_MM_DVFS_AHB_FD_DVFS_INDEX_IDLE_CFG          0x07C8UL
#define REG_MM_DVFS_AHB_DCAM0_1_DVFS_INDEX_CFG          0x07CCUL
#define REG_MM_DVFS_AHB_DCAM0_1_DVFS_INDEX_IDLE_CFG     0x07D0UL
#define REG_MM_DVFS_AHB_DCAM0_1_AXI_DVFS_INDEX_CFG      0x07D4UL
#define REG_MM_DVFS_AHB_DCAM0_1_AXI_DVFS_INDEX_IDLE_CFG 0x07D8UL
#define REG_MM_DVFS_AHB_DCAM2_3_DVFS_INDEX_CFG          0x07DCUL
#define REG_MM_DVFS_AHB_DCAM2_3_DVFS_INDEX_IDLE_CFG     0x07E0UL
#define REG_MM_DVFS_AHB_DCAM2_3_AXI_DVFS_INDEX_CFG      0x07E4UL
#define REG_MM_DVFS_AHB_DCAM2_3_AXI_DVFS_INDEX_IDLE_CFG 0x07E8UL
#define REG_MM_DVFS_AHB_DCAM_MTX_DVFS_INDEX_CFG         0x07ECUL
#define REG_MM_DVFS_AHB_DCAM_MTX_DVFS_INDEX_IDLE_CFG    0x07F0UL
#define REG_MM_DVFS_AHB_MM_MTX_DATA_DVFS_INDEX_CFG      0x07F4UL
#define REG_MM_DVFS_AHB_MM_MTX_DATA_DVFS_INDEX_IDLE_CFG 0x07F8UL
#define REG_MM_DVFS_AHB_JPG_DVFS_INDEX_CFG              0x07FCUL
#define REG_MM_DVFS_AHB_JPG_DVFS_INDEX_IDLE_CFG         0x0800UL


#define REG_MM_DVFS_AHB_FREQ_UPD_STATE0          0x0924UL
#define REG_MM_DVFS_AHB_FREQ_UPD_STATE1          0x0928UL
#define REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG0 0x0944UL
#define REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG1 0x0948UL
#define REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG2 0x094CUL
#define REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG3 0x0950UL
#define REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG4 0x0954UL
#define REG_MM_DVFS_AHB_MM_FREQ_UPD_TYPE_CFG0    0x09ACUL
#define REG_MM_DVFS_AHB_MM_DFS_IDLE_DISABLE_CFG0 0x09B4UL

#define REG_MM_DVFS_AHB_MM_DVFS_RESERVED_REG_CFG0 0x0A58UL
#define REG_MM_DVFS_AHB_MM_DVFS_RESERVED_REG_CFG1 0x0A5CUL
#define REG_MM_DVFS_AHB_MM_DVFS_RESERVED_REG_CFG2 0x0A60UL
#define REG_MM_DVFS_AHB_MM_DVFS_RESERVED_REG_CFG3 0x0A64UL

/*REG_MM_DVFS_AHB_MM_DVFS_HOLD_CTRL, 0x30014000 */
#define BIT_MM_DVFS_HOLD                (BIT(0))

/*REG_MM_DVFS_AHB_MM_DVFS_WAIT_WINDOW_CFG0, 0x30014004 */
#define BITS_MM_DVFS_UP_WINDOW(_X_)     ((_X_) << 16 & (BIT(16)|BIT(17)| \
	BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25) \
	|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)))
#define BITS_MM_DVFS_DOWN_WINDOW(_X_)   ((_X_) << 0  & \
	(BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9) \
	|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)))

/*REG_MM_DVFS_AHB_MM_FREQ_UPDATE_BYPASS0, 0x30014048 */
#define BIT_REG_JPG_FREQ_UPD_EN_BYP         (BIT(13))
#define BIT_REG_MM_MTX_DATA_FREQ_UPD_EN_BYP (BIT(12))
#define BIT_REG_DCAM_MTX_FREQ_UPD_EN_BYP    (BIT(11))
#define BIT_REG_DCAM2_3_AXI_FREQ_UPD_EN_BYP (BIT(10))
#define BIT_REG_DCAM2_3_FREQ_UPD_EN_BYP     (BIT(9))
#define BIT_REG_DCAM0_1_AXI_FREQ_UPD_EN_BYP (BIT(8))
#define BIT_REG_DCAM0_1_FREQ_UPD_EN_BYP     (BIT(7))
#define BIT_REG_FD_FREQ_UPD_EN_BYP          (BIT(6))
#define BIT_REG_DEPTH_FREQ_UPD_EN_BYP       (BIT(5))
#define BIT_REG_CPP_FREQ_UPD_EN_BYP         (BIT(4))
#define BIT_REG_ISP_FREQ_UPD_EN_BYP         (BIT(3))
#define BIT_REG_VDSP_MTX_DATA_FREQ_UPD_EN_BYP   (BIT(2))
#define BIT_REG_VDMA_FREQ_UPD_EN_BYP            (BIT(1))
#define BIT_REG_VDSP_FREQ_UPD_EN_BYP            (BIT(0))

#define SHIFT_BIT_REG_JPG_FREQ_UPD_EN_BYP         (13)
#define SHIFT_BIT_REG_MM_MTX_DATA_FREQ_UPD_EN_BYP (12)
#define SHIFT_BIT_REG_DCAM_MTX_FREQ_UPD_EN_BYP    (11)
#define SHIFT_BIT_REG_DCAM2_3_AXI_FREQ_UPD_EN_BYP (10)
#define SHIFT_BIT_REG_DCAM2_3_FREQ_UPD_EN_BYP     (9)
#define SHIFT_BIT_REG_DCAM0_1_AXI_FREQ_UPD_EN_BYP (8)
#define SHIFT_BIT_REG_DCAM0_1_FREQ_UPD_EN_BYP     (7)
#define SHIFT_BIT_REG_FD_FREQ_UPD_EN_BYP          (6)
#define SHIFT_BIT_REG_DEPTH_FREQ_UPD_EN_BYP       (5)
#define SHIFT_BIT_REG_CPP_FREQ_UPD_EN_BYP         (4)
#define SHIFT_BIT_REG_ISP_FREQ_UPD_EN_BYP         (3)
#define SHIFT_BIT_VDSP_MTX_DATA_FREQ_UPD_EN_BYP   (2)
#define SHIFT_BIT_VDMA_FREQ_UPD_EN_BYP            (1)
#define SHIFT_BIT_VDSP_FREQ_UPD_EN_BYP            (0)

/*REG_MM_DVFS_AHB_CGM_MM_DVFS_CLK_GATE_CTRL, 0x30014050 */
#define BIT_CGM_MM_DVFS_FORCE_EN        (BIT(1))
#define BIT_CGM_MM_DVFS_AUTO_GATE_SEL   (BIT(0))

/*REG_MM_DVFS_AHB_MM_DVFS_VOLTAGE_DBG0, 0x30014054 */
#define BITS_JPG_VOLTAGE(_X_)         ((_X_) << 28 & (BIT(28)|BIT(29)|BIT(30)|BIT(31)))
#define BITS_MM_MTX_DATA_VOLTAGE(_X_) ((_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)))
#define BITS_DCAM_MTX_VOLTAGE(_X_)    ((_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)))
#define BITS_DCAM2_3_AXI_VOLTAGE(_X_) ((_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)))
#define BITS_DCAM2_3_VOLTAGE(_X_)     ((_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)))
#define BITS_DCAM0_1_AXI_VOLTAGE(_X_) ((_X_) << 8  & (BIT(8)|BIT(9)|BIT(10)|BIT(11)))
#define BITS_DCAM0_1_VOLTAGE(_X_)     ((_X_) << 4  & (BIT(4)|BIT(5)|BIT(6)|BIT(7)))
#define BITS_MM_INTERNAL_VOTE_VOLTAGE(_X_) ((_X_) << 0  & \
(BIT(0)|BIT(1)|BIT(2)|BIT(3)))

/* shift and mask definitions additional for READ ONLY bits */
#define SHFT_BITS_JPG_VOLTAGE                 (28)
#define MASK_BITS_JPG_VOLTAGE                 (0xF)
#define SHFT_BITS_MM_MTX_DATA_VOLTAGE         (24)
#define MASK_BITS_MM_MTX_DATA_VOLTAGE         (0xF)
#define SHFT_BITS_DCAM_MTX_VOLTAGE            (20)
#define MASK_BITS_DCAM_MTX_VOLTAGE            (0xF)
#define SHFT_BITS_DCAM2_3_AXI_VOLTAGE         (16)
#define MASK_BITS_DCAM2_3_AXI_VOLTAGE         (0xF)
#define SHFT_BITS_DCAM2_3_VOLTAGE             (12)
#define MASK_BITS_DCAM2_3_VOLTAGE             (0xF)
#define SHFT_BITS_DCAM0_1_AXI_VOLTAGE         (8)
#define MASK_BITS_DCAM0_1_AXI_VOLTAGE         (0xF)
#define SHFT_BITS_DCAM0_1_VOLTAGE             (4)
#define MASK_BITS_DCAM0_1_VOLTAGE             (0xF)
#define SHFT_BITS_MM_INTERNAL_VOTE_VOLTAGE    (0)
#define MASK_BITS_MM_INTERNAL_VOTE_VOLTAGE    (0xF)


/*REG_MM_DVFS_AHB_MM_DVFS_VOLTAGE_DBG1, 0x30014058 */
#define BITS_FD_VOLTAGE(_X_)            ((_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)))
#define BITS_DEPTH_VOLTAGE(_X_)         ((_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)))
#define BITS_CPP_VOLTAGE(_X_)           ((_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)))
#define BITS_ISP_VOLTAGE(_X_)           ((_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)))
#define BITS_VDSP_MTX_DATA_VOLTAGE(_X_) ((_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)))
#define BITS_VDMA_VOLTAGE(_X_)          ((_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)))
#define BITS_VDSP_VOLTAGE(_X_)          ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)|BIT(3)))

/* shift and mask definitions additional for READ ONLY bits */
#define SHFT_BITS_FD_VOLTAGE             (24)
#define MASK_BITS_FD_VOLTAGE             (0xF)
#define SHFT_BITS_DEPTH_VOLTAGE          (20)
#define MASK_BITS_DEPTH_VOLTAGE          (0xF)
#define SHFT_BITS_CPP_VOLTAGE            (16)
#define MASK_BITS_CPP_VOLTAGE            (0xF)
#define SHFT_BITS_ISP_VOLTAGE            (12)
#define MASK_BITS_ISP_VOLTAGE            (0xF)
#define SHFT_BITS_VDSP_MTX_DATA_VOLTAGE  (8)
#define MASK_BITS_VDSP_MTX_DATA_VOLTAGE  (0xF)
#define SHFT_BITS_VDMA_VOLTAGE           (4)
#define MASK_BITS_VDMA_VOLTAGE           (0xF)
#define SHFT_BITS_VDSP_VOLTAGE           (0)
#define MASK_BITS_VDSP_VOLTAGE           (0xF)


/* REG_MM_DVFS_AHB_MM_VDSP_CGM_CFG_DBG, 0x30014068 */
#define BITS_CGM_VDSP_M_DIV_DVFS(_X_) ((_X_) << 3  & (BIT(3)|BIT(4)))
#define BITS_CGM_VDSP_SEL_DVFS(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_MM_VDMA_CGM_CFG_DBG, 0x3001406C */
#define BITS_CGM_VDMA_SEL_DVFS(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_MM_VDSP_MTX_DATA_DVFS_CGM_CFG_DBG, 0x30014070 */
#define BITS_CGM_VDSP_MTX_DATA_SEL_DVFS(_X_)  ((_X_) << 0  & (BIT(0)|BIT(1)))

/* REG_MM_DVFS_AHB_MM_ISP_DVFS_CGM_CFG_DBG, 0x30014074 */
#define BITS_CGM_ISP_SEL_DVFS(_X_)  ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_MM_CPP_DVFS_CGM_CFG_DBG, 0x30014078 */
#define BITS_CGM_CPP_SEL_DVFS(_X_)  ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_MM_DEPTH_DVFS_CGM_CFG_DBG, 0x3001407C */
#define BITS_CGM_DEPTH_SEL_DVFS(_X_)  ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_MM_FD_DVFS_CGM_CFG_DBG, 0x30014080 */
#define BITS_CGM_FD_SEL_DVFS(_X_)  ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* shift and mask definitions additional for READ ONLY bits */
#define SHFT_BITS_CGM_VDSP_M_DIV_DVFS        (3)
#define MASK_BITS_CGM_VDSP_M_DIV_DVFS        (0x3)
#define SHFT_BITS_CGM_VDSP_SEL_DVFS          (0)
#define MASK_BITS_CGM_VDSP_SEL_DVFS          (0x7)
#define SHFT_BITS_CGM_VDMA_SEL_DVFS          (0)
#define MASK_BITS_CGM_VDMA_SEL_DVFS          (0x7)
#define SHFT_BITS_CGM_VDSP_MTX_DATA_SEL_DVFS (0)
#define MASK_BITS_CGM_VDSP_MTX_DATA_SEL_DVFS (0x3)
#define SHFT_BITS_CGM_ISP_SEL_DVFS           (0)
#define MASK_BITS_CGM_ISP_SEL_DVFS           (0x7)
#define SHFT_BITS_CGM_CPP_SEL_DVFS           (0)
#define MASK_BITS_CGM_CPP_SEL_DVFS           (0x7)
#define SHFT_BITS_CGM_DEPTH_SEL_DVFS         (0)
#define MASK_BITS_CGM_DEPTH_SEL_DVFS         (0x7)
#define SHFT_BITS_CGM_FD_SEL_DVFS            (0)
#define MASK_BITS_CGM_FD_SEL_DVFS            (0x7)



/* REG_MM_DVFS_AHB_MM_DCAM0_1_DVFS_CGM_CFG_DBG, 0x30014084 */
#define BITS_CGM_DCAM0_1_SEL_DVFS(_X_)  ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_MM_DCAM0_1_AXI_DVFS_CGM_CFG_DBG, 0x30014088 */
#define BITS_CGM_DCAM0_1_AXI_SEL_DVFS(_X_)  ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_MM_DCAM2_3_DVFS_CGM_CFG_DBG, 0x3001408C */
#define BITS_CGM_DCAM2_3_SEL_DVFS(_X_)  ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_MM_DCAM2_3_AXI_DVFS_CGM_CFG_DBG, 0x30014090 */
#define BITS_CGM_DCAM2_3_AXI_SEL_DVFS(_X_)  ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_MM_DCAM_MTX_DVFS_CGM_CFG_DBG, 0x30014094 */
#define BITS_CGM_DCAM_MTX_SEL_DVFS(_X_)  ((_X_) << 0  & (BIT(0)|BIT(1)))

/* REG_MM_DVFS_AHB_MM_MTX_DATA_DVFS_CGM_CFG_DBG, 0x30014098 */
#define BITS_CGM_MM_MTX_DATA_SEL_DVFS(_X_)  ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_MM_JPG_DVFS_CGM_CFG_DBG, 0x3001409C */
#define BITS_CGM_JPG_SEL_DVFS(_X_)  ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/* shift and mask definitions additional for READ ONLY bits */
#define SHFT_BITS_CGM_DCAM0_1_SEL_DVFS        (0)
#define MASK_BITS_CGM_DCAM0_1_SEL_DVFS        (0x7)
#define SHFT_BITS_CGM_DCAM0_1_AXI_SEL_DVFS    (0)
#define MASK_BITS_CGM_DCAM0_1_AXI_SEL_DVFS    (0x7)
#define SHFT_BITS_CGM_DCAM2_3_SEL_DVFS        (0)
#define MASK_BITS_CGM_DCAM2_3_SEL_DVFS        (0x7)
#define SHFT_BITS_CGM_DCAM2_3_AXI_SEL_DVFS    (0)
#define MASK_BITS_CGM_DCAM2_3_AXI_SEL_DVFS    (0x7)
#define SHFT_BITS_CGM_DCAM_MTX_SEL_DVFS       (0)
#define MASK_BITS_CGM_DCAM_MTX_SEL_DVFS       (0x3)
#define SHFT_BITS_CGM_MM_MTX_DATA_SEL_DVFS    (0)
#define MASK_BITS_CGM_MM_MTX_DATA_SEL_DVFS    (0x7)
#define SHFT_BITS_CGM_JPG_SEL_DVFS            (0)
#define MASK_BITS_CGM_JPG_SEL_DVFS            (0x7)


/* REG_MM_DVFS_AHB_MM_DVFS_STATE_DBG, 0x30014130 */
/* No interface so far*/
#define BIT_MM_SYS_DVFS_BUSY                (BIT(28))
#define BITS_MM_DVFS_WINDOW_CNT(_X_)  ((_X_) << 12 & (BIT(12)|BIT(13)| \
	BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)| \
	BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)))
#define BITS_MM_CURRENT_VOLTAGE(_X_)  ((_X_) << 8 & (BIT(8)|BIT(9)| \
	BIT(10)|BIT(11)))
#define BITS_MM_DVFS_STATE(_X_)       ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))
/* shift and mask definitions additional for READ ONLY bits */
#define SHFT_BITS_MM_SYS_DVFS_BUSY         (28)
#define MASK_BITS_MM_SYS_DVFS_BUSY         (0x1)
#define SHFT_BITS_MM_DVFS_WINDOW_CNT       (12)
#define MASK_BITS_MM_DVFS_WINDOW_CNT       (0xFFFF)
#define SHFT_BITS_MM_CURRENT_VOLTAGE       (8)
#define MASK_BITS_MM_CURRENT_VOLTAGE       (0xF)
#define SHFT_BITS_MM_DVFS_STATE            (0)
#define MASK_BITS_MM_DVFS_STATE            (0x7)

//VDSP
/* bits definitions for register REG_MM_DVFS_AHB_VDSP_INDEX0_MAP, 0x30014154 */
#define BITS_VDSP_VOL_INDEX0(_X_)       ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)|BIT(8)))
#define BITS_CGM_VDSP_M_DIV_INDEX0(_X_) ((_X_) << 3  & (BIT(3)|BIT(4)))
#define BITS_CGM_VDSP_SEL_INDEX0(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_VDSP_INDEX1_MAP, 0x30014158 */
#define BITS_VDSP_VOL_INDEX1(_X_)       ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)|BIT(8)))
#define BITS_CGM_VDSP_M_DIV_INDEX1(_X_) ((_X_) << 3  & (BIT(3)|BIT(4)))
#define BITS_CGM_VDSP_SEL_INDEX1(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_VDSP_INDEX2_MAP, 0x3001415C */
#define BITS_VDSP_VOL_INDEX2(_X_)       ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)|BIT(8)))
#define BITS_CGM_VDSP_M_DIV_INDEX2(_X_) ((_X_) << 3  & (BIT(3)|BIT(4)))
#define BITS_CGM_VDSP_SEL_INDEX2(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_VDSP_INDEX3_MAP, 0x30014160 */
#define BITS_VDSP_VOL_INDEX3(_X_)       ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)|BIT(8)))
#define BITS_CGM_VDSP_M_DIV_INDEX3(_X_) ((_X_) << 3  & (BIT(3)|BIT(4)))
#define BITS_CGM_VDSP_SEL_INDEX3(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_VDSP_INDEX4_MAP, 0x30014164 */
#define BITS_VDSP_VOL_INDEX4(_X_)       ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)|BIT(8)))
#define BITS_CGM_VDSP_M_DIV_INDEX4(_X_) ((_X_) << 3  & (BIT(3)|BIT(4)))
#define BITS_CGM_VDSP_SEL_INDEX4(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_VDSP_INDEX5_MAP, 0x30014168 */
#define BITS_VDSP_VOL_INDEX5(_X_)       ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)|BIT(8)))
#define BITS_CGM_VDSP_M_DIV_INDEX5(_X_) ((_X_) << 3  & (BIT(3)|BIT(4)))
#define BITS_CGM_VDSP_SEL_INDEX5(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_VDSP_INDEX6_MAP, 0x3001416C */
#define BITS_VDSP_VOL_INDEX6(_X_)       ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)|BIT(8)))
#define BITS_CGM_VDSP_M_DIV_INDEX6(_X_) ((_X_) << 3  & (BIT(3)|BIT(4)))
#define BITS_CGM_VDSP_SEL_INDEX6(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_VDSP_INDEX7_MAP, 0x30014170 */
#define BITS_VDSP_VOL_INDEX7(_X_)       ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)|BIT(8)))
#define BITS_CGM_VDSP_M_DIV_INDEX7(_X_) ((_X_) << 3  & (BIT(3)|BIT(4)))
#define BITS_CGM_VDSP_SEL_INDEX7(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


//VDMA
/* bits definitions for register REG_MM_DVFS_AHB_VDMA_INDEX0_MAP, 0x30014174 */
#define BITS_VDMA_VOL_INDEX0(_X_)       ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_VDMA_SEL_INDEX0(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_VDMA_INDEX1_MAP, 0x30014178 */
#define BITS_VDMA_VOL_INDEX1(_X_)       ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_VDMA_SEL_INDEX1(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_VDMA_INDEX2_MAP, 0x3001417C */
#define BITS_VDMA_VOL_INDEX2(_X_)       ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_VDMA_SEL_INDEX2(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_VDMA_INDEX3_MAP, 0x30014180 */
#define BITS_VDMA_VOL_INDEX3(_X_)       ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_VDMA_SEL_INDEX3(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_VDMA_INDEX4_MAP, 0x30014184 */
#define BITS_VDMA_VOL_INDEX4(_X_)       ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_VDMA_SEL_INDEX4(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_VDMA_INDEX5_MAP, 0x30014188 */
#define BITS_VDMA_VOL_INDEX5(_X_)       ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_VDMA_SEL_INDEX5(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_VDMA_INDEX6_MAP, 0x3001418C */
#define BITS_VDMA_VOL_INDEX6(_X_)       ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_VDMA_SEL_INDEX6(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_VDMA_INDEX7_MAP, 0x30014190 */
#define BITS_VDMA_VOL_INDEX7(_X_)       ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_VDMA_SEL_INDEX7(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

//VDSP_MTX_DATA
/* bits definitions for register REG_MM_DVFS_AHB_VDMA_IMTX_DATA_INDEX0_MAP, 0x30014194 */
#define BITS_VDSP_MTX_DATA_VOL_INDEX0(_X_) ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_VDSP_MTX_DATA_SEL_INDEX0(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_VDMA_IMTX_DATA_INDEX1_MAP, 0x30014198 */
#define BITS_VDSP_MTX_DATA_VOL_INDEX1(_X_) ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_VDSP_MTX_DATA_SEL_INDEX1(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_VDMA_IMTX_DATA_INDEX2_MAP, 0x3001419C */
#define BITS_VDSP_MTX_DATA_VOL_INDEX2(_X_) ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_VDSP_MTX_DATA_SEL_INDEX2(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_VDMA_IMTX_DATA_INDEX3_MAP, 0x300141A0 */
#define BITS_VDSP_MTX_DATA_VOL_INDEX3(_X_) ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_VDSP_MTX_DATA_SEL_INDEX3(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_VDMA_IMTX_DATA_INDEX4_MAP, 0x300141A4 */
#define BITS_VDSP_MTX_DATA_VOL_INDEX4(_X_) ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_VDSP_MTX_DATA_SEL_INDEX4(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_VDMA_IMTX_DATA_INDEX5_MAP, 0x300141A8 */
#define BITS_VDSP_MTX_DATA_VOL_INDEX5(_X_) ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_VDSP_MTX_DATA_SEL_INDEX5(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_VDMA_IMTX_DATA_INDEX6_MAP, 0x300141AC */
#define BITS_VDSP_MTX_DATA_VOL_INDEX6(_X_) ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_VDSP_MTX_DATA_SEL_INDEX6(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_VDMA_IMTX_DATA_INDEX7_MAP, 0x300141B0 */
#define BITS_VDSP_MTX_DATA_VOL_INDEX7(_X_) ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_VDSP_MTX_DATA_SEL_INDEX7(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)))


// ISP
/* bits definitions for register REG_MM_DVFS_AHB_ISP_INDEX0_MAP, 0x300141B4 */
#define BITS_ISP_VOL_INDEX0(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_ISP_SEL_INDEX0(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_ISP_INDEX1_MAP, 0x300141B8 */
#define BITS_ISP_VOL_INDEX1(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_ISP_SEL_INDEX1(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_ISP_INDEX2_MAP, 0x300141BC */
#define BITS_ISP_VOL_INDEX2(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_ISP_SEL_INDEX2(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_ISP_INDEX3_MAP, 0x300140C0 */
#define BITS_ISP_VOL_INDEX3(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_ISP_SEL_INDEX3(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_ISP_INDEX4_MAP, 0x300140C4 */
#define BITS_ISP_VOL_INDEX4(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_ISP_SEL_INDEX4(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/* bits definitions for register REG_MM_DVFS_AHB_ISP_INDEX5_MAP, 0x300140C8 */
#define BITS_ISP_VOL_INDEX5(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_ISP_SEL_INDEX5(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/* bits definitions for register REG_MM_DVFS_AHB_ISP_INDEX6_MAP, 0x300140CC */
#define BITS_ISP_VOL_INDEX6(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_ISP_SEL_INDEX6(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/* bits definitions for register REG_MM_DVFS_AHB_ISP_INDEX7_MAP, 0x300140D0 */
#define BITS_ISP_VOL_INDEX7(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_ISP_SEL_INDEX7(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

// CPP
/* bits definitions for register REG_MM_DVFS_AHB_CPP_INDEX0_MAP, 0x300140D4 */
#define BITS_CPP_VOL_INDEX0(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_CPP_SEL_INDEX0(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_CPP_INDEX1_MAP, 0x300140D8 */
#define BITS_CPP_VOL_INDEX1(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_CPP_SEL_INDEX1(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_CPP_INDEX2_MAP, 0x300140DC */
#define BITS_CPP_VOL_INDEX2(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_CPP_SEL_INDEX2(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_CPP_INDEX3_MAP, 0x300140E0 */
#define BITS_CPP_VOL_INDEX3(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_CPP_SEL_INDEX3(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_CPP_INDEX4_MAP, 0x300141E4 */
#define BITS_CPP_VOL_INDEX4(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_CPP_SEL_INDEX4(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_CPP_INDEX5_MAP, 0x300141E8 */
#define BITS_CPP_VOL_INDEX5(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_CPP_SEL_INDEX5(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_CPP_INDEX6_MAP, 0x300141EC */
#define BITS_CPP_VOL_INDEX6(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_CPP_SEL_INDEX6(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_CPP_INDEX7_MAP, 0x300141F0 */
#define BITS_CPP_VOL_INDEX7(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_CPP_SEL_INDEX7(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

//DEPTH
/* bits definitions for register REG_MM_DVFS_AHB_DEPTH_INDEX0_MAP, 0x300140F4 */
#define BITS_DEPTH_VOL_INDEX0(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DEPTH_SEL_INDEX0(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_DEPTH_INDEX1_MAP, 0x300140F8 */
#define BITS_DEPTH_VOL_INDEX1(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DEPTH_SEL_INDEX1(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_DEPTH_INDEX2_MAP, 0x300140FC */
#define BITS_DEPTH_VOL_INDEX2(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DEPTH_SEL_INDEX2(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_DEPTH_INDEX3_MAP, 0x30014200 */
#define BITS_DEPTH_VOL_INDEX3(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DEPTH_SEL_INDEX3(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_DEPTH_INDEX4_MAP, 0x30014204 */
#define BITS_DEPTH_VOL_INDEX4(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DEPTH_SEL_INDEX4(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_DEPTH_INDEX5_MAP, 0x30014208 */
#define BITS_DEPTH_VOL_INDEX5(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DEPTH_SEL_INDEX5(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_DEPTH_INDEX6_MAP, 0x3001420C */
#define BITS_DEPTH_VOL_INDEX6(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DEPTH_SEL_INDEX6(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_DEPTH_INDEX7_MAP, 0x30014210 */
#define BITS_DEPTH_VOL_INDEX7(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DEPTH_SEL_INDEX7(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

//FD
/* bits definitions for register REG_MM_DVFS_AHB_FD_INDEX0_MAP, 0x30014214 */
#define BITS_FD_VOL_INDEX0(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_FD_SEL_INDEX0(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_FD_INDEX1_MAP, 0x30014218 */
#define BITS_FD_VOL_INDEX1(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_FD_SEL_INDEX1(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_FD_INDEX2_MAP, 0x3001421C */
#define BITS_FD_VOL_INDEX2(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_FD_SEL_INDEX2(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_FD_INDEX3_MAP, 0x30014220 */
#define BITS_FD_VOL_INDEX3(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_FD_SEL_INDEX3(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_FD_INDEX4_MAP, 0x30014224 */
#define BITS_FD_VOL_INDEX4(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_FD_SEL_INDEX4(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_FD_INDEX5_MAP, 0x62600228 */
#define BITS_FD_VOL_INDEX5(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_FD_SEL_INDEX5(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_FD_INDEX6_MAP, 0x3001422C */
#define BITS_FD_VOL_INDEX6(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_FD_SEL_INDEX6(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_FD_INDEX7_MAP, 0x30014230 */
#define BITS_FD_VOL_INDEX7(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_FD_SEL_INDEX7(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


//DCAM0_1
/*REG_MM_DVFS_AHB_DCAM0_1_INDEX0_MAP, 0x30014234 */
#define BITS_DCAM0_1_VOL_INDEX0(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM0_1_SEL_INDEX0(_X_)  ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_DCAM0_1_INDEX1_MAP, 0x30014238 */
#define BITS_DCAM0_1_VOL_INDEX1(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM0_1_SEL_INDEX1(_X_)  ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_DCAM0_1_INDEX2_MAP, 0x3001423C */
#define BITS_DCAM0_1_VOL_INDEX2(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM0_1_SEL_INDEX2(_X_)  ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_DCAM0_1_INDEX3_MAP, 0x30014240 */
#define BITS_DCAM0_1_VOL_INDEX3(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM0_1_SEL_INDEX3(_X_)  ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_DCAM0_1_INDEX4_MAP, 0x30014244 */
#define BITS_DCAM0_1_VOL_INDEX4(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM0_1_SEL_INDEX4(_X_)  ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_DCAM0_1_INDEX5_MAP, 0x30014248 */
#define BITS_DCAM0_1_VOL_INDEX5(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM0_1_SEL_INDEX5(_X_)  ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_DCAM0_1_INDEX6_MAP, 0x3001424C */
#define BITS_DCAM0_1_VOL_INDEX6(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM0_1_SEL_INDEX6(_X_)  ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_DCAM0_1_INDEX7_MAP, 0x30014250 */
#define BITS_DCAM0_1_VOL_INDEX7(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM0_1_SEL_INDEX7(_X_)  ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))


/* REG_MM_DVFS_AHB_DCAM0_1_AXI_INDEX0_MAP, 0x30014254 */
#define BITS_DCAM0_1_AXI_VOL_INDEX0(_X_) ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM0_1_AXI_SEL_INDEX0(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_DCAM_AXI_INDEX1_MAP, 0x30014258 */
#define BITS_DCAM0_1_AXI_VOL_INDEX1(_X_) ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM0_1_AXI_SEL_INDEX1(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/* REG_MM_DVFS_AHB_DCAM0_1_AXI_INDEX2_MAP, 0x3001425C */
#define BITS_DCAM0_1_AXI_VOL_INDEX2(_X_) ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM0_1_AXI_SEL_INDEX2(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/* REG_MM_DVFS_AHB_DCAM0_1_AXI_INDEX3_MAP, 0x30014260 */
#define BITS_DCAM0_1_AXI_VOL_INDEX3(_X_) ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM0_1_AXI_SEL_INDEX3(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/*  REG_MM_DVFS_AHB_DCAM0_1_AXI_INDEX4_MAP, 0x30014264 */
#define BITS_DCAM0_1_AXI_VOL_INDEX4(_X_) ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM0_1_AXI_SEL_INDEX4(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/* REG_MM_DVFS_AHB_DCAM0_1_AXI_INDEX5_MAP, 0x30014268 */
#define BITS_DCAM0_1_AXI_VOL_INDEX5(_X_) ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM0_1_AXI_SEL_INDEX5(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/* REG_MM_DVFS_AHB_DCAM0_1_AXI_INDEX6_MAP, 0x3001426C */
#define BITS_DCAM0_1_AXI_VOL_INDEX6(_X_) ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM0_1_AXI_SEL_INDEX6(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/*REG_MM_DVFS_AHB_DCAM0_1_AXI_INDEX7_MAP, 0x30014270 */
#define BITS_DCAM0_1_AXI_VOL_INDEX7(_X_) ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM0_1_AXI_SEL_INDEX7(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


//DCAM2_3
/*REG_MM_DVFS_AHB_DCAM2_3_INDEX0_MAP, 0x30014274 */
#define BITS_DCAM2_3_VOL_INDEX0(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM2_3_SEL_INDEX0(_X_)  ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_DCAM2_3_INDEX1_MAP, 0x30014278 */
#define BITS_DCAM2_3_VOL_INDEX1(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM2_3_SEL_INDEX1(_X_)  ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_DCAM2_3_INDEX2_MAP, 0x3001427C */
#define BITS_DCAM2_3_VOL_INDEX2(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM2_3_SEL_INDEX2(_X_)  ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_DCAM2_3_INDEX3_MAP, 0x30014280 */
#define BITS_DCAM2_3_VOL_INDEX3(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM2_3_SEL_INDEX3(_X_)  ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_DCAM2_3_INDEX4_MAP, 0x30014284 */
#define BITS_DCAM2_3_VOL_INDEX4(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM2_3_SEL_INDEX4(_X_)  ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_DCAM2_3_INDEX5_MAP, 0x30014288 */
#define BITS_DCAM2_3_VOL_INDEX5(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM2_3_SEL_INDEX5(_X_)  ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_DCAM2_3_INDEX6_MAP, 0x3001428C */
#define BITS_DCAM2_3_VOL_INDEX6(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM2_3_SEL_INDEX6(_X_)  ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_DCAM2_3_INDEX7_MAP, 0x30014290 */
#define BITS_DCAM2_3_VOL_INDEX7(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM2_3_SEL_INDEX7(_X_)  ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))


/* REG_MM_DVFS_AHB_DCAM2_3_AXI_INDEX0_MAP, 0x30014294 */
#define BITS_DCAM2_3_AXI_VOL_INDEX0(_X_) ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM2_3_AXI_SEL_INDEX0(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_DCAM2_3_AXI_INDEX1_MAP, 0x30014298 */
#define BITS_DCAM2_3_AXI_VOL_INDEX1(_X_) ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM2_3_AXI_SEL_INDEX1(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/* REG_MM_DVFS_AHB_DCAM2_3_AXI_INDEX2_MAP, 0x3001429C */
#define BITS_DCAM2_3_AXI_VOL_INDEX2(_X_) ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM2_3_AXI_SEL_INDEX2(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/* REG_MM_DVFS_AHB_DCAM2_3_AXI_INDEX3_MAP, 0x300142A0 */
#define BITS_DCAM2_3_AXI_VOL_INDEX3(_X_) ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM2_3_AXI_SEL_INDEX3(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/*  REG_MM_DVFS_AHB_DCAM2_3_AXI_INDEX4_MAP, 0x300142A4 */
#define BITS_DCAM2_3_AXI_VOL_INDEX4(_X_) ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM2_3_AXI_SEL_INDEX4(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/* REG_MM_DVFS_AHB_DCAM2_3_AXI_INDEX5_MAP, 0x300142A8 */
#define BITS_DCAM2_3_AXI_VOL_INDEX5(_X_) ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM2_3_AXI_SEL_INDEX5(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/* REG_MM_DVFS_AHB_DCAM2_3_AXI_INDEX6_MAP, 0x300142AC */
#define BITS_DCAM2_3_AXI_VOL_INDEX6(_X_) ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM2_3_AXI_SEL_INDEX6(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/*REG_MM_DVFS_AHB_DCAM2_3_AXI_INDEX7_MAP, 0x300142B0 */
#define BITS_DCAM2_3_AXI_VOL_INDEX7(_X_) ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM2_3_AXI_SEL_INDEX7(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

//DCAM_MTX
#define BITS_DCAM_MTX_VOL_INDEX0(_X_) ((_X_) << 2 & (BIT(2)|BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_DCAM_MTX_SEL_INDEX0(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)))

//MM
/* REG_MM_DVFS_AHB_MM_MTX_DATA_INDEX0_MAP, 0x300142D4 */
#define BITS_MM_MTX_DATA_VOL_INDEX0(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_MM_MTX_DATA_SEL_INDEX0(_X_) ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_MM_MTX_DATA_INDEX1_MAP, 0x300142D8 */
#define BITS_MM_MTX_DATA_VOL_INDEX1(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_MM_MTX_DATA_SEL_INDEX1(_X_) ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))


/* REG_MM_DVFS_AHB_MM_MTX_DATA_INDEX2_MAP, 0x300142DC */
#define BITS_MM_MTX_DATA_VOL_INDEX2(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_MM_MTX_DATA_SEL_INDEX2(_X_) ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))


/* REG_MM_DVFS_AHB_MM_MTX_DATA_INDEX3_MAP, 0x626002E0 */
#define BITS_MM_MTX_DATA_VOL_INDEX3(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_MM_MTX_DATA_SEL_INDEX3(_X_) ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))


/* REG_MM_DVFS_AHB_MM_MTX_DATA_INDEX4_MAP, 0x300142E4 */
#define BITS_MM_MTX_DATA_VOL_INDEX4(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_MM_MTX_DATA_SEL_INDEX4(_X_) ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))


/* REG_MM_DVFS_AHB_MM_MTX_DATA_INDEX5_MAP, 0x300142E8 */
#define BITS_MM_MTX_DATA_VOL_INDEX5(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_MM_MTX_DATA_SEL_INDEX5(_X_) ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))


/* REG_MM_DVFS_AHB_MM_MTX_DATA_INDEX6_MAP, 0x300142EC */
#define BITS_MM_MTX_DATA_VOL_INDEX6(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_MM_MTX_DATA_SEL_INDEX6(_X_) ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))


/* REG_MM_DVFS_AHB_MM_MTX_DATA_INDEX7_MAP, 0x300142F0 */
#define BITS_MM_MTX_DATA_VOL_INDEX7(_X_)  ((_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_MM_MTX_DATA_SEL_INDEX7(_X_) ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))

//JPG
/* bits definitions for register REG_MM_DVFS_AHB_JPG_INDEX0_MAP, 0x300142F4 */
#define BITS_JPG_VOL_INDEX0(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_JPG_SEL_INDEX0(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_JPG_INDEX1_MAP, 0x300142F8 */
#define BITS_JPG_VOL_INDEX1(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_JPG_SEL_INDEX1(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_JPG_INDEX2_MAP, 0x300142FC */
#define BITS_JPG_VOL_INDEX2(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_JPG_SEL_INDEX2(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_JPG_INDEX3_MAP, 0x30014300 */
#define BITS_JPG_VOL_INDEX3(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_JPG_SEL_INDEX3(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_JPG_INDEX4_MAP, 0x30014304 */
#define BITS_JPG_VOL_INDEX4(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_JPG_SEL_INDEX4(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_JPG_INDEX5_MAP, 0x30014308 */
#define BITS_JPG_VOL_INDEX5(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_JPG_SEL_INDEX5(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_JPG_INDEX6_MAP, 0x3001430C */
#define BITS_JPG_VOL_INDEX6(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_JPG_SEL_INDEX6(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_JPG_INDEX7_MAP, 0x30014310 */
#define BITS_JPG_VOL_INDEX7(_X_)        ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_JPG_SEL_INDEX7(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

//DVFS_INDEX_CFG
/*  REG_MM_DVFS_AHB_VDSP_DVFS_INDEX_CFG, 0x30014794 */
#define BITS_VDSP_DVFS_INDEX(_X_)        ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_VDSP_DVFS_INDEX_IDLE_CFG, 0x30014798 */
#define BITS_VDSP_DVFS_INDEX_IDLE(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_VDMA_DVFS_INDEX_CFG, 0x3001479C */
#define BITS_VDMA_DVFS_INDEX(_X_)        ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_VDSP_DVFS_INDEX_IDLE_CFG, 0x300147A0 */
#define BITS_VDMA_DVFS_INDEX_IDLE(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_VDSP_MTX_DATA_DVFS_INDEX_CFG, 0x300147A4 */
#define BITS_VDSP_MTX_DATA_DVFS_INDEX(_X_)        ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_VDSP_MTX_DATA_DVFS_INDEX_IDLE_CFG, 0x300147A8 */
#define BITS_VDSP_MTX_DATA_DVFS_INDEX_IDLE(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_ISP_DVFS_INDEX_CFG, 0x300147AC */
#define BITS_ISP_DVFS_INDEX(_X_)        ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_ISP_DVFS_INDEX_IDLE_CFG, 0x300147B0 */
#define BITS_ISP_DVFS_INDEX_IDLE(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/*REG_MM_DVFS_AHB_CPP_DVFS_INDEX_CFG, 0x300147B4 */
#define BITS_CPP_DVFS_INDEX(_X_)        ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_CPP_DVFS_INDEX_IDLE_CFG, 0x300147B8 */
#define BITS_CPP_DVFS_INDEX_IDLE(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_DEPTH_DVFS_INDEX_CFG, 0x300147BC */
#define BITS_DEPTH_DVFS_INDEX(_X_)        ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_DEPTH_DVFS_INDEX_IDLE_CFG, 0x300147C0 */
#define BITS_DEPTH_DVFS_INDEX_IDLE(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))


/* REG_MM_DVFS_AHB_FD_DVFS_INDEX_CFG, 0x300147C4 */
#define BITS_FD_DVFS_INDEX(_X_)         ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_FD_DVFS_INDEX_IDLE_CFG, 0x300147C8 */
#define BITS_FD_DVFS_INDEX_IDLE(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* _MM_DVFS_AHB_DCAM0_1_DVFS_INDEX_CFG, 0x300147CC */
#define BITS_DCAM0_1_DVFS_INDEX(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_DCAM0_1_DVFS_INDEX_IDLE_CFG, 0x300147D0 */
#define BITS_DCAM0_1_DVFS_INDEX_IDLE(_X_) ((_X_) << 0 & (BIT(0)|BIT(1) \
|BIT(2)))

/* _MM_DVFS_AHB_DCAM0_1_AXI_DVFS_INDEX_CFG, 0x300147D4 */
#define BITS_DCAM0_1_AXI_DVFS_INDEX(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_DCAM0_1_AXI_DVFS_INDEX_IDLE_CFG, 0x300147D8 */
#define BITS_DCAM0_1_AXI_DVFS_INDEX_IDLE(_X_) ((_X_) << 0 & (BIT(0)|BIT(1) \
|BIT(2)))

/* _MM_DVFS_AHB_DCAM2_3_DVFS_INDEX_CFG, 0x300147DC */
#define BITS_DCAM2_3_DVFS_INDEX(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_DCAM2_3_DVFS_INDEX_IDLE_CFG, 0x300147E0 */
#define BITS_DCAM2_3_DVFS_INDEX_IDLE(_X_) ((_X_) << 0 & (BIT(0)|BIT(1)|BIT(2)))

/* _MM_DVFS_AHB_DCAM2_3_AXI_DVFS_INDEX_CFG, 0x300147E4 */
#define BITS_DCAM2_3_AXI_DVFS_INDEX(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_DCAM2_3_AXI_DVFS_INDEX_IDLE_CFG, 0x300147E8 */
#define BITS_DCAM2_3_AXI_DVFS_INDEX_IDLE(_X_) ((_X_) << 0 & (BIT(0)|BIT(1) \
|BIT(2)))

/* REG_MM_DVFS_AHB_DCAM_MTX_DVFS_INDEX_CFG, 0x300147EC */
#define BITS_DCAM_MTX_DVFS_INDEX(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_DCAM_MTX_DVFS_INDEX_IDLE_CFG, 0x300147F0 */
#define BITS_DCAM_MTX_DVFS_INDEX_IDLE(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_MM_MTX_DATA_DVFS_INDEX_CFG, 0x300147F4 */
#define BITS_MM_MTX_DATA_DVFS_INDEX(_X_)     ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_MM_MTX_DATA_DVFS_INDEX_IDLE_CFG, 0x300147F8 */
#define BITS_MM_MTX_DATA_DVFS_INDEX_IDLE(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_JPG_DVFS_INDEX_CFG, 0x300147FC */
#define BITS_JPG_DVFS_INDEX(_X_)        ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_JPG_DVFS_INDEX_IDLE_CFG, 0x30014800 */
#define BITS_JPG_DVFS_INDEX_IDLE(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_FREQ_UPD_STATE0, 0x30014924 */
/* No interface so far*/
#define BITS_JPG_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 28 & (BIT(28)| \
BIT(29)|BIT(30)|BIT(31)))
#define BITS_MM_MTX_DATA_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 24 & (BIT(24)| \
BIT(25)|BIT(26)|BIT(27)))
#define BITS_DCAM_MTX_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 20 & (BIT(20)| \
BIT(21)|BIT(22)|BIT(23)))
#define BITS_DCAM2_3_AXI_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 16 & (BIT(16)| \
BIT(17)|BIT(18)|BIT(19)))
#define BITS_DCAM2_3_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 12 & (BIT(12)| \
BIT(13)|BIT(14)|BIT(15)))
#define BITS_DCAM0_1_AXI_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 8  & (BIT(8)| \
BIT(9)|BIT(10)|BIT(11)))
#define BITS_DCAM0_1_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 4  & (BIT(4)| \
BIT(5)|BIT(6)|BIT(7)))
#define BITS_FD_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 0  & (BIT(0)| \
BIT(1)|BIT(2)|BIT(3)))

/* shift and mask definitions additional for READ ONLY bits */
#define SHFT_BITS_JPG_DVFS_FREQ_UPD_STATE          (28)
#define MASK_BITS_JPG_DVFS_FREQ_UPD_STATE          (0xF)
#define SHFT_BITS_MM_MTX_DATA_DVFS_FREQ_UPD_STATE  (24)
#define MASK_BITS_MM_MTX_DATA_DVFS_FREQ_UPD_STATE  (0xF)
#define SHFT_BITS_DCAM_MTX_DVFS_FREQ_UPD_STATE     (20)
#define MASK_BITS_DCAM_MTX_DVFS_FREQ_UPD_STATE     (0xF)
#define SHFT_BITS_DCAM2_3_AXI_DVFS_FREQ_UPD_STATE  (16)
#define MASK_BITS_DCAM2_3_AXI_DVFS_FREQ_UPD_STATE  (0xF)
#define SHFT_BITS_DCAM2_3_DVFS_FREQ_UPD_STATE      (12)
#define MASK_BITS_DCAM2_3_DVFS_FREQ_UPD_STATE      (0xF)
#define SHFT_BITS_DCAM0_1_AXI_DVFS_FREQ_UPD_STATE  (8)
#define MASK_BITS_DCAM0_1_AXI_DVFS_FREQ_UPD_STATE  (0xF)
#define SHFT_BITS_DCAM0_1_DVFS_FREQ_UPD_STATE      (4)
#define MASK_BITS_DCAM0_1_DVFS_FREQ_UPD_STATE      (0xF)
#define SHFT_BITS_FD_DVFS_FREQ_UPD_STATE           (0)
#define MASK_BITS_FD_DVFS_FREQ_UPD_STATE           (0xF)

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_FREQ_UPD_STATE1, 0x30014928 */
/* No interface so far*/
#define BITS_DEPTH_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 20 & (BIT(20)| \
BIT(21)|BIT(22)|BIT(23)))
#define BITS_CPP_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 16 & (BIT(16)| \
BIT(17)|BIT(18)|BIT(19)))
#define BITS_ISP_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 12 & (BIT(12)| \
BIT(13)|BIT(14)|BIT(15)))
#define BITS_VDSP_MTX_DATA_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 8 & (BIT(8)| \
BIT(9)|BIT(10)|BIT(11)))
#define BITS_VDMA_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 4  & (BIT(4)| \
BIT(5)|BIT(6)|BIT(7)))
#define BITS_VDSP_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 0  & (BIT(0)| \
BIT(1)|BIT(2)|BIT(3)))

/* shift and mask definitions additional for READ ONLY bits */
#define SHFT_BITS_DEPTH_DVFS_FREQ_UPD_STATE         (20)
#define MASK_BITS_DEPTH_DVFS_FREQ_UPD_STATE         (0xF)
#define SHFT_BITS_CPP_DVFS_FREQ_UPD_STATE           (16)
#define MASK_BITS_CPP_DVFS_FREQ_UPD_STATE           (0xF)
#define SHFT_BITS_ISP_DVFS_FREQ_UPD_STATE           (12)
#define MASK_BITS_ISP_DVFS_FREQ_UPD_STATE           (0xF)
#define SHFT_BITS_VDSP_MTX_DATA_DVFS_FREQ_UPD_STATE (8)
#define MASK_BITS_VDSP_MTX_DATA_DVFS_FREQ_UPD_STATE (0xF)
#define SHFT_BITS_VDMA_DVFS_FREQ_UPD_STATE          (4)
#define MASK_BITS_VDMA_DVFS_FREQ_UPD_STATE          (0xF)
#define SHFT_BITS_VDSP_DVFS_FREQ_UPD_STATE          (0)
#define MASK_BITS_VDSP_DVFS_FREQ_UPD_STATE          (0xF)



/* bits definitions for register */
/*REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG0, 0x30014944 */
#define BITS_JPG_GFREE_WAIT_DELAY(_X_)  ((_X_) << 20 & (BIT(20)| \
BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)))
#define BITS_MM_MTX_GFREE_WAIT_DELAY(_X_)  ((_X_) << 10 & (BIT(10)| \
BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)))
#define BITS_DCAM_MTX_GFREE_WAIT_DELAY(_X_)  ((_X_) << 0  & (BIT(0)| \
BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)))

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG1, 0x30014948 */
#define BITS_DCAM2_3_AXI_GFREE_WAIT_DELAY(_X_) ((_X_) << 20 & (BIT(20)| \
BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)))
#define BITS_DCAM2_3_GFREE_WAIT_DELAY(_X_) ((_X_) << 10 & (BIT(10)| \
BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)))
#define BITS_DCAM0_1_AXI_GFREE_WAIT_DELAY(_X_) ((_X_) << 0  & (BIT(0)| \
BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)))

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG2, 0x3001494C */
#define BITS_DCAM0_1_GFREE_WAIT_DELAY(_X_) ((_X_) << 20 & (BIT(20)| \
BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)))
#define BITS_FD_GFREE_WAIT_DELAY(_X_) ((_X_) << 10 & (BIT(10)| \
BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)))
#define BITS_DEPTH_GFREE_WAIT_DELAY(_X_) ((_X_) << 0  & (BIT(0)| \
BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)))

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG3, 0x30014950 */
#define BITS_CPP_GFREE_WAIT_DELAY(_X_) ((_X_) << 20 & (BIT(20)| \
BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)))
#define BITS_ISP_GFREE_WAIT_DELAY(_X_) ((_X_) << 10 & (BIT(10)| \
BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)))
#define BITS_VDSP_MTX_DATA_GFREE_WAIT_DELAY(_X_) ((_X_) << 0  & (BIT(0)| \
BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)))

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG4, 0x30014954 */
#define BITS_VDMA_GFREE_WAIT_DELAY(_X_) ((_X_) << 10 & (BIT(10)| \
BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)))
#define BITS_VDSP_GFREE_WAIT_DELAY(_X_) ((_X_) << 0  & (BIT(0)| \
BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)))


/* bits definitions for register*/
/*REG_MM_DVFS_AHB_MM_FREQ_UPD_TYPE_CFG0, 0x300149AC */
#define BIT_JPG_FREQ_UPD_DELAY_EN              (BIT(27))
#define BIT_JPG_FREQ_UPD_HDSK_EN               (BIT(26))
#define BIT_MM_MTX_DATA_FREQ_UPD_DELAY_EN      (BIT(25))
#define BIT_MM_MTX_DATA_FREQ_UPD_HDSK_EN       (BIT(24))
#define BIT_DCAM_MTX_FREQ_UPD_DELAY_EN         (BIT(23))
#define BIT_DCAM_MTX_FREQ_UPD_HDSK_EN          (BIT(22))
#define BIT_DCAM2_3_AXI_FREQ_UPD_DELAY_EN      (BIT(21))
#define BIT_DCAM2_3_AXI_FREQ_UPD_HDSK_EN       (BIT(20))
#define BIT_DCAM2_3_FREQ_UPD_DELAY_EN          (BIT(19))
#define BIT_DCAM2_3_FREQ_UPD_HDSK_EN           (BIT(18))
#define BIT_DCAM0_1_AXI_FREQ_UPD_DELAY_EN      (BIT(17))
#define BIT_DCAM0_1_AXI_FREQ_UPD_HDSK_EN       (BIT(16))
#define BIT_DCAM0_1_FREQ_UPD_DELAY_EN          (BIT(15))
#define BIT_DCAM0_1_FREQ_UPD_HDSK_EN           (BIT(14))
#define BIT_FD_FREQ_UPD_DELAY_EN               (BIT(13))
#define BIT_FD_FREQ_UPD_HDSK_EN                (BIT(12))
#define BIT_DEPTH_FREQ_UPD_DELAY_EN            (BIT(11))
#define BIT_DEPTH_FREQ_UPD_HDSK_EN             (BIT(10))
#define BIT_CPP_FREQ_UPD_DELAY_EN              (BIT(9))
#define BIT_CPP_FREQ_UPD_HDSK_EN               (BIT(8))
#define BIT_ISP_FREQ_UPD_DELAY_EN              (BIT(7))
#define BIT_ISP_FREQ_UPD_HDSK_EN               (BIT(6))
#define BIT_VDSP_MTX_DATA_FREQ_UPD_DELAY_EN    (BIT(5))
#define BIT_VDSP_MTX_DATA_FREQ_UPD_HDSK_EN     (BIT(4))
#define BIT_VDMA_FREQ_UPD_DELAY_EN             (BIT(3))
#define BIT_VDMA_FREQ_UPD_HDSK_EN              (BIT(2))
#define BIT_VDSP_FREQ_UPD_DELAY_EN             (BIT(1))
#define BIT_VDSP_FREQ_UPD_HDSK_EN              (BIT(0))

#define SHIFT_BIT_JPG_FREQ_UPD_DELAY_EN           (27)
#define SHIFT_BIT_JPG_FREQ_UPD_HDSK_EN            (26)
#define SHIFT_BIT_MM_MTX_DATA_FREQ_UPD_DELAY_EN   (25)
#define SHIFT_BIT_MM_MTX_DATA_FREQ_UPD_HDSK_EN    (24)
#define SHIFT_BIT_DCAM_MTX_FREQ_UPD_DELAY_EN      (23)
#define SHIFT_BIT_DCAM_MTX_FREQ_UPD_HDSK_EN       (22)
#define SHIFT_BIT_DCAM2_3_AXI_FREQ_UPD_DELAY_EN   (21)
#define SHIFT_BIT_DCAM2_3_AXI_FREQ_UPD_HDSK_EN    (20)
#define SHIFT_BIT_DCAM2_3_FREQ_UPD_DELAY_EN       (19)
#define SHIFT_BIT_DCAM2_3_FREQ_UPD_HDSK_EN        (18)
#define SHIFT_BIT_DCAM0_1_AXI_FREQ_UPD_DELAY_EN   (17)
#define SHIFT_BIT_DCAM0_1_AXI_FREQ_UPD_HDSK_EN    (16)
#define SHIFT_BIT_DCAM0_1_FREQ_UPD_DELAY_EN       (15)
#define SHIFT_BIT_DCAM0_1_FREQ_UPD_HDSK_EN        (14)
#define SHIFT_BIT_FD_FREQ_UPD_DELAY_EN            (13)
#define SHIFT_BIT_FD_FREQ_UPD_HDSK_EN             (12)
#define SHIFT_BIT_DEPTH_FREQ_UPD_DELAY_EN         (11)
#define SHIFT_BIT_DEPTH_FREQ_UPD_HDSK_EN          (10)
#define SHIFT_BIT_CPP_FREQ_UPD_DELAY_EN           (9)
#define SHIFT_BIT_CPP_FREQ_UPD_HDSK_EN            (8)
#define SHIFT_BIT_ISP_FREQ_UPD_DELAY_EN           (7)
#define SHIFT_BIT_ISP_FREQ_UPD_HDSK_EN            (6)
#define SHIFT_BIT_VDSP_MTX_DATA_FREQ_UPD_DELAY_EN (5)
#define SHIFT_BIT_VDSP_MTX_DATA_FREQ_UPD_HDSK_EN  (4)
#define SHIFT_BIT_VDMA_FREQ_UPD_DELAY_EN          (3)
#define SHIFT_BIT_VDMA_FREQ_UPD_HDSK_EN           (2)
#define SHIFT_BIT_VDSP_FREQ_UPD_DELAY_EN          (1)
#define SHIFT_BIT_VDSP_FREQ_UPD_HDSK_EN           (0)

/*#define REG_MM_DVFS_AHB_MM_DFS_IDLE_DISABLE_CFG0 0x300149B4UL*/
#define BIT_JPG_DFS_IDLE_DISABLE             (BIT(13))
#define BIT_MM_MTX_DFS_IDLE_DISABLE          (BIT(12))
#define BIT_DCAM_MTX_DFS_IDLE_DISABLE        (BIT(11))
#define BIT_DCAM2_3_AXI_DFS_IDLE_DISABLE     (BIT(10))
#define BIT_DCAM2_3_DFS_IDLE_DISABLE         (BIT(9))
#define BIT_DCAM0_1_AXI_DFS_IDLE_DISABLE     (BIT(8))
#define BIT_DCAM0_1_DFS_IDLE_DISABLE         (BIT(7))
#define BIT_FD_DFS_IDLE_DISABLE              (BIT(6))
#define BIT_DEPTH_DFS_IDLE_DISABLE           (BIT(5))
#define BIT_CPP_DFS_IDLE_DISABLE             (BIT(4))
#define BIT_ISP_DFS_IDLE_DISABLE             (BIT(3))
#define BIT_VDSP_MTX_DFS_IDLE_DISABLE        (BIT(2))
#define BIT_VDMA_DFS_IDLE_DISABLE            (BIT(1))
#define BIT_VDSP_DFS_IDLE_DISABLE            (BIT(0))

#define SHIFT_BIT_JPG_DFS_IDLE_DISABLE         (13)
#define SHIFT_BIT_MM_MTX_DFS_IDLE_DISABLE      (12)
#define SHIFT_BIT_DCAM_MTX_DFS_IDLE_DISABLE    (11)
#define SHIFT_BIT_DCAM2_3_AXI_DFS_IDLE_DISABLE (10)
#define SHIFT_BIT_DCAM2_3_DFS_IDLE_DISABLE      (9)
#define SHIFT_BIT_DCAM0_1_AXI_DFS_IDLE_DISABLE  (8)
#define SHIFT_BIT_DCAM0_1_DFS_IDLE_DISABLE      (7)
#define SHIFT_BIT_FD_DFS_IDLE_DISABLE           (6)
#define SHIFT_BIT_DEPTH_DFS_IDLE_DISABLE        (5)
#define SHIFT_BIT_CPP_DFS_IDLE_DISABLE          (4)
#define SHIFT_BIT_ISP_DFS_IDLE_DISABLE          (3)
#define SHIFT_BIT_VDSP_MTX_DFS_IDLE_DISABLE     (2)
#define SHIFT_BIT_VDMA_DFS_IDLE_DISABLE         (1)
#define SHIFT_BIT_VDSP_DFS_IDLE_DISABLE         (0)

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_MM_DVFS_RESERVED_REG_CFG0, 0x30014A58 */
#define BITS_MM_DVFS_RES_REG0(_X_)      ((_X_))

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_MM_DVFS_RESERVED_REG_CFG1, 0x30014A5C */
#define BITS_MM_DVFS_RES_REG1(_X_)      ((_X_))

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_MM_DVFS_RESERVED_REG_CFG2, 0x30014A60 */
#define BITS_MM_DVFS_RES_REG2(_X_)      ((_X_))

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_MM_DVFS_RESERVED_REG_CFG3, 0x30014A64 */
#define BITS_MM_DVFS_RES_REG3(_X_)      ((_X_))

/* vars definitions for controller REGS_MM_DVFS_AHB */
//General Mask
#define MASK_DVFS_FOUR_BITS  (0x0F)
#define MASK_DVFS_THREE_BITS (0x07)
#define MASK_DVFS_TWO_BITS   (0x03)
#define MASK_DVFS_ONE_BIT    (0x01)
#endif /* __SHARKL6PRO_MM_DVFS_H____ */
