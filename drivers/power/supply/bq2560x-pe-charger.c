// SPDX-License-Identifier: GPL-2.0:
// Copyright (c) 2021 unisoc.

/*
 * Driver for the TI bq2560xpe charger.
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
#include <linux/power/sprd_battery_info.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/sysfs.h>
#include <linux/pm_wakeup.h>

#define BQ2560XPE_REG_0				0x0
#define BQ2560XPE_REG_1				0x1
#define BQ2560XPE_REG_2				0x2
#define BQ2560XPE_REG_3				0x3
#define BQ2560XPE_REG_4				0x4
#define BQ2560XPE_REG_5				0x5
#define BQ2560XPE_REG_6				0x6
#define BQ2560XPE_REG_7				0x7
#define BQ2560XPE_REG_8				0x8
#define BQ2560XPE_REG_9				0x9
#define BQ2560XPE_REG_A				0xa
#define BQ2560XPE_REG_B				0xb
#define BQ2560XPE_REG_NUM				12


#define BQ2560XPE_REG_OVP_MASK			GENMASK(7, 6)
#define BQ2560XPE_REG_OVP_SHIFT			6

#define BQ2560XPE_REG_IINLIM_BASE			100
#define BQ2560XPE_REG_LIMIT_CURRENT_MASK		GENMASK(4, 0)


#define BQ2560XPE_LIMIT_CURRENT_MAX		3200000
#define BQ2560XPE_LIMIT_CURRENT_OFFSET		100000

#define BQ2560XPE_WAKE_UP_MS			1000


#define BQ2560XPE_BATTERY_NAME			"sc27xx-fgu"
#define BQ2560XPE_CHARGER_NAME			"bq2560x_charger"
#define BQ2560XPE_FCHG_OVP_5V			5000
#define BQ2560XPE_FCHG_OVP_6V			6000
#define BQ2560XPE_FCHG_OVP_9V			9000
#define BQ2560XPE_FCHG_OVP_14V			14000
#define BQ2560XPE_FAST_CHARGER_VOLTAGE_MAX	10500000
#define BQ2560XPE_NORMAL_CHARGER_VOLTAGE_MAX	6500000

#define VBUS_9V 9000000
#define VBUS_7V 7000000
#define VBUS_5V 5000000
#define VBUS_1V 1000000
#define V_500MV 700000
#define I_3A 3000000
#define I_2A 2000000
#define I_500MA 500000
#define I_100MA 100000

enum chip_type{
	CHIP_NONE=0,
	CHIP_SGM41511,
	CHIP_SGM41542,
};


struct bq2560xpe_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *psy_usb;
	struct mutex lock;
	struct mutex pe_handshake_lock;
	struct delayed_work work;
	struct regmap *pmic;
	u32 last_limit_cur;
	u32 actual_limit_cur;
	bool shutdown_flag;

	u32 current_vbus;
	u32  set_vbus;
	struct completion completion;
	u32 state;
	u32 charger_online;
	bool detected;
	char charge_ic_vendor_name[50];
};


static int bq2560xpe_charger_get_limit_current(struct bq2560xpe_charger_info *info,
	                     u32 *limit_cur)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(BQ2560XPE_CHARGER_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of bq2560x_charger\n");
		return false;
	}

	ret = power_supply_get_property(psy,
					POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
					&val);
	power_supply_put(psy);
	if (ret == 0)
	{
		*limit_cur = val.intval;
		info->last_limit_cur = val.intval;
	}

	return ret;
}

static int bq2560xpe_charger_set_limit_current(struct bq2560xpe_charger_info *info,
					     u32 limit_cur)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(BQ2560XPE_CHARGER_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of bq2560x_charger\n");
		return false;
	}

	info->last_limit_cur = limit_cur;
	val.intval = limit_cur;
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
					&val);
	power_supply_put(psy);

	return ret;
}

static int bq2560xpe_hsphy_set_dpdm(struct bq2560xpe_charger_info *info ,  int on)
{
	int ret = 0;
	int cnt=5;
	u32 val;
	static int bc1p2_connetc=1;
	//#define SC2730_CHG_PD		0x19e8
	//#define SC2730_CHARGE_STATUS		0x1b9c
	//#define SC2730_CHG_DET_FGU_CTRL		0x1ba0
	//#define UMP9620_CHARGE_STATUS		0x239c
	//#define UMP9620_CHG_DET_FGU_CTRL	0x23a0
	#define CHARGE_PD		0x21e8
	#define CHG_DET_FGU_CTRL	0x23a0


	dev_err(info->dev, "%s;on=%d;connect=%d;\n",__func__,on,bc1p2_connetc);
	if(bc1p2_connetc == on)
		return 0;

	bc1p2_connetc = on ;
	
	if (on) {
			ret = regmap_update_bits(info->pmic, CHG_DET_FGU_CTRL,      //CHGR_DET_FGU_CTRL
				 1,
				 0);         //        to bc1.2 ,bc1.2 enable

			cnt=5;
			do {
				msleep(1);
				ret = regmap_read(info->pmic, CHG_DET_FGU_CTRL, &val);

				if ((val & 1) == 0) {
					break;
				}
				dev_err(info->dev, "%s;bc1.2 enable c=%d;\n",__func__,cnt);

				ret = regmap_update_bits(info->pmic, CHG_DET_FGU_CTRL,      //CHGR_DET_FGU_CTRL
					 1,
					 0);         //        to usb phy ,bc1.2 enable
			} while (--cnt > 0);


			ret = regmap_update_bits(info->pmic, CHARGE_PD,     // 0x1e8 vddusb33 power down
				 1,
				 1);         


	} else {


			ret = regmap_update_bits(info->pmic, CHARGE_PD,     // 0x1e8 vddusb33 power up
				 1,
				 0);         

			ret = regmap_update_bits(info->pmic, CHG_DET_FGU_CTRL,      //CHGR_DET_FGU_CTRL
				 1,
				 1);         //        to usb phy ,bc1.2 disble

			cnt=5;
			do {
				msleep(1);
				ret = regmap_read(info->pmic, CHG_DET_FGU_CTRL, &val);

				if (val & 1) {
					break;
				}
				dev_err(info->dev, "%s;bc1.2 disble c=%d;\n",__func__,cnt);

				ret = regmap_update_bits(info->pmic, CHG_DET_FGU_CTRL,      //CHGR_DET_FGU_CTRL
					 1,
					 1);         //        to usb phy ,bc1.2 disble
			} while (--cnt > 0);


	}

	return ret;
}

static int bq2560xpe_fgu_get_vbus(struct bq2560xpe_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(BQ2560XPE_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return false;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	power_supply_put(psy);

	return val.intval;
}
static int bq2560xped_set_ta_current_pattern(struct bq2560xpe_charger_info *info, bool is_increase)
{
	unsigned int status = 0;


	if(true == is_increase) {
		bq2560xpe_charger_set_limit_current(info,I_100MA); /* 100mA */
		msleep(85);

		bq2560xpe_charger_set_limit_current(info,I_500MA);; /* 500mA */
		//pr_err("[bq2560xped] mtk_ta_increase() on 1");
		msleep(85);

		bq2560xpe_charger_set_limit_current(info,I_100MA); /* 100mA */
		//pr_err("[bq2560xped] mtk_ta_increase() off 1");
		msleep(85);

		bq2560xpe_charger_set_limit_current(info,I_500MA);; /* 500mA */
		//pr_err("[bq2560xped] mtk_ta_increase() on 2");
		msleep(85);

		bq2560xpe_charger_set_limit_current(info,I_100MA); /* 100mA */
		//pr_err("[bq2560xped] mtk_ta_increase() off 2");
		msleep(85);

		bq2560xpe_charger_set_limit_current(info,I_500MA);; /* 500mA */
		//pr_err("[bq2560xped] mtk_ta_increase() on 3");
		msleep(281);

		bq2560xpe_charger_set_limit_current(info,I_100MA); /* 100mA */
		//pr_err("[bq2560xped] mtk_ta_increase() off 3");
		msleep(85);

		bq2560xpe_charger_set_limit_current(info,I_500MA);; /* 500mA */
		//pr_err("[bq2560xped] mtk_ta_increase() on 4");
		msleep(281);

		bq2560xpe_charger_set_limit_current(info,I_100MA); /* 100mA */
		//pr_err("[bq2560xped] mtk_ta_increase() off 4");
		msleep(85);

		bq2560xpe_charger_set_limit_current(info,I_500MA);; /* 500mA */
		//pr_err("[bq2560xped] mtk_ta_increase() on 5");
		msleep(281);

		bq2560xpe_charger_set_limit_current(info,I_100MA); /* 100mA */
		//pr_err("[bq2560xped] mtk_ta_increase() off 5");
		msleep(85);

		bq2560xpe_charger_set_limit_current(info,I_500MA);; /* 500mA */
		//pr_err("[bq2560xped] mtk_ta_increase() on 6");
		msleep(485);

		bq2560xpe_charger_set_limit_current(info,I_100MA); /* 100mA */
		//pr_err("[bq2560xped] mtk_ta_increase() off 6");
		msleep(50);

		pr_err("[bq2560xped] mtk_ta_increase() end\n");

		bq2560xpe_charger_set_limit_current(info,I_500MA);; /* 500mA */
		msleep(200);
	} else {
		bq2560xpe_charger_set_limit_current(info,I_100MA); /* 100mA */
		msleep(85);

		bq2560xpe_charger_set_limit_current(info,I_500MA);; /* 500mA */
		//pr_err("[bq2560xped] mtk_ta_decrease() on 1");
		msleep(281);

		bq2560xpe_charger_set_limit_current(info,I_100MA); /* 100mA */
		//pr_err("[bq2560xped] mtk_ta_decrease() off 1");
		msleep(85);

		bq2560xpe_charger_set_limit_current(info,I_500MA);; /* 500mA */
		//pr_err("[bq2560xped] mtk_ta_decrease() on 2");
		msleep(281);

		bq2560xpe_charger_set_limit_current(info,I_100MA); /* 100mA */
		//pr_err("[bq2560xped] mtk_ta_decrease() off 2");
		msleep(85);

		bq2560xpe_charger_set_limit_current(info,I_500MA);; /* 500mA */
		//pr_err("[bq2560xped] mtk_ta_decrease() on 3");
		msleep(281);

		bq2560xpe_charger_set_limit_current(info,I_100MA); /* 100mA */
		//pr_err("[bq2560xped] mtk_ta_decrease() off 3");
		msleep(85);

		bq2560xpe_charger_set_limit_current(info,I_500MA);; /* 500mA */
		//pr_err("[bq2560xped] mtk_ta_decrease() on 4");
		msleep(85);

		bq2560xpe_charger_set_limit_current(info,I_100MA); /* 100mA */
		//pr_err("[bq2560xped] mtk_ta_decrease() off 4");
		msleep(85);

		bq2560xpe_charger_set_limit_current(info,I_500MA);; /* 500mA */
		//pr_err("[bq2560xped] mtk_ta_decrease() on 5");
		msleep(85);

		bq2560xpe_charger_set_limit_current(info,I_100MA); /* 100mA */
		//pr_err("[bq2560xped] mtk_ta_decrease() off 5");
		msleep(85);

		bq2560xpe_charger_set_limit_current(info,I_500MA);; /* 500mA */
		//pr_err("[bq2560xped] mtk_ta_decrease() on 6");
		msleep(485);

		bq2560xpe_charger_set_limit_current(info,I_100MA); /* 100mA */
		//pr_err("[bq2560xped] mtk_ta_decrease() off 6");
		msleep(50);

		pr_err("[bq2560xped] mtk_ta_decrease() end\n");

		bq2560xpe_charger_set_limit_current(info,I_500MA);; /* 500mA */
	}

	return status;
}
static int bq2560xpe_first_check_pe(struct bq2560xpe_charger_info *info)
{
	int vol;
	int try_count=0;
	int last_limit_current;
	u32 vbus;
	

	bq2560xpe_hsphy_set_dpdm(info, 0);
	
//	bq2560xpe_charger_set_ovp(info, BQ2560XPE_FCHG_OVP_9V);
	bq2560xpe_charger_get_limit_current(info, &last_limit_current);


	vbus = VBUS_9V;
	bq2560xped_set_ta_current_pattern(info, true);  // up
	do{
		msleep(100);
		
		vol=bq2560xpe_fgu_get_vbus(info);
		dev_info(info->dev, "%s;%d;%d;\n",__func__,vol,try_count);

		if(vol < VBUS_1V)
			goto first_check_pe ;       //charger plug out
		 
		if(vol<vbus )
		{
			
			if((abs(vol -vbus)) < V_500MV)
			{
				info->state = POWER_SUPPLY_CHARGE_TYPE_FAST;
				info->current_vbus = vbus;
				break;
				//vbus = VBUS_5V;  //check pe ok ,and back to 5v
				//bq2560xped_set_ta_current_pattern(info, false);  //down
			}
			else
				bq2560xped_set_ta_current_pattern(info, true);  // up
			dev_info(info->dev, "%s;up; count %d;\n",__func__,try_count);
			
		}	
		else if(vol>vbus)	
		{
			if((abs(vol -vbus)) < V_500MV)
				break;
			bq2560xped_set_ta_current_pattern(info, false);  //down
			dev_info(info->dev, "%s;down; count %d;\n",__func__,try_count);
		}

		try_count++;


	}while((abs(vol-vbus))>V_500MV && try_count <5);

	dev_info(info->dev, "%s;count %d;%d;%d;\n",__func__,try_count,last_limit_current/1000,info->last_limit_cur/1000);

first_check_pe:
	if(info->last_limit_cur == I_500MA)
		bq2560xpe_charger_set_limit_current(info, last_limit_current);


	bq2560xpe_hsphy_set_dpdm(info, 1);

	return info->state;
		
}
static int bq2560xpe_set_pe(struct bq2560xpe_charger_info *info, u32 vbus)
{
	int vol;
	int try_count=0;
	int delta;
	int last_limit_current;

	bq2560xpe_charger_get_limit_current(info, &last_limit_current);

	dev_info(info->dev, "%s;cur=%d;vbus=%d;%d;\n",__func__,info->last_limit_cur/1000,vbus/1000,info->current_vbus/1000);

	if (vbus/1000 > BQ2560XPE_FCHG_OVP_5V)
		vbus = VBUS_9V;
	else 
		vbus = VBUS_5V;



	if( vbus == info->current_vbus )
		goto pe_exit_no_change ;


	vol=bq2560xpe_fgu_get_vbus(info);
	if((abs(vol -vbus)) < V_500MV)
		goto pe_exit_no_change ;

	bq2560xpe_hsphy_set_dpdm(info, 0);
	msleep(100);

//	bq2560xpe_charger_set_ovp(info, BQ2560XPE_FCHG_OVP_9V);

	if(vbus > info->current_vbus)
		bq2560xped_set_ta_current_pattern(info, true);  // up
	else	
		bq2560xped_set_ta_current_pattern(info, false);  //down

	do{
		msleep(100);
		
		vol=bq2560xpe_fgu_get_vbus(info);
		dev_info(info->dev, "%s;%d;%d;\n",__func__,vol,try_count);

		if(vol < VBUS_1V)
			goto pe_exit ;       //charger plug out
		 
		if(vol<vbus )
		{
			
			if((abs(vol -vbus)) < V_500MV)
				break;
			bq2560xped_set_ta_current_pattern(info, true);  // up
			dev_info(info->dev, "%s;up; count %d;\n",__func__,try_count);
			
		}	
		else if(vol>vbus)	
		{
			if((abs(vol -vbus)) < V_500MV)
				break;
			bq2560xped_set_ta_current_pattern(info, false);  //down
			dev_info(info->dev, "%s;down; count %d;\n",__func__,try_count);
		}

		try_count++;


	}while((abs(vol-vbus))>V_500MV && try_count <8);

	delta =abs(vol-vbus);

	if(delta < V_500MV  && vbus > VBUS_5V)
	{
		info->current_vbus = vbus;

	}
	else
	{
		info->current_vbus = VBUS_5V;

	}


	dev_info(info->dev, "%s;count %d;%d;%d;\n",__func__,try_count,last_limit_current/1000,info->last_limit_cur/1000);

pe_exit:

	if(info->last_limit_cur == I_500MA)
		bq2560xpe_charger_set_limit_current(info, last_limit_current);


	bq2560xpe_hsphy_set_dpdm(info, 1);


pe_exit_no_change:
	return 0;

}

