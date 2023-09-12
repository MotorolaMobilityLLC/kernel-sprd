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

#ifndef _DCAM_REG_H_
#define _DCAM_REG_H_

extern unsigned long s_dcam_regbase[];

#define DCAM_BASE(idx)              (s_dcam_regbase[idx])
#define DCAM0_CFG                   (0x0000UL)
#define DCAM0_CONTROL               (0x0004UL)
#define DCAM0_FULL_CFG              (0x0008UL)
#define DCAM0_FULL_CROP_START       (0x000CUL)
#define DCAM0_FULL_CROP_SIZE        (0x0010UL)
#define DCAM0_CAM_BIN_CFG           (0x0014UL)
#define DCAM0_BIN_CROP_START        (0x0018UL)
#define DCAM0_BIN_CROP_SIZE         (0x001CUL)
#define DCAM0_INT_MASK              (0x003CUL)
#define DCAM0_INT_EN                (0x0040UL)
#define DCAM0_INT_CLR               (0x0044UL)
#define DCAM0_INT_RAW               (0x0048UL)
#define DCAM0_FULL_BASE_WADDR       (0x004CUL)
#define DCAM0_BIN_BASE_WADDR0       (0x0050UL)
#define DCAM0_AEM_BASE_WADDR        (0x0054UL)
#define DCAM0_LENS_BASE_RADDR       (0x0058UL)
#define DCAM0_PDAF_BASE_WADDR       (0x005CUL)
#define DCAM0_VCH2_BASE_WADDR       (0x0060UL)
#define DCAM0_VCH3_BASE_WADDR       (0x0064UL)
#define DCAM0_PATH_BUSY             (0x0068UL)
#define DCAM0_PATH_STOP             (0x006CUL)
#define DCAM0_MIPI_CAP_CFG          (0x0100UL)
#define DCAM0_MIPI_CAP_FRM_CTRL     (0x0104UL)
#define DCAM0_MIPI_CAP_FRM_CLR      (0x0108UL)
#define DCAM0_MIPI_CAP_START        (0x010CUL)
#define DCAM0_MIPI_CAP_END          (0x0110UL)
#define DCAM0_MIPI_CAP_WORD_CNT     (0x0114UL)
#define DCAM0_MIPI_CAP_HEIGHT_SIZE  (0x0118UL)
#define DCAM0_VC2_CONTROL           (0x011CUL)
#define DCAM0_IMAGE_DT_VC_CONTROL   (0x0120UL)
#define DCAM0_PDAF_CONTROL          (0x0124UL)
#define DCAM0_MIPI_REDUNDANT        (0x0128UL)
#define DCAM0_PDAF_CAP_FRM_SIZE     (0x012CUL)
#define DCAM0_CAP_VCH2_SIZE         (0x0130UL)
#define DCAM0_CAP_VCH3_SIZE         (0x0134UL)
#define DCAM0_MODE                  (0x0140UL)
#define DCAM0_BIN_BASE_WADDR1       (0x0144UL)
#define DCAM0_BIN_BASE_WADDR2       (0x0148UL)
#define DCAM0_BIN_BASE_WADDR3       (0x014CUL)
#define DCAM0_IP_REVISION           (0x0180UL)
#define DCAM0_FULL_PATH_STATUS      (0x0184UL)
#define DCAM0_BIN_PATH_STATUS       (0x0188UL)
#define DCAM0_AEM_PATH_STATUS       (0x018CUL)
#define DCAM0_PDAF_PATH_STATUS      (0x0190UL)
#define DCAM0_VCH2_PATH_STATUS      (0x0194UL)
#define DCAM0_VCH3_PATH_STATUS      (0x0198UL)
#define DCAM0_PATH_FULL             (0x019CUL)
#define DCAM0_BIN_CORE_CTRL         (0x0200UL)
#define DCAM0_PDAF_EXTR_CTRL        (0x0240UL)
#define DCAM0_PDAF_SKIP_FRM         (0x0244UL)
#define DCAM0_PDAF_EXTR_ROI_ST      (0x0248UL)
#define DCAM0_PDAF_EXTR_ROI_SIZE    (0x024CUL)
#define DCAM0_BLC_BYPASS            (0x0250UL)
#define DCAM0_PATH_ENDIAN           (0x0260UL)
#define DCAM0_AEM_PARA              (0x0300UL)
#define DCAM0_BLC_PARA_R_B          (0x0304UL)
#define DCAM0_BLC_PARA_G            (0x0308UL)
#define DCAM0_LENS_LOAD_ENABLE      (0x030CUL)
#define DCAM0_AEM_CFG_READY         (0x0310UL)
#define DCAM0_LENS_GRID_NUMBER      (0x0314UL)
#define DCAM0_LENS_GRID_SIZE        (0x0318UL)
#define DCAM0_AEM_OFFSET            (0x031CUL)
#define DCAM0_AEM_BLK_SIZE          (0x0320UL)
#define DCAM0_LENS_LOAD_DONE        (0x0324UL)
#define DCAM0_AEM_SKIP_NUM_CLR      (0x0328UL)
#define DCAM0_AEM_RED_THR           (0x032CUL)
#define DCAM0_AEM_BLUE_THR          (0x0330UL)
#define DCAM0_AEM_GREEN_THR         (0x0334UL)
#define DCAM0_APB_SRAM_CTRL         (0x0380UL)
#define DCAM0_PDAF_EXTR_POS         (0x0400UL)
#define DCAM0_AEM_WEI_TABLE         (0x0800UL)

