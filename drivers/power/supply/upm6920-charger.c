
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

#define UPM6920_DRV_VERSION             "1.0.0_UP"

#define UPM6920_REG_0                   0x00
#define REG00_EN_HIZ_MASK               BIT(7)
#define REG00_EN_HIZ_SHIFT              7
#define REG00_EN_HIZ                    1
#define REG00_EXIT_HIZ                  0
#define REG00_IINDPM_MASK               GENMASK(5, 0)
#define REG00_IINDPM_SHIFT              0
#define REG00_IINDPM_BASE               100
#define REG00_IINDPM_LSB                50
#define REG00_IINDPM_MIN                100
#define REG00_IINDPM_MAX                3250

#define UPM6920_REG_1                   0x01

#define UPM6920_REG_2                   0x02

#define UPM6920_REG_3                   0x03
#define REG03_WD_RST_MASK               BIT(6)
#define REG03_OTG_MASK                  BIT(5)
#define REG03_OTG_SHIFT                 5
#define REG03_OTG_ENABLE                1
#define REG03_OTG_DISABLE               0
#define REG03_CHG_MASK                  BIT(4)
#define REG03_CHG_SHIFT                 4
#define REG03_CHG_ENABLE                1
#define REG03_CHG_DISABLE               0


#define UPM6920_REG_4                   0x04
#define REG04_ICC_MASK                  GENMASK(6, 0)
#define REG04_ICC_SHIFT                 0
#define REG04_ICC_BASE                  0
#define REG04_ICC_LSB                   64
#define REG04_ICC_MIN                   0
#define REG04_ICC_MAX                   5056

#define UPM6920_REG_5                   0x05
#define REG05_ITC_MASK                  GENMASK(7, 4)
#define REG05_ITC_SHIFT                 4
#define REG05_ITC_BASE                  64
#define REG05_ITC_LSB                   64
#define REG05_ITC_MIN                   64
#define REG05_ITC_MAX                   1024
#define REG05_ITERM_MASK                GENMASK(3, 0)
#define REG05_ITERM_SHIFT               0
#define REG05_ITERM_BASE                64
#define REG05_ITERM_LSB                 64
#define REG05_ITERM_MIN                 64
#define REG05_ITERM_MAX                 1024

#define UPM6920_REG_6                   0x06
#define REG06_VREG_MASK                 GENMASK(7, 2)
#define REG06_VREG_SHIFT                2
#define REG06_VREG_BASE                 3840
#define REG06_VREG_LSB                  16
#define REG06_VREG_MIN                  3840
#define REG06_VREG_MAX                  4608
#define REG06_VBAT_LOW_MASK             BIT(1)
#define REG06_VBAT_LOW_SHIFT            1
#define REG06_VBAT_LOW_2P8V             0
#define REG06_VBAT_LOW_3P0V             1
#define REG06_VRECHG_MASK               BIT(0)
#define REG06_VRECHG_SHIFT              0
#define REG06_VRECHG_100MV              0
#define REG06_VRECHG_200MV              1

#define UPM6920_REG_7                   0x07
#define REG07_TWD_MASK                  GENMASK(5, 4)
#define REG07_TWD_SHIFT                 4
#define REG07_TWD_DISABLE               0
#define REG07_TWD_40S                   1
#define REG07_TWD_80S                   2
#define REG07_TWD_160S                  3
#define REG07_EN_TIMER_MASK             BIT(3)
#define REG07_EN_TIMER_SHIFT            3
#define REG07_CHG_TIMER_ENABLE          1
#define REG07_CHG_TIMER_DISABLE         0


#define UPM6920_REG_8                   0x08

#define UPM6920_REG_9                   0x09
#define REG09_BATFET_DIS_MASK           BIT(5)
#define REG09_BATFET_DIS_SHIFT          5
#define REG09_BATFET_ENABLE             0
#define REG09_BATFET_DISABLE            1

#define UPM6920_REG_A                   0x0A
#define UPM6920_REG_BOOST_MASK          GENMASK(2, 0)
#define UPM6920_REG_BOOST_SHIFT         0
#define UPM6920_REG_BOOST_LIMIT_MA      750

#define UPM6920_REG_B                   0x0B

#define UPM6920_REG_C                   0x0C
#define REG0C_OTG_FAULT                 BIT(6)

#define UPM6920_REG_D                   0x0D
#define REG0D_FORCEVINDPM_MASK          BIT(7)
#define REG0D_FORCEVINDPM_SHIFT         7

#define REG0D_VINDPM_MASK               GENMASK(6, 0)
#define REG0D_VINDPM_BASE               2600
#define REG0D_VINDPM_LSB                100
#define REG0D_VINDPM_MIN                3900
#define REG0D_VINDPM_MAX                15300 

#define UPM6920_REG_E                   0x0E
#define UPM6920_REG_F                   0x0F
#define UPM6920_REG_10                  0x10
#define UPM6920_REG_11                  0x11
#define UPM6920_REG_12                  0x12
#define UPM6920_REG_13                  0x13

#define UPM6920_REG_14                  0x14
#define REG14_REG_RST_MASK              BIT(7)
#define REG14_REG_RST_SHIFT             7
#define REG14_REG_RESET                 1
#define REG14_VENDOR_ID_MASK            GENMASK(5, 3)
#define REG14_VENDOR_ID_SHIFT           3
#define UPM6920_VENDOR_ID               3

#define UPM6920_REG_NUM                 21

#define UPM6920_BATTERY_NAME            "sc27xx-fgu"
#define BIT_DP_DM_BC_ENB                BIT(0)

#define UPM6920_DISABLE_PIN_MASK		BIT(0)
#define UPM6920_DISABLE_PIN_MASK_2721		BIT(15)

#define UPM6920_OTG_VALID_MS			500
#define UPM6920_FEED_WATCHDOG_VALID_MS		50
#define UPM6920_OTG_RETRY_TIMES			10

#define UPM6920_ROLE_MASTER_DEFAULT     1
#define UPM6920_ROLE_SLAVE              2

#define UPM6920_FCHG_OVP_6V             6000
#define UPM6920_FCHG_OVP_9V             9000
#define UPM6920_FCHG_OVP_14V            14000

#define UPM6920_FAST_CHARGER_VOLTAGE_MAX    10500000
#define UPM6920_NORMAL_CHARGER_VOLTAGE_MAX  6500000

#define UPM6920_WAKE_UP_MS			1000
#define UPM6920_CURRENT_WORK_MS			100

#define UPM6920_WAIT_WL_VBUS_STABLE_CUR_THR	200000

#define UPM6920_PROBE_TIMEOUT         msecs_to_jiffies(3000)
#define UPM6920_OTG_ALARM_TIMER_S		15
#define UPM6920_WATCH_DOG_TIME_OUT_MS      15000

struct upm6920_charger_sysfs {
    char *name;
    struct attribute_group attr_g;
    struct device_attribute attr_upm6920_dump_reg;
    struct device_attribute attr_upm6920_lookup_reg;
    struct device_attribute attr_upm6920_sel_reg_id;
    struct device_attribute attr_upm6920_reg_val;
    struct device_attribute attr_upm6920_batfet_val;
    struct device_attribute attr_upm6920_hizi_val;
    struct attribute *attrs[7];

    struct upm6920_charger_info *info;
};

struct upm6920_charge_current {
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
};

struct upm6920_charger_info {
    struct i2c_client *client;
    struct device *dev;
    struct power_supply *psy_usb;
    struct upm6920_charge_current cur;
    struct mutex lock;
	struct mutex input_limit_cur_lock;
    struct delayed_work otg_work;
    struct delayed_work wdt_work;
    struct delayed_work cur_work;
    struct regmap *pmic;
    struct gpio_desc *gpiod;
    struct extcon_dev *typec_extcon;
    struct alarm otg_timer;
    struct upm6920_charger_sysfs *sysfs;
	struct completion probe_init;
    u32 charger_detect;
    u32 charger_pd;
    u32 charger_pd_mask;
    u32 new_charge_limit_cur;
    u32 current_charge_limit_cur;
    u32 new_input_limit_cur;
    u32 current_input_limit_cur;
    u32 last_limit_cur;
    u32 actual_limit_voltage;
    u32 actual_limit_cur;
	u32 role;
	u64 last_wdt_time;
	bool charging;
	bool need_disable_Q1;
	int termination_cur;
	bool disable_wdg;
	bool otg_enable;
	unsigned int irq_gpio;
	bool is_wireless_charge;
	bool is_charger_online;
	int reg_id;
	bool disable_power_path;
	bool probe_initialized;
	bool use_typec_extcon;
	bool shutdown_flag;
};

struct upm6920_charger_reg_tab {
    int id;
    u32 addr;
    char *name;
};

static struct upm6920_charger_reg_tab reg_tab[UPM6920_REG_NUM + 1] = {
    {0, UPM6920_REG_0, "EN_HIZ/EN_ILIM/IINDPM"},
    {1, UPM6920_REG_1, "DP_DRIVE/DM_DRIVE/EN_12V/VINDPM_OS"},
    {2, UPM6920_REG_2, "CONV_START/CONV_RATE/BOOST_FRE/ICO_EN/HVDCP_EN/FORCE_DPD/AUTO_DPDM_EN"},
    {3, UPM6920_REG_3, "FORCE_DSEL/WD_RST/OTG_CFG/CHG_CFG/VSYS_MIN/VBATMIN_SEL"},
    {4, UPM6920_REG_4, "ICC"},
    {5, UPM6920_REG_5, "ITC/ITERM"},
    {6, UPM6920_REG_6, "CV/VBAT_LOW/VRECHG"},
    {7, UPM6920_REG_7, "EN_TERM/STAT_DIS/TWD/EN_TIMER/TCHG/JEITA_ISET"},
    {8, UPM6920_REG_8, "BAT_COMP/VCLAMP/TJREG"},
    {9, UPM6920_REG_9, "FORCE_ICO/TMR2X_EN/BATFET_DIS/JEITA_VSET_WARM/BATGET_DLY/BATFET_RST_EN"},
    {10, UPM6920_REG_A, "V_OTG/PFM_OTG_DIS/IBOOST_LIM"},
    {11, UPM6920_REG_B, "VBUS_STAT/CHRG_STAT/PG_STAT/VSYS_STAT"},
    {12, UPM6920_REG_C, "JWD_FAULT/OTG_FAULT/CHRG_FAULT/BAT_FAULT/NTC_FAULT"},
    {13, UPM6920_REG_D, "FORCE_VINDPM/VINDPM"},
    {14, UPM6920_REG_E, "THERMAL_STAT/VBAT"},
    {15, UPM6920_REG_F, "VSYS"},
    {16, UPM6920_REG_10, "NTC"},
    {17, UPM6920_REG_11, "VBUS_GD/VBUS"},
    {18, UPM6920_REG_12, "ICC"},
    {19, UPM6920_REG_13, "VINDPM_STAT/IINDPM_STAT/IDPM_ICO"},
    {20, UPM6920_REG_14, "REG_RST/ICO_STAT/PN/NTC_PROFILE/DEV_VERSION"},
    {21, 0, "null"},
};

