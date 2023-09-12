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

#include <linux/bitops.h>
#include "dcam_interface.h"

extern unsigned long g_dcam_regbase[];
extern unsigned long g_dcam_aximbase[];
extern unsigned long g_dcam_mmubase;
extern unsigned long g_dcam_phys_base[];

/* DCAM0/DCAM1 module registers define */
#define DCAM_IP_REVISION            (0x0000UL)
#define DCAM_CONTROL                (0x0004UL)
#define DCAM_CFG                    (0x0008UL)
#define DCAM_MODE                   (0x000CUL)
#define DCAM_BIN_BASE_WADDR0        (0x0010UL)
#define DCAM_BIN_BASE_WADDR1        (0x0014UL)
#define DCAM_BIN_BASE_WADDR2        (0x0018UL)
#define DCAM_BIN_BASE_WADDR3        (0x001CUL)
#define DCAM_FULL_BASE_WADDR        (0x0020UL)
#define DCAM_AEM_BASE_WADDR         (0x0024UL)
#define DCAM_LENS_BASE_RADDR        (0x0028UL)
#define DCAM_PDAF_BASE_WADDR        (0x002CUL)
#define DCAM_VCH2_BASE_WADDR        (0x0030UL)
#define DCAM_VCH3_BASE_WADDR        (0x0034UL)

#define DCAM_INT_MASK               (0x003CUL)
#define DCAM_INT_EN                 (0x0040UL)
#define DCAM_INT_CLR                (0x0044UL)
#define DCAM_INT_RAW                (0x0048UL)

#define DCAM_PATH_FULL_STS          (0x004CUL)
#define DCAM_FULL_PATH_STATUS       (0x0050UL)
#define DCAM_BIN_PATH_STATUS        (0x0054UL)
#define DCAM_AEM_PATH_STATUS        (0x0058UL)
#define DCAM_PDAF_PATH_STATUS       (0x005CUL)
#define DCAM_VCH2_PATH_STATUS       (0x0060UL)
#define DCAM_VCH3_PATH_STATUS       (0x0064UL)
#define DCAM_PATH_BUSY              (0x0068UL)
#define DCAM_PATH_STOP              (0x006CUL)
#define DCAM_PATH_ENDIAN            (0x0070UL)

#define DCAM_FULL_CFG               (0x0074UL)
#define DCAM_FULL_CROP_START        (0x0078UL)
#define DCAM_FULL_CROP_SIZE         (0x007CUL)

#define DCAM_LENS_GRID_NUMBER       (0x0080UL)
#define DCAM_LENS_GRID_SIZE         (0x0084UL)
#define DCAM_LENS_LOAD_ENABLE       (0x0088UL)
#define DCAM_LENS_LOAD_CLR          (0x008CUL)

#define DCAM_AEM_FRM_CTRL0          (0x0090UL)
#define DCAM_AEM_FRM_CTRL1          (0x0094UL)
#define DCAM_AEM_OFFSET             (0x0098UL)
#define DCAM_AEM_BLK_NUM            (0x009CUL)
#define DCAM_AEM_BLK_SIZE           (0x00A0UL)
#define DCAM_AEM_RED_THR            (0x00A4UL)
#define DCAM_AEM_BLUE_THR           (0x00A8UL)
#define DCAM_AEM_GREEN_THR          (0x00ACUL)
#define DCAM_AEM_BASE_WADDR1        (0x0024UL)
#define DCAM_AEM_BASE_WADDR2        (0x0024UL)
#define DCAM_AEM_BASE_WADDR3        (0x0024UL)

#define DCAM_APB_SRAM_CTRL          (0x00B0UL)
#define DCAM_PDAF_EXTR_CTRL         (0x00B8UL)
#define DCAM_PDAF_SKIP_FRM          (0x00BCUL)
#define DCAM_PDAF_SKIP_FRM1         (0x00C0UL)
#define DCAM_PDAF_EXTR_ROI_ST       (0x00C4UL)
#define DCAM_PDAF_EXTR_ROI_SIZE     (0x00C8UL)
#define DCAM0_PDAF_EXTR_POS         (0x0C00UL)

#define DCAM_MIPI_REDUNDANT         (0x00D0UL)
#define DCAM_CAP_WORD_CNT           (0x00D4UL)
#define DCAM_CAP_FRM_CLR            (0x00D8UL)
#define DCAM_CAP_RAW_SIZE           (0x00DCUL)
#define DCAM_CAP_PDAF_SIZE          (0x00E0UL)
#define DCAM_CAP_VCH2_SIZE          (0x00E4UL)
#define DCAM_CAP_VCH3_SIZE          (0x00E8UL)

