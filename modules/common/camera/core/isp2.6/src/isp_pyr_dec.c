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

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include "isp_pyr_dec.h"
#include "isp_fmcu.h"
#include "alg_slice_calc.h"
#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_PYR_DEC: %d %d %s : "fmt, current->pid, __LINE__, __func__

static uint32_t isppyrdec_cal_pitch(uint32_t w, uint32_t format)
{
	uint32_t pitch = 0;

	switch (format) {
	case ISP_FETCH_YUV420_2FRAME:
	case ISP_FETCH_YVU420_2FRAME:
		pitch = w;
		break;
	case ISP_FETCH_YUV420_2FRAME_MIPI:
	case ISP_FETCH_YVU420_2FRAME_MIPI:
		pitch = (w * 10 + 127) / 128 * 128 / 8;
		break;
	case ISP_FETCH_YVU420_2FRAME_10:
	case ISP_FETCH_YUV420_2FRAME_10:
		pitch = (w * 16 + 127) / 128 * 128 / 8;
		break;
	default:
		pitch =w;
		pr_err("fail to support foramt %d w%d\n", format, w);
		break;
	}

	return pitch;
}

static void isppyrdec_src_frame_ret(void *param)
{
	struct camera_frame *frame = NULL;
	struct isp_dec_pipe_dev *dec_dev = NULL;
	struct isp_dec_sw_ctx *pctx = NULL;

	if (!param) {
		pr_err("fail to get input ptr.\n");
		return;
	}

	frame = (struct camera_frame *)param;
	dec_dev = (struct isp_dec_pipe_dev *)frame->priv_data;
	if (!dec_dev) {
		pr_err("fail to get input ptr.\n");
		return;
	}

	pctx = &dec_dev->sw_ctx[frame->dec_ctx_id];
	if (!pctx) {
		pr_err("fail to get src_frame pctx.\n");
		return;
	}

	if (frame->buf.mapping_state & CAM_BUF_MAPPING_DEV)
		cam_buf_iommu_unmap(&frame->buf);
	pctx->cb_func(ISP_CB_RET_SRC_BUF, frame, pctx->cb_priv_data);
}

static int isppyrrec_irq_free(struct isp_dec_pipe_dev *ctx)
{
	struct cam_hw_info *hw = NULL;

	if (!ctx) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	hw = ctx->hw;
	devm_free_irq(&hw->pdev->dev, ctx->irq_no, (void *)ctx);

	return 0;
}

static int isppyrdec_irq_request(struct isp_dec_pipe_dev *ctx)
{
	int ret = 0;
	struct cam_hw_info *hw = NULL;

	if (!ctx) {
		pr_err("fail to get valid input ptr ctx address is NULL\n");
		return -EFAULT;
	}

	hw = ctx->hw;
	ctx->irq_no = hw->ip_isp->dec_irq_no;
	if (!ctx->isr_func) {
		pr_err("fail to get isp dec irq call back func\n");
		return -EFAULT;
	}

	ret = devm_request_irq(&hw->pdev->dev, ctx->irq_no, ctx->isr_func,
			IRQF_SHARED, "isp_pyr_dec", (void *)ctx);
	if (ret) {
		pr_err("fail to install isp pyr dec irq_no %d\n", ctx->irq_no);
		return -EFAULT;
	}

	return ret;
}

static int isppyrdec_offline_thread_stop(void *param)
{
	int cnt = 0;
	int ret = 0;
	struct cam_thread_info *thrd = NULL;
	struct isp_dec_pipe_dev *pctx = NULL;

	thrd = (struct cam_thread_info *)param;
	pctx = (struct isp_dec_pipe_dev *)thrd->ctx_handle;

	if (thrd->thread_task) {
		atomic_set(&thrd->thread_stop, 1);
		complete(&thrd->thread_com);
		while (cnt < 2500) {
			cnt++;
			if (atomic_read(&thrd->thread_stop) == 0)
				break;
			udelay(1000);
		}
		thrd->thread_task = NULL;
		pr_info("offline thread stopped. wait %d ms\n", cnt);
	}

	/* wait for last frame done */
	ret = wait_for_completion_interruptible_timeout(
		&pctx->frm_done, ISP_CONTEXT_TIMEOUT);
	if (ret == -ERESTARTSYS)
		pr_err("fail to interrupt, when isp wait\n");
	else if (ret == 0)
		pr_err("fail to wait pyr dec, timeout.\n");
	else
		pr_info("wait time %d\n", ret);
	return 0;
}

static int isppyrdec_offline_thread_loop(void *arg)
{
	struct isp_dec_pipe_dev *dev = NULL;
	struct cam_thread_info *thrd = NULL;
	struct isp_dec_sw_ctx *pctx = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}

	thrd = (struct cam_thread_info *)arg;
	dev = (struct isp_dec_pipe_dev *)thrd->ctx_handle;

	while (1) {
		if (wait_for_completion_interruptible(
			&thrd->thread_com) == 0) {
			if (atomic_cmpxchg(&thrd->thread_stop, 1, 0) == 1) {
				pr_info("isp pyr dec thread stop.\n");
				break;
			}
			pr_debug("thread com done.\n");

			if (thrd->proc_func(dev)) {
				pctx = &dev->sw_ctx[dev->cur_ctx_id];
				pctx->cb_func(ISP_CB_DEV_ERR, dev, pctx->cb_priv_data);
				pr_err("fail to start isp %d pyr dec proc. exit thread\n", dev->cur_ctx_id);
				break;
			}
		} else {
			pr_debug("offline thread exit!");
			break;
		}
	}

	return 0;
}

