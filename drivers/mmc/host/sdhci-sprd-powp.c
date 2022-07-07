// SPDX-License-Identifier: GPL-2.0
//
// Secure Digital Host Controller
//
// Copyright (C) 2022 Spreadtrum, Inc.
#include <linux/bug.h>
#include <linux/errno.h>
#include <linux/mmc/ioctl.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/types.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include "sdhci-sprd-powp.h"

static inline void get_fixed_wp_params(unsigned long long *start,
	unsigned long long size, unsigned int wp_grp_size, unsigned int *cnt)
{
	unsigned long long end;

	end = *start + size;

	if (*start % wp_grp_size)
		*start = *start + (wp_grp_size - *start % wp_grp_size);

	/*To make sure at least one aligned WP_GRP in the partition address range*/
	if ((*start + wp_grp_size) > end) {
		pr_err("%s: partition is too small to set wp!\n", __func__);
		*cnt = 0;
	} else
		*cnt = (end - *start) / wp_grp_size;
}

static int emmc_set_usr_wp(struct mmc_card *card, unsigned char wp)
{
	struct mmc_host *host = card->host;
	struct mmc_command cmd = {0};
	struct mmc_request mrq = {NULL};

	/* clr usr_wp */
	cmd.opcode = MMC_SWITCH;
	cmd.arg = (MMC_SWITCH_MODE_CLEAR_BITS << 24) |
		(EXT_CSD_USR_WP << 16) |
		((EXT_CSD_USR_WP_EN_PERM_WP | EXT_CSD_USR_WP_EN_PWR_WP) << 8) |
		EXT_CSD_CMD_SET_NORMAL;
	cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	mrq.cmd = &cmd;
	mmc_wait_for_req(card->host, &mrq);
	if (cmd.error) {
		pr_err("%s: clr usr_wp fail, cmd error %d\n", mmc_hostname(host), cmd.error);
		return cmd.error;
	}

	/* set usr_wp*/
	cmd.arg = (MMC_SWITCH_MODE_SET_BITS << 24) |
		(EXT_CSD_USR_WP << 16) | (wp << 8) | EXT_CSD_CMD_SET_NORMAL;
	mmc_wait_for_req(card->host, &mrq);
	if (cmd.error) {
		pr_err("%s: set usr_wp fail, cmd error %d\n", mmc_hostname(host), cmd.error);
		return cmd.error;
	}

	return 0;
}

static int emmc_get_write_protect_part(struct mmc_card *card,
	u64 *start, u64 *size)
{
	struct mmc_host *host = card->host;
	struct device *dev = host->parent;
	struct device_node *np = dev->of_node;
	u32 data[2];
	int ret;

	ret = of_property_read_u32_array(np, "sprd,part-address-size", data, 2);
	if (ret) {
		pr_err("%s: no sprd,part-address-size setting in node\n", mmc_hostname(host));
		return ret;
	}

	*start = (u64)data[0];
	*size = (u64)data[1];

	pr_info("%s: protect address: 0x%llx, protect size: 0x%llx\n",
		mmc_hostname(host), *start, *size);

	return 0;
}

static int emmc_set_wp(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	struct mmc_command cmd = {0};
	struct mmc_request mrq = {NULL};
	unsigned long long start;
	unsigned long long size;
	unsigned int wp_grp_size;
	unsigned int cnt;
	unsigned int i;
	int err = 0;

	err = emmc_get_write_protect_part(card, &start, &size);
	if (err)
		return err;

	wp_grp_size = card->ext_csd.hc_erase_size *
			card->ext_csd.raw_hc_erase_gap_size;

	get_fixed_wp_params(&start, size, wp_grp_size, &cnt);

	mrq.cmd = &cmd;
	cmd.opcode = MMC_SET_WRITE_PROT;
	cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	for (i = 0; i < cnt; i++) {
		cmd.arg = start + i * wp_grp_size;
		mmc_wait_for_req(card->host, &mrq);
		if (cmd.error) {
			pr_err("%s: cmd error %d\n", mmc_hostname(host), cmd.error);
			return cmd.error;
		}
	}

	return 0;
}

static int emmc_set_wp_by_partitions(struct mmc_card *card, unsigned char wp_type)
{
	int err;
	struct mmc_host *host = card->host;

	err = emmc_set_usr_wp(card, wp_type);
	if (err)
		return err;

	err = emmc_set_wp(card);
	if (err) {
		pr_err("%s: set partition protect_data wp is failed!\n",
			mmc_hostname(host));
		return err;
	}

	return err;
}

static int emmc_check_wp_fn(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	struct device *dev = host->parent;
	struct device_node *np = dev->of_node;
	int ret;
	const char *name;

	ret = of_property_read_string(np, "sprd,wp_fn", &name);
	if (ret) {
		pr_err("%s: can not read the property of sprd wp_fn\n",
			mmc_hostname(host));
		return ret;
	}

	if (strcmp(name, "enable") == 0)
		return 0;

	return -1;
}

int set_power_on_write_protect(struct mmc_card *card)
{
	int err;

	err = emmc_check_wp_fn(card);
	if (err)
		return err;

	err = emmc_set_wp_by_partitions(card, EXT_CSD_USR_WP_EN_PWR_WP);

	return err;
}