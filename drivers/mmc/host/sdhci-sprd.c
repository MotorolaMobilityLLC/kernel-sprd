// SPDX-License-Identifier: GPL-2.0
//
// Secure Digital Host Controller
//
// Copyright (C) 2018 Spreadtrum, Inc.
// Author: Chunyan Zhang <chunyan.zhang@unisoc.com>

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <trace/hooks/mmc.h>

#include "sdhci-pltfm.h"
#include "mmc_hsq.h"

#include "mmc_swcq.h"
#include "sdhci-sprd-swcq.h"
#include "sdhci-sprd-swcq.c"

#include "cqhci.h"
#include <trace/hooks/mmc.h>

#include "sdhci-sprd-powp.h"
#include "sdhci-sprd-powp.c"

#include "sdhci-sprd-tuning.h"
#include "sdhci-sprd-tuning.c"

#include "sdhci-sprd-ffu.h"
#include "sdhci-sprd-ffu.c"

#include "sdhci-sprd-health.h"
#include "sdhci-sprd-health.c"

#ifdef CONFIG_SPRD_DEBUG
#include "sdhci-sprd-debugfs.h"
#include "sdhci-sprd-debugfs.c"
#endif

#define DRIVER_NAME "sprd-sdhci"
#define SDHCI_SPRD_DUMP(f, x...) \
	pr_err("%s: " DRIVER_NAME ": " f, mmc_hostname(host->mmc), ## x)

#define HOST_IS_SD_TYPE(c) (((c)->caps2 & (MMC_CAP2_NO_MMC | MMC_CAP2_NO_SDIO)) \
					== (MMC_CAP2_NO_MMC | MMC_CAP2_NO_SDIO))

#define SEND_SD_SWITCH	6

/* SDHCI_ARGUMENT2 register high 16bit */
#define SDHCI_SPRD_ARG2_STUFF		GENMASK(31, 16)

#define SDHCI_SPRD_REG_32_DLL_CFG	0x200
#define  SDHCI_SPRD_DLL_ALL_CPST_EN	(BIT(18) | BIT(24) | BIT(25) | BIT(26) | BIT(27))
#define  SDHCI_SPRD_DLL_EN		BIT(21)
#define  SDHCI_SPRD_DLL_SEARCH_MODE	BIT(16)
#define  SDHCI_SPRD_DLL_INIT_COUNT	0xc00
#ifdef CONFIG_MMC_SPRD_SDHCR11P3
#define  SDHCI_SPRD_DLL_PHASE_INTERNAL	0x2
#else
#define SDHCI_SPRD_DLL_PHASE_INTERNAL	0x3
#endif

#define SDHCI_SPRD_REG_32_DLL_DLY	0x204

#define SDHCI_SPRD_REG_32_DLL_DLY_OFFSET	0x208
#define  SDHCIBSPRD_IT_WR_DLY_INV		BIT(5)
#define  SDHCI_SPRD_BIT_CMD_DLY_INV		BIT(13)
#define  SDHCI_SPRD_BIT_POSRD_DLY_INV		BIT(21)
#define  SDHCI_SPRD_BIT_NEGRD_DLY_INV		BIT(29)

#define SDHCI_SPRD_REG_32_BUSY_POSI		0x250
#define  SDHCI_SPRD_BIT_OUTR_CLK_AUTO_EN	BIT(25)
#define  SDHCI_SPRD_BIT_INNR_CLK_AUTO_EN	BIT(24)

#define SDHCI_SPRD_REG_DEBOUNCE		0x28C
#define  SDHCI_SPRD_BIT_DLL_BAK		BIT(0)
#define  SDHCI_SPRD_BIT_DLL_VAL		BIT(1)

#define  SDHCI_SPRD_INT_SIGNAL_MASK	0x1B7F410B

/* SDHCI_HOST_CONTROL2 */
#define  SDHCI_SPRD_CTRL_HS200		0x0005
#define  SDHCI_SPRD_CTRL_HS400		0x0006
#define  SDHCI_SPRD_CTRL_HS400ES	0x0007

/*
 * According to the standard specification, BIT(3) of SDHCI_SOFTWARE_RESET is
 * reserved, and only used on Spreadtrum's design, the hardware cannot work
 * if this bit is cleared.
 * 1 : normal work
 * 0 : hardware reset
 */
#define  SDHCI_HW_RESET_CARD		BIT(3)

#define SDHCI_SPRD_MAX_CUR		1020
#define SDHCI_SPRD_CLK_MAX_DIV		1023

#define SDHCI_SPRD_CLK_DEF_RATE		26000000
#define SDHCI_SPRD_PHY_DLL_CLK		52000000
/*
 * The following register is defined by spreadtrum self.
 * It is not standard register of SDIO
 */
#define SDHCI_SPRD_REG_32_DLL_STS0	0x210
#define SDHCI_SPRD_DLL_LOCKED		BIT(18)

#define SDHCI_SPRD_REG_32_DLL_STS1	0x214
#define SDHCI_SPRD_DLL_WAIT_CNT		0xC0000000


#define SDHCI_SPRD_REG_8_DATWR_DLY	0x204
#define SDHCI_SPRD_REG_8_CMDRD_DLY	0x205
#define SDHCI_SPRD_REG_8_POSRD_DLY	0x206
#define SDHCI_SPRD_REG_8_NEGRD_DLY	0x207


#define SDHCI_SPRD_WR_DLY_MASK		0xff
#define SDHCI_SPRD_CMD_DLY_MASK		(0xff << 8)
#define SDHCI_SPRD_POSRD_DLY_MASK		(0xff << 16)
#define SDHCI_SPRD_NEGRD_DLY_MASK		(0xff << 24)
#define SDHCI_SPRD_DLY_TIMING(wr_dly, cmd_dly, posrd_dly, negrd_dly) \
		((wr_dly) | ((cmd_dly) << 8) | \
		((posrd_dly) << 16) | ((negrd_dly) << 24))

#define SDHCI_INT_CMD_ERR_MASK (SDHCI_INT_TIMEOUT | SDHCI_INT_CRC | \
		SDHCI_INT_END_BIT | SDHCI_INT_INDEX)

struct ranges_t {
	int start;
	int end;
};

struct register_hotplug {
	struct regmap *regmap;
	u32 reg;
	u32 mask;
};

struct sdhci_sprd_host {
	struct platform_device *pdev;
	u32 version;
	struct clk *clk_sdio;
	struct clk *clk_enable;
	struct clk *clk_2x_enable;
	struct clk *clk_1x_enable;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_uhs;
	struct pinctrl_state *pins_default;
	u32 base_rate;
	int flags; /* backup of host attribute */
	u32 phy_delay[MMC_TIMING_MMC_HS400 + 2];
	u32 dll_cnt;
	u32 mid_dll_cnt;
	struct ranges_t *ranges;
	int detect_gpio;
	bool detect_gpio_polar;
	struct register_hotplug reg_detect_polar;
	struct register_hotplug reg_protect_enable;
	struct register_hotplug reg_debounce_en;
	struct register_hotplug reg_debounce_cn;
	struct register_hotplug reg_rmldo_en;
	unsigned char	power_mode;
	bool support_swcq;
	bool support_cqe;
	bool support_ice;
	void __iomem *cqe_mem;	/* SPRD CQE mapped address (if available) */
	void __iomem *ice_mem;	/* SPRD ICE mapped address (if available) */
	u32 tuning_flag;
	u32 cpst_cmd_dly;
	u32 int_status;
	bool cmd_dly_all_pass;
};

enum sdhci_sprd_tuning_type {
	SDHCI_SPRD_TUNING_DEFAULT,
	SDHCI_SPRD_TUNING_SD_HS,
	SDHCI_SPRD_TUNING_SD_UHS_CMD,
	SDHCI_SPRD_TUNING_SD_UHS_DATA,
};

struct sdhci_sprd_phy_cfg {
	const char *property;
	u8 timing;
};

static const struct sdhci_sprd_phy_cfg sdhci_sprd_phy_cfgs[] = {
	{ "sprd,phy-delay-legacy", MMC_TIMING_LEGACY, },
	{ "sprd,phy-delay-sd-highspeed", MMC_TIMING_SD_HS, },
	{ "sprd,phy-delay-sd-uhs-sdr50", MMC_TIMING_UHS_SDR50, },
	{ "sprd,phy-delay-sd-uhs-sdr104", MMC_TIMING_UHS_SDR104, },
	{ "sprd,phy-delay-mmc-highspeed", MMC_TIMING_MMC_HS, },
	{ "sprd,phy-delay-mmc-ddr52", MMC_TIMING_MMC_DDR52, },
	{ "sprd,phy-delay-mmc-hs200", MMC_TIMING_MMC_HS200, },
	{ "sprd,phy-delay-mmc-hs400", MMC_TIMING_MMC_HS400, },
	{ "sprd,phy-delay-mmc-hs400es", MMC_TIMING_MMC_HS400 + 1, },
};

#define TO_SPRD_HOST(host) sdhci_pltfm_priv(sdhci_priv(host))

static void sdhci_sprd_health_and_powp(void *data, struct mmc_card *card)
{
	int err;

	/* mmc health init */
	err = sprd_mmc_health_init(card);
	if (err)
		pr_err("sprd_mmc_health_init: function error = %d\n", err);
	/* mmc powp handle */
	if (!mmc_check_wp_fn(card->host))
		mmc_set_powp(card);
}

