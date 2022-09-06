/*
 * sound/soc/sprd/dai/i2s/i2s.c
 *
 * SPRD SoC CPU-DAI -- SpreadTrum SOC DAI
 *
 * Copyright (C) 2015 SpreadTrum Ltd.
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
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt(" TDM ")""fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "sprd-audio.h"
#include "sprd-asoc-common.h"
#if (defined(CONFIG_SND_SOC_UNISOC_AUDIO_TWO_STAGE_DMAENGINE) || defined(CONFIG_SND_SOC_UNISOC_AUDIO_TWO_STAGE_DMAENGINE_MODULE))
#include "sprd-2stage-dmaengine-pcm.h"
#else
#include "sprd-dmaengine-pcm.h"
#endif
#include "tdm.h"
#include "sprd-tdm.h"

#define DAI_NAME_SIZE		20
#define DMA_REQ_TDM_TX		(20 + 1)
#define DMA_REQ_TDM_RX		(21 + 1)

#define TDM_REG(tdm, offset) ((unsigned long)((tdm)->membase + (offset)))
#define TDM_PHY_REG(tdm, offset) ((phys_addr_t)(tdm)->memphys + (offset))

#define SPRD_TDM_FMTBIT \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | \
	SNDRV_PCM_FMTBIT_S32_LE)

static int tdm_enable;
static struct tdm_config *dup_config;

static struct regmap *ap_ahb_gpr;

static struct regmap *audcp_clk_rf;
#define audcp_clk_rf_reg_set(reg, bit) \
		regmap_update_bits(audcp_clk_rf, (reg), (bit), (bit))
#define audcp_clk_rf_reg_clr(reg, bit) \
		regmap_update_bits(audcp_clk_rf, (reg), (bit), (0))
#define audcp_clk_rf_reg_read(reg, val) \
		regmap_read(audcp_clk_rf, (reg), (val))

static struct regmap *audcp_dvfs_apb_rf;
#define audcp_dvfs_apb_rf_reg_update(reg, msk, val) \
	regmap_update_bits(audcp_dvfs_apb_rf, (reg), (msk), (val))
#define audcp_dvfs_apb_rf_reg_set(reg, bit) \
	regmap_update_bits(audcp_dvfs_apb_rf, (reg), (bit), (bit))
#define audcp_dvfs_apb_rf_reg_clr(reg, bit) \
	regmap_update_bits(audcp_dvfs_apb_rf, (reg), (bit), (0))
#define audcp_dvfs_apb_rf_reg_read(reg, val) \
		regmap_read(audcp_dvfs_apb_rf, (reg), (val))

static struct regmap *aon_apb_rf;
#define aon_apb_rf_reg_set(reg, bit) \
		regmap_update_bits(aon_apb_rf, (reg), (bit), (bit))
#define aon_apb_rf_reg_clr(reg, bit) \
		regmap_update_bits(aon_apb_rf, (reg), (bit), (0))
#define aon_apb_rf_reg_read(reg, val) \
		regmap_read(aon_apb_rf, (reg), (val))

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
int agdsp_access_enable(void)
	__attribute__ ((weak, alias("__agdsp_access_enable")));
static int __agdsp_access_enable(void)
{
	pr_debug("%s\n", __func__);
	return 0;
}

int agdsp_access_disable(void)
	__attribute__ ((weak, alias("__agdsp_access_disable")));
static int __agdsp_access_disable(void)
{
	pr_debug("%s\n", __func__);
	return 0;
}
#pragma GCC diagnostic pop

struct tdm_priv {
	struct device *dev;
	atomic_t open_cnt;
	char dai_name[DAI_NAME_SIZE];
	unsigned int *memphys;
	void __iomem *membase;
	unsigned int reg_size;
	int set_reg_rtx;
	struct tdm_config config;
};

/* default tdm config */
static const struct tdm_config def_tdm_config = {
	.tdm_fs = 48000,
	.slave_timeout = 0x1E00,
	.trx_mst_mode = TDM_MASTER,
	.duplex_mode = TDM_FULL_DUPLEX,
	.trx_threshold = 128,
	.trx_data_width = TDM_16BIT,
	.trx_data_mode = TDM_I2S_COMPATIBLE,
	.trx_slot_width = TDM_16BIT,
	.trx_slot_num = TDM_2SLOT,
	.trx_msb_mode = TDM_LSB,
	.trx_pulse_mode = TDM_LRCK,
	.tdm_slot_valid = 3,
	.trx_sync_mode = USE_LRCK_N_CAPTURE,
	.byte_per_chan = 2,
	.tx_watermark = 12,
	.rx_watermark = 20,
	.tdm_lrck_inv = LRCK_INVERT_I2S,
	.tx_bck_pos_dly = 0,
	.rx_bck_pos_dly = 0,
};

char *use_dma_name[] = {
	"tdm_tx", "tdm_rx",
};

static struct sprd_pcm_dma_params tdm_pcm_stereo_out = {
	.name = "TDM PCM Stereo out",
	.irq_type = 0,
	.desc = {
		 .datawidth = DMA_SLAVE_BUSWIDTH_4_BYTES,
		 .src_step = 4,
		 .des_step = 0,
		 },
};

static struct sprd_pcm_dma_params tdm_pcm_stereo_in = {
	.name = "TDM PCM Stereo in",
	.irq_type = 0,
	.desc = {
		 .datawidth = DMA_SLAVE_BUSWIDTH_4_BYTES,
		 .src_step = 0,
		 .des_step = 4,
		 },
};

static inline int tdm_reg_read(unsigned long reg)
{
	return readl_relaxed((void *__iomem)reg);
}

static inline void tdm_reg_raw_write(unsigned long reg, int val)
{
	writel_relaxed(val, (void *__iomem)reg);
}

/*
 * Returns 1 for change, 0 for no change, or negative error code.
 */
static int tdm_reg_update(unsigned long reg, int val, int mask)
{
	int new, old;

	old = tdm_reg_read(reg);
	new = (old & ~mask) | (val & mask);
	tdm_reg_raw_write(reg, new);
	sp_asoc_pr_reg("[0x%04lx] U:[0x%08x] R:[0x%08x]\n",
			reg & 0xFFFF, new, tdm_reg_read(reg));
	return old != new;
}

