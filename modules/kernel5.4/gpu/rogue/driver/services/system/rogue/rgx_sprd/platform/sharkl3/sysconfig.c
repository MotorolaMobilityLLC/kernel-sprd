/*************************************************************************/ /*!
@File
@Title          System Configuration
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    System Configuration functions
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include "interrupt_support.h"
#include "pvrsrv_device.h"
#include "syscommon.h"
#if defined(SUPPORT_PVRSRV_GPUVIRT)
#include "vz_vmm_pvz.h"
#endif
#include "sysinfo.h"
#include "sysconfig.h"
#include "physheap.h"
#if defined(SUPPORT_ION)
#include "ion_support.h"
#endif
#include "sprd_init.h"
#include <linux/dma-mapping.h>
#include "rgxtimecorr.h"
#include "rgxdebug.h"

static RGX_TIMING_INFORMATION	gsRGXTimingInfo;
static RGX_DATA					gsRGXData;
static PVRSRV_DEVICE_CONFIG 	gsDevices[1];
static PHYS_HEAP_FUNCTIONS		gsPhysHeapFuncs;

#if defined(SUPPORT_PVRSRV_GPUVIRT)
static PHYS_HEAP_CONFIG			gsPhysHeapConfig[2];
#else
static PHYS_HEAP_CONFIG			gsPhysHeapConfig[1];
#endif

/*
	CPU to Device physical address translation
*/
static
void UMAPhysHeapCpuPAddrToDevPAddr(IMG_HANDLE hPrivData,
								   IMG_UINT32 ui32NumOfAddr,
								   IMG_DEV_PHYADDR *psDevPAddr,
								   IMG_CPU_PHYADDR *psCpuPAddr)
{
	PVR_UNREFERENCED_PARAMETER(hPrivData);

	/* Optimise common case */
	psDevPAddr[0].uiAddr = psCpuPAddr[0].uiAddr;
	if (ui32NumOfAddr > 1)
	{
		IMG_UINT32 ui32Idx;
		for (ui32Idx = 1; ui32Idx < ui32NumOfAddr; ++ui32Idx)
		{
			psDevPAddr[ui32Idx].uiAddr = psCpuPAddr[ui32Idx].uiAddr;
		}
	}
}

/*
	Device to CPU physical address translation
*/
static
void UMAPhysHeapDevPAddrToCpuPAddr(IMG_HANDLE hPrivData,
								   IMG_UINT32 ui32NumOfAddr,
								   IMG_CPU_PHYADDR *psCpuPAddr,
								   IMG_DEV_PHYADDR *psDevPAddr)
{
	PVR_UNREFERENCED_PARAMETER(hPrivData);

	/* Optimise common case */
	psCpuPAddr[0].uiAddr = psDevPAddr[0].uiAddr;
	if (ui32NumOfAddr > 1)
	{
		IMG_UINT32 ui32Idx;
		for (ui32Idx = 1; ui32Idx < ui32NumOfAddr; ++ui32Idx)
		{
			psCpuPAddr[ui32Idx].uiAddr = psDevPAddr[ui32Idx].uiAddr;
		}
	}
}

