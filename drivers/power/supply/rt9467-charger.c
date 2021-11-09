/*
 * Driver for the TI rt9467 charger.
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
#include "rt9467.h"

#define RT9467_REG_0				0x0
#define RT9467_REG_1				0x1
#define RT9467_REG_2				0x2
#define RT9467_REG_3				0x3
#define RT9467_REG_4				0x4
#define RT9467_REG_5				0x5
#define RT9467_REG_6				0x6
#define RT9467_REG_7				0x7
#define RT9467_REG_8				0x8
#define RT9467_REG_9				0x9
#define RT9467_REG_A				0xa
#define RT9467_REG_B				0xb
#define RT9467_REG_NUM				12

#define RT9467_BATTERY_NAME			"sc27xx-fgu"
#define BIT_DP_DM_BC_ENB			BIT(0)
#define RT9467_OTG_ALARM_TIMER_MS		15000

#define	RT9467_REG_IINLIM_BASE			100

#define RT9467_REG_ICHG_LSB			60

#define RT9467_REG_ICHG_MASK			GENMASK(5, 0)

#define RT9467_REG_CHG_MASK			GENMASK(4, 4)
#define RT9467_REG_CHG_SHIFT			4

#define RT9467_REG_EN_TIMER_MASK	GENMASK(3, 3)


#define RT9467_REG_RESET_MASK			GENMASK(6, 6)

#define RT9467_REG_OTG_MASK			GENMASK(5, 5)
#define RT9467_REG_BOOST_FAULT_MASK		GENMASK(7, 5)

#define RT9467_REG_WATCHDOG_MASK		GENMASK(6, 6)

#define RT9467_REG_WATCHDOG_TIMER_MASK		GENMASK(5, 4)
#define RT9467_REG_WATCHDOG_TIMER_SHIFT	4

#define RT9467_REG_TERMINAL_VOLTAGE_MASK	GENMASK(7, 3)
#define RT9467_REG_TERMINAL_VOLTAGE_SHIFT	3

#define RT9467_REG_TERMINAL_CUR_MASK		GENMASK(3, 0)

#define RT9467_REG_VINDPM_VOLTAGE_MASK		GENMASK(3, 0)
#define RT9467_REG_OVP_MASK			GENMASK(7, 6)
#define RT9467_REG_OVP_SHIFT			6

#define RT9467_REG_EN_HIZ_MASK			GENMASK(7, 7)
#define RT9467_REG_EN_HIZ_SHIFT		7

#define RT9467_REG_LIMIT_CURRENT_MASK		GENMASK(4, 0)

#define RT9467_DISABLE_PIN_MASK		BIT(0)
#define RT9467_DISABLE_PIN_MASK_2721		BIT(15)

#define RT9467_OTG_VALID_MS			500
#define RT9467_FEED_WATCHDOG_VALID_MS		50
#define RT9467_OTG_RETRY_TIMES			10
#define RT9467_LIMIT_CURRENT_MAX		3200000
#define RT9467_LIMIT_CURRENT_OFFSET		100000
#define RT9467_REG_IINDPM_LSB			100

#define RT9467_ROLE_MASTER_DEFAULT		1
#define RT9467_ROLE_SLAVE			2

#define RT9467_FCHG_OVP_6V			6000
#define RT9467_FCHG_OVP_9V			9000
#define RT9467_FCHG_OVP_14V			14000
#define RT9467_FAST_CHARGER_VOLTAGE_MAX	10500000
#define RT9467_NORMAL_CHARGER_VOLTAGE_MAX	6500000

#define RT9467_WAKE_UP_MS			1000
#define RT9467_CURRENT_WORK_MS			msecs_to_jiffies(100)


#define I2C_ACCESS_MAX_RETRY	5
#define RT9467_DRV_VERSION	"1.0.19_MTK"

/* ======================= */
/* RT9467 Parameter        */
/* ======================= */

static const u32 rt9467_boost_oc_threshold[] = {
	500000, 700000, 1100000, 1300000, 1800000, 2100000, 2400000, 3000000,
}; /* uA */

static const u32 rt9467_safety_timer[] = {
	4, 6, 8, 10, 12, 14, 16, 20,
}; /* hour */

enum rt9467_irq_idx {
	RT9467_IRQIDX_CHG_STATC = 0,
	RT9467_IRQIDX_CHG_FAULT,
	RT9467_IRQIDX_TS_STATC,
	RT9467_IRQIDX_CHG_IRQ1,
	RT9467_IRQIDX_CHG_IRQ2,
	RT9467_IRQIDX_CHG_IRQ3,
	RT9467_IRQIDX_DPDM_IRQ,
	RT9467_IRQIDX_MAX,
};

enum rt9467_irq_stat {
	RT9467_IRQSTAT_CHG_STATC = 0,
	RT9467_IRQSTAT_CHG_FAULT,
	RT9467_IRQSTAT_TS_STATC,
	RT9467_IRQSTAT_MAX,
};

enum rt9467_chg_type {
	RT9467_CHG_TYPE_NOVBUS = 0,
	RT9467_CHG_TYPE_UNDER_GOING,
	RT9467_CHG_TYPE_SDP,
	RT9467_CHG_TYPE_SDPNSTD,
	RT9467_CHG_TYPE_DCP,
	RT9467_CHG_TYPE_CDP,
	RT9467_CHG_TYPE_MAX,
};

enum rt9467_usbsw_state {
	RT9467_USBSW_CHG = 0,
	RT9467_USBSW_USB,
};

static const u8 rt9467_irq_maskall[RT9467_IRQIDX_MAX] = {
	0xF0, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

struct irq_mapping_tbl {
	const char *name;
	const int id;
};

#define RT9467_IRQ_MAPPING(_name, _id) {.name = #_name, .id = _id}
static const struct irq_mapping_tbl rt9467_irq_mapping_tbl[] = {
	RT9467_IRQ_MAPPING(chg_treg, 4),
	RT9467_IRQ_MAPPING(chg_aicr, 5),
	RT9467_IRQ_MAPPING(chg_mivr, 6),
	RT9467_IRQ_MAPPING(pwr_rdy, 7),
	RT9467_IRQ_MAPPING(chg_vsysuv, 12),
	RT9467_IRQ_MAPPING(chg_vsysov, 13),
	RT9467_IRQ_MAPPING(chg_vbatov, 14),
	RT9467_IRQ_MAPPING(chg_vbusov, 15),
	RT9467_IRQ_MAPPING(ts_batcold, 20),
	RT9467_IRQ_MAPPING(ts_batcool, 21),
	RT9467_IRQ_MAPPING(ts_batwarm, 22),
	RT9467_IRQ_MAPPING(ts_bathot, 23),
	RT9467_IRQ_MAPPING(ts_statci, 24),
	RT9467_IRQ_MAPPING(chg_faulti, 25),
	RT9467_IRQ_MAPPING(chg_statci, 26),
	RT9467_IRQ_MAPPING(chg_tmri, 27),
	RT9467_IRQ_MAPPING(chg_batabsi, 28),
	RT9467_IRQ_MAPPING(chg_adpbadi, 29),
	RT9467_IRQ_MAPPING(chg_rvpi, 30),
	RT9467_IRQ_MAPPING(otpi, 31),
	RT9467_IRQ_MAPPING(chg_aiclmeasi, 32),
	RT9467_IRQ_MAPPING(chg_ichgmeasi, 33),
	RT9467_IRQ_MAPPING(chgdet_donei, 34),
	RT9467_IRQ_MAPPING(wdtmri, 35),
	RT9467_IRQ_MAPPING(ssfinishi, 36),
	RT9467_IRQ_MAPPING(chg_rechgi, 37),
	RT9467_IRQ_MAPPING(chg_termi, 38),
	RT9467_IRQ_MAPPING(chg_ieoci, 39),
	RT9467_IRQ_MAPPING(adc_donei, 40),
	RT9467_IRQ_MAPPING(pumpx_donei, 41),
	RT9467_IRQ_MAPPING(bst_batuvi, 45),
	RT9467_IRQ_MAPPING(bst_midovi, 46),
	RT9467_IRQ_MAPPING(bst_olpi, 47),
	RT9467_IRQ_MAPPING(attachi, 48),
	RT9467_IRQ_MAPPING(detachi, 49),
	RT9467_IRQ_MAPPING(chgdeti, 54),
	RT9467_IRQ_MAPPING(dcdti, 55),
};

enum rt9467_charging_status {
	RT9467_CHG_STATUS_READY = 0,
	RT9467_CHG_STATUS_PROGRESS,
	RT9467_CHG_STATUS_DONE,
	RT9467_CHG_STATUS_FAULT,
	RT9467_CHG_STATUS_MAX,
};

static const char *rt9467_chg_status_name[RT9467_CHG_STATUS_MAX] = {
	"ready", "progress", "done", "fault",
};

static const u8 rt9467_val_en_hidden_mode[] = {
	0x49, 0x32, 0xB6, 0x27, 0x48, 0x18, 0x03, 0xE2,
};

enum rt9467_iin_limit_sel {
	RT9467_IINLMTSEL_3_2A = 0,
	RT9467_IINLMTSEL_CHG_TYP,
	RT9467_IINLMTSEL_AICR,
	RT9467_IINLMTSEL_LOWER_LEVEL, /* lower of above three */
};

enum rt9467_adc_sel {
	RT9467_ADC_VBUS_DIV5 = 1,
	RT9467_ADC_VBUS_DIV2,
	RT9467_ADC_VSYS,
	RT9467_ADC_VBAT,
	RT9467_ADC_TS_BAT = 6,
	RT9467_ADC_IBUS = 8,
	RT9467_ADC_IBAT,
	RT9467_ADC_REGN = 11,
	RT9467_ADC_TEMP_JC,
	RT9467_ADC_MAX,
};

/*
 * Unit for each ADC parameter
 * 0 stands for reserved
 * For TS_BAT, the real unit is 0.25.
 * Here we use 25, please remember to divide 100 while showing the value
 */
static const int rt9467_adc_unit[RT9467_ADC_MAX] = {
	0,
	RT9467_ADC_UNIT_VBUS_DIV5,
	RT9467_ADC_UNIT_VBUS_DIV2,
	RT9467_ADC_UNIT_VSYS,
	RT9467_ADC_UNIT_VBAT,
	0,
	RT9467_ADC_UNIT_TS_BAT,
	0,
	RT9467_ADC_UNIT_IBUS,
	RT9467_ADC_UNIT_IBAT,
	0,
	RT9467_ADC_UNIT_REGN,
	RT9467_ADC_UNIT_TEMP_JC,
};

static const int rt9467_adc_offset[RT9467_ADC_MAX] = {
	0,
	RT9467_ADC_OFFSET_VBUS_DIV5,
	RT9467_ADC_OFFSET_VBUS_DIV2,
	RT9467_ADC_OFFSET_VSYS,
	RT9467_ADC_OFFSET_VBAT,
	0,
	RT9467_ADC_OFFSET_TS_BAT,
	0,
	RT9467_ADC_OFFSET_IBUS,
	RT9467_ADC_OFFSET_IBAT,
	0,
	RT9467_ADC_OFFSET_REGN,
	RT9467_ADC_OFFSET_TEMP_JC,
};

struct rt9467_desc {
	u32 ichg;	/* uA */
	u32 aicr;	/* uA */
	u32 mivr;	/* uV */
	u32 cv;		/* uV */
	u32 ieoc;	/* uA */
	u32 safety_timer;	/* hour */
	u32 ircmp_resistor;	/* uohm */
	u32 ircmp_vclamp;	/* uV */
	bool en_te;
	bool en_wdt;
	bool en_irq_pulse;
	bool en_jeita;
	bool en_chgdet;
	int regmap_represent_slave_addr;
	const char *regmap_name;
	const char *chg_dev_name;
	bool ceb_invert;
};

/* These default values will be applied if there's no property in dts */
static struct rt9467_desc rt9467_default_desc = {
	.ichg = 2000000,	/* uA */
	.aicr = 500000,		/* uA */
	.mivr = 4400000,	/* uV */
	.cv = 4350000,		/* uA */
	.ieoc = 250000,		/* uA */
	.safety_timer = 12,
#ifdef CONFIG_MTK_BIF_SUPPORT
	.ircmp_resistor = 0,		/* uohm */
	.ircmp_vclamp = 0,		/* uV */
#else
	.ircmp_resistor = 25000,	/* uohm */
	.ircmp_vclamp = 32000,		/* uV */
#endif /* CONFIG_MTK_BIF_SUPPORT */
	.en_te = true,
	.en_wdt = true,
	.en_irq_pulse = false,
	.en_jeita = false,
	.en_chgdet = true,
	.regmap_represent_slave_addr = RT9467_SLAVE_ADDR,
	.regmap_name = "rt9467",
	.chg_dev_name = "primary_chg",
	.ceb_invert = false,
};


/* ======================= */
/* Register Address        */
/* ======================= */

static const unsigned char rt9467_reg_addr[] = {
	RT9467_REG_CORE_CTRL0,
	RT9467_REG_CHG_CTRL1,
	RT9467_REG_CHG_CTRL2,
	RT9467_REG_CHG_CTRL3,
	RT9467_REG_CHG_CTRL4,
	RT9467_REG_CHG_CTRL5,
	RT9467_REG_CHG_CTRL6,
	RT9467_REG_CHG_CTRL7,
	RT9467_REG_CHG_CTRL8,
	RT9467_REG_CHG_CTRL9,
	RT9467_REG_CHG_CTRL10,
	RT9467_REG_CHG_CTRL11,
	RT9467_REG_CHG_CTRL12,
	RT9467_REG_CHG_CTRL13,
	RT9467_REG_CHG_CTRL14,
	RT9467_REG_CHG_CTRL15,
	RT9467_REG_CHG_CTRL16,
	RT9467_REG_CHG_ADC,
	RT9467_REG_CHG_DPDM1,
	RT9467_REG_CHG_DPDM2,
	RT9467_REG_CHG_DPDM3,
	RT9467_REG_CHG_CTRL19,
	RT9467_REG_CHG_CTRL17,
	RT9467_REG_CHG_CTRL18,
	RT9467_REG_DEVICE_ID,
	RT9467_REG_CHG_STAT,
	RT9467_REG_CHG_NTC,
	RT9467_REG_ADC_DATA_H,
	RT9467_REG_ADC_DATA_L,
	RT9467_REG_ADC_DATA_TUNE_H,
	RT9467_REG_ADC_DATA_TUNE_L,
	RT9467_REG_ADC_DATA_ORG_H,
	RT9467_REG_ADC_DATA_ORG_L,
	RT9467_REG_CHG_STATC,
	RT9467_REG_CHG_FAULT,
	RT9467_REG_TS_STATC,
	/* Skip IRQ evt to prevent reading clear while dumping registers */
	RT9467_REG_CHG_STATC_CTRL,
	RT9467_REG_CHG_FAULT_CTRL,
	RT9467_REG_TS_STATC_CTRL,
	RT9467_REG_CHG_IRQ1_CTRL,
	RT9467_REG_CHG_IRQ2_CTRL,
	RT9467_REG_CHG_IRQ3_CTRL,
	RT9467_REG_DPDM_IRQ_CTRL,
};




struct rt9467_charger_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_rt9467_dump_reg;
	struct device_attribute attr_rt9467_lookup_reg;
	struct device_attribute attr_rt9467_sel_reg_id;
	struct device_attribute attr_rt9467_reg_val;
	struct attribute *attrs[5];

	struct rt9467_info *info;
};

struct rt9467_info {
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
	struct rt9467_charger_sysfs *sysfs;
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



	struct mutex i2c_access_lock;
	struct mutex adc_access_lock;
	struct mutex irq_access_lock;
	struct mutex aicr_access_lock;
	struct mutex ichg_access_lock;
	struct mutex pe_access_lock;
	struct mutex hidden_mode_lock;
	struct mutex bc12_access_lock;
	struct mutex ieoc_lock;
	struct mutex tchg_lock;
	struct rt9467_desc *desc;
	struct power_supply *psy;
	wait_queue_head_t wait_queue;
	int irq;
	int aicr_limit;
	u32 intr_gpio;
	u32 ceb_gpio;
	u8 chip_rev;
	u8 irq_flag[RT9467_IRQIDX_MAX];
	u8 irq_stat[RT9467_IRQSTAT_MAX];
	u8 irq_mask[RT9467_IRQIDX_MAX];
	u32 hidden_mode_cnt;
	bool bc12_en;
	bool pwr_rdy;
	u32 ieoc;
	u32 ichg;
	bool ieoc_wkard;
	atomic_t bc12_sdp_cnt;
	atomic_t bc12_wkard;
	int tchg;

#ifdef CONFIG_TCPC_CLASS
	atomic_t tcpc_usb_connected;
#else
	struct work_struct chgdet_work;
#endif /* CONFIG_TCPC_CLASS */

#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_device *regmap_dev;
	struct rt_regmap_properties *regmap_prop;
#endif /* CONFIG_RT_REGMAP */
};

struct rt9467_charger_reg_tab {
	int id;
	u32 addr;
	char *name;
};

static struct rt9467_charger_reg_tab reg_tab[RT9467_REG_NUM + 1] = {
	{0, RT9467_REG_0, "EN_HIZ/EN_ICHG_MON/IINDPM"},
	{1, RT9467_REG_1, "PFM _DIS/WD_RST/OTG_CONFIG/CHG_CONFIG/SYS_Min/Min_VBAT_SEL"},
	{2, RT9467_REG_2, "BOOST_LIM/Q1_FULLON/ICHG"},
	{3, RT9467_REG_3, "IPRECHG/ITERM"},
	{4, RT9467_REG_4, "VREG/TOPOFF_TIMER/VRECHG"},
	{5, RT9467_REG_5, "EN_TERM/WATCHDOG/EN_TIMER/CHG_TIMER/TREG/JEITA_ISET"},
	{6, RT9467_REG_6, "OVP/BOOSTV/VINDPM"},
	{7, RT9467_REG_7, "IINDET_EN/TMR2X_EN/BATFET_DIS/JEITA_VSET/BATFET_DLY/"
				"BATFET_RST_EN/VDPM_BAT_TRACK"},
	{8, RT9467_REG_8, "VBUS_STAT/CHRG_STAT/PG_STAT/THERM_STAT/VSYS_STAT"},
	{9, RT9467_REG_9, "WATCHDOG_FAULT/BOOST_FAULT/CHRG_FAULT/BAT_FAULT/NTC_FAULT"},
	{10, RT9467_REG_A, "VBUS_GD/VINDPM_STAT/IINDPM_STAT/TOPOFF_ACTIVE/ACOV_STAT/"
				"VINDPM_INT_ MASK/IINDPM_INT_ MASK"},
	{11, RT9467_REG_B, "REG_RST/PN/DEV_REV"},
	{12, 0, "null"},
};

#include <ontim/ontim_dev_dgb.h>
static  char charge_ic_vendor_name[50]="RT9467";
DEV_ATTR_DECLARE(charge_ic)
DEV_ATTR_DEFINE("vendor",charge_ic_vendor_name)
DEV_ATTR_DECLARE_END;
ONTIM_DEBUG_DECLARE_AND_INIT(charge_ic,charge_ic,8);

static void power_path_control(struct rt9467_info *info)
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
rt9467_charger_set_limit_current(struct rt9467_info *info,
				  u32 limit_cur);

