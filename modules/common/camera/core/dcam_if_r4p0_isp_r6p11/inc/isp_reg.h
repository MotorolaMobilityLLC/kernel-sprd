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

#ifndef _REG_ISP_H_
#define _REG_ISP_H_

#include <linux/types.h>

#include "sprd_mm.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_REG: %d: %d %s: " \
	fmt, current->pid, __LINE__, __func__

#define ISP_MAX_COUNT			2

#define ISP_REG_SIZE			0x40000UL
#define ISP_REG_DEF			0x4000UL  /* 0x0000 ~ 0xFFFF */

#define ISP_PMU_EN                     (0x8010UL)

/*isp sub block: CFG*/
#define ISP_CFG_STATUS0						(0x8100UL)
#define ISP_CFG_STATUS1						(0x8104UL)
#define ISP_CFG_STATUS2						(0x8108UL)
#define ISP_CFG_STATUS3				       (0x810CUL)
#define ISP_CFG_PAMATER						(0x8110UL)
#define ISP_CFG_TM_NUM						(0x8114UL)
#define ISP_CFG_CAP0_TH						(0x8118UL)
#define ISP_CFG_CAP1_TH						(0x811CUL)
#define ISP_CFG_PRE0_CMD_ADDR				(0x8120UL)
#define ISP_CFG_PRE1_CMD_ADDR				(0x8124UL)
#define ISP_CFG_CAP0_CMD_ADDR				(0x8128UL)
#define ISP_CFG_CAP1_CMD_ADDR				(0x812CUL)
#define ISP_CFG_PRE0_START					(0x8130UL)
#define ISP_CFG_PRE1_START					(0x8134UL)
#define ISP_CFG_CAP0_START					(0x8138UL)
#define ISP_CFG_CAP1_START					(0x813CUL)
#define ISP_CFG_REG_RDY						(0x8140UL)
#define ISP_CFG_CAP_FMCU_RDY				(0x8144UL)
#define ISP_CFG_STATUS						(0x8150UL)

/* isp sub block: FMCU
 * 1.6.5.2 FMCU CMD format:SW will tell FMCU do action
 * by address 0x60a0F030
 */
#define ISP_FMCU_STATUS0				(0xF000UL)
#define ISP_FMCU_STATUS1				(0xF004UL)
#define ISP_FMCU_STATUS2				(0xF008UL)
#define ISP_FMCU_STATUS3				(0xF00CUL)
#define ISP_FMCU_STATUS4				(0xF010UL)
#define ISP_FMCU_CTRL					(0xF014UL)
#define ISP_FMCU_DDR_ADR				(0xF018UL)
#define ISP_FMCU_AHB_ARB				(0xF01CUL)
#define ISP_FMCU_START					(0xF020UL)
#define ISP_FMCU_TIME_OUT_THD			(0xF024UL)
#define ISP_FMCU_CMD_READY				(0xF028UL)
#define ISP_FMCU_ISP_REG_REGION		(0xF02CUL)
#define ISP_FMCU_CMD					(0xF030UL)
#define ISP_FMCU_STOP					(0xF034UL)
#define ISP_FMCU_SW_TRIGGER			(0xF03CUL)

/*isp sub block: Interrupt*/
#define ISP_P0_INT_BASE					(0x0000UL)
#define ISP_P1_INT_BASE					(0x0D00UL)
#define ISP_C0_INT_BASE					(0x0E00UL)
#define ISP_C1_INT_BASE					(0x0F00UL)

#define ISP_INT_STATUS					(0x0000UL)
#define ISP_INT_STATUS1					(0x0004UL)
#define ISP_INT_EN0						(0x0010UL)
#define ISP_INT_CLR0					(0x0014UL)
#define ISP_INT_EN0_SET					(0x0080UL)
#define ISP_INT_EN0_CLR					(0x00D0UL)
#define ISP_INT_RAW0					(0x0018UL)
#define ISP_INT_INT0					(0x001CUL)
#define ISP_INT_EN1						(0x0020UL)
#define ISP_INT_CLR1					(0x0024UL)
#define ISP_INT_EN1_SET					(0x0090UL)
#define ISP_INT_EN1_CLR					(0x00E0UL)
#define ISP_INT_RAW1					(0x0028UL)
#define ISP_INT_INT1					(0x002CUL)
#define ISP_INT_EN2						(0x0030UL)
#define ISP_INT_CLR2					(0x0034UL)
#define ISP_INT_EN2_SET					(0x00A0UL)
#define ISP_INT_EN2_CLR					(0x00F0UL)
#define ISP_INT_RAW2					(0x0038UL)
#define ISP_INT_INT2					(0x003CUL)
#define ISP_INT_EN3						(0x0040UL)
#define ISP_INT_CLR3					(0x0044UL)
#define ISP_INT_EN3_SET					(0x00B0UL)
#define ISP_INT_EN3_CLR					(0x0100UL)
#define ISP_INT_RAW3					(0x0048UL)
#define ISP_INT_INT3					(0x004CUL)
#define ISP_INT_ALL_DONE_CTRL			(0x0050UL)
#define ISP_INT_SKIP_CTRL				(0x0054UL)
#define ISP_INT_SKIP_CTRL1				(0x0058UL)
#define ISP_INT_ALL_DONE_SRC_CTRL		(0x005CUL)

/*isp sub block: Fetch*/
#define ISP_FETCH_STATUS0                   (0x0100UL)
#define ISP_FETCH_STATUS1                   (0x0104UL)
#define ISP_FETCH_STATUS2                   (0x0108UL)
#define ISP_FETCH_STATUS3                   (0x010CUL)
#define ISP_FETCH_PARAM0					(0x0110UL)
#define ISP_FETCH_MEM_SLICE_SIZE			(0x0114UL)
#define ISP_FETCH_SLICE_Y_ADDR				(0x0118UL)
#define ISP_FETCH_Y_PITCH                   (0x011CUL)
#define ISP_FETCH_SLICE_U_ADDR				(0x0120UL)
#define ISP_FETCH_U_PITCH                   (0x0124UL)
#define ISP_FETCH_SLICE_V_ADDR				(0x0128UL)
#define ISP_FETCH_V_PITCH                   (0x012CUL)
#define ISP_FETCH_MIPI_PARAM		        (0x0130UL)
#define ISP_FETCH_LINE_DLY_CTRL				(0x0134UL)
#define ISP_FETCH_PARAM1					(0x0138UL)
#define ISP_FETCH_START						(0x013CUL)
#define ISP_FETCH_STATUS4					(0x0140UL)
#define ISP_FETCH_STATUS5					(0x0144UL)
#define ISP_FETCH_STATUS6					(0x0148UL)
#define ISP_FETCH_STATUS7					(0x014CUL)
#define ISP_FETCH_STATUS8					(0x0150UL)
#define ISP_FETCH_STATUS9					(0x0154UL)
#define ISP_FETCH_STATUS10					(0x0158UL)
#define ISP_FETCH_STATUS11					(0x015CUL)
#define ISP_FETCH_STATUS12					(0x0160UL)
#define ISP_FETCH_STATUS13					(0x0164UL)
#define ISP_FETCH_STATUS14					(0x0168UL)
#define ISP_FETCH_STATUS15					(0x016CUL)
#define ISP_FETCH_MIPI_STATUS				(0x0170UL)

/*isp sub block: Store*/
#define ISP_STORE_BASE					(0x0200UL)
#define ISP_STORE_PRE_CAP_BASE			(0xC100UL)
#define ISP_STORE_VID_BASE				(0xD100UL)

/*used for store_out, store_Pre_Cap and store_Vid*/
#define ISP_STORE_STATUS0					(0x0000UL)
#define ISP_STORE_STATUS1					(0x0004UL)
#define ISP_STORE_STATUS2					(0x0008UL)
#define ISP_STORE_STATUS3					(0x000CUL)
#define ISP_STORE_PARAM						(0x0010UL)
#define ISP_STORE_SLICE_SIZE				(0x0014UL)
#define ISP_STORE_BORDER					(0x0018UL)
#define ISP_STORE_SLICE_Y_ADDR				(0x001CUL)
#define ISP_STORE_Y_PITCH					(0x0020UL)
#define ISP_STORE_SLICE_U_ADDR				(0x0024UL)
#define ISP_STORE_U_PITCH					(0x0028UL)
#define ISP_STORE_SLICE_V_ADDR				(0x002CUL)
#define ISP_STORE_V_PITCH					(0x0030UL)
#define ISP_STORE_READ_CTRL					(0x0034UL)
#define ISP_STORE_SHADOW_CLR_SEL			(0x0038UL)
#define ISP_STORE_SHADOW_CLR				(0x003CUL)
#define ISP_STORE_TRIM_STATUS				(0x0040UL)

/*isp sub block: SCALER*/
#define ISP_SCALER_PRE_CAP_BASE	(0xC000UL)
#define ISP_SCALER_VID_BASE			(0xD000UL)

#define ISP_SCALER_PRE_CFG			0xC014UL
#define ISP_SCALER_PRE_TRIM1_SIZE	0xC044UL
#define ISP_SCALER_VID_CFG			0xD014UL
#define ISP_SCALER_VID_TRIM1_SIZE	0xD044UL
#define ISP_SCALER_CAP_CFG			0xE014UL
#define ISP_SCALER_CAP_TRIM1_SIZE	0xE044UL

