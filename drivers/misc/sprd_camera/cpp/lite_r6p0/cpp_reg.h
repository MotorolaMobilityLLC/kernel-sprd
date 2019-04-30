/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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

#ifndef _CPP_REG_H_
#define _CPP_REG_H_

#define MM_AHB_RESET                (0x0004UL)
#define CPP_DMA_AHB_RESET_BIT       (1 << 13)
#define CPP_PATH1_AHB_RESET_BIT     (1 << 14)
#define CPP_PATH0_AHB_RESET_BIT     (1 << 15)
#define CPP_AHB_RESET_BIT           (1 << 16)
#define CPP_AHB_REST_MASK           (1 << 18)
#define CPP_PATH_RESET_MASK \
		(CPP_AHB_RESET_BIT | CPP_PATH0_AHB_RESET_BIT | \
		CPP_PATH1_AHB_RESET_BIT | CPP_DMA_AHB_RESET_BIT)

#define CPP_BASE                    0x00

#define CPP_PATH_EB                 0x00
#define CPP_DMA_EB_BIT              BIT(2)
#define CPP_ROT_EB_BIT              BIT(1)
#define CPP_SCALE_PATH_EB_BIT       BIT(0)

#define CPP_PATH_START              0x04
#define CPP_DMA_START_BIT           BIT(2)
#define CPP_ROT_START_BIT           BIT(1)
#define CPP_SCALE_START_BIT         BIT(0)

#define CPP_PATH_STS                0x08
#define CPP_PATH2_STATUS_BIT_BUSY   BIT(2)
#define CPP_PATH1_STATUS_BIT_BUSY   BIT(1)
#define CPP_PATH0_STATUS_BIT_BUSY   BIT(0)

#define CPP_INT_STS                 0x0C
#define CPP_MMU_PAOR_WD_STATUS_BIT  BIT(10)
#define CPP_MMU_PAOR_RD_STATUS_BIT  BIT(9)
#define CPP_MMU_UNS_WR_STATUS_BIT   BIT(8)
#define CPP_MMU_UNS_RD_STATUS_BIT   BIT(7)
#define CPP_MMU_INV_WR_STATUS_BIT   BIT(6)
#define CPP_MMU_INV_RD_STATUS_BIT   BIT(5)
#define CPP_MMU_VAOR_WR_STATUS_BIT  BIT(4)
#define CPP_MMU_VAOR_RD_STATUS_BIT  BIT(3)
#define CPP_PATH2_IRQ_STATUS_BIT    BIT(2)
#define CPP_PATH1_IRQ_STATUS_BIT    BIT(1)
#define CPP_PATH0_IRQ_STATUS_BIT    BIT(0)

#define CPP_INT_MASK                0x10
#define SC_PATH_INT_BIT             BIT(0)
#define ROT_PATH_INT_BIT            BIT(1)
#define DMA_PATH_INT_BIT            BIT(2)
#define MMU_VAOR_RD_INT_BIT         BIT(3)
#define MMU_VAOR_WR_INT_BIT         BIT(4)
#define MMU_INV_RD_INT_BIT          BIT(5)
#define MMU_INV_WR_INT_BIT          BIT(6)
#define MMU_UNS_RD_INT_BIT          BIT(7)
#define MMU_UNS_WR_INT_BIT          BIT(8)
#define MMU_PAOR_RD_INT_BIT         BIT(9)
#define MMU_PAOR_WR_INT_BIT         BIT(10)

#define CPP_PATH_DONE \
		(ROT_PATH_INT_BIT | SC_PATH_INT_BIT | \
		DMA_PATH_INT_BIT)
#define CPP_MMU_ERROR_INT \
		((MMU_VAOR_RD_INT_BIT | MMU_VAOR_WR_INT_BIT) | \
		(MMU_INV_RD_INT_BIT | MMU_INV_WR_INT_BIT) | \
		(MMU_UNS_RD_INT_BIT | MMU_UNS_RD_INT_BIT) | \
		(MMU_PAOR_RD_INT_BIT | MMU_PAOR_WR_INT_BIT))

#define CPP_INT_CLR                     0x14
#define CPP_INT_RAW                     0x18
#define CPP_DMA_IRQ_BIT                 BIT(2)
#define CPP_ROT_IRQ_BIT                 BIT(1)
#define CPP_SCALE_IRQ_BIT               BIT(0)

#define CPP_AXIM_CHN_SET                0x1C
#define CPP_PATH0_IN_ENDIAN             (7 << 0)
#define CPP_PATH0_IN_UV_ENDIAN          (1 << 3)
#define CPP_PATH0_OUT_ENDIAN            (7 << 4)
#define CPP_PATH0_OUT_UV_ENDIAN         (1 << 7)
#define CPP_PATH1_ENDIAN                (7 << 8)
#define CPP_PATH2_IN_ENDIAN             (7 << 16)
#define CPP_PATH2_OUT_ENDIAN            (7 << 19)
#define CPP_AXIM_CHN_SET_QOS_MASK       (0xF << 24)
#define CPP_AXIM_ARCACHE                (3 << 28)
#define CPP_AXIM_AWCACHE                (3 << 30)

