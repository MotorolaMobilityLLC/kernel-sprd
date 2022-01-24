/*
 * Driver for the TI bq2560x charger.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/alarmtimer.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/sysfs.h>
#include <linux/usb/phy.h>
#include <linux/pm_wakeup.h>
#include <uapi/linux/usb/charger.h>

#define BQ2560X_REG_0				0x0
#define BQ2560X_REG_1				0x1
#define BQ2560X_REG_2				0x2
#define BQ2560X_REG_3				0x3
#define BQ2560X_REG_4				0x4
#define BQ2560X_REG_5				0x5
#define BQ2560X_REG_6				0x6
#define BQ2560X_REG_7				0x7
#define BQ2560X_REG_8				0x8
#define BQ2560X_REG_9				0x9
#define BQ2560X_REG_A				0xa
#define BQ2560X_REG_B				0xb
#define BQ2560X_REG_C				0xc
#define BQ2560X_REG_D				0xd
#define BQ2560X_REG_E				0xe
#define BQ2560X_REG_F				0xf
#define BQ2560X_REG_NUM				12

#define BQ2560X_BATTERY_NAME			"sc27xx-fgu"
#define BIT_DP_DM_BC_ENB			BIT(0)
#define BQ2560X_OTG_ALARM_TIMER_MS		15000

#define	BQ2560X_REG_IINLIM_BASE			100

#define BQ2560X_REG_ICHG_LSB			60

#define BQ2560X_REG_ICHG_MASK			GENMASK(5, 0)

#define BQ2560X_REG_CHG_MASK			GENMASK(4, 4)
#define BQ2560X_REG_CHG_SHIFT			4

#define BQ2560X_REG_EN_TIMER_MASK	GENMASK(3, 3)


#define BQ2560X_REG_RESET_MASK			GENMASK(6, 6)

#define BQ2560X_REG_OTG_MASK			GENMASK(5, 5)
#define BQ2560X_REG_BOOST_FAULT_MASK		GENMASK(7, 5)

#define BQ2560X_REG_WATCHDOG_MASK		GENMASK(6, 6)

#define BQ2560X_REG_WATCHDOG_TIMER_MASK		GENMASK(5, 4)
#define BQ2560X_REG_WATCHDOG_TIMER_SHIFT	4

#define BQ2560X_REG_TERMINAL_VOLTAGE_MASK	GENMASK(7, 3)
#define BQ2560X_REG_TERMINAL_VOLTAGE_SHIFT	3

#define BQ2560X_REG_TERMINAL_CUR_MASK		GENMASK(3, 0)

#define BQ2560X_REG_VINDPM_VOLTAGE_MASK		GENMASK(3, 0)
#define BQ2560X_REG_OVP_MASK			GENMASK(7, 6)
#define BQ2560X_REG_OVP_SHIFT			6

#define BQ2560X_REG_EN_HIZ_MASK			GENMASK(7, 7)
#define BQ2560X_REG_EN_HIZ_SHIFT		7

#define BQ2560X_REG_LIMIT_CURRENT_MASK		GENMASK(4, 0)

#define BQ2560X_DISABLE_PIN_MASK		BIT(0)
#define BQ2560X_DISABLE_PIN_MASK_2721		BIT(15)

#define BQ2560X_OTG_VALID_MS			500
#define BQ2560X_FEED_WATCHDOG_VALID_MS		50
#define BQ2560X_OTG_RETRY_TIMES			10
#define BQ2560X_LIMIT_CURRENT_MAX		3200000
#define BQ2560X_LIMIT_CURRENT_OFFSET		100000
#define BQ2560X_REG_IINDPM_LSB			100

#define BQ2560X_ROLE_MASTER_DEFAULT		1
#define BQ2560X_ROLE_SLAVE			2

#define BQ2560X_FCHG_OVP_6V			6000
#define BQ2560X_FCHG_OVP_9V			9000
#define BQ2560X_FCHG_OVP_14V			14000
#define BQ2560X_FAST_CHARGER_VOLTAGE_MAX	10500000
#define BQ2560X_NORMAL_CHARGER_VOLTAGE_MAX	6500000

#define VBUS_9V 9000000
#define VBUS_7V 7000000
#define VBUS_5V 5000000
#define VBUS_1V 1000000
#define V_500MV 700000
#define I_500MA 500000


#define BQ2560X_WAKE_UP_MS			1000
#define BQ2560X_CURRENT_WORK_MS			msecs_to_jiffies(100)

struct bq2560x_charger_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_bq2560x_dump_reg;
	struct device_attribute attr_bq2560x_lookup_reg;
	struct device_attribute attr_bq2560x_sel_reg_id;
	struct device_attribute attr_bq2560x_reg_val;
	struct attribute *attrs[5];

	struct bq2560x_charger_info *info;
};

struct bq2560x_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct power_supply *psy_usb;
	struct power_supply_charge_current cur;
	struct work_struct work;
	struct mutex lock;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	struct delayed_work cur_work;
	struct regmap *pmic;
	struct gpio_desc *gpiod;
	struct extcon_dev *edev;
	struct alarm otg_timer;
	struct bq2560x_charger_sysfs *sysfs;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	u32 limit;
	u32 new_charge_limit_cur;
	u32 current_charge_limit_cur;
	u32 new_input_limit_cur;
	u32 current_input_limit_cur;
	u32 last_limit_cur;
	u32 actual_limit_cur;
	u32 role;
	bool charging;
	bool need_disable_Q1;
	int termination_cur;
	bool otg_enable;
	unsigned int irq_gpio;
	bool is_wireless_charge;

	int reg_id;
	bool disable_power_path;

	bool enable_pe;
       bool enable_qc;
	u32 current_vbus;
	u32  set_vbus;
	struct delayed_work hand_work;
	unsigned int dpdm_gpio;
	
};

struct bq2560x_charger_reg_tab {
	int id;
	u32 addr;
	char *name;
};

static struct bq2560x_charger_reg_tab reg_tab[BQ2560X_REG_NUM + 1] = {
	{0, BQ2560X_REG_0, "EN_HIZ/EN_ICHG_MON/IINDPM"},
	{1, BQ2560X_REG_1, "PFM _DIS/WD_RST/OTG_CONFIG/CHG_CONFIG/SYS_Min/Min_VBAT_SEL"},
	{2, BQ2560X_REG_2, "BOOST_LIM/Q1_FULLON/ICHG"},
	{3, BQ2560X_REG_3, "IPRECHG/ITERM"},
	{4, BQ2560X_REG_4, "VREG/TOPOFF_TIMER/VRECHG"},
	{5, BQ2560X_REG_5, "EN_TERM/WATCHDOG/EN_TIMER/CHG_TIMER/TREG/JEITA_ISET"},
	{6, BQ2560X_REG_6, "OVP/BOOSTV/VINDPM"},
	{7, BQ2560X_REG_7, "IINDET_EN/TMR2X_EN/BATFET_DIS/JEITA_VSET/BATFET_DLY/"
				"BATFET_RST_EN/VDPM_BAT_TRACK"},
	{8, BQ2560X_REG_8, "VBUS_STAT/CHRG_STAT/PG_STAT/THERM_STAT/VSYS_STAT"},
	{9, BQ2560X_REG_9, "WATCHDOG_FAULT/BOOST_FAULT/CHRG_FAULT/BAT_FAULT/NTC_FAULT"},
	{10, BQ2560X_REG_A, "VBUS_GD/VINDPM_STAT/IINDPM_STAT/TOPOFF_ACTIVE/ACOV_STAT/"
				"VINDPM_INT_ MASK/IINDPM_INT_ MASK"},
	{11, BQ2560X_REG_B, "REG_RST/PN/DEV_REV"},
	{12, 0, "null"},
};

#include <ontim/ontim_dev_dgb.h>
static  char charge_ic_vendor_name[50]="BQ2560x";
DEV_ATTR_DECLARE(charge_ic)
DEV_ATTR_DEFINE("vendor",charge_ic_vendor_name)
DEV_ATTR_DECLARE_END;
ONTIM_DEBUG_DECLARE_AND_INIT(charge_ic,charge_ic,8);

static void power_path_control(struct bq2560x_charger_info *info)
{
	extern char *saved_command_line;
	char result[5];
	char *match = strstr(saved_command_line, "androidboot.mode=");

	if (match) {
		memcpy(result, (match + strlen("androidboot.mode=")),
		       sizeof(result) - 1);
		if ((!strcmp(result, "cali")) || (!strcmp(result, "auto")))
			info->disable_power_path = true;
	}
}

static int
bq2560x_charger_set_limit_current(struct bq2560x_charger_info *info,
				  u32 limit_cur);

static bool bq2560x_charger_is_bat_present(struct bq2560x_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(BQ2560X_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return present;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&val);
	if (ret == 0 && val.intval)
		present = true;
	power_supply_put(psy);

	if (ret)
		dev_err(info->dev,
			"Failed to get property of present:%d\n", ret);

	return present;
}

static int bq2560x_charger_is_fgu_present(struct bq2560x_charger_info *info)
{
	struct power_supply *psy;

	psy = power_supply_get_by_name(BQ2560X_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to find psy of sc27xx_fgu\n");
		return -ENODEV;
	}
	power_supply_put(psy);

	return 0;
}

static int bq2560x_block_read(struct bq2560x_charger_info *info, u8 reg, u8 *data, u32 len)
{

	return i2c_smbus_read_i2c_block_data(info->client, reg, len, data);
}

static int bq2560x_read(struct bq2560x_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int bq2560x_write(struct bq2560x_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int bq2560x_update_bits(struct bq2560x_charger_info *info, u8 reg,
			       u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = bq2560x_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return bq2560x_write(info, reg, v);
}

static void bq2560x_dump_regs(struct bq2560x_charger_info *info)
{

	u8 val[0x10];
	int ret;

	ret = bq2560x_block_read(info, BQ2560X_REG_0, &val[0],0x10);


	dev_err(info->dev,"bq25601 [0]=%.2x [1]=%.2x [2]=%.2x [3]=%.2x  [4]=%.2x [5]=%.2x [6]=%.2x [7]=%.2x\n",
		                      val[0],val[1],val[2],val[3],val[4],val[5],val[6], val[7]);
	dev_err(info->dev,"bq25601 [8]=%.2x [9]=%.2x [a]=%.2x [b]=%.2x  [c]=%.2x [d]=%.2x [e]=%.2x [f]=%.2x\n",
		                      val[8],val[9],val[0xa],val[0xb],val[0xc],val[0xd],val[0xe],val[0xf]);
}


static int
bq2560x_charger_set_vindpm(struct bq2560x_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < 3900)
		reg_val = 0x0;
	else if (vol > 5400)
		reg_val = 0x0f;
	else
		reg_val = (vol - 3900) / 100;

	return bq2560x_update_bits(info, BQ2560X_REG_6,
				   BQ2560X_REG_VINDPM_VOLTAGE_MASK, reg_val);
}
static int  bq2560x_enable_powerpath(struct bq2560x_charger_info *info, bool en)
{
	int ret;

	dev_err(info->dev,"%s; %d;\n", __func__,en);

	if(en)
		ret=bq2560x_charger_set_vindpm(info, 4500);
	else	
		ret=bq2560x_charger_set_vindpm(info, 5400);

	return ret;
}

static int
bq2560x_charger_set_ovp(struct bq2560x_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < 5500)
		reg_val = 0x0;
	else if (vol > 5500 && vol < 6500)
		reg_val = 0x01;
	else if (vol > 6500 && vol < 10500)
		reg_val = 0x02;
	else
		reg_val = 0x03;

	return bq2560x_update_bits(info, BQ2560X_REG_6,
				   BQ2560X_REG_OVP_MASK,
				   reg_val << BQ2560X_REG_OVP_SHIFT);
}

static int
bq2560x_charger_set_termina_vol(struct bq2560x_charger_info *info, u32 vol)
{
	u8 reg_val;

	dev_err(info->dev, "%s;%d;\n",__func__,vol);

	if (vol < 3856)
		reg_val = 0x0;
	else
		reg_val = (vol+32 - 3856) / 32;

	return bq2560x_update_bits(info, BQ2560X_REG_4,
				   BQ2560X_REG_TERMINAL_VOLTAGE_MASK,
				   reg_val << BQ2560X_REG_TERMINAL_VOLTAGE_SHIFT);
}

static int
bq2560x_charger_get_termina_vol(struct bq2560x_charger_info *info, u32 *vol)
{
	u8 reg_val;
	int ret;

	ret = bq2560x_read(info, BQ2560X_REG_4, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= BQ2560X_REG_TERMINAL_VOLTAGE_MASK;
	*vol = 3856 + (reg_val >> BQ2560X_REG_TERMINAL_VOLTAGE_SHIFT) * 32 ;

	return 0;
}

static int
bq2560x_charger_set_termina_cur(struct bq2560x_charger_info *info, u32 cur)
{
	u8 reg_val;

	if (cur <= 60)
		reg_val = 0x0;
	else if (cur >= 480)
		reg_val = 0x8;
	else
		reg_val = (cur - 60) / 60;

	return bq2560x_update_bits(info, BQ2560X_REG_3,
				   BQ2560X_REG_TERMINAL_CUR_MASK,
				   reg_val);
}

static int bq2560x_charger_hw_init(struct bq2560x_charger_info *info)
{
	struct power_supply_battery_info bat_info = { };
	int voltage_max_microvolt, termination_cur;
	int ret;

	ret = power_supply_get_battery_info(info->psy_usb, &bat_info, 0);
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");

		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 100 mA, and default
		 * charge termination voltage to 4.2V.
		 */
		info->cur.sdp_limit = 500000;
		info->cur.sdp_cur = 500000;
		info->cur.dcp_limit = 5000000;
		info->cur.dcp_cur = 500000;
		info->cur.cdp_limit = 5000000;
		info->cur.cdp_cur = 1500000;
		info->cur.unknown_limit = 5000000;
		info->cur.unknown_cur = 500000;
	} else {
		info->cur.sdp_limit = bat_info.cur.sdp_limit;
		info->cur.sdp_cur = bat_info.cur.sdp_cur;
		info->cur.dcp_limit = bat_info.cur.dcp_limit;
		info->cur.dcp_cur = bat_info.cur.dcp_cur;
		info->cur.cdp_limit = bat_info.cur.cdp_limit;
		info->cur.cdp_cur = bat_info.cur.cdp_cur;
		info->cur.unknown_limit = bat_info.cur.unknown_limit;
		info->cur.unknown_cur = bat_info.cur.unknown_cur;
		info->cur.fchg_limit = bat_info.cur.fchg_limit;
		info->cur.fchg_cur = bat_info.cur.fchg_cur;

		voltage_max_microvolt =
			bat_info.constant_charge_voltage_max_uv / 1000;
		termination_cur = bat_info.charge_term_current_ua / 1000;
		info->termination_cur = termination_cur;
		power_supply_put_battery_info(info->psy_usb, &bat_info);

		//ret = bq2560x_update_bits(info, BQ2560X_REG_B,
		//			  BQ2560X_REG_RESET_MASK,
		//			  BQ2560X_REG_RESET_MASK);

		if (ret) {
			dev_err(info->dev, "reset bq2560x failed\n");
			return ret;
		}

		if (info->role == BQ2560X_ROLE_MASTER_DEFAULT) {
			ret = bq2560x_charger_set_ovp(info, BQ2560X_FCHG_OVP_6V);
			if (ret) {
				dev_err(info->dev, "set bq2560x ovp failed\n");
				return ret;
			}
		} else if (info->role == BQ2560X_ROLE_SLAVE) {
			ret = bq2560x_charger_set_ovp(info, BQ2560X_FCHG_OVP_9V);
			if (ret) {
				dev_err(info->dev, "set bq2560x slave ovp failed\n");
				return ret;
			}
		}

		ret = bq2560x_charger_set_vindpm(info, 4500);
		if (ret) {
			dev_err(info->dev, "set bq2560x vindpm vol failed\n");
			return ret;
		}

		ret = bq2560x_charger_set_termina_vol(info,
						      voltage_max_microvolt);
		if (ret) {
			dev_err(info->dev, "set bq2560x terminal vol failed\n");
			return ret;
		}

		ret = bq2560x_charger_set_termina_cur(info, termination_cur);
		if (ret) {
			dev_err(info->dev, "set bq2560x terminal cur failed\n");
			return ret;
		}

		ret = bq2560x_charger_set_limit_current(info,
							info->cur.unknown_cur);
		if (ret)
			dev_err(info->dev, "set bq2560x limit current failed\n");

		ret = bq2560x_update_bits(info, BQ2560X_REG_5,
					  BQ2560X_REG_EN_TIMER_MASK,
					  0);
		ret = bq2560x_update_bits(info, BQ2560X_REG_5,     //WATCHDOG
					  0x30,
					  0);		
		ret = bq2560x_update_bits(info, BQ2560X_REG_4,   //TOPOFF_TIMER
					  0x06,
					  0);
		ret = bq2560x_update_bits(info, BQ2560X_REG_F,   //VREG_FIT  -16   //4.448v
					  0xc0,
					  0xc0);

//		ret = bq2560x_update_bits(info, BQ2560X_REG_F,   //VREG_FIT  +8            //4.472v
//					  0x40,
//					  0x40);

	}

	info->current_charge_limit_cur = BQ2560X_REG_ICHG_LSB * 1000;
	info->current_input_limit_cur = BQ2560X_REG_IINDPM_LSB * 1000;

	bq2560x_dump_regs(info);
	
	return ret;
}

