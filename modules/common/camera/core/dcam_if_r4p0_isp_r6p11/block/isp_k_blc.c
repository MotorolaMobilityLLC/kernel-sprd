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
#define pr_fmt(fmt) "BLC: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define ISP_BLC_B_PARAM_R_MASK (0x3FF)
#define ISP_BLC_B_PARAM_B_MASK (0xFFC00)
#define ISP_BLC_B_PARAM_GR_MASK (0x3FF)
#define ISP_BLC_B_PARAM_GB_MASK (0xFFC00)

static int isp_k_blc_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int mask = 0;
	struct isp_dev_blc_info blc_info;

	memset(&blc_info, 0x00, sizeof(blc_info));
	ret = copy_from_user((void *)&blc_info, param->property_param,
			sizeof(blc_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_BLC_PARAM, BIT_0, blc_info.bypass);
	if (blc_info.bypass) {
		isp_dbg_s_ori_byp(SBLK_BYPASS, raw_blc, idx);
		return 0;
	}

	val = ((blc_info.b & 0x3FF) << 10) | (blc_info.r & 0x3FF);
	mask = ISP_BLC_B_PARAM_R_MASK | ISP_BLC_B_PARAM_B_MASK;
	ISP_REG_MWR(idx, ISP_BLC_B_PARAM_R_B, mask, val);

	val = ((blc_info.gb & 0x3FF) << 10) | (blc_info.gr & 0x3FF);
	mask = ISP_BLC_B_PARAM_GR_MASK | ISP_BLC_B_PARAM_GB_MASK;
	ISP_REG_MWR(idx, ISP_BLC_B_PARAM_G, mask, val);

	return ret;
}

int isp_k_cfg_blc(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx)
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
	case ISP_PRO_BLC_BLOCK:
		ret = isp_k_blc_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
