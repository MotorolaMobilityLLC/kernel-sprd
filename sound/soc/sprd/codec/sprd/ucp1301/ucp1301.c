/*
 * SPDX-License-Identifier: GPL-2.0
 * Core MFD(Charger, ADC, Flash and GPIO) driver for UCP1301
 *
 * Copyright (c) 2019 Dialog Semiconductor.
 */
#include "sprd-asoc-debug.h"
#define pr_fmt(fmt) pr_sprd_fmt("ucp1301")""fmt

#include <linux/i2c.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include "sprd-asoc-common.h"
#include "ucp1301.h"

#define UCP1301_I2C_NAME    "ucp1301"
#define UCP1301_DRIVER_VERSION  "v1.0.0"
#define UCP1301_CHIP_ID 0x1301a000

#define UCP_I2C_RETRIES 5
#define UCP_I2C_RETRY_DELAY 2
#define UCP_READ_CHIPID_RETRIES 5
#define UCP_READ_CHIPID_RETRY_DELAY 2
#define UCP_READ_EFS_STATUS_RETRIES 3
#define UCP_EFS_DATA_REG_COUNTS 4
/* Timeout (us) of polling the status */
#define UCP_READ_EFS_POLL_TIMEOUT	800
#define UCP_READ_EFS_POLL_DELAY_US	200

enum ucp1301_audio_mode {
	HW_OFF,
	SPK_AB,
	RCV_AB,
	SPK_D,
	RCV_D,
	MODE_MAX
};

enum ucp1301_ivsense_mode {
	IVS_UCP1301,
	IVS_UCP1300A,
	IVS_UCP1300B,
	IVS_MODE_MAX
};

struct ucp1301_t {
	struct i2c_client *i2c_client;
	struct regmap *regmap;
	struct device *dev;
	struct gpio_desc *reset_gpio;
	bool init_flag;
	bool hw_enabled;
	bool class_ab;/* true class AB, false class D */
	bool bypass;/* true bypass, false boost */
	/*
	 * enabled in class D mode only, true: remote mode, false local mode,
	 * 1301 use remote mode, 1300A/B use local mode
	 */
	bool ivsense_mode_curt;
	bool ivsense_mode_to_set;
	enum ucp1301_ivsense_mode ivsense_mode;
	u32 vosel;/* value of RG_BST_VOSEL */
	u32 efs_data[4];/* 0 L, 1 M, 2 H, 3 T */
	u32 calib_code;
	enum ucp1301_audio_mode mode_curt;/* current mode */
	enum ucp1301_audio_mode mode_to_set;
	enum ucp1301_audio_mode class_mode;
	u32 clsd_trim;
};

struct ucp1301_t *ucp1301_g;

static u32 efs_data_reg[4] = {
	REG_EFS_RD_DATA_L,/* low */
	REG_EFS_RD_DATA_M,
	REG_EFS_RD_DATA_H,
	REG_EFS_RD_DATA_T/* high */
};

static const struct regmap_config ucp1301_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
};

static int ucp1301_write_clsd_trim(struct ucp1301_t *ucp1301, u32 clsd_trim)
{
	u32 val;
	int new_trim, ret;

	/* check current state */
	ret = regmap_read(ucp1301->regmap, REG_BST_REG0, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "set clsd trim, read reg 0x%x fail, %d\n",
			REG_BST_REG0, ret);
		return ret;
	}
	ucp1301->bypass = (val & BIT_RG_BST_BYPASS) > 0 ? true : false;
	dev_dbg(ucp1301->dev, "set clsd trim, databuf %d, bypass %d, calib_code %d(0x%x)\n",
		clsd_trim, ucp1301->bypass, ucp1301->calib_code,
		ucp1301->calib_code);

	if (ucp1301->bypass) {
		dev_err(ucp1301->dev, "you can't set clsd_trim when bypass mode is on\n");
		return -EINVAL;
	}

	new_trim = ucp1301->calib_code + clsd_trim / 100 - 16;
	if (new_trim < 0)
		new_trim = 0;
	else
		new_trim = new_trim & BIT_RG_PMU_OSC1P6M_CLSD_TRIM(0x1f);

	if (new_trim != ucp1301->calib_code) {
		regmap_update_bits(ucp1301->regmap, REG_PMU_REG1,
				   BIT_PMU_OSC1P6M_CLSD_SW_SEL,
				   BIT_PMU_OSC1P6M_CLSD_SW_SEL);
		regmap_update_bits(ucp1301->regmap, REG_PMU_REG1,
				   BIT_RG_PMU_OSC1P6M_CLSD_TRIM(0x1f),
				   new_trim);
		ucp1301->calib_code = new_trim;
		pr_info("set clsd trim, calib_code %d(0x%x)\n",
			ucp1301->calib_code, ucp1301->calib_code);
	}

	return 0;
}

