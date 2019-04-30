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
#define pr_fmt(fmt) "HIST2: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_hist2_block(struct isp_io_param *param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_hist2_info hist2_info;
	unsigned int val = 0;

	memset(&hist2_info, 0x00, sizeof(hist2_info));
	ret = copy_from_user((void *)&hist2_info,
		param->property_param, sizeof(hist2_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}
	ISP_REG_MWR(idx, ISP_HIST2_PARAM, BIT_0, hist2_info.bypass);

	if (hist2_info.bypass) {
		ISP_REG_MWR(idx, ISP_HIST2_CFG_READY, BIT_0, 1);
		return 0;
	}

	ISP_REG_MWR(idx, ISP_HIST2_PARAM, BIT_1, hist2_info.mode << 1);

	ISP_REG_MWR(idx, ISP_HIST2_PARAM, 0xF0, hist2_info.skip_num << 4);

	val = (hist2_info.hist_roi.start_y & 0xFFFF)
		| ((hist2_info.hist_roi.start_x & 0xFFFF) << 16);
	ISP_REG_WR(idx, ISP_HIST2_ROI_S0, val);

	val = (hist2_info.hist_roi.end_y & 0xFFFF)
		| ((hist2_info.hist_roi.end_x & 0xFFFF) << 16);
	ISP_REG_WR(idx, ISP_HIST2_ROI_E0, val);

	ISP_REG_MWR(idx, ISP_HIST2_SKIP_NUM_CLR, BIT_0, 1);

	ISP_REG_MWR(idx, ISP_HIST2_CFG_READY, BIT_0, 1);

	return ret;
}

int isp_k_cfg_hist2(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t com_idx)
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
	case ISP_PRO_HIST2_BLOCK:
		ret = isp_k_hist2_block(param, com_idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
