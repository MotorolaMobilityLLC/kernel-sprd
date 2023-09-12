/*
 * Copyright (C) 2012 akm.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <linux/regulator/consumer.h>
#include <linux/input.h>
#include <linux/regmap.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/gpio.h>

#include <uapi/linux/sched/types.h>
#include <linux/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/kthread.h>
#include <linux/sched/rt.h>
#include "akm09911.h"

#define POLL_INTERVAL_DEFAULT 20

#define AKM099XX_MIN_DELAY 5
#define AKM099XX_PRODUCT_ID 0x48
#define AKM09911_DEVICE_ID 0x05
#define AKM09918_DEVICE_ID 0x0C
#define AKM_DEVICE_ID_NODE "device_id"

#define AKM099XX_SINGLE_POWER 0

#define AKM099XX_DELAY_TM_MS    10
#define AKM099XX_DELAY_SET  75
#define AKM099XX_DELAY_RESET 75
#define AKM099XX_RETRY_COUNT    10
#define AKM099XX_DEFAULT_INTERVAL_MS    50
#define AKM099XX_TIMEOUT_SET_MS 15000

#define SENS_0600_Q16  ((int32_t)(39322)) /* 0.6 * 2^32  in Q16 format */
#define SENS_0150_Q16  ((int32_t)(9830))  /* 0.15 * 2^32 in Q16 format */

/* Global variable for ASA value. */
static uint8_t g_asa[3];
static int32_t g_raw_to_micro_q16[3];
static uint8_t device_id;

struct AKM099XX_data {
	struct i2c_client *i2c;
	struct input_dev *idev;
	struct hrtimer work_timer;
	struct completion report_complete;
	struct task_struct *thread;
	bool hrtimer_running;
	int gpio_reset;
	int poll_interval;
	struct mutex lock;
	atomic_t enabled;
	unsigned int on_before_suspend;
};

struct i2c_client *this_client;

static int AKM099XX_i2c_rxdata(struct i2c_client *i2c, unsigned char *rxData, int length)
{
	struct i2c_msg msgs[] = {
		{
			.addr = i2c->addr,
			.flags = 0,
			.len = 1,
			.buf = rxData,
		},
		{
			.addr = i2c->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = rxData,
		}, };
	unsigned char addr = rxData[0];

	if (i2c_transfer(i2c->adapter, msgs, 2) < 0) {
		dev_err(&i2c->dev, "%s: transfer failed.", __func__);
		return -EIO;
	}

	dev_vdbg(&i2c->dev, "RxData: len=%02x, addr=%02x, data=%02x",
			length, addr, rxData[0]);
	return 0;
}

static int AKM099XX_i2c_txdata(struct i2c_client *i2c, unsigned char *txData, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = i2c->addr,
			.flags = 0,
			.len = length,
			.buf = txData,
		}, };

	if (i2c_transfer(i2c->adapter, msg, 1) < 0) {
		dev_err(&i2c->dev, "%s: transfer failed.", __func__);
		return -EIO;
	}

	dev_vdbg(&i2c->dev, "TxData: len=%02x, addr=%02x data=%02x",
			length, txData[0], txData[1]);
	return 0;
}

static int AKM099XX_read_xyz(struct AKM099XX_data *akm, int *vec)
{
	unsigned char data[9];
	int tmp[3] = {0};
	int rc = 0;

	/* read xyz raw data */
	data[0] = AKM099XX_REG_DATA;
	rc = AKM099XX_i2c_rxdata(akm->i2c, data, 9);
	if (rc) {
		dev_err(&akm->i2c->dev, "read reg id failed.(%d)\n", rc);
		return rc;
	}

	tmp[0] = (int16_t)(((uint16_t)(data[2]) << 8) | (uint16_t)(data[1]));
	tmp[1] = (int16_t)(((uint16_t)(data[4]) << 8) | (uint16_t)(data[3]));
	tmp[2] = (int16_t)(((uint16_t)(data[6]) << 8) | (uint16_t)(data[5]));

	/* multiply ASA and convert to micro tesla in Q16 */
	tmp[0] *= g_raw_to_micro_q16[0];
	tmp[1] *= g_raw_to_micro_q16[1];
	tmp[2] *= g_raw_to_micro_q16[2];

	/* axis conversion parameter */
	vec[0] = tmp[0];
	vec[1] = tmp[1];
	vec[2] = tmp[2];

	return rc;
}

