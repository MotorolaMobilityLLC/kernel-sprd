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

#ifndef _REG_ISP_H_
#define _REG_ISP_H_

#include "dcam_reg.h"

#define ISP_MAX_COUNT                  2

extern unsigned long s_isp_regbase[ISP_MAX_COUNT];
extern unsigned long isp_phys_base[ISP_MAX_COUNT];
extern unsigned long *isp_cfg_poll_addr[ISP_MAX_COUNT][4];
extern unsigned long *isp_cfg_work_poll_addr[ISP_MAX_COUNT][4];

#define ISP_PHYS_ADDR(idx)             (isp_phys_base[idx])

/*isp sub block: FMCU*/
#define ISP_FMCU_STATUS0                (0xF000UL)
#define ISP_FMCU_STATUS1                (0xF004UL)
#define ISP_FMCU_STATUS2                (0xF008UL)
#define ISP_FMCU_STATUS3                (0xF00CUL)
#define ISP_FMCU_STATUS4                (0xF010UL)
#define ISP_FMCU_CTRL                    (0xF014UL)
#define ISP_FMCU_DDR_ADR                (0xF018UL)
#define ISP_FMCU_AHB_ARB                (0xF01CUL)
#define ISP_FMCU_START                    (0xF020UL)
#define ISP_FMCU_TIME_OUT_THD            (0xF024UL)
#define ISP_FMCU_CMD_READY            (0xF028UL)
#define ISP_FMCU_ISP_REG_REGION        (0xF02CUL)
#define ISP_FMCU_STOP                    (0xF034UL)
#define ISP_FMCU_SW_TRIGGER            (0xF03CUL)

//fmcu0 & fmcu1 use 0xf030
#define ISP_FMCU_CMD                    (0xF030UL)

/*isp sub block: FMCU1*/
#define ISP_FMCU1_STATUS0                (0xF100UL)
#define ISP_FMCU1_STATUS1                (0xF104UL)
#define ISP_FMCU1_STATUS2                (0xF108UL)
#define ISP_FMCU1_STATUS3                (0xF10CUL)
#define ISP_FMCU1_STATUS4                (0xF110UL)
#define ISP_FMCU1_CTRL                    (0xF114UL)
#define ISP_FMCU1_DDR_ADR                (0xF118UL)
#define ISP_FMCU1_AHB_ARB                (0xF11CUL)
#define ISP_FMCU1_START                (0xF120UL)
#define ISP_FMCU1_TIME_OUT_THD        (0xF124UL)
#define ISP_FMCU1_CMD_READY            (0xF128UL)
#define ISP_FMCU1_ISP_REG_REGION        (0xF12CUL)
#define ISP_FMCU1_CMD                    (0xF130UL)
#define ISP_FMCU1_STOP                    (0xF134UL)
#define ISP_FMCU1_SW_TRIGGER            (0xF13CUL)

/*isp sub block: Interrupt*/
#define ISP_P0_INT_BASE                    (0x0000UL)
#define ISP_P1_INT_BASE                    (0x0D00UL)
#define ISP_C0_INT_BASE                    (0x0E00UL)
#define ISP_C1_INT_BASE                    (0x0F00UL)

#define ISP_INT_EN0                        (0x0010UL)
#define ISP_INT_CLR0                    (0x0014UL)
#define ISP_INT_RAW0                    (0x0018UL)
#define ISP_INT_INT0                    (0x001CUL)
#define ISP_INT_EN1                        (0x0020UL)
#define ISP_INT_CLR1                    (0x0024UL)
#define ISP_INT_RAW1                    (0x0028UL)
#define ISP_INT_INT1                    (0x002CUL)
#define ISP_INT_ALL_DONE_CTRL            (0x0030UL)
#define ISP_INT_SKIP_CTRL                (0x0034UL)
#define ISP_INT_SKIP_CTRL1                (0x0038UL)
#define ISP_INT_ALL_DONE_SRC_CTRL        (0x003CUL)

/*isp sub block: Fetch*/
#define ISP_FETCH_STATUS0                       (0x0100UL)
#define ISP_FETCH_STATUS1                       (0x0104UL)
#define ISP_FETCH_STATUS2                       (0x0108UL)
#define ISP_FETCH_STATUS3                       (0x010CUL)
#define ISP_FETCH_PARAM0                (0x0110UL)
#define ISP_FETCH_MEM_SLICE_SIZE        (0x0114UL)
#define ISP_FETCH_SLICE_Y_ADDR            (0x0118UL)
#define ISP_FETCH_Y_PITCH                       (0x011CUL)
#define ISP_FETCH_SLICE_U_ADDR        (0x0120UL)
#define ISP_FETCH_U_PITCH                (0x0124UL)
#define ISP_FETCH_SLICE_V_ADDR        (0x0128UL)
#define ISP_FETCH_V_PITCH                (0x012CUL)
#define ISP_FETCH_MIPI_PARAM            (0x0130UL)
#define ISP_FETCH_LINE_DLY_CTRL         (0x0134UL)
#define ISP_FETCH_PARAM1                (0x0138UL)
#define ISP_FETCH_START                    (0x013CUL)
#define ISP_FETCH_STATUS4                       (0x0140UL)
#define ISP_FETCH_STATUS5                       (0x0144UL)
#define ISP_FETCH_STATUS6                       (0x0148UL)
#define ISP_FETCH_STATUS7                       (0x014CUL)
#define ISP_FETCH_STATUS8                       (0x0150UL)
#define ISP_FETCH_STATUS9                       (0x0154UL)
#define ISP_FETCH_STATUS10                      (0x0158UL)
#define ISP_FETCH_STATUS11                      (0x015CUL)
#define ISP_FETCH_STATUS12                      (0x0160UL)
#define ISP_FETCH_STATUS13                      (0x0164UL)
#define ISP_FETCH_STATUS14                (0x0168UL)
#define ISP_FETCH_STATUS15                (0x016CUL)
#define ISP_FETCH_MIPI_STATUS            (0x0170UL)

/* fetch fbd */
#define    ISP_FBD_RAW_SEL                        (0x0C10UL)
#define    ISP_FBD_RAW_SLICE_SIZE              (0x0C14UL)
#define    ISP_FBD_RAW_PARAM0                    (0x0C18UL)
#define    ISP_FBD_RAW_PARAM1                    (0x0C1CUL)
#define    ISP_FBD_RAW_PARAM2                    (0x0C20UL)
#define    ISP_FBD_RAW_PARAM3                    (0x0C24UL)
#define    ISP_FBD_RAW_PARAM4                    (0x0C28UL)
#define    ISP_FBD_RAW_PARAM5                    (0x0C2CUL)
#define    ISP_FBD_RAW_PARAM6                    (0x0C30UL)
#define    ISP_FBD_RAW_LOW_PARAM0           (0x0C34UL)
#define    ISP_FBD_RAW_LOW_PARAM1           (0x0C38UL)
#define    ISP_FBD_RAW_START                    (0x0C3CUL)
#define    ISP_FBD_RAW_HBLANK                (0x0C40UL)
#define    ISP_FBD_RAW_LOW_4BIT_PARAM0            (0x0C44UL)
#define    ISP_FBD_RAW_LOW_4BIT_PARAM1            (0x0C48UL)

/*isp sub block: Store*/
#define ISP_STORE_BASE                    (0x0200UL)
#define ISP_STORE_PRE_CAP_BASE        (0xC100UL)
#define ISP_STORE_VID_BASE                (0xD100UL)
#define ISP_STORE_THU_BASE                (0xE100UL)

/*used for store_out, store_Pre_Cap and store_Vid*/
#define ISP_STORE_STATUS0                    (0x0000UL)
#define ISP_STORE_STATUS1                       (0x0004UL)
#define ISP_STORE_STATUS2                         (0x0008UL)
#define ISP_STORE_STATUS3                    (0x000CUL)
#define ISP_STORE_PARAM                         (0x0010UL)
#define ISP_STORE_SLICE_SIZE                   (0x0014UL)
#define ISP_STORE_BORDER                       (0x0018UL)
#define ISP_STORE_SLICE_Y_ADDR               (0x001CUL)
#define ISP_STORE_Y_PITCH                          (0x0020UL)
#define ISP_STORE_SLICE_U_ADDR               (0x0024UL)
#define ISP_STORE_U_PITCH                      (0x0028UL)
#define ISP_STORE_SLICE_V_ADDR               (0x002CUL)
#define ISP_STORE_V_PITCH                       (0x0030UL)
#define ISP_STORE_READ_CTRL                   (0x0034UL)
#define ISP_STORE_SHADOW_CLR_SEL            (0x0038UL)
#define ISP_STORE_SHADOW_CLR                   (0x003CUL)
#define ISP_STORE_TRIM_STATUS                   (0x0040UL)

/* store fbc */
#define ISP_VID_FBC_STORE_BASE (0xD300UL)
#define ISP_CAP_FBC_STORE_BASE (0xC300UL)
#define ISP_FBC_STORE_PARAM (0x0010UL)
#define ISP_FBC_STORE_SLICE_SIZE (0x0014UL)
#define ISP_FBC_STORE_SLICE_HEADER_OFFSET_ADDR (0x001CUL)
#define ISP_FBC_STORE_BORDER (0x0018UL)
#define ISP_FBC_STORE_SLICE_Y_ADDR (0x0020UL)
#define ISP_FBC_STORE_SLICE_Y_HEADER (0x0024UL)
#define ISP_FBC_STORE_TILE_PITCH (0x0028UL)
#define ISP_FBC_STORE_P0 (0x0030UL)

/*isp sub block: SCALER    PRE*/
#define ISP_SCALER_PRE_STATUS0                (0xC000UL)
#define ISP_SCALER_PRE_CFG                    (0xC010UL) //SharkL2 (0xC014UL)
#define ISP_SCALER_PRE_TRIM1_SIZE            (0xC034UL) //SharkL2 (0xC044UL)

/*isp sub block: SCALER    VID*/
#define ISP_SCALER_VID_STATUS0                (0xD000UL)
#define ISP_SCALER_VID_CFG                    (0xD010UL) //SharkL2 (0xD014UL)
#define ISP_SCALER_VID_TRIM1_SIZE            (0xD034UL) //SharkL2 (0xD044UL)

/*isp sub block: SCALER    CAP*/
#define ISP_SCALER_CAP_STATUS0                (0xC000UL)
#define ISP_SCALER_CAP_CFG                    (0xC010UL)
#define ISP_SCALER_CAP_TRIM1_SIZE            (0xC034UL)

/*isp sub block: YUV_SCALER*/
#define ISP_YUV_SCALER_PRE_CAP_BASE        (0xC000UL)
#define ISP_YUV_SCALER_VID_BASE            (0xD000UL)
#define ISP_YUV_SCALER_THUMB_BASE            (0xE000UL)

#define ISP_YUV_SCALER_CFG                    (0x0010UL)
#define ISP_YUV_SCALER_SRC_SIZE                (0x0014UL)
#define ISP_YUV_SCALER_DES_SIZE                (0x0018UL)
#define ISP_YUV_SCALER_TRIM0_START            (0x001CUL)
#define ISP_YUV_SCALER_TRIM0_SIZE            (0x0020UL)
#define ISP_YUV_SCALER_IP                    (0x0024UL)
#define ISP_YUV_SCALER_CIP                    (0x0028UL)
#define ISP_YUV_SCALER_FACTOR                (0x002CUL)
#define ISP_YUV_SCALER_TRIM1_START            (0x0030UL)
#define ISP_YUV_SCALER_TRIM1_SIZE            (0x0034UL)
#define ISP_YUV_SCALER_IP_VER                (0x0038UL)
#define ISP_YUV_SCALER_CIP_VER                (0x003CUL)
#define ISP_YUV_SCALER_FACTOR_VER            (0x0040UL)
#define ISP_YUV_SCALER_DEBUG                (0x0044UL)
#define ISP_YUV_SCALER_HBLANK                   (0x0048UL)
#define ISP_YUV_SCALER_FRAME_CNT_CLR        (0x004CUL)
#define ISP_YUV_SCALER_RES                    (0x0050UL)
#define ISP_YUV_SCALER_SHRINK                (0x0054UL)
#define ISP_YUV_SCALER_EFFECT                (0x0058UL)
#define ISP_YUV_SCALER_REGULAR                (0x005CUL)
#define ISP_YUV_SCALER_VID_LUMA_HCOEF_BUF0            (0x38100UL)
#define ISP_YUV_SCALER_VID_CHROMA_HCOEF_BUF0            (0x38300UL)
#define ISP_YUV_SCALER_VID_LUMA_VCOEF_BUF0            (0x384F0UL)
#define ISP_YUV_SCALER_VID_CHROMA_VCOEF_BUF0            (0x38AF0UL)
#define ISP_YUV_SCALER_VID_LUMA_HCOEF_BUF1            (0x38200UL)
#define ISP_YUV_SCALER_VID_CHROMA_HCOEF_BUF1            (0x38380UL)
#define ISP_YUV_SCALER_VID_LUMA_VCOEF_BUF1            (0x387F0UL)
#define ISP_YUV_SCALER_VID_CHROMA_VCOEF_BUF1            (0x38DF0UL)
#define ISP_YUV_SCALER_PRE_CAP_LUMA_HCOEF_BUF0        (0x39100UL)
#define ISP_YUV_SCALER_PRE_CAP_CHROMA_HCOEF_BUF0    (0x39300UL)
#define ISP_YUV_SCALER_PRE_CAP_LUMA_VCOEF_BUF0        (0x394F0UL)
#define ISP_YUV_SCALER_PRE_CAP_CHROMA_VCOEF_BUF0    (0x39AF0UL)
#define ISP_YUV_SCALER_PRE_CAP_LUMA_HCOEF_BUF1        (0x39200UL)
#define ISP_YUV_SCALER_PRE_CAP_CHROMA_HCOEF_BUF1    (0x39380UL)
#define ISP_YUV_SCALER_PRE_CAP_LUMA_VCOEF_BUF1        (0x397F0UL)
#define ISP_YUV_SCALER_PRE_CAP_CHROMA_VCOEF_BUF1    (0x39DF0UL)

/*isp sub block: dispatch*/
#define ISP_DISPATCH_STATUS_CH0               (0x0300UL)
#define ISP_DISPATCH_STATUS_DBG_CH0           (0x0304UL)
#define ISP_DISPATCH_STATUS_COMM               (0x0308UL)
#define ISP_DISPATCH_STATUS1_CH0               (0x030CUL)
#define ISP_DISPATCH_CH0_BAYER                       (0x0310UL)
#define ISP_DISPATCH_CH0_SIZE                   (0x0314UL)
#define ISP_DISPATCH_DLY                       (0x0318UL)
#define ISP_DISPATCH_HW_CTRL_CH0               (0x031CUL)
#define ISP_DISPATCH_DLY1                       (0x0320UL)
#define ISP_PIPE_BUF_CTRL_CH0                   (0x0324UL)

/*isp sub block: arbiter*/
#define ISP_ARBITER_WR_STATUS                (0x0400UL)
#define ISP_ARBITER_RD_STATUS                (0x0404UL)
#define ISP_ARBITER_ENDIAN_COMM            (0x0410UL)
#define ISP_ARBITER_ENDIAN_CH0            (0x0414UL)
#define ISP_ARBITER_ENDIAN_CH1            (0x0418UL)
#define ISP_ARBITER_WR_PARAM0            (0x041CUL)
#define ISP_ARBITER_WR_PARAM1            (0x0420UL)
#define ISP_ARBITER_WR_PARAM2            (0x0424UL)
#define ISP_ARBITER_WR_PARAM3            (0x0428UL)
#define ISP_ARBITER_WR_PARAM4            (0x042CUL)
#define ISP_ARBITER_RD_PARAM0                (0x043CUL)
#define ISP_ARBITER_RD_PARAM1                (0x0440UL)
#define ISP_ARBITER_RD_PARAM2                (0x0444UL)
#define ISP_ARBITER_RD_PARAM3                (0x0448UL)
#define ISP_ARBITER_RD_PARAM4                (0x044CUL)
#define ISP_ARBITER_RD_PARAM5                (0x0450UL)
#define ISP_ARBITER_RD_PARAM6                (0x0454UL)

/*isp sub block: axi*/
#define ISP_AXI_WR_MASTER_STATUS               (0x0500UL)
#define ISP_AXI_RD_MASTER_STATUS              (0x0504UL)
#define ISP_AXI_ITI2AXIM_CTRL                   (0x0510UL)
#define ISP_AXI_ARBITER_WQOS                   (0x0514UL)
#define ISP_AXI_ARBITER_RQOS                   (0x0518UL)
#define ISP_AXI_ISOLATION                    (0x051CUL)
#define ISP_AXI_MONITOR_PARAM0            (0x0520UL)
#define ISP_AXI_MONITOR_PARAM1            (0x0524UL)
#define ISP_AXI_MONITOR_PARAM2            (0x0528UL)
#define ISP_AXI_MONITOR_PARAM3            (0x052CUL)
#define ISP_AXI_STATUS0                        (0x0530UL)
#define ISP_AXI_STATUS1                        (0x0534UL)
#define ISP_AXI_STATUS2                        (0x0538UL)
#define ISP_AXI_STATUS3                        (0x053CUL)