static void sdhci_sprd_init_config(struct sdhci_host *host)
{
	u16 val;

	/* set dll backup mode */
	val = sdhci_readl(host, SDHCI_SPRD_REG_DEBOUNCE);
	val |= SDHCI_SPRD_BIT_DLL_BAK | SDHCI_SPRD_BIT_DLL_VAL;
	sdhci_writel(host, val, SDHCI_SPRD_REG_DEBOUNCE);
}

static inline u32 sdhci_sprd_readl(struct sdhci_host *host, int reg)
{
	u32 sts = 0;
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	if (unlikely(reg == SDHCI_MAX_CURRENT))
		return SDHCI_SPRD_MAX_CUR;

	sts = readl_relaxed(host->ioaddr + reg);

	if (likely(reg == SDHCI_INT_STATUS) && sprd_host->support_cqe &&
	    (readl(sprd_host->cqe_mem + CQHCI_IS) &
	    readl(sprd_host->cqe_mem + CQHCI_ISTE) &
	    readl(sprd_host->cqe_mem + CQHCI_ISGE)))
		sts |= SDHCI_INT_CQE;

	return sts;
}

static inline void sdhci_sprd_writel(struct sdhci_host *host, u32 val, int reg)
{
	/* SDHCI_MAX_CURRENT is reserved on Spreadtrum's platform */
	if (unlikely(reg == SDHCI_MAX_CURRENT))
		return;

	if (unlikely(reg == SDHCI_SIGNAL_ENABLE || reg == SDHCI_INT_ENABLE))
		val = val & SDHCI_SPRD_INT_SIGNAL_MASK;

	writel_relaxed(val, host->ioaddr + reg);
}

static inline void sdhci_sprd_writew(struct sdhci_host *host, u16 val, int reg)
{
	u16 mask = 0;

	/* SDHCI_BLOCK_COUNT is Read Only on Spreadtrum's platform */
	if (unlikely(reg == SDHCI_BLOCK_COUNT))
		return;

	if ((strcmp(mmc_hostname(host->mmc), "mmc1") == 0) &&
		(reg == SDHCI_COMMAND) &&
		(host->cmd->opcode == SEND_SD_SWITCH) &&
		(host->clock <= 400000) &&
		(host->cmd->arg == 0x80FFFFF1))
		val &= ~(mask | SDHCI_CMD_CRC);
	writew(val, host->ioaddr + reg);
}

static inline void sdhci_sprd_writeb(struct sdhci_host *host, u8 val, int reg)
{
	if (unlikely(reg == SDHCI_SOFTWARE_RESET)) {
		/*
		 * Since BIT(3) of SDHCI_SOFTWARE_RESET is reserved according to the
		 * standard specification, sdhci_reset() write this register directly
		 * without checking other reserved bits, that will clear BIT(3) which
		 * is defined as hardware reset on Spreadtrum's platform and clearing
		 * it by mistake will lead the card not work. So here we need to work
		 * around it.
		 */
		if (readb_relaxed(host->ioaddr + reg) & SDHCI_HW_RESET_CARD)
			val |= SDHCI_HW_RESET_CARD;
	}

	writeb_relaxed(val, host->ioaddr + reg);
}

static inline void sdhci_sprd_sd_clk_off(struct sdhci_host *host)
{
	u16 ctrl = sdhci_readw(host, SDHCI_CLOCK_CONTROL);

	ctrl &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, ctrl, SDHCI_CLOCK_CONTROL);
}

static inline void sdhci_sprd_sd_clk_on(struct sdhci_host *host)
{
	u16 ctrl;

	ctrl = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	ctrl |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, ctrl, SDHCI_CLOCK_CONTROL);
}

static inline void
sdhci_sprd_set_dll_invert(struct sdhci_host *host, u32 mask, bool en)
{
	u32 dll_dly_offset;

	dll_dly_offset = sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_DLY_OFFSET);
	if (en)
		dll_dly_offset |= mask;
	else
		dll_dly_offset &= ~mask;
	sdhci_writel(host, dll_dly_offset, SDHCI_SPRD_REG_32_DLL_DLY_OFFSET);
}

static inline u32 sdhci_sprd_calc_div(u32 base_clk, u32 clk)
{
	u32 div;

	/* select 2x clock source */
	if (base_clk <= clk * 2)
		return 0;

	div = (u32) (base_clk / (clk * 2));

	if ((base_clk / div) > (clk * 2))
		div++;

	if (div % 2)
		div = (div + 1) / 2;
	else
		div = div / 2;

	if (div > SDHCI_SPRD_CLK_MAX_DIV)
		div = SDHCI_SPRD_CLK_MAX_DIV;

	return div;
}

static inline void _sdhci_sprd_set_clock(struct sdhci_host *host,
					unsigned int clk)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	u32 div, val, mask;

	sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);

	div = sdhci_sprd_calc_div(sprd_host->base_rate, clk);
	div = ((div & 0x300) >> 2) | ((div & 0xFF) << 8);

	sdhci_enable_clk(host, div);

	/* enable auto gate sdhc_enable_auto_gate */
	if (clk > 400000) {
		val = sdhci_readl(host, SDHCI_SPRD_REG_32_BUSY_POSI);
		mask = SDHCI_SPRD_BIT_OUTR_CLK_AUTO_EN |
			SDHCI_SPRD_BIT_INNR_CLK_AUTO_EN;
		if (mask != (val & mask)) {
			val |= mask;
			sdhci_writel(host, val, SDHCI_SPRD_REG_32_BUSY_POSI);
		}
	} else {
		val = sdhci_readl(host, SDHCI_SPRD_REG_32_BUSY_POSI);
		mask = SDHCI_SPRD_BIT_OUTR_CLK_AUTO_EN |
			SDHCI_SPRD_BIT_INNR_CLK_AUTO_EN;
		if (val & mask) {
			val &= ~mask;
			sdhci_writel(host, val, SDHCI_SPRD_REG_32_BUSY_POSI);
		}
	}
}

static void sdhci_sprd_enable_phy_dll(struct sdhci_host *host)
{
	u32 tmp;

	tmp = sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_CFG);
	tmp &= ~(SDHCI_SPRD_DLL_EN | SDHCI_SPRD_DLL_ALL_CPST_EN);
	sdhci_writel(host, tmp, SDHCI_SPRD_REG_32_DLL_CFG);
	/* wait 1ms */
	usleep_range_state(1000, 1250, TASK_UNINTERRUPTIBLE);

	tmp = sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_CFG);
	tmp |= SDHCI_SPRD_DLL_ALL_CPST_EN | SDHCI_SPRD_DLL_SEARCH_MODE |
		SDHCI_SPRD_DLL_INIT_COUNT | SDHCI_SPRD_DLL_PHASE_INTERNAL;
	sdhci_writel(host, tmp, SDHCI_SPRD_REG_32_DLL_CFG);
	/* wait 1ms */
	usleep_range_state(1000, 1250, TASK_UNINTERRUPTIBLE);

	tmp = sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_CFG);
	tmp |= SDHCI_SPRD_DLL_EN;
	sdhci_writel(host, tmp, SDHCI_SPRD_REG_32_DLL_CFG);
	/* wait 1ms */
	usleep_range_state(1000, 1250, TASK_UNINTERRUPTIBLE);

	if (read_poll_timeout(sdhci_readl, tmp, (tmp & SDHCI_SPRD_DLL_LOCKED),
		2000, USEC_PER_SEC, false, host, SDHCI_SPRD_REG_32_DLL_STS0)) {
		pr_err("%s: DLL locked fail!\n", mmc_hostname(host->mmc));
		pr_info("%s: DLL_STS0 : 0x%x, DLL_CFG : 0x%x\n",
			 mmc_hostname(host->mmc),
			 sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_STS0),
			 sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_CFG));
	}
}

static bool sdhci_sprd_check_invert(struct sdhci_host *host)
{
	if ((host->mmc->index == 0) && (host->timing == MMC_TIMING_MMC_HS))
		return true;

	return false;
}

static void sdhci_sprd_set_clock(struct sdhci_host *host, unsigned int clock)
{
	bool en = false, clk_changed = false;
	u16 clk;

	if (clock == 0) {
		sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);
	} else if (clock != host->clock) {
		sdhci_sprd_sd_clk_off(host);
		_sdhci_sprd_set_clock(host, clock);

		if (clock <= 400000 || sdhci_sprd_check_invert(host))
			en = true;
		sdhci_sprd_set_dll_invert(host, SDHCI_SPRD_BIT_CMD_DLY_INV |
					  SDHCI_SPRD_BIT_POSRD_DLY_INV, en);
		clk_changed = true;
	} else {
		clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
		clk |= SDHCI_CLOCK_CARD_EN;
		sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
	}

	/*
	 * According to the Spreadtrum SD host specification, when we changed
	 * the clock to be more than 52M, we should enable the PHY DLL which
	 * is used to track the clock frequency to make the clock work more
	 * stable. Otherwise deviation may occur of the higher clock.
	 */
	if (clk_changed && clock > SDHCI_SPRD_PHY_DLL_CLK)
		sdhci_sprd_enable_phy_dll(host);
}

static unsigned int sdhci_sprd_get_max_clock(struct sdhci_host *host)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	return clk_round_rate(sprd_host->clk_sdio, ULONG_MAX);
}

