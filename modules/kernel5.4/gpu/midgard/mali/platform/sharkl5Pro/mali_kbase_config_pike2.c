/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */



#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/hwspinlock.h>
#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#include <mali_kbase_config.h>
#include <mali_kbase_debug.h>
#include <backend/gpu/mali_kbase_device_internal.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/of_address.h>
#ifdef KBASE_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#ifdef CONFIG_MALI_HOTPLUG
#include <hotplug/mali_kbase_hotplug.h>
#endif /* CONFIG_MALI_HOTPLUG */
#include <linux/regulator/consumer.h>

#define DTS_CLK_OFFSET      2
#define PM_RUNTIME_DELAY_MS 50
#define UP_THRESHOLD        9/10

struct gpu_freq_info {
	struct clk* clk_src;
	int freq;	//kHz
	int volt;	//uV
	int div;
	int up_threshold;
};

struct gpu_reg_info {
	struct regmap* regmap_ptr;
	uint32_t args[2];
};

struct gpu_dfs_context {
	int gpu_clock_on;
	int gpu_power_on;
	int cur_load;
	int cur_voltage;

	struct clk*  gpu_clock;
	struct clk* gpu_clock_i;
	struct clk** gpu_clk_src;
	int gpu_clk_num;

	struct gpu_freq_info* freq_list;
	int freq_list_len;

	const struct gpu_freq_info* freq_cur;
	const struct gpu_freq_info* freq_next;

	const struct gpu_freq_info* freq_min;
	const struct gpu_freq_info* freq_max;
	const struct gpu_freq_info* freq_default;
	const struct gpu_freq_info* freq_9;
	const struct gpu_freq_info* freq_8;
	const struct gpu_freq_info* freq_7;
	const struct gpu_freq_info* freq_5;
	const struct gpu_freq_info* freq_range_max;
	const struct gpu_freq_info* freq_range_min;

	struct workqueue_struct *gpu_dfs_workqueue;
	struct semaphore* sem;

	struct gpu_reg_info top_reg;
};

#ifndef CONFIG_OF
static struct kbase_io_resources io_resources = {
	.job_irq_number = 68,
	.mmu_irq_number = 69,
	.gpu_irq_number = 70,
	.io_memory_region = {
	.start = 0x2F010000,
	.end = 0x2F010000 + (4096 * 4) - 1}
};
#endif

DEFINE_SEMAPHORE(gpu_dfs_sem);
static struct gpu_dfs_context gpu_dfs_ctx=
{
	.gpu_clock_on=0,
	.gpu_power_on=0,

	.sem=&gpu_dfs_sem,
};

extern int gpu_boost_level;

extern int gpu_freq_cur;
extern int gpu_voltage_cur;
#ifdef CONFIG_MALI_MIDGARD_DVFS
extern int gpu_freq_min_limit;
extern int gpu_freq_max_limit;
#endif
extern char* gpu_freq_list;

static void gpu_freq_list_show(char* buf)
{
	int i=0,len=0;

	for(i=0; i<gpu_dfs_ctx.freq_list_len; i++)
	{
		len = sprintf(buf,"%2d  %6d kHz %6d uV\n", i, gpu_dfs_ctx.freq_list[i].freq, gpu_dfs_ctx.freq_list[i].volt);
		buf += len;
	}
}

