/*
 * Driver for the FAIRCHILD bq24157 charger.
 * Author: Mark A. Greer <mgreer@animalcreek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
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


#define BQ24157_BATTERY_NAME				"sc27xx-fgu"
#define BIT_DP_DM_BC_ENB				BIT(0)
#define BQ24157_OTG_VALID_MS				500
#define BQ24157_FEED_WATCHDOG_VALID_MS			50

#define BQ24157_REG_HZ_MODE_MASK			GENMASK(1, 1)
#define BQ24157_REG_OPA_MODE_MASK			GENMASK(0, 0)

#define BQ24157_DISABLE_PIN_MASK_2730			BIT(0)
#define BQ24157_DISABLE_PIN_MASK_2721			BIT(15)
#define BQ24157_DISABLE_PIN_MASK_2720			BIT(0)

#define VENDOR_BQ24157					(0x1)

/******************************************************************************
* Register addresses
******************************************************************************/

#define BQ24157_CON0      0x00
#define BQ24157_CON1      0x01
#define BQ24157_CON2      0x02
#define BQ24157_CON3      0x03
#define BQ24157_CON4      0x04
#define BQ24157_CON5      0x05
#define BQ24157_CON6      0x06
#define BQ24157_REG_NUM		7

#define BQ24157_REG_OREG (0x1)

#define BQ24157_OTG_EN (0x3 << 4)
#define BQ24157_OTG_EN_SHIFT (4)

/******************************************************************************
* Register bits
******************************************************************************/
/* CON0 */
#define CON0_TMR_RST_MASK	0x01
#define CON0_TMR_RST_SHIFT	7
#define CON0_OTG_MASK		0x01
#define CON0_OTG_SHIFT		7
#define CON0_EN_STAT_MASK	0x01
#define CON0_EN_STAT_SHIFT	6
#define CON0_STAT_MASK		0x03
#define CON0_STAT_SHIFT		4
#define CON0_BOOST_MASK		0x01
#define CON0_BOOST_SHIFT	3
#define CON0_FAULT_MASK		0x07
#define CON0_FAULT_SHIFT	0

/* CON1 */
#define CON1_LIN_LIMIT_MASK	0x03
#define CON1_LIN_LIMIT_SHIFT	6
#define CON1_LOW_V_MASK		0x03
#define CON1_LOW_V_SHIFT	4
#define CON1_TE_MASK		0x01
#define CON1_TE_SHIFT		3
#define CON1_CE_MASK		0x01
#define CON1_CE_SHIFT		2
#define CON1_HZ_MODE_MASK	0x01
#define CON1_HZ_MODE_SHIFT	1
#define CON1_OPA_MODE_MASK	0x01
#define CON1_OPA_MODE_SHIFT	0

/* CON2 */
#define CON2_OREG_MASK		0x3F
#define CON2_OREG_SHIFT		2
#define CON2_OTG_PL_MASK	0x01
#define CON2_OTG_PL_SHIFT	1
#define CON2_OTG_EN_MASK	0x01
#define CON2_OTG_EN_SHIFT	0

/* CON3 */
#define CON3_VENDER_CODE_MASK	0x07
#define CON3_VENDER_CODE_SHIFT	5
#define CON3_PIN_MASK		0x03
#define CON3_PIN_SHIFT		3
#define CON3_REVISION_MASK	0x07
#define CON3_REVISION_SHIFT	0

/* CON4 */
#define CON4_RESET_MASK		0x01
#define CON4_RESET_SHIFT	7
#define CON4_I_CHR_MASK		0x07
#define CON4_I_CHR_SHIFT	4
#define CON4_I_TERM_MASK	0x07
#define CON4_I_TERM_SHIFT	0

/* CON5 */
#define CON5_FLAG_MASK	0x01
#define CON5_FLAG_SHIFT	7
#define CON5_DIS_VREG_MASK	0x01
#define CON5_DIS_VREG_SHIFT	6
#define CON5_IO_LEVEL_MASK	0x01
#define CON5_IO_LEVEL_SHIFT	5
#define CON5_SP_STATUS_MASK	0x01
#define CON5_SP_STATUS_SHIFT	4
#define CON5_EN_LEVEL_MASK	0x01
#define CON5_EN_LEVEL_SHIFT	3
#define CON5_VSP_MASK		0x07
#define CON5_VSP_SHIFT		0

