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
#define pr_fmt(fmt) "RGBD: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_yrandom_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	struct isp_dev_yrandom_info yrandom_info;
	unsigned int val = 0;

	memset(&yrandom_info, 0x00, sizeof(yrandom_info));

	ret = copy_from_user((void *)&yrandom_info,
		param->property_param, sizeof(yrandom_info));
	if (ret != 0) {
		pr_err("fail to copy_from_user, ret = 0x%x\n",
			(unsigned int)ret);
		return -1;
	}

	ISP_REG_MWR(idx, ISP_RGBG_YRANDOM_PARAM1,
		BIT_0, yrandom_info.yrandom_bypass);
	if (yrandom_info.yrandom_bypass)
		return 0;

	ISP_REG_MWR(idx, ISP_RGBG_YRANDOM_PARAM1,
		0xFFFFFF00, yrandom_info.seed << 8);
	val = (yrandom_info.shift & 0xF)
		| ((yrandom_info.offset & 0x7FF) << 16);
	ISP_REG_MWR(idx, ISP_RGBG_YRANDOM_PARAM2, 0x7FF000F, val);

	val = (yrandom_info.takeBit[0]  & 0xF) |
		((yrandom_info.takeBit[1] & 0xF) << 4) |
		((yrandom_info.takeBit[2] & 0xF) << 8) |
		((yrandom_info.takeBit[3] & 0xF) << 12) |
		((yrandom_info.takeBit[4] & 0xF) << 16) |
		((yrandom_info.takeBit[5] & 0xF) << 20) |
		((yrandom_info.takeBit[6] & 0xF) << 24) |
		((yrandom_info.takeBit[7] & 0xF) << 28);
	ISP_REG_WR(idx, ISP_RGBG_YRANDOM_PARAM3, val);

	return ret;
}

static int isp_k_yrandom_chk_sum_clr(struct isp_io_param *param,
			enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;

	ret = copy_from_user((void *)&val,
		param->property_param, sizeof(unsigned int));
	if (ret != 0) {
		pr_err("fail to copy_from_user, ret = 0x%x\n",
			(unsigned int)ret);
		return -1;
	}

	if (val)
		ISP_REG_OWR(idx, ISP_RGBG_YRANDOM_PARAM1, BIT_2);
	else
		ISP_REG_MWR(idx, ISP_RGBG_YRANDOM_PARAM1, BIT_2, 0);

	return ret;
}

static int isp_k_yrandom_init(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;

	ret = copy_from_user((void *)&val,
		param->property_param, sizeof(unsigned int));
	if (ret != 0) {
		pr_err("fail to copy_from_user, ret = 0x%x\n",
			(unsigned int)ret);
		return -1;
	}

	ISP_REG_WR(idx, ISP_RGBG_YRANDOM_INIT, val & 0x1);

	return ret;
}

int isp_k_cfg_yrandom(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get valid pproperty_param\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_YRANDOM_BLOCK:
		ret = isp_k_yrandom_block(param, idx);
		break;
	case ISP_PRO_YRANDOM_INIT:
		ret = isp_k_yrandom_init(param, idx);
		break;
	case ISP_PRO_YRANDOM_CHK_SUM_CLR:
		ret = isp_k_yrandom_chk_sum_clr(param, idx);
		break;
	default:
		pr_err("fail to get right cmd id:%d\n",
			param->property);
		break;
	}

	return ret;
}
