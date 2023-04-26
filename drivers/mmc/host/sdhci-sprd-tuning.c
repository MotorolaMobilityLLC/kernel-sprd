// SPDX-License-Identifier: GPL-2.0
//
// Secure Digital Host Controller
//
// Copyright (C) 2022 Spreadtrum, Inc.
// Author: Wei Zheng <wei.zheng@unisoc.com>
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
#include "sdhci-sprd-tuning.h"
#include "../core/core.h"
#include "../core/mmc_ops.h"
#include "../core/card.h"

#define MMC_TUNING_RANGE 512
#define SDHCI_SPRD_DLL_DLY 0x204

struct sprd_host_tuning_info {
	u32 cur_idx;
	u32 send_cmd_result[MMC_TUNING_RANGE];
	u32 response[MMC_TUNING_RANGE];
	u32 present_state[MMC_TUNING_RANGE];
	u32 int_status[MMC_TUNING_RANGE];
	u32 dll_delay[MMC_TUNING_RANGE];
};

struct sprd_host_tuning_info sprd_tuning_info[3];

void sprd_host_tuning_info_dump(struct sdhci_host *host)
{
	struct sprd_host_tuning_info *info;
	u32 i;

	info = &sprd_tuning_info[host->mmc->index];

	for (i = 0; i <= info->cur_idx; i++) {
		pr_err("%s: %3dth tuning: %s 0x10:%08x 0x24:%08x 0x30:%08x 0x204:%08x\n",
			mmc_hostname(host->mmc),
			i,
			info->send_cmd_result[i] == 1 ? "success" : " failed",
			info->response[i],
			info->present_state[i],
			info->int_status[i],
			info->dll_delay[i]);
	}
}

void sprd_host_tuning_info_update_index(struct sdhci_host *host, int index)
{
	struct sprd_host_tuning_info *info;

	info = &sprd_tuning_info[host->mmc->index];

	info->cur_idx = index;
}

void sprd_host_tuning_info_update_intstatus(struct sdhci_host *host)
{
	struct sprd_host_tuning_info *info;
	u32 int_status;

	info = &sprd_tuning_info[host->mmc->index];

	int_status = sdhci_readl(host, SDHCI_INT_STATUS);
	if (int_status & SDHCI_INT_ERROR_MASK)
		info->send_cmd_result[info->cur_idx] = 0;
	else
		info->send_cmd_result[info->cur_idx] = 1;

	info->response[info->cur_idx] = sdhci_readl(host, SDHCI_RESPONSE);
	info->present_state[info->cur_idx] = sdhci_readl(host, SDHCI_PRESENT_STATE);
	info->int_status[info->cur_idx] = sdhci_readl(host, SDHCI_INT_STATUS);
	info->dll_delay[info->cur_idx] = sdhci_readl(host, SDHCI_SPRD_DLL_DLY);
}

int mmc_send_tuning_cmd(struct mmc_host *host)
{
	struct mmc_command cmd = {};

	cmd.opcode = MMC_SET_BLOCKLEN;
	cmd.arg = 512;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC;

	return mmc_wait_for_cmd(host, &cmd, 1);
}

int mmc_send_tuning_read(struct mmc_host *host)
{
	struct mmc_request mrq = {};
	struct mmc_command cmd = {};
	struct mmc_data data = {};
	struct scatterlist sg;
	int size = 512, err = 0;
	u8 *data_buf;

	data_buf = kzalloc(size, GFP_KERNEL);
	if (!data_buf)
		return -ENOMEM;

	mrq.cmd = &cmd;
	mrq.data = &data;

	cmd.opcode = MMC_READ_SINGLE_BLOCK;
	cmd.flags = MMC_RSP_R1B | MMC_CMD_ADTC;

	data.blksz = size;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.blk_addr = 0;
	data.timeout_ns = 100000000;
	data.timeout_clks = 0;

	data.sg = &sg;
	data.sg_len = 1;
	sg_init_one(&sg, data_buf, size);

	mmc_wait_for_req(host, &mrq);

	if (cmd.error) {
		err = cmd.error;
		goto out;
	}

	if (data.error) {
		err = data.error;
		goto out;
	}

out:
	kfree(data_buf);
	return err;
}