/*isp sub block: common*/
#define ISP_COMMON_VERSION                (0x0700UL)
#define ISP_COMMON_STATUS0                (0x0704UL)
#define ISP_COMMON_STATUS1                (0x0708UL)
#define ISP_COMMON_SPACE_SEL                (0x0710UL)
#define ISP_COMMON_SCL_PATH_SEL            (0x0714UL)
#define ISP_COMMON_FMCU_PATH_SEL            (0x0718UL)
#define ISP_COMMON_GCLK_CTRL_0            (0x071CUL)
#define ISP_COMMON_GCLK_CTRL_1            (0x0720UL)
#define ISP_COMMON_GCLK_CTRL_2            (0x0724UL)
#define ISP_COMMON_GCLK_CTRL_3            (0x0728UL)
#define ISP_COMMON_FMCU_PATH_SEL1        (0x072CUL)
#define ISP_COMMON_SHADOW_CTRL_CH0        (0x0730UL)
#define ISP_COMMON_PMU_RAM_MASK            (0x0734UL)
#define ISP_WORK_CTRL                        (0x0738UL)
#define ISP_BLOCK_MODE                        (0x073CUL)

/*isp sub block: Pre Global Gain*/
#define ISP_PGG_STATUS                           (0x1000UL)
#define ISP_PGG_PARAM                              (0x1010UL)


#define ISP_YUV_YRANDOM_PARAM1            (0x5E10UL)
#define ISP_YUV_YRANDOM_PARAM2            (0x5E14UL)
#define ISP_YUV_YRANDOM_PARAM3            (0x5E18UL)
#define ISP_YUV_YRANDOM_STATUS1            (0x5E1CUL)
#define ISP_YUV_YRANDOM_STATUS2            (0x5E20UL)
#define ISP_YUV_YRANDOM_INIT                   (0x5E24UL)

/*isp sub block: POST BLC*/
#define ISP_POST_BLC_STATUS                    (0x1300UL)
#define ISP_POST_BLC_PARA                       (0x1310UL) //sharkl2 (0x1314UL)
#define ISP_POST_BLC_B_PARA_R_B            (0x1314UL) //sharkl2 (0x1318UL)
#define ISP_POST_BLC_PARA_G                    (0x1318UL) //sharkl2 (0x131CUL)

/*isp sub block: BWU*/
#define ISP_BWU_PARAM                (0x1A10UL)

/*isp sub block: GrGb Curve - GC*/
#define ISP_GRGB_STATUS                (0x1B00UL)
#define ISP_GRGB_CTRL                     (0x1B10UL)
#define ISP_GRGB_CFG0                    (0x1B14UL)
#define ISP_GRGB_LUM_FLAT_T               (0x1B18UL)
#define ISP_GRGB_LUM_FLAT_R               (0x1B1CUL)
#define ISP_GRGB_LUM_EDGE_T              (0x1B20UL)
#define ISP_GRGB_LUM_EDGE_R              (0x1B24UL)
#define ISP_GRGB_LUM_TEX_T                 (0x1B28UL)
#define ISP_GRGB_LUM_TEX_R                 (0x1B2CUL)
#define ISP_GRGB_FREZ_FLAT_T              (0x1B30UL)
#define ISP_GRGB_FREZ_FLAT_R              (0x1B34UL)
#define ISP_GRGB_FREZ_EDGE_T             (0x1B38UL)
#define ISP_GRGB_FREZ_EDGE_R             (0x1B3CUL)
#define ISP_GRGB_FREZ_TEX_T                 (0x1B40UL)
#define ISP_GRGB_FREZ_TEX_R                 (0x1B44UL)

/*isp sub block: GrGb2 Curve - GC2*/
#define ISP_GC2_CTRL                    (0x1F10UL)
#define ISP_GC2_PARA1               (0x1F14UL)
#define ISP_GC2_PARA2                (0x1F18UL)
#define ISP_GC2_PARA3                (0x1F1CUL)
#define ISP_GC2_PARA4                (0x1F20UL)
#define ISP_GC2_PARA5                (0x1F24UL)
#define ISP_GC2_PARA6                (0x1F28UL)
#define ISP_GC2_PARA7                (0x1F2CUL)
#define ISP_GC2_PARA8                (0x1F30UL)
#define ISP_GC2_PARA9                (0x1F34UL)
#define ISP_GC2_PARA10                (0x1F38UL)
#define ISP_GC2_PARA11                (0x1F3CUL)
#define ISP_GC2_PARA12                (0x1F40UL)
#define ISP_GC2_PARA13                (0x1F44UL)
#define ISP_GC2_PARA14                (0x1F48UL)
#define ISP_GC2_PARA15                (0x1F4CUL)
#define ISP_GC2_PARA16                (0x1F50UL)
#define ISP_GC2_PARA17                (0x1F54UL)
#define ISP_GC2_PARA18                (0x1F58UL)
#define ISP_GC2_PARA19                (0x1F5CUL)
#define ISP_GC2_PARA20                (0x1F60UL)
#define ISP_GC2_PARA21                (0x1F64UL)
#define ISP_GC2_PARA22                (0x1F68UL)
#define ISP_GC2_PARA23                (0x1F6CUL)
#define ISP_GC2_PARA24                (0x1F70UL)
#define ISP_GC2_PARA25                (0x1F74UL)
#define ISP_GC2_PARA26                (0x1F78UL)
#define ISP_GC2_PARA27                (0x1F7CUL)
#define ISP_GC2_PARA28                (0x1F80UL)
#define ISP_GC2_PARA29                (0x1F84UL)
#define ISP_GC2_PARA30                (0x1F88UL)

/*isp sub block: NLM VST IVST*/
#define ISP_VST_PARA                            (0x1C10UL)
#define ISP_IVST_PARA                        (0x1E10UL)
#define ISP_NLM_LB_STATUS0                    (0x2000UL)
#define ISP_NLM_LB_STATUS1                    (0x2004UL)
#define ISP_NLM_STATUS                        (0x2008UL)
#define ISP_NLM_PARA                        (0x2010UL)
#define ISP_NLM_MODE_CNT                    (0x2014UL)
#define ISP_NLM_SIMPLE_BPC                    (0x2018UL)
#define ISP_NLM_LUM_THRESHOLD                (0x201CUL)
#define ISP_NLM_DIRECTION_TH                (0x2020UL)
#define ISP_NLM_IS_FLAT                        (0x2024UL)
#define ISP_NLM_LUT_W_0                           (0x2028UL)
#define ISP_NLM_LUM0_FLAT0_PARAM            (0x2088UL)
#define ISP_NLM_LUM0_FLAT0_ADDBACK        (0x208CUL)
#define ISP_NLM_LUM0_FLAT1_PARAM            (0x2090UL)
#define ISP_NLM_LUM0_FLAT1_ADDBACK        (0x2094UL)
#define ISP_NLM_LUM0_FLAT2_PARAM            (0x2098UL)
#define ISP_NLM_LUM0_FLAT2_ADDBACK        (0x209CUL)
#define ISP_NLM_LUM0_FLAT3_PARAM            (0x20A0UL)
#define ISP_NLM_LUM0_FLAT3_ADDBACK        (0x20A4UL)
#define ISP_NLM_ADDBACK3                          (0x20E8UL)
#define ISP_NLM_RADIAL_1D_PARAM                    (0x20ECUL)
#define ISP_NLM_RADIAL_1D_DIST                    (0x20F0UL)
#define ISP_NLM_RADIAL_1D_THRESHOLD        (0x20F4UL)
#define ISP_NLM_RADIAL_1D_THR0                       (0x20F8UL)
#define ISP_NLM_RADIAL_1D_RATIO            (0x211CUL)
#define ISP_NLM_RADIAL_1D_GAIN_MAX        (0x214CUL)
#define ISP_NLM_RADIAL_1D_ADDBACK00        (0x2150UL)
#define ISP_REG_VST_ADDR0                    (0x19000UL)
#define ISP_REG_IVST_ADDR0                    (0x1A000UL)
#define ISP_REG_VST_ADDR1                    (0x22000UL)
#define ISP_REG_IVST_ADDR1                    (0x23000UL)

/*isp sub block: CFA NEW: clolor filter    array*/
#define ISP_CFAE_STATUS                                (0x3000UL)
#define ISP_CFAE_NEW_CFG0                    (0x3010UL)
#define ISP_CFAE_INTP_CFG0                    (0x3014UL)
#define ISP_CFAE_INTP_CFG1                    (0x3018UL)
#define ISP_CFAE_INTP_CFG2                    (0x301CUL)
#define ISP_CFAE_INTP_CFG3                    (0x3020UL)
#define ISP_CFAE_INTP_CFG4                    (0x3024UL)
#define ISP_CFAE_INTP_CFG5                    (0x3028UL)
#define ISP_CFAE_CSS_CFG0                    (0x302CUL)
#define ISP_CFAE_CSS_CFG1                    (0x3030UL)
#define ISP_CFAE_CSS_CFG2                    (0x3034UL)
#define ISP_CFAE_CSS_CFG3                    (0x3038UL)
#define ISP_CFAE_CSS_CFG4                    (0x303CUL)
#define ISP_CFAE_CSS_CFG5                    (0x3040UL)
#define ISP_CFAE_CSS_CFG6                    (0x3044UL)
#define ISP_CFAE_CSS_CFG7                    (0x3048UL)
#define ISP_CFAE_CSS_CFG8                    (0x304CUL)
#define ISP_CFAE_CSS_CFG9                    (0x3050UL)
#define ISP_CFAE_CSS_CFG10                    (0x3054UL)
#define ISP_CFAE_CSS_CFG11                    (0x3058UL)
#define ISP_CFAE_GBUF_CFG                    (0x305CUL)

