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
#include "sprd_isp_r6p91.h"
#include <asm/cacheflush.h>
#include "isp_reg.h"
#include "isp_block.h"

static int32_t isp_k_binning_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_binning4awb_info binning_info;

	ret = copy_from_user((void *)&binning_info,
		param->property_param, sizeof(binning_info));
	if (ret != 0) {
		pr_info("copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (binning_info.burst_mode & 0x3) << 2;
	ISP_REG_MWR(ISP_BINNING_PARAM, 0xC, val);

	val = (binning_info.endian & 0x1) << 1;
	ISP_REG_MWR(ISP_BINNING_PARAM, BIT_1, val);

	val = (binning_info.mem_fifo_clr & 0x1) << 7;
	ISP_REG_MWR(ISP_BINNING_PARAM, 0x80, val);

	val = ((binning_info.hx & 0x7) << 4)
		| ((binning_info.vx & 0x7) << 8);
	ISP_REG_MWR(ISP_BINNING_PARAM, 0x770, val);

	ISP_REG_WR(ISP_BINNING_PITCH, binning_info.pitch);

	/* awb use ae statis instead of binning */
	/*ISP_REG_MWR(ISP_BINNING_PARAM, BIT_0, binning_info.bypass);*/
	ISP_REG_MWR(ISP_BINNING_PARAM, BIT_0, 1);

	return ret;

}

int32_t isp_k_cfg_binning(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_binning: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_binning: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_BINNING4AWB_BLOCK:
		ret = isp_k_binning_block(param);
		break;
	default:
		pr_info("fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
