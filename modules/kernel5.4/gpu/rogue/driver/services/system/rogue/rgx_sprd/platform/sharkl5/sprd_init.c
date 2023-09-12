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
#define VENDOR_FTRACE_MODULE_NAME    "unisoc-gpu"

//#define CREATE_TRACE_POINTS
//#include "sprd_trace.h"

#define SYS_RGX_ACTIVE_POWER_LATENCY_MS (15)
#define PM_RUNTIME_DELAY_MS (50)

#define DTS_CLK_OFFSET          6
#define FREQ_KHZ                1000

#if defined(SUPPORT_LINUX_DVFS) || defined(SUPPORT_PDVFS)
#define GPU_POLL_MS             50
#define GPU_UP_THRESHOLD        80
#define GPU_DOWN_DIFFERENTIAL   10
#endif

struct gpu_qos_config {
	u8 arqos;
	u8 awqos;
	u8 arqos_threshold;
	u8 awqos_threshold;
};

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
	struct clk*  clk_gpu_core_eb;
	struct clk*  clk_gpu_core;
	struct clk*  clk_gpu_mem_eb;
	struct clk*  clk_gpu_mem;
	struct clk*  clk_gpu_sys_eb;
	struct clk*  clk_gpu_sys;
	struct clk** gpu_clk_src;
	int gpu_clk_num;

	struct gpu_freq_info* freq_list;
	int freq_list_len;

#if defined(SUPPORT_PDVFS)
	IMG_OPP  *pasOPPTable;
#endif

	int cur_index;
	const struct gpu_freq_info* freq_cur;

	const struct gpu_freq_info* freq_default;

	struct semaphore* sem;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
	struct regmap* pmu_apb_reg_base;
#else
	struct gpu_reg_info top_force_reg;
	struct gpu_reg_info rgx_dust_force_reg;
	struct gpu_reg_info rgx_dust_auto_reg;
	struct gpu_reg_info gpu_qos_sel;
	struct gpu_reg_info gpu_qos;
	struct gpu_reg_info dvfs_index_cfg;
	struct gpu_reg_info sw_dvfs_ctrl;
	struct gpu_reg_info pdvfs_cfg;
#endif
};

DEFINE_SEMAPHORE(gpu_dvfs_sem);
static struct gpu_dvfs_context gpu_dvfs_ctx=
{
	.gpu_clock_on=0,
	.gpu_power_on=0,

	.sem=&gpu_dvfs_sem,
};

