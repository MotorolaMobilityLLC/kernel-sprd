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

#ifndef __qogirl6_MM_DVFS_REG_H____
#define __qogirl6_MM_DVFS_REG_H____

/* registers definitions for controller REGS_MM_DVFS_AHB, 0x30014000 */
#define REG_MM_DVFS_AHB_MM_DVFS_HOLD_CTRL          0x0000UL
#define REG_MM_DVFS_AHB_MM_DVFS_WAIT_WINDOW_CFG    0x0004UL
#define REG_MM_DVFS_AHB_MM_DFS_EN_CTRL             0x0008UL
#define REG_MM_DVFS_AHB_MM_DFS_SW_TRIG_CFG         0x000CUL
#define REG_MM_DVFS_AHB_MM_MIN_VOLTAGE_CFG         0x0010UL
#define REG_MM_DVFS_AHB_MM_SW_DVFS_CTRL            0x002CUL
#define REG_MM_DVFS_AHB_MM_FREQ_UPDATE_BYPASS      0x0030UL
#define REG_MM_DVFS_AHB_CGM_MM_DVFS_CLK_GATE_CTRL  0x0034UL
#define REG_MM_DVFS_AHB_MM_DVFS_VOLTAGE_DBG        0x0038UL
#define REG_MM_DVFS_AHB_MM_DVFS_CGM_CFG_DBG        0x0048UL
#define REG_MM_DVFS_AHB_MM_DVFS_STATE_DBG          0x004CUL
#define REG_MM_DVFS_AHB_ISP_INDEX0_MAP   0x0050UL
#define REG_MM_DVFS_AHB_ISP_INDEX1_MAP   0x0054UL
#define REG_MM_DVFS_AHB_ISP_INDEX2_MAP   0x0058UL
#define REG_MM_DVFS_AHB_ISP_INDEX3_MAP   0x005CUL
#define REG_MM_DVFS_AHB_ISP_INDEX4_MAP   0x0060UL
#define REG_MM_DVFS_AHB_ISP_INDEX5_MAP   0x0064UL
#define REG_MM_DVFS_AHB_ISP_INDEX6_MAP   0x0068UL
#define REG_MM_DVFS_AHB_ISP_INDEX7_MAP   0x006CUL
#define REG_MM_DVFS_AHB_JPG_INDEX0_MAP   0x0070UL
#define REG_MM_DVFS_AHB_JPG_INDEX1_MAP   0x0074UL
#define REG_MM_DVFS_AHB_JPG_INDEX2_MAP   0x0078UL
#define REG_MM_DVFS_AHB_JPG_INDEX3_MAP   0x007CUL
#define REG_MM_DVFS_AHB_JPG_INDEX4_MAP   0x0080UL
#define REG_MM_DVFS_AHB_JPG_INDEX5_MAP   0x0084UL
#define REG_MM_DVFS_AHB_JPG_INDEX6_MAP   0x0088UL
#define REG_MM_DVFS_AHB_JPG_INDEX7_MAP   0x008CUL
#define REG_MM_DVFS_AHB_CPP_INDEX0_MAP   0x0090UL
#define REG_MM_DVFS_AHB_CPP_INDEX1_MAP   0x0094UL
#define REG_MM_DVFS_AHB_CPP_INDEX2_MAP   0x0098UL
#define REG_MM_DVFS_AHB_CPP_INDEX3_MAP   0x009CUL
#define REG_MM_DVFS_AHB_CPP_INDEX4_MAP   0x0100UL
#define REG_MM_DVFS_AHB_CPP_INDEX5_MAP   0x0104UL
#define REG_MM_DVFS_AHB_CPP_INDEX6_MAP   0x0108UL
#define REG_MM_DVFS_AHB_CPP_INDEX7_MAP   0x010CUL
#define REG_MM_DVFS_AHB_DCAM_IF_INDEX0_MAP    0x0110UL
#define REG_MM_DVFS_AHB_DCAM_IF_INDEX1_MAP    0x0114UL
#define REG_MM_DVFS_AHB_DCAM_IF_INDEX2_MAP    0x0118UL
#define REG_MM_DVFS_AHB_DCAM_IF_INDEX3_MAP    0x011CUL
#define REG_MM_DVFS_AHB_DCAM_IF_INDEX4_MAP    0x0120UL
#define REG_MM_DVFS_AHB_DCAM_IF_INDEX5_MAP    0x0124UL
#define REG_MM_DVFS_AHB_DCAM_IF_INDEX6_MAP    0x0128UL
#define REG_MM_DVFS_AHB_DCAM_IF_INDEX7_MAP    0x012CUL
#define REG_MM_DVFS_AHB_DCAM_AXI_INDEX0_MAP   0x0130UL
#define REG_MM_DVFS_AHB_DCAM_AXI_INDEX1_MAP   0x0134UL
#define REG_MM_DVFS_AHB_DCAM_AXI_INDEX2_MAP   0x0138UL
#define REG_MM_DVFS_AHB_DCAM_AXI_INDEX3_MAP   0x013CUL
#define REG_MM_DVFS_AHB_DCAM_AXI_INDEX4_MAP   0x0140UL
#define REG_MM_DVFS_AHB_DCAM_AXI_INDEX5_MAP   0x0144UL
#define REG_MM_DVFS_AHB_DCAM_AXI_INDEX6_MAP   0x0148UL
#define REG_MM_DVFS_AHB_DCAM_AXI_INDEX7_MAP   0x014CUL
#define REG_MM_DVFS_AHB_MM_MTX_INDEX0_MAP     0x0150UL
#define REG_MM_DVFS_AHB_MM_MTX_INDEX1_MAP     0x0154UL
#define REG_MM_DVFS_AHB_MM_MTX_INDEX2_MAP     0x0158UL
#define REG_MM_DVFS_AHB_MM_MTX_INDEX3_MAP     0x015CUL
#define REG_MM_DVFS_AHB_MM_MTX_INDEX4_MAP     0x0160UL
#define REG_MM_DVFS_AHB_MM_MTX_INDEX5_MAP     0x0164UL
#define REG_MM_DVFS_AHB_MM_MTX_INDEX6_MAP     0x0168UL
#define REG_MM_DVFS_AHB_MM_MTX_INDEX7_MAP     0x016CUL
#define REG_MM_DVFS_AHB_FD_INDEX0_MAP   0x0170UL
#define REG_MM_DVFS_AHB_FD_INDEX1_MAP   0x0174UL
#define REG_MM_DVFS_AHB_FD_INDEX2_MAP   0x0178UL
#define REG_MM_DVFS_AHB_FD_INDEX3_MAP   0x017CUL
#define REG_MM_DVFS_AHB_FD_INDEX4_MAP   0x0180UL
#define REG_MM_DVFS_AHB_FD_INDEX5_MAP   0x0184UL
#define REG_MM_DVFS_AHB_FD_INDEX6_MAP   0x0188UL
#define REG_MM_DVFS_AHB_FD_INDEX7_MAP   0x018CUL
#define REG_MM_DVFS_AHB_ISP_DVFS_INDEX_CFG           0x01C0UL
#define REG_MM_DVFS_AHB_ISP_DVFS_INDEX_IDLE_CFG      0x01C4UL
#define REG_MM_DVFS_AHB_JPG_DVFS_INDEX_CFG           0x01C8UL
#define REG_MM_DVFS_AHB_JPG_DVFS_INDEX_IDLE_CFG      0x01CCUL
#define REG_MM_DVFS_AHB_CPP_DVFS_INDEX_CFG           0x01D0UL
#define REG_MM_DVFS_AHB_CPP_DVFS_INDEX_IDLE_CFG      0x01D4UL
#define REG_MM_DVFS_AHB_MM_MTX_DVFS_INDEX_CFG        0x01D8UL
#define REG_MM_DVFS_AHB_MM_MTX_DVFS_INDEX_IDLE_CFG   0x01DCUL
#define REG_MM_DVFS_AHB_DCAM_IF_DVFS_INDEX_CFG       0x01E0UL
#define REG_MM_DVFS_AHB_DCAM_IF_DVFS_INDEX_IDLE_CFG  0x01E4UL
#define REG_MM_DVFS_AHB_DCAM_AXI_DVFS_INDEX_CFG      0x01E8UL
#define REG_MM_DVFS_AHB_DCAM_AXI_DVFS_INDEX_IDLE_CFG 0x01ECUL
#define REG_MM_DVFS_AHB_FD_DVFS_INDEX_CFG            0x01F0UL
#define REG_MM_DVFS_AHB_FD_DVFS_INDEX_IDLE_CFG       0x01F4UL
#define REG_MM_DVFS_AHB_FREQ_UPD_STATE           0x0210UL
#define REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG0 0x0214UL
#define REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG1 0x0218UL
#define REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG2 0x021CUL
#define REG_MM_DVFS_AHB_MM_FREQ_UPD_TYPE_CFG     0x0220UL
#define REG_MM_DVFS_AHB_MM_DFS_IDLE_DISABLE_CFG  0x0224UL

