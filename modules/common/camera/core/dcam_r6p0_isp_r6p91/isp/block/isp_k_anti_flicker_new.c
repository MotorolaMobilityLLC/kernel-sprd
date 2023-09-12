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

static int isp_k_afl_new_bypass(struct isp_io_param *param)
{
	int ret = 0;
	unsigned int bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param,
			sizeof(bypass));
	if (ret != 0) {
		pr_err("%s: error: copy from user 0x%x\n",
			__func__, (unsigned int)ret);
		return -EPERM;
	}

	pr_debug("%s: afl bypass %d, cur_status: 0x%x\n", __func__, bypass,
		 ISP_REG_RD(ISP_ANTI_FLICKER_NEW_PARAM0));
	if (bypass == 0)
		ISP_REG_MWR(ISP_ANTI_FLICKER_NEW_PARAM0, BIT_0, 0);

	return ret;
}

static int isp_k_anti_flicker_new_block(struct isp_io_param *param)
{
	int ret = 0;
	struct isp_dev_anti_flicker_new_info afl_info;

	memset(&afl_info, 0x00, sizeof(afl_info));
	ret = copy_from_user((void *)&afl_info, param->property_param,
			sizeof(afl_info));
	if (ret != 0) {
		pr_err("%s: error: copy from user ret=0x%x\n",
				__func__, (unsigned int)ret);
		return -EPERM;
	}

	/* enable new anti flicker moudle */
	ISP_REG_MWR(ISP_ANTI_FLICKER_NEW_PARAM0, BIT_0, afl_info.bypass);
	if (afl_info.bypass) {
		pr_info("%s: afl bypass\n", __func__);
		return 0;
	}

	ISP_REG_MWR(ISP_ANTI_FLICKER_NEW_PARAM0, BIT_1,
		afl_info.mode << 1);
	ISP_REG_MWR(ISP_ANTI_FLICKER_NEW_PARAM0,
		(BIT_2 | BIT_3 | BIT_4 | BIT_5),
		afl_info.skip_frame_num << 2);

#if 0
	ISP_REG_MWR(ISP_ANTI_FLICKER_NEW_PARAM0, BIT_8,
		afl_info.done_not_clr << 8);
#endif

	ISP_REG_MWR(ISP_ANTI_FLICKER_NEW_PARAM1,
				0xFFFFFF, afl_info.afl_stepx);

	ISP_REG_MWR(ISP_ANTI_FLICKER_NEW_PARAM2,
				0xFFFFFF, afl_info.afl_stepy);
	ISP_REG_MWR(ISP_ANTI_FLICKER_NEW_PARAM2,
				0xFF << 24, afl_info.frame_num << 24);

	ISP_REG_MWR(ISP_ANTI_FLICKER_NEW_COL_POS,
				0xFFFF, afl_info.start_col);
	ISP_REG_MWR(ISP_ANTI_FLICKER_NEW_COL_POS,
				0xFFFF << 16, afl_info.end_col << 16);

	return ret;
}


int isp_k_cfg_anti_flicker_new(struct isp_io_param *param)
{
	int ret = 0;
	enum isp_dev_afl_sel sel = 0;

	if (!param) {
		pr_err("%s: error: param is null\n", __func__);
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("%s: error: property_param is null\n", __func__);
		return -EPERM;
	}

	sel = isp_k_common_afl_get();
	if (sel != ISP_DEV_ANTI_FLICKER_NEW) {
		pr_info("the valid afl is anti_flicker_old!\n");
		return 0;
	}

	switch (param->property) {
	case ISP_PRO_ANTI_FLICKER_NEW_BLOCK:
		ret = isp_k_anti_flicker_new_block(param);
		break;
	case ISP_PRO_ANTI_FLICKER_NEW_BYPASS:
		ret = isp_k_afl_new_bypass(param);
		break;
	default:
		pr_err("%s: cmd id:%d, not supported\n",
			__func__, param->property);
		break;
	}

	return ret;
}

