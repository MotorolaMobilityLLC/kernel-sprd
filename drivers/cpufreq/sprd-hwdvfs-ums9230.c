// SPDX-License-Identifier: GPL-2.0
//
// Unisoc APCPU HW DVFS driver
//
// Copyright (C) 2020 Unisoc, Inc.
// Author: Jinyu Xiong <jinyu.xiong@unisoc.com>

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


#define DVFS_DEVICE_NUM		12

#define	DEV_CORE0	0
#define	DEV_CORE1	1
#define	DEV_CORE2	2
#define	DEV_CORE3	3
#define	DEV_CORE4	4
#define	DEV_CORE5	5
#define	DEV_CORE6	6
#define	DEV_CORE7	7
#define	DEV_SCU		8
#define	DEV_ATB		9
#define	DEV_PERIPH	10
#define	DEV_GIC		11

#define DCDC_CPU0	0
#define DCDC_CPU1	1
#define VIR_DCDC_CPU_ADI_NUM	DCDC_NUM
#define DCDC_CPU_I2C(n)	(DCDC_CPU##n + VIR_DCDC_CPU_ADI_NUM)

static struct pmic_data pmic_array[MAX_PMIC_TYPE_NUM] = {
	[PMIC_SC2730] = {
		.volt_base = 0,
		.per_step = 3125,
		.margin_us = 40,
		.update = default_dcdc_volt_update,
		.up_cycle_calculate = default_cycle_calculate,
		.down_cycle_calculate = default_cycle_calculate,
	},
	[PMIC_FAN5355_05] = {
		.volt_base = 600000,
		.per_step = 10000,
		.margin_us = 30,
		.update = default_dcdc_volt_update,
		.up_cycle_calculate = default_cycle_calculate,
		.down_cycle_calculate = default_cycle_calculate,
	},
	{
	},
};

/* ums9230 Private Data */
static struct volt_grades_table ums9230_volt_grades_tbl[] = {
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
	[DCDC_CPU_I2C(1)] = {
		.regs_array = {
			GENREGSET(0x12c, 0, 0x7f),
			GENREGSET(0x12c, 8, 0x7f),
			GENREGSET(0x12c, 16, 0x7f),
			GENREGSET(0x12c, 24, 0x7f),
			GENREGSET(0x130, 0, 0x7f),
			GENREGSET(0x130, 8, 0x7f),
			GENREGSET(0x130, 16, 0x7f),
			GENREGSET(0x130, 24, 0x7f),
		},
		.grade_count = 8,
	},
};

static struct udelay_tbl ums9230_up_udelay_tbl[DCDC_NUM] = {
	[DCDC_CPU0] = {
		.tbl = {
			GENREGSET(0x58, 0, 0xffff),
			GENREGSET(0x58, 16, 0xffff),
			GENREGSET(0x54, 0, 0xffff),
			GENREGSET(0x54, 16, 0xffff),
			GENREGSET(0x50, 0, 0xffff),
			GENREGSET(0x50, 16, 0xffff),
			GENREGSET(0x110, 0, 0xffff),
			GENREGSET(0x110, 16, 0xffff),
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
			GENREGSET(0x118, 16, 0xffff),
		},
	},
};

static struct udelay_tbl ums9230_down_udelay_tbl[DCDC_NUM] = {
	[DCDC_CPU0] = {
		.tbl = {
			GENREGSET(0x64, 0, 0xffff),
			GENREGSET(0x64, 16, 0xffff),
			GENREGSET(0x60, 0, 0xffff),
			GENREGSET(0x60, 16, 0xffff),
			GENREGSET(0x5c, 0, 0xffff),
			GENREGSET(0x5c, 16, 0xffff),
			GENREGSET(0x114, 0, 0xffff),
			GENREGSET(0x114, 16, 0xffff),
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
			GENREGSET(0x11c, 16, 0xffff),
		},
	},
};

