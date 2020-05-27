/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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
#include <linux/cpufreq.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include "sprd-hwdvfs-archdata.h"

#define	HOST_CLUSTER_NUM	2
#define SLAVE_CLUSTER_NUM	3
#define MPLL_NUM		3
#define DCDC_NUM		2

#define CPU_LIT_CLUSTER		0
#define CPU_BIG_CLUSTER		1

#define SCU_SLAVE_CLUSTER	0
#define PERIPH_SLAVE_CLUSTER	1
#define GIC_SLAVE_CLUSTER	2

#define MPLL0	0
#define MPLL1	1
#define MPLL2	2


#define DVFS_DEVICE_NUM		8

#define	DEV_CORE0	0
#define	DEV_CORE1	1
#define	DEV_CORE2	2
#define	DEV_CORE3	3
#define	DEV_SCU		4
#define	DEV_ATB		5
#define	DEV_PERIPH	6
#define	DEV_GIC		7

#define DCDC_CPU0	0
#define DCDC_CPU1	1
#define VIR_DCDC_CPU_ADI_NUM	DCDC_NUM
#define DCDC_CPU_I2C(n)	(DCDC_CPU##n + VIR_DCDC_CPU_ADI_NUM)

static struct pmic_data pmic_array[MAX_PMIC_TYPE_NUM] = {
	[PMIC_SC2730] = {
		.volt_base = 0,
		.per_step = 3125,
		.margin_us = 20,
		.update = default_dcdc_volt_update,
		.up_cycle_calculate = default_cycle_calculate,
		.down_cycle_calculate = default_cycle_calculate,
	},
	[PMIC_SC2703] = {
		.volt_base = 300000,
		.per_step = 10000,
		.margin_us = 20,
		.update = default_dcdc_volt_update,
		.up_cycle_calculate = default_cycle_calculate,
		.down_cycle_calculate = default_cycle_calculate,
	},
	{
	},
};

/* UMS312 Private Data */
static struct volt_grades_table ums312_volt_grades_tbl[] = {
	[DCDC_CPU0] = {
		.regs_array = {
			GENREGSET(0xf4, 0, 0x1ff),
			GENREGSET(0xf4, 9, 0x1ff),
			GENREGSET(0xf4, 18, 0x1ff),
			GENREGSET(0xf8, 0, 0x1ff),
			GENREGSET(0xf8, 9, 0x1ff),
			GENREGSET(0xf8, 18, 0x1ff),
			GENREGSET(0xfc, 0, 0x1ff),
			GENREGSET(0xfc, 9, 0x1ff),
		},
		.grade_count = 8,
	},
	[DCDC_CPU1] = {
		.regs_array = {
			GENREGSET(0x100, 0, 0x1ff),
			GENREGSET(0x100, 9, 0x1ff),
			GENREGSET(0x100, 18, 0x1ff),
			GENREGSET(0x104, 0, 0x1ff),
			GENREGSET(0x104, 9, 0x1ff),
			GENREGSET(0x104, 18, 0x1ff),
		},
		.grade_count = 6,
	},
	[DCDC_CPU_I2C(1)] = {
		.regs_array = {
			GENREGSET(0x12c, 0, 0x7f),
			GENREGSET(0x12c, 7, 0x7f),
			GENREGSET(0x12c, 14, 0x7f),
			GENREGSET(0x12c, 21, 0x7f),
			GENREGSET(0x130, 0, 0x7f),
			GENREGSET(0x130, 7, 0x7f),
		},
		.grade_count = 6,
	},
};

static struct udelay_tbl ums312_up_udelay_tbl[DCDC_NUM] = {
	[DCDC_CPU0] = {
		.tbl = {
			GENREGSET(0x58, 0, 0x1ffff),
			GENREGSET(0x58, 16, 0x1ffff),
			GENREGSET(0x54, 0, 0x1ffff),
			GENREGSET(0x54, 16, 0x1ffff),
			GENREGSET(0x50, 0, 0x1ffff),
			GENREGSET(0x50, 16, 0x1ffff),
			GENREGSET(0x110, 0, 0x1ffff),
		},
	},
	[DCDC_CPU1] = {
		.tbl = {
			GENREGSET(0x84, 0, 0xffff),
			GENREGSET(0x84, 16, 0xffff),
			GENREGSET(0x80, 0, 0xffff),
			GENREGSET(0x80, 16, 0xffff),
			GENREGSET(0x7c, 0, 0xffff),
			GENREGSET(0x7c, 16, 0xffff),
			GENREGSET(0x118, 0, 0xffff),
		},
	},
};

