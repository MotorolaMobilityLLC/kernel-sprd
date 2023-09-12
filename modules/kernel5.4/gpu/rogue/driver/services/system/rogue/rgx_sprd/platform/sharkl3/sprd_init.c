/*!
@Title          System Configuration
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
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
@Description    System Configuration functions
*/

#include <linux/io.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/mfd/syscon.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
#include <linux/mfd/syscon/sprd-glb.h>
#endif
#include <linux/semaphore.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include "rgxdevice.h"
#include "pvr_debug.h"
#include "img_types.h"
#include "sprd_init.h"
#include <linux/vmalloc.h>

#include <linux/smp.h>
#include <trace/events/power.h>
#include "rgxdebug.h"
#define VENDOR_FTRACE_MODULE_NAME    "unisoc-gpu"

//#define CREATE_TRACE_POINTS
//#include "sprd_trace.h"

#define SYS_RGX_ACTIVE_POWER_LATENCY_MS (15)
#define PM_RUNTIME_DELAY_MS (50)

#define DTS_CLK_OFFSET          3
#define FREQ_KHZ                1000

#if defined(SUPPORT_LINUX_DVFS) || defined(SUPPORT_PDVFS)
#define GPU_POLL_MS             50
#define GPU_UP_THRESHOLD        40
#define GPU_DOWN_DIFFERENTIAL   5
#endif

#define MOVE_BIT_LEFT(x,n) ((unsigned long)x << n)

#if defined(PVR_BOOST)
int gpu_boost_level = 0;
#endif

struct gpu_freq_info {
	struct clk* clk_src;
	int freq;    //kHz
	int volt;    //uV
	int div;
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
struct gpu_reg_info {
	struct regmap* regmap_ptr;
	uint32_t args[2];
};
#endif

struct gpu_dvfs_context {
	int gpu_clock_on;
	int gpu_power_on;

	int cur_voltage;	//uV

	struct clk*  clk_gpu_i;
	struct clk*  clk_gpu_core;
	struct clk*  clk_gpu_soc;
	struct clk** gpu_clk_src;
	int gpu_clk_num;

	struct gpu_freq_info* freq_list;
	int freq_list_len;

#if defined(SUPPORT_PDVFS)
	IMG_OPP  *pasOPPTable;
#endif

	const struct gpu_freq_info* freq_cur;

	const struct gpu_freq_info* freq_default;

	struct semaphore* sem;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
	struct regmap* pmu_apb_reg_base;
#else
	struct gpu_reg_info top_force_reg;
	struct gpu_reg_info core_force_reg;
	struct gpu_reg_info core_auto_reg;
	struct gpu_reg_info gpu_top_state_reg;
	struct gpu_reg_info gpu_core_state_reg;
	struct gpu_reg_info clk_gpu_eb_reg;
#endif
};

DEFINE_SEMAPHORE(gpu_dvfs_sem);
static struct gpu_dvfs_context gpu_dvfs_ctx=
{
	.gpu_clock_on=0,
	.gpu_power_on=0,

