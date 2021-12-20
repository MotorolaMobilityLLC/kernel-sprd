/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Spreatrum camera pd driver header
 * Copyright (C) 2021 Spreadtrum, Inc.
 * Author: Hongjian Wang <hongjian.wang@spreadtrum.com>
 */


#ifndef SPRD_CAMSYS_DOMAIN_H
#define SPRD_CAMSYS_DOMAIN_H

#include <linux/pm_domain.h>

enum {
       _E_PW_OFF = 0,
       _E_PW_ON  = 1,
};

enum sprd_campw_id {
	CAM_PW_PIKE2,
	CAM_PW_SHARKL3,
	CAM_PW_SHARKL5PRO,
	CAM_PW_SHARKLE,
	CAM_PW_QOGIRN6PRO,
	CAM_PW_QOGIRL6,
	CAM_PW_MAX
};

struct register_gpr {
	struct regmap *gpr;
	uint32_t reg;
	uint32_t mask;
};

struct camsys_power_info;

struct camsys_power_ops {
	long (*sprd_campw_init)(struct platform_device *pdev, struct camsys_power_info *pw_info);
	int (*sprd_cam_domain_eb)(struct camsys_power_info *domain);
	int (*sprd_cam_domain_disable)(struct camsys_power_info *domain);
	int (*sprd_cam_pw_on)(struct camsys_power_info *domain);
	int (*sprd_cam_pw_off)(struct camsys_power_info *domain);
};

struct camsys_power_info {
	atomic_t inited;
	struct mutex mlock;
	struct generic_pm_domain pd;
	struct camsys_power_ops *ops;

	union {
		struct {
			uint8_t mm_qos_ar;
			uint8_t mm_qos_aw;

			struct clk *cam_mm_eb;
			struct clk *cam_mm_ahb_eb;

			struct clk *cam_ahb_clk;
			struct clk *cam_ahb_clk_parent;
			struct clk *cam_ahb_clk_default;

			struct clk *cam_mtx_clk;
			struct clk *cam_mtx_clk_parent;
			struct clk *cam_mtx_clk_default;

			struct clk *isppll_clk;

			struct register_gpr regs[6];
		} l5pro;
		struct {
			struct clk *cam_clk_cphy_cfg_gate_eb;
			struct clk *cam_mm_eb;

			struct clk *cam_ahb_clk;
			struct clk *cam_ahb_clk_default;
			struct clk *cam_ahb_clk_parent;

			struct clk *cam_emc_clk;
			struct clk *cam_emc_clk_default;
			struct clk *cam_emc_clk_parent;

			struct register_gpr syscon_regs[6];
		} l3;
		struct {
			unsigned int chip_id0;
			unsigned int chip_id1;

			struct clk *cam_ckg_eb;
			struct clk *cam_mm_eb;

			struct clk *cam_ahb_clk;
			struct clk *cam_ahb_clk_default;
			struct clk *cam_ahb_clk_parent;

			struct regmap *cam_ahb_gpr;
			struct regmap *pmu_apb_gpr;
			struct regmap *aon_apb_gpr;
		} pike2;
		struct {
			atomic_t users_pw;
			atomic_t users_clk;
			struct clk *cam_clk_cphy_cfg_gate_eb;
			struct clk *cam_ckg_eb;
			struct clk *cam_mm_eb;

			struct clk *cam_ahb_clk;
			struct clk *cam_ahb_clk_default;
			struct clk *cam_ahb_clk_parent;

			struct register_gpr syscon_regs[5];
		} le;
		struct {
			struct clk *cam_mm_eb;
			struct clk *cam_mm_ahb_eb;

			struct clk *cam_ahb_clk;
			struct clk *cam_ahb_clk_parent;
			struct clk *cam_ahb_clk_default;

			struct clk *cam_mtx_clk;
			struct clk *cam_mtx_clk_parent;
			struct clk *cam_mtx_clk_default;

			struct clk *isppll_clk;

			struct register_gpr regs[4];
		} qogirl6;
	} u;
};

extern  struct camsys_power_ops camsys_power_ops_l5pro;
extern  struct camsys_power_ops camsys_power_ops_l3;
extern  struct camsys_power_ops camsys_power_ops_le;
extern  struct camsys_power_ops camsys_power_ops_pike2;
extern  struct camsys_power_ops camsys_power_ops_qogirl6;

int sprd_mm_pw_notify_register(struct notifier_block *nb);
int sprd_mm_pw_notify_unregister(struct notifier_block *nb);
#endif
