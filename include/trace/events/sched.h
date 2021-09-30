/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM sched

#if !defined(_TRACE_SCHED_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SCHED_H

#include <linux/sched/numa_balancing.h>
#include <linux/tracepoint.h>
#include <linux/binfmts.h>
#include <linux/sched/idle.h>
#include "../../../kernel/sched/sprd-eas.h"

/*
 * Tracepoint for calling kthread_stop, performed to end a kthread:
 */
TRACE_EVENT(sched_kthread_stop,

	TP_PROTO(struct task_struct *t),

	TP_ARGS(t),

	TP_STRUCT__entry(
		__array(	char,	comm,	TASK_COMM_LEN	)
		__field(	pid_t,	pid			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, t->comm, TASK_COMM_LEN);
		__entry->pid	= t->pid;
	),

	TP_printk("comm=%s pid=%d", __entry->comm, __entry->pid)
);

/*
 * Tracepoint for the return value of the kthread stopping:
 */
TRACE_EVENT(sched_kthread_stop_ret,

	TP_PROTO(int ret),

	TP_ARGS(ret),

	TP_STRUCT__entry(
		__field(	int,	ret	)
	),

	TP_fast_assign(
		__entry->ret	= ret;
	),

	TP_printk("ret=%d", __entry->ret)
);

/*
 * Tracepoint for waking up a task:
 */