/*isp SCALER PRE_CAP/VID offset*/
#define ISP_SCALER_CFG				(0x0010UL)
#define ISP_SCALER_SRC_SIZE			(0x0014UL)
#define ISP_SCALER_DES_SIZE			(0x0018UL)
#define ISP_SCALER_TRIM0_START		(0x001CUL)
#define ISP_SCALER_TRIM0_SIZE		(0x0020UL)
#define ISP_SCALER_IP				(0x0024UL)
#define ISP_SCALER_CIP				(0x0028UL)
#define ISP_SCALER_FACTOR			(0x002CUL)
#define ISP_SCALER_TRIM1_START		(0x0030UL)
#define ISP_SCALER_TRIM1_SIZE		(0x0034UL)
#define ISP_SCALER_VER_IP			(0x0038UL)
#define ISP_SCALER_VER_CIP			(0x003CUL)
#define ISP_SCALER_VER_FACTOR		(0x0040UL)
#define ISP_SCALER_DEBUG			(0x0044UL)
#define ISP_SCALER_HBLANK			(0x0048UL)
#define ISP_SCALER_FRAME_CNT_CLR	(0x004CUL)
#define ISP_SCALER_RES				(0x0050UL)
#define ISP_SCALER_SHRINK_CFG		(0x0054UL)
#define ISP_SCALER_EFFECT_CFG		(0x0058UL)
#define ISP_SCALER_REGULAR_CFG		(0x005CUL)
#define ISP_SCALER_LUMA_HCOEFF		(0x0400UL)
#define ISP_SCALER_CHROMA_HCOEFF	(0x0480UL)
#define ISP_SCALER_LUMA_VCOEFF		(0x04F0UL)
#define ISP_SCALER_CHROMA_VCOEFF	(0x08F0UL)

/*isp sub block: dispatch*/
#define ISP_DISPATCH_STATUS_CH0			(0x0300UL)
#define ISP_DISPATCH_STATUS_DBG_CH0		(0x0304UL)
#define ISP_DISPATCH_STATUS_COMM		(0x0308UL)
#define ISP_DISPATCH_STATUS1_CH0		(0x030CUL)
#define ISP_DISPATCH_CH0_BAYER			(0x0310UL)
#define ISP_DISPATCH_CH0_SIZE			(0x0314UL)
#define ISP_DISPATCH_DLY				(0x0318UL)
#define ISP_DISPATCH_HW_CTRL_CH0		(0x031CUL)
#define ISP_DISPATCH_DLY1				(0x0320UL)
/* #define ISP_PIPE_BUF_CTRL_CH0			(0x0324UL) */

/*isp sub block: arbiter*/
#define ISP_ARBITER_WR_STATUS		(0x0400UL)
#define ISP_ARBITER_RD_STATUS		(0x0404UL)
#define ISP_ARBITER_ENDIAN_COMM		(0x0410UL)
#define ISP_ARBITER_ENDIAN_CH0		(0x0414UL)
#define ISP_ARBITER_WR_PARAM0		(0x041CUL)
#define ISP_ARBITER_WR_PARAM1		(0x0420UL)
#define ISP_ARBITER_WR_PARAM2		(0x0424UL)
#define ISP_ARBITER_WR_PARAM3		(0x0428UL)
#define ISP_ARBITER_WR_PARAM4		(0x042CUL)
#define ISP_ARBITER_WR_PARAM5		(0x0430UL)
#define ISP_ARBITER_WR_PARAM6		(0x0434UL)
#define ISP_ARBITER_WR_PARAM7		(0x0438UL)
#define ISP_ARBITER_RD_PARAM0		(0x043CUL)
#define ISP_ARBITER_RD_PARAM1		(0x0440UL)
#define ISP_ARBITER_RD_PARAM2		(0x0444UL)
#define ISP_ARBITER_RD_PARAM3		(0x0448UL)
#define ISP_ARBITER_RD_PARAM4		(0x044CUL)
#define ISP_ARBITER_RD_PARAM5		(0x0450UL)
#define ISP_ARBITER_RD_PARAM6		(0x0454UL)
#define ISP_ARBITER_RD_PARAM7		(0x0458UL)

/*isp sub block: axi*/
#define ISP_AXI_WR_MASTER_STATUS			(0x0500UL)
#define ISP_AXI_RD_MASTER_STATUS			(0x0504UL)
#define ISP_AXI_ITI2AXIM_CTRL				(0x0510UL)
#define ISP_AXI_ARBITER_WQOS				(0x0514UL)
#define ISP_AXI_ARBITER_RQOS				(0x0518UL)
#define ISP_AXI_ISOLATION				(0x051CUL)
#define ISP_AXI_STATUS0				(0x0520UL)
#define ISP_AXI_STATUS1				(0x0524UL)
#define ISP_AXI_STATUS2				(0x0528UL)
#define ISP_AXI_STATUS3				(0x052CUL)


/*isp sub block: common*/
#define ISP_COMMON_VERSION				(0x0700UL)
#define ISP_COMMON_STATUS0				(0x0704UL)
#define ISP_COMMON_STATUS1				(0x0708UL)
#define ISP_COMMON_STATUS2				(0x070CUL)
#define ISP_COMMON_SPACE_SEL			(0x0710UL)
#define ISP_COMMON_SCL_PATH_SEL			(0x0714UL)
#define ISP_COMMON_BIN_AFL_CTRL			(0x0718UL)
#define ISP_COMMON_GCLK_CTRL_0			(0x071CUL)
#define ISP_COMMON_GCLK_CTRL_1			(0x0720UL)
#define ISP_COMMON_GCLK_CTRL_2			(0x0724UL)
#define ISP_COMMON_GCLK_CTRL_3			(0x0728UL)
#define ISP_COMMON_SOFT_RST_CTRL		(0x072CUL)
#define ISP_COMMON_SHADOW_CTRL_CH0		(0x0730UL)
#define ISP_COMMON_PMU_RAM_MASK			(0x0734UL)
/* #define ISP_IDX_MODE					(0x0738UL) */

/*isp sub block: MMU*/
#define MMU_INT_EN				(0x0800UL)
#define MMU_INT_CLR				(0x0804UL)
#define MMU_INT_RAW_STS				(0x0808UL)
#define MMU_INT_MASKED_STS			(0x080CUL)
#define MMU_W_BYPASS				(0x0810UL)
#define MMU_R_BYPASS						(0x0814UL)
#define MMU_W_TLB_EN				(0x0818UL)
#define MMU_R_TLB_EN				(0x081CUL)
#define MMU_CFG					(0x0820UL)
#define MMU_FIRST_VPN				(0x0824UL)
#define MMU_PTBA_LO				(0x0828UL)
#define MMU_PTBA_HI				(0x082CUL)
#define MMU_PT_SIZE				(0x0830UL)
#define MMU_DEFAULT_PPN				(0x0834UL)
#define MMU_TRANS_CNT_R				(0x0900UL)
#define MMU_TLB_MISS_CNT_R			(0x0904UL)
#define MMU_FIRST_OFR_ADDR_LO_R				(0x0908UL)
#define MMU_FIRST_OFR_ADDR_HI_R				(0x090CUL)
#define MMU_LATEST_OFR_ADDR_LO_R		(0x0910UL)
#define MMU_LATEST_OFR_ADDR_HI_R		(0x0914UL)
#define MMU_FIRST_INVALID_PAGE_ADDR_LO_R	(0x0918UL)
#define MMU_FIRST_INVALID_PAGE_ADDR_HI_R	(0x091CUL)
#define MMU_LATEST_INVALID_PAGE_ADDR_LO_R	(0x0920UL)
#define MMU_LATEST_INVALID_PAGE_ADDR_HI_R	(0x0924UL)
#define MMU_FIRST_UNSECURE_ADDR_LO_R		(0x0928UL)
#define MMU_FIRST_UNSECURE_ADDR_HI_R		(0x092CUL)
#define MMU_LATEST_UNSECURE_ADDR_LO_R		(0x0930UL)
#define MMU_LATEST_UNSECURE_ADDR_HI_R		(0x0934UL)
#define MMU_TRANS_CNT_W						(0x0938UL)
#define MMU_TLB_MISS_CNT_W					(0x093CUL)
#define MMU_FIRST_OFR_ADDR_LO_W				(0x0940UL)
#define MMU_FIRST_OFR_ADDR_HI_W				(0x0944UL)
#define MMU_LATEST_OFR_ADDR_LO_W			(0x0948UL)
#define MMU_LATEST_OFR_ADDR_HI_W			(0x094CUL)
#define MMU_FIRST_INVALID_PAGE_ADDR_LO_W	(0x0950UL)
#define MMU_FIRST_INVALID_PAGE_ADDR_HI_W	(0x0954UL)
#define MMU_LATEST_INVALID_PAGE_ADDR_LO_W	(0x0958UL)
#define MMU_LATEST_INVALID_PAGE_ADDR_HI_W	(0x095CUL)
#define MMU_FIRST_UNSECURE_ADDR_LO_W		(0x0960UL)
#define MMU_FIRST_UNSECURE_ADDR_HI_W		(0x0964UL)
#define MMU_LATEST_UNSECURE_ADDR_LO_W		(0x0968UL)
#define MMU_LATEST_UNSECURE_ADDR_HI_W		(0x096CUL)
#define MMU_DEBUG				(0x0970UL)
#define MMU_PARAM				(0x09F8UL)
#define MMU_REV					(0x09FCUL)
#define MMU_INT_EN_SEC				(0x0A00UL)
#define MMU_INT_CLR_SEC				(0x0A04UL)
#define MMU_INT_RAW_STS_SEC			(0x0A08UL)
#define MMU_INT_MASKED_STS_SEC			(0x0A0CUL)
#define MMU_W_BYPASS_SEC			(0x0A10UL)
#define MMU_R_BYPASS_SEC			(0x0A14UL)
#define MMU_W_TLB_EN_SEC			(0x0A18UL)
#define MMU_R_TLB_EN_SEC			(0x0A1CUL)
#define MMU_CFG_SEC				(0x0A20UL)
#define MMU_FIRST_VPN_SEC			(0x0A24UL)
#define MMU_PTBA_LO_SEC				(0x0A28UL)
#define MMU_PTBA_HI_SEC				(0x0A2CUL)
#define MMU_PT_SIZE_SEC				(0x0A30UL)
#define MMU_DEFAULT_PPN_SEC			(0x0A34UL)
#define MMU_SECURITY_EN				(0x0BFCUL)

