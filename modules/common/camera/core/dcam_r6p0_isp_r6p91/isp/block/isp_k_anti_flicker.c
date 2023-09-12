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
#include <asm/cacheflush.h>
#include "isp_block.h"

static int32_t isp_k_afl_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass,
		param->property_param, sizeof(bypass));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass)
		ISP_REG_MWR(ISP_ANTI_FLICKER_PARAM0, BIT_0, 1);
	else
		ISP_REG_MWR(ISP_ANTI_FLICKER_PARAM0, BIT_0, 0);

	return ret;
}

static int32_t isp_k_anti_flicker_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_anti_flicker_info afl_info;

	memset(&afl_info, 0x00, sizeof(afl_info));

	ret = copy_from_user((void *)&afl_info,
		param->property_param, sizeof(afl_info));
	if (ret != 0) {
		pr_info("isp_k_anti_flicker_block: copy error, ret=0x%x\n",
			(uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_ANTI_FLICKER_PARAM0, BIT_0,
		afl_info.bypass);
	if (afl_info.bypass) {
		pr_info("%s: afl bypass\n", __func__);
		return 0;
	}

	ISP_REG_MWR(ISP_ANTI_FLICKER_PARAM0, BIT_1,
		afl_info.mode << 1);

	ISP_REG_MWR(ISP_ANTI_FLICKER_PARAM0, (BIT_2 | BIT_3 | BIT_4 | BIT_5),
		afl_info.skip_frame_num << 2);

	ISP_REG_MWR(ISP_ANTI_FLICKER_PARAM1, 0xF, afl_info.line_step);

	ISP_REG_MWR(ISP_ANTI_FLICKER_PARAM1, 0xFF<<8, afl_info.frame_num<<8);

	ISP_REG_MWR(ISP_ANTI_FLICKER_PARAM1, 0xFFFF<<16, afl_info.vheight<<16);

	ISP_REG_MWR(ISP_ANTI_FLICKER_COL_POS, 0xFFFF, afl_info.start_col);

	ISP_REG_MWR(ISP_ANTI_FLICKER_COL_POS, 0xFFFF << 16,
		afl_info.end_col << 16);

	return ret;
}

int32_t isp_k_cfg_anti_flicker(struct isp_io_param *param)
{
	int32_t ret = 0;
	enum isp_dev_afl_sel sel = 0;

	if (!param) {
		pr_info("isp_k_cfg_anti_flicker: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("property_param is null error.\n");
		return -1;
	}

	sel = isp_k_common_afl_get();
	if (sel != ISP_DEV_ANTI_FLICKER) {
		pr_err("the valid afl is anti_flicker_new!\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_ANTI_FLICKER_BLOCK:
		ret = isp_k_anti_flicker_block(param);
		break;
	case ISP_PRO_ANTI_FLICKER_BYPASS:
		ret = isp_k_afl_bypass(param);
		break;
	default:
		pr_info("fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}