static void AKM099XX_report_values(struct AKM099XX_data *akm, int *xyz)
{
	input_report_abs(akm->idev, ABS_RX, xyz[0]);
	input_report_abs(akm->idev, ABS_RY, xyz[1]);
	input_report_abs(akm->idev, ABS_RZ, xyz[2]);
	input_sync(akm->idev);
}

static void AKM099XX_input_work_func(struct AKM099XX_data *akm)
{
	int xyz[3] = { 0 };
	int err;

	err = AKM099XX_read_xyz(akm, xyz);

	if (err < 0) {
		printk("AKM099XX_read_xyz failed\n");
	} else {
		AKM099XX_report_values(akm, xyz);
	}
}

static enum hrtimer_restart AKM099XX_work(struct hrtimer *timer)
{
	struct AKM099XX_data *mag;
	ktime_t poll_delay;
	mag = container_of((struct hrtimer *)timer, struct AKM099XX_data, work_timer);

	complete(&mag->report_complete);
	if (mag->poll_interval > 0) {
		poll_delay = ktime_set(0, mag->poll_interval * NSEC_PER_MSEC);
	} else {
		poll_delay = ktime_set(0, POLL_INTERVAL_DEFAULT * NSEC_PER_MSEC);
	}
	mag->hrtimer_running = true;
	hrtimer_forward_now(&mag->work_timer, poll_delay);

	return HRTIMER_RESTART;
}

static int report_event(void *data)
{
	struct AKM099XX_data *mag = data;

	while (1) {
		/* wait for report event */
		wait_for_completion(&mag->report_complete);
		mutex_lock(&mag->lock);
		if (atomic_read(&mag->enabled) <= 0) {
			mutex_unlock(&mag->lock);
			continue;
		}
		AKM099XX_input_work_func(mag);
		mutex_unlock(&mag->lock);
	}
	return 0;
}

