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
#include "isp_hw.h"
#include "isp_reg.h"
#include "cam_types.h"
#include "cam_block.h"

#define ISP_K_HIST_MIN_W 400
#define ISP_K_HIST_MIN_H 400

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "HIST2: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_hist2_block(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	uint32_t val;
	struct isp_dev_hist2_info hist2_info;

	ret = copy_from_user((void *)&hist2_info,
			param->property_param,
			sizeof(hist2_info));

	/* force isp hist2 enable */
	hist2_info.bypass = 0x0;

	/* 0 for single frame, 1 for multi frame*/
	hist2_info.mode = 0x1;

	if ((hist2_info.hist_roi.end_x - hist2_info.hist_roi.start_x) < ISP_K_HIST_MIN_W) {
		hist2_info.hist_roi.start_x = 0;
		hist2_info.hist_roi.end_x = ISP_K_HIST_MIN_W - 1;
	}

	if ((hist2_info.hist_roi.end_y - hist2_info.hist_roi.start_y) < ISP_K_HIST_MIN_H) {
		hist2_info.hist_roi.start_y = 0;
		hist2_info.hist_roi.end_y = ISP_K_HIST_MIN_H - 1;
	}

	pr_debug("bypass[%d] mode[%d] skip[%d] channel[%d] sx[%d] sy[%d] ex[%d] ey[%d] clr[%d]\n",
			hist2_info.bypass,
			hist2_info.mode,
			hist2_info.skip_num,
			hist2_info.channel_sel,
			hist2_info.hist_roi.start_x,
			hist2_info.hist_roi.start_y,
			hist2_info.hist_roi.end_x,
			hist2_info.hist_roi.end_y,
			hist2_info.skip_num_clr);

	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return ret;
	}

	pr_debug("hist_bypass cfg [%d]\n", (g_isp_bypass[idx] & (1 << _EISP_HIST2)));

	if (g_isp_bypass[idx] & (1 << _EISP_HIST2))
		hist2_info.bypass = 1;

	pr_debug("hist_bypass info [%d]\n", hist2_info.bypass);

	ISP_REG_MWR(idx, ISP_HIST2_PARAM, BIT_0, hist2_info.bypass);
	if (hist2_info.bypass) {
		ISP_REG_MWR(idx, ISP_HIST2_CFG_RDY, BIT_0, 1);
		return 0;
	}

	val = ((hist2_info.mode & 1) << 1) |
		((hist2_info.skip_num & 0xF) << 4) |
		((hist2_info.channel_sel & 0x1) << 8);
	ISP_REG_MWR(idx, ISP_HIST2_PARAM, 0x1F2, val);

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

int isp_k_cfg_hist2(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;

	switch (param->property) {
	case ISP_PRO_HIST2_BLOCK:
		ret = isp_k_hist2_block(param, isp_k_param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
