/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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
#include <video/sprd_sensor_k.h>
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

struct compat_sensor_muti_aec_i2c_tag {
	compat_caddr_t sensor_id;
	uint16_t id_size;
	compat_caddr_t i2c_slave_addr;
	uint16_t i2c_slave_len;
	compat_caddr_t addr_bits_type;
	uint16_t addr_bits_type_len;
	compat_caddr_t data_bits_type;
	uint16_t data_bits_type_len;
	compat_caddr_t master_i2c_tab;
	uint16_t msize;
	compat_caddr_t slave_i2c_tab;
	uint16_t ssize;
};

#define COMPAT_SENSOR_IO_I2C_WRITE_REGS \
	_IOW(SENSOR_IOC_MAGIC,  14, struct compat_sensor_reg_tab_tag)
#define COMPAT_SENSOR_IO_GRC_I2C_WRITE \
	_IOW(SENSOR_IOC_MAGIC,  19, struct compat_sensor_i2c_tag)
#define COMPAT_SENSOR_IO_GRC_I2C_READ \
	_IOWR(SENSOR_IOC_MAGIC, 20, struct compat_sensor_i2c_tag)
#define COMPAT_SENSOR_IO_MUTI_I2C_WRITE \
	_IOW(SENSOR_IOC_MAGIC,  23, struct compat_sensor_muti_aec_i2c_tag)
#define COMPAT_SENSOR_IO_READ_OTPDATA \
	_IOWR(SENSOR_IOC_MAGIC, 254, struct compat_sensor_otp_param_tag)

static long compat_get_sensor_reg_tab_tag(
			struct compat_sensor_reg_tab_tag __user *data32,
			struct sensor_reg_tab_tag __user *data)
{
	unsigned long c;
	uint32_t i;
	int err;

	err = get_user(c, &data32->sensor_reg_tab_ptr);
	err |= put_user(((struct sensor_reg_tag *)c),
					&data->sensor_reg_tab_ptr);
	err |= get_user(i, &data32->reg_count);
	err |= put_user(i, &data->reg_count);
	err |= get_user(i, &data32->reg_bits);
	err |= put_user(i, &data->reg_bits);
	err |= get_user(i, &data32->burst_mode);
	err |= put_user(i, &data->burst_mode);

	return err;
}

static long compat_get_sensor_i2c_tag(
			struct compat_sensor_i2c_tag __user *data32,
			struct sensor_i2c_tag __user *data)
{
	unsigned long c;
	uint16_t i;
	int err;

	err = get_user(c, &data32->i2c_data);
	err |= put_user(((uint8_t *)c), &data->i2c_data);
	err |= get_user(i, &data32->i2c_count);
	err |= put_user(i, &data->i2c_count);
	err |= get_user(i, &data32->slave_addr);
	err |= put_user(i, &data->slave_addr);
	err |= get_user(i, &data32->read_len);
	err |= put_user(i, &data->read_len);

	return err;
}

static long compat_get_muti_aec_i2c_tag(
			struct compat_sensor_muti_aec_i2c_tag __user *data32,
			struct sensor_muti_aec_i2c_tag __user *data)
{
	unsigned long c;
	uint16_t i;
	int err;

	err = get_user(c, &data32->sensor_id);
	err |= put_user(compat_ptr(c), &data->sensor_id);
	err |= get_user(i, &data32->id_size);
	err |= put_user(i, &data->id_size);
	err |= get_user(c, &data32->i2c_slave_addr);
	err |= put_user(compat_ptr(c), &data->i2c_slave_addr);
	err |= get_user(i, &data32->i2c_slave_len);
	err |= put_user(i, &data->i2c_slave_len);
	err |= get_user(c, &data32->addr_bits_type);
	err |= put_user(compat_ptr(c), &data->addr_bits_type);
	err |= get_user(i, &data32->addr_bits_type_len);
	err |= put_user(i, &data->addr_bits_type_len);
	err |= get_user(c, &data32->data_bits_type);
	err |= put_user(compat_ptr(c), &data->data_bits_type);
	err |= get_user(i, &data32->data_bits_type_len);
	err |= put_user(i, &data->data_bits_type_len);
	err |= get_user(c, &data32->master_i2c_tab);
	err |= put_user(compat_ptr(c), &data->master_i2c_tab);
	err |= get_user(i, &data32->msize);
	err |= put_user(i, &data->msize);
	err |= get_user(c, &data32->slave_i2c_tab);
	err |= put_user(compat_ptr(c), &data->slave_i2c_tab);
	err |= get_user(i, &data32->ssize);
	err |= put_user(i, &data->ssize);

