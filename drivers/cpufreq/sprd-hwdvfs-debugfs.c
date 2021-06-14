// SPDX-License-Identifier: GPL-2.0
//
// Unisoc APCPU HW DVFS driver
//
// Copyright (C) 2020 Unisoc, Inc.
// Author: Jack Liu <jack.liu@unisoc.com>

#include <linux/debugfs.h>
#include "sprd-hwdvfs-cpufreq.h"

static struct dentry *debugfs_root;
static char opp_name_before_bin[CPUFREQ_OPP_NAME_LEN];

struct dvfs_debug_param {
	const char *name;
	umode_t mode;
	const struct file_operations *fops;
};

static int debugfs_cpu_bin_get(void *data, u64 *val)
{
	struct sprd_cpufreq_info *info = data;

	if (!info || !info->pcluster) {
		pr_err("cpu information is missing\n");
		return -ENODEV;
	}

	*val = info->pcluster->cpu_bin;

	dev_dbg(info->pdev, "debug: get cpu bin val: %llu\n", *val);

	return 0;
}

static int debugfs_cpu_bin_set(void *data, u64 val)
{
	struct sprd_cpufreq_info *info = data;
	struct sprd_cpu_cluster_info *clu;
	char *temp, str[16] = "";
	int ret;

	if (!info || !info->pcluster) {
		pr_err("cpu information is missing\n");
		return -ENODEV;
	}

	clu = info->pcluster;

	if (val && clu->cpu_bin != INVALID_CPU_BIN) {
		dev_err(info->pdev, "the cpu binning value already is existed, earse it first\n");
		return -EINVAL;
	}

	dev_dbg(info->pdev, "debug: set cpu bin val: %llu\n", val);

	if (val) {
		ret = cpubin2str(val, &temp);
		if (ret)
			return ret;

		if (clu->cpu_bin == INVALID_CPU_BIN)
			strcpy(opp_name_before_bin, clu->opp_name);

		mutex_lock(&clu->opp_mutex);
		clu->cpu_bin = val;
		sprintf(str, "-%s", temp);
		strcat(clu->opp_name, str);
		strcat(clu->default_opp_name, str);
		clu->cpubin_str = temp;
		mutex_unlock(&clu->opp_mutex);
	} else {
		mutex_lock(&clu->opp_mutex);
		clu->cpu_bin = INVALID_CPU_BIN;
		strcpy(clu->opp_name, opp_name_before_bin);
		strcpy(clu->default_opp_name, opp_name_before_bin);
		clu->cpubin_str = "";
		mutex_unlock(&clu->opp_mutex);
	}

	dev_dbg(info->pdev, "debug: current opp name: %s\n", clu->opp_name);

	return 0;
}

static int debugfs_cpu_temp_get(void *data, u64 *val)
{
	struct sprd_cpufreq_info *info = data;

	if (!info || !info->pcluster) {
		pr_err("cpu information is missing\n");
		return -ENODEV;
	}

	*val = info->pcluster->temp_now;

	return 0;
}

static int debugfs_cpu_temp_set(void *data, u64 val)
{
	struct sprd_cpufreq_info *info = data;
	int ret, temp_now = val;

	if (!info || !info->pcluster) {
		pr_err("cpu information is missing\n");
		return -ENODEV;
	}

	if (info->pcluster->cpu_bin == INVALID_CPU_BIN) {
		dev_err(info->pdev, "cpu binning value is empty!\n");
		return -EINVAL;
	}

	if (!info->pcluster->temp_threshold_defined)
		info->pcluster->temp_threshold_defined = true;

	dev_dbg(info->pdev, "debug: set cpu temperature: %llu\n", val);

	temp_now = temp_now * 1000;

	ret = sprd_cpufreq_update_opp_normal(info->cpu_id, temp_now);
	if (!ret)
		dev_err(info->pdev, "failed to update opp according to temp\n");

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_cpu_bin, debugfs_cpu_bin_get, debugfs_cpu_bin_set,
			"%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_cpu_temp, debugfs_cpu_temp_get,
			debugfs_cpu_temp_set, "%llu\n");

static struct dvfs_debug_param dvfs_debug_node[] = {
	{"cpu_bin", 0400, &fops_cpu_bin,},
	{"cpu_temp", 0600, &fops_cpu_temp,},
	{},
};

int sprd_cpufreq_debugfs_add(struct sprd_cpufreq_info *info)
{
	struct sprd_cpu_cluster_info *clu;
	char name[10];
	u32 j = 0;

	if (!info || !info->pcluster) {
		pr_err("cpu information is missing\n");
		return -ENODEV;
	}

	if (!debugfs_root) {
		debugfs_root = debugfs_create_dir("cpudvfs", NULL);
		if (IS_ERR_OR_NULL(debugfs_root)) {
			dev_err(info->pdev,
				"failed to create debugfs node for dvfs\n");
			return -EINVAL;
		}
	}

	sprintf(name, "cluster%d", info->clu_id);
	clu = info->pcluster;

	if (!clu->dir) {
		clu->dir = debugfs_create_dir(name, debugfs_root);
		if (!clu->dir) {
			dev_err(info->pdev, "failed to set child debug dir\n");
			return -EINVAL;
		}
	}

	while (dvfs_debug_node[j].name) {
		debugfs_create_file(dvfs_debug_node[j].name,
				    dvfs_debug_node[j].mode, clu->dir, info,
				    dvfs_debug_node[j].fops);
		j += 1;
	}

	return 0;
}

int sprd_cpufreq_debugfs_remove(struct sprd_cpufreq_info *info)
{
	if (!info || !info->pcluster) {
		pr_err("cpu information is missing\n");
		return -ENODEV;
	}

	debugfs_remove_recursive(info->pcluster->dir);
	info->pcluster->dir = NULL;
	return 0;
}

MODULE_LICENSE("GPL v2");
