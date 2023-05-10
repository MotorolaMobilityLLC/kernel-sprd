// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020 Spreadtrum Communications Inc.

#define pr_fmt(fmt)	"core_ctl: " fmt

#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/syscore_ops.h>
#include <uapi/linux/sched/types.h>

#include "uni_sched.h"
#include "trace.h"

#define MAX_CPUS_PER_CLUSTER 6
#define MAX_CLUSTERS 3

/* mask of CPUs on which there is an outstanding pause claim */
static cpumask_t cpus_paused_by_us = { CPU_BITS_NONE };

struct cluster_data {
	bool			inited;
	unsigned int		min_cpus;
	unsigned int		max_cpus;
	unsigned int		active_cpus;
	unsigned int		num_cpus;
	unsigned int		nr_not_preferred_cpus;
	cpumask_t		cpu_mask;
	unsigned int		need_cpus;
	unsigned int		max_nr;
	struct list_head	lru;
	unsigned int		first_cpu;
	struct kobject		kobj;
	unsigned int		pause_cpu;
	unsigned int		resume_cpu;
};

struct cpu_data {
	bool			is_busy;
	unsigned int		busy;
	unsigned int		cpu;
	bool			not_preferred;
	struct cluster_data	*cluster;
	struct list_head	sib;
	bool			disabled;
};

static DEFINE_PER_CPU(struct cpu_data, cpu_state);
static struct cluster_data cluster_state[MAX_CLUSTERS];
static unsigned int num_clusters;

#define for_each_cluster(cluster, idx) \
	for (; (idx) < num_clusters && ((cluster) = &cluster_state[idx]);\
		idx++)

/* single core_ctl thread for all pause/unpause core_ctl operations */
struct task_struct *core_ctl_thread;

/* single lock per single thread for core_ctl
 * protects core_ctl_pending flag
 */
spinlock_t core_ctl_pending_lock;
static bool core_ctl_pending;

static DEFINE_SPINLOCK(state_lock);
static void apply_need(struct cluster_data *state);
static void wake_up_core_ctl_thread(void);
static bool initialized;

ATOMIC_NOTIFIER_HEAD(core_ctl_notifier);

static unsigned int get_active_cpu_count(const struct cluster_data *cluster);

/* ========================= sysfs interface =========================== */

static ssize_t store_pause_cpu(struct cluster_data *state,
					const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	if (val >= state->first_cpu &&
	    val <= state->first_cpu + state->num_cpus - 1 &&
	    !cpu_halted(val)) {
		state->pause_cpu = val;
		state->min_cpus--;
		apply_need(state);

		return count;
	}

	return -EINVAL;
}

static ssize_t show_pause_cpu(const struct cluster_data *state,
				     char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->pause_cpu);
}

static ssize_t store_resume_cpu(struct cluster_data *state,
					const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	if (val >= state->first_cpu &&
	    val <= state->first_cpu + state->num_cpus - 1 &&
	    cpu_halted(val)) {
		state->resume_cpu = val;
		state->min_cpus++;
		apply_need(state);

		return count;
	}

	return -EINVAL;

}

static ssize_t show_resume_cpu(const struct cluster_data *state,
				     char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->resume_cpu);
}

static ssize_t show_active_cpus(const struct cluster_data *state, char *buf)
{
	int active_cpus = get_active_cpu_count(state);

	return scnprintf(buf, PAGE_SIZE, "%u\n", active_cpus);
}

static unsigned int cluster_paused_cpus(const struct cluster_data *cluster)
{
	cpumask_t cluster_paused_cpus;

	cpumask_and(&cluster_paused_cpus, &cluster->cpu_mask, &cpus_paused_by_us);
	return cpumask_weight(&cluster_paused_cpus);
}

