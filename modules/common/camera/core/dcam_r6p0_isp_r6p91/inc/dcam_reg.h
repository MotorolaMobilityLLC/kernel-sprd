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

#ifndef _REG_DCAM_TIGER_H_
#define _REG_DCAM_TIGER_H_

#ifndef CONFIG_64BIT
/*#include <soc/sprd/globalregs.h>*/
#endif
extern unsigned long            s_dcam_regbase;

#define BIT_0                                          0x01
#define BIT_1                                          0x02
#define BIT_2                                          0x04
#define BIT_3                                          0x08
#define BIT_4                                          0x10
#define BIT_5                                          0x20
#define BIT_6                                          0x40
#define BIT_7                                          0x80
#define BIT_8                                          0x0100
#define BIT_9                                          0x0200
#define BIT_10                                         0x0400
#define BIT_11                                         0x0800
#define BIT_12                                         0x1000
#define BIT_13                                         0x2000
#define BIT_14                                         0x4000
#define BIT_15                                         0x8000
#define BIT_16                                         0x010000
#define BIT_17                                         0x020000
#define BIT_18                                         0x040000
#define BIT_19                                         0x080000
#define BIT_20                                         0x100000
#define BIT_21                                         0x200000
#define BIT_22                                         0x400000
#define BIT_23                                         0x800000
#define BIT_24                                         0x01000000
#define BIT_25                                         0x02000000
#define BIT_26                                         0x04000000
#define BIT_27                                         0x08000000
#define BIT_28                                         0x10000000
#define BIT_29                                         0x20000000
#define BIT_30                                         0x40000000
#define BIT_31                                         0x80000000

#define GLOBAL_BASE                                    SPRD_GREG_BASE
#define ARM_GLB_GEN0                                   (GLOBAL_BASE + 0x008UL)
#define ARM_GLB_GEN3                                   (GLOBAL_BASE + 0x01CUL)
#define ARM_GLB_PLL_SCR                                (GLOBAL_BASE + 0x070UL)
/*#define GR_CLK_GEN5                                  (GLOBAL_BASE + 0x07CUL)*/
#define CLK_DLY_CTRL                                   (GLOBAL_BASE + 0x05CUL)

/*TODO: dcam powerOn will accomplished in cam_pw_domain. */
#define SPRD_MMAHB_BASE                                0x60D00000
#define DCAM_AHB_BASE                                  SPRD_MMAHB_BASE
#define DCAM_AHB_CTL0                               (DCAM_AHB_BASE + 0x0000UL)
#define DCAM_EB                                     (DCAM_AHB_CTL0 + 0x0000UL)
#define CSI2_DPHY_EB                                (DCAM_AHB_CTL0 + 0x0000UL)
#define DCAM_RST                                    (DCAM_AHB_CTL0 + 0x0004UL)
#define DCAM_MATRIX_EB                              (DCAM_AHB_CTL0 + 0x0008UL)
#define DCAM_CCIR_PCLK_EB                           (SPRD_MMCKG_BASE + 0x0028UL)


/*DCAM_RST*/
#define DCAM_RST_BIT					BIT_0
#define DCAM_PATH0_RST_BIT			BIT_1
#define DCAM_PATH1_RST_BIT			BIT_2
#define DCAM_PATH2_RST_BIT			BIT_3
#define DCAM_ROT_RST_BIT				BIT_4
#define ISP_LOG_RST_BIT					BIT_5
#define ISP_CFG_RST_BIT					BIT_6
#define VSP_RST_BIT						BIT_7
#define JPG_RST_BIT						BIT_8
#define CSI_RST_BIT						BIT_9
#define CAM_CKG_RST_BIT				BIT_10
#define CAM_MTX_RST_BIT				BIT_11
#define VSP_MTX_RST_BIT				BIT_12