static bool rt9467_charger_is_bat_present(struct rt9467_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(RT9467_BATTERY_NAME);
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

static int rt9467_charger_is_fgu_present(struct rt9467_info *info)
{
	struct power_supply *psy;

	psy = power_supply_get_by_name(RT9467_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to find psy of sc27xx_fgu\n");
		return -ENODEV;
	}
	power_supply_put(psy);

	return 0;
}

/* ========================= */
/* I2C operations            */
/* ========================= */

static int rt9467_read(struct rt9467_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int rt9467_write(struct rt9467_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int rt9467_update_bits(struct rt9467_info *info, u8 reg,
			       u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = rt9467_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return rt9467_write(info, reg, v);
}
static int rt9467_device_read(void *client, u32 addr, int leng, void *dst)
{
	struct i2c_client *i2c = (struct i2c_client *)client;

	return i2c_smbus_read_i2c_block_data(i2c, addr, leng, dst);
}

static int rt9467_device_write(void *client, u32 addr, int leng,
	const void *src)
{
	struct i2c_client *i2c = (struct i2c_client *)client;

	return i2c_smbus_write_i2c_block_data(i2c, addr, leng, src);
}


static inline int __rt9467_i2c_write_byte(struct rt9467_info *info, u8 cmd,
	u8 data)
{
	int ret = 0, retry = 0;

	do {
		ret = rt9467_device_write(info->client, cmd, 1, &data);
		retry++;
		if (ret < 0)
			udelay(10);
	} while (ret < 0 && retry < I2C_ACCESS_MAX_RETRY);

	if (ret < 0)
		dev_notice(info->dev, "%s: I2CW[0x%02X] = 0x%02X fail\n",
			__func__, cmd, data);
	else
		dev_dbg(info->dev, "%s: I2CW[0x%02X] = 0x%02X\n", __func__,
			cmd, data);

	return ret;
}

static int rt9467_i2c_write_byte(struct rt9467_info *info, u8 cmd, u8 data)
{
	int ret = 0;

	mutex_lock(&info->i2c_access_lock);
	ret = __rt9467_i2c_write_byte(info, cmd, data);
	mutex_unlock(&info->i2c_access_lock);

	return ret;
}

static inline int __rt9467_i2c_read_byte(struct rt9467_info *info, u8 cmd)
{
	int ret = 0, ret_val = 0, retry = 0;

	do {
		ret = rt9467_device_read(info->client, cmd, 1, &ret_val);
		retry++;
		if (ret < 0)
			udelay(10);
	} while (ret < 0 && retry < I2C_ACCESS_MAX_RETRY);

	if (ret < 0) {
		dev_notice(info->dev, "%s: I2CR[0x%02X] fail\n", __func__, cmd);
		return ret;
	}

	ret_val = ret_val & 0xFF;

	dev_dbg(info->dev, "%s: I2CR[0x%02X] = 0x%02X\n", __func__, cmd,
		ret_val);

	return ret_val;
}

static int rt9467_i2c_read_byte(struct rt9467_info *info, u8 cmd)
{
	int ret = 0;

	mutex_lock(&info->i2c_access_lock);
	ret = __rt9467_i2c_read_byte(info, cmd);
	mutex_unlock(&info->i2c_access_lock);

	if (ret < 0)
		return ret;

	return (ret & 0xFF);
}

static inline int __rt9467_i2c_block_write(struct rt9467_info *info, u8 cmd,
	u32 leng, const u8 *data)
{
	int ret = 0;

	ret = rt9467_device_write(info->client, cmd, leng, data);

	return ret;
}


static int rt9467_i2c_block_write(struct rt9467_info *info, u8 cmd, u32 leng,
	const u8 *data)
{
	int ret = 0;

	mutex_lock(&info->i2c_access_lock);
	ret = __rt9467_i2c_block_write(info, cmd, leng, data);
	mutex_unlock(&info->i2c_access_lock);

	return ret;
}

static inline int __rt9467_i2c_block_read(struct rt9467_info *info, u8 cmd,
	u32 leng, u8 *data)
{
	int ret = 0;

	ret = rt9467_device_read(info->client, cmd, leng, data);

	return ret;
}


static int rt9467_i2c_block_read(struct rt9467_info *info, u8 cmd, u32 leng,
	u8 *data)
{
	int ret = 0;

	mutex_lock(&info->i2c_access_lock);
	ret = __rt9467_i2c_block_read(info, cmd, leng, data);
	mutex_unlock(&info->i2c_access_lock);

	return ret;
}


static int rt9467_i2c_test_bit(struct rt9467_info *info, u8 cmd, u8 shift,
	bool *is_one)
{
	int ret = 0;
	u8 data = 0;

	ret = rt9467_i2c_read_byte(info, cmd);
	if (ret < 0) {
		*is_one = false;
		return ret;
	}

	data = ret & (1 << shift);
	*is_one = (data == 0 ? false : true);

	return ret;
}

static int rt9467_i2c_update_bits(struct rt9467_info *info, u8 cmd, u8 data,
	u8 mask)
{
	int ret = 0;
	u8 reg_data = 0;

	mutex_lock(&info->i2c_access_lock);
	ret = __rt9467_i2c_read_byte(info, cmd);
	if (ret < 0) {
		mutex_unlock(&info->i2c_access_lock);
		return ret;
	}

	reg_data = ret & 0xFF;
	reg_data &= ~mask;
	reg_data |= (data & mask);

	ret = __rt9467_i2c_write_byte(info, cmd, reg_data);
	mutex_unlock(&info->i2c_access_lock);

	return ret;
}

static inline int rt9467_set_bit(struct rt9467_info *info, u8 reg, u8 mask)
{
	return rt9467_i2c_update_bits(info, reg, mask, mask);
}

static inline int rt9467_clr_bit(struct rt9467_info *info, u8 reg, u8 mask)
{
	return rt9467_i2c_update_bits(info, reg, 0x00, mask);
}

/* ================== */
/* Internal Functions */
/* ================== */
static int rt9467_enable_charging(struct rt9467_info *info, bool en);
static int __rt9467_get_mivr(struct rt9467_info *info, u32 *mivr);
static int rt9467_get_ieoc(struct rt9467_info *info, u32 *ieoc);
static int __rt9467_get_ichg(struct rt9467_info *info, u32 *ichg);
static int rt9467_enable_hidden_mode(struct rt9467_info *info, bool en);
static int __rt9467_get_mivr(struct rt9467_info *info, u32 *mivr);
#ifdef CODE_OLD
static int rt9467_get_mivr(struct charger_device *chg_dev, u32 *mivr);
static int rt9467_get_ichg(struct charger_device *chg_dev, u32 *ichg);
static int rt9467_set_aicr(struct charger_device *chg_dev, u32 aicr);
static int rt9467_set_ichg(struct charger_device *chg_dev, u32 aicr);
static int rt9467_kick_wdt(struct charger_device *chg_dev);
static int rt9467_enable_charging(struct charger_device *chg_dev, bool en);
#endif
static inline void rt9467_irq_set_flag(struct rt9467_info *info, u8 *irq,
	u8 mask)
{
	mutex_lock(&info->irq_access_lock);
	*irq |= mask;
	mutex_unlock(&info->irq_access_lock);
}

static inline void rt9467_irq_clr_flag(struct rt9467_info *info, u8 *irq,
	u8 mask)
{
	mutex_lock(&info->irq_access_lock);
	*irq &= ~mask;
	mutex_unlock(&info->irq_access_lock);
}

static inline const char *rt9467_get_irq_name(struct rt9467_info *info,
	int irqnum)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(rt9467_irq_mapping_tbl); i++) {
		if (rt9467_irq_mapping_tbl[i].id == irqnum)
			return rt9467_irq_mapping_tbl[i].name;
	}

	return "not found";
}

static inline void rt9467_irq_mask(struct rt9467_info *info, int irqnum)
{
	dev_dbg(info->dev, "%s: irq = %d, %s\n", __func__, irqnum,
		rt9467_get_irq_name(info, irqnum));
	info->irq_mask[irqnum / 8] |= (1 << (irqnum % 8));
}

static inline void rt9467_irq_unmask(struct rt9467_info *info, int irqnum)
{
	dev_dbg(info->dev, "%s: irq = %d, %s\n", __func__, irqnum,
		rt9467_get_irq_name(info, irqnum));
	info->irq_mask[irqnum / 8] &= ~(1 << (irqnum % 8));
}

static inline u8 rt9467_closest_reg(u32 min, u32 max, u32 step, u32 target)
{
	/* Smaller than minimum supported value, use minimum one */
	if (target < min)
		return 0;

	/* Greater than maximum supported value, use maximum one */
	if (target >= max)
		return (max - min) / step;

	return (target - min) / step;
}

static inline u8 rt9467_closest_reg_via_tbl(const u32 *tbl, u32 tbl_size,
	u32 target)
{
	u32 i = 0;

	/* Smaller than minimum supported value, use minimum one */
	if (target < tbl[0])
		return 0;

	for (i = 0; i < tbl_size - 1; i++) {
		if (target >= tbl[i] && target < tbl[i + 1])
			return i;
	}

	/* Greater than maximum supported value, use maximum one */
	return tbl_size - 1;
}

static inline u32 rt9467_closest_value(u32 min, u32 max, u32 step, u8 reg_val)
{
	u32 ret_val = 0;

	ret_val = min + reg_val * step;
	if (ret_val > max)
		ret_val = max;

	return ret_val;
}

static int rt9467_get_aicr(struct rt9467_info *info, u32 *aicr)
{
	int ret = 0;
	u8 reg_aicr = 0;

	ret = rt9467_i2c_read_byte(info, RT9467_REG_CHG_CTRL3);
	if (ret < 0)
		return ret;

	reg_aicr = (ret & RT9467_MASK_AICR) >> RT9467_SHIFT_AICR;
	*aicr = rt9467_closest_value(RT9467_AICR_MIN, RT9467_AICR_MAX,
		RT9467_AICR_STEP, reg_aicr);

	return ret;
}

static int rt9467_get_adc(struct rt9467_info *info,
	enum rt9467_adc_sel adc_sel, int *adc_val)
{
	int ret = 0, i = 0;
	const int max_wait_times = 6;
	u8 adc_data[6] = {0};
	u32 aicr = 0, ichg = 0;
	bool adc_start = false;

	mutex_lock(&info->adc_access_lock);

	rt9467_enable_hidden_mode(info, true);

	/* Select ADC to desired channel */
	ret = rt9467_i2c_update_bits(info, RT9467_REG_CHG_ADC,
		adc_sel << RT9467_SHIFT_ADC_IN_SEL, RT9467_MASK_ADC_IN_SEL);
	if (ret < 0) {
		dev_notice(info->dev, "%s: select ch to %d fail(%d)\n",
			__func__, adc_sel, ret);
		goto out;
	}

	/* Workaround for IBUS & IBAT */
	if (adc_sel == RT9467_ADC_IBUS) {
		mutex_lock(&info->aicr_access_lock);
		ret = rt9467_get_aicr(info, &aicr);
		if (ret < 0) {
			dev_notice(info->dev, "%s: get aicr fail\n", __func__);
			goto out_unlock_all;
		}
	} else if (adc_sel == RT9467_ADC_IBAT) {
		mutex_lock(&info->ichg_access_lock);
		ret = __rt9467_get_ichg(info, &ichg);
		if (ret < 0) {
			dev_notice(info->dev, "%s: get ichg fail\n", __func__);
			goto out_unlock_all;
		}
	}

	/* Start ADC conversation */
	ret = rt9467_set_bit(info, RT9467_REG_CHG_ADC, RT9467_MASK_ADC_START);
	if (ret < 0) {
		dev_notice(info->dev, "%s: start con fail(%d), sel = %d\n",
			__func__, ret, adc_sel);
		goto out_unlock_all;
	}

	for (i = 0; i < max_wait_times; i++) {
		msleep(35);
		ret = rt9467_i2c_test_bit(info, RT9467_REG_CHG_ADC,
			RT9467_SHIFT_ADC_START, &adc_start);
		if (ret >= 0 && !adc_start)
			break;
	}
	if (i == max_wait_times) {
		dev_notice(info->dev, "%s: wait con fail(%d), sel = %d\n",
			__func__, ret, adc_sel);
		ret = -EINVAL;
		goto out_unlock_all;
	}

	mdelay(1);

	/* Read ADC data high/low byte */
	ret = rt9467_i2c_block_read(info, RT9467_REG_ADC_DATA_H, 6, adc_data);
	if (ret < 0) {
		dev_notice(info->dev, "%s: read ADC data fail\n", __func__);
		goto out_unlock_all;
	}
	dev_dbg(info->dev,
		"%s: adc_tune = (0x%02X, 0x%02X), adc_org = (0x%02X, 0x%02X)\n",
		__func__, adc_data[2], adc_data[3], adc_data[4], adc_data[5]);

	/* Calculate ADC value */
	*adc_val = ((adc_data[0] << 8) + adc_data[1]) * rt9467_adc_unit[adc_sel]
		+ rt9467_adc_offset[adc_sel];

	dev_dbg(info->dev,
		"%s: adc_sel = %d, adc_h = 0x%02X, adc_l = 0x%02X, val = %d\n",
		__func__, adc_sel, adc_data[0], adc_data[1], *adc_val);

	ret = 0;

out_unlock_all:
	/* Coefficient of IBUS & IBAT */
	if (adc_sel == RT9467_ADC_IBUS) {
		if (aicr < 400000) /* 400mA */
			*adc_val = *adc_val * 67 / 100;
		mutex_unlock(&info->aicr_access_lock);
	} else if (adc_sel == RT9467_ADC_IBAT) {
		if (ichg >= 100000 && ichg <= 450000) /* 100~450mA */
			*adc_val = *adc_val * 57 / 100;
		else if (ichg >= 500000 && ichg <= 850000) /* 500~850mA */
			*adc_val = *adc_val * 63 / 100;
		mutex_unlock(&info->ichg_access_lock);
	}

out:
	rt9467_enable_hidden_mode(info, false);
	mutex_unlock(&info->adc_access_lock);
	return ret;
}
#ifdef CODE_OLD
static int rt9467_set_usbsw_state(struct rt9467_info *info, int state)
{
	dev_info(info->dev, "%s: state = %d\n", __func__, state);

	if (state == RT9467_USBSW_CHG)
		Charger_Detect_Init();
	else
		Charger_Detect_Release();

	return 0;
}
static inline int __rt9467_enable_chgdet_flow(struct rt9467_info *info, bool en)
{
	int ret = 0;
	enum rt9467_usbsw_state usbsw =
		en ? RT9467_USBSW_CHG : RT9467_USBSW_USB;

	dev_info(info->dev, "%s: en = %d\n", __func__, en);
	rt9467_set_usbsw_state(info, usbsw);
	ret = (en ? rt9467_set_bit : rt9467_clr_bit)
		(info, RT9467_REG_CHG_DPDM1, RT9467_MASK_USBCHGEN);
	if (ret >= 0)
		info->bc12_en = en;

	return ret;
}

static int rt9467_enable_chgdet_flow(struct rt9467_info *info, bool en)
{
	int ret = 0, i = 0;
	bool pwr_rdy = false;
	const int max_wait_cnt = 200;

	dev_info(info->dev, "%s: en = %d\n", __func__, en);

	if (en) {
		/* Workaround for CDP port */
		for (i = 0; i < max_wait_cnt; i++) {
			if (is_usb_rdy())
				break;
			dev_dbg(info->dev, "%s: CDP block\n", __func__);
			ret = rt9467_i2c_test_bit(info, RT9467_REG_CHG_STATC,
				RT9467_SHIFT_PWR_RDY, &pwr_rdy);
			if (ret >= 0 && !pwr_rdy) {
				dev_info(info->dev, "%s: plug out\n",
					__func__);
				return 0;
			}
			msleep(100);
		}
		if (i == max_wait_cnt)
			dev_notice(info->dev, "%s: CDP timeout\n", __func__);
		else
			dev_info(info->dev, "%s: CDP free\n", __func__);
	}

	mutex_lock(&info->bc12_access_lock);
	ret = __rt9467_enable_chgdet_flow(info, en);
	mutex_unlock(&info->bc12_access_lock);

	return ret;
}
static int rt9467_inform_psy_changed(struct rt9467_info *info)
{
	int ret = 0;
	union power_supply_propval propval;

	dev_info(info->dev, "%s: pwr_rdy = %d, type = %d\n", __func__,
		info->pwr_rdy, info->chg_type);

	/* Get chg type det power supply */
	info->psy = power_supply_get_by_name("charger");
	if (!info->psy) {
		dev_notice(info->dev, "%s: get power supply fail\n", __func__);
		return -EINVAL;
	}

	/* inform chg det power supply */
	propval.intval = info->pwr_rdy;
	ret = power_supply_set_property(info->psy, POWER_SUPPLY_PROP_ONLINE,
		&propval);
	if (ret < 0)
		dev_notice(info->dev, "%s: psy online fail(%d)\n", __func__,
			ret);

	propval.intval = info->chg_type;
	ret = power_supply_set_property(info->psy,
		POWER_SUPPLY_PROP_CHARGE_TYPE, &propval);
	if (ret < 0)
		dev_notice(info->dev, "%s: psy type fail(%d)\n", __func__, ret);

	return ret;
}
#endif
static inline int rt9467_enable_ilim(struct rt9467_info *info, bool en)
{
	dev_info(info->dev, "%s: en = %d\n", __func__, en);
	return (en ? rt9467_set_bit : rt9467_clr_bit)
		(info, RT9467_REG_CHG_CTRL3, RT9467_MASK_ILIM_EN);
}

static inline int rt9467_toggle_chgdet_flow(struct rt9467_info *info)
{
	int ret = 0;
	u8 data = 0, usbd_off[2] = {0}, usbd_on[2] = {0};
	struct i2c_client *client = info->client;
	struct i2c_msg msgs[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = usbd_off,
		},
		{
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = usbd_on,
		},
	};

	/* read data */
	ret = i2c_smbus_read_i2c_block_data(client, RT9467_REG_CHG_DPDM1,
		1, &data);
	if (ret < 0) {
		dev_notice(info->dev, "%s: read usbd fail\n", __func__);
		goto out;
	}

	/* usbd off and then on */
	usbd_off[0] = usbd_on[0] = RT9467_REG_CHG_DPDM1;
	usbd_off[1] = data & ~RT9467_MASK_USBCHGEN;
	usbd_on[1] = data | RT9467_MASK_USBCHGEN;
	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0)
		dev_notice(info->dev, "%s: usbd off/on fail(%d)\n",
				      __func__, ret);
out:

	return ret < 0 ? ret : 0;
}