#define DCAM_IMAGE_CONTROL          (0x00F0UL)
#define DCAM_PDAF_CONTROL           (0x00F4UL)
#define DCAM_VC2_CONTROL            (0x00F8UL)

#define DCAM_MIPI_CAP_CFG           (0x0100UL)
#define DCAM_MIPI_CAP_FRM_CTRL      (0x0104UL)
#define DCAM_MIPI_CAP_START         (0x0108UL)
#define DCAM_MIPI_CAP_END           (0x010CUL)

#define ISP_BPC_MAP_ADDR            (0x0110UL)
#define ISP_BPC_OUT_ADDR            (0x0114UL)

#define ISP_NR3_WADDR               (0x0124UL)
#define NR3_FAST_ME_PARAM0          (0x01B4UL)
#define NR3_FAST_ME_PARAM           (0x01B8UL)
#define NR3_FAST_ME_ROI_PARAM0      (0x01BCUL)
#define NR3_FAST_ME_ROI_PARAM1      (0x01C0UL)
#define NR3_FAST_ME_OUT0            (0x01C4UL)
#define NR3_FAST_ME_OUT1            (0x01C8UL)

#define ISP_AFM_BASE_WADDR          (0x0118UL)
#define ISP_AFM_FRM_CTRL            (0x0180UL)
#define ISP_AFM_FRM_CTRL1           (0x0184UL)
#define ISP_AFM_PARAMETERS          (0x0188UL)
#define ISP_AFM_ENHANCE_CTRL        (0x018CUL)
#define ISP_AFM_DONE_TILE_NUM       (0x0190UL)
#define ISP_AFM_CROP_START          (0x02CCUL)
#define ISP_AFM_CROP_SIZE           (0x02D0UL)
#define ISP_AFM_WIN_RANGE0S         (0x02D4UL)
#define ISP_AFM_WIN_RANGE0E         (0x02D8UL)
#define ISP_AFM_WIN_RANGE1S         (0x02DCUL)
#define ISP_AFM_IIR_FILTER0         (0x02E0UL)
#define ISP_AFM_IIR_FILTER1         (0x02E4UL)
#define ISP_AFM_IIR_FILTER2         (0x02E8UL)
#define ISP_AFM_IIR_FILTER3         (0x02ECUL)
#define ISP_AFM_IIR_FILTER4         (0x02F0UL)
#define ISP_AFM_IIR_FILTER5         (0x02F4UL)
#define ISP_AFM_ENHANCE_FV0_THD     (0x02F8UL)
#define ISP_AFM_ENHANCE_FV1_THD     (0x02FCUL)
#define ISP_AFM_ENHANCE_FV1_COEFF00 (0x0300UL)
#define ISP_AFM_ENHANCE_FV1_COEFF01 (0x0304UL)
#define ISP_AFM_ENHANCE_FV1_COEFF10 (0x0308UL)
#define ISP_AFM_ENHANCE_FV1_COEFF11 (0x030CUL)
#define ISP_AFM_ENHANCE_FV1_COEFF20 (0x0310UL)
#define ISP_AFM_ENHANCE_FV1_COEFF21 (0x0314UL)
#define ISP_AFM_ENHANCE_FV1_COEFF30 (0x0318UL)
#define ISP_AFM_ENHANCE_FV1_COEFF31 (0x031CUL)

#define DCAM_CAM_BIN_CFG            (0x0020CUL)
#define DCAM_BIN_CROP_START         (0x00210UL)
#define DCAM_BIN_CROP_SIZE          (0x00214UL)
#define DCAM_CROP0_START            (0x0218UL)
#define DCAM_CROP0_X                (0x021CUL)
#define DCAM_RDS_DES_SIZE           (0x0220UL)

#define DCAM_BLC_PARA_R_B           (0x0224UL)
#define DCAM_BLC_PARA_G             (0x0228UL)

#define ISP_RGBG_PARAM              (0x022CUL)
#define ISP_RGBG_RB                 (0x0230UL)
#define ISP_RGBG_G                  (0x0234UL)
#define ISP_RGBG_YRANDOM_PARAMETER0 (0x0238UL)
#define ISP_RGBG_YRANDOM_PARAMETER1 (0x023CUL)
#define ISP_RGBG_YRANDOM_PARAMETER2 (0x0240UL)

#define ISP_AWBC_PARAM              (0x0244UL)
#define ISP_AWBC_GAIN0              (0x0248UL)
#define ISP_AWBC_GAIN1              (0x024CUL)
#define ISP_AWBC_THRD               (0x0250UL)
#define ISP_AWBC_OFFSET0            (0x0254UL)
#define ISP_AWBC_OFFSET1            (0x0258UL)

