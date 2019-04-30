/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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
#include <video/sprd_mm.h>

#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "BLC: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_blc_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_dev_blc_info blc_info;

	memset(&blc_info, 0x00, sizeof(blc_info));
	ret = copy_from_user((void *)&blc_info, param->property_param,
			sizeof(blc_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	DCAM_REG_MWR(idx, DCAM_BLC_PARA_R_B, BIT_31, blc_info.bypass << 31);
	if (blc_info.bypass)
		return 0;

	val = ((blc_info.b & 0x3FFF) << 10) | (blc_info.r & 0x3FFF);
	DCAM_REG_WR(idx, DCAM_BLC_PARA_R_B, val);

	val = ((blc_info.gb & 0x3FFF) << 10) | (blc_info.gr & 0x3FFF);
	DCAM_REG_WR(idx, DCAM_BLC_PARA_G, val);

	return ret;
}

int isp_k_cfg_blc(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t com_idx)
{
	int ret = 0;
	enum isp_id idx = ISP_ID_0;

	if (!param) {
		pr_err("fail to get param\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get param\n");
		return -EPERM;
	}

	idx = ISP_GET_ISP_ID(com_idx);
	switch (param->property) {
	case ISP_PRO_BLC_BLOCK:
		ret = isp_k_blc_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