static int isppyrdec_fetch_get(struct isp_dec_pipe_dev *dev, uint32_t idx)
{
	int ret = 0, i = 0;
	uint32_t start_col = 0, start_row = 0;
	uint32_t end_col = 0, end_row = 0;
	uint32_t ch_offset[3] = { 0 };
	uint32_t mipi_word_num_start[16] = {0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5};
	uint32_t mipi_word_num_end[16] = {0, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5};
	struct slice_fetch_info *slc_fetch = NULL;
	struct isp_dec_fetch_info *fetch = NULL;
	struct isp_dec_slice_desc *cur_slc = NULL;

	if (!dev) {
		pr_err("fail to get valid input dev\n");
		return -EFAULT;
	}

	fetch = &dev->fetch_dec_info;
	fetch->bypass = 0;
	fetch->color_format = dev->in_fmt;
	fetch->addr[0] = dev->fetch_addr[idx].addr_ch0;
	fetch->addr[1] = dev->fetch_addr[idx].addr_ch1;
	fetch->width = dev->dec_layer_size[idx].w;
	fetch->height = dev->dec_layer_size[idx].h;
	if (idx != 0)
		fetch->color_format = dev->pyr_fmt;
	fetch->pitch[0] = isppyrdec_cal_pitch(fetch->width, fetch->color_format);
	fetch->pitch[1] = fetch->pitch[0];
	fetch->chk_sum_clr_en = 0;
	fetch->ft0_axi_reorder_en = 0;
	fetch->ft1_axi_reorder_en = 0;
	fetch->substract = 0;
	fetch->ft0_max_len_sel = 1;
	fetch->ft1_max_len_sel = 1;
	/* this default val come from spec need to confirm */
	fetch->ft0_retain_num = 0x20;
	fetch->ft1_retain_num = 0x20;

	cur_slc = &dev->slices[0];
	for (i = 0; i < dev->slice_num; i++, cur_slc++) {
		slc_fetch = &cur_slc->slice_fetch;
		start_col = cur_slc->slice_fetch_pos.start_col;
		start_row = cur_slc->slice_fetch_pos.start_row;
		end_col = cur_slc->slice_fetch_pos.end_col;
		end_row = cur_slc->slice_fetch_pos.end_row;

		switch (fetch->color_format) {
		case ISP_FETCH_YUV420_2FRAME:
		case ISP_FETCH_YVU420_2FRAME:
			ch_offset[0] = start_row * fetch->pitch[0] + start_col;
			ch_offset[1] = (start_row >> 1) * fetch->pitch[1] + start_col;
			break;
		case ISP_FETCH_YUV420_2FRAME_MIPI:
		case ISP_FETCH_YVU420_2FRAME_MIPI:
			ch_offset[0] = start_row * fetch->pitch[0]
				+ (start_col >> 2) * 5 + (start_col & 0x3);
			ch_offset[1] = (start_row >> 1) * fetch->pitch[1]
				+ (start_col >> 2) * 5 + (start_col & 0x3);
			slc_fetch->mipi_byte_rel_pos = start_col & 0x0f;
			slc_fetch->mipi_word_num = ((((end_col + 1) >> 4) * 5
				+ mipi_word_num_end[(end_col + 1) & 0x0f])
				-(((start_col + 1) >> 4) * 5
				+ mipi_word_num_start[(start_col + 1) & 0x0f]) + 1);
			slc_fetch->mipi_byte_rel_pos_uv = slc_fetch->mipi_byte_rel_pos;
			slc_fetch->mipi_word_num_uv = slc_fetch->mipi_word_num;
			slc_fetch->mipi10_en = 1;
			if (dev->fetch_path_sel) {
				slc_fetch->fetch_fbd.slice_size.w = end_col - start_col + 1;
				slc_fetch->fetch_fbd.slice_size.h = end_row - start_row + 1;
				slc_fetch->fetch_fbd.slice_start_pxl_xpt = start_col;
				slc_fetch->fetch_fbd.slice_start_pxl_ypt = start_row;
				slc_fetch->fetch_fbd.slice_start_header_addr = dev->yuv_afbd_info.slice_start_header_addr
					+ ((start_row / ISP_FBD_TILE_HEIGHT) * dev->yuv_afbd_info.tile_num_pitch +
					start_col / ISP_FBD_TILE_WIDTH) * 16;
			}
			ISP_DEC_DEBUG("(%d %d %d %d), pitch %d, offset0 %x offset1 %x, mipi %d %d\n",
				start_row, start_col, end_row, end_col,
				fetch->pitch[0], ch_offset[0], ch_offset[1],
				slc_fetch->mipi_byte_rel_pos, slc_fetch->mipi_word_num);
			break;
		default:
			ch_offset[0] = start_row * fetch->pitch[0] + start_col * 2;
			break;
		}

		slc_fetch->addr.addr_ch0 = fetch->addr[0] + ch_offset[0];
		slc_fetch->addr.addr_ch1 = fetch->addr[1] + ch_offset[1];
		slc_fetch->size.h = end_row - start_row + 1;
		slc_fetch->size.w = end_col - start_col + 1;

		ISP_DEC_DEBUG("slice fetch size %d, %d\n", slc_fetch->size.w, slc_fetch->size.h);
	}

	return ret;
}

static int isppyrdec_store_dct_get(struct isp_dec_pipe_dev *dev, uint32_t idx)
{
	int ret = 0, i = 0;
	uint32_t start_col = 0, start_row = 0;
	uint32_t end_col = 0, end_row = 0;
	uint32_t overlap_left = 0, overlap_up = 0;
	uint32_t overlap_right = 0, overlap_down = 0;
	uint32_t ch_offset[3] = { 0 };
	struct isp_dec_slice_desc *cur_slc = NULL;
	struct slice_store_info *slc_dct_store = NULL;
	struct isp_dec_store_info *store_dct = NULL;

	if (!dev) {
		pr_err("fail to get valid input dev address is NULL \n");
		return -EFAULT;
	}

	store_dct = &dev->store_dct_info;
	if (idx != 0) {
		/* store dct only need output for layer0 */
		store_dct->bypass = 1;
		return 0;
	}
	store_dct->bypass = 0;
	store_dct->color_format = dev->out_fmt;
	store_dct->addr[0] = dev->store_addr[idx].addr_ch0;
	store_dct->addr[1] = dev->store_addr[idx].addr_ch1;
	store_dct->width = dev->dec_layer_size[idx].w;
	store_dct->height = dev->dec_layer_size[idx].h;
	store_dct->pitch[0] = isppyrdec_cal_pitch(store_dct->width, store_dct->color_format);
	store_dct->pitch[1] = store_dct->pitch[0];
	store_dct->data_10b = 0;
	store_dct->mipi_en = 0;
	store_dct->flip_en = 0;
	store_dct->last_frm_en = 1;
	store_dct->mono_en = 0;
	store_dct->mirror_en = 0;
	store_dct->burst_len = 0;
	store_dct->speed2x = 1;
	store_dct->shadow_clr_sel = 1;
	store_dct->shadow_clr = 1;
	store_dct->rd_ctrl = 0;

	cur_slc = &dev->slices[0];
	for (i = 0; i < dev->slice_num; i++, cur_slc++) {
		slc_dct_store = &cur_slc->slice_dct_store;
		start_col = cur_slc->slice_store_dct_pos.start_col;
		start_row = cur_slc->slice_store_dct_pos.start_row;
		end_col = cur_slc->slice_store_dct_pos.end_col;
		end_row = cur_slc->slice_store_dct_pos.end_row;
		overlap_up = cur_slc->slice_dct_overlap.overlap_up;
		overlap_down = cur_slc->slice_dct_overlap.overlap_down;
		overlap_left = cur_slc->slice_dct_overlap.overlap_left;
		overlap_right = cur_slc->slice_dct_overlap.overlap_right;

		switch (store_dct->color_format) {
		case ISP_FETCH_YVU420_2FRAME_MIPI:
		case ISP_FETCH_YUV420_2FRAME_MIPI:
			ch_offset[0] = start_row * store_dct->pitch[0] + start_col * 5 /4 + (start_col & 0x3);
			ch_offset[1] = ((start_row * store_dct->pitch[1]) >> 1) + start_col * 5 /4 + (start_col & 0x3);
			break;
		case ISP_FETCH_YUV420_2FRAME:
		case ISP_FETCH_YVU420_2FRAME:
			ch_offset[0] = start_row * store_dct->pitch[0] + start_col;
			ch_offset[1] = ((start_row * store_dct->pitch[1]) >> 1) + start_col;
			break;
		case ISP_FETCH_YVU420_2FRAME_10:
		case ISP_FETCH_YUV420_2FRAME_10:
			ch_offset[0] = start_row * store_dct->pitch[0] + (start_col << 1);
			ch_offset[1] = ((start_row * store_dct->pitch[1]) >> 1) + (start_col << 1);
			break;
		default:
			ch_offset[0] = start_row * store_dct->pitch[0] + start_col;
			ch_offset[1] = ((start_row * store_dct->pitch[1]) >> 1) + start_col;
			break;
		}

		slc_dct_store->addr.addr_ch0 = store_dct->addr[0] + ch_offset[0];
		slc_dct_store->addr.addr_ch1 = store_dct->addr[1] + ch_offset[1];
		slc_dct_store->size.h = end_row - start_row + 1;
		slc_dct_store->size.w = end_col - start_col + 1;
		slc_dct_store->border.up_border = overlap_up;
		slc_dct_store->border.down_border = overlap_down;
		slc_dct_store->border.left_border = overlap_left;
		slc_dct_store->border.right_border = overlap_right;

		ISP_DEC_DEBUG("slice store size %d, %d offset %x %x\n",
			slc_dct_store->size.w, slc_dct_store->size.h, ch_offset[0], ch_offset[1]);
	}

	return ret;
}