static void bq2560xpe_work(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct bq2560xpe_charger_info *info = container_of(dwork, struct bq2560xpe_charger_info, work);

	mutex_lock(&info->pe_handshake_lock);
	if (!info->charger_online) {
		info->state = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		info->detected = false;
	} else if (!info->detected && !info->shutdown_flag) {
		info->detected = true;

		if (bq2560xpe_first_check_pe(info) == POWER_SUPPLY_CHARGE_TYPE_FAST) {
			/*
			 * Must release info->pe_handshake_lock before send fast charge event
			 * to charger manager, otherwise it will cause deadlock.
			 */
			mutex_unlock(&info->pe_handshake_lock);
			power_supply_changed(info->psy_usb);
			dev_info(info->dev, "pe_enable\n");
			return;
		}

	}

	mutex_unlock(&info->pe_handshake_lock);
}

static int bq2560xpe_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{
	struct bq2560xpe_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = info->state;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = VBUS_9V;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = I_3A;
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int bq2560xpe_charger_usb_set_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    const union power_supply_propval *val)
{
	struct bq2560xpe_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;


	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (val->intval == true) {
			info->charger_online = 1;
			schedule_delayed_work(&info->work, 0);
			break;
		} else if (val->intval == false) {
			info->charger_online = 0;
			cancel_delayed_work(&info->work);
			schedule_delayed_work(&info->work, 0);
			break;
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if(val->intval == BQ2560XPE_FCHG_OVP_5V)
		ret = bq2560xpe_set_pe(info, val->intval);
		if (ret)
			dev_err(info->dev, "failed to adjust pe vol\n");
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:

		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int bq2560xpe_charger_property_is_writeable(struct power_supply *psy,
						 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_property bq2560xpe_usb_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
};

static const struct power_supply_desc bq2560xpe_charger_desc = {
	.name			= "bq2560xpe_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= bq2560xpe_usb_props,
	.num_properties		= ARRAY_SIZE(bq2560xpe_usb_props),
	.get_property		= bq2560xpe_charger_usb_get_property,
	.set_property		= bq2560xpe_charger_usb_set_property,
	.property_is_writeable	= bq2560xpe_charger_property_is_writeable,
};

static int bq2560xpe_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct bq2560xpe_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret;
	struct power_supply *psy;
	union power_supply_propval val;

	psy = power_supply_get_by_name("bq2560x_charger");
		if (!psy) {
			dev_err(dev, "%s Cannot find power supply \"bq2560x_charger\"\n",__func__);
			return -EPROBE_DEFER;
		}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE,
					&val);

	power_supply_put(psy);

	if(val.intval != CHIP_SGM41511)
		return -ENODEV;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->client = client;
	info->dev = dev;

	
	i2c_set_clientdata(client, info);


	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np)
		regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");

	if (!regmap_np) {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
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
	mutex_init(&info->pe_handshake_lock);

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;

	info->psy_usb = devm_power_supply_register(dev,
						   &bq2560xpe_charger_desc,
						   &charger_cfg);

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto err_regmap_exit;
	}


	device_init_wakeup(info->dev, true);

	info->state = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

	INIT_DELAYED_WORK(&info->work, bq2560xpe_work);

	dev_err(info->dev, "%s;probe ok;\n",__func__);

	return 0;

