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
#include "isp_dewarping.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "DEWARPING: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int isp_dewarping_dewarp_cache_set(void *handle)
{
	uint32_t val = 0x7;
	uint32_t ret = 0;
	uint32_t idx;
	struct isp_dewarp_cache_info *dewarp_cache = NULL;

	if (!handle) {
		pr_err("fail to get valid arg\n");
		return -EFAULT;
	}
	dewarp_cache = (struct isp_dewarp_cache_info *)handle;
	idx = dewarp_cache->ctx_id;
	pr_debug("enter: fmt:%d, pitch:%d, is_pack:%d\n", dewarp_cache->yuv_format,
			dewarp_cache->frame_pitch, dewarp_cache->dewarp_cache_mipi);
	if (g_isp_bypass[idx] & (1 << _EISP_DEWARP))
		dewarp_cache->dewarp_cache_bypass = 1;

	ISP_REG_MWR(idx, ISP_DEWARPING_CACHE_PARA, BIT_0, dewarp_cache->dewarp_cache_bypass);

	if (dewarp_cache->dewarp_cache_bypass)
		return 0;

	ISP_REG_MWR(idx, ISP_COMMON_SCL_PATH_SEL, BIT_10 | BIT_11 , dewarp_cache->fetch_path_sel << 10);

	val = ((dewarp_cache->dewarp_cache_endian & 0x3) << 1) | ((dewarp_cache->dewarp_cache_prefetch_len & 0x7) << 3) |
		((dewarp_cache->dewarp_cache_mipi & 0x1) << 6) | ((dewarp_cache->yuv_format & 0x1) << 7);
	ISP_REG_MWR(idx, ISP_DEWARPING_CACHE_PARA, 0xFE, val);
	ISP_REG_WR(idx, ISP_DEWARPING_CACHE_FRAME_WIDTH, dewarp_cache->frame_pitch);
	ISP_REG_WR(idx, ISP_DEWARPING_CACHE_FRAME_YADDR, dewarp_cache->addr.addr_ch0);
	ISP_REG_WR(idx, ISP_DEWARPING_CACHE_FRAME_UVADDR, dewarp_cache->addr.addr_ch1);
	return ret;
}

static int isp_dewarping_coef_config(void *handle)
{
	uint32_t val = 0;
	uint32_t ret = 0;
	uint32_t i = 0;
	uint32_t idx;
	struct isp_dewarping_blk_info *dewarp_ctx;

	if (!handle) {
		pr_err("fail to dewarping_config_reg parm NULL\n");
		return -EFAULT;
	}
	dewarp_ctx = (struct isp_dewarping_blk_info *)handle;
	idx = dewarp_ctx->cxt_id;

	/* wait otp and other coeff format */
	for (i = 0; i < dewarp_ctx->grid_data_size; i++) {
		val =dewarp_ctx->grid_x_ch0_buf[i];
		ISP_REG_WR(idx, ISP_DEWARPING_GRID_X_CH0 + i * 4, val);
	}

	for (i = 0; i < dewarp_ctx->grid_data_size; i++) {
		val = dewarp_ctx->grid_y_ch0_buf[i];
		ISP_REG_WR(idx, ISP_DEWARPING_GRID_Y_CH0 + i * 4, val);
	}

	for(i = 0; i < MAX_GRID_SIZE; i++) {
		val = (dewarp_ctx->bicubic_coef_i[3 * i + 0] & 0xFFFF) | ((dewarp_ctx->bicubic_coef_i[3 * i + 1] & 0xFFFF) << 16);
		ISP_REG_WR(idx, ISP_DEWARPING_CORD_COEF_CH0 + 2 *  i * 4, val);
		val = dewarp_ctx->bicubic_coef_i[3 * i + 2] & 0xFFFF ;
		ISP_REG_MWR(idx, ISP_DEWARPING_CORD_COEF_CH0 + (2 * i + 1) * 4, 0xFFFF, val);
	}

	for(i = 0; i < LXY_MULT; i++) {
		val = (dewarp_ctx->pixel_interp_coef[3 * i + 0] & 0xFFFF) | ((dewarp_ctx->pixel_interp_coef[3 * i + 1] & 0xFFFF) << 16);
		ISP_REG_WR(idx, ISP_DEWARPING_PXL_COEF_CH0 + 2 * i * 4, val);
		val = dewarp_ctx->pixel_interp_coef[3 * i + 2] & 0xFFFF;
		ISP_REG_MWR(idx, ISP_DEWARPING_PXL_COEF_CH0 + (2 * i + 1) * 4, 0xFFFF, val);
	}

	return ret;
}