static int isppyrdec_store_dec_get(struct isp_dec_pipe_dev *dev, uint32_t idx)
{
	int ret = 0, i = 0;
	uint32_t start_col = 0, start_row = 0;
	uint32_t end_col = 0, end_row = 0;
	uint32_t overlap_left = 0, overlap_up = 0;
	uint32_t overlap_right = 0, overlap_down = 0;
	uint32_t ch_offset[3] = { 0 };
	struct isp_dec_slice_desc *cur_slc = NULL;
	struct slice_store_info *slc_dec_store = NULL;
	struct isp_dec_store_info *store_dec = NULL;

	if (!dev) {
		pr_err("fail to get valid input dev\n");
		return -EFAULT;
	}

	store_dec = &dev->store_dec_info;
	idx = idx +1;
	store_dec->bypass = 0;
	store_dec->color_format = dev->pyr_fmt;
	store_dec->addr[0] = dev->store_addr[idx].addr_ch0;
	store_dec->addr[1] = dev->store_addr[idx].addr_ch1;
	store_dec->width = dev->dec_layer_size[idx].w;
	store_dec->height = dev->dec_layer_size[idx].h;
	store_dec->pitch[0] = isppyrdec_cal_pitch(store_dec->width, store_dec->color_format);
	store_dec->pitch[1] = store_dec->pitch[0];
	store_dec->data_10b = 0;
	store_dec->mipi_en = 0;
	store_dec->flip_en = 0;
	store_dec->last_frm_en = 1;
	store_dec->mono_en = 0;
	store_dec->mirror_en = 0;
	store_dec->burst_len = 1;
	store_dec->speed2x = 1;
	store_dec->shadow_clr_sel = 1;
	store_dec->shadow_clr = 1;
	store_dec->rd_ctrl = 0;

	cur_slc = &dev->slices[0];
	for (i = 0; i < dev->slice_num; i++, cur_slc++) {
		slc_dec_store = &cur_slc->slice_dec_store;
		start_col = cur_slc->slice_store_dec_pos.start_col;
		start_row = cur_slc->slice_store_dec_pos.start_row;
		end_col = cur_slc->slice_store_dec_pos.end_col;
		end_row = cur_slc->slice_store_dec_pos.end_row;
		overlap_up = cur_slc->slice_dec_overlap.overlap_up;
		overlap_down = cur_slc->slice_dec_overlap.overlap_down;
		overlap_left = cur_slc->slice_dec_overlap.overlap_left;
		overlap_right = cur_slc->slice_dec_overlap.overlap_right;

		switch (store_dec->color_format) {
		case ISP_FETCH_YVU420_2FRAME_MIPI:
		case ISP_FETCH_YUV420_2FRAME_MIPI:
			ch_offset[0] = start_row * store_dec->pitch[0] + start_col * 5 /4 + (start_col & 0x3);
			ch_offset[1] = ((start_row * store_dec->pitch[1]) >> 1) + start_col * 5 /4 + (start_col & 0x3);
			break;
		case ISP_FETCH_YVU420_2FRAME_10:
		case ISP_FETCH_YUV420_2FRAME_10:
			ch_offset[0] = start_row * store_dec->pitch[0] + (start_col << 1);
			ch_offset[1] = ((start_row * store_dec->pitch[1]) >> 1) + (start_col << 1);
			break;
		default:
			ch_offset[0] = start_row * store_dec->pitch[0] + start_col;
			ch_offset[1] = ((start_row * store_dec->pitch[1]) >> 1) + start_col;
			break;
		}

		slc_dec_store->addr.addr_ch0 = store_dec->addr[0] + ch_offset[0];
		slc_dec_store->addr.addr_ch1 = store_dec->addr[1] + ch_offset[1];
		slc_dec_store->size.h = end_row - start_row + 1;
		slc_dec_store->size.w = end_col - start_col + 1;
		slc_dec_store->border.up_border = overlap_up;
		slc_dec_store->border.down_border = overlap_down;
		slc_dec_store->border.left_border = overlap_left;
		slc_dec_store->border.right_border = overlap_right;

		ISP_DEC_DEBUG("slice store size %d, %d offset %x %x\n",
			slc_dec_store->size.w, slc_dec_store->size.h, ch_offset[0], ch_offset[1]);
	}

	return ret;
}

static int isppyrdec_offline_get(struct isp_dec_pipe_dev *dev, uint32_t idx)
{
	int ret = 0, i = 0;
	struct isp_dec_slice_desc *cur_slc = NULL;
	struct slice_pyr_dec_info *slc_pyr_dec = NULL;
	struct isp_dec_offline_info *dec_offline = NULL;

	if (!dev) {
		pr_err("fail to get valid input dev\n");
		return -EFAULT;
	}

	dec_offline = &dev->offline_dec_info;
	dec_offline->bypass = 0;
	dec_offline->hor_padding_en = 0;
	dec_offline->hor_padding_num = 0;
	dec_offline->ver_padding_en = 0;
	dec_offline->ver_padding_num = 0;
	dec_offline->dispatch_dbg_mode_ch0 = 1;
	dec_offline->dispatch_done_cfg_mode = 1;
	dec_offline->dispatch_pipe_flush_num = 4;
	dec_offline->dispatch_pipe_hblank_num = 60;
	dec_offline->dispatch_pipe_nfull_num = 100;
	dec_offline->dispatch_width_dly_num_flash = 640;
	dec_offline->dispatch_width_flash_mode = 0;
	dec_offline->dispatch_yuv_start_order = 0;
	dec_offline->dispatch_yuv_start_row_num = 1;

	if(idx == 0) {
		dec_offline->hor_padding_num = dev->dec_padding_size.w;
		dec_offline->ver_padding_num = dev->dec_padding_size.h;
		if (dec_offline->hor_padding_num)
			dec_offline->hor_padding_en = 1;
		if (dec_offline->ver_padding_num)
			dec_offline->ver_padding_en = 1;
	}

	cur_slc = &dev->slices[0];
	for (i = 0; i < dev->slice_num; i++, cur_slc++) {
		slc_pyr_dec = &cur_slc->slice_pyr_dec;
		slc_pyr_dec->hor_padding_en = dec_offline->hor_padding_en;
		slc_pyr_dec->ver_padding_en = dec_offline->ver_padding_en;
		slc_pyr_dec->hor_padding_num = dec_offline->hor_padding_num;
		slc_pyr_dec->ver_padding_num = dec_offline->ver_padding_num;
		/* padding num only need for last slice */
		if (i != (dev->slice_num - 1)) {
			slc_pyr_dec->hor_padding_en = 0;
			slc_pyr_dec->hor_padding_num = 0;
		}
		slc_pyr_dec->dispatch_dly_width_num = 80;
		slc_pyr_dec->dispatch_dly_height_num = 80;
		if (cur_slc->slice_fetch.size.w <= 40 || cur_slc->slice_fetch.size.h <= 32)
			slc_pyr_dec->dispatch_dly_height_num = 256;

		if (idx == 0) {
			slc_pyr_dec->dispatch_dly_width_num = slc_pyr_dec->hor_padding_num + 20;
			slc_pyr_dec->dispatch_dly_height_num = slc_pyr_dec->ver_padding_num + 20;
		}
	}

	return ret;
}

