// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 * MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This driver enables to monitor battery health and control charger
 * during suspend-to-mem.
 * Charger manager depends on other devices. Register this later than
 * the depending devices.
 *
**/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/io.h>
#include <linux/firmware.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/reboot.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/workqueue.h>

/*
 * Default temperature threshold for charging.
 * Every temperature units are in tenth of centigrade.
 */
#define CM_DEFAULT_RECHARGE_TEMP_DIFF		50
#define CM_DEFAULT_CHARGE_TEMP_MAX		500
#define CM_CAP_CYCLE_TRACK_TIME			15
#define CM_UVLO_OFFSET				50000
#define CM_FORCE_SET_FUEL_CAP_FULL		1000
#define CM_LOW_TEMP_REGION			100
#define CM_UVLO_CALIBRATION_VOLTAGE_THRESHOLD	3250000
#define CM_UVLO_CALIBRATION_CNT_THRESHOLD	5
#define CM_LOW_TEMP_SHUTDOWN_VALTAGE		3200000
#define CM_LOW_CAP_SHUTDOWN_VOLTAGE_THRESHOLD	3300000

#define CM_CAP_ONE_PERCENT			10
#define CM_HCAP_DECREASE_STEP			8
#define CM_HCAP_THRESHOLD			995
#define CM_CAP_FULL_PERCENT			1000
#define CM_MAGIC_NUM				0x5A5AA5A5
#define CM_CAPACITY_LEVEL_CRITICAL		0
#define CM_CAPACITY_LEVEL_LOW			15
#define CM_CAPACITY_LEVEL_NORMAL		85
#define CM_CAPACITY_LEVEL_FULL			100
#define CM_CAPACITY_LEVEL_CRITICAL_VOLTAGE	3200000
#define CM_FAST_CHARGE_ENABLE_BATTERY_VOLTAGE	3400000
#define CM_FAST_CHARGE_ENABLE_CURRENT		1200000
#define CM_FAST_CHARGE_ENABLE_THERMAL_CURRENT	1000000
#define CM_FAST_CHARGE_DISABLE_BATTERY_VOLTAGE	3400000
#define CM_FAST_CHARGE_DISABLE_CURRENT		1000000
#define CM_FAST_CHARGE_TRANSITION_CURRENT_1P5A	1500000
#define CM_FAST_CHARGE_CURRENT_2A		2000000
#define CM_FAST_CHARGE_VOLTAGE_20V		20000000
#define CM_FAST_CHARGE_VOLTAGE_15V		15000000
#define CM_FAST_CHARGE_VOLTAGE_12V		12000000
#define CM_FAST_CHARGE_VOLTAGE_9V		9000000
#define CM_FAST_CHARGE_VOLTAGE_5V		5000000
#define CM_FAST_CHARGE_VOLTAGE_5V_THRESHOLD	6500000
#define CM_FAST_CHARGE_START_VOLTAGE_LTHRESHOLD	3520000
#define CM_FAST_CHARGE_START_VOLTAGE_HTHRESHOLD	4200000
#define CM_FAST_CHARGE_ENABLE_COUNT		3
#define CM_FAST_CHARGE_DISABLE_COUNT		2

#define CM_CP_VSTEP				20000
#define CM_CP_ISTEP				50000
#define CM_CP_PRIMARY_CHARGER_DIS_TIMEOUT	20
#define CM_CP_IBAT_UCP_THRESHOLD		8
#define CM_CP_ADJUST_VOLTAGE_THRESHOLD		(5 * 1000 / CM_CP_WORK_TIME_MS)
#define CM_CP_ACC_VBAT_HTHRESHOLD		3850000
#define CM_CP_VBAT_STEP1			300000
#define CM_CP_VBAT_STEP2			150000
#define CM_CP_VBAT_STEP3			50000
#define CM_CP_IBAT_STEP1			2000000
#define CM_CP_IBAT_STEP2			1000000
#define CM_CP_IBAT_STEP3			100000
#define CM_CP_VBUS_STEP1			2000000
#define CM_CP_VBUS_STEP2			1000000
#define CM_CP_VBUS_STEP3			50000
#define CM_CP_IBUS_STEP1			1000000
#define CM_CP_IBUS_STEP2			500000
#define CM_CP_IBUS_STEP3			100000
#define CM_CP_DEFAULT_TAPER_CURRENT		1000000

#define CM_PPS_5V_PROG_MAX			6200000
#define CM_PPS_VOLTAGE_11V			11000000
#define CM_PPS_VOLTAGE_16V			16000000
#define CM_PPS_VOLTAGE_21V			21000000

#define CM_CP_VBUS_ERRORLO_THRESHOLD(x)		((int)(x * 205 / 100))
#define CM_CP_VBUS_ERRORHI_THRESHOLD(x)		((int)(x * 240 / 100))

#define CM_IR_COMPENSATION_TIME			3

#define CM_CP_WORK_TIME_MS			500
#define CM_CHK_DIS_FCHG_WORK_MS			5000
#define CM_TRY_DIS_FCHG_WORK_MS			100
#define MAX_STATE                               1

static const char * const cm_cp_state_names[] = {
	[CM_CP_STATE_UNKNOWN] = "Charge pump state: UNKNOWN",
	[CM_CP_STATE_RECOVERY] = "Charge pump state: RECOVERY",
	[CM_CP_STATE_ENTRY] = "Charge pump state: ENTRY",
	[CM_CP_STATE_CHECK_VBUS] = "Charge pump state: CHECK VBUS",
	[CM_CP_STATE_TUNE] = "Charge pump state: TUNE",
	[CM_CP_STATE_EXIT] = "Charge pump state: EXIT",
};

static char *charger_manager_supplied_to[] = {
	"audio-ldo",
};

/*
 * Regard CM_JIFFIES_SMALL jiffies is small enough to ignore for
 * delayed works so that we can run delayed works with CM_JIFFIES_SMALL
 * without any delays.
 */
#define	CM_JIFFIES_SMALL	(2)

/* If y is valid (> 0) and smaller than x, do x = y */
#define CM_MIN_VALID(x, y)	x = (((y > 0) && ((x) > (y))) ? (y) : (x))

/*
 * Regard CM_RTC_SMALL (sec) is small enough to ignore error in invoking
 * rtc alarm. It should be 2 or larger
 */
#define CM_RTC_SMALL		(2)

#define CM_EVENT_TYPE_NUM	6

static struct charger_type charger_usb_type[20] = {
	{POWER_SUPPLY_USB_TYPE_SDP, CM_CHARGER_TYPE_SDP},
	{POWER_SUPPLY_USB_TYPE_DCP, CM_CHARGER_TYPE_DCP},
	{POWER_SUPPLY_USB_TYPE_CDP, CM_CHARGER_TYPE_CDP},
	{POWER_SUPPLY_USB_TYPE_UNKNOWN, CM_CHARGER_TYPE_UNKNOWN},
};

static struct charger_type charger_fchg_type[20] = {
	{POWER_SUPPLY_CHARGE_TYPE_FAST, CM_CHARGER_TYPE_FAST},
	{POWER_SUPPLY_CHARGE_TYPE_ADAPTIVE, CM_CHARGER_TYPE_ADAPTIVE},
	{POWER_SUPPLY_CHARGE_TYPE_UNKNOWN, CM_CHARGER_TYPE_UNKNOWN},
};

static struct charger_type charger_wireless_type[20] = {
	{POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP, CM_WIRELESS_CHARGER_TYPE_BPP},
	{POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP, CM_WIRELESS_CHARGER_TYPE_EPP},
	{POWER_SUPPLY_WIRELESS_CHARGER_TYPE_UNKNOWN, CM_CHARGER_TYPE_UNKNOWN},
};

static LIST_HEAD(cm_list);
static DEFINE_MUTEX(cm_list_mtx);

/* About in-suspend (suspend-again) monitoring */
static struct alarm *cm_timer;

static bool cm_suspended;
static bool cm_timer_set;
static unsigned long cm_suspend_duration_ms;
static int cm_event_num;
static enum cm_event_types cm_event_type[CM_EVENT_TYPE_NUM];
static char *cm_event_msg[CM_EVENT_TYPE_NUM];

/* About normal (not suspended) monitoring */
static unsigned long polling_jiffy = ULONG_MAX; /* ULONG_MAX: no polling */
static unsigned long next_polling; /* Next appointed polling time */
static struct workqueue_struct *cm_wq; /* init at driver add */
static struct delayed_work cm_monitor_work; /* init at driver add */

static bool allow_charger_enable;
static bool is_charger_mode;
static void cm_notify_type_handle(struct charger_manager *cm, enum cm_event_types type, char *msg);
static bool cm_manager_adjust_current(struct charger_manager *cm, int jeita_status);
static void cm_update_charger_type_status(struct charger_manager *cm);
static int cm_manager_get_jeita_status(struct charger_manager *cm, int cur_temp);
static bool cm_charger_is_support_fchg(struct charger_manager *cm);

static void cm_cap_remap_init_boundary(struct charger_desc *desc, int index, struct device *dev)
{

	if (index == 0) {
		desc->cap_remap_table[index].lb = (desc->cap_remap_table[index].lcap) * 1000;
		desc->cap_remap_total_cnt = desc->cap_remap_table[index].lcap;
	} else {
		desc->cap_remap_table[index].lb = desc->cap_remap_table[index - 1].hb +
			(desc->cap_remap_table[index].lcap -
			 desc->cap_remap_table[index - 1].hcap) * 1000;
		desc->cap_remap_total_cnt += (desc->cap_remap_table[index].lcap -
					      desc->cap_remap_table[index - 1].hcap);
	}

	desc->cap_remap_table[index].hb = desc->cap_remap_table[index].lb +
		(desc->cap_remap_table[index].hcap - desc->cap_remap_table[index].lcap) *
		desc->cap_remap_table[index].cnt * 1000;

	desc->cap_remap_total_cnt +=
		(desc->cap_remap_table[index].hcap - desc->cap_remap_table[index].lcap) *
		desc->cap_remap_table[index].cnt;

	dev_dbg(dev, "%s, cap_remap_table[%d].lb =%d,cap_remap_table[%d].hb = %d\n",
		 __func__, index, desc->cap_remap_table[index].lb, index,
		 desc->cap_remap_table[index].hb);
}

/*
 * cm_capacity_remap - remap fuel_cap
 * @ fuel_cap: cap from fuel gauge
 * Return the remapped cap
 */
static int cm_capacity_remap(struct charger_manager *cm, int fuel_cap)
{
	int i, temp, cap = 0;

	if (cm->desc->cap_remap_full_percent) {
		fuel_cap = fuel_cap * 100 / cm->desc->cap_remap_full_percent;
		if (fuel_cap > CM_CAP_FULL_PERCENT)
			fuel_cap  = CM_CAP_FULL_PERCENT;
	}

	if (!cm->desc->cap_remap_table)
		return fuel_cap;

	if (fuel_cap < 0) {
		fuel_cap = 0;
		return 0;
	} else if (fuel_cap >  CM_CAP_FULL_PERCENT) {
		fuel_cap  = CM_CAP_FULL_PERCENT;
		return fuel_cap;
	}

	temp = fuel_cap * cm->desc->cap_remap_total_cnt;

	for (i = 0; i < cm->desc->cap_remap_table_len; i++) {
		if (temp <= cm->desc->cap_remap_table[i].lb) {
			if (i == 0)
				cap = DIV_ROUND_CLOSEST(temp, 100);
			else
				cap = DIV_ROUND_CLOSEST((temp -
					cm->desc->cap_remap_table[i - 1].hb), 100) +
					cm->desc->cap_remap_table[i - 1].hcap * 10;
			break;
		} else if (temp <= cm->desc->cap_remap_table[i].hb) {
			cap = DIV_ROUND_CLOSEST((temp - cm->desc->cap_remap_table[i].lb),
						cm->desc->cap_remap_table[i].cnt * 100)
				+ cm->desc->cap_remap_table[i].lcap * 10;
			break;
		}

		if (i == cm->desc->cap_remap_table_len - 1 && temp > cm->desc->cap_remap_table[i].hb)
			cap = DIV_ROUND_CLOSEST((temp - cm->desc->cap_remap_table[i].hb), 100)
				+ cm->desc->cap_remap_table[i].hcap;

	}

	return cap;
}

static int cm_init_cap_remap_table(struct charger_desc *desc, struct device *dev)
{

	struct device_node *np = dev->of_node;
	const __be32 *list;
	int i, size;

	list = of_get_property(np, "cm-cap-remap-table", &size);
	if (!list || !size) {
		dev_err(dev, "%s  get cm-cap-remap-table fail\n", __func__);
		return 0;
	}
	desc->cap_remap_table_len = (u32)size / (3 * sizeof(__be32));
	desc->cap_remap_table = devm_kzalloc(dev, sizeof(struct cap_remap_table) *
				(desc->cap_remap_table_len + 1), GFP_KERNEL);
	if (!desc->cap_remap_table) {
		dev_err(dev, "%s, get cap_remap_table fail\n", __func__);
		return -ENOMEM;
	}
	for (i = 0; i < desc->cap_remap_table_len; i++) {
		desc->cap_remap_table[i].lcap = be32_to_cpu(*list++);
		desc->cap_remap_table[i].hcap = be32_to_cpu(*list++);
		desc->cap_remap_table[i].cnt = be32_to_cpu(*list++);

		cm_cap_remap_init_boundary(desc, i, dev);

		dev_dbg(dev, "cap_remap_table[%d].lcap= %d,cap_remap_table[%d].hcap = %d,"
			 "cap_remap_table[%d].cnt= %d\n", i, desc->cap_remap_table[i].lcap,
			 i, desc->cap_remap_table[i].hcap, i, desc->cap_remap_table[i].cnt);
	}

	if (desc->cap_remap_table[desc->cap_remap_table_len - 1].hcap != 100)
		desc->cap_remap_total_cnt +=
			(100 - desc->cap_remap_table[desc->cap_remap_table_len - 1].hcap);

	dev_dbg(dev, "cap_remap_total_cnt =%d, cap_remap_table_len = %d\n",
		 desc->cap_remap_total_cnt, desc->cap_remap_table_len);

	return 0;
}

/**
 * is_batt_present - See if the battery presents in place.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_batt_present(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	bool present = false;
	int i, ret;

	switch (cm->desc->battery_present) {
	case CM_BATTERY_PRESENT:
		present = true;
		break;
	case CM_NO_BATTERY:
		break;
	case CM_FUEL_GAUGE:
		psy = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
		if (!psy)
			break;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &val);
		if (ret == 0 && val.intval)
			present = true;
		power_supply_put(psy);
		break;
	case CM_CHARGER_STAT:
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(
					cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}

			ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &val);
			power_supply_put(psy);
			if (ret == 0 && val.intval) {
				present = true;
				break;
			}
		}
		break;
	}

	return present;
}

static bool is_ext_wl_pwr_online(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	bool online = false;
	int i, ret;

	if (!cm->desc->psy_wl_charger_stat)
		return online;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_wl_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_wl_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_wl_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
		power_supply_put(psy);
		if (ret == 0 && val.intval) {
			online = true;
			break;
		}
	}

	return online;
}

/**
 * is_ext_usb_pwr_online - See if an external power source is attached to charge
 * @cm: the Charger Manager representing the battery.
 *
 * Returns true if at least one of the chargers of the battery has an external
 * power source attached to charge the battery regardless of whether it is
 * actually charging or not.
 */
static bool is_ext_usb_pwr_online(struct charger_manager *cm)
{
	bool online = false;

	if (cm->vchg_info->ops && cm->vchg_info->ops->is_charger_online)
		return cm->vchg_info->ops->is_charger_online(cm->vchg_info);

	return online;
}

static bool is_ext_pwr_online(struct charger_manager *cm)
{
	bool online = false;

	if (is_ext_usb_pwr_online(cm) || is_ext_wl_pwr_online(cm))
		online = true;

	return online;
}

/**
 * get_cp_ibat_uA - Get the charge current of the battery from charge pump
 * @cm: the Charger Manager representing the battery.
 * @uA: the current returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_cp_ibat_uA(struct charger_manager *cm, int *uA)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	if (!cm || !cm->desc || !cm->desc->psy_cp_stat)
		return ret;

	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		val.intval = CM_IBAT_CURRENT_NOW_CMD;
		ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
		power_supply_put(cp_psy);
		if (ret == 0)
			*uA += val.intval;
	}

	return ret;
}

/**
 * get_cp_vbat_uV - Get the voltage level of the battery from charge pump
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_cp_vbat_uV(struct charger_manager *cm, int *uV)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	if (!cm || !cm->desc || !cm->desc->psy_cp_stat)
		return ret;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
		power_supply_put(cp_psy);
		if (ret == 0) {
			*uV = val.intval;
			break;
		}
	}

	return ret;
}

/**
 * get_cp_vbus_uV - Get the voltage level of the bus from charge pump
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_cp_vbus_uV(struct charger_manager *cm, int *uV)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	if (!cm || !cm->desc || !cm->desc->psy_cp_stat)
		return ret;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		ret = power_supply_get_property(cp_psy,
						POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &val);
		power_supply_put(cp_psy);
		if (ret == 0) {
			*uV = val.intval;
			break;
		}
	}

	return ret;
}

 /**
  * get_cp_ibus_uA - Get the current level of the charge pump
  * @cm: the Charger Manager representing the battery.
  * @uA: the current level returned.
  *
  * Returns 0 if there is no error.
  * Returns a negative value on error.
  */
static int get_cp_ibus_uA(struct charger_manager *cm, int *cur)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	if (!cm->desc->psy_cp_stat)
		return 0;

	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		val.intval = CM_IBUS_CURRENT_NOW_CMD;
		ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
		power_supply_put(cp_psy);
		if (ret == 0)
			*cur += val.intval;
	}

	return ret;
}

static int get_cp_ibat_uA_by_id(struct charger_manager *cm, int *cur, int id)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int ret = -ENODEV;

	*cur = 0;

	if (!cm->desc->psy_cp_stat || !cm->desc->psy_cp_stat[id])
		return 0;

	cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[id]);
	if (!cp_psy) {
		dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
			cm->desc->psy_cp_stat[id]);
		return ret;
	}

	ret = power_supply_get_property(cp_psy,
					POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	power_supply_put(cp_psy);
	if (ret == 0)
		*cur = val.intval;

	return ret;
}

 /**
  * get_ibat_avg_uA - Get the current level of the battery
  * @cm: the Charger Manager representing the battery.
  * @uA: the current level returned.
  *
  * Returns 0 if there is no error.
  * Returns a negative value on error.
  */
static int get_ibat_avg_uA(struct charger_manager *cm, int *uA)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CURRENT_AVG, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*uA = val.intval;
	return 0;
}

 /**
  * get_ibat_now_uA - Get the current level of the battery
  * @cm: the Charger Manager representing the battery.
  * @uA: the current level returned.
  *
  * Returns 0 if there is no error.
  * Returns a negative value on error.
  */
static int get_ibat_now_uA(struct charger_manager *cm, int *uA)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	val.intval = CM_IBAT_CURRENT_NOW_CMD;
	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*uA = val.intval;
	return 0;
}

/**
 *
 * get_vbat_avg_uV - Get the voltage level of the battery
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_vbat_avg_uV(struct charger_manager *cm, int *uV)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_VOLTAGE_AVG, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*uV = val.intval;
	return 0;
}

/*
 * get_batt_ocv - Get the battery ocv
 * level of the battery.
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_batt_ocv(struct charger_manager *cm, int *ocv)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_VOLTAGE_OCV, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*ocv = val.intval;
	return 0;
}

/*
 * get_batt_now - Get the battery voltage now
 * level of the battery.
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_vbat_now_uV(struct charger_manager *cm, int *ocv)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*ocv = val.intval;
	return 0;
}

/**
 * get_batt_cap - Get the cap level of the battery
 * @cm: the Charger Manager representing the battery.
 * @uV: the cap level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_batt_cap(struct charger_manager *cm, int *cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	val.intval = CM_CAPACITY;
	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*cap = val.intval;
	return 0;
}

/**
 * get_batt_total_cap - Get the total capacity level of the battery
 * @cm: the Charger Manager representing the battery.
 * @uV: the total_cap level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_batt_total_cap(struct charger_manager *cm, u32 *total_cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
					&val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*total_cap = val.intval;

	return 0;
}

/*
 * get_boot_cap - Get the battery boot capacity
 * of the battery.
 * @cm: the Charger Manager representing the battery.
 * @cap: the battery capacity returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_boot_cap(struct charger_manager *cm, int *cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	val.intval = CM_BOOT_CAPACITY;
	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*cap = val.intval;
	return 0;
}

static int cm_get_charge_cycle(struct charger_manager *cm, int *cycle)
{
	struct power_supply *fuel_gauge = NULL;
	union power_supply_propval val;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		ret = -ENODEV;
		return ret;
	}

	*cycle = 0;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CYCLE_COUNT, &val);
	if (ret)
		return ret;

	power_supply_put(fuel_gauge);
	*cycle = val.intval;

	return 0;
}

static int cm_get_bc1p2_type(struct charger_manager *cm, u32 *type)
{
	int ret = -EINVAL;

	*type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	if (cm->vchg_info->ops && cm->vchg_info->ops->get_bc1p2_type) {
		*type = cm->vchg_info->ops->get_bc1p2_type(cm->vchg_info);
		ret = 0;
	}

	return ret;
}

static void cm_get_charger_type(struct charger_manager *cm,
				enum cm_charger_type_flag chg_type_flag,
				u32 *type)
{
	struct charger_type *chg_type;

	switch (chg_type_flag) {
	case CM_FCHG_TYPE:
		chg_type = charger_fchg_type;
		break;
	case CM_WL_TYPE:
		chg_type = charger_wireless_type;
		break;
	case CM_USB_TYPE:
	default:
		chg_type = charger_usb_type;
		break;
	}

	if (!chg_type) {
		dev_err(cm->dev, "%s, chg_type is NULL\n", __func__);
		*type = CM_CHARGER_TYPE_UNKNOWN;
		return;
	}

	while ((chg_type)->adap_type != CM_CHARGER_TYPE_UNKNOWN) {
		if (*type == chg_type->psy_type) {
			*type = chg_type->adap_type;
			return;
		}

		chg_type++;
	}
}

/**
 * get_usb_charger_type - Get the charger type
 * @cm: the Charger Manager representing the battery.
 * @type: the charger type returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_usb_charger_type(struct charger_manager *cm, u32 *type)
{
	int ret = -EINVAL;

	mutex_lock(&cm->desc->charger_type_mtx);
	if (cm->desc->is_fast_charge) {
		mutex_unlock(&cm->desc->charger_type_mtx);
		return 0;
	}

	ret = cm_get_bc1p2_type(cm, type);
	cm_get_charger_type(cm, CM_USB_TYPE, type);

	mutex_unlock(&cm->desc->charger_type_mtx);
	return ret;
}

/**
 * get_wireless_charger_type - Get the wireless_charger type
 * @cm: the Charger Manager representing the battery.
 * @type: the wireless charger type returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_wireless_charger_type(struct charger_manager *cm, u32 *type)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret = -EINVAL, i;

	if (!cm->desc->psy_wl_charger_stat)
		return 0;

	mutex_lock(&cm->desc->charger_type_mtx);
	for (i = 0; cm->desc->psy_wl_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_wl_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_wl_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TYPE, &val);
		power_supply_put(psy);
		if (ret == 0) {
			*type = val.intval;
			break;
		}
	}

	cm_get_charger_type(cm, CM_WL_TYPE, type);
	mutex_unlock(&cm->desc->charger_type_mtx);

	return ret;
}

/**
 * set_batt_cap - Set the cap level of the battery
 * @cm: the Charger Manager representing the battery.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int set_batt_cap(struct charger_manager *cm, int cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(cm->dev, "can not find fuel gauge device\n");
		return -ENODEV;
	}

	val.intval = cap;
	ret = power_supply_set_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		dev_err(cm->dev, "failed to save current battery capacity\n");

	return ret;
}
/**
 * get_charger_voltage - Get the charging voltage from fgu
 * @cm: the Charger Manager representing the battery.
 * @cur: the charging input voltage returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_charger_voltage(struct charger_manager *cm, int *vol)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret = -ENODEV;

	if (!is_ext_pwr_online(cm))
		return 0;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(cm->dev, "Cannot find power supply  %s\n",
			cm->desc->psy_fuel_gauge);
		return	ret;
	}

	ret = power_supply_get_property(fuel_gauge,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &val);
	power_supply_put(fuel_gauge);
	if (ret == 0)
		*vol = val.intval;

	return ret;
}

/**
 * adjust_fuel_cap - Adjust the fuel cap level
 * @cm: the Charger Manager representing the battery.
 * @cap: the adjust fuel cap level.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int adjust_fuel_cap(struct charger_manager *cm, int cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	val.intval = cap;
	ret = power_supply_set_property(fuel_gauge,
					POWER_SUPPLY_PROP_CALIBRATE, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		dev_err(cm->dev, "failed to adjust fuel cap\n");

	return ret;
}

/**
 * get_constant_charge_current - Get the charging current from charging ic
 * @cm: the Charger Manager representing the battery.
 * @cur: the charging current returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_constant_charge_current(struct charger_manager *cm, int *cur)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret = -ENODEV;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy,
						POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT, &val);
		power_supply_put(psy);
		if (ret == 0) {
			*cur += val.intval;
		}
	}

	return ret;
}

/**
 * get_input_current_limit - Get the input current limit from charging ic
 * @cm: the Charger Manager representing the battery.
 * @cur: the charging input limit current returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_input_current_limit(struct charger_manager *cm, int *cur)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret = -ENODEV;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy,
						POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
						&val);
		power_supply_put(psy);
		if (ret == 0)
			*cur += val.intval;
	}

	return ret;
}

static int get_charger_input_current(struct charger_manager *cm, int *cur)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret = -ENODEV;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = CM_IBUS_CURRENT_NOW_CMD;
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
		power_supply_put(psy);
		if (ret == 0)
			*cur += val.intval;
	}

	return ret;
}

static void cm_set_charger_present(struct charger_manager *cm, bool present)
{
	int ret, i;
	union power_supply_propval val = {0,};
	struct power_supply *psy;

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find primary power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = present;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_PRESENT, &val);
		power_supply_put(psy);
		if (ret) {
			dev_err(cm->dev, "Fail to set present[%d] of %s, ret = %d\n",
				present, cm->desc->psy_charger_stat[i], ret);
			continue;
		}
	}
}

static bool cm_reset_basp_parameters(struct charger_manager *cm, int volt_uv)
{
	struct sprd_battery_jeita_table *table;
	int i, j, size;
	bool is_need_update = false;

	if (cm->desc->constant_charge_voltage_max_uv == volt_uv) {
		dev_warn(cm->dev, "BASP does not reset: volt_uv == constant charge voltage\n");
		return is_need_update;
	}

	cm->desc->ir_comp.us = volt_uv;
	cm->desc->cp.cp_target_vbat = volt_uv;
	cm->desc->constant_charge_voltage_max_uv = volt_uv;
	cm->desc->fullbatt_uV = volt_uv - cm->desc->fullbatt_voltage_offset_uv;

	for (i = SPRD_BATTERY_JEITA_DCP; i < SPRD_BATTERY_JEITA_MAX; i++) {
		table = cm->desc->jeita_tab_array[i];
		size = cm->desc->jeita_size[i];

		if (!table || !size)
			continue;

		for (j = 0; j < size; j++) {
			if (table[j].term_volt > volt_uv) {
				is_need_update = true;
				dev_info(cm->dev, "%s, set table[%d] from %d to %d\n",
					 sprd_battery_jeita_type_names[i], j,
					 table[j].term_volt, volt_uv);
				table[j].term_volt = volt_uv;
			}
		}
	}

	return is_need_update;
}

static int cm_set_basp_max_volt(struct charger_manager *cm, int max_volt_uv)
{
	struct power_supply *fuel_gauge = NULL;
	union power_supply_propval val;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		ret = -ENODEV;
		return ret;
	}

	val.intval = max_volt_uv;
	ret = power_supply_set_property(fuel_gauge, POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN, &val);
	power_supply_put(fuel_gauge);

	if (ret)
		dev_err(cm->dev, "failed to set basp max voltage, ret = %d\n", ret);

	return ret;
}

static int cm_get_basp_max_volt(struct charger_manager *cm, int *max_volt_uv)
{
	struct power_supply *fuel_gauge = NULL;
	union power_supply_propval val;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(cm->dev, "%s: Fail to get fuel_gauge\n", __func__);
		ret = -ENODEV;
		return ret;
	}

	*max_volt_uv = 0;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN, &val);
	if (ret) {
		dev_err(cm->dev, "Fail to get voltage max design, ret = %d\n", ret);
		return ret;
	}

	power_supply_put(fuel_gauge);
	*max_volt_uv = val.intval;

	return ret;
}

static bool cm_init_basp_parameter(struct charger_manager *cm)
{
	int ret;
	int max_volt_uv;

	ret = cm_get_basp_max_volt(cm, &max_volt_uv);
	if (ret)
		return false;

	if (max_volt_uv == 0 || max_volt_uv == -1)
		return false;

	return cm_reset_basp_parameters(cm, max_volt_uv);
}

static void cm_power_path_enable(struct charger_manager *cm, int cmd)
{
	int ret, i;
	union power_supply_propval val = {0,};
	struct power_supply *psy;

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find primary power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = cmd;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		power_supply_put(psy);
		if (ret) {
			dev_err(cm->dev, "Fail to set power_path[%d] of %s, ret = %d\n",
				cmd, cm->desc->psy_charger_stat[i], ret);
			continue;
		}
	}
}

static bool cm_is_power_path_enabled(struct charger_manager *cm)
{
	int ret, i;
	bool enabled = false;
	union power_supply_propval val = {0,};
	struct power_supply *psy;

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find primary power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = CM_POWER_PATH_ENABLE_CMD;
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		power_supply_put(psy);
		if (!ret) {
			if (val.intval) {
				enabled = true;
				break;
			}
		}
	}

	dev_info(cm->dev, "%s: %s\n", __func__, enabled ? "enabled" : "disabled");
	return enabled;
}

/**
 * is_charging - Returns true if the battery is being charged.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_charging(struct charger_manager *cm)
{
	bool charging = false;
	struct power_supply *psy;
	union power_supply_propval val;
	int i, ret;

	/* If there is no battery, it cannot be charged */
	if (!is_batt_present(cm))
		return false;

	if (!is_ext_pwr_online(cm))
		return charging;

	/* If at least one of the charger is charging, return yes */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		/* 1. The charger sholuld not be DISABLED */
		if (cm->emergency_stop)
			continue;
		if (!cm->charger_enabled)
			continue;

		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
			continue;
		}

		/*
		 * 2. The charger should not be FULL, DISCHARGING,
		 * or NOT_CHARGING.
		 */
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS,
				&val);
		power_supply_put(psy);
		if (ret) {
			dev_warn(cm->dev, "Cannot read STATUS value from %s\n",
				 cm->desc->psy_charger_stat[i]);
			continue;
		}
		if (val.intval == POWER_SUPPLY_STATUS_FULL ||
		    val.intval == POWER_SUPPLY_STATUS_DISCHARGING ||
		    val.intval == POWER_SUPPLY_STATUS_NOT_CHARGING)
			continue;

		/* Then, this is charging. */
		charging = true;
		break;
	}

	return charging;
}

