// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020 Spreadtrum Communications Inc.

#define pr_fmt(fmt) "sprd_cpu_cooling: " fmt

#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/rculist.h>
#include <linux/rcupdate.h>
#include <linux/debugfs.h>
#include <linux/suspend.h>

#include <linux/sched.h>
#include <linux/cpumask.h>

#include <linux/cpu.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>

#if defined(CONFIG_OTP_SPRD_AP_EFUSE)
#include <linux/sprd_otp.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif
#include <trace/events/thermal.h>

#define MAX_SENSOR_NUMBER	8
static atomic_t in_suspend;

/*
 * Tscale = aT^3 + bT^2 + cT + d
 * Vscale = aV^3 + bV^2 + cV + d
 */
struct scale_coeff {
	int scale_a;
	int scale_b;
	int scale_c;
	int scale_d;
};

/* Pdyn = dynperghz * freq * (V/Vbase)^2 */
struct dyn_power_coeff {
	int dynperghz;
	int freq;
	int voltage_base;
};

struct cluster_power_coefficients {
	u32 hotplug_period;
	u32 min_cpufreq;
	u32 min_cpunum;
	u32 max_temp;
	int leak_core_base;
	int leak_cluster_base;
	struct scale_coeff core_temp_scale;
	struct scale_coeff core_voltage_scale;
	struct scale_coeff cluster_temp_scale;
	struct scale_coeff cluster_voltage_scale;
	struct dyn_power_coeff core_coeff;
	struct dyn_power_coeff cluster_coeff;
	struct thermal_cooling_device *cdev;
	struct cpumask clip_cpus;
	int nsensor;
	const char *sensor_names[MAX_SENSOR_NUMBER];
	struct thermal_zone_device *thm_zones[MAX_SENSOR_NUMBER];
	int core_temp[MAX_SENSOR_NUMBER];
};

static int counts;
static struct cluster_power_coefficients *cluster_data;

struct cpu_power_ops {

	u64 (*get_core_dyn_power_p)(int cooling_id, unsigned int freq_mhz,
			unsigned int voltage_mv);

	u64 (*get_cluster_dyn_power_p)(int cooling_id, unsigned int freq_mhz,
			unsigned int voltage_mv);

	int (*get_static_power_p)(cpumask_t *cpumask, int interval,
			unsigned long u_volt, u32 *power, int temperature);

	int (*get_core_static_power_p)(cpumask_t *cpumask, int interval,
			unsigned long u_volt, u32 *power, int temperature);

	u32 (*get_cluster_min_cpufreq_p)(int cooling_id);

	u32 (*get_cluster_min_cpunum_p)(int cooling_id);

	u32 (*get_cluster_max_temp_p)(int cooling_id);

	u32 (*get_cluster_cycle_p)(int cooling_id);

	u32 (*get_sensor_count_p)(int cooling_id);

	int (*get_min_temp_unisolated_core_p)(int cooling_id, int cpu,
					      int *temp);

	int (*get_min_temp_isolated_core_p)(int cooling_id, int cpu, int *temp);

	int (*get_all_core_temp_p)(int cooling_id, int cpu);

	void (*get_core_temp_p)(int cooling_id, int cpu, int *temp);
};

struct run_cpus_table {
	u32 cpus;
	u32 power;
};

struct cpu_cooling_device {
	int id;
	int round;
	int cycle;
	int min_cpus;
	int nsensor;
	unsigned int total;
	unsigned int level;
	unsigned int run_cpus;
	unsigned int max_level;
	unsigned int power;
	unsigned int cur_freq;
	unsigned int min_freq;
	unsigned int total_freq;
	unsigned int max_temp;
	struct cpumask allowed_cpus;
	struct run_cpus_table *table;
	struct thermal_cooling_device *cdev;
	struct cpu_power_ops *power_ops;
	struct cpumask idle_cpus;
};

static int (*cpu_isolate_fun)(struct cpumask *mask, int type);
static int (*cpu_isolated_fun)(int cpu);
static DEFINE_IDA(cpu_ida);

int cpu_isolate_funs(unsigned long fun1, unsigned long fun2)
{
	cpu_isolate_fun = (void *)fun1;
	cpu_isolated_fun = (void *)fun2;
	pr_info("isolate flag:%d\n", (cpu_isolate_fun && cpu_isolated_fun) ? 1 : 0);

	return 0;
}
EXPORT_SYMBOL_GPL(cpu_isolate_funs);


static int get_cluster_id(int cpu)
{
	return topology_physical_package_id((cpu));
}

static unsigned int get_level(struct cpu_cooling_device *cpu_cdev,
			       unsigned int cpus)
{
	struct run_cpus_table *table = cpu_cdev->table;
	unsigned int level;

	for (level = 0; level <= cpu_cdev->max_level; level++)
		if (cpus == table[level].cpus)
			break;

	return level;
}

static int cpu_get_static_power(struct cpu_cooling_device *cpu_cdev,
			    struct thermal_zone_device *tz, unsigned long freq,
			    u32 *power, u32 cpus)
{
	struct dev_pm_opp *opp;
	unsigned long voltage;
	unsigned long freq_hz = freq * 1000;
	struct cpumask cpus_mask;
	struct device *dev;
	int temp;
	unsigned int i, cpu;

	if (!cpu_cdev->power_ops || !cpus) {
		*power = 0;
		return 0;
	}

	cpu = cpumask_any(&cpu_cdev->allowed_cpus);
	dev = get_cpu_device(cpu);
	WARN_ON(!dev);

	opp = dev_pm_opp_find_freq_exact(dev, freq_hz, true);
	if (IS_ERR(opp)) {
		dev_warn_ratelimited(dev, "Failed to find OPP for frequency %lu: %ld\n",
				     freq_hz, PTR_ERR(opp));
		return -EINVAL;
	}

	voltage = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);

	if (voltage == 0) {
		dev_err_ratelimited(dev, "Failed to get voltage for frequency %lu\n",
				    freq_hz);
		return -EINVAL;
	}

	temp = tz->temperature;
	cpumask_clear(&cpus_mask);
	for (i = cpu; i < cpu + cpus; i++)
		cpumask_set_cpu(i, &cpus_mask);

	cpu_cdev->power_ops->get_core_static_power_p(
						&cpus_mask, tz->passive_delay,
						voltage, power, temp);

	return 0;
}

