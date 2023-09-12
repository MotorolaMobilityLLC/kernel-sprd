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

extern unsigned long g_dcam_regbase[];
extern unsigned long g_dcam_aximbase[];
extern unsigned long g_dcam_mmubase;
extern unsigned long g_dcam_phys_base[];

#define DCAM_PATH_CROP_ALIGN                  8

/* DCAM0/DCAM1 module registers define */
#define DCAM_CFG                              (0x0008UL)
#define DCAM_MIPI_CAP_FRM_CTRL                (0x0104UL)

#define DCAM_IP_REVISION                      (0x0000UL)
#define DCAM_CONTROL                          (0x0004UL)
#define DCAM_APB_SRAM_CTRL                    (0x0008UL)
#define DCAM_SPARE_CTRL                       (0x000CUL)
#define DCAM_MIPI_CAP_CFG                     (0x0010UL)
#define DCAM_MIPI_CAP_START                   (0x0014UL)
#define DCAM_MIPI_CAP_END                     (0x0018UL)
#define DCAM_IMAGE_CONTROL                    (0x001CUL)
#define DCAM_MIPI_REDUNDANT                   (0x0020UL)
#define DCAM_CAP_FRM_CLR                      (0x0024UL)
#define DCAM_CAP_RAW_SIZE                     (0x0028UL)
#define DCAM_INT_MASK                         (0x0030UL)
#define DCAM_INT_EN                           (0x0034UL)
#define DCAM_INT_CLR                          (0x0038UL)
#define DCAM_INT_RAW                          (0x003CUL)
#define DCAM_3A_INT_MASK                      (0x0040UL)
#define DCAM_3A_INT_EN                        (0x0044UL)
#define DCAM_3A_INT_CLR                       (0x0048UL)
#define DCAM_3A_INT_RAW                       (0x004CUL)
#define DCAM_PATH_OVF                         (0x0050UL)
#define DCAM_PATH_BUSY                        (0x0054UL)
#define DCAM_PATH_STOP                        (0x0058UL)
#define DCAM_PATH_ENDIAN                      (0x005CUL)
#define DCAM_FBC_PAYLOAD_WADDR                (0x0060UL)
#define DCAM_FBC_MID_WADDR                    (0x0064UL)
#define DCAM_FULL_BASE_WADDR                  (0x0068UL)
#define DCAM_BIN_BASE_WADDR0                  (0x0070UL)
#define DCAM_BIN_BASE_WADDR1                  (0x0074UL)
#define DCAM_BIN_BASE_WADDR2                  (0x0078UL)
#define DCAM_BIN_BASE_WADDR3                  (0x007CUL)
#define DCAM_FBC_CTRL                         (0x00F0UL)
#define DCAM_FBC_CTRL0                        (0x00F4UL)
#define DCAM_FBC_CTRL1                        (0x00F8UL)
#define DCAM_FULL_CFG                         (0x0100UL)
#define DCAM_FULL_CROP_START                  (0x0104UL)
#define DCAM_FULL_CROP_SIZE                   (0x0108UL)
#define DCAM_FULL_PATH_STATUS                 (0x010CUL)
#define DCAM_CAM_BIN_CFG                      (0x0120UL)
#define DCAM_CAM_BIN_CROP_START               (0x0124UL)
#define DCAM_CAM_BIN_CROP_SIZE                (0x0128UL)
#define DCAM_RDS_DES_SIZE                     (0x012CUL)
#define DCAM_RDS_SLICE_CTRL0                  (0x0130UL)
#define DCAM_RDS_SLICE_CTRL1                  (0x0134UL)
#define DCAM_RDS_SLICE_CTRL2                  (0x0138UL)
#define DCAM_RDS_SLICE_CTRL3                  (0x013CUL)
#define DCAM_BIN_PATH_STATUS                  (0x00140UL)
#define DCAM_BAYER_INFO_CFG                   (0x0180UL)
#define DCAM_CROP0_START                      (0x03C0UL)
#define DCAM_CROP0_SIZE                       (0x03C4UL)

#define DCAM_BLC_PARA_R_B                     (0x0184UL)
#define DCAM_BLC_PARA_G                       (0x0188UL)

