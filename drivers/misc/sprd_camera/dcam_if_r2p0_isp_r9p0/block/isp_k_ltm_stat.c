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
#define pr_fmt(fmt) "LTM_STAT: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define	ISP_FRGB_LTM_STAT_BUF0				0
#define	ISP_FRGB_LTM_STAT_BUF1				1

static int isp_k_ltm_stat_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int base = 0;
	unsigned int buf_addr = 0;
	unsigned int buf_addr_0 = 0;
	unsigned int buf_addr_1 = 0;
	int i = 0;
	struct isp_dev_ltm_stat_block_info ltm_stat_info = {0};

	ret = copy_from_user((void *)&ltm_stat_info,
		param->property_param, sizeof(ltm_stat_info));
	if (ret != 0) {
		pr_err("fail to check param, ret = 0x%x\n", (unsigned int)ret);
		return -1;
	}

	switch (param->sub_block) {
	case ISP_BLOCK_LTM_RGB_STAT:
		base = ISP_LTM_STAT_RGB_BASE;
		buf_addr_0 = ISP_LTM_RGB_STAT_BUF0_ADDR;
		buf_addr_1 = ISP_LTM_RGB_STAT_BUF1_ADDR;
		break;
	case ISP_BLOCK_LTM_YUV_STAT:
		base = ISP_LTM_STAT_YUV_BASE;
		buf_addr_0 = ISP_LTM_YUV_STAT_BUF0_ADDR;
		buf_addr_1 = ISP_LTM_YUV_STAT_BUF1_ADDR;
		break;
	default:
		pr_err("fail to check cmd %d.\n", param->sub_block);
		return -1;
	}

	ISP_REG_MWR(idx, base + ISP_LTM_PARAMETERS,
		BIT_0, ltm_stat_info.bypass);
	if (ltm_stat_info.bypass)
		return 0;

	val = ((ltm_stat_info.binning_en & 0x1) << 1) |
		((ltm_stat_info.region_est_en & 0x1) << 2) |
		((ltm_stat_info.buf_full_mode & 0x1) << 3) |
		((ltm_stat_info.channel_sel & 0x1) << 4) |
		((ltm_stat_info.lut_buf_sel & 0x1) << 5);
	ISP_REG_MWR(idx, base + ISP_LTM_PARAMETERS, 0x3E, val);

	if (ISP_FRGB_LTM_STAT_BUF0 ==
		ltm_stat_info.buf_info.buf_sel)
		buf_addr = buf_addr_0;
	else
		buf_addr = buf_addr_1;
	for (i = 0; i < ISP_LTM_STAT_TABLE_NUM; i++)
		ISP_REG_WR(idx, buf_addr + i * 4,
			ltm_stat_info.buf_info.text_point_thres[i]);

	val = (ltm_stat_info.tile_width & 0x1FF) |
		((ltm_stat_info.tile_height & 0x1FF) << 16);
	ISP_REG_MWR(idx, base + ISP_LTM_TILE_RANGE, 0x1FF01FF, val);

	val = (ltm_stat_info.clip_limit & 0xFFFF) |
		((ltm_stat_info.clip_limit_min & 0xFFFF) << 16);
	ISP_REG_WR(idx, base + ISP_LTM_CLIP_LIMIT, val);

	val = (ltm_stat_info.text_proportion & 0x1F) |
		((ltm_stat_info.text_point_thres & 0x3F) << 16);
	ISP_REG_WR(idx, base + ISP_LTM_THRES, val);

	val = ltm_stat_info.ddr_pitch & 0xFFFF;
	ISP_REG_MWR(idx, base + ISP_LTM_PITCH, 0xFFFF, val);

	return ret;
}

