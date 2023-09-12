/*
*****************************************************************************
* Copyright by ams AG                                                       *
* All rights are reserved.                                                  *
*                                                                           *
* IMPORTANT - PLEASE READ CAREFULLY BEFORE COPYING, INSTALLING OR USING     *
* THE SOFTWARE.                                                             *
*                                                                           *
* THIS SOFTWARE IS PROVIDED FOR USE ONLY IN CONJUNCTION WITH AMS PRODUCTS.  *
* USE OF THE SOFTWARE IN CONJUNCTION WITH NON-AMS-PRODUCTS IS EXPLICITLY    *
* EXCLUDED.                                                                 *
*                                                                           *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         *
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS         *
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT  *
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,     *
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT          *
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     *
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY     *
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT       *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE     *
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.      *
*****************************************************************************
*/

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "ams_tcs3430.h"
#include "tcs3430_regs.h"
#ifdef CONFIG_SENSORS_TCS3430_WITH_LUX_CCT
#include "tcs3430_als.h"
#endif

#define TCS3430_CMD_ALS_INT_CLR 0x10
#define TCS3430_CMD_ALL_INT_CLR	0x98

static int tcs3430_read_all(struct tcs3430_chip *chip);
static void tcs3430_get_als(struct tcs3430_chip *chip);
static int tcs3430_als_mux_toggle(struct tcs3430_chip *chip);

enum tsc3430_amux_operation {
	TCS3430_X_ONLY,
	TCS3430_IR2_ONLY,
	TCS3430_X_IR2_TOGGLE,
};

enum tcs3430_ir_mux {
	TCS3430_AMUX_IR2_DATA = (1 << 3),
	TCS3430_AMUX_X_DATA   = (0 << 3),
};

enum tcs3430_en_reg {
	TCS3430_EN_PON        = (1 << 0),
	TCS3430_EN_AEN        = (1 << 1),
	TCS3430_EN_WEN        = (1 << 3),
};

enum tcs3430_status {
	TCS3430_ST_CINT       = (1 << 3),
	TCS3430_ST_AINT       = (1 << 4),
	TCS3430_ST_ASAT       = (1 << 7),
};

enum tcs3430_ctrl_reg {
	AGAIN_MASK_1   = (0 << 0),
	AGAIN_MASK_4   = (1 << 0),
	AGAIN_MASK_16  = (2 << 0),
	AGAIN_MASK_64  = (3 << 0),
};

static u8 const tcs3430_ids[] = {
	0xdc,
};

static char const *tcs3430_names[] = {
	"tcs3430",
};

static u8 const restorable_regs[] = {
	TCS3430_REG_ATIME,
	TCS3430_REG_PERS,
	TCS3430_REG_CFG1,
};

u8 const als_gains[] = {
	1,
	4,
	16,
	64
};

static u8 const reg_addr[] = {
	0x80,  /* ENABLE */
	0x81,  /* ATIME */
	0x83,  /* WTIME */
	0x84,  /* AILTL */
	0x85,  /* AILTH */
	0x86,  /* AIHTL */
	0x87,  /* AIHTH */
	0x8C,  /* PERS */
	0x90,  /* CFG1 */
	0x91,  /* REVID */
	0x92,  /* ID */
	0x93,  /* WTIME */
	0x94,  /* CH0DATAL */
	0x95,  /* CH0DATAH */
	0x96,  /* CH1DATAL */
	0x97,  /* CH1DATAH */
	0x98,  /* CH2DATAL */
	0x99,  /* CH2DATAH */
	0x9A,  /* CH3DATAL */
	0x9B,  /* CH3DATAH */
	0x9F,  /* CFG2 */
	0xAB,  /* CFG3 */
	0xD6,  /* AZ_CONFIG */
	0xDD,  /* INTENAB */
};

static int tcs3430_i2c_blk_read(struct tcs3430_chip *chip, u8 reg,
				u8 *val, int size)
{
	s32 ret;
	u8 addr;
	struct i2c_client *client = chip->client;
	
	addr = reg_addr[reg];
	ret = i2c_smbus_read_i2c_block_data(client, addr, size, val);
	if (ret < 0)
		dev_err(&client->dev, "%s: failed at address %x (%d bytes)\n",
			__func__, addr, size);
	return ret;
}

static int tcs3430_i2c_read(struct tcs3430_chip *chip, u8 reg, u8 *val)
{
	int ret;
	u8 addr;
	s32 read;
	struct i2c_client *client = chip->client;

	addr = reg_addr[reg];
	ret = i2c_smbus_write_byte(client, addr);
	if (ret < 0) {
		mdelay(3);
		ret = i2c_smbus_write_byte(client, addr);
		if (ret < 0) {
			dev_err(&client->dev,
				"%s: failed 2x to write register %x\n",
				__func__, addr);
			return ret;
		}
	}

	read = i2c_smbus_read_byte(client);
	if (read < 0) {
		mdelay(3);
		read = i2c_smbus_read_byte(client);
		if (read < 0) {
			dev_err(&client->dev,
				"%s: failed read from register %x\n",
				__func__, addr);
		}
		return ret;
	}

	*val = (u8)read;
	return 0;
}

