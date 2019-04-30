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
#define pr_fmt(fmt) "BCHS: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_bchs_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	struct isp_dev_bchs_info_v1 bchs_info;

	memset(&bchs_info, 0x00, sizeof(bchs_info));

	ret = copy_from_user((void *)&bchs_info,
		param->property_param, sizeof(bchs_info));
	if (ret != 0) {
		pr_err("fail to copy_from_user, ret = 0x%x\n",
			(unsigned int)ret);
		return -1;
	}
	ISP_REG_MWR(idx, ISP_BCHS_PARAM, BIT_0, bchs_info.bchs_bypass);
	if (bchs_info.bchs_bypass)
		return 0;

	ISP_REG_MWR(idx, ISP_BCHS_PARAM, 0xffffffff,
		(bchs_info.cnta_en<<4)|
		(bchs_info.brta_en<<3)|
		(bchs_info.hua_en<<2)|
		(bchs_info.csa_en<<1)|
		(bchs_info.bchs_bypass<<0));

	ISP_REG_MWR(idx, ISP_CSA_FACTOR, 0xffffffff,
		(bchs_info.csa_factor_u<<8)|
		(bchs_info.csa_factor_v<<0));

	ISP_REG_MWR(idx, ISP_HUA_FACTOR, 0xffffffff,
		((bchs_info.hua_cos_value&0x1ff)<<16)|
		((bchs_info.hua_sina_value&0x1ff)<<0));
	ISP_REG_MWR(idx, ISP_BRTA_FACTOR, 0xffffffff,
		(bchs_info.brta_factor<<0));

	ISP_REG_MWR(idx, ISP_CNTA_FACTOR, 0xffffffff,
		(bchs_info.cnta_factor<<0));

	return ret;
}

static int isp_k_bchs_chk_sum_clr(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;

	ret = copy_from_user((void *)&val, param->property_param,
			sizeof(unsigned int));
	if (ret != 0) {
		pr_err("fail to check param 0x%x\n", (unsigned int)ret);
		return -1;
	}

	if (val)
		ISP_REG_OWR(idx, ISP_BCHS_PARAM, BIT_5);
	else
		ISP_REG_MWR(idx, ISP_BCHS_PARAM, BIT_5, 0);

	return ret;
}

int isp_k_cfg_bchs(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to check param.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_err("fail to check param\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_BCHS_BLOCK:
		ret = isp_k_bchs_block(param, idx);
		break;
	case ISP_PRO_BCHS_CHK_SUM_CLR:
		ret = isp_k_bchs_chk_sum_clr(param, idx);
		break;
	default:
		pr_err("fail to get valid bchs:cmd id:%d.\n", param->property);
		break;
	}

	return ret;
}