static bool enable_dump_stack;
module_param(enable_dump_stack, bool, 0644);

#ifndef OTG_USE_REGULATOR
static int upm6920_charger_enable_otg(struct upm6920_charger_info *info);
static int upm6920_charger_disable_otg(struct upm6920_charger_info *info);
static int upm6920_charger_vbus_is_enabled(struct upm6920_charger_info *info);
#endif
static void upm6920_charger_dump_stack(void)
{
	if (enable_dump_stack)
		dump_stack();
}
static void power_path_control(struct upm6920_charger_info *info)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	int ret;
	char *match;
	char result[5];

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (ret) {
		info->disable_power_path = false;
		return;
	}

	if (strncmp(cmd_line, "charger", strlen("charger")) == 0)
		info->disable_power_path = true;

	match = strstr(cmd_line, "sprdboot.mode=");
	if (match) {
		memcpy(result, (match + strlen("sprdboot.mode=")),
			sizeof(result) - 1);
		if ((!strcmp(result, "cali")) || (!strcmp(result, "auto")))
			info->disable_power_path = true;
	}
    pr_err("%s:line%d: \n", __func__, __LINE__);
}

static int
upm6920_charger_set_limit_current(struct upm6920_charger_info *info,
                u32 limit_cur, bool enable);
static u32 upm6920_charger_get_limit_current(struct upm6920_charger_info *info,
					     u32 *limit_cur);

static bool upm6920_charger_is_bat_present(struct upm6920_charger_info *info)
{
    struct power_supply *psy;
    union power_supply_propval val;
    bool present = false;
    int ret;

    psy = power_supply_get_by_name(UPM6920_BATTERY_NAME);
    if (!psy) {
        dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
        return present;
    }

	val.intval = 0;
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

static int upm6920_charger_is_fgu_present(struct upm6920_charger_info *info)
{
    struct power_supply *psy;

    psy = power_supply_get_by_name(UPM6920_BATTERY_NAME);
    if (!psy) {
        dev_err(info->dev, "Failed to find psy of sc27xx_fgu\n");
        return -ENODEV;
    }
    power_supply_put(psy);

    return 0;
}

static int __upm6920_read_reg(struct upm6920_charger_info *info, u8 reg, u8 *data)
{
	int ret;

    ret = i2c_smbus_read_byte_data(info->client, reg);
    if (ret < 0) {
        pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
        return ret;
    }
    *data = (u8)ret;

    return 0;
}

static int __upm6920_write_reg(struct upm6920_charger_info *info, int reg, u8 val)
{
    s32 ret;
    ret = i2c_smbus_write_byte_data(info->client, reg, val);
    if (ret < 0) {
        pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
                val, reg, ret);
        return ret;
    }
    return 0;
}

static int upm6920_read(struct upm6920_charger_info *info, u8 reg, u8 *data)
{
    int ret;

    //mutex_lock(&info->i2c_rw_lock);
    ret = __upm6920_read_reg(info, reg, data);
    //mutex_unlock(&info->i2c_rw_lock);

    return ret;
}

static int upm6920_write(struct upm6920_charger_info *info, u8 reg, u8 data)
{
    int ret;

    //mutex_lock(&info->i2c_rw_lock);
    ret = __upm6920_write_reg(info, reg, data);
    //mutex_unlock(&info->i2c_rw_lock);

    if (ret) {
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
    }

    return ret;
}

static int upm6920_update_bits(struct upm6920_charger_info *info, u8 reg,
                u8 mask, u8 data)
{
    u8 v;
    int ret;

    //mutex_lock(&info->i2c_rw_lock);
    ret = __upm6920_read_reg(info, reg, &v);
    if (ret) {
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
        goto out;
    }
    v &= ~mask;
    v |= (data & mask);

    ret = __upm6920_write_reg(info, reg, v);
    if (ret) {
        pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
    }
out:
    //mutex_unlock(&info->i2c_rw_lock);
    return ret;
}

static int upm6920_charger_get_vendor_id_part_value(struct upm6920_charger_info *info)
{
    u8 reg_val;
    u8 reg_part_val;
    int ret;

    ret = __upm6920_read_reg(info, UPM6920_REG_14, &reg_val);
    if (ret < 0) {
        dev_err(info->dev, "Failed to get vendor id, ret = %d\n", ret);
        return ret;
    }
    reg_part_val = reg_val;

    reg_val &= REG14_VENDOR_ID_MASK;
    reg_val >>= REG14_VENDOR_ID_SHIFT;
    if (reg_val != UPM6920_VENDOR_ID) {
        dev_err(info->dev, "The vendor id is 0x%x\n", reg_val);
        return -EINVAL;
    }

    return 0;
}

static int upm6920_charger_force_vindpm(struct upm6920_charger_info *info)
{
    return upm6920_update_bits(info, UPM6920_REG_D,
                REG0D_FORCEVINDPM_MASK, REG0D_FORCEVINDPM_MASK);
}

static int upm6920_charger_set_vindpm(struct upm6920_charger_info *info, 
            u32 vol)
{
    u8 reg_val;

    if (vol < REG0D_VINDPM_MIN)
        vol = REG0D_VINDPM_MIN;
    else if (vol > REG0D_VINDPM_MAX)
        vol = REG0D_VINDPM_MAX;

    reg_val = (vol - REG0D_VINDPM_BASE) / REG0D_VINDPM_LSB;

	upm6920_charger_force_vindpm(info);

    return upm6920_update_bits(info, UPM6920_REG_D,
                REG0D_VINDPM_MASK, reg_val);
}

static int upm6920_charger_increase_ocp_current(struct upm6920_charger_info *info) 
{
	int ret = 0;

	ret = upm6920_write(info, 0xa9, 0x6e);
	if (ret)
        dev_err(info->dev, "upm6920 write reg_a9 6e failed, ret:%d\n", ret);
	ret = upm6920_update_bits(info, 0xd3, 0xe0, 0x80);
	if (ret)
        dev_err(info->dev, "upm6920 write reg_d3 failed, ret:%d\n", ret);
	ret = upm6920_write(info, 0xa9, 0x00);
	if (ret)
        dev_err(info->dev, "upm6920 write reg_a9 00 failed, ret:%d\n", ret);

    dev_err(info->dev, "upm6920 increase ocp current\n");
	return ret;
}

static int upm6920_charger_set_ovp(struct upm6920_charger_info *info, 
            u32 vol)
{
    //default 14V
    return 0;
}

static int upm6920_charger_set_termina_vol(struct upm6920_charger_info *info, 
            u32 volt)
{
    int ret;
    u8 reg_val;
    
    if (volt < REG06_VREG_MIN)
        volt = REG06_VREG_MIN;
    else if (volt > REG06_VREG_MAX)
        volt = REG06_VREG_MAX;

    reg_val = (volt - REG06_VREG_BASE) / REG06_VREG_LSB;

    ret = upm6920_update_bits(info, UPM6920_REG_6, REG06_VREG_MASK,
                reg_val << REG06_VREG_SHIFT);
    if (ret != 0) {
        dev_err(info->dev, "upm6920 set failed\n");
    } else {
        info->actual_limit_voltage = 
            (reg_val * REG06_VREG_LSB) + REG06_VREG_BASE;
        dev_err(info->dev, "upm6920 set success, the value is %d\n", 
            info->actual_limit_voltage);
    }

    return ret;
}

static int upm6920_charger_set_termina_cur(struct upm6920_charger_info *info, 
            u32 curr)
{
    u8 reg_val;

    if (curr < REG05_ITERM_MIN)
        curr = REG05_ITERM_MIN;
    else if (curr > REG05_ITERM_MAX)
        curr = REG05_ITERM_MAX;
    
    reg_val = (curr - REG05_ITERM_BASE) / REG05_ITERM_LSB;

    return upm6920_update_bits(info, UPM6920_REG_5,
                REG05_ITERM_MASK, reg_val << REG05_ITERM_SHIFT);
}

#if 0
static int upm6920_charger_set_recharge(struct upm6920_charger_info *info, 
            u32 mv)
{
    u8 reg_val = REG06_VRECHG_200MV;

    if (mv < 200) {
        reg_val = REG06_VRECHG_100MV;
    }

    return upm6920_update_bits(info, UPM6920_REG_6,
                REG06_VRECHG_MASK,
                reg_val << REG06_VRECHG_SHIFT);
}
#endif

static int upm6920_charger_en_chg_timer(struct upm6920_charger_info *info, 
            bool val)
{
    int ret = 0;
    u8 reg_val = val ? REG07_CHG_TIMER_ENABLE : REG07_CHG_TIMER_DISABLE;

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    pr_info("UPM6920 EN_TIMER is %s\n", val ? "enable" : "disable");

    ret = upm6920_update_bits(info, UPM6920_REG_7,
            REG07_EN_TIMER_MASK,
            reg_val << REG07_EN_TIMER_SHIFT);
    if (ret) {
        pr_err("%s: set UPM6920 chg_timer failed\n", __func__);
    }

    return ret;
}

