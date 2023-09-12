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

#include "isp_pyr_rec.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_PYR_REC: %d %d %s : "fmt, current->pid, __LINE__, __func__

#define CLIP(x, maxv, minv) \
	do { \
		if (x > maxv) \
			x = maxv; \
		else if (x < minv) \
			x = minv; \
	} while (0)

struct pyr_ynr_alg_cal {
	uint16_t Radius[6];
	uint16_t imgCenterX[6];
	uint16_t imgCenterY[6];
	uint16_t max_dist[5];
};

static void cal_radius_distance_octagon_func(uint16_t *dis_radius,
		uint16_t global_row, uint16_t global_col, uint16_t imgCenterX, uint16_t imgCenterY)
{
	uint16_t dist_vertical = abs(global_row - imgCenterY);
	uint16_t dist_horizontal = abs(global_col - imgCenterX);
	uint16_t dist_v_thr = (uint16_t)(((uint32_t)(dist_horizontal) * 53 + 64) >> 7);
	uint16_t dist_h_thr = (uint16_t)(((uint32_t)(dist_vertical) * 53 + 64) >> 7);

	if (dist_vertical > dist_v_thr && dist_horizontal > dist_h_thr)
		*dis_radius = dist_vertical + dist_horizontal;
	else
		*dis_radius = (uint16_t)(((uint32_t)(MAX(dist_vertical, dist_horizontal)) * 181 + 64) >> 7);
}

static void cal_max_dist(struct pyr_ynr_alg_cal *ynr_param, int layer_id,
	int layer0_frame_W, int layer0_frame_H)
{
	int i = 0, x_cor = 0, y_cor = 0;
	uint16_t radius = 0, centerX = 0, centerY = 0;
	uint16_t CenterX = ynr_param->imgCenterX[0];
	uint16_t CenterY = ynr_param->imgCenterY[0];
	uint16_t Radius = ynr_param->Radius[0];

	for(i = 1; i <= layer_id; i++) {
		int layer_frame_W = layer0_frame_W >> i;
		int layer_frame_H = layer0_frame_H >> i;

		radius = (Radius + (1 << (i - 1)) -1) >> i;
		centerX = (CenterX + (1 << (i - 1)) - 1) >> i;
		centerY = (CenterY + (1 << (i - 1)) - 1) >> i;
		CLIP(centerX, layer_frame_W, 0);
		CLIP(centerY, layer_frame_H, 0);
		CLIP(radius, layer_frame_H, 0);
		ynr_param->imgCenterX[i] = centerX ;
		ynr_param->imgCenterY[i]= centerY;

		if(centerX > layer_frame_W / 2)
			x_cor = 0;
		else
			x_cor = layer_frame_W;

		if(centerY > layer_frame_H / 2)
			y_cor = 0;
		else
			y_cor = layer_frame_H;

		cal_radius_distance_octagon_func(&ynr_param->max_dist[i - 1], y_cor, x_cor, centerX, centerY);

		if(radius > ynr_param->max_dist[i - 1])
			radius = ynr_param->max_dist[i - 1];
		ynr_param->Radius[i] = radius;
	}
}

static int isppyrrec_pitch_get(uint32_t format, uint32_t w)
{
	uint32_t pitch = 0;

	switch (format) {
	case ISP_FETCH_YUV420_2FRAME:
	case ISP_FETCH_YVU420_2FRAME:
		pitch = w;
		break;
	case ISP_FETCH_YUV420_2FRAME_10:
	case ISP_FETCH_YVU420_2FRAME_10:
		pitch = (w * 16 + 127) / 128 * 128 / 8;
		break;
	case ISP_FETCH_YUV420_2FRAME_MIPI:
	case ISP_FETCH_YVU420_2FRAME_MIPI:
		pitch = (w * 10 + 127) / 128 * 128 / 8;
		break;
	default:
		pr_err("fail to get fetch format: %d\n", format);
		break;
	}

	return pitch;
}

