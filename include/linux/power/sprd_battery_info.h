/*
 * Copyright (C) 2011 Unisoc Co., Ltd.
 * Jinfeng.lin <Jinfeng.Lin1@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _SPRD_BATTERY_INFO_H
#define _SPRD_BATTERY_INFO_H

#include <linux/device.h>

#define SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX 20

/* microAmps */
struct sprd_battery_charge_current {
	int sdp_limit;
	int sdp_cur;
	int dcp_limit;
	int dcp_cur;
	int cdp_limit;
	int cdp_cur;
	int unknown_limit;
	int unknown_cur;
	int fchg_limit;
	int fchg_cur;
	int flash_limit;
	int flash_cur;
	int wl_bpp_cur;
	int wl_bpp_limit;
	int wl_epp_cur;
	int wl_epp_limit;
};

struct sprd_battery_info {
	/* microAmp-hours */
	int charge_full_design_uah;
	/* microVolts */
	int voltage_min_design_uv;
	/* microAmps */
	int precharge_current_ua;
	/* microAmps */
	int charge_term_current_ua;
	/* microAmps */
	int constant_charge_current_max_ua;
	/* microVolts */
	int constant_charge_voltage_max_uv;
	/* microOhms */
	int factory_internal_resistance_uohm;
	/* microVolts */
	int fast_charge_ocv_threshold_uv;
	/* microOhms */
	int connector_resistance_uohm;
	/* celsius */
	int battery_internal_resistance_temp_table[SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX];
	int battery_internal_resistance_temp_table_len;
	/* Milliohm */
	int *battery_internal_resistance_table[SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX];
	int battery_internal_resistance_table_len[SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX];
	/* microVolts */
	int *battery_internal_resistance_ocv_table;
	int battery_internal_resistance_ocv_table_len;

	struct sprd_battery_charge_current cur;
};

extern void sprd_battery_put_battery_info(struct power_supply *psy,
					  struct sprd_battery_info *info);
extern int sprd_battery_get_battery_info(struct power_supply *psy,
					 struct sprd_battery_info *info);
extern void sprd_battery_find_resistance_table(struct power_supply *psy,
					       int **resistance_table,
					       int table_len, int *temp_table,
					       int temp_len, int temp,
					       int *target_table);

#endif /* _SPRD_BATTERY_INFO_H */