static int upm6920_charger_set_wd_timer(struct upm6920_charger_info *info,
            int time)
{
    u8 reg_val;

    if (time == 0) {
        reg_val = REG07_TWD_DISABLE;
    } else if (time <= 40) {
        reg_val = REG07_TWD_40S;
    } else if (time <= 80) {
        reg_val = REG07_TWD_80S;
    } else {
        reg_val = REG07_TWD_160S;
    }

    return upm6920_update_bits(info, UPM6920_REG_7, REG07_TWD_MASK, 
                reg_val << REG07_TWD_SHIFT);
}

static int upm6920_otg_boost_lim(struct upm6920_charger_info *info,
            u32 cur)
{
    u8 reg_val = 0;

    if (cur == 750)
        reg_val = 1;
    else
        reg_val = 4;

    return upm6920_update_bits(info, UPM6920_REG_A, UPM6920_REG_BOOST_MASK,
                reg_val);
}

static int upm6920_charger_set_chg_en(struct upm6920_charger_info *info, 
            bool enable)
{
    u8 reg_val = enable ? REG03_CHG_ENABLE : REG03_CHG_DISABLE;

    return upm6920_update_bits(info, UPM6920_REG_3, REG03_CHG_MASK, 
                reg_val << REG03_CHG_SHIFT);
}

static int upm6920_charger_set_otg_en(struct upm6920_charger_info *info, 
            bool enable)
{
    u8 reg_val = enable ? REG03_OTG_ENABLE : REG03_OTG_DISABLE;

    return upm6920_update_bits(info, UPM6920_REG_3, REG03_OTG_MASK, 
                reg_val << REG03_OTG_SHIFT);
}

static int upm6920_charger_hw_init(struct upm6920_charger_info *info)
{
	struct sprd_battery_info bat_info = {};
	int voltage_max_microvolt, termination_cur;
	int ret;

	ret = sprd_battery_get_battery_info(info->psy_usb, &bat_info);
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");
        pr_err("%s:ret=%d line%d: \n", __func__, ret, __LINE__);
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
        sprd_battery_put_battery_info(info->psy_usb, &bat_info);
	}

    ret = upm6920_update_bits(info, UPM6920_REG_14,
                REG14_REG_RST_MASK,
                REG14_REG_RESET << REG14_REG_RST_SHIFT);
    if (ret) {
        dev_err(info->dev, "reset upm6920 failed\n");
        return ret;
    }

    pr_err("%s:ret=%d line%d: \n", __func__, ret, __LINE__);
    if (info->role == UPM6920_ROLE_MASTER_DEFAULT) {
        ret = upm6920_charger_set_ovp(info, UPM6920_FCHG_OVP_6V);
        if (ret) {
            dev_err(info->dev, "set upm6920 ovp failed\n");
            return ret;
        }
    } else if (info->role == UPM6920_ROLE_SLAVE) {
        ret = upm6920_charger_set_ovp(info, UPM6920_FCHG_OVP_9V);
        if (ret) {
            dev_err(info->dev, "set upm6920 slave ovp failed\n");
            return ret;
        }
    }

	ret = upm6920_charger_increase_ocp_current(info);
    if (ret) {
        dev_err(info->dev, "set upm6920 ocp failed\n");
        return ret;
	}

    ret = upm6920_charger_set_vindpm(info, 4500);
    if (ret) {
        dev_err(info->dev, "set upm6920 vindpm vol failed\n");
        return ret;
    }

    ret = upm6920_charger_set_termina_vol(info, voltage_max_microvolt);
    if (ret) {
        dev_err(info->dev, "set upm6920 terminal vol failed\n");
        return ret;
    }

    ret = upm6920_charger_set_termina_cur(info, termination_cur);
    if (ret) {
        dev_err(info->dev, "set upm6920 terminal cur failed\n");
        return ret;
    }

    ret = upm6920_charger_set_limit_current(info, info->cur.unknown_cur, false);
    if (ret)
        dev_err(info->dev, "set upm6920 limit current failed\n");

    ret = upm6920_charger_en_chg_timer(info, false);
    if (ret)
        dev_err(info->dev, "failed to disable chg_timer\n");

    ret = upm6920_charger_set_wd_timer(info, 0);
    if (ret)
        dev_err(info->dev, "failed to disable wdt\n");

    ret = upm6920_otg_boost_lim(info, UPM6920_REG_BOOST_LIMIT_MA);
    if (ret)
        dev_err(info->dev, "failed to set boost lim\n");

    info->current_charge_limit_cur = 2020 * 1000;
    info->current_input_limit_cur = REG00_IINDPM_LSB * 1000;

    dev_err(info->dev, "init upm6920 unisemipower\n");
    return ret;
}

static int upm6920_enter_hiz_mode(struct upm6920_charger_info *info)
{
    int ret;

    ret = upm6920_update_bits(info, UPM6920_REG_0,
                REG00_EN_HIZ_MASK, REG00_EN_HIZ << REG00_EN_HIZ_SHIFT);
    if (ret)
        dev_err(info->dev, "enter HIZ mode failed\n");

    return ret;
}

static int upm6920_exit_hiz_mode(struct upm6920_charger_info *info)
{
    int ret;

    ret = upm6920_update_bits(info, UPM6920_REG_0,
                REG00_EN_HIZ_MASK, REG00_EXIT_HIZ << REG00_EN_HIZ_SHIFT);
    if (ret)
        dev_err(info->dev, "exit HIZ mode failed\n");

    return ret;
}

static int upm6920_charger_get_charge_voltage(struct upm6920_charger_info *info,
            u32 *charge_vol)
{
    struct power_supply *psy;
    union power_supply_propval val;
    int ret;

    psy = power_supply_get_by_name(UPM6920_BATTERY_NAME);
    if (!psy) {
        dev_err(info->dev, "failed to get UPM6920_BATTERY_NAME\n");
        return -ENODEV;
    }

	val.intval = 0;
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

static int upm6920_charger_start_charge(struct upm6920_charger_info *info)
{
    int ret = 0;

    ret = upm6920_exit_hiz_mode(info);
    if (ret) {
        return ret;
    }

    ret = upm6920_charger_set_wd_timer(info, 0);
    if (ret) {
        dev_err(info->dev, "Failed to disable upm6920 watchdog\n");
        return ret;
    }

    if (info->role == UPM6920_ROLE_MASTER_DEFAULT) {
        ret = regmap_update_bits(info->pmic, info->charger_pd,
                    info->charger_pd_mask, 0);
        if (ret) {
            dev_err(info->dev, "enable upm6920 charge failed\n");
            return ret;
        }

        ret = upm6920_charger_set_chg_en(info , true);
        if (ret) {
            dev_err(info->dev, "enable upm6920 charge en failed\n");
            return ret;
        }
    } else if (info->role == UPM6920_ROLE_SLAVE) {
        gpiod_set_value_cansleep(info->gpiod, 0);
    }

    ret = upm6920_charger_set_limit_current(info,
                        info->last_limit_cur, false);
    if (ret) {
        dev_err(info->dev, "failed to set limit current\n");
        return ret;
    }

    ret = upm6920_charger_set_termina_cur(info, info->termination_cur);
    if (ret)
        dev_err(info->dev, "set upm6920 terminal cur failed\n");
    return ret;
}

static void upm6920_charger_stop_charge(struct upm6920_charger_info *info, bool present)
{
    int ret;

	dev_info(info->dev, "%s:line%d: stop charge\n", __func__, __LINE__);

    if (info->role == UPM6920_ROLE_MASTER_DEFAULT) {
        if (!present || info->need_disable_Q1) {
            ret = upm6920_enter_hiz_mode(info);
            if (ret)
                dev_err(info->dev, "enable HIZ mode failed\n");

            info->need_disable_Q1 = false;
        }

        ret = regmap_update_bits(info->pmic, info->charger_pd,
                    info->charger_pd_mask,
                    info->charger_pd_mask);
        if (ret)
            dev_err(info->dev, "disable upm6920 charge failed\n");

        if (info->is_wireless_charge) {
            ret = upm6920_charger_set_chg_en(info, false);
            if (ret)
                dev_err(info->dev, "disable upm6920 charge en failed\n");
        }
    } else if (info->role == UPM6920_ROLE_SLAVE) {
        ret = upm6920_enter_hiz_mode(info);
        if (ret)
            dev_err(info->dev, "enable HIZ mode failed\n");

        gpiod_set_value_cansleep(info->gpiod, 1);
    }

    if (info->disable_power_path) {
        ret = upm6920_enter_hiz_mode(info);
        if (ret)
            dev_err(info->dev, "Failed to disable power path\n");
    }

    ret = upm6920_charger_set_wd_timer(info, 0);
    if (ret)
        dev_err(info->dev, "Failed to disable upm6920 watchdog\n");
}

static int upm6920_charger_set_current(struct upm6920_charger_info *info,
                    u32 uA)
{
    u8 reg_val;
    
    uA = uA / 1000;
    dev_err(info->dev, "upm6920 set_current %d\n", uA);
    if (uA < REG04_ICC_MIN) {
        uA = REG04_ICC_MIN;
    } else if (uA > REG04_ICC_MAX) {
        uA = REG04_ICC_MAX;
    }

    reg_val = (uA - REG04_ICC_BASE) / REG04_ICC_LSB;

    return upm6920_update_bits(info, UPM6920_REG_4, REG04_ICC_MASK, 
                reg_val << REG04_ICC_SHIFT);
}

static int upm6920_charger_get_current(struct upm6920_charger_info *info,
            u32 *cur)
{
    u8 reg_val;
    int ret;

    ret = upm6920_read(info, UPM6920_REG_4, &reg_val);
    if (ret < 0)
        return ret;

    reg_val = (reg_val & REG04_ICC_MASK) >> REG04_ICC_SHIFT;
    
    *cur = (reg_val * REG04_ICC_LSB + REG04_ICC_BASE) * 1000;

    return 0;
}

static int upm6920_charger_set_limit_current(struct upm6920_charger_info *info,
            u32 limit_cur, bool enable)
{
	u8 reg_val;
	int ret = 0;

	mutex_lock(&info->input_limit_cur_lock);
	if (enable) {
		ret = upm6920_charger_get_limit_current(info, &limit_cur);
		if (ret) {
			dev_err(info->dev, "get limit cur failed\n");
			goto out;
		}

		if (limit_cur == info->actual_limit_cur)
			goto out;
		limit_cur = info->actual_limit_cur;
	}

	dev_dbg(info->dev, "%s:line%d: set limit cur = %d\n", __func__, __LINE__, limit_cur);
    info->last_limit_cur = limit_cur;
	limit_cur -= (REG00_IINDPM_BASE * 1000);
    limit_cur = limit_cur / 1000;
    if (limit_cur < REG00_IINDPM_MIN) {
        limit_cur = REG00_IINDPM_MIN;
    } else if (limit_cur > REG00_IINDPM_MAX) {
        limit_cur = REG00_IINDPM_MAX;
    }

    reg_val = limit_cur / REG00_IINDPM_LSB;

    ret = upm6920_update_bits(info, UPM6920_REG_0, REG00_IINDPM_MASK,
                reg_val << REG00_IINDPM_SHIFT);
    if (ret)
        dev_err(info->dev, "set upm6920 limit cur failed\n");

	dev_info(info->dev, "set limit current reg_val = %#x, actual_limit_cur = %d\n",
		 reg_val, info->actual_limit_cur);

out:
	mutex_unlock(&info->input_limit_cur_lock);

    return ret;
}

static u32 upm6920_charger_get_limit_current(struct upm6920_charger_info *info,
                u32 *limit_cur)
{
    u8 reg_val;
    int ret;

    ret = upm6920_read(info, UPM6920_REG_0, &reg_val);
    if (ret < 0)
        return ret;

    reg_val = (reg_val & REG00_IINDPM_MASK) >> REG00_IINDPM_SHIFT;
    *limit_cur = (reg_val * REG00_IINDPM_LSB + REG00_IINDPM_BASE) * 1000;
	dev_err(info->dev, "upm6920_charger_get_limit_current =  %d\n",*limit_cur);

    return 0;
}

static int upm6920_charger_get_health(struct upm6920_charger_info *info, u32 *health)
{
    *health = POWER_SUPPLY_HEALTH_GOOD;

    return 0;
}

static void upm6920_dump_register(struct upm6920_charger_info *info)
{
    int i, ret, len, idx = 0;
    u8 reg_val;
    char buf[500];

    memset(buf, '\0', sizeof(buf));
    for (i = 0; i < UPM6920_REG_NUM; i++) {
        ret = upm6920_read(info, reg_tab[i].addr, &reg_val);
        if (ret == 0) {
            len = snprintf(buf + idx, sizeof(buf) - idx,
                    "[REG_0x%.2x]=0x%.2x  ",
                    reg_tab[i].addr, reg_val);
            idx += len;
        }
    }

    dev_err(info->dev, "%s: %s", __func__, buf);
}


static irqreturn_t upm6920_int_handler(int irq, void *dev_id)
{
    struct upm6920_charger_info *info = dev_id;

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return IRQ_HANDLED;
    }

    dev_info(info->dev, "interrupt occurs\n");
    upm6920_dump_register(info);

    return IRQ_HANDLED;
}



