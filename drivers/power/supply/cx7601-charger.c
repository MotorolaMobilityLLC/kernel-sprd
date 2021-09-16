/*
 * Driver for the TI cx7601 charger.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>
#include "cx7601_reg.h"


#define CX7601_BATTERY_NAME			"sc27xx-fgu"
#define BIT_DP_DM_BC_ENB			BIT(0)



#define CX7601_DISABLE_PIN_MASK_2730		BIT(0)
#define CX7601_DISABLE_PIN_MASK_2721		BIT(15)
#define CX7601_DISABLE_PIN_MASK_2720		BIT(0)

#define CX7601_OTG_VALID_MS			500
#define CX7601_FEED_WATCHDOG_VALID_MS		50
#define CX7601_OTG_RETRY_TIMES			10

#define CX7601_ROLE_MASTER_DEFAULT		1
#define CX7601_ROLE_SLAVE			2

enum vboost {
	BOOSTV_4850 = 4850,
	BOOSTV_5000 = 5000,
	BOOSTV_5150 = 5150,
	BOOSTV_5300	= 5300,
};

enum iboost {
	BOOSTI_500 = 500,
	BOOSTI_1200 = 1200,
};

enum vac_ovp {
	VAC_OVP_5500 = 5500,
	VAC_OVP_6200 = 6500,
	VAC_OVP_10500 = 10500,
	VAC_OVP_14300 = 14300,
};

struct cx7601_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct power_supply *psy_usb;
	struct power_supply_charge_current cur;
	struct work_struct work;
	struct mutex lock;
	bool charging;
	u32 limit;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	struct regmap *pmic;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	struct gpio_desc *gpiod;
	struct extcon_dev *edev;
	u32 last_limit_current;
	u32 last_current;
	u32 role;
	bool need_disable_Q1;
	u32 term_voltage;
};

#include <ontim/ontim_dev_dgb.h>
static  char charge_ic_vendor_name[50]="BQ2560x";
DEV_ATTR_DECLARE(charge_ic)
DEV_ATTR_DEFINE("vendor",charge_ic_vendor_name)
DEV_ATTR_DECLARE_END;
ONTIM_DEBUG_DECLARE_AND_INIT(charge_ic,charge_ic,8);

static int
cx7601_charger_set_limit_current(struct cx7601_charger_info *info,
				  u32 limit_cur);

static bool cx7601_charger_is_bat_present(struct cx7601_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(CX7601_BATTERY_NAME);
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

static int cx7601_read(struct cx7601_charger_info *info,  u8 *data, u8 reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int cx7601_write(struct cx7601_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int cx7601_update_bits(struct cx7601_charger_info *info, u8 reg,
			       u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = cx7601_read(info,  &v, reg);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return cx7601_write(info, reg, v);
}

static void cx7601_dump_regs(struct cx7601_charger_info *info)
{

	int addr;
	u8 val[0x0e];
	int ret;

	for (addr = 0x0; addr <= 0x0D; addr++) {
		ret = cx7601_read(info, &val[addr] , addr);
	}
	dev_err(info->dev,"cx7601 [0]=0x%.2x [1]=0x%.2x [2]=0x%.2x [3]=0x%.2x [4]=0x%.2x [5]=0x%.2x [6]=0x%.2x \n",
		                      val[0],val[1],val[2],val[3],val[4],val[5],val[6]);
	dev_err(info->dev,"cx7601 [7]=0x%.2x [8]=0x%.2x [9]=0x%.2x [a]=0x%.2x [b]=0x%.2x [c]=0x%.2x  [d]=0x%.2x  \n",
		                      val[7],val[8],val[9],val[0xa],val[0xb],val[0xc],val[0xd]);

}


static int
cx7601_charger_set_vindpm(struct cx7601_charger_info *info, u32 vol)
{
	u8 val;
	u32 voltage;
	voltage = (vol- REG00_VINDPM_BASE) /
			REG00_VINDPM_LSB;
	val = (u8)(voltage << REG00_VINDPM_SHIFT);
	pr_err(" cx7601_charger_set_vindpm %d;\n",vol);
	return cx7601_update_bits(info, CX7601_REG_00, REG00_VINDPM_MASK,
				val);
}
static int  cx7601_enable_powerpath(struct cx7601_charger_info *info, bool en)
{
	int ret;

	dev_err(info->dev,"%s; %d;\n", __func__,en);

	if(en)
		ret=cx7601_charger_set_vindpm(info, 4520);
	else	
		ret=cx7601_charger_set_vindpm(info, 5080);

	return ret;
}

static int
cx7601_charger_set_ovp(struct cx7601_charger_info *info, u32 vol)
{
	u8 val;

	if (vol == VAC_OVP_14300)
		val = REG07_AVOV_TH_14P3V;
	else if (vol == VAC_OVP_10500)
		val = REG07_AVOV_TH_10P5V;
	else if (vol == VAC_OVP_6200)
		val = REG07_AVOV_TH_6P2V;
	else
		val = REG07_AVOV_TH_5P5V;

	return cx7601_update_bits(info, CX7601_REG_07, REG07_AVOV_TH_MASK,
				val << REG07_AVOV_TH_SHIFT);//done
}

static int cx7601_enable_otg(struct cx7601_charger_info *info)
{
	u8 val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;
        pr_info("cx7601_enable_otg enter\n");
	return cx7601_update_bits(info, CX7601_REG_01,
				REG01_OTG_CONFIG_MASK, val);

}

static int cx7601_disable_otg(struct cx7601_charger_info *info)
{
	u8 val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;
	pr_info("cx7601_disable_otg enter\n");
	return cx7601_update_bits(info, CX7601_REG_01,
				   REG01_OTG_CONFIG_MASK, val);

}

static int cx7601_enable_charger(struct cx7601_charger_info *info)
{
	int ret;
	u8 val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;
	//cx7601_dump_regs(bq);
	ret = cx7601_update_bits(info, CX7601_REG_01, REG01_CHG_CONFIG_MASK, val);

	return ret;
}
#ifdef TEST_CX7601
static int cx7601_disable_charger(struct cx7601_charger_info *info)
{
	int ret;
	u8 val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;

	ret = cx7601_update_bits(info, CX7601_REG_01, REG01_CHG_CONFIG_MASK, val);
	return ret;
}
#endif
static int cx7601_set_term_current(struct cx7601_charger_info *info, int curr)
{
	u8 iterm;

	if (curr < REG03_ITERM_BASE)
		curr = REG03_ITERM_BASE;

	iterm = (curr - REG03_ITERM_BASE) / REG03_ITERM_LSB;

	return cx7601_update_bits(info, CX7601_REG_03, REG03_ITERM_MASK,
				iterm << REG03_ITERM_SHIFT);
}


static int cx7601_set_prechg_current(struct cx7601_charger_info *info, int curr)
{
	u8 iprechg;

	if(curr <= 180)
		iprechg   = 1;
	else if (curr <= 256)
		iprechg = 2;
	else if(curr <= 384)
		iprechg = 3;
	else if(curr <=512)
		iprechg = 4;
	else
		iprechg = (curr - REG03_IPRECHG_BASE) / REG03_IPRECHG_LSB;

	return cx7601_update_bits(info, CX7601_REG_03, REG03_IPRECHG_MASK,
				iprechg << REG03_IPRECHG_SHIFT);
}
#ifdef TEST_CX7601
static int cx7601_set_watchdog_timer(struct cx7601_charger_info *info, u8 timeout)
{
	u8 temp;

	temp = (u8)(((timeout - REG05_WDT_BASE) / REG05_WDT_LSB) << REG05_WDT_SHIFT);

	return cx7601_update_bits(info, CX7601_REG_05, REG05_WDT_MASK, temp);
}
#endif

static int cx7601_disable_watchdog_timer(struct cx7601_charger_info *info)
{
	u8 val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;

	return cx7601_update_bits(info, CX7601_REG_05, REG05_WDT_MASK, val);
}
#ifdef TEST_CX7601
static int cx7601_reset_watchdog_timer(struct cx7601_charger_info *info)
{
	u8 val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;

	return cx7601_update_bits(info, CX7601_REG_01, REG01_WDT_RESET_MASK, val);
}

static int cx7601_reset_chip(struct cx7601_charger_info *info)
{
	int ret;

    u8 val = REG01_REG_RESET << REG01_REG_RESET_SHIFT;
	ret = cx7601_update_bits(info, CX7601_REG_01, REG01_REG_RESET_MASK, val);
	pr_err("cx7601_reset_chip\n");
	return ret;
}

static int cx7601_enter_hiz_mode(struct cx7601_charger_info *info)
{
	u8 val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

	return cx7601_update_bits(info, CX7601_REG_00, REG00_ENHIZ_MASK, val);

}

static int cx7601_exit_hiz_mode(struct cx7601_charger_info *info)
{

	u8 val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

	return cx7601_update_bits(info, CX7601_REG_00, REG00_ENHIZ_MASK, val);

}

static int cx7601_get_hiz_mode(struct cx7601_charger_info *info, u8 *state)
{
	u8 val;
	int ret;

	ret = cx7601_read(info, &val, CX7601_REG_00);
	if (ret)
		return ret;
	*state = (val & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;

	return 0;
}
#endif

static int cx7601_enable_term(struct cx7601_charger_info *info, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
	else
		val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

	ret = cx7601_update_bits(info, CX7601_REG_05, REG05_EN_TERM_MASK, val);

	return ret;
}

static int cx7601_set_boost_current(struct cx7601_charger_info *info, int curr)
{
	u8 val;

	val = REG01_BOOST_LIM_0P5A;
	if (curr == BOOSTI_1200)
		val = REG01_BOOST_LIM_1P2A;

	return cx7601_update_bits(info, CX7601_REG_01, REG01_BOOST_LIM_MASK,
				val << REG01_BOOST_LIM_SHIFT);//done
}

static int cx7601_set_boost_voltage(struct cx7601_charger_info *info, int volt)
{
	u8 val;

	if (volt == BOOSTV_4850)
		val = REG06_BOOSTV_4P85V;
	else if (volt == BOOSTV_5150)
		val = REG06_BOOSTV_5P15V;
	else if (volt == BOOSTV_5300)
		val = REG06_BOOSTV_5P3V;
	else
		val = REG06_BOOSTV_5P15V;

	return cx7601_update_bits(info, CX7601_REG_06, REG06_BOOSTV_MASK,
				val << REG06_BOOSTV_SHIFT);
}

#ifdef TEST_CX7601
static int cx7601_set_chargevolt(struct cx7601_charger_info *info, int volt)
{
	u8 val;

	if (volt < REG04_VREG_BASE)
		volt = REG04_VREG_BASE;

	if (volt == 4420)
		volt = 4384;
	if (volt == 4240)
		volt = 4192;
	val = (volt - REG04_VREG_BASE)/REG04_VREG_LSB;
	return cx7601_update_bits(info, CX7601_REG_04, REG04_VREG_MASK,
				val << REG04_VREG_SHIFT);
}

static int cx7601_set_input_volt_limit(struct cx7601_charger_info *info, int volt)
{
	u8 val;

	if (volt < REG00_VINDPM_BASE)
		volt = REG00_VINDPM_BASE;

	val = (volt - REG00_VINDPM_BASE) / REG00_VINDPM_LSB;

	if(val > 10)
		val = 10;
	pr_err(" cx7601_set_input_volt_limit volt= %d\n", volt);
	return cx7601_update_bits(info, CX7601_REG_00, REG00_VINDPM_MASK,
				val << REG00_VINDPM_SHIFT);//done
}
#endif
static int cx7601_set_input_current_limit(struct cx7601_charger_info *info, int curr)
{
	u8 val;
 
	
	if (curr <= 100)
		val = 0;
	else if(curr <= 150)
		val = 1;
	else if(curr <= 500)
		val = 2;
	else if(curr <= 900)
		val = 3;
	else if(curr <= 1000)
		val = 4;
	else if(curr <= 1500)
		val = 5;
	else if(curr <= 2000)
		val = 6;
	else
		val = 7;

	return cx7601_update_bits(info, CX7601_REG_00, REG00_IINLIM_MASK,
				val << REG00_IINLIM_SHIFT);//done
}

static int cx7601_set_chargecurrent(struct cx7601_charger_info *info, int curr)
{
	u8 ichg;

	
	if (curr < REG02_ICHG_BASE){
		curr = REG02_ICHG_BASE;
	}
	else if(curr > 2100)
		curr = 2100;

	ichg = (curr - REG02_ICHG_BASE)/REG02_ICHG_LSB;
	return cx7601_update_bits(info, CX7601_REG_02, REG02_ICHG_MASK,
				ichg << REG02_ICHG_SHIFT);

}

static int cx7601_set_stat_ctrl(struct cx7601_charger_info *info, int ctrl)
{
	u8 val;

	val = ctrl;

	return cx7601_update_bits(info, CX7601_REG_00, REG00_STAT_CTRL_MASK,
				val << REG00_STAT_CTRL_SHIFT);
}

#ifdef TEST_CX7601
static int cx7601_enable_batfet(struct cx7601_charger_info *info)
{
	const u8 val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;

	return cx7601_update_bits(info, CX7601_REG_07, REG07_BATFET_DIS_MASK,
				val);
}


static int cx7601_disable_batfet(struct cx7601_charger_info *info)
{
	const u8 val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;

	return cx7601_update_bits(info, CX7601_REG_07, REG07_BATFET_DIS_MASK,
				val);
}

static int cx7601_set_batfet_delay(struct cx7601_charger_info *info, uint8_t delay)
{
	u8 val;

	if (delay == 0)
		val = REG0C_BATFET_DLY_0S;
	else
		val = REG0C_BATFET_DLY_10S;

	val <<= REG0C_BATFET_DLY_SHIFT;

	return cx7601_update_bits(info, CX7601_REG_0C, REG0C_BATFET_DLY_MASK,
								val);//done
}

static int cx7601_enable_safety_timer(struct cx7601_charger_info *info)
{
	const u8 val = REG05_CHG_TIMER_ENABLE << REG05_EN_TIMER_SHIFT;

	return cx7601_update_bits(info, CX7601_REG_05, REG05_EN_TIMER_MASK,
				val);
}
#endif


static int cx7601_disable_safety_timer(struct cx7601_charger_info *info)
{
	const u8 val = REG05_CHG_TIMER_DISABLE << REG05_EN_TIMER_SHIFT;

	return cx7601_update_bits(info, CX7601_REG_05, REG05_EN_TIMER_MASK,
				val);
}

static int cx7601_trim(struct cx7601_charger_info *info)
{
	int ret;
//	u8 data;
	
//	ret = cx7601_write(info, 0x40, 0x50);
//	ret = cx7601_write(info, 0x40, 0x57);
//	ret = cx7601_write(info, 0x40, 0x44);
//	ret = cx7601_write(info, 0x83, 0x2D);
//	ret = cx7601_read(info, &data, 0x83);
//	if (data != 0x2C) {
//		pr_err("Failed to trim cx7601: reg=%02X, data=%02X\n", 0x83, data);
//	}
//	else {
//		pr_err("Trim cx7601 OK\n");
//	}
	ret = cx7601_update_bits(info,0x84,0x03, 0x02);
	ret = cx7601_write(info, 0x40, 0x00);
	ret = cx7601_update_bits(info, CX7601_REG_0C, 0x20, 0x20); //BAT_LOADEN=1
	return ret;
}

static int cx7601_init_device(struct cx7601_charger_info *info)
{
	int ret;

	cx7601_trim(info);
	cx7601_disable_watchdog_timer(info);
	cx7601_charger_set_vindpm(info,4520);
    cx7601_disable_safety_timer(info);     //modified
    cx7601_enable_term(info,false);
	
	ret = cx7601_set_stat_ctrl(info, 3);//info->platform_data->statctrl);
	if (ret)
		pr_err("Failed to set stat pin control mode, ret = %d\n",ret);

	ret = cx7601_set_prechg_current(info, 384);//info->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n",ret);

	ret = cx7601_set_term_current(info, 256);//info->platform_data->iterm);
	if (ret)
		pr_err("Failed to set termination current, ret = %d\n",ret);

	ret = cx7601_set_boost_voltage(info, 5150);//info->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n",ret);

	ret = cx7601_set_boost_current(info, 1200);//info->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n",ret);

	ret = cx7601_charger_set_ovp(info, 6500);//info->platform_data->vac_ovp);
	if (ret)
		pr_err("Failed to set acovp threshold, ret = %d\n",ret);

	return 0;
}

static int cx7601_charger_hw_init(struct cx7601_charger_info *info)
{
	struct power_supply_battery_info bat_info = { };
	int voltage_max_microvolt, current_max_ua;
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
		current_max_ua = bat_info.constant_charge_current_max_ua / 1000;
		power_supply_put_battery_info(info->psy_usb, &bat_info);


		cx7601_init_device(info);

	}

	cx7601_dump_regs(info);

	return ret;
}

static int
cx7601_charger_set_termina_vol(struct cx7601_charger_info *info, u32 vol)
{
	u8 val;

	info->term_voltage = vol;
	if (vol < REG04_VREG_BASE)
		vol = REG04_VREG_BASE;

	val = (vol - REG04_VREG_BASE)/REG04_VREG_LSB;
	return cx7601_update_bits(info, CX7601_REG_04, REG04_VREG_MASK,
				val << REG04_VREG_SHIFT);
}

static int
cx7601_charger_get_termina_vol(struct cx7601_charger_info *info, u32 *vol)
{
	u8 reg_val;
	int ret;

	ret = cx7601_read(info,  &reg_val, CX7601_REG_04);
	if (ret < 0)
		return ret;

	reg_val &= REG04_VREG_MASK;
	*vol = REG04_VREG_BASE + (reg_val >> REG04_VREG_SHIFT) * 16;

	return 0;
}

static int cx7601_charging(struct cx7601_charger_info *info, bool enable)
{

	int ret = 0;
	u8 val;

	
	if (enable)
  	{
		ret = cx7601_enable_charger(info);
  	 }
	else
  	{
	//	ret = cx7601_disable_charger(info);
   	}
	ret = cx7601_read(info, &val, CX7601_REG_01);

	pr_err("%s %s  %s;%d;\n",__func__, enable ? "enable" : "disable",
				  !ret ? "successfully" : "failed",!!(val & REG01_CHG_CONFIG_MASK));

	return ret;
}

static int cx7601_charger_start_charge(struct cx7601_charger_info *info)
{
	int ret;

	if (info->role == CX7601_ROLE_MASTER_DEFAULT) {
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask, 0);
		if (ret) {
			dev_err(info->dev, "enable cx7601 charge failed\n");
			return ret;
		}
	} else if (info->role == CX7601_ROLE_SLAVE) {
		gpiod_set_value_cansleep(info->gpiod, 0);
	}

	cx7601_enable_term(info,true);

	cx7601_charging(info,true);

	ret = cx7601_charger_set_limit_current(info,
						info->last_limit_current);
	if (ret)
		dev_err(info->dev, "failed to set limit current\n");

	return ret;
}

static void cx7601_charger_stop_charge(struct cx7601_charger_info *info)
{
	int ret;
	bool present = cx7601_charger_is_bat_present(info);

	if (info->role == CX7601_ROLE_MASTER_DEFAULT) {
		if (!present || info->need_disable_Q1) {

			info->need_disable_Q1 = false;
		}

		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask,
					 info->charger_pd_mask);
		if (ret)
			dev_err(info->dev, "disable cx7601 charge failed\n");
	} else if (info->role == CX7601_ROLE_SLAVE) {

		gpiod_set_value_cansleep(info->gpiod, 1);
	}

	cx7601_charging(info,false);

}

static int cx7601_charger_set_current(struct cx7601_charger_info *info,
				       u32 cur)
{

	info->last_current = cur;
	pr_info("[%s] cur=%d\n", __func__, cur);

	cx7601_set_chargecurrent(info,cur);
	return 0;
}

static int cx7601_charger_get_current(struct cx7601_charger_info *info,
				       u32 *cur)
{

	u8 reg_val;
	int ichg;
	int ret;

	ret = cx7601_read(info, &reg_val, CX7601_REG_02);
	if (!ret) {
		ichg = (reg_val & REG02_ICHG_MASK) >> REG02_ICHG_SHIFT;
		ichg = ichg * REG02_ICHG_LSB + REG02_ICHG_BASE;
		*cur = ichg * 1000;
	}
	pr_info("[%s] cur=%d\n", __func__, *cur);

	return 0;
}

static int
cx7601_charger_set_limit_current(struct cx7601_charger_info *info,
				  u32 limit_cur)
{

	info->last_limit_current = limit_cur;
	pr_info("[%s] limit_cur=%d\n", __func__, limit_cur);
	cx7601_set_input_current_limit(info, limit_cur);

	return 0;
}

static u32
cx7601_charger_get_limit_current(struct cx7601_charger_info *info,
				  u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = cx7601_read(info,  &reg_val, CX7601_REG_00);
	if (ret < 0)
		return ret;

	reg_val &= REG00_IINLIM_MASK;
	reg_val = reg_val >>REG00_IINLIM_SHIFT;

	if(reg_val == 0)
		*limit_cur = 100 *1000;
	else if(reg_val == 1)
		*limit_cur = 150 *1000;
	else if(reg_val == 2)
		*limit_cur = 500 *1000;
	else if(reg_val == 3)
		*limit_cur = 900 *1000;
	else if(reg_val == 4)
		*limit_cur = 1000 *1000;
	else if(reg_val == 5)
		*limit_cur = 1500 *1000;
	else if(reg_val == 6)
		*limit_cur = 2000 *1000;
	else if(reg_val == 7)
		*limit_cur = 3000 *1000;

	pr_info("[%s] limit_cur=%d\n", __func__, *limit_cur /1000);
		

	return 0;
}

static int cx7601_charger_get_health(struct cx7601_charger_info *info,
				      u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int cx7601_charger_get_online(struct cx7601_charger_info *info,
				      u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}

static int cx7601_charger_feed_watchdog(struct cx7601_charger_info *info,
					 u32 val)
{
	u8 reg;
	static u8 ovp=0;

//	cx7601_reset_watchdog_timer(info);
	cx7601_read(info, &reg, CX7601_REG_09 );
	if((ovp==0) &&  ((reg & 0x08) == 0x08)) //ovp
	{
		dev_err(info->dev,"%s ovp 09 reg=%x;",__func__,reg);
		cx7601_update_bits(info,CX7601_REG_07,0x20,0x20);
		ovp=1;
	}
	else if((ovp==1)  && ((reg & 0x08) == 0x00))  // not ovp
	{

		dev_err(info->dev,"%s novp  09 reg=%x;",__func__,reg);
		cx7601_update_bits(info,CX7601_REG_07,0x20,0x00);
		ovp=0;	
	}


	cx7601_read(info, &reg, CX7601_REG_05 );
	if((reg & 0x30 ) == 0x10)
	{
		dev_err(info->dev,"%s  05 reg=%x;",__func__,reg);
		cx7601_init_device(info);
		cx7601_charger_set_termina_vol(info, info->term_voltage);
		cx7601_charger_set_limit_current(info, info->last_limit_current);
		cx7601_charger_set_current(info, info->last_current);
	}

	cx7601_dump_regs(info);

	return 0;
}

static bool cx7601_charge_done(struct cx7601_charger_info *info)
{
	if (info->charging)
	{
		unsigned char val = 0;

		cx7601_read(info, &val, 0x08 );
		
		val = ( val >> 4 ) & 0x03;

		if(val == 0x3)
		{
			cx7601_enable_term(info,false);

			return true;
		}
		else
			return false;
	}	
	else
		return false;
}

static int cx7601_charger_get_status(struct cx7601_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int cx7601_charger_set_status(struct cx7601_charger_info *info,
				      int val)
{
	int ret = 0;


	if (!val && info->charging) {
		cx7601_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = cx7601_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static void cx7601_charger_work(struct work_struct *data)
{
	struct cx7601_charger_info *info =
		container_of(data, struct cx7601_charger_info, work);
	int limit_cur, cur, ret;
	bool present = cx7601_charger_is_bat_present(info);

	mutex_lock(&info->lock);

	if (info->limit > 0 && !info->charging && present) {
		/* set current limitation and start to charge */
		switch (info->usb_phy->chg_type) {
		case SDP_TYPE:
			limit_cur = info->cur.sdp_limit;
			cur = info->cur.sdp_cur;
			break;
		case DCP_TYPE:
			limit_cur = info->cur.dcp_limit;
			cur = info->cur.dcp_cur;
			break;
		case CDP_TYPE:
			limit_cur = info->cur.cdp_limit;
			cur = info->cur.cdp_cur;
			break;
		default:
			limit_cur = info->cur.unknown_limit;
			cur = info->cur.unknown_cur;
		}

		ret = cx7601_charger_set_limit_current(info, limit_cur/1000);
		if (ret)
			goto out;

		ret = cx7601_charger_set_current(info, cur/1000);
		if (ret)
			goto out;

	} else if ((!info->limit && info->charging) || !present) {
	}