static void tdm_debug_reg_read(struct tdm_priv *tdm)
{
	int i;
	unsigned long reg;
	unsigned long tdm_regarray[12] = {TDM_REG_TX_CTRL, TDM_REG_TX_CFG0,
					TDM_REG_TX_CFG1, TDM_REG_TX_STAT, TDM_REG_TX_CFG2,
					TDM_REG_RX_CTRL, TDM_REG_RX_CFG0, TDM_REG_RX_CFG1,
					TDM_REG_RX_STAT, TDM_REG_RX_CFG2, TDM_REG_MISC_CTRL,
					TDM_REG_INT_STAT};

	for (i = 0; i < 11; i++) {
		reg = TDM_REG(tdm, tdm_regarray[i]);
		sp_asoc_pr_reg("A:[0x%08lx] R:[0x%08x]\n", reg & 0xFFFFFFFF,
			tdm_reg_read(reg));
	}
}

static int tdm_get_dma_data_width(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;

	if (config->trx_data_width == TDM_16BIT)
		return DMA_SLAVE_BUSWIDTH_2_BYTES;
	return DMA_SLAVE_BUSWIDTH_4_BYTES;
}

static int tdm_get_dma_step(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;

	if (config->trx_slot_width == TDM_16BIT)
		return 2;
	return 4;
}

static int tdm_global_enable(struct tdm_priv *tdm)
{
	int ret;

	sp_asoc_pr_dbg("%s\n", __func__);
	ret = agdsp_access_enable();
	tdm_enable = 1;
	return ret;
}

static int tdm_global_disable(struct tdm_priv *tdm)
{
	int ret;

	sp_asoc_pr_dbg("%s\n", __func__);
	ret = agdsp_access_disable();
	tdm_enable = 0;
	return ret;
}

void tdm_module_en(struct tdm_priv *tdm, bool en)
{
	int ret;

	pr_info("%s %d \n", __func__, __LINE__);
	if (en) {
		ret = tdm_global_enable(tdm);
		if (ret) {
			pr_err("%s agdsp_access_enable error %d",
						__func__, ret);
			return;
		}
		agcp_ahb_reg_set(GLB_MODULE_RST0_STS, TDM_HF_SOFT_RST);
		udelay(10);
		agcp_ahb_reg_clr(GLB_MODULE_RST0_STS, TDM_HF_SOFT_RST);
		agcp_ahb_reg_set(GLB_MODULE_RST0_STS, TDM_SOFT_RST);
		udelay(10);
		agcp_ahb_reg_clr(GLB_MODULE_RST0_STS, TDM_SOFT_RST);
		agcp_ahb_reg_set(GLB_MODULE_EN0_STS, TDM_HF_EN);
		agcp_ahb_reg_set(GLB_MODULE_EN0_STS, TDM_EN);
		agcp_ahb_reg_update(GLB_MODULE_EN1_STS,
					DMA_TDM_RX_SEL(0x3), DMA_TDM_RX_SEL(0x1));
		agcp_ahb_reg_update(GLB_MODULE_EN1_STS,
					DMA_TDM_TX_SEL(0x3), DMA_TDM_TX_SEL(0x1));
	} else {
		agcp_ahb_reg_set(GLB_MODULE_RST0_STS, TDM_HF_SOFT_RST);
		udelay(10);
		agcp_ahb_reg_clr(GLB_MODULE_RST0_STS, TDM_HF_SOFT_RST);
		agcp_ahb_reg_set(GLB_MODULE_RST0_STS, TDM_SOFT_RST);
		udelay(10);
		agcp_ahb_reg_clr(GLB_MODULE_RST0_STS, TDM_SOFT_RST);
		agcp_ahb_reg_clr(GLB_MODULE_EN0_STS, TDM_HF_EN);
		agcp_ahb_reg_clr(GLB_MODULE_EN0_STS, TDM_EN);
		agcp_ahb_reg_update(GLB_MODULE_EN1_STS,
					DMA_TDM_RX_SEL(0x3), DMA_TDM_RX_SEL(0x0));
		agcp_ahb_reg_update(GLB_MODULE_EN1_STS,
					DMA_TDM_TX_SEL(0x3), DMA_TDM_TX_SEL(0x0));

		ret = tdm_global_disable(tdm);
		if (ret) {
			pr_err("%s agdsp_access_disable error %d",
						__func__, ret);
			return;
		}
	}
}

static void tdm_clk_set_cfg(struct tdm_priv *tdm)
{
	if (!tdm_enable) {
		pr_err("tdm is not open\n");
		return;
	} else {
		pr_info("tdm_clk_set_cfg set reg!");
		audcp_clk_rf_reg_set(AUDCP_CLK_RF_CGM_TDM_SLV_SEL,
							CGM_TDM_SLV_PAD_SEL);
		audcp_clk_rf_reg_set(AUDCP_CLK_RF_CGM_TDM_SEL,
							CGM_TDM_SEL);
		audcp_clk_rf_reg_set(AUDCP_CLK_RF_CGM_TDM_HF_SEL,
							CGM_TDM_HF_SEL_CLK);
	}
}

static void tdm_audcp_dvfs_set(struct tdm_priv *tdm)
{
	if (!tdm_enable) {
		pr_err("tdm is not open\n");
		return;
	} else {
		pr_info("tdm_audcp_dvfs_set set reg!");
		audcp_dvfs_apb_rf_reg_update(TDM_HF_INDEX0_MAP,
			TDM_HF_VOL_INDEX0(0xf), TDM_HF_VOL_INDEX0(0));
		audcp_dvfs_apb_rf_reg_clr(AUD_CP_DFS_IDLE_DISABLE,
			TDM_HF_DFS_IDLE_DISABLE);
		audcp_dvfs_apb_rf_reg_set(AD_CP_DVFS_CLK_REG_CFG0,
			CGM_TDM_HF_SEL);
		audcp_dvfs_apb_rf_reg_set(AD_CP_DVFS_CLK_REG_CFG0, 0x3);
		audcp_dvfs_apb_rf_reg_clr(AD_CP_DVFS_CLK_REG_CFG0, 0x1FF8);
	}
}

static void tdm_aud_access_en(struct tdm_priv *tdm)
{
	if (!tdm_enable) {
		pr_err("tdm is not open\n");
		return;
	} else {
		pr_info("tdm_aud_access_en set reg!");
		aon_apb_rf_reg_set(AUDCP_CTRL,
						AON_2_AUD_ACCESS_EN);
	}
}