static int isppyrdec_dct_ynr_get(struct isp_dec_pipe_dev *dev, uint32_t idx)
{
	int ret = 0, i = 0;
	struct isp_dec_slice_desc *cur_slc = NULL;
	struct isp_dec_dct_ynr_info *dct_ynr = NULL;

	if (!dev) {
		pr_err("fail to get valid input dev\n");
		return -EFAULT;
	}

	dct_ynr = &dev->dct_ynr_info;
	cur_slc = &dev->slices[0];
	dct_ynr->img.w = dev->dec_layer_size[0].w;
	dct_ynr->img.h = dev->dec_layer_size[0].h;

	dct_ynr->dct->rnr_radius = dct_ynr->dct_radius;
	dct_ynr->dct->rnr_imgCenterX = dev->dec_layer_size[0].w >> 1;
	dct_ynr->dct->rnr_imgCenterY = dev->dec_layer_size[0].h >> 1;
	pr_debug("radius %d, Center x %d, y %d, img  w %d, h %d\n", dct_ynr->dct->rnr_radius, dct_ynr->dct->rnr_imgCenterX,
		dct_ynr->dct->rnr_imgCenterY, dct_ynr->img.w, dct_ynr->img.h);

	for (i = 0; i < dev->slice_num; i++, cur_slc++) {
		dct_ynr->start[i].w = cur_slc->slice_fetch_pos.start_col;
		dct_ynr->start[i].h = cur_slc->slice_fetch_pos.start_row;
	}

	return ret;
}

static int isppyrdec_calc_base_info(struct isp_dec_pipe_dev *dev)
{
	int ret = 0, i = 0, j = 0;
	uint32_t slice_num, slice_w, slice_h;
	uint32_t slice_max_w, max_w;
	uint32_t linebuf_len, slice_rows, slice_cols;
	uint32_t img_w, img_h, slice_w_org, slice_h_org;
	struct alg_dec_offline_overlap *dec_ovlap = NULL;
	struct isp_dec_overlap_info *cur_ovlap = NULL;
	uint32_t dec_slice_num[MAX_PYR_DEC_LAYER_NUM] = {0};

	if (!dev ) {
		pr_err("fail to get valid input handle\n");
		return -EFAULT;
	}
	dec_ovlap = vzalloc(sizeof(struct alg_dec_offline_overlap));
	if (!dec_ovlap)
		return -EFAULT;

	cur_ovlap = &dev->overlap_dec_info[0];
	/* calc the slice w & h base on input size */
	max_w = dev->src.w;
	slice_num = 1;
	linebuf_len = ISP_MAX_LINE_WIDTH;
	slice_max_w = linebuf_len - SLICE_OVERLAP_W_MAX;
	if (max_w <= linebuf_len) {
		slice_w = max_w;
	} else {
		do {
			slice_num++;
			slice_w = (max_w + slice_num - 1) / slice_num;
		} while (slice_w >= slice_max_w);
	}
	pr_debug("input_w %d, slice_num %d, slice_w %d\n", max_w, slice_num, slice_w);

	dec_ovlap->slice_w = slice_w;
	dec_ovlap->slice_h = dev->src.h / SLICE_H_NUM_MAX;
	dec_ovlap->slice_w = ISP_ALIGNED(dec_ovlap->slice_w);
	dec_ovlap->slice_h = ISP_ALIGNED(dec_ovlap->slice_h);
	dec_ovlap->img_w = dev->src.w;
	dec_ovlap->img_h = dev->src.h;
	dec_ovlap->crop_en = 0;
	dec_ovlap->img_type = 4;
	dec_ovlap->dct_bypass = 0;
	dec_ovlap->dec_offline_bypass = 0;
	dec_ovlap->layerNum = dev->layer_num + 1;
	dec_ovlap->slice_mode = 1;
	dec_ovlap->MaxSliceWidth = linebuf_len;
	alg_slice_calc_dec_offline_overlap(dec_ovlap);

	for (i = 0; i < dec_ovlap->layerNum; i++, cur_ovlap++) {
		img_w = dev->dec_layer_size[0].w >> i;
		img_h = dev->dec_layer_size[0].h >> i;
		slice_w_org = dec_ovlap->slice_w >> i;
		slice_h_org = dec_ovlap->slice_h >> i;
		slice_w = ((img_w << i) * slice_w_org + dev->src.w - 1) / dev->src.w;
		slice_h = ((img_h << i) * slice_h_org + dev->src.h - 1) / dev->src.h;
		slice_cols = (img_w + slice_w - 1) / slice_w;
		if(img_w <= linebuf_len)
			slice_cols = 1;
		slice_rows = (img_h + slice_h - 1) / slice_h;
		cur_ovlap->slice_num = slice_cols * slice_rows;
		dec_slice_num[i] = cur_ovlap->slice_num;
		ISP_DEC_DEBUG("layer %d num %d\n", i, cur_ovlap->slice_num);
		if (i < dev->layer_num) {
			for (j = 0; j < cur_ovlap->slice_num; j++) {
				cur_ovlap->slice_fetch_region[j].start_col = dec_ovlap->fecth_dec_region[i][j].sx;
				cur_ovlap->slice_fetch_region[j].start_row = dec_ovlap->fecth_dec_region[i][j].sy;
				cur_ovlap->slice_fetch_region[j].end_col = dec_ovlap->fecth_dec_region[i][j].ex;
				cur_ovlap->slice_fetch_region[j].end_row = dec_ovlap->fecth_dec_region[i][j].ey;
				ISP_DEC_DEBUG("fetch sx %d sy %d ex %d ey %d\n", cur_ovlap->slice_fetch_region[j].start_col,
					cur_ovlap->slice_fetch_region[j].start_row, cur_ovlap->slice_fetch_region[j].end_col,
					cur_ovlap->slice_fetch_region[j].end_row);
			}
		}
		if (i == 0)
			slice_num = dec_slice_num[i];
		else
			slice_num = dec_slice_num[i - 1];
		for (j = 0; j < slice_num; j++) {
			cur_ovlap->slice_store_region[j].start_col = dec_ovlap->store_dec_region[i][j].sx;
			cur_ovlap->slice_store_region[j].start_row = dec_ovlap->store_dec_region[i][j].sy;
			cur_ovlap->slice_store_region[j].end_col = dec_ovlap->store_dec_region[i][j].ex;
			cur_ovlap->slice_store_region[j].end_row = dec_ovlap->store_dec_region[i][j].ey;
			ISP_DEC_DEBUG("store sx %d sy %d ex %d ey %d\n", cur_ovlap->slice_store_region[j].start_col,
				cur_ovlap->slice_store_region[j].start_row, cur_ovlap->slice_store_region[j].end_col,
				cur_ovlap->slice_store_region[j].end_row);
			cur_ovlap->slice_store_overlap[j].overlap_up = dec_ovlap->store_dec_overlap[i][j].ov_up;
			cur_ovlap->slice_store_overlap[j].overlap_down = dec_ovlap->store_dec_overlap[i][j].ov_down;
			cur_ovlap->slice_store_overlap[j].overlap_left = dec_ovlap->store_dec_overlap[i][j].ov_left;
			cur_ovlap->slice_store_overlap[j].overlap_right = dec_ovlap->store_dec_overlap[i][j].ov_right;
			ISP_DEC_DEBUG("store up %d down %d left %d right %d\n", cur_ovlap->slice_store_overlap[j].overlap_up,
				cur_ovlap->slice_store_overlap[j].overlap_down, cur_ovlap->slice_store_overlap[j].overlap_left,
				cur_ovlap->slice_store_overlap[j].overlap_right);
		}
	}
	if (dec_ovlap) {
		vfree(dec_ovlap);
		dec_ovlap = NULL;
	}

	return ret;
}