static struct udelay_tbl ums312_down_udelay_tbl[DCDC_NUM] = {
	[DCDC_CPU0] = {
		.tbl = {
			GENREGSET(0x64, 0, 0xffff),
			GENREGSET(0x64, 16, 0xffff),
			GENREGSET(0x60, 0, 0xffff),
			GENREGSET(0x60, 16, 0xffff),
			GENREGSET(0x5c, 0, 0xffff),
			GENREGSET(0x5c, 16, 0xffff),
			GENREGSET(0x114, 0, 0xffff),
		},
	},
	[DCDC_CPU1] = {
		.tbl = {
			GENREGSET(0x90, 0, 0xffff),
			GENREGSET(0x90, 16, 0xffff),
			GENREGSET(0x8c, 0, 0xffff),
			GENREGSET(0x8c, 16, 0xffff),
			GENREGSET(0x88, 0, 0xffff),
			GENREGSET(0x88, 16, 0xffff),
			GENREGSET(0x11c, 0, 0xffff),
		},
	},
};

static struct reg_info ums312_volt_misc_cfg[] = {
	GENREGVALSET(0, 0, 0, 0),
};

static struct reg_info ums312_freq_misc_cfg[] = {
	/* Set default work index 7 for lit core */
	GENREGVALSET(0x214, 0, 0xf, 7),
	/* Set default work index 3 for big core */
	GENREGVALSET(0x224, 0, 0xf, 3),
	GENREGVALSET(0, 0, 0, 0),
};

static struct mpll_index_tbl ums312_mpll_index_tbl[MPLL_NUM] = {
	[MPLL1] = {
		.entry = {
			/* MPLL1 Index2 */
			{
				.icp = GENREGVALSET(0x30, 0, 0x7, 5),
				.postdiv = GENREGVALSET(0x30, 3, 0x1, 0),
				.n = GENREGVALSET(0x30, 4, 0x7ff, 0x4d),
			},
		},
	},
};

static struct reg_info ums312_mpll_auto_relock_cfg[MPLL_NUM] = {
	[MPLL0] = GENREGVALSET(0x244, 1, 0x1, 1),
	[MPLL1] = GENREGVALSET(0x258, 1, 0x1, 1),
	[MPLL2] = GENREGVALSET(0x26c, 1, 0x1, 1),
};

static struct reg_info ums312_mpll_auto_pd_cfg[MPLL_NUM] = {
	[MPLL0] = GENREGVALSET(0x244, 0, 0x1, 1),
	[MPLL1] = GENREGVALSET(0x258, 0, 0x1, 1),
	[MPLL2] = GENREGVALSET(0x26c, 0, 0x1, 1),
};

static struct reg_info ums312_idle_vol_cfg[DCDC_NUM] = {
	[DCDC_CPU0] = GENREGVALSET(0x16c, 3, 0x7, 6),
	[DCDC_CPU1] = GENREGVALSET(0x16c, 0, 0x7, 5),
};

static struct reg_info ums312_third_pmic_cfg[DCDC_NUM] = {
	[DCDC_CPU1] = GENREGSET(0x120, 0, 0x1),
};

static struct reg_info ums312_host_vol_auto_tune_cfg[DCDC_NUM] = {
	[DCDC_CPU0] = GENREGVALSET(0x68, 20, 0x1, 0),
	[DCDC_CPU1] = GENREGVALSET(0x94, 0, 0x1, 0),
};

static struct reg_info ums312_host_freq_auto_tune_cfg[HOST_CLUSTER_NUM] = {
	[CPU_LIT_CLUSTER] = GENREGVALSET(0x150, 6, 0x1, 0),
	[CPU_BIG_CLUSTER] = GENREGVALSET(0x150, 7, 0x1, 0),
};

static struct reg_info ums312_slave_freq_auto_tune_cfg[SLAVE_CLUSTER_NUM] = {
	[SCU_SLAVE_CLUSTER] = GENREGVALSET(0x18, 1, 0x1, 1),
	[PERIPH_SLAVE_CLUSTER] = GENREGVALSET(0x18, 0, 0x1, 1),
	[GIC_SLAVE_CLUSTER] = GENREGVALSET(0x18, 2, 0x1, 1),
};

static struct reg_info ums312_hcluster_work_index_cfg[HOST_CLUSTER_NUM] = {
	[CPU_LIT_CLUSTER] = GENREGSET(0x214, 0, 0xf),
	[CPU_BIG_CLUSTER] = GENREGSET(0x224, 0, 0xf),
};

static struct reg_info ums312_hcluster_idle_index_cfg[HOST_CLUSTER_NUM] = {
	[CPU_LIT_CLUSTER] = GENREGSET(0x218, 0, 0xf),
	[CPU_BIG_CLUSTER] = GENREGSET(0x228, 0, 0xf),
};