static int cpu_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct cpu_cooling_device *cpu_cdev = cdev->devdata;

	*state = cpu_cdev->max_level;
	return 0;
}

static int cpu_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct cpu_cooling_device *cpu_cdev = cdev->devdata;

	*state = cpu_cdev->level;

	return 0;
}

static int cpu_down_cpus(struct thermal_cooling_device *cdev,
			       u32 cur_cpus, u32 target_cpus)
{
	int cpu, ret;
	unsigned int first, ncpus;
	struct cpumask mask;
	struct cpu_cooling_device *cpu_cdev = cdev->devdata;

	first = cpumask_first(&cpu_cdev->allowed_cpus);
	ncpus = cpumask_weight(&cpu_cdev->allowed_cpus);

	if (cpu_isolate_fun && cpu_isolated_fun) {
		pr_info("cpu%d isolate cpus:%d curr_cpus:%u target_cpus:%u\n",
			first, cur_cpus - target_cpus, cur_cpus, target_cpus);
		cpumask_clear(&mask);
		for (cpu = (first + ncpus - 1); cpu >= first; cpu--) {
			if (cur_cpus == target_cpus || !cpu)
				break;
			if ((target_cpus < cur_cpus) && !cpu_isolated_fun(cpu)) {
				cpumask_set_cpu(cpu, &mask);
				cpumask_set_cpu(cpu, &cpu_cdev->idle_cpus);
				cur_cpus--;
			}
		}

		if (!cpumask_empty(&mask) && cpu_isolate_fun(&mask, 0))
			cpumask_andnot(&cpu_cdev->idle_cpus,
				       &cpu_cdev->idle_cpus, &mask);

		return 0;
	} else {
		pr_info("cpu%d hotplug out cpus:%d curr_cpus:%u target_cpus:%u\n",
			first, cur_cpus - target_cpus, cur_cpus, target_cpus);
		for (cpu = (first + ncpus - 1); cpu >= first; cpu--) {
			if (cur_cpus == target_cpus)
				break;
			if ((target_cpus < cur_cpus) && cpu_online(cpu)) {
				ret = cpu_down(cpu);
				if (!ret && !cpu_online(cpu)) {
					cur_cpus--;
					cpumask_set_cpu(cpu,
						&cpu_cdev->idle_cpus);
				}
			}
		}

	}

	return 0;
}

static int cpu_up_cpus(struct thermal_cooling_device *cdev,
			       u32 cur_cpus, u32 target_cpus)
{
	int cpu, ret;
	unsigned int first, ncpus;
	struct cpumask mask;
	struct cpu_cooling_device *cpu_cdev = cdev->devdata;

	first = cpumask_first(&cpu_cdev->allowed_cpus);
	ncpus = cpumask_weight(&cpu_cdev->allowed_cpus);

	if (cpu_isolate_fun && cpu_isolated_fun) {
		pr_info("cpu%d unisolate cpus:%d curr_cpus:%u target_cpus:%u\n",
			first, target_cpus - cur_cpus, cur_cpus, target_cpus);
		cpumask_clear(&mask);
		for (cpu = first; cpu < first + ncpus; cpu++) {
			if (cur_cpus == target_cpus)
				break;
			if ((target_cpus > cur_cpus) &&
			    cpu_online(cpu) && cpu_isolated_fun(cpu)) {
				cpumask_set_cpu(cpu, &mask);
				cpumask_clear_cpu(cpu, &cpu_cdev->idle_cpus);
				cur_cpus++;
			}
		}

		if (!cpumask_empty(&mask) && cpu_isolate_fun(&mask, 1))
			cpumask_or(&cpu_cdev->idle_cpus,
				   &cpu_cdev->idle_cpus, &mask);

		return 0;

	} else {
		pr_info("cpu%d hotplug in cpus:%d curr_cpus:%u target_cpus:%u\n",
			first, target_cpus - cur_cpus, cur_cpus, target_cpus);
		cpu = first;
		for_each_cpu(cpu, &cpu_cdev->allowed_cpus) {
			if (cur_cpus == target_cpus)
				break;
			if ((target_cpus > cur_cpus) && !cpu_online(cpu)) {
				ret = cpu_up(cpu);
				if (!ret && cpu_online(cpu)) {
					cpumask_clear_cpu(cpu,
							  &cpu_cdev->idle_cpus
							  );
					cur_cpus++;
				}
			}
		}
	}

	return 0;
}

static void cpu_update_target_cpus(struct thermal_cooling_device *cdev,
				   unsigned long state)
{
	u32 cur_run_cpus, next_run_cpus;
	struct cpumask run_cpus;
	struct cpu_cooling_device *cpu_cdev = cdev->devdata;

	cpumask_andnot(&run_cpus, &cpu_cdev->allowed_cpus,
		       &cpu_cdev->idle_cpus);
	cpu_cdev->run_cpus = cpumask_weight(&run_cpus);
	cur_run_cpus = cpu_cdev->run_cpus;
	next_run_cpus = cpu_cdev->table[state].cpus;

	if (cur_run_cpus > next_run_cpus)
		cpu_down_cpus(cdev, cur_run_cpus, next_run_cpus);
	else
		cpu_up_cpus(cdev, cur_run_cpus, next_run_cpus);

	cpumask_andnot(&run_cpus, &cpu_cdev->allowed_cpus,
		       &cpu_cdev->idle_cpus);
	cpu_cdev->run_cpus = cpumask_weight(&run_cpus);
	cpu_cdev->level = get_level(cpu_cdev, cpu_cdev->run_cpus);

}

