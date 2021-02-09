// SPDX-License-Identifier: GPL-2.0
/*
 * AP/CH time synchronization for Spreadtrum SoCs
 *
 * Copyright (C) 2021 Spreadtrum corporation. http://www.unisoc.com
 *
 * Author: Ruifeng Zhang <Ruifeng.Zhang1@unisoc.com>
 */

#include <clocksource/arm_arch_timer.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/sipc.h>
#include <linux/soc/sprd/sprd_systimer.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>

#define SENSOR_TS_CTRL_NUM	4
#define SENSOR_TS_CTRL_SIZE	4
#define SENSOR_TS_CTRL_SEL	(1 << 8)

#undef pr_fmt
#define pr_fmt(fmt) "sprd_time_sync_ch: " fmt

#define DEFAULT_TIMEVALE_MS (1000 * 60 * 10) //10min

static void __iomem *sprd_time_sync_ch_addr_base;

static struct hrtimer sprd_time_sync_ch_timer;

static struct sprd_cnt_to_boot{
	u64 ts_cnt;
	u64 sysfrt_cnt;
	u64 boottime;
} sprd_ctb;

static DEFINE_SPINLOCK(sprd_time_sync_ch_lock);

#if IS_ENABLED(CONFIG_ARM)
static inline u64 sprd_arch_counter_get_cntpct(void)
{
	u64 cval;

	isb();
	asm volatile("mrrc p15, 0, %Q0, %R0, c14" : "=r" (cval));
	return cval;
}
#else
static inline u64 sprd_arch_counter_get_cntpct(void)
{
	u64 cval;

	isb();
	asm volatile("mrs %0, cntpct_el0" : "=r" (cval));
	return cval;
}
#endif

static inline void update_cnt_to_boot(void)
{
	sprd_ctb.ts_cnt = sprd_arch_counter_get_cntpct();
	sprd_ctb.sysfrt_cnt = sprd_sysfrt_read();
	sprd_ctb.boottime = ktime_get_boot_fast_ns();
}

/* send only once to ensure resume time */
static void sprd_time_sync_ch_send(void)
{

#if IS_ENABLED(CONFIG_SPRD_DEBUG)
	pr_info("send time to ch. ts_cnt = %llu sysfrt_cnt = %lu boottime = %llu\n",
		 sprd_ctb.ts_cnt,
		 sprd_ctb.sysfrt_cnt,
		 sprd_ctb.boottime);
#endif
}

void sprd_time_sync_ch_resume(void)
{
	u32 time_sync_ch_ctrl;
	unsigned long flags;
	int i;

	if (!sprd_time_sync_ch_addr_base)
		return;

	spin_lock_irqsave(&sprd_time_sync_ch_lock, flags);
	/* set 0 since ap active */
	for (i = 0; i < SENSOR_TS_CTRL_NUM; i++) {
		time_sync_ch_ctrl = readl_relaxed(sprd_time_sync_ch_addr_base + i * SENSOR_TS_CTRL_SIZE);
		time_sync_ch_ctrl &= ~SENSOR_TS_CTRL_SEL;
		writel_relaxed(time_sync_ch_ctrl, sprd_time_sync_ch_addr_base + i * SENSOR_TS_CTRL_SIZE);
	}

	update_cnt_to_boot();
	spin_unlock_irqrestore(&sprd_time_sync_ch_lock, flags);

	sprd_time_sync_ch_send();
}

int sprd_time_sync_ch_suspend(void)
{
	u32 time_sync_ch_ctrl;
	unsigned long flags;
	int i;

	if (!sprd_time_sync_ch_addr_base)
		return 0;

	spin_lock_irqsave(&sprd_time_sync_ch_lock, flags);
	/* set 1 since AP suspend */
	for (i = 0; i < SENSOR_TS_CTRL_NUM; i++) {
		time_sync_ch_ctrl = readl_relaxed(sprd_time_sync_ch_addr_base + i * SENSOR_TS_CTRL_SIZE);
		time_sync_ch_ctrl |= SENSOR_TS_CTRL_SEL;
		writel_relaxed(time_sync_ch_ctrl, sprd_time_sync_ch_addr_base + i * SENSOR_TS_CTRL_SIZE);
	}
	spin_unlock_irqrestore(&sprd_time_sync_ch_lock, flags);

	return 0;
}

/* sysfs resume/suspend bits for time_sync_ch */
static struct syscore_ops sprd_time_sync_ch_syscore_ops = {
	.resume		= sprd_time_sync_ch_resume,
	.suspend	= sprd_time_sync_ch_suspend,
};

static enum hrtimer_restart sprd_sync_timer_ch(struct hrtimer *hr)
{
	unsigned long flags;

	spin_lock_irqsave(&sprd_time_sync_ch_lock, flags);
	update_cnt_to_boot();
	spin_unlock_irqrestore(&sprd_time_sync_ch_lock, flags);

	sprd_time_sync_ch_send();

	hrtimer_forward_now(&sprd_time_sync_ch_timer, ms_to_ktime(DEFAULT_TIMEVALE_MS));

	return HRTIMER_RESTART;
}

static int sprd_time_sync_ch_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	unsigned long flags;

	sprd_time_sync_ch_addr_base = of_iomap(np, 0);
	if (!sprd_time_sync_ch_addr_base) {
		pr_err("Can't map sprd time sync ch addr.\n");
		return -EFAULT;
	}

	register_syscore_ops(&sprd_time_sync_ch_syscore_ops);

	/* update/send sprd_ctb value first */
	spin_lock_irqsave(&sprd_time_sync_ch_lock, flags);
	update_cnt_to_boot();
	spin_unlock_irqrestore(&sprd_time_sync_ch_lock, flags);

	sprd_time_sync_ch_send();

	hrtimer_init(&sprd_time_sync_ch_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sprd_time_sync_ch_timer.function = sprd_sync_timer_ch;
	hrtimer_start(&sprd_time_sync_ch_timer, ms_to_ktime(DEFAULT_TIMEVALE_MS), HRTIMER_MODE_REL);

	pr_info("probe done.\n");

	return 0;
}

static int sprd_time_sync_ch_remove(struct platform_device *pdev)
{
	sprd_time_sync_ch_addr_base = NULL;

	return 0;
}

static const struct of_device_id sprd_time_sync_ch_ids[] = {
	{ .compatible = "sprd,time-sync-ch", },
	{},
};

static struct platform_driver sprd_time_sync_ch_driver = {
	.probe = sprd_time_sync_ch_probe,
	.remove = sprd_time_sync_ch_remove,
	.driver = {
		.name = "sprd_time_sync_ch",
		.of_match_table = sprd_time_sync_ch_ids,
	},
};

module_platform_driver(sprd_time_sync_ch_driver);

MODULE_AUTHOR("Ruifeng Zhang");
MODULE_DESCRIPTION("sprd time sync ch driver");
MODULE_LICENSE("GPL v2");
