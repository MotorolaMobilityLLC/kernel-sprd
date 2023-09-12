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

#ifndef _ISP_CORE_H_
#define _ISP_CORE_H_

#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sprd_ion.h>
#include "sprd_img.h"

#include "cam_types.h"
#include "cam_queue.h"
#include "cam_block.h"
#include "isp_interface.h"
#include "isp_3dnr.h"
#include "isp_ltm.h"
#include "isp_gtm.h"

#define ISP_LINE_BUFFER_W          ISP_MAX_LINE_WIDTH
#define ISP_IN_Q_LEN               8
#define ISP_PROC_Q_LEN             2
#define ISP_RESULT_Q_LEN           2
#define ISP_SLW_IN_Q_LEN           50
#define ISP_SLW_PROC_Q_LEN         50
#define ISP_SLW_RESULT_Q_LEN       50
#define ISP_OUT_BUF_Q_LEN          96
#define ISP_RESERVE_BUF_Q_LEN      48
#define ISP_STREAM_STATE_Q_LEN     12
#define ISP_SW_CONTEXT_Q_LEN       ISP_SW_CONTEXT_NUM

#define ODATA_YUV420               1
#define ODATA_YUV422               0

#define ISP_PIXEL_ALIGN_WIDTH      4
#define ISP_PIXEL_ALIGN_HEIGHT     2

#define AFBC_PADDING_W_YUV420      32
#define AFBC_PADDING_H_YUV420      8
#define AFBC_HEADER_SIZE           16
#define AFBC_PAYLOAD_SIZE          84
#define ISP_FBD_BASE_ALIGN         256

#define ISP_ALIGN_W(_a)            ((_a) & ~(ISP_PIXEL_ALIGN_WIDTH - 1))
#define ISP_ALIGN_H(_a)            ((_a) & ~(ISP_PIXEL_ALIGN_HEIGHT - 1))
#define ISP_DIV_ALIGN_W(_a, _b)    (((_a) / (_b)) & ~(ISP_PIXEL_ALIGN_WIDTH - 1))
#define ISP_DIV_ALIGN_H(_a, _b)    (((_a) / (_b)) & ~(ISP_PIXEL_ALIGN_HEIGHT - 1))

enum isp_afbd_data_bits {
	AFBD_FETCH_8BITS = 5,
	AFBD_FETCH_10BITS = 7,
};

enum isp_raw_format {
	ISP_RAW_PACK10 = 0x00,
	ISP_RAW_HALF10 = 0x01,
	ISP_RAW_HALF14 = 0x02,
	ISP_RAW_FORMAT_MAX
};

enum isp_postproc_type {
	POSTPROC_FRAME_DONE,
	POSTPORC_HIST_DONE,
	POSTPROC_MAX,
};

enum isp_frame_bit_width {
	ISP_FRAME_8_BIT = 8,
	ISP_FRAME_10_BIT = 10,
	ISP_FRAME_12_BIT = 12,
	ISP_FRAME_14_BIT = 14,
};

/*
 * Before N6pro all use ISP_THUMB_SCL_VER_0
 * N6pro use ISP_THUMB_SCL_VER_1.
*/
enum isp_thumb_scaler_version {
	ISP_THUMB_SCL_VER_0,
	ISP_THUMB_SCL_VER_1,
	ISP_THUMB_SCL__MAX,
};

typedef int (*func_isp_cfg_param)(
	struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx);

typedef int (*func_isp_cfg_block_param)(
	struct isp_k_block *param_block,
	struct isp_k_block *isp_k_param);

typedef int(*isp_irq_postproc)(void *handle, uint32_t idx,
	enum isp_postproc_type type);

struct isp_cfg_entry {
	uint32_t sub_block;
	func_isp_cfg_param cfg_func;
};

struct isp_cfg_pre_param {
	uint32_t sub_block;
	func_isp_cfg_block_param cfg_block_func;
};

struct slice_dyn_calc_param{
	uint32_t verison;
	uint32_t path_en[ISP_SPATH_NUM];
	uint32_t pyr_layer_num;
	uint32_t need_dewarping;
	struct img_size src;
	struct img_trim crop;
	struct isp_hw_path_scaler *path_scaler[ISP_SPATH_NUM];
	struct isp_hw_thumbscaler_info *thumb_scaler;
	struct isp_store_info *store[ISP_SPATH_NUM];
};

