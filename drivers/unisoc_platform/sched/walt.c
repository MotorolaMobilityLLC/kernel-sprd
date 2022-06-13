// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2022, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Unisoc, Inc.
 */

#define pr_fmt(fmt)	"unisoc-sched: " fmt

#include <linux/kmemleak.h>
#include <linux/syscore_ops.h>

#include <trace/hooks/sched.h>
#include <trace/hooks/topology.h>

#include "walt.h"

#define CREATE_TRACE_POINTS
#include "trace.h"

#define WINDOW_STATS_RECENT		0
#define WINDOW_STATS_MAX		1
#define WINDOW_STATS_MAX_RECENT_AVG	2
#define WINDOW_STATS_AVG		3
#define WINDOW_STATS_INVALID_POLICY	4

#define WALT_FREQ_ACCOUNT_WAIT_TIME	0

static __read_mostly unsigned int walt_ravg_hist_size = 6;
static __read_mostly unsigned int walt_window_stats_policy = WINDOW_STATS_MAX;
__read_mostly unsigned int sysctl_walt_account_wait_time;
__read_mostly unsigned int sysctl_walt_io_is_busy;
__read_mostly unsigned int sysctl_sched_walt_cpu_high_irqload =
							(10 * NSEC_PER_MSEC);

unsigned int sysctl_sched_walt_init_task_load_pct = 10;

/*
 * Window size (in ns). Adjust for the tick size so that the window
 * rollover occurs just before the tick boundary.
 */
__read_mostly unsigned int walt_ravg_window =
					    (16000000 / TICK_NSEC) * TICK_NSEC;

__read_mostly unsigned int sysctl_walt_busy_threshold = 50;
__read_mostly unsigned int sysctl_sched_walt_cross_window_util = 1;

static unsigned int sync_cpu;
static ktime_t ktime_last;
static __read_mostly bool walt_ktime_suspended;

unsigned int min_max_possible_capacity = 1024;
unsigned int max_possible_capacity = 1024; /* max(rq->max_possible_capacity) */

unsigned long walt_cpu_util_freq(int cpu)
{
	u64 walt_cpu_util;
	struct rq *rq = cpu_rq(cpu);
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	unsigned long util_freq;

	walt_cpu_util = wrq->cumulative_runnable_avg;
	walt_cpu_util <<= SCHED_CAPACITY_SHIFT;
	do_div(walt_cpu_util, walt_ravg_window);

	if (wrq->is_busy == CPU_BUSY_SET || sysctl_walt_io_is_busy != 0) {
		u64 prev_runnable_sum = wrq->prev_runnable_sum;

		prev_runnable_sum <<= SCHED_CAPACITY_SHIFT;
		do_div(prev_runnable_sum, walt_ravg_window);
		walt_cpu_util = max(walt_cpu_util, prev_runnable_sum);
	}

	util_freq = min_t(unsigned long, walt_cpu_util, capacity_orig_of(cpu));

	return walt_uclamp_rq_util_with(rq, util_freq, NULL);
}
EXPORT_SYMBOL_GPL(walt_cpu_util_freq);

static inline unsigned int walt_task_load(struct walt_task_ravg *wtr)
{
	return wtr->demand;
}

static inline void fixup_cum_window_demand(struct rq *rq, s64 delta)
{
	struct walt_rq *wrq = (struct walt_rq *)rq->android_vendor_data1;

	wrq->cum_window_demand += delta;
	if (unlikely((s64)wrq->cum_window_demand < 0))
		wrq->cum_window_demand = 0;
}

static void
walt_inc_cumulative_runnable_avg(struct rq *rq, struct task_struct *p)
{
	struct walt_rq *wrq = (struct walt_rq *)rq->android_vendor_data1;
	struct walt_task_ravg *wtr = (struct walt_task_ravg *)p->android_vendor_data1;

	wrq->cumulative_runnable_avg += wtr->demand;

	/*
	 * Add a task's contribution to the cumulative window demand when
	 *
	 * (1) task is enqueued with on_rq = 1 i.e migration,
	 *     prio/cgroup/class change.
	 * (2) task is waking for the first time in this window.
	 */
	if (p->on_rq || (wtr->last_sleep_ts < wrq->window_start))
		fixup_cum_window_demand(rq, wtr->demand);
}

static void
walt_dec_cumulative_runnable_avg(struct rq *rq, struct task_struct *p)
{
	struct walt_rq *wrq = (struct walt_rq *)rq->android_vendor_data1;
	struct walt_task_ravg *wtr = (struct walt_task_ravg *)p->android_vendor_data1;

	wrq->cumulative_runnable_avg -= wtr->demand;
	BUG_ON((s64)wrq->cumulative_runnable_avg < 0);

	/*
	 * on_rq will be 1 for sleeping tasks. So check if the task
	 * is migrating or dequeuing in RUNNING state to change the
	 * prio/cgroup/class.
	 */
	if (task_on_rq_migrating(p) || task_is_running(p))
		fixup_cum_window_demand(rq, -(s64)wtr->demand);
}

static void
walt_fixup_cumulative_runnable_avg(struct rq *rq, struct walt_task_ravg *wtr,
				   u64 new_task_load)
{
	struct walt_rq *wrq = (struct walt_rq *)rq->android_vendor_data1;
	s64 task_load_delta = (s64)new_task_load - walt_task_load(wtr);

	wrq->cumulative_runnable_avg += task_load_delta;
	if (unlikely((s64)wrq->cumulative_runnable_avg < 0))
		panic("cra less than zero: tld: %lld, task_load(p) = %u\n",
			task_load_delta, walt_task_load(wtr));

	fixup_cum_window_demand(rq, task_load_delta);
}

u64 walt_ktime_clock(void)
{
	if (unlikely(walt_ktime_suspended))
		return ktime_to_ns(ktime_last);
	return ktime_get_ns();
}

static void walt_resume(void)
{
	walt_ktime_suspended = false;
}

static int walt_suspend(void)
{
	ktime_last = ktime_get();
	walt_ktime_suspended = true;
	return 0;
}

static struct syscore_ops walt_syscore_ops = {
	.resume	= walt_resume,
	.suspend = walt_suspend
};

static inline bool exiting_task(struct task_struct *p)
{
	return !!(p->flags & PF_EXITING);
}