static int isppyrrec_ref_fetch_get(struct isp_rec_ctx_desc *ctx, uint32_t idx)
{
	int ret = 0, i = 0;
	uint32_t start_col = 0, start_row = 0;
	uint32_t end_col = 0, end_row = 0;
	uint32_t ch_offset[3] = { 0 }, pitch = 0;
	uint32_t mipi_word_num_start[16] = {0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5};
	uint32_t mipi_word_num_end[16] = {0, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5};
	struct slice_fetch_info *slc_fetch = NULL;
	struct isp_rec_fetch_info *ref_fetch = NULL;
	struct isp_rec_slice_desc *cur_slc = NULL;

	if (!ctx) {
		pr_err("fail to get valid input ctx\n");
		return -EFAULT;
	}

	ref_fetch = &ctx->ref_fetch;
	ref_fetch->bypass = 0;
	ref_fetch->color_format = ctx->pyr_fmt;
	if (idx == ctx->layer_num) {
		ref_fetch->addr[0] = ctx->fetch_addr[idx].addr_ch0;
		ref_fetch->addr[1] = ctx->fetch_addr[idx].addr_ch1;
	} else {
		ref_fetch->addr[0] = ctx->store_addr[idx].addr_ch0;
		ref_fetch->addr[1] = ctx->store_addr[idx].addr_ch1;
	}
	ref_fetch->width = ctx->pyr_layer_size[idx].w;
	ref_fetch->height = ctx->pyr_layer_size[idx].h;
	pitch = isppyrrec_pitch_get(ref_fetch->color_format, ref_fetch->width);
	ref_fetch->pitch[0] = pitch;
	ref_fetch->pitch[1] = pitch;
	ref_fetch->chk_sum_clr_en = 0;
	ref_fetch->ft0_axi_reorder_en = 0;
	ref_fetch->ft1_axi_reorder_en = 0;
	ref_fetch->substract = 0;
	/* To avoid rec fifo err, isp fetch burst_lens = 8, then MIN_PYR_WIDTH >= 128;
	 isp fetch burst_lens = 16, then MIN_PYR_WIDTH >= 256. */
	ref_fetch->ft0_max_len_sel = 0;
	ref_fetch->ft1_max_len_sel = 0;
	ref_fetch->ft0_retain_num = 16;
	ref_fetch->ft1_retain_num = 16;

	cur_slc = &ctx->slices[0];
	for (i = 0; i < ctx->slice_num; i++, cur_slc++) {
		slc_fetch = &cur_slc->slice_ref_fetch;
		start_col = cur_slc->slice_fetch0_pos.start_col;
		start_row = cur_slc->slice_fetch0_pos.start_row;
		end_col = cur_slc->slice_fetch0_pos.end_col;
		end_row = cur_slc->slice_fetch0_pos.end_row;

		switch (ref_fetch->color_format) {
		case ISP_FETCH_YUV420_2FRAME:
		case ISP_FETCH_YVU420_2FRAME:
			ch_offset[0] = start_row * ref_fetch->pitch[0] + start_col;
			ch_offset[1] = (start_row >> 1) * ref_fetch->pitch[1] + start_col;
			break;
		case ISP_FETCH_YUV420_2FRAME_10:
		case ISP_FETCH_YVU420_2FRAME_10:
			ch_offset[0] = start_row * ref_fetch->pitch[0] + start_col * 2;
			ch_offset[1] = (start_row >> 1) * ref_fetch->pitch[1] + start_col * 2;
			break;
		case ISP_FETCH_YUV420_2FRAME_MIPI:
		case ISP_FETCH_YVU420_2FRAME_MIPI:
			ch_offset[0] = start_row * ref_fetch->pitch[0]
				+ (start_col >> 2) * 5 + (start_col & 0x3);
			ch_offset[1] = (start_row >> 1) * ref_fetch->pitch[1]
				+ (start_col >> 2) * 5 + (start_col & 0x3);
			slc_fetch->mipi_byte_rel_pos = start_col & 0x0f;
			slc_fetch->mipi_word_num = ((((end_col + 1) >> 4) * 5
				+ mipi_word_num_end[(end_col + 1) & 0x0f])
				-(((start_col + 1) >> 4) * 5
				+ mipi_word_num_start[(start_col + 1) & 0x0f]) + 1);
			slc_fetch->mipi_byte_rel_pos_uv = slc_fetch->mipi_byte_rel_pos;
			slc_fetch->mipi_word_num_uv = slc_fetch->mipi_word_num;
			slc_fetch->mipi10_en = 1;
			ISP_PYR_DEBUG("fetch0 (%d %d %d %d), pitch %d, offset %d, mipi %d %d\n",
				start_col, start_row, end_col, end_row,
				ref_fetch->pitch[0], ch_offset[0],
				slc_fetch->mipi_byte_rel_pos, slc_fetch->mipi_word_num);
			break;
		default:
			ch_offset[0] = start_row * ref_fetch->pitch[0] + start_col * 2;
			break;
		}

		slc_fetch->addr.addr_ch0 = ref_fetch->addr[0] + ch_offset[0];
		slc_fetch->addr.addr_ch1 = ref_fetch->addr[1] + ch_offset[1];
		slc_fetch->size.h = end_row - start_row + 1;
		slc_fetch->size.w = end_col - start_col + 1;

		ISP_PYR_DEBUG("ref slice fetch0 size %d, %d\n", slc_fetch->size.w, slc_fetch->size.h);
	}

	return ret;
}