static int tcs3430_i2c_write(struct tcs3430_chip *chip, u8 reg, u8 val)
{
	int ret;
	u8 addr;
	struct i2c_client *client = chip->client;

	addr = reg_addr[reg];
	ret = i2c_smbus_write_byte_data(client, addr, val);
	if (ret < 0) {
		mdelay(3);
		ret = i2c_smbus_write_byte_data(client, addr, val);
		if (ret < 0) {
			dev_err(&client->dev,
				"%s: failed to write register %x err= %d\n",
				__func__, addr, ret);
		}
	}

	return ret;
}

static int tcs3430_flush_regs(struct tcs3430_chip *chip)
{
	unsigned i;
	int rc;
	u8 reg;

	dev_info(&chip->client->dev, "%s\n", __func__);

	for (i = 0; i < ARRAY_SIZE(restorable_regs); i++) {
		reg = restorable_regs[i];
		rc = tcs3430_i2c_write(chip, reg, chip->shadow[reg]);
		if (rc) {
			dev_err(&chip->client->dev, "%s: err on reg 0x%02x\n",
					__func__, reg_addr[reg]);
			break;
		}
	}

	return rc;

}

static int tcs3430_update_azcfg_reg(struct tcs3430_chip *chip)
{
	return	tcs3430_i2c_write(chip, TCS3430_REG_AZ_CONFIG,
			chip->shadow[TCS3430_REG_AZ_CONFIG]);
}


static int tcs3430_update_enable_reg(struct tcs3430_chip *chip)
{
	return	tcs3430_i2c_write(chip, TCS3430_REG_ENABLE,
			chip->shadow[TCS3430_REG_ENABLE]);
}

static u8 tcs3430_gain_to_mask(int gain)
{
	u8 mask;

	switch (gain) {
	case 1:
		mask = AGAIN_MASK_1;
		break;
	case 4:
		mask = AGAIN_MASK_4;
		break;
	case 16:
		mask = AGAIN_MASK_16;
		break;
	case 64:
		mask = AGAIN_MASK_64;
		break;
	default:
		mask = 0xFF;
	}
	return mask;
}

static int tcs3430_set_als_gain(struct tcs3430_chip *chip, int gain)
{
	int rc;
	u8 gain_mask;
	u8 ctrl_reg;

	gain_mask = tcs3430_gain_to_mask(gain);
	if (gain_mask == 0xFF) {
		dev_err(&chip->client->dev, "%s: illegal als gain %d\n",
			__func__, gain);
		return -EINVAL;
	}

	ctrl_reg = chip->shadow[TCS3430_REG_CFG1] & ~TCS3430_ALS_GAIN_MASK;
	ctrl_reg |= gain_mask;
	rc = tcs3430_i2c_write(chip, TCS3430_REG_CFG1, ctrl_reg);
	if (!rc) {
		chip->shadow[TCS3430_REG_CFG1] = ctrl_reg;
		chip->params.als_gain = gain;
		dev_info(&chip->client->dev, "%s: new als gain %d\n",
			 __func__, gain);
	}
	return rc;
}

#ifdef CONFIG_SENSORS_TCS3430_WITH_LUX_CCT
static int tcs3430_get_lux(struct tcs3430_chip *chip)
{
	s32 lux = 0;
	s32 cct = 0;
	u32 sat = MAX_ALS_VALUE;

	if (chip->als_inf.y_raw <= MIN_ALS_VALUE) {
#ifdef LUX_MESSAGES
		dev_info(&chip->client->dev,
			 "%s: darkness\n", __func__);
#endif
		lux = 0;
		goto exit;
	} else if (chip->als_inf.y_raw >= sat) {
#ifdef LUX_MESSAGES
		dev_info(&chip->client->dev,
			 "%s: saturation, keep lux & cct\n", __func__);
#endif
		lux = chip->als_inf.lux;
		cct = chip->als_inf.cct;
		goto exit;
	}

	lux = tcs3430_calc_lux_fixed(chip);
	cct = tcs3430_calc_cct_fixed(chip);
exit:
	chip->als_inf.lux = (u16) lux;
	chip->als_inf.cct = (u16) cct;
	return 0;
}
#endif

static int tcs3430_pltf_power_on(struct tcs3430_chip *chip)
{
	int rc = 0;
	if (chip->pdata->platform_power) {
	//	rc = chip->pdata->platform_power(&chip->client->dev,
	//		POWER_ON);
		mdelay(10);
	}
	chip->unpowered = rc != 0;
	return rc;
}

static int tcs3430_pltf_power_off(struct tcs3430_chip *chip)
{
	int rc = 0;
	if (chip->pdata->platform_power) {
	//	rc = chip->pdata->platform_power(&chip->client->dev,
	//		POWER_OFF);
		chip->unpowered = rc == 0;
	} else {
		chip->unpowered = false;
	}
	return rc;
}