#ifdef CODE_OLD
static int rt9467_bc12_sdp_workaround(struct rt9467_info *info)
{
	int ret = 0;

	dev_info(info->dev, "%s\n", __func__);

	mutex_lock(&info->i2c_access_lock);

	ret = rt9467_toggle_chgdet_flow(info);
	if (ret < 0)
		goto err;

	mdelay(10);

	ret = rt9467_toggle_chgdet_flow(info);
	if (ret < 0)
		goto err;

	goto out;
err:
	dev_notice(info->dev, "%s: fail\n", __func__);
out:
	mutex_unlock(&info->i2c_access_lock);
	return ret;
}
static int __rt9467_chgdet_handler(struct rt9467_info *info)
{
	int ret = 0;
	bool pwr_rdy = false, inform_psy = true;
	u8 usb_status = 0;

	dev_info(info->dev, "%s\n", __func__);

	/* disabled by user, do nothing */
	if (!info->desc->en_chgdet) {
		dev_info(info->dev, "%s: bc12 is disabled by dts\n", __func__);
		return 0;
	}

#ifdef CONFIG_TCPC_CLASS
	pwr_rdy = atomic_read(&info->tcpc_usb_connected);
#else
	/* check power ready */
	ret = rt9467_i2c_test_bit(info, RT9467_REG_CHG_STATC,
		RT9467_SHIFT_PWR_RDY, &pwr_rdy);
	if (ret < 0) {
		dev_notice(info->dev, "%s: read pwr rdy state fail\n",
			__func__);
		return ret;
	}
#endif

	/* no change in pwr_rdy state */
	if (info->pwr_rdy == pwr_rdy &&
		atomic_read(&info->bc12_wkard) == 0) {
		dev_info(info->dev, "%s: pwr_rdy(%d) state is the same\n",
			__func__, pwr_rdy);
		inform_psy = false;
		goto out;
	}
	info->pwr_rdy = pwr_rdy;

	/* plug out */
	if (!pwr_rdy) {
		info->chg_type = CHARGER_UNKNOWN;
		atomic_set(&info->bc12_sdp_cnt, 0);
		goto out;
	}

	/* plug in */
	ret = rt9467_i2c_read_byte(info, RT9467_REG_CHG_DPDM2);
	if (ret < 0) {
		dev_notice(info->dev, "%s: read type fail\n", __func__);
		return ret;
	}
	usb_status = (ret & RT9467_MASK_USB_STATUS) >> RT9467_SHIFT_USB_STATUS;

	switch (usb_status) {
	case RT9467_CHG_TYPE_UNDER_GOING:
		dev_info(info->dev, "%s: under going...\n", __func__);
		return ret;
	case RT9467_CHG_TYPE_SDP:
		info->chg_type = STANDARD_HOST;
		break;
	case RT9467_CHG_TYPE_SDPNSTD:
		info->chg_type = NONSTANDARD_CHARGER;
		break;
	case RT9467_CHG_TYPE_CDP:
		info->chg_type = CHARGING_HOST;
		break;
	case RT9467_CHG_TYPE_DCP:
		info->chg_type = STANDARD_CHARGER;
		break;
	default:
		info->chg_type = NONSTANDARD_CHARGER;
		break;
	}

	/* BC12 workaround (NONSTD -> STP) */
	if (atomic_read(&info->bc12_sdp_cnt) < 2 &&
		info->chg_type == STANDARD_HOST) {
		ret = rt9467_bc12_sdp_workaround(info);
		/* Workaround success, wait for next event */
		if (ret >= 0) {
			atomic_inc(&info->bc12_sdp_cnt);
			atomic_set(&info->bc12_wkard, 1);
			return ret;
		}
		goto out;
	}
out:
	atomic_set(&info->bc12_wkard, 0);

	if (info->chg_type != STANDARD_CHARGER) {
		/* turn off USB charger detection */
		ret = __rt9467_enable_chgdet_flow(info, false);
		if (ret < 0)
			dev_notice(info->dev, "%s: disable chrdet fail\n",
					      __func__);
	}

	if (inform_psy)
		rt9467_inform_psy_changed(info);

	return 0;
}

static int rt9467_chgdet_handler(struct rt9467_info *info)
{
	int ret = 0;

	mutex_lock(&info->bc12_access_lock);
	ret = __rt9467_chgdet_handler(info);
	mutex_unlock(&info->bc12_access_lock);

	return ret;
}
static int rt9467_set_aicl_vth(struct rt9467_info *info, u32 aicl_vth)
{
	u8 reg_aicl_vth = 0;

	reg_aicl_vth = rt9467_closest_reg(RT9467_AICL_VTH_MIN,
		RT9467_AICL_VTH_MAX, RT9467_AICL_VTH_STEP, aicl_vth);

	dev_info(info->dev, "%s: vth = %d(0x%02X)\n", __func__, aicl_vth,
		reg_aicl_vth);

	return rt9467_i2c_update_bits(info, RT9467_REG_CHG_CTRL14,
		reg_aicl_vth << RT9467_SHIFT_AICL_VTH, RT9467_MASK_AICL_VTH);
}
#endif

static int __rt9467_set_aicr(struct rt9467_info *info, u32 aicr)
{
	u8 reg_aicr = 0;

	reg_aicr = rt9467_closest_reg(RT9467_AICR_MIN, RT9467_AICR_MAX,
		RT9467_AICR_STEP, aicr);

	dev_info(info->dev, "%s: aicr = %d(0x%02X)\n", __func__, aicr,
		reg_aicr);

	return rt9467_i2c_update_bits(info, RT9467_REG_CHG_CTRL3,
		reg_aicr << RT9467_SHIFT_AICR, RT9467_MASK_AICR);
}

#ifdef OLD_CODE
static int __rt9467_run_aicl(struct rt9467_info *info)
{
	int ret = 0;
	u32 mivr = 0, aicl_vth = 0, aicr = 0;
	bool mivr_act = false;

	/* Check whether MIVR loop is active */
	ret = rt9467_i2c_test_bit(info, RT9467_REG_CHG_STATC,
		RT9467_SHIFT_CHG_MIVR, &mivr_act);
	if (ret < 0) {
		dev_notice(info->dev, "%s: read mivr stat fail\n", __func__);
		goto out;
	}

	if (!mivr_act) {
		dev_info(info->dev, "%s: mivr loop is not active\n", __func__);
		goto out;
	}

	ret = __rt9467_get_mivr(info, &mivr);
	if (ret < 0)
		goto out;

	/* Check if there's a suitable AICL_VTH */
	aicl_vth = mivr + 200000;
	if (aicl_vth > RT9467_AICL_VTH_MAX) {
		dev_notice(info->dev, "%s: no suitable vth, vth = %d\n",
			__func__, aicl_vth);
		ret = -EINVAL;
		goto out;
	}

	ret = rt9467_set_aicl_vth(info, aicl_vth);
	if (ret < 0)
		goto out;

	/* Clear AICL measurement IRQ */
	rt9467_irq_clr_flag(info, &info->irq_flag[RT9467_IRQIDX_CHG_IRQ2],
		RT9467_MASK_CHG_AICLMEASI);

	mutex_lock(&info->pe_access_lock);
	mutex_lock(&info->aicr_access_lock);

	ret = rt9467_set_bit(info, RT9467_REG_CHG_CTRL14,
		RT9467_MASK_AICL_MEAS);
	if (ret < 0)
		goto unlock_out;

	ret = wait_event_interruptible_timeout(info->wait_queue,
		info->irq_flag[RT9467_IRQIDX_CHG_IRQ2] &
		RT9467_MASK_CHG_AICLMEASI,
		msecs_to_jiffies(3500));
	if (ret <= 0) {
		dev_notice(info->dev, "%s: wait AICL time out\n", __func__);
		ret = -EIO;
		goto unlock_out;
	}

	ret = rt9467_get_aicr(info, &aicr);
	if (ret < 0)
		goto unlock_out;

	info->aicr_limit = aicr;
	dev_dbg(info->dev, "%s: OK, aicr upper bound = %dmA\n", __func__,
		aicr / 1000);

unlock_out:
	mutex_unlock(&info->aicr_access_lock);
	mutex_unlock(&info->pe_access_lock);
out:
	return ret;
}
#ifndef CONFIG_TCPC_CLASS
static void rt9467_chgdet_work_handler(struct work_struct *work)
{
	int ret = 0;
	bool pwr_rdy = false;
	struct rt9467_info *info = (struct rt9467_info *)container_of(work,
		struct rt9467_info, chgdet_work);

	/* Check power ready */
	ret = rt9467_i2c_test_bit(info, RT9467_REG_CHG_STATC,
		RT9467_SHIFT_PWR_RDY, &pwr_rdy);

	if (!pwr_rdy)
		return;

	/* Enable USB charger type detection */
	ret = rt9467_enable_chgdet_flow(info, true);
	if (ret < 0)
		dev_notice(info->dev, "%s: enable usb chrdet fail\n", __func__);

}
#endif /* CONFIG_TCPC_CLASS */

/* Prevent back boost */
static int rt9467_toggle_cfo(struct rt9467_info *info)
{
	int ret = 0;
	u8 data = 0;

	dev_info(info->dev, "%s\n", __func__);
	mutex_lock(&info->i2c_access_lock);
	ret = rt9467_device_read(info->client, RT9467_REG_CHG_CTRL2, 1, &data);
	if (ret < 0) {
		dev_notice(info->dev, "%s read cfo fail(%d)\n", __func__, ret);
		goto out;
	}

	/* CFO off */
	data &= ~RT9467_MASK_CFO_EN;
	ret = rt9467_device_write(info->client, RT9467_REG_CHG_CTRL2, 1, &data);
	if (ret < 0) {
		dev_notice(info->dev, "%s cfo off fail(%d)\n", __func__, ret);
		goto out;
	}

	/* CFO on */
	data |= RT9467_MASK_CFO_EN;
	ret = rt9467_device_write(info->client, RT9467_REG_CHG_CTRL2, 1, &data);
	if (ret < 0)
		dev_notice(info->dev, "%s cfo on fail(%d)\n", __func__, ret);

out:
	mutex_unlock(&info->i2c_access_lock);
	return ret;
}

/* IRQ handlers */
static int rt9467_pwr_rdy_irq_handler(struct rt9467_info *info)
{
#ifndef CONFIG_TCPC_CLASS
	int ret = 0;
	bool pwr_rdy = false;
#endif /* CONFIG_TCPC_CLASS */

	dev_notice(info->dev, "%s\n", __func__);

#ifndef CONFIG_TCPC_CLASS
	ret = rt9467_i2c_test_bit(info, RT9467_REG_CHG_STATC,
		RT9467_SHIFT_PWR_RDY, &pwr_rdy);
	if (ret < 0) {
		dev_notice(info->dev, "%s: read pwr rdy fail\n", __func__);
		goto out;
	}

	if (!pwr_rdy) {
		dev_info(info->dev, "%s: pwr rdy = 0\n", __func__);
		goto out;
	}

	ret = rt9467_enable_chgdet_flow(info, true);
	if (ret < 0)
		dev_notice(info->dev, "%s: en chgdet fail(%d)\n", __func__,
			ret);

out:
#endif /* CONFIG_TCPC_CLASS */

	return 0;
}

static int rt9467_chg_mivr_irq_handler(struct rt9467_info *info)
{
	int ret = 0;
	bool mivr_act = false;
	int adc_ibus = 0;

	dev_notice(info->dev, "%s\n", __func__);

	/* Check whether MIVR loop is active */
	ret = rt9467_i2c_test_bit(info, RT9467_REG_CHG_STATC,
		RT9467_SHIFT_CHG_MIVR, &mivr_act);
	if (ret < 0) {
		dev_notice(info->dev, "%s: read mivr stat fail\n", __func__);
		goto out;
	}

	if (!mivr_act) {
		dev_info(info->dev, "%s: mivr loop is not active\n", __func__);
		goto out;
	}

	if (strcmp(info->desc->chg_dev_name, "primary_chg") == 0) {
		/* Check IBUS ADC */
		ret = rt9467_get_adc(info, RT9467_ADC_IBUS, &adc_ibus);
		if (ret < 0) {
			dev_notice(info->dev, "%s: get ibus fail\n", __func__);
			return ret;
		}
		if (adc_ibus < 100000) { /* 100mA */
			ret = rt9467_toggle_cfo(info);
			return ret;
		}
	}
out:
	return 0;
}

static int rt9467_chg_aicr_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_chg_treg_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_chg_vsysuv_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_chg_vsysov_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_chg_vbatov_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_chg_vbusov_irq_handler(struct rt9467_info *info)
{
	int ret = 0;
	bool vbusov = false;
	struct chgdev_notify *noti = &(info->chg_dev->noti);

	dev_notice(info->dev, "%s\n", __func__);
	ret = rt9467_i2c_test_bit(info, RT9467_REG_CHG_FAULT,
		RT9467_SHIFT_VBUSOV, &vbusov);
	if (ret < 0)
		return ret;

	noti->vbusov_stat = vbusov;
	dev_info(info->dev, "%s: vbusov = %d\n", __func__, vbusov);
	charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_VBUS_OVP);

	return 0;
}

static int rt9467_ts_bat_cold_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_ts_bat_cool_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_ts_bat_warm_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_ts_bat_hot_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_ts_statci_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_chg_faulti_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_chg_statci_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_chg_tmri_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT);
	return 0;
}

static int rt9467_chg_batabsi_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_chg_adpbadi_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_chg_rvpi_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_chg_otpi_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_chg_aiclmeasi_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	rt9467_irq_set_flag(info, &info->irq_flag[RT9467_IRQIDX_CHG_IRQ2],
		RT9467_MASK_CHG_AICLMEASI);
	wake_up_interruptible(&info->wait_queue);
	return 0;
}

static int rt9467_chg_ichgmeasi_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_chgdet_donei_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_wdtmri_irq_handler(struct rt9467_info *info)
{
	int ret = 0;

	dev_notice(info->dev, "%s\n", __func__);
	ret = rt9467_kick_wdt(info->chg_dev);
	if (ret < 0)
		dev_notice(info->dev, "%s: kick wdt fail\n", __func__);

	return ret;
}

static int rt9467_ssfinishi_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_chg_rechgi_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_RECHG);
	return 0;
}

static int rt9467_chg_termi_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_chg_ieoci_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_EOC);
	return 0;
}

static int rt9467_adc_donei_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_pumpx_donei_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_bst_batuvi_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_bst_midovi_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_bst_olpi_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_attachi_irq_handler(struct rt9467_info *info)
{
	int ret = 0;

	dev_notice(info->dev, "%s\n", __func__);

	/* check bc12_en state */
	mutex_lock(&info->bc12_access_lock);
	if (!info->bc12_en) {
		dev_notice(info->dev, "%s: bc12 disabled, ignore irq\n",
			__func__);
		goto out;
	}
	ret = __rt9467_chgdet_handler(info);
out:
	mutex_unlock(&info->bc12_access_lock);
	return ret;
}

static int rt9467_detachi_irq_handler(struct rt9467_info *info)
{
	int ret = 0;

	dev_notice(info->dev, "%s\n", __func__);
#ifndef CONFIG_TCPC_CLASS
	ret = rt9467_chgdet_handler(info);
#endif /* CONFIG_TCPC_CLASS */
	return ret;
}

static int rt9467_chgdeti_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

static int rt9467_dcdti_irq_handler(struct rt9467_info *info)
{
	dev_notice(info->dev, "%s\n", __func__);
	return 0;
}

typedef int (*rt9467_irq_fptr)(struct rt9467_info *);
static rt9467_irq_fptr rt9467_irq_handler_tbl[56] = {
	NULL,
	NULL,
	NULL,
	NULL,
	rt9467_chg_treg_irq_handler,
	rt9467_chg_aicr_irq_handler,
	rt9467_chg_mivr_irq_handler,
	rt9467_pwr_rdy_irq_handler,
	NULL,
	NULL,
	NULL,
	NULL,
	rt9467_chg_vsysuv_irq_handler,
	rt9467_chg_vsysov_irq_handler,
	rt9467_chg_vbatov_irq_handler,
	rt9467_chg_vbusov_irq_handler,
	NULL,
	NULL,
	NULL,
	NULL,
	rt9467_ts_bat_cold_irq_handler,
	rt9467_ts_bat_cool_irq_handler,
	rt9467_ts_bat_warm_irq_handler,
	rt9467_ts_bat_hot_irq_handler,
	rt9467_ts_statci_irq_handler,
	rt9467_chg_faulti_irq_handler,
	rt9467_chg_statci_irq_handler,
	rt9467_chg_tmri_irq_handler,
	rt9467_chg_batabsi_irq_handler,
	rt9467_chg_adpbadi_irq_handler,
	rt9467_chg_rvpi_irq_handler,
	rt9467_chg_otpi_irq_handler,
	rt9467_chg_aiclmeasi_irq_handler,
	rt9467_chg_ichgmeasi_irq_handler,
	rt9467_chgdet_donei_irq_handler,
	rt9467_wdtmri_irq_handler,
	rt9467_ssfinishi_irq_handler,
	rt9467_chg_rechgi_irq_handler,
	rt9467_chg_termi_irq_handler,
	rt9467_chg_ieoci_irq_handler,
	rt9467_adc_donei_irq_handler,
	rt9467_pumpx_donei_irq_handler,
	NULL,
	NULL,
	NULL,
	rt9467_bst_batuvi_irq_handler,
	rt9467_bst_midovi_irq_handler,
	rt9467_bst_olpi_irq_handler,
	rt9467_attachi_irq_handler,
	rt9467_detachi_irq_handler,
	NULL,
	NULL,
	NULL,
	rt9467_chgdeti_irq_handler,
	rt9467_dcdti_irq_handler,
};

static inline int rt9467_enable_irqrez(struct rt9467_info *info, bool en)
{
	dev_info(info->dev, "%s: en = %d\n", __func__, en);
	return (en ? rt9467_set_bit : rt9467_clr_bit)
		(info, RT9467_REG_CHG_CTRL13, RT9467_MASK_IRQ_REZ);
}

static int __rt9467_irq_handler(struct rt9467_info *info)
{
	int ret = 0, i = 0, j = 0;
	u8 evt[RT9467_IRQIDX_MAX] = {0};
	u8 mask[RT9467_IRQIDX_MAX] = {0};
	u8 stat[RT9467_IRQSTAT_MAX] = {0};
	u8 usb_status_old = 0, usb_status_new = 0;

	dev_info(info->dev, "%s\n", __func__);

	/* Read DPDM status before reading evts */
	ret = rt9467_i2c_read_byte(info, RT9467_REG_CHG_DPDM2);
	if (ret < 0) {
		dev_notice(info->dev, "%s: read type fail\n", __func__);
		goto err_read_irq;
	}
	usb_status_old = (ret & RT9467_MASK_USB_STATUS) >>
		RT9467_SHIFT_USB_STATUS;

	/* Read event and skip CHG_IRQ3 */
	ret = rt9467_i2c_block_read(info, RT9467_REG_CHG_IRQ1, 2, &evt[3]);
	if (ret < 0) {
		dev_notice(info->dev, "%s: read evt1 fail(%d)\n", __func__,
			ret);
		goto err_read_irq;
	}

	ret = rt9467_i2c_block_read(info, RT9467_REG_DPDM_IRQ, 1, &evt[6]);
	if (ret < 0) {
		dev_notice(info->dev, "%s: read evt2 fail(%d)\n", __func__,
			ret);
		goto err_read_irq;
	}

	ret = rt9467_i2c_block_read(info, RT9467_REG_CHG_STATC, 3, evt);
	if (ret < 0) {
		dev_notice(info->dev, "%s: read stat fail(%d)\n", __func__,
			ret);
		goto err_read_irq;
	}

	/* Read DPDM status after reading evts */
	ret = rt9467_i2c_read_byte(info, RT9467_REG_CHG_DPDM2);
	if (ret < 0) {
		dev_notice(info->dev, "%s: read type fail\n", __func__);
		goto err_read_irq;
	}
	usb_status_new = (ret & RT9467_MASK_USB_STATUS) >>
		RT9467_SHIFT_USB_STATUS;

	/* Read mask */
	ret = rt9467_i2c_block_read(info, RT9467_REG_CHG_STATC_CTRL,
		ARRAY_SIZE(mask), mask);
	if (ret < 0) {
		dev_notice(info->dev, "%s: read mask fail(%d)\n", __func__,
			ret);
		goto err_read_irq;
	}

	/* Detach */
	if (usb_status_old != RT9467_CHG_TYPE_NOVBUS &&
		usb_status_new == RT9467_CHG_TYPE_NOVBUS)
		evt[RT9467_IRQIDX_DPDM_IRQ] |= 0x02;

	/* Attach */
	if (usb_status_new >= RT9467_CHG_TYPE_SDP &&
		usb_status_new <= RT9467_CHG_TYPE_CDP &&
		usb_status_old != usb_status_new)
		evt[RT9467_IRQIDX_DPDM_IRQ] |= 0x01;

	/* Store/Update stat */
	memcpy(stat, info->irq_stat, RT9467_IRQSTAT_MAX);

	for (i = 0; i < RT9467_IRQIDX_MAX; i++) {
		evt[i] &= ~mask[i];
		if (i < RT9467_IRQSTAT_MAX) {
			info->irq_stat[i] = evt[i];
			evt[i] ^= stat[i];
		}
		for (j = 0; j < 8; j++) {
			if (!(evt[i] & (1 << j)))
				continue;
			if (rt9467_irq_handler_tbl[i * 8 + j])
				rt9467_irq_handler_tbl[i * 8 + j](info);
		}
	}

err_read_irq:
	return ret;
}