/*isp sub block: Pre Global	Gain*/
#define ISP_PGG_STATUS				(0x1000UL)
#define ISP_PGG_PARAM				(0x1010UL)

/*isp sub block: BLC: back level calibration*/
#define ISP_BLC_STATUS				(0x1100UL)
#define ISP_BLC_PARAM				(0x1110UL)
#define ISP_BLC_B_PARAM_R_B			(0x1114UL)
#define ISP_BLC_B_PARAM_G			(0x1118UL)

/*isp sub block: RGB-Gain*/
#define ISP_RGBG_STATUS				(0x1200UL)
#define ISP_RGBG_PARAM				(0x1210UL)
#define ISP_RGBG_RB				(0x1214UL)
#define ISP_RGBG_G				(0x1218UL)
#define ISP_RGBG_PARAM0				(0x121CUL)
#define ISP_RGBG_PARAM1				(0x1220UL)
#define ISP_RGBG_PARAM2				(0x1224UL)
#define ISP_RGBG_STATUS0			(0x1228UL)
#define ISP_RGBG_STATUS1			(0x122CUL)
#define ISP_RGBG_STATUS2			(0x1230UL)
#define ISP_RGBG_RANDOM_INIT			(0x1234UL)

/*isp sub block: POST BLC*/
#define ISP_POST_BLC_STATUS			(0x1300UL)
#define ISP_POST_BLC_PARA			(0x1310UL)
#define ISP_POST_BLC_B_PARA_R_B			(0x1314UL)
#define ISP_POST_BLC_PARA_G			(0x1318UL)

/*isp sub block: NLC*/
#define ISP_NLC_STATUS					(0x1400UL)
#define ISP_NLC_PARA					(0x1410UL)
#define ISP_NLC_PARA_R0					(0x1414UL)
#define ISP_NLC_PARA_R9					(0x1438UL)
#define ISP_NLC_PARA_G0					(0x143CUL)
#define ISP_NLC_PARA_G9					(0x1460UL)
#define ISP_NLC_PARA_B0					(0x1464UL)
#define ISP_NLC_PARA_B9					(0x1488UL)
#define ISP_NLC_PARA_L0					(0x148CUL)
#define ISP_NLC_PARA_L8					(0x14ACUL)

/*isp sub block: lens shading calibration*/
#define ISP_LENS_STATUS			(0x1500UL)
#define ISP_LENS_PARAM			(0x1510UL)
#define ISP_LENS_PARAM_ADDR		(0x1514UL)
#define ISP_LENS_SLICE_POS		(0x1518UL)
#define ISP_LENS_LOAD_EB		(0x151CUL)
#define ISP_LENS_GRID_PITCH		(0x1520UL)
#define ISP_LENS_GRID_SIZE		(0x1524UL)
#define ISP_LENS_LOAD_BUF		(0x1528UL)
#define ISP_LENS_MISC			(0x152CUL)
#define ISP_LENS_SLICE_SIZE		(0x1530UL)
#define ISP_LENS_INIT_COOR		(0x1534UL)
#define ISP_LENS_Q0_VALUE		(0x1538UL)
#define ISP_LENS_Q1_VALUE		(0x153CUL)
#define ISP_LENS_Q2_VALUE		(0x1540UL)
#define ISP_LENS_Q3_VALUE		(0x1544UL)
#define ISP_LENS_Q4_VALUE		(0x1548UL)
#define ISP_LENS_WEIGHT_ADDR		(0x36400UL) /*need further confirm*/

/*isp sub block: binning*/
#define ISP_BINNING_STATUS0					(0x1600UL)
#define ISP_BINNING_STATUS1					(0x1604UL)
#define ISP_BINNING_PARAM					(0x1610UL)
#define ISP_BINNING_MEM_ADDR				(0x1614UL)
#define ISP_BINNING_DDR_WR_NUM				(0x1618UL)
#define ISP_BINNING_CFG_READY				(0x161CUL)
#define ISP_BINNING_SKIP_NUM_CLR			(0x1620UL)

/*isp sub block: AWBC*/
#define ISP_AWBC_STATUS					(0x1800UL)
#define ISP_AWBC_PARAM					(0x1810UL)
#define ISP_AWBC_GAIN0					(0x1814UL)
#define ISP_AWBC_GAIN1					(0x1818UL)
#define ISP_AWBC_THRD					(0x181CUL)
#define ISP_AWBC_OFFSET0				(0x1820UL)
#define ISP_AWBC_OFFSET1				(0x1824UL)

/*isp sub block: AEM*/
#define ISP_AEM_STATUS0				(0x1900UL)
#define ISP_AEM_STATUS1				(0x1904UL)
#define ISP_AEM_PARAM				(0x1910UL)
#define ISP_AEM_OFFSET				(0x1914UL)
#define ISP_AEM_BLK_SIZE			(0x1918UL)
#define ISP_AEM_SLICE_SIZE			(0x191CUL)
#define ISP_AEM_DDR_ADDR			(0x1920UL)
#define ISP_AEM_DDR_WR_NUM			(0x1924UL)
#define ISP_AEM_CFG_READY			(0x1928UL)
#define ISP_AEM_SKIP_NUM_CLR		(0x192CUL)

/*isp sub block: new bpc*/
#define ISP_BPC_STATUS					(0x1A00UL)
#define ISP_BPC_PARAM					(0x1A10UL)
#define ISP_BPC_BAD_PIXEL_TH0			(0x1A14UL)
#define ISP_BPC_BAD_PIXEL_TH1			(0x1A18UL)
#define ISP_BPC_BAD_PIXEL_TH2			(0x1A1CUL)
#define ISP_BPC_BAD_PIXEL_TH3			(0x1A20UL)
#define ISP_BPC_FLAT_TH					(0x1A24UL)
#define ISP_BPC_EDGE_RATIO				(0x1A28UL)
#define ISP_BPC_BAD_PIXEL_PARAM			(0x1A2CUL)
#define ISP_BPC_BAD_PIXEL_COEF			(0x1A30UL)
#define ISP_BPC_LUTWORD0				(0x1A34UL)
#define ISP_BPC_MAP_ADDR				(0x1A54UL)
#define ISP_BPC_MAP_CTRL				(0x1A58UL)
#define ISP_BPC_MAP_CTRL1				(0x1A5CUL)
#define ISP_BPC_OUT_ADDR			(0x1A60UL)
#define ISP_BPC_OUT_START_POS			(0x1A64UL)
#define ISP_BPC_OUT_ROI_START			(0x1A68UL)
#define ISP_BPC_OUT_ROI_END				(0x1A6CUL)

/*isp sub block: GrGb Curve - GC*/
#define ISP_GRGB_STATUS					(0x1B00UL)
#define ISP_GRGB_CTRL					(0x1B10UL)
#define ISP_GRGB_CFG0					(0x1B14UL)
#define ISP_GRGB_LUM_FLAT_T				(0x1B18UL)
#define ISP_GRGB_LUM_FLAT_T_R			(0x1B1CUL)
#define ISP_GRGB_LUM_EDGE_T			(0x1B20UL)
#define ISP_GRGB_LUM_EDGE_T_R			(0x1B24UL)
#define ISP_GRGB_LUM_TEX_T			(0x1B28UL)
#define ISP_GRGB_LUM_TEXT_T_R			(0x1B2CUL)
#define ISP_GRGB_FREZ_FLAT_T			(0x1B30UL)
#define ISP_GRGB_FREZ_FLAT_T_R			(0x1B34UL)
#define ISP_GRGB_FREZ_EDGE_T			(0x1B38UL)
#define ISP_GRGB_FREZ_EDGE_T_R			(0x1B3CUL)
#define ISP_GRGB_FREZ_TEX_T			(0x1B40UL)
#define ISP_GRGB_FREZ_TEXT_T_R			(0x1B44UL)

