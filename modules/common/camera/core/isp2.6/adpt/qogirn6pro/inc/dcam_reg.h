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
extern unsigned long g_dcam_fmcubase;
extern unsigned long g_dcam_phys_base[];

#define DCAM_PATH_CROP_ALIGN                            8

extern unsigned long g_reg_wr_flag;
extern spinlock_t g_reg_wr_lock;

/* DCAM0/DCAM1 module registers define */
#define DCAM_BASE_ADDR                                  (0x3e000000)
#define DCAM_COMMON_BASE                                (0x0000UL)

#define DCAM_IP_REVISION                                (0x0000UL)
#define DCAM_COMMOM_STATUS0                             (DCAM_COMMON_BASE + 0x0004UL)
#define DCAM_COMMOM_STATUS1                             (DCAM_COMMON_BASE + 0x0008UL)
#define DCAM_COMMOM_STATUS2                             (DCAM_COMMON_BASE + 0x000CUL)
#define DCAM_CONTROL                                    (DCAM_COMMON_BASE + 0x002CUL)

#define DCAM_INT0_MASK                                  (DCAM_COMMON_BASE + 0x0030UL)
#define DCAM_INT0_EN                                    (DCAM_COMMON_BASE + 0x0034UL)
#define DCAM_INT0_CLR                                   (DCAM_COMMON_BASE + 0x0038UL)
#define DCAM_INT0_RAW                                   (DCAM_COMMON_BASE + 0x003CUL)
#define DCAM_INT1_MASK                                  (DCAM_COMMON_BASE + 0x0040UL)
#define DCAM_INT1_EN                                    (DCAM_COMMON_BASE + 0x0044UL)
#define DCAM_INT1_CLR                                   (DCAM_COMMON_BASE + 0x0048UL)
#define DCAM_INT1_RAW                                   (DCAM_COMMON_BASE + 0x004CUL)
#define DCAM_3A_INT_MASK                                (DCAM_COMMON_BASE + 0x0050UL)
#define DCAM_3A_INT_EN                                  (DCAM_COMMON_BASE + 0x0054UL)
#define DCAM_3A_INT_CLR                                 (DCAM_COMMON_BASE + 0x0058UL)
#define DCAM_3A_INT_RAW                                 (DCAM_COMMON_BASE + 0x005CUL)
#define DCAM_PATH_OVF                                   (DCAM_COMMON_BASE + 0x0060UL)
#define DCAM_PATH_BUSY                                  (DCAM_COMMON_BASE + 0x0064UL)
#define DCAM_PATH_STOP                                  (DCAM_COMMON_BASE + 0x0068UL)
#define DCAM_APB_SRAM_CTRL                              (DCAM_COMMON_BASE + 0x006CUL)
#define DCAM_SPARE_CTRL                                 (DCAM_COMMON_BASE + 0x0070UL)
#define DCAM_GCLK_CTRL0                                 (DCAM_COMMON_BASE + 0x0074UL)
#define DCAM_GCLK_CTRL1                                 (DCAM_COMMON_BASE + 0x0078UL)
#define DCAM_GCLK_CTRL2                                 (DCAM_COMMON_BASE + 0x007CUL)
#define DCAM_BUF_CTRL                                   (DCAM_COMMON_BASE + 0x0080UL)
#define DCAM_BUF_SEL_OUT                                (DCAM_COMMON_BASE + 0x0084UL)
#define DCAM_IMG_SIZE                                   (DCAM_COMMON_BASE + 0x0088UL)
#define DCAM_FLUSH_CTRL                                 (DCAM_COMMON_BASE + 0x008CUL)
#define DCAM_PATH_SEL                                   (DCAM_COMMON_BASE + 0x0090UL)
#define DCAM_GTM_STATUS0                                (DCAM_COMMON_BASE + 0x00A8UL)
#define DCAM_GTM_STATUS1                                (DCAM_COMMON_BASE + 0x00ACUL)
#define DCAM_GTM_STATUS2                                (DCAM_COMMON_BASE + 0x00B0UL)
#define DCAM_GTM_STATUS3                                (DCAM_COMMON_BASE + 0x00B4UL)
#define DCAM_GTM_STATUS4                                (DCAM_COMMON_BASE + 0x00B8UL)
#define DCAM_GTM_STATUS5                                (DCAM_COMMON_BASE + 0x00BCUL)
#define DCAM_PRE_FBC_STATUS0                            (DCAM_COMMON_BASE + 0x00C0UL)
#define DCAM_PRE_FBC_STATUS1                            (DCAM_COMMON_BASE + 0x00C4UL)
#define DCAM_PRE_FBC_STATUS2                            (DCAM_COMMON_BASE + 0x00C8UL)
#define DCAM_PRE_FBC_STATUS3                            (DCAM_COMMON_BASE + 0x00CCUL)
#define DCAM_CAP_FBC_STATUS0                            (DCAM_COMMON_BASE + 0x00D0UL)
#define DCAM_CAP_FBC_STATUS1                            (DCAM_COMMON_BASE + 0x00D4UL)
#define DCAM_CAP_FBC_STATUS2                            (DCAM_COMMON_BASE + 0x00D8UL)
#define DCAM_CAP_FBC_STATUS3                            (DCAM_COMMON_BASE + 0x00DCUL)


/*for dcam IT*/
#define DCAM_INT_MASK                                   DCAM_INT0_MASK
#define DCAM_INT_EN                                     DCAM_INT0_EN
#define DCAM_INT_CLR                                    DCAM_INT0_CLR
#define DCAM_INT_RAW                                    DCAM_INT0_RAW
#define DCAM_CHECK_LASE_STATUS                          DCAM_CAP_FBC_STATUS3

#define DCAM_MIPI_CAP_BASE                              (0x0400UL)
#define DCAM_MIPI_CAP_CFG                               (DCAM_MIPI_CAP_BASE + 0x0010UL)
#define DCAM_MIPI_CAP_START                             (DCAM_MIPI_CAP_BASE + 0x0014UL)
#define DCAM_MIPI_CAP_END                               (DCAM_MIPI_CAP_BASE + 0x0018UL)
#define DCAM_IMAGE_CONTROL                              (DCAM_MIPI_CAP_BASE + 0x001CUL)
#define DCAM_VCH1_CAP_CFG0                              (DCAM_MIPI_CAP_BASE + 0x0020UL)
#define DCAM_VCH1_CAP_CFG1                              (DCAM_MIPI_CAP_BASE + 0x0024UL)
#define DCAM_VCH2_CAP_CFG0                              (DCAM_MIPI_CAP_BASE + 0x0028UL)
#define DCAM_VCH2_CAP_CFG1                              (DCAM_MIPI_CAP_BASE + 0x002CUL)
#define DCAM_VCH3_CAP_CFG0                              (DCAM_MIPI_CAP_BASE + 0x0030UL)
#define DCAM_VCH3_CAP_CFG1                              (DCAM_MIPI_CAP_BASE + 0x0034UL)
#define DCAM_MIPI_CAP_CFG1                              (DCAM_MIPI_CAP_BASE + 0x0038UL)
#define DCAM_MIPI_CAP_CFG2                              (DCAM_MIPI_CAP_BASE + 0x003CUL)
#define DCAM_MIPI_CAP_CFG3                              (DCAM_MIPI_CAP_BASE + 0x0040UL)
#define DCAM_MIPI_REDUNDANT                             (DCAM_MIPI_CAP_BASE + 0x0044UL)
#define DCAM_CAP_FRM_CLR                                (DCAM_MIPI_CAP_BASE + 0x0048UL)
#define DCAM_CAP_RAW_SIZE                               (DCAM_MIPI_CAP_BASE + 0x004CUL)

#define DCAM_BWU1_BASE                                  (0x1000UL)
#define DCAM_BWU1_PARAM                                 (DCAM_BWU1_BASE + 0x0010UL)

#define DCAM_BIN_4IN1_BASE                              (0x1100UL)
#define DCAM_BIN_4IN1_CTRL0                             (DCAM_BIN_4IN1_BASE + 0x0010UL)
#define DCAM_BIN_4IN1_PARA0                             (DCAM_BIN_4IN1_BASE + 0x0014UL)
#define DCAM_BIN_4IN1_PARA1                             (DCAM_BIN_4IN1_BASE + 0x0018UL)

#define DCAM_BLC_BASE                                   (0x1300UL)
#define DCAM_BLC_PARA                                   (DCAM_BLC_BASE + 0x0010UL)
#define DCAM_BLC_PARA_R_B                               (DCAM_BLC_BASE + 0x0014UL)
#define DCAM_BLC_PARA_G                                 (DCAM_BLC_BASE + 0x0018UL)

#define DCAM_RGBG_BASE                                  (0x1800UL)
#define DCAM_RGBG_YRANDOM_PARAMETER0                    (DCAM_RGBG_BASE + 0x0010UL)
#define DCAM_RGBG_RB                                    (DCAM_RGBG_BASE + 0x0014UL)
#define DCAM_RGBG_G                                     (DCAM_RGBG_BASE + 0x0018UL)
#define DCAM_RGBG_YRANDOM_PARAMETER1                    (DCAM_RGBG_BASE + 0x001CUL)
#define DCAM_RGBG_YRANDOM_PARAMETER2                    (DCAM_RGBG_BASE + 0x0020UL)
#define DCAM_RGBG_YUV_YRANDOM_STATUS0                   (DCAM_RGBG_BASE + 0x0024UL)
#define DCAM_RGBG_YUV_YRANDOM_STATUS1                   (DCAM_RGBG_BASE + 0x0028UL)
#define DCAM_RGBG_YUV_YRANDOM_STATUS2                   (DCAM_RGBG_BASE + 0x002CUL)

#define DCAM_ISP_PPE_BASE                               (0x1400UL)
#define ISP_PPI_PARAM                                   (DCAM_ISP_PPE_BASE + 0x0010UL)
#define ISP_PPI_BLOCK_COL                               (DCAM_ISP_PPE_BASE + 0x0014UL)
#define ISP_PPI_BLOCK_ROW                               (DCAM_ISP_PPE_BASE + 0x0018UL)
#define ISP_PPI_GLB_START                               (DCAM_ISP_PPE_BASE + 0x001CUL)
#define ISP_PPI_AF_WIN_START                            (DCAM_ISP_PPE_BASE + 0x0020UL)
#define ISP_PPI_AF_WIN_END                              (DCAM_ISP_PPE_BASE + 0x0024UL)
#define ISP_PPI_PATTERN01                               (DCAM_ISP_PPE_BASE + 0x0028UL)
#define ISP_PPI_PATTERN02                               (DCAM_ISP_PPE_BASE + 0x002CUL)
#define ISP_PPI_PATTERN03                               (DCAM_ISP_PPE_BASE + 0x0030UL)
#define ISP_PPI_PATTERN04                               (DCAM_ISP_PPE_BASE + 0x0034UL)
#define ISP_PPI_PATTERN05                               (DCAM_ISP_PPE_BASE + 0x0038UL)
#define ISP_PPI_PATTERN06                               (DCAM_ISP_PPE_BASE + 0x003CUL)
#define ISP_PPI_PATTERN07                               (DCAM_ISP_PPE_BASE + 0x0040UL)
#define ISP_PPI_PATTERN08                               (DCAM_ISP_PPE_BASE + 0x0044UL)
#define ISP_PPI_PATTERN09                               (DCAM_ISP_PPE_BASE + 0x0048UL)
#define ISP_PPI_PATTERN10                               (DCAM_ISP_PPE_BASE + 0x004CUL)
#define ISP_PPI_PATTERN11                               (DCAM_ISP_PPE_BASE + 0x0050UL)
#define ISP_PPI_PATTERN12                               (DCAM_ISP_PPE_BASE + 0x0054UL)
#define ISP_PPI_PATTERN13                               (DCAM_ISP_PPE_BASE + 0x0058UL)
#define ISP_PPI_PATTERN14                               (DCAM_ISP_PPE_BASE + 0x005CUL)
#define ISP_PPI_PATTERN15                               (DCAM_ISP_PPE_BASE + 0x0060UL)
#define ISP_PPI_PATTERN16                               (DCAM_ISP_PPE_BASE + 0x0064UL)
#define ISP_PPI_PATTERN17                               (DCAM_ISP_PPE_BASE + 0x0068UL)
#define ISP_PPI_PATTERN18                               (DCAM_ISP_PPE_BASE + 0x006CUL)
#define ISP_PPI_PATTERN19                               (DCAM_ISP_PPE_BASE + 0x0070UL)
#define ISP_PPI_PATTERN20                               (DCAM_ISP_PPE_BASE + 0x0074UL)
#define ISP_PPI_PATTERN21                               (DCAM_ISP_PPE_BASE + 0x0078UL)
#define ISP_PPI_PATTERN22                               (DCAM_ISP_PPE_BASE + 0x007CUL)
#define ISP_PPI_PATTERN23                               (DCAM_ISP_PPE_BASE + 0x0080UL)
#define ISP_PPI_PATTERN24                               (DCAM_ISP_PPE_BASE + 0x0084UL)
#define ISP_PPI_PATTERN25                               (DCAM_ISP_PPE_BASE + 0x0088UL)
#define ISP_PPI_PATTERN26                               (DCAM_ISP_PPE_BASE + 0x008CUL)
#define ISP_PPI_PATTERN27                               (DCAM_ISP_PPE_BASE + 0x0090UL)
#define ISP_PPI_PATTERN28                               (DCAM_ISP_PPE_BASE + 0x0094UL)
#define ISP_PPI_PATTERN29                               (DCAM_ISP_PPE_BASE + 0x0098UL)
#define ISP_PPI_PATTERN30                               (DCAM_ISP_PPE_BASE + 0x009CUL)
#define ISP_PPI_PATTERN31                               (DCAM_ISP_PPE_BASE + 0x00A0UL)
#define ISP_PPI_PATTERN32                               (DCAM_ISP_PPE_BASE + 0x00A4UL)
#define DCAM_PPE_FRM_CTRL0                              (DCAM_ISP_PPE_BASE+ 0x00A8UL)
#define DCAM_PPE_FRM_CTRL1                              (DCAM_ISP_PPE_BASE + 0x00ACUL)
#define DCAM_PPE_RIGHT_WADDR                            (DCAM_ISP_PPE_BASE + 0x00B0UL)
#define DCAM_PDAF_CONTROL                               (DCAM_ISP_PPE_BASE + 0x00B4UL)
#define DCAM_PDAF_CORR_TABLE                            (0xDC00UL)