static unsigned int sdhci_sprd_get_min_clock(struct sdhci_host *host)
{
	return 100000;
}

static void sdhci_sprd_set_uhs_signaling(struct sdhci_host *host,
					 unsigned int timing)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	struct mmc_host *mmc = host->mmc;
	u32 *p = sprd_host->phy_delay;
	u16 ctrl_2;
	bool en = false;

	ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	/* Select Bus Speed Mode for host */
	ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;
	switch (timing) {
	case MMC_TIMING_UHS_SDR12:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR12;
		break;
	case MMC_TIMING_MMC_HS:
	case MMC_TIMING_SD_HS:
	case MMC_TIMING_UHS_SDR25:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR25;
		break;
	case MMC_TIMING_UHS_SDR50:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR50;
		break;
	case MMC_TIMING_UHS_SDR104:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR104;
		break;
	case MMC_TIMING_UHS_DDR50:
	case MMC_TIMING_MMC_DDR52:
		ctrl_2 |= SDHCI_CTRL_UHS_DDR50;
		break;
	case MMC_TIMING_MMC_HS200:
		ctrl_2 |= SDHCI_SPRD_CTRL_HS200;
		break;
	case MMC_TIMING_MMC_HS400:
		if (mmc->ios.enhanced_strobe)
			ctrl_2 |= SDHCI_SPRD_CTRL_HS400ES;
		else
			ctrl_2 |= SDHCI_SPRD_CTRL_HS400;
		break;
	default:
		break;
	}

	sdhci_writew(host, ctrl_2, SDHCI_HOST_CONTROL2);

	if (timing == MMC_TIMING_SD_HS || timing == MMC_TIMING_MMC_HS ||
			timing == MMC_TIMING_LEGACY)
		en = true;

	sdhci_sprd_set_dll_invert(host, SDHCI_SPRD_BIT_CMD_DLY_INV |
		SDHCI_SPRD_BIT_POSRD_DLY_INV, en);

	if (!mmc->ios.enhanced_strobe)
		sdhci_writel(host, p[timing], SDHCI_SPRD_REG_32_DLL_DLY);
	else
		sdhci_writel(host, p[MMC_TIMING_MMC_HS400 + 1], SDHCI_SPRD_REG_32_DLL_DLY);
}

static void sdhci_sprd_hw_reset(struct sdhci_host *host)
{
	int val;

	/*
	 * Note: don't use sdhci_writeb() API here since it is redirected to
	 * sdhci_sprd_writeb() in which we have a workaround for
	 * SDHCI_SOFTWARE_RESET which would make bit SDHCI_HW_RESET_CARD can
	 * not be cleared.
	 */
	val = readb_relaxed(host->ioaddr + SDHCI_SOFTWARE_RESET);
	val &= ~SDHCI_HW_RESET_CARD;
	writeb_relaxed(val, host->ioaddr + SDHCI_SOFTWARE_RESET);
	/* wait for 10 us */
	usleep_range_state(10, 20, TASK_UNINTERRUPTIBLE);

	val |= SDHCI_HW_RESET_CARD;
	writeb_relaxed(val, host->ioaddr + SDHCI_SOFTWARE_RESET);
	usleep_range_state(300, 500, TASK_UNINTERRUPTIBLE);
}

static unsigned int sdhci_sprd_get_max_timeout_count(struct sdhci_host *host)
{
	/* The Spredtrum controller actual maximum timeout count is 1 << 31 */
	return 1 << 31;
}

static unsigned int sdhci_sprd_get_ro(struct sdhci_host *host)
{
	return 0;
}

static void sdhci_sprd_request_done(struct sdhci_host *host,
				    struct mmc_request *mrq)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	/* Validate if the request was from software queue firstly. */
	if (HOST_IS_EMMC_TYPE(host->mmc) && sprd_host->support_swcq) {
		if (mmc_swcq_finalize_request(host->mmc, mrq))
			return;
	} else if (sprd_host->support_cqe) {
		mmc_request_done(host->mmc, mrq);
			return;
	} else {
		if (mmc_hsq_finalize_request(host->mmc, mrq))
			return;
	}

	mmc_request_done(host->mmc, mrq);
}

static void sdhci_sprd_set_dll_value(struct sdhci_host *host, u32 *delay_val, u32 opcode, u32 val,
					enum sdhci_sprd_tuning_type type)
{
	switch (type) {
	case SDHCI_SPRD_TUNING_SD_HS:
		if (opcode == MMC_SET_BLOCKLEN) {
			*delay_val &= ~(SDHCI_SPRD_CMD_DLY_MASK);
			*delay_val |= ((val << 8) & SDHCI_SPRD_CMD_DLY_MASK);
		} else {
			*delay_val &= ~(SDHCI_SPRD_POSRD_DLY_MASK);
			*delay_val |= ((val << 16) & SDHCI_SPRD_POSRD_DLY_MASK);
		}
		break;
	case SDHCI_SPRD_TUNING_SD_UHS_CMD:
		*delay_val &= ~(SDHCI_SPRD_CMD_DLY_MASK);
		*delay_val |= ((val << 8) & SDHCI_SPRD_CMD_DLY_MASK);
		break;
	case SDHCI_SPRD_TUNING_SD_UHS_DATA:
		*delay_val &= ~(SDHCI_SPRD_POSRD_DLY_MASK);
		*delay_val |= ((val << 16) & SDHCI_SPRD_POSRD_DLY_MASK);
		break;
	default:
		*delay_val &= ~(SDHCI_SPRD_CMD_DLY_MASK | SDHCI_SPRD_POSRD_DLY_MASK);
		*delay_val |= (((val << 8) & SDHCI_SPRD_CMD_DLY_MASK) |
			 ((val << 16) & SDHCI_SPRD_POSRD_DLY_MASK));
		break;
	}

	sdhci_writel(host, *delay_val, SDHCI_SPRD_REG_32_DLL_DLY);
	pr_debug("%s: dll_dly 0x%08x\n", mmc_hostname(host->mmc), *delay_val);
}

static int sprd_calc_hs_mode_tuning_range(struct sdhci_sprd_host *host, int *value_t)
{
	int i;
	bool prev_vl = 0;
	int range_count = 0;
	u32 dll_cnt = host->dll_cnt;
	u32 mid_dll_cnt = host->mid_dll_cnt;
	struct ranges_t *ranges = host->ranges;

	for (i = 0; i < mid_dll_cnt; i++) {
		if ((!prev_vl) && value_t[i] && value_t[i + dll_cnt] && value_t[i + mid_dll_cnt]) {
			range_count++;
			ranges[range_count - 1].start = i;
		}

		if (value_t[i] && value_t[i + dll_cnt] && value_t[i + mid_dll_cnt]) {
			ranges[range_count - 1].end = i;
			pr_debug("recalculate tuning ok: %d\n", i);
		} else
			pr_debug("recalculate tuning fail: %d\n", i);

		prev_vl = value_t[i] && value_t[i + dll_cnt] && value_t[i + mid_dll_cnt];
	}

	host->ranges = ranges;

	return range_count;
}

static int sprd_calc_tuning_range(struct sdhci_sprd_host *host, int *value_t)
{
	int i;
	bool prev_vl = 0;
	int range_count = 0;
	u32 dll_cnt = host->dll_cnt;
	u32 mid_dll_cnt = host->mid_dll_cnt;
	struct ranges_t *ranges = host->ranges;

	/*
	 * first: 0 <= i < mid_dll_cnt
	 * tuning range: (0 ~ mid_dll_cnt) && (dll_cnt ~ dll_cnt + mid_dll_cnt)
	 */
	for (i = 0; i < mid_dll_cnt; i++) {
		if ((!prev_vl) && value_t[i] && value_t[i + dll_cnt]) {
			range_count++;
			ranges[range_count - 1].start = i;
		}

		if (value_t[i] && value_t[i + dll_cnt]) {
			ranges[range_count - 1].end = i;
			pr_debug("recalculate tuning ok: %d\n", i);
		} else
			pr_debug("recalculate tuning fail: %d\n", i);

		prev_vl = value_t[i] && value_t[i + dll_cnt];
	}

	/*
	 * second: mid_dll_cnt <= i < dll_cnt
	 * tuning range: mid_dll_cnt ~ dll_cnt
	 */
	for (i = mid_dll_cnt; i < dll_cnt; i++) {
		if ((!prev_vl) && value_t[i]) {
			range_count++;
			ranges[range_count - 1].start = i;
		}

		if (value_t[i]) {
			ranges[range_count - 1].end = i;
			pr_debug("recalculate tuning ok: %d\n", i);
		} else
			pr_debug("recalculate tuning fail: %d\n", i);

		prev_vl = value_t[i];
	}

	host->ranges = ranges;

	return range_count;
}