static void tdm_set_dma_en(struct tdm_priv *tdm, int enable)
{
	int mask = TDM_MISC_CTRL_DMA_EN;
	unsigned long reg = TDM_REG(tdm, TDM_REG_MISC_CTRL);

	pr_info("%s Enable = %d\n", __func__, enable);
	if (!enable) {
		if (atomic_read(&tdm->open_cnt) <= 0)
			tdm_reg_update(reg, 0, mask);
	} else {
		tdm_reg_update(reg, mask, mask);
	}
}

static void tdm_set_duplex_mode(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;
	int shift = 3;
	int mask = 0x3 << shift;
	int val = 0;
	unsigned long reg = TDM_REG(tdm, TDM_REG_MISC_CTRL);

	pr_info("%s\n", __func__);
	val = config->duplex_mode;
	tdm_reg_update(reg, val << shift, mask);
}

static void tdm_set_auto_gate_en(struct tdm_priv *tdm, int enable)
{
	int mask = TDM_MISC_CTRL_AUTO_GATE_EN;
	unsigned long reg = TDM_REG(tdm, TDM_REG_MISC_CTRL);

	pr_info("%s Enable = %d\n", __func__, enable);
	if (!enable) {
		if (atomic_read(&tdm->open_cnt) <= 0)
			tdm_reg_update(reg, 0, mask);
	} else {
		tdm_reg_update(reg, mask, mask);
	}
}

struct tdm_config *sprd_tdm_dai_to_config(struct snd_soc_dai *dai)
{
	struct tdm_priv *tdm = NULL;
	struct tdm_config *config = NULL;

	tdm = snd_soc_dai_get_drvdata(dai);
	config = &tdm->config;
	return config;
}
EXPORT_SYMBOL(sprd_tdm_dai_to_config);

static void tdm_set_int_en(struct tdm_priv *tdm, int enable)
{
	int mask = TDM_MISC_CTRL_INT_EN;
	unsigned long reg = TDM_REG(tdm, TDM_REG_MISC_CTRL);

	pr_info("%s Enable = %d\n", __func__, enable);
	if (!enable) {
		if (atomic_read(&tdm->open_cnt) <= 0)
			tdm_reg_update(reg, 0, mask);
	} else {
		tdm_reg_update(reg, mask, mask);
	}
}

static void tdm_get_int_stat(struct tdm_priv *tdm)
{
	unsigned long reg = TDM_REG(tdm, TDM_REG_INT_STAT);

	sp_asoc_pr_reg("[0x%04lx] R:[0x%08x]\n", reg & 0xFFFF,
		       tdm_reg_read(reg));
}

static void tdm_clr_tx_int(struct tdm_priv *tdm)
{
	int mask = TDM_INT_STAT_TXFIFO_ALMOST_EMPTY
				| TDM_INT_STAT_TXFIFO_UNDERFLOW
				| TDM_INT_STAT_TX_TIMEOUT;
	unsigned long reg = TDM_REG(tdm, TDM_REG_INT_STAT);

	pr_info("%s\n", __func__);
	tdm_reg_update(reg, mask, mask);
}

static void tdm_clr_rx_int(struct tdm_priv *tdm)
{
	int mask = TDM_INT_STAT_RXFIFO_ALMOST_FULL
				| TDM_INT_STAT_RXFIFO_OVERFLOW
				| TDM_INT_STAT_RX_TIMEOUT;
	unsigned long reg = TDM_REG(tdm, TDM_REG_INT_STAT);

	pr_info("%s\n", __func__);
	tdm_reg_update(reg, mask, mask);
}

static void tdm_bck_pos_dly(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;
	int mask;
	int shift = 4;
	unsigned long reg;

	pr_info("%s\n", __func__);
	if (tdm->set_reg_rtx == SET_TX_REG) {
		mask = TDM_TX_CTRL_BCK_POS_DLY(0x7);
		reg = TDM_REG(tdm, TDM_REG_TX_CTRL);
		tdm_reg_update(reg,
			config->tx_bck_pos_dly << shift, mask);
	} else {
		mask = TDM_RX_CTRL_BCK_POS_DLY(0x7);
		reg = TDM_REG(tdm, TDM_REG_RX_CTRL);
		tdm_reg_update(reg,
			config->rx_bck_pos_dly << shift, mask);
	}
}

static void tdm_io_en(struct tdm_priv *tdm, int enable)
{
	int mask;
	int shift = 3;
	unsigned long reg;
	int val = enable << shift;

	pr_info("%s Enable = %d\n", __func__, enable);
	if (tdm->set_reg_rtx == SET_TX_REG) {
		mask = TDM_TX_CTRL_IO_EN;
		reg = TDM_REG(tdm, TDM_REG_TX_CTRL);
		tdm_reg_update(reg, val, mask);
	} else {
		mask = TDM_RX_CTRL_IO_EN;
		reg = TDM_REG(tdm, TDM_REG_RX_CTRL);
		tdm_reg_update(reg, val, mask);
	}
}

static void tdm_timeout_en(struct tdm_priv *tdm, int enable)
{
	int mask;
	int shift = 2;
	unsigned long reg;
	int val = enable << shift;

	pr_info("%s Enable = %d\n", __func__, enable);
	if (tdm->set_reg_rtx == SET_TX_REG) {
		mask = TDM_TX_CTRL_TIMEOUT_EN;
		reg = TDM_REG(tdm, TDM_REG_TX_CTRL);
		tdm_reg_update(reg, val, mask);
	} else {
		mask = TDM_RX_CTRL_TIMEOUT_EN;
		reg = TDM_REG(tdm, TDM_REG_RX_CTRL);
		tdm_reg_update(reg, val, mask);
	}
}

static void tdm_soft_reset(struct tdm_priv *tdm)
{
	int mask;
	unsigned long reg;

	pr_info("%s\n", __func__);
	if (tdm->set_reg_rtx == SET_TX_REG) {
		mask = TDM_TX_CTRL_SOFT_RST;
		reg = TDM_REG(tdm, TDM_REG_TX_CTRL);
		tdm_reg_update(reg, 1, mask);
		udelay(10);
		tdm_reg_update(reg, 0, mask);
	} else {
		mask = TDM_RX_CTRL_SOFT_RST;
		reg = TDM_REG(tdm, TDM_REG_RX_CTRL);
		tdm_reg_update(reg, 1, mask);
		udelay(10);
		tdm_reg_update(reg, 0, mask);
	}
}

