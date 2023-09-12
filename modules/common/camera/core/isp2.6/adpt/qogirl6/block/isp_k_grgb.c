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
#define pr_fmt(fmt) "GRGB: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_grgb_block(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	int  i = 0;
	uint32_t val = 0;
	struct isp_dev_grgb_info *grgb_info;

	grgb_info = &isp_k_param->grgb_info;

	ret = copy_from_user((void *)grgb_info,
			param->property_param,
			sizeof(struct isp_dev_grgb_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	if (g_isp_bypass[idx] & (1 << _EISP_GC))
		return 0;
	/*valid when bpc_mode_en_gc=1*/
	ISP_REG_MWR(idx, ISP_GRGB_CTRL, BIT_0, grgb_info->bypass);
	if (grgb_info->bypass)
		return 0;

	val = ((grgb_info->slash_edge_thr & 0x7F) << 24) |
		(grgb_info->check_sum_clr & 0x01) << 23 |
		((grgb_info->hv_edge_thr & 0x7F) << 16) |
		((grgb_info->diff_thd & 0x3FF) << 1);
	ISP_REG_MWR(idx, ISP_GRGB_CTRL, 0x7FFF07FE, val);

	val = ((grgb_info->gb_ratio & 0x1F) << 26) |
		((grgb_info->hv_flat_thr & 0x3FF) << 16) |
		((grgb_info->gr_ratio & 0x1F) << 10) |
		(grgb_info->slash_flat_thr & 0x3FF);
	ISP_REG_WR(idx, ISP_GRGB_CFG0, val);

	for (i = 0; i < 3; i++) {
		val = ((grgb_info->lum.curve_t[i][0] & 0x3FF) << 20) |
			((grgb_info->lum.curve_t[i][1] & 0x3FF) << 10) |
			(grgb_info->lum.curve_t[i][2] & 0x3FF);
		ISP_REG_WR(idx, ISP_GRGB_LUM_FLAT_T + i * 8, val);

		val = ((grgb_info->lum.curve_t[i][3] & 0x3FF) << 15) |
			((grgb_info->lum.curve_r[i][0] & 0x1F) << 10) |
			((grgb_info->lum.curve_r[i][1] & 0x1F) << 5) |
			(grgb_info->lum.curve_r[i][2] & 0x1F);
		ISP_REG_WR(idx, ISP_GRGB_LUM_FLAT_R + i * 8, val);
	}

	for (i = 0; i < 3; i++) {
		val = ((grgb_info->frez.curve_t[i][0] & 0x3FF) << 20) |
			((grgb_info->frez.curve_t[i][1] & 0x3FF) << 10) |
			(grgb_info->frez.curve_t[i][2] & 0x3FF);
		ISP_REG_WR(idx, ISP_GRGB_FREZ_FLAT_T + i * 8, val);

		val = ((grgb_info->frez.curve_t[i][3] & 0x3FF) << 15) |
			((grgb_info->frez.curve_r[i][0] & 0x1F) << 10) |
			((grgb_info->frez.curve_r[i][1] & 0x1F) << 5) |
			(grgb_info->frez.curve_r[i][2] & 0x1F);
		ISP_REG_WR(idx, ISP_GRGB_FREZ_FLAT_R + i * 8, val);
	}

	return ret;
}

int isp_k_cfg_grgb(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;

	if (param->property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_GRGB_BLOCK:
		ret = isp_k_grgb_block(param, isp_k_param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