	.sem=&gpu_dvfs_sem,
};


u32 top_force = 0;
u32 core_force = 0;
u32 top_pwr = 0;
u32 core_pwr = 0;
u32 clk_gpu_eb_pwr = 0;

const u32 top_force_mask = MOVE_BIT_LEFT(1,25);
const u32 core_force_mask = MOVE_BIT_LEFT(1,25);
const u32 top_pwr_mask = MOVE_BIT_LEFT(0x1f,22);
const u32 core_pwr_mask = MOVE_BIT_LEFT(0x1f,5);
const u32 clk_gpu_eb_mask = MOVE_BIT_LEFT(0x1,0);

const u32 top_force_pwr_on = MOVE_BIT_LEFT(0,25);
const u32 core_force_pwr_on = MOVE_BIT_LEFT(0,25);
const u32 top_pwr_on = MOVE_BIT_LEFT(0,22);
const u32 top_pwr_ing = MOVE_BIT_LEFT(6,22);
const u32 core_pwr_ing =  MOVE_BIT_LEFT(0xf,5);
const u32 core_pwr_on = MOVE_BIT_LEFT(0,5);
const u32 clk_gpu_eb_on = MOVE_BIT_LEFT(0x1,0);

void CheckGpuPowClkState(PVRSRV_DEVICE_NODE *psDeviceNode,DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,void *pvDumpDebugFile);
void CheckGpuPowClkState(PVRSRV_DEVICE_NODE *psDeviceNode,DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                         void *pvDumpDebugFile )
{
	regmap_read(gpu_dvfs_ctx.top_force_reg.regmap_ptr, gpu_dvfs_ctx.top_force_reg.args[0], &top_force);
	regmap_read(gpu_dvfs_ctx.core_force_reg.regmap_ptr, gpu_dvfs_ctx.core_force_reg.args[0], &core_force);
	regmap_read(gpu_dvfs_ctx.gpu_top_state_reg.regmap_ptr, gpu_dvfs_ctx.gpu_top_state_reg.args[0], &top_pwr);
	regmap_read(gpu_dvfs_ctx.gpu_core_state_reg.regmap_ptr, gpu_dvfs_ctx.gpu_core_state_reg.args[0], &core_pwr);
	regmap_read(gpu_dvfs_ctx.clk_gpu_eb_reg.regmap_ptr, gpu_dvfs_ctx.clk_gpu_eb_reg.args[0], &clk_gpu_eb_pwr);

	top_force = top_force & top_force_mask;
	core_force = core_force & core_force_mask;
	top_pwr = top_pwr & top_pwr_mask;
	core_pwr = core_pwr & core_pwr_mask;
	clk_gpu_eb_pwr = clk_gpu_eb_pwr &clk_gpu_eb_mask;

	if ((gpu_dvfs_ctx.gpu_power_on == 1) && (gpu_dvfs_ctx.gpu_clock_on == 1))
	{
		if ( (top_force == top_force_pwr_on)  && (core_force == core_force_pwr_on))
		{
			if ((top_pwr == top_pwr_on) && (core_pwr == core_pwr_on))
			{
			PVR_DUMPDEBUG_LOG("gpu_top and gpu_core pwr_on is enabled.");

			}

		}
		else
		{
			PVR_DUMPDEBUG_LOG("gpu_top or gpu_core pwr_on not enabled.");
		}
	}
	else
	{
		PVR_DUMPDEBUG_LOG("GPU power is off now!!!");
	}

	PVR_DUMPDEBUG_LOG("RGX GPU REG STATE:top_force[0x%x], core_force[0x%x], top_pwr[0x%x], core_pwr[0x%x], clk_gpu_eb_pwr[0x%x]",top_force,core_force,top_pwr,core_pwr,clk_gpu_eb_pwr);
}




#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
static int pmu_glb_set(unsigned long reg, u32 bit)
{
	int value;
	//base:0xE42B000
	regmap_read(gpu_dvfs_ctx.pmu_apb_reg_base, reg , &value);
	value = value | bit;
	regmap_write(gpu_dvfs_ctx.pmu_apb_reg_base, reg, value);
	return 0;
}

static int pmu_glb_clr(unsigned long reg, u32 bit)
{
	int value;
	//base:0xE42B000
	regmap_read(gpu_dvfs_ctx.pmu_apb_reg_base, reg , &value);
	value = value & ~bit;
	regmap_write(gpu_dvfs_ctx.pmu_apb_reg_base, reg, value);
	return 0;
}
#endif

static PVRSRV_ERROR RgxDeviceInit(PVRSRV_DEVICE_CONFIG* psDevConfig, struct platform_device *pDevice)
{
	PVRSRV_ERROR result = PVRSRV_OK;
	struct resource *reg_res = NULL;
	struct resource *irq_res = NULL;

	//the first memory resource is the physical address of the GPU registers
	reg_res = platform_get_resource(pDevice, IORESOURCE_MEM, 0);
	if (!reg_res) {
		PVR_DPF((PVR_DBG_ERROR, "RgxDeviceInit No MEM resource"));
		result = PVRSRV_ERROR_INIT_FAILURE;
		return (result);
	}

	/* Device setup information */
	psDevConfig->sRegsCpuPBase.uiAddr = reg_res->start;
	psDevConfig->ui32RegsSize = resource_size(reg_res);

	//init irq
	irq_res = platform_get_resource(pDevice, IORESOURCE_IRQ, 0);
	if (!irq_res) {
		PVR_DPF((PVR_DBG_ERROR, "RgxDeviceInit No IRQ resource"));
		result = PVRSRV_ERROR_INIT_FAILURE;
		return (result);
	}
	psDevConfig->ui32IRQ            = irq_res->start;
	psDevConfig->eCacheSnoopingMode = PVRSRV_DEVICE_SNOOP_NONE;

	return (result);
}

#if defined(SUPPORT_PDVFS)
static void FillOppTable(void)
{
	int i = 0;

	gpu_dvfs_ctx.pasOPPTable= vmalloc(sizeof(IMG_OPP) * gpu_dvfs_ctx.freq_list_len);
	PVR_ASSERT(NULL != gpu_dvfs_ctx.pasOPPTable);

	for(i=0; i<gpu_dvfs_ctx.freq_list_len; i++)
	{
		gpu_dvfs_ctx.pasOPPTable[i].ui32Freq = gpu_dvfs_ctx.freq_list[i].freq * FREQ_KHZ;
		gpu_dvfs_ctx.pasOPPTable[i].ui32Volt = gpu_dvfs_ctx.freq_list[i].volt;
	}
}
#endif

static void RgxFreqInit(struct device *dev)
{
	int i = 0, clk_cnt = 0;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
	gpu_dvfs_ctx.pmu_apb_reg_base = syscon_regmap_lookup_by_phandle(dev->of_node,"sprd,syscon-pmu-apb");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.pmu_apb_reg_base);
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	gpu_dvfs_ctx.top_force_reg.regmap_ptr = syscon_regmap_lookup_by_name(dev->of_node,"top_force_shutdown");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.top_force_reg.regmap_ptr);
	syscon_get_args_by_name(dev->of_node,"top_force_shutdown", 2, (uint32_t *)gpu_dvfs_ctx.top_force_reg.args);

	gpu_dvfs_ctx.core_force_reg.regmap_ptr = syscon_regmap_lookup_by_name(dev->of_node,"core_force_shutdown");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.core_force_reg.regmap_ptr);
	syscon_get_args_by_name(dev->of_node,"core_force_shutdown", 2, (uint32_t *)gpu_dvfs_ctx.core_force_reg.args);

	gpu_dvfs_ctx.core_auto_reg.regmap_ptr = syscon_regmap_lookup_by_name(dev->of_node,"core_auto_shutdown");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.core_auto_reg.regmap_ptr);
	syscon_get_args_by_name(dev->of_node,"core_auto_shutdown", 2, (uint32_t *)gpu_dvfs_ctx.core_auto_reg.args);