static void tdm_controller_en(struct tdm_priv *tdm, int enable)
{
	int mask;
	int shift = 0;
	unsigned long reg;
	int val = enable << shift;

	pr_info("%s\n", __func__);
	if (tdm->set_reg_rtx == SET_TX_REG) {
		mask = TDM_TX_CTRL_ENABLE;
		reg = TDM_REG(tdm, TDM_REG_TX_CTRL);
		tdm_reg_update(reg, val, mask);
	} else {
		mask = TDM_RX_CTRL_ENABLE;
		reg = TDM_REG(tdm, TDM_REG_RX_CTRL);
		tdm_reg_update(reg, val, mask);
	}
}

static void tdm_set_threshold(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;
	int mask;
	int shift = 24;
	unsigned long reg;

	pr_info("%s\n", __func__);
	if (tdm->set_reg_rtx == SET_TX_REG) {
		mask = TDM_TX_CFG0_THRESHOLD(0xff);
		reg = TDM_REG(tdm, TDM_REG_TX_CFG0);
		tdm_reg_update(reg,
			config->trx_threshold << shift, mask);
	} else {
		mask = TDM_RX_CFG0_THRESHOLD(0xff);
		reg = TDM_REG(tdm, TDM_REG_RX_CFG0);
		tdm_reg_update(reg,
			config->trx_threshold << shift, mask);
	}
}

static void tdm_set_slot_valid(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;
	int mask;
	int shift = 16;
	unsigned long reg;

	pr_info("%s\n", __func__);
	if (tdm->set_reg_rtx == SET_TX_REG) {
		mask = TDM_TX_CFG0_SLOT_VALID(0xff);
		reg = TDM_REG(tdm, TDM_REG_TX_CFG0);
		tdm_reg_update(reg,
			config->tdm_slot_valid << shift, mask);
	} else {
		mask = TDM_RX_CFG0_SLOT_VALID(0xff);
		reg = TDM_REG(tdm, TDM_REG_RX_CFG0);
		tdm_reg_update(reg,
			config->tdm_slot_valid << shift, mask);
	}
}

static void tdm_set_data_width(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;
	int shift = 14;
	int mask;
	unsigned long reg;

	pr_info("%s\n", __func__);
	if (tdm->set_reg_rtx == SET_TX_REG) {
		mask = TDM_TX_CFG0_DATA_WIDTH(0x3);
		reg = TDM_REG(tdm, TDM_REG_TX_CFG0);
		tdm_reg_update(reg,
			config->trx_data_width << shift, mask);
	} else {
		mask = TDM_RX_CFG0_DATA_WIDTH(0x3);
		reg = TDM_REG(tdm, TDM_REG_RX_CFG0);
		tdm_reg_update(reg,
			config->trx_data_width << shift, mask);
	}
}

static void tdm_set_data_mode(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;
	int shift = 12;
	int mask;
	unsigned long reg;

	pr_info("%s\n", __func__);
	if (tdm->set_reg_rtx == SET_TX_REG) {
		mask = TDM_TX_CFG0_DATA_MODE(0x3);
		reg = TDM_REG(tdm, TDM_REG_TX_CFG0);
		tdm_reg_update(reg,
			config->trx_data_mode << shift, mask);
	} else {
		mask = TDM_RX_CFG0_DATA_MODE(0x3);
		reg = TDM_REG(tdm, TDM_REG_RX_CFG0);
		tdm_reg_update(reg,
			config->trx_data_mode << shift, mask);
	}
}

static void tdm_set_slot_width(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;
	int shift = 10;
	int mask;
	unsigned long reg;

	pr_info("%s\n", __func__);
	if (tdm->set_reg_rtx == SET_TX_REG) {
		mask = TDM_TX_CFG0_SLOT_WIDTH(0x3);
		reg = TDM_REG(tdm, TDM_REG_TX_CFG0);
		tdm_reg_update(reg,
			config->trx_slot_width << shift, mask);
	} else {
		mask = TDM_RX_CFG0_SLOT_WIDTH(0x3);
		reg = TDM_REG(tdm, TDM_REG_RX_CFG0);
		tdm_reg_update(reg,
			config->trx_slot_width << shift, mask);
	}
}

static void tdm_set_slot_num(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;
	int num_shift = 8;
	int num_mask;
	int valid_shift = 16;
	int valid_mask = 0xFF << valid_shift;
	unsigned long reg;

	pr_info("%s trx_slot_num : %d \n",
				__func__, config->trx_slot_num);
	if (tdm->set_reg_rtx == SET_TX_REG) {
		num_mask = TDM_TX_CFG0_SLOT_NUM(0x3);
		reg = TDM_REG(tdm, TDM_REG_TX_CFG0);
		tdm_reg_update(reg,
			config->trx_slot_num << num_shift, num_mask);
		if (config->trx_slot_num == TDM_2SLOT)
			tdm_reg_update(reg,
				0x3 << valid_shift, valid_mask);
		else if (config->trx_slot_num == TDM_4SLOT)
			tdm_reg_update(reg,
				0xf << valid_shift, valid_mask);
		else if (config->trx_slot_num == TDM_8SLOT)
			tdm_reg_update(reg,
				valid_mask, valid_mask);
	} else {
		num_mask = TDM_RX_CFG0_SLOT_NUM(0x3);
		reg = TDM_REG(tdm, TDM_REG_RX_CFG0);
		tdm_reg_update(reg,
			config->trx_slot_num << num_shift, num_mask);
		if (config->trx_slot_num == TDM_2SLOT)
			tdm_reg_update(reg,
				0x3 << valid_shift, valid_mask);
		else if (config->trx_slot_num == TDM_4SLOT)
			tdm_reg_update(reg,
				0xf << valid_shift, valid_mask);
		else if (config->trx_slot_num == TDM_8SLOT)
			tdm_reg_update(reg, valid_mask, valid_mask);
	}
}