static int sdhci_sprd_tuning(struct mmc_host *mmc, u32 opcode, enum sdhci_sprd_tuning_type type)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	u32 *p = sprd_host->phy_delay;
	int err = 0;
	int i = 0;
	bool value, first_vl, prev_vl = 0;
	int *value_t;
	struct ranges_t *ranges;
	int length;
	unsigned int range_count = 0;
	int longest_range_len = 0;
	int longest_range = 0;
	int mid_step;
	int final_phase;
	u32 dll_cfg, mid_dll_cnt, dll_cnt, dll_dly;
	bool cfg_use_adma = false;
	int cmd_error = 0;

	sdhci_reset(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA);

	dll_cfg = sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_CFG);
	dll_cfg &= ~(0xf << 24);
	sdhci_writel(host, dll_cfg, SDHCI_SPRD_REG_32_DLL_CFG);

	if (type == SDHCI_SPRD_TUNING_SD_HS)
		dll_cnt = 128;
	else
		dll_cnt = sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_STS0) & 0xff;
	dll_cnt = dll_cnt << 1;
	length = (dll_cnt * 150) / 100;
	pr_info("%s: dll config 0x%08x, dll count %d, tuning length: %d\n",
		mmc_hostname(mmc), dll_cfg, dll_cnt, length);

	ranges = kmalloc_array(length + 1, sizeof(*ranges), GFP_KERNEL);
	if (!ranges)
		return -ENOMEM;
	value_t = kmalloc_array(length + 1, sizeof(*value_t), GFP_KERNEL);
	if (!value_t) {
		kfree(ranges);
		return -ENOMEM;
	}

	dll_dly = p[mmc->ios.timing];
	if (type == SDHCI_SPRD_TUNING_SD_UHS_DATA) {
		/* if tuning uhs data, cmd delay value must in cpst_en disabled status */
		dll_dly &= ~SDHCI_SPRD_CMD_DLY_MASK;
		dll_dly |= (sprd_host->cpst_cmd_dly << 8);
		sprd_host->cpst_cmd_dly = 0;
	}

	/* config dma mode in tuning. */
	if (!cfg_use_adma && (host->flags & SDHCI_USE_ADMA) && strcmp(mmc_hostname(mmc), "mmc0")) {
		cfg_use_adma = true;
		host->flags &= ~SDHCI_USE_ADMA;
		host->flags |= SDHCI_USE_SDMA;
	}

	do {
		sdhci_sprd_set_dll_value(host, &dll_dly, opcode, i, type);

		sprd_host->tuning_flag = 1;
		if (type == SDHCI_SPRD_TUNING_SD_HS) {
			if (opcode == MMC_SET_BLOCKLEN)
				value = !mmc_send_tuning_cmd(mmc);
			else
				value = !mmc_send_tuning_read(mmc);
		} else if (type == SDHCI_SPRD_TUNING_SD_UHS_CMD) {
			value = !mmc_send_tuning_cmd(mmc);
		} else {
			value = !mmc_send_tuning(mmc, opcode, &cmd_error);
		}

		/*
		 * if data tuning occur cmd error and previous cmd tuning all pass,
		 * it means the current cmd delay is not a good one.
		 * then reset cmd delay to 0
		 */
		if ((type == SDHCI_SPRD_TUNING_SD_UHS_DATA) && cmd_error) {
			pr_info("%s: cmd error %d occurs in data tuning, dly = 0x%x\n",
				mmc_hostname(mmc), cmd_error, dll_dly);
			if (sprd_host->cmd_dly_all_pass == true) {
				dll_dly &= ~SDHCI_SPRD_CMD_DLY_MASK;
				sdhci_sprd_set_dll_value(host, &p[mmc->ios.timing],
					MMC_SET_BLOCKLEN, 0, SDHCI_SPRD_TUNING_SD_UHS_CMD);

				i--;
				cmd_error = 0;
				sprd_host->tuning_flag = 0;
				sprd_host->cmd_dly_all_pass = false;
				continue;
			}
		}

		sprd_host->tuning_flag = 0;

		if ((!prev_vl) && value) {
			range_count++;
			ranges[range_count - 1].start = i;
		}
		if (value) {
			pr_debug("%s tuning ok: %d\n", mmc_hostname(mmc), i);
			ranges[range_count - 1].end = i;
			value_t[i] = value;
		} else {
			pr_debug("%s tuning fail: %d\n", mmc_hostname(mmc), i);
			value_t[i] = value;
		}

		prev_vl = value;
	} while (++i <= length);

	mid_dll_cnt = length - dll_cnt;
	sprd_host->dll_cnt = dll_cnt;
	sprd_host->mid_dll_cnt = mid_dll_cnt;
	sprd_host->ranges = ranges;

	first_vl = (value_t[0] && value_t[dll_cnt]);
	if (type == SDHCI_SPRD_TUNING_SD_HS)
		range_count = sprd_calc_hs_mode_tuning_range(sprd_host, value_t);
	else
		range_count = sprd_calc_tuning_range(sprd_host, value_t);

	if (range_count == 0) {
		pr_warn("%s: all tuning phases fail!\n", mmc_hostname(mmc));
		err = -EIO;
		goto out;
	}
	if (type == SDHCI_SPRD_TUNING_SD_HS) {
		if ((range_count > 1) && (ranges[range_count - 1].end == 127)
				&& (ranges[0].start == 0)) {
			ranges[0].start = ranges[range_count - 1].start;
			range_count--;

			if (ranges[0].end >= mid_dll_cnt)
				ranges[0].end = mid_dll_cnt;
		}
	} else {
		if ((range_count > 1) && first_vl && value) {
			ranges[0].start = ranges[range_count - 1].start;
			range_count--;

			if (ranges[0].end >= mid_dll_cnt)
				ranges[0].end = mid_dll_cnt;
		}
	}

	for (i = 0; i < range_count; i++) {
		int len = (ranges[i].end - ranges[i].start + 1);

		if (len < 0) {
			if (type == SDHCI_SPRD_TUNING_SD_HS)
				len += mid_dll_cnt;
			else
				len += dll_cnt;
		}

		pr_debug("%s: good tuning phase range %d ~ %d\n",
			 mmc_hostname(mmc), ranges[i].start, ranges[i].end);

		if (longest_range_len < len) {
			longest_range_len = len;
			longest_range = i;
		}

	}
	pr_info("%s: the best tuning step range %d-%d(the length is %d)\n",
		mmc_hostname(mmc), ranges[longest_range].start,
		ranges[longest_range].end, longest_range_len);

	if ((longest_range_len == dll_cnt) && (type == SDHCI_SPRD_TUNING_SD_UHS_CMD))
		sprd_host->cmd_dly_all_pass = true;
	else
		sprd_host->cmd_dly_all_pass = false;

	mid_step = ranges[longest_range].start + longest_range_len / 2;
	mid_step %= dll_cnt;

	if (type == SDHCI_SPRD_TUNING_SD_UHS_CMD)
		sprd_host->cpst_cmd_dly = mid_step;

	if (type == SDHCI_SPRD_TUNING_SD_HS) {
		dll_cfg &= ~(0xf << 24);
		sdhci_writel(host, dll_cfg, SDHCI_SPRD_REG_32_DLL_CFG);
	} else {
		dll_cfg |= 0xf << 24;
		sdhci_writel(host, dll_cfg, SDHCI_SPRD_REG_32_DLL_CFG);
	}

	if (mid_step <= dll_cnt)
		final_phase = (mid_step * 256) / dll_cnt;
	else
		final_phase = 0xff;

	if (host->flags & SDHCI_HS400_TUNING) {
		p[MMC_TIMING_MMC_HS400] &= ~SDHCI_SPRD_CMD_DLY_MASK;
		p[MMC_TIMING_MMC_HS400] |= (final_phase << 8);
	}
	sdhci_sprd_set_dll_value(host, &p[mmc->ios.timing], opcode, final_phase, type);

	pr_info("%s: the best step %d, phase 0x%02x, delay value 0x%08x\n",
		mmc_hostname(mmc), mid_step, final_phase, p[mmc->ios.timing]);
	err = 0;

out:
	host->flags &= ~SDHCI_HS400_TUNING;

	if (cfg_use_adma) {
		host->flags &= ~SDHCI_USE_SDMA;
		host->flags |= SDHCI_USE_ADMA;
	}

	kfree(ranges);
	kfree(value_t);

	return err;
}

static int sdhci_sprd_execute_tuning(struct mmc_host *mmc, u32 opcode)
{
	enum sdhci_sprd_tuning_type type = SDHCI_SPRD_TUNING_DEFAULT;
	int err = 0;

	/*
	 * For better compatibility with some SD Cards,
	 * it must tuning CMD Line and DATA Line separately
	 */
	if (!strcmp(mmc_hostname(mmc), "mmc1")) {
		if ((mmc->ios.timing == MMC_TIMING_UHS_SDR104) ||
			(mmc->ios.timing == MMC_TIMING_UHS_SDR50))
			type = SDHCI_SPRD_TUNING_SD_UHS_CMD;
		else if (mmc->ios.timing == MMC_TIMING_SD_HS)
			type = SDHCI_SPRD_TUNING_SD_HS;
	}

	err = sdhci_sprd_tuning(mmc, opcode, type);
	if (!err && (type == SDHCI_SPRD_TUNING_SD_UHS_CMD)) {
		type = SDHCI_SPRD_TUNING_SD_UHS_DATA;
		err = sdhci_sprd_tuning(mmc, opcode, type);
	}

	return err;
}

static void sdhci_sprd_sd_cmdline_tuning(void *data, struct mmc_card *card, int *err)
{
	if (card->host->ios.timing == MMC_TIMING_SD_HS) {
		*err = sdhci_sprd_execute_tuning(card->host, MMC_SET_BLOCKLEN);
		if (*err)
			pr_err("%s: high-speed cmd tuning failed\n",
				mmc_hostname(card->host));
	}
}