static int upm6920_charger_get_status(struct upm6920_charger_info *info)
{
    if (info->charging)
        return POWER_SUPPLY_STATUS_CHARGING;
    else
        return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static bool upm6920_charger_get_power_path_status(struct upm6920_charger_info *info)
{
	u8 value;
	int ret;
	bool power_path_enabled = true;

	ret = upm6920_read(info, UPM6920_REG_0, &value);
	if (ret < 0) {
		dev_err(info->dev, "Fail to get power path status, ret = %d\n", ret);
		return power_path_enabled;
	}

	if (value & REG00_EN_HIZ_MASK)
		power_path_enabled = false;

	return power_path_enabled;
}


static void upm6920_check_wireless_charge(struct upm6920_charger_info *info, bool enable)
{
    int ret;

    if (!enable)
        cancel_delayed_work_sync(&info->cur_work);

    if (info->is_wireless_charge && enable) {
        cancel_delayed_work_sync(&info->cur_work);
        ret = upm6920_charger_set_current(info, info->current_charge_limit_cur);
        if (ret < 0)
            dev_err(info->dev, "%s:set charge current failed\n", __func__);

        ret = upm6920_charger_set_current(info, info->current_input_limit_cur);
        if (ret < 0)
            dev_err(info->dev, "%s:set charge current failed\n", __func__);

        pm_wakeup_event(info->dev, UPM6920_WAKE_UP_MS);
        schedule_delayed_work(&info->cur_work, msecs_to_jiffies(UPM6920_CURRENT_WORK_MS));
    } else if (info->is_wireless_charge && !enable) {
        info->new_charge_limit_cur = info->current_charge_limit_cur;
        info->current_charge_limit_cur = REG04_ICC_LSB * 1000;
        info->new_input_limit_cur = info->current_input_limit_cur;
        info->current_input_limit_cur = REG00_IINDPM_LSB * 1000;
    } else if (!info->is_wireless_charge && !enable) {
        info->new_charge_limit_cur = REG04_ICC_LSB * 1000;
        info->current_charge_limit_cur = REG04_ICC_LSB * 1000;
        info->new_input_limit_cur = REG00_IINDPM_LSB * 1000;
        info->current_input_limit_cur = REG00_IINDPM_LSB * 1000;
    }
}

static int upm6920_charger_set_status(struct upm6920_charger_info *info,
                    int val, u32 input_vol, bool bat_present)
{
    int ret = 0;

    if (val == CM_FAST_CHARGE_OVP_ENABLE_CMD) {
        ret = upm6920_charger_set_ovp(info, UPM6920_FCHG_OVP_9V);
        if (ret) {
            dev_err(info->dev, "failed to set fast charge 9V ovp\n");
            return ret;
        }
    } else if (val == CM_FAST_CHARGE_OVP_DISABLE_CMD) {
        ret = upm6920_charger_set_ovp(info, UPM6920_FCHG_OVP_6V);
        if (ret) {
            dev_err(info->dev, "failed to set fast charge 5V ovp\n");
            return ret;
        }
        if (info->role == UPM6920_ROLE_MASTER_DEFAULT) {
            if (input_vol > UPM6920_FAST_CHARGER_VOLTAGE_MAX)
                info->need_disable_Q1 = true;
        }
    } else if ((val == false) &&
        (info->role == UPM6920_ROLE_MASTER_DEFAULT)) {
        if (input_vol > UPM6920_NORMAL_CHARGER_VOLTAGE_MAX)
            info->need_disable_Q1 = true;
    }

    if (val > CM_FAST_CHARGE_NORMAL_CMD)
        return 0;

    if (!val && info->charging) {
        upm6920_check_wireless_charge(info, false);
        upm6920_charger_stop_charge(info, bat_present);
        info->charging = false;
        pr_err("%s:line info->charging = false val->intval =%d \n", __func__, val);
    } else if (val && !info->charging) {
        upm6920_check_wireless_charge(info, true);
        ret = upm6920_charger_start_charge(info);
        if (ret)
            dev_err(info->dev, "start charge failed\n");
        else
            info->charging = true;
        pr_err("%s:line info->charging = true val->intval =%d \n", __func__, val);
    }

    return ret;
}

static void upm6920_current_work(struct work_struct *data)
{
    struct delayed_work *dwork = to_delayed_work(data);
    struct upm6920_charger_info *info =
        container_of(dwork, struct upm6920_charger_info, cur_work);
    int ret = 0, delay_work_ms = 10 * UPM6920_CURRENT_WORK_MS;
    bool need_return = false;

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return;
    }

    if (info->current_charge_limit_cur > info->new_charge_limit_cur) {
        ret = upm6920_charger_set_current(info, info->new_charge_limit_cur);
        if (ret < 0)
            dev_err(info->dev, "%s: set charge limit cur failed\n", __func__);
        return;
    }

    if (info->current_input_limit_cur > info->new_input_limit_cur) {
        ret = upm6920_charger_set_limit_current(info, info->new_input_limit_cur, false);
        if (ret < 0)
            dev_err(info->dev, "%s: set input limit cur failed\n", __func__);
        return;
    }

    if (info->current_charge_limit_cur + REG04_ICC_LSB * 1000 <=
        info->new_charge_limit_cur)
        info->current_charge_limit_cur += REG04_ICC_LSB * 1000;
    else
        need_return = true;

    if (info->current_input_limit_cur + REG00_IINDPM_LSB * 1000 <=
        info->new_input_limit_cur)
        info->current_input_limit_cur += REG00_IINDPM_LSB * 1000;
    else if (need_return)
        return;

    ret = upm6920_charger_set_current(info, info->current_charge_limit_cur);
    if (ret < 0) {
        dev_err(info->dev, "set charge limit current failed\n");
        return;
    }

    ret = upm6920_charger_set_limit_current(info, info->current_input_limit_cur, false);
    if (ret < 0) {
        dev_err(info->dev, "set input limit current failed\n");
        return;
    }
    upm6920_dump_register(info);
    dev_info(info->dev, "set charge_limit_cur %duA, input_limit_curr %duA\n",
        info->current_charge_limit_cur, info->current_input_limit_cur);

	if (info->current_charge_limit_cur < UPM6920_WAIT_WL_VBUS_STABLE_CUR_THR)
		delay_work_ms = UPM6920_CURRENT_WORK_MS * 50;

	schedule_delayed_work(&info->cur_work, msecs_to_jiffies(delay_work_ms));
}

static bool upm6920_probe_is_ready(struct upm6920_charger_info *info)
{
	unsigned long timeout;

	if (unlikely(!info->probe_initialized)) {
		timeout = wait_for_completion_timeout(&info->probe_init, UPM6920_PROBE_TIMEOUT);
		if (!timeout) {
			dev_err(info->dev, "%s wait probe timeout\n", __func__);
			return false;
		}
	}

	return true;
}

