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

#include "sprd_mm.h"
#include "sprd_isp_hw.h"
#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "UVD: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

static int isp_k_uvd_block(struct isp_io_param *param,
						enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_dev_uvd_info uvd_info;

	memset(&uvd_info, 0x00, sizeof(uvd_info));
	ret = copy_from_user((void *)&uvd_info,
		param->property_param, sizeof(uvd_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_UVD_PARAM, BIT_0, uvd_info.bypass);
	if (uvd_info.bypass) {
		isp_dbg_s_ori_byp(SBLK_BYPASS, full_uvd, idx);
		return 0;
	}

	val = (uvd_info.lum_th_h_len  & 0x7) |
		  ((uvd_info.lum_th_h     & 0xFF) << 8) |
		  ((uvd_info.lum_th_l_len & 0x7) << 16) |
		  ((uvd_info.lum_th_l     & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_UVD_PARAM0, val);

	val = (uvd_info.chroma_min_h  & 0xFF) |
		  ((uvd_info.chroma_min_l & 0xFF) << 8) |
		  ((uvd_info.chroma_max_h & 0xFF) << 16) |
		  ((uvd_info.chroma_max_l & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_UVD_PARAM1, val);

	val = (uvd_info.u_th.th_h[1]  & 0xFF) |
		  ((uvd_info.u_th.th_l[1] & 0xFF) << 8) |
		  ((uvd_info.u_th.th_h[0] & 0xFF) << 16) |
		  ((uvd_info.u_th.th_l[0] & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_UVD_PARAM2, val);

	val = (uvd_info.v_th.th_h[1]  & 0xFF) |
		  ((uvd_info.v_th.th_l[1] & 0xFF) << 8) |
		  ((uvd_info.v_th.th_h[0] & 0xFF) << 16) |
		  ((uvd_info.v_th.th_l[0] & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_UVD_PARAM3, val);

	val = (uvd_info.ratio           & 0x7F) |
		  ((uvd_info.ratio_uv_min   & 0x7F) << 8) |
		  ((uvd_info.ratio_y_min[0] & 0x7F) << 16) |
		  ((uvd_info.ratio_y_min[1] & 0x7F) << 24);
	ISP_REG_WR(idx, ISP_UVD_PARAM4, val);

	val = (uvd_info.ratio0 & 0x7F) |
		  ((uvd_info.ratio1 & 0x7F) << 8)  |
		  ((uvd_info.y_th_l_len & 0x7)  << 16) |
		  ((uvd_info.y_th_h_len & 0x7)  << 20) |
		  ((uvd_info.uv_abs_th_len & 0x7)  << 24);
	ISP_REG_WR(idx, ISP_UVD_PARAM5, val);

	return ret;
}

int isp_k_cfg_uvd(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param.\n");
		return -EPERM;
	}

	if (!param->property_param) {
		pr_err("fail to get property param.\n");
		return -EPERM;
	}

	switch (param->property) {
	case ISP_PRO_UVD_BLOCK:
		ret = isp_k_uvd_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