/*isp sub block: radial	lnc*/
#define ISP_RLSC_STATUS0				   (0x1F00UL)
#define ISP_RLSC_STATUS1				   (0x1F04UL)
#define ISP_RLSC_CTRL					   (0x1F10UL)
#define ISP_RLSC_CFG0					   (0x1F14UL)
#define ISP_RLSC_CFG1					   (0x1F18UL)
#define ISP_RLSC_CFG2					   (0x1F1CUL)
#define ISP_RLSC_CFG3					   (0x1F20UL)
#define ISP_RLSC_CFG4					   (0x1F24UL)
#define ISP_RLSC_R_CFG0					   (0x1F28UL)
#define ISP_RLSC_R_CFG1					   (0x1F2CUL)
#define ISP_RLSC_R2_CFG0				   (0x1F30UL)
#define ISP_RLSC_DR2_CFG0				   (0x1F34UL)
#define ISP_RLSC_R2_CFG1				   (0x1F38UL)
#define ISP_RLSC_DR2_CFG1				   (0x1F3CUL)
#define ISP_RLSC_R2_CFG2				   (0x1F40UL)
#define ISP_RLSC_DR2_CFG2				   (0x1F44UL)
#define ISP_RLSC_R2_CFG3				   (0x1F48UL)
#define ISP_RLSC_DR2_CFG3				   (0x1F4CUL)

/*isp sub block: NLM VST IVST*/
#define ISP_VST_PARA			(0x1C10UL)
#define ISP_IVST_PARA			(0x1E10UL)
#define ISP_NLM_LB_STATUS0		(0x1D00UL)
#define ISP_NLM_LB_STATUS1		(0x1D04UL)
#define ISP_NLM_STATUS			(0x1D08UL)
#define ISP_NLM_PARA			(0x1D10UL)
#define ISP_NLM_MODE_CNT		(0x1D14UL)
#define	ISP_NLM_SIMPLE_BPC		(0x1D18UL)
#define ISP_NLM_LUM_TH			(0x1D1CUL)
#define ISP_NLM_DIRECTION_TH		(0x1D20UL)
#define ISP_NLM_IS_FLAT			(0x1D24UL)
#define ISP_NLM_LUT_W_0			(0x1D28UL)
#define ISP_NLM_LUM0_FLAT0_PARAM	(0x1D88UL)
#define ISP_NLM_LUM0_FLAT0_ADDBACK	(0x1D8CUL)
#define ISP_NLM_LUM0_FLAT1_PARAM	(0x1D90UL)
#define ISP_NLM_LUM0_FLAT1_ADDBACK	(0x1D94UL)
#define ISP_NLM_LUM0_FLAT2_PARAM	(0x1D98UL)
#define ISP_NLM_LUM0_FLAT2_ADDBACK	(0x1D9CUL)
#define ISP_NLM_LUM0_FLAT3_PARAM	(0x1DA0UL)
#define ISP_NLM_LUM0_FLAT3_ADDBACK	(0x1DA4UL)
#define ISP_NLM_ADDBACK3		(0x1DE8UL)
#define ISP_REG_VST_ADDR0		(0x19000UL) /*Not listed in spec*/
#define ISP_REG_IVST_ADDR0		(0x1A000UL) /*Not listed in spec*/
#define ISP_REG_VST_ADDR1		(0x22000UL) /*Not listed in spec*/
#define ISP_REG_IVST_ADDR1		(0x23000UL) /*Not listed in spec*/

/*isp sub block:CMC: Color matrix correction for 10	bit*/
#define ISP_CMC10_STATUS0		(0x3100UL)
#define ISP_CMC10_STATUS1		(0x3104UL)
#define ISP_CMC10_PARAM			(0x3110UL)
#define ISP_CMC10_MATRIX0		(0x3114UL)


/*isp sub block: CFA NEW: clolor filter	array*/
#define ISP_CFAE_STATUS				(0x3000UL)
#define ISP_CFAE_NEW_CFG0			(0x3010UL)
#define ISP_CFAE_INTP_CFG0			(0x3014UL)
#define ISP_CFAE_INTP_CFG1			(0x3018UL)
#define ISP_CFAE_INTP_CFG2			(0x301CUL)
#define ISP_CFAE_INTP_CFG3			(0x3020UL)
#define ISP_CFAE_INTP_CFG4			(0x3024UL)
#define ISP_CFAE_INTP_CFG5			(0x3028UL)
#define ISP_CFAE_CSS_CFG0			(0x302CUL)
#define ISP_CFAE_CSS_CFG1			(0x3030UL)
#define ISP_CFAE_CSS_CFG2			(0x3034UL)
#define ISP_CFAE_CSS_CFG3			(0x3038UL)
#define ISP_CFAE_CSS_CFG4			(0x303CUL)
#define ISP_CFAE_CSS_CFG5			(0x3040UL)
#define ISP_CFAE_CSS_CFG6			(0x3044UL)
#define ISP_CFAE_CSS_CFG7			(0x3048UL)
#define ISP_CFAE_CSS_CFG8			(0x304CUL)
#define ISP_CFAE_CSS_CFG9			(0x3050UL)
#define ISP_CFAE_CSS_CFG10			(0x3054UL)
#define ISP_CFAE_CSS_CFG11			(0x3058UL)
#define ISP_CFAE_GBUF_CFG			(0x305CUL)

/*isp sub block: FRGB GAMMA */
#define ISP_GAMMA_STATUS			(0x3200UL)
#define ISP_GAMMA_PARAM				(0x3210UL)

/*isp sub block: CCE: clolor conversion	enhancement*/
#define ISP_CCE_STATUS0				(0x3500UL)
#define ISP_CCE_STATUS1				(0x3504UL)
#define ISP_CCE_PARAM				(0x3510UL)
#define ISP_CCE_MATRIX0				(0x3514UL)
#define ISP_CCE_MATRIX1				(0x3518UL)
#define ISP_CCE_MATRIX2				(0x351CUL)
#define ISP_CCE_MATRIX3				(0x3520UL)
#define ISP_CCE_MATRIX4				(0x3524UL)
#define ISP_CCE_SHIFT				(0x3528UL)

/*isp sub block: UVD: UV division*/
#define ISP_UVD_STATUS0				(0x3600UL)
#define ISP_UVD_STATUS1				(0x3604UL)
#define ISP_UVD_PARAM				(0x3610UL)
#define ISP_UVD_PARAM0				(0x3614UL)
#define ISP_UVD_PARAM1				(0x3618UL)
#define ISP_UVD_PARAM2				(0x361CUL)
#define ISP_UVD_PARAM3				(0x3620UL)
#define ISP_UVD_PARAM4				(0x3624UL)
#define ISP_UVD_PARAM5				(0x3628UL)

/*isp sub block:HSV*/
#define ISP_HSV_STATUS				(0x3300UL)
#define ISP_HSV_PARAM				(0x3310UL)
#define ISP_HSV_CFG0				(0x3314UL)
#define ISP_HSV_CFG1				(0x3318UL)
#define ISP_HSV_CFG2				(0x331CUL)
#define ISP_HSV_CFG3				(0x3320UL)
#define ISP_HSV_CFG4				(0x3324UL)
#define ISP_HSV_CFG5				(0x3328UL)
#define ISP_HSV_CFG6				(0x332CUL)
#define ISP_HSV_CFG7				(0x3330UL)
#define ISP_HSV_CFG8				(0x3334UL)
#define ISP_HSV_CFG9				(0x3338UL)
#define ISP_HSV_CFG10				(0x333CUL)
#define ISP_HSV_CFG11				(0x3340UL)
#define ISP_HSV_CFG12				(0x3344UL)
#define ISP_HSV_CFG13				(0x3348UL)
#define ISP_HSV_CFG14				(0x334CUL)

/*isp sub block:ISP_PSTRZ*/
#define ISP_PSTRZ_STATUS			(0x3400UL)
#define ISP_PSTRZ_PARAM				(0x3410UL)
#define ISP_PSTRZ_LEVEL0			(0x3414UL)
#define ISP_PSTRZ_LEVEL1			(0x3418UL)
#define ISP_PSTRZ_LEVEL2			(0x341CUL)
#define ISP_PSTRZ_LEVEL3			(0x3420UL)
#define ISP_PSTRZ_LEVEL4			(0x3424UL)
#define ISP_PSTRZ_LEVEL5			(0x3428UL)
#define ISP_PSTRZ_LEVEL6			(0x342CUL)
#define ISP_PSTRZ_LEVEL7			(0x3430UL)