static inline void mali_freq_init(struct device *dev)
{
#ifdef CONFIG_OF
	int i = 0, clk_cnt = 0;

	gpu_dfs_ctx.top_reg.regmap_ptr = syscon_regmap_lookup_by_name(dev->of_node,"top_force_shutdown");
	KBASE_DEBUG_ASSERT(gpu_dfs_ctx.top_reg.regmap_ptr);
	syscon_get_args_by_name(dev->of_node,"top_force_shutdown", 2, (uint32_t *)gpu_dfs_ctx.top_reg.args);

	gpu_dfs_ctx.gpu_clock_i = of_clk_get(dev->of_node, 0);
	gpu_dfs_ctx.gpu_clock = of_clk_get(dev->of_node, 1);
	KBASE_DEBUG_ASSERT(gpu_dfs_ctx.gpu_clock_i);
	KBASE_DEBUG_ASSERT(gpu_dfs_ctx.gpu_clock);

	clk_cnt = of_clk_get_parent_count(dev->of_node);
	gpu_dfs_ctx.gpu_clk_num = clk_cnt - DTS_CLK_OFFSET;

	gpu_dfs_ctx.gpu_clk_src = vmalloc(sizeof(struct clk*) * gpu_dfs_ctx.gpu_clk_num);
	KBASE_DEBUG_ASSERT(gpu_dfs_ctx.gpu_clk_src);

	for (i = 0; i < gpu_dfs_ctx.gpu_clk_num; i++)
	{
		gpu_dfs_ctx.gpu_clk_src[i] = of_clk_get(dev->of_node, i+DTS_CLK_OFFSET);
		KBASE_DEBUG_ASSERT(gpu_dfs_ctx.gpu_clk_src[i]);
	}

	gpu_dfs_ctx.freq_list_len = of_property_count_elems_of_size(dev->of_node,"sprd,dfs-lists",4*sizeof(u32));
	gpu_dfs_ctx.freq_list = vmalloc(sizeof(struct gpu_freq_info) * gpu_dfs_ctx.freq_list_len);
	KBASE_DEBUG_ASSERT(gpu_dfs_ctx.freq_list);

	for(i=0; i<gpu_dfs_ctx.freq_list_len; i++)
	{
		int clk = 0;

		of_property_read_u32_index(dev->of_node, "sprd,dfs-lists", 4*i+2, &clk);
		gpu_dfs_ctx.freq_list[i].clk_src = gpu_dfs_ctx.gpu_clk_src[clk-DTS_CLK_OFFSET];
		KBASE_DEBUG_ASSERT(gpu_dfs_ctx.freq_list[i].clk_src);
		of_property_read_u32_index(dev->of_node, "sprd,dfs-lists", 4*i, &gpu_dfs_ctx.freq_list[i].freq);
		of_property_read_u32_index(dev->of_node, "sprd,dfs-lists", 4*i+1, &gpu_dfs_ctx.freq_list[i].volt);
		of_property_read_u32_index(dev->of_node, "sprd,dfs-lists", 4*i+3, &gpu_dfs_ctx.freq_list[i].div);
		gpu_dfs_ctx.freq_list[i].up_threshold =  gpu_dfs_ctx.freq_list[i].freq * UP_THRESHOLD;
	}

	of_property_read_u32(dev->of_node, "sprd,dfs-default", &i);
	gpu_dfs_ctx.freq_default = &gpu_dfs_ctx.freq_list[i];
	KBASE_DEBUG_ASSERT(gpu_dfs_ctx.freq_default);

	of_property_read_u32(dev->of_node, "sprd,dfs-scene-extreme", &i);
	gpu_dfs_ctx.freq_9 = &gpu_dfs_ctx.freq_list[i];
	KBASE_DEBUG_ASSERT(gpu_dfs_ctx.freq_9);

	of_property_read_u32(dev->of_node, "sprd,dfs-scene-high", &i);
	gpu_dfs_ctx.freq_8 = &gpu_dfs_ctx.freq_list[i];
	KBASE_DEBUG_ASSERT(gpu_dfs_ctx.freq_8);

	of_property_read_u32(dev->of_node, "sprd,dfs-scene-medium", &i);
	gpu_dfs_ctx.freq_7 = &gpu_dfs_ctx.freq_list[i];
	KBASE_DEBUG_ASSERT(gpu_dfs_ctx.freq_7);

	of_property_read_u32(dev->of_node, "sprd,dfs-scene-low", &i);
	gpu_dfs_ctx.freq_5 = &gpu_dfs_ctx.freq_list[i];
	KBASE_DEBUG_ASSERT(gpu_dfs_ctx.freq_5);

	of_property_read_u32(dev->of_node, "sprd,dfs-range-max", &i);
	gpu_dfs_ctx.freq_range_max = &gpu_dfs_ctx.freq_list[i];
	KBASE_DEBUG_ASSERT(gpu_dfs_ctx.freq_range_max);

	of_property_read_u32(dev->of_node, "sprd,dfs-range-min", &i);
	gpu_dfs_ctx.freq_range_min = &gpu_dfs_ctx.freq_list[i];
	KBASE_DEBUG_ASSERT(gpu_dfs_ctx.freq_range_min);

	gpu_dfs_ctx.freq_max = gpu_dfs_ctx.freq_range_max;
	gpu_dfs_ctx.freq_min = gpu_dfs_ctx.freq_range_min;
	gpu_dfs_ctx.freq_cur = gpu_dfs_ctx.freq_default;
	gpu_dfs_ctx.cur_voltage = gpu_voltage_cur = gpu_dfs_ctx.freq_cur->volt;
#endif
}