static int isppyrrec_cur_fetch_get(struct isp_rec_ctx_desc *ctx, uint32_t idx)
{
	int ret = 0, i = 0;
	uint32_t start_col = 0, start_row = 0;
	uint32_t end_col = 0, end_row = 0;
	uint32_t ch_offset[3] = { 0 }, pitch = 0;
	uint32_t mipi_word_num_start[16] = {0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5};
	uint32_t mipi_word_num_end[16] = {0, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5};
	struct slice_fetch_info *slc_fetch = NULL;
	struct isp_rec_fetch_info *cur_fetch = NULL;
	struct isp_rec_slice_desc *cur_slc = NULL;

	if (!ctx || (idx < 1)) {
		pr_err("fail to get valid input ctx %p idx %d\n", ctx, idx);
		return -EFAULT;
	}

	cur_fetch = &ctx->cur_fetch;
	idx = idx - 1;
	cur_fetch->bypass = 0;
	cur_fetch->color_format = ctx->in_fmt;
	cur_fetch->addr[0] = ctx->fetch_addr[idx].addr_ch0;
	cur_fetch->addr[1] = ctx->fetch_addr[idx].addr_ch1;
	cur_fetch->width = ctx->pyr_layer_size[idx].w;
	cur_fetch->height = ctx->pyr_layer_size[idx].h;
	if (idx != 0)
		cur_fetch->color_format = ctx->pyr_fmt;
	pitch = isppyrrec_pitch_get(cur_fetch->color_format, cur_fetch->width);
	cur_fetch->pitch[0] = pitch;
	cur_fetch->pitch[1] = pitch;
	cur_fetch->chk_sum_clr_en = 0;
	cur_fetch->ft0_axi_reorder_en = 0;
	cur_fetch->ft1_axi_reorder_en = 0;
	cur_fetch->substract = 0;
	cur_fetch->ft0_max_len_sel = 1;
	cur_fetch->ft1_max_len_sel = 1;
	cur_fetch->ft0_retain_num = 16;
	cur_fetch->ft1_retain_num = 16;

	cur_slc = &ctx->slices[0];
	for (i = 0; i < ctx->slice_num; i++, cur_slc++) {
		slc_fetch = &cur_slc->slice_cur_fetch;
		start_col = cur_slc->slice_fetch1_pos.start_col;
		start_row = cur_slc->slice_fetch1_pos.start_row;
		end_col = cur_slc->slice_fetch1_pos.end_col;
		end_row = cur_slc->slice_fetch1_pos.end_row;

		switch (cur_fetch->color_format) {
		case ISP_FETCH_YUV420_2FRAME:
		case ISP_FETCH_YVU420_2FRAME:
			ch_offset[0] = start_row * cur_fetch->pitch[0] + start_col;
			ch_offset[1] = (start_row >> 1) * cur_fetch->pitch[1] + start_col;
			break;
		case ISP_FETCH_YUV420_2FRAME_MIPI:
		case ISP_FETCH_YVU420_2FRAME_MIPI:
			ch_offset[0] = start_row * cur_fetch->pitch[0]
				+ (start_col >> 2) * 5 + (start_col & 0x3);
			ch_offset[1] = (start_row >> 1) * cur_fetch->pitch[1]
				+ (start_col >> 2) * 5 + (start_col & 0x3);
			slc_fetch->mipi_byte_rel_pos = start_col & 0x0f;
			slc_fetch->mipi_word_num = ((((end_col + 1) >> 4) * 5
				+ mipi_word_num_end[(end_col + 1) & 0x0f])
				-(((start_col + 1) >> 4) * 5
				+ mipi_word_num_start[(start_col + 1) & 0x0f]) + 1);
			slc_fetch->mipi_byte_rel_pos_uv = slc_fetch->mipi_byte_rel_pos;
			slc_fetch->mipi_word_num_uv = slc_fetch->mipi_word_num;
			slc_fetch->mipi10_en = 1;
			ISP_PYR_DEBUG("fetch1 (%d %d %d %d), pitch %d, offset %d, mipi %d %d\n",
				start_col, start_row, end_col, end_row,
				cur_fetch->pitch[0], ch_offset[0],
				slc_fetch->mipi_byte_rel_pos, slc_fetch->mipi_word_num);
			break;
		default:
			ch_offset[0] = start_row * cur_fetch->pitch[0] + start_col * 2;
			break;
		}

		slc_fetch->addr.addr_ch0 = cur_fetch->addr[0] + ch_offset[0];
		slc_fetch->addr.addr_ch1 = cur_fetch->addr[1] + ch_offset[1];
		slc_fetch->size.h = end_row - start_row + 1;
		slc_fetch->size.w = end_col - start_col + 1;

		ISP_PYR_DEBUG("cur slice fetch1 size %d, %d\n", slc_fetch->size.w, slc_fetch->size.h);
	}

	return ret;
}

