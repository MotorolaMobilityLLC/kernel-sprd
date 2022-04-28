// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, The Linux Foundation. All rights reserved.
 */

#include <linux/sysctl.h>
#include "walt.h"

static int one_hundred = 100;
static int one_thousand = 1000;

unsigned int sysctl_sched_uclamp_threshold = 100;
#if IS_ENABLED(CONFIG_UCLAMP_MIN_TO_BOOST)
/* map util clamp_min to boost */
unsigned int sysctl_sched_uclamp_min_to_boost = 1;
#endif

struct ctl_table walt_table[] = {
	{
		.procname	= "sched_walt_init_task_load_pct",
		.data		= &sysctl_sched_walt_init_task_load_pct,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1         = SYSCTL_ZERO,
		.extra2         = &one_hundred,
	},
	{
		.procname	= "sched_walt_cpu_high_irqload",
		.data		= &sysctl_sched_walt_cpu_high_irqload,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname       = "sched_walt_busy_threshold",
		.data           = &sysctl_walt_busy_threshold,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1         = SYSCTL_ZERO,
		.extra2         = &one_hundred,
	},
	{
		.procname       = "sched_walt_cross_window_util",
		.data           = &sysctl_sched_walt_cross_window_util,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname       = "sched_walt_account_wait_time",
		.data           = &sysctl_walt_account_wait_time,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname       = "sched_walt_io_is_busy",
		.data           = &sysctl_walt_io_is_busy,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "sched_uclamp_threshold",
		.data		= &sysctl_sched_uclamp_threshold,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= &one_thousand,
	},
#if IS_ENABLED(CONFIG_UCLAMP_MIN_TO_BOOST)
	{
		.procname	= "sched_uclamp_min2boost",
		.data		= &sysctl_sched_uclamp_min_to_boost,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
#endif
	{ }
};

struct ctl_table walt_base_table[] = {
	{
		.procname	= "walt",
		.mode		= 0555,
		.child		= walt_table,
	},
	{ },
};
