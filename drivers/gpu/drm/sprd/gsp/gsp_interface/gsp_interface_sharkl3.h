/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _GSP_INTERFACE_SHARKL3_H
#define _GSP_INTERFACE_SHARKL3_H

#include <linux/of.h>
#include <linux/regmap.h>
#include "../gsp_interface.h"


#define GSP_SHARKL3 "sharkl3"

#define SHARKL3_AON_APB_DISP_EB_NAME	  ("clk_aon_apb_disp_eb")

struct gsp_interface_sharkl3 {
	struct gsp_interface common;

	struct clk *clk_aon_apb_disp_eb;
	struct regmap *module_en_regmap;
	struct regmap *reset_regmap;
};

int gsp_interface_sharkl3_parse_dt(struct gsp_interface *intf,
				struct device_node *node);

int gsp_interface_sharkl3_init(struct gsp_interface *intf);
int gsp_interface_sharkl3_deinit(struct gsp_interface *intf);

int gsp_interface_sharkl3_prepare(struct gsp_interface *intf);
int gsp_interface_sharkl3_unprepare(struct gsp_interface *intf);

int gsp_interface_sharkl3_reset(struct gsp_interface *intf);

void gsp_interface_sharkl3_dump(struct gsp_interface *inf);

#endif
