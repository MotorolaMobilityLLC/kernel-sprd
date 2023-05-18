// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Unisoc Communications Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm/emem.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/profile.h>
#include <linux/sched/signal.h>
#include <linux/swap.h>
#include <linux/workqueue.h>

/* killed process oom score adj threshold */
#define DEFAULT_PROC_ADJ		900
#define EMEM_SHOW_INTERVAL		5
#define EMEM_SHOW_KILL_ADJ900_INTERVAL  600

/*
 * The written value is the killed process adj, then trigger to show enhance
 * memory information. it's written to /proc/emem_trigger
 */
int sysctl_emem_trigger;

static struct work_struct unisoc_emem_work;
static DEFINE_SPINLOCK(unisoc_emem_lock);
/* User knob to enable/disable enhance meminfo feature */
static int enable_unisoc_meminfo;
static int high_freq_print_threshold = 200;

module_param_named(enable, enable_unisoc_meminfo, int, 0644);
module_param_named(high_freq_print_threshold, high_freq_print_threshold,
		int, 0644);

static void unisoc_enhance_meminfo(unsigned long interval)
{
	static unsigned long last_jiffies;

	if (time_after(jiffies, last_jiffies + interval * HZ)) {
		pr_info("++++++++++++++++++++++UNISOC_SHOW_MEM_BEGIN++++++++++++++++++++\n");
		pr_info("The killed process adj = %d\n", sysctl_emem_trigger);
		unisoc_enhanced_show_mem();
		last_jiffies = jiffies;
		pr_info("+++++++++++++++++++++++UNISOC_SHOW_MEM_END+++++++++++++++++++++\n");
	}
}

static void unisoc_emem_workfn(struct work_struct *work)
{
	if (enable_unisoc_meminfo) {
		if (sysctl_emem_trigger <= high_freq_print_threshold)
			unisoc_enhance_meminfo(EMEM_SHOW_INTERVAL);
		else
			unisoc_enhance_meminfo(EMEM_SHOW_KILL_ADJ900_INTERVAL);
	}
}

static struct task_struct *check_lock_task_mm(struct task_struct *p)
{
	struct task_struct *t;

	rcu_read_lock();
	for_each_thread(p, t) {
		task_lock(t);
		if (likely(t->mm))
			goto found;
		task_unlock(t);
	}
	t = NULL;
found:
	rcu_read_unlock();

	return t;
}

static void dump_tasks_info(void)
{
	struct task_struct *p;
	struct task_struct *task;
	struct sysinfo si;

	si_swapinfo(&si);
	pr_info("Enhanced Mem-info :TASK\n");
	pr_info("Detail:\n");
	pr_info("[ pid ]   uid  tgid total_vm      rss   swap cpu oom_score_adj name\n");

	rcu_read_lock();
	for_each_process(p) {
		/* check unkillable tasks */
		if (is_global_init(p))
			continue;
		if (p->flags & PF_KTHREAD)
			continue;
		task = check_lock_task_mm(p);
		if (!task) {
			/*
			 * This is a kthread or all of p's threads have already
			 * detached their mm's.  There's no need to report
			 * them; they can't be oom killed anyway.
			 */
			continue;
		}
		pr_info("[%5d] %5d %5d %8lu %8lu %6lu %3u         %5d %s\n",
			task->pid, from_kuid(&init_user_ns, task_uid(task)),
			task->tgid, task->mm->total_vm, get_mm_rss(task->mm),
			get_mm_counter(task->mm, MM_SWAPENTS),
			task_cpu(task),
			task->signal->oom_score_adj, task->comm);
		task_unlock(task);
	}
	rcu_read_unlock();

	pr_info("Total used:\n");
	pr_info("     anon: %lu kB\n", ((global_node_page_state(NR_ACTIVE_ANON)
		     + global_node_page_state(NR_INACTIVE_ANON)) << PAGE_SHIFT)
			/ 1024);
	pr_info("   swaped: %lu kB\n", ((si.totalswap - si.freeswap)
		<< PAGE_SHIFT) / 1024);
}

static int e_show_mem_handler(struct notifier_block *nb,
			unsigned long val, void *data)
{
	dump_tasks_info();
	return 0;
}

static struct notifier_block e_show_mem_notifier = {
	.notifier_call = e_show_mem_handler,
};

static ssize_t emem_trigger_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	char buffer[12];
	int trigger_adj;
	int ret;

	memset(buffer, 0, sizeof(buffer));
	if (count > sizeof(buffer) - 1)
		count = sizeof(buffer) - 1;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	ret = kstrtoint(buffer, 0, &trigger_adj);
	if (ret)
		return ret;

	sysctl_emem_trigger = trigger_adj;
	if (sysctl_emem_trigger <= DEFAULT_PROC_ADJ) {
		spin_lock(&unisoc_emem_lock);
		queue_work(system_power_efficient_wq, &unisoc_emem_work);
		spin_unlock(&unisoc_emem_lock);
	}
	return count;
}

const struct proc_ops proc_emem_trigger_operations = {
	.proc_write		= emem_trigger_write,
};

static int emem_init(void)
{
	INIT_WORK(&unisoc_emem_work, unisoc_emem_workfn);
	register_unisoc_show_mem_notifier(&e_show_mem_notifier);
	proc_create("emem_trigger", 0200, NULL, &proc_emem_trigger_operations);
	return 0;
}

static void emem_exit(void)
{
	unregister_unisoc_show_mem_notifier(&e_show_mem_notifier);
	remove_proc_entry("emem_trigger", NULL);
}

subsys_initcall(emem_init);
module_exit(emem_exit);
MODULE_IMPORT_NS(MINIDUMP);
MODULE_LICENSE("GPL v2");
