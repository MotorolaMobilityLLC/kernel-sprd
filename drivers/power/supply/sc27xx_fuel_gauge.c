// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 Spreadtrum Communications Inc.

#include <linux/gpio/consumer.h>
#include <linux/iio/consumer.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/pm_wakeup.h>
#include <linux/usb/phy.h>

/* PMIC global control registers definition */
#define SC27XX_MODULE_EN0		0xc08
#define SC27XX_CLK_EN0			0xc18
#define SC2730_MODULE_EN0		0x1808
#define SC2730_CLK_EN0			0x1810
#define UMP9620_MODULE_EN0		0x2008
#define UMP9620_CLK_EN0			0x2010
#define SC2720_MODULE_EN0		0xc08
#define SC2720_CLK_EN0			0xc10
#define SC27XX_FGU_EN			BIT(7)
#define SC27XX_FGU_RTC_EN		BIT(6)

/* FGU registers definition */
#define SC27XX_FGU_START		0x0
#define SC27XX_FGU_CONFIG		0x4
#define SC27XX_FGU_ADC_CONFIG		0x8
#define SC27XX_FGU_STATUS		0xc
#define SC27XX_FGU_INT_EN		0x10
#define SC27XX_FGU_INT_CLR		0x14
#define SC27XX_FGU_INT_STS		0x1c
#define SC27XX_FGU_VOLTAGE		0x20
#define SC27XX_FGU_OCV			0x24
#define SC27XX_FGU_POCV			0x28
#define SC27XX_FGU_CURRENT		0x2c
#define SC27XX_FGU_LOW_OVERLOAD		0x34
#define SC27XX_FGU_CLBCNT_SETH		0x50
#define SC27XX_FGU_CLBCNT_SETL		0x54
#define SC27XX_FGU_CLBCNT_DELTH		0x58
#define SC27XX_FGU_CLBCNT_DELTL		0x5c
#define SC27XX_FGU_CLBCNT_VALH		0x68
#define SC27XX_FGU_CLBCNT_VALL		0x6c
#define SC27XX_FGU_CLBCNT_QMAXL		0x74
#define SC27XX_FGU_RELAX_CURT_THRE	0x80
#define SC27XX_FGU_RELAX_CNT_THRE	0x84
#define SC27XX_FGU_USER_AREA_SET	0xa0
#define SC27XX_FGU_USER_AREA_CLEAR	0xa4
#define SC27XX_FGU_USER_AREA_STATUS	0xa8
#define SC27XX_FGU_USER_AREA_SET1	0xc0
#define SC27XX_FGU_USER_AREA_CLEAR1	0xc4
#define SC27XX_FGU_USER_AREA_STATUS1	0xc8
#define SC27XX_FGU_VOLTAGE_BUF		0xd0
#define SC27XX_FGU_CURRENT_BUF		0xf0
#define SC27XX_FGU_REG_MAX		0x260

/* SC27XX_FGU_CONFIG */
#define SC27XX_FGU_LOW_POWER_MODE	BIT(1)
#define SC27XX_FGU_RELAX_CNT_MODE	0
#define SC27XX_FGU_DEEP_SLEEP_MODE	1

#define SC27XX_WRITE_SELCLB_EN		BIT(0)
#define SC27XX_FGU_CLBCNT_MASK		GENMASK(15, 0)
#define SC27XX_FGU_CLBCNT_SHIFT		16
#define SC27XX_FGU_LOW_OVERLOAD_MASK	GENMASK(12, 0)

#define SC27XX_FGU_INT_MASK		GENMASK(9, 0)
#define SC27XX_FGU_LOW_OVERLOAD_INT	BIT(0)
#define SC27XX_FGU_CLBCNT_DELTA_INT	BIT(2)
#define SC27XX_FGU_RELX_CNT_INT		BIT(3)
#define SC27XX_FGU_RELX_CNT_STS		BIT(3)

#define SC27XX_FGU_MODE_AREA_MASK	GENMASK(15, 12)
#define SC27XX_FGU_CAP_AREA_MASK	GENMASK(11, 0)
#define SC27XX_FGU_MODE_AREA_SHIFT	12
#define SC27XX_FGU_CAP_INTEGER_MASK	GENMASK(7, 0)
#define SC27XX_FGU_CAP_DECIMAL_MASK	GENMASK(3, 0)
#define SC27XX_FGU_CAP_DECIMAL_SHIFT	8

#define SC27XX_FGU_FIRST_POWERTON	GENMASK(3, 0)
#define SC27XX_FGU_DEFAULT_CAP		GENMASK(11, 0)
#define SC27XX_FGU_NORMAIL_POWERTON	0x5
#define SC27XX_FGU_RTC2_RESET_VALUE	0xA05
#define SC27XX_FGU_NORMAL_TO_FIRST_POWERON	0xA

#define SC27XX_FGU_CUR_BASIC_ADC	8192
#define SC27XX_FGU_POCV_VOLT_THRESHOLD	3400
#define SC27XX_FGU_SAMPLE_HZ		2
#define SC27XX_FGU_TEMP_BUFF_CNT	10
#define SC27XX_FGU_LOW_TEMP_REGION	100

/* micro Ohms */
#define SC27XX_FGU_IDEAL_RESISTANCE	20000
#define SC27XX_FGU_LOW_VBAT_REGION	3300
#define SC27XX_FGU_LOW_VBAT_REC_REGION	3400
#define SC27XX_FGU_RELAX_CNT_THRESHOLD	320
#define SC27XX_FGU_RELAX_CUR_THRESHOLD_MA	30
#define SC27XX_FGU_SLP_CAP_CALIB_SLP_TIME	300
#define SC27XX_FGU_SLP_CAP_CALIB_TEMP_LOW	100
#define SC27XX_FGU_SLP_CAP_CALIB_TEMP_HI	450

/* Efuse fgu calibration bit definition */
#define SC2720_FGU_CAL			GENMASK(8, 0)
#define SC2720_FGU_CAL_SHIFT		0
#define SC2730_FGU_CAL			GENMASK(8, 0)
#define SC2730_FGU_CAL_SHIFT		0
#define SC2731_FGU_CAL			GENMASK(8, 0)
#define SC2731_FGU_CAL_SHIFT		0
#define UMP9620_FGU_CAL			GENMASK(15, 7)
#define UMP9620_FGU_CAL_SHIFT		7

/* SC27XX_FGU_RELAX_CURT_THRE */
#define SC2720_FGU_RELAX_CURT_THRE_MASK		GENMASK(13, 0)
#define SC2720_FGU_RELAX_CURT_THRE_SHITF	0
/* SC27XX_FGU_RELAX_CNT_THRE */
#define SC2720_FGU_RELAX_CNT_THRE_MASK		GENMASK(12, 0)
#define SC2720_FGU_RELAX_CNT_THRE_SHITF		0

#define SC27XX_FGU_CURRENT_BUFF_CNT	8
#define SC27XX_FGU_DISCHG_CNT		4
#define SC27XX_FGU_VOLTAGE_BUFF_CNT	8
#define SC27XX_FGU_MAGIC_NUMBER		0x5a5aa5a5
#define SC27XX_FGU_DEBUG_EN_CMD		0x5a5aa5a5
#define SC27XX_FGU_DEBUG_DIS_CMD	0x5a5a5a5a
#define SC27XX_FGU_FCC_PERCENT		1000

#define SC27XX_FGU_TRACK_CAP_START_VOLTAGE		3650
#define SC27XX_FGU_TRACK_CAP_START_CURRENT		50
#define SC27XX_FGU_TRACK_CAP_KEY0			0x20160726
#define SC27XX_FGU_TRACK_CAP_KEY1			0x15211517
#define SC27XX_FGU_TRACK_HIGH_TEMP_THRESHOLD		450
#define SC27XX_FGU_TRACK_LOW_TEMP_THRESHOLD		150
#define SC27XX_FGU_TRACK_TIMEOUT_THRESHOLD		108000
#define SC27XX_FGU_TRACK_START_CAP_THRESHOLD		200
#define SC27XX_FGU_TRACK_WAKE_UP_MS			25000
#define SC27XX_FGU_TRACK_FILE_PATH "/mnt/vendor/battery/calibration_data/.battery_file"

#define interpolate(x, x1, y1, x2, y2) \
	((y1) + ((((y2) - (y1)) * ((x) - (x1))) / ((x2) - (x1))))

struct power_supply_vol_temp_table {
	int vol;	/* microVolts */
	int temp;	/* celsius */
};

struct power_supply_capacity_temp_table {
	int temp;	/* celsius */
	int cap;	/* capacity percentage */
};

enum sc27xx_fgu_track_state {
	CAP_TRACK_INIT,
	CAP_TRACK_IDLE,
	CAP_TRACK_UPDATING,
	CAP_TRACK_DONE,
	CAP_TRACK_ERR,
};

struct sc27xx_fgu_track_capacity {
	enum sc27xx_fgu_track_state state;
	bool clear_cap_flag;
	int start_clbcnt;
	int start_cap;
	int end_vol;
	int end_cur;
	s64 start_time;
	bool cap_tracking;
	struct delayed_work track_capacity_work;
	struct delayed_work fgu_update_work;
};

struct sc27xx_fgu_debug_info {
	bool temp_debug_en;
	bool vbat_now_debug_en;
	bool ocv_debug_en;
	bool cur_now_debug_en;
	bool batt_present_debug_en;
	bool chg_vol_debug_en;
	bool batt_health_debug_en;

	int debug_temp;
	int debug_vbat_now;
	int debug_ocv;
	int debug_cur_now;
	bool debug_batt_present;
	int debug_chg_vol;
	int debug_batt_health;

	int sel_reg_id;
};

struct sc27xx_fgu_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_sc27xx_fgu_dump_info;
	struct device_attribute attr_sc27xx_fgu_sel_reg_id;
	struct device_attribute attr_sc27xx_fgu_reg_val;
	struct device_attribute attr_sc27xx_fgu_enable_sleep_calib;
	struct device_attribute attr_sc27xx_fgu_relax_cnt_th;
	struct device_attribute attr_sc27xx_fgu_relax_cur_th;
	struct attribute *attrs[7];

	struct sc27xx_fgu_data *data;
};

struct sc27xx_fgu_energy_density_ocv_table {
	int engy_dens_ocv_hi;
	int engy_dens_ocv_lo;
};

struct sc27xx_fgu_sleep_capacity_calibration {
	bool support_slp_calib;
	int suspend_ocv;
	int resume_ocv;
	int suspend_clbcnt;
	int resume_clbcnt;
	u64 suspend_time;
	u64 resume_time;
	int resume_ocv_cap;

	int relax_cnt_threshold;
	int relax_cur_threshold;

	bool relax_cnt_int_ocurred;
};

/*
 * struct sc27xx_fgu_data: describe the FGU device
 * @regmap: regmap for register access
 * @dev: platform device
 * @battery: battery power supply
 * @base: the base offset for the controller
 * @lock: protect the structure
 * @gpiod: GPIO for battery detection
 * @channel: IIO channel to get battery temperature
 * @charge_chan: IIO channel to get charge voltage
 * @internal_resist: the battery internal resistance in mOhm
 * @total_cap: the total capacity of the battery in mAh
 * @init_cap: the initial capacity of the battery in mAh
 * @alarm_cap: the alarm capacity
 * @normal_temp_cap: the normal temperature capacity
 * @init_clbcnt: the initial coulomb counter
 * @max_volt: the maximum constant input voltage in millivolt
 * @min_volt: the minimum drained battery voltage in microvolt
 * @boot_volt: the voltage measured during boot in microvolt
 * @table_len: the capacity table length
 * @temp_table_len: temp_table length
 * @cap_table_len：the capacity temperature table length
 * @resist_table_len: the resistance table length
 * @cur_1000ma_adc: ADC value corresponding to 1000 mA
 * @vol_1000mv_adc: ADC value corresponding to 1000 mV
 * @calib_resist: the real resistance of coulomb counter chip in uOhm
 * @comp_resistance: the coulomb counter internal and the board ground resistance
 * @index: record temp_buff array index
 * @temp_buff: record the battery temperature for each measurement
 * @bat_temp: the battery temperature
 * @cap_table: capacity table with corresponding ocv
 * @temp_table: the NTC voltage table with corresponding battery temperature
 * @cap_temp_table: the capacity table with corresponding temperature
 * @resist_table: resistance percent table with corresponding temperature
 */
struct sc27xx_fgu_data {
	struct regmap *regmap;
	struct device *dev;
	struct power_supply *battery;
	u32 base;
	struct mutex lock;
	struct gpio_desc *gpiod;
	struct iio_channel *channel;
	struct iio_channel *charge_chan;
	bool bat_present;
	int internal_resist;
	int total_cap;
	int init_cap;
	int alarm_cap;
	int boot_cap;
	int normal_temp_cap;
	int init_clbcnt;
	int max_volt;
	int min_volt;
	int boot_volt;
	int table_len;
	int temp_table_len;
	int cap_table_len;
	int resist_table_len;
	int dens_table_len;
	int cur_1000ma_adc;
	int vol_1000mv_adc;
	int calib_resist;
	int first_calib_volt;
	int first_calib_cap;
	int uusoc_vbat;
	unsigned int comp_resistance;
	int index;
	int ocv;
	int batt_uV;
	int temp_buff[SC27XX_FGU_TEMP_BUFF_CNT];
	int cur_now_buff[SC27XX_FGU_CURRENT_BUFF_CNT];
	bool dischg_trend[SC27XX_FGU_DISCHG_CNT];
	int last_clbcnt;
	int cur_clbcnt;
	int bat_temp;
	bool online;
	bool is_first_poweron;
	u32 chg_type;
	struct sc27xx_fgu_track_capacity track;
	struct power_supply_battery_ocv_table *cap_table;
	struct power_supply_vol_temp_table *temp_table;
	struct power_supply_capacity_temp_table *cap_temp_table;
	struct power_supply_resistance_temp_table *resist_table;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	const struct sc27xx_fgu_variant_data *pdata;
	struct sc27xx_fgu_debug_info debug_info;
	struct sc27xx_fgu_sleep_capacity_calibration slp_cap_calib;
	struct sc27xx_fgu_energy_density_ocv_table *dens_table;
	struct sc27xx_fgu_sysfs *sysfs;
};

struct sc27xx_fgu_variant_data {
	u32 module_en;
	u32 clk_en;
	u32 fgu_cal;
	u32 fgu_cal_shift;
};

static const struct sc27xx_fgu_variant_data sc2731_info = {
	.module_en = SC27XX_MODULE_EN0,
	.clk_en = SC27XX_CLK_EN0,
	.fgu_cal = SC2731_FGU_CAL,
	.fgu_cal_shift = SC2731_FGU_CAL_SHIFT,
};

