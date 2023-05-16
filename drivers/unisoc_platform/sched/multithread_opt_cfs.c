// SPDX-License-Identifier: GPL-2.0-only
/*
 * Antutu Mutithread case pf need improve
 * Copyright (C) 2022 Unisoc corporation. http://www.unisoc.com
 */

#include <linux/sched.h>
#include <linux/reciprocal_div.h>
#include <trace/hooks/sched.h>
#include <trace/events/sched.h>
#include <linux/moduleparam.h>
#include <linux/module.h>

#include "uni_sched.h"

#define HEAVY_LOAD_RUNTIME     (1024000000)
#define HEAVY_LOAD_SCALE       (80)

static bool multi_thread_enable(void)
{
	return (sysctl_cpu_multi_thread_opt == 1) ? true : false;
}

static bool is_heavy_load_task(struct task_struct *p)
{
	int cpu;
	unsigned long thresh_load;
	struct reciprocal_value spc_rdiv = reciprocal_value(100);

	if (!sysctl_cpu_multi_thread_opt || !p)
		return false;

	for_each_cpu(cpu, cpu_active_mask) {
		struct rq *rq = cpu_rq(cpu);
		struct task_struct *p_curr = rq->curr;

		thresh_load = capacity_orig_of(cpu) * HEAVY_LOAD_SCALE;
		if (uclamp_task_util(p_curr) >= reciprocal_divide(thresh_load, spc_rdiv))
			continue;
		else
			return false;
	}
	return true;
}

static void check_preempt_tick_handler(void *data, struct task_struct *p,
				unsigned long *ideal_runtime, bool *skip_preempt,
				unsigned long delta_exec, struct cfs_rq *cfs_rq,
				struct sched_entity *curr, unsigned int granularity)
{
	if (unlikely(multi_thread_enable() && is_heavy_load_task(p)))
		*ideal_runtime = HEAVY_LOAD_RUNTIME;
}

static void check_preempt_wakeup_handler(void *data, struct rq *rq, struct task_struct *p,
				bool *preempt, bool *nopreempt, int wake_flags,
				struct sched_entity *se, struct sched_entity *pse,
				int next_buddy_marked, unsigned int granularity)
{
	if (unlikely(multi_thread_enable() && is_heavy_load_task(rq->curr)))
		*nopreempt = true;
}

static void sched_rebalance_domains_handler(void *data, struct rq *rq, int *continue_balancing)
{
	if (unlikely(multi_thread_enable() && is_heavy_load_task(rq->curr)))
		*continue_balancing = 0;
}

int init_multithread_opt(void)
{
	register_trace_android_rvh_check_preempt_tick(check_preempt_tick_handler, NULL);
	register_trace_android_rvh_check_preempt_wakeup(check_preempt_wakeup_handler, NULL);
	register_trace_android_rvh_sched_rebalance_domains(sched_rebalance_domains_handler, NULL);
	return 0;
}

MODULE_LICENSE("GPL v2");