#define REG_MM_DVFS_AHB_MM_DVFS_RESERVED_REG_CFG0 0x0230UL
#define REG_MM_DVFS_AHB_MM_DVFS_RESERVED_REG_CFG1 0x0234UL
#define REG_MM_DVFS_AHB_MM_DVFS_RESERVED_REG_CFG2 0x0238UL
#define REG_MM_DVFS_AHB_MM_DVFS_RESERVED_REG_CFG3 0x023CUL

/*REG_MM_DVFS_AHB_MM_DVFS_HOLD_CTRL, 0x30014000 */
#define BIT_MM_DVFS_HOLD                (BIT(0))

/*REG_MM_DVFS_AHB_MM_DVFS_WAIT_WINDOW_CFG, 0x30014004 */
#define BITS_MM_DVFS_UP_WINDOW(_X_)     ((_X_) << 16 & (BIT(16)|BIT(17)| \
	BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)\
	|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)))
#define BITS_MM_DVFS_DOWN_WINDOW(_X_)   ((_X_) << 0  & \
	(BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)\
	|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)))

/*register REG_MM_DVFS_AHB_MM_DFS_EN_CTRL, 0x30014008 */
#define BIT_DCAM_AXI_DFS_EN             (BIT(7))
#define BIT_FD_DFS_EN                   (BIT(6))
#define BIT_DCAM_IF_DFS_EN              (BIT(5))
#define BIT_JPG_DFS_EN                  (BIT(3))
#define BIT_CPP_DFS_EN                  (BIT(2))
#define BIT_ISP_DFS_EN                  (BIT(1))
#define BIT_MM_MTX_DVFS_SW_EN           (BIT(0))

/*REG_MM_DVFS_AHB_MM_DFS_SW_TRIG_CFG, 0x3001400C */
#define BIT_FD_DFS_SW_TRIG                   (BIT(6))
#define BIT_DCAM_DFS_SW_TRIG                 (BIT(5))
#define BIT_JPG_DFS_SW_TRIG                  (BIT(3))
#define BIT_CPP_DFS_SW_TRIG                  (BIT(2))
#define BIT_ISP_DFS_SW_TRIG                  (BIT(1))

/*REG_MM_DVFS_AHB_MM_MIN_VOLTAGE_CFG, 0x30014010 */
#define BITS_MM_SYS_MIN_VOLTAGE(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_MM_SW_DVFS_CTRL, 0x3001402C */
#define BIT_MM_DVFS_ACK                 (BIT(8))
#define BITS_MM_DVFS_VOLTAGE_SW(_X_)    ((_X_) << 4  & (BIT(4)|BIT(5)|BIT(6)))
#define BITS_MM_CURRENT_VOLTAGE_SW(_X_) ((_X_) << 1  & (BIT(1)|BIT(2)|BIT(3)))
#define BIT_MM_DVFS_REQ_SW              (BIT(0))

/*REG_MM_DVFS_AHB_MM_FREQ_UPDATE_BYPASS, 0x30014030 */
#define BIT_REG_FD_FREQ_UPD_EN_BYP       (BIT(6))
#define BIT_REG_DCAM_IF_FREQ_UPD_EN_BYP  (BIT(5))
#define BIT_REG_JPG_FREQ_UPD_EN_BYP      (BIT(4))
#define BIT_REG_CPP_FREQ_UPD_EN_BYP      (BIT(3))
#define BIT_REG_ISP_FREQ_UPD_EN_BYP      (BIT(2))
#define BIT_REG_MM_MTX_FREQ_UPD_EN_BYP   (BIT(1))
#define BIT_REG_DCAM_AXI_FREQ_UPD_EN_BYP (BIT(0))

