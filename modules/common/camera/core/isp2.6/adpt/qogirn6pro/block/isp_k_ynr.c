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

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "YNR: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int isp_k_ynr_block(struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_ynr_info_v3 *ynr = NULL;
	if (isp_k_param->ynr_info_v3.isupdate == 0)
		return ret;

	ynr = &isp_k_param->ynr_info_v3;
	isp_k_param->ynr_info_v3.isupdate = 0;

	if (g_isp_bypass[idx] & (1 << _EISP_YNR))
		ynr->bypass = 1;

	return ret;
}

int isp_k_cfg_ynr(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_ynr_info_v3 *ynr = NULL;

	ynr = &isp_k_param->ynr_info_v3;

	switch (param->property) {
	case ISP_PRO_YNR_BLOCK:
		ret = copy_from_user((void *)ynr, param->property_param, sizeof(struct isp_dev_ynr_info_v3));
		if (ret != 0) {
			pr_err("fail to copy from user, ret = %d\n", ret);
			return ret;
		}

		isp_k_param->ynr_info_v3.isupdate = 1;

		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}

int isp_k_update_ynr(void *handle)
{
	int ret = 0;
	uint32_t radius = 0, radius_limit = 0;
	uint32_t idx = 0, new_width = 0, old_width = 0, sensor_width = 0;
	uint32_t new_height = 0, old_height = 0, sensor_height = 0;
	struct isp_dev_ynr_info_v3 *ynr_info = NULL;
	struct isp_sw_context *pctx = NULL;

	if (!handle) {
		pr_err("fail to get invalid in ptr\n");
		return -EFAULT;
	}

	pctx = (struct isp_sw_context *)handle;
	idx = pctx->ctx_id;
	new_width = pctx->isp_k_param.blkparam_info.new_width;
	new_height = pctx->isp_k_param.blkparam_info.new_height;
	old_width = pctx->isp_k_param.blkparam_info.old_width;
	old_height = pctx->isp_k_param.blkparam_info.old_height;
	sensor_width = pctx->uinfo.sn_size.w;
	sensor_height = pctx->uinfo.sn_size.h;

	ynr_info = &pctx->isp_k_param.ynr_info_v3;
	if (ynr_info->bypass)
		return 0;

	if (ynr_info->radius_base == 0)
		ynr_info->radius_base = 1024;

	radius = sensor_height * ynr_info->radius_factor / ynr_info->radius_base;
	radius_limit = new_height;
	radius = (radius < radius_limit) ? radius : radius_limit;
	radius = new_height * radius / old_height;

	pctx->isp_k_param.ynr_radius = radius;
	pr_debug("base %d, factor %d, radius %d\n", ynr_info->radius_base, ynr_info->radius_factor, radius);

	return ret;
}

int isp_k_cpy_ynr(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if (isp_k_param->ynr_info_v3.isupdate == 1) {
		memcpy(&param_block->ynr_info_v3, &isp_k_param->ynr_info_v3, sizeof(struct isp_dev_ynr_info_v3));
		isp_k_param->ynr_info_v3.isupdate = 0;
		param_block->ynr_info_v3.isupdate = 1;
	}

	return ret;
}