static bool cm_primary_charger_enable(struct charger_manager *cm, bool enable)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret;

	if (!cm->desc->psy_charger_stat || !cm->desc->psy_charger_stat[0])
		return false;

	psy = power_supply_get_by_name(cm->desc->psy_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			cm->desc->psy_charger_stat[0]);
		return false;
	}

	val.intval = enable;
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev, "failed to %s primary charger, ret = %d\n",
			enable ? "enable" : "disable", ret);
		return false;
	}

	return true;
}

/**
 * is_full_charged - Returns true if the battery is fully charged.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_full_charged(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	bool is_full = false;
	int ret = 0;
	int uV, uA;

	/* If there is no battery, it cannot be charged */
	if (!is_batt_present(cm))
		return false;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return false;

	if (desc->fullbatt_full_capacity > 0) {
		val.intval = 0;

		/* Not full if capacity of fuel gauge isn't full */
		ret = power_supply_get_property(fuel_gauge,
						POWER_SUPPLY_PROP_CHARGE_FULL, &val);
		if (!ret && val.intval > desc->fullbatt_full_capacity) {
			is_full = true;
			goto out;
		}
	}

	/* Full, if it's over the fullbatt voltage */
	if (desc->fullbatt_uV > 0 && desc->fullbatt_uA > 0) {
		ret = get_vbat_now_uV(cm, &uV);
		if (ret)
			goto out;

		ret = get_ibat_now_uA(cm, &uA);
		if (ret)
			goto out;

		/* Battery is already full, checks voltage drop. */
		if (cm->battery_status == POWER_SUPPLY_STATUS_FULL && desc->fullbatt_vchkdrop_uV) {
			int batt_ocv;

			ret = get_batt_ocv(cm, &batt_ocv);
			if (ret || batt_ocv < 0)
				goto out;

			if ((u32)batt_ocv > (cm->desc->fullbatt_uV - cm->desc->fullbatt_vchkdrop_uV))
				is_full = true;
			goto out;
		}

		if (desc->first_fullbatt_uA > 0 && uV >= desc->fullbatt_uV &&
		    uA > desc->fullbatt_uA && uA <= desc->first_fullbatt_uA && uA >= 0) {
			if (++desc->first_trigger_cnt > 1)
				cm->desc->force_set_full = true;
		} else {
			desc->first_trigger_cnt = 0;
		}

		if (uV >= desc->fullbatt_uV && uA <= desc->fullbatt_uA && uA >= 0) {
			if (++desc->trigger_cnt > 1) {
				if (cm->desc->cap >= CM_CAP_FULL_PERCENT) {
					if (desc->trigger_cnt == 2)
						adjust_fuel_cap(cm, CM_FORCE_SET_FUEL_CAP_FULL);
					is_full = true;
				} else {
					is_full = false;
					adjust_fuel_cap(cm, CM_FORCE_SET_FUEL_CAP_FULL);
					if (desc->trigger_cnt == 2)
						cm_primary_charger_enable(cm, false);
				}
				cm->desc->force_set_full = true;
			} else {
				is_full = false;
			}
			goto out;
		} else {
			is_full = false;
			desc->trigger_cnt = 0;
			goto out;
		}
	}

	/* Full, if the capacity is more than fullbatt_soc */
	if (desc->fullbatt_soc > 0) {
		val.intval = 0;

		ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY, &val);
		if (!ret && val.intval >= desc->fullbatt_soc) {
			is_full = true;
			goto out;
		}
	}

out:
	power_supply_put(fuel_gauge);
	return is_full;
}

/**
 * is_polling_required - Return true if need to continue polling for this CM.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_polling_required(struct charger_manager *cm)
{
	switch (cm->desc->polling_mode) {
	case CM_POLL_DISABLE:
		return false;
	case CM_POLL_ALWAYS:
		return true;
	case CM_POLL_EXTERNAL_POWER_ONLY:
		return is_ext_pwr_online(cm);
	case CM_POLL_CHARGING_ONLY:
		return is_charging(cm);
	default:
		dev_warn(cm->dev, "Incorrect polling_mode (%d)\n",
			 cm->desc->polling_mode);
	}

	return false;
}

static void cm_update_current_jeita_status(struct charger_manager *cm)
{
	int cur_jeita_status;

	/**
	 * Note that it need to vote for ibat before the caller of this function
	 * if does not define jeita table
	 */
	if (cm->desc->jeita_tab_size && !cm->charging_status) {
		cur_jeita_status = cm_manager_get_jeita_status(cm, cm->desc->temperature);
		if (cm->desc->jeita_disabled)
			cur_jeita_status = cm->desc->force_jeita_status;
		cm_manager_adjust_current(cm, cur_jeita_status);
	}
}

static void cm_update_charge_info(struct charger_manager *cm, int cmd)
{
	struct charger_desc *desc = cm->desc;
	struct cm_thermal_info *thm_info = &cm->desc->thm_info;

	mutex_lock(&cm->desc->charge_info_mtx);
	switch (desc->charger_type) {
	case CM_CHARGER_TYPE_DCP:
		desc->charge_limit_cur = desc->cur.dcp_cur;
		desc->input_limit_cur = desc->cur.dcp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_DCP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_DCP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_DCP];
		}
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case CM_CHARGER_TYPE_SDP:
		desc->charge_limit_cur = desc->cur.sdp_cur;
		desc->input_limit_cur = desc->cur.sdp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_SDP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_SDP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_SDP];
		}
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case CM_CHARGER_TYPE_CDP:
		desc->charge_limit_cur = desc->cur.cdp_cur;
		desc->input_limit_cur = desc->cur.cdp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_CDP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_CDP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_CDP];
		}
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case CM_CHARGER_TYPE_FAST:
		if (desc->enable_fast_charge) {
			desc->charge_limit_cur = desc->cur.fchg_cur;
			desc->input_limit_cur = desc->cur.fchg_limit;
			thm_info->adapter_default_charge_vol = 9;
			if (desc->jeita_size[SPRD_BATTERY_JEITA_FCHG]) {
				desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_FCHG];
				desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_FCHG];
			}
			if (desc->fast_charge_voltage_max)
				desc->charge_voltage_max = desc->fast_charge_voltage_max;
			if (desc->fast_charge_voltage_drop)
				desc->charge_voltage_drop = desc->fast_charge_voltage_drop;
			break;
		}
		desc->charge_limit_cur = desc->cur.dcp_cur;
		desc->input_limit_cur = desc->cur.dcp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_DCP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_DCP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_DCP];
		}
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case CM_CHARGER_TYPE_ADAPTIVE:
		if (desc->cp.cp_running && !desc->cp.recovery) {
			desc->charge_limit_cur = desc->cur.flash_cur;
			desc->input_limit_cur = desc->cur.flash_limit;
			thm_info->adapter_default_charge_vol = 11;
			if (desc->jeita_size[SPRD_BATTERY_JEITA_FLASH]) {
				desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_FLASH];
				desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_FLASH];
			}
			if (desc->flash_charge_voltage_max)
				desc->charge_voltage_max = desc->flash_charge_voltage_max;
			if (desc->flash_charge_voltage_drop)
				desc->charge_voltage_drop = desc->flash_charge_voltage_drop;
			break;
		}
		desc->charge_limit_cur = desc->cur.dcp_cur;
		desc->input_limit_cur = desc->cur.dcp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_DCP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_DCP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_DCP];
		}
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case CM_WIRELESS_CHARGER_TYPE_BPP:
		desc->charge_limit_cur = desc->cur.wl_bpp_cur;
		desc->input_limit_cur = desc->cur.wl_bpp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_WL_BPP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_WL_BPP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_WL_BPP];
		}
		if (desc->wireless_normal_charge_voltage_max)
			desc->charge_voltage_max = desc->wireless_normal_charge_voltage_max;
		if (desc->wireless_normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->wireless_normal_charge_voltage_drop;
		break;
	case CM_WIRELESS_CHARGER_TYPE_EPP:
		desc->charge_limit_cur = desc->cur.wl_epp_cur;
		desc->input_limit_cur = desc->cur.wl_epp_limit;
		thm_info->adapter_default_charge_vol = 11;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_WL_EPP]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_WL_EPP];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_WL_EPP];
		}
		if (desc->wireless_fast_charge_voltage_max)
			desc->charge_voltage_max = desc->wireless_fast_charge_voltage_max;
		if (desc->wireless_fast_charge_voltage_drop)
			desc->charge_voltage_drop = desc->wireless_fast_charge_voltage_drop;
		break;
	default:
		desc->charge_limit_cur = desc->cur.unknown_cur;
		desc->input_limit_cur = desc->cur.unknown_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_size[SPRD_BATTERY_JEITA_UNKNOWN]) {
			desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_UNKNOWN];
			desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_UNKNOWN];
		}
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	}

	mutex_unlock(&cm->desc->charge_info_mtx);

	if (thm_info->thm_pwr && thm_info->adapter_default_charge_vol)
		thm_info->thm_adjust_cur = (int)(thm_info->thm_pwr /
			thm_info->adapter_default_charge_vol) * 1000;

	dev_info(cm->dev, "%s, chgr type = %d, fchg_en = %d, cp_running = %d, cp_recovery = %d"
		 " max chg_lmt_cur = %duA, max inpt_lmt_cur = %duA, max chg_volt = %duV,"
		 " chg_volt_drop = %d, adapter_chg_volt = %dmV, thm_cur = %d, chg_info_cmd = 0x%x,"
		 " jeita_size = %d\n",
		 __func__, desc->charger_type, desc->enable_fast_charge, desc->cp.cp_running,
		 desc->cp.recovery, desc->charge_limit_cur, desc->input_limit_cur,
		 desc->charge_voltage_max, desc->charge_voltage_drop,
		 thm_info->adapter_default_charge_vol * 1000, thm_info->thm_adjust_cur, cmd,
		 desc->jeita_tab_size);

	if (!cm->cm_charge_vote || !cm->cm_charge_vote->vote) {
		dev_err(cm->dev, "%s: cm_charge_vote is null\n", __func__);
		return;
	}

	if (cmd & CM_CHARGE_INFO_CHARGE_LIMIT)
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBAT,
					 SPRD_VOTE_TYPE_IBAT_ID_CHARGER_TYPE,
					 SPRD_VOTE_CMD_MIN, desc->charge_limit_cur, cm);
	if (cmd & CM_CHARGE_INFO_INPUT_LIMIT)
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBUS,
					 SPRD_VOTE_TYPE_IBUS_ID_CHARGER_TYPE,
					 SPRD_VOTE_CMD_MIN, desc->input_limit_cur, cm);
	if (cmd & CM_CHARGE_INFO_THERMAL_LIMIT && thm_info->thm_adjust_cur > 0) {
		/* The ChargerIC with linear charging cannot set Ibus, only Ibat. */
		if (cm->desc->thm_info.need_calib_charge_lmt)
			cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBAT,
					 SPRD_VOTE_TYPE_IBAT_ID_CHARGE_CONTROL_LIMIT,
					 SPRD_VOTE_CMD_MIN,
					 cm->desc->thm_info.thm_adjust_cur, cm);
		else
			cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
						 SPRD_VOTE_TYPE_IBUS,
						 SPRD_VOTE_TYPE_IBUS_ID_CHARGE_CONTROL_LIMIT,
						 SPRD_VOTE_CMD_MIN,
						 cm->desc->thm_info.thm_adjust_cur, cm);
	}

	if (cmd & CM_CHARGE_INFO_JEITA_LIMIT) {
		cm_update_current_jeita_status(cm);
		if (cm->charging_status & (CM_CHARGE_TEMP_OVERHEAT | CM_CHARGE_TEMP_COLD))
			mod_delayed_work(cm_wq, &cm_monitor_work, 0);
	}
}

static void cm_vote_property(struct charger_manager *cm, int target_val,
			     const char **name, enum power_supply_property psp)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret;

	if (!name) {
		dev_err(cm->dev, "psy name is null!!!\n");
		return;
	}

	for (i = 0; name[i]; i++) {
		psy = power_supply_get_by_name(name[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n", name[i]);
			continue;
		}

		val.intval = target_val;
		ret = power_supply_set_property(psy, psp, &val);
		power_supply_put(psy);
		if (ret)
			dev_err(cm->dev, "failed to %s set power_supply_property[%d], ret = %d\n",
				name[i], psp, ret);
	}
}

static int cm_check_parallel_charger(struct charger_manager *cm, int cur)
{
	if (cm->desc->enable_fast_charge && cm->desc->psy_charger_stat[1])
		cur /= 2;

	return cur;
}

static void cm_sprd_vote_callback(struct sprd_vote *vote_gov, int vote_type,
				  int value, void *data)
{
	struct charger_manager *cm = (struct charger_manager *)data;
	const char **psy_charger_name;

	dev_info(cm->dev, "%s, %s[%d]\n", __func__, vote_type_names[vote_type], value);
	switch (vote_type) {
	case SPRD_VOTE_TYPE_IBAT:
		psy_charger_name = cm->desc->psy_charger_stat;
		value = cm_check_parallel_charger(cm, value);
		cm_vote_property(cm, value, psy_charger_name,
				 POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT);
		break;
	case SPRD_VOTE_TYPE_IBUS:
		psy_charger_name = cm->desc->psy_charger_stat;
		value = cm_check_parallel_charger(cm, value);
		cm_vote_property(cm, value, psy_charger_name,
				 POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT);
		break;
	case SPRD_VOTE_TYPE_CCCV:
		psy_charger_name = cm->desc->psy_charger_stat;
		if (cm->desc->cp.cp_running)
			psy_charger_name = cm->desc->psy_cp_stat;
		cm_vote_property(cm, value, psy_charger_name,
				 POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX);
		break;
	default:
		dev_err(cm->dev, "vote_gov: vote_type[%d] error!!!\n", vote_type);
		break;
	}
}

static int cm_get_adapter_max_voltage(struct charger_manager *cm, int *max_vol)
{
	int ret;

	*max_vol = 0;
	if (!cm->fchg_info->ops || !cm->fchg_info->ops->get_fchg_vol_max) {
		dev_err(cm->dev, "%s, fchg ops or get_fchg_vol_max is null\n", __func__);
		return -EINVAL;
	}

	ret = cm->fchg_info->ops->get_fchg_vol_max(cm->fchg_info, max_vol);
	if (ret)
		dev_err(cm->dev, "%s, failed to get fchg max voltage, ret=%d\n",
			__func__, ret);

	return ret;
}

static int cm_get_adapter_max_current(struct charger_manager *cm, int input_vol, int *max_cur)
{
	int ret;

	*max_cur = 0;
	if (!cm->fchg_info->ops || !cm->fchg_info->ops->get_fchg_cur_max) {
		dev_err(cm->dev, "%s, fchg ops or get_fchg_cur_max is null\n", __func__);
		return -EINVAL;
	}

	ret = cm->fchg_info->ops->get_fchg_cur_max(cm->fchg_info, input_vol, max_cur);
	if (ret)
		dev_err(cm->dev, "%s, failed to get fchg max current, ret=%d\n",
			__func__, ret);

	return ret;
}

static int cm_set_charger_ovp(struct charger_manager *cm, int cmd)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret, i;

	if (!desc->psy_charger_stat) {
		dev_err(cm->dev, "psy_charger_stat is null!!!\n");
		return -ENODEV;
	}

	/*
	 * make the psy_charger_stat[0] to be main charger,
	 * set the main charger charge current and limit current
	 * in 9V/5V fast charge status.
	 */
	for (i = 0; desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			return -ENODEV;
		}

		val.intval = cmd;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		power_supply_put(psy);
		if (ret) {
			dev_err(cm->dev, "failed to set \"%s\" ovp cmd = %d, ret = %d\n",
				desc->psy_charger_stat[i], cmd, ret);
			return ret;
		}
	}

	return 0;
}

static int cm_enable_second_charger(struct charger_manager *cm, bool enable)
{

	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (!desc->psy_charger_stat[1])
		return 0;

	psy = power_supply_get_by_name(desc->psy_charger_stat[1]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_charger_stat[1]);
		power_supply_put(psy);
		return -ENODEV;
	}

	/*
	 * enable/disable the second charger to start/stop charge
	 */
	val.intval = enable;
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev,
			"failed to %s second charger \n", enable ? "enable" : "disable");
		return ret;
	}

	return 0;
}

static int cm_adjust_fchg_voltage(struct charger_manager *cm, int vol)
{
	int ret;

	if (!cm->fchg_info->ops || !cm->fchg_info->ops->adj_fchg_vol) {
		dev_err(cm->dev, "%s, fchg ops or adj_fchg_vol is null\n", __func__);
		return -EINVAL;
	}

	ret = cm->fchg_info->ops->adj_fchg_vol(cm->fchg_info, vol);
	if (ret)
		dev_err(cm->dev, "%s, failed to adjust fchg voltage vol=%d, ret=%d\n",
			__func__, vol, ret);

	return ret;
}

static bool cm_is_reach_fchg_threshold(struct charger_manager *cm)
{
	int batt_ocv, batt_uA, fchg_ocv_threshold, thm_cur;
	int cur_jeita_status, target_cur;

	if (get_batt_ocv(cm, &batt_ocv)) {
		dev_err(cm->dev, "get_batt_ocv error.\n");
		return false;
	}

	if (get_ibat_now_uA(cm, &batt_uA)) {
		dev_err(cm->dev, "get_ibat_now_uA error.\n");
		return false;
	}

	target_cur = batt_uA;
	if (cm->desc->jeita_tab_size) {
		cur_jeita_status = cm_manager_get_jeita_status(cm, cm->desc->temperature);
		if (cm->desc->jeita_disabled)
			cur_jeita_status = cm->desc->force_jeita_status;

		target_cur = 0;
		if (cur_jeita_status != cm->desc->jeita_tab_size)
			target_cur = cm->desc->jeita_tab[cur_jeita_status].current_ua;
	}

	fchg_ocv_threshold = CM_FAST_CHARGE_START_VOLTAGE_HTHRESHOLD;
	if (cm->desc->fchg_ocv_threshold > 0)
		fchg_ocv_threshold = cm->desc->fchg_ocv_threshold;

	thm_cur = CM_FAST_CHARGE_ENABLE_THERMAL_CURRENT;
	if (cm->desc->thm_info.thm_adjust_cur > 0)
		thm_cur = cm->desc->thm_info.thm_adjust_cur;

	if (target_cur >= CM_FAST_CHARGE_ENABLE_CURRENT &&
	    thm_cur >= CM_FAST_CHARGE_ENABLE_THERMAL_CURRENT &&
	    batt_ocv >= CM_FAST_CHARGE_START_VOLTAGE_LTHRESHOLD &&
	    batt_ocv < fchg_ocv_threshold)
		return true;
	else if (batt_ocv >= CM_FAST_CHARGE_START_VOLTAGE_LTHRESHOLD &&
		 batt_uA >= CM_FAST_CHARGE_ENABLE_CURRENT)
		return true;

	return false;
}

static int cm_fixed_fchg_enable(struct charger_manager *cm)
{
	int ret, adapter_max_vbus;

	/*
	 * if it occurs emergency event, don't enable fast charge.
	 */
	if (cm->emergency_stop)
		return -EAGAIN;

	if (!cm->desc) {
		dev_err(cm->dev, "cm->desc is a null pointer!!!\n");
		return 0;
	}

	/*
	 * if it don't define sprd,support-fchg in dts,
	 * we think that it don't plan to use fast charge.
	 */
	if (!cm->fchg_info->support_fchg)
		return 0;

	if (!cm->desc->is_fast_charge || cm->desc->enable_fast_charge)
		return 0;

	if (!cm->cm_charge_vote || !cm->cm_charge_vote->vote) {
		dev_err(cm->dev, "%s: cm_charge_vote is null\n", __func__);
		return 0;
	}

	/*
	 * cm->desc->enable_fast_charge should be set to true when the transient
	 * current is voting, otherwise the current of the parallel charging
	 * scheme cannot be halved.
	 *
	 * In the normal fast charge voltage regulation process, add the normal
	 * fast charge transition current to prevent overload and other abnormal
	 * situations.
	 */
	cm->desc->enable_fast_charge = true;
	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_IBUS,
				 SPRD_VOTE_TYPE_IBUS_ID_FCHG_FIXED_TRANSITION,
				 SPRD_VOTE_CMD_MIN, CM_FAST_CHARGE_TRANSITION_CURRENT_1P5A, cm);

	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

	/*
	 * adjust over voltage protection in 9V
	 */
	ret = cm_set_charger_ovp(cm, CM_FAST_CHARGE_OVP_ENABLE_CMD);
	if (ret) {
		dev_err(cm->dev, "failed to enable fchg ovp\n");
		/*
		 * if it failed to set fast charge ovp, reset to DCP setting
		 * first so that the charging ovp can reach the condition again.
		 */
		goto tran_cur_err;
	}

	/*
	 * adjust fast charger output voltage from 5V to 9V
	 */
	ret = cm_get_adapter_max_voltage(cm, &adapter_max_vbus);
	if (ret) {
		dev_err(cm->dev, "failed to obtain the adapter max voltage\n");
		goto ovp_err;
	}

	if (adapter_max_vbus > CM_FAST_CHARGE_VOLTAGE_9V)
		adapter_max_vbus = CM_FAST_CHARGE_VOLTAGE_9V;

	ret = cm_adjust_fchg_voltage(cm, adapter_max_vbus);
	if (ret) {
		dev_err(cm->dev, "failed to adjust fast charger voltage\n");
		goto ovp_err;
	}

	ret = cm_enable_second_charger(cm, true);
	if (ret) {
		dev_err(cm->dev, "failed to enable second charger\n");
		goto adj_vol_err;
	}

	goto out;

adj_vol_err:
	cm_adjust_fchg_voltage(cm, CM_FAST_CHARGE_VOLTAGE_5V);

ovp_err:
	cm_set_charger_ovp(cm, CM_FAST_CHARGE_OVP_DISABLE_CMD);

tran_cur_err:
	cm->desc->enable_fast_charge = false;
	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

out:
	cm->cm_charge_vote->vote(cm->cm_charge_vote, false,
				 SPRD_VOTE_TYPE_IBUS,
				 SPRD_VOTE_TYPE_IBUS_ID_FCHG_FIXED_TRANSITION,
				 SPRD_VOTE_CMD_MIN, CM_FAST_CHARGE_TRANSITION_CURRENT_1P5A, cm);
	return ret;
}

static int cm_fixed_fchg_disable(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	int ret, charge_vol;

	if (!desc->enable_fast_charge)
		return 0;

	if (!cm->cm_charge_vote || !cm->cm_charge_vote->vote) {
		dev_err(cm->dev, "%s: cm_charge_vote is null\n", __func__);
		return 0;
	}

	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_IBUS,
				 SPRD_VOTE_TYPE_IBUS_ID_FCHG_FIXED_TRANSITION,
				 SPRD_VOTE_CMD_MIN, CM_FAST_CHARGE_TRANSITION_CURRENT_1P5A, cm);

	/*
	 * If defined psy_charger_stat[1], then disable the second
	 * charger first.
	 */
	ret = cm_enable_second_charger(cm, false);
	if (ret) {
		dev_err(cm->dev, "failed to disable second charger\n");
		goto out;
	}

	/*
	 * Adjust fast charger output voltage from 9V to 5V
	 */
	if (!desc->wait_vbus_stable &&
	    cm_adjust_fchg_voltage(cm, CM_FAST_CHARGE_VOLTAGE_5V)) {
		dev_err(cm->dev, "%s, failed to adjust 5V fast charger voltage\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	/*
	 * Waiting for the charger to step down to prevent the occurrence
	 * of Vbus overvoltage.
	 * Reason: It takes a certain time for the charger to switch from
	 *         9V to 5V. At this time, if the OVP is directly set to
	 *         6.5V, there is a small probability that Vbus overvoltage
	 *         will occur.
	 */
	ret = get_charger_voltage(cm, &charge_vol);
	if (ret) {
		dev_err(cm->dev, "%s, fail to get charge vol\n", __func__);
		goto out;
	}

	if (charge_vol > desc->normal_charge_voltage_max) {
		dev_err(cm->dev, "%s, waiting for the charger to step down\n", __func__);
		desc->wait_vbus_stable = true;
		ret = -EINVAL;
		goto out;
	}

	desc->wait_vbus_stable = false;

	ret = cm_set_charger_ovp(cm, CM_FAST_CHARGE_OVP_DISABLE_CMD);
	if (ret) {
		dev_err(cm->dev, "%s, failed to disable fchg ovp\n", __func__);
		goto out;
	}

	desc->enable_fast_charge = false;
	desc->fast_charge_disable_count = 0;
	/*
	 * Adjust over voltage protection in 5V
	 */
	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

out:
	cm->cm_charge_vote->vote(cm->cm_charge_vote, false,
				 SPRD_VOTE_TYPE_IBUS,
				 SPRD_VOTE_TYPE_IBUS_ID_FCHG_FIXED_TRANSITION,
				 SPRD_VOTE_CMD_MIN, CM_FAST_CHARGE_TRANSITION_CURRENT_1P5A, cm);
	return ret;
}

static bool cm_is_disable_fixed_fchg_check(struct charger_manager *cm, int *delay_work_ms)
{
	int batt_uV, batt_uA, ret, chg_vol = 0;

	*delay_work_ms = cm->desc->polling_interval_ms;
	if (!cm->desc->enable_fast_charge)
		return true;

	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret) {
		dev_err(cm->dev, "%s, failed to get batt uV, ret=%d\n", __func__, ret);
		return false;
	}

	ret = get_ibat_now_uA(cm, &batt_uA);
	if (ret) {
		dev_err(cm->dev, "%s, failed to get batt uA, ret=%d\n", __func__, ret);
		return false;
	}

	ret = get_charger_voltage(cm, &chg_vol);
	if (ret)
		dev_err(cm->dev, "%s, get chg_vol error, ret=%d\n", __func__, ret);

	if (batt_uV < CM_FAST_CHARGE_DISABLE_BATTERY_VOLTAGE ||
	    batt_uA < CM_FAST_CHARGE_DISABLE_CURRENT ||
	    (!ret && chg_vol < CM_FAST_CHARGE_VOLTAGE_5V_THRESHOLD)) {
		cm->desc->fast_charge_disable_count++;
		*delay_work_ms = CM_CHK_DIS_FCHG_WORK_MS;
		pm_wakeup_event(cm->dev, CM_CHK_DIS_FCHG_WORK_MS + 500);
	} else {
		cm->desc->fast_charge_disable_count = 0;
	}

	if (cm->desc->fast_charge_disable_count < CM_FAST_CHARGE_DISABLE_COUNT)
		return false;

	dev_info(cm->dev, "%s, vbat: %d, ibat: %d, vbus: %d, exit fixed fchg\n",
		 __func__, batt_uV, batt_uA, chg_vol);
	return true;
}

