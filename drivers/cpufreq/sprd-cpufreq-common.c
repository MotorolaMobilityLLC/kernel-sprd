/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
 #define pr_fmt(fmt)  "sprd_cpufreq: " fmt

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of_platform.h>
#include <linux/compiler.h>
#include "sprd-cpufreq-common.h"
#include "sprd-cpufreqhw.h"

struct sprd_cpufreq_driver_data *cpufreq_datas[SPRD_CPUFREQ_MAX_MODULE];

__weak struct sprd_cpudvfs_device *sprd_hardware_dvfs_device_get(void)
{
	return NULL;
}

/* Initializes OPP tables based on old-deprecated bindings */
int dev_pm_opp_of_add_table_binning(int cluster,
				    struct device *dev,
				    struct device_node *np_cpufreq_data,
				 struct sprd_cpufreq_driver_data *cpufreq_data)
{
	const struct property *prop = NULL, *prop1 = NULL;
	struct sprd_cpudvfs_device *pdevice;
	struct sprd_cpudvfs_ops *driver;
	char opp_string[30] = "operating-points";
	int count = 0;
	const __be32 *val;
	int nr;

	if ((!dev && !np_cpufreq_data) || !cpufreq_data) {
		pr_err("empty input parameter\n");
		return -ENOENT;
	}

	if (dev)
		prop = of_find_property(dev->of_node, opp_string, NULL);
	prop1 = of_find_property(np_cpufreq_data, opp_string, NULL);
	if (!prop && !prop1)
		return -ENODEV;
	if (prop && !prop->value)
		return -ENODATA;
	if (prop1 && !prop1->value)
		return -ENODATA;
	if (prop1)
		prop = prop1;
	/*
	 * Each OPP is a set of tuples consisting of frequency and
	 * voltage like <freq-kHz vol-uV>.
	 */
	nr = prop->length / sizeof(u32);
	if (nr % 2) {
		dev_err(dev, "Invalid OPP list\n");
		return -EINVAL;
	}

	cpufreq_data->freqvolts = 0;
	val = prop->value;
	while (nr) {
		unsigned long freq = be32_to_cpup(val++) * 1000;
		unsigned long volt = be32_to_cpup(val++);

		if (dev)
			dev_pm_opp_remove(dev, freq);

		if (dev && dev_pm_opp_add(dev, freq, volt)) {
			dev_warn(dev, "dev_pm Failed to add OPP %ld\n", freq);
		} else {
			if (freq / 1000 > cpufreq_data->temp_max_freq)
				cpufreq_data->temp_max_freq = freq / 1000;
		}
		if (count < SPRD_CPUFREQ_MAX_FREQ_VOLT) {
			cpufreq_data->freqvolt[count].freq = freq;
			cpufreq_data->freqvolt[count].volt = volt;
			cpufreq_data->freqvolts++;
		}

		count++;
		nr -= 2;
	}

	pdevice = sprd_hardware_dvfs_device_get();

	if (!pdevice)
		return 0;

	driver = &pdevice->ops;

	if (!driver->probed || !driver->opp_add) {
		pr_err("driver opertions is empty\n");
		return -EINVAL;
	}

	if (!driver->probed(pdevice->archdata, cluster)) {
		pr_err("the cpu dvfs device has not been probed\n");
		return -EINVAL;
	}

	while (count-- > 0)
		driver->opp_add(pdevice->archdata, cluster,
				cpufreq_data->freqvolt[count].freq,
				cpufreq_data->freqvolt[count].volt,
				cpufreq_data->freqvolts - 1 - count);

	return 0;
}

static int dev_pm_opp_of_add_table_binning_slave(
	struct sprd_cpufreq_driver_data *c_host,
	int temp_now)
{
	struct device_node *np = NULL, *np_host = NULL;
	unsigned int cluster = SPRD_CPUFREQ_MAX_MODULE;
	struct device_node *cpu_np = NULL;
	struct device *cpu_dev;
	int ret = 0, i;

	if (!c_host) {
		pr_err("dvfs host device is NULL\n");
		return -ENODEV;
	}

	if (c_host->sub_cluster_bits == 0)
		return 0;

	cpu_dev = c_host->cpu_dev;
	if (!cpu_dev) {
		pr_err("cpu device is null.\n");
		return -ENODEV;
	}
	cpu_np = of_node_get(cpu_dev->of_node);
	if (!cpu_np) {
		pr_err("failed to find cpu node\n");
		return -ENOENT;
	}

	np_host = of_parse_phandle(cpu_np, "cpufreq-data-v1", 0);
	if (!np_host) {
		pr_err("Can not find cpufreq node in dts\n");
		of_node_put(cpu_np);
		return -ENOENT;
	}

	for (i = 0; i < SPRD_CPUFREQ_MAX_MODULE; i++) {
		np = of_parse_phandle(np_host, "cpufreq-sub-clusters", i);
		if (!np) {
			pr_debug("index %d not found sub-clusters\n", i);
			goto free_np;
		}
		pr_debug("slave index %d name is found %s\n", i, np->full_name);

		if (of_property_read_u32(np, "cpufreq-cluster-id", &cluster)) {
			pr_err("index %d not found cpufreq_custer_id\n", i);
			ret = -ENODEV;
			goto free_np;
		}
		if (cluster >= SPRD_CPUFREQ_MAX_MODULE ||
		    !cpufreq_datas[cluster]) {
			pr_err("index %d cluster %u is NULL\n", i, cluster);
			continue;
		}
		cpufreq_datas[cluster]->temp_now = temp_now;
		ret = dev_pm_opp_of_add_table_binning(cluster,
						      NULL,
						      np,
						      cpufreq_datas[cluster]);
		if (ret)
			goto free_np;
	}

free_np:
	if (np)
		of_node_put(np);
	if (np_host)
		of_node_put(np_host);
	return ret;
}

