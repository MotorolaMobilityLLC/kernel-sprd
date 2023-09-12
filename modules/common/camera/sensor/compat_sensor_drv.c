/*
 * Copyright (C) 2021-2022 UNISOC Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "sprd_sensor.h"
#include "compat_sensor_drv.h"

struct compat_sensor_reg_tab_tag {
	compat_caddr_t sensor_reg_tab_ptr;
	uint32_t reg_count;
	uint32_t reg_bits;
	uint32_t burst_mode;
};

struct compat_sensor_i2c_tag {
	compat_caddr_t i2c_data;
	uint16_t i2c_count;
	uint16_t slave_addr;
	uint16_t read_len;
};

struct compat_sensor_otp_data_info_tag {
	uint32_t size;
	compat_caddr_t data_ptr;
};

struct compat_sensor_otp_param_tag {
	uint32_t type;
	uint32_t start_addr;
	uint32_t len;
	compat_caddr_t buff;
	struct compat_sensor_otp_data_info_tag golden;
	struct compat_sensor_otp_data_info_tag awb;
	struct compat_sensor_otp_data_info_tag lsc;
};


#define COMPAT_SENSOR_IO_I2C_WRITE_REGS \
	_IOW(SENSOR_IOC_MAGIC,  14, struct compat_sensor_reg_tab_tag)
#define COMPAT_SENSOR_IO_GRC_I2C_WRITE \
	_IOW(SENSOR_IOC_MAGIC,  19, struct compat_sensor_i2c_tag)
#define COMPAT_SENSOR_IO_GRC_I2C_READ \
	_IOWR(SENSOR_IOC_MAGIC, 20, struct compat_sensor_i2c_tag)
#define COMPAT_SENSOR_IO_MUTI_I2C_WRITE \
	_IOW(SENSOR_IOC_MAGIC,  23, struct sensor_muti_aec_i2c_tag)
#define COMPAT_SENSOR_IO_READ_OTPDATA \
	_IOWR(SENSOR_IOC_MAGIC, 254, struct compat_sensor_otp_param_tag)

long compat_sensor_k_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	long err = 0;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_SENSOR_IO_I2C_WRITE_REGS:
	{
		struct compat_sensor_reg_tab_tag __user *data32;

		data32 = compat_ptr(arg);
		err = filp->f_op->unlocked_ioctl(filp,
				SENSOR_IO_I2C_WRITE_REGS,
				(unsigned long)data32);
		break;
	}

	case COMPAT_SENSOR_IO_MUTI_I2C_WRITE:
	{
		struct sensor_muti_aec_i2c_tag __user *data32;

		data32 = compat_ptr(arg);
		err = filp->f_op->unlocked_ioctl(filp,
				SENSOR_IO_MUTI_I2C_WRITE,
				(unsigned long)data32);
		break;
	}

	case COMPAT_SENSOR_IO_GRC_I2C_WRITE:
	{
		struct compat_sensor_i2c_tag __user *data32;

		data32 = compat_ptr(arg);
		err = filp->f_op->unlocked_ioctl(filp,
				SENSOR_IO_GRC_I2C_WRITE,
				(unsigned long)data32);
		break;
	}

	case COMPAT_SENSOR_IO_GRC_I2C_READ:
	{
		struct compat_sensor_i2c_tag __user *data32;

		data32 = compat_ptr(arg);
		err = filp->f_op->unlocked_ioctl(filp,
				SENSOR_IO_GRC_I2C_READ,
				(unsigned long)data32);
		break;
	}

	case COMPAT_SENSOR_IO_READ_OTPDATA:
	{
		struct compat_sensor_otp_param_tag __user *data32;

		data32 = compat_ptr(arg);
		err = filp->f_op->unlocked_ioctl(filp,
				SENSOR_IO_READ_OTPDATA,
				(unsigned long)data32);
		break;
	}

	default:
		err = filp->f_op->unlocked_ioctl(filp, cmd,
				(unsigned long)compat_ptr(arg));
		break;
	}

	return err;
}
