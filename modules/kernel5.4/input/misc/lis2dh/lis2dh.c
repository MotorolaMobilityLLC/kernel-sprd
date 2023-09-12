/******************** (C) COPYRIGHT 2020 Unisoc Communications Inc. ************
*
* File Name	: lis2dh.c
* Authors	: Drive and Tools Technical Resources Department-Sensor_SH Team
*		    : Tianmin.Yang (tianmin.yang@unisoc.com)
*
* Version	: V.0.0.1
* Date		: 2020/Sep/04
* Description   : LIS2DH accelerometer driver code and sensor API
*
********************************************************************************
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
* OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
* PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
* AS A RESULT, Unisoc SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
*******************************************************************************/
/*******************************************************************************
*Version History.
*
* Revision 0.0.1: 2020/Sep/04
* first revision
*
*******************************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/i2c-dev.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/hrtimer.h>
#include <linux/string.h>
#include <linux/of_device.h>
#include <linux/sysfs.h>
#include "lis2dh.h"

#define SENSOR_NAME                      "lis2dh"
#define ACC_OUT_ADD                      0x28
#define MS_TO_NS(x)                      (x*1000000L)
#define ACC_L_OFFSET                     4
#define ACC_SUPPORT_SMBUS                0
#define I2C_AUTO_INCREMENT               0x80
#define MAX_RETRY_I2C_XFER               (100)
#define G_MAX                            16000
/**/
static struct input_dev *dev;
static struct i2c_client *logal_client;
static u8 read_reg_value;

#if ACC_SUPPORT_SMBUS
static int lis2dh_read_byte(struct i2c_client *client,
		u8 reg_addr, u8 *data)
{
	int dummy;
	int err = -1;

	dummy = i2c_smbus_read_byte_data(client, reg_addr);
	if(dummy < 0)
		return err;
	*data = dummy & 0x000000ff;
	return 0;
}

static int lis2dh_write_byte(struct i2c_client *client,
		u8 reg_addr, u8 *data)
{
	int dummy;
	int err = -1;

	dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
	if(dummy < 0)
		return err;

	return 0;
}

static int lis2dh_block_read(struct i2c_client *client,
				u8 reg_addr, int len, u8 *data)
{
	int dummy;
	int err = -1;

	dummy = i2c_smbus_read_i2c_block_data(client, reg_addr, len, data);
	if (dummy < 0)
		return err;

	return 0;
}

static int lis2dh_block_write(struct i2c_client *client,
				u8 reg_addr, int len, u8 *data)
{
	int dummy;
	int err = -1;

	dummy = i2c_smbus_write_i2c_block_data(client, reg_addr, len, data);
	if (dummy < 0)
		return err;

	return 0;
}
#else
static int lis2dh_block_read(struct i2c_client *client, u8 reg_addr,
					int len, u8 *data)
{
	int err = 0;
	struct i2c_msg msg[2];
	int retry;

	reg_addr |= ((len > 1) ? I2C_AUTO_INCREMENT : 0);
	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = data;

	for (retry = 0; retry < MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			mdelay(1);
	}

	if (MAX_RETRY_I2C_XFER <= retry)
		return -EIO;

	return err;
}

static int lis2dh_read_byte(struct i2c_client *client,
		u8 reg_addr, u8 *data)
{
	return lis2dh_block_read(client, reg_addr, 1, data);
}

static int lis2dh_block_write(struct i2c_client *client,
			u8 reg_addr, int len, u8 *data)
{
	int err = 0;
	u8 *send;
	struct i2c_msg msg;

	send = kmalloc_array((len+1), sizeof(u8), GFP_KERNEL);
	if (!send)
		return -ENOMEM;

	reg_addr |= ((len > 1) ? I2C_AUTO_INCREMENT : 0);
	send[0] = reg_addr;
	memcpy(&send[1], data, len * sizeof(u8));
	len++;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.len = len;
	msg.buf = send;

	err = i2c_transfer(client->adapter, &msg, 1);

	kfree(send);
	return err;
}

static int lis2dh_write_byte(struct i2c_client *client,
		u8 reg_addr, u8 *data)
{
	return lis2dh_block_write(client, reg_addr, 1, data);
}
#endif

static int set_reg(struct i2c_client *client, u8 reg_addr,
		u8 reg_value, u8 mask)
{
	int err = -1;
	u8 buf = 0;

	err = lis2dh_read_byte(client, reg_addr, &buf);
	if(err < 0)
		return err;
	buf = (((~mask) & buf) | reg_value);
	err = lis2dh_write_byte(client, reg_addr, &buf);
	if(err < 0)
		return err;
	return 0;
}