/* CON6 */
#define CON6_ISAFE_MASK		0x07
#define CON6_ISAFE_SHIFT	4
#define CON6_VSAFE_MASK		0x0F
#define CON6_VSAFE_SHIFT	0

static unsigned int watchdog_time=0;

struct bq24157_charger_info {
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
	u32 voltage_max_microvolt;
};

#include <ontim/ontim_dev_dgb.h>
static  char charge_ic_vendor_name[50]="BQ24157";
DEV_ATTR_DECLARE(charge_ic)
DEV_ATTR_DEFINE("vendor",charge_ic_vendor_name)
DEV_ATTR_DECLARE_END;
ONTIM_DEBUG_DECLARE_AND_INIT(charge_ic,charge_ic,8);

static bool is_eta6937=false;

static unsigned int bq24157_get_max_cur(void)
{
	if(is_eta6937)
		return 	1050000;
	else
		return 	1150000;
}
static bool bq24157_charger_is_bat_present(struct bq24157_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(BQ24157_BATTERY_NAME);
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

static int bq24157_read(struct bq24157_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int bq24157_write(struct bq24157_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int bq24157_update_bits(struct bq24157_charger_info *info, u8 reg,
		u8 mask, u8 shift ,u8 data)
{
	u8 v;
	int ret;

	ret = bq24157_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~(mask << shift);
	v |= (data << shift);

	return bq24157_write(info, reg, v);
}
static int bq24157_dump_register(struct bq24157_charger_info *info)
{
	int i;
	u8 val[BQ24157_REG_NUM];

	for (i = 0; i < BQ24157_REG_NUM; i++) {
		bq24157_read(info,i, &val[i]);
	}
	dev_err(info->dev,"bq24157 [0x0]=0x%x [0x1]=0x%x [0x2]=0x%x  [0x3]=0x%x [0x4]=0x%x [0x5]=0x%x [0x6]=0x%x \n",
		                      val[0],val[1],val[2],
		                      val[3],val[4],val[5],val[6]);

	return 0;
}

static void bq24157_set_ce(struct bq24157_charger_info *info, u8 val)
{
	bq24157_update_bits(info, BQ24157_CON1,
				CON1_CE_MASK,
				CON1_CE_SHIFT,
				val
				);
}


static void bq24157_set_te(struct bq24157_charger_info *info, u8 val)
{
	bq24157_update_bits(info, BQ24157_CON1,
				CON1_TE_MASK,
				CON1_TE_SHIFT,
				val
				);
}


static void bq24157_set_hz_mode(struct bq24157_charger_info *info, u8 val)
{
	bq24157_update_bits(info, BQ24157_CON1,
				CON1_HZ_MODE_MASK,
				CON1_HZ_MODE_SHIFT,
				val
				);
}

static void bq24157_set_opa_mode(struct bq24157_charger_info *info, u8 val)
{
	bq24157_update_bits(info, BQ24157_CON1,
				CON1_OPA_MODE_MASK,
				CON1_OPA_MODE_SHIFT,
				val
				);
}

static void bq24157_set_otg_en(struct bq24157_charger_info *info, u8 val)
{
	bq24157_update_bits(info, BQ24157_CON2,
				CON2_OTG_EN_MASK,
				CON2_OTG_EN_SHIFT,
				val
				);
}

static int
bq24157_charger_set_termina_vol(struct bq24157_charger_info *info, u32 vol)
{
	u8 reg_val;
	dev_err(info->dev, "%s;%d;\n",__func__,vol);

	if(vol <= 3500){
		reg_val = 0x0;
	}else if( vol >= 4440){
		reg_val = 0x2f;
	}else{
		reg_val = (vol - 3500) / 20;
	}

	return bq24157_update_bits(info, BQ24157_CON2,
				    CON2_OREG_MASK,
				    CON2_OREG_SHIFT,
				    reg_val);
}

static int bq24157_charger_get_termina_vol(struct bq24157_charger_info *info, u32 *vol)
{

	u8 reg_val;
	int ret;

	ret = bq24157_read(info, BQ24157_CON2, &reg_val);
	if (ret < 0)
		return ret;

	reg_val = reg_val >> CON2_OREG_SHIFT;
	reg_val &= CON2_OREG_MASK;

	*vol = 3500 + (reg_val * 20);
	if(*vol >4440)
		*vol	 = 4440;
	
	return 0;
}

void bq24157_set_iterm(struct bq24157_charger_info *info, u8 val)
{
	bq24157_update_bits(info, BQ24157_CON4,
				CON4_I_TERM_MASK,
				CON4_I_TERM_SHIFT,
				val
				);
}

void bq24157_set_vsp(struct bq24157_charger_info *info, u8 val)
{
	bq24157_update_bits(info, BQ24157_CON5,
				CON5_VSP_MASK,
				CON5_VSP_SHIFT,
				val
				);
}

void bq24157_set_io_level(struct bq24157_charger_info *info, u8 val)
{
	bq24157_update_bits(info,BQ24157_CON5,
				CON5_IO_LEVEL_MASK,
				CON5_IO_LEVEL_SHIFT,
				val
				);
}

static void bq24157_set_tmr_rst(struct bq24157_charger_info *info, u8 val)
{
	
	if(info->charging)
	{
		watchdog_time++;
	       if(watchdog_time > 600)
			bq24157_set_ce(info,1);		
	}
	else
	{
		watchdog_time = 0;
	}
		
	bq24157_update_bits(info, BQ24157_CON0,
				CON0_TMR_RST_MASK,
				CON0_TMR_RST_SHIFT,
				val
				);
	bq24157_dump_register(info);

	if(info->charging && watchdog_time > 600)
	{
		dev_err(info->dev,"%s;%d;\n",__func__,watchdog_time);
		watchdog_time = 0;
		if(is_eta6937)	
		bq24157_write(info,0x06, 0xac);

		bq24157_set_hz_mode(info,0);
		bq24157_set_opa_mode(info,0);
		bq24157_set_te(info,1);

		bq24157_set_iterm(info,2);
		bq24157_set_vsp(info,3);
		bq24157_set_ce(info,0);
		
	}
	
}

static int bq24157_enable_charging(struct bq24157_charger_info *info, bool en)
{
	unsigned int ret = 0;

	dev_err(info->dev, "%s;%d;\n",__func__,en);

	if (en) {
		watchdog_time = 0;
		
		if(is_eta6937)	
		bq24157_write(info,0x06, 0xac);	/* ISAFE = 1550mA, VSAFE = 4.4V */
		bq24157_set_ce(info,1);
		bq24157_charger_set_termina_vol(info, info->voltage_max_microvolt);
		bq24157_set_hz_mode(info,0);
		bq24157_set_opa_mode(info,0);
		bq24157_set_te(info,1);

		bq24157_set_iterm(info,2);
		bq24157_set_vsp(info,3);
		bq24157_set_ce(info,0);
		
	} else {
//		bq24157_set_ce(info,1);
//		bq24157_set_hz_mode(info,1);
		bq24157_charger_set_termina_vol(info,3500);
}

	return ret;
}

static int bq24157_charger_hw_init(struct bq24157_charger_info *info)
{
	struct power_supply_battery_info bat_info = { };
	int ret;

	ret = power_supply_get_battery_info(info->psy_usb, &bat_info);
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
		info->voltage_max_microvolt = bat_info.constant_charge_voltage_max_uv / 1000;
		power_supply_put_battery_info(info->psy_usb, &bat_info);

		}

	bq24157_write(info,0x06, 0xac);	/* ISAFE = 1550mA, VSAFE = 4.4V */

	bq24157_write(info,0x00, 0xC0);	/* kick chip watch dog */
	bq24157_write(info,0x01, 0xb8);	/* TE=1, CE=0, HZ_MODE=0, OPA_MODE=0 */
	bq24157_write(info,0x05, 0x03);

	bq24157_write(info,0x04, 0x02);	

       bq24157_write(info,0x02, 0x02);//cccv 3.5v

	bq24157_dump_register(info);


	return ret;
}

static int bq24157_charger_start_charge(struct bq24157_charger_info *info)
{
	int ret;

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask, 0);

	bq24157_enable_charging(info ,1 );
	
	if (ret)
		dev_err(info->dev, "enable bq24157 charge failed\n");

	return ret;
}

static void bq24157_charger_stop_charge(struct bq24157_charger_info *info)
{
	int ret;

	ret = regmap_update_bits(info->pmic, info->charger_pd,
				 info->charger_pd_mask,
				 info->charger_pd_mask);

	bq24157_enable_charging(info ,0 );

	if (ret)
		dev_err(info->dev, "disable bq24157 charge failed\n");
}
static int bq24157_charger_set_current(struct bq24157_charger_info *info,
					u32 cur)
{
	u8 reg_val;

	dev_err(info->dev, "%s;%d;%d;\n",__func__,cur,bq24157_get_max_cur());

	if (cur <= 500000) 
	{
		bq24157_set_io_level(info,0);
		
		if(is_eta6937)
			reg_val = 0;
		else
			reg_val = 3;
	}else {
		bq24157_set_io_level(info,0);

		if(cur > bq24157_get_max_cur())
			cur= bq24157_get_max_cur() ;
		reg_val = (cur-550000)/100000;
	}

	return bq24157_update_bits(info,BQ24157_CON4,
				CON4_I_CHR_MASK,
				CON4_I_CHR_SHIFT,
				reg_val
				);
}

static int bq24157_charger_get_current(struct bq24157_charger_info *info,
					u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = bq24157_read(info, BQ24157_CON4, &reg_val);
	if (ret < 0)
		return ret;

	reg_val = reg_val >> CON4_I_CHR_SHIFT;
	reg_val &= CON4_I_CHR_MASK;

	*cur = (reg_val * 100000) + 550000;
	return 0;
}

static int
bq24157_charger_set_limit_current(struct bq24157_charger_info *info,
				   u32 cur)
{
	u8 reg_val;
	int ret;

	dev_err(info->dev, "%s;%d;\n",__func__,cur);

	if (cur <= 100000)
		reg_val = 0x0;
	else if (cur <= 500000)
		reg_val = 0x1;
	else if (cur <= 800000)
		reg_val = 0x2;
	else
		reg_val = 0x3;

	ret = bq24157_update_bits(info, BQ24157_CON1,
				    CON1_LIN_LIMIT_MASK,
				    CON1_LIN_LIMIT_SHIFT,
				    reg_val);
	if (ret)
		dev_err(info->dev, "set bq24157 limit cur failed\n");

	return ret;
}

static u32
bq24157_charger_get_limit_current(struct bq24157_charger_info *info,
				   u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = bq24157_read(info, BQ24157_CON1, &reg_val);
	if (ret < 0)
		return ret;

	reg_val = reg_val >> CON1_LIN_LIMIT_SHIFT;
	reg_val &= CON1_LIN_LIMIT_MASK;

	switch (reg_val) {
	case 0:
		*limit_cur = 100000;
		break;
	case 1:
		*limit_cur = 500000;
		break;
	case 2:
		*limit_cur = 800000;
		break;
	case 3:
		*limit_cur = 1500000;
		break;
	default:
		*limit_cur = 1500000;
	}

	return 0;
}

static int bq24157_charger_get_health(struct bq24157_charger_info *info,
				     u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int bq24157_charger_get_online(struct bq24157_charger_info *info,
				     u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}

static int bq24157_charger_feed_watchdog(struct bq24157_charger_info *info,
					  u32 val)
{
	bq24157_set_tmr_rst(info, 1);

	return 0;
}
static bool bq24157_charge_done(struct bq24157_charger_info *info)
{
	if (info->charging)
	{
		unsigned char val = 0;

		bq24157_read(info, BQ24157_CON0, &val);
		val = ( val >> CON0_STAT_SHIFT ) & CON0_STAT_MASK;

		if(val == 0x2)
			return true;
		else
			return false;
	}	
	else
		return false;
}

static int bq24157_charger_get_status(struct bq24157_charger_info *info)
{
	if (info->charging == true)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int bq24157_charger_set_status(struct bq24157_charger_info *info,
				       int val)
{
	int ret = 0;

	if (!val && info->charging) {
		bq24157_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = bq24157_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static void bq24157_charger_work(struct work_struct *data)
{
	struct bq24157_charger_info *info =
		container_of(data, struct bq24157_charger_info, work);
	int limit_cur, cur, ret;
	bool present = bq24157_charger_is_bat_present(info);
	dev_info(info->dev, "%s;p%d, l%d,chg%d;\n",
		__func__, present, info->limit,info->charging );

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

		ret = bq24157_charger_set_limit_current(info, limit_cur);
		if (ret)
			goto out;

		ret = bq24157_charger_set_current(info, cur);
		if (ret)
			goto out;

		ret = bq24157_charger_start_charge(info);
		if (ret)
			goto out;

		info->charging = true;
	} else if ((!info->limit && info->charging) || !present) {
		/* Stop charging */
		info->charging = false;
		bq24157_charger_stop_charge(info);
	}

out:
	mutex_unlock(&info->lock);
	dev_info(info->dev, "battery present = %d, charger type = %d\n",
		 present, info->usb_phy->chg_type);
	cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}


static int bq24157_charger_usb_change(struct notifier_block *nb,
				       unsigned long limit, void *data)
{
	struct bq24157_charger_info *info =
		container_of(nb, struct bq24157_charger_info, usb_notify);

	info->limit = limit;

	schedule_work(&info->work);
	return NOTIFY_OK;
}

static int bq24157_charger_usb_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct bq24157_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, online, health,vol;
	enum usb_charger_type type;
	int ret = 0;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->limit)
			val->intval = bq24157_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = bq24157_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = bq24157_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = bq24157_charger_get_online(info, &online);
		if (ret)
			goto out;

		val->intval = online;

		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = bq24157_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;

	case POWER_SUPPLY_PROP_USB_TYPE:
		type = info->usb_phy->chg_type;;
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
	case POWER_SUPPLY_PROP_TYPE:
		type = info->usb_phy->chg_type;;
		switch (type) {
		case SDP_TYPE:
			val->intval = POWER_SUPPLY_TYPE_USB;
			break;

		case DCP_TYPE:
			val->intval = POWER_SUPPLY_TYPE_USB_DCP;
			break;

		case CDP_TYPE:
			val->intval = POWER_SUPPLY_TYPE_USB_CDP;
			break;

		default:
			val->intval = POWER_SUPPLY_TYPE_UNKNOWN;
		}
		break;
		
	case POWER_SUPPLY_PROP_CHARGE_FULL:
			val->intval =bq24157_charge_done(info);
		break;
		
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = bq24157_charger_get_termina_vol(info, &vol);
		val->intval = vol *1000;
		break;

	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int bq24157_charger_usb_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct bq24157_charger_info *info = power_supply_get_drvdata(psy);
	int ret;
	dev_err(info->dev, "%s;%d;\n",__func__,psp);

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = bq24157_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = bq24157_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_STATUS:
		ret = bq24157_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;

	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		ret = bq24157_charger_feed_watchdog(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "feed charger watchdog failed\n");
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = bq24157_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int bq24157_charger_property_is_writeable(struct power_supply *psy,
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

static enum power_supply_usb_type bq24157_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static enum power_supply_property bq24157_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_CHARGE_FULL,
};

static const struct power_supply_desc bq24157_charger_desc = {
	.name			= "charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= bq24157_usb_props,
	.num_properties		= ARRAY_SIZE(bq24157_usb_props),
	.get_property		= bq24157_charger_usb_get_property,
	.set_property		= bq24157_charger_usb_set_property,
	.property_is_writeable	= bq24157_charger_property_is_writeable,
	.usb_types		= bq24157_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(bq24157_charger_usb_types),
};

static void bq24157_charger_detect_status(struct bq24157_charger_info *info)
{
	int min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;

	usb_phy_get_charger_current(info->usb_phy, &min, &max);
	info->limit = min;
	schedule_work(&info->work);
}

static void
bq24157_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct bq24157_charger_info *info = container_of(dwork,
							  struct bq24157_charger_info,
							  wdt_work);

	bq24157_set_tmr_rst(info, 1);

	schedule_delayed_work(&info->wdt_work, HZ * 15);
}

static void bq24157_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct bq24157_charger_info *info = container_of(dwork,
			struct bq24157_charger_info, otg_work);

	if (!extcon_get_state(info->edev, EXTCON_USB)) {
		bq24157_set_opa_mode(info, 1);
		bq24157_set_otg_en(info, 1);
	}

	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(500));
}