static const struct sc27xx_fgu_variant_data sc2730_info = {
	.module_en = SC2730_MODULE_EN0,
	.clk_en = SC2730_CLK_EN0,
	.fgu_cal = SC2730_FGU_CAL,
	.fgu_cal_shift = SC2730_FGU_CAL_SHIFT,
};

static const struct sc27xx_fgu_variant_data ump9620_info = {
	.module_en = UMP9620_MODULE_EN0,
	.clk_en = UMP9620_CLK_EN0,
	.fgu_cal = UMP9620_FGU_CAL,
	.fgu_cal_shift = UMP9620_FGU_CAL_SHIFT,
};

static const struct sc27xx_fgu_variant_data sc2720_info = {
	.module_en = SC2720_MODULE_EN0,
	.clk_en = SC2720_CLK_EN0,
	.fgu_cal = SC2720_FGU_CAL,
	.fgu_cal_shift = SC2720_FGU_CAL_SHIFT,
};

static bool is_charger_mode;

static int get_boot_mode(void)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (ret)
		return ret;

	if (!strncmp(cmd_line, "charger", strlen("charger")))
		is_charger_mode =  true;

	return 0;
}

static int sc27xx_fgu_cap_to_clbcnt(struct sc27xx_fgu_data *data, int capacity);
static void sc27xx_fgu_capacity_calibration(struct sc27xx_fgu_data *data, bool int_mode);
static void sc27xx_fgu_adjust_cap(struct sc27xx_fgu_data *data, int cap);
static int sc27xx_fgu_get_temp(struct sc27xx_fgu_data *data, int *temp);
static int sc27xx_fgu_get_vbat_ocv(struct sc27xx_fgu_data *data, int *val);
static int sc27xx_fgu_get_vbat_now(struct sc27xx_fgu_data *data, int *val);

static const char * const sc27xx_charger_supply_name[] = {
	"sc2731_charger",
	"sc2720_charger",
	"sc2721_charger",
	"sc2723_charger",
	"sc2703_charger",
	"fan54015_charger",
	"bq2560x_charger",
	"bq25890_charger",
	"bq25910_charger",
	"eta6937_charger",
	"aw32257",
};

static int sc27xx_fgu_adc_to_current(struct sc27xx_fgu_data *data, s64 adc)
{
	return DIV_S64_ROUND_CLOSEST(adc * 1000, data->cur_1000ma_adc);
}

static int sc27xx_fgu_adc_to_voltage(struct sc27xx_fgu_data *data, s64 adc)
{
	return DIV_S64_ROUND_CLOSEST(adc * 1000, data->vol_1000mv_adc);
}

static int sc27xx_fgu_voltage_to_adc(struct sc27xx_fgu_data *data, int vol)
{
	return DIV_ROUND_CLOSEST(vol * data->vol_1000mv_adc, 1000);
}

static int sc27xx_fgu_temp_to_cap(struct power_supply_capacity_temp_table *table,
				  int table_len, int temp)
{
	int i, capacity;

	temp = temp / 10;
	for (i = 0; i < table_len; i++)
		if (temp > table[i].temp)
			break;

	if (i > 0 && i < table_len) {
		capacity = interpolate(temp,
				   table[i].temp,
				   table[i].cap * 10,
				   table[i - 1].temp,
				   table[i - 1].cap * 10);
	} else if (i == 0) {
		capacity = table[0].cap * 10;
	} else {
		capacity = table[table_len - 1].cap * 10;
	}

	return DIV_ROUND_UP(capacity, 10);
}

static bool sc27xx_fgu_is_first_poweron(struct sc27xx_fgu_data *data)
{
	int ret, status = 0, cap, mode;

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_USER_AREA_STATUS, &status);
	if (ret)
		return false;

	/*
	 * We use low 4 bits to save the last battery capacity and high 12 bits
	 * to save the system boot mode.
	 */
	mode = (status & SC27XX_FGU_MODE_AREA_MASK) >> SC27XX_FGU_MODE_AREA_SHIFT;
	cap = status & SC27XX_FGU_CAP_AREA_MASK;

	/*
	 * When FGU has been powered down, the user area registers became
	 * default value (0xffff), which can be used to valid if the system is
	 * first power on or not.
	 */
	if (mode == SC27XX_FGU_FIRST_POWERTON || cap == SC27XX_FGU_DEFAULT_CAP ||
	    mode == SC27XX_FGU_NORMAL_TO_FIRST_POWERON)
		return true;

	return false;
}

static int sc27xx_fgu_save_boot_mode(struct sc27xx_fgu_data *data,
				     int boot_mode)
{
	int ret;

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_CLEAR,
				 SC27XX_FGU_MODE_AREA_MASK,
				 SC27XX_FGU_MODE_AREA_MASK);
	if (ret)
		return ret;

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully according to the datasheet.
	 */
	usleep_range(200, 210);

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_SET,
				 SC27XX_FGU_MODE_AREA_MASK,
				 boot_mode << SC27XX_FGU_MODE_AREA_SHIFT);
	if (ret)
		return ret;

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully according to the datasheet.
	 */
	usleep_range(200, 210);

	/*
	 * According to the datasheet, we should set the USER_AREA_CLEAR to 0 to
	 * make the user area data available, otherwise we can not save the user
	 * area data.
	 */
	return regmap_update_bits(data->regmap,
				  data->base + SC27XX_FGU_USER_AREA_CLEAR,
				  SC27XX_FGU_MODE_AREA_MASK, 0);
}

static int sc27xx_fgu_save_last_cap(struct sc27xx_fgu_data *data, int cap)
{
	int ret;
	u32 value;

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_CLEAR,
				 SC27XX_FGU_CAP_AREA_MASK,
				 SC27XX_FGU_CAP_AREA_MASK);
	if (ret)
		return ret;

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully according to the datasheet.
	 */
	usleep_range(200, 210);

	value = (cap / 10) & SC27XX_FGU_CAP_INTEGER_MASK;
	value |= ((cap % 10) & SC27XX_FGU_CAP_DECIMAL_MASK) << SC27XX_FGU_CAP_DECIMAL_SHIFT;

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_SET,
				 SC27XX_FGU_CAP_AREA_MASK, value);
	if (ret)
		return ret;

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully according to the datasheet.
	 */
	usleep_range(200, 210);

	/*
	 * According to the datasheet, we should set the USER_AREA_CLEAR to 0 to
	 * make the user area data available, otherwise we can not save the user
	 * area data.
	 */
	return regmap_update_bits(data->regmap,
				  data->base + SC27XX_FGU_USER_AREA_CLEAR,
				  SC27XX_FGU_CAP_AREA_MASK, 0);
}

/*
 * We get the percentage at the current temperature by multiplying
 * the percentage at normal temperature by the temperature conversion
 * factor, and save the percentage before conversion in the rtc register
 */
static int sc27xx_fgu_save_normal_temperature_cap(struct sc27xx_fgu_data *data, int cap)
{
	int ret;
	u32 value;

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_CLEAR1,
				 SC27XX_FGU_CAP_AREA_MASK,
				 SC27XX_FGU_CAP_AREA_MASK);
	if (ret)
		return ret;

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully.
	 */
	usleep_range(200, 210);

	value = (cap / 10) & SC27XX_FGU_CAP_INTEGER_MASK;
	value |= ((cap % 10) & SC27XX_FGU_CAP_DECIMAL_MASK) << SC27XX_FGU_CAP_DECIMAL_SHIFT;

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_USER_AREA_SET1,
				 SC27XX_FGU_CAP_AREA_MASK, value);
	if (ret)
		return ret;

	/*
	 * Since the user area registers are put on power always-on region,
	 * then these registers changing time will be a little long. Thus
	 * here we should delay 200us to wait until values are updated
	 * successfully.
	 */
	usleep_range(200, 210);

	return regmap_update_bits(data->regmap,
				  data->base + SC27XX_FGU_USER_AREA_CLEAR1,
				  SC27XX_FGU_CAP_AREA_MASK, 0);
}

static int sc27xx_fgu_read_normal_temperature_cap(struct sc27xx_fgu_data *data, int *cap)
{
	int ret;
	unsigned int value;

	ret = regmap_read(data->regmap,
			  data->base + SC27XX_FGU_USER_AREA_STATUS1, &value);
	if (ret)
		return ret;

	*cap = (value & SC27XX_FGU_CAP_INTEGER_MASK) * 10;
	*cap += (value >> SC27XX_FGU_CAP_DECIMAL_SHIFT) & SC27XX_FGU_CAP_DECIMAL_MASK;

	return 0;
}

static int sc27xx_fgu_read_last_cap(struct sc27xx_fgu_data *data, int *cap)
{
	int ret;
	unsigned int value = 0;

	ret = regmap_read(data->regmap,
			  data->base + SC27XX_FGU_USER_AREA_STATUS, &value);
	if (ret)
		return ret;

	*cap = (value & SC27XX_FGU_CAP_INTEGER_MASK) * 10;
	*cap += (value >> SC27XX_FGU_CAP_DECIMAL_SHIFT) & SC27XX_FGU_CAP_DECIMAL_MASK;

	return 0;
}

static int sc27xx_fgu_get_boot_voltage(struct sc27xx_fgu_data *data, int *pocv)
{
	int volt, cur, oci, ret, ocv;

	/*
	 * After system booting on, the SC27XX_FGU_CLBCNT_QMAXL register saved
	 * the first sampled open circuit current.
	 */
	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_CLBCNT_QMAXL, &cur);
	if (ret) {
		dev_err(data->dev, "Failed to read CLBCNT_QMAXL, ret = %d\n",
			ret);
		return ret;
	}

	cur <<= 1;
	oci = sc27xx_fgu_adc_to_current(data, (s64)cur - SC27XX_FGU_CUR_BASIC_ADC);

	/*
	 * Should get the OCV from SC27XX_FGU_POCV register at the system
	 * beginning. It is ADC values reading from registers which need to
	 * convert the corresponding voltage.
	 */
	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_POCV, &volt);
	if (ret) {
		dev_err(data->dev, "Failed to read FGU_POCV, ret = %d\n", ret);
		return ret;
	}

	volt = sc27xx_fgu_adc_to_voltage(data, volt);
	if (volt < SC27XX_FGU_POCV_VOLT_THRESHOLD) {
		ret = sc27xx_fgu_get_vbat_ocv(data, &ocv);
		if (ret) {
			dev_err(data->dev, "Failed to read volt, ret = %d\n", ret);
			return ret;
		}
		volt = ocv / 1000;
	}
	*pocv = volt * 1000 - oci * data->internal_resist;
	dev_info(data->dev, "oci = %d, volt = %d, pocv = %d\n", oci, volt, *pocv);

	return 0;
}

/*
 * When system boots on, we can not read battery capacity from coulomb
 * registers, since now the coulomb registers are invalid. So we should
 * calculate the battery open circuit voltage, and get current battery
 * capacity according to the capacity table.
 */
static int sc27xx_fgu_get_boot_capacity(struct sc27xx_fgu_data *data, int *cap)
{
	int ocv, ret;
	bool is_first_poweron = sc27xx_fgu_is_first_poweron(data);

	if (is_charger_mode)
		sc27xx_fgu_get_boot_voltage(data, &data->boot_volt);
	/*
	 * If system is not the first power on, we should use the last saved
	 * battery capacity as the initial battery capacity. Otherwise we should
	 * re-calculate the initial battery capacity.
	 */
	if (!is_first_poweron) {
		ret = sc27xx_fgu_read_last_cap(data, cap);
		if (ret) {
			dev_err(data->dev, "Failed to read last cap, ret = %d\n",
				ret);
			return ret;
		}

		data->boot_cap = *cap;
		ret = sc27xx_fgu_read_normal_temperature_cap(data, cap);
		if (ret) {
			dev_err(data->dev, "Failed to read normal temperature cap, ret = %d\n",
				ret);
			return ret;
		}

		if (*cap == SC27XX_FGU_DEFAULT_CAP || *cap == SC27XX_FGU_RTC2_RESET_VALUE) {
			*cap = data->boot_cap;
			ret = sc27xx_fgu_save_normal_temperature_cap(data, data->boot_cap);
			if (ret < 0)
				dev_err(data->dev, "Failed to initialize fgu user area status1 register\n");
		}
		dev_info(data->dev, "init: boot_cap = %d, normal_cap = %d\n", data->boot_cap, *cap);

		return sc27xx_fgu_save_boot_mode(data, SC27XX_FGU_NORMAIL_POWERTON);
	}

	sc27xx_fgu_get_boot_voltage(data, &ocv);
	/*
	 * Parse the capacity table to look up the correct capacity percent
	 * according to current battery's corresponding OCV values.
	 */
	*cap = power_supply_ocv2cap_simple(data->cap_table, data->table_len, ocv);

	*cap *= 10;
	data->boot_cap = *cap;
	ret = sc27xx_fgu_save_last_cap(data, *cap);
	if (ret) {
		dev_err(data->dev, "Failed to save last cap, ret = %d\n", ret);
		return ret;
	}

	data->is_first_poweron = true;
	dev_info(data->dev, "First_poweron: ocv = %d, cap = %d\n", ocv, *cap);
	return sc27xx_fgu_save_boot_mode(data, SC27XX_FGU_NORMAIL_POWERTON);
}

static int sc27xx_fgu_set_clbcnt(struct sc27xx_fgu_data *data, int clbcnt)
{
	int ret;

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_CLBCNT_SETL,
				 SC27XX_FGU_CLBCNT_MASK, clbcnt);
	if (ret)
		return ret;

	ret = regmap_update_bits(data->regmap,
				 data->base + SC27XX_FGU_CLBCNT_SETH,
				 SC27XX_FGU_CLBCNT_MASK,
				 clbcnt >> SC27XX_FGU_CLBCNT_SHIFT);
	if (ret)
		return ret;

	return regmap_update_bits(data->regmap, data->base + SC27XX_FGU_START,
				 SC27XX_WRITE_SELCLB_EN,
				 SC27XX_WRITE_SELCLB_EN);
}

static int sc27xx_fgu_get_clbcnt(struct sc27xx_fgu_data *data, int *clb_cnt)
{
	int ccl, cch, ret;

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_CLBCNT_VALL, &ccl);
	if (ret)
		return ret;

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_CLBCNT_VALH, &cch);
	if (ret)
		return ret;

	*clb_cnt = ccl & SC27XX_FGU_CLBCNT_MASK;
	*clb_cnt |= (cch & SC27XX_FGU_CLBCNT_MASK) << SC27XX_FGU_CLBCNT_SHIFT;

	return 0;
}

