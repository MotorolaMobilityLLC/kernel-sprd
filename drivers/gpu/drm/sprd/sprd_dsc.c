// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/module.h>
#include "sprd_dsc.h"
#include "sprd_dpu.h"
#include "sprd_dsi_panel.h"

static void set_defaults(struct dsc_init_param *dsc_init, int bpc)
{
	int i;

	/* Default is for 8bpc/8bpp */
	int default_rcofs[] = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12 };
	int default_minqp[] = { 0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 5, 7, 13 };
	int default_maxqp[] = { 4, 4, 5, 6, 7, 7, 7, 8, 9, 10, 11, 12, 13, 13, 15 };
	int default_threshold[] = { 896, 1792, 2688, 3584, 4480, 5376, 6272, 6720, 7168, 7616,
							7744, 7872, 8000, 8064 };
	int default_rcofs_10[] = { 2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -10, -12, -12,
					-12 };
	int default_minqp_10[] = { 0, 4, 5, 6, 7, 7, 7, 7, 7, 7, 9, 9, 9, 11, 15 };
	int default_maxqp_10[] = { 7, 8, 9, 10, 11, 11, 11, 12, 13, 13, 14, 14, 15, 15, 16 };

	dsc_init->rc_model_size = 8192;
	dsc_init->bit_per_pixel = bpc;
	dsc_init->bit_per_component = bpc;
	dsc_init->enable_422 = 0;
	dsc_init->init_simple_422 = 0;
	dsc_init->bp_enable = 1;
	dsc_init->initial_fullness_ofs = (bpc == 8) ? 6144 : 5632;
	dsc_init->initial_delay = (bpc == 8) ? 475 : 410;
	dsc_init->first_linebpg_ofs = 12;
	dsc_init->init_slice_width = 0;
	dsc_init->init_slice_height = 0;
	dsc_init->use_yuv_input = 0;

	dsc_init->line_buffer_bpc = dsc_init->bit_per_component + 1;

	/* Standards compliant DPX settings */
	dsc_init->enable_vbr = 0;
	dsc_init->init_native_420 = 0;
	dsc_init->init_native_422 = 0;
	dsc_init->second_linebpg_ofs = -1;

	dsc_init->init_dsc_version_minor = 2;
	dsc_init->tgt_offset_hi = 3;
	dsc_init->tgt_offset_lo = 3;
	dsc_init->rc_edge_factor = 6;
	dsc_init->init_quant_incr_limit0 = (bpc == 8) ? 11 : 15;
	dsc_init->init_quant_incr_limit1 = (bpc == 8) ? 11 : 15;
	dsc_init->flatness_minqp = (bpc == 8) ? 3 : 7;
	dsc_init->flatness_maxqp = (bpc == 8) ? 12 : 16;
	dsc_init->init_flatness_det_thresh = (bpc == 8) ? 2 : 8;
	dsc_init->init_mux_word_size = 0;
	dsc_init->init_pic_width = 1920;
	dsc_init->init_pic_height = 1080;

	for (i = 0; i < 15; ++i) {
		dsc_init->rc_offset[i] = (bpc == 8) ? default_rcofs[i] : default_rcofs_10[i];
		dsc_init->rc_minqp[i] = (bpc == 8) ? default_minqp[i] : default_minqp_10[i];
		dsc_init->rc_maxqp[i] = (bpc == 8) ? default_maxqp[i] : default_maxqp_10[i];
		if (i < 14)
			dsc_init->rc_buf_thresh[i] = default_threshold[i];
	}
}