#define DCAM_LSCM_BASE                                  (0x1900UL)
#define DCAM_LSCM_FRM_CTRL0                             (DCAM_LSCM_BASE + 0x0010UL)
#define DCAM_LSCM_FRM_CTRL1                             (DCAM_LSCM_BASE + 0x0014UL)
#define DCAM_LSCM_OFFSET                                (DCAM_LSCM_BASE + 0x0018UL)
#define DCAM_LSCM_BLK_NUM                               (DCAM_LSCM_BASE + 0x001CUL)
#define DCAM_LSCM_BLK_SIZE                              (DCAM_LSCM_BASE + 0x0020UL)
#define DCAM_LSCM_BASE_WADDR                            (DCAM_LSCM_BASE + 0x0024UL)
#define DCAM_LSCM_BURST_STOP                            (DCAM_LSCM_BASE + 0x0028UL)

#define DCAM_LENS_BASE                                  (0x1A00UL)
#define DCAM_LENS_LOAD_ENABLE                           (DCAM_LENS_BASE + 0x0010UL)
#define DCAM_LENS_GRID_NUMBER                           (DCAM_LENS_BASE + 0x0014UL)
#define DCAM_LENS_GRID_SIZE                             (DCAM_LENS_BASE + 0x0018UL)
#define DCAM_LENS_LOAD_CLR                              (DCAM_LENS_BASE + 0x001CUL)
#define DCAM_LENS_SLICE_CTRL0                           (DCAM_LENS_BASE + 0x0020UL)
#define DCAM_LENS_SLICE_CTRL1                           (DCAM_LENS_BASE + 0x0024UL)
#define DCAM_LENS_STATUS                                (DCAM_LENS_BASE + 0x0028UL)
#define DCAM_LENS_GRID_WIDTH                            (DCAM_LENS_BASE + 0x002CUL)
#define DCAM_LENS_BASE_RADDR                            (DCAM_LENS_BASE + 0x0030UL)

#define DCAM_AFL_BASE                                   (0x1B00UL)
#define ISP_AFL_STATUS0                                 (DCAM_AFL_BASE + 0x0000UL)
#define ISP_AFL_STATUS1                                 (DCAM_AFL_BASE + 0x0004UL)
#define ISP_AFL_STATUS2                                 (DCAM_AFL_BASE + 0x0008UL)
#define ISP_AFL_PARAM0                                  (DCAM_AFL_BASE + 0x0010UL)
#define ISP_AFL_PARAM1                                  (DCAM_AFL_BASE + 0x0014UL)
#define ISP_AFL_PARAM2                                  (DCAM_AFL_BASE + 0x0018UL)
#define ISP_AFL_COL_POS                                 (DCAM_AFL_BASE + 0x001CUL)
#define ISP_AFL_DDR_INIT_ADDR                           (DCAM_AFL_BASE + 0x0020UL)
#define ISP_AFL_REGION0                                 (DCAM_AFL_BASE + 0x0024UL)
#define ISP_AFL_REGION1                                 (DCAM_AFL_BASE + 0x0028UL)
#define ISP_AFL_REGION2                                 (DCAM_AFL_BASE + 0x002CUL)
#define ISP_AFL_REGION_WADDR                            (DCAM_AFL_BASE + 0x0030UL)
#define ISP_AFL_SUM1                                    (DCAM_AFL_BASE + 0x0034UL)
#define ISP_AFL_SUM2                                    (DCAM_AFL_BASE + 0x0038UL)
#define ISP_AFL_CFG_READY                               (DCAM_AFL_BASE + 0x003CUL)
#define ISP_AFL_SKIP_NUM_CLR                            (DCAM_AFL_BASE + 0x0040UL)
#define ISP_AFL_SLICE1                                  (DCAM_AFL_BASE + 0x0044UL)
#define ISP_AFL_SLICE2                                  (DCAM_AFL_BASE + 0x0048UL)
#define ISP_AFL_SLICE3                                  (DCAM_AFL_BASE + 0x004CUL)
#define ISP_AFL_SLICE4                                  (DCAM_AFL_BASE + 0x0050UL)

#define DCAM_BAYER_HIST_BASE                            (0x1C00UL)
#define DCAM_BAYER_HIST_CTRL0                           (DCAM_BAYER_HIST_BASE + 0x0010UL)
#define DCAM_BAYER_HIST_CTRL1                           (DCAM_BAYER_HIST_BASE + 0x0014UL)
#define DCAM_BAYER_HIST_START                           (DCAM_BAYER_HIST_BASE + 0x0018UL)
#define DCAM_BAYER_HIST_END                             (DCAM_BAYER_HIST_BASE + 0x001CUL)
#define DCAM_BAYER_HIST_STATUS                          (DCAM_BAYER_HIST_BASE + 0x0020UL)
#define DCAM_BAYER_HIST_BASE_WADDR                      (DCAM_BAYER_HIST_BASE + 0x0024UL)

#define DCAM_AEM_BASE                                   (0x1D00UL)
#define DCAM_AEM_STATUS0                                (DCAM_AEM_BASE + 0x0000UL)
#define DCAM_AEM_STATUS1                                (DCAM_AEM_BASE + 0x0004UL)
#define DCAM_AEM_FRM_CTRL0                              (DCAM_AEM_BASE + 0x0010UL)
#define DCAM_AEM_OFFSET                                 (DCAM_AEM_BASE + 0x0014UL)
#define DCAM_AEM_BLK_NUM                                (DCAM_AEM_BASE + 0x0018UL)
#define DCAM_AEM_BLK_SIZE                               (DCAM_AEM_BASE + 0x001CUL)
#define DCAM_AEM_RED_THR                                (DCAM_AEM_BASE + 0x0020UL)
#define DCAM_AEM_BLUE_THR                               (DCAM_AEM_BASE + 0x0024UL)
#define DCAM_AEM_GREEN_THR                              (DCAM_AEM_BASE + 0x0028UL)
#define DCAM_AEM_BASE_WADDR                             (DCAM_AEM_BASE + 0x002CUL)
#define DCAM_AEM_FRM_CTRL1                              (DCAM_AEM_BASE + 0x0030UL)
#define DCAM_AEM_FRM_CTRL2                              (DCAM_AEM_BASE + 0x0034UL)

#define DCAM_CROP0_BASE                                 (0x1E00UL)
#define DCAM_CROP0_PARAM0                               (DCAM_CROP0_BASE + 0x0010UL)
#define DCAM_CROP0_PARAM1                               (DCAM_CROP0_BASE + 0x0014UL)
#define DCAM_CROP0_PARAM2                               (DCAM_CROP0_BASE + 0x0018UL)

#define DCAM_AWBC_BASE                                  (0x1F00UL)
#define DCAM_AWBC_GAIN0                                 (DCAM_AWBC_BASE + 0x0010UL)
#define DCAM_AWBC_GAIN1                                 (DCAM_AWBC_BASE + 0x0014UL)
#define DCAM_AWBC_THRD0                                 (DCAM_AWBC_BASE + 0x0018UL)
#define DCAM_AWBC_THRD1                                 (DCAM_AWBC_BASE + 0x001CUL)
#define DCAM_AWBC_OFFSET0                               (DCAM_AWBC_BASE + 0x0020UL)
#define DCAM_AWBC_OFFSET1                               (DCAM_AWBC_BASE + 0x0024UL)
#define DCAM_AWBC_STATUS                                (DCAM_AWBC_BASE + 0x0028UL)

