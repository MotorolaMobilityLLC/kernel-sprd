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
#define pr_fmt(fmt) "3DLUT: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int isp_k_3dlut_block(struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	int i = 0, val = 0;
	struct isp_dev_3dlut_info *lut3d_info = NULL;

	if (!isp_k_param) {
		pr_err("fail to get input ptr\n");
		return -1;
	}
	if (isp_k_param->lut3d_info.isupdate == 0)
		return ret;

	lut3d_info = &isp_k_param->lut3d_info;
	isp_k_param->lut3d_info.isupdate = 0;

	if (g_isp_bypass[idx] & (1 << _EISP_3DLUT))
		lut3d_info->rgb3dlut_bypass = 1;
	if (lut3d_info->rgb3dlut_bypass) {
		pr_debug("idx %d, 3dlut_bypass!\n", idx);
		return 0;
	}

	for (i = 0; i < ISP_LUT3D_NUM; i++) {
		val = (lut3d_info->rgb3dlut_ct_table[i][5] & 0x3FF) |
			((lut3d_info->rgb3dlut_ct_table[i][4] & 0x3FF) << 10) |
			((lut3d_info->rgb3dlut_ct_table[i][3] & 0x3FF) << 20);
		ISP_REG_MWR(idx, ISP_3D_LUT_BUF0_CH0 + i * 4, 0x3FFFFFFF, val);
	}

	return ret;
}

int isp_k_cfg_3dlut(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_3dlut_info *lut3d_info = NULL;

	lut3d_info = &isp_k_param->lut3d_info;

	switch (param->property) {
	case ISP_PRO_3DLUT_BLOCK:
		ret = copy_from_user((void *)lut3d_info, param->property_param, sizeof(struct isp_dev_3dlut_info));
		if (ret != 0) {
			pr_err("fail to copy from user, ret = %d\n", ret);
			return ret;
		}
		lut3d_info->isupdate = 1;
		if (g_isp_bypass[idx] & (1 << _EISP_3DLUT))
			lut3d_info->rgb3dlut_bypass = 1;
		break;
	default:
		pr_err("fail to idx %d, support cmd id = %d\n", idx, param->property);
		break;
	}

	return ret;
}

int isp_k_cpy_3dlut(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if (isp_k_param->lut3d_info.isupdate == 1) {
		memcpy(&param_block->lut3d_info, &isp_k_param->lut3d_info, sizeof(struct isp_dev_3dlut_info));
		isp_k_param->lut3d_info.isupdate = 0;
		param_block->lut3d_info.isupdate = 1;
	}

	return ret;
}
