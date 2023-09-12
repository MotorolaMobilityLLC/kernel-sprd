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
#include "dcam_block.h"
#include "dcam_drv.h"
#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "BLC: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

static int dcam_k_blc_block(struct isp_io_param *param, enum dcam_id idx)
{
	int ret = 0;
	struct isp_dev_blc_info blc_info;

	memset(&blc_info, 0x00, sizeof(blc_info));
	ret = copy_from_user((void *)&blc_info, param->property_param,
			sizeof(blc_info));
	if (unlikely(ret != 0)) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EPERM;
	}

	sprd_dcam_glb_reg_mwr(idx, DCAM0_BLC_BYPASS,
		  BIT_0,
	      blc_info.bypass,
	      DCAM_REG_MAX);

	if (blc_info.bypass)
		return 0;

	sprd_dcam_glb_reg_mwr(idx, DCAM0_BLC_PARA_R_B,
	      0x000FFFFF,
	      ((blc_info.b & 0x3FF) << 10) |
	      (blc_info.r & 0x3FF),
	      DCAM_REG_MAX);

	sprd_dcam_glb_reg_mwr(idx, DCAM0_BLC_PARA_G,
	      0x000FFFFF,
	      ((blc_info.gb & 0x3FF) << 10) |
	      (blc_info.gr & 0x3FF),
	      DCAM_REG_MAX);

	return ret;
}

int dcam_k_cfg_blc(struct isp_io_param *param, enum dcam_id idx)
{
	int ret = 0;

	if (unlikely(!param || !param->property_param)) {
		pr_err("fail to get param: %p or property param.\n", param);
		return -EPERM;
	}

	switch (param->property) {
	case DCAM_PRO_BLC_BLOCK:
		ret = dcam_k_blc_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
