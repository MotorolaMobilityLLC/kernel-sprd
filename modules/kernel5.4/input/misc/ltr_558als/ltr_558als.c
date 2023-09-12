/*
 * File:ltr_558als.c
 * Author:Yaochuan Li <yaochuan.li@spreadtrum.com>
 * Created:2013-03-18
 * Description:LTR-558ALS Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/uaccess.h>
//#include <linux/wakelock.h>
#include "ltr_558als.h"

#define LTR558_DBG
#ifdef LTR558_DBG
#define MODNAME				"[LTR558]"
#undef pr_fmt
#define pr_fmt(fmt)  MODNAME"%s %d:" fmt, __func__, __LINE__
#define PRINT_FUN()        pr_info("\n")
#define PRINT_ERR(fmt, ...)  pr_err(fmt, ##__VA_ARGS__)
#define PRINT_INFO(fmt, ...)  pr_info(fmt, ##__VA_ARGS__)
#define PRINT_DBG(fmt, ...)  pr_debug(fmt, ##__VA_ARGS__)
#else
#define PRINT_DBG(x...)
#define PRINT_INFO(x...)  printk(KERN_INFO "[LTR558_INFO] " x)
#define PRINT_ERR(format, x...)  printk(KERN_ERR "[LTR558_ERR] func: %s  line: %04d  info: " format, __func__, __LINE__, ## x)
#endif

typedef struct tag_ltr558 {
	struct input_dev *input;
	struct i2c_client *client;
	struct work_struct work;
	struct workqueue_struct *ltr_work_queue;
} ltr558_t, *ltr558_p;

static int ps_threshold_high = 600;
static int ps_threshold_low = 500;
/* prox data max range */
static int dyna_cali = 2047;

static int last_als_value = -1;

static int p_flag;
static int l_flag;
static u8 p_gainrange = PS_RANGE4;
static u8 l_gainrange = ALS_RANGE1_320;
static struct i2c_client *this_client;
static int LTR_PLS_MODE;

#ifdef CONFIG_ARCH_WHALE
#define PS_N_PULSES_VAL  0x03
#else
#define PS_N_PULSES_VAL 0x0A
#endif

static int ltr558_reg_init(void);
//static struct wake_lock psensor_timeout_wakelock;
static struct wakeup_source *ws;
#define WAKELOCK_TIMEOUT_INT_MS 50
#define WAKELOCK_TIMEOUT_WORK_MS 500
/*param tab*/
struct ltr558_params {
	u16	thresh1;
	u16	thresh2;
	u16	thresh3;
	u16	thresh4;
	u16	thresh5;
	u16	thresh6;
	u32	lux_correction;/* *n/10000 */
};
static struct ltr558_params param_group = {1500, 250, 220, 1800, 1600, ALS_553_RANGE1_64K, 10000};

static int ltr558_i2c_read_bytes(u8 index, u8 *rx_buff, u8 length)
{
	int ret = -1;
	struct i2c_msg msgs[] = {
		{
			.addr = this_client->addr,/* chip address, 7bit */
			.flags = 0,/* write */
			.len = 1,
			.buf = &index,
		},
		{
			.addr = this_client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = rx_buff,
		},
	};

	ret = i2c_transfer(this_client->adapter, msgs, 2);
	if (ret != 2)
		PRINT_ERR("READ ERROR!ret=%d\n", ret);
	return ret;
}

static int ltr558_i2c_write_bytes(u8 *tx_buff, u8 length)
{
	int ret = -1;
	struct i2c_msg msgs[1];

	msgs[0].addr = this_client->addr;
	msgs[0].flags = 0;
	msgs[0].len = length;
	msgs[0].buf = tx_buff;

	ret = i2c_transfer(this_client->adapter, msgs, 1);
	if (ret != 1)
		PRINT_ERR("WRITE ERROR!ret=%d\n", ret);
	return ret;
}

static int ltr558_i2c_read_2_bytes(u8 reg)
{
	int ret = 0;
	u8 data[2] = {0};

	ret = ltr558_i2c_read_bytes(reg, data, 2);
	if (ret != 2) {
		PRINT_ERR("READ ERROR!ret=%d\n", ret);
		return -1;
	}

	ret = data[1];
	ret = (ret<<8) | data[0];

	return ret;
}

static int ltr558_i2c_read_1_byte(u8 reg)
{
	int ret = 0;
	u8 data[1] = {0};

	ret = ltr558_i2c_read_bytes(reg, data, 1);
	if (ret != 2) {
		PRINT_ERR("READ ERROR!ret=%d\n", ret);
		return -1;
	}

	ret = data[0];
	return ret;
}

static int ltr558_i2c_write_2_bytes(u8 reg, u16 value)
{
	int ret = 0;
	u8 data[3] = {0};

	data[0] = reg;
	data[1] = value & 0x00FF;
	data[2] = value >> 8;

	ret = ltr558_i2c_write_bytes(data, 3);
	if (ret != 1) {
		PRINT_ERR("WRITE ERROR!ret=%d\n", ret);
		return -1;
	}
	return 1;
}

static int ltr558_i2c_write_1_byte(u8 reg, u8 value)
{
	int ret = 0;
	u8 data[2] = {0};

	data[0] = reg;
	data[1] = value;

	ret = ltr558_i2c_write_bytes(data, 2);
	if (ret != 1) {
		PRINT_ERR("WRITE ERROR!ret=%d\n", ret);
		return -1;
	}
	return 1;
}