static struct reg_info ums9230_volt_misc_cfg[] = {
	/* Set the buck_en bit of 3rd dcdc_cpu1_voltage to 1 */
	GENREGVALSET(0x12c, 0, 0xff, 0x8f),
	GENREGVALSET(0x12c, 8, 0xff, 0x94),
	GENREGVALSET(0x12c, 16, 0xff, 0x99),
	GENREGVALSET(0x12c, 24, 0xff, 0x9e),
	GENREGVALSET(0x130, 0, 0xff, 0xa3),
	GENREGVALSET(0x130, 8, 0xff, 0xa8),
	GENREGVALSET(0x130, 16, 0xff, 0xa8),
	GENREGVALSET(0x130, 24, 0xff, 0xa8),
	GENREGVALSET(0, 0, 0, 0),
};

static struct reg_info ums9230_freq_misc_cfg[] = {
	/* Set default work index 10 to mpll for lit core */
	GENREGVALSET(0x214, 0, 0xf, 0xa),
	/* Set default work index 9 to mpll for big core */
	GENREGVALSET(0x224, 0, 0xf, 9),
	/* Set default work index 10 to mpll for scu */
	GENREGVALSET(0x22c, 0, 0xf, 0xa),
	GENREGVALSET(0, 0, 0, 0),
};

static struct mpll_index_tbl ums9230_mpll_index_tbl[MPLL_NUM] = {
	[MPLL0] = {
		.entry = {
			/* MPLL0 Index0 */
			{
				.icp = GENREGVALSET(0xa8, 0, 0x7, 0),
				.postdiv = GENREGVALSET(0xa8, 3, 0x1, 0),
				.n = GENREGVALSET(0xa8, 4, 0x7ff, 0x22),
			},
			/* MPLL0 Index1 */
			{
				.icp = GENREGVALSET(0xac, 0, 0x7, 0),
				.postdiv = GENREGVALSET(0xac, 3, 0x1, 0),
				.n = GENREGVALSET(0xac, 4, 0x7ff, 0x28),
			},
			/* MPLL0 Index2 */
			{
				.icp = GENREGVALSET(0xb0, 0, 0x7, 1),
				.postdiv = GENREGVALSET(0xb0, 3, 0x1, 0),
				.n = GENREGVALSET(0xb0, 4, 0x7ff, 0x35),
			},
			/* MPLL0 Index3 */
			{
				.icp = GENREGVALSET(0xb4, 0, 0x7, 3),
				.postdiv = GENREGVALSET(0xb4, 3, 0x1, 0),
				.n = GENREGVALSET(0xb4, 4, 0x7ff, 0x3e),
			},
			/* MPLL0 Index4 */
			{
				.icp = GENREGVALSET(0xb8, 0, 0x7, 3),
				.postdiv = GENREGVALSET(0xb8, 3, 0x1, 0),
				.n = GENREGVALSET(0xb8, 4, 0x7ff, 0x42),
			},
			/* MPLL0 Index5 */
			{
				.icp = GENREGVALSET(0xbc, 0, 0x7, 4),
				.postdiv = GENREGVALSET(0xbc, 3, 0x1, 0),
				.n = GENREGVALSET(0xbc, 4, 0x7ff, 0x46),
			},
			/* MPLL0 Index6 */
			{
				.icp = GENREGVALSET(0xc0, 0, 0x7, 4),
				.postdiv = GENREGVALSET(0xc0, 3, 0x1, 0),
				.n = GENREGVALSET(0xc0, 4, 0x7ff, 0x4b),
			},
			/* MPLL0 Index7 */
			{
				.icp = GENREGVALSET(0xc4, 0, 0x7, 5),
				.postdiv = GENREGVALSET(0xc4, 3, 0x1, 0),
				.n = GENREGVALSET(0xc4, 4, 0x7ff, 0x4d),
			},
		},
	},