struct slice_cfg_input {
	uint32_t ltm_rgb_eb;
	uint32_t ltm_yuv_eb;
	uint32_t gtm_rgb_eb;
	uint32_t pyr_rec_eb;
	uint32_t nlm_center_x;
	uint32_t nlm_center_y;
	uint32_t ynr_center_x;
	uint32_t ynr_center_y;
	struct img_size frame_in_size;
	struct img_size *frame_out_size[ISP_SPATH_NUM];
	struct isp_hw_fetch_info *frame_fetch;
	struct isp_fbd_raw_info *frame_fbd_raw;
	struct isp_fbd_yuv_info *frame_fbd_yuv;
	struct isp_hw_thumbscaler_info *thumb_scaler;
	struct isp_store_info *frame_store[ISP_SPATH_NUM];
	struct isp_afbc_store_info *frame_afbc_store[AFBC_PATH_NUM];
	struct yuv_scaler_info *frame_scaler[ISP_SPATH_NUM];
	struct img_deci_info *frame_deci[ISP_SPATH_NUM];
	struct img_trim *frame_trim0[ISP_SPATH_NUM];
	struct img_trim *frame_trim1[ISP_SPATH_NUM];
	struct isp_3dnr_ctx_desc *nr3_ctx;
	struct isp_ltm_ctx_desc *rgb_ltm;
	struct isp_ltm_ctx_desc *yuv_ltm;
	struct isp_gtm_ctx_desc *rgb_gtm;
	struct isp_k_block *nofilter_ctx;
	struct slice_dyn_calc_param calc_dyn_ov;
};

struct isp_path_desc {
	atomic_t user_cnt;
	enum isp_sub_path_id spath_id;
	int32_t reserved_buf_fd;
	size_t reserve_buf_size[3];
	struct isp_sw_context *attach_ctx;
	struct cam_hw_info *hw;

	int q_init;
	struct camera_queue reserved_buf_queue;
	struct camera_queue out_buf_queue;
	struct camera_queue result_queue;
};

struct isp_pipe_info {
	struct isp_hw_fetch_info fetch;
	struct isp_fbd_raw_info fetch_fbd;
	struct isp_fbd_yuv_info fetch_fbd_yuv;
	struct isp_hw_path_scaler scaler[ISP_SPATH_NUM];
	struct isp_hw_thumbscaler_info thumb_scaler;
	struct isp_hw_path_store store[ISP_SPATH_NUM];
	struct isp_hw_afbc_path afbc[AFBC_PATH_NUM];
};

struct isp_path_uinfo {
	/* 1 for fbc store; 0 for normal store */
	uint32_t store_fbc;
	uint32_t out_fmt;
	uint32_t bind_type;
	uint32_t slave_path_id;
	uint32_t data_in_bits;
	uint32_t regular_mode;
	uint32_t uframe_sync;
	uint32_t scaler_coeff_ex;
	uint32_t scaler_bypass_ctrl;
	struct img_endian data_endian;
	struct img_size dst;
	struct img_trim in_trim;
};

struct isp_uinfo {
	/* common info from cam core */
	uint32_t in_fmt;
	uint32_t pack_bits;
	uint32_t is_pack;
	uint32_t data_in_bits;
	/* pyr output*/
	uint32_t pyr_data_in_bits;
	uint32_t pyr_is_pack;
	/* GR, R, B, Gb */
	uint32_t bayer_pattern;
	uint32_t ltm_rgb;
	uint32_t ltm_yuv;
	uint32_t mode_ltm;
	uint32_t gtm_rgb;
	uint32_t mode_gtm;
	uint32_t mode_3dnr;
	uint32_t slw_state;
	uint32_t enable_slowmotion;
	uint32_t slowmotion_240fp_count;
	uint32_t slowmotion_count;
	uint32_t stage_a_frame_num;
	uint32_t stage_a_valid_count;
	uint32_t stage_b_frame_num;
	uint32_t stage_c_frame_num;
	uint32_t uframe_sync;
	uint32_t scaler_coeff_ex;
	uint32_t pyr_layer_num;
	uint32_t is_dewarping;

	/* compression info from cam core */
	/*2:dewarp  1: fetch_fbd; 0: fetch */
	uint32_t fetch_path_sel;
	/* 0: 14bit; 1: 10bit */
	uint32_t fetch_fbd_4bit_bypass;
	/* 1: 3dnr compressed; 0: 3dnr plain data */
	uint32_t nr3_fbc_fbd;