#define ISP_RGBG_RB                           (0x01A0UL)
#define ISP_RGBG_G                            (0x01A4UL)
#define ISP_RGBG_YRANDOM_PARAMETER0           (0x01A8UL)
#define ISP_RGBG_YRANDOM_PARAMETER1           (0x01ACUL)
#define ISP_RGBG_YRANDOM_PARAMETER2           (0x01B0UL)
#define ISP_RGBG_YUV_YRANDOM_STATUS0          (0x01B4UL)
#define ISP_RGBG_YUV_YRANDOM_STATUS1          (0x01B8UL)
#define ISP_RGBG_YUV_YRANDOM_STATUS2          (0x01BCUL)

#define DCAM_AEM_BASE_WADDR                   (0x00A0UL)
#define DCAM_AEM_BASE_WADDR1                  (0x00A4UL)
#define DCAM_AEM_BASE_WADDR2                  (0x00A8UL)
#define DCAM_AEM_BASE_WADDR3                  (0x00ACUL)
#define DCAM_AEM_FRM_CTRL0                    (0x0300UL)
#define DCAM_AEM_FRM_CTRL1                    (0x0304UL)
#define DCAM_AEM_OFFSET                       (0x0308UL)
#define DCAM_AEM_BLK_NUM                      (0x030CUL)
#define DCAM_AEM_BLK_SIZE                     (0x0310UL)
#define DCAM_AEM_RED_THR                      (0x0314UL)
#define DCAM_AEM_BLUE_THR                     (0x0318UL)
#define DCAM_AEM_GREEN_THR                    (0x031CUL)
#define DCAM_AEM_STATUS                       (0x0320UL)

#define DCAM_HIST_BASE_WADDR                  (0x00C0UL)
#define DCAM_HIST_BASE_WADDR1                 (0x00C4UL)
#define DCAM_HIST_BASE_WADDR2                 (0x00C8UL)
#define DCAM_HIST_BASE_WADDR3                 (0x00CCUL)
#define DCAM_HIST_FRM_CTRL0                   (0x0340UL)
#define DCAM_HIST_FRM_CTRL1                   (0x0344UL)
#define DCAM_BAYER_HIST_START                 (0x0348UL)
#define DCAM_BAYER_HIST_END                   (0x034CUL)
#define DCAM_HIST_STATUS                      (0x0350UL)

#define ISP_AWBC_GAIN0                        (0x03D0UL)
#define ISP_AWBC_GAIN1                        (0x03d4UL)
#define ISP_AWBC_THRD                         (0x03d8UL)
#define ISP_AWBC_THRD1                        (0x03dcUL)
#define ISP_AWBC_OFFSET0                      (0x03e0UL)
#define ISP_AWBC_OFFSET1                      (0x03e4UL)
#define ISP_AWBC_STATUS                       (0x03E8UL)

#define ISP_AFM_BASE_WADDR                    (0x00E0UL)
#define ISP_AFM_FRM_CTRL                      (0x0480UL)
#define ISP_AFM_FRM_CTRL1                     (0x0484UL)
#define ISP_AFM_PARAMETERS                    (0x0488UL)
#define ISP_AFM_ENHANCE_CTRL                  (0x048CUL)
#define ISP_AFM_CROP_START                    (0x0490UL)
#define ISP_AFM_CROP_SIZE                     (0x0494UL)
#define ISP_AFM_WIN_RANGE0S                   (0x0498UL)
#define ISP_AFM_WIN_RANGE0E                   (0x049CUL)
#define ISP_AFM_WIN_RANGE1S                   (0x04A0UL)
#define ISP_AFM_IIR_FILTER0                   (0x04A4UL)
#define ISP_AFM_IIR_FILTER1                   (0x04A8UL)
#define ISP_AFM_IIR_FILTER2                   (0x04ACUL)
#define ISP_AFM_IIR_FILTER3                   (0x04B0UL)
#define ISP_AFM_IIR_FILTER4                   (0x04B4UL)
#define ISP_AFM_IIR_FILTER5                   (0x04B8UL)
#define ISP_AFM_ENHANCE_FV0_THD               (0x04BCUL)
#define ISP_AFM_ENHANCE_FV1_THD               (0x04C0UL)
#define ISP_AFM_ENHANCE_FV1_COEFF00           (0x04C4UL)
#define ISP_AFM_ENHANCE_FV1_COEFF01           (0x04C8UL)
#define ISP_AFM_ENHANCE_FV1_COEFF10           (0x04CCUL)
#define ISP_AFM_ENHANCE_FV1_COEFF11           (0x04D0UL)
#define ISP_AFM_ENHANCE_FV1_COEFF20           (0x04D4UL)
#define ISP_AFM_ENHANCE_FV1_COEFF21           (0x04D8UL)
#define ISP_AFM_ENHANCE_FV1_COEFF30           (0x04DCUL)
#define ISP_AFM_ENHANCE_FV1_COEFF31           (0x04E0UL)
#define ISP_AFM_TILE_CNT_OUT                  (0x04E4UL)
#define ISP_AFM_CNT_OUT                       (0x04E8UL)
#define ISP_AFM_STATUS0                       (0x04ECUL)
#define ISP_AFM_STATUS1                       (0x04F0UL)