static void check_poll_interval_valid(unsigned long *poll_interval)
{
	u32 i = ARRAY_SIZE(odr_parame_table) - 1;

	if (*poll_interval <= odr_parame_table[i].interval_ms)
		*poll_interval = odr_parame_table[i].interval_ms + 1;
	else if (*poll_interval > odr_parame_table[0].interval_ms)
		*poll_interval = odr_parame_table[0].interval_ms;
}

static int interval_to_odr(unsigned long poll_interval,
					struct lis2dh_data *data)
{
	u32 i;

	check_poll_interval_valid(&poll_interval);
	for (i = 0; i < ARRAY_SIZE(odr_parame_table); i++) {
		if (poll_interval >= odr_parame_table[i].interval_ms) {
			data->odr = odr_parame_table[i].odr;
			break;
		}
	}
	data->poll_interval = poll_interval;
	return 0;
}

static int lis2dh_set_operating_mode(struct i2c_client *client,
					u8 mode)
{
	int err = 0;
	u32 i = 0;
	struct lis2dh_data *data = i2c_get_clientdata(client);

	for(i = 0; i < ARRAY_SIZE(mode_array); i++) {
		if(mode_array[i].label == mode) {
			err = set_reg(client, mode_array[i].reg_addr,
							mode_array[i].reg_value,
							mode_array[i].mask);
			if(err < 0)
				return err;
		}
	}
	data->operating_mode = mode;
	return 0;
}

static int lis2dh_set_range(struct i2c_client *client,
				u8 range)
{
	int err = -1;
	u32 i = 0;
	struct lis2dh_data *data = i2c_get_clientdata(client);

	for(i = 0; i < ARRAY_SIZE(range_arry); i++) {
		if(range_arry[i].label == range) {
			err = set_reg(client, range_arry[i].reg_addr,
							range_arry[i].reg_value,
							range_arry[i].mask);
			if(err < 0)
				return err;
			break;
		}
	}
	if(i == ARRAY_SIZE(range_arry))
		return -EINVAL;

	data->fs_range = range;
	dev_info(&client->dev, "%s finished\n", __FUNCTION__);
	return 0;
}

static int lis2dh_set_odr(struct i2c_client *client,
				u32 odr)
{
	int err = -1;
	u32 i = 0;
	struct lis2dh_data *data = i2c_get_clientdata(client);

	for(i = 0; i < ARRAY_SIZE(odr_array); i++) {
		if(odr_array[i].label == odr) {
			err = set_reg(client, odr_array[i].reg_addr,
					odr_array[i].reg_value, odr_array[i].mask);
			if(err < 0)
				return err;
			break;
		}
	}
	if(i == ARRAY_SIZE(odr_array))
		return -EINVAL;

	data->odr = odr;
	dev_info(&client->dev, "%s finished\n", __FUNCTION__);
	return 0;
}

static ssize_t lis2dh_read_range(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lis2dh_data *data = i2c_get_clientdata(logal_client);

	return sprintf(buf, "%d\n", data->fs_range);
}

static ssize_t lis2dh_write_range(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int err;
	unsigned long range;
	struct lis2dh_data *data = i2c_get_clientdata(logal_client);

	if(kstrtoul(buf, 10, &range))
		return -EINVAL;

	mutex_lock(&data->lock);
	err = lis2dh_set_range(logal_client, range);
	if(err < 0) {
		mutex_unlock(&data->lock);
		return -EINVAL;
	}
	mutex_unlock(&data->lock);
	return count;
}

static ssize_t lis2dh_read_sensor_info(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lis2dh_data *data = i2c_get_clientdata(logal_client);

	return sprintf(buf, "RANGE:%dG ODR:%dHZ MODE:%d DELAY:%dms\n",
			data->fs_range, data->odr, data->operating_mode,
							data->poll_interval);
}

static int lis2dh_reg_init(struct i2c_client *client)
{
	int err = 0;
	int i;

	for(i = 0; i < ARRAY_SIZE(init_array); i++) {
		err += set_reg(client, init_array[i].reg_addr,
			init_array[i].reg_value, init_array[i].mask);
	}
	if(err < 0) {
		dev_err(&client->dev, "reg_init error\n");
		return err;
	}
	dev_info(&client->dev, "%s finished\n", __FUNCTION__);
	return 0;
}