static int upm6920_charger_usb_get_property(struct power_supply *psy,
                        enum power_supply_property psp,
                        union power_supply_propval *val)
{
    struct upm6920_charger_info *info = power_supply_get_drvdata(psy);
    u32 cur = 0, health, enabled = 0;
    int ret = 0;

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

	if (!upm6920_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}
    mutex_lock(&info->lock);

    switch (psp) {
    case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == CM_POWER_PATH_ENABLE_CMD ||
		    val->intval == CM_POWER_PATH_DISABLE_CMD) {
			val->intval = upm6920_charger_get_power_path_status(info);
			break;
		}

        val->intval = upm6920_charger_get_status(info);
        pr_err("%s:line val->intval =%d \n", __func__, val->intval);
        break;

    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        if (!info->charging) {
            val->intval = 0;
        } else {
            ret = upm6920_charger_get_current(info, &cur);
            if (ret)
                goto out;

            val->intval = cur;
        }
        break;

    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        if (!info->charging) {
            val->intval = 0;
        } else {
            ret = upm6920_charger_get_limit_current(info, &cur);
            if (ret)
                goto out;

            val->intval = cur;
        }
        break;

    case POWER_SUPPLY_PROP_HEALTH:
        if (info->charging) {
            val->intval = 0;
        } else {
            ret = upm6920_charger_get_health(info, &health);
            if (ret)
                goto out;

            val->intval = health;
        }
        break;

        case POWER_SUPPLY_PROP_CALIBRATE:
            if (info->role == UPM6920_ROLE_MASTER_DEFAULT) {
            ret = regmap_read(info->pmic, info->charger_pd, &enabled);
            if (ret) {
                dev_err(info->dev, "get upm6920 charge status failed\n");
                goto out;
            }
            val->intval = !(enabled & info->charger_pd_mask);
            } else if (info->role == UPM6920_ROLE_SLAVE) {
            enabled = gpiod_get_value_cansleep(info->gpiod);
            val->intval = !enabled;
            }

            break;
#ifndef OTG_USE_REGULATOR
	case POWER_SUPPLY_PROP_SCOPE:
		val->intval = upm6920_charger_vbus_is_enabled(info);
		break;
#endif
    default:
        ret = -EINVAL;
    }

out:
    mutex_unlock(&info->lock);
    return ret;
}

static int upm6920_charger_usb_set_property(struct power_supply *psy,
                enum power_supply_property psp,
                const union power_supply_propval *val)
{
    struct upm6920_charger_info *info = power_supply_get_drvdata(psy);
    int ret = 0;
    u32 input_vol;
    bool bat_present;

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    if (psp == POWER_SUPPLY_PROP_STATUS || psp == POWER_SUPPLY_PROP_CALIBRATE) {
        bat_present = upm6920_charger_is_bat_present(info);
        ret = upm6920_charger_get_charge_voltage(info, &input_vol);
        if (ret) {
            input_vol = 0;
            dev_err(info->dev, "failed to get charge voltage! ret = %d\n", ret);
        }
    }

	if (!upm6920_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}
    mutex_lock(&info->lock);

    switch (psp) {
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        if (info->is_wireless_charge) {
            cancel_delayed_work_sync(&info->cur_work);
            info->new_charge_limit_cur = val->intval;
            pm_wakeup_event(info->dev, UPM6920_WAKE_UP_MS);
            schedule_delayed_work(&info->cur_work,
						msecs_to_jiffies(UPM6920_CURRENT_WORK_MS * 2));
            break;
        }

        ret = upm6920_charger_set_current(info, val->intval);
        if (ret < 0)
            dev_err(info->dev, "set charge current failed\n");
        break;
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        if (info->is_wireless_charge) {
            cancel_delayed_work_sync(&info->cur_work);
            info->new_input_limit_cur = val->intval;
            pm_wakeup_event(info->dev, UPM6920_WAKE_UP_MS);
            schedule_delayed_work(&info->cur_work,
						msecs_to_jiffies(UPM6920_CURRENT_WORK_MS * 2));
            break;
        }

        ret = upm6920_charger_set_limit_current(info, val->intval, false);
        if (ret < 0)
            dev_err(info->dev, "set input current limit failed\n");
        break;
    case POWER_SUPPLY_PROP_STATUS:
        if (val->intval == CM_POWER_PATH_ENABLE_CMD) {
            upm6920_exit_hiz_mode(info);
            break;
        } else if (val->intval == CM_POWER_PATH_DISABLE_CMD) {
            upm6920_enter_hiz_mode(info);
            break;
        }

        ret = upm6920_charger_set_status(info, val->intval, input_vol, bat_present);
        if (ret < 0)
            dev_err(info->dev, "set charge status failed\n");
        break;

    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
        ret = upm6920_charger_set_termina_vol(info, val->intval / 1000);
        if (ret < 0)
            dev_err(info->dev, "failed to set terminate voltage\n");
        break;

    case POWER_SUPPLY_PROP_CALIBRATE:
        if (val->intval == true) {
            upm6920_check_wireless_charge(info, true);
            ret = upm6920_charger_start_charge(info);
            if (ret)
                dev_err(info->dev, "start charge failed\n");
        } else if (val->intval == false) {
            upm6920_check_wireless_charge(info, false);
            upm6920_charger_stop_charge(info, bat_present);
        }
        break;
    case POWER_SUPPLY_PROP_TYPE:
        if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP) {
            info->is_wireless_charge = true;
            ret = upm6920_charger_set_ovp(info, UPM6920_FCHG_OVP_6V);
        } else if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP) {
            info->is_wireless_charge = true;
            ret = upm6920_charger_set_ovp(info, UPM6920_FCHG_OVP_14V);
        } else {
            info->is_wireless_charge = false;
            ret = upm6920_charger_set_ovp(info, UPM6920_FCHG_OVP_6V);
        }
        if (ret)
            dev_err(info->dev, "failed to set fast charge ovp\n");

        break;
	case POWER_SUPPLY_PROP_PRESENT:
		info->is_charger_online = val->intval;
		if (val->intval == true) {
			info->last_wdt_time = ktime_to_ms(ktime_get());
			schedule_delayed_work(&info->wdt_work, 0);
		} else {
			info->actual_limit_cur = 0;
			cancel_delayed_work_sync(&info->wdt_work);
		}
		break;
#ifndef OTG_USE_REGULATOR
	case POWER_SUPPLY_PROP_SCOPE:
		dev_err(info->dev, "xzy:set otg %d\n",val->intval);
		if (val->intval == 1)
			upm6920_charger_enable_otg(info);
		else
			upm6920_charger_disable_otg(info);
		break;
#endif

    default:
        ret = -EINVAL;
    }
    upm6920_dump_register(info);
    mutex_unlock(&info->lock);
    return ret;
}

static int upm6920_charger_property_is_writeable(struct power_supply *psy,
                        enum power_supply_property psp)
{
    int ret;

    switch (psp) {
    case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
    case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        case POWER_SUPPLY_PROP_CALIBRATE:
    case POWER_SUPPLY_PROP_TYPE:
    case POWER_SUPPLY_PROP_STATUS:
    case POWER_SUPPLY_PROP_PRESENT:
        ret = 1;
        break;

    default:
        ret = 0;
    }

    return ret;
}

static enum power_supply_property upm6920_usb_props[] = {
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
    POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_CALIBRATE,
	POWER_SUPPLY_PROP_TYPE,
};

static const struct power_supply_desc upm6920_charger_desc = {
    .name            = "charger",
    //.type            = POWER_SUPPLY_TYPE_USB,
    .type            = POWER_SUPPLY_TYPE_UNKNOWN,
    .properties        = upm6920_usb_props,
    .num_properties        = ARRAY_SIZE(upm6920_usb_props),
    .get_property        = upm6920_charger_usb_get_property,
    .set_property        = upm6920_charger_usb_set_property,
    .property_is_writeable    = upm6920_charger_property_is_writeable,
    //.usb_types        = upm6920_charger_usb_types,
    //.num_usb_types        = ARRAY_SIZE(upm6920_charger_usb_types),
};

static const struct power_supply_desc upm6920_slave_charger_desc = {
    .name            = "upm6920_slave_charger",
    //.type            = POWER_SUPPLY_TYPE_USB,
    .type            = POWER_SUPPLY_TYPE_UNKNOWN,
    .properties        = upm6920_usb_props,
    .num_properties        = ARRAY_SIZE(upm6920_usb_props),
    .get_property        = upm6920_charger_usb_get_property,
    .set_property        = upm6920_charger_usb_set_property,
    .property_is_writeable    = upm6920_charger_property_is_writeable,
    //.usb_types        = upm6920_charger_usb_types,
    //.num_usb_types        = ARRAY_SIZE(upm6920_charger_usb_types),
};

static ssize_t upm6920_register_value_show(struct device *dev,
                    struct device_attribute *attr,
                    char *buf)
{
    struct upm6920_charger_sysfs *upm6920_sysfs =
        container_of(attr, struct upm6920_charger_sysfs,
                attr_upm6920_reg_val);
    struct  upm6920_charger_info *info =  upm6920_sysfs->info;
    u8 val;
    int ret;

    if (!info)
        return snprintf(buf, PAGE_SIZE, "%s  upm6920_sysfs->info is null\n", __func__);

    ret = upm6920_read(info, reg_tab[info->reg_id].addr, &val);
    if (ret) {
        dev_err(info->dev, "fail to get  UPM6920_REG_0x%.2x value, ret = %d\n",
            reg_tab[info->reg_id].addr, ret);
        return snprintf(buf, PAGE_SIZE, "fail to get  UPM6920_REG_0x%.2x value\n",
                reg_tab[info->reg_id].addr);
    }

    return snprintf(buf, PAGE_SIZE, "UPM6920_REG_0x%.2x = 0x%.2x\n",
            reg_tab[info->reg_id].addr, val);
}