static u64 update_window_start(struct rq *rq, u64 wallclock)
{
	s64 delta;
	int nr_windows;
	struct walt_rq *wrq = (struct walt_rq *)rq->android_vendor_data1;
	u64 old_window_start = wrq->window_start;

	delta = wallclock - wrq->window_start;
	/* If the MPM global timer is cleared, set delta as 0 to avoid kernel BUG happening */
	if (delta < 0) {
		delta = 0;
		WARN_ONCE(1, "WALT wallclock appears to have gone backwards or reset\n");
	}

	if (delta < walt_ravg_window)
		return old_window_start;

	nr_windows = div64_u64(delta, walt_ravg_window);
	wrq->window_start += (u64)nr_windows * (u64)walt_ravg_window;

	wrq->cum_window_demand = wrq->cumulative_runnable_avg;

	return old_window_start;
}
/*
 * Translate absolute delta time accounted on a CPU
 * to a scale where 1024 is the capacity of the most
 * capable CPU running at FMAX
 */
static u64 scale_exec_time(u64 delta, struct rq *rq)
{
	u64 cap_curr = cap_scale(arch_scale_cpu_capacity(cpu_of(rq)),
				arch_scale_freq_capacity(cpu_of(rq)));

	delta = cap_scale(delta, cap_curr);

	return delta;
}

static inline int cpu_is_waiting_on_io(struct rq *rq)
{
	if (!sysctl_walt_io_is_busy)
		return 0;

	return atomic_read(&rq->nr_iowait);
}


static int account_busy_for_cpu_time(struct rq *rq, struct task_struct *p,
				     u64 irqtime, int event)
{
	if (is_idle_task(p)) {
		/* TASK_WAKE && TASK_MIGRATE is not possible on idle task! */
		if (event == PICK_NEXT_TASK)
			return 0;

		/* PUT_PREV_TASK, TASK_UPDATE && IRQ_UPDATE are left */
		return irqtime || cpu_is_waiting_on_io(rq);
	}

	if (event == TASK_WAKE)
		return 0;

	if (event == PUT_PREV_TASK || event == IRQ_UPDATE ||
				      event == TASK_UPDATE)
		return 1;

	/* Only TASK_MIGRATE && PICK_NEXT_TASK left */
	return WALT_FREQ_ACCOUNT_WAIT_TIME;
}

/*
 * Account cpu activity in its busy time counters (rq->curr/prev_runnable_sum)
 */
static void update_cpu_busy_time(struct task_struct *p, struct rq *rq,
	     int event, u64 wallclock, u64 irqtime)
{
	struct walt_task_ravg *wtr = (struct walt_task_ravg *)p->android_vendor_data1;
	struct walt_rq *wrq = (struct walt_rq *)rq->android_vendor_data1;
	int new_window, nr_full_windows = 0;
	int p_is_curr_task = (p == rq->curr);
	u64 mark_start = wtr->mark_start;
	u64 window_start = wrq->window_start;
	u32 window_size = walt_ravg_window;
	u64 delta;

	new_window = mark_start < window_start;
	if (new_window)
		nr_full_windows = div64_u64((window_start - mark_start),
						window_size);

	/*
	 * Handle per-task window rollover. We don't care about the idle
	 * task or exiting tasks.
	 */
	if (new_window && !is_idle_task(p) && !exiting_task(p)) {
		u32 curr_window = 0;

		if (!nr_full_windows)
			curr_window = wtr->curr_window;

		wtr->prev_window = curr_window;
		wtr->curr_window = 0;
	}

	if (!account_busy_for_cpu_time(rq, p, irqtime, event)) {
		/*
		 * account_busy_for_cpu_time() = 0, so no update to the
		 * task's current window needs to be made. This could be
		 * for example
		 *
		 *   - a wakeup event on a task within the current
		 *     window (!new_window below, no action required),
		 *   - switching to a new task from idle (PICK_NEXT_TASK)
		 *     in a new window where irqtime is 0 and we aren't
		 *     waiting on IO
		 */
		if (!new_window)
			return;

		/*
		 * A new window has started. The RQ demand must be rolled
		 * over if p is the current task.
		 */
		if (p_is_curr_task) {
			u64 prev_sum = 0;

			/* p is either idle task or an exiting task */
			if (!nr_full_windows)
				prev_sum = wrq->curr_runnable_sum;

			wrq->prev_runnable_sum = prev_sum;
			wrq->curr_runnable_sum = 0;
		}

		return;
	}

	if (!new_window) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. No rollover
		 * since we didn't start a new window. An example of this is
		 * when a task starts execution and then sleeps within the
		 * same window.
		 */

		if (!irqtime || !is_idle_task(p) || cpu_is_waiting_on_io(rq))
			delta = wallclock - mark_start;
		else
			delta = irqtime;
		delta = scale_exec_time(delta, rq);
		wrq->curr_runnable_sum += delta;
		if (!is_idle_task(p) && !exiting_task(p))
			wtr->curr_window += delta;

		return;
	}

	if (!p_is_curr_task) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. A new window
		 * has also started, but p is not the current task, so the
		 * window is not rolled over - just split up and account
		 * as necessary into curr and prev. The window is only
		 * rolled over when a new window is processed for the current
		 * task.
		 *
		 * Irqtime can't be accounted by a task that isn't the
		 * currently running task.
		 */

		if (!nr_full_windows) {
			/*
			 * A full window hasn't elapsed, account partial
			 * contribution to previous completed window.
			 */
			delta = scale_exec_time(window_start - mark_start, rq);
			if (!exiting_task(p))
				wtr->prev_window += delta;
		} else {
			/*
			 * Since at least one full window has elapsed,
			 * the contribution to the previous window is the
			 * full window (window_size).
			 */
			delta = scale_exec_time(window_size, rq);
			if (!exiting_task(p))
				wtr->prev_window = delta;
		}
		wrq->prev_runnable_sum += delta;

		/* Account piece of busy time in the current window. */
		delta = scale_exec_time(wallclock - window_start, rq);
		wrq->curr_runnable_sum += delta;
		if (!exiting_task(p))
			wtr->curr_window = delta;

		return;
	}

	if (!irqtime || !is_idle_task(p) || cpu_is_waiting_on_io(rq)) {
		/*
		 *account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. A new window
		 * has started and p is the current task so rollover is
		 * needed. If any of these three above conditions are true
		 * then this busy time can't be accounted as irqtime.
		 *
		 * Busy time for the idle task or exiting tasks need not
		 * be accounted.
		 *
		 * An example of this would be a task that starts execution
		 * and then sleeps once a new window has begun.
		 */

		if (!nr_full_windows) {
			/*
			 * A full window hasn't elapsed, account partial
			 * contribution to previous completed window.
			 */
			delta = scale_exec_time(window_start - mark_start, rq);
			if (!is_idle_task(p) && !exiting_task(p))
				wtr->prev_window += delta;

			delta += wrq->curr_runnable_sum;
		} else {
			/*
			 * Since at least one full window has elapsed,
			 * the contribution to the previous window is the
			 * full window (window_size).
			 */
			delta = scale_exec_time(window_size, rq);
			if (!is_idle_task(p) && !exiting_task(p))
				wtr->prev_window = delta;

		}
		/*
		 * Rollover for normal runnable sum is done here by overwriting
		 * the values in prev_runnable_sum and curr_runnable_sum.
		 * Rollover for new task runnable sum has completed by previous
		 * if-else statement.
		 */
		wrq->prev_runnable_sum = delta;

		/* Account piece of busy time in the current window. */
		delta = scale_exec_time(wallclock - window_start, rq);
		wrq->curr_runnable_sum = delta;
		if (!is_idle_task(p) && !exiting_task(p))
			wtr->curr_window = delta;

		return;
	}

	if (irqtime) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. A new window
		 * has started and p is the current task so rollover is
		 * needed. The current task must be the idle task because
		 * irqtime is not accounted for any other task.
		 *
		 * Irqtime will be accounted each time we process IRQ activity
		 * after a period of idleness, so we know the IRQ busy time
		 * started at wallclock - irqtime.
		 */

		BUG_ON(!is_idle_task(p));
		mark_start = wallclock - irqtime;

		/*
		 * Roll window over. If IRQ busy time was just in the current
		 * window then that is all that need be accounted.
		 */
		wrq->prev_runnable_sum = wrq->curr_runnable_sum;
		if (mark_start > window_start) {
			wrq->curr_runnable_sum = scale_exec_time(irqtime, rq);
			return;
		}

		/*
		 * The IRQ busy time spanned multiple windows. Process the
		 * busy time preceding the current window start first.
		 */
		delta = window_start - mark_start;
		if (delta > window_size)
			delta = window_size;
		delta = scale_exec_time(delta, rq);
		wrq->prev_runnable_sum += delta;

		/* Process the remaining IRQ busy time in the current window. */
		delta = wallclock - window_start;
		wrq->curr_runnable_sum = scale_exec_time(delta, rq);

		return;
	}

	BUG();
}

