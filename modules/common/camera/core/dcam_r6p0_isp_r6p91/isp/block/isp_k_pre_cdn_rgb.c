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

static int32_t isp_k_pre_cdn_rgb_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_pre_cdn_rgb_info pcr_info;

	memset(&pcr_info, 0x00, sizeof(pcr_info));

	ret = copy_from_user((void *)&pcr_info,
		param->property_param, sizeof(pcr_info));
	if (ret != 0) {
		pr_info("copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_PRECNRNEW_CFG, BIT_0, pcr_info.bypass);

	ISP_REG_MWR(ISP_PRECNRNEW_CFG, 0x3 << 1, pcr_info.median_mode << 1);

	val = pcr_info.median_thr & 0xFFFF;
	ISP_REG_WR(ISP_PRECNRNEW_MEDIAN_THR, val);
	val = (pcr_info.thru0 & 0xFFFF)
		| ((pcr_info.thru1 & 0xFFFF) << 16);
	ISP_REG_WR(ISP_PRECNRNEW_THRU, val);
	val = (pcr_info.thrv0 & 0xFFFF)
		| ((pcr_info.thrv1 & 0xFFFF) << 16);
	ISP_REG_WR(ISP_PRECNRNEW_THRV, val);

	return ret;

}

int32_t isp_k_cfg_pre_cdn_rgb(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_pre_cdn_rgb: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_PRE_CDN_RGB_BLOCK:
		ret = isp_k_pre_cdn_rgb_block(param);
		break;
	default:
		pr_info("fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}

