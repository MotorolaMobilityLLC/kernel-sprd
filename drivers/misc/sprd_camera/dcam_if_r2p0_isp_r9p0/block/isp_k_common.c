/*
 * Copyright (C) 2017-2018 Spreadtrum Communications Inc.
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
#define pr_fmt(fmt) "COMMON: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_common_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	struct isp_dev_common_info common_info;

	memset(&common_info, 0x00, sizeof(common_info));

	ret = copy_from_user((void *)&common_info, param->property_param,
			sizeof(common_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_HREG_MWR(idx, ISP_COMMON_SPACE_SEL,
			3, common_info.fetch_color_space_sel);

	ISP_HREG_MWR(idx, ISP_COMMON_SPACE_SEL, 3 << 2,
		common_info.store_color_space_sel << 2);

	ISP_HREG_MWR(idx, ISP_COMMON_SPACE_SEL,
			BIT_4, common_info.ch0_path_ctrl << 4);

	ISP_HREG_MWR(idx, ISP_COMMON_PMU_RAM_MASK,
			BIT_0, common_info.ram_mask);

	ISP_HREG_WR(idx, ISP_COMMON_GCLK_CTRL_0,
				common_info.gclk_ctrl_rrgb);
	ISP_HREG_WR(idx, ISP_COMMON_GCLK_CTRL_1,
				common_info.gclk_ctrl_yiq_frgb);
	ISP_HREG_WR(idx, ISP_COMMON_GCLK_CTRL_2,
				common_info.gclk_ctrl_yuv);

	ISP_HREG_MWR(idx, ISP_COMMON_SHADOW_CTRL_CH0, BIT_16,
		common_info.shadow_ctrl_ch0.shadow_mctrl << 16);

	ISP_HREG_MWR(idx, ISP_COMMON_SHADOW_CTRL_CH0, 1 << 21,
		common_info.shadow_ctrl_ch0.comm_cfg_rdy << 21);

	return ret;
}

static int isp_k_3a_shadow_ctrl(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int enable = 0;

	ret = copy_from_user((void *)&enable, param->property_param,
			sizeof(enable));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_HREG_MWR(idx, ISP_COMMON_SHADOW_CTRL_CH0, 1 << 21, 1 << 21);

	return ret;
}

static int isp_k_common_shadow_ctrl(struct isp_io_param *param,
						enum isp_id idx)
{
	int ret = 0;
	unsigned int enable = 0;

	ret = copy_from_user((void *)&enable, param->property_param,
			sizeof(enable));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_HREG_MWR(idx, ISP_COMMON_SHADOW_CTRL_CH0, BIT_16, enable << 16);
	ISP_HREG_MWR(idx, ISP_COMMON_SHADOW_CTRL_CH0, 1 << 21, 1 << 21);

	return ret;
}

int isp_k_cfg_common(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t com_idx)
{
	int ret = 0;
	enum isp_id idx = ISP_ID_0;

	if (!param) {
		pr_err("fail to get param\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -EPERM;
	}

	idx = ISP_GET_ISP_ID(com_idx);
	switch (param->property) {
	case ISP_PRO_COMMON_BLOCK:
		ret = isp_k_common_block(param, idx);
		break;
	case ISP_PRO_COMMON_COMM_SHADOW:
		ret = isp_k_common_shadow_ctrl(param, idx);
		break;
	case ISP_PRO_COMMON_3A_SINGLE_FRAME_CTRL:
		ret = isp_k_3a_shadow_ctrl(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