static ssize_t upm6920_register_value_store(struct device *dev,
                        struct device_attribute *attr,
                        const char *buf, size_t count)
{
    struct upm6920_charger_sysfs *upm6920_sysfs =
        container_of(attr, struct upm6920_charger_sysfs,
                attr_upm6920_reg_val);
    struct upm6920_charger_info *info = upm6920_sysfs->info;
    u8 val;
    int ret;

    if (!info) {
        dev_err(dev, "%s upm6920_sysfs->info is null\n", __func__);
        return count;
    }

    ret =  kstrtou8(buf, 16, &val);
    if (ret) {
        dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
        return count;
    }

    ret = upm6920_write(info, reg_tab[info->reg_id].addr, val);
    if (ret) {
        dev_err(info->dev, "fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
                val, reg_tab[info->reg_id].addr, ret);
        return count;
    }

    dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n", val, reg_tab[info->reg_id].addr);
    return count;
}

static ssize_t upm6920_register_id_store(struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    struct upm6920_charger_sysfs *upm6920_sysfs =
        container_of(attr, struct upm6920_charger_sysfs,
                attr_upm6920_sel_reg_id);
    struct upm6920_charger_info *info = upm6920_sysfs->info;
    int ret, id;

    if (!info) {
        dev_err(dev, "%s upm6920_sysfs->info is null\n", __func__);
        return count;
    }

    ret =  kstrtoint(buf, 10, &id);
    if (ret) {
        dev_err(info->dev, "%s store register id fail\n", upm6920_sysfs->name);
        return count;
    }

    if (id < 0 || id >= UPM6920_REG_NUM) {
        dev_err(info->dev, "%s store register id fail, id = %d is out of range\n",
            upm6920_sysfs->name, id);
        return count;
    }

    info->reg_id = id;

    dev_info(info->dev, "%s store register id = %d success\n", upm6920_sysfs->name, id);
    return count;
}

static ssize_t upm6920_register_id_show(struct device *dev,
                    struct device_attribute *attr,
                    char *buf)
{
    struct upm6920_charger_sysfs *upm6920_sysfs =
        container_of(attr, struct upm6920_charger_sysfs,
                attr_upm6920_sel_reg_id);
    struct upm6920_charger_info *info = upm6920_sysfs->info;

    if (!info)
        return snprintf(buf, PAGE_SIZE, "%s upm6920_sysfs->info is null\n", __func__);

    return snprintf(buf, PAGE_SIZE, "Curent register id = %d\n", info->reg_id);
}

static ssize_t upm6920_register_batfet_store(struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    struct upm6920_charger_sysfs *upm6920_sysfs =
    container_of(attr, struct upm6920_charger_sysfs, attr_upm6920_batfet_val);
    struct upm6920_charger_info *info = upm6920_sysfs->info;
    int ret;
    bool batfet;

    if (!info) {
        dev_err(dev, "%s upm6920_sysfs->info is null\n", __func__);
        return count;
    }

    ret =  kstrtobool(buf, &batfet);
    if (ret) {
        dev_err(info->dev, "batfet fail\n");
        return count;
    }

    if (batfet) {
        ret = upm6920_update_bits(info, UPM6920_REG_9, REG09_BATFET_DIS_MASK, 
                REG09_BATFET_DISABLE << REG09_BATFET_DIS_SHIFT);
        if (ret)
            dev_err(info->dev, "enter batfet mode failed\n");
    } else {
        ret = upm6920_update_bits(info, UPM6920_REG_9, REG09_BATFET_DIS_MASK, 
                REG09_BATFET_ENABLE << REG09_BATFET_DIS_SHIFT);
        if (ret)
            dev_err(info->dev, "exit batfet mode failed\n");
    }
    return count;
}

static ssize_t upm6920_register_batfet_show(struct device *dev,
                    struct device_attribute *attr,
                    char *buf)
{    u8 batfet , value;
    int ret;
    struct upm6920_charger_sysfs *upm6920_sysfs =
        container_of(attr, struct upm6920_charger_sysfs,
                attr_upm6920_batfet_val);
    struct upm6920_charger_info *info = upm6920_sysfs->info;

    if (!info)
        return snprintf(buf, PAGE_SIZE, "%s upm6920_sysfs->info is null\n", __func__);
    ret = upm6920_read(info, UPM6920_REG_9, &batfet);
    value = (batfet & REG09_BATFET_DIS_MASK) >> REG09_BATFET_DIS_SHIFT;
    return sprintf(buf, "%d\n", value);
}

static ssize_t upm6920_register_hizi_store(struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    struct upm6920_charger_sysfs *upm6920_sysfs =
        container_of(attr, struct upm6920_charger_sysfs,
                attr_upm6920_hizi_val);
    struct upm6920_charger_info *info = upm6920_sysfs->info;
    int ret;
    bool batfet;

    if (!info) {
        dev_err(dev, "%s upm6920_sysfs->info is null\n", __func__);
        return count;
    }

    ret =  kstrtobool(buf, &batfet);
    if (ret) {
        dev_err(info->dev, "hizi_store fail\n");
        return count;
    }

    if (batfet) {
        ret = upm6920_enter_hiz_mode(info);
        if (ret)
            dev_err(info->dev, "enter HIZ mode failed\n");
    } else {
        ret = upm6920_exit_hiz_mode(info);
        if (ret)
            dev_err(info->dev, "exit HIZ mode failed\n");
    }
    return count;
}

static ssize_t upm6920_register_hizi_show(struct device *dev,
                    struct device_attribute *attr,
                    char *buf)
{    u8 batfet , value;
    int ret;
    struct upm6920_charger_sysfs *upm6920_sysfs =
    container_of(attr, struct upm6920_charger_sysfs, attr_upm6920_hizi_val);
    struct upm6920_charger_info *info = upm6920_sysfs->info;

    if (!info)
        return snprintf(buf, PAGE_SIZE, "%s upm6920_sysfs->info is null\n", __func__);
    ret = upm6920_read(info, UPM6920_REG_0, &batfet);
    value = (batfet & REG00_EN_HIZ_MASK) >> REG00_EN_HIZ_SHIFT;
    return sprintf(buf, "%d\n", value);
}
static ssize_t upm6920_register_table_show(struct device *dev,
                    struct device_attribute *attr,
                    char *buf)
{
    struct upm6920_charger_sysfs *upm6920_sysfs =
    container_of(attr, struct upm6920_charger_sysfs, attr_upm6920_lookup_reg);
    struct upm6920_charger_info *info = upm6920_sysfs->info;
    int i, len, idx = 0;
    char reg_tab_buf[2048];

    if (!info)
        return snprintf(buf, PAGE_SIZE, "%s upm6920_sysfs->info is null\n", __func__);

    memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
    len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
            "Format: [id] [addr] [desc]\n");
    idx += len;

    for (i = 0; i < UPM6920_REG_NUM; i++) {
        len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
                "[%d] [REG_0x%.2x] [%s]; \n",
                reg_tab[i].id, reg_tab[i].addr, reg_tab[i].name);
        idx += len;
    }

    return snprintf(buf, PAGE_SIZE, "%s\n", reg_tab_buf);
}

static ssize_t upm6920_dump_register_show(struct device *dev,
                    struct device_attribute *attr,
                    char *buf)
{
    struct upm6920_charger_sysfs *upm6920_sysfs =
        container_of(attr, struct upm6920_charger_sysfs,
                attr_upm6920_dump_reg);
    struct upm6920_charger_info *info = upm6920_sysfs->info;

    if (!info)
        return snprintf(buf, PAGE_SIZE, "%s upm6920_sysfs->info is null\n", __func__);

    upm6920_dump_register(info);

    return snprintf(buf, PAGE_SIZE, "%s\n", upm6920_sysfs->name);
}

static int upm6920_register_sysfs(struct upm6920_charger_info *info)
{
    struct upm6920_charger_sysfs *upm6920_sysfs;
    int ret;

    upm6920_sysfs = devm_kzalloc(info->dev, sizeof(*upm6920_sysfs), GFP_KERNEL);
    if (!upm6920_sysfs)
        return -ENOMEM;

    info->sysfs = upm6920_sysfs;
    upm6920_sysfs->name = "upm6920_sysfs";
    upm6920_sysfs->info = info;
    upm6920_sysfs->attrs[0] = &upm6920_sysfs->attr_upm6920_dump_reg.attr;
    upm6920_sysfs->attrs[1] = &upm6920_sysfs->attr_upm6920_lookup_reg.attr;
    upm6920_sysfs->attrs[2] = &upm6920_sysfs->attr_upm6920_sel_reg_id.attr;
    upm6920_sysfs->attrs[3] = &upm6920_sysfs->attr_upm6920_reg_val.attr;
    upm6920_sysfs->attrs[4] = &upm6920_sysfs->attr_upm6920_batfet_val.attr;
    upm6920_sysfs->attrs[5] = &upm6920_sysfs->attr_upm6920_hizi_val.attr;
    upm6920_sysfs->attrs[6] = NULL;
    upm6920_sysfs->attr_g.name = "debug";
    upm6920_sysfs->attr_g.attrs = upm6920_sysfs->attrs;

    sysfs_attr_init(&upm6920_sysfs->attr_upm6920_dump_reg.attr);
    upm6920_sysfs->attr_upm6920_dump_reg.attr.name = "upm6920_dump_reg";
    upm6920_sysfs->attr_upm6920_dump_reg.attr.mode = 0444;
    upm6920_sysfs->attr_upm6920_dump_reg.show = upm6920_dump_register_show;

    sysfs_attr_init(&upm6920_sysfs->attr_upm6920_lookup_reg.attr);
    upm6920_sysfs->attr_upm6920_lookup_reg.attr.name = "upm6920_lookup_reg";
    upm6920_sysfs->attr_upm6920_lookup_reg.attr.mode = 0444;
    upm6920_sysfs->attr_upm6920_lookup_reg.show = upm6920_register_table_show;

    sysfs_attr_init(&upm6920_sysfs->attr_upm6920_sel_reg_id.attr);
    upm6920_sysfs->attr_upm6920_sel_reg_id.attr.name = "upm6920_sel_reg_id";
    upm6920_sysfs->attr_upm6920_sel_reg_id.attr.mode = 0644;
    upm6920_sysfs->attr_upm6920_sel_reg_id.show = upm6920_register_id_show;
    upm6920_sysfs->attr_upm6920_sel_reg_id.store = upm6920_register_id_store;

    sysfs_attr_init(&upm6920_sysfs->attr_upm6920_reg_val.attr);
    upm6920_sysfs->attr_upm6920_reg_val.attr.name = "upm6920_reg_val";
    upm6920_sysfs->attr_upm6920_reg_val.attr.mode = 0644;
    upm6920_sysfs->attr_upm6920_reg_val.show = upm6920_register_value_show;
    upm6920_sysfs->attr_upm6920_reg_val.store = upm6920_register_value_store;

    sysfs_attr_init(&upm6920_sysfs->attr_upm6920_batfet_val.attr);
    upm6920_sysfs->attr_upm6920_batfet_val.attr.name = "charger_batfet_val";
    upm6920_sysfs->attr_upm6920_batfet_val.attr.mode = 0644;
    upm6920_sysfs->attr_upm6920_batfet_val.show = upm6920_register_batfet_show;
    upm6920_sysfs->attr_upm6920_batfet_val.store = upm6920_register_batfet_store;

    sysfs_attr_init(&upm6920_sysfs->attr_upm6920_batfet_val.attr);
    upm6920_sysfs->attr_upm6920_hizi_val.attr.name = "charger_hizi_val";
    upm6920_sysfs->attr_upm6920_hizi_val.attr.mode = 0644;
    upm6920_sysfs->attr_upm6920_hizi_val.show = upm6920_register_hizi_show;
    upm6920_sysfs->attr_upm6920_hizi_val.store = upm6920_register_hizi_store;

    ret = sysfs_create_group(&info->psy_usb->dev.kobj, &upm6920_sysfs->attr_g);
    if (ret < 0)
        dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

    return ret;
}