static struct input_dev *AKM099XX_init_input(struct AKM099XX_data *akm)
{
	int status;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };
	struct input_dev *input = NULL;

	printk("%s\n", __FUNCTION__);
	hrtimer_init(&akm->work_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	akm->work_timer.function = AKM099XX_work;
	akm->hrtimer_running = false;
	init_completion(&akm->report_complete);
	akm->thread = kthread_run(report_event, akm, "msensor_report_event");
	if (IS_ERR(akm->thread)) {
		printk("unable to create report_event thread\n");
		return NULL;
	}
	sched_setscheduler_nocheck(akm->thread, SCHED_FIFO, &param);

	input = devm_input_allocate_device(&akm->i2c->dev);
	if (!input) {
		kthread_stop(akm->thread);
		return NULL;
	}
	input->name = "compass";
	input->phys = "AKM099XX/input0";
	input->id.bustype = BUS_I2C;

	__set_bit(EV_ABS, input->evbit);
	input_set_events_per_packet(input, 100);
	input_set_abs_params(input, ABS_RX, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(input, ABS_RY, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(input, ABS_RZ, INT_MIN, INT_MAX, 0, 0);
	/*input_set_abs_params(input, ABS_RUDDER, 0, 3, 0, 0);*/

	/* Report the dummy value */
	input_set_abs_params(input, ABS_MISC, INT_MIN, INT_MAX, 0, 0);

	input_set_capability(input, EV_REL, REL_X);
	input_set_capability(input, EV_REL, REL_Y);
	input_set_capability(input, EV_REL, REL_Z);

	status = input_register_device(input);
	if (status) {
		dev_err(&akm->i2c->dev, "error registering input device\n");
		kthread_stop(akm->thread);
		return NULL;
	}

	return input;
}


static int AKM099XX_check_device(struct AKM099XX_data *akm)
{
	int rc;
	unsigned char rd_buffer[2];
	unsigned char tx_buffer[2];

	/* Soft Reset */
	tx_buffer[0] = AK099XX_REG_CNTL3;
	tx_buffer[1] = AK099XX_SOFT_RESET;
	rc = AKM099XX_i2c_txdata(akm->i2c, tx_buffer, 2);
	if (rc) {
		return -ENODEV;
	}

	/* When succeeded, sleep 'Twait' */
	msleep(100);

	/* read sensor company ID */
	rd_buffer[0] = AK099XX_REG_WIA1;
	rc = AKM099XX_i2c_rxdata(akm->i2c, rd_buffer, 1);
	if (rc) {
		dev_err(&akm->i2c->dev, "read reg id failed.(%d)\n", rc);
		return rc;
	}
	dev_info(&akm->i2c->dev, "AKM099XX company_id is 0x%x\n", rd_buffer[0]);

	if (rd_buffer[0] != AKM099XX_PRODUCT_ID)
		return -ENODEV;

	/* read sensor device ID */
	rd_buffer[0] = AK099XX_REG_WIA2;
	rc = AKM099XX_i2c_rxdata(akm->i2c, rd_buffer, 1);
	if (rc) {
		dev_err(&akm->i2c->dev, "read reg id failed.(%d)\n", rc);
		return rc;
	}
	dev_info(&akm->i2c->dev, "AKM099XX device_id is 0x%x\n", rd_buffer[0]);
	device_id = rd_buffer[0];

	/* read fuse reg value */
	if (rd_buffer[0] == AKM09911_DEVICE_ID) {

		/* Read FUSE ROM value */
		tx_buffer[0] = AK099XX_REG_CNTL2;
		tx_buffer[1] = AK099XX_MODE_FUSE_ACCESS;
		rc = AKM099XX_i2c_txdata(akm->i2c, tx_buffer, 2);
		if (rc) {
			return rc;
		}

		g_asa[0] = AK099XX_FUSE_ASAX;
		rc = AKM099XX_i2c_rxdata(akm->i2c, g_asa, 3);
		if (rc) {
			return rc;
		}

		tx_buffer[0] = AK099XX_REG_CNTL2;
		tx_buffer[1] = AK099XX_MODE_POWER_DOWN;
		rc = AKM099XX_i2c_txdata(akm->i2c, tx_buffer, 2);
		if (rc) {
			return rc;
		}
	} else {
		/* Other device does not have ASA. */
		g_asa[0] = 128;
		g_asa[1] = 128;
		g_asa[2] = 128;
	}

	if (rd_buffer[0] == AKM09911_DEVICE_ID) {
		/* The equation is: H_adj = H_raw x (ASA / 128 + 1)
		 * Convert from LSB to micro tesla in Q16, multiply (SENS x 2^16)
		 * Simplify the equation: coeff = ((ASA + 128) x SENS x 2^16) >> 7
		 * In case of AK09911, SENS = 0.6.
		 * So coeff = ((ASA + 128) x 39322) >> 7
		 */
		dev_info(&akm->i2c->dev, "AKM099XX g_asa[0]=%d,g_asa[1]=%d,g_asa[2]=%d\n",
			g_asa[0], g_asa[1], g_asa[2]);
		g_raw_to_micro_q16[0] = ((int32_t)(g_asa[0] + 128) * SENS_0600_Q16) >> 7;
		g_raw_to_micro_q16[1] = ((int32_t)(g_asa[1] + 128) * SENS_0600_Q16) >> 7;
		g_raw_to_micro_q16[2] = ((int32_t)(g_asa[2] + 128) * SENS_0600_Q16) >> 7;
	} else if (rd_buffer[0] == AKM09918_DEVICE_ID) {
		/* AK09913 or newer devices does not have ASA register. It means that user
		 * does not need to write adjustment equation.
		 */
		g_raw_to_micro_q16[0] = SENS_0150_Q16;
		g_raw_to_micro_q16[1] = SENS_0150_Q16;
		g_raw_to_micro_q16[2] = SENS_0150_Q16;
	}

	return 0;
}

#ifdef CONFIG_OF
static int AKM099XX_parse_dt(struct i2c_client *client, struct AKM099XX_data *akm)
{
	struct device_node *np = client->dev.of_node;

	akm->gpio_reset = of_get_gpio(np, 0);

	printk("akm099xx gpio_reset = %d\n", akm->gpio_reset);
	return 0;
}
#endif

static int AKM099XX_set_poll_delay(struct AKM099XX_data *akm, unsigned int delay_msec)
{
  ktime_t poll_delay;

  if (atomic_read(&akm->enabled)) {
		if (akm->poll_interval > 0) {
			poll_delay = ktime_set(0, akm->poll_interval * NSEC_PER_MSEC);
		} else {
			poll_delay = ktime_set(0, POLL_INTERVAL_DEFAULT * NSEC_PER_MSEC);
		}
		hrtimer_start(&akm->work_timer, poll_delay, HRTIMER_MODE_REL);
	}

	return 0;
}

static int AKM099XX_set_enable(struct AKM099XX_data *akm, unsigned int enable)
{
	unsigned char buffer[2];
	ktime_t poll_delay;
	if (!akm) {
		printk("akm pointer is NULL\n");
		return -1;
	}

	if (enable) {
	buffer[0] = 0x31;
	buffer[1] = 0x08;
	if (AKM099XX_i2c_txdata(akm->i2c, buffer, 2)) {
		dev_warn(&akm->i2c->dev, "write reg 0x31 failed at %d\n", __LINE__);
	}
		akm->hrtimer_running = true;
		if (akm->poll_interval > 0) {
			poll_delay = ktime_set(0, akm->poll_interval * NSEC_PER_MSEC);
		} else {
			poll_delay = ktime_set(0, POLL_INTERVAL_DEFAULT * NSEC_PER_MSEC);
		}
		hrtimer_start(&akm->work_timer, poll_delay, HRTIMER_MODE_REL);
		atomic_set(&akm->enabled, 1);
	} else {
	buffer[0] = 0x31;
	buffer[1] = 0x00;
	if (AKM099XX_i2c_txdata(akm->i2c, buffer, 2)) {
		dev_warn(&akm->i2c->dev, "write reg 0x31 failed at %d\n", __LINE__);
	}
		if (akm->hrtimer_running) {
			akm->hrtimer_running = false;
			hrtimer_cancel(&akm->work_timer);
		}
		atomic_set(&akm->enabled, 0);
	}

	return 0;
}

static ssize_t AKM099XX_device_id(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", device_id);
}

static ssize_t AKM099XX_chip_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", "akm099xx chip");
}

static ssize_t AKM099XX_layout_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t AKM099XX_layout_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	  return 0;
}

