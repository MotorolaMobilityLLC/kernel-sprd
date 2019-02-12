/*
 * PCIe host controller driver for Spreadtrum SoCs
 *
 * Copyright (C) 2019 Spreadtrum corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "pcie-designware.h"
#include "pcie-sprd.h"

#define NUM_OF_ARGS 5

int sprd_pcie_syscon_setting(struct platform_device *pdev, char *env)
{
	struct device_node *np = pdev->dev.of_node;
	int i, count, err;
	u32 type, delay, reg, mask, val, tmp_val;
	struct of_phandle_args out_args;
	struct regmap *iomap;

	if (!of_find_property(np, env, NULL)) {
		dev_info(&pdev->dev,
			 "it's not a real SoC, please ignore pcie syscons\n");
		return 0;
	}

	/* one handle and NUM_OF_ARGS args */
	count = of_property_count_elems_of_size(np, env,
		(NUM_OF_ARGS + 1) * sizeof(u32));
	dev_dbg(&pdev->dev, "property (%s) reg count is %d :\n", env, count);

	for (i = 0; i < count; i++) {
		err = of_parse_phandle_with_fixed_args(np, env, NUM_OF_ARGS,
				i, &out_args);
		if (err < 0)
			return err;

		type = out_args.args[0];
		delay = out_args.args[1];
		reg = out_args.args[2];
		mask = out_args.args[3];
		val = out_args.args[4];

		iomap = syscon_node_to_regmap(out_args.np);
		switch (type) {
		case 0:
			regmap_update_bits(iomap, reg, mask, val);
			break;

		case 1:
			regmap_read(iomap, reg, &tmp_val);
			tmp_val &= (~mask);
			tmp_val |= (val & mask);
			regmap_write(iomap, reg, tmp_val);
			break;
		}
		if (delay)
			msleep(delay);

		regmap_read(iomap, reg, &tmp_val);
		dev_dbg(&pdev->dev,
			"%2d:reg[0x%8x] mask[0x%8x] val[0x%8x] result[0x%8x]\n",
			i, reg, mask, val, tmp_val);
	}

	return i;
}