static int isppyrrec_reconstruct_get(struct isp_rec_ctx_desc *ctx, uint32_t idx)
{
	int ret = 0, i = 0;
	uint32_t start_col = 0, start_row = 0;
	uint32_t end_col = 0, end_row = 0;
	struct isp_rec_slice_desc *cur_slc = NULL;
	struct isp_pyr_rec_info *rec_cfg = NULL;
	struct slice_pyr_rec_info *slc_pyr_rec = NULL;

	if (!ctx) {
		pr_err("fail to get valid input ctx %p\n", ctx);
		return -EFAULT;
	}

	rec_cfg = &ctx->pyr_rec;
	rec_cfg->reconstruct_bypass = 0;
	rec_cfg->layer_num = idx -1;
	rec_cfg->pre_layer_width = ctx->pyr_layer_size[idx].w;
	rec_cfg->pre_layer_height = ctx->pyr_layer_size[idx].h;
	rec_cfg->cur_layer_width = ctx->pyr_layer_size[idx - 1].w;
	rec_cfg->cur_layer_height = ctx->pyr_layer_size[idx - 1].h;
	rec_cfg->out_width = ctx->pyr_layer_size[idx - 1].w;
	rec_cfg->out_height = ctx->pyr_layer_size[idx - 1].h;
	rec_cfg->hor_padding_num = 0;
	rec_cfg->ver_padding_num = 0;
	rec_cfg->drop_en = 0;
	rec_cfg->hor_padding_en = 0;
	rec_cfg->ver_padding_en = 0;
	rec_cfg->rec_path_sel = 0;
	if (rec_cfg->layer_num == 0) {
		rec_cfg->hor_padding_num = ctx->pyr_padding_size.w;
		rec_cfg->ver_padding_num = ctx->pyr_padding_size.h;
		if (rec_cfg->hor_padding_num) {
			rec_cfg->hor_padding_en = 1;
			rec_cfg->drop_en = 1;
		}
		if (rec_cfg->ver_padding_num) {
			rec_cfg->ver_padding_en = 1;
			rec_cfg->drop_en = 1;
		}
		rec_cfg->rec_path_sel = 1;
	}
	rec_cfg->reduce_flt_hblank = rec_cfg->hor_padding_num + 20;
	rec_cfg->reduce_flt_vblank = rec_cfg->ver_padding_num + 20;
	/* default param from spec */
	rec_cfg->hblank_num = 0x46;
	rec_cfg->fifo0_nfull_num = 0x157C;
	rec_cfg->fifo1_nfull_num = 0x2A94;
	rec_cfg->fifo2_nfull_num = 0x302;
	rec_cfg->fifo3_nfull_num = 0x60E;
	rec_cfg->fifo4_nfull_num = 0x15B8;
	rec_cfg->fifo5_nfull_num = 0x5B4;

	cur_slc = &ctx->slices[0];
	for (i = 0; i < ctx->slice_num; i++, cur_slc++) {
		slc_pyr_rec = &cur_slc->slice_pyr_rec;
		start_col = cur_slc->slice_fetch0_pos.start_col;
		start_row = cur_slc->slice_fetch0_pos.start_row;
		end_col = cur_slc->slice_fetch0_pos.end_col;
		end_row = cur_slc->slice_fetch0_pos.end_row;
		slc_pyr_rec->pre_layer.h = end_row - start_row + 1;
		slc_pyr_rec->pre_layer.w = end_col - start_col + 1;

		start_col = cur_slc->slice_fetch1_pos.start_col;
		start_row = cur_slc->slice_fetch1_pos.start_row;
		end_col = cur_slc->slice_fetch1_pos.end_col;
		end_row = cur_slc->slice_fetch1_pos.end_row;
		slc_pyr_rec->out.h = end_row - start_row + 1;
		slc_pyr_rec->out.w = end_col - start_col + 1;

		slc_pyr_rec->rec_path_sel = rec_cfg->rec_path_sel;
		slc_pyr_rec->ver_padding_en = rec_cfg->ver_padding_en;
		slc_pyr_rec->ver_padding_num = rec_cfg->ver_padding_num;
		slc_pyr_rec->reduce_flt_vblank = rec_cfg->ver_padding_num + 20;
		slc_pyr_rec->cur_layer.h = slc_pyr_rec->out.h + slc_pyr_rec->ver_padding_num;
		slc_pyr_rec->cur_layer.w = slc_pyr_rec->out.w;
		slc_pyr_rec->hor_padding_en = 0;
		slc_pyr_rec->hor_padding_num = 0;
		if (i == (ctx->slice_num - 1)) {
			slc_pyr_rec->hor_padding_en = rec_cfg->hor_padding_en;
			slc_pyr_rec->hor_padding_num = rec_cfg->hor_padding_num;
			slc_pyr_rec->cur_layer.w = slc_pyr_rec->out.w + slc_pyr_rec->hor_padding_num;
		}
		slc_pyr_rec->reduce_flt_hblank = slc_pyr_rec->hor_padding_num + 20;

		slc_pyr_rec->dispatch_dly_width_num = 60;
		slc_pyr_rec->dispatch_dly_height_num = 37;
		slc_pyr_rec->dispatch_pipe_full_num = 100;
		slc_pyr_rec->dispatch_mode = 1;
		slc_pyr_rec->yuv_start_row_num = 4;
		if (slc_pyr_rec->pre_layer.w <= ISP_FLASH_LIMIT_WIDTH) {
			slc_pyr_rec->width_flash_mode = 1;
			slc_pyr_rec->width_dly_num_flash = 0x0fff;
		} else {
			slc_pyr_rec->width_flash_mode = 0;
			slc_pyr_rec->width_dly_num_flash = 0x0280;
		}
		if (slc_pyr_rec->pre_layer.w <= 40 && slc_pyr_rec->pre_layer.h <= 32)
			slc_pyr_rec->dispatch_dly_height_num = 256;
		if (rec_cfg->layer_num == 0)
			slc_pyr_rec->dispatch_pipe_full_num = (4096 - slc_pyr_rec->out.w - 200) / 2;

		ISP_PYR_DEBUG("slice pyr_rec cur %d, %d pre %d %d out %d %d\n",
			slc_pyr_rec->cur_layer.w , slc_pyr_rec->cur_layer.h,
			slc_pyr_rec->pre_layer.w, slc_pyr_rec->pre_layer.h,
			slc_pyr_rec->out.w, slc_pyr_rec->out.h);
	}

	return ret;
}

