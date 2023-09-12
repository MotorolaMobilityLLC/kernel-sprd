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
#define pr_fmt(fmt) "RGB_GAIN: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

#define ISP_RGBG_PARAM_GGAIN_MASK	(0xFFFF0000)
#define ISP_RGBG_G_MASK		(0xFFFF)

#define ISP_RGBG_PARAM0_SEED_MASK	(0xFFFFFF00)
#define ISP_RGBG_PARAM0_RBYPASS_MASK	(BIT_0)

#define ISP_RGBG_PARAM1_RSHIFT_MASK	(0xF)
#define ISP_RGBG_PARAM1_RANGE_MASK	(0x70)
#define ISP_RGBG_PARAM1_ROFFSET_MASK	(0x7FF0000)

static int isp_k_rgb_gain_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_dev_rgb_gain_info gain_info;

	memset(&gain_info, 0x00, sizeof(gain_info));
	ret = copy_from_user((void *)&gain_info,
		param->property_param, sizeof(gain_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	ISP_REG_MWR(idx, ISP_RGBG_PARAM, BIT_0, gain_info.bypass);
	if (gain_info.bypass) {
		isp_dbg_s_ori_byp(SBLK_BYPASS, raw_rgbg, idx);
		return 0;
	}

	if (!gain_info.bypass) {
		val = (gain_info.global_gain & 0xFFFF) << 16;
		ISP_REG_MWR(idx, ISP_RGBG_PARAM,
			ISP_RGBG_PARAM_GGAIN_MASK, val);

		val = ((gain_info.r_gain & 0xFFFF) << 16)
			| (gain_info.b_gain & 0xFFFF);
		ISP_REG_WR(idx, ISP_RGBG_RB, val);
		val = gain_info.g_gain & 0xFFFF;
		ISP_REG_MWR(idx, ISP_RGBG_G, ISP_RGBG_G_MASK, val);
	}

	return ret;
}

int isp_k_cfg_rgb_gain(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_RGB_GAIN_BLOCK:
		ret = isp_k_rgb_gain_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