static int tcs3430_irq_clr(struct tcs3430_chip *chip, u8 int2clr)
{
	int ret, ret2;
	/*printk("%s %d %x", __func__, __LINE__, int2clr);*/
	ret = tcs3430_i2c_write(chip, TCS3430_REG_STATUS, int2clr);
	if (ret < 0) {
		mdelay(3);
	    ret2 = tcs3430_i2c_write(chip, TCS3430_REG_STATUS, int2clr);
		if (ret2 < 0) {
			dev_err(&chip->client->dev,
				"%s: failed 2x, to clear irq %02x\n",
				__func__, int2clr);
		}
		return ret2;
	}
	return ret;

}

static void tcs3430_get_als(struct tcs3430_chip *chip)
{
	u8 *buf = &chip->shadow[TCS3430_REG_CH0DATAL];

	if (chip->rev == 0) {
		/* extract raw X channel data */
		chip->als_inf.x_raw = le16_to_cpup((const __le16 *)&buf[0]);

		/* extract raw Y channel data */
		buf = &chip->shadow[TCS3430_REG_CH1DATAL];
		chip->als_inf.y_raw = le16_to_cpup((const __le16 *)&buf[0]);

		/* extract raw IR2 channel data */
		buf = &chip->shadow[TCS3430_REG_CH2DATAL];
		chip->als_inf.ir2_raw = le16_to_cpup((const __le16 *)&buf[0]);

		/* extract raw Z/IR1 channel data */
		buf = &chip->shadow[TCS3430_REG_CH3DATAL];
		if (chip->amux == 0) {
			chip->als_inf.z_raw =
				le16_to_cpup((const __le16 *)&buf[0]);
		} else {
			chip->als_inf.ir1_raw =
				le16_to_cpup((const __le16 *)&buf[0]);
		}

	} else {
		/* extract raw Z channel data */
		chip->als_inf.z_raw = le16_to_cpup((const __le16 *)&buf[0]);

		/* extract raw Y channel data */
		buf = &chip->shadow[TCS3430_REG_CH1DATAL];
		chip->als_inf.y_raw = le16_to_cpup((const __le16 *)&buf[0]);

		/* extract raw IR1 channel data */
		buf = &chip->shadow[TCS3430_REG_CH2DATAL];
		chip->als_inf.ir1_raw = le16_to_cpup((const __le16 *)&buf[0]);

		/* extract raw X/IR2 channel data */
		buf = &chip->shadow[TCS3430_REG_CH3DATAL];
		if (chip->amux == 0) {
			chip->als_inf.x_raw =
				le16_to_cpup((const __le16 *)&buf[0]);
		} else {
			chip->als_inf.ir2_raw =
				le16_to_cpup((const __le16 *)&buf[0]);
		}

	}
}

static int tcs3430_read_all(struct tcs3430_chip *chip)
{
	int ret = 0;

	ret = tcs3430_i2c_blk_read(chip, TCS3430_REG_CH0DATAL,
				   &chip->shadow[TCS3430_REG_CH0DATAL], 8);

	return (ret < 0) ? ret : 0;
}

#ifdef CONFIG_SENSORS_TCS3430_WITH_LUX_CCT
static void tcs3430_report_als(struct tcs3430_chip *chip)
{
	if (chip->a_idev) {
		int rc = tcs3430_get_lux(chip);
		if (!rc) {
			int lux = chip->als_inf.lux;
			input_report_abs(chip->a_idev, ABS_MISC, lux);
			input_sync(chip->a_idev);
		}
	}
}
#endif

static int tcs3430_irq_handler(struct tcs3430_chip *chip)
{
	u8 status;

	/* Read status first and check to see if there
	 * are pending interrupts for this device since
	 * the interrupt line is shared on the host platform.
	 */
	int ret = tcs3430_i2c_read(chip, TCS3430_REG_STATUS,
				   &chip->shadow[TCS3430_REG_STATUS]);

	if (ret) {
		ret = 0;
		goto exit_clr;
	}

	status = chip->shadow[TCS3430_REG_STATUS];
	if ((status & (TCS3430_ST_AINT)) == (TCS3430_ST_AINT)) {
		tcs3430_read_all(chip);  /* read ADC channels */
		tcs3430_get_als(chip);
#ifdef CONFIG_SENSORS_TCS3430_WITH_LUX_CCT
		tcs3430_report_als(chip);
#endif
		/* Toggle mode? */
		if (chip->amux_state == TCS3430_X_IR2_TOGGLE)
			tcs3430_als_mux_toggle(chip);

		tcs3430_irq_clr(chip, TCS3430_CMD_ALS_INT_CLR);
		ret = 1;
	} else {
		ret = 0;
	}

exit_clr:
	tcs3430_irq_clr(chip, TCS3430_CMD_ALL_INT_CLR);

	return ret;
}

static irqreturn_t tcs3430_irq(int irq, void *handle)
{
	struct tcs3430_chip *chip = handle;
	struct device *dev = &chip->client->dev;
	int ret;

	mutex_lock(&chip->lock);
	if (chip->in_suspend) {
		dev_info(dev, "%s: in suspend\n", __func__);
		chip->irq_pending = 1;
		disable_irq_nosync(chip->client->irq);
		ret = 0;
		goto bypass;
	}
	ret = tcs3430_irq_handler(chip);
bypass:
	mutex_unlock(&chip->lock);
	return ret ? IRQ_HANDLED : IRQ_NONE;
}