static int account_busy_for_task_demand(struct task_struct *p, int event)
{
	unsigned int account_wait_time = 0;

	/*
	 * No need to bother updating task demand for exiting tasks
	 * or the idle task.
	 */
	if (exiting_task(p) || is_idle_task(p))
		return 0;

	if (tg_account_wait_time(p) || sysctl_walt_account_wait_time)
		account_wait_time = 1;

	/*
	 * When a task is waking up it is completing a segment of non-busy
	 * time. Likewise, if wait time is not treated as busy time, then
	 * when a task begins to run or is migrated, it is not running and
	 * is completing a segment of non-busy time.
	 */
	if (event == TASK_WAKE || (!account_wait_time &&
			 (event == PICK_NEXT_TASK || event == TASK_MIGRATE)))
		return 0;

	return 1;
}

/*
 * Called when new window is starting for a task, to record cpu usage over
 * recently concluded window(s). Normally 'samples' should be 1. It can be > 1
 * when, say, a real-time task runs without preemption for several windows at a
 * stretch.
 */
static void update_history(struct rq *rq, struct task_struct *p,
			 u32 runtime, int samples, int event)
{
	struct walt_task_ravg *wtr = (struct walt_task_ravg *)p->android_vendor_data1;
	u32 *hist = &wtr->sum_history[0];
	int ridx, widx;
	u32 max = 0, avg, demand;
	u64 sum = 0;

	/* Ignore windows where task had no activity */
	if (!runtime || is_idle_task(p) || exiting_task(p) || !samples)
		goto done;

	/* Push new 'runtime' value onto stack */
	widx = walt_ravg_hist_size - 1;
	ridx = widx - samples;
	for (; ridx >= 0; --widx, --ridx) {
		hist[widx] = hist[ridx];
		sum += hist[widx];
		if (hist[widx] > max)
			max = hist[widx];
	}

	for (widx = 0; widx < samples && widx < walt_ravg_hist_size; widx++) {
		hist[widx] = runtime;
		sum += hist[widx];
		if (hist[widx] > max)
			max = hist[widx];
	}

	wtr->sum = 0;

	if (walt_window_stats_policy == WINDOW_STATS_RECENT) {
		demand = runtime;
	} else if (walt_window_stats_policy == WINDOW_STATS_MAX) {
		demand = max;
	} else {
		avg = div64_u64(sum, walt_ravg_hist_size);
		if (walt_window_stats_policy == WINDOW_STATS_AVG)
			demand = avg;
		else
			demand = max(avg, runtime);
	}

	/*
	 * A throttled deadline sched class task gets dequeued without
	 * changing p->on_rq. Since the dequeue decrements hmp stats
	 * avoid decrementing it here again.
	 *
	 * When window is rolled over, the cumulative window demand
	 * is reset to the cumulative runnable average (contribution from
	 * the tasks on the runqueue). If the current task is dequeued
	 * already, it's demand is not included in the cumulative runnable
	 * average. So add the task demand separately to cumulative window
	 * demand.
	 */
	if (!task_has_dl_policy(p) || !p->dl.dl_throttled) {
		if (task_on_rq_queued(p))
			walt_fixup_cumulative_runnable_avg(rq, wtr, demand);
		else if (rq->curr == p)
			fixup_cum_window_demand(rq, demand);
	}

	wtr->demand = demand;
	wtr->demand_scale = scale_demand(demand);

done:
	trace_walt_update_history(rq, p, wtr, runtime, samples, event);
}

static void add_to_task_demand(struct rq *rq, struct task_struct *p,
				u64 delta)
{
	struct walt_task_ravg *wtr = (struct walt_task_ravg *)p->android_vendor_data1;

	delta = scale_exec_time(delta, rq);
	wtr->sum += delta;
	if (unlikely(wtr->sum > walt_ravg_window))
		wtr->sum = walt_ravg_window;

	if (sysctl_sched_walt_cross_window_util) {
		wtr->sum_latest += delta;
		if (unlikely(wtr->sum_latest > walt_ravg_window))
			wtr->sum_latest = walt_ravg_window;
	}
}

