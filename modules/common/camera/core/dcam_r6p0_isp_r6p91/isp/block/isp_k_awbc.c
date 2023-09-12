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
#include "isp_block.h"

static int isp_k_awbc_block(struct isp_io_param *param)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_dev_awb_info awb_info;

	memset(&awb_info, 0x00, sizeof(awb_info));

	ret = copy_from_user((void *)&awb_info, param->property_param,
			sizeof(awb_info));
	if (ret != 0) {
		pr_err("awbm_bypass: copy error, ret=0x%x\n",
						(unsigned int)ret);
		return -EPERM;
	}

	ISP_REG_MWR(ISP_AWBC_PARAM, BIT_0, awb_info.awbc_bypass);
	if (awb_info.awbc_bypass)
		return 0;

	val = ((awb_info.gain.b & 0x3FFF) << 16) | (awb_info.gain.r & 0x3FFF);
	ISP_REG_WR(ISP_AWBC_GAIN0, val);

	val = ((awb_info.gain.gb & 0x3FFF) << 16) |
		(awb_info.gain.gr & 0x3FFF);
	ISP_REG_WR(ISP_AWBC_GAIN1, val);

	val = ((awb_info.thrd.b & 0x3FF) << 20) |
		((awb_info.thrd.g & 0x3FF) << 10) |
		(awb_info.thrd.r & 0x3FF);
	ISP_REG_WR(ISP_AWBC_THRD, val);

	val = ((awb_info.gain_offset.b & 0xFFFF) << 16) |
		(awb_info.gain_offset.r & 0xFFFF);
	ISP_REG_WR(ISP_AWBC_OFFSET0, val);

	val = ((awb_info.gain_offset.gb & 0xFFFF) << 16) |
		(awb_info.gain_offset.gr & 0xFFFF);
	ISP_REG_WR(ISP_AWBC_OFFSET1, val);

	return ret;
}

static int isp_k_awbc_gain(struct isp_io_param *param)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_awbc_rgb awbc_gain;

	memset(&awbc_gain, 0x00, sizeof(awbc_gain));

	ret = copy_from_user(&awbc_gain,
		param->property_param, sizeof(awbc_gain));
	if (ret != 0) {
		pr_err("isp_k_awbc_gain: copy_from_user error, ret = %d\n",
			ret);
		return -EFAULT;
	}
	val = ((awbc_gain.b & 0x3FFF) << 16) | (awbc_gain.r & 0x3FFF);
	ISP_REG_WR(ISP_AWBC_GAIN0, val);

	val = ((awbc_gain.g & 0x3FFF) << 16) |
		(awbc_gain.g & 0x3FFF);
	ISP_REG_WR(ISP_AWBC_GAIN1, val);

	return ret;
}

int32_t isp_k_cfg_awbc(struct isp_io_param *param)
{
	int ret = 0;

	if (!param) {
		pr_err("isp_k_cfg_awbc: param is null error.\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("isp_k_cfg_awbc: property_param is null error.\n");
		return -EPERM;
	}

	switch (param->property) {
	case ISP_PRO_AWB_BLOCK:
		ret = isp_k_awbc_block(param);
		break;
	case ISP_PRO_AWBC_GAIN:
		ret = isp_k_awbc_gain(param);
		break;
	default:
		pr_err("awb:cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