/*REG_MM_DVFS_AHB_CGM_MM_DVFS_CLK_GATE_CTRL, 0x30014034 */
#define BIT_CGM_MM_DVFS_FORCE_EN        (BIT(1))
#define BIT_CGM_MM_DVFS_AUTO_GATE_SEL   (BIT(0))

/*REG_MM_DVFS_AHB_MM_DVFS_VOLTAGE_DBG, 0x30014038 */
#define BITS_MM_CURRENT_VOLTAGE(_X_)  ((_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)))
#define BITS_FD_VOLTAGE(_X_)          ((_X_) << 21 & (BIT(21)|BIT(22)|BIT(23)))
#define BITS_DCAM_IF_VOLTAGE(_X_)     ((_X_) << 18 & (BIT(18)|BIT(19)|BIT(20)))
#define BITS_DCAM_AXI_VOLTAGE(_X_)    ((_X_) << 15 & (BIT(15)|BIT(16)|BIT(17)))
#define BITS_JPG_VOLTAGE(_X_)         ((_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)))
#define BITS_CPP_VOLTAGE(_X_)         ((_X_) << 9  & (BIT(9)|BIT(10)|BIT(11)))
#define BITS_ISP_VOLTAGE(_X_)         ((_X_) << 6  & (BIT(6)|BIT(7)|BIT(8)))
#define BITS_MM_MTX_VOLTAGE(_X_)      ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_MM_INTERNAL_VOTE_VOLTAGE(_X_) ((_X_) << 0  & \
(BIT(0)|BIT(1)|BIT(2)))

/* shift and mask definitions additional for READ ONLY bits */
#define SHFT_BITS_MM_CURRENT_VOLTAGE    (24)
#define MASK_BITS_MM_CURRENT_VOLTAGE    (0x7)
#define SHFT_BITS_FD_VOLTAGE            (21)
#define MASK_BITS_FD_VOLTAGE            (0x7)
#define SHFT_BITS_DCAM_IF_VOLTAGE       (18)
#define MASK_BITS_DCAM_IF_VOLTAGE       (0x7)
#define SHFT_BITS_DCAM_AXI_VOLTAGE      (15)
#define MASK_BITS_DCAM_AXI_VOLTAGE      (0x7)
#define SHFT_BITS_JPG_VOLTAGE           (12)
#define MASK_BITS_JPG_VOLTAGE           (0x7)
#define SHFT_BITS_CPP_VOLTAGE           (9)
#define MASK_BITS_CPP_VOLTAGE           (0x7)
#define SHFT_BITS_ISP_VOLTAGE           (6)
#define MASK_BITS_ISP_VOLTAGE           (0x7)
#define SHFT_BITS_MM_MTX_VOLTAGE        (3)
#define MASK_BITS_MM_MTX_VOLTAGE        (0x7)
#define SHFT_BITS_MM_INTERNAL_VOTE_VOLTAGE (0)
#define MASK_BITS_MM_INTERNAL_VOTE_VOLTAGE (0x7)

/* REG_MM_DVFS_AHB_MM_DVFS_CGM_CFG_DBG, 0x30014048 */
#define BITS_CGM_FD_SEL_DVFS(_X_)       ((_X_) << 23 & (BIT(23)|BIT(24)))
#define BITS_CGM_DCAM_IF_FDIV_DENOM_DVFS(_X_) ((_X_) << 19 & \
	(BIT(19)|BIT(20)|BIT(21)|BIT(22)))
#define BITS_CGM_DCAM_IF_FDIV_NUM_DVFS(_X_) ((_X_) << 15 & \
	(BIT(15)|BIT(16)|BIT(17)|BIT(18)))
#define BITS_CGM_DCAM_IF_SEL_DVFS(_X_)  ((_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)))
#define BITS_CGM_DCAM_AXI_SEL_DVFS(_X_) ((_X_) << 10  & (BIT(10)|BIT(11)))
#define BITS_CGM_JPG_SEL_DVFS(_X_)      ((_X_) << 8  & (BIT(8)|BIT(9)))
#define BITS_CGM_CPP_SEL_DVFS(_X_)      ((_X_) << 6  & (BIT(6)|BIT(7)))
#define BITS_CGM_ISP_SEL_DVFS(_X_)      ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_MM_MTX_SEL_DVFS(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* shift and mask definitions additional for READ ONLY bits */
#define SHFT_BITS_CGM_FD_SEL_DVFS       (23)
#define MASK_BITS_CGM_FD_SEL_DVFS       (0x3)
#define SHFT_BITS_CGM_DCAM_IF_FDIV_DENOM_DVFS (19)
#define MASK_BITS_CGM_DCAM_IF_FDIV_DENOM_DVFS (0xF)
#define SHFT_BITS_CGM_DCAM_IF_FDIV_NUM_DVFS (15)
#define MASK_BITS_CGM_DCAM_IF_FDIV_NUM_DVFS (0xF)
#define SHFT_BITS_CGM_DCAM_IF_SEL_DVFS  (12)
#define MASK_BITS_CGM_DCAM_IF_SEL_DVFS  (0x7)
#define SHFT_BITS_CGM_DCAM_AXI_SEL_DVFS (10)
#define MASK_BITS_CGM_DCAM_AXI_SEL_DVFS (0x3)
#define SHFT_BITS_CGM_JPG_SEL_DVFS      (8)
#define MASK_BITS_CGM_JPG_SEL_DVFS      (0x3)
#define SHFT_BITS_CGM_CPP_SEL_DVFS      (6)
#define MASK_BITS_CGM_CPP_SEL_DVFS      (0x3)
#define SHFT_BITS_CGM_ISP_SEL_DVFS      (3)
#define MASK_BITS_CGM_ISP_SEL_DVFS      (0x7)
#define SHFT_BITS_CGM_MM_MTX_SEL_DVFS   (0)
#define MASK_BITS_CGM_MM_MTX_SEL_DVFS   (0x7)

