// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * it's hardware watchdog feeder code for Phoenix II
 */

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/percpu.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/smp.h>
#include <linux/delay.h>
#include <linux/smpboot.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <uapi/linux/sched/types.h>
#include <linux/watchdog.h>
#include <linux/seq_file.h>
#include <linux/export.h>

#if IS_ENABLED(CONFIG_SPRD_WATCHDOG_FIQ)
#include <linux/sprd_wdt_fiq.h>
#endif

#undef pr_fmt
#define pr_fmt(fmt) "sprd_wdf: " fmt

static DEFINE_PER_CPU(struct task_struct *, hang_debug_task_store);
unsigned int cpu_feed_mask;
unsigned int cpu_feed_bitmap;
EXPORT_SYMBOL(cpu_feed_mask);
EXPORT_SYMBOL(cpu_feed_bitmap);
static DEFINE_MUTEX(wdf_mutex);
/**
 * choose hrtimer here due to hrtimer was handled in hard interrupt context
 * while timer was handled in soft interrupt context
 */
static DEFINE_PER_CPU(struct hrtimer, sprd_wdt_hrtimer);
/* 1: which cpu need to feed, 0: cpu doesn't need to feed */
static DEFINE_PER_CPU(int, g_enable);
struct watchdog_device *wdd;
/* set default watchdog time */
static u64 g_interval = 8;
static u64 g_timeout = 40;
static u64 g_pretimeout = 20;
static int wdt_disable;

static enum hrtimer_restart sprd_wdt_timer_func(struct hrtimer *hrtimer)
{
	/**
	 * hrtimer_cancel will be called in disable wdt context, however,
	 * check wdt_disable here to bail out early. mutex shouldn't be
	 * added here to avoid dead-lock due to mutex would have been held
	 * before hrtimer_cancel.
	 */
	if (wdt_disable) {
		pr_debug("hrtimer func: wdt_disable\n");
		return HRTIMER_NORESTART;
	}

	__this_cpu_write(g_enable, 1);
	wake_up_process(__this_cpu_read(hang_debug_task_store));
	hrtimer_forward_now(hrtimer, ms_to_ktime(g_interval * MSEC_PER_SEC));
	return HRTIMER_RESTART;
}

static void sprd_wdf_hrtimer_enable(unsigned int cpu)
{
	struct hrtimer *hrtimer = this_cpu_ptr(&sprd_wdt_hrtimer);

	hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer->function = sprd_wdt_timer_func;
	hrtimer_start(hrtimer, ms_to_ktime(g_interval * MSEC_PER_SEC),
		      HRTIMER_MODE_REL_PINNED);
}

static void hang_debug_create(unsigned int cpu)
{
	struct task_struct *hang_debug_t;
	struct sched_param param = {.sched_priority = (MAX_RT_PRIO - 1)};

	hang_debug_t = per_cpu(hang_debug_task_store, cpu);
	sched_setscheduler(hang_debug_t, SCHED_FIFO, &param);

	per_cpu(g_enable, cpu) = 0;
}

static int hang_debug_should_run(unsigned int cpu)
{
	return per_cpu(g_enable, cpu);
}

static void hang_debug_park(unsigned int cpu)
{
	struct hrtimer *hrtimer = this_cpu_ptr(&sprd_wdt_hrtimer);

	mutex_lock(&wdf_mutex);
	cpu_feed_mask &= (~(1U << cpu));
	cpu_feed_bitmap = 0;
	pr_debug("offline cpu = %u\n", cpu);

	if (wdt_disable)
		goto out;
#if IS_ENABLED(CONFIG_SPRD_WATCHDOG_FIQ)
	if (wdd->ops->start)
		wdd->ops->start(wdd);
#endif
	hrtimer_cancel(hrtimer);
out:
	mutex_unlock(&wdf_mutex);
}

static void hang_debug_unpark(unsigned int cpu)
{

	mutex_lock(&wdf_mutex);
	cpu_feed_mask |= (1U << cpu);
	cpu_feed_bitmap = 0;
	pr_debug("online cpu = %u\n", cpu);

	if (wdt_disable)
		goto out;
#if IS_ENABLED(CONFIG_SPRD_WATCHDOG_FIQ)
	if (wdd->ops->start)
		wdd->ops->start(wdd);
#endif
	hrtimer_start(this_cpu_ptr(&sprd_wdt_hrtimer),
			ms_to_ktime(g_interval * MSEC_PER_SEC),
			HRTIMER_MODE_REL_PINNED);
out:
	mutex_unlock(&wdf_mutex);
}