/*DCAM INT*/
#define DCAM_ISR_SENSOR_SOF			BIT_0
#define DCAM_ISR_SENSOR_EOF			BIT_1
#define DCAM_ISR_CAP_SOF			BIT_2
#define DCAM_ISR_CAP_EOF			BIT_3
#define DCAM_ISR_CAM0_TX_DONE			BIT_4
#define DCAM_ISR_CAM0_BUF_OVF			BIT_5
#define DCAM_ISR_CAM1_TX_DONE			BIT_6
#define DCAM_ISR_CAM1_BUF_OVF			BIT_7
#define DCAM_ISR_CAM2_TX_DONE			BIT_8
#define DCAM_ISR_CAM2_BUF_OVF			BIT_9
#define DCAM_ISR_LINE_ERR			BIT_10
#define DCAM_ISR_FRM_ERR			BIT_11
#define DCAM_ISR_ISP_OVF			BIT_13
#define DCAM_ISR_MIPI_OVF			BIT_14
#define DCAM_ISR_ROT_DONE			BIT_15
#define DCAM_ISR_PDAF_TX_DONE			BIT_16
#define DCAM_ISR_PATH0_SOF			BIT_18
#define DCAM_ISR_PATH1_SOF			BIT_19
#define DCAM_ISR_PATH2_SOF			BIT_20
#define DCAM_ISR_PATH0_END			BIT_21
#define DCAM_ISR_PATH1_END			BIT_22
#define DCAM_ISR_PATH2_END			BIT_23
#define DCAM_ISR_MMU_VAOR_RD			BIT_24
#define DCAM_ISR_MMU_VAOR_WR			BIT_25
#define DCAM_ISR_MMU_INV_RD			BIT_26
#define DCAM_ISR_MMU_INV_WR			BIT_27
#define DCAM_ISR_MMU_UNS_RD			BIT_28
#define DCAM_ISR_MMU_UNS_WR			BIT_29
#define DCAM_ISR_MMU_PAOR_RD			BIT_30
#define DCAM_ISR_MMU_PAOR_WR			BIT_31

/*TODO*/
#define DCAM_BASE                                      (s_dcam_regbase)
#define DCAM_CFG                                       (DCAM_BASE + 0x0000UL)
#define DCAM_CONTROL                                   (DCAM_BASE + 0x0004UL)
#define DCAM_PATH0_CFG                                 (DCAM_BASE + 0x0008UL)
#define DCAM_PATH0_SRC_SIZE                            (DCAM_BASE + 0x000CUL)
#define DCAM_PATH1_CFG                                 (DCAM_BASE + 0x0010UL)
#define DCAM_PATH1_SRC_SIZE                            (DCAM_BASE + 0x0014UL)
#define DCAM_PATH1_DST_SIZE                            (DCAM_BASE + 0x0018UL)
#define DCAM_PATH1_TRIM_START                          (DCAM_BASE + 0x001CUL)
#define DCAM_PATH1_TRIM_SIZE                           (DCAM_BASE + 0x0020UL)
#define DCAM_PATH2_CFG                                 (DCAM_BASE + 0x0024UL)
#define DCAM_PATH2_SRC_SIZE                            (DCAM_BASE + 0x0028UL)
#define DCAM_PATH2_DST_SIZE                            (DCAM_BASE + 0x002CUL)
#define DCAM_PATH2_TRIM_START                          (DCAM_BASE + 0x0030UL)
#define DCAM_PATH2_TRIM_SIZE                           (DCAM_BASE + 0x0034UL)
/*#define REV_SLICE_VER                                (DCAM_BASE + 0x0038UL)*/
#define DCAM_INT_STS                                   (DCAM_BASE + 0x003CUL)
#define DCAM_INT_MASK                                  (DCAM_BASE + 0x0040UL)
#define DCAM_INT_CLR                                   (DCAM_BASE + 0x0044UL)
#define DCAM_INT_RAW                                   (DCAM_BASE + 0x0048UL)
#define DCAM_FRM_ADDR0                                 (DCAM_BASE + 0x004CUL)
#define DCAM_FRM_ADDR1                                 (DCAM_BASE + 0x0050UL)
#define DCAM_FRM_ADDR2                                 (DCAM_BASE + 0x0054UL)
#define DCAM_FRM_ADDR3                                 (DCAM_BASE + 0x0058UL)
#define DCAM_FRM_ADDR4                                 (DCAM_BASE + 0x005CUL)
#define DCAM_FRM_ADDR5                                 (DCAM_BASE + 0x0060UL)
#define DCAM_FRM_ADDR6                                 (DCAM_BASE + 0x0064UL)
#define DCAM_BURST_GAP                                 (DCAM_BASE + 0x0068UL)
#define DCAM_ENDIAN_SEL                                (DCAM_BASE + 0x006CUL)
#define DCAM_AHBM_STS                                  (DCAM_BASE + 0x0070UL)
#define DCAM_FRM_ADDR7                                 (DCAM_BASE + 0x0074UL)
#define DCAM_FRM_ADDR8                                 (DCAM_BASE + 0x0078UL)
#define DCAM_FRM_ADDR9                                 (DCAM_BASE + 0x007CUL)
#define DCAM_FRM_ADDR10                                (DCAM_BASE + 0x0080UL)
#define DCAM_FRM_ADDR11                                (DCAM_BASE + 0x0084UL)
#define DCAM_FRM_ADDR12                                (DCAM_BASE + 0x0088UL)
#define DCAM_FRM_ADDR13                                (DCAM_BASE + 0x008CUL)

