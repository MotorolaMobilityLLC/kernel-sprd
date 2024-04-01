// SPDX-License-Identifier: GPL-2.0:
// Copyright (c) 2021 unisoc.

/*
 * Driver for the TI bq2560xqc charger.
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
#include <linux/iio/consumer.h>

#define BQ2560XQC_REG_0				0x0
#define BQ2560XQC_REG_1				0x1
#define BQ2560XQC_REG_2				0x2
#define BQ2560XQC_REG_3				0x3
#define BQ2560XQC_REG_4				0x4
#define BQ2560XQC_REG_5				0x5
#define BQ2560XQC_REG_6				0x6
#define BQ2560XQC_REG_7				0x7
#define BQ2560XQC_REG_8				0x8
#define BQ2560XQC_REG_9				0x9
#define BQ2560XQC_REG_A				0xa
#define BQ2560XQC_REG_B				0xb
#define BQ2560XQC_REG_NUM				12


#define BQ2560XQC_REG_OVP_MASK			GENMASK(7, 6)
#define BQ2560XQC_REG_OVP_SHIFT			6

#define BQ2560XQC_REG_IINLIM_BASE			100
#define BQ2560XQC_REG_LIMIT_CURRENT_MASK		GENMASK(4, 0)


#define BQ2560XQC_LIMIT_CURRENT_MAX		3200000
#define BQ2560XQC_LIMIT_CURRENT_OFFSET		100000

#define BQ2560XQC_WAKE_UP_MS			1000


#define BQ2560XQC_BATTERY_NAME			"sc27xx-fgu"
#define BQ2560XQC_MAIN_NAME			"charger"
#define BQ2560XQC_CP_NAME			 "bq2597x-standalone"

#define VBUS_12V 12000000
#define VBUS_11V 11000000
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
	CHIP_SGM41542=2,
};

enum adjust_voltage_direct
{
	ADJUST_UP=0,
	ADJUST_DOWN,
};

struct bq2560xqc_charger_info {
	struct device *dev;
	struct power_supply *psy_usb;
	struct mutex lock;
	struct mutex qc_handshake_lock;
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
	struct power_supply *psy_bq2560x;
	struct power_supply *psy_fgu;
	unsigned int dpdm_gpio;
	struct power_supply *psy_cp;
	struct regulator	*vdd;

};

static int bq2560xqc_write(struct bq2560xqc_charger_info *info, int reg, int data)
{
	union power_supply_propval val;
	int ret;

	val.intval = (reg << 8) | data;

	ret = power_supply_set_property(info->psy_bq2560x, POWER_SUPPLY_PROP_TECHNOLOGY,
					&val);
	return ret;

}

static int bq2560xqc_charger_get_limit_current(struct bq2560xqc_charger_info *info,
	                     u32 *limit_cur)
{
	union power_supply_propval val;
	int ret;


	ret = power_supply_get_property(info->psy_bq2560x,
					POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
					&val);
	if (ret == 0)
	{
		*limit_cur = val.intval;
		info->last_limit_cur = val.intval;
	}

	return ret;
}
static int bq2560xqc_check_qc(struct bq2560xqc_charger_info *info)
{
//				io-channels = <&pmic_adc 30>, <&pmic_adc 31>;
//				io-channel-names = "dp", "dm";
	struct iio_channel	*dp;
	struct iio_channel	*dm;
	int dm_voltage=0, dp_voltage=0;
#define SC2730_CHARGE_DET_FGU_CTRL	0x3A0
//#define SC2730_ADC_OFFSET		0x1800
#define UMP9620_ADC_OFFSET		0x2000
#define BIT_DP_DM_AUX_EN		BIT(1)
#define BIT_DP_DM_BC_ENB		BIT(0)
#define VOLT_LO_LIMIT			1200
#define VOLT_HI_LIMIT			600

	dp = devm_iio_channel_get(info->dev, "dp");
	dm = devm_iio_channel_get(info->dev, "dm");
	if (!dm || !dp) {
		dev_err(info->dev, "%s; dp:%p, dm:%p\n",__func__,
			dp, dm);
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}

	regmap_update_bits(info->pmic,
		UMP9620_ADC_OFFSET | SC2730_CHARGE_DET_FGU_CTRL,
		BIT_DP_DM_AUX_EN | BIT_DP_DM_BC_ENB,
		BIT_DP_DM_AUX_EN);

	msleep(1500);
	iio_read_channel_processed(dp, &dp_voltage);
	iio_read_channel_processed(dm, &dm_voltage);

	dp_voltage = dp_voltage*15/10;
	dm_voltage = dm_voltage*15/10;
	
	dev_err(info->dev, "%s;%d;%d;\n",__func__,dp_voltage,dm_voltage);

	regmap_update_bits(info->pmic,
		UMP9620_ADC_OFFSET | SC2730_CHARGE_DET_FGU_CTRL,
		BIT_DP_DM_AUX_EN | BIT_DP_DM_BC_ENB, 0);

	if(dp_voltage>500 && dm_voltage<100)
		info->state = POWER_SUPPLY_CHARGE_TYPE_FAST;

	return info->state;
}

static int bq2560xqc_charger_set_limit_current(struct bq2560xqc_charger_info *info,
					     u32 limit_cur)
{
	union power_supply_propval val;
	int ret;


	info->last_limit_cur = limit_cur;
	val.intval = limit_cur;
	ret = power_supply_set_property(info->psy_bq2560x, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
					&val);

	return ret;
}

static int bq2560xqc_hsphy_set_dpdm(struct bq2560xqc_charger_info *info ,  int on)
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

			if (info->vdd)
				regulator_disable(info->vdd);

	} else {

			if (info->vdd)
				ret = regulator_enable(info->vdd);

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

static int bq2560xqc_fgu_get_vbus(struct bq2560xqc_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(BQ2560XQC_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return false;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	power_supply_put(psy);

	return val.intval;
}
static int bq2560xqc_cp_get_vbus(struct bq2560xqc_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(BQ2560XQC_CP_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return false;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	power_supply_put(psy);

	return val.intval;
}

// 0.6 0.6  12v
// 3.3 0.6  9v
// 0.6 3.3  contimuous mode
// 3.3 3.3   20v
// 0.6 hz   5v

static int bq2560xqc_set_qc(struct bq2560xqc_charger_info *info,int voltage)
{

	dev_info(info->dev, "%s;%d;%d;\n",__func__,info->current_vbus,voltage);

	info->current_vbus = voltage;

	if(info->current_vbus == VBUS_5V)
		bq2560xqc_write(info, 0x0d, 0x10);      //d+ 0.6
	else if(info->current_vbus == VBUS_9V)	
		bq2560xqc_write(info, 0x0d, 0x1c);      //d+ 3.3 , d- 0.6
	else if(info->current_vbus == VBUS_12V)	
		bq2560xqc_write(info, 0x0d, 0x14);      //d+ 0.6  d- 0.6

	return 0;	
}
static int bq2560xqc_set_qc_continue(struct bq2560xqc_charger_info *info,int step,int direction)
{
	int i;

	dev_info(info->dev, "%s;step:%d;direction=%d;\n",__func__,step,direction);
	if(step == 0)
		return 0;
	
	bq2560xqc_write(info, 0x0d, 0x15);      //d+ 0.6, d- 3.3  enter continue mode
	msleep(200);

	for(i=0;i< step;i++)
	{
		if(direction == ADJUST_UP)
		{
			bq2560xqc_write(info, 0x0d, 0x1e);      //d+ 3.3, d- 3.3  up
			msleep(5);
		}		
		else
		{
			bq2560xqc_write(info, 0x0d, 0x14);      //d+ 0.6, d- 0.6  down
			msleep(5);
		}
			
		bq2560xqc_write(info, 0x0d, 0x1e);      //d+ 0.6, d- 3.3 back to continue mode
		bq2560xqc_write(info, 0x0d, 0x1e);      //d+ 0.6, d- 3.3 back to continue mode
		msleep(5);
	}

	return 0;
}

static int bq2560xqc_fchg_adjust_voltage(struct bq2560xqc_charger_info *info, u32 input_vol)
{
	#define CM_CP_VSTEP 200000
	int vbus_step = 0, delta_vbus_uV;
	int fgu_vbus,cp_vbus;

	dev_info(info->dev, "%s;%d;%d;\n",__func__,info->current_vbus,input_vol);

	if(info->current_vbus == 0)
		info->current_vbus = VBUS_5V;

	fgu_vbus=bq2560xqc_fgu_get_vbus(info);
	cp_vbus=bq2560xqc_cp_get_vbus(info);

	dev_info(info->dev, "%s;fgu%d;cp%d;\n",__func__,fgu_vbus,cp_vbus);
	info->current_vbus = cp_vbus;

	if(input_vol > info->current_vbus)
	{
		delta_vbus_uV = input_vol - info->current_vbus;		
		vbus_step = delta_vbus_uV /CM_CP_VSTEP;
		bq2560xqc_set_qc_continue(info, vbus_step, ADJUST_UP);
	}
	else
	{
		delta_vbus_uV = info->current_vbus - input_vol;		
		vbus_step = delta_vbus_uV /CM_CP_VSTEP;
		bq2560xqc_set_qc_continue(info, vbus_step, ADJUST_DOWN);
	}

	fgu_vbus=bq2560xqc_fgu_get_vbus(info);
	cp_vbus=bq2560xqc_cp_get_vbus(info);

	if(0)//input_vol > 7000000  && cp_vbus <= 5000000)
	{
		dev_info(info->dev, "%s ;dp dm reset;\n",__func__);
		bq2560xqc_write(info, 0x0d, 0x00);      //d+ 0.0
		msleep(1000);
		bq2560xqc_write(info, 0x0d, 0x10);      //d+ 0.6
		msleep(1000);

	}

	dev_info(info->dev, "%s exit;fgu%d;cp%d;\n",__func__,fgu_vbus,cp_vbus);


	return 0;
}

static int bq2560xqc_first_check_qc(struct bq2560xqc_charger_info *info)
{
	int vol;
	int try_count=0;
	int last_limit_current;
	
	bq2560xqc_check_qc(info);
	return info->state;

	msleep(2500);
	
//	bq2560xqc_charger_set_ovp(info, BQ2560XQC_FCHG_OVP_9V);
	bq2560xqc_charger_get_limit_current(info, &last_limit_current);
	bq2560xqc_charger_set_limit_current(info, I_100MA);

	bq2560xqc_set_qc(info, VBUS_5V);
	msleep(2500);
	bq2560xqc_set_qc(info, VBUS_9V);
	
	do{
		msleep(2500);
		
		vol=bq2560xqc_fgu_get_vbus(info);
		dev_info(info->dev, "%s;%d;%d;\n",__func__,vol,try_count);

		if(vol < VBUS_1V)
			goto first_check_qc ;       //charger plug out
		 
			
		if((abs(vol -info->current_vbus)) < V_500MV)
		{
			info->state = POWER_SUPPLY_CHARGE_TYPE_FAST;
			 //check qc ok ,and back to 5v
			bq2560xqc_set_qc(info, VBUS_5V);  
		}
		dev_info(info->dev, "%s;up; count %d;\n",__func__,try_count);
			
		try_count++;

	}while((abs(vol-info->current_vbus))>V_500MV && try_count <5);

	dev_info(info->dev, "%s;count %d;%d;%d;\n",__func__,try_count,last_limit_current/1000,info->last_limit_cur/1000);

first_check_qc:
	if(info->last_limit_cur == I_100MA)
		bq2560xqc_charger_set_limit_current(info, last_limit_current);


	return info->state;
		
}

static void bq2560xqc_work(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct bq2560xqc_charger_info *info = container_of(dwork, struct bq2560xqc_charger_info, work);

	mutex_lock(&info->qc_handshake_lock);
	if (!info->charger_online) {
		info->state = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		info->detected = false;
	} else if (!info->detected && !info->shutdown_flag) {
		info->detected = true;

		if (bq2560xqc_first_check_qc(info) == POWER_SUPPLY_CHARGE_TYPE_FAST) {
			/*
			 * Must release info->qc_handshake_lock before send fast charge event
			 * to charger manager, otherwise it will cause deadlock.
			 */
			bq2560xqc_set_qc(info, VBUS_5V);
			gpio_direction_output(info->dpdm_gpio, 1); //analogy swtich
			bq2560xqc_hsphy_set_dpdm(info, 0); //pmic D+ D- high impedance
			 
			mutex_unlock(&info->qc_handshake_lock);
			power_supply_changed(info->psy_usb);
			dev_info(info->dev, "qc_enable\n");
			return;
		}

	}

	mutex_unlock(&info->qc_handshake_lock);
}