static int sc27xx_fgu_get_vbat_avg(struct sc27xx_fgu_data *data, int *val)
{
	int ret, i;
	u32 vol = 0;

	*val = 0;
	for (i = 0; i < SC27XX_FGU_VOLTAGE_BUFF_CNT; i++) {
		ret = regmap_read(data->regmap,
				  data->base + SC27XX_FGU_VOLTAGE_BUF + i * 4,
				  &vol);
		if (ret)
			return ret;

		/*
		 * It is ADC values reading from registers which need to convert to
		 * corresponding voltage values.
		 */
		*val += sc27xx_fgu_adc_to_voltage(data, vol);
	}

	*val /= 8;

	return 0;
}

static int sc27xx_fgu_get_current_now(struct sc27xx_fgu_data *data, int *val)
{
	int ret;
	u32 cur = 0;

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_CURRENT, &cur);
	if (ret)
		return ret;

	/*
	 * It is ADC values reading from registers which need to convert to
	 * corresponding current values.
	 */
	*val = sc27xx_fgu_adc_to_current(data, (s64)cur - SC27XX_FGU_CUR_BASIC_ADC);

	return 0;
}

static int sc27xx_fgu_get_capacity(struct sc27xx_fgu_data *data, int *cap)
{
	int ret, cur_clbcnt, delta_clbcnt, delta_cap, temp, temp_cap;

	/* Get current coulomb counters firstly */
	ret = sc27xx_fgu_get_clbcnt(data, &cur_clbcnt);
	if (ret)
		return ret;

	delta_clbcnt = cur_clbcnt - data->init_clbcnt;
	data->last_clbcnt = data->cur_clbcnt;
	data->cur_clbcnt = cur_clbcnt;

	/*
	 * Convert coulomb counter to delta capacity (mAh), and set multiplier
	 * as 10 to improve the precision.
	 */
	temp = DIV_ROUND_CLOSEST(delta_clbcnt * 10, 36 * SC27XX_FGU_SAMPLE_HZ);
	if (temp > 0)
		temp = temp + data->cur_1000ma_adc / 2;
	else
		temp = temp - data->cur_1000ma_adc / 2;

	temp = div_s64(temp, data->cur_1000ma_adc);

	/*
	 * Convert to capacity percent of the battery total capacity,
	 * and multiplier is 100 too.
	 */
	delta_cap = DIV_ROUND_CLOSEST(temp * 1000, data->total_cap);
	*cap = delta_cap + data->init_cap;
	data->normal_temp_cap = *cap;
	if (data->normal_temp_cap < 0)
		data->normal_temp_cap = 0;
	else if (data->normal_temp_cap > 1000)
		data->normal_temp_cap = 1000;

	dev_info(data->dev, "init_cap = %d, init_clbcnt = %d, cur_clbcnt = %d, normal_cap = %d, "
		 "delta_cap = %d, Tbat  = %d, uusoc_vbat = %d\n",
		 data->init_cap, data->init_clbcnt, cur_clbcnt,
		 data->normal_temp_cap, delta_cap, data->bat_temp, data->uusoc_vbat);

	if (*cap < 0) {
		*cap = 0;
		dev_err(data->dev, "ERORR: normal_cap is < 0, adjust!!!\n");
		data->uusoc_vbat = 0;
		sc27xx_fgu_adjust_cap(data, 0);
		return 0;
	} else if (*cap > SC27XX_FGU_FCC_PERCENT) {
		dev_info(data->dev, "normal_cap is > 1000, adjust !!!\n");
		*cap = SC27XX_FGU_FCC_PERCENT;
		sc27xx_fgu_adjust_cap(data, SC27XX_FGU_FCC_PERCENT);
		return 0;
	}

	if (data->cap_table_len > 0) {
		temp_cap = sc27xx_fgu_temp_to_cap(data->cap_temp_table,
						  data->cap_table_len,
						  data->bat_temp);
		/*
		 * Battery capacity at different temperatures, we think
		 * the change is linear, the follow the formula: y = ax + k
		 *
		 * for example: display 100% at 25 degrees need to display
		 * 100% at -10 degrees, display 10% at 25 degrees need to
		 * display 0% at -10 degrees, substituting the above special
		 * points will deduced follow formula.
		 * formula 1:
		 * Capacity_Delta = 100 - Capacity_Percentage(T1)
		 * formula 2:
		 * Capacity_temp = (Capacity_Percentage(current) -
		 * Capacity_Delta) * 100 /(100 - Capacity_Delta)
		 */
		temp_cap *= 10;

		*cap = DIV_ROUND_CLOSEST((*cap + temp_cap - 1000) * 1000, temp_cap);
		if (*cap < 0) {
			*cap = 0;
		} else if (*cap > SC27XX_FGU_FCC_PERCENT) {
			dev_info(data->dev, "Capacity_temp > 1000, adjust !!!\n");
			*cap = SC27XX_FGU_FCC_PERCENT;
		}
	}

	if (*cap > 1000) {
		*cap = 1000;
		data->init_cap = 1000 - delta_cap;
		return 0;
	}
	/* Calibrate the battery capacity in a normal range. */
	sc27xx_fgu_capacity_calibration(data, false);

	*cap -= data->uusoc_vbat;
	if (*cap < 0) {
		*cap = 0;
	} else if (*cap > SC27XX_FGU_FCC_PERCENT) {
		dev_info(data->dev, "Capacity_temp > 1000, adjust !!!\n");
		*cap = SC27XX_FGU_FCC_PERCENT;
	}

	return 0;
}

static int sc27xx_fgu_get_vbat_now(struct sc27xx_fgu_data *data, int *val)
{
	int ret, vol = 0;

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_VOLTAGE, &vol);
	if (ret)
		return ret;

	/*
	 * It is ADC values reading from registers which need to convert to
	 * corresponding voltage values.
	 */
	*val = sc27xx_fgu_adc_to_voltage(data, vol);

	return 0;
}

static int sc27xx_fgu_get_current_avg(struct sc27xx_fgu_data *data, int *val)
{
	int ret, cur = 0;
	int i;

	*val = 0;

	for (i = 0; i < SC27XX_FGU_CURRENT_BUFF_CNT; i++) {
		ret = regmap_read(data->regmap, data->base + SC27XX_FGU_CURRENT_BUF + i * 4, &cur);
		if (ret)
			return ret;
		/*
		 * It is ADC values reading from registers which need to convert to
		 * corresponding current values.
		 */
		*val += sc27xx_fgu_adc_to_current(data, (s64)cur - SC27XX_FGU_CUR_BASIC_ADC);
	}

	*val /= 8;

	return 0;
}

static int sc27xx_fgu_get_vbat_ocv(struct sc27xx_fgu_data *data, int *val)
{
	int vol, cur, ret, resistance;

	ret = sc27xx_fgu_get_vbat_now(data, &vol);
	if (ret)
		return ret;

	ret = sc27xx_fgu_get_current_now(data, &cur);
	if (ret)
		return ret;

	resistance = data->internal_resist;
	if (data->resist_table_len > 0) {
		resistance = power_supply_temp2resist_simple(data->resist_table,
							     data->resist_table_len,
							     data->bat_temp / 10);
		resistance = data->internal_resist * resistance / 100;
	}

	/* Return the battery OCV in micro volts. */
	*val = vol * 1000 - cur * resistance;

	return 0;
}

static int sc27xx_fgu_vol_to_temp(struct power_supply_vol_temp_table *table,
				  int table_len, int vol)
{
	int i, temp;

	for (i = 0; i < table_len; i++)
		if (vol > table[i].vol)
			break;

	if (i > 0 && i < table_len) {
		temp = interpolate(vol,
				   table[i].vol,
				   table[i].temp,
				   table[i - 1].vol,
				   table[i - 1].temp);
	} else if (i == 0) {
		temp = table[0].temp;
	} else {
		temp = table[table_len - 1].temp;
	}

	return temp - 1000;
}

static int sc27xx_fgu_get_charge_vol(struct sc27xx_fgu_data *data, int *val)
{
	int ret, vol;

	ret = iio_read_channel_processed(data->charge_chan, &vol);
	if (ret < 0)
		return ret;

	*val = vol;
	return 0;
}

static int sc27xx_fgu_get_average_temp(struct sc27xx_fgu_data *data, int temp)
{
	int i, min, max;
	int sum = 0;

	if (data->temp_buff[0] == -500) {
		for (i = 0; i < SC27XX_FGU_TEMP_BUFF_CNT; i++)
			data->temp_buff[i] = temp;
	}

	if (data->index >= SC27XX_FGU_TEMP_BUFF_CNT)
		data->index = 0;

	data->temp_buff[data->index++] = temp;
	min = max = data->temp_buff[0];

	for (i = 0; i < SC27XX_FGU_TEMP_BUFF_CNT; i++) {
		if (data->temp_buff[i] > max)
			max = data->temp_buff[i];

		if (data->temp_buff[i] < min)
			min = data->temp_buff[i];

		sum += data->temp_buff[i];
	}

	sum = sum - max - min;

	return sum / (SC27XX_FGU_TEMP_BUFF_CNT - 2);
}

static int sc27xx_fgu_get_temp(struct sc27xx_fgu_data *data, int *temp)
{
	int vol, ret;

	ret = iio_read_channel_processed(data->channel, &vol);
	if (ret < 0)
		return ret;

	if (data->comp_resistance) {
		int bat_current, resistance_vol;

		ret = sc27xx_fgu_get_current_now(data, &bat_current);
		if (ret) {
			dev_err(data->dev, "failed to get battery current\n");
			return ret;
		}

		/*
		 * Due to the ntc resistor is connected to the coulomb counter
		 * internal resistance and the board ground impedance at 1850mv.
		 * so need to compensate for coulomb resistance and voltage loss
		 * to ground impedance.
		 * Follow the formula below:
		 * formula:
		 * Vadc = Vresistance + (1850 - Vresistance) * R / 47k + R
		 * ->
		 *  UR = Vadc -Vresistance +
		 *  Vresistance * (Vadc - Vresistance) / (1850 - Vresistance)
		 */
		resistance_vol = bat_current * data->comp_resistance;
		resistance_vol = DIV_ROUND_CLOSEST(resistance_vol, 1000);

		vol = vol - (resistance_vol * (1850 - vol)) / (1850 - resistance_vol);

		if (vol < 0)
			vol = 0;
	}

	if (data->temp_table_len > 0) {
		*temp = sc27xx_fgu_vol_to_temp(data->temp_table,
					       data->temp_table_len,
					       vol * 1000);
		*temp = sc27xx_fgu_get_average_temp(data, *temp);
	} else {
		*temp = 200;
	}

	data->bat_temp = *temp;

	return 0;
}

static int sc27xx_fgu_get_health(struct sc27xx_fgu_data *data, int *health)
{
	int ret, vol;

	ret = sc27xx_fgu_get_vbat_now(data, &vol);
	if (ret)
		return ret;

	if (vol > data->max_volt)
		*health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	else
		*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int sc27xx_fgu_get_status(struct sc27xx_fgu_data *data, int *status)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret = -EINVAL;

	for (i = 0; i < ARRAY_SIZE(sc27xx_charger_supply_name); i++) {
		psy = power_supply_get_by_name(sc27xx_charger_supply_name[i]);
		if (!psy)
			continue;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		power_supply_put(psy);
		if (ret)
			return ret;

		*status = val.intval;
		if (*status == POWER_SUPPLY_STATUS_CHARGING)
			break;
	}

	return ret;
}

static int sc27xx_fgu_parse_energy_density_ocv_table(struct sc27xx_fgu_data *data)
{
	struct device_node *np = data->dev->of_node;
	struct sc27xx_fgu_energy_density_ocv_table *table;
	const __be32 *list;
	int i, size;

	list = of_get_property(np, "sprd,energy-desity-ocv-table", &size);
	if (!list || !size)
		return 0;

	data->dens_table_len = size / (sizeof(struct sc27xx_fgu_energy_density_ocv_table) /
				       sizeof(int) * sizeof(__be32));

	table = devm_kzalloc(data->dev, sizeof(struct sc27xx_fgu_energy_density_ocv_table) *
			     (data->dens_table_len + 1), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	for (i = 0; i < data->dens_table_len; i++) {
		table[i].engy_dens_ocv_lo = be32_to_cpu(*list++);
		table[i].engy_dens_ocv_hi = be32_to_cpu(*list++);
		dev_info(data->dev, "engy_dens_ocv_hi = %d, engy_dens_ocv_lo = %d\n",
			 table[i].engy_dens_ocv_hi, table[i].engy_dens_ocv_lo);
	}

	data->dens_table = table;

	return 0;
}

static int sc27xx_fgu_suspend_calib_check_chg_sts(struct sc27xx_fgu_data *data)
{
	int ret = -EINVAL;
	int status;

	ret = sc27xx_fgu_get_status(data, &status);
	if (ret) {
		dev_err(data->dev, "Suspend calib failed to get charging status, ret = %d\n", ret);
		return ret;
	}

	if (status != POWER_SUPPLY_STATUS_NOT_CHARGING &&
	    status != POWER_SUPPLY_STATUS_DISCHARGING) {
		dev_info(data->dev, "Suspend calib charging status = %d, not meet conditions\n", ret);
		return ret;
	}

	return 0;
}

static int sc27xx_fgu_suspend_calib_check_temp(struct sc27xx_fgu_data *data)
{
	int ret, temp, i;

	for (i = 0; i < SC27XX_FGU_TEMP_BUFF_CNT; i++) {
		ret = sc27xx_fgu_get_temp(data, &temp);
		if (ret) {
			dev_err(data->dev, "Suspend calib failed to temp, ret = %d\n", ret);
			return ret;
		}
		udelay(100);
	}

	if (temp < SC27XX_FGU_SLP_CAP_CALIB_TEMP_LOW || temp > SC27XX_FGU_SLP_CAP_CALIB_TEMP_HI) {
		dev_err(data->dev, "Suspend calib  temp = %d out range\n", temp);
		ret = -EINVAL;
	}

	dev_info(data->dev, "%s, temp = %d\n", __func__, temp);

	return ret;
}

static int sc27xx_fgu_suspend_calib_check_relax_cnt_int(struct sc27xx_fgu_data *data)
{
	int ret = -EINVAL;
	u32 int_status;

	mutex_lock(&data->lock);
	if (data->slp_cap_calib.relax_cnt_int_ocurred) {
		data->slp_cap_calib.relax_cnt_int_ocurred = false;
		ret = 0;
		dev_info(data->dev, "RELX_CNT_INT ocurred 1!!\n");
		goto no_relax_cnt_int;
	}

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_INT_STS, &int_status);
	if (ret) {
		dev_err(data->dev, "suspend_calib failed to get fgu interrupt status, ret = %d\n", ret);
		goto no_relax_cnt_int;
	}

	if (!(int_status & SC27XX_FGU_RELX_CNT_STS)) {
		dev_info(data->dev, "no RELX_CNT_INT ocurred!!\n");
		ret = -EINVAL;
		goto no_relax_cnt_int;
	}

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_CLR,
				SC27XX_FGU_RELX_CNT_STS, SC27XX_FGU_RELX_CNT_STS);
	if (ret)
		dev_err(data->dev, "failed to clear  RELX_CNT_STS interrupt status, ret = %d\n", ret);

	dev_info(data->dev, "RELX_CNT_INT ocurred!!\n");
	ret = 0;

