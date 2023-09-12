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

#ifndef _ISP_PYR_REC_H_
#define _ISP_PYR_REC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cam_queue.h"
#include "isp_slice.h"

#ifdef ISP_PYR_DEBUG_ON
#define ISP_PYR_DEBUG pr_info
#else
#define ISP_PYR_DEBUG pr_debug
#endif

#define PYR_REC_ADDR_NUM        ISP_PYR_DEC_LAYER_NUM + 1
#define ISP_FLASH_LIMIT_WIDTH   768

enum {
	ISP_PYR_REC_CUR,
	ISP_PYR_REC_REF,
	ISP_PYR_REC_MAX,
};

struct isp_rec_fetch_info {
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

struct rec_ynr_layer_cfg {
	uint32_t gf_enable;
	uint32_t gf_radius;
	uint32_t gf_rnr_offset;
	uint32_t lum_thresh0;
	uint32_t lum_thresh1;
	uint32_t gf_epsilon_low;
	uint32_t gf_epsilon_mid;
	uint32_t gf_epsilon_high;
	uint32_t gf_rnr_ratio;
	uint32_t gf_addback_ratio;
	uint32_t gf_addback_clip;
	uint32_t gf_addback_en;
	uint32_t max_dist;
	uint32_t ynr_radius;
	struct img_size imgcenter;
};

struct isp_rec_ynr_info {
	uint32_t rec_ynr_bypass;
	uint32_t layer_num;
	struct img_size img;
	struct img_size start;
	struct rec_ynr_layer_cfg ynr_cfg_layer[5];
	struct isp_dev_ynr_info_v3 *pyr_ynr;
};

struct isp_rec_cnr_info {
	uint32_t rec_cnr_bypass;
	uint32_t layer_num;
	struct img_size img_center;
	struct isp_dev_cnr_h_info *pyr_cnr;
};

struct isp_rec_store_info {
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

struct isp_pyr_rec_info {
	uint32_t layer_num;
	uint32_t drop_en;
	uint32_t reconstruct_bypass;
	uint32_t out_height;
	uint32_t out_width;
	uint32_t fifo1_nfull_num;
	uint32_t fifo0_nfull_num;
	uint32_t pre_layer_width;
	uint32_t pre_layer_height;
	uint32_t fifo3_nfull_num;
	uint32_t fifo2_nfull_num;
	uint32_t fifo5_nfull_num;
	uint32_t fifo4_nfull_num;
	uint32_t hblank_num;
	uint32_t ver_padding_num;
	uint32_t ver_padding_en;
	uint32_t hor_padding_num;
	uint32_t hor_padding_en;
	uint32_t cur_layer_width;
	uint32_t cur_layer_height;
	uint32_t reduce_flt_vblank;
	uint32_t reduce_flt_hblank;
	uint32_t rec_path_sel;
};

enum isp_rec_cfg_cmd {
	ISP_REC_CFG_BUF,
	ISP_REC_CFG_LAYER_NUM,
	ISP_REC_CFG_WORK_MODE,
	ISP_REC_CFG_HW_CTX_IDX,
	ISP_REC_CFG_FMCU_HANDLE,
	ISP_REC_CFG_DEWARPING_EB,
	ISP_REC_CFG_MAX,
};

struct isp_rec_ops {
	int (*cfg_param)(void *rec_handle, enum isp_rec_cfg_cmd cmd, void *param);
	int (*pipe_proc)(void *rec_handle, void *param);
};

struct isp_pyr_rec_in {
	uint32_t in_fmt;
	uint32_t pyr_fmt;
	uint32_t pyr_ynr_radius;
	uint32_t pyr_cnr_radius;
	struct img_size src;
	struct img_trim in_trim;
	struct img_addr in_addr;
	struct img_addr out_addr;
	uint32_t slice_num[ISP_PYR_DEC_LAYER_NUM];
	struct isp_dev_ynr_info_v3 *pyr_ynr;
	struct isp_dev_cnr_h_info *pyr_cnr;
	struct alg_slice_drv_overlap *slice_overlap;
};

struct isp_rec_slice_desc {
	struct slice_pos_info slice_fetch0_pos;
	struct slice_pos_info slice_fetch1_pos;
	struct slice_pos_info slice_store_pos;
	struct slice_overlap_info slice_overlap;
	struct slice_fetch_info slice_ref_fetch;
	struct slice_fetch_info slice_cur_fetch;
	struct slice_pyr_rec_info slice_pyr_rec;
	struct slice_store_info slice_rec_store;
};

struct isp_rec_ctx_desc {
	uint32_t ctx_id;
	uint32_t in_fmt;
	uint32_t out_fmt;
	uint32_t pyr_fmt;
	uint32_t cur_layer;
	uint32_t slice_num;
	uint32_t hw_ctx_id;
	uint32_t layer_num;
	uint32_t dewarp_eb;
	uint32_t cur_slice_id;
	uint32_t rec_ynr_radius;
	uint32_t rec_cnr_radius;
	uint32_t fbcd_buffer_size;
	enum isp_fetch_path_select fetch_path_sel;
	enum isp_work_mode wmode;
	struct img_addr fetch_addr[PYR_REC_ADDR_NUM];
	struct img_addr store_addr[ISP_PYR_DEC_LAYER_NUM];
	struct img_size pyr_layer_size[PYR_REC_ADDR_NUM];
	struct img_size pyr_padding_size;
	struct camera_frame *buf_info;
	void *fmcu_handle;

	/* cur frame fetch: big size use rec fetch */
	struct isp_rec_fetch_info cur_fetch;
	/* ref frame fetch: little size use normal fetch */
	struct isp_rec_fetch_info ref_fetch;
	struct isp_fbd_yuv_info fetch_fbd;
	struct isp_rec_ynr_info rec_ynr;
	struct isp_rec_cnr_info rec_cnr;
	struct isp_pyr_rec_info pyr_rec;
	struct isp_rec_store_info rec_store;

	struct isp_rec_slice_desc slices[SLICE_NUM_MAX];

	struct isp_rec_ops ops;
	struct cam_hw_info *hw;
};

void *isp_pyr_rec_ctx_get(uint32_t idx, void *hw);
void isp_pyr_rec_ctx_put(void *ctx);

#ifdef __cplusplus
}
#endif

#endif