#define DCAM_PPE_RIGHT_WADDR                  (0x008CUL)
#define DCAM_PDAF_BASE_WADDR                  (0x0080UL)
#define DCAM_PDAF_CONTROL                     (0x0200UL)
#define DCAM_PPE_FRM_CTRL0                    (0x0204UL)
#define DCAM_PPE_FRM_CTRL1                    (0x0208UL)
#define DCAM_PPE_STATUS                       (0x020CUL)
#define DCAM_PDAF_PATH_STATUS                 (0x0210UL)
#define ISP_PPI_PARAM                         (0x0214UL)
#define ISP_PPI_BLOCK_COL                     (0x0218UL)
#define ISP_PPI_BLOCK_ROW                     (0x021CUL)
#define ISP_PPI_GLB_START                     (0x0220UL)
#define ISP_PPI_AF_WIN_START                  (0x0230UL)
#define ISP_PPI_AF_WIN_END                    (0x0234UL)
#define ISP_PPI_PATTERN01                     (0x0238UL)
#define ISP_PPI_PATTERN02                     (0x023CUL)
#define ISP_PPI_PATTERN03                     (0x0240UL)
#define ISP_PPI_PATTERN04                     (0x0244UL)
#define ISP_PPI_PATTERN05                     (0x0248UL)
#define ISP_PPI_PATTERN06                     (0x024CUL)
#define ISP_PPI_PATTERN07                     (0x0250UL)
#define ISP_PPI_PATTERN08                     (0x0254UL)
#define ISP_PPI_PATTERN09                     (0x0258UL)
#define ISP_PPI_PATTERN10                     (0x025CUL)
#define ISP_PPI_PATTERN11                     (0x0260UL)
#define ISP_PPI_PATTERN12                     (0x0264UL)
#define ISP_PPI_PATTERN13                     (0x0268UL)
#define ISP_PPI_PATTERN14                     (0x026CUL)
#define ISP_PPI_PATTERN15                     (0x0270UL)
#define ISP_PPI_PATTERN16                     (0x0274UL)
#define ISP_PPI_PATTERN17                     (0x0278UL)
#define ISP_PPI_PATTERN18                     (0x027CUL)
#define ISP_PPI_PATTERN19                     (0x0280UL)
#define ISP_PPI_PATTERN20                     (0x0284UL)
#define ISP_PPI_PATTERN21                     (0x0288UL)
#define ISP_PPI_PATTERN22                     (0x028CUL)
#define ISP_PPI_PATTERN23                     (0x0290UL)
#define ISP_PPI_PATTERN24                     (0x0294UL)
#define ISP_PPI_PATTERN25                     (0x0298UL)
#define ISP_PPI_PATTERN26                     (0x029CUL)
#define ISP_PPI_PATTERN27                     (0x02A0UL)
#define ISP_PPI_PATTERN28                     (0x02A4UL)
#define ISP_PPI_PATTERN29                     (0x02A8UL)
#define ISP_PPI_PATTERN30                     (0x02ACUL)
#define ISP_PPI_PATTERN31                     (0x02B0UL)
#define ISP_PPI_PATTERN32                     (0x02B4UL)

#define DCAM_VCH2_BASE_WADDR                  (0x0084UL)
#define DCAM_VCH3_BASE_WADDR                  (0x0088UL)
#define DCAM_VC2_CONTROL                      (0x0160UL)
#define DCAM_VC3_CONTROL                      (0x0170UL)
#define DCAM_VCH2_PATH_STATUS                 (0x0164UL)
#define DCAM_VCH3_PATH_STATUS                 (0x0174UL)

