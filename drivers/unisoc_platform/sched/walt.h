// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2022, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Unisoc, Inc.
 */
#ifndef _WALT_H
#define _WALT_H

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/unisoc_vd_def.h>

#include "trace.h"
#include "../../../kernel/sched/sched.h"

#define WALT_NR_CPUS	8
#define MAX_CLUSTERS	3

extern bool walt_disabled;

extern __read_mostly unsigned int walt_ravg_window;

enum task_event {
	PUT_PREV_TASK   = 0,
	PICK_NEXT_TASK  = 1,
	TASK_WAKE       = 2,
	TASK_MIGRATE    = 3,
	TASK_UPDATE     = 4,
	IRQ_UPDATE	= 5,
};

struct pd_cache {
	unsigned long wake_util;
	unsigned long cap_orig;
	unsigned long cap;
	unsigned long thermal_pressure;
	unsigned long base_energy;
	bool is_idle;
};

extern struct ctl_table walt_base_table[];
extern struct ctl_table boost_table[];
extern unsigned int sysctl_sched_walt_cross_window_util;
extern unsigned int sysctl_walt_io_is_busy;
extern unsigned int sysctl_walt_busy_threshold;
extern unsigned int sysctl_sched_walt_init_task_load_pct;
extern unsigned int sysctl_sched_walt_cpu_high_irqload;
extern unsigned int sysctl_sched_uclamp_threshold;
extern unsigned int sysctl_walt_account_irq_time;
extern unsigned int sysctl_sched_task_util_prefer_little;
#if IS_ENABLED(CONFIG_UCLAMP_MIN_TO_BOOST)
extern unsigned int sysctl_sched_uclamp_min_to_boost;
#endif
#define scale_demand(d) ((d) / (walt_ravg_window >> SCHED_CAPACITY_SHIFT))

extern unsigned int min_max_possible_capacity;
extern unsigned int max_possible_capacity;
extern unsigned int sched_cap_margin_up[WALT_NR_CPUS];
extern unsigned int sched_cap_margin_dn[WALT_NR_CPUS];

extern struct list_head cluster_head;
extern int num_sched_clusters;
#define for_each_sched_cluster(cluster) \
	list_for_each_entry_rcu(cluster, &cluster_head, list)

static inline int cluster_first_cpu(struct sched_cluster *cluster)
{
	return cpumask_first(&cluster->cpus);
}

static inline bool is_max_capacity_cpu(int cpu)
{
	return arch_scale_cpu_capacity(cpu) == max_possible_capacity;
}

static inline bool is_min_capacity_cpu(int cpu)
{
	return arch_scale_cpu_capacity(cpu) == min_max_possible_capacity;
}

static inline bool is_min_capacity_cluster(struct sched_cluster *cluster)
{
	return is_min_capacity_cpu(cluster_first_cpu(cluster));
}

static inline int same_cluster(int src_cpu, int dst_cpu)
{
	struct uni_rq *src_uni_rq = (struct uni_rq *) cpu_rq(src_cpu)->android_vendor_data1;
	struct uni_rq *dest_uni_rq = (struct uni_rq *) cpu_rq(dst_cpu)->android_vendor_data1;

	return src_uni_rq->cluster == dest_uni_rq->cluster;
}

extern u64 walt_ktime_clock(void);
extern void walt_rt_init(void);
extern void walt_fair_init(void);
extern unsigned long walt_cpu_util_freq(int cpu);
extern int proc_cpuload_init(void);
#ifdef CONFIG_UNISOC_HUNG_TASK_ENH
extern int hung_task_enh_init(void);
#endif

extern void walt_update_task_group(struct cgroup_subsys_state *css);
extern void walt_init_group_control(void);
#ifdef CONFIG_UNISOC_GROUP_BOOST
extern void walt_group_boost_enqueue(struct rq *rq, struct task_struct *p);
extern void walt_group_boost_dequeue(struct rq *rq, struct task_struct *p);
extern int cpu_group_boost(int cpu);
extern unsigned long boosted_cpu_util(int cpu, unsigned long util);
#else
static inline void walt_group_boost_enqueue(struct rq *rq, struct task_struct *p) { };
static inline void walt_group_boost_dequeue(struct rq *rq, struct task_struct *p) { };
static inline int cpu_group_boost(int cpu)
{
	return 0;
}
#endif

static inline struct task_group *css_tg(struct cgroup_subsys_state *css)
{
	return css ? container_of(css, struct task_group, css) : NULL;
}

static inline struct uni_task_group *get_uni_task_group(struct task_struct *p)
{
	return (struct uni_task_group *) (p->sched_task_group->android_vendor_data1);
}

