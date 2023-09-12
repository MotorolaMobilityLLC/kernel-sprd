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
#define pr_fmt(fmt) "POST_BLC: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

#define ISP_POST_BLC_B_PARAM_R_MASK (0x3FF)
#define ISP_POST_BLC_B_PARAM_B_MASK (0xFFC00)
#define ISP_POST_BLC_B_PARAM_GR_MASK (0x3FF)
#define ISP_POST_BLC_B_PARAM_GB_MASK (0xFFC00)

static int isp_k_post_blc_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int mask = 0;
	struct isp_dev_post_blc_info post_blc_info;

	memset(&post_blc_info, 0x00, sizeof(post_blc_info));
	ret = copy_from_user((void *)&post_blc_info,
		param->property_param, sizeof(post_blc_info));
	if (ret != 0) {
		pr_err("fail to copy param form user, ret = %d\n", ret);
		return -EFAULT;
	}

	ISP_REG_MWR(idx, ISP_POST_BLC_PARA, BIT_0, post_blc_info.bypass);
	if (post_blc_info.bypass) {
		isp_dbg_s_ori_byp(SBLK_BYPASS, raw_postblc, idx);
		return 0;
	}

	val = ((post_blc_info.b_para & 0x3FF) << 10)
		| (post_blc_info.r_para & 0x3FF);
	mask = ISP_POST_BLC_B_PARAM_R_MASK | ISP_POST_BLC_B_PARAM_B_MASK;
	ISP_REG_MWR(idx, ISP_POST_BLC_B_PARA_R_B, mask, val);

	val = ((post_blc_info.gb_para & 0x3FF) << 10)
		| (post_blc_info.gr_para & 0x3FF);
	mask = ISP_POST_BLC_B_PARAM_GR_MASK | ISP_POST_BLC_B_PARAM_GB_MASK;
	ISP_REG_MWR(idx, ISP_POST_BLC_PARA_G, mask, val);


	return ret;

}

int isp_k_cfg_post_blc(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param is null.\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param is null.\n");
		return -EPERM;
	}

	switch (param->property) {
	case ISP_PRO_POST_BLC_BLOCK:
		ret = isp_k_post_blc_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
