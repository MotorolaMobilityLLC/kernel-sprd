// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, The Linux Foundation. All rights reserved.
 */

#include <trace/hooks/sched.h>

#include "walt.h"
#include "trace.h"

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

/*
 * The policy of a RT boosted task (via PI mutex) still is a fair task,
 * so use prio check as well. The prio check alone is not sufficient
 * since idle task's prio is also 120.
 */
static inline bool is_fair_task(struct task_struct *p)
{
	return p->prio >= MAX_RT_PRIO && !is_idle_task(p);
}

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

static bool cpu_overutilized(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	if (is_max_capacity_cpu(cpu)) {
		if (is_idle_cpu(cpu) || rq->nr_running <= 1)
			return false;
	}

	return walt_cpu_util(cpu) * sched_cap_margin_up[cpu] >
					capacity_orig_of(cpu) * 1024;
}

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

static inline int task_fits_capacity(struct task_struct *p, unsigned long capacity, int cpu)
{
	unsigned int margin;

	if (capacity_orig_of(task_cpu(p)) > capacity_orig_of(cpu))
		margin = sched_cap_margin_dn[cpu];
	else
		margin = sched_cap_margin_up[cpu];

	return uclamp_task_util(p) * margin < capacity * 1024;
}

static inline int util_fits_capacity(unsigned long util, unsigned long capacity,
					int prev_cpu, int cpu)
{
	unsigned int margin;

	if (capacity_orig_of(prev_cpu) > capacity_orig_of(cpu))
		margin = sched_cap_margin_dn[cpu];
	else
		margin = sched_cap_margin_up[cpu];

	return util * margin < capacity * 1024;
}

#if IS_ENABLED(CONFIG_UNISOC_ROTATION_TASK)
/* ========================= define data struct =========================== */
struct rotation_data {
	struct task_struct *rotation_thread;
	struct task_struct *src_task;
	struct task_struct *dst_task;
	int src_cpu;
	int dst_cpu;
};

static DEFINE_PER_CPU(struct rotation_data, rotation_datas);

#define ENABLE_DELAY_SEC	60
#define BIG_TASK_NUM		4
/* default enable rotation feature */
static bool rotation_enable;
#define threshold_time (sysctl_rotation_threshold_ms * 1000000)

/* after system start 30s, start rotation feature.*/
static struct timer_list rotation_timer;

/* core function */
static void check_for_task_rotation(struct rq *src_rq)
{
	int i, src_cpu = cpu_of(src_rq);
	struct rq *dst_rq;
	int deserved_cpu = nr_cpu_ids, dst_cpu = nr_cpu_ids;
	struct rotation_data *rd = NULL;
	u64 wc, wait, max_wait = 0;
	u64 run, max_run = 0;
	int big_task = 0;
	struct walt_task_ravg *wtr;

	if (!rotation_enable || !sysctl_rotation_enable)
		return;

	if (!is_min_capacity_cpu(src_cpu))
		return;

	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);
		struct task_struct *curr_task = rq->curr;

		if (is_fair_task(curr_task) &&
		    !task_fits_capacity(curr_task, capacity_of(i), i))
			big_task += 1;
	}
	if (big_task < BIG_TASK_NUM)
		return;

	wc = walt_ktime_clock();
	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);
		struct task_struct *curr_task = rq->curr;

		if (!is_min_capacity_cpu(i) || is_reserved(i))
			continue;

		if (!rq->misfit_task_load || !is_fair_task(curr_task) ||
		    task_fits_capacity(curr_task, capacity_of(i), i))
			continue;

		wtr = (struct walt_task_ravg *) curr_task->android_vendor_data1;
		wait = wc - wtr->last_enqueue_ts;
		if (wait > max_wait) {
			max_wait = wait;
			deserved_cpu = i;
		}
	}

	if (deserved_cpu != src_cpu)
		return;

	for_each_possible_cpu(i) {
		struct rq *rq = cpu_rq(i);

		if (is_min_capacity_cpu(i) || is_reserved(i))
			continue;

		if (!is_fair_task(rq->curr))
			continue;

		if (rq->nr_running > 1)
			continue;

		wtr = (struct walt_task_ravg *) rq->curr->android_vendor_data1;
		run = wc - wtr->last_enqueue_ts;

		if (run < threshold_time)
			continue;

		if (run > max_run) {
			max_run = run;
			dst_cpu = i;
		}
	}

	if (dst_cpu == nr_cpu_ids)
		return;

	dst_rq = cpu_rq(dst_cpu);

	double_rq_lock(src_rq, dst_rq);
	if (is_fair_task(dst_rq->curr) &&
		!src_rq->active_balance && !dst_rq->active_balance &&
		cpumask_test_cpu(dst_cpu, src_rq->curr->cpus_ptr) &&
		cpumask_test_cpu(src_cpu, dst_rq->curr->cpus_ptr)) {

		get_task_struct(src_rq->curr);
		get_task_struct(dst_rq->curr);

		mark_reserved(src_cpu);
		mark_reserved(dst_cpu);

		rd = &per_cpu(rotation_datas, src_cpu);

		rd->src_task = src_rq->curr;
		rd->dst_task = dst_rq->curr;

		rd->src_cpu = src_cpu;
		rd->dst_cpu = dst_cpu;

		src_rq->active_balance = 1;
		dst_rq->active_balance = 1;
	}
	double_rq_unlock(src_rq, dst_rq);

	if (rd) {
		wake_up_process(rd->rotation_thread);
		trace_sched_task_rotation(rd->src_cpu, rd->dst_cpu,
				rd->src_task->pid, rd->dst_task->pid);
	}
}