/*isp sub block:CMC: Color matrix correction for 10    bit*/
#define ISP_CMC10_STATUS0                    (0x3100UL)
#define ISP_CMC10_STATUS1                    (0x3104UL)
#define ISP_CMC10_PARAM                               (0x3110UL)
#define ISP_CMC10_MATRIX0                            (0x3114UL)
#define ISP_CMC10_MATRIX0_BUF                (0x312CUL)
#define ISP_CMC10_MATRIX1                           (0x3118UL)
#define ISP_CMC10_MATRIX2                           (0x311CUL)
#define ISP_CMC10_MATRIX3                           (0x3120UL)
#define ISP_CMC10_MATRIX4                           (0x3124UL)

/*isp sub block: FRGB GAMMA */
#define ISP_GAMMA_PARAM                    (0x3210UL)

/*isp sub block:HSV*/
#define ISP_HSV_PARAM                        (0x3310UL)
#define ISP_HSV_CFG0                                (0x3314UL)
#define ISP_HSV_CFG1                                (0x3318UL)
#define ISP_HSV_CFG2                               (0x331CUL)
#define ISP_HSV_CFG3                                (0x3320UL)
#define ISP_HSV_CFG4                                (0x3324UL)
#define ISP_HSV_CFG5                                (0x3328UL)
#define ISP_HSV_CFG6                                (0x332CUL)
#define ISP_HSV_CFG7                                (0x3330UL)
#define ISP_HSV_CFG8                                (0x3334UL)
#define ISP_HSV_CFG9                                (0x3338UL)
#define ISP_HSV_CFG10                               (0x333CUL)
#define ISP_HSV_CFG11                               (0x3340UL)
#define ISP_HSV_CFG12                               (0x3344UL)
#define ISP_HSV_CFG13                               (0x3348UL)
#define ISP_HSV_CFG14                               (0x334CUL)
#define ISP_HSV_CFG15                               (0x3350UL)
#define ISP_HSV_CFG16                               (0x3354UL)
#define ISP_HSV_CFG17                               (0x3358UL)
#define ISP_HSV_CFG18                               (0x335cUL)
#define ISP_HSV_CFG19                               (0x3360UL)
#define ISP_HSV_CFG20                               (0x3364UL)
#define ISP_HSV_CFG21                               (0x3368UL)

/*isp sub block:ISP_POSTERIZE*/
#define ISP_PSTRZ_STATUS                    (0x3400UL) //(0x3B00UL)
#define ISP_PSTRZ_PARAM                        (0x3410UL) //(0x3B14UL)
#define ISP_PSTRZ_R_BUF0_CH0                   (0x1b400UL)
#define ISP_PSTRZ_G_BUF0_CH0                   (0x1c400UL)
#define ISP_PSTRZ_B_BUF0_CH0                   (0x1d400UL)
#define ISP_PSTRZ_R_BUF1_CH0                   (0x24400UL)
#define ISP_PSTRZ_G_BUF1_CH0                   (0x25400UL)
#define ISP_PSTRZ_B_BUF1_CH0                   (0x26400UL)


/*isp sub block: CCE: color conversion enhancement*/
#define ISP_CCE_PARAM                                  (0x3510UL)
#define ISP_CCE_MATRIX0                        (0x3514UL)
#define ISP_CCE_MATRIX1                        (0x3518UL)
#define ISP_CCE_MATRIX2                        (0x351CUL)
#define ISP_CCE_MATRIX3                        (0x3520UL)
#define ISP_CCE_MATRIX4                        (0x3524UL)
#define ISP_CCE_SHIFT                           (0x3528UL)

/*isp sub block: UVD: UV division, include in CCE block*/
#define ISP_UVD_STATUS0                        (0x3600UL)
#define ISP_UVD_PARAM                        (0x3610UL)
#define ISP_UVD_PARAM0                        (0x3614UL)
#define ISP_UVD_PARAM1                        (0x3618UL)
#define ISP_UVD_PARAM2                        (0x361CUL)
#define ISP_UVD_PARAM3                        (0x3620UL)
#define ISP_UVD_PARAM4                        (0x3624UL)
#define ISP_UVD_PARAM5                        (0x3628UL)

//LTM_STAT
#define ISP_LTM_STAT_RGB_BASE		(0x3800UL)
#define ISP_LTM_STAT_YUV_BASE		(0x5500UL)

#define ISP_LTM_PARAMETERS			(0x0010UL)
#define ISP_LTM_ROI_START			(0x0014UL)
#define ISP_LTM_TILE_RANGE			(0x0018UL)
#define ISP_LTM_CLIP_LIMIT			(0x0020UL)
#define ISP_LTM_THRES				(0x0024UL)
#define ISP_LTM_ADDR				(0x0028UL)
#define ISP_LTM_PITCH				(0x002CUL)
#define ISP_LTM_RGB_STAT_BUF0_ADDR			(0x1F000UL)
#define ISP_LTM_RGB_STAT_BUF1_ADDR			(0x28000UL)
#define ISP_LTM_YUV_STAT_BUF0_ADDR			(0x1F400UL)
#define ISP_LTM_YUV_STAT_BUF1_ADDR			(0x28400UL)

//LTM_MAP
#define ISP_LTM_MAP_RGB_BASE		(0x3900UL)
#define ISP_LTM_MAP_YUV_BASE		(0x5F00UL)

#define ISP_LTM_MAP_PARAM0			(0x0010UL)
#define ISP_LTM_MAP_PARAM1			(0x0014UL)
#define ISP_LTM_MAP_PARAM2			(0x0018UL)
#define ISP_LTM_MAP_PARAM3			(0x001CUL)
#define ISP_LTM_MAP_PARAM4			(0x0020UL)
#define ISP_LTM_MAP_PARAM5			(0x0024UL)

/*isp sub block:ANTI FLICKER NEW*/
#define ISP_ANTI_FLICKER_NEW_STATUS0              (0x4600UL)
#define ISP_ANTI_FLICKER_NEW_STATUS1              (0x4604UL)
#define ISP_ANTI_FLICKER_NEW_STATUS2              (0x4608UL)
#define ISP_ANTI_FLICKER_NEW_STATUS3              (0x460CUL)
#define ISP_ANTI_FLICKER_NEW_PARAM0            (0x4610UL)
#define ISP_ANTI_FLICKER_NEW_PARAM1            (0x4614UL)
#define ISP_ANTI_FLICKER_NEW_PARAM2            (0x4618UL)
#define ISP_ANTI_FLICKER_NEW_COL_POS            (0x461CUL)
#define ISP_ANTI_FLICKER_NEW_DDR_INIT_ADDR    (0x4620UL)
#define ISP_ANTI_FLICKER_NEW_REGION0            (0x4624UL)
#define ISP_ANTI_FLICKER_NEW_REGION1            (0x4628UL)
#define ISP_ANTI_FLICKER_NEW_REGION2            (0x462CUL)
#define ISP_ANTI_FLICKER_NEW_REGION3            (0x4630UL)
#define ISP_ANTI_FLICKER_NEW_SUM1            (0x4634UL)
#define ISP_ANTI_FLICKER_NEW_SUM2            (0x4638UL)
#define ISP_ANTI_FLICKER_NEW_CFG_READY        (0x463CUL)
#define ISP_ANTI_FLICKER_NEW_SKIP_NUM_CLR    (0x4640UL)

/*isp sub block: Pre-CDN*/
#define ISP_PRECDN_STATUS0                       (0x5000UL)
#define ISP_PRECDN_PARAM                       (0x5010UL)
#define ISP_PRECDN_MEDIAN_THRUV01               (0x5014UL)
#define ISP_PRECDN_THRYUV                       (0x5018UL)
#define ISP_PRECDN_SEGU_0                       (0x501CUL)
#define ISP_PRECDN_SEGU_1                       (0x5020UL)
#define ISP_PRECDN_SEGU_2                       (0x5024UL)
#define ISP_PRECDN_SEGU_3                       (0x5028UL)
#define ISP_PRECDN_SEGV_0                       (0x502CUL)
#define ISP_PRECDN_SEGV_1                       (0x5030UL)
#define ISP_PRECDN_SEGV_2                       (0x5034UL)
#define ISP_PRECDN_SEGV_3                       (0x5038UL)
#define ISP_PRECDN_SEGY_0                       (0x503CUL)
#define ISP_PRECDN_SEGY_1                       (0x5040UL)
#define ISP_PRECDN_SEGY_2                       (0x5044UL)
#define ISP_PRECDN_SEGY_3                       (0x5048UL)
#define ISP_PRECDN_DISTW0                       (0x504CUL)
#define ISP_PRECDN_DISTW1                       (0x5050UL)
#define ISP_PRECDN_DISTW2                       (0x5054UL)
#define ISP_PRECDN_DISTW3                       (0x5058UL)

