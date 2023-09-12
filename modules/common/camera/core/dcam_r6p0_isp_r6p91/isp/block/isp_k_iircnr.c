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

static int32_t isp_k_iircnr_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_iircnr_info iircnr_info;
	uint32_t val = 0;

	memset(&iircnr_info, 0x00, sizeof(iircnr_info));

	ret = copy_from_user((void *)&iircnr_info,
		param->property_param, sizeof(iircnr_info));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (iircnr_info.mode)
		ISP_REG_OWR(ISP_IIRCNR_PARAM1, BIT_0);
	else
		ISP_REG_MWR(ISP_IIRCNR_PARAM1, BIT_0, 0);

	val = ((iircnr_info.y_th & 0xFF) << 12)
		| ((iircnr_info.uv_th & 0x7FF) << 1);
	ISP_REG_MWR(ISP_IIRCNR_PARAM1, 0xFFFFE, val);

	val = ((iircnr_info.uv_pg_th & 0x3FFF) << 10)
		| (iircnr_info.uv_dist & 0x3FF);
	ISP_REG_WR(ISP_IIRCNR_PARAM2, val);

	val = ((iircnr_info.uv_low_thr2 & 0x1FFFF) << 14)
		| (iircnr_info.uv_low_thr1 & 0x3FFF);
	ISP_REG_WR(ISP_IIRCNR_PARAM3, val);

	val = iircnr_info.uv_s_th & 0xFF;
	ISP_REG_MWR(ISP_IIRCNR_PARAM6, 0xFF, val);

	val = (iircnr_info.alpha_low_v & 0x3FFF)
		| ((iircnr_info.alpha_low_u & 0x3FFF) << 14);
	ISP_REG_WR(ISP_IIRCNR_PARAM7, val);

	val = iircnr_info.uv_high_thr2 & 0x1FFFF;
	ISP_REG_WR(ISP_IIRCNR_PARAM9, val);

	val = iircnr_info.ymd_u & 0x3FFFFFFF;
	ISP_REG_WR(ISP_IIRCNR_PARAM4, val);

	val = iircnr_info.ymd_v & 0x3FFFFFFF;
	ISP_REG_WR(ISP_IIRCNR_PARAM5, val);

	ISP_REG_MWR(ISP_IIRCNR_PARAM6, 0xFF00, iircnr_info.slope << 8);
	ISP_REG_WR(ISP_IIRCNR_PARAM8, iircnr_info.factor & 0x1FFFFFF);


	if (iircnr_info.bypass)
		ISP_REG_OWR(ISP_IIRCNR_PARAM, BIT_0);
	else
		ISP_REG_MWR(ISP_IIRCNR_PARAM, BIT_0, 0);

	return ret;
}

static int32_t isp_k_yrandom_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_yrandom_info_v1 yrandom_info;
	uint32_t val = 0;

	memset(&yrandom_info, 0x00, sizeof(yrandom_info));

	ret = copy_from_user((void *)&yrandom_info,
		param->property_param, sizeof(yrandom_info));
	if (ret != 0) {
		pr_err("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_YRANDOM_PARAM1, 0xFFFFFF00, yrandom_info.seed << 8);
	ISP_REG_MWR(ISP_YRANDOM_PARAM1, BIT_1, yrandom_info.mode << 1);
	/*ISP_REG_MWR(ISP_YRANDOM_INIT, BIT_0, yrandom_info.init);*/
	ISP_REG_MWR(ISP_YRANDOM_PARAM2, 0x7FF0000, yrandom_info.offset << 16);
	ISP_REG_MWR(ISP_YRANDOM_PARAM2, 0xF, yrandom_info.shift);

	val = (yrandom_info.takeBit[0] & 0xF)
		| ((yrandom_info.takeBit[1] & 0xF) << 4)
		| ((yrandom_info.takeBit[2] & 0xF) << 8)
		| ((yrandom_info.takeBit[3] & 0xF) << 12)
		| ((yrandom_info.takeBit[4] & 0xF) << 16)
		| ((yrandom_info.takeBit[5] & 0xF) << 20)
		| ((yrandom_info.takeBit[6] & 0xF) << 24)
		| ((yrandom_info.takeBit[7] & 0xF) << 28);
	ISP_REG_WR(ISP_YRANDOM_PARAM3, val);

	ISP_REG_MWR(ISP_YRANDOM_PARAM1, BIT_0, yrandom_info.bypass);

	if (yrandom_info.bypass == 0)
		ISP_REG_MWR(ISP_YRANDOM_INIT, BIT_0, 1);

	pr_debug("yrandom init %d, bypass %d\n",
		yrandom_info.init, yrandom_info.bypass);

	return ret;
}

int32_t isp_k_cfg_iircnr(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_iircnr: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_iircnr: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_IIRCNR_BLOCK:
		ret = isp_k_iircnr_block(param);
		break;
	case ISP_PRO_YRANDOM_BLOCK:
		ret = isp_k_yrandom_block(param);
		break;
	default:
		pr_info("fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
