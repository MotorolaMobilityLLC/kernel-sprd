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
#define pr_fmt(fmt) "DISPATCH: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

#define ISP2DDR_BUF_LIMIT	640

struct isp_pipe_buf_ctrl_ch0 {
	uint8_t pipe_hblank_num;
	uint8_t pipe_flush_num;
	uint16_t pipe_nfull_num;
};

struct isp_dispatch_line_dly1 {
	uint8_t done_line_dly_num;
	uint16_t width_dly_num_flash;
	uint8_t width_flash_mode;
};

struct isp_dispatch_info_inner {
	/*1.1.54.5	ISP_DISPATCH_CH0_BAYER*/
	uint32_t  bayer_ch0;

	/*1.1.54.6	ISP_DISPATCH_CH0_SIZE*/
	struct isp_img_size ch0_size;

	/*1.1.54.7	ISP_DISPATCH_DLY*/
	uint16_t  width_dly_num_ch0;
	uint16_t  height_dly_num_ch0;

	/*1.1.54.7	1.1.54.8	ISP_DISPATCH_HW_CTRL_CH0*/
	uint16_t  nready_cfg_ch0;
	uint16_t  nready_width_ch0;

	uint16_t  ready_width_ch0;
	uint16_t  dbg_mode_ch0;

	/*1.1.54.9	ISP_DISPATCH_LINE_DLY1*/
	struct isp_dispatch_line_dly1 dispatch_ldly1;

	struct isp_pipe_buf_ctrl_ch0 yuv_pipe_buf;
	/* struct isp_slice_dispatch_info slice_info_array[SLICE_NUM_MAX]; */
};

static struct isp_dispatch_info_inner s_dispatch_info_inner = {
	0/*BAYER0_GRBG*/,
	{0, 0}, 0x3C, 0x1D, 0x0,
	0x4, 0x8, 0, {0x1C, 0x280, 0}, {0x3c, 0x4, 0x64}
};

static inline void isp_dispatch_overwrite_info(
	struct isp_dispatch_info_inner *dispatch_info_inner,
	struct isp_dev_dispatch_info *dispatch_info)
{
	dispatch_info_inner->bayer_ch0 = dispatch_info->bayer_ch0;
	dispatch_info_inner->ch0_size = dispatch_info->ch0_size;
	dispatch_info_inner->width_dly_num_ch0 =
					dispatch_info->width_dly_num_ch0;
	dispatch_info_inner->height_dly_num_ch0 =
					dispatch_info->height_dly_num_ch0;
	dispatch_info_inner->nready_cfg_ch0 = dispatch_info->nready_cfg_ch0;
	dispatch_info_inner->nready_width_ch0 = dispatch_info->nready_width_ch0;
}

static int isp_k_dispatch_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_img_size size;
	struct isp_dev_dispatch_info dispatch_info;
	struct isp_dispatch_info_inner dispatch_info_inner;

	dispatch_info_inner = s_dispatch_info_inner;
	memset(&dispatch_info, 0x00, sizeof(dispatch_info));
	ret = copy_from_user((void *)&dispatch_info,
		param->property_param, sizeof(struct isp_dev_dispatch_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	isp_dispatch_overwrite_info(&dispatch_info_inner, &dispatch_info);

	ISP_REG_WR(idx, ISP_DISPATCH_CH0_BAYER,
		dispatch_info_inner.bayer_ch0 & 0x3);

	size = dispatch_info_inner.ch0_size;
	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);
	ISP_REG_WR(idx, ISP_DISPATCH_CH0_SIZE, val);

	val = ((dispatch_info_inner.height_dly_num_ch0 & 0xFF) << 8) |
			(dispatch_info_inner.width_dly_num_ch0 & 0xFF);

	if (size.width <= ISP2DDR_BUF_LIMIT)
		ISP_REG_WR(idx, ISP_DISPATCH_DLY, 0x213c);
	else
		ISP_REG_WR(idx, ISP_DISPATCH_DLY, val);

	val =	((dispatch_info_inner.dbg_mode_ch0 & 0x1) << 31) |
			((dispatch_info_inner.ready_width_ch0 & 0x1fff) << 16) |
			((dispatch_info_inner.nready_cfg_ch0 & 0x1) << 15) |
			(dispatch_info_inner.nready_width_ch0 & 0x1FFF);
	ISP_REG_WR(idx, ISP_DISPATCH_HW_CTRL_CH0, val);

	val = (dispatch_info_inner.dispatch_ldly1.done_line_dly_num & 0xff);
#ifdef REG_AP_MODE
	ISP_REG_MWR(idx, ISP_DISPATCH_DLY1, 0xFF, val);
#else
	if (val < dispatch_info_inner.height_dly_num_ch0) {
		if (size.width <= ISP2DDR_BUF_LIMIT)
			ISP_REG_MWR(idx, ISP_DISPATCH_DLY1, 0xFF, 0x20);
		else
			ISP_REG_MWR(idx, ISP_DISPATCH_DLY1, 0xFF, val);
	} else {
		pr_err("fail to get valid done_line_dly_num %d, too large\n",
                        val);
		return -1;
	}
#endif

	return ret;
}

static int isp_k_dispatch_ch0_size
	(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_img_size size = {0};

	ret = copy_from_user((void *)&size,
		param->property_param, sizeof(size));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}
	pr_debug("dispatch_ch0_size W 0x%x, H 0x%x.\n",
		 size.width, size.height);
	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);
	ISP_REG_WR(idx, ISP_DISPATCH_CH0_SIZE, val);

	return ret;
}

int isp_k_cfg_dispatch(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param.\n");
		return -EPERM;
	}
	if (param->property_param == NULL) {
		pr_err("fail to get property param.\n");
		return -EPERM;
	}
	switch (param->property) {
	case ISP_PRO_DISPATCH_BLOCK:
		ret = isp_k_dispatch_block(param, idx);
		break;
	case ISP_PRO_DISPATCH_CH0_SIZE:
		ret = isp_k_dispatch_ch0_size(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}

