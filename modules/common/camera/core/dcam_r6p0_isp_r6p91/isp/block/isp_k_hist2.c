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

static int32_t isp_k_hist2_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_hist2_info hist2_info;
	uint32_t val = 0;
	uint32_t i = 0;

	memset(&hist2_info, 0x00, sizeof(hist2_info));

	ret = copy_from_user((void *)&hist2_info,
		param->property_param, sizeof(hist2_info));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (hist2_info.skip_num_clr)
		ISP_REG_OWR(ISP_HIST2_SKIP_NUM_CLR, BIT_0);
	else
		ISP_REG_MWR(ISP_HIST2_SKIP_NUM_CLR, BIT_0, 0);

	ISP_REG_MWR(ISP_HIST2_PARAM, 0xF0, hist2_info.skip_num << 4);

	if (hist2_info.mode)
		ISP_REG_OWR(ISP_HIST2_PARAM, BIT_1);
	else
		ISP_REG_MWR(ISP_HIST2_PARAM, BIT_1, 0);

	val = (hist2_info.hist_roi_y_s[i] & 0xFFFF)
		| ((hist2_info.hist_roi_x_s[i] & 0xFFFF) << 16);
	ISP_REG_WR(ISP_HIST2_ROI_S0, val);

	val = (hist2_info.hist_roi_y_e[i] & 0xFFFF)
		| ((hist2_info.hist_roi_x_e[i] & 0xFFFF) << 16);
	ISP_REG_WR(ISP_HIST2_ROI_E0, val);

	if (hist2_info.en)
		ISP_REG_OWR(ISP_HIST2_BUF_RST_EN, BIT_0);
	else
		ISP_REG_MWR(ISP_HIST2_BUF_RST_EN, BIT_0, 0);

	if (hist2_info.bypass)
		ISP_REG_OWR(ISP_HIST2_PARAM, BIT_0);
	else
		ISP_REG_MWR(ISP_HIST2_PARAM, BIT_0, 0);

	return ret;
}

int32_t isp_k_cfg_hist2(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_hist2: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_hist2: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_HIST2_BLOCK:
		ret = isp_k_hist2_block(param);
		break;
	default:
		pr_info("fail cmd id:%d,not supported.\n", param->property);
		break;
	}

	return ret;
}