static void do_rotation_task(struct rotation_data *rd)
{
	unsigned long flags;
	struct rq *src_rq = cpu_rq(rd->src_cpu), *dst_rq = cpu_rq(rd->dst_cpu);

	migrate_swap(rd->src_task, rd->dst_task, rd->dst_cpu, rd->src_cpu);

	put_task_struct(rd->src_task);
	put_task_struct(rd->dst_task);

	local_irq_save(flags);
	double_rq_lock(src_rq, dst_rq);
	dst_rq->active_balance = 0;
	src_rq->active_balance = 0;
	double_rq_unlock(src_rq, dst_rq);
	local_irq_restore(flags);

	clear_reserved(rd->src_cpu);
	clear_reserved(rd->dst_cpu);
}

static int __ref try_rotation_task(void *data)
{
	struct rotation_data *rd = data;

	do {
		do_rotation_task(rd);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	} while (!kthread_should_stop());

	return 0;
}

static void set_rotation_enable(struct timer_list *t)
{
	rotation_enable = true;
	pr_info("start rotation feature\n");
}

static void rotation_task_init(void)
{
	int ret = 0;
	int i;

	rotation_enable = false;

	for_each_possible_cpu(i) {
		struct rotation_data *rd = &per_cpu(rotation_datas, i);
		struct sched_param param = { .sched_priority = 49 };
		struct task_struct *thread;

		thread = kthread_create(try_rotation_task, (void *)rd,
					"rotation/%d", i);
		if (IS_ERR(thread))
			return;

		ret = sched_setscheduler_nocheck(thread, SCHED_FIFO, &param);
		if (ret) {
			kthread_stop(thread);
			return;
		}

		rd->rotation_thread = thread;
	}

	timer_setup(&rotation_timer, set_rotation_enable, 0);
	rotation_timer.expires = jiffies + ENABLE_DELAY_SEC * HZ;
	add_timer(&rotation_timer);

	pr_info("%s OK\n", __func__);
}
#endif
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
	unsigned int margin;

	if (capacity_orig == max_capacity && is_idle_cpu(cpu))
		return true;

	capacity = capacity_orig - thermal_pressure;

	cpu_util = cpu_util_without(cpu, p);
	cpu_util += walt_task_util(p);
	cpu_util = walt_uclamp_rq_util_with(cpu_rq(cpu), cpu_util, p);

	if (capacity_orig_of(task_cpu(p)) > capacity_orig)
		margin = sched_cap_margin_dn[cpu];
	else
		margin = sched_cap_margin_up[cpu];

	return cpu_util * margin <  capacity * 1024;
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

		if (!cpumask_test_cpu(cpu, p->cpus_ptr) || is_reserved(cpu))
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

			if (!cpumask_test_cpu(cpu, p->cpus_ptr) || is_reserved(cpu))
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

			if (!big_is_idle &&
			    !util_fits_capacity(util, cpu_cap, prev_cpu, cpu))
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

