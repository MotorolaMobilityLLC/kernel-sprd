// SPDX-License-Identifier: GPL-2.0
//
// Secure Digital Host Controller
//
// Copyright (C) 2022 Spreadtrum, Inc.
// Author: Wei Zheng <wei.zheng@unisoc.com>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/hdreg.h>
#include <linux/kdev_t.h>
#include <linux/blkdev.h>
#include <linux/cdev.h>
#include <linux/ktime.h>
#include <linux/reboot.h>
#include <linux/mmc/ioctl.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/uaccess.h>
#include <trace/hooks/mmc.h>

#include "../core/core.h"
#include "../core/mmc_ops.h"
#include "../core/card.h"
#include "../core/queue.h"
#include "sdhci.h"
#include "mmc_swcq.h"
#include <linux/mmc/sdio_func.h>

#define MMC_ARRAY_SIZE 14	/* 2^(14-2) = 4096ms/blocks */
#define MMC_SPEED_0M 0
#define MMC_SPEED_1M 100
/*
 * convert ms to index: (ilog2(ms) + 1)
 * array[6]++ means the time is: 32ms <= time < 64ms
 * [0] [1] [2] [3] [4] [5]  [6]  [7]  [8]   [9]   [10]  [11]   [12]   [13]
 * 0ms 1ms 2ms 4ms 8ms 16ms 32ms 64ms 128ms 256ms 512ms 1024ms 2048ms 4096ms
 *
 * convert block_length to index: (ilog2(block_length) + 1)
 * array[6]++ means the block_length  is: 32blocks <= block_length < 64blocks
 * [0]     [1]      [2]      [3]      [4]      [5]       [6]       [7]
 * 0block  1blocks  2blocks  4blocks  8blocks  16blocks  32blocks  64blocks
 * [8]        [9]        [10]       [11]        [12]        [13]
 * 128blocks  256blocks  512blocks  1024blocks  2048blocks  4096blocks
 */
struct mmc_debug_info {
	char name[8];
	u32 cmd;
	u32 arg;
	u32 blocks;
	u32 intmask;
	u32 cnt_time;
	u64 read_total_blocks;
	u64 write_total_blocks;
	struct mmc_request *mrq;
	ktime_t start_time;
	ktime_t end_time;
	unsigned long read_total_time;
	unsigned long write_total_time;
	unsigned long cmd_2_end[MMC_ARRAY_SIZE];
	unsigned long data_2_end[MMC_ARRAY_SIZE];
	unsigned long block_len[MMC_ARRAY_SIZE];
};

#define rq_log(array, fmt, ...) \
	pr_err(fmt ":%5ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld %4ld\n", \
		##__VA_ARGS__, array[0], array[1], array[2], array[3], \
		array[4], array[5], array[6], array[7], array[8], \
		array[9], array[10], array[11], array[12], array[13])

static struct mmc_debug_info mmc_debug[3] = {
	{.name = "mmc0"}, {.name = "mmc1"}, {.name = "mmc2"}
};

static void mmc_debug_is_emmc(struct sdhci_host *host, struct mmc_debug_info *info)
{
	bool flag = true;

	if (HOST_IS_EMMC_TYPE(host->mmc) && info->mrq &&
		host->mmc->cqe_ops->cqe_timeout)
		host->mmc->cqe_ops->cqe_timeout(host->mmc, info->mrq, &flag);
}

static void mmc_debug_print(struct mmc_debug_info *info, struct sdhci_host *host)
{
	u64 read_speed = 0;
	u64 write_speed = 0;
	u64 wspeed_temp = 0, rspeed_temp = 0;
	u64 wspeed_mod = 0, rspeed_mod = 0;

	if ((ktime_to_ms(ktime_get()) - info->cnt_time) > (10000ULL)) {
		/* calculate read/write speed */
		if (info->read_total_time) {
			read_speed = info->read_total_blocks * 50000;
			do_div(read_speed, info->read_total_time);
		}
		if (info->write_total_time) {
			write_speed = info->write_total_blocks * 50000;
			do_div(write_speed, info->write_total_time);
		}

		/* print debug messages of mmc io */
		rq_log(info->cmd_2_end, "|__c2e%9s", info->name);
		rq_log(info->data_2_end, "|__d2e%9s", info->name);
		rq_log(info->block_len, "|__blocks%6s", info->name);
		rspeed_temp = read_speed;
		wspeed_temp = write_speed;
		rspeed_mod = do_div(rspeed_temp, 100);
		wspeed_mod = do_div(wspeed_temp, 100);
		pr_err("|__speed%7s: r= %lld.%lldM/s, w= %lld.%lldM/s, r_blk= %lld, w_blk= %lld\n",
			info->name, rspeed_temp, rspeed_mod, wspeed_temp, wspeed_mod,
			info->read_total_blocks, info->write_total_blocks);
		if ((read_speed > MMC_SPEED_0M && read_speed < MMC_SPEED_1M) ||
			(write_speed > MMC_SPEED_0M && write_speed < MMC_SPEED_1M))
			mmc_debug_is_emmc(host, info);

		/* clear mmc_debug structure except name*/
		memset(&info->cmd, 0, sizeof(struct mmc_debug_info) - 8);
		info->cnt_time = ktime_to_ms(ktime_get());
	}
}