static int isppyrrec_ynr_get(struct isp_rec_ctx_desc *ctx, uint32_t idx)
{
	int ret = 0, i = 0;
	uint32_t layer_num_ynr = 0;
	struct isp_rec_ynr_info *ynr_info = NULL;
	struct isp_dev_ynr_info_v3 *pyr_ynr = NULL;
	struct pyr_ynr_alg_cal ynr_alg_cal = {0};

	if (!ctx) {
		pr_err("fail to get valid input ctx %p\n", ctx);
		return -EFAULT;
	}

	ynr_info = &ctx->rec_ynr;
	pyr_ynr = ynr_info->pyr_ynr;
	ynr_info->rec_ynr_bypass = pyr_ynr->bypass;
	ynr_info->layer_num = ctx->cur_layer + 1;
	ynr_info->start.w = 0;
	ynr_info->start.h = 0;
	ynr_info->img.w = ctx->pyr_layer_size[ynr_info->layer_num].w;
	ynr_info->img.h = ctx->pyr_layer_size[ynr_info->layer_num].h;

	ynr_alg_cal.imgCenterX[0] = ctx->pyr_layer_size[0].w / 2;
	ynr_alg_cal.imgCenterY[0] = ctx->pyr_layer_size[0].h / 2;
	ynr_alg_cal.Radius[0] = ctx->rec_ynr_radius;
	layer_num_ynr = ctx->layer_num;
	for (i = 0; i < 5; i++) {
		/* some ynr param only need cfg once */
		if (ynr_info->layer_num == ctx->layer_num) {
			ynr_info->ynr_cfg_layer[i].gf_addback_clip = pyr_ynr->ynr_layer[i].gf_addback_clip;
			ynr_info->ynr_cfg_layer[i].gf_addback_en = pyr_ynr->ynr_layer[i].gf_addback_enable;
			ynr_info->ynr_cfg_layer[i].gf_addback_ratio = pyr_ynr->ynr_layer[i].gf_addback_ratio;
			ynr_info->ynr_cfg_layer[i].gf_enable = pyr_ynr->ynr_layer[i].gf_enable;
			ynr_info->ynr_cfg_layer[i].gf_epsilon_high = pyr_ynr->ynr_layer[i].gf_epsilon_high;
			ynr_info->ynr_cfg_layer[i].gf_epsilon_low = pyr_ynr->ynr_layer[i].gf_epsilon_low;
			ynr_info->ynr_cfg_layer[i].gf_epsilon_mid = pyr_ynr->ynr_layer[i].gf_epsilon_mid;
			ynr_info->ynr_cfg_layer[i].gf_radius = pyr_ynr->ynr_layer[i].gf_radius;
			ynr_info->ynr_cfg_layer[i].gf_rnr_offset = pyr_ynr->ynr_layer[i].gf_rnr_offset;
			ynr_info->ynr_cfg_layer[i].gf_rnr_ratio = pyr_ynr->ynr_layer[i].gf_rnr_ratio;
			ynr_info->ynr_cfg_layer[i].lum_thresh0 = pyr_ynr->ynr_layer[i].lum_thresh[0];
			ynr_info->ynr_cfg_layer[i].lum_thresh1 = pyr_ynr->ynr_layer[i].lum_thresh[1];
		}
		cal_max_dist(&ynr_alg_cal, layer_num_ynr, ctx->pyr_layer_size[0].w, ctx->pyr_layer_size[0].h);
		ynr_info->ynr_cfg_layer[i].imgcenter.w = ynr_alg_cal.imgCenterX[i + 1];
		ynr_info->ynr_cfg_layer[i].imgcenter.h = ynr_alg_cal.imgCenterY[i + 1];
		ynr_info->ynr_cfg_layer[i].ynr_radius = ynr_alg_cal.Radius[i + 1];
		ynr_info->ynr_cfg_layer[i].max_dist = ynr_alg_cal.max_dist[i];
	}

	return ret;
}

static int isppyrrec_cnr_get(struct isp_rec_ctx_desc *ctx, uint32_t idx)
{
	int ret = 0;
	struct isp_rec_cnr_info *cnr_info = NULL;
	uint32_t radius = 0, cnr_radius = 0;

	if (!ctx) {
		pr_err("fail to get valid input ctx %p\n", ctx);
		return -EFAULT;
	}

	cnr_info = &ctx->rec_cnr;
	cnr_info->layer_num = ctx->cur_layer + 1;
	cnr_info->rec_cnr_bypass = cnr_info->pyr_cnr->bypass;
	cnr_info->img_center.w = ctx->pyr_layer_size[cnr_info->layer_num].w / 4;
	cnr_info->img_center.h = ctx->pyr_layer_size[cnr_info->layer_num].h / 4;

	cnr_radius = ctx->rec_cnr_radius;
	radius = cnr_radius >> ctx->cur_layer;
	cnr_info->pyr_cnr->layer_cnr_h[ctx->cur_layer].radius = radius;
	pr_debug("cur_layer %d, layer_num %d, img_center w %d, h %d, radius %d\n", ctx->cur_layer, cnr_info->layer_num,
		cnr_info->img_center.w, cnr_info->img_center.h, radius);

	return ret;
}


static int isppyrrec_store_get(struct isp_rec_ctx_desc *ctx, uint32_t idx)
{
	int ret = 0, i = 0;
	uint32_t start_col = 0, start_row = 0;
	uint32_t end_col = 0, end_row = 0;
	uint32_t overlap_left = 0, overlap_up = 0;
	uint32_t overlap_right = 0, overlap_down = 0;
	uint32_t start_row_out = 0, start_col_out = 0;
	uint32_t ch_offset[3] = { 0 }, pitch = 0;
	struct isp_rec_slice_desc *cur_slc = NULL;
	struct slice_store_info *slc_rec_store = NULL;
	struct isp_rec_store_info *rec_store = NULL;

	if (!ctx || (idx < 1)) {
		pr_err("fail to get valid input ctx %p idx %d\n", ctx, idx);
		return -EFAULT;
	}

	rec_store = &ctx->rec_store;
	idx = idx - 1;
	rec_store->bypass = 0;
	rec_store->color_format = ctx->pyr_fmt;
	rec_store->addr[0] = ctx->store_addr[idx].addr_ch0;
	rec_store->addr[1] = ctx->store_addr[idx].addr_ch1;
	rec_store->width = ctx->pyr_layer_size[idx].w;
	rec_store->height = ctx->pyr_layer_size[idx].h;
	pitch = isppyrrec_pitch_get(rec_store->color_format, rec_store->width);
	rec_store->pitch[0] = pitch;
	rec_store->pitch[1] = pitch;
	rec_store->data_10b = 0;
	rec_store->mipi_en = 0;
	rec_store->flip_en = 0;
	rec_store->last_frm_en = 1;
	rec_store->mono_en = 0;
	rec_store->mirror_en = 0;
	rec_store->burst_len = 0;
	rec_store->speed2x = 1;
	rec_store->shadow_clr_sel = 1;
	rec_store->shadow_clr = 1;
	rec_store->rd_ctrl = 0;

	cur_slc = &ctx->slices[0];
	for (i = 0; i < ctx->slice_num; i++, cur_slc++) {
		slc_rec_store = &cur_slc->slice_rec_store;
		start_col = cur_slc->slice_store_pos.start_col;
		start_row = cur_slc->slice_store_pos.start_row;
		end_col = cur_slc->slice_store_pos.end_col;
		end_row = cur_slc->slice_store_pos.end_row;
		overlap_up = cur_slc->slice_overlap.overlap_up;
		overlap_down = cur_slc->slice_overlap.overlap_down;
		overlap_left = cur_slc->slice_overlap.overlap_left;
		overlap_right = cur_slc->slice_overlap.overlap_right;
		start_row_out = start_row;
		start_col_out = start_col;

		switch (rec_store->color_format) {
		case ISP_FETCH_YUV420_2FRAME_MIPI:
		case ISP_FETCH_YVU420_2FRAME_MIPI:
			ch_offset[0] = start_row_out * rec_store->pitch[0] + start_col_out * 5 /4;
			ch_offset[1] = ((start_row_out * rec_store->pitch[1]) >> 1) + start_col_out * 5 /4;
			break;
		case ISP_FETCH_YUV420_2FRAME_10:
		case ISP_FETCH_YVU420_2FRAME_10:
			ch_offset[0] = start_row_out * rec_store->pitch[0] + (start_col_out << 1);
			ch_offset[1] = ((start_row_out * rec_store->pitch[1]) >> 1) + (start_col_out << 1);
			break;
		default:
			ch_offset[0] = start_row_out * rec_store->pitch[0] + start_col_out;
			ch_offset[1] = ((start_row_out * rec_store->pitch[1]) >> 1) + start_col_out;
			break;
		}

		slc_rec_store->addr.addr_ch0 = rec_store->addr[0] + ch_offset[0];
		slc_rec_store->addr.addr_ch1 = rec_store->addr[1] + ch_offset[1];
		slc_rec_store->size.h = end_row - start_row + 1;
		slc_rec_store->size.w = end_col - start_col + 1;
		slc_rec_store->border.up_border = overlap_up;
		slc_rec_store->border.down_border = overlap_down;
		slc_rec_store->border.left_border = overlap_left;
		slc_rec_store->border.right_border = overlap_right;

		ISP_PYR_DEBUG("slice store size %d, %d\n", slc_rec_store->size.w, slc_rec_store->size.h);
	}

	return ret;
}