/* class AB depop boost on mode(spk + bst mode) */
static void ucp1301_depop_ab_boost_on(struct ucp1301_t *ucp1301, bool enable)
{
	if (enable) {
		regmap_update_bits(ucp1301->regmap, REG_CLSD_REG0,
				   BIT_RG_CLSD_MODE_SEL, BIT_RG_CLSD_MODE_SEL);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_BST_ACTIVE, BIT_BST_ACTIVE);
		regmap_update_bits(ucp1301->regmap, REG_BST_REG0,
				   BIT_RG_BST_BYPASS, 0);
		regmap_update_bits(ucp1301->regmap, REG_BST_REG1,
				   BIT_RG_BST_VOSEL(0xf), BIT_RG_BST_VOSEL(1));
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CHIP_EN, BIT_CHIP_EN);
		/* dc calibration time */
		sprd_msleep(40);
		regmap_update_bits(ucp1301->regmap, REG_RESERVED_REG1,
				   BIT_RG_RESERVED1(0x4),
				   BIT_RG_RESERVED1(0x4));
		sprd_msleep(5);
		regmap_update_bits(ucp1301->regmap, REG_RESERVED_REG1,
				   BIT_RG_RESERVED1(0x2),
				   BIT_RG_RESERVED1(0x2));

		sprd_msleep(5);
		regmap_update_bits(ucp1301->regmap, REG_CLSAB_REG0,
				   BIT_RG_CLSAB_MODE_EN, BIT_RG_CLSAB_MODE_EN);
		sprd_msleep(5);
		regmap_update_bits(ucp1301->regmap, REG_RESERVED_REG1,
				   BIT_RG_RESERVED1(0x2), 0);
		sprd_msleep(5);
		regmap_update_bits(ucp1301->regmap, REG_RESERVED_REG1,
				   BIT_RG_RESERVED1(0x4), 0);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, BIT_CLSD_ACTIVE);
		/* pcc time */
		sprd_msleep(30);
	} else {
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, 0);
		/* pcc time */
		sprd_msleep(30);
		regmap_update_bits(ucp1301->regmap, REG_CLSAB_REG0,
				   BIT_RG_CLSAB_MODE_EN, 0);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CHIP_EN, 0);
	}
}

/*
 * class AB depop boost on mode(rcv + bypass mode),
 * only BIT_RG_CLSD_MODE_SEL and BIT_RG_BST_BYPASS
 * bits is different, compare to boost_on mode
 */
static void ucp1301_depop_ab_boost_bypass(struct ucp1301_t *ucp1301,
					  bool enable)
{
	int ret;

	if (enable) {
		regmap_update_bits(ucp1301->regmap, REG_CLSD_REG0,
				   BIT_RG_CLSD_MODE_SEL, 0);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_BST_ACTIVE, BIT_BST_ACTIVE);
		regmap_update_bits(ucp1301->regmap, REG_BST_REG0,
				   BIT_RG_BST_BYPASS, BIT_RG_BST_BYPASS);
		regmap_update_bits(ucp1301->regmap, REG_BST_REG1,
				   BIT_RG_BST_VOSEL(0xf), BIT_RG_BST_VOSEL(1));
		ret = regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
					 BIT_CHIP_EN,
					 BIT_CHIP_EN);
		/* dc calibration time */
		sprd_msleep(40);
		regmap_update_bits(ucp1301->regmap, REG_RESERVED_REG1,
				   BIT_RG_RESERVED1(0x4),
				   BIT_RG_RESERVED1(0x4));
		sprd_msleep(5);
		regmap_update_bits(ucp1301->regmap, REG_RESERVED_REG1,
				   BIT_RG_RESERVED1(0x2),
				   BIT_RG_RESERVED1(0x2));

		sprd_msleep(5);
		regmap_update_bits(ucp1301->regmap, REG_CLSAB_REG0,
				   BIT_RG_CLSAB_MODE_EN, BIT_RG_CLSAB_MODE_EN);
		sprd_msleep(5);
		regmap_update_bits(ucp1301->regmap, REG_RESERVED_REG1,
				   BIT_RG_RESERVED1(0x2), 0);
		sprd_msleep(5);
		regmap_update_bits(ucp1301->regmap, REG_RESERVED_REG1,
				   BIT_RG_RESERVED1(0x4), 0);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, BIT_CLSD_ACTIVE);
	} else {
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, 0);
		regmap_update_bits(ucp1301->regmap, REG_CLSAB_REG0,
				   BIT_RG_CLSAB_MODE_EN, 0);
		ret = regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
					 BIT_CHIP_EN, 0);
	}
	if (ret)
		dev_err(ucp1301->dev, "boost_bypass, set BIT_CHIP_EN to %d fail, %d\n",
			enable, ret);
	/* pcc time */
	sprd_msleep(30);
}

/* spk + boost mode */
static void ucp1301_d_boost_on(struct ucp1301_t *ucp1301, bool on)
{
	int ret;
	u32 temp;

	if (on) {
		regmap_update_bits(ucp1301->regmap, REG_BST_REG0,
				   BIT_RG_BST_BYPASS, 0);
		regmap_update_bits(ucp1301->regmap, REG_CLSD_REG0,
				   BIT_RG_CLSD_MODE_SEL, BIT_RG_CLSD_MODE_SEL);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, BIT_CLSD_ACTIVE);
	} else {
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, 0);
	}

	temp = on ? BIT_CHIP_EN : 0;
	ret = regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				 BIT_CHIP_EN, temp);
	if (ret) {
		dev_err(ucp1301->dev, "boost_on, set BIT_CHIP_EN to %d fail, %d\n",
			on, ret);
		return;
	}

	/* wait circuit to response */
	sprd_msleep(30);
}

/* rcv + bypass mode */
static void ucp1301_d_bypass(struct ucp1301_t *ucp1301, bool on)
{
	int ret;
	u32 temp;

	if (on) {
		regmap_update_bits(ucp1301->regmap, REG_BST_REG0,
				   BIT_RG_BST_BYPASS, BIT_RG_BST_BYPASS);
		regmap_update_bits(ucp1301->regmap, REG_CLSD_REG0,
				   BIT_RG_CLSD_MODE_SEL, 0);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, BIT_CLSD_ACTIVE);
	} else {
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, 0);
	}

	temp = on ? BIT_CHIP_EN : 0;
	ret = regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				 BIT_CHIP_EN, temp);
	if (ret) {
		dev_err(ucp1301->dev, "d_bypass, set BIT_CHIP_EN to %d fail, %d\n",
			on, ret);
		return;
	}

	/* wait circuit to response */
	sprd_msleep(30);
}