no_relax_cnt_int:
	mutex_unlock(&data->lock);
	return ret;
}

static int sc27xx_fgu_suspend_calib_check_sleep_time(struct sc27xx_fgu_data *data)
{
	struct timespec64 cur_time;

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	data->slp_cap_calib.resume_time =  cur_time.tv_sec;

	dev_info(data->dev, "%s, resume_time = %lld, suspend_time = %lld\n",
		 __func__, data->slp_cap_calib.resume_time, data->slp_cap_calib.suspend_time);

	if (data->slp_cap_calib.resume_time - data->slp_cap_calib.suspend_time <
	    SC27XX_FGU_SLP_CAP_CALIB_SLP_TIME) {
		dev_info(data->dev, "suspend time not met: suspend_time = %lld, resume_time = %lld\n",
			 data->slp_cap_calib.suspend_time,
			 data->slp_cap_calib.resume_time);
		return -EINVAL;
	}

	return 0;
}

static int sc27xx_fgu_suspend_calib_check_sleep_cur(struct sc27xx_fgu_data *data)
{
	int mah, clbcnt, times, sleep_cur = 0;

	sc27xx_fgu_get_clbcnt(data, &data->slp_cap_calib.resume_clbcnt);

	clbcnt = data->slp_cap_calib.suspend_clbcnt - data->slp_cap_calib.resume_clbcnt;
	times = data->slp_cap_calib.resume_time -  data->slp_cap_calib.suspend_time;

	mah = DIV_ROUND_CLOSEST(clbcnt * 10, 36 * SC27XX_FGU_SAMPLE_HZ);

	/* sleep_cur = mah * 3600 / times, but mah may be some, and will be zero after div_s64.
	 * so we *3600 first.
	 */
	mah *= 3600;
	if (mah > 0)
		mah = mah + data->cur_1000ma_adc / 2;
	else
		mah = mah - data->cur_1000ma_adc / 2;

	mah = div_s64(mah, data->cur_1000ma_adc);

	sleep_cur = mah / times;

	dev_info(data->dev, "%s, suspend_clbcnt = %d, resume_clbcnt = %d, clbcnt = %d\n",
		 __func__, data->slp_cap_calib.suspend_clbcnt,
		 data->slp_cap_calib.resume_clbcnt, clbcnt);
	dev_info(data->dev, "%s, sleep_cur = %d, times = %d, clbcnt = %d, mah = %d, 1000ma_adc = %d\n",
		 __func__, sleep_cur, times, clbcnt, mah, data->cur_1000ma_adc);

	if (abs(sleep_cur) > data->slp_cap_calib.relax_cur_threshold) {
		dev_info(data->dev, "Sleep calib sleep current = %d, not meet conditions\n", sleep_cur);
		return -EINVAL;
	}

	return 0;
}

static int sc27xx_fgu_suspend_calib_get_ocv(struct sc27xx_fgu_data *data)
{
	int ret, i, cur = 0x7fffffff;
	u32 vol = 0;

	for (i = SC27XX_FGU_VOLTAGE_BUFF_CNT - 1; i >= 0; i--) {
		vol = 0;
		ret = regmap_read(data->regmap,
				  data->base + SC27XX_FGU_VOLTAGE_BUF + i * 4,
				  &vol);
		if (ret) {
			dev_info(data->dev, "Sleep calib fail to get vbat_buf[%d]\n", i);
			continue;
		}

		/*
		 * It is ADC values reading from registers which need to convert to
		 * corresponding voltage values.
		 */
		vol = sc27xx_fgu_adc_to_voltage(data, vol);

		cur = 0x7fffffff;
		ret = regmap_read(data->regmap,
				  data->base + SC27XX_FGU_CURRENT_BUF + i * 4,
				  &cur);
		if (ret) {
			dev_info(data->dev, "Sleep calib fail to get cur_buf[%d]\n", i);
			continue;
		}

		/*
		 * It is ADC values reading from registers which need to convert to
		 * corresponding current values.
		 */
		cur = sc27xx_fgu_adc_to_current(data, cur - SC27XX_FGU_CUR_BASIC_ADC);
		if (abs(cur) < data->slp_cap_calib.relax_cur_threshold) {
			dev_info(data->dev, "Sleep calib get cur[%d] = %d met condition\n", i, cur);
			break;
		}
	}

	if (vol == 0 || cur == 0x7fffffff) {
		dev_info(data->dev, "Sleep calib fail to get cur and vol: cur = %d, vol = %d\n",
			 cur, vol);
		return -EINVAL;
	}

	dev_info(data->dev, "Sleep calib vol = %d, cur = %d, i = %d\n", vol, cur, i);

	data->slp_cap_calib.resume_ocv = vol * 1000;

	return 0;
}

static int sc27xx_fgu_suspend_calib_check_ocv(struct sc27xx_fgu_data *data)
{
	bool is_matched = false;
	int i;

	if (data->dens_table_len == 0) {
		dev_warn(data->dev, "Sleep calib energy density ocv table len is 0 !!!!\n");
		return -EINVAL;
	}

	for (i = 0; i < data->dens_table_len; i++) {
		if (data->slp_cap_calib.resume_ocv > data->dens_table[i].engy_dens_ocv_lo &&
		    data->slp_cap_calib.resume_ocv < data->dens_table[i].engy_dens_ocv_hi) {
			dev_info(data->dev, "Sleep calib get valid resume ocv, vol = %d\n",
				 data->slp_cap_calib.resume_ocv);
			is_matched = true;
			break;
		}
	}

	if (is_matched)
		return 0;

	dev_info(data->dev, "Sleep calib resume ocv is out of dens range, vol = %d\n",
		 data->slp_cap_calib.resume_ocv);

	return -EINVAL;
}

static void sc27xx_fgu_suspend_calib_cap_calib(struct sc27xx_fgu_data *data)
{
	data->slp_cap_calib.resume_ocv_cap =
		power_supply_ocv2cap_simple(data->cap_table,
					    data->table_len,
					    data->slp_cap_calib.resume_ocv);

	data->slp_cap_calib.resume_ocv_cap *= 10;

	dev_info(data->dev, "%s, resume_ocv_cap = %d, normal_temp_cap = %d, init_cap = %d\n",
		 __func__, data->slp_cap_calib.resume_ocv_cap,
		 data->normal_temp_cap, data->init_cap);

	if (data->slp_cap_calib.resume_ocv_cap > data->normal_temp_cap + 30)
		data->init_cap += (data->slp_cap_calib.resume_ocv_cap -
				   data->normal_temp_cap - 30);
	else if (data->slp_cap_calib.resume_ocv_cap < data->normal_temp_cap - 30)
		data->init_cap -= (data->normal_temp_cap -
				   data->slp_cap_calib.resume_ocv_cap - 30);
}

static void sc27xx_fgu_suspend_calib_check(struct sc27xx_fgu_data *data)
{
	int ret;

	if (!data->slp_cap_calib.support_slp_calib)
		return;

	ret = sc27xx_fgu_suspend_calib_check_chg_sts(data);
	if (ret)
		return;

	ret = sc27xx_fgu_suspend_calib_check_temp(data);
	if (ret)
		return;

	ret = sc27xx_fgu_suspend_calib_check_relax_cnt_int(data);
	if (ret)
		return;

	ret = sc27xx_fgu_suspend_calib_check_sleep_time(data);
	if (ret)
		return;

	ret = sc27xx_fgu_suspend_calib_check_sleep_cur(data);
	if (ret)
		return;

	ret = sc27xx_fgu_suspend_calib_get_ocv(data);
	if (ret)
		return;

	ret = sc27xx_fgu_suspend_calib_check_ocv(data);
	if (ret)
		return;

	sc27xx_fgu_suspend_calib_cap_calib(data);

	return;
}

static void sc27xx_fgu_enable_relax_cnt_int(struct sc27xx_fgu_data *data)
{
	int ret;

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_CLR,
				SC27XX_FGU_RELX_CNT_STS, SC27XX_FGU_RELX_CNT_STS);
	if (ret)
		dev_err(data->dev, "Sleep calib failed to clear  RELX_CNT_INT_STS, ret = %d\n", ret);

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_EN,
				 SC27XX_FGU_RELX_CNT_INT, SC27XX_FGU_RELX_CNT_INT);
	if (ret)
		dev_err(data->dev, "Sleep calib Fail to enable RELX_CNT_INT, re= %d\n", ret);
}

static int sc27xx_fgu_relax_mode_config(struct sc27xx_fgu_data *data)
{
	int ret;

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_RELAX_CNT_THRE,
				 SC2720_FGU_RELAX_CNT_THRE_MASK,
				 data->slp_cap_calib.relax_cnt_threshold <<
				 SC2720_FGU_RELAX_CNT_THRE_SHITF);
	if (ret) {
		dev_err(data->dev, "Sleep calib Fail to enable RELX_CNT_THRE, re= %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_RELAX_CURT_THRE,
				 SC2720_FGU_RELAX_CURT_THRE_MASK,
				 data->slp_cap_calib.relax_cur_threshold <<
				 SC2720_FGU_RELAX_CURT_THRE_SHITF);
	if (ret)
		dev_err(data->dev, "Sleep calib Fail to enable RELX_CURT_THRE, re= %d\n", ret);

	return ret;
}

static void sc27xx_fgu_suspend_calib_config(struct sc27xx_fgu_data *data)
{
	struct timespec64 cur_time;
	int ret;

	if (!data->slp_cap_calib.support_slp_calib)
		return;

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	data->slp_cap_calib.suspend_time =  cur_time.tv_sec;
	sc27xx_fgu_get_clbcnt(data, &data->slp_cap_calib.suspend_clbcnt);
	ret = sc27xx_fgu_relax_mode_config(data);
	if (!ret)
		sc27xx_fgu_enable_relax_cnt_int(data);
}

static int sc27xx_fgu_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	struct sc27xx_fgu_data *data = power_supply_get_drvdata(psy);
	int ret = 0, value = 0;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&data->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = sc27xx_fgu_get_status(data, &value);
		if (ret)
			goto error;

		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (data->debug_info.batt_health_debug_en) {
			val->intval = data->debug_info.debug_batt_health;
			break;
		}

		ret = sc27xx_fgu_get_health(data, &value);
		if (ret)
			goto error;

		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = data->bat_present;

		if (data->debug_info.batt_present_debug_en)
			val->intval = data->debug_info.debug_batt_present;

		break;

	case POWER_SUPPLY_PROP_TEMP:
		if (data->debug_info.temp_debug_en)
			val->intval = data->debug_info.debug_temp;
		else if (data->temp_table_len <= 0)
			val->intval = 200;
		else {
			ret = sc27xx_fgu_get_temp(data, &value);
			if (ret < 0 && !data->debug_info.temp_debug_en)
				goto error;

			ret = 0;
			val->intval = value;
		}

		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == CM_BOOT_CAPACITY) {
			val->intval = data->boot_cap;
			break;
		}

		ret = sc27xx_fgu_get_capacity(data, &value);
		if (ret)
			goto error;

		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = sc27xx_fgu_get_vbat_avg(data, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (data->debug_info.vbat_now_debug_en) {
			val->intval = data->debug_info.debug_vbat_now;
			break;
		}

		ret = sc27xx_fgu_get_vbat_now(data, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		if (data->debug_info.ocv_debug_en) {
			val->intval = data->debug_info.debug_ocv;
			break;
		}
		ret = sc27xx_fgu_get_vbat_ocv(data, &value);
		if (ret)
			goto error;

		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		if (data->debug_info.chg_vol_debug_en) {
			val->intval = data->debug_info.debug_chg_vol;
			break;
		}

		ret = sc27xx_fgu_get_charge_vol(data, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = sc27xx_fgu_get_current_avg(data, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (data->debug_info.cur_now_debug_en) {
			val->intval = data->debug_info.debug_cur_now;
			break;
		}

		ret = sc27xx_fgu_get_current_now(data, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = data->total_cap * 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = sc27xx_fgu_get_clbcnt(data, &value);
		if (ret)
			goto error;

		value = DIV_ROUND_CLOSEST(value * 10, 36 * SC27XX_FGU_SAMPLE_HZ);
		val->intval = sc27xx_fgu_adc_to_current(data, (s64)value);

		break;

	case POWER_SUPPLY_PROP_VOLTAGE_BOOT:
		val->intval = data->boot_volt;
		break;

	default:
		ret = -EINVAL;
		break;
	}

error:
	mutex_unlock(&data->lock);
	return ret;
}

static int sc27xx_fgu_set_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   const union power_supply_propval *val)
{
	struct sc27xx_fgu_data *data = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&data->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = sc27xx_fgu_save_last_cap(data, val->intval);
		if (ret < 0) {
			dev_err(data->dev, "failed to save battery capacity\n");
			goto error;
		}

		ret = sc27xx_fgu_save_normal_temperature_cap(data, data->normal_temp_cap);
		if (ret < 0)
			dev_err(data->dev, "failed to save normal temperature capacity\n");
		break;

	case POWER_SUPPLY_PROP_CALIBRATE:
		sc27xx_fgu_adjust_cap(data, val->intval);
		break;

	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		data->total_cap = val->intval / 1000;
		break;

	case POWER_SUPPLY_PROP_TEMP:
		if (val->intval == SC27XX_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change battery temperature to debug mode\n");
			data->debug_info.temp_debug_en = true;
			data->debug_info.debug_temp = 200;
			break;
		} else if (val->intval == SC27XX_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery battery temperature to normal mode\n");
			data->debug_info.temp_debug_en = false;
			break;
		} else if (!data->debug_info.temp_debug_en) {
			dev_info(data->dev, "Battery temperature not in debug mode\n");
			break;
		}

		data->debug_info.debug_temp = val->intval;
		dev_info(data->dev, "Battery debug temperature = %d\n", val->intval);
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		if (val->intval == SC27XX_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change battery present to debug mode\n");
			data->debug_info.debug_batt_present = true;
			data->debug_info.batt_present_debug_en = true;
			break;
		} else if (val->intval == SC27XX_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery battery present to normal mode\n");
			data->debug_info.batt_present_debug_en = false;
			break;
		} else if (!data->debug_info.batt_present_debug_en) {
			dev_info(data->dev, "Battery present not in debug mode\n");
			break;
		}

		data->debug_info.debug_batt_present = !!val->intval;
		mutex_unlock(&data->lock);
		cm_notify_event(data->battery, data->debug_info.debug_batt_present ?
				CM_EVENT_BATT_IN : CM_EVENT_BATT_OUT, NULL);
		dev_info(data->dev, "Battery debug present = %d\n", !!val->intval);
		return ret;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (val->intval == SC27XX_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change voltage_now to debug mode\n");
			data->debug_info.debug_vbat_now = 4000000;
			data->debug_info.vbat_now_debug_en = true;
			break;
		} else if (val->intval == SC27XX_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery voltage_now to normal mode\n");
			data->debug_info.vbat_now_debug_en = false;
			data->debug_info.debug_vbat_now = 0;
			break;
		} else if (!data->debug_info.vbat_now_debug_en) {
			dev_info(data->dev, "Voltage_now not in debug mode\n");
			break;
		}

		data->debug_info.debug_vbat_now = val->intval;
		dev_info(data->dev, "Battery debug voltage_now = %d\n", val->intval);
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (val->intval == SC27XX_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change current_now to debug mode\n");
			data->debug_info.debug_cur_now = 1000000;
			data->debug_info.cur_now_debug_en = true;
			break;
		} else if (val->intval == SC27XX_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery current_now to normal mode\n");
			data->debug_info.cur_now_debug_en = false;
			data->debug_info.debug_cur_now = 0;
			break;
		} else if (!data->debug_info.cur_now_debug_en) {
			dev_info(data->dev, "Current_now not in debug mode\n");
			break;
		}

		data->debug_info.debug_cur_now = val->intval;
		dev_info(data->dev, "Battery debug current_now = %d\n", val->intval);
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		if (val->intval == SC27XX_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change charge voltage to debug mode\n");
			data->debug_info.debug_chg_vol = 5000000;
			data->debug_info.chg_vol_debug_en = true;
			break;
		} else if (val->intval == SC27XX_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery charge voltage to normal mode\n");
			data->debug_info.chg_vol_debug_en = false;
			data->debug_info.debug_chg_vol = 0;
			break;
		} else if (!data->debug_info.chg_vol_debug_en) {
			dev_info(data->dev, "Charge voltage not in debug mode\n");
			break;
		}

		data->debug_info.debug_chg_vol = val->intval;
		dev_info(data->dev, "Battery debug charge voltage = %d\n", val->intval);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		if (val->intval == SC27XX_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change OCV voltage to debug mode\n");
			data->debug_info.debug_ocv = 4000000;
			data->debug_info.ocv_debug_en = true;
			break;
		} else if (val->intval == SC27XX_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery OCV voltage to normal mode\n");
			data->debug_info.ocv_debug_en = false;
			data->debug_info.debug_ocv = 0;
			break;
		} else if (!data->debug_info.ocv_debug_en) {
			dev_info(data->dev, "OCV voltage not in debug mode\n");
			break;
		}

		data->debug_info.debug_ocv = val->intval;
		dev_info(data->dev, "Battery debug OCV voltage = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (val->intval == SC27XX_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change Battery Health to debug mode\n");
			data->debug_info.batt_health_debug_en = true;
			data->debug_info.debug_batt_health = 1;
			break;
		} else if (val->intval == SC27XX_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery  Battery Health to normal mode\n");
			data->debug_info.batt_health_debug_en = false;
			data->debug_info.debug_batt_health = 1;
			break;
		} else if (!data->debug_info.batt_health_debug_en) {
			dev_info(data->dev, "OCV  Battery Health not in debug mode\n");
			break;
		}

		data->debug_info.debug_batt_health = val->intval;
		dev_info(data->dev, "Battery debug  Battery Health = %d\n", val->intval);
		break;

	default:
		ret = -EINVAL;
	}

error:
	mutex_unlock(&data->lock);
	return ret;
}