static irqreturn_t rt9467_irq_handler(int irq, void *data)
{
	int ret = 0;
	struct rt9467_info *info = (struct rt9467_info *)data;

	dev_info(info->dev, "%s\n", __func__);

	ret = __rt9467_irq_handler(info);
	ret = rt9467_enable_irqrez(info, true);
	if (ret < 0)
		dev_notice(info->dev, "%s: en irqrez fail\n", __func__);

	return IRQ_HANDLED;
}

static int rt9467_irq_register(struct rt9467_info *info)
{
	int ret = 0, len = 0;
	char *name = NULL;

	if (strcmp(info->desc->chg_dev_name, "secondary_chg") == 0)
		return 0;

	dev_info(info->dev, "%s\n", __func__);

	/* request gpio */
	len = strlen(info->desc->chg_dev_name);
	name = devm_kzalloc(info->dev, len + 10, GFP_KERNEL);
	snprintf(name,  len + 10, "%s_irq_gpio", info->desc->chg_dev_name);
	ret = devm_gpio_request_one(info->dev, info->intr_gpio, GPIOF_IN, name);
	if (ret < 0) {
		dev_notice(info->dev, "%s: gpio request fail\n", __func__);
		return ret;
	}

	ret = gpio_to_irq(info->intr_gpio);
	if (ret < 0) {
		dev_notice(info->dev, "%s: irq mapping fail\n", __func__);
		return ret;
	}
	info->irq = ret;
	dev_info(info->dev, "%s: irq = %d\n", __func__, info->irq);

	/* Request threaded IRQ */
	name = devm_kzalloc(info->dev, len + 5, GFP_KERNEL);
	snprintf(name, len + 5, "%s_irq", info->desc->chg_dev_name);
	ret = devm_request_threaded_irq(info->dev, info->irq, NULL,
		rt9467_irq_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, name,
		info);
	if (ret < 0) {
		dev_notice(info->dev, "%s: request thread irq fail\n",
			__func__);
		return ret;
	}
	device_init_wakeup(info->dev, true);

	return 0;
}


static inline int rt9467_irq_init(struct rt9467_info *info)
{
	dev_info(info->dev, "%s\n", __func__);
	return rt9467_i2c_block_write(info, RT9467_REG_CHG_STATC_CTRL,
		ARRAY_SIZE(info->irq_mask), info->irq_mask);
}

static bool rt9467_is_hw_exist(struct rt9467_info *info)
{
	int ret = 0;
	u8 vendor_id = 0, chip_rev = 0;

	ret = i2c_smbus_read_byte_data(info->client, RT9467_REG_DEVICE_ID);
	if (ret < 0)
		return false;

	vendor_id = ret & 0xF0;
	chip_rev = ret & 0x0F;
	if (vendor_id != RT9467_VENDOR_ID) {
		dev_notice(info->dev, "%s: vendor id is incorrect (0x%02X)\n",
			__func__, vendor_id);
		return false;
	}

	dev_info(info->dev, "%s: 0x%02X\n", __func__, chip_rev);
	info->chip_rev = chip_rev;

	return true;
}
#endif

static inline int rt9467_maskall_irq(struct rt9467_info *info)
{
	dev_info(info->dev, "%s\n", __func__);
	return rt9467_i2c_block_write(info, RT9467_REG_CHG_STATC_CTRL,
		ARRAY_SIZE(rt9467_irq_maskall), rt9467_irq_maskall);
}

static int rt9467_set_safety_timer(struct rt9467_info *info, u32 hr)
{
	u8 reg_st = 0;

	reg_st = rt9467_closest_reg_via_tbl(rt9467_safety_timer,
		ARRAY_SIZE(rt9467_safety_timer), hr);

	dev_info(info->dev, "%s: time = %d(0x%02X)\n", __func__, hr, reg_st);

	return rt9467_i2c_update_bits(info, RT9467_REG_CHG_CTRL12,
		reg_st << RT9467_SHIFT_WT_FC, RT9467_MASK_WT_FC);
}

static inline int rt9467_enable_wdt(struct rt9467_info *info, bool en)
{
	dev_info(info->dev, "%s: en = %d\n", __func__, en);
	return (en ? rt9467_set_bit : rt9467_clr_bit)
		(info, RT9467_REG_CHG_CTRL13, RT9467_MASK_WDT_EN);
}

static inline int rt9467_select_input_current_limit(struct rt9467_info *info,
	enum rt9467_iin_limit_sel sel)
{
	dev_info(info->dev, "%s: sel = %d\n", __func__, sel);
	return rt9467_i2c_update_bits(info, RT9467_REG_CHG_CTRL2,
		sel << RT9467_SHIFT_IINLMTSEL, RT9467_MASK_IINLMTSEL);
}

static int rt9467_enable_hidden_mode(struct rt9467_info *info, bool en)
{
	int ret = 0;

	mutex_lock(&info->hidden_mode_lock);

	if (en) {
		if (info->hidden_mode_cnt == 0) {
			ret = rt9467_i2c_block_write(info, 0x70,
				ARRAY_SIZE(rt9467_val_en_hidden_mode),
				rt9467_val_en_hidden_mode);
			if (ret < 0)
				goto err;
		}
		info->hidden_mode_cnt++;
	} else {
		if (info->hidden_mode_cnt == 1) /* last one */
			ret = rt9467_i2c_write_byte(info, 0x70, 0x00);
		info->hidden_mode_cnt--;
		if (ret < 0)
			goto err;
	}
	dev_dbg(info->dev, "%s: en = %d\n", __func__, en);
	goto out;

err:
	dev_notice(info->dev, "%s: en = %d fail(%d)\n", __func__, en, ret);
out:
	mutex_unlock(&info->hidden_mode_lock);
	return ret;
}

#ifdef CODE_OLD
static int rt9467_set_iprec(struct rt9467_info *info, u32 iprec)
{
	u8 reg_iprec = 0;

	reg_iprec = rt9467_closest_reg(RT9467_IPREC_MIN, RT9467_IPREC_MAX,
		RT9467_IPREC_STEP, iprec);

	dev_info(info->dev, "%s: iprec = %d(0x%02X)\n", __func__, iprec,
		reg_iprec);

	return rt9467_i2c_update_bits(info, RT9467_REG_CHG_CTRL8,
		reg_iprec << RT9467_SHIFT_IPREC, RT9467_MASK_IPREC);
}
#endif
static int rt9467_sw_workaround(struct rt9467_info *info)
{
	int ret = 0;

	dev_info(info->dev, "%s\n", __func__);

	rt9467_enable_hidden_mode(info, true);

	/* Modify UG driver */
	ret = rt9467_i2c_update_bits(info, RT9467_REG_CHG_HIDDEN_CTRL4, 0xC0,
		0xF0);
	if (ret < 0)
		dev_notice(info->dev, "%s: set UG driver fail\n", __func__);

	ret = rt9467_i2c_read_byte(info, RT9467_REG_CHG_HIDDEN_CTRL4);
	dev_info(info->dev, "%s: reg0x23 = 0x%02X\n", __func__, ret);

	/* Disable TS auto sensing */
	ret = rt9467_clr_bit(info, RT9467_REG_CHG_HIDDEN_CTRL15, 0x01);
	if (ret < 0)
		goto out;

#ifdef CODE_OLD
	/* Set precharge current to 850mA, only do this in normal boot */
	if (info->chip_rev <= RT9467_CHIP_REV_E3) {
		/* Worst case delay: wait auto sensing */
		msleep(200);

		if (get_boot_mode() == NORMAL_BOOT) {
			ret = rt9467_set_iprec(info, 850000);
			if (ret < 0)
				goto out;

			/* Increase Isys drop threshold to 2.5A */
			ret = rt9467_i2c_write_byte(info,
				RT9467_REG_CHG_HIDDEN_CTRL7, 0x1C);
			if (ret < 0)
				goto out;
		}
	}
#endif
	/* Only revision <= E1 needs the following workaround */
	if (info->chip_rev > RT9467_CHIP_REV_E1)
		goto out;

	/* ICC: modify sensing node, make it more accurate */
	ret = rt9467_i2c_write_byte(info, RT9467_REG_CHG_HIDDEN_CTRL8, 0x00);
	if (ret < 0)
		goto out;

	/* DIMIN level */
	ret = rt9467_i2c_write_byte(info, RT9467_REG_CHG_HIDDEN_CTRL9, 0x86);

out:
	rt9467_enable_hidden_mode(info, false);
	return ret;
}

static inline int rt9467_enable_hz(struct rt9467_info *info, bool en)
{
	dev_info(info->dev, "%s: en = %d\n", __func__, en);
	return (en ? rt9467_set_bit : rt9467_clr_bit)
		(info, RT9467_REG_CHG_CTRL1, RT9467_MASK_HZ_EN);
}

/* Reset all registers' value to default */
static int rt9467_reset_chip(struct rt9467_info *info)
{
	int ret = 0;

	dev_info(info->dev, "%s\n", __func__);

	/* disable hz before reset chip */
	ret = rt9467_enable_hz(info, false);
	if (ret < 0) {
		dev_notice(info->dev, "%s: disable hz fail\n", __func__);
		return ret;
	}

	return rt9467_set_bit(info, RT9467_REG_CORE_CTRL0, RT9467_MASK_RST);
}

static inline int __rt9467_enable_te(struct rt9467_info *info, bool en)
{
	dev_info(info->dev, "%s: en = %d\n", __func__, en);
	return (en ? rt9467_set_bit : rt9467_clr_bit)
		(info, RT9467_REG_CHG_CTRL2, RT9467_MASK_TE_EN);
}

static inline int __rt9467_enable_safety_timer(struct rt9467_info *info,
	bool en)
{
	dev_info(info->dev, "%s: en = %d\n", __func__, en);
	return (en ? rt9467_set_bit : rt9467_clr_bit)
		(info, RT9467_REG_CHG_CTRL12, RT9467_MASK_TMR_EN);
}

static int __rt9467_set_ieoc(struct rt9467_info *info, u32 ieoc)
{
	int ret = 0;
	u8 reg_ieoc = 0;

	/* IEOC workaround */
	if (info->ieoc_wkard)
		ieoc += 100000; /* 100mA */

	reg_ieoc = rt9467_closest_reg(RT9467_IEOC_MIN, RT9467_IEOC_MAX,
		RT9467_IEOC_STEP, ieoc);

	dev_info(info->dev, "%s: ieoc = %d(0x%02X)\n", __func__, ieoc,
		reg_ieoc);

	ret = rt9467_i2c_update_bits(info, RT9467_REG_CHG_CTRL9,
		reg_ieoc << RT9467_SHIFT_IEOC, RT9467_MASK_IEOC);

	/* Store IEOC */
	return rt9467_get_ieoc(info, &info->ieoc);
}

static int __rt9467_get_mivr(struct rt9467_info *info, u32 *mivr)
{
	int ret = 0;
	u8 reg_mivr = 0;

	ret = rt9467_i2c_read_byte(info, RT9467_REG_CHG_CTRL6);
	if (ret < 0)
		return ret;
	reg_mivr = ((ret & RT9467_MASK_MIVR) >> RT9467_SHIFT_MIVR) & 0xFF;

	*mivr = rt9467_closest_value(RT9467_MIVR_MIN, RT9467_MIVR_MAX,
		RT9467_MIVR_STEP, reg_mivr);

	return ret;
}

static int __rt9467_set_mivr(struct rt9467_info *info, u32 mivr)
{
	u8 reg_mivr = 0;

	reg_mivr = rt9467_closest_reg(RT9467_MIVR_MIN, RT9467_MIVR_MAX,
		RT9467_MIVR_STEP, mivr);

	dev_info(info->dev, "%s: mivr = %d(0x%02X)\n", __func__, mivr,
		reg_mivr);

	return rt9467_i2c_update_bits(info, RT9467_REG_CHG_CTRL6,
		reg_mivr << RT9467_SHIFT_MIVR, RT9467_MASK_MIVR);
}

static inline int rt9467_enable_jeita(struct rt9467_info *info, bool en)
{
	dev_info(info->dev, "%s: en = %d\n", __func__, en);
	return (en ? rt9467_set_bit : rt9467_clr_bit)
		(info, RT9467_REG_CHG_CTRL16, RT9467_MASK_JEITA_EN);
}


static int rt9467_get_charging_status(struct rt9467_info *info,
	enum rt9467_charging_status *chg_stat)
{
	int ret = 0;

	ret = rt9467_i2c_read_byte(info, RT9467_REG_CHG_STAT);
	if (ret < 0)
		return ret;

	*chg_stat = (ret & RT9467_MASK_CHG_STAT) >> RT9467_SHIFT_CHG_STAT;

	return ret;
}

static int rt9467_get_ieoc(struct rt9467_info *info, u32 *ieoc)
{
	int ret = 0;
	u8 reg_ieoc = 0;

	ret = rt9467_i2c_read_byte(info, RT9467_REG_CHG_CTRL9);
	if (ret < 0)
		return ret;

	reg_ieoc = (ret & RT9467_MASK_IEOC) >> RT9467_SHIFT_IEOC;
	*ieoc = rt9467_closest_value(RT9467_IEOC_MIN, RT9467_IEOC_MAX,
		RT9467_IEOC_STEP, reg_ieoc);

	return ret;
}

static inline int __rt9467_is_charging_enable(struct rt9467_info *info,
	bool *en)
{
	return rt9467_i2c_test_bit(info, RT9467_REG_CHG_CTRL2,
		RT9467_SHIFT_CHG_EN, en);
}

static int __rt9467_get_ichg(struct rt9467_info *info, u32 *ichg)
{
	int ret = 0;
	u8 reg_ichg = 0;

	ret = rt9467_i2c_read_byte(info, RT9467_REG_CHG_CTRL7);
	if (ret < 0)
		return ret;

	reg_ichg = (ret & RT9467_MASK_ICHG) >> RT9467_SHIFT_ICHG;
	*ichg = rt9467_closest_value(RT9467_ICHG_MIN, RT9467_ICHG_MAX,
		RT9467_ICHG_STEP, reg_ichg);

	return ret;
}

static inline int rt9467_ichg_workaround(struct rt9467_info *info, u32 uA)
{
	int ret = 0;

	/* Vsys short protection */
	rt9467_enable_hidden_mode(info, true);

	if (info->ichg >= 900000 && uA < 900000)
		ret = rt9467_i2c_update_bits(info, RT9467_REG_CHG_HIDDEN_CTRL7,
			0x00, 0x60);
	else if (uA >= 900000 && info->ichg < 900000)
		ret = rt9467_i2c_update_bits(info, RT9467_REG_CHG_HIDDEN_CTRL7,
			0x40, 0x60);

	rt9467_enable_hidden_mode(info, false);
	return ret;
}

static int __rt9467_set_ichg(struct rt9467_info *info, u32 ichg)
{
	int ret = 0;
	u8 reg_ichg = 0;

	if (strcmp(info->desc->chg_dev_name, "primary_chg") == 0) {
		ichg = (ichg < 500000) ? 500000 : ichg;
		rt9467_ichg_workaround(info, ichg);
	}

	reg_ichg = rt9467_closest_reg(RT9467_ICHG_MIN, RT9467_ICHG_MAX,
		RT9467_ICHG_STEP, ichg);

	dev_info(info->dev, "%s: ichg = %d(0x%02X)\n", __func__, ichg,
		reg_ichg);

	ret = rt9467_i2c_update_bits(info, RT9467_REG_CHG_CTRL7,
		reg_ichg << RT9467_SHIFT_ICHG, RT9467_MASK_ICHG);
	if (ret < 0)
		return ret;

	/* Store Ichg setting */
	__rt9467_get_ichg(info, &info->ichg);

	/* Workaround to make IEOC accurate */
	if (ichg < 900000 && !info->ieoc_wkard) { /* 900mA */
		ret = __rt9467_set_ieoc(info, info->ieoc + 100000);
		info->ieoc_wkard = true;
	} else if (ichg >= 900000 && info->ieoc_wkard) {
		info->ieoc_wkard = false;
		ret = __rt9467_set_ieoc(info, info->ieoc - 100000);
	}

	return ret;
}

static int __rt9467_set_cv(struct rt9467_info *info, u32 cv)
{
	u8 reg_cv = 0;

	reg_cv = rt9467_closest_reg(RT9467_CV_MIN, RT9467_CV_MAX,
		RT9467_CV_STEP, cv);

	dev_info(info->dev, "%s: cv = %d(0x%02X)\n", __func__, cv, reg_cv);

	return rt9467_i2c_update_bits(info, RT9467_REG_CHG_CTRL4,
		reg_cv << RT9467_SHIFT_CV, RT9467_MASK_CV);
}

static int rt9467_set_ircmp_resistor(struct rt9467_info *info, u32 uohm)
{
	u8 reg_resistor = 0;

	reg_resistor = rt9467_closest_reg(RT9467_IRCMP_RES_MIN,
		RT9467_IRCMP_RES_MAX, RT9467_IRCMP_RES_STEP, uohm);

	dev_info(info->dev, "%s: resistor = %d(0x%02X)\n", __func__, uohm,
		reg_resistor);

	return rt9467_i2c_update_bits(info, RT9467_REG_CHG_CTRL18,
		reg_resistor << RT9467_SHIFT_IRCMP_RES, RT9467_MASK_IRCMP_RES);
}

static int rt9467_set_ircmp_vclamp(struct rt9467_info *info, u32 uV)
{
	u8 reg_vclamp = 0;

	reg_vclamp = rt9467_closest_reg(RT9467_IRCMP_VCLAMP_MIN,
		RT9467_IRCMP_VCLAMP_MAX, RT9467_IRCMP_VCLAMP_STEP, uV);

	dev_info(info->dev, "%s: vclamp = %d(0x%02X)\n", __func__, uV,
		reg_vclamp);

	return rt9467_i2c_update_bits(info, RT9467_REG_CHG_CTRL18,
		reg_vclamp << RT9467_SHIFT_IRCMP_VCLAMP,
		RT9467_MASK_IRCMP_VCLAMP);
}
#ifdef OLD
static int rt9467_enable_pump_express(struct rt9467_info *info, bool en)
{
	int ret = 0, i = 0;
	bool pumpx_en = false;
	const int max_wait_times = 3;

	dev_info(info->dev, "%s: en = %d\n", __func__, en);

	ret = __rt9467_set_aicr(info, 800000);
	if (ret < 0)
		return ret;

	ret = __rt9467_set_ichg(info, 2000000);
	if (ret < 0)
		return ret;

	ret = rt9467_enable_charging(info, true);
	if (ret < 0)
		return ret;

	rt9467_enable_hidden_mode(info, true);

	ret = rt9467_clr_bit(info, RT9467_REG_CHG_HIDDEN_CTRL9, 0x80);
	if (ret < 0)
		dev_notice(info->dev, "%s: disable skip mode fail\n", __func__);

	ret = (en ? rt9467_set_bit : rt9467_clr_bit)
		(info, RT9467_REG_CHG_CTRL17, RT9467_MASK_PUMPX_EN);
	if (ret < 0)
		goto out;

	for (i = 0; i < max_wait_times; i++) {
		msleep(2500);
		ret = rt9467_i2c_test_bit(info, RT9467_REG_CHG_CTRL17,
			RT9467_SHIFT_PUMPX_EN, &pumpx_en);
		if (ret >= 0 && !pumpx_en)
			break;
	}
	if (i == max_wait_times) {
		dev_notice(info->dev, "%s: pumpx done fail(%d)\n", __func__,
			ret);
		ret = -EIO;
	} else
		ret = 0;

out:
	rt9467_set_bit(info, RT9467_REG_CHG_HIDDEN_CTRL9, 0x80);
	rt9467_enable_hidden_mode(info, false);
	return ret;
}
#endif
static inline int rt9467_enable_irq_pulse(struct rt9467_info *info, bool en)
{
	dev_info(info->dev, "%s: en = %d\n", __func__, en);
	return (en ? rt9467_set_bit : rt9467_clr_bit)
		(info, RT9467_REG_CHG_CTRL1, RT9467_MASK_IRQ_PULSE);
}
#ifdef CODE_OLD
static inline int rt9467_get_irq_number(struct rt9467_info *info,
	const char *name)
{
	int i = 0;

	if (!name) {
		dev_notice(info->dev, "%s: null name\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(rt9467_irq_mapping_tbl); i++) {
		if (!strcmp(name, rt9467_irq_mapping_tbl[i].name))
			return rt9467_irq_mapping_tbl[i].id;
	}

	return -EINVAL;
}


/* =========================================================== */
/* Released interfaces                                         */
/* =========================================================== */
static int rt9467_enable_charging(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_info(info->dev, "%s: en = %d\n", __func__, en);

	/* set hz/ceb pin for secondary charger */
	if (strcmp(info->desc->chg_dev_name, "secondary_chg") == 0) {
		ret = rt9467_enable_hz(info, !en);
		if (ret < 0) {
			dev_notice(info->dev, "%s: set hz of sec chg fail\n",
				__func__);
			return ret;
		}
		if (info->desc->ceb_invert)
			gpio_set_value(info->ceb_gpio, en);
		else
			gpio_set_value(info->ceb_gpio, !en);
	}

	return (en ? rt9467_set_bit : rt9467_clr_bit)
		(info, RT9467_REG_CHG_CTRL2, RT9467_MASK_CHG_EN);
}

static int rt9467_enable_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	return __rt9467_enable_safety_timer(info, en);
}