static void tcs3430_set_defaults(struct tcs3430_chip *chip)
{
	struct device *dev = &chip->client->dev;
	u8 gain_mask;

	if (chip->pdata) {
		dev_info(dev, "%s: Loading platform data\n", __func__);
		chip->params.als_time_ms = chip->pdata->parameters.als_time_ms;
		chip->params.persist  = chip->pdata->parameters.persist;
		chip->params.als_gain = chip->pdata->parameters.als_gain;
	} else {
		dev_info(dev, "%s: use defaults\n", __func__);
		chip->params.als_time_ms = TCS3430_DEFAULT_ATIME;
		chip->params.persist  = TCS3430_DEFAULT_PERSIST;
		chip->params.als_gain = TCS3430_DEFAULT_AGAIN;
	}

	chip->shadow[TCS3430_REG_ATIME] =
		TCS3430_MS_TO_ATIME(chip->params.als_time_ms);

	chip->shadow[TCS3430_REG_PERS]  = ALS_PERSIST(chip->params.persist);
	gain_mask = tcs3430_gain_to_mask(chip->params.als_gain);
	if (gain_mask != 0xFF) {
		chip->shadow[TCS3430_REG_CFG1]  = gain_mask;
	} else {
		dev_err(&chip->client->dev,
			"%s: illegal platform data gain %d\n",
			__func__, chip->params.als_gain);

		/* Set valid default */
		chip->params.als_gain = TCS3430_DEFAULT_AGAIN;
		chip->shadow[TCS3430_REG_CFG1]  = AGAIN_MASK_16;
	}

	tcs3430_flush_regs(chip);

/* ENABLE INTERRUPTS */
	tcs3430_i2c_write(chip, TCS3430_REG_INTENAB, 0x10);  /* set AIEN */
}

static int tcs3430_als_enable(struct tcs3430_chip *chip, int on)
{
	int rc;

	dev_info(&chip->client->dev, "%s: on = %d\n", __func__, on);
	if (on) {
		tcs3430_irq_clr(chip, TCS3430_CMD_ALS_INT_CLR);
		chip->shadow[TCS3430_REG_ENABLE] |=
				(TCS3430_EN_PON | TCS3430_EN_AEN);

		rc = tcs3430_update_enable_reg(chip);
		if (rc)
			return rc;
		mdelay(3);
	} else {
		chip->shadow[TCS3430_REG_ENABLE] &=
			~(TCS3430_EN_AEN);
		rc = tcs3430_update_enable_reg(chip);
		tcs3430_irq_clr(chip, TCS3430_CMD_ALS_INT_CLR);
	}

	chip->als_enabled = on;

	return rc;
}

#ifdef CONFIG_SENSORS_TCS3430_WITH_LUX_CCT
static ssize_t tcs3430_device_als_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcs3430_chip *chip = dev_get_drvdata(dev);

	dev_info(&chip->client->dev, "%s\n", __func__);

	mutex_lock(&chip->lock);
	tcs3430_read_all(chip);
	tcs3430_get_als(chip);
	tcs3430_get_lux(chip);
	mutex_unlock(&chip->lock);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.lux);
}
#endif

static ssize_t tcs3430_als_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcs3430_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_enabled);
}

static ssize_t tcs3430_als_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct tcs3430_chip *chip = dev_get_drvdata(dev);
	bool value;

	if (strtobool(buf, &value))
		return -EINVAL;

	if (value)
		tcs3430_als_enable(chip, 1);
	else
		tcs3430_als_enable(chip, 0);

	return size;
}

static ssize_t tcs3430_als_gain_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcs3430_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n",	chip->params.als_gain);
}

static ssize_t tcs3430_als_y_show(struct device *dev,
	struct device_attribute *attr, char *buf)
		{
	struct tcs3430_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.y_raw);
}

static ssize_t tcs3430_als_z_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcs3430_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.z_raw);
}

static ssize_t tcs3430_als_ir2_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcs3430_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.ir2_raw);
}

static ssize_t tcs3430_als_x_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcs3430_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.x_raw);
}

static ssize_t tcs3430_als_ir1_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcs3430_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.ir1_raw);
}

#ifdef CONFIG_SENSORS_TCS3430_WITH_LUX_CCT
static ssize_t tcs3430_als_cct_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcs3430_chip *chip = dev_get_drvdata(dev);
	tcs3430_read_all(chip);
	tcs3430_get_als(chip);
	tcs3430_get_lux(chip);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.cct);
}
#endif