/*
 * Account cpu demand of task and/or update task's cpu demand history
 *
 * ms = p->ravg.mark_start;
 * wc = wallclock
 * ws = rq->window_start
 *
 * Three possibilities:
 *
 *	a) Task event is contained within one window.
 *		window_start < mark_start < wallclock
 *
 *		ws   ms  wc
 *		|    |   |
 *		V    V   V
 *		|---------------|
 *
 *	In this case, p->ravg.sum is updated *iff* event is appropriate
 *	(ex: event == PUT_PREV_TASK)
 *
 *	b) Task event spans two windows.
 *		mark_start < window_start < wallclock
 *
 *		ms   ws   wc
 *		|    |    |
 *		V    V    V
 *		-----|-------------------
 *
 *	In this case, p->ravg.sum is updated with (ws - ms) *iff* event
 *	is appropriate, then a new window sample is recorded followed
 *	by p->ravg.sum being set to (wc - ws) *iff* event is appropriate.
 *
 *	c) Task event spans more than two windows.
 *
 *		ms ws_tmp			   ws  wc
 *		|  |				   |   |
 *		V  V				   V   V
 *		---|-------|-------|-------|-------|------
 *		   |				   |
 *		   |<------ nr_full_windows ------>|
 *
 *	In this case, p->ravg.sum is updated with (ws_tmp - ms) first *iff*
 *	event is appropriate, window sample of p->ravg.sum is recorded,
 *	'nr_full_window' samples of window_size is also recorded *iff*
 *	event is appropriate and finally p->ravg.sum is set to (wc - ws)
 *	*iff* event is appropriate.
 *
 * IMPORTANT : Leave p->ravg.mark_start unchanged, as update_cpu_busy_time()
 * depends on it!
 */
static void update_task_demand(struct task_struct *p, struct rq *rq,
	     int event, u64 wallclock)
{
	struct walt_task_ravg *wtr = (struct walt_task_ravg *)p->android_vendor_data1;
	struct walt_rq *wrq = (struct walt_rq *)rq->android_vendor_data1;
	u64 mark_start = wtr->mark_start;
	u64 delta, window_start = wrq->window_start;
	int new_window, nr_full_windows;
	u32 window_size = walt_ravg_window;
	u32 window_scale = scale_exec_time(window_size, rq);

	new_window = mark_start < window_start;

	if (!account_busy_for_task_demand(p, event)) {
		if (new_window) {
			/*
			 * If the time accounted isn't being accounted as
			 * busy time, and a new window started, only the
			 * previous window need be closed out with the
			 * pre-existing demand. Multiple windows may have
			 * elapsed, but since empty windows are dropped,
			 * it is not necessary to account those.
			 */
			update_history(rq, p, wtr->sum, 1, event);
		}
		if (sysctl_sched_walt_cross_window_util)
			wtr->sum_latest = 0;
		return;
	}

	if (!new_window) {
		/*
		 * The simple case - busy time contained within the existing
		 * window.
		 */
		add_to_task_demand(rq, p, wallclock - mark_start);

		goto done;
	}

	/*
	 * Busy time spans at least two windows. Temporarily rewind
	 * window_start to first window boundary after mark_start.
	 */
	delta = window_start - mark_start;
	nr_full_windows = div64_u64(delta, window_size);
	window_start -= (u64)nr_full_windows * (u64)window_size;

	/* Process (window_start - mark_start) first */
	add_to_task_demand(rq, p, window_start - mark_start);

	/* Push new sample(s) into task's demand history */
	update_history(rq, p, wtr->sum, 1, event);
	if (sysctl_sched_walt_cross_window_util)
		wtr->sum = wtr->sum_latest;
	if (nr_full_windows) {
		update_history(rq, p, window_scale,
			       nr_full_windows, event);
		if (sysctl_sched_walt_cross_window_util) {
			wtr->sum = window_scale;
			wtr->sum_latest = window_scale;
		}
	}
	/*
	 * Roll window_start back to current to process any remainder
	 * in current window.
	 */
	window_start += (u64)nr_full_windows * (u64)window_size;

	/* Process (wallclock - window_start) next */
	mark_start = window_start;
	add_to_task_demand(rq, p, wallclock - mark_start);

done:
	/*
	 * Update task demand in current window when policy is
	 * WINDOW_STATS_MAX. The purpose is to create opportunity
	 * for rising cpu freq when cr_avg is used for cpufreq
	 */
	if (wtr->sum > wtr->demand && walt_window_stats_policy == WINDOW_STATS_MAX) {
		if (!task_has_dl_policy(p) || !p->dl.dl_throttled) {
			if (task_on_rq_queued(p))
				walt_fixup_cumulative_runnable_avg(rq, wtr, wtr->sum);
			else if (rq->curr == p)
				fixup_cum_window_demand(rq, wtr->sum);
		}
		wtr->demand = wtr->sum;
		wtr->demand_scale = scale_demand(wtr->sum);
	}
}

/* Reflect task activity on its demand and cpu's busy time statistics */
static void walt_update_task_ravg(struct task_struct *p, struct rq *rq,
	     int event, u64 wallclock, u64 irqtime)
{
	struct walt_task_ravg *wtr = (struct walt_task_ravg *)p->android_vendor_data1;
	struct walt_rq *wrq = (struct walt_rq *)rq->android_vendor_data1;
	u64 old_window_start;

	if (unlikely(!wrq->window_start))
		return;

	lockdep_assert_rq_held(rq);

	old_window_start = update_window_start(rq, wallclock);

	if (!wtr->mark_start)
		goto done;

	update_task_demand(p, rq, event, wallclock);
	update_cpu_busy_time(p, rq, event, wallclock, irqtime);

done:
	if (wrq->window_start > old_window_start) {
		unsigned long cap_orig = capacity_orig_of(cpu_of(rq));
		u64 busy_limit = (walt_ravg_window * sysctl_walt_busy_threshold) / 100;

		busy_limit = (busy_limit * cap_orig) >> SCHED_CAPACITY_SHIFT;
		if (wrq->prev_runnable_sum >= busy_limit) {
			if (wrq->is_busy == CPU_BUSY_CLR)
				wrq->is_busy = CPU_BUSY_PREPARE;
			else if (wrq->is_busy == CPU_BUSY_PREPARE)
				wrq->is_busy = CPU_BUSY_SET;
		} else if (wrq->is_busy != CPU_BUSY_CLR) {
			wrq->is_busy = CPU_BUSY_CLR;
		}
	}

	trace_walt_update_task_ravg(p, rq, wtr, wrq, event, wallclock, irqtime);

	wtr->mark_start = wallclock;
}

