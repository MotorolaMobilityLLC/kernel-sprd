// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 Unisoc Co., Ltd.
 * Jinfeng.Lin <Jinfeng.Lin1@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/slab.h>

#define SPRD_BATTERY_OCV_TABLE_CHECK_VOLT_UV		3400000
#define SPRD_BATTERY_OCV_CAP_TABLE_CHECK_VOLT_THRESHOLD	3000000

static int sprd_battery_parse_cmdline_match(struct power_supply *psy,
					    char *match_str, char *result, int size)
{
	struct device_node *cmdline_node = NULL;
	const char *cmdline;
	char *match, *match_end;
	int len, match_str_len, ret;

	if (!result || !match_str)
		return -EINVAL;

	memset(result, '\0', size);
	match_str_len = strlen(match_str);

	cmdline_node = of_find_node_by_path("/chosen");
	if (!cmdline_node) {
		dev_warn(&psy->dev, "%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = of_property_read_string(cmdline_node, "bootargs", &cmdline);
	if (ret) {
		dev_warn(&psy->dev, "%s failed to read bootargs\n", __func__);
		return -EINVAL;
	}

	match = strstr(cmdline, match_str);
	if (!match) {
		dev_warn(&psy->dev, "Mmatch: %s fail in cmdline\n", match_str);
		return -EINVAL;
	}

	match_end = strstr((match + match_str_len), " ");
	if (!match_end) {
		dev_warn(&psy->dev, "Match end of : %s fail in cmdline\n", match_str);
		return -EINVAL;
	}

	len = match_end - (match + match_str_len);
	if (len < 0 || len > size) {
		dev_warn(&psy->dev, "Match cmdline :%s fail, len = %d\n", match_str, len);
		return -EINVAL;
	}

	memcpy(result, (match + match_str_len), len);

	return 0;
}

int sprd_battery_parse_battery_id(struct power_supply *psy)
{
	char *str = "bat.id=";
	char result[32] = {};
	int id = 0, ret;

	ret = sprd_battery_parse_cmdline_match(psy, str, result, sizeof(result));
	if (!ret) {
		ret = kstrtoint(result, 10, &id);
		if (ret) {
			id = 0;
			dev_err(&psy->dev, "Covert bat_id fail, ret = %d, result = %s\n",
				ret, result);
		}
	}
	dev_info(&psy->dev, "Batteryid = %d\n", id);

	return id;
}
EXPORT_SYMBOL_GPL(sprd_battery_parse_battery_id);

static bool sprd_battery_ocv_cap_table_check(struct power_supply *psy,
					     struct sprd_battery_ocv_table *table,
					     int table_len)
{
	int i;

	if (table[table_len - 1].ocv / SPRD_BATTERY_OCV_CAP_TABLE_CHECK_VOLT_THRESHOLD != 1 ||
	    table[0].capacity != 100 || table[table_len - 1].capacity != 0)
		return false;
	for (i = 0; i < table_len - 2; i++) {
		if (table[i].ocv <= table[i + 1].ocv ||
		    table[i].capacity <= table[i + 1].capacity) {
			dev_info(&psy->dev, "the index = %d or %d ocv_cap table value is abnormal!\n",
				 i, i + 1);
			return false;
		}
	}

	return true;
}

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

static int sprd_battery_parse_battery_ocv_capacity_table(struct sprd_battery_info *info,
							 struct device_node *battery_np,
							 struct power_supply *psy)
{
	int len, index;

	len = of_property_count_u32_elems(battery_np, "ocv-capacity-celsius");
	if (len < 0 && len != -EINVAL) {
		return len;
	} else if (len > SPRD_BATTERY_OCV_TEMP_MAX) {
		dev_err(&psy->dev, "Too many temperature values\n");
		return -EINVAL;
	} else if (len > 0) {
		of_property_read_u32_array(battery_np, "ocv-capacity-celsius",
					   info->battery_ocv_temp_table, len);
	}

	for (index = 0; index < len; index++) {
		struct sprd_battery_ocv_table *table;
		char *propname;
		const __be32 *list;
		int i, tab_len, size;

		propname = kasprintf(GFP_KERNEL, "ocv-capacity-table-%d", index);
		list = of_get_property(battery_np, propname, &size);
		if (!list || !size) {
			dev_err(&psy->dev, "failed to get %s\n", propname);
			kfree(propname);
			return -EINVAL;
		}

		kfree(propname);
		tab_len = size / (2 * sizeof(__be32));
		info->battery_ocv_table_len[index] = tab_len;

		table = info->battery_ocv_table[index] =
			devm_kcalloc(&psy->dev, tab_len, sizeof(*table), GFP_KERNEL);
		if (!info->battery_ocv_table[index])
			return -ENOMEM;

		for (i = 0; i < tab_len; i++) {
			table[i].ocv = be32_to_cpu(*list++);
			table[i].capacity = be32_to_cpu(*list++);
		}

		if (!sprd_battery_ocv_cap_table_check(psy, info->battery_ocv_table[index],
						      info->battery_ocv_table_len[index])) {
			dev_err(&psy->dev, "ocv_cap_table value is wrong, please check\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int sprd_battery_parse_battery_voltage_temp_table(struct sprd_battery_info *info,
							 struct device_node *battery_np,
							 struct power_supply *psy)
{
	struct sprd_battery_vol_temp_table *battery_vol_temp_table;
	const __be32 *list;
	int index, len;

	list = of_get_property(battery_np, "voltage-temp-table", &len);
	if (!list || !len)
		return 0;

	info->battery_vol_temp_table_len = len / (2 * sizeof(__be32));
	battery_vol_temp_table = info->battery_vol_temp_table =
		devm_kcalloc(&psy->dev, info->battery_vol_temp_table_len,
			     sizeof(*battery_vol_temp_table), GFP_KERNEL);
	if (!info->battery_vol_temp_table)
		return -ENOMEM;

	for (index = 0; index < info->battery_vol_temp_table_len; index++) {
		battery_vol_temp_table[index].vol = be32_to_cpu(*list++);
		battery_vol_temp_table[index].temp = be32_to_cpu(*list++);
	}

	return 0;
}

static int sprd_battery_parse_battery_temp_capacity_table(struct sprd_battery_info *info,
							  struct device_node *battery_np,
							  struct power_supply *psy)
{
	struct sprd_battery_temp_cap_table *battery_temp_cap_table;
	const __be32 *list;
	int index, len;

	list = of_get_property(battery_np, "capacity-temp-table", &len);
	if (!list || !len)
		return 0;

	info->battery_temp_cap_table_len = len / (2 * sizeof(__be32));
	battery_temp_cap_table = info->battery_temp_cap_table =
		devm_kcalloc(&psy->dev, info->battery_temp_cap_table_len,
			     sizeof(*battery_temp_cap_table), GFP_KERNEL);
	if (!info->battery_temp_cap_table)
		return -ENOMEM;

	for (index = 0; index < info->battery_temp_cap_table_len; index++) {
		battery_temp_cap_table[index].temp = be32_to_cpu(*list++);
		battery_temp_cap_table[index].cap = be32_to_cpu(*list++);
	}

	return 0;
}

static int sprd_battery_parse_battery_resistance_temp_table(struct sprd_battery_info *info,
							    struct device_node *battery_np,
							    struct power_supply *psy)
{
	struct sprd_battery_resistance_temp_table *resist_table;
	const __be32 *list;
	int index, len;

	list = of_get_property(battery_np, "resistance-temp-table", &len);
	if (!list || !len)
		return 0;

	info->battery_temp_resist_table_len = len / (2 * sizeof(__be32));
	resist_table = info->battery_temp_resist_table = devm_kcalloc(&psy->dev,
							 info->battery_temp_resist_table_len,
							 sizeof(*resist_table),
							 GFP_KERNEL);
	if (!info->battery_temp_resist_table)
		return -ENOMEM;

	for (index = 0; index < info->battery_temp_resist_table_len; index++) {
		resist_table[index].temp = be32_to_cpu(*list++);
		resist_table[index].resistance = be32_to_cpu(*list++);
	}

	return 0;
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
		int i, size;
		u32 tab_len;

		propname = kasprintf(GFP_KERNEL, "battery-internal-resistance-table-%d", index);
		list = of_get_property(battery_np, propname, &size);
		if (!list || !size) {
			dev_err(&psy->dev, "failed to get %s\n", propname);
			kfree(propname);
			return -EINVAL;
		}

		kfree(propname);
		tab_len = (u32)size / (sizeof(__be32));
		info->battery_internal_resistance_table_len[index] = tab_len;

		table = info->battery_internal_resistance_table[index] =
			devm_kzalloc(&psy->dev, tab_len * sizeof(*table), GFP_KERNEL);
		if (!info->battery_internal_resistance_table[index]) {
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

	info->battery_internal_resistance_ocv_table_len = (u32)size / (sizeof(__be32));
	resistance_ocv_table = info->battery_internal_resistance_ocv_table =
		devm_kcalloc(&psy->dev, info->battery_internal_resistance_ocv_table_len,
			     sizeof(*resistance_ocv_table), GFP_KERNEL);
	if (!info->battery_internal_resistance_ocv_table) {
		dev_err(&psy->dev, "battery_internal_resistance_ocv_table is null\n");
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

static int sprd_battery_energy_density_ocv_table_check(density_ocv_table *table, int len)
{
	int i;

	for (i = 0; i < len - 1; i++) {
		if (table[i].engy_dens_ocv_lo >= table[i].engy_dens_ocv_hi)
			return false;
	}

	for (i = 0; i < len - 2; i++) {
		if ((table[i].engy_dens_ocv_lo >= table[i + 1].engy_dens_ocv_lo) ||
		    (table[i].engy_dens_ocv_hi >= table[i + 1].engy_dens_ocv_hi))
			return false;
	}

	return true;
}

static int sprd_battery_parse_energy_density_ocv_table(struct sprd_battery_info *info,
						       struct device_node *battery_np,
						       struct power_supply *psy)
{
	struct sprd_battery_energy_density_ocv_table *table;
	const __be32 *list;
	int i, size;

	list = of_get_property(battery_np, "energy-desity-ocv-table", &size);
	if (!list || !size)
		return 0;

	info->dens_ocv_table_len = size / (sizeof(density_ocv_table) /
					   sizeof(int) * sizeof(__be32));

	table = devm_kzalloc(&psy->dev, sizeof(density_ocv_table) *
			     (info->dens_ocv_table_len + 1), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	for (i = 0; i < info->dens_ocv_table_len; i++) {
		table[i].engy_dens_ocv_lo = be32_to_cpu(*list++);
		table[i].engy_dens_ocv_hi = be32_to_cpu(*list++);
		dev_info(&psy->dev, "engy_dens_ocv_hi = %d, engy_dens_ocv_lo = %d\n",
			 table[i].engy_dens_ocv_hi, table[i].engy_dens_ocv_lo);
	}

	info->dens_ocv_table = table;

	if (!sprd_battery_energy_density_ocv_table_check(info->dens_ocv_table,
							 info->dens_ocv_table_len)) {
		dev_err(&psy->dev, "density ocv table value is wrong, please check\n");
		return -EINVAL;
	}

	return 0;
}

static int sprd_battery_parse_basp_ocv_table(struct sprd_battery_info *info,
					     struct device_node *battery_np,
					     struct power_supply *psy)
{
	int fcc_table_len, cc_voltage_max_table_len, ocv_table_len;
	const __be32 *list;
	int i, index, size;
	int *fcc_table, *cc_voltage_max_table;
	struct sprd_battery_ocv_table *ocv_table;
	char *propname;

	/* parse  basp-charge-full-design-microamp-hours*/
	list = of_get_property(battery_np, "basp-charge-full-design-microamp-hours", &size);
	if (!list || !size)
		return 0;

	fcc_table_len = size / (sizeof(__be32));
	fcc_table = devm_kcalloc(&psy->dev, fcc_table_len, sizeof(*fcc_table), GFP_KERNEL);
	if (!fcc_table) {
		dev_err(&psy->dev, "basp_charge_full_design_uah_table is null\n");
		return -ENOMEM;
	}

	for (index = 0; index < fcc_table_len; index++)
		fcc_table[index] = be32_to_cpu(*list++);

	/* Check basp_charge_full_design_uah_table */
	for (index = 0; index < fcc_table_len - 1; index++) {
		if (fcc_table[index] <= fcc_table[index + 1]) {
			dev_err(&psy->dev, "basp_fcc_uah_table is wrong, please check\n");
			return -EINVAL;
		}
	}

	info->basp_charge_full_design_uah_table = fcc_table;
	info->basp_charge_full_design_uah_table_len = fcc_table_len;

	/* parse  basp_constant_charge_voltage_max_microvolt*/
	list = of_get_property(battery_np, "basp-constant-charge-voltage-max-microvolt", &size);
	if (!list || !size)
		return 0;

	cc_voltage_max_table_len = size / (sizeof(__be32));
	cc_voltage_max_table = devm_kcalloc(&psy->dev, cc_voltage_max_table_len,
					    sizeof(*cc_voltage_max_table), GFP_KERNEL);
	if (!cc_voltage_max_table) {
		dev_err(&psy->dev, "basp_cc_voltage_max_microvolt_table is null\n");
		return -ENOMEM;
	}

	for (index = 0; index < cc_voltage_max_table_len; index++)
		cc_voltage_max_table[index] = be32_to_cpu(*list++);

	/* Check basp_constant_charge_voltage_max_microvolt_table */
	for (index = 0; index < cc_voltage_max_table_len - 1; index++) {
		if (cc_voltage_max_table[index] <= cc_voltage_max_table[index + 1]) {
			dev_err(&psy->dev, "basp_cc_voltage_table is wrong, please check\n");
			return -EINVAL;
		}
	}

	/* check table len */
	if (fcc_table_len != cc_voltage_max_table_len) {
		dev_err(&psy->dev, "basp table len is wrong, please check\n");
		return -EINVAL;
	}

	info->basp_constant_charge_voltage_max_uv_table = cc_voltage_max_table;
	info->basp_constant_charge_voltage_max_uv_table_len = cc_voltage_max_table_len;

	for (index = 0; index < fcc_table_len; index++) {
		if (index == 0)
			propname = kasprintf(GFP_KERNEL, "ocv-capacity-table-%d", index);
		else
			propname = kasprintf(GFP_KERNEL, "basp-ocv-capacity-table-%d", index - 1);

		if (!propname) {
			dev_err(&psy->dev, "propname is null!!!\n");
			return -EINVAL;
		}

		list = of_get_property(battery_np, propname, &size);
		if (!list || !size) {
			dev_err(&psy->dev, "failed to get %s\n", propname);
			kfree(propname);
			return -EINVAL;
		}

		kfree(propname);
		ocv_table_len = size / (2 * sizeof(__be32));
		ocv_table =  devm_kzalloc(&psy->dev,
					  ocv_table_len *
					  sizeof(struct sprd_battery_ocv_table),
					  GFP_KERNEL);
		if (!ocv_table) {
			dev_err(&psy->dev, "ocv_table is null, index = %d\n", index);
			return -ENOMEM;
		}

		for (i = 0; i < ocv_table_len; i++) {
			ocv_table[i].ocv = be32_to_cpu(*list++);
			ocv_table[i].capacity = be32_to_cpu(*list++);
		}

		info->basp_ocv_table[index] = ocv_table;
		info->basp_ocv_table_len[index] = ocv_table_len;
	}

	/* check basp_constant_charge_voltage_max_uv_table_len*/
	for (i = 0; i < fcc_table_len; i++) {
		if (info->basp_ocv_table_len[index] != info->basp_ocv_table_len[index + 1]) {
			dev_err(&psy->dev, "basp_ocv_table_len is wrong, please check\n");
			return -EINVAL;
		}
	}

	/* check basp_constant_charge_voltage_max_uv_table */
	for (i = 0; i < fcc_table_len; i++) {
		ocv_table = info->basp_ocv_table[i];
		if ((ocv_table[0].ocv < SPRD_BATTERY_OCV_TABLE_CHECK_VOLT_UV) ||
			    (ocv_table[info->basp_ocv_table_len[i] - 1].ocv <
			     SPRD_BATTERY_OCV_TABLE_CHECK_VOLT_UV) ||
			    (ocv_table[0].capacity > 100) || (ocv_table[0].capacity < 0)) {
			dev_err(&psy->dev, "basp_ocv_table[%d] unit is wrong, please check\n", i);
			return -EINVAL;
		}

		for (index = 0; index < info->basp_ocv_table_len[i] - 2; index++) {
			if ((ocv_table[index].ocv <= ocv_table[index + 1].ocv) ||
			    (ocv_table[index].capacity <= ocv_table[index + 1].capacity)) {
				dev_err(&psy->dev, "basp_ocv_table[%d] order is wrong, please check\n", i);
				return -EINVAL;
			}
		}
	}

	return 0;
}

static int sprd_battery_init_jeita_table(struct sprd_battery_info *info,
					 struct device_node *battery_np,
					 struct device *dev,
					 int jeita_num)
{
	struct sprd_battery_jeita_table *table;
	const __be32 *list;
	const char *np_name = sprd_battery_jeita_type_names[jeita_num];
	struct sprd_battery_jeita_table **cur_table = &info->jeita_table[jeita_num];
	int i, size;

	list = of_get_property(battery_np, np_name, &size);
	if (!list || !size)
		return 0;

	info->sprd_battery_jeita_size[jeita_num] =
		size / (sizeof(struct sprd_battery_jeita_table) /
			sizeof(int) * sizeof(__be32));

	table = devm_kzalloc(dev, sizeof(struct sprd_battery_jeita_table) *
			     (info->sprd_battery_jeita_size[jeita_num] + 1), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	for (i = 0; i < info->sprd_battery_jeita_size[jeita_num]; i++) {
		table[i].temp = be32_to_cpu(*list++) - 1000;
		table[i].recovery_temp = be32_to_cpu(*list++) - 1000;
		table[i].current_ua = be32_to_cpu(*list++);
		table[i].term_volt = be32_to_cpu(*list++);
	}

	*cur_table = table;

	return 0;
}

static int sprd_battery_parse_jeita_table(struct sprd_battery_info *info,
					  struct device_node *battery_np,
					  struct power_supply *psy)
{
	int ret, i;

	for (i = SPRD_BATTERY_JEITA_DCP; i < SPRD_BATTERY_JEITA_MAX; i++) {
		ret = sprd_battery_init_jeita_table(info, battery_np, &psy->dev, i);
		if (ret)
			return ret;
	}

	return 0;
}

struct sprd_battery_ocv_table *sprd_battery_find_ocv2cap_table(struct sprd_battery_info *info,
							       int temp, int *table_len)
{
	int best_temp_diff = INT_MAX, temp_diff;
	u8 i, best_index = 0;

	if (!info->battery_ocv_table[0])
		return NULL;

	for (i = 0; i < SPRD_BATTERY_OCV_TEMP_MAX; i++) {
		temp_diff = abs(info->battery_ocv_temp_table[i] - temp);

		if (temp_diff < best_temp_diff) {
			best_temp_diff = temp_diff;
			best_index = i;
		}
	}

	*table_len = info->battery_ocv_table_len[best_index];
	return info->battery_ocv_table[best_index];
}
EXPORT_SYMBOL_GPL(sprd_battery_find_ocv2cap_table);

int sprd_battery_get_battery_info(struct power_supply *psy, struct sprd_battery_info *info)
{
	struct device_node *battery_np;
	const char *value;
	int err, index, battery_id;

	info->charge_full_design_uah         = -EINVAL;
	info->voltage_min_design_uv          = -EINVAL;
	info->precharge_current_ua           = -EINVAL;
	info->charge_term_current_ua         = -EINVAL;
	info->constant_charge_current_max_ua = -EINVAL;
	info->constant_charge_voltage_max_uv = -EINVAL;
	info->fullbatt_voltage_offset_uv = 0;
	info->factory_internal_resistance_uohm  = -EINVAL;
	info->battery_internal_resistance_ocv_table = NULL;
	info->battery_internal_resistance_temp_table_len = -EINVAL;
	info->battery_internal_resistance_ocv_table_len = -EINVAL;
	info->basp_charge_full_design_uah_table = NULL;
	info->basp_charge_full_design_uah_table_len = -EINVAL;
	info->basp_constant_charge_voltage_max_uv_table = NULL;
	info->basp_constant_charge_voltage_max_uv_table_len = -EINVAL;
	info->fullbatt_track_end_voltage_uv = -EINVAL;
	info->fullbatt_track_end_current_uA = -EINVAL;
	info->first_capacity_calibration_voltage_uv = -EINVAL;
	info->first_capacity_calibration_capacity = -EINVAL;
	info->fast_charge_ocv_threshold_uv = -EINVAL;
	info->force_jeita_status = -EINVAL;
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

	for (index = 0; index < SPRD_BATTERY_BASP_OCV_TABLE_MAX; index++) {
		info->basp_ocv_table[index] = NULL;
		info->basp_ocv_table_len[index] = -EINVAL;
	}

	if (!psy->of_node) {
		dev_warn(&psy->dev, "%s currently only supports devicetree\n", __func__);
		return -ENXIO;
	}

	battery_id = sprd_battery_parse_battery_id(psy);

	battery_np = of_parse_phandle(psy->of_node, "monitored-battery", battery_id);
	if (!battery_np) {
		dev_warn(&psy->dev, "Fail to get monitored-battery-id-%d\n", battery_id);
		return -ENODEV;
	}

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
	of_property_read_u32(battery_np, "constant-charge-current-max-microamp",
			     &info->constant_charge_current_max_ua);
	of_property_read_u32(battery_np, "constant-charge-voltage-max-microvolt",
			     &info->constant_charge_voltage_max_uv);
	of_property_read_u32(battery_np, "fullbatt-voltage-offset-microvolt",
			     &info->fullbatt_voltage_offset_uv);
	of_property_read_u32(battery_np, "factory-internal-resistance-micro-ohms",
			     &info->factory_internal_resistance_uohm);
	of_property_read_u32(battery_np, "fullbatt-track-end-vol",
			     &info->fullbatt_track_end_voltage_uv);
	of_property_read_u32(battery_np, "fullbatt-track-end-cur",
			     &info->fullbatt_track_end_current_uA);
	of_property_read_u32(battery_np, "first-calib-voltage",
			     &info->first_capacity_calibration_voltage_uv);
	of_property_read_u32(battery_np, "first-calib-capacity",
			     &info->first_capacity_calibration_capacity);
	of_property_read_u32(battery_np, "fullbatt-voltage",
			     &info->fullbatt_voltage_uv);
	of_property_read_u32(battery_np, "fullbatt-current",
			     &info->fullbatt_current_uA);
	of_property_read_u32(battery_np, "first-fullbatt-current",
			     &info->first_fullbatt_current_uA);
	of_property_read_u32(battery_np, "fast-charge-threshold-microvolt",
			     &info->fast_charge_ocv_threshold_uv);
	of_property_read_u32(battery_np, "ir-rc-micro-ohms", &info->ir.rc_uohm);
	of_property_read_u32(battery_np, "ir-us-upper-limit-microvolt",
			     &info->ir.us_upper_limit_uv);
	of_property_read_u32(battery_np, "ir-cv-offset-microvolt",
			     &info->ir.cv_upper_limit_offset_uv);
	of_property_read_u32(battery_np, "force-jeita-status",
			     &info->force_jeita_status);

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

	err = sprd_battery_parse_battery_ocv_capacity_table(info, battery_np, psy);
	if (err) {
		dev_err(&psy->dev, "Fail to get battery ocv capacity table, ret = %d\n", err);
		return err;
	}

	err = sprd_battery_parse_battery_voltage_temp_table(info, battery_np, psy);
	if (err) {
		dev_err(&psy->dev, "Fail to get battery voltage temp table, ret = %d\n", err);
		return err;
	}

	err = sprd_battery_parse_battery_temp_capacity_table(info, battery_np, psy);
	if (err) {
		dev_err(&psy->dev, "Fail to get battery temp capacity table, ret = %d\n", err);
		return err;
	}

	err = sprd_battery_parse_battery_resistance_temp_table(info, battery_np, psy);
	if (err) {
		dev_err(&psy->dev, "Fail to get battery resist temp table, ret = %d\n", err);
		return err;
	}

	err = sprd_battery_parse_battery_internal_resistance_table(info, battery_np, psy);
	if (err) {
		dev_err(&psy->dev, "Fail to get factory internal resist table, ret = %d\n", err);
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
		return err;
	}

	err = sprd_battery_parse_energy_density_ocv_table(info, battery_np, psy);
	if (err) {
		dev_err(&psy->dev, "Fail to parse density ocv table, ret = %d\n", err);
		return err;
	}

	err = sprd_battery_parse_basp_ocv_table(info, battery_np, psy);
	if (err) {
		dev_err(&psy->dev, "Fail to parse basp ocv table, ret = %d\n", err);
		return err;
	}

	err = sprd_battery_parse_jeita_table(info, battery_np, psy);
	if (err) {
		dev_err(&psy->dev, "Fail to parse jeita table, err = %d\n", err);
		return err;
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

	for (i = 0; i < SPRD_BATTERY_BASP_OCV_TABLE_MAX; i++) {
		if (info->basp_ocv_table[i])
			devm_kfree(&psy->dev, info->basp_ocv_table[i]);
	}

	if (info->battery_internal_resistance_ocv_table)
		devm_kfree(&psy->dev, info->battery_internal_resistance_ocv_table);
	if (info->basp_charge_full_design_uah_table)
		devm_kfree(&psy->dev, info->basp_charge_full_design_uah_table);
	if (info->basp_constant_charge_voltage_max_uv_table)
		devm_kfree(&psy->dev, info->basp_constant_charge_voltage_max_uv_table);
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

MODULE_LICENSE("GPL v2");
