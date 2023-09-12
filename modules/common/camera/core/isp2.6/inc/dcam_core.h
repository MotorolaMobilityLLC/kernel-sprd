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

#ifndef _DCAM_CORE_H_
#define _DCAM_CORE_H_

#include "sprd_img.h"
#include <linux/sprd_ion.h>

#include "cam_types.h"
#include "cam_queue.h"
#include "cam_block.h"
#include "cam_hw.h"
#include "dcam_interface.h"
#include "dcam_fmcu.h"

#define DCAM_IN_Q_LEN                     12
#define DCAM_PROC_Q_LEN                   12
#define DCAM_IRQ_Q_LEN                    256
#define DCAM_FULL_MV_Q_LEN                12
#define DCAM_BIN_MV_Q_LEN                 12
#define DCAM_MIDDLE_Q_LEN                 50
#define DCAM_OFFLINE_PARAM_Q_LEN          20

/* TODO: extend these for slow motion dev */
#define DCAM_RESULT_Q_LEN                 50
#define DCAM_OUT_BUF_Q_LEN                50
#define DCAM_RESERVE_BUF_Q_LEN            50

#define DCAM_LSC_BUF_SIZE                 0x3000
#define DCAM_INT_PROC_FRM_NUM             256

#define DCAM_OFFLINE_TIMEOUT              msecs_to_jiffies(2000)
#define DCAM_OFFLINE_SLC_MAX              3

/* get index of timestamp from frame index */
#define tsid(x)                           ((x) & (DCAM_FRAME_TIMESTAMP_COUNT - 1))
#define DCAM_FETCH_TWICE(p)               (p->raw_fetch_num > 1)
#define DCAM_FIRST_FETCH(p)               (p->raw_fetch_count == 1)
#define DCAM_LAST_FETCH(p)                (p->raw_fetch_count == 2)

#define DCAM_FRAME_INDEX_MAX              0xFFFFFFFF

struct dcam_pipe_dev;

typedef int (*func_dcam_cfg_param)(struct isp_io_param *param,
				struct dcam_dev_param *p);
struct dcam_cfg_entry {
	uint32_t sub_block;
	func_dcam_cfg_param cfg_func;
};

enum dcam_context_id {
	DCAM_CXT_0,
	DCAM_CXT_1,
	DCAM_CXT_2,
	DCAM_CXT_3,
	DCAM_CXT_NUM,
};

enum dcam_scaler_type {
	DCAM_SCALER_BINNING = 0,
	DCAM_SCALER_RAW_DOWNSISER,
	DCAM_SCALER_BY_YUVSCALER,
	DCAM_SCALER_BYPASS,
	DCAM_SCALER_MAX,
};

enum dcam_path_state {
	DCAM_PATH_IDLE,
	DCAM_PATH_PAUSE,
	DCAM_PATH_RESUME,
};

enum dcam_csi_state {
	DCAM_CSI_IDLE,
	DCAM_CSI_PAUSE,
	DCAM_CSI_RESUME,
};

enum dcam_slowmotion_type {
	DCAM_SLW_OFF = 0,
	DCAM_SLW_AP,
	DCAM_SLW_FMCU,
};

struct dcam_rds_slice_ctrl {
	uint32_t rds_input_h_global;
	uint32_t rds_input_w_global;
	uint32_t rds_output_h_global;
	uint32_t rds_output_w_global;
	uint32_t rds_init_phase_int1;
	uint32_t rds_init_phase_int0;
	uint32_t rds_init_phase_rdm1;
	uint32_t rds_init_phase_rdm0;
};

struct dcam_path_desc {
	atomic_t user_cnt;
	enum dcam_path_id path_id;

	spinlock_t size_lock;
	uint32_t size_update;
	void *priv_size_data;

	spinlock_t state_lock;
	enum dcam_path_state state;
	uint32_t state_update;

	struct img_endian endian;
	struct img_size in_size;
	struct img_trim in_trim;
	struct img_trim total_in_trim;
	struct img_size out_size;
	struct sprd_img_rect zoom_ratio_base;

	uint32_t fbc_mode;
	uint32_t base_update;
	uint32_t bayer_pattern;
	uint32_t out_fmt;
	uint32_t data_bits;
	uint32_t pack_bits;
	uint32_t is_4in1;
	uint32_t is_pack;
	uint32_t out_pitch;
	/* pyr output */
	uint32_t pyr_data_bits;
	uint32_t pyr_is_pack;
	/* full path source sel */
	uint32_t src_sel;

	/* for bin path */
	uint32_t is_slw;
	uint32_t slw_frm_num;
	uint32_t bin_ratio;
	uint32_t scaler_sel;/* 0: bining, 1: RDS, 2&3: bypass */
	void *rds_coeff_buf;
	uint32_t rds_coeff_size;
	uint32_t dst_crop_w;

	uint32_t frm_deci;
	uint32_t frm_deci_cnt;

	uint32_t frm_skip;
	uint32_t frm_cnt;