static int ltr_read_chip_info(struct i2c_client *client, char *buf)
{
	if (buf == NULL)
		return -1;
	if (client == NULL) {
		*buf = 0;
		return -2;
	}

	if (LTR_PLS_553 == LTR_PLS_MODE) {
		sprintf(buf, "LTR558ALS");
		PRINT_INFO("[LTR553] ltr_read_chip_info LTR553ALS\n");
	}
	if (LTR_PLS_558 == LTR_PLS_MODE) {
		sprintf(buf, "LTR558ALS");
		PRINT_INFO("[LTR558] ltr_read_chip_info LTR558ALS\n");
	}
	return 0;
}

static void dynamic_calibrate(void)
{
	int i = 0;
	int val = 0;
	int data_total = 0;
	int noise = 0;

	for (i = 0; i < 3; i++) {
		msleep(20);
		val = ltr558_i2c_read_2_bytes(LTR558_PS_DATA_0);
		data_total += val;
	}
	noise = data_total/3;
	if (noise < (dyna_cali + 500)) {
		dyna_cali = noise;
		if (noise < param_group.thresh1) {
			ps_threshold_high = noise + param_group.thresh2;
			ps_threshold_low = noise + param_group.thresh3;
		} else {
			ps_threshold_high = param_group.thresh4;
			ps_threshold_low = param_group.thresh5;
		}
	}
	PRINT_INFO("%s noise=%d ps_threshold_high=%d, ps_threshold_low=%d\n", __func__, noise, ps_threshold_high, ps_threshold_low);
	ltr558_i2c_write_1_byte(LTR558_PS_THRES_UP_0, ps_threshold_high & 0xff);
	ltr558_i2c_write_1_byte(LTR558_PS_THRES_UP_1, (ps_threshold_high>>8) & 0x07);
	ltr558_i2c_write_1_byte(LTR558_PS_THRES_LOW_0, ps_threshold_low & 0xff);
	ltr558_i2c_write_1_byte(LTR558_PS_THRES_LOW_1, (ps_threshold_low>>8) & 0x07);
}

static int ltr558_ps_enable(u8 gainrange)
{
	int ret = -1;
	u8 setgain;

	if (LTR_PLS_553 == LTR_PLS_MODE) {
		switch	(gainrange) {
		case PS_553_RANGE16:
			setgain = MODE_PS_553_ON_Gain16;
			break;
		case PS_553_RANGE32:
			setgain = MODE_PS_553_ON_Gain32;
			break;
		case PS_553_RANGE64:
			setgain = MODE_PS_553_ON_Gain64;
			break;
		default:
			setgain = MODE_PS_553_ON_Gain16;
			break;
		}
	} else {
		switch	(gainrange) {
		case PS_558_RANGE1:
			setgain = MODE_PS_558_ON_Gain1;
			break;
		case PS_558_RANGE2:
			setgain = MODE_PS_558_ON_Gain4;
			break;
		case PS_558_RANGE4:
			setgain = MODE_PS_558_ON_Gain8;
			break;
		case PS_558_RANGE8:
			setgain = MODE_PS_558_ON_Gain16;
			break;
		default:
			setgain = MODE_PS_558_ON_Gain8;
			break;
		}
	}

	ret = ltr558_i2c_write_1_byte(LTR558_PS_CONTR, setgain);
	mdelay(WAKEUP_DELAY);

	if (setgain != ltr558_i2c_read_1_byte(LTR558_PS_CONTR)) {
		ret = ltr558_i2c_write_1_byte(LTR558_PS_CONTR, setgain);
		mdelay(WAKEUP_DELAY);
	}
	/*-----modified by hongguang@wecorp for dynamic calibrate------*/

	dynamic_calibrate();

	PRINT_INFO("ltr558_ps_enable, gainrange=%d, ret=%d\n", gainrange, ret);
	/*-----modified by hongguang@wecorp for dynamic calibrate------*/
	if (ret >= 0)
		return 0;
	else
		return ret;
}

static int ltr558_ps_disable(void)
{
	int ret = -1;

	ret = ltr558_i2c_write_1_byte(LTR558_PS_CONTR, MODE_PS_STANDBY);

	PRINT_INFO("ltr558_ps_disable, ret=%d\n", ret);
	if (ret >= 0)
		return 0;
	else
		return ret;
}

