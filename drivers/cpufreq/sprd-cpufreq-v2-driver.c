// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2021 Unisoc, Inc.
#define pr_fmt(fmt) "sprd-apcpu-dvfs: " fmt

#include "sprd-cpufreq-v2.h"
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/nvmem-consumer.h>
#include <linux/pm_qos.h>
#include <linux/thermal.h>

#define low_check(temp)		((int)(temp) < (int)DVFS_TEMP_LOW_LIMIT)
#define upper_check(temp)	((int)(temp) >= (int)DVFS_TEMP_UPPER_LIMIT)
#define temp_check(temp)	(low_check(temp) || upper_check(temp))

#define ON_BOOST		0
#define OUT_BOOST		1
#define SPRD_CPUFREQ_BOOST_DURATION	(60ul * HZ)

struct cluster_prop {
	char *name;
	u32 *value;
	void **ops;
};

static struct device *dev;
static struct cluster_info *pclusters;
static unsigned long boot_done_timestamp;

/* cluster common interface */
static int sprd_cluster_info(u32 cpu_idx)
{
	struct device_node *cpu_np;
	struct of_phandle_args args;
	int ret, index;

	if (cpu_idx >= nr_cpu_ids)
		return -EINVAL;

	cpu_np = of_cpu_device_node_get(cpu_idx);
	if (!cpu_np)
		return -EINVAL;

	ret = of_parse_phandle_with_args(cpu_np, "sprd,freq-domain",
					 "#freq-domain-cells", 0, &args);
	of_node_put(cpu_np);
	if (ret)
		return -EINVAL;

	index = args.args[0];

	return index;
}

int sprd_cluster_num(void)
{
	unsigned int cpu_max = num_possible_cpus() - 1;

	return sprd_cluster_info(cpu_max) + 1;
}

static int sprd_cpufreq_boost_judge(struct cpufreq_policy *policy)
{
	struct cluster_info *cluster = policy->driver_data;

	if (time_after(jiffies, boot_done_timestamp)) {
		cluster->boost_enable = false;
		pr_info("Disables boost it is %lu seconds after boot up\n",
			SPRD_CPUFREQ_BOOST_DURATION / HZ);
	}

	if (cluster->boost_enable) {
		if (policy->max >= policy->cpuinfo.max_freq)
			return ON_BOOST;
		cluster->boost_enable = false;
		pr_info("Disables boost due to policy max(%d<%d)\n",
			policy->max, policy->cpuinfo.max_freq);
	}

	return OUT_BOOST;
}

static int sprd_nvmem_info_read(struct device_node *node, const char *name, u32 *value)
{
	struct nvmem_cell *cell;
	void *buf;
	size_t len = 0;
	int ret = 0;

	cell = of_nvmem_cell_get(node, name);
	if (IS_ERR(cell)) {
		ret = PTR_ERR(cell);
		if (ret == -EPROBE_DEFER)
			pr_warn("cell for cpufreq not ready, retry\n");
		else
			pr_err("failed to get cell for cpufreq\n");

		return ret;
	}

	buf = nvmem_cell_read(cell, &len);
	if (IS_ERR(buf)) {
		nvmem_cell_put(cell);
		return PTR_ERR(buf);
	}

	if (len > sizeof(u32))
		len = sizeof(u32);

	memcpy(value, buf, len);

	kfree(buf);
	nvmem_cell_put(cell);

	return 0;
}

static int sprd_temp_list_init(struct list_head *head)
{
	struct temp_node *node;

	if (!list_empty_careful(head)) {
		dev_warn(dev, "%s: temp list is also init\n", __func__);
		return 0;
	}

	/* add upper temp node */
	node = devm_kzalloc(dev, sizeof(*node), GFP_KERNEL);
	if (!node) {
		dev_err(dev, "%s: alloc for upper temp node error\n", __func__);
		return -ENOMEM;
	}

	node->temp = DVFS_TEMP_UPPER_LIMIT;

	list_add(&node->list, head);

	/* add low temp node */
	node = devm_kzalloc(dev, sizeof(*node), GFP_KERNEL);
	if (!node) {
		dev_err(dev, "%s: alloc for low temp node error\n", __func__);
		return -ENOMEM;
	}

	node->temp = DVFS_TEMP_LOW_LIMIT;

	list_add(&node->list, head);

	return 0;
}