static ssize_t AKM099XX_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	int vec[3];
	struct AKM099XX_data *akm = dev_get_drvdata(dev);

	ret = AKM099XX_read_xyz(akm, vec);
	if (ret) {
		dev_warn(&akm->i2c->dev, "read xyz failed\n");
		return ret;
	}

	return sprintf(buf, "%d %d %d\n", vec[0], vec[1], vec[2]);
}

static ssize_t AKM099XX_delay_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}

static DEVICE_ATTR(device_id, S_IRUSR|S_IRGRP, AKM099XX_device_id, NULL);
static DEVICE_ATTR(chipinfo, S_IRUSR|S_IRGRP, AKM099XX_chip_info, NULL);
static DEVICE_ATTR(layout, S_IRUSR|S_IRGRP|S_IWUSR, AKM099XX_layout_show, AKM099XX_layout_store);
static DEVICE_ATTR(value, S_IRUSR|S_IRGRP, AKM099XX_value_show, NULL);
static DEVICE_ATTR(delay, S_IRUSR|S_IRGRP, AKM099XX_delay_show, NULL);

static struct attribute *AKM099XX_attributes[] = {
	&dev_attr_device_id.attr,
	&dev_attr_chipinfo.attr,
	&dev_attr_layout.attr,
	&dev_attr_value.attr,
	&dev_attr_delay.attr,
	NULL,
};