static void upm6920_charger_feed_watchdog_work(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct upm6920_charger_info *info = container_of(dwork,
                            struct upm6920_charger_info,
                            wdt_work);
    int ret;

    ret = upm6920_update_bits(info, UPM6920_REG_3,
                REG03_WD_RST_MASK, REG03_WD_RST_MASK);
	if (ret)
		schedule_delayed_work(&info->wdt_work, HZ * 1);
	else
		schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#if IS_ENABLED(CONFIG_REGULATOR)
static bool upm6920_charger_check_otg_valid(struct upm6920_charger_info *info)
{
    int ret;
    u8 value = 0;
    bool status = false;

    ret = upm6920_read(info, UPM6920_REG_3, &value);
    if (ret) {
        dev_err(info->dev, "get upm6920 charger otg valid status failed\n");
        return status;
    }

    if (value & REG03_OTG_MASK)
        status = true;
    else
        dev_err(info->dev, "otg is not valid, REG_1 = 0x%x\n", value);

    return status;
}

static bool upm6920_charger_check_otg_fault(struct upm6920_charger_info *info)
{
    int ret;
    u8 value = 0;
    bool status = true;

    ret = upm6920_read(info, UPM6920_REG_C, &value);
    if (ret) {
        dev_err(info->dev, "get upm6920 charger otg fault status failed\n");
        return status;
    }

    if (!(value & REG0C_OTG_FAULT))
        status = false;
    else
        dev_err(info->dev, "boost fault occurs, REG_9 = 0x%x\n", value);

    return status;
}

static void upm6920_charger_otg_work(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct upm6920_charger_info *info = container_of(dwork,
            struct upm6920_charger_info, otg_work);
    bool otg_valid = upm6920_charger_check_otg_valid(info);
    bool otg_fault;
    int ret, retry = 0;

    if (otg_valid)
        goto out;

    do {
        otg_fault = upm6920_charger_check_otg_fault(info);
        if (!otg_fault) {
            ret = upm6920_charger_set_otg_en(info, true);
            if (ret)
                dev_err(info->dev, "restart upm6920 charger otg failed\n");

        }

        otg_valid = upm6920_charger_check_otg_valid(info);
    } while (!otg_valid && retry++ < UPM6920_OTG_RETRY_TIMES);

    if (retry >= UPM6920_OTG_RETRY_TIMES) {
        dev_err(info->dev, "Restart OTG failed\n");
        return;
    }

out:
	dev_dbg(info->dev, "%s:line%d:schedule_work\n", __func__, __LINE__);
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

#ifdef OTG_USE_REGULATOR
static int upm6920_charger_enable_otg(struct regulator_dev *dev)
{
    struct upm6920_charger_info *info = rdev_get_drvdata(dev);
    int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->shutdown_flag)
		return ret;

	upm6920_charger_dump_stack();

	if (!upm6920_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}
    /*
    * Disable charger detection function in case
    * affecting the OTG timing sequence.
    */
    if (!info->use_typec_extcon) {
        ret = regmap_update_bits(info->pmic, info->charger_detect,
                    BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
        if (ret) {
            dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
            return ret;
        }
    }


    ret = upm6920_charger_set_otg_en(info, true);
    if (ret) {
        dev_err(info->dev, "enable upm6920 otg failed\n");
        regmap_update_bits(info->pmic, info->charger_detect,
                BIT_DP_DM_BC_ENB, 0);
        return ret;
    }

    info->otg_enable = true;
	info->last_wdt_time = ktime_to_ms(ktime_get());
    schedule_delayed_work(&info->wdt_work,
                msecs_to_jiffies(UPM6920_FEED_WATCHDOG_VALID_MS));
    schedule_delayed_work(&info->otg_work,
                msecs_to_jiffies(UPM6920_OTG_VALID_MS));

	dev_info(info->dev, "%s:line%d:enable_otg\n", __func__, __LINE__);

    return ret;
}

static int upm6920_charger_disable_otg(struct regulator_dev *dev)
{
    struct upm6920_charger_info *info = rdev_get_drvdata(dev);
    int ret = 0;

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

	upm6920_charger_dump_stack();

	if (!upm6920_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}
    info->otg_enable = false;
    cancel_delayed_work_sync(&info->wdt_work);
    cancel_delayed_work_sync(&info->otg_work);
    ret = upm6920_charger_set_otg_en(info, false);
    if (ret) {
        dev_err(info->dev, "disable upm6920 otg failed\n");
        return ret;
    }

    if (!info->use_typec_extcon) {
        ret = regmap_update_bits(info->pmic, info->charger_detect, BIT_DP_DM_BC_ENB, 0);
        if (ret)
            dev_err(info->dev, "enable BC1.2 failed\n");
    }
	dev_info(info->dev, "%s:line%d:disable_otg\n", __func__, __LINE__);

	return ret;
}

static int upm6920_charger_vbus_is_enabled(struct regulator_dev *dev)
{
    struct upm6920_charger_info *info = rdev_get_drvdata(dev);
    int ret;
    u8 val;

    dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    ret = upm6920_read(info, UPM6920_REG_3, &val);
    if (ret) {
        dev_err(info->dev, "failed to get upm6920 otg status\n");
        return ret;
    }

    val &= REG03_OTG_MASK;
    dev_info(info->dev, "%s:line%d val = %d\n", __func__, __LINE__, val);
    
    return val;
}

static const struct regulator_ops upm6920_charger_vbus_ops = {
    .enable = upm6920_charger_enable_otg,
    .disable = upm6920_charger_disable_otg,
    .is_enabled = upm6920_charger_vbus_is_enabled,
};

static const struct regulator_desc upm6920_charger_vbus_desc = {
    .name = "otg-vbus",
    .of_match = "otg-vbus",
    //.name = "upm6920_otg_vbus",
    //.of_match = "upm6920_otg_vbus",
    .type = REGULATOR_VOLTAGE,
    .owner = THIS_MODULE,
    .ops = &upm6920_charger_vbus_ops,
    .fixed_uV = 5000000,
    .n_voltages = 1,
};

static int upm6920_charger_register_vbus_regulator(struct upm6920_charger_info *info)
{
    struct regulator_config cfg = { };
    struct regulator_dev *reg;
    int ret = 0;

    cfg.dev = info->dev;
    cfg.driver_data = info;
    reg = devm_regulator_register(info->dev,
                    &upm6920_charger_vbus_desc, &cfg);
    if (IS_ERR(reg)) {
        ret = PTR_ERR(reg);
        dev_err(info->dev, "Can't register regulator:%d\n", ret);
    }

    return ret;
}
#else
static int upm6920_charger_enable_otg(struct upm6920_charger_info *info)
{
    int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->shutdown_flag)
		return ret;

	upm6920_charger_dump_stack();

	if (!upm6920_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}
    /*
    * Disable charger detection function in case
    * affecting the OTG timing sequence.
    */
    if (!info->use_typec_extcon) {
        ret = regmap_update_bits(info->pmic, info->charger_detect,
                    BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
        if (ret) {
            dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
            return ret;
        }
    }

    ret = upm6920_charger_set_otg_en(info, true);
    if (ret) {
        dev_err(info->dev, "enable upm6920 otg failed\n");
        regmap_update_bits(info->pmic, info->charger_detect,
                BIT_DP_DM_BC_ENB, 0);
        return ret;
    }

    info->otg_enable = true;
	info->last_wdt_time = ktime_to_ms(ktime_get());
    schedule_delayed_work(&info->wdt_work,
                msecs_to_jiffies(UPM6920_FEED_WATCHDOG_VALID_MS));
    schedule_delayed_work(&info->otg_work,
                msecs_to_jiffies(UPM6920_OTG_VALID_MS));

	dev_info(info->dev, "%s:line%d:enable_otg\n", __func__, __LINE__);

    return ret;
}

static int upm6920_charger_disable_otg(struct upm6920_charger_info *info)
{
    int ret = 0;

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

	upm6920_charger_dump_stack();

    if (!upm6920_probe_is_ready(info)) {
        dev_err(info->dev, "%s wait probe timeout\n", __func__);
        return -EINVAL;
    }
    info->otg_enable = false;
    cancel_delayed_work_sync(&info->wdt_work);
    cancel_delayed_work_sync(&info->otg_work);
    ret = upm6920_charger_set_otg_en(info, false);
    if (ret) {
        dev_err(info->dev, "disable upm6920 otg failed\n");
        return ret;
    }

    if (!info->use_typec_extcon) {
        ret = regmap_update_bits(info->pmic, info->charger_detect, BIT_DP_DM_BC_ENB, 0);
        if (ret)
            dev_err(info->dev, "enable BC1.2 failed\n");
    }
	dev_info(info->dev, "%s:line%d:disable_otg\n", __func__, __LINE__);

	return ret;
}

static int upm6920_charger_vbus_is_enabled(struct upm6920_charger_info *info)
{
    int ret;
    u8 val;

    dev_info(info->dev, "%s:line%d enter\n", __func__, __LINE__);
    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    ret = upm6920_read(info, UPM6920_REG_3, &val);
    if (ret) {
        dev_err(info->dev, "failed to get upm6920 otg status\n");
        return ret;
    }

    val &= REG03_OTG_MASK;
    dev_info(info->dev, "%s:line%d val = %d\n", __func__, __LINE__, val);

    return val;
}
static int
upm6920_charger_register_vbus_regulator(struct upm6920_charger_info *info)
{
	return 0;
}
#endif

#else
static int
upm6920_charger_register_vbus_regulator(struct upm6920_charger_info *info)
{
    return 0;
}
#endif

static int upm6920_charger_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
    struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
    struct device *dev = &client->dev;
    struct power_supply_config charger_cfg = { };
    struct upm6920_charger_info *info;
    struct device_node *regmap_np;
    struct platform_device *regmap_pdev;
    int ret;
	bool bat_present;

    if (!adapter) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    if (!dev) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
        dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
        return -ENODEV;
    }

    pr_info("%s (%s): initializing...\n", __func__, UPM6920_DRV_VERSION);

    info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
    if (!info)
        return -ENOMEM;

    info->client = client;
    info->dev = dev;
	info->client->addr = 0x6a;

    ret = upm6920_charger_get_vendor_id_part_value(info);
    if (ret) {
        dev_err(dev, "failed to get vendor id, part value\n");
        return ret;
    }

    i2c_set_clientdata(client, info);
    power_path_control(info);

    ret = upm6920_charger_is_fgu_present(info);
    if (ret) {
        dev_err(dev, "sc27xx_fgu not ready.\n");
        return -EPROBE_DEFER;
    }

    info->use_typec_extcon = device_property_read_bool(dev, "use-typec-extcon");
	info->disable_wdg = device_property_read_bool(dev, "disable-otg-wdg-in-sleep");

    ret = device_property_read_bool(dev, "role-slave");
    if (ret)
        info->role = UPM6920_ROLE_SLAVE;
    else
        info->role = UPM6920_ROLE_MASTER_DEFAULT;

    if (info->role == UPM6920_ROLE_SLAVE) {
        info->gpiod = devm_gpiod_get(dev, "enable", GPIOD_OUT_HIGH);
        if (IS_ERR(info->gpiod)) {
            dev_err(dev, "failed to get enable gpio\n");
            return PTR_ERR(info->gpiod);
        }
    }

    regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
    if (!regmap_np)
        regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");

    if (regmap_np) {
        if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
            info->charger_pd_mask = UPM6920_DISABLE_PIN_MASK_2721;
        else
            info->charger_pd_mask = UPM6920_DISABLE_PIN_MASK;
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

	bat_present = upm6920_charger_is_bat_present(info);

	mutex_init(&info->lock);
	mutex_init(&info->input_limit_cur_lock);
	init_completion(&info->probe_init);

    charger_cfg.drv_data = info;
    charger_cfg.of_node = dev->of_node;
    if (info->role == UPM6920_ROLE_MASTER_DEFAULT) {
        info->psy_usb = devm_power_supply_register(dev,
                            &upm6920_charger_desc,
                            &charger_cfg);
    } else if (info->role == UPM6920_ROLE_SLAVE) {
        info->psy_usb = devm_power_supply_register(dev,
                            &upm6920_slave_charger_desc,
                            &charger_cfg);
    }

    if (IS_ERR(info->psy_usb)) {
        dev_err(dev, "failed to register power supply\n");
        ret = PTR_ERR(info->psy_usb);
        goto err_regmap_exit;
    }

    ret = upm6920_charger_hw_init(info);
    if (ret) {
        dev_err(dev, "failed to upm6920_charger_hw_init\n");
        goto err_psy_usb;
    }

    upm6920_charger_stop_charge(info, bat_present);

    device_init_wakeup(info->dev, true);

    alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);
    INIT_DELAYED_WORK(&info->otg_work, upm6920_charger_otg_work);
    INIT_DELAYED_WORK(&info->wdt_work, upm6920_charger_feed_watchdog_work);

    /*
    * only master to support otg
    */
    if (info->role == UPM6920_ROLE_MASTER_DEFAULT) {
        ret = upm6920_charger_register_vbus_regulator(info);
        if (ret) {
            dev_err(dev, "failed to register vbus regulator.\n");
            goto err_psy_usb;
        }
    }

    INIT_DELAYED_WORK(&info->cur_work, upm6920_current_work);

    ret = upm6920_register_sysfs(info);
    if (ret) {
        dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
        goto error_sysfs;
    }

    info->irq_gpio = of_get_named_gpio(info->dev->of_node, "irq-gpio", 0);
    if (gpio_is_valid(info->irq_gpio)) {
        ret = devm_gpio_request_one(info->dev, info->irq_gpio,
                        GPIOF_DIR_IN, "upm6920_int");
        if (!ret)
            info->client->irq = gpio_to_irq(info->irq_gpio);
        else
            dev_err(dev, "int request failed, ret = %d\n", ret);

        if (info->client->irq < 0) {
            dev_err(dev, "failed to get irq no\n");
            gpio_free(info->irq_gpio);
        } else {
            ret = devm_request_threaded_irq(&info->client->dev, info->client->irq,
                            NULL, upm6920_int_handler,
                            IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                            "upm6920 interrupt", info);
            if (ret)
                dev_err(info->dev, "Failed irq = %d ret = %d\n",
                    info->client->irq, ret);
            else
                enable_irq_wake(client->irq);
        }
    } else {
        dev_err(dev, "failed to get irq gpio\n");
    }

	info->probe_initialized = true;
	complete_all(&info->probe_init);

	upm6920_dump_register(info);
	dev_info(dev, "use_typec_extcon = %d\n", info->use_typec_extcon);

	return 0;

error_sysfs:
	sysfs_remove_group(&info->psy_usb->dev.kobj, &info->sysfs->attr_g);
err_psy_usb:
	if (info->irq_gpio)
		gpio_free(info->irq_gpio);
err_regmap_exit:
	mutex_destroy(&info->lock);
	return ret;
}