static int sprd_temp_list_add(struct list_head *head, int temp)
{
	struct temp_node *node, *pos;

	if (temp_check(temp) || list_empty_careful(head)) {
		dev_err(dev, "%s: temp %d or list is error\n", __func__, temp);
		return -EINVAL;
	}

	list_for_each_entry(pos, head, list) {
		if (temp == pos->temp)
			return 0;
		if (temp < pos->temp)
			break;
	}

	node = devm_kzalloc(dev, sizeof(*node), GFP_KERNEL);
	if (!node) {
		dev_err(dev, "%s: alloc for temp node error\n", __func__);
		return -ENOMEM;
	}

	node->temp = temp;

	list_add_tail(&node->list, &pos->list);

	return 0;
}

static struct temp_node *sprd_temp_list_find(struct list_head *head, int temp)
{
	struct temp_node *pos, *next;

	if (temp_check(temp)) {
		dev_err(dev, "%s: temp %d is out of range\n", __func__, temp);
		return NULL;
	}

	list_for_each_entry(pos, head, list) {
		next = list_entry(pos->list.next, typeof(*pos), list);
		if ((temp >= pos->temp) && (temp < next->temp))
			break;
	}

	return pos;
}

static int sprd_policy_table_update(struct cpufreq_policy *policy, struct temp_node *node)
{
	struct cpufreq_frequency_table *old_table, *new_table __maybe_unused;
	struct cluster_info *cluster;
	struct device *cpu;
	struct dev_pm_opp *opp;
	unsigned long rate = 0;
	u64 freq, vol;
	int i, ret;

	cpu = get_cpu_device(policy->cpu);
	if (!cpu) {
		dev_err(dev, "%s: get cpu %u dev error\n", __func__, policy->cpu);
		return -EINVAL;
	}

	cluster = (struct cluster_info *)policy->driver_data;
	if (!cluster || !cluster->table_update || !cluster->pair_get) {
		dev_err(dev, "%s: get cpu %u cluster info error\n", __func__, policy->cpu);
		return -EINVAL;
	}

	dev_info(dev, "%s: update cluster %u temp %d dvfs table\n", __func__, cluster->id, node->temp);

	ret = cluster->table_update(cluster->id, node->temp, &cluster->table_entry_num);
	if (ret) {
		dev_err(dev, "%s: update cluster %u temp %d table error\n", __func__, cluster->id, node->temp);
		return -EINVAL;
	}

	dev_dbg(dev, "%s: cluster %u dvfs table entry num is %u\n", __func__, cluster->id, cluster->table_entry_num);

	old_table = policy->freq_table;
	if (old_table) {
		while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(cpu, &rate))) {
			dev_pm_opp_put(opp);
			dev_pm_opp_remove(cpu, rate);
			rate++;
		}
	}

	dev_info(dev, "%s: update cluster %u opp\n", __func__, cluster->id);

	for (i = 0; i < cluster->table_entry_num; ++i) {
		ret = cluster->pair_get(cluster->id, i, &freq, &vol);
		if (ret) {
			dev_err(dev, "%s: get cluster %u index %u pair error\n", __func__, cluster->id, i);
			return -EINVAL;
		}

		dev_info(dev, "%s: add %lluHz/%lluuV to opp\n", __func__, freq, vol);

		ret = dev_pm_opp_add(cpu, freq, vol);
		if (ret) {
			dev_err(dev, "%s: add %lluHz/%lluuV pair to opp error(%d)\n", __func__, freq, vol, ret);
			return -EINVAL;
		}
	}

	if (node->temp_table)
		policy->freq_table = node->temp_table;
	else {
		ret = dev_pm_opp_init_cpufreq_table(cpu, &new_table);
		if (ret) {
			dev_err(dev, "%s: init cluster %u freq table error(%d)\n", __func__, cluster->id, ret);
			return -EINVAL;
		}

		policy->freq_table = new_table;
		node->temp_table = new_table;
	}

	policy->suspend_freq = policy->freq_table[0].frequency;

	return 0;
}