/*
 * static void reset_task_stats(struct task_struct *p)
 * {
 *         struct walt_task_ravg *wtr = (struct walt_task_ravg *)p->android_vendor_data1;
 *
 *         memset(wtr, 0, sizeof(struct walt_task_ravg));
 * }
 */

static void walt_mark_task_starting(struct task_struct *p)
{
	u64 wallclock;
	struct rq *rq = task_rq(p);
	struct walt_task_ravg *wtr = (struct walt_task_ravg *)p->android_vendor_data1;
	struct walt_rq *wrq = (struct walt_rq *)rq->android_vendor_data1;

	if (unlikely(!wrq->window_start)) {
//		reset_task_stats(p);
		return;
	}

	wallclock = walt_ktime_clock();
	wtr->mark_start = wallclock;
}

static void walt_set_window_start(struct rq *rq)
{
	struct walt_rq *wrq = (struct walt_rq *)rq->android_vendor_data1;
	struct walt_task_ravg *curr_wtr = (struct walt_task_ravg *)rq->curr->android_vendor_data1;

	if (likely(wrq->window_start))
		return;

	if (cpu_of(rq) == sync_cpu) {
		wrq->window_start = 1;
	} else {
		struct rq *sync_rq = cpu_rq(sync_cpu);
		struct walt_rq *sync_wrq = (struct walt_rq *)sync_rq->android_vendor_data1;

		raw_spin_rq_unlock(rq);
		double_rq_lock(rq, sync_rq);
		wrq->window_start = sync_wrq->window_start;
		wrq->curr_runnable_sum = wrq->prev_runnable_sum = 0;
		raw_spin_rq_unlock(sync_rq);
	}

	curr_wtr->mark_start = wrq->window_start;
}

static void walt_migrate_sync_cpu(int cpu)
{
	if (cpu == sync_cpu)
		sync_cpu = smp_processor_id();
}

static void walt_account_irqtime(int cpu, struct task_struct *curr, u64 delta)
{
	struct rq *rq = cpu_rq(cpu);
	struct walt_rq *wrq = (struct walt_rq *)rq->android_vendor_data1;
	unsigned long flags;
	u64 cur_jiffies_ts, nr_windows;

	raw_spin_rq_lock_irqsave(rq, flags);

	cur_jiffies_ts = get_jiffies_64();

	if (is_idle_task(curr))
		walt_update_task_ravg(curr, rq, IRQ_UPDATE, walt_ktime_clock(),
				 delta);

	nr_windows = cur_jiffies_ts - wrq->irqload_ts;

	if (nr_windows) {
		if (nr_windows < 10) {
			/* Decay CPU's irqload by 3/4 for each window. */
			wrq->avg_irqload *= (3 * nr_windows);
			wrq->avg_irqload = div64_u64(wrq->avg_irqload,
						    4 * nr_windows);
		} else {
			wrq->avg_irqload = 0;
		}
		wrq->avg_irqload += wrq->cur_irqload;
		wrq->cur_irqload = 0;
	}

	wrq->cur_irqload += delta;
	wrq->irqload_ts = cur_jiffies_ts;
	raw_spin_rq_unlock_irqrestore(rq, flags);
}

static void walt_fixup_busy_time(struct task_struct *p, int new_cpu)
{
	struct rq *src_rq = task_rq(p);
	struct rq *dest_rq = cpu_rq(new_cpu);
	struct walt_rq *src_wrq = (struct walt_rq *)src_rq->android_vendor_data1;
	struct walt_rq *dst_wrq = (struct walt_rq *)dest_rq->android_vendor_data1;
	struct walt_task_ravg *wtr = (struct walt_task_ravg *)p->android_vendor_data1;
	u64 wallclock;
	unsigned int p_state = READ_ONCE(p->__state);

	if (!p->on_rq && p_state != TASK_WAKING)
		return;

	if (exiting_task(p))
		return;

	if (p_state == TASK_WAKING)
		double_rq_lock(src_rq, dest_rq);

	lockdep_assert_rq_held(src_rq);
	lockdep_assert_rq_held(dest_rq);

	wallclock = walt_ktime_clock();

	walt_update_task_ravg(task_rq(p)->curr, task_rq(p),
			TASK_UPDATE, wallclock, 0);
	walt_update_task_ravg(dest_rq->curr, dest_rq,
			TASK_UPDATE, wallclock, 0);

	walt_update_task_ravg(p, task_rq(p), TASK_MIGRATE, wallclock, 0);

	/*
	 * When a task is migrating during the wakeup, adjust
	 * the task's contribution towards cumulative window
	 * demand.
	 */
	if (p_state == TASK_WAKING &&
	    wtr->last_sleep_ts >= src_wrq->window_start) {
		fixup_cum_window_demand(src_rq, -(s64)wtr->demand);
		fixup_cum_window_demand(dest_rq, wtr->demand);
	}

	if (wtr->curr_window) {
		src_wrq->curr_runnable_sum -= wtr->curr_window;
		dst_wrq->curr_runnable_sum += wtr->curr_window;
	}

	if (wtr->prev_window) {
		src_wrq->prev_runnable_sum -= wtr->prev_window;
		dst_wrq->prev_runnable_sum += wtr->prev_window;
	}

	if ((s64)src_wrq->prev_runnable_sum < 0) {
		src_wrq->prev_runnable_sum = 0;
		WARN_ON(1);
	}
	if ((s64)src_wrq->curr_runnable_sum < 0) {
		src_wrq->curr_runnable_sum = 0;
		WARN_ON(1);
	}

	trace_walt_migration_update_sum(src_rq, src_wrq, p);
	trace_walt_migration_update_sum(dest_rq, dst_wrq, p);

	if (p_state == TASK_WAKING)
		double_rq_unlock(src_rq, dest_rq);
}

static void __sched_fork_init(struct task_struct *p)
{
	struct walt_task_ravg *wtr = (struct walt_task_ravg *)p->android_vendor_data1;

	wtr->last_sleep_ts = 0;
	wtr->last_enqueue_ts = 0;
}