static int cm_get_ibat_avg(struct charger_manager *cm, int *ibat)
{
	int ret, batt_uA, min, max, i, sum = 0;
	struct cm_ir_compensation *ir_sts = &cm->desc->ir_comp;

	ret = get_ibat_now_uA(cm, &batt_uA);
	if (ret) {
		dev_err(cm->dev, "get bat_uA error.\n");
		return ret;
	}

	if (ir_sts->ibat_index >= CM_IBAT_BUFF_CNT)
		ir_sts->ibat_index = 0;
	ir_sts->ibat_buf[ir_sts->ibat_index++] = batt_uA;

	if (ir_sts->ibat_buf[CM_IBAT_BUFF_CNT - 1] == CM_MAGIC_NUM)
		return -EINVAL;

	min = max = ir_sts->ibat_buf[0];
	for (i = 0; i < CM_IBAT_BUFF_CNT; i++) {
		if (max < ir_sts->ibat_buf[i])
			max = ir_sts->ibat_buf[i];
		if (min > ir_sts->ibat_buf[i])
			min = ir_sts->ibat_buf[i];
		sum += ir_sts->ibat_buf[i];
	}

	sum  = sum - min - max;

	*ibat = DIV_ROUND_CLOSEST(sum, (CM_IBAT_BUFF_CNT - 2));

	if (*ibat < 0)
		*ibat = 0;

	return ret;
}

static void cm_ir_compensation_init(struct charger_manager *cm)
{
	cm->desc->ir_comp.ibat_buf[CM_IBAT_BUFF_CNT - 1] = CM_MAGIC_NUM;
	cm->desc->ir_comp.ibat_index = 0;
	cm->desc->ir_comp.last_target_cccv = 0;
	if (cm->cm_charge_vote)
		cm->cm_charge_vote->vote(cm->cm_charge_vote, false,
					 SPRD_VOTE_TYPE_CCCV,
					 SPRD_VOTE_TYPE_CCCV_ID_IR,
					 SPRD_VOTE_CMD_MIN,
					 0, cm);
}

static void cm_ir_compensation_enable(struct charger_manager *cm, bool enable)
{
	struct cm_ir_compensation *ir_sts = &cm->desc->ir_comp;

	cm_ir_compensation_init(cm);

	if (enable) {
		if (ir_sts->rc && !ir_sts->ir_compensation_en) {
			dev_info(cm->dev, "%s enable ir compensation\n", __func__);
			ir_sts->ir_compensation_en = true;
			queue_delayed_work(system_power_efficient_wq,
					   &cm->ir_compensation_work,
					   CM_IR_COMPENSATION_TIME * HZ);
		}
		ir_sts->ir_compensation_en = true;
	} else {
		if (ir_sts->ir_compensation_en) {
			dev_info(cm->dev, "%s stop ir compensation\n", __func__);
			cancel_delayed_work_sync(&cm->ir_compensation_work);
			ir_sts->ir_compensation_en = false;
		}
	}
}

static void cm_ir_compensation(struct charger_manager *cm, enum cm_ir_comp_state state, int *target)
{
	struct cm_ir_compensation *ir_sts = &cm->desc->ir_comp;
	int ibat_avg, target_cccv;

	if (!ir_sts->rc)
		return;

	if (cm_get_ibat_avg(cm, &ibat_avg))
		return;

	target_cccv = ir_sts->us + (ibat_avg / 1000)  * ir_sts->rc;

	if (target_cccv < ir_sts->us_lower_limit)
		target_cccv = ir_sts->us_lower_limit;
	else if (target_cccv > ir_sts->us_upper_limit)
		target_cccv = ir_sts->us_upper_limit;

	*target = target_cccv;

	if ((*target / 1000) == (ir_sts->last_target_cccv / 1000))
		return;

	dev_info(cm->dev, "%s, us = %d, rc = %d, upper_limit = %d, lower_limit = %d, "
		 "target_cccv = %d, ibat_avg = %d, offset = %d\n",
		 __func__, ir_sts->us, ir_sts->rc, ir_sts->us_upper_limit,
		 ir_sts->us_lower_limit, target_cccv, ibat_avg,
		 ir_sts->cp_upper_limit_offset);

	ir_sts->last_target_cccv = *target;
	switch (state) {
	case CM_IR_COMP_STATE_CP:
		target_cccv = min(ir_sts->us_upper_limit,
				  (*target + ir_sts->cp_upper_limit_offset));
		fallthrough;
	case CM_IR_COMP_STATE_NORMAL:
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					SPRD_VOTE_TYPE_CCCV,
					SPRD_VOTE_TYPE_CCCV_ID_IR,
					SPRD_VOTE_CMD_MAX,
					target_cccv, cm);
		break;
	default:
		break;
	}
}

static void cm_ir_compensation_works(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
						  struct charger_manager,
						  ir_compensation_work);

	int target_cccv;

	cm_ir_compensation(cm, CM_IR_COMP_STATE_NORMAL, &target_cccv);
	queue_delayed_work(system_power_efficient_wq,
			   &cm->ir_compensation_work,
			   CM_IR_COMPENSATION_TIME * HZ);
}

static void cm_fixed_fchg_control_switch(struct charger_manager *cm, bool enable)
{
	dev_dbg(cm->dev, "%s enable = %d start\n", __func__, enable);

	if (!cm->fchg_info->support_fchg)
		return;

	cm->desc->check_fixed_fchg_threshold = enable;
	if (!enable && cm->desc->fixed_fchg_running) {
		cancel_delayed_work_sync(&cm->fixed_fchg_work);
		schedule_delayed_work(&cm->fixed_fchg_work, 0);
	}
}

static bool cm_is_need_start_fixed_fchg(struct charger_manager *cm)
{
	bool need = false;

	if (!cm->fchg_info->support_fchg || cm->desc->fixed_fchg_running)
		return false;

	cm_charger_is_support_fchg(cm);
	if (cm->desc->fast_charger_type == CM_CHARGER_TYPE_FAST &&
	    cm->charger_enabled && cm->desc->check_fixed_fchg_threshold &&
	    cm_is_reach_fchg_threshold(cm))
		need = true;

	return need;
}

static void cm_start_fixed_fchg(struct charger_manager *cm, bool start)
{
	if (!cm->desc->fixed_fchg_running && start) {
		dev_info(cm->dev, "%s, reach fchg threshold, enable it\n", __func__);
		cm->desc->fixed_fchg_running = true;
		schedule_delayed_work(&cm->fixed_fchg_work, 0);
	}
}

static void cm_fixed_fchg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
						  struct charger_manager,
						  fixed_fchg_work);
	int ret, delay_work_ms = cm->desc->polling_interval_ms;

	/*
	 * Effects:
	 *   1. Prevent CM_FAST_CHARGE_ENABLE_COUNT from becoming PPS
	 *      within the time and enable the fast charge status.
	 */
	if (!cm->charger_enabled || cm->desc->fast_charger_type != CM_CHARGER_TYPE_FAST)
		goto stop_fixed_fchg;

	/*
	 * The first if branch: fix the problem that the Xiaomi 65W
	 *                      charger PD2.0 and PPS follow closely.
	 */
	if (cm->desc->fast_charger_type == CM_CHARGER_TYPE_FAST &&
	    cm->desc->fast_charge_enable_count < CM_FAST_CHARGE_ENABLE_COUNT) {
		cm->desc->fast_charge_enable_count++;
		delay_work_ms = CM_CP_WORK_TIME_MS;
	} else if (cm->desc->enable_fast_charge) {
		if (cm_is_disable_fixed_fchg_check(cm, &delay_work_ms))
			goto stop_fixed_fchg;
	} else {
		ret = cm_fixed_fchg_enable(cm);
		if (ret) {
			dev_err(cm->dev, "%s, failed to enable fixed fchg\n", __func__);
			cm->desc->fixed_fchg_running = false;
			cm->desc->fast_charge_enable_count = 0;
			return;
		}
	}

	schedule_delayed_work(&cm->fixed_fchg_work, msecs_to_jiffies(delay_work_ms));
	return;

stop_fixed_fchg:
	ret = cm_fixed_fchg_disable(cm);
	if (ret) {
		dev_err(cm->dev, "%s, failed to disable fixed fchg, try again!\n", __func__);
		schedule_delayed_work(&cm->fixed_fchg_work,
				      msecs_to_jiffies(CM_TRY_DIS_FCHG_WORK_MS));
		return;
	}
	cm->desc->fixed_fchg_running = false;
	cm->desc->fast_charge_enable_count = 0;
}

static void cm_cp_state_change(struct charger_manager *cm, int state)
{
	cm->desc->cp.cp_state = state;
	dev_dbg(cm->dev, "%s, current cp_state = %d\n", __func__, state);
}

static bool cm_cp_master_charger_enable(struct charger_manager *cm, bool enable)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int ret;

	if (!cm->desc->psy_cp_stat)
		return false;

	cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[0]);
	if (!cp_psy) {
		dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[0]);
		return false;
	}

	val.intval = enable;
	ret = power_supply_set_property(cp_psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
	power_supply_put(cp_psy);
	if (ret) {
		dev_err(cm->dev, "failed to %s master charge pump, ret = %d\n",
			enable ? "enabel" : "disable", ret);
		return false;
	}

	return true;
}

static void cm_init_cp(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	if (!cm->desc->psy_cp_stat)
		return;

	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		val.intval = CM_USB_PRESENT_CMD;
		ret = power_supply_set_property(cp_psy, POWER_SUPPLY_PROP_PRESENT, &val);
		power_supply_put(cp_psy);
		if (ret) {
			dev_err(cm->dev, "fail to init cp[%d], ret = %d\n", i, ret);
			break;
		}
	}
}

static int cm_adjust_fchg_current(struct charger_manager *cm, int cur)
{
	union power_supply_propval val;
	int ret;

	if (!cm->fchg_info->ops || !cm->fchg_info->ops->adj_fchg_cur) {
		dev_err(cm->dev, "%s, fchg ops or adj_fchg_cur is null\n", __func__);
		return -EINVAL;
	}

	val.intval = cur;
	ret = cm->fchg_info->ops->adj_fchg_cur(cm->fchg_info, cur);
	if (ret) {
		dev_err(cm->dev, "%s, failed to adjust fchg current = %d, ret=%d\n",
			__func__, cur, ret);
		return ret;
	}

	return 0;
}

/*
 *  Relying on the fast charging protocol of DP/DM for handshake,
 *  the handshake can only be perfomed after the BC1.2 result is
 *  identified as DCP, such as the SFCP protocol.
 */
static void cm_enable_fixed_fchg_handshake(struct charger_manager *cm, bool enable)
{
	dev_dbg(cm->dev, "%s, %s fixed fchg handshake\n", __func__, enable ? "enable" : "disable");
	if (!cm->fchg_info || !cm->fchg_info->ops || !cm->fchg_info->ops->enable_fixed_fchg) {
		dev_err(cm->dev, "%s, fchg_info or ops or enable_fixed_fchg is null\n", __func__);
		return;
	}

	if (!cm->fchg_info->support_fchg)
		return;

	if (enable && !cm->desc->is_fast_charge &&
	    cm->desc->charger_type == CM_CHARGER_TYPE_DCP)
		cm->fchg_info->ops->enable_fixed_fchg(cm->fchg_info, true);
	else if (!enable)
		cm->fchg_info->ops->enable_fixed_fchg(cm->fchg_info, false);
}

static int cm_fast_enable_pps(struct charger_manager *cm, bool enable)
{
	int ret;

	dev_dbg(cm->dev, "%s, pps %s\n", __func__, enable ? "enable" : "disable");
	if (!cm->fchg_info->ops || !cm->fchg_info->ops->enable_dynamic_fchg) {
		dev_err(cm->dev, "%s, ops or enable_dynamic_fchg is null\n", __func__);
		return -EINVAL;
	}

	ret = cm->fchg_info->ops->enable_dynamic_fchg(cm->fchg_info, enable);
	if (ret)
		dev_err(cm->dev, "%s, failed to %s pps, ret=%d\n",
			__func__, enable ? "enable" : "disable", ret);

	return ret;
}

static bool cm_check_primary_charger_enabled(struct charger_manager *cm)
{
	int ret;
	bool enabled = false;
	union power_supply_propval val = {0,};
	struct power_supply *psy;

	psy = power_supply_get_by_name(cm->desc->psy_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find primary power supply \"%s\"\n",
			cm->desc->psy_charger_stat[0]);
		return false;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
	power_supply_put(psy);
	if (!ret) {
		if (val.intval)
			enabled = true;
	}

	dev_dbg(cm->dev, "%s: %s\n", __func__, enabled ? "enabled" : "disabled");
	return enabled;
}

static bool cm_check_cp_charger_enabled(struct charger_manager *cm)
{
	int ret;
	bool enabled = false;
	union power_supply_propval val = {0,};
	struct power_supply *cp_psy;

	if (!cm->desc->psy_cp_stat)
		return false;

	cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[0]);
	if (!cp_psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			cm->desc->psy_cp_stat[0]);
		return false;
	}

	ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
	power_supply_put(cp_psy);
	if (!ret)
		enabled = !!val.intval;

	dev_dbg(cm->dev, "%s: %s\n", __func__, enabled ? "enabled" : "disabled");

	return enabled;
}

static void cm_cp_clear_soft_alarm_status(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_info(cm->dev, "%s\n", __func__);
	cp->cp_soft_alarm_event = false;
	cp->alm.bat_ovp_alarm = false;
	cp->alm.bat_ocp_alarm = false;
	cp->alm.bus_ovp_alarm = false;
	cp->alm.bus_ocp_alarm = false;
	cp->alm.bat_ucp_alarm = false;
}

static void cm_cp_clear_fault_status(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_info(cm->dev, "%s\n", __func__);
	cp->cp_fault_event = false;
	cp->flt.bat_ovp_fault = false;
	cp->flt.bat_ocp_fault = false;
	cp->flt.bus_ovp_fault = false;
	cp->flt.bus_ocp_fault = false;
	cp->flt.bat_therm_fault = false;
	cp->flt.bus_therm_fault = false;
	cp->flt.die_therm_fault = false;

	cp->alm.bat_ovp_alarm = false;
	cp->alm.bat_ocp_alarm = false;
	cp->alm.bus_ovp_alarm = false;
	cp->alm.bus_ocp_alarm = false;
	cp->alm.bat_therm_alarm = false;
	cp->alm.bus_therm_alarm = false;
	cp->alm.die_therm_alarm = false;
	cp->alm.bat_ucp_alarm = false;

}

static void cm_check_cp_soft_monitor_alarm_status(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (!cm->desc->psy_cp_stat)
		return;

	psy = power_supply_get_by_name(cm->desc->psy_cp_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			cm->desc->psy_cp_stat[0]);
		return;
	}

	/*
	 *  If CP has an alarm register, return 0 without software monitoring.
	 *  Such as bq25970.
	 */
	val.intval = CM_SOFT_ALARM_HEALTH_CMD;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_HEALTH, &val);
	if (ret) {
		dev_err(cm->dev, "failed to get soft monitor alarm staus.\n");
		return;
	}

	if (!val.intval)
		return;

	cp->cp_soft_alarm_event = true;
	cp->alm.bat_ovp_alarm = !!(val.intval & CM_CHARGER_BAT_OVP_ALARM_MASK);
	cp->alm.bat_ocp_alarm = !!(val.intval & CM_CHARGER_BAT_OCP_ALARM_MASK);
	cp->alm.bus_ovp_alarm = !!(val.intval & CM_CHARGER_BUS_OVP_ALARM_MASK);
	cp->alm.bus_ocp_alarm = !!(val.intval & CM_CHARGER_BUS_OCP_ALARM_MASK);
	cp->alm.bat_ucp_alarm = !!(val.intval & CM_CHARGER_BAT_UCP_ALARM_MASK);

	dev_dbg(cm->dev, "%s, bat_ucp_alarm = %d, bat_ocp_alarm = %d, bat_ovp_alarm = %d,"
		" bus_ovp_alarm = %d, bus_ocp_alarm = %d\n",
		__func__, cp->alm.bat_ucp_alarm, cp->alm.bat_ocp_alarm, cp->alm.bat_ovp_alarm,
		cp->alm.bus_ovp_alarm, cp->alm.bus_ocp_alarm);
}

static void cm_check_cp_fault_status(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (!cm->desc->psy_cp_stat || !cm->desc->cm_check_int)
		return;

	dev_info(cm->dev, "%s\n", __func__);

	cm->desc->cm_check_int = false;
	cp->cp_fault_event = true;

	psy = power_supply_get_by_name(cm->desc->psy_cp_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			cm->desc->psy_cp_stat[0]);
		return;
	}

	val.intval = CM_FAULT_HEALTH_CMD;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_HEALTH, &val);
	if (!ret) {
		cp->flt.bat_ovp_fault = !!(val.intval & CM_CHARGER_BAT_OVP_FAULT_MASK);
		cp->flt.bat_ocp_fault = !!(val.intval & CM_CHARGER_BAT_OCP_FAULT_MASK);
		cp->flt.bus_ovp_fault = !!(val.intval & CM_CHARGER_BUS_OVP_FAULT_MASK);
		cp->flt.bus_ocp_fault = !!(val.intval & CM_CHARGER_BUS_OCP_FAULT_MASK);
		cp->flt.bat_therm_fault = !!(val.intval & CM_CHARGER_BAT_THERM_FAULT_MASK);
		cp->flt.bus_therm_fault = !!(val.intval & CM_CHARGER_BUS_THERM_FAULT_MASK);
		cp->flt.die_therm_fault = !!(val.intval & CM_CHARGER_DIE_THERM_FAULT_MASK);
		cp->alm.bat_ovp_alarm = !!(val.intval & CM_CHARGER_BAT_OVP_ALARM_MASK);
		cp->alm.bat_ocp_alarm = !!(val.intval & CM_CHARGER_BAT_OCP_ALARM_MASK);
		cp->alm.bus_ovp_alarm = !!(val.intval & CM_CHARGER_BUS_OVP_ALARM_MASK);
		cp->alm.bus_ocp_alarm = !!(val.intval & CM_CHARGER_BUS_OCP_ALARM_MASK);
		cp->alm.bat_therm_alarm = !!(val.intval & CM_CHARGER_BAT_THERM_ALARM_MASK);
		cp->alm.bus_therm_alarm = !!(val.intval & CM_CHARGER_BUS_THERM_ALARM_MASK);
		cp->alm.die_therm_alarm = !!(val.intval & CM_CHARGER_DIE_THERM_ALARM_MASK);
		cp->alm.bat_ucp_alarm = !!(val.intval & CM_CHARGER_BAT_UCP_ALARM_MASK);
	}
}

static void cm_update_cp_charger_status(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	cp->ibus_uA = 0;
	cp->vbat_uV = 0;
	cp->vbus_uV = 0;
	cp->ibat_uA = 0;

	if (cp->cp_running) {
		if (get_cp_ibus_uA(cm, &cp->ibus_uA)) {
			cp->ibus_uA = 0;
			dev_err(cm->dev, "get ibus current error.\n");
		}

		if (get_cp_vbat_uV(cm, &cp->vbat_uV)) {
			cp->vbat_uV = 0;
			dev_err(cm->dev, "get vbatt error.\n");
		}

		if (get_cp_vbus_uV(cm, &cp->vbus_uV)) {
			cp->vbat_uV = 0;
			dev_err(cm->dev, "get vbus error.\n");
		}

		if (get_cp_ibat_uA(cm, &cp->ibat_uA)) {
			cp->ibat_uA = 0;
			dev_err(cm->dev, "get vbatt error.\n");
		}

	} else {
		if (get_charger_input_current(cm, &cp->ibus_uA)) {
			cp->ibus_uA = 0;
			dev_err(cm->dev, "get ibus current error.\n");
		}

		if (get_vbat_now_uV(cm, &cp->vbat_uV)) {
			cp->vbat_uV = 0;
			dev_err(cm->dev, "get vbatt error.\n");
		}

		if (get_charger_voltage(cm, &cp->vbus_uV)) {
			cp->vbat_uV = 0;
			dev_err(cm->dev, "get vbus error.\n");
		}


		if (get_ibat_now_uA(cm, &cp->ibat_uA)) {
			cp->ibat_uA = 0;
			dev_err(cm->dev, "get vbatt error.\n");
		}
	}

	dev_dbg(cm->dev, " %s,  %s, batt_uV = %duV, vbus_uV = %duV, batt_uA = %duA, ibus_uA = %duA\n",
	       __func__, (cp->cp_running ? "charge pump" : "Primary charger"),
	       cp->vbat_uV, cp->vbus_uV, cp->ibat_uA, cp->ibus_uA);
}

static void cm_cp_check_vbus_status(struct charger_manager *cm)
{
	struct cm_fault_status *fault = &cm->desc->cp.flt;
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int ret;

	fault->vbus_error_lo = false;
	fault->vbus_error_hi = false;

	if (!cm->desc->psy_cp_stat || !cm->desc->cp.cp_running)
		return;

	cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[0]);
	if (!cp_psy) {
		dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[0]);
		return;
	}

	val.intval = CM_BUS_ERR_HEALTH_CMD;
	ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_HEALTH, &val);
	power_supply_put(cp_psy);
	if (ret) {
		dev_err(cm->dev, "failed to get vbus status, ret = %d\n", ret);
		return;
	}

	fault->vbus_error_lo = !!(val.intval & CM_CHARGER_BUS_ERR_LO_MASK);
	fault->vbus_error_hi = !!(val.intval & CM_CHARGER_BUS_ERR_HI_MASK);
}

static void cm_check_target_ibus(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int target_ibus;

	target_ibus = cp->cp_max_ibus;

	if (cp->adapter_max_ibus > 0)
		target_ibus = min(target_ibus, cp->adapter_max_ibus);

	if (cm->desc->thm_info.thm_adjust_cur > 0)
		target_ibus = min(target_ibus, cm->desc->thm_info.thm_adjust_cur);

	cp->cp_target_ibus = target_ibus;

	dev_dbg(cm->dev, "%s, adp_max_ibus = %d, cp_max_ibus = %d, thm_cur = %d, target_ibus = %d\n",
	       __func__, cp->adapter_max_ibus, cp->cp_max_ibus,
	       cm->desc->thm_info.thm_adjust_cur, cp->cp_target_ibus);
}

static void cm_check_target_vbus(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	if (cp->adapter_max_vbus > 0)
		cp->cp_target_vbus = min(cp->cp_target_vbus, cp->adapter_max_vbus);

	dev_dbg(cm->dev, "%s, adp_max_vbus = %d, target_vbus = %d\n",
	       __func__, cp->adapter_max_vbus, cp->cp_target_vbus);
}

static int cm_cp_vbat_step_algo(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int vbat_step = 0, delta_vbat_uV;

	delta_vbat_uV = cp->cp_target_vbat - cp->vbat_uV;

	if (cp->vbat_uV > 0 && delta_vbat_uV > CM_CP_VBAT_STEP1)
		vbat_step = CM_CP_VSTEP * 3;
	else if (cp->vbat_uV > 0 && delta_vbat_uV > CM_CP_VBAT_STEP2)
		vbat_step = CM_CP_VSTEP * 2;
	else if (cp->vbat_uV > 0 && delta_vbat_uV > CM_CP_VBAT_STEP3)
		vbat_step = CM_CP_VSTEP;
	else if (cp->vbat_uV > 0 && delta_vbat_uV < 0)
		vbat_step = -CM_CP_VSTEP * 2;

	return vbat_step;
}

static int cm_cp_ibat_step_algo(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int ibat_step = 0, delta_ibat_uA;

	delta_ibat_uA = cp->cp_target_ibat - cp->ibat_uA;

	if (cp->ibat_uA > 0 && delta_ibat_uA > CM_CP_IBAT_STEP1)
		ibat_step = CM_CP_VSTEP * 3;
	else if (cp->ibat_uA > 0 && delta_ibat_uA > CM_CP_IBAT_STEP2)
		ibat_step = CM_CP_VSTEP * 2;
	else if (cp->ibat_uA > 0 && delta_ibat_uA > CM_CP_IBAT_STEP3)
		ibat_step = CM_CP_VSTEP;
	else if (cp->ibat_uA > 0 && delta_ibat_uA < 0)
		ibat_step = -CM_CP_VSTEP * 2;

	return ibat_step;
}

static int cm_cp_vbus_step_algo(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int vbus_step = 0, delta_vbus_uV;

	delta_vbus_uV = cp->adapter_max_vbus - cp->vbus_uV;

	if (cp->vbus_uV > 0 && delta_vbus_uV > CM_CP_VBUS_STEP1)
		vbus_step = CM_CP_VSTEP * 3;
	else if (cp->vbus_uV > 0 && delta_vbus_uV > CM_CP_VBUS_STEP2)
		vbus_step = CM_CP_VSTEP * 2;
	else if (cp->vbus_uV > 0 && delta_vbus_uV > CM_CP_VBUS_STEP3)
		vbus_step = CM_CP_VSTEP;
	else if (cp->vbus_uV > 0 && delta_vbus_uV < 0)
		vbus_step = -CM_CP_VSTEP * 2;

	return vbus_step;
}

static int cm_cp_ibus_step_algo(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int ibus_step = 0, delta_ibus_uA;

	delta_ibus_uA = cp->cp_target_ibus - cp->ibus_uA;

	if (cp->ibus_uA > 0 && delta_ibus_uA > CM_CP_IBUS_STEP1)
		ibus_step = CM_CP_VSTEP * 3;
	else if (cp->ibus_uA > 0 && delta_ibus_uA > CM_CP_IBUS_STEP2)
		ibus_step = CM_CP_VSTEP * 2;
	else if (cp->ibus_uA > 0 && delta_ibus_uA > CM_CP_IBUS_STEP3)
		ibus_step = CM_CP_VSTEP;
	else if (cp->ibus_uA > 0 && delta_ibus_uA < 0)
		ibus_step = -CM_CP_VSTEP * 2;

	return ibus_step;
}