static inline unsigned int tg_init_load_pct(struct task_struct *p)
{
	struct uni_task_group *wtg = get_uni_task_group(p);

	return wtg->init_task_load_pct;
}

static inline unsigned int tg_account_wait_time(struct task_struct *p)
{
	struct uni_task_group *wtg = get_uni_task_group(p);

	return wtg->account_wait_time;
}


static inline int task_group_boost(struct task_struct *p)
{
#ifdef CONFIG_UNISOC_GROUP_BOOST
	struct uni_task_group *wtg = get_uni_task_group(p);

	return wtg->boost;
#else
	return 0;
#endif
}

#define WALT_HIGH_IRQ_TIMEOUT 3
static inline int walt_cpu_high_irqload(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct uni_rq *uni_rq = (struct uni_rq *)rq->android_vendor_data1;
	s64 delta = get_jiffies_64() - uni_rq->irqload_ts;
	u64 irq_load = 0;

	/*
	 * Current context can be preempted by irq and rq->irqload_ts can be
	 * updated by irq context so that delta can be negative.
	 * But this is okay and we can safely return as this means there
	 * was recent irq occurrence.
	 */

	if (delta < WALT_HIGH_IRQ_TIMEOUT)
		irq_load = uni_rq->avg_irqload;

	return irq_load >= sysctl_sched_walt_cpu_high_irqload;
}

static inline unsigned long walt_task_util(struct task_struct *p)
{
	struct uni_task_struct *uni_tsk = (struct uni_task_struct *) p->android_vendor_data1;

	return uni_tsk->demand_scale;
}

static inline unsigned long walt_cpu_util(int cpu)
{
	struct uni_rq *uni_rq = (struct uni_rq *) cpu_rq(cpu)->android_vendor_data1;
	u64 cpu_util = uni_rq->cumulative_runnable_avg;

	cpu_util <<= SCHED_CAPACITY_SHIFT;
	do_div(cpu_util, walt_ravg_window);

	return min_t(unsigned long, cpu_util, capacity_orig_of(cpu));
}

#ifdef CONFIG_UNISOC_GROUP_BOOST
extern unsigned int sysctl_sched_spc_threshold;
static inline int group_boost_margin(long util, int boost)
{
	int margin;

	/*
	 * Signal proportional compensation (SPC)
	 *
	 * The Boost (B) value is used to compute a Margin (M) which is
	 * proportional to the complement of the original Signal (S):
	 *   M = B * (SCHED_CAPACITY_SCALE - S)
	 * The obtained M could be used by the caller to "boost" S.
	 */
	if (boost >= 0) {
		if (util < sysctl_sched_spc_threshold)
			margin = util * boost / 100;
		else
			margin  = (SCHED_CAPACITY_SCALE - util) * boost / 100;
	} else
		margin = util * boost / 100;

	return margin;
}

static inline unsigned long boosted_task_util(struct task_struct *task)
{
	unsigned long util = walt_task_util(task);
	int boost = task_group_boost(task);
	int margin = group_boost_margin(util, boost);

	trace_sched_boost_task(task, util, boost, margin);

	return util + margin;
}
#else
static inline unsigned long boosted_cpu_util(int cpu, unsigned long util)
{
	return util;
}
static inline unsigned long boosted_task_util(struct task_struct *task)
{
	return walt_task_util(task);
}
#endif

#ifdef CONFIG_UCLAMP_TASK
#if IS_ENABLED(CONFIG_UCLAMP_MIN_TO_BOOST)
static inline unsigned long uclamp_transform_boost(unsigned long util,
						unsigned long uclamp_min,
						unsigned long uclamp_max)
{
	unsigned long margin, boost;

	if (unlikely(uclamp_min > uclamp_max))
		return util;

	if (util >= uclamp_max)
		return uclamp_max;

	boost = util < sysctl_sched_uclamp_threshold ? util : (uclamp_max - util);

	margin = DIV_ROUND_CLOSEST_ULL(uclamp_min * boost, SCHED_CAPACITY_SCALE);

	return util + margin;
}
#endif

static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	unsigned long min_util = uclamp_eff_value(p, UCLAMP_MIN);
	unsigned long max_util = uclamp_eff_value(p, UCLAMP_MAX);
	unsigned long util;

	util = boosted_task_util(p);

#ifdef CONFIG_UCLAMP_MIN_TO_BOOST
	if (sysctl_sched_uclamp_min_to_boost)
		util = uclamp_transform_boost(util, min_util, max_util);
	else if (util >= sysctl_sched_uclamp_threshold)
		util = clamp(util, min_util, max_util);
