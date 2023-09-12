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

#include "isp_reg.h"
#include "isp_dec_int.h"
#include "isp_pyr_dec.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "PYR_K_DEC: %d %d %s : "fmt, current->pid, __LINE__, __func__

typedef void(*isp_dec_isr)(void *param);

static const uint32_t isp_dec_irq_process[] = {
	ISP_INT_DEC_FMCU_CONFIG_DONE,
};

static void isppyrdec_fmcu_config_done(void *dec_handle)
{
	struct isp_dec_pipe_dev *dev = NULL;
	dev = (struct isp_dec_pipe_dev *)dec_handle;

	if (!dev) {
		pr_err("fail to get valid dec_handle\n");
		return;
	}

	if (dev->irq_proc_func)
		dev->irq_proc_func(dev);

	pr_debug("dec fmcu config done\n");
}

static isp_dec_isr isp_dec_isr_handler[32] = {
	[ISP_INT_DEC_FMCU_CONFIG_DONE] = isppyrdec_fmcu_config_done,
};

static irqreturn_t isppyrdec_isr_root(int irq, void *priv)
{
	uint32_t irq_line = 0, k = 0;
	uint32_t err_mask = 0;
	uint32_t irq_numbers = 0;
	uint32_t cur_ctx_id = 0;
	struct isp_dec_pipe_dev *ctx = (struct isp_dec_pipe_dev *)priv;
	struct isp_dec_sw_ctx *pctx = NULL;

	if (!ctx) {
		pr_err("fail to get valid dev\n");
		return IRQ_HANDLED;
	}

	if (unlikely(irq != ctx->irq_no)) {
		pr_err("fail to match isppyr dec irq %d %d\n", irq, ctx->irq_no);
		return IRQ_NONE;
	}

	irq_numbers = ARRAY_SIZE(isp_dec_irq_process);
	err_mask = ISP_DEC_INT_LINE_MASK_ERR;
	irq_line = ISP_HREG_RD(ISP_DEC_INT_BASE + ISP_INT_INT0);

	cur_ctx_id = ctx->cur_ctx_id;
	if (&ctx->sw_ctx[cur_ctx_id] == NULL) {
		pr_err("fail to get sw_ctx\n");
		return IRQ_HANDLED;
	} else {
		pctx = &ctx->sw_ctx[cur_ctx_id];
		pctx->in_irq_handler = 1;
	}
	ISP_HREG_WR(ISP_DEC_INT_BASE + ISP_INT_CLR0, irq_line);

	if (atomic_read(&ctx->proc_eb) < 1) {
		pr_err("fail to eb dec proc, ctx %d stopped, irq 0x%x\n",
			cur_ctx_id, irq_line);
		pctx->in_irq_handler = 0;
		return IRQ_HANDLED;
	}

	pr_debug("isp pyr dec irq status:%d\n", irq_line);
	if (unlikely(err_mask & irq_line)) {
		pr_err("fail to get normal isp dec status 0x%x\n", irq_line);
		/* print isp dec reg info here */
		return IRQ_HANDLED;
	}

	for (k = 0; k < irq_numbers; k++) {
		uint32_t irq_id = isp_dec_irq_process[k];

		if (irq_line & (1 << irq_id)) {
			if (isp_dec_isr_handler[irq_id]) {
				isp_dec_isr_handler[irq_id](ctx);
			}
		}
		irq_line &= ~(1 << irq_id);
		if (!irq_line)
			break;
	}
	pctx->in_irq_handler = 0;

	return IRQ_HANDLED;
}

int isp_pyr_dec_irq_func(void *handle)
{
	int ret = 0;
	struct isp_dec_pipe_dev *ctx = NULL;

	if (!handle) {
		pr_err("fail to isp_dec handle NULL\n");
		return -EFAULT;
	}

	ctx = (struct isp_dec_pipe_dev *)handle;
	ctx->isr_func = isppyrdec_isr_root;

	return ret;
}

