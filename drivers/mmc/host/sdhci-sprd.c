// SPDX-License-Identifier: GPL-2.0
//
// Secure Digital Host Controller
//
// Copyright (C) 2018 Spreadtrum, Inc.
// Author: Chunyan Zhang <chunyan.zhang@unisoc.com>

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include "sdhci-pltfm.h"
#include "mmc_hsq.h"

/* SDHCI_ARGUMENT2 register high 16bit */
#define SDHCI_SPRD_ARG2_STUFF		GENMASK(31, 16)

#define SDHCI_SPRD_REG_32_DLL_CFG	0x200
#define  SDHCI_SPRD_DLL_ALL_CPST_EN	(BIT(18) | BIT(24) | BIT(25) | BIT(26) | BIT(27))
#define  SDHCI_SPRD_DLL_EN		BIT(21)
#define  SDHCI_SPRD_DLL_SEARCH_MODE	BIT(16)
#define  SDHCI_SPRD_DLL_INIT_COUNT	0xc00
#define  SDHCI_SPRD_DLL_PHASE_INTERNAL	0x3
#define  SDHCI_SPRD_DLL_CPST_THRES	(0x20)


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

#define SDHCI_SPRD_MAX_CUR		0xFFFFFF
#define SDHCI_SPRD_CLK_MAX_DIV		1023

#define SDHCI_SPRD_CLK_DEF_RATE		26000000
#define SDHCI_SPRD_PHY_DLL_CLK		52000000
/*
 * The following register is defined by spreadtrum self.
 * It is not standard register of SDIO
 */
#define SDHCI_SPRD_REG_32_DLL_STS0	0x210
#define SDHCI_SPRD_DLL_LOCKED		0x00040000

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


struct ranges_t {
	int start;
	int end;
};

struct sdhci_sprd_host {
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
	if (unlikely(reg == SDHCI_MAX_CURRENT))
		return SDHCI_SPRD_MAX_CUR;

	return readl_relaxed(host->ioaddr + reg);
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
	/* SDHCI_BLOCK_COUNT is Read Only on Spreadtrum's platform */
	if (unlikely(reg == SDHCI_BLOCK_COUNT))
		return;

	writew_relaxed(val, host->ioaddr + reg);
}

static inline void sdhci_sprd_writeb(struct sdhci_host *host, u8 val, int reg)
{
	/*
	 * Since BIT(3) of SDHCI_SOFTWARE_RESET is reserved according to the
	 * standard specification, sdhci_reset() write this register directly
	 * without checking other reserved bits, that will clear BIT(3) which
	 * is defined as hardware reset on Spreadtrum's platform and clearing
	 * it by mistake will lead the card not work. So here we need to work
	 * around it.
	 */
	if (unlikely(reg == SDHCI_SOFTWARE_RESET)) {
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

	if (div > SDHCI_SPRD_CLK_MAX_DIV)
		div = SDHCI_SPRD_CLK_MAX_DIV;

	if (div % 2)
		div = (div + 1) / 2;
	else
		div = div / 2;

	return div;
}

static inline void _sdhci_sprd_set_clock(struct sdhci_host *host,
					unsigned int clk)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	u32 div, val, mask;

	usleep_range(20000, 22500);

	sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);

	div = sdhci_sprd_calc_div(sprd_host->base_rate, clk);
	div = ((div & 0x300) >> 2) | ((div & 0xFF) << 8);
	sdhci_enable_clk(host, div);

	/* enable auto gate sdhc_enable_auto_gate */
	val = sdhci_readl(host, SDHCI_SPRD_REG_32_BUSY_POSI);
	mask = SDHCI_SPRD_BIT_OUTR_CLK_AUTO_EN |
	       SDHCI_SPRD_BIT_INNR_CLK_AUTO_EN;
	if (mask != (val & mask)) {
		val |= mask;
		sdhci_writel(host, val, SDHCI_SPRD_REG_32_BUSY_POSI);
	}

}