#define DCAM1_CFG                   (0x0000UL)
#define DCAM1_CONTROL               (0x0004UL)
#define DCAM1_FULL_CFG              (0x0008UL)
#define DCAM1_FULL_CROP_START       (0x000CUL)
#define DCAM1_FULL_CROP_SIZE        (0x0010UL)
#define DCAM1_CAM_BIN_CFG           (0x0014UL)
#define DCAM1_BIN_CROP_START        (0x0018UL)
#define DCAM1_BIN_CROP_SIZE         (0x001CUL)
#define DCAM1_INT_MASK              (0x003CUL)
#define DCAM1_INT_EN                (0x0040UL)
#define DCAM1_INT_CLR               (0x0044UL)
#define DCAM1_INT_RAW               (0x0048UL)
#define DCAM1_FULL_BASE_WADDR       (0x004CUL)
#define DCAM1_BIN_BASE_WADDR0       (0x0050UL)
#define DCAM1_AEM_BASE_WADDR        (0x0054UL)
#define DCAM1_LENS_BASE_RADDR       (0x0058UL)
#define DCAM1_VCH_BASE_WADDR        (0x005CUL)
#define DCAM1_PATH_BUSY             (0x0068UL)
#define DCAM1_PATH_STOP             (0x006CUL)
#define DCAM1_MIPI_CAP_CFG          (0x0100UL)
#define DCAM1_MIPI_CAP_FRM_CTRL     (0x0104UL)
#define DCAM1_MIPI_CAP_FRM_CLR      (0x0108UL)
#define DCAM1_MIPI_CAP_START        (0x010CUL)
#define DCAM1_MIPI_CAP_END          (0x0110UL)
#define DCAM1_MIPI_CAP_WORD_CNT     (0x0114UL)
#define DCAM1_MIPI_CAP_HEIGHT_SIZE  (0x0118UL)
#define DCAM1_IMAGE_DT_VC_CONTROL   (0x011CUL)
#define DCAM1_MIPI_REDUNDANT        (0x0128UL)
#define DCAM1_CAP_VCH_SIZE          (0x0134UL)
#define DCAM1_MODE                  (0x0140UL)
#define DCAM1_BIN_BASE_WADDR1       (0x0144UL)
#define DCAM1_BIN_BASE_WADDR2       (0x0148UL)
#define DCAM1_BIN_BASE_WADDR3       (0x014CUL)
#define DCAM1_IP_REVISION           (0x0180UL)
#define DCAM1_FULL_PATH_STATUS      (0x0184UL)
#define DCAM1_BIN_PATH_STATUS       (0x0188UL)
#define DCAM1_AEM_PATH_STATUS       (0x018CUL)
#define DCAM1_VCH_PATH_STATUS       (0x0190UL)
#define DCAM1_PATH_FULL             (0x019CUL)
#define DCAM1_BIN_CORE_CTRL         (0x0200UL)
#define DCAM1_BLC_BYPASS            (0x0250UL)
#define DCAM1_PATH_ENDIAN           (0x0260UL)
#define DCAM1_AEM_PARA              (0x0300UL)
#define DCAM1_BLC_PARA_R_B          (0x0304UL)
#define DCAM1_BLC_PARA_G            (0x0308UL)
#define DCAM1_LENS_LOAD_ENABLE      (0x030CUL)
#define DCAM1_AEM_CFG_READY         (0x0310UL)
#define DCAM1_LENS_GRID_NUMBER      (0x0314UL)
#define DCAM1_LENS_GRID_SIZE        (0x0318UL)
#define DCAM1_AEM_OFFSET            (0x031CUL)
#define DCAM1_AEM_BLK_SIZE          (0x0320UL)
#define DCAM1_LENS_LOAD_DONE        (0x0324UL)
#define DCAM1_AEM_SKIP_NUM_CLR      (0x0328UL)
#define DCAM1_AEM_RED_THR           (0x032CUL)
#define DCAM1_AEM_BLUE_THR          (0x0330UL)
#define DCAM1_AEM_GREEN_THR         (0x0334UL)
#define DCAM1_APB_SRAM_CTRL         (0x0380UL)
#define DCAM1_AEM_WEI_TABLE         (0x0800UL)