static int
bq2560x_charger_get_charge_voltage(struct bq2560x_charger_info *info,
				   u32 *charge_vol)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(BQ2560X_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "failed to get BQ2560X_BATTERY_NAME\n");
		return -ENODEV;
	}

	ret = power_supply_get_property(psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	power_supply_put(psy);
	if (ret) {
		dev_err(info->dev, "failed to get CONSTANT_CHARGE_VOLTAGE\n");
		return ret;
	}

	*charge_vol = val.intval;

	return 0;
}

static int bq2560x_charger_start_charge(struct bq2560x_charger_info *info)
{
	int ret = 0;

	ret = bq2560x_update_bits(info, BQ2560X_REG_0,
				  BQ2560X_REG_EN_HIZ_MASK, 0);
	if (ret)
		dev_err(info->dev, "disable HIZ mode failed\n");


	if (info->role == BQ2560X_ROLE_MASTER_DEFAULT) {
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask, 0);
		if (ret) {
			dev_err(info->dev, "enable bq2560x charge failed\n");
			return ret;
		}

		ret = bq2560x_update_bits(info, BQ2560X_REG_1,
					  BQ2560X_REG_CHG_MASK,
					  0x1 << BQ2560X_REG_CHG_SHIFT);
		if (ret) {
			dev_err(info->dev, "enable bq2560x charge en failed\n");
			return ret;
		}
	} else if (info->role == BQ2560X_ROLE_SLAVE) {
		gpiod_set_value_cansleep(info->gpiod, 0);
	}

	ret = bq2560x_charger_set_limit_current(info,
						info->last_limit_cur);
	if (ret) {
		dev_err(info->dev, "failed to set limit current\n");
		return ret;
	}

	ret = bq2560x_charger_set_termina_cur(info, info->termination_cur);
	if (ret)
		dev_err(info->dev, "set bq2560x terminal cur failed\n");

	return ret;
}

