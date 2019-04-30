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

#include "cam_drv_compat.h"

static int sprd_camdrv_compat_get_set_statis_buf(
		struct compat_isp_statis_buf_input __user *data32,
		struct isp_statis_buf_input __user *data)
{
	int err = 0;
	uint32_t tmp = 0;
	compat_ulong_t val;

	err = get_user(tmp, &data32->buf_size);
	err |= put_user(tmp, &data->buf_size);

	err = get_user(tmp, &data32->dcam_stat_buf_size);
	err |= put_user(tmp, &data->dcam_stat_buf_size);

	err |= get_user(tmp, &data32->buf_num);
	err |= put_user(tmp, &data->buf_num);

	err |= get_user(val, &data32->phy_addr);
	err |= put_user(val, &data->phy_addr);

	err |= get_user(val, &data32->vir_addr);
	err |= put_user(val, &data->vir_addr);

	err |= get_user(val, &data32->addr_offset);
	err |= put_user(val, &data->addr_offset);

	err |= get_user(tmp, &data32->kaddr[0]);
	err |= put_user(tmp, &data->kaddr[0]);

	err |= get_user(tmp, &data32->kaddr[1]);
	err |= put_user(tmp, &data->kaddr[1]);

	err |= get_user(val, &data32->mfd);
	err |= put_user(val, &data->mfd);

	err |= get_user(val, &data32->dev_fd);
	err |= put_user(val, &data->dev_fd);

	err |= get_user(tmp, &data32->buf_property);
	err |= put_user(tmp, &data->buf_property);

	err |= get_user(tmp, &data32->buf_flag);
	err |= put_user(tmp, &data->buf_flag);

	err |= get_user(tmp, &data32->statis_valid);
	err |= put_user(tmp, &data->statis_valid);

	err |= get_user(tmp, &data32->reserved[0]);
	err |= put_user(tmp, &data->reserved[0]);

	err |= get_user(tmp, &data32->reserved[1]);
	err |= put_user(tmp, &data->reserved[1]);

	err |= get_user(tmp, &data32->reserved[2]);
	err |= put_user(tmp, &data->reserved[2]);

	err |= get_user(tmp, &data32->reserved[3]);
	err |= put_user(tmp, &data->reserved[3]);

	return err;
}

static int sprd_camdrv_compat_get_raw_proc_info(
		struct compat_isp_raw_proc_info __user *data32,
		struct isp_raw_proc_info __user *data)
{
	int err = 0;
	uint32_t tmp = 0;
	compat_ulong_t val;

	err = get_user(tmp, &data32->in_size.width);
	err |= put_user(tmp, &data->in_size.width);

	err = get_user(tmp, &data32->in_size.height);
	err |= put_user(tmp, &data->in_size.height);

	err = get_user(tmp, &data32->out_size.width);
	err |= put_user(tmp, &data->out_size.width);

	err = get_user(tmp, &data32->out_size.height);
	err |= put_user(tmp, &data->out_size.height);

	err = get_user(val, &data32->img_vir.chn0);
	err |= put_user(val, &data->img_vir.chn0);

	err = get_user(val, &data32->img_vir.chn1);
	err |= put_user(val, &data->img_vir.chn1);

	err = get_user(val, &data32->img_vir.chn2);
	err |= put_user(val, &data->img_vir.chn2);

	err = get_user(val, &data32->img_offset.chn0);
	err |= put_user(val, &data->img_offset.chn0);

	err = get_user(val, &data32->img_offset.chn1);
	err |= put_user(val, &data->img_offset.chn1);

	err = get_user(val, &data32->img_offset.chn2);
	err |= put_user(val, &data->img_offset.chn2);

	err = get_user(tmp, &data32->img_fd);
	err |= put_user(tmp, &data->img_fd);

	return err;
}

static int sprd_camdrv_compat_get_isp_io_param(
		struct compat_isp_io_param __user *data32,
		struct isp_io_param __user *data)
{
	int err = 0;
	uint32_t tmp = 0;
	unsigned long parm;

	err = get_user(tmp, &data32->isp_id);
	err |= put_user(tmp, &data->isp_id);

