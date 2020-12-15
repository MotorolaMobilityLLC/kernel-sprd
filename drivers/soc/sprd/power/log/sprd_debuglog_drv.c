/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
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
#include <linux/device.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/sprd_sip_svc.h>
#include "sprd_debuglog_data.h"

static struct debug_log *dbg_log;

#if defined(CONFIG_SPRD_SIP_FW)
static int (*plat_match)(char *buff, u32 intc, u32 second, u32 thrid);
#endif

static int display_register_bit(char *reg_name,
					struct reg_bit *bit, int num, u32 value)
{
	u32 val;
	int i;

	if (!bit) {
		pr_info("   #--Bit is null\n");
		return -EINVAL;
	}

	for (i = 0; i < num; ++i) {
		val = (value >> bit[i].offset) & bit[i].mask;
		if (val != bit[i].expect)
			pr_info("   #--%s: %u", bit[i].name, val);
	}

	return 0;
}

static int display_register(char *table_name,
			      struct reg_info *reg, int num, struct regmap *map)
{
	int i, ret;
	u32 val;

	if (!table_name || !reg || !map) {
		pr_info("  ##--Parameters is null\n");
		return -EINVAL;
	}

	for (i = 0; i < num; ++i) {
		ret = regmap_read(map, reg[i].offset, &val);
		if (ret) {
			pr_info("  ##--Read %s.%s(0x%08x) error\n",
					table_name, reg[i].name, reg[i].offset);
			continue;
		}

		pr_info("  ##--%s.%s: 0x%08x\n", table_name, reg[i].name, val);

		ret = display_register_bit(reg[i].name,
						   reg[i].bit, reg[i].num, val);
		if (ret)
			pr_info("  ##--Display %s error\n", reg[i].name);
	}

	return 0;
}

/**
 * debug_sleep_check - display ap deep sleep condition
 */
static int debug_sleep_check(struct device *dev, void *data, int num)
{
	struct device_node *node;
	struct reg_table *table;
	struct regmap *map;
	int ret, i;

	if (!data || !num) {
		dev_err(dev, "Check parameter is error.\n");
		return -EINVAL;
	}

	node = dev->of_node;
	table = (struct reg_table *)data;

	pr_info(" ###--AP DEEP SLEEP CONDITION CHECK\n");

	for (i = 0; i < num; ++i) {
		map = syscon_regmap_lookup_by_phandle(node, table[i].dts);
		if (IS_ERR(map)) {
			pr_info("  ##--Get %s regmap error\n", table[i].dts);
			continue;
		}
		ret = display_register(table[i].name,
					       table[i].reg, table[i].num, map);
		if (ret)
			pr_info("  ##--Display %s error\n", table[i].name);
	}

	pr_info(" ###--END\n");

	return 0;
}

/**
 * debug_wakeup_source - display wake up source
 */
static int debug_wakeup_source(struct device *dev, void *data, int num)
{
#if defined(CONFIG_SPRD_SIP_FW)
	struct sprd_sip_svc_pwr_ops *ops;
	struct sprd_sip_svc_handle *sip;
	u32 major, second, thrid;
	char str[128];
#else
	struct device_node *node;
	struct regmap *map;
	u32 val;
#endif
	struct intc_info *intc;
	u32 inum, ibit;
	int ret;

	if (!data || !num) {
		dev_err(dev, "Wakeup source parameter is error.\n");
		return -EINVAL;
	}

	intc = (struct intc_info *)data;

#if defined(CONFIG_SPRD_SIP_FW)
	sip = sprd_sip_svc_get_handle();
	if (!sip) {
		dev_err(dev, "Get wakeup sip error.\n");
		return -EINVAL;
	}

	ops = &sip->pwr_ops;

	ret = ops->get_wakeup_source(&major, &second, &thrid);
	if (ret) {
		dev_err(dev, "Get wakeup source error.\n");
		return -EINVAL;
	}

	inum = (major >> 16) & 0xFFFF;
	ibit = major & 0xFFFF;
#else
	node = dev->of_node;

	for (inum = 0; inum < num; ++inum) {
		map = syscon_regmap_lookup_by_phandle(node, intc[inum].dts);
		if (IS_ERR(map)) {
			dev_err(dev, "Get %s regmap error\n", intc[inum].dts);
			return -EINVAL;
		}

		ret = regmap_read(map, 0, &val); /* INTC MASK reg offset is 0 */
		if (ret) {
			dev_err(dev, "Read %s%d error\n", "AP_INTC", inum);
			return -EINVAL;
		}

		if (!val)
			continue;

		for (ibit = 0U; !(val & 1); ++ibit)
			val >>= 1;

		break;
	}
#endif

	if (inum >= num || ibit >= 32) {
		dev_err(dev, "Intc num or bit error.\n");
		return -EINVAL;
	}

	pr_info(" ###--WAKEUP SOURCE\n");

	pr_info("  ##--%s(%d:%d)\n", intc[inum].bits[ibit], inum, ibit);

#if defined(CONFIG_SPRD_SIP_FW)
	ret = plat_match ? plat_match(str, major, second, thrid) : -EINVAL;
	if (!ret)
		pr_info("   #--%s\n", str);
#endif

	pr_info(" ###--END\n");

	return 0;
}

/**
 * debug_monitor_scan - display power state in cycle
 */
