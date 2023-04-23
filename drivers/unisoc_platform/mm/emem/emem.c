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
#include <linux/proc_fs.h>
#include <linux/profile.h>
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
	proc_create("emem_trigger", 0200, NULL, &proc_emem_trigger_operations);
	return 0;
}

static void emem_exit(void)
{
	remove_proc_entry("emem_trigger", NULL);
}

subsys_initcall(emem_init);
module_exit(emem_exit);
MODULE_IMPORT_NS(MINIDUMP);
MODULE_LICENSE("GPL v2");
