// SPDX-License-Identifier: GPL-2.0
//
// Unisoc cpufreq driver public data
//
// Copyright (C) 2021 Unisoc, Inc.

#include "sprd-cpufreq-common.h"
#include "sprd-cpufreq-v2.h"

unsigned int sprd_cpufreq_update_normal(struct cpufreq_policy policy,
					    int cpu, int temp)
{
	struct cluster_info *info = NULL;

	info = policy.driver_data;
	if (IS_ERR_OR_NULL(info) || !info->update_opp)
		return 0;

	return info->update_opp(cpu, temp);
}

unsigned int sprd_cpufreq_update_common(struct cpufreq_policy policy,
					    int cpu, int temp)
{
	struct sprd_cpufreq_driver_data *data = NULL;

	data = policy.driver_data;
	if (IS_ERR_OR_NULL(data) || !data->update_opp)
		return 0;

	return data->update_opp(cpu, temp);
}

unsigned int sprd_cpufreq_update_opp(int cpu, int temp_now)
{
	struct device_node *cpu_np, *cpufreq_np;
	struct device *cpu_dev;
	struct cpufreq_policy policy;
	unsigned int max_freq = 0;

	if (cpufreq_get_policy(&policy, 0)) {
		pr_debug("%s: No cpu data found\n", __func__);
		return 0;
	}

	cpu_dev = get_cpu_device(0);
	if (!cpu_dev) {
		pr_err("%s: Failed to get cpu0 device\n", __func__);
		return 0;
	}

	cpu_np = of_node_get(cpu_dev->of_node);
	if (!cpu_np) {
		pr_err("%s: Failed to find cpu node\n", __func__);
		return 0;
	}

	cpufreq_np = of_parse_phandle(cpu_np, "cpufreq-data-v1", 0);
	if (!cpufreq_np) {
		pr_err("%s: No cpufreq data found for cpu0\n", __func__);
		of_node_put(cpu_np);
		return 0;
	}

	pr_debug("%s: cluster name is: %s\n", __func__, cpufreq_np->full_name);

	if (!strcmp(cpufreq_np->full_name, "cpufreq-cluster0"))
		max_freq = sprd_cpufreq_update_normal(policy, cpu, temp_now);
	else if (!strcmp(cpufreq_np->full_name, "cpufreq-clus0"))
		max_freq = sprd_cpufreq_update_common(policy, cpu, temp_now);
	else
		pr_err("%s: Error name of cpufreq data v1!\n", __func__);

	of_node_put(cpufreq_np);
	of_node_put(cpu_np);

	return max_freq;
}
EXPORT_SYMBOL_GPL(sprd_cpufreq_update_opp);

MODULE_LICENSE("GPL v2");
