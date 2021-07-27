/*
 * linux/drivers/mmc/host/sprd-dbg.c - Secure Digital Host Controller
 * Interface driver debug
 *
 * Copyright (C) 2018 Spreadtrum corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 */

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/io.h>
#include <linux/scatterlist.h>
#include <linux/gpio.h>
#include <linux/sched/clock.h>
#include "sprd-sdhcr11.h"
#include "../core/core.h"
#include "../core/card.h"
#include "../core/mmc_ops.h"



/*
 * snprintf may return a value of size or "more" to indicate
 * that the output was truncated, thus be careful of "more"
 * case.
 */
#define SPREAD_PRINTF(buff, size, evt, fmt, args...) \
do { \
	if (buff && size && *(size)) { \
		unsigned long var = snprintf(*(buff), *(size), fmt, ##args); \
		if (var > 0) { \
			if (var > *(size)) \
				var = *(size); \
			*(size) -= var; \
			*(buff) += var; \
		} \
	} \
	if (evt) \
		seq_printf(evt, fmt, ##args); \
	if (!buff && !evt) { \
		pr_info(fmt, ##args); \
	} \
} while (0)

#define dbg_max_cnt (100)
#ifdef CONFIG_SPRD_MMC_DEBUG
#ifdef SPRD_MMC_LOW_IO_DEBUG
#define dbg_max_cnt_low_io (5000)
#define criterion_low_io (10 * 1024) /* unit: KB/s */
#endif
#define SPRD_AEE_BUFFER_SIZE (300 * 1024)
struct dbg_run_host_log {
	unsigned long long time_sec;
	unsigned long long time_usec;
	int type;
	int cmd;
	int arg;
	int cpu;
	unsigned long active_reqs;
	int skip;
};

#ifdef SPRD_MMC_LOW_IO_DEBUG
struct dbg_run_host_log_low_io {
	int cmd;
	u32 address;
	unsigned long long size;
	unsigned long long time;
	unsigned long long time_diff;
	int continuous_count;
};
#endif

struct dbg_task_log {
	u32 address;
	unsigned long long size;
};
struct dbg_dma_cmd_log {
	unsigned long long time;
	int cmd;
	int arg;
};

static struct dbg_run_host_log dbg_run_host_log_dat[dbg_max_cnt];

#ifdef SPRD_MMC_LOW_IO_DEBUG
static struct dbg_run_host_log_low_io
	dbg_run_host_log_dat_low_io[dbg_max_cnt_low_io];
static int dbg_host_cnt_low_io;
#endif

static struct dbg_dma_cmd_log dbg_dma_cmd_log_dat;
static struct dbg_task_log dbg_task_log_dat[32];
char sprd_aee_buffer[SPRD_AEE_BUFFER_SIZE];
static int dbg_host_cnt;

static unsigned int printk_cpu_test = UINT_MAX;
struct sprd_sdhc_host *host_emmc;

/*
 * type 0: cmd; type 1: rsp; type 3: dma end
 * when type 3: arg 0: no data crc error; arg 1: data crc error
 * @cpu, current CPU ID
 * @reserved, userd for softirq dump "data_active_reqs"
 */
inline void __dbg_add_host_log(struct mmc_host *mmc, int type,
			int cmd, int arg, int cpu, unsigned long reserved)
{
	unsigned long long t, tn;
	unsigned long long nanosec_rem;
	static int last_cmd, last_arg, skip;
	int l_skip = 0;
	struct sprd_sdhc_host *host = mmc_priv(mmc);
	static int tag = -1;
#ifdef SPRD_MMC_LOW_IO_DEBUG
	static int continuous_count_low_io;
#endif

	/* only log emmc */
	if (strcmp(host->device_name, "sdio_emmc") != 0)
		return;

	t = cpu_clock(printk_cpu_test);

switch (type) {
	case 0: /* normal - cmd */
		tn = t;
		nanosec_rem = do_div(t, 1000000000)/1000;
		if (cmd == 44) {
			tag = (arg >> 16) & 0x1f;
			dbg_task_log_dat[tag].size = arg & 0xffff;
		} else if (cmd == 45) {
			dbg_task_log_dat[tag].address = arg;
		} else if (cmd == 46 || cmd == 47) {
			dbg_dma_cmd_log_dat.time = tn;
			dbg_dma_cmd_log_dat.cmd = cmd;
			dbg_dma_cmd_log_dat.arg = arg;
		}

		dbg_run_host_log_dat[dbg_host_cnt].time_sec = t;
		dbg_run_host_log_dat[dbg_host_cnt].time_usec = nanosec_rem;
		dbg_run_host_log_dat[dbg_host_cnt].type = type;
		dbg_run_host_log_dat[dbg_host_cnt].cmd = cmd;
		dbg_run_host_log_dat[dbg_host_cnt].arg = arg;
		dbg_run_host_log_dat[dbg_host_cnt].skip = l_skip;
		dbg_host_cnt++;
		if (dbg_host_cnt >= dbg_max_cnt)
			dbg_host_cnt = 0;
		break;
	case 1: /* normal -rsp */
		nanosec_rem = do_div(t, 1000000000)/1000;
		/*skip log if last cmd rsp are the same*/
		if (last_cmd == cmd &&
			last_arg == arg && cmd == 13) {
			skip++;
			if (dbg_host_cnt == 0)
				dbg_host_cnt = dbg_max_cnt;
			/*remove type = 0, command*/
			dbg_host_cnt--;
			break;
		}
		last_cmd = cmd;
		last_arg = arg;
		l_skip = skip;
		skip = 0;

		dbg_run_host_log_dat[dbg_host_cnt].time_sec = t;
		dbg_run_host_log_dat[dbg_host_cnt].time_usec = nanosec_rem;
		dbg_run_host_log_dat[dbg_host_cnt].type = type;
		dbg_run_host_log_dat[dbg_host_cnt].cmd = cmd;
		dbg_run_host_log_dat[dbg_host_cnt].arg = arg;
		dbg_run_host_log_dat[dbg_host_cnt].skip = l_skip;
		dbg_host_cnt++;
		if (dbg_host_cnt >= dbg_max_cnt)
			dbg_host_cnt = 0;
		break;

#ifdef SPRD_MMC_LOW_IO_DEBUG
	case 3:
		/*
		 * try to reduce executing time in case 3 to keep performance
		 * not to drop.
		 */
		if (dbg_dma_cmd_log_dat.cmd) {
			dbg_run_host_log_dat_low_io[dbg_host_cnt_low_io].cmd
				= dbg_dma_cmd_log_dat.cmd;
			dbg_dma_cmd_log_dat.cmd = 0;
		} else
			break;

		dbg_run_host_log_dat_low_io[dbg_host_cnt_low_io].time = t;
		dbg_run_host_log_dat_low_io[dbg_host_cnt_low_io].time_diff
			= t - dbg_dma_cmd_log_dat.time;

		tag = (dbg_dma_cmd_log_dat.arg >> 16) & 0x1f;
		dbg_run_host_log_dat_low_io[dbg_host_cnt_low_io].address
			= dbg_task_log_dat[tag].address;
		dbg_run_host_log_dat_low_io[dbg_host_cnt_low_io].size
			= dbg_task_log_dat[tag].size;

		/* if speed < criterion_low_io, record it  */
		if ((dbg_run_host_log_dat_low_io[dbg_host_cnt_low_io].size
		* 1000000000 >> 1) < (criterion_low_io
		* dbg_run_host_log_dat_low_io[dbg_host_cnt_low_io].time_diff)) {
			dbg_run_host_log_dat_low_io[dbg_host_cnt_low_io]
				.continuous_count = ++continuous_count_low_io;
			dbg_host_cnt_low_io++;
			if (dbg_host_cnt_low_io >= dbg_max_cnt_low_io)
				dbg_host_cnt_low_io = 0;
		} else
			continuous_count_low_io = 0;
		break;
#endif
	default:
		break;
	}
}

/* all cases which except softirq of IO */
void dbg_add_host_log(struct mmc_host *mmc, int type,
		int cmd, int arg)
{
	__dbg_add_host_log(mmc, type, cmd, arg, -1, 0);
}


#ifdef SPRD_MMC_LOW_IO_DEBUG
void mmc_low_io_dump(char **buff, unsigned long *size, struct seq_file *m,
	struct mmc_host *mmc)
{
	int i, j;
	unsigned long long t, nanosec_rem, speed;
	char dir;

	if (!mmc || !mmc->card)
		return;

	SPREAD_PRINTF(buff, size, m, "\nLow IO (<%dKB/s):\n",
		criterion_low_io);
	SPREAD_PRINTF(buff, size, m,
		"index time direction address size speed continuous_count\n");

	i = dbg_host_cnt_low_io - 1;
	if (i < 0)
		i = dbg_max_cnt_low_io - 1;

	for (j = 0; j < dbg_max_cnt_low_io; j++) {
		t = dbg_run_host_log_dat_low_io[i].time;
		nanosec_rem = do_div(t, 1000000000)/1000;
		speed = dbg_run_host_log_dat_low_io[i].size * 1000000000;
		if (dbg_run_host_log_dat_low_io[i].time_diff != 0)
			do_div(speed, dbg_run_host_log_dat_low_io[i].time_diff);
		else
			speed = 0;

		if (dbg_run_host_log_dat_low_io[i].cmd == 46)
			dir = 'R';
		else if (dbg_run_host_log_dat_low_io[i].cmd == 47)
			dir = 'W';
		else
			dir = 'N';

		SPREAD_PRINTF(buff, size, m,
			"%05d[%5llu.%06llu]%c,0x%08x,%4lluKB,%6lluKB/s,%d\n",
			j, t, nanosec_rem, dir,
			dbg_run_host_log_dat_low_io[i].address,
			dbg_run_host_log_dat_low_io[i].size >> 1, speed >> 1,
			dbg_run_host_log_dat_low_io[i].continuous_count);
		if (--i < 0)
			i = dbg_max_cnt_low_io - 1;
	}
}
#else
void mmc_low_io_dump(char **buff, unsigned long *size, struct seq_file *m,
	struct mmc_host *mmc)
{
}
#endif

void mmc_cmd_dump(char **buff, unsigned long *size, struct seq_file *m,
	struct mmc_host *mmc, u32 latest_cnt)
{
	int i, j;
	int tag = -1;
	int is_read, is_rel, is_fprg;
	unsigned long long time_sec, time_usec;
	int type, cmd, arg, skip, cnt, cpu;
	unsigned long active_reqs;
	struct sprd_sdhc_host *host;
	u32 dump_cnt;


	if (!mmc || !mmc->card)
		return;
	/* only dump msdc0 */
	host = mmc_priv(mmc);
	if (!host || strcmp(host->device_name, "sdio_emmc") != 0)
		return;

	dump_cnt = min_t(u32, latest_cnt, dbg_max_cnt);

	i = dbg_host_cnt - 1;
	if (i < 0)
		i = dbg_max_cnt - 1;

	for (j = 0; j < dump_cnt; j++) {
		time_sec = dbg_run_host_log_dat[i].time_sec;
		time_usec = dbg_run_host_log_dat[i].time_usec;
		type = dbg_run_host_log_dat[i].type;
		cmd = dbg_run_host_log_dat[i].cmd;
		arg = dbg_run_host_log_dat[i].arg;
		skip = dbg_run_host_log_dat[i].skip;
		if (dbg_run_host_log_dat[i].type == 70) {
			cpu = dbg_run_host_log_dat[i].cpu;
			active_reqs = dbg_run_host_log_dat[i].active_reqs;
		} else {
			cpu = -1;
			active_reqs = 0;
		}
		if (cmd == 44 && !type) {
			cnt = arg & 0xffff;
			tag = (arg >> 16) & 0x1f;
			is_read = (arg >> 30) & 0x1;
			is_rel = (arg >> 31) & 0x1;
			is_fprg = (arg >> 24) & 0x1;
			SPREAD_PRINTF(buff, size, m,
		"%03d [%5llu.%06llu]%2d %3d %08x id=%02d %s cnt=%d %d %d\n",
				j, time_sec, time_usec,
				type, cmd, arg, tag,
				is_read ? "R" : "W",
				cnt, is_rel, is_fprg);
		} else if ((cmd == 46 || cmd == 47) && !type) {
			tag = (arg >> 16) & 0x1f;
			SPREAD_PRINTF(buff, size, m,
				"%03d [%5llu.%06llu]%2d %3d %08x id=%02d\n",
				j, time_sec, time_usec,
				type, cmd, arg, tag);
		} else
			SPREAD_PRINTF(buff, size, m,
			"%03d [%5llu.%06llu]%2d %3d %08x (%d) (0x%08lx) (%d)\n",
				j, time_sec, time_usec,
				type, cmd, arg, skip, active_reqs, cpu);
		i--;
		if (i < 0)
			i = dbg_max_cnt - 1;
	}
#ifdef CONFIG_EMMC_SOFTWARE_CQ_SUPPORT
	SPREAD_PRINTF(buff, size, m,
		"areq_cnt:%d, task_id_index %08lx, cq_wait_rdy:%d, cq_rdy_cnt:%d/n",
		atomic_read(&mmc->areq_cnt),
		mmc->task_id_index,
		atomic_read(&mmc->cq_wait_rdy),
		atomic_read(&mmc->cq_rdy_cnt));
#endif
	SPREAD_PRINTF(buff, size, m,
		"claimed(%d), claim_cnt(%d), claimer pid(%d), comm %s\n",
		mmc->claimed, mmc->claim_cnt,
		mmc->claimer ? mmc->claimer->pid : 0,
		mmc->claimer ? mmc->claimer->comm : "NULL");
}

void sprd_dump_host_state(char **buff, unsigned long *size,
	struct seq_file *m, struct sprd_sdhc_host *host)
{
	/* add log description*/
	SPREAD_PRINTF(buff, size, m,
		"column 1   : log number(Reverse order);\n");
	SPREAD_PRINTF(buff, size, m,
		"column 2   : kernel time\n");
	SPREAD_PRINTF(buff, size, m,
		"column 3   : type(0-cmd, 1-resp;\n");
	SPREAD_PRINTF(buff, size, m,
		"column 4&5 : cmd index&arg(1XX-task XX's task descriptor low 32bit, ");
	SPREAD_PRINTF(buff, size, m,
		"2XX-task XX's task descriptor high 32bit;\n");
	SPREAD_PRINTF(buff, size, m,
		"column 6   : repeat count(The role of problem analysis is low);\n");
	SPREAD_PRINTF(buff, size, m,
		"column 7   : record data_active_reqs;\n");
}


void get_sprd_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	struct sprd_sdhc_host *host = host_emmc;
	unsigned long free_size = SPRD_AEE_BUFFER_SIZE;
	char *buff = sprd_aee_buffer;

	if (host == NULL) {
		pr_info("====== Null host emmc, dump skipped ======\n");
		goto exit;
	}
    //dump_sdio_reg(host);
	pr_info("====== emmc dump ======\n");

	sprd_dump_host_state(&buff, &free_size, NULL, host);
	mmc_cmd_dump(&buff, &free_size, NULL, host->mmc, dbg_max_cnt);
	mmc_low_io_dump(&buff, &free_size, NULL, host->mmc);

exit:
	/* retrun start location */
	*vaddr = (unsigned long)sprd_aee_buffer;
	*size = SPRD_AEE_BUFFER_SIZE - free_size;
}
EXPORT_SYMBOL(get_sprd_aee_buffer);
#else
inline void dbg_add_sd_log(struct mmc_host *mmc, int type, int cmd, int arg)
{
	//pr_info("config MTK_MMC_DEBUG is not set: %s!\n",__func__);
}
inline void dbg_add_host_log(struct mmc_host *mmc, int type, int cmd, int arg)
{
	//pr_info("config MTK_MMC_DEBUG is not set: %s!\n",__func__);
}
inline void dbg_add_sirq_log(struct mmc_host *mmc, int type,
		int cmd, int arg, int cpu, unsigned long active_reqs)
{
	//pr_info("config MTK_MMC_DEBUG is not set: %s!\n",__func__);
}
void mmc_cmd_dump(char **buff, unsigned long *size, struct seq_file *m,
	struct mmc_host *mmc, u32 latest_cnt)
{
	//pr_info("config MTK_MMC_DEBUG is not set: %s!\n",__func__);
}
void sprd_dump_host_state(char **buff, unsigned long *size,
		struct seq_file *m, struct sprd_sdhc_host *host)
{
	//pr_info("config MTK_MMC_DEBUG is not set: %s!\n",__func__);
}
static void msdc_proc_dump(struct seq_file *m, u32 id)
{
	//pr_info("config MTK_MMC_DEBUG is not set : %s!\n",__func__);
}
void get_sprd_aee_buffer(unsigned long *vaddr, unsigned long *size)
{
	//pr_info("config MTK_MMC_DEBUG is not set : %s!\n",__func__);
}
#endif
