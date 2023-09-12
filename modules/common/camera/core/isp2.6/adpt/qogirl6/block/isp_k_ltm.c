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
#include "cam_block.h"
#include "isp_ltm.h"
#include "sprd_isp_2v6.h"
#include "cam_queue.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_K_LTM: %d %d %s : " fmt, current->pid, __LINE__, __func__

#define ISP_LTM_HIST_BUF0                0

static void isp_ltm_config_hists(uint32_t idx,
	enum isp_ltm_region ltm_id, struct isp_ltm_hists *hists)
{
	unsigned int val, i;
	unsigned int base = 0;
	unsigned int buf_addr = 0;
	unsigned int buf_addr_0 = 0;
	unsigned int buf_addr_1 = 0;

	switch (ltm_id) {
	case LTM_RGB:
		base = ISP_LTM_HIST_RGB_BASE;
		buf_addr_0 = ISP_LTM_RGB_HIST_BUF0_ADDR;
		buf_addr_1 = ISP_LTM_RGB_HIST_BUF1_ADDR;
		break;
	case LTM_YUV:
		base = ISP_LTM_HIST_YUV_BASE;
		buf_addr_0 = ISP_LTM_YUV_HIST_BUF0_ADDR;
		buf_addr_1 = ISP_LTM_YUV_HIST_BUF1_ADDR;
		break;
	default:
		pr_err("fail to get cmd id:%d, not supported.\n", ltm_id);
		return;
	}

	pr_debug("isp %d rgb ltm hist bypass %d\n", idx, hists->bypass);
	if ((g_isp_bypass[idx] >> _EISP_LTM) & 1)
		hists->bypass = 1;
	ISP_REG_MWR(idx, base + ISP_LTM_HIST_PARAM, BIT_0, hists->bypass);
	if (hists->bypass)
		return;

	val = ((hists->buf_sel & 0x1) << 5) |
		((hists->channel_sel & 0x1) << 4) |
		((hists->buf_full_mode & 0x1) << 3) |
		((hists->region_est_en & 0x1) << 2) |
		((hists->binning_en    & 0x1) << 1) |
		(hists->bypass        & 0x1);
	ISP_REG_WR(idx, base + ISP_LTM_HIST_PARAM, val);

	val = ((hists->roi_start_y & 0x1FFF) << 16) |
		(hists->roi_start_x & 0x1FFF);
	ISP_REG_WR(idx, base + ISP_LTM_ROI_START, val); /* slice */

	/* tile_num_y tile_num_x HOW TODO */
	val = ((hists->tile_num_y_minus & 0x7)   << 28) |
		((hists->tile_height      & 0x1FF) << 16) |
		((hists->tile_num_x_minus & 0x7)   << 12) |
		(hists->tile_width       & 0x1FF);
	ISP_REG_WR(idx, base + ISP_LTM_TILE_RANGE, val); /* slice */

	val = ((hists->clip_limit_min & 0xFFFF) << 16) |
		(hists->clip_limit     & 0xFFFF);
	ISP_REG_WR(idx, base + ISP_LTM_CLIP_LIMIT, val);

	val = hists->texture_proportion & 0x1F;
	ISP_REG_WR(idx, base + ISP_LTM_THRES, val);

	val = hists->addr;
	ISP_REG_WR(idx, base + ISP_LTM_ADDR, val); /* slice */

	val = ((hists->wr_num & 0x1FF) << 16) |
		(hists->pitch & 0xFFFF);
	ISP_REG_WR(idx, base + ISP_LTM_PITCH, val); /* slice */

	if (hists->buf_sel == ISP_LTM_HIST_BUF0)
		buf_addr = buf_addr_0;
	else
		buf_addr = buf_addr_1;
	for (i = 0; i < LTM_HIST_TABLE_NUM; i++)
		ISP_REG_WR(idx, buf_addr + i * 4, hists->ltm_hist_table[i]);
}

static void isp_ltm_config_map(uint32_t idx,
	enum isp_ltm_region ltm_id, struct isp_ltm_map *map)
{
	unsigned int val;
	unsigned int base = 0;

	switch (ltm_id) {
	case LTM_RGB:
		base = ISP_LTM_MAP_RGB_BASE;
		break;
	case LTM_YUV:
		base = ISP_LTM_MAP_YUV_BASE;
		break;
	default:
		pr_err("fail to get cmd id:%d, not supported.\n", ltm_id);
		return;
	}

	pr_debug("isp %d rgb ltm map bypass %d\n", idx, map->bypass);
	if ((g_isp_bypass[idx] >> _EISP_LTM) & 1)
		map->bypass = 1;
	ISP_REG_MWR(idx, base + ISP_LTM_MAP_PARAM0, BIT_0, map->bypass);
	if (map->bypass)
		return;