out:
	mutex_unlock(&info->lock);
	dev_info(info->dev, "battery present = %d, charger type = %d\n",
		 present, info->usb_phy->chg_type);
	cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}


static int cx7601_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct cx7601_charger_info *info =
		container_of(nb, struct cx7601_charger_info, usb_notify);

	info->limit = limit;

	/*
	 * only master should do work when vbus change.
	 * let info->limit = limit, slave will online, too.
	 */
	if (info->role == CX7601_ROLE_SLAVE)
		return NOTIFY_OK;

	schedule_work(&info->work);
	return NOTIFY_OK;
}

static int cx7601_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{
	struct cx7601_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, online, health, vol;
	enum usb_charger_type type;
	int ret = 0;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->limit)
			val->intval = cx7601_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = cx7601_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = cx7601_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = cx7601_charger_get_online(info, &online);
		if (ret)
			goto out;

		val->intval = online;

		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = cx7601_charger_get_health(info, &health);
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

		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
			val->intval =cx7601_charge_done(info);
		break;
		
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = cx7601_charger_get_termina_vol(info, &vol);
		val->intval = vol *1000;
		break;

	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int cx7601_charger_usb_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct cx7601_charger_info *info = power_supply_get_drvdata(psy);
	int ret;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = cx7601_charger_set_current(info, val->intval/1000);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = cx7601_charger_set_limit_current(info, val->intval/1000);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_STATUS:
		ret = cx7601_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;

	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		ret = cx7601_charger_feed_watchdog(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "feed charger watchdog failed\n");
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = cx7601_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		cx7601_enable_powerpath(info, val->intval);
		break;

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int cx7601_charger_property_is_writeable(struct power_supply *psy,
						 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_STATUS:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_usb_type cx7601_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static enum power_supply_property cx7601_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_POWER_NOW,
};