static int isppyrdec_cfg_fetch(struct isp_dec_pipe_dev *ctx)
{
	int ret = 0;
	uint32_t addr = 0, cmd = 0, base = 0, color_format = 0;
	struct isp_fmcu_ctx_desc *fmcu = NULL;
	struct isp_dec_fetch_info *dec_fetch = NULL;
	struct isp_fbd_yuv_info *dec_afbd_fetch = NULL;
	struct slice_fetch_info *fetch_slc = NULL;

	if (ctx == NULL) {
		pr_err("fail to get isp dec ctx.\n");
		return -EFAULT;
	}

	dec_fetch = &ctx->fetch_dec_info;
	dec_afbd_fetch = &ctx->yuv_afbd_info;
	fmcu = (struct isp_fmcu_ctx_desc *)ctx->fmcu_handle;
	fetch_slc = &ctx->slices[ctx->cur_slice_id].slice_fetch;

	switch (dec_fetch->color_format) {
	case ISP_FETCH_YVU420_2FRAME_10:
	case ISP_FETCH_YVU420_2FRAME_MIPI:
		color_format = 1;
		break;
	case ISP_FETCH_YVU420_2FRAME:
		color_format = 3;
		break;
	case ISP_FETCH_YUV420_2FRAME_10:
	case ISP_FETCH_YUV420_2FRAME_MIPI:
		color_format = 0;
		break;
	case ISP_FETCH_YUV420_2FRAME:
		color_format = 2;
		break;
	case ISP_FETCH_FULL_RGB10:
		color_format = 4;
		break;
	default:
		pr_err("fail to get isp fetch format:%d\n", dec_fetch->color_format);
		break;
	}

	if (ctx->cur_layer_id == 0 && ctx->fetch_path_sel) {
		base = ISP_YUV_DEC_AFBD_FETCH_BASE;

		if (ctx->yuv_afbd_info.data_bits == DCAM_STORE_8_BIT)
			ctx->yuv_afbd_info.afbc_mode = AFBD_FETCH_8BITS;
		else if (ctx->yuv_afbd_info.data_bits == DCAM_STORE_10_BIT)
			ctx->yuv_afbd_info.afbc_mode = AFBD_FETCH_10BITS;
		else
			pr_debug("No use afbc mode.\n");

		addr = ISP_GET_REG(ISP_COMMON_SCL_PATH_SEL);
		cmd = ISP_REG_RD(ctx->cur_ctx_id, ISP_COMMON_SCL_PATH_SEL);
		cmd = cmd | (1 << 14);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_FETCH_PARAM0) + PYR_DEC_FETCH_BASE;
		cmd = 0x1;
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_AFBD_FETCH_SEL) + base;
		cmd = (ctx->yuv_afbd_info.fetch_fbd_bypass & 0x1) |
			(0x1 << 1) | (0x1 << 3) | ((ctx->yuv_afbd_info.afbc_mode & 0x1F) << 4);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_AFBD_FETCH_HBLANK_TILE_PITCH ) + base;
		cmd = ((ctx->yuv_afbd_info.tile_num_pitch & 0x7FF) << 16) | (0x8000);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_AFBD_FETCH_SLICE_SIZE) + base;
		cmd = ((fetch_slc->fetch_fbd.slice_size.h & 0xFFFF) << 16) |
			(fetch_slc->fetch_fbd.slice_size.w & 0xFFFF);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_AFBD_FETCH_PARAM0) + base;
		cmd = fetch_slc->fetch_fbd.slice_start_pxl_xpt |
			fetch_slc->fetch_fbd.slice_start_pxl_ypt << 16;
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_AFBD_FETCH_PARAM1) + base;
		cmd = ctx->yuv_afbd_info.frame_header_base_addr;
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_AFBD_FETCH_PARAM2) + base;
		cmd = fetch_slc->fetch_fbd.slice_start_header_addr;
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_DISPATCH_CH0_SIZE) + PYR_DEC_DISPATCH_BASE;
		cmd = ((fetch_slc->fetch_fbd.slice_size.h & 0xFFFF) << 16) |
			(fetch_slc->fetch_fbd.slice_size.w & 0xFFFF);
		FMCU_PUSH(fmcu, addr, cmd);
	} else {
		addr = ISP_GET_REG(ISP_COMMON_SCL_PATH_SEL);
		cmd = ISP_REG_RD(ctx->cur_ctx_id, ISP_COMMON_SCL_PATH_SEL);
		cmd = cmd & 0xFFFF3FFF;
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_AFBD_FETCH_SEL) + ISP_YUV_DEC_AFBD_FETCH_BASE;
		cmd = 0x1;
		FMCU_PUSH(fmcu, addr, cmd);

		base = PYR_DEC_FETCH_BASE;
		addr = ISP_GET_REG(ISP_FETCH_PARAM0) + base;
		cmd =((dec_fetch->chk_sum_clr_en & 0x1) << 11) |
			((dec_fetch->ft1_axi_reorder_en & 0x1) << 9) |
			((dec_fetch->ft0_axi_reorder_en & 0x1) << 8) |
			((color_format & 0x7) << 4) |
			((dec_fetch->substract & 0x1) << 1) |
			((dec_fetch->bypass & 0x1) << 0);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_FETCH_MEM_SLICE_SIZE) + base;
		cmd = ((fetch_slc->size.h & 0xFFFF) << 16) | (fetch_slc->size.w & 0xFFFF);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_FETCH_SLICE_Y_PITCH) + base;
		cmd = dec_fetch->pitch[0];
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_FETCH_SLICE_Y_ADDR) + base;
		cmd = fetch_slc->addr.addr_ch0;
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_FETCH_SLICE_U_PITCH) + base;
		cmd = dec_fetch->pitch[1];
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_FETCH_SLICE_U_ADDR) + base;
		cmd = fetch_slc->addr.addr_ch1;
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_FETCH_MIPI_PARAM) + base;
		cmd = (fetch_slc->mipi_word_num & 0xFFFF) |
			((fetch_slc->mipi_byte_rel_pos & 0xF) << 16) |
			((fetch_slc->mipi10_en & 0x1) << 20);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_FETCH_MIPI_PARAM_UV) + base;
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_DISPATCH_CH0_SIZE) + PYR_DEC_DISPATCH_BASE;
		cmd = ((fetch_slc->size.h & 0xFFFF) << 16)
			| (fetch_slc->size.w & 0xFFFF);
		FMCU_PUSH(fmcu, addr, cmd);
	}
	return ret;
}

