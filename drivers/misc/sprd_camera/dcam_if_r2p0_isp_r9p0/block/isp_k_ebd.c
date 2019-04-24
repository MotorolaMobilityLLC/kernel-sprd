/*
 * Copyright (C) 2017-2018 Spreadtrum Communications Inc.
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
#include "isp_block.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "EBD: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_ebd_block(struct isp_io_param *param, enum dcam_id idx)
{
	DCAM_REG_MWR(idx, DCAM_VCH3_CONTROL, BIT_0, 1);
	return 0;
}

int isp_k_cfg_ebd(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t com_idx)
{
	int ret = 0;
	enum isp_id idx;
	enum dcam_id id = ISP_ID_0;

	if (!param) {
		pr_err("fail to get param\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -1;
	}

	idx = ISP_GET_ISP_ID(com_idx);
	id = (enum dcam_id)idx;
	switch (param->property) {
	case ISP_PRO_EBD_BLOCK:
		ret = isp_k_ebd_block(param, id);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
