// SPDX-License-Identifier: GPL-2.0-only
/*
 * SPRD thermal control
 *
 * Copyright (C) 2022 Unisoc corporation. http://www.unisoc.com
 */

#define pr_fmt(fmt) "sprd_thm_ctl: " fmt

#include <linux/cpu_cooling.h>
#include <linux/cpufreq.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/thermal.h>
#include <trace/hooks/thermal.h>
#include <trace/events/power.h>

#define MAX_CLUSTER_NUM	3
#define FTRACE_CLUS0_LIMIT_FREQ_NAME "thermal-cpufreq-0-limit"
#define FTRACE_CLUS1_LIMIT_FREQ_NAME "thermal-cpufreq-1-limit"
#define FTRACE_CLUS2_LIMIT_FREQ_NAME "thermal-cpufreq-2-limit"

static unsigned int thm_enable = 1;
static unsigned int user_power_range;
static struct thermal_zone_device *soc_tz;

struct sprd_thermal_ctl {
	struct cpufreq_policy	*policy;
	int			cluster_id;
	struct list_head	node;
};

static LIST_HEAD(thermal_policy_list);

static void unisoc_thermal_register(void *data, struct cpufreq_policy *policy)
{
	struct sprd_thermal_ctl *thm_ctl;

	if (!policy) {
		pr_err("Failed to get policy\n");
		return;
	}

	if (!policy->cdev) {
		pr_err("Failed to get cdev\n");
		return;
	}

	list_for_each_entry(thm_ctl, &thermal_policy_list, node)
		if (thm_ctl->policy == policy)
			return;

	list_for_each_entry(thm_ctl, &thermal_policy_list, node)
		if (thm_ctl->policy == NULL) {
			thm_ctl->policy = policy;
			pr_info("Success to get policy for cdev%d\n", policy->cdev->id);
			break;
		}
}

static void unisoc_thermal_unregister(void *data, struct cpufreq_policy *policy)
{
	struct sprd_thermal_ctl *thm_ctl;

	list_for_each_entry(thm_ctl, &thermal_policy_list, node) {
		if (thm_ctl->policy == policy) {
			thm_ctl->policy = NULL;
			break;
		}
	}
}

/* thermal power throttle control */
static void unisoc_enable_thermal_power_throttle(void *data, bool *enable, bool *override)
{
	if (!thm_enable)
		*enable = false;

	if (user_power_range)
		*override = true;
}

/* modify IPA power_range by user_power_range */
static void unisoc_thermal_power_cap(void *data, unsigned int *power_range)
{
	if (user_power_range && user_power_range < *power_range)
		*power_range = user_power_range;
}

/* modify thermal target frequency */
static inline
struct cpufreq_policy *find_next_cpufreq_policy(struct cpufreq_policy *curr)
{
	struct sprd_thermal_ctl *thm_ctl;

	list_for_each_entry(thm_ctl, &thermal_policy_list, node) {
		if (!thm_ctl->policy)
			continue;
		if ((curr->cdev->id + 1) == thm_ctl->policy->cdev->id)
			return thm_ctl->policy;
	}
	return NULL;
}

static void unisoc_modify_thermal_target_freq(void *data,
		struct cpufreq_policy *policy, unsigned int *target_freq)
{
	unsigned int curr_max_freq;
	struct cpufreq_policy *cpufreq_policy_find;
	struct thermal_cooling_device *cdev = policy->cdev;

	if (IS_ERR_OR_NULL(cdev)) {
		pr_err("Failed to get current policy->cdev.\n");
		return;
	}

	curr_max_freq = cpufreq_quick_get_max(policy->cpu);

	cpufreq_policy_find = find_next_cpufreq_policy(policy);
	if (cpufreq_policy_find && curr_max_freq > *target_freq)
		if (cpufreq_policy_find->max != cpufreq_policy_find->min)
			*target_freq = curr_max_freq;

	/* Debug info for cpufreq cooling device */
	pr_info("cpu%d temp:%d target_freq:%u\n", policy->cpu, soc_tz->temperature, *target_freq);

	switch (cdev->id) {
	case 0:
		trace_clock_set_rate(FTRACE_CLUS0_LIMIT_FREQ_NAME,
			*target_freq, smp_processor_id());
		break;

	case 1:
		trace_clock_set_rate(FTRACE_CLUS1_LIMIT_FREQ_NAME,
			*target_freq, smp_processor_id());
		break;

	case 2:
		trace_clock_set_rate(FTRACE_CLUS2_LIMIT_FREQ_NAME,
			*target_freq, smp_processor_id());
		break;

	default:
		break;
	}
}

/* modify request frequency */
static void unisoc_modify_thermal_request_freq(void *data,
		struct cpufreq_policy *policy, unsigned long *request_freq)
{
	*request_freq = cpufreq_quick_get_max(policy->cpu);
}

