#undef TRACE_SYSTEM
#define TRACE_SYSTEM sprd_img_gpu

#if !defined(_TRACE_SPRD_IMG_GPU_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SPRD_IMG_GPU_H

#include <linux/tracepoint.h>
#include <linux/binfmts.h>

/*
 * Tracepoint for sprd test:
 */
TRACE_EVENT(sprd_gpu_devfreq,

	TP_PROTO(int arg1),

	TP_ARGS(arg1),

	TP_STRUCT__entry(
		__field(	int,	arg1		)
	),

	TP_fast_assign(
		__entry->arg1	= arg1;
	),

	TP_printk("sprd_gpu_devfreq is : %d ",__entry->arg1)
);

#endif /* _TRACE_SPRD_IMG_GPU_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE sprd_trace
#include <trace/define_trace.h>