static int cpu_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct cpu_cooling_device *cpu_cdev = cdev->devdata;

	/* Request state should be less than max_level */
	if (WARN_ON(state > cpu_cdev->max_level))
		return -EINVAL;

	/* Check if the old cooling action is same as new cooling action */
	if (cpu_cdev->level == state)
		return 0;

	cpu_update_target_cpus(cdev, state);

	return 0;
}

static int cpu_get_requested_power(struct thermal_cooling_device *cdev,
				       struct thermal_zone_device *tz,
				       u32 *power)
{
	unsigned int freq;
	unsigned int ncpus;
	int cpu, ret;
	u32 static_power;
	struct cpu_cooling_device *cpu_cdev = cdev->devdata;

	cpu = cpumask_any(&cpu_cdev->allowed_cpus);
	freq = cpufreq_quick_get_max(cpu);
	ncpus = cpumask_weight(&cpu_cdev->allowed_cpus);
	ret = cpu_get_static_power(cpu_cdev, tz, freq, &static_power, 1);
	if (ret)
		static_power = 0;

	cpu_cdev->cur_freq = freq;
	cpu_cdev->power = static_power;
	*power = static_power * ncpus;

	return 0;
}

static int cpu_state2power(struct thermal_cooling_device *cdev,
			       struct thermal_zone_device *tz,
			       unsigned long state, u32 *power)
{
	unsigned int num_cpus;
	u32 static_power;
	struct cpu_cooling_device *cpu_cdev = cdev->devdata;

	/* Request state should be less than max_level */
	if (WARN_ON(state > cpu_cdev->max_level))
		return -EINVAL;

	num_cpus = cpu_cdev->table[state].cpus;
	static_power = cpu_cdev->power;
	*power = static_power * num_cpus;

	return 0;
}

static int cpu_power2state(struct thermal_cooling_device *cdev,
			       struct thermal_zone_device *tz, u32 power,
			       unsigned long *state)
{
	unsigned int cpu, avg_freq;
	int cpus, max_cpus, id;
	u32 static_power;
	struct cpu_cooling_device *cpu_cdev = cdev->devdata;

	if (!cpu_cdev->cycle) {
		*state = get_level(cpu_cdev, cpu_cdev->run_cpus);
		return 0;
	}

	cpu = cpumask_any(&cpu_cdev->allowed_cpus);
	static_power = cpu_cdev->power;
	if (!static_power) {
		*state = get_level(cpu_cdev, cpu_cdev->run_cpus);
		return -EINVAL;
	}

	cpus = power / static_power;
	max_cpus = cpumask_weight(&cpu_cdev->allowed_cpus);
	cpus = min(cpus, max_cpus);
	cpu_cdev->total += cpus;
	cpu_cdev->total_freq += cpu_cdev->cur_freq;
	cpu_cdev->round++;

	if (cpu_cdev->round < cpu_cdev->cycle) {
		*state = get_level(cpu_cdev, cpu_cdev->run_cpus);
		return 0;
	}

	cpus = cpu_cdev->total / cpu_cdev->round;
	avg_freq = cpu_cdev->total_freq / cpu_cdev->round;
	cpu_cdev->total = 0;
	cpu_cdev->total_freq = 0;
	cpu_cdev->round = 0;

	id = get_cluster_id(cpu);
	if (cpu_cdev->power_ops->get_cluster_min_cpunum_p != NULL &&
	    cpu_cdev->power_ops->get_cluster_min_cpufreq_p != NULL &&
	    cpu_cdev->power_ops->get_cluster_max_temp_p != NULL) {
		cpu_cdev->min_cpus =
			cpu_cdev->power_ops->get_cluster_min_cpunum_p(id);
		cpu_cdev->min_freq =
			cpu_cdev->power_ops->get_cluster_min_cpufreq_p(id);
		cpu_cdev->max_temp =
			cpu_cdev->power_ops->get_cluster_max_temp_p(id);
	}

	cpus = max(cpus, cpu_cdev->min_cpus);
	if (cpus < cpu_cdev->run_cpus) {
		if (avg_freq > cpu_cdev->min_freq)  {
			*state = get_level(cpu_cdev, cpu_cdev->run_cpus);
			return 0;
		} else if (tz->temperature < cpu_cdev->max_temp) {
			*state = get_level(cpu_cdev, cpu_cdev->run_cpus);
			return 0;
		} else
			*state = get_level(cpu_cdev, cpus);

	} else
		*state = get_level(cpu_cdev, cpus);

	pr_debug("cpu%u temp:%u cur_freq:%u cur_cpus:%d target_cpus:%d\n",
		cpu, tz->temperature, cpu_cdev->cur_freq, cpu_cdev->run_cpus, cpus);

	return 0;
}

static struct thermal_cooling_device_ops cpu_power_cooling_ops = {
	.get_max_state		= cpu_get_max_state,
	.get_cur_state		= cpu_get_cur_state,
	.set_cur_state		= cpu_set_cur_state,
	.get_requested_power	= cpu_get_requested_power,
	.state2power		= cpu_state2power,
	.power2state		= cpu_power2state,
};

struct thermal_cooling_device *
cpu_cooling_register(struct device_node *np,
			const struct cpumask *cpus_mask,
			struct cpu_power_ops *plat_power_ops)
{
	struct thermal_cooling_device *cdev;
	struct cpu_cooling_device *cpu_cdev;
	char dev_name[THERMAL_NAME_LENGTH];
	unsigned int i, num_cpus;
	int ret, id;
	struct thermal_cooling_device_ops *cooling_ops;

	if (!np)
		return ERR_PTR(-EINVAL);

	cpu_cdev = kzalloc(sizeof(*cpu_cdev), GFP_KERNEL);
	if (!cpu_cdev)
		return ERR_PTR(-ENOMEM);

	num_cpus = cpumask_weight(cpus_mask);
	if (!num_cpus) {
		cdev = ERR_PTR(-ENOMEM);
		goto cpu_cdev;
	}

	cpu_cdev->max_level = num_cpus;
	cpumask_copy(&cpu_cdev->allowed_cpus, cpus_mask);
	cpu_cdev->table = kmalloc_array(num_cpus+1,
					sizeof(*cpu_cdev->table),
					GFP_KERNEL);
	if (!cpu_cdev->table) {
		cdev = ERR_PTR(-ENOMEM);
		goto cpu_cdev;
	}