static int ltr558_als_enable(u8 gainrange)
{
	int ret = -1;
	u8 setgain;

	if (LTR_PLS_553 == LTR_PLS_MODE) {
		switch (gainrange) {
		case ALS_553_RANGE1_64K:
			setgain = MODE_ALS_553_ON_Range1;
			break;

		case ALS_553_RANGE2_32K:
			setgain = MODE_ALS_553_ON_Range2;
			break;

		case ALS_553_RANGE4_16K:
			setgain = MODE_ALS_553_ON_Range4;
			break;

		case ALS_553_RANGE8_8K:
			setgain = MODE_ALS_553_ON_Range8;
			break;

		case ALS_553_RANGE48_1K3:
			setgain = MODE_ALS_553_ON_Range48;
			break;

		case ALS_553_RANGE96_600:
			setgain = MODE_ALS_553_ON_Range96;
			break;

		default:
			setgain = MODE_ALS_553_ON_Range1;
			break;
		}
	} else {
		switch (gainrange) {
		case  ALS_558_RANGE1_320:
			setgain = MODE_ALS_558_ON_Range1;
			break;
		case ALS_558_RANGE2_64K:
			setgain = MODE_ALS_558_ON_Range2;
			break;
		default:
			setgain = MODE_ALS_558_ON_Range1;
			break;
		}
	}

	ltr558_i2c_write_1_byte(LTR558_INTERRUPT, 0x03);
	/* als */
	ltr558_i2c_write_1_byte(LTR558_ALS_THRES_UP_0, 0x00);
	ltr558_i2c_write_1_byte(LTR558_ALS_THRES_UP_1, 0x00);
	ltr558_i2c_write_1_byte(LTR558_ALS_THRES_LOW_0, 0x01);
	ltr558_i2c_write_1_byte(LTR558_ALS_THRES_LOW_1, 0x00);
	mdelay(WAKEUP_DELAY);

	ret = ltr558_i2c_write_1_byte(LTR558_ALS_CONTR, setgain);
	mdelay(WAKEUP_DELAY);

	if (setgain != ltr558_i2c_read_1_byte(LTR558_ALS_CONTR)) {
		ret = ltr558_i2c_write_1_byte(LTR558_ALS_CONTR, setgain);
		mdelay(WAKEUP_DELAY);
	}

	ltr558_i2c_read_1_byte(LTR558_ALS_PS_STATUS);
	ltr558_i2c_read_2_bytes(LTR558_PS_DATA_0);
	ltr558_i2c_read_2_bytes(LTR558_ALS_DATA_CH1);
	ltr558_i2c_read_2_bytes(LTR558_ALS_DATA_CH0);

	last_als_value = -1;
	PRINT_INFO("ltr558_als_enable, gainrange=%d, ret = %d\n", gainrange, ret);
	if (ret >= 0)
		return 0;
	else
		return ret;
}

/* Put ALS into Standby mode */
static int ltr558_als_disable(void)
{
	int ret = -1;

	/* als */
	ret = ltr558_i2c_write_1_byte(LTR558_ALS_CONTR, MODE_ALS_STANDBY);
	PRINT_INFO("ltr558_als_disable, ret=%d\n", ret);

	ltr558_i2c_write_1_byte(LTR558_INTERRUPT, 0x01);
	ltr558_i2c_write_1_byte(LTR558_ALS_THRES_UP_0, 0xFF);
	ltr558_i2c_write_1_byte(LTR558_ALS_THRES_UP_1, 0xFF);
	ltr558_i2c_write_1_byte(LTR558_ALS_THRES_LOW_0, 0x00);
	ltr558_i2c_write_1_byte(LTR558_ALS_THRES_LOW_1, 0x00);
	mdelay(WAKEUP_DELAY);

	if (ret >= 0)
		return 0;
	else
		return ret;
}

static int ltr558_als_read(int gainrange)
{
	int luxdata_int;
	int ratio;
	int ch0, ch1;

	/*IMPORTANT!CH1 MUST BE READ FIRST!*/
	ch1 = ltr558_i2c_read_2_bytes(LTR558_ALS_DATA_CH1);
	ch0 = ltr558_i2c_read_2_bytes(LTR558_ALS_DATA_CH0);

	PRINT_DBG("ch0=%d,  ch1=%d\n", ch0, ch1);
	if (0 == (ch0 + ch1))
		ratio = 100;
	else
		ratio = (ch1 * 100) / (ch0 + ch1);

	/* Compute Lux data from ALS data (ch0 and ch1) */
	/* modified by hongguang@wecorp */
	if (ratio < 45)
		luxdata_int = (((17743 * ch0)+(11059 * ch1)))/10000;
	else if (ratio < 64)
		luxdata_int = (((42785 * ch0)-(19548 * ch1)))/10000;
	else if (ratio < 85)
		luxdata_int = (((5926 * ch0)+(1185 * ch1)))/10000;
	else
		luxdata_int = 0;

	/* modified by hongguang@wecorp */
	/* lux modify */
	if (param_group.lux_correction > 0)/*u32*/
		luxdata_int = luxdata_int*param_group.lux_correction/10000;
	return luxdata_int;
}

static int ltr558_open(struct inode *inode, struct file *file)
{
	PRINT_INFO("ltr558_open\n");
	return 0;
}

static int ltr558_release(struct inode *inode, struct file *file)
{
	PRINT_INFO("ltr558_release\n");
	return 0;
}