#else
	gpu_dvfs_ctx.top_force_reg.regmap_ptr = syscon_regmap_lookup_by_phandle(dev->of_node,"top_force_shutdown");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.top_force_reg.regmap_ptr);
	syscon_regmap_lookup_by_phandle_args(dev->of_node,"top_force_shutdown", 2, (uint32_t *)gpu_dvfs_ctx.top_force_reg.args);

	gpu_dvfs_ctx.core_force_reg.regmap_ptr = syscon_regmap_lookup_by_phandle(dev->of_node,"core_force_shutdown");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.core_force_reg.regmap_ptr);
	syscon_regmap_lookup_by_phandle_args(dev->of_node,"core_force_shutdown", 2, (uint32_t *)gpu_dvfs_ctx.core_force_reg.args);

	gpu_dvfs_ctx.gpu_top_state_reg.regmap_ptr = syscon_regmap_lookup_by_phandle(dev->of_node,"gpu_top_state");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.gpu_top_state_reg.regmap_ptr);
	syscon_regmap_lookup_by_phandle_args(dev->of_node,"gpu_top_state", 2, (uint32_t *)gpu_dvfs_ctx.gpu_top_state_reg.args);

	gpu_dvfs_ctx.gpu_core_state_reg.regmap_ptr = syscon_regmap_lookup_by_phandle(dev->of_node,"gpu_core_state");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.gpu_core_state_reg.regmap_ptr);
	syscon_regmap_lookup_by_phandle_args(dev->of_node,"gpu_core_state", 2, (uint32_t *)gpu_dvfs_ctx.gpu_core_state_reg.args);

	gpu_dvfs_ctx.clk_gpu_eb_reg.regmap_ptr = syscon_regmap_lookup_by_phandle(dev->of_node,"clk_gpu_eb");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.clk_gpu_eb_reg.regmap_ptr);
	syscon_regmap_lookup_by_phandle_args(dev->of_node,"clk_gpu_eb", 2, (uint32_t *)gpu_dvfs_ctx.clk_gpu_eb_reg.args);

	gpu_dvfs_ctx.core_auto_reg.regmap_ptr = syscon_regmap_lookup_by_phandle(dev->of_node,"core_auto_shutdown");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.core_auto_reg.regmap_ptr);
	syscon_regmap_lookup_by_phandle_args(dev->of_node,"core_auto_shutdown", 2, (uint32_t *)gpu_dvfs_ctx.core_auto_reg.args);