static inline void mali_power_on(void)
{
	regmap_update_bits(gpu_dfs_ctx.top_reg.regmap_ptr, gpu_dfs_ctx.top_reg.args[0], gpu_dfs_ctx.top_reg.args[1], ~gpu_dfs_ctx.top_reg.args[1]);

	udelay(300);
	gpu_dfs_ctx.gpu_power_on = 1;
}

static inline void mali_power_off(void)
{
	gpu_dfs_ctx.gpu_power_on = 0;

	regmap_update_bits(gpu_dfs_ctx.top_reg.regmap_ptr, gpu_dfs_ctx.top_reg.args[0], gpu_dfs_ctx.top_reg.args[1], gpu_dfs_ctx.top_reg.args[1]);
}

static inline void mali_clock_on(void)
{
	int i;

	//enable all clocks
	for(i=0;i<gpu_dfs_ctx.gpu_clk_num;i++)
	{
		clk_prepare_enable(gpu_dfs_ctx.gpu_clk_src[i]);
	}

	clk_prepare_enable(gpu_dfs_ctx.gpu_clock_i);

	//enable gpu clock
	clk_prepare_enable(gpu_dfs_ctx.gpu_clock);
	udelay(300);

	//set gpu clock parent
	clk_set_parent(gpu_dfs_ctx.gpu_clock, gpu_dfs_ctx.freq_default->clk_src);

	KBASE_DEBUG_ASSERT(gpu_dfs_ctx.freq_cur);
	clk_set_parent(gpu_dfs_ctx.gpu_clock, gpu_dfs_ctx.freq_cur->clk_src);

	gpu_dfs_ctx.gpu_clock_on = 1;

	gpu_freq_cur = gpu_dfs_ctx.freq_cur->freq;
}

static inline void mali_clock_off(void)
{
	int i;

	gpu_freq_cur = 0;

	gpu_dfs_ctx.gpu_clock_on = 0;

	//disable gpu clock
	clk_disable_unprepare(gpu_dfs_ctx.gpu_clock);
	clk_disable_unprepare(gpu_dfs_ctx.gpu_clock_i);

	//disable all clocks
	for(i=0;i<gpu_dfs_ctx.gpu_clk_num;i++)
	{
		clk_disable_unprepare(gpu_dfs_ctx.gpu_clk_src[i]);
	}
}

static int mali_platform_init(struct kbase_device *kbdev)
{
	KBASE_DEBUG_PRINT(4, "mali------------------- mali_platform_init\n");
	//gpu freq
	mali_freq_init(kbdev->dev);

	mali_power_on();

	//clock on
	mali_clock_on();

#ifdef CONFIG_MALI_HOTPLUG
	kbase_hotplug_wake_up();
#endif

	gpu_dfs_ctx.gpu_dfs_workqueue = create_singlethread_workqueue("gpu_dfs");

	gpu_freq_list = (char*)vmalloc(256*sizeof(char));
	gpu_freq_list_show(gpu_freq_list);

	return 0;
}

static void mali_platform_term(struct kbase_device *kbdev)
{
	KBASE_DEBUG_PRINT(4, "mali------------------- mali_platform_term\n");
	down(gpu_dfs_ctx.sem);
	//destory work queue
	destroy_workqueue(gpu_dfs_ctx.gpu_dfs_workqueue);

	//clock off
	mali_clock_off();

	//power off
	mali_power_off();

	//free
	vfree(gpu_freq_list);
	vfree(gpu_dfs_ctx.freq_list);
	vfree(gpu_dfs_ctx.gpu_clk_src);

	up(gpu_dfs_ctx.sem);
}