static int isppyrdec_cfg_store(struct isp_dec_pipe_dev *ctx, uint32_t store_block)
{
	int ret = 0;
	uint32_t addr = 0, cmd = 0, base = 0;
	uint32_t color_format = 0, data_10b = 0;
	struct isp_fmcu_ctx_desc *fmcu = NULL;
	struct isp_dec_store_info *store = NULL;
	struct slice_store_info *store_slc = NULL;

	fmcu = (struct isp_fmcu_ctx_desc *)ctx->fmcu_handle;
	switch (store_block) {
	case ISP_PYR_DEC_STORE_DCT:
		base = PYR_DCT_STORE_BASE;
		store = &ctx->store_dct_info;
		store_slc = &ctx->slices[ctx->cur_slice_id].slice_dct_store;
		break;
	case ISP_PYR_DEC_STORE_DEC:
		base = PYR_DEC_STORE_BASE;
		store = &ctx->store_dec_info;
		store_slc = &ctx->slices[ctx->cur_slice_id].slice_dec_store;
		break;
	default:
		pr_err("fail to support rec fetch %d.\n", store_block);
		return -EFAULT;
	}

	switch (store->color_format) {
	case ISP_FETCH_YUV420_2FRAME_MIPI:
		color_format = 0xC;
		data_10b = 1;
		break;
	case ISP_FETCH_YVU420_2FRAME_MIPI:
		color_format = 0xD;
		data_10b = 1;
		break;
	case ISP_FETCH_YUV420_2FRAME_10:
		color_format = 0x4;
		data_10b = 1;
		break;
	case ISP_FETCH_YVU420_2FRAME_10:
		color_format = 0x5;
		data_10b = 1;
		break;
	case ISP_FETCH_YVU420_2FRAME:
		color_format = 0x5;
		data_10b = 0;
		break;
	default:
		data_10b = 0;
		pr_err("fail to support color foramt %d.\n", store->color_format);
	}

	addr = ISP_GET_REG(ISP_STORE_PARAM) + base;
	cmd = ((store->bypass & 0x1) << 0) |
		((store->burst_len & 0x1) << 1) |
		((store->speed2x & 0x1) << 2) |
		((store->mirror_en & 0x1) <<3) |
		((color_format & 0xf) << 4) |
		((store->mipi_en& 0x1) << 7) |
		((store->endian & 0x3) << 8) |
		((store->mono_en & 0x1) << 10) |
		((data_10b & 0x1) << 11) |
		((store->flip_en & 0x1) << 12) |
		((store->last_frm_en & 0x3) << 13);
	FMCU_PUSH(fmcu, addr, cmd);

	if (store->bypass)
		return 0;

	addr = ISP_GET_REG(ISP_STORE_SLICE_SIZE) + base;
	cmd = ((store_slc->size.h & 0xFFFF) << 16) | (store_slc->size.w & 0xFFFF);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_STORE_BORDER) + base;
	cmd =  (store_slc->border.left_border & 0xFFFF) |
		((store_slc->border.right_border & 0xFFFF) << 16);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_STORE_BORDER_1) + base;
	cmd = (store_slc->border.up_border & 0xFFFF) |
		((store_slc->border.down_border & 0xFFFF) << 16);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_STORE_SLICE_Y_ADDR) + base;
	cmd = store_slc->addr.addr_ch0;
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_STORE_Y_PITCH) + base;
	cmd = store->pitch[0];
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_STORE_SLICE_U_ADDR) + base;
	cmd = store_slc->addr.addr_ch1;
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_STORE_U_PITCH) + base;
	cmd = store->pitch[1];
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_STORE_SHADOW_CLR) + base;
	cmd = 1;
	FMCU_PUSH(fmcu, addr, cmd);

	return ret;
}

