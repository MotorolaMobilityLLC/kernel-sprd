/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Spreatrum camera pd driver header
 * Copyright (C) 2021 Spreadtrum, Inc.
 * Author: Hongjian Wang <hongjian.wang@spreadtrum.com>
 */


#ifndef SPRD_CAMSYS_DOMAIN_H
#define SPRD_CAMSYS_DOMAIN_H

enum sprd_campw_id {
	CAM_PW_PIKE2,
	CAM_PW_SHARKL3,
	CAM_PW_SHARKL5PRO,
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
	int (*sprd_campw_init)(struct platform_device *pdev, struct camsys_power_info *pw_info);
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

			struct register_gpr regs[5];
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

			struct register_gpr syscon_regs[5];
		} l3;
	} u;
};

extern  struct camsys_power_ops camsys_power_ops_l3;
extern  struct camsys_power_ops camsys_power_ops_l5pro;

#endif