/* REG_MM_DVFS_AHB_MM_DVFS_STATE_DBG, 0x3001404C */
#define BIT_MM_DVFS_BUSY                (BIT(19))
#define BITS_MM_DVFS_WINDOW_CNT(_X_)    ((_X_) << 3  & (BIT(3)|BIT(4)| \
	BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13) \
	|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)))
#define BITS_MM_DVFS_STATE(_X_)         ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))
/* shift and mask definitions additional for READ ONLY bits */
#define SHFT_BITS_MM_DVFS_WINDOW_CNT    (3)
#define MASK_BITS_MM_DVFS_WINDOW_CNT    (0xFFFF)
#define SHFT_BITS_MM_DVFS_STATE         (0)
#define MASK_BITS_MM_DVFS_STATE         (0x7)

/* bits definitions for register REG_MM_DVFS_AHB_ISP_INDEX0_MAP, 0x30014050 */
#define BITS_ISP_VOL_INDEX0(_X_)        ((_X_) << 6  & (BIT(6)|BIT(7)|BIT(8)))
#define BITS_ISP_VOTE_MTX_INDEX0(_X_)   ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_ISP_SEL_INDEX0(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_ISP_INDEX1_MAP, 0x30014054 */
#define BITS_ISP_VOL_INDEX1(_X_)        ((_X_) << 6  & (BIT(6)|BIT(7)|BIT(8)))
#define BITS_ISP_VOTE_MTX_INDEX1(_X_)   ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_ISP_SEL_INDEX1(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_ISP_INDEX2_MAP, 0x30014058 */
#define BITS_ISP_VOL_INDEX2(_X_)        ((_X_) << 6  & (BIT(6)|BIT(7)|BIT(8)))
#define BITS_ISP_VOTE_MTX_INDEX2(_X_)   ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_ISP_SEL_INDEX2(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_ISP_INDEX3_MAP, 0x3001405C */
#define BITS_ISP_VOL_INDEX3(_X_)        ((_X_) << 6  & (BIT(6)|BIT(7)|BIT(8)))
#define BITS_ISP_VOTE_MTX_INDEX3(_X_)   ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_ISP_SEL_INDEX3(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_ISP_INDEX4_MAP, 0x30014060 */
#define BITS_ISP_VOL_INDEX4(_X_)        ((_X_) << 6  & (BIT(6)|BIT(7)|BIT(8)))
#define BITS_ISP_VOTE_MTX_INDEX4(_X_)   ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_ISP_SEL_INDEX4(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_ISP_INDEX5_MAP, 0x30014064 */
#define BITS_ISP_VOL_INDEX5(_X_)        ((_X_) << 6  & (BIT(6)|BIT(7)|BIT(8)))
#define BITS_ISP_VOTE_MTX_INDEX5(_X_)   ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_ISP_SEL_INDEX5(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_ISP_INDEX6_MAP, 0x30014068 */
#define BITS_ISP_VOL_INDEX6(_X_)        ((_X_) << 6  & (BIT(6)|BIT(7)|BIT(8)))
#define BITS_ISP_VOTE_MTX_INDEX6(_X_)   ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_ISP_SEL_INDEX6(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_ISP_INDEX7_MAP, 0x3001406C */
#define BITS_ISP_VOL_INDEX7(_X_)        ((_X_) << 6  & (BIT(6)|BIT(7)|BIT(8)))
#define BITS_ISP_VOTE_MTX_INDEX7(_X_)   ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_ISP_SEL_INDEX7(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_JPG_INDEX0_MAP, 0x30014070 */
#define BITS_JPG_VOL_INDEX0(_X_)        ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_JPG_VOTE_MTX_INDEX0(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_JPG_SEL_INDEX0(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_JPG_INDEX1_MAP, 0x30014074 */
#define BITS_JPG_VOL_INDEX1(_X_)        ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_JPG_VOTE_MTX_INDEX1(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_JPG_SEL_INDEX1(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_JPG_INDEX2_MAP, 0x30014078 */
#define BITS_JPG_VOL_INDEX2(_X_)        ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_JPG_VOTE_MTX_INDEX2(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_JPG_SEL_INDEX2(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_JPG_INDEX3_MAP, 0x3001407C */
#define BITS_JPG_VOL_INDEX3(_X_)        ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_JPG_VOTE_MTX_INDEX3(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_JPG_SEL_INDEX3(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_JPG_INDEX4_MAP, 0x30014080 */
#define BITS_JPG_VOL_INDEX4(_X_)        ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_JPG_VOTE_MTX_INDEX4(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_JPG_SEL_INDEX4(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_JPG_INDEX5_MAP, 0x30014084 */
#define BITS_JPG_VOL_INDEX5(_X_)        ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_JPG_VOTE_MTX_INDEX5(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_JPG_SEL_INDEX5(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_JPG_INDEX6_MAP, 0x30014088 */
#define BITS_JPG_VOL_INDEX6(_X_)        ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_JPG_VOTE_MTX_INDEX6(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_JPG_SEL_INDEX6(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_JPG_INDEX7_MAP, 0x3001408C */
#define BITS_JPG_VOL_INDEX7(_X_)        ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_JPG_VOTE_MTX_INDEX7(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_JPG_SEL_INDEX7(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_CPP_INDEX0_MAP, 0x30014090 */
#define BITS_CPP_VOL_INDEX0(_X_)        ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_CPP_VOTE_MTX_INDEX0(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_CPP_SEL_INDEX0(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_CPP_INDEX1_MAP, 0x30014094 */
#define BITS_CPP_VOL_INDEX1(_X_)        ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_CPP_VOTE_MTX_INDEX1(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_CPP_SEL_INDEX1(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_CPP_INDEX2_MAP, 0x30014098 */
#define BITS_CPP_VOL_INDEX2(_X_)        ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_CPP_VOTE_MTX_INDEX2(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_CPP_SEL_INDEX2(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_CPP_INDEX3_MAP, 0x3001409C */
#define BITS_CPP_VOL_INDEX3(_X_)        ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_CPP_VOTE_MTX_INDEX3(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_CPP_SEL_INDEX3(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_CPP_INDEX4_MAP, 0x30014100 */
#define BITS_CPP_VOL_INDEX4(_X_)        ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_CPP_VOTE_MTX_INDEX4(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_CPP_SEL_INDEX4(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_CPP_INDEX5_MAP, 0x30014104 */
#define BITS_CPP_VOL_INDEX5(_X_)        ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_CPP_VOTE_MTX_INDEX5(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_CPP_SEL_INDEX5(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_CPP_INDEX6_MAP, 0x30014108 */
#define BITS_CPP_VOL_INDEX6(_X_)        ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_CPP_VOTE_MTX_INDEX6(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_CPP_SEL_INDEX6(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_CPP_INDEX7_MAP, 0x3001410C */
#define BITS_CPP_VOL_INDEX7(_X_)        ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_CPP_VOTE_MTX_INDEX7(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_CPP_SEL_INDEX7(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)))