static int lis2dh_power_off(struct i2c_client *client)
{
	int err = -1;

	err = lis2dh_set_operating_mode(client, POWER_DOWN_MODE);
	if(err < 0)
		return err;
	dev_info(&client->dev, "%s finished\n", __FUNCTION__);
	return 0;
}

static int lis2dh_init(struct lis2dh_data *data)
{
	int err = -1;

	err = lis2dh_reg_init(data->lis2dh_client);
	if(err < 0) {
		lis2dh_power_off(data->lis2dh_client);
		return -EINVAL;
	}

	return 0;
}

static ssize_t lis2dh_read_enable_device(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lis2dh_data *data = i2c_get_clientdata(logal_client);

	return sprintf(buf, "%d\n", atomic_read(&data->enable));
}

static ssize_t lis2dh_write_enable_device(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct lis2dh_data *data = i2c_get_clientdata(logal_client);
	unsigned long val;

	if(kstrtoul(buf, 10, &val))
		return -EINVAL;

	if(val) {
		if(!atomic_read(&data->enable)) {
			lis2dh_set_operating_mode(logal_client, HIGH_RESOLUTION_MODE);
			lis2dh_set_odr(logal_client, data->odr);
			lis2dh_set_range(logal_client, data->fs_range);
			hrtimer_start(&data->hr_timer, data->ktime,
						HRTIMER_MODE_REL);
			atomic_set(&data->enable, 1);
		}
	} else {
		if(atomic_read(&data->enable)) {
			lis2dh_set_operating_mode(logal_client, POWER_DOWN_MODE);
			hrtimer_cancel(&data->hr_timer);
			atomic_set(&data->enable, 0);
		}
	}
	return size;
}

static ssize_t lis2dh_read_poll_interval(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lis2dh_data *data = i2c_get_clientdata(logal_client);

	return sprintf(buf, "%d\n", data->poll_interval);
}

static ssize_t lis2dh_write_poll_interval(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long interval_ms;
	struct lis2dh_data *data = i2c_get_clientdata(logal_client);

	if(kstrtoul(buf, 10, &interval_ms))
		return -EINVAL;

	check_poll_interval_valid(&interval_ms);

	mutex_lock(&data->lock);
	data->poll_interval = interval_ms;
	data->ktime = ktime_set(0, MS_TO_NS(data->poll_interval));
	interval_to_odr(interval_ms, data);
	lis2dh_set_odr(logal_client, data->odr);
	mutex_unlock(&data->lock);

	return size;
}

static ssize_t lis2dh_read_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lis2dh_data *data = i2c_get_clientdata(logal_client);

	return sprintf(buf, "%d\n", data->operating_mode);
}

static ssize_t lis2dh_write_mode(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct lis2dh_data *data = i2c_get_clientdata(logal_client);
	unsigned long val;

	if(kstrtoul(buf, 10, &val))
		return -EINVAL;

	mutex_lock(&data->lock);
	lis2dh_set_operating_mode(logal_client, val);
	mutex_unlock(&data->lock);

	return size;
}

static ssize_t lis2dh_read_odr(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct lis2dh_data *data = i2c_get_clientdata(logal_client);

	return sprintf(buf, "%d\n", data->odr);
}

static ssize_t lis2dh_write_odr(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct lis2dh_data *data = i2c_get_clientdata(logal_client);
	unsigned long val;

	if(kstrtoul(buf, 10, &val))
		return -EINVAL;

	mutex_lock(&data->lock);
	lis2dh_set_odr(logal_client, val);
	mutex_unlock(&data->lock);

	return size;
}

static ssize_t lis2dh_read_chip_id_reg(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err = 0;
	u8 reg_buf = 0;

	err = lis2dh_read_byte(logal_client, chip_id.reg_addr, &reg_buf);
	if(err < 0)
		return err;

	return sprintf(buf, "CHIP_ID_REG:0x%02x\n", reg_buf);
}

static ssize_t lis2dh_read_odr_reg(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int err = 0;
	u8 reg_buf = 0;

	err = lis2dh_read_byte(logal_client, odr_array[0].reg_addr, &reg_buf);
	if(err < 0)
		return err;

	return sprintf(buf, "ODR_REG:0x%02x\n", reg_buf);
}