PVRSRV_ERROR SysDevInit(void *pvOSDevice, PVRSRV_DEVICE_CONFIG **ppsDevConfig)
{
	IMG_UINT32 ui32NextPhysHeapID = 0;

	dma_set_mask(pvOSDevice, DMA_BIT_MASK(36));

	if (gsDevices[0].pvOSDevice)
	{
		return PVRSRV_ERROR_INVALID_DEVICE;
	}

	/* SPRD Init */
	RgxSprdInit(&gsDevices[0], &gsRGXTimingInfo, pvOSDevice);

	/*
	 * Setup information about physical memory heap(s) we have
	 */
	gsPhysHeapFuncs.pfnCpuPAddrToDevPAddr = UMAPhysHeapCpuPAddrToDevPAddr;
	gsPhysHeapFuncs.pfnDevPAddrToCpuPAddr = UMAPhysHeapDevPAddrToCpuPAddr;

	gsPhysHeapConfig[ui32NextPhysHeapID].pszPDumpMemspaceName = "SYSMEM";
	gsPhysHeapConfig[ui32NextPhysHeapID].eType = PHYS_HEAP_TYPE_UMA;
	gsPhysHeapConfig[ui32NextPhysHeapID].psMemFuncs = &gsPhysHeapFuncs;
	gsPhysHeapConfig[ui32NextPhysHeapID].hPrivData = NULL;
	gsPhysHeapConfig[ui32NextPhysHeapID].ui32UsageFlags = PHYS_HEAP_USAGE_GPU_LOCAL;
	ui32NextPhysHeapID += 1;

	/*
	 *Setup RGX specific data
	 */
	gsRGXData.psRGXTimingInfo = &gsRGXTimingInfo;

	/*
	 * Setup device
	 */
	gsDevices[0].pvOSDevice             = pvOSDevice;
	gsDevices[0].pszName                = "RGX";
	gsDevices[0].pszVersion             = NULL;

	/* Device's physical heaps */
	gsDevices[0].pasPhysHeaps = gsPhysHeapConfig;
	gsDevices[0].ui32PhysHeapCount = ARRAY_SIZE(gsPhysHeapConfig);
	gsDevices[0].eDefaultHeap = PVRSRV_PHYS_HEAP_GPU_LOCAL;
	/* No clock frequency either */
	gsDevices[0].pfnClockFreqGet        = NULL;

	gsDevices[0].hDevData               = &gsRGXData;
	gsDevices[0].bDevicePA0IsValid  = IMG_FALSE;

	/* Setup other system specific stuff */
#if defined(SUPPORT_ION)
	IonInit(NULL);
#endif

#if defined(SUPPORT_PVRSRV_GPUVIRT)
	PVR_ASSERT(ui32NextPhysHeapID < ARRAY_SIZE(gsPhysHeapConfig));

	/* Virtualization support services needs to know which heap ID corresponds to FW */
	gsDevices[0].pasPhysHeaps[PVRSRV_PHYS_HEAP_FW_LOCAL] = ui32NextPhysHeapID;
	gsPhysHeapConfig[ui32NextPhysHeapID].pszPDumpMemspaceName = "SYSMEM";
	gsPhysHeapConfig[ui32NextPhysHeapID].eType = PHYS_HEAP_TYPE_UMA;
	gsPhysHeapConfig[ui32NextPhysHeapID].psMemFuncs = &gsPhysHeapFuncs;
	gsPhysHeapConfig[ui32NextPhysHeapID].hPrivData = NULL;
	gsPhysHeapConfig[ui32NextPhysHeapID].ui32UsageFlags = PHYS_HEAP_USAGE_FW_LOCAL;

	SysVzDevInit(&gsDevices[0]);
#endif

	*ppsDevConfig = &gsDevices[0];

	return PVRSRV_OK;
}

void SysDevDeInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
#if defined(SUPPORT_PVRSRV_GPUVIRT)
	SysVzDevDeInit(psDevConfig);
#endif
	/* sprd DeInit */
	RgxSprdDeInit();

#if defined(SUPPORT_ION)
	IonDeinit();
#endif

	psDevConfig->pvOSDevice = NULL;
}

PVRSRV_ERROR SysInstallDeviceLISR(IMG_HANDLE hSysData,
								  IMG_UINT32 ui32IRQ,
								  const IMG_CHAR *pszName,
								  PFN_LISR pfnLISR,
								  void *pvData,
								  IMG_HANDLE *phLISRData)
{
	IMG_UINT32 ui32IRQFlags = SYS_IRQ_FLAG_TRIGGER_DEFAULT;

	PVR_UNREFERENCED_PARAMETER(hSysData);

#if defined(PVRSRV_GPUVIRT_MULTIDRV_MODEL)
	ui32IRQFlags |= SYS_IRQ_FLAG_SHARED;
#endif

	return OSInstallSystemLISR(phLISRData, ui32IRQ, pszName, pfnLISR, pvData,
							   ui32IRQFlags);
}

PVRSRV_ERROR SysUninstallDeviceLISR(IMG_HANDLE hLISRData)
{
	return OSUninstallSystemLISR(hLISRData);
}

PVRSRV_ERROR SysDebugInfo(PVRSRV_DEVICE_CONFIG *psDevConfig,
				DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
				void *pvDumpDebugFile)
{
#if defined(NO_HARDWARE)
	PVR_UNREFERENCED_PARAMETER(psDevConfig);
	PVR_UNREFERENCED_PARAMETER(pfnDumpDebugPrintf);
	PVR_UNREFERENCED_PARAMETER(pvDumpDebugFile);
	return PVRSRV_OK;
#else
	PVRSRV_ERROR            eError;
	IMG_CHAR *pszName;

	pszName = psDevConfig->pszName;
	PVR_DUMPDEBUG_LOG("------[ rgx_GPU %s Cur_FREQ and PMU_REG_STATE ]------",pszName);
	eError = RGXDebugPrintGpuCurrentFreq(psDevConfig, pfnDumpDebugPrintf, pvDumpDebugFile);
	CheckGpuPowClkState(psDevConfig->psDevNode, pfnDumpDebugPrintf, pvDumpDebugFile);

	return eError;

#endif
}


/******************************************************************************
 End of file (sysconfig.c)
******************************************************************************/