static ssize_t tcs3430_als_gain_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	unsigned long gain;
	int i = 0;
	int rc;
	struct tcs3430_chip *chip = dev_get_drvdata(dev);

	rc = kstrtoul(buf, 10, &gain);

	if (rc)
		return -EINVAL;

	if (gain != 1 && gain != 4 && gain != 16 && gain != 64) {
		dev_err(&chip->client->dev, "%s: illegal als gain %d\n",
			__func__, (int)gain);
		return -EINVAL;
	}

	while (i < sizeof(als_gains)) {
		if (gain == als_gains[i])
			break;
		i++;
	}

	mutex_lock(&chip->lock);
	if (gain)
		rc = tcs3430_set_als_gain(chip, als_gains[i]);

	tcs3430_flush_regs(chip);
	mutex_unlock(&chip->lock);
	return rc ? rc : size;
}

static ssize_t tcs3430_als_persist_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcs3430_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n",
			(((chip->shadow[TCS3430_REG_PERS]) & 0x0f)));
}

static ssize_t tcs3430_als_persist_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	long persist;	int rc;
	struct tcs3430_chip *chip = dev_get_drvdata(dev);

	rc = kstrtoul(buf, 10, &persist);
	if (rc)
		return -EINVAL;

	mutex_lock(&chip->lock);
	chip->shadow[TCS3430_REG_PERS] &= 0xF0;
	chip->shadow[TCS3430_REG_PERS] |= ((u8)persist & 0x0F);

	tcs3430_flush_regs(chip);

	mutex_unlock(&chip->lock);
	return size;
}

static ssize_t tcs3430_als_atime_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcs3430_chip *chip = dev_get_drvdata(dev);
	int t;
	t = TCS3430_ATIME_TO_MS(chip->shadow[TCS3430_REG_ATIME]);
	return snprintf(buf, PAGE_SIZE, "%d (in ms)\n", t);
}

static ssize_t tcs3430_als_atime_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	long milliseconds;
	int rc;
	uint8_t atime;
	struct tcs3430_chip *chip = dev_get_drvdata(dev);

	rc = kstrtoul(buf, 10, &milliseconds);
	if (rc)
		return -EINVAL;

	atime = TCS3430_MS_TO_ATIME(milliseconds);
	mutex_lock(&chip->lock);
	chip->shadow[TCS3430_REG_ATIME] = atime;
	tcs3430_flush_regs(chip);
	mutex_unlock(&chip->lock);
	return size;
}

static ssize_t tcs3430_als_mux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tcs3430_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE,
			"%d\n", chip->amux_state);
}

static ssize_t tcs3430_als_mux_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	unsigned long mux_op;
	int rc;
	u8 cfg1;
	struct tcs3430_chip *chip = dev_get_drvdata(dev);

	rc = kstrtoul(buf, 10, &mux_op);
	if (rc || mux_op > 2)
		return -EINVAL;
	mutex_lock(&chip->lock);
	cfg1 = chip->shadow[TCS3430_REG_CFG1];
	chip->amux_state = mux_op;
	if (mux_op > 0) {
		chip->amux = 0xff;

		/* IR2 Mode
		   If mux_op is greater than 0, then
		   the mode is either IR2 only or toggle.
		   Only IR2 mode or X mode will cause a write to
		   CFG1:amux in this function.  If toggle
		   mode, the CFG1:amux bit is toggled in the
		   IRQ handler function after the channel data
		   has been read and stored in the appropriate
		   tcs3430_als_info field.
		*/
		if (mux_op == TCS3430_IR2_ONLY)
			cfg1 |= TCS3430_AMUX_IR2_DATA;
	} else {
		chip->amux = 0x00;
		/* X mode */
		cfg1 &= ~TCS3430_AMUX_IR2_DATA;
		cfg1 |= TCS3430_AMUX_X_DATA;
	}
	chip->shadow[TCS3430_REG_CFG1] = cfg1;
	tcs3430_flush_regs(chip);
	mutex_unlock(&chip->lock);
	return size;
}

static int tcs3430_als_mux_toggle(struct tcs3430_chip *chip)
{
	u8 cfg1;
	int ret;

	cfg1 = chip->shadow[TCS3430_REG_CFG1];

	if (cfg1 & TCS3430_AMUX_IR2_DATA) {
		cfg1 &= ~TCS3430_AMUX_IR2_DATA;  /* clear */
		cfg1 |= TCS3430_AMUX_X_DATA;
	} else {
		cfg1 &= ~TCS3430_AMUX_IR2_DATA;
		cfg1 |= TCS3430_AMUX_IR2_DATA;
	}

	chip->shadow[TCS3430_REG_CFG1] = cfg1;
	ret = tcs3430_i2c_write(chip, TCS3430_REG_CFG1,
				chip->shadow[TCS3430_REG_CFG1]);

	if (ret >= 0)
		chip->amux = ~chip->amux;

	return (ret < 0) ? ret : 0;
}

static struct device_attribute als_attrs[] = {
	__ATTR_RW(tcs3430_als_atime),
	__ATTR_RO(tcs3430_als_x),
	__ATTR_RO(tcs3430_als_y),
	__ATTR_RO(tcs3430_als_z),
	__ATTR_RO(tcs3430_als_ir1),
	__ATTR_RO(tcs3430_als_ir2),
	__ATTR_RW(tcs3430_als_gain),
	__ATTR_RW(tcs3430_als_enable),
	__ATTR_RW(tcs3430_als_persist),
	__ATTR_RW(tcs3430_als_mux),
#ifdef CONFIG_SENSORS_TCS3430_WITH_LUX_CCT
	__ATTR_RO(tcs3430_device_als_lux),
	__ATTR_RO(tcs3430_als_cct),
#endif
};