	atomic_t set_frm_cnt;
	atomic_t is_shutoff;
	uint32_t reg_addr;
	struct camera_frame *cur_frame;
	struct camera_queue reserved_buf_queue;
	struct camera_queue out_buf_queue;
	struct camera_queue alter_out_queue;
	struct camera_queue result_queue;
	struct dcam_rds_slice_ctrl gphase;
	struct camera_queue middle_queue;
	struct yuv_scaler_info scaler_info;
	struct dcam_hw_dec_store_cfg dec_store_info;
};

/*
 * state machine for DCAM
 *
 * @INIT:             initial state of this dcam_pipe_dev
 * @IDLE:             after hardware initialized, power/clk/int should be
 *                    prepared, which indicates registers are ready for
 *                    accessing
 * @RUNNING:          this dcam_pipe_dev is receiving data from CSI or DDR, and
 *                    writing data from one or more paths
 * @ERROR:            something is wrong, we should reset hardware right now
 */
enum dcam_state {
	STATE_INIT = 0,
	STATE_IDLE = 1,
	STATE_RUNNING = 2,
	STATE_ERROR = 3,
};

/* for multi dcam context (offline) */
struct dcam_pipe_context {
	atomic_t user_cnt;
	uint32_t ctx_id;
	struct dcam_dev_param blk_pm;
};

struct dcam_offline_slice_info {
	uint32_t cur_slice;
	struct img_trim slice_trim[DCAM_OFFLINE_SLC_MAX];
};
/*
 * A dcam_pipe_dev is a digital IP including one input for raw RGB or YUV
 * data, several outputs which usually be called paths. Input data can be
 * frames received from a CSI controller, or pure image data fetched from DDR.
 * Output paths contains a full path, a binning path and other data paths.
 * There may be raw domain processors in IP, each may or may NOT have data
 * output. Those who outputs data to some address in DDR behaves just same
 * as full or binning path. They can be treated as a special type of path.
 *
 * Each IP should implement a dcam_pipe_dev according to features it has. The
 * IP version may change, but basic function of DCAM IP is similar.
 *
 * @idx:               index of this device
 * @irq:               interrupt number for this device
 * @state:             working state of this device
 *
 * @frame_index:       frame index, tracked by CAP_SOF
 * @index_to_set:      index of next frame, or group of frames, updated in
 *                     CAP_SOF or initial phase
 * @need_fix:          leave fixing work to next CAP_SOF, only in slow motion
 *                     for now
 * @handled_bits:      mask bits that will not be handled this time
 * @iommu_status:      iommu status register
 * @frame_ts:          timestamp at SOF, time without suspend
 * @frame_ts_boot:     timestamp at SOF, ns counts from system boot
 *
 * @slowmotion_count:  frame count in a slow motion group, AKA slow motion rate
 *
 * @helper_enabled:    sync enabled path IDs
 * @helper_lock:       this lock protects synchronizer helper related data
 * @helper_list:       a list of sync helpers
 * @helpers:           memory for helpers
 *
 * @raw_proc_scene:    hwsim flag for offline process
 */
struct dcam_sw_context {
	atomic_t user_cnt;
	atomic_t state;/* TODO: use mutex to protect */
	atomic_t virtualsensor_cap_en;/* for virtual sensor only pre channel ret buf */

	uint32_t sw_ctx_id;
	uint32_t hw_ctx_id;
	struct dcam_hw_context *hw_ctx;
	uint32_t csi_connect_stat;
	struct dcam_fmcu_ctx_desc *fmcu;