static void ucp1301_d_ab_switch(struct ucp1301_t *ucp1301, bool on)
{
	if (on) {/* class D switch to class AB */
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, 0);
		regmap_update_bits(ucp1301->regmap, REG_CLSAB_REG0,
				   BIT_RG_CLSAB_MODE_EN, BIT_RG_CLSAB_MODE_EN);
	} else {/* class AB switch to class D */
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, 0);
		regmap_update_bits(ucp1301->regmap, REG_CLSAB_REG0,
				   BIT_RG_CLSAB_MODE_EN, 0);
	}
}

static void ucp1301_bst_bypass_switch(struct ucp1301_t *ucp1301, bool on)
{
	int ret;

	if (on) {/* boost switch to bypass */
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, 0);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CHIP_EN, 0);
		regmap_update_bits(ucp1301->regmap, REG_BST_REG0,
				   BIT_RG_BST_BYPASS, BIT_RG_BST_BYPASS);
	} else {/* bypass switch to bootst */
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CLSD_ACTIVE, 0);
		regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
				   BIT_CHIP_EN, 0);
		regmap_update_bits(ucp1301->regmap, REG_BST_REG0,
				   BIT_RG_BST_BYPASS, 0);
	}
	ret = regmap_update_bits(ucp1301->regmap, REG_MODULE_EN, BIT_CHIP_EN,
				 BIT_CHIP_EN);
	if (ret)
		dev_err(ucp1301->dev, "bypass_switch, set BIT_CHIP_EN to 1 fail, %d\n",
			ret);
}

static int ucp1301_read_efs_data(struct ucp1301_t *ucp1301)
{
	int ret, i = 0;

	do {
		ret = regmap_read(ucp1301->regmap, efs_data_reg[i],
				  &ucp1301->efs_data[i]);
		if (ret < 0) {
			dev_err(ucp1301->dev, "read efs data, read reg 0x%x fail, %d\n",
				efs_data_reg[i], ret);
			return ret;
		}
		dev_dbg(ucp1301->dev, "efs[%d] 0x%x\n",
			i, ucp1301->efs_data[i]);
	} while (++i < UCP_EFS_DATA_REG_COUNTS);

	return 0;
}

static int ucp1301_read_efuse(struct ucp1301_t *ucp1301)
{
	u32 val;
	int ret;

	ret = regmap_read_poll_timeout(ucp1301->regmap, REG_EFS_STATUS, val,
				       (val & BIT_EFS_IDLE),
				       UCP_READ_EFS_POLL_DELAY_US,
				       UCP_READ_EFS_POLL_TIMEOUT);
	if (ret) {
		dev_err(ucp1301->dev, "check bit BIT_EFS_IDLE fail, %d\n", ret);
		return ret;
	}
	ret = regmap_update_bits(ucp1301->regmap, REG_EFS_MODE_CTRL,
				 BIT_EFS_NORMAL_READ_START,
				 BIT_EFS_NORMAL_READ_START);
	if (ret) {
		dev_err(ucp1301->dev, "update BIT_EFS_NORMAL_READ_START fail, %d\n",
			ret);
		return ret;
	}
	ret = regmap_read_poll_timeout(ucp1301->regmap, REG_EFS_STATUS, val,
		((val & BIT_EFS_IDLE) && (val & BIT_EFS_NORMAL_RD_DONE_FLAG)),
				       UCP_READ_EFS_POLL_DELAY_US,
				       UCP_READ_EFS_POLL_TIMEOUT);
	if (ret) {
		dev_err(ucp1301->dev, "check bits BIT_EFS_IDLE, BIT_EFS_NORMAL_RD_DONE_FLAG fail, %d\n",
			ret);
		return ret;
	}

	ret = ucp1301_read_efs_data(ucp1301);
	if (ret) {
		dev_err(ucp1301->dev, "read efs data fail, %d\n", ret);
		return ret;
	}
	ret = regmap_update_bits(ucp1301->regmap, REG_EFS_MODE_CTRL,
				 BIT_EFS_NORMAL_READ_DONE_FLAG_CLR,
				 BIT_EFS_NORMAL_READ_DONE_FLAG_CLR);
	if (ret) {
		dev_err(ucp1301->dev, "update BIT_EFS_NORMAL_READ_DONE_FLAG_CLR fail, %d\n",
			ret);
		return ret;
	}

	if ((ucp1301->efs_data[0] & BIT_EFS_RD_DATA_L_PRO) > 0) {
		ucp1301->calib_code =
			(ucp1301->efs_data[0] &
			 BIT_EFS_RD_DATA_L_OSC1P6M_CLSD_TRIM(0x1f)) >> 9;
	} else {
		ucp1301->calib_code = 0xf;/* set default value if  */
		pr_warn("chip not calibrated, set default value 0xf for calib_code\n");
	}

	pr_info("read efuse, calib_code 0x%x\n", ucp1301->calib_code);

	return 0;
}