static void upm6920_charger_shutdown(struct i2c_client *client)
{
    struct upm6920_charger_info *info = i2c_get_clientdata(client);
    int ret = 0;

    cancel_delayed_work_sync(&info->wdt_work);
    if (info->otg_enable) {
        info->otg_enable = false;
        cancel_delayed_work_sync(&info->otg_work);
        ret = upm6920_update_bits(info, UPM6920_REG_3,
                    REG03_OTG_MASK,0);
        if (ret)
            dev_err(info->dev, "disable upm6920 otg failed ret = %d\n", ret);

        /* Enable charger detection function to identify the charger type */
        ret = regmap_update_bits(info->pmic, info->charger_detect,
                    BIT_DP_DM_BC_ENB, 0);
        if (ret)
            dev_err(info->dev,
                "enable charger detection function failed ret = %d\n", ret);
    }
	info->shutdown_flag = true;
}

static int upm6920_charger_remove(struct i2c_client *client)
{
    struct upm6920_charger_info *info = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);

    return 0;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int upm6920_charger_suspend(struct device *dev)
{
    struct upm6920_charger_info *info = dev_get_drvdata(dev);
    ktime_t now, add;

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    if (!info->otg_enable)
        return 0;

    cancel_delayed_work_sync(&info->wdt_work);
    cancel_delayed_work_sync(&info->cur_work);

    now = ktime_get_boottime();
    add = ktime_set(UPM6920_OTG_ALARM_TIMER_S, 0);
    alarm_start(&info->otg_timer, ktime_add(now, add));

    return 0;
}

static int upm6920_charger_resume(struct device *dev)
{
    struct upm6920_charger_info *info = dev_get_drvdata(dev);

    if (!info) {
        pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
        return -EINVAL;
    }

    if (!info->otg_enable)
        return 0;

    alarm_cancel(&info->otg_timer);

    schedule_delayed_work(&info->wdt_work, HZ * 15);
    schedule_delayed_work(&info->cur_work, 0);

    return 0;
}
#endif

static const struct dev_pm_ops upm6920_charger_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(upm6920_charger_suspend,
                upm6920_charger_resume)
};

static const struct i2c_device_id upm6920_i2c_id[] = {
    {"upm6920_chg", 0},
    {}
};

static const struct of_device_id upm6920_charger_of_match[] = {
    { .compatible = "up,upm6920_chg", },
    { }
};

MODULE_DEVICE_TABLE(of, upm6920_charger_of_match);

static struct i2c_driver upm6920_master_charger_driver = {
    .driver = {
        .name = "upm6920_chg",
        .of_match_table = upm6920_charger_of_match,
        .pm = &upm6920_charger_pm_ops,
    },
    .probe = upm6920_charger_probe,
    .shutdown = upm6920_charger_shutdown,
    .remove = upm6920_charger_remove,
    .id_table = upm6920_i2c_id,
};

module_i2c_driver(upm6920_master_charger_driver);
MODULE_DESCRIPTION("UPM6920 Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(UPM6920_DRV_VERSION);
MODULE_AUTHOR("Unisemipower <lai.du@unisemipower.com>");