#define ISP_AFL_GLB_WADDR                     (0x00D0UL)
#define ISP_AFL_REGION_WADDR                  (0x00D4UL)
#define ISP_AFL_FRM_CTRL0                     (0x0360UL)
#define ISP_AFL_FRM_CTRL1                     (0x0364UL)
#define ISP_AFL_PARAM0                        (0x0368UL)
#define ISP_AFL_PARAM1                        (0x036cUL)
#define ISP_AFL_PARAM2                        (0x0370UL)
#define ISP_AFL_COL_POS                       (0x0374UL)
#define ISP_AFL_REGION0                       (0x0378UL)
#define ISP_AFL_REGION1                       (0x037cUL)
#define ISP_AFL_REGION2                       (0x0380UL)
#define ISP_AFL_SUM1                          (0x0384UL)
#define ISP_AFL_SUM2                          (0x0388UL)
#define ISP_AFL_STATUS0                       (0x038cUL)
#define ISP_AFL_STATUS1                       (0x0390UL)
#define ISP_AFL_STATUS2                       (0x0394UL)
#define ISP_AFL_SLICE1                        (0x0398UL)
#define ISP_AFL_SLICE2                        (0x039CUL)
#define ISP_AFL_SLICE3                        (0x03A0UL)
#define ISP_AFL_SLICE4                        (0x03A4UL)
#define ISP_AFL_ADDR_STATUS                   (0x03A8UL)

#define DCAM_LENS_BASE_RADDR                  (0x0090UL)
#define DCAM_LENS_GRID_NUMBER                 (0x02E0UL)
#define DCAM_LENS_GRID_SIZE                   (0x02E4UL)
#define DCAM_LENS_LOAD_ENABLE                 (0x02E8UL)
#define DCAM_LENS_LOAD_CLR                    (0x02ECUL)
#define DCAM_LENS_SLICE_CTRL0                 (0x02F0UL)
#define DCAM_LENS_SLICE_CTRL1                 (0x02F4UL)
#define DCAM_LENS_STATUS                      (0x02F8UL)
#define DCAM_LSC_WEI_LAST0                    (0x0D00UL)
#define DCAM_LSC_WEI_LAST1                    (0x0D04UL)

#define DCAM_LSCM_BASE_WADDR                  (0x0094UL)
#define DCAM_LSCM_FRM_CTRL0                   (0x02C0UL)
#define DCAM_LSCM_FRM_CTRL1                   (0x02C4UL)
#define DCAM_LSCM_OFFSET                      (0x02C8UL)
#define DCAM_LSCM_BLK_NUM                     (0x02CCUL)
#define DCAM_LSCM_BLK_SIZE                    (0x02D0UL)

#define ISP_BPC_MAP_ADDR                      (0x00D8UL)
#define ISP_BPC_OUT_ADDR                      (0x00DCUL)
#define ISP_BPC_PARAM                         (0x0400UL)
#define ISP_BPC_MAP_CTRL                      (0x0404UL)
#define ISP_BPC_BAD_PIXEL_TH0                 (0x0408UL)
#define ISP_BPC_BAD_PIXEL_TH1                 (0x040CUL)
#define ISP_BPC_BAD_PIXEL_TH2                 (0x0410UL)
#define ISP_BPC_BAD_PIXEL_TH3                 (0x0414UL)
#define ISP_BPC_FLAT_TH                       (0x0418UL)
#define ISP_BPC_EDGE_RATIO                    (0x041CUL)
#define ISP_BPC_BAD_PIXEL_PARAM               (0x0420UL)
#define ISP_BPC_BAD_PIXEL_COEFF               (0x0424UL)
#define ISP_BPC_LUTWORD0                      (0x0428UL)
#define ISP_BPC_LUTWORD1                      (0x042CUL)
#define ISP_BPC_LUTWORD2                      (0x0430UL)
#define ISP_BPC_LUTWORD3                      (0x0434UL)
#define ISP_BPC_LUTWORD4                      (0x0438UL)
#define ISP_BPC_LUTWORD5                      (0x043CUL)
#define ISP_BPC_LUTWORD6                      (0x0440UL)
#define ISP_BPC_LUTWORD7                      (0x0444UL)
#define ISP_BPC_PPI_RANG                      (0x0448UL)
#define ISP_BPC_PPI_RANG1                     (0x044CUL)
#define ISP_BPC_PPI_GLB_START                 (0x0450UL)
#define ISP_BPC_SLICE_ROI_S                   (0x0454UL)
#define ISP_BPC_SLICE_ROI_R                   (0x0458UL)
#define ISP_BPC_LAST_ADDR                     (0x045CUL)
#define ISP_BPC_STATUS0                       (0x0460UL)
#define ISP_BPC_STATUS1                       (0x0464UL)