#endif

	gpu_dvfs_ctx.clk_gpu_i = of_clk_get(dev->of_node, 0);
	PVR_ASSERT(NULL != gpu_dvfs_ctx.clk_gpu_i);

	gpu_dvfs_ctx.clk_gpu_core = of_clk_get(dev->of_node, 1);
	PVR_ASSERT(NULL != gpu_dvfs_ctx.clk_gpu_core);

	gpu_dvfs_ctx.clk_gpu_soc = of_clk_get(dev->of_node, 2);
	PVR_ASSERT(NULL != gpu_dvfs_ctx.clk_gpu_soc);

	clk_cnt = of_clk_get_parent_count(dev->of_node);
	gpu_dvfs_ctx.gpu_clk_num = clk_cnt - DTS_CLK_OFFSET;

	gpu_dvfs_ctx.gpu_clk_src = vmalloc(sizeof(struct clk*) * gpu_dvfs_ctx.gpu_clk_num);
	PVR_ASSERT(NULL != gpu_dvfs_ctx.gpu_clk_src);

	for (i = 0; i < gpu_dvfs_ctx.gpu_clk_num; i++)
	{
		gpu_dvfs_ctx.gpu_clk_src[i] = of_clk_get(dev->of_node, i+DTS_CLK_OFFSET);
		PVR_ASSERT(NULL != gpu_dvfs_ctx.gpu_clk_src[i]);
	}

	gpu_dvfs_ctx.freq_list_len = of_property_count_elems_of_size(dev->of_node,"sprd,dvfs-lists",4*sizeof(u32));
	gpu_dvfs_ctx.freq_list = vmalloc(sizeof(struct gpu_freq_info) * gpu_dvfs_ctx.freq_list_len);
	PVR_ASSERT(NULL != gpu_dvfs_ctx.freq_list);

	for(i=0; i<gpu_dvfs_ctx.freq_list_len; i++)
	{
		int clk = 0;

		of_property_read_u32_index(dev->of_node, "sprd,dvfs-lists", 4*i+2, &clk);
		gpu_dvfs_ctx.freq_list[i].clk_src = gpu_dvfs_ctx.gpu_clk_src[clk-DTS_CLK_OFFSET];
		PVR_ASSERT(NULL != gpu_dvfs_ctx.freq_list[i].clk_src);
		of_property_read_u32_index(dev->of_node, "sprd,dvfs-lists", 4*i,   &gpu_dvfs_ctx.freq_list[i].freq);
		of_property_read_u32_index(dev->of_node, "sprd,dvfs-lists", 4*i+1, &gpu_dvfs_ctx.freq_list[i].volt);
		of_property_read_u32_index(dev->of_node, "sprd,dvfs-lists", 4*i+3, &gpu_dvfs_ctx.freq_list[i].div);
	}