static ssize_t lis2dh_read_range_reg(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int err = 0;
	u8 reg_buf = 0;

	err = lis2dh_read_byte(logal_client, range_arry[0].reg_addr, &reg_buf);
	if(err < 0)
		return err;

	return sprintf(buf, "RANGE_REG:0x%02x\n", reg_buf);
}

static ssize_t lis2dh_read_chip_info(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s", SENSOR_NAME);
}

static u8 show_reg_value(u8 reg_addr)
{
	int err = 0;
	u8 reg_buf = 0;
	err = lis2dh_read_byte(logal_client, reg_addr, &reg_buf);
	if(err < 0)
		return err;

	return reg_buf;
}

static ssize_t lis2dh_read_read_reg(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%02x\n", read_reg_value);
}

static ssize_t lis2dh_write_read_reg(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct lis2dh_data *data = i2c_get_clientdata(logal_client);
	unsigned long val;

	if(kstrtoul(buf, 16, &val))
		return -EINVAL;

	mutex_lock(&data->lock);
	read_reg_value = show_reg_value(val);
	mutex_unlock(&data->lock);

	return size;
}

static DEVICE_ATTR(acc_chip_info, 0444,
		lis2dh_read_chip_info, NULL);

static DEVICE_ATTR(range, 0644,
		lis2dh_read_range, lis2dh_write_range);

static DEVICE_ATTR(sensor_info, 0444,
		lis2dh_read_sensor_info, NULL);

static DEVICE_ATTR(acc_enable, 0644,
		lis2dh_read_enable_device, lis2dh_write_enable_device);

static DEVICE_ATTR(acc_delay, 0644,
		lis2dh_read_poll_interval, lis2dh_write_poll_interval);

static DEVICE_ATTR(odr, 0644,
		lis2dh_read_odr, lis2dh_write_odr);

static DEVICE_ATTR(mode, 0644,
		lis2dh_read_mode, lis2dh_write_mode);

static DEVICE_ATTR(chip_id_reg, 0444,
		lis2dh_read_chip_id_reg, NULL);

static DEVICE_ATTR(odr_reg, 0444,
		lis2dh_read_odr_reg, NULL);

static DEVICE_ATTR(range_reg, 0444,
		lis2dh_read_range_reg, NULL);

static DEVICE_ATTR(read_reg, 0644,
		lis2dh_read_read_reg, lis2dh_write_read_reg);

static struct attribute *function_attrs[] = {
	&dev_attr_acc_chip_info.attr,
	&dev_attr_range.attr,
	&dev_attr_sensor_info.attr,
	&dev_attr_acc_enable.attr,
	&dev_attr_acc_delay.attr,
	&dev_attr_odr.attr,
	&dev_attr_mode.attr,

	&dev_attr_chip_id_reg.attr,
	&dev_attr_odr_reg.attr,
	&dev_attr_range_reg.attr,
	&dev_attr_read_reg.attr,
	NULL
};

static const struct attribute_group function_attr_grp = {
	.attrs = function_attrs,
};

static int lis2dh_check_chip_id(struct i2c_client *client)
{
	int err = -1;
	u8 buf = 0;
	struct lis2dh_data *data = i2c_get_clientdata(client);

	err = lis2dh_read_byte(client, chip_id.reg_addr, &buf);
	if(err < 0)
		return err;
	else {
		if(buf == chip_id.reg_value) {
			dev_info(&client->dev, "%s finished\n", __FUNCTION__);
			data->chip_id = buf;
			return 0;
		} else
			return -EINVAL;
	}
}

static int lis2dh_input_init(struct lis2dh_data *data)
{
	int err = -1;

	dev = devm_input_allocate_device(data->dev);
	if(!dev)
		return -ENOMEM;

	dev->name = "accelerometer";
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_abs_params(dev, ABS_X, -G_MAX, G_MAX, 0, -1);
	input_set_abs_params(dev, ABS_Y, -G_MAX, G_MAX, 0, -1);
	input_set_abs_params(dev, ABS_Z, -G_MAX, G_MAX, 0, -1);

	__set_bit(EV_MSC, dev->evbit);
	__set_bit(MSC_SCAN, dev->mscbit);
	__set_bit(MSC_MAX, dev->mscbit);

	input_set_drvdata(dev, data);
	err = input_register_device(dev);
	if(err < 0)
		return err;

	data->input_dev = dev;
	dev_info(data->dev, "%s finished\n", __FUNCTION__);
	return 0;
}