static int bq24157_charger_enable_otg(struct regulator_dev *dev)
{
	struct bq24157_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	dev_err(info->dev, "%s;1\n",__func__);
	 
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
	if (ret) {
		dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
		return ret;
	}

	bq24157_set_opa_mode(info, 1);
	bq24157_set_otg_en(info,1);

	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(BQ24157_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(BQ24157_OTG_VALID_MS));

	return 0;
}

static int bq24157_charger_disable_otg(struct regulator_dev *dev)
{
	struct bq24157_charger_info *info = rdev_get_drvdata(dev);

	dev_err(info->dev, "%s;1\n",__func__);

	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	bq24157_set_opa_mode(info, 0);
	bq24157_set_otg_en(info, 0);

	/* Enable charger detection function to identify the charger type */
	return regmap_update_bits(info->pmic, info->charger_detect,
				  BIT_DP_DM_BC_ENB, 0);
}

static int bq24157_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct bq24157_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	ret = bq24157_read(info, BQ24157_CON1, &val);
	val = val >> CON1_OPA_MODE_SHIFT;
	val &= CON1_OPA_MODE_MASK;
	if (ret) {
		dev_err(info->dev, "failed to get bq24157 otg status\n");
		return ret;
	}

	return val;
}

static const struct regulator_ops bq24157_charger_vbus_ops = {
	.enable = bq24157_charger_enable_otg,
	.disable = bq24157_charger_disable_otg,
	.is_enabled = bq24157_charger_vbus_is_enabled,
};