static void sdhci_sprd_enable_phy_dll(struct sdhci_host *host)
{
	u32 tmp;

	tmp = sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_CFG);
	tmp &= ~(SDHCI_SPRD_DLL_EN | SDHCI_SPRD_DLL_ALL_CPST_EN);
	sdhci_writel(host, tmp, SDHCI_SPRD_REG_32_DLL_CFG);
	/* wait 1ms */
	usleep_range(1000, 1250);

	tmp = sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_CFG);
	tmp |= SDHCI_SPRD_DLL_ALL_CPST_EN | SDHCI_SPRD_DLL_SEARCH_MODE |
		SDHCI_SPRD_DLL_INIT_COUNT | SDHCI_SPRD_DLL_PHASE_INTERNAL |
		SDHCI_SPRD_DLL_CPST_THRES | SDHCI_SPRD_DLL_WAIT_CNT;
	sdhci_writel(host, tmp, SDHCI_SPRD_REG_32_DLL_CFG);
	/* wait 1ms */
	usleep_range(1000, 1250);

	tmp = sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_CFG);
	tmp |= SDHCI_SPRD_DLL_EN;
	sdhci_writel(host, tmp, SDHCI_SPRD_REG_32_DLL_CFG);
	/* wait 1ms */
	usleep_range(1000, 1250);

	while ((sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_STS0) &
		SDHCI_SPRD_DLL_LOCKED) != SDHCI_SPRD_DLL_LOCKED) {
		pr_warn("%s: dpll locked fail!\n", mmc_hostname(host->mmc));
		pr_info("%s: DLL_STS0 : 0x%x, DLL_CFG : 0x%x\n",
			 mmc_hostname(host->mmc),
			 sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_STS0),
			 sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_CFG));
		pr_info("%s: DLL_DLY : 0x%x, DLL_STS1 : 0x%x\n",
			 mmc_hostname(host->mmc),
			 sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_DLY),
			 sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_STS1));
		mdelay(1);
	}

	pr_info("%s: dpll locked done\n", mmc_hostname(host->mmc));
}

static void sdhci_sprd_set_clock(struct sdhci_host *host, unsigned int clock)
{
	bool en = false, clk_changed = false;

	if (clock == 0) {
		sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);
	} else if (clock != host->clock) {
		sdhci_sprd_sd_clk_off(host);
		_sdhci_sprd_set_clock(host, clock);

		if (clock <= 400000)
			en = true;
		sdhci_sprd_set_dll_invert(host, SDHCI_SPRD_BIT_CMD_DLY_INV |
					  SDHCI_SPRD_BIT_POSRD_DLY_INV, en);
		clk_changed = true;
	} else {
		_sdhci_sprd_set_clock(host, clock);
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
	return 400000;
}

static void sdhci_sprd_set_uhs_signaling(struct sdhci_host *host,
					 unsigned int timing)
{
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	struct mmc_host *mmc = host->mmc;
	u32 *p = sprd_host->phy_delay;
	u16 ctrl_2;

	if (timing == host->timing)
		return;

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
		ctrl_2 |= SDHCI_SPRD_CTRL_HS400;
		break;
	default:
		break;
	}

	sdhci_writew(host, ctrl_2, SDHCI_HOST_CONTROL2);

	if (!mmc->ios.enhanced_strobe)
		sdhci_writel(host, p[timing], SDHCI_SPRD_REG_32_DLL_DLY);
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
	usleep_range(10, 20);

	val |= SDHCI_HW_RESET_CARD;
	writeb_relaxed(val, host->ioaddr + SDHCI_SOFTWARE_RESET);
	usleep_range(300, 500);
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
	/* Validate if the request was from software queue firstly. */
	if (mmc_hsq_finalize_request(host->mmc, mrq))
		return;

	 mmc_request_done(host->mmc, mrq);
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