#if defined(SUPPORT_PDVFS)
	FillOppTable();
#endif

	of_property_read_u32(dev->of_node, "sprd,dvfs-default", &i);
	gpu_dvfs_ctx.freq_default = &gpu_dvfs_ctx.freq_list[i];
	PVR_ASSERT(NULL !=gpu_dvfs_ctx.freq_default);

	gpu_dvfs_ctx.freq_cur = gpu_dvfs_ctx.freq_default;
	gpu_dvfs_ctx.cur_voltage = gpu_dvfs_ctx.freq_cur->volt;
}

static void RgxTimingInfoInit(RGX_TIMING_INFORMATION* psRGXTimingInfo)
{
	PVR_ASSERT(NULL != psRGXTimingInfo);

	/*
	 * Setup RGX specific timing data
	 */
	psRGXTimingInfo->ui32CoreClockSpeed    = gpu_dvfs_ctx.freq_default->freq * FREQ_KHZ;
	psRGXTimingInfo->bEnableActivePM       = IMG_TRUE;
	psRGXTimingInfo->bEnableRDPowIsland    = IMG_TRUE;
	psRGXTimingInfo->ui32ActivePMLatencyms = SYS_RGX_ACTIVE_POWER_LATENCY_MS;
}

#if defined(SUPPORT_LINUX_DVFS) || defined(SUPPORT_PDVFS)
static int RgxFreqSearch(struct gpu_freq_info freq_list[], int len, int key)
{
	int low = 0, high = len-1, mid = 0;

	if (0 > key)
	{
		return -1;
	}

	while (low <= high)
	{
		mid = (low+high)/2;
		if (key == freq_list[mid].freq)
		{
			return mid;
		}

		if(key < freq_list[mid].freq)
		{
			high = mid-1;
		}
		else
		{
			low = mid+1;
		}
	}
	return -1;
}

static int RgxSetFreqVolt(IMG_UINT32 ui32Freq, IMG_UINT32 ui32Volt)
{
	int index = -1, err = -1;

	ui32Freq = ui32Freq/FREQ_KHZ;
	index = RgxFreqSearch(gpu_dvfs_ctx.freq_list, gpu_dvfs_ctx.freq_list_len, ui32Freq);
	PVR_DPF((PVR_DBG_WARNING, "GPU DVFS %s index=%d cur_freq=%d cur_voltage=%d --> ui32Freq=%d ui32Volt=%d gpu_power_on=%d gpu_clock_on=%d \n",
		__func__, index, gpu_dvfs_ctx.freq_cur->freq, gpu_dvfs_ctx.cur_voltage, ui32Freq, ui32Volt,
		gpu_dvfs_ctx.gpu_power_on, gpu_dvfs_ctx.gpu_clock_on));
	if (0 <= index)
	{
		down(gpu_dvfs_ctx.sem);
		if(gpu_dvfs_ctx.gpu_power_on && gpu_dvfs_ctx.gpu_clock_on)
		{
			if(ui32Freq != gpu_dvfs_ctx.freq_cur->freq)
			{
				//set gpu core clk
				clk_set_parent(gpu_dvfs_ctx.clk_gpu_core, gpu_dvfs_ctx.freq_list[index].clk_src);

				//set gpu mem clk
				clk_set_parent(gpu_dvfs_ctx.clk_gpu_soc, gpu_dvfs_ctx.freq_list[index].clk_src);
			}
		}
		gpu_dvfs_ctx.freq_cur = &gpu_dvfs_ctx.freq_list[index];
		err = 0;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
		trace_clock_set_rate(VENDOR_FTRACE_MODULE_NAME, gpu_dvfs_ctx.freq_cur->freq, raw_smp_processor_id());
#endif
		up(gpu_dvfs_ctx.sem);
	}

	return (err);
}