DECLARE_EVENT_CLASS(sched_wakeup_template,

	TP_PROTO(struct task_struct *p),

	TP_ARGS(__perf_task(p)),

	TP_STRUCT__entry(
		__array(	char,	comm,	TASK_COMM_LEN	)
		__field(	pid_t,	pid			)
		__field(	int,	prio			)
		__field(	int,	success			)
		__field(	int,	target_cpu		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->prio		= p->prio; /* XXX SCHED_DEADLINE */
		__entry->success	= 1; /* rudiment, kill when possible */
		__entry->target_cpu	= task_cpu(p);
	),

	TP_printk("comm=%s pid=%d prio=%u target_cpu=%03d",
		  __entry->comm, __entry->pid, __entry->prio,
		  __entry->target_cpu)
);

/*
 * Tracepoint called when waking a task; this tracepoint is guaranteed to be
 * called from the waking context.
 */
DEFINE_EVENT(sched_wakeup_template, sched_waking,
	     TP_PROTO(struct task_struct *p),
	     TP_ARGS(p));

/*
 * Tracepoint called when the task is actually woken; p->state == TASK_RUNNNG.
 * It it not always called from the waking context.
 */
DEFINE_EVENT(sched_wakeup_template, sched_wakeup,
	     TP_PROTO(struct task_struct *p),
	     TP_ARGS(p));

/*
 * Tracepoint for waking up a new task:
 */
DEFINE_EVENT(sched_wakeup_template, sched_wakeup_new,
	     TP_PROTO(struct task_struct *p),
	     TP_ARGS(p));

#ifdef CREATE_TRACE_POINTS
static inline long __trace_sched_switch_state(bool preempt, struct task_struct *p)
{
	unsigned int state;

#ifdef CONFIG_SCHED_DEBUG
	BUG_ON(p != current);
#endif /* CONFIG_SCHED_DEBUG */

	/*
	 * Preemption ignores task state, therefore preempted tasks are always
	 * RUNNING (we will not have dequeued if state != RUNNING).
	 */
	if (preempt)
		return TASK_REPORT_MAX;

	/*
	 * task_state_index() uses fls() and returns a value from 0-8 range.
	 * Decrement it by 1 (except TASK_RUNNING state i.e 0) before using
	 * it for left shift operation to get the correct task->state
	 * mapping.
	 */
	state = task_state_index(p);

	return state ? (1 << (state - 1)) : state;
}
#endif /* CREATE_TRACE_POINTS */

/*
 * Tracepoint for task switches, performed by the scheduler:
 */
TRACE_EVENT(sched_switch,

	TP_PROTO(bool preempt,
		 struct task_struct *prev,
		 struct task_struct *next),

	TP_ARGS(preempt, prev, next),

	TP_STRUCT__entry(
		__array(	char,	prev_comm,	TASK_COMM_LEN	)
		__field(	pid_t,	prev_pid			)
		__field(	int,	prev_prio			)
		__field(	long,	prev_state			)
		__array(	char,	next_comm,	TASK_COMM_LEN	)
		__field(	pid_t,	next_pid			)
		__field(	int,	next_prio			)
	),

	TP_fast_assign(
		memcpy(__entry->next_comm, next->comm, TASK_COMM_LEN);
		__entry->prev_pid	= prev->pid;
		__entry->prev_prio	= prev->prio;
		__entry->prev_state	= __trace_sched_switch_state(preempt, prev);
		memcpy(__entry->prev_comm, prev->comm, TASK_COMM_LEN);
		__entry->next_pid	= next->pid;
		__entry->next_prio	= next->prio;
		/* XXX SCHED_DEADLINE */
	),

	TP_printk("prev_comm=%s prev_pid=%d prev_prio=%u prev_state=%s%s ==> next_comm=%s next_pid=%d next_prio=%u",
		__entry->prev_comm, __entry->prev_pid, __entry->prev_prio,

		(__entry->prev_state & (TASK_REPORT_MAX - 1)) ?
		  __print_flags(__entry->prev_state & (TASK_REPORT_MAX - 1), "|",
				{ TASK_INTERRUPTIBLE, "S" },
				{ TASK_UNINTERRUPTIBLE, "D" },
				{ __TASK_STOPPED, "T" },
				{ __TASK_TRACED, "t" },
				{ EXIT_DEAD, "X" },
				{ EXIT_ZOMBIE, "Z" },
				{ TASK_PARKED, "P" },
				{ TASK_DEAD, "I" }) :
		  "R",

		__entry->prev_state & TASK_REPORT_MAX ? "+" : "",
		__entry->next_comm, __entry->next_pid, __entry->next_prio)
);

/*
 * Tracepoint for a task being migrated:
 */
TRACE_EVENT(sched_migrate_task,

	TP_PROTO(struct task_struct *p, int dest_cpu),

	TP_ARGS(p, dest_cpu),

	TP_STRUCT__entry(
		__array(	char,	comm,	TASK_COMM_LEN	)
		__field(	pid_t,	pid			)
		__field(	int,	prio			)
		__field(	int,	orig_cpu		)
		__field(	int,	dest_cpu		)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->prio		= p->prio; /* XXX SCHED_DEADLINE */
		__entry->orig_cpu	= task_cpu(p);
		__entry->dest_cpu	= dest_cpu;
	),

	TP_printk("comm=%s pid=%d prio=%d orig_cpu=%d dest_cpu=%d",
		  __entry->comm, __entry->pid, __entry->prio,
		  __entry->orig_cpu, __entry->dest_cpu)
);

DECLARE_EVENT_CLASS(sched_process_template,

	TP_PROTO(struct task_struct *p),

	TP_ARGS(p),

	TP_STRUCT__entry(
		__array(	char,	comm,	TASK_COMM_LEN	)
		__field(	pid_t,	pid			)
		__field(	int,	prio			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->prio		= p->prio; /* XXX SCHED_DEADLINE */
	),

	TP_printk("comm=%s pid=%d prio=%d",
		  __entry->comm, __entry->pid, __entry->prio)
);

/*
 * Tracepoint for freeing a task:
 */
DEFINE_EVENT(sched_process_template, sched_process_free,
	     TP_PROTO(struct task_struct *p),
	     TP_ARGS(p));

/*
 * Tracepoint for a task exiting:
 */
DEFINE_EVENT(sched_process_template, sched_process_exit,
	     TP_PROTO(struct task_struct *p),
	     TP_ARGS(p));

/*
 * Tracepoint for waiting on task to unschedule:
 */
DEFINE_EVENT(sched_process_template, sched_wait_task,
	TP_PROTO(struct task_struct *p),
	TP_ARGS(p));

/*
 * Tracepoint for a waiting task:
 */
TRACE_EVENT(sched_process_wait,

	TP_PROTO(struct pid *pid),

	TP_ARGS(pid),

	TP_STRUCT__entry(
		__array(	char,	comm,	TASK_COMM_LEN	)
		__field(	pid_t,	pid			)
		__field(	int,	prio			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
		__entry->pid		= pid_nr(pid);
		__entry->prio		= current->prio; /* XXX SCHED_DEADLINE */
	),

	TP_printk("comm=%s pid=%d prio=%d",
		  __entry->comm, __entry->pid, __entry->prio)
);

/*
 * Tracepoint for do_fork:
 */
TRACE_EVENT(sched_process_fork,

	TP_PROTO(struct task_struct *parent, struct task_struct *child),

	TP_ARGS(parent, child),

	TP_STRUCT__entry(
		__array(	char,	parent_comm,	TASK_COMM_LEN	)
		__field(	pid_t,	parent_pid			)
		__array(	char,	child_comm,	TASK_COMM_LEN	)
		__field(	pid_t,	child_pid			)
	),

	TP_fast_assign(
		memcpy(__entry->parent_comm, parent->comm, TASK_COMM_LEN);
		__entry->parent_pid	= parent->pid;
		memcpy(__entry->child_comm, child->comm, TASK_COMM_LEN);
		__entry->child_pid	= child->pid;
	),

	TP_printk("comm=%s pid=%d child_comm=%s child_pid=%d",
		__entry->parent_comm, __entry->parent_pid,
		__entry->child_comm, __entry->child_pid)
);

/*
 * Tracepoint for exec:
 */
TRACE_EVENT(sched_process_exec,

	TP_PROTO(struct task_struct *p, pid_t old_pid,
		 struct linux_binprm *bprm),

	TP_ARGS(p, old_pid, bprm),

	TP_STRUCT__entry(
		__string(	filename,	bprm->filename	)
		__field(	pid_t,		pid		)
		__field(	pid_t,		old_pid		)
	),

	TP_fast_assign(
		__assign_str(filename, bprm->filename);
		__entry->pid		= p->pid;
		__entry->old_pid	= old_pid;
	),

	TP_printk("filename=%s pid=%d old_pid=%d", __get_str(filename),
		  __entry->pid, __entry->old_pid)
);


#ifdef CONFIG_SCHEDSTATS
#define DEFINE_EVENT_SCHEDSTAT DEFINE_EVENT
#define DECLARE_EVENT_CLASS_SCHEDSTAT DECLARE_EVENT_CLASS
#else
#define DEFINE_EVENT_SCHEDSTAT DEFINE_EVENT_NOP
#define DECLARE_EVENT_CLASS_SCHEDSTAT DECLARE_EVENT_CLASS_NOP
#endif

/*
 * XXX the below sched_stat tracepoints only apply to SCHED_OTHER/BATCH/IDLE
 *     adding sched_stat support to SCHED_FIFO/RR would be welcome.
 */
DECLARE_EVENT_CLASS_SCHEDSTAT(sched_stat_template,

	TP_PROTO(struct task_struct *tsk, u64 delay),

	TP_ARGS(__perf_task(tsk), __perf_count(delay)),

	TP_STRUCT__entry(
		__array( char,	comm,	TASK_COMM_LEN	)
		__field( pid_t,	pid			)
		__field( u64,	delay			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid	= tsk->pid;
		__entry->delay	= delay;
	),

	TP_printk("comm=%s pid=%d delay=%Lu [ns]",
			__entry->comm, __entry->pid,
			(unsigned long long)__entry->delay)
);

/*
 * Tracepoint for accounting wait time (time the task is runnable
 * but not actually running due to scheduler contention).
 */
DEFINE_EVENT_SCHEDSTAT(sched_stat_template, sched_stat_wait,
	     TP_PROTO(struct task_struct *tsk, u64 delay),
	     TP_ARGS(tsk, delay));

/*
 * Tracepoint for accounting sleep time (time the task is not runnable,
 * including iowait, see below).
 */
DEFINE_EVENT_SCHEDSTAT(sched_stat_template, sched_stat_sleep,
	     TP_PROTO(struct task_struct *tsk, u64 delay),
	     TP_ARGS(tsk, delay));

/*
 * Tracepoint for accounting iowait time (time the task is not runnable
 * due to waiting on IO to complete).
 */
DEFINE_EVENT_SCHEDSTAT(sched_stat_template, sched_stat_iowait,
	     TP_PROTO(struct task_struct *tsk, u64 delay),
	     TP_ARGS(tsk, delay));

/*
 * Tracepoint for accounting blocked time (time the task is in uninterruptible).
 */
DEFINE_EVENT_SCHEDSTAT(sched_stat_template, sched_stat_blocked,
	     TP_PROTO(struct task_struct *tsk, u64 delay),
	     TP_ARGS(tsk, delay));

/*
 * Tracepoint for recording the cause of uninterruptible sleep.
 */
TRACE_EVENT(sched_blocked_reason,

	TP_PROTO(struct task_struct *tsk),

	TP_ARGS(tsk),

	TP_STRUCT__entry(
		__field( pid_t,	pid	)
		__field( void*, caller	)
		__field( bool, io_wait	)
	),

	TP_fast_assign(
		__entry->pid	= tsk->pid;
		__entry->caller = (void *)get_wchan(tsk);
		__entry->io_wait = tsk->in_iowait;
	),

	TP_printk("pid=%d iowait=%d caller=%pS", __entry->pid, __entry->io_wait, __entry->caller)
);

/*
 * Tracepoint for accounting runtime (time the task is executing
 * on a CPU).
 */
DECLARE_EVENT_CLASS(sched_stat_runtime,

	TP_PROTO(struct task_struct *tsk, u64 runtime, u64 vruntime),

	TP_ARGS(tsk, __perf_count(runtime), vruntime),

	TP_STRUCT__entry(
		__array( char,	comm,	TASK_COMM_LEN	)
		__field( pid_t,	pid			)
		__field( u64,	runtime			)
		__field( u64,	vruntime			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid		= tsk->pid;
		__entry->runtime	= runtime;
		__entry->vruntime	= vruntime;
	),

	TP_printk("comm=%s pid=%d runtime=%Lu [ns] vruntime=%Lu [ns]",
			__entry->comm, __entry->pid,
			(unsigned long long)__entry->runtime,
			(unsigned long long)__entry->vruntime)
);

DEFINE_EVENT(sched_stat_runtime, sched_stat_runtime,
	     TP_PROTO(struct task_struct *tsk, u64 runtime, u64 vruntime),
	     TP_ARGS(tsk, runtime, vruntime));

/*
 * Tracepoint for showing priority inheritance modifying a tasks
 * priority.
 */
TRACE_EVENT(sched_pi_setprio,

	TP_PROTO(struct task_struct *tsk, struct task_struct *pi_task),

	TP_ARGS(tsk, pi_task),

	TP_STRUCT__entry(
		__array( char,	comm,	TASK_COMM_LEN	)
		__field( pid_t,	pid			)
		__field( int,	oldprio			)
		__field( int,	newprio			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid		= tsk->pid;
		__entry->oldprio	= tsk->prio;
		__entry->newprio	= pi_task ?
				min(tsk->normal_prio, pi_task->prio) :
				tsk->normal_prio;
		/* XXX SCHED_DEADLINE bits missing */
	),

	TP_printk("comm=%s pid=%d oldprio=%d newprio=%d",
			__entry->comm, __entry->pid,
			__entry->oldprio, __entry->newprio)
);

#ifdef CONFIG_DETECT_HUNG_TASK
TRACE_EVENT(sched_process_hang,
	TP_PROTO(struct task_struct *tsk),
	TP_ARGS(tsk),

	TP_STRUCT__entry(
		__array( char,	comm,	TASK_COMM_LEN	)
		__field( pid_t,	pid			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid = tsk->pid;
	),

	TP_printk("comm=%s pid=%d", __entry->comm, __entry->pid)
);
#endif /* CONFIG_DETECT_HUNG_TASK */

DECLARE_EVENT_CLASS(sched_move_task_template,

	TP_PROTO(struct task_struct *tsk, int src_cpu, int dst_cpu),

	TP_ARGS(tsk, src_cpu, dst_cpu),

	TP_STRUCT__entry(
		__field( pid_t,	pid			)
		__field( pid_t,	tgid			)
		__field( pid_t,	ngid			)
		__field( int,	src_cpu			)
		__field( int,	src_nid			)
		__field( int,	dst_cpu			)
		__field( int,	dst_nid			)
	),

	TP_fast_assign(
		__entry->pid		= task_pid_nr(tsk);
		__entry->tgid		= task_tgid_nr(tsk);
		__entry->ngid		= task_numa_group_id(tsk);
		__entry->src_cpu	= src_cpu;
		__entry->src_nid	= cpu_to_node(src_cpu);
		__entry->dst_cpu	= dst_cpu;
		__entry->dst_nid	= cpu_to_node(dst_cpu);
	),

	TP_printk("pid=%d tgid=%d ngid=%d src_cpu=%d src_nid=%d dst_cpu=%d dst_nid=%d",
			__entry->pid, __entry->tgid, __entry->ngid,
			__entry->src_cpu, __entry->src_nid,
			__entry->dst_cpu, __entry->dst_nid)
);

/*
 * Tracks migration of tasks from one runqueue to another. Can be used to
 * detect if automatic NUMA balancing is bouncing between nodes
 */
DEFINE_EVENT(sched_move_task_template, sched_move_numa,
	TP_PROTO(struct task_struct *tsk, int src_cpu, int dst_cpu),

	TP_ARGS(tsk, src_cpu, dst_cpu)
);

DEFINE_EVENT(sched_move_task_template, sched_stick_numa,
	TP_PROTO(struct task_struct *tsk, int src_cpu, int dst_cpu),

	TP_ARGS(tsk, src_cpu, dst_cpu)
);

TRACE_EVENT(sched_swap_numa,

	TP_PROTO(struct task_struct *src_tsk, int src_cpu,
		 struct task_struct *dst_tsk, int dst_cpu),

	TP_ARGS(src_tsk, src_cpu, dst_tsk, dst_cpu),

	TP_STRUCT__entry(
		__field( pid_t,	src_pid			)
		__field( pid_t,	src_tgid		)
		__field( pid_t,	src_ngid		)
		__field( int,	src_cpu			)
		__field( int,	src_nid			)
		__field( pid_t,	dst_pid			)
		__field( pid_t,	dst_tgid		)
		__field( pid_t,	dst_ngid		)
		__field( int,	dst_cpu			)
		__field( int,	dst_nid			)
	),

	TP_fast_assign(
		__entry->src_pid	= task_pid_nr(src_tsk);
		__entry->src_tgid	= task_tgid_nr(src_tsk);
		__entry->src_ngid	= task_numa_group_id(src_tsk);
		__entry->src_cpu	= src_cpu;
		__entry->src_nid	= cpu_to_node(src_cpu);
		__entry->dst_pid	= task_pid_nr(dst_tsk);
		__entry->dst_tgid	= task_tgid_nr(dst_tsk);
		__entry->dst_ngid	= task_numa_group_id(dst_tsk);
		__entry->dst_cpu	= dst_cpu;
		__entry->dst_nid	= cpu_to_node(dst_cpu);
	),

	TP_printk("src_pid=%d src_tgid=%d src_ngid=%d src_cpu=%d src_nid=%d dst_pid=%d dst_tgid=%d dst_ngid=%d dst_cpu=%d dst_nid=%d",
			__entry->src_pid, __entry->src_tgid, __entry->src_ngid,
			__entry->src_cpu, __entry->src_nid,
			__entry->dst_pid, __entry->dst_tgid, __entry->dst_ngid,
			__entry->dst_cpu, __entry->dst_nid)
);

/*
 * Tracepoint for waking a polling cpu without an IPI.
 */
TRACE_EVENT(sched_wake_idle_without_ipi,

	TP_PROTO(int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(	int,	cpu	)
	),

	TP_fast_assign(
		__entry->cpu	= cpu;
	),

	TP_printk("cpu=%d", __entry->cpu)
);

/*trace cfs task info in feec*/
TRACE_EVENT(sched_feec_task_info,

	TP_PROTO(struct task_struct *p, int prev_cpu, unsigned long task_util,
		 unsigned long uclamp_util, int boosted,
		 int latency_sensitive, int blocked),

	TP_ARGS(p, prev_cpu, task_util, uclamp_util, boosted, latency_sensitive, blocked),

	TP_STRUCT__entry(
		__array(char,	comm,	TASK_COMM_LEN)
		__field(pid_t,	pid)
		__field(int,	prev_cpu)
		__field(unsigned long, task_util)
		__field(unsigned long, uclamp_util)
		__field(int, boosted)
		__field(int, latency_sensitive)
		__field(int, blocked)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->prev_cpu		= prev_cpu;
		__entry->task_util		= task_util;
		__entry->uclamp_util		= uclamp_util;
		__entry->boosted		= boosted;
		__entry->latency_sensitive	= latency_sensitive;
		__entry->blocked		= blocked;
	),

	TP_printk("comm=%s pid=%d prev_cpu=%d util=%lu uclamp_util=%lu boosted=%d latency_sensitive=%d blocked=%d",
		__entry->comm, __entry->pid, __entry->prev_cpu, __entry->task_util, __entry->uclamp_util,
		__entry->boosted, __entry->latency_sensitive, __entry->blocked)
);

/*
 * trace cfs rq info
 */
TRACE_EVENT(sched_feec_rq_task_util,

	TP_PROTO(int cpu, struct task_struct *p, unsigned int idle, struct pd_cache *pd_cache,
		 unsigned long util, unsigned long spare_cap, unsigned long cpu_cap),

	TP_ARGS(cpu, p, idle, pd_cache, util, spare_cap, cpu_cap),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(unsigned int,	nr_running)
		__field(unsigned int,	idle)
		__field(unsigned long,	cfs_util_with_p)
		__array(char,   comm,   TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(unsigned long,	spare_cap)
		__field(unsigned long,	cpu_cfs_cap)
		__field(unsigned long,	capacity_orig)
		__field(unsigned long,	base_cfs_util)
		__field(unsigned long,	base_cfs_est)
		__field(unsigned long,	base_cfs_max)
		__field(unsigned long,	base_freq_util)
		__field(unsigned long,	base_nrg_util)
		__field(unsigned long,	base_irq_util)
		__field(unsigned long,	base_rt_util)
		__field(unsigned long,	base_dl_util)
		__field(unsigned long,	base_dl_bw)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->nr_running		= cpu_rq(cpu)->nr_running;
		__entry->idle			= idle;
		__entry->cfs_util_with_p	= util;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->spare_cap		= spare_cap;
		__entry->cpu_cfs_cap		= cpu_cap;
		__entry->capacity_orig		= capacity_orig_of(cpu);
		__entry->base_cfs_util		= pd_cache->util;
		__entry->base_cfs_est		= pd_cache->util_est;
		__entry->base_cfs_max		= pd_cache->util_cfs;
		__entry->base_freq_util		= pd_cache->freq_util;
		__entry->base_nrg_util		= pd_cache->nrg_util;
		__entry->base_irq_util		= pd_cache->util_irq;
		__entry->base_rt_util		= pd_cache->util_rt;
		__entry->base_dl_util		= pd_cache->util_dl;
		__entry->base_dl_bw		= pd_cache->bw_dl;
	),

	TP_printk("cpu=%d nr_running=%u idle =%d cfs_util_with_p=%lu comm=%s pid=%d spare_cap=%lu capacity_of=%lu capacity_orig=%lu base_cfs_util=%lu "
		  "base_cfs_est=%lu base_cfs_max=%lu base_freq_util=%lu base_nrg_util=%lu base_irq_util=%lu base_rt_util=%lu base_dl_util=%lu base_dl_bw=%lu",
		__entry->cpu, __entry->nr_running, __entry->idle, __entry->cfs_util_with_p, __entry->comm, __entry->pid, __entry->spare_cap,
		__entry->cpu_cfs_cap, __entry->capacity_orig, __entry->base_cfs_util, __entry->base_cfs_est, __entry->base_cfs_max,
		__entry->base_freq_util, __entry->base_nrg_util, __entry->base_irq_util, __entry->base_rt_util, __entry->base_dl_util, __entry->base_dl_bw)
);

/*
 * trace energy diff info
 */
TRACE_EVENT(sched_energy_diff,

	TP_PROTO(unsigned long pd_energy, unsigned long base_energy, unsigned long prev_delta,
		unsigned long curr_delta, unsigned long best_delta, int prev_cpu,
		int best_energy_cpu, int max_spare_cap_cpu),

	TP_ARGS(pd_energy, base_energy, prev_delta, curr_delta, best_delta, prev_cpu, best_energy_cpu, max_spare_cap_cpu),

	TP_STRUCT__entry(
		__field(unsigned long,	pd_energy)
		__field(unsigned long,	base_energy)
		__field(unsigned long,	prev_delta)
		__field(unsigned long,	curr_delta)
		__field(unsigned long,	best_delta)
		__field(int,		prev_cpu)
		__field(int,		best_energy_cpu)
		__field(int,		max_spare_cap_cpu)
	),

	TP_fast_assign(
		__entry->pd_energy		= pd_energy;
		__entry->base_energy		= base_energy;
		__entry->prev_delta		= prev_delta;
		__entry->curr_delta		= curr_delta;
		__entry->best_delta		= best_delta;
		__entry->prev_cpu		= prev_cpu;
		__entry->best_energy_cpu	= best_energy_cpu;
		__entry->max_spare_cap_cpu	= max_spare_cap_cpu;
	),

	TP_printk("pd_eng=%lu base_eng=%lu p_delta=%lu c_delta=%lu b_delta=%lu prev_cpu=%d best_eng_cpu=%d max_spare_cpu=%d",
		  __entry->pd_energy, __entry->base_energy, __entry->prev_delta, __entry->curr_delta, __entry->best_delta,
		  __entry->prev_cpu, __entry->best_energy_cpu, __entry->max_spare_cap_cpu)
);

/*trace feec candidates */
TRACE_EVENT(sched_feec_candidates,

	TP_PROTO(int prev_cpu, int best_energy_cpu, unsigned long base_energy, unsigned long prev_delta,
		 unsigned long best_delta, int best_idle_cpu, int max_spare_cap_cpu_ls),

	TP_ARGS(prev_cpu, best_energy_cpu, base_energy, prev_delta, best_delta, best_idle_cpu, max_spare_cap_cpu_ls),

	TP_STRUCT__entry(
		__field(int,		prev_cpu)
		__field(int,		best_energy_cpu)
		__field(unsigned long,	base_energy)
		__field(unsigned long,	prev_delta)
		__field(unsigned long,	best_delta)
		__field(unsigned long,	threshold)
		__field(int,		best_idle_cpu)
		__field(int,		max_spare_cap_cpu_ls)
	),

	TP_fast_assign(
		__entry->prev_cpu		= prev_cpu;
		__entry->best_energy_cpu	= best_energy_cpu;
		__entry->best_idle_cpu		= best_idle_cpu;
		__entry->base_energy		= base_energy;
		__entry->prev_delta		= prev_delta;
		__entry->best_delta		= best_delta;
		__entry->threshold		= prev_delta == ULONG_MAX ? 0 : ((prev_delta + base_energy) >> 4);
		__entry->max_spare_cap_cpu_ls	= max_spare_cap_cpu_ls;
	),

	TP_printk("prev_cpu=%d best_eng_cpu=%d base_eng=%lu p_delta=%lu b_delta=%lu threshold=%lu best_idle_cpu=%d max_spare_cpu_ls=%d",
		  __entry->prev_cpu, __entry->best_energy_cpu, __entry->base_energy,
		  __entry->prev_delta, __entry->best_delta, __entry->threshold,
		  __entry->best_idle_cpu, __entry->max_spare_cap_cpu_ls)
);

/*
 * trace load balance info: trace sched group load balance stats info
 */
TRACE_EVENT(sched_load_balance_sg_stats,

	TP_PROTO(unsigned long sg_cpus, int group_type, unsigned int idle_cpus, unsigned int sum_nr_running,
		 unsigned long group_load, unsigned long group_capacity, unsigned long group_util, int group_no_capacity,
		 unsigned long load_per_task, unsigned long misfit_load, unsigned long busiest),

	TP_ARGS(sg_cpus, group_type, idle_cpus, sum_nr_running, group_load, group_capacity,
	    group_util, group_no_capacity, load_per_task, misfit_load, busiest),

	TP_STRUCT__entry(
		__field(unsigned long,	group_mask)
		__field(int,		group_type)
		__field(unsigned int,	group_idle_cpus)
		__field(unsigned int,	sum_nr_running)
		__field(unsigned long,	group_load)
		__field(unsigned long,	group_capacity)
		__field(unsigned long,	group_util)
		__field(int,		group_no_capacity)
		__field(unsigned long,	load_per_task)
		__field(unsigned long,	misfit_task_load)
		__field(unsigned long,	busiest)
	),

	TP_fast_assign(
		__entry->group_mask		= sg_cpus;
		__entry->group_type		= group_type;
		__entry->group_idle_cpus	= idle_cpus;
		__entry->sum_nr_running		= sum_nr_running;
		__entry->group_load		= group_load;
		__entry->group_capacity		= group_capacity;
		__entry->group_util		= group_util;
		__entry->group_no_capacity	= group_no_capacity;
		__entry->load_per_task		= load_per_task;
		__entry->misfit_task_load	= misfit_load;
		__entry->busiest		= busiest;
	),

	TP_printk("sched_group=%#lx type=%d idle_cpus=%u sum_nr_run=%u group_load=%lu capacity=%lu util=%lu no_capacity=%d lpt=%lu misfit_tload=%lu busiest_group=%#lx",
		  __entry->group_mask, __entry->group_type, __entry->group_idle_cpus, __entry->sum_nr_running, __entry->group_load, __entry->group_capacity, __entry->group_util,
		  __entry->group_no_capacity, __entry->load_per_task, __entry->misfit_task_load, __entry->busiest)
);

TRACE_EVENT(sched_load_balance_stats,

	TP_PROTO(unsigned long busiest, int bgroup_type, unsigned long bavg_load, unsigned long bload_per_task, unsigned long local,
		 int lgroup_type, unsigned long lavg_load, unsigned long lload_per_task, unsigned long sds_avg_load, unsigned long imbalance),

	TP_ARGS(busiest, bgroup_type, bavg_load, bload_per_task, local, lgroup_type, lavg_load, lload_per_task, sds_avg_load, imbalance),

	TP_STRUCT__entry(
		__field(unsigned long,	busiest)
		__field(int,		bgp_type)
		__field(unsigned long,	bavg_load)
		__field(unsigned long,	blpt)
		__field(unsigned long,	local)
		__field(int,		lgp_type)
		__field(unsigned long,	lavg_load)
		__field(unsigned long,	llpt)
		__field(unsigned long,	sds_avg)
		__field(unsigned long,	imbalance)
	),

	TP_fast_assign(
		__entry->busiest		= busiest;
		__entry->bgp_type		= bgroup_type;
		__entry->bavg_load		= bavg_load;
		__entry->blpt			= bload_per_task;
		__entry->bgp_type		= bgroup_type;
		__entry->local			= local;
		__entry->lgp_type		= lgroup_type;
		__entry->lavg_load		= lavg_load;
		__entry->llpt			= lload_per_task;
		__entry->sds_avg		= sds_avg_load;
		__entry->imbalance		= imbalance;
	),

	TP_printk("busiest_group=%#lx busiest_type=%d busiest_avg_load=%ld busiest_lpt=%ld local_group=%#lx local_type=%d local_avg_load=%ld local_lpt=%ld domain_avg_load=%ld imbalance=%ld",
		  __entry->busiest, __entry->bgp_type, __entry->bavg_load, __entry->blpt, __entry->local, __entry->lgp_type, __entry->lavg_load, __entry->llpt, __entry->sds_avg, __entry->imbalance)
);

TRACE_EVENT(sched_update_sg_lb,

	TP_PROTO(unsigned long avg_load, unsigned long group_load,
		unsigned long group_capacity,
		int group_no_capacity, int group_weight, int group_type),

	TP_ARGS(avg_load, group_load, group_capacity,
		group_no_capacity, group_weight, group_type),

	TP_STRUCT__entry(
		__field(unsigned long, avg_load)
		__field(unsigned long, group_load)
		__field(unsigned long, group_capacity)
		__field(int, group_no_capacity)
		__field(int, group_weight)
		__field(int, group_type)
	),

	TP_fast_assign(
		__entry->avg_load       = avg_load;
		__entry->group_load     = group_load;
		__entry->group_capacity = group_capacity;
		__entry->group_no_capacity = group_no_capacity;
		__entry->group_weight = group_weight;
		__entry->group_type = group_type;
	),

	TP_printk("avg_load=%lu group_load=%lu group_capacity=%lu group_no_capacity=%d weight=%d group_type=%d",
		__entry->avg_load, __entry->group_load, __entry->group_capacity,
		__entry->group_no_capacity, __entry->group_weight, __entry->group_type)
);

TRACE_EVENT(sched_load_balance_skip_tasks,

	TP_PROTO(int scpu, int dcpu, int grp_type, int pid,
		unsigned long h_load, unsigned long task_util,
		unsigned long affinity),

	TP_ARGS(scpu, dcpu, grp_type, pid, h_load, task_util, affinity),

	TP_STRUCT__entry(
		__field(int,            scpu)
		__field(int,            grp_type)
		__field(int,            dcpu)
		__field(int,            pid)
		__field(unsigned long,  affinity)
		__field(unsigned long,  task_util)
		__field(unsigned long,  h_load)
	),

	TP_fast_assign(
		__entry->scpu		= scpu;
		__entry->grp_type	= grp_type;
		__entry->dcpu		= dcpu;
		__entry->pid		= pid;
		__entry->affinity	= affinity;
		__entry->task_util	= task_util;
		__entry->h_load		= h_load;
	),

	TP_printk("src_cpu=%d group_type=%d dst_cpu=%d pid=%d affinity=%#lx task_util=%lu task_h_load=%lu",
		__entry->scpu, __entry->grp_type, __entry->dcpu, __entry->pid,
		__entry->affinity, __entry->task_util, __entry->h_load)
);

TRACE_EVENT(sched_load_balance,

	TP_PROTO(int cpu, enum cpu_idle_type idle, int balance,
		unsigned long group_mask, int busiest_nr_running,
		unsigned long imbalance, unsigned int env_flags, int ld_moved,
		unsigned int balance_interval, int active_balance,
		int overutilized),

	TP_ARGS(cpu, idle, balance, group_mask, busiest_nr_running,
		imbalance, env_flags, ld_moved, balance_interval,
		active_balance, overutilized),

	TP_STRUCT__entry(
		__field(int,			cpu)
		__field(enum cpu_idle_type,	idle)
		__field(int,			balance)
		__field(unsigned long,		group_mask)
		__field(int,			busiest_nr_running)
		__field(unsigned long,		imbalance)
		__field(unsigned int,		env_flags)
		__field(int,			ld_moved)
		__field(unsigned int,		balance_interval)
		__field(int,			active_balance)
		__field(int,			overutilized)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->idle			= idle;
		__entry->balance		= balance;
		__entry->group_mask		= group_mask;
		__entry->busiest_nr_running	= busiest_nr_running;
		__entry->imbalance		= imbalance;
		__entry->env_flags		= env_flags;
		__entry->ld_moved		= ld_moved;
		__entry->balance_interval	= balance_interval;
		__entry->active_balance		= active_balance;
		__entry->overutilized		= overutilized;
	),

	TP_printk("cpu=%d state=%s balance=%d group=%#lx busy_nr=%d imbalance=%ld flags=%#x ld_moved=%d interval=%d active_balance=%d overutilized=%d",
		__entry->cpu, __entry->idle == CPU_IDLE ? "idle" :
		(__entry->idle == CPU_NEWLY_IDLE ? "newly_idle" : "busy"),
		__entry->balance, __entry->group_mask, __entry->busiest_nr_running,
		__entry->imbalance, __entry->env_flags, __entry->ld_moved,
		__entry->balance_interval, __entry->active_balance,
		__entry->overutilized)
);

TRACE_EVENT(sched_load_balance_nohz_kick,

	TP_PROTO(int cpu, int kick_cpu),

	TP_ARGS(cpu, kick_cpu),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(unsigned int,	cpu_nr)
		__field(unsigned long,	misfit_task_load)
		__field(int,		kick_cpu)
		__field(unsigned long,	nohz_flags)
	),

	TP_fast_assign(
		__entry->cpu		  = cpu;
		__entry->cpu_nr		  = cpu_rq(cpu)->nr_running;
		__entry->misfit_task_load = cpu_rq(cpu)->misfit_task_load;
		__entry->kick_cpu	  = kick_cpu;
		__entry->nohz_flags	  = atomic_read(nohz_flags(kick_cpu));
	),

	TP_printk("cpu=%d nr_run=%u misfit_task_load=%lu kick_cpu=%d nohz_flags=0x%lx",
		__entry->cpu, __entry->cpu_nr, __entry->misfit_task_load,
		__entry->kick_cpu, __entry->nohz_flags)
);
#ifdef CONFIG_SPRD_ROTATION_TASK
/* task_rotation info */
TRACE_EVENT(sched_task_rotation,

	TP_PROTO(int src_cpu, int dst_cpu, int src_pid, int dst_pid),

	TP_ARGS(src_cpu, dst_cpu, src_pid, dst_pid),

	TP_STRUCT__entry(
		__field(int,	src_cpu)
		__field(int,	dst_cpu)
		__field(int,	src_pid)
		__field(int,	dst_pid)
	),

	TP_fast_assign(
		__entry->src_cpu = src_cpu;
		__entry->dst_cpu = dst_cpu;
		__entry->src_pid = src_pid;
		__entry->dst_pid = dst_pid;
	),

	TP_printk("src_cpu=%d dst_cpu=%d src_pid=%d dst_pid=%d",
		__entry->src_cpu, __entry->dst_cpu,
		__entry->src_pid, __entry->dst_pid
	)
);
#endif
/*
 * Following tracepoints are not exported in tracefs and provide hooking
 * mechanisms only for testing and debugging purposes.
 *
 * Postfixed with _tp to make them easily identifiable in the code.
 */
DECLARE_TRACE(pelt_cfs_tp,
	TP_PROTO(struct cfs_rq *cfs_rq),
	TP_ARGS(cfs_rq));

DECLARE_TRACE(pelt_rt_tp,
	TP_PROTO(struct rq *rq),
	TP_ARGS(rq));

DECLARE_TRACE(pelt_dl_tp,
	TP_PROTO(struct rq *rq),
	TP_ARGS(rq));

DECLARE_TRACE(pelt_irq_tp,
	TP_PROTO(struct rq *rq),
	TP_ARGS(rq));

DECLARE_TRACE(pelt_se_tp,
	TP_PROTO(struct sched_entity *se),
	TP_ARGS(se));

DECLARE_TRACE(sched_overutilized_tp,
	TP_PROTO(struct root_domain *rd, bool overutilized),
	TP_ARGS(rd, overutilized));

DECLARE_TRACE(sched_util_est_cfs_tp,
	TP_PROTO(struct cfs_rq *cfs_rq),
	TP_ARGS(cfs_rq));

DECLARE_TRACE(sched_util_est_se_tp,
	TP_PROTO(struct sched_entity *se),
	TP_ARGS(se));

#endif /* _TRACE_SCHED_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