static ssize_t show_global_state(const struct cluster_data *state, char *buf)
{
	struct cpu_data *c;
	struct cluster_data *cluster;
	ssize_t count = 0;
	unsigned int cpu;

	spin_lock_irq(&state_lock);
	for_each_possible_cpu(cpu) {
		c = &per_cpu(cpu_state, cpu);
		cluster = c->cluster;
		if (!cluster || !cluster->inited)
			continue;

		count += scnprintf(buf + count, PAGE_SIZE - count,
					"CPU%u\n", cpu);
		count += scnprintf(buf + count, PAGE_SIZE - count,
					"\tCPU: %u\n", c->cpu);
		count += scnprintf(buf + count, PAGE_SIZE - count,
					"\tOnline: %u\n",
					cpu_online(c->cpu));
		count += scnprintf(buf + count, PAGE_SIZE - count,
					"\tPaused: %u\n",
					cpu_halted(c->cpu));
		count += scnprintf(buf + count, PAGE_SIZE - count,
					"\tFirst CPU: %u\n",
						cluster->first_cpu);
		count += scnprintf(buf + count, PAGE_SIZE - count,
			"\tActive CPUs: %u\n", get_active_cpu_count(cluster));
		count += scnprintf(buf + count, PAGE_SIZE - count,
				"\tCluster paused CPUs: %u\n",
				   cluster_paused_cpus(cluster));
	}
	spin_unlock_irq(&state_lock);

	return count;
}

struct core_ctl_attr {
	struct attribute	attr;
	ssize_t			(*show)(const struct cluster_data *cd, char *c);
	ssize_t			(*store)(struct cluster_data *cd, const char *c,
							size_t count);
};