static void RgxDVFSInit(PVRSRV_DEVICE_CONFIG* psDevConfig)
{
#if defined(SUPPORT_PDVFS)
	psDevConfig->sDVFS.sDVFSDeviceCfg.pasOPPTable = gpu_dvfs_ctx.pasOPPTable;
	psDevConfig->sDVFS.sDVFSDeviceCfg.ui32OPPTableSize = gpu_dvfs_ctx.freq_list_len;
#endif
	psDevConfig->sDVFS.sDVFSDeviceCfg.ui32PollMs = GPU_POLL_MS;
	psDevConfig->sDVFS.sDVFSDeviceCfg.bIdleReq = IMG_FALSE;
	psDevConfig->sDVFS.sDVFSDeviceCfg.pfnSetFreqVolt = RgxSetFreqVolt;

	psDevConfig->sDVFS.sDVFSGovernorCfg.ui32UpThreshold = GPU_UP_THRESHOLD;
	psDevConfig->sDVFS.sDVFSGovernorCfg.ui32DownDifferential = GPU_DOWN_DIFFERENTIAL;
}
#endif

static void RgxPowerOn(void)
{
	//GPU power
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
	pmu_glb_clr(REG_PMU_APB_PD_GPU_TOP_CFG, BIT_PMU_APB_PD_GPU_TOP_FORCE_SHUTDOWN);
	pmu_glb_clr(REG_PMU_APB_PD_GPU_CORE_CFG, BIT_PMU_APB_PD_GPU_CORE_FORCE_SHUTDOWN);
	pmu_glb_set(REG_PMU_APB_PD_GPU_CORE_CFG, BIT_PMU_APB_PD_GPU_CORE_AUTO_SHUTDOWN_EN);
#else
	regmap_update_bits(gpu_dvfs_ctx.top_force_reg.regmap_ptr, gpu_dvfs_ctx.top_force_reg.args[0], gpu_dvfs_ctx.top_force_reg.args[1], 0);
	regmap_update_bits(gpu_dvfs_ctx.core_force_reg.regmap_ptr, gpu_dvfs_ctx.core_force_reg.args[0], gpu_dvfs_ctx.core_force_reg.args[1], 0);
	regmap_update_bits(gpu_dvfs_ctx.core_auto_reg.regmap_ptr, gpu_dvfs_ctx.core_auto_reg.args[0], gpu_dvfs_ctx.core_auto_reg.args[1], gpu_dvfs_ctx.core_auto_reg.args[1]);
#endif

	gpu_dvfs_ctx.gpu_power_on = 1;
}

static void RgxPowerOff(void)
{
	gpu_dvfs_ctx.gpu_power_on = 0;

	//GPU power
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
	pmu_glb_set(REG_PMU_APB_PD_GPU_TOP_CFG, BIT_PMU_APB_PD_GPU_TOP_FORCE_SHUTDOWN);
	pmu_glb_set(REG_PMU_APB_PD_GPU_CORE_CFG, BIT_PMU_APB_PD_GPU_CORE_FORCE_SHUTDOWN);
	pmu_glb_clr(REG_PMU_APB_PD_GPU_CORE_CFG, BIT_PMU_APB_PD_GPU_CORE_AUTO_SHUTDOWN_EN);
#else
	regmap_update_bits(gpu_dvfs_ctx.top_force_reg.regmap_ptr, gpu_dvfs_ctx.top_force_reg.args[0], gpu_dvfs_ctx.top_force_reg.args[1], gpu_dvfs_ctx.top_force_reg.args[1]);
	regmap_update_bits(gpu_dvfs_ctx.core_force_reg.regmap_ptr, gpu_dvfs_ctx.core_force_reg.args[0], gpu_dvfs_ctx.core_force_reg.args[1], gpu_dvfs_ctx.core_force_reg.args[1]);
	regmap_update_bits(gpu_dvfs_ctx.core_auto_reg.regmap_ptr, gpu_dvfs_ctx.core_auto_reg.args[0], gpu_dvfs_ctx.core_auto_reg.args[1], 0);
#endif
}