int ucp1301_hw_on(struct ucp1301_t *ucp1301, bool on)
{
	int ret;

	if (!ucp1301->reset_gpio) {
		dev_err(ucp1301->dev, "hw_on failed, reset_gpio error\n");
		return -EINVAL;
	}

	dev_dbg(ucp1301->dev, "hw_on, on %d\n", on);
	if (on) {
		gpiod_set_value_cansleep(ucp1301->reset_gpio, false);
		usleep_range(2000, 2050);
		gpiod_set_value_cansleep(ucp1301->reset_gpio, true);
		usleep_range(2000, 2050);
		ucp1301->hw_enabled = true;
	} else {
		ret = regmap_update_bits(ucp1301->regmap, REG_MODULE_EN,
					 BIT_CHIP_EN, 0);
		if (ret)
			dev_warn(ucp1301->dev, "hw_on, set BIT_CHIP_EN to 0 fail, %d\n",
				 ret);
		gpiod_set_value_cansleep(ucp1301->reset_gpio, false);
		usleep_range(2000, 2050);
		ucp1301->hw_enabled = false;
		ucp1301->mode_curt = HW_OFF;
	}

	return 0;
}

int ucp1301_audio_receiver(struct ucp1301_t *ucp1301, bool on)
{
	if (on) {
		ucp1301_hw_on(ucp1301, true);
		ucp1301_depop_ab_boost_bypass(ucp1301, true);
		ucp1301->mode_curt = RCV_AB;
	} else {
		ucp1301_depop_ab_boost_bypass(ucp1301, false);
		ucp1301_hw_on(ucp1301, false);
	}

	return 0;
}

int ucp1301_audio_speaker(struct ucp1301_t *ucp1301, bool on)
{
	if (on) {
		ucp1301_hw_on(ucp1301, true);
		ucp1301_depop_ab_boost_on(ucp1301, true);
		ucp1301->mode_curt = SPK_AB;
	} else {
		ucp1301_depop_ab_boost_on(ucp1301, false);
		ucp1301_hw_on(ucp1301, false);
	}

	return 0;
}

int ucp1301_audio_receiver_d(struct ucp1301_t *ucp1301, bool on)
{
	if (on) {
		ucp1301_hw_on(ucp1301, true);
		ucp1301_d_bypass(ucp1301, true);
		ucp1301->mode_curt = RCV_D;
	} else {
		ucp1301_d_bypass(ucp1301, false);
		ucp1301_hw_on(ucp1301, false);
	}
	return 0;
}

int ucp1301_audio_speaker_d(struct ucp1301_t *ucp1301, bool on)
{
	if (on) {
		ucp1301_hw_on(ucp1301, true);
		ucp1301_d_boost_on(ucp1301, true);
		ucp1301->mode_curt = SPK_D;
	} else {
		ucp1301_d_boost_on(ucp1301, false);
		ucp1301_hw_on(ucp1301, false);
	}

	return 0;
}

static ssize_t ucp1301_get_reg(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ucp1301_t *ucp1301 = i2c_get_clientdata(client);
	u32 reg, reg_val[5];
	int ret;
	ssize_t len = 0;
	u8 i;

	for (reg = REG_CHIP_ID_LOW; reg < UCP1301_REG_MAX; reg += 5) {
		for (i = 0; i < 5; i++) {
			ret = regmap_read(ucp1301->regmap, reg + i,
					  &reg_val[i]);
			if (ret < 0) {
				dev_err(ucp1301->dev, "get reg, read reg 0x%x fail, ret %d\n",
					reg + i - CTL_BASE_ANA_APB_IF, ret);
				return ret;
			}
		}
		len += sprintf(buf + len,
			       "0x%02x | %04x %04x %04x %04x %04x\n",
			       reg, reg_val[0], reg_val[1],
			       reg_val[2], reg_val[3], reg_val[4]);
	}
	/* read the last(max reg) reg, 0x32 */
	if (reg >= UCP1301_REG_MAX) {
		ret = regmap_read(ucp1301->regmap, reg, &reg_val[0]);
		if (ret < 0) {
			dev_err(ucp1301->dev, "get reg, read reg 0x%x fail, ret %d\n",
				reg - CTL_BASE_ANA_APB_IF, ret);
			return ret;
		}
	}
	len += sprintf(buf + len, "0x%02x | %04x\n", reg,
		       reg_val[0]);

	return len;
}

static ssize_t ucp1301_set_reg(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t len)
{
	u32 databuf[2];
	struct i2c_client *client = to_i2c_client(dev);
	struct ucp1301_t *ucp1301 = i2c_get_clientdata(client);

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) != 2) {
		dev_err(ucp1301->dev, "set reg, parse data fail\n");
		return -EINVAL;
	}

	dev_dbg(ucp1301->dev, "set reg, reg 0x%x --> val 0x%x\n",
		databuf[0], databuf[1]);
	regmap_write(ucp1301->regmap, databuf[0], databuf[1]);

	return len;
}

static ssize_t ucp1301_get_hw_state(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);

	return sprintf(buf, "hwenable: %d\n", ucp1301->hw_enabled);
}

static ssize_t ucp1301_set_hw_state(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t len)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);
	u32 databuf;
	int ret;

	ret = kstrtouint(buf, 10, &databuf);
	if (ret) {
		dev_err(ucp1301->dev, "set hw state, fail %d, buf %s\n",
			ret, buf);
		return ret;
	}

	if (databuf == 0)
		ucp1301_hw_on(ucp1301, false);
	else
		ucp1301_hw_on(ucp1301, true);

	return len;
}