static const struct attribute_group AKM099XX_attr_group = {
		.attrs = AKM099XX_attributes,
};

static int AKM099XX_open(struct inode *inode, struct file *file)
{
	int ret = -1;
	ret = nonseekable_open(inode, file);

	return ret;
}

static int AKM099XX_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long AKM099XX_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	int delay;				/* for GET_DELAY */
	int set_delay;
	struct AKM099XX_data *clientdata = i2c_get_clientdata(this_client);
	uint32_t enable;

		switch (cmd) {
		case AKM099XX_IOC_SET_DELAY:
			if (copy_from_user(&set_delay, argp, sizeof(set_delay))) {
				printk("akm add set delay failed\n");
				return -EFAULT;
			}

			if (set_delay < AKM099XX_MIN_DELAY) {
				set_delay = AKM099XX_MIN_DELAY;
			}

			printk("%s %s %d xiexie\n", __func__, "AKM099XX_IOC_SET_DELAY", set_delay);
			clientdata->poll_interval = set_delay;
			AKM099XX_set_poll_delay(clientdata, clientdata->poll_interval);
			break;

		case ECOMPASS_IOC_GET_DELAY:
			delay = clientdata->poll_interval;
			if (copy_to_user(argp, &delay, sizeof(delay))) {
				printk(KERN_ERR "copy_to_user failed.");
				return -EFAULT;
			}
			break;

		case MSENSOR_IOCTL_MSENSOR_ENABLE:
			if (copy_from_user(&enable, argp, sizeof(enable))) {
				return -EFAULT;
			}

			printk("AKM MSENSOR_ENABLE  %s enable = %d xiexie \n", __FUNCTION__, enable);
			AKM099XX_set_enable(clientdata, enable);
			break;
		case MSENSOR_IOCTL_SENSOR_ENABLE:
			if (copy_from_user(&enable, argp, sizeof(enable))) {
				return -EFAULT;
			}
			if (enable > 1) {
				return -EINVAL;
			}
			printk("%s enable = %d xiexie \n", __FUNCTION__, enable);
			break;

		default:
			printk(KERN_ERR "%s not supported = 0x%04x", __FUNCTION__, cmd);
			break;
	}

	return 0;
}

static struct file_operations AKM099XX_fops = {
	.owner = THIS_MODULE,
	.open = AKM099XX_open,
	.release = AKM099XX_release,
	.unlocked_ioctl = AKM099XX_unlocked_ioctl,
};

static struct miscdevice AKM099XX_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "AKM099XX",
    .fops = &AKM099XX_fops,
};