static void RgxClockOn(void)
{
	int i;

	//enable all clocks
	for(i=0;i<gpu_dvfs_ctx.gpu_clk_num;i++)
	{
		clk_prepare_enable(gpu_dvfs_ctx.gpu_clk_src[i]);
	}
	clk_prepare_enable(gpu_dvfs_ctx.clk_gpu_i);

	//enable gpu clock
	clk_prepare_enable(gpu_dvfs_ctx.clk_gpu_core);
	clk_prepare_enable(gpu_dvfs_ctx.clk_gpu_soc);
	udelay(300);

	//set gpu clock parent
	clk_set_parent(gpu_dvfs_ctx.clk_gpu_core, gpu_dvfs_ctx.freq_default->clk_src);
	clk_set_parent(gpu_dvfs_ctx.clk_gpu_soc, gpu_dvfs_ctx.freq_default->clk_src);

	PVR_ASSERT(NULL != gpu_dvfs_ctx.freq_cur);
	clk_set_parent(gpu_dvfs_ctx.clk_gpu_core, gpu_dvfs_ctx.freq_cur->clk_src);
	clk_set_parent(gpu_dvfs_ctx.clk_gpu_soc, gpu_dvfs_ctx.freq_cur->clk_src);

	gpu_dvfs_ctx.gpu_clock_on = 1;
}

static void RgxClockOff(void)
{
	int i;

	gpu_dvfs_ctx.gpu_clock_on = 0;

	//disable gpu clock
	clk_disable_unprepare(gpu_dvfs_ctx.clk_gpu_core);
	clk_disable_unprepare(gpu_dvfs_ctx.clk_gpu_soc);
	clk_disable_unprepare(gpu_dvfs_ctx.clk_gpu_i);

	//disable all clocks
	for(i=0;i<gpu_dvfs_ctx.gpu_clk_num;i++)
	{
		clk_disable_unprepare(gpu_dvfs_ctx.gpu_clk_src[i]);
	}
}

static PVRSRV_ERROR SprdPrePowerState(IMG_HANDLE hSysData, PVRSRV_SYS_POWER_STATE eNewPowerState, PVRSRV_SYS_POWER_STATE eCurrentPowerState, IMG_UINT32 bForced)
{
	PVRSRV_ERROR result = PVRSRV_OK;

	// PVR_DPF((PVR_DBG_WARNING, "GPU power %s eNewPowerState=%d eCurrentPowerState=%d bForced=%d  gpu_power_on=%d gpu_clock_on=%d",
	// 	__func__, eNewPowerState, eCurrentPowerState, bForced,
	// 	gpu_dvfs_ctx.gpu_power_on, gpu_dvfs_ctx.gpu_clock_on));
	if ((PVRSRV_SYS_POWER_STATE_ON == eNewPowerState) &&
		(eNewPowerState != eCurrentPowerState))
	{
		down(gpu_dvfs_ctx.sem);
		if (!gpu_dvfs_ctx.gpu_power_on)
		{
			RgxPowerOn();
			RgxClockOn();
		}

		if (!gpu_dvfs_ctx.gpu_clock_on)
		{
			RgxClockOn();
		}
		up(gpu_dvfs_ctx.sem);
	}
	return (result);
}

static PVRSRV_ERROR SprdPostPowerState(IMG_HANDLE hSysData, PVRSRV_SYS_POWER_STATE eNewPowerState, PVRSRV_SYS_POWER_STATE eCurrentPowerState, IMG_UINT32 bForced)
{
	PVRSRV_ERROR result = PVRSRV_OK;

	// PVR_DPF((PVR_DBG_WARNING, "GPU power %s eNewPowerState=%d eCurrentPowerState=%d bForced=%d gpu_power_on=%d gpu_clock_on=%d",
	// 	__func__, eNewPowerState, eCurrentPowerState, bForced,
	// 	gpu_dvfs_ctx.gpu_power_on, gpu_dvfs_ctx.gpu_clock_on));
	if ((PVRSRV_SYS_POWER_STATE_OFF == eNewPowerState) &&
		(eNewPowerState != eCurrentPowerState))
	{
		down(gpu_dvfs_ctx.sem);
		if(gpu_dvfs_ctx.gpu_clock_on)
		{
			RgxClockOff();
		}

		if(gpu_dvfs_ctx.gpu_power_on)
		{
			RgxPowerOff();
		}
		up(gpu_dvfs_ctx.sem);
	}
	return (result);
}