#define CPP_AXIM_BURST_GAP_PATH0        0x20
#define CPP_PATH0_WCH_BURST_GAP         0xFFFF
#define CPP_PATH0_RCH_BURST_GAP         (0xFFFF << 16)
#define CPP_AXIM_BURST_GAP_PATH1        0x24
#define CPP_AXIM_BURST_GAP_PATH2        0x28

#define CPP_AXIM_STS                    0x30
#define CPP_AXIM_RCH_STS                BIT(2)
#define CPP_AXIM_WCH_STS                BIT(1)
#define CPP_AXIM_CHN_STS                BIT(0)

#define CPP_PATH0_SRC_ADDR_Y            0x40
#define CPP_PATH0_SRC_ADDR_UV           0x44
#define CPP_PATH0_SC_DES_ADDR_Y         0x48
#define CPP_PATH0_SC_DES_ADDR_UV        0x4C
#define CPP_PATH0_BP_DES_ADDR_Y         0x50
#define CPP_PATH0_BP_DES_ADDR_UV        0x54

#define CPP_PATH0_CFG0                  0x58
#define CPP_PATH0_INPUT_FORMAT          (3 << 0)
#define CPP_PATH0_OUTPUT_FORMAT         (3 << 2)
#define CPP_PATH0_HOR_DECI              (3 << 4)
#define CPP_PATH0_VER_DECI              (3 << 6)
#define CPP_PATH0_BP_EB                 BIT(8)
#define CPP_PATH0_CLK_SWITCH            (1 << 9)

#define CPP_PATH0_CFG1                  0x5C
#define CPP_PATH0_SRC_HEIGHT_MASK       (0x3FFF << 16)
#define CPP_PATH0_SRC_WIDTH_MASK        0x3FFF

#define CPP_PATH0_CFG2                  0x60
#define CPP_PATH0_SC_DES_HEIGHT_MASK    (0x3FFF << 16)
#define CPP_PATH0_SC_DES_WIDTH_MASK     (0x3FFF)

#define CPP_PATH0_CFG3                  0x64
#define CPP_PATH0_BP_DES_HEIGHT_MASK    (0x3FFF << 16)
#define CPP_PATH0_BP_DES_WIDTH_MASK     0x3FFF

#define CPP_PATH0_CFG4                  0x68
#define CPP_PATH0_SRC_PITCH             0x3FFF

#define CPP_PATH0_CFG5                  0x6C
#define CPP_PATH0_BP_DES_PITCH          (0x3FFF << 16)
#define CPP_PATH0_SC_DES_PITCH          0x3FFF

#define CPP_PATH0_CFG6                  0x70
#define CPP_PATH0_SRC_OFFSET_X_MASK     (0x3FFF << 16)
#define CPP_PATH0_SRC_OFFSET_Y_MASK     0x3FFF

#define CPP_PATH0_CFG7                  0x74
#define CPP_PATH0_SC_DES_OFFSET_X_MASK  (0x3FFF << 16)
#define CPP_PATH0_SC_DES_OFFSET_Y_MASK  0x3FFF

#define CPP_PATH0_CFG8                  0x78
#define CPP_PATH0_BP_DES_OFFSET_X_MASK  (0x3FFF << 16)
#define CPP_PATH0_BP_DES_OFFSET_Y_MASK  0x3FFF

#define CPP_PATH0_SC_IN_TRIM_OFFSET             0x7C
#define CPP_PATH0_SC_IN_TRIM_OFFSET_X_MASK      (0x3FFF << 16)
#define CPP_PATH0_SC_IN_TRIM_OFFSET_Y_MASK      0x3FFF

#define CPP_PATH0_SC_IN_TRIM_SIZE               0x80
#define CPP_PATH0_SC_IN_TRIM_HEIGHT_MASK        (0x3FFF << 16)
#define CPP_PATH0_SC_IN_TRIM_WIDTH_MASK         0x3FFF

#define CPP_PATH0_SC_OUT_TRIM_OFFSET            0x84
#define CPP_PATH0_SC_OUT_TRIM_OFFSET_X_MASK     (0x3FFF << 16)
#define CPP_PATH0_SC_OUT_TRIM_OFFSET_Y_MASK     0x3FFF

#define CPP_PATH0_SC_OUT_TRIM_SIZE              0x88
#define CPP_PATH0_SC_OUT_TRIM_HEIGHT_MASK       (0x3FFF << 16)
#define CPP_PATH0_SC_OUT_TRIM_WIDTH_MASK        0x3FFF

#define CPP_PATH0_BP_TRIM_OFFSET                0x8C
#define CPP_PATH0_BP_TRIM_OFFSET_X_MASK         (0x3FFF << 16)
#define CPP_PATH0_BP_TRIM_OFFSET_Y_MASK         0x3FFF

