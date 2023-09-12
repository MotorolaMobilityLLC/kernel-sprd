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
#define pr_fmt(fmt) "PSTRZ: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

static int isp_k_posterize_block(struct isp_io_param *param,
				enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int i = 0;
	struct isp_dev_posterize_info pstrz_info;

	memset(&pstrz_info, 0x00, sizeof(pstrz_info));

	ret = copy_from_user((void *)&pstrz_info,
		param->property_param, sizeof(pstrz_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_PSTRZ_PARAM, BIT_0, pstrz_info.bypass);
	if (pstrz_info.bypass) {
		pr_debug("bypass.\n");
		isp_dbg_s_ori_byp(SBLK_BYPASS, full_pstrz, idx);
		return 0;
	}

	for (i = 0; i < POSTERIZE_NUM; i++) {
		val = (pstrz_info.posterize_level_out[i] & 0xFF) |
			  ((pstrz_info.posterize_level_top[i] &	0xFF) << 8) |
			  ((pstrz_info.posterize_level_bottom[i] & 0xFF) << 16);
		ISP_REG_WR(idx, ISP_PSTRZ_LEVEL0 + i * 4, val);
	}

	return ret;

}

int isp_k_cfg_pstrz(struct isp_io_param *param,
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
	case ISP_PRO_POSTERIZE_BLOCK:
		ret = isp_k_posterize_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
				param->property);
		break;
	}

	return ret;
}
