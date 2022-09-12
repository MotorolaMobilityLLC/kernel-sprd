// SPDX-License-Identifier: GPL-2.0
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
	if (!data_buf) {
		pr_err("%s: data_buf create false\n", mmc_hostname(host));
		return -ENOMEM;
	}
	mrq.cmd = &cmd;
	mrq.data = &data;

	cmd.opcode = MMC_READ_SINGLE_BLOCK;
	cmd.flags = MMC_RSP_R1B | MMC_CMD_ADTC;

	data.blksz = size;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.blk_addr = 0;


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