static int isppyrdec_param_cfg(struct isp_dec_pipe_dev *dev, uint32_t index)
{
	int ret = 0;

	if (!dev ) {
		pr_err("fail to get valid input handle\n");
		return -EFAULT;
	}

	ret = isppyrdec_fetch_get(dev, index);
	if (ret) {
		pr_err("fail to get isp pyr_dec fetch\n");
		return ret;
	}

	ret = isppyrdec_store_dct_get(dev, index);
	if (ret) {
		pr_err("fail to get isp pyr_dec store dct\n");
		return ret;
	}

	ret = isppyrdec_store_dec_get(dev, index);
	if (ret) {
		pr_err("fail to get isp pyr_dec store dec\n");
		return ret;
	}

	ret = isppyrdec_offline_get(dev, index);
	if (ret) {
		pr_err("fail to get isp pyr_dec offline\n");
		return ret;
	}

	ret = isppyrdec_dct_ynr_get(dev, index);
	if (ret) {
		pr_err("fail to get isp dct_ynr\n");
		return ret;
	}

	return ret;
}

static int isppyrdec_afbd_get(uint32_t fmt, void *cfg_out, struct camera_frame *frame)
{

	int32_t tile_col = 0, tile_row = 0;
	struct isp_fbd_yuv_info *fbd_yuv = NULL;
	struct dcam_compress_cal_para cal_fbc = {0};

	if (!cfg_out || !frame) {
		pr_err("fail to get valid input ptr %p\n", cfg_out);
		return -EFAULT;
	}

	fbd_yuv = (struct isp_fbd_yuv_info *)cfg_out;

	if (frame->is_compressed == 0)
		return 0;

	fbd_yuv->fetch_fbd_bypass = 0;
	fbd_yuv->slice_size.w = frame->width;
	fbd_yuv->slice_size.h = frame->height;
	tile_col = (fbd_yuv->slice_size.w + ISP_FBD_TILE_WIDTH - 1) / ISP_FBD_TILE_WIDTH;
	tile_row =(fbd_yuv->slice_size.h + ISP_FBD_TILE_HEIGHT - 1) / ISP_FBD_TILE_HEIGHT;

	fbd_yuv->tile_num_pitch = tile_col;
	fbd_yuv->slice_start_pxl_xpt = 0;
	fbd_yuv->slice_start_pxl_ypt = 0;

	cal_fbc.compress_4bit_bypass = frame->compress_4bit_bypass;
	cal_fbc.data_bits = frame->data_bits;
	cal_fbc.fbc_info = &frame->fbc_info;
	cal_fbc.in = frame->buf.iova[0];
	if (fmt == ISP_FETCH_YUV420_2FRAME_10 || fmt == ISP_FETCH_YUV420_2FRAME_MIPI)
		cal_fbc.fmt = DCAM_STORE_YUV420;
	else if (fmt == ISP_FETCH_YVU420_2FRAME_10 || fmt == ISP_FETCH_YVU420_2FRAME_MIPI)
		cal_fbc.fmt = DCAM_STORE_YVU420;
	cal_fbc.height = fbd_yuv->slice_size.h;
	cal_fbc.width = fbd_yuv->slice_size.w;
	cal_fbc.out = &fbd_yuv->hw_addr;
	dcam_if_cal_compressed_addr(&cal_fbc);
	fbd_yuv->buffer_size = cal_fbc.fbc_info->buffer_size;

	/* store start address for slice use */
	fbd_yuv->frame_header_base_addr = fbd_yuv->hw_addr.addr0;
	fbd_yuv->slice_start_header_addr = fbd_yuv->frame_header_base_addr +
		((fbd_yuv->slice_start_pxl_ypt / ISP_FBD_TILE_HEIGHT) * fbd_yuv->tile_num_pitch +
		fbd_yuv->slice_start_pxl_xpt / ISP_FBD_TILE_WIDTH) * 16;
	fbd_yuv->data_bits = cal_fbc.data_bits;

	pr_debug("iova:%x, fetch_fbd: %u 0x%x 0x%x, 0x%x, size %u %u, channel_id:%d, tile_col:%d\n",
		 frame->buf.iova[0], frame->fid, fbd_yuv->hw_addr.addr0,
		 fbd_yuv->hw_addr.addr1, fbd_yuv->hw_addr.addr2,
		frame->width, frame->height, frame->channel_id, fbd_yuv->tile_num_pitch);

	return 0;
}