/*isp sub block: AFM : auto	focus monitor*/
#define ISP_RGB_AFM_STATUS0				(0x3800UL)
#define ISP_RGB_AFM_STATUS1				(0x3804UL)
#define ISP_RGB_AFM_PARAM				(0x3810UL)
#define ISP_RGB_AFM_FRAME_RANGE			(0x3814UL)
#define ISP_RGB_AFM_WIN_RANGE0S			(0x3818UL)
#define ISP_RGB_AFM_WIN_RANGE0E			(0x381CUL)
#define ISP_RGB_AFM_WIN_RANGE1S			(0x3820UL) /*SharkLe new*/
#define ISP_RGB_AFM_WIN_RANGE1E			(0x3824UL) /*SharkLe new*/
#define ISP_RGB_AFM_WIN_RANGE2S			(0x3828UL) /*SharkLe new*/
#define ISP_RGB_AFM_WIN_RANGE2E			(0x382CUL) /*SharkLe new*/
#define ISP_RGB_AFM_WIN_RANGE3S			(0x3830UL) /*SharkLe new*/
#define ISP_RGB_AFM_WIN_RANGE3E			(0x3834UL) /*SharkLe new*/
#define ISP_RGB_AFM_WIN_RANGE4S			(0x3838UL) /*SharkLe new*/
#define ISP_RGB_AFM_WIN_RANGE4E			(0x383CUL) /*SharkLe new*/
#define ISP_RGB_AFM_WIN_RANGE5S			(0x3840UL) /*SharkLe new*/
#define ISP_RGB_AFM_WIN_RANGE5E			(0x3844UL) /*SharkLe new*/
#define ISP_RGB_AFM_WIN_RANGE6S			(0x3848UL) /*SharkLe new*/
#define ISP_RGB_AFM_WIN_RANGE6E			(0x384CUL) /*SharkLe new*/
#define ISP_RGB_AFM_WIN_RANGE7S			(0x3850UL) /*SharkLe new*/
#define ISP_RGB_AFM_WIN_RANGE7E			(0x3854UL) /*SharkLe new*/
#define ISP_RGB_AFM_WIN_RANGE8S			(0x3858UL) /*SharkLe new*/
#define ISP_RGB_AFM_WIN_RANGE8E			(0x385CUL) /*SharkLe new*/
#define ISP_RGB_AFM_WIN_RANGE9S			(0x3860UL) /*SharkLe new*/
#define ISP_RGB_AFM_WIN_RANGE9E			(0x3864UL) /*SharkLe new*/
#define ISP_RGB_AFM_IIR_FILTER0			(0x3868UL)
#define ISP_RGB_AFM_IIR_FILTER1			(0x386CUL)
#define ISP_RGB_AFM_IIR_FILTER2			(0x3870UL)
#define ISP_RGB_AFM_IIR_FILTER3			(0x3874UL)
#define ISP_RGB_AFM_IIR_FILTER4			(0x3878UL)
#define ISP_RGB_AFM_IIR_FILTER5			(0x387CUL)
#define ISP_RGB_AFM_ENHANCE_CTRL		(0x3880UL)
#define ISP_RGB_AFM_ENHANCE_FV0_THD		(0x3884UL)
#define ISP_RGB_AFM_ENHANCE_FV1_THD		(0x3888UL)
#define ISP_RGB_AFM_ENHANCE_FV1_COEFF00	(0x388CUL)
#define ISP_RGB_AFM_ENHANCE_FV1_COEFF01	(0x3890UL)
#define ISP_RGB_AFM_ENHANCE_FV1_COEFF10	(0x3894UL)
#define ISP_RGB_AFM_ENHANCE_FV1_COEFF11	(0x3898UL)
#define ISP_RGB_AFM_ENHANCE_FV1_COEFF20	(0x389CUL)
#define ISP_RGB_AFM_ENHANCE_FV1_COEFF21	(0x38A0UL)
#define ISP_RGB_AFM_ENHANCE_FV1_COEFF30	(0x38A4UL)
#define ISP_RGB_AFM_ENHANCE_FV1_COEFF31	(0x38A8UL)
#define ISP_RGB_AFM_ENHANCE_FV0_0_BUF0_LOW		(0x38ACUL)
#define ISP_RGB_AFM_ENHANCE_FV1_0_BUF0_LOW		(0x38D4UL)
#define ISP_RGB_AFM_ENHANCE_FV0_FV1_0_BUF0_HIGH		(0x38FCUL)
#define ISP_RGB_AFM_ENHANCE_FV0_0_BUF1_LOW		(0x3924UL)
#define ISP_RGB_AFM_CFG_READY				   (0x399CUL)
#define ISP_RGB_AFM_SKIP_NUM_CLR			   (0x39A0UL)

/*isp sub block:ANTI FLICKER NEW*/
#define ISP_ANTI_FLICKER_NEW_STATUS0		  (0x4600UL)
#define ISP_ANTI_FLICKER_NEW_STATUS1		  (0x4604UL)
#define ISP_ANTI_FLICKER_NEW_STATUS2		  (0x4608UL)
#define ISP_ANTI_FLICKER_NEW_STATUS3		  (0x460CUL)
#define ISP_ANTI_FLICKER_NEW_PARAM0			(0x4610UL)
#define ISP_ANTI_FLICKER_NEW_PARAM1			(0x4614UL)
#define ISP_ANTI_FLICKER_NEW_PARAM2			(0x4618UL)
#define ISP_ANTI_FLICKER_NEW_COL_POS		(0x461CUL)
#define ISP_ANTI_FLICKER_NEW_DDR_INIT_ADDR	(0x4620UL)
#define ISP_ANTI_FLICKER_NEW_REGION0		(0x4624UL)
#define ISP_ANTI_FLICKER_NEW_REGION1		(0x4628UL)
#define ISP_ANTI_FLICKER_NEW_REGION2		(0x462CUL)
#define ISP_ANTI_FLICKER_NEW_REGION3		(0x4630UL)
#define ISP_ANTI_FLICKER_NEW_SUM1			(0x4634UL)
#define ISP_ANTI_FLICKER_NEW_SUM2			(0x4638UL)
#define ISP_ANTI_FLICKER_NEW_CFG_READY		(0x463CUL)
#define ISP_ANTI_FLICKER_NEW_SKIP_NUM_CLR	(0x4640UL)

/*isp sub block: Pre-CDN*/
#define ISP_PRECDN_STATUS0				   (0x5000UL)
#define ISP_PRECDN_STATUS1				   (0x5004UL)
#define ISP_PRECDN_PARAM				   (0x5010UL)
#define ISP_PRECDN_MEDIAN_THRUV01		   (0x5014UL)
#define ISP_PRECDN_THRYUV				   (0x5018UL)
#define ISP_PRECDN_SEGU_0				   (0x501CUL)
#define ISP_PRECDN_SEGU_1				   (0x5020UL)
#define ISP_PRECDN_SEGU_2				   (0x5024UL)
#define ISP_PRECDN_SEGU_3				   (0x5028UL)
#define ISP_PRECDN_SEGV_0				   (0x502CUL)
#define ISP_PRECDN_SEGV_1				   (0x5030UL)
#define ISP_PRECDN_SEGV_2				   (0x5034UL)
#define ISP_PRECDN_SEGV_3				   (0x5038UL)
#define ISP_PRECDN_SEGY_0				   (0x503CUL)
#define ISP_PRECDN_SEGY_1				   (0x5040UL)
#define ISP_PRECDN_SEGY_2				   (0x5044UL)
#define ISP_PRECDN_SEGY_3				   (0x5048UL)
#define ISP_PRECDN_DISTW0				   (0x504CUL)
#define ISP_PRECDN_DISTW1				   (0x5050UL)
#define ISP_PRECDN_DISTW2				   (0x5054UL)
#define ISP_PRECDN_DISTW3				   (0x5058UL)

/*isp sub block: YNR*/
#define ISP_YNR_STATUS0					(0x5100UL)
#define ISP_YNR_STATUS1					(0x5104UL)
#define ISP_YNR_CTRL0					(0x5110UL)
#define ISP_YNR_CFG0					(0x5114UL)
#define ISP_YNR_CFG1					(0x5118UL)
#define ISP_YNR_CFG2					(0x511CUL)
#define ISP_YNR_CFG3					(0x5120UL)
#define ISP_YNR_CFG4					(0x5124UL)
#define ISP_YNR_CFG5					(0x5128UL)
#define ISP_YNR_CFG6					(0x512CUL)
#define ISP_YNR_CFG7					(0x5130UL)
#define ISP_YNR_CFG8					(0x5134UL)
#define ISP_YNR_CFG9					(0x5138UL)
#define ISP_YNR_CFG10					(0x513CUL)
#define ISP_YNR_WLT0					(0x5140UL)
#define ISP_YNR_WLT1					(0x5144UL)
#define ISP_YNR_WLT2					(0x5148UL)
#define ISP_YNR_WLT3					(0x514CUL)
#define ISP_YNR_WLT4					(0x5150UL)
#define ISP_YNR_WLT5					(0x5154UL)
#define ISP_YNR_FRATIO0					(0x5158UL)
#define ISP_YNR_FRATIO1					(0x515CUL)
#define ISP_YNR_FRATIO2					(0x5160UL)
#define ISP_YNR_FRATIO3					(0x5164UL)
#define ISP_YNR_FRATIO4					(0x5168UL)
#define ISP_YNR_FRATIO5					(0x516CUL)
#define ISP_YNR_FRATIO6					(0x5170UL)
#define ISP_YNR_FRATIO7					(0x5174UL)
#define ISP_YNR_CFG11					(0x5178UL)
#define ISP_YNR_CFG12					(0x517CUL)
#define ISP_YNR_CFG13					(0x5180UL)
#define ISP_YNR_CFG14					(0x5184UL)
#define ISP_YNR_CFG15					(0x5188UL)
#define ISP_YNR_CFG16					(0x518CUL)
#define ISP_YNR_CFG17					(0x5190UL)