static void sdhci_sprd_sd_dataline_tuning(void *data, struct mmc_card *card, int *err)
{
	if (card->host->ios.timing == MMC_TIMING_SD_HS) {
		*err = sdhci_sprd_execute_tuning(card->host, MMC_SEND_TUNING_BLOCK);
		if (*err)
			pr_err("%s: high-speed data and cmd tuning failed\n",
				mmc_hostname(card->host));
	}
}

static void sdhci_sprd_fast_hotplug_disable(struct sdhci_sprd_host *sprd_host)
{
	regmap_update_bits(sprd_host->reg_protect_enable.regmap,
		sprd_host->reg_protect_enable.reg,
		sprd_host->reg_protect_enable.mask, 0);
}

static void sdhci_sprd_fast_hotplug_enable(struct sdhci_sprd_host *sprd_host)
{
	int debounce_counter = 3;
	u32 reg_value = 0;
	int ret = 0;

	if (sprd_host->reg_rmldo_en.regmap) {
		/* this register do not support update in bits */
		ret = regmap_read(sprd_host->reg_rmldo_en.regmap,
				sprd_host->reg_rmldo_en.reg,
				&reg_value);
		if (ret < 0) {
			pr_err("remap global register failed!\n");
			return;
		}
		reg_value |= sprd_host->reg_rmldo_en.mask;
		ret = regmap_write(sprd_host->reg_rmldo_en.regmap,
				sprd_host->reg_rmldo_en.reg,
				reg_value);
		if (ret < 0) {
			pr_err("remap global register failed!\n");
			return;
		}
	}

	regmap_update_bits(sprd_host->reg_protect_enable.regmap,
		sprd_host->reg_protect_enable.reg,
		sprd_host->reg_protect_enable.mask,
		sprd_host->reg_protect_enable.mask);
	regmap_update_bits(sprd_host->reg_debounce_en.regmap,
		sprd_host->reg_debounce_en.reg,
		sprd_host->reg_debounce_en.mask,
		sprd_host->reg_debounce_en.mask);
	regmap_update_bits(sprd_host->reg_debounce_cn.regmap,
		sprd_host->reg_debounce_cn.reg,
		sprd_host->reg_debounce_cn.mask,
		debounce_counter << 16);
	if (sprd_host->detect_gpio_polar)
		regmap_update_bits(sprd_host->reg_detect_polar.regmap,
			sprd_host->reg_detect_polar.reg,
			sprd_host->reg_detect_polar.mask, 0);
	else
		regmap_update_bits(sprd_host->reg_detect_polar.regmap,
			sprd_host->reg_detect_polar.reg,
			sprd_host->reg_detect_polar.mask,
			sprd_host->reg_detect_polar.mask);
}

static void sdhci_sprd_signal_voltage_on_off(struct sdhci_host *host,
	u32 on_off)
{
	const char *name = mmc_hostname(host->mmc);

	if (IS_ERR(host->mmc->supply.vqmmc))
		return;

	if (on_off) {
		if (!regulator_is_enabled(host->mmc->supply.vqmmc)) {
			if (regulator_enable(host->mmc->supply.vqmmc))
				pr_err("%s: signal voltage enable fail!\n", name);
			else if (regulator_is_enabled(host->mmc->supply.vqmmc))
				pr_debug("%s: signal voltage enable success!\n", name);
			else
				pr_err("%s: signal voltage enable hw fail!\n", name);
		}
	} else {
		if (regulator_is_enabled(host->mmc->supply.vqmmc)) {
			if (regulator_disable(host->mmc->supply.vqmmc))
				pr_err("%s: signal voltage disable fail\n", name);
			else if (!regulator_is_enabled(host->mmc->supply.vqmmc))
				pr_debug("%s: signal voltage disable success!\n", name);
			else
				pr_err("%s: signal voltage disable hw fail\n", name);
		}
	}
}

static void sdhci_sprd_set_power(struct sdhci_host *host, unsigned char mode,
	unsigned short vdd)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	struct mmc_host *mmc = host->mmc;
	int ret;

	if (sprd_host->power_mode == mmc->ios.power_mode)
		return;

	switch (mode) {
	case MMC_POWER_OFF:
		if (sprd_host->reg_protect_enable.regmap
				&& host->mmc_host_ops.get_cd(host->mmc))
			sdhci_sprd_fast_hotplug_disable(sprd_host);

		if (!host->mmc_host_ops.get_cd(host->mmc)) {
			unsigned char old_signal_voltage = mmc->ios.signal_voltage;
			unsigned short old_vdd = mmc->ios.vdd;
			/*
			 * make sure io_voltage will keep 3.3V in next power up while plugin sd,
			 * but will not do this in deepsleep power off
			 */
			mmc->ios.signal_voltage = MMC_SIGNAL_VOLTAGE_330;
			mmc->ios.vdd = 18;
			ret = host->mmc_host_ops.start_signal_voltage_switch(mmc, &mmc->ios);
			if (ret)
				pr_err("%s: signal voltage set to 3.3v fail %d!\n",
				       mmc_hostname(host->mmc), ret);
			mmc->ios.vdd = old_vdd;
			mmc->ios.signal_voltage = old_signal_voltage;
		}

		sdhci_sprd_signal_voltage_on_off(host, 0);
		if (!IS_ERR(mmc->supply.vmmc))
			mmc_regulator_set_ocr(host->mmc, mmc->supply.vmmc, 0);
		break;
	case MMC_POWER_ON:
	case MMC_POWER_UP:
		if (!IS_ERR(mmc->supply.vmmc))
			mmc_regulator_set_ocr(host->mmc, mmc->supply.vmmc, vdd);
		usleep_range_state(200, 250, TASK_UNINTERRUPTIBLE);
		sdhci_sprd_signal_voltage_on_off(host, 1);

		if (sprd_host->reg_detect_polar.regmap && sprd_host->reg_protect_enable.regmap
			&& sprd_host->reg_detect_polar.regmap
			&& sprd_host->reg_protect_enable.regmap
			&& host->mmc_host_ops.get_cd(host->mmc))
			sdhci_sprd_fast_hotplug_enable(sprd_host);
		break;
	}

	sprd_host->power_mode = mmc->ios.power_mode;
}

static void sdhci_sprd_dump_vendor_regs(struct sdhci_host *host)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	char sdhci_hostname[64];

	if (!host->mmc->card || sprd_host->tuning_flag)
		return;

	SDHCI_SPRD_DUMP("CMD%d Error\n", SDHCI_GET_CMD(sdhci_readw(host, SDHCI_COMMAND)));

	sprintf(sdhci_hostname, "%s%s", mmc_hostname(host->mmc),
			": sprd-sdhci + 0x000: ");
	print_hex_dump(KERN_ERR, sdhci_hostname, DUMP_PREFIX_OFFSET,
			16, 4, host->ioaddr, 64, 0);

	sprintf(sdhci_hostname, "%s%s", mmc_hostname(host->mmc),
			": sprd-sdhci + 0x200: ");
	print_hex_dump(KERN_ERR, sdhci_hostname, DUMP_PREFIX_OFFSET,
			16, 4, host->ioaddr + 0x200, 48, 0);

	sprintf(sdhci_hostname, "%s%s", mmc_hostname(host->mmc),
			": sprd-sdhci + 0x250: ");
	print_hex_dump(KERN_ERR, sdhci_hostname, DUMP_PREFIX_OFFSET,
			16, 4, host->ioaddr + 0x250, 32, 0);

	if (sprd_host->support_cqe) {
		sprintf(sdhci_hostname, "%s%s", mmc_hostname(host->mmc),
				": sprd-sdhci + 0x300: ");
		print_hex_dump(KERN_ERR, sdhci_hostname, DUMP_PREFIX_OFFSET,
				16, 4, host->ioaddr + 0x300, 128, 0);
	}
}

static u32 sdhci_sprd_cqe_irq(struct sdhci_host *host, u32 intmask)
{
	int cmd_error = 0;
	int data_error = 0;
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	sprd_host->int_status = intmask;

	if (intmask & SDHCI_INT_ERROR_MASK)
		sdhci_sprd_dump_vendor_regs(host);

	if (!sdhci_cqe_irq(host, intmask, &cmd_error, &data_error))
		return intmask;

	cqhci_irq(host->mmc, intmask, cmd_error, data_error);

	return 0;
}

void sdhci_sprd_reset(struct sdhci_host *host, u8 mask)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	if (sprd_host->support_cqe && (host->mmc->caps2 & MMC_CAP2_CQE) && (mask & SDHCI_RESET_ALL)
	    && host->mmc->cqe_private)
		cqhci_deactivate(host->mmc);

	sdhci_reset(host, mask);
}

static struct sdhci_ops sdhci_sprd_ops = {
	.read_l = sdhci_sprd_readl,
	.write_l = sdhci_sprd_writel,
	.write_b = sdhci_sprd_writeb,
	.write_w = sdhci_sprd_writew,
	.set_clock = sdhci_sprd_set_clock,
	.set_power = sdhci_sprd_set_power,
	.get_max_clock = sdhci_sprd_get_max_clock,
	.get_min_clock = sdhci_sprd_get_min_clock,
	.set_bus_width = sdhci_set_bus_width,
	.reset = sdhci_sprd_reset,
	.set_uhs_signaling = sdhci_sprd_set_uhs_signaling,
	.hw_reset = sdhci_sprd_hw_reset,
	.get_max_timeout_count = sdhci_sprd_get_max_timeout_count,
	.get_ro = sdhci_sprd_get_ro,
	.request_done = sdhci_sprd_request_done,
	.dump_vendor_regs = sdhci_sprd_dumpregs,
	.irq = sdhci_sprd_cqe_irq,
};

