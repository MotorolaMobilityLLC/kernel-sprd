// SPDX-License-Identifier: GPL-2.0
//
// Secure Digital Host Controller
//
// Copyright (C) 2023 Spreadtrum, Inc.
// Author: Wenchao Chen <wenchao.chen@unisoc.com>
#include <linux/moduleparam.h>
#include <linux/export.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/fault-inject.h>
#include <linux/proc_fs.h>
#include <linux/profile.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>

#include "../core/core.h"
#include "../core/card.h"
#include "../core/host.h"
#include "../core/mmc_ops.h"
#include "sdhci.h"

#include "sdhci-sprd-debug.h"
#include "sdhci-sprd-debug.c"

#define SPRD_SPEED_MODE_NAME_MAX	20
#define SPRD_SPEED_MODE_NAME_MIN	2

#define MMC_TIMING 0
#define SD_TIMING 1
#define NOT_SUPPORT 3

bool debug_en;

struct mmc_speed_config {
	char *name;
	u8 support; /* support: sd or mmc */
	u32 caps; /* sd: need modify host caps */
	u32 type; /* mmc: need modify mmc_avail_type */
};

static const struct mmc_speed_config mmc_speed[] = {
	{"LEGACY", SD_TIMING, MMC_CAP_SD_HIGHSPEED, 0}, /* MMC_TIMING_LEGACY: 0 */
	{"HS", MMC_TIMING, 0, EXT_CSD_CARD_TYPE_HS}, /* MMC_TIMING_MMC_HS: 1 */
	{"HS", SD_TIMING, MMC_CAP_SD_HIGHSPEED, 0}, /* MMC_TIMING_SD_HS: 2 */
	{"SDR12", NOT_SUPPORT, MMC_CAP_UHS_SDR12, 0}, /* MMC_TIMING_UHS_SDR12: 3 */
	{"SDR25", NOT_SUPPORT, MMC_CAP_UHS_SDR25, 0}, /* MMC_TIMING_UHS_SDR25: 4 */
	{"SDR50", SD_TIMING, MMC_CAP_UHS_SDR50, 0}, /* MMC_TIMING_UHS_SDR50: 5 */
	{"SDR104", SD_TIMING, MMC_CAP_UHS_SDR104, 0}, /* MMC_TIMING_UHS_SDR104: 6 */
	{"DDR50", NOT_SUPPORT, MMC_CAP_DDR, 0}, /* MMC_TIMING_UHS_DDR50: 7 */
	{"DDR52", MMC_TIMING, 0, EXT_CSD_CARD_TYPE_DDR_52}, /* MMC_TIMING_MMC_DDR52: 8 */
	{"HS200", MMC_TIMING, 0, EXT_CSD_CARD_TYPE_HS200}, /* MMC_TIMING_MMC_HS200: 9 */
	{"HS400", MMC_TIMING, 0, EXT_CSD_CARD_TYPE_HS400}, /* MMC_TIMING_MMC_HS400: 10 */
	{"HS400ES", MMC_TIMING, 0, EXT_CSD_CARD_TYPE_HS400ES}, /* add new define HS400_ES: 11 */
};

static int sdhci_sprd_set_timing_show(struct seq_file *file, void *data)
{
	struct mmc_host *host = file->private;
	u8 timing = host->ios.enhanced_strobe ? host->ios.timing + 1 : host->ios.timing;
	static const char * const mmc_select_mode[] = {
		"support select HS, DDR52, HS200, HS400, HS400ES\n", /* EMMC */
		"support select LEGACY, HS, SDR50, SDR104\n" /* SD */
	};

	seq_printf(file, "%s current speed is: [%s], %s\n",
		mmc_hostname(host), mmc_speed[timing].name, mmc_select_mode[host->index]);

	return 0;
}

static int sdhci_sprd_set_timing_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdhci_sprd_set_timing_show, PDE_DATA(inode));
}

static bool sdhci_sprd_raw_cfg(struct mmc_host *host, u32 caps, u32 mmc_type)
{
	static u32 sd_caps, mmc_avail_type; /* record mmc default config */

	if (mmc_card_sd(host->card)) {
		sd_caps |= host->caps;
		host->caps = sd_caps;
	} else if (mmc_card_mmc(host->card)) {
		mmc_avail_type |= host->card->mmc_avail_type;
		host->card->mmc_avail_type = mmc_avail_type;
	}

	if (!(sd_caps & caps) && !(mmc_avail_type & mmc_type))
		return false;

	return true;
}

static ssize_t sdhci_sprd_set_timing_write(struct file *filp, const char __user *ubuf,
				   size_t cnt, loff_t *ppos)
{
	struct mmc_host *host = PDE_DATA(file_inode(filp));
	char temp[SPRD_SPEED_MODE_NAME_MAX] = {0};
	bool flag = false;
	int i;

	if (!host->card || mmc_card_sdio(host->card) ||
		cnt > SPRD_SPEED_MODE_NAME_MAX || cnt < SPRD_SPEED_MODE_NAME_MIN)
		goto out;

	if (copy_from_user(temp, ubuf, cnt - 1))
		return -EFAULT;

	for (i = 0; i < ARRAY_SIZE(mmc_speed); i++) {
		if (mmc_speed[i].support != host->index)
			continue;
		if (flag) {
			host->card->mmc_avail_type &= ~mmc_speed[i].type;
			host->caps &= ~mmc_speed[i].caps;
		} else if (!strcmp(mmc_speed[i].name, temp)) {
			if (!sdhci_sprd_raw_cfg(host, mmc_speed[i].caps, mmc_speed[i].type))
				break;
			flag = true;
		}
	}

	if (!flag) {
		pr_err("%s does not support %s, set timing fail!\n", mmc_hostname(host), temp);
		goto out;
	}

	mmc_claim_host(host);
	host->bus_ops->hw_reset(host);
	mmc_release_host(host);

	pr_info("%s current speed is: [%s], set timing success!\n",
		mmc_hostname(host), temp);
out:
	return cnt;
}