static void sprd_cpufreq_temp_work_func(struct work_struct *work)
{
	int temp;
	unsigned int freq;
	struct delayed_work *dwork = to_delayed_work(work);
	struct cluster_info *cluster =
		container_of(dwork, struct cluster_info, temp_work);

	if (!cluster) {
		pr_err("%s: get temp work cluster info error\n", __func__);
		return;
	}

	if (IS_ERR_OR_NULL(cluster->cpu_tz)) {
		cluster->cpu_tz = thermal_zone_get_zone_by_name(cluster->tz_name);
		if (IS_ERR_OR_NULL(cluster->cpu_tz)) {
			dev_warn(dev, "%s: failed to get cluster %u thmzone device\n", __func__, cluster->id);
			queue_delayed_work(system_highpri_wq, &cluster->temp_work,
					   msecs_to_jiffies(DVFS_TEMP_UPDATE_MS));
			return;
		}
	}

	thermal_zone_get_temp(cluster->cpu_tz, &temp);

	freq = sprd_cpufreq_update_opp(cluster->cpu, temp);
	if (freq)
		dev_info(dev, "%s: cluster[%u] update max freq[%u]\n", __func__, cluster->id, freq);

	queue_delayed_work(system_highpri_wq, &cluster->temp_work,
			   msecs_to_jiffies(DVFS_TEMP_UPDATE_MS));
}

/* sprd_cpufreq_driver interface */
static int sprd_cpufreq_init(struct cpufreq_policy *policy)
{
	struct cluster_info *cluster;
	u64 freq;
	int ret;

	cluster = pclusters + sprd_cluster_info(policy->cpu);
	if (!cluster || !cluster->freq_get || !cluster->dvfs_enable) {
		dev_err(dev, "%s: get cpu %u cluster info error\n", __func__, policy->cpu);
		return -EINVAL;
	}

	mutex_lock(&cluster->mutex);

	/* CPUs in the same cluster share same clock and power domain */
	ret = of_perf_domain_get_sharing_cpumask(policy->cpu, "sprd,freq-domain",
						 "#freq-domain-cells",
						 policy->cpus);
	if (ret < 0) {
		dev_err(dev, "%s: cpufreq cluster cpumask error", __func__);
		goto unlock_ret;
	}

	/* init other cpu policy link in same cluster */
	policy->dvfs_possible_from_any_cpu = true;

	policy->transition_delay_us = cluster->transition_delay;
	policy->driver_data = cluster;

	/* init dvfs table use current temp */
	ret = sprd_policy_table_update(policy, cluster->temp_currt_node);
	if (ret) {
		dev_err(dev, "%s: update cluster %u table error\n", __func__, cluster->id);
		goto unlock_ret;
	}

	/* get current freq for policy */
	ret = cluster->freq_get(cluster->id, &freq);
	if (ret) {
		dev_err(dev, "%s: get cluster %u current freq error\n", __func__, cluster->id);
		goto unlock_ret;
	}

	do_div(freq, 1000); /* from Hz to KHz */
	policy->cur = (u32)freq;

	/* enable dvfs phy */
	ret = cluster->dvfs_enable(cluster->id);
	if (ret) {
		dev_err(dev, "%s: enable cluster %u dvfs error\n", __func__, cluster->id);
		goto unlock_ret;
	}

	/* init debugfs interface to debug dvfs */
	ret = sprd_debug_cluster_init(policy);
	if (ret) {
		dev_err(dev, "%s: init cluster %u debug error\n", __func__, cluster->id);
		goto unlock_ret;
	}

	if (cluster->temp_enable) {
		ret = freq_qos_add_request(&policy->constraints,
					   &cluster->max_req, FREQ_QOS_MAX,
					   FREQ_QOS_MAX_DEFAULT_VALUE);
		if (ret < 0) {
			dev_err(dev, "%s: failed to add freq qos\n", __func__);
			goto unlock_ret;
		}

		queue_delayed_work(system_highpri_wq, &cluster->temp_work,
				   msecs_to_jiffies(DVFS_TEMP_UPDATE_MS));
	}

	mutex_unlock(&cluster->mutex);

	return 0;

unlock_ret:
	mutex_unlock(&cluster->mutex);

	return -EINVAL;
}

