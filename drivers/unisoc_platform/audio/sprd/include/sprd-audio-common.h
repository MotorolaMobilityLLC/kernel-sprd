#ifndef __SPRD_AUDIO_COMMON_H_
#define __SPRD_AUDIO_COMMON_H_
/*
 * sprd-asoc-agcp.h and sprd-audio-sharkl3.h have many similar parameters,
 * which will be placed in this file. This file should be included in
 * sprd-asoc-agcp.h and sprd-audio-sharkl3.h
 */
#include "sprd-audio-sharkl3.h"
#include "sprd-audio-agcp.h"

#define CODEC_DP_BASE		0x1000
#define CODEC_AP_OFFSET		0
#define CODEC_AP_BASE_2721		0x2000
#define CODEC_AP_BASE_AGCP		0x3000
/*AP_APB registers offset */
#define REG_AP_APB_APB_EB		0x0000
#define REG_AP_APB_APB_RST		0x0004

/* REG_AP_APB_APB_EB */
#define BIT_AP_APB_IIS0_EB                      BIT(1)

/* REG_AP_APB_APB_RST */
#define BIT_AP_APB_IIS0_SOFT_RST                BIT(1)

enum ag_iis {
	AG_IIS0,
	AG_IIS1,
	AG_IIS2,
	AG_IIS_MAX
};

/* vbc r1p0v3 and r2p0 have no such control.
 * AGCP IIS multiplexer setting.
 * @iis: the iis channel to be set.
 * @en:
 *   0: AG_IIS0_EXT_SEL to whale2 top
 *   1: AG_IIS0_EXT_SEL to audio top
 */
static inline int arch_audio_iis_to_audio_top_enable(
	enum ag_iis iis, int en, struct device *dev)
{
	u32 val;
	int ret;
#if (defined(CONFIG_SND_SOC_UNISOC_SHARKL3) || defined(CONFIG_SND_SOC_UNISOC_SHARKL3_MODULE))
	return 0;
#endif

	agcp_ahb_gpr_null_check();

	switch (iis) {
	case AG_IIS0:
		val = BIT_AG_IIS0_EXT_SEL;
		break;
	case AG_IIS1:
		val = BIT_AG_IIS1_EXT_SEL;
		break;
	case AG_IIS2:
		val = BIT_AG_IIS2_EXT_SEL;
		break;
	default:
		pr_err("%s, agcp iis mux setting error!\n", __func__);
		return -1;
	}

	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		pr_err("%s, agdsp_access_enable failed!\n", __func__);
		return ret;
	}

	if (en)
		agcp_ahb_reg_set(REG_AGCP_AHB_EXT_ACC_AG_SEL, val);
	else
		agcp_ahb_reg_clr(REG_AGCP_AHB_EXT_ACC_AG_SEL, val);

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return 0;
}
/* i2s setting */
static inline const char *arch_audio_i2s_clk_name(int id)
{
	switch (id) {
	case 0:
		return "clk_iis0";
	case 1:
		return "clk_iis1";
	case 2:
		return "clk_iis2";
	case 3:
		return "clk_iis3";
	default:
		break;
	}
	return NULL;
}