static inline int64_t lis2dh_acc_get_time_ns(void)
{
	struct timespec64 ts;

	ktime_get_boottime_ts64(&ts);

	return timespec64_to_ns(&ts);
}

static enum hrtimer_restart
	lis2dh_reportdata_timer_fun(struct hrtimer *timer)
{
	struct lis2dh_data *data;

	data = container_of((struct hrtimer *)timer,
					struct lis2dh_data, hr_timer);
	data->timestamp = lis2dh_acc_get_time_ns();
	queue_work(data->acc_workqueue, &data->report_data_work);
	return HRTIMER_NORESTART;
}

static int lis2dh_get_acc_data(struct lis2dh_data *data)
{
	int err = -1;
	u8 block_buf[6] = {0};

	err = lis2dh_block_read(data->lis2dh_client, ACC_OUT_ADD, 6, block_buf);
	if (err < 0)
		return err;

	data->value.x = (((s16) ((block_buf[1] << 8) | block_buf[0])) >> ACC_L_OFFSET);
	data->value.y = (((s16) ((block_buf[3] << 8) | block_buf[2])) >> ACC_L_OFFSET);
	data->value.z = (((s16) ((block_buf[5] << 8) | block_buf[4])) >> ACC_L_OFFSET);
	return 0;
}

static void lis2dh_report_values(struct lis2dh_data *data)
{
	input_report_abs(data->input_dev, ABS_X, data->value.x);
	input_report_abs(data->input_dev, ABS_Y, data->value.y);
	input_report_abs(data->input_dev, ABS_Z, data->value.z);

	input_event(data->input_dev, EV_MSC, MSC_SCAN,
				data->timestamp >> 32);
	input_event(data->input_dev, EV_MSC, MSC_MAX,
				data->timestamp & 0xffffffff);
	input_sync(data->input_dev);
}

static void lis2dh_timer_work_func(struct work_struct *work)
{
	int err = -1;
	ktime_t tmpkt = 0;
	struct lis2dh_data *data;

	data = container_of((struct work_struct *)work,
				struct lis2dh_data, report_data_work);

	tmpkt = ktime_sub(data->ktime,
		ktime_set(0,(lis2dh_acc_get_time_ns() - data->timestamp)));
	if(tmpkt <= 0) {
		hrtimer_start(&data->hr_timer,
			ktime_set(0, 0), HRTIMER_MODE_REL);
	} else {
		dev_info(&data->lis2dh_client->dev, "%s:timestamp_debug tmpkt =%lld\n",
				__FUNCTION__, tmpkt);
		hrtimer_start(&data->hr_timer, tmpkt, HRTIMER_MODE_REL);
	}

	mutex_lock(&data->lock);
	err = lis2dh_get_acc_data(data);
	if(err < 0)
		dev_err(&data->lis2dh_client->dev, "get_acceleration_data failed\n");
	else
		lis2dh_report_values(data);

	mutex_unlock(&data->lock);
}

static void data_init(struct lis2dh_data *data, struct i2c_client *client)
{
	data->name = SENSOR_NAME;
	data->fs_range = 4;
	data->odr = 50;
	data->operating_mode = HIGH_RESOLUTION_MODE;
	data->poll_interval = 100;
	data->value.x = 0;
	data->value.y = 0;
	data->value.z = 0;
	atomic_set(&data->enable, 0);
	data->ktime = ktime_set(0, MS_TO_NS(data->poll_interval));
	data->dev = &client->dev;
	i2c_set_clientdata(client, data);
	data->lis2dh_client = client;
}

static int lis2dh_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err = -1;
	struct lis2dh_data *data;
	logal_client = client;

	dev_info(&client->dev, "%s start\n", __FUNCTION__);
	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -EIO;
		goto exit;
	}

	data = devm_kzalloc(&client->dev, sizeof(struct lis2dh_data), GFP_KERNEL);
	if(!data) {
		err = -ENOMEM;
		goto exit_free;
	}

	data_init(data, client);
	mutex_init(&data->lock);
	mutex_lock(&data->lock);
	err = lis2dh_check_chip_id(client);
	if(err < 0)
		goto exit_mutexunlock;

	err = lis2dh_init(data);
	if(err < 0)
		goto exit_power_off;

	err = lis2dh_input_init(data);
	if(err < 0)
		goto exit_power_off;

	err = sysfs_create_group(&data->input_dev->dev.kobj, &function_attr_grp);
	if(err < 0)
		goto exit_input_cleanup;
	kobject_uevent(&data->input_dev->dev.kobj, KOBJ_ADD);

	data->acc_workqueue = create_workqueue("lis2dh_acc_workqueue");
	if(!data->acc_workqueue) {
		dev_err(&client->dev, "create workqueue error\n");
		err = -EINVAL;
		goto exit_remove_sysfs_int;
	}
	mutex_unlock(&data->lock);

	INIT_WORK(&data->report_data_work, lis2dh_timer_work_func);
	hrtimer_init(&data->hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->hr_timer.function = &lis2dh_reportdata_timer_fun;

	atomic_set(&data->enable, 0);
	dev_info(&client->dev, "%s finished\n", __FUNCTION__);
	return 0;

exit_remove_sysfs_int:
	sysfs_remove_group(&data->input_dev->dev.kobj, &function_attr_grp);
exit_input_cleanup:
exit_power_off:
	lis2dh_power_off(client);
exit_mutexunlock:
	mutex_unlock(&data->lock);
exit_free:
exit:
	return err;
}