static void bq2560x_charger_stop_charge(struct bq2560x_charger_info *info)
{
	int ret;
	bool present = bq2560x_charger_is_bat_present(info);

	if (info->role == BQ2560X_ROLE_MASTER_DEFAULT) {
		if (!present || info->need_disable_Q1) {
//			ret = bq2560x_update_bits(info, BQ2560X_REG_0,
//						  BQ2560X_REG_EN_HIZ_MASK,
//						  0x01 << BQ2560X_REG_EN_HIZ_SHIFT);
//			if (ret)
//				dev_err(info->dev, "enable HIZ mode failed\n");

			info->need_disable_Q1 = false;
		}

		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask,
					 info->charger_pd_mask);
		if (ret)
			dev_err(info->dev, "disable bq2560x charge failed\n");

		if (info->is_wireless_charge) {
			ret = bq2560x_update_bits(info, BQ2560X_REG_1,
						BQ2560X_REG_CHG_MASK,
						0x0);
			if (ret)
				dev_err(info->dev, "disable bq2560x charge en failed\n");
		}
	} else if (info->role == BQ2560X_ROLE_SLAVE) {
//		ret = bq2560x_update_bits(info, BQ2560X_REG_0,
//					  BQ2560X_REG_EN_HIZ_MASK,
//					  0x01 << BQ2560X_REG_EN_HIZ_SHIFT);
//		if (ret)
//			dev_err(info->dev, "enable HIZ mode failed\n");

		gpiod_set_value_cansleep(info->gpiod, 1);
	}

	if (info->disable_power_path) {
//		ret = bq2560x_update_bits(info, BQ2560X_REG_0,
//					  BQ2560X_REG_EN_HIZ_MASK,
//					  0x01 << BQ2560X_REG_EN_HIZ_SHIFT);
//		if (ret)
//			dev_err(info->dev, "Failed to disable power path\n");
	}

//	if (ret)
//		dev_err(info->dev, "Failed to disable bq2560x watchdog\n");
}

static int bq2560x_charger_set_current(struct bq2560x_charger_info *info,
				       u32 cur)
{
	u8 reg_val;

	dev_err(info->dev, "%s;%d;\n",__func__,cur/1000);

	cur = cur / 1000;

	if(cur >3780)
		cur =3780;
	reg_val = cur / BQ2560X_REG_ICHG_LSB;
	reg_val &= BQ2560X_REG_ICHG_MASK;

	return bq2560x_update_bits(info, BQ2560X_REG_2,
				   BQ2560X_REG_ICHG_MASK,
				   reg_val);
}

