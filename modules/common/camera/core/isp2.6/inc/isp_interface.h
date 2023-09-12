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

#ifndef _ISP_INTERFACE_H_
#define _ISP_INTERFACE_H_

#include <linux/of.h>
#include <linux/platform_device.h>

#include "cam_hw.h"
#include "cam_types.h"

#define ISP_SW_CONTEXT_NUM              32
#define ISP_MAX_LINE_WIDTH              2592
#define ISP_NR3_BUF_NUM                 2
#define ISP_LTM_BUF_NUM                 10
#define ISP_PYR_REC_BUF_NUM             2
#define CAMERA_RESERVE_FRAME_NUM        0xffffffff
#define ISP_FBC_3DNR_PAD_WIDTH          256
#define ISP_FBC_3DNR_PAD_HEIGHT         4
#define ISP_FBC_STORE_TILE_WIDTH        32
#define ISP_FBC_STORE_TILE_HEIGHT       8
#define ISP_PYR_DEC_LAYER_NUM           5
#define MAX_PYR_DEC_LAYER_NUM           (ISP_PYR_DEC_LAYER_NUM + 1)
#define ISP_PYRDEC_BUF_Q_LEN            8
#define ISP_GTMHIST_BUF_Q_LEN           16
#define ISP_HIST2_BUF_Q_LEN             16
#define ISP_CONTEXT_TIMEOUT             msecs_to_jiffies(2000)

enum isp_context_id {
	ISP_CONTEXT_P0,
	ISP_CONTEXT_C0,
	ISP_CONTEXT_P1,
	ISP_CONTEXT_C1,
	ISP_CONTEXT_P2,
	ISP_CONTEXT_C2,
	ISP_CONTEXT_P3,
	ISP_CONTEXT_C3,
	ISP_CONTEXT_SUPERZOOM,
	ISP_CONTEXT_SW_NUM
};

enum isp_context_hw_id {
	ISP_CONTEXT_HW_P0,
	ISP_CONTEXT_HW_C0,
	ISP_CONTEXT_HW_P1,
	ISP_CONTEXT_HW_C1,
	ISP_CONTEXT_HW_NUM
};

enum isp_path_cfg_cmd {
	ISP_PATH_CFG_CTX_BASE,
	ISP_PATH_CFG_CTX_SIZE,
	ISP_PATH_CFG_CTX_COMPRESSION,
	ISP_PATH_CFG_CTX_UFRAME_SYNC,
	ISP_PATH_CFG_PATH_BASE,
	ISP_PATH_CFG_PATH_SIZE,
	ISP_PATH_CFG_PATH_COMPRESSION,
	ISP_PATH_CFG_PATH_UFRAME_SYNC,
	ISP_PATH_CFG_OUTPUT_BUF,
	ISP_PATH_RETURN_OUTPUT_BUF,
	ISP_PATH_CFG_OUTPUT_RESERVED_BUF,
	ISP_PATH_CFG_3DNR_BUF,
	ISP_PATH_CFG_RGB_LTM_BUF,
	ISP_PATH_CFG_YUV_LTM_BUF,
	ISP_PATH_CFG_POSTPROC_BUF,
	ISP_PATH_CFG_3DNR_MODE,
	ISP_PATH_CFG_PYR_REC_BUF,
	ISP_PATH_CFG_PYR_DEC_BUF,
	ISP_PATH_CFG_PATH_SLW,
	ISP_PATH_CFG_MAX,
};

enum isp_work_mode {
	ISP_CFG_MODE,
	ISP_AP_MODE,
	ISP_WM_MAX
};

enum isp_3dnr_mode {
	MODE_3DNR_OFF,
	MODE_3DNR_PRE,
	MODE_3DNR_CAP,
	MODE_3DNR_MAX,
};

enum isp_ltm_mode {
	MODE_LTM_OFF,
	MODE_LTM_PRE,
	MODE_LTM_CAP,
	MODE_LTM_MAX
};

enum isp_ltm_region {
	LTM_RGB,
	LTM_YUV,
	LTM_MAX
};

enum isp_gtm_mode {
	MODE_GTM_OFF,
	MODE_GTM_PRE,
	MODE_GTM_CAP,
	MODE_GTM_MAX
};

enum isp_path_binding_type {
	ISP_PATH_ALONE = 0,
	ISP_PATH_MASTER,
	ISP_PATH_SLAVE,
};