static long ltr558_misc_ioctl_handler(struct file *file, unsigned int cmd,
					  unsigned long arg, int compat_mode)
{
	void __user *argp = (void __user *)arg;
	int flag, err;
	char strbuf[256];

	PRINT_INFO("ioctl cmd %d,compat_mode=%d\n",
			_IOC_NR(cmd), compat_mode);
	switch (cmd) {
	case LTR_IOCTL_SET_PFLAG: {
		  if (copy_from_user(&flag, argp, sizeof(flag)))
			  return -EFAULT;
		  PRINT_INFO("SET_PFLAG = %d\n", flag);
		  if (flag == 1) {
			  ltr558_reg_init();
			  msleep(20);
			  if (ltr558_ps_enable(p_gainrange))
				  return -EIO;
		  } else if (flag == 0) {
			  if (ltr558_ps_disable())
				  return -EIO;
		  } else
			  return -EINVAL;
		  p_flag = flag;
	  }
	  break;
	case LTR_IOCTL_SET_LFLAG: {
		  if (copy_from_user(&flag, argp, sizeof(flag)))
			  return -EFAULT;
		  PRINT_INFO("SET_LFLAG = %d\n", flag);
		  if (flag == 1) {
			  ltr558_reg_init();
			  msleep(20);
			  if (ltr558_als_enable(l_gainrange))
				  return -EIO;
		  } else if (flag == 0) {
			  if (ltr558_als_disable())
				  return -EIO;
		  } else
			  return -EINVAL;
		  l_flag = flag;
	  }
	  break;
	case LTR_IOCTL_GET_CHIPINFO: {
		     err = ltr_read_chip_info(this_client, strbuf);
		     if (err < 0)
			     return -EFAULT;
		     if (copy_to_user(argp, strbuf, strlen(strbuf)+1))
			     return -EFAULT;
	     }
	     break;
	case LTR_IOCTL_GET_PFLAG: {
		  flag = p_flag;
		  PRINT_INFO("PFLAG = %d\n", flag);
		  if (copy_to_user(argp, &flag, sizeof(flag)))
			  return -EFAULT;
	  }
	  break;
	case LTR_IOCTL_GET_LFLAG: {
		  flag = l_flag;
		  PRINT_INFO("LFLAG=%d\n", flag);
		  if (copy_to_user(argp, &flag, sizeof(flag)))
			  return -EFAULT;
	  }
	  break;
	default:
	  PRINT_ERR("unknown cmd:0x%08X(%d)\n", cmd, cmd);
	  break;
	}

	return 0;
}

static long ltr558_ioctl(struct file *file,
					 unsigned int cmd, unsigned long arg)
{
	return ltr558_misc_ioctl_handler(file, cmd, arg, 0);
}

#ifdef CONFIG_COMPAT
static long ltr558_misc_ioctl_compat(struct file *file,
					 unsigned int cmd, unsigned long arg)
{
	return ltr558_misc_ioctl_handler(file, cmd, arg, 1);
}
#endif

static const struct file_operations ltr558_fops = {
	 .owner = THIS_MODULE,
	 .open = ltr558_open,
	 .release = ltr558_release,
	 .unlocked_ioctl = ltr558_ioctl,
#ifdef CONFIG_COMPAT
	 .compat_ioctl = ltr558_misc_ioctl_compat,
#endif
};

static struct miscdevice ltr558_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = LTR558_I2C_NAME,
	.fops = &ltr558_fops,
};

static void ltr558_work(struct work_struct *work)
{
	int status = 0;
	int value = 0;
	ltr558_t *pls = container_of(work, ltr558_t, work);

	status = ltr558_i2c_read_1_byte(LTR558_ALS_PS_STATUS);
	while (status < 0 && value < 5) {/*100ms+*/
		/*add 1s time out lock in plsensor interrupt*/
		mdelay(100);
		PRINT_INFO("Repeat=%d\n", value);
		value++;
		status = ltr558_i2c_read_1_byte(LTR558_ALS_PS_STATUS);
	}
	PRINT_INFO("LTR558_ALS_PS_STATUS = 0x%02x\n", status);
	if ((0x03 == (status & 0x03)) && (LTR_PLS_MODE == LTR_PLS_558)) {/*is 558 PS*/
		value = ltr558_i2c_read_2_bytes(LTR558_PS_DATA_0);
		PRINT_INFO("LTR_PLS_MODE is pls 558, LTR558_PS_DATA_0 = %d\n", value);
		/* modified by hongguang@wecorp for dynamic calibrate */
		if (value >= ps_threshold_high) {	  /* 3cm high */
			ltr558_i2c_write_2_bytes(LTR558_PS_THRES_UP, 0x07FF);
			ltr558_i2c_write_2_bytes(LTR558_PS_THRES_LOW, ps_threshold_low);
			input_report_abs(pls->input, ABS_DISTANCE, 0);
			input_sync(pls->input);
			PRINT_INFO("PS = 0\n");
		} else if (value <= ps_threshold_low) {		/* 5cm low */
			if (dyna_cali > 20 && value < (dyna_cali - 100)) {
				if (value < param_group.thresh1) {
					ps_threshold_high = value + param_group.thresh2;
					ps_threshold_low = value + param_group.thresh3;
				} else {
					ps_threshold_high = param_group.thresh4;
					ps_threshold_low = param_group.thresh5;
				}
			}
			/* wake lock only when report 1 */
			__pm_wakeup_event(ws, msecs_to_jiffies(WAKELOCK_TIMEOUT_WORK_MS));
			ltr558_i2c_write_2_bytes(LTR558_PS_THRES_UP, ps_threshold_high);
			ltr558_i2c_write_2_bytes(LTR558_PS_THRES_LOW, 0x0000);
			input_report_abs(pls->input, ABS_DISTANCE, 5);
			input_sync(pls->input);
			PRINT_INFO("PS = 5\n");
		}
	}
	if ((0x03 == (status & 0x03)) && (LTR_PLS_MODE == LTR_PLS_553)) {/*is 553 PS*/
		value = ltr558_i2c_read_2_bytes(LTR558_PS_DATA_0);
		PRINT_INFO("LTR is 553,PS_DATA_0=%d\n", value);
		if (value >= ps_threshold_high) {
			ltr558_i2c_write_2_bytes(LTR558_PS_THRES_UP, 0x07FF);
			ltr558_i2c_write_2_bytes(LTR558_PS_THRES_LOW, ps_threshold_low);
			input_report_abs(pls->input, ABS_DISTANCE, 0);
			input_sync(pls->input);
			PRINT_INFO("PS = 0\n");
		} else if (value <= ps_threshold_low) {
			/* handle exception: phone is covered when prox sensor is turned on */
			/* modified by hongguang@wecorp for dynamic calibrate */
			if (dyna_cali > 20 && value < (dyna_cali - 100)) {
				if (value < param_group.thresh1) {
					ps_threshold_high = value + param_group.thresh2;
					ps_threshold_low = value + param_group.thresh3;
				} else {
					ps_threshold_high = param_group.thresh4;
					ps_threshold_low = param_group.thresh5;
				}
			}
			/* wake lock only when report 1 */
			__pm_wakeup_event(ws, msecs_to_jiffies(WAKELOCK_TIMEOUT_WORK_MS));

			ltr558_i2c_write_2_bytes(LTR558_PS_THRES_UP, ps_threshold_high);
			ltr558_i2c_write_2_bytes(LTR558_PS_THRES_LOW, 0x0000);
			input_report_abs(pls->input, ABS_DISTANCE, 5);
			input_sync(pls->input);
			PRINT_INFO("PS = 5\n");
		}
	}
	if (0x0c == (status & 0x0c)) {/*is ALS*/
		value = ltr558_als_read(l_gainrange);
		PRINT_INFO("ALS INT ALS_DATA_VAL=0x%04X\n", value);
		if (value != last_als_value) {
			input_report_abs(pls->input, ABS_MISC, value);
			input_sync(pls->input);
			last_als_value = value;
		}
	}
	enable_irq(pls->client->irq);
}