static int bq2560xqc_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{
	struct bq2560xqc_charger_info *info = power_supply_get_drvdata(psy);
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
		if(info->psy_cp)
			val->intval = VBUS_11V;
		else
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
static int bq2560xqc_charger_usb_set_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    const union power_supply_propval *val)
{
	struct bq2560xqc_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		info->psy_cp = power_supply_get_by_name(BQ2560XQC_CP_NAME);
		if (val->intval == true) {
			info->charger_online = 1;
			schedule_delayed_work(&info->work, 0);
			break;
		} else if (val->intval == false) {
			bq2560xqc_hsphy_set_dpdm(info, 1); //pimc D+ D- connect to bc1.2
			gpio_direction_output(info->dpdm_gpio, 0); //analogy swtich
			bq2560xqc_write(info, 0x0d, 0x00);      //sgm41542 d+ d-  high impedance
			info->charger_online = 0;
			info->current_vbus = 0;
			cancel_delayed_work(&info->work);
			schedule_delayed_work(&info->work, 0);
			break;
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:

		if(info->psy_cp)
		{
			ret = bq2560xqc_fchg_adjust_voltage(info, val->intval);
		}
		else
		{
			if(val->intval < VBUS_9V)
				ret = bq2560xqc_set_qc(info, VBUS_5V);
			else
				ret = bq2560xqc_set_qc(info, VBUS_9V);
		}
		if (ret)
			dev_err(info->dev, "failed to adjust qc vol\n");
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:

		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int bq2560xqc_charger_property_is_writeable(struct power_supply *psy,
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

static enum power_supply_property bq2560xqc_usb_props[] = {
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
};

static const struct power_supply_desc bq2560xqc_charger_desc = {
	.name			= "bq2560xqc_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= bq2560xqc_usb_props,
	.num_properties		= ARRAY_SIZE(bq2560xqc_usb_props),
	.get_property		= bq2560xqc_charger_usb_get_property,
	.set_property		= bq2560xqc_charger_usb_set_property,
	.property_is_writeable	= bq2560xqc_charger_property_is_writeable,
};

static int bq2560xqc_charger_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct power_supply_config charger_cfg = { };
	struct bq2560xqc_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret;
	struct power_supply *psy;
	union power_supply_propval val;

	psy = power_supply_get_by_name(BQ2560XQC_MAIN_NAME);
		if (!psy) {
			dev_err(dev, "%s Cannot find power supply \"bq2560x_charger\"\n",__func__);
			return -EPROBE_DEFER;
		}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE,
					&val);


	if(val.intval != CHIP_SGM41542)
	{
		dev_err(dev, "%s;%d;exit;\n",__func__,val.intval);
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;
	info->psy_bq2560x = psy;


	platform_set_drvdata(pdev, info);

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

	info->dpdm_gpio = of_get_named_gpio(info->dev->of_node, "dpdm-gpio", 0);
	if (gpio_is_valid(info->dpdm_gpio)) {
		ret = devm_gpio_request_one(info->dev, info->dpdm_gpio,
					    GPIOF_ACTIVE_LOW, "bq2560x_dpdm");
		if (ret)
			dev_err(dev, "dpdm-gpio request failed, ret = %x\n", ret);
	}
	gpio_direction_output(info->dpdm_gpio, 0); //analogy swtich


	mutex_init(&info->lock);
	mutex_init(&info->qc_handshake_lock);

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;

	info->psy_usb = devm_power_supply_register(dev,
						   &bq2560xqc_charger_desc,
						   &charger_cfg);

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto err_regmap_exit;
	}

	info->vdd = devm_regulator_get_optional(dev, "vdd");
	if (IS_ERR(info->vdd)) {
		dev_warn(dev, "unable to get ssphy vdd supply\n");
		info->vdd = NULL;
	}

	device_init_wakeup(info->dev, true);

	info->state = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;

	INIT_DELAYED_WORK(&info->work, bq2560xqc_work);

	dev_err(info->dev, "%s;probe ok;\n",__func__);

	return 0;

err_regmap_exit:
	mutex_destroy(&info->lock);
	mutex_destroy(&info->qc_handshake_lock);
	return ret;
}



static int bq2560xqc_charger_remove(struct platform_device *pdev)
{
	struct bq2560xqc_charger_info *info = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&info->work);
	return 0;
}

static void bq2560xqc_charger_shutdown(struct platform_device *pdev)
{
	struct bq2560xqc_charger_info *info = platform_get_drvdata(pdev);

	info->shutdown_flag = true;
	cancel_delayed_work_sync(&info->work);
}


static const struct of_device_id bq2560xqc_charger_of_match[] = {
	{ .compatible = "ti,bq2560xqc_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, bq2560xqc_charger_of_match);

static struct platform_driver bq2560xqc_charger_driver = {
	.driver = {
		.name = "bq2560xqc_chg",
		.of_match_table = bq2560xqc_charger_of_match,
	},
	.probe = bq2560xqc_charger_probe,
	.shutdown = bq2560xqc_charger_shutdown,
	.remove = bq2560xqc_charger_remove,
};

module_platform_driver(bq2560xqc_charger_driver);
MODULE_DESCRIPTION("BQ2560XQC Charger Driver");
MODULE_LICENSE("GPL v2");