#define DCAM_BPC_BASE                                   (0x2000UL)
#define DCAM_BPC_PARAM                                  (DCAM_BPC_BASE + 0x0010UL)
#define DCAM_BPC_MAP_CTRL                               (DCAM_BPC_BASE + 0x0014UL)
#define DCAM_BPC_BAD_PIXEL_TH0                          (DCAM_BPC_BASE + 0x0018UL)
#define DCAM_BPC_BAD_PIXEL_TH1                          (DCAM_BPC_BASE + 0x001CUL)
#define DCAM_BPC_BAD_PIXEL_TH2                          (DCAM_BPC_BASE + 0x0020UL)
#define DCAM_BPC_BAD_PIXEL_TH3                          (DCAM_BPC_BASE + 0x0024UL)
#define DCAM_BPC_FLAT_TH                                (DCAM_BPC_BASE + 0x0028UL)
#define DCAM_BPC_EDGE_RATIO0                            (DCAM_BPC_BASE + 0x002CUL)
#define DCAM_BPC_EDGE_RATIO1                            (DCAM_BPC_BASE + 0x0030UL)
#define DCAM_BPC_BAD_PIXEL_PARAM                        (DCAM_BPC_BASE + 0x0034UL)
#define DCAM_BPC_GDIF_TH                                (DCAM_BPC_BASE + 0x0038UL)
#define DCAM_BPC_LUTWORD0                               (DCAM_BPC_BASE + 0x003CUL)
#define DCAM_BPC_LUTWORD1                               (DCAM_BPC_BASE + 0x0040UL)
#define DCAM_BPC_LUTWORD2                               (DCAM_BPC_BASE + 0x0044UL)
#define DCAM_BPC_LUTWORD3                               (DCAM_BPC_BASE + 0x0048UL)
#define DCAM_BPC_LUTWORD4                               (DCAM_BPC_BASE + 0x004CUL)
#define DCAM_BPC_LUTWORD5                               (DCAM_BPC_BASE + 0x0050UL)
#define DCAM_BPC_LUTWORD6                               (DCAM_BPC_BASE + 0x0054UL)
#define DCAM_BPC_LUTWORD7                               (DCAM_BPC_BASE + 0x0058UL)
#define DCAM_BPC_PPI_RANG                               (DCAM_BPC_BASE + 0x005CUL)
#define DCAM_BPC_PPI_RANG1                              (DCAM_BPC_BASE + 0x0060UL)
#define DCAM_BPC_PPI_GLB_START                          (DCAM_BPC_BASE + 0x0064UL)
#define DCAM_BPC_SLICE_ROI_S                            (DCAM_BPC_BASE + 0x0068UL)
#define DCAM_BPC_SLICE_ROI_R                            (DCAM_BPC_BASE + 0x006CUL)
#define DCAM_BPC_LAST_ADDR                              (DCAM_BPC_BASE + 0x0070UL)
#define DCAM_BPC_STATUS0                                (DCAM_BPC_BASE + 0x0074UL)
#define DCAM_BPC_STATUS1                                (DCAM_BPC_BASE + 0x0078UL)
#define DCAM_BPC_PPI_PARAM                              (DCAM_BPC_BASE + 0x007CUL)
#define DCAM_BPC_PPI_PATTERN01                          (DCAM_BPC_BASE + 0x0080UL)
#define DCAM_BPC_PPI_PATTERN02                          (DCAM_BPC_BASE + 0x0084UL)
#define DCAM_BPC_PPI_PATTERN03                          (DCAM_BPC_BASE + 0x0088UL)
#define DCAM_BPC_PPI_PATTERN04                          (DCAM_BPC_BASE + 0x008CUL)
#define DCAM_BPC_PPI_PATTERN05                          (DCAM_BPC_BASE + 0x0090UL)
#define DCAM_BPC_PPI_PATTERN06                          (DCAM_BPC_BASE + 0x0094UL)
#define DCAM_BPC_PPI_PATTERN07                          (DCAM_BPC_BASE + 0x0098UL)
#define DCAM_BPC_PPI_PATTERN08                          (DCAM_BPC_BASE + 0x009CUL)
#define DCAM_BPC_PPI_PATTERN09                          (DCAM_BPC_BASE + 0x00A0UL)
#define DCAM_BPC_PPI_PATTERN10                          (DCAM_BPC_BASE + 0x00A4UL)
#define DCAM_BPC_PPI_PATTERN11                          (DCAM_BPC_BASE + 0x00A8UL)
#define DCAM_BPC_PPI_PATTERN12                          (DCAM_BPC_BASE + 0x00ACUL)
#define DCAM_BPC_PPI_PATTERN13                          (DCAM_BPC_BASE + 0x00B0UL)
#define DCAM_BPC_PPI_PATTERN14                          (DCAM_BPC_BASE + 0x00B4UL)
#define DCAM_BPC_PPI_PATTERN15                          (DCAM_BPC_BASE + 0x00B8UL)
#define DCAM_BPC_PPI_PATTERN16                          (DCAM_BPC_BASE + 0x00BCUL)
#define DCAM_BPC_PPI_PATTERN17                          (DCAM_BPC_BASE + 0x00C0UL)
#define DCAM_BPC_PPI_PATTERN18                          (DCAM_BPC_BASE + 0x00C4UL)
#define DCAM_BPC_PPI_PATTERN19                          (DCAM_BPC_BASE + 0x00C8UL)
#define DCAM_BPC_PPI_PATTERN20                          (DCAM_BPC_BASE + 0x00CCUL)
#define DCAM_BPC_PPI_PATTERN21                          (DCAM_BPC_BASE + 0x00D0UL)
#define DCAM_BPC_PPI_PATTERN22                          (DCAM_BPC_BASE + 0x00D4UL)
#define DCAM_BPC_PPI_PATTERN23                          (DCAM_BPC_BASE + 0x00D8UL)
#define DCAM_BPC_PPI_PATTERN24                          (DCAM_BPC_BASE + 0x00DCUL)
#define DCAM_BPC_PPI_PATTERN25                          (DCAM_BPC_BASE + 0x00E0UL)
#define DCAM_BPC_PPI_PATTERN26                          (DCAM_BPC_BASE + 0x00E4UL)
#define DCAM_BPC_PPI_PATTERN27                          (DCAM_BPC_BASE + 0x00E8UL)
#define DCAM_BPC_PPI_PATTERN28                          (DCAM_BPC_BASE + 0x00ECUL)
#define DCAM_BPC_PPI_PATTERN29                          (DCAM_BPC_BASE + 0x00F0UL)
#define DCAM_BPC_PPI_PATTERN30                          (DCAM_BPC_BASE + 0x00F4UL)
#define DCAM_BPC_PPI_PATTERN31                          (DCAM_BPC_BASE + 0x00F8UL)
#define DCAM_BPC_PPI_PATTERN32                          (DCAM_BPC_BASE + 0x00FCUL)
#define DCAM_BPC_MAP_ADDR                               (DCAM_BPC_BASE + 0x0100UL)
#define DCAM_BPC_OUT_ADDR                               (DCAM_BPC_BASE + 0x0104UL)

#define DCAM_CROP1_BASE                                 (0x2200UL)
#define DCAM_CROP1_CTRL                                 (DCAM_CROP1_BASE + 0x0010UL)
#define DCAM_CROP1_START                                (DCAM_CROP1_BASE + 0x0014UL)
#define DCAM_CROP1_SIZE                                 (DCAM_CROP1_BASE + 0x0018UL)

#define DCAM_BWD1_BASE                                  (0x2300UL)
#define DCAM_BWD1_PARAM                                 (DCAM_BWD1_BASE + 0x0010UL)

#define DCAM_3DNR_ME_BASE                               (0x2500UL)
#define DCAM_NR3_FAST_ME_PARAM                          (DCAM_3DNR_ME_BASE + 0x0010UL)
#define DCAM_NR3_FAST_ME_ROI_PARAM0                     (DCAM_3DNR_ME_BASE + 0x0014UL)
#define DCAM_NR3_FAST_ME_ROI_PARAM1                     (DCAM_3DNR_ME_BASE + 0x0018UL)
#define DCAM_NR3_FAST_ME_OUT0                           (DCAM_3DNR_ME_BASE + 0x001CUL)
#define DCAM_NR3_FAST_ME_OUT1                           (DCAM_3DNR_ME_BASE + 0x0020UL)
#define DCAM_NR3_FAST_ME_STATUS                         (DCAM_3DNR_ME_BASE + 0x0024UL)
#define DCAM_NR3_WADDR                                  (DCAM_3DNR_ME_BASE + 0x0028UL)

#define DCAM_VST_BASE                                   (0x2600UL)
#define DCAM_VST_PARA                                   (DCAM_VST_BASE + 0x0010UL)
#define DCAM_VST_TABLE                                  (0xA000UL)

#define DCAM_NLM_BASE                                   (0x2800UL)
#define DCAM_NLM_PARA                                   (DCAM_NLM_BASE + 0x0010UL)
#define DCAM_NLM_MODE_CNT                               (DCAM_NLM_BASE + 0x0014UL)
#define DCAM_NLM_SIMPLE_BPC                             (DCAM_NLM_BASE + 0x0018UL)
#define DCAM_NLM_LUM_THRESHOLD                          (DCAM_NLM_BASE + 0x001CUL)
#define DCAM_NLM_DIRECTION_TH                           (DCAM_NLM_BASE + 0x0020UL)
#define DCAM_NLM_IS_FLAT                                (DCAM_NLM_BASE + 0x0024UL)
#define DCAM_NLM_LUT_W_0                                (DCAM_NLM_BASE + 0x0028UL)
#define DCAM_NLM_LUT_W_1                                (DCAM_NLM_BASE + 0x002CUL)
#define DCAM_NLM_LUT_W_2                                (DCAM_NLM_BASE + 0x0030UL)
#define DCAM_NLM_LUT_W_3                                (DCAM_NLM_BASE + 0x0034UL)
#define DCAM_NLM_LUT_W_4                                (DCAM_NLM_BASE + 0x0038UL)
#define DCAM_NLM_LUT_W_5                                (DCAM_NLM_BASE + 0x003CUL)
#define DCAM_NLM_LUT_W_6                                (DCAM_NLM_BASE + 0x0040UL)
#define DCAM_NLM_LUT_W_7                                (DCAM_NLM_BASE + 0x0044UL)
#define DCAM_NLM_LUT_W_8                                (DCAM_NLM_BASE + 0x0048UL)
#define DCAM_NLM_LUT_W_9                                (DCAM_NLM_BASE + 0x004CUL)
#define DCAM_NLM_LUT_W_10                               (DCAM_NLM_BASE + 0x0050UL)
#define DCAM_NLM_LUT_W_11                               (DCAM_NLM_BASE + 0x0054UL)
#define DCAM_NLM_LUT_W_12                               (DCAM_NLM_BASE + 0x0058UL)
#define DCAM_NLM_LUT_W_13                               (DCAM_NLM_BASE + 0x005CUL)
#define DCAM_NLM_LUT_W_14                               (DCAM_NLM_BASE + 0x0060UL)
#define DCAM_NLM_LUT_W_15                               (DCAM_NLM_BASE + 0x0064UL)
#define DCAM_NLM_LUT_W_16                               (DCAM_NLM_BASE + 0x0068UL)
#define DCAM_NLM_LUT_W_17                               (DCAM_NLM_BASE + 0x006CUL)
#define DCAM_NLM_LUT_W_18                               (DCAM_NLM_BASE + 0x0070UL)
#define DCAM_NLM_LUT_W_19                               (DCAM_NLM_BASE + 0x0074UL)
#define DCAM_NLM_LUT_W_20                               (DCAM_NLM_BASE + 0x0078UL)
#define DCAM_NLM_LUT_W_21                               (DCAM_NLM_BASE + 0x007CUL)
#define DCAM_NLM_LUT_W_22                               (DCAM_NLM_BASE + 0x0080UL)
#define DCAM_NLM_LUT_W_23                               (DCAM_NLM_BASE + 0x0084UL)
#define DCAM_NLM_LUM0_FLAT0_PARAM                       (DCAM_NLM_BASE + 0x0088UL)
#define DCAM_NLM_LUM0_FLAT0_ADDBACK                     (DCAM_NLM_BASE + 0x008CUL)
#define DCAM_NLM_LUM0_FLAT1_PARAM                       (DCAM_NLM_BASE + 0x0090UL)
#define DCAM_NLM_LUM0_FLAT1_ADDBACK                     (DCAM_NLM_BASE + 0x0094UL)
#define DCAM_NLM_LUM0_FLAT2_PARAM                       (DCAM_NLM_BASE + 0x0098UL)
#define DCAM_NLM_LUM0_FLAT2_ADDBACK                     (DCAM_NLM_BASE + 0x009CUL)
#define DCAM_NLM_LUM0_FLAT3_PARAM                       (DCAM_NLM_BASE + 0x00A0UL)
#define DCAM_NLM_LUM0_FLAT3_ADDBACK                     (DCAM_NLM_BASE + 0x00A4UL)
#define DCAM_NLM_LUM1_FLAT0_PARAM                       (DCAM_NLM_BASE + 0x00A8UL)
#define DCAM_NLM_LUM1_FLAT0_ADDBACK                     (DCAM_NLM_BASE + 0x00ACUL)
#define DCAM_NLM_LUM1_FLAT1_PARAM                       (DCAM_NLM_BASE + 0x00B0UL)
#define DCAM_NLM_LUM1_FLAT1_ADDBACK                     (DCAM_NLM_BASE + 0x00B4UL)
#define DCAM_NLM_LUM1_FLAT2_PARAM                       (DCAM_NLM_BASE + 0x00B8UL)
#define DCAM_NLM_LUM1_FLAT2_ADDBACK                     (DCAM_NLM_BASE + 0x00BCUL)
#define DCAM_NLM_LUM1_FLAT3_PARAM                       (DCAM_NLM_BASE + 0x00C0UL)
#define DCAM_NLM_LUM1_FLAT3_ADDBACK                     (DCAM_NLM_BASE + 0x00C4UL)
#define DCAM_NLM_LUM2_FLAT0_PARAM                       (DCAM_NLM_BASE + 0x00C8UL)
#define DCAM_NLM_LUM2_FLAT0_ADDBACK                     (DCAM_NLM_BASE + 0x00CCUL)
#define DCAM_NLM_LUM2_FLAT1_PARAM                       (DCAM_NLM_BASE + 0x00D0UL)
#define DCAM_NLM_LUM2_FLAT1_ADDBACK                     (DCAM_NLM_BASE + 0x00D4UL)
#define DCAM_NLM_LUM2_FLAT2_PARAM                       (DCAM_NLM_BASE + 0x00D8UL)
#define DCAM_NLM_LUM2_FLAT2_ADDBACK                     (DCAM_NLM_BASE + 0x00DCUL)
#define DCAM_NLM_LUM2_FLAT3_PARAM                       (DCAM_NLM_BASE + 0x00E0UL)
#define DCAM_NLM_LUM2_FLAT3_ADDBACK                     (DCAM_NLM_BASE + 0x00E4UL)
#define DCAM_NLM_ADDBACK3                               (DCAM_NLM_BASE + 0x00E8UL)
#define DCAM_NLM_RADIAL_1D_PARAM                        (DCAM_NLM_BASE + 0x00ECUL)
#define DCAM_NLM_RADIAL_1D_DIST                         (DCAM_NLM_BASE + 0x00F0UL)
#define DCAM_NLM_RADIAL_1D_THRESHOLD                    (DCAM_NLM_BASE + 0x00F4UL)
#define DCAM_NLM_RADIAL_1D_THR0                         (DCAM_NLM_BASE + 0x00F8UL)
#define DCAM_NLM_RADIAL_1D_THR1                         (DCAM_NLM_BASE + 0x00FCUL)
#define DCAM_NLM_RADIAL_1D_THR2                         (DCAM_NLM_BASE + 0x0100UL)
#define DCAM_NLM_RADIAL_1D_THR3                         (DCAM_NLM_BASE + 0x0104UL)
#define DCAM_NLM_RADIAL_1D_THR4                         (DCAM_NLM_BASE + 0x0108UL)
#define DCAM_NLM_RADIAL_1D_THR5                         (DCAM_NLM_BASE + 0x010CUL)
#define DCAM_NLM_RADIAL_1D_THR6                         (DCAM_NLM_BASE + 0x0110UL)
#define DCAM_NLM_RADIAL_1D_THR7                         (DCAM_NLM_BASE + 0x0114UL)
#define DCAM_NLM_RADIAL_1D_THR8                         (DCAM_NLM_BASE + 0x0118UL)
#define DCAM_NLM_RADIAL_1D_RATIO                        (DCAM_NLM_BASE + 0x011CUL)
#define DCAM_NLM_RADIAL_1D_RATIO1                       (DCAM_NLM_BASE + 0x0120UL)
#define DCAM_NLM_RADIAL_1D_RATIO2                       (DCAM_NLM_BASE + 0x0124UL)
#define DCAM_NLM_RADIAL_1D_RATIO3                       (DCAM_NLM_BASE + 0x0128UL)
#define DCAM_NLM_RADIAL_1D_RATIO4                       (DCAM_NLM_BASE + 0x012CUL)
#define DCAM_NLM_RADIAL_1D_RATIO5                       (DCAM_NLM_BASE + 0x0130UL)
#define DCAM_NLM_RADIAL_1D_RATIO6                       (DCAM_NLM_BASE + 0x0134UL)
#define DCAM_NLM_RADIAL_1D_RATIO7                       (DCAM_NLM_BASE + 0x0138UL)
#define DCAM_NLM_RADIAL_1D_RATIO8                       (DCAM_NLM_BASE + 0x013CUL)
#define DCAM_NLM_RADIAL_1D_RATIO9                       (DCAM_NLM_BASE + 0x0140UL)
#define DCAM_NLM_RADIAL_1D_RATIO10                      (DCAM_NLM_BASE + 0x0144UL)
#define DCAM_NLM_RADIAL_1D_RATIO11                      (DCAM_NLM_BASE + 0x0148UL)
#define DCAM_NLM_RADIAL_1D_GAIN_MAX                     (DCAM_NLM_BASE + 0x014CUL)
#define DCAM_NLM_RADIAL_1D_ADDBACK00                    (DCAM_NLM_BASE + 0x0150UL)
#define DCAM_NLM_RADIAL_1D_ADDBACK01                    (DCAM_NLM_BASE + 0x0154UL)
#define DCAM_NLM_RADIAL_1D_ADDBACK02                    (DCAM_NLM_BASE + 0x0158UL)
#define DCAM_NLM_RADIAL_1D_ADDBACK03                    (DCAM_NLM_BASE + 0x015CUL)
#define DCAM_NLM_RADIAL_1D_ADDBACK10                    (DCAM_NLM_BASE + 0x0160UL)
#define DCAM_NLM_RADIAL_1D_ADDBACK11                    (DCAM_NLM_BASE + 0x0164UL)
#define DCAM_NLM_RADIAL_1D_ADDBACK12                    (DCAM_NLM_BASE + 0x0168UL)
#define DCAM_NLM_RADIAL_1D_ADDBACK13                    (DCAM_NLM_BASE + 0x016CUL)
#define DCAM_NLM_RADIAL_1D_ADDBACK20                    (DCAM_NLM_BASE + 0x0170UL)
#define DCAM_NLM_RADIAL_1D_ADDBACK21                    (DCAM_NLM_BASE + 0x0174UL)
#define DCAM_NLM_RADIAL_1D_ADDBACK22                    (DCAM_NLM_BASE + 0x0178UL)
#define DCAM_NLM_RADIAL_1D_ADDBACK23                    (DCAM_NLM_BASE + 0x017CUL)