/* hold low level until als & ps data register are read */
static irqreturn_t ltr558_irq_handler(int irq, void *dev_id)
{
	ltr558_t *pls = (ltr558_t *) dev_id;

	/* PRINT_INFO("irq handler\n"); */
	__pm_wakeup_event(ws, msecs_to_jiffies(WAKELOCK_TIMEOUT_INT_MS));
	disable_irq_nosync(pls->client->irq);
	queue_work(pls->ltr_work_queue, &pls->work);
	return IRQ_HANDLED;
}

static int ltr558_reg_init(void)
{
	if (LTR_PLS_558 == LTR_PLS_MODE) {
		ltr558_i2c_write_1_byte(LTR558_PS_LED, 0x7B);
		ltr558_i2c_write_1_byte(LTR558_PS_N_PULSES, 0x1f);
		ltr558_i2c_write_1_byte(LTR558_PS_MEAS_RATE, 0x00);
		ltr558_i2c_write_1_byte(LTR558_ALS_MEAS_RATE, 0x13);
		ltr558_i2c_write_1_byte(LTR558_INTERRUPT_PERSIST, 0x02);
		/* set: INT MODE=updated after every measurement,
		 *=active low level,
		 *=both PS & ALS measurement can trigger interrupt
		 */
		ltr558_i2c_write_1_byte(LTR558_INTERRUPT, 0x0B);
	} else {
		ltr558_i2c_write_1_byte(LTR558_PS_LED, 0x7B);
		ltr558_i2c_write_1_byte(LTR558_PS_N_PULSES, PS_N_PULSES_VAL);
		ltr558_i2c_write_1_byte(LTR558_PS_MEAS_RATE, 0x00);
		ltr558_i2c_write_1_byte(LTR558_ALS_MEAS_RATE, 0x0b);
		ltr558_i2c_write_1_byte(LTR558_INTERRUPT_PERSIST, 0x00);
		/* set: INT MODE=updated after every measurement,
		 * =active low level,
		 * =both PS & ALS measurement can trigger interrupt
		 */
		ltr558_i2c_write_1_byte(LTR558_INTERRUPT, 0x03);
	}

	/* als */
	ltr558_i2c_write_1_byte(LTR558_ALS_THRES_UP_0, 0x00);
	ltr558_i2c_write_1_byte(LTR558_ALS_THRES_UP_1, 0x00);
	ltr558_i2c_write_1_byte(LTR558_ALS_THRES_LOW_0, 0x01);
	ltr558_i2c_write_1_byte(LTR558_ALS_THRES_LOW_1, 0x00);
	msleep(WAKEUP_DELAY);
	PRINT_INFO("ltr558_reg_init success!\n");
	return 0;
}

static int ltr558_version_check(void)
{
	int part_id = -1;
	int manufacturer_id = -1;

	part_id = ltr558_i2c_read_1_byte(LTR558_PART_NUMBER_ID);
	manufacturer_id = ltr558_i2c_read_1_byte(LTR558_MANUFACTURER_ID);
	PRINT_INFO("PART_ID: 0x%02X MANUFACTURER_ID: 0x%02X\n", part_id, manufacturer_id);

	if (part_id == 0x80 && manufacturer_id == 0x05) {
		PRINT_INFO("I'm LTR558, and I'm working now\n");
		return 0;
	} else if (part_id == 0x92 && manufacturer_id == 0x05) {
		PRINT_INFO("I'm LTR553, and I'm working now\n");
		return 0;
	}
	PRINT_ERR("can't read who am I\n");
	return -1;
}

static ssize_t ltr558_register_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int address, value;
	int len;

	/* echo "85 8" > regs */
	/* 0x85: 0x08 */
	len = sscanf(buf, "%3x %3x", &address, &value);
	PRINT_INFO("address=0x%02x, value=0x%02x\n", address, value);
	ltr558_i2c_write_1_byte(address, value);

	return count;
}

static ssize_t ltr558_register_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;
	int i;
	u8 val;

	for (i = 0x80; i < 0x9f; i++) {
		val = ltr558_i2c_read_1_byte(i);
		count += sprintf(buf + count, "0x%02x: 0x%02x\n", i, val);
	}

	return count;
}