#define core_ctl_attr_ro(_name)		\
static struct core_ctl_attr _name =	\
__ATTR(_name, 0444, show_##_name, NULL)

#define core_ctl_attr_rw(_name)			\
static struct core_ctl_attr _name =		\
__ATTR(_name, 0644, show_##_name, store_##_name)

core_ctl_attr_ro(active_cpus);
core_ctl_attr_ro(global_state);
core_ctl_attr_rw(pause_cpu);
core_ctl_attr_rw(resume_cpu);

static struct attribute *default_attrs[] = {
	&active_cpus.attr,
	&global_state.attr,
	&pause_cpu.attr,
	&resume_cpu.attr,
	NULL
};

#define to_cluster_data(k) container_of(k, struct cluster_data, kobj)
#define to_attr(a) container_of(a, struct core_ctl_attr, attr)
static ssize_t show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct cluster_data *data = to_cluster_data(kobj);
	struct core_ctl_attr *cattr = to_attr(attr);
	ssize_t ret = -EIO;

	if (cattr->show)
		ret = cattr->show(data, buf);

	return ret;
}

static ssize_t store(struct kobject *kobj, struct attribute *attr,
		     const char *buf, size_t count)
{
	struct cluster_data *data = to_cluster_data(kobj);
	struct core_ctl_attr *cattr = to_attr(attr);
	ssize_t ret = -EIO;

	if (cattr->store)
		ret = cattr->store(data, buf, count);

	return ret;
}

static const struct sysfs_ops sysfs_ops = {
	.show	= show,
	.store	= store,
};

static struct kobj_type ktype_core_ctl = {
	.sysfs_ops	= &sysfs_ops,
	.default_attrs	= default_attrs,
};


/* ======================= load based core count  ====================== */

static unsigned int apply_limits(const struct cluster_data *cluster,
				 unsigned int need_cpus)
{
	return min(max(cluster->min_cpus, need_cpus), cluster->max_cpus);
}

static unsigned int get_active_cpu_count(const struct cluster_data *cluster)
{
	cpumask_t cpus;

	cpumask_andnot(&cpus, &cluster->cpu_mask, cpu_halt_mask);
	return cpumask_weight(&cpus);
}

static bool is_active(const struct cpu_data *state)
{
	return cpu_active(state->cpu) && !cpu_halted(state->cpu);
}

static bool adjustment_possible(const struct cluster_data *cluster,
							unsigned int need)
{
	return (need < cluster->active_cpus || (need > cluster->active_cpus &&
						cluster_paused_cpus(cluster)));
}

static bool eval_need(struct cluster_data *cluster)
{
	unsigned long flags;
	unsigned int need_cpus = 0, last_need;
	bool adj_now = false;
	bool adj_possible = false;
	unsigned int new_need;

	if (unlikely(!cluster->inited))
		return false;

	spin_lock_irqsave(&state_lock, flags);

	cluster->active_cpus = get_active_cpu_count(cluster);
	need_cpus = cluster->min_cpus;
	new_need = apply_limits(cluster, need_cpus);
	last_need = cluster->need_cpus;

	if (new_need > cluster->active_cpus) {
		adj_now = true;
	} else {
		/*
		 * When there is no change in need and there are no more
		 * active CPUs than currently needed, just update the
		 * need time stamp and return.
		 */
		if (new_need == last_need && new_need == cluster->active_cpus) {
			adj_now = false;
			goto unlock;
		}
		adj_now = true;
	}

	if (adj_now) {
		adj_possible = adjustment_possible(cluster, new_need);
		cluster->need_cpus = new_need;
	}

unlock:
	spin_unlock_irqrestore(&state_lock, flags);

	return adj_now && adj_possible;
}

static void apply_need(struct cluster_data *cluster)
{
	if (eval_need(cluster))
		wake_up_core_ctl_thread();
}

/* ========================= core count enforcement ==================== */

static void wake_up_core_ctl_thread(void)
{
	unsigned long flags;

	spin_lock_irqsave(&core_ctl_pending_lock, flags);
	core_ctl_pending = true;
	spin_unlock_irqrestore(&core_ctl_pending_lock, flags);

	wake_up_process(core_ctl_thread);
}

/* must be called with state_lock held */
static void move_cpu_lru(struct cpu_data *cpu_data)
{
	list_del(&cpu_data->sib);
	list_add_tail(&cpu_data->sib, &cpu_data->cluster->lru);
}

static void try_to_pause(struct cluster_data *cluster, unsigned int need,
			 struct cpumask *pause_cpus)
{
	struct cpu_data *c, *tmp;
	unsigned long flags;
	unsigned int num_cpus = cluster->num_cpus;
	unsigned int nr_pending = 0, active_cpus = cluster->active_cpus;

	/*
	 * Protect against entry being removed (and added at tail) by other
	 * thread (hotplug).
	 */
	spin_lock_irqsave(&state_lock, flags);
	list_for_each_entry_safe(c, tmp, &cluster->lru, sib) {
		if (!num_cpus--)
			break;

		if (c->disabled || !is_active(c))
			continue;

		if (active_cpus - nr_pending == need)
			break;

		/* Don't pause busy CPUs. */
		if (c->is_busy)
			continue;

		/*
		 * We pause only the not_preferred CPUs. If none
		 * of the CPUs are selected as not_preferred, then
		 * all CPUs are eligible for pausing.
		 */
		if (cluster->nr_not_preferred_cpus && !c->not_preferred)
			continue;

		if (cluster->pause_cpu != c->cpu)
			continue;

		pr_debug("Trying to pause CPU%u\n", c->cpu);
		cpumask_set_cpu(c->cpu, pause_cpus);
		nr_pending++;
		move_cpu_lru(c);
	}

	spin_unlock_irqrestore(&state_lock, flags);
}

static int __try_to_resume(struct cluster_data *cluster, unsigned int need,
			   bool force, struct cpumask *unpause_cpus)
{
	struct cpu_data *c, *tmp;
	unsigned long flags;
	unsigned int num_cpus = cluster->num_cpus;
	unsigned int nr_pending = 0, active_cpus = cluster->active_cpus;

	/*
	 * Protect against entry being removed (and added at tail) by other
	 * thread (hotplug).
	 */
	spin_lock_irqsave(&state_lock, flags);
	list_for_each_entry_safe(c, tmp, &cluster->lru, sib) {
		if (!num_cpus--)
			break;

		if (!cpumask_test_cpu(c->cpu, &cpus_paused_by_us))
			continue;
		if ((cpu_active(c->cpu) && !cpu_halted(c->cpu)) ||
			(!force && c->not_preferred))
			continue;
		if (active_cpus + nr_pending == need)
			break;
		if (cluster->resume_cpu != c->cpu)
			continue;

		pr_debug("Trying to resume CPU%u\n", c->cpu);

		cpumask_set_cpu(c->cpu, unpause_cpus);
		nr_pending++;
		move_cpu_lru(c);
	}

	spin_unlock_irqrestore(&state_lock, flags);

	return nr_pending;
}

static void try_to_resume(struct cluster_data *cluster, unsigned int need,
			  struct cpumask *unpause_cpus)
{
	bool force_use_non_preferred = false;
	unsigned int nr_pending;

	/*
	 * __try_to_resume() marks the CPUs to be resumed but active_cpus
	 * won't be reflected yet. So use the nr_pending to adjust active
	 * count.
	 */
	nr_pending = __try_to_resume(cluster, need, force_use_non_preferred, unpause_cpus);

	if (cluster->active_cpus + nr_pending == need)
		return;

	force_use_non_preferred = true;
	__try_to_resume(cluster, need, force_use_non_preferred, unpause_cpus);
}

/*
 * core_ctl_pause_cpus: pause a set of CPUs as requested by core_ctl, handling errors.
 *
 * In order to handle errors properly, and properly track success, the cpus being
 * passed to walt_pause_cpus needs to be saved off. It needs to be saved because
 * walt_pause_cpus will modify the value (through pause_cpus()). Pause_cpus modifies
 * the value because it updates the variable to eliminate CPUs that are already paused.
 * THIS code, however, must be very careful to track what cpus were requested, rather
 * than what cpus actually were paused in this action. Otherwise, the ref-counts in
 * walt_pause.c will get out of sync with this code.
 */
static void core_ctl_pause_cpus(struct cpumask *cpus_to_pause)
{
	cpumask_t saved_cpus;

	/* be careful to only pause CPUs not paused before to ensure ref-count sync */
	cpumask_andnot(cpus_to_pause, cpus_to_pause, &cpus_paused_by_us);
	cpumask_copy(&saved_cpus, cpus_to_pause);

	if (cpumask_any(cpus_to_pause) < nr_cpu_ids) {
		if (pause_cpus(cpus_to_pause, PAUSE_CORE_CTL) < 0)
			pr_debug("core_ctl pause operation failed cpus=%*pbl paused_by_us=%*pbl\n",
				 cpumask_pr_args(cpus_to_pause),
				 cpumask_pr_args(&cpus_paused_by_us));
		else
			cpumask_or(&cpus_paused_by_us, &cpus_paused_by_us, &saved_cpus);
	}
}

/*
 * core_ctl_resume_cpus: resume a set of CPUs as requested by core_ctl, handling errors.
 *
 * In order to handle errors properly, and properly track success, the cpus being
 * passed to walt_resume_cpus needs to be saved off. It needs to be saved because
 * walt_resume_cpus will modify the value (through resume_cpus()). Resume_cpus modifies
 * the value because it updates the variable to eliminate CPUs that are already resumed.
 * THIS code, however, must be very careful to track what cpus were requested, rather
 * than what cpus actually were resumed in this action. Otherwise, the ref-counts in
 * walt_pause.c will get out of sync with this code.
 */
static void core_ctl_resume_cpus(struct cpumask *cpus_to_unpause)
{
	cpumask_t saved_cpus;

	/* be careful to only unpause CPUs paused before to ensure ref-count sync */
	cpumask_and(cpus_to_unpause, cpus_to_unpause, &cpus_paused_by_us);
	cpumask_copy(&saved_cpus, cpus_to_unpause);

	if (cpumask_any(cpus_to_unpause) < nr_cpu_ids) {
		if (resume_cpus(cpus_to_unpause, PAUSE_CORE_CTL) < 0)
			pr_debug("core_ctl resume operation failed cpus=%*pbl paused_by_us=%*pbl\n",
				 cpumask_pr_args(cpus_to_unpause),
				 cpumask_pr_args(&cpus_paused_by_us));
		else
			cpumask_andnot(&cpus_paused_by_us, &cpus_paused_by_us, &saved_cpus);
	}
}

static void __ref do_core_ctl(void)
{
	struct cluster_data *cluster;
	unsigned int index = 0;
	unsigned int need;
	cpumask_t cpus_to_pause = { CPU_BITS_NONE };
	cpumask_t cpus_to_unpause = { CPU_BITS_NONE };

	for_each_cluster(cluster, index) {

		cluster->active_cpus = get_active_cpu_count(cluster);
		need = apply_limits(cluster, cluster->need_cpus);

		if (adjustment_possible(cluster, need)) {
			pr_debug("Trying to adjust group %u from %u to %u\n",
				 cluster->first_cpu, cluster->active_cpus, need);

			if (cluster->active_cpus > need)
				try_to_pause(cluster, need, &cpus_to_pause);

			else if (cluster->active_cpus < need)
				try_to_resume(cluster, need, &cpus_to_unpause);
		}
	}

	core_ctl_pause_cpus(&cpus_to_pause);
	core_ctl_resume_cpus(&cpus_to_unpause);
}

static int __ref try_core_ctl(void *data)
{
	unsigned long flags;

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		spin_lock_irqsave(&core_ctl_pending_lock, flags);
		if (!core_ctl_pending) {
			spin_unlock_irqrestore(&core_ctl_pending_lock, flags);
			schedule();
			if (kthread_should_stop())
				break;
			spin_lock_irqsave(&core_ctl_pending_lock, flags);
		}
		set_current_state(TASK_RUNNING);
		core_ctl_pending = false;
		spin_unlock_irqrestore(&core_ctl_pending_lock, flags);

		do_core_ctl();
	}

	return 0;
}