#define DCAM_IVST_BASE                                  (0x2700UL)
#define DCAM_IVST_PARA                                  (DCAM_IVST_BASE + 0x0010UL)
#define DCAM_IVST_TABLE                                 (0xB000UL)

#define DCAM_CROP3_BASE                                 (0x2A00UL)
#define DCAM_CROP3_CTRL                                 (DCAM_CROP3_BASE + 0x0010UL)
#define DCAM_CROP3_START                                (DCAM_CROP3_BASE + 0x0014UL)
#define DCAM_CROP3_SIZE                                 (DCAM_CROP3_BASE + 0x0018UL)

#define DCAM_AFM_BIN_BASE                               (0x2B00UL)
#define DCAM_AFM_BIN_CTRL                               (DCAM_AFM_BIN_BASE + 0x0010UL)
#define DCAM_AFM_BIN_PARAM                              (DCAM_AFM_BIN_BASE + 0x0014UL)
#define DCAM_AFM_BIN_DBG0                               (DCAM_AFM_BIN_BASE + 0x0018UL)
#define DCAM_AFM_BIN_DBG1                               (DCAM_AFM_BIN_BASE + 0x001CUL)

#define DCAM_AFM_GAMC_BASE                              (0x2C00UL)
#define DCAM_AFM_GAMC_CTRL                              (DCAM_AFM_GAMC_BASE + 0x0010UL)
#define DCAM_AFM_GAMC_DBG0                              (DCAM_AFM_GAMC_BASE + 0x0014UL)
#define DCAM_AFM_GAMC_DBG1                              (DCAM_AFM_GAMC_BASE + 0x0018UL)
#define DCAM_AFM_FGAMMA_TABLE                           (0xF000UL)

#define DCAM_AFM_BASE                                   (0x2D00UL)
#define DCAM_AFM_FRM_CTRL                               (DCAM_AFM_BASE + 0x0010UL)
#define DCAM_AFM_FRM_CTRL1                              (DCAM_AFM_BASE + 0x0014UL)
#define DCAM_AFM_PARAMETERS                             (DCAM_AFM_BASE + 0x0018UL)
#define DCAM_AFM_ENHANCE_CTRL                           (DCAM_AFM_BASE + 0x001CUL)
#define DCAM_AFM_WIN_RANGE0S                            (DCAM_AFM_BASE + 0x0020UL)
#define DCAM_AFM_WIN_RANGE0E                            (DCAM_AFM_BASE + 0x0024UL)
#define DCAM_AFM_WIN_RANGE1S                            (DCAM_AFM_BASE + 0x0028UL)
#define DCAM_AFM_IIR_FILTER0                            (DCAM_AFM_BASE + 0x002CUL)
#define DCAM_AFM_IIR_FILTER1                            (DCAM_AFM_BASE + 0x0030UL)
#define DCAM_AFM_IIR_FILTER2                            (DCAM_AFM_BASE + 0x0034UL)
#define DCAM_AFM_IIR_FILTER3                            (DCAM_AFM_BASE + 0x0038UL)
#define DCAM_AFM_IIR_FILTER4                            (DCAM_AFM_BASE + 0x003CUL)
#define DCAM_AFM_IIR_FILTER5                            (DCAM_AFM_BASE + 0x0040UL)
#define DCAM_AFM_ENHANCE_FV0_THD                        (DCAM_AFM_BASE + 0x0044UL)
#define DCAM_AFM_ENHANCE_FV1_THD                        (DCAM_AFM_BASE + 0x0048UL)
#define DCAM_AFM_ENHANCE_FV1_COEFF00                    (DCAM_AFM_BASE + 0x004CUL)
#define DCAM_AFM_ENHANCE_FV1_COEFF01                    (DCAM_AFM_BASE + 0x0050UL)
#define DCAM_AFM_ENHANCE_FV1_COEFF10                    (DCAM_AFM_BASE + 0x0054UL)
#define DCAM_AFM_ENHANCE_FV1_COEFF11                    (DCAM_AFM_BASE + 0x0058UL)
#define DCAM_AFM_ENHANCE_FV1_COEFF20                    (DCAM_AFM_BASE + 0x005CUL)
#define DCAM_AFM_ENHANCE_FV1_COEFF21                    (DCAM_AFM_BASE + 0x0060UL)
#define DCAM_AFM_ENHANCE_FV1_COEFF30                    (DCAM_AFM_BASE + 0x0064UL)
#define DCAM_AFM_ENHANCE_FV1_COEFF31                    (DCAM_AFM_BASE + 0x0068UL)
#define DCAM_AFM_MAX_LUM                                (DCAM_AFM_BASE + 0x006CUL)
#define DCAM_AFM_LUM_FV_BASE_WADDR                      (DCAM_AFM_BASE + 0x0070UL)
#define DCAM_AFM_HIST_BASE_WADDR                        (DCAM_AFM_BASE + 0x0074UL)
#define DCAM_AFM_TILE_CNT_OUT                           (DCAM_AFM_BASE + 0x0078UL)
#define DCAM_AFM_CNT_OUT                                (DCAM_AFM_BASE + 0x007CUL)
#define DCAM_AFM_STATUS0                                (DCAM_AFM_BASE + 0x0080UL)
#define DCAM_AFM_STATUS1                                (DCAM_AFM_BASE + 0x0084UL)
#define DCAM_AFM_WR_STATUS                              (DCAM_AFM_BASE + 0x0088UL)