static ssize_t ucp1301_get_mode(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);
	ssize_t len;

	len = sprintf(buf, "current mode %d\n", ucp1301->class_mode);
	len += sprintf(buf + len,
		       "mode: 0 hwoff, 1 spk ab, 2 rcv ab, 3 spk d, 4 rev d\n");

	return len;
}

static void ucp1301_save_mode(struct ucp1301_t *ucp1301, u32 mode_type)
{
	switch (mode_type) {
	case 0:
		ucp1301->class_mode = HW_OFF;
		break;
	case 1:
		ucp1301->class_mode = SPK_AB;
		break;
	case 2:
		ucp1301->class_mode = RCV_AB;
		break;
	case 3:
		ucp1301->class_mode = SPK_D;
		break;
	case 4:
		ucp1301->class_mode = RCV_D;
		break;
	default:
		dev_err(ucp1301->dev, "unknown mode type %d, set to default\n",
			mode_type);
		ucp1301->class_mode = SPK_D;
	}
}

static ssize_t ucp1301_set_mode(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);
	u32 mode_type;
	int ret;

	ret = kstrtouint(buf, 10, &mode_type);
	if (ret) {
		dev_err(ucp1301->dev, "set mode,fail %d, buf %s\n", ret, buf);
		return ret;
	}

	pr_info("set mode, mode_type %d, buf %s\n", mode_type, buf);
	ucp1301_save_mode(ucp1301, mode_type);

	return len;
}

/* return current AB/D mode, 1 class AB, 0 class D */
static ssize_t ucp1301_get_abd(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ucp1301_t *ucp1301 = i2c_get_clientdata(client);
	u32 val;
	int ret;

	ret = regmap_read(ucp1301->regmap, REG_CLSAB_REG0, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "get abd, read reg 0x%x fail, %d\n",
			REG_CLSAB_REG0, ret);
		return ret;
	}
	ucp1301->class_ab = (val & BIT_RG_CLSAB_MODE_EN) > 0 ? true : false;

	return sprintf(buf, "current class_ab %d\n", ucp1301->class_ab);
}

static ssize_t ucp1301_set_abd(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t len)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ucp1301_t *ucp1301 = i2c_get_clientdata(client);
	u32 databuf, val;
	int ret;

	ret = kstrtouint(buf, 10, &databuf);
	if (ret) {
		dev_err(ucp1301->dev, "set abd, fail %d, buf %s\n", ret, buf);
		return ret;
	}

	/* check current state */
	ret = regmap_read(ucp1301->regmap, REG_CLSAB_REG0, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "set abd, read reg 0x%x fail, %d\n",
			REG_CLSAB_REG0, ret);
		return ret;
	}
	ucp1301->class_ab = (val & BIT_RG_CLSAB_MODE_EN) > 0 ? true : false;
	pr_info("set abd, databuf %d, class_ab %d, buf %s\n",
		databuf, ucp1301->class_ab, buf);

	if (databuf == ucp1301->class_ab) {
		dev_err(ucp1301->dev, "class_ab was %d already, needn't to set\n",
			ucp1301->class_ab);
		return -EINVAL;
	}
	ucp1301_d_ab_switch(ucp1301, ucp1301->class_ab);

	return len;
}

/* return current Boost/bypass mode, 1 bypass, 0 boost */
static ssize_t ucp1301_get_bypass(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ucp1301_t *ucp1301 = i2c_get_clientdata(client);
	u32 val;
	int ret;

	ret = regmap_read(ucp1301->regmap, REG_BST_REG0, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "get bypass, read reg 0x%x fail, %d\n",
			REG_BST_REG0, ret);
		return ret;
	}
	ucp1301->bypass = (val & BIT_RG_BST_BYPASS) > 0 ? true : false;

	return sprintf(buf, "current bypass %d\n", ucp1301->bypass);
}

static ssize_t ucp1301_set_bypass(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t len)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);
	u32 databuf, val;
	int ret;

	ret = kstrtouint(buf, 10, &databuf);
	if (ret) {
		dev_err(ucp1301->dev, "set bypass, fail %d, buf %s\n",
			ret, buf);
		return ret;
	}

	/* check current state */
	ret = regmap_read(ucp1301->regmap, REG_BST_REG0, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "set bypass, read reg 0x%x fail, %d\n",
			REG_BST_REG0, ret);
		return ret;
	}
	ucp1301->bypass = (val & BIT_RG_BST_BYPASS) > 0 ? true : false;
	pr_info("set bypass, databuf %d, bypass %d, buf %s\n",
		databuf, ucp1301->bypass, buf);

	if (databuf == ucp1301->bypass) {
		dev_err(ucp1301->dev, "set bypass was %d already, needn't to set\n",
			ucp1301->bypass);
		return len;
	}
	ucp1301_bst_bypass_switch(ucp1301, ucp1301->bypass);

	return len;
}

/* return current VOSEL value */
static ssize_t ucp1301_get_vosel(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);
	u32 val, val2;
	int ret;

	/* read vosel */
	ret = regmap_read(ucp1301->regmap, REG_BST_REG1, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "get vosel, read reg 0x%x fail, %d\n",
			REG_BST_REG0, ret);
		return ret;
	}
	ucp1301->vosel = (val & BIT_RG_BST_VOSEL(0xf)) >> 7;

	/* read bypass state */
	ret = regmap_read(ucp1301->regmap, REG_BST_REG0, &val2);
	if (ret < 0) {
		dev_err(ucp1301->dev, "get vosel, read reg 0x%x fail, %d\n",
			REG_BST_REG0, ret);
		return ret;
	}
	ucp1301->bypass = (val2 & BIT_RG_BST_BYPASS) > 0 ? true : false;

	pr_info("get vosel, vosel 0x%x, bypass %d, REG1 0x%x\n",
		ucp1301->vosel, ucp1301->bypass, val);
	return sprintf(buf, "current vosel 0x%x, bypass %d\n",
		       ucp1301->vosel, ucp1301->bypass);
}