static void sdhci_sprd_check_auto_cmd23(struct mmc_host *mmc,
					struct mmc_request *mrq)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	host->flags |= sprd_host->flags & SDHCI_AUTO_CMD23;

	/*
	 * From version 4.10 onward, ARGUMENT2 register is also as 32-bit
	 * block count register which doesn't support stuff bits of
	 * CMD23 argument on Spreadtrum's sd host controller.
	 */
	if (sprd_host->support_swcq) {
		if (host->version >= SDHCI_SPEC_410 &&
		    mrq->sbc && ((mrq->sbc->arg & SDHCI_SPRD_ARG2_STUFF) ||
		    (host->mmc->card && mmc_card_cmdq(host->mmc->card))) &&
		    (host->flags & SDHCI_AUTO_CMD23))
			host->flags &= ~SDHCI_AUTO_CMD23;
	} else {
		if (host->version >= SDHCI_SPEC_410 &&
		    mrq->sbc && (mrq->sbc->arg & SDHCI_SPRD_ARG2_STUFF) &&
		    (host->flags & SDHCI_AUTO_CMD23))
			host->flags &= ~SDHCI_AUTO_CMD23;
	}
}

static void sdhci_sprd_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct mmc_swcq *swcq = mmc->cqe_private;
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	sdhci_sprd_check_auto_cmd23(mmc, mrq);
	if (HOST_IS_EMMC_TYPE(mmc) && sprd_host->support_swcq) {
		if (!swcq->need_polling) {
			sdhci_writel(host, host->ier, SDHCI_SIGNAL_ENABLE);
			if (mrq->cmd)
				dbg_add_host_log(mmc, 0, mrq->cmd->opcode, mrq->cmd->arg, mrq);
		} else {
			sdhci_sprd_request_sync(mmc, mrq);
			return;
		}
	}

	sdhci_request(mmc, mrq);
}

static int sdhci_sprd_request_atomic(struct mmc_host *mmc,
				      struct mmc_request *mrq)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	sdhci_sprd_check_auto_cmd23(mmc, mrq);

	if (sprd_host->support_swcq) {
		if (HOST_IS_EMMC_TYPE(mmc) && mmc->card && mmc_card_cmdq(mmc->card))
			return sdhci_sprd_request_sync(mmc, mrq);
		return _sdhci_request_atomic(mmc, mrq);
	} else {
		return sdhci_request_atomic(mmc, mrq);
	}
}

static int sdhci_sprd_voltage_switch(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	int ret;

	if (!IS_ERR(mmc->supply.vqmmc)) {
		ret = mmc_regulator_set_vqmmc(mmc, ios);
		if (ret < 0) {
			pr_err("%s: Switching signalling voltage failed, ret = %d\n",
				mmc_hostname(mmc), ret);
			return ret;
		}
	}

	if (IS_ERR(sprd_host->pinctrl))
		/*
		 * If pinctrl not defined in dts, still need reset here because
		 * voltage have changed. Otherwise the controller will not work
		 * normally. We had ever encoutered CMD2 timeout before.
		 */
		goto reset;

	switch (ios->signal_voltage) {
	case MMC_SIGNAL_VOLTAGE_180:
		ret = pinctrl_select_state(sprd_host->pinctrl,
					   sprd_host->pins_uhs);
		if (ret) {
			pr_err("%s: failed to select uhs pin state\n",
			       mmc_hostname(mmc));
			return ret;
		}
		break;

	default:
		fallthrough;
	case MMC_SIGNAL_VOLTAGE_330:
		ret = pinctrl_select_state(sprd_host->pinctrl,
					   sprd_host->pins_default);
		if (ret) {
			pr_err("%s: failed to select default pin state\n",
			       mmc_hostname(mmc));
			return ret;
		}
		break;
	}

	/* Wait for 300 ~ 500 us for pin state stable */
	usleep_range_state(300, 500, TASK_UNINTERRUPTIBLE);

reset:
	sdhci_reset(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA);

	return 0;
}

static void sdhci_sprd_hs400_enhanced_strobe(struct mmc_host *mmc,
					     struct mmc_ios *ios)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	u32 *p = sprd_host->phy_delay;
	u16 ctrl_2;

	if (!ios->enhanced_strobe)
		return;

	sdhci_sprd_sd_clk_off(host);

	/* Set HS400 enhanced strobe mode */
	ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;
	ctrl_2 |= SDHCI_SPRD_CTRL_HS400ES;
	sdhci_writew(host, ctrl_2, SDHCI_HOST_CONTROL2);

	sdhci_sprd_sd_clk_on(host);

	/* Set the PHY DLL delay value for HS400 enhanced strobe mode */
	sdhci_writel(host, p[MMC_TIMING_MMC_HS400 + 1],
		     SDHCI_SPRD_REG_32_DLL_DLY);
}

static void sdhci_sprd_phy_param_parse(struct sdhci_sprd_host *sprd_host,
				       struct device_node *np)
{
	u32 *p = sprd_host->phy_delay;
	int ret, i, index;
	u32 val[4];

	for (i = 0; i < ARRAY_SIZE(sdhci_sprd_phy_cfgs); i++) {
		ret = of_property_read_u32_array(np,
				sdhci_sprd_phy_cfgs[i].property, val, 4);
		if (ret)
			continue;

		index = sdhci_sprd_phy_cfgs[i].timing;
		p[index] = val[0] | (val[1] << 8) | (val[2] << 16) | (val[3] << 24);
	}
}

static const struct sdhci_pltfm_data sdhci_sprd_pdata = {
	.quirks = SDHCI_QUIRK_BROKEN_CARD_DETECTION |
		  SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK |
		  SDHCI_QUIRK_MISSING_CAPS,
	.quirks2 = SDHCI_QUIRK2_BROKEN_HS200 |
		   SDHCI_QUIRK2_USE_32BIT_BLK_CNT |
		   SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
	.ops = &sdhci_sprd_ops,
};

static void sdhci_sprd_get_fast_hotplug_reg(struct device_node *np,
	struct register_hotplug *reg, const char *name)
{
	struct regmap *regmap;
	u32 syscon_args[2];

	regmap = syscon_regmap_lookup_by_phandle_args(np, name, 2, syscon_args);
	if (IS_ERR(regmap)) {
		pr_warn("read sdio fast hotplug %s regmap fail\n", name);
		reg->regmap = NULL;
		reg->reg = 0x0;
		reg->mask = 0x0;
		goto out;
	} else {
		reg->regmap = regmap;
		reg->reg = syscon_args[0];
		reg->mask = syscon_args[1];
	}

out:
	of_node_put(np);
}

static void sdhci_sprd_get_fast_hotplug_info(struct device_node *np,
	struct sdhci_sprd_host *sprd_host)
{
	sdhci_sprd_get_fast_hotplug_reg(np, &sprd_host->reg_detect_polar,
		"sd-detect-pol-syscon");
	sdhci_sprd_get_fast_hotplug_reg(np, &sprd_host->reg_protect_enable,
		"sd-hotplug-protect-en-syscon");
	sdhci_sprd_get_fast_hotplug_reg(np, &sprd_host->reg_debounce_en,
		"sd-hotplug-debounce-en-syscon");
	sdhci_sprd_get_fast_hotplug_reg(np, &sprd_host->reg_debounce_cn,
		"sd-hotplug-debounce-cn-syscon");
	sdhci_sprd_get_fast_hotplug_reg(np, &sprd_host->reg_rmldo_en,
		"sd-hotplug-rmldo-en-syscon");
}

static int sdhci_sprd_ice_init(struct sdhci_host *host,
				  struct cqhci_host *cq_host)
{
	struct mmc_host *mmc = host->mmc;
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	struct device *dev = mmc_dev(mmc);
	struct resource *res;

	if (!(cqhci_readl(cq_host, CQHCI_CAP) & CQHCI_CAP_CS)) {
		dev_warn(dev, "CQE no supprots ICE\n");
		return 0;
	}

	if (!sprd_host->support_ice)
		goto disable;

	res = platform_get_resource_byname(sprd_host->pdev, IORESOURCE_MEM, "ice");
	if (!res) {
		dev_warn(dev, "ICE registers not found\n");
		goto disable;
	}

	sprd_host->ice_mem = devm_ioremap_resource(dev, res);
	if (IS_ERR(sprd_host->ice_mem))
		return PTR_ERR(sprd_host->ice_mem);

	mmc->caps2 |= MMC_CAP2_CRYPTO;

	return 0;

disable:
	dev_warn(dev, "Disabling inline encryption support\n");

	return 0;
}

static void sdhci_sprd_cqe_write_l(struct cqhci_host *host, u32 val, int reg)
{
	struct mmc_host *mmc = host->mmc;
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(mmc_priv(mmc));

	if (reg >= CQHCI_CCAP) {
		writel(val, sprd_host->ice_mem + reg);
	} else {
		/* Prevent TCL int storms. */
		if ((reg == CQHCI_ISGE) && (val & CQHCI_IS_TCL))
			val &= ~CQHCI_IS_TCL;
		writel(val, host->mmio + reg);
	}

	/* Ensure all writes are done before interrupts are enabled */
	wmb();
}