/*REG_MM_DVFS_AHB_DCAM_IF_INDEX0_MAP, 0x30014110 */


#define BITS_DCAM_IF_VOTE_AXI_INDEX0(_X_)       ((_X_) << 14 & \
	(BIT(14)|BIT(15)|BIT(16)))
#define BITS_DCAM_IF_VOL_INDEX0(_X_)            ((_X_) << 11 & \
	(BIT(11)|BIT(12)|BIT(13)))
#define BITS_CGM_DCAM_IF_FDIV_DENOM_INDEX0(_X_) ((_X_) << 7 & \
	(BIT(7)|BIT(8)|BIT(9)|BIT(10)))
#define BITS_CGM_DCAM_IF_FDIV_NUM_INDEX0(_X_)   ((_X_) << 3 & \
	(BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM_IF_SEL_INDEX0(_X_)        ((_X_) << 0 & \
	(BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_DCAM_IF_INDEX1_MAP, 0x30014114 */
#define BITS_DCAM_IF_VOTE_AXI_INDEX1(_X_)       ((_X_) << 14 & \
	(BIT(14)|BIT(15)|BIT(16)))
#define BITS_DCAM_IF_VOL_INDEX1(_X_)            ((_X_) << 11 & \
	(BIT(11)|BIT(12)|BIT(13)))
#define BITS_CGM_DCAM_IF_FDIV_DENOM_INDEX1(_X_) ((_X_) << 7 & \
	(BIT(7)|BIT(8)|BIT(9)|BIT(10)))
#define BITS_CGM_DCAM_IF_FDIV_NUM_INDEX1(_X_)   ((_X_) << 3 & \
	(BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM_IF_SEL_INDEX1(_X_)        ((_X_) << 0 & \
	(BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_DCAM_IF_INDEX2_MAP, 0x30014118 */
#define BITS_DCAM_IF_VOTE_AXI_INDEX2(_X_)       ((_X_) << 14 & \
	(BIT(14)|BIT(15)|BIT(16)))
#define BITS_DCAM_IF_VOL_INDEX2(_X_)            ((_X_) << 11 & \
	(BIT(11)|BIT(12)|BIT(13)))
#define BITS_CGM_DCAM_IF_FDIV_DENOM_INDEX2(_X_) ((_X_) << 7 & \
	(BIT(7)|BIT(8)|BIT(9)|BIT(10)))
#define BITS_CGM_DCAM_IF_FDIV_NUM_INDEX2(_X_)   ((_X_) << 3 & \
	(BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM_IF_SEL_INDEX2(_X_)        ((_X_) << 0 & \
	(BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_DCAM_IF_INDEX3_MAP, 0x3001411C */
#define BITS_DCAM_IF_VOTE_AXI_INDEX3(_X_)       ((_X_) << 14 & \
	(BIT(14)|BIT(15)|BIT(16)))
#define BITS_DCAM_IF_VOL_INDEX3(_X_)            ((_X_) << 11 & \
	(BIT(11)|BIT(12)|BIT(13)))
#define BITS_CGM_DCAM_IF_FDIV_DENOM_INDEX3(_X_) ((_X_) << 7 & \
	(BIT(7)|BIT(8)|BIT(9)|BIT(10)))
#define BITS_CGM_DCAM_IF_FDIV_NUM_INDEX3(_X_)   ((_X_) << 3 & \
	(BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM_IF_SEL_INDEX3(_X_)        ((_X_) << 0 & \
	(BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_DCAM_IF_INDEX4_MAP, 0x30014120 */
#define BITS_DCAM_IF_VOTE_AXI_INDEX4(_X_)       ((_X_) << 14 & \
	(BIT(14)|BIT(15)|BIT(16)))
#define BITS_DCAM_IF_VOL_INDEX4(_X_)            ((_X_) << 11 & \
	(BIT(11)|BIT(12)|BIT(13)))
#define BITS_CGM_DCAM_IF_FDIV_DENOM_INDEX4(_X_) ((_X_) << 7 & \
	(BIT(7)|BIT(8)|BIT(9)|BIT(10)))
#define BITS_CGM_DCAM_IF_FDIV_NUM_INDEX4(_X_)   ((_X_) << 3 & \
	(BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM_IF_SEL_INDEX4(_X_)        ((_X_) << 0 & \
    (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_DCAM_IF_INDEX5_MAP, 0x30014124 */
#define BITS_DCAM_IF_VOTE_AXI_INDEX5(_X_)       ((_X_) << 14 & \
	(BIT(14)|BIT(15)|BIT(16)))
#define BITS_DCAM_IF_VOL_INDEX5(_X_)            ((_X_) << 11 &\
	(BIT(11)|BIT(12)|BIT(13)))
#define BITS_CGM_DCAM_IF_FDIV_DENOM_INDEX5(_X_) ((_X_) << 7 & \
	(BIT(7)|BIT(8)|BIT(9)|BIT(10)))