static int calc_rc_params(struct dsc_init_param *dsc_init, struct dsc_cfg *dsc_cfg,
					int pixelsPerGroup, int numSsps)
{
	int groupsPerLine;
	int num_extra_mux_bits;
	int sliceBits;
	int final_value;
	int final_scale;
	int invalid = 0;
	int groups_total;
	int slicew;

	slicew = dsc_cfg->slice_width;

	if (dsc_init->first_linebpg_ofs < 0) {
		if (dsc_cfg->slice_height >= 8)
			dsc_init->first_linebpg_ofs = 12 + ((int)(9/100 * MIN(34,
							dsc_cfg->slice_height - 8)));
		else
			dsc_init->first_linebpg_ofs = 2 * (dsc_cfg->slice_height - 1);
	}
	if (dsc_init->second_linebpg_ofs < 0)
		dsc_init->second_linebpg_ofs = dsc_init->init_native_420 ? 12 : 0;

	dsc_cfg->first_line_bpg_ofs = dsc_init->first_linebpg_ofs;
	dsc_cfg->second_line_bpg_ofs = dsc_init->second_linebpg_ofs;

	groupsPerLine = (slicew + pixelsPerGroup-1) / pixelsPerGroup;
	dsc_cfg->chunk_size = (int) (((slicew * dsc_init->bit_per_pixel) + 7) / 8);

	if (dsc_cfg->convert_rgb)
		num_extra_mux_bits = (numSsps * (dsc_init->init_mux_word_size +
							(4 * dsc_init->bit_per_component + 4) - 2));
	else if (!dsc_cfg->native_422)
		num_extra_mux_bits = (numSsps * dsc_init->init_mux_word_size
				+ (4 * dsc_init->bit_per_component + 4)
				+ 2 * (4*dsc_init->bit_per_component) - 2);
	else
		num_extra_mux_bits = (numSsps*dsc_init->init_mux_word_size
						+ (4 * dsc_init->bit_per_component + 4)
						+ 3 * (4 * dsc_init->bit_per_component) - 2);
	sliceBits = 8 * dsc_cfg->chunk_size * dsc_cfg->slice_height;
	while ((num_extra_mux_bits > 0) && ((sliceBits - num_extra_mux_bits)
						% dsc_init->init_mux_word_size))
		num_extra_mux_bits--;

	if (groupsPerLine < dsc_cfg->initial_scale_value - 8)
		dsc_cfg->initial_scale_value = groupsPerLine + 8;
	if (dsc_cfg->initial_scale_value > 8)
		dsc_cfg->scale_decrement_interval = groupsPerLine
							/ (dsc_cfg->initial_scale_value - 8);
	else
		dsc_cfg->scale_decrement_interval = 4095;

	dsc_cfg->initial_xmit_delay = dsc_init->initial_delay;
	final_value = dsc_cfg->rc_model_size - ((dsc_cfg->initial_xmit_delay
			* dsc_cfg->bits_per_pixel + 8)>>4) + num_extra_mux_bits;
	dsc_cfg->final_offset = final_value;
	final_scale = 8 * dsc_cfg->rc_model_size / (dsc_cfg->rc_model_size - final_value);
	if (dsc_cfg->slice_height > 1)
		dsc_cfg->nfl_bpg_offset = (int)(((dsc_cfg->first_line_bpg_ofs
						<< OFFSET_FRACTIONAL_BITS)
						+ (dsc_cfg->slice_height - 1) - 1)
						/ (dsc_cfg->slice_height - 1));
	else
		dsc_cfg->nfl_bpg_offset = 0;
	if (dsc_cfg->nfl_bpg_offset > 65535)
		invalid = 1;
	if (dsc_cfg->slice_height > 2)
		dsc_cfg->nsl_bpg_offset = (int)(((dsc_cfg->second_line_bpg_ofs
						<< OFFSET_FRACTIONAL_BITS)
						+ (dsc_cfg->slice_height - 1) - 1)
						/ (dsc_cfg->slice_height - 1));
	else
		dsc_cfg->nsl_bpg_offset = 0;
	if (dsc_cfg->nsl_bpg_offset > 65535)
		invalid = 1;
	groups_total = groupsPerLine * dsc_cfg->slice_height;
	dsc_cfg->slice_bpg_offset = (int)(((1<<OFFSET_FRACTIONAL_BITS) *
					(dsc_cfg->rc_model_size - dsc_cfg->initial_offset
					+ num_extra_mux_bits)
					+ groups_total - 1) / (groups_total));

	if (dsc_cfg->slice_height == 1) {
		if (dsc_cfg->first_line_bpg_ofs > 0)
			DRM_ERROR("For slice_height == 1, the FIRST_LINE_BPG_OFFSET must be 0\n");
	} else if (pixelsPerGroup * dsc_init->bit_per_pixel -
					((dsc_cfg->slice_bpg_offset + dsc_cfg->nfl_bpg_offset)
					/ (1<<OFFSET_FRACTIONAL_BITS)) < (1+5*pixelsPerGroup))
		DRM_ERROR("The bits/pixel allocation too low ");
	if (final_scale > 9) {
		dsc_cfg->scale_increment_interval = (int)((1<<OFFSET_FRACTIONAL_BITS)
					* dsc_cfg->final_offset / ((final_scale - 9)
					* (dsc_cfg->nfl_bpg_offset
					+ dsc_cfg->slice_bpg_offset + dsc_cfg->nsl_bpg_offset)));
	} else
		dsc_cfg->scale_increment_interval = 0;

	return invalid;
}

