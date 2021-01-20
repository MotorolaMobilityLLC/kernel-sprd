/* SPDX-License-Identifier: GPL-2.0-only */
/*
 *  syscore_s2idle_ops.h - System core operations.
 */

#ifndef _LINUX_SYSCORE_S2IDLE_OPS_H
#define _LINUX_SYSCORE_S2IDLE_OPS_H

#include <linux/list.h>

struct syscore_s2idle_ops {
	struct list_head node;
	int (*suspend)(void);
	void (*resume)(void);
	void (*shutdown)(void);
};

extern void register_syscore_s2idle_ops(struct syscore_s2idle_ops *ops);
extern void unregister_syscore_s2idle_ops(struct syscore_s2idle_ops *ops);
#ifdef CONFIG_PM_SLEEP
extern int syscore_s2idle_suspend(void);
extern void syscore_s2idle_resume(void);
#endif
extern void syscore_s2idle_shutdown(void);

#endif
