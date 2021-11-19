// SPDX-License-Identifier: GPL-2.0
/*
 *  syscore_s2idle.c - Execution of system core operations.
 */

#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/syscore_s2idle_ops.h>
#include <trace/events/power.h>
#include <linux/wakeup_reason.h>

static LIST_HEAD(syscore_s2idle_ops_list);
static DEFINE_MUTEX(syscore_s2idle_ops_lock);

/**
 * register_syscore_s2idle_ops - Register a set of system core operations.
 * @ops: System core operations to register.
 */
void register_syscore_s2idle_ops(struct syscore_s2idle_ops *ops)
{
	mutex_lock(&syscore_s2idle_ops_lock);
	list_add_tail(&ops->node, &syscore_s2idle_ops_list);
	mutex_unlock(&syscore_s2idle_ops_lock);
}
EXPORT_SYMBOL_GPL(register_syscore_s2idle_ops);

/**
 * unregister_syscore_s2idle_ops - Unregister a set of system core operations.
 * @ops: System core operations to unregister.
 */
void unregister_syscore_s2idle_ops(struct syscore_s2idle_ops *ops)
{
	mutex_lock(&syscore_s2idle_ops_lock);
	list_del(&ops->node);
	mutex_unlock(&syscore_s2idle_ops_lock);
}
EXPORT_SYMBOL_GPL(unregister_syscore_s2idle_ops);

#ifdef CONFIG_PM_SLEEP
/**
 * syscore_s2idle_suspend - Execute all the registered system core suspend callbacks.
 *
 * This function is executed with one CPU on-line and disabled interrupts.
 */
int syscore_s2idle_suspend(void)
{
	struct syscore_s2idle_ops *ops;
	int ret = 0;

	trace_suspend_resume(TPS("syscore_s2idle_suspend"), 0, true);
	pr_debug("Checking wakeup interrupts\n");

	/* Return error code if there are any wakeup interrupts pending. */
	if (pm_wakeup_pending())
		return -EBUSY;

	WARN_ONCE(!irqs_disabled(),
		"Interrupts enabled before system core suspend.\n");

	list_for_each_entry_reverse(ops, &syscore_s2idle_ops_list, node)
		if (ops->suspend) {
			if (initcall_debug)
				pr_info("PM: Calling %pS\n", ops->suspend);
			ret = ops->suspend();
			if (ret)
				goto err_out;
			WARN_ONCE(!irqs_disabled(),
				"Interrupts enabled after %pS\n", ops->suspend);
		}

	trace_suspend_resume(TPS("syscore_s2idle_suspend"), 0, false);
	return 0;

 err_out:
	log_suspend_abort_reason("System core suspend callback %pS failed",
		ops->suspend);
	pr_err("PM: System core suspend callback %pS failed.\n", ops->suspend);

	list_for_each_entry_continue(ops, &syscore_s2idle_ops_list, node)
		if (ops->resume)
			ops->resume();

	return ret;
}
EXPORT_SYMBOL_GPL(syscore_s2idle_suspend);

/**
 * syscore_s2idle_resume - Execute all the registered system core resume callbacks.
 *
 * This function is executed with one CPU on-line and disabled interrupts.
 */
void syscore_s2idle_resume(void)
{
	struct syscore_s2idle_ops *ops;

	trace_suspend_resume(TPS("syscore_s2idle_resume"), 0, true);
	WARN_ONCE(!irqs_disabled(),
		"Interrupts enabled before system core resume.\n");

	list_for_each_entry(ops, &syscore_s2idle_ops_list, node)
		if (ops->resume) {
			if (initcall_debug)
				pr_info("PM: Calling %pS\n", ops->resume);
			ops->resume();
			WARN_ONCE(!irqs_disabled(),
				"Interrupts enabled after %pS\n", ops->resume);
		}
	trace_suspend_resume(TPS("syscore_s2idle_resume"), 0, false);
}
EXPORT_SYMBOL_GPL(syscore_s2idle_resume);
#endif /* CONFIG_PM_SLEEP */

/**
 * syscore_s2idle_shutdown - Execute all the registered system core shutdown callbacks.
 */
void syscore_s2idle_shutdown(void)
{
	struct syscore_s2idle_ops *ops;

	mutex_lock(&syscore_s2idle_ops_lock);

	list_for_each_entry_reverse(ops, &syscore_s2idle_ops_list, node)
		if (ops->shutdown) {
			if (initcall_debug)
				pr_info("PM: Calling %pS\n", ops->shutdown);
			ops->shutdown();
		}

	mutex_unlock(&syscore_s2idle_ops_lock);
}

static int (*sprd_wdt_fiq_syscore_suspend_ptr)(void);
static void (*sprd_wdt_fiq_syscore_resume_ptr)(void);

static int __nocfi sprd_module_notifier_fn(struct notifier_block *nb,
					   unsigned long action,
					   void *data)
{
	struct module *module = data;
	struct syscore_s2idle_ops sprd_wdt_fiq_syscore_s2idle_ops;

	if (action != MODULE_STATE_LIVE)
		return NOTIFY_DONE;

	/* return immediately if the func has been found */
	if (sprd_wdt_fiq_syscore_suspend_ptr && sprd_wdt_fiq_syscore_resume_ptr)
		return NOTIFY_DONE;

	if (!strncmp(module->name, "sprd_wdt_fiq", strlen("sprd_wdt_fiq"))) {
		sprd_wdt_fiq_syscore_suspend_ptr = (void *)
			module_kallsyms_lookup_name("sprd_wdt_fiq_syscore_suspend");
		sprd_wdt_fiq_syscore_resume_ptr = (void *)
			module_kallsyms_lookup_name("sprd_wdt_fiq_syscore_resume");
	}

	if (!sprd_wdt_fiq_syscore_suspend_ptr)
		return NOTIFY_DONE;
	if (!sprd_wdt_fiq_syscore_resume_ptr)
		return NOTIFY_DONE;

	sprd_wdt_fiq_syscore_s2idle_ops.suspend = sprd_wdt_fiq_syscore_suspend_ptr;
	sprd_wdt_fiq_syscore_s2idle_ops.resume = sprd_wdt_fiq_syscore_resume_ptr;

	register_syscore_s2idle_ops(&sprd_wdt_fiq_syscore_s2idle_ops);

	printk("sprd wdt fiq syscore s2idle register done.\n");

	return NOTIFY_DONE;
}

static struct notifier_block sprd_module_notifier = {
	.notifier_call = sprd_module_notifier_fn,
};

static int __init syscore_s2idle_init(void)
{
	/* register a notifier which called when modules install */
	register_module_notifier(&sprd_module_notifier);

	return 0;
}

late_initcall(syscore_s2idle_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Suspend to idle syscore driver");
MODULE_AUTHOR("Ruifeng Zhang");