static void write_bits(int val, int size, unsigned char *buf, int *bit_count)
{
	int curbit;
	int bitcntmod8;
	int bufidx;
	int i;

	if (size > 32)
		DRM_ERROR("error: supports max of 32 bits\n");

	for (i = size-1; i >= 0; --i) {
		bitcntmod8 = (*bit_count)%8;
		bufidx = (*bit_count)>>3;
		curbit = (val >> i) & 1;
		if (bitcntmod8 == 0)
			buf[bufidx] = 0;
		if (curbit)
			buf[bufidx] |= (1<<(7-bitcntmod8));
		(*bit_count)++;
	}
}

static void write_pps(unsigned char *buf, struct dsc_cfg *dsc_cfg)
{
	int nbits = 0;
	int i;

	write_bits(1, 4, buf, &nbits);
	write_bits(dsc_cfg->dsc_version_minor, 4, buf, &nbits);
	write_bits(dsc_cfg->pps_identifier, 8, buf, &nbits);
	write_bits(0, 8, buf, &nbits);
	write_bits(dsc_cfg->bits_per_component, 4, buf, &nbits);
	write_bits(dsc_cfg->linebuf_depth, 4, buf, &nbits);
	write_bits(0, 2, buf, &nbits);
	write_bits(dsc_cfg->block_pred_enable, 1, buf, &nbits);
	write_bits(dsc_cfg->convert_rgb, 1, buf, &nbits);
	write_bits(dsc_cfg->simple_422, 1, buf, &nbits);
	write_bits(dsc_cfg->vbr_enable, 1, buf, &nbits);
	write_bits(dsc_cfg->bits_per_pixel, 10, buf, &nbits);
	write_bits(dsc_cfg->pic_height, 16, buf, &nbits);
	write_bits(dsc_cfg->pic_width, 16, buf, &nbits);
	write_bits(dsc_cfg->slice_height, 16, buf, &nbits);
	write_bits(dsc_cfg->slice_width, 16, buf, &nbits);
	write_bits(dsc_cfg->chunk_size, 16, buf, &nbits);
	write_bits(0, 6, buf, &nbits);
	write_bits(dsc_cfg->initial_xmit_delay, 10, buf, &nbits);
	write_bits(dsc_cfg->initial_dec_delay, 16, buf, &nbits);
	write_bits(0, 10, buf, &nbits);
	write_bits(dsc_cfg->initial_scale_value, 6, buf, &nbits);
	write_bits(dsc_cfg->scale_increment_interval, 16, buf, &nbits);
	write_bits(0, 4, buf, &nbits);
	write_bits(dsc_cfg->scale_decrement_interval, 12, buf, &nbits);
	write_bits(0, 11, buf, &nbits);
	write_bits(dsc_cfg->first_line_bpg_ofs, 5, buf, &nbits);
	write_bits(dsc_cfg->nfl_bpg_offset, 16, buf, &nbits);
	write_bits(dsc_cfg->slice_bpg_offset, 16, buf, &nbits);
	write_bits(dsc_cfg->initial_offset, 16, buf, &nbits);
	write_bits(dsc_cfg->final_offset, 16, buf, &nbits);
	write_bits(0, 3, buf, &nbits);
	write_bits(dsc_cfg->flatness_min_qp, 5, buf, &nbits);
	write_bits(0, 3, buf, &nbits);
	write_bits(dsc_cfg->flatness_max_qp, 5, buf, &nbits);

	/* RC parameter set */
	write_bits(dsc_cfg->rc_model_size, 16, buf, &nbits);
	write_bits(0, 4, buf, &nbits);
	write_bits(dsc_cfg->rc_edge_factor, 4, buf, &nbits);
	write_bits(0, 3, buf, &nbits);
	write_bits(dsc_cfg->rc_quant_incr_limit0, 5, buf, &nbits);
	write_bits(0, 3, buf, &nbits);
	write_bits(dsc_cfg->rc_quant_incr_limit1, 5, buf, &nbits);
	write_bits(dsc_cfg->rc_tgt_offset_hi, 4, buf, &nbits);
	write_bits(dsc_cfg->rc_tgt_offset_lo, 4, buf, &nbits);

	for (i = 0; i < 14; ++i)
		write_bits(dsc_cfg->rc_buf_thresh[i]>>6, 8, buf, &nbits);

	for (i = 0; i < 15; ++i) {
		write_bits(dsc_cfg->rc_range_params[i].range_min_qp, 5, buf, &nbits);
		write_bits(dsc_cfg->rc_range_params[i].range_max_qp, 5, buf, &nbits);
		write_bits(dsc_cfg->rc_range_params[i].range_bpg_offset, 6, buf, &nbits);
	}

}

