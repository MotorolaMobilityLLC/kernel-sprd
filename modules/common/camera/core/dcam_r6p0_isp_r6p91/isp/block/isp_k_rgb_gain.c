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

static int32_t isp_k_rgb_gain_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_rgb_gain_info gain_info;

	ret = copy_from_user((void *)&gain_info,
		param->property_param, sizeof(gain_info));
	if (ret != 0) {
		pr_info("copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (gain_info.global_gain & 0xFFFF) << 16;
	ISP_REG_MWR(ISP_RGBG_PARAM, 0xFFFF0000, val);

	val = ((gain_info.r_gain & 0xFFFF) << 16)
		| (gain_info.b_gain & 0xFFFF);
	ISP_REG_WR(ISP_RGBG_RB, val);
	val = gain_info.g_gain & 0xFFFF;
	ISP_REG_WR(ISP_RGBG_G, val);

	ISP_REG_MWR(ISP_RGBG_PARAM, BIT_0, gain_info.bypass);

	return ret;

}

int32_t isp_k_cfg_rgb_gain(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_rgb_gain: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_rgb_gain: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_RGB_GAIN_BLOCK:
		ret = isp_k_rgb_gain_block(param);
		break;
	default:
		pr_info("fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
