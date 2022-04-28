// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2022, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Unisoc, Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM unisoc_sched

#if !defined(_TRACE_WALT_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_WALT_H

#include <linux/tracepoint.h>

#include "walt.h"

TRACE_EVENT(walt_update_task_ravg,

	TP_PROTO(struct task_struct *p, struct rq *rq,
		 struct walt_task_ravg *wtr, struct walt_rq *wrq,
		 int evt, u64 wallclock, u64 irqtime),

	TP_ARGS(p, rq, wtr, wrq, evt, wallclock, irqtime),

	TP_STRUCT__entry(
		__array(char,	comm,	TASK_COMM_LEN)
		__field(pid_t,	pid)
		__field(pid_t,	cur_pid)
		__field(u64,	wallclock)
		__field(u64,	mark_start)
		__field(u64,	delta_m)
		__field(u64,	win_start)
		__field(u64,	delta)
		__field(u64,	irqtime)
		__array(char,   evt, 16)
		__field(u32,	demand)
		__field(u32,	demand_scale)
		__field(u32,	sum)
		__field(int,	cpu)
		__field(u64,	cs)
		__field(u64,	ps)
		__field(u32,	curr_window)
		__field(u32,	prev_window)
		__field(u64,	nt_cs)
		__field(u64,	nt_ps)
		__field(u32,	active_windows)
	),

	TP_fast_assign(
			static const char *walt_event_names[] = {
				"PUT_PREV_TASK",
				"PICK_NEXT_TASK",
				"TASK_WAKE",
				"TASK_MIGRATE",
				"TASK_UPDATE",
				"IRQ_UPDATE"
			};
		__entry->wallclock	= wallclock;
		__entry->win_start	= wrq->window_start;
		__entry->delta		= (wallclock - wrq->window_start);
		strcpy(__entry->evt, walt_event_names[evt]);
		__entry->cpu		= rq->cpu;
		__entry->cur_pid	= rq->curr->pid;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->mark_start	= wtr->mark_start;
		__entry->delta_m	= (wallclock - wtr->mark_start);
		__entry->demand		= wtr->demand;
		__entry->demand_scale	= wtr->demand_scale;
		__entry->sum		= wtr->sum;
		__entry->irqtime	= irqtime;
		__entry->cs		= wrq->curr_runnable_sum;
		__entry->ps		= wrq->prev_runnable_sum;
		__entry->curr_window	= wtr->curr_window;
		__entry->prev_window	= wtr->prev_window;
	),

	TP_printk("wallclock=%llu window_start=%llu delta=%llu event=%s cpu=%d"
		" cur_pid=%d pid=%d comm=%s walt_util=%d mark_start=%llu delta=%llu"
		" demand=%u  sum=%u irqtime=%llu  curr_runnable_sum=%llu"
		" prev_runnable_sum=%llu cur_window=%u prev_window=%u",
		__entry->wallclock, __entry->win_start, __entry->delta,
		__entry->evt, __entry->cpu, __entry->cur_pid,
		__entry->pid, __entry->comm, __entry->demand_scale,
		__entry->mark_start, __entry->delta_m, __entry->demand,
		__entry->sum, __entry->irqtime,	__entry->cs, __entry->ps,
		__entry->curr_window, __entry->prev_window
		)
);

TRACE_EVENT(walt_update_history,

	TP_PROTO(struct rq *rq, struct task_struct *p,
		 struct walt_task_ravg *wtr, u32 runtime,
		 int samples, int evt),

	TP_ARGS(rq, p, wtr, runtime, samples, evt),

	TP_STRUCT__entry(
		__array(char,		comm,	TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(unsigned int,	runtime)
		__field(int,		samples)
		__field(int,		evt)
		__field(u64,		demand)
		__array(unsigned int,	hist, RAVG_HIST_SIZE_MAX)
		__field(int,		cpu)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid            = p->pid;
		__entry->runtime        = runtime;
		__entry->samples        = samples;
		__entry->evt            = evt;
		__entry->demand         = wtr->demand;
		memcpy(__entry->hist, wtr->sum_history,
					RAVG_HIST_SIZE_MAX * sizeof(u32));
		__entry->cpu            = rq->cpu;
	),

	TP_printk("pid=%d comm=%s runtime=%u samples=%d event=%d demand=%llu"
		" cpu=%d hist0-5=%u %u %u %u %u %u",
		__entry->pid, __entry->comm,
		__entry->runtime, __entry->samples, __entry->evt,
		__entry->demand, __entry->cpu,
		__entry->hist[0], __entry->hist[1],
		__entry->hist[2], __entry->hist[3],
		__entry->hist[4], __entry->hist[5])
);

TRACE_EVENT(walt_migration_update_sum,

	TP_PROTO(struct rq *rq, struct walt_rq *wrq, struct task_struct *p),

	TP_ARGS(rq, wrq, p),

	TP_STRUCT__entry(
		__field(int,	cpu)
		__field(int,	pid)
		__field(u64,	cs)
		__field(u64,	ps)
	),

	TP_fast_assign(
		__entry->cpu		= cpu_of(rq);
		__entry->cs		= wrq->curr_runnable_sum;
		__entry->ps		= wrq->prev_runnable_sum;
		__entry->pid		= p->pid;
	),

	TP_printk("cpu=%d curr_runnable_sum=%llu prev_runnable_sum=%llu pid=%d",
		  __entry->cpu, __entry->cs, __entry->ps, __entry->pid)
);
#endif /* _TRACE_WALT_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/unisoc_platform/sched
#define TRACE_INCLUDE_FILE trace

#include <trace/define_trace.h>
