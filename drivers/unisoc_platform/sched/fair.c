// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, The Linux Foundation. All rights reserved.
 */

#include <trace/hooks/sched.h>

#include "walt.h"
#include "trace.h"

#define cap_margin	1280

/*
 * Remove and clamp on negative, from a local variable.
 *
 * A variant of sub_positive(), which does not use explicit load-store
 * and is thus optimized for local variable updates.
 */
#define lsub_positive(_ptr, _val) do {				\
	typeof(_ptr) ptr = (_ptr);				\
	*ptr -= min_t(typeof(*ptr), *ptr, _val);		\
} while (0)

/**
 * is_idle_cpu - is a given CPU idle currently?
 * @cpu: the processor in question.
 *
 * Return: 1 if the CPU is currently idle. 0 otherwise.
 */
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
/*
 * The margin used when comparing utilization with CPU capacity.
 *
 * (default: ~20%)
 */
#define fits_capacity(cap, max)	((cap) * cap_margin < (max) * 1024)

static unsigned long cpu_util_without(int cpu, struct task_struct *p)
{
	unsigned long util;

	/*
	 * WALT does not decay idle tasks in the same manner
	 * as PELT, so it makes little sense to subtract task
	 * utilization from cpu utilization. Instead just use
	 * cpu_util for this case.
	 */
	if (likely(READ_ONCE(p->__state) == TASK_WAKING))
		return walt_cpu_util(cpu);

	/* Task has no contribution or is new */
	if (cpu != task_cpu(p) || !READ_ONCE(p->se.avg.last_update_time))
		return walt_cpu_util(cpu);

	util = max_t(long, walt_cpu_util(cpu) - walt_task_util(p), 0);

	return min_t(unsigned long, util, capacity_orig_of(cpu));
}

static inline unsigned long capacity_of(int cpu)
{
	return cpu_rq(cpu)->cpu_capacity;
}

static inline int task_fits_capacity(struct task_struct *p, long capacity)
{
	return fits_capacity(uclamp_task_util(p), capacity);
}

/*
 * walt_compute_energy(): Estimates the energy that @pd would consume if @p was
 * migrated to @dst_cpu. compute_energy() predicts what will be the utilization
 * landscape of @pd's CPUs after the task migration, and uses the Energy Model
 * to compute what would be the energy if we decided to actually migrate that
 * task.
 */
static long walt_compute_energy(struct task_struct *p, int dst_cpu,
				struct perf_domain *pd, struct pd_cache *pdc)
{
	struct cpumask *pd_mask = perf_domain_span(pd);
	unsigned long cpu_cap = arch_scale_cpu_capacity(cpumask_first(pd_mask));
	unsigned long max_util = 0, sum_util = 0;
	unsigned long _cpu_cap = cpu_cap;
	unsigned long energy = 0;
	int cpu;

	_cpu_cap -= arch_scale_thermal_pressure(cpumask_first(pd_mask));

	/*
	 * The capacity state of CPUs of the current rd can be driven by CPUs
	 * of another rd if they belong to the same pd. So, account for the
	 * utilization of these CPUs too by masking pd with cpu_online_mask
	 * instead of the rd span.
	 *
	 * If an entire pd is outside of the current rd, it will not appear in
	 * its pd list and will not be accounted by compute_energy().
	 */
	for_each_cpu_and(cpu, pd_mask, cpu_online_mask) {
		unsigned long cpu_util;
		struct task_struct *tsk = NULL;

		cpu_util = pdc[cpu].wake_util;
		if (cpu == dst_cpu) {
			tsk = p;
			cpu_util += walt_task_util(p);
		}

		cpu_util = walt_uclamp_rq_util_with(cpu_rq(cpu), cpu_util, tsk);

		sum_util += min(cpu_util, _cpu_cap);

		max_util = max(max_util, min(cpu_util, _cpu_cap));
	}

	energy = em_cpu_energy(pd->em_pd, max_util, sum_util, _cpu_cap);

	return energy;
}

static bool task_can_place_on_cpu(struct task_struct *p, int cpu)
{
	unsigned long capacity_orig = capacity_orig_of(cpu);
	unsigned long thermal_pressure = arch_scale_thermal_pressure(cpu);
	unsigned long max_capacity = max_possible_capacity;
	unsigned long capacity, cpu_util;

	if (capacity_orig == max_capacity && is_idle_cpu(cpu))
		return true;

	capacity = capacity_orig - thermal_pressure;

	cpu_util = cpu_util_without(cpu, p);
	cpu_util += walt_task_util(p);
	cpu_util = walt_uclamp_rq_util_with(cpu_rq(cpu), cpu_util, p);

	return fits_capacity(cpu_util, capacity);
}

static inline int select_cpu_when_overutiled(struct task_struct *p, int prev_cpu,
					     struct pd_cache *pdc)
{
	int cpu, best_active_cpu = -1, best_idle_cpu = -1;
	int max_cap_idle = INT_MIN, max_spare = INT_MIN, least_running = INT_MAX;
	struct cpuidle_state *idle;
	unsigned int min_exit_lat = UINT_MAX;