	val = ((map->fetch_wait_line & 0x1) << 4) |
		((map->fetch_wait_en   & 0x1) << 3) |
		((map->hist_error_en   & 0x1) << 2) |
		((map->burst8_en       & 0x1) << 1) |
		(map->bypass          & 0x1);
	ISP_REG_WR(idx, base + ISP_LTM_MAP_PARAM0, val);

	val = ((map->tile_y_num  & 0x7)   << 28) |
		((map->tile_x_num  & 0x7)   << 24) |
		((map->tile_height & 0x7FF) << 12) |
		(map->tile_width  & 0x7FF);
	ISP_REG_WR(idx, base + ISP_LTM_MAP_PARAM1, val); /* slice */

	val = map->tile_size_pro & 0x3FFFFF;
	ISP_REG_WR(idx, base + ISP_LTM_MAP_PARAM2, val);

	val = ((map->tile_right_flag & 0x1)   << 31) |
		((map->tile_start_y    & 0xFFF) << 16) |
		((map->tile_left_flag  & 0x1)   << 15) |
		(map->tile_start_x    & 0xFFF);
	ISP_REG_WR(idx, base + ISP_LTM_MAP_PARAM3, val); /* slice */

	val = map->mem_init_addr;
	ISP_REG_WR(idx, base + ISP_LTM_MAP_PARAM4, val); /* slice */

	val = (map->hist_pitch & 0x7) << 24;
	ISP_REG_WR(idx, base + ISP_LTM_MAP_PARAM5, val);
}

int isp_ltm_config_param(void *handle)
{
	uint32_t idx = 0;
	struct isp_ltm_ctx_desc *ctx = NULL;
	struct isp_ltm_hists *hists = NULL;
	struct isp_ltm_map *map = NULL;

	if (!handle) {
		pr_err("fail to get invalid in ptr\n");
		return -EFAULT;
	}

	ctx = (struct isp_ltm_ctx_desc *)handle;
	idx = ctx->ctx_id;
	hists = &ctx->hists;
	map = &ctx->map;
	if (ctx->bypass) {
		hists->bypass = 1;
		map->bypass = 1;
	}

	isp_ltm_config_hists(idx, ctx->ltm_id, hists);
	isp_ltm_config_map(idx, ctx->ltm_id, map);

	return 0;
}

int isp_k_ltm_rgb_block(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_rgb_ltm_info *p = NULL;

	p = &isp_k_param->ltm_rgb_info;

	ret = copy_from_user((void *)p,
			(void __user *)param->property_param,
			sizeof(struct isp_dev_rgb_ltm_info));
	if (ret != 0) {
		pr_err("fail to get ltm from user, ret = %d\n", ret);
		return -EPERM;
	}
	isp_k_param->ltm_rgb_info.isupdate = 1;

	pr_debug("isp %d ltm hist %d map %d\n",
		idx, p->ltm_stat.bypass, p->ltm_map.bypass);

	return ret;
}

int isp_k_ltm_yuv_block(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_yuv_ltm_info *p = NULL;

	p = &isp_k_param->ltm_yuv_info;

	ret = copy_from_user((void *)p,
			(void __user *)param->property_param,
			sizeof(struct isp_dev_yuv_ltm_info));
	if (ret != 0) {
		pr_err("fail to get ltm from user, ret = %d\n", ret);
		return -EPERM;
	}
	isp_k_param->ltm_yuv_info.isupdate = 1;

	return ret;
}

int isp_k_cfg_rgb_ltm(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;

	switch (param->property) {
	case ISP_PRO_RGB_LTM_BLOCK:
		ret = isp_k_ltm_rgb_block(param, isp_k_param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}

int isp_k_cfg_yuv_ltm(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;

	switch (param->property) {
	case ISP_PRO_YUV_LTM_BLOCK:
		ret = isp_k_ltm_yuv_block(param, isp_k_param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}

int isp_k_cpy_yuv_ltm(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if(isp_k_param->ltm_yuv_info.isupdate == 1) {
		memcpy(&param_block->ltm_yuv_info, &isp_k_param->ltm_yuv_info, sizeof(struct isp_dev_yuv_ltm_info));
		isp_k_param->ltm_yuv_info.isupdate = 0;
		param_block->ltm_yuv_info.isupdate = 1;
	}

	return ret;
}

int isp_k_cpy_rgb_ltm(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if (isp_k_param->ltm_rgb_info.isupdate == 1) {
		memcpy(&param_block->ltm_rgb_info, &isp_k_param->ltm_rgb_info, sizeof(struct isp_dev_rgb_ltm_info));
		isp_k_param->ltm_rgb_info.isupdate = 0;
		param_block->ltm_rgb_info.isupdate = 1;
	}

	return ret;
}