static int rt9467_set_boost_current_limit(struct charger_device *chg_dev,
	u32 current_limit)
{
	u8 reg_ilimit = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	reg_ilimit = rt9467_closest_reg_via_tbl(rt9467_boost_oc_threshold,
		ARRAY_SIZE(rt9467_boost_oc_threshold), current_limit);

	dev_info(info->dev, "%s: boost ilimit = %d(0x%02X)\n", __func__,
		current_limit, reg_ilimit);

	return rt9467_i2c_update_bits(info, RT9467_REG_CHG_CTRL10,
		reg_ilimit << RT9467_SHIFT_BOOST_OC, RT9467_MASK_BOOST_OC);
}

static int rt9467_enable_otg(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	bool en_otg = false;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);
	u8 hidden_val = en ? 0x00 : 0x0F;
	u8 lg_slew_rate = en ? 0xCC : 0xC3;

	dev_info(info->dev, "%s: en = %d\n", __func__, en);

	rt9467_enable_hidden_mode(info, true);

	/* Set OTG_OC to 500mA */
	ret = rt9467_set_boost_current_limit(chg_dev, 500000);
	if (ret < 0) {
		dev_notice(info->dev, "%s: set current limit fail\n", __func__);
		return ret;
	}

	/*
	 * Woraround : slow Low side mos Gate driver slew rate
	 * for decline VBUS noise
	 * reg[0x23] = 0xCC after entering OTG mode
	 * reg[0x23] = 0xC3 after leaving OTG mode
	 */
	ret = rt9467_i2c_write_byte(info, RT9467_REG_CHG_HIDDEN_CTRL4,
		lg_slew_rate);
	if (ret < 0) {
		dev_notice(info->dev,
			"%s: set Low side mos Gate drive speed fail(%d)\n",
			__func__, ret);
		goto out;
	}

	/* Enable WDT */
	if (en && info->desc->en_wdt) {
		ret = rt9467_enable_wdt(info, true);
		if (ret < 0) {
			dev_notice(info->dev, "%s: en wdt fail\n", __func__);
			goto err_en_otg;
		}
	}

	/* Switch OPA mode */
	ret = (en ? rt9467_set_bit : rt9467_clr_bit)
		(info, RT9467_REG_CHG_CTRL1, RT9467_MASK_OPA_MODE);

	msleep(20);

	if (en) {
		ret = rt9467_i2c_test_bit(info, RT9467_REG_CHG_CTRL1,
			RT9467_SHIFT_OPA_MODE, &en_otg);
		if (ret < 0 || !en_otg) {
			dev_notice(info->dev, "%s: otg fail(%d)\n", __func__,
				ret);
			goto err_en_otg;
		}
	}

	/*
	 * Woraround reg[0x25] = 0x00 after entering OTG mode
	 * reg[0x25] = 0x0F after leaving OTG mode
	 */
	ret = rt9467_i2c_write_byte(info, RT9467_REG_CHG_HIDDEN_CTRL6,
		hidden_val);
	if (ret < 0)
		dev_notice(info->dev, "%s: workaroud fail(%d)\n", __func__,
			ret);

	/* Disable WDT */
	if (!en) {
		ret = rt9467_enable_wdt(info, false);
		if (ret < 0)
			dev_notice(info->dev, "%s: disable wdt fail\n",
				__func__);
	}
	goto out;

err_en_otg:
	/* Disable WDT */
	ret = rt9467_enable_wdt(info, false);
	if (ret < 0)
		dev_notice(info->dev, "%s: disable wdt fail\n", __func__);

	/* Recover Low side mos Gate slew rate */
	ret = rt9467_i2c_write_byte(info, RT9467_REG_CHG_HIDDEN_CTRL4, 0x73);
	if (ret < 0)
		dev_notice(info->dev,
			"%s: recover Low side mos Gate drive speed fail(%d)\n",
			__func__, ret);
	ret = -EIO;
out:
	rt9467_enable_hidden_mode(info, false);
	return ret;
}

static int rt9467_enable_discharge(struct charger_device *chg_dev, bool en)
{
	int ret = 0, i = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);
	const int check_dischg_max = 3;
	bool is_dischg = true;

	dev_info(info->dev, "%s: en = %d\n", __func__, en);

	ret = rt9467_enable_hidden_mode(info, true);
	if (ret < 0)
		return ret;

	/* Set bit2 of reg[0x21] to 1 to enable discharging */
	ret = (en ? rt9467_set_bit : rt9467_clr_bit)(info,
		RT9467_REG_CHG_HIDDEN_CTRL2, 0x04);
	if (ret < 0) {
		dev_notice(info->dev, "%s: en = %d, fail\n", __func__, en);
		return ret;
	}

	if (!en) {
		for (i = 0; i < check_dischg_max; i++) {
			ret = rt9467_i2c_test_bit(info,
				RT9467_REG_CHG_HIDDEN_CTRL2, 2, &is_dischg);
			if (ret >= 0 && !is_dischg)
				break;
			/* Disable discharging */
			ret = rt9467_clr_bit(info, RT9467_REG_CHG_HIDDEN_CTRL2,
				0x04);
		}
		if (i == check_dischg_max)
			dev_notice(info->dev, "%s: disable dischg fail(%d)\n",
				__func__, ret);
	}

	rt9467_enable_hidden_mode(info, false);
	return ret;
}

static int rt9467_enable_power_path(struct charger_device *chg_dev, bool en)
{
	u32 mivr = (en ? 4500000 : RT9467_MIVR_MAX);
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_info(info->dev, "%s: en = %d\n", __func__, en);
	return __rt9467_set_mivr(info, mivr);
}

static int rt9467_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	int ret = 0;

#ifdef CONFIG_TCPC_CLASS
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	if (!info->desc->en_chgdet) {
		dev_info(info->dev, "%s: bc12 is disabled by dts\n", __func__);
		return 0;
	}

	dev_info(info->dev, "%s: en = %d\n", __func__, en);

	atomic_set(&info->tcpc_usb_connected, en);

	/* TypeC detach */
	if (!en) {
		ret = rt9467_chgdet_handler(info);
		return ret;
	}

	/* plug in, make usb switch to RT9467 */
	ret = rt9467_enable_chgdet_flow(info, true);
	if (ret < 0)
		dev_notice(info->dev, "%s: en chgdet fail(%d)\n", __func__,
			ret);
#endif /* CONFIG_TCPC_CLASS */

	return ret;
}

static int rt9467_is_power_path_enable(struct charger_device *chg_dev, bool *en)
{
	int ret = 0;
	u32 mivr = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = __rt9467_get_mivr(info, &mivr);
	if (ret < 0)
		return ret;

	*en = ((mivr == RT9467_MIVR_MAX) ? false : true);

	return ret;
}

static int rt9467_set_ichg(struct charger_device *chg_dev, u32 ichg)
{
	int ret = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	mutex_lock(&info->ichg_access_lock);
	mutex_lock(&info->ieoc_lock);
	ret = __rt9467_set_ichg(info, ichg);
	mutex_unlock(&info->ieoc_lock);
	mutex_unlock(&info->ichg_access_lock);

	return ret;
}

static int rt9467_set_ieoc(struct charger_device *chg_dev, u32 ieoc)
{
	int ret = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	mutex_lock(&info->ichg_access_lock);
	mutex_lock(&info->ieoc_lock);
	ret = __rt9467_set_ieoc(info, ieoc);
	mutex_unlock(&info->ieoc_lock);
	mutex_unlock(&info->ichg_access_lock);

	return ret;
}

static int rt9467_set_aicr(struct charger_device *chg_dev, u32 aicr)
{
	int ret = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	mutex_lock(&info->aicr_access_lock);
	ret = __rt9467_set_aicr(info, aicr);
	mutex_unlock(&info->aicr_access_lock);

	return ret;
}

static int rt9467_set_mivr(struct charger_device *chg_dev, u32 mivr)
{
	int ret = 0;
	bool en = true;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = rt9467_is_power_path_enable(chg_dev, &en);
	if (ret < 0) {
		dev_notice(info->dev, "%s: get power path en fail\n", __func__);
		return ret;
	}

	if (!en) {
		dev_info(info->dev,
			"%s: power path is disabled, op is not allowed\n",
			__func__);
		return -EINVAL;
	}

	return __rt9467_set_mivr(info, mivr);
}

static int rt9467_set_cv(struct charger_device *chg_dev, u32 cv)
{
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	return __rt9467_set_cv(info, cv);
}

static int rt9467_set_pep_current_pattern(struct charger_device *chg_dev,
	bool is_increase)
{
	int ret = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_info(info->dev, "%s: pump_up = %d\n", __func__, is_increase);

	mutex_lock(&info->pe_access_lock);

	/* Set to PE1.0 */
	ret = rt9467_clr_bit(info, RT9467_REG_CHG_CTRL17,
		RT9467_MASK_PUMPX_20_10);

	/* Set Pump Up/Down */
	ret = (is_increase ? rt9467_set_bit : rt9467_clr_bit)
		(info, RT9467_REG_CHG_CTRL17, RT9467_MASK_PUMPX_UP_DN);

	/* Enable PumpX */
	ret = rt9467_enable_pump_express(info, true);
	mutex_unlock(&info->pe_access_lock);

	return ret;
}

static int rt9467_set_pep20_reset(struct charger_device *chg_dev)
{
	int ret = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	mutex_lock(&info->pe_access_lock);
	ret = rt9467_set_mivr(chg_dev, 4500000);
	if (ret < 0)
		goto out;

	/* Disable PSK mode */
	rt9467_enable_hidden_mode(info, true);
	ret = rt9467_clr_bit(info, RT9467_REG_CHG_HIDDEN_CTRL9, 0x80);
	if (ret < 0)
		dev_notice(info->dev, "%s: disable skip mode fail\n", __func__);

	ret = rt9467_set_aicr(chg_dev, 100000);
	if (ret < 0)
		goto psk_out;

	msleep(250);

	ret = rt9467_set_aicr(chg_dev, 700000);

psk_out:
	rt9467_set_bit(info, RT9467_REG_CHG_HIDDEN_CTRL9, 0x80);
	rt9467_enable_hidden_mode(info, false);
out:
	mutex_unlock(&info->pe_access_lock);
	return ret;
}

static int rt9467_set_pep20_current_pattern(struct charger_device *chg_dev,
	u32 uV)
{
	int ret = 0;
	u8 reg_volt = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	mutex_lock(&info->pe_access_lock);

	reg_volt = rt9467_closest_reg(RT9467_PEP20_VOLT_MIN,
		RT9467_PEP20_VOLT_MAX, RT9467_PEP20_VOLT_STEP, uV);

	dev_info(info->dev, "%s: volt = %d(0x%02X)\n", __func__, uV, reg_volt);

	/* Set to PEP2.0 */
	ret = rt9467_set_bit(info, RT9467_REG_CHG_CTRL17,
		RT9467_MASK_PUMPX_20_10);
	if (ret < 0)
		goto out;

	/* Set Voltage */
	ret = rt9467_i2c_update_bits(info, RT9467_REG_CHG_CTRL17,
		reg_volt << RT9467_SHIFT_PUMPX_DEC, RT9467_MASK_PUMPX_DEC);
	if (ret < 0)
		goto out;

	/* Enable PumpX */
	ret = rt9467_enable_pump_express(info, true);

out:
	mutex_unlock(&info->pe_access_lock);
	return ret;
}

static int rt9467_enable_cable_drop_comp(struct charger_device *chg_dev,
	bool en)
{
	int ret = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_info(info->dev, "%s: en = %d\n", __func__, en);

	mutex_lock(&info->pe_access_lock);

	/* Set to PEP2.0 */
	ret = rt9467_set_bit(info, RT9467_REG_CHG_CTRL17,
		RT9467_MASK_PUMPX_20_10);
	if (ret < 0)
		goto out;

	/* Set Voltage */
	ret = rt9467_i2c_update_bits(info, RT9467_REG_CHG_CTRL17,
		0x1F << RT9467_SHIFT_PUMPX_DEC, RT9467_MASK_PUMPX_DEC);
	if (ret < 0)
		goto out;

	/* Enable PumpX */
	ret = rt9467_enable_pump_express(info, true);

out:
	mutex_unlock(&info->pe_access_lock);
	return ret;
}

static int rt9467_get_ichg(struct charger_device *chg_dev, u32 *ichg)
{
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	return __rt9467_get_ichg(info, ichg);
}


static int rt9467_get_cv(struct charger_device *chg_dev, u32 *cv)
{
	int ret = 0;
	u8 reg_cv = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = rt9467_i2c_read_byte(info, RT9467_REG_CHG_CTRL4);
	if (ret < 0)
		return ret;

	reg_cv = (ret & RT9467_MASK_CV) >> RT9467_SHIFT_CV;
	*cv = rt9467_closest_value(RT9467_CV_MIN, RT9467_CV_MAX,
		RT9467_CV_STEP, reg_cv);

	return ret;
}

static int rt9467_get_tchg(struct charger_device *chg_dev, int *tchg_min,
	int *tchg_max)
{
	int ret = 0, adc_temp = 0;
	u32 retry_cnt = 3;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	/* Get value from ADC */
	ret = rt9467_get_adc(info, RT9467_ADC_TEMP_JC, &adc_temp);
	if (ret < 0)
		return ret;

	/* Check unusual temperature */
	while (adc_temp >= 120 && retry_cnt > 0) {
		dev_notice(info->dev,
			   "%s: [WARNING] t = %d\n", __func__, adc_temp);
		rt9467_get_adc(info, RT9467_ADC_VBAT, &adc_temp);
		ret = rt9467_get_adc(info, RT9467_ADC_TEMP_JC, &adc_temp);
		retry_cnt--;
	}
	if (ret < 0)
		return ret;

	mutex_lock(&info->tchg_lock);
	/* Use previous one to prevent system from rebooting */
	if (adc_temp >= 120)
		adc_temp = info->tchg;
	else
		info->tchg = adc_temp;
	mutex_unlock(&info->tchg_lock);

	*tchg_min = adc_temp;
	*tchg_max = adc_temp;

	dev_info(info->dev, "%s: temperature = %d\n", __func__, adc_temp);
	return ret;
}

static int rt9467_get_mivr_state(struct charger_device *chg_dev, bool *in_loop)
{
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	return rt9467_i2c_test_bit(info, RT9467_REG_CHG_STATC,
				   RT9467_SHIFT_CHG_MIVR, in_loop);
}

static int rt9467_get_ibat(struct charger_device *chg_dev, u32 *ibat)
{
	int ret = 0, adc_ibat = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	/* Get value from ADC */
	ret = rt9467_get_adc(info, RT9467_ADC_IBAT, &adc_ibat);
	if (ret < 0)
		return ret;

	*ibat = adc_ibat;

	dev_info(info->dev, "%s: ibat = %dmA\n", __func__, adc_ibat);
	return ret;
}
#endif

#if 0 /* Uncomment if you need this API */
static int rt9467_get_vbus(struct charger_device *chg_dev, u32 *vbus)
{
	int ret = 0, adc_vbus = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	/* Get value from ADC */
	ret = rt9467_get_adc(info, RT9467_ADC_VBUS_DIV2, &adc_vbus);
	if (ret < 0)
		return ret;

	*vbus = adc_vbus;

	dev_info(info->dev, "%s: vbus = %dmA\n", __func__, adc_vbus);
	return ret;
}
#endif
#ifdef OLD_CODE
static int rt9467_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	int ret = 0;
	enum rt9467_charging_status chg_stat = RT9467_CHG_STATUS_READY;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = rt9467_get_charging_status(info, &chg_stat);

	/* Return is charging done or not */
	switch (chg_stat) {
	case RT9467_CHG_STATUS_READY:
	case RT9467_CHG_STATUS_PROGRESS:
	case RT9467_CHG_STATUS_FAULT:
		*done = false;
		break;
	case RT9467_CHG_STATUS_DONE:
		*done = true;
		break;
	default:
		*done = false;
		break;
	}

	return ret;
}

static int rt9467_is_safety_timer_enable(struct charger_device *chg_dev,
	bool *en)
{
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	return rt9467_i2c_test_bit(info, RT9467_REG_CHG_CTRL12,
		RT9467_SHIFT_TMR_EN, en);
}

static int rt9467_kick_wdt(struct charger_device *chg_dev)
{
	enum rt9467_charging_status chg_status = RT9467_CHG_STATUS_READY;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	/* Any I2C communication can reset watchdog timer */
	return rt9467_get_charging_status(info, &chg_status);
}

static int rt9467_set_pep20_efficiency_table(struct charger_device *chg_dev)
{
	int ret = 0;
	struct charger_manager *chg_mgr = NULL;

	chg_mgr = charger_dev_get_drvdata(chg_dev);
	if (!chg_mgr)
		return -EINVAL;

	chg_mgr->pe2.profile[0].vchr = 8000000;
	chg_mgr->pe2.profile[1].vchr = 8000000;
	chg_mgr->pe2.profile[2].vchr = 8000000;
	chg_mgr->pe2.profile[3].vchr = 8500000;
	chg_mgr->pe2.profile[4].vchr = 8500000;
	chg_mgr->pe2.profile[5].vchr = 8500000;
	chg_mgr->pe2.profile[6].vchr = 9000000;
	chg_mgr->pe2.profile[7].vchr = 9000000;
	chg_mgr->pe2.profile[8].vchr = 9500000;
	chg_mgr->pe2.profile[9].vchr = 9500000;

	return ret;
}
static int __rt9467_enable_auto_sensing(struct rt9467_info *info, bool en)
{
	int ret = 0;
	u8 auto_sense = 0;
	u8 *data = 0x00;

	/* enter hidden mode */
	ret = rt9467_device_write(info->client, 0x70,
		ARRAY_SIZE(rt9467_val_en_hidden_mode),
		rt9467_val_en_hidden_mode);
	if (ret < 0)
		return ret;

	ret = rt9467_device_read(info->client, RT9467_REG_CHG_HIDDEN_CTRL15, 1,
		&auto_sense);
	if (ret < 0) {
		dev_notice(info->dev, "%s: read auto sense fail\n", __func__);
		goto out;
	}

	if (en)
		auto_sense &= 0xFE; /* clear bit0 */
	else
		auto_sense |= 0x01; /* set bit0 */
	ret = rt9467_device_write(info->client, RT9467_REG_CHG_HIDDEN_CTRL15, 1,
		&auto_sense);
	if (ret < 0)
		dev_notice(info->dev, "%s: en = %d fail\n", __func__, en);

out:
	return rt9467_device_write(info->client, 0x70, 1, &data);
}
/*
 * This function is used in shutdown function
 * Use i2c smbus directly
 */
