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
#define pr_fmt(fmt) "CSA: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_csa_block(struct isp_io_param *param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_csa_info csa_info;
	unsigned int val = 0;

	memset(&csa_info, 0x00, sizeof(csa_info));

	ret = copy_from_user((void *)&csa_info, param->property_param,
			sizeof(csa_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_CSA_PARAM, BIT_0, csa_info.bypass);
	if (csa_info.bypass)
		return 0;

	val = ((csa_info.factor_v & 0xFF) << 8) |
		((csa_info.factor_u & 0xFF) << 16);
	ISP_REG_MWR(idx, ISP_CSA_PARAM, 0xFFFF00, val);

	return ret;
}

int isp_k_cfg_csa(struct isp_io_param *param,
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
	case ISP_PRO_CSA_BLOCK:
		ret = isp_k_csa_block(param, com_idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