static int isppyrdec_offline_frame_start(void *handle)
{
	int ret = 0;
	uint32_t ctx_id = 0, loop = 0;
	uint32_t layer_num = 0, i = 0, j = 0, pitch = 0;
	uint32_t offset = 0, size = 0;
	struct isp_dec_pipe_dev *dec_dev = NULL;
	struct isp_dec_sw_ctx *pctx = NULL;
	struct camera_frame *pframe = NULL;
	struct camera_frame *out_frame = NULL;
	struct isp_fmcu_ctx_desc *fmcu = NULL;
	struct isp_dec_slice_desc *cur_slc = NULL;
	struct isp_dec_overlap_info *cur_ovlap = NULL, *temp_ovlap = NULL;
	struct isp_hw_k_blk_func dec_cfg_func;
	struct isp_hw_k_blk_func dct_update_func;
	struct isp_k_block *dct_param = NULL;
	if (!handle) {
		pr_err("fail to get valid input handle\n");
		return -EFAULT;
	}

	dec_dev = (struct isp_dec_pipe_dev *)handle;
	fmcu = (struct isp_fmcu_ctx_desc *)dec_dev->fmcu_handle;
	pframe = cam_queue_dequeue(&dec_dev->in_queue, struct camera_frame, list);
	if (pframe == NULL) {
		pr_err("fail to get input frame %p\n", pframe);
		goto exit;
	}
	ctx_id = pframe->dec_ctx_id;
	pctx = &dec_dev->sw_ctx[ctx_id];
	layer_num = ISP_PYR_DEC_LAYER_NUM;
	if (atomic_read(&pctx->cap_cnt) != pframe->cap_cnt) {
		pr_debug("status:%d,cap_cnt:%d", atomic_read(&pctx->cap_cnt), pframe->cap_cnt);
		isppyrdec_src_frame_ret(pframe);
		return 0;
	}
	ret = cam_buf_iommu_map(&pframe->buf, CAM_IOMMUDEV_ISP);
	if (ret) {
		pr_err("fail to map buf to ISP iommu. cxt %d\n", ctx_id);
		ret = -EINVAL;
		goto exit;
	}

	loop = 0;
	do {
		ret = cam_queue_enqueue(&dec_dev->proc_queue, &pframe->list);
		if (ret == 0)
			break;
		pr_debug("wait for proc queue. loop %d\n", loop);
		usleep_range(600, 2000);
	} while (loop++ < 500);

	if (ret) {
		pr_err("fail to input frame queue, timeout.\n");
		ret = -EINVAL;
		goto inq_overflow;
	}

	if (!pframe->blkparam_info.param_block) {
		pr_err("fail to get dec param, fid %d\n", pframe->fid);
		goto out_err;
	}

	loop = 0;
	do {
		pctx->buf_cb_func((void *)&out_frame, pctx->buf_cb_priv_data);
		if (out_frame)
			break;
		pr_debug("wait for out buf. loop %d\n", loop);
		usleep_range(600, 2000);
	} while (loop++ < 500);

	if (!out_frame) {
		pr_err("fail to get outframe loop cnt %d cxt %d\n", loop, ctx_id);
		goto out_err;
	}

	out_frame->blkparam_info = pframe->blkparam_info;
	dct_param = out_frame->blkparam_info.param_block;
	dec_dev->dct_ynr_info.dct = &dct_param->dct_info;
	dec_dev->dct_ynr_info.old_width = dct_param->blkparam_info.old_width;
	dec_dev->dct_ynr_info.old_height = dct_param->blkparam_info.old_height;
	dec_dev->dct_ynr_info.new_width = dct_param->blkparam_info.new_width;
	dec_dev->dct_ynr_info.new_height = dct_param->blkparam_info.new_height;
	dec_dev->dct_ynr_info.sensor_height = dct_param->blkparam_info.sensor_height;
	dec_dev->dct_ynr_info.sensor_width = dct_param->blkparam_info.sensor_width;
	dct_update_func.index = ISP_K_BLK_DCT_UPDATE;
	dec_dev->hw->isp_ioctl(dec_dev->hw, ISP_HW_CFG_K_BLK_FUNC_GET, &dct_update_func);
	if (dct_update_func.k_blk_func)
		dct_update_func.k_blk_func(dec_dev);
	dct_param->dct_radius = dec_dev->dct_ynr_info.dct_radius;

	pctx->buf_out = out_frame;
	ret = cam_buf_iommu_map(&pctx->buf_out->buf, CAM_IOMMUDEV_ISP);
	if (ret) {
		pr_err("fail to map buf to ISP iommu. cxt %d\n", ctx_id);
		goto map_err;
	}

	if (pframe->is_compressed)
		ret = isppyrdec_afbd_get(dec_dev->in_fmt, &dec_dev->yuv_afbd_info, pframe);

	dec_dev->src.w = pframe->width;
	dec_dev->src.h = pframe->height;
	out_frame->width = pframe->width;
	out_frame->height = pframe->height;
	out_frame->cap_cnt = pframe->cap_cnt;
	/* update layer num based on img size */
	while (isp_rec_small_layer_w(dec_dev->src.w, layer_num) < MIN_PYR_WIDTH ||
		isp_rec_small_layer_h(dec_dev->src.h, layer_num) < MIN_PYR_HEIGHT) {
		pr_debug("layer num need decrease based on small input %d %d\n",
			dec_dev->src.w, dec_dev->src.h);
		layer_num--;
	}
	dec_dev->layer_num = layer_num;
	pitch = isppyrdec_cal_pitch(dec_dev->src.w, dec_dev->in_fmt);
	size = pitch * dec_dev->src.h;
	dec_dev->fetch_path_sel = pframe->is_compressed;
	dec_dev->fetch_addr[0].addr_ch0 = pframe->buf.iova[0];
	dec_dev->fetch_addr[0].addr_ch1 = pframe->buf.iova[0] + size;
	dec_dev->store_addr[0].addr_ch0 = pctx->buf_out->buf.iova[0];
	dec_dev->store_addr[0].addr_ch1 = dec_dev->store_addr[0].addr_ch0 + size;
	dec_dev->yuv_afbd_info.frame_header_base_addr = pframe->buf.iova[0];
	dec_dev->yuv_afbd_info.slice_start_header_addr = pframe->buf.iova[0];
	dec_dev->dec_layer_size[0].w = isp_rec_layer0_width(dec_dev->src.w, layer_num);
	dec_dev->dec_layer_size[0].h = isp_rec_layer0_heigh(dec_dev->src.h, layer_num);
	dec_dev->dec_padding_size.w = dec_dev->dec_layer_size[0].w - dec_dev->src.w;
	dec_dev->dec_padding_size.h = dec_dev->dec_layer_size[0].h - dec_dev->src.h;
	ISP_DEC_DEBUG("isp %d layer0 size %d %d padding size %d %d\n", ctx_id,
		dec_dev->dec_layer_size[0].w, dec_dev->dec_layer_size[0].h,
		dec_dev->dec_padding_size.w, dec_dev->dec_padding_size.h);
	ISP_DEC_DEBUG("layer0 fetch addr %x %x store addr %x %x\n",
		dec_dev->fetch_addr[0].addr_ch0, dec_dev->fetch_addr[0].addr_ch1,
		dec_dev->store_addr[0].addr_ch0, dec_dev->store_addr[0].addr_ch1);
	for (i = 1; i < layer_num + 1; i++) {
		offset += (size * 3 / 2);
		dec_dev->dec_layer_size[i].w = dec_dev->dec_layer_size[i - 1].w / 2;
		dec_dev->dec_layer_size[i].h = dec_dev->dec_layer_size[i - 1].h / 2;
		pitch = isppyrdec_cal_pitch(dec_dev->dec_layer_size[i].w, dec_dev->pyr_fmt);
		size = pitch * dec_dev->dec_layer_size[i].h;
		dec_dev->store_addr[i].addr_ch0 = pctx->buf_out->buf.iova[0] + offset;
		dec_dev->store_addr[i].addr_ch1 = dec_dev->store_addr[i].addr_ch0 + size;
		if (i < layer_num) {
			dec_dev->fetch_addr[i].addr_ch0 = dec_dev->store_addr[i].addr_ch0;
			dec_dev->fetch_addr[i].addr_ch1 = dec_dev->store_addr[i].addr_ch1;
		}
		ISP_DEC_DEBUG("isp %d layer%d size %d %d\n", ctx_id, i,
			dec_dev->dec_layer_size[i].w, dec_dev->dec_layer_size[i].h);
		if (i < layer_num)
			ISP_DEC_DEBUG("layer %d fetch addr %x %x\n", i,
				dec_dev->fetch_addr[i].addr_ch0, dec_dev->fetch_addr[i].addr_ch1);
		ISP_DEC_DEBUG("layer %d store addr %x %x\n", i,
			dec_dev->store_addr[i].addr_ch0, dec_dev->store_addr[i].addr_ch1);
	}
	/* layer 0 fetch & store size is real size not include padding size */
	dec_dev->dec_layer_size[0].w = dec_dev->src.w;
	dec_dev->dec_layer_size[0].h = dec_dev->src.h;

	ret = isppyrdec_calc_base_info(dec_dev);
	if (ret) {
		pr_err("fail to cal dec base info\n");
		ret = -EINVAL;
		goto calc_err;
	}

	fmcu->ops->ctx_reset(fmcu);
	dec_cfg_func.index = ISP_K_BLK_PYR_DEC_CFG;
	dec_dev->hw->isp_ioctl(dec_dev->hw, ISP_HW_CFG_K_BLK_FUNC_GET, &dec_cfg_func);
	for (i = 0; i < layer_num; i++) {
		cur_ovlap = &dec_dev->overlap_dec_info[i];
		cur_slc = &dec_dev->slices[0];
		dec_dev->slice_num = cur_ovlap->slice_num;
		dec_dev->cur_layer_id = i;
		for (j = 0; j < dec_dev->slice_num; j++, cur_slc++) {
			cur_slc->slice_fetch_pos = cur_ovlap->slice_fetch_region[j];
			cur_slc->slice_store_dct_pos = cur_ovlap->slice_store_region[j];
			cur_slc->slice_dct_overlap = cur_ovlap->slice_store_overlap[j];
			temp_ovlap = &dec_dev->overlap_dec_info[i + 1];
			cur_slc->slice_store_dec_pos = temp_ovlap->slice_store_region[j];
			cur_slc->slice_dec_overlap = temp_ovlap->slice_store_overlap[j];
		}

		isppyrdec_param_cfg(dec_dev, i);
		for (j = 0; j < dec_dev->slice_num; j++) {
			dec_dev->cur_slice_id = j;
			if (dec_cfg_func.k_blk_func)
				dec_cfg_func.k_blk_func(dec_dev);
		}
	}
	ret = wait_for_completion_interruptible_timeout(&dec_dev->frm_done,
			ISP_CONTEXT_TIMEOUT);
	if (ret == -ERESTARTSYS) {
		pr_err("fail to interrupt, when isp dec wait\n");
		ret = -EFAULT;
		goto calc_err;
	} else if (ret == 0) {
		pr_err("fail to wait isp dec context %d, timeout.\n", ctx_id);
		ret = -EFAULT;
		goto calc_err;
	}

	/* keep param node untile dec start successful, or recycle param node with input pframe */
	pframe->blkparam_info.update = 0;
	pframe->blkparam_info.param_block = NULL;
	pframe->blkparam_info.blk_param_node = NULL;

	dec_dev->cur_ctx_id = ctx_id;
	/* start pyr dec fmcu */
	ISP_DEC_DEBUG("isp pyr dec fmcu start\n");
	fmcu->ops->hw_start(fmcu);

	return 0;

calc_err:
	if (pctx->buf_out)
		ret = cam_buf_iommu_unmap(&pctx->buf_out->buf);
out_err:
map_err:
	pframe = cam_queue_dequeue_tail(&dec_dev->proc_queue);
inq_overflow:
	dec_dev->cur_ctx_id = ctx_id;
	if (pframe)
		isppyrdec_src_frame_ret(pframe);
exit:
	return ret;
}

