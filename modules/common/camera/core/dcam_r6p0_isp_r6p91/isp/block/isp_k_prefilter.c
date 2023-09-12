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
#include "sprd_isp_r6p91.h"
#include "isp_reg.h"

static int32_t isp_k_prefilter_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_prefilter_info_v1 prefilter_info;

	memset(&prefilter_info, 0x00, sizeof(prefilter_info));

	ret = copy_from_user((void *)&prefilter_info,
		param->property_param, sizeof(prefilter_info));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (prefilter_info.writeback)
		ISP_REG_OWR(ISP_PREF_PARAM, BIT_1);
	else
		ISP_REG_MWR(ISP_PREF_PARAM, BIT_1, 0);

	ISP_REG_WR(ISP_PREF_THRD, prefilter_info.thrd & 0xFF);

	if (prefilter_info.bypass)
		ISP_REG_OWR(ISP_PREF_PARAM, BIT_0);
	else
		ISP_REG_MWR(ISP_PREF_PARAM, BIT_0, 0);
	return ret;
}

int32_t isp_k_cfg_prefilter(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_prefilter: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_prefilter: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_PREF_BLOCK:
		ret = isp_k_prefilter_block(param);
		break;
	default:
		pr_info("fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