static ssize_t ucp1301_set_vosel(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t len)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);
	u32 databuf, val;
	int ret;

	ret = kstrtouint(buf, 16, &databuf);
	if (ret) {
		dev_err(ucp1301->dev, "set vosel, fail %d, buf %s\n", ret, buf);
		return ret;
	}

	/* check current state */
	ret = regmap_read(ucp1301->regmap, REG_BST_REG0, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "set vosel, read reg 0x%x fail, %d\n",
			REG_BST_REG0, ret);
		return ret;
	}
	ucp1301->bypass = (val & BIT_RG_BST_BYPASS) > 0 ? true : false;
	dev_dbg(ucp1301->dev, "set vosel, databuf 0x%x, bypass %d, buf %s\n",
		databuf, ucp1301->bypass, buf);

	if (ucp1301->bypass) {
		dev_err(ucp1301->dev, "you can't set vosel when bypass mode is on\n");
		return len;
	}
	regmap_update_bits(ucp1301->regmap, REG_BST_REG1, BIT_RG_BST_VOSEL(0xf),
			   BIT_RG_BST_VOSEL(databuf));

	return len;
}

static int ucp1301_read_clsd_trim(struct ucp1301_t *ucp1301, u32 *clsd_trim)
{
	u32 val;
	int ret;

	/* read clsd_trim */
	ret = regmap_read(ucp1301->regmap, REG_PMU_REG1, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "get clsd trim, read reg 0x%x fail, %d\n",
			REG_PMU_REG1, ret);
		return ret;
	}
	*clsd_trim = val & BIT_RG_PMU_OSC1P6M_CLSD_TRIM(0x1f);

	/* read bypass state */
	ret = regmap_read(ucp1301->regmap, REG_BST_REG0, &val);
	if (ret < 0) {
		dev_err(ucp1301->dev, "get clsd trim, read reg 0x%x fail, %d\n",
			REG_BST_REG0, ret);
		return ret;
	}
	ucp1301->bypass = (val & BIT_RG_BST_BYPASS) > 0 ? true : false;

	dev_dbg(ucp1301->dev, "get clsd trim, calib_code %d(0x%x), clsd_trim 0x%x, bypass %d\n",
		ucp1301->calib_code, ucp1301->calib_code, *clsd_trim,
		ucp1301->bypass);

	return 0;
}

/*
 * return current /ori clsd_trim value, ori clsd_trim value is
 * 0xf, corresponding clock output frequency is 1.6 MHz
 */
static ssize_t ucp1301_get_clsd_trim(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);
	u32 clsd_trim;
	int ret;

	ret = ucp1301_read_clsd_trim(ucp1301, &clsd_trim);
	if (ret)
		dev_warn(ucp1301->dev, "get clsd trim fail, %d\n", ret);

	return sprintf(buf,
		       "current calib_code %d(0x%x), clsd_trim 0x%x, bypass %d\n",
		       ucp1301->calib_code, ucp1301->calib_code,
		       clsd_trim, ucp1301->bypass);
}

/* the unit is KHz, if databuf[0] = 100, it means 100 KHz */
static ssize_t ucp1301_set_clsd_trim(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t len)
{
	struct ucp1301_t *ucp1301 = dev_get_drvdata(dev);
	u32 databuf;
	int ret;

	ret = kstrtouint(buf, 10, &databuf);
	if (ret) {
		dev_err(ucp1301->dev, "set clsd trim, fail %d, buf %s\n",
			ret, buf);
		return ret;
	}

	ucp1301->clsd_trim = databuf;
	ret = ucp1301_write_clsd_trim(ucp1301, ucp1301->clsd_trim);
	if (ret)
		return ret;

	return len;
}

static void ucp1301_ivsense_remote(struct ucp1301_t *ucp1301)
{
	u32 val, mask;

	mask = BIT_RG_AUD_PA_SVSNSAD | BIT_RG_AUD_PA_SISNSAD;
	regmap_update_bits(ucp1301->regmap, REG_IV_SENSE_FILTER_REG0, mask,
			   mask);

	mask = BIT_RG_AUD_AD_CLK_RST_V | BIT_RG_AUD_AD_CLK_RST_I |
		BIT_RG_AUD_ADC_V_RST | BIT_RG_AUD_ADC_I_RST |
		BIT_RG_AUD_AD_D_GATE_V | BIT_RG_AUD_AD_D_GATE_I;
	regmap_update_bits(ucp1301->regmap, REG_IV_SENSE_ADC_REG0, mask, 0);

	mask = BIT_RG_AUD_PA_VSNS_EN | BIT_RG_AUD_PA_ISNS_EN;
	regmap_update_bits(ucp1301->regmap, REG_IV_SENSE_FILTER_REG0, mask,
			   mask);

	mask = BIT_RG_AUD_ADPGA_IBIAS_EN | BIT_RG_AUD_VCM_VREF_BUF_EN |
		BIT_RG_AUD_AD_CLK_EN_V | BIT_RG_AUD_AD_CLK_EN_I |
		BIT_RG_AUD_ADC_V_EN | BIT_RG_AUD_ADC_I_EN;
	regmap_update_bits(ucp1301->regmap, REG_IV_SENSE_ADC_REG0, mask, mask);

	mask = BIT_RG_AUD_PA_VS_G(0x3) | BIT_RG_AUD_PA_IS_G(0x3);
	val = BIT_RG_AUD_PA_VS_G(0) | BIT_RG_AUD_PA_IS_G(0x3);
	regmap_update_bits(ucp1301->regmap, REG_IV_SENSE_FILTER_REG0, mask,
			   val);
}