static int isppyrrec_block_cfg_get(struct isp_rec_ctx_desc *ctx, uint32_t idx)
{
	int ret = 0;

	if (!ctx) {
		pr_err("fail to get valid input ctx\n");
		return -EFAULT;
	}

	ret = isppyrrec_ref_fetch_get(ctx, idx);
	if (ret) {
		pr_err("fail to get isp pyr_rec ref_fetch\n");
		return ret;
	}

	ret = isppyrrec_cur_fetch_get(ctx, idx);
	if (ret) {
		pr_err("fail to get isp pyr_rec cur_fetch\n");
		return ret;
	}

	ret = isppyrrec_reconstruct_get(ctx, idx);
	if (ret) {
		pr_err("fail to get isp pyr_rec reconstruct\n");
		return ret;
	}

	ret = isppyrrec_ynr_get(ctx, idx);
	if (ret) {
		pr_err("fail to get isp pyr_rec ynr\n");
		return ret;
	}

	ret = isppyrrec_cnr_get(ctx, idx);
	if (ret) {
		pr_err("fail to get isp pyr_rec cnr\n");
		return ret;
	}

	ret = isppyrrec_store_get(ctx, idx);
	if (ret) {
		pr_err("fail to get isp pyr_rec store\n");
		return ret;
	}

	return ret;
}

static int isppyrrec_cfg_param(void *handle,
		enum isp_rec_cfg_cmd cmd, void *param)
{
	int ret = 0;
	struct isp_rec_ctx_desc *rec_ctx = NULL;
	struct camera_frame * pframe = NULL;

	if (!handle || !param) {
		pr_err("fail to get valid input ptr %p cmd%d\n", handle, cmd);
		return -EFAULT;
	}

	rec_ctx = (struct isp_rec_ctx_desc *)handle;
	switch (cmd) {
	case ISP_REC_CFG_BUF:
		pframe = (struct camera_frame *)param;
		ret = cam_buf_iommu_map(&pframe->buf, CAM_IOMMUDEV_ISP);
		if (ret) {
			pr_err("fail to map isp pyr rec iommu buf.\n");
			ret = -EINVAL;
			goto exit;
		}

		if (rec_ctx->buf_info == NULL) {
			rec_ctx->buf_info = pframe;
			pr_debug("REC buf 0x%p = 0x%lx\n", pframe,
				rec_ctx->buf_info->buf.iova[0]);
			break;
		} else
			pr_debug("REC buf 0x%p = 0x%lx\n", pframe,
				rec_ctx->buf_info->buf.iova[0]);
		break;
	case ISP_REC_CFG_LAYER_NUM:
		rec_ctx->layer_num = *(uint32_t *)param;
		pr_debug("layer num %d\n", rec_ctx->layer_num);
		break;
	case ISP_REC_CFG_WORK_MODE:
		rec_ctx->wmode = *(uint32_t *)param;
		pr_debug("work mode %d\n", rec_ctx->wmode);
		break;
	case ISP_REC_CFG_HW_CTX_IDX:
		rec_ctx->hw_ctx_id = *(uint32_t *)param;
		pr_debug("hw ctx id %d\n", rec_ctx->hw_ctx_id);
		break;
	case ISP_REC_CFG_FMCU_HANDLE:
		rec_ctx->fmcu_handle = param;
		break;
	case ISP_REC_CFG_DEWARPING_EB:
		rec_ctx->dewarp_eb = *(uint32_t *)param;
		pr_debug("dewarp eb %d\n", rec_ctx->dewarp_eb);
		break;
	default:
		pr_err("fail to get known cmd: %d\n", cmd);
		ret = -EFAULT;
		break;
	}

exit:
	return ret;
}