static bool cm_cp_tune_algo(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	int vbat_step = 0;
	int ibat_step = 0;
	int vbus_step = 0;
	int ibus_step = 0;
	int alarm_step = 0;
	bool is_taper_done = false;

	/* check taper done*/
	if (cp->vbat_uV >= cp->cp_target_vbat - 50000) {
		if (cp->ibat_uA < cp->cp_taper_current) {
			if (cp->cp_taper_trigger_cnt++ > 5) {
				is_taper_done = true;
				cp->cp_taper_trigger_cnt = 0;
				return is_taper_done;
			}
		} else {
			cp->cp_taper_trigger_cnt = 0;
		}
	}

	/* check battery voltage*/
	vbat_step = cm_cp_vbat_step_algo(cm);

	/* check battery current*/
	ibat_step = cm_cp_ibat_step_algo(cm);

	/* check bus voltage*/
	vbus_step = cm_cp_vbus_step_algo(cm);

	/* check bus current*/
	cm_check_target_ibus(cm);
	ibus_step = cm_cp_ibus_step_algo(cm);

	/* check alarm status*/
	if (cp->alm.bat_ovp_alarm || cp->alm.bat_ocp_alarm ||
	    cp->alm.bus_ovp_alarm || cp->alm.bus_ocp_alarm ||
	    cp->alm.bat_therm_alarm || cp->alm.bus_therm_alarm ||
	    cp->alm.die_therm_alarm) {
		dev_warn(cm->dev, "%s, bat_ovp_alarm = %d, bat_ocp_alarm = %d, bus_ovp_alarm = %d, "
			 "bus_ocp_alarm = %d, bat_therm_alarm = %d, bus_therm_alarm = %d, "
			 "die_therm_alarm = %d\n", __func__, cp->alm.bat_ovp_alarm,
			 cp->alm.bat_ocp_alarm, cp->alm.bus_ovp_alarm,
			 cp->alm.bus_ocp_alarm, cp->alm.bat_therm_alarm,
			 cp->alm.bus_therm_alarm, cp->alm.die_therm_alarm);
		if (cp->cp_soft_alarm_event)
			alarm_step = -CM_CP_VSTEP * 3;
		else
			alarm_step = -CM_CP_VSTEP * 2;
	} else {
		alarm_step = CM_CP_VSTEP * 3;
	}

	cp->cp_target_vbus += min(min(min(min(vbat_step, ibat_step),
					  vbus_step), ibus_step), alarm_step);
	cm_check_target_vbus(cm);

	dev_info(cm->dev, "%s vbatt = %duV, ibatt = %duA, vbus = %duV, ibus = %duA, "
		 "cp_target_vbat = %duV, cp_target_ibat = %duA, cp_target_vbus = %duV, "
		 "cp_target_ibus = %duA, cp_taper_current = %duA, taper_cnt = %d, "
		 "vbat_step = %d, ibat_step = %d, vbus_step = %d, ibus_step = %d, alarm_step = %d, "
		 "adapter_max_vbus = %duV, adapter_max_ibus = %duA, ucp_cnt = %d\n",
		 __func__, cp->vbat_uV, cp->ibat_uA, cp->vbus_uV, cp->ibus_uA,
		 cp->cp_target_vbat, cp->cp_target_ibat, cp->cp_target_vbus,
		 cp->cp_target_ibus, cp->cp_taper_current, cp->cp_taper_trigger_cnt,
		 vbat_step, ibat_step, vbus_step, ibus_step, alarm_step,
		 cp->adapter_max_vbus, cp->adapter_max_ibus, cp->cp_ibat_ucp_cnt);

	return is_taper_done;
}

static bool cm_cp_check_ibat_ucp_status(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	bool status = false;
	bool ibat_ucp_flag = false;

	if (cp->alm.bat_ucp_alarm) {
		dev_warn(cm->dev, "%s, bat_ucp_alarm = %d\n", __func__, cp->alm.bat_ucp_alarm);
		cp->cp_ibat_ucp_cnt++;
		ibat_ucp_flag = true;
	}

	if (!cp->cp_ibat_ucp_cnt)
		return status;

	if (cp->vbat_uV >= cp->cp_target_vbat - 50000) {
		cp->cp_ibat_ucp_cnt = 0;
		return status;
	}

	if (cp->ibat_uA < cp->cp_taper_current && !(ibat_ucp_flag))
		cp->cp_ibat_ucp_cnt++;
	else if (cp->ibat_uA >= cp->cp_taper_current)
		cp->cp_ibat_ucp_cnt = 0;

	if (cp->cp_ibat_ucp_cnt > CM_CP_IBAT_UCP_THRESHOLD)
		status = true;

	return status;
}

static void cm_cp_state_recovery(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
	       cp->cp_state, cm_cp_state_names[cp->cp_state]);

	if (is_ext_pwr_online(cm) && cm_is_reach_fchg_threshold(cm)) {
		cm_cp_state_change(cm, CM_CP_STATE_ENTRY);
	} else {
		cm->desc->cp.recovery = false;
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
	}
}

static void cm_cp_state_entry(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	static int primary_charger_dis_retry;

	dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
	       cp->cp_state, cm_cp_state_names[cp->cp_state]);

	cm->desc->cm_check_fault = false;
	cm_fast_enable_pps(cm, false);
	if (cm_fast_enable_pps(cm, true)) {
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		dev_err(cm->dev, "fail to enable pps\n");
		return;
	}

	cm_cp_master_charger_enable(cm, false);
	cm_primary_charger_enable(cm, false);
	cm_ir_compensation_enable(cm, false);

	if (cm_check_primary_charger_enabled(cm)) {
		if (primary_charger_dis_retry++ > CM_CP_PRIMARY_CHARGER_DIS_TIMEOUT) {
			cm_cp_state_change(cm, CM_CP_STATE_EXIT);
			primary_charger_dis_retry = 0;
		}
		return;
	}

	if (cm_get_adapter_max_voltage(cm, &cp->adapter_max_vbus)) {
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		return;
	}

	if (cm_get_adapter_max_current(cm, 0, &cp->adapter_max_ibus)) {
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		return;
	}

	/*
	 * The CM_PPS_5V_PROG_MAX reference value is derived from
	 * section 10.2.3.2 of the PD3.0 spec. The CP turn-on voltage
	 * is required to be greater than 2.05 times the battery
	 * voltage, and the battery voltage must be at least greater
	 * than 3.5V.
	 */
	if (cp->adapter_max_vbus <= CM_PPS_5V_PROG_MAX) {
		dev_info(cm->dev, "%s, APDO max_vol %d can't start the cp, exit pps!!!\n",
			 __func__, cp->adapter_max_vbus);
		cm->desc->force_pps_diasbled = true;
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		return;
	}

	cm_init_cp(cm);

	cp->recovery = false;
	cm->desc->enable_fast_charge = true;

	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

	cm_cp_master_charger_enable(cm, true);

	cm->desc->cp.tune_vbus_retry = 0;
	primary_charger_dis_retry = 0;
	cp->cp_ibat_ucp_cnt = 0;

	cp->cp_target_ibus = cp->cp_max_ibus;

	if (cp->vbat_uV <= CM_CP_ACC_VBAT_HTHRESHOLD)
		cp->cp_target_vbus =  CM_CP_VBUS_ERRORLO_THRESHOLD(cp->vbat_uV) + 10 * CM_CP_VSTEP;
	else
		cp->cp_target_vbus =  CM_CP_VBUS_ERRORLO_THRESHOLD(cp->vbat_uV) + 2 * CM_CP_VSTEP;

	dev_dbg(cm->dev, "%s, target_ibat = %d, cp_target_vbus = %d\n",
		 __func__, cp->cp_target_ibat, cp->cp_target_vbus);

	cm_check_target_vbus(cm);
	cm_adjust_fchg_voltage(cm, cp->cp_target_vbus);
	cp->cp_last_target_vbus = cp->cp_target_vbus;

	cm_check_target_ibus(cm);
	cm_adjust_fchg_current(cm, cp->cp_target_ibus);
	cm_cp_state_change(cm, CM_CP_STATE_CHECK_VBUS);
}

static void cm_cp_state_check_vbus(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
		 cp->cp_state, cm_cp_state_names[cp->cp_state]);

	if (cp->flt.vbus_error_lo &&
	    cp->vbus_uV <  CM_CP_VBUS_ERRORHI_THRESHOLD(cp->vbat_uV)) {
		cp->tune_vbus_retry++;
		cp->cp_target_vbus += 2 * CM_CP_VSTEP;
		cm_check_target_vbus(cm);

		if (cm_adjust_fchg_voltage(cm, cp->cp_target_vbus))
			cp->cp_target_vbus -= 2 * CM_CP_VSTEP;

	} else if (cp->flt.vbus_error_hi &&
		   cp->vbus_uV >  CM_CP_VBUS_ERRORLO_THRESHOLD(cp->vbat_uV)) {
		cp->tune_vbus_retry++;
		cp->cp_target_vbus -= CM_CP_VSTEP;
		if (cm_adjust_fchg_voltage(cm, cp->cp_target_vbus))
			dev_err(cm->dev, "fail to adjust pps voltage = %duV\n",
				cp->cp_target_vbus);
	} else {
		dev_info(cm->dev, "adapter volt tune ok, retry %d times\n",
			 cp->tune_vbus_retry);
		cm_cp_state_change(cm, CM_CP_STATE_TUNE);

		if (!cm_check_cp_charger_enabled(cm))
			cm_cp_master_charger_enable(cm, true);

		cm->desc->cm_check_fault = true;
		return;
	}

	dev_info(cm->dev, " %s, target_ibat = %duA, cp_target_vbus = %duV, vbus_err_lo = %d, "
		 "vbus_err_hi = %d, retry_time = %d",
		 __func__, cp->cp_target_ibat, cp->cp_target_vbus,
		 cp->flt.vbus_error_lo, cp->flt.vbus_error_hi, cp->tune_vbus_retry);

	if (cp->tune_vbus_retry >= 50) {
		dev_info(cm->dev, "Failed to tune adapter volt into valid range,move to CM_CP_STATE_EXIT\n");
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
	}
}

static void cm_cp_state_tune(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int target_vbat = 0;

	if (!cp->cp_state_tune_log) {
		dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
			 cp->cp_state, cm_cp_state_names[cp->cp_state]);
		cp->cp_state_tune_log = true;
	}

	cm_ir_compensation(cm, CM_IR_COMP_STATE_CP, &target_vbat);
	if (target_vbat > 0)
		cp->cp_target_vbat = target_vbat;

	if (cp->flt.bat_therm_fault || cp->flt.die_therm_fault ||
	    cp->flt.bus_therm_fault) {
		dev_err(cm->dev, "bat_therm_fault = %d, die_therm_fault = %d, exit cp\n",
			 cp->flt.bat_therm_fault, cp->flt.die_therm_fault);
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);

	} else if (cp->flt.bat_ocp_fault || cp->flt.bat_ovp_fault ||
		cp->flt.bus_ocp_fault || cp->flt.bus_ovp_fault) {
		dev_err(cm->dev, "bat_ocp_fault = %d, bat_ovp_fault = %d, "
			 "bus_ocp_fault = %d, bus_ovp_fault = %d, exit cp\n",
			 cp->flt.bat_ocp_fault, cp->flt.bat_ovp_fault,
			 cp->flt.bus_ocp_fault, cp->flt.bus_ovp_fault);
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);

	} else if (!cm_check_cp_charger_enabled(cm) &&
		   (cp->flt.vbus_error_hi || cp->flt.vbus_error_lo)) {
		dev_err(cm->dev, " %s some error happen, need recovery\n", __func__);
		cp->recovery = true;
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);

	} else if (!cm_check_cp_charger_enabled(cm)) {
		dev_err(cm->dev, "%s cp charger is disabled, exit cp\n", __func__);
		cp->recovery = true;
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
	} else if (cm_cp_check_ibat_ucp_status(cm)) {
		dev_err(cm->dev, "cp_ibat_ucp_cnt =%d, exit cp!\n", cp->cp_ibat_ucp_cnt);
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
	} else {
		dev_info(cm->dev, "cp is ok, fine tune\n");
		if (cm_cp_tune_algo(cm)) {
			dev_info(cm->dev, "taper done, exit cp machine\n");
			cm_cp_state_change(cm, CM_CP_STATE_EXIT);
			cp->recovery = false;
		} else {
			if (cp->cp_last_target_vbus != cp->cp_target_vbus) {
				cm_adjust_fchg_voltage(cm, cp->cp_target_vbus);
				cp->cp_last_target_vbus = cp->cp_target_vbus;
				cp->cp_adjust_cnt = 0;
			} else if (cp->cp_adjust_cnt++ > CM_CP_ADJUST_VOLTAGE_THRESHOLD) {
				cm_adjust_fchg_voltage(cm, cp->cp_target_vbus);
				cp->cp_adjust_cnt = 0;
			}
		}
	}

	if (cp->cp_soft_alarm_event)
		cm_cp_clear_soft_alarm_status(cm);

	if (cp->cp_fault_event)
		cm_cp_clear_fault_status(cm);
}

static void cm_cp_state_exit(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
		 cp->cp_state, cm_cp_state_names[cp->cp_state]);

	if (!cm_cp_master_charger_enable(cm, false))
		return;

	/* Hardreset will request 5V/2A or 5V/3A default.
	 * Disable pps will request sink-pdos PDO_FIXED value.
	 * And PDO_FIXED defined in dts is 5V/2A or 5V/3A, so
	 * we does not need requeset 5V/2A or 5V/3A when exit cp
	 */
	cm_fast_enable_pps(cm, false);

	if (!cp->recovery)
		cp->cp_running = false;

	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

	if (!cm->charging_status && !cm->emergency_stop) {
		cm_primary_charger_enable(cm, true);
		cm_ir_compensation_enable(cm, true);
	}

	if (cp->recovery)
		cm_cp_state_change(cm, CM_CP_STATE_RECOVERY);

	cm->desc->cm_check_fault = false;
	cm->desc->enable_fast_charge = false;
	cp->cp_soft_alarm_event = false;
	cp->cp_fault_event = false;
	cp->cp_ibat_ucp_cnt = 0;
	cp->cp_state_tune_log = false;
	cp->cp_taper_trigger_cnt = 0;
}

static int cm_cp_state_machine(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_dbg(cm->dev, "%s, state %d, %s\n", __func__,
	       cp->cp_state, cm_cp_state_names[cp->cp_state]);

	switch (cp->cp_state) {
	case CM_CP_STATE_RECOVERY:
		cm_cp_state_recovery(cm);
		break;
	case CM_CP_STATE_ENTRY:
		cm_cp_state_entry(cm);
		break;
	case CM_CP_STATE_CHECK_VBUS:
		cm_cp_state_check_vbus(cm);
		break;
	case CM_CP_STATE_TUNE:
		cm_cp_state_tune(cm);
		break;
	case CM_CP_STATE_EXIT:
		cm_cp_state_exit(cm);
		break;
	case CM_CP_STATE_UNKNOWN:
	default:
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		break;
	}

	return 0;
}

static void cm_cp_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
						  struct charger_manager,
						  cp_work);

	cm_update_cp_charger_status(cm);
	cm_cp_check_vbus_status(cm);
	cm_check_cp_soft_monitor_alarm_status(cm);

	if (cm->desc->cm_check_int && cm->desc->cm_check_fault)
		cm_check_cp_fault_status(cm);

	if (cm->desc->cp.cp_running && !cm_cp_state_machine(cm))
		schedule_delayed_work(&cm->cp_work, msecs_to_jiffies(CM_CP_WORK_TIME_MS));
}

static void cm_cp_control_switch(struct charger_manager *cm, bool enable)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_dbg(cm->dev, "%s enable = %d start\n", __func__, enable);

	if (!cm->desc->psy_cp_stat)
		return;

	if (enable) {
		cp->check_cp_threshold = enable;
	} else {
		cp->check_cp_threshold = enable;
		cp->recovery = false;
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		if (cp->cp_running) {
			cancel_delayed_work_sync(&cm->cp_work);
			cm_cp_state_machine(cm);
		}
		__pm_relax(cm->cp_ws);
	}
}

static bool cm_is_need_start_cp(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	bool need = false;
	int ret;

	if (!cm->desc->psy_cp_stat || cm->desc->cp.cp_running || cm->desc->force_pps_diasbled ||
	    cm->desc->fast_charger_type != CM_CHARGER_TYPE_ADAPTIVE)
		return false;

	/*
	 * Before starting the cp state machine, you need to turn
	 * off fixed_fchg. If the shutdown fails, the next charging
	 * cycle will be judged again.
	 */
	if (cm->desc->fixed_fchg_running) {
		cancel_delayed_work_sync(&cm->fixed_fchg_work);
		ret = cm_fixed_fchg_disable(cm);
		if (ret) {
			dev_err(cm->dev, "%s, failed to disable fixed fchg\n", __func__);
			return false;
		}
	}

	cm_charger_is_support_fchg(cm);
	dev_info(cm->dev, "%s, check_cp_threshold = %d, pps_running = %d, fast_charger_type = %d\n",
		 __func__, cp->check_cp_threshold, cp->cp_running, cm->desc->fast_charger_type);
	if (cp->check_cp_threshold && !cp->cp_running &&
	    cm_is_reach_fchg_threshold(cm) && cm->charger_enabled)
		need = true;

	return need;
}

static void cm_start_cp_state_machine(struct charger_manager *cm, bool start)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	if (!cp->cp_running && start) {
		dev_info(cm->dev, "%s, reach pps threshold\n", __func__);
		cp->cp_running = start;
		cm->desc->cm_check_fault = false;
		__pm_stay_awake(cm->cp_ws);
		cm_cp_state_change(cm, CM_CP_STATE_ENTRY);
		schedule_delayed_work(&cm->cp_work, 0);
	}
}

static int try_charger_enable_by_psy(struct charger_manager *cm, bool enable)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *psy;
	int i, err;

	for (i = 0; desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = enable;
		err = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS,
						&val);
		power_supply_put(psy);
		if (err)
			return err;
		if (desc->psy_charger_stat[1])
			break;
	}

	return 0;
}

static int try_wireless_charger_enable_by_psy(struct charger_manager *cm, bool enable)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *psy;
	int i, err;

	if (!cm->desc->psy_wl_charger_stat)
		return 0;

	for (i = 0; desc->psy_wl_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_wl_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_wl_charger_stat[i]);
			continue;
		}

		val.intval = enable;
		err = power_supply_set_property(psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
		power_supply_put(psy);
		if (err)
			return err;
	}

	return 0;
}

static int try_wireless_cp_converter_enable_by_psy(struct charger_manager *cm, bool enable)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *psy;
	int i, err;

	if (!cm->desc->psy_cp_converter_stat)
		return 0;

	for (i = 0; desc->psy_cp_converter_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_cp_converter_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = enable;
		err = power_supply_set_property(psy, POWER_SUPPLY_PROP_CALIBRATE, &val);
		power_supply_put(psy);
		if (err)
			return err;
	}

	return 0;
}

static int cm_set_primary_charge_wirless_type(struct charger_manager *cm, bool enable)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret = 0;

	psy = power_supply_get_by_name(cm->desc->psy_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			cm->desc->psy_charger_stat[0]);
		return false;
	}

	if (enable) {
		switch (cm->desc->charger_type) {
		case CM_WIRELESS_CHARGER_TYPE_BPP:
			val.intval = POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP;
			break;
		case CM_WIRELESS_CHARGER_TYPE_EPP:
			val.intval = POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP;
			break;
		default:
			val.intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		}
	} else {
		val.intval = 0;
	}

	dev_info(cm->dev, "set wirless type = %d\n", val.intval);
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_TYPE, &val);
	power_supply_put(psy);

	return ret;
}

static void try_wireless_charger_enable(struct charger_manager *cm, bool enable)
{
	int ret = 0;

	ret = cm_set_primary_charge_wirless_type(cm, enable);
	if (ret) {
		dev_err(cm->dev, "set wl type to primary charge fail, ret = %d\n", ret);
		return;
	}

	ret = try_wireless_charger_enable_by_psy(cm, enable);
	if (ret) {
		dev_err(cm->dev, "enable wl charger fail, ret = %d\n", ret);
		return;
	}

	ret = try_wireless_cp_converter_enable_by_psy(cm, enable);
	if (ret)
		dev_err(cm->dev, "enable wl charger fail, ret = %d\n", ret);
}

/**
 * try_charger_enable - Enable/Disable chargers altogether
 * @cm: the Charger Manager representing the battery.
 * @enable: true: enable / false: disable
 *
 * Note that Charger Manager keeps the charger enabled regardless whether
 * the charger is charging or not (because battery is full or no external
 * power source exists) except when CM needs to disable chargers forcibly
 * because of emergency causes; when the battery is overheated or too cold.
 */
static int try_charger_enable(struct charger_manager *cm, bool enable)
{
	int err = 0;

	/* Ignore if it's redundant command */
	if (enable == cm->charger_enabled)
		return 0;

	if (enable) {
		if (cm->emergency_stop)
			return -EAGAIN;

		/*
		 * Enable charge is permitted in calibration mode
		 * even if use fake battery.
		 * So it will not return in calibration mode.
		 */
		if (!is_batt_present(cm) && !allow_charger_enable)
			return 0;
		/*
		 * Save start time of charging to limit
		 * maximum possible charging time.
		 */
		cm->charging_start_time = ktime_to_ms(ktime_get());
		cm->charging_end_time = 0;

		err = try_charger_enable_by_psy(cm, enable);
		mutex_lock(&cm->desc->keep_awake_mtx);
		if (!err)
			cm->charger_enabled = enable;
		if (!err && cm->desc->keep_awake) {
			dev_info(cm->dev, "acquire charger_manager_wakelock when enable charge\n");
			__pm_stay_awake(cm->charge_ws);
		}
		mutex_unlock(&cm->desc->keep_awake_mtx);
		cm_ir_compensation_enable(cm, enable);
		cm_fixed_fchg_control_switch(cm, enable);
		cm_cp_control_switch(cm, enable);
	} else {
		/*
		 * Save end time of charging to maintain fully charged state
		 * of battery after full-batt.
		 */
		cm->charging_start_time = 0;
		cm->charging_end_time = ktime_to_ms(ktime_get());
		cm_cp_control_switch(cm, enable);
		cm_fixed_fchg_control_switch(cm, enable);
		cm_ir_compensation_enable(cm, enable);
		err = try_charger_enable_by_psy(cm, enable);
		mutex_lock(&cm->desc->keep_awake_mtx);
		if (!err)
			cm->charger_enabled = enable;
		if (!err && cm->desc->keep_awake) {
			dev_info(cm->dev, "Release charger_manager_wakelock when disable charge\n");
			__pm_relax(cm->charge_ws);
		}
		mutex_unlock(&cm->desc->keep_awake_mtx);
	}

	if (!err)
		power_supply_changed(cm->charger_psy);
	return err;
}

/**
 * try_charger_restart - Restart charging.
 * @cm: the Charger Manager representing the battery.
 *
 * Restart charging by turning off and on the charger.
 */
static int try_charger_restart(struct charger_manager *cm)
{
	int err;

	if (cm->emergency_stop)
		return -EAGAIN;

	err = try_charger_enable(cm, false);
	if (err)
		return err;

	return try_charger_enable(cm, true);
}

/**
 * fullbatt_vchk - Check voltage drop some times after "FULL" event.
 * @work: the work_struct appointing the function
 *
 * If a user has designated "fullbatt_vchkdrop_ms/uV" values with
 * charger_desc, Charger Manager checks voltage drop after the battery
 * "FULL" event. It checks whether the voltage has dropped more than
 * fullbatt_vchkdrop_uV by calling this function after fullbatt_vchkrop_ms.
 */
static void fullbatt_vchk(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
			struct charger_manager, fullbatt_vchk_work);
	struct charger_desc *desc = cm->desc;
	int batt_ocv, err, diff;

	/* remove the appointment for fullbatt_vchk */
	cm->fullbatt_vchk_jiffies_at = 0;

	if (!desc->fullbatt_vchkdrop_uV || !desc->fullbatt_vchkdrop_ms)
		return;

	err = get_batt_ocv(cm, &batt_ocv);
	if (err) {
		dev_err(cm->dev, "%s: get_batt_ocV error(%d)\n", __func__, err);
		return;
	}

	diff = desc->fullbatt_uV - batt_ocv;
	if (diff < 0)
		return;

	dev_info(cm->dev, "VBATT dropped %duV after full-batt\n", diff);

	if (diff >= desc->fullbatt_vchkdrop_uV)
		try_charger_restart(cm);

}

/**
 * check_charging_duration - Monitor charging/discharging duration
 * @cm: the Charger Manager representing the battery.
 *
 * If whole charging duration exceed 'charging_max_duration_ms',
 * cm stop charging to prevent overcharge/overheat. If discharging
 * duration exceed 'discharging _max_duration_ms', charger cable is
 * attached, after full-batt, cm start charging to maintain fully
 * charged state for battery.
 */
static void check_charging_duration(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	u64 curr = ktime_to_ms(ktime_get());
	u64 duration;
	int ret = false;

	if (!desc->charging_max_duration_ms && !desc->discharging_max_duration_ms)
		return;

	if (cm->charger_enabled) {
		int batt_ocv, diff;

		ret = get_batt_ocv(cm, &batt_ocv);
		if (ret) {
			dev_err(cm->dev, "failed to get battery OCV\n");
			return;
		}

		diff = desc->fullbatt_uV - batt_ocv;
		duration = curr - cm->charging_start_time;

		if (duration > desc->charging_max_duration_ms &&
		    diff < desc->fullbatt_vchkdrop_uV) {
			dev_info(cm->dev, "Charging duration exceed %ums\n",
				 desc->charging_max_duration_ms);
			cm->charging_status |= CM_CHARGE_DURATION_ABNORMAL;
			try_charger_enable(cm, false);
		}
	} else if (!cm->charger_enabled && (cm->charging_status & CM_CHARGE_DURATION_ABNORMAL)) {
		duration = curr - cm->charging_end_time;

		if (duration > desc->discharging_max_duration_ms) {
			dev_info(cm->dev, "Discharging duration exceed %ums\n",
				 desc->discharging_max_duration_ms);
			cm->charging_status &= ~CM_CHARGE_DURATION_ABNORMAL;
		}
	}

	return;
}

static int cm_get_battery_temperature_by_psy(struct charger_manager *cm, int *temp)
{
	struct power_supply *fuel_gauge;
	int ret;
	int64_t temp_val;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_TEMP,
				(union power_supply_propval *)&temp_val);
	power_supply_put(fuel_gauge);

	*temp = (int)temp_val;
	return ret;
}

static int cm_get_battery_temperature(struct charger_manager *cm, int *temp)
{
	int ret = 0;

	if (!cm->desc->measure_battery_temp)
		return -ENODEV;

#if IS_ENABLED(CONFIG_THERMAL)
	if (cm->tzd_batt) {
		ret = thermal_zone_get_temp(cm->tzd_batt, temp);
		if (!ret)
			/* Calibrate temperature unit */
			*temp /= 100;
	} else
#endif
	{
		/* if-else continued from CONFIG_THERMAL */
		*temp = cm->desc->temperature;
	}

	return ret;
}

static int cm_check_thermal_status(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	int temp, upper_limit, lower_limit;
	int ret = 0;

	ret = cm_get_battery_temperature(cm, &temp);
	if (ret) {
		/* FIXME:
		 * No information of battery temperature might
		 * occur hazardous result. We have to handle it
		 * depending on battery type.
		 */
		dev_err(cm->dev, "Failed to get battery temperature\n");
		return 0;
	}

	upper_limit = desc->temp_max;
	lower_limit = desc->temp_min;

	if (cm->emergency_stop) {
		upper_limit -= desc->temp_diff;
		lower_limit += desc->temp_diff;
	}

	if (temp > upper_limit)
		ret = CM_EVENT_BATT_OVERHEAT;
	else if (temp < lower_limit)
		ret = CM_EVENT_BATT_COLD;

	cm->emergency_stop = ret;

	return ret;
}

static void cm_check_charge_voltage(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	int ret, charge_vol;

	if (!desc->charge_voltage_max || !desc->charge_voltage_drop)
		return;

	mutex_lock(&cm->desc->charge_info_mtx);
	ret = get_charger_voltage(cm, &charge_vol);
	if (ret) {
		mutex_unlock(&cm->desc->charge_info_mtx);
		dev_warn(cm->dev, "Fail to get charge vol, ret = %d.\n", ret);
		return;
	}

	if (cm->charger_enabled && charge_vol > desc->charge_voltage_max) {
		dev_info(cm->dev, "Charging voltage %d is larger than %d\n",
			 charge_vol, desc->charge_voltage_max);
		cm->charging_status |= CM_CHARGE_VOLTAGE_ABNORMAL;
		mutex_unlock(&cm->desc->charge_info_mtx);
		try_charger_enable(cm, false);
	} else if (!cm->charger_enabled &&
		   charge_vol <= (desc->charge_voltage_max - desc->charge_voltage_drop) &&
		   (cm->charging_status & CM_CHARGE_VOLTAGE_ABNORMAL)) {
		dev_info(cm->dev, "Charging voltage %d less than %d, recharging\n",
			 charge_vol, desc->charge_voltage_max - desc->charge_voltage_drop);
		mutex_unlock(&cm->desc->charge_info_mtx);
		cm->charging_status &= ~CM_CHARGE_VOLTAGE_ABNORMAL;
	} else {
		mutex_unlock(&cm->desc->charge_info_mtx);
	}
}