static void tdm_set_sync_en(struct tdm_priv *tdm, int enable)
{
	int mask;
	unsigned long reg;

	pr_info("%s Enable = %d\n", __func__, enable);
	if (!enable) {
		if (tdm->set_reg_rtx == SET_TX_REG) {
			mask = TDM_TX_CFG0_SYNC_EN;
			reg = TDM_REG(tdm, TDM_REG_TX_CFG0);
			tdm_reg_update(reg, 0, mask);
		} else {
			mask = TDM_RX_CFG0_SYNC_EN;
			reg = TDM_REG(tdm, TDM_REG_RX_CFG0);
			tdm_reg_update(reg, 0, mask);
		}
	} else {
		if (tdm->set_reg_rtx == SET_TX_REG) {
			mask = TDM_TX_CFG0_SYNC_EN;
			reg = TDM_REG(tdm, TDM_REG_TX_CFG0);
			tdm_reg_update(reg, mask, mask);
		} else {
			mask = TDM_RX_CFG0_SYNC_EN;
			reg = TDM_REG(tdm, TDM_REG_RX_CFG0);
			tdm_reg_update(reg, mask, mask);
		}
	}
}

static void tdm_set_msb_mode(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;
	int mask;
	unsigned long reg;

	pr_info("%s\n", __func__);
	if (tdm->set_reg_rtx == SET_TX_REG) {
		mask = TDM_TX_CFG0_MSB_MODE;
		reg = TDM_REG(tdm, TDM_REG_TX_CFG0);
		tdm_reg_update(reg,
			(config->trx_msb_mode == TDM_MSB) ? mask : 0, mask);
	} else {
		mask = TDM_RX_CFG0_MSB_MODE;
		reg = TDM_REG(tdm, TDM_REG_RX_CFG0);
		tdm_reg_update(reg,
			(config->trx_msb_mode == TDM_MSB) ? mask : 0, mask);
	}
}

static void tdm_set_sync_mode(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;
	int mask;
	unsigned long reg;

	pr_info("%s\n", __func__);
	if (tdm->set_reg_rtx == SET_TX_REG) {
		mask = TDM_TX_CFG0_SYNC_MODE;
		reg = TDM_REG(tdm, TDM_REG_TX_CFG0);
		tdm_reg_update(reg,
			(config->trx_sync_mode == USE_LRCK_N_CAPTURE) ? mask : 0, mask);
	} else {
		mask = TDM_RX_CFG0_SYNC_MODE;
		reg = TDM_REG(tdm, TDM_REG_RX_CFG0);
		tdm_reg_update(reg,
			(config->trx_sync_mode == USE_LRCK_N_CAPTURE) ? mask : 0, mask);
	}
}

static void tdm_set_lrck_invert(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;
	int mask;
	unsigned long reg;

	pr_info("%s\n", __func__);
	if (tdm->set_reg_rtx == SET_TX_REG) {
		mask = TDM_TX_CFG0_LRCK_INVERT;
		reg = TDM_REG(tdm, TDM_REG_TX_CFG0);
		tdm_reg_update(reg,
			(config->tdm_lrck_inv == LRCK_INVERT_RJ_LJ) ? mask : 0, mask);
	} else {
		mask = TDM_RX_CFG0_LRCK_INVERT;
		reg = TDM_REG(tdm, TDM_REG_RX_CFG0);
		tdm_reg_update(reg,
			(config->tdm_lrck_inv == LRCK_INVERT_RJ_LJ) ? mask : 0, mask);
	}
}

static void tdm_set_pulse_mode(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;
	int shift = 1;
	int mask;
	unsigned long reg;

	pr_info("%s\n", __func__);
	if (tdm->set_reg_rtx == SET_TX_REG) {
		mask = TDM_TX_CFG0_PULSE_MODE(0x3);
		reg = TDM_REG(tdm, TDM_REG_TX_CFG0);
		tdm_reg_update(reg,
			config->trx_pulse_mode << shift, mask);
	} else {
		mask = TDM_RX_CFG0_PULSE_MODE(0x3);
		reg = TDM_REG(tdm, TDM_REG_RX_CFG0);
		tdm_reg_update(reg,
			config->trx_pulse_mode << shift, mask);
	}
}

static void tdm_set_mst_mode(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;
	int mask;
	unsigned long reg;

	pr_info("%s\n", __func__);
	if (tdm->set_reg_rtx == SET_TX_REG) {
		mask = TDM_TX_CFG0_MST_MODE;
		reg = TDM_REG(tdm, TDM_REG_TX_CFG0);
		tdm_reg_update(reg,
			(config->trx_mst_mode == TDM_MASTER) ? mask : 0, mask);
	} else {
		mask = TDM_RX_CFG0_MST_MODE;
		reg = TDM_REG(tdm, TDM_REG_RX_CFG0);
		tdm_reg_update(reg,
			(config->trx_mst_mode == TDM_MASTER) ? mask : 0, mask);
	}
}

static int tdm_calc_bclk_div_num(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;
	int val, slot_num, slot_width, tdm_fs;

	if (config->trx_slot_num == 0)
		slot_num = 2;
	else if (config->trx_slot_num == 1)
		slot_num = 4;
	else
		slot_num = 8;

	if (config->trx_slot_width == 0)
		slot_width = 16;
	else
		slot_width = 32;

	tdm_fs = config->tdm_fs / 1000;

	val = (49152 / (tdm_fs * slot_num * slot_width)) >> 1;
	--val;
	return val;
}

static void tdm_set_bclk_div_num(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;
	int mask;
	int val = 0;
	unsigned long reg;

	pr_info("%s\n", __func__);
	if (config->trx_mst_mode == TDM_MASTER) {
		if (tdm->set_reg_rtx == SET_TX_REG) {
			mask = TDM_TX_CFG1_BCK_DIV_NUM(0xffff);
			reg = TDM_REG(tdm, TDM_REG_TX_CFG1);
			val = tdm_calc_bclk_div_num(tdm);
			tdm_reg_update(reg, val, mask);
		} else {
			mask = TDM_RX_CFG1_BCK_DIV_NUM(0xffff);
			reg = TDM_REG(tdm, TDM_REG_RX_CFG1);
			val = tdm_calc_bclk_div_num(tdm);
			tdm_reg_update(reg, val, mask);
		}
	}
}

static int tdm_calc_lrclk_div_num(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;
	int val;

	if (config->trx_pulse_mode == 0)
		val = (49152 / (config->tdm_fs / 1000)) >> 1;
	else
		val = 49152 / (config->tdm_fs / 1000);
	--val;
	return val;
}