static struct gpu_qos_config gpu_qos_cfg=
{
	.arqos=0,
	.awqos=0,
	.arqos_threshold=0,
	.awqos_threshold=0,
};

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

	struct device_node *qos_node = NULL;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
	gpu_dvfs_ctx.pmu_apb_reg_base = syscon_regmap_lookup_by_phandle(dev->of_node,"sprd,syscon-pmu-apb");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.pmu_apb_reg_base);
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	gpu_dvfs_ctx.top_force_reg.regmap_ptr = syscon_regmap_lookup_by_name(dev->of_node,"top_force_shutdown");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.top_force_reg.regmap_ptr);
	syscon_get_args_by_name(dev->of_node,"top_force_shutdown", 2, (uint32_t *)gpu_dvfs_ctx.top_force_reg.args);

	gpu_dvfs_ctx.rgx_dust_force_reg.regmap_ptr = syscon_regmap_lookup_by_name(dev->of_node,"rgx_dust_force_shutdown");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.rgx_dust_force_reg.regmap_ptr);
	syscon_get_args_by_name(dev->of_node,"rgx_dust_force_shutdown", 2, (uint32_t *)gpu_dvfs_ctx.rgx_dust_force_reg.args);

	gpu_dvfs_ctx.rgx_dust_auto_reg.regmap_ptr = syscon_regmap_lookup_by_name(dev->of_node,"rgx_dust_auto_shutdown");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.rgx_dust_auto_reg.regmap_ptr);
	syscon_get_args_by_name(dev->of_node,"rgx_dust_auto_shutdown", 2, (uint32_t *)gpu_dvfs_ctx.rgx_dust_auto_reg.args);

	//qos
	gpu_dvfs_ctx.gpu_qos_sel.regmap_ptr = syscon_regmap_lookup_by_name(dev->of_node,"gpu_qos_sel");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.gpu_qos_sel.regmap_ptr);
	syscon_get_args_by_name(dev->of_node,"gpu_qos_sel", 2, (uint32_t *)gpu_dvfs_ctx.gpu_qos_sel.args);

	gpu_dvfs_ctx.gpu_qos.regmap_ptr = syscon_regmap_lookup_by_name(dev->of_node,"gpu_qos");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.gpu_qos.regmap_ptr);
	syscon_get_args_by_name(dev->of_node,"gpu_qos", 2, (uint32_t *)gpu_dvfs_ctx.gpu_qos.args);

	//gpu index cfg
	gpu_dvfs_ctx.dvfs_index_cfg.regmap_ptr = syscon_regmap_lookup_by_name(dev->of_node,"dvfs_index_cfg");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.dvfs_index_cfg.regmap_ptr);
	syscon_get_args_by_name(dev->of_node,"dvfs_index_cfg", 2, (uint32_t *)gpu_dvfs_ctx.dvfs_index_cfg.args);

	//sw dvfs ctrl
	gpu_dvfs_ctx.sw_dvfs_ctrl.regmap_ptr = syscon_regmap_lookup_by_name(dev->of_node,"sw_dvfs_ctrl");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.sw_dvfs_ctrl.regmap_ptr);
	syscon_get_args_by_name(dev->of_node,"sw_dvfs_ctrl", 2, (uint32_t *)gpu_dvfs_ctx.sw_dvfs_ctrl.args);

	//pdvfs cfg
	gpu_dvfs_ctx.pdvfs_cfg.regmap_ptr = syscon_regmap_lookup_by_name(dev->of_node,"pdvfs_cfg");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.pdvfs_cfg.regmap_ptr);
	syscon_get_args_by_name(dev->of_node,"pdvfs_cfg", 2, (uint32_t *)gpu_dvfs_ctx.pdvfs_cfg.args);
#else
	gpu_dvfs_ctx.top_force_reg.regmap_ptr = syscon_regmap_lookup_by_phandle(dev->of_node,"top_force_shutdown");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.top_force_reg.regmap_ptr);
	syscon_regmap_lookup_by_phandle_args(dev->of_node,"top_force_shutdown", 2, (uint32_t *)gpu_dvfs_ctx.top_force_reg.args);

	gpu_dvfs_ctx.rgx_dust_force_reg.regmap_ptr = syscon_regmap_lookup_by_phandle(dev->of_node,"rgx_dust_force_shutdown");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.rgx_dust_force_reg.regmap_ptr);
	syscon_regmap_lookup_by_phandle_args(dev->of_node,"rgx_dust_force_shutdown", 2, (uint32_t *)gpu_dvfs_ctx.rgx_dust_force_reg.args);

	gpu_dvfs_ctx.rgx_dust_auto_reg.regmap_ptr = syscon_regmap_lookup_by_phandle(dev->of_node,"rgx_dust_auto_shutdown");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.rgx_dust_auto_reg.regmap_ptr);
	syscon_regmap_lookup_by_phandle_args(dev->of_node,"rgx_dust_auto_shutdown", 2, (uint32_t *)gpu_dvfs_ctx.rgx_dust_auto_reg.args);

	//qos
	gpu_dvfs_ctx.gpu_qos_sel.regmap_ptr = syscon_regmap_lookup_by_phandle(dev->of_node,"gpu_qos_sel");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.gpu_qos_sel.regmap_ptr);
	syscon_regmap_lookup_by_phandle_args(dev->of_node,"gpu_qos_sel", 2, (uint32_t *)gpu_dvfs_ctx.gpu_qos_sel.args);

	gpu_dvfs_ctx.gpu_qos.regmap_ptr = syscon_regmap_lookup_by_phandle(dev->of_node,"gpu_qos");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.gpu_qos.regmap_ptr);
	syscon_regmap_lookup_by_phandle_args(dev->of_node,"gpu_qos", 2, (uint32_t *)gpu_dvfs_ctx.gpu_qos.args);

	//gpu index cfg
	gpu_dvfs_ctx.dvfs_index_cfg.regmap_ptr = syscon_regmap_lookup_by_phandle(dev->of_node,"dvfs_index_cfg");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.dvfs_index_cfg.regmap_ptr);
	syscon_regmap_lookup_by_phandle_args(dev->of_node,"dvfs_index_cfg", 2, (uint32_t *)gpu_dvfs_ctx.dvfs_index_cfg.args);

	//sw dvfs ctrl
	gpu_dvfs_ctx.sw_dvfs_ctrl.regmap_ptr = syscon_regmap_lookup_by_phandle(dev->of_node,"sw_dvfs_ctrl");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.sw_dvfs_ctrl.regmap_ptr);
	syscon_regmap_lookup_by_phandle_args(dev->of_node,"sw_dvfs_ctrl", 2, (uint32_t *)gpu_dvfs_ctx.sw_dvfs_ctrl.args);

	//pdvfs cfg
	gpu_dvfs_ctx.pdvfs_cfg.regmap_ptr = syscon_regmap_lookup_by_phandle(dev->of_node,"pdvfs_cfg");
	PVR_ASSERT(NULL != gpu_dvfs_ctx.pdvfs_cfg.regmap_ptr);
	syscon_regmap_lookup_by_phandle_args(dev->of_node,"pdvfs_cfg", 2, (uint32_t *)gpu_dvfs_ctx.pdvfs_cfg.args);
