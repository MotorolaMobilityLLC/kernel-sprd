// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, The Linux Foundation. All rights reserved.
 */

#include <trace/hooks/sched.h>

#include "walt.h"

static DEFINE_PER_CPU(cpumask_var_t, walt_local_cpu_mask);

static int is_idle_cpu(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	if (rq->curr != rq->idle)
		return 0;

	if (rq->nr_running)
		return 0;

#ifdef CONFIG_SMP
	if (rq->ttwu_pending)
		return 0;
#endif

	return 1;
}

#ifdef CONFIG_UCLAMP_TASK
/*
 * Verify the fitness of task @p to run on @cpu taking into account the uclamp
 * settings.
 *
 * This check is only important for heterogeneous systems where uclamp_min value
 * is higher than the capacity of a @cpu. For non-heterogeneous system this
 * function will always return true.
 *
 * The function will return true if the capacity of the @cpu is >= the
 * uclamp_min and false otherwise.
 *
 * Note that uclamp_min will be clamped to uclamp_max if uclamp_min
 * > uclamp_max.
 */
static inline bool walt_rt_task_fits_capacity(struct task_struct *p, int cpu)
{
	unsigned long cpu_cap;
	unsigned long task_util;

	task_util = uclamp_task_util(p);

	cpu_cap = capacity_orig_of(cpu);
	cpu_cap -= arch_scale_thermal_pressure(cpu);

	return cpu_cap > task_util;
}
#else
static inline bool walt_rt_task_fits_capacity(struct task_struct *p, int cpu)
{
	return true;
}
#endif
static void walt_rt_filter_energy_cpu(void *data, struct task_struct *task,
				struct cpumask *lowest_mask, int ret, int *best_cpu)
{
	int cpu, best_active_cpu = -1, best_idle_cpu = -1;
	int prev_cpu = task_cpu(task);
	unsigned long task_util = uclamp_task_util(task);
	unsigned long best_idle_cap = ULONG_MAX;
	unsigned long best_active_cap = ULONG_MAX;
	unsigned int min_exit_lat = UINT_MAX;
	unsigned long min_util = UINT_MAX;
	struct cpuidle_state *idle;

	if (static_branch_unlikely(&walt_disabled))
		return;

	if (!ret)
		return; /* No targets found */

	/* fast path for prev_cpu */
	if (cpumask_test_cpu(prev_cpu, lowest_mask) && is_idle_cpu(prev_cpu) &&
	    is_min_capacity_cpu(prev_cpu)) {
		*best_cpu = prev_cpu;
		return;
	}

	for_each_cpu_and(cpu, lowest_mask, cpu_online_mask) {
		unsigned long cpu_util;
		unsigned long cpu_cap;
		unsigned int idle_exit_latency;

		if (!cpumask_test_cpu(cpu, task->cpus_ptr))
			continue;

		cpu_util = walt_cpu_util(cpu) + task_util;
		cpu_cap = capacity_orig_of(cpu);

		if (cpu_util * sched_cap_margin_up[cpu] > cpu_cap * 1024)
			continue;

		if (is_idle_cpu(cpu)) {
			/* fast path for prev_cpu */
			if (is_min_capacity_cpu(cpu) && cpu == prev_cpu) {
				*best_cpu = cpu;
				return;
			}

			idle = idle_get_state(cpu_rq(cpu));
			if (idle)
				idle_exit_latency = idle->exit_latency;
			else
				idle_exit_latency = 0;

			if (cpu_cap < best_idle_cap || (cpu_cap == best_idle_cap &&
					idle_exit_latency < min_exit_lat)) {
				best_idle_cpu = cpu;
				best_idle_cap = cpu_cap;
				min_exit_lat = idle_exit_latency;
			}

			continue;
		}

		if (cpu_cap < best_active_cap || (cpu_cap == best_active_cap &&
							cpu_util < min_util)) {
			best_active_cpu = cpu;
			best_active_cap = cpu_cap;
			min_util = cpu_util;
		}
	}

	/*prefer small idle core */
	if (best_idle_cpu >= 0 && is_min_capacity_cpu(best_idle_cpu)) {
		*best_cpu = best_idle_cpu;
		return;
	}

