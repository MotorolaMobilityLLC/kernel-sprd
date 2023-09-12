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
#define pr_fmt(fmt) "HUE: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

static int isp_k_hue_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	struct isp_dev_hue_info hue_info;

	memset(&hue_info, 0x00, sizeof(hue_info));
	ret = copy_from_user((void *)&hue_info,
		param->property_param, sizeof(hue_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}
	ISP_REG_MWR(idx, ISP_HUA_PARAM, BIT_0, hue_info.bypass);
	if (hue_info.bypass) {
		isp_dbg_s_ori_byp(SBLK_BYPASS, yuv_hua, idx);
		return 0;
	}

	ISP_REG_MWR(idx, ISP_HUA_PARAM, 0x1FF0, hue_info.theta << 4);

	return ret;
}

int isp_k_cfg_hue(struct isp_io_param *param,
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
	case ISP_PRO_HUE_BLOCK:
		ret = isp_k_hue_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