static int isp_k_ltm_stat_slice(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int base = 0;
	struct isp_dev_ltm_stat_slice_info slice_info = {0};

	ret = copy_from_user((void *)&slice_info,
		param->property_param, sizeof(slice_info));
	if (ret != 0) {
		pr_err("fail to check param, ret = 0x%x\n", (unsigned int)ret);
		return -1;
	}

	switch (param->sub_block) {
	case ISP_BLOCK_LTM_RGB_STAT:
		base = ISP_LTM_STAT_RGB_BASE;
		break;
	case ISP_BLOCK_LTM_YUV_STAT:
		base = ISP_LTM_STAT_YUV_BASE;
		break;
	default:
		pr_err("fail to check cmd id:%d, not supported.\n",
			param->sub_block);
		return -1;
	}

	val = (slice_info.roi_start_x & 0x1FFF) |
		((slice_info.roi_start_y & 0x1FFF) << 16);
	ISP_REG_WR(idx, base + ISP_LTM_ROI_START, val);
	val = ((slice_info.tile_num_x_minus1 & 0x7) << 12) |
		((slice_info.tile_num_y_minus1 & 0x7) << 28);
	ISP_REG_MWR(idx, base + ISP_LTM_TILE_RANGE, 0x70007000, val);
	val = slice_info.ddr_addr;
	ISP_REG_WR(idx, base + ISP_LTM_ADDR, val);
	val = (slice_info.ddr_wr_num & 0x1FF) << 16;
	ISP_REG_MWR(idx, base + ISP_LTM_PITCH, 0x1FF0000, val);

	return ret;
}

static int isp_k_ltm_stat_buf_sel(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int base = 0;

	ret = copy_from_user((void *)&val,
		param->property_param, sizeof(val));
	if (ret != 0) {
		pr_err("fail to check param, ret = 0x%x\n",
			(unsigned int)ret);
		return -1;
	}

	switch (param->sub_block) {
	case ISP_BLOCK_LTM_RGB_STAT:
		base = ISP_LTM_STAT_RGB_BASE;
		break;
	case ISP_BLOCK_LTM_YUV_STAT:
		base = ISP_LTM_STAT_YUV_BASE;
		break;
	default:
		pr_err("fail to check cmd, id:%d, not supported.\n",
			param->sub_block);
		return -1;
	}

	ISP_REG_MWR(idx, base + ISP_LTM_PARAMETERS, BIT_5, val << 5);

	return ret;
}

static int isp_k_ltm_stat_pingpong_buf(struct isp_io_param *param,
	enum isp_id idx)
{
	int ret = 0;
	unsigned int base = 0;
	unsigned int buf_addr_0 = 0;
	unsigned int buf_addr_1 = 0;
	struct isp_dev_ltm_stat_buf_info buf_info = {0};
	unsigned int buf_addr = 0;
	int i = 0;

	ret = copy_from_user((void *)&buf_info,
		param->property_param, sizeof(buf_info));
	if (ret != 0) {
		pr_err("fail to copy param, ret = 0x%x\n",
			(unsigned int)ret);
		return -1;
	}

	switch (param->sub_block) {
	case ISP_BLOCK_LTM_RGB_STAT:
		base = ISP_LTM_STAT_RGB_BASE;
		buf_addr_0 = ISP_LTM_RGB_STAT_BUF0_ADDR;
		buf_addr_1 = ISP_LTM_RGB_STAT_BUF1_ADDR;
		break;
	case ISP_BLOCK_LTM_YUV_STAT:
		base = ISP_LTM_STAT_YUV_BASE;
		buf_addr_0 = ISP_LTM_YUV_STAT_BUF0_ADDR;
		buf_addr_1 = ISP_LTM_YUV_STAT_BUF1_ADDR;
		break;
	default:
		pr_err("fail to check cmd id:%d, not supported.\n",
			param->sub_block);
		return -1;
	}

	ISP_REG_MWR(idx, base + ISP_LTM_PARAMETERS,
		BIT_5, buf_info.buf_sel << 5);

	if (buf_info.buf_sel == ISP_FRGB_LTM_STAT_BUF0)
		buf_addr = buf_addr_0;
	else
		buf_addr = buf_addr_1;
	for (i = 0; i < ISP_LTM_STAT_TABLE_NUM; i++)
		ISP_REG_WR(idx, buf_addr + i * 4, buf_info.text_point_thres[i]);

	return ret;
}

int isp_k_cfg_ltm_stat(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to check param\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_err("fail to check param\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_LTM_STAT_BLOCK:
		ret = isp_k_ltm_stat_block(param, idx);
		break;
	case ISP_PRO_LTM_STAT_SLICE:
		ret = isp_k_ltm_stat_slice(param, idx);
		break;
	case ISP_PRO_LTM_STAT_PINGPONG_BUF:
		ret = isp_k_ltm_stat_pingpong_buf(param, idx);
		break;
	case ISP_PRO_LTM_STAT_BUF_SEL:
		ret = isp_k_ltm_stat_buf_sel(param, idx);
		break;
	default:
		pr_err("fail to check cmd id:%d, not supported.\n",
			param->property);
		break;
	}

	return ret;
}