static void write_reg(struct dsc_cfg *dsc_cfg, unsigned char *pps)
{
	u32 cfg;

	dsc_cfg->reg.dsc_ctrl = (dsc_cfg->rc_model_size<<16)
				| (dsc_cfg->flatness_det_thresh << 2) | 3;
	dsc_cfg->reg.dsc_pic_size = (dsc_cfg->pic_height << 16) | (dsc_cfg->pic_width);
	dsc_cfg->reg.dsc_grp_size = (dsc_cfg->slice_height << 16)
						| ((dsc_cfg->slice_width % 3) << 11)
						| (dsc_cfg->slice_width + 2) / 3;

	if (dsc_cfg->slice_width == dsc_cfg->pic_width / 2)
		cfg = 1 << 26;
	else
		cfg = 0;

	if (16 == dsc_cfg->slice_height)
		cfg = cfg | (3 << 24);
	else if (32 == dsc_cfg->slice_height)
		cfg = cfg | (2 << 24);
	else if (dsc_cfg->slice_height != dsc_cfg->pic_height)
		cfg = cfg | (1 << 24);

	dsc_cfg->reg.dsc_slice_size = cfg | dsc_cfg->slice_height * ((dsc_cfg->slice_width + 2) / 3);

	dsc_cfg->reg.dsc_cfg0 = ((pps[37] & 0x1f) << 26) | ((pps[36] & 0x1f) << 21)
		| ((pps[27] & 0x1f) << 16) | ((pps[21] & 0x3f) << 10)
		| ((pps[16] & 0x3) << 8) | pps[17];
	dsc_cfg->reg.dsc_cfg1 = ((pps[24] & 0xf) << 24) | (pps[25] << 16)
		| (pps[22] << 8) | pps[23];
	dsc_cfg->reg.dsc_cfg2 = (pps[30] << 24) | (pps[31] << 16) |
		(pps[28] << 8) | pps[29];
	dsc_cfg->reg.dsc_cfg3 = (pps[34] << 24) | (pps[35] << 16) |
		(pps[32] << 8) | pps[33];
	dsc_cfg->reg.dsc_cfg4 = (pps[43] << 16) | ((pps[42] & 0x1f) << 9) |
		((pps[41] & 0x1f) << 4) | (pps[40] & 0xf);
	dsc_cfg->reg.dsc_cfg5 = (pps[47] << 24) | (pps[46] << 16) |
		(pps[45] << 8) | pps[44];
	dsc_cfg->reg.dsc_cfg6 = (pps[51] << 24) | (pps[50] << 16) |
		(pps[49] << 8) | pps[48];
	dsc_cfg->reg.dsc_cfg7 = (pps[55] << 24) | (pps[54] << 16) |
		(pps[53] << 8) | pps[52];
	dsc_cfg->reg.dsc_cfg8 = (pps[57] << 8) | pps[56];
	dsc_cfg->reg.dsc_cfg9 = (pps[60] << 24) | (pps[61] << 16) |
		(pps[58] << 8) | pps[59];
	dsc_cfg->reg.dsc_cfg10 = (pps[64] << 24) | (pps[65] << 16) |
		(pps[62] << 8) | pps[63];
	dsc_cfg->reg.dsc_cfg11 = (pps[68] << 24) | (pps[69] << 16) |
		(pps[66] << 8) | pps[67];
	dsc_cfg->reg.dsc_cfg12 = (pps[72] << 24) | (pps[73] << 16) |
		(pps[70] << 8) | pps[71];
	dsc_cfg->reg.dsc_cfg13 = (pps[76] << 24) | (pps[77] << 16) |
		(pps[74] << 8) | pps[75];
	dsc_cfg->reg.dsc_cfg14 = (pps[80] << 24) | (pps[81] << 16) |
		(pps[78] << 8) | pps[79];
	dsc_cfg->reg.dsc_cfg15 = (pps[84] << 24) | (pps[85] << 16) |
		(pps[82] << 8) | pps[83];
	dsc_cfg->reg.dsc_cfg16 = (pps[86] << 8) | pps[87];
}

