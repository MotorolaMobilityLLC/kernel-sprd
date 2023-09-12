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

static int32_t isp_k_grgb_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_grgb_info grgb_info;

	memset(&grgb_info, 0x00, sizeof(grgb_info));

	ret = copy_from_user((void *)&grgb_info,
		param->property_param, sizeof(grgb_info));
	if (ret != 0) {
		pr_info("isp_k_grgb_block: copy error,ret=0x%x\n",
						(uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_GRGB_PARAM, BIT_0, grgb_info.bypass);

	val = ((grgb_info.diff_thd & 0x3FF) << 7)
		| ((grgb_info.edge_thd & 0x3F) << 1);
	ISP_REG_MWR(ISP_GRGB_PARAM, 0x1FFFE, val);
	val = grgb_info.grid_thd & 0xFFF;
	ISP_REG_MWR(ISP_GRGB_GRID, 0xFFF, val);

	return ret;

}

int32_t isp_k_cfg_grgb(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_grgb: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_grgb: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_GRGB_BLOCK:
		ret = isp_k_grgb_block(param);
		break;
	default:
		pr_info("fail cmd id:%d,not supported.\n", param->property);
		break;
	}

	return ret;
}