static int isppyrdec_cfg_offline(struct isp_dec_pipe_dev *ctx)
{
	int ret = 0;
	uint32_t addr = 0, cmd = 0;
	struct isp_fmcu_ctx_desc *fmcu = NULL;
	struct isp_dec_offline_info *dec_off_info = NULL;
	struct slice_pyr_dec_info *pyr_dec_slc = NULL;
	struct cam_hw_info *hw = NULL;

	fmcu = (struct isp_fmcu_ctx_desc *)ctx->fmcu_handle;
	dec_off_info = &ctx->offline_dec_info;
	pyr_dec_slc = &ctx->slices[ctx->cur_slice_id].slice_pyr_dec;
	hw = ctx->hw;

	if (ctx->cur_layer_id == 0 && ctx->fetch_path_sel) {
		addr = ISP_GET_REG(ISP_DEC_OFFLINE_PARAM);
		cmd = ((dec_off_info->fmcu_path_sel & 0x1) << 7) |
			((1 & 0x1) << 6) |
			((dec_off_info->vector_channel_idx & 0x7) << 3) |
			((dec_off_info->chksum_wrk_mode & 0x1) << 2) |
			((dec_off_info->chksum_clr_mode & 0x1) << 1);
		FMCU_PUSH(fmcu, addr, cmd);
	} else {
		addr = ISP_GET_REG(ISP_DEC_OFFLINE_PARAM);
		cmd = ((dec_off_info->fmcu_path_sel & 0x1) << 7) |
			((dec_off_info->fetch_path_sel & 0x1) << 6) |
			((dec_off_info->vector_channel_idx & 0x7) << 3) |
			((dec_off_info->chksum_wrk_mode & 0x1) << 2) |
			((dec_off_info->chksum_clr_mode & 0x1) << 1);
		FMCU_PUSH(fmcu, addr, cmd);
	}

	addr = ISP_GET_REG(ISP_DEC_OFFLINE_PARAM1);
	cmd = (pyr_dec_slc->hor_padding_en & 0x1) |
		((pyr_dec_slc->hor_padding_num & 0x7FFF) <<1) |
		((pyr_dec_slc->ver_padding_en & 0x1) <<16) |
		((pyr_dec_slc->ver_padding_num & 0x7F) <<17);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_DISPATCH_DLY) + PYR_DEC_DISPATCH_BASE;
	cmd = ((pyr_dec_slc->dispatch_dly_height_num & 0xFFFF) << 16)
		| (pyr_dec_slc->dispatch_dly_width_num & 0xFFFF);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_DISPATCH_LINE_DLY1) + PYR_DEC_DISPATCH_BASE;
	cmd = (dec_off_info->dispatch_width_dly_num_flash & 0xFFFF) |
		((dec_off_info->dispatch_done_cfg_mode & 0x1) << 16) |
		((dec_off_info->dispatch_yuv_start_row_num & 0xFF) << 20)|
		((dec_off_info->dispatch_yuv_start_order & 1) << 28)|
		((dec_off_info->dispatch_dbg_mode_ch0 & 1) << 30)|
		((dec_off_info->dispatch_width_flash_mode & 1) << 31);
	FMCU_PUSH(fmcu, addr, cmd);

	addr = ISP_GET_REG(ISP_DISPATCH_PIPE_BUF_CTRL_CH0) + PYR_DEC_DISPATCH_BASE;
	cmd = (dec_off_info->dispatch_pipe_hblank_num & 0xFF) |
		((dec_off_info->dispatch_pipe_flush_num & 0xFF) << 8)
		| ((dec_off_info->dispatch_pipe_nfull_num & 0x7FF) << 16);
	FMCU_PUSH(fmcu, addr, cmd);

	if (ctx->fetch_path_sel && ctx->cur_layer_id == 0) {
		addr = ISP_GET_REG(ISP_AFBD_FETCH_START) + ISP_YUV_DEC_AFBD_FETCH_BASE;
		cmd = 1;
		FMCU_PUSH(fmcu, addr, cmd);
	} else {
		/* for normal fetch if use fbd need update fetch start & common sel */
		addr = ISP_GET_REG(ISP_FETCH_START) + PYR_DEC_FETCH_BASE;
		cmd = 1;
		FMCU_PUSH(fmcu, addr, cmd);
	}

	addr = ISP_GET_REG(ISP_FMCU_CMD);
	cmd = PRE0_ALL_DONE;
	FMCU_PUSH(fmcu, addr, cmd);

	hw->isp_ioctl(hw, ISP_HW_CFG_FMCU_CMD_ALIGN, fmcu);

	return ret;
}