static int isppyrdec_offline_thread_create(void *param)
{
	struct isp_dec_pipe_dev *dec_dev = NULL;
	struct cam_thread_info *thrd = NULL;
	char thread_name[32] = { 0 };

	dec_dev = (struct isp_dec_pipe_dev *)param;
	thrd = &dec_dev->thread;
	thrd->ctx_handle = dec_dev;

	if (thrd->thread_task) {
		pr_info("isp pyr dec offline thread created is exist.\n");
		return 0;
	}

	thrd->proc_func = isppyrdec_offline_frame_start;
	sprintf(thread_name, "isp_pyr_dec_offline");
	atomic_set(&thrd->thread_stop, 0);
	init_completion(&thrd->thread_com);
	thrd->thread_task = kthread_run(isppyrdec_offline_thread_loop, thrd, "%s", thread_name);
	if (IS_ERR_OR_NULL(thrd->thread_task)) {
		pr_err("fail to start isp pyr dec thread %ld\n", PTR_ERR(thrd->thread_task));
		return -EFAULT;
	}

	return 0;
}

static int isppyrdec_cfg_param(void *handle, int ctx_id,
		enum isp_dec_cfg_cmd cmd, void *in_fmt, void *pyr_fmt)
{
	int ret = 0;
	struct isp_dec_pipe_dev *dec_dev = NULL;
	struct isp_dec_sw_ctx *pctx = NULL;

	if (!handle || !in_fmt || !pyr_fmt) {
		pr_err("fail to get valid input ptr %p\n", handle);
		return -EFAULT;
	}

	dec_dev = (struct isp_dec_pipe_dev *)handle;
	pctx = &dec_dev->sw_ctx[ctx_id];
	switch (cmd) {
	case ISP_DEC_CFG_IN_FORMAT:
		dec_dev->in_fmt = *(uint32_t *)in_fmt;
		dec_dev->out_fmt = dec_dev->in_fmt;
		dec_dev->pyr_fmt = *(uint32_t *)pyr_fmt;
		ISP_DEC_DEBUG("DEC proc format %d\n", dec_dev->in_fmt);
		ISP_DEC_DEBUG("DEC proc pyr format %d\n", dec_dev->pyr_fmt);
		break;
	default:
		pr_err("fail to get known cmd: %d\n", cmd);
		ret = -EFAULT;
		break;
	}

	return ret;
}

static int isppyrdec_proc_frame(void *handle, void *param)
{
	int ret = 0;
	struct isp_dec_pipe_dev *dec_dev = NULL;
	struct camera_frame *pframe = NULL;

	if (!handle || !param) {
		pr_err("fail to get valid input ptr, dec_handle %p, param %p\n", handle, param);
		return -EFAULT;
	}

	dec_dev = (struct isp_dec_pipe_dev *)handle;
	pframe = (struct camera_frame *)param;
	pframe->priv_data = dec_dev;

	pframe->cap_cnt = atomic_read(&dec_dev->sw_ctx[pframe->dec_ctx_id].cap_cnt);
	ret = cam_queue_enqueue(&dec_dev->in_queue, &pframe->list);
	if (ret == 0)
		complete(&dec_dev->thread.thread_com);

	return ret;
}

static int isppyrdec_callback_set(void *handle, int ctx_id, isp_dev_callback cb,
		void *priv_data)
{
	int ret = 0;
	struct isp_dec_pipe_dev *dec_dev = NULL;
	struct isp_dec_sw_ctx *pctx = NULL;

	if (!handle || !cb || !priv_data) {
		pr_err("fail to get valid input ptr, dec_handle %p, callback %p, priv_data %p\n",
			handle, cb, priv_data);
		return -EFAULT;
	}

	dec_dev = (struct isp_dec_pipe_dev *)handle;
	pctx = &dec_dev->sw_ctx[ctx_id];
	if (pctx->cb_func == NULL) {
		pctx->cb_func = cb;
		pctx->cb_priv_data = priv_data;
		pr_debug("ctx %d cb %p, %p\n", ctx_id, cb, priv_data);
	}

	return ret;
}

static int isppyrdec_outbuf_callback_get(void *handle, int ctx_id, pyr_dec_buf_cb cb,
		void *priv_data)
{
	int ret = 0;
	struct isp_dec_pipe_dev *dec_dev = NULL;
	struct isp_dec_sw_ctx *pctx = NULL;

	if (!handle || !cb || !priv_data) {
		pr_err("fail to get valid input ptr, dec_handle %p, callback %p, priv_data %p\n",
			handle, cb, priv_data);
		return -EFAULT;
	}

	dec_dev = (struct isp_dec_pipe_dev *)handle;
	pctx = &dec_dev->sw_ctx[ctx_id];
	if (pctx->buf_cb_func == NULL) {
		pctx->buf_cb_func = cb;
		pctx->buf_cb_priv_data = priv_data;
		pr_debug("ctx %d cb %p, %p\n", ctx_id, cb, priv_data);
	}

	return ret;
}

