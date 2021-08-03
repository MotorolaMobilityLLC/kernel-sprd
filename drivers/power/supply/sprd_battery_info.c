/*
 * Copyright (C) 2011 Unisoc Co., Ltd.
 * Jinfeng.Lin <Jinfeng.Lin1@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/slab.h>

#define SPRD_BATTERY_OCV_TABLE_CHECK_VOLT_UV 3400000

static bool sprd_battery_resistance_ocv_table_check(int *ocv_table, int len)
{
	int i;

	if ((ocv_table[0] < SPRD_BATTERY_OCV_TABLE_CHECK_VOLT_UV) ||
	    (ocv_table[len - 1] < SPRD_BATTERY_OCV_TABLE_CHECK_VOLT_UV))
		return false;

	for (i = 0; i < len - 2; i++) {
		if (ocv_table[i] <= ocv_table[i + 1])
			return false;
	}

	return true;
}

static int sprd_battery_resistance_temp_table_check(int resist_temp_size,
						    int *resist_temp_table_size)
{
	int i, temp;

	temp = resist_temp_table_size[0];

	for (i = 1; i < resist_temp_size; i++) {
		if (temp != resist_temp_table_size[i]) {
			pr_err("battery_internal_resistance_table_len[%d] is wrong\n", i);
			return -EINVAL;
		}
	}

	return 0;
}

static int sprd_battery_resistance_ocv_table_size_check(int resist_temp_table_size,
							int resist_ocv_table_size)
{
	if (resist_temp_table_size != resist_ocv_table_size) {
		pr_err("resist_ocv_table_size is wrong\n");
		return -EINVAL;
	}

	return 0;
}

static int sprd_battery_factory_internal_resistance_check(struct sprd_battery_info *info)
{
	int ret;

	ret = sprd_battery_resistance_temp_table_check(info->battery_internal_resistance_temp_table_len,
						       info->battery_internal_resistance_table_len);
	if (ret)
		return ret;

	ret = sprd_battery_resistance_ocv_table_size_check(info->battery_internal_resistance_table_len[0],
							   info->battery_internal_resistance_ocv_table_len);

	return ret;
}

static int sprd_battery_parse_battery_internal_resistance_table(struct sprd_battery_info *info,
								struct device_node *battery_np,
								struct power_supply *psy)
{
	int len, index;

	len = of_property_count_u32_elems(battery_np, "battery-internal-resistance-celsius");
	if (len < 0 && len != -EINVAL) {
		return len;
	} else if (len > SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX) {
		dev_err(&psy->dev, "Too many temperature values\n");
		return -EINVAL;
	} else if (len > 0) {
		info->battery_internal_resistance_temp_table_len = len;
		of_property_read_u32_array(battery_np, "battery-internal-resistance-celsius",
					   info->battery_internal_resistance_temp_table, len);
		for (index = 0; index < len; index++)
			info->battery_internal_resistance_temp_table[index] *= 10;
	}

	for (index = 0; index < len; index++) {
		int *table;
		char *propname;
		const __be32 *list;
		int i, tab_len, size;

		propname = kasprintf(GFP_KERNEL, "battery-internal-resistance-table-%d", index);
		list = of_get_property(battery_np, propname, &size);
		if (!list || !size) {
			dev_err(&psy->dev, "failed to get %s\n", propname);
			kfree(propname);
			sprd_battery_put_battery_info(psy, info);
			return -EINVAL;
		}

		kfree(propname);
		tab_len = size / (sizeof(__be32));
		info->battery_internal_resistance_table_len[index] = tab_len;

		table = info->battery_internal_resistance_table[index] =
			devm_kzalloc(&psy->dev, tab_len * sizeof(*table), GFP_KERNEL);
		if (!info->battery_internal_resistance_table[index]) {
			sprd_battery_put_battery_info(psy, info);
			return -ENOMEM;
		}

		for (i = 0; i < tab_len; i++)
			table[i] = be32_to_cpu(*list++);
	}

	return 0;
}

static int sprd_battery_parse_battery_internal_resistance_ocv_table(struct sprd_battery_info *info,
								    struct device_node *battery_np,
								    struct power_supply *psy)
{
	const __be32 *list;
	int index, size;
	int *resistance_ocv_table;

	list = of_get_property(battery_np, "battery-internal-resistance-ocv-table", &size);
	if (!list || !size)
		return 0;

	info->battery_internal_resistance_ocv_table_len = size / (sizeof(__be32));
	resistance_ocv_table = info->battery_internal_resistance_ocv_table =
		devm_kcalloc(&psy->dev, info->battery_internal_resistance_ocv_table_len,
			     sizeof(*resistance_ocv_table), GFP_KERNEL);
	if (!info->battery_internal_resistance_ocv_table) {
		dev_err(&psy->dev, "battery_internal_resistance_ocv_table is null\n");
		sprd_battery_put_battery_info(psy, info);
		return -ENOMEM;
	}

	for (index = 0; index < info->battery_internal_resistance_ocv_table_len; index++)
		resistance_ocv_table[index] = be32_to_cpu(*list++);

	if (!sprd_battery_resistance_ocv_table_check(resistance_ocv_table,
		info->battery_internal_resistance_ocv_table_len)) {
		dev_err(&psy->dev, "resistance_ocv_table value is wrong, please check\n");
		return -EINVAL;
	}

	return 0;
}

int sprd_battery_get_battery_info(struct power_supply *psy, struct sprd_battery_info *info)
{
	struct device_node *battery_np;
	const char *value;
	int err, index;

	info->charge_full_design_uah         = -EINVAL;
	info->voltage_min_design_uv          = -EINVAL;
	info->precharge_current_ua           = -EINVAL;
	info->charge_term_current_ua         = -EINVAL;
	info->constant_charge_current_max_ua = -EINVAL;
	info->constant_charge_voltage_max_uv = -EINVAL;
	info->factory_internal_resistance_uohm  = -EINVAL;
	info->battery_internal_resistance_ocv_table = NULL;
	info->battery_internal_resistance_temp_table_len = -EINVAL;
	info->battery_internal_resistance_ocv_table_len = -EINVAL;
	info->cur.sdp_cur = -EINVAL;
	info->cur.sdp_limit = -EINVAL;
	info->cur.dcp_cur = -EINVAL;
	info->cur.dcp_limit = -EINVAL;
	info->cur.cdp_cur = -EINVAL;
	info->cur.cdp_limit = -EINVAL;
	info->cur.unknown_cur = -EINVAL;
	info->cur.unknown_limit = -EINVAL;
	info->cur.fchg_cur = -EINVAL;
	info->cur.fchg_limit = -EINVAL;
	info->cur.flash_cur = -EINVAL;
	info->cur.flash_limit = -EINVAL;
	info->cur.wl_bpp_cur = -EINVAL;
	info->cur.wl_bpp_limit = -EINVAL;
	info->cur.wl_epp_cur = -EINVAL;
	info->cur.wl_epp_limit = -EINVAL;

	for (index = 0; index < SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX; index++) {
		info->battery_internal_resistance_temp_table[index] = -EINVAL;
		info->battery_internal_resistance_table[index] = NULL;
		info->battery_internal_resistance_table_len[index] = -EINVAL;
	}

	if (!psy->of_node) {
		dev_warn(&psy->dev, "%s currently only supports devicetree\n", __func__);
		return -ENXIO;
	}

	battery_np = of_parse_phandle(psy->of_node, "monitored-battery", 0);
	if (!battery_np)
		return -ENODEV;

	err = of_property_read_string(battery_np, "compatible", &value);
	if (err)
		return err;

	if (strcmp("simple-battery", value))
		return -ENODEV;

	of_property_read_u32(battery_np, "charge-full-design-microamp-hours",
			     &info->charge_full_design_uah);
	of_property_read_u32(battery_np, "voltage-min-design-microvolt",
			     &info->voltage_min_design_uv);
	of_property_read_u32(battery_np, "precharge-current-microamp",
			     &info->precharge_current_ua);
	of_property_read_u32(battery_np, "charge-term-current-microamp",
			     &info->charge_term_current_ua);
	of_property_read_u32(battery_np, "constant_charge_current_max_microamp",
			     &info->constant_charge_current_max_ua);
	of_property_read_u32(battery_np, "constant_charge_voltage_max_microvolt",
			     &info->constant_charge_voltage_max_uv);
	of_property_read_u32(battery_np, "factory-internal-resistance-micro-ohms",
			     &info->factory_internal_resistance_uohm);
	of_property_read_u32_index(battery_np, "charge-sdp-current-microamp", 0,
				   &info->cur.sdp_cur);
	of_property_read_u32_index(battery_np, "charge-sdp-current-microamp", 1,
				   &info->cur.sdp_limit);
	of_property_read_u32_index(battery_np, "charge-dcp-current-microamp", 0,
				   &info->cur.dcp_cur);
	of_property_read_u32_index(battery_np, "charge-dcp-current-microamp", 1,
				   &info->cur.dcp_limit);
	of_property_read_u32_index(battery_np, "charge-cdp-current-microamp", 0,
				   &info->cur.cdp_cur);
	of_property_read_u32_index(battery_np, "charge-cdp-current-microamp", 1,
				   &info->cur.cdp_limit);
	of_property_read_u32_index(battery_np, "charge-unknown-current-microamp", 0,
				   &info->cur.unknown_cur);
	of_property_read_u32_index(battery_np, "charge-unknown-current-microamp", 1,
				   &info->cur.unknown_limit);
	of_property_read_u32_index(battery_np, "charge-fchg-current-microamp", 0,
				   &info->cur.fchg_cur);
	of_property_read_u32_index(battery_np, "charge-fchg-current-microamp", 1,
				   &info->cur.fchg_limit);
	of_property_read_u32_index(battery_np, "charge-flash-current-microamp", 0,
				   &info->cur.flash_cur);
	of_property_read_u32_index(battery_np, "charge-flash-current-microamp", 1,
				   &info->cur.flash_limit);
	of_property_read_u32_index(battery_np, "charge-wl-bpp-current-microamp", 0,
				   &info->cur.wl_bpp_cur);
	of_property_read_u32_index(battery_np, "charge-wl-bpp-current-microamp", 1,
				   &info->cur.wl_bpp_limit);
	of_property_read_u32_index(battery_np, "charge-wl-epp-current-microamp", 0,
				   &info->cur.wl_epp_cur);
	of_property_read_u32_index(battery_np, "charge-wl-epp-current-microamp", 1,
				   &info->cur.wl_epp_limit);

	err = sprd_battery_parse_battery_internal_resistance_table(info, battery_np, psy);
	if (err) {
		dev_err(&psy->dev, "Fail to get factory internal resistance table, ret = %d\n", err);
		return err;
	}

	err = sprd_battery_parse_battery_internal_resistance_ocv_table(info, battery_np, psy);
	if (err) {
		dev_err(&psy->dev, "Fail to get factory internal resist ocv table, ret = %d\n", err);
		return err;
	}

	err = sprd_battery_factory_internal_resistance_check(info);
	if (err) {
		dev_err(&psy->dev, "factory_internal_resistance is not right, err = %d\n", err);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(sprd_battery_get_battery_info);

void sprd_battery_put_battery_info(struct power_supply *psy, struct sprd_battery_info *info)
{
	int i;

	for (i = 0; i < SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX; i++) {
		if (info->battery_internal_resistance_table[i])
			devm_kfree(&psy->dev, info->battery_internal_resistance_table[i]);
	}

	if (info->battery_internal_resistance_ocv_table)
		devm_kfree(&psy->dev, info->battery_internal_resistance_ocv_table);
}
EXPORT_SYMBOL_GPL(sprd_battery_put_battery_info);

void sprd_battery_find_resistance_table(struct power_supply *psy,
					int **resistance_table, int table_len,
					int *temp_table, int temp_len, int temp,
					int *target_table)
{
	int temp_index, i, delta_res;

	if (!resistance_table[0] || !temp_table || !target_table)
		return;

	for (i = temp_len - 1; i >= 0; i--) {
		if (temp > temp_table[i])
			break;
	}

	temp_index = i;

	if (temp_index == temp_len - 1) {
		memcpy(target_table, resistance_table[temp_len - 1], table_len);
		return;
	} else if (temp_index == -1) {
		memcpy(target_table, resistance_table[0], table_len);
		return;
	}

	for (i = 0; i < table_len; i++) {
		delta_res = DIV_ROUND_CLOSEST((resistance_table[temp_index + 1][i] -
					       resistance_table[temp_index][i]) *
					      (temp - temp_table[temp_index]),
					      (temp_table[temp_index + 1] -
					       temp_table[temp_index]));

		target_table[i] = resistance_table[temp_index][i] + delta_res;
	}
}
EXPORT_SYMBOL_GPL(sprd_battery_find_resistance_table);

