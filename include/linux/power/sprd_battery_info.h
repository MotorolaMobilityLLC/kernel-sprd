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

#define SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX	20
#define SPRD_BATTERY_BASP_OCV_TABLE_MAX		20
#define SPRD_BATTERY_OCV_TEMP_MAX		20

enum sprd_battery_jeita_types {
	SPRD_BATTERY_JEITA_DCP = 0,
	SPRD_BATTERY_JEITA_SDP,
	SPRD_BATTERY_JEITA_CDP,
	SPRD_BATTERY_JEITA_UNKNOWN,
	SPRD_BATTERY_JEITA_FCHG,
	SPRD_BATTERY_JEITA_FLASH,
	SPRD_BATTERY_JEITA_WL_BPP,
	SPRD_BATTERY_JEITA_WL_EPP,
	SPRD_BATTERY_JEITA_MAX,
};

static const char * const sprd_battery_jeita_type_names[] = {
	[SPRD_BATTERY_JEITA_UNKNOWN] = "unknown-jeita-temp-table",
	[SPRD_BATTERY_JEITA_SDP] = "sdp-jeita-temp-table",
	[SPRD_BATTERY_JEITA_CDP] = "cdp-jeita-temp-table",
	[SPRD_BATTERY_JEITA_DCP] = "dcp-jeita-temp-table",
	[SPRD_BATTERY_JEITA_FCHG] = "fchg-jeita-temp-table",
	[SPRD_BATTERY_JEITA_FLASH] = "flash-jeita-temp-table",
	[SPRD_BATTERY_JEITA_WL_BPP] = "wl-bpp-jeita-temp-table",
	[SPRD_BATTERY_JEITA_WL_EPP] = "wl-epp-jeita-temp-table",
};

struct sprd_battery_jeita_table {
	int temp;
	int recovery_temp;
	int current_ua;
	int term_volt;
};

struct sprd_battery_ir_compensation {
	/* microOhms */
	int rc_uohm;
	int us_upper_limit_uv;
	int cv_upper_limit_offset_uv;
};

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

typedef struct sprd_battery_energy_density_ocv_table {
	int engy_dens_ocv_hi;
	int engy_dens_ocv_lo;
} density_ocv_table;

struct sprd_battery_ocv_table {
	int ocv;	/* microVolts */
	int capacity;	/* percent */
};

struct sprd_battery_resistance_temp_table {
	int temp;	/* celsius */
	int resistance;	/* internal resistance percent */
};

struct sprd_battery_vol_temp_table {
	int vol;	/* microVolts */
	int temp;	/* celsius */
};

struct sprd_battery_temp_cap_table {
	int temp;	/* celsius */
	int cap;	/* capacity percentage */
};

struct sprd_battery_info {
	/* microAmp-hours */
	int charge_full_design_uah;
	/* microVolts */
	int voltage_min_design_uv;
	/* microVolts */
	int batt_ovp_threshold_uv;
	/* microAmps */
	int precharge_current_ua;
	/* microAmps */
	int charge_term_current_ua;
	/* microAmps */
	int constant_charge_current_max_ua;
	/* microVolts */
	int constant_charge_voltage_max_uv;
	/* microVolts */
	int fullbatt_voltage_offset_uv;
	/* microOhms */
	int factory_internal_resistance_uohm;
	/* microVolts */
	int fast_charge_ocv_threshold_uv;
	/* microOhms */
	int connector_resistance_uohm;

	/* microVolts */
	unsigned int fullbatt_voltage_uv;
	/* microAmps */
	unsigned int fullbatt_current_uA;
	/* microAmps */
	unsigned int first_fullbatt_current_uA;
	/* microVolts */
	int fullbatt_track_end_voltage_uv;
	/* microAmps */
	int fullbatt_track_end_current_uA;
	/* microVolts */
	int first_capacity_calibration_voltage_uv;
	/* percentage */
	int first_capacity_calibration_capacity;
	/* celsius */
	int battery_internal_resistance_temp_table[SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX];
	int battery_internal_resistance_temp_table_len;
	int battery_ocv_temp_table[SPRD_BATTERY_OCV_TEMP_MAX];
	/* Milliohm */
	int *battery_internal_resistance_table[SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX];
	int battery_internal_resistance_table_len[SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX];
	/* microVolts */
	int *battery_internal_resistance_ocv_table;
	int battery_internal_resistance_ocv_table_len;

	struct sprd_battery_charge_current cur;
	density_ocv_table *cap_calib_dens_ocv_table;
	int cap_calib_dens_ocv_table_len;

	density_ocv_table *cap_track_dens_ocv_table;
	int cap_track_dens_ocv_table_len;

	struct sprd_battery_ocv_table *battery_ocv_table[SPRD_BATTERY_OCV_TEMP_MAX];
	int battery_ocv_table_len[SPRD_BATTERY_OCV_TEMP_MAX];

	struct sprd_battery_vol_temp_table *battery_vol_temp_table;
	int battery_vol_temp_table_len;

	struct sprd_battery_temp_cap_table *battery_temp_cap_table;
	int battery_temp_cap_table_len;

	struct sprd_battery_resistance_temp_table *battery_temp_resist_table;
	int battery_temp_resist_table_len;

	struct sprd_battery_ocv_table *basp_ocv_table[SPRD_BATTERY_BASP_OCV_TABLE_MAX];
	int basp_ocv_table_len[SPRD_BATTERY_BASP_OCV_TABLE_MAX];
	/* microAmp-hours */
	int *basp_charge_full_design_uah_table;
	int basp_charge_full_design_uah_table_len;
	int *basp_constant_charge_voltage_max_uv_table;
	int basp_constant_charge_voltage_max_uv_table_len;

	struct sprd_battery_jeita_table *jeita_table[SPRD_BATTERY_JEITA_MAX];
	u32 max_current_jeita_index[SPRD_BATTERY_JEITA_MAX];
	u32 sprd_battery_jeita_size[SPRD_BATTERY_JEITA_MAX];

	struct sprd_battery_ir_compensation ir;
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
extern int sprd_battery_parse_battery_id(struct power_supply *psy);
extern struct sprd_battery_ocv_table *
sprd_battery_find_ocv2cap_table(struct sprd_battery_info *info,
				int temp, int *table_len);

#endif /* _SPRD_BATTERY_INFO_H */