static int rt9467_sw_reset(struct rt9467_info *info)
{
	int ret = 0;
	u8 evt[RT9467_IRQIDX_MAX] = {0};

	/* Register 0x01 ~ 0x10 */
	u8 reg_data[] = {
		0x10, 0x03, 0x23, 0x3C, 0x67, 0x0B, 0x4C, 0xA1,
		0x3C, 0x58, 0x2C, 0x02, 0x52, 0x05, 0x00, 0x10
	};

	dev_info(info->dev, "%s\n", __func__);

	/* Disable auto sensing/Enable HZ,ship mode of secondary charger */
	if (strcmp(info->desc->chg_dev_name, "secondary_chg") == 0) {
		mutex_lock(&info->hidden_mode_lock);
		mutex_lock(&info->i2c_access_lock);
		__rt9467_enable_auto_sensing(info, false);
		mutex_unlock(&info->i2c_access_lock);
		mutex_unlock(&info->hidden_mode_lock);

		reg_data[0] = 0x14; /* HZ */
		reg_data[1] = 0x83; /* Shipping mode */
	}

	/* Mask all irq */
	mutex_lock(&info->i2c_access_lock);
	ret = rt9467_device_write(info->client, RT9467_REG_CHG_STATC_CTRL,
		ARRAY_SIZE(rt9467_irq_maskall), rt9467_irq_maskall);
	if (ret < 0)
		dev_notice(info->dev, "%s: mask all irq fail\n", __func__);

	/* Read all irq */
	ret = rt9467_device_read(info->client, RT9467_REG_CHG_STATC, 5, evt);
	if (ret < 0)
		dev_notice(info->dev, "%s: read evt1 fail(%d)\n", __func__,
			ret);

	ret = rt9467_device_read(info->client, RT9467_REG_DPDM_IRQ, 1, &evt[6]);
	if (ret < 0)
		dev_notice(info->dev, "%s: read evt2 fail(%d)\n", __func__,
			ret);

	/* Reset necessary registers */
	ret = rt9467_device_write(info->client, RT9467_REG_CHG_CTRL1,
		ARRAY_SIZE(reg_data), reg_data);
	if (ret < 0)
		dev_notice(info->dev, "%s: reset registers fail\n", __func__);
	mutex_unlock(&info->i2c_access_lock);

	return ret;
}
#endif
static int rt9467_init_setting(struct rt9467_info *info)
{
	int ret = 0;
	u8 evt[RT9467_IRQIDX_MAX] = {0};

	dev_info(info->dev, "%s\n", __func__);

	info->desc = &rt9467_default_desc;
	
	info->desc->ichg = 2000000;
	info->desc->aicr = 500000;
	info->desc->mivr = 4500000;
	info->desc->cv = 4400000;
	info->desc->ieoc = 250000;
	info->desc->safety_timer = 12;
	info->desc->ircmp_resistor = 25000;
	info->desc->ircmp_vclamp = 32000;
	info->desc->en_te = true;
	info->desc->en_wdt = true;
	info->desc->en_irq_pulse = false;
	info->desc->en_jeita = false;
	info->desc->ceb_invert = false;
	info->desc->en_chgdet = true;

#ifdef CODE_OLD
	/* disable USB charger type detection before reset IRQ */
	ret = rt9467_enable_chgdet_flow(info, false);
	if (ret < 0) {
		dev_notice(info->dev, "%s: disable usb chrdet fail\n",
			__func__);
		goto err;
	}
#endif

	ret = rt9467_clr_bit(info, RT9467_REG_CHG_DPDM1, 0x40);
	if (ret < 0) {
		dev_notice(info->dev, "%s: disable attach delay fail\n",
			__func__);
		goto err;
	}

	/* mask all irq */
	ret = rt9467_maskall_irq(info);
	if (ret < 0) {
		dev_notice(info->dev, "%s: mask all irq fail\n", __func__);
		goto err;
	}

	/* clear event */
	ret = rt9467_i2c_block_read(info, RT9467_REG_CHG_STATC, ARRAY_SIZE(evt),
		evt);
	if (ret < 0) {
		dev_notice(info->dev, "%s: clr evt fail(%d)\n", __func__, ret);
		goto err;
	}

	ret = __rt9467_set_ichg(info, info->desc->ichg);
	if (ret < 0)
		dev_notice(info->dev, "%s: set ichg fail\n", __func__);
	dev_info(info->dev, "%s 2 \n", __func__);

	ret = __rt9467_set_aicr(info, info->desc->aicr);
	if (ret < 0)
		dev_notice(info->dev, "%s: set aicr fail\n", __func__);

	ret = __rt9467_set_mivr(info, info->desc->mivr);
	if (ret < 0)
		dev_notice(info->dev, "%s: set mivr fail\n", __func__);

	ret = __rt9467_set_cv(info, info->desc->cv);
	if (ret < 0)
		dev_notice(info->dev, "%s: set cv fail\n", __func__);

	ret = __rt9467_set_ieoc(info, info->desc->ieoc);
	if (ret < 0)
		dev_notice(info->dev, "%s: set ieoc fail\n", __func__);

	ret = __rt9467_enable_te(info, info->desc->en_te);
	if (ret < 0)
		dev_notice(info->dev, "%s: set te fail\n", __func__);

	ret = rt9467_set_safety_timer(info, info->desc->safety_timer);
	if (ret < 0)
		dev_notice(info->dev, "%s: set fast timer fail\n", __func__);

	ret = __rt9467_enable_safety_timer(info, true);
	if (ret < 0)
		dev_notice(info->dev, "%s: enable chg timer fail\n", __func__);

	ret = rt9467_enable_wdt(info, info->desc->en_wdt);
	if (ret < 0)
		dev_notice(info->dev, "%s: set wdt fail\n", __func__);

	ret = rt9467_enable_jeita(info, info->desc->en_jeita);
	if (ret < 0)
		dev_notice(info->dev, "%s: disable jeita fail\n", __func__);

	ret = rt9467_enable_irq_pulse(info, info->desc->en_irq_pulse);
	if (ret < 0)
		dev_notice(info->dev, "%s: set irq pulse fail\n", __func__);

	/* set ircomp according to BIF */
	ret = rt9467_set_ircmp_resistor(info, info->desc->ircmp_resistor);
	if (ret < 0)
		dev_notice(info->dev, "%s: set ircmp resistor fail\n",
			__func__);

	ret = rt9467_set_ircmp_vclamp(info, info->desc->ircmp_vclamp);
	if (ret < 0)
		dev_notice(info->dev, "%s: set ircmp clamp fail\n", __func__);

	ret = rt9467_sw_workaround(info);
	if (ret < 0) {
		dev_notice(info->dev, "%s: workaround fail\n", __func__);
		return ret;
	}

#ifdef CODE_OLD
	/* Enable HZ mode of secondary charger */
	if (strcmp(info->desc->chg_dev_name, "secondary_chg") == 0) {
		ret = rt9467_enable_hz(info, true);
		if (ret < 0)
			dev_notice(info->dev, "%s: hz sec chg fail\n",
				__func__);
	}
#endif	
err:
	return ret;
}

#ifdef CODE_OLD
static int rt9467_plug_in(struct charger_device *chg_dev)
{
	int ret = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_info(info->dev, "%s\n", __func__);

	/* Enable WDT */
	if (info->desc->en_wdt) {
		ret = rt9467_enable_wdt(info, true);
		if (ret < 0)
			dev_notice(info->dev, "%s: en wdt fail\n", __func__);
	}

	/* Enable charging */
	if (strcmp(info->desc->chg_dev_name, "primary_chg") == 0) {
		ret = rt9467_enable_charging(chg_dev, true);
		if (ret < 0)
			dev_notice(info->dev, "%s: en chg fail\n", __func__);
	}

	return ret;
}

static int rt9467_plug_out(struct charger_device *chg_dev)
{
	int ret = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	dev_info(info->dev, "%s\n", __func__);

	/* Reset AICR limit */
	info->aicr_limit = -1;

	/* Disable charging */
	ret = rt9467_enable_charging(chg_dev, false);
	if (ret < 0) {
		dev_notice(info->dev, "%s: disable chg fail\n", __func__);
		return ret;
	}

	/* Disable WDT */
	ret = rt9467_enable_wdt(info, false);
	if (ret < 0)
		dev_notice(info->dev, "%s: disable wdt fail\n", __func__);

	/* enable HZ mode of secondary charger */
	if (strcmp(info->desc->chg_dev_name, "secondary_chg") == 0) {
		ret = rt9467_enable_hz(info, true);
		if (ret < 0)
			dev_notice(info->dev, "%s: en hz of sec chg fail\n",
				__func__);
	}

	return ret;
}

static int rt9467_is_chip_enabled(struct charger_device *chg_dev, bool *en)
{
	*en = true;
	return 0;
}
static int rt9467_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	return __rt9467_is_charging_enable(info, en);
}

static int rt9467_get_min_ichg(struct charger_device *chg_dev, u32 *uA)
{
	int ret = 0;

	*uA = rt9467_closest_value(RT9467_ICHG_MIN, RT9467_ICHG_MAX,
		RT9467_ICHG_STEP, 0);

	return ret;
}

static int rt9467_run_aicl(struct charger_device *chg_dev, u32 *uA)
{
	int ret = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = __rt9467_run_aicl(info);
	if (ret >= 0)
		*uA = info->aicr_limit;

	return ret;
}

static int rt9467_enable_te(struct charger_device *chg_dev, bool en)
{
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	return __rt9467_enable_te(info, en);
}

static int rt9467_reset_eoc_state(struct charger_device *chg_dev)
{
	int ret = 0;
	struct rt9467_info *info = dev_get_drvdata(&chg_dev->dev);

	/* Toggle EOC_RST */
	rt9467_enable_hidden_mode(info, true);
	ret = rt9467_set_bit(info, RT9467_REG_CHG_HIDDEN_CTRL1, 0x80);
	if (ret < 0) {
		dev_notice(info->dev, "%s: set eoc rst fail\n", __func__);
		goto out;
	}

	ret = rt9467_clr_bit(info, RT9467_REG_CHG_HIDDEN_CTRL1, 0x80);
	if (ret < 0)
		dev_notice(info->dev, "%s: clr eoc rst fail\n", __func__);
out:
	rt9467_enable_hidden_mode(info, false);
	return ret;
}

#endif


static int rt9467_enable_charging(struct rt9467_info *info, bool en)
{

	dev_info(info->dev, "%s: en = %d\n", __func__, en);

	return (en ? rt9467_set_bit : rt9467_clr_bit)
		(info, RT9467_REG_CHG_CTRL2, RT9467_MASK_CHG_EN);
}
static int rt9467_get_cv(struct rt9467_info *info, u32 *cv)
{
	int ret = 0;
	u8 reg_cv = 0;

	ret = rt9467_i2c_read_byte(info, RT9467_REG_CHG_CTRL4);
	if (ret < 0)
		return ret;

	reg_cv = (ret & RT9467_MASK_CV) >> RT9467_SHIFT_CV;
	*cv = rt9467_closest_value(RT9467_CV_MIN, RT9467_CV_MAX,
		RT9467_CV_STEP, reg_cv);

	return ret;
}

static void rt9467_dump_regs(struct rt9467_info *info)
{
	int i = 0, ret = 0;
	u32 ichg = 0, aicr = 0, mivr = 0, ieoc = 0, cv = 0;
	bool chg_en = 0;
	int adc_vsys = 0, adc_vbat = 0, adc_ibat = 0;
	int adc_ibus = 0, adc_vbus = 0;
	enum rt9467_charging_status chg_status = RT9467_CHG_STATUS_READY;
	u8 chg_stat = 0, chg_ctrl[2] = {0};

	ret = __rt9467_get_ichg(info, &ichg);
	ret = rt9467_get_aicr(info, &aicr);
	ret = __rt9467_get_mivr(info, &mivr);
	ret = __rt9467_is_charging_enable(info, &chg_en);
	ret = rt9467_get_ieoc(info, &ieoc);
	ret = rt9467_get_cv(info, &cv);
	ret = rt9467_get_charging_status(info, &chg_status);
	ret = rt9467_get_adc(info, RT9467_ADC_VSYS, &adc_vsys);
	ret = rt9467_get_adc(info, RT9467_ADC_VBAT, &adc_vbat);
	ret = rt9467_get_adc(info, RT9467_ADC_IBAT, &adc_ibat);
	ret = rt9467_get_adc(info, RT9467_ADC_IBUS, &adc_ibus);
	ret = rt9467_get_adc(info, RT9467_ADC_VBUS_DIV5, &adc_vbus);
	chg_stat = rt9467_i2c_read_byte(info, RT9467_REG_CHG_STATC);
	ret = rt9467_i2c_block_read(info, RT9467_REG_CHG_CTRL1, 2, chg_ctrl);

	/* Charging fault, dump all registers' value */
	if (chg_status == RT9467_CHG_STATUS_FAULT) {
		for (i = 0; i < ARRAY_SIZE(rt9467_reg_addr); i++)
			ret = rt9467_i2c_read_byte(info, rt9467_reg_addr[i]);
	}

	dev_info(info->dev,
	"%s: ICHG = %dmA, AICR = %dmA, MIVR = %dmV, IEOC = %dmA, CV = %dmV\n",
	__func__, ichg / 1000, aicr / 1000, mivr / 1000,
	ieoc / 1000, cv / 1000);

	dev_info(info->dev,
	"%s: VSYS = %dmV, VBAT = %dmV, IBAT = %dmA, IBUS = %dmA, VBUS = %dmV\n",
	__func__, adc_vsys / 1000, adc_vbat / 1000, adc_ibat / 1000,
	adc_ibus / 1000, adc_vbus / 1000);

	dev_info(info->dev,
		"%s: CHG_EN = %d, CHG_STATUS = %s, CHG_STAT = 0x%02X\n",
		__func__, chg_en, rt9467_chg_status_name[chg_status], chg_stat);

	dev_info(info->dev, "%s: CHG_CTRL1 = 0x%02X, CHG_CTRL2 = 0x%02X\n",
		__func__, chg_ctrl[0], chg_ctrl[1]);

}



static int
rt9467_charger_set_vindpm(struct rt9467_info *info, u32 vol)
{

	return __rt9467_set_mivr(info, vol);
}
static int  rt9467_enable_powerpath(struct rt9467_info *info, bool en)
{
	int ret;

	dev_err(info->dev,"%s; %d;\n", __func__,en);

	if(en)
		ret=rt9467_charger_set_vindpm(info, 4500000);
	else	
		ret=rt9467_charger_set_vindpm(info, 5400000);

	return ret;
}

static int
rt9467_charger_set_ovp(struct rt9467_info *info, u32 vol)
{
	return 0;
}

static int
rt9467_charger_set_termina_vol(struct rt9467_info *info, u32 vol)
{

	__rt9467_set_cv(info, vol);
	return 0;
}

static int
rt9467_charger_get_termina_vol(struct rt9467_info *info, u32 *vol)
{
	int ret = 0;
	u8 reg_cv = 0;

	ret = rt9467_i2c_read_byte(info, RT9467_REG_CHG_CTRL4);
	if (ret < 0)
		return ret;

	reg_cv = (ret & RT9467_MASK_CV) >> RT9467_SHIFT_CV;
	*vol = rt9467_closest_value(RT9467_CV_MIN, RT9467_CV_MAX,
		RT9467_CV_STEP, reg_cv);
	dev_info(info->dev, "%s: cv = %d(0x%02X)\n", __func__, *vol, reg_cv);

	return 0;
}

static int
rt9467_charger_set_termina_cur(struct rt9467_info *info, u32 cur)
{
	__rt9467_set_ieoc(info, cur);
	return 0;
}

static int rt9467_charger_hw_init(struct rt9467_info *info)
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




		ret = rt9467_reset_chip(info);
		if (ret < 0) {
			dev_notice(info->dev, "%s: reset chip fail\n", __func__);
		}

		ret = rt9467_init_setting(info);
		if (ret < 0) {
			dev_notice(info->dev, "%s: init setting fail\n", __func__);
		}



		if (info->role == RT9467_ROLE_MASTER_DEFAULT) {
			ret = rt9467_charger_set_ovp(info, RT9467_FCHG_OVP_6V);
			if (ret) {
				dev_err(info->dev, "set rt9467 ovp failed\n");
				return ret;
			}
		} else if (info->role == RT9467_ROLE_SLAVE) {
			ret = rt9467_charger_set_ovp(info, RT9467_FCHG_OVP_9V);
			if (ret<0) {
				dev_err(info->dev, "set rt9467 slave ovp failed\n");
				return ret;
			}
		}

		ret = rt9467_charger_set_vindpm(info, 4500);
		if (ret<0) {
			dev_err(info->dev, "set rt9467 vindpm vol failed\n");
			return ret;
		}

		ret = rt9467_charger_set_termina_vol(info,
						      voltage_max_microvolt);
		if (ret<0) {
			dev_err(info->dev, "set rt9467 terminal vol failed\n");
			return ret;
		}

		ret = rt9467_charger_set_termina_cur(info, termination_cur);
		if (ret<0) {
			dev_err(info->dev, "set rt9467 terminal cur failed\n");
	//		return ret;
		}

		ret = rt9467_charger_set_limit_current(info,
							info->cur.unknown_cur);
		if (ret<0)
			dev_err(info->dev, "set rt9467 limit current failed\n");


	}

	info->current_charge_limit_cur = RT9467_REG_ICHG_LSB * 1000;
	info->current_input_limit_cur = RT9467_REG_IINDPM_LSB * 1000;

	rt9467_dump_regs(info);
	
	return ret;
}

static int
rt9467_charger_get_charge_voltage(struct rt9467_info *info,
				   u32 *charge_vol)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(RT9467_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "failed to get RT9467_BATTERY_NAME\n");
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

static int rt9467_charger_start_charge(struct rt9467_info *info)
{
	int ret = 0;

//	ret = rt9467_update_bits(info, RT9467_REG_0,
//				  RT9467_REG_EN_HIZ_MASK, 0);
//	if (ret)
//		dev_err(info->dev, "disable HIZ mode failed\n");


	if (info->role == RT9467_ROLE_MASTER_DEFAULT) {
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask, 0);
		if (ret) {
			dev_err(info->dev, "enable rt9467 charge failed\n");
			return ret;
		}

		ret = rt9467_enable_charging(info, true);
		if (ret < 0) {
			dev_err(info->dev, "enable rt9467 charge en failed\n");
			return ret;
		}
	} else if (info->role == RT9467_ROLE_SLAVE) {
		gpiod_set_value_cansleep(info->gpiod, 0);
	}

	ret = rt9467_charger_set_limit_current(info,
						info->last_limit_cur);
	if (ret<0) {
		dev_err(info->dev, "failed to set limit current\n");
		return ret;
	}

	ret = rt9467_charger_set_termina_cur(info, info->termination_cur);
	if (ret<0)
		dev_err(info->dev, "set rt9467 terminal cur failed\n");

	return 0;
}