/*isp sub block: YNR*/
#define ISP_YNR_STATUS0                        (0x5100UL)
#define ISP_YNR_CONTRL0                (0x5110UL)
#define ISP_YNR_CFG0                (0x5114UL)
#define ISP_YNR_CFG1                (0x5118UL)
#define ISP_YNR_CFG2                (0x511CUL)
#define ISP_YNR_CFG3                (0x5120UL)
#define ISP_YNR_CFG4                (0x5124UL)
#define ISP_YNR_CFG5                (0x5128UL)
#define ISP_YNR_CFG6                (0x512CUL)
#define ISP_YNR_CFG7                (0x5130UL)
#define ISP_YNR_CFG8                (0x5134UL)
#define ISP_YNR_CFG9                (0x5138UL)
#define ISP_YNR_CFG10                (0x513CUL)
#define ISP_YNR_CFG11                (0x5140UL)
#define ISP_YNR_CFG12                (0x5144UL)
#define ISP_YNR_CFG13                (0x5148UL)
#define ISP_YNR_CFG14                (0x514cUL)
#define ISP_YNR_CFG15                (0x5150UL)
#define ISP_YNR_CFG16                (0x5154UL)
#define ISP_YNR_CFG17                (0x5158UL)
#define ISP_YNR_CFG18                (0x515cUL)
#define ISP_YNR_CFG19                (0x5160UL)
#define ISP_YNR_CFG20                (0x5164UL)
#define ISP_YNR_CFG21                (0x5168UL)
#define ISP_YNR_CFG22                (0x516cUL)
#define ISP_YNR_CFG23                (0x5170UL)
#define ISP_YNR_CFG24                (0x5174UL)
#define ISP_YNR_CFG25                (0x5178UL)
#define ISP_YNR_CFG26                (0x517cUL)
#define ISP_YNR_CFG27                (0x5180UL)
#define ISP_YNR_CFG28                (0x5184UL)
#define ISP_YNR_CFG29                (0x5188UL)
#define ISP_YNR_CFG30                (0x518cUL)
#define ISP_YNR_CFG31                (0x5190UL)
#define ISP_YNR_CFG32                (0x5194UL)
#define ISP_YNR_CFG33                (0x5198UL)

/* bchs */
#define ISP_BCHS_PARAM                (0x5910UL)
#define ISP_CSA_FACTOR                (0x5914UL)
#define ISP_HUA_FACTOR                (0x5918UL)
#define ISP_BRTA_FACTOR                (0x5920UL)
#define ISP_CNTA_FACTOR                (0x5924UL)

/*isp sub block: Brightness*/
#define ISP_BRIGHT_STATUS                    (0x5200UL)
#define ISP_BRIGHT_PARAM                    (0x5210UL)

/*isp sub block: Contrast*/
#define ISP_CONTRAST_STATUS                    (0x5300UL)
#define ISP_CONTRAST_PARAM                    (0x5310UL)

/*isp sub block: HIST :    histogram*/
#define ISP_HIST_STATUS                        (0x3700UL)
#define ISP_HIST_PARAM                        (0x3710UL)
#define ISP_HIST_ROI_START                    (0x3714UL)
#define ISP_HIST_ROI_END                    (0x3718UL)
#define ISP_HIST_CFG_READY                    (0x371CUL)
#define ISP_HIST_SKIP_NUM_CLR                (0x3720UL)

/*isp sub block:LTM HIST FRGB*/
#define ISP_YUV_lTM_HISTS_PARAM0              (0x5510UL)

/*isp sub block: HIST2*/
#define ISP_HIST2_PARAM                        (0x3710UL)
#define ISP_HIST2_ROI_S0                        (0x3714UL)
#define ISP_HIST2_ROI_E0                        (0x3718UL)
#define ISP_HIST2_CFG_READY                    (0x371CUL)
#define ISP_HIST2_SKIP_NUM_CLR                (0x3720UL)

/*isp sub block: cdn*/
#define ISP_CDN_STATUS0                        (0x5600UL)
#define ISP_CDN_PARAM                        (0x5610UL)
#define ISP_CDN_THRUV                        (0x5614UL)
#define ISP_CDN_U_RANWEI_0                    (0x5618UL) // RANWEI_0 ~ RANWEI_7
#define ISP_CDN_U_RANWEI_7                    (0x5634UL)
#define ISP_CDN_V_RANWEI_0                    (0x5638UL)
#define ISP_CDN_V_RANWEI_7                    (0x5654UL)

/*isp sub block: edge*/
#define ISP_EE_STATUS                        (0x5700UL)
#define ISP_EE_PARAM                        (0x5710UL)
#define ISP_EE_CFG0                            (0x5714UL)
#define ISP_EE_CFG1                            (0x5718UL)
#define ISP_EE_CFG2                            (0x571CUL)
#define ISP_EE_ADP_CFG0                        (0x5720UL)
#define ISP_EE_ADP_CFG1                        (0x5724UL)
#define ISP_EE_ADP_CFG2                        (0x5728UL)
#define ISP_EE_IPD_CFG0                        (0x572CUL)
#define ISP_EE_IPD_CFG1                        (0x5730UL)
#define ISP_EE_IPD_CFG2                        (0x5734UL)
#define ISP_EE_LUM_CFG0                    (0x5738UL)
#define ISP_EE_LUM_CFG1                    (0x573CUL)
#define ISP_EE_LUM_CFG2                    (0x5740UL)
#define ISP_EE_LUM_CFG3                    (0x5744UL)
#define ISP_EE_LUM_CFG4                    (0x5748UL)
#define ISP_EE_LUM_CFG5                    (0x574CUL)
#define ISP_EE_LUM_CFG6                    (0x5750UL)
#define ISP_EE_LUM_CFG7                    (0x5754UL)
#define ISP_EE_LUM_CFG8                    (0x5758UL)
#define ISP_EE_LUM_CFG9                    (0x575CUL)
#define ISP_EE_LUM_CFG10                    (0x5760UL)
#define ISP_EE_LUM_CFG11                    (0x5764UL)
#define ISP_EE_LUM_CFG12                    (0x5768UL)
#define ISP_EE_LUM_CFG13                    (0x576CUL)
#define ISP_EE_LUM_CFG14                    (0x5770UL)
#define ISP_EE_LUM_CFG15                    (0x5774UL)
#define ISP_EE_LUM_CFG16                    (0x5778UL)
#define ISP_EE_LUM_CFG17                    (0x577CUL)
#define ISP_EE_LUM_CFG18                    (0x5780UL)

/*isp sub block: csa*/
#define ISP_CSA_STATUS                        (0x5800UL)
#define ISP_CSA_PARAM                        (0x5810UL)

/*isp sub block: hua*/
#define ISP_HUA_STATUS                        (0x5900UL)
#define ISP_HUA_PARAM                        (0x5910UL)

/*isp sub block: post-cdn*/
#define ISP_POSTCDN_STATUS                    (0x5A00UL)
#define ISP_POSTCDN_COMMON_CTRL            (0x5A10UL)
#define ISP_POSTCDN_ADPT_THR                (0x5A14UL)
#define ISP_POSTCDN_UVTHR                    (0x5A18UL)
#define ISP_POSTCDN_THRU                    (0x5A1CUL)
#define ISP_POSTCDN_THRV                    (0x5A20UL)
#define ISP_POSTCDN_RSEGU01                (0x5A24UL)
#define ISP_POSTCDN_RSEGU23                (0x5A28UL)
#define ISP_POSTCDN_RSEGU45                (0x5A2CUL)
#define ISP_POSTCDN_RSEGU6                    (0x5A30UL)
#define ISP_POSTCDN_RSEGV01                (0x5A34UL)
#define ISP_POSTCDN_RSEGV23                (0x5A38UL)
#define ISP_POSTCDN_RSEGV45                (0x5A3CUL)
#define ISP_POSTCDN_RSEGV6                    (0x5A40UL)
#define ISP_POSTCDN_R_DISTW0                (0x5A44UL) //DISTW0~DISTW14
#define ISP_POSTCDN_START_ROW_MOD4        (0x5A80UL) //SLICE_CTRL

/*isp sub block: ygamma*/
#define ISP_YGAMMA_STATUS                    (0x5B00UL)
#define ISP_YGAMMA_PARAM                    (0x5B10UL)

/*isp sub block: ydelay-- also means uvdelay*/
#define ISP_YDELAY_STATUS                       (0x5C00UL)
#define ISP_YDELAY_PARAM                       (0x5C10UL)
#define ISP_YDELAY_STEP                              (0x5C14UL) //SharkLe new