enum isp_offline_param_valid {
	ISP_SRC_SIZE = (1 << 0),
	ISP_PATH0_TRIM = (1 << 1),
	ISP_PATH1_TRIM = (1 << 2),
	ISP_PATH2_TRIM = (1 << 3),
};

enum isp_ioctrl_cmd {
	ISP_IOCTL_CFG_STATIS_BUF,
	ISP_IOCTL_CFG_SEC,
	ISP_IOCTL_CFG_PYR_REC_NUM,
	ISP_IOCTL_CFG_THREAD_PROC_STOP,
	ISP_IOCTL_CFG_POST_CNT,
	ISP_IOCTL_CFG_POST_MULTI_SCENE,
	ISP_IOCTL_CFG_CTX_CAP_CNT,
	ISP_IOCTL_CFG_MAX,
};

enum isp_stream_state {
	ISP_STREAM_NORMAL_PROC,
	ISP_STREAM_POST_PROC,
	ISP_STREAM_MAX,
};

enum isp_stream_buf_type {
	ISP_STREAM_BUF_OUT,
	ISP_STREAM_BUF_RESERVED,
	ISP_STREAM_BUF_POSTPROC,
	ISP_STREAM_BUF_RESULT,
	ISP_STREAM_BUF_MAX,
};

enum isp_stream_data_src {
	ISP_STREAM_SRC_DCAM,
	ISP_STREAM_SRC_ISP,
	ISP_STREAM_SRC_MAX,
};

enum isp_stream_frame_type {
	ISP_STREAM_SIGNLE,
	ISP_STREAM_MULTI,
	ISP_STRESM_MAX,
};

struct isp_init_param {
	uint32_t is_high_fps;
	uint32_t cam_id;
	uint32_t blkparam_node_num;
};

struct isp_ctx_base_desc {
	uint32_t mode_3dnr;
	uint32_t mode_ltm;
	uint32_t ltm_rgb;
	uint32_t ltm_yuv;
	uint32_t mode_gtm;
	uint32_t gtm_rgb;
	uint32_t in_fmt;
	uint32_t is_pack;
	uint32_t data_in_bits;
	uint32_t pack_bits;
	uint32_t bayer_pattern;
	uint32_t enable_slowmotion;
	uint32_t slowmotion_count;
	uint32_t slw_state;
	uint32_t slowmotion_stage_a_num;
	uint32_t slowmotion_stage_a_valid_num;
	uint32_t slowmotion_stage_b_num;
	uint32_t pyr_data_bits;
	uint32_t pyr_is_pack;
	enum cam_ch_id ch_id;
	struct sprd_img_size sn_size;
};

struct isp_ctx_size_desc {
	struct img_size src;
	struct img_trim crop;
	uint32_t zoom_conflict_with_ltm;
};

struct isp_ctx_compress_desc {
	uint32_t fetch_fbd;
	uint32_t fetch_fbd_4bit_bypass;
	uint32_t nr3_fbc_fbd;
};

struct isp_path_base_desc {
	uint32_t out_fmt;
	uint32_t slave_type;
	uint32_t slave_path_id;
	uint32_t regular_mode;
	uint32_t data_bits;
	struct img_endian endian;
	struct img_size output_size;
};

struct isp_path_compression_desc {
	uint32_t store_fbc;
};

static inline void isp_3dnr_cal_compressed_addr(uint32_t width,
			uint32_t height, unsigned long in,
			struct compressed_addr *out)
{
	uint32_t pixel_count, header_bytes_y, header_bytes_c;

	if (unlikely(!out))
		return;

	pixel_count = roundup(width, ISP_FBC_3DNR_PAD_WIDTH) *
		roundup(height, ISP_FBC_3DNR_PAD_HEIGHT);
	/* add some redundant space for fbc output */
	header_bytes_y = pixel_count >> 9;
	header_bytes_y += FBC_HEADER_REDUNDANT;
	header_bytes_c = pixel_count >> 10;
	header_bytes_c += FBC_HEADER_REDUNDANT;

	out->addr0 = (uint32_t)in;
	out->addr1 = ALIGN(out->addr0 + header_bytes_y, FBC_TILE_ADDR_ALIGN);
	out->addr2 = ALIGN(out->addr1 + header_bytes_c + pixel_count,
			FBC_TILE_ADDR_ALIGN);
}