static void walt_init_new_task_load(struct task_struct *p)
{
	int i;
	struct walt_task_ravg *wtr = (struct walt_task_ravg *)p->android_vendor_data1;
	struct walt_task_ravg *cur_wtr =
			(struct walt_task_ravg *)current->android_vendor_data1;
	u32 init_load_windows =
			div64_u64((u64)sysctl_sched_walt_init_task_load_pct *
					(u64)walt_ravg_window, 100);
	u32 init_load_pct = cur_wtr->init_load_pct;
	u32 tg_load_pct = tg_init_load_pct(p);

	wtr->init_load_pct = 0;
	wtr->mark_start = 0;
	wtr->sum = 0;
	wtr->sum_latest = 0;
	wtr->curr_window = 0;
	wtr->prev_window = 0;

	init_load_pct = init_load_pct > tg_load_pct ? init_load_pct : tg_load_pct;

	if (init_load_pct) {
		init_load_windows = div64_u64((u64)init_load_pct *
			  (u64)walt_ravg_window, 100);
	}

	if (unlikely(is_idle_task(p)))
		init_load_windows = 0;

	wtr->demand = init_load_windows;
	wtr->demand_scale = scale_demand(init_load_windows);
	for (i = 0; i < RAVG_HIST_SIZE_MAX; ++i)
		wtr->sum_history[i] = init_load_windows;

	__sched_fork_init(p);
}
struct list_head cluster_head;
int num_sched_clusters;

static struct sched_cluster init_cluster = {
	.list			= LIST_HEAD_INIT(init_cluster.list),
	.id			= 0,
	.capacity		= 1024,
};

static void init_clusters(void)
{
	init_cluster.cpus = *cpu_possible_mask;
	raw_spin_lock_init(&init_cluster.load_lock);
	INIT_LIST_HEAD(&cluster_head);
	list_add(&init_cluster.list, &cluster_head);
}

static void insert_cluster(struct sched_cluster *cluster, struct list_head *head)
{
	struct sched_cluster *tmp;
	struct list_head *iter = head;

	list_for_each_entry(tmp, head, list) {
		if (arch_scale_cpu_capacity(cpumask_first(&cluster->cpus))
			< arch_scale_cpu_capacity(cpumask_first(&tmp->cpus)))
			break;
		iter = &tmp->list;
	}

	list_add(&cluster->list, iter);
}

static struct sched_cluster *alloc_new_cluster(const struct cpumask *cpus)
{
	struct sched_cluster *cluster = NULL;

	cluster = kzalloc(sizeof(struct sched_cluster), GFP_ATOMIC);
	BUG_ON(!cluster);

	INIT_LIST_HEAD(&cluster->list);

	raw_spin_lock_init(&cluster->load_lock);
	cluster->cpus = *cpus;

	return cluster;
}

static void add_cluster(const struct cpumask *cpus, struct list_head *head)
{
	struct sched_cluster *cluster = alloc_new_cluster(cpus);
	int i;
	struct walt_rq *wrq;

	for_each_cpu(i, cpus) {
		wrq = (struct walt_rq *) cpu_rq(i)->android_vendor_data1;
		wrq->cluster = cluster;
	}
	insert_cluster(cluster, head);
	num_sched_clusters++;
}

static void cleanup_clusters(struct list_head *head)
{
	struct sched_cluster *cluster, *tmp;
	int i;
	struct walt_rq *wrq;

	list_for_each_entry_safe(cluster, tmp, head, list) {
		for_each_cpu(i, &cluster->cpus) {
			wrq = (struct walt_rq *) cpu_rq(i)->android_vendor_data1;
			wrq->cluster = &init_cluster;
		}
		list_del(&cluster->list);
		num_sched_clusters--;
		kfree(cluster);
	}
}

static inline void assign_cluster_ids(struct list_head *head)
{
	struct sched_cluster *cluster;
	int pos = 0;

	list_for_each_entry(cluster, head, list) {
		cluster->id = pos;
		pos++;
	}

	WARN_ON(pos > MAX_CLUSTERS);
}

static inline void
move_list(struct list_head *dst, struct list_head *src, bool sync_rcu)
{
	struct list_head *first, *last;

	first = src->next;
	last = src->prev;

	if (sync_rcu) {
		INIT_LIST_HEAD_RCU(src);
		synchronize_rcu();
	}

	first->prev = dst;
	dst->prev = last;
	last->next = dst;

	/* Ensure list sanity beore making the head visible to all CPUs. */
	smp_mb();
	dst->next = first;
}

static void parse_capacity_from_clusters(void)
{
	struct sched_cluster *cluster;
	unsigned long biggest_cap = 0, smallest_cap = ULONG_MAX;

	for_each_sched_cluster(cluster) {
		unsigned long cap = arch_scale_cpu_capacity(cluster_first_cpu(cluster));

		if (cap > biggest_cap)
			biggest_cap = cap;

		if (cap < smallest_cap)
			smallest_cap = cap;

		cluster->capacity = cap;
	}

	max_possible_capacity = biggest_cap;
	min_max_possible_capacity = smallest_cap;
}

static void get_possible_siblings(int cpuid, struct cpumask *cluster_cpus)
{
	int cpu;
	struct cpu_topology *cpuid_topo = &cpu_topology[cpuid];
	unsigned long cpu_cap, cpuid_cap = arch_scale_cpu_capacity(cpuid);

	if (cpuid_topo->package_id == -1)
		return;

	for_each_possible_cpu(cpu) {
		cpu_cap = arch_scale_cpu_capacity(cpu);

		if (cpu_cap != cpuid_cap)
			continue;
		cpumask_set_cpu(cpu, cluster_cpus);
	}
}

static void walt_update_cluster_topology(void)
{
	struct cpumask cpus = *cpu_possible_mask;
	struct cpumask cluster_cpus;
	struct list_head new_head;
	int i;

	INIT_LIST_HEAD(&new_head);

	for_each_cpu(i, &cpus) {
		cpumask_clear(&cluster_cpus);
		get_possible_siblings(i, &cluster_cpus);
		if (cpumask_empty(&cluster_cpus)) {
			WARN(1, "WALT: Invalid cpu topology!!");
			cleanup_clusters(&new_head);
			return;
		}
		cpumask_andnot(&cpus, &cpus, &cluster_cpus);
		add_cluster(&cluster_cpus, &new_head);
	}

	assign_cluster_ids(&new_head);

	/*
	 * Ensure cluster ids are visible to all CPUs before making
	 * cluster_head visible.
	 */
	move_list(&cluster_head, &new_head, false);
	parse_capacity_from_clusters();
}