#define ISP_NR3_WADDR                         (0x00E4UL)
#define NR3_FAST_ME_PARAM                     (0x0500UL)
#define NR3_FAST_ME_ROI_PARAM0                (0x0504UL)
#define NR3_FAST_ME_ROI_PARAM1                (0x0508UL)
#define NR3_FAST_ME_OUT0                      (0x050CUL)
#define NR3_FAST_ME_OUT1                      (0x0510UL)
#define NR3_FAST_ME_STATUS                    (0x0514UL)

#define DCAM_GTM_GLB_CTRL                     (0x0520UL)
#define GTM_HIST_CTRL0                        (0x0524UL)
#define GTM_HIST_CTRL1                        (0x0528UL)
#define GTM_HIST_YMIN                         (0x052CUL)
#define GTM_HIST_CTRL2                        (0x0530UL)
#define GTM_HIST_CTRL3                        (0x0534UL)
#define GTM_HIST_CTRL4                        (0x0538UL)
#define GTM_HIST_CTRL5                        (0x053CUL)
#define GTM_HIST_CTRL6                        (0x0540UL)
#define GTM_HIST_CTRL7                        (0x0544UL)
#define GTM_LOG_DIFF                          (0x0548UL)
#define GTM_TM_YMIN_SMOOTH                    (0x054CUL)
#define GTM_TM_LUMAFILTER0                    (0x0550UL)
#define GTM_TM_LUMAFILTER1                    (0x0554UL)
#define GTM_TM_LUMAFILTER2                    (0x0558UL)
#define GTM_TM_RGB2YCOEFF0                    (0x055CUL)
#define GTM_TM_RGB2YCOEFF1                    (0x0560UL)
#define GTM_SLICE_LINE_STARTPOS               (0x0584UL)
#define GTM_SLICE_LINE_ENDPOS                 (0x0588UL)
#define GTM_STATUS                            (0x0590UL)

#define DCAM_PATH_STOP_MASK                   (0x2DFFUL)
#define DCAM_PATH_BUSY_MASK                   (0x2FFFUL)

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
#define DCAM2_PATH0_BASE_WADDR                (0x0080UL)
#define DCAM2_PATH1_BASE_WADDR                (0x0084UL)
/* DCAM2 registers define end */