static inline uint32_t
isp_3dnr_cal_compressed_size(uint32_t width, uint32_t height)
{
	uint32_t pixel_count, header_y, tile_y, header_c, tile_c;

	pixel_count = roundup(width, ISP_FBC_3DNR_PAD_WIDTH) *
		roundup(height, ISP_FBC_3DNR_PAD_HEIGHT);
	header_y = pixel_count >> 9;
	header_y += FBC_HEADER_REDUNDANT + FBC_TILE_ADDR_ALIGN;
	tile_y = pixel_count;
	header_c = pixel_count >> 10;
	header_c += FBC_HEADER_REDUNDANT + FBC_TILE_ADDR_ALIGN;
	tile_c = pixel_count >> 1;

	return header_y + tile_y + header_c + tile_c;
}

static inline uint32_t isp_rec_layer0_width(uint32_t w, uint32_t layer_num)
{
	uint32_t width = 0, i = 0;
	uint32_t w_align = PYR_DEC_WIDTH_ALIGN;

	for (i = 0; i < layer_num; i++)
		w_align *= 2;

	width = ALIGN(w, w_align);
	return width;
}

static inline uint32_t isp_rec_layer0_heigh(uint32_t h, uint32_t layer_num)
{
	uint32_t height = 0, i = 0;
	uint32_t h_align = PYR_DEC_HEIGHT_ALIGN;

	for (i = 0; i < layer_num; i++)
		h_align *= 2;

	height = ALIGN(h, h_align);
	return height;
}

static inline uint32_t isp_rec_small_layer_w(uint32_t w, uint32_t layer_num)
{
	uint32_t width = 0, i = 0;
	uint32_t w_align = PYR_DEC_WIDTH_ALIGN;

	for (i = 0; i < layer_num; i++)
		w_align *= 2;

	width = ALIGN(w, w_align);
	for (i = 0; i < layer_num; i++)
		width = width / 2;

	return width;
}

static inline uint32_t isp_rec_small_layer_h(uint32_t h, uint32_t layer_num)
{
	uint32_t height = 0, i = 0;
	uint32_t h_align = PYR_DEC_HEIGHT_ALIGN;

	for (i = 0; i < layer_num; i++)
		h_align *= 2;

	height = ALIGN(h, h_align);
	for (i = 0; i < layer_num; i++)
		height = height / 2;

	return height;
}

struct isp_offline_param {
	uint32_t valid;
	struct img_scaler_info src_info;
	struct img_trim trim_path[ISP_SPATH_NUM];
	void *prev;
};

enum isp_start_ctrl {
	ISP_START_CTRL_EN,
	ISP_START_CTRL_DIS,
	ISP_START_CTRL_MAX,
};

enum isp_callback_ctrl {
	ISP_CALLBACK_CTRL_USER,
	ISP_CALLBACK_CTRL_MAX,
};

struct isp_data_ctrl_cfg {
	enum isp_start_ctrl start_ctrl;
	enum isp_callback_ctrl callback_ctrl;
	uint32_t in_format;
	uint32_t out_format;
	struct img_size src;
	struct img_trim crop;
	struct img_size dst;
};

struct isp_secen_ctrl_info {
	struct isp_data_ctrl_cfg fdrl_ctrl;
	struct isp_data_ctrl_cfg fdrh_ctrl;
};

struct isp_pipe_ops {
	int (*open)(void *isp_handle, void *arg);
	int (*close)(void *isp_handle);
	int (*reset)(void *isp_handle, void *arg);

	int (*get_context)(void *isp_handle, void *param);
	int (*put_context)(void *isp_handle, int ctx_id);

	int (*get_path)(void *isp_handle, int ctx_id, int path_id);
	int (*get_param)(void *handle, void *arg);
	int (*put_path)(void *isp_handle, int ctx_id, int path_id);
	int (*cfg_path)(void *isp_handle, enum isp_path_cfg_cmd cfg_cmd,
			int ctx_id, int path_id, void *param);
	int (*ioctl)(void *isp_handle, int ctx_id,
			enum isp_ioctrl_cmd cmd, void *param);
	int (*cfg_blk_param)(void *isp_handle, int ctx_id, void *param);
	int (*proc_frame)(void *isp_handle, void *param, int ctx_id);
	int (*set_callback)(void *isp_handle, int ctx_id,
			isp_dev_callback cb, void *priv_data);
	int (*update_clk)(void *isp_handle, void *arg);
	int (*clear_stream_ctrl)(void *isp_handle, int ctx_id);
	int (*set_datactrl)(void *handle, void *in, void *out);
	int (*clear_blk_param_q)(void *isp_handle, int ctx_id);
};

#endif
