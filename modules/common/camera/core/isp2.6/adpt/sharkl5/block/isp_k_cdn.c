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

#include "isp_hw.h"
#include "isp_reg.h"
#include "cam_types.h"
#include "cam_block.h"
#include "cam_queue.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CDN: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int isp_k_cdn_block(struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	int i = 0;
	uint32_t val = 0;
	struct isp_dev_cdn_info *cdn_info = NULL;

	if (isp_k_param->cdn_info.isupdate == 0)
		return ret;
	cdn_info = &isp_k_param->cdn_info;
	isp_k_param->cdn_info.isupdate = 0;

	if (g_isp_bypass[idx] & (1 << _EISP_CDN))
		cdn_info->bypass = 1;
	if (cdn_info->bypass)
		return 0;

	val = ((cdn_info->filter_bypass & 1) << 1) |
		((cdn_info->median_writeback_en & 1) << 2) |
		((cdn_info->median_mode & 0x7) << 3) |
		((cdn_info->gaussian_mode & 0x3) << 6) |
		((cdn_info->median_thr & 0x3FFF) << 8);
	ISP_REG_MWR(idx, ISP_CDN_PARAM, 0x3FFFFE, val);

	val = (cdn_info->median_thru0 & 0x7F) |
		((cdn_info->median_thru1 & 0xFF) << 8) |
		((cdn_info->median_thrv0 & 0x7F) << 16) |
		((cdn_info->median_thrv1 & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_CDN_THRUV, val);

	for (i = 0; i < 7; i++) {
		val = (cdn_info->rangewu[i * 4] & 0x3F) |
			((cdn_info->rangewu[i * 4 + 1] & 0x3F) << 8) |
			((cdn_info->rangewu[i * 4 + 2] & 0x3F) << 16) |
			((cdn_info->rangewu[i * 4 + 3] & 0x3F) << 24);
		ISP_REG_WR(idx, ISP_CDN_U_RANWEI_0 + i * 4, val);
	}

	val = (cdn_info->rangewu[28] & 0x3F) |
		((cdn_info->rangewu[29] & 0x3F) << 8) |
		((cdn_info->rangewu[30] & 0x3F) << 16);
	ISP_REG_WR(idx, ISP_CDN_U_RANWEI_7, val);

	for (i = 0; i < 7; i++) {
		val = (cdn_info->rangewv[i * 4] & 0x3F) |
			((cdn_info->rangewv[i * 4 + 1] & 0x3F) << 8) |
			((cdn_info->rangewv[i * 4 + 2] & 0x3F) << 16) |
			((cdn_info->rangewv[i * 4 + 3] & 0x3F) << 24);
		ISP_REG_WR(idx, ISP_CDN_V_RANWEI_0 + i * 4, val);
	}

	val = (cdn_info->rangewv[28] & 0x3F) |
		((cdn_info->rangewv[29] & 0x3F) << 8) |
		((cdn_info->rangewv[30] & 0x3F) << 16);
	ISP_REG_WR(idx, ISP_CDN_V_RANWEI_7, val);

	return ret;
}

int isp_k_cfg_cdn(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;

	struct isp_dev_cdn_info *cdn_info = NULL;

	cdn_info = &isp_k_param->cdn_info;

	switch (param->property) {
	case ISP_PRO_CDN_BLOCK:
		ret = copy_from_user((void *)cdn_info, param->property_param, sizeof(struct isp_dev_cdn_info));
		if (ret != 0) {
			pr_err("fail to copy from user, ret = %d\n", ret);
			return ret;
		}
		isp_k_param->cdn_info.isupdate = 1;
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}

int isp_k_cpy_cdn(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if (isp_k_param->cdn_info.isupdate == 1) {
		memcpy(&param_block->cdn_info, &isp_k_param->cdn_info, sizeof(struct isp_dev_cdn_info));
		isp_k_param->cdn_info.isupdate = 0;
		param_block->cdn_info.isupdate = 1;
	}

	return ret;
}