static int isppyrdec_irq_proc(void *handle)
{
	int ret = 0;
	uint32_t cur_ctx_id = 0;
	struct isp_dec_pipe_dev *dec_dev = NULL;
	struct isp_dec_sw_ctx *pctx = NULL;
	struct camera_frame *pframe = NULL;

	if (!handle) {
		pr_err("fail to get invalid ptr\n");
		return -EFAULT;
	}

	dec_dev = (struct isp_dec_pipe_dev *)handle;
	cur_ctx_id = dec_dev->cur_ctx_id;
	pctx = &dec_dev->sw_ctx[cur_ctx_id];
	if (!pctx) {
		pr_err("fail to get pctx:%x\n", pctx);
		return -EFAULT;
	}
	complete(&dec_dev->frm_done);

	pframe = cam_queue_dequeue(&dec_dev->proc_queue, struct camera_frame, list);
	if (pframe) {
		if (!pctx->buf_out) {
			pr_err("fail to get pctx->buf_out:%x\n", pctx->buf_out);
			return -EFAULT;
		}
		/* return buffer to cam channel shared buffer queue. */
		pctx->buf_out->fid = pframe->fid;
		pctx->buf_out->sensor_time = pframe->sensor_time;
		pctx->buf_out->boot_sensor_time = pframe->boot_sensor_time;
		pctx->buf_out->pyr_status = pframe->pyr_status;
		cam_buf_iommu_unmap(&pframe->buf);
		if (pctx->cb_func)
			pctx->cb_func(ISP_CB_RET_SRC_BUF, pframe, pctx->cb_priv_data);
		else
			pr_err("fail to get cb_func ptr at ret src\n");
	} else {
		pr_err("fail to get src frame sw_idx=%d proc_queue.cnt:%d\n",
			cur_ctx_id, dec_dev->proc_queue.cnt);
	}

	pframe = pctx->buf_out;
	pctx->buf_out = NULL;
	if (pframe) {
		/* return buffer to cam core for start isp pyrrec proc */
		cam_buf_iommu_unmap(&pframe->buf);
		pframe->need_pyr_rec = 1;
		if (pctx->cb_func)
			pctx->cb_func(ISP_CB_RET_PYR_DEC_BUF, pframe, pctx->cb_priv_data);
		else
			pr_err("fail to get cb_func ptr at ret pyr dec\n");
	} else {
		pr_err("fail to get src frame sw_idx=%d \n", cur_ctx_id);
	}

	return ret;
}

static int isppyrdec_proc_init(void *handle)
{
	int ret = 0;
	struct isp_dec_pipe_dev *dec_dev = NULL;

	if (!handle) {
		pr_err("fail to get valid input ptr");
		return -EFAULT;
	}

	dec_dev = (struct isp_dec_pipe_dev *)handle;
	if (atomic_read(&dec_dev->proc_eb) == 0) {
		cam_queue_init(&dec_dev->in_queue, ISP_PYRDEC_BUF_Q_LEN,
			isppyrdec_src_frame_ret);
		cam_queue_init(&dec_dev->proc_queue, ISP_PYRDEC_BUF_Q_LEN,
			isppyrdec_src_frame_ret);
		complete(&dec_dev->frm_done);
		ret = isppyrdec_offline_thread_create(dec_dev);
		if (unlikely(ret != 0)) {
			pr_err("fail to create offline thread for isp pyr dec\n");
			return -EFAULT;
		}
		atomic_set(&dec_dev->proc_eb, 1);
	}

	return ret;
}

static int isppyrdec_proc_deinit(void *handle, int ctx_id)
{
	int ret = 0, loop = 0;
	struct isp_dec_pipe_dev *dec_dev = NULL;
	struct isp_dec_sw_ctx *pctx = NULL;

	if (!handle) {
		pr_err("fail to get valid input ptr");
		return -EFAULT;
	}

	dec_dev = (struct isp_dec_pipe_dev *)handle;
	pctx = &dec_dev->sw_ctx[ctx_id];

	if (atomic_read(&dec_dev->proc_eb) == 1) {
		isppyrdec_offline_thread_stop(&dec_dev->thread);
		while (pctx->in_irq_handler && (loop < 1000)) {
			pr_info_ratelimited("dec in irq. wait %d\n", loop);
			loop++;
			udelay(500);
		}
		cam_queue_clear(&dec_dev->in_queue, struct camera_frame, list);
		cam_queue_clear(&dec_dev->proc_queue, struct camera_frame, list);
		atomic_set(&dec_dev->proc_eb, 0);
	}
	pctx->cb_func = NULL;
	pctx->cb_priv_data = NULL;
	pctx->buf_cb_func = NULL;
	pctx->buf_cb_priv_data = NULL;
	atomic_set(&pctx->cap_cnt, 0);
	return ret;
}

void *isp_pyr_dec_dev_get(void *isp_handle, void *hw)
{
	int ret = 0;
	struct isp_dec_pipe_dev *dec_dev = NULL;
	struct isp_fmcu_ctx_desc *fmcu = NULL;
	struct isp_hw_k_blk_func irq_func;

	dec_dev = vzalloc(sizeof(struct isp_dec_pipe_dev));
	if (!dec_dev)
		return NULL;

	dec_dev->isp_handle = isp_handle;
	dec_dev->hw = hw;
	dec_dev->layer_num = ISP_PYR_DEC_LAYER_NUM;
	/* get isp dec irq call back function */
	irq_func.index = ISP_K_BLK_PYR_DEC_IRQ_FUNC;
	dec_dev->hw->isp_ioctl(hw, ISP_HW_CFG_K_BLK_FUNC_GET, &irq_func);
	if (irq_func.k_blk_func)
		irq_func.k_blk_func(dec_dev);
	ret = isppyrdec_irq_request(dec_dev);
	if (unlikely(ret != 0)) {
		pr_err("fail to request irq for isp pyr dec\n");
		goto irq_err;
	}
	init_completion(&dec_dev->frm_done);
	atomic_set(&dec_dev->proc_eb, 0);
	fmcu = isp_fmcu_dec_ctx_get(hw);
	if (fmcu && fmcu->ops) {
		fmcu->hw = hw;
		ret = fmcu->ops->ctx_init(fmcu);
		if (ret) {
			pr_err("fail to init fmcu ctx\n");
			isp_fmcu_ctx_desc_put(fmcu);
		} else {
			dec_dev->fmcu_handle = fmcu;
		}
	} else {
		pr_info("no more fmcu or ops\n");
	}
	dec_dev->irq_proc_func = isppyrdec_irq_proc;
	dec_dev->ops.cfg_param = isppyrdec_cfg_param;
	dec_dev->ops.proc_frame = isppyrdec_proc_frame;
	dec_dev->ops.set_callback = isppyrdec_callback_set;
	dec_dev->ops.get_out_buf_cb = isppyrdec_outbuf_callback_get;
	dec_dev->ops.proc_init = isppyrdec_proc_init;
	dec_dev->ops.proc_deinit = isppyrdec_proc_deinit;

	return dec_dev;

irq_err:
	if (dec_dev) {
		vfree(dec_dev);
		dec_dev = NULL;
	}

	return dec_dev;
}

void isp_pyr_dec_dev_put(void *dec_handle)
{
	struct isp_dec_pipe_dev *dec_dev = NULL;
	struct isp_fmcu_ctx_desc *fmcu = NULL;

	if (!dec_handle) {
		pr_err("fail to get valid rec handle\n");
		return;
	}

	dec_dev = (struct isp_dec_pipe_dev *)dec_handle;
	/* irq disable */
	isppyrrec_irq_free(dec_dev);
	atomic_set(&dec_dev->proc_eb, 0);
	fmcu = (struct isp_fmcu_ctx_desc *)dec_dev->fmcu_handle;
	if (fmcu) {
		fmcu->ops->ctx_deinit(fmcu);
		isp_fmcu_ctx_desc_put(fmcu);
	}
	dec_dev->fmcu_handle = NULL;
	if (dec_dev)
		vfree(dec_dev);
	dec_dev = NULL;
}