static int sprd_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct cluster_info *cluster;
	struct device *cpu;
	int ret;

	cluster = (struct cluster_info *)policy->driver_data;
	if (!cluster) {
		dev_err(dev, "%s: policy is not init\n", __func__);
		return -EINVAL;
	}

	cpu = get_cpu_device(policy->cpu);
	if (!cpu) {
		dev_err(dev, "%s: get cpu %u device error\n", __func__, policy->cpu);
		return -EINVAL;
	}

	mutex_lock(&cluster->mutex);

	if (cluster->temp_enable) {
		cancel_delayed_work_sync(&cluster->temp_work);
		freq_qos_remove_request(&cluster->max_req);
	}

	dev_pm_opp_free_cpufreq_table(cpu, &policy->freq_table);
	dev_pm_opp_of_remove_table(cpu);

	ret = sprd_debug_cluster_exit(policy);
	if (ret)
		dev_warn(dev, "%s: cluster %u debug exit error\n", __func__, cluster->id);

	policy->driver_data = NULL;

	mutex_unlock(&cluster->mutex);

	return 0;
}

static int sprd_cpufreq_table_verify(struct cpufreq_policy_data *policy_data)
{
	return cpufreq_generic_frequency_table_verify(policy_data);
}

static int sprd_cpufreq_set_target_index(struct cpufreq_policy *policy, u32 index)
{
	struct cluster_info *cluster;
	unsigned int freq;
	int ret;

	cluster = (struct cluster_info *)policy->driver_data;
	if (!cluster || !cluster->freq_set) {
		dev_err(dev, "%s: policy is not init\n", __func__);
		return -EINVAL;
	}

	if (cluster->boost_enable) {
		ret = sprd_cpufreq_boost_judge(policy);
		if (ret == ON_BOOST)
			return 0;
	}

	mutex_lock(&cluster->mutex);

	if (index >= cluster->table_entry_num) {
		dev_err(dev, "%s: cluster %u index %u is error\n", __func__, cluster->id, index);
		mutex_unlock(&cluster->mutex);
		return -EINVAL;
	}

	ret = cluster->freq_set(cluster->id, index);
	if (ret) {
		dev_err(dev, "%s: set cluster %u index %u error\n", __func__, cluster->id, index);
		mutex_unlock(&cluster->mutex);
		return -EINVAL;
	}

	freq = policy->freq_table[index].frequency;

	mutex_unlock(&cluster->mutex);

	return 0;
}

static u32 sprd_cpufreq_get(u32 cpu)
{
	struct cluster_info *cluster;
	u64 freq;
	int ret;

	cluster = pclusters + sprd_cluster_info(cpu);
	if (!cluster || !cluster->freq_get) {
		dev_err(dev, "%s: get cpu %u cluster info error\n", __func__, cpu);
		return 0;
	}

	mutex_lock(&cluster->mutex);

	ret = cluster->freq_get(cluster->id, &freq);
	if (ret) {
		dev_err(dev, "%s: get cluster %u current freq error\n", __func__, cluster->id);
		mutex_unlock(&cluster->mutex);
		return 0;
	}

	mutex_unlock(&cluster->mutex);

	do_div(freq, 1000); /* from Hz to KHz */

	return (u32)freq;
}

