/* SPDX-License-Identifier: GPL-2.0 */
//
// Unisoc APCPU HW DVFS driver
//
// Copyright (C) 2020 Unisoc, Inc.
// Author: Jack Liu <jack.liu@unisoc.com>

#ifndef SPRD_HWDVFS_NORMAL_H
#define SPRD_HWDVFS_NORMAL_H

#include "sprd-hwdvfs-archdata.h"

struct plat_opp {
	unsigned long freq;	/*hz*/
	unsigned long volt;	/*uV*/
};

struct cpudvfs_phy_ops {
	int (*dvfs_enable)(void *pdev, u32 clu_id, bool enable);
	int (*volt_grade_table_update)(void *pdev, u32 clu_id,
				       unsigned long freq_hz,
				       unsigned long volt_uV, int opp_idx);
	int (*udelay_update)(void *pdev, u32 clu_id);
	int (*index_map_table_update)(void *pdev, char *opp_name, u32 clu_id,
				      char *cpudiff_str, char *cpubin_str,
				      int curr_temp_threshold);
	int (*idle_pd_volt_update)(void *pdev, u32 clu_id);
	int (*target_set)(void *pdev, u32 clu_id, u32 opp_idx);
	unsigned int (*freq_get)(void *pdev, u32 clu_id);
};

struct mpll_cfg {
	struct regmap *anag_map;
	u32 anag_reg, dbg_sel;
	bool output_switch_done;
};

struct dcdc_pwr {
	char name[10];
	bool third_pmic_used;
	u32 pmic_num;		/* sequence number in pmic_array*/
	u32 tuning_latency_us;
	u32 voltage_grade_num;
	u32 slew_rate;		/* mv/us */
	unsigned long grade_volt_val_array[MAX_VOLT_GRADE_NUM];	/* in uV*/
	bool i2c_used, i2c_shared;
	struct i2c_client *i2c_client;
};

struct cpudvfs_device {
	struct regmap *aonmap;
	struct device *dev;
	void __iomem *membase;
	const struct dvfs_private_data *priv;
	struct device_node *topdvfs_of_node;
	struct regmap *topdvfs_map;
	struct device_node *of_node;
	struct dvfs_cluster *phost_cluster;
	struct dvfs_cluster *pslave_cluster;
	struct dvfs_cluster *hcluster[8];
	struct dvfs_cluster *scluster[8];
	struct cpudvfs_phy_ops *phy_ops;
	u32 host_cluster_num, slave_cluster_num;
	u32 total_cluster_num;
	u32 dvfs_device_num;
	struct mpll_cfg *mplls;
	u32 mpll_num;
	struct dcdc_pwr *pwr;
	u32 dcdc_num;
	u32 pmic_type_sum; /* the total number of different pmics*/
	bool i2c_used[DVFS_HOST_CLUSTER_MAX];
};

#endif /* DVFS_CTRL_H */