static int isppyrrec_pipe_proc(void *handle, void *param)
{
	int ret = 0;
	uint32_t i = 0, j = 0, layer_num = 0;
	uint32_t offset = 0, offset1 = 0, align = 1, size = 0, pitch = 0;
	struct isp_rec_ctx_desc *rec_ctx = NULL;
	struct isp_pyr_rec_in *in_ptr = NULL;
	struct isp_rec_slice_desc *cur_slc = NULL;
	struct alg_slice_drv_overlap *slice_overlap = NULL;
	struct isp_hw_k_blk_func rec_share_func, rec_frame_func, rec_slice_func;

	if (!handle || !param) {
		pr_err("fail to get valid input ptr %d\n", handle);
		return -EFAULT;
	}

	in_ptr = (struct isp_pyr_rec_in *)param;
	rec_ctx = (struct isp_rec_ctx_desc *)handle;
	layer_num = rec_ctx->layer_num;
	slice_overlap = in_ptr->slice_overlap;

	/* update layer num based on img size */
	while (isp_rec_small_layer_w(in_ptr->src.w, layer_num) < MIN_PYR_WIDTH ||
		isp_rec_small_layer_h(in_ptr->src.h, layer_num) < MIN_PYR_HEIGHT) {
		pr_debug("layer num need decrease based on small input %d %d\n",
			in_ptr->src.w, in_ptr->src.h);
		layer_num--;
	}
	rec_ctx->layer_num = layer_num;
	/* calc multi layer pyramid dec input addr & size */
	pitch = isppyrrec_pitch_get(in_ptr->in_fmt, in_ptr->src.w);
	size = pitch * in_ptr->src.h;
	rec_ctx->pyr_fmt = in_ptr->pyr_fmt;
	rec_ctx->rec_ynr.pyr_ynr = in_ptr->pyr_ynr;
	rec_ctx->rec_cnr.pyr_cnr = in_ptr->pyr_cnr;
	rec_ctx->rec_ynr_radius = in_ptr->pyr_ynr_radius;
	rec_ctx->rec_cnr_radius = in_ptr->pyr_cnr_radius;
	rec_ctx->in_fmt = in_ptr->in_fmt;
	rec_ctx->out_fmt = in_ptr->in_fmt;
	rec_ctx->fetch_addr[0] = in_ptr->in_addr;
	rec_ctx->pyr_layer_size[0].w = isp_rec_layer0_width(in_ptr->src.w, layer_num);
	rec_ctx->pyr_layer_size[0].h = isp_rec_layer0_heigh(in_ptr->src.h, layer_num);
	rec_ctx->pyr_padding_size.w = rec_ctx->pyr_layer_size[0].w - in_ptr->src.w;
	rec_ctx->pyr_padding_size.h = rec_ctx->pyr_layer_size[0].h - in_ptr->src.h;
	rec_ctx->store_addr[0].addr_ch0 = rec_ctx->buf_info->buf.iova[0];
	rec_ctx->store_addr[0].addr_ch1 = rec_ctx->store_addr[0].addr_ch0 + size;

	ISP_PYR_DEBUG("isp %d rec layer num %d\n", rec_ctx->ctx_id, layer_num);
	ISP_PYR_DEBUG("isp %d layer0 size %d %d padding %d %d\n", rec_ctx->ctx_id,
		rec_ctx->pyr_layer_size[0].w, rec_ctx->pyr_layer_size[0].h,
		rec_ctx->pyr_padding_size.w, rec_ctx->pyr_padding_size.h);
	ISP_PYR_DEBUG("in format layer0 %d fetch addr %x %x %x\n", rec_ctx->in_fmt,
		rec_ctx->fetch_addr[0].addr_ch0, rec_ctx->fetch_addr[0].addr_ch1);
	for (i = 1; i < layer_num + 1; i++) {
		align = align * 2;
		offset += (size * 3 / 2);
		if ((rec_ctx->fetch_path_sel == ISP_FETCH_PATH_FBD) && (i == 1))
			offset = rec_ctx->fbcd_buffer_size;
		rec_ctx->pyr_layer_size[i].w = rec_ctx->pyr_layer_size[0].w /align;
		rec_ctx->pyr_layer_size[i].h = rec_ctx->pyr_layer_size[0].h /align;
		pitch = isppyrrec_pitch_get(rec_ctx->pyr_fmt, rec_ctx->pyr_layer_size[i].w);
		size = pitch * rec_ctx->pyr_layer_size[i].h;
		if (i < layer_num) {
			rec_ctx->store_addr[i].addr_ch0 = rec_ctx->buf_info->buf.iova[0] + offset1;
			rec_ctx->store_addr[i].addr_ch1 = rec_ctx->store_addr[i].addr_ch0 + size;
		}
		offset1 += (size * 3 / 2);
		rec_ctx->fetch_addr[i].addr_ch0 = in_ptr->in_addr.addr_ch0 + offset;
		rec_ctx->fetch_addr[i].addr_ch1 = rec_ctx->fetch_addr[i].addr_ch0 + size;
		ISP_PYR_DEBUG("isp %d layer%d size %d %d\n", rec_ctx->ctx_id, i,
			rec_ctx->pyr_layer_size[i].w, rec_ctx->pyr_layer_size[i].h);
		ISP_PYR_DEBUG("layer %d fetch addr %x %x\n", i,
			rec_ctx->fetch_addr[i].addr_ch0, rec_ctx->fetch_addr[i].addr_ch1);
	}

	/* layer 0 fetch & store size is real size not include padding size */
	rec_ctx->pyr_layer_size[0].w = in_ptr->src.w;
	rec_ctx->pyr_layer_size[0].h = in_ptr->src.h;
	ISP_PYR_DEBUG("isp %d layer0 size %d %d\n", rec_ctx->ctx_id,
		rec_ctx->pyr_layer_size[0].w, rec_ctx->pyr_layer_size[0].h);

	rec_share_func.index = ISP_K_BLK_PYR_REC_SHARE;
	rec_ctx->hw->isp_ioctl(rec_ctx->hw, ISP_HW_CFG_K_BLK_FUNC_GET, &rec_share_func);
	rec_frame_func.index = ISP_K_BLK_PYR_REC_FRAME;
	rec_ctx->hw->isp_ioctl(rec_ctx->hw, ISP_HW_CFG_K_BLK_FUNC_GET, &rec_frame_func);
	rec_slice_func.index = ISP_K_BLK_PYR_REC_SLICE;
	rec_ctx->hw->isp_ioctl(rec_ctx->hw, ISP_HW_CFG_K_BLK_FUNC_GET, &rec_slice_func);
	for (i = layer_num; i > 0; i--) {
		rec_ctx->cur_layer = i - 1;
		rec_ctx->slice_num = slice_overlap->slice_number[i];
		ISP_PYR_DEBUG("layer %d slice num %d\n", i, rec_ctx->slice_num);
		cur_slc = &rec_ctx->slices[0];
		for (j = 0; j < rec_ctx->slice_num; j++, cur_slc++) {
			cur_slc->slice_fetch0_pos.start_row = slice_overlap->fecth0_slice_region[i][j].sy;
			cur_slc->slice_fetch0_pos.start_col = slice_overlap->fecth0_slice_region[i][j].sx;
			cur_slc->slice_fetch0_pos.end_row = slice_overlap->fecth0_slice_region[i][j].ey;
			cur_slc->slice_fetch0_pos.end_col = slice_overlap->fecth0_slice_region[i][j].ex;
			cur_slc->slice_fetch1_pos.start_row = slice_overlap->fecth1_slice_region[i - 1][j].sy;
			cur_slc->slice_fetch1_pos.start_col = slice_overlap->fecth1_slice_region[i - 1][j].sx;
			cur_slc->slice_fetch1_pos.end_row = slice_overlap->fecth1_slice_region[i - 1][j].ey;
			cur_slc->slice_fetch1_pos.end_col = slice_overlap->fecth1_slice_region[i - 1][j].ex;
			cur_slc->slice_store_pos.start_row = slice_overlap->store_rec_slice_region[i - 1][j].sy;
			cur_slc->slice_store_pos.start_col = slice_overlap->store_rec_slice_region[i - 1][j].sx;
			cur_slc->slice_store_pos.end_row = slice_overlap->store_rec_slice_region[i - 1][j].ey;
			cur_slc->slice_store_pos.end_col = slice_overlap->store_rec_slice_region[i - 1][j].ex;
			cur_slc->slice_overlap.overlap_up = slice_overlap->store_rec_slice_crop_overlap[i - 1][j].ov_up;
			cur_slc->slice_overlap.overlap_down = slice_overlap->store_rec_slice_crop_overlap[i - 1][j].ov_down;
			cur_slc->slice_overlap.overlap_left = slice_overlap->store_rec_slice_crop_overlap[i - 1][j].ov_left;
			cur_slc->slice_overlap.overlap_right = slice_overlap->store_rec_slice_crop_overlap[i - 1][j].ov_right;
		}
		ret = isppyrrec_block_cfg_get(rec_ctx, i);
		if (ret) {
			pr_err("fail to get isp pyr_rec block_cfg\n");
			return ret;
		}

		if (i == layer_num && rec_share_func.k_blk_func)
			rec_share_func.k_blk_func(rec_ctx);

		if (rec_frame_func.k_blk_func)
			rec_frame_func.k_blk_func(rec_ctx);
		if (i != 1) {
			for (j = 0; j < rec_ctx->slice_num; j++) {
				rec_ctx->cur_slice_id = j;
				if (rec_slice_func.k_blk_func)
					rec_slice_func.k_blk_func(rec_ctx);
			}
		}
	}

	return ret;
}

