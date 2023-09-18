// SPDX-License-Identifier: GPL-2.0
#define pr_fmt(fmt)  "sprd_modules_notify: " fmt

#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/radix-tree.h>
#include <linux/seq_file.h>
#include <linux/sched/clock.h>

#include "native_hang_monitor.h"

#if IS_ENABLED(CONFIG_SPRD_MODULES_NOTIFY)

/*unit: ms*/
#ifdef CONFIG_SPRD_DEBUG
#define MODULES_TIME_OUT (1600)
#else
#define MODULES_TIME_OUT (2000)
#endif

static RADIX_TREE(modules_tree, GFP_KERNEL|GFP_ATOMIC);
static char *module_name;
static struct timer_list module_timer;

/* Handle one module, either coming or alive. */
static int sprd_module_notify(struct notifier_block *self,
			unsigned long val, void *data)
{
	struct module *mod = data;
	int ret = 0;

	switch (val) {
	case MODULE_STATE_COMING:
		pr_debug("coming, name %s\n", mod->name);
		module_name = mod->name;
#ifdef CONFIG_ANDROID_KABI_RESERVE
		mod->android_kabi_reserved1 = ktime_get_boot_fast_ns();
		radix_tree_insert(&modules_tree, mod->android_kabi_reserved1, mod);
		module_timer.android_kabi_reserved1 = 1;
#endif
		module_timer.expires = jiffies + msecs_to_jiffies(MODULES_TIME_OUT);
		add_timer(&module_timer);
		break;

	case MODULE_STATE_LIVE:
		del_timer(&module_timer);

#ifdef CONFIG_ANDROID_KABI_RESERVE
		module_timer.android_kabi_reserved1 = 0;
		pr_debug("live, name %s ,diff = %llu ns\n", mod->name,
				ktime_get_boot_fast_ns() - mod->android_kabi_reserved1);

		mod->android_kabi_reserved2 = ktime_get_boot_fast_ns()
							- mod->android_kabi_reserved1;
#endif
		break;

	case  MODULE_STATE_GOING:
#ifdef CONFIG_ANDROID_KABI_RESERVE
		if (module_timer.android_kabi_reserved1)
			module_timer.android_kabi_reserved1 = 0;
#endif
		del_timer(&module_timer);
		break;
	default:
		break;
	}
	return ret;
}

static void sprd_module_time_out(struct timer_list *t)
{
#ifdef CONFIG_SPRD_SKYNET
	pr_debug("%s.ko loads too long time, panic timeout = %d ms\n",
			module_name, MODULES_TIME_OUT);
#else
	panic("%s.ko loads too long time, panic timeout = %d ms, loglevel = %d\n",
			module_name, MODULES_TIME_OUT, console_printk[0]);
#endif
}

static struct notifier_block sprd_module_nb = {
	.notifier_call = sprd_module_notify,
	.priority = 99,
};

static int sprd_modules_show(struct seq_file *m, void *v)
{
	void __rcu **slot;
	struct radix_tree_iter iter;
	struct module *modules;

	seq_puts(m, "module name, loading time(ns), kernel boottime(ns)\n");
	radix_tree_for_each_slot(slot, &modules_tree, &iter, 0) {
		if (slot != NULL)
			modules = *slot;

#ifdef CONFIG_ANDROID_KABI_RESERVE
		if (modules) {
			seq_printf(m, "%-50s  %-20lld  %-20lld\n",
					modules->name, modules->android_kabi_reserved2,
					modules->android_kabi_reserved1);
			modules = NULL;
		}
	}
#endif
	return 0;
}

int sprd_modules_init(void)
{
	int ret;
	static struct proc_dir_entry *modules_entry;

	ret = register_module_notifier(&sprd_module_nb);
	if (ret)
		pr_warn("Failed to register sprd module notifier\n");

	timer_setup(&module_timer, sprd_module_time_out, TIMER_DEFERRABLE);

	modules_entry = proc_create_single("modules_list", 0444, NULL, sprd_modules_show);
	if (!modules_entry) {
		pr_err("Failed to create /proc/modules_list\n");
		return -1;
	}

	pr_info("Initialized\n");
	return ret;
}

void  sprd_modules_exit(void)
{
	unregister_module_notifier(&sprd_module_nb);
	pr_warn("Exited\n");
}
#endif