/*isp sub block: Brightness*/
#define ISP_BRIGHT_STATUS				(0x5200UL)
#define ISP_BRIGHT_PARAM				(0x5210UL)

/*isp sub block: Contrast*/
#define ISP_CONTRAST_STATUS				(0x5300UL)
#define ISP_CONTRAST_PARAM				(0x5310UL)

/*isp sub block: HIST :	histogram*/
#define ISP_HIST_STATUS					(0x5400UL)
#define ISP_HIST_PARAM					(0x5410UL)
#define ISP_HIST_CFG_READY				(0x5414UL)
#define ISP_HIST_SKIP_NUM_CLR			(0x5418UL)

/*isp sub block: HIST2*/
#define ISP_HIST2_STATUS				(0x5500UL)
#define ISP_HIST2_PARAM					(0x5510UL)
#define ISP_HIST2_ROI_S0				(0x5514UL)
#define ISP_HIST2_ROI_E0				(0x5518UL)
#define ISP_HIST2_CFG_RDY				(0x551CUL)
#define ISP_HIST2_SKIP_CLR			(0x5520UL)

/*isp sub block: cdn*/
#define ISP_CDN_STATUS0			(0x5600UL)
#define ISP_CDN_STATUS1			(0x5604UL)
#define ISP_CDN_PARAM			(0x5610UL)
#define ISP_CDN_THRUV			(0x5614UL)
#define ISP_CDN_U_RANWEI_0		(0x5618UL) /* RANWEI_0 ~ RANWEI_7 */
#define ISP_CDN_U_RANWEI_7		(0x5634UL)
#define ISP_CDN_V_RANWEI_0		(0x5638UL)
#define ISP_CDN_V_RANWEI_7		(0x5654UL)

/*isp sub block: edge*/
#define ISP_EE_STATUS				(0x5700UL)
#define ISP_EE_PARAM				(0x5710UL)
#define ISP_EE_CFG0					(0x5714UL)
#define ISP_EE_CFG1					(0x5718UL)
#define ISP_EE_CFG2					(0x571CUL)
#define ISP_EE_ADP_CFG0				(0x5720UL)
#define ISP_EE_ADP_CFG1				(0x5724UL)
#define ISP_EE_ADP_CFG2				(0x5728UL)
#define ISP_EE_IPD_CFG0				(0x572CUL)
#define ISP_EE_IPD_CFG1				(0x5730UL)
#define ISP_EE_IPD_CFG2				(0x5734UL)
#define ISP_EE_LUM_CFG0				(0x5738UL)
#define ISP_EE_LUM_CFG1				(0x573CUL)
#define ISP_EE_LUM_CFG2				(0x5740UL)
#define ISP_EE_LUM_CFG3				(0x5744UL)
#define ISP_EE_LUM_CFG4				(0x5748UL)
#define ISP_EE_LUM_CFG5				(0x574CUL)
#define ISP_EE_LUM_CFG6				(0x5750UL)
#define ISP_EE_LUM_CFG7				(0x5754UL)
#define ISP_EE_LUM_CFG8				(0x5758UL)
#define ISP_EE_LUM_CFG9				(0x575CUL)
#define ISP_EE_LUM_CFG10			(0x5760UL)
#define ISP_EE_LUM_CFG11			(0x5764UL)
#define ISP_EE_LUM_CFG12			(0x5768UL)
#define ISP_EE_LUM_CFG13			(0x576CUL)
#define ISP_EE_LUM_CFG14			(0x5770UL)
#define ISP_EE_LUM_CFG15			(0x5774UL)
#define ISP_EE_LUM_CFG16			(0x5778UL)
#define ISP_EE_LUM_CFG17			(0x577CUL)
#define ISP_EE_LUM_CFG18			(0x5780UL)

/*isp sub block: csa*/
#define ISP_CSA_STATUS				(0x5800UL)
#define ISP_CSA_PARAM				(0x5810UL)

/*isp sub block: hua*/
#define ISP_HUA_STATUS				(0x5900UL)
#define ISP_HUA_PARAM				(0x5910UL)

/*isp sub block: post-cdn*/
#define ISP_POSTCDN_STATUS				(0x5A00UL)
#define ISP_POSTCDN_COMMON_CTRL			(0x5A10UL)
#define ISP_POSTCDN_ADPT_THR			(0x5A14UL)
#define ISP_POSTCDN_UVTHR				(0x5A18UL)
#define ISP_POSTCDN_THRU				(0x5A1CUL)
#define ISP_POSTCDN_THRV				(0x5A20UL)
#define ISP_POSTCDN_RSEGU01				(0x5A24UL)
#define ISP_POSTCDN_RSEGU23				(0x5A28UL)
#define ISP_POSTCDN_RSEGU45				(0x5A2CUL)
#define ISP_POSTCDN_RSEGU6				(0x5A30UL)
#define ISP_POSTCDN_RSEGV01				(0x5A34UL)
#define ISP_POSTCDN_RSEGV23				(0x5A38UL)
#define ISP_POSTCDN_RSEGV45				(0x5A3CUL)
#define ISP_POSTCDN_RSEGV6				(0x5A40UL)
#define ISP_POSTCDN_R_DISTW0			(0x5A44UL) /*DISTW0~DISTW14*/
#define ISP_POSTCDN_START_ROW_MOD4		(0x5A80UL) /*SLICE_CTRL*/

/*isp sub block: ygamma*/
#define ISP_YGAMMA_STATUS				(0x5B00UL)
#define ISP_YGAMMA_PARAM				(0x5B10UL)

/*isp sub block: ydelay-- also means uvdelay*/
#define ISP_YDELAY_STATUS				(0x5C00UL)
#define ISP_YDELAY_PARAM				(0x5C10UL)
#define ISP_YDELAY_STEP				    (0x5C14UL) /*SharkLe new*/

/*isp sub block: iircnr*/
#define ISP_IIRCNR_STATUS0				(0x5D00UL)
#define ISP_IIRCNR_STATUS1				(0x5D04UL)
#define ISP_IIRCNR_PARAM				(0x5D10UL)
#define ISP_IIRCNR_PARAM1				(0x5D14UL)
#define ISP_IIRCNR_PARAM2				(0x5D18UL)
#define ISP_IIRCNR_PARAM3				(0x5D1CUL)
#define ISP_IIRCNR_PARAM4				(0x5D20UL)
#define ISP_IIRCNR_PARAM5				(0x5D24UL)
#define ISP_IIRCNR_PARAM6				(0x5D28UL)
#define ISP_IIRCNR_PARAM7				(0x5D2CUL)
#define ISP_IIRCNR_PARAM8				(0x5D30UL)
#define ISP_IIRCNR_PARAM9				(0x5D34UL)
#define ISP_YUV_IIRCNR_NEW_0					(0x5D38UL)
#define ISP_YUV_IIRCNR_NEW_1					(0x5D3CUL)
#define ISP_YUV_IIRCNR_NEW_2					(0x5D40UL)
#define ISP_YUV_IIRCNR_NEW_3					(0x5D44UL)
#define ISP_YUV_IIRCNR_NEW_4					(0x5D48UL)
#define ISP_YUV_IIRCNR_NEW_5					(0x5D4CUL)
#define ISP_YUV_IIRCNR_NEW_6					(0x5D50UL)
#define ISP_YUV_IIRCNR_NEW_7					(0x5D54UL)
#define ISP_YUV_IIRCNR_NEW_8					(0x5D58UL)
#define ISP_YUV_IIRCNR_NEW_9					(0x5D5CUL)
#define ISP_YUV_IIRCNR_NEW_10				(0x5D60UL)
#define ISP_YUV_IIRCNR_NEW_11				(0x5D64UL)
#define ISP_YUV_IIRCNR_NEW_12				(0x5D68UL)
#define ISP_YUV_IIRCNR_NEW_13				(0x5D6CUL)
#define ISP_YUV_IIRCNR_NEW_14				(0x5D70UL)
#define ISP_YUV_IIRCNR_NEW_15				(0x5D74UL)
#define ISP_YUV_IIRCNR_NEW_16				(0x5D78UL)
#define ISP_YUV_IIRCNR_NEW_17				(0x5D7CUL)
#define ISP_YUV_IIRCNR_NEW_18				(0x5D80UL)
#define ISP_YUV_IIRCNR_NEW_19				(0x5D84UL)
#define ISP_YUV_IIRCNR_NEW_20				(0x5D88UL)
#define ISP_YUV_IIRCNR_NEW_21				(0x5D8CUL)
#define ISP_YUV_IIRCNR_NEW_22				(0x5D90UL)
#define ISP_YUV_IIRCNR_NEW_23				(0x5D94UL)
#define ISP_YUV_IIRCNR_NEW_24				(0x5D98UL)
#define ISP_YUV_IIRCNR_NEW_25				(0x5D9CUL)
#define ISP_YUV_IIRCNR_NEW_26				(0x5DA0UL)
#define ISP_YUV_IIRCNR_NEW_27				(0x5DA4UL)
#define ISP_YUV_IIRCNR_NEW_28				(0x5DA8UL)
#define ISP_YUV_IIRCNR_NEW_29				(0x5DACUL)
#define ISP_YUV_IIRCNR_NEW_30				(0x5DB0UL)
#define ISP_YUV_IIRCNR_NEW_31				(0x5DB4UL)
#define ISP_YUV_IIRCNR_NEW_32				(0x5DB8UL)
#define ISP_YUV_IIRCNR_NEW_33				(0x5DBCUL)
#define ISP_YUV_IIRCNR_NEW_34				(0x5DC0UL)
#define ISP_YUV_IIRCNR_NEW_35				(0x5DC4UL)
#define ISP_YUV_IIRCNR_NEW_36				(0x5DC8UL)
#define ISP_YUV_IIRCNR_NEW_37				(0x5DCCUL)
#define ISP_YUV_IIRCNR_NEW_38				(0x5DD0UL)
#define ISP_YUV_IIRCNR_NEW_39				(0x5DD4UL)

