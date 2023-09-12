/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_DSC_H_
#define _SPRD_DSC_H_

#include <linux/list.h>
#include <drm/drm_print.h>

#define NUM_BUF_RANGES		15
#define OFFSET_FRACTIONAL_BITS	11
#define PPS_SIZE		128

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

struct dsc_range_cfg {
	int  range_min_qp;
	int  range_max_qp;
	int  range_bpg_offset;
};

struct dsc_init_param {
	int rc_model_size;
	int bit_per_pixel;
	int bit_per_component;
	int enable_422;
	int init_simple_422;
	int init_native_422;
	int init_native_420;
	int line_buffer_bpc;
	int bp_enable;
	int initial_delay;
	int init_slice_width;
	int init_slice_height;
	int first_linebpg_ofs;
	int second_linebpg_ofs;
	int initial_fullness_ofs;
	int use_yuv_input;
	int rc_offset[15];
	int rc_minqp[15];
	int rc_maxqp[15];
	int rc_buf_thresh[14];
	int tgt_offset_hi;
	int tgt_offset_lo;
	int rc_edge_factor;
	int init_quant_incr_limit0;
	int init_quant_incr_limit1;
	int flatness_minqp;
	int flatness_maxqp;
	int init_flatness_det_thresh;
	int enable_vbr;
	int init_mux_word_size;
	int init_pic_width;
	int init_pic_height;
	int init_dsc_version_minor;
};

struct dsc_reg {
	u32 dsc_ctrl;
	u32 dsc_pic_size;
	u32 dsc_grp_size;
	u32 dsc_slice_size;
	u32 dsc_h_timing;
	u32 dsc_v_timing;
	u32 dsc_cfg0;
	u32 dsc_cfg1;
	u32 dsc_cfg2;
	u32 dsc_cfg3;
	u32 dsc_cfg4;
	u32 dsc_cfg5;
	u32 dsc_cfg6;
	u32 dsc_cfg7;
	u32 dsc_cfg8;
	u32 dsc_cfg9;
	u32 dsc_cfg10;
	u32 dsc_cfg11;
	u32 dsc_cfg12;
	u32 dsc_cfg13;
	u32 dsc_cfg14;
	u32 dsc_cfg15;
	u32 dsc_cfg16;
};

struct dsc_cfg {
	int linebuf_depth;
	int bits_per_component;
	int convert_rgb;
	int slice_width;
	int slice_height;
	int simple_422;
	int native_422;
	int native_420;
	int pic_width;
	int pic_height;
	int rc_tgt_offset_hi;
	int rc_tgt_offset_lo;
	int bits_per_pixel;
	int rc_edge_factor;
	int rc_quant_incr_limit1;
	int rc_quant_incr_limit0;
	int initial_xmit_delay;
	int initial_dec_delay;
	int block_pred_enable;
	int first_line_bpg_ofs;
	int second_line_bpg_ofs;
	int initial_offset;
	int rc_buf_thresh[NUM_BUF_RANGES-1];
	int rc_model_size;
	int flatness_min_qp;
	int flatness_max_qp;
	int flatness_det_thresh;
	int initial_scale_value;
	int scale_decrement_interval;
	int scale_increment_interval;
	int nfl_bpg_offset;
	int nsl_bpg_offset;
	int slice_bpg_offset;
	int final_offset;
	int vbr_enable;
	int mux_word_size;
	int chunk_size;
	int pps_identifier;
	int dsc_version_minor;
	struct dsc_reg reg;
	struct dsc_range_cfg rc_range_params[NUM_BUF_RANGES];
};

int calc_dsc_params(struct dsc_init_param *dsc_init);

#endif /* _SPRD_DSC_H_ */