	for_each_online_cpu(cpu) {
		int spare_cap;
		struct rq *rq = cpu_rq(cpu);
		unsigned int idle_exit_latency = UINT_MAX;

		if (!cpumask_test_cpu(cpu, p->cpus_ptr))
			continue;

		if (is_idle_cpu(cpu)) {
			idle = idle_get_state(cpu_rq(cpu));
			if (idle)
				idle_exit_latency = idle->exit_latency;
			else
				idle_exit_latency = 0;

			if (pdc[cpu].cap > max_cap_idle) {
				best_idle_cpu = cpu;
				max_cap_idle = pdc[cpu].cap;
				min_exit_lat = idle_exit_latency;
			} else if (best_idle_cpu >= 0 &&
				   pdc[cpu].cap == max_cap_idle &&
				   idle_exit_latency < min_exit_lat) {
				best_idle_cpu = cpu;
				max_cap_idle = pdc[cpu].cap;
				min_exit_lat = idle_exit_latency;
			}

			continue;
		}

		spare_cap = pdc[cpu].cap - pdc[cpu].wake_util;
		if (spare_cap > max_spare) {
			max_spare = spare_cap;
			best_active_cpu = cpu;
			least_running = rq->nr_running;
		} else if (spare_cap == max_spare &&
				rq->nr_running < least_running) {
			max_spare = spare_cap;
			best_active_cpu = cpu;
			least_running = rq->nr_running;
		}
	}

	return best_idle_cpu >= 0 ? best_idle_cpu : best_active_cpu;

}

static inline int select_cpu_with_same_energy(int prev_cpu, int best_cpu,
					struct pd_cache *pdc, bool boosted)
{
	/* the prev_cpu and the best_cpu belong to the same cluster */
	if (boosted && pdc[prev_cpu].cap_orig == pdc[best_cpu].cap_orig &&
	    pdc[best_cpu].wake_util < pdc[prev_cpu].wake_util)
		return best_cpu;

	/* prefer smaller cluster */
	if (!boosted && pdc[prev_cpu].cap_orig > pdc[best_cpu].cap_orig)
		return best_cpu;

	return prev_cpu;
}

static inline void
snapshot_pd_cache_of(struct pd_cache *pd_cache, int cpu, struct task_struct *p)
{
	pd_cache[cpu].wake_util = cpu_util_without(cpu, p);
	pd_cache[cpu].cap_orig = capacity_orig_of(cpu);
	pd_cache[cpu].thermal_pressure = arch_scale_thermal_pressure(cpu);
	pd_cache[cpu].cap = pd_cache[cpu].cap_orig - pd_cache[cpu].thermal_pressure;
	pd_cache[cpu].is_idle = is_idle_cpu(cpu);
}

