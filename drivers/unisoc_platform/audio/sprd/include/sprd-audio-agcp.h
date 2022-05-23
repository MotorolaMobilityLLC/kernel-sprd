/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

#ifndef __SPRD_AUDIO_AGCP_H_
#define __SPRD_AUDIO_AGCP_H_

#ifndef __SPRD_AUDIO_H
#error  "Don't include this file directly, include sprd-audio.h"
#endif

#include <linux/device.h>
#include <linux/pm_runtime.h>

/* AGCP AHB registers doesn't defined by global header file. So
 * define them here.
 */
#define REG_AGCP_AHB_MODULE_EB0_STS	0x00
#define REG_AGCP_AHB_MODULE_RST0_STS	0x08
#define REG_AGCP_AHB_EXT_ACC_AG_SEL	0x3c

/*--------------------------------------------------
 * Register Name   : REG_AGCP_AHB_MODULE_EB0_STS
 * Register Offset : 0x0000
 * Description     :
 * ---------------------------------------------------
 */
#define BIT_AUDIF_CKG_AUTO_EN	BIT(20)
#define BIT_AUD_EB				BIT(19)

/* for ums9620 */
#define BIT_AUDIF_CKG_AUTO_EN_V2	BIT(22)
#define BIT_AUD_EB_V2				BIT(21)
#define BIT_VBC_EB_V2				BIT(15)
#define BIT_DMA_AP_ASHB_EB_V2		BIT(19)
#define BIT_DMA_AP_EB_V2			BIT(6)

/*--------------------------------------------------
 * Register Name   : REG_AGCP_AHB_MODULE_RST0_STS
 * Register Offset : 0x0008
 * Description     :
 * ---------------------------------------------------
 */
#define BIT_AUD_SOFT_RST		BIT(25)

/*--------------------------------------------------
 * Register Name   : REG_AGCP_AHB_EXT_ACC_AG_SEL
 * Register Offset : 0x003c
 * Description     :
 * ---------------------------------------------------
 */
#define BIT_AG_IIS2_EXT_SEL                      BIT(2)
#define BIT_AG_IIS1_EXT_SEL                      BIT(1)
#define BIT_AG_IIS0_EXT_SEL                      BIT(0)

#define BIT_AG_IIS4_EXT_SEL_V1                      (BIT(6) | BIT(7))
#define BIT_AG_IIS2_EXT_SEL_V1                      (BIT(4) | BIT(5))
#define BIT_AG_IIS1_EXT_SEL_V1                      (BIT(2) | BIT(3))
#define BIT_AG_IIS0_EXT_SEL_V1                      (BIT(0) | BIT(1))

#define SHIFT_AG_IIS4_EXT_SEL_V1				6
#define SHIFT_AG_IIS2_EXT_SEL_V1				4
#define SHIFT_AG_IIS1_EXT_SEL_V1				2
#define SHIFT_AG_IIS0_EXT_SEL_V1				0

/* ums9620 */
#define BIT_AG_IIS3_EXT_SEL_V2                      BIT(8)
#define BIT_AG_IIS4_EXT_SEL_V2                      (BIT(6) | BIT(7))
#define BIT_AG_IIS2_EXT_SEL_V2                      (BIT(4) | BIT(5))
#define BIT_AG_IIS1_EXT_SEL_V2                      (BIT(2) | BIT(3))
#define BIT_AG_IIS0_EXT_SEL_V2                      (BIT(0) | BIT(1))

#define SHIFT_AG_IIS3_EXT_SEL_V2				8
#define SHIFT_AG_IIS4_EXT_SEL_V2				6
#define SHIFT_AG_IIS2_EXT_SEL_V2				4
#define SHIFT_AG_IIS1_EXT_SEL_V2				2
#define SHIFT_AG_IIS0_EXT_SEL_V2				0

/* ----------------------------------------------- */
enum ag_iis_v1 {
	AG_IIS0_V1,
	AG_IIS1_V1,
	AG_IIS2_V1,
	AG_IIS4_V1,
	AG_IIS_V1_MAX
};

/* used for ums9620 */
enum ag_iis_v2 {
	AG_IIS0_V2,
	AG_IIS1_V2,
	AG_IIS2_V2,
	AG_IIS4_V2,
	AG_IIS3_V2,
	AG_IIS_V2_MAX
};

static inline int arch_audio_iis_to_audio_top_enable_v1(
	enum ag_iis_v1 iis, int mode, struct device *dev)
{
	u32 mask;
	int shift;
	int ret;

