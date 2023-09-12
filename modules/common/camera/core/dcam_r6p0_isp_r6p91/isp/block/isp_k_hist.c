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

int32_t isp_k_hist_statistic_r6p9(uint64_t *addr)
{
	uint32_t i = 0;

	for (i = 0; i < ISP_HIST_ITEMS; i++)
		*addr++ = ISP_REG_RD(ISP_HIST_CH0 + 0x4 * i);
	return 0;
}

static int32_t isp_k_hist_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_hist_info hist_info;

	memset(&hist_info, 0x00, sizeof(hist_info));

	ret = copy_from_user((void *)&hist_info,
		param->property_param, sizeof(hist_info));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (hist_info.buf_rst_en)
		ISP_REG_OWR(ISP_HIST_BUF_RST_EN, BIT_0);
	else
		ISP_REG_MWR(ISP_HIST_BUF_RST_EN, BIT_0, 0);

	if (hist_info.skip_num_clr)
		ISP_REG_OWR(ISP_HIST_SKIP_NUM_CLR, BIT_0);
	else
		ISP_REG_MWR(ISP_HIST_SKIP_NUM_CLR, BIT_0, 0);

	ISP_REG_MWR(ISP_HIST_PARAM, 0xF0, hist_info.skip_num << 4);

	if (hist_info.mode)
		ISP_REG_OWR(ISP_HIST_PARAM, BIT_1);
	else
		ISP_REG_MWR(ISP_HIST_PARAM, BIT_1, 0);

	if (hist_info.bypass)
		ISP_REG_OWR(ISP_HIST_PARAM, BIT_0);
	else
		ISP_REG_MWR(ISP_HIST_PARAM, BIT_0, 0);

	return ret;
}

int32_t isp_k_cfg_hist(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_hist: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_hist: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_HIST_BLOCK:
		ret = isp_k_hist_block(param);
		break;
	default:
		pr_info("fail cmd id:%d,not supported.\n", param->property);
		break;
	}

	return ret;
}
