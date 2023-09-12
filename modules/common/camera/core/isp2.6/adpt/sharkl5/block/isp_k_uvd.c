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

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "UVD : %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int isp_k_uvd_block(struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	uint32_t val = 0;
	struct isp_dev_uvd_info_v2 *uvd_info = NULL;
	if (isp_k_param->uvd_info_v2.isupdate == 0)
		return ret;
	uvd_info = &isp_k_param->uvd_info_v2;
	isp_k_param->uvd_info_v2.isupdate = 0;

	if (g_isp_bypass[idx] & (1 << _EISP_UVD))
		uvd_info->bypass = 1;
	if (uvd_info->bypass)
		return 0;

	val = (uvd_info->lum_th_h_len & 0x7) |
		((uvd_info->lum_th_h & 0xFF) << 8) |
		((uvd_info->lum_th_l_len & 0x7) << 16) |
		((uvd_info->lum_th_l & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_UVD_PARAM0, val);

	/* new: update from min_h/min_l  to chroma_ratio  */
	val = ((uvd_info->chroma_ratio & 0x7F)) |
			((uvd_info->chroma_max_h & 0xFF) << 16) |
			((uvd_info->chroma_max_l & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_UVD_PARAM1, val);

	val = (uvd_info->u_th.th_h[1] & 0xFF) |
		((uvd_info->u_th.th_l[1] & 0xFF) << 8) |
		((uvd_info->u_th.th_h[0] & 0xFF) << 16) |
		((uvd_info->u_th.th_l[0] & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_UVD_PARAM2, val);

	val = (uvd_info->v_th.th_h[1] & 0xFF) |
		((uvd_info->v_th.th_l[1] & 0xFF) << 8) |
		((uvd_info->v_th.th_h[0] & 0xFF) << 16) |
		((uvd_info->v_th.th_l[0] & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_UVD_PARAM3, val);

	val = (uvd_info->luma_ratio & 0x7F) |
		((uvd_info->ratio_uv_min & 0x7F) << 8) |
		((uvd_info->ratio_y_min[0] & 0x7F) << 16) |
		((uvd_info->ratio_y_min[1] & 0x7F) << 24);
	ISP_REG_WR(idx, ISP_UVD_PARAM4, val);

	val = (uvd_info->ratio0 & 0x7F) |
		((uvd_info->ratio1 & 0x7F) << 8) |
		((uvd_info->y_th_l_len & 0x7) << 16) |
		((uvd_info->y_th_h_len & 0x7) << 20) |
		((uvd_info->uv_abs_th_len & 0x7) << 24);
	ISP_REG_WR(idx, ISP_UVD_PARAM5, val);

	return ret;
}

int isp_k_cfg_uvd(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_uvd_info_v2 *uvd_info = NULL;
	uvd_info = &isp_k_param->uvd_info_v2;

	switch (param->property) {
	case ISP_PRO_UVD_BLOCK:
		ret = copy_from_user((void *)uvd_info, param->property_param, sizeof(struct isp_dev_uvd_info_v2));
		if (ret != 0) {
			pr_err("fail to copy from user, ret = %d\n", ret);
			return ret;
		}
		isp_k_param->uvd_info_v2.isupdate = 1;
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}

int isp_k_cpy_uvd(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if (isp_k_param->uvd_info_v2.isupdate == 1) {
		memcpy(&param_block->uvd_info_v2, &isp_k_param->uvd_info_v2, sizeof(struct isp_dev_uvd_info_v2));
		isp_k_param->uvd_info_v2.isupdate = 0;
		param_block->uvd_info_v2.isupdate = 1;
	}

	return ret;
}