static const struct regulator_desc bq24157_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &bq24157_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int
bq24157_charger_register_vbus_regulator(struct bq24157_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &bq24157_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

static int bq24157_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct bq24157_charger_info *info;
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

	bq24157_read(info,BQ24157_CON3, &val);
	if( val == 0x51)
//	       strncpy(charge_ic_vendor_name,"BQ24157",20);
	       strncpy(charge_ic_vendor_name,"SY6923",20);
	else if ( val == 0x41 )
       	strncpy(charge_ic_vendor_name,"HL7005",20);
	else if ( val == 0x54  )
	{
       	strncpy(charge_ic_vendor_name,"ETA6937",20);
		is_eta6937 = true;
	}
	else
		return -ENODEV;

	dev_err(dev, "%s;%s;\n",__func__,charge_ic_vendor_name);
	
	mutex_init(&info->lock);
	INIT_WORK(&info->work, bq24157_charger_work);

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

	ret = bq24157_charger_register_vbus_regulator(info);
	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
		return ret;
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
		info->charger_pd_mask = BQ24157_DISABLE_PIN_MASK_2730;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
		info->charger_pd_mask = BQ24157_DISABLE_PIN_MASK_2721;
	else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2720"))
		info->charger_pd_mask = BQ24157_DISABLE_PIN_MASK_2720;
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

	info->usb_notify.notifier_call = bq24157_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		return ret;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy_usb = devm_power_supply_register(dev,
						   &bq24157_charger_desc,
						   &charger_cfg);
	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return PTR_ERR(info->psy_usb);
	}

	ret = bq24157_charger_hw_init(info);
	if (ret) {
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return ret;
	}
	bq24157_charger_detect_status(info);
	INIT_DELAYED_WORK(&info->otg_work, bq24157_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work,
			  bq24157_charger_feed_watchdog_work);
	dev_err(dev, "bq24157 ok to register\n");

//+add by hzb for ontim debug
        REGISTER_AND_INIT_ONTIM_DEBUG_FOR_THIS_DEV();
//-add by hzb for ontim debug

	return 0;
}

static int bq24157_charger_remove(struct i2c_client *client)
{
	struct bq24157_charger_info *info = i2c_get_clientdata(client);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

static const struct i2c_device_id bq24157_i2c_id[] = {
	{"bq24157_chg", 0},
	{}
};

static const struct of_device_id bq24157_charger_of_match[] = {
	{ .compatible = "ti,bq24157_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, bq24157_charger_of_match);

static struct i2c_driver bq24157_charger_driver = {
	.driver = {
		.name = "bq24157_chg",
		.of_match_table = bq24157_charger_of_match,
	},
	.probe = bq24157_charger_probe,
	.remove = bq24157_charger_remove,
	.id_table = bq24157_i2c_id,
};

module_i2c_driver(bq24157_charger_driver);
MODULE_DESCRIPTION("BQ24157 Charger Driver");
MODULE_LICENSE("GPL v2");