static int walt_find_energy_efficient_cpu(struct task_struct *p, int prev_cpu, int sync)
{
	unsigned long prev_delta = ULONG_MAX, best_delta = ULONG_MAX;
	int cpu = smp_processor_id();
	struct root_domain *rd = cpu_rq(cpu)->rd;
	int max_spare_cap_cpu_ls = prev_cpu, best_idle_cpu = -1;
	int best_energy_cpu = -1, target = -1;
	unsigned long max_spare_cap_ls = 0, target_cap = ULONG_MAX;
	unsigned long cpu_cap, util, uclamp_util, base_energy = 0;
	bool boosted, blocked, latency_sensitive = false;
	unsigned int min_exit_lat = UINT_MAX;
	struct cpuidle_state *idle;
	struct perf_domain *pd;
	struct pd_cache pdc[NR_CPUS];

	rcu_read_lock();
	pd = rcu_dereference(rd->pd);
	if (!pd)
		goto unlock;

	if (sync && cpu_rq(cpu)->nr_running == 1 &&
	    cpumask_test_cpu(cpu, p->cpus_ptr) &&
	    is_min_capacity_cpu(cpu) &&
	    task_can_place_on_cpu(p, cpu)) {
		rcu_read_unlock();
		return cpu;
	}

	uclamp_util = uclamp_task_util(p);
	latency_sensitive = uclamp_latency_sensitive(p);
	boosted = uclamp_boosted(p);
	blocked = uclamp_blocked(p);

	trace_sched_feec_task_info(p, prev_cpu, walt_task_util(p), uclamp_util,
				   boosted, latency_sensitive, blocked);

	for (; pd; pd = pd->next) {
		unsigned long cur_delta = ULONG_MAX, spare_cap, max_spare_cap = 0;
		bool compute_prev_delta = false;
		unsigned long base_energy_pd;
		int max_spare_cap_cpu = -1;

		for_each_cpu_and(cpu, perf_domain_span(pd), cpu_online_mask) {
			bool big_is_idle = false;
			unsigned int idle_exit_latency = UINT_MAX;

			snapshot_pd_cache_of(pdc, cpu, p);

			if (!cpumask_test_cpu(cpu, p->cpus_ptr))
				continue;

			/* speed up goto big core */
			util = pdc[cpu].wake_util + uclamp_util;
			cpu_cap = capacity_of(cpu);
			spare_cap = cpu_cap;
			lsub_positive(&spare_cap, util);

			if (pdc[cpu].is_idle) {
				idle = idle_get_state(cpu_rq(cpu));
				if (idle)
					idle_exit_latency = idle->exit_latency;
				else
					idle_exit_latency = 0;

				if (is_max_capacity_cpu(cpu))
					big_is_idle = true;
			}

			trace_sched_feec_rq_task_util(cpu, p, &pdc[cpu],
						      util, spare_cap, cpu_cap);

			if (!big_is_idle && !fits_capacity(util, cpu_cap))
				continue;

			if (blocked && is_min_capacity_cpu(cpu)) {
				target = cpu;
				goto unlock;
			}

			if (!latency_sensitive && cpu == prev_cpu) {
				/* Always use prev_cpu as a candidate. */
				compute_prev_delta = true;
			} else if (spare_cap > max_spare_cap) {
				/*
				 * Find the CPU with the maximum spare capacity
				 * in the performance domain.
				 */
				max_spare_cap = spare_cap;
				max_spare_cap_cpu = cpu;
			} else if (spare_cap == 0 && big_is_idle &&
				   max_spare_cap == 0) {
				max_spare_cap = spare_cap;
				max_spare_cap_cpu = cpu;
			}

			if (!latency_sensitive)
				continue;

			if (pdc[cpu].is_idle) {
				/* prefer idle CPU with lower cap_orig */
				if (pdc[cpu].cap_orig > target_cap)
					continue;

				if (idle && idle->exit_latency > min_exit_lat &&
				    pdc[cpu].cap_orig == target_cap)
					continue;

				if (best_idle_cpu == prev_cpu)
					continue;

				min_exit_lat = idle_exit_latency;
				target_cap = pdc[cpu].cap_orig;
				best_idle_cpu = cpu;
			} else if (spare_cap > max_spare_cap_ls) {
				max_spare_cap_ls = spare_cap;
				max_spare_cap_cpu_ls = cpu;
			}
		}

		if (latency_sensitive ||
		   (max_spare_cap_cpu < 0 && !compute_prev_delta))
			continue;

		/* Compute the 'base' energy of the pd, without @p */
		base_energy_pd = walt_compute_energy(p, -1, pd, pdc);
		base_energy += base_energy_pd;

		/* Evaluate the energy impact of using prev_cpu. */
		if (compute_prev_delta) {
			prev_delta = walt_compute_energy(p, prev_cpu, pd, pdc);
			prev_delta -= base_energy_pd;
			if (prev_delta < best_delta) {
				best_delta = prev_delta;
				best_energy_cpu = prev_cpu;
			}
		}

		/* Evaluate the energy impact of using max_spare_cap_cpu. */
		if (max_spare_cap_cpu >= 0) {
			cur_delta = walt_compute_energy(p, max_spare_cap_cpu, pd, pdc);
			cur_delta -= base_energy_pd;

			/* prefer small core when delta is equal, but it need
			 * satisfy the small core has the small cpu number.
			 */
			if (cur_delta <= best_delta) {
				best_delta = cur_delta;
				best_energy_cpu = max_spare_cap_cpu;
			}
		}
		trace_sched_energy_diff(base_energy_pd, base_energy, prev_delta,
					cur_delta, best_delta, prev_cpu,
					best_energy_cpu, max_spare_cap_cpu);
	}
	rcu_read_unlock();

	trace_sched_feec_candidates(prev_cpu, best_energy_cpu, base_energy, prev_delta,
				    best_delta, best_idle_cpu, max_spare_cap_cpu_ls);

	if (latency_sensitive)
		return best_idle_cpu >= 0 ? best_idle_cpu : max_spare_cap_cpu_ls;

	/* all cpus are overutiled */
	if (best_energy_cpu < 0)
		return select_cpu_when_overutiled(p, prev_cpu, pdc);
	/*
	 * Pick the best CPU if prev_cpu cannot be used, or if it saves at
	 * least 6% of the energy used by prev_cpu.
	 */
	if (prev_delta == ULONG_MAX || best_energy_cpu == prev_cpu)
		return best_energy_cpu;

	if ((prev_delta - best_delta) > ((prev_delta + base_energy) >> 4))
		return best_energy_cpu;

	return select_cpu_with_same_energy(prev_cpu, best_energy_cpu,
					   pdc, boosted);

	return -1;

unlock:
	rcu_read_unlock();

	return target;

}

static void walt_select_task_rq_fair(void *data, struct task_struct *p, int prev_cpu,
					int sd_flag, int wake_flags, int *target_cpu)
{
	int sync;

	if (static_branch_unlikely(&walt_disabled))
		return;

	sync = (wake_flags & WF_SYNC) && !(current->flags & PF_EXITING);

	*target_cpu = walt_find_energy_efficient_cpu(p, prev_cpu, sync);
}

void walt_fair_init(void)
{
	register_trace_android_rvh_select_task_rq_fair(walt_select_task_rq_fair, NULL);
}
