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

#include "sprd_mm.h"
#include "sprd_isp_hw.h"
#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "AWBC: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

static int isp_k_awb_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_dev_awb_info awb_info;

	memset(&awb_info, 0x00, sizeof(awb_info));

	ret = copy_from_user((void *)&awb_info, param->property_param,
			sizeof(awb_info));
	if (ret != 0) {
		pr_err("fail to reading params, ret=%d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_AWBC_PARAM, BIT_0, awb_info.awbc_bypass);
	if (awb_info.awbc_bypass) {
		isp_dbg_s_ori_byp(SBLK_BYPASS, raw_awb, idx);
		pr_info("awbc_bypass: %d\n", awb_info.awbc_bypass);
		return 0;
	}

	val = ((awb_info.gain.b & 0x3FFF) << 16) |
		(awb_info.gain.r & 0x3FFF);
	ISP_REG_WR(idx, ISP_AWBC_GAIN0, val);

	val = ((awb_info.gain.gb & 0x3FFF) << 16) |
		(awb_info.gain.gr & 0x3FFF);
	ISP_REG_WR(idx, ISP_AWBC_GAIN1, val);

	val = ((awb_info.thrd.b & 0x3FF) << 20) |
		((awb_info.thrd.g & 0x3FF) << 10) |
		(awb_info.thrd.r & 0x3FF);
	ISP_REG_WR(idx, ISP_AWBC_THRD, val);

	val = ((awb_info.gain_offset.b & 0xFFFF) << 16) |
		(awb_info.gain_offset.r & 0xFFFF);
	ISP_REG_WR(idx, ISP_AWBC_OFFSET0, val);

	val = ((awb_info.gain_offset.gb & 0xFFFF) << 16) |
		(awb_info.gain_offset.gr & 0xFFFF);
	ISP_REG_WR(idx, ISP_AWBC_OFFSET1, val);

	return ret;
}

static int isp_k_awbc_gain(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_awbc_rgb awbc_gain;

	memset(&awbc_gain, 0x00, sizeof(awbc_gain));
	ret = copy_from_user(&awbc_gain,
			param->property_param, sizeof(awbc_gain));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return ret;
	}
	val = ((awbc_gain.b & 0x3FFF) << 16) | (awbc_gain.r & 0x3FFF);
	ISP_REG_WR(idx, ISP_AWBC_GAIN0, val);

	val = ((awbc_gain.g & 0x3FFF) << 16) |
		(awbc_gain.g & 0x3FFF);
	ISP_REG_WR(idx, ISP_AWBC_GAIN1, val);

	return ret;
}

int isp_k_cfg_awb(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param.\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property param.\n");
		return -EPERM;
	}

	switch (param->property) {
	case ISP_PRO_AWB_BLOCK:
		ret = isp_k_awb_block(param, idx);
		break;
	case ISP_PRO_AWBC_GAIN:
		ret = isp_k_awbc_gain(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