static int isppyrdec_cfg_dct_ynr(struct isp_dec_pipe_dev *ctx)
{
	int ret = 0;
	uint32_t addr = 0, cmd = 0, bypass = 0;
	struct isp_fmcu_ctx_desc *fmcu = NULL;
	struct isp_dec_dct_ynr_info *dct_ynr = NULL;
	struct isp_dev_dct_info *dct = NULL;

	fmcu = (struct isp_fmcu_ctx_desc *)ctx->fmcu_handle;
	dct_ynr = &ctx->dct_ynr_info;
	dct = dct_ynr->dct;

	/* Only layer0 need open dct ynr */
	if (ctx->cur_layer_id == 0)
		bypass = dct->bypass;
	else
		bypass = 1;

	addr = ISP_GET_REG(ISP_YNR_DCT_PARAM);
	cmd = (dct->shrink_en << 1) | (dct->lnr_en << 2) |
		(dct->fnr_en << 3) | (dct->rnr_en << 4) |
		(dct->blend_en << 5) | (dct->direction_smooth_en << 6) |
		(dct->addback_en << 7) | bypass;
	FMCU_PUSH(fmcu, addr, cmd);

	if (ctx->cur_layer_id == 0) {
		addr = ISP_GET_REG(ISP_YNR_DCT_PARAM1);
		cmd = (dct->coef_thresh0 & 0x1F) | ((dct->coef_thresh1& 0x1F) << 8) |
			((dct->coef_thresh2 & 0x1F) << 16) | ((dct->coef_thresh3 & 0x1F) << 24);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_YNR_DCT_PARAM2);
		cmd = (dct->coef_ratio0 & 0xFF) | ((dct->coef_ratio1 & 0xFF) << 8) |
			((dct->coef_ratio2 & 0xFF) << 16);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_YNR_DCT_PARAM3);
		cmd = (dct->luma_thresh0 & 0x1F) | ((dct->luma_thresh1 & 0x1F) << 8) |
			((dct->luma_ratio0 & 0xFF) << 16) | ((dct->luma_ratio1 & 0xFF) << 24);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_YNR_DCT_PARAM4);
		cmd = (dct->flat_th & 0xFFF) | ((dct->fnr_thresh0 & 0xFF) << 12) |
			((dct->fnr_thresh1 & 0xFF) << 20);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_YNR_DCT_PARAM5);
		cmd = (dct->fnr_ratio0 & 0xFF) | ((dct->fnr_ratio1 & 0xFF) << 8) |
			((dct->rnr_radius & 0xFFFF) << 16);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_YNR_DCT_PARAM6);
		cmd = (dct->rnr_imgCenterX & 0xFFFF) | ((dct->rnr_imgCenterY & 0xFFFF) << 16);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_YNR_DCT_PARAM7);
		cmd = (dct->rnr_step & 0xFF) | ((dct->rnr_ratio0 & 0xFF) << 8) |
			((dct->rnr_ratio1 & 0xFF) << 16) | ((dct->blend_weight & 0xFF) << 24);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_YNR_DCT_PARAM8);
		cmd = (dct->blend_radius & 0x3) | ((dct->blend_epsilon & 0x3FFF) << 2) |
			((dct->blend_thresh0 & 0x1F) << 16) | ((dct->blend_thresh1 & 0x1F) << 21);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_YNR_DCT_PARAM9);
		cmd = (dct->blend_ratio0 & 0xFF) | ((dct->blend_ratio1 & 0xFF) << 8) |
			((dct->direction_mode & 0x7) << 16) | ((dct->direction_thresh_diff & 0x1FF) << 19);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_YNR_DCT_PARAM10);
		cmd = (dct->direction_thresh_min & 0x1FF) | ((dct->direction_freq_hop_total_num_thresh & 0x7F) << 9) |
			((dct->direction_freq_hop_thresh & 0xFF) << 16) | ((dct->direction_freq_hop_control_en & 0x1) << 24);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_YNR_DCT_PARAM11);
		cmd = (dct->direction_hop_thresh_diff & 0x1FF) | ((dct->direction_hop_thresh_min & 0x1FF) << 16);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_YNR_DCT_PARAM12);
		cmd = (dct->addback_ratio & 0xFF) | ((dct->addback_clip & 0xFF) << 8);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_YNR_DCT_PARAM13);
		cmd = (dct_ynr->start[ctx->cur_slice_id].w & 0xFFFF) |
			((dct_ynr->start[ctx->cur_slice_id].h & 0xFFFF) << 16);
		FMCU_PUSH(fmcu, addr, cmd);

		addr = ISP_GET_REG(ISP_YNR_DCT_PARAM14);
		cmd = (dct_ynr->img.w & 0xFFFF) | ((dct_ynr->img.h & 0xFFFF) << 16);
		FMCU_PUSH(fmcu, addr, cmd);

		if (ctx->store_dct_info.color_format == ISP_FETCH_YVU420_2FRAME) {
			addr = ISP_GET_REG(ISP_YNR_DCT_PARAM15);
			cmd = (16 & 0xFFFF) | ((0 & 0xFFFF) << 16);
			FMCU_PUSH(fmcu, addr, cmd);
		}
	}

	return ret;
}

int isp_pyr_dec_config(void *handle)
{
	int ret = 0;
	struct isp_dec_pipe_dev *ctx = NULL;

	if (!handle) {
		pr_err("fail to isp_dec handle NULL\n");
		return -EFAULT;
	}

	ctx = (struct isp_dec_pipe_dev *)handle;

	isppyrdec_cfg_fetch(ctx);
	isppyrdec_cfg_store(ctx, ISP_PYR_DEC_STORE_DCT);
	isppyrdec_cfg_store(ctx, ISP_PYR_DEC_STORE_DEC);
	isppyrdec_cfg_dct_ynr(ctx);
	isppyrdec_cfg_offline(ctx);

	return ret;
}
