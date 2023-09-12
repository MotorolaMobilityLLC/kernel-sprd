/*************************************************************************/ /*!
@File
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/
#undef TRACE_SYSTEM
#define TRACE_SYSTEM power

#if !defined(TRACE_GPU_WORK_PERIOD_H) || defined(TRACE_HEADER_MULTI_READ)
#define TRACE_GPU_WORK_PERIOD_H

#include <linux/tracepoint.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
int PVRGpuTraceEnableWorkPeriodCallback(void);
#else
void PVRGpuTraceEnableWorkPeriodCallback(void);
#endif
void PVRGpuTraceDisableWorkPeriodCallback(void);

/*
 * The gpu_work_period event indicates that details of how much work the GPU
 * was performing for |uid| during the period.
 *
 * The event should be emitted for a period at most 1 second after
 * |end_time_ns| and must be emitted the event at most 2 seconds after
 * |end_time_ns|. A period's duration (|end_time_ns| - |start_time_ns|) must
 * be at most 1 second. The |total_active_duration_ns| value must be less than
 * or equal to the period duration (|end_time_ns| - |start_time_ns|).
 *
 * @gpu_id: A value that uniquely identifies the GPU within the system.
 *
 * @uid: The UID of the application that submitted work to the GPU.
 *
 * @start_time_ns: The start time of the period in nanoseconds.
 *
 * @end_time_ns: The end time of the period in nanoseconds.
 *
 * @total_active_duration_ns: The amount of time the GPU was running GPU work
 *                            for |uid| during the period
 *
 */
TRACE_EVENT_FN(gpu_work_period,

	TP_PROTO(unsigned int gpu_id, unsigned int uid, unsigned long start_time_ns,
	         unsigned long end_time_ns, unsigned long total_active_duration_ns),

	TP_ARGS(gpu_id, uid, start_time_ns, end_time_ns, total_active_duration_ns),

	TP_STRUCT__entry(
		__field(u32, gpu_id)
		__field(u32, uid)
		__field(u64, start_time_ns)
		__field(u64, end_time_ns)
		__field(u64, total_active_duration_ns)
	),

	TP_fast_assign(
		__entry->gpu_id = gpu_id;
		__entry->uid = uid;
		__entry->start_time_ns = start_time_ns;
		__entry->end_time_ns = end_time_ns;
		__entry->total_active_duration_ns = total_active_duration_ns;
	),

	TP_printk("gpu_id=%u uid=%u start_time_ns=%lu end_time_ns=%lu "
			"total_active_duration_ns=%lu",
		(unsigned int)__entry->gpu_id,
		(unsigned int)__entry->uid,
		(unsigned long)__entry->start_time_ns,
		(unsigned long)__entry->end_time_ns,
		(unsigned long)__entry->total_active_duration_ns),

	PVRGpuTraceEnableWorkPeriodCallback,
	PVRGpuTraceDisableWorkPeriodCallback
);

#endif /* TRACE_GPU_WORK_PERIOD_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .

/* This is needed because the name of this file doesn't match TRACE_SYSTEM. */
#define TRACE_INCLUDE_FILE gpu_work

/* This part must be outside protection */
#include <trace/define_trace.h>