static struct reg_info ums312_dev_idle_disable_cfg[DVFS_DEVICE_NUM] = {
	[DEV_CORE0] = GENREGVALSET(0x1c, 0, 0x1, 1),
	[DEV_CORE1] = GENREGVALSET(0x1c, 1, 0x1, 1),
	[DEV_CORE2] = GENREGVALSET(0x1c, 2, 0x1, 1),
	[DEV_CORE3] = GENREGVALSET(0x1c, 3, 0x1, 1),
	[DEV_SCU] = GENREGVALSET(0x1c, 4, 0x1, 1),
	[DEV_ATB] = GENREGVALSET(0x1c, 5, 0x1, 1),
	[DEV_PERIPH] = GENREGVALSET(0x1c, 6, 0x1, 1),
	[DEV_GIC] = GENREGVALSET(0x1c, 7, 0x1, 1),
};

static
struct dvfs_index_tbl ums312_host_cluster_index_tbl[HOST_CLUSTER_NUM] = {
	[CPU_LIT_CLUSTER] = {
		.regs = {
			GENREGSET(0x60, 0, 0xffff),
			GENREGSET(0x64, 0, 0xffff),
			GENREGSET(0x68, 0, 0xffff),
			GENREGSET(0x6c, 0, 0xffff),
			GENREGSET(0x70, 0, 0xffff),
			GENREGSET(0x74, 0, 0xffff),
			GENREGSET(0x78, 0, 0xffff),
			GENREGSET(0x7c, 0, 0xffff),
			GENREGSET(0x80, 0, 0xffff),
			GENREGSET(0x84, 0, 0xffff),
		},
		.entry_info = {
			DECLARE_DVFS_INDEX_INFO("sel", 0, 0x3),
			DECLARE_DVFS_INDEX_INFO("div", 2, 0x7),
			DECLARE_DVFS_INDEX_INFO("voted_volt", 15, 0x7),
			DECLARE_DVFS_INDEX_INFO("voted_scu_index", 5, 0xf),
			DECLARE_DVFS_INDEX_INFO("voted_peri_index", 9, 0x7),
			DECLARE_DVFS_INDEX_INFO("voted_gic_index", 18, 0x7),
			DECLARE_DVFS_INDEX_INFO("voted_mpll_index", 12, 0x7),
		},
		.tbl_row_num = 10,
		.tbl_column_num = 7,
	},
	[CPU_BIG_CLUSTER] = {
		.regs = {
			GENREGSET(0xa0, 0, 0xffff),
			GENREGSET(0xa4, 0, 0xffff),
			GENREGSET(0xa8, 0, 0xffff),
			GENREGSET(0xac, 0, 0xffff),
			GENREGSET(0xb0, 0, 0xffff),
			GENREGSET(0xb4, 0, 0xffff),
			GENREGSET(0xb8, 0, 0xffff),
		},
		.entry_info = {
			DECLARE_DVFS_INDEX_INFO("sel", 0, 0x3),
			DECLARE_DVFS_INDEX_INFO("div", 2, 0x7),
			DECLARE_DVFS_INDEX_INFO("voted_volt", 15, 0x7),
			DECLARE_DVFS_INDEX_INFO("voted_scu_index", 5, 0xf),
			DECLARE_DVFS_INDEX_INFO("voted_peri_index", 9, 0x7),
			DECLARE_DVFS_INDEX_INFO("voted_gic_index", 21, 0x7),
			DECLARE_DVFS_INDEX_INFO("voted_mpll_index", 12, 0x7),
		},
		.tbl_row_num = 7,
		.tbl_column_num = 7,
	},
};