/**
 * sprd_cpufreq_update_opp() - returns the max freq of a cpu
 * and update dvfs table by temp_now
 * @cpu: which cpu you want to update dvfs table
 * @temp_now: current temperature on this cpu, mini-degree.
 *
 * Return:
 * 1.cluster is not working, then return 0
 * 2.succeed to update dvfs table
 * then return max freq(KHZ) of this cluster
 */
unsigned int sprd_cpufreq_update_opp(int cpu, int temp_now)
{
	struct sprd_cpufreq_driver_data *data;
	unsigned int max_freq = 0;
	int cluster;

	temp_now = temp_now / 1000;
	if (temp_now <= SPRD_CPUFREQ_TEMP_MIN ||
	    temp_now >= SPRD_CPUFREQ_TEMP_MAX)
		return 0;

	cluster = topology_physical_package_id(cpu);
	if (cluster > SPRD_CPUFREQ_MAX_CLUSTER) {
		pr_err("cpu%d is overflowd %d\n", cpu,
		       SPRD_CPUFREQ_MAX_CLUSTER);
		return -EINVAL;
	}

	data = cpufreq_datas[cluster];

	if (data && data->online && data->temp_max > 0) {
		/* Never block IPA thread */
		if (!mutex_trylock(data->volt_lock))
			return 0;
		data->temp_now = temp_now;
		if (temp_now < data->temp_bottom && !data->temp_fall_time)
			data->temp_fall_time = jiffies +
					       SPRD_CPUFREQ_TEMP_FALL_HZ;
		if (temp_now >= data->temp_bottom)
			data->temp_fall_time = 0;
		if (temp_now >= data->temp_top || (data->temp_fall_time &&
				time_after(jiffies, data->temp_fall_time))) {
			 /* if fails to update slave dvfs table,
			  * never update any more this time,
			  * try to update slave and host dvfs table next time,
			  * because once host dvfs table is updated,
			  * slave dvfs table can not be update here any more.
			  */
			if (!dev_pm_opp_of_add_table_binning_slave(data,
								   temp_now)) {
				data->temp_fall_time = 0;
				if (!dev_pm_opp_of_add_table_binning(
				    data->cluster, data->cpu_dev, NULL, data))
					max_freq = data->temp_max_freq;
				dev_info(data->cpu_dev,
					 "update temp_max_freq %u\n", max_freq);
			}
		}
		mutex_unlock(data->volt_lock);
	}

	return max_freq;
}
EXPORT_SYMBOL_GPL(sprd_cpufreq_update_opp);

static int sprd_cpufreq_cpuhp_online(unsigned int cpu)
{
	unsigned int olcpu = 0;
	struct sprd_cpufreq_driver_data *c;

	if (!is_big_cluster(cpu))
		return NOTIFY_DONE;

	for_each_online_cpu(olcpu)
		if (is_big_cluster(olcpu))
			return NOTIFY_DONE;

	c = sprd_cpufreq_data(cpu);
	if (c && c->cpufreq_online)
		c->cpufreq_online(cpu);
	else
		pr_debug("get c->cpufreq_online pointer failed!\n");
	return NOTIFY_DONE;
}

static int sprd_cpufreq_cpuhp_offline(unsigned int cpu)
{
	unsigned int olcpu = 0;
	struct sprd_cpufreq_driver_data *c;

	if (!is_big_cluster(cpu))
		return NOTIFY_DONE;

	for_each_online_cpu(olcpu)
		if (is_big_cluster(olcpu))
			return NOTIFY_DONE;

	c = sprd_cpufreq_data(cpu);
	if (c && c->cpufreq_offline)
		c->cpufreq_offline(cpu);
	else
		pr_debug("get c->cpufreq_offline pointer failed!\n");
	return NOTIFY_DONE;
}

/* Cpufreq hotplug setup */

int sprd_cpufreq_cpuhp_setup(void)
{
	return cpuhp_setup_state(CPUHP_BP_PREPARE_DYN,
				"cpuhotplug/sprd_cpufreq_cpuhp:online",
				sprd_cpufreq_cpuhp_online,
				sprd_cpufreq_cpuhp_offline);
}
