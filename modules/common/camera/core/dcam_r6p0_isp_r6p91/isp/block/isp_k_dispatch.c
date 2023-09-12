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

static int32_t isp_k_dispatch_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_dispatch_info_v1 dispatch_info;

	memset(&dispatch_info, 0x00, sizeof(dispatch_info));

	ret = copy_from_user((void *)&dispatch_info,
		param->property_param, sizeof(dispatch_info));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_WR(ISP_DISPATCH_CH0_BAYER, dispatch_info.bayer_ch0 & 0x3);
	/*ISP_REG_WR(ISP_DISPATCH_CH1_BAYER, dispatch_info.bayer_ch1 & 0x3);*/

	return ret;
}

static int32_t isp_k_dispatch_ch0_bayer(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;

	ret = copy_from_user((void *)&val,
		param->property_param, sizeof(uint32_t));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_WR(ISP_DISPATCH_CH0_BAYER, val & 0x3);

	return ret;
}

static int32_t isp_k_dispatch_ch1_bayer(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;

	ret = copy_from_user((void *)&val,
		param->property_param, sizeof(uint32_t));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	/*ISP_REG_WR(ISP_DISPATCH_CH1_BAYER, val & 0x3);*/

	return ret;
}

static int32_t isp_k_dispatch_ch0_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size = {0};

	ret = copy_from_user((void *)&size,
		param->property_param, sizeof(size));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0xFFFF) << 16)
		| (size.width & 0xFFFF);
	ISP_REG_WR(ISP_DISPATCH_CH0_SIZE, val);
	/* add dispatch blinking when sensor output small resolution*/
	if (size.width <= ISP_LOW_CLOCK_WIDTH)
		ISP_REG_MWR(ISP_DISPATCH_DLY, 0xff, 0xff);
	else
		ISP_REG_MWR(ISP_DISPATCH_DLY, 0xff, 0x22);

	return ret;
}

static int32_t isp_k_dispatch_ch1_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size = {0};

	ret = copy_from_user((void *)&size,
		param->property_param, sizeof(size));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0xFFFF) << 16)
		| (size.width & 0xFFFF);
	/*ISP_REG_WR(ISP_DISPATCH_CH1_SIZE, val);*/

	return ret;
}

int32_t isp_k_cfg_dispatch(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_dispatch: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_dispatch: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_DISPATCH_BLOCK:
		ret = isp_k_dispatch_block(param);
		break;
	case ISP_PRO_DISPATCH_CH0_BAYER:
		ret = isp_k_dispatch_ch0_bayer(param);
		break;
	case ISP_PRO_DISPATCH_CH1_BAYER:
		ret = isp_k_dispatch_ch1_bayer(param);
		break;
	case ISP_PRO_DISPATCH_CH0_SIZE:
		ret = isp_k_dispatch_ch0_size(param);
		break;
	case ISP_PRO_DISPATCH_CH1_SIZE:
		ret = isp_k_dispatch_ch1_size(param);
		break;
	default:
		pr_info("fail cmd id:%d,not supported.\n", param->property);
		break;
	}

	return ret;
}

