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
#define pr_fmt(fmt) "HIST2: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_hist2_block(struct isp_io_param *param, enum isp_id idx)
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
		ISP_REG_MWR(idx, ISP_HIST2_CFG_RDY, BIT_0, 1);
		isp_dbg_s_ori_byp(SBLK_BYPASS, yuv_hist2, idx);
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

	ISP_REG_MWR(idx, ISP_HIST2_SKIP_CLR, BIT_0, 1);

	ISP_REG_MWR(idx, ISP_HIST2_CFG_RDY, BIT_0, 1);

	return ret;
}

static int isp_k_hist2_bypass(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param,
					sizeof(unsigned int));
	if (ret != 0) {
		pr_err("fail to copy from user, ret=0x%x\n",
				 (unsigned int)ret);
		return -EPERM;
	}
	pr_debug("%s: hist2 bypass %d\n", __func__, bypass);

	ISP_REG_MWR(idx, ISP_HIST2_PARAM, BIT_0, bypass);
	ISP_REG_MWR(idx, ISP_HIST2_CFG_RDY, BIT_0, 1);

	return ret;
}

int isp_k_cfg_hist2(struct isp_io_param *param,
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
	case ISP_PRO_HIST2_BLOCK:
		ret = isp_k_hist2_block(param, idx);
		break;
	case ISP_PRO_HIST2_BYPASS:
		ret = isp_k_hist2_bypass(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}