#define BITS_CGM_DCAM_IF_FDIV_NUM_INDEX5(_X_)   ((_X_) << 3 & \
	(BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM_IF_SEL_INDEX5(_X_)        ((_X_) << 0 & \
	(BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_DCAM_IF_INDEX6_MAP, 0x30014128 */
#define BITS_DCAM_IF_VOTE_AXI_INDEX6(_X_)       ((_X_) << 14 & \
	(BIT(14)|BIT(15)|BIT(16)))
#define BITS_DCAM_IF_VOL_INDEX6(_X_)            ((_X_) << 11 & \
	(BIT(11)|BIT(12)|BIT(13)))
#define BITS_CGM_DCAM_IF_FDIV_DENOM_INDEX6(_X_) ((_X_) << 7 & \
	(BIT(7)|BIT(8)|BIT(9)|BIT(10)))
#define BITS_CGM_DCAM_IF_FDIV_NUM_INDEX6(_X_)   ((_X_) << 3 & \
	(BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM_IF_SEL_INDEX6(_X_)        ((_X_) << 0 & \
    (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_DCAM_IF_INDEX7_MAP, 0x3001412C */
#define BITS_DCAM_IF_VOTE_AXI_INDEX7(_X_)       ((_X_) << 14 & \
    (BIT(14)|BIT(15)|BIT(16)))
#define BITS_DCAM_IF_VOL_INDEX7(_X_)            ((_X_) << 11 & \
    (BIT(11)|BIT(12)|BIT(13)))
#define BITS_CGM_DCAM_IF_FDIV_DENOM_INDEX7(_X_) ((_X_) << 7  & \
    (BIT(7)|BIT(8)|BIT(9)|BIT(10)))
#define BITS_CGM_DCAM_IF_FDIV_NUM_INDEX7(_X_)   ((_X_) << 3  & \
    (BIT(3)|BIT(4)|BIT(5)|BIT(6)))
#define BITS_CGM_DCAM_IF_SEL_INDEX7(_X_)        ((_X_) << 0  & \
    (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_DCAM_AXI_INDEX0_MAP, 0x30014130 */
#define BITS_DCAM_AXI_VOL_INDEX0(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_DCAM_AXI_SEL_INDEX0(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)))

/*  REG_MM_DVFS_AHB_DCAM_AXI_INDEX1_MAP, 0x30014134 */
#define BITS_DCAM_AXI_VOL_INDEX1(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_DCAM_AXI_SEL_INDEX1(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)))

/* REG_MM_DVFS_AHB_DCAM_AXI_INDEX2_MAP, 0x30014138 */
#define BITS_DCAM_AXI_VOL_INDEX2(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_DCAM_AXI_SEL_INDEX2(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)))

/* REG_MM_DVFS_AHB_DCAM_AXI_INDEX3_MAP, 0x3001413C */
#define BITS_DCAM_AXI_VOL_INDEX3(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_DCAM_AXI_SEL_INDEX3(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)))

/*  REG_MM_DVFS_AHB_DCAM_AXI_INDEX4_MAP, 0x30014140 */
#define BITS_DCAM_AXI_VOL_INDEX4(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_DCAM_AXI_SEL_INDEX4(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)))

/* REG_MM_DVFS_AHB_DCAM_AXI_INDEX5_MAP, 0x30014144 */
#define BITS_DCAM_AXI_VOL_INDEX5(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_DCAM_AXI_SEL_INDEX5(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)))

/* REG_MM_DVFS_AHB_DCAM_AXI_INDEX6_MAP, 0x30014148 */
#define BITS_DCAM_AXI_VOL_INDEX6(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_DCAM_AXI_SEL_INDEX6(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)))

/*REG_MM_DVFS_AHB_DCAM_AXI_INDEX7_MAP, 0x3001414C */
#define BITS_DCAM_AXI_VOL_INDEX7(_X_)   ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_DCAM_AXI_SEL_INDEX7(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)))

/* REG_MM_DVFS_AHB_MM_MTX_INDEX0_MAP, 0x30014150 */
#define BITS_MM_MTX_VOL_INDEX0(_X_)     ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_MM_MTX_SEL_INDEX0(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_MM_MTX_INDEX1_MAP, 0x30014154 */
#define BITS_MM_MTX_VOL_INDEX1(_X_)     ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_MM_MTX_SEL_INDEX1(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_MM_MTX_INDEX2_MAP, 0x30014158 */
#define BITS_MM_MTX_VOL_INDEX2(_X_)     ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_MM_MTX_SEL_INDEX2(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_MM_MTX_INDEX3_MAP, 0x3001415C */
#define BITS_MM_MTX_VOL_INDEX3(_X_)     ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_MM_MTX_SEL_INDEX3(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_MM_MTX_INDEX4_MAP, 0x30014160 */
#define BITS_MM_MTX_VOL_INDEX4(_X_)     ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_MM_MTX_SEL_INDEX4(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_MM_MTX_INDEX5_MAP, 0x30014164 */
#define BITS_MM_MTX_VOL_INDEX5(_X_)     ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_MM_MTX_SEL_INDEX5(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_MM_MTX_INDEX6_MAP, 0x30014168 */
#define BITS_MM_MTX_VOL_INDEX6(_X_)     ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_MM_MTX_SEL_INDEX6(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_MM_MTX_INDEX7_MAP, 0x3001416C */
#define BITS_MM_MTX_VOL_INDEX7(_X_)     ((_X_) << 3  & (BIT(3)|BIT(4)|BIT(5)))
#define BITS_CGM_MM_MTX_SEL_INDEX7(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register REG_MM_DVFS_AHB_FD_INDEX0_MAP, 0x30014170 */
#define BITS_FD_VOL_INDEX0(_X_)         ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_FD_VOTE_MTX_INDEX0(_X_)    ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_FD_SEL_INDEX0(_X_)     ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_FD_INDEX1_MAP, 0x30014174 */
#define BITS_FD_VOL_INDEX1(_X_)         ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_FD_VOTE_MTX_INDEX1(_X_)    ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_FD_SEL_INDEX1(_X_)     ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_FD_INDEX2_MAP, 0x30014178 */
#define BITS_FD_VOL_INDEX2(_X_)         ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_FD_VOTE_MTX_INDEX2(_X_)    ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_FD_SEL_INDEX2(_X_)     ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_FD_INDEX3_MAP, 0x3001417C */
#define BITS_FD_VOL_INDEX3(_X_)         ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_FD_VOTE_MTX_INDEX3(_X_)    ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_FD_SEL_INDEX3(_X_)     ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_FD_INDEX4_MAP, 0x30014180 */
#define BITS_FD_VOL_INDEX4(_X_)         ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_FD_VOTE_MTX_INDEX4(_X_)    ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_FD_SEL_INDEX4(_X_)     ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_FD_INDEX5_MAP, 0x30014184 */
#define BITS_FD_VOL_INDEX5(_X_)         ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_FD_VOTE_MTX_INDEX5(_X_)    ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_FD_SEL_INDEX5(_X_)     ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_FD_INDEX6_MAP, 0x30014188 */
#define BITS_FD_VOL_INDEX6(_X_)         ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_FD_VOTE_MTX_INDEX6(_X_)    ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_FD_SEL_INDEX6(_X_)     ((_X_) << 0  & (BIT(0)|BIT(1)))

/* bits definitions for register REG_MM_DVFS_AHB_FD_INDEX7_MAP, 0x3001418C */
#define BITS_FD_VOL_INDEX7(_X_)         ((_X_) << 5  & (BIT(5)|BIT(6)|BIT(7)))
#define BITS_FD_VOTE_MTX_INDEX7(_X_)    ((_X_) << 2  & (BIT(2)|BIT(3)|BIT(4)))
#define BITS_CGM_FD_SEL_INDEX7(_X_)     ((_X_) << 0  & (BIT(0)|BIT(1)))

/*  REG_MM_DVFS_AHB_ISP_DVFS_INDEX_CFG, 0x300141C0 */
#define BITS_ISP_DVFS_INDEX(_X_)        ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_ISP_DVFS_INDEX_IDLE_CFG, 0x300141C4 */
#define BITS_ISP_DVFS_INDEX_IDLE(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_JPG_DVFS_INDEX_CFG, 0x300141C8 */
#define BITS_JPG_DVFS_INDEX(_X_)        ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_JPG_DVFS_INDEX_IDLE_CFG, 0x300141CC */
#define BITS_JPG_DVFS_INDEX_IDLE(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_CPP_DVFS_INDEX_CFG, 0x300141D0 */
#define BITS_CPP_DVFS_INDEX(_X_)        ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_CPP_DVFS_INDEX_IDLE_CFG, 0x300141D4 */
#define BITS_CPP_DVFS_INDEX_IDLE(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*REG_MM_DVFS_AHB_MM_MTX_DVFS_INDEX_CFG, 0x300141D8 */
#define BITS_MM_MTX_DVFS_INDEX(_X_)     ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_MM_MTX_DVFS_INDEX_IDLE_CFG, 0x300141DC */
#define BITS_MM_MTX_DVFS_INDEX_IDLE(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_DCAM_IF_DVFS_INDEX_CFG, 0x300141E0 */
#define BITS_DCAM_IF_DVFS_INDEX(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_DCAM_IF_DVFS_INDEX_IDLE_CFG, 0x300141E4 */
#define BITS_DCAM_IF_DVFS_INDEX_IDLE(_X_) ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* _MM_DVFS_AHB_DCAM_AXI_DVFS_INDEX_CFG, 0x300141E8 */
#define BITS_DCAM_AXI_DVFS_INDEX(_X_)   ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/*  REG_MM_DVFS_AHB_DCAM_AXI_DVFS_INDEX_IDLE_CFG, 0x300141EC */
#define BITS_DCAM_AXI_DVFS_INDEX_IDLE(_X_) ((_X_) << 0  & (BIT(0) | \
    BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_FD_DVFS_INDEX_CFG, 0x300141F0 */
#define BITS_FD_DVFS_INDEX(_X_)         ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* REG_MM_DVFS_AHB_FD_DVFS_INDEX_IDLE_CFG, 0x300141F4 */
#define BITS_FD_DVFS_INDEX_IDLE(_X_)    ((_X_) << 0  & (BIT(0)|BIT(1)|BIT(2)))

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_FREQ_UPD_STATE, 0x30014210 */
#define BITS_FD_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 24 & (BIT(24)| \
    BIT(25)|BIT(26)|BIT(27)))