#define ISP_BPC_GC_CFG              (0x0158UL)
#define ISP_BPC_PARAM               (0x015CUL)
#define ISP_BPC_MAP_CTRL            (0x0160UL)
#define ISP_BPC_MAP_CTRL1           (0x0164UL)
#define ISP_BPC_BAD_PIXEL_TH0       (0x025CUL)
#define ISP_BPC_BAD_PIXEL_TH1       (0x0260UL)
#define ISP_BPC_BAD_PIXEL_TH2       (0x0264UL)
#define ISP_BPC_BAD_PIXEL_TH3       (0x0268UL)
#define ISP_BPC_FLAT_TH             (0x026CUL)
#define ISP_BPC_EDGE_RATIO          (0x0270UL)
#define ISP_BPC_BAD_PIXEL_PARAM     (0x0274UL)
#define ISP_BPC_BAD_PIXEL_COEFF     (0x0278UL)
#define ISP_BPC_LUTWORD0            (0x027CUL)
#define ISP_BPC_LUTWORD1            (0x0280UL)
#define ISP_BPC_LUTWORD2            (0x0284UL)
#define ISP_BPC_LUTWORD3            (0x0288UL)
#define ISP_BPC_LUTWORD4            (0x028CUL)
#define ISP_BPC_LUTWORD5            (0x0290UL)
#define ISP_BPC_LUTWORD6            (0x0294UL)
#define ISP_BPC_LUTWORD7            (0x0298UL)
#define ISP_BPC_MAP_ADDR            (0x0110UL)
#define ISP_BPC_OUT_ADDR            (0x0114UL)

#define DCAM_GRGB_CTRL              (0x0168UL)
#define DCAM_GRGB_CFG0              (0x016CUL)
#define DCAM_GRGB_LUM_FLAT_T        (0x029CUL)
#define DCAM_GRGB_LUM_FLAT_R        (0x02A0UL)
#define DCAM_GRGB_LUM_EDGE_T        (0x02A4UL)
#define DCAM_GRGB_LUM_EDGE_R        (0x02A8UL)
#define DCAM_GRGB_LUM_TEX_T         (0x02ACUL)
#define DCAM_GRGB_LUM_TEX_R         (0x02B0UL)
#define DCAM_GRGB_FREZ_FLAT_T       (0x02B4UL)
#define DCAM_GRGB_FREZ_FLAT_R       (0x02B8UL)
#define DCAM_GRGB_FREZ_EDGE_T       (0x02BCUL)
#define DCAM_GRGB_FREZ_EDGE_R       (0x02C0UL)
#define DCAM_GRGB_FREZ_TEX_T        (0x02C4UL)
#define DCAM_GRGB_FREZ_TEX_R        (0x02C8UL)

#define ISP_AFL_GLB_WADDR           (0x011CUL)
#define ISP_AFL_REGION_WADDR        (0X0120UL)
#define ISP_AFL_PARAM0              (0x01A0UL)
#define ISP_AFL_FRM_CTRL            (0x01A4UL)
#define ISP_AFL_CFG_READY           (0x01A8UL)
#define ISP_AFL_PARAM1              (0x0320UL)
#define ISP_AFL_PARAM2              (0x0324UL)
#define ISP_AFL_COL_POS             (0x0328UL)
#define ISP_AFL_REGION0             (0x032CUL)
#define ISP_AFL_REGION1             (0x0330UL)
#define ISP_AFL_REGION2             (0x0334UL)
#define ISP_AFL_SUM1                (0x0338UL)
#define ISP_AFL_SUM2                (0x033CUL)

struct dcam_control_field {
	uint32_t cap_frc_copy: 1;
	uint32_t cap_auto_copy: 1;
	uint32_t reserved: 2;
	uint32_t coeff_frc_copy: 1;
	uint32_t coeff_auto_copy: 1;
	uint32_t rds_frc_copy: 1;
	uint32_t rds_auto_copy: 1;

	uint32_t full_frc_copy: 1;
	uint32_t full_auto_copy: 1;
	uint32_t bin_frc_copy: 1;
	uint32_t bin_auto_copy: 1;
	uint32_t pdaf_frc_copy: 1;
	uint32_t pdaf_auto_copy: 1;
	uint32_t vch2_frc_copy: 1;
	uint32_t vch2_auto_copy: 1;

	uint32_t vch3_frc_copy: 1;
	uint32_t vch3_auto_copy: 1;
};

struct dcam_cfg_field {
	uint32_t cap_eb: 1;
	uint32_t full_path_eb : 1;
	uint32_t bin_path_eb: 1;
	uint32_t pdaf_path_eb: 1;
	uint32_t vch2_path_eb: 1;
	uint32_t vch3_path_eb: 1;
};

