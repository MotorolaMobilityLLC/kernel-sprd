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
#define pr_fmt(fmt) "ARBITER: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_arbiter_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	struct isp_dev_arbiter_info arbiter_info;
	unsigned int val = 0;

	memset(&arbiter_info, 0x00, sizeof(arbiter_info));

	ret = copy_from_user((void *)&arbiter_info, param->property_param,
			sizeof(arbiter_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	val = ((arbiter_info.fetch_bit_reorder & 0x1) << 2)
		| (arbiter_info.fetch_raw_endian & 0x3)
		| ((arbiter_info.fetch_raw_word_change & 1) << 3);
	ISP_HREG_WR(idx, ISP_ARBITER_ENDIAN_CH0, val);

	return ret;
}

int isp_k_cfg_arbiter(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t com_idx)
{
	int ret = 0;
	enum isp_id idx = ISP_ID_0;

	if (!param) {
		pr_err("fail to get param\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -EPERM;
	}

	idx = ISP_GET_ISP_ID(com_idx);
	switch (param->property) {
	case ISP_PRO_ARBITER_BLOCK:
		ret = isp_k_arbiter_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