static const struct power_supply_desc cx7601_charger_desc = {
	.name			= "charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= cx7601_usb_props,
	.num_properties		= ARRAY_SIZE(cx7601_usb_props),
	.get_property		= cx7601_charger_usb_get_property,
	.set_property		= cx7601_charger_usb_set_property,
	.property_is_writeable	= cx7601_charger_property_is_writeable,
	.usb_types		= cx7601_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(cx7601_charger_usb_types),
};

static const struct power_supply_desc cx7601_slave_charger_desc = {
	.name			= "cx7601_slave_charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= cx7601_usb_props,
	.num_properties		= ARRAY_SIZE(cx7601_usb_props),
	.get_property		= cx7601_charger_usb_get_property,
	.set_property		= cx7601_charger_usb_set_property,
	.property_is_writeable	= cx7601_charger_property_is_writeable,
	.usb_types		= cx7601_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(cx7601_charger_usb_types),
};

static void cx7601_charger_detect_status(struct cx7601_charger_info *info)
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
	if (info->role == CX7601_ROLE_SLAVE)
		return;
	schedule_work(&info->work);
}

static void
cx7601_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct cx7601_charger_info *info = container_of(dwork,
							 struct cx7601_charger_info,
							 wdt_work);

//	cx7601_reset_watchdog_timer(info);

	cx7601_dump_regs(info);

	schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#ifdef CONFIG_REGULATOR