static ssize_t ltr558_als_integ_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int comres = 0;

	comres =  ltr558_i2c_read_1_byte(LTR558_ALS_INTEG_TIME__REG);
	if (comres < 0)
		return -EINVAL;
	PRINT_INFO("comres=0x%02x\n", comres);
	data = LTR558_GET_BITSLICE(comres,
			LTR558_ALS_INTEG_TIME);

	return sprintf(buf, "0x%02x\n", data);
}

static ssize_t ltr558_als_integ_time_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int error;
	unsigned long newdata;
	int comres = 0;

	error = kstrtoul(buf, 10, &newdata);
	if (error)
		return error;

	comres =  ltr558_i2c_read_1_byte(LTR558_ALS_INTEG_TIME__REG);
	PRINT_INFO("comres=0x%02x newdata=0x%02x\n", comres, (unsigned char)newdata);
	newdata = LTR558_SET_BITSLICE(comres,
			LTR558_ALS_INTEG_TIME, newdata);
	PRINT_INFO("newdata=0x%02x\n", (unsigned char)newdata);
	comres = ltr558_i2c_write_1_byte(LTR558_ALS_INTEG_TIME__REG, newdata);
	if (comres < 0)
		return -EINVAL;

	return count;
}
static ssize_t ltr558_als_measure_rate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int comres = 0;

	comres =  ltr558_i2c_read_1_byte(LTR558_ALS_MEAS_RATE__REG);
	if (comres < 0)
		return -EINVAL;
	PRINT_INFO("comres=0x%02x\n", comres);
	data = LTR558_GET_BITSLICE(comres,
			LTR558_ALS_MEAS_RATE);

	return sprintf(buf, "0x%02x\n", data);
}

static ssize_t ltr558_als_measure_rate_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int error;
	unsigned long newdata;
	int comres = 0;

	error = kstrtoul(buf, 10, &newdata);
	if (error)
		return error;

	comres =  ltr558_i2c_read_1_byte(LTR558_ALS_MEAS_RATE__REG);
	PRINT_INFO("comres=0x%02x newdata=0x%02x\n", comres, (unsigned char)newdata);
	newdata = LTR558_SET_BITSLICE(comres,
			LTR558_ALS_MEAS_RATE, newdata);
	PRINT_INFO("newdata=0x%02x\n", (unsigned char)newdata);
	comres = ltr558_i2c_write_1_byte(LTR558_ALS_MEAS_RATE__REG, newdata);
	if (comres < 0)
		return -EINVAL;

	return count;
}

static ssize_t ltr558_ps_noise_show(struct device *dev,
		struct device_attribute *attr, char *buf){
	int i = 0;
	int val = 0;
	int data_total = 0;
	int noise = 0;

	for (i = 0; i < 5; i++) {
		msleep(20);
		val = ltr558_i2c_read_2_bytes(LTR558_PS_DATA_0);
		data_total += val;
	}
	noise = data_total/5;

	return sprintf(buf, "%d\n", noise);
}


static DEVICE_ATTR(regs, 0644, ltr558_register_show, ltr558_register_store);
static DEVICE_ATTR(als_integ_time, 0644, ltr558_als_integ_time_show, ltr558_als_integ_time_store);
static DEVICE_ATTR(als_measure_rate, 0644, ltr558_als_measure_rate_show, ltr558_als_measure_rate_store);
static DEVICE_ATTR(ps_noise, 0444, ltr558_ps_noise_show, NULL);

static struct attribute *ltr558_attributes[] = {
	&dev_attr_regs.attr,
	&dev_attr_als_measure_rate.attr,
	&dev_attr_als_integ_time.attr,
	NULL
};

static struct attribute_group ltr558_attribute_group = {
	.attrs = ltr558_attributes
};

static int ltr_558als_sysfs_init(struct input_dev *input_dev)
{
	struct kobject *ltr_558als_kobj;
	int ret = -1;

	ltr_558als_kobj = kobject_create_and_add("ltr_558als", &(input_dev->dev.kobj));
	if (ltr_558als_kobj == NULL) {
		ret = -ENOMEM;
		PRINT_ERR("register sysfs failed. ret = %d\n", ret);
		return ret;
	}

	ret = sysfs_create_group(ltr_558als_kobj,
			&ltr558_attribute_group);
	if (ret) {
		PRINT_ERR("create sysfs failed. ret = %d\n", ret);
		return ret;
	}
	return ret;
}

static int ltr558_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	ltr558_t *ltr_558als = NULL;
	struct input_dev *input_dev = NULL;
	struct ltr558_pls_platform_data *pdata = client->dev.platform_data;
	int chip_id = 0;
	struct class *pls_class;
	struct device *pls_cmd_dev;

