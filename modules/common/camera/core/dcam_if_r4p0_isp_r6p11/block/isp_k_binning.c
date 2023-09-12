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
#define pr_fmt(fmt) "BINNING: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_binning_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_dev_binning4awb_info binning_info;
	unsigned int binning_h = 0;
	unsigned int binning_w = 0;

	memset(&binning_info, 0x00, sizeof(binning_info));
	ret = copy_from_user((void *)&binning_info, param->property_param,
			sizeof(binning_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	binning_info.bin_mode_sel = 1;
	binning_info.bin_skip_num = 0;
	binning_info.mem_fifo_clr = 0;
	binning_h = (binning_info.img_size.height >> binning_info.vx) & ~0x1;
	binning_w = (binning_info.img_size.width >> binning_info.hx) & ~0x1;
	binning_info.ddr_wr_num = (binning_h * binning_w + 5) / 6;
	binning_info.skip_num_clr = 1;

	if (ISP_GET_SID(idx) == ISP_SCENE_CAP) {
		pr_debug("capture binning bypass\n");
		binning_info.bypass = 1;
	}

	ISP_REG_MWR(idx, ISP_BINNING_PARAM, BIT_0, binning_info.bypass);
	if (binning_info.bypass) {
		ISP_REG_MWR(idx, ISP_BINNING_CFG_READY, BIT_0, 0x1);
		isp_dbg_s_ori_byp(SBLK_BYPASS, raw_bin, idx);
		return 0;
	}

	val = (binning_info.bin_mode_sel & 0x1) << 1;
	ISP_REG_MWR(idx, ISP_BINNING_PARAM, BIT_1, val);

	val = ((binning_info.hx & 0x7) << 4) |
			((binning_info.vx & 0x7) << 8);
	ISP_REG_MWR(idx, ISP_BINNING_PARAM, 0x770, val);

	ISP_REG_MWR(idx, ISP_BINNING_PARAM, 0x80,
		(binning_info.mem_fifo_clr & 0x1) << 7);
	ISP_REG_MWR(idx, ISP_BINNING_PARAM, 0xF000,
		(binning_info.bin_skip_num & 0xF) << 12);

	ISP_REG_MWR(idx, ISP_BINNING_DDR_WR_NUM,
		0xFFFF, binning_info.ddr_wr_num);

	val = (binning_info.skip_num_clr & 0x1);
	ISP_REG_MWR(idx, ISP_BINNING_SKIP_NUM_CLR, BIT_0, val);

	ISP_REG_MWR(idx, ISP_BINNING_CFG_READY, BIT_0, 0x1);

	return ret;
}

static int isp_k_binning_bypass(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param,
					sizeof(unsigned int));
	if (ret != 0) {
		pr_err("fail to copy from user,ret=0x%x\n", (unsigned int)ret);
		return -EPERM;
	}
	pr_debug("%s: binning bypass %d\n", __func__, bypass);

	ISP_REG_MWR(idx, ISP_BINNING_PARAM, BIT_0, bypass);
	ISP_REG_MWR(idx, ISP_BINNING_CFG_READY, BIT_0, 0x1);

	return ret;
}

int isp_k_cfg_binning(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -EPERM;
	}

	switch (param->property) {
	case ISP_PRO_BINNING_BLOCK:
		ret = isp_k_binning_block(param, idx);
		break;
	case ISP_PRO_BINNING_BYPASS:
		ret = isp_k_binning_bypass(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