err_regmap_exit:
	mutex_destroy(&info->lock);
	mutex_destroy(&info->pe_handshake_lock);
	return ret;
}



static int bq2560xpe_charger_remove(struct i2c_client *client)
{
	struct bq2560xpe_charger_info *info = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&info->work);
	return 0;
}

static void bq2560xpe_charger_shutdown(struct i2c_client *client)
{
	struct bq2560xpe_charger_info *info = i2c_get_clientdata(client);

	info->shutdown_flag = true;
	cancel_delayed_work_sync(&info->work);
}

static const struct i2c_device_id bq2560xpe_i2c_id[] = {
	{"bq2560xpe_chg", 0},
	{}
};

static const struct of_device_id bq2560xpe_charger_of_match[] = {
	{ .compatible = "ti,bq2560xpe_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, bq2560xpe_charger_of_match);

static struct i2c_driver bq2560xpe_charger_driver = {
	.driver = {
		.name = "bq2560xpe_chg",
		.of_match_table = bq2560xpe_charger_of_match,
	},
	.probe = bq2560xpe_charger_probe,
	.shutdown = bq2560xpe_charger_shutdown,
	.remove = bq2560xpe_charger_remove,
	.id_table = bq2560xpe_i2c_id,
};

module_i2c_driver(bq2560xpe_charger_driver);
MODULE_DESCRIPTION("BQ2560XPE Charger Driver");
MODULE_LICENSE("GPL v2");