	ret = ida_simple_get(&cpu_ida, 0, 0, GFP_KERNEL);
	if (ret < 0) {
		cdev = ERR_PTR(ret);
		goto free_table;
	}
	cpu_cdev->id = ret;

	snprintf(dev_name, sizeof(dev_name), "thermal-cpu-%d",
		 cpu_cdev->id);

	for (i = 0; i <= cpu_cdev->max_level; i++) {
		cpu_cdev->table[i].cpus = num_cpus;
		cpu_cdev->table[i].power = 0;
		num_cpus--;
	}

	if (plat_power_ops)
		cpu_cdev->power_ops = plat_power_ops;

	cpu_cdev->cycle = 0;
	id = get_cluster_id(cpumask_any(&cpu_cdev->allowed_cpus));
	if (cpu_cdev->power_ops->get_cluster_cycle_p != NULL)
		cpu_cdev->cycle = cpu_cdev->power_ops->get_cluster_cycle_p(id);

	cpu_cdev->nsensor = 0;
	if (cpu_cdev->power_ops->get_sensor_count_p != NULL)
		cpu_cdev->nsensor =
			cpu_cdev->power_ops->get_sensor_count_p(id);

	cooling_ops = &cpu_power_cooling_ops;
	cdev = thermal_of_cooling_device_register(np, dev_name, cpu_cdev,
						  cooling_ops);
	if (IS_ERR(cdev))
		goto remove_ida;

	cpu_cdev->round = 0;
	cpu_cdev->total = 0;
	cpu_cdev->total_freq = 0;
	cpu_cdev->min_cpus = 0;
	cpu_cdev->power = 0;
	cpu_cdev->run_cpus = cpu_cdev->table[0].cpus;
	cpu_cdev->level = 0;
	cpu_cdev->cdev = cdev;

	return cdev;

remove_ida:
	ida_simple_remove(&cpu_ida, cpu_cdev->id);
free_table:
	kfree(cpu_cdev->table);
cpu_cdev:
	kfree(cpu_cdev);
	return cdev;
}

static int  cpu_cooling_unregister(struct thermal_cooling_device *cdev)
{
	struct cpu_cooling_device *cpu_cdev;

	if (!cdev)
		return -ENODEV;

	cpu_cdev = cdev->devdata;
	cpu_set_cur_state(cdev, 0);
	thermal_cooling_device_unregister(cpu_cdev->cdev);
	ida_simple_remove(&cpu_ida, cpu_cdev->id);
	kfree(cpu_cdev->table);
	kfree(cpu_cdev);

	return 0;
}

#define FRAC_BITS 10
#define int_to_frac(x) ((x) << FRAC_BITS)
#define frac_to_int(x) ((x) >> FRAC_BITS)

#define SPRD_CPU_STORE(_name) \
	static ssize_t sprd_cpu_store_##_name(struct device *dev, \
			struct device_attribute *attr, \
			const char *buf, size_t count)
#define SPRD_CPU_SHOW(_name) \
	static ssize_t sprd_cpu_show_##_name(struct device *dev, \
			struct device_attribute *attr, \
			char *buf)

/* sys I/F for cooling device */
#define to_cooling_device(_dev)	\
	container_of(_dev, struct thermal_cooling_device, device)

#define SPRD_CPU_ATTR(_name) \
{ \
	.attr = { .name = #_name, .mode = 0664,}, \
	.show = sprd_cpu_show_##_name, \
	.store = sprd_cpu_store_##_name, \
}
#define SPRD_CPU_ATTR_RO(_name) \
{ \
	.attr = { .name = #_name, .mode = 0444, }, \
	.show = sprd_cpu_show_##_name, \
}
#define SPRD_CPU_ATTR_WO(_name) \
{ \
	.attr = { .name = #_name, .mode = 0220, }, \
	.store = sprd_cpu_store_##_name, \
}

SPRD_CPU_SHOW(min_freq);
SPRD_CPU_STORE(min_freq);
SPRD_CPU_SHOW(min_core_num);
SPRD_CPU_STORE(min_core_num);
SPRD_CPU_SHOW(max_ctrl_temp);
SPRD_CPU_STORE(max_ctrl_temp);
static struct device_attribute sprd_cpu_atrr[] = {
	SPRD_CPU_ATTR(min_freq),
	SPRD_CPU_ATTR(min_core_num),
	SPRD_CPU_ATTR(max_ctrl_temp),
};

static int sprd_cpu_creat_attr(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(sprd_cpu_atrr); i++) {
		rc = device_create_file(dev, &sprd_cpu_atrr[i]);
		if (rc)
			goto sprd_attrs_failed;
	}
	goto sprd_attrs_succeed;

sprd_attrs_failed:
	while (i--)
		device_remove_file(dev, &sprd_cpu_atrr[i]);

sprd_attrs_succeed:
	return rc;
}

static int sprd_cpu_remove_attr(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sprd_cpu_atrr); i++)
		device_remove_file(dev, &sprd_cpu_atrr[i]);
	return 0;
}

/**
 * mul_frac() - multiply two fixed-point numbers
 * @x:	first multiplicand
 * @y:	second multiplicand
 *
 * Return: the result of multiplying two fixed-point numbers.  The
 * result is also a fixed-point number.
 */
static inline s64 mul_frac(s64 x, s64 y)
{
	return (x * y) >> FRAC_BITS;
}

/* return (leak * 100)  */
static int get_cpu_static_power_coeff(int cluster_id)
{
	return cluster_data[cluster_id].leak_core_base;
}

/* return (leak * 100)  */
static int get_cache_static_power_coeff(int cluster_id)
{
	return cluster_data[cluster_id].leak_cluster_base;
}