static int tcs3430_add_sysfs_interfaces(struct device *dev,
	struct device_attribute *a, int size)
{
	int i;
	for (i = 0; i < size; i++)
		if (device_create_file(dev, a + i))
			goto undo;
	return 0;
undo:
	for (; i >= 0 ; i--)
		device_remove_file(dev, a + i);
	dev_err(dev, "%s: failed to create sysfs interface\n", __func__);
	return -ENODEV;
}

static void tcs3430_remove_sysfs_interfaces(struct device *dev,
	struct device_attribute *a, int size)
{
	int i;
	for (i = 0; i < size; i++)
		device_remove_file(dev, a + i);
}

static int tcs3430_get_id(struct tcs3430_chip *chip, u8 *id, u8 *rev)
{
	u8 revid_raw;
	tcs3430_i2c_read(chip, TCS3430_REG_REVID, &revid_raw);
	*rev = revid_raw & 0x07;    /* Rev is in bits 2:0 */
	return tcs3430_i2c_read(chip, TCS3430_REG_ID, id);
}

static int tcs3430_power_on(struct tcs3430_chip *chip)
{
	int rc;
	rc = tcs3430_pltf_power_on(chip);
	if (rc)
		return rc;
	dev_info(&chip->client->dev, "%s: chip was off, restoring regs\n",
			__func__);
	return tcs3430_flush_regs(chip);
}
#if 0
static int tcs3430_als_idev_open(struct input_dev *idev)
{
	struct tcs3430_chip *chip = dev_get_drvdata(&idev->dev);
	int rc;
	bool als = chip->a_idev && chip->a_idev->users;

	dev_info(&idev->dev, "%s\n", __func__);
	mutex_lock(&chip->lock);
	if (chip->unpowered) {
		rc = tcs3430_power_on(chip);
		if (rc)
			goto chip_on_err;
	}
	rc = tcs3430_als_enable(chip, 1);
	if (rc && !als)
		tcs3430_pltf_power_off(chip);
chip_on_err:
	mutex_unlock(&chip->lock);
	return rc;
}

static void tcs3430_als_idev_close(struct input_dev *idev)
{
	struct tcs3430_chip *chip = dev_get_drvdata(&idev->dev);

	dev_info(&idev->dev, "%s\n", __func__);
	mutex_lock(&chip->lock);
	tcs3430_als_enable(chip, 0);
	mutex_unlock(&chip->lock);
}
#endif
static const struct of_device_id tcs3430_i2c_dt_ids[] = {
	{.compatible = "ams,tcs3430",},
	{},
};
MODULE_DEVICE_TABLE(of, tcs3430_i2c_dt_ids);

static int tcs3430_init_dt(struct ams_tcs3430_platform_data *pdata)
{
	struct device_node *np = pdata->of_node;
	const char *str;
	u32 val;

	if (!pdata->of_node)
		return 0;

	if (!of_property_read_string(np, "als_name", &str))
		pdata->als_name = str;

	if (!of_property_read_u32(np, "persist", &val))
		pdata->parameters.persist = val;

	//Stored as Q1 fixed point
	if (!of_property_read_u32(np, "als_gain", &val))
		pdata->parameters.als_gain = val;

	if (!of_property_read_u32(np, "als_time_ms", &val))
		pdata->parameters.als_time_ms = val;

	if (!of_property_read_u32(np, "int_config", &val))
		pdata->int_config = val;

/*	if (!of_property_read_u32(np, "als_auto_gain", &val))
		pdata->parameters.als_gain_auto = val;

	if (!of_property_read_u32(np, "als_deltap", &val))
		pdata->parameters.als_deltap = val;

	if (!of_property_read_u32(np, "als_time", &val))
		pdata->parameters.als_time = val;

	if (!of_property_read_u32(np, "d_factor", &val))
		pdata->parameters.d_factor = val;

	if (!of_property_read_u32(np, "ch0_coef0", &val))
		pdata->parameters.lux_segment[0].ch0_coef = val;

	if (!of_property_read_u32(np, "ch1_coef0", &val))
		pdata->parameters.lux_segment[0].ch1_coef = val;

	if (!of_property_read_u32(np, "ch0_coef1", &val))
		pdata->parameters.lux_segment[1].ch0_coef = val;

	if (!of_property_read_u32(np, "ch1_coef1", &val))
		pdata->parameters.lux_segment[1].ch1_coef = val;
*/
	if (!of_property_read_u32(np, "als_can_wake", &val))
		pdata->als_can_wake = (val == 0) ? false : true;

	if (!of_property_read_u32(np, "gpios", &val)){
		pdata->int_gpio = of_get_named_gpio_flags(np, "gpios", 0, NULL);//val;
		printk("%s %d %d %d", __func__,__LINE__,of_get_gpio(np, 0),pdata->int_gpio);
	}
	return 0;
}