static int cm_set_health_cmd(struct charger_manager *cm)
{
	int ret;
	union power_supply_propval val;
	struct power_supply *fuel_gauge;

	val.intval = CM_GOOD_HEALTH_CMD;
	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENOMEM;

	ret = power_supply_set_property(fuel_gauge, POWER_SUPPLY_PROP_HEALTH, &val);
	power_supply_put(fuel_gauge);

	return ret;
}

static void cm_check_battery_voltage(struct charger_manager *cm)
{
	int ret, batt_uV, health;
	union power_supply_propval val;
	struct power_supply *fuel_gauge;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_HEALTH, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return;

	health = val.intval;

	mutex_lock(&cm->desc->charge_info_mtx);
	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret) {
		dev_err(cm->dev, "%s, failed to get batt uV, ret=%d\n", __func__, ret);
		mutex_unlock(&cm->desc->charge_info_mtx);
		return;
	}

	if (cm->charger_enabled && (health == POWER_SUPPLY_HEALTH_OVERVOLTAGE)) {
		dev_info(cm->dev, "battery voltage too high, stop charge!!!\n");
		cm->charging_status |= CM_CHARGE_BATT_OVERVOLTAGE;
		mutex_unlock(&cm->desc->charge_info_mtx);
		try_charger_enable(cm, false);
	} else if (!cm->charger_enabled && batt_uV <= cm->desc->constant_charge_voltage_max_uv &&
		   (cm->charging_status & CM_CHARGE_BATT_OVERVOLTAGE)) {
			ret = cm_set_health_cmd(cm);
			if (ret) {
				dev_err(cm->dev, "failed to set health cmd, ret=%d\n", ret);
				mutex_unlock(&cm->desc->charge_info_mtx);
				return;
			}

			dev_info(cm->dev, "battery voltage %d less than %d, recharging\n", batt_uV,
				 cm->desc->constant_charge_voltage_max_uv);
			mutex_unlock(&cm->desc->charge_info_mtx);
			cm->charging_status &= ~CM_CHARGE_BATT_OVERVOLTAGE;
	} else {
		mutex_unlock(&cm->desc->charge_info_mtx);
	}
}

static void cm_check_charge_health(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int health = POWER_SUPPLY_HEALTH_UNKNOWN;
	int ret, i;

	for (i = 0; desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_HEALTH, &val);
		power_supply_put(psy);
		if (ret)
			return;
		health = val.intval;
	}

	if (health == POWER_SUPPLY_HEALTH_UNKNOWN)
		return;

	if (cm->charger_enabled && health != POWER_SUPPLY_HEALTH_GOOD) {
		dev_info(cm->dev, "Charging health is not good\n");
		cm->charging_status |= CM_CHARGE_HEALTH_ABNORMAL;
		try_charger_enable(cm, false);
	} else if (!cm->charger_enabled && health == POWER_SUPPLY_HEALTH_GOOD &&
		   (cm->charging_status & CM_CHARGE_HEALTH_ABNORMAL)) {
		dev_info(cm->dev, "Charging health is recover good\n");
		cm->charging_status &= ~CM_CHARGE_HEALTH_ABNORMAL;
	}
}

static bool cm_manager_adjust_current(struct charger_manager *cm, int jeita_status)
{
	struct charger_desc *desc = cm->desc;
	int term_volt, target_cur;

	if (jeita_status > desc->jeita_tab_size)
		jeita_status = desc->jeita_tab_size;

	if (jeita_status == 0 || jeita_status == desc->jeita_tab_size) {
		dev_warn(cm->dev,
			 "stop charging due to battery overheat or cold\n");

		if (jeita_status == 0) {
			cm->charging_status &= ~CM_CHARGE_TEMP_OVERHEAT;
			cm->charging_status |= CM_CHARGE_TEMP_COLD;
		} else {
			cm->charging_status &= ~CM_CHARGE_TEMP_COLD;
			cm->charging_status |= CM_CHARGE_TEMP_OVERHEAT;
		}
		return false;
	}

	term_volt = desc->jeita_tab[jeita_status].term_volt;
	target_cur = desc->jeita_tab[jeita_status].current_ua;

	cm->desc->ir_comp.us = term_volt;
	cm->desc->ir_comp.us_lower_limit = term_volt;

	if (cm->desc->cp.cp_running && !cm_check_primary_charger_enabled(cm)) {
		dev_info(cm->dev, "cp target terminate voltage = %d, target current = %d\n",
			 term_volt, target_cur);
		cm->desc->cp.cp_target_ibat = target_cur;
		goto exit;
	}

	dev_info(cm->dev, "target terminate voltage = %d, target current = %d\n",
		 term_volt, target_cur);

	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_IBAT,
				 SPRD_VOTE_TYPE_IBAT_ID_JEITA,
				 SPRD_VOTE_CMD_MIN,
				 target_cur, cm);
	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_CCCV,
				 SPRD_VOTE_TYPE_CCCV_ID_JEITA,
				 SPRD_VOTE_CMD_MIN,
				 term_volt, cm);

exit:
	cm->charging_status &= ~(CM_CHARGE_TEMP_OVERHEAT | CM_CHARGE_TEMP_COLD);
	return true;
}

static void cm_jeita_temp_goes_down(struct charger_desc *desc, int status,
				    int recovery_status, int *jeita_status)
{
	if (recovery_status == desc->jeita_tab_size) {
		if (*jeita_status >= recovery_status)
			*jeita_status = recovery_status;
		return;
	}

	if (desc->jeita_tab[recovery_status].temp > desc->jeita_tab[recovery_status].recovery_temp) {
		if (*jeita_status >= recovery_status)
			*jeita_status = recovery_status;
		return;
	}

	if (*jeita_status >= status)
		*jeita_status = status;
}

static void cm_jeita_temp_goes_up(struct charger_desc *desc, int status,
				  int recovery_status, int *jeita_status)
{
	if (recovery_status == desc->jeita_tab_size) {
		if (*jeita_status <= status)
			*jeita_status = status;
		return;
	}

	if (desc->jeita_tab[recovery_status].temp < desc->jeita_tab[recovery_status].recovery_temp) {
		if (*jeita_status <= recovery_status)
			*jeita_status = recovery_status;
		return;
	}

	if (*jeita_status <= status)
		*jeita_status = status;
}

static int cm_manager_get_jeita_status(struct charger_manager *cm, int cur_temp)
{
	struct charger_desc *desc = cm->desc;
	static int jeita_status, last_temp = -200;
	int i, temp_status, recovery_temp_status = -1;

	for (i = desc->jeita_tab_size - 1; i >= 0; i--) {
		if ((cur_temp >= desc->jeita_tab[i].temp && i > 0) ||
		    (cur_temp > desc->jeita_tab[i].temp && i == 0)) {
			break;
		}
	}

	temp_status = i + 1;

	if (temp_status == desc->jeita_tab_size) {
		jeita_status = desc->jeita_tab_size;
		recovery_temp_status = desc->jeita_tab_size;
		goto out;
	} else if (temp_status == 0) {
		jeita_status = 0;
		recovery_temp_status = 0;
		goto out;
	}

	for (i = desc->jeita_tab_size - 1; i >= 0; i--) {
		if ((cur_temp >= desc->jeita_tab[i].recovery_temp && i > 0) ||
		    (cur_temp > desc->jeita_tab[i].recovery_temp && i == 0)) {
			break;
		}
	}

	recovery_temp_status = i + 1;

	/* temperature goes down */
	if (last_temp > cur_temp)
		cm_jeita_temp_goes_down(desc, temp_status, recovery_temp_status, &jeita_status);
	/* temperature goes up */
	else
		cm_jeita_temp_goes_up(desc, temp_status, recovery_temp_status, &jeita_status);

out:
	last_temp = cur_temp;
	dev_info(cm->dev, "%s: jeita status:(%d) %d %d, temperature:%d, jeita_size:%d\n",
		 __func__, jeita_status, temp_status, recovery_temp_status,
		 cur_temp, desc->jeita_tab_size);

	return jeita_status;
}

static int cm_manager_jeita_current_monitor(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	static int last_jeita_status = -1, temp_up_trigger, temp_down_trigger;
	int cur_jeita_status;
	static bool is_normal = true;

	if (!desc->jeita_tab_size)
		return 0;

	if (!is_ext_pwr_online(cm)) {
		if (last_jeita_status != -1)
			last_jeita_status = -1;

		return 0;
	}

	if (desc->jeita_disabled) {
		if (last_jeita_status != cm->desc->force_jeita_status) {
			dev_info(cm->dev, "Disable jeita and force jeita state to force_jeita_status\n");
			last_jeita_status = cm->desc->force_jeita_status;
			desc->thm_info.thm_adjust_cur = -EINVAL;
			cm_manager_adjust_current(cm, last_jeita_status);
		}

		return 0;
	}

	cur_jeita_status = cm_manager_get_jeita_status(cm, desc->temperature);

	dev_info(cm->dev, "current-last jeita status: %d-%d, current temperature: %d\n",
		 cur_jeita_status, last_jeita_status, desc->temperature);

	/*
	 * We should give a initial jeita status with adjusting the charging
	 * current when pluging in the cabel.
	 */
	if (last_jeita_status == -1) {
		is_normal = cm_manager_adjust_current(cm, cur_jeita_status);
		last_jeita_status = cur_jeita_status;
		goto out;
	}

	if (cur_jeita_status > last_jeita_status) {
		temp_down_trigger = 0;

		if (++temp_up_trigger > 2) {
			is_normal = cm_manager_adjust_current(cm,
							      cur_jeita_status);
			last_jeita_status = cur_jeita_status;
		}
	} else if (cur_jeita_status < last_jeita_status) {
		temp_up_trigger = 0;

		if (++temp_down_trigger > 2) {
			is_normal = cm_manager_adjust_current(cm,
							      cur_jeita_status);
			last_jeita_status = cur_jeita_status;
		}
	} else {
		temp_up_trigger = 0;
		temp_down_trigger = 0;
	}

out:
	if (!is_normal)
		return -EAGAIN;

	return 0;
}

/**
 * cm_get_target_status - Check current status and get next target status.
 * @cm: the Charger Manager representing the battery.
 */