/*isp sub block: iircnr*/
#define ISP_IIRCNR_STATUS0                    (0x5D00UL)
#define ISP_IIRCNR_STATUS1                    (0x5D04UL)
#define ISP_IIRCNR_PARAM                     (0x5D10UL)
#define ISP_IIRCNR_PARAM1                    (0x5D14UL)
#define ISP_IIRCNR_PARAM2                    (0x5D18UL)
#define ISP_IIRCNR_PARAM3                    (0x5D1CUL)
#define ISP_IIRCNR_PARAM4                    (0x5D20UL)
#define ISP_IIRCNR_PARAM5                    (0x5D24UL)
#define ISP_IIRCNR_PARAM6                    (0x5D28UL)
#define ISP_IIRCNR_PARAM7                    (0x5D2CUL)
#define ISP_IIRCNR_PARAM8                    (0x5D30UL)
#define ISP_IIRCNR_PARAM9                    (0x5D34UL)
#define ISP_IIRCNR_NEW0                        (0x5D38UL)
#define ISP_IIRCNR_NEW1                        (0x5D3CUL)
#define ISP_IIRCNR_NEW2                        (0x5D40UL)
#define ISP_IIRCNR_NEW3                        (0x5D44UL)
#define ISP_IIRCNR_NEW4                        (0x5D48UL)
#define ISP_IIRCNR_NEW5                        (0x5D4CUL)
#define ISP_IIRCNR_NEW6                        (0x5D50UL)
#define ISP_IIRCNR_NEW7                        (0x5D54UL)
#define ISP_IIRCNR_NEW8                        (0x5D58UL)
#define ISP_IIRCNR_NEW9                        (0x5D5CUL)
#define ISP_IIRCNR_NEW10                    (0x5D60UL)
#define ISP_IIRCNR_NEW11                    (0x5D64UL)
#define ISP_IIRCNR_NEW12                    (0x5D68UL)
#define ISP_IIRCNR_NEW13                    (0x5D6CUL)
#define ISP_IIRCNR_NEW14                    (0x5D70UL)
#define ISP_IIRCNR_NEW15                    (0x5D74UL)
#define ISP_IIRCNR_NEW16                    (0x5D78UL)
#define ISP_IIRCNR_NEW17                    (0x5D7CUL)
#define ISP_IIRCNR_NEW18                    (0x5D80UL)
#define ISP_IIRCNR_NEW19                    (0x5D84UL)
#define ISP_IIRCNR_NEW20                    (0x5D88UL)
#define ISP_IIRCNR_NEW21                    (0x5D8CUL)
#define ISP_IIRCNR_NEW22                    (0x5D90UL)
#define ISP_IIRCNR_NEW23                    (0x5D94UL)
#define ISP_IIRCNR_NEW24                    (0x5D98UL)
#define ISP_IIRCNR_NEW25                    (0x5D9CUL)
#define ISP_IIRCNR_NEW26                    (0x5DA0UL)
#define ISP_IIRCNR_NEW27                    (0x5DA4UL)
#define ISP_IIRCNR_NEW28                    (0x5DA8UL)
#define ISP_IIRCNR_NEW29                    (0x5DACUL)
#define ISP_IIRCNR_NEW30                    (0x5DB0UL)
#define ISP_IIRCNR_NEW31                    (0x5DB4UL)
#define ISP_IIRCNR_NEW32                    (0x5DB8UL)
#define ISP_IIRCNR_NEW33                    (0x5DBCUL)
#define ISP_IIRCNR_NEW34                    (0x5DC0UL)
#define ISP_IIRCNR_NEW35                    (0x5DC4UL)
#define ISP_IIRCNR_NEW36                    (0x5DC8UL)
#define ISP_IIRCNR_NEW37                    (0x5DCCUL)
#define ISP_IIRCNR_NEW38                    (0x5DD0UL)
#define ISP_IIRCNR_NEW39                      (0x5DD4UL)

/*isp sub block: YRANDOM */
#define ISP_YRANDOM_STATUS0                    (0x5E00UL)
#define ISP_YRANDOM_CHKSUM                    (0x5E04UL)
#define ISP_YRANDOM_PARAM1                    (0x5E10UL)
#define ISP_YRANDOM_PARAM2                    (0x5E14UL)
#define ISP_YRANDOM_PARAM3                    (0x5E18UL)
#define ISP_YRANDOM_STATUS1                    (0x5E1CUL)
#define ISP_YRANDOM_STATUS2                    (0x5E20UL)
#define ISP_YRANDOM_INIT                       (0x5E24UL)

/*isp sub block:LTM HIST FRGB*/
#define ISP_YUV_lTMAP_PARAM0              (0x5F10UL)

/*isp sub block: CFG*/
#define ISP_CFG_STATUS0                        (0x8100UL)
#define ISP_CFG_STATUS1                        (0x8104UL)
#define ISP_CFG_STATUS2                        (0x8108UL)
#define ISP_CFG_STATUS3                           (0x810CUL)
#define ISP_CFG_PAMATER                        (0x8110UL)
#define ISP_CFG_TM_NUM                        (0x8114UL)
#define ISP_CFG_CAP0_TH                        (0x8118UL)
#define ISP_CFG_CAP1_TH                        (0x811CUL)
#define ISP_CFG_PRE0_CMD_ADDR                (0x8120UL)
#define ISP_CFG_PRE1_CMD_ADDR                (0x8124UL)
#define ISP_CFG_CAP0_CMD_ADDR                (0x8128UL)
#define ISP_CFG_CAP1_CMD_ADDR                (0x812CUL)
#define ISP_CFG_PRE0_START                    (0x8130UL)
#define ISP_CFG_PRE1_START                    (0x8134UL)
#define ISP_CFG_CAP0_START                    (0x8138UL)
#define ISP_CFG_CAP1_START                    (0x813CUL)
#define ISP_CFG_AUTO_CG_EN                    (0x8140UL)
#define ISP_CFG_CAP_FMCU_RDY                (0x8144UL)
#define ISP_CFG_STATUS                        (0x8150UL)

/*mem ctrl*/
#define ISP_YUV_3DNR_MEM_CTRL_PARAM0        (0x9010UL)
#define ISP_YUV_3DNR_MEM_CTRL_LINE_MODE        (0x9014UL)
#define ISP_YUV_3DNR_MEM_CTRL_PARAM1        (0x9018UL)
#define ISP_YUV_3DNR_MEM_CTRL_PARAM2        (0x901CUL)
#define ISP_YUV_3DNR_MEM_CTRL_PARAM3        (0x9020UL)
#define ISP_YUV_3DNR_MEM_CTRL_PARAM4        (0x9024UL)
#define ISP_YUV_3DNR_MEM_CTRL_PARAM5        (0x9028UL)
#define ISP_YUV_3DNR_MEM_CTRL_PARAM7        (0x902CUL)
#define ISP_YUV_3DNR_MEM_CTRL_FT_CUR_LUMA_ADDR            (0x9030UL)
#define ISP_YUV_3DNR_MEM_CTRL_FT_CUR_CHROMA_ADDR        (0x9034UL)
#define ISP_YUV_3DNR_MEM_CTRL_FT_CTRL_PITCH                (0x9038UL)
#define ISP_YUV_3DNR_MEM_CTRL_PARAM8        (0x903CUL)
#define ISP_YUV_3DNR_MEM_CTRL_PARAM9        (0x9040UL)
#define ISP_YUV_3DNR_MEM_CTRL_PARAM10        (0x9044UL)
#define ISP_YUV_3DNR_MEM_CTRL_PARAM11        (0x9048UL)
#define ISP_YUV_3DNR_MEM_CTRL_PARAM12        (0x904CUL)
#define ISP_YUV_3DNR_MEM_CTRL_PARAM13        (0x9050UL)

/*isp sub block: 3DNR*/
#define ISP_YUV_3DNR_CONTROL0                (0x9110UL)
#define ISP_YUV_3DNR_CFG1                    (0x9114UL)
#define ISP_YUV_3DNR_CFG2                    (0x9118UL)
#define ISP_YUV_3DNR_CFG3                    (0x911CUL)
#define ISP_YUV_3DNR_CFG4                    (0x9120UL)
#define ISP_YUV_3DNR_CFG5                    (0x9124UL)
#define ISP_YUV_3DNR_CFG6                    (0x9128UL)
#define ISP_YUV_3DNR_CFG7                    (0x912CUL)
#define ISP_YUV_3DNR_CFG8                    (0x9130UL)
#define ISP_YUV_3DNR_CFG9                    (0x9134UL)
#define ISP_YUV_3DNR_CFG10                    (0x9138UL)
#define ISP_YUV_3DNR_CFG11                    (0x913CUL)
#define ISP_YUV_3DNR_CFG12                    (0x9140UL)
#define ISP_YUV_3DNR_CFG13                    (0x9144UL)
#define ISP_YUV_3DNR_CFG14                    (0x9148UL)
#define ISP_YUV_3DNR_CFG15                    (0x914CUL)
#define ISP_YUV_3DNR_CFG16                    (0x9150UL)
#define ISP_YUV_3DNR_CFG17                    (0x9154UL)
#define ISP_YUV_3DNR_CFG18                    (0x9158UL)
#define ISP_YUV_3DNR_CFG19                    (0x915CUL)
#define ISP_YUV_3DNR_CFG20                    (0x9160UL)
#define ISP_YUV_3DNR_CFG21                    (0x9164UL)
#define ISP_YUV_3DNR_CFG22                    (0x9168UL)
#define ISP_YUV_3DNR_CFG23                    (0x916CUL)
#define ISP_YUV_3DNR_CFG24                    (0x9170UL)