/*
 * detach_task() -- detach the task for the migration specified in env
 */
static void walt_detach_task(struct task_struct *p, struct rq *src_rq,
						    struct rq *dst_rq)
{

	lockdep_assert_rq_held(src_rq);

	deactivate_task(src_rq, p, 0);
	double_lock_balance(src_rq, dst_rq);
	if (!(src_rq->clock_update_flags & RQCF_UPDATED))
		update_rq_clock(src_rq);
	set_task_cpu(p, dst_rq->cpu);
	double_unlock_balance(src_rq, dst_rq);
}

static void walt_attach_task(struct rq *rq, struct task_struct *p)
{
	lockdep_assert_rq_held(rq);

	BUG_ON(task_rq(p) != rq);
	activate_task(rq, p, ENQUEUE_NOCLOCK);
	check_preempt_curr(rq, p, 0);
}

/*
 * attach_one_task() -- attaches the task returned from detach_one_task() to
 * its new rq.
 */
static void walt_attach_one_task(struct rq *rq, struct task_struct *p)
{
	struct rq_flags rf;

	rq_lock(rq, &rf);
	update_rq_clock(rq);
	walt_attach_task(rq, p);
	rq_unlock(rq, &rf);
}

static void walt_migrate_queued_task(void *data, struct rq *rq,
				     struct rq_flags *rf, struct task_struct *p,
				     int new_cpu, int *detached)
{
	if (static_branch_unlikely(&walt_disabled))
		return;

	/*
	 * WALT expects both source and destination rqs to be
	 * held when set_task_cpu() is called on a queued task.
	 * so implementing this detach hook. unpin the lock
	 * before detaching and repin it later to make lockdep
	 * happy.
	 */
	BUG_ON(!rf);

	rq_unpin_lock(rq, rf);
	walt_detach_task(p, rq, cpu_rq(new_cpu));
	rq_repin_lock(rq, rf);

	*detached = 1;
}

static void walt_can_migrate_task(void *data, struct task_struct *p,
				  int dst_cpu, int *can_migrate)
{
	struct walt_rq *wrq = (struct walt_rq *) task_rq(p)->android_vendor_data1;

	if (static_branch_unlikely(&walt_disabled))
		return;

	/* Don't detach task if it is under active migration */
	if (unlikely(wrq->push_task == p))
		*can_migrate = 0;
}

static void walt_find_busiest_group(void *data, struct sched_group *busiest,
				    struct rq *dst_rq, int *out_balance)
{
	int busiest_cpu;

	if (static_branch_unlikely(&walt_disabled))
		return;

	if (!busiest)
		return;

	/*there is only one cpu in group */
	busiest_cpu = group_first_cpu(busiest);

	/* it's not necessary to pull task when cpus belong to
	 * same cluster and the buiest_cpu's running is <=1;
	 */
	if (same_cluster(busiest_cpu, cpu_of(dst_rq)) &&
	    cpu_rq(busiest_cpu)->nr_running > 1)
		*out_balance = 0;

}

/*
 * static void walt_find_busiest_queue(void *data, int dst_cpu,
 *                                     struct sched_group *group,
 *                                     struct cpumask *env_cpus,
 *                                     struct rq **busiest, int *done)
 * {
 *         if (static_branch_unlikely(&walt_disabled))
 *                 return;
 * }
 */

static void walt_nohz_balancer_kick(void *data, struct rq *rq,
					unsigned int *flags, int *done)
{
	if (static_branch_unlikely(&walt_disabled))
		return;

	if (rq->nr_running >= 2 && (cpu_overutilized(rq->cpu) ||
		is_min_capacity_cpu(rq->cpu)))
		*flags = NOHZ_KICK_MASK;

	*done = 1;
}