static void mmc_debug_calc(struct mmc_debug_info *info)
{
	u32 cmd = info->cmd;

	/* judge sdio read/write cmd type */
	if (!strcmp(info->name, "mmc2") &&
		(cmd == SD_IO_RW_DIRECT || cmd == SD_IO_RW_EXTENDED))
		cmd = info->arg >> 31 ? MMC_EXECUTE_WRITE_TASK : MMC_EXECUTE_READ_TASK;


	/* record read/write info */
	if (cmd == MMC_READ_MULTIPLE_BLOCK || cmd == MMC_EXECUTE_READ_TASK ||
		cmd == MMC_READ_SINGLE_BLOCK) {
		info->read_total_blocks += info->blocks;
		info->read_total_time += ktime_to_us(info->end_time - info->start_time);
	} else if (cmd == MMC_WRITE_BLOCK || cmd == MMC_EXECUTE_WRITE_TASK ||
		cmd == MMC_WRITE_MULTIPLE_BLOCK) {
		info->write_total_blocks += info->blocks;
		info->write_total_time += ktime_to_us(info->end_time - info->start_time);
	}
}

static void mmc_debug_handle_rsp(struct sdhci_host *host, struct mmc_debug_info *info)
{
	u8 index;
	u32 msecs;

	if (info->intmask & SDHCI_INT_RESPONSE) {
		/* cmd interrupt respond */
		info->end_time = ktime_get();
		msecs = ktime_to_ms(info->end_time - info->start_time);
		index = msecs > 0 ? min((MMC_ARRAY_SIZE - 1), ilog2(msecs) + 1) : 0;
		info->cmd_2_end[index]++;
		if (index >= 11) {
			pr_err("%s: cmd rsp over 1s! cmd= %d blk= %d arg= %x mrq= %p rsp= %x\n",
			info->name, info->cmd, info->blocks, info->arg,
			info->mrq, sdhci_readl(host, SDHCI_RESPONSE));
			mmc_debug_is_emmc(host, info);
		}
	}

	if (info->intmask & SDHCI_INT_DATA_END) {
		/* data interrupt respond */
		info->end_time = ktime_get();
		msecs = ktime_to_ms(info->end_time - info->start_time);
		index = info->blocks > 0 ? min((MMC_ARRAY_SIZE - 1), ilog2(info->blocks) + 1) : 0;
		info->block_len[index]++;
		index = msecs > 0 ? min((MMC_ARRAY_SIZE - 1), ilog2(msecs) + 1) : 0;
		info->data_2_end[index]++;
		if (index >= 11) {
			pr_err("%s: data rsp over 1s! cmd= %d blk= %d arg= %x mrq= 0x%p\n",
				info->name, info->cmd, info->blocks, info->arg, info->mrq);
			mmc_debug_is_emmc(host, info);
		}
		mmc_debug_calc(info);
		info->start_time = 0;
		mmc_debug_print(info, host);
	}
}

void mmc_debug_update(struct sdhci_host *host, struct mmc_command *cmd, u32 intmask)
{
	struct mmc_debug_info *info = NULL;
	u8 i;

	/* select mmc type */
	for (i = 0; i <= 2; i++) {
		if (!strcmp(mmc_hostname(host->mmc), mmc_debug[i].name))
			info = &mmc_debug[i];
	}

	if (!info)
		return;

	if (!intmask && cmd) {
		/* send cmd */
		info->cmd = cmd->opcode;
		info->arg = cmd->arg;
		info->mrq = cmd->mrq;
		info->blocks = cmd->mrq->data ? cmd->mrq->data->blocks : 0;
		info->start_time = ktime_get();

		return;
	}

	/* handle mmc interrupt */
	info->intmask = intmask;
	if (!(intmask & SDHCI_INT_ERROR_MASK) && info->start_time)
		mmc_debug_handle_rsp(host, info);
}
EXPORT_SYMBOL(mmc_debug_update);