static void ucp1301_ivsense_local(struct ucp1301_t *ucp1301)
{
	u32 mask;

	ucp1301_ivsense_remote(ucp1301);
	mask = BIT_RG_AUD_PA_ISNS_EN | BIT_RG_AUD_PA_SISNSAD;
	regmap_update_bits(ucp1301->regmap, REG_IV_SENSE_FILTER_REG0, mask,
			   mask);

	mask = BIT_RG_AUD_ADC_I_EN;
	regmap_update_bits(ucp1301->regmap, REG_IV_SENSE_ADC_REG0, mask, mask);
}

static void ucp1301_ivsense_set(struct ucp1301_t *ucp1301)
{
	enum ucp1301_ivsense_mode ivsense_mode = ucp1301->ivsense_mode;

	switch (ivsense_mode) {
	case IVS_UCP1300A:
		ucp1301_ivsense_local(ucp1301);
		break;
	case IVS_UCP1301:
		ucp1301_ivsense_remote(ucp1301);
		break;
	default:
		dev_info(ucp1301->dev, "ivsense_set, this is ucp1300b or others %d\n",
			 ivsense_mode);
	}
}

static ssize_t ucp1301_get_ivsense_mode(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ucp1301_t *ucp1301 = i2c_get_clientdata(client);
	ssize_t len;

	len = sprintf(buf,
		      "ivsense_mode %d\n", ucp1301->ivsense_mode);
	len += sprintf(buf + len, "mode: 0 remote, 1 local, 2 no\n");

	return len;
}

static ssize_t ucp1301_set_ivsense_mode(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ucp1301_t *ucp1301 = i2c_get_clientdata(client);
	u32 ivsense_mode;
	int ret;

	ret = kstrtouint(buf, 10, &ivsense_mode);
	if (ret) {
		dev_err(ucp1301->dev, "set ivsense, fail %d, buf %s\n",
			ret, buf);
		return ret;
	}

	if (ivsense_mode > IVS_UCP1300B) {
		dev_err(ucp1301->dev, "set ivsense, out of range %d\n",
			ivsense_mode);
		return -EINVAL;
	}

	ucp1301->ivsense_mode = ivsense_mode;
	dev_dbg(ucp1301->dev, "set ivsense, ivsense_mode %d\n",
		ucp1301->ivsense_mode);

	return len;
}

static DEVICE_ATTR(regs, 0660, ucp1301_get_reg, ucp1301_set_reg);
static DEVICE_ATTR(hwenable, 0660, ucp1301_get_hw_state, ucp1301_set_hw_state);
static DEVICE_ATTR(mode, 0660, ucp1301_get_mode, ucp1301_set_mode);
static DEVICE_ATTR(abd_switch, 0660, ucp1301_get_abd, ucp1301_set_abd);
static DEVICE_ATTR(bypass, 0660, ucp1301_get_bypass, ucp1301_set_bypass);
static DEVICE_ATTR(vosel, 0660, ucp1301_get_vosel, ucp1301_set_vosel);
static DEVICE_ATTR(clsd_trim, 0660, ucp1301_get_clsd_trim,
		   ucp1301_set_clsd_trim);
static DEVICE_ATTR(ivsense_mode, 0660, ucp1301_get_ivsense_mode,
		   ucp1301_set_ivsense_mode);

static struct attribute *ucp1301_attributes[] = {
	&dev_attr_regs.attr,
	&dev_attr_hwenable.attr,
	&dev_attr_mode.attr,
	&dev_attr_abd_switch.attr,
	&dev_attr_bypass.attr,
	&dev_attr_vosel.attr,
	&dev_attr_clsd_trim.attr,
	&dev_attr_ivsense_mode.attr,
	NULL
};

static struct attribute_group ucp1301_attribute_group = {
	.attrs = ucp1301_attributes
};

static int ucp1301_debug_sysfs_init(struct ucp1301_t *ucp1301)
{
	int ret;

	ret = sysfs_create_group(&ucp1301->dev->kobj, &ucp1301_attribute_group);
	if (ret < 0)
		dev_info(ucp1301->dev, "fail to create sysfs attr files\n");

	return ret;
}

void ucp1301_audio_on(bool on_off)
{
	enum ucp1301_audio_mode mode;

	pr_info("ucp1301 audio on, on_off %d, mode_to_set %d, mode_curt %d\n",
		on_off, ucp1301_g->mode_to_set, ucp1301_g->mode_curt);
	if (ucp1301_g->mode_to_set != ucp1301_g->mode_curt)
		mode = ucp1301_g->mode_to_set;
	else
		mode = ucp1301_g->mode_curt;

	switch (mode) {
	case HW_OFF:
		ucp1301_hw_on(ucp1301_g, on_off);
		break;
	case SPK_AB:
		ucp1301_audio_speaker(ucp1301_g, on_off);
		break;
	case RCV_AB:
		ucp1301_audio_receiver(ucp1301_g, on_off);
		break;
	case SPK_D:
		ucp1301_audio_speaker_d(ucp1301_g, on_off);
		break;
	case RCV_D:
		ucp1301_audio_receiver_d(ucp1301_g, on_off);
		break;
	default:
		dev_err(ucp1301_g->dev, "ucp1301 audio on, power down for mode error %d\n",
			mode);
		ucp1301_hw_on(ucp1301_g, false);
	}

	if (on_off && (mode == SPK_D || mode == RCV_D)) {
		ucp1301_ivsense_set(ucp1301_g);
		/* temp source code, wait for kcontrol about */
		regmap_update_bits(ucp1301_g->regmap, REG_AGC_GAIN0,
				   BIT_AGC_GAIN0(0x7f), BIT_AGC_GAIN0(0x57));
		regmap_update_bits(ucp1301_g->regmap, REG_AGC_EN, BIT_AGC_EN,
				   BIT_AGC_EN);
	}
}