static void walt_find_new_ilb(void *data, struct cpumask *nohz_idle_cpus_mask,
					  int *ilb)
{
	int cpu = smp_processor_id();
	cpumask_t idle_cpus, tmp_cpus;
	struct sched_cluster *cluster;
	unsigned long ref_cap = capacity_orig_of(cpu);
	unsigned long best_cap, best_cap_cpu = -1;
	int is_small_cpu;

	if (static_branch_unlikely(&walt_disabled))
		return;

	cpumask_and(&idle_cpus, nohz_idle_cpus_mask,
			housekeeping_cpumask(HK_FLAG_MISC));

	if (cpumask_empty(&idle_cpus))
		return;

	is_small_cpu = is_min_capacity_cpu(cpu);
	best_cap = is_small_cpu ? ULONG_MAX : 0;

	for_each_sched_cluster(cluster) {
		int i;
		unsigned long cap;

		cpumask_and(&tmp_cpus, &idle_cpus, &cluster->cpus);

		/* This cluster did not have any idle CPUs */
		if (cpumask_empty(&tmp_cpus))
			continue;

		i = cpumask_first(&tmp_cpus);

		cap = capacity_orig_of(i);

		/* The first preference is for the same capacity CPU */
		if (cap == ref_cap) {
			*ilb = i;
			goto out;
		}

		/*
		 * When there are no idle CPUs in the same cluster, prefer cpu
		 * with best capacity:
		 * this_cpu is:
		 * small cpu : prefer middle cpu;
		 * middle cpu: prefer big cpu;
		 * big cpu   : prefer middle cpu;
		 */
		if (is_small_cpu) {
			if (cap < best_cap) {
				best_cap = cap;
				best_cap_cpu = i;
			}
		} else {
			if (cap > best_cap) {
				best_cap = cap;
				best_cap_cpu = i;
			}
		}

	}
out:
	*ilb = best_cap_cpu;

	trace_sched_find_new_ilb(cpu, ref_cap, best_cap_cpu, best_cap, *ilb);
}

static int walt_active_migration_cpu_stop(void *data)
{
	struct rq *busiest_rq = data;
	int busiest_cpu = cpu_of(busiest_rq);
	int target_cpu = busiest_rq->push_cpu;
	struct rq *target_rq = cpu_rq(target_cpu);
	struct walt_rq *busiest_wrq = (struct walt_rq *) busiest_rq->android_vendor_data1;
	struct task_struct *push_task;
	struct rq_flags rf;
	int push_task_detached = 0;

	rq_lock_irq(busiest_rq, &rf);
	push_task = busiest_wrq->push_task;

	if (!cpu_active(busiest_cpu) || !cpu_active(target_cpu) || !push_task)
		goto out_unlock;

	/* Make sure the requested CPU hasn't gone down in the meantime: */
	if (unlikely(busiest_cpu != smp_processor_id() ||
		     !busiest_rq->active_balance))
		goto out_unlock;

	/* Is there any task to move? */
	if (busiest_rq->nr_running <= 1)
		goto out_unlock;
	/*
	 * This condition is "impossible", if it occurs
	 * we need to fix it. Originally reported by
	 * Bjorn Helgaas on a 128-CPU setup.
	 */
	BUG_ON(busiest_rq == target_rq);

	if (task_on_rq_queued(push_task) &&
	    READ_ONCE(push_task->__state) == TASK_RUNNING &&
	    task_cpu(push_task) == busiest_cpu &&
	    cpu_active(target_cpu) &&
	    cpumask_test_cpu(target_cpu, push_task->cpus_ptr)) {
		update_rq_clock(busiest_rq);
		walt_detach_task(push_task, busiest_rq, target_rq);
		push_task_detached = 1;
	}

out_unlock:
	busiest_rq->active_balance = 0;
	clear_reserved(target_cpu);
	busiest_wrq->push_task = NULL;
	rq_unlock(busiest_rq, &rf);

	if (push_task_detached)
		walt_attach_one_task(target_rq, push_task);

	if (push_task)
		put_task_struct(push_task);

	local_irq_enable();

	return 0;
}