static void sc27xx_fgu_external_power_changed(struct power_supply *psy)
{
	struct sc27xx_fgu_data *data = power_supply_get_drvdata(psy);

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	power_supply_changed(data->battery);
}

static int sc27xx_fgu_property_is_writeable(struct power_supply *psy,
					    enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CALIBRATE:
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
	case POWER_SUPPLY_PROP_HEALTH:
		return 1;

	default:
		return 0;
	}
}

static enum power_supply_property sc27xx_fgu_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_BOOT,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_CALIBRATE,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
};

static const struct power_supply_desc sc27xx_fgu_desc = {
	.name			= "sc27xx-fgu",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= sc27xx_fgu_props,
	.num_properties		= ARRAY_SIZE(sc27xx_fgu_props),
	.get_property		= sc27xx_fgu_get_property,
	.set_property		= sc27xx_fgu_set_property,
	.external_power_changed	= sc27xx_fgu_external_power_changed,
	.property_is_writeable	= sc27xx_fgu_property_is_writeable,
	.no_thermal		= true,
};

static void sc27xx_fgu_adjust_cap(struct sc27xx_fgu_data *data, int cap)
{
	int ret;

	data->init_cap = cap;
	ret = sc27xx_fgu_get_clbcnt(data, &data->init_clbcnt);
	if (ret)
		dev_err(data->dev, "failed to get init coulomb counter\n");
}

static void sc27xx_fgu_adjust_uusoc_vbat(struct sc27xx_fgu_data *data)
{
	if (data->batt_uV >= SC27XX_FGU_LOW_VBAT_REC_REGION) {
		data->uusoc_vbat = 0;
	} else if (data->batt_uV >= SC27XX_FGU_LOW_VBAT_REGION) {
		if (data->uusoc_vbat >= 5)
			data->uusoc_vbat -= 5;
	}
}

static void sc27xx_fgu_low_capacity_match_ocv(struct sc27xx_fgu_data *data)
{
	if (data->ocv < data->min_volt && data->normal_temp_cap > data->alarm_cap) {
		data->init_cap -= 5;
		if (data->init_cap < 0)
			data->init_cap = 0;
	} else if (data->ocv > data->min_volt && data->normal_temp_cap <= data->alarm_cap) {
		sc27xx_fgu_adjust_cap(data, data->alarm_cap);
	} else if (data->ocv <= data->cap_table[data->table_len - 1].ocv) {
		sc27xx_fgu_adjust_cap(data, 0);
	} else if (data->first_calib_volt > 0 && data->first_calib_cap > 0 &&
		   data->ocv <= data->first_calib_volt &&
		   data->normal_temp_cap > data->first_calib_cap) {
		data->init_cap -= 5;
		if (data->init_cap < 0)
			data->init_cap = 0;
	} else if (data->batt_uV < SC27XX_FGU_LOW_VBAT_REGION &&
		   data->normal_temp_cap > data->alarm_cap)
		data->uusoc_vbat += 5;

	sc27xx_fgu_adjust_uusoc_vbat(data);
}

static bool sc27xx_fgu_discharging_current_trend(struct sc27xx_fgu_data *data)
{
	int i, ret, cur = 0;
	bool is_discharging = true;

	if (data->cur_now_buff[SC27XX_FGU_CURRENT_BUFF_CNT - 1] == SC27XX_FGU_MAGIC_NUMBER) {
		is_discharging = false;
		for (i = 0; i < SC27XX_FGU_CURRENT_BUFF_CNT; i++) {
			ret = regmap_read(data->regmap,
					  data->base + SC27XX_FGU_CURRENT_BUF + i * 4,
					  &cur);
			if (ret) {
				dev_err(data->dev, "fail to init cur_now_buff[%d]\n", i);
				return is_discharging;
			}

			data->cur_now_buff[i] =
				sc27xx_fgu_adc_to_current(data, cur - SC27XX_FGU_CUR_BASIC_ADC);
		}

		return is_discharging;
	}

	for (i = 0; i < SC27XX_FGU_CURRENT_BUFF_CNT; i++) {
		if (data->cur_now_buff[i] > 0)
			is_discharging = false;
	}

	for (i = 0; i < SC27XX_FGU_CURRENT_BUFF_CNT; i++) {
		ret = regmap_read(data->regmap,
				  data->base + SC27XX_FGU_CURRENT_BUF + i * 4,
				  &cur);
		if (ret) {
			dev_err(data->dev, "fail to get cur_now_buff[%d]\n", i);
			data->cur_now_buff[SC27XX_FGU_CURRENT_BUFF_CNT - 1] =
				SC27XX_FGU_MAGIC_NUMBER;
			is_discharging = false;
			return is_discharging;
		}

		data->cur_now_buff[i] =
			sc27xx_fgu_adc_to_current(data, cur - SC27XX_FGU_CUR_BASIC_ADC);
		if (data->cur_now_buff[i] > 0)
			is_discharging = false;
	}

	return is_discharging;
}

static bool sc27xx_fgu_discharging_clbcnt_trend(struct sc27xx_fgu_data *data)
{
	if (data->last_clbcnt - data->cur_clbcnt > 0)
		return true;
	else
		return false;
}

static bool sc27xx_fgu_discharging_trend(struct sc27xx_fgu_data *data, int chg_sts)
{
	bool discharging = true;
	static int dischg_cnt;
	int i;

	if (dischg_cnt >= SC27XX_FGU_DISCHG_CNT)
		dischg_cnt = 0;

	if (!sc27xx_fgu_discharging_current_trend(data)) {
		discharging =  false;
		goto charging;
	}

	if (!sc27xx_fgu_discharging_clbcnt_trend(data)) {
		discharging =  false;
		goto charging;
	}

	data->dischg_trend[dischg_cnt++] = true;

	for (i = 0; i < SC27XX_FGU_DISCHG_CNT; i++) {
		if (!data->dischg_trend[i]) {
			discharging =  false;
			return discharging;
		}
	}

	if (chg_sts == POWER_SUPPLY_STATUS_CHARGING && discharging)
		dev_info(data->dev, "%s: discharging\n", __func__);

	return discharging;

charging:
	data->dischg_trend[dischg_cnt++] = false;
	return discharging;
}

static void sc27xx_fgu_capacity_calibration(struct sc27xx_fgu_data *data, bool int_mode)
{
	int ret, chg_sts, adc;

	ret = sc27xx_fgu_get_vbat_ocv(data, &data->ocv);
	if (ret) {
		dev_err(data->dev, "get battery ocv error.\n");
		return;
	}

	ret =  sc27xx_fgu_get_vbat_now(data, &data->batt_uV);
	if (ret) {
		dev_err(data->dev, "get battery vol error.\n");
		return;
	}

	ret = sc27xx_fgu_get_status(data, &chg_sts);
	if (ret) {
		dev_err(data->dev, "get charger status error.\n");
		return;
	}

	/*
	 * If we are in charging mode or the battery temperature is
	 * 10 degrees or less, then we do not need to calibrate the
	 * lower capacity.
	 */
	if ((!sc27xx_fgu_discharging_trend(data, chg_sts) &&
	     chg_sts == POWER_SUPPLY_STATUS_CHARGING) ||
	    data->bat_temp <= SC27XX_FGU_LOW_TEMP_REGION) {
		sc27xx_fgu_adjust_uusoc_vbat(data);
		return;
	}

	sc27xx_fgu_low_capacity_match_ocv(data);

	if (data->ocv <= data->min_volt) {
		if (!int_mode)
			return;

		/*
		 * After adjusting the battery capacity, we should set the
		 * lowest alarm voltage instead.
		 */
		data->min_volt = data->cap_table[data->table_len - 1].ocv;
		data->alarm_cap = power_supply_ocv2cap_simple(data->cap_table,
							      data->table_len,
							      data->min_volt);

		data->alarm_cap *= 10;

		adc = sc27xx_fgu_voltage_to_adc(data, data->min_volt / 1000);
		regmap_update_bits(data->regmap,
				   data->base + SC27XX_FGU_LOW_OVERLOAD,
				   SC27XX_FGU_LOW_OVERLOAD_MASK, adc);
	}
}

static irqreturn_t sc27xx_fgu_interrupt(int irq, void *dev_id)
{
	struct sc27xx_fgu_data *data = dev_id;
	int ret, cap;
	u32 status;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return IRQ_HANDLED;
	}

	mutex_lock(&data->lock);

	ret = regmap_read(data->regmap, data->base + SC27XX_FGU_INT_STS,
			  &status);
	if (ret)
		goto out;

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_CLR,
				 status, status);
	if (ret)
		goto out;

	if (status & SC27XX_FGU_RELX_CNT_STS) {
		data->slp_cap_calib.relax_cnt_int_ocurred = true;
		dev_info(data->dev, "%s,  RELX_CNT_INT ocurred!!\n", __func__);
	}

	/*
	 * When low overload voltage interrupt happens, we should calibrate the
	 * battery capacity in lower voltage stage.
	 */
	if (!(status & SC27XX_FGU_LOW_OVERLOAD_INT))
		goto out;

	ret = sc27xx_fgu_get_capacity(data, &cap);
	if (ret)
		goto out;

	sc27xx_fgu_capacity_calibration(data, true);

out:
	mutex_unlock(&data->lock);

	power_supply_changed(data->battery);
	return IRQ_HANDLED;
}

static irqreturn_t sc27xx_fgu_bat_detection(int irq, void *dev_id)
{
	struct sc27xx_fgu_data *data = dev_id;
	int state;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return IRQ_HANDLED;
	}

	mutex_lock(&data->lock);

	state = gpiod_get_value_cansleep(data->gpiod);
	if (state < 0) {
		dev_err(data->dev, "failed to get gpio state\n");
		mutex_unlock(&data->lock);
		return IRQ_RETVAL(state);
	}

	data->bat_present = !!state;

	mutex_unlock(&data->lock);

	power_supply_changed(data->battery);

	cm_notify_event(data->battery,
			data->bat_present ? CM_EVENT_BATT_IN : CM_EVENT_BATT_OUT,
			NULL);

	return IRQ_HANDLED;
}

static void sc27xx_fgu_disable(void *_data)
{
	struct sc27xx_fgu_data *data = _data;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	regmap_update_bits(data->regmap, SC27XX_CLK_EN0, SC27XX_FGU_RTC_EN, 0);
	regmap_update_bits(data->regmap, SC27XX_MODULE_EN0, SC27XX_FGU_EN, 0);
}

static int sc27xx_fgu_cap_to_clbcnt(struct sc27xx_fgu_data *data, int capacity)
{
	/*
	 * Get current capacity (mAh) = battery total capacity (mAh) *
	 * current capacity percent (capacity / 100).
	 */
	int cur_cap = DIV_ROUND_CLOSEST(data->total_cap * capacity, 1000);

	/*
	 * Convert current capacity (mAh) to coulomb counter according to the
	 * formula: 1 mAh =3.6 coulomb.
	 */
	return DIV_ROUND_CLOSEST(cur_cap * 36 * data->cur_1000ma_adc * SC27XX_FGU_SAMPLE_HZ, 10);
}