static void walt_init_existing_task_load(struct task_struct *p)
{
	int i;
	struct walt_task_ravg *wtr = (struct walt_task_ravg *)p->android_vendor_data1;

	wtr->init_load_pct = 0;
	wtr->mark_start = 0;
	wtr->sum = 0;
	wtr->sum_latest = 0;
	wtr->curr_window = 0;
	wtr->prev_window = 0;

	wtr->demand = 0;
	wtr->demand_scale = 0;
	for (i = 0; i < RAVG_HIST_SIZE_MAX; ++i)
		wtr->sum_history[i] = 0;

	__sched_fork_init(p);
}

static void walt_sched_init_rq(struct rq *rq)
{
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;

	wrq->push_task = NULL;

	wrq->cumulative_runnable_avg = 0;
	wrq->window_start = 0;
	wrq->cur_irqload = 0;
	wrq->avg_irqload = 0;
	wrq->irqload_ts = 0;
	wrq->is_busy = 0;
	wrq->sched_flag = 0;

	wrq->cumulative_runnable_avg = 0;
	wrq->curr_runnable_sum = wrq->prev_runnable_sum = 0;
}

DEFINE_STATIC_KEY_TRUE(walt_disabled);

static void walt_update_task_group(struct cgroup_subsys_state *css)
{
	if (!strcmp(css->cgroup->kn->name, "top-app"))
		walt_init_topapp_tg(css_tg(css));
	else
		walt_init_tg(css_tg(css));
}

static void android_rvh_cpu_cgroup_online(void *data, struct cgroup_subsys_state *css)
{
	if (static_branch_unlikely(&walt_disabled))
		return;

	walt_update_task_group(css);
}

static void android_rvh_build_perf_domains(void *data, bool *eas_check)
{
	if (static_branch_unlikely(&walt_disabled))
		return;

	*eas_check = true;
}

static void android_rvh_sched_cpu_starting(void *data, int cpu)
{
	unsigned long flags;
	struct rq *rq = cpu_rq(cpu);

	if (static_branch_unlikely(&walt_disabled))
		return;

	raw_spin_rq_lock_irqsave(rq, flags);
	walt_set_window_start(rq);
	raw_spin_rq_unlock_irqrestore(rq, flags);
}

static void android_rvh_sched_cpu_dying(void *data, int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct rq_flags rf;

	if (static_branch_unlikely(&walt_disabled))
		return;

	rq_lock_irqsave(rq, &rf);
	walt_migrate_sync_cpu(cpu);
	rq_unlock_irqrestore(rq, &rf);
}

static void android_rvh_sched_fork_init(void *data, struct task_struct *p)
{
	if (static_branch_unlikely(&walt_disabled))
		return;

	__sched_fork_init(p);
}

static void android_rvh_wake_up_new_task(void *data, struct task_struct *p)
{
	if (static_branch_unlikely(&walt_disabled))
		return;

	walt_init_new_task_load(p);
}

static void android_rvh_new_task_stats(void *data, struct task_struct *p)
{
	if (static_branch_unlikely(&walt_disabled))
		return;

	walt_mark_task_starting(p);
}

static void android_rvh_set_task_cpu(void *data, struct task_struct *p, unsigned int new_cpu)
{
	if (static_branch_unlikely(&walt_disabled))
		return;

	walt_fixup_busy_time(p, new_cpu);

}

static void android_rvh_try_to_wake_up(void *data, struct task_struct *p)
{
	struct rq *rq = cpu_rq(task_cpu(p));
	struct rq_flags rf;
	u64 wallclock;

	if (static_branch_unlikely(&walt_disabled))
		return;

	rq_lock_irqsave(rq, &rf);
	wallclock = walt_ktime_clock();
	walt_update_task_ravg(rq->curr, rq, TASK_UPDATE, wallclock, 0);
	walt_update_task_ravg(p, rq, TASK_WAKE, wallclock, 0);
	rq_unlock_irqrestore(rq, &rf);
}

static void android_rvh_try_to_wake_up_success(void *data, struct task_struct *p)
{
	if (static_branch_unlikely(&walt_disabled))
		return;
	//reserved
}

static void android_rvh_enqueue_task(void *data, struct rq *rq, struct task_struct *p, int flags)
{
	if (static_branch_unlikely(&walt_disabled))
		return;

	walt_inc_cumulative_runnable_avg(rq, p);
}

static void android_rvh_after_enqueue_task(void *data, struct rq *rq, struct task_struct *p, int flags)
{
	struct walt_task_ravg *wtr = (struct walt_task_ravg *)p->android_vendor_data1;
	u64 wallclock = walt_ktime_clock();

	if (static_branch_unlikely(&walt_disabled))
		return;

	wtr->last_enqueue_ts = wallclock;

	walt_cpufreq_update_util(rq, 0);
}

static void android_rvh_dequeue_task(void *data, struct rq *rq, struct task_struct *p, int flags)
{
	if (static_branch_unlikely(&walt_disabled))
		return;

	walt_dec_cumulative_runnable_avg(rq, p);
}

static void android_rvh_after_dequeue_task(void *data, struct rq *rq, struct task_struct *p, int flags)
{
	if (static_branch_unlikely(&walt_disabled))
		return;

	walt_cpufreq_update_util(rq, 0);
}

static void android_rvh_tick_entry(void *data, struct rq *rq)
{
	lockdep_assert_rq_held(rq);

	if (static_branch_unlikely(&walt_disabled))
		return;

	walt_set_window_start(rq);
	walt_update_task_ravg(rq->curr, rq, TASK_UPDATE, walt_ktime_clock(), 0);

	walt_cpufreq_update_util(rq, 0);
}

static void android_rvh_account_irq(void *data, struct task_struct *curr, int cpu, s64 delta)
{
	if (static_branch_unlikely(&walt_disabled) || !sysctl_walt_account_irq_time)
		return;

	if (hardirq_count() ||
	   (in_serving_softirq() && curr != this_cpu_ksoftirqd()))
		walt_account_irqtime(cpu, curr, delta);
}

static void android_rvh_schedule(void *data, struct task_struct *prev,
				 struct task_struct *next, struct rq *rq)
{
	struct walt_task_ravg *prev_wtr = (struct walt_task_ravg *)prev->android_vendor_data1;
	u64 wallclock = walt_ktime_clock();

	if (static_branch_unlikely(&walt_disabled))
		return;