int isp_dewarping_frame_config(void *handle)
{
	uint32_t val = 0;
	uint32_t ret = 0;
	uint32_t idx;
	struct isp_dewarping_blk_info *dewarp_ctx;

	if (!handle) {
		pr_err("fail to dewarping_config_reg parm NULL\n");
		return -EFAULT;
	}
	dewarp_ctx = (struct isp_dewarping_blk_info *)handle;
	idx = dewarp_ctx->cxt_id;

	val = ((dewarp_ctx->grid_size & 0x1ff) << 0);
	ISP_REG_WR(idx, ISP_DEWARPING_GRID_SIZE, val);

	val = ((dewarp_ctx->grid_num_x & 0x3f) << 16) |
		((dewarp_ctx->grid_num_y & 0x3f) << 0);
	ISP_REG_WR(idx, ISP_DEWARPING_GRID_NUM, val);

	val = ((dewarp_ctx->pos_x & 0x1fff) << 16) |
		((dewarp_ctx->pos_y & 0x1fff) << 0);
	ISP_REG_WR(idx, ISP_DEWARPING_POS, val);

	val = ((dewarp_ctx->dst_width& 0x1fff) << 16) |
		((dewarp_ctx->dst_height& 0x1fff) << 0);
	ISP_REG_WR(idx, ISP_DEWARPING_DST_SIZE, val);

	val = ((dewarp_ctx->src_width & 0x1fff) << 16) |
		((dewarp_ctx->src_height & 0x1fff) << 0);
	ISP_REG_WR(idx, ISP_DEWARPING_SRC_SIZE, val);

	val = ((dewarp_ctx->dewarping_lbuf_ctrl_nfull_size & 0xf) << 0);
	ISP_REG_WR(idx, ISP_DEWARPING_NFULL_SIZE, val);

	val = ((dewarp_ctx->start_mb_x & 0x3ff) << 0);
	ISP_REG_WR(idx, ISP_DEWARPING_SLICE_START, val);

	val = ((dewarp_ctx->mb_y_num & 0x3ff) << 16) |
		((dewarp_ctx->mb_x_num & 0x3ff) << 0);
	ISP_REG_WR(idx, ISP_DEWARPING_MB_NUM, val);

	val = ((dewarp_ctx->init_start_row & 0x1fff) << 16) |
		((dewarp_ctx->init_start_col & 0x1fff) << 0);
	ISP_REG_WR(idx, ISP_DEWARPING_INIT_OFFSET, val);

	val = ((dewarp_ctx->chk_wrk_mode & 0x1) << 1) |
		((dewarp_ctx->chk_clr_mode & 0x1) << 0);
	ISP_REG_WR(idx, ISP_DEWARPING_CHK_SUM, val);

	val = ((dewarp_ctx->crop_start_x & 0xffff) << 16) |
		((dewarp_ctx->crop_start_y & 0xffff) << 0);
	ISP_REG_WR(idx, ISP_DEWARPING_CROP_PARA, val);

	isp_dewarping_coef_config(dewarp_ctx);

	return ret;
}

static int isp_k_dewarping_slice_param_set(void *handle)
{
	uint32_t addr = 0, cmd = 0;
	struct isp_fmcu_ctx_desc *fmcu = NULL;
	struct isp_dewarp_ctx_desc *ctx = NULL;
	struct isp_dewarping_slice *dewarp_slc = NULL;

	if (!handle) {
		pr_err("fail to dewarp_config_reg parm NULL\n");
		return -EFAULT;
	}

	ctx = (struct isp_dewarp_ctx_desc *)handle;
	dewarp_slc = &ctx->slice_info[ctx->cur_slice_id];
	fmcu = (struct isp_fmcu_ctx_desc *)ctx->fmcu_handle;

	addr = ISP_GET_REG(ISP_DEWARPING_DST_SIZE);
	cmd = ((dewarp_slc->dewarp_slice.dst_width & 0x1fff) << 16) |
		((dewarp_slc->dewarp_slice.dst_height & 0x1fff) << 0);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_DEWARPING_SRC_SIZE);
	cmd = ((dewarp_slc->dewarp_slice.slice_width & 0x1fff) << 16) |
		((dewarp_slc->dewarp_slice.slice_height & 0x1fff) << 0);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_DEWARPING_SLICE_START);
	cmd = ((dewarp_slc->dewarp_slice.start_mb_x & 0x3ff) << 0);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_DEWARPING_MB_NUM);
	cmd = ((dewarp_slc->dewarp_slice.mb_y_num & 0x3ff) << 16) |
		((dewarp_slc->dewarp_slice.mb_x_num & 0x3ff) << 0);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_DEWARPING_INIT_OFFSET);
	cmd = ((dewarp_slc->dewarp_slice.init_start_row & 0x1fff) << 16) |
		((dewarp_slc->dewarp_slice.init_start_col & 0x1fff) << 0);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_DEWARPING_CROP_PARA);
	cmd = ((dewarp_slc->dewarp_slice.crop_start_x & 0xffff) << 16) |
		((dewarp_slc->dewarp_slice.crop_start_y & 0xffff) << 0);
	FMCU_PUSH(fmcu, addr, cmd);

	return 0;
}

int isp_k_dewarping_slice_config(void *handle)
{
	int ret = 0;
	struct isp_dewarp_ctx_desc *ctx = NULL;
	struct isp_dewarping_slice *dewarp_slc = NULL;
	struct isp_hw_fmcu_cfg fmcu_cfg;
	struct isp_hw_slices_fmcu_cmds parg;

	if (!handle) {
		pr_err("fail to dewarp_config_reg parm NULL\n");
		return -EFAULT;
	}

	ctx = (struct isp_dewarp_ctx_desc *)handle;
	dewarp_slc = &ctx->slice_info[ctx->cur_slice_id];

	if (ctx->wmode == ISP_CFG_MODE) {
		fmcu_cfg.fmcu = ctx->fmcu_handle;
		fmcu_cfg.ctx_id = ctx->hw_ctx_id;
		ctx->hw->isp_ioctl(ctx->hw, ISP_HW_CFG_FMCU_CFG, &fmcu_cfg);
	}

	isp_k_dewarping_slice_param_set(handle);

	parg.wmode = ctx->wmode;
	parg.hw_ctx_id = ctx->hw_ctx_id;
	parg.fmcu = ctx->fmcu_handle;
	ctx->hw->isp_ioctl(ctx->hw, ISP_HW_CFG_SLICE_FMCU_DEWARP_CMD, &parg);

	return ret;
}