#define BITS_JPG_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 20 & (BIT(20)| \
    BIT(21)|BIT(22)|BIT(23)))
#define BITS_CPP_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 16 & (BIT(16)| \
    BIT(17)|BIT(18)|BIT(19)))
#define BITS_ISP_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 12 & (BIT(12)| \
    BIT(13)|BIT(14)|BIT(15)))
#define BITS_MM_MTX_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 8  & (BIT(8)| \
    BIT(9)|BIT(10)|BIT(11)))
#define BITS_DCAM_AXI_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 4  & (BIT(4)| \
    BIT(5)|BIT(6)|BIT(7)))
#define BITS_DCAM_IF_DVFS_FREQ_UPD_STATE(_X_) ((_X_) << 0  & (BIT(0)| \
    BIT(1)|BIT(2)|BIT(3)))

/* shift and mask definitions additional for READ ONLY bits */
#define SHFT_BITS_FD_DVFS_FREQ_UPD_STATE (24)
#define MASK_BITS_FD_DVFS_FREQ_UPD_STATE (0xF)
#define SHFT_BITS_JPG_DVFS_FREQ_UPD_STATE (20)
#define MASK_BITS_JPG_DVFS_FREQ_UPD_STATE (0xF)
#define SHFT_BITS_CPP_DVFS_FREQ_UPD_STATE (16)
#define MASK_BITS_CPP_DVFS_FREQ_UPD_STATE (0xF)
#define SHFT_BITS_ISP_DVFS_FREQ_UPD_STATE (12)
#define MASK_BITS_ISP_DVFS_FREQ_UPD_STATE (0xF)
#define SHFT_BITS_MM_MTX_DVFS_FREQ_UPD_STATE (8)
#define MASK_BITS_MM_MTX_DVFS_FREQ_UPD_STATE (0xF)
#define SHFT_BITS_DCAM_AXI_DVFS_FREQ_UPD_STATE (4)
#define MASK_BITS_DCAM_AXI_DVFS_FREQ_UPD_STATE (0xF)
#define SHFT_BITS_DCAM_IF_DVFS_FREQ_UPD_STATE (0)
#define MASK_BITS_DCAM_IF_DVFS_FREQ_UPD_STATE (0xF)