	if (likely(prev != next)) {
		if (!prev->on_rq)
			prev_wtr->last_sleep_ts = wallclock;

		walt_update_task_ravg(prev, rq, PUT_PREV_TASK, wallclock, 0);
		walt_update_task_ravg(next, rq, PICK_NEXT_TASK, wallclock, 0);
	} else {
		walt_update_task_ravg(prev, rq, TASK_UPDATE, wallclock, 0);
	}
}

static void walt_effective_cpu_util(void *data, int cpu, unsigned long util_cfs,
				    unsigned long max, int type,
				    struct task_struct *p, unsigned long *new_util)
{
	u64 walt_cpu_util;
	struct rq *rq = cpu_rq(cpu);
	struct walt_rq *wrq = (struct walt_rq *) rq->android_vendor_data1;
	u64 prev_runnable_sum;

	if (static_branch_unlikely(&walt_disabled))
		return;

	walt_cpu_util = wrq->cumulative_runnable_avg;
	walt_cpu_util <<= SCHED_CAPACITY_SHIFT;
	do_div(walt_cpu_util, walt_ravg_window);

	prev_runnable_sum = wrq->prev_runnable_sum;
	prev_runnable_sum <<= SCHED_CAPACITY_SHIFT;
	do_div(prev_runnable_sum, walt_ravg_window);

	walt_cpu_util = max(walt_cpu_util, prev_runnable_sum);

	*new_util = min_t(unsigned long, walt_cpu_util, capacity_orig_of(cpu));
}

static void register_walt_vendor_hooks(void)
{
	register_trace_android_rvh_build_perf_domains(android_rvh_build_perf_domains, NULL);
	register_trace_android_rvh_sched_cpu_starting(android_rvh_sched_cpu_starting, NULL);
	register_trace_android_rvh_sched_cpu_dying(android_rvh_sched_cpu_dying, NULL);
	register_trace_android_rvh_sched_fork_init(android_rvh_sched_fork_init, NULL);
	register_trace_android_rvh_wake_up_new_task(android_rvh_wake_up_new_task, NULL);
	register_trace_android_rvh_new_task_stats(android_rvh_new_task_stats, NULL);
	register_trace_android_rvh_set_task_cpu(android_rvh_set_task_cpu, NULL);
	register_trace_android_rvh_try_to_wake_up(android_rvh_try_to_wake_up, NULL);
	register_trace_android_rvh_try_to_wake_up_success(android_rvh_try_to_wake_up_success, NULL);
	register_trace_android_rvh_enqueue_task(android_rvh_enqueue_task, NULL);
	register_trace_android_rvh_after_enqueue_task(android_rvh_after_enqueue_task, NULL);
	register_trace_android_rvh_dequeue_task(android_rvh_dequeue_task, NULL);
	register_trace_android_rvh_after_dequeue_task(android_rvh_after_dequeue_task, NULL);
	register_trace_android_rvh_tick_entry(android_rvh_tick_entry, NULL);
	register_trace_android_rvh_account_irq(android_rvh_account_irq, NULL);
	register_trace_android_rvh_schedule(android_rvh_schedule, NULL);
	register_trace_android_rvh_effective_cpu_util(walt_effective_cpu_util, NULL);
	register_trace_android_rvh_cpu_cgroup_online(android_rvh_cpu_cgroup_online, NULL);
}

static int walt_init_stop_handler(void *data)
{
	int cpu;
	struct task_struct *g, *p;
	u64 window_start_ns, nr_windows;
	struct walt_rq *wrq;

	read_lock(&tasklist_lock);
	for_each_possible_cpu(cpu) {
		raw_spin_rq_lock(cpu_rq(cpu));
	}

	do_each_thread(g, p) {
		walt_init_existing_task_load(p);
	} while_each_thread(g, p);

	window_start_ns = ktime_get_ns();
	nr_windows = div64_u64(window_start_ns, walt_ravg_window);
	window_start_ns = (u64)nr_windows * (u64)walt_ravg_window;

	for_each_possible_cpu(cpu) {
		struct rq *rq = cpu_rq(cpu);

		/* Create task members for idle thread */
		walt_init_new_task_load(rq->idle);

		walt_sched_init_rq(rq);

		wrq = (struct walt_rq *) rq->android_vendor_data1;
		wrq->window_start = window_start_ns;
	}

	walt_update_cluster_topology();

	static_branch_disable(&walt_disabled);

	for_each_possible_cpu(cpu) {
		raw_spin_rq_unlock(cpu_rq(cpu));
	}
	read_unlock(&tasklist_lock);

	return 0;
}

static void walt_init_task_group_all(void)
{
        struct cgroup_subsys_state *css = &root_task_group.css;
        struct cgroup_subsys_state *top_css = css;

        rcu_read_lock();
        css_for_each_child(css, top_css)
                walt_update_task_group(css);
        rcu_read_unlock();
}

static void walt_init(struct work_struct *work)
{
	struct ctl_table_header *hdr;
	static atomic_t already_inited = ATOMIC_INIT(0);

	might_sleep();

	if (atomic_cmpxchg(&already_inited, 0, 1))
		return;

	register_syscore_ops(&walt_syscore_ops);

	init_clusters();

	walt_init_task_group_all();

	register_walt_vendor_hooks();
	walt_rt_init();
	walt_fair_init();

	stop_machine(walt_init_stop_handler, NULL, NULL);

	hdr = register_sysctl_table(walt_base_table);
}

static DECLARE_WORK(walt_init_work, walt_init);
static void android_vh_update_topology_flags_workfn(void *unused, void *unused2)
{
	schedule_work(&walt_init_work);
}

#define WALT_VENDOR_DATA_TEST(wstruct, kstruct)		\
	BUILD_BUG_ON(sizeof(wstruct) > (sizeof(u64) *	\
			ARRAY_SIZE(((kstruct *)0)->android_vendor_data1)))

static __init int walt_module_init(void)
{
	WALT_VENDOR_DATA_TEST(struct walt_task_ravg, struct task_struct);
	WALT_VENDOR_DATA_TEST(struct walt_rq, struct rq);
	WALT_VENDOR_DATA_TEST(struct walt_task_group, struct task_group);

	register_trace_android_vh_update_topology_flags_workfn(
			android_vh_update_topology_flags_workfn, NULL);

	if (topology_update_done)
		schedule_work(&walt_init_work);

	pr_info("walt sched module init done\n");

	return 0;
}
core_initcall(walt_module_init);
MODULE_LICENSE("GPL v2");
