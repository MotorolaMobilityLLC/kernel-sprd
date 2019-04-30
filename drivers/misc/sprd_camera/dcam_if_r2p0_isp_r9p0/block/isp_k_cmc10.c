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
#define pr_fmt(fmt) "CMC10: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_cmc10_block(struct isp_io_param *param, uint32_t idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_dev_cmc10_info cmc10_info;

	memset(&cmc10_info, 0x00, sizeof(cmc10_info));

	ret = copy_from_user((void *)&cmc10_info, param->property_param,
			sizeof(cmc10_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_CMC10_PARAM, BIT_0, cmc10_info.bypass);
	if (cmc10_info.bypass)
		return 0;

	val = ((cmc10_info.matrix.val[1] & 0x3FFF) << 14) |
		(cmc10_info.matrix.val[0] & 0x3FFF);
	ISP_REG_WR(idx, ISP_CMC10_MATRIX0, val);

	val = ((cmc10_info.matrix.val[3] & 0x3FFF) << 14) |
		(cmc10_info.matrix.val[2] & 0x3FFF);
	ISP_REG_WR(idx, (ISP_CMC10_MATRIX0 + 4), val);

	val = ((cmc10_info.matrix.val[5] & 0x3FFF) << 14) |
		(cmc10_info.matrix.val[4] & 0x3FFF);
	ISP_REG_WR(idx, (ISP_CMC10_MATRIX0 + 8), val);

	val = ((cmc10_info.matrix.val[7] & 0x3FFF) << 14) |
		(cmc10_info.matrix.val[6] & 0x3FFF);
	ISP_REG_WR(idx, (ISP_CMC10_MATRIX0 + 12), val);

	val = cmc10_info.matrix.val[8] & 0x3FFF;
	ISP_REG_WR(idx, (ISP_CMC10_MATRIX0 + 16), val);

	return ret;
}

int isp_k_cfg_cmc10(struct isp_io_param *param,
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
	case ISP_PRO_CMC_BLOCK:
		ret = isp_k_cmc10_block(param, com_idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
