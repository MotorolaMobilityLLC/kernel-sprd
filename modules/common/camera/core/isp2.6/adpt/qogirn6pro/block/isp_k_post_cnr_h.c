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
#include "isp_core.h"
#include "cam_queue.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "POSTCNR: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int isp_k_post_cnr_block(struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	uint32_t i = 0, val = 0, layer_num = 0;
	struct isp_dev_post_cnr_h_info *post_cnr_h_info = NULL;
	struct isp_post_cnr_h *param_post_cnr_h = NULL;

        if (!isp_k_param) {
		pr_err("fail to get input ptr\n");
		return -1;
	}
	if (isp_k_param->post_cnr_h_info.isupdate == 0)
		return ret;

	post_cnr_h_info = &isp_k_param->post_cnr_h_info;
	param_post_cnr_h = &post_cnr_h_info->param_post_cnr_h;
	isp_k_param->post_cnr_h_info.isupdate = 0;

	layer_num = 0;
	ISP_REG_MWR(idx, ISP_YUV_CNR_CONTRL0, 0xE, layer_num << 1);
	if (g_isp_bypass[idx] & (1 << _EISP_POSTCNR))
		post_cnr_h_info->bypass = 1;

	ISP_REG_MWR(idx, ISP_YUV_CNR_CONTRL0, BIT_0, post_cnr_h_info->bypass);

	if (post_cnr_h_info->bypass) {
		pr_info("idx %d, post_cnr_bypass!\n", idx);
		return 0;
	}

	val = ((param_post_cnr_h->radius & 0xFFFF) << 16) |
		((param_post_cnr_h->minRatio & 0x3FF) << 2) |
		((param_post_cnr_h->denoise_radial_en & 0x1) << 1) |
		(param_post_cnr_h->lowpass_filter_en & 0x1);
	ISP_REG_WR(idx, ISP_YUV_CNR_CFG0, val);

	val = ((param_post_cnr_h->imgCenterY & 0xFFFF) << 16) |
		(param_post_cnr_h->imgCenterX & 0xFFFF);
	ISP_REG_WR(idx, ISP_YUV_CNR_CFG1, val);

	val = ((param_post_cnr_h->filter_size & 0x3) << 28) |
		((param_post_cnr_h->slope & 0xFFF) << 16) |
		((param_post_cnr_h->luma_th[1] & 0xFF) << 8) |
		(param_post_cnr_h->luma_th[0] & 0xFF);
	ISP_REG_WR(idx, ISP_YUV_CNR_CFG2, val);

	for (i = 0; i < 18; i++) {
		val = ((param_post_cnr_h->weight_y[0][4 * i + 3] & 0xFF) << 24) |
			((param_post_cnr_h->weight_y[0][4 * i + 2] & 0xFF) << 16) |
			((param_post_cnr_h->weight_y[0][4 * i + 1] & 0xFF) << 8) |
			(param_post_cnr_h->weight_y[0][4 * i] & 0xFF);
		ISP_REG_WR(idx, ISP_YUV_CNR_Y_L0_WHT0 + 4 * i, val);

		val = ((param_post_cnr_h->weight_y[1][4 * i + 3] & 0xFF) << 24) |
			((param_post_cnr_h->weight_y[1][4 * i + 2] & 0xFF) << 16) |
			((param_post_cnr_h->weight_y[1][4 * i + 1] & 0xFF) << 8) |
			(param_post_cnr_h->weight_y[1][4 * i] & 0xFF);
		ISP_REG_WR(idx, ISP_YUV_CNR_Y_L1_WHT0 + 4*i, val);

		val = ((param_post_cnr_h->weight_y[2][4 * i + 3] & 0xFF) << 24) |
			((param_post_cnr_h->weight_y[2][4 * i + 2] & 0xFF) << 16) |
			((param_post_cnr_h->weight_y[2][4 * i + 1] & 0xFF) << 8) |
			(param_post_cnr_h->weight_y[2][4 * i] & 0xFF);
		ISP_REG_WR(idx, ISP_YUV_CNR_Y_L2_WHT0 + 4 * i, val);

		val = ((param_post_cnr_h->weight_uv[0][4 * i + 3] & 0xFF) << 24) |
			((param_post_cnr_h->weight_uv[0][4 * i + 2] & 0xFF) << 16) |
			((param_post_cnr_h->weight_uv[0][4 * i + 1] & 0xFF) << 8) |
			(param_post_cnr_h->weight_uv[0][4 * i] & 0xFF);
		ISP_REG_WR(idx, ISP_YUV_CNR_UV_L0_WHT0 + 4 * i, val);

		val = ((param_post_cnr_h->weight_uv[1][4 * i + 3] & 0xFF) << 24) |
			((param_post_cnr_h->weight_uv[1][4 * i + 2] & 0xFF) << 16) |
			((param_post_cnr_h->weight_uv[1][4 * i + 1] & 0xFF) << 8) |
			(param_post_cnr_h->weight_uv[1][4 * i] & 0xFF);
		ISP_REG_WR(idx, ISP_YUV_CNR_UV_L1_WHT0 + 4 * i, val);

		val = ((param_post_cnr_h->weight_uv[2][4 * i + 3] & 0xFF) << 24) |
			((param_post_cnr_h->weight_uv[2][4 * i + 2] & 0xFF) << 16) |
			((param_post_cnr_h->weight_uv[2][4 * i + 1] & 0xFF) << 8) |
			(param_post_cnr_h->weight_uv[2][4 * i] & 0xFF);
		ISP_REG_WR(idx, ISP_YUV_CNR_UV_L2_WHT0 + 4 * i, val);
	}
	return ret;
}