int calc_dsc_params(struct dsc_init_param *dsc_init)
{
	struct dpu_context *ctx =
		(struct dpu_context *)container_of(dsc_init, struct dpu_context, dsc_init);
	struct sprd_dpu *dpu =
		(struct sprd_dpu *)container_of(ctx, struct sprd_dpu, ctx);
	struct sprd_panel *panel =
		(struct sprd_panel *)container_of(dpu->dsi->panel, struct sprd_panel, base);

	int slicew, sliceh;
	int target_bpp_x16;
	int prev_min_qp, prev_max_qp, prev_thresh, prev_offset;
	unsigned char pps[PPS_SIZE];
	int pixelsPerGroup = 3, numSsps = 3;
	int i;

	set_defaults(&ctx->dsc_init, panel->info.output_bpc);

	memset(&ctx->dsc_cfg, 0, sizeof(struct dsc_cfg));

	ctx->dsc_init.init_pic_width = ctx->vm.hactive;
	ctx->dsc_init.init_pic_height = ctx->vm.vactive;
	ctx->dsc_init.init_slice_width = panel->info.slice_width;
	ctx->dsc_init.init_slice_height = panel->info.slice_height;
	memset(pps, 0, PPS_SIZE);

	ctx->dsc_cfg.dsc_version_minor = ctx->dsc_init.init_dsc_version_minor;

	ctx->dsc_cfg.pic_width = ctx->dsc_init.init_pic_width;
	ctx->dsc_cfg.pic_height = ctx->dsc_init.init_pic_height;
	ctx->dsc_cfg.bits_per_component = ctx->dsc_init.bit_per_component;
	ctx->dsc_cfg.linebuf_depth = ctx->dsc_init.line_buffer_bpc;

	if (ctx->dsc_init.init_mux_word_size == 0) {
		if (ctx->dsc_cfg.bits_per_component <= 10)
			ctx->dsc_init.init_mux_word_size = 48;
		else
			ctx->dsc_init.init_mux_word_size = 64;
	}
	ctx->dsc_cfg.mux_word_size = ctx->dsc_init.init_mux_word_size;
	ctx->dsc_cfg.convert_rgb = !ctx->dsc_init.use_yuv_input;
	ctx->dsc_cfg.rc_tgt_offset_hi = ctx->dsc_init.tgt_offset_hi;

	ctx->dsc_cfg.rc_tgt_offset_lo = ctx->dsc_init.tgt_offset_lo;
	target_bpp_x16 = (int)(ctx->dsc_init.bit_per_pixel * 16 + 5/10);
	ctx->dsc_cfg.bits_per_pixel = target_bpp_x16;

	ctx->dsc_cfg.rc_edge_factor = ctx->dsc_init.rc_edge_factor;
	ctx->dsc_cfg.rc_quant_incr_limit1 = ctx->dsc_init.init_quant_incr_limit1;
	ctx->dsc_cfg.rc_quant_incr_limit0 = ctx->dsc_init.init_quant_incr_limit0;
	prev_min_qp = ctx->dsc_init.rc_minqp[0];
	prev_max_qp = ctx->dsc_init.rc_maxqp[0];
	prev_thresh = ctx->dsc_init.rc_buf_thresh[0];
	prev_offset = ctx->dsc_init.rc_offset[0];
	for (i = 0; i < NUM_BUF_RANGES; ++i) {
		ctx->dsc_cfg.rc_range_params[i].range_bpg_offset = ctx->dsc_init.rc_offset[i];
		ctx->dsc_cfg.rc_range_params[i].range_max_qp = ctx->dsc_init.rc_maxqp[i];
		ctx->dsc_cfg.rc_range_params[i].range_min_qp = ctx->dsc_init.rc_minqp[i];
		if (i < NUM_BUF_RANGES-1) {
			ctx->dsc_cfg.rc_buf_thresh[i] = ctx->dsc_init.rc_buf_thresh[i];
			prev_thresh = ctx->dsc_init.rc_buf_thresh[i];
		}
	}
	ctx->dsc_cfg.rc_model_size = ctx->dsc_init.rc_model_size;
	ctx->dsc_cfg.initial_xmit_delay = ctx->dsc_init.initial_delay;
	ctx->dsc_cfg.block_pred_enable = ctx->dsc_init.bp_enable;
	ctx->dsc_cfg.initial_offset = ctx->dsc_init.initial_fullness_ofs;

	ctx->dsc_cfg.flatness_min_qp = ctx->dsc_init.flatness_minqp;
	ctx->dsc_cfg.flatness_max_qp = ctx->dsc_init.flatness_maxqp;
	ctx->dsc_cfg.flatness_det_thresh = ctx->dsc_init.init_flatness_det_thresh;
	ctx->dsc_cfg.initial_scale_value = 8 * ctx->dsc_cfg.rc_model_size
					/ (ctx->dsc_cfg.rc_model_size
					- ctx->dsc_cfg.initial_offset);
	ctx->dsc_cfg.vbr_enable = ctx->dsc_init.enable_vbr;

	slicew = (ctx->dsc_init.init_slice_width ? ctx->dsc_init.init_slice_width
			: ctx->dsc_cfg.pic_width);
	sliceh = (ctx->dsc_init.init_slice_height ? ctx->dsc_init.init_slice_height
			: ctx->dsc_cfg.pic_height);

	ctx->dsc_cfg.slice_width = slicew;
	ctx->dsc_cfg.slice_height = sliceh;

	if (calc_rc_params(&ctx->dsc_init, &ctx->dsc_cfg, pixelsPerGroup, numSsps))
		DRM_ERROR("One or more PPS parameters exceeded their allowed bit depth.");

	write_pps(pps, &ctx->dsc_cfg);

	write_reg(&ctx->dsc_cfg, pps);

	return 0;
}

MODULE_AUTHOR("Zhen Gao <zhen.gao@unisoc.com>");
MODULE_DESCRIPTION("Display DSC Parameters Calculate");
MODULE_LICENSE("GPL v2");