static void cx7601_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct cx7601_charger_info *info = container_of(dwork,
			struct cx7601_charger_info, otg_work);
	bool otg_valid = extcon_get_state(info->edev, EXTCON_USB);
	int ret, retry = 0;

	if (otg_valid)
		goto out;

	do {
		ret = cx7601_enable_otg(info);
		if (ret)
			dev_err(info->dev, "restart bq2560x charger otg failed\n");
		
		otg_valid = extcon_get_state(info->edev, EXTCON_USB);
	} while (!otg_valid && retry++ < CX7601_OTG_RETRY_TIMES);

	if (retry >= CX7601_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int cx7601_charger_enable_otg(struct regulator_dev *dev)
{
	struct cx7601_charger_info *info = rdev_get_drvdata(dev);
	int ret;

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

	ret = cx7601_enable_otg(info);
	
	if (ret) {
		dev_err(info->dev, "enable cx7601 otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect,
				   BIT_DP_DM_BC_ENB, 0);
		return ret;
	}

	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(CX7601_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(CX7601_OTG_VALID_MS));

	return 0;
}

static int cx7601_charger_disable_otg(struct regulator_dev *dev)
{
	struct cx7601_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	ret = cx7601_disable_otg(info);
	if (ret) {
		dev_err(info->dev, "disable cx7601 otg failed\n");
		return ret;
	}

	/* Enable charger detection function to identify the charger type */
	return regmap_update_bits(info->pmic, info->charger_detect,
				  BIT_DP_DM_BC_ENB, 0);
}

static int cx7601_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct cx7601_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	ret = cx7601_read(info, &val, CX7601_REG_01);
	if (ret) {
		dev_err(info->dev, "failed to get cx7601 otg status\n");
		return ret;
	}

	val &= REG01_OTG_CONFIG_MASK;

	return val;
}