static int sc27xx_fgu_calibration(struct sc27xx_fgu_data *data)
{
	struct nvmem_cell *cell;
	const struct sc27xx_fgu_variant_data *pdata = data->pdata;
	int calib_data, cal_4200mv;
	void *buf;
	size_t len;

	if (!pdata) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	cell = nvmem_cell_get(data->dev, "fgu_calib");
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(&calib_data, buf, min(len, sizeof(u32)));

	/*
	 * Get the ADC value corresponding to 4200 mV from eFuse controller
	 * according to below formula. Then convert to ADC values corresponding
	 * to 1000 mV and 1000 mA.
	 */
	cal_4200mv = ((calib_data & pdata->fgu_cal) >> pdata->fgu_cal_shift)
			+ 6963 - 4096 - 256;
	data->vol_1000mv_adc = DIV_ROUND_CLOSEST(cal_4200mv * 10, 42);
	data->cur_1000ma_adc =
		DIV_ROUND_CLOSEST(data->vol_1000mv_adc * 4 * data->calib_resist,
				  SC27XX_FGU_IDEAL_RESISTANCE);

	kfree(buf);
	return 0;
}

static int sc27xx_fgu_get_battery_table_info(struct power_supply *psy,
				  struct sc27xx_fgu_data *data)
{
	struct device_node *battery_np;
	const char *value;
	const __be32 *list;
	int err, index, size;

	data->temp_table = NULL;
	data->temp_table_len = -EINVAL;

	if (!psy->of_node) {
		dev_warn(&psy->dev, "%s currently only supports devicetree\n",
			 __func__);
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

	list = of_get_property(battery_np, "voltage-temp-table", &size);
	if (!list || !size)
		return 0;

	data->temp_table_len = size / (2 * sizeof(__be32));
	data->temp_table = devm_kcalloc(&psy->dev,
					data->temp_table_len,
					sizeof(*data->temp_table),
					GFP_KERNEL);
	if (!data->temp_table)
		return -ENOMEM;

	/*
	 * We should give a initial temperature value of temp_buff.
	 */
	data->temp_buff[0] = -500;

	for (index = 0; index < data->temp_table_len; index++) {
		data->temp_table[index].vol = be32_to_cpu(*list++);
		data->temp_table[index].temp = be32_to_cpu(*list++);
	}

	list = of_get_property(battery_np, "capacity-temp-table", &size);
	if (!list || !size)
		return 0;

	data->cap_table_len = size / (2 * sizeof(__be32));
	data->cap_temp_table = devm_kcalloc(&psy->dev,
					    data->cap_table_len,
					    sizeof(*data->cap_temp_table),
					    GFP_KERNEL);
	if (!data->cap_temp_table)
		return -ENOMEM;

	for (index = 0; index < data->cap_table_len; index++) {
		data->cap_temp_table[index].temp = be32_to_cpu(*list++);
		data->cap_temp_table[index].cap = be32_to_cpu(*list++);
	}

	return 0;
}

static void sc27xx_fgu_track_capacity_work(struct work_struct *work)
{
	struct sc27xx_fgu_data *data = container_of(work,
						  struct sc27xx_fgu_data,
						  track.track_capacity_work.work);
	u32 total_cap, capacity, check_capacity, file_buf[2];

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}
	/*
	 * open the track file on here.
	 */
	total_cap = data->total_cap;

	switch (data->track.state) {
	case CAP_TRACK_INIT:
		/*
		 * When the capacity tracking function starts to work,
		 * need to read the last saved capacity value from the
		 * file system, for security reasons we need to decrypt,
		 * in contrast, when writing data to the file system,
		 * we need to encrypt it.
		 */
		data->track.state = CAP_TRACK_IDLE;

		if (data->is_first_poweron && !data->track.clear_cap_flag) {
			file_buf[0] = 0 ^ SC27XX_FGU_TRACK_CAP_KEY0;
			file_buf[1] = 0 ^ SC27XX_FGU_TRACK_CAP_KEY1;
			/*
			 * write file_buf to the track file.
			 */
			data->track.clear_cap_flag = true;
			break;
		}
		dev_info(data->dev, "clear_cap_flag = %d\n", data->track.clear_cap_flag);
		/*
		 * read file and check the track file.
		 */
		capacity = file_buf[0] ^ SC27XX_FGU_TRACK_CAP_KEY0;
		check_capacity = file_buf[1] ^ SC27XX_FGU_TRACK_CAP_KEY1;
		if (capacity != check_capacity) {
			dev_err(data->dev, "track file data error.\n");
		}

		if (abs(total_cap - capacity) < total_cap / 5)
			data->total_cap = capacity;
		break;

	case CAP_TRACK_DONE:
		data->track.state = CAP_TRACK_IDLE;
		file_buf[0] = total_cap ^ SC27XX_FGU_TRACK_CAP_KEY0;
		file_buf[1] = total_cap ^ SC27XX_FGU_TRACK_CAP_KEY1;
		/*
		 * write file_buf to the track file.
		 */
		break;

	default:
		data->track.state = CAP_TRACK_IDLE;
		break;
	}
/*
 * close the track file.
 */
}

static int sc27xx_fgu_get_batt_energy_now(struct sc27xx_fgu_data *data, int *clbcnt)
{
	int value = 0, ret;

	ret = sc27xx_fgu_get_clbcnt(data, &value);
	if (ret) {
		dev_err(data->dev, "failed to get clbcnt\n");
		return ret;
	}

	value = DIV_ROUND_CLOSEST(value * 10, 36 * SC27XX_FGU_SAMPLE_HZ);
	*clbcnt = sc27xx_fgu_adc_to_current(data, value);

	return 0;
}

static void sc27xx_fgu_track_capacity_monitor(struct sc27xx_fgu_data *data)
{
	int ibat_avg, ret;
	int capacity, clbcnt, ocv, vbat_avg;
	u32 total_cap;

	if (!data->track.cap_tracking)
		return;

	if (!data->bat_present) {
		dev_err(data->dev, "battery is not present, cancel monitor.\n");
		return;
	}

	if (data->bat_temp > SC27XX_FGU_TRACK_HIGH_TEMP_THRESHOLD ||
	    data->bat_temp < SC27XX_FGU_TRACK_LOW_TEMP_THRESHOLD) {
		dev_err(data->dev, "exceed temperature range, cancel monitor.\n");
		return;
	}

	ret = sc27xx_fgu_get_current_avg(data, &ibat_avg);
	if (ret) {
		dev_err(data->dev, "failed to get relax current.\n");
		return;
	}

	ret = sc27xx_fgu_get_vbat_avg(data, &vbat_avg);
	if (ret) {
		dev_err(data->dev, "failed to get battery voltage.\n");
		return;
	}

	ret = sc27xx_fgu_get_vbat_ocv(data, &ocv);
	if (ret) {
		dev_err(data->dev, "get ocv error\n");
		return;
	}

	ocv = ocv / 1000;

	/*
	 * If the capacity tracking monitor in idle state, we will
	 * record the start battery coulomb. when the capacity
	 * tracking monitor meet end condition, also will record
	 * the end battery coulomb, we can calculate the actual
	 * battery capacity by delta coulomb.
	 * if the following formula , we will replace the standard
	 * capacity with the calculated actual capacity.
	 * formula:
	 * abs(current_capacity -capacity) < capacity / 5
	 */
	switch (data->track.state) {
	case CAP_TRACK_ERR:
		dev_err(data->dev, "track status error, cancel monitor.\n");
		return;

	case CAP_TRACK_IDLE:
		/*
		 * The capacity tracking monitor start condition is
		 * divided into two types:
		 * 1.poweroff charging mode:
		 * the boot voltage is less than 3500000uv, because
		 * we set the ocv minimum value is 3400000uv, so the
		 * the tracking start voltage value we set needs to
		 * be infinitely close to the shutdown value.
		 * 2.power on normal mode:
		 * the current less than 30000ua and the voltage
		 * less than 3650000uv. When meet the above conditions,
		 * the battery is almost empty, which is the result of
		 * multiple test data, so this point suitable as a
		 * starting condition.
		 */

		if (abs(ibat_avg) > SC27XX_FGU_TRACK_CAP_START_CURRENT ||
		    ocv > SC27XX_FGU_TRACK_CAP_START_VOLTAGE) {
			dev_info(data->dev, "not satisfy power on start condition.\n");
			return;
		}

		dev_info(data->dev, "start ibat_avg = %d, vbat_avg = %d, ocv = %d\n",
			 ibat_avg, vbat_avg, ocv);
		/*
		 * Parse the capacity table to look up the correct capacity percent
		 * according to current battery's corresponding OCV values.
		 */
		data->track.start_cap = power_supply_ocv2cap_simple(data->cap_table,
								    data->table_len,
								    ocv * 1000);
		data->track.start_cap *= 10;
		dev_info(data->dev, "is_charger_mode = %d, start_cap = %d\n",
			 is_charger_mode, data->track.start_cap);
		/*
		 * When the capacity tracking start condition is met,
		 * the battery is almost empty,so we set a starting
		 * threshold, if it is greater than it will not enable
		 * the capacity tracking function, now we set the capacity
		 * tracking monitor initial percentage threshold to 20%.
		 */
		if (data->track.start_cap > SC27XX_FGU_TRACK_START_CAP_THRESHOLD) {
			dev_info(data->dev,
				 "does not satisfy the track start condition, start_cap = %d\n",
				 data->track.start_cap);
			data->track.start_cap = 0;
			return;
		}

		ret = sc27xx_fgu_get_batt_energy_now(data, &clbcnt);
		if (ret) {
			dev_err(data->dev, "failed to get energy now.\n");
			return;
		}

		data->track.start_time = ktime_divns(ktime_get_boottime(), NSEC_PER_SEC);
		data->track.start_clbcnt = clbcnt;
		data->track.state = CAP_TRACK_UPDATING;
		dev_info(data->dev, "start_time = %lld, clbcnt = %d\n",
			 data->track.start_time, clbcnt);
		break;

	case CAP_TRACK_UPDATING:
		if ((ktime_divns(ktime_get_boottime(), NSEC_PER_SEC) -
		     data->track.start_time) > SC27XX_FGU_TRACK_TIMEOUT_THRESHOLD) {
			data->track.state = CAP_TRACK_IDLE;
			dev_err(data->dev, "track capacity time out.\n");
			return;
		}

		/*
		 * When the capacity tracking end condition is met,
		 * the battery voltage is almost full, so we use full
		 * stop charging condition as the the capacity
		 * tracking end condition.
		 */
		if (vbat_avg > data->track.end_vol &&
		    ibat_avg < data->track.end_cur) {
			if (!data->online) {
				dev_err(data->dev, "pwr not online\n");
				return;
			}


			if ((data->chg_type == POWER_SUPPLY_CHARGER_TYPE_UNKNOWN)
			    || (data->chg_type == POWER_SUPPLY_USB_CHARGER_TYPE_SDP)) {
				dev_err(data->dev, "chg_type not support, ret = %d\n", ret);
				return;
			}

			ret = sc27xx_fgu_get_batt_energy_now(data, &clbcnt);
			if (ret) {
				dev_err(data->dev, "failed to get energy now.\n");
				return;
			}

			total_cap = data->total_cap;
			dev_info(data->dev, "end ibat_avg = %d, vbat_avg = %d, ocv = %d\n",
				 ibat_avg, vbat_avg, ocv);
			/*
			 * Due to the capacity tracking function started, the
			 * coulomb amount corresponding to the initial
			 * percentage was not counted, so we need to
			 * compensate initial coulomb with following
			 * formula, we assume that coulomb and capacity
			 * are directly proportional.
			 *
			 * For example:
			 * if capacity tracking function started,  the battery
			 * percentage is 3%, we will count the capacity from
			 * 3% to 100%, it will discard capacity from 0% to 3%
			 * so we use "total_cap * (start_cap / 100)" to
			 * compensate.
			 *
			 * formula:
			 * capacity = total_cap * (start_cap / 100) + capacity
			 */
			capacity = (clbcnt - data->track.start_clbcnt) / 1000;
			dev_info(data->dev, "clbcnt = %d, start_clbcnt = %d,"
				 "capacity_temp = %d\n", clbcnt,
				 data->track.start_clbcnt, capacity);
			capacity = (total_cap * (u32)data->track.start_cap) / 1000 + (u32)capacity;
			dev_info(data->dev, "total_cap = %d, start_cap = %d, capacity = %d\n",
				 total_cap, data->track.start_cap, capacity);

			if (abs(capacity - total_cap) < total_cap / 5) {
				data->total_cap = capacity;
				data->track.state = CAP_TRACK_DONE;
				queue_delayed_work(system_power_efficient_wq,
						   &data->track.track_capacity_work,
						   0);
				dev_info(data->dev,
					 "track capacity is done capacity = %d, diff_cap = %d\n",
					 capacity, (capacity - (int)total_cap));
			} else {
				data->track.state = CAP_TRACK_IDLE;
				dev_info(data->dev,
					 "less than half standard capacity.\n");
			}
		}
		break;

	default:
		break;
	}
}

static void sc27xx_fgu_monitor(struct work_struct *work)
{

	struct sc27xx_fgu_data *data = container_of(work,
						  struct sc27xx_fgu_data,
						  track.fgu_update_work.work);

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	sc27xx_fgu_track_capacity_monitor(data);
	dev_info(data->dev, "track_sts: %d", data->track.state);
	queue_delayed_work(system_power_efficient_wq, &data->track.fgu_update_work, 15 * HZ);
}

static void sc27xx_fgu_track_capacity_init(struct sc27xx_fgu_data *data)
{
	INIT_DELAYED_WORK(&data->track.track_capacity_work,
			  sc27xx_fgu_track_capacity_work);
	INIT_DELAYED_WORK(&data->track.fgu_update_work, sc27xx_fgu_monitor);

	if (!data->track.end_vol || !data->track.end_cur)
		return;

	data->track.state = CAP_TRACK_INIT;

	pm_wakeup_event(data->dev, SC27XX_FGU_TRACK_WAKE_UP_MS);

	queue_delayed_work(system_power_efficient_wq,
			   &data->track.track_capacity_work,
			   5 * HZ);
	queue_delayed_work(system_power_efficient_wq, &data->track.fgu_update_work, 15 * HZ);
	dev_info(data->dev, "end_vol = %d, end_cur = %d\n", data->track.end_vol,
		 data->track.end_cur);
}