static inline int arch_audio_i2s_enable(int id)
{
	int ret = 0;

	switch (id) {
	case 0:
		ap_apb_reg_set(REG_AP_APB_APB_EB, BIT_AP_APB_IIS0_EB);
		break;
	case 1:
	case 2:
	case 3:
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static inline int arch_audio_i2s_disable(int id)
{
	int ret = 0;

	switch (id) {
	case 0:
		ap_apb_reg_clr(REG_AP_APB_APB_EB, BIT_AP_APB_IIS0_EB);
		break;
	case 1:
	case 2:
	case 3:
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static inline int arch_audio_i2s_reset(int id)
{
	int ret = 0;

	switch (id) {
	case 0:
		ap_apb_reg_set(REG_AP_APB_APB_RST, BIT_AP_APB_IIS0_SOFT_RST);
		udelay(10);
		ap_apb_reg_clr(REG_AP_APB_APB_RST, BIT_AP_APB_IIS0_SOFT_RST);
		break;
	case 1:
#if (defined(CONFIG_SND_SOC_UNISOC_SHARKL3) || defined(CONFIG_SND_SOC_UNISOC_SHARKL3_MODULE))
		ap_apb_reg_set(REG_AP_APB_APB_RST, BIT_AP_APB_IIS0_SOFT_RST);
		udelay(10);
		ap_apb_reg_clr(REG_AP_APB_APB_RST, BIT_AP_APB_IIS0_SOFT_RST);
		break;
#endif
	case 2:
#if (defined(CONFIG_SND_SOC_UNISOC_SHARKL3) || defined(CONFIG_SND_SOC_UNISOC_SHARKL3_MODULE))
		ap_apb_reg_set(REG_AP_APB_APB_RST, BIT_AP_APB_IIS0_SOFT_RST);
		udelay(10);
		ap_apb_reg_clr(REG_AP_APB_APB_RST, BIT_AP_APB_IIS0_SOFT_RST);
		break;
#endif
	case 3:
#if (defined(CONFIG_SND_SOC_UNISOC_SHARKL3) || defined(CONFIG_SND_SOC_UNISOC_SHARKL3_MODULE))
		ap_apb_reg_set(REG_AP_APB_APB_RST, BIT_AP_APB_IIS0_SOFT_RST);
		udelay(10);
		ap_apb_reg_clr(REG_AP_APB_APB_RST, BIT_AP_APB_IIS0_SOFT_RST);
		break;
#endif
	default:
		ret = -ENODEV;
		break;
	}
	return ret;
}

static inline int arch_audio_i2s_tx_dma_info(int id)
{
	int ret = 0;

	switch (id) {
	case 0:
		ret = DMA_REQ_IIS0_TX;
		break;
	case 1:
	case 2:
	case 3:
	default:
		ret = -ENODEV;
		break;
	}

	return ret;
}

static inline int arch_audio_i2s_rx_dma_info(int id)
{
	int ret = 0;

	switch (id) {
	case 0:
		ret = DMA_REQ_IIS0_RX;
		break;
	case 1:
	case 2:
	case 3:
	default:
		ret = -ENODEV;
		break;
	}

	return ret;
}

/* Codec digital part in soc setting */
static inline int arch_audio_codec_digital_reg_enable(struct device *dev)
{
	int ret = 0;
	int test_v;

#if (defined(CONFIG_SND_SOC_UNISOC_SHARKL3) || defined(CONFIG_SND_SOC_UNISOC_SHARKL3_MODULE))
	aon_apb_gpr_null_check();
	ret = aon_apb_reg_set(REG_AON_APB_APB_EB0, BIT_AON_APB_AUD_EB);
	if (ret >= 0)
		arch_audio_codec_audif_enable(0);
	return ret;
#endif

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
	agcp_ahb_reg_read(REG_AGCP_AHB_MODULE_EB0_STS, &test_v);

	pr_debug("enter %s REG_AGCP_AHB_MODULE_EB0_STS, test_v=%#x\n",
		__func__, test_v);
	ret = agcp_ahb_reg_set(REG_AGCP_AHB_MODULE_EB0_STS, BIT_AUD_EB);
	if (ret >= 0)
		ret = agcp_ahb_reg_set(REG_AGCP_AHB_MODULE_EB0_STS,
			BIT_AUDIF_CKG_AUTO_EN);
	agcp_ahb_reg_read(REG_AGCP_AHB_MODULE_EB0_STS, &test_v);
	pr_debug("%s set aud en %#x\n", __func__, test_v);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return ret;
}

static inline int arch_audio_codec_digital_reg_disable(struct device *dev)
{
	int ret = 0;

#if (defined(CONFIG_SND_SOC_UNISOC_SHARKL3) || defined(CONFIG_SND_SOC_UNISOC_SHARKL3_MODULE))
	aon_apb_gpr_null_check();
	arch_audio_codec_audif_disable();
	aon_apb_reg_clr(REG_AON_APB_APB_EB0, BIT_AON_APB_AUD_EB);
	return 0;
#endif
	agcp_ahb_gpr_null_check();
	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		pr_err("%s, agdsp_access_enable failed!\n", __func__);
		return ret;
	}
	ret = agcp_ahb_reg_clr(REG_AGCP_AHB_MODULE_EB0_STS,
		BIT_AUDIF_CKG_AUTO_EN);
	if (ret >= 0)
		ret = agcp_ahb_reg_clr(REG_AGCP_AHB_MODULE_EB0_STS, BIT_AUD_EB);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return ret;
}

static inline int arch_audio_codec_digital_enable(void)
{
	int ret = 0;

#if (defined(CONFIG_SND_SOC_UNISOC_SHARKL3) || defined(CONFIG_SND_SOC_UNISOC_SHARKL3_MODULE))
	ret = anlg_phy_g_null_check();
	if (ret < 0) {
		pr_err("%s failed\n", __func__);
		return ret;
	}
	/* internal digital 26M enable */
	ret = anlg_phy_g_reg_set(REG_AON_APB_SINDRV_CTRL,
			BIT_AON_APB_SINDRV_ENA);
	if (ret != 0)
		pr_err("%s set failed", __func__);
#endif

	return ret;
}

static inline int arch_audio_codec_digital_disable(void)
{
	int ret = 0;

#if (defined(CONFIG_SND_SOC_UNISOC_SHARKL3) || defined(CONFIG_SND_SOC_UNISOC_SHARKL3_MODULE))
	ret = anlg_phy_g_null_check();
	if (ret < 0) {
		pr_err("%s failed\n", __func__);
		return ret;
	}
	/* internal digital 26M disable */
	ret = anlg_phy_g_reg_clr(REG_AON_APB_SINDRV_CTRL,
			BIT_AON_APB_SINDRV_ENA);
	if (ret != 0)
		pr_err("%s set failed", __func__);
#endif

	return ret;
}

static inline int arch_audio_codec_switch2ap(void)
{

#if (defined(CONFIG_SND_SOC_UNISOC_SHARKL3) || defined(CONFIG_SND_SOC_UNISOC_SHARKL3_MODULE))
	return arch_audio_codec_switch(ADU_DIGITAL_INT_TO_AP_CTRL);
#endif

	return 0;
}

static inline int arch_audio_codec_digital_reset_agdsp(struct device *dev)
{
	int ret = 0;

	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		pr_err("%s, agdsp_access_enable failed!\n", __func__);
		return ret;
	}
	agcp_ahb_gpr_null_check();
	agcp_ahb_reg_set(REG_AGCP_AHB_MODULE_RST0_STS, BIT_AUD_SOFT_RST);
	udelay(10);
	agcp_ahb_reg_clr(REG_AGCP_AHB_MODULE_RST0_STS, BIT_AUD_SOFT_RST);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return ret;
}
static inline void arch_audio_codec_digital_reset_aon(void)
{
	aon_apb_gpr_null_check();
	aon_apb_reg_set(REG_AON_APB_APB_RST0, BIT_AON_APB_AUD_SOFT_RST);
	aon_apb_reg_set(REG_AON_APB_APB_RST0, BIT_AON_APB_AUDIF_SOFT_RST);
	udelay(10);
	aon_apb_reg_clr(REG_AON_APB_APB_RST0, BIT_AON_APB_AUD_SOFT_RST);
	aon_apb_reg_clr(REG_AON_APB_APB_RST0, BIT_AON_APB_AUDIF_SOFT_RST);
}

static inline int arch_audio_codec_digital_reset(struct device *dev)
{
#if (defined(CONFIG_SND_SOC_UNISOC_SHARKL3) || defined(CONFIG_SND_SOC_UNISOC_SHARKL3_MODULE))
	arch_audio_codec_digital_reset_aon();
	return 0;
#else
	return arch_audio_codec_digital_reset_agdsp(dev);
#endif
}

#endif