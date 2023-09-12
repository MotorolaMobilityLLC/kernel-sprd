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
#define pr_fmt(fmt) "HIST: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_hist_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	struct isp_dev_hist_info hist_info;

	memset(&hist_info, 0x00, sizeof(hist_info));
	ret = copy_from_user((void *)&hist_info,
		param->property_param, sizeof(hist_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	ISP_REG_MWR(idx, ISP_HIST_PARAM, BIT_0, hist_info.bypass);
	if (hist_info.bypass) {
		ISP_REG_MWR(idx, ISP_HIST_CFG_READY, BIT_0, 1);
		isp_dbg_s_ori_byp(SBLK_BYPASS, yuv_hist, idx);
		pr_debug("%s: hist bypass %d\n", __func__, hist_info.bypass);
		return 0;
	}
	ISP_REG_MWR(idx, ISP_HIST_PARAM, BIT_1, hist_info.mode << 1);
	ISP_REG_MWR(idx, ISP_HIST_PARAM, 0xF0,  hist_info.skip_num << 4);

	ISP_REG_MWR(idx, ISP_HIST_SKIP_NUM_CLR, BIT_0, 1);

	ISP_REG_MWR(idx, ISP_HIST_CFG_READY, BIT_0, 1);

	return ret;
}

static int isp_k_hist_bypass(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param,
					sizeof(bypass));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}
	pr_debug("%s: hist bypass %d\n", __func__, bypass);

	ISP_REG_MWR(idx, ISP_HIST_PARAM, BIT_0, bypass);
	ISP_REG_MWR(idx, ISP_HIST_CFG_READY, BIT_0, 1);

	return ret;
}

int isp_k_cfg_hist(struct isp_io_param *param,
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
	case ISP_PRO_HIST_BLOCK:
		ret = isp_k_hist_block(param, idx);
		break;
	case ISP_PRO_HIST_BYPASS:
		ret = isp_k_hist_bypass(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