/* bits definitions for register */
/*REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG0, 0x30014214 */
#define BITS_ISP_GFREE_WAIT_DELAY(_X_)  ((_X_) << 20 & (BIT(20)| \
BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)))
#define BITS_CPP_GFREE_WAIT_DELAY(_X_)  ((_X_) << 10 & (BIT(10)| \
BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)))
#define BITS_JPG_GFREE_WAIT_DELAY(_X_)  ((_X_) << 0  & (BIT(0)| \
BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)))

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG1, 0x30014218 */
#define BITS_MM_MTX_GFREE_WAIT_DELAY(_X_) ((_X_) << 20 & (BIT(20)| \
BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)))
#define BITS_DCAM_AXI_GFREE_WAIT_DELAY(_X_) ((_X_) << 10 & (BIT(10)| \
BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)))
#define BITS_DCAM_IF_GFREE_WAIT_DELAY(_X_) ((_X_) << 0  & (BIT(0)| \
BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)))

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_MM_GFREE_WAIT_DELAY_CFG2, 0x3001421C */
#define BITS_FD_GFREE_WAIT_DELAY(_X_)   ((_X_) << 0	& (BIT(0) \
|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)))

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_MM_FREQ_UPD_TYPE_CFG, 0x30014220 */
#define BIT_FD_FREQ_UPD_DELAY_EN        (BIT(13))
#define BIT_FD_FREQ_UPD_HDSK_EN         (BIT(12))
#define BIT_ISP_FREQ_UPD_DELAY_EN       (BIT(11))
#define BIT_ISP_FREQ_UPD_HDSK_EN        (BIT(10))
#define BIT_CPP_FREQ_UPD_DELAY_EN       (BIT(9))
#define BIT_CPP_FREQ_UPD_HDSK_EN        (BIT(8))
#define BIT_JPG_FREQ_UPD_DELAY_EN       (BIT(7))
#define BIT_JPG_FREQ_UPD_HDSK_EN        (BIT(6))
#define BIT_MM_MTX_FREQ_UPD_DELAY_EN    (BIT(5))
#define BIT_MM_MTX_FREQ_UPD_HDSK_EN     (BIT(4))
#define BIT_DCAM_AXI_FREQ_UPD_DELAY_EN  (BIT(3))
#define BIT_DCAM_AXI_FREQ_UPD_HDSK_EN   (BIT(2))
#define BIT_DCAM_IF_FREQ_UPD_DELAY_EN   (BIT(1))
#define BIT_DCAM_IF_FREQ_UPD_HDSK_EN    (BIT(0))

#define SHIFT_BIT_FD_FREQ_UPD_DELAY_EN        (13)
#define SHIFT_BIT_FD_FREQ_UPD_HDSK_EN         (12)
#define SHIFT_BIT_ISP_FREQ_UPD_DELAY_EN       (11)
#define SHIFT_BIT_ISP_FREQ_UPD_HDSK_EN        (10)
#define SHIFT_BIT_CPP_FREQ_UPD_DELAY_EN       (9)
#define SHIFT_BIT_CPP_FREQ_UPD_HDSK_EN        (8)
#define SHIFT_BIT_JPG_FREQ_UPD_DELAY_EN       (7)
#define SHIFT_BIT_JPG_FREQ_UPD_HDSK_EN        (6)
#define SHIFT_BIT_MM_MTX_FREQ_UPD_DELAY_EN    (5)
#define SHIFT_BIT_MM_MTX_FREQ_UPD_HDSK_EN     (4)
#define SHIFT_BIT_DCAM_AXI_FREQ_UPD_DELAY_EN  (3)
#define SHIFT_BIT_DCAM_AXI_FREQ_UPD_HDSK_EN   (2)
#define SHIFT_BIT_DCAM_IF_FREQ_UPD_DELAY_EN   (1)
#define SHIFT_BIT_DCAM_IF_FREQ_UPD_HDSK_EN    (0)

/*#define REG_MM_DVFS_AHB_MM_DFS_IDLE_DISABLE_CFG 0x30014224 */
#define BIT_MTX_DFS_IDLE_DISABLE (BIT(6))
#define BIT_FD_DFS_IDLE_DISABLE (BIT(5))
#define BIT_ISP_DFS_IDLE_DISABLE (BIT(4))
#define BIT_CPP_DFS_IDLE_DISABLE (BIT(3))
#define BIT_JPG_DFS_IDLE_DISABLE (BIT(2))
#define BIT_DCAM_AXI_DFS_IDLE_DISABLE (BIT(1))
#define BIT_DCAM_IF_DFS_IDLE_DISABLE (BIT(0))

#define SHIFT_BIT_MTX_DFS_IDLE_DISABLE (6)
#define SHIFT_BIT_FD_DFS_IDLE_DISABLE (5)
#define SHIFT_BIT_ISP_DFS_IDLE_DISABLE (4)
#define SHIFT_BIT_CPP_DFS_IDLE_DISABLE (3)
#define SHIFT_BIT_JPG_DFS_IDLE_DISABLE (2)
#define SHIFT_BIT_DCAM_AXI_DFS_IDLE_DISABLE (1)
#define SHIFT_BIT_DCAM_IF_DFS_IDLE_DISABLE (0)

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_MM_DVFS_RESERVED_REG_CFG0, 0x30014230 */
#define BITS_MM_DVFS_RES_REG0(_X_)      ((_X_))

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_MM_DVFS_RESERVED_REG_CFG1, 0x30014234 */
#define BITS_MM_DVFS_RES_REG1(_X_)      ((_X_))

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_MM_DVFS_RESERVED_REG_CFG2, 0x30014238 */
#define BITS_MM_DVFS_RES_REG2(_X_)      ((_X_))

/* bits definitions for register*/
/*REG_MM_DVFS_AHB_MM_DVFS_RESERVED_REG_CFG3, 0x3001423C */
#define BITS_MM_DVFS_RES_REG3(_X_)      ((_X_))

/* vars definitions for controller REGS_MM_DVFS_AHB */
//General Mask
#define MASK_DVFS_THREE_BITS (0x07)
#define MASK_DVFS_TWO_BITS (0x03)
#define MASK_DVFS_ONE_BIT (0x01)
#endif /* __qogirl6_MM_DVFS_H____ */