static ssize_t sprd_cpu_show_min_freq(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct thermal_cooling_device *cdev = to_cooling_device(dev);
	struct cpu_cooling_device *cpu_cdev = cdev->devdata;
	int id = get_cluster_id(cpumask_any(&cpu_cdev->allowed_cpus));

	return sprintf(buf, "%u\n", cluster_data[id].min_cpufreq);
}

static ssize_t sprd_cpu_store_min_freq(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct thermal_cooling_device *cdev = to_cooling_device(dev);
	struct cpu_cooling_device *cpu_cdev = cdev->devdata;
	int id = get_cluster_id(cpumask_any(&cpu_cdev->allowed_cpus));
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	cluster_data[id].min_cpufreq = val;

	return count;

}

static ssize_t sprd_cpu_show_min_core_num(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct thermal_cooling_device *cdev = to_cooling_device(dev);
	struct cpu_cooling_device *cpu_cdev = cdev->devdata;
	int id = get_cluster_id(cpumask_any(&cpu_cdev->allowed_cpus));

	return sprintf(buf, "%u\n", cluster_data[id].min_cpunum);
}

static ssize_t sprd_cpu_store_min_core_num(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct thermal_cooling_device *cdev = to_cooling_device(dev);
	struct cpu_cooling_device *cpu_cdev = cdev->devdata;
	int id = get_cluster_id(cpumask_any(&cpu_cdev->allowed_cpus));
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	cluster_data[id].min_cpunum = val;

	return count;
}

static ssize_t sprd_cpu_show_max_ctrl_temp(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct thermal_cooling_device *cdev = to_cooling_device(dev);
	struct cpu_cooling_device *cpu_cdev = cdev->devdata;
	int id = get_cluster_id(cpumask_any(&cpu_cdev->allowed_cpus));

	return sprintf(buf, "%u\n", cluster_data[id].max_temp);
}

static ssize_t sprd_cpu_store_max_ctrl_temp(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct thermal_cooling_device *cdev = to_cooling_device(dev);
	struct cpu_cooling_device *cpu_cdev = cdev->devdata;
	int id = get_cluster_id(cpumask_any(&cpu_cdev->allowed_cpus));
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	cluster_data[id].max_temp = val;

	return count;
}

static int get_all_core_temp(int cluster_id, int cpu)
{
	int i, ret = 0;
	struct thermal_zone_device *tz = NULL;
	struct cluster_power_coefficients *cpc;

	cpc = &cluster_data[cluster_id];
	for (i = 0; i < (cpc->nsensor); i++) {
		tz = cpc->thm_zones[i];
		if (!tz || IS_ERR(tz) || !tz->ops->get_temp) {
			pr_err("get thermal zone failed\n");
			return -1;
		}

		ret = tz->ops->get_temp(tz, &(cpc->core_temp[cpu+i]));
		if (ret) {
			pr_err("get thermal %s temp failed\n", tz->type);
			return -1;
		}

		pr_debug("%s:%d\n", tz->type, cpc->core_temp[cpu+i]);
	}

	return ret;
}

static void get_core_temp(int cluster_id, int cpu, int *temp)
{
	struct cluster_power_coefficients *cpc;

	cpc = &cluster_data[cluster_id];
	*temp = cpc->core_temp[cpu];

}

static u64 get_core_dyn_power(int cluster_id,
	unsigned int freq_mhz, unsigned int voltage_mv)
{
	u64 power = 0;
	int voltage_base = cluster_data[cluster_id].core_coeff.voltage_base;
	int dyn_base = cluster_data[cluster_id].core_coeff.dynperghz;

	power = (u64)dyn_base * freq_mhz * voltage_mv * voltage_mv;
	do_div(power, voltage_base * voltage_base);
	do_div(power, 10000);

	pr_debug("cluster:%d core_dyn_p:%u freq:%u voltage:%u v_base:%d\n",
		cluster_id, (u32)power, freq_mhz, voltage_mv, voltage_base);

	return power;
}

static u32 get_cluster_min_cpufreq(int cluster_id)
{
	return cluster_data[cluster_id].min_cpufreq;
}

static u32 get_cluster_min_cpunum(int cluster_id)
{
	return cluster_data[cluster_id].min_cpunum;
}

static u32 get_cluster_max_temp(int cluster_id)
{
	return cluster_data[cluster_id].max_temp;
}

static u32 get_cluster_cycle(int cluster_id)
{
	return cluster_data[cluster_id].hotplug_period;
}

static u32 get_sensor_count(int cluster_id)
{
	return cluster_data[cluster_id].nsensor;
}

static u64 get_cluster_dyn_power(int cluster_id,
	unsigned int freq_mhz, unsigned int voltage_mv)
{
	u64 power = 0;
	int voltage_base = cluster_data[cluster_id].cluster_coeff.voltage_base;
	int dyn_base = cluster_data[cluster_id].cluster_coeff.dynperghz;

	power = (u64)dyn_base * freq_mhz * voltage_mv * voltage_mv;
	do_div(power, voltage_base * voltage_base);
	do_div(power, 10000);

	pr_debug("cluster:%d cluster_dyn_power:%u freq:%u voltage:%u voltage_base:%d\n",
		cluster_id, (u32)power, freq_mhz, voltage_mv, voltage_base);

	return power;
}

/*
 *Tscale = 0.0000825T^3 - 0.0117T^2 + 0.608T - 8.185
 * return Tscale * 1000
 */
static u64 get_cluster_temperature_scale(int cluster_id, unsigned long temp)
{
	u64 t_scale = 0;
	struct scale_coeff *coeff =
		&cluster_data[cluster_id].cluster_temp_scale;

	t_scale = coeff->scale_a * temp * temp * temp
		+ coeff->scale_b * temp * temp
		+ coeff->scale_c * temp
		+ coeff->scale_d;

	do_div(t_scale, 10000);

	return t_scale;
}

/*
 *Tscale = 0.0000825T^3 - 0.0117T^2 + 0.608T - 8.185
 * return Tscale * 1000
 */