#else
	if (util >= sysctl_sched_uclamp_threshold)
		util = clamp(util, min_util, max_util);

#endif

	return util;
}

static __always_inline
unsigned long walt_uclamp_rq_util_with(struct rq *rq, unsigned long util,
				  struct task_struct *p)
{
	unsigned long min_util = 0;
	unsigned long max_util = 0;

	if (!static_branch_likely(&sched_uclamp_used))
		return util;

	if (p) {
		min_util = uclamp_eff_value(p, UCLAMP_MIN);
		max_util = uclamp_eff_value(p, UCLAMP_MAX);

		/*
		 * Ignore last runnable task's max clamp, as this task will
		 * reset it. Similarly, no need to read the rq's min clamp.
		 */
		if (rq->uclamp_flags & UCLAMP_FLAG_IDLE)
			goto out;
	}

	min_util = max_t(unsigned long, min_util, READ_ONCE(rq->uclamp[UCLAMP_MIN].value));
	max_util = max_t(unsigned long, max_util, READ_ONCE(rq->uclamp[UCLAMP_MAX].value));
out:
	/*
	 * Since CPU's {min,max}_util clamps are MAX aggregated considering
	 * RUNNABLE tasks with _different_ clamps, we can end up with an
	 * inversion. Fix it now when the clamps are applied.
	 */
	if (unlikely(min_util >= max_util))
		return min_util;

#if IS_ENABLED(CONFIG_UCLAMP_MIN_TO_BOOST)
	if (sysctl_sched_uclamp_min_to_boost)
		util = uclamp_transform_boost(util, min_util, max_util);
	else if (util >= sysctl_sched_uclamp_threshold)
		util = clamp(util, min_util, max_util);
#else
	if (util >= sysctl_sched_uclamp_threshold)
		util = clamp(util, min_util, max_util);
#endif

	return util;
}

static inline bool uclamp_blocked(struct task_struct *p)
{
	return uclamp_eff_value(p, UCLAMP_MAX) < SCHED_CAPACITY_SCALE;
}

#else
static inline unsigned long uclamp_task_util(struct task_struct *p)
{
	return boost_task_util(p);
}

static inline
unsigned long walt_uclamp_rq_util_with(struct rq *rq, unsigned long util,
				  struct task_struct *p)
{
	return util;
}

static inline bool uclamp_blocked(struct task_struct *p)
{
	return false;
}
#endif

#ifdef CONFIG_CPU_FREQ
struct walt_update_util_data {
	void (*func)(struct walt_update_util_data *data, u64 time, unsigned int flags);
};

void walt_cpufreq_add_update_util_hook(int cpu, struct walt_update_util_data *data,
			void (*func)(struct walt_update_util_data *data, u64 time,
				unsigned int flags));
void walt_cpufreq_remove_update_util_hook(int cpu);
bool walt_cpufreq_this_cpu_can_update(struct cpufreq_policy *policy);

DECLARE_PER_CPU(struct walt_update_util_data __rcu *, walt_cpufreq_update_util_data);
static inline void walt_cpufreq_update_util(struct rq *rq, unsigned int flags)
{
	struct walt_update_util_data *data;

	data = rcu_dereference_sched(*per_cpu_ptr(&walt_cpufreq_update_util_data,
						  cpu_of(rq)));
	if (data)
		data->func(data, rq_clock(rq), flags);
}
#else
static inline void walt_cpufreq_update_util(struct rq *rq, unsigned int flags) {}
#endif

#if IS_ENABLED(CONFIG_UNISOC_ROTATION_TASK)
#define CPU_RESERVED	1
extern unsigned int sysctl_rotation_enable;
extern unsigned int sysctl_rotation_threshold_ms;

static inline bool is_reserved(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct uni_rq *uni_rq = (struct uni_rq *) rq->android_vendor_data1;

	return test_bit(CPU_RESERVED, &uni_rq->sched_flag);
}

static inline void mark_reserved(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct uni_rq *uni_rq = (struct uni_rq *) rq->android_vendor_data1;

	test_and_set_bit(CPU_RESERVED, &uni_rq->sched_flag);
}

static inline void clear_reserved(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct uni_rq *uni_rq = (struct uni_rq *) rq->android_vendor_data1;

	return clear_bit(CPU_RESERVED, &uni_rq->sched_flag);
}
#else
static inline bool is_reserved(int cpu)
{
	return false;
}
static inline void mark_reserved(int cpu) { }
static inline void clear_reserved(int cpu) { }
static inline void rotation_task_init(void) { }
static inline void check_for_task_rotation(struct rq *src_rq) { }
#endif
#endif /* _WALT_H */