	[MPLL1] = {
		.entry = {
			/* MPLL1 Index0 */
			{
				.icp = GENREGVALSET(0x78, 0, 0x7, 0),
				.postdiv = GENREGVALSET(0x78, 3, 0x1, 0),
				.n = GENREGVALSET(0x78, 4, 0x7ff, 0x28),
			},
			/* MPLL1 Index1 */
			{
				.icp = GENREGVALSET(0x7c, 0, 0x7, 2),
				.postdiv = GENREGVALSET(0x7c, 3, 0x1, 0),
				.n = GENREGVALSET(0x7c, 4, 0x7ff, 0x36),
			},
			/* MPLL1 Index2 */
			{
				.icp = GENREGVALSET(0x80, 0, 0x7, 3),
				.postdiv = GENREGVALSET(0x80, 3, 0x1, 0),
				.n = GENREGVALSET(0x80, 4, 0x7ff, 0x3e),
			},
			/* MPLL1 Index3 */
			{
				.icp = GENREGVALSET(0x84, 0, 0x7, 3),
				.postdiv = GENREGVALSET(0x84, 3, 0x1, 0),
				.n = GENREGVALSET(0x84, 4, 0x7ff, 0x41),
			},
			/* MPLL1 Index4 */
			{
				.icp = GENREGVALSET(0x88, 0, 0x7, 4),
				.postdiv = GENREGVALSET(0x88, 3, 0x1, 0),
				.n = GENREGVALSET(0x88, 4, 0x7ff, 0x46),
			},
			/* MPLL1 Index5 */
			{
				.icp = GENREGVALSET(0x8c, 0, 0x7, 4),
				.postdiv = GENREGVALSET(0x8c, 3, 0x1, 0),
				.n = GENREGVALSET(0x8c, 4, 0x7ff, 0x49),
			},
			/* MPLL1 Index6 */
			{
				.icp = GENREGVALSET(0x90, 0, 0x7, 4),
				.postdiv = GENREGVALSET(0x90, 3, 0x1, 0),
				.n = GENREGVALSET(0x90, 4, 0x7ff, 0x4b),
			},
			/* MPLL1 Index7 */
			{
				.icp = GENREGVALSET(0x94, 0, 0x7, 5),
				.postdiv = GENREGVALSET(0x94, 3, 0x1, 0),
				.n = GENREGVALSET(0x94, 4, 0x7ff, 0x4d),
			},
		},
	},

	[MPLL2] = {
		.entry = {
			/* MPLL2 Index0 */
			{
				.icp = GENREGVALSET(0xec, 0, 0x7, 0),
				.postdiv = GENREGVALSET(0xec, 3, 0x1, 1),
				.n = GENREGVALSET(0xec, 4, 0x7ff, 0x2a),
			},
			/* MPLL2 Index1 */
			{
				.icp = GENREGVALSET(0xf0, 0, 0x7, 2),
				.postdiv = GENREGVALSET(0xf0, 3, 0x1, 1),
				.n = GENREGVALSET(0xf0, 4, 0x7ff, 0x36),
			},
			/* MPLL2 Index2 */
			{
				.icp = GENREGVALSET(0xf4, 0, 0x7, 4),
				.postdiv = GENREGVALSET(0xf4, 3, 0x1, 1),
				.n = GENREGVALSET(0xf4, 4, 0x7ff, 0x4a),
			},
			/* MPLL2 Index3 */
			{
				.icp = GENREGVALSET(0xf8, 0, 0x7, 0),
				.postdiv = GENREGVALSET(0xf8, 3, 0x1, 0),
				.n = GENREGVALSET(0xf8, 4, 0x7ff, 0x2a),
			},
			/* MPLL2 Index4 */
			{
				.icp = GENREGVALSET(0xfc, 0, 0x7, 1),
				.postdiv = GENREGVALSET(0xfc, 3, 0x1, 0),
				.n = GENREGVALSET(0xfc, 4, 0x7ff, 0x31),
			},
			/* MPLL2 Index5 */
			{
				.icp = GENREGVALSET(0x100, 0, 0x7, 1),
				.postdiv = GENREGVALSET(0x100, 3, 0x1, 0),
				.n = GENREGVALSET(0x100, 4, 0x7ff, 0x33),
			},
			/* MPLL2 Index6 */
			{
				.icp = GENREGVALSET(0x104, 0, 0x7, 1),
				.postdiv = GENREGVALSET(0x104, 3, 0x1, 0),
				.n = GENREGVALSET(0x104, 4, 0x7ff, 0x34),
			},
			/* MPLL2 Index7 */
			{
				.icp = GENREGVALSET(0x108, 0, 0x7, 1),
				.postdiv = GENREGVALSET(0x108, 3, 0x1, 0),
				.n = GENREGVALSET(0x108, 4, 0x7ff, 0x34),
			},
		},
	},
};

