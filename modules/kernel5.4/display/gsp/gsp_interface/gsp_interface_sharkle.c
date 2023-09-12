// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include "gsp_interface_sharkle.h"
#include "../gsp_debug.h"
#include "../gsp_interface.h"


int gsp_interface_sharkle_parse_dt(struct gsp_interface *intf,
				  struct device_node *node)
{
	return 0;
}

int gsp_interface_sharkle_init(struct gsp_interface *intf)
{
	return 0;
}

int gsp_interface_sharkle_deinit(struct gsp_interface *intf)
{
	return 0;
}

int gsp_interface_sharkle_prepare(struct gsp_interface *intf)
{
	return 0;
}

int gsp_interface_sharkle_unprepare(struct gsp_interface *intf)
{
	return 0;
}

int gsp_interface_sharkle_reset(struct gsp_interface *intf)
{
	return 0;
}

void gsp_interface_sharkle_dump(struct gsp_interface *intf)
{

}
