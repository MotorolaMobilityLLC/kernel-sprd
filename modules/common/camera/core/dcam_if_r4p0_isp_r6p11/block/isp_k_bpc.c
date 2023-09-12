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
#define pr_fmt(fmt) "BPC: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_bpc_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int i = 0;
	struct isp_dev_bpc_info bpc_info;

	memset(&bpc_info, 0x00, sizeof(bpc_info));

	ret = copy_from_user((void *)&bpc_info, param->property_param,
			sizeof(bpc_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_BPC_PARAM, BIT_0, bpc_info.bypass);
	if (bpc_info.bypass) {
		isp_dbg_s_ori_byp(SBLK_BYPASS, raw_bpc, idx);
		return 0;
	}

	ISP_REG_MWR(idx, ISP_BPC_PARAM, BIT_4 |
			BIT_5, bpc_info.bpc_mode << 4);
	ISP_REG_MWR(idx, ISP_BPC_PARAM, BIT_9 |
			BIT_8, bpc_info.edge_hv_mode << 8);
	ISP_REG_MWR(idx, ISP_BPC_PARAM, BIT_11 |
			BIT_10, bpc_info.edge_rd_mode << 10);
	ISP_REG_MWR(idx, ISP_BPC_PARAM,
			BIT_12, bpc_info.bad_pixel_pos_out_en << 12);
	ISP_REG_MWR(idx, ISP_BPC_PARAM, 0xFF0000, bpc_info.rd_retain_num << 16);
	ISP_REG_MWR(idx, ISP_BPC_PARAM, BIT_24, bpc_info.rd_max_len_sel << 24);
	ISP_REG_MWR(idx, ISP_BPC_PARAM, BIT_25, bpc_info.wr_max_len_sel << 25);

	for (i = 0; i < 4; i++) {
		val = ((bpc_info.double_badpixel_th[i] & 0x3FF) << 20) |
			((bpc_info.three_badpixel_th[i] & 0x3FF) << 10) |
			((bpc_info.four_badpixel_th[i] & 0x3FF));
		ISP_REG_WR(idx, ISP_BPC_BAD_PIXEL_TH0 + i * 4, val);
	}

	val = ((bpc_info.shift[0] & 0xF) << 28) |
			((bpc_info.shift[1] & 0xF) << 24) |
			((bpc_info.shift[2] & 0xF) << 20) |
			((bpc_info.flat_th & 0x3FF) << 10) |
			((bpc_info.texture_th & 0x3FF));
	ISP_REG_WR(idx, ISP_BPC_FLAT_TH, val);

	val = ((bpc_info.edge_ratio_rd & 0x1FF) << 16) |
			((bpc_info.edge_ratio_hv & 0x1FF));
	ISP_REG_WR(idx, ISP_BPC_EDGE_RATIO, val);

	val = ((bpc_info.low_coeff & 0x7) << 24) |
			((bpc_info.high_coeff & 0x7) << 16) |
			((bpc_info.low_offset & 0xFF) << 8) |
			((bpc_info.high_offset & 0xFF));
	ISP_REG_WR(idx, ISP_BPC_BAD_PIXEL_PARAM, val);

	val = ((bpc_info.max_coeff & 0x1F) << 16) |
			((bpc_info.min_coeff & 0x1F));

	ISP_REG_WR(idx, ISP_BPC_BAD_PIXEL_COEF, val);

	for (i = 0; i < 8; i++) {
		val = ((bpc_info.lut_level[i] & 0x3FF) << 20) |
				((bpc_info.slope_k[i] & 0x3FF) << 10) |
				(bpc_info.intercept_b[i] & 0x3FF);
		ISP_REG_WR(idx, ISP_BPC_LUTWORD0 + i * 4, val);
	}

	ISP_REG_WR(idx, ISP_BPC_MAP_ADDR, bpc_info.map_addr);
	ISP_REG_MWR(idx, ISP_BPC_MAP_CTRL,
			BIT_0, bpc_info.bad_map_hw_fifo_clr_en);
	ISP_REG_MWR(idx, ISP_BPC_MAP_CTRL1,
			0xFFFF0000, bpc_info.bad_pixel_num << 16);
	ISP_REG_WR(idx, ISP_BPC_OUT_ADDR, bpc_info.bad_pixel_pos_out_addr);

	return ret;
}

int isp_k_cfg_bpc(struct isp_io_param *param,
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
	case ISP_PRO_BPC_BLOCK:
		ret = isp_k_bpc_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