static u64 get_core_temperature_scale(int cluster_id, unsigned long temp)
{
	u64 t_scale = 0;
	struct scale_coeff *coeff = &cluster_data[cluster_id].core_temp_scale;

	t_scale = coeff->scale_a * temp * temp * temp
		+ coeff->scale_b * temp * temp
		+ coeff->scale_c * temp
		+ coeff->scale_d;

	do_div(t_scale, 10000);

	return t_scale;
}

/*
 * Vscale = eV^3 + fV^2 + gV + h
 * Vscale = 33.31V^3 - 73.25V^2 + 54.44V - 12.81
 * return Vscale * 1000
 */
static u64 get_cluster_voltage_scale(int cluster_id, unsigned long u_volt)
{
	unsigned long m_volt = u_volt / 1000;
	u64 v_scale = 0;
	int cubic = 0, square = 0, common = 0, data = 0;
	struct scale_coeff *coeff =
		&cluster_data[cluster_id].cluster_voltage_scale;

	/* In order to ensure accuracy of data and data does not overflow.
	 * exam: cubic = eV^3 = e * mV^3 * 10^(-9)  For return Vscale * 1000,
	 * we use div 10^6.Because of parameter 'e' is bigger(10^2) than nomal
	 * In the last divided by 10^2.
	 */
	/* In order to avoid the computational problem caused by the error.*/
	cubic = (m_volt * m_volt * m_volt) / 1000000;
	cubic = coeff->scale_a * cubic;

	square = (m_volt * m_volt) / 1000;
	square = coeff->scale_b * square;

	common = coeff->scale_c * m_volt;

	data = coeff->scale_d * 1000;

	v_scale = (u64)(cubic + square + common + data);

	do_div(v_scale, 100);

	return v_scale;
}

/*
 * Vscale = eV^3 + fV^2 + gV + h
 * Vscale = 33.31V^3 - 73.25V^2 + 54.44V - 12.81
 * return Vscale * 1000
 */
static u64 get_core_voltage_scale(int cluster_id, unsigned long u_volt)
{
	unsigned long m_volt = u_volt / 1000;
	u64 v_scale = 0;
	int cubic = 0, square = 0, common = 0, data = 0;
	struct scale_coeff *coeff =
		&cluster_data[cluster_id].core_voltage_scale;

	/* In order to ensure accuracy of data and data does not overflow.
	 * exam: cubic = eV^3 = e * mV^3 * 10^(-9)  For return Vscale * 1000,
	 * we use div 10^6.Because of parameter 'e' is bigger(10^2) than nomal
	 * In the last divided by 10^2.
	 */
	/* In order to avoid the computational problem caused by the error.*/
	cubic = (m_volt * m_volt * m_volt) / 1000000;
	cubic = coeff->scale_a * cubic;

	square = (m_volt * m_volt) / 1000;
	square = coeff->scale_b * square;

	common = coeff->scale_c * m_volt;

	data = coeff->scale_d * 1000;

	v_scale = (u64)(cubic + square + common + data);

	do_div(v_scale, 100);

	return v_scale;
}

/* voltage in uV and temperature in mC */
static int get_static_power(cpumask_t *cpumask, int interval,
		unsigned long u_volt, u32 *power, int temperature)
{
	unsigned long core_t_scale, core_v_scale;
	unsigned long cluster_t_scale = 0, cluster_v_scale = 0;
	u32 cpu_coeff;
	u32 tmp_power = 0;
	int nr_cpus = cpumask_weight(cpumask);
	int cache_coeff = 0;
	int cluster_id =
		get_cluster_id(cpumask_any(cpumask));

	/* get coeff * 100 */
	cpu_coeff = get_cpu_static_power_coeff(cluster_id);
	/* get Tscale * 1000 */
	core_t_scale =
		get_core_temperature_scale(cluster_id, temperature / 1000);
	/* get Vscale * 1000 */
	core_v_scale =
		get_core_voltage_scale(cluster_id, u_volt);

	/* In order to avoid the computational problem caused by the error.*/
	if ((core_t_scale * core_v_scale) > 1000000) {
		tmp_power = (core_t_scale * core_v_scale) / 1000000;
		*power = (nr_cpus * cpu_coeff * tmp_power) / 100;
	} else {
		tmp_power = (core_t_scale * core_v_scale) / 100000;
		*power = (nr_cpus * cpu_coeff * tmp_power) / 1000;
	}

	if (nr_cpus) {
		/* get cluster-Tscale * 1000 */
		cluster_t_scale = get_cluster_temperature_scale(cluster_id,
							temperature / 1000);
		/* get cluster-Vscale * 1000 */
		cluster_v_scale = get_cluster_voltage_scale(cluster_id, u_volt);
		/* get coeff * 100 */
		cache_coeff = get_cache_static_power_coeff(cluster_id);
		if ((cluster_v_scale * cluster_t_scale) > 1000000) {
			tmp_power =
				(cluster_v_scale * cluster_t_scale) / 1000000;
			*power += (cache_coeff * tmp_power) / 100;
		} else {
			tmp_power =
				(cluster_v_scale * cluster_t_scale) / 100000;
			*power += (cache_coeff * tmp_power) / 1000;
		}
	}

	pr_debug("cluster:%d cpus:%d m_volt:%lu static_power:%u\n",
		cluster_id, nr_cpus, u_volt / 1000, *power);
	pr_debug("-->cpu_coeff:%d core_t_scale:%lu core_v_scale:%lu\n",
		cpu_coeff, core_t_scale, core_v_scale);
	pr_debug("-->cache_coeff:%d cluster_t_scale:%lu cluster_v_scale:%lu\n",
		cache_coeff, cluster_t_scale, cluster_v_scale);

	return 0;
}