static void tdm_set_lrclk_div_num(struct tdm_priv *tdm)
{
	int shift = 16;
	int mask;
	int val = 0;
	unsigned long reg;

	pr_info("%s\n", __func__);
	if (tdm->set_reg_rtx == SET_TX_REG) {
		mask = TDM_TX_CFG1_LRCK_DIV_NUM(0xffff);
		reg = TDM_REG(tdm, TDM_REG_TX_CFG1);
		val = tdm_calc_lrclk_div_num(tdm);
		tdm_reg_update(reg, val << shift, mask);
	} else {
		mask = TDM_RX_CFG1_LRCK_DIV_NUM(0xffff);
		reg = TDM_REG(tdm, TDM_REG_RX_CFG1);
		val = tdm_calc_lrclk_div_num(tdm);
		tdm_reg_update(reg, val << shift, mask);
	}
}

static void tdm_set_rate_config(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;

	if (config->trx_mst_mode == TDM_MASTER) {
		tdm_set_lrclk_div_num(tdm);
		tdm_set_bclk_div_num(tdm);
	}
}

static void tdm_configs_set(struct tdm_priv *tdm)
{
	struct tdm_config *config = &tdm->config;

	pr_info("%s\n", __func__);

	tdm_set_threshold(tdm);
	tdm_set_mst_mode(tdm);
	tdm_set_data_width(tdm);
	tdm_set_data_mode(tdm);
	tdm_set_slot_width(tdm);
	tdm_set_slot_valid(tdm);
	tdm_set_slot_num(tdm);
	tdm_set_msb_mode(tdm);
	tdm_set_pulse_mode(tdm);
	tdm_set_lrck_invert(tdm);
	if (config->trx_mst_mode == TDM_SLAVE) {
		tdm_set_sync_en(tdm, 1);
		tdm_set_sync_mode(tdm);
	}
}

static void sprd_tdm_init_state(struct tdm_priv *tdm)
{
	pr_info("%s %d \n", __func__, __LINE__);

	tdm_set_duplex_mode(tdm);
	tdm_set_auto_gate_en(tdm, 1);
	tdm_set_dma_en(tdm, 0);
	tdm_set_int_en(tdm, 0);
	tdm_timeout_en(tdm, 0);
	tdm_bck_pos_dly(tdm);

	tdm_get_int_stat(tdm);
	tdm_clr_tx_int(tdm);
	tdm_clr_rx_int(tdm);
}

static int sprd_tdm_startup(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	struct tdm_priv *tdm = snd_soc_dai_get_drvdata(dai);

	pr_info("%s %d \n", __func__, __LINE__);

	tdm_module_en(tdm, true);
	tdm_clk_set_cfg(tdm);
	tdm_audcp_dvfs_set(tdm);
	tdm_aud_access_en(tdm);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		tdm->set_reg_rtx = SET_TX_REG;
	else
		tdm->set_reg_rtx = SET_RX_REG;

	sprd_tdm_init_state(tdm);
	tdm_configs_set(tdm);
	tdm_soft_reset(tdm);

	pr_info("%s %d \n", __func__, __LINE__);

	return 0;
}

static void sprd_tdm_shutdown(struct snd_pcm_substream *substream,
		       struct snd_soc_dai *dai)
{
	struct tdm_priv *tdm = snd_soc_dai_get_drvdata(dai);
	pr_info("%s %d \n", __func__, __LINE__);

	tdm_module_en(tdm, false);
}

static int sprd_tdm_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params,
			 struct snd_soc_dai *dai)
{
	struct sprd_pcm_dma_params *dma_data;
	struct tdm_priv *tdm = snd_soc_dai_get_drvdata(dai);
	pr_info("%s %d \n", __func__, __LINE__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dma_data = &tdm_pcm_stereo_out;
		dma_data->channels[0] = DMA_REQ_TDM_TX;
		dma_data->used_dma_channel_name[0] = use_dma_name[0];
		dma_data->desc.fragmens_len = TDM_FIFO_DEPTH -
					tdm->config.tx_watermark;
	} else {
		dma_data = &tdm_pcm_stereo_in;
		dma_data->channels[0] = DMA_REQ_TDM_RX;
		dma_data->used_dma_channel_name[0] = use_dma_name[1];
		dma_data->desc.fragmens_len = tdm->config.rx_watermark;
	}

	dma_data->desc.datawidth = tdm_get_dma_data_width(tdm);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data->desc.src_step = tdm_get_dma_step(tdm);
	else
		dma_data->desc.des_step = tdm_get_dma_step(tdm);

	pr_info(
		"format %d, desc.datawidth %d, desc.src_step %d, desc.des_step %d\n",
		params_format(params), dma_data->desc.datawidth,
		dma_data->desc.src_step, dma_data->desc.des_step);

	dma_data->dev_paddr[0] = TDM_PHY_REG(tdm, TDM_REG_RX_FIFO);
	pr_info("dma_data->dev_paddr[0]: 0x %d ", dma_data->dev_paddr[0]);

	snd_soc_dai_set_dma_data(dai, substream, dma_data);

	tdm_set_rate_config(tdm);

	return 0;
}

