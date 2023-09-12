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

#ifndef _ISP_GTM_H_
#define _ISP_GTM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cam_queue.h"
#include "dcam_interface.h"
#include "isp_interface.h"

#define GTM_ID_MAX               3
#define GTM_HIST_BIN_NUM         128
#define ISP_GM_TIMEOUT           msecs_to_jiffies(100)

enum isp_gtm_param_type {
	ISP_GTM_PARAM_PRE,
	ISP_GTM_PARAM_CAP,
	ISP_GTM_PARAM_MAX,
};

enum isp_gtm_cfg_cmd {
	ISP_GTM_CFG_EB,
	ISP_GTM_CFG_MODE,
	ISP_GTM_CFG_FRAME_ID,
	ISP_GTM_CFG_HIST_BYPASS,
	ISP_GTM_CFG_MAP_BYPASS,
	ISP_GTM_CFG_MOD_EN,
	ISP_GTM_CFG_CALC_MODE,
	ISP_GTM_CFG_MAX,
};

struct isp_gtm_ops {
	int (*cfg_param)(void *handle, enum isp_gtm_cfg_cmd cmd, void *param);
	int (*pipe_proc)(void *handle, void *param, void *param2);
	int (*get_preview_hist_cal)(void *handle);
	int (*sync_completion_get)(void *handle);
	int (*sync_completion_done)(void *handle);
};

struct isp_gtm_mapping {
	uint32_t ctx_id;
	uint32_t fid;
	uint32_t sw_mode;
	uint32_t gtm_hw_ymax;
	uint32_t gtm_hw_ymin;
	uint32_t gtm_hw_target_norm;
	uint32_t gtm_hw_yavg;
	uint32_t gtm_hw_lr_int;
	uint32_t gtm_img_key_value;
	uint32_t gtm_hw_log_diff_int;
	uint32_t gtm_hw_log_min_int;
	uint32_t gtm_hw_log_diff;
	uint32_t gtm_fsm_state;
	uint32_t gtm_blk_row_ocnt;
	uint32_t gtm_blk_col_ocnt;
};

struct isp_gtm_bypass_param {
	uint32_t ctx_id;
	uint32_t mod_en;
	uint32_t hist_bypass;
	uint32_t map_bypass;
};

struct isp_gtm_sync {
	atomic_t user_cnt;
	atomic_t prev_fid;
	atomic_t wait_completion_fid;
	struct completion share_comp;
	struct dcam_dev_raw_gtm_block_info tuning;
	struct isp_gtm_mapping mapping;
};

struct isp_gtm_k_block{
	struct isp_gtm_ctx_desc *ctx;
	struct dcam_dev_raw_gtm_block_info *tuning;
	struct cam_gtm_mapping *map;
};

struct isp_gtm_ctx_desc {
	uint32_t enable;
	uint32_t bypass;
	uint32_t mode;
	uint32_t ctx_id;
	uint32_t cam_id;
	uint32_t fid;
	atomic_t cnt;
	uint32_t calc_mode;
	uint32_t gtm_mode_en;
	uint32_t gtm_map_bypass;
	uint32_t gtm_hist_stat_bypass;
	uint32_t gtm_cur_is_first_frame;
	uint32_t gtm_tm_luma_est_mode;
	struct img_size src;
	struct cam_hw_info *hw;
	struct isp_gtm_sync *sync;
	struct isp_gtm_ops gtm_ops;
};

void *ispgtm_rgb_ctx_get(uint32_t idx, enum camera_id cam_id, void *hw);
void isp_gtm_rgb_ctx_put(void *gtm_handle);
void isp_gtm_sync_init(void);
void isp_gtm_sync_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* _ISP_GTM_H_ */