int isp_k_cfg_post_cnr_h(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_post_cnr_h_info *post_cnr_h_info = NULL;

	post_cnr_h_info = &isp_k_param->post_cnr_h_info;

	switch (param->property) {
	case ISP_PRO_POST_CNR_H_BLOCK:
		ret = copy_from_user((void *)post_cnr_h_info,
				param->property_param,
				sizeof(struct isp_dev_post_cnr_h_info));
		if (ret != 0) {
			pr_err("fail to copy from user, ret = %d\n", ret);
			return ret;
		}
		post_cnr_h_info->isupdate = 1;
		if (g_isp_bypass[idx] & (1 << _EISP_POSTCNR))
			post_cnr_h_info->bypass = 1;
		break;
	default:
		pr_err("fail to idx %d, support cmd id = %d\n", idx, param->property);
		break;
	}

	return ret;
}

int isp_k_update_post_cnr(void *handle)
{
	int ret = 0;
	uint32_t val = 0, center_x = 0, center_y = 0;
	uint32_t radius = 0, radius_limit = 0;
	uint32_t idx = 0, new_width = 0, old_width = 0, sensor_width = 0;
	uint32_t new_height = 0, old_height = 0, sensor_height = 0;
	struct isp_dev_post_cnr_h_info *post_cnr_info = NULL;
	struct isp_post_cnr_h *param_post_cnr_info = NULL;
	struct isp_sw_context *pctx = NULL;

	if (!handle) {
		pr_err("fail to get invalid in ptr\n");
		return -EFAULT;
	}

	pctx = (struct isp_sw_context *)handle;
	idx = pctx->ctx_id;
	new_width = pctx->isp_using_param->blkparam_info.new_width;
	new_height = pctx->isp_using_param->blkparam_info.new_height;
	old_width = pctx->isp_using_param->blkparam_info.old_width;
	old_height = pctx->isp_using_param->blkparam_info.old_height;
	sensor_width = pctx->uinfo.sn_size.w;
	sensor_height = pctx->uinfo.sn_size.h;

	post_cnr_info = &pctx->isp_using_param->post_cnr_h_info;
	param_post_cnr_info = &post_cnr_info->param_post_cnr_h;

	if (post_cnr_info->bypass)
		return 0;

	center_x = new_width >> 1;
	center_y = new_height >> 1;
	val = (center_y << 16) | center_x;
	ISP_REG_WR(idx, ISP_YUV_CNR_CFG1, val);

	param_post_cnr_info->imgCenterX = center_x;
	param_post_cnr_info->imgCenterY = center_y;

	if (post_cnr_info->radius_base == 0)
		post_cnr_info->radius_base = 1024;

	radius = ((sensor_width+sensor_height) / 2) * param_post_cnr_info->base_radius_factor / post_cnr_info->radius_base;
	radius_limit = (new_width+new_height ) / 2;
	radius = (radius < radius_limit) ? radius : radius_limit;
	radius = new_height * radius / old_height;

	param_post_cnr_info->radius = radius;
	val = ((param_post_cnr_info->radius & 0xFFFF) << 16) |
		((param_post_cnr_info->minRatio & 0x3FF) << 2) |
		((param_post_cnr_info->denoise_radial_en & 0x1) << 1) |
		(param_post_cnr_info->lowpass_filter_en & 0x1);
	ISP_REG_WR(idx, ISP_YUV_CNR_CFG0, val);
	pr_debug("isp%d,cen %d %d, base %d, factor %d, radius %d\n",
		pctx->ctx_id, center_x, center_y, post_cnr_info->radius_base, param_post_cnr_info->base_radius_factor, radius);

	return ret;
}

int isp_k_cpy_post_cnr_h(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if (isp_k_param->post_cnr_h_info.isupdate == 1) {
		memcpy(&param_block->post_cnr_h_info, &isp_k_param->post_cnr_h_info, sizeof(struct isp_dev_post_cnr_h_info));
		isp_k_param->post_cnr_h_info.isupdate = 0;
		param_block->post_cnr_h_info.isupdate = 1;
	}

	return ret;
}