/* ============================ init code ============================== */

static struct cluster_data *find_cluster_by_first_cpu(unsigned int first_cpu)
{
	unsigned int i;

	for (i = 0; i < num_clusters; ++i) {
		if (cluster_state[i].first_cpu == first_cpu)
			return &cluster_state[i];
	}

	return NULL;
}

static int cluster_init(const struct cpumask *mask)
{
	struct device *dev;
	unsigned int first_cpu = cpumask_first(mask);
	struct cluster_data *cluster;
	struct cpu_data *state;
	unsigned int cpu;

	if (find_cluster_by_first_cpu(first_cpu))
		return 0;

	dev = get_cpu_device(first_cpu);
	if (!dev)
		return -ENODEV;

	pr_info("Creating CPU group %d\n", first_cpu);

	if (num_clusters == MAX_CLUSTERS) {
		pr_err("Unsupported number of clusters. Only %u supported\n",
								MAX_CLUSTERS);
		return -EINVAL;
	}
	cluster = &cluster_state[num_clusters];
	++num_clusters;

	cpumask_copy(&cluster->cpu_mask, mask);
	cluster->num_cpus = cpumask_weight(mask);
	if (cluster->num_cpus > MAX_CPUS_PER_CLUSTER) {
		pr_err("HW configuration not supported\n");
		return -EINVAL;
	}
	cluster->first_cpu = first_cpu;
	cluster->min_cpus = cluster->num_cpus;
	cluster->max_cpus = cluster->num_cpus;
	cluster->need_cpus = cluster->num_cpus;
	cluster->nr_not_preferred_cpus = 0;
	cluster->pause_cpu = 0;
	cluster->resume_cpu = 0;
	INIT_LIST_HEAD(&cluster->lru);

	for_each_cpu(cpu, mask) {
		pr_info("Init CPU%u state\n", cpu);

		state = &per_cpu(cpu_state, cpu);
		state->cluster = cluster;
		state->cpu = cpu;
		state->disabled = get_cpu_device(cpu) &&
				get_cpu_device(cpu)->offline_disabled;
		list_add_tail(&state->sib, &cluster->lru);
	}
	cluster->active_cpus = get_active_cpu_count(cluster);

	cluster->inited = true;

	kobject_init(&cluster->kobj, &ktype_core_ctl);
	return kobject_add(&cluster->kobj, &dev->kobj, "core_ctl");
}

int core_ctl_init(void)
{
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };
	struct sched_cluster *cluster;
	int ret;

	spin_lock_init(&core_ctl_pending_lock);

	/* initialize our single kthread, after spin lock init */
	core_ctl_thread = kthread_run(try_core_ctl, NULL, "core_ctl");

	if (IS_ERR(core_ctl_thread))
		return PTR_ERR(core_ctl_thread);

	sched_setscheduler_nocheck(core_ctl_thread, SCHED_FIFO, &param);

	for_each_sched_cluster(cluster) {
		ret = cluster_init(&cluster->cpus);
		if (ret)
			pr_warn("unable to create core ctl group: %d\n", ret);
	}

	initialized = true;

	return 0;
}