void *isp_pyr_rec_ctx_get(uint32_t idx, void *hw)
{
	struct isp_rec_ctx_desc *rec_ctx = NULL;

	rec_ctx = vzalloc(sizeof(struct isp_rec_ctx_desc));
	if (!rec_ctx)
		return NULL;

	rec_ctx->ctx_id = idx;
	rec_ctx->hw = hw;
	rec_ctx->ops.cfg_param = isppyrrec_cfg_param;
	rec_ctx->ops.pipe_proc = isppyrrec_pipe_proc;

	return rec_ctx;
}

void isp_pyr_rec_ctx_put(void *rec_handle)
{
	struct isp_rec_ctx_desc *rec_ctx = NULL;
	struct camera_buf *buf_info = NULL;

	if (!rec_handle) {
		pr_err("fail to get valid rec handle\n");
		return;
	}

	rec_ctx = (struct isp_rec_ctx_desc *)rec_handle;
	if (rec_ctx->buf_info) {
		buf_info = &rec_ctx->buf_info->buf;
		if (buf_info && buf_info->mapping_state & CAM_BUF_MAPPING_DEV) {
			cam_buf_iommu_unmap(buf_info);
			buf_info = NULL;
		}
	}

	if (rec_ctx)
		vfree(rec_ctx);
	rec_ctx = NULL;
}