static void rt9467_charger_stop_charge(struct rt9467_info *info)
{
	int ret;
	bool present = rt9467_charger_is_bat_present(info);

	if (info->role == RT9467_ROLE_MASTER_DEFAULT) {
		if (!present || info->need_disable_Q1) {
//			ret = rt9467_update_bits(info, RT9467_REG_0,
//						  RT9467_REG_EN_HIZ_MASK,
//						  0x01 << RT9467_REG_EN_HIZ_SHIFT);
//			if (ret)
//				dev_err(info->dev, "enable HIZ mode failed\n");

			info->need_disable_Q1 = false;
		}

		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask,
					 info->charger_pd_mask);
		if (ret)
			dev_err(info->dev, "disable rt9467 charge failed\n");

		if (info->is_wireless_charge) {
			ret = rt9467_enable_charging(info, false);
			if (ret < 0) 
				dev_err(info->dev, "disable rt9467 charge en failed\n");
		}
	} else if (info->role == RT9467_ROLE_SLAVE) {
//		ret = rt9467_update_bits(info, RT9467_REG_0,
//					  RT9467_REG_EN_HIZ_MASK,
//					  0x01 << RT9467_REG_EN_HIZ_SHIFT);
//		if (ret)
//			dev_err(info->dev, "enable HIZ mode failed\n");

		gpiod_set_value_cansleep(info->gpiod, 1);
	}

	if (info->disable_power_path) {
//		ret = rt9467_update_bits(info, RT9467_REG_0,
//					  RT9467_REG_EN_HIZ_MASK,
//					  0x01 << RT9467_REG_EN_HIZ_SHIFT);
//		if (ret)
//			dev_err(info->dev, "Failed to disable power path\n");
	}

//	if (ret)
//		dev_err(info->dev, "Failed to disable rt9467 watchdog\n");
}

static int rt9467_charger_set_current(struct rt9467_info *info,
				       u32 cur)
{
	__rt9467_set_ichg(info, cur);
	return 0;
}

static int rt9467_charger_get_current(struct rt9467_info *info,
				       u32 *cur)
{
	__rt9467_get_ichg(info, cur);
	return 0;
}

static int
rt9467_charger_set_limit_current(struct rt9467_info *info,
				  u32 limit_cur)
{

	int ret = 0;

	ret = __rt9467_set_aicr(info, limit_cur);

	info->last_limit_cur = limit_cur;

	info->actual_limit_cur = limit_cur;

	return 0;
}

static u32
rt9467_charger_get_limit_current(struct rt9467_info *info,
				  u32 *limit_cur)
{
	int ret = 0;
	u8 reg_aicr = 0;

	ret = rt9467_i2c_read_byte(info, RT9467_REG_CHG_CTRL3);
	if (ret < 0)
		return ret;

	reg_aicr = (ret & RT9467_MASK_AICR) >> RT9467_SHIFT_AICR;
	*limit_cur = rt9467_closest_value(RT9467_AICR_MIN, RT9467_AICR_MAX,
		RT9467_AICR_STEP, reg_aicr);

	return 0;
}