static struct reg_info ums9230_mpll_auto_relock_cfg[MPLL_NUM] = {
	[MPLL0] = GENREGVALSET(0x244, 1, 0x1, 1),
	[MPLL1] = GENREGVALSET(0x258, 1, 0x1, 1),
	[MPLL2] = GENREGVALSET(0x26c, 1, 0x1, 1),
};

static struct reg_info ums9230_mpll_auto_pd_cfg[MPLL_NUM] = {
	[MPLL0] = GENREGVALSET(0x244, 0, 0x1, 1),
	[MPLL1] = GENREGVALSET(0x258, 0, 0x1, 1),
	[MPLL2] = GENREGVALSET(0x26c, 0, 0x1, 1),
};

static struct reg_info ums9230_idle_vol_cfg[DCDC_NUM] = {
	[DCDC_CPU0] = GENREGVALSET(0x16c, 3, 0x7, 1),
	[DCDC_CPU1] = GENREGVALSET(0x16c, 0, 0x7, 1),
};

static struct reg_info ums9230_third_pmic_cfg[DCDC_NUM] = {
	[DCDC_CPU1] = GENREGSET(0x120, 0, 0x1),
};

static struct reg_info ums9230_host_vol_auto_tune_cfg[DCDC_NUM] = {
	[DCDC_CPU0] = GENREGVALSET(0x68, 20, 0x1, 0),
	[DCDC_CPU1] = GENREGVALSET(0x94, 0, 0x1, 0),
};

static struct reg_info ums9230_host_freq_auto_tune_cfg[HOST_CLUSTER_NUM] = {
	[CPU_LIT_CLUSTER] = GENREGVALSET(0x150, 6, 0x1, 0),
	[CPU_BIG_CLUSTER] = GENREGVALSET(0x150, 7, 0x1, 0),
};

static struct reg_info ums9230_slave_freq_auto_tune_cfg[SLAVE_CLUSTER_NUM] = {
	[SCU_SLAVE_CLUSTER] = GENREGVALSET(0x18, 1, 0x1, 1),
	[PERIPH_SLAVE_CLUSTER] = GENREGVALSET(0x18, 0, 0x1, 1),
	[GIC_SLAVE_CLUSTER] = GENREGVALSET(0x18, 2, 0x1, 1),
};

static struct reg_info ums9230_hcluster_work_index_cfg[HOST_CLUSTER_NUM] = {
	[CPU_LIT_CLUSTER] = GENREGSET(0x214, 0, 0xf),
	[CPU_BIG_CLUSTER] = GENREGSET(0x224, 0, 0xf),
};

static struct reg_info ums9230_hcluster_idle_index_cfg[HOST_CLUSTER_NUM] = {
	[CPU_LIT_CLUSTER] = GENREGSET(0x218, 0, 0xf),
	[CPU_BIG_CLUSTER] = GENREGSET(0x228, 0, 0xf),
};

static struct reg_info ums9230_dev_idle_disable_cfg[DVFS_DEVICE_NUM] = {
	[DEV_CORE0] = GENREGVALSET(0x1c, 0, 0x1, 1),
	[DEV_CORE1] = GENREGVALSET(0x1c, 1, 0x1, 1),
	[DEV_CORE2] = GENREGVALSET(0x1c, 2, 0x1, 1),
	[DEV_CORE3] = GENREGVALSET(0x1c, 3, 0x1, 1),
	[DEV_CORE4] = GENREGVALSET(0x1c, 8, 0x1, 1),
	[DEV_CORE5] = GENREGVALSET(0x1c, 9, 0x1, 1),
	[DEV_CORE6] = GENREGVALSET(0x1c, 10, 0x1, 1),
	[DEV_CORE7] = GENREGVALSET(0x1c, 11, 0x1, 1),
	[DEV_SCU] = GENREGVALSET(0x1c, 4, 0x1, 1),
	[DEV_ATB] = GENREGVALSET(0x1c, 5, 0x1, 1),
	[DEV_PERIPH] = GENREGVALSET(0x1c, 6, 0x1, 1),
	[DEV_GIC] = GENREGVALSET(0x1c, 7, 0x1, 1),
};