/*3dnr store*/
#define ISP_STORE_LITE_PARAM                    (0x9210UL)
#define ISP_STORE_LITE_SIZE                        (0x9214UL)
#define ISP_STORE_LITE_ADDR0                    (0x9218UL)
#define ISP_STORE_LITE_ADDR1                    (0x921CUL)
#define ISP_STORE_LITE_PITCH                    (0x9220UL)
#define ISP_STORE_LITE_SHADOW_CLR                (0x9224UL)

/*3dnr crop*/
#define ISP_YUV_3DNR_MEM_CTRL_PRE_PARAM0    (0x9310UL)
#define ISP_YUV_3DNR_MEM_CTRL_PRE_PARAM1    (0x9314UL)
#define ISP_YUV_3DNR_MEM_CTRL_PRE_PARAM2    (0x9318UL)
#define ISP_YUV_3DNR_MEM_CTRL_PRE_PARAM3    (0x931CUL)

/*3dnr_fbc*/
#define ISP_FBC_3DNR_STORE_PARAM             (0x9410UL)
#define ISP_FBC_3DNR_STORE_SLICE_SIZE        (0x9414UL)
#define ISP_FBC_3DNR_STORE_SLICE_Y_ADDR        (0x9418UL)
#define ISP_FBC_3DNR_STORE_SLICE_C_ADDR        (0x941CUL)
#define ISP_FBC_3DNR_STORE_TILE_PITCH        (0x9420UL)
#define ISP_FBC_3DNR_STORE_SLICE_Y_HEADER    (0x9424UL)
#define ISP_FBC_3DNR_STORE_SLICE_C_HEADER    (0x9428UL)
#define ISP_FBC_3DNR_STORE_CONSTANT            (0x942CUL)
#define ISP_FBC_3DNR_STORE_BITS                (0x9430UL)
#define ISP_FBC_3DNR_STORE_TILE_NUM            (0x9434UL)
#define ISP_FBC_3DNR_STORE_NFULL_LEVEL      (0x9438UL)

/*3dnr_fbd*/
#define ISP_FBD_NR3_PARAM0                     (0x9510UL)
#define ISP_FBD_NR3_PARAM1                    (0x9514UL)
#define ISP_FBD_NR3_PARAM2                    (0x9518UL)
#define ISP_FBD_NR3_T_ADDR_Y                (0x951CUL)
#define ISP_FBD_NR3_H_ADDR_Y                (0x9520UL)
#define ISP_FBD_NR3_T_ADDR_C                (0x9524UL)
#define ISP_FBD_NR3_H_ADDR_C                (0x9528UL)
#define ISP_FBD_NR3_SIZE_Y                    (0x952CUL)
#define ISP_FBD_NR3_SIZE_C                    (0x9530UL)
#define ISP_FBD_NR3_START_Y                    (0x9534UL)
#define ISP_FBD_NR3_START_C                    (0x9538UL)
#define ISP_FBD_NR3_TILE_SIZE_Y                (0x953CUL)
#define ISP_FBD_NR3_TILE_SIZE_C                (0x9540UL)
#define ISP_FBD_NR3_SLICE_TILE_PARAM        (0x9544UL)
#define ISP_FBD_NR3_READ_SPECIAL            (0x9548UL)


/*isp sub block: NOISE FILTER, include in scaler_cap*/
#define ISP_NF_STATUS0                           (0xC200UL)
#define ISP_NF_STATUS1                           (0xC204UL)
#define ISP_NF_CTRL                               (0xC210UL)
#define ISP_NF_SEED0                           (0xC214UL)
#define ISP_NF_SEED1                           (0xC218UL)
#define ISP_NF_SEED2                           (0xC21CUL)
#define ISP_NF_SEED3                           (0xC220UL)
#define ISP_NF_TB4                               (0xC224UL)
#define ISP_NF_SF                               (0xC228UL)
#define ISP_NF_THR                               (0xC22CUL)
#define ISP_NF_CV_T12                           (0xC230UL)
#define ISP_NF_CV_R                               (0xC234UL)
#define ISP_NF_NS_CLIP                           (0xC238UL)
#define ISP_NF_SEED0_OUT                       (0xC23CUL)
#define ISP_NF_SEED_INIT                       (0xC24CUL)
#define ISP_NF_CV_T34                           (0xC250UL)

//thumbnailscaler
#define ISP_THMB_CFG                (0xE010UL)
#define ISP_THMB_BEFORE_TRIM_SIZE    (0xE014UL)
#define ISP_THMB_Y_SLICE_SRC_SIZE    (0xE018UL)
#define ISP_THMB_Y_DES_SIZE            (0xE01CUL)
#define ISP_THMB_Y_TRIM0_START        (0xE020UL)
#define ISP_THMB_Y_TRIM0_SIZE        (0xE024UL)
#define ISP_THMB_Y_INIT_PHASE        (0xE028UL)
#define ISP_THUMB_Y_FACTOR_HOR    (0xE02CUL)
#define ISP_THUMB_Y_FACTOR_VER        (0xE030UL)
#define ISP_THMB_UV_SLICE_SRC_SIZE    (0xE034UL)
#define ISP_THMB_UV_DES_SIZE        (0xE038UL)
#define ISP_THMB_UV_TRIM0_START    (0xE03CUL)
#define ISP_THMB_UV_TRIM0_SIZE        (0xE040UL)
#define ISP_THMB_UV_INIT_PHASE        (0xE044UL)
#define ISP_THUMB_UV_FACTOR_HOR    (0xE048UL)
#define ISP_THUMB_UV_FACTOR_VER    (0xE04CUL)
#define ISP_THUMBNAIL_SHRINK_CFG    (0xE050UL)
#define ISP_THUMBNAIL_EFFECT_CFG    (0xE054UL)
#define ISP_THUMBNAIL_REGULAR_CFG    (0xE058UL)
#define ISP_SCL_DEBUG                (0xE05CUL)
#define ISP_THMB_FRAME_CNT_CLR        (0xE060UL)

#define ISP_AEM_CH0                            (0x12000UL)
#define ISP_HIST_CH0                         (0x15000UL)
#define ISP_HIST2_BUF0_CH0                    (0x16000UL)
#define ISP_HSV_BUF0_CH0                    (0x18000UL)
#define ISP_VST_BUF0_CH0                    (0x19000UL)
#define ISP_IVST_BUF0_CH0                    (0x1A000UL)
#define ISP_FGAMMA_R_BUF0_CH0                (0x1B000UL)
#define ISP_FGAMMA_G_BUF0_CH0                (0x1C000UL)
#define ISP_FGAMMA_B_BUF0_CH0                (0x1D000UL)
#define ISP_YGAMMA_BUF0_CH0                (0x1E000UL)
#define ISP_HSV_BUF1_CH0                    (0x21000UL)
#define ISP_VST_BUF1_CH0                    (0x22000UL)
#define ISP_IVST_BUF1_CH0                    (0x23000UL)
#define ISP_FGAMMA_R_BUF1_CH0                (0x24000UL)
#define ISP_FGAMMA_G_BUF1_CH0                (0x25000UL)
#define ISP_FGAMMA_B_BUF1_CH0                (0x26000UL)
#define ISP_YGAMMA_BUF1_CH0                (0x27000UL)
#define ISP_LEN_BUF0_CH0                    (0x29000UL)
#define ISP_LEN_BUF1_CH0                    (0x2c000UL)
#define ISP_1D_LENS_GR_BUF0_CH0            (0x30000UL)
#define ISP_1D_LENS_R_BUF0_CH0            (0x30400UL)
#define ISP_1D_LENS_B_BUF0_CH0                (0x30800UL)
#define ISP_1D_LENS_GB_BUF0_CH0            (0x30c00UL)
#define ISP_1D_LENS_GR_BUF1_CH0            (0x30000UL)
#define ISP_1D_LENS_R_BUF1_CH0            (0x30400UL)
#define ISP_1D_LENS_B_BUF1_CH0                (0x30800UL)
#define ISP_1D_LENS_GB_BUF1_CH0            (0x30c00UL)
#define ISP_LENS_WEIGHT_BUF0                (0x36400UL)
#define ISP_CFG0_BUF                        (0x36c00UL)
#define ISP_CFG1_BUF                        (0x36e00UL)
#define ISP_LENS_WEIGHT_BUF1                (0x37000UL)