/* DCAM AXIM registers define 1 */
#define AXIM_CTRL                             (0x0000UL)
#define AXIM_DBG_STS                          (0x0004UL)
#define CAP_SENSOR_CTRL                       (0x0008UL)
#define AXIM_WORD_ENDIAN                      (0x000CUL)
#define DCAM_LBUF_SHARE_MODE                  (0x0010UL)
#define DCAM_SPARE_REG_0                      (0x0014UL)
#define AXIM_SPARE_REG_0                      (0x0018UL)
#define SPARE_REG_ICG                         (0x001CUL)
#define IMG_FETCH_CTRL                        (0x0020UL)
#define IMG_FETCH_SIZE                        (0x0024UL)
#define IMG_FETCH_X                           (0x0028UL)
#define IMG_FETCH_START                       (0x002CUL)
#define IMG_FETCH_RADDR                       (0x0030UL)
#define DCAM_PORT_CFG                         (0x0040UL)
#define ZZHDR_SHADOW_CTRL                     (0x00FCUL)
#define HDR_GLB_CTRL                          (0x0100UL)
#define HDR_EXPO_RATIO                        (0x0104UL)
#define HDR_INTERP_CTRL0                      (0x0108UL)
#define HDR_INTERP_CTRL1                      (0x010CUL)
#define HDR_INTERP_CTRL2                      (0x0110UL)
#define HDR_INTERP_CTRL3                      (0x0114UL)
#define HDR_INTERP_CTRL5                      (0x011CUL)
#define HDR_INTERP_CTRL6                      (0x0120UL)
#define HDR_INTERP_CTRL7                      (0x0124UL)
#define HDR_INTERP_CTRL8                      (0x0128UL)
#define HDR_INTERP_CTRL9                      (0x012CUL)
#define HDR_INTERP_CTRL10                     (0x0130UL)
#define HDR_INTERP_CTRL11                     (0x0134UL)
#define HDR_GETESTACT_CTRL                    (0x0138UL)
#define HDR_GETESTMOT_CTRL                    (0x013CUL)
#define HDR_T2SMOOTHING_CTRL0                 (0x0140UL)
#define HDR_T2SMOOTHING_CTRL1                 (0x0144UL)
#define HDR_T2SMOOTHING_CTRL2                 (0x0148UL)
#define HDR_REC_CTRL                          (0x014CUL)
#define HDR_HIST_CTRL0                        (0x0150UL)
#define HDR_HIST_CTRL1                        (0x0154UL)
#define HDR_HIST_CTRL2                        (0x0158UL)
#define HDR_HIST_CTRL3                        (0x015CUL)
#define HDR_HIST_CTRL4                        (0x0160UL)
#define HDR_HIST_CTRL5                        (0x0164UL)
#define HDR_HIST_CTRL6                        (0x0168UL)
#define HDR_HIST_CTRL7                        (0x016CUL)
#define HDR_LOG_DIFF                          (0x0170UL)
#define HDR_TM_YMIN_SMOOTH                    (0x0174UL)
#define HDR_TM_LUMAFILTER0                    (0x0178UL)
#define HDR_TM_LUMAFILTER1                    (0x017CUL)
#define HDR_HIST_XPTS_0                       (0x0180UL)
#define HDR_HIST_XPTS_2                       (0x0184UL)
#define HDR_HIST_XPTS_4                       (0x0188UL)
#define HDR_HIST_XPTS_6                       (0x018CUL)
#define HDR_HIST_XPTS_8                       (0x0190UL)
#define HDR_HIST_XPTS_10                      (0x0194UL)
#define HDR_HIST_XPTS_12                      (0x0198UL)
#define HDR_HIST_XPTS_14                      (0x019CUL)
#define HDR_HIST_XPTS_16                      (0x01A0UL)
#define HDR_HIST_XPTS_18                      (0x01A4UL)
#define HDR_HIST_XPTS_20                      (0x01A8UL)
#define HDR_HIST_XPTS_22                      (0x01ACUL)
#define HDR_HIST_XPTS_24                      (0x01B0UL)
#define HDR_HIST_XPTS_26                      (0x01B4UL)
#define HDR_HIST_XPTS_28                      (0x01B8UL)
#define HDR_HIST_XPTS_30                      (0x01BCUL)
#define HDR_HIST_XPTS_32                      (0x01C0UL)
#define HDR_HIST_XPTS_34                      (0x01C4UL)
#define HDR_HIST_XPTS_36                      (0x01C8UL)
#define HDR_HIST_XPTS_38                      (0x01CCUL)
#define HDR_HIST_XPTS_40                      (0x01D0UL)
#define HDR_HIST_XPTS_42                      (0x01D4UL)
#define HDR_HIST_XPTS_44                      (0x01D8UL)
#define HDR_HIST_XPTS_46                      (0x01DCUL)
#define HDR_HIST_XPTS_48                      (0x01E0UL)
#define HDR_HIST_XPTS_50                      (0x01E4UL)
#define HDR_HIST_XPTS_52                      (0x01E8UL)
#define HDR_HIST_XPTS_54                      (0x01ECUL)
#define HDR_HIST_XPTS_56                      (0x01F0UL)
#define HDR_HIST_XPTS_58                      (0x01F4UL)
#define HDR_HIST_XPTS_60                      (0x01F8UL)
#define HDR_HIST_XPTS_62                      (0x01FCUL)
#define HDR_HIST_XPTS_64                      (0x0200UL)
#define HDR_HIST_XPTS_66                      (0x0204UL)
#define HDR_HIST_XPTS_68                      (0x0208UL)
#define HDR_HIST_XPTS_70                      (0x020CUL)
#define HDR_HIST_XPTS_72                      (0x0210UL)
#define HDR_HIST_XPTS_74                      (0x0214UL)
#define HDR_HIST_XPTS_76                      (0x0218UL)
#define HDR_HIST_XPTS_78                      (0x021CUL)
#define HDR_HIST_XPTS_80                      (0x0220UL)
#define HDR_HIST_XPTS_82                      (0x0224UL)
#define HDR_HIST_XPTS_84                      (0x0228UL)
#define HDR_HIST_XPTS_86                      (0x022CUL)
#define HDR_HIST_XPTS_88                      (0x0230UL)
#define HDR_HIST_XPTS_90                      (0x0234UL)
#define HDR_HIST_XPTS_92                      (0x0238UL)
#define HDR_HIST_XPTS_94                      (0x023CUL)
#define HDR_HIST_XPTS_96                      (0x0240UL)
#define HDR_HIST_XPTS_98                      (0x0244UL)
#define HDR_HIST_XPTS_100                     (0x0248UL)
#define HDR_HIST_XPTS_102                     (0x024CUL)
#define HDR_HIST_XPTS_104                     (0x0250UL)
#define HDR_HIST_XPTS_106                     (0x0254UL)
#define HDR_HIST_XPTS_108                     (0x0258UL)
#define HDR_HIST_XPTS_110                     (0x025CUL)
#define HDR_HIST_XPTS_112                     (0x0260UL)
#define HDR_HIST_XPTS_114                     (0x0264UL)
#define HDR_HIST_XPTS_116                     (0x0268UL)
#define HDR_HIST_XPTS_118                     (0x026CUL)
#define HDR_HIST_XPTS_120                     (0x0270UL)
#define HDR_HIST_XPTS_122                     (0x0274UL)
#define HDR_HIST_XPTS_124                     (0x0278UL)
#define HDR_HIST_XPTS_126                     (0x027CUL)