	return err;
}

static long compat_get_otp_param_tag(
			struct compat_sensor_otp_param_tag __user *data32,
			struct _sensor_otp_param_tag __user *data)
{
	unsigned long c;
	uint32_t i;
	int err;

	err = get_user(i, &data32->type);
	err |= put_user(i, &data->type);
	err |= get_user(i, &data32->start_addr);
	err |= put_user(i, &data->start_addr);
	err |= get_user(i, &data32->len);
	err |= put_user(i, &data->len);
	err |= get_user(c, &data32->buff);
	err |= put_user(((uint8_t *)c), &data->buff);

	err |= get_user(i, &data32->golden.size);
	err |= put_user(i, &data->golden.size);
	err |= get_user(c, &data32->golden.data_ptr);
	err |= put_user(((void *)c), &data->golden.data_ptr);

	err |= get_user(i, &data32->awb.size);
	err |= put_user(i, &data->awb.size);
	err |= get_user(c, &data32->awb.data_ptr);
	err |= put_user(((void *)c), &data->awb.data_ptr);

	err |= get_user(i, &data32->lsc.size);
	err |= put_user(i, &data->lsc.size);
	err |= get_user(c, &data32->lsc.data_ptr);
	err |= put_user(((void *)c), &data->lsc.data_ptr);
	return err;
}

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
		struct sensor_reg_tab_tag __user *data;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_sensor_reg_tab_tag(data32, data);
		if (err)
			break;
		err = filp->f_op->unlocked_ioctl(filp,
				SENSOR_IO_I2C_WRITE_REGS,
				(unsigned long)data);
		break;
	}

	case COMPAT_SENSOR_IO_MUTI_I2C_WRITE:
	{
		struct compat_sensor_muti_aec_i2c_tag __user *data32;
		struct sensor_muti_aec_i2c_tag __user *data;

		data32 = compat_ptr(arg);

		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_muti_aec_i2c_tag(data32, data);
		if (err)
			break;
		err = filp->f_op->unlocked_ioctl(filp,
				SENSOR_IO_MUTI_I2C_WRITE,
				(unsigned long)data);
		break;
	}

	case COMPAT_SENSOR_IO_GRC_I2C_WRITE:
	{
		struct compat_sensor_i2c_tag __user *data32;
		struct sensor_i2c_tag __user *data;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_sensor_i2c_tag(data32, data);
		if (err)
			break;
		err = filp->f_op->unlocked_ioctl(filp,
				SENSOR_IO_GRC_I2C_WRITE,
				(unsigned long)data);
		break;
	}

	case COMPAT_SENSOR_IO_GRC_I2C_READ:
	{
		struct compat_sensor_i2c_tag __user *data32;
		struct sensor_i2c_tag __user *data;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_sensor_i2c_tag(data32, data);
		if (err)
			break;
		err = filp->f_op->unlocked_ioctl(filp,
				SENSOR_IO_GRC_I2C_READ,
				(unsigned long)data);
		break;
	}

	case COMPAT_SENSOR_IO_READ_OTPDATA:
	{
		struct compat_sensor_otp_param_tag __user *data32;
		struct _sensor_otp_param_tag __user *data;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_otp_param_tag(data32, data);
		if (err)
			break;
		err = filp->f_op->unlocked_ioctl(filp,
				SENSOR_IO_READ_OTPDATA,
				(unsigned long)data);
		break;
	}

	default:
		err = filp->f_op->unlocked_ioctl(filp, cmd,
				(unsigned long)compat_ptr(arg));
		break;
	}

	return err;
}