static
struct dvfs_index_tbl ums312_slave_cluster_index_tbl[SLAVE_CLUSTER_NUM] = {
	[SCU_SLAVE_CLUSTER] = {
		.regs = {
			GENREGSET(0x180, 0, 0xffff),
			GENREGSET(0x184, 0, 0xffff),
			GENREGSET(0x188, 0, 0xffff),
			GENREGSET(0x18c, 0, 0xffff),
			GENREGSET(0x190, 0, 0xffff),
			GENREGSET(0x194, 0, 0xffff),
			GENREGSET(0x198, 0, 0xffff),
			GENREGSET(0x19c, 0, 0xffff),
			GENREGSET(0x1a0, 0, 0xffff),
			GENREGSET(0x1a4, 0, 0xffff),
		},
		.entry_info = {
			DECLARE_DVFS_INDEX_INFO("sel", 0, 0x3),
			DECLARE_DVFS_INDEX_INFO("div", 2, 0x7),
			DECLARE_DVFS_INDEX_INFO("voted_volt", 11, 0x7),
			DECLARE_DVFS_INDEX_INFO("ace_div", 5, 0x7),
			DECLARE_DVFS_INDEX_INFO("voted_mpll_index", 8, 0x7),
		},
		.tbl_row_num = 10,
		.tbl_column_num = 5,
	},
	[PERIPH_SLAVE_CLUSTER] = {
		.regs = {
			GENREGSET(0x1f4, 0, 0xffff),
			GENREGSET(0x1f8, 0, 0xffff),
			GENREGSET(0x1fc, 0, 0xffff),
			GENREGSET(0x200, 0, 0xffff),
			GENREGSET(0x204, 0, 0xffff),
			GENREGSET(0x208, 0, 0xffff),
		},
		.entry_info = {
			DECLARE_DVFS_INDEX_INFO("sel", 0, 0x3),
			DECLARE_DVFS_INDEX_INFO("div", 2, 0x7),
			DECLARE_DVFS_INDEX_INFO("voted_volt", 5, 0x7),
		},
		.tbl_row_num = 6,
		.tbl_column_num = 3,
	},
	[GIC_SLAVE_CLUSTER] = {
		.regs = {
			GENREGSET(0x280, 0, 0xffff),
			GENREGSET(0x284, 0, 0xffff),
			GENREGSET(0x288, 0, 0xffff),
			GENREGSET(0x28c, 0, 0xffff),
			GENREGSET(0x290, 0, 0xffff),
			GENREGSET(0x294, 0, 0xffff),
		},
		.entry_info = {
			DECLARE_DVFS_INDEX_INFO("sel", 0, 0x3),
			DECLARE_DVFS_INDEX_INFO("div", 2, 0x7),
			DECLARE_DVFS_INDEX_INFO("voted_volt", 5, 0x7),
		},
		.tbl_row_num = 6,
		.tbl_column_num = 3,
	},
};

static struct topdvfs_volt_manager ums312_volt_manager = {
	.grade_tbl = ums312_volt_grades_tbl,
	.up_udelay_tbl =  ums312_up_udelay_tbl,
	.down_udelay_tbl = ums312_down_udelay_tbl,
	.host_vol_auto_tune_cfg = ums312_host_vol_auto_tune_cfg,
	.host_freq_auto_tune_cfg = ums312_host_freq_auto_tune_cfg,
	.idle_vol_cfg = ums312_idle_vol_cfg,
	.misc_cfg = ums312_volt_misc_cfg,
	.third_pmic_cfg = ums312_third_pmic_cfg,
	.dcdc_num = DCDC_NUM,
	.vir_dcdc_adi_num = VIR_DCDC_CPU_ADI_NUM,
};

static struct cpudvfs_freq_manager ums312_freq_manager = {
	.slave_freq_auto_tune_cfg = ums312_slave_freq_auto_tune_cfg,
	.hcluster_work_index_cfg = ums312_hcluster_work_index_cfg,
	.hcluster_idle_index_cfg = ums312_hcluster_idle_index_cfg,
	.dev_idle_disable_cfg = ums312_dev_idle_disable_cfg,
	.host_cluster_index_tbl = ums312_host_cluster_index_tbl,
	.slave_cluster_index_tbl = ums312_slave_cluster_index_tbl,
	.misc_cfg = ums312_freq_misc_cfg,
	.host_cluster_num = HOST_CLUSTER_NUM,
	.slave_cluster_num = SLAVE_CLUSTER_NUM,
	.dvfs_device_num = DVFS_DEVICE_NUM,
};

static struct mpll_freq_manager ums312_mpll_manager = {
	.mpll_tbl = ums312_mpll_index_tbl,
	.auto_relock_cfg =  ums312_mpll_auto_relock_cfg,
	.auto_pd_cfg = ums312_mpll_auto_pd_cfg,
	.mpll_num = MPLL_NUM,
};

const struct dvfs_private_data ums312_dvfs_private_data = {
	.module_clk_khz = 128000,
	.pmic = pmic_array,
	.volt_manager = &ums312_volt_manager,
	.freq_manager = &ums312_freq_manager,
	.mpll_manager = &ums312_mpll_manager,
};

struct dvfs_cluster global_host_cluster[HOST_CLUSTER_NUM] = {
	DECLARE_APCPU_DVFS_CLUSTER("lit_core_cluster"),
	DECLARE_APCPU_DVFS_CLUSTER("big_core_cluster"),
};

struct dvfs_cluster global_slave_cluster[SLAVE_CLUSTER_NUM] = {
	DECLARE_APCPU_DVFS_CLUSTER("scu_cluster"),
	DECLARE_APCPU_DVFS_CLUSTER("periph_cluster"),
	DECLARE_APCPU_DVFS_CLUSTER("gic_cluster"),
};