#ifdef CONFIG_OF
	struct device_node *np = client->dev.of_node;
	int datatemp[6] = {0};

	if (np && !pdata) {
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Could not allocate struct ltr558_pls_platform_data");
			goto exit_allocate_pdata_failed;
		}
		/* 2015-9-2luciddle:read sensitive from dts */
		ret = of_property_read_u32_array(np, "sensitive", datatemp, 6);
		if (ret || 0 == datatemp[0]) {
			PRINT_INFO("Read sensitive erro.\n");
			param_group.thresh1 = 1500;
			param_group.thresh2 = 250;
			param_group.thresh3 = 220;
			param_group.thresh4 = 1800;
			param_group.thresh5 = 1600;
			param_group.thresh6 = ALS_553_RANGE1_64K;
		} else {
			param_group.thresh1 = (u16)datatemp[0];
			param_group.thresh2 = (u16)datatemp[1];
			param_group.thresh3 = (u16)datatemp[2];
			param_group.thresh4 = (u16)datatemp[3];
			param_group.thresh5 = (u16)datatemp[4];
			param_group.thresh6 = (u16)datatemp[5];
		}
		PRINT_INFO("Sensitive param is %d,%d,%d,%d,%d,%d.\n", param_group.thresh1, param_group.thresh2, param_group.thresh3
				, param_group.thresh4, param_group.thresh5, param_group.thresh6);
		ret = of_property_read_u32(np, "luxcorrection", datatemp);
		if (ret || 0 == datatemp[0])
			param_group.lux_correction = 10000;
		else
			param_group.lux_correction = (u32)datatemp[0];

		PRINT_INFO("10000times, lux_correction=%d\n", param_group.lux_correction);
		/*2015/9/2 end*/
		pdata->irq_gpio_number = of_get_gpio(np, 0);
		if (pdata->irq_gpio_number < 0) {
			dev_err(&client->dev, "fail to get irq_gpio_number\n");
			kfree(pdata);
			goto exit_irq_gpio_read_fail;
		}
		client->dev.platform_data = pdata;
	}
#endif
	ret = gpio_request(pdata->irq_gpio_number, LTR558_PLS_IRQ_PIN);
	if (ret) {
		PRINT_ERR("gpio_request failed!\n");
		goto exit_gpio_request_failed;
	}
	gpio_direction_input(pdata->irq_gpio_number);
	client->irq = gpio_to_irq(pdata->irq_gpio_number);
	PRINT_INFO("client->irq = %d\n", client->irq);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		PRINT_ERR("i2c_check_functionality failed!\n");
		ret = -ENODEV;
		goto exit_i2c_check_functionality_failed;
	}

	ltr_558als = kzalloc(sizeof(ltr558_t), GFP_KERNEL);
	if (!ltr_558als) {
		PRINT_ERR("kzalloc failed!\n");
		ret = -ENOMEM;
		goto exit_kzalloc_failed;
	}

	i2c_set_clientdata(client, ltr_558als);
	ltr_558als->client = client;
	this_client = client;

	chip_id = ltr558_i2c_read_1_byte(LTR558_PART_ID);
	if (chip_id < 0) {
		PRINT_ERR("ltr558 or ltr553 read chip_id failed!\n");
		ret = -ENOMEM;
		goto exit_read_chip_id_failed;
	}
	if (LTR_553_PART_ID == chip_id) {
		LTR_PLS_MODE = LTR_PLS_553;
		p_gainrange = PS_553_RANGE16;
		l_gainrange = param_group.thresh6; /*ALS_553_RANGE1_64K;150727 light gain*/
	} else {
		LTR_PLS_MODE = LTR_PLS_558;
		p_gainrange = PS_558_RANGE4;
		if (ALS_558_RANGE1_320 == param_group.thresh6 ||
				ALS_558_RANGE2_64K == param_group.thresh6) {
			l_gainrange = param_group.thresh6;
		} else {
			l_gainrange = ALS_558_RANGE1_320;
		}
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		PRINT_ERR("input_allocate_device failed!\n");
		ret = -ENOMEM;
		goto exit_input_allocate_device_failed;
	}

	input_dev->name = LTR558_INPUT_DEV;
	input_dev->phys = LTR558_INPUT_DEV;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0010;
	ltr_558als->input = input_dev;

	__set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, -1);
	input_set_abs_params(input_dev, ABS_MISC, 0, 100001, 0, -1);
	ret = input_register_device(input_dev);
	if (ret < 0) {
		PRINT_ERR("input_register_device failed!\n");
		input_free_device(input_dev);
		input_dev = NULL;
		goto exit_input_register_device_failed;
	}
	ret = misc_register(&ltr558_device);
	if (ret) {
		PRINT_ERR("misc_register failed!\n");
		goto exit_misc_register_failed;
	}
	if (ltr558_reg_init() < 0) {
		PRINT_ERR("ltr558_reg_init failed!\n");
		ret = -1;
		goto exit_ltr558_reg_init_failed;
	}
	ret = ltr558_version_check();
	if (ret) {
		PRINT_ERR("ltr558_version_check failed!\n");
		goto exit_ltr558_version_check_failed;
	}
	INIT_WORK(&ltr_558als->work, ltr558_work);
	/* freezable workqueue ensure runnnig work before
	 * device_suspend and after device_resume
	 */
	ltr_558als->ltr_work_queue =
		create_freezable_workqueue(LTR558_I2C_NAME);
	if (!ltr_558als->ltr_work_queue) {
		PRINT_ERR("create_singlethread_workqueue failed!\n");
		goto exit_create_singlethread_workqueue_failed;
	}
	ws = wakeup_source_create("psensor timeout wakelock");
	wakeup_source_add(ws);
	if (client->irq > 0) {
		/* irq may missed if edge falling when als/ps are both enable */
		ret = request_irq(client->irq, ltr558_irq_handler,
				IRQ_TYPE_LEVEL_LOW | IRQF_NO_SUSPEND,
				client->name, ltr_558als);
		if (ret < 0) {
			PRINT_ERR("request_irq failed!\n");
			goto exit_request_irq_failed;
		}
	}
	ret = ltr_558als_sysfs_init(input_dev);
	if (ret) {
		PRINT_ERR("ltr_558als_sysfs_init failed!\n");
		goto exit_ltr_558als_sysfs_init_failed;
	}

	pls_class = class_create(THIS_MODULE, "xr-pls");
	if (IS_ERR(pls_class))
		PRINT_ERR("Failed to create class(xr-pls)!\n");
	pls_cmd_dev = device_create(pls_class, NULL, 0, NULL, "device");
	if (IS_ERR(pls_cmd_dev))
		PRINT_ERR("Failed to create device(pls_cmd_dev)!\n");
	if (device_create_file(pls_cmd_dev, &dev_attr_ps_noise) < 0) {
		PRINT_ERR("Failed to create device file(%s)!\n",
			dev_attr_ps_noise.attr.name);
	}

	PRINT_INFO("probe success!\n");
	return 0;