static int cm_get_target_status(struct charger_manager *cm)
{
	int ret;

	/*
	 * Adjust the charging current according to current battery
	 * temperature jeita table.
	 */
	ret = cm_manager_jeita_current_monitor(cm);
	if (ret)
		dev_warn(cm->dev, "Errors orrurs when adjusting charging current\n");

	if (!is_ext_pwr_online(cm) || (!is_batt_present(cm) && !allow_charger_enable))
		return POWER_SUPPLY_STATUS_DISCHARGING;

	if (cm_check_thermal_status(cm))
		return POWER_SUPPLY_STATUS_NOT_CHARGING;

	if (cm->charging_status & (CM_CHARGE_TEMP_OVERHEAT | CM_CHARGE_TEMP_COLD)) {
		dev_warn(cm->dev, "battery overheat or cold is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	cm_check_charge_health(cm);
	if (cm->charging_status & CM_CHARGE_HEALTH_ABNORMAL) {
		dev_warn(cm->dev, "Charging health is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	cm_check_charge_voltage(cm);
	if (cm->charging_status & CM_CHARGE_VOLTAGE_ABNORMAL) {
		dev_warn(cm->dev, "Charging voltage is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	cm_check_battery_voltage(cm);
	if (cm->charging_status & CM_CHARGE_BATT_OVERVOLTAGE) {
		dev_warn(cm->dev, "battery over voltage is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	check_charging_duration(cm);
	if (cm->charging_status & CM_CHARGE_DURATION_ABNORMAL) {
		dev_warn(cm->dev, "Charging duration is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	if (is_full_charged(cm))
		return POWER_SUPPLY_STATUS_FULL;
	/* Charging is allowed. */
	return POWER_SUPPLY_STATUS_CHARGING;
}

/**
 * _cm_monitor - Monitor the temperature and return true for exceptions.
 * @cm: the Charger Manager representing the battery.
 *
 * Returns true if there is an event to notify for the battery.
 * (True if the status of "emergency_stop" changes)
 */
static bool _cm_monitor(struct charger_manager *cm)
{
	int i, target;
	static int last_target = -1;

	for (i = 0; i < cm->desc->num_sysfs; i++) {
		if (cm->desc->sysfs[i].externally_control) {
			dev_info(cm->dev, "Charger has been controlled externally, so no need monitoring\n");
			last_target = -1;
			return false;
		}
	}

	target = cm_get_target_status(cm);

	if (target == POWER_SUPPLY_STATUS_CHARGING) {
		cm->emergency_stop = 0;
		cm->charging_status = 0;
		try_charger_enable(cm, true);

		if (!cm->desc->cp.cp_running && !cm_check_primary_charger_enabled(cm)
		    && !cm->desc->force_set_full) {
			dev_info(cm->dev, "%s, primary charger does not enable,enable it\n", __func__);
			cm_primary_charger_enable(cm, true);
		}

		if (cm_is_need_start_cp(cm))
			cm_start_cp_state_machine(cm, true);
		else if (!cm->desc->cp.cp_running && cm_is_need_start_fixed_fchg(cm))
			cm_start_fixed_fchg(cm, true);
	} else {
		try_charger_enable(cm, false);
	}

	if (last_target != target) {
		last_target = target;
		power_supply_changed(cm->charger_psy);
	}

	dev_info(cm->dev, "target %d, charging_status %d\n", target, cm->charging_status);
	return (target == POWER_SUPPLY_STATUS_NOT_CHARGING);

}

/**
 * cm_monitor - Monitor every battery.
 *
 * Returns true if there is an event to notify from any of the batteries.
 * (True if the status of "emergency_stop" changes)
 */
static bool cm_monitor(void)
{
	bool stop = false;
	struct charger_manager *cm;

	mutex_lock(&cm_list_mtx);

	list_for_each_entry(cm, &cm_list, entry) {
		if (_cm_monitor(cm))
			stop = true;
	}

	mutex_unlock(&cm_list_mtx);

	return stop;
}

/**
 * _setup_polling - Setup the next instance of polling.
 * @work: work_struct of the function _setup_polling.
 */
static void _setup_polling(struct work_struct *work)
{
	unsigned long min = ULONG_MAX;
	struct charger_manager *cm;
	bool keep_polling = false;
	unsigned long _next_polling;

	mutex_lock(&cm_list_mtx);

	list_for_each_entry(cm, &cm_list, entry) {
		if (is_polling_required(cm) && cm->desc->polling_interval_ms) {
			keep_polling = true;

			if (min > cm->desc->polling_interval_ms)
				min = cm->desc->polling_interval_ms;
		}
	}

	polling_jiffy = msecs_to_jiffies(min);
	if (polling_jiffy <= CM_JIFFIES_SMALL)
		polling_jiffy = CM_JIFFIES_SMALL + 1;

	if (!keep_polling)
		polling_jiffy = ULONG_MAX;
	if (polling_jiffy == ULONG_MAX)
		goto out;

	WARN(cm_wq == NULL, "charger-manager: workqueue not initialized"
			    ". try it later. %s\n", __func__);

	/*
	 * Use mod_delayed_work() iff the next polling interval should
	 * occur before the currently scheduled one.  If @cm_monitor_work
	 * isn't active, the end result is the same, so no need to worry
	 * about stale @next_polling.
	 */
	_next_polling = jiffies + polling_jiffy;

	if (time_before(_next_polling, next_polling)) {
		mod_delayed_work(cm_wq, &cm_monitor_work, polling_jiffy);
		next_polling = _next_polling;
	} else {
		if (queue_delayed_work(cm_wq, &cm_monitor_work, polling_jiffy))
			next_polling = _next_polling;
	}
out:
	mutex_unlock(&cm_list_mtx);
}
static DECLARE_WORK(setup_polling, _setup_polling);

/**
 * cm_monitor_poller - The Monitor / Poller.
 * @work: work_struct of the function cm_monitor_poller
 *
 * During non-suspended state, cm_monitor_poller is used to poll and monitor
 * the batteries.
 */
static void cm_monitor_poller(struct work_struct *work)
{
	cm_monitor();
	schedule_work(&setup_polling);
}

/**
 * fullbatt_handler - Event handler for CM_EVENT_BATT_FULL
 * @cm: the Charger Manager representing the battery.
 */
static void fullbatt_handler(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;

	if (!desc->fullbatt_vchkdrop_uV || !desc->fullbatt_vchkdrop_ms)
		goto out;

	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	mod_delayed_work(cm_wq, &cm->fullbatt_vchk_work,
			 msecs_to_jiffies(desc->fullbatt_vchkdrop_ms));
	cm->fullbatt_vchk_jiffies_at = jiffies + msecs_to_jiffies(
				       desc->fullbatt_vchkdrop_ms);

	if (cm->fullbatt_vchk_jiffies_at == 0)
		cm->fullbatt_vchk_jiffies_at = 1;

out:
	dev_info(cm->dev, "EVENT_HANDLE: Battery Fully Charged\n");
}

/**
 * battout_handler - Event handler for CM_EVENT_BATT_OUT
 * @cm: the Charger Manager representing the battery.
 */
static void battout_handler(struct charger_manager *cm)
{
	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	if (!is_batt_present(cm)) {
		dev_emerg(cm->dev, "Battery Pulled Out!\n");
		try_charger_enable(cm, false);
	} else {
		dev_emerg(cm->dev, "Battery Pulled in!\n");

		if (cm->charging_status) {
			dev_emerg(cm->dev, "Charger status abnormal, stop charge!\n");
			try_charger_enable(cm, false);
		} else {
			try_charger_enable(cm, true);
		}
	}
}

static bool cm_charger_is_support_fchg(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	u32 fchg_type;
	int ret;

	if (!cm->fchg_info->support_fchg || !cm->fchg_info->ops ||
	    !cm->fchg_info->ops->get_fchg_type)
		return false;

	ret = cm->fchg_info->ops->get_fchg_type(cm->fchg_info, &fchg_type);
	if (!ret) {
		if (fchg_type == POWER_SUPPLY_CHARGE_TYPE_FAST ||
		    fchg_type == POWER_SUPPLY_CHARGE_TYPE_ADAPTIVE) {
			mutex_lock(&cm->desc->charger_type_mtx);
			desc->is_fast_charge = true;
			if (!desc->psy_cp_stat &&
			    fchg_type == POWER_SUPPLY_CHARGE_TYPE_ADAPTIVE)
				fchg_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
			cm_get_charger_type(cm, CM_FCHG_TYPE, &fchg_type);
			desc->fast_charger_type = fchg_type;
			desc->charger_type = fchg_type;
			mutex_unlock(&cm->desc->charger_type_mtx);
			return true;
		}
	}

	return false;
}

static void cm_charger_int_handler(struct charger_manager *cm)
{
	dev_info(cm->dev, "%s\n", __func__);
	cm->desc->cm_check_int = true;
}


/**
 * fast_charge_handler - Event handler for CM_EVENT_FAST_CHARGE
 * @cm: the Charger Manager representing the battery.
 */
static void fast_charge_handler(struct charger_manager *cm)
{
	bool ext_pwr_online;

	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	cm_charger_is_support_fchg(cm);
	ext_pwr_online = is_ext_pwr_online(cm);

	dev_info(cm->dev, "%s, fast_charger_type = %d, cp_running = %d, "
		 "charger_enabled = %d, ext_pwr_online = %d\n",
		 __func__, cm->desc->fast_charger_type, cm->desc->cp.cp_running,
		 cm->charger_enabled, ext_pwr_online);

	if (!ext_pwr_online)
		return;

	if (cm->desc->is_fast_charge && !cm->desc->enable_fast_charge)
		cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
					   CM_CHARGE_INFO_INPUT_LIMIT));

	/*
	 * Once the fast charge is identified, it is necessary to open
	 * the charge in the first time to avoid the fast charge to boost
	 * the voltage in the next charging cycle, especially the SFCP
	 * fast charge.
	 */
	if (cm->desc->fast_charger_type == CM_CHARGER_TYPE_FAST &&
	    cm->charger_enabled)
		mod_delayed_work(cm_wq, &cm_monitor_work, 0);

	if (cm->desc->fast_charger_type == CM_CHARGER_TYPE_ADAPTIVE &&
	    !cm->desc->cp.cp_running && cm->charger_enabled) {
		cm_cp_control_switch(cm, true);
		mod_delayed_work(cm_wq, &cm_monitor_work, 0);
	}
}

/**
 * misc_event_handler - Handler for other events
 * @cm: the Charger Manager representing the battery.
 * @type: the Charger Manager representing the battery.
 */
static void misc_event_handler(struct charger_manager *cm, enum cm_event_types type)
{
	int ret;

	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	if (is_ext_pwr_online(cm)) {
		cm_set_charger_present(cm, true);
		if (is_ext_usb_pwr_online(cm) && type == CM_EVENT_WL_CHG_START_STOP) {
			dev_warn(cm->dev, "usb charging, does not need start wl charge\n");
			return;
		} else if (is_ext_usb_pwr_online(cm)) {
			if (cm->desc->wl_charge_en) {
				try_wireless_charger_enable(cm, false);
				try_charger_enable(cm, false);
				cm->desc->wl_charge_en = false;
			}

			if (!cm->desc->is_fast_charge) {
				ret = get_usb_charger_type(cm, &cm->desc->charger_type);
				if (ret)
					dev_warn(cm->dev, "Fail to get usb charger type, ret = %d", ret);

				cm_enable_fixed_fchg_handshake(cm, true);
			}

			cm->desc->usb_charge_en = true;
		} else {
			if (cm->desc->usb_charge_en) {
				cm_enable_fixed_fchg_handshake(cm, false);
				try_charger_enable(cm, false);
				cm->desc->force_pps_diasbled = false;
				cm->desc->is_fast_charge = false;
				cm->desc->enable_fast_charge = false;
				cm->desc->fast_charge_enable_count = 0;
				cm->desc->fast_charge_disable_count = 0;
				cm->desc->fixed_fchg_running = false;
				cm->desc->wait_vbus_stable = false;
				cm->desc->cp.cp_running = false;
				cm->desc->fast_charger_type = 0;
				cm->desc->cp.cp_target_vbus = 0;
				cm->desc->usb_charge_en = false;
				cm->desc->charger_type = 0;
			}

			ret = get_wireless_charger_type(cm, &cm->desc->charger_type);
			if (ret)
				dev_warn(cm->dev, "Fail to get wl charger type, ret = %d\n", ret);

			try_wireless_charger_enable(cm, true);
			cm->desc->wl_charge_en = true;
		}

		cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
					   CM_CHARGE_INFO_INPUT_LIMIT |
					   CM_CHARGE_INFO_JEITA_LIMIT));
	} else {
		try_wireless_charger_enable(cm, false);
		cm_enable_fixed_fchg_handshake(cm, false);
		try_charger_enable(cm, false);
		cm_set_charger_present(cm, false);
		cancel_delayed_work_sync(&cm_monitor_work);
		cancel_delayed_work_sync(&cm->cp_work);
		_cm_monitor(cm);

		cm->desc->force_pps_diasbled = false;
		cm->desc->is_fast_charge = false;
		cm->desc->ir_comp.ir_compensation_en = false;
		cm->desc->enable_fast_charge = false;
		cm->desc->fast_charge_enable_count = 0;
		cm->desc->fast_charge_disable_count = 0;
		cm->desc->fixed_fchg_running = false;
		cm->desc->wait_vbus_stable = false;
		cm->desc->cp.cp_running = false;
		cm->desc->cm_check_int = false;
		cm->desc->fast_charger_type = 0;
		cm->desc->charger_type = 0;
		cm->desc->cp.cp_target_vbus = 0;
		cm->desc->force_set_full = false;
		cm->emergency_stop = 0;
		cm->charging_status = 0;
		cm->desc->thm_info.thm_adjust_cur = -EINVAL;
		cm->desc->thm_info.thm_pwr = 0;
		cm->desc->thm_info.adapter_default_charge_vol = 5;
		cm->desc->wl_charge_en = 0;
		cm->desc->usb_charge_en = 0;
		cm->cm_charge_vote->vote(cm->cm_charge_vote, false,
					 SPRD_VOTE_TYPE_ALL, 0, 0, 0, cm);
	}

	cm_update_charger_type_status(cm);

	if (is_polling_required(cm) && cm->desc->polling_interval_ms)
		mod_delayed_work(cm_wq, &cm_monitor_work, 0);

	power_supply_changed(cm->charger_psy);
}

static void cm_get_charging_status(struct charger_manager *cm, int *status)
{
	if (is_charging(cm)) {
		cm->battery_status = POWER_SUPPLY_STATUS_CHARGING;
	} else if (is_ext_pwr_online(cm)) {
		if (is_full_charged(cm))
			cm->battery_status = POWER_SUPPLY_STATUS_FULL;
		else
			cm->battery_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	} else {
		cm->battery_status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	*status = cm->battery_status;
}

static void cm_get_charging_health_status(struct charger_manager *cm, int *status)
{
	if (cm->emergency_stop == CM_EVENT_BATT_OVERHEAT ||
	    (cm->charging_status & CM_CHARGE_TEMP_OVERHEAT))
		*status = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (cm->emergency_stop == CM_EVENT_BATT_COLD ||
		 (cm->charging_status & CM_CHARGE_TEMP_COLD))
		*status = POWER_SUPPLY_HEALTH_COLD;
	else if (cm->charging_status & (CM_CHARGE_VOLTAGE_ABNORMAL | CM_CHARGE_BATT_OVERVOLTAGE))
		*status = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	else
		*status = POWER_SUPPLY_HEALTH_GOOD;
}

static int cm_get_battery_technology(struct charger_manager *cm, union power_supply_propval *val)
{
	struct power_supply *fuel_gauge = NULL;
	int ret;

	val->intval = POWER_SUPPLY_TECHNOLOGY_UNKNOWN;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_TECHNOLOGY, val);
	power_supply_put(fuel_gauge);

	return ret;
}

static void cm_get_uisoc(struct charger_manager *cm, int *uisoc)
{
	if (!is_batt_present(cm)) {
		/* There is no battery. Assume 100% */
		*uisoc = 100;
		return;
	}

	*uisoc = DIV_ROUND_CLOSEST(cm->desc->cap, 10);
	if (*uisoc > 100)
		*uisoc = 100;
	else if (*uisoc < 0)
		*uisoc = 0;
}

static int cm_get_capacity_level(struct charger_manager *cm)
{
	int level = POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
	int uisoc, ocv_uv = 0, batt_uv = 0;

	if (!is_batt_present(cm)) {
		/* There is no battery. Assume 100% */
		level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		return level;
	}

	uisoc = DIV_ROUND_CLOSEST(cm->desc->cap, 10);

	if (uisoc >= CM_CAPACITY_LEVEL_FULL)
		level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (uisoc > CM_CAPACITY_LEVEL_NORMAL)
		level = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	else if (uisoc > CM_CAPACITY_LEVEL_LOW)
		level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	else if (uisoc > CM_CAPACITY_LEVEL_CRITICAL)
		level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else
		level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;


	if (get_batt_ocv(cm, &ocv_uv)) {
		dev_err(cm->dev, "%s, get_batt_ocV error.\n", __func__);
		return level;
	}

	if (get_vbat_now_uV(cm, &batt_uv)) {
		dev_err(cm->dev, "%s, get_batt_uv error.\n", __func__);
		return level;
	}

	if (level == POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL && is_charging(cm) &&
	    ocv_uv > CM_CAPACITY_LEVEL_CRITICAL_VOLTAGE &&
	    batt_uv > CM_LOW_CAP_SHUTDOWN_VOLTAGE_THRESHOLD)
		level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;

	return level;
}

static int cm_get_charge_full_design(struct charger_manager *cm, union power_supply_propval *val)
{
	struct power_supply *fuel_gauge = NULL;
	int ret;

	val->intval = 0;
	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN, val);
	power_supply_put(fuel_gauge);

	return ret;
}

static int cm_get_charge_now(struct charger_manager *cm, int *charge_now)
{
	int total_uah;
	int ret;

	ret = get_batt_total_cap(cm, &total_uah);
	if (ret) {
		dev_err(cm->dev, "failed to get total uah.\n");
		return ret;
	}

	*charge_now = total_uah * cm->desc->cap / CM_CAP_FULL_PERCENT;

	return ret;
}

static int cm_get_charge_counter(struct charger_manager *cm, int *charge_counter)
{
	int ret;

	*charge_counter = 0;
	ret = cm_get_charge_now(cm, charge_counter);

	if (*charge_counter <= 0) {
		*charge_counter = 1;
		ret = 0;
	}

	return ret;
}

static int cm_get_charge_control_limit(struct charger_manager *cm,
				       union power_supply_propval *val)
{
	struct power_supply *psy = NULL;
	int i, ret = 0;

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, val);
		power_supply_put(psy);
		if (!ret) {
			if (cm->desc->enable_fast_charge && cm->desc->psy_charger_stat[1])
				val->intval *= 2;

			break;
		}

		ret = power_supply_get_property(psy,
						POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
						val);
		if (!ret)
			break;
	}

	return ret;
}

static int cm_get_charge_full_uah(struct charger_manager *cm, union power_supply_propval *val)
{
	return cm_get_charge_full_design(cm, val);
}

static int cm_get_time_to_full_now(struct charger_manager *cm, int *time)
{
	unsigned int total_cap = 0;
	int chg_cur = 0;
	int ret;

	ret = get_constant_charge_current(cm, &chg_cur);
	if (ret) {
		dev_err(cm->dev, "get chg_cur error.\n");
		return ret;
	}

	chg_cur = chg_cur / 1000;

	ret = get_batt_total_cap(cm, &total_cap);
	if (ret) {
		dev_err(cm->dev, "failed to get total cap.\n");
		return ret;
	}

	total_cap = total_cap / 1000;

	*time = ((1000 - cm->desc->cap) * total_cap / 1000) * 3600 / chg_cur;

	if (*time <= 0)
		*time = 1;

	return ret;
}

static void cm_get_voltage_max(struct charger_manager *cm, int *voltage_max)
{
	int adapter_max_vbus = CM_FAST_CHARGE_VOLTAGE_5V, chg_type_max_vbus = 0;
	int ret = 0;

	if (!is_ext_pwr_online(cm)) {
		*voltage_max = min(chg_type_max_vbus, adapter_max_vbus);
		return;
	}

	switch (cm->desc->charger_type) {
	case CM_CHARGER_TYPE_FAST:
		if (!cm->desc->fast_charge_voltage_max) {
			chg_type_max_vbus = CM_FAST_CHARGE_VOLTAGE_5V;
			break;
		}

		if (cm->desc->fast_charge_voltage_max > CM_FAST_CHARGE_VOLTAGE_20V)
			chg_type_max_vbus = CM_FAST_CHARGE_VOLTAGE_20V;
		else if (cm->desc->fast_charge_voltage_max > CM_FAST_CHARGE_VOLTAGE_15V)
			chg_type_max_vbus = CM_FAST_CHARGE_VOLTAGE_15V;
		else if (cm->desc->fast_charge_voltage_max > CM_FAST_CHARGE_VOLTAGE_12V)
			chg_type_max_vbus = CM_FAST_CHARGE_VOLTAGE_12V;
		else if (cm->desc->fast_charge_voltage_max > CM_FAST_CHARGE_VOLTAGE_9V)
			chg_type_max_vbus = CM_FAST_CHARGE_VOLTAGE_9V;

		ret = cm_get_adapter_max_voltage(cm, &adapter_max_vbus);
		if (ret) {
			adapter_max_vbus = CM_FAST_CHARGE_VOLTAGE_5V;
			dev_err(cm->dev,
				"%s, failed to obtain the adapter max_vol in fixed fchg\n",
				__func__);
		}
		break;
	case CM_CHARGER_TYPE_ADAPTIVE:
		if (!cm->desc->flash_charge_voltage_max) {
			chg_type_max_vbus = CM_FAST_CHARGE_VOLTAGE_5V;
			break;
		}

		if (cm->desc->flash_charge_voltage_max > CM_FAST_CHARGE_VOLTAGE_20V)
			chg_type_max_vbus = CM_PPS_VOLTAGE_21V;
		else if (cm->desc->flash_charge_voltage_max > CM_FAST_CHARGE_VOLTAGE_15V)
			chg_type_max_vbus = CM_PPS_VOLTAGE_16V;
		else if (cm->desc->flash_charge_voltage_max > CM_FAST_CHARGE_VOLTAGE_9V)
			chg_type_max_vbus = CM_PPS_VOLTAGE_11V;

		ret = cm_get_adapter_max_voltage(cm, &adapter_max_vbus);
		if (ret) {
			adapter_max_vbus = CM_FAST_CHARGE_VOLTAGE_5V;
			dev_err(cm->dev,
				"%s, failed to obtain the adapter max_vol in pps\n",
				__func__);
			break;
		}

		if (cm->desc->charger_type == CM_CHARGER_TYPE_ADAPTIVE &&
		    cm->desc->force_pps_diasbled)
			adapter_max_vbus = CM_FAST_CHARGE_VOLTAGE_5V;
		break;
	case CM_WIRELESS_CHARGER_TYPE_EPP:
		if (!cm->desc->wireless_fast_charge_voltage_max) {
			chg_type_max_vbus = CM_FAST_CHARGE_VOLTAGE_5V;
			break;
		}

		chg_type_max_vbus = cm->desc->wireless_fast_charge_voltage_max;
		break;
	case CM_CHARGER_TYPE_DCP:
	case CM_CHARGER_TYPE_CDP:
	case CM_CHARGER_TYPE_SDP:
	case CM_CHARGER_TYPE_UNKNOWN:
	case CM_WIRELESS_CHARGER_TYPE_BPP:
	default:
		chg_type_max_vbus = CM_FAST_CHARGE_VOLTAGE_5V;
		break;
	}

	*voltage_max = min(chg_type_max_vbus, adapter_max_vbus);
}

static void cm_get_current_max(struct charger_manager *cm, int *current_max)
{
	int adapter_max_ibus = CM_FAST_CHARGE_CURRENT_2A, chg_type_max_ibus = 0;
	int opt_max_vbus;
	int ret = 0;

	if (!is_ext_pwr_online(cm)) {
		*current_max = min(chg_type_max_ibus, adapter_max_ibus);
		return;
	}

	switch (cm->desc->charger_type) {
	case CM_CHARGER_TYPE_DCP:
		chg_type_max_ibus = cm->desc->cur.dcp_limit;
		break;
	case CM_CHARGER_TYPE_SDP:
		chg_type_max_ibus = cm->desc->cur.sdp_limit;
		break;
	case CM_CHARGER_TYPE_CDP:
		chg_type_max_ibus = cm->desc->cur.cdp_limit;
		break;
	case CM_CHARGER_TYPE_FAST:
		chg_type_max_ibus = cm->desc->cur.fchg_limit;
		cm_get_voltage_max(cm, &opt_max_vbus);
		ret = cm_get_adapter_max_current(cm, opt_max_vbus, &adapter_max_ibus);
		if (ret) {
			adapter_max_ibus = CM_FAST_CHARGE_CURRENT_2A;
			dev_err(cm->dev,
				"%s, failed to obtain the adapter max_cur in fixed fchg\n",
				__func__);
		}
		break;
	case CM_CHARGER_TYPE_ADAPTIVE:
		chg_type_max_ibus = cm->desc->cur.flash_limit;
		cm_get_voltage_max(cm, &opt_max_vbus);
		ret = cm_get_adapter_max_current(cm, opt_max_vbus, &adapter_max_ibus);
		if (ret) {
			adapter_max_ibus = CM_FAST_CHARGE_CURRENT_2A;
			dev_err(cm->dev,
				"%s, failed to obtain the adapter max_cur in pps\n", __func__);
			break;
		}

		if (cm->desc->charger_type == CM_CHARGER_TYPE_ADAPTIVE &&
		    cm->desc->force_pps_diasbled)
			adapter_max_ibus = CM_FAST_CHARGE_CURRENT_2A;
		break;
	case CM_WIRELESS_CHARGER_TYPE_BPP:
		chg_type_max_ibus = cm->desc->cur.wl_bpp_limit;
		break;
	case CM_WIRELESS_CHARGER_TYPE_EPP:
		chg_type_max_ibus = cm->desc->cur.wl_epp_limit;
		break;
	case CM_CHARGER_TYPE_UNKNOWN:
	default:
		chg_type_max_ibus = cm->desc->cur.unknown_limit;
		break;
	}

	*current_max = min(chg_type_max_ibus, adapter_max_ibus);
}

static void cm_set_charge_control_limit(struct charger_manager *cm, int power)
{
	dev_info(cm->dev, "thermal set charge power limit, thm_pwr = %dmW\n", power);
	cm->desc->thm_info.thm_pwr = power;
	cm_update_charge_info(cm, CM_CHARGE_INFO_THERMAL_LIMIT);

	if (cm->desc->cp.cp_running)
		cm_check_target_ibus(cm);
}

static int cm_set_voltage_max_design(struct charger_manager *cm, int voltage_max)
{
	int ret;

	ret = cm_set_basp_max_volt(cm, voltage_max);
	if (ret)
		return ret;

	if (cm_init_basp_parameter(cm)) {
		if (cm->cm_charge_vote && cm->cm_charge_vote->vote)
			cm_update_charge_info(cm, CM_CHARGE_INFO_JEITA_LIMIT);
	}

	return ret;
}

static int cm_get_power_supply_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	int ret = 0;
	struct cm_power_supply_data *data = container_of(psy->desc, struct  cm_power_supply_data, psd);

	if (!data || !data->cm)
		return -ENOMEM;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->ONLINE;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		cm_get_current_max(data->cm, &val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		cm_get_voltage_max(data->cm, &val->intval);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void cm_get_charge_type(struct charger_manager *cm, int *charge_type)
{
	switch (cm->desc->charger_type) {
	case CM_CHARGER_TYPE_SDP:
	case CM_CHARGER_TYPE_DCP:
	case CM_CHARGER_TYPE_CDP:
		*charge_type = POWER_SUPPLY_CHARGE_TYPE_STANDARD;
		break;

	case CM_CHARGER_TYPE_FAST:
		*charge_type = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;

	case CM_CHARGER_TYPE_ADAPTIVE:
		*charge_type = POWER_SUPPLY_CHARGE_TYPE_ADAPTIVE;
		break;
	default:
		*charge_type = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}
}

static bool cm_add_battery_psy_property(struct charger_manager *cm,
					enum power_supply_property psp,
					enum power_supply_property *properties,
					size_t *num_properties)
{
	u32 i;

	for (i = 0; i < *num_properties; i++)
		if (properties[i] == psp)
			break;

	if (i == *num_properties) {
		properties[*num_properties] = psp;
		(*num_properties)++;
		return true;
	}
	return false;
}

static int charger_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct charger_manager *cm = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!cm)
		return -ENOMEM;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		cm_get_charging_status(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		cm_get_charging_health_status(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = is_batt_present(cm);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = get_vbat_avg_uV(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = get_vbat_now_uV(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = get_ibat_avg_uA(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = get_ibat_now_uA(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		ret = cm_get_battery_technology(cm, val);
		break;

	case POWER_SUPPLY_PROP_TEMP:
		val->intval = cm->desc->temperature;
		break;

	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		return cm_get_battery_temperature(cm, &val->intval);

	case POWER_SUPPLY_PROP_CAPACITY:
		cm_get_uisoc(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = cm_get_capacity_level(cm);
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = is_ext_pwr_online(cm);
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = cm_get_charge_full_uah(cm, val);
		break;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = cm_get_charge_now(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = get_constant_charge_current(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = get_input_current_limit(cm,  &val->intval);
		break;

	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		ret = cm_get_charge_counter(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		ret = cm_get_charge_control_limit(cm, val);
		break;

	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		val->intval = MAX_STATE;
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret = cm_get_charge_full_design(cm, val);
		break;

	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		ret = cm_get_time_to_full_now(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_USB_TYPE:
		ret = cm_get_bc1p2_type(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		cm_get_charge_type(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = cm_get_charge_cycle(cm, &val->intval);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		ret = cm_get_basp_max_volt(cm, &val->intval);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int charger_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct charger_manager *cm = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBAT,
					 SPRD_VOTE_TYPE_IBAT_ID_CONSTANT_CHARGE_CURRENT,
					 SPRD_VOTE_CMD_MIN,
					 val->intval, cm);
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		/* The ChargerIC with linear charging cannot set Ibus, only Ibat. */
		if (cm->desc->thm_info.need_calib_charge_lmt) {
			cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBAT,
					 SPRD_VOTE_TYPE_IBAT_ID_INPUT_CURRENT_LIMIT,
					 SPRD_VOTE_CMD_MIN,
					 val->intval, cm);
			break;
		}

		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBUS,
					 SPRD_VOTE_TYPE_IBUS_ID_INPUT_CURRENT_LIMIT,
					 SPRD_VOTE_CMD_MIN,
					 val->intval, cm);
		break;

	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		cm_set_charge_control_limit(cm, val->intval);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		ret = cm_set_voltage_max_design(cm, val->intval);
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static int charger_property_is_writeable(struct power_supply *psy, enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}
#define NUM_CHARGER_PSY_OPTIONAL	(4)

static enum power_supply_property wireless_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static enum power_supply_property ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static enum power_supply_property default_charger_props[] = {
	/* Guaranteed to provide */
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	/*
	 * Optional properties are:
	 * POWER_SUPPLY_PROP_CHARGE_NOW,
	 */
};

/* wireless_data initialization */
static struct cm_power_supply_data wireless_main = {
	.psd = {
		.name = "wireless",
		.type =	POWER_SUPPLY_TYPE_WIRELESS,
		.properties = wireless_props,
		.num_properties = ARRAY_SIZE(wireless_props),
		.get_property = cm_get_power_supply_property,
	},
	.ONLINE = 0,
};

/* ac_data initialization */
static struct cm_power_supply_data ac_main = {
	.psd = {
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.properties = ac_props,
		.num_properties = ARRAY_SIZE(ac_props),
		.get_property = cm_get_power_supply_property,
	},
	.ONLINE = 0,
};

/* usb_data initialization */
static struct cm_power_supply_data usb_main = {
	.psd = {
		.name = "usb",
		.type = POWER_SUPPLY_TYPE_USB,
		.properties = usb_props,
		.num_properties = ARRAY_SIZE(usb_props),
		.get_property = cm_get_power_supply_property,
	},
	.ONLINE = 0,
};

static enum power_supply_usb_type default_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static const struct power_supply_desc psy_default = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = default_charger_props,
	.num_properties = ARRAY_SIZE(default_charger_props),
	.get_property = charger_get_property,
	.set_property = charger_set_property,
	.property_is_writeable	= charger_property_is_writeable,
	.usb_types		= default_usb_types,
	.num_usb_types		= ARRAY_SIZE(default_usb_types),
	.no_thermal = true,
};

static void cm_update_charger_type_status(struct charger_manager *cm)
{

	if (is_ext_usb_pwr_online(cm)) {
		switch (cm->desc->charger_type) {
		case CM_CHARGER_TYPE_DCP:
		case CM_CHARGER_TYPE_FAST:
		case CM_CHARGER_TYPE_ADAPTIVE:
			wireless_main.ONLINE = 0;
			usb_main.ONLINE = 0;
			ac_main.ONLINE = 1;
			break;
		default:
			wireless_main.ONLINE = 0;
			ac_main.ONLINE = 0;
			usb_main.ONLINE = 1;
			break;
		}
	} else if (is_ext_wl_pwr_online(cm)) {
		wireless_main.ONLINE = 1;
		ac_main.ONLINE = 0;
		usb_main.ONLINE = 0;
	} else {
		wireless_main.ONLINE = 0;
		ac_main.ONLINE = 0;
		usb_main.ONLINE = 0;
	}
}

/**
 * cm_setup_timer - For in-suspend monitoring setup wakeup alarm
 *		    for suspend_again.
 *
 * Returns true if the alarm is set for Charger Manager to use.
 * Returns false if
 *	cm_setup_timer fails to set an alarm,
 *	cm_setup_timer does not need to set an alarm for Charger Manager,
 *	or an alarm previously configured is to be used.
 */
static bool cm_setup_timer(void)
{
	struct charger_manager *cm;
	unsigned int wakeup_ms = UINT_MAX;
	int timer_req = 0;

	if (time_after(next_polling, jiffies))
		CM_MIN_VALID(wakeup_ms,
			jiffies_to_msecs(next_polling - jiffies));

	mutex_lock(&cm_list_mtx);
	list_for_each_entry(cm, &cm_list, entry) {
		unsigned int fbchk_ms = 0;

		/* fullbatt_vchk is required. setup timer for that */
		if (cm->fullbatt_vchk_jiffies_at) {
			fbchk_ms = jiffies_to_msecs(cm->fullbatt_vchk_jiffies_at
						    - jiffies);
			if (time_is_before_eq_jiffies(
				cm->fullbatt_vchk_jiffies_at) ||
				msecs_to_jiffies(fbchk_ms) < CM_JIFFIES_SMALL) {
				fullbatt_vchk(&cm->fullbatt_vchk_work.work);
				fbchk_ms = 0;
			}
		}
		CM_MIN_VALID(wakeup_ms, fbchk_ms);

		/* Skip if polling is not required for this CM */
		if (!is_polling_required(cm) && !cm->emergency_stop)
			continue;
		timer_req++;
		if (cm->desc->polling_interval_ms == 0)
			continue;
		if (cm->desc->ir_comp.ir_compensation_en)
			CM_MIN_VALID(wakeup_ms, CM_IR_COMPENSATION_TIME * 1000);
		else
			CM_MIN_VALID(wakeup_ms, cm->desc->polling_interval_ms);
	}
	mutex_unlock(&cm_list_mtx);

	if (timer_req && cm_timer) {
		ktime_t now, add;

		/*
		 * Set alarm with the polling interval (wakeup_ms)
		 * The alarm time should be NOW + CM_RTC_SMALL or later.
		 */
		if (wakeup_ms == UINT_MAX ||
			wakeup_ms < CM_RTC_SMALL * MSEC_PER_SEC)
			wakeup_ms = 2 * CM_RTC_SMALL * MSEC_PER_SEC;

		pr_info("Charger Manager wakeup timer: %u ms\n", wakeup_ms);

		now = ktime_get_boottime();
		add = ktime_set(wakeup_ms / MSEC_PER_SEC,
				(wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
		alarm_start(cm_timer, ktime_add(now, add));

		cm_suspend_duration_ms = wakeup_ms;

		return true;
	}
	return false;
}

static ssize_t jeita_control_show(struct device *dev,  struct device_attribute *attr, char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_jeita_control);
	struct charger_desc *desc;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	desc = sysfs->cm->desc;
	if (!desc) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	return sprintf(buf, "%d\n", !desc->jeita_disabled);
}

static ssize_t jeita_control_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_jeita_control);
	struct charger_desc *desc;
	bool enabled;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	desc = sysfs->cm->desc;
	if (!desc) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret =  kstrtobool(buf, &enabled);
	if (ret)
		return ret;

	desc->jeita_disabled = !enabled;

	return count;
}

static ssize_t
charge_pump_present_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_charge_pump_present);
	struct charger_manager *cm;
	bool status = false;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (cm_check_cp_charger_enabled(cm))
		status = true;

	return sprintf(buf, "%d\n", status);
}

static ssize_t charge_pump_present_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret;
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_charge_pump_present);
	struct charger_manager *cm;
	bool enabled;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret =  kstrtobool(buf, &enabled);
	if (ret)
		return ret;

	if (enabled) {
		cm_init_cp(cm);
		cm_primary_charger_enable(cm, false);
		if (cm_check_primary_charger_enabled(cm)) {
			dev_err(cm->dev, "Fail to disable primary charger\n");
			return -EINVAL;
		}

		cm_cp_master_charger_enable(cm, true);
		if (!cm_check_cp_charger_enabled(cm))
			dev_err(cm->dev, "Fail to enable charge pump\n");
	} else {
		if (!cm_cp_master_charger_enable(cm, false))
			dev_err(cm->dev, "Fail to disable master charge pump\n");
	}

	return count;
}

static ssize_t
charge_pump_current_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_charge_pump_current);
	struct charger_manager *cm;
	int cur, ret;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (sysfs->cp_id < 0) {
		dev_err(cm->dev, "charge pump id is error!!!!!!\n");
		cur = 0;
		return sprintf(buf, "%d\n", cur);
	}

	ret = get_cp_ibat_uA_by_id(cm, &cur, sysfs->cp_id);
	if (ret)
		cur = 0;

	return sprintf(buf, "%d\n", cur);
}

static ssize_t charge_pump_current_id_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	int ret;
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_charge_pump_current);
	struct charger_manager *cm;
	int cp_id;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret =  kstrtoint(buf, 10, &cp_id);
	if (ret)
		return ret;

	if (cp_id < 0) {
		dev_err(cm->dev, "charge pump id is error!!!!!!\n");
		cp_id = 0;
	}
	sysfs->cp_id = cp_id;

	return count;
}

static ssize_t charger_stop_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_stop_charge);
	bool stop_charge;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}
	stop_charge = is_charging(sysfs->cm);

	return sprintf(buf, "%d\n", !stop_charge);
}

static ssize_t charger_stop_store(struct device *dev,
				  struct device_attribute *attr, const char *buf,
				  size_t count)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_stop_charge);
	struct charger_manager *cm;
	int stop_charge, ret;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm || !cm->charger_psy) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret = sscanf(buf, "%d", &stop_charge);
	if (!ret)
		return -EINVAL;

	sysfs->externally_control = !!stop_charge;
	if (!is_ext_pwr_online(cm))
		return -EINVAL;

	dev_dbg(cm->dev, "%s, stop_charge=%d\n", __func__, stop_charge);
	if (!stop_charge) {
		ret = try_charger_enable(cm, true);
		if (ret) {
			dev_err(cm->dev, "failed to start charger.\n");
			return ret;
		}
		mod_delayed_work(cm_wq, &cm_monitor_work, 0);
	} else {
		ret = try_charger_enable(cm, false);
		if (ret) {
			dev_err(cm->dev, "failed to stop charger.\n");
			return ret;
		}
	}

	power_supply_changed(cm->charger_psy);
	return count;
}

static ssize_t charger_externally_control_show(struct device *dev,
					       struct device_attribute *attr, char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_externally_control);

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	return sprintf(buf, "%d\n", sysfs->externally_control);
}

static ssize_t charger_externally_control_store(struct device *dev,
						struct device_attribute *attr, const char *buf,
						size_t count)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_externally_control);
	struct charger_manager *cm;
	struct charger_desc *desc;
	int i;
	int ret;
	int externally_control;
	int chargers_externally_control = 1;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	desc = cm->desc;
	if (!desc) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret = sscanf(buf, "%d", &externally_control);
	if (ret == 0) {
		ret = -EINVAL;
		return ret;
	}

	if (!externally_control) {
		sysfs->externally_control = 0;
		return count;
	}

	for (i = 0; i < desc->num_sysfs; i++) {
		if (&desc->sysfs[i] != sysfs &&
			!desc->sysfs[i].externally_control) {
			/*
			 * At least, one charger is controlled by
			 * charger-manager
			 */
			chargers_externally_control = 0;
			break;
		}
	}

	if (!chargers_externally_control) {
		if (cm->charger_enabled) {
			try_charger_enable(sysfs->cm, false);
			sysfs->externally_control = externally_control;
			try_charger_enable(sysfs->cm, true);
		} else {
			sysfs->externally_control = externally_control;
		}
	} else {
		dev_warn(cm->dev,
			 "regulator should be controlled in charger-manager because charger-manager"
			 "must need at least one charger for charging\n");
	}

	return count;
}

static ssize_t cp_num_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_cp_num);
	struct charger_manager *cm;
	int cp_num = 0;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm || !cm->desc) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cp_num = cm->desc->cp_nums;
	return sprintf(buf, "%d\n", cp_num);
}

static ssize_t enable_power_path_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_enable_power_path);
	struct charger_manager *cm;
	bool power_path_enabled;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	power_path_enabled = cm_is_power_path_enabled(cm);

	return sprintf(buf, "%d\n", power_path_enabled);
}

static ssize_t enable_power_path_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_enable_power_path);
	struct charger_manager *cm;
	bool power_path_enabled;
	int ret;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm || !cm->charger_psy) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret =  kstrtobool(buf, &power_path_enabled);
	if (ret)
		return ret;

	if (power_path_enabled)
		cm_power_path_enable(cm, CM_POWER_PATH_ENABLE_CMD);
	else
		cm_power_path_enable(cm, CM_POWER_PATH_DISABLE_CMD);

	power_supply_changed(cm->charger_psy);

	return count;
}

static ssize_t keep_awake_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_keep_awake);
	struct charger_manager *cm;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm || !cm->desc) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	return sprintf(buf, "%d\n", cm->desc->keep_awake);
}

static ssize_t keep_awake_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_keep_awake);
	struct charger_manager *cm;
	bool enabled;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm || !cm->desc) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ret =  kstrtobool(buf, &enabled);
	if (ret)
		return ret;

	if (cm->desc->keep_awake != enabled) {
		mutex_lock(&cm->desc->keep_awake_mtx);
		if (cm->charger_enabled && enabled) {
			dev_info(cm->dev, "Acquire charger_manager_wakelock when enable charge\n");
			__pm_stay_awake(cm->charge_ws);
		} else if (cm->charger_enabled && !enabled) {
			dev_info(cm->dev, "Release charger_manager_wakelock when disable charge\n");
			__pm_relax(cm->charge_ws);
		}
		cm->desc->keep_awake = enabled;
		mutex_unlock(&cm->desc->keep_awake_mtx);
	}

	return count;
}

static ssize_t support_fast_charge_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct charger_sysfs_ctl_item *sysfs = container_of(attr, struct charger_sysfs_ctl_item,
							    attr_support_fast_charge);
	struct charger_manager *cm;
	bool support_fast_charge = false;

	if (!sysfs) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	cm = sysfs->cm;
	if (!cm || !cm->fchg_info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	support_fast_charge = cm->fchg_info->support_fchg;

	return sprintf(buf, "%d\n", support_fast_charge);
}

/**
 * charger_manager_prepare_sysfs - Prepare sysfs entry for each charger
 * @cm: the Charger Manager representing the battery.
 *
 * This function add sysfs entry for charger to control charger from
 * user-space. If some development board use one more chargers for charging
 * but only need one charger on specific case which is dependent on user
 * scenario or hardware restrictions, the user enter 1 or 0(zero) to '/sys/
 * class/power_supply/battery/charger.[index]/externally_control'. For example,
 * if user enter 1 to 'sys/class/power_supply/battery/charger.[index]/
 * externally_control, this charger isn't controlled from charger-manager and
 * always stay off state.
 */
