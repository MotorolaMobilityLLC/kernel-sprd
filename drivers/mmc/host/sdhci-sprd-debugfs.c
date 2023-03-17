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

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>

#include "../core/core.h"
#include "../core/card.h"
#include "../core/host.h"
#include "../core/mmc_ops.h"
#include "sdhci.h"

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
