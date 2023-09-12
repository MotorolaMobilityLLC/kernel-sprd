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
#include "cam_queue.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "IIRCNR: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int isp_k_iircnr_block(struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	uint32_t val = 0;
	uint32_t i = 0;
	struct isp_dev_iircnr_info *iircnr_info = NULL;
	if (isp_k_param->iircnr_info.isupdate == 0)
		return ret;

	iircnr_info = &isp_k_param->iircnr_info;
	isp_k_param->iircnr_info.isupdate = 0;

	if (g_isp_bypass[idx] & (1 << _EISP_IIRCNR))
		iircnr_info->bypass = 1;
	if (iircnr_info->bypass)
		return 0;

	val = ((iircnr_info->y_min_th & 0xFF) << 20)
		| ((iircnr_info->y_max_th & 0xFF) << 12)
		| ((iircnr_info->uv_th & 0x7FF) << 1)
		| (iircnr_info->mode & 0x1);
	ISP_REG_MWR(idx, ISP_IIRCNR_PARAM1, 0xFFFFFFF, val);

	val = ((iircnr_info->sat_ratio & 0x7F) << 24)
		| ((iircnr_info->uv_pg_th & 0x3FFF) << 10)
		| (iircnr_info->uv_dist & 0x3FF);
	ISP_REG_WR(idx, ISP_IIRCNR_PARAM2, val);

	val = ((iircnr_info->uv_low_thr2 & 0x1FFFF) << 14)
		| (iircnr_info->uv_low_thr1 & 0x3FFF);
	ISP_REG_WR(idx, ISP_IIRCNR_PARAM3, val);

	val = iircnr_info->ymd_u & 0x3FFFFFFF;
	ISP_REG_WR(idx, ISP_IIRCNR_PARAM4, val);

	val = iircnr_info->ymd_v & 0x3FFFFFFF;
	ISP_REG_WR(idx, ISP_IIRCNR_PARAM5, val);

	val = ((iircnr_info->y_th & 0xFF) << 19)
		| ((iircnr_info->slope_y_0 & 0x7FF) << 8)
		| (iircnr_info->uv_s_th & 0xFF);
	ISP_REG_WR(idx, ISP_IIRCNR_PARAM6, val);

	val = (iircnr_info->alpha_low_v & 0x3FFF)
		| ((iircnr_info->alpha_low_u & 0x3FFF) << 14);
	ISP_REG_WR(idx, ISP_IIRCNR_PARAM7, val);

	ISP_REG_WR(idx, ISP_IIRCNR_PARAM8,
		iircnr_info->middle_factor_y_0 & 0x7FFFFFF);

	val = iircnr_info->uv_high_thr2_0 & 0x1FFFF;
	ISP_REG_WR(idx, ISP_IIRCNR_PARAM9, val);

	ISP_REG_WR(idx, ISP_YUV_IIRCNR_NEW_0,
		iircnr_info->ymd_min_u & 0x3FFFFFFF);
	ISP_REG_WR(idx, ISP_YUV_IIRCNR_NEW_1,
		iircnr_info->ymd_min_v & 0x3FFFFFFF);

	for (i = 0; i < 7; i++) {
		val = ((iircnr_info->uv_low_thr[i][0] & 0x1FFFF) << 14) |
			(iircnr_info->uv_low_thr[i][1] & 0x3FFF);
		ISP_REG_WR(idx, ISP_YUV_IIRCNR_NEW_2 + 4 * i, val);
	}

	for (i = 0; i < 8; i += 2) {
		val = ((iircnr_info->y_edge_thr_max[i + 1] & 0xFFFF) << 16) |
			(iircnr_info->y_edge_thr_max[i] & 0xFFFF);
		ISP_REG_WR(idx, ISP_YUV_IIRCNR_NEW_9 + 4 * (i >> 1), val);

		val = ((iircnr_info->y_edge_thr_min[i + 1] & 0xFFFF) << 16) |
			(iircnr_info->y_edge_thr_min[i] & 0xFFFF);
		ISP_REG_WR(idx, ISP_YUV_IIRCNR_NEW_13 + 4 * (i >> 1), val);
	}

	for (i = 0; i < 7; i++) {
		val = ((iircnr_info->uv_high_thr2[i] & 0x1FFFF) << 11) |
			(iircnr_info->slope_y[i] & 0x7FF);
		ISP_REG_WR(idx, ISP_YUV_IIRCNR_NEW_17 + 4 * i, val);

		ISP_REG_WR(idx, ISP_YUV_IIRCNR_NEW_24 + 4 * i,
			iircnr_info->middle_factor_y[i] & 0x7FFFFFF);
	}

	for (i = 0; i < 8; i++) {
		val = ((iircnr_info->middle_factor_uv[i] & 0xFFFFFF) << 7) |
			(iircnr_info->slope_uv[i] & 0x7F);
		ISP_REG_WR(idx, ISP_YUV_IIRCNR_NEW_31 + 4 * i, val);
	}

	val = ((iircnr_info->pre_uv_th & 0xFF) << 16) |
		((iircnr_info->css_lum_thr & 0xFF) << 8) |
		(iircnr_info->uv_diff_thr & 0xFF);
	ISP_REG_WR(idx, ISP_YUV_IIRCNR_NEW_39, val);

	return ret;
}


int isp_k_cfg_iircnr(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_iircnr_info *iircnr_info = NULL;

	iircnr_info = &isp_k_param->iircnr_info;

	switch (param->property) {
	case ISP_PRO_IIRCNR_BLOCK:
		ret = copy_from_user((void *)iircnr_info, param->property_param, sizeof(struct isp_dev_iircnr_info));
		if (ret != 0) {
			pr_err("fail to copy from user, ret = %d\n", ret);
			return -1;
		}
		isp_k_param->iircnr_info.isupdate = 1;

		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}

int isp_k_cpy_iircnr(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if (isp_k_param->iircnr_info.isupdate == 1) {
		memcpy(&param_block->iircnr_info, &isp_k_param->iircnr_info, sizeof(struct isp_dev_iircnr_info));
		isp_k_param->iircnr_info.isupdate = 0;
		param_block->iircnr_info.isupdate = 1;
	}

	return ret;
}
