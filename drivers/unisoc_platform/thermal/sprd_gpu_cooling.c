// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020 Spreadtrum Communications Inc.

#define pr_fmt(fmt) "sprd_gpu_cooling: " fmt

#include <linux/devfreq_cooling.h>
#include <linux/thermal.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/printk.h>
#include <linux/unisoc_gpu_cooling.h>

#define GPU_CLUSTER_ID 0
#define GPU_CORE_NUM 1
#define NP_NAME_LEN 20


struct cluster_power_coefficients {
	struct thermal_cooling_device *gpu_cooling;
	char devname[NP_NAME_LEN];
	int weight;
	void *devdata;
};

static struct cluster_power_coefficients *cluster_data;
static struct thermal_zone_device *gpu_tz;

static struct thermal_cooling_device *
cluster_data_get_dev_by_name(const char *name)
{
	int i = 0;

	while (&cluster_data[i] != NULL) {
		if (!strncmp(name, cluster_data[i].devname, strlen(name)))
			return cluster_data[i].gpu_cooling;
		i++;
	}

	return NULL;
}

int create_gpu_cooling_device(struct devfreq *gpudev, u64 *mask)
{
	struct device_node *np, *child;
	struct thermal_cooling_device *devfreq_cooling;
	int cluster_count;
	int ret = 0;

	if (gpudev == NULL || mask == NULL) {
		pr_err("params is not complete!\n");
		return -ENODEV;
	}

	np = of_find_node_by_name(NULL, "gpu-cooling-devices");
	if (!np) {
		pr_err("unable to find thermal zones\n");
		return -ENODEV;
	}

	cluster_count = of_get_child_count(np);

	cluster_data = kcalloc(cluster_count,
		sizeof(struct cluster_power_coefficients), GFP_KERNEL);
	if (!cluster_data)
		return -ENOMEM;

	for_each_child_of_node(np, child) {
		int cluster_id;

		if (!of_device_is_compatible(child, "sprd,mali-power-model")) {
			pr_err("power_model incompatible\n");
			ret = -ENODEV;
			goto free_cluster;
		}

		/* Check whether child is enabled or not */
		if (!of_device_is_available(child))
			continue;

		cluster_id = of_alias_get_id(child, "gpu-cooling");
		if (cluster_id == -ENODEV) {
			pr_err("fail to get cooling devices id\n");
			ret = -ENODEV;
			goto free_cluster;
		}

		devfreq_cooling = devfreq_cooling_em_register(gpudev, NULL);

		if (IS_ERR_OR_NULL(devfreq_cooling)) {
			ret = PTR_ERR(devfreq_cooling);
			pr_err("fail to register cool-dev (%d)\n", ret);
			goto free_cluster;
		}

		strlcpy(cluster_data[cluster_id].devname,
			child->name, strlen(child->name));
		cluster_data[cluster_id].gpu_cooling = devfreq_cooling;

		gpu_tz = thermal_zone_get_zone_by_name("gpu-thmzone");
	}

	return ret;

free_cluster:
	kfree(cluster_data);
	cluster_data = NULL;

	return ret;
}
EXPORT_SYMBOL_GPL(create_gpu_cooling_device);

int destroy_gpu_cooling_device(void)
{
	struct device_node *np, *child;

	np = of_find_node_by_name(NULL, "gpu-cooling-devices");
	if (!np) {
		pr_err("unable to find thermal zones\n");
		return -ENODEV;
	}

	for_each_child_of_node(np, child) {
		struct thermal_cooling_device *cdev;

		cdev = cluster_data_get_dev_by_name(child->name);
		if (IS_ERR(cdev))
			continue;
		devfreq_cooling_unregister(cdev);
	}

	kfree(cluster_data);
	cluster_data = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(destroy_gpu_cooling_device);

static int __init sprd_gpu_cooling_device_init(void)
{
	return 0;
}

static void __exit sprd_gpu_cooling_device_exit(void)
{
}

late_initcall(sprd_gpu_cooling_device_init);
module_exit(sprd_gpu_cooling_device_exit);

MODULE_DESCRIPTION("sprd gpu cooling driver");
MODULE_LICENSE("GPL v2");