static int sdhci_sprd_execute_tuning(struct mmc_host *mmc, u32 opcode)
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

	sdhci_reset(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA);

	dll_cfg = sdhci_readl(host, SDHCI_SPRD_REG_32_DLL_CFG);
	dll_cfg &= ~(0xf << 24);
	sdhci_writel(host, dll_cfg, SDHCI_SPRD_REG_32_DLL_CFG);
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
	do {
		if (host->flags & SDHCI_HS400_TUNING) {
			dll_dly &= ~SDHCI_SPRD_CMD_DLY_MASK;
			dll_dly |= (i << 8);
		} else {
			dll_dly &= ~(SDHCI_SPRD_CMD_DLY_MASK |
				     SDHCI_SPRD_POSRD_DLY_MASK);
			dll_dly |= (((i << 8) & SDHCI_SPRD_CMD_DLY_MASK) |
				    ((i << 16) & SDHCI_SPRD_POSRD_DLY_MASK));
		}
		sdhci_writel(host, dll_dly, SDHCI_SPRD_REG_32_DLL_DLY);
		pr_debug("%s: dll_dly 0x%08x\n", mmc_hostname(mmc), dll_dly);

		value = !mmc_send_tuning(mmc, opcode, NULL);

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
	range_count = sprd_calc_tuning_range(sprd_host, value_t);

	if (range_count == 0) {
		pr_warn("%s: all tuning phases fail!\n", mmc_hostname(mmc));
		err = -EIO;
		goto out;
	}

	if ((range_count > 1) && first_vl && value) {
		ranges[0].start = ranges[range_count - 1].start;
		range_count--;

		if (ranges[0].end >= mid_dll_cnt)
			ranges[0].end = mid_dll_cnt;
	}

	for (i = 0; i < range_count; i++) {
		int len = (ranges[i].end - ranges[i].start + 1);

		if (len < 0)
			len += dll_cnt;

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

	mid_step = ranges[longest_range].start + longest_range_len / 2;
	mid_step %= dll_cnt;

	dll_cfg |= 0xf << 24;
	sdhci_writel(host, dll_cfg, SDHCI_SPRD_REG_32_DLL_CFG);

	if (mid_step <= dll_cnt)
		final_phase = (mid_step * 256) / dll_cnt;
	else
		final_phase = 0xff;

	if (host->flags & SDHCI_HS400_TUNING) {
		p[mmc->ios.timing] &= ~SDHCI_SPRD_CMD_DLY_MASK;
		p[mmc->ios.timing] |= (final_phase << 8);
	} else {
		p[mmc->ios.timing] &= ~(SDHCI_SPRD_CMD_DLY_MASK |
					SDHCI_SPRD_POSRD_DLY_MASK);
		p[mmc->ios.timing] |=
			(((final_phase << 8) & SDHCI_SPRD_CMD_DLY_MASK) |
			 ((final_phase << 16) & SDHCI_SPRD_POSRD_DLY_MASK));
	}

	pr_info("%s: the best step %d, phase 0x%02x, delay value 0x%08x\n",
		mmc_hostname(mmc), mid_step, final_phase, p[mmc->ios.timing]);
	sdhci_writel(host, p[mmc->ios.timing], SDHCI_SPRD_REG_32_DLL_DLY);
	err = 0;

out:
	host->flags &= ~SDHCI_HS400_TUNING;
	kfree(ranges);
	kfree(value_t);

	return err;
}


static struct sdhci_ops sdhci_sprd_ops = {
	.read_l = sdhci_sprd_readl,
	.write_l = sdhci_sprd_writel,
	.write_b = sdhci_sprd_writeb,
	.set_clock = sdhci_sprd_set_clock,
	.get_max_clock = sdhci_sprd_get_max_clock,
	.get_min_clock = sdhci_sprd_get_min_clock,
	.set_bus_width = sdhci_set_bus_width,
	.reset = sdhci_reset,
	.set_uhs_signaling = sdhci_sprd_set_uhs_signaling,
	.hw_reset = sdhci_sprd_hw_reset,
	.get_max_timeout_count = sdhci_sprd_get_max_timeout_count,
	.get_ro = sdhci_sprd_get_ro,
	.request_done = sdhci_sprd_request_done,
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
	if (host->version >= SDHCI_SPEC_410 &&
	    mrq->sbc && (mrq->sbc->arg & SDHCI_SPRD_ARG2_STUFF) &&
	    (host->flags & SDHCI_AUTO_CMD23))
		host->flags &= ~SDHCI_AUTO_CMD23;
}

static void sdhci_sprd_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	sdhci_sprd_check_auto_cmd23(mmc, mrq);

	sdhci_request(mmc, mrq);
}