#define REV_BURST_IN_CFG                               (DCAM_BASE + 0x0098UL)
#define REV_BURST_IN_TRIM_START                        (DCAM_BASE + 0x009CUL)
#define REV_BURST_IN_TRIM_SIZE                         (DCAM_BASE + 0x00A0UL)
#define YUV_SHRINK_CFG                                 (DCAM_BASE + 0x00B0UL)
#define YUV_EFFECT_CFG                                 (DCAM_BASE + 0x00B4UL)
#define YUV_REGULAR_CFG                                (DCAM_BASE + 0x00B8UL)
#define REV_SLICE_CFG                                  (DCAM_BASE)
#define PATH1_SLICE_O_VCNT                             (DCAM_BASE)
#define PATH2_SLICE_O_VCNT                             (DCAM_BASE)
#define DCAM_PATH0_TRIM_START                          (DCAM_BASE + 0x00C0UL)
#define DCAM_PATH0_TRIM_SIZE                           (DCAM_BASE + 0x00C4UL)
#define CAM0_PITCH                                     (DCAM_BASE + 0x00C8UL)
#define CAM1_PITCH                                     (DCAM_BASE + 0x00CCUL)
#define CAM2_PITCH                                     (DCAM_BASE + 0x00D0UL)
#define AXIM_DBG_STS                                   (DCAM_BASE + 0x00D4UL)
#define AXIM_AWQOS                                     (DCAM_BASE + 0x00D8UL)
#define AXIM_ARQOS                                     (DCAM_BASE + 0x00DCUL)
#define MIPI_CAP_CFG                                   (DCAM_BASE + 0x0100UL)
#define MIPI_CAP_FRM_CTRL                              (DCAM_BASE + 0x0104UL)
#define MIPI_CAP_START                                 (DCAM_BASE + 0x0108UL)
#define MIPI_CAP_END                                   (DCAM_BASE + 0x010CUL)
#define MIPI_CAP_IMG_DECI                              (DCAM_BASE + 0x0110UL)
#define CAP_SENSOR_CTRL                                (DCAM_BASE + 0x0114UL)
#define IMAGE_CONTROL                                  (DCAM_BASE + 0x0118UL)
#define ROTATE_PATH_CFG                                (DCAM_BASE + 0x0120UL)
#define ROTATE_SRC_WIDTH                               (DCAM_BASE + 0x0124UL)
#define ROTATE_OFFSET_START                            (DCAM_BASE + 0x0128UL)
#define ROTATE_IMG_SIZE                                (DCAM_BASE + 0x012CUL)
#define MIPI_REDUNDANT                                 (DCAM_BASE + 0x0130UL)
#define PDAF_CAP_FRM_SIZE                              (DCAM_BASE + 0x0134UL)
#define ISP_OUT_FRM_SIZE                               (DCAM_BASE + 0x0138UL)
#define MIPI_CAP_WIDTH_SIZE                            (DCAM_BASE + 0x013CUL)
#define MIPI_CAP_HEIGHT_SIZE                           (DCAM_BASE + 0x0140UL)
#define SPARE_REG_EN                                   (DCAM_BASE + 0x0144UL)
#define AXIM_SPARE_REG                                 (DCAM_BASE + 0x0148UL)
#define DCAM_SPARE_REG                                 (DCAM_BASE + 0x014CUL)
#define DCAM_IP_REVISION                               (DCAM_BASE + 0x0180UL)
#define MMU_EN                                         (DCAM_BASE + 0x0200UL)
#define MMU_UPDATE                                     (DCAM_BASE + 0x0204UL)
#define MMU_MIN_VPN                                    (DCAM_BASE + 0x0208UL)
#define MMU_VPN_RANGE                                  (DCAM_BASE + 0x020CUL)
#define MMU_PT_ADDR                                    (DCAM_BASE + 0x0210UL)
#define MMU_DEFAULT_PAGE                               (DCAM_BASE + 0x0214UL)
#define MMU_VAOR_ADDR_RD                               (DCAM_BASE + 0x0218UL)
#define MMU_VAOR_ADDR_WR                               (DCAM_BASE + 0x021CUL)
#define MMU_INV_ADDR_RD                                (DCAM_BASE + 0x0220UL)
#define MMU_INV_ADDR_WR                                (DCAM_BASE + 0x0224UL)
#define MMU_UNS_ADDR_RD                                (DCAM_BASE + 0x0228UL)
#define MMU_UNS_ADDR_WR                                (DCAM_BASE + 0x022CUL)
#define MMU_MISS_CNT                                   (DCAM_BASE + 0x0230UL)
#define MMU_PT_UPDATE_QOS                              (DCAM_BASE + 0x0234UL)
#define MMU_VERSION                                    (DCAM_BASE + 0x0238UL)
#define MMU_MIN_PPN1                                   (DCAM_BASE + 0x023CUL)
#define MMU_PPN_RANGE1                                 (DCAM_BASE + 0x0240UL)
#define MMU_MIN_PPN2                                   (DCAM_BASE + 0x0244UL)
#define MMU_PPN_RANGE2                                 (DCAM_BASE + 0x0248UL)
#define MMU_VPN_PAOR_RD                                (DCAM_BASE + 0x024CUL)
#define MMU_VPN_PAOR_WR                                (DCAM_BASE + 0x0250UL)
#define MMU_PPN_PAOR_RD                                (DCAM_BASE + 0x0254UL)
#define MMU_PPN_PAOR_WR                                (DCAM_BASE + 0x0258UL)
#define MMU_REG_AU_MANAGE                              (DCAM_BASE + 0x025CUL)