static int sprd_tdm_trigger(struct snd_pcm_substream *substream, int cmd,
		       struct snd_soc_dai *dai)
{
	struct tdm_priv *tdm = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	pr_info("%s %d \n", __func__, __LINE__);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		tdm_io_en(tdm, 1);
		udelay(10);
		tdm_controller_en(tdm, 1);
		tdm_set_dma_en(tdm, 1);
		tdm_debug_reg_read(tdm);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		tdm_set_dma_en(tdm, 0);
		tdm_controller_en(tdm, 0);
		udelay(10);
		tdm_io_en(tdm, 0);
		tdm_debug_reg_read(tdm);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct snd_soc_dai_ops sprd_tdm_dai_ops = {
	.startup	= sprd_tdm_startup,
	.hw_params	= sprd_tdm_hw_params,
	.trigger	= sprd_tdm_trigger,
	.shutdown	= sprd_tdm_shutdown,
};

struct snd_soc_dai_driver sprd_tdm_dai[] = {
	{
		.id = TDM_MAGIC_ID,
		.name	= "tdm-dai",
		.playback   = {
			.channels_min	= 1,
			.channels_max	= 2,
			.rates		= SNDRV_PCM_RATE_CONTINUOUS,
			.rate_min = 8000,
			.rate_max	= 192000,
			.formats	= SPRD_TDM_FMTBIT,
		},
		.capture = {
			.channels_min	= 1,
			.channels_max	= 2,
			.rates = SNDRV_PCM_RATE_CONTINUOUS,
			.rate_min = 8000,
			.rate_max = 96000,
			.formats	= SPRD_TDM_FMTBIT,
		},
		.ops = &sprd_tdm_dai_ops,
	}
};

static const struct snd_soc_component_driver sprd_tdm_component = {
	.name			= "sprd-tdm",
};

static void tdm_config_setting(int index, int value, struct tdm_config *config)
{
	pr_debug("%s tdm index %d,value %d\n", __func__, index, value);
	if (!config) {
		pr_err("%s config is NULL,error!\n", __func__);
		return;
	}
	switch (index) {
	case TDM_FS:
		if (value >= SAMPLATE_MIN && value <= SAMPLATE_MAX)
			config->tdm_fs = value;
		break;
	case MST_MODE:
		if (value == TDM_MASTER || value == TDM_SLAVE)
			config->trx_mst_mode = value;
		break;
	case DUPLEX_MODE:
		if (value == TDM_FULL_DUPLEX ||
		    value == TDM_HALF_DUPLEX_RX ||
			value == TDM_HALF_DUPLEX_TX)
			config->duplex_mode = value;
		break;
	case TRX_THRESHOLD:
		config->trx_threshold = value;
		break;
	case TRX_DATA_WIDTH:
		if (value == TDM_16BIT ||
		    value == TDM_24BIT ||
			value == TDM_32BIT)
			config->trx_data_width = value;
		break;
	case TRX_DATA_MODE:
		if (value == TDM_I2S_COMPATIBLE ||
		    value == TDM_LEFT_JUSTIFILED ||
			value == TDM_RIGHT_JUSTIFILED)
			config->trx_data_mode = value;
		break;
	case TRX_SLOT_WIDTH:
		if (value == TDM_16BIT ||
		    value == TDM_24BIT ||
			value == TDM_32BIT)
			config->trx_slot_width = value;
		break;
	case TRX_SLOT_NUM:
		if (value == TDM_2SLOT ||
		    value == TDM_4SLOT ||
			value == TDM_8SLOT)
			config->trx_slot_num = value;
		break;
	case TRX_MSB_MODE:
		if (value == TDM_LSB || value == TDM_MSB)
			config->trx_msb_mode = value;
		break;
	case TRX_PULSE_MODE:
		if (value == TDM_LRCK ||
		    value == TDM_BCK_PLUSE ||
			value == TDM_SLOT_PLUSE)
			config->trx_pulse_mode = value;
		break;
	case TDM_SLOT_VALID:
		config->tdm_slot_valid = value;
		break;
	case TRX_SYNC_MODE:
		if (value == USE_LRCK_P_CAPTURE || value == USE_LRCK_N_CAPTURE)
			config->trx_sync_mode = value;
		break;
	case TDM_LRCK_INV:
	if (value == TDM_MSBJUSTFIED || value == TDM_COMPATIBLE)
			config->tdm_lrck_inv = value;
		break;
	case TX_BCK_POS_DLY:
			config->tx_bck_pos_dly = value;
		break;
	case RX_BCK_POS_DLY:
			config->rx_bck_pos_dly = value;
		break;
	default:
		pr_err("tdm echo cmd is invalid\n");
		break;
	}
}

static int tdm_config_getting(int index, struct tdm_config *config)
{
	pr_debug("%s tdm index %d\n", __func__, index);
	if (!config) {
		pr_err("%s config is NULL,error\n", __func__);
		return -1;
	}
	switch (index) {
	case TDM_FS:
		return config->tdm_fs;
	case MST_MODE:
		return config->trx_mst_mode;
	case DUPLEX_MODE:
		return config->duplex_mode;
	case TRX_THRESHOLD:
		return config->trx_threshold;
	case TRX_DATA_WIDTH:
		return config->trx_data_width;
	case TRX_DATA_MODE:
		return config->trx_data_mode;
	case TRX_SLOT_WIDTH:
		return config->trx_slot_width;
	case TRX_SLOT_NUM:
		return config->trx_slot_num;
	case TRX_MSB_MODE:
		return config->trx_msb_mode;
	case TRX_PULSE_MODE:
		return config->trx_pulse_mode;
	case TDM_SLOT_VALID:
		return config->tdm_slot_valid;
	case TRX_SYNC_MODE:
		return config->trx_sync_mode;
	case TDM_LRCK_INV:
		return config->tdm_lrck_inv;
	case TX_BCK_POS_DLY:
		return config->tx_bck_pos_dly;
	case RX_BCK_POS_DLY:
		return config->rx_bck_pos_dly;
	default:
		pr_err("tdm config index is invalid\n");
	return -1;
	}
}

int tdm_config_get(struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	if (!dup_config) {
		pr_err("%s return\n", __func__);
		return 0;
	}
	ucontrol->value.integer.value[0] =
					tdm_config_getting(id, dup_config);
	pr_debug("%s return value %ld,id %d\n", __func__,
		 ucontrol->value.integer.value[0], id);
	return 0;
}
EXPORT_SYMBOL(tdm_config_get);

int tdm_config_set(struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int id = FUN_REG(mc->reg);

	if (!dup_config) {
		pr_err("%s return\n", __func__);
		return 0;
	}
	tdm_config_setting(id,
			ucontrol->value.integer.value[0], dup_config);
	return 0;
}
EXPORT_SYMBOL(tdm_config_set);

static int tdm_drv_probe(struct platform_device *pdev)
{
	struct tdm_priv *tdm;
	struct resource *res;
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	u32 val[2];
	int ret = 0;

	pr_info("%s\n", __func__);

	if (!node) {
		pr_err("ERR: %s, node is NULL!\n", __func__);
		return -ENODEV;
	}

	tdm = devm_kzalloc(&pdev->dev,
				sizeof(struct tdm_priv), GFP_KERNEL);
	if (!tdm)
		return -ENOMEM;

	tdm->dev = dev;

	if (!arch_audio_get_agcp_ahb_gpr()) {
		ap_ahb_gpr = syscon_regmap_lookup_by_phandle(
			pdev->dev.of_node, "sprd,syscon-agcp-ahb");
		if (IS_ERR(ap_ahb_gpr)) {
			pr_err("ERR: Get the tdm ap apb syscon failed!\n");
			ap_ahb_gpr = NULL;
			goto out;
		}
		arch_audio_set_agcp_ahb_gpr(ap_ahb_gpr);
	}

	audcp_clk_rf = syscon_regmap_lookup_by_phandle(
		pdev->dev.of_node, "sprd,syscon-clk-rf");
	if (IS_ERR(audcp_clk_rf)) {
		pr_err("ERR: Get the tdm audcp clk rf syscon failed!\n");
		audcp_clk_rf = NULL;
		goto out;
	}

	audcp_dvfs_apb_rf = syscon_regmap_lookup_by_phandle(
		pdev->dev.of_node, "sprd,syscon-dvfs-apb");
	if (IS_ERR(audcp_dvfs_apb_rf)) {
		pr_err("ERR: Get the tdm audcp dvfs apb rf syscon failed!\n");
		audcp_dvfs_apb_rf = NULL;
		goto out;
	}

	aon_apb_rf = syscon_regmap_lookup_by_phandle(
		pdev->dev.of_node, "sprd,syscon-aon-apb");
	if (IS_ERR(aon_apb_rf)) {
		pr_err("ERR: Get the tdm aon apb rf syscon failed!\n");
		aon_apb_rf = NULL;
		goto out;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("ERR:Must give me the base address!\n");
		goto out;
	}

	tdm->memphys = (unsigned int *)res->start;
	tdm->reg_size = (unsigned int)resource_size(res);
	tdm->membase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(tdm->membase)) {
		pr_err("ERR:tdm reg address ioremap_nocache error !\n");
		return -EINVAL;
	}
	sp_asoc_pr_dbg("tdm->memphys[0x%p] tdm->membase:[0x%p]",
					tdm->memphys, tdm->membase);

