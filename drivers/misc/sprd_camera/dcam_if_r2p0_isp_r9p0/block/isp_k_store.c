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
#define pr_fmt(fmt) "STORE: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_store_block(struct isp_io_param *param, uint32_t idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_img_size size;
	struct isp_dev_store_info store_info;

	memset(&store_info, 0x00, sizeof(store_info));
	ret = copy_from_user((void *)&store_info,
		param->property_param, sizeof(store_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	ISP_REG_MWR(idx, ISP_STORE_BASE+ISP_STORE_PARAM,
			BIT_0, store_info.bypass);
	if (store_info.bypass)
		return 0;

	ISP_REG_MWR(idx, ISP_STORE_BASE+ISP_STORE_PARAM,
		0xF0, (store_info.color_format << 4));

	ISP_REG_MWR(idx, ISP_STORE_BASE+ISP_STORE_PARAM,
			0x300, store_info.endian << 8);

	size = store_info.size;
	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);
	ISP_REG_WR(idx, ISP_STORE_BASE + ISP_STORE_SLICE_SIZE, val);

	val = ((store_info.border.right_border & 0xFF) << 24) |
		((store_info.border.left_border & 0xFF) << 16) |
		((store_info.border.down_border & 0xFF) << 8) |
		(store_info.border.up_border & 0xFF);
	ISP_REG_WR(idx, ISP_STORE_BORDER, val);
	ISP_REG_WR(idx, ISP_STORE_BASE+ISP_STORE_SLICE_Y_ADDR,
			store_info.addr.chn0);

	ISP_REG_WR(idx, ISP_STORE_BASE+ISP_STORE_Y_PITCH,
			(store_info.pitch.chn0 & 0xFFFF));
	ISP_REG_WR(idx, ISP_STORE_BASE+ISP_STORE_SLICE_U_ADDR,
			store_info.addr.chn1);
	ISP_REG_WR(idx, ISP_STORE_BASE+ISP_STORE_U_PITCH,
			(store_info.pitch.chn1 & 0xFFFF));

	ISP_REG_WR(idx, ISP_STORE_BASE+ISP_STORE_SLICE_V_ADDR,
			store_info.addr.chn2);
	ISP_REG_WR(idx, ISP_STORE_BASE+ISP_STORE_V_PITCH,
			(store_info.pitch.chn2 & 0xFFFF));

	ISP_REG_MWR(idx, ISP_STORE_READ_CTRL,
			0x3, store_info.rd_ctrl);
	ISP_REG_MWR(idx, ISP_STORE_READ_CTRL,
			0xFFFFFFFC, store_info.store_res << 2);

	ISP_REG_MWR(idx, ISP_STORE_BASE+ISP_STORE_SHADOW_CLR_SEL,
			BIT_1, store_info.shadow_clr_sel << 1);
	ISP_REG_MWR(idx, ISP_STORE_BASE+ISP_STORE_SHADOW_CLR,
			BIT_0, store_info.shadow_clr);

	return ret;
}

static int isp_k_store_slice_size
	(struct isp_io_param *param, uint32_t idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_img_size size = {0, 0};
	unsigned int base = 0;

	switch (param->sub_block) {
	case ISP_BLOCK_STORE:
		base = ISP_STORE_BASE;
		break;
	case ISP_BLOCK_STOREA:
		base = ISP_STORE_PRE_CAP_BASE;
		break;
	case ISP_BLOCK_STORE1:
		base = ISP_STORE_VID_BASE;
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->sub_block);
		return -1;
	}

	ret = copy_from_user((void *)&size,
		param->property_param, sizeof(size));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);
	ISP_REG_WR(idx, base+ISP_STORE_SLICE_SIZE, val);

	return ret;
}

int isp_k_cfg_store(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t com_idx)
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
	case ISP_PRO_STORE_BLOCK:
		ret = isp_store_block(param, com_idx);
		break;
	case ISP_PRO_STORE_SLICE_SIZE:
		ret = isp_k_store_slice_size(param, com_idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