static int charger_manager_prepare_sysfs(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct charger_sysfs_ctl_item *sysfs;
	int chargers_externally_control = 1;
	char *name;
	int i;

	desc->num_sysfs = 1;

	desc->sysfs_groups = devm_kcalloc(cm->dev, desc->num_sysfs + 1, sizeof(*desc->sysfs_groups),
					  GFP_KERNEL);
	if (!desc->sysfs_groups)
		return -ENOMEM;

	/* Create sysfs entry to control charger */
	for (i = 0; i < desc->num_sysfs; i++) {
		sysfs = devm_kzalloc(cm->dev, sizeof(*sysfs), GFP_KERNEL);
		if (!sysfs)
			return -ENOMEM;

		desc->sysfs = sysfs;
		desc->sysfs->cm = cm;
		name = devm_kasprintf(cm->dev, GFP_KERNEL, "charger.%d", i);
		if (!name)
			return -ENOMEM;

		sysfs->attrs[0] = &sysfs->attr_externally_control.attr;
		sysfs->attrs[1] = &sysfs->attr_stop_charge.attr;
		sysfs->attrs[2] = &sysfs->attr_jeita_control.attr;
		sysfs->attrs[3] = &sysfs->attr_cp_num.attr;
		sysfs->attrs[4] = &sysfs->attr_charge_pump_present.attr;
		sysfs->attrs[5] = &sysfs->attr_charge_pump_current.attr;
		sysfs->attrs[6] = &sysfs->attr_enable_power_path.attr;
		sysfs->attrs[7] = &sysfs->attr_keep_awake.attr;
		sysfs->attrs[8] = &sysfs->attr_support_fast_charge.attr;
		sysfs->attrs[9] = NULL;

		sysfs->attr_grp.name = name;
		sysfs->attr_grp.attrs = sysfs->attrs;
		desc->sysfs_groups[i] = &sysfs->attr_grp;

		sysfs_attr_init(&sysfs->attr_stop_charge.attr);
		sysfs->attr_stop_charge.attr.name = "stop_charge";
		sysfs->attr_stop_charge.attr.mode = 0644;
		sysfs->attr_stop_charge.show = charger_stop_show;
		sysfs->attr_stop_charge.store = charger_stop_store;

		sysfs_attr_init(&sysfs->attr_jeita_control.attr);
		sysfs->attr_jeita_control.attr.name = "jeita_control";
		sysfs->attr_jeita_control.attr.mode = 0644;
		sysfs->attr_jeita_control.show = jeita_control_show;
		sysfs->attr_jeita_control.store = jeita_control_store;

		sysfs_attr_init(&sysfs->attr_cp_num.attr);
		sysfs->attr_cp_num.attr.name = "cp_num";
		sysfs->attr_cp_num.attr.mode = 0444;
		sysfs->attr_cp_num.show = cp_num_show;

		sysfs_attr_init(&sysfs->attr_charge_pump_present.attr);
		sysfs->attr_charge_pump_present.attr.name = "charge_pump_present";
		sysfs->attr_charge_pump_present.attr.mode = 0644;
		sysfs->attr_charge_pump_present.show = charge_pump_present_show;
		sysfs->attr_charge_pump_present.store = charge_pump_present_store;

		sysfs_attr_init(&sysfs->attr_charge_pump_current.attr);
		sysfs->attr_charge_pump_current.attr.name = "charge_pump_current";
		sysfs->attr_charge_pump_current.attr.mode = 0644;
		sysfs->attr_charge_pump_current.show = charge_pump_current_show;
		sysfs->attr_charge_pump_current.store = charge_pump_current_id_store;

		sysfs_attr_init(&sysfs->attr_enable_power_path.attr);
		sysfs->attr_enable_power_path.attr.name = "enable_power_path";
		sysfs->attr_enable_power_path.attr.mode = 0644;
		sysfs->attr_enable_power_path.show = enable_power_path_show;
		sysfs->attr_enable_power_path.store = enable_power_path_store;

		sysfs_attr_init(&sysfs->attr_keep_awake.attr);
		sysfs->attr_keep_awake.attr.name = "keep_awake";
		sysfs->attr_keep_awake.attr.mode = 0644;
		sysfs->attr_keep_awake.show = keep_awake_show;
		sysfs->attr_keep_awake.store = keep_awake_store;

		sysfs_attr_init(&sysfs->attr_support_fast_charge.attr);
		sysfs->attr_support_fast_charge.attr.name = "support_fast_charge";
		sysfs->attr_support_fast_charge.attr.mode = 0444;
		sysfs->attr_support_fast_charge.show = support_fast_charge_show;

		sysfs_attr_init(&sysfs->attr_externally_control.attr);
		sysfs->attr_externally_control.attr.name = "externally_control";
		sysfs->attr_externally_control.attr.mode = 0644;
		sysfs->attr_externally_control.show
				= charger_externally_control_show;
		sysfs->attr_externally_control.store
				= charger_externally_control_store;

		if (!desc->sysfs[i].externally_control || !chargers_externally_control)
			chargers_externally_control = 0;

		dev_info(cm->dev, "regulator's externally_control is %d\n",
			 sysfs->externally_control);
	}

	if (chargers_externally_control) {
		dev_err(cm->dev, "Cannot register regulator because charger-manager"
			"must need at least one charger for charging battery\n");
		return -EINVAL;
	}

	return 0;
}

static int cm_init_thermal_data(struct charger_manager *cm,
				struct power_supply *fuel_gauge,
				enum power_supply_property *properties,
				size_t *num_properties)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	int ret;

	/* Verify whether fuel gauge provides battery temperature */
	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_TEMP, &val);

	if (!ret) {
		if (!cm_add_battery_psy_property(cm, POWER_SUPPLY_PROP_TEMP,
						 properties, num_properties))
			dev_warn(cm->dev, "POWER_SUPPLY_PROP_TEMP is present\n");
		cm->desc->measure_battery_temp = true;
	}
#if IS_ENABLED(CONFIG_THERMAL)
	if (desc->thermal_zone) {
		cm->tzd_batt =
			thermal_zone_get_zone_by_name(desc->thermal_zone);
		if (IS_ERR(cm->tzd_batt))
			return PTR_ERR(cm->tzd_batt);

		/* Use external thermometer */
		if (!cm_add_battery_psy_property(cm, POWER_SUPPLY_PROP_TEMP_AMBIENT, properties, num_properties))
			dev_warn(cm->dev, "POWER_SUPPLY_PROP_TEMP_AMBIENT is present\n");
		cm->desc->measure_battery_temp = true;
		ret = 0;
	}
#endif
	if (cm->desc->measure_battery_temp) {
		/* NOTICE : Default allowable minimum charge temperature is 0 */
		if (!desc->temp_max)
			desc->temp_max = CM_DEFAULT_CHARGE_TEMP_MAX;
		if (!desc->temp_diff)
			desc->temp_diff = CM_DEFAULT_RECHARGE_TEMP_DIFF;
	}

	return ret;
}

static int cm_init_jeita_table(struct sprd_battery_info *info,
			       struct charger_desc *desc, struct device *dev)
{
	int i;

	for (i = SPRD_BATTERY_JEITA_DCP; i < SPRD_BATTERY_JEITA_MAX; i++) {
		desc->jeita_size[i] = info->sprd_battery_jeita_size[i];
		if (!desc->jeita_size[i]) {
			dev_dbg(dev, "%s jeita_size is zero\n",
				 sprd_battery_jeita_type_names[i]);
			continue;
		}

		desc->jeita_tab_array[i] = devm_kmemdup(dev, info->jeita_table[i],
							desc->jeita_size[i] *
							sizeof(struct sprd_battery_jeita_table),
							GFP_KERNEL);
		if (!desc->jeita_tab_array[i]) {
			dev_warn(dev, "Fail to kmemdup %s\n",
				 sprd_battery_jeita_type_names[i]);
			return -ENOMEM;
		}
	}

	desc->jeita_tab = desc->jeita_tab_array[SPRD_BATTERY_JEITA_UNKNOWN];
	desc->jeita_tab_size = desc->jeita_size[SPRD_BATTERY_JEITA_UNKNOWN];

	return 0;
}

static const struct of_device_id charger_manager_match[] = {
	{
		.compatible = "charger-manager",
	},
	{},
};
MODULE_DEVICE_TABLE(of, charger_manager_match);

static struct charger_desc *of_cm_parse_desc(struct device *dev)
{
	struct charger_desc *desc;
	struct device_node *np = dev->of_node;
	u32 poll_mode = CM_POLL_DISABLE;
	u32 battery_stat = CM_NO_BATTERY;
	int ret, i = 0, num_chgs = 0;
	int num_cp_psys = 0;

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return ERR_PTR(-ENOMEM);

	of_property_read_string(np, "cm-name", &desc->psy_name);

	of_property_read_u32(np, "cm-poll-mode", &poll_mode);
	desc->polling_mode = poll_mode;

	desc->uvlo_shutdown_mode = CM_SHUTDOWN_MODE_ANDROID;
	of_property_read_u32(np, "cm-uvlo-shutdown-mode", &desc->uvlo_shutdown_mode);

	of_property_read_u32(np, "cm-poll-interval",
				&desc->polling_interval_ms);

	of_property_read_u32(np, "cm-fullbatt-vchkdrop-ms",
					&desc->fullbatt_vchkdrop_ms);
	of_property_read_u32(np, "cm-fullbatt-vchkdrop-volt",
					&desc->fullbatt_vchkdrop_uV);
	of_property_read_u32(np, "cm-fullbatt-soc", &desc->fullbatt_soc);
	of_property_read_u32(np, "cm-fullbatt-capacity",
					&desc->fullbatt_full_capacity);
	of_property_read_u32(np, "cm-shutdown-voltage", &desc->shutdown_voltage);
	of_property_read_u32(np, "cm-tickle-time-out", &desc->trickle_time_out);
	of_property_read_u32(np, "cm-one-cap-time", &desc->cap_one_time);
	of_property_read_u32(np, "cm-wdt-interval", &desc->wdt_interval);

	of_property_read_u32(np, "cm-battery-stat", &battery_stat);
	desc->battery_present = battery_stat;

	/* chargers */
	num_chgs = of_property_count_strings(np, "cm-chargers");
	if (num_chgs > 0) {
		/* Allocate empty bin at the tail of array */
		desc->psy_charger_stat = devm_kcalloc(dev,
						      num_chgs + 1,
						      sizeof(char *),
						      GFP_KERNEL);
		if (!desc->psy_charger_stat)
			return ERR_PTR(-ENOMEM);

		for (i = 0; i < num_chgs; i++)
			of_property_read_string_index(np, "cm-chargers", i,
						      &desc->psy_charger_stat[i]);
	}

	desc->enable_alt_cp_adapt =
		device_property_read_bool(dev, "cm-alt-cp-adapt-enable");

	/* alternative charge pupms power supply */
	num_cp_psys = of_property_count_strings(np, "cm-alt-cp-power-supplys");
	dev_info(dev, "%s num_cp_psys = %d\n", __func__, num_cp_psys);
	if (num_cp_psys > 0) {
		desc->psy_cp_nums = num_cp_psys;
		/* Allocate empty bin at the tail of array */
		desc->psy_alt_cp_adpt_stat = devm_kzalloc(dev, sizeof(char *)
						* (num_cp_psys + 1), GFP_KERNEL);
		if (desc->psy_alt_cp_adpt_stat) {
			for (i = 0; i < num_cp_psys; i++)
				of_property_read_string_index(np, "cm-alt-cp-power-supplys",
						i, &desc->psy_alt_cp_adpt_stat[i]);
		} else {
			return ERR_PTR(-ENOMEM);
		}
	}

	/* charge pumps */
	num_chgs = of_property_count_strings(np, "cm-charge-pumps");
	if (num_chgs > 0) {
		/* Allocate empty bin at the tail of array */
		desc->cp_nums = num_chgs;
		desc->psy_cp_stat =
			devm_kzalloc(dev, sizeof(char *) * (u32)(num_chgs + 1), GFP_KERNEL);
		if (!desc->psy_cp_stat)
			return ERR_PTR(-ENOMEM);

		for (i = 0; i < num_chgs; i++)
			of_property_read_string_index(np, "cm-charge-pumps", i,
						      &desc->psy_cp_stat[i]);
	}

	/* wireless chargers */
	num_chgs = of_property_count_strings(np, "cm-wireless-chargers");
	if (num_chgs > 0) {
		/* Allocate empty bin at the tail of array */
		desc->psy_wl_charger_stat =
			devm_kzalloc(dev,  sizeof(char *) * (u32)(num_chgs + 1), GFP_KERNEL);
		if (desc->psy_wl_charger_stat) {
			for (i = 0; i < num_chgs; i++)
				of_property_read_string_index(np, "cm-wireless-chargers",
						i, &desc->psy_wl_charger_stat[i]);
		} else {
			return ERR_PTR(-ENOMEM);
		}
	}

	/* wireless charge pump converters */
	num_chgs = of_property_count_strings(np, "cm-wireless-charge-pump-converters");
	if (num_chgs > 0) {
		/* Allocate empty bin at the tail of array */
		desc->psy_cp_converter_stat =
			devm_kzalloc(dev, sizeof(char *) * (u32)(num_chgs + 1), GFP_KERNEL);
		if (desc->psy_cp_converter_stat) {
			for (i = 0; i < num_chgs; i++)
				of_property_read_string_index(np, "cm-wireless-charge-pump-converters",
						i, &desc->psy_cp_converter_stat[i]);
		} else {
			return ERR_PTR(-ENOMEM);
		}
	}

	of_property_read_string(np, "cm-fuel-gauge", &desc->psy_fuel_gauge);

	of_property_read_string(np, "cm-thermal-zone", &desc->thermal_zone);

	of_property_read_u32(np, "cm-battery-cold", &desc->temp_min);
	if (of_get_property(np, "cm-battery-cold-in-minus", NULL))
		desc->temp_min *= -1;
	of_property_read_u32(np, "cm-battery-hot", &desc->temp_max);
	of_property_read_u32(np, "cm-battery-temp-diff", &desc->temp_diff);

	of_property_read_u32(np, "cm-charging-max",
				&desc->charging_max_duration_ms);
	of_property_read_u32(np, "cm-discharging-max",
				&desc->discharging_max_duration_ms);
	of_property_read_u32(np, "cm-charge-voltage-max",
			     &desc->normal_charge_voltage_max);
	of_property_read_u32(np, "cm-charge-voltage-drop",
			     &desc->normal_charge_voltage_drop);
	of_property_read_u32(np, "cm-fast-charge-voltage-max",
			     &desc->fast_charge_voltage_max);
	of_property_read_u32(np, "cm-fast-charge-voltage-drop",
			     &desc->fast_charge_voltage_drop);
	of_property_read_u32(np, "cm-flash-charge-voltage-max",
			     &desc->flash_charge_voltage_max);
	of_property_read_u32(np, "cm-flash-charge-voltage-drop",
			     &desc->flash_charge_voltage_drop);
	of_property_read_u32(np, "cm-wireless-charge-voltage-max",
			     &desc->wireless_normal_charge_voltage_max);
	of_property_read_u32(np, "cm-wireless-charge-voltage-drop",
			     &desc->wireless_normal_charge_voltage_drop);
	of_property_read_u32(np, "cm-wireless-fast-charge-voltage-max",
			     &desc->wireless_fast_charge_voltage_max);
	of_property_read_u32(np, "cm-wireless-fast-charge-voltage-drop",
			     &desc->wireless_fast_charge_voltage_drop);
	of_property_read_u32(np, "cm-cp-taper-current",
			     &desc->cp.cp_taper_current);
	of_property_read_u32(np, "cm-cap-full-advance-percent",
			     &desc->cap_remap_full_percent);

	if (desc->psy_cp_stat && !desc->cp.cp_taper_current)
		desc->cp.cp_taper_current = CM_CP_DEFAULT_TAPER_CURRENT;

	ret = cm_init_cap_remap_table(desc, dev);
	if (ret)
		dev_err(dev, "%s init cap remap table fail\n", __func__);

	return desc;
}

static inline struct charger_desc *cm_get_drv_data(struct platform_device *pdev)
{
	if (pdev->dev.of_node)
		return of_cm_parse_desc(&pdev->dev);
	return dev_get_platdata(&pdev->dev);
}

static int cm_get_bat_info(struct charger_manager *cm)
{
	struct sprd_battery_info info = {};
	int ret;

	ret = sprd_battery_get_battery_info(cm->charger_psy, &info);
	if (ret) {
		dev_err(cm->dev, "failed to get battery information\n");
		sprd_battery_put_battery_info(cm->charger_psy, &info);
		return ret;
	}

	cm->desc->internal_resist = info.factory_internal_resistance_uohm / 1000;
	cm->desc->ir_comp.us = info.constant_charge_voltage_max_uv;
	cm->desc->ir_comp.us_upper_limit = info.ir.us_upper_limit_uv;
	cm->desc->ir_comp.rc = info.ir.rc_uohm / 1000;
	cm->desc->ir_comp.cp_upper_limit_offset = info.ir.cv_upper_limit_offset_uv;
	cm->desc->constant_charge_voltage_max_uv = info.constant_charge_voltage_max_uv;
	cm->desc->fullbatt_voltage_offset_uv = info.fullbatt_voltage_offset_uv;
	cm->desc->fchg_ocv_threshold = info.fast_charge_ocv_threshold_uv;
	cm->desc->cp.cp_target_vbat = info.constant_charge_voltage_max_uv;
	cm->desc->cp.cp_max_ibat = info.cur.flash_cur;
	cm->desc->cp.cp_target_ibat = info.cur.flash_cur;
	cm->desc->cp.cp_max_ibus = info.cur.flash_limit;
	cm->desc->cur.sdp_limit = info.cur.sdp_limit;
	cm->desc->cur.sdp_cur = info.cur.sdp_cur;
	cm->desc->cur.dcp_limit = info.cur.dcp_limit;
	cm->desc->cur.dcp_cur = info.cur.dcp_cur;
	cm->desc->cur.cdp_limit = info.cur.cdp_limit;
	cm->desc->cur.cdp_cur = info.cur.cdp_cur;
	cm->desc->cur.unknown_limit = info.cur.unknown_limit;
	cm->desc->cur.unknown_cur = info.cur.unknown_cur;
	cm->desc->cur.fchg_limit = info.cur.fchg_limit;
	cm->desc->cur.fchg_cur = info.cur.fchg_cur;
	cm->desc->cur.flash_limit = info.cur.flash_limit;
	cm->desc->cur.flash_cur = info.cur.flash_cur;
	cm->desc->cur.wl_bpp_limit = info.cur.wl_bpp_limit;
	cm->desc->cur.wl_bpp_cur = info.cur.wl_bpp_cur;
	cm->desc->cur.wl_epp_limit = info.cur.wl_epp_limit;
	cm->desc->cur.wl_epp_cur = info.cur.wl_epp_cur;
	cm->desc->fullbatt_uV = info.fullbatt_voltage_uv;
	cm->desc->fullbatt_uA = info.fullbatt_current_uA;
	cm->desc->first_fullbatt_uA = info.first_fullbatt_current_uA;
	cm->desc->force_jeita_status = info.force_jeita_status;

	dev_info(cm->dev, "SPRD_BATTERY_INFO: internal_resist = %d, us = %d, "
		 "constant_charge_voltage_max_uv = %d, fchg_ocv_threshold = %d, "
		 "cp_target_vbat = %d, cp_max_ibat = %d, cp_target_ibat = %d "
		 "cp_max_ibus = %d, sdp_limit = %d, sdp_cur = %d, dcp_limit = %d, "
		 "dcp_cur = %d, cdp_limit = %d, cdp_cur = %d, unknown_limit=%d, "
		 "unknown_cur = %d. fchg_limit = %d, fchg_cur = %d, flash_limit = %d, "
		 "flash_cur = %d, wl_bpp_limit = %d, wl_bpp_cur= %d, wl_epp_limit = %d, "
		 "wl_epp_cur = %d, fullbatt_uV= %d, fullbatt_uA= %d, "
		 "cm->desc->first_fullbatt_uA = %d, us_upper_limit = %d, rc = %d, "
		 "cp_upper_limit_offset = %d, force_jeita_status = %d\n",
		 cm->desc->internal_resist, cm->desc->ir_comp.us,
		 cm->desc->constant_charge_voltage_max_uv, cm->desc->fchg_ocv_threshold,
		 cm->desc->cp.cp_target_vbat, cm->desc->cp.cp_max_ibat,
		 cm->desc->cp.cp_target_ibat, cm->desc->cp.cp_max_ibus, cm->desc->cur.sdp_limit,
		 cm->desc->cur.sdp_cur, cm->desc->cur.dcp_limit, cm->desc->cur.dcp_cur,
		 cm->desc->cur.cdp_limit, cm->desc->cur.cdp_cur, cm->desc->cur.unknown_limit,
		 cm->desc->cur.unknown_cur, cm->desc->cur.fchg_limit, cm->desc->cur.fchg_cur,
		 cm->desc->cur.flash_limit, cm->desc->cur.flash_cur, cm->desc->cur.wl_bpp_limit,
		 cm->desc->cur.wl_bpp_cur, cm->desc->cur.wl_epp_limit, cm->desc->cur.wl_epp_cur,
		 cm->desc->fullbatt_uV, cm->desc->fullbatt_uA, cm->desc->first_fullbatt_uA,
		 cm->desc->ir_comp.us_upper_limit, cm->desc->ir_comp.rc,
		 cm->desc->ir_comp.cp_upper_limit_offset, cm->desc->force_jeita_status);

	ret = cm_init_jeita_table(&info, cm->desc, cm->dev);
	if (ret) {
		sprd_battery_put_battery_info(cm->charger_psy, &info);
		return ret;
	}

	if (cm->desc->force_jeita_status <= 0)
		cm->desc->force_jeita_status = (cm->desc->jeita_tab_size + 1) / 2;

	if (cm->desc->fullbatt_uV == 0)
		dev_info(cm->dev, "Ignoring full-battery voltage threshold as it is not supplied\n");

	if (cm->desc->fullbatt_uA == 0)
		dev_info(cm->dev, "Ignoring full-battery current threshold as it is not supplied\n");

	if (cm->desc->fullbatt_voltage_offset_uv == 0)
		dev_info(cm->dev, "Ignoring full-battery voltage offset as it is not supplied\n");

	sprd_battery_put_battery_info(cm->charger_psy, &info);

	return 0;
}

static void cm_uvlo_check_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
				struct charger_manager, uvlo_work);
	int batt_uV, ret;

	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret || batt_uV < 0) {
		dev_err(cm->dev, "get_vbat_now_uV error.\n");
		return;
	}

	if ((u32)batt_uV < cm->desc->shutdown_voltage)
		cm->desc->uvlo_trigger_cnt++;
	else
		cm->desc->uvlo_trigger_cnt = 0;

	if (cm->desc->uvlo_trigger_cnt >= CM_UVLO_CALIBRATION_CNT_THRESHOLD) {
		dev_err(cm->dev, "WARN: batt_uV less than uvlo, will shutdown\n");
		set_batt_cap(cm, 0);
		switch (cm->desc->uvlo_shutdown_mode) {
		case CM_SHUTDOWN_MODE_ORDERLY:
			orderly_poweroff(true);
			break;

		case CM_SHUTDOWN_MODE_KERNEL:
			kernel_power_off();
			break;

		case CM_SHUTDOWN_MODE_ANDROID:
			cancel_delayed_work_sync(&cm->cap_update_work);
			cm->desc->cap = 0;
			power_supply_changed(cm->charger_psy);
			break;

		default:
			dev_warn(cm->dev, "Incorrect uvlo_shutdown_mode (%d)\n",
				 cm->desc->uvlo_shutdown_mode);
		}
	}

	if (batt_uV < CM_UVLO_CALIBRATION_VOLTAGE_THRESHOLD)
		schedule_delayed_work(&cm->uvlo_work, msecs_to_jiffies(800));
}

static void cm_batt_works(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
				struct charger_manager, cap_update_work);
	struct timespec64 cur_time;
	int batt_uV, batt_ocV, batt_uA, fuel_cap, ret;
	int period_time, flush_time, cur_temp, board_temp = 0;
	int chg_cur = 0, chg_limit_cur = 0, input_cur = 0;
	int chg_vol = 0, vbat_avg = 0, ibat_avg = 0, recharge_uv = 0;
	static int last_fuel_cap = CM_MAGIC_NUM;

	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret) {
		dev_err(cm->dev, "get_vbat_now_uV error.\n");
		goto schedule_cap_update_work;
	}

	ret = get_vbat_avg_uV(cm, &vbat_avg);
	if (ret)
		dev_err(cm->dev, "get_vbat_avg_uV error.\n");

	ret = get_batt_ocv(cm, &batt_ocV);
	if (ret) {
		dev_err(cm->dev, "get_batt_ocV error.\n");
		goto schedule_cap_update_work;
	}

	ret = get_ibat_now_uA(cm, &batt_uA);
	if (ret) {
		dev_err(cm->dev, "get batt_uA error.\n");
		goto schedule_cap_update_work;
	}

	ret = get_ibat_avg_uA(cm, &ibat_avg);
	if (ret)
		dev_err(cm->dev, "get ibat_avg_uA error.\n");

	ret = get_batt_cap(cm, &fuel_cap);
	if (ret) {
		dev_err(cm->dev, "get fuel_cap error.\n");
		goto schedule_cap_update_work;
	}
	fuel_cap = cm_capacity_remap(cm, fuel_cap);

	ret = get_constant_charge_current(cm, &chg_cur);
	if (ret)
		dev_warn(cm->dev, "get constant charge error.\n");

	ret = get_input_current_limit(cm, &chg_limit_cur);
	if (ret)
		dev_warn(cm->dev, "get chg_limit_cur error.\n");

	if (cm->desc->cp.cp_running)
		ret = get_cp_ibus_uA(cm, &input_cur);
	else
		ret = get_charger_input_current(cm, &input_cur);
	if (ret)
		dev_warn(cm->dev, "cant not get input_cur.\n");

	ret = get_charger_voltage(cm, &chg_vol);
	if (ret)
		dev_warn(cm->dev, "get chg_vol error.\n");

	ret = cm_get_battery_temperature_by_psy(cm, &cur_temp);
	if (ret) {
		dev_err(cm->dev, "failed to get battery temperature\n");
		goto schedule_cap_update_work;
	}

	cm->desc->temperature = cur_temp;

	ret = cm_get_battery_temperature(cm, &board_temp);
	if (ret)
		dev_warn(cm->dev, "failed to get board temperature\n");

	if (cur_temp <= CM_LOW_TEMP_REGION &&
	    batt_uV <= CM_LOW_TEMP_SHUTDOWN_VALTAGE) {
		if (cm->desc->low_temp_trigger_cnt++ > 1)
			fuel_cap = 0;
	} else if (cm->desc->low_temp_trigger_cnt != 0) {
		cm->desc->low_temp_trigger_cnt = 0;
	}

	if (fuel_cap > CM_CAP_FULL_PERCENT)
		fuel_cap = CM_CAP_FULL_PERCENT;
	else if (fuel_cap < 0)
		fuel_cap = 0;

	if (last_fuel_cap == CM_MAGIC_NUM)
		last_fuel_cap = fuel_cap;

	cur_time = ktime_to_timespec64(ktime_get_boottime());

	if (is_full_charged(cm))
		cm->battery_status = POWER_SUPPLY_STATUS_FULL;
	else if (is_charging(cm))
		cm->battery_status = POWER_SUPPLY_STATUS_CHARGING;
	else if (is_ext_pwr_online(cm))
		cm->battery_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	else
		cm->battery_status = POWER_SUPPLY_STATUS_DISCHARGING;

	/*
	 * Record the charging time when battery
	 * capacity is larger than 99%.
	 */
	if (cm->battery_status == POWER_SUPPLY_STATUS_CHARGING) {
		if (cm->desc->cap >= 985) {
			cm->desc->trickle_time =
				cur_time.tv_sec - cm->desc->trickle_start_time;
		} else {
			cm->desc->trickle_start_time = cur_time.tv_sec;
			cm->desc->trickle_time = 0;
		}
	} else {
		cm->desc->trickle_start_time = cur_time.tv_sec;
		cm->desc->trickle_time = cm->desc->trickle_time_out +
				cm->desc->cap_one_time;
	}

	flush_time = cur_time.tv_sec - cm->desc->update_capacity_time;
	period_time = cur_time.tv_sec - cm->desc->last_query_time;
	cm->desc->last_query_time = cur_time.tv_sec;

	if (cm->desc->force_set_full && is_ext_pwr_online(cm))
		cm->desc->charger_status = POWER_SUPPLY_STATUS_FULL;
	else
		cm->desc->charger_status = cm->battery_status;

	dev_info(cm->dev, "vbat: %d, vbat_avg: %d, OCV: %d, ibat: %d, ibat_avg: %d, ibus: %d,"
		 " vbus: %d, msoc: %d, chg_sts: %d, frce_full: %d, chg_lmt_cur: %d,"
		 " inpt_lmt_cur: %d, chgr_type: %d, Tboard: %d, Tbatt: %d, thm_cur: %d,"
		 " thm_pwr: %d, is_fchg: %d, fchg_en: %d, tflush: %d, tperiod: %d\n",
		 batt_uV, vbat_avg, batt_ocV, batt_uA, ibat_avg, input_cur, chg_vol, fuel_cap,
		 cm->desc->charger_status, cm->desc->force_set_full, chg_cur, chg_limit_cur,
		 cm->desc->charger_type, board_temp, cur_temp,
		 cm->desc->thm_info.thm_adjust_cur, cm->desc->thm_info.thm_pwr,
		 cm->desc->is_fast_charge, cm->desc->enable_fast_charge, flush_time, period_time);

	switch (cm->desc->charger_status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		last_fuel_cap = fuel_cap;
		if (fuel_cap < cm->desc->cap) {
			if (batt_uA >= 0) {
				fuel_cap = cm->desc->cap;
			} else {
				if (period_time < cm->desc->cap_one_time) {
					/*
					 * The percentage of electricity is not
					 * allowed to change by 1% in cm->desc->cap_one_time.
					 */
					if ((cm->desc->cap - fuel_cap) >= 5)
						fuel_cap = cm->desc->cap - 5;
					if (flush_time < cm->desc->cap_one_time &&
					    DIV_ROUND_CLOSEST(fuel_cap, 10) !=
					    DIV_ROUND_CLOSEST(cm->desc->cap, 10))
						fuel_cap = cm->desc->cap;
				} else {
					/*
					 * If wake up from long sleep mode,
					 * will make a percentage compensation based on time.
					 */
					if ((cm->desc->cap - fuel_cap) >=
					    (period_time / cm->desc->cap_one_time) * 10)
						fuel_cap = cm->desc->cap -
							(period_time / cm->desc->cap_one_time) * 10;
				}
			}
		} else if (fuel_cap > cm->desc->cap) {
			if (period_time < cm->desc->cap_one_time) {
				if ((fuel_cap - cm->desc->cap) >= 5)
					fuel_cap = cm->desc->cap + 5;
				if (flush_time < cm->desc->cap_one_time &&
				    DIV_ROUND_CLOSEST(fuel_cap, 10) !=
				    DIV_ROUND_CLOSEST(cm->desc->cap, 10))
					fuel_cap = cm->desc->cap;
			} else {
				/*
				 * If wake up from long sleep mode,
				 * will make a percentage compensation based on time.
				 */
				if ((fuel_cap - cm->desc->cap) >=
				    (period_time / cm->desc->cap_one_time) * 10)
					fuel_cap = cm->desc->cap +
						(period_time / cm->desc->cap_one_time) * 10;
			}
		}

		if (cm->desc->cap >= 985 && cm->desc->cap <= 994 &&
		    fuel_cap >= CM_CAP_FULL_PERCENT)
			fuel_cap = 994;
		/*
		 * Record 99% of the charging time.
		 * if it is greater than 1500s,
		 * it will be mandatory to display 100%,
		 * but the background is still charging.
		 */
		if (cm->desc->cap >= 985 &&
		    cm->desc->trickle_time >= cm->desc->trickle_time_out &&
		    cm->desc->trickle_time_out > 0 &&
		    batt_uA > 0)
			cm->desc->force_set_full = true;

		break;

	case POWER_SUPPLY_STATUS_NOT_CHARGING:
	case POWER_SUPPLY_STATUS_DISCHARGING:
		/*
		 * In not charging status,
		 * the cap is not allowed to increase.
		 */
		if (fuel_cap >= cm->desc->cap) {
			last_fuel_cap = fuel_cap;
			fuel_cap = cm->desc->cap;
		} else if (cm->desc->cap >= CM_HCAP_THRESHOLD) {
			if (last_fuel_cap - fuel_cap >= CM_HCAP_DECREASE_STEP) {
				if (cm->desc->cap - fuel_cap >= CM_CAP_ONE_PERCENT)
					fuel_cap = cm->desc->cap - CM_CAP_ONE_PERCENT;
				else
					fuel_cap = cm->desc->cap - CM_HCAP_DECREASE_STEP;

				last_fuel_cap -= CM_HCAP_DECREASE_STEP;
			} else {
				fuel_cap = cm->desc->cap;
			}
		} else {
			if (period_time < cm->desc->cap_one_time) {
				if ((cm->desc->cap - fuel_cap) >= 5)
					fuel_cap = cm->desc->cap - 5;
				if (flush_time < cm->desc->cap_one_time &&
				    DIV_ROUND_CLOSEST(fuel_cap, 10) !=
				    DIV_ROUND_CLOSEST(cm->desc->cap, 10))
					fuel_cap = cm->desc->cap;
			} else {
				/*
				 * If wake up from long sleep mode,
				 * will make a percentage compensation based on time.
				 */
				if ((cm->desc->cap - fuel_cap) >=
				    (period_time / cm->desc->cap_one_time) * 10)
					fuel_cap = cm->desc->cap -
						(period_time / cm->desc->cap_one_time) * 10;
			}
		}
		break;

	case POWER_SUPPLY_STATUS_FULL:
		last_fuel_cap = fuel_cap;
		cm->desc->update_capacity_time = cur_time.tv_sec;
		recharge_uv = cm->desc->fullbatt_uV - cm->desc->fullbatt_vchkdrop_uV - 50000;
		if ((batt_ocV < recharge_uv) && (batt_uA < 0)) {
			cm->desc->force_set_full = false;
			dev_info(cm->dev, "recharge_uv = %d\n", recharge_uv);
		}

		if (is_ext_pwr_online(cm)) {
			if (fuel_cap != CM_CAP_FULL_PERCENT)
				fuel_cap = CM_CAP_FULL_PERCENT;

			if (fuel_cap > cm->desc->cap)
				fuel_cap = cm->desc->cap + 1;
		}

		break;
	default:
		break;
	}

	if (batt_uV < CM_UVLO_CALIBRATION_VOLTAGE_THRESHOLD) {
		dev_info(cm->dev, "batt_uV is less than UVLO calib volt\n");
		schedule_delayed_work(&cm->uvlo_work, msecs_to_jiffies(100));
	}

	dev_info(cm->dev, "new_uisoc = %d, old_uisoc = %d\n", fuel_cap, cm->desc->cap);

	if (fuel_cap != cm->desc->cap) {
		if (DIV_ROUND_CLOSEST(fuel_cap, 10) != DIV_ROUND_CLOSEST(cm->desc->cap, 10)) {
			cm->desc->cap = fuel_cap;
			cm->desc->update_capacity_time = cur_time.tv_sec;
			power_supply_changed(cm->charger_psy);
		}

		cm->desc->cap = fuel_cap;
		if (cm->desc->uvlo_trigger_cnt < CM_UVLO_CALIBRATION_CNT_THRESHOLD)
			set_batt_cap(cm, cm->desc->cap);
	}