struct kbase_platform_funcs_conf platform_pike2_funcs = {
	.platform_init_func = mali_platform_init,
	.platform_term_func = mali_platform_term
};

static void mali_power_mode_change(struct kbase_device *kbdev, int power_mode)
{
	down(gpu_dfs_ctx.sem);
	KBASE_DEBUG_PRINT(3, "mali_power_mode_change: %d, gpu_power_on=%d gpu_clock_on=%d\n",power_mode,gpu_dfs_ctx.gpu_power_on,gpu_dfs_ctx.gpu_clock_on);
	switch (power_mode)
	{
		case 0://power on
			if (!gpu_dfs_ctx.gpu_power_on)
			{
				mali_power_on();
				mali_clock_on();
			}

			if (!gpu_dfs_ctx.gpu_clock_on)
			{
				mali_clock_on();
			}
#ifdef CONFIG_MALI_HOTPLUG
			kbase_hotplug_wake_up();
#endif
			break;

		case 1://light sleep
		case 2://deep sleep
			if(gpu_dfs_ctx.gpu_clock_on)
			{
				mali_clock_off();
			}

			if(gpu_dfs_ctx.gpu_power_on)
			{
				mali_power_off();
			}
			break;

		default:
			break;
	}
	up(gpu_dfs_ctx.sem);
}

static void pm_callback_power_off(struct kbase_device *kbdev)
{
#ifdef KBASE_PM_RUNTIME
	int res;

	KBASE_DEBUG_PRINT(4, "mali------------------- pm_callback_power_off\n");
	res = pm_runtime_put_sync(kbdev->dev);
	if (res < 0)
	{
		printk(KERN_ERR "mali----pm_runtime_put_sync return (%d)\n", res);
	}
#endif

	mali_power_mode_change(kbdev, 1);
}

static int pm_callback_power_on(struct kbase_device *kbdev)
{
	KBASE_DEBUG_PRINT(4, "mali------------------- pm_callback_power_on\n");
	mali_power_mode_change(kbdev, 0);

#ifdef KBASE_PM_RUNTIME
	{
		int res;

		res = pm_runtime_get_sync(kbdev->dev);
		if (res < 0)
		{
			printk(KERN_ERR "mali----pm_runtime_get_sync return (%d)\n", res);
		}
	}
#endif

	return 1;
}

static void pm_callback_power_suspend(struct kbase_device *kbdev)
{
	KBASE_DEBUG_PRINT(4, "mali------------------- pm_callback_power_suspend\n");
	mali_power_mode_change(kbdev, 2);
}

static void pm_callback_power_resume(struct kbase_device *kbdev)
{
	KBASE_DEBUG_PRINT(4, "mali------------------- pm_callback_power_resume\n");
	mali_power_mode_change(kbdev, 0);
}

#ifdef KBASE_PM_RUNTIME
static int pm_callback_power_runtime_init(struct kbase_device *kbdev)
{
	KBASE_DEBUG_PRINT(4, "mali------------------- pm_callback_power_runtime_init\n");
	pm_runtime_set_active(kbdev->dev);
	pm_suspend_ignore_children(kbdev->dev, true);
	pm_runtime_set_autosuspend_delay(kbdev->dev, PM_RUNTIME_DELAY_MS);
	pm_runtime_use_autosuspend(kbdev->dev);
	pm_runtime_enable(kbdev->dev);

	return 0;
}

static void pm_callback_power_runtime_term(struct kbase_device *kbdev)
{
	KBASE_DEBUG_PRINT(4, "mali------------------- pm_callback_power_runtime_term\n");
	pm_runtime_disable(kbdev->dev);
}
#endif/*CONFIG_PM_RUNTIME*/

struct kbase_pm_callback_conf pm_pike2_callbacks = {
	.power_off_callback = pm_callback_power_off,
	.power_on_callback = pm_callback_power_on,
	.power_suspend_callback = pm_callback_power_suspend,
	.power_resume_callback = pm_callback_power_resume,
#ifdef KBASE_PM_RUNTIME
	.power_runtime_init_callback = pm_callback_power_runtime_init,
	.power_runtime_term_callback = pm_callback_power_runtime_term,
	.power_runtime_off_callback = NULL,
	.power_runtime_on_callback = NULL
#endif
};