exit_ltr_558als_sysfs_init_failed:
	free_irq(ltr_558als->client->irq, ltr_558als);
exit_request_irq_failed:
	destroy_workqueue(ltr_558als->ltr_work_queue);
	ltr_558als->ltr_work_queue = NULL;
	wakeup_source_remove(ws);
	wakeup_source_destroy(ws);
exit_create_singlethread_workqueue_failed:
exit_ltr558_version_check_failed:
exit_ltr558_reg_init_failed:
	misc_deregister(&ltr558_device);
exit_misc_register_failed:
	input_unregister_device(input_dev);
exit_input_register_device_failed:
exit_input_allocate_device_failed:
exit_read_chip_id_failed:
	kfree(ltr_558als);
	ltr_558als = NULL;
exit_kzalloc_failed:
exit_i2c_check_functionality_failed:
	gpio_free(pdata->irq_gpio_number);
exit_gpio_request_failed:
exit_irq_gpio_read_fail:
exit_allocate_pdata_failed:
	PRINT_ERR("probe failed!\n");
	return ret;
}

static int ltr558_remove(struct i2c_client *client)
{
	ltr558_t *ltr_558als = i2c_get_clientdata(client);

	wakeup_source_remove(ws);
	wakeup_source_destroy(ws);
	flush_workqueue(ltr_558als->ltr_work_queue);
	destroy_workqueue(ltr_558als->ltr_work_queue);
	ltr_558als->ltr_work_queue = NULL;

	misc_deregister(&ltr558_device);
	input_unregister_device(ltr_558als->input);
	ltr_558als->input = NULL;
	free_irq(ltr_558als->client->irq, ltr_558als);
	kfree(ltr_558als);
	ltr_558als = NULL;
	this_client = NULL;

	PRINT_INFO("ltr558_remove\n");
	return 0;
}

static int ltr558_suspend(struct i2c_client *client, pm_message_t mesg)
{
	PRINT_INFO("--\n");
	if (l_flag == 1) {
		ltr558_i2c_write_1_byte(LTR558_ALS_THRES_UP_0, 0xff);
		ltr558_i2c_write_1_byte(LTR558_ALS_THRES_UP_1, 0xff);
		ltr558_i2c_write_1_byte(LTR558_ALS_THRES_LOW_0, 0x00);
		ltr558_i2c_write_1_byte(LTR558_ALS_THRES_LOW_1, 0x00);
		ltr558_i2c_write_1_byte(LTR558_ALS_CONTR, MODE_ALS_STANDBY);
	}
	PRINT_INFO("l_flag:%d -- 20180201\n", l_flag);
	return 0;
}

static int ltr558_resume(struct i2c_client *client)
{
	PRINT_INFO("--\n");
	if (l_flag == 1)
		ltr558_als_enable(l_gainrange);
	PRINT_INFO("l_flag:%d -- 20180201\n", l_flag);
	return 0;
}

static int ltr558_pm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	pm_message_t mesg = {0};

	ltr558_suspend(client, mesg);
	return 0;
}

static int ltr558_pm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	ltr558_resume(client);
	return 0;
}

static const struct dev_pm_ops ltr558_pm_ops = {
	.suspend = ltr558_pm_suspend,
	.resume = ltr558_pm_resume,
};
static const struct i2c_device_id ltr558_id[] = {
	{LTR558_I2C_NAME, 0},
	{}
};

static const struct of_device_id ltr558_of_match[] = {
	{ .compatible = "LITEON,ltr_558als", },
	{}
};

MODULE_DEVICE_TABLE(of, ltr558_of_match);

static struct i2c_driver ltr558_driver = {
	.driver = {
		.owner  = THIS_MODULE,
		.name = LTR558_I2C_NAME,
		.of_match_table = ltr558_of_match,
		.pm = &ltr558_pm_ops
	},
	.probe = ltr558_probe,
	.remove = ltr558_remove,
	.id_table = ltr558_id,
};

static int __init ltr558_init(void)
{
	int ret = -1;

	ret = i2c_add_driver(&ltr558_driver);
	if (ret) {
		PRINT_ERR("i2c_add_driver failed!\n");
		return ret;
	}
	return ret;
}

static void __exit ltr558_exit(void)
{
	i2c_del_driver(&ltr558_driver);
}

late_initcall(ltr558_init);
module_exit(ltr558_exit);

MODULE_AUTHOR("Yaochuan Li <yaochuan.li@spreadtrum.com>");
MODULE_DESCRIPTION("Proximity&Light Sensor LTR558ALS DRIVER");
MODULE_LICENSE("GPL");