/*isp sub block: YRANDOM */
#define ISP_YRANDOM_STATUS0				(0x5E00UL)
#define ISP_YRANDOM_CHKSUM				(0x5E04UL)
#define ISP_YRANDOM_PARAM1				(0x5E10UL)
#define ISP_YRANDOM_PARAM2				(0x5E14UL)
#define ISP_YRANDOM_PARAM3				(0x5E18UL)
#define ISP_YRANDOM_STATUS1				(0x5E1CUL)
#define ISP_YRANDOM_STATUS2				(0x5E20UL)
#define ISP_YRANDOM_INIT				(0x5E24UL)

/*isp sub block: NOISE FILTER*/
#define ISP_YUV_NF_STATUS0				(0xC200UL)
#define ISP_YUV_NF_CTRL					(0xC210UL)
#define ISP_YUV_NF_SEED0				(0xC214UL)
#define ISP_YUV_NF_SEED1				(0xC218UL)
#define ISP_YUV_NF_SEED2				(0xC21CUL)
#define ISP_YUV_NF_SEED3				(0xC220UL)
#define ISP_YUV_NF_TB4					(0xC224UL)
#define ISP_YUV_NF_SF					(0xC228UL)
#define ISP_YUV_NF_THR					(0xC22CUL)
#define ISP_YUV_NF_CV_T12				(0xC230UL)
#define ISP_YUV_NF_CV_R					(0xC234UL)
#define ISP_YUV_NF_CLIP					(0xC238UL)
#define ISP_YUV_NF_SEED0_OUT				(0xC23CUL)
#define ISP_YUV_NF_SEED_INIT				(0xC24CUL)
#define ISP_YUV_NF_CV_T34				(0xC250UL)

/* pingpang buffer
 *	0x60a12000~0x60a13fff		ISP_AEM_CH0
 *	0x60a15000~0x60a153ff	ISP_HIST_CH0
 *	0x60a16000~0x60a160ff	ISP_HIST2_BUF0_CH0
 *	0x60a18000~0x60a185a3	ISP_HSV_BUF0_CH0
 *	0x60a19000~0x60a19fff		ISP_VST_BUF0_CH0
 *	0x60a1a000~0x60a1afff		ISP_IVST_BUF0_CH0
 *	0x60a1b000~0x60a1b203	ISP_FGAMMA_R_BUF0_CH0
 *	0x60a1c000~0x60a1c203	ISP_FGAMMA_G_BUF0_CH0
 *	0x60a1d000~0x60a1d203	ISP_FGAMMA_B_BUF0_CH0
 *	0x60a1e000~0x60a1e203	ISP_YGAMMA_BUF0_CH0
 *	0x60a21000~0x60a215a3	ISP_HSV_BUF1_CH0
 *	0x60a22000~0x60a22fff		ISP_VST_BUF1_CH0
 *	0x60a23000~0x60a23fff		ISP_IVST_BUF1_CH0
 *	0x60a24000~0x60a24203	ISP_FGAMMA_R_BUF1_CH0
 *	0x60a25000~0x60a25203	ISP_FGAMMA_G_BUF1_CH0
 *	0x60a26000~0x60a26203	ISP_FGAMMA_B_BUF1_CH0
 *	0x60a27000~0x60a27203	ISP_YGAMMA_BUF1_CH0
 *	0x60a29000~0x60a2b2ff	ISP_LENS_BUF0_CH0
 *	0x60a2c000~0x60a2e2ff	ISP_LENS_BUF1_CH0
 *	0x60a30000~0x60a303ff	ISP_1D_LENS_GR_BUF0_CH0
 *	0x60a30400~0x60a307ff	ISP_1D_LENS_R_BUF0_CH0
 *	0x60a30800~0x60a30bff	ISP_1D_LENS_B_BUF0_CH0
 *	0x60a30c00~0x60a30fff		ISP_1D_LENS_GB_BUF0_CH0
 *	0x60a31000~0x60a313ff	ISP_1D_LENS_GR_BUF1_CH0
 *	0x60a31400~0x60a317ff	ISP_1D_LENS_R_BUF1_CH0
 *	0x60a31800~0x60a31bff	ISP_1D_LENS_B_BUF1_CH0
 *	0x60a31c00~0x60a31fff		ISP_1D_LENS_GB_BUF1_CH0
 *	0x60a36400~0x60a36bff	ISP_LENS_WEIGHT_BUF
 *	0x60a36c00~0x60a36dff	ISP_CFG0_BUF
 *	0x60a36e00~0x60a36fff		ISP_CFG1_BUF
 *	0x60a37000~0x60a3740f	ISP_LENS_WEIGHT_BUF1
 */
#define ISP_AEM_CH0				(0x12000UL)
#define ISP_HIST_CH0				(0x15000UL)
#define ISP_HIST2_BUF0_CH0				(0x16000UL)
#define ISP_HIST2_ROI0_CH0			(0x16000UL)
#define ISP_HIST2_ROI1_CH0			(0x16100UL)
#define ISP_HIST2_ROI2_CH0			(0x16200UL)
#define ISP_HIST2_ROI3_CH0			(0x16300UL)
#define ISP_HSV_BUF0_CH0				(0x18000UL)
#define ISP_VST_BUF0_CH0				(0x19000UL)
#define ISP_IVST_BUF0_CH0				(0x1A000UL)
#define ISP_FGAMMA_R_BUF0_CH0				(0x1B000UL)
#define ISP_FGAMMA_G_BUF0_CH0				(0x1C000UL)
#define ISP_FGAMMA_B_BUF0_CH0				(0x1D000UL)
#define ISP_YGAMMA_BUF0_CH0				(0x1E000UL)
#define ISP_HSV_BUF1_CH0				(0x21000UL)
#define ISP_VST_BUF1_CH0				(0x22000UL)
#define ISP_IVST_BUF1_CH0				(0x23000UL)
#define ISP_FGAMMA_R_BUF1_CH0				(0x24000UL)
#define ISP_FGAMMA_G_BUF1_CH0				(0x25000UL)
#define ISP_FGAMMA_B_BUF1_CH0				(0x26000UL)
#define ISP_YGAMMA_BUF1_CH0				(0x27000UL)
#define ISP_LEN_BUF0_CH0				(0x29000UL)
#define ISP_LEN_BUF1_CH0				(0x2c000UL)
#define ISP_1D_LENS_GR_BUF0_CH0			(0x30000UL)
#define ISP_1D_LENS_R_BUF0_CH0			(0x30400UL)
#define ISP_1D_LENS_B_BUF0_CH0				(0x30800UL)
#define ISP_1D_LENS_GB_BUF0_CH0			(0x30c00UL)
#define ISP_1D_LENS_GR_BUF1_CH0			(0x31000UL)
#define ISP_1D_LENS_R_BUF1_CH0			(0x31400UL)
#define ISP_1D_LENS_B_BUF1_CH0				(0x31800UL)
#define ISP_1D_LENS_GB_BUF1_CH0			(0x31c00UL)
#define ISP_LENS_WEIGHT_BUF0				(0x36400UL)
#define ISP_CFG0_BUF					(0x36c00UL)
#define ISP_CFG1_BUF					(0x36e00UL)
#define ISP_LENS_WEIGHT_BUF1				(0x37000UL)


/* default regsiter content */
extern int ISP_REG_DEFAULT[ISP_REG_DEF];

/* register address handling */
extern unsigned long s_isp_regbase[ISP_MAX_COUNT];
extern unsigned long isp_phys_base[ISP_MAX_COUNT];
extern unsigned long isp_cctx_cmdbuf_addr[ISP_MAX_COUNT][2];
extern unsigned long *isp_addr_poll[2][4];
extern void *g_isp_dev[ISP_MAX_COUNT];

#define ISP_PHYS_ADDR(iid)	(isp_phys_base[iid])
#define ISP_GET_REG(iid, reg)	(ISP_PHYS_ADDR(iid) + reg)

/*
 * turn on/off isp_cfg mode
 * 1: cfg mode on
 * 0: cfg mode off
 */
#define ON_ISP_CFG_MODE 1

