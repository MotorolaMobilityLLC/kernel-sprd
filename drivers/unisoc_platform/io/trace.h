/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2022, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022, Unisoc, Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM unisoc_io

#if !defined(_TRACE_UNISOC_IO_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_UNISOC_IO_H

#include <linux/tracepoint.h>

TRACE_EVENT(iolimit_write_control,
	TP_PROTO(unsigned long delta),

	TP_ARGS(delta),

	TP_STRUCT__entry(
		__field(pid_t, tgid)
		__field(pid_t, pid)
		__array(char,   comm,   TASK_COMM_LEN)
		__field(unsigned long, delta)
	),

	TP_fast_assign(
		__entry->tgid = current->tgid;
		__entry->pid  = current->pid;
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
		__entry->delta = delta * 1000 / HZ;
	),

	TP_printk("tgid:%d pid:%d comm=%s delta=%lu\n",
		__entry->tgid,
		__entry->pid,
		__entry->comm,
		__entry->delta
	)
);

#endif /* _TRACE_UNISOC_IO_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE trace

#include <trace/define_trace.h>