/* thermal temperature debug */
static int thermal_temp_debug(struct thermal_zone_device *tz)
{
	int crit_temp = 0, warn_temp;
	int ret = -EPERM;
	int count;
	enum thermal_trip_type type;

	for (count = 0; count < tz->trips; count++) {
		ret = tz->ops->get_trip_type(tz, count, &type);
		if (!ret && type == THERMAL_TRIP_CRITICAL) {
			ret = tz->ops->get_trip_temp(tz, count,
				&crit_temp);
			warn_temp = crit_temp - 5000;
			if (!ret && tz->temperature > warn_temp)
				pr_alert_ratelimited("tz id=%d type=%s temperature reached %d\n",
					tz->id, tz->type, tz->temperature);
			break;
		}
	}

	return ret;
}

static void unisoc_get_thermal_zone_device(void *data, struct thermal_zone_device *tz)
{
	thermal_temp_debug(tz);
}

/* sysfs attribute create */
static ssize_t
thm_enable_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", thm_enable);
}

static ssize_t
thm_enable_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	u32 enable;

	if (kstrtou32(buf, 10, &enable) || enable < 0 || enable > 1)
		return -EINVAL;

	thm_enable = enable;

	return count;
}

static ssize_t
user_power_range_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", user_power_range);
}

static ssize_t
user_power_range_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	u32 power_value;

	if (kstrtou32(buf, 10, &power_value) || power_value < 0)
		return -EINVAL;

	user_power_range = power_value;

	return count;
}

static DEVICE_ATTR_RW(thm_enable);
static DEVICE_ATTR_RW(user_power_range);

static int sprd_thermal_ctl_init(void)
{
	int i, ret;
	struct sprd_thermal_ctl *thm_ctl, *tmp_thm_ctl;

	for (i = 0; i < MAX_CLUSTER_NUM; i++) {
		thm_ctl = kzalloc(sizeof(*thm_ctl), GFP_KERNEL);

		if (thm_ctl) {
			thm_ctl->policy = NULL;
			list_add(&thm_ctl->node, &thermal_policy_list);
		} else {
			goto fail;
		}
	}

	soc_tz = thermal_zone_get_zone_by_name("soc-thmzone");
	if (IS_ERR(soc_tz)) {
		pr_err("Failed to get soc thermal zone\n");
		goto fail;
	}

	ret = device_create_file(&soc_tz->device, &dev_attr_thm_enable);
	if (ret) {
		pr_err("Failed to create thm_enable\n");
		goto fail;
	}

	ret = device_create_file(&soc_tz->device, &dev_attr_user_power_range);
	if (ret) {
		pr_err("Failed to create user_power_range\n");
		goto remove_file;
	}

	register_trace_android_vh_thermal_register(unisoc_thermal_register, NULL);
	register_trace_android_vh_thermal_unregister(unisoc_thermal_unregister, NULL);
	register_trace_android_vh_enable_thermal_power_throttle(unisoc_enable_thermal_power_throttle, NULL);
	register_trace_android_vh_thermal_power_cap(unisoc_thermal_power_cap, NULL);
	register_trace_android_vh_modify_thermal_target_freq(unisoc_modify_thermal_target_freq, NULL);
	register_trace_android_vh_modify_thermal_request_freq(unisoc_modify_thermal_request_freq, NULL);
	register_trace_android_vh_get_thermal_zone_device(unisoc_get_thermal_zone_device, NULL);

	return 0;

remove_file:
	device_remove_file(&soc_tz->device, &dev_attr_thm_enable);

fail:
	list_for_each_entry_safe(thm_ctl, tmp_thm_ctl, &thermal_policy_list, node) {
		list_del(&thm_ctl->node);
		kfree(thm_ctl);
	}

	return -ENOMEM;
}

static void sprd_thermal_ctl_exit(void)
{
	struct sprd_thermal_ctl *thm_ctl, *tmp_thm_ctl;

	unregister_trace_android_vh_thermal_register(unisoc_thermal_register, NULL);
	unregister_trace_android_vh_thermal_unregister(unisoc_thermal_unregister, NULL);

	list_for_each_entry_safe(thm_ctl, tmp_thm_ctl, &thermal_policy_list, node) {
		list_del(&thm_ctl->node);
		kfree(thm_ctl);
	}

	unregister_trace_android_vh_enable_thermal_power_throttle(unisoc_enable_thermal_power_throttle, NULL);
	unregister_trace_android_vh_thermal_power_cap(unisoc_thermal_power_cap, NULL);
	unregister_trace_android_vh_modify_thermal_target_freq(unisoc_modify_thermal_target_freq, NULL);
	unregister_trace_android_vh_modify_thermal_request_freq(unisoc_modify_thermal_request_freq, NULL);
	unregister_trace_android_vh_get_thermal_zone_device(unisoc_get_thermal_zone_device, NULL);

	device_remove_file(&soc_tz->device, &dev_attr_thm_enable);
	device_remove_file(&soc_tz->device, &dev_attr_user_power_range);

}

module_init(sprd_thermal_ctl_init);
module_exit(sprd_thermal_ctl_exit);

MODULE_DESCRIPTION("for sprd thermal control");
MODULE_LICENSE("GPL");
