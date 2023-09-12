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

#ifndef _ISP_PYR_DEC_H_
#define _ISP_PYR_DEC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cam_queue.h"
#include "isp_slice.h"

#ifdef ISP_PYR_DEC_DEBUG_ON
#define ISP_DEC_DEBUG pr_info
#else
#define ISP_DEC_DEBUG pr_debug
#endif

#define PYR_DEC_ADDR_NUM        ISP_PYR_DEC_LAYER_NUM + 1

typedef int(*isppyrdec_irq_proc_func)(void *handle);

enum {
	ISP_PYR_DEC_STORE_DCT,
	ISP_PYR_DEC_STORE_DEC,
	ISP_PYR_DEC_MAX,
};

enum isp_dec_cfg_cmd {
	ISP_DEC_CFG_IN_FORMAT,
	ISP_DEC_CFG_MAX,
};

struct isp_dec_ops {
	int (*cfg_param)(void *dec_handle, int ctx_id, enum isp_dec_cfg_cmd cmd, void *in_fmt, void *pyr_fmt);
	int (*proc_frame)(void *dec_handle, void *param);
	int (*set_callback)(void *dec_handle, int ctx_id, isp_dev_callback cb, void *priv_data);
	int (*get_out_buf_cb)(void *dec_handle, int ctx_id, pyr_dec_buf_cb cb, void *priv_data);
	int (*proc_init)(void *dec_handle);
	int (*proc_deinit)(void *dec_handle, int ctx_id);
};

struct isp_dec_sw_ctx {
	atomic_t cap_cnt;
	uint32_t in_irq_handler;
	struct camera_frame *buf_out;
	isp_dev_callback cb_func;
	void *cb_priv_data;
	pyr_dec_buf_cb buf_cb_func;
	void *buf_cb_priv_data;
};

struct isp_dec_fetch_info {
	uint32_t bypass;
	uint32_t color_format;
	uint32_t width;
	uint32_t height;
	uint32_t pitch[2];
	uint32_t addr[2];
	uint32_t mipi_word;
	uint32_t mipi_byte;
	uint32_t mipi10_en;
	uint32_t chk_sum_clr_en;
	uint32_t ft1_axi_reorder_en;
	uint32_t ft0_axi_reorder_en;
	uint32_t substract;
	uint32_t ft1_max_len_sel;
	uint32_t ft1_retain_num;
	uint32_t ft0_max_len_sel;
	uint32_t ft0_retain_num;
};

struct isp_dec_store_info {
	uint32_t bypass;
	uint32_t endian;
	uint32_t mono_en;
	uint32_t color_format;
	uint32_t burst_len;
	uint32_t mirror_en;
	uint32_t flip_en;
	uint32_t speed2x;
	uint32_t shadow_clr_sel;
	uint32_t last_frm_en;
	uint32_t pitch[2];
	uint32_t addr[2];
	uint32_t data_10b;
	uint32_t mipi_en;
	uint32_t width;
	uint32_t height;
	uint32_t border_up;
	uint32_t border_down;
	uint32_t border_left;
	uint32_t border_right;
	uint32_t rd_ctrl;
	uint32_t shadow_clr;
	uint32_t slice_offset;
	uint32_t uvdelay_bypass;
	uint32_t uvdelay_slice_width;
};

struct isp_dec_offline_info {
	uint32_t bypass;
	uint32_t fmcu_path_sel;
	uint32_t fetch_path_sel;
	uint32_t vector_channel_idx;
	uint32_t chksum_wrk_mode;
	uint32_t chksum_clr_mode;
	uint32_t hor_padding_en;
	uint32_t hor_padding_num;
	uint32_t ver_padding_en;
	uint32_t ver_padding_num;
	uint32_t dispatch_dbg_mode_ch0;
	uint32_t dispatch_done_cfg_mode;
	uint32_t dispatch_width_dly_num_flash;
	uint32_t dispatch_pipe_nfull_num;
	uint32_t dispatch_pipe_flush_num;
	uint32_t dispatch_pipe_hblank_num;
	uint32_t dispatch_yuv_start_order;
	uint32_t dispatch_yuv_start_row_num;
	uint32_t dispatch_width_flash_mode;
};

struct slice_pyr_dec_info {
	uint32_t hor_padding_en;
	uint32_t hor_padding_num;
	uint32_t ver_padding_en;
	uint32_t ver_padding_num;
	uint32_t dispatch_dly_width_num;
	uint32_t dispatch_dly_height_num;
};

struct isp_dec_slice_desc {
	struct slice_pos_info slice_fetch_pos;
	struct slice_pos_info slice_store_dct_pos;
	struct slice_pos_info slice_store_dec_pos;
	struct slice_overlap_info slice_dct_overlap;
	struct slice_overlap_info slice_dec_overlap;
	struct slice_fetch_info slice_fetch;
	struct slice_store_info slice_dec_store;
	struct slice_store_info slice_dct_store;
	struct slice_pyr_dec_info slice_pyr_dec;
};

struct isp_dec_overlap_info {
	uint32_t slice_num;
	struct slice_pos_info slice_fetch_region[SLICE_NUM_MAX];
	struct slice_pos_info slice_store_region[SLICE_NUM_MAX];
	struct slice_overlap_info slice_fetch_overlap[SLICE_NUM_MAX];
	struct slice_overlap_info slice_store_overlap[SLICE_NUM_MAX];
};

struct isp_dec_dct_ynr_info {
	uint32_t dct_radius;
	uint32_t old_width;
	uint32_t old_height;
	uint32_t new_width;
	uint32_t new_height;
	uint32_t sensor_width;
	uint32_t sensor_height;
	struct img_size img;
	struct img_size start[SLICE_NUM_MAX];
	struct isp_dev_dct_info *dct;
};

struct isp_dec_pipe_dev {
	uint32_t layer_num;
	uint32_t slice_num;
	uint32_t cur_layer_id;
	uint32_t cur_slice_id;
	uint32_t fetch_path_sel;
	uint32_t cur_ctx_id;
	uint32_t irq_no;
	atomic_t proc_eb;
	irq_handler_t isr_func;
	isppyrdec_irq_proc_func irq_proc_func;
	void *isp_handle;
	void *fmcu_handle;
	struct cam_hw_info *hw;
	struct cam_thread_info thread;
	struct completion frm_done;
	struct isp_dec_ops ops;
	struct isp_dec_sw_ctx sw_ctx[ISP_CONTEXT_SW_NUM];

	struct camera_queue in_queue;
	struct camera_queue proc_queue;

	uint32_t in_fmt;
	uint32_t out_fmt;
	uint32_t pyr_fmt;
	struct img_size src;
	struct img_size dec_padding_size;
	struct img_size dec_layer_size[PYR_DEC_ADDR_NUM];
	struct img_addr fetch_addr[ISP_PYR_DEC_LAYER_NUM];
	struct img_addr store_addr[PYR_DEC_ADDR_NUM];
	struct isp_dec_overlap_info overlap_dec_info[MAX_PYR_DEC_LAYER_NUM];
	struct isp_fbd_yuv_info yuv_afbd_info;
	struct isp_dec_fetch_info fetch_dec_info;
	struct isp_dec_offline_info offline_dec_info;
	struct isp_dec_store_info store_dec_info;
	struct isp_dec_store_info store_dct_info;
	struct isp_dec_dct_ynr_info dct_ynr_info;
	struct isp_dec_slice_desc slices[SLICE_NUM_MAX];
};

void *isp_pyr_dec_dev_get(void *isp_handle, void *hw);
void isp_pyr_dec_dev_put(void *ctx);

#ifdef __cplusplus
}
#endif

#endif

