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
#define pr_fmt(fmt) "CONTRAST: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_contrast_block(struct isp_io_param *param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_contrast_info contrast_info;

	memset(&contrast_info, 0x00, sizeof(contrast_info));

	ret = copy_from_user((void *)&contrast_info, param->property_param,
			sizeof(contrast_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}
	ISP_REG_MWR(idx, ISP_CONTRAST_PARAM, BIT_0, contrast_info.bypass);
	if (contrast_info.bypass)
		return 0;

	ISP_REG_MWR(idx, ISP_CONTRAST_PARAM,
			0x1FE, (contrast_info.factor << 1));

	return ret;
}

int isp_k_cfg_contrast(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t com_idx)
{
	int ret = 0;

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
		ret = isp_k_contrast_block(param, com_idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
