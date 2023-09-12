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

static int32_t isp_k_cmc10_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_cmc10_info cmc10_info;

	memset(&cmc10_info, 0x00, sizeof(cmc10_info));

	ret = copy_from_user((void *)&cmc10_info,
		param->property_param, sizeof(cmc10_info));
	if (ret != 0) {
		pr_info("copy error,ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_CMC10_PARAM, BIT_0, cmc10_info.bypass);

	val = ((cmc10_info.matrix.val[1] & 0x3FFF) << 14)
		| (cmc10_info.matrix.val[0] & 0x3FFF);
	ISP_REG_WR(ISP_CMC10_MATRIX0, val);
	val = ((cmc10_info.matrix.val[3] & 0x3FFF) << 14)
		| (cmc10_info.matrix.val[2] & 0x3FFF);
	ISP_REG_WR((ISP_CMC10_MATRIX0 + 4), val);
	val = ((cmc10_info.matrix.val[5] & 0x3FFF) << 14)
		| (cmc10_info.matrix.val[4] & 0x3FFF);
	ISP_REG_WR((ISP_CMC10_MATRIX0 + 8), val);
	val = ((cmc10_info.matrix.val[7] & 0x3FFF) << 14)
		| (cmc10_info.matrix.val[6] & 0x3FFF);
	ISP_REG_WR((ISP_CMC10_MATRIX0 + 12), val);
	val = cmc10_info.matrix.val[8] & 0x3FFF;
	ISP_REG_WR((ISP_CMC10_MATRIX0 + 16), val);

	return ret;
}

int32_t isp_k_cfg_cmc10(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_cmc: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_cmc: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_CMC_BLOCK:
		ret = isp_k_cmc10_block(param);
		break;
	default:
		pr_info("fail cmd id:%d,not supported.\n", param->property);
		break;
	}

	return ret;
}

