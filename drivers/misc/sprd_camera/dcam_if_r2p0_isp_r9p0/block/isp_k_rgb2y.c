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
#define pr_fmt(fmt) "RGB2Y:%d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_rgb2y_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_dev_rgb2y_info rgb2y_info;

	memset(&rgb2y_info, 0x00, sizeof(rgb2y_info));

	ret = copy_from_user((void *)&rgb2y_info,
		param->property_param, sizeof(rgb2y_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	DCAM_REG_MWR(idx, ISP_AFL_PARAM0, BIT_1, rgb2y_info.bypass << 1);
	if (rgb2y_info.bypass)
		return 0;

	val = ((rgb2y_info.bayer2y_chanel & 0x3) << 6) |
		(rgb2y_info.bayer2y_mode & 0x3) << 4;
	DCAM_REG_MWR(idx, ISP_AFL_PARAM0, 0xF << 4, val);

	return ret;
}

int isp_k_cfg_rgb2y(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t com_idx)
{
	int ret = 0;
	enum isp_id idx = ISP_ID_0;

	if (!param) {
		pr_err("fail to get param\n");
		return -1;
	}
	if (!param->property_param) {
		pr_err("fail to get property_param\n");
		return -1;
	}

	idx = ISP_GET_ISP_ID(com_idx);
	switch (param->property) {
	case ISP_PRO_RGB2Y_BLOCK:
		ret = isp_k_rgb2y_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