#endif

	/* qos dts parse */
	qos_node = of_parse_phandle(dev->of_node, "sprd,qos", 0);
	if (qos_node)
	{
		if (of_property_read_u8(qos_node, "arqos", &gpu_qos_cfg.arqos)) {
			pr_warn("gpu arqos config reading fail.\n");
		}
		if (of_property_read_u8(qos_node, "awqos", &gpu_qos_cfg.awqos)) {
			pr_warn("gpu awqos config reading fail.\n");
		}
		if (of_property_read_u8(qos_node, "arqos-threshold", &gpu_qos_cfg.arqos_threshold)) {
			pr_warn("gpu arqos_threshold config reading fail.\n");
		}
		if (of_property_read_u8(qos_node, "awqos-threshold", &gpu_qos_cfg.awqos_threshold)) {
			pr_warn("gpu awqos_threshold config reading fail.\n");
		}
	} else {
		pr_warn("can't find gpu qos config node\n");
	}


	gpu_dvfs_ctx.clk_gpu_i = of_clk_get(dev->of_node, 0);
	PVR_ASSERT(NULL != gpu_dvfs_ctx.clk_gpu_i);

	gpu_dvfs_ctx.clk_gpu_core_eb = of_clk_get(dev->of_node, 1);
	PVR_ASSERT(NULL != gpu_dvfs_ctx.clk_gpu_core_eb);

	gpu_dvfs_ctx.clk_gpu_core = of_clk_get(dev->of_node, 2);
	PVR_ASSERT(NULL != gpu_dvfs_ctx.clk_gpu_core);

	gpu_dvfs_ctx.clk_gpu_mem_eb = of_clk_get(dev->of_node, 3);
	PVR_ASSERT(NULL != gpu_dvfs_ctx.clk_gpu_mem_eb);

	gpu_dvfs_ctx.clk_gpu_mem = of_clk_get(dev->of_node, 4);
	PVR_ASSERT(NULL != gpu_dvfs_ctx.clk_gpu_mem);

	gpu_dvfs_ctx.clk_gpu_sys_eb = of_clk_get(dev->of_node, 5);
	PVR_ASSERT(NULL != gpu_dvfs_ctx.clk_gpu_sys_eb);

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

	gpu_dvfs_ctx.cur_index = i;
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