static u32 sdhci_sprd_cqe_read_l(struct cqhci_host *host, int reg)
{
	struct mmc_host *mmc = host->mmc;
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(mmc_priv(mmc));

	if (reg >= CQHCI_CCAP)
		return readl(sprd_host->ice_mem + reg);

	return readl(host->mmio + reg);
}

static void sdhci_sprd_cqe_enable(struct mmc_host *mmc)
{
	struct sdhci_host *host = mmc_priv(mmc);
	unsigned long flags;
	u8 ctrl;

	sdhci_cqe_enable(mmc);

	spin_lock_irqsave(&host->lock, flags);

	/* Make sure that controller DMA is SDMA mode when using CQE */
	ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);
	ctrl &= ~SDHCI_CTRL_DMA_MASK;
	sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);

	spin_unlock_irqrestore(&host->lock, flags);
}

static void sdhci_sprd_cqe_disable(struct mmc_host *mmc, bool recovery)
{
	struct sdhci_host *host = mmc_priv(mmc);
	unsigned long flags;
	u32 ctrl;

	/*
	 * When CQE is halted, the legacy SDHCI path operates only
	 * on 16-byte descriptors in 64bit mode.
	 */
	if (host->flags & SDHCI_USE_64_BIT_DMA)
		host->desc_sz = 16;

	spin_lock_irqsave(&host->lock, flags);

	/*
	 * During CQE command transfers, command complete bit gets latched.
	 * So s/w should clear command complete interrupt status when CQE is
	 * either halted or disabled. Otherwise unexpected SDCHI legacy
	 * interrupt gets triggered when CQE is halted/disabled.
	 */
	ctrl = sdhci_readl(host, SDHCI_INT_ENABLE);
	ctrl |= SDHCI_INT_RESPONSE;
	sdhci_writel(host,  ctrl, SDHCI_INT_ENABLE);
	sdhci_writel(host, SDHCI_INT_RESPONSE, SDHCI_INT_STATUS);

	spin_unlock_irqrestore(&host->lock, flags);

	sdhci_cqe_disable(mmc, recovery);
}

static void sdhci_sprd_cqe_dump_vendor_regs(struct mmc_host *mmc)
{
	struct sdhci_host *host = mmc_priv(mmc);

	sdhci_sprd_dump_vendor_regs(host);
}

static void sdhci_sprd_cqe_pre_enable(struct mmc_host *mmc)
{
	struct cqhci_host *cq_host = mmc->cqe_private;
	u32 reg;

	reg = cqhci_readl(cq_host, CQHCI_CFG);
	reg |= CQHCI_ENABLE;
	cqhci_writel(cq_host, reg, CQHCI_CFG);
}

static void sdhci_sprd_cqe_post_disable(struct mmc_host *mmc)
{
	struct cqhci_host *cq_host = mmc->cqe_private;
	u32 reg;

	reg = cqhci_readl(cq_host, CQHCI_CFG);
	reg &= ~CQHCI_ENABLE;
	cqhci_writel(cq_host, reg, CQHCI_CFG);
}

static const struct cqhci_host_ops sdhci_sprd_cqhci_ops = {
	.write_l = sdhci_sprd_cqe_write_l,
	.read_l = sdhci_sprd_cqe_read_l,
	.enable = sdhci_sprd_cqe_enable,
	.disable = sdhci_sprd_cqe_disable,
	.dumpregs = sdhci_sprd_cqe_dump_vendor_regs,
	.pre_enable = sdhci_sprd_cqe_pre_enable,
	.post_disable = sdhci_sprd_cqe_post_disable,
};

static int sdhci_sprd_cqe_add_host(struct sdhci_host *host,
				struct platform_device *pdev)
{
	struct cqhci_host *cq_host;
	bool dma64;
	int ret;
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	cq_host = cqhci_pltfm_init(pdev);
	if (IS_ERR(cq_host)) {
		ret = PTR_ERR(cq_host);
		dev_err(&pdev->dev, "cqhci-pltfm init: failed: %d\n", ret);
		goto cleanup;
	}

	sprd_host->cqe_mem = cq_host->mmio;
	host->mmc->caps2 |= MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD;
	cq_host->ops = &sdhci_sprd_cqhci_ops;

	dma64 = host->flags & SDHCI_USE_64_BIT_DMA;

	ret = sdhci_sprd_ice_init(host, cq_host);
	if (ret) {
		dev_err(&pdev->dev, "%s: ICE init: failed (%d)\n",
				mmc_hostname(host->mmc), ret);
		goto cleanup;
	}

	ret = cqhci_init(cq_host, host->mmc, dma64);
	if (ret) {
		dev_err(&pdev->dev, "%s: CQE init: failed (%d)\n",
				mmc_hostname(host->mmc), ret);
		goto cleanup;
	}

	/* Enable force hw reset during cqe recovery */
	host->mmc->cqe_recovery_reset_always = true;

	dev_info(&pdev->dev, "%s: CQE init: success\n", mmc_hostname(host->mmc));

cleanup:
	return ret;
}

#if !IS_MODULE(CONFIG_MMC_CQHCI)
int cqhci_resume(struct mmc_host *mmc)
{
	return -EINVAL;
}

int cqhci_deactivate(struct mmc_host *mmc)
{
	return -EINVAL;
}

int cqhci_init(struct cqhci_host *cq_host, struct mmc_host *mmc,
	      bool dma64)
{
	return -EINVAL;
}

struct cqhci_host *cqhci_pltfm_init(struct platform_device *pdev)
{
	return ERR_PTR(-EINVAL);
}

irqreturn_t cqhci_irq(struct mmc_host *mmc, u32 intmask, int cmd_error,
		      int data_error)
{
	return IRQ_HANDLED;
}
#endif

/*
 * Vendor hook can only be registered once.
 */
static void sdhci_sprd_register_vendor_hook(struct sdhci_host *host)
{
	if (!strcmp(mmc_hostname(host->mmc), "mmc1")) {
		register_trace_android_rvh_mmc_sd_cmdline_timing(
			sdhci_sprd_sd_cmdline_tuning,
			NULL);
		register_trace_android_rvh_mmc_sd_cmdline_timing(
			sdhci_sprd_sd_dataline_tuning,
			NULL);
	}
	if (!strcmp(mmc_hostname(host->mmc), "mmc0")) {
		register_trace_android_rvh_mmc_partition_status(
			sdhci_sprd_health_and_powp,
			NULL);
	}
}