static const struct regulator_ops cx7601_charger_vbus_ops = {
	.enable = cx7601_charger_enable_otg,
	.disable = cx7601_charger_disable_otg,
	.is_enabled = cx7601_charger_vbus_is_enabled,
};

static const struct regulator_desc cx7601_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &cx7601_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int
cx7601_charger_register_vbus_regulator(struct cx7601_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &cx7601_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int
cx7601_charger_register_vbus_regulator(struct cx7601_charger_info *info)
{
	return 0;
}
#endif

static int cx7601_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct cx7601_charger_info *info;
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
	info->client->addr = 0x6b;
	info->dev = dev;

	cx7601_write(info, 0x40, 0x50);
	cx7601_write(info, 0x40, 0x57);
	cx7601_write(info, 0x40, 0x44);
	cx7601_write(info, 0x83, 0x2D);
	cx7601_read(info, &val, 0x83);
	dev_err(dev, "%s;enter;0x83=%x;\n",__func__,val);


	cx7601_read(info, &val, CX7601_REG_0A);
	dev_err(dev, "%s;%x;\n",__func__,val);
	if( ((val & 0xe0) >>5) == 0x02  &&   (val & 0x07) == 0x00 )
	       strncpy(charge_ic_vendor_name,"CX7601",20);
	else
		return -ENODEV;

	dev_err(dev, "%s;%s;\n",__func__,charge_ic_vendor_name);

	ret = device_property_read_bool(dev, "role-slave");
	if (ret)
		info->role = CX7601_ROLE_SLAVE;
	else
		info->role = CX7601_ROLE_MASTER_DEFAULT;

	if (info->role == CX7601_ROLE_SLAVE) {
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

	/*
	 * only master to support otg
	 */
	if (info->role == CX7601_ROLE_MASTER_DEFAULT) {
		ret = cx7601_charger_register_vbus_regulator(info);
		if (ret) {
			dev_err(dev, "failed to register vbus regulator.\n");
			return ret;
		}
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
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

	if (of_device_is_compatible(regmap_np->parent, "sprd,sc2730"))
		info->charger_pd_mask = CX7601_DISABLE_PIN_MASK_2730;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
		info->charger_pd_mask = CX7601_DISABLE_PIN_MASK_2721;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2720"))
		info->charger_pd_mask = CX7601_DISABLE_PIN_MASK_2720;
	else {
		dev_err(dev, "failed to get charger_pd mask\n");
		return -EINVAL;
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

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	if (info->role == CX7601_ROLE_MASTER_DEFAULT) {
		info->psy_usb = devm_power_supply_register(dev,
							   &cx7601_charger_desc,
							   &charger_cfg);
	} else if (info->role == CX7601_ROLE_SLAVE) {
		info->psy_usb = devm_power_supply_register(dev,
							   &cx7601_slave_charger_desc,
							   &charger_cfg);
	}

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		return PTR_ERR(info->psy_usb);
	}

	ret = cx7601_charger_hw_init(info);
	if (ret)
		return ret;

	info->usb_notify.notifier_call = cx7601_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		return ret;
	}

	mutex_init(&info->lock);
	INIT_WORK(&info->work, cx7601_charger_work);

	cx7601_charger_detect_status(info);
	INIT_DELAYED_WORK(&info->otg_work, cx7601_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work,
			  cx7601_charger_feed_watchdog_work);

	dev_err(dev, "cx7601_charger_probe ok to register\n");

//+add by hzb for ontim debug
        REGISTER_AND_INIT_ONTIM_DEBUG_FOR_THIS_DEV();
//-add by hzb for ontim debug

	return 0;
}

static int cx7601_charger_remove(struct i2c_client *client)
{
	struct cx7601_charger_info *info = i2c_get_clientdata(client);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

static const struct i2c_device_id cx7601_i2c_id[] = {
	{"cx7601_chg", 0},
	{}
};

static const struct of_device_id cx7601_charger_of_match[] = {
	{ .compatible = "sun,cx7601_chg", },
	{ }
};

static const struct i2c_device_id cx7601_slave_i2c_id[] = {
	{"cx7601_slave_chg", 0},
	{}
};

static const struct of_device_id cx7601_slave_charger_of_match[] = {
	{ .compatible = "sun,cx7601_slave_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, cx7601_charger_of_match);
MODULE_DEVICE_TABLE(of, cx7601_slave_charger_of_match);

static struct i2c_driver cx7601_master_charger_driver = {
	.driver = {
		.name = "cx7601_chg",
		.of_match_table = cx7601_charger_of_match,
	},
	.probe = cx7601_charger_probe,
	.remove = cx7601_charger_remove,
	.id_table = cx7601_i2c_id,
};

static struct i2c_driver cx7601_slave_charger_driver = {
	.driver = {
		.name = "cx7601_slave_chg",
		.of_match_table = cx7601_slave_charger_of_match,
	},
	.probe = cx7601_charger_probe,
	.remove = cx7601_charger_remove,
	.id_table = cx7601_slave_i2c_id,
};

module_i2c_driver(cx7601_master_charger_driver);
module_i2c_driver(cx7601_slave_charger_driver);
MODULE_DESCRIPTION("CX7601 Charger Driver");
MODULE_LICENSE("GPL v2");