static DEFINE_RAW_SPINLOCK(migration_lock);
static void android_vh_scheduler_tick(void *unused, struct rq *rq)
{
	int prev_cpu = rq->cpu, new_cpu;
	struct task_struct *p = rq->curr;
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	int ret;

	if (static_branch_unlikely(&walt_disabled))
		return;

	if (!is_fair_task(p) || !rq->misfit_task_load ||
	    READ_ONCE(p->__state) != TASK_RUNNING || p->nr_cpus_allowed == 1)
		return;

	raw_spin_lock(&migration_lock);

	rcu_read_lock();
	new_cpu = walt_find_energy_efficient_cpu(p, prev_cpu, 0);
	rcu_read_unlock();

	if ((new_cpu != -1) &&
	    (capacity_orig_of(new_cpu) > capacity_orig_of(prev_cpu))) {
		/* Invoke active balance to force migrate currently running task */
		raw_spin_rq_lock(rq);

		if (rq->active_balance) {
			raw_spin_rq_unlock(rq);
			goto out_unlock;
		}

		rq->active_balance = 1;
		rq->push_cpu = new_cpu;
		get_task_struct(p);
		wrq->push_task = p;

		raw_spin_rq_unlock(rq);

		mark_reserved(new_cpu);

		raw_spin_unlock(&migration_lock);

		trace_sched_active_migration(p, prev_cpu, new_cpu);

		ret = stop_one_cpu_nowait(prev_cpu, walt_active_migration_cpu_stop,
					rq, &rq->active_balance_work);

		if (!ret)
			clear_reserved(new_cpu);

		return;
	} else {
		check_for_task_rotation(rq);
	}

out_unlock:
	raw_spin_unlock(&migration_lock);
}

static void walt_cpu_overutilzed(void *data, int cpu, int *overutilized)
{
	if (static_branch_unlikely(&walt_disabled))
		return;

	*overutilized = cpu_overutilized(cpu);
}

static void android_rvh_update_misfit_status(void *data, struct task_struct *p,
					     struct rq *rq, bool *need_update)
{
	struct walt_task_ravg *wtr;
	struct walt_rq *wrq;

	if (static_branch_unlikely(&walt_disabled))
		return;

	*need_update = false;

	if (!p || p->nr_cpus_allowed == 1) {
		rq->misfit_task_load = 0;
		return;
	}

	wrq = (struct walt_rq *) rq->android_vendor_data1;
	wtr = (struct walt_task_ravg *) p->android_vendor_data1;

	if (is_max_capacity_cpu(cpu_of(rq)) ||
	    task_fits_capacity(p, capacity_orig_of(cpu_of(rq)), cpu_of(rq))) {
		rq->misfit_task_load = 0;
		return;
	}

	/*
	 * Make sure that misfit_task_load will not be null even if
	 * task_h_load() returns 0.
	 */
	rq->misfit_task_load = max_t(unsigned long, walt_task_util(p), 1);
}

void walt_fair_init(void)
{
	register_trace_android_rvh_update_misfit_status(android_rvh_update_misfit_status, NULL);
	register_trace_android_rvh_cpu_overutilized(walt_cpu_overutilzed, NULL);
	register_trace_android_vh_scheduler_tick(android_vh_scheduler_tick, NULL);
	register_trace_android_rvh_migrate_queued_task(walt_migrate_queued_task, NULL);
	register_trace_android_rvh_can_migrate_task(walt_can_migrate_task, NULL);
	register_trace_android_rvh_find_new_ilb(walt_find_new_ilb, NULL);
	register_trace_android_rvh_sched_nohz_balancer_kick(walt_nohz_balancer_kick, NULL);
//	register_trace_android_rvh_find_busiest_queue(walt_find_busiest_queue, NULL);
	register_trace_android_rvh_find_busiest_group(walt_find_busiest_group, NULL);
	register_trace_android_rvh_select_task_rq_fair(walt_select_task_rq_fair, NULL);

	rotation_task_init();
}