struct path_stop_field {
	uint32_t full_path_stop: 1;
	uint32_t bin_path_stop: 1;
};

struct full_cfg_field {
	uint32_t pack_bits: 1;
	uint32_t crop_eb: 1;
	uint32_t src_sel: 1;
};

struct bin_cfg_field {
	uint32_t pack_bits: 1;
	uint32_t bin_ratio: 1;
	uint32_t scaler_sel: 2;
	uint32_t reserved: 12;

	uint32_t slw_en: 1;
	uint32_t slw_addr_num: 3;
};

struct rds_des_field {
	uint32_t raw_downsizer_with: 13;
	uint32_t resersed0: 3;
	uint32_t raw_downsizer_height: 12;
};

struct endian_field {
	uint32_t reserved: 16;
	uint32_t full_endian: 2;
	uint32_t bin_endian: 2;
	uint32_t pdaf_endian: 2;
	uint32_t vch2_endian: 2;
	uint32_t vch3_endian: 2;
};

extern const unsigned long slowmotion_store_addr[3][4];

/* DCAM2 registers define, the other same as DCAM0 */
#define DCAM2_PATH1_BASE_WADDR      (0x0010UL)
#define DCAM2_PATH0_BASE_WADDR      (0x0020UL)
#define DCAM2_IMAGE_CONTROL         (0x0114UL)
#define DCAM2_MIPI_CAP_START        (0x010CUL)
#define DCAM2_MIPI_CAP_END          (0x0110UL)
/* DCAM2 registers define end */

/* DCAM AXIM registers define 1 */
#define AXIM_CTRL                   (0x0000UL)
#define AXIM_DBG_STS                (0x0004UL)
#define CAP_SENSOR_CTRL             (0x0008UL)
#define AXIM_WORD_ENDIAN            (0x000CUL)
#define MMU_CTRL                    (0x0010UL)
#define DCAM_SPARE_REG_0            (0x0014UL)
#define AXIM_SPARE_REG_0            (0x0018UL)
#define SPARE_REG_ICG               (0x001CUL)
#define IMG_FETCH_START             (0x0020UL)
#define IMG_FETCH_CTRL              (0x0024UL)
#define IMG_FETCH_SIZE              (0x0028UL)
#define IMG_FETCH_X                 (0x002CUL)
#define IMG_FETCH_RADDR             (0x0030UL)

#define DCAM_LBUF_SHARE_MODE        (0x0040UL)

/* DCAM AXIM registers define 2 */
#define MMU_EN                      (0x0000UL)
#define MMU_UPDATE                  (0x0004UL)
#define MMU_MIN_VPN                 (0x0008UL)
#define MMU_VPN_RANGE               (0x000CUL)
#define MMU_PT_ADDR                 (0x0010UL)
#define MMU_DEFAULT_PAGE            (0x0014UL)
#define MMU_VAOR_ADDR_RD            (0x0018UL)
#define MMU_VAOR_ADDR_WR            (0x001CUL)
#define MMU_INV_ADDR_RD             (0x0020UL)
#define MMU_INV_ADDR_WR             (0x0024UL)
#define MMU_UNS_ADDR_RD             (0x0028UL)
#define MMU_UNS_ADDR_WR             (0x002CUL)
#define MMU_MISS_CNT                (0x0030UL)
#define MMU_PT_UPDATE_QOS           (0x0034UL)
#define MMU_VERSION                 (0x0038UL)
#define MMU_MIN_PPN1                (0x003CUL)
#define MMU_PPN_RANGE1              (0x0040UL)
#define MMU_MIN_PPN2                (0x0044UL)
#define MMU_PPN_RANGE2              (0x0048UL)
#define MMU_VPN_PAOR_RD             (0x004CUL)
#define MMU_VPN_PAOR_WR             (0x0050UL)
#define MMU_PPN_PAOR_RD             (0x0054UL)
#define MMU_PPN_PAOR_WR             (0x0058UL)
#define MMU_REG_AU_MANAGE           (0x005CUL)
#define MMU_PAGE_RD_CH              (0x0060UL)
#define MMU_PAGE_WR_CH              (0x0064UL)
#define MMU_READ_PAGE_CMD_CNT       (0x0068UL)
#define MMU_READ_PAGE_LATENCY_CNT   (0x006CUL)
#define MMU_PAGE_MAX_LATENCY        (0x0070UL)
#define MMU_STS                     (0x0080UL)
#define MMU_EN_SHAD                 (0x0084UL)
#define MMU_MIN_VPN_SHAD            (0x0088UL)
#define MMU_VPN_RANGE_SHAD          (0x008CUL)
#define MMU_PT_ADDR_SHAD            (0x0090UL)
#define MMU_DEFAULT_PAGE_SHAD       (0x0094UL)
#define MMU_PT_UPDATE_QOS_SHAD      (0x0098UL)
#define MMU_MIN_PPN1_SHAD           (0x009CUL)
#define MMU_PPN_RANGE1_SHAD         (0x00A0UL)
#define MMU_MIN_PPN2_SHAD           (0x00A4UL)
#define MMU_PPN_RANGE2_SHAD         (0x00A8UL)