	/* input info from cam core */
	struct sprd_img_size sn_size;/* sensor size */
	struct img_size src;
	struct img_size ori_src;
	struct img_trim crop;
	struct img_scaler_info original;

	/* output info from cam core */
	struct isp_path_uinfo path_info[ISP_SPATH_NUM];
};

struct isp_sw_context {
	struct list_head list;
	atomic_t user_cnt;
	atomic_t state_user_cnt;
	atomic_t post_cap_cnt;
	atomic_t cap_cnt;
	uint32_t is_post_multi;
	uint32_t started;
	uint32_t ctx_id;
	uint32_t in_irq_handler;
	uint32_t iommu_status;
	uint32_t slw_frm_cnt;
	enum isp_postproc_type post_type;
	enum camera_id attach_cam_id;
	enum cam_ch_id ch_id;

	struct isp_uinfo uinfo;
	struct isp_uinfo pipe_src;
	struct isp_pipe_info pipe_info;

	struct isp_path_desc isp_path[ISP_SPATH_NUM];
	struct isp_pipe_dev *dev;
	struct cam_hw_info *hw;
	void *slice_ctx;
	void *nr3_handle;
	void *rgb_ltm_handle;
	void *yuv_ltm_handle;
	void *rec_handle;
	void *rgb_gtm_handle;
	void *dewarp_handle;
	struct isp_k_block isp_k_param;
	struct camera_frame *isp_receive_param;
	struct isp_k_block *isp_using_param;

	struct cam_thread_info thread;
	struct cam_thread_info postproc_thread;
	struct completion frm_done;
	struct completion slice_done;
	/* lock ctx/path param(size) updated from zoom */
	struct mutex param_mutex;
	/* lock block param to avoid acrossing frame */
	struct mutex blkpm_lock;
	/*stopping thread, stop wait for out frame*/
	uint32_t thread_doing_stop;

	struct camera_queue param_share_queue;
	struct camera_queue param_buf_queue;
	struct mutex blkpm_q_lock;

	struct camera_queue in_queue;
	struct camera_queue proc_queue;
	struct camera_queue stream_ctrl_in_q;
	struct camera_queue stream_ctrl_proc_q;
	struct camera_queue pyrdec_buf_queue;
	struct camera_queue post_proc_queue;

	struct camera_frame *postproc_buf;
	struct camera_buf statis_buf_array[STATIS_TYPE_MAX][STATIS_BUF_NUM_MAX];
	struct camera_queue hist2_result_queue;
	struct camera_queue gtmhist_result_queue;

	uint32_t multi_slice;
	uint32_t is_last_slice;
	uint32_t valid_slc_num;
	uint32_t sw_slice_num;
	uint32_t sw_slice_no;
	uint32_t rps;

	uint32_t zoom_conflict_with_ltm;

	isp_irq_postproc postproc_func;
	isp_dev_callback isp_cb_func;
	void *cb_priv_data;
};

struct isp_hw_context {
	atomic_t user_cnt;
	uint32_t sw_ctx_id;
	uint32_t hw_ctx_id;
	uint32_t fmcu_used;
	void *fmcu_handle;
	struct isp_sw_context *pctx;
};

struct isp_pipe_dev {
	uint32_t irq_no[2];
	atomic_t user_cnt;
	atomic_t pd_clk_rdy;
	atomic_t enable;
	struct mutex path_mutex;
	spinlock_t ctx_lock;
	enum isp_work_mode wmode;
	enum sprd_cam_sec_mode sec_mode;
	void *cfg_handle;
	void *pyr_dec_handle;
	struct camera_queue sw_ctx_q;
	struct isp_sw_context *sw_ctx[ISP_SW_CONTEXT_NUM];
	struct isp_hw_context hw_ctx[ISP_CONTEXT_HW_NUM];
	struct cam_hw_info *isp_hw;
	struct isp_pipe_ops *isp_ops;
};

struct isp_statis_buf_size_info {
	enum isp_statis_buf_type buf_type;
	size_t buf_size;
	size_t buf_cnt;
};

void *isp_core_pipe_dev_get(void);
int isp_core_pipe_dev_put(void *isp_handle);

int isp_core_hw_context_id_get(struct isp_sw_context *pctx);
int isp_core_sw_context_id_get(enum isp_context_hw_id hw_ctx_id, struct isp_pipe_dev *dev);
int isp_core_context_bind(struct isp_sw_context *pctx, int fmcu_need);
int isp_core_context_unbind(struct isp_sw_context *pctx);

#endif
