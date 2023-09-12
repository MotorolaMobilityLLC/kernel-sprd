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
#define pr_fmt(fmt) "CNR: %d %d %s : "fmt, current->pid, __LINE__, __func__

int isp_k_cnr_block(struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_cnr_h_info *cnr = NULL;

	if (isp_k_param->cnr_info.isupdate == 0)
		return ret;

	cnr = &isp_k_param->cnr_info;
	isp_k_param->cnr_info.isupdate = 0;
	if (g_isp_bypass[idx] & (1 << _EISP_CNR))
		cnr->bypass = 1;
	return ret;
}

int isp_k_cfg_cnr(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_cnr_h_info *cnr = NULL;

	cnr = &isp_k_param->cnr_info;

	switch (param->property) {
	case ISP_PRO_CNR_H_BLOCK:
		ret = copy_from_user((void *)cnr, param->property_param,
				sizeof(struct isp_dev_cnr_h_info));
		if (ret != 0) {
			pr_err("fail to copy from user, ret = %d\n", ret);
			return ret;
		}
		cnr->isupdate = 1;
		if (g_isp_bypass[idx] & (1 << _EISP_CNR))
			cnr->bypass = 1;
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}

int isp_k_update_cnr(void *handle)
{
	int ret = 0;
	uint32_t radius = 0, radius_limit = 0;
	uint32_t idx = 0, new_width = 0, old_width = 0, sensor_width = 0;
	uint32_t new_height = 0, old_height = 0, sensor_height = 0;
	struct isp_dev_cnr_h_info *cnr_info_h = NULL;
	struct isp_cnr_h_info *param_cnr_info = NULL;
	struct isp_sw_context *pctx = NULL;

	if (!handle) {
		pr_err("fail to get invalid in ptr\n");
		return -EFAULT;
	}

	pctx = (struct isp_sw_context *)handle;
	idx = pctx->ctx_id;
	new_width = pctx->isp_using_param->blkparam_info.new_width;
	new_height = pctx->isp_using_param->blkparam_info.new_height;
	old_width = pctx->isp_using_param->blkparam_info.old_width;
	old_height = pctx->isp_using_param->blkparam_info.old_height;
	sensor_width = pctx->uinfo.sn_size.w;
	sensor_height = pctx->uinfo.sn_size.h;

	cnr_info_h = &pctx->isp_using_param->cnr_info;
	param_cnr_info = &cnr_info_h->layer_cnr_h[0];

	if (cnr_info_h->bypass)
		return 0;

	if (cnr_info_h->radius_base == 0)
		cnr_info_h->radius_base = 1024;

	radius = ((sensor_width + sensor_height) / 4) * param_cnr_info->base_radius_factor / cnr_info_h->radius_base;
	radius_limit = (new_height + new_width) / 4;
	radius = (radius < radius_limit) ? radius : radius_limit;
	radius = new_height * radius / old_height;

	pctx->isp_using_param->cnr_radius = radius;
	pr_debug("base %d, factor %d, radius %d\n", cnr_info_h->radius_base, param_cnr_info->base_radius_factor, radius);

	return ret;
}

int isp_k_cpy_cnr(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if (isp_k_param->cnr_info.isupdate == 1) {
		memcpy(&param_block->cnr_info, &isp_k_param->cnr_info, sizeof(struct isp_dev_cnr_h_info));
		isp_k_param->cnr_info.isupdate = 0;
		param_block->cnr_info.isupdate = 1;
	}

	return ret;
}