#define BLC_CFG                                        (DCAM_BASE)
#define BLC_START                                      (DCAM_BASE)
#define BLC_END                                        (DCAM_BASE)
#define BLC_TARGET                                     (DCAM_BASE)
#define BLC_MANUAL                                     (DCAM_BASE)
#define BLC_VALUE1                                     (DCAM_BASE)
#define BLC_VALUE2                                     (DCAM_BASE)

#define DCAM_END                                       (DCAM_BASE + 0x4000)

#define DCAM_PATH_NUM                                  3
#define DCAM_CAP_SKIP_FRM_MAX                          16
#define DCAM_FRM_DECI_FAC_MAX                          4
#define DCAM_CAP_FRAME_WIDTH_MAX                       4092
#define DCAM_CAP_FRAME_HEIGHT_MAX                      4092

#define DCAM_PATH_FRAME_WIDTH_MAX                      4092
#define DCAM_PATH_FRAME_HEIGHT_MAX                     4092
#define DCAM_PATH1_FRAME_WIDTH_MAX                      4092
#define DCAM_PATH1_FRAME_HEIGHT_MAX                     4092
#define DCAM_PATH2_FRAME_WIDTH_MAX                     4092
#define DCAM_PATH2_FRAME_HEIGHT_MAX                    4092

/* cap deci: 0 - 1/8 */
#define DCAM_CAP_X_DECI_FAC_MAX                4
#define DCAM_CAP_Y_DECI_FAC_MAX                4
#define DCAM_PATH_FRAME_ROT_MAX                4
#define DCAM_JPG_BUF_UNIT                      (1 << 15)
#define DCAM_JPG_UNITS                         (1 << 10)
#define DCAM_SC_COEFF_UP_MAX                   4 /* path scaling: 1/8 - 4 */
#define DCAM_SC_COEFF_DOWN_MAX                 4 /* path scaling: 1/4 - 4*/
#define DCAM_PATH_DECI_FAC_MAX                 4 /* path deci: 1/2 - 1/16*/
#define DCAM_PATH1_LINE_BUF_LENGTH             1920/*2048*/

#define DCAM_PATH2_LINE_BUF_LENGTH                     3264/*4096*/
#define DCAM_ISP_LINE_BUF_LENGTH                       3280
#define DCAM_SCALING_THRESHOLD                         2300

/*TODO: not found IRQ_DCAM_INT declare, declare here for compile pass*/
#define IRQ_DCAM_INT                                   20
#define DCAM_IRQ                                       IRQ_DCAM_INT

#define DCAM_PIXEL_ALIGN_WIDTH                         4
#define DCAM_PIXEL_ALIGN_HEIGHT                        2

#define DCAM_PATH_NUM                                  3

enum {
	DCAM_PATH0 = 0,
	DCAM_PATH1,
	DCAM_PATH2,
	DCAM_PATH_MAX
};

enum dcam_id {
	DCAM_ID_0 = 0,
	DCAM_ID_1 = 1,
	DCAM_ID_MAX = 2,
};

#ifdef __cplusplus
}
#endif

#endif
/*_REG_DCAM_TIGER_H_*/