static int AKM099XX_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int res = 0;
	struct AKM099XX_data *akm;

	printk("akm probing AKM099XX 111 \n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("AKM099XX i2c functionality check failed.\n");
		res = -ENODEV;
		goto out;
	}

	akm = devm_kzalloc(&client->dev, sizeof(struct AKM099XX_data),
			GFP_KERNEL);
	if (!akm) {
		dev_err(&client->dev, "memory allocation failed.\n");
		res = -ENOMEM;
		goto out;
	}

	res = AKM099XX_parse_dt(client, akm);
	if (res) {
		dev_err(&client->dev, "Unable to parse platform data.(%d)", res);
		goto free_akm;
	}

	res = devm_gpio_request(&client->dev, akm->gpio_reset, "magnetic_reset");
	if (res) {
		printk("gpio pin request fail (%d)\n", res);
		res = -EINVAL;
		goto free_akm;
	}
	gpio_direction_output(akm->gpio_reset, 0);
	mdelay(1);
	gpio_direction_output(akm->gpio_reset, 1);
	mdelay(1);

	akm->poll_interval = AKM099XX_DEFAULT_INTERVAL_MS;
	atomic_set(&akm->enabled, 0);
	akm->on_before_suspend = 0;

	this_client = client;
	akm->i2c = client;
	dev_set_drvdata(&client->dev, akm);
	i2c_set_clientdata(akm->i2c, akm);
	this_client = client;

	mutex_init(&akm->lock);

	res = AKM099XX_check_device(akm);
	if (res) {
		dev_err(&client->dev, "Check device failed\n");
		goto free_akm;
	}

	akm->idev = AKM099XX_init_input(akm);
	if (!akm->idev) {
		dev_err(&client->dev, "init input device failed\n");
		res = -ENODEV;
		goto  free_akm;
	}

	res = misc_register(&AKM099XX_device);
	if (res) {
		printk(KERN_ERR "AKM099XX_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	/* create sysfs group */
	res = sysfs_create_group(&akm->idev->dev.kobj, &AKM099XX_attr_group);
	if (res) {
		res = -EROFS;
		dev_err(&client->dev, "Unable to creat sysfs group\n");
		goto exit_sysfs_create_group_failed;
	}

	kobject_uevent(&akm->idev->dev.kobj, KOBJ_ADD);

	printk("AKM099XX successfully probed\n");

	return 0;

exit_sysfs_create_group_failed:
	misc_deregister(&AKM099XX_device);
exit_misc_device_register_failed:
	kthread_stop(akm->thread);
free_akm:
out:
	return res;
}

static int AKM099XX_remove(struct i2c_client *client)
{
	struct AKM099XX_data *akm = dev_get_drvdata(&client->dev);

	hrtimer_cancel(&akm->work_timer);
	kthread_stop(akm->thread);
	sysfs_remove_group(&akm->idev->dev.kobj, &AKM099XX_attr_group);
	misc_deregister(&AKM099XX_device);

	return 0;
}

static int AKM099XX_suspend(struct device *dev)
{
	int res = 0;
	struct AKM099XX_data *akm = dev_get_drvdata(dev);
	dev_dbg(dev, "suspended\n");
	akm->on_before_suspend = atomic_read(&akm->enabled);
	if (akm->on_before_suspend) {
		return AKM099XX_set_enable(akm, false);
	}

	return res;
}

static int AKM099XX_resume(struct device *dev)
{
	int res = 0;
	struct AKM099XX_data *akm = dev_get_drvdata(dev);

	dev_dbg(dev, "resumed\n");

	if (akm->on_before_suspend) {
		return AKM099XX_set_enable(akm, true);
	}

	return res;
}

static const struct i2c_device_id AKM099XX_id[] = {
	{ AKM099XX_I2C_NAME, 0 },
	{ }
};

static struct of_device_id AKM099XX_match_table[] = {
	{ .compatible = "ak,akm099xx", },
	{ },
};

static const struct dev_pm_ops AKM099XX_pm_ops = {
	.suspend = AKM099XX_suspend,
	.resume = AKM099XX_resume,
};

static struct i2c_driver AKM099XX_driver = {
	.probe 		= AKM099XX_probe,
	.remove 	= AKM099XX_remove,
	.id_table	= AKM099XX_id,
	.driver 	= {
		.owner	= THIS_MODULE,
		.name	= AKM099XX_I2C_NAME,
		.of_match_table = AKM099XX_match_table,
		.pm = &AKM099XX_pm_ops,
	},
};

static int __init AKM099XX_init(void)
{
	return i2c_add_driver(&AKM099XX_driver);
}

static void __exit AKM099XX_exit(void)
{
	i2c_del_driver(&AKM099XX_driver);
}

MODULE_DESCRIPTION("akm AKM099XX Magnetic Sensor Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

late_initcall(AKM099XX_init);
module_exit(AKM099XX_exit);