	tdm->config = def_tdm_config;

	if (!of_property_read_u32
		(node, "sprd,duplex_mode", &val[0])) {
		tdm->config.duplex_mode = val[0];
		sp_asoc_pr_dbg("Change duplex_mode to %d!\n", val[0]);
	}

	if (!of_property_read_u32
		(node, "sprd,slave_timeout", &val[0])) {
		tdm->config.slave_timeout = val[0];
		sp_asoc_pr_dbg("Change slave_timeout to %d!\n", val[0]);
	}

	if (!of_property_read_u32
		(node, "sprd,tdm_fs", &val[0])) {
		tdm->config.tdm_fs = val[0];
		sp_asoc_pr_dbg("Change tdm_fs to %d!\n", val[0]);
	}

	if (!of_property_read_u32
		(node, "sprd,trx_mst_mode", &val[0])) {
		tdm->config.trx_mst_mode = val[0];
		sp_asoc_pr_dbg("Change trx_mst_mode to %d!\n", val[0]);
	}

	if (!of_property_read_u32
		(node, "sprd,trx_threshold", &val[0])) {
		tdm->config.trx_threshold = val[0];
		sp_asoc_pr_dbg("Change trx_threshold to %d!\n", val[0]);
	}

	if (!of_property_read_u32
		(node, "sprd,trx_data_width", &val[0])) {
		tdm->config.trx_data_width = val[0];
		sp_asoc_pr_dbg("Change trx_data_width to %d!\n", val[0]);
	}

	if (!of_property_read_u32
		(node, "sprd,trx_data_mode", &val[0])) {
		tdm->config.trx_data_mode = val[0];
		sp_asoc_pr_dbg("Change trx_data_mode to %d!\n", val[0]);
	}

	if (!of_property_read_u32
		(node, "sprd,trx_slot_width", &val[0])) {
		tdm->config.trx_slot_width = val[0];
		sp_asoc_pr_dbg("Change trx_slot_width to %d!\n", val[0]);
	}

	if (!of_property_read_u32
		(node, "sprd,trx_slot_num", &val[0])) {
		tdm->config.trx_slot_num = val[0];
		sp_asoc_pr_dbg("Change trx_slot_num to %d!\n", val[0]);
	}

	if (!of_property_read_u32
		(node, "sprd,trx_msb_mode", &val[0])) {
		tdm->config.trx_msb_mode = val[0];
		sp_asoc_pr_dbg("Change trx_msb_mode to %d!\n", val[0]);
	}

	if (!of_property_read_u32
		(node, "sprd,trx_pulse_mode", &val[0])) {
		tdm->config.trx_pulse_mode = val[0];
		sp_asoc_pr_dbg("Change trx_pulse_mode to %d!\n", val[0]);
	}

	if (!of_property_read_u32
		(node, "sprd,tdm_slot_valid", &val[0])) {
		tdm->config.tdm_slot_valid = val[0];
		sp_asoc_pr_dbg("Change tdm_slot_valid to %d!\n", val[0]);
	}

	if (!of_property_read_u32
		(node, "sprd,trx_sync_mode", &val[0])) {
		tdm->config.trx_sync_mode = val[0];
		sp_asoc_pr_dbg("Change trx_sync_mode to %d!\n", val[0]);
	}

	platform_set_drvdata(pdev, tdm);

	atomic_set(&tdm->open_cnt, 0);

	ret = snd_soc_register_component(&pdev->dev,
				 &sprd_tdm_component,
				sprd_tdm_dai,
				ARRAY_SIZE(sprd_tdm_dai));
	if (ret) {
		dev_err(&pdev->dev, "Register DAI failed: %d\n", ret);
		return ret;
	}
	dup_config = &tdm->config;

	return ret;
out:
	return -EINVAL;
}

static int tdm_drv_remove(struct platform_device *pdev)
{
	struct tdm_priv *tdm;

	tdm = platform_get_drvdata(pdev);

	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tdm_of_match[] = {
	{.compatible = "unisoc,tdm",},
	{},
};

MODULE_DEVICE_TABLE(of, tdm_of_match);
#endif

static struct platform_driver tdm_driver = {
	.driver = {
		.name = "tdm",
		.owner = THIS_MODULE,
		.of_match_table = tdm_of_match,
	},

    .probe = tdm_drv_probe,
    .remove = tdm_drv_remove,
};
module_platform_driver(tdm_driver);

MODULE_DESCRIPTION("SPRD ASoC TDM CUP-DAI driver");
MODULE_DESCRIPTION("TDM DAI driver");
MODULE_LICENSE("GPL v2");