#define DCAM_NLM_IMBLANCE_BASE                          (0x2E00UL)
#define DCAM_NLM_IMBLANCE_CTRL                          (DCAM_NLM_IMBLANCE_BASE + 0x0010UL)
#define DCAM_NLM_IMBLANCE_PARA1                         (DCAM_NLM_IMBLANCE_BASE + 0x0014UL)
#define DCAM_NLM_IMBLANCE_PARA2                         (DCAM_NLM_IMBLANCE_BASE + 0x0018UL)
#define DCAM_NLM_IMBLANCE_PARA3                         (DCAM_NLM_IMBLANCE_BASE + 0x001CUL)
#define DCAM_NLM_IMBLANCE_PARA4                         (DCAM_NLM_IMBLANCE_BASE + 0x0020UL)
#define DCAM_NLM_IMBLANCE_PARA5                         (DCAM_NLM_IMBLANCE_BASE + 0x0024UL)
#define DCAM_NLM_IMBLANCE_PARA6                         (DCAM_NLM_IMBLANCE_BASE + 0x0028UL)
#define DCAM_NLM_IMBLANCE_PARA7                         (DCAM_NLM_IMBLANCE_BASE + 0x002CUL)
#define DCAM_NLM_IMBLANCE_PARA8                         (DCAM_NLM_IMBLANCE_BASE + 0x0030UL)
#define DCAM_NLM_IMBLANCE_PARA9                         (DCAM_NLM_IMBLANCE_BASE + 0x0034UL)
#define DCAM_NLM_IMBLANCE_PARA10                        (DCAM_NLM_IMBLANCE_BASE + 0x0038UL)
#define DCAM_NLM_IMBLANCE_PARA11                        (DCAM_NLM_IMBLANCE_BASE + 0x003CUL)
#define DCAM_NLM_IMBLANCE_PARA12                        (DCAM_NLM_IMBLANCE_BASE + 0x0040UL)
#define DCAM_NLM_IMBLANCE_PARA13                        (DCAM_NLM_IMBLANCE_BASE + 0x0044UL)
#define DCAM_NLM_IMBLANCE_PARA14                        (DCAM_NLM_IMBLANCE_BASE + 0x0048UL)
#define DCAM_NLM_IMBLANCE_PARA15                        (DCAM_NLM_IMBLANCE_BASE + 0x004CUL)
#define DCAM_NLM_IMBLANCE_PARA16                        (DCAM_NLM_IMBLANCE_BASE + 0x0050UL)
#define DCAM_NLM_IMBLANCE_PARA17                        (DCAM_NLM_IMBLANCE_BASE + 0x0054UL)
#define DCAM_NLM_IMBLANCE_PARA18                        (DCAM_NLM_IMBLANCE_BASE + 0x0058UL)
#define DCAM_NLM_IMBLANCE_PARA19                        (DCAM_NLM_IMBLANCE_BASE + 0x005CUL)
#define DCAM_NLM_IMBLANCE_PARA20                        (DCAM_NLM_IMBLANCE_BASE + 0x0060UL)
#define DCAM_NLM_IMBLANCE_PARA21                        (DCAM_NLM_IMBLANCE_BASE + 0x0064UL)
#define DCAM_NLM_IMBLANCE_PARA22                        (DCAM_NLM_IMBLANCE_BASE + 0x0068UL)
#define DCAM_NLM_IMBLANCE_PARA23                        (DCAM_NLM_IMBLANCE_BASE + 0x006CUL)
#define DCAM_NLM_IMBLANCE_PARA24                        (DCAM_NLM_IMBLANCE_BASE + 0x0070UL)
#define DCAM_NLM_IMBLANCE_PARA25                        (DCAM_NLM_IMBLANCE_BASE + 0x0074UL)
#define DCAM_NLM_IMBLANCE_PARA26                        (DCAM_NLM_IMBLANCE_BASE + 0x0078UL)
#define DCAM_NLM_IMBLANCE_PARA27                        (DCAM_NLM_IMBLANCE_BASE + 0x007CUL)
#define DCAM_NLM_IMBLANCE_PARA28                        (DCAM_NLM_IMBLANCE_BASE + 0x0080UL)
#define DCAM_NLM_IMBLANCE_PARA29                        (DCAM_NLM_IMBLANCE_BASE + 0x0084UL)
#define DCAM_NLM_IMBLANCE_PARA30                        (DCAM_NLM_IMBLANCE_BASE + 0x0088UL)
#define DCAM_NLM_IMBLANCE_PARA31                        (DCAM_NLM_IMBLANCE_BASE + 0x008CUL)
#define DCAM_NLM_IMBLANCE_PARA32                        (DCAM_NLM_IMBLANCE_BASE + 0x0090UL)
#define DCAM_NLM_IMBLANCE_STATUS0                       (DCAM_NLM_IMBLANCE_BASE + 0x0094UL)

#define DCAM_CFA_BASE                                   (0x3400UL)
#define DCAM_CFA_NEW_CFG0                               (DCAM_CFA_BASE + 0x0010UL)
#define DCAM_CFA_INTP_CFG0                              (DCAM_CFA_BASE + 0x0014UL)
#define DCAM_CFA_INTP_CFG1                              (DCAM_CFA_BASE + 0x0018UL)
#define DCAM_CFA_INTP_CFG2                              (DCAM_CFA_BASE + 0x001CUL)
#define DCAM_CFA_INTP_CFG3                              (DCAM_CFA_BASE + 0x0020UL)
#define DCAM_CFA_INTP_CFG4                              (DCAM_CFA_BASE + 0x0024UL)
#define DCAM_CFA_INTP_CFG5                              (DCAM_CFA_BASE + 0x0028UL)
#define DCAM_CFA_CSS_CFG0                               (DCAM_CFA_BASE + 0x002CUL)
#define DCAM_CFA_CSS_CFG1                               (DCAM_CFA_BASE + 0x0030UL)
#define DCAM_CFA_CSS_CFG2                               (DCAM_CFA_BASE + 0x0034UL)
#define DCAM_CFA_CSS_CFG3                               (DCAM_CFA_BASE + 0x0038UL)
#define DCAM_CFA_CSS_CFG4                               (DCAM_CFA_BASE + 0x003CUL)
#define DCAM_CFA_CSS_CFG5                               (DCAM_CFA_BASE + 0x0040UL)
#define DCAM_CFA_CSS_CFG6                               (DCAM_CFA_BASE + 0x0044UL)
#define DCAM_CFA_CSS_CFG7                               (DCAM_CFA_BASE + 0x0048UL)
#define DCAM_CFA_CSS_CFG8                               (DCAM_CFA_BASE + 0x004CUL)
#define DCAM_CFA_CSS_CFG9                               (DCAM_CFA_BASE + 0x0050UL)
#define DCAM_CFA_CSS_CFG10                              (DCAM_CFA_BASE + 0x0054UL)
#define DCAM_CFA_CSS_CFG11                              (DCAM_CFA_BASE + 0x0058UL)
#define DCAM_CFA_GBUF_CFG                               (DCAM_CFA_BASE + 0x005CUL)

#define DCAM_CMC10_BASE                                 (0x3500UL)
#define DCAM_CMC10_PARAM                                (DCAM_CMC10_BASE + 0x0010UL)
#define DCAM_CMC10_MATRIX0                              (DCAM_CMC10_BASE + 0x0014UL)
#define DCAM_CMC10_MATRIX1                              (DCAM_CMC10_BASE + 0x0018UL)
#define DCAM_CMC10_MATRIX2                              (DCAM_CMC10_BASE + 0x001CUL)
#define DCAM_CMC10_MATRIX3                              (DCAM_CMC10_BASE + 0x0020UL)
#define DCAM_CMC10_MATRIX4                              (DCAM_CMC10_BASE + 0x0024UL)

#define DCAM_CCE_BASE                                   (0x4000UL)
#define DCAM_CCE_PARAM                                  (DCAM_CCE_BASE + 0x0010UL)
#define DCAM_CCE_MATRIX0                                (DCAM_CCE_BASE + 0x0014UL)
#define DCAM_CCE_MATRIX1                                (DCAM_CCE_BASE + 0x0018UL)
#define DCAM_CCE_MATRIX2                                (DCAM_CCE_BASE + 0x001CUL)
#define DCAM_CCE_MATRIX3                                (DCAM_CCE_BASE + 0x0020UL)
#define DCAM_CCE_MATRIX4                                (DCAM_CCE_BASE + 0x0024UL)
#define DCAM_CCE_SHIFT                                  (DCAM_CCE_BASE + 0x0028UL)

#define DCAM_FGAMMA10_BASE                              (0x3600UL)
#define DCAM_FGAMMA10_PARAM                             (DCAM_FGAMMA10_BASE + 0x0010UL)
#define DCAM_FGAMMA10_TABLE                             (0xC000UL)

#define DCAM_HISTS_BASE                                 (0x3700UL)
#define DCAM_HIST_ROI_CTRL0                             (DCAM_HISTS_BASE + 0x0010UL)
#define DCAM_HIST_ROI_CTRL1                             (DCAM_HISTS_BASE + 0x0014UL)
#define DCAM_HIST_ROI_START                             (DCAM_HISTS_BASE + 0x0018UL)
#define DCAM_HIST_ROI_END                               (DCAM_HISTS_BASE + 0x001CUL)
#define DCAM_HIST_ROI_STATUS                            (DCAM_HISTS_BASE + 0x0020UL)
#define DCAM_HIST_ROI_BASE_WADDR                        (DCAM_HISTS_BASE + 0x0024UL)
#define DCAM_HIST_ROI_BASE_WADDR1                       (DCAM_HISTS_BASE + 0x0028UL)
#define DCAM_HIST_ROI_BASE_WADDR2                       (DCAM_HISTS_BASE + 0x002CUL)
#define DCAM_HIST_ROI_BASE_WADDR3                       (DCAM_HISTS_BASE + 0x0030UL)

#define DCAM_YUV444TOYUV420_BASE                        (0x4100UL)
#define DCAM_YUV444TOYUV420_PARAM                       (DCAM_YUV444TOYUV420_BASE + 0x0010UL)
#define DCAM_YUV444TO420_IMAGE_WIDTH                    (DCAM_YUV444TOYUV420_BASE + 0x0014UL)

#define DCAM_SCL0_BASE                                  (0x4200UL)
#define DCAM_SCL0_CFG                                   (DCAM_SCL0_BASE + 0x0010UL)
#define DCAM_SCL0_SRC_SIZE                              (DCAM_SCL0_BASE + 0x0014UL)
#define DCAM_SCL0_DES_SIZE                              (DCAM_SCL0_BASE + 0x0018UL)
#define DCAM_SCL0_TRIM0_START                           (DCAM_SCL0_BASE + 0x001CUL)
#define DCAM_SCL0_TRIM0_SIZE                            (DCAM_SCL0_BASE + 0x0020UL)
#define DCAM_SCL0_IP                                    (DCAM_SCL0_BASE + 0x0024UL)
#define DCAM_SCL0_CIP                                   (DCAM_SCL0_BASE + 0x0028UL)
#define DCAM_SCL0_FACTOR                                (DCAM_SCL0_BASE + 0x002CUL)
#define DCAM_SCL0_TRIM1_START                           (DCAM_SCL0_BASE + 0x0030UL)
#define DCAM_SCL0_TRIM1_SIZE                            (DCAM_SCL0_BASE + 0x0034UL)
#define DCAM_SCL0_VER_IP                                (DCAM_SCL0_BASE + 0x0038UL)
#define DCAM_SCL0_VER_CIP                               (DCAM_SCL0_BASE + 0x003CUL)
#define DCAM_SCL0_VER_FACTOR                            (DCAM_SCL0_BASE + 0x0040UL)
#define DCAM_SCL0_DEBUG                                 (DCAM_SCL0_BASE + 0x0044UL)
#define DCAM_SCL0_HBLANK                                (DCAM_SCL0_BASE + 0x0048UL)
#define DCAM_SCL0_FRAME_CNT_CLR                         (DCAM_SCL0_BASE + 0x004CUL)
#define DCAM_SCL0_RES                                   (DCAM_SCL0_BASE + 0x0050UL)
#define DCAM_SCL0_BWD_PARA                              (DCAM_SCL0_BASE + 0x0054UL)

#define DCAM_SCL0_HOR_COEF0_Y                           (0x4710UL)
#define DCAM_SCL0_HOR_COEF0_UV                          (0x4790UL)
#define DCAM_SCL0_HOR_COEF1_Y                           (0x4810UL)
#define DCAM_SCL0_HOR_COEF1_UV                          (0x4890UL)
#define DCAM_SCL0_HOR_COEF2_Y                           (0x4910UL)
#define DCAM_SCL0_HOR_COEF2_UV                          (0x4990UL)
#define DCAM_SCL0_HOR_COEF3_Y                           (0x4A10UL)
#define DCAM_SCL0_HOR_COEF3_UV                          (0x4A90UL)
#define DCAM_SCL0_VER_COEF_Y                            (0xE0F0UL)
#define DCAM_SCL0_VER_COEF_UV                           (0xE5F0UL)

#define DCAM_CROP2_BASE                                 (0x4300UL)
#define DCAM_CROP2_CTRL                                 (DCAM_CROP2_BASE + 0x0010UL)
#define DCAM_CROP2_START                                (DCAM_CROP2_BASE + 0x0014UL)
#define DCAM_CROP2_SIZE                                 (DCAM_CROP2_BASE + 0x0018UL)

#define DCAM_BWD0_BASE                                  (0x4400UL)
#define DCAM_BWD0_PARAM                                 (DCAM_BWD0_BASE + 0x0010UL)

#define DCAM_BWD2_BASE                                  (0x4500UL)
#define DCAM_BWD2_PARAM                                 (DCAM_BWD2_BASE + 0x0010UL)

#define DCAM_DEC_ONLINE_BASE                            (0x4600UL)
#define DCAM_DEC_ONLINE_PARAM                           (DCAM_DEC_ONLINE_BASE + 0x0010UL)
#define DCAM_DEC_ONLINE_PARAM1                          (DCAM_DEC_ONLINE_BASE + 0x0014UL)
#define DCAM_DEC_ONLINE_STATUS0                         (DCAM_DEC_ONLINE_BASE + 0x0018UL)
/* skip 1~15 */
#define DCAM_DEC_ONLINE_STATUS16                        (DCAM_DEC_ONLINE_BASE + 0x0058UL)

#define DCAM_STORE0_BASE                                (0x5000UL)
#define DCAM_STORE0_PARAM                               (DCAM_STORE0_BASE + 0x0010UL)
#define DCAM_STORE0_SLICE_SIZE                          (DCAM_STORE0_BASE + 0x0014UL)
#define DCAM_STORE0_BORDER                              (DCAM_STORE0_BASE + 0x0018UL)
#define DCAM_STORE0_SLICE_Y_ADDR                        (DCAM_STORE0_BASE + 0x001CUL)
#define DCAM_STORE0_Y_PITCH                             (DCAM_STORE0_BASE + 0x0020UL)
#define DCAM_STORE0_SLICE_U_ADDR                        (DCAM_STORE0_BASE + 0x0024UL)
#define DCAM_STORE0_U_PITCH                             (DCAM_STORE0_BASE + 0x0028UL)
#define DCAM_STORE0_BORDER1                             (DCAM_STORE0_BASE + 0x002CUL)
#define DCAM_STORE0_V_PITCH                             (DCAM_STORE0_BASE + 0x0030UL)
#define DCAM_STORE0_READ_CTRL                           (DCAM_STORE0_BASE + 0x0034UL)
#define DCAM_STORE0_SHADOW_CLR_SEL                      (DCAM_STORE0_BASE + 0x0038UL)
#define DCAM_STORE0_SHADOW_CLR                          (DCAM_STORE0_BASE + 0x003CUL)

#define DCAM_STORE4_BASE                                (0x5100UL)
#define DCAM_STORE4_PARAM                               (DCAM_STORE4_BASE + 0x0010UL)
#define DCAM_STORE4_SLICE_SIZE                          (DCAM_STORE4_BASE + 0x0014UL)
#define DCAM_STORE4_BORDER                              (DCAM_STORE4_BASE + 0x0018UL)
#define DCAM_STORE4_SLICE_Y_ADDR                        (DCAM_STORE4_BASE + 0x001CUL)
#define DCAM_STORE4_Y_PITCH                             (DCAM_STORE4_BASE + 0x0020UL)
#define DCAM_STORE4_SLICE_U_ADDR                        (DCAM_STORE4_BASE + 0x0024UL)
#define DCAM_STORE4_U_PITCH                             (DCAM_STORE4_BASE + 0x0028UL)
#define DCAM_STORE4_SLICE_V_ADDR                        (DCAM_STORE4_BASE + 0x002CUL)
#define DCAM_STORE4_V_PITCH                             (DCAM_STORE4_BASE + 0x0030UL)
#define DCAM_STORE4_READ_CTRL                           (DCAM_STORE4_BASE + 0x0034UL)
#define DCAM_STORE4_SHADOW_CLR_SEL                      (DCAM_STORE4_BASE + 0x0038UL)
#define DCAM_STORE4_SHADOW_CLR                          (DCAM_STORE4_BASE + 0x003CUL)

#define DCAM_STORE_DEC_L1_BASE                          (0x5900UL)
#define DCAM_STORE_DEC_L2_BASE                          (0x5A00UL)
#define DCAM_STORE_DEC_L3_BASE                          (0x5B00UL)
#define DCAM_STORE_DEC_L4_BASE                          (0x5C00UL)

#define DCAM_STORE_DEC_PARAM                            (0x0010UL)
#define DCAM_STORE_DEC_SLICE_SIZE                       (0x0014UL)
#define DCAM_STORE_DEC_BORDER                           (0x0018UL)
#define DCAM_STORE_DEC_SLICE_Y_ADDR                     (0x001CUL)
#define DCAM_STORE_DEC_Y_PITCH                          (0x0020UL)
#define DCAM_STORE_DEC_SLICE_U_ADDR                     (0x0024UL)
#define DCAM_STORE_DEC_U_PITCH                          (0x0028UL)
#define DCAM_STORE_DEC_SLICE_V_ADDR                     (0x002CUL)
#define DCAM_STORE_DEC_V_PITCH                          (0x0030UL)
#define DCAM_STORE_DEC_READ_CTRL                        (0x0034UL)
#define DCAM_STORE_DEC_SHADOW_CLR_SEL                   (0x0038UL)
#define DCAM_STORE_DEC_SHADOW_CLR                       (0x003CUL)

#define DCAM_VCH1_BASE                                  (0x5200UL)
#define DCAM_VCH1_STATUS0                               (DCAM_VCH1_BASE + 0x0000UL)
#define DCAM_VC1_CONTROL                                (DCAM_VCH1_BASE + 0x0010UL)
#define DCAM_PDAF_BASE_WADDR                            (0x5214UL)

#define DCAM_VCH2_BASE                                  (0x5300UL)
#define DCAM_VCH2_FULL_STATUS0                          (DCAM_VCH2_BASE + 0x0000UL)
#define DCAM_VCH2_CONTROL                               (DCAM_VCH2_BASE + 0x0010UL)
#define DCAM_VCH2_BASE_WADDR                            (DCAM_VCH2_BASE + 0x0014UL)

#define DCAM_VCH3_BASE                                  (0x5400UL)
#define DCAM_VCH3_STATUS0                               (DCAM_VCH3_BASE + 0x0000UL)
#define DCAM_VCH3_CONTROL                               (DCAM_VCH3_BASE + 0x0010UL)
#define DCAM_VCH3_BASE_WADDR                            (DCAM_VCH3_BASE + 0x0014UL)

#define DCAM_FBC_RAW_BASE                               (0x5500UL)
#define DCAM_FBC_RAW_PARAM                              (DCAM_FBC_RAW_BASE + 0x0010UL)
#define DCAM_FBC_RAW_SLICE_SIZE                         (DCAM_FBC_RAW_BASE + 0x0014UL)
#define DCAM_FBC_RAW_BORDER                             (DCAM_FBC_RAW_BASE + 0x0018UL)
#define DCAM_FBC_RAW_SLICE_OFFSET                       (DCAM_FBC_RAW_BASE + 0x001CUL)
#define DCAM_FBC_RAW_SLICE_Y_ADDR                       (DCAM_FBC_RAW_BASE + 0x0020UL)
#define DCAM_FBC_RAW_SLICE_Y_HEADER                     (DCAM_FBC_RAW_BASE + 0x0024UL)
#define DCAM_FBC_RAW_SLICE_TILE_PITCH                   (DCAM_FBC_RAW_BASE + 0x0028UL)
#define DCAM_FBC_RAW_NFULL_LEVEL                        (DCAM_FBC_RAW_BASE + 0x002CUL)
#define DCAM_FBC_RAW_SLCIE_LOWBIT_ADDR                  (DCAM_FBC_RAW_BASE + 0x0030UL)
#define DCAM_FBC_RAW_LOWBIT_PITCH                       (DCAM_FBC_RAW_BASE + 0x0034UL)
#define DCAM_FBC_RAW_P0                                 (DCAM_FBC_RAW_BASE + 0x0038UL)
#define DCAM_FBC_RAW_STATUS0                            (DCAM_FBC_RAW_BASE + 0x0080UL)
#define DCAM_FBC_RAW_STATUS1                            (DCAM_FBC_RAW_BASE + 0x0084UL)
#define DCAM_FBC_RAW_STATUS2                            (DCAM_FBC_RAW_BASE + 0x0088UL)
#define DCAM_FBC_RAW_STATUS3                            (DCAM_FBC_RAW_BASE + 0x008CUL)
#define DCAM_FBC_RAW_STATUS4                            (DCAM_FBC_RAW_BASE + 0x0090UL)

#define DCAM_YUV_FBC_SCAL_BASE                          (0x5600UL)
#define DCAM_YUV_FBC_SCAL_PARAM                         (DCAM_YUV_FBC_SCAL_BASE + 0x0010UL)
#define DCAM_YUV_FBC_SCAL_SLICE_SIZE                    (DCAM_YUV_FBC_SCAL_BASE + 0x0014UL)
#define DCAM_YUV_FBC_SCAL_BORDER                        (DCAM_YUV_FBC_SCAL_BASE + 0x0018UL)
#define DCAM_YUV_FBC_SCAL_SLICE_PLOAD_OFFSET_ADDR       (DCAM_YUV_FBC_SCAL_BASE + 0x001CUL)
#define DCAM_YUV_FBC_SCAL_SLICE_PLOAD_BASE_ADDR         (DCAM_YUV_FBC_SCAL_BASE + 0x0020UL)
#define DCAM_YUV_FBC_SCAL_SLICE_HEADER_BASE_ADDR        (DCAM_YUV_FBC_SCAL_BASE + 0x0024UL)
#define DCAM_YUV_FBC_SCAL_TILE_PITCH                    (DCAM_YUV_FBC_SCAL_BASE + 0x0028UL)
#define DCAM_YUV_FBC_SCAL_NFULL_LEVEL                   (DCAM_YUV_FBC_SCAL_BASE + 0x002CUL)
#define DCAM_YUV_FBC_SCAL_P0                            (DCAM_YUV_FBC_SCAL_BASE + 0x0030UL)
#define DCAM_YUV_FBC_SCAL_STATUS0                       (DCAM_YUV_FBC_SCAL_BASE + 0x0080UL)
#define DCAM_YUV_FBC_SCAL_STATUS1                       (DCAM_YUV_FBC_SCAL_BASE + 0x0084UL)
#define DCAM_YUV_FBC_SCAL_STATUS2                       (DCAM_YUV_FBC_SCAL_BASE + 0x0088UL)
#define DCAM_YUV_FBC_SCAL_STATUS3                       (DCAM_YUV_FBC_SCAL_BASE + 0x008CUL)

#define DCAM_RAW_PATH_BASE                              (0x5800UL)
#define DCAM_RAW_PATH_STATUS0                           (DCAM_RAW_PATH_BASE + 0x0000UL)
#define DCAM_RAW_PATH_STATUS1                           (DCAM_RAW_PATH_BASE + 0x0004UL)
#define DCAM_RAW_PATH_STATUS2                           (DCAM_RAW_PATH_BASE + 0x0008UL)
#define DCAM_RAW_PATH_STATUS3                           (DCAM_RAW_PATH_BASE + 0x000CUL)
#define DCAM_RAW_PATH_CFG                               (DCAM_RAW_PATH_BASE + 0x0010UL)
#define DCAM_RAW_PATH_SIZE                              (DCAM_RAW_PATH_BASE + 0x0014UL)
#define DCAM_RAW_PATH_CROP_START                        (DCAM_RAW_PATH_BASE + 0x0018UL)
#define DCAM_RAW_PATH_CROP_SIZE                         (DCAM_RAW_PATH_BASE + 0x001CUL)
#define DCAM_RAW_PATH_BASE_WADDR                        (DCAM_RAW_PATH_BASE + 0x0020UL)

#define DCAM_RGBG_BASE                                  (0x1800UL)
#define ISP_RGBG_YRANDOM_PARAMETER0                     (DCAM_RGBG_BASE + 0x0010UL)
#define ISP_RGBG_RB                                     (DCAM_RGBG_BASE + 0x0014UL)
#define ISP_RGBG_G                                      (DCAM_RGBG_BASE + 0x0018UL)
#define ISP_RGBG_YRANDOM_PARAMETER1                     (DCAM_RGBG_BASE + 0x001CUL)
#define ISP_RGBG_YRANDOM_PARAMETER2                     (DCAM_RGBG_BASE + 0x0020UL)
#define ISP_RGBG_YUV_YRANDOM_STATUS0                    (DCAM_RGBG_BASE + 0x0024UL)
#define ISP_RGBG_YUV_YRANDOM_STATUS1                    (DCAM_RGBG_BASE + 0x0028UL)
#define ISP_RGBG_YUV_YRANDOM_STATUS2                    (DCAM_RGBG_BASE + 0x002CUL)

#define DCAM_GTM_BASE                                   (0x3000UL)
#define DCAM_GTM_GLB_CTRL                               (DCAM_GTM_BASE + 0x0010UL)
#define GTM_SLICE_LINE_POS                              (DCAM_GTM_BASE + 0x0014UL)
#define GTM_HIST_CTRL0                                  (DCAM_GTM_BASE + 0x0018UL)
#define GTM_HIST_CTRL1                                  (DCAM_GTM_BASE + 0x001CUL)
#define GTM_HIST_YMIN                                   (DCAM_GTM_BASE + 0x0020UL)
#define GTM_HIST_CTRL2                                  (DCAM_GTM_BASE + 0x0024UL)
#define GTM_HIST_CTRL3                                  (DCAM_GTM_BASE + 0x0028UL)
#define GTM_HIST_CTRL4                                  (DCAM_GTM_BASE + 0x002CUL)
#define GTM_HIST_CTRL5                                  (DCAM_GTM_BASE + 0x0030UL)
#define GTM_HIST_CTRL6                                  (DCAM_GTM_BASE + 0x0034UL)
#define GTM_HIST_CTRL7                                  (DCAM_GTM_BASE + 0x0038UL)
#define GTM_LOG_DIFF                                    (DCAM_GTM_BASE + 0x003CUL)
#define GTM_TM_YMIN_SMOOTH                              (DCAM_GTM_BASE + 0x0040UL)
#define GTM_TM_FILTER_DIST0                             (DCAM_GTM_BASE + 0x0044UL)
#define GTM_TM_FILTER_DIST8                             (DCAM_GTM_BASE + 0x0064UL)
#define GTM_TM_FILTER_DISTW0                            (DCAM_GTM_BASE + 0x0068UL)
#define GTM_TM_FILTER_DISTW6                            (DCAM_GTM_BASE + 0x0080UL)
#define GTM_TM_FILTER_RANGEW0                           (DCAM_GTM_BASE + 0x0084UL)
#define GTM_TM_FILTER_RANGEW20                          (DCAM_GTM_BASE + 0x00D4UL)
#define GTM_TM_RGB2YCOEFF0                              (DCAM_GTM_BASE + 0x00D8UL)
#define GTM_TM_RGB2YCOEFF1                              (DCAM_GTM_BASE + 0x00DCUL)
#define GTM_STATUS                                      (DCAM_GTM_BASE + 0x0060UL)
#define GTM_HIST_XPTS_0                                 (DCAM_GTM_BASE + 0x00E0UL)
#define GTM_HIST_CNT                                    (0xD000)

#define DCAM_PATH_STOP_MASK                             (0x2DFFUL)
#define DCAM_PATH_BUSY_MASK                             (0x2FFFUL)

#define MM_DCAM_FMCU_BASE                               (0x0000UL)
#define DCAM_FMCU_CTRL                                  (MM_DCAM_FMCU_BASE + 0x0014UL)
#define DCAM_FMCU_DDR_ADR                               (MM_DCAM_FMCU_BASE + 0x0018UL)
#define DCAM_FMCU_AHB_ARB                               (MM_DCAM_FMCU_BASE + 0x001CUL)
#define DCAM_FMCU_START                                 (MM_DCAM_FMCU_BASE + 0x0020UL)
#define DCAM_FMCU_TIME_OUT_THD                          (MM_DCAM_FMCU_BASE + 0x0024UL)
#define DCAM_FMCU_CMD_READY                             (MM_DCAM_FMCU_BASE + 0x0028UL)
#define DCAM_FMCU_ISP_REG_REGION                        (MM_DCAM_FMCU_BASE + 0x002CUL)
#define DCAM_FMCU_STOP                                  (MM_DCAM_FMCU_BASE + 0x0034UL)
#define DCAM_FMCU_RESERVED                              (MM_DCAM_FMCU_BASE + 0x0038UL)
#define DCAM_FMCU_SW_TRIGGER                            (MM_DCAM_FMCU_BASE + 0x003CUL)
#define DCAM_FMCU_CMD                                   (0xF030UL)

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
#define DCAM2_PATH0_BASE_WADDR                          (0x0080UL)
#define DCAM2_PATH1_BASE_WADDR                          (0x0084UL)
/* DCAM2 registers define end */

/* DCAM AXIM registers define 1 */
#define MM_DCAM_AXIMMU_BASE                             (0x0000UL)
#define AXIM_STATUS0                                    (MM_DCAM_AXIMMU_BASE + 0x0000UL)
#define AXIM_STATUS1                                    (MM_DCAM_AXIMMU_BASE + 0x0004UL)
#define AXIM_STATUS2                                    (MM_DCAM_AXIMMU_BASE + 0x0008UL)
#define AXIM_STATUS3                                    (MM_DCAM_AXIMMU_BASE + 0x000CUL)
#define AXIM_CTRL                                       (MM_DCAM_AXIMMU_BASE + 0x0010UL)
#define AXIM_DBG_STS                                    (MM_DCAM_AXIMMU_BASE + 0x0014UL)
#define CAP_SENSOR_CTRL                                 (MM_DCAM_AXIMMU_BASE + 0x0018UL)
#define AXIM_WORD_ENDIAN                                (MM_DCAM_AXIMMU_BASE + 0x001CUL)
#define DCAM_LBUF_SHARE_MODE                            (MM_DCAM_AXIMMU_BASE + 0x0020UL)
#define DCAM_SPARE_REG_0                                (MM_DCAM_AXIMMU_BASE + 0x0024UL)
#define AXIM_SPARE_REG_0                                (MM_DCAM_AXIMMU_BASE + 0x0028UL)
#define SPARE_REG_ICG                                   (MM_DCAM_AXIMMU_BASE + 0x002CUL)
#define IMG_FETCH_CTRL                                  (MM_DCAM_AXIMMU_BASE + 0x0030UL)
#define IMG_FETCH_SIZE                                  (MM_DCAM_AXIMMU_BASE + 0x0034UL)
#define IMG_FETCH_X                                     (MM_DCAM_AXIMMU_BASE + 0x0038UL)
#define IMG_FETCH_START                                 (MM_DCAM_AXIMMU_BASE + 0x003CUL)
#define IMG_FETCH_RADDR                                 (MM_DCAM_AXIMMU_BASE + 0x0040UL)
#define DCAM_PORT_CFG                                   (MM_DCAM_AXIMMU_BASE + 0x0044UL)
#define DCAM_PUM_CFG                                    (MM_DCAM_AXIMMU_BASE + 0x0048UL)
#define DCAM_AXIM_CFG                                   (MM_DCAM_AXIMMU_BASE + 0x004CUL)
#define AXIM_CNT_CFG                                    (MM_DCAM_AXIMMU_BASE + 0x0050UL)
#define AXIM_CNT_CLR                                    (MM_DCAM_AXIMMU_BASE + 0x0054UL)
#define ARBITER_QOS_CFG0                                (MM_DCAM_AXIMMU_BASE + 0x0058UL)
#define ARBITER_QOS_CFG1                                (MM_DCAM_AXIMMU_BASE + 0x005CUL)
#define AXIM_CMD_CFG                                    (MM_DCAM_AXIMMU_BASE + 0x0060UL)
#define DUMMY_SLAVE_CLOSE                               (MM_DCAM_AXIMMU_BASE + 0x0064UL)
#define AXIM_CNT_STATUS0                                (MM_DCAM_AXIMMU_BASE + 0x0068UL)
#define AXIM_CNT_STATUS1                                (MM_DCAM_AXIMMU_BASE + 0x006CUL)
#define AXIM_CNT_STATUS2                                (MM_DCAM_AXIMMU_BASE + 0x0070UL)
#define AXIM_CNT_STATUS3                                (MM_DCAM_AXIMMU_BASE + 0x0074UL)
#define AXIM_CNT_STATUS4                                (MM_DCAM_AXIMMU_BASE + 0x0078UL)
#define DCAM_FMCU_STATUS0                               (MM_DCAM_AXIMMU_BASE + 0x007CUL)
#define DCAM_FMCU_STATUS1                               (MM_DCAM_AXIMMU_BASE + 0x0080UL)
#define DCAM_FMCU_STATUS2                               (MM_DCAM_AXIMMU_BASE + 0x0084UL)
#define DCAM_FMCU_STATUS3                               (MM_DCAM_AXIMMU_BASE + 0x0088UL)
#define DCAM_FMCU_STATUS4                               (MM_DCAM_AXIMMU_BASE + 0x008CUL)
#define DCAM_FMCU_STATUS5                               (MM_DCAM_AXIMMU_BASE + 0x0090UL)
#define AXIM_CNT_STATUS5                                (MM_DCAM_AXIMMU_BASE + 0x0094UL)
#define AXIM_CNT_STATUS6                                (MM_DCAM_AXIMMU_BASE + 0x0098UL)
#define AXIM_CNT_STATUS7                                (MM_DCAM_AXIMMU_BASE + 0x009CUL)
#define AXIM_DEBUG_CFG1                                 (MM_DCAM_AXIMMU_BASE + 0x00A0UL)
#define AXIM_DEBUG_CFG2                                 (MM_DCAM_AXIMMU_BASE + 0x00A4UL)
#define DCAM_DUMMY_SLAVE                                (MM_DCAM_AXIMMU_BASE + 0x00A8UL)
#define DCAM_DUMMY_SLAVE_CFG                            (MM_DCAM_AXIMMU_BASE + 0x00ACUL)

/* DCAM AXIM registers define 2 */
#define MMU_EN                                          (0x0000UL)
#define MMU_UPDATE                                      (0x0004UL)
#define MMU_MIN_VPN                                     (0x0008UL)
#define MMU_VPN_RANGE                                   (0x000CUL)
#define MMU_PT_ADDR                                     (0x0010UL)
#define MMU_DEFAULT_PAGE                                (0x0014UL)
#define MMU_VAOR_ADDR_RD                                (0x0018UL)
#define MMU_VAOR_ADDR_WR                                (0x001CUL)
#define MMU_INV_ADDR_RD                                 (0x0020UL)
#define MMU_INV_ADDR_WR                                 (0x0024UL)
#define MMU_UNS_ADDR_RD                                 (0x0028UL)
#define MMU_UNS_ADDR_WR                                 (0x002CUL)
#define MMU_MISS_CNT                                    (0x0030UL)
#define MMU_PT_UPDATE_QOS                               (0x0034UL)
#define MMU_VERSION                                     (0x0038UL)
#define MMU_MIN_PPN1                                    (0x003CUL)
#define MMU_PPN_RANGE1                                  (0x0040UL)
#define MMU_MIN_PPN2                                    (0x0044UL)
#define MMU_PPN_RANGE2                                  (0x0048UL)
#define MMU_VPN_PAOR_RD                                 (0x004CUL)
#define MMU_VPN_PAOR_WR                                 (0x0050UL)
#define MMU_PPN_PAOR_RD                                 (0x0054UL)
#define MMU_PPN_PAOR_WR                                 (0x0058UL)
#define MMU_REG_AU_MANAGE                               (0x005CUL)
#define MMU_PAGE_RD_CH                                  (0x0060UL)
#define MMU_PAGE_WR_CH                                  (0x0064UL)
#define MMU_READ_PAGE_CMD_CNT                           (0x0068UL)
#define MMU_READ_PAGE_LATENCY_CNT                       (0x006CUL)
#define MMU_PAGE_MAX_LATENCY                            (0x0070UL)
#define MMU_STS                                         (0x0080UL)
#define MMU_EN_SHAD                                     (0x0084UL)
#define MMU_MIN_VPN_SHAD                                (0x0088UL)
#define MMU_VPN_RANGE_SHAD                              (0x008CUL)
#define MMU_PT_ADDR_SHAD                                (0x0090UL)
#define MMU_DEFAULT_PAGE_SHAD                           (0x0094UL)
#define MMU_PT_UPDATE_QOS_SHAD                          (0x0098UL)
#define MMU_MIN_PPN1_SHAD                               (0x009CUL)
#define MMU_PPN_RANGE1_SHAD                             (0x00A0UL)
#define MMU_MIN_PPN2_SHAD                               (0x00A4UL)
#define MMU_PPN_RANGE2_SHAD                             (0x00A8UL)

