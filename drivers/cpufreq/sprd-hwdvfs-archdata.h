/* SPDX-License-Identifier: GPL-2.0 */
//
// Unisoc APCPU HW DVFS driver
//
// Copyright (C) 2020 Unisoc, Inc.
// Author: Jack Liu <jack.liu@unisoc.com>
#ifndef SPRD_HWDVFS_ARCHDATA_H
#define SPRD_HWDVFS_ARCHDATA_H

#include <linux/cpufreq.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>

#define MAX_VOLT_GRADE_NUM	16
#define MAX_MPLL_INDEX_NUM	8
#define MAX_APCPU_DVFS_MISC_CFG_ENTRY	16
#define MAX_TOP_DVFS_MISC_CFG_ENTRY	16
#define MAX_DVFS_INDEX_NUM		16

#define GENREGSET(r, o, m)		{.reg = r, .off = o, .msk = m, .val = 0}
#define GENREGVALSET(r, o, m, v)	{.reg = r, .off = o, .msk = m, .val = v}
#define SPECREGVALSET(x, r, o, m, v) \
	{.x.reg = r, .x.off = o, .x.msk = m, .x.val = v}

#define MAX_MPLL	8
#define DVFS_HOST_CLUSTER_MAX	4

#define MAX_DTS_TBL_NAME_LEN	36

struct dvfs_cluster {
	u32 id;				/* cluster id */
	char *name;			/* cluster name */
	bool is_host;
	u32 dcdc;
	unsigned long pre_grade_volt;
	u32 tmp_vol_grade;
	u32 max_vol_grade;
	void *parent;			/* parent device */
	struct plat_opp *freqvolt;	/* store the opp for the cluster */
	struct device_node *of_node;
	char dts_tbl_name[MAX_DTS_TBL_NAME_LEN]; /* map table name in dts */
	char default_tbl_name[MAX_DTS_TBL_NAME_LEN];
	u32 tbl_column_num;		/* the column num in map table */
	u32 tbl_row_num;		/* the row num in map table */
	u32 map_idx_max;
	u32 *opp_map_tbl;
	bool existed;
};

enum cpudvfs_pmic {
	PMIC_SC2730,
	PMIC_SC2703,
	PMIC_FAN5355_05,
	MAX_PMIC_TYPE_NUM = 8,
};

struct reg_info {
	u32 reg;
	u32 off;
	u32 msk;
	u32 val;
};

struct output_parameter {
	struct reg_info postdiv;
	struct reg_info icp;
	struct reg_info n;
};

struct mpll_index_tbl {
	struct output_parameter entry[MAX_MPLL_INDEX_NUM];
};

struct volt_grades_table {
	struct reg_info regs_array[MAX_VOLT_GRADE_NUM];
	int grade_count;
};

struct udelay_tbl {
	struct reg_info tbl[MAX_VOLT_GRADE_NUM];
};

struct dvfs_index_entry_info {
	char *name;
	u8 off, msk;
};

struct dvfs_index_tbl {
	struct reg_info regs[MAX_DVFS_INDEX_NUM];
	struct dvfs_index_entry_info  entry_info[MAX_DVFS_INDEX_NUM];
	u32 tbl_row_num, tbl_column_num;
};

struct mpll_freq_manager {
	struct mpll_index_tbl *mpll_tbl;
	struct reg_info *auto_relock_cfg;
	struct reg_info *auto_pd_cfg;
	u32 mpll_num;
};

struct  topdvfs_volt_manager {
	struct volt_grades_table *grade_tbl;
	struct udelay_tbl *up_udelay_tbl;
	struct udelay_tbl *down_udelay_tbl;
	struct reg_info *host_vol_auto_tune_cfg;
	struct reg_info *host_freq_auto_tune_cfg;
	struct reg_info *idle_vol_cfg;
	struct reg_info *misc_cfg;
	struct reg_info *third_pmic_cfg;
	u32 dcdc_num, vir_dcdc_adi_num;
};

struct cpudvfs_freq_manager {
	struct reg_info *slave_freq_auto_tune_cfg;
	struct reg_info *dev_idle_disable_cfg;
	struct reg_info *misc_cfg;
	struct reg_info *hcluster_work_index_cfg;
	struct reg_info *hcluster_idle_index_cfg;
	struct dvfs_index_tbl *host_cluster_index_tbl;
	struct dvfs_index_tbl *slave_cluster_index_tbl;
	u32 host_cluster_num, slave_cluster_num;
	u32 dvfs_device_num;
};

struct pmic_data {
	u32 volt_base;	/*uV*/
	u32 per_step;	/*uV*/
	u32 margin_us;
	int (*update)(struct regmap *map, struct reg_info *regs, void *pm,
		      unsigned long u_volt, int index, int count);
	u32 (*up_cycle_calculate)(u32 max_val_uV, u32 slew_rate,
				  u32 module_clk_khz, u32 margin_us);
	u32 (*down_cycle_calculate)(u32 max_val_uV, u32 slew_rate,
				    u32 module_clk_hz, u32 margin_us);
};

struct dvfs_private_data {
	u32 module_clk_khz;
	struct pmic_data *pmic;
	struct topdvfs_volt_manager *volt_manager;
	struct cpudvfs_freq_manager *freq_manager;
	struct mpll_freq_manager *mpll_manager;
	struct dvfs_cluster *host_cluster;
	struct dvfs_cluster *slave_cluster;
};

#define DECLARE_APCPU_DVFS_CLUSTER(clu_name)	\
	{				\
		.name = clu_name,	\
		.dts_tbl_name = clu_name "_tbl",	\
		.default_tbl_name = clu_name "_tbl",	\
	}

#define DECLARE_DVFS_INDEX_INFO(column_entry_name, f, m)	\
	{					\
		.name = column_entry_name,	\
		.off = f,			\
		.msk = m,			\
	}

extern int default_dcdc_volt_update(struct regmap *map, struct reg_info *regs,
				    void *data, unsigned long u_volt, int index,
				    int count);
extern u32 default_cycle_calculate(u32 max_val_uV, u32 slew_rate,
				   u32 module_clk_hz, u32 margin_us);

#endif