static int bq2560x_charger_get_current(struct bq2560x_charger_info *info,
				       u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = bq2560x_read(info, BQ2560X_REG_2, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= BQ2560X_REG_ICHG_MASK;
	*cur = reg_val * BQ2560X_REG_ICHG_LSB * 1000;

	return 0;
}

static int
bq2560x_charger_set_limit_current(struct bq2560x_charger_info *info,
				  u32 limit_cur)
{
	u8 reg_val;
	int ret;

	dev_err(info->dev, "%s;%d;\n",__func__,limit_cur/1000);

	if (limit_cur >= BQ2560X_LIMIT_CURRENT_MAX)
		limit_cur = BQ2560X_LIMIT_CURRENT_MAX;

	info->last_limit_cur = limit_cur;
	limit_cur -= BQ2560X_LIMIT_CURRENT_OFFSET;
	limit_cur = limit_cur / 1000;
	reg_val = limit_cur / BQ2560X_REG_IINLIM_BASE;

	ret = bq2560x_update_bits(info, BQ2560X_REG_0,
				  BQ2560X_REG_LIMIT_CURRENT_MASK,
				  reg_val);
	if (ret)
		dev_err(info->dev, "set bq2560x limit cur failed\n");

	info->actual_limit_cur = reg_val * BQ2560X_REG_IINLIM_BASE * 1000;
	info->actual_limit_cur += BQ2560X_LIMIT_CURRENT_OFFSET;

	return ret;
}

static u32
bq2560x_charger_get_limit_current(struct bq2560x_charger_info *info,
				  u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = bq2560x_read(info, BQ2560X_REG_0, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= BQ2560X_REG_LIMIT_CURRENT_MASK;
	*limit_cur = reg_val * BQ2560X_REG_IINLIM_BASE * 1000;
	*limit_cur += BQ2560X_LIMIT_CURRENT_OFFSET;
	if (*limit_cur >= BQ2560X_LIMIT_CURRENT_MAX)
		*limit_cur = BQ2560X_LIMIT_CURRENT_MAX;

	return 0;
}

static int bq2560x_charger_get_health(struct bq2560x_charger_info *info,
				      u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int bq2560x_charger_get_online(struct bq2560x_charger_info *info,
				      u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}

static int bq2560x_charger_feed_watchdog(struct bq2560x_charger_info *info,
					 u32 val)
{
	int ret;
	u32 limit_cur = 0;

	ret = bq2560x_update_bits(info, BQ2560X_REG_1,
				  BQ2560X_REG_RESET_MASK,
				  BQ2560X_REG_RESET_MASK);
	if (ret) {
		dev_err(info->dev, "reset bq2560x failed\n");
		return ret;
	}

	bq2560x_dump_regs(info);

	ret = bq2560x_charger_get_limit_current(info, &limit_cur);
	if (ret) {
		dev_err(info->dev, "get limit cur failed\n");
		return ret;
	}

	if (info->actual_limit_cur == limit_cur)
		return 0;

	ret = bq2560x_charger_set_limit_current(info, info->actual_limit_cur);
	if (ret) {
		dev_err(info->dev, "set limit cur failed\n");
		return ret;
	}


	return 0;
}
#if 0
static irqreturn_t bq2560x_int_handler(int irq, void *dev_id)
{
	struct bq2560x_charger_info *info = dev_id;

	dev_info(info->dev, "interrupt occurs\n");
//	bq2560x_dump_regs(info);

	return IRQ_HANDLED;
}
#endif
static int bq2560x_charger_set_fchg_current(struct bq2560x_charger_info *info,
					    u32 val)
{
	int ret, limit_cur, cur;

	if (val == CM_FAST_CHARGE_ENABLE_CMD) {
		limit_cur = info->cur.fchg_limit;
		cur = info->cur.fchg_cur;
	} else if (val == CM_FAST_CHARGE_DISABLE_CMD) {
		limit_cur = info->cur.dcp_limit;
		cur = info->cur.dcp_cur;
	} else {
		return 0;
	}

	ret = bq2560x_charger_set_limit_current(info, limit_cur);
	if (ret) {
		dev_err(info->dev, "failed to set fchg limit current\n");
		return ret;
	}

	ret = bq2560x_charger_set_current(info, cur);
	if (ret) {
		dev_err(info->dev, "failed to set fchg current\n");
		return ret;
	}

	return 0;
}

static bool bq2560x_charge_done(struct bq2560x_charger_info *info)
{
	if (info->charging)
	{
		unsigned char val = 0;

		bq2560x_read(info, 0x08, &val);
		
		val = ( val >> 3 ) & 0x03;

		if(val == 0x3)
			return true;
		else
			return false;
	}	
	else
		return false;
}

static int bq2560x_charger_get_status(struct bq2560x_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static void bq2560x_check_wireless_charge(struct bq2560x_charger_info *info, bool enable)
{
	int ret;

	if (!enable)
		cancel_delayed_work_sync(&info->cur_work);

	if (info->is_wireless_charge && enable) {
		cancel_delayed_work_sync(&info->cur_work);
		ret = bq2560x_charger_set_current(info, info->current_charge_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s:set charge current failed\n", __func__);

		ret = bq2560x_charger_set_current(info, info->current_input_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s:set charge current failed\n", __func__);

		pm_wakeup_event(info->dev, BQ2560X_WAKE_UP_MS);
		schedule_delayed_work(&info->cur_work, BQ2560X_CURRENT_WORK_MS);
	} else if (info->is_wireless_charge && !enable) {
		info->new_charge_limit_cur = info->current_charge_limit_cur;
		info->current_charge_limit_cur = BQ2560X_REG_ICHG_LSB * 1000;
		info->new_input_limit_cur = info->current_input_limit_cur;
		info->current_input_limit_cur = BQ2560X_REG_IINDPM_LSB * 1000;
	} else if (!info->is_wireless_charge && !enable) {
		info->new_charge_limit_cur = BQ2560X_REG_ICHG_LSB * 1000;
		info->current_charge_limit_cur = BQ2560X_REG_ICHG_LSB * 1000;
		info->new_input_limit_cur = BQ2560X_REG_IINDPM_LSB * 1000;
		info->current_input_limit_cur = BQ2560X_REG_IINDPM_LSB * 1000;
	}
}

static int bq2560x_charger_set_status(struct bq2560x_charger_info *info,
				      int val)
{
	int ret = 0;
	u32 input_vol;

	if (val == CM_FAST_CHARGE_ENABLE_CMD) {
		ret = bq2560x_charger_set_fchg_current(info, val);
		if (ret) {
			dev_err(info->dev, "failed to set 9V fast charge current\n");
			return ret;
		}
		ret = bq2560x_charger_set_ovp(info, BQ2560X_FCHG_OVP_9V);
		if (ret) {
			dev_err(info->dev, "failed to set fast charge 9V ovp\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_DISABLE_CMD) {
		ret = bq2560x_charger_set_fchg_current(info, val);
		if (ret) {
			dev_err(info->dev, "failed to set 5V normal charge current\n");
			return ret;
		}
		ret = bq2560x_charger_set_ovp(info, BQ2560X_FCHG_OVP_6V);
		if (ret) {
			dev_err(info->dev, "failed to set fast charge 5V ovp\n");
			return ret;
		}
		if (info->role == BQ2560X_ROLE_MASTER_DEFAULT) {
			ret = bq2560x_charger_get_charge_voltage(info, &input_vol);
			if (ret) {
				dev_err(info->dev, "failed to get 9V charge voltage\n");
				return ret;
			}
			if (input_vol > BQ2560X_FAST_CHARGER_VOLTAGE_MAX)
				info->need_disable_Q1 = true;
		}
	} else if ((val == false) &&
		   (info->role == BQ2560X_ROLE_MASTER_DEFAULT)) {
		ret = bq2560x_charger_get_charge_voltage(info, &input_vol);
		if (ret) {
			dev_err(info->dev, "failed to get 5V charge voltage\n");
			return ret;
		}
		if (input_vol > BQ2560X_NORMAL_CHARGER_VOLTAGE_MAX)
			info->need_disable_Q1 = true;
	}

	if (val > CM_FAST_CHARGE_NORMAL_CMD)
		return 0;

	if (!val && info->charging) {
		bq2560x_check_wireless_charge(info, false);
		bq2560x_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		bq2560x_check_wireless_charge(info, true);
		ret = bq2560x_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static bool bq2560x_charger_get_power_path_status(struct bq2560x_charger_info *info)
{
	u8 value;
	int ret;
	bool power_path_enabled = true;

	ret = bq2560x_read(info, BQ2560X_REG_0, &value);
	if (ret < 0) {
		dev_err(info->dev, "Fail to get power path status, ret = %d\n", ret);
		return power_path_enabled;
	}

	if (value & BQ2560X_REG_EN_HIZ_MASK)
		power_path_enabled = false;

	return power_path_enabled;
}

static int bq2560x_charger_set_power_path_status(struct bq2560x_charger_info *info, bool enable)
{
	int ret = 0;
	u8 value = 0x1;

	if (enable)
		value = 0;

	ret = bq2560x_update_bits(info, BQ2560X_REG_0,
				  BQ2560X_REG_EN_HIZ_MASK,
				  value << BQ2560X_REG_EN_HIZ_SHIFT);
	if (ret)
		dev_err(info->dev, "%s HIZ mode failed, ret = %d\n",
			enable ? "Enable" : "Disable", ret);

	return ret;
}
static int bq2560x_fgu_get_vbus(struct bq2560x_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(BQ2560X_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return false;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	power_supply_put(psy);

	return val.intval;
}
#if 1
static void bq2560x_set_pe(struct bq2560x_charger_info *info, u32 vbus)
{
		int vol;
		int try_count=0;
		u8 reg_val;
		int delta;
		int last_limit_current;

		dev_info(info->dev, "%s;cur=%d;vbus=%d;%d;\n",__func__,info->last_limit_cur/1000,vbus/1000,info->current_vbus/1000);
		
		if(info->enable_qc)
			goto pe_exit ;

		if( vbus == info->current_vbus )
			goto pe_exit ;

		vol=bq2560x_fgu_get_vbus(info);
		if((abs(vol -vbus)) < V_500MV)
			goto pe_exit ;

		bq2560x_charger_set_ovp(info, BQ2560X_FCHG_OVP_9V);
		last_limit_current = info->last_limit_cur;
		bq2560x_charger_set_limit_current(info, I_500MA);
		msleep(500);
		if(vbus > info->current_vbus)
			bq2560x_write(info, 0x0d, 0xc0);  //enable and up
		else	
			bq2560x_write(info, 0x0d, 0xa0);  //enable and down

		do{
			msleep(100);
			
			vol=bq2560x_fgu_get_vbus(info);
			 bq2560x_read(info, 0x0d, &reg_val);
			dev_info(info->dev, "%s;%d;0x%x;%d;\n",__func__,vol,reg_val,try_count);

			if(vol < VBUS_1V)
				goto pe_exit ;       //charger plug out
			 
			if(vol<vbus && (reg_val & 0x40) ==0)
			{
				
				if((abs(vol -vbus)) < V_500MV)
					break;
				bq2560x_write(info, 0x0d,0xc0);   //up
				dev_info(info->dev, "%s;up; count %d;\n",__func__,try_count);
				
			}	
			else if(vol>vbus && (reg_val & 0x20) ==0)	
			{
				if((abs(vol -vbus)) < V_500MV)
					break;
				bq2560x_write(info, 0x0d,0xa0); //down
				dev_info(info->dev, "%s;down; count %d;\n",__func__,try_count);
			}

			try_count++;


		}while((abs(vol-vbus))>V_500MV && try_count <80);

		delta =abs(vol-vbus);

		if(delta < V_500MV  && vbus > VBUS_5V)
		{
			info->enable_pe = true;
			info->current_vbus = vbus;

		}
		else
		{
			info->enable_pe = false;
			info->current_vbus = VBUS_5V;

		}


		dev_info(info->dev, "%s;%d;count %d;%d;%d;\n",__func__,info->enable_pe,try_count,last_limit_current/1000,info->last_limit_cur/1000);

pe_exit:
		if(info->last_limit_cur == I_500MA)
			bq2560x_charger_set_limit_current(info, last_limit_current);

		bq2560x_write(info, 0x0d, 0x00);   //clear

		info->usb_phy->sprd_hsphy_set_dpdm(info->usb_phy,1);

		return ;

}
#endif
#if 1
static void bq2560x_check_qc(struct bq2560x_charger_info *info, u32 vbus)
{

	int vol;
	int try_count=0;
	u8 reg_val;
	int last_limit_current;
	dev_info(info->dev, "%s;cur=%d;vbus=%d;%d;\n",__func__,info->last_limit_cur/1000,vbus/1000,info->current_vbus/1000);

	if(info->enable_pe)
		goto qc_exit ;
	
	if( vbus == info->current_vbus )
		return;      

	vol=bq2560x_fgu_get_vbus(info);
	if((abs(vol -vbus)) < V_500MV)
		return;      

	gpio_direction_output(info->dpdm_gpio, 1);

	info->usb_phy->sprd_hsphy_set_dpdm(info->usb_phy,0);
	msleep(2500);

	last_limit_current = info->last_limit_cur;
	bq2560x_charger_set_ovp(info, BQ2560X_FCHG_OVP_9V);
	bq2560x_charger_set_limit_current(info, 100000);
//	bq2560x_write(info, 0x0d, 0x14);      //d+ 0.6  d- 0.6
	bq2560x_write(info, 0x0d, 0x10);      //d+ 0.6
	msleep(2500);
	if(vbus == VBUS_9V)
		bq2560x_write(info, 0x0d, 0x1c);      //d+ 3.3 , d- 0.6
	else if (vbus == VBUS_5V)	
		bq2560x_write(info, 0x0d, 0x10);      //d+ 0.6

//		bq2560x_write(info, 0x0d, 0x06);      //  d- 3.3       programm
//		bq2560x_write(info, 0x0d, 0x04);      //  d- 0.6      12v

	do{

		msleep(2500);

		vol = bq2560x_fgu_get_vbus(info);

		if(vol < VBUS_1V)
			goto qc_exit  ;       //charger plug out

		if((abs(vol -vbus)) < V_500MV)
			break;



	       bq2560x_read(info, 0x0d, &reg_val);

		try_count++;

		dev_info(info->dev, "%s;QC checke %d;0x%x;%d;\n",__func__,vol,reg_val,try_count);

	}while(try_count<3);


	if(info->last_limit_cur == 100000)
		bq2560x_charger_set_limit_current(info, last_limit_current);

	dev_info(info->dev, "%s;QC checke?? %d;%d;\n",__func__,vol,try_count);


	if((abs(vol-vbus)) < V_500MV  && vbus > VBUS_5V)
	{
		info->enable_qc = true;
		info->current_vbus = vbus;

	}
	else
	{
		info->enable_qc = false;
		info->current_vbus = VBUS_5V;
		goto qc_exit ;
	}



	return ;
qc_exit:

		if(info->last_limit_cur == 100000)
			bq2560x_charger_set_limit_current(info, last_limit_current);

		bq2560x_write(info, 0x0d, 0x00);   //clear

		gpio_direction_output(info->dpdm_gpio, 0); //analogy swtich

		info->usb_phy->sprd_hsphy_set_dpdm(info->usb_phy,1);

		return ;

}
#endif
static void bq2560x_charger_hand_work(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct bq2560x_charger_info *info =
		container_of(dwork, struct bq2560x_charger_info, hand_work);

	
	bq2560x_set_pe(info, info->set_vbus);
	if(0)
	bq2560x_check_qc(info,info->set_vbus);


}

static void bq2560x_charger_work(struct work_struct *data)
{
	struct bq2560x_charger_info *info =
		container_of(data, struct bq2560x_charger_info, work);
	bool present = bq2560x_charger_is_bat_present(info);

	dev_info(info->dev, "battery present = %d, charger type = %d\n",
		 present, info->usb_phy->chg_type);


	if(info->limit > 0 && !info->charging )
	{
		if(info->usb_phy->chg_type == DCP_TYPE)
		{
			info->set_vbus = VBUS_9V;
			
			info->usb_phy->sprd_hsphy_set_dpdm(info->usb_phy,0);

			schedule_delayed_work(&info->hand_work,  msecs_to_jiffies(10000));
		}
	}
	else if(!info->limit && info->charging)
	{
			cancel_delayed_work_sync(&info->hand_work);

			info->enable_pe = false;
			info->current_vbus = VBUS_5V;

			gpio_direction_output(info->dpdm_gpio, 0); //analogy swtich
			info->usb_phy->sprd_hsphy_set_dpdm(info->usb_phy,1);

	}
	cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}

static void bq2560x_current_work(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct bq2560x_charger_info *info =
		container_of(dwork, struct bq2560x_charger_info, cur_work);
	int ret = 0;
	bool need_return = false;

	if (info->current_charge_limit_cur > info->new_charge_limit_cur) {
		ret = bq2560x_charger_set_current(info, info->new_charge_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s: set charge limit cur failed\n", __func__);
		return;
	}

	if (info->current_input_limit_cur > info->new_input_limit_cur) {
		ret = bq2560x_charger_set_limit_current(info, info->new_input_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s: set input limit cur failed\n", __func__);
		return;
	}

	if (info->current_charge_limit_cur + BQ2560X_REG_ICHG_LSB * 1000 <=
	    info->new_charge_limit_cur)
		info->current_charge_limit_cur += BQ2560X_REG_ICHG_LSB * 1000;
	else
		need_return = true;

	if (info->current_input_limit_cur + BQ2560X_REG_IINDPM_LSB * 1000 <=
	    info->new_input_limit_cur)
		info->current_input_limit_cur += BQ2560X_REG_IINDPM_LSB * 1000;
	else if (need_return)
		return;

	ret = bq2560x_charger_set_current(info, info->current_charge_limit_cur);
	if (ret < 0) {
		dev_err(info->dev, "set charge limit current failed\n");
		return;
	}

	ret = bq2560x_charger_set_limit_current(info, info->current_input_limit_cur);
	if (ret < 0) {
		dev_err(info->dev, "set input limit current failed\n");
		return;
	}

	dev_info(info->dev, "set charge_limit_cur %duA, input_limit_curr %duA\n",
		info->current_charge_limit_cur, info->current_input_limit_cur);

	schedule_delayed_work(&info->cur_work, BQ2560X_CURRENT_WORK_MS);
}


static int bq2560x_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct bq2560x_charger_info *info =
		container_of(nb, struct bq2560x_charger_info, usb_notify);

	info->limit = limit;

	/*
	 * only master should do work when vbus change.
	 * let info->limit = limit, slave will online, too.
	 */
	if (info->role == BQ2560X_ROLE_SLAVE)
		return NOTIFY_OK;

	pm_wakeup_event(info->dev, BQ2560X_WAKE_UP_MS);

	schedule_work(&info->work);
	return NOTIFY_OK;
}

static int bq2560x_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{
	struct bq2560x_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, online, health, vol,enabled = 0;
	enum usb_charger_type type;
	int ret = 0;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == CM_POWER_PATH_ENABLE_CMD ||
		    val->intval == CM_POWER_PATH_DISABLE_CMD) {
			val->intval = bq2560x_charger_get_power_path_status(info);
			break;
		}

		if (info->limit || info->is_wireless_charge)
			val->intval = bq2560x_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = bq2560x_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = bq2560x_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = bq2560x_charger_get_online(info, &online);
		if (ret)
			goto out;

		val->intval = online;

		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = bq2560x_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;

	case POWER_SUPPLY_PROP_USB_TYPE:
		type = info->usb_phy->chg_type;

		switch (type) {
		case SDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_SDP;
			break;

		case DCP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
			break;

		case CDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_CDP;
			break;

		default:
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}
		
		if(info->enable_pe ||info->enable_qc)
			val->intval = POWER_SUPPLY_USB_TYPE_SFCP_1P0;
		
		break;

	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		if (info->role == BQ2560X_ROLE_MASTER_DEFAULT) {
			ret = regmap_read(info->pmic, info->charger_pd, &enabled);
			if (ret) {
				dev_err(info->dev, "get bq2560x charge status failed\n");
				goto out;
			}
		} else if (info->role == BQ2560X_ROLE_SLAVE) {
			enabled = gpiod_get_value_cansleep(info->gpiod);
		}

		val->intval = !enabled;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
			val->intval =bq2560x_charge_done(info);
		break;
		
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = bq2560x_charger_get_termina_vol(info, &vol);
		val->intval = vol *1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if(info->enable_pe ||info->enable_qc)
			val->intval =VBUS_9V;
		else
			val->intval =VBUS_5V;
		break;

	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int bq2560x_charger_usb_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct bq2560x_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (info->is_wireless_charge) {
			cancel_delayed_work_sync(&info->cur_work);
			info->new_charge_limit_cur = val->intval;
			pm_wakeup_event(info->dev, BQ2560X_WAKE_UP_MS);
			schedule_delayed_work(&info->cur_work, BQ2560X_CURRENT_WORK_MS * 2);
			break;
		}

		ret = bq2560x_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (info->is_wireless_charge) {
			cancel_delayed_work_sync(&info->cur_work);
			info->new_input_limit_cur = val->intval;
			pm_wakeup_event(info->dev, BQ2560X_WAKE_UP_MS);
			schedule_delayed_work(&info->cur_work, BQ2560X_CURRENT_WORK_MS * 2);
			break;
		}

		ret = bq2560x_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == CM_POWER_PATH_ENABLE_CMD) {
			ret = bq2560x_charger_set_power_path_status(info, true);
			break;
		} else if (val->intval == CM_POWER_PATH_DISABLE_CMD) {
			ret = bq2560x_charger_set_power_path_status(info, false);
			break;
		}

		ret = bq2560x_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;

	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		ret = bq2560x_charger_feed_watchdog(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "feed charger watchdog failed\n");
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = bq2560x_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		bq2560x_enable_powerpath(info, val->intval);
		break;

	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		if (val->intval == true) {
			bq2560x_check_wireless_charge(info, true);
			ret = bq2560x_charger_start_charge(info);
			if (ret)
				dev_err(info->dev, "start charge failed\n");
		} else if (val->intval == false) {
			bq2560x_check_wireless_charge(info, false);
			bq2560x_charger_stop_charge(info);
		}
		break;
	case POWER_SUPPLY_PROP_WIRELESS_TYPE:
		if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP) {
			info->is_wireless_charge = true;
			ret = bq2560x_charger_set_ovp(info, BQ2560X_FCHG_OVP_6V);
		} else if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP) {
			info->is_wireless_charge = true;
			ret = bq2560x_charger_set_ovp(info, BQ2560X_FCHG_OVP_14V);
		} else {
			info->is_wireless_charge = false;
			ret = bq2560x_charger_set_ovp(info, BQ2560X_FCHG_OVP_6V);
		}
		if (ret)
			dev_err(info->dev, "failed to set fast charge ovp\n");

		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (info->enable_pe || info->enable_qc) 
		{
			if( info->current_vbus == val->intval )
				break;
			info->set_vbus = val->intval;
			
			info->usb_phy->sprd_hsphy_set_dpdm(info->usb_phy,0);

			schedule_delayed_work(&info->hand_work,  250);
		}	
		break;		
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int bq2560x_charger_property_is_writeable(struct power_supply *psy,
						 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
	case POWER_SUPPLY_PROP_WIRELESS_TYPE:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_usb_type bq2560x_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID,
	POWER_SUPPLY_USB_TYPE_SFCP_1P0	
};

static enum power_supply_property bq2560x_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CHARGE_ENABLED,
	POWER_SUPPLY_PROP_WIRELESS_TYPE,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static const struct power_supply_desc bq2560x_charger_desc = {
	.name			= "charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= bq2560x_usb_props,
	.num_properties		= ARRAY_SIZE(bq2560x_usb_props),
	.get_property		= bq2560x_charger_usb_get_property,
	.set_property		= bq2560x_charger_usb_set_property,
	.property_is_writeable	= bq2560x_charger_property_is_writeable,
	.usb_types		= bq2560x_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(bq2560x_charger_usb_types),
};

static const struct power_supply_desc bq2560x_slave_charger_desc = {
	.name			= "bq2560x_slave_charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= bq2560x_usb_props,
	.num_properties		= ARRAY_SIZE(bq2560x_usb_props),
	.get_property		= bq2560x_charger_usb_get_property,
	.set_property		= bq2560x_charger_usb_set_property,
	.property_is_writeable	= bq2560x_charger_property_is_writeable,
	.usb_types		= bq2560x_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(bq2560x_charger_usb_types),
};

static ssize_t bq2560x_register_value_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct bq2560x_charger_sysfs *bq2560x_sysfs =
		container_of(attr, struct bq2560x_charger_sysfs,
			     attr_bq2560x_reg_val);
	struct  bq2560x_charger_info *info =  bq2560x_sysfs->info;
	u8 val;
	int ret;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s  bq2560x_sysfs->info is null\n", __func__);

	ret = bq2560x_read(info, reg_tab[info->reg_id].addr, &val);
	if (ret) {
		dev_err(info->dev, "fail to get  BQ2560X_REG_0x%.2x value, ret = %d\n",
			reg_tab[info->reg_id].addr, ret);
		return snprintf(buf, PAGE_SIZE, "fail to get  BQ2560X_REG_0x%.2x value\n",
			       reg_tab[info->reg_id].addr);
	}

	return snprintf(buf, PAGE_SIZE, "BQ2560X_REG_0x%.2x = 0x%.2x\n",
			reg_tab[info->reg_id].addr, val);
}

static ssize_t bq2560x_register_value_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct bq2560x_charger_sysfs *bq2560x_sysfs =
		container_of(attr, struct bq2560x_charger_sysfs,
			     attr_bq2560x_reg_val);
	struct bq2560x_charger_info *info = bq2560x_sysfs->info;
	u8 val;
	int ret;

	if (!info) {
		dev_err(dev, "%s bq2560x_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtou8(buf, 16, &val);
	if (ret) {
		dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	ret = bq2560x_write(info, reg_tab[info->reg_id].addr, val);
	if (ret) {
		dev_err(info->dev, "fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
				val, reg_tab[info->reg_id].addr, ret);
		return count;
	}

	dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n", val, reg_tab[info->reg_id].addr);
	return count;
}

static ssize_t bq2560x_register_id_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct bq2560x_charger_sysfs *bq2560x_sysfs =
		container_of(attr, struct bq2560x_charger_sysfs,
			     attr_bq2560x_sel_reg_id);
	struct bq2560x_charger_info *info = bq2560x_sysfs->info;
	int ret, id;

	if (!info) {
		dev_err(dev, "%s bq2560x_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtoint(buf, 10, &id);
	if (ret) {
		dev_err(info->dev, "%s store register id fail\n", bq2560x_sysfs->name);
		return count;
	}

	if (id < 0 || id >= BQ2560X_REG_NUM) {
		dev_err(info->dev, "%s store register id fail, id = %d is out of range\n",
			bq2560x_sysfs->name, id);
		return count;
	}

	info->reg_id = id;

	dev_info(info->dev, "%s store register id = %d success\n", bq2560x_sysfs->name, id);
	return count;
}

static ssize_t bq2560x_register_id_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct bq2560x_charger_sysfs *bq2560x_sysfs =
		container_of(attr, struct bq2560x_charger_sysfs,
			     attr_bq2560x_sel_reg_id);
	struct bq2560x_charger_info *info = bq2560x_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s bq2560x_sysfs->info is null\n", __func__);

	return snprintf(buf, PAGE_SIZE, "Curent register id = %d\n", info->reg_id);
}

static ssize_t bq2560x_register_table_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct bq2560x_charger_sysfs *bq2560x_sysfs =
		container_of(attr, struct bq2560x_charger_sysfs,
			     attr_bq2560x_lookup_reg);
	struct bq2560x_charger_info *info = bq2560x_sysfs->info;
	int i, len, idx = 0;
	char reg_tab_buf[2048];

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s bq2560x_sysfs->info is null\n", __func__);

	memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
	len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
		       "Format: [id] [addr] [desc]\n");
	idx += len;

	for (i = 0; i < BQ2560X_REG_NUM; i++) {
		len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
			       "[%d] [REG_0x%.2x] [%s]; \n",
			       reg_tab[i].id, reg_tab[i].addr, reg_tab[i].name);
		idx += len;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", reg_tab_buf);
}

static ssize_t bq2560x_dump_register_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct bq2560x_charger_sysfs *bq2560x_sysfs =
		container_of(attr, struct bq2560x_charger_sysfs,
			     attr_bq2560x_dump_reg);
	struct bq2560x_charger_info *info = bq2560x_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s bq2560x_sysfs->info is null\n", __func__);

	bq2560x_dump_regs(info);

	return snprintf(buf, PAGE_SIZE, "%s\n", bq2560x_sysfs->name);
}

static int bq2560x_register_sysfs(struct bq2560x_charger_info *info)
{
	struct bq2560x_charger_sysfs *bq2560x_sysfs;
	int ret;

	bq2560x_sysfs = devm_kzalloc(info->dev, sizeof(*bq2560x_sysfs), GFP_KERNEL);
	if (!bq2560x_sysfs)
		return -ENOMEM;

	info->sysfs = bq2560x_sysfs;
	bq2560x_sysfs->name = "bq2560x_sysfs";
	bq2560x_sysfs->info = info;
	bq2560x_sysfs->attrs[0] = &bq2560x_sysfs->attr_bq2560x_dump_reg.attr;
	bq2560x_sysfs->attrs[1] = &bq2560x_sysfs->attr_bq2560x_lookup_reg.attr;
	bq2560x_sysfs->attrs[2] = &bq2560x_sysfs->attr_bq2560x_sel_reg_id.attr;
	bq2560x_sysfs->attrs[3] = &bq2560x_sysfs->attr_bq2560x_reg_val.attr;
	bq2560x_sysfs->attrs[4] = NULL;
	bq2560x_sysfs->attr_g.name = "debug";
	bq2560x_sysfs->attr_g.attrs = bq2560x_sysfs->attrs;

	sysfs_attr_init(&bq2560x_sysfs->attr_bq2560x_dump_reg.attr);
	bq2560x_sysfs->attr_bq2560x_dump_reg.attr.name = "bq2560x_dump_reg";
	bq2560x_sysfs->attr_bq2560x_dump_reg.attr.mode = 0444;
	bq2560x_sysfs->attr_bq2560x_dump_reg.show = bq2560x_dump_register_show;

	sysfs_attr_init(&bq2560x_sysfs->attr_bq2560x_lookup_reg.attr);
	bq2560x_sysfs->attr_bq2560x_lookup_reg.attr.name = "bq2560x_lookup_reg";
	bq2560x_sysfs->attr_bq2560x_lookup_reg.attr.mode = 0444;
	bq2560x_sysfs->attr_bq2560x_lookup_reg.show = bq2560x_register_table_show;

	sysfs_attr_init(&bq2560x_sysfs->attr_bq2560x_sel_reg_id.attr);
	bq2560x_sysfs->attr_bq2560x_sel_reg_id.attr.name = "bq2560x_sel_reg_id";
	bq2560x_sysfs->attr_bq2560x_sel_reg_id.attr.mode = 0644;
	bq2560x_sysfs->attr_bq2560x_sel_reg_id.show = bq2560x_register_id_show;
	bq2560x_sysfs->attr_bq2560x_sel_reg_id.store = bq2560x_register_id_store;

	sysfs_attr_init(&bq2560x_sysfs->attr_bq2560x_reg_val.attr);
	bq2560x_sysfs->attr_bq2560x_reg_val.attr.name = "bq2560x_reg_val";
	bq2560x_sysfs->attr_bq2560x_reg_val.attr.mode = 0644;
	bq2560x_sysfs->attr_bq2560x_reg_val.show = bq2560x_register_value_show;
	bq2560x_sysfs->attr_bq2560x_reg_val.store = bq2560x_register_value_store;

	ret = sysfs_create_group(&info->psy_usb->dev.kobj, &bq2560x_sysfs->attr_g);
	if (ret < 0)
		dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static void bq2560x_charger_detect_status(struct bq2560x_charger_info *info)
{
	unsigned int min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;

	usb_phy_get_charger_current(info->usb_phy, &min, &max);
	info->limit = min;

	/*
	 * slave no need to start charge when vbus change.
	 * due to charging in shut down will check each psy
	 * whether online or not, so let info->limit = min.
	 */
	if (info->role == BQ2560X_ROLE_SLAVE)
		return;
	schedule_work(&info->work);
}

static void
bq2560x_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct bq2560x_charger_info *info = container_of(dwork,
							 struct bq2560x_charger_info,
							 wdt_work);
	int ret;

	ret = bq2560x_update_bits(info, BQ2560X_REG_1,
				  BQ2560X_REG_WATCHDOG_MASK,
				  BQ2560X_REG_WATCHDOG_MASK);
	if (ret) {
		dev_err(info->dev, "reset bq2560x failed\n");
		return;
	}

	bq2560x_dump_regs(info);

	schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#ifdef CONFIG_REGULATOR
static bool bq2560x_charger_check_otg_valid(struct bq2560x_charger_info *info)
{
	return extcon_get_state(info->edev, EXTCON_USB);
}

static bool bq2560x_charger_check_otg_fault(struct bq2560x_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = true;

	ret = bq2560x_read(info, BQ2560X_REG_8, &value);
	if (ret) {
		dev_err(info->dev, "get bq2560x charger otg fault status failed\n");
		return status;
	}

	if ((value & BQ2560X_REG_BOOST_FAULT_MASK ) == 0xe0)
		status = false;
	else
		dev_err(info->dev, "otg fault occurs, REG_8 = 0x%x\n", value);

	return status;
}

static void bq2560x_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct bq2560x_charger_info *info = container_of(dwork,
			struct bq2560x_charger_info, otg_work);
	bool otg_valid = bq2560x_charger_check_otg_valid(info);
	bool otg_fault;
	int ret, retry = 0;

	if (otg_valid)
		goto out;

	do {
		otg_fault = bq2560x_charger_check_otg_fault(info);
		if (otg_fault) {
			ret = bq2560x_update_bits(info, BQ2560X_REG_1,
						  BQ2560X_REG_OTG_MASK,
						  BQ2560X_REG_OTG_MASK);
			if (ret)
				dev_err(info->dev, "restart bq2560x charger otg failed\n");
		}

		otg_valid = bq2560x_charger_check_otg_valid(info);
	} while (!otg_valid && retry++ < BQ2560X_OTG_RETRY_TIMES);

	if (retry >= BQ2560X_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int bq2560x_charger_enable_otg(struct regulator_dev *dev)
{
	struct bq2560x_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	dev_err(info->dev, "%s\n",__func__);
	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
	if (ret) {
		dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
		return ret;
	}

	ret = bq2560x_update_bits(info, BQ2560X_REG_1,
				  BQ2560X_REG_OTG_MASK,
				  BQ2560X_REG_OTG_MASK);
	if (ret) {
		dev_err(info->dev, "enable bq2560x otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect,
				   BIT_DP_DM_BC_ENB, 0);
		return ret;
	}

	info->otg_enable = true;
	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(BQ2560X_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(BQ2560X_OTG_VALID_MS));

	bq2560x_dump_regs(info);

	return 0;
}

static int bq2560x_charger_disable_otg(struct regulator_dev *dev)
{
	struct bq2560x_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	dev_err(info->dev, "%s\n",__func__);

	info->otg_enable = false;
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	ret = bq2560x_update_bits(info, BQ2560X_REG_1,
				  BQ2560X_REG_OTG_MASK,
				  0);
	if (ret) {
		dev_err(info->dev, "disable bq2560x otg failed\n");
		return ret;
	}

	/* Enable charger detection function to identify the charger type */
	return regmap_update_bits(info->pmic, info->charger_detect,
				  BIT_DP_DM_BC_ENB, 0);
}

static int bq2560x_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct bq2560x_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	ret = bq2560x_read(info, BQ2560X_REG_1, &val);
	if (ret) {
		dev_err(info->dev, "failed to get bq2560x otg status\n");
		return ret;
	}

	val &= BQ2560X_REG_OTG_MASK;

	return val;
}

static const struct regulator_ops bq2560x_charger_vbus_ops = {
	.enable = bq2560x_charger_enable_otg,
	.disable = bq2560x_charger_disable_otg,
	.is_enabled = bq2560x_charger_vbus_is_enabled,
};

static const struct regulator_desc bq2560x_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &bq2560x_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int
bq2560x_charger_register_vbus_regulator(struct bq2560x_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &bq2560x_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int
bq2560x_charger_register_vbus_regulator(struct bq2560x_charger_info *info)
{
	return 0;
}
#endif

static int bq2560x_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct bq2560x_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	unsigned char val = 0;
	int ret;
	dev_err(dev, "%s;enter;\n",__func__);

//+add by hzb for ontim debug
        if(CHECK_THIS_DEV_DEBUG_AREADY_EXIT()==0)
        {
           return -EIO;
        }
//-add by hzb for ontim debug

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->client = client;
	info->dev = dev;

	bq2560x_read(info,BQ2560X_REG_B, &val);
	dev_err(dev, "%s;%x;\n",__func__,val);
	if( (val & 0x7c) == 0x48)
		strncpy(charge_ic_vendor_name,"SY6974",20);
	else if ( val == 0x11 )
		strncpy(charge_ic_vendor_name,"BQ25601",20);
//	else if ( (val & 0x7c) == 0x14  )
//		strncpy(charge_ic_vendor_name,"SGM41511",20);
	else
	{
		info->client->addr = 0x3b;
		bq2560x_read(info,BQ2560X_REG_B, &val);
		dev_err(dev, "%s;i2c 0x3b  val=%x;\n",__func__,val);
		if ( ((val & 0x7c) == 0x64)  || ((val & 0x7c) == 0x6c) )
			strncpy(charge_ic_vendor_name,"SGM41542",20);
		else
			return -ENODEV;
	}

	dev_err(dev, "%s;%s;\n",__func__,charge_ic_vendor_name);

	alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);

	i2c_set_clientdata(client, info);
	power_path_control(info);
	ret = device_property_read_bool(dev, "role-slave");
	if (ret)
		info->role = BQ2560X_ROLE_SLAVE;
	else
		info->role = BQ2560X_ROLE_MASTER_DEFAULT;

	if (info->role == BQ2560X_ROLE_SLAVE) {
		info->gpiod = devm_gpiod_get(dev, "enable", GPIOD_OUT_HIGH);
		if (IS_ERR(info->gpiod)) {
			dev_err(dev, "failed to get enable gpio\n");
			return PTR_ERR(info->gpiod);
		}
	}

	info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);

	if (IS_ERR(info->usb_phy)) {
		dev_err(dev, "failed to find USB phy\n");
		return PTR_ERR(info->usb_phy);
	}

	info->edev = extcon_get_edev_by_phandle(info->dev, 0);
	if (IS_ERR(info->edev)) {
		dev_err(dev, "failed to find vbus extcon device.\n");
		return PTR_ERR(info->edev);
	}

	ret = bq2560x_charger_is_fgu_present(info);
	if (ret) {
		dev_err(dev, "sc27xx_fgu not ready.\n");
		return -EPROBE_DEFER;
	}

	/*
	 * only master to support otg
	 */
	if (info->role == BQ2560X_ROLE_MASTER_DEFAULT) {
		ret = bq2560x_charger_register_vbus_regulator(info);
		if (ret) {
			dev_err(dev, "failed to register vbus regulator.\n");
			return ret;
		}
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np)
		regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");

	if (regmap_np) {
		if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
			info->charger_pd_mask = BQ2560X_DISABLE_PIN_MASK_2721;
		else
			info->charger_pd_mask = BQ2560X_DISABLE_PIN_MASK;
	} else {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &info->charger_detect);
	if (ret) {
		dev_err(dev, "failed to get charger_detect\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &info->charger_pd);
	if (ret) {
		dev_err(dev, "failed to get charger_pd reg\n");
		return ret;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon device\n");
		return -ENODEV;
	}

	of_node_put(regmap_np);
	info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!info->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		return -ENODEV;
	}
	mutex_init(&info->lock);

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	if (info->role == BQ2560X_ROLE_MASTER_DEFAULT) {
		info->psy_usb = devm_power_supply_register(dev,
							   &bq2560x_charger_desc,
							   &charger_cfg);
	} else if (info->role == BQ2560X_ROLE_SLAVE) {
		info->psy_usb = devm_power_supply_register(dev,
							   &bq2560x_slave_charger_desc,
							   &charger_cfg);
	}

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto err_regmap_exit;
	}

	info->dpdm_gpio = of_get_named_gpio(info->dev->of_node, "dpdm-gpio", 0);
	if (gpio_is_valid(info->dpdm_gpio)) {
		ret = devm_gpio_request_one(info->dev, info->dpdm_gpio,
					    GPIOF_ACTIVE_LOW, "bq2560x_dpdm");
		if (ret)
			dev_err(dev, "dpdm-gpio request failed, ret = %x\n", ret);
	}
	
#if 0
	info->irq_gpio = of_get_named_gpio(info->dev->of_node, "irq-gpio", 0);
	if (gpio_is_valid(info->irq_gpio)) {
		ret = devm_gpio_request_one(info->dev, info->irq_gpio,
					    GPIOF_DIR_IN, "bq2560x_int");
		if (!ret)
			info->client->irq = gpio_to_irq(info->irq_gpio);
		else
			dev_err(dev, "int request failed, ret = %d\n", ret);

		if (info->client->irq < 0) {
			dev_err(dev, "failed to get irq no\n");
			gpio_free(info->irq_gpio);
		} else {
			ret = devm_request_threaded_irq(&info->client->dev, info->client->irq,
							NULL, bq2560x_int_handler,
							IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
							"bq2560x interrupt", info);
			if (ret)
				dev_err(info->dev, "Failed irq = %d ret = %d\n",
					info->client->irq, ret);
			else
				enable_irq_wake(client->irq);
		}
	} else {
		dev_err(dev, "failed to get irq gpio\n");
	}
#endif
	ret = bq2560x_charger_hw_init(info);
	if (ret) {
		dev_err(dev, "failed to bq2560x_charger_hw_init\n");
		goto err_psy_usb;
	}

	bq2560x_charger_stop_charge(info);

	device_init_wakeup(info->dev, true);
	info->usb_notify.notifier_call = bq2560x_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		goto err_psy_usb;
	}

	ret = bq2560x_register_sysfs(info);
	if (ret) {
		dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
		goto error_sysfs;
	}

	info->enable_pe = false;
	info->enable_qc = false;
	info->current_vbus =VBUS_5V;
	
	INIT_WORK(&info->work, bq2560x_charger_work);
	INIT_DELAYED_WORK(&info->cur_work, bq2560x_current_work);

	bq2560x_charger_detect_status(info);
	INIT_DELAYED_WORK(&info->otg_work, bq2560x_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work,
			  bq2560x_charger_feed_watchdog_work);

	INIT_DELAYED_WORK(&info->hand_work, bq2560x_charger_hand_work);
	

	dev_err(dev, "bq2560x_charger_probe ok to register\n");

//+add by hzb for ontim debug
        REGISTER_AND_INIT_ONTIM_DEBUG_FOR_THIS_DEV();
//-add by hzb for ontim debug

	return 0;

error_sysfs:
	sysfs_remove_group(&info->psy_usb->dev.kobj, &info->sysfs->attr_g);
	usb_unregister_notifier(info->usb_phy, &info->usb_notify);
err_psy_usb:
	power_supply_unregister(info->psy_usb);
	if (info->irq_gpio)
		gpio_free(info->irq_gpio);
err_regmap_exit:
	regmap_exit(info->pmic);
	mutex_destroy(&info->lock);
	return ret;
}

static void bq2560x_charger_shutdown(struct i2c_client *client)
{
	struct bq2560x_charger_info *info = i2c_get_clientdata(client);
	int ret = 0;

	cancel_delayed_work_sync(&info->wdt_work);
	if (info->otg_enable) {
		info->otg_enable = false;
		cancel_delayed_work_sync(&info->otg_work);
		ret = bq2560x_update_bits(info, BQ2560X_REG_1,
					  BQ2560X_REG_OTG_MASK,
					  0);
		if (ret)
			dev_err(info->dev, "disable bq2560x otg failed ret = %d\n", ret);

		/* Enable charger detection function to identify the charger type */
		ret = regmap_update_bits(info->pmic, info->charger_detect,
					 BIT_DP_DM_BC_ENB, 0);
		if (ret)
			dev_err(info->dev,
				"enable charger detection function failed ret = %d\n", ret);
	}
}

static int bq2560x_charger_remove(struct i2c_client *client)
{
	struct bq2560x_charger_info *info = i2c_get_clientdata(client);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int bq2560x_charger_suspend(struct device *dev)
{
	struct bq2560x_charger_info *info = dev_get_drvdata(dev);
	ktime_t now, add;
	unsigned int wakeup_ms = BQ2560X_OTG_ALARM_TIMER_MS;
	int ret;

	if (!info->otg_enable)
		return 0;

	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->cur_work);

	/* feed watchdog first before suspend */
	ret = bq2560x_update_bits(info, BQ2560X_REG_1,
				   BQ2560X_REG_RESET_MASK,
				   BQ2560X_REG_RESET_MASK);
	if (ret)
		dev_warn(info->dev, "reset bq2560x failed before suspend\n");

	now = ktime_get_boottime();
	add = ktime_set(wakeup_ms / MSEC_PER_SEC,
			(wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
	alarm_start(&info->otg_timer, ktime_add(now, add));

	return 0;
}

static int bq2560x_charger_resume(struct device *dev)
{
	struct bq2560x_charger_info *info = dev_get_drvdata(dev);
	int ret;

	if (!info->otg_enable)
		return 0;

	alarm_cancel(&info->otg_timer);

	/* feed watchdog first after resume */
	ret = bq2560x_update_bits(info, BQ2560X_REG_1,
				   BQ2560X_REG_RESET_MASK,
				   BQ2560X_REG_RESET_MASK);
	if (ret)
		dev_warn(info->dev, "reset bq2560x failed after resume\n");

	schedule_delayed_work(&info->wdt_work, HZ * 15);
	schedule_delayed_work(&info->cur_work, 0);

	return 0;
}
#endif

static const struct dev_pm_ops bq2560x_charger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(bq2560x_charger_suspend,
				bq2560x_charger_resume)
};

static const struct i2c_device_id bq2560x_i2c_id[] = {
	{"bq2560x_chg", 0},
	{}
};

static const struct of_device_id bq2560x_charger_of_match[] = {
	{ .compatible = "ti,bq2560x_chg", },
	{ }
};

static const struct i2c_device_id bq2560x_slave_i2c_id[] = {
	{"bq2560x_slave_chg", 0},
	{}
};

static const struct of_device_id bq2560x_slave_charger_of_match[] = {
	{ .compatible = "ti,bq2560x_slave_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, bq2560x_charger_of_match);
MODULE_DEVICE_TABLE(of, bq2560x_slave_charger_of_match);

static struct i2c_driver bq2560x_master_charger_driver = {
	.driver = {
		.name = "bq2560x_chg",
		.of_match_table = bq2560x_charger_of_match,
		.pm = &bq2560x_charger_pm_ops,
	},
	.probe = bq2560x_charger_probe,
	.remove = bq2560x_charger_remove,
	.shutdown = bq2560x_charger_shutdown,
	.id_table = bq2560x_i2c_id,
};

static struct i2c_driver bq2560x_slave_charger_driver = {
	.driver = {
		.name = "bq2560_slave_chg",
		.of_match_table = bq2560x_slave_charger_of_match,
		.pm = &bq2560x_charger_pm_ops,
	},
	.probe = bq2560x_charger_probe,
	.remove = bq2560x_charger_remove,
	.id_table = bq2560x_slave_i2c_id,
};

module_i2c_driver(bq2560x_master_charger_driver);
module_i2c_driver(bq2560x_slave_charger_driver);
MODULE_DESCRIPTION("BQ2560X Charger Driver");
MODULE_LICENSE("GPL v2");