/* DCAM AXIM registers define 2 */
#define MMU_EN                                (0x0000UL)
#define MMU_UPDATE                            (0x0004UL)
#define MMU_MIN_VPN                           (0x0008UL)
#define MMU_VPN_RANGE                         (0x000CUL)
#define MMU_PT_ADDR                           (0x0010UL)
#define MMU_DEFAULT_PAGE                      (0x0014UL)
#define MMU_VAOR_ADDR_RD                      (0x0018UL)
#define MMU_VAOR_ADDR_WR                      (0x001CUL)
#define MMU_INV_ADDR_RD                       (0x0020UL)
#define MMU_INV_ADDR_WR                       (0x0024UL)
#define MMU_UNS_ADDR_RD                       (0x0028UL)
#define MMU_UNS_ADDR_WR                       (0x002CUL)
#define MMU_MISS_CNT                          (0x0030UL)
#define MMU_PT_UPDATE_QOS                     (0x0034UL)
#define MMU_VERSION                           (0x0038UL)
#define MMU_MIN_PPN1                          (0x003CUL)
#define MMU_PPN_RANGE1                        (0x0040UL)
#define MMU_MIN_PPN2                          (0x0044UL)
#define MMU_PPN_RANGE2                        (0x0048UL)
#define MMU_VPN_PAOR_RD                       (0x004CUL)
#define MMU_VPN_PAOR_WR                       (0x0050UL)
#define MMU_PPN_PAOR_RD                       (0x0054UL)
#define MMU_PPN_PAOR_WR                       (0x0058UL)
#define MMU_REG_AU_MANAGE                     (0x005CUL)
#define MMU_PAGE_RD_CH                        (0x0060UL)
#define MMU_PAGE_WR_CH                        (0x0064UL)
#define MMU_READ_PAGE_CMD_CNT                 (0x0068UL)
#define MMU_READ_PAGE_LATENCY_CNT             (0x006CUL)
#define MMU_PAGE_MAX_LATENCY                  (0x0070UL)
#define MMU_STS                               (0x0080UL)
#define MMU_EN_SHAD                           (0x0084UL)
#define MMU_MIN_VPN_SHAD                      (0x0088UL)
#define MMU_VPN_RANGE_SHAD                    (0x008CUL)
#define MMU_PT_ADDR_SHAD                      (0x0090UL)
#define MMU_DEFAULT_PAGE_SHAD                 (0x0094UL)
#define MMU_PT_UPDATE_QOS_SHAD                (0x0098UL)
#define MMU_MIN_PPN1_SHAD                     (0x009CUL)
#define MMU_PPN_RANGE1_SHAD                   (0x00A0UL)
#define MMU_MIN_PPN2_SHAD                     (0x00A4UL)
#define MMU_PPN_RANGE2_SHAD                   (0x00A8UL)

/* buffer addr map */
#define GTM_HIST_XPTS                         (0x0600UL)
#define GTM_HIST_XPTS_CNT                     (0x0100UL)
#define GTM_HIST_CNT                          (0x0700UL)

#define LSC_WEI_TABLE_START                   (0x0900UL)
#define LSC_WEI_TABLE_SIZE                    (0x0400UL)
#define LSC_WEI_TABLE_MAX_NUM                 (0x00FFUL)

#define RDS_COEF_TABLE_START                  (0x0d40UL)
#define RDS_COEF_TABLE_SIZE                   (0x00C0UL)

#define PDAF_CORR_TABLE_START                 (0x0E00UL)
#define PDAF_CORR_TABLE_SIZE                  (0x0200UL)