static void hang_debug_task(unsigned int cpu)
{

	mutex_lock(&wdf_mutex);
	if (wdt_disable)
		goto out;

	cpu_feed_bitmap |= (1U << cpu);
	if (cpu_feed_mask == cpu_feed_bitmap) {
		pr_debug("feed wdt cpu_feed_bitmap = 0x%08x\n", cpu_feed_bitmap);
		cpu_feed_bitmap = 0;
#if IS_ENABLED(CONFIG_SPRD_WATCHDOG_FIQ)
		if (wdd->ops->start)
			wdd->ops->start(wdd);
#endif
	} else
		pr_debug("cpu_feed_bitmap = 0x%08x\n", cpu_feed_bitmap);

	__this_cpu_write(g_enable, 0);
out:
	mutex_unlock(&wdf_mutex);
}

static struct smp_hotplug_thread hang_debug_threads = {
	.store			= &hang_debug_task_store,
	.thread_should_run	= hang_debug_should_run,
	.create			= hang_debug_create,
	.thread_fn		= hang_debug_task,
	.thread_comm	= "hang_debug/%u",
	.setup			= sprd_wdf_hrtimer_enable,
	.park			= hang_debug_park,
	.unpark			= hang_debug_unpark,
};

static void smp_hrtimer_start(void *data)
{
	int cpu = smp_processor_id();

	pr_debug("hrtimer restart on cpu%d\n", cpu);
	hrtimer_start(this_cpu_ptr(&sprd_wdt_hrtimer),
			ms_to_ktime(g_interval * MSEC_PER_SEC),
			HRTIMER_MODE_REL_PINNED);
}

static int hang_debug_proc_read(struct seq_file *s, void *v)
{
	int cpu;

	seq_printf(s, "WDF: interval=%llu pretimeout=%llu timeout=%llu bitmap=0x%08x/0x%08x\n",
		       g_interval, g_pretimeout, g_timeout,
		       cpu_feed_bitmap, cpu_feed_mask);

	for_each_online_cpu(cpu) {
		seq_printf(s, "[cpu%d] g_enable = %d\n", cpu, per_cpu(g_enable, cpu));
	}

	return 0;
}

static int hang_debug_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, hang_debug_proc_read, NULL);
}

static ssize_t hang_debug_proc_write(struct file *file, const char *buf,
	size_t count, loff_t *data)
{
	u64 timeout;
	u64 pretimeout;
	int err, cpu;

	err = kstrtoull_from_user(buf, count, 0, &timeout);
	if (err)
		return -EINVAL;

	if (!timeout || timeout < 5) {
		pr_err("Input timeout must bigger than 5.\n");
		return -EINVAL;
	}

	if (g_timeout != timeout) {
		mutex_lock(&wdf_mutex);
		cpu_feed_bitmap = 0;

		/* cancel timer on each cpu */
		for_each_online_cpu(cpu)
			hrtimer_cancel(per_cpu_ptr(&sprd_wdt_hrtimer, cpu));

		g_timeout = timeout;

		pretimeout = timeout;
		pretimeout = div_u64(pretimeout, 2);

		g_interval = timeout;
		g_interval = div_u64(g_interval, 5);

		pr_notice("timeout = %llu interval = %llu\n", g_timeout, g_interval);

#if IS_ENABLED(CONFIG_SPRD_WATCHDOG_FIQ)
		/* set new timeout and touch watchdog */
		if (wdd->ops->set_timeout)
			wdd->ops->set_timeout(wdd, (u32)g_timeout);
		if (wdd->ops->set_pretimeout)
			wdd->ops->set_pretimeout(wdd, pretimeout);
		if (wdd->ops->start)
			wdd->ops->start(wdd);
#endif
		/* restart timer on each cpu */
		for_each_online_cpu(cpu)
			smp_call_function_single(cpu, smp_hrtimer_start,
					NULL, true);
		mutex_unlock(&wdf_mutex);
	}

	return count;
}

static const struct file_operations hang_debug_proc_fops = {
	.owner = THIS_MODULE,
	.open = hang_debug_proc_open,
	.read = seq_read,
	.write = hang_debug_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int wdt_disable_proc_read(struct seq_file *s, void *v)
{
	seq_printf(s, "%d\n", wdt_disable);
	return 0;
}

static int wdt_disable_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, wdt_disable_proc_read, NULL);
}