static
struct dvfs_index_tbl ums9230_host_cluster_index_tbl[HOST_CLUSTER_NUM] = {
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
			GENREGSET(0x88, 0, 0xffff),
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
		.tbl_row_num = 11,
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
			GENREGSET(0xbc, 0, 0xffff),
			GENREGSET(0xc0, 0, 0xffff),
			GENREGSET(0xc4, 0, 0xffff),
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
		.tbl_row_num = 10,
		.tbl_column_num = 7,
	},
};

static
struct dvfs_index_tbl ums9230_slave_cluster_index_tbl[SLAVE_CLUSTER_NUM] = {
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
			GENREGSET(0x1a8, 0, 0xffff),
		},
		.entry_info = {
			DECLARE_DVFS_INDEX_INFO("sel", 0, 0x3),
			DECLARE_DVFS_INDEX_INFO("div", 2, 0x7),
			DECLARE_DVFS_INDEX_INFO("voted_volt", 11, 0x7),
			DECLARE_DVFS_INDEX_INFO("ace_div", 5, 0x7),
			DECLARE_DVFS_INDEX_INFO("voted_mpll_index", 8, 0x7),
		},
		.tbl_row_num = 11,
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

static struct topdvfs_volt_manager ums9230_volt_manager = {
	.grade_tbl = ums9230_volt_grades_tbl,
	.up_udelay_tbl =  ums9230_up_udelay_tbl,
	.down_udelay_tbl = ums9230_down_udelay_tbl,
	.host_vol_auto_tune_cfg = ums9230_host_vol_auto_tune_cfg,
	.host_freq_auto_tune_cfg = ums9230_host_freq_auto_tune_cfg,
	.idle_vol_cfg = ums9230_idle_vol_cfg,
	.misc_cfg = ums9230_volt_misc_cfg,
	.third_pmic_cfg = ums9230_third_pmic_cfg,
	.dcdc_num = DCDC_NUM,
	.vir_dcdc_adi_num = VIR_DCDC_CPU_ADI_NUM,
};

static struct cpudvfs_freq_manager ums9230_freq_manager = {
	.slave_freq_auto_tune_cfg = ums9230_slave_freq_auto_tune_cfg,
	.hcluster_work_index_cfg = ums9230_hcluster_work_index_cfg,
	.hcluster_idle_index_cfg = ums9230_hcluster_idle_index_cfg,
	.dev_idle_disable_cfg = ums9230_dev_idle_disable_cfg,
	.host_cluster_index_tbl = ums9230_host_cluster_index_tbl,
	.slave_cluster_index_tbl = ums9230_slave_cluster_index_tbl,
	.misc_cfg = ums9230_freq_misc_cfg,
	.host_cluster_num = HOST_CLUSTER_NUM,
	.slave_cluster_num = SLAVE_CLUSTER_NUM,
	.dvfs_device_num = DVFS_DEVICE_NUM,
};

static struct mpll_freq_manager ums9230_mpll_manager = {
	.mpll_tbl = ums9230_mpll_index_tbl,
	.auto_relock_cfg =  ums9230_mpll_auto_relock_cfg,
	.auto_pd_cfg = ums9230_mpll_auto_pd_cfg,
	.mpll_num = MPLL_NUM,
};

static struct dvfs_cluster host_cluster[HOST_CLUSTER_NUM] = {
	DECLARE_APCPU_DVFS_CLUSTER("lit_core_cluster"),
	DECLARE_APCPU_DVFS_CLUSTER("big_core_cluster"),
};

static struct dvfs_cluster slave_cluster[SLAVE_CLUSTER_NUM] = {
	DECLARE_APCPU_DVFS_CLUSTER("scu_cluster"),
	DECLARE_APCPU_DVFS_CLUSTER("periph_cluster"),
	DECLARE_APCPU_DVFS_CLUSTER("gic_cluster"),
};

const struct dvfs_private_data ums9230_dvfs_private_data = {
	.module_clk_khz = 128000,
	.pmic = pmic_array,
	.volt_manager = &ums9230_volt_manager,
	.freq_manager = &ums9230_freq_manager,
	.mpll_manager = &ums9230_mpll_manager,
	.host_cluster = host_cluster,
	.slave_cluster = slave_cluster,
};

MODULE_LICENSE("GPL v2");