static int lis2dh_remove(struct i2c_client *client)
{
	struct lis2dh_data *data = i2c_get_clientdata(client);

	dev_info(&client->dev, "%s start!\n", __FUNCTION__);
	cancel_work_sync(&data->report_data_work);
	if(data->acc_workqueue) {
		destroy_workqueue(data->acc_workqueue);
		data->acc_workqueue = NULL;
	}
	hrtimer_cancel(&data->hr_timer);
	lis2dh_power_off(client);
	sysfs_remove_group(&data->input_dev->dev.kobj, &function_attr_grp);
	dev_info(&client->dev, "%s finished!\n", __FUNCTION__);
	return 0;
}

static int lis2dh_acc_resume(struct i2c_client *client)
{
	int err = -1;
	struct lis2dh_data *data = i2c_get_clientdata(client);

	dev_info(&client->dev, "%s start! data->on_before_suspend = %d\n",
		__FUNCTION__, data->on_before_suspend);

	if(data->on_before_suspend) {
		err = lis2dh_init(data);
		if(err < 0) {
			atomic_set(&data->enable, 0);
			return -EINVAL;
		}
		lis2dh_set_operating_mode(client, data->operating_mode);
		lis2dh_set_range(client, data->fs_range);
		lis2dh_set_odr(client, data->odr);
		hrtimer_start(&data->hr_timer,
				data->ktime, HRTIMER_MODE_REL);
		atomic_set(&data->enable, 1);
	}
	dev_info(&client->dev, "%s finished! \n", __FUNCTION__);
	return 0;
}

static int lis2dh_acc_suspend(struct i2c_client *client)
{
	struct lis2dh_data *data = i2c_get_clientdata(client);

	data->on_before_suspend = atomic_read(&data->enable);
	dev_info(&client->dev, "%s start! data->on_before_suspend = %d\n",
		__FUNCTION__, data->on_before_suspend);

	if(data->on_before_suspend) {
		mutex_lock(&data->lock);
		hrtimer_cancel(&data->hr_timer);
		lis2dh_power_off(client);
		atomic_set(&data->enable, 0);
		mutex_unlock(&data->lock);
		dev_info(&client->dev, "%s finished! \n", __FUNCTION__);
	}
	return 0;
}

static int lis2dh_pm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	return lis2dh_acc_resume(client);
}

static int lis2dh_pm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	return lis2dh_acc_suspend(client);
}

static const struct dev_pm_ops lis2dh_pm_ops = {
	.suspend = lis2dh_pm_suspend,
	.resume = lis2dh_pm_resume,
};

static const struct i2c_device_id lis2dh_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lis2dh_id);

static const struct of_device_id lis2dh_of_match[] = {
	{ .compatible = "st,lis2dh", },
	{ .compatible = "st,lis2dh12"},
	{ },
};
MODULE_DEVICE_TABLE(of, lis2dh_of_match);

static struct i2c_driver lis2dh_driver = {
	.driver = {
		.name = SENSOR_NAME,
		.of_match_table = lis2dh_of_match,
		.pm = &lis2dh_pm_ops
	},
	.id_table = lis2dh_id,
	.probe = lis2dh_probe,
	.remove = lis2dh_remove,
};
module_i2c_driver(lis2dh_driver);

MODULE_DESCRIPTION("ACCELEROMETER SENSOR DRIVER");
MODULE_AUTHOR("tianmin.yang@unisoc.com");
MODULE_LICENSE("GPL v2");