/* voltage in uV and temperature in mC */
static int get_core_static_power(cpumask_t *cpumask, int interval,
		unsigned long u_volt, u32 *power, int temperature)
{
	unsigned long core_t_scale, core_v_scale;
	u32 cpu_coeff;
	u32 tmp_power = 0;
	int nr_cpus = cpumask_weight(cpumask);
	int cluster_id =
		get_cluster_id(cpumask_any(cpumask));

	/* get coeff * 100 */
	cpu_coeff = get_cpu_static_power_coeff(cluster_id);
	/* get Tscale * 1000 */
	core_t_scale =
		get_core_temperature_scale(cluster_id, temperature / 1000);
	/* get Vscale * 1000 */
	core_v_scale =
		get_core_voltage_scale(cluster_id, u_volt);

	/* In order to avoid the computational problem caused by the error.*/
	if ((core_t_scale * core_v_scale) > 1000000) {
		tmp_power = (core_t_scale * core_v_scale) / 1000000;
		*power = (nr_cpus * cpu_coeff * tmp_power) / 100;
	} else {
		tmp_power = (core_t_scale * core_v_scale) / 100000;
		*power = (nr_cpus * cpu_coeff * tmp_power) / 1000;
	}

	pr_debug("cluster:%d cpus:%d m_volt:%lu core_static_power:%u\n",
		cluster_id, nr_cpus, u_volt / 1000, *power);
	pr_debug("-->cpu_coeff:%d core_t_scale:%lu core_v_scale:%lu\n",
		cpu_coeff, core_t_scale, core_v_scale);

	return 0;
}
/* return (leakage * 10) */
static u64 get_leak_base(int cluster_id, int val, int *coeff)
{
	int i;
	u64 leak_base;

	if (cluster_id)
		leak_base = ((val>>16) & 0x1F) + 1;
	else
		leak_base = ((val>>11) & 0x1F) + 1;

	/* (LIT_LEAK[4:0]+1) x 2mA x 0.85V x 18.69% */
	for (i = 0; i < 3; i++)
		leak_base = leak_base * coeff[i];
	do_div(leak_base, 100000);

	return leak_base;
}

static int sprd_get_power_model_coeff(struct device_node *np,
		struct cluster_power_coefficients *power_coeff, int cluster_id)
{
	int ret;
	int val = 0;
	int efuse_block = -1;
	int efuse_switch = 0;
	int coeff[3];
	int count, i;

