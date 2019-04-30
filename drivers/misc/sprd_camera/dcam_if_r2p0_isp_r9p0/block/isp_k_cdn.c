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
#define pr_fmt(fmt) "CDN: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_yuv_cdn_block(struct isp_io_param *param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_yuv_cdn_info cdn_info;
	unsigned int val = 0;
	unsigned int i = 0;

	memset(&cdn_info, 0x00, sizeof(cdn_info));

	ret = copy_from_user((void *)&cdn_info, param->property_param,
			sizeof(cdn_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_CDN_PARAM, BIT_0, cdn_info.bypass);
	if (cdn_info.bypass)
		return 0;

	val = ((cdn_info.filter_bypass & 1) << 1) |
		((cdn_info.median_writeback_en & 1) << 2) |
		((cdn_info.median_mode & 0x7) << 3) |
		((cdn_info.gaussian_mode & 0x3) << 6) |
		((cdn_info.median_thr & 0x3FFF) << 8);
	ISP_REG_MWR(idx, ISP_CDN_PARAM, 0x3FFFFE, val);

	val = (cdn_info.median_thru0 & 0x7F) |
		((cdn_info.median_thru1 & 0xFF) << 8) |
		((cdn_info.median_thrv0 & 0x7F) << 16) |
		((cdn_info.median_thrv1 & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_CDN_THRUV, val);

	for (i = 0; i < 7; i++) {
		val = (cdn_info.rangewu[i*4] & 0x3F) |
			((cdn_info.rangewu[i * 4 + 1] & 0x3F) << 8) |
			((cdn_info.rangewu[i * 4 + 2] & 0x3F) << 16) |
			((cdn_info.rangewu[i * 4 + 3] & 0x3F) << 24);
		ISP_REG_WR(idx, ISP_CDN_U_RANWEI_0 + i * 4, val);
	}

	val = (cdn_info.rangewu[28] & 0x3F) |
		((cdn_info.rangewu[29] & 0x3F) << 8) |
		((cdn_info.rangewu[30] & 0x3F) << 16);
	ISP_REG_WR(idx, ISP_CDN_U_RANWEI_7, val);

	for (i = 0; i < 7; i++) {
		val = (cdn_info.rangewv[i*4] & 0x3F) |
			((cdn_info.rangewv[i * 4 + 1] & 0x3F) << 8) |
			((cdn_info.rangewv[i * 4 + 2] & 0x3F) << 16) |
			((cdn_info.rangewv[i * 4 + 3] & 0x3F) << 24);
		ISP_REG_WR(idx, ISP_CDN_V_RANWEI_0 + i * 4, val);
	}

	val = (cdn_info.rangewv[28] & 0x3F) |
		((cdn_info.rangewv[29] & 0x3F) << 8) |
		((cdn_info.rangewv[30] & 0x3F) << 16);
	ISP_REG_WR(idx, ISP_CDN_V_RANWEI_7, val);

	return ret;
}

int isp_k_cfg_yuv_cdn(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t com_idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -EPERM;
	}

	switch (param->property) {
	case ISP_PRO_YUV_CDN_BLOCK:
		ret = isp_k_yuv_cdn_block(param, com_idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
