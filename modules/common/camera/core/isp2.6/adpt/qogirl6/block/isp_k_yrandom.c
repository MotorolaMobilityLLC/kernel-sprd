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
#define pr_fmt(fmt) "YRANDOM: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int isp_k_yrandom_block(struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	uint32_t val = 0;
	struct isp_dev_yrandom_info *yrandom_info = NULL;
	if (isp_k_param->yrandom_info.isupdate == 0)
		return ret;

	yrandom_info = &isp_k_param->yrandom_info;
	isp_k_param->yrandom_info.isupdate = 0;

	if (g_isp_bypass[idx] & (1 << _EISP_YRAND))
		yrandom_info->bypass = 1;
	if (yrandom_info->bypass)
		return 0;

	val = (yrandom_info->seed << 8) |
		((yrandom_info->mode & 1) << 1);
	ISP_REG_MWR(idx, ISP_YRANDOM_PARAM1, 0xFFFFFF02, val);

	if (yrandom_info->mode == 0)
		ISP_REG_WR(idx, ISP_YRANDOM_INIT, 1);

	val = (yrandom_info->shift & 0xF) |
		((yrandom_info->offset & 0x7FF) << 16);
	ISP_REG_MWR(idx, ISP_YRANDOM_PARAM2, 0x7FF000F, val);

	val = (yrandom_info->takeBit[0]  & 0xF) |
		((yrandom_info->takeBit[1] & 0xF) << 4) |
		((yrandom_info->takeBit[2] & 0xF) << 8) |
		((yrandom_info->takeBit[3] & 0xF) << 12) |
		((yrandom_info->takeBit[4] & 0xF) << 16) |
		((yrandom_info->takeBit[5] & 0xF) << 20) |
		((yrandom_info->takeBit[6] & 0xF) << 24) |
		((yrandom_info->takeBit[7] & 0xF) << 28);
	ISP_REG_WR(idx, ISP_YRANDOM_PARAM3, val);

	return ret;
}

int isp_k_cfg_yrandom(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_yrandom_info *yrandom_info = NULL;

	yrandom_info = &isp_k_param->yrandom_info;

	switch (param->property) {
	case ISP_PRO_YRANDOM_BLOCK:
		ret = copy_from_user((void *)yrandom_info, param->property_param, sizeof(struct isp_dev_yrandom_info));
		if (ret != 0) {
			pr_err("fail to copy from user, ret = %d\n", ret);
			return ret;
		}
		isp_k_param->yrandom_info.isupdate = 1;
		break;
	default:
		pr_err("fail to support cmd id:%d\n", param->property);
		break;
	}

	return ret;
}

int isp_k_cpy_yrandom(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if (isp_k_param->yrandom_info.isupdate == 1) {
		memcpy(&param_block->yrandom_info, &isp_k_param->yrandom_info, sizeof(struct isp_dev_yrandom_info));
		isp_k_param->yrandom_info.isupdate = 0;
		param_block->yrandom_info.isupdate = 1;
	}
	return ret;
}