void sprd_wdf_wdt_disable(int disable)
{
	int cpu;

	mutex_lock(&wdf_mutex);
	if (disable && !wdt_disable) {
		wdt_disable = 1;
		cpu_feed_bitmap = 0;
		for_each_online_cpu(cpu) {
			/* cancel timer on each cpu */
			hrtimer_cancel(per_cpu_ptr(&sprd_wdt_hrtimer, cpu));
			per_cpu(g_enable, cpu) = 0;
		}
#if IS_ENABLED(CONFIG_SPRD_WATCHDOG_FIQ)
		if (wdd->ops->stop)
			wdd->ops->stop(wdd);
#endif
	} else if (!disable && wdt_disable) {
		wdt_disable = 0;
#if IS_ENABLED(CONFIG_SPRD_WATCHDOG_FIQ)
		if (wdd->ops->set_timeout)
			wdd->ops->set_timeout(wdd, (u32)g_timeout);
		if (wdd->ops->set_pretimeout)
			wdd->ops->set_pretimeout(wdd, (u32)g_pretimeout);
		if (wdd->ops->start)
			wdd->ops->start(wdd);
#endif
		/* restart timer on each cpu */
		for_each_online_cpu(cpu)
			smp_call_function_single(cpu, smp_hrtimer_start,
					NULL, true);
	}
	mutex_unlock(&wdf_mutex);
}

EXPORT_SYMBOL(sprd_wdf_wdt_disable);

static ssize_t wdt_disable_proc_write(struct file *file, const char *buf,
	size_t count, loff_t *data)
{
	unsigned long long disable;
	int err;

	err = kstrtoull_from_user(buf, count, 0, &disable);
	if (err)
		return -EINVAL;

	pr_notice("watchdog disable = %d\n", (int)disable);
	sprd_wdf_wdt_disable((int)disable);

	return count;
}

static const struct file_operations wdt_disable_proc_fops = {
	.owner = THIS_MODULE,
	.open = wdt_disable_proc_open,
	.read = seq_read,
	.write = wdt_disable_proc_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int wdf_panic_event(struct notifier_block *self,
					unsigned long val, void *reason)
{
	sprd_wdf_wdt_disable(1);
	return NOTIFY_DONE;
}

static struct notifier_block wdf_panic_event_nb = {
	.notifier_call	= wdf_panic_event,
	.priority	= INT_MIN,
};

static int __init sprd_hang_debug_init(void)
{
	int cpu;
	struct proc_dir_entry *de = NULL;
	struct proc_dir_entry *df = NULL;

#if IS_ENABLED(CONFIG_SPRD_WATCHDOG_FIQ)
	int ret = 0;

	ret = sprd_wdt_fiq_get_dev(&wdd);
	if (ret) {
		pr_err("get public api error in sprd_wdt %d\n", ret);
		return ret;
	}

	if (wdd->ops->set_timeout)
		wdd->ops->set_timeout(wdd, (u32)g_timeout);

	if (wdd->ops->set_pretimeout)
		wdd->ops->set_pretimeout(wdd, (u32)g_pretimeout);

	if (wdd->ops->start)
		wdd->ops->start(wdd);

	pr_notice("%s\n", __func__);
#else
	pr_notice("No config sprd_wdt device\n");
#endif

	cpu_feed_mask = 0;
	cpu_feed_bitmap = 0;
	for_each_online_cpu(cpu) {
		cpu_feed_mask |= (1 << cpu);
	}

	BUG_ON(smpboot_register_percpu_thread(&hang_debug_threads));

	de = proc_mkdir("sprd_hang_debug", NULL);
	if (de) {
		df = proc_create("wdf_timeout", 0660, de, &hang_debug_proc_fops);
		if (!df)
			pr_err("create /proc/sprd_hang_debug/wdf_timeout failed\n");
		df = proc_create("wdt_disable", 0660, de, &wdt_disable_proc_fops);
		if (!df)
			pr_err("create /proc/sprd_hang_debug/wdt_disable failed\n");
	} else {
		pr_err("create /proc/sprd_hang_debug/ failed\n");
	}

	atomic_notifier_chain_register(&panic_notifier_list,
					&wdf_panic_event_nb);

	return 0;
}

late_initcall(sprd_hang_debug_init);

MODULE_DESCRIPTION("sprd hang debug wdf driver");
MODULE_LICENSE("GPL v2");