static int sdhci_sprd_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	enum of_gpio_flags flags;
	struct sdhci_host *host;
	struct sdhci_sprd_host *sprd_host;
	struct mmc_hsq *hsq;
	struct clk *clk;
	int ret = 0;
	struct device_node *node = pdev->dev.of_node;

	host = sdhci_pltfm_init(pdev, &sdhci_sprd_pdata, sizeof(*sprd_host));
	if (IS_ERR(host))
		return PTR_ERR(host);

	host->dma_mask = DMA_BIT_MASK(64);
	pdev->dev.dma_mask = &host->dma_mask;
	host->mmc_host_ops.request = sdhci_sprd_request;
	host->mmc_host_ops.hs400_enhanced_strobe =
		sdhci_sprd_hs400_enhanced_strobe;
	/*
	 * We can not use the standard ops to change and detect the voltage
	 * signal for Spreadtrum SD host controller, since our voltage regulator
	 * for I/O is fixed in hardware, that means we do not need control
	 * the standard SD host controller to change the I/O voltage.
	 */
	host->mmc_host_ops.start_signal_voltage_switch =
		sdhci_sprd_voltage_switch;
	host->mmc_host_ops.execute_tuning = sdhci_sprd_execute_tuning;

	host->mmc->caps = MMC_CAP_SD_HIGHSPEED | MMC_CAP_MMC_HIGHSPEED |
		MMC_CAP_WAIT_WHILE_BUSY;

	ret = mmc_of_parse(host->mmc);
	if (ret)
		goto pltfm_free;

	if (!mmc_card_is_removable(host->mmc))
		host->mmc_host_ops.request_atomic = sdhci_sprd_request_atomic;
	else
		host->always_defer_done = true;

	sprd_host = TO_SPRD_HOST(host);
	sdhci_sprd_phy_param_parse(sprd_host, pdev->dev.of_node);

	sprd_host->pdev = pdev;

	sprd_host->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (!IS_ERR(sprd_host->pinctrl)) {
		sprd_host->pins_uhs =
			pinctrl_lookup_state(sprd_host->pinctrl, "state_uhs");
		if (IS_ERR(sprd_host->pins_uhs)) {
			ret = PTR_ERR(sprd_host->pins_uhs);
			goto pltfm_free;
		}

		sprd_host->pins_default =
			pinctrl_lookup_state(sprd_host->pinctrl, "default");
		if (IS_ERR(sprd_host->pins_default)) {
			ret = PTR_ERR(sprd_host->pins_default);
			goto pltfm_free;
		}
	}

	sprd_host->detect_gpio = of_get_named_gpio_flags(np, "cd-gpios", 0, &flags);
	if (!gpio_is_valid(sprd_host->detect_gpio))
		sprd_host->detect_gpio = -1;
	else {
		sdhci_sprd_get_fast_hotplug_info(np, sprd_host);
		sprd_host->detect_gpio_polar = flags;
	}

	clk = devm_clk_get(&pdev->dev, "sdio");
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		goto pltfm_free;
	}
	sprd_host->clk_sdio = clk;
	sprd_host->base_rate = clk_get_rate(sprd_host->clk_sdio);
	if (!sprd_host->base_rate) {
		sprd_host->base_rate = SDHCI_SPRD_CLK_DEF_RATE;
		pr_err("%s: base rate is 0, please check clk source\n",
			mmc_hostname(host->mmc));
	}

	if (of_property_read_bool(node, "supports-swcq")) {
		sprd_host->support_swcq = true;
	} else {
		sprd_host->support_swcq = false;
		if (of_property_read_bool(node, "supports-cqe")) {
			sprd_host->support_cqe = true;
			if (of_property_read_bool(node, "supports-ice"))
				sprd_host->support_ice = true;
			else
				sprd_host->support_ice = false;
		} else {
			sprd_host->support_cqe = false;
			sprd_host->support_ice = false;
		}
	}

	clk = devm_clk_get(&pdev->dev, "enable");
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		goto pltfm_free;
	}
	sprd_host->clk_enable = clk;

	clk = devm_clk_get(&pdev->dev, "1x_enable");
	if (!IS_ERR(clk))
		sprd_host->clk_1x_enable = clk;

	clk = devm_clk_get(&pdev->dev, "2x_enable");
	if (!IS_ERR(clk))
		sprd_host->clk_2x_enable = clk;

	ret = clk_prepare_enable(sprd_host->clk_sdio);
	if (ret)
		goto pltfm_free;

	ret = clk_prepare_enable(sprd_host->clk_enable);
	if (ret)
		goto clk_sdio_disable;

	ret = clk_prepare_enable(sprd_host->clk_1x_enable);
	if (ret)
		goto clk_disable;

	ret = clk_prepare_enable(sprd_host->clk_2x_enable);
	if (ret)
		goto clk_1x_disable;

	sdhci_sprd_init_config(host);
	host->version = sdhci_readw(host, SDHCI_HOST_VERSION);
	sprd_host->version = ((host->version & SDHCI_VENDOR_VER_MASK) >>
			       SDHCI_VENDOR_VER_SHIFT);

	sprd_host->power_mode = MMC_POWER_OFF;

	pm_runtime_get_noresume(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, 50);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, 1);

	sdhci_enable_v4_mode(host);

	/*
	 * Supply the existing CAPS, but clear the UHS-I modes. This
	 * will allow these modes to be specified only by device
	 * tree properties through mmc_of_parse().
	 */
	host->caps = sdhci_readl(host, SDHCI_CAPABILITIES);
	host->caps1 = sdhci_readl(host, SDHCI_CAPABILITIES_1);
	host->caps1 &= ~(SDHCI_SUPPORT_SDR50 | SDHCI_SUPPORT_SDR104 |
			 SDHCI_SUPPORT_DDR50);

	if (HOST_IS_SD_TYPE(host->mmc)) {
		ret = mmc_regulator_get_supply(host->mmc);
		if (ret)
			goto pm_runtime_disable;
	}

	ret = sdhci_setup_host(host);
	if (ret)
		goto pm_runtime_disable;

	host->mmc->ocr_avail = 0x40000;
	host->mmc->ocr_avail_sdio = host->mmc->ocr_avail;
	host->mmc->ocr_avail_sd = host->mmc->ocr_avail;
	host->mmc->ocr_avail_mmc = host->mmc->ocr_avail;

	host->mmc->max_current_330 = SDHCI_SPRD_MAX_CUR;
	host->mmc->max_current_300 = SDHCI_SPRD_MAX_CUR;
	host->mmc->max_current_180 = SDHCI_SPRD_MAX_CUR;

	sprd_host->flags = host->flags;

	if (sprd_host->support_swcq) {
		ret = mmc_hsq_swcq_init(host, pdev);
		if (ret)
			goto err_cleanup_host;
	} else if (sprd_host->support_cqe) {
		ret = sdhci_sprd_cqe_add_host(host, pdev);
		if (ret)
			goto err_cleanup_host;
	} else {
		hsq = devm_kzalloc(&pdev->dev, sizeof(*hsq), GFP_KERNEL);
		if (!hsq) {
			ret = -ENOMEM;
			goto err_cleanup_host;
		}

		ret = mmc_hsq_init(hsq, host->mmc);
		if (ret)
			goto err_cleanup_host;
	}

	ret = __sdhci_add_host(host);
	if (ret)
		goto err_cleanup_host;

	if (sprd_host->support_swcq) {
		ret = sdhci_sprd_irq_request_swcq(host);
		if (ret)
			goto err_cleanup_host;
	}

	if (!mmc_check_wp_fn(host->mmc)) {
		ret = mmc_wp_init(host->mmc);
		if (ret)
			goto err_cleanup_host;
	}

	if (!of_property_read_bool(node, "no-ffu")) {
		ret = mmc_ffu_init(host);
		if (ret)
			goto err_cleanup_host;
	}

	/* disable polling scan for sdiocard */
	if ((host->mmc->caps2 & MMC_CAP2_NO_SD)
			&& (host->mmc->caps2 & MMC_CAP2_NO_MMC)) {
		host->mmc->caps &= ~MMC_CAP_NEEDS_POLL;
	}

	sdhci_sprd_register_vendor_hook(host);

#ifdef CONFIG_SPRD_DEBUG
	sdhci_sprd_add_host_debugfs(host);
#endif

	pm_runtime_mark_last_busy(&pdev->dev);
	pm_runtime_put_autosuspend(&pdev->dev);

	return 0;

err_cleanup_host:
	sdhci_cleanup_host(host);

pm_runtime_disable:
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);

	clk_disable_unprepare(sprd_host->clk_2x_enable);

clk_1x_disable:
	clk_disable_unprepare(sprd_host->clk_1x_enable);

clk_disable:
	clk_disable_unprepare(sprd_host->clk_enable);

clk_sdio_disable:
	clk_disable_unprepare(sprd_host->clk_sdio);

pltfm_free:
	sdhci_pltfm_free(pdev);
	return ret;
}

static int sdhci_sprd_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	if (!mmc_check_wp_fn(host->mmc))
		mmc_wp_remove(host->mmc);

	mmc_ffu_remove(host);
	sdhci_remove_host(host, 0);

	clk_disable_unprepare(sprd_host->clk_sdio);
	clk_disable_unprepare(sprd_host->clk_enable);
	clk_disable_unprepare(sprd_host->clk_2x_enable);
	clk_disable_unprepare(sprd_host->clk_1x_enable);

	sdhci_pltfm_free(pdev);

	return 0;
}

static const struct of_device_id sdhci_sprd_of_match[] = {
	{ .compatible = "sprd,sdhci-r11", },
	{ }
};
MODULE_DEVICE_TABLE(of, sdhci_sprd_of_match);

#ifdef CONFIG_PM
static int sdhci_sprd_runtime_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

	if (HOST_IS_EMMC_TYPE(host->mmc) && sprd_host->support_swcq)
		mmc_swcq_suspend(host->mmc);
	else if (sprd_host->support_cqe)
		cqhci_suspend(host->mmc);
	else
		mmc_hsq_suspend(host->mmc);

	sdhci_runtime_suspend_host(host);

	clk_disable_unprepare(sprd_host->clk_sdio);
	clk_disable_unprepare(sprd_host->clk_enable);
	clk_disable_unprepare(sprd_host->clk_2x_enable);
	clk_disable_unprepare(sprd_host->clk_1x_enable);

	return 0;
}

static int sdhci_sprd_runtime_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	int ret;

	ret = clk_prepare_enable(sprd_host->clk_1x_enable);
	if (ret)
		return ret;

	ret = clk_prepare_enable(sprd_host->clk_2x_enable);
	if (ret)
		goto clk_1x_disable;

	ret = clk_prepare_enable(sprd_host->clk_enable);
	if (ret)
		goto clk_2x_disable;

	ret = clk_prepare_enable(sprd_host->clk_sdio);
	if (ret)
		goto clk_disable;

	sdhci_runtime_resume_host(host, 1);

	if (HOST_IS_EMMC_TYPE(host->mmc) && sprd_host->support_swcq)
		mmc_swcq_resume(host->mmc);
	else if (sprd_host->support_cqe)
		cqhci_resume(host->mmc);
	else
		mmc_hsq_resume(host->mmc);

	return 0;

clk_disable:
	clk_disable_unprepare(sprd_host->clk_enable);

clk_2x_disable:
	clk_disable_unprepare(sprd_host->clk_2x_enable);

clk_1x_disable:
	clk_disable_unprepare(sprd_host->clk_1x_enable);

	return ret;
}
#endif

static const struct dev_pm_ops sdhci_sprd_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(sdhci_sprd_runtime_suspend,
			   sdhci_sprd_runtime_resume, NULL)
};

static struct platform_driver sdhci_sprd_driver = {
	.probe = sdhci_sprd_probe,
	.remove = sdhci_sprd_remove,
	.driver = {
		.name = "sdhci_sprd_r11",
		.of_match_table = sdhci_sprd_of_match,
		.pm = &sdhci_sprd_pm_ops,
	},
};
module_platform_driver(sdhci_sprd_driver);

MODULE_DESCRIPTION("Spreadtrum sdio host controller r11 driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sdhci-sprd-r11");