#define CPP_PATH0_BP_TRIM_SIZE                  0x90
#define CPP_PATH0_BP_TRIM_HEIGHT_MASK           (0x3FFF << 16)
#define CPP_PATH0_BP_TRIM_WIDTH_MASK            0x3FFF

#define CPP_PATH0_SC_FULL_IN_SIZE               0x94
#define CPP_PATH0_SC_FULL_IN_HEIGHT_MASK        (0x3FFF << 16)
#define CPP_PATH0_SC_FULL_IN_WIDTH_MASK         0x3FFF

#define CPP_PATH0_SC_FULL_OUT_SIZE              0x98
#define CPP_PATH0_SC_FULL_OUT_HEIGHT_MASK       (0x3FFF << 16)
#define CPP_PATH0_SC_FULL_OUT_WIDTH_MASK        0x3FFF

#define CPP_PATH0_SC_SLICE_IN_SIZE              0x9C
#define CPP_PATH0_SC_SLICE_IN_HEIGHT_MASK       (0x3FFF << 16)
#define CPP_PATH0_SC_SLICE_IN_WIDTH_MASK        0x3FFF

#define CPP_PATH0_SC_SLICE_OUT_SIZE             0xA0
#define CPP_PATH0_SC_SLICE_OUT_HEIGHT_MASK      (0x3FFF << 16)
#define CPP_PATH0_SC_SLICE_OUT_WIDTH_MASK       0x3FFF

#define CPP_PATH0_SC_Y_HOR_INI_PHASE            0xA4
#define CPP_PATH0_SC_Y_HOR_INI_PHASE_INT        (0xF << 16)
#define CPP_PATH0_SC_Y_HOR_INI_PHASE_FRAC       0x1FFF

#define CPP_PATH0_SC_UV_HOR_INI_PHASE           0xA8
#define CPP_PATH0_SC_UV_HOR_INI_PHASE_INT       (0xF << 16)
#define CPP_PATH0_SC_UV_HOR_INI_PHASE_FRAC      0x1FFF

#define CPP_PATH0_SC_Y_VER_INI_PHASE            0xAC
#define CPP_PATH0_SC_Y_VER_INI_PHASE_INT        (0xF << 16)
#define CPP_PATH0_SC_Y_VER_INI_PHASE_FRAC       0x1FFF

#define CPP_PATH0_SC_UV_VER_INI_PHASE           0xB0
#define CPP_PATH0_SC_UV_VER_INI_PHASE_INT       (0xF << 16)
#define CPP_PATH0_SC_UV_VER_INI_PHASE_FRAC      0x1FFF

#define CPP_PATH0_SC_TAP                        0xB4
#define CPP_PATH0_Y_VER_TAP                     (0xF << 5)
#define CPP_PATH0_UV_VER_TAP                    0x1F

#define CPP_PATH0_SC_YUV_REGULATE_0             0xC0
#define CPP_PATH0_SC_YUV_REGULATE_1             0xC4
#define CPP_PATH0_SC_YUV_REGULATE_2             0xC8
#define CPP_PATH0_BP_YUV_REGULATE_0             0xD0
#define CPP_PATH0_BP_YUV_REGULATE_1             0xD4
#define CPP_PATH0_BP_YUV_REGULATE_2             0xD8

#define CPP_ROTATION_SRC_ADDR                   0x100
#define CPP_ROTATION_DES_ADDR                   0x104
#define CPP_ROTATION_IMG_SIZE                   0x108
#define CPP_ROTATION_SRC_PITCH                  0x10C
#define CPP_ROTATION_OFFSET_START               0x110
#define CPP_ROTATION_PATH_CFG                   0x114
#define CPP_ROT_UV_MODE_BIT                     (1 << 4)
#define CPP_ROT_MODE_MASK                       (3 << 2)
#define CPP_ROT_PIXEL_FORMAT_BIT                (3 << 0)

#define CPP_DMA_SRC_ADDR                        0x180
#define CPP_DMA_DES_ADDR                        0x184
#define CPP_DMA_CFG                             0x188
#define CPP_DMA_TOTAL_NUM_MASK                  0xFFFFF

#define CPP_END                                 0x200

#define CPP_MMU_EN                              0x0200
#define MMU_PPN_RANGE1                          0x0240
#define MMU_PPN_RANGE2                          0x0248

#define CPP_MMU_PT_UPDATE_QOS                   0x0234
#define CPP_MMU_PT_UPDATE_QOS_MASK              (0xF << 0)

#define PATH0_LUMA_H_COEFF_BASE_ADDR            0x0400
#define PATH0_LUMA_H_COEFF_NUM                  32

#define PATH0_CHROMA_H_COEFF_BASE_ADDR          0x0480
#define PATH0_CHROMA_H_COEFF_NUM                16

#define PATH0_V_COEFF_BASE_ADDR                 0x04F0
#define PATH0_V_COEFF_NUM                       132
#endif