#define LSC_GRID_BUF_START0                   (0x4000UL)
#define LSC_GRID_BUF_START1                   (0x8000UL)
#define LSC_GRID_BUF_SIZE                     (0x4000UL)

#define LSC_GRID_BUF_START2                   (0xc000UL)
#define LSC_GRID_BUF_SIZE2                    (0x3000UL)

/*
 * DCAM register map range of sharkl5pro
 *
 * 0x0000 ~ 0x0fff(1K):                DCAM0
 *        |-------- 0x0000 ~ 0x05ff:   common config
 *        |-------- 0x0600 ~ 0x06ff:   gtm hist xpts
 *        |-------- 0x0700 ~ 0x08ff:   gtm hist cnt
 *        |-------- 0x0900 ~ 0x0cff:   lsc weight table
 *        |-------- 0x0d40 ~ 0x0dff:   rds coef table
 *        |-------- 0x0e00 ~ 0x0fff:   pdaf corr tables
 *
 * 0x1000 ~ 0x1fff(1K):                DCAM1
 *        |-------- 0x1000 ~ 0x15ff:   common config
 *        |-------- 0x1600 ~ 0x16ff:   gtm hist xpts
 *        |-------- 0x1700 ~ 0x18ff:   gtm hist cnt
 *        |-------- 0x1900 ~ 0x1cff:   lsc weight table
 *        |-------- 0x1d40 ~ 0x1dff:   rds coef table
 *        |-------- 0x1e00 ~ 0x1fff:   pdaf corr tables
 *
 * 0x2000 ~ 0x2fff(1K):                DCAM2
 *        |-------- 0x2000 ~ 0x25ff:   common config
 *        |-------- 0x2600 ~ 0x26ff:   gtm hist xpts
 *        |-------- 0x2700 ~ 0x28ff:   gtm hist cnt
 *        |-------- 0x2900 ~ 0x2cff:   lsc weight table
 *        |-------- 0x2d40 ~ 0x2dff:   rds coef table
 *        |-------- 0x2e00 ~ 0x2fff:   pdaf corr tables
 *
 * 0x3000 ~ 0x3fff(1K):                AXIM
 *
 * 0x4000 ~ 0x7fff(4K):                DCAM0 lsc grid table
 *
 * 0x8000 ~ 0xbfff(4K):                DCAM1 lsc grid table
 *
 * 0xc000 ~ 0xefff(3K):                DCAM2 lsc grid table
 *
 */

#define DCAM_BASE(idx)                        (g_dcam_regbase[idx])
#define DCAM_AXIM_BASE                        (g_dcam_aximbase[0])
/* TODO: implement mmu */
#define DCAM_MMU_BASE                         (g_dcam_mmubase)
#define DCAM_PHYS_ADDR(idx)                   (g_dcam_phys_base[idx])
#define DCAM_GET_REG(idx, reg)                (DCAM_PHYS_ADDR(idx) + (reg))

#define DCAM_REG_WR(idx, reg, val)            (REG_WR(DCAM_BASE(idx)+(reg), (val)))
#define DCAM_REG_RD(idx, reg)                 (REG_RD(DCAM_BASE(idx)+(reg)))
#define DCAM_REG_MWR(idx, reg, msk, val)      DCAM_REG_WR((idx), (reg), ((val) & (msk)) | (DCAM_REG_RD(idx, reg) & (~(msk))))

#define DCAM_AXIM_WR(reg, val)                (REG_WR(DCAM_AXIM_BASE+(reg), (val)))
#define DCAM_AXIM_RD(reg)                     (REG_RD(DCAM_AXIM_BASE+(reg)))
#define DCAM_AXIM_MWR(reg, msk, val)          DCAM_AXIM_WR((reg), ((val) & (msk)) | (DCAM_AXIM_RD(reg) & (~(msk))))

#define DCAM_MMU_WR(reg, val)                 (REG_WR(DCAM_MMU_BASE+(reg), (val)))
#define DCAM_MMU_RD(reg)                      (REG_RD(DCAM_MMU_BASE+(reg)))
#define DCAM_MMU_MWR(reg, msk, val)           DCAM_MMU_WR((reg), ((val) & (msk)) | (DCAM_MMU_RD(reg) & (~(msk))))

/*for dcam IT*/
#define DCAM_CHECK_LASE_STATUS                DCAM_RDS_DES_SIZE
/* TODO: add DCAM0/1 lsc grid table mapping */

#endif /* _DCAM_REG_H_ */