static int ucp1301_parse_dt(struct ucp1301_t *ucp1301, struct device_node *np)
{
	ucp1301->reset_gpio = devm_gpiod_get_index(ucp1301->dev, "reset", 0,
						   GPIOD_ASIS);
	if (IS_ERR(ucp1301->reset_gpio)) {
		dev_err(ucp1301->dev, "parse 'reset-gpios' fail\n");
		return PTR_ERR(ucp1301->reset_gpio);
	}
	gpiod_direction_output(ucp1301->reset_gpio, 0);

	return 0;
}

static int ucp1301_read_chipid(struct ucp1301_t *ucp1301)
{
	u32 cnt = 0, chip_id = 0, val_temp;
	int ret;

	while (cnt < UCP_READ_CHIPID_RETRIES) {
		ret = regmap_read(ucp1301->regmap, REG_CHIP_ID_HIGH, &val_temp);
		if (ret < 0) {
			dev_err(ucp1301->dev, "read chip id high fail %d\n",
				ret);
			return ret;
		}
		chip_id = val_temp << 16;
		ret = regmap_read(ucp1301->regmap, REG_CHIP_ID_LOW, &val_temp);
		if (ret < 0) {
			dev_err(ucp1301->dev, "read chip id low fail %d\n",
				ret);
			return ret;
		}
		chip_id |= val_temp;

		if (chip_id == UCP1301_CHIP_ID) {
			pr_info("read chipid successful 0x%x\n", chip_id);
			return 0;
		}
		dev_err(ucp1301->dev,
			"read chipid fail, try again, 0x%x, cnt %d\n",
			chip_id, cnt);

		cnt++;
		msleep(UCP_READ_CHIPID_RETRY_DELAY);
	}

	return -EINVAL;
}

static int ucp1301_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;
	struct device *dev = &client->dev;
	struct ucp1301_t *ucp1301;
	struct regmap *regmap;
	int ret;

	ucp1301 = devm_kzalloc(&client->dev, sizeof(struct ucp1301_t),
			       GFP_KERNEL);
	if (!ucp1301)
		return -ENOMEM;

	ucp1301->dev = dev;
	ucp1301->i2c_client = client;
	i2c_set_clientdata(client, ucp1301);
	ucp1301_g = ucp1301;

	regmap = devm_regmap_init_i2c(client, &ucp1301_regmap_config);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(dev, "Failed to allocate register map: %d\n", ret);
		return ret;
	}
	ucp1301->regmap = regmap;

	ret = ucp1301_parse_dt(ucp1301, np);
	if (ret) {
		dev_err(&client->dev, "failed to parse device tree node\n");
		return ret;
	}

	ucp1301_hw_on(ucp1301, true);
	ret = ucp1301_read_chipid(ucp1301);
	if (ret < 0) {
		dev_err(&client->dev, "ucp1301_read_chipid failed ret=%d\n",
			ret);
		return ret;
	}
	ucp1301_read_efuse(ucp1301);

	ucp1301_debug_sysfs_init(ucp1301);
	ucp1301_hw_on(ucp1301, false);
	ucp1301->init_flag = true;
	ucp1301->mode_curt = SPK_D;
	ucp1301->mode_to_set = SPK_D;
	ucp1301->ivsense_mode_curt = true;
	ucp1301->ivsense_mode_to_set = true;

	return 0;
}

static int ucp1301_i2c_remove(struct i2c_client *client)
{
	struct ucp1301_t *ucp1301 = i2c_get_clientdata(client);

	sysfs_remove_group(&ucp1301->dev->kobj, &ucp1301_attribute_group);

	return 0;
}

static const struct i2c_device_id ucp1301_i2c_id[] = {
	{ UCP1301_I2C_NAME, 0 },
	{ }
};

static const struct of_device_id extpa_of_match[] = {
	{ .compatible = "sprd,ucp1301-smartpa" },
	{},
};

static struct i2c_driver ucp1301_i2c_driver = {
	.driver = {
		.name = UCP1301_I2C_NAME,
		.of_match_table = extpa_of_match,
	},
	.probe = ucp1301_i2c_probe,
	.remove = ucp1301_i2c_remove,
	.id_table    = ucp1301_i2c_id,
};

static int __init ucp1301_init(void)
{
	int ret;

	ret = i2c_add_driver(&ucp1301_i2c_driver);
	if (ret) {
		pr_err("ucp1301 init, Unable to register driver (%d)\n", ret);
		return ret;
	}
	return 0;
}

static void __exit ucp1301_exit(void)
{
	i2c_del_driver(&ucp1301_i2c_driver);
}

late_initcall(ucp1301_init);
module_exit(ucp1301_exit);

MODULE_DESCRIPTION("SPRD SMART PA UCP1301 driver");
MODULE_AUTHOR("Harvey Yin <harvey.yin@unisoc.com>");