/*
 * Divide isp_pipe_dev->id into 3 parts:
 * @scene_id, pre(0) or cap(1)
 * @mode_id, 0: cfg mode, 1:ap mode,
 * @isp_id, isp instance id
 * each part has 4bits.
 *
 * Use ISP_GET_xID(idx) to get
 * specified id from idx(id extend).
 * Use ISP_SET_xID(idx, id) to set id to
 * idx.
 *
 * MSB                               LSB
 * |----4-----|----4------|-----4------|
 * | scene id | mode_id   |   isp id   |
 *
 * scope of the three id:
 * scene id: 0 / 1
 * mode_id: 0 / 1
 * isp_id: 0 / 1
 */

/*
 * According to the scope of ids defined above,
 * check the id's validatity.
 */
#define BAD_ID 0xbad
#define CHECK_ID_VALID(id)	 ((id) == 0 || (id) == 1)
#define CHECK_IDX_VALID(id)	 ((id) != BAD_ID)

#define ID_MASK 0x1
#define SID_POS 8 /* position of isp Scene ID in com_idx */
#define MID_POS 4 /* position of isp work Mode ID in com_idx */
#define IID_POS 0 /* position of isp Instance ID in com_idx*/

/*
 * It is recommended to use CHECK_VALID after calling the
 * 3 macros below, in order to confirm the validatity of ids.
 */
#define ISP_GET_IID(idx)	(((idx) >> IID_POS) & ID_MASK)
#define ISP_GET_MID(idx)					\
	((ON_ISP_CFG_MODE == 1) ?				\
	 (((idx) >> MID_POS) & ID_MASK) : ISP_AP_MODE)

#define ISP_GET_SID(idx)	(((idx) >> SID_POS) & ID_MASK)

#define ISP_SET_IID(idx, id)					\
do {								\
	if (CHECK_ID_VALID(id))					\
		idx = ((((id) & ID_MASK) << IID_POS) |		\
			(idx & ~(ID_MASK << IID_POS)));		\
	else							\
		pr_info("ignore invalid iid 0x%x\n", id);	\
} while (0)

#define ISP_SET_MID(idx, id)					\
do {								\
	if (CHECK_ID_VALID(id))					\
		idx = ((((id) & ID_MASK) << MID_POS) |		\
			(idx & ~(ID_MASK << MID_POS)));		\
	else							\
		pr_info("ignore invalid mid 0x%x\n", id);	\
} while (0)

#define ISP_SET_SID(idx, id)					\
do {								\
	if (CHECK_ID_VALID(id))					\
		idx = ((((id) & ID_MASK) << SID_POS) |		\
			(idx & ~(ID_MASK << SID_POS)));		\
	else							\
		pr_info("ignore invalid sid 0x%x\n", id);	\
} while (0)

#define ISP_SAVE_SID(idx, flag)	({flag = ISP_GET_SID(idx); })

#define ISP_RESTORE_SID(idx, flag) ISP_SET_SID(idx, flag)


/* Composite idx according to the rules above */
#define ISP_COM_IDX(idx, scene_id, work_mode_id, isp_id)	\
do {								\
	if (CHECK_ID_VALID(scene_id) &&				\
	    CHECK_ID_VALID(work_mode_id) &&			\
	    CHECK_ID_VALID(isp_id))				\
		idx = ((isp_id) |				\
		       ((work_mode_id) << 4) |			\
		       ((scene_id) << 8));			\
	else {							\
		idx = BAD_ID;					\
		pr_err("invalid idx, set com_idx to 0x%x\n",	\
		       BAD_ID);					\
	}							\
} while (0)


#define ISP_GET_CFG_ID(scene_id, isp_id) ((scene_id) << 1 | (isp_id))

/* set fmcu owner with cfg_id */
#define ISP_SET_FMCU_OWNER(dev)						\
	({(dev)->fmcu_owner = ISP_GET_CFG_ID(ISP_GET_SID(dev->com_idx),	\
					     ISP_GET_IID(dev->com_idx)); })

#define ISP_BASE_ADDR(idx) \
	(*(isp_addr_poll[ISP_GET_MID(idx)][(ISP_GET_IID(idx) << 1) | \
						ISP_GET_SID(idx)]))

#define ISP_REG_WR(idx, reg, val)					\
do {									\
	if (!IS_ERR_OR_NULL((void *)ISP_BASE_ADDR(idx)))		\
		REG_WR(ISP_BASE_ADDR(idx) + (reg), (val));		\
	else								\
		WARN_ON(1);						\
} while (0)

#define ISP_PAGE_REG_RD(idx, reg)  (REG_RD(ISP_BASE_ADDR((idx)) + (reg)))
#define ISP_REG_MWR(idx, reg, msk, val)  \
		ISP_REG_WR((idx), (reg), \
			    (((val) & (msk)) | \
			    (ISP_PAGE_REG_RD((idx), (reg)) & (~(msk)))))
#define ISP_REG_OWR(idx, reg, val)  \
		ISP_REG_WR((idx), (reg), \
			    (ISP_PAGE_REG_RD((idx), (reg)) | (val)))

/*
 * The isp modules below won't access CFG buffers,
 * using ISP_HREG_xxx to config HW registers directly.
 *
 *	isp axi,
 *	isp arbiter,
 *	isp common,
 *	isp CFG,
 *	isp FMCU,
 *	isp mmu,
 *	isp core,
 *	isp int (0x60a0000, 0d00, 0e00, 0f00)
 */
#define ISP_HREG_WR(idx, reg, val) \
		REG_WR(s_isp_regbase[ISP_GET_IID(idx)] + (reg), (val))

#define ISP_HREG_RD(idx, reg) \
		REG_RD(s_isp_regbase[ISP_GET_IID(idx)] + (reg))

#define ISP_HREG_MWR(idx, reg, msk, val) \
		REG_WR(s_isp_regbase[ISP_GET_IID(idx)] + (reg), \
			((val) & (msk)) | \
			(REG_RD(s_isp_regbase[ISP_GET_IID(idx)] + (reg)) & \
			 (~(msk))))

#define ISP_HREG_OWR(idx, reg, val) \
		REG_WR(s_isp_regbase[ISP_GET_IID(idx)] + (reg), \
			(REG_RD(s_isp_regbase[ISP_GET_IID(idx)] + (reg)) | \
			 (val)))


/* enable path */
#define enable_scl_pre_cap_path(idx) \
	ISP_REG_MWR((idx), ISP_STORE_PRE_CAP_BASE + ISP_STORE_PARAM, BIT_0, 0)

#define enable_scl_vid_path(idx) \
	ISP_REG_MWR((idx), ISP_STORE_VID_BASE + ISP_STORE_PARAM, BIT_0, 0)

/* disable path */
#define disable_scl_pre_cap_path(idx) \
	ISP_REG_MWR((idx), ISP_STORE_PRE_CAP_BASE + ISP_STORE_PARAM, BIT_0, 1)

#define disable_scl_vid_path(idx) \
	ISP_REG_MWR((idx), ISP_STORE_VID_BASE + ISP_STORE_PARAM, BIT_0, 1)

/* open path */
#define open_scl_pre_cap_path_from_pipeline(idx) \
	ISP_HREG_MWR((idx), ISP_COMMON_SCL_PATH_SEL, (BIT_0 | BIT_1), 0 << 0)

#define open_scl_pre_cap_path_from_dispatch(idx) \
	ISP_HREG_MWR((idx), ISP_COMMON_SCL_PATH_SEL, (BIT_0 | BIT_1), 1 << 0)

#define open_scl_vid_path_from_pipeline(idx) \
	ISP_HREG_MWR((idx), ISP_COMMON_SCL_PATH_SEL, (BIT_2 | BIT_3), 0 << 2)

#define open_scl_vid_path_from_dispatch(idx) \
	ISP_HREG_MWR((idx), ISP_COMMON_SCL_PATH_SEL, (BIT_2 | BIT_3), 1 << 2)

/* close path */
#define close_scl_pre_cap_path(idx) \
	ISP_HREG_MWR((idx), ISP_COMMON_SCL_PATH_SEL, (BIT_0 | BIT_1), 3 << 0)

#define close_scl_vid_path(idx) \
	ISP_HREG_MWR((idx), ISP_COMMON_SCL_PATH_SEL, (BIT_2 | BIT_3), 3 << 2)

/* on: enable and open path */
#define on_scl_pre_cap_path_from_pipeline(idx)		\
do {							\
	enable_scl_pre_cap_path(idx);			\
	open_scl_pre_cap_path_from_pipeline(idx);	\
} while (0)

#define on_scl_pre_cap_path_from_dispatch(idx)		\
do {							\
	enable_scl_pre_cap_path(idx);			\
	open_scl_pre_cap_path_from_dispatch(idx);	\
} while (0)

#define on_scl_vid_path_from_pipeline(idx)		\
do {							\
	enable_scl_vid_path(idx);			\
	open_scl_vid_path_from_pipeline(idx);		\
} while (0)

#define on_scl_vid_path_from_dispatch(idx)		\
do {							\
	enable_scl_vid_path(idx);			\
	open_scl_vid_path_from_dispatch(idx);		\
} while (0)

/* off: disable and close path */
#define off_scl_pre_cap_path(idx)			\
do {							\
	disable_scl_pre_cap_path(idx);			\
	close_scl_pre_cap_path(idx);			\
} while (0)

#define off_scl_vid_path(idx)				\
do {							\
	disable_scl_vid_path(idx);			\
	close_scl_vid_path(idx);			\
} while (0)

#endif