static const struct proc_ops sdhci_sprd_set_timing_fops = {
	.proc_open = sdhci_sprd_set_timing_open,
	.proc_read = seq_read,
	.proc_write = sdhci_sprd_set_timing_write,
	.proc_release = single_release,
};

static int sdhci_sprd_reset_show(struct seq_file *file, void *data)
{
	seq_puts(file, "triger\n");

	return 0;
}

static int sdhci_sprd_reset_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdhci_sprd_reset_show, inode->i_private);
}

static bool decode_state(const char *buf, size_t n)
{
	char *p;
	int len;

	p = memchr(buf, '\n', n);
	len = p ? p - buf : n;

	if (len == 6 && str_has_prefix(buf, "triger"))
		return true;

	return false;
}

static ssize_t sdhci_sprd_reset_write(struct file *filp, const char __user *ubuf,
				   size_t cnt, loff_t *ppos)
{
	struct mmc_host *host = filp->f_mapping->host->i_private;
	char temp[7];

	if (copy_from_user(&temp, ubuf, sizeof(temp)))
		return -EFAULT;

	if (!decode_state(temp, sizeof(temp)))
		return -EINVAL;

	if (!host->card)
		return -EOPNOTSUPP;

	mmc_claim_host(host);

	host->ops->hw_reset(host);

	mmc_release_host(host);

	return cnt;
}

static const struct file_operations sdhci_sprd_reset_fops = {
	.open = sdhci_sprd_reset_open,
	.read = seq_read,
	.write = sdhci_sprd_reset_write,
	.release = single_release,
};

void sdhci_sprd_add_host_debugfs(struct sdhci_host *host)
{
	struct mmc_host *mmc = host->mmc;

	if (!mmc->debugfs_root || (mmc->index > 0))
		return;

	debugfs_create_file_unsafe("hw_reset", 0600, mmc->debugfs_root, mmc,
				   &sdhci_sprd_reset_fops);
}

static int sdhci_sprd_debugen_show(struct seq_file *file, void *data)
{
	seq_printf(file, "%d\n", debug_en);

	return 0;
}

static int sdhci_sprd_debugen_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdhci_sprd_debugen_show, inode->i_private);
}

static ssize_t sdhci_sprd_debugen_write(struct file *filp, const char __user *ubuf,
				   size_t cnt, loff_t *ppos)
{
	char val;

	if (cnt <= 0)
		goto end;

	if (get_user(val, ubuf))
		return -EFAULT;

	if (val == '1')
		debug_en = 1;
	else
		debug_en = 0;

end:
	return cnt;
}

static const struct proc_ops sdhci_sprd_debugen_fops = {
	.proc_open = sdhci_sprd_debugen_open,
	.proc_read = seq_read,
	.proc_write = sdhci_sprd_debugen_write,
	.proc_release = single_release,
};

static int sdhci_sprd_debuginfo_show(struct seq_file *file, void *data)
{
	struct mmc_host *mmc = file->private;

	if (!mmc) {
		pr_err("no mmc");
		return 0;
	}

	seq_printf(file, "r= %lld.%lldM/s, w= %lld.%lldM/s\n", mmc_debug[mmc->index].rspeed,
		   mmc_debug[mmc->index].rspeed_mod, mmc_debug[mmc->index].wspeed,
		   mmc_debug[mmc->index].wspeed_mod);

	return 0;
}

static int sdhci_sprd_debuginfo_open(struct inode *inode, struct file *file)
{
	return single_open(file, sdhci_sprd_debuginfo_show, PDE_DATA(inode));
}

static const struct proc_ops sdhci_sprd_debuginfo_fops = {
	.proc_open = sdhci_sprd_debuginfo_open,
	.proc_read = seq_read,
	.proc_release = single_release,
};

void sdhci_sprd_add_host_debug(struct sdhci_host *host)
{
	static struct proc_dir_entry *debug_parent;
	static struct proc_dir_entry *debug_en_data;
	struct mmc_host *mmc = host->mmc;

	debug_parent = proc_mkdir(mmc_hostname(host->mmc), NULL);
	if (!debug_parent) {
		pr_err("%s: failed to create sprd_host_debug proc entry\n",
			__func__);

		goto err;
	}

	if (host->mmc->index == 0) {
		debug_en_data = proc_create_data("debug_enable", 0660, debug_parent,
			&sdhci_sprd_debugen_fops, NULL);
		if (!debug_en_data) {
			pr_err("%s: failed to create node: /proc/%s/debug_enable\n",
				__func__, mmc_hostname(mmc));

			goto err;
		}
	}

	if ((host->mmc->index == 0) || (host->mmc->index == 1)) {
		debug_en_data = proc_create_data("debug_info", 0664, debug_parent,
			&sdhci_sprd_debuginfo_fops, mmc);
		if (!debug_en_data) {
			pr_err("%s: failed to create node: /proc/%s/debug_enable\n",
				__func__, mmc_hostname(mmc));

			goto err;
		}
	}

	if (host->mmc->index != 2) {
		debug_en_data = proc_create_data("set_timing", 0660, debug_parent,
			&sdhci_sprd_set_timing_fops, mmc);
		if (!debug_en_data) {
			pr_err("%s: failed to create node: /proc/%s/set_timing\n",
				__func__, mmc_hostname(mmc));

			goto err;
		}
	}

	return;
err:
	//call the function will cause gki error
	remove_proc_subtree(mmc_hostname(mmc), NULL);
}