#define TCS3430_INT_GPIO 240//133//112+128=240 l5pro
static int tcs3430_probe(struct i2c_client *client,
				   const struct i2c_device_id *idp)
{
	int i, ret;
	u8 id, rev;
	bool powered = false;
	unsigned long default_irq_trigger = 0;

	struct device *dev = &client->dev;
	static struct tcs3430_chip *chip;
	struct ams_tcs3430_platform_data *pdata = dev->platform_data;
	printk("%s E\n", __func__);

	if (!pdata) {
		pdata = kzalloc(sizeof(struct ams_tcs3430_platform_data),
				GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;

		if (of_match_device(tcs3430_i2c_dt_ids, &client->dev)) {
			pdata->of_node = client->dev.of_node;
			ret = tcs3430_init_dt(pdata);
			if (ret)
				return ret;
		}
	}

	if (!(pdata->als_name) || client->irq < 0) {
		dev_err(dev, "%s: no reason to run.\n", __func__);
		ret = -EINVAL;
		goto init_failed;
	}

	chip = kzalloc(sizeof(struct tcs3430_chip), GFP_KERNEL);
	if (!chip) {
		ret = -ENOMEM;
		goto malloc_failed;
	}
	chip->client = client;
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);

	ret = tcs3430_get_id(chip, &id, &rev);
    if (ret < 0) {
        goto get_id_failed;
    } else {
	    chip->rev = rev;
	    dev_info(dev, "%s: device id:%02x device rev:%02x\n",
		     __func__, id, rev);
    }

	for (i = 0; i < ARRAY_SIZE(tcs3430_ids); i++) {
		if (id == tcs3430_ids[i])
			break;
	}
	if (i < ARRAY_SIZE(tcs3430_names)) {
		dev_info(dev, "%s: '%s rev. %d' detected\n", __func__,
			 tcs3430_names[i], rev);
		chip->device_index = i;
	} else {
		dev_err(dev, "%s: not supported chip id\n", __func__);
		ret = -EOPNOTSUPP;
		goto id_failed;
	}
	mutex_init(&chip->lock);
	tcs3430_set_defaults(chip);
	ret = tcs3430_flush_regs(chip);
	if (ret)
		goto flush_regs_failed;

	if (pdata->platform_power) {
	//	pdata->platform_power(dev, POWER_OFF);
		powered = false;
		chip->unpowered = true;
	}

	if (!pdata->als_name)
		goto input_a_alloc_failed;
	chip->a_idev = input_allocate_device();
	if (!chip->a_idev) {
		dev_err(dev, "%s: no memory for input_dev '%s'\n",
			__func__, pdata->als_name);
		ret = -ENODEV;
		goto input_a_alloc_failed;
	}

	chip->a_idev->name = pdata->als_name;
	chip->a_idev->id.bustype = BUS_I2C;
	set_bit(EV_ABS, chip->a_idev->evbit);
	set_bit(ABS_MISC, chip->a_idev->absbit);
	input_set_abs_params(chip->a_idev, ABS_MISC, 0, 65535, 0, 0);
//	chip->a_idev->open = tcs3430_als_idev_open;
//	chip->a_idev->close = tcs3430_als_idev_close;
	dev_set_drvdata(&chip->a_idev->dev, chip);
	ret = input_register_device(chip->a_idev);
	if (ret) {
		input_free_device(chip->a_idev);
		dev_err(dev, "%s: cant register input '%s'\n",
			__func__, pdata->als_name);
		goto input_a_alloc_failed;
	}
	ret = tcs3430_add_sysfs_interfaces(&chip->a_idev->dev,
					   als_attrs, ARRAY_SIZE(als_attrs));
	if (ret)
		goto input_a_sysfs_failed;

    if (pdata->int_gpio <= 0)
		pdata->int_gpio = TCS3430_INT_GPIO;

	if (gpio_is_valid(pdata->int_gpio))
		ret = devm_gpio_request(dev, pdata->int_gpio, "gpio_tcs3430");
	else
		dev_err(dev,"invalid gpio %d",pdata->int_gpio);
	if (ret < 0) {
		dev_err(dev, "gpio_request error %d\n", pdata->int_gpio);
		return -EINVAL;
	}

	ret = gpio_direction_input(pdata->int_gpio);
	if (ret < 0) {
		dev_err(dev, "gpio_direction_input error\n");
		return -EINVAL;
	}

	client->irq = gpio_to_irq(pdata->int_gpio);
	/* interrupt enable  : irq 0 is not allowed */
	if (!client->irq) {
		dev_err(dev, "No IRQ configured\n");
		return -EINVAL;
	}
	default_irq_trigger =
	    irqd_get_trigger_type(irq_get_irq_data(client->irq));

	ret = devm_request_threaded_irq(dev, client->irq, NULL, &tcs3430_irq,
			   default_irq_trigger | IRQF_SHARED | IRQF_ONESHOT,
			   dev_name(dev), chip);
	if (ret) {
		dev_err(dev, "Failed to request irq %d\n", client->irq);
		goto irq_register_fail;
	}
	chip->shadow[TCS3430_REG_AZ_CONFIG] = 0xff;

	tcs3430_update_azcfg_reg(chip);

	chip->shadow[TCS3430_REG_ENABLE] =
		(TCS3430_EN_PON | TCS3430_EN_AEN);

	tcs3430_update_enable_reg(chip);
	chip->amux_state = TCS3430_X_ONLY;
	chip->amux = 0x00;
	dev_info(dev, "Probe ok.\n");

	printk("%s Probe ok %d %d.\n", __func__, __LINE__,pdata->int_gpio);

	return 0;

irq_register_fail:
	gpio_free(pdata->int_gpio);
	if (chip->a_idev) {
		tcs3430_remove_sysfs_interfaces(&chip->a_idev->dev,
						als_attrs,
						ARRAY_SIZE(als_attrs));
input_a_sysfs_failed:
		input_unregister_device(chip->a_idev);
	}
input_a_alloc_failed:
flush_regs_failed:
id_failed:
get_id_failed:
	i2c_set_clientdata(client, NULL);
    kfree(chip);
malloc_failed:
init_failed:
	kfree(pdata);
	dev_err(dev, "Probe failed.\n");
	return ret;
}

