/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _GSP_INTERFACE_PIKE2_H
#define _GSP_INTERFACE_PIKE2_H

#include <linux/of.h>
#include <linux/regmap.h>
#include "../gsp_interface.h"


#define GSP_PIKE2 "pike2"

struct gsp_interface_pike2 {
	void __iomem *gsp_qos_base;
	struct gsp_interface common;
	struct regmap *module_en_regmap;
	struct regmap *reset_regmap;
};

int gsp_interface_pike2_parse_dt(struct gsp_interface *intf,
				  struct device_node *node);

int gsp_interface_pike2_init(struct gsp_interface *intf);
int gsp_interface_pike2_deinit(struct gsp_interface *intf);

int gsp_interface_pike2_prepare(struct gsp_interface *intf);
int gsp_interface_pike2_unprepare(struct gsp_interface *intf);

int gsp_interface_pike2_reset(struct gsp_interface *intf);

void gsp_interface_pike2_dump(struct gsp_interface *inf);

#endif
