/*
 * Copyright (C) 2017-2018 Spreadtrum Communications Inc.
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

#include <linux/bitops.h>

extern unsigned long s_dcam_regbase[];
extern unsigned long s_dcam_aximbase;
extern unsigned long s_dcam_mmubase;

#define DCAM_BASE(idx)                 (s_dcam_regbase[idx])
#define DCAM_IP_REVISION               (0x0000UL)
#define DCAM_CONTROL                   (0x0004UL)
#define DCAM_CFG                       (0x0008UL)
#define DCAM_MODE                      (0x000CUL)

#define DCAM_BIN_BASE_WADDR0           (0x0010UL)
#define DCAM_BIN_BASE_WADDR1           (0x0014UL)
#define DCAM_BIN_BASE_WADDR2           (0x0018UL)
#define DCAM_BIN_BASE_WADDR3           (0x001CUL)
#define DCAM_FULL_BASE_WADDR           (0x0020UL)
#define DCAM_AEM_BASE_WADDR            (0x0024UL)
#define DCAM_LENS_BASE_WADDR           (0x0028UL)
#define DCAM_PDAF_BASE_WADDR           (0x002CUL)
#define DCAM_VCH2_BASE_WADDR           (0x0030UL)
#define DCAM_VCH3_BASE_WADDR           (0x0034UL)
#define DCAM_SPARE_CTRL                (0x0038UL)

#define DCAM_INT_MASK                  (0x003CUL)
#define DCAM_INT_EN                    (0x0040UL)
#define DCAM_INT_CLR                   (0x0044UL)
#define DCAM_INT_RAW                   (0x0048UL)

#define DCAM_PATH_FULL                 (0x004CUL)
#define DCAM_FULL_PATH_STATUS          (0x0050UL)
#define DCAM_BIN_PATH_STATUS           (0x0054UL)
#define DCAM_AEM_PATH_STATUS           (0x0058UL)
#define DCAM_PDAF_PATH_STATUS          (0x005CUL)
#define DCAM_VCH2_PATH_STATUS          (0x0060UL)
#define DCAM_VCH3_PATH_STATUS          (0x0064UL)

#define DCAM_PATH_BUSY                 (0x0068UL)
#define DCAM_PATH_STOP                 (0x006CUL)
#define DCAM_PATH_ENDIAN               (0x0070UL)

#define DCAM_FULL_CFG                  (0x0074UL)
#define DCAM_FULL_CROP_START           (0x0078UL)
#define DCAM_FULL_CROP_SIZE            (0x007CUL)

#define DCAM_APB_SRAM_CTRL             (0x00B0UL)
#define DCAM_PDAF_EXTR_CTRL            (0x00B8UL)
#define DCAM_PDAF_SKIP_FRM             (0x00BCUL)
#define DCAM_PDAF_SKIP_FRM1            (0x00C0UL)
#define DCAM_PDAF_EXTR_ROI_ST          (0x00C4UL)
#define DCAM_PDAF_EXTR_ROI_SIZE        (0x00C8UL)
#define DCAM_PDAF_CONTROL              (0x00F4UL)
#define DCAM_VH2_CONTROL               (0x00F8UL)
#define DCAM0_PDAF_EXTR_POS            (0x0C00UL)

#define DCAM_MIPI_REDUNDANT            (0x00D0UL)
#define DCAM_MIPI_CAP_WORD_CNT         (0x00D4UL)
#define DCAM_MIPI_CAP_FRM_CLR          (0x00D8UL)
#define DCAM_MIPI_CAP_RAW_SIZE         (0x00DCUL)
#define DCAM_PDAF_CAP_FRM_SIZE         (0x00E0UL)
#define DCAM_CAP_VCH2_SIZE             (0x00E4UL)
#define DCAM_CAP_VCH3_SIZE             (0x00E8UL)
#define DCAM_IMAGE_CONTROL             (0x00F0UL)
#define DCAM2_IMAGE_CONTROL            (0x0114UL)

#define DCAM_MIPI_CAP_CFG              (0x0100UL)
#define DCAM_MIPI_CAP_FRM_CTRL         (0x0104UL)
#define DCAM_MIPI_CAP_START            (0x0108UL)
#define DCAM_MIPI_CAP_END              (0x010CUL)

#define DCAM2_MIPI_CAP_START           (0x010CUL)
#define DCAM2_MIPI_CAP_END             (0x0110UL)

#define DCAM_CAM_BIN_CFG               (0x020CUL)
#define DCAM_BIN_CROP_START            (0x0210UL)
#define DCAM_BIN_CROP_SIZE             (0x0214UL)
#define DCAM_CROP0_START               (0x0218UL)
#define DCAM_CROP0_SIZE                (0x021CUL)
#define DCAM_RDS_DES_SIZE              (0x0220UL)

#define DCAM_BLC_PARA_R_B              (0x0224UL)
#define DCAM_BLC_PARA_G                (0x0228UL)

#define RDS_COEFF_START                (0x0340)
#define RDS_COEFF_SIZE                 (0x00C0)

#define ISP_RGBG_PARAM                 (0x022CUL)
#define ISP_RGBG_RB                    (0x0230UL)
#define ISP_RGBG_G                     (0x0234UL)
#define ISP_RGBG_YRANDOM_PARAM0        (0x0238UL)
#define ISP_RGBG_YRANDOM_PARAM1        (0x023CUL)
#define ISP_RGBG_YRANDOM_PARAM2        (0x0240UL)
#define ISP_RGBG_YRANDOM_INIT          (0x013CUL)

#define ISP_LENS_WEIGHT_ADDR           (0x0400UL)
#define ISP_LENS_BASE_RADDR            (0x0028UL)
#define ISP_LENS_GRID_NUMBER           (0x0080UL)
#define ISP_LENS_GRID_SIZE             (0x0084UL)
#define ISP_LENS_LOAD_EB               (0x0088UL)
#define ISP_LENS_LOAD_CLR              (0x008CUL)

#define ISP_AWBC_PARAM                 (0x0244UL)
#define ISP_AWBC_GAIN0                 (0x0248UL)
#define ISP_AWBC_GAIN1                 (0x024CUL)
#define ISP_AWBC_THRD                  (0x0250UL)
#define ISP_AWBC_OFFSET0               (0x0254UL)
#define ISP_AWBC_OFFSET1               (0x0258UL)

#define ISP_AEM_PARAM                  (0x0090UL)
#define ISP_AEM_PARAM1                 (0x0094UL)
#define ISP_AEM_OFFSET                 (0x0098UL)
#define ISP_AEM_BLK_NUM                (0x009CUL)
#define ISP_AEM_BLK_SIZE               (0x00A0UL)
#define ISP_AEM_RED_THR                (0x00A4UL)
#define ISP_AEM_BLUE_THR               (0x00A8UL)
#define ISP_AEM_GREEN_THR              (0x00ACUL)

#define ISP_BPC_GC_CFG                 (0x0158UL)
#define ISP_BPC_PARAM                  (0x015CUL)
#define ISP_BPC_BAD_PIXEL_TH0          (0x025CUL)
#define ISP_BPC_BAD_PIXEL_TH1          (0x0260UL)
#define ISP_BPC_BAD_PIXEL_TH2          (0x0264UL)
#define ISP_BPC_BAD_PIXEL_TH3          (0x0268UL)
#define ISP_BPC_FLAT_TH                (0x026CUL)
#define ISP_BPC_EDGE_RATIO             (0x0270UL)
#define ISP_BPC_BAD_PIXEL_PARAM        (0x0274UL)
#define ISP_BPC_BAD_PIXEL_COEF         (0x0278UL)
#define ISP_BPC_LUTWORD0               (0x027CUL)
#define ISP_BPC_MAP_ADDR               (0x0110UL)
#define ISP_BPC_MAP_CTRL               (0x0160UL)
#define ISP_BPC_MAP_CTRL1              (0x0164UL)
#define ISP_BPC_OUT_ADDR               (0x0114UL)

#define ISP_GRGB_STATUS                (0x0150UL)
#define ISP_GRGB_CTRL                  (0x0168UL)
#define ISP_GRGB_CFG0                  (0x016CUL)
#define ISP_GRGB_LUM_FLAT_T            (0x029CUL)
#define ISP_GRGB_LUM_FLAT_R            (0x02A0UL)
#define ISP_GRGB_LUM_EDGE_T            (0x02A4UL)
#define ISP_GRGB_LUM_EDGE_R            (0x02A8UL)
#define ISP_GRGB_LUM_TEX_T             (0x02ACUL)
#define ISP_GRGB_LUM_TEX_R             (0x02B0UL)
#define ISP_GRGB_FREZ_FLAT_T           (0x02B4UL)
#define ISP_GRGB_FREZ_FLAT_R           (0x02B8UL)
#define ISP_GRGB_FREZ_EDGE_T           (0x02BCUL)
#define ISP_GRGB_FREZ_EDGE_R           (0x02C0UL)
#define ISP_GRGB_FREZ_TEX_T            (0x02C4UL)
#define ISP_GRGB_FREZ_TEX_R            (0x02C8UL)

#define ISP_RAW_AFM_ADDR               (0x0118UL)
#define ISP_RAW_AFM_FRAM_CTRL          (0x0180UL)
#define ISP_RAW_AFM_FRAM_CTRL1         (0x0184UL)
#define ISP_RAW_AFM_PARAMETERS         (0x0188UL)
#define ISP_RAW_AFM_ENHANCE_CTRL       (0x018CUL)
#define ISP_RAW_AFM_DONE_TILE_NUM      (0x0190UL)
#define ISP_RAW_AFM_CROP_START         (0x02CCUL)
#define ISP_RAW_AFM_CROP_SIZE          (0x02D0UL)
#define ISP_RAW_AFM_FRAME_RANGE        (0x2814UL)
#define ISP_RAW_AFM_WIN_RANGE0S        (0x02D4UL)
#define ISP_RAW_AFM_WIN_RANGE0E        (0x02D8UL)
#define ISP_RAW_AFM_WIN_RANGE1S        (0x02DCUL)
#define ISP_RAW_AFM_IIR_FILTER0        (0x02E0UL)
#define ISP_RAW_AFM_IIR_FILTER1        (0x02E4UL)
#define ISP_RAW_AFM_IIR_FILTER2        (0x02E8UL)
#define ISP_RAW_AFM_IIR_FILTER3        (0x02ECUL)
#define ISP_RAW_AFM_IIR_FILTER4        (0x02F0UL)
#define ISP_RAW_AFM_IIR_FILTER5        (0x02F4UL)

#define ISP_RAW_AFM_ENHANCE_FV0_THD    (0x02F8UL)
#define ISP_RAW_AFM_ENHANCE_FV1_THD    (0x02FCUL)
#define ISP_RAW_AFM_ENHANCE_FV1_COEFF00 (0x0300UL)
#define ISP_RAW_AFM_ENHANCE_FV1_COEFF01 (0x0304UL)
#define ISP_RAW_AFM_ENHANCE_FV1_COEFF10 (0x0308UL)
#define ISP_RAW_AFM_ENHANCE_FV1_COEFF11 (0x030CUL)
#define ISP_RAW_AFM_ENHANCE_FV1_COEFF20 (0x0310UL)
#define ISP_RAW_AFM_ENHANCE_FV1_COEFF21 (0x0314UL)
#define ISP_RAW_AFM_ENHANCE_FV1_COEFF30 (0x0318UL)
#define ISP_RAW_AFM_ENHANCE_FV1_COEFF31 (0x031CUL)

#define ISP_ANTI_FLICKER_GLB_WADDR     (0x011CUL)
#define ISP_ANTI_FLICKER_REGION_WADDR  (0X0120UL)
#define ISP_ANTI_FLICKER_PARAM0        (0x01A0UL)
#define ISP_ANTI_FLICKER_FRAM_CTRL     (0x01A4UL)
#define ISP_ANTI_FLICKER_NEW_CFG_READY (0x01A8UL)
#define ISP_ANTI_FLICKER_NEW_PARAM1    (0x0320UL)
#define ISP_ANTI_FLICKER_NEW_PARAM2    (0x0324UL)
#define ISP_ANTI_FLICKER_NEW_COL_POS   (0x0328UL)
#define ISP_ANTI_FLICKER_NEW_REGION0   (0x032CUL)
#define ISP_ANTI_FLICKER_NEW_REGION1   (0x0330UL)
#define ISP_ANTI_FLICKER_NEW_REGION2   (0x0334UL)
#define ISP_ANTI_FLICKER_NEW_SUM1      (0x0338UL)
#define ISP_ANTI_FLICKER_NEW_SUM2      (0x033CUL)

#define DCAM_NR3_BASE_WADDR            (0x0124UL)
#define DCAM_NR3_STATUS                (0x01B0UL)
#define DCAM_NR3_PARA0                 (0x01B4UL)
#define DCAM_NR3_PARA1                 (0x01B8UL)
#define DCAM_NR3_ROI_PARA0             (0x01BCUL)
#define DCAM_NR3_ROI_PARA1             (0x01C0UL)
#define DCAM_NR3_OUT0                  (0x01C4UL)
#define DCAM_NR3_OUT1                  (0x01C8UL)

#define DCAM_AXIM_BASE                 s_dcam_aximbase
#define DCAM_AXIM_CTRL                 (0x0000UL)
#define DCAM_AXIM_DBG_STS              (0x0004UL)
#define DCAM_CAP_SENSOR_CTRL           (0x0008UL)

#define REG_DCAM_IMG_FETCH_START       (0x0020)
#define REG_DCAM_IMG_FETCH_CTRL        (0x0024)
#define REG_DCAM_IMG_FETCH_SIZE        (0x0028)
#define REG_DCAM_IMG_FETCH_X           (0x002c)
#define REG_DCAM_IMG_FETCH_RADDR       (0x0030)

#define DCAM_MMU_BASE                  s_dcam_mmubase
#define DCAM_MMU_EN                    (0x0000UL)
#define DCAM_MMU_UPDATA                (0x0004UL)
#define DCAM_MMU_MIN_VPN               (0x0008UL)
#define DCAM_MMU_VPN_RANGE             (0x000CUL)
#define DCAM_MMU_PT_ADDR               (0x0010UL)
#define DCAM_MMU_DEFAULT_PAGE          (0x0014UL)
#define DCAM_MMU_VAOR_ADDR_RD          (0x0018UL)
#define DCAM_MMU_VAOR_ADDR_WR          (0x001CUL)
#define DCAM_MMU_INV_ADDR_RD           (0x0020UL)
#define DCAM_MMU_INV_ADDR_WR           (0x0024UL)
#define DCAM_MMU_UNS_ADDR_RD           (0x0028UL)
#define DCAM_MMU_UNS_ADDR_WR           (0x002CUL)
#define DCAM_MMU_MISS_CNT              (0x0030UL)
#define DCAM_MMU_PT_UPDATA_QOS         (0x0034UL)
#define DCAM_MMU_VERSION               (0x0038UL)
#define DCAM_MMU_MIN_PPN1              (0x003CUL)
#define DCAM_MMU_PPN_RANGE1            (0x0040UL)
#define DCAM_MMU_MIN_PPN               (0x0044UL)
#define DCAM_MMU_PPN_RANGE2            (0x0048UL)
#define DCAM_MMU_VPN_PAOR_RD           (0x004CUL)
#define DCAM_MMU_VPN_PAOR_WR           (0x0050UL)
#define DCAM_MMU_PPN_PAOR_RD           (0x0054UL)
#define DCAM_MMU_PPN_PAOR_WR           (0x0058UL)
#define DCAM_MMU_REG_AU_MANAGE         (0x005CUL)
#define DCAM_MMU_STS                   (0x0080UL)
#define DCAM_MMU_EN_SHAD               (0x0084UL)

#define DCAM_CAP_SKIP_FRM_MAX          16
#define DCAM_FRM_DECI_FAC_MAX          4
#define DCAM_CAP_FRAME_WIDTH_MAX       4672
#define DCAM_CAP_FRAME_HEIGHT_MAX      3504

#define DCAM1_CAP_FRAME_WIDTH_MAX      3264
#define DCAM1_CAP_FRAME_HEIGHT_MAX     2448
#define DCAM2_CAP_FRAME_WIDTH_MAX      1632
#define DCAM2_CAP_FRAME_HEIGHT_MAX     1224

#define DCAM_CAP_X_DECI_FAC_MAX        4
#define DCAM_CAP_Y_DECI_FAC_MAX        4

#define DCAM_BIN_WIDTH_MAX            4672
#define DCAM_BIN_HEIGHT_MAX           3504

#define DCAM_SCALING_THRESHOLD         4672

#define CAMERA_SC_COEFF_UP_MAX         4
#define CAMERA_SC_COEFF_DOWN_MAX       4
#define CAMERA_PATH_DECI_FAC_MAX       4

#define DCAM_MAX_COUNT                 DCAM_ID_MAX
#define DCAM0_3DNR_ME_WIDTH_MAX        4608
#define DCAM0_3DNR_ME_HEIGHT_MAX       3456
#define DCAM1_3DNR_ME_WIDTH_MAX        (2128 << 1)
#define DCAM1_3DNR_ME_HEIGHT_MAX       (1600 << 1)

enum dcam_id {
	DCAM_ID_0 = 0,
	DCAM_ID_1,
	DCAM_ID_2,
	DCAM_ID_MAX,
};

enum camera_copy_id {
	CAP_COPY = BIT(0),
	RDS_COPY = BIT(1),
	FULL_COPY = BIT(2),
	BIN_COPY = BIT(3),
	AEM_COPY = BIT(4),
	PDAF_COPY = BIT(5),
	VCH2_COPY = BIT(6),
	VCH3_COPY = BIT(7),
	ALL_COPY = 0xFF,
};

#define DCAM_REG_WR(idx, reg, val) (REG_WR(DCAM_BASE(idx)+reg, val))
#define DCAM_REG_RD(idx, reg) (REG_RD(DCAM_BASE(idx)+reg))
#define DCAM_REG_MWR(idx, reg, msk, val) DCAM_REG_WR(idx, reg, \
	((val) & (msk)) | (DCAM_REG_RD(idx, reg) & (~(msk))))
#define DCAM_AXIM_WR(reg, val) (REG_WR(DCAM_AXIM_BASE+reg, val))
#define DCAM_AXIM_RD(reg) (REG_RD(DCAM_AXIM_BASE+reg))
#define DCAM_AXIM_MWR(reg, msk, val) DCAM_AXIM_WR(reg, \
	((val) & (msk)) | (DCAM_AXIM_RD(reg) & (~(msk))))

#define DCAM_MMU_WR(reg, val) (REG_WR(DCAM_MMU_BASE+reg, val))
#define DCAM_MMU_RD(reg) (REG_RD(DCAM_MMU_BASE+reg))

#endif /* _DCAM_REG_H_ */