static void RgxPowerManager(PVRSRV_DEVICE_CONFIG* psDevConfig)
{
	//No power management on no HW system
	psDevConfig->pfnPrePowerState  = SprdPrePowerState;
	psDevConfig->pfnPostPowerState = SprdPostPowerState;
}

void RgxSprdInit(PVRSRV_DEVICE_CONFIG* psDevConfig, RGX_TIMING_INFORMATION* psRGXTimingInfo, void *pvOSDevice)
{
	struct platform_device *pDevice = to_platform_device((struct device *)pvOSDevice);

	//device init
	RgxDeviceInit(psDevConfig, pDevice);

	//gpu freq
	RgxFreqInit(&pDevice->dev);

	//rgx timing info
	RgxTimingInfoInit(psRGXTimingInfo);

#if defined(SUPPORT_LINUX_DVFS) || defined(SUPPORT_PDVFS)
	//DVFS init
	RgxDVFSInit(psDevConfig);
#endif

	//rgx power manager
	RgxPowerManager(psDevConfig);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_set_active(&pDevice->dev);
	pm_suspend_ignore_children(&pDevice->dev, true);
	pm_runtime_set_autosuspend_delay(&pDevice->dev, PM_RUNTIME_DELAY_MS);
	pm_runtime_use_autosuspend(&pDevice->dev);
	pm_runtime_enable(&pDevice->dev);
#endif

	//power on
	RgxPowerOn();

	//clock on
	RgxClockOn();
}

void RgxSprdDeInit(void)
{
	down(gpu_dvfs_ctx.sem);

	//clock off
	RgxClockOff();

	//power off
	RgxPowerOff();

	//free
	vfree(gpu_dvfs_ctx.freq_list);
	vfree(gpu_dvfs_ctx.gpu_clk_src);
#if defined(SUPPORT_PDVFS)
	vfree(gpu_dvfs_ctx.pasOPPTable);
#endif
	up(gpu_dvfs_ctx.sem);
}

#if defined(PVR_BOOST)
void RgxSetBoost(IMG_PID pid, IMG_CHAR *pProcName)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
	int len = -1, boost_level = 0;
	char cmd[256];
	struct file *fp=NULL;

	if (!strstr(pProcName, "RenderThread"))
	{
		return;
	}

	//get boost level
	fp = filp_open("/proc/self/cmdline", O_RDONLY, 0);
	if (IS_ERR(fp))
	{
		PVR_DPF((PVR_DBG_ERROR, "%s open file fail! \n", __func__));
		return;
	}
	len = kernel_read(fp, 0, cmd, sizeof(cmd)-1);
	filp_close(fp, NULL);
	if(len<0)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s read file fail! \n", __func__));
		return;
	}
	cmd[len]='\0';

	if (strstr(cmd, "com.eg.android.AlipayGphone"))
	{
		PVR_DPF((PVR_DBG_WARNING, "RgxSetBoost name%s  boost_level=10 \n", cmd));
		boost_level = 10;
	}

	if (gpu_boost_level < boost_level)
	{
		gpu_boost_level = boost_level;
	}
#endif
}

void RgxModifyTargetFreq(struct device *dev, unsigned long *target_freq)
{
	switch(gpu_boost_level)
	{
	case 10:
		*target_freq = gpu_dvfs_ctx.freq_list[gpu_dvfs_ctx.freq_list_len-1].freq * FREQ_KHZ;
		PVR_DPF((PVR_DBG_WARNING, "RgxModifyTargetFreq target_freq = %lu! \n", *target_freq));
		break;

	case 9:
		break;

	case 7:
		break;

	case 5:
		break;

	case 0:
	default:
		break;
	}

	gpu_boost_level = 0;
}
#endif