static int sc27xx_fgu_usb_change(struct notifier_block *nb,
				       unsigned long limit, void *info)
{
	u32 type;
	struct sc27xx_fgu_data *data =
		container_of(nb, struct sc27xx_fgu_data, usb_notify);

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return NOTIFY_OK;
	}

	pm_stay_awake(data->dev);

	if (limit)
		data->online = true;
	else
		data->online = false;

	type = data->usb_phy->chg_type;

	switch (type) {
	case SDP_TYPE:
		data->chg_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;

	case DCP_TYPE:
		data->chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;

	case CDP_TYPE:
		data->chg_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;

	default:
		data->chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	}

	pm_relax(data->dev);

	return NOTIFY_OK;
}

static ssize_t sc27xx_fgu_dump_info_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_dump_info);
	struct sc27xx_fgu_data *data = sysfs->data;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sc27xx_fgu_data is null\n", __func__);
	}

	return snprintf(buf, PAGE_SIZE, "[batt present:%d];\n[total_cap:%d];\n[init_cap:%d];\n"
			"[init_clbcnt:%d];\n[alarm_cap:%d];\n[boot_cap:%d];\n[normal_temp_cap:%d];\n"
			"[max_volt:%d];\n[min_volt:%d];\n[first_calib_volt:%d];\n[first_calib_cap:%d];\n"
			"[uusoc_vbat:%d];\n[boot_vol:%d];\n[last_clbcnt:%d];\n[cur_clbcnt:%d];\n"
			"[bat_temp:%d];\n[online:%d];\n[is_first_poweron:%d];\n[chg_type:%d]\n",
			data->bat_present, data->total_cap, data->init_cap, data->init_clbcnt,
			data->alarm_cap, data->boot_cap, data->normal_temp_cap, data->max_volt,
			data->min_volt, data->first_calib_volt, data->first_calib_cap,
			data->uusoc_vbat, data->boot_volt, data->last_clbcnt, data->cur_clbcnt,
			data->bat_temp, data->online, data->is_first_poweron, data->chg_type);
}

static ssize_t sc27xx_fgu_sel_reg_id_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_sel_reg_id);
	struct sc27xx_fgu_data *data = sysfs->data;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sc27xx_fgu_data is null\n", __func__);
	}

	return snprintf(buf, PAGE_SIZE, "[sel_reg_id:0x%x]\n", data->debug_info.sel_reg_id);
}

static ssize_t sc27xx_fgu_sel_reg_id_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_sel_reg_id);
	struct sc27xx_fgu_data *data = sysfs->data;
	u32 val;
	int ret;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return count;
	}

	ret =  kstrtouint(buf, 16, &val);
	if (ret) {
		dev_err(data->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	if (val > SC27XX_FGU_REG_MAX) {
		dev_err(data->dev, "val = %d, out of SC27XX_FGU_REG_MAX\n", val);
		return count;
	}

	data->debug_info.sel_reg_id = val;

	return count;
}

static ssize_t sc27xx_fgu_reg_val_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_reg_val);
	struct sc27xx_fgu_data *data = sysfs->data;
	u32 reg_val;
	int ret;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sc27xx_fgu_data is null\n", __func__);
	}

	ret = regmap_read(data->regmap,
			  data->base + data->debug_info.sel_reg_id,
			  &reg_val);
	if (ret)
		return snprintf(buf, PAGE_SIZE, "Fail to read [REG_0x%x], ret = %d\n",
				data->debug_info.sel_reg_id, ret);

	return snprintf(buf, PAGE_SIZE, "[REG_0x%x][0x%x]\n",
			data->debug_info.sel_reg_id, reg_val);
}

static ssize_t sc27xx_fgu_reg_val_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_reg_val);
	struct sc27xx_fgu_data *data = sysfs->data;
	u32 reg_val;
	int ret;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return count;
	}

	ret =  kstrtouint(buf, 16, &reg_val);
	if (ret) {
		dev_err(data->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	dev_info(data->dev, "Try to set [REG_0x%x][0x%x]\n", data->debug_info.sel_reg_id, reg_val);

	ret = regmap_write(data->regmap, data->base + data->debug_info.sel_reg_id, reg_val);
	if (ret)
		dev_err(data->dev, "fail to set [REG_0x%x][0x%x], ret = %d\n",
			data->debug_info.sel_reg_id, reg_val, ret);

	return count;
}

static ssize_t sc27xx_fgu_enable_sleep_calib_show(struct device *dev,
						  struct device_attribute *attr,
						  char *buf)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_enable_sleep_calib);
	struct sc27xx_fgu_data *data = sysfs->data;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sc27xx_fgu_data is null\n", __func__);
	}

	return snprintf(buf, PAGE_SIZE, "capacity sleep calibration function [%s]\n",
			data->slp_cap_calib.support_slp_calib ? "Enabled" : "Disabled");
}

static ssize_t sc27xx_fgu_enable_sleep_calib_store(struct device *dev,
						   struct device_attribute *attr,
						   const char *buf, size_t count)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_enable_sleep_calib);
	struct sc27xx_fgu_data *data = sysfs->data;
	bool enbale_slp_calib;
	int ret;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return count;
	}

	ret =  kstrtobool(buf, &enbale_slp_calib);
	if (ret) {
		dev_err(data->dev, "fail to get sleep_calib info, ret = %d\n", ret);
		return count;
	}

	data->slp_cap_calib.support_slp_calib = enbale_slp_calib;

	dev_info(data->dev, "Try to [%s] capacity sleep calibration function\n",
		 data->slp_cap_calib.support_slp_calib ? "Enabled" : "Disabled");

	return count;
}

static ssize_t sc27xx_fgu_relax_cnt_th_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_relax_cnt_th);
	struct sc27xx_fgu_data *data = sysfs->data;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sc27xx_fgu_data is null\n", __func__);
	}

	return snprintf(buf, PAGE_SIZE, "[relax_cnt_th][%d]\n",
			data->slp_cap_calib.relax_cnt_threshold);
}

static ssize_t sc27xx_fgu_relax_cnt_th_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_relax_cnt_th);
	struct sc27xx_fgu_data *data = sysfs->data;
	u32 relax_cnt;
	int ret;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return count;
	}

	ret =  kstrtouint(buf, 10, &relax_cnt);
	if (ret) {
		dev_err(data->dev, "fail to get relax_cnt info, ret = %d\n", ret);
		return count;
	}

	data->slp_cap_calib.relax_cnt_threshold = relax_cnt;

	dev_info(data->dev, "Try to set [relax_cnt_th] to [%d]\n",
		 data->slp_cap_calib.relax_cnt_threshold);

	return count;
}

static ssize_t sc27xx_fgu_relax_cur_th_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_relax_cur_th);
	struct sc27xx_fgu_data *data = sysfs->data;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sc27xx_fgu_data is null\n", __func__);
	}

	return snprintf(buf, PAGE_SIZE, "[relax_cur_th][%d]\n",
			data->slp_cap_calib.relax_cur_threshold);
}

static ssize_t sc27xx_fgu_relax_cur_th_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	struct sc27xx_fgu_sysfs *sysfs =
		container_of(attr, struct sc27xx_fgu_sysfs,
			     attr_sc27xx_fgu_relax_cur_th);
	struct sc27xx_fgu_data *data = sysfs->data;
	u32 relax_cur;
	int ret;

	if (!data) {
		dev_err(dev, "%s sc27xx_fgu_data is null\n", __func__);
		return count;
	}

	ret =  kstrtouint(buf, 10, &relax_cur);
	if (ret) {
		dev_err(data->dev, "fail to get relax_cnt info, ret = %d\n", ret);
		return count;
	}

	data->slp_cap_calib.relax_cur_threshold = relax_cur;

	dev_info(data->dev, "Try to set [relax_cur_th] to [%d]\n",
		 data->slp_cap_calib.relax_cur_threshold);

	return count;
}

static int sc27xx_fgu_register_sysfs(struct sc27xx_fgu_data *data)
{
	struct sc27xx_fgu_sysfs *sysfs;
	int ret;

	sysfs = devm_kzalloc(data->dev, sizeof(*sysfs), GFP_KERNEL);
	if (!sysfs)
		return -ENOMEM;

	data->sysfs = sysfs;
	sysfs->data = data;
	sysfs->name = "sc27xx_fgu_sysfs";
	sysfs->attrs[0] = &sysfs->attr_sc27xx_fgu_dump_info.attr;
	sysfs->attrs[1] = &sysfs->attr_sc27xx_fgu_sel_reg_id.attr;
	sysfs->attrs[2] = &sysfs->attr_sc27xx_fgu_reg_val.attr;
	sysfs->attrs[3] = &sysfs->attr_sc27xx_fgu_enable_sleep_calib.attr;
	sysfs->attrs[4] = &sysfs->attr_sc27xx_fgu_relax_cnt_th.attr;
	sysfs->attrs[5] = &sysfs->attr_sc27xx_fgu_relax_cur_th.attr;
	sysfs->attrs[6] = NULL;
	sysfs->attr_g.name = "debug";
	sysfs->attr_g.attrs = sysfs->attrs;

	sysfs_attr_init(&sysfs->attr_sc27xx_fgu_dump_info.attr);
	sysfs->attr_sc27xx_fgu_dump_info.attr.name = "dump_info";
	sysfs->attr_sc27xx_fgu_dump_info.attr.mode = 0444;
	sysfs->attr_sc27xx_fgu_dump_info.show = sc27xx_fgu_dump_info_show;

	sysfs_attr_init(&sysfs->attr_sc27xx_fgu_sel_reg_id.attr);
	sysfs->attr_sc27xx_fgu_sel_reg_id.attr.name = "sel_reg_id";
	sysfs->attr_sc27xx_fgu_sel_reg_id.attr.mode = 0644;
	sysfs->attr_sc27xx_fgu_sel_reg_id.show = sc27xx_fgu_sel_reg_id_show;
	sysfs->attr_sc27xx_fgu_sel_reg_id.store = sc27xx_fgu_sel_reg_id_store;

	sysfs_attr_init(&sysfs->attr_sc27xx_fgu_reg_val.attr);
	sysfs->attr_sc27xx_fgu_reg_val.attr.name = "reg_val";
	sysfs->attr_sc27xx_fgu_reg_val.attr.mode = 0644;
	sysfs->attr_sc27xx_fgu_reg_val.show = sc27xx_fgu_reg_val_show;
	sysfs->attr_sc27xx_fgu_reg_val.store = sc27xx_fgu_reg_val_store;

	sysfs_attr_init(&sysfs->attr_sc27xx_fgu_enable_sleep_calib.attr);
	sysfs->attr_sc27xx_fgu_enable_sleep_calib.attr.name = "enable_sleep_calib";
	sysfs->attr_sc27xx_fgu_enable_sleep_calib.attr.mode = 0644;
	sysfs->attr_sc27xx_fgu_enable_sleep_calib.show = sc27xx_fgu_enable_sleep_calib_show;
	sysfs->attr_sc27xx_fgu_enable_sleep_calib.store = sc27xx_fgu_enable_sleep_calib_store;

	sysfs_attr_init(&sysfs->attr_sc27xx_fgu_relax_cnt_th.attr);
	sysfs->attr_sc27xx_fgu_relax_cnt_th.attr.name = "relax_cnt_th";
	sysfs->attr_sc27xx_fgu_relax_cnt_th.attr.mode = 0644;
	sysfs->attr_sc27xx_fgu_relax_cnt_th.show = sc27xx_fgu_relax_cnt_th_show;
	sysfs->attr_sc27xx_fgu_relax_cnt_th.store = sc27xx_fgu_relax_cnt_th_store;

	sysfs_attr_init(&sysfs->attr_sc27xx_fgu_relax_cur_th.attr);
	sysfs->attr_sc27xx_fgu_relax_cur_th.attr.name = "relax_cur_th";
	sysfs->attr_sc27xx_fgu_relax_cur_th.attr.mode = 0644;
	sysfs->attr_sc27xx_fgu_relax_cur_th.show = sc27xx_fgu_relax_cur_th_show;
	sysfs->attr_sc27xx_fgu_relax_cur_th.store = sc27xx_fgu_relax_cur_th_store;

	ret = sysfs_create_group(&data->battery->dev.kobj, &sysfs->attr_g);
	if (ret < 0)
		dev_err(data->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static int sc27xx_fgu_hw_init(struct sc27xx_fgu_data *data,
			      const struct sc27xx_fgu_variant_data *pdata)
{
	struct power_supply_battery_info info = { };
	struct power_supply_battery_ocv_table *table;
	int ret, delta_clbcnt, alarm_adc;

	data->cur_now_buff[SC27XX_FGU_CURRENT_BUFF_CNT - 1] = SC27XX_FGU_MAGIC_NUMBER;

	ret = power_supply_get_battery_info(data->battery, &info);
	if (ret) {
		dev_err(data->dev, "failed to get battery information\n");
		return ret;
	}

	data->total_cap = info.charge_full_design_uah / 1000;
	data->max_volt = info.constant_charge_voltage_max_uv / 1000;
	data->internal_resist = info.factory_internal_resistance_uohm / 1000;
	data->min_volt = info.voltage_min_design_uv;

	/*
	 * For SC27XX fuel gauge device, we only use one ocv-capacity
	 * table in normal temperature 20 Celsius.
	 */
	table = power_supply_find_ocv2cap_table(&info, 20, &data->table_len);
	if (!table)
		return -EINVAL;

	data->cap_table = devm_kmemdup(data->dev, table,
				       data->table_len * sizeof(*table),
				       GFP_KERNEL);
	if (!data->cap_table) {
		power_supply_put_battery_info(data->battery, &info);
		return -ENOMEM;
	}

	ret = sc27xx_fgu_get_battery_table_info(data->battery, data);
	if (ret) {
		dev_err(data->dev, "failed to get battery table information\n");
		return ret;
	}

	data->alarm_cap = power_supply_ocv2cap_simple(data->cap_table,
						      data->table_len,
						      data->min_volt);
	data->alarm_cap *= 10;

	if (!data->alarm_cap)
		data->alarm_cap += 10;

	data->resist_table_len = info.resist_table_size;
	if (data->resist_table_len > 0) {
		data->resist_table = devm_kmemdup(data->dev, info.resist_table,
						  data->resist_table_len *
						  sizeof(struct power_supply_resistance_temp_table),
						  GFP_KERNEL);
		if (!data->resist_table) {
			power_supply_put_battery_info(data->battery, &info);
			return -ENOMEM;
		}
	}

	power_supply_put_battery_info(data->battery, &info);

	ret = sc27xx_fgu_calibration(data);
	if (ret)
		return ret;

	/* Enable the FGU module */
	ret = regmap_update_bits(data->regmap, pdata->module_en,
				 SC27XX_FGU_EN, SC27XX_FGU_EN);
	if (ret) {
		dev_err(data->dev, "failed to enable fgu\n");
		return ret;
	}

	/* Enable the FGU RTC clock to make it work */
	ret = regmap_update_bits(data->regmap, pdata->clk_en,
				 SC27XX_FGU_RTC_EN, SC27XX_FGU_RTC_EN);
	if (ret) {
		dev_err(data->dev, "failed to enable fgu RTC clock\n");
		goto disable_fgu;
	}

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_CLR,
				 SC27XX_FGU_INT_MASK, SC27XX_FGU_INT_MASK);
	if (ret) {
		dev_err(data->dev, "failed to clear interrupt status\n");
		goto disable_clk;
	}

	/*
	 * Set the voltage low overload threshold, which means when the battery
	 * voltage is lower than this threshold, the controller will generate
	 * one interrupt to notify.
	 */
	alarm_adc = sc27xx_fgu_voltage_to_adc(data, data->min_volt / 1000);
	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_LOW_OVERLOAD,
				 SC27XX_FGU_LOW_OVERLOAD_MASK, alarm_adc);
	if (ret) {
		dev_err(data->dev, "failed to set fgu low overload\n");
		goto disable_clk;
	}

	/*
	 * Set the coulomb counter delta threshold, that means when the coulomb
	 * counter change is multiples of the delta threshold, the controller
	 * will generate one interrupt to notify the users to update the battery
	 * capacity. Now we set the delta threshold as a counter value of 1%
	 * capacity.
	 */
	delta_clbcnt = sc27xx_fgu_cap_to_clbcnt(data, 10);

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_CLBCNT_DELTL,
				 SC27XX_FGU_CLBCNT_MASK, delta_clbcnt);
	if (ret) {
		dev_err(data->dev, "failed to set low delta coulomb counter\n");
		goto disable_clk;
	}

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_CLBCNT_DELTH,
				 SC27XX_FGU_CLBCNT_MASK,
				 delta_clbcnt >> SC27XX_FGU_CLBCNT_SHIFT);
	if (ret) {
		dev_err(data->dev, "failed to set high delta coulomb counter\n");
		goto disable_clk;
	}

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_CONFIG,
				 SC27XX_FGU_LOW_POWER_MODE, SC27XX_FGU_RELAX_CNT_MODE);
	if (ret) {
		dev_err(data->dev, "Fail to enable RELAX_CNT_MODE, re= %d\n", ret);
		goto disable_clk;
	}

	/*
	 * Get the boot battery capacity when system powers on, which is used to
	 * initialize the coulomb counter. After that, we can read the coulomb
	 * counter to measure the battery capacity.
	 */
	ret = sc27xx_fgu_get_boot_capacity(data, &data->init_cap);
	if (ret) {
		dev_err(data->dev, "failed to get boot capacity\n");
		goto disable_clk;
	}

	/*
	 * Convert battery capacity to the corresponding initial coulomb counter
	 * and set into coulomb counter registers.
	 */
	data->init_clbcnt = sc27xx_fgu_cap_to_clbcnt(data, data->init_cap);
	data->last_clbcnt = data->cur_clbcnt = data->init_clbcnt;
	ret = sc27xx_fgu_set_clbcnt(data, data->init_clbcnt);
	if (ret) {
		dev_err(data->dev, "failed to initialize coulomb counter\n");
		goto disable_clk;
	}

	ret = sc27xx_fgu_get_temp(data, &data->bat_temp);
	if (ret) {
		dev_err(data->dev, "failed to get battery temperature\n");
		goto disable_clk;
	}

	return 0;

