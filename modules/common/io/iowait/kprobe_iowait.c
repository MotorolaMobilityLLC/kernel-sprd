// SPDX-License-Identifier: GPL-2.0-only
/*
 * kprobe_iowait.c
 *
 * Print task iowait time and panic when stall in balance_dirty_pages().
 * Note:
 * 1. Need modify code when ANDROID_VENDOR_DATA_ARRAY changed in task_struct.
 * 2. Can't rmmod this module because use restricted vendor hook;
 *
 * Copyright (C) 2022-2023 UNISOC, Inc.
 *             https://www.unisoc.com/
 *
 * Author: Hongyu.Jin@unisoc.com
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <trace/hooks/sched.h>
#include <linux/stacktrace.h>

// iowait stacktrace depth
#define NR_STACK_FRAME    3

static unsigned long iowait_threshold = 500;          // ms
module_param(iowait_threshold, ulong, 0664);
MODULE_PARM_DESC(iowait_threshold, "iowait print threshold in milliseconds");

static unsigned long balance_panic_threshold = 60;   // second
module_param(balance_panic_threshold, ulong, 0664);
MODULE_PARM_DESC(balance_panic_threshold, "balance_dirty_pages panic threshold in seconds");

static int balance_dirty_entry_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	if (current == NULL) return -1;

	current->android_vendor_data1[62] = ktime_get_boot_fast_ns();
	return 0;
}
NOKPROBE_SYMBOL(balance_dirty_entry_handler);

static int balance_dirty_ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	if (current == NULL) return 0;

	current->android_vendor_data1[62] = 0;
	return 0;
}
NOKPROBE_SYMBOL(balance_dirty_ret_handler);

static struct kretprobe balance_dirty_kretprobe = {
	.kp.symbol_name = "balance_dirty_pages",
	.handler		= balance_dirty_ret_handler,
	.entry_handler  = balance_dirty_entry_handler,
	.data_size		= 0,
	/* Probe up to instances concurrently. */
	.maxactive		= 100,
};

module_param_named(balance_maxactive, balance_dirty_kretprobe.maxactive, int, 0664);
MODULE_PARM_DESC(balance_maxactive, "balance kretprobe maxactive");
module_param_named(balance_nmissed, balance_dirty_kretprobe.nmissed, int, 0444);
MODULE_PARM_DESC(balance_nmissed, "blance kretprobe nmissed");

void dequeue_task_vh(void *data,struct rq *rq, struct task_struct *tsk, int flags)
{
	if (tsk == NULL) return;

	if (tsk->in_iowait)
		tsk->android_vendor_data1[63] = ktime_get_boot_fast_ns();

	return;
}

// Use andriod_rvh_queue/dequeu_task vendor hook.
void enqueue_task_vh(void *data, struct rq *rq, struct task_struct *tsk, int flags)
{
	u64 delta;
	u64 now = ktime_get_boot_fast_ns();

	if (tsk == NULL) return;

	if (tsk->in_iowait) {
		if (tsk->android_vendor_data1[63] && now > tsk->android_vendor_data1[63]) {
			delta = now - tsk->android_vendor_data1[63];
			if (delta > iowait_threshold * NSEC_PER_MSEC) {
				int i;
				unsigned int nr_entries;
				unsigned long stack_entries[NR_STACK_FRAME];

				pr_info("iowait: %5lldms [%-16s][%5ld] %-16s",
					ktime_to_ms(delta), tsk->comm, tsk->pid,
					tsk->group_leader ? tsk->group_leader->comm : "");

				nr_entries = stack_trace_save_tsk(tsk, stack_entries, NR_STACK_FRAME, 0);
				for (i = 0; i < nr_entries; i++) {
					pr_cont("%s%ps", i? "<": " ", (void *)stack_entries[i]);
				}
			}
		}

		if (tsk->android_vendor_data1[62] &&
			((now - tsk->android_vendor_data1[62]) > balance_panic_threshold * NSEC_PER_SEC)) {
			sched_show_task(tsk);
			panic("task %px stall in balance_dirty_pages\n", tsk);
		}
	}
}

static int __init kprobe_init(void)
{
	int ret;

	ret = register_kretprobe(&balance_dirty_kretprobe);
	if (ret < 0) {
		pr_err("register_kretprobe failed, returned %d\n", ret);
		return ret;
	}
	pr_info("Planted return probe at %s: %p\n",
			balance_dirty_kretprobe.kp.symbol_name, balance_dirty_kretprobe.kp.addr);

	register_trace_android_rvh_dequeue_task(dequeue_task_vh, NULL);
	register_trace_android_rvh_enqueue_task(enqueue_task_vh, NULL);
	return 0;
}

static void __exit kprobe_exit(void)
{
	unregister_kretprobe(&balance_dirty_kretprobe);
	pr_info("kretprobe at %p unregistered\n", balance_dirty_kretprobe.kp.addr);

	/* nmissed > 0 suggests that maxactive was set too low. */
	pr_info("Missed probing %d instances of %s\n",
		balance_dirty_kretprobe.nmissed, balance_dirty_kretprobe.kp.symbol_name);
}

module_init(kprobe_init)
module_exit(kprobe_exit)
MODULE_LICENSE("GPL");