#if !defined(PVR_HW_DVFS)
static int getVoltageIndex(int target_voltage)
{
	int volt_index = 0;

	//get sw voltage index 0: 0.65v 1:0.7v 2:0.75v 3:0.8v
	switch (target_voltage)
	{
	case 700000:
		volt_index = 1;
		break;

	case 750000:
		volt_index = 2;
		break;

	case 800000:
	default:
		volt_index = 3;
		break;
	}

	return (volt_index);
}

static bool setSwVoltage(int volt_index)
{
	/*bool result = false;
	int i = 0;
	IMG_UINT32 sw_dvfs_ctrl = 0;

	//set sw voltage and req sw
	sw_dvfs_ctrl = (volt_index << 4) | 0x1;
	regmap_update_bits(gpu_dvfs_ctx.sw_dvfs_ctrl.regmap_ptr, gpu_dvfs_ctx.sw_dvfs_ctrl.args[0], gpu_dvfs_ctx.sw_dvfs_ctrl.args[1], sw_dvfs_ctrl);

	//read dvfs ack
	for (i=0; i<3; i++)
	{
		regmap_read(gpu_dvfs_ctx.sw_dvfs_ctrl.regmap_ptr, gpu_dvfs_ctx.sw_dvfs_ctrl.args[0], &sw_dvfs_ctrl);
		if (sw_dvfs_ctrl & 0x100)
		{
			result = true;
			break;
		}
		else
		{
			udelay(5);
		}
	}

	//clear req sw
	regmap_update_bits(gpu_dvfs_ctx.sw_dvfs_ctrl.regmap_ptr, gpu_dvfs_ctx.sw_dvfs_ctrl.args[0], 1, 0);

	return (result);*/
	return true;
}

static bool upgrade_voltage(int index)
{
	bool result = true;
	int target_voltage = 0, volt_index = 0;

	target_voltage = gpu_dvfs_ctx.freq_list[index].volt;
	PVR_DPF((PVR_DBG_WARNING, "GPU_DVFS upgrade_voltage target_voltage=%d cur_voltage=%d \n",target_voltage, gpu_dvfs_ctx.cur_voltage));
	if (target_voltage > gpu_dvfs_ctx.cur_voltage)
	{
		//get voltage index
		volt_index = getVoltageIndex(target_voltage);

		//set sw voltage
		result = setSwVoltage(volt_index);
		if (result)
		{
			//set gpu current voltage
			gpu_dvfs_ctx.cur_voltage = target_voltage;
		}
	}

	return (result);
}

static bool downgrade_voltage(int index)
{
	bool result = true;
	int target_voltage = 0, volt_index = 0;

	target_voltage = gpu_dvfs_ctx.freq_list[index].volt;
	PVR_DPF((PVR_DBG_WARNING, "GPU_DVFS downgrade_voltage target_voltage=%d cur_voltage=%d \n",target_voltage, gpu_dvfs_ctx.cur_voltage));
	if (target_voltage < gpu_dvfs_ctx.cur_voltage)
	{
		//get voltage index
		volt_index = getVoltageIndex(target_voltage);

		//set sw voltage
		result = setSwVoltage(volt_index);
		if (result)
		{
			//set gpu current voltage
			gpu_dvfs_ctx.cur_voltage = target_voltage;
		}
	}

	return (result);
}
#endif

