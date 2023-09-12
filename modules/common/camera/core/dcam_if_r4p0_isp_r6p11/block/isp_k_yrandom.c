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
#define pr_fmt(fmt) "YRANDOM: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

static int32_t isp_k_yrandom_block(struct isp_io_param *param, enum isp_id idx)
{
	int32_t ret = 0;
	struct isp_dev_yrandom_info yrandom_info;
	uint32_t val = 0;

	memset(&yrandom_info, 0x00, sizeof(yrandom_info));
	ret = copy_from_user((void *)&yrandom_info,
			param->property_param, sizeof(yrandom_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_YRANDOM_PARAM1, BIT_0, yrandom_info.bypass);
	if (yrandom_info.bypass) {
		isp_dbg_s_ori_byp(SBLK_BYPASS, yuv_random, idx);
		return 0;
	}

	ISP_REG_MWR(idx, ISP_YRANDOM_PARAM1, 0xFFFFFF00,
		yrandom_info.seed << 8);
	ISP_REG_MWR(idx, ISP_YRANDOM_PARAM1, BIT_1, yrandom_info.mode << 1);
	ISP_REG_MWR(idx, ISP_YRANDOM_INIT, BIT_0, yrandom_info.init);
	ISP_REG_MWR(idx, ISP_YRANDOM_PARAM2, 0x7FF0000,
		yrandom_info.offset << 16);
	ISP_REG_MWR(idx, ISP_YRANDOM_PARAM2, 0xF, yrandom_info.shift);

	val = (yrandom_info.takeBit[0] & 0xF)
		| ((yrandom_info.takeBit[1] & 0xF) << 4)
		| ((yrandom_info.takeBit[2] & 0xF) << 8)
		| ((yrandom_info.takeBit[3] & 0xF) << 12)
		| ((yrandom_info.takeBit[4] & 0xF) << 16)
		| ((yrandom_info.takeBit[5] & 0xF) << 20)
		| ((yrandom_info.takeBit[6] & 0xF) << 24)
		| ((yrandom_info.takeBit[7] & 0xF) << 28);
	ISP_REG_WR(idx, ISP_YRANDOM_PARAM3, val);

	return ret;
}

int isp_k_cfg_yrandom(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param.\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property param.\n");
		return -EPERM;
	}

	switch (param->property) {
	case ISP_PRO_YRANDOM_BLOCK:
		ret = isp_k_yrandom_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