	if (!np) {
		pr_err("device node not found\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "sprd,efuse-block15", &efuse_block);
	if (ret) {
		pr_err("fail to get cooling devices efuse_block\n");
		efuse_block = -1;
	}

	ret = of_property_read_u32(np, "sprd,efuse-switch", &efuse_switch);
	if (ret)
		pr_err("fail to get cooling devices efuse_switch\n");

	if (efuse_switch) {
#if defined(CONFIG_OTP_SPRD_AP_EFUSE)
		if (efuse_block >= 0)
			val = sprd_ap_efuse_read(efuse_block);
#endif

		pr_debug("sci_efuse_leak --val : %x\n", val);
		if (val) {
			ret = of_property_read_u32_array(np,
					"sprd,leak-core", coeff, 3);
			if (ret) {
				pr_err("fail to get cooling devices leak-core-coeff\n");
				return -EINVAL;
			}

			power_coeff->leak_core_base =
				get_leak_base(cluster_id, val, coeff);

			ret = of_property_read_u32_array(np,
				"sprd,leak-cluster", coeff, 3);
			if (ret) {
				pr_err("fail to get cooling devices leak-cluster-coeff\n");
				return -EINVAL;
			}

			power_coeff->leak_cluster_base =
				get_leak_base(cluster_id, val, coeff);
		}
	}

	if (!val) {
		ret = of_property_read_u32(np, "sprd,core-base",
				&power_coeff->leak_core_base);
		if (ret) {
			pr_err("fail to get default cooling devices leak-core-base\n");
			return -EINVAL;
		}

		ret = of_property_read_u32(np, "sprd,cluster-base",
				&power_coeff->leak_cluster_base);
		if (ret) {
			pr_err("fail to get default cooling devices leak-cluster-base\n");
			return -EINVAL;
		}
	}

	ret = of_property_read_u32_array(np, "sprd,core-temp-scale",
			(int *)&power_coeff->core_temp_scale,
			sizeof(struct scale_coeff) / sizeof(int));
	if (ret) {
		pr_err("fail to get cooling devices core-temp-scale\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "sprd,core-voltage-scale",
			(int *)&power_coeff->core_voltage_scale,
			sizeof(struct scale_coeff) / sizeof(int));
	if (ret) {
		pr_err("fail to get cooling devices core-voltage-scale\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "sprd,cluster-temp-scale",
			(int *)&power_coeff->cluster_temp_scale,
			sizeof(struct scale_coeff) / sizeof(int));
	if (ret) {
		pr_err("fail to get cooling devices cluster-temp-scale\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "sprd,cluster-voltage-scale",
			(int *)&power_coeff->cluster_voltage_scale,
			sizeof(struct scale_coeff) / sizeof(int));
	if (ret) {
		pr_err("fail to get cooling devices cluster_voltage-scale\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "sprd,dynamic-core",
			(int *)&power_coeff->core_coeff,
			sizeof(struct dyn_power_coeff) / sizeof(int));
	if (ret) {
		pr_err("fail to get cooling devices dynamic-core-coeff\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "sprd,dynamic-cluster",
			(int *)&power_coeff->cluster_coeff,
			sizeof(struct dyn_power_coeff) / sizeof(int));
	if (ret) {
		pr_err("fail to get cooling devices dynamic-cluster-coeff\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np,
		"sprd,hotplug-period", &power_coeff->hotplug_period);
	if (ret)
		pr_err("fail to get cooling devices efuse_block\n");

	ret = of_property_read_u32(np,
		"sprd,min-cpufreq", &power_coeff->min_cpufreq);
	if (ret)
		pr_err("fail to get cooling devices min_cpufreq\n");

	ret = of_property_read_u32(np,
		"sprd,min-cpunum", &power_coeff->min_cpunum);
	if (ret)
		pr_err("fail to get cooling devices min_cpunum\n");

	ret = of_property_read_u32(np,
		"sprd,max-temp", &power_coeff->max_temp);
	if (ret)
		pr_err("fail to get cooling devices max_temp\n");

	power_coeff->nsensor = 0;
	count = of_property_count_strings(np, "sprd,sensor-names");
	if (count < 0) {
		pr_err("sensor names not found\n");
		return 0;
	}

	power_coeff->nsensor = count;
	for (i = 0; i < count; i++) {
		ret = of_property_read_string_index(np, "sprd,sensor-names",
			i, &power_coeff->sensor_names[i]);
		if (ret)
			pr_err("fail to get sensor-names\n");
	}

	for (i = 0; i < power_coeff->nsensor; i++) {
		power_coeff->thm_zones[i] =
		thermal_zone_get_zone_by_name(
			power_coeff->sensor_names[i]);
		if (IS_ERR(power_coeff->thm_zones[i])) {
			pr_err("get thermal zone %s failed\n",
					power_coeff->sensor_names[i]);
		}
	}

	return 0;
}

static int cpu_cooling_pm_notify(struct notifier_block *nb,
				unsigned long mode, void *_unused)
{
	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_RESTORE_PREPARE:
	case PM_SUSPEND_PREPARE:
		atomic_set(&in_suspend, 1);
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		atomic_set(&in_suspend, 0);
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block cpu_cooling_pm_nb = {
	.notifier_call = cpu_cooling_pm_notify,
};

static struct cpu_power_ops power_ops = {
		.get_core_dyn_power_p = get_core_dyn_power,
		.get_cluster_dyn_power_p = get_cluster_dyn_power,
		.get_static_power_p = get_static_power,
		.get_core_static_power_p = get_core_static_power,
		.get_cluster_min_cpufreq_p = get_cluster_min_cpufreq,
		.get_cluster_min_cpunum_p = get_cluster_min_cpunum,
		.get_cluster_max_temp_p = get_cluster_max_temp,
		.get_cluster_cycle_p = get_cluster_cycle,
		.get_sensor_count_p = get_sensor_count,
		.get_core_temp_p = get_core_temp,
		.get_all_core_temp_p = get_all_core_temp,
};

static int create_cpu_cooling_device(void)
{
	struct device_node *np, *child;
	int ret = 0;
	int cpu = 0;
	int result = 0;
	struct cpumask cpu_online_check;
	struct device *dev = NULL;
	struct thermal_cooling_device *cool_dev = NULL;
	struct cluster_power_coefficients *cluster = NULL;

	np = of_find_node_by_name(NULL, "cooling-devices");
	if (!np) {
		pr_err("unable to find thermal zones\n");
		return 0;
	}

	counts = of_get_child_count(np);

	cluster_data = kcalloc(counts,
		sizeof(struct cluster_power_coefficients), GFP_KERNEL);
	if (cluster_data == NULL) {
		ret = -ENOMEM;
		goto ERR_RET;
	}

	for_each_child_of_node(np, child) {
		int cluster_id_n;

		if (!of_device_is_available(child))
			continue;

		cluster_id_n = of_alias_get_id(child, "cooling-device");
		if (cluster_id_n == -ENODEV) {
			pr_err("fail to get cooling devices id\n");
			goto free_cluster;
		}
		cluster = &cluster_data[cluster_id_n];

		for_each_possible_cpu(cpu) {
			int cluster_id = get_cluster_id(cpu);

			if (cluster_id > counts) {
				pr_warn("cluster_id id: %d > %d\n",
					cluster_id, counts);
				ret = -ENODEV;
				goto free_cluster;
			} else if (cluster_id == cluster_id_n)
				cpumask_set_cpu(cpu, &cluster->clip_cpus);
		}

		if (!cpumask_and(&cpu_online_check,
				&cluster->clip_cpus, cpu_online_mask)) {
			pr_warn("%s cpu offline unnormal\n", __func__);
			continue;
		}

		ret = sprd_get_power_model_coeff(child,
			&cluster_data[cluster_id_n], cluster_id_n);
		if (ret) {
			pr_err("fail to get power model coeff !\n");
			goto free_cluster;
		}

		cool_dev = cpu_cooling_register(child,
						&cluster->clip_cpus,
						&power_ops);
		if (IS_ERR(cool_dev)) {
			pr_err("fail to register cooling device\n");
			continue;
		}
		cluster->cdev = cool_dev;

		for_each_cpu(cpu, &cluster->clip_cpus) {
			dev = get_cpu_device(cpu);
			if (!dev) {
				pr_err("No cpu device for cpu %d\n", cpu);
				continue;
			}
			if (dev_pm_opp_get_opp_count(dev) > 0)
				break;
		}

		if (cool_dev->devdata != NULL)
			sprd_cpu_creat_attr(&cool_dev->device);
		else {
			pr_err("No cpu cooling devices!\n");
		}
	}

	result = register_pm_notifier(&cpu_cooling_pm_nb);
	if (result)
		pr_warn("Thermal: Can not register suspend notifier, return %d\n",
			result);

	return ret;

free_cluster:
	kfree(cluster_data);
	cluster_data = NULL;
ERR_RET:
	return ret;
}

static int destroy_cpu_cooling_device(void)
{
	int id, ret = 0;
	struct thermal_cooling_device *cdev = NULL;
	struct cluster_power_coefficients *cluster = NULL;

	unregister_pm_notifier(&cpu_cooling_pm_nb);
	for (id = 0; id < counts; id++) {
		cluster = &cluster_data[id];
		cdev = cluster->cdev;
		if (cdev) {
			sprd_cpu_remove_attr(&cdev->device);
			ret = cpu_cooling_unregister(cdev);
			if (ret < 0) {
				pr_err("fail to unregister cpu cooling\n");
			}
		}
	}

	kfree(cluster_data);
	cluster_data = NULL;

	return 0;
}



static int __init sprd_cpu_cooling_device_init(void)
{
	return create_cpu_cooling_device();
}

static void __exit sprd_cpu_cooling_device_exit(void)
{
	destroy_cpu_cooling_device();
}

late_initcall(sprd_cpu_cooling_device_init);
module_exit(sprd_cpu_cooling_device_exit);

MODULE_DESCRIPTION("sprd cpu cooling driver");
MODULE_LICENSE("GPL v2");