static int RgxSetFreqVolt(IMG_UINT32 ui32Freq, IMG_UINT32 ui32Volt)
{
	bool ret = true;
	int index = -1, err = -1;

	ui32Freq = ui32Freq/FREQ_KHZ;
	index = RgxFreqSearch(gpu_dvfs_ctx.freq_list, gpu_dvfs_ctx.freq_list_len, ui32Freq);
	PVR_DPF((PVR_DBG_WARNING, "GPU DVFS %s index=%d cur_freq=%d cur_voltage=%d --> ui32Freq=%d ui32Volt=%d gpu_power_on=%d gpu_clock_on=%d \n",
		__func__, index, gpu_dvfs_ctx.freq_cur->freq, gpu_dvfs_ctx.cur_voltage, ui32Freq, ui32Volt,
		gpu_dvfs_ctx.gpu_power_on, gpu_dvfs_ctx.gpu_clock_on));
	if (0 <= index)
	{
		down(gpu_dvfs_ctx.sem);

		//set frequency
		if (gpu_dvfs_ctx.gpu_power_on && gpu_dvfs_ctx.gpu_clock_on)
		{
#if !defined(PVR_HW_DVFS)
			//upgrade voltage
			ret = upgrade_voltage(index);
#endif

			if (ret)
			{
#if defined(PVR_HW_DVFS)
				//set dvfs index, 0: 384M 1:512M 2:614.4M 3:768M 4:800M
				regmap_update_bits(gpu_dvfs_ctx.dvfs_index_cfg.regmap_ptr, gpu_dvfs_ctx.dvfs_index_cfg.args[0], gpu_dvfs_ctx.dvfs_index_cfg.args[1], index);
#else
				if (ui32Freq != gpu_dvfs_ctx.freq_cur->freq)
				{
					//set gpu core clk
					clk_set_parent(gpu_dvfs_ctx.clk_gpu_core, gpu_dvfs_ctx.freq_list[index].clk_src);

					//set gpu mem clk
					clk_set_parent(gpu_dvfs_ctx.clk_gpu_mem, gpu_dvfs_ctx.freq_list[index].clk_src);
				}
#endif
			}

#if !defined(PVR_HW_DVFS)
			//downgrade voltage
			ret = downgrade_voltage(index);
#endif
		}
		gpu_dvfs_ctx.cur_index = index;
		gpu_dvfs_ctx.freq_cur = &gpu_dvfs_ctx.freq_list[index];
		err = 0;
		//trace_sprd_gpu_devfreq(gpu_dvfs_ctx.freq_cur->freq);
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
#if defined(SUPPORT_LINUX_DVFS)
	psDevConfig->sDVFS.sDVFSGovernorCfg.ui32UpThreshold = GPU_UP_THRESHOLD;
	psDevConfig->sDVFS.sDVFSGovernorCfg.ui32DownDifferential = GPU_DOWN_DIFFERENTIAL;

	psDevConfig->sDVFS.sDVFSDeviceCfg.ui32PollMs = GPU_POLL_MS;
#endif
	psDevConfig->sDVFS.sDVFSDeviceCfg.bIdleReq = IMG_FALSE;
	psDevConfig->sDVFS.sDVFSDeviceCfg.pfnSetFreqVolt = RgxSetFreqVolt;
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
	regmap_update_bits(gpu_dvfs_ctx.top_force_reg.regmap_ptr, gpu_dvfs_ctx.top_force_reg.args[0], gpu_dvfs_ctx.top_force_reg.args[1], ~gpu_dvfs_ctx.top_force_reg.args[1]);
	regmap_update_bits(gpu_dvfs_ctx.rgx_dust_force_reg.regmap_ptr, gpu_dvfs_ctx.rgx_dust_force_reg.args[0], gpu_dvfs_ctx.rgx_dust_force_reg.args[1], ~gpu_dvfs_ctx.rgx_dust_force_reg.args[1]);
	regmap_update_bits(gpu_dvfs_ctx.rgx_dust_auto_reg.regmap_ptr, gpu_dvfs_ctx.rgx_dust_auto_reg.args[0], gpu_dvfs_ctx.rgx_dust_auto_reg.args[1], gpu_dvfs_ctx.rgx_dust_auto_reg.args[1]);
#endif
	udelay(100);

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
	regmap_update_bits(gpu_dvfs_ctx.rgx_dust_force_reg.regmap_ptr, gpu_dvfs_ctx.rgx_dust_force_reg.args[0], gpu_dvfs_ctx.rgx_dust_force_reg.args[1], gpu_dvfs_ctx.rgx_dust_force_reg.args[1]);
	regmap_update_bits(gpu_dvfs_ctx.rgx_dust_auto_reg.regmap_ptr, gpu_dvfs_ctx.rgx_dust_auto_reg.args[0], gpu_dvfs_ctx.rgx_dust_auto_reg.args[1], ~gpu_dvfs_ctx.rgx_dust_auto_reg.args[1]);
#endif
}
static void RgxQosConfig(void)
{
	regmap_update_bits(gpu_dvfs_ctx.gpu_qos_sel.regmap_ptr, gpu_dvfs_ctx.gpu_qos_sel.args[0], gpu_dvfs_ctx.gpu_qos_sel.args[1], gpu_dvfs_ctx.gpu_qos_sel.args[1]);
	regmap_update_bits(gpu_dvfs_ctx.gpu_qos.regmap_ptr, gpu_dvfs_ctx.gpu_qos.args[0], gpu_dvfs_ctx.gpu_qos.args[1], ((gpu_qos_cfg.awqos_threshold << 12) | (gpu_qos_cfg.arqos_threshold << 8) | (gpu_qos_cfg.awqos << 4) | gpu_qos_cfg.arqos));
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
	clk_prepare_enable(gpu_dvfs_ctx.clk_gpu_core_eb);
	clk_prepare_enable(gpu_dvfs_ctx.clk_gpu_core);
	clk_prepare_enable(gpu_dvfs_ctx.clk_gpu_mem_eb);
	clk_prepare_enable(gpu_dvfs_ctx.clk_gpu_mem);
	clk_prepare_enable(gpu_dvfs_ctx.clk_gpu_sys_eb);
	udelay(200);

#if defined(PVR_HW_DVFS)
	//set dvfs index, 0: 384M 1:512M 2:614.4M 3:768M 4:800M
	PVR_ASSERT(0 <= gpu_dvfs_ctx.cur_index);
	regmap_update_bits(gpu_dvfs_ctx.dvfs_index_cfg.regmap_ptr, gpu_dvfs_ctx.dvfs_index_cfg.args[0], gpu_dvfs_ctx.dvfs_index_cfg.args[1], gpu_dvfs_ctx.cur_index);
#else
#if defined(SUPPORT_PDVFS)
	regmap_update_bits(gpu_dvfs_ctx.pdvfs_cfg.regmap_ptr, gpu_dvfs_ctx.pdvfs_cfg.args[0], gpu_dvfs_ctx.pdvfs_cfg.args[1], gpu_dvfs_ctx.pdvfs_cfg.args[1]);
#else
	//set gpu clock parent
	clk_set_parent(gpu_dvfs_ctx.clk_gpu_core, gpu_dvfs_ctx.freq_default->clk_src);
	clk_set_parent(gpu_dvfs_ctx.clk_gpu_mem, gpu_dvfs_ctx.freq_default->clk_src);

	PVR_ASSERT(NULL != gpu_dvfs_ctx.freq_cur);
	clk_set_parent(gpu_dvfs_ctx.clk_gpu_core, gpu_dvfs_ctx.freq_cur->clk_src);
	clk_set_parent(gpu_dvfs_ctx.clk_gpu_mem, gpu_dvfs_ctx.freq_cur->clk_src);
#endif //SUPPORT_PDVFS
#endif //PVR_HW_DVFS

	//qos
	RgxQosConfig();

	gpu_dvfs_ctx.gpu_clock_on = 1;
}

static void RgxClockOff(void)
{
	int i;

	gpu_dvfs_ctx.gpu_clock_on = 0;

	//disable gpu clock
	clk_disable_unprepare(gpu_dvfs_ctx.clk_gpu_core);
	clk_disable_unprepare(gpu_dvfs_ctx.clk_gpu_core_eb);
	clk_disable_unprepare(gpu_dvfs_ctx.clk_gpu_mem);
	clk_disable_unprepare(gpu_dvfs_ctx.clk_gpu_mem_eb);
	clk_disable_unprepare(gpu_dvfs_ctx.clk_gpu_sys_eb);
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


