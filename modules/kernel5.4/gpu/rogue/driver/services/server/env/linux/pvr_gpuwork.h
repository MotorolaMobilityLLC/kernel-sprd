/*************************************************************************/ /*!
@File           pvr_gpuwork.h
@Title          PVR GPU Work Period Tracepoint Implementation
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef _PVR_GPUWORK_H_
#define _PVR_GPUWORK_H_

#include "pvrsrv_error.h"
#include "img_types.h"
#include "img_defs.h"

typedef enum
{
	PVR_GPU_WORK_EVENT_START,
	PVR_GPU_WORK_EVENT_END,
} PVR_GPU_WORK_EVENT_TYPE;

struct pid *pvr_find_get_pid(pid_t nr);

PVRSRV_ERROR GpuTraceWorkPeriodInitialize(void);
void GpuTraceSupportDeInitialize(void);

PVRSRV_ERROR GpuTraceWorkPeriodEventStatsRegister(
		IMG_HANDLE *phGpuWorkPeriodEventStats);
void GpuTraceWorkPeriodEventStatsUnregister(
		IMG_HANDLE hGpuWorkPeriodEventStats);

void GpuTraceWorkPeriod(IMG_PID pid, IMG_UINT32 u32GpuId,
		IMG_UINT64 ui64HWTimestampInOSTime,
		IMG_UINT32 ui32IntJobRef,
		PVR_GPU_WORK_EVENT_TYPE eEventType);

#endif /* defined(_PVR_GPUWORK_H_)*/