static struct kbase_platform_config versatile_platform_config = {
#ifndef CONFIG_OF
	.io_resources = &io_resources
#endif
};

struct kbase_platform_config *kbase_get_platform_config(void)
{
	return &versatile_platform_config;
}

int kbase_platform_early_init(void)
{
	/* Nothing needed at this stage */
	return 0;
}

#if defined(CONFIG_MALI_MIDGARD_DVFS) || defined(CONFIG_MALI_DEVFREQ)
static int freq_search(struct gpu_freq_info freq_list[], int len, int key)
{
	int low=0, high=len-1, mid;

	if (0 > key)
	{
		return -1;
	}

	while(low <= high)
	{
		mid = (low+high)/2;
		if(key == freq_list[mid].freq)
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

static void gpu_change_freq(void)
{
	KBASE_DEBUG_PRINT(3, "GPU_DVFS gpu_change_freq cur_freq %6d -> next_freq %6d\n",
				gpu_dfs_ctx.freq_cur->freq, gpu_dfs_ctx.freq_next->freq);
	if(gpu_dfs_ctx.freq_next != gpu_dfs_ctx.freq_cur)
	{
		if(gpu_dfs_ctx.freq_next->clk_src != gpu_dfs_ctx.freq_cur->clk_src)
		{
			clk_set_parent(gpu_dfs_ctx.gpu_clock, gpu_dfs_ctx.freq_next->clk_src);
		}

		gpu_dfs_ctx.freq_cur = gpu_dfs_ctx.freq_next;
		gpu_freq_cur = gpu_dfs_ctx.freq_cur->freq;
	}
}

static int gpu_change_volt_freq(void)
{
	int result = -1;

	down(gpu_dfs_ctx.sem);
	KBASE_DEBUG_PRINT(3, "GPU_DVFS gpu_change_volt_freq gpu_power_on=%d, gpu_clock_on=%d\n",
		gpu_dfs_ctx.gpu_power_on, gpu_dfs_ctx.gpu_clock_on);
	//set frequency
	if (gpu_dfs_ctx.gpu_power_on && gpu_dfs_ctx.gpu_clock_on)
	{
		gpu_change_freq();
	}
	else
	{
		//power on will set freq
		gpu_dfs_ctx.freq_cur = gpu_dfs_ctx.freq_next;
	}

	result = 0;
	up(gpu_dfs_ctx.sem);

	return (result);
}
#endif

#ifdef CONFIG_MALI_MIDGARD_DVFS
static const struct gpu_freq_info* get_next_freq(const struct gpu_freq_info* min_freq, const struct gpu_freq_info* max_freq, int target)
{
	const struct gpu_freq_info* freq;

	for (freq = min_freq; freq <= max_freq; freq++)
	{
		if (freq->up_threshold > target)
		{
			return freq;
		}
	}
	return max_freq;
}

static void gpu_dfs_func(struct work_struct *work)
{
	gpu_change_volt_freq();
}

static DECLARE_WORK(gpu_dfs_work, &gpu_dfs_func);

int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation,
	u32 util_gl_share, u32 util_cl_share[2])
{
	int result = 0, min_index = -1, max_index = -1;

	gpu_dfs_ctx.cur_load = utilisation;

	KBASE_DEBUG_PRINT(3, "MALI_DVFS utilisation=%d util_gl_share=%d, util_cl_share[0]=%d, util_cl_share[1]=%d \n",
		utilisation, util_gl_share, util_cl_share[0], util_cl_share[1]);
	KBASE_DEBUG_PRINT(3, "MALI_DVFS gpu_boost_level:%d \n", gpu_boost_level);

	switch(gpu_boost_level)
	{
	case 10:
		gpu_dfs_ctx.freq_max =
		gpu_dfs_ctx.freq_min = &gpu_dfs_ctx.freq_list[gpu_dfs_ctx.freq_list_len-1];
		break;

	case 9:
		gpu_dfs_ctx.freq_max =
		gpu_dfs_ctx.freq_min = gpu_dfs_ctx.freq_9;
		break;

	case 7:
		gpu_dfs_ctx.freq_max =
		gpu_dfs_ctx.freq_min = gpu_dfs_ctx.freq_7;
		break;

	case 5:
		gpu_dfs_ctx.freq_max =
		gpu_dfs_ctx.freq_min = gpu_dfs_ctx.freq_5;
		break;

	case 0:
	default:
		gpu_dfs_ctx.freq_max = gpu_dfs_ctx.freq_range_max;
		gpu_dfs_ctx.freq_min = gpu_dfs_ctx.freq_range_min;
		break;
	}

	gpu_boost_level = 0;

	//limit min freq
	min_index = freq_search(gpu_dfs_ctx.freq_list, gpu_dfs_ctx.freq_list_len, gpu_freq_min_limit);
	if ((0 <= min_index) &&
		(gpu_dfs_ctx.freq_min->freq < gpu_dfs_ctx.freq_list[min_index].freq))
	{
		gpu_dfs_ctx.freq_min = &gpu_dfs_ctx.freq_list[min_index];
		if (gpu_dfs_ctx.freq_min->freq > gpu_dfs_ctx.freq_max->freq)
		{
			gpu_dfs_ctx.freq_max = gpu_dfs_ctx.freq_min;
		}
	}

	//limit max freq
	max_index = freq_search(gpu_dfs_ctx.freq_list, gpu_dfs_ctx.freq_list_len, gpu_freq_max_limit);
	if ((0 <= max_index) &&
		(gpu_dfs_ctx.freq_max->freq > gpu_dfs_ctx.freq_list[max_index].freq))
	{
		gpu_dfs_ctx.freq_max = &gpu_dfs_ctx.freq_list[max_index];
		if (gpu_dfs_ctx.freq_max->freq < gpu_dfs_ctx.freq_min->freq)
		{
			gpu_dfs_ctx.freq_min = gpu_dfs_ctx.freq_max;
		}
	}

	// if the loading ratio is greater then 90%, switch the clock to the maximum
	if(gpu_dfs_ctx.cur_load >= (100*UP_THRESHOLD))
	{
		gpu_dfs_ctx.freq_next = gpu_dfs_ctx.freq_max;
	}
	else
	{
		int target_freq = gpu_dfs_ctx.freq_cur->freq * gpu_dfs_ctx.cur_load / 100;
		gpu_dfs_ctx.freq_next = get_next_freq(gpu_dfs_ctx.freq_min, gpu_dfs_ctx.freq_max, target_freq);
	}

	KBASE_DEBUG_PRINT(3, "MALI_DVFS util %3d; cur_freq %6d -> next_freq %6d\n",
		gpu_dfs_ctx.cur_load, gpu_dfs_ctx.freq_cur->freq, gpu_dfs_ctx.freq_next->freq);

	if(gpu_dfs_ctx.freq_next->freq != gpu_dfs_ctx.freq_cur->freq)
	{
		queue_work(gpu_dfs_ctx.gpu_dfs_workqueue, &gpu_dfs_work);
	}

	return (result);
}
#endif/*CONFIG_MALI_MIDGARD_DVFS*/

#ifdef CONFIG_MALI_DEVFREQ
#define FREQ_KHZ    1000

int kbase_platform_get_init_freq(void)
{
	return (gpu_dfs_ctx.freq_default->freq * FREQ_KHZ);
}

int kbase_platform_set_freq(int freq)
{
	int result = -1, index = -1;

	freq = freq/FREQ_KHZ;
	index = freq_search(gpu_dfs_ctx.freq_list, gpu_dfs_ctx.freq_list_len, freq);
	KBASE_DEBUG_PRINT(2, "GPU_DVFS  kbase_platform_set_freq index=%d! freq=%d \n", index, freq);
	if (0 <= index)
	{
		gpu_dfs_ctx.freq_next = &gpu_dfs_ctx.freq_list[index];
		result = gpu_change_volt_freq();
	}

	return (result);
}

#ifdef CONFIG_LIMIT_MIN_FREQ
void kbase_platform_limit_min_freq(struct device *dev)
{
	//remove 153.6M and 256M for HD
	//dev_pm_opp_remove(dev, gpu_dfs_ctx.freq_list[0].freq * FREQ_KHZ);
	//dev_pm_opp_remove(dev, gpu_dfs_ctx.freq_list[1].freq * FREQ_KHZ);
}
#endif


#ifdef CONFIG_MALI_BOOST
void kbase_platform_modify_target_freq(struct device *dev, unsigned long *target_freq)
{
	int min_index = -1, max_index = -1;
	struct kbase_device *kbdev = dev_get_drvdata(dev);

	switch(gpu_boost_level)
	{
	case 10:
		gpu_dfs_ctx.freq_max =
		gpu_dfs_ctx.freq_min = &gpu_dfs_ctx.freq_list[gpu_dfs_ctx.freq_list_len-1];
		break;

	case 9:
		gpu_dfs_ctx.freq_max =
		gpu_dfs_ctx.freq_min = gpu_dfs_ctx.freq_9;
		break;

	case 7:
		gpu_dfs_ctx.freq_max =
		gpu_dfs_ctx.freq_min = gpu_dfs_ctx.freq_7;
		break;

	case 5:
		gpu_dfs_ctx.freq_max =
		gpu_dfs_ctx.freq_min = gpu_dfs_ctx.freq_5;
		break;

	case 0:
	default:
		gpu_dfs_ctx.freq_max = gpu_dfs_ctx.freq_range_max;
		gpu_dfs_ctx.freq_min = gpu_dfs_ctx.freq_range_min;
		break;
	}

	//limit min freq
	min_index = freq_search(gpu_dfs_ctx.freq_list, gpu_dfs_ctx.freq_list_len, kbdev->devfreq->min_freq/FREQ_KHZ);
	if ((0 <= min_index) &&
		(gpu_dfs_ctx.freq_min->freq < gpu_dfs_ctx.freq_list[min_index].freq))
	{
		gpu_dfs_ctx.freq_min = &gpu_dfs_ctx.freq_list[min_index];
		if (gpu_dfs_ctx.freq_min->freq > gpu_dfs_ctx.freq_max->freq)
		{
			gpu_dfs_ctx.freq_max = gpu_dfs_ctx.freq_min;
		}
	}

	//limit max freq
	max_index = freq_search(gpu_dfs_ctx.freq_list, gpu_dfs_ctx.freq_list_len, kbdev->devfreq->max_freq/FREQ_KHZ);
	if ((0 <= max_index) &&
		(gpu_dfs_ctx.freq_max->freq > gpu_dfs_ctx.freq_list[max_index].freq))
	{
		gpu_dfs_ctx.freq_max = &gpu_dfs_ctx.freq_list[max_index];
		if (gpu_dfs_ctx.freq_max->freq < gpu_dfs_ctx.freq_min->freq)
		{
			gpu_dfs_ctx.freq_min = gpu_dfs_ctx.freq_max;
		}
	}

	KBASE_DEBUG_PRINT(3, "GPU_DVFS kbase_platform_modify_target_freq gpu_boost_level:%d min_freq=%d max_freq=%d target_freq=%lu\n",
		gpu_boost_level, gpu_dfs_ctx.freq_min->freq, gpu_dfs_ctx.freq_max->freq, *target_freq);

	gpu_boost_level = 0;

	//set target frequency
	if (*target_freq < gpu_dfs_ctx.freq_min->freq*FREQ_KHZ)
	{
		*target_freq = gpu_dfs_ctx.freq_min->freq*FREQ_KHZ;
	}
	if (*target_freq > gpu_dfs_ctx.freq_max->freq*FREQ_KHZ)
	{
		*target_freq = gpu_dfs_ctx.freq_max->freq*FREQ_KHZ;
	}
}
#endif
#endif

bool kbase_mali_is_powered(void)
{
	bool result = false;

	down(gpu_dfs_ctx.sem);
	if (gpu_dfs_ctx.gpu_power_on && gpu_dfs_ctx.gpu_clock_on)
	{
		result = true;
	}
	up(gpu_dfs_ctx.sem);

	return (result);
}

#ifdef CONFIG_MALI_BOOST
void kbase_platform_set_boost(struct kbase_device *kbdev, int boost_level)
{
	if (gpu_boost_level < boost_level)
	{
		gpu_boost_level = boost_level;
		KBASE_DEBUG_PRINT(3, "kbase_platform_set_boost gpu_boost_level =%d\n", gpu_boost_level);
	}
}
#endif