	err |= get_user(tmp, &data32->scene_id);
	err |= put_user(tmp, &data->scene_id);

	err |= get_user(tmp, &data32->sub_block);
	err |= put_user(tmp, &data->sub_block);

	err |= get_user(tmp, &data32->property);
	err |= put_user(tmp, &data->property);

	err |= get_user(parm, &data32->property_param);
	err |= put_user(((void *)parm), &data->property_param);

	return err;
}

static int sprd_camdrv_compat_get_isp_capability(
		struct compat_sprd_isp_capability __user *data32,
		struct sprd_isp_capability __user *data)
{
	int err = 0;
	uint32_t tmp = 0;
	unsigned long parm;

	err = get_user(tmp, &data32->isp_id);
	err |= put_user(tmp, &data->isp_id);

	err |= get_user(tmp, &data32->index);
	err |= put_user(tmp, &data->index);

	err |= get_user(parm, &data32->property_param);
	err |= put_user(((void *)parm), &data->property_param);

	return err;
}

static int sprd_camdrv_compat_put_isp_capability(
		struct compat_sprd_isp_capability __user *data32,
		struct sprd_isp_capability __user *data)
{
	int err = 0;
	uint32_t tmp = 0;
	compat_caddr_t parm;

	err = get_user(tmp, &data->isp_id);
	err |= put_user(tmp, &data32->isp_id);

	err |= get_user(tmp, &data->index);
	err |= put_user(tmp, &data32->index);

	err |= get_user(parm, (compat_caddr_t *)&data->property_param);
	err |= put_user(parm, &data32->property_param);

	return err;
}

long sprd_cam_drv_compat_ioctl(
	struct file *file, unsigned int cmd,
	unsigned long param)
{
	long ret = 0;

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_SPRD_ISP_IO_SET_STATIS_BUF:
	{
		struct compat_isp_statis_buf_input __user *data32;
		struct isp_statis_buf_input __user *data;

		data32 = compat_ptr(param);
		data = compat_alloc_user_space(
			sizeof(struct isp_statis_buf_input));

		sprd_camdrv_compat_get_set_statis_buf(data32, data);
		file->f_op->unlocked_ioctl(file,
			SPRD_ISP_IO_SET_STATIS_BUF, (unsigned long)data);
		break;
	}
	case COMPAT_SPRD_ISP_IO_CAPABILITY:
	{
		struct compat_sprd_isp_capability __user *data32;
		struct sprd_isp_capability __user *data;

		data32 = compat_ptr(param);
		data = compat_alloc_user_space(
			sizeof(struct sprd_isp_capability));

		sprd_camdrv_compat_get_isp_capability(data32, data);
		file->f_op->unlocked_ioctl(file,
			SPRD_ISP_IO_CAPABILITY, (unsigned long)data);
		sprd_camdrv_compat_put_isp_capability(data32, data);

		break;
	}
	case COMPAT_SPRD_ISP_IO_CFG_PARAM:
	{
		struct compat_isp_io_param __user *data32;
		struct isp_io_param __user *data;

		data32 = compat_ptr(param);
		data = compat_alloc_user_space(sizeof(struct isp_io_param));
		sprd_camdrv_compat_get_isp_io_param(data32, data);

		file->f_op->unlocked_ioctl(file,
				SPRD_ISP_IO_CFG_PARAM,
				(unsigned long)data);
		break;
	}
	case COMPAT_SPRD_ISP_IO_RAW_CAP:
	{
		struct compat_isp_raw_proc_info __user *data32;
		struct isp_raw_proc_info __user *data;

		data32 = compat_ptr(param);
		data = compat_alloc_user_space(
			sizeof(struct isp_raw_proc_info));

		sprd_camdrv_compat_get_raw_proc_info(data32,
			data);
		file->f_op->unlocked_ioctl(file,
				SPRD_ISP_IO_RAW_CAP,
				(unsigned long)data);
		break;
	}
	default:
		file->f_op->unlocked_ioctl(file, cmd, param);
		break;
	}

	return ret;
}