	ret = agcp_ahb_gpr_null_check();
	if (ret) {
		pr_err("%s agcp_ahb_gpr_null_check failed!", __func__);
		return -1;
	}
	switch (iis) {
	case AG_IIS0_V1:
		mask = BIT_AG_IIS0_EXT_SEL_V1;
		shift = SHIFT_AG_IIS0_EXT_SEL_V1;
		break;
	case AG_IIS1_V1:
		mask = BIT_AG_IIS1_EXT_SEL_V1;
		shift = SHIFT_AG_IIS1_EXT_SEL_V1;
		break;
	case AG_IIS2_V1:
		mask = BIT_AG_IIS2_EXT_SEL_V1;
		shift = SHIFT_AG_IIS2_EXT_SEL_V1;
		break;
	case AG_IIS4_V1:
		mask = BIT_AG_IIS4_EXT_SEL_V1;
		shift = SHIFT_AG_IIS4_EXT_SEL_V1;
		break;
	default:
		pr_err("%s, sharkl6 agcp iis mux setting error!\n", __func__);
		return -1;
	}

	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		pr_err("%s, agdsp_access_enable failed!\n", __func__);
		return ret;
	}

	agcp_ahb_reg_update(REG_AGCP_AHB_EXT_ACC_AG_SEL, mask, mode<<shift);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return 0;
}

/* used for ums9620 */
static inline int arch_audio_iis_to_audio_top_enable_v2(
	enum ag_iis_v2 iis, int mode, struct device *dev)
{
	u32 mask;
	int shift, ret;

	ret = agcp_ahb_gpr_null_check();
	if (ret) {
		pr_err("%s agcp_ahb_gpr_null_check failed!", __func__);
		return -1;
	}

	switch (iis) {
	case AG_IIS0_V2:
		mask = BIT_AG_IIS0_EXT_SEL_V2;
		shift = SHIFT_AG_IIS0_EXT_SEL_V2;
		break;
	case AG_IIS1_V2:
		mask = BIT_AG_IIS1_EXT_SEL_V2;
		shift = SHIFT_AG_IIS1_EXT_SEL_V2;
		break;
	case AG_IIS2_V2:
		mask = BIT_AG_IIS2_EXT_SEL_V2;
		shift = SHIFT_AG_IIS2_EXT_SEL_V2;
		break;
	case AG_IIS4_V2:
		mask = BIT_AG_IIS4_EXT_SEL_V2;
		shift = SHIFT_AG_IIS4_EXT_SEL_V2;
		break;
	case AG_IIS3_V2:
		mask = BIT_AG_IIS3_EXT_SEL_V2;
		shift = SHIFT_AG_IIS3_EXT_SEL_V2;
		break;
	default:
		pr_err("%s agcp iis mux setting error!\n", __func__);
		return -1;
	}

	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		pr_err("%s, agdsp_access_enable failed!\n", __func__);
		return ret;
	}

	agcp_ahb_reg_update(REG_AGCP_AHB_EXT_ACC_AG_SEL, mask, mode<<shift);

	agcp_ahb_reg_read(REG_AGCP_AHB_EXT_ACC_AG_SEL, &mask);
	pr_debug("%s AHB_EXT_ACC_AG_SEL 0x%x\n", __func__, mask);

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return 0;
}

/* Codec digital part in soc setting for ums9620 */
static inline int arch_audio_codec_digital_reg_enable_v2(struct device *dev)
{
	int ret = 0;
	unsigned int val;

	ret = agcp_ahb_gpr_null_check();
	if (ret) {
		pr_err("%s agcp_ahb_gpr_null_check failed!", __func__);
		return -1;
	}
	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		pr_err("%s, agdsp_access_enable failed!\n", __func__);
		return ret;
	}

	ret = agcp_ahb_reg_set(REG_AGCP_AHB_MODULE_EB0_STS, BIT_AUD_EB_V2);
	if (ret >= 0)
		ret = agcp_ahb_reg_set(REG_AGCP_AHB_MODULE_EB0_STS,
			BIT_AUDIF_CKG_AUTO_EN_V2);
	agcp_ahb_reg_read(REG_AGCP_AHB_MODULE_EB0_STS, &val);
	pr_debug("%s AHB_MODULE_EB0_STS 0x%x\n", __func__, val);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return ret;
}

static inline int arch_audio_codec_digital_reg_disable_v2(struct device *dev)
{
	int ret = 0;

	agcp_ahb_gpr_null_check();
	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		pr_err("%s, agdsp_access_enable failed!\n", __func__);
		return ret;
	}
	ret = agcp_ahb_reg_clr(REG_AGCP_AHB_MODULE_EB0_STS,
		BIT_AUDIF_CKG_AUTO_EN_V2);
	if (ret >= 0)
		ret = agcp_ahb_reg_clr(REG_AGCP_AHB_MODULE_EB0_STS,
				       BIT_AUD_EB_V2);

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return ret;
}

#define DMA_REQ_IIS0_RX			(2 + 1)
#define DMA_REQ_IIS0_TX			(3 + 1)

#endif/* __SPRD_AUDIO_AGCP_H_ */