/* buffer addr map */
#define RDS_COEF_TABLE_START        (0x0340UL)
#define RDS_COEF_TABLE_SIZE         (0x00C0UL)

#define LSC_WEI_TABLE_START         (0x0400UL)
#define LSC_WEI_TABLE_MAX_NUM       (0x00FFUL)

#define PDAF_CORR_TABLE_START       (0x0E00UL)
#define PDAF_CORR_TABLE_SIZE        (0x0200UL)

#define LSC_GRID_BUF_START0         (0x4000UL)
#define LSC_GRID_BUF_START1         (0x8000UL)
#define LSC_GRID_BUF_SIZE           (0x4000UL)

#define DCAM_PATH_CROP_ALIGN 4

/*
 * DCAM register map range
 *
 * 0x0000 ~ 0x0fff(1K):                DCAM0
 *        |-------- 0x0000 ~ 0x03ff:   common config
 *        |-------- 0x0400 ~ 0x04bf:   rds coef table
 *        |-------- 0x0800 ~ 0x0bff:   lsc weight table
 *        |-------- 0x0e00 ~ 0x0fff:   pdaf corr table
 *
 * 0x1000 ~ 0x1fff(1K):                DCAM1
 *        |-------- 0x1000 ~ 0x13ff:   common config
 *        |-------- 0x1400 ~ 0x14bf:   rds coef table
 *        |-------- 0x1800 ~ 0x1bff:   lsc weight table
 *        |-------- 0x1e00 ~ 0x1fff:   pdaf corr table
 *
 * 0x2000 ~ 0x2fff(1K):                DCAM2
 *
 * 0x3000 ~ 0x3fff(1K):                AXIM
 *
 * 0x4000 ~ 0x7fff(4K):                DCAM0 lsc grid table
 *
 * 0x8000 ~ 0xbfff(4K):                DCAM1 lsc grid table
 *
 * 0xc000 ~ 0xcfff(1K):                MMU
 *
 */

#define DCAM_BASE(idx)                   (g_dcam_regbase[idx])
#define DCAM_AXIM_BASE                   (g_dcam_aximbase[0])
/* TODO: implement mmu */
#define DCAM_MMU_BASE                    (g_dcam_mmubase)
#define DCAM_PHYS_ADDR(idx)              (g_dcam_phys_base[idx])
#define DCAM_GET_REG(idx, reg)           (DCAM_PHYS_ADDR(idx) + (reg))

#define DCAM_REG_WR(idx, reg, val)       (REG_WR(DCAM_BASE(idx)+(reg), (val)))
#define DCAM_REG_RD(idx, reg)            (REG_RD(DCAM_BASE(idx)+(reg)))
#define DCAM_REG_MWR(idx, reg, msk, val) DCAM_REG_WR((idx), (reg), ((val) & (msk)) | (DCAM_REG_RD(idx, reg) & (~(msk))))

#define DCAM_AXIM_WR(reg, val)           (REG_WR(DCAM_AXIM_BASE+(reg), (val)))
#define DCAM_AXIM_RD(reg)                (REG_RD(DCAM_AXIM_BASE+(reg)))
#define DCAM_AXIM_MWR(reg, msk, val)     DCAM_AXIM_WR((reg), ((val) & (msk)) | (DCAM_AXIM_RD(reg) & (~(msk))))

#define DCAM_MMU_WR(reg, val)            (REG_WR(DCAM_MMU_BASE+(reg), (val)))
#define DCAM_MMU_RD(reg)                 (REG_RD(DCAM_MMU_BASE+(reg)))
#define DCAM_MMU_MWR(reg, msk, val)      DCAM_MMU_WR((reg), ((val) & (msk)) | (DCAM_MMU_RD(reg) & (~(msk))))

/*for dcam IT*/
#define DCAM_CHECK_LASE_STATUS           DCAM_RDS_DES_SIZE
/* TODO: add DCAM0/1 lsc grid table mapping */

#endif/* _DCAM_REG_H_ */