static int tcs3430_suspend(struct device *dev)
{
	struct tcs3430_chip *chip = dev_get_drvdata(dev);
	struct ams_tcs3430_platform_data *pdata = dev->platform_data;
	dev_info(dev, "%s\n", __func__);
	mutex_lock(&chip->lock);
	chip->in_suspend = 1;

	/* Blocks if work is active */
	if (chip->a_idev && chip->a_idev->users) {
		if (pdata->als_can_wake) {
			dev_info(dev, "set wake on als\n");
			chip->wake_irq = 1;
		} else {
			dev_info(dev, "als off\n");
			tcs3430_als_enable(chip, 0);
		}
	}
	if (chip->wake_irq) {
		irq_set_irq_wake(chip->client->irq, 1);
	} else if (!chip->unpowered) {
		dev_info(dev, "powering off\n");
		tcs3430_pltf_power_off(chip);
	}
	mutex_unlock(&chip->lock);

	return 0;
}

static int tcs3430_resume(struct device *dev)
{
	struct tcs3430_chip *chip = dev_get_drvdata(dev);
	bool als_on;
	int rc = 0;
	mutex_lock(&chip->lock);
	als_on = chip->a_idev && chip->a_idev->users;
	chip->in_suspend = 0;

	dev_info(dev, "%s: powerd %d, als: needed %d  enabled %d",
			__func__, !chip->unpowered, als_on,
			chip->als_enabled);

	if (chip->wake_irq) {
		irq_set_irq_wake(chip->client->irq, 0);
		chip->wake_irq = 0;
	}
	if (chip->unpowered && (als_on)) {
		dev_info(dev, "powering on\n");
		rc = tcs3430_power_on(chip);
		if (rc)
			goto err_power;
	}

	if (als_on && !chip->als_enabled)
		(void)tcs3430_als_enable(chip, 1);
	if (chip->irq_pending) {
		dev_info(dev, "%s: pending interrupt\n", __func__);
		chip->irq_pending = 0;
		(void)tcs3430_irq_handler(chip);
		enable_irq(chip->client->irq);
	}
err_power:
	mutex_unlock(&chip->lock);

	return 0;
}

static int tcs3430_remove(struct i2c_client *client)
{
	struct tcs3430_chip *chip = i2c_get_clientdata(client);
	//free_irq(client->irq, chip);
	if (chip->a_idev) {
		tcs3430_remove_sysfs_interfaces(&chip->a_idev->dev,
			als_attrs, ARRAY_SIZE(als_attrs));
		input_unregister_device(chip->a_idev);
	}
	gpio_free(chip->pdata->int_gpio);
	if (chip->pdata->platform_teardown)
		chip->pdata->platform_teardown(&client->dev);
	i2c_set_clientdata(client, NULL);
#ifdef CONFIG_OF
	kfree(chip->pdata);
#endif
	kfree(chip);
	return 0;
}

static struct i2c_device_id tcs3430_idtable[] = {
	{ "tcs3430", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, tcs3430_idtable);

static const struct dev_pm_ops tcs3430_pm_ops = {
	.suspend = tcs3430_suspend,
	.resume  = tcs3430_resume,
};

static struct i2c_driver tcs3430_driver = {
	.driver = {
		.name = "tcs3430",
		.pm = &tcs3430_pm_ops,
	},
	.id_table = tcs3430_idtable,
	.probe = tcs3430_probe,
	.remove = tcs3430_remove,
};

static int __init tcs3430_init(void)
{
	return i2c_add_driver(&tcs3430_driver);
}

static void __exit tcs3430_exit(void)
{
	i2c_del_driver(&tcs3430_driver);
}

module_init(tcs3430_init);
module_exit(tcs3430_exit);

MODULE_AUTHOR("AMS AOS Software<cs.americas@ams.com>");
MODULE_DESCRIPTION("AMS TCS3430 ambient light and color (XYZ) sensor");
MODULE_LICENSE("Dual MIT/GPL");