schedule_cap_update_work:
	queue_delayed_work(system_power_efficient_wq,
			   &cm->cap_update_work,
			   CM_CAP_CYCLE_TRACK_TIME * HZ);
}

static int cm_check_alt_cp_psy_ready_status(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	int i;

	if (!desc->psy_cp_stat || !desc->psy_alt_cp_adpt_stat) {
		dev_err(cm->dev, "%s, cp not exit\n", __func__);
		return 0;
	}

	psy = power_supply_get_by_name(desc->psy_cp_stat[0]);
	if (psy) {
		dev_info(cm->dev, "%s, find preferred cp \"%s\"\n",
			 __func__, desc->psy_cp_stat[0]);
		goto done;
	}

	for (i = 0; desc->psy_alt_cp_adpt_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_alt_cp_adpt_stat[i]);
		if (!psy) {
			dev_warn(cm->dev, "%s, cannot find alt cp \"%s\"\n",
				 __func__, desc->psy_alt_cp_adpt_stat[i]);
		} else {
			dev_info(cm->dev, "%s, find alt cp \"%s\"\n",
				 __func__, desc->psy_alt_cp_adpt_stat[i]);
			desc->psy_cp_stat[0] = desc->psy_alt_cp_adpt_stat[i];
			goto done;
		}
	}

	if (i == desc->psy_cp_nums) {
		dev_err(cm->dev, "%s, cannot find all cp\n", __func__);
		return -EPROBE_DEFER;
	}

done:
	power_supply_put(psy);
	return 0;
}

static int get_boot_mode(void)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (ret)
		return ret;

	if (strstr(cmd_line, "sprdboot.mode=cali") ||
	    strstr(cmd_line, "sprdboot.mode=autotest"))
		allow_charger_enable = true;
	else if (strstr(cmd_line, "sprdboot.mode=charger"))
		is_charger_mode =  true;

	return 0;
}

static int charger_manager_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct charger_desc *desc = cm_get_drv_data(pdev);
	struct charger_manager *cm;
	int ret, i = 0;
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	enum power_supply_property *properties;
	size_t num_properties;
	struct power_supply_config psy_cfg = {};
	struct timespec64 cur_time;

	if (IS_ERR(desc)) {
		dev_err(&pdev->dev, "No platform data (desc) found\n");
		return PTR_ERR(desc);
	}

	cm = devm_kzalloc(&pdev->dev, sizeof(*cm), GFP_KERNEL);
	if (!cm)
		return -ENOMEM;

	/* Basic Values. Unspecified are Null or 0 */
	cm->dev = &pdev->dev;
	cm->desc = desc;
	psy_cfg.drv_data = cm;

	/* Initialize alarm timer */
	if (alarmtimer_get_rtcdev()) {
		cm_timer = devm_kzalloc(cm->dev, sizeof(*cm_timer), GFP_KERNEL);
		if (!cm_timer)
			return -ENOMEM;
		alarm_init(cm_timer, ALARM_BOOTTIME, NULL);
	}

	cm->vchg_info = sprd_vchg_info_register(cm->dev);
	if (IS_ERR(cm->vchg_info)) {
		dev_info(&pdev->dev, "Fail to init vchg info\n");
		return -ENOMEM;
	}

	if (cm->vchg_info->ops && cm->vchg_info->ops->parse_dts &&
	    cm->vchg_info->ops->parse_dts(cm->vchg_info)) {
		dev_err(&pdev->dev, "failed to parse sprd vchg parameters\n");
		return -EPROBE_DEFER;
	}

	cm->fchg_info = sprd_fchg_info_register(cm->dev);
	if (IS_ERR(cm->fchg_info)) {
		dev_err(&pdev->dev, "Fail to register fchg info\n");
		return -ENOMEM;
	}

	/*
	 * Some of the following do not need to be errors.
	 * Users may intentionally ignore those features.
	 */

	if (!desc->fullbatt_vchkdrop_ms || !desc->fullbatt_vchkdrop_uV) {
		dev_info(&pdev->dev, "Disabling full-battery voltage drop checking mechanism as it is not supplied\n");
		desc->fullbatt_vchkdrop_ms = 0;
		desc->fullbatt_vchkdrop_uV = 0;
	}
	if (desc->fullbatt_soc == 0)
		dev_info(&pdev->dev, "Ignoring full-battery soc(state of charge) threshold as it is not supplied\n");

	if (desc->fullbatt_full_capacity == 0)
		dev_info(&pdev->dev, "Ignoring full-battery full capacity threshold as it is not supplied\n");

	if (!desc->psy_charger_stat || !desc->psy_charger_stat[0]) {
		dev_err(&pdev->dev, "No power supply defined\n");
		return -EINVAL;
	}

	if (!desc->psy_fuel_gauge) {
		dev_err(&pdev->dev, "No fuel gauge power supply defined\n");
		return -EINVAL;
	}

	ret = get_boot_mode();
	if (ret) {
		pr_err("boot_mode can't not parse bootargs property\n");
		return ret;
	}

	/* Check if charger's supplies are present at probe */
	for (i = 0; desc->psy_charger_stat[i]; i++) {
		struct power_supply *psy;

		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(&pdev->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			return -EPROBE_DEFER;
		}
		power_supply_put(psy);
	}

	if (desc->enable_alt_cp_adapt && (desc->psy_cp_nums > 0)) {
		ret = cm_check_alt_cp_psy_ready_status(cm);
		if (ret < 0) {
			dev_err(&pdev->dev, "can't find cp\n");
			return ret;
		}
	}

	if (cm->desc->polling_mode != CM_POLL_DISABLE &&
	    (desc->polling_interval_ms == 0 ||
	     msecs_to_jiffies(desc->polling_interval_ms) <= CM_JIFFIES_SMALL)) {
		dev_err(&pdev->dev, "polling_interval_ms is too small\n");
		return -EINVAL;
	}

	if (!desc->charging_max_duration_ms ||
			!desc->discharging_max_duration_ms) {
		dev_info(&pdev->dev, "Cannot limit charging duration checking mechanism to prevent overcharge/overheat and control discharging duration\n");
		desc->charging_max_duration_ms = 0;
		desc->discharging_max_duration_ms = 0;
	}

	if (!desc->charge_voltage_max || !desc->charge_voltage_drop) {
		dev_info(&pdev->dev, "Cannot validate charge voltage\n");
		desc->charge_voltage_max = 0;
		desc->charge_voltage_drop = 0;
	}

	platform_set_drvdata(pdev, cm);

	memcpy(&cm->charger_psy_desc, &psy_default, sizeof(psy_default));

	if (!desc->psy_name)
		strncpy(cm->psy_name_buf, psy_default.name, PSY_NAME_MAX);
	else
		strncpy(cm->psy_name_buf, desc->psy_name, PSY_NAME_MAX);
	cm->charger_psy_desc.name = cm->psy_name_buf;

	/* Allocate for psy properties because they may vary */
	properties = devm_kcalloc(&pdev->dev,
			     ARRAY_SIZE(default_charger_props) +
				NUM_CHARGER_PSY_OPTIONAL,
			     sizeof(*properties), GFP_KERNEL);
	if (!properties)
		return -ENOMEM;

	memcpy(properties, default_charger_props,
		sizeof(enum power_supply_property) *
		ARRAY_SIZE(default_charger_props));
	num_properties = ARRAY_SIZE(default_charger_props);

	/* Find which optional psy-properties are available */
	fuel_gauge = power_supply_get_by_name(desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(&pdev->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_fuel_gauge);
		return -EPROBE_DEFER;
	}

	if (!power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CHARGE_NOW, &val)) {
		if (!cm_add_battery_psy_property(cm, POWER_SUPPLY_PROP_CHARGE_NOW, properties, &num_properties))
			dev_warn(&pdev->dev, "POWER_SUPPLY_PROP_CHARGE_NOW is present\n");
	}

	val.intval = CM_IBAT_CURRENT_NOW_CMD;
	if (!power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CURRENT_NOW, &val)) {
		if (!cm_add_battery_psy_property(cm, POWER_SUPPLY_PROP_CURRENT_NOW, properties, &num_properties))
			dev_warn(&pdev->dev, "POWER_SUPPLY_PROP_CURRENT_NOW is present\n");
	}

	ret = get_boot_cap(cm, &cm->desc->cap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get initial battery capacity\n");
		return ret;
	}
	if (device_property_read_bool(&pdev->dev, "cm-keep-awake"))
		cm->desc->keep_awake = true;
	cm->desc->thm_info.thm_adjust_cur = -EINVAL;
	cm->desc->ir_comp.ibat_buf[CM_IBAT_BUFF_CNT - 1] = CM_MAGIC_NUM;
	cm->desc->ir_comp.us_lower_limit = cm->desc->ir_comp.us;

	if (device_property_read_bool(&pdev->dev, "cm-support-linear-charge"))
		cm->desc->thm_info.need_calib_charge_lmt = true;

	ret = cm_get_battery_temperature_by_psy(cm, &cm->desc->temperature);
	if (ret) {
		dev_err(cm->dev, "failed to get battery temperature\n");
		return ret;
	}

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	cm->desc->update_capacity_time = cur_time.tv_sec;
	cm->desc->last_query_time = cur_time.tv_sec;

	ret = cm_init_thermal_data(cm, fuel_gauge, properties, &num_properties);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize thermal data\n");
		cm->desc->measure_battery_temp = false;
	}
	power_supply_put(fuel_gauge);

	cm->charger_psy_desc.properties = properties;
	cm->charger_psy_desc.num_properties = num_properties;

	INIT_DELAYED_WORK(&cm->fullbatt_vchk_work, fullbatt_vchk);
	INIT_DELAYED_WORK(&cm->cap_update_work, cm_batt_works);
	INIT_DELAYED_WORK(&cm->fixed_fchg_work, cm_fixed_fchg_work);
	INIT_DELAYED_WORK(&cm->cp_work, cm_cp_work);
	INIT_DELAYED_WORK(&cm->ir_compensation_work, cm_ir_compensation_works);

	mutex_init(&cm->desc->charge_info_mtx);

	/* Register sysfs entry for charger */
	ret = charger_manager_prepare_sysfs(cm);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Cannot prepare sysfs entry of regulators\n");
		return ret;
	}

	if (!desc->sysfs || desc->num_sysfs < 1) {
		dev_err(&pdev->dev, "sysfs undefined\n");
		return -EINVAL;
	}

	psy_cfg.attr_grp = desc->sysfs_groups;
	psy_cfg.of_node = np;

	cm->charger_psy = power_supply_register(&pdev->dev,
						&cm->charger_psy_desc,
						&psy_cfg);
	if (IS_ERR(cm->charger_psy)) {
		dev_err(&pdev->dev, "Cannot register charger-manager with name \"%s\"\n",
			cm->charger_psy_desc.name);
		return PTR_ERR(cm->charger_psy);
	}
	cm->charger_psy->supplied_to = charger_manager_supplied_to;
	cm->charger_psy->num_supplicants =
		ARRAY_SIZE(charger_manager_supplied_to);

	wireless_main.cm = cm;
	wireless_main.psy = power_supply_register(&pdev->dev, &wireless_main.psd, NULL);
	if (IS_ERR(wireless_main.psy)) {
		dev_err(&pdev->dev, "Cannot register wireless_main.psy with name \"%s\"\n",
			wireless_main.psd.name);
		return PTR_ERR(wireless_main.psy);

	}

	ac_main.cm = cm;
	ac_main.psy = power_supply_register(&pdev->dev, &ac_main.psd, NULL);
	if (IS_ERR(ac_main.psy)) {
		dev_err(&pdev->dev, "Cannot register usb_main.psy with name \"%s\"\n",
			ac_main.psd.name);
		return PTR_ERR(ac_main.psy);

	}

	usb_main.cm = cm;
	usb_main.psy = power_supply_register(&pdev->dev, &usb_main.psd, NULL);
	if (IS_ERR(usb_main.psy)) {
		dev_err(&pdev->dev, "Cannot register usb_main.psy with name \"%s\"\n",
			usb_main.psd.name);
		return PTR_ERR(usb_main.psy);

	}

	mutex_init(&cm->desc->keep_awake_mtx);

	/* Add to the list */
	mutex_lock(&cm_list_mtx);
	list_add(&cm->entry, &cm_list);
	mutex_unlock(&cm_list_mtx);

	/*
	 * Charger-manager is capable of waking up the system from sleep
	 * when event is happened through cm_notify_event()
	 */
	device_init_wakeup(&pdev->dev, true);
	device_set_wakeup_capable(&pdev->dev, false);
	cm->charge_ws = wakeup_source_create("charger_manager_wakelock");
	wakeup_source_add(cm->charge_ws);
	cm->cp_ws = wakeup_source_create("charger_pump_wakelock");
	wakeup_source_add(cm->cp_ws);
	mutex_init(&cm->desc->charger_type_mtx);

	ret = cm_get_bat_info(cm);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get battery information\n");
		goto err;
	}

	cm->cm_charge_vote = sprd_charge_vote_register("cm_charge_vote",
						       cm_sprd_vote_callback,
						       cm,
						       &cm->charger_psy->dev);
	if (IS_ERR(cm->cm_charge_vote)) {
		dev_err(&pdev->dev, "Failed to register charge vote\n");
		ret = PTR_ERR(cm->cm_charge_vote);
		goto err;
	}

	cm_init_basp_parameter(cm);

	if (cm->fchg_info->ops && cm->fchg_info->ops->extcon_init &&
	    cm->fchg_info->ops->extcon_init(cm->fchg_info, cm->charger_psy)) {
		dev_err(&pdev->dev, "Failed to initialize fchg extcon\n");
		ret = -EPROBE_DEFER;
		goto err;
	}

	if (cm->vchg_info->ops && cm->vchg_info->ops->init &&
	    cm->vchg_info->ops->init(cm->vchg_info, cm->charger_psy)) {
		dev_err(&pdev->dev, "Failed to register vchg detect notify\n");
		ret = -EPROBE_DEFER;
		goto err;
	}

	if (is_ext_usb_pwr_online(cm) && cm->fchg_info->ops && cm->fchg_info->ops->fchg_detect)
		cm->fchg_info->ops->fchg_detect(cm->fchg_info);

	if (cm_event_num > 0) {
		for (i = 0; i < cm_event_num; i++)
			cm_notify_type_handle(cm, cm_event_type[i], cm_event_msg[i]);
		cm_event_num = 0;
	}
	/*
	 * Charger-manager have to check the charging state right after
	 * initialization of charger-manager and then update current charging
	 * state.
	 */
	cm_monitor();

	schedule_work(&setup_polling);

	queue_delayed_work(system_power_efficient_wq, &cm->cap_update_work, CM_CAP_CYCLE_TRACK_TIME * HZ);
	INIT_DELAYED_WORK(&cm->uvlo_work, cm_uvlo_check_work);

	return 0;

err:

	power_supply_unregister(cm->charger_psy);
	wakeup_source_remove(cm->charge_ws);

	return ret;
}

static int charger_manager_remove(struct platform_device *pdev)
{
	struct charger_manager *cm = platform_get_drvdata(pdev);

	/* Remove from the list */
	mutex_lock(&cm_list_mtx);
	list_del(&cm->entry);
	mutex_unlock(&cm_list_mtx);

	cancel_work_sync(&setup_polling);
	cancel_delayed_work_sync(&cm_monitor_work);
	cancel_delayed_work_sync(&cm->cap_update_work);
	cancel_delayed_work_sync(&cm->fullbatt_vchk_work);
	cancel_delayed_work_sync(&cm->uvlo_work);

	power_supply_unregister(cm->charger_psy);

	try_charger_enable(cm, false);
	wakeup_source_remove(cm->charge_ws);

	return 0;
}

static void charger_manager_shutdown(struct platform_device *pdev)
{
	struct charger_manager *cm = platform_get_drvdata(pdev);

	if (cm->desc->uvlo_trigger_cnt < CM_UVLO_CALIBRATION_CNT_THRESHOLD)
		set_batt_cap(cm, cm->desc->cap);

	cancel_delayed_work_sync(&cm_monitor_work);
	cancel_delayed_work_sync(&cm->fullbatt_vchk_work);
	cancel_delayed_work_sync(&cm->cap_update_work);
}

static const struct platform_device_id charger_manager_id[] = {
	{ "charger-manager", 0 },
	{ },
};
MODULE_DEVICE_TABLE(platform, charger_manager_id);

static int cm_suspend_noirq(struct device *dev)
{
	if (device_may_wakeup(dev)) {
		device_set_wakeup_capable(dev, false);
		return -EAGAIN;
	}

	return 0;
}

static int cm_suspend_prepare(struct device *dev)
{
	struct charger_manager *cm = dev_get_drvdata(dev);

	if (!cm_suspended)
		cm_suspended = true;

	cm_timer_set = cm_setup_timer();

	if (cm_timer_set) {
		cancel_work_sync(&setup_polling);
		cancel_delayed_work_sync(&cm_monitor_work);
		cancel_delayed_work(&cm->fullbatt_vchk_work);
		cancel_delayed_work_sync(&cm->cap_update_work);
		cancel_delayed_work_sync(&cm->uvlo_work);
	}

	return 0;
}

static void cm_suspend_complete(struct device *dev)
{
	struct charger_manager *cm = dev_get_drvdata(dev);

	if (cm_suspended)
		cm_suspended = false;

	if (cm_timer_set) {
		ktime_t remain;

		alarm_cancel(cm_timer);
		cm_timer_set = false;
		remain = alarm_expires_remaining(cm_timer);
		if (remain > 0)
			cm_suspend_duration_ms -= ktime_to_ms(remain);
		schedule_work(&setup_polling);
	}

	_cm_monitor(cm);
	cm_batt_works(&cm->cap_update_work.work);

	/* Re-enqueue delayed work (fullbatt_vchk_work) */
	if (cm->fullbatt_vchk_jiffies_at) {
		unsigned long delay = 0;
		unsigned long now = jiffies + CM_JIFFIES_SMALL;

		if (time_after_eq(now, cm->fullbatt_vchk_jiffies_at)) {
			delay = (unsigned long)((long)now
				- (long)(cm->fullbatt_vchk_jiffies_at));
			delay = jiffies_to_msecs(delay);
		} else {
			delay = 0;
		}

		/*
		 * Account for cm_suspend_duration_ms with assuming that
		 * timer stops in suspend.
		 */
		if (delay > cm_suspend_duration_ms)
			delay -= cm_suspend_duration_ms;
		else
			delay = 0;

		queue_delayed_work(cm_wq, &cm->fullbatt_vchk_work,
				   msecs_to_jiffies(delay));
	}
	device_set_wakeup_capable(cm->dev, false);
}

static const struct dev_pm_ops charger_manager_pm = {
	.prepare	= cm_suspend_prepare,
	.suspend_noirq	= cm_suspend_noirq,
	.complete	= cm_suspend_complete,
};

static struct platform_driver charger_manager_driver = {
	.driver = {
		.name = "charger-manager",
		.pm = &charger_manager_pm,
		.of_match_table = charger_manager_match,
	},
	.probe = charger_manager_probe,
	.remove = charger_manager_remove,
	.shutdown = charger_manager_shutdown,
	.id_table = charger_manager_id,
};

static int __init charger_manager_init(void)
{
	cm_wq = create_freezable_workqueue("charger_manager");
	if (unlikely(!cm_wq))
		return -ENOMEM;

	INIT_DELAYED_WORK(&cm_monitor_work, cm_monitor_poller);

	return platform_driver_register(&charger_manager_driver);
}
late_initcall(charger_manager_init);

static void __exit charger_manager_cleanup(void)
{
	destroy_workqueue(cm_wq);
	cm_wq = NULL;

	platform_driver_unregister(&charger_manager_driver);
}
module_exit(charger_manager_cleanup);

/**
 * cm_notify_type_handle - charger driver handle charger event
 * @cm: the Charger Manager representing the battery
 * @type: type of charger event
 * @msg: optional message passed to uevent_notify function
 */
static void cm_notify_type_handle(struct charger_manager *cm, enum cm_event_types type, char *msg)
{
	switch (type) {
	case CM_EVENT_BATT_FULL:
		fullbatt_handler(cm);
		break;
	case CM_EVENT_BATT_IN:
	case CM_EVENT_BATT_OUT:
		battout_handler(cm);
		break;
	case CM_EVENT_WL_CHG_START_STOP:
	case CM_EVENT_EXT_PWR_IN_OUT ... CM_EVENT_CHG_START_STOP:
		misc_event_handler(cm, type);
		break;
	case CM_EVENT_FAST_CHARGE:
		fast_charge_handler(cm);
		break;
	case CM_EVENT_INT:
		cm_charger_int_handler(cm);
		break;
	case CM_EVENT_BATT_OVERVOLTAGE:
		mod_delayed_work(cm_wq, &cm_monitor_work, 0);
		break;
	case CM_EVENT_UNKNOWN:
	case CM_EVENT_OTHERS:
	default:
		dev_err(cm->dev, "%s: type not specified\n", __func__);
		break;
	}

	power_supply_changed(cm->charger_psy);

}

/**
 * cm_notify_event - charger driver notify Charger Manager of charger event
 * @psy: pointer to instance of charger's power_supply
 * @type: type of charger event
 * @msg: optional message passed to uevent_notify function
 */
void cm_notify_event(struct power_supply *psy, enum cm_event_types type,
		     char *msg)
{
	struct charger_manager *cm;
	bool found_power_supply = false;

	if (psy == NULL)
		return;

	mutex_lock(&cm_list_mtx);
	list_for_each_entry(cm, &cm_list, entry) {
		if (cm->charger_psy->desc) {
			if (strcmp(psy->desc->name, cm->charger_psy->desc->name) == 0) {
				found_power_supply = true;
				break;
			}
		}

		if (cm->desc->psy_charger_stat) {
			if (match_string(cm->desc->psy_charger_stat, -1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}

		if (cm->desc->psy_fuel_gauge) {
			/*
			 * fgu has only one string and no null pointer at the end,
			 * only needs to compare once before exiting th loop, so 1 here and -1 elsewhere.
			 */
			if (match_string(&cm->desc->psy_fuel_gauge, 1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}

		if (cm->desc->psy_cp_stat) {
			if (match_string(cm->desc->psy_cp_stat, -1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}

		if (cm->desc->psy_wl_charger_stat) {
			if (match_string(cm->desc->psy_wl_charger_stat, -1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}
	}

	mutex_unlock(&cm_list_mtx);

	if (!found_power_supply || !cm->cm_charge_vote) {
		if (cm_event_num < CM_EVENT_TYPE_NUM) {
			cm_event_msg[cm_event_num] = msg;
			cm_event_type[cm_event_num++] = type;
		} else {
			pr_err("%s: too many cm_event_num!!\n", __func__);
		}
		return;
	}

	cm_notify_type_handle(cm, type, msg);
}
EXPORT_SYMBOL_GPL(cm_notify_event);

MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_DESCRIPTION("Charger Manager");
MODULE_LICENSE("GPL");
