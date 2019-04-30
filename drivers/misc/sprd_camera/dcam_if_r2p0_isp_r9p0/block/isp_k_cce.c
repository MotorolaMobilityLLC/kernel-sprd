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
#define pr_fmt(fmt) "CCE: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_cce_block_matrix
	(struct isp_io_param *param, uint32_t idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_dev_cce_info cce_info;

	memset(&cce_info, 0x00, sizeof(cce_info));

	ret = copy_from_user((void *)&cce_info, param->property_param,
			sizeof(cce_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_CCE_PARAM, BIT_0, cce_info.bypass);
	if (cce_info.bypass)
		return 0;

	val = ((cce_info.matrix[1] & 0x7FF) << 11) |
		(cce_info.matrix[0] & 0x7FF);
	ISP_REG_WR(idx, ISP_CCE_MATRIX0, val);

	val = ((cce_info.matrix[3] & 0x7FF) << 11) |
		(cce_info.matrix[2] & 0x7FF);
	ISP_REG_WR(idx, ISP_CCE_MATRIX1, val);

	val = ((cce_info.matrix[5] & 0x7FF) << 11) |
		(cce_info.matrix[4] & 0x7FF);
	ISP_REG_WR(idx, ISP_CCE_MATRIX2, val);

	val = ((cce_info.matrix[7] & 0x7FF) << 11) |
		(cce_info.matrix[6] & 0x7FF);
	ISP_REG_WR(idx, ISP_CCE_MATRIX3, val);

	val = cce_info.matrix[8] & 0x7FF;
	ISP_REG_WR(idx, ISP_CCE_MATRIX4, val);

	val = (cce_info.y_offset & 0x1FF)
		| ((cce_info.u_offset & 0x1FF) << 9)
		| ((cce_info.v_offset & 0x1FF) << 18);
	ISP_REG_WR(idx, ISP_CCE_SHIFT, (val & 0x7FFFFFF));

	return ret;
}

int isp_k_cfg_cce(struct isp_io_param *param,
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
	case ISP_PRO_CCE_BLOCK_MATRIX:
		ret = isp_k_cce_block_matrix(param, com_idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
