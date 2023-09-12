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
#define pr_fmt(fmt) "CONTRAST: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int isp_k_contrast1_block(struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_contrast_info *contrast_info = NULL;
	if (isp_k_param->contrast_info.isupdate == 0)
		return ret;
	contrast_info = &isp_k_param->contrast_info;
	isp_k_param->contrast_info.isupdate = 0;
	if (g_isp_bypass[idx] & (1 << _EISP_CONTRAST))
		contrast_info->bypass = 1;
	if (contrast_info->bypass)
		return 0;

	ISP_REG_MWR(idx, ISP_CONTRAST_PARAM,
			0x1FE, (contrast_info->factor << 1));

	return ret;
}

int isp_k_cfg_contrast(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_contrast_info *contrast_info = NULL;
	contrast_info = &isp_k_param->contrast_info;
	if (!param) {
		pr_err("fail to get param\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -EPERM;
	}

	switch (param->property) {
	case ISP_PRO_CONTRAST_BLOCK:
		ret = copy_from_user((void *)contrast_info,
			param->property_param,
			sizeof(struct isp_dev_contrast_info));
		if (ret != 0) {
			pr_err("fail to copy from user, ret = %d\n", ret);
			return -EPERM;
		}
		isp_k_param->contrast_info.isupdate = 1;
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}

int isp_k_cpy_contrast1(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if (isp_k_param->contrast_info.isupdate == 1) {
		memcpy(&param_block->contrast_info, &isp_k_param->contrast_info, sizeof(struct isp_dev_contrast_info));
		isp_k_param->contrast_info.isupdate = 0;
		param_block->contrast_info.isupdate = 1;
	}

	return ret;
}