static int sdhci_sprd_request_atomic(struct mmc_host *mmc,
				      struct mmc_request *mrq)
{
	sdhci_sprd_check_auto_cmd23(mmc, mrq);

	return sdhci_request_atomic(mmc, mrq);
}

static int sdhci_sprd_voltage_switch(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);
	int ret;

	if (!IS_ERR(mmc->supply.vqmmc)) {
		ret = mmc_regulator_set_vqmmc(mmc, ios);
		if (ret) {
			pr_err("%s: Switching signalling voltage failed\n",
			       mmc_hostname(mmc));
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
		/* fall-through */
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

reset:
	/* Wait for 300 ~ 500 us for pin state stable */
	usleep_range(300, 500);
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

static int sdhci_sprd_probe(struct platform_device *pdev)
{
	struct sdhci_host *host;
	struct sdhci_sprd_host *sprd_host;
	struct mmc_hsq *hsq;
	struct clk *clk;
	int ret = 0;

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

	clk = devm_clk_get(&pdev->dev, "sdio");
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		goto pltfm_free;
	}
	sprd_host->clk_sdio = clk;
	sprd_host->base_rate = clk_get_rate(sprd_host->clk_sdio);
	if (!sprd_host->base_rate)
		sprd_host->base_rate = SDHCI_SPRD_CLK_DEF_RATE;

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
		goto clk_disable;

	ret = clk_prepare_enable(sprd_host->clk_1x_enable);
	if (ret)
		goto clk_disable2;

	ret = clk_prepare_enable(sprd_host->clk_2x_enable);
	if (ret)
		goto clk_disable2;

	sdhci_sprd_init_config(host);
	host->version = sdhci_readw(host, SDHCI_HOST_VERSION);
	sprd_host->version = ((host->version & SDHCI_VENDOR_VER_MASK) >>
			       SDHCI_VENDOR_VER_SHIFT);

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

	ret = sdhci_setup_host(host);
	if (ret)
		goto pm_runtime_disable;

	sprd_host->flags = host->flags;

	hsq = devm_kzalloc(&pdev->dev, sizeof(*hsq), GFP_KERNEL);
	if (!hsq) {
		ret = -ENOMEM;
		goto err_cleanup_host;
	}

	ret = mmc_hsq_init(hsq, host->mmc);
	if (ret)
		goto err_cleanup_host;

	ret = __sdhci_add_host(host);
	if (ret)
		goto err_cleanup_host;

	pm_runtime_mark_last_busy(&pdev->dev);
	pm_runtime_put_autosuspend(&pdev->dev);

	return 0;

err_cleanup_host:
	sdhci_cleanup_host(host);

pm_runtime_disable:
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);

	clk_disable_unprepare(sprd_host->clk_1x_enable);
	clk_disable_unprepare(sprd_host->clk_2x_enable);

clk_disable2:
	clk_disable_unprepare(sprd_host->clk_enable);

clk_disable:
	clk_disable_unprepare(sprd_host->clk_sdio);

pltfm_free:
	sdhci_pltfm_free(pdev);
	return ret;
}

static int sdhci_sprd_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_sprd_host *sprd_host = TO_SPRD_HOST(host);

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
		return ret;

	ret = clk_prepare_enable(sprd_host->clk_enable);
	if (ret)
		goto clk_2x_disable;

	ret = clk_prepare_enable(sprd_host->clk_sdio);
	if (ret)
		goto clk_disable;

	sdhci_runtime_resume_host(host, 1);
	mmc_hsq_resume(host->mmc);

	return 0;

clk_disable:
	clk_disable_unprepare(sprd_host->clk_enable);

clk_2x_disable:
	clk_disable_unprepare(sprd_host->clk_2x_enable);
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
		.of_match_table = of_match_ptr(sdhci_sprd_of_match),
		.pm = &sdhci_sprd_pm_ops,
	},
};
module_platform_driver(sdhci_sprd_driver);

MODULE_DESCRIPTION("Spreadtrum sdio host controller r11 driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sdhci-sprd-r11");