#define DCAM_AXIM_CTRL              (0x0000UL)
#define DCAM_AXIM_DBG_STS           (0x0004UL)
#define DCAM_AXIM_WORD_ENDIAN       (0x000CUL)

#define DCAM_MMU_EN                 (0x0100UL)
#define DCAM_MMU_VAOR_ADDR_RD       (0x0118UL)
#define DCAM_MMU_VAOR_ADDR_WR       (0x011CUL)
#define DCAM_MMU_INV_ADDR_RD        (0x0120UL)
#define DCAM_MMU_INV_ADDR_WR        (0x0124UL)
#define DCAM_MMU_PT_UPDATE_QOS      (0x0134UL)
#define DCAM_MMU_STS                (0x0160UL)
#define DCAM_MMU_PT_UPDATE_QOS_MASK (0xF)

#define DCAM_AXIM_AQOS_MASK         (0x1E70F0F)

#define DCAM_CAP_SKIP_FRM_MAX       16
#define DCAM_FRM_DECI_FAC_MAX       4
#define DCAM_CAP_FRAME_WIDTH_MAX    4224
#define DCAM_CAP_FRAME_HEIGHT_MAX   3136

#define DCAM_ISP_LINE_BUF_LENGTH    4224
#define DCAM_SCALING_THRESHOLD      2600

#define DCAM_CAP_X_DECI_FAC_MAX     4
#define DCAM_CAP_Y_DECI_FAC_MAX     4
#define CAMERA_SC_COEFF_UP_MAX      4
#define CAMERA_SC_COEFF_DOWN_MAX    4
#define CAMERA_PATH_DECI_FAC_MAX    4

#define DCAM_IRQ                    IRQ_DCAM_INT

#define DCAM0_CFG_PATH_FULL         BIT(0)
#define DCAM0_CFG_PATH_BIN          BIT(1)
#define DCAM0_CFG_PATH_AEM          BIT(2)
#define DCAM0_CFG_PATH_PDAF         BIT(3)
#define DCAM0_CFG_PATH_VCH2         BIT(4)
#define DCAM0_CFG_PATH_VCH3         BIT(5)
#define DCAM1_CFG_PATH_FULL         BIT(0)
#define DCAM1_CFG_PATH_BIN          BIT(1)
#define DCAM1_CFG_PATH_AEM          BIT(2)
#define DCAM1_CFG_PATH_VCH          BIT(3)

enum dcam_id {
	DCAM_ID_0 = 0,
	DCAM_ID_1 = 1,
	DCAM_ID_MAX = 2,
};

enum camera_path_id {
	CAMERA_RAW_PATH = 0,
	CAMERA_PRE_PATH,
	CAMERA_VID_PATH,
	CAMERA_CAP_PATH,
	CAMERA_PDAF_PATH,
	CAMERA_AEM_PATH,
	CAMERA_EBD_PATH,
	CAMERA_MAX_PATH,
};

#define DCAM_REG_WR(idx, reg, val)  (REG_WR(DCAM_BASE(idx)+reg, val))
#define DCAM_REG_RD(idx, reg)  (REG_RD(DCAM_BASE(idx)+reg))
#define DCAM_REG_MWR(idx, reg, msk, val)  DCAM_REG_WR(idx, reg, \
	((val) & (msk)) | (DCAM_REG_RD(idx, reg) & (~(msk))))
#define DCAM_AXI_REG_WR(reg, val)  (REG_WR(s_dcam_axi_base+reg, val))
#define DCAM_AXI_REG_RD(reg)  (REG_RD(s_dcam_axi_base+reg))
#define DCAM_AXI_REG_MWR(reg, msk, val)  DCAM_AXI_REG_WR(reg, \
	((val) & (msk)) | (DCAM_AXI_REG_RD(reg) & (~(msk))))

#endif /* _DCAM_REG_H_ */