static int debug_monitor_scan(struct device *dev, void *data, int num)
{
	struct device_node *node;
	struct reg_table *table;
	struct regmap *map;
	int ret, i;

	if (!data || !num) {
		dev_err(dev, "Scan parameter is error.\n");
		return -EINVAL;
	}

	node = dev->of_node;
	table = (struct reg_table *)data;

	pr_info(" ###--POWER DOMAIN STATE\n");

	for (i = 0; i < num; ++i) {
		map = syscon_regmap_lookup_by_phandle(node, table[i].dts);
		if (IS_ERR(map)) {
			pr_info("  ##--Get %s regmap error\n", table[i].dts);
			continue;
		}
		ret = display_register(table[i].name,
					       table[i].reg, table[i].num, map);
		if (ret)
			pr_info("  ##--Display %s error\n", table[i].name);
	}

	pr_info(" ###--END\n");

	return 0;
}

/**
 * debug_driver_probe - add the debug log driver
 */
static int debug_driver_probe(struct platform_device *pdev)
{
	const struct debug_data *data;
	struct device_node *node;
	struct device *dev;
	int ret;

	dev = &pdev->dev;

	dev_info(dev, "Init debug log driver.\n");

	node = dev->of_node;
	if (!node) {
		dev_err(dev, "Not found device node!\n");
		return -ENODEV;
	}

	dbg_log = devm_kzalloc(dev, sizeof(struct debug_log), GFP_KERNEL);
	if (!dbg_log) {
		dev_err(dev, "Debug memory alloc error.\n");
		return -ENOMEM;
	}

	dbg_log->dev = dev;

	data = of_device_get_match_data(dev);
	if (!data) {
		dev_err(dev, "No matched private driver data found\n");
		return -EINVAL;
	}

#if defined(CONFIG_SPRD_SIP_FW)
	plat_match = data->wakeup_source_match;
#endif

	dbg_log->sleep = devm_kzalloc(dev,
					sizeof(struct debug_event), GFP_KERNEL);
	if (!dbg_log->sleep) {
		dev_err(dev, "Debug sleep memory alloc error.\n");
		return -ENOMEM;
	}

	dbg_log->sleep->num = data->check.num;
	dbg_log->sleep->data = data->check.data;
	dbg_log->sleep->ph = debug_sleep_check;

	dbg_log->wakeup = devm_kzalloc(dev,
					sizeof(struct debug_event), GFP_KERNEL);
	if (!dbg_log->wakeup) {
		dev_err(dev, "Debug wakeup memory alloc error.\n");
		return -ENOMEM;
	}

	dbg_log->wakeup->num = data->intc.num;
	dbg_log->wakeup->data = data->intc.data;
	dbg_log->wakeup->ph = debug_wakeup_source;

	dbg_log->monitor = devm_kzalloc(dev,
				      sizeof(struct debug_monitor), GFP_KERNEL);
	if (!dbg_log->monitor) {
		dev_err(dev, "The debug memory alloc error.\n");
		return -ENOMEM;
	}

	ret = of_property_read_u32(node,
				 "sprd,scan-enable", &dbg_log->monitor->enable);
	if (ret) {
		dev_err(dev, "Get scan enable attribute error.\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node,
			     "sprd,scan-interval", &dbg_log->monitor->interval);
	if (ret) {
		dev_err(dev, "Get scan interval attribute error.\n");
		return -EINVAL;
	}

	dbg_log->monitor->event.num = data->monitor.num;
	dbg_log->monitor->event.data = data->monitor.data;
	dbg_log->monitor->event.ph = debug_monitor_scan;

	ret = sprd_debug_log_register(dbg_log);
	if (ret) {
		dev_err(dev, "Register debug log error.\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * debug_driver_remove - remove the debug log driver
 */
static int debug_driver_remove(struct platform_device *pdev)
{
	return sprd_debug_log_unregister();
}

/* Platform debug data */
const struct debug_data __weak sprd_sharkl6pro_debug_data;
const struct debug_data __weak sprd_sharkl6_debug_data;
const struct debug_data __weak sprd_sharkl5pro_debug_data;
const struct debug_data __weak sprd_sharkl5_debug_data;
const struct debug_data __weak sprd_sharkl3_debug_data;
const struct debug_data __weak sprd_sharkle_debug_data;
const struct debug_data __weak sprd_pike2_debug_data;

static const struct of_device_id debug_of_match[] = {
	{
		.compatible = "sprd,debuglog-sharkl6pro",
		.data = &sprd_sharkl6pro_debug_data,
	},
	{
		.compatible = "sprd,debuglog-sharkl6",
		.data = &sprd_sharkl6_debug_data,
	},
	{
		.compatible = "sprd,debuglog-sharkl5pro",
		.data = &sprd_sharkl5pro_debug_data,
	},
	{
		.compatible = "sprd,debuglog-sharkl5",
		.data = &sprd_sharkl5_debug_data,
	},
	{
		.compatible = "sprd,debuglog-sharkl3",
		.data = &sprd_sharkl3_debug_data,
	},
	{
		.compatible = "sprd,debuglog-sharkle",
		.data = &sprd_sharkle_debug_data,
	},
	{
		.compatible = "sprd,debuglog-pike2",
		.data = &sprd_pike2_debug_data,
	},
	{
	},
};
MODULE_DEVICE_TABLE(of, debug_of_match);

static struct platform_driver sprd_debug_driver = {
	.driver = {
		.name = "sprd-debuglog",
		.of_match_table = debug_of_match,
	},
	.probe = debug_driver_probe,
	.remove = debug_driver_remove,
};

module_platform_driver(sprd_debug_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jamesj Chen<Jamesj.Chen@unisoc.com>");
MODULE_DESCRIPTION("sprd debug log driver");