/*isp v    memory1: awbm*/
#define ISP_RAW_AWBM_OUTPUT                   (0x10000)

/*isp v    memory2: aem*/
#define ISP_RAW_AEM_OUTPUT                   (0x12000)

/*isp v    memory3: yiq_aem*/
#define ISP_YIQ_AEM_OUTPUT                   (0x14000)

/*isp v    memory11: 3D_LUT0*/
#define ISP_3D_LUT0_OUTPUT                   (0x17000)

/*isp v    memory12: HSV BUF0*/
#define ISP_HSV_BUF0_OUTPUT                   (0x18000)

/*isp v    memory20: 3D_LUT1*/
#define ISP_3D_LUT1_OUTPUT                   (0x20000)

/*isp v    memory21: HSV BUF1*/
#define ISP_HSV_BUF1_OUTPUT                   (0x21000)

/*irq line number in system*/
#define ISP_RST_LOG_BIT                       BIT_2
#define ISP_RST_CFG_BIT                       BIT_3

#define ISP_IRQ_HW_MASK                       (0xFFFFFFFF)
#define ISP_IRQ_NUM                           (32)
#define ISP_REG_BUF_SIZE                   (4 *    1024)
#define ISP_RAW_AE_BUF_SIZE                   (1024 * 4 * 3)
#define ISP_FRGB_GAMMA_BUF_SIZE               (257    * 4)
#define ISP_YUV_YGAMMA_BUF_SIZE               (129    * 4)
#define ISP_RAW_AWB_BUF_SIZE               (256    * 4    * 9)
#define ISP_BING4AWB_SIZE                   (640    * 480 *    2)
#define ISP_YIQ_ANTIFLICKER_SIZE           (3120 * 4 * 61)
#define ISP_YIQ_AEM_BUF_SIZE               (1024 * 4)

/*ping pang    buffer num*/
#define ISP_PINGPANG_CTM_NUM               729
#define ISP_CTM_PARAM_NUM                  (729 * 4)
#define ISP_PINGPANG_HSV_NUM               361
#define ISP_HSV_PARAM_NUM                  (361 * 2)
#define ISP_PINGPANG_FRGB_GAMC_NODE           129
#define ISP_PINGPANG_FRGB_GAMC_NUM           257
#define ISP_PINGPANG_YUV_YGAMMA_NUM           129
#define ISP_VST_IVST_NUM                   1024
#define ISP_AFM_WIN_NUM_V1                   25
#define ISP_AFM_WIN_NUM_R6P9               10

#define ISP_BYPASS_EB                       1
#define ISP_BYPASS_DIS                       0
#define ISP_AWBM_ITEM                       1024
#define ISP_HIST_ITEM                       256
#define ISP_HDR_COMP_ITEM                   64
#define ISP_HDR_P2E_ITEM                   32
#define ISP_HDR_E2P_ITEM                   32
#define ISP_AEM_ITEM                       1024
#define ISP_RAW_AWBM_ITEM                   256
#define ISP_RAW_AEM_ITEM                   1024
#define ISP_3D_LUT_ITEM                       729
#define ISP_RAW_AFM_ITEM                   25
#define ISP_YIQ_AFM_ITEM                   100
#define ISP_YIQ_AEM_ITEM                   1024
#define ISP_HSV_ITEM                       361

#define ISP_PATH_FRAME_WIDTH_MAX       4672
#define ISP_PATH_FRAME_HEIGHT_MAX      3504
#define ISP_DCAM0_LINE_BUF_LENGTH      4672
#define ISP_DCAM1_LINE_BUF_LENGTH      4672
#define ISP_DCAM2_LINE_BUF_LENGTH      1600

/*
 * Divide isp_pipe_dev->id into 3 parts:
 * @scene_id, pre(0) or cap(1)
 * @mode_id, 0: cfg mode, 1:ap mode,
 * @isp_id, isp instance id
 * each part has 8bits.
 *
 * Use ISP_GET_xID(idx) to get
 * specified id from idx(id extend).
 * Use ISP_SET_xID(idx, id) to set id to idx.
 *
 * MSB                               LSB
 * |----8-----|----8------|-----8------|
 * | scene id | mode_id   |   isp id   |
 *
 * scope of the three id:
 * scene id: 0 / 1
 * mode_id: 0 / 1
 * isp_id: 0 / 1
 */

#define ID_MASK 0x1
#define SCENE_ID_POS 16/* position of isp Scene ID in com_idx */
#define MODE_ID_POS 8/* position of isp work Mode ID in com_idx */
#define ISP_ID_POS 0/* position of isp Instance ID in com_idx*/

#define ISP_GET_ISP_ID(idx)            (((idx) >> ISP_ID_POS) & ID_MASK)
#define ISP_GET_SCENE_ID(idx)          (((idx) >> SCENE_ID_POS) & ID_MASK)
#define ISP_GET_MODE_ID(idx)           (((idx) >> MODE_ID_POS) & ID_MASK)

#define ISP_SET_ISP_ID(idx, id) \
		((idx) = ((((id) & ID_MASK) << ISP_ID_POS) | \
		((idx) & ~(ID_MASK << ISP_ID_POS))))

#define ISP_SET_MODE_ID(idx, id) \
		((idx) = ((((id) & ID_MASK) << MODE_ID_POS) | \
		((idx) & ~(ID_MASK << MODE_ID_POS))))

#define ISP_SET_SCENE_ID(idx, id) \
		((idx) = ((((id) & ID_MASK) << SCENE_ID_POS) | \
		((idx) & ~(ID_MASK << SCENE_ID_POS))))

/* Composite idx according to the rules above */
#define ISP_COM_IDX(idx, scene_id, mode_id, isp_id)	\
		((idx) = ((isp_id) | ((mode_id) << 8)  | ((scene_id) << 16)))

#define ISP_BASE_ADDR(idx) \
	(*(isp_cfg_poll_addr[ISP_GET_MODE_ID(idx)] \
		[(ISP_GET_ISP_ID(idx) << 1) | \
		ISP_GET_SCENE_ID(idx)]))

#define ISP_WORK_BASE_ADDR(idx) \
	(*(isp_cfg_work_poll_addr[ISP_GET_MODE_ID(idx)] \
		[(ISP_GET_ISP_ID(idx) << 1) | \
		ISP_GET_SCENE_ID(idx)]))

#define ISP_GET_REG(idx, reg)  (ISP_PHYS_ADDR(idx) + (reg))
#define ISP_REG_WR(idx, reg, val)  (REG_WR(ISP_BASE_ADDR(idx) + (reg), (val)))
#define ISP_REG_CFG_RD(idx, reg)  (REG_RD(ISP_BASE_ADDR(idx) + (reg)))
#define ISP_REG_RD(idx, reg)  (REG_RD(s_isp_regbase[idx] + (reg)))
#define ISP_REG_MWR(idx, reg, msk, val)  (ISP_REG_WR((idx), (reg), \
		((val) & (msk)) | \
		(ISP_REG_CFG_RD((idx), (reg)) & (~(msk)))))
#define ISP_REG_OWR(idx, reg, val)  \
		(ISP_REG_WR((idx), (reg),\
			(ISP_REG_CFG_RD((idx), (reg)) | (val))))

#define ISP_WORK_REG_WR(idx, reg, val) \
	(REG_WR(ISP_WORK_BASE_ADDR(idx) + (reg), (val)))
#define ISP_WORK_REG_CFG_RD(idx, reg)  (REG_RD(ISP_WORK_BASE_ADDR(idx) + (reg)))
#define ISP_WORK_REG_MWR(idx, reg, msk, val)  (ISP_WORK_REG_WR((idx), (reg), \
		((val) & (msk)) | \
		(ISP_WORK_REG_CFG_RD((idx), (reg)) & (~(msk)))))
#define ISP_WORK_REG_OWR(idx, reg, val)  \
		(ISP_WORK_REG_WR((idx), (reg),\
			(ISP_WORK_REG_CFG_RD((idx), (reg)) | (val))))

/* won't access CFG buffers */
#define ISP_HREG_WR(idx, reg, val) \
		(REG_WR(s_isp_regbase[idx] + (reg), (val)))

#define ISP_HREG_RD(idx, reg) \
		(REG_RD(s_isp_regbase[idx] + (reg)))

#define ISP_HREG_MWR(idx, reg, msk, val) \
		(REG_WR(s_isp_regbase[idx] + (reg), \
		((val) & (msk)) | \
		(REG_RD(s_isp_regbase[idx] + (reg)) & \
		(~(msk)))))

#define ISP_HREG_OWR(idx, reg, val) \
		(REG_WR(s_isp_regbase[idx] + (reg), \
			(REG_RD(s_isp_regbase[idx] + (reg)) | (val))))
#endif