static int sprd_cpufreq_suspend(struct cpufreq_policy *policy)
{
	struct cluster_info *cluster = policy->driver_data;

	if (!strcmp(policy->governor->name, "userspace")) {
		dev_info(dev, "%s: do nothing for governor-%s\n", __func__, policy->governor->name);
		return 0;
	}

	if (cluster->boost_enable) {
		cluster->boost_enable = false;
		sprd_cpufreq_set_target_index(policy, 0);
	}

	return cpufreq_generic_suspend(policy);
}

static int sprd_cpufreq_resume(struct cpufreq_policy *policy)
{
	if (!strcmp(policy->governor->name, "userspace")) {
		dev_info(dev, "%s: do nothing for governor-%s\n", __func__, policy->governor->name);
		return 0;
	}

	return cpufreq_generic_suspend(policy);
}

static int sprd_cpufreq_online(struct cpufreq_policy *policy)
{
	struct cluster_info *cluster;

	cluster = (struct cluster_info *)policy->driver_data;
	if (!cluster) {
		dev_err(dev, "%s: policy is not init\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&cluster->mutex);

	if (cluster->temp_enable)
		queue_delayed_work(system_highpri_wq, &cluster->temp_work,
				   msecs_to_jiffies(DVFS_TEMP_UPDATE_MS));

	mutex_unlock(&cluster->mutex);

	return 0;
}

static int sprd_cpufreq_offline(struct cpufreq_policy *policy)
{
	struct cluster_info *cluster;

	cluster = (struct cluster_info *)policy->driver_data;
	if (!cluster) {
		dev_err(dev, "%s: policy is not init\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&cluster->mutex);

	if (cluster->temp_enable)
		cancel_delayed_work_sync(&cluster->temp_work);

	mutex_unlock(&cluster->mutex);

	return 0;
}

static struct cpufreq_driver sprd_cpufreq_driver = {
	.name = "sprd-cpufreq-v2",
	.flags = CPUFREQ_NEED_INITIAL_FREQ_CHECK |
		 CPUFREQ_HAVE_GOVERNOR_PER_POLICY |
		 CPUFREQ_IS_COOLING_DEV,
	.init = sprd_cpufreq_init,
	.exit = sprd_cpufreq_exit,
	.verify = sprd_cpufreq_table_verify,
	.target_index = sprd_cpufreq_set_target_index,
	.register_em = cpufreq_register_em_with_opp,
	.get = sprd_cpufreq_get,
	.suspend = sprd_cpufreq_suspend,
	.resume = sprd_cpufreq_resume,
	.online = sprd_cpufreq_online,
	.offline = sprd_cpufreq_offline,
	.attr = cpufreq_generic_attr,
};

/* init inerface */
static int sprd_cluster_temp_init(struct cluster_info *cluster)
{
	const char *name = "sprd,temp-threshold";
	struct property *prop;
	u32 num, val;
	int i, ret;

	INIT_LIST_HEAD(&cluster->temp_list_head);

	ret = sprd_temp_list_init(&cluster->temp_list_head);
	if (ret) {
		dev_err(dev, "%s: init cluster %u temp limit error\n", __func__, cluster->id);
		return -EINVAL;
	}

	cluster->temp_level_node = sprd_temp_list_find(&cluster->temp_list_head, DVFS_TEMP_LOW_LIMIT);
	cluster->temp_currt_node = cluster->temp_level_node;

	cluster->temp_tick = 0U;

	prop = of_find_property(cluster->node, name, &num);
	if (!prop || !num) {
		dev_warn(dev, "%s: find cluster %u temp property error\n", __func__, cluster->id);
		cluster->temp_enable = false;
		return 0;
	}

	cluster->temp_enable = true;
	INIT_DELAYED_WORK(&cluster->temp_work, sprd_cpufreq_temp_work_func);

	if (of_property_read_string(cluster->node, "sprd,thmzone-names", &cluster->tz_name)) {
		dev_err(dev, "%s: get cluster %u thmzone name error\n", __func__, cluster->id);
		return -EINVAL;
	}

	for (i = 0; i < num / sizeof(u32); i++) {
		ret = of_property_read_u32_index(cluster->node, name, i, &val);
		if (ret) {
			dev_err(dev, "%s: get cluster %u temp error\n", __func__, cluster->id);
			return -EINVAL;
		}

		ret = sprd_temp_list_add(&cluster->temp_list_head, (int)val);
		if (ret) {
			dev_err(dev, "%s: add cluster %u temp error\n", __func__, cluster->id);
			return -EINVAL;
		}
	}

	return 0;
}

static int sprd_cluster_props_init(struct cluster_info *cluster)
{
	int (*ops)(u32 id, u32 val);
	struct cluster_prop *p;
	struct device_node *hwf;
	int i, ret;
	struct cluster_prop props[] = {
		{
			.name = "sprd,voltage-step",
			.value = &cluster->voltage_step,
			.ops = (void **)&cluster->step_set,
		}, {
			.name = "sprd,voltage-margin",
			.value = &cluster->voltage_margin,
			.ops = (void **)&cluster->margin_set,
		}, {
			.name = "sprd,transition-delay",
			.value = &cluster->transition_delay,
			.ops = NULL,
		}, {
			.name = "sprd,pmic-type",
			.value = &cluster->pmic_type,
			.ops = (void **)&cluster->pmic_set,
		}
	};

	for (i = 0; i < ARRAY_SIZE(props); ++i) {
		p = props + i;
		ops = p->ops ? *p->ops : NULL;

		ret = of_property_read_u32(cluster->node, p->name, p->value);
		if (ret) {
			dev_warn(dev, "%s: get cluster %u '%s' value error\n", __func__, cluster->id, p->name);
			*p->value = 0U;
			continue;
		}

		ret = ops ? ops(cluster->id, *p->value) : 0;
		if (ret) {
			dev_err(dev, "%s: set cluster %u '%s' value error\n", __func__, cluster->id, p->name);
			return -EINVAL;
		}
	}

	ret = of_property_match_string(cluster->node, "nvmem-cell-names", "dvfs_bin");
	if (ret == -EINVAL) { /* No definition is allowed */
		dev_warn(dev, "%s: Warning: no 'dvfs_bin' appointed\n", __func__);
		cluster->bin = 0U;
	} else {
		ret = sprd_nvmem_info_read(cluster->node, "dvfs_bin", &cluster->bin);
		if (ret) {
			dev_err(dev, "%s: error in reading dvfs bin value\n", __func__);
			return ret;
		}

		ret = cluster->bin_set(cluster->id, cluster->bin);
		if (ret) {
			dev_err(dev, "%s: set cluster %u 'binning' value error\n", __func__, cluster->id);
			return -EINVAL;
		}
	}

	if (of_property_read_bool(cluster->node, "sprd,multi-version")) {
		hwf = of_find_node_by_path("/hwfeature/auto");
		if (IS_ERR_OR_NULL(hwf)) {
			dev_err(dev, "%s: no hwfeature/auto node found\n", __func__);
			return PTR_ERR(hwf);
		}

		cluster->version = (u64 *)of_get_property(hwf, "efuse", NULL);
		ret = cluster->version_set(cluster->id, cluster->version);
		if (ret) {
			dev_err(dev, "%s: set cluster %u 'version' value error\n", __func__, cluster->id);
			return -EINVAL;
		}
	}

	if (of_property_read_bool(cluster->node, "sprd,cpufreq-boost"))
		cluster->boost_enable = true;

	return 0;
}

static int sprd_cluster_ops_init(struct cluster_info *cluster)
{
	struct sprd_sip_svc_dvfs_ops *ops;
	struct sprd_sip_svc_handle *sip;

	sip = sprd_sip_svc_get_handle();
	if (!sip) {
		dev_err(dev, "%s: get sip error\n", __func__);
		return -EINVAL;
	}

	ops = &sip->dvfs_ops;

	cluster->dvfs_enable = ops->dvfs_enable;
	cluster->table_update = ops->table_update;
	cluster->step_set = ops->step_set;
	cluster->margin_set = ops->margin_set;
	cluster->freq_set = ops->freq_set;
	cluster->freq_get = ops->freq_get;
	cluster->pair_get = ops->pair_get;
	cluster->pmic_set = ops->pmic_set;
	cluster->bin_set = ops->bin_set;
	cluster->version_set = ops->version_set;
	cluster->dvfs_init = ops->dvfs_init;

	return 0;
}

static struct device_node *sprd_cluster_node_init(u32 cpu_idx)
{
	struct device *cpu_dev;

	cpu_dev = get_cpu_device(cpu_idx);
	if (!cpu_dev)
		return NULL;

	return of_parse_phandle(cpu_dev->of_node, "sprd,freq-domain", 0);
}

static int sprd_cluster_info_init(struct cluster_info *clusters)
{
	struct cluster_info *cluster;
	unsigned int cpu;
	int ret;

	for_each_possible_cpu(cpu) {
		cluster = clusters + sprd_cluster_info(cpu);
		if (cluster->node) {
			dev_dbg(dev, "%s: cluster %u info is also init\n", __func__, cluster->id);
			continue;
		}

		cluster->node = sprd_cluster_node_init(cpu);
		if (!cluster->node) {
			dev_err(dev, "%s: init cluster %u node error\n", __func__, cluster->id);
			return -EINVAL;
		}

		cluster->id = (u32)sprd_cluster_info(cpu);
		cluster->cpu = cpu;

		mutex_init(&cluster->mutex);

		ret = sprd_cluster_ops_init(cluster);
		if (ret) {
			dev_err(dev, "%s: init cluster %u ops error\n", __func__, cluster->id);
			return -EINVAL;
		}

		ret = sprd_cluster_props_init(cluster);
		if (ret) {
			dev_err(dev, "%s: init cluster %u props error\n", __func__, cluster->id);
			return ret;
		}

		ret = sprd_cluster_temp_init(cluster);
		if (ret) {
			dev_err(dev, "%s: init cluster %u temp error\n", __func__, cluster->id);
			return -EINVAL;
		}
	}

	return 0;
}

static int sprd_cpufreq_probe(struct platform_device *pdev)
{
	int ret;

	boot_done_timestamp = jiffies + SPRD_CPUFREQ_BOOST_DURATION;

	dev = &pdev->dev;

	dev_info(dev, "%s: probe sprd cpufreq v2 driver\n", __func__);

	pclusters = devm_kzalloc(dev, sizeof(*pclusters) * sprd_cluster_num(), GFP_KERNEL);
	if (!pclusters) {
		dev_err(dev, "%s: alloc memory for cluster info error\n", __func__);
		return -ENOMEM;
	}

	ret = sprd_cluster_info_init(pclusters);
	if (ret) {
		dev_err(dev, "%s: init cluster info error\n", __func__);
		return ret;
	}

	ret = pclusters->dvfs_init();
	if (ret) {
		dev_err(dev, "%s: init dvfs device error\n", __func__);
		return -EINVAL;
	}

	ret = sprd_debug_init(dev);
	if (ret) {
		dev_err(dev, "%s: init dvfs debug error\n", __func__);
		return -EINVAL;
	}

	ret = cpufreq_register_driver(&sprd_cpufreq_driver);
	if (ret)
		dev_err(dev, "%s: register cpufreq driver error\n", __func__);
	else
		dev_info(dev, "%s: register cpufreq driver success\n", __func__);

	return ret;
}

/**
 * sprd_cpufreq_update_opp() - returns the max freq of a cpu and update dvfs
 * table by temp_now
 *
 * @cpu: which cpu you want to update dvfs table
 * @now_temp: current temperature on this cpu, mini-degree.
 *
 * Return:
 * 1.cluster is not working, then return 0
 * 2.succeed to update dvfs table then return max freq(KHZ) of this cluster
 */
unsigned int sprd_cpufreq_update_opp(int cpu, int now_temp)
{
	struct cpufreq_policy *policy;
	struct cluster_info *cluster;
	int temp = now_temp / 1000;
	struct temp_node *node;
	u64 freq;
	int ret;

	policy = cpufreq_cpu_get(cpu);
	if (!policy) {
		dev_err(dev, "%s: get cpu %u policy error\n", __func__, cpu);
		return 0;
	}

	cluster = (struct cluster_info *)policy->driver_data;
	if (!cluster || !cluster->pair_get) {
		dev_err(dev, "%s: cpu %u cluster info error\n", __func__, cpu);
		cpufreq_cpu_put(policy);
		return 0;
	}

	mutex_lock(&cluster->mutex);

	node = sprd_temp_list_find(&cluster->temp_list_head, temp);

	if (!node) {
		cluster->temp_level_node = cluster->temp_currt_node;
		cluster->temp_tick = 0U;
		goto ret_error;
	}

	/* immediate response to temp rise */
	if (node->temp > cluster->temp_currt_node->temp) {
		cluster->temp_level_node = node;
		cluster->temp_tick = DVFS_TEMP_MAX_TICKS;
	} else if (node != cluster->temp_level_node) {
		cluster->temp_level_node = node;
		cluster->temp_tick = 0U;
		goto ret_error;
	}

	++cluster->temp_tick;
	if (cluster->temp_tick < DVFS_TEMP_MAX_TICKS)
		goto ret_error;

	cluster->temp_tick = 0U;

	if (cluster->temp_level_node == cluster->temp_currt_node)
		goto ret_error;

	dev_info(dev, "%s: update cluster %u table to %d(%d) degrees celsius\n", __func__, cluster->id, temp, cluster->temp_level_node->temp);

	/* delay is required to ensure that the last process is completed */
	udelay(100);

	ret = sprd_policy_table_update(policy, cluster->temp_level_node);
	if (ret) {
		dev_err(dev, "%s: update cluster %u table error\n", __func__, cluster->id);
		goto ret_error;
	}

	cluster->temp_currt_node = cluster->temp_level_node;

	ret = cluster->pair_get(cluster->id, cluster->table_entry_num - 1, &freq, NULL);
	if (ret) {
		dev_err(dev, "%s: get cluster %u max freq error\n", __func__, cluster->id);
		goto ret_error;
	}

	do_div(freq, 1000); /* from Hz to KHz */

	freq_qos_update_request(&cluster->max_req, freq);

	cpufreq_cpu_put(policy);
	mutex_unlock(&cluster->mutex);

	return (unsigned int)freq;

ret_error:
	cpufreq_cpu_put(policy);
	mutex_unlock(&cluster->mutex);

	return 0;
}

static const struct of_device_id sprd_cpufreq_of_match[] = {
	{
		.compatible = "sprd,cpufreq-v2",
	},
	{
		/* Sentinel */
	},
};

static struct platform_driver sprd_cpufreq_platform_driver = {
	.driver = {
		.name = "sprd-cpufreq-v2",
		.of_match_table = sprd_cpufreq_of_match,
	},
	.probe = sprd_cpufreq_probe,
};

static int __init sprd_cpufreq_platform_driver_register(void)
{
	return platform_driver_register(&sprd_cpufreq_platform_driver);
}

device_initcall(sprd_cpufreq_platform_driver_register);

MODULE_DESCRIPTION("sprd cpufreq v2 driver");
MODULE_LICENSE("GPL v2");
