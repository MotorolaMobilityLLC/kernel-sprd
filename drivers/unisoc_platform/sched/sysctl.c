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
unsigned int sysctl_walt_account_irq_time = 1;

/*up cap margin default value: ~20%*/
static unsigned int sysctl_sched_cap_margin_up_pct[MAX_CLUSTERS] = {
					[0 ... MAX_CLUSTERS-1] = 80};
/*down cap margin default value: ~20%*/
static unsigned int sysctl_sched_cap_margin_dn_pct[MAX_CLUSTERS]  = {
					[0 ... MAX_CLUSTERS-1] = 80};

unsigned int sched_cap_margin_up[WALT_NR_CPUS] = { [0 ... WALT_NR_CPUS-1] = 1280};
unsigned int sched_cap_margin_dn[WALT_NR_CPUS] = { [0 ... WALT_NR_CPUS-1] = 1280};

#ifdef CONFIG_PROC_SYSCTL
static void sched_update_cap_migrate_values(bool up)
{
	int i = 0, cpu;
	struct sched_cluster *cluster;
	int cap_margin_levels = num_sched_clusters ? num_sched_clusters : 1;

	/* per cluster should have a capacity_margin value. */
	for_each_sched_cluster(cluster) {
		for_each_cpu(cpu, &cluster->cpus) {
			if (up)
				sched_cap_margin_up[cpu] =
					SCHED_FIXEDPOINT_SCALE * 100 /
					sysctl_sched_cap_margin_up_pct[i];
			else
				sched_cap_margin_dn[cpu] =
					SCHED_FIXEDPOINT_SCALE * 100 /
					sysctl_sched_cap_margin_dn_pct[i];
		}
		if (++i >= cap_margin_levels)
			break;
	}
}

/*
 * userspace can use write to set new updowm capacity_margin value.
 * for example. echo 80 90 > sysctl_sched_cap_margin_up
 * echo 70 80 > sysctl_sched_cap_margin_dn.
 */
static int sched_updown_migrate_handler(struct ctl_table *table, int write,
				 void __user *buffer, size_t *lenp,
				 loff_t *ppos)
{
	int ret, i;
	unsigned int *data = (unsigned int *)table->data;
	static DEFINE_MUTEX(mutex);
	unsigned long cap_margin_levels = num_sched_clusters ? num_sched_clusters : 1;
	unsigned int val[MAX_CLUSTERS];
	struct ctl_table tmp = {
		.data   = &val,
		.maxlen = sizeof(int) * cap_margin_levels,
		.mode   = table->mode,
	};

	mutex_lock(&mutex);

	if (!write) {
		for (i = 0; i < cap_margin_levels; i++)
			val[i] = data[i];

		ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);

		goto unlock_mutex;
	}

	ret = proc_dointvec(&tmp, write, buffer, lenp, ppos);
	if (ret)
		goto unlock_mutex;

	/* check if pct values are valid */
	for (i = 0; i < cap_margin_levels; i++) {
		if (val[i] <= 0 || val[i] > 100) {
			ret = -EINVAL;
			goto unlock_mutex;
		}
	}

	for (i = 0; i < cap_margin_levels; i++)
		data[i] = val[i];

	sched_update_cap_migrate_values(data == &sysctl_sched_cap_margin_up_pct[0]);

unlock_mutex:
	mutex_unlock(&mutex);

	return ret;
}
#endif

#if IS_ENABLED(CONFIG_UNISOC_ROTATION_TASK)
unsigned int sysctl_rotation_enable = 1;
/* default threshold value is 40ms */
unsigned int sysctl_rotation_threshold_ms = 40;

struct ctl_table rotation_table[] = {
	{
		.procname       = "rotation_enable",
		.data           = &sysctl_rotation_enable,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "rotation_threshold_ms",
		.data		= &sysctl_rotation_threshold_ms,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ONE,
		.extra2		= &one_thousand,
	},
	{ },
};
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
	{
		.procname	= "walt_account_irq_time",
		.data		= &sysctl_walt_account_irq_time,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
#ifdef CONFIG_PROC_SYSCTL
	{
		.procname	= "sched_cap_margin_up",
		.data		= &sysctl_sched_cap_margin_up_pct,
		.maxlen		= sizeof(unsigned int) * MAX_CLUSTERS,
		.mode		= 0644,
		.proc_handler	= sched_updown_migrate_handler,
	},
	{
		.procname	= "sched_cap_margin_down",
		.data		= &sysctl_sched_cap_margin_dn_pct,
		.maxlen		= sizeof(unsigned int) * MAX_CLUSTERS,
		.mode		= 0644,
		.proc_handler	= sched_updown_migrate_handler,
	},
#endif
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
#if IS_ENABLED(CONFIG_UNISOC_ROTATION_TASK)
	{
		.procname	= "rotation",
		.mode		= 0555,
		.child		= rotation_table,
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