	uint32_t do_tasklet;
	uint32_t auto_cpy_id;
	uint32_t base_fid;
	uint32_t frame_index;
	uint32_t index_to_set;
	bool need_fix;
	uint32_t iommu_status;
	timespec frame_ts[DCAM_FRAME_TIMESTAMP_COUNT];
	ktime_t frame_ts_boot[DCAM_FRAME_TIMESTAMP_COUNT];
	uint32_t slowmotion_count;
	enum dcam_slowmotion_type slw_type;
	spinlock_t glb_reg_lock;
	bool dcamsec_eb;
	uint32_t err_status;/* TODO: change to use state */
	uint32_t err_count;/* iommu register dump count in dcam_err */
	uint32_t pack_bits;/* use for offline fetch */
	uint32_t is_4in1;
	uint32_t raw_callback;
	uint32_t lowlux_4in1;/* 4in1 low lux mode capture */
	uint32_t skip_4in1;/* need skip 1 frame then change full source */
	uint32_t is_3dnr;
	uint32_t is_pyr_rec;
	uint32_t is_ebd;
	uint32_t is_raw_alg;
	uint32_t param_frame_sync;
	uint32_t raw_alg_type;
	uint32_t need_dcam_raw;
	uint32_t offline;/* flag: set 1 for 4in1 go through dcam1 bin */
	uint32_t rps;/* raw_proc_scene 0:normal 1:hwsim*/
	uint32_t virtualsensor;
	uint32_t dcam_slice_mode;
	uint32_t slice_num;
	uint32_t slice_count;
	uint32_t dec_all_done;
	uint32_t dec_layer0_done;
	uint32_t prev_fbc_done;
	uint32_t cap_fbc_done;
	spinlock_t fbc_lock;
	struct img_trim slice_trim;/* for sw slices */
	struct img_trim hw_slices[DCAM_OFFLINE_SLC_MAX];/* for offline hw slices */
	struct img_trim *cur_slice;
	uint32_t raw_cap;
	uint32_t raw_fetch_num;
	uint32_t raw_fetch_count;
	struct completion slice_done;
	struct completion frm_done;
	struct completion offline_complete;
	uint32_t zoom_ratio;
	uint32_t total_zoom;
	struct img_trim next_roi;
	uint32_t iommu_enable;
	struct dcam_mipi_info cap_info;
	void *internal_reserved_buf;/* for statis path output */
	struct camera_buf statis_buf_array[STATIS_TYPE_MAX][STATIS_BUF_NUM_MAX];
	void *cb_priv_data;
	uint32_t cur_ctx_id;
	struct dcam_pipe_context ctx[DCAM_CXT_NUM];
	struct dcam_path_desc path[DCAM_PATH_MAX];
	struct dcam_fetch_info fetch;
	struct camera_queue in_queue;
	struct camera_queue proc_queue;
	struct cam_thread_info thread;
	struct dcam_pipe_dev *dev;
	dcam_dev_callback dcam_cb_func;
	struct dcam_offline_slice_info slice_info;
	struct dcam_compress_info fbc_info;
	share_buf_get_cb buf_get_cb;
	void *buf_cb_data;
	atomic_t shadow_done_cnt;
	atomic_t shadow_config_cnt;
	struct nr3_me_data nr3_me;
	struct camera_queue fullpath_mv_queue;
	struct camera_queue binpath_mv_queue;
	struct cam_thread_info dcam_interruption_proc_thrd;
	struct camera_queue interruption_sts_queue;
	struct camera_queue blk_param_queue;
	struct dcam_dev_param *pm;
};

struct dcam_hw_context {
	atomic_t user_cnt;
	uint32_t irq;
	uint32_t sw_ctx_id;
	uint32_t hw_ctx_id;
	uint32_t frame_index;
	uint32_t handled_bits;
	uint32_t handled_bits_on_int1;
	struct dcam_sw_context *sw_ctx;
	struct dcam_fmcu_ctx_desc *fmcu;
	struct cam_hw_info *hw;
};

struct dcam_pipe_dev {
	atomic_t user_cnt;
	atomic_t enable;
	struct dcam_sw_context sw_ctx[DCAM_SW_CONTEXT_MAX];
	struct dcam_hw_context hw_ctx[DCAM_HW_CONTEXT_MAX];
	struct mutex ctx_mutex;
	struct mutex path_mutex;
	spinlock_t ctx_lock;
	struct dcam_pipe_ops *dcam_pipe_ops;
	struct cam_hw_info *hw;
};

enum dcam_bind_mode {
	DCAM_BIND_FIXED = 0,
	DCAM_BIND_DYNAMIC,
	DCAM_BIND_MAX
};

struct dcam_switch_param {
	uint32_t csi_id;
	uint32_t dcam_id;
	uint32_t is_recovery;
};

static inline uint32_t cal_sprd_yuv_pitch(uint32_t w, uint32_t dcam_out_bits, uint32_t is_pack)
{
	if (dcam_out_bits != DCAM_STORE_8_BIT) {
		if(is_pack)
			w = (w * 10 + 127) / 128 * 128 / 8;
		else
			w = (w * 16 + 127) / 128 * 128 / 8;
	}

	return w;
}

int dcam_core_context_bind(struct dcam_sw_context *pctx, enum dcam_bind_mode mode, uint32_t dcam_idx);
int dcam_core_context_unbind(struct dcam_sw_context *pctx);
int dcam_core_hw_context_id_get(struct dcam_sw_context *pctx);
void dcam_core_get_fmcu(struct dcam_hw_context *pctx_hw);
void dcam_core_put_fmcu(struct dcam_hw_context *pctx_hw);
int dcam_core_ctx_switch(struct dcam_sw_context *ori_sw_ctx, struct dcam_sw_context *new_sw_ctx,
						struct dcam_hw_context *hw_ctx);
int dcam_core_hw_slices_set(struct dcam_sw_context *pctx, struct camera_frame *pframe, uint32_t slice_wmax);
void dcam_core_offline_debug_dump(struct dcam_sw_context *pctx, struct dcam_dev_param *pm, struct camera_frame *proc_frame);
int dcam_core_slice_trim_get(uint32_t width, uint32_t heigth, uint32_t slice_num, uint32_t slice_no, struct img_trim *slice_trim);
int dcam_core_offline_slices_sw_start(void *param);
void dcamcore_empty_interrupt_put(void *param);
int dcamcore_thread_create(void *ctx_handle, struct cam_thread_info *thrd, proc_func func);

#endif