disable_clk:
	regmap_update_bits(data->regmap, pdata->clk_en, SC27XX_FGU_RTC_EN, 0);
disable_fgu:
	regmap_update_bits(data->regmap, pdata->module_en, SC27XX_FGU_EN, 0);

	return ret;
}

static int sc27xx_fgu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct power_supply_config fgu_cfg = { };
	struct sc27xx_fgu_data *data;
	int ret, irq;

	if (!np || !dev) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}

	data->dev = &pdev->dev;
	platform_set_drvdata(pdev, data);

	data->pdata = of_device_get_match_data(&pdev->dev);
	if (!data->pdata) {
		dev_err(&pdev->dev, "no matching driver data found\n");
		return -EINVAL;
	}

	data->regmap = dev_get_regmap(dev->parent, NULL);
	if (!data->regmap) {
		dev_err(dev, "failed to get regmap\n");
		return -ENODEV;
	}

	data->track.cap_tracking = device_property_read_bool(dev, "fgu-capacity-track");
	if (data->track.cap_tracking) {
		data->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
		if (IS_ERR(data->usb_phy)) {
			dev_err(dev, "failed to find USB phy\n");
			return -EPROBE_DEFER;
		}
	}

	data->channel = devm_iio_channel_get(dev, "bat-temp");
	if (IS_ERR(data->channel)) {
		dev_err(dev, "failed to get IIO channel, ret = %ld\n", PTR_ERR(data->channel));
		return PTR_ERR(data->channel);
	}

	data->charge_chan = devm_iio_channel_get(dev, "charge-vol");
	if (IS_ERR(data->charge_chan)) {
		dev_err(dev, "failed to get charge IIO channel, ret = %ld\n",
			PTR_ERR(data->charge_chan));
		return PTR_ERR(data->charge_chan);
	}

	ret = device_property_read_u32(dev, "reg", &data->base);
	if (ret) {
		dev_err(dev, "failed to get fgu address\n");
		return ret;
	}

	ret = device_property_read_u32(dev, "sprd,calib-resistance-micro-ohms",
				       &data->calib_resist);
	if (ret) {
		dev_err(dev, "failed to get fgu calibration resistance\n");
		return ret;
	}

	ret = device_property_read_u32(dev, "first-calib-voltage", &data->first_calib_volt);
	if (ret)
		dev_warn(dev, "failed to get fgu first calibration voltage\n");

	ret = device_property_read_u32(dev, "first-calib-capacity", &data->first_calib_cap);
	if (ret)
		dev_warn(dev, "failed to get fgu first calibration capacity\n");

	ret = device_property_read_u32(dev, "sprd,comp-resistance-mohm", &data->comp_resistance);
	if (ret)
		dev_warn(dev, "no fgu compensated resistance support\n");

	ret = device_property_read_u32(dev, "fullbatt-track-end-cur", &data->track.end_cur);
	if (ret)
		dev_warn(dev, "no fgu track.end_cur support\n");

	ret = device_property_read_u32(dev, "fullbatt-track-end-vol", &data->track.end_vol);
	if (ret)
		dev_warn(dev, "no fgu track.end_vol support\n");

	data->slp_cap_calib.support_slp_calib =
		device_property_read_bool(dev, "sprd,capacity-sleep-calibration");
	if (!data->slp_cap_calib.support_slp_calib) {
		dev_warn(&pdev->dev, "no fgu capacity sleep calibration support\n");
	} else {
		ret = device_property_read_u32(dev, "sprd,relax-counter-threshold",
					       &data->slp_cap_calib.relax_cnt_threshold);
		if (ret)
			dev_warn(dev, "no relax-counter-threshold support\n");

		ret = device_property_read_u32(dev, "sprd,relax-current-threshold",
					       &data->slp_cap_calib.relax_cur_threshold);
		if (ret)
			dev_warn(dev, "no relax_current_threshold support\n");

		if (data->slp_cap_calib.relax_cnt_threshold < SC27XX_FGU_RELAX_CNT_THRESHOLD)
			data->slp_cap_calib.relax_cnt_threshold = SC27XX_FGU_RELAX_CNT_THRESHOLD;

		if (data->slp_cap_calib.relax_cur_threshold == 0)
			data->slp_cap_calib.relax_cur_threshold = SC27XX_FGU_RELAX_CUR_THRESHOLD_MA;

		ret = sc27xx_fgu_parse_energy_density_ocv_table(data);
		if (ret) {
			dev_err(dev, "Fail to parse energy density ocv table, ret = %d\n", ret);
			return ret;
		}
	}

	data->gpiod = devm_gpiod_get(dev, "bat-detect", GPIOD_IN);
	if (IS_ERR(data->gpiod)) {
		dev_err(dev, "failed to get battery detection GPIO\n");
		return PTR_ERR(data->gpiod);
	}

	ret = gpiod_get_value_cansleep(data->gpiod);
	if (ret < 0) {
		dev_err(dev, "failed to get gpio state\n");
		return ret;
	}

	data->bat_present = !!ret;
	mutex_init(&data->lock);
	mutex_lock(&data->lock);

	fgu_cfg.drv_data = data;
	fgu_cfg.of_node = np;
	data->battery = devm_power_supply_register(dev, &sc27xx_fgu_desc, &fgu_cfg);
	if (IS_ERR(data->battery)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(data->battery);
		goto err;
	}

	ret = devm_add_action_or_reset(dev, sc27xx_fgu_disable, data);
	if (ret) {
		dev_err(dev, "failed to add fgu disable action\n");
		goto err;
	}

	ret = get_boot_mode();
	if (ret) {
		pr_err("get_boot_mode can't not parse bootargs property\n");
		goto err;
	}

	ret = sc27xx_fgu_hw_init(data, data->pdata);
	if (ret) {
		dev_err(dev, "failed to initialize fgu hardware\n");
		goto err;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "no irq resource specified\n");
		ret = irq;
		goto err;
	}

	ret = devm_request_threaded_irq(dev, irq, NULL,	sc27xx_fgu_interrupt,
					IRQF_NO_SUSPEND | IRQF_ONESHOT,
					pdev->name, data);
	if (ret) {
		dev_err(dev, "failed to request fgu IRQ\n");
		goto err;
	}

	irq = gpiod_to_irq(data->gpiod);
	if (irq < 0) {
		dev_err(dev, "failed to translate GPIO to IRQ\n");
		ret = irq;
		goto err;
	}

	ret = devm_request_threaded_irq(dev, irq, NULL,
					sc27xx_fgu_bat_detection,
					IRQF_ONESHOT | IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING,
					pdev->name, data);
	if (ret) {
		dev_err(dev, "failed to request IRQ\n");
		goto err;
	}

	if (data->track.cap_tracking && data->track.end_vol && data->track.end_cur) {
		device_init_wakeup(dev, true);
		data->usb_notify.notifier_call = sc27xx_fgu_usb_change;
		ret = usb_register_notifier(data->usb_phy, &data->usb_notify);
		if (ret) {
			dev_err(dev, "failed to register notifier:%d\n", ret);
			goto err;
		}
		sc27xx_fgu_track_capacity_init(data);
	}

	ret = sc27xx_fgu_register_sysfs(data);
	if (ret)
		dev_err(&pdev->dev, "register sysfs fail, ret = %d\n", ret);

	mutex_unlock(&data->lock);
	return 0;

err:
	sc27xx_fgu_disable(data);
	mutex_unlock(&data->lock);
	mutex_destroy(&data->lock);
	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int sc27xx_fgu_resume(struct device *dev)
{
	struct sc27xx_fgu_data *data = dev_get_drvdata(dev);
	int ret;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	sc27xx_fgu_suspend_calib_check(data);

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_EN,
				 SC27XX_FGU_LOW_OVERLOAD_INT |
				 SC27XX_FGU_CLBCNT_DELTA_INT, 0);
	if (ret) {
		dev_err(data->dev, "failed to disable fgu interrupts\n");
		return ret;
	}

	if (data->track.cap_tracking)
		sc27xx_fgu_monitor(&data->track.fgu_update_work.work);

	return 0;
}

static int sc27xx_fgu_suspend(struct device *dev)
{
	struct sc27xx_fgu_data *data = dev_get_drvdata(dev);
	int ret, status, ocv;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = sc27xx_fgu_get_status(data, &status);
	if (ret) {
		dev_err(data->dev, "failed to get charging status, ret = %d\n", ret);
		return ret;
	}

	/*
	 * If we are charging, then no need to enable the FGU interrupts to
	 * adjust the battery capacity.
	 */
	if (status != POWER_SUPPLY_STATUS_NOT_CHARGING &&
	    status != POWER_SUPPLY_STATUS_DISCHARGING)
		return 0;

	ret = regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_EN,
				 SC27XX_FGU_LOW_OVERLOAD_INT,
				 SC27XX_FGU_LOW_OVERLOAD_INT);
	if (ret) {
		dev_err(data->dev, "failed to enable low voltage interrupt\n");
		return ret;
	}

	ret = sc27xx_fgu_get_vbat_ocv(data, &ocv);
	if (ret)
		goto disable_int;

	/*
	 * If current OCV is less than the minimum voltage, we should enable the
	 * coulomb counter threshold interrupt to notify events to adjust the
	 * battery capacity.
	 */
	if (ocv < data->min_volt) {
		ret = regmap_update_bits(data->regmap,
					 data->base + SC27XX_FGU_INT_EN,
					 SC27XX_FGU_CLBCNT_DELTA_INT,
					 SC27XX_FGU_CLBCNT_DELTA_INT);
		if (ret) {
			dev_err(data->dev,
				"failed to enable coulomb threshold int\n");
			goto disable_int;
		}
	}

	if (data->track.cap_tracking) {
		cancel_delayed_work_sync(&data->track.track_capacity_work);
		cancel_delayed_work_sync(&data->track.fgu_update_work);
	}

	sc27xx_fgu_suspend_calib_config(data);

	return 0;

disable_int:
	regmap_update_bits(data->regmap, data->base + SC27XX_FGU_INT_EN,
			   SC27XX_FGU_LOW_OVERLOAD_INT, 0);
	return ret;
}
#endif

static const struct dev_pm_ops sc27xx_fgu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sc27xx_fgu_suspend, sc27xx_fgu_resume)
};

static const struct of_device_id sc27xx_fgu_of_match[] = {
	{ .compatible = "sprd,sc2731-fgu", .data = &sc2731_info},
	{ .compatible = "sprd,sc2730-fgu", .data = &sc2730_info},
	{ .compatible = "sprd,sc2720-fgu", .data = &sc2720_info},
	{ .compatible = "sprd,ump9620-fgu", .data = &ump9620_info},
	{ }
};

static struct platform_driver sc27xx_fgu_driver = {
	.probe = sc27xx_fgu_probe,
	.driver = {
		.name = "sc27xx-fgu",
		.of_match_table = sc27xx_fgu_of_match,
		.pm = &sc27xx_fgu_pm_ops,
	}
};

module_platform_driver(sc27xx_fgu_driver);

MODULE_DESCRIPTION("Spreadtrum SC27XX PMICs Fual Gauge Unit Driver");
MODULE_LICENSE("GPL v2");