	if (best_active_cpu >= 0 && is_min_capacity_cpu(best_active_cpu)) {
		*best_cpu = best_active_cpu;
		return;
	}

	*best_cpu = best_idle_cpu > 0 ? best_idle_cpu : best_active_cpu;
}

static void walt_select_task_rq_rt(void *data, struct task_struct *p, int cpu,
					int sd_flag, int wake_flags, int *new_cpu)
{
	struct task_struct *curr;
	struct cpumask *lowest_mask = this_cpu_cpumask_var_ptr(walt_local_cpu_mask);
	struct rq *rq;
	bool test;
	int target = -1;
	bool may_not_preempt;
	int ret;

	if (static_branch_unlikely(&walt_disabled))
		return;

	/* For anything but wake ups, just return the task_cpu */
	if (!(wake_flags & (WF_TTWU | WF_FORK)))
		return;

	/* Make sure the mask is initialized first */
	if (unlikely(!lowest_mask))
		return;

	rq = cpu_rq(cpu);

	rcu_read_lock();
	curr = READ_ONCE(rq->curr); /* unlocked access */

	if (p->nr_cpus_allowed == 1)
		goto unlock; /* No other targets possible */
	/*
	 * If the current task on @p's runqueue is a softirq task,
	 * it may run without preemption for a time that is
	 * ill-suited for a waiting RT task. Therefore, try to
	 * wake this RT task on another runqueue.
	 *
	 * Also, if the current task on @p's runqueue is an RT task, then
	 * try to see if we can wake this RT task up on another
	 * runqueue. Otherwise simply start this RT task
	 * on its current runqueue.
	 *
	 * We want to avoid overloading runqueues. If the woken
	 * task is a higher priority, then it will stay on this CPU
	 * and the lower prio task should be moved to another CPU.
	 * Even though this will probably make the lower prio task
	 * lose its cache, we do not want to bounce a higher task
	 * around just because it gave up its CPU, perhaps for a
	 * lock?
	 *
	 * For equal prio tasks, we just let the scheduler sort it out.
	 *
	 * Otherwise, just let it ride on the affined RQ and the
	 * post-schedule router will push the preempted task away
	 *
	 * This test is optimistic, if we get it wrong the load-balancer
	 * will have to sort it out.
	 *
	 * We take into account the capacity of the CPU to ensure it fits the
	 * requirement of the task - which is only important on heterogeneous
	 * systems like big.LITTLE.
	 */
	may_not_preempt = task_may_not_preempt(curr, cpu);
	test = (curr && (may_not_preempt || (unlikely(rt_task(curr)) &&
		(curr->nr_cpus_allowed < 2 || curr->prio <= p->prio))));

	ret = cpupri_find_fitness(&task_rq(p)->rd->cpupri, p, lowest_mask,
				  walt_rt_task_fits_capacity);

	walt_rt_filter_energy_cpu(NULL, p, lowest_mask, ret, &target);

	/*
	 * Bail out if we were forcing a migration to find a better
	 * fitting CPU but our search failed.
	 */
	if (!test && target != -1 && !walt_rt_task_fits_capacity(p, target)) {
		*new_cpu = cpu;
		goto unlock;
	}

	/*
	 * If cpu is non-preemptible, prefer remote cpu
	 * even if it's running a higher-prio task.
	 * Otherwise: Don't bother moving it if the destination CPU is
	 * not running a lower priority task.
	 */
	if (target != -1 &&
	    (may_not_preempt || p->prio < cpu_rq(target)->rt.highest_prio.curr))
		*new_cpu = target;

unlock:
	rcu_read_unlock();

}

void walt_rt_init(void)
{
	unsigned int i;

	for_each_possible_cpu(i) {
		if (!(zalloc_cpumask_var_node(&per_cpu(walt_local_cpu_mask, i),
						GFP_KERNEL, cpu_to_node(i)))) {
			pr_err("walt_local_cpu_mask alloc failed for cpu%d\n", i);
			return;
		}
	}

	register_trace_android_rvh_select_task_rq_rt(walt_select_task_rq_rt, NULL);
	register_trace_android_rvh_find_lowest_rq(walt_rt_filter_energy_cpu, NULL);

}
