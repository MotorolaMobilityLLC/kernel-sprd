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

#include <linux/uaccess.h>
#include <sprd_mm.h>

#include "isp_hw.h"
#include "isp_reg.h"
#include "cam_types.h"
#include "cam_block.h"
#include "isp_core.h"
#include "cam_queue.h"
#include "isp_pyr_dec.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "DCT: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int isp_k_dct_block(struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_dct_info *dct = NULL;
	if (isp_k_param->dct_info.isupdate == 0)
		return ret;

	dct = &isp_k_param->dct_info;
	isp_k_param->dct_info.isupdate = 0;

	if (g_isp_bypass[idx] & (1 << _EISP_DCT))
		dct->bypass = 1;

	return ret;
}

int isp_k_cfg_dct(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_dct_info *dct = NULL;

	dct = &isp_k_param->dct_info;

	switch (param->property) {
	case ISP_PRO_DCT_BLOCK:
		ret = copy_from_user((void *)dct, param->property_param, sizeof(struct isp_dev_dct_info));
		if (ret != 0) {
			pr_err("fail to copy from user, ret = %d\n", ret);
			return ret;
		}
		dct->isupdate = 1;
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}

int isp_k_update_dct(void *handle)
{
	int ret = 0;
	uint32_t radius = 0, radius_limit = 0;
	uint32_t new_width = 0, old_width = 0, sensor_width = 0;
	uint32_t new_height = 0, old_height = 0, sensor_height = 0;
	struct isp_dev_dct_info *dct_info = NULL;
	struct isp_dec_pipe_dev *dec_dev = NULL;

	if (!handle) {
		pr_err("fail to get invalid in ptr\n");
		return -EFAULT;
	}

	dec_dev = (struct isp_dec_pipe_dev *)handle;

	new_width = dec_dev->dct_ynr_info.new_width;
	new_height = dec_dev->dct_ynr_info.new_height;
	old_width = dec_dev->dct_ynr_info.old_width;
	old_height = dec_dev->dct_ynr_info.old_height;
	sensor_width = dec_dev->dct_ynr_info.sensor_width;
	sensor_height = dec_dev->dct_ynr_info.sensor_height;

	dct_info = dec_dev->dct_ynr_info.dct;
	if (dct_info->bypass)
		return 0;

	if (dct_info->rnr_radius_base == 0)
		dct_info->rnr_radius_base = 1024;

	radius = sensor_height * dct_info->rnr_radius_factor / dct_info->rnr_radius_base;
	radius_limit = new_height;
	radius = (radius < radius_limit) ? radius : radius_limit;
	radius = new_height * radius / old_height;

	dec_dev->dct_ynr_info.dct_radius = radius;
	pr_debug("base %d, factor %d, radius %d\n", dct_info->rnr_radius_base, dct_info->rnr_radius_factor, radius);

	return ret;
}

int isp_k_cpy_dct(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if (isp_k_param->dct_info.isupdate == 1) {
		memcpy(&param_block->dct_info, &isp_k_param->dct_info, sizeof(struct isp_dev_dct_info));
		isp_k_param->dct_info.isupdate = 0;
		param_block->dct_info.isupdate = 1;
	}

	return ret;
}
