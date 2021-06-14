/* SPDX-License-Identifier: GPL-2.0 */
//
// Unisoc APCPU HW DVFS driver
//
// Copyright (C) 2020 Unisoc, Inc.
// Author: Jack Liu <jack.liu@unisoc.com>

#ifndef SPRD_HW_CPUFREQ_H
#define SPRD_HW_CPUFREQ_H

#include "sprd-hwdvfs-normal.h"

#define TEMP_THRESHOLD_NUM		4
#define INVALID_CPU_BIN			0
#define INVALID_MIN_VOL			(-1)
#define SPRD_CPUFREQ_TEMP_MIN		(-200)
#define	SPRD_CPUFREQ_TEMP_MAX		200
#define CPUFREQ_OPP_NAME_LEN		36
#define SPRD_CPUFREQ_BOOST_DURATION	(60ul * HZ)

#define VENDOR_NAME	"operating-points"

#define MAX_CPU_VER_NAME_LEN		16

struct sprd_cpu_cluster_info {
	struct dentry *dir;
	struct mutex opp_mutex;
	int temp_threshold_max, temp_now;
	int temp_top, temp_bottom;
	unsigned int temp_list[TEMP_THRESHOLD_NUM];
	int temp_min, temp_max;
	int curr_temp_threshold;
	unsigned long temp_fall_time;
	unsigned long freq_temp_fall_hz;
	bool online, temp_threshold_defined;
	bool arch_dvfs_enabled;
	const char *bin_prop_name;
	const char *temp_threshold_name;
	char opp_name[CPUFREQ_OPP_NAME_LEN];	/* vendor + cpu bin + temp */
	char default_opp_name[CPUFREQ_OPP_NAME_LEN];  /*vendor + cpu bin */
	char pre_opp_name[CPUFREQ_OPP_NAME_LEN];
	u32 cpu_bin, max_cpu_bin, min_vol;
	char *cpubin_str;
	unsigned int temp_max_freq;
	char cpu_diff_ver[MAX_CPU_VER_NAME_LEN];
};

struct sprd_cpufreq_info {
	struct device *pdev;
	struct cpumask cpus;
	struct cpufreq_policy *policy;
	struct opp_table *opp_table;
	struct device *cpu_dev;
	struct device_node *cpu_np;
	struct device_node *cpufreq_np;
	struct device_node *cpudvfs_dev_np;
	struct cpudvfs_device *parchdev;
	unsigned int clu_id, cpu_id;
	struct sprd_cpu_cluster_info *pcluster;
	unsigned int policy_trans;
	/*CUFREQ points to update opp by temp*/
	unsigned int (*update_opp)(int cpu, int temp_now);
};

static inline int cpubin2str(int bin, char **name)
{
	switch (bin) {
	case 1:
		*name = "ff";
		break;
	case 2:
		*name = "tt";
		break;
	case 3:
		*name = "ss";
		break;
	case 4:
		*name = "od";
		break;
	default:
		pr_err("invalid cpu bin value(%d)\n", bin);
		*name = NULL;
		return -EINVAL;
	}

	return 0;
}

unsigned int sprd_cpufreq_update_opp_normal(int cpu, int temp_now);

#if defined(CONFIG_DEBUG_FS)
int sprd_cpufreq_debugfs_add(struct sprd_cpufreq_info *info);
int sprd_cpufreq_debugfs_remove(struct sprd_cpufreq_info *info);
#else
static inline int sprd_cpufreq_debugfs_add(struct sprd_cpufreq_info *info)
{
	return 0;
}
static inline int sprd_cpufreq_debugfs_remove(struct sprd_cpufreq_info *info)
{
	return 0;
}
#endif

#endif