/* buffer addr map */
#define GTM_HIST_XPTS                                   (0x0600UL)
#define GTM_HIST_XPTS_CNT                               (0x0100UL)

#define LSC_WEI_TABLE_START                             (0x0900UL)
#define LSC_WEI_TABLE_SIZE                              (0x0400UL)
#define LSC_WEI_TABLE_MAX_NUM                           (0x00FFUL)


#define LSC_WEI_X_TABLE                                 (0xd200UL)
#define LSC_WEI_Y_TABLE                                 (0xd700UL)

#define RDS_COEF_TABLE_START                            (0x0d40UL)
#define RDS_COEF_TABLE_SIZE                             (0x00C0UL)

#define PDAF_CORR_TABLE_START                           (0xDC00UL)
#define PDAF_CORR_TABLE_SIZE                            (0x0200UL)

#define LSC_GRID_BUF_START0                             (0x4000UL)
#define LSC_GRID_BUF_START1                             (0x8000UL)
#define LSC_GRID_BUF_SIZE                               (0x4000UL)

#define LSC_GRID_BUF_START2                             (0xc000UL)
#define LSC_GRID_BUF_SIZE2                              (0x3000UL)

#define DCAM_LITE_APB_BASE                              (0x0000UL)
#define DCAM_LITE_IP_REVISION                           (DCAM_LITE_APB_BASE + 0x0000UL)
#define DCAM_LITE_LITE_STATUS0                          (DCAM_LITE_APB_BASE + 0x0004UL)
#define DCAM_LITE_LITE_STATUS1                          (DCAM_LITE_APB_BASE + 0x0008UL)
#define DCAM_LITE_LITE_STATUS2                          (DCAM_LITE_APB_BASE + 0x000CUL)
#define DCAM_LITE_CONTROL                               (DCAM_LITE_APB_BASE + 0x0010UL)
#define DCAM_LITE_CFG                                   (DCAM_LITE_APB_BASE + 0x0014UL)
#define DCAM_LITE_PATH1_BASE_WADDR0                     (DCAM_LITE_APB_BASE + 0x0018UL)
#define DCAM_LITE_PATH0_BASE_WADDR                      (DCAM_LITE_APB_BASE + 0x001cUL)
#define DCAM_LITE_INT_MASK                              (DCAM_LITE_APB_BASE + 0x0020UL)
#define DCAM_LITE_INT_EN                                (DCAM_LITE_APB_BASE + 0x0024UL)
#define DCAM_LITE_INT_CLR                               (DCAM_LITE_APB_BASE + 0x0028UL)
#define DCAM_LITE_INT_RAW                               (DCAM_LITE_APB_BASE + 0x002CUL)
#define DCAM_LITE_PATH_FULL                             (DCAM_LITE_APB_BASE + 0x0030UL)
#define DCAM_LITE_PATH0_STATUS                          (DCAM_LITE_APB_BASE + 0x0034UL)
#define DCAM_LITE_PATH1_STATUS                          (DCAM_LITE_APB_BASE + 0x0038UL)
#define DCAM_LITE_PATH_BUSY                             (DCAM_LITE_APB_BASE + 0x003CUL)
#define DCAM_LITE_PATH_STOP                             (DCAM_LITE_APB_BASE + 0x0040UL)
#define DCAM_LITE_PATH_ENDIAN                           (DCAM_LITE_APB_BASE + 0x0044UL)
#define DCAM_LITE_SPARE_CTRL                            (DCAM_LITE_APB_BASE + 0x0048UL)
#define DCAM_LITE_MIPI_CAP_CFG                          (DCAM_LITE_APB_BASE + 0x004CUL)
#define DCAM_LITE_MIPI_CAP_FRM_CTRL                     (DCAM_LITE_APB_BASE + 0x0050UL)
#define DCAM_LITE_MIPI_CAP_FRM_CLR                      (DCAM_LITE_APB_BASE + 0x0054UL)
#define DCAM_LITE_MIPI_CAP_START                        (DCAM_LITE_APB_BASE + 0x0058UL)
#define DCAM_LITE_MIPI_CAP_END                          (DCAM_LITE_APB_BASE + 0x005CUL)
#define DCAM_LITE_IMAGE_DT_VC_CONTROL                   (DCAM_LITE_APB_BASE + 0x0060UL)
#define DCAM_LITE_MIPI_REDUNDANT                        (DCAM_LITE_APB_BASE + 0x0064UL)
#define DCAM_LITE_MIPI_CAP_WORD_CNT                     (DCAM_LITE_APB_BASE + 0x0068UL)
#define DCAM_LITE_MIPI_CAP_HEIGHT_SIZE                  (DCAM_LITE_APB_BASE + 0x006CUL)

#define DCAM_LITE_AXIMMU_BASE                           (0x8000UL)
#define DCAM_LITE_AXIM_STATUS0                          (DCAM_LITE_AXIMMU_BASE + 0x0000UL)
#define DCAM_LITE_AXIM_CTRL                             (DCAM_LITE_AXIMMU_BASE + 0x0004UL)
#define DCAM_LITE_AXIM_DBG_STS                          (DCAM_LITE_AXIMMU_BASE + 0x0008UL)
#define DCAM_LITE_CAP_SENSOR_CTRL                       (DCAM_LITE_AXIMMU_BASE + 0x000CUL)
#define DCAM_LITE_AXIM_WORD_ENDIAN                      (DCAM_LITE_AXIMMU_BASE + 0x0010UL)
#define DCAM_LITE_DCAM_SPARE_REG_0                      (DCAM_LITE_AXIMMU_BASE + 0x0014UL)
#define DCAM_LITE_AXIM_SPARE_REG_0                      (DCAM_LITE_AXIMMU_BASE + 0x0018UL)
#define DCAM_LITE_SPARE_REG_ICG                         (DCAM_LITE_AXIMMU_BASE + 0x001CUL)
#define DCAM_LITE_PROT_CFG                              (DCAM_LITE_AXIMMU_BASE + 0x0020UL)
#define DCAM_LITE_PMU_CFG                               (DCAM_LITE_AXIMMU_BASE + 0x0024UL)

/*
 * DCAM register map range of qogirn6pro
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

#define DCAM_BASE(idx)                                  (g_dcam_regbase[idx])
#define DCAM_AXIM_BASE(idx)                             (g_dcam_aximbase[idx])
#define DCAM_FMCU_BASE                                  (g_dcam_fmcubase)
/* TODO: implement mmu */
#define DCAM_MMU_BASE                                   (g_dcam_mmubase)
#define DCAM_PHYS_ADDR(idx)                             (g_dcam_phys_base[idx])
#define DCAM_GET_REG(idx, reg)                          (DCAM_PHYS_ADDR(idx) + (reg))

#define DCAM_REG_WR(idx, reg, val) ({                                                \
	unsigned long __flags;                                                       \
	spin_lock_irqsave(&g_reg_wr_lock, __flags);                                  \
	(REG_WR(DCAM_BASE(idx)+(reg), (val)));                                   \
	spin_unlock_irqrestore(&g_reg_wr_lock, __flags);                                 \
})

#define DCAM_REG_RD(idx, reg) ({                                          \
	unsigned long __flags;                                                       \
	uint32_t val;                                                       \
	spin_lock_irqsave(&g_reg_wr_lock, __flags);                                  \
	val = (REG_RD(DCAM_BASE(idx)+(reg)));                                          \
	spin_unlock_irqrestore(&g_reg_wr_lock, __flags);                                 \
	val;                                        \
})

#define DCAM_REG_MWR(idx, reg, msk, val) ({                               \
	unsigned long __flags;                                                       \
	spin_lock_irqsave(&g_reg_wr_lock, __flags);                                  \
	(REG_WR(DCAM_BASE(idx)+(reg), ((val) & (msk)) | (REG_RD(DCAM_BASE(idx)+(reg)) & (~(msk)))));            \
	spin_unlock_irqrestore(&g_reg_wr_lock, __flags);                                 \
})

#define DCAM_AXIM_WR(id, reg, val) ({                                 \
	unsigned long __flags;                                                       \
	spin_lock_irqsave(&g_reg_wr_lock, __flags);                                  \
	(REG_WR(DCAM_AXIM_BASE(id)+(reg), (val)));                           \
	spin_unlock_irqrestore(&g_reg_wr_lock, __flags);                                 \
})

#define DCAM_AXIM_RD(id, reg) ({                                         \
	unsigned long __flags;                                                       \
	uint32_t val;                                                       \
	spin_lock_irqsave(&g_reg_wr_lock, __flags);                                  \
	val = (REG_RD(DCAM_AXIM_BASE(id)+(reg)));                                          \
	spin_unlock_irqrestore(&g_reg_wr_lock, __flags);                                 \
	val;                                        \
})

#define DCAM_AXIM_MWR(id, reg, msk, val) ({                               \
	unsigned long __flags;                                                       \
	spin_lock_irqsave(&g_reg_wr_lock, __flags);                                  \
	(REG_WR(DCAM_AXIM_BASE(id)+(reg), ((val) & (msk)) | (REG_RD(DCAM_AXIM_BASE(id)+(reg)) & (~(msk)))));            \
	spin_unlock_irqrestore(&g_reg_wr_lock, __flags);                                 \
})

#define DCAM_MMU_WR(reg, val) ({                                          \
	unsigned long __flags;                                                       \
	spin_lock_irqsave(&g_reg_wr_lock, __flags);                                  \
	(REG_WR(DCAM_MMU_BASE+(reg), (val)));                           \
	spin_unlock_irqrestore(&g_reg_wr_lock, __flags);                                 \
})

#define DCAM_MMU_RD(reg) ({                                        \
	unsigned long __flags;                                                       \
	uint32_t val;                                                       \
	spin_lock_irqsave(&g_reg_wr_lock, __flags);                                  \
	val = (REG_RD(DCAM_MMU_BASE+(reg)));                                          \
	spin_unlock_irqrestore(&g_reg_wr_lock, __flags);                                 \
	val;                                        \
})

#define DCAM_MMU_MWR(reg, msk, val) ({                             \
	unsigned long __flags;                                                       \
	spin_lock_irqsave(&g_reg_wr_lock, __flags);                                  \
	(REG_WR(DCAM_MMU_BASE+(reg), ((val) & (msk)) | (REG_RD(DCAM_MMU_BASE+(reg)) & (~(msk)))));            \
	spin_unlock_irqrestore(&g_reg_wr_lock, __flags);                                 \
})

#define DCAM_FMCU_WR(reg, val) ({                                \
	unsigned long __flags;                                                       \
	spin_lock_irqsave(&g_reg_wr_lock, __flags);                                  \
	(REG_WR(DCAM_FMCU_BASE+(reg), (val)));                           \
	spin_unlock_irqrestore(&g_reg_wr_lock, __flags);                                 \
})

#define DCAM_FMCU_RD(reg) ({                                              \
	unsigned long __flags;                                                       \
	uint32_t val;                                                       \
	spin_lock_irqsave(&g_reg_wr_lock, __flags);                                                            \
	val = (REG_RD(DCAM_FMCU_BASE+(reg)));                                          \
	spin_unlock_irqrestore(&g_reg_wr_lock, __flags);                                 \
	val;                                        \
})

#define DCAM_FMCU_MWR(reg, msk, val) ({                             \
	unsigned long __flags;                                                       \
	spin_lock_irqsave(&g_reg_wr_lock, __flags);                                  \
	(REG_WR(DCAM_FMCU_BASE+(reg), ((val) & (msk)) | (REG_RD(DCAM_FMCU_BASE+(reg)) & (~(msk)))));            \
	spin_unlock_irqrestore(&g_reg_wr_lock, __flags);                                 \
})

/* TODO: add DCAM0/1 lsc grid table mapping */

#endif /* _DCAM_REG_H_ */