static int rt9467_charger_get_health(struct rt9467_info *info,
				      u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int rt9467_charger_get_online(struct rt9467_info *info,
				      u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}

static int rt9467_charger_feed_watchdog(struct rt9467_info *info,
					 u32 val)
{
	int ret;
	u32 limit_cur = 0;


	rt9467_dump_regs(info);

	ret = rt9467_charger_get_limit_current(info, &limit_cur);
	if (ret<0) {
		dev_err(info->dev, "get limit cur failed\n");
		return ret;
	}

	if (info->actual_limit_cur == limit_cur)
		return 0;

	ret = rt9467_charger_set_limit_current(info, info->actual_limit_cur);
	if (ret<0) {
		dev_err(info->dev, "set limit cur failed\n");
		return ret;
	}


	return 0;
}
#if 0
static irqreturn_t rt9467_int_handler(int irq, void *dev_id)
{
	struct rt9467_info *info = dev_id;

	dev_info(info->dev, "interrupt occurs\n");
//	rt9467_dump_regs(info);

	return IRQ_HANDLED;
}
#endif
static int rt9467_charger_set_fchg_current(struct rt9467_info *info,
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

	ret = rt9467_charger_set_limit_current(info, limit_cur);
	if (ret<0) {
		dev_err(info->dev, "failed to set fchg limit current\n");
		return ret;
	}

	ret = rt9467_charger_set_current(info, cur);
	if (ret<0) {
		dev_err(info->dev, "failed to set fchg current\n");
		return ret;
	}

	return 0;
}

static bool rt9467_charge_done(struct rt9467_info *info)
{
	int ret = false;
	enum rt9467_charging_status chg_stat = RT9467_CHG_STATUS_READY;

	if (info->charging)
	{


		ret = rt9467_get_charging_status(info, &chg_stat);

		/* Return is charging done or not */
		switch (chg_stat) {
		case RT9467_CHG_STATUS_READY:
		case RT9467_CHG_STATUS_PROGRESS:
		case RT9467_CHG_STATUS_FAULT:
			ret = false;
			break;
		case RT9467_CHG_STATUS_DONE:
			ret = true;
			break;
		default:
			ret= false;
			break;
		}
	}
	
	return ret;
}

static int rt9467_charger_get_status(struct rt9467_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static void rt9467_check_wireless_charge(struct rt9467_info *info, bool enable)
{
	int ret;

	if (!enable)
		cancel_delayed_work_sync(&info->cur_work);

	if (info->is_wireless_charge && enable) {
		cancel_delayed_work_sync(&info->cur_work);
		ret = rt9467_charger_set_current(info, info->current_charge_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s:set charge current failed\n", __func__);

		ret = rt9467_charger_set_current(info, info->current_input_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s:set charge current failed\n", __func__);

		pm_wakeup_event(info->dev, RT9467_WAKE_UP_MS);
		schedule_delayed_work(&info->cur_work, RT9467_CURRENT_WORK_MS);
	} else if (info->is_wireless_charge && !enable) {
		info->new_charge_limit_cur = info->current_charge_limit_cur;
		info->current_charge_limit_cur = RT9467_REG_ICHG_LSB * 1000;
		info->new_input_limit_cur = info->current_input_limit_cur;
		info->current_input_limit_cur = RT9467_REG_IINDPM_LSB * 1000;
	} else if (!info->is_wireless_charge && !enable) {
		info->new_charge_limit_cur = RT9467_REG_ICHG_LSB * 1000;
		info->current_charge_limit_cur = RT9467_REG_ICHG_LSB * 1000;
		info->new_input_limit_cur = RT9467_REG_IINDPM_LSB * 1000;
		info->current_input_limit_cur = RT9467_REG_IINDPM_LSB * 1000;
	}
}

static int rt9467_charger_set_status(struct rt9467_info *info,
				      int val)
{
	int ret = 0;
	u32 input_vol;

	if (val == CM_FAST_CHARGE_ENABLE_CMD) {
		ret = rt9467_charger_set_fchg_current(info, val);
		if (ret<0) {
			dev_err(info->dev, "failed to set 9V fast charge current\n");
			return ret;
		}
		ret = rt9467_charger_set_ovp(info, RT9467_FCHG_OVP_9V);
		if (ret<0) {
			dev_err(info->dev, "failed to set fast charge 9V ovp\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_DISABLE_CMD) {
		ret = rt9467_charger_set_fchg_current(info, val);
		if (ret<0) {
			dev_err(info->dev, "failed to set 5V normal charge current\n");
			return ret;
		}
		ret = rt9467_charger_set_ovp(info, RT9467_FCHG_OVP_6V);
		if (ret<0) {
			dev_err(info->dev, "failed to set fast charge 5V ovp\n");
			return ret;
		}
		if (info->role == RT9467_ROLE_MASTER_DEFAULT) {
			ret = rt9467_charger_get_charge_voltage(info, &input_vol);
			if (ret<0) {
				dev_err(info->dev, "failed to get 9V charge voltage\n");
				return ret;
			}
			if (input_vol > RT9467_FAST_CHARGER_VOLTAGE_MAX)
				info->need_disable_Q1 = true;
		}
	} else if ((val == false) &&
		   (info->role == RT9467_ROLE_MASTER_DEFAULT)) {
		ret = rt9467_charger_get_charge_voltage(info, &input_vol);
		if (ret<0) {
			dev_err(info->dev, "failed to get 5V charge voltage\n");
			return ret;
		}
		if (input_vol > RT9467_NORMAL_CHARGER_VOLTAGE_MAX)
			info->need_disable_Q1 = true;
	}

	if (val > CM_FAST_CHARGE_NORMAL_CMD)
		return 0;

	if (!val && info->charging) {
		rt9467_check_wireless_charge(info, false);
		rt9467_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		rt9467_check_wireless_charge(info, true);
		ret = rt9467_charger_start_charge(info);
		if (ret<0)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return 0;
}

static bool rt9467_charger_get_power_path_status(struct rt9467_info *info)
{
	int ret;
	bool power_path_enabled = true;

	ret = rt9467_i2c_test_bit(info, RT9467_REG_CHG_CTRL1,
		RT9467_MASK_HZ_EN, &power_path_enabled);

	if (ret < 0) {
		dev_err(info->dev, "Fail to get power path status, ret = %d\n", ret);
		return power_path_enabled;
	}

	return !power_path_enabled;
}

static int rt9467_charger_set_power_path_status(struct rt9467_info *info, bool enable)
{
	int ret = 0;
	u8 value = 0x1;

	if (enable)
		value = 0;


	ret = rt9467_i2c_update_bits(info, RT9467_REG_CHG_CTRL1,
		value<< RT9467_SHIFT_HZ_EN, 0x04);

	if (ret<0)
		dev_err(info->dev, "%s HIZ mode failed, ret = %d\n",
			enable ? "Enable" : "Disable", ret);

	return 0;
}

static void rt9467_charger_work(struct work_struct *data)
{
	struct rt9467_info *info =
		container_of(data, struct rt9467_info, work);
	bool present = rt9467_charger_is_bat_present(info);

	dev_info(info->dev, "battery present = %d, charger type = %d\n",
		 present, info->usb_phy->chg_type);
	cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}

static void rt9467_current_work(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct rt9467_info *info =
		container_of(dwork, struct rt9467_info, cur_work);
	int ret = 0;
	bool need_return = false;

	if (info->current_charge_limit_cur > info->new_charge_limit_cur) {
		ret = rt9467_charger_set_current(info, info->new_charge_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s: set charge limit cur failed\n", __func__);
		return;
	}

	if (info->current_input_limit_cur > info->new_input_limit_cur) {
		ret = rt9467_charger_set_limit_current(info, info->new_input_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s: set input limit cur failed\n", __func__);
		return;
	}

	if (info->current_charge_limit_cur + RT9467_REG_ICHG_LSB * 1000 <=
	    info->new_charge_limit_cur)
		info->current_charge_limit_cur += RT9467_REG_ICHG_LSB * 1000;
	else
		need_return = true;

	if (info->current_input_limit_cur + RT9467_REG_IINDPM_LSB * 1000 <=
	    info->new_input_limit_cur)
		info->current_input_limit_cur += RT9467_REG_IINDPM_LSB * 1000;
	else if (need_return)
		return;

	ret = rt9467_charger_set_current(info, info->current_charge_limit_cur);
	if (ret < 0) {
		dev_err(info->dev, "set charge limit current failed\n");
		return;
	}

	ret = rt9467_charger_set_limit_current(info, info->current_input_limit_cur);
	if (ret < 0) {
		dev_err(info->dev, "set input limit current failed\n");
		return;
	}

	dev_info(info->dev, "set charge_limit_cur %duA, input_limit_curr %duA\n",
		info->current_charge_limit_cur, info->current_input_limit_cur);

	schedule_delayed_work(&info->cur_work, RT9467_CURRENT_WORK_MS);
}


static int rt9467_charger_usb_change(struct notifier_block *nb,
				      unsigned long limit, void *data)
{
	struct rt9467_info *info =
		container_of(nb, struct rt9467_info, usb_notify);

	info->limit = limit;

	/*
	 * only master should do work when vbus change.
	 * let info->limit = limit, slave will online, too.
	 */
	if (info->role == RT9467_ROLE_SLAVE)
		return NOTIFY_OK;

	pm_wakeup_event(info->dev, RT9467_WAKE_UP_MS);

	schedule_work(&info->work);
	return NOTIFY_OK;
}

static int rt9467_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{
	struct rt9467_info *info = power_supply_get_drvdata(psy);
	u32 cur, online, health, vol,enabled = 0;
	enum usb_charger_type type;
	int ret = 0;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == CM_POWER_PATH_ENABLE_CMD ||
		    val->intval == CM_POWER_PATH_DISABLE_CMD) {
			val->intval = rt9467_charger_get_power_path_status(info);
			break;
		}

		if (info->limit || info->is_wireless_charge)
			val->intval = rt9467_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = rt9467_charger_get_current(info, &cur);
			if (ret<0)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = rt9467_charger_get_limit_current(info, &cur);
			if (ret<0)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = rt9467_charger_get_online(info, &online);
		if (ret<0)
			goto out;

		val->intval = online;

		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = rt9467_charger_get_health(info, &health);
			if (ret<0)
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

	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		if (info->role == RT9467_ROLE_MASTER_DEFAULT) {
			ret = regmap_read(info->pmic, info->charger_pd, &enabled);
			if (ret) {
				dev_err(info->dev, "get rt9467 charge status failed\n");
				goto out;
			}
		} else if (info->role == RT9467_ROLE_SLAVE) {
			enabled = gpiod_get_value_cansleep(info->gpiod);
		}

		val->intval = !enabled;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
			val->intval =rt9467_charge_done(info);
		break;
		
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = rt9467_charger_get_termina_vol(info, &vol);
		val->intval = vol *1000;
		break;

	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int rt9467_charger_usb_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct rt9467_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (info->is_wireless_charge) {
			cancel_delayed_work_sync(&info->cur_work);
			info->new_charge_limit_cur = val->intval;
			pm_wakeup_event(info->dev, RT9467_WAKE_UP_MS);
			schedule_delayed_work(&info->cur_work, RT9467_CURRENT_WORK_MS * 2);
			break;
		}

		ret = rt9467_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (info->is_wireless_charge) {
			cancel_delayed_work_sync(&info->cur_work);
			info->new_input_limit_cur = val->intval;
			pm_wakeup_event(info->dev, RT9467_WAKE_UP_MS);
			schedule_delayed_work(&info->cur_work, RT9467_CURRENT_WORK_MS * 2);
			break;
		}

		ret = rt9467_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;

	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == CM_POWER_PATH_ENABLE_CMD) {
			ret = rt9467_charger_set_power_path_status(info, true);
			break;
		} else if (val->intval == CM_POWER_PATH_DISABLE_CMD) {
			ret = rt9467_charger_set_power_path_status(info, false);
			break;
		}

		ret = rt9467_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;

	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		ret = rt9467_charger_feed_watchdog(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "feed charger watchdog failed\n");
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = rt9467_charger_set_termina_vol(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		rt9467_enable_powerpath(info, val->intval);
		break;

	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		if (val->intval == true) {
			rt9467_check_wireless_charge(info, true);
			ret = rt9467_charger_start_charge(info);
			if (ret<0)
				dev_err(info->dev, "start charge failed\n");
		} else if (val->intval == false) {
			rt9467_check_wireless_charge(info, false);
			rt9467_charger_stop_charge(info);
		}
		break;
	case POWER_SUPPLY_PROP_WIRELESS_TYPE:
		if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP) {
			info->is_wireless_charge = true;
			ret = rt9467_charger_set_ovp(info, RT9467_FCHG_OVP_6V);
		} else if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP) {
			info->is_wireless_charge = true;
			ret = rt9467_charger_set_ovp(info, RT9467_FCHG_OVP_14V);
		} else {
			info->is_wireless_charge = false;
			ret = rt9467_charger_set_ovp(info, RT9467_FCHG_OVP_6V);
		}
		if (ret<0)
			dev_err(info->dev, "failed to set fast charge ovp\n");

		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int rt9467_charger_property_is_writeable(struct power_supply *psy,
						 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
	case POWER_SUPPLY_PROP_WIRELESS_TYPE:
	case POWER_SUPPLY_PROP_STATUS:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_usb_type rt9467_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static enum power_supply_property rt9467_usb_props[] = {
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
};

static const struct power_supply_desc rt9467_charger_desc = {
	.name			= "charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= rt9467_usb_props,
	.num_properties		= ARRAY_SIZE(rt9467_usb_props),
	.get_property		= rt9467_charger_usb_get_property,
	.set_property		= rt9467_charger_usb_set_property,
	.property_is_writeable	= rt9467_charger_property_is_writeable,
	.usb_types		= rt9467_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(rt9467_charger_usb_types),
};

static const struct power_supply_desc rt9467_slave_charger_desc = {
	.name			= "rt9467_slave_charger",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= rt9467_usb_props,
	.num_properties		= ARRAY_SIZE(rt9467_usb_props),
	.get_property		= rt9467_charger_usb_get_property,
	.set_property		= rt9467_charger_usb_set_property,
	.property_is_writeable	= rt9467_charger_property_is_writeable,
	.usb_types		= rt9467_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(rt9467_charger_usb_types),
};

static ssize_t rt9467_register_value_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct rt9467_charger_sysfs *rt9467_sysfs =
		container_of(attr, struct rt9467_charger_sysfs,
			     attr_rt9467_reg_val);
	struct  rt9467_info *info =  rt9467_sysfs->info;
	u8 val;
	int ret;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s  rt9467_sysfs->info is null\n", __func__);

	ret = rt9467_read(info, reg_tab[info->reg_id].addr, &val);
	if (ret) {
		dev_err(info->dev, "fail to get  RT9467_REG_0x%.2x value, ret = %d\n",
			reg_tab[info->reg_id].addr, ret);
		return snprintf(buf, PAGE_SIZE, "fail to get  RT9467_REG_0x%.2x value\n",
			       reg_tab[info->reg_id].addr);
	}

	return snprintf(buf, PAGE_SIZE, "RT9467_REG_0x%.2x = 0x%.2x\n",
			reg_tab[info->reg_id].addr, val);
}

static ssize_t rt9467_register_value_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct rt9467_charger_sysfs *rt9467_sysfs =
		container_of(attr, struct rt9467_charger_sysfs,
			     attr_rt9467_reg_val);
	struct rt9467_info *info = rt9467_sysfs->info;
	u8 val;
	int ret;

	if (!info) {
		dev_err(dev, "%s rt9467_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtou8(buf, 16, &val);
	if (ret) {
		dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	ret = rt9467_write(info, reg_tab[info->reg_id].addr, val);
	if (ret) {
		dev_err(info->dev, "fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
				val, reg_tab[info->reg_id].addr, ret);
		return count;
	}

	dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n", val, reg_tab[info->reg_id].addr);
	return count;
}

static ssize_t rt9467_register_id_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct rt9467_charger_sysfs *rt9467_sysfs =
		container_of(attr, struct rt9467_charger_sysfs,
			     attr_rt9467_sel_reg_id);
	struct rt9467_info *info = rt9467_sysfs->info;
	int ret, id;

	if (!info) {
		dev_err(dev, "%s rt9467_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtoint(buf, 10, &id);
	if (ret) {
		dev_err(info->dev, "%s store register id fail\n", rt9467_sysfs->name);
		return count;
	}

	if (id < 0 || id >= RT9467_REG_NUM) {
		dev_err(info->dev, "%s store register id fail, id = %d is out of range\n",
			rt9467_sysfs->name, id);
		return count;
	}

	info->reg_id = id;

	dev_info(info->dev, "%s store register id = %d success\n", rt9467_sysfs->name, id);
	return count;
}

static ssize_t rt9467_register_id_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct rt9467_charger_sysfs *rt9467_sysfs =
		container_of(attr, struct rt9467_charger_sysfs,
			     attr_rt9467_sel_reg_id);
	struct rt9467_info *info = rt9467_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s rt9467_sysfs->info is null\n", __func__);

	return snprintf(buf, PAGE_SIZE, "Curent register id = %d\n", info->reg_id);
}

static ssize_t rt9467_register_table_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct rt9467_charger_sysfs *rt9467_sysfs =
		container_of(attr, struct rt9467_charger_sysfs,
			     attr_rt9467_lookup_reg);
	struct rt9467_info *info = rt9467_sysfs->info;
	int i, len, idx = 0;
	char reg_tab_buf[2048];

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s rt9467_sysfs->info is null\n", __func__);

	memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
	len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
		       "Format: [id] [addr] [desc]\n");
	idx += len;

	for (i = 0; i < RT9467_REG_NUM; i++) {
		len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
			       "[%d] [REG_0x%.2x] [%s]; \n",
			       reg_tab[i].id, reg_tab[i].addr, reg_tab[i].name);
		idx += len;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", reg_tab_buf);
}

static ssize_t rt9467_dump_register_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct rt9467_charger_sysfs *rt9467_sysfs =
		container_of(attr, struct rt9467_charger_sysfs,
			     attr_rt9467_dump_reg);
	struct rt9467_info *info = rt9467_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s rt9467_sysfs->info is null\n", __func__);

	rt9467_dump_regs(info);

	return snprintf(buf, PAGE_SIZE, "%s\n", rt9467_sysfs->name);
}

static int rt9467_register_sysfs(struct rt9467_info *info)
{
	struct rt9467_charger_sysfs *rt9467_sysfs;
	int ret;

	rt9467_sysfs = devm_kzalloc(info->dev, sizeof(*rt9467_sysfs), GFP_KERNEL);
	if (!rt9467_sysfs)
		return -ENOMEM;

	info->sysfs = rt9467_sysfs;
	rt9467_sysfs->name = "rt9467_sysfs";
	rt9467_sysfs->info = info;
	rt9467_sysfs->attrs[0] = &rt9467_sysfs->attr_rt9467_dump_reg.attr;
	rt9467_sysfs->attrs[1] = &rt9467_sysfs->attr_rt9467_lookup_reg.attr;
	rt9467_sysfs->attrs[2] = &rt9467_sysfs->attr_rt9467_sel_reg_id.attr;
	rt9467_sysfs->attrs[3] = &rt9467_sysfs->attr_rt9467_reg_val.attr;
	rt9467_sysfs->attrs[4] = NULL;
	rt9467_sysfs->attr_g.name = "debug";
	rt9467_sysfs->attr_g.attrs = rt9467_sysfs->attrs;

	sysfs_attr_init(&rt9467_sysfs->attr_rt9467_dump_reg.attr);
	rt9467_sysfs->attr_rt9467_dump_reg.attr.name = "rt9467_dump_reg";
	rt9467_sysfs->attr_rt9467_dump_reg.attr.mode = 0444;
	rt9467_sysfs->attr_rt9467_dump_reg.show = rt9467_dump_register_show;

	sysfs_attr_init(&rt9467_sysfs->attr_rt9467_lookup_reg.attr);
	rt9467_sysfs->attr_rt9467_lookup_reg.attr.name = "rt9467_lookup_reg";
	rt9467_sysfs->attr_rt9467_lookup_reg.attr.mode = 0444;
	rt9467_sysfs->attr_rt9467_lookup_reg.show = rt9467_register_table_show;

	sysfs_attr_init(&rt9467_sysfs->attr_rt9467_sel_reg_id.attr);
	rt9467_sysfs->attr_rt9467_sel_reg_id.attr.name = "rt9467_sel_reg_id";
	rt9467_sysfs->attr_rt9467_sel_reg_id.attr.mode = 0644;
	rt9467_sysfs->attr_rt9467_sel_reg_id.show = rt9467_register_id_show;
	rt9467_sysfs->attr_rt9467_sel_reg_id.store = rt9467_register_id_store;

	sysfs_attr_init(&rt9467_sysfs->attr_rt9467_reg_val.attr);
	rt9467_sysfs->attr_rt9467_reg_val.attr.name = "rt9467_reg_val";
	rt9467_sysfs->attr_rt9467_reg_val.attr.mode = 0644;
	rt9467_sysfs->attr_rt9467_reg_val.show = rt9467_register_value_show;
	rt9467_sysfs->attr_rt9467_reg_val.store = rt9467_register_value_store;

	ret = sysfs_create_group(&info->psy_usb->dev.kobj, &rt9467_sysfs->attr_g);
	if (ret < 0)
		dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static void rt9467_charger_detect_status(struct rt9467_info *info)
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
	if (info->role == RT9467_ROLE_SLAVE)
		return;
	schedule_work(&info->work);
}

static void
rt9467_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct rt9467_info *info = container_of(dwork,
							 struct rt9467_info,
							 wdt_work);

	rt9467_dump_regs(info);

	schedule_delayed_work(&info->wdt_work, HZ * 15);
}

static int rt9467_enable_otg(struct rt9467_info *info, bool en)
{
	int ret = 0;
	bool en_otg = false;
	u8 hidden_val = en ? 0x00 : 0x0F;
	u8 lg_slew_rate = en ? 0xCC : 0xC3;
	u8 reg_ilimit = 0;

	dev_info(info->dev, "%s: en = %d\n", __func__, en);

	rt9467_enable_hidden_mode(info, true);

	/* Set OTG_OC to 500mA */

	reg_ilimit = rt9467_closest_reg_via_tbl(rt9467_boost_oc_threshold,
		ARRAY_SIZE(rt9467_boost_oc_threshold), 500000);

	dev_info(info->dev, "%s: boost ilimit = 500000(0x%02X)\n", __func__,
		 reg_ilimit);

	ret = rt9467_i2c_update_bits(info, RT9467_REG_CHG_CTRL10,
		reg_ilimit << RT9467_SHIFT_BOOST_OC, RT9467_MASK_BOOST_OC);
	if (ret < 0) {
		dev_notice(info->dev, "%s: set current limit fail\n", __func__);
		return ret;
	}

	/*
	 * Woraround : slow Low side mos Gate driver slew rate
	 * for decline VBUS noise
	 * reg[0x23] = 0xCC after entering OTG mode
	 * reg[0x23] = 0xC3 after leaving OTG mode
	 */
	ret = rt9467_i2c_write_byte(info, RT9467_REG_CHG_HIDDEN_CTRL4,
		lg_slew_rate);
	if (ret < 0) {
		dev_notice(info->dev,
			"%s: set Low side mos Gate drive speed fail(%d)\n",
			__func__, ret);
		goto out;
	}

	/* Enable WDT */
	if (en && info->desc->en_wdt) {
		ret = rt9467_enable_wdt(info, true);
		if (ret < 0) {
			dev_notice(info->dev, "%s: en wdt fail\n", __func__);
			goto err_en_otg;
		}
	}

	/* Switch OPA mode */
	ret = (en ? rt9467_set_bit : rt9467_clr_bit)
		(info, RT9467_REG_CHG_CTRL1, RT9467_MASK_OPA_MODE);

	msleep(20);

	if (en) {
		ret = rt9467_i2c_test_bit(info, RT9467_REG_CHG_CTRL1,
			RT9467_SHIFT_OPA_MODE, &en_otg);
		if (ret < 0 || !en_otg) {
			dev_notice(info->dev, "%s: otg fail(%d)\n", __func__,
				ret);
			goto err_en_otg;
		}
	}

	/*
	 * Woraround reg[0x25] = 0x00 after entering OTG mode
	 * reg[0x25] = 0x0F after leaving OTG mode
	 */
	ret = rt9467_i2c_write_byte(info, RT9467_REG_CHG_HIDDEN_CTRL6,
		hidden_val);
	if (ret < 0)
		dev_notice(info->dev, "%s: workaroud fail(%d)\n", __func__,
			ret);

	/* Disable WDT */
	if (!en) {
		ret = rt9467_enable_wdt(info, false);
		if (ret < 0)
			dev_notice(info->dev, "%s: disable wdt fail\n",
				__func__);
	}
	goto out;

err_en_otg:
	/* Disable WDT */
	ret = rt9467_enable_wdt(info, false);
	if (ret < 0)
		dev_notice(info->dev, "%s: disable wdt fail\n", __func__);

	/* Recover Low side mos Gate slew rate */
	ret = rt9467_i2c_write_byte(info, RT9467_REG_CHG_HIDDEN_CTRL4, 0x73);
	if (ret < 0)
		dev_notice(info->dev,
			"%s: recover Low side mos Gate drive speed fail(%d)\n",
			__func__, ret);
	ret = -EIO;
out:
	rt9467_enable_hidden_mode(info, false);
	return ret;
}
#ifdef CONFIG_REGULATOR
static bool rt9467_charger_check_otg_valid(struct rt9467_info *info)
{
	return extcon_get_state(info->edev, EXTCON_USB);
}

static int rt9467_charger_check_otg_fault(struct rt9467_info *info)
{
	int ret;
	bool en_otg;

	ret = rt9467_i2c_test_bit(info, RT9467_REG_CHG_CTRL1,
		RT9467_SHIFT_OPA_MODE, &en_otg);

	if (ret < 0) {
		dev_err(info->dev, "%s;otg fault occurs\n",__func__);
		return ret;
	}

	return en_otg;
}

static void rt9467_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct rt9467_info *info = container_of(dwork,
			struct rt9467_info, otg_work);
	bool otg_valid = rt9467_charger_check_otg_valid(info);
	bool otg_fault;
	int ret, retry = 0;

	if (otg_valid)
		goto out;

	do {
		otg_fault = rt9467_charger_check_otg_fault(info);
		if (!otg_fault) {

			ret = rt9467_set_bit(info, RT9467_REG_CHG_CTRL1, RT9467_MASK_OPA_MODE);
			
			if (ret)
				dev_err(info->dev, "restart rt9467 charger otg failed\n");
		}

		otg_valid = rt9467_charger_check_otg_valid(info);
	} while (!otg_valid && retry++ < RT9467_OTG_RETRY_TIMES);

	if (retry >= RT9467_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int rt9467_charger_enable_otg(struct regulator_dev *dev)
{
	struct rt9467_info *info = rdev_get_drvdata(dev);
	int ret =0;

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


	ret = rt9467_enable_otg(info , true);

	if (ret) {
		dev_err(info->dev, "enable rt9467 otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect,
				   BIT_DP_DM_BC_ENB, 0);
		return ret;
	}

	info->otg_enable = true;
	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(RT9467_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(RT9467_OTG_VALID_MS));

	rt9467_dump_regs(info);

	return 0;
}

static int rt9467_charger_disable_otg(struct regulator_dev *dev)
{
	struct rt9467_info *info = rdev_get_drvdata(dev);
	int ret;
	dev_err(info->dev, "%s\n",__func__);

	info->otg_enable = false;
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	ret = rt9467_enable_otg(info, false);
	if (ret<0) {
		dev_err(info->dev, "disable rt9467 otg failed\n");
		return ret;
	}

	/* Enable charger detection function to identify the charger type */
	return regmap_update_bits(info->pmic, info->charger_detect,
				  BIT_DP_DM_BC_ENB, 0);
}

static int rt9467_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct rt9467_info *info = rdev_get_drvdata(dev);
	int ret;
	bool en_otg;

	ret = rt9467_i2c_test_bit(info, RT9467_REG_CHG_CTRL1,
		RT9467_SHIFT_OPA_MODE, &en_otg);

	if (ret < 0) {
		dev_err(info->dev, "failed to get rt9467 otg status\n");
		return ret;
	}

	return en_otg;
}

static const struct regulator_ops rt9467_charger_vbus_ops = {
	.enable = rt9467_charger_enable_otg,
	.disable = rt9467_charger_disable_otg,
	.is_enabled = rt9467_charger_vbus_is_enabled,
};

static const struct regulator_desc rt9467_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &rt9467_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int
rt9467_charger_register_vbus_regulator(struct rt9467_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &rt9467_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int
rt9467_charger_register_vbus_regulator(struct rt9467_info *info)
{
	return 0;
}
#endif

static int rt9467_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct rt9467_info *info;
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

	rt9467_read(info,RT9467_REG_DEVICE_ID, &val);
	dev_err(dev, "%s;%x;\n",__func__,val);
	if( (val & 0xf0) == RT9467_VENDOR_ID)
	{
		strncpy(charge_ic_vendor_name,"RT9467",20);
		info->chip_rev = val & 0x0F;
		
	}
	else
		return -ENODEV;

	dev_err(dev, "%s;%s;\n",__func__,charge_ic_vendor_name);

	alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);

	i2c_set_clientdata(client, info);
	power_path_control(info);
	ret = device_property_read_bool(dev, "role-slave");
	if (ret)
		info->role = RT9467_ROLE_SLAVE;
	else
		info->role = RT9467_ROLE_MASTER_DEFAULT;

	if (info->role == RT9467_ROLE_SLAVE) {
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

	ret = rt9467_charger_is_fgu_present(info);
	if (ret) {
		dev_err(dev, "sc27xx_fgu not ready.\n");
		return -EPROBE_DEFER;
	}

	/*
	 * only master to support otg
	 */
	if (info->role == RT9467_ROLE_MASTER_DEFAULT) {
		ret = rt9467_charger_register_vbus_regulator(info);
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
			info->charger_pd_mask = RT9467_DISABLE_PIN_MASK_2721;
		else
			info->charger_pd_mask = RT9467_DISABLE_PIN_MASK;
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
	if (info->role == RT9467_ROLE_MASTER_DEFAULT) {
		info->psy_usb = devm_power_supply_register(dev,
							   &rt9467_charger_desc,
							   &charger_cfg);
	} else if (info->role == RT9467_ROLE_SLAVE) {
		info->psy_usb = devm_power_supply_register(dev,
							   &rt9467_slave_charger_desc,
							   &charger_cfg);
	}

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto err_regmap_exit;
	}
#if 0
	info->irq_gpio = of_get_named_gpio(info->dev->of_node, "irq-gpio", 0);
	if (gpio_is_valid(info->irq_gpio)) {
		ret = devm_gpio_request_one(info->dev, info->irq_gpio,
					    GPIOF_DIR_IN, "rt9467_int");
		if (!ret)
			info->client->irq = gpio_to_irq(info->irq_gpio);
		else
			dev_err(dev, "int request failed, ret = %d\n", ret);

		if (info->client->irq < 0) {
			dev_err(dev, "failed to get irq no\n");
			gpio_free(info->irq_gpio);
		} else {
			ret = devm_request_threaded_irq(&info->client->dev, info->client->irq,
							NULL, rt9467_int_handler,
							IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
							"rt9467 interrupt", info);
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

	mutex_init(&info->i2c_access_lock);
	mutex_init(&info->adc_access_lock);
	mutex_init(&info->irq_access_lock);
	mutex_init(&info->aicr_access_lock);
	mutex_init(&info->ichg_access_lock);
	mutex_init(&info->hidden_mode_lock);
	mutex_init(&info->pe_access_lock);
	mutex_init(&info->bc12_access_lock);
	mutex_init(&info->ieoc_lock);
	mutex_init(&info->tchg_lock);
	atomic_set(&info->bc12_sdp_cnt, 0);
	atomic_set(&info->bc12_wkard, 0);

	ret = rt9467_charger_hw_init(info);
	if (ret<0) {
		dev_err(dev, "failed to rt9467_charger_hw_init\n");
		goto err_psy_usb;
	}

	rt9467_charger_stop_charge(info);

	device_init_wakeup(info->dev, true);
	info->usb_notify.notifier_call = rt9467_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		goto err_psy_usb;
	}

	ret = rt9467_register_sysfs(info);
	if (ret) {
		dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
		goto error_sysfs;
	}

	INIT_WORK(&info->work, rt9467_charger_work);
	INIT_DELAYED_WORK(&info->cur_work, rt9467_current_work);

	rt9467_charger_detect_status(info);
	INIT_DELAYED_WORK(&info->otg_work, rt9467_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work,
			  rt9467_charger_feed_watchdog_work);

	dev_err(dev, "rt9467_charger_probe ok to register\n");

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

static void rt9467_charger_shutdown(struct i2c_client *client)
{
	struct rt9467_info *info = i2c_get_clientdata(client);
	int ret = 0;

	cancel_delayed_work_sync(&info->wdt_work);
	if (info->otg_enable) {
		info->otg_enable = false;
		cancel_delayed_work_sync(&info->otg_work);
		ret = rt9467_update_bits(info, RT9467_REG_1,
					  RT9467_REG_OTG_MASK,
					  0);
		if (ret)
			dev_err(info->dev, "disable rt9467 otg failed ret = %d\n", ret);

		/* Enable charger detection function to identify the charger type */
		ret = regmap_update_bits(info->pmic, info->charger_detect,
					 BIT_DP_DM_BC_ENB, 0);
		if (ret)
			dev_err(info->dev,
				"enable charger detection function failed ret = %d\n", ret);
	}
}

static int rt9467_charger_remove(struct i2c_client *client)
{
	struct rt9467_info *info = i2c_get_clientdata(client);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int rt9467_charger_suspend(struct device *dev)
{
	struct rt9467_info *info = dev_get_drvdata(dev);
	ktime_t now, add;
	unsigned int wakeup_ms = RT9467_OTG_ALARM_TIMER_MS;
	int ret;

	if (!info->otg_enable)
		return 0;

	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->cur_work);

	/* feed watchdog first before suspend */
	ret = rt9467_update_bits(info, RT9467_REG_1,
				   RT9467_REG_RESET_MASK,
				   RT9467_REG_RESET_MASK);
	if (ret)
		dev_warn(info->dev, "reset rt9467 failed before suspend\n");

	now = ktime_get_boottime();
	add = ktime_set(wakeup_ms / MSEC_PER_SEC,
			(wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
	alarm_start(&info->otg_timer, ktime_add(now, add));

	return 0;
}

static int rt9467_charger_resume(struct device *dev)
{
	struct rt9467_info *info = dev_get_drvdata(dev);
	int ret;

	if (!info->otg_enable)
		return 0;

	alarm_cancel(&info->otg_timer);

	/* feed watchdog first after resume */
	ret = rt9467_update_bits(info, RT9467_REG_1,
				   RT9467_REG_RESET_MASK,
				   RT9467_REG_RESET_MASK);
	if (ret)
		dev_warn(info->dev, "reset rt9467 failed after resume\n");

	schedule_delayed_work(&info->wdt_work, HZ * 15);
	schedule_delayed_work(&info->cur_work, 0);

	return 0;
}
#endif

static const struct dev_pm_ops rt9467_charger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(rt9467_charger_suspend,
				rt9467_charger_resume)
};

static const struct i2c_device_id rt9467_i2c_id[] = {
	{"rt9467_chg", 0},
	{}
};

static const struct of_device_id rt9467_charger_of_match[] = {
	{ .compatible = "richtek,rt9467_chg", },
	{ }
};

static const struct i2c_device_id rt9467_slave_i2c_id[] = {
	{"rt9467_slave_chg", 0},
	{}
};

static const struct of_device_id rt9467_slave_charger_of_match[] = {
	{ .compatible = "richtek,rt9467_slave_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, rt9467_charger_of_match);
MODULE_DEVICE_TABLE(of, rt9467_slave_charger_of_match);

static struct i2c_driver rt9467_master_charger_driver = {
	.driver = {
		.name = "rt9467_chg",
		.of_match_table = rt9467_charger_of_match,
		.pm = &rt9467_charger_pm_ops,
	},
	.probe = rt9467_charger_probe,
	.remove = rt9467_charger_remove,
	.id_table = rt9467_i2c_id,
};

static struct i2c_driver rt9467_slave_charger_driver = {
	.driver = {
		.name = "rt9467_slave_chg",
		.of_match_table = rt9467_slave_charger_of_match,
		.pm = &rt9467_charger_pm_ops,
	},
	.probe = rt9467_charger_probe,
	.shutdown = rt9467_charger_shutdown,
	.remove = rt9467_charger_remove,
	.id_table = rt9467_slave_i2c_id,
};

module_i2c_driver(rt9467_master_charger_driver);
module_i2c_driver(rt9467_slave_charger_driver);
MODULE_DESCRIPTION("RT9467 Charger Driver");
MODULE_LICENSE("GPL v2");
