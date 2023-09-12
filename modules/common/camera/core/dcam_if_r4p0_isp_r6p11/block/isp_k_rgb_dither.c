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
#define pr_fmt(fmt) "RGB_DITHER: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

#define ISP_RGBG_PARAM1_RSHIFT_MASK	(0xF)
#define ISP_RGBG_PARAM1_RANGE_MASK	(0x70)
#define ISP_RGBG_PARAM1_ROFFSET_MASK	(0x7FF0000)

#define ISP_RGBG_PARAM0_SEED_MASK	(0xFFFFFF00)
#define ISP_RGBG_PARAM0_RBYPASS_MASK	(BIT_0)

static int isp_k_rgb_dither_random_block
		(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int mask = 0;
	struct isp_dev_rgb_dither_info rgb_gain;

	memset(&rgb_gain, 0x00, sizeof(rgb_gain));
	ret = copy_from_user((void *)&rgb_gain,
		param->property_param, sizeof(struct isp_dev_rgb_dither_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	ISP_REG_MWR(idx, ISP_RGBG_PARAM0, BIT_0, rgb_gain.random_bypass);
	if (rgb_gain.random_bypass) {
		isp_dbg_s_ori_byp(SBLK_BYPASS, raw_rgbd, idx);
		return 0;
	}

	val = ((rgb_gain.seed & 0xFFFFFF) << 8)
		| ((rgb_gain.random_mode & 0x1) << 2);
	mask = ISP_RGBG_PARAM0_SEED_MASK | ISP_RGBG_PARAM0_RBYPASS_MASK;
	ISP_REG_MWR(idx, ISP_RGBG_PARAM0, mask, val);

	val = (rgb_gain.r_shift & 0xF)
		| ((rgb_gain.range & 0x7) << 4)
		| ((rgb_gain.r_offset & 0x7FF) << 16);
	mask = ISP_RGBG_PARAM1_RSHIFT_MASK | ISP_RGBG_PARAM1_RANGE_MASK
		| ISP_RGBG_PARAM1_ROFFSET_MASK;
	ISP_REG_MWR(idx, ISP_RGBG_PARAM1, mask, val);

	val = (rgb_gain.takebit[0] & 0xF)
		| ((rgb_gain.takebit[1] & 0xF) << 4)
		| ((rgb_gain.takebit[2] & 0xF) << 8)
		| ((rgb_gain.takebit[3] & 0xF) << 12)
		| ((rgb_gain.takebit[4] & 0xF) << 16)
		| ((rgb_gain.takebit[5] & 0xF) << 20)
		| ((rgb_gain.takebit[6] & 0xF) << 24)
		| ((rgb_gain.takebit[7] & 0xF) << 28);
	ISP_REG_WR(idx, ISP_RGBG_PARAM2, val);

	return ret;
}

int isp_k_cfg_rgb_dither(struct isp_io_param *param,
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
	case ISP_PRO_RGB_EDITHER_RANDOM_BLOCK:
		ret = isp_k_rgb_dither_random_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
