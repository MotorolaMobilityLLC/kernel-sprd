/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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

#ifndef _DCAM_DRV_H_
#define _DCAM_DRV_H_

#include <video/sprd_img.h>

#include "dcam_reg.h"
#include "cam_common.h"
#include "dcam_int.h"
#include "cam_buf.h"
#include "cam_queue.h"
#include "cam_statistic.h"
#include "isp_3dnr_drv.h"

#define DCAM_LOWEST_ADDR               0x800
#define DCAM_ADDR_INVALID(addr) \
	((unsigned long)(addr) < DCAM_LOWEST_ADDR)
#define DCAM_YUV_ADDR_INVALID(y, u, v) \
	(DCAM_ADDR_INVALID(y) && \
	DCAM_ADDR_INVALID(u) && \
	DCAM_ADDR_INVALID(v))


#define DCAM_STATE_QUICKQUIT           0x01
#define DCAM2_SOFT_RST                 BIT(4)
#define DCAM_AXIM_AQOS_MASK            (0x30FFFF)
#define BPC_CLK_EB_LEP                 BIT(7)
#define DCAM_SC_COEFF_BUF_COUNT        10
#define DCAM_COEFF_CNT                 48

#define FIX_Q                          8
#define FIX_ONE                        (1 << FIX_Q)
#define FIX_CAST(a)                    ((a) << FIX_Q)
#define FIX_UNCAST(a)                  (((a) + (1 << (FIX_Q - 1))) >> FIX_Q)
#define FIX_MUL(a, b)                  (((a) * (b) + (1 << (FIX_Q-1))) \
					>> FIX_Q)
#define FIX64_MUL(a, b)                (((int64_t)(a) * (int64_t)(b) \
					+ (1 << (FIX_Q - 1))) >> FIX_Q)
#define FIX_DIV(a, b)                  ((((a) << FIX_Q) + ((b) / 2)) / (b))
#define FIX_FRACTION(a)                ((a) - (((a) >> FIX_Q) << FIX_Q))
#define FREQ_NUM                       4
#define FREQ_PARAM_NUM                 (FREQ_NUM-1)
#define CHANNEL_NUM                    2

enum chip_id {
	SHARKL3 = 0,
	SHARKLEP
};

enum dcam_drv_rtn {
	DCAM_RTN_SUCCESS = 0,
	DCAM_RTN_PARA_ERR = 0x10,
	DCAM_RTN_IO_ID_ERR,
	DCAM_RTN_ISR_ID_ERR,
	DCAM_RTN_MODE_ERR,
	DCAM_RTN_TIMEOUT,

	DCAM_RTN_CAP_IN_BITS_ERR = 0x20,
	DCAM_RTN_CAP_IN_PATTERN_ERR,
	DCAM_RTN_CAP_SKIP_FRAME_ERR,
	DCAM_RTN_CAP_FRAME_DECI_ERR,
	DCAM_RTN_CAP_XY_DECI_ERR,
	DCAM_RTN_CAP_FRAME_SIZE_ERR,
	DCAM_RTN_CAP_SENSOR_MODE_ERR,
	DCAM_RTN_CAP_IF_MODE_ERR,

	DCAM_RTN_PATH_ADDR_ERR = 0x30,
	DCAM_RTN_PATH_FRAME_LOCKED,
	DCAM_RTN_PATH_GEN_COEFF_ERR,
	DCAM_RTN_PATH_ENDIAN_ERR,
	DCAM_RTN_PATH_OUT_SIZE_ERR,
	DCAM_RTN_PATH_FRM_DECI_ERR,
	DCAM_RTN_MAX
};

enum dcam_cfg_id {
	DCAM_CAP_SENSOR_MODE = 0,
	DCAM_CAP_HREF_SEL,
	DCAM_CAP_DATA_BITS,
	DCAM_CAP_DATA_PATTERN,
	DCAM_CAP_PRE_SKIP_CNT,
	DCAM_CAP_FRM_DECI,
	DCAM_CAP_FRM_COUNT_CLR,
	DCAM_CAP_INPUT_RECT,
	DCAM_CAP_IMAGE_XY_DECI,
	DCAM_CAP_SAMPLE_MODE,
	DCAM_CAP_4IN1_BYPASS,
	DCAM_CAP_DUAL_MODE,

	DCAM_PATH_INPUT_SIZE,
	DCAM_PATH_INPUT_RECT,
	DCAM_PATH_INPUT_ADDR,
	DCAM_PATH_OUTPUT_SIZE,
	DCAM_PATH_OUTPUT_FORMAT,
	DCAM_PATH_OUTPUT_LOOSE,
	DCAM_PATH_OUTPUT_ADDR,
	DCAM_PATH_OUTPUT_RESERVED_ADDR,
	DCAM_PATH_DATA_ENDIAN,
	DCAM_PATH_ENABLE,
	DCAM_PATH_SRC_SEL,
	DCAM_PATH_BUF_NUM,
	DCAM_PATH_ASSOC,
	DCAM_PATH_NEED_DOWNSIZER,
	DCAM_PATH_ZOOM_INFO,

	DCAM_FETCH_DATA_PACKET,
	DCAM_FETCH_DATA_ENDIAN,
	DCAM_FETCH_INPUT_RECT,
	DCAM_FETCH_INPUT_ADDR,
	DCAM_FETCH_START,
	DCAM_CFG_ID_E_MAX
};

enum dcam_data_endian {
	DCAM_ENDIAN_LITTLE = 0,
	DCAM_ENDIAN_BIG,
	DCAM_ENDIAN_HALFBIG,
	DCAM_ENDIAN_HALFLITTLE,
	DCAM_ENDIAN_MAX
};

enum dcam_glb_reg_id {
	DCAM_CFG_REG = 0,
	DCAM_CONTROL_REG,
	DCAM_INIT_MASK_REG,
	DCAM_INIT_CLR_REG,
	DCAM_AHBM_STS_REG,
	DCAM_ENDIAN_REG,
	DCAM_AXIM_REG,
	DCAM_REG_MAX
};

enum path_status {
	DCAM_ST_STOP = 0,
	DCAM_ST_START,
	DCAM_ST_PAUSE,
};

struct dcam_cap_sync_pol {
	unsigned char vsync_pol;
	unsigned char hsync_pol;
	unsigned char pclk_pol;
	unsigned char need_href;
	unsigned char pclk_src;
	unsigned char reserved[3];
};

struct dcam_cap_dec {
	unsigned char x_factor;
	unsigned char y_factor;
};

struct dcam_cap_desc {
	enum dcam_cap_sensor_mode input_format;
	enum dcam_capture_mode cap_mode;
	struct camera_rect cap_rect;
};

struct dcam_fetch_desc {
	int is_loose;
	struct camera_rect input_rect;
	struct camera_frame frame;
};

struct dcam_path_valid {
	uint32_t output_format:1;
	uint32_t src_sel:1;
	uint32_t data_endian:1;
};

struct dcam_zoom_param {
	struct camera_size in_size;
	struct camera_rect in_rect;
	struct camera_size out_size;
	unsigned char bin_crop_bypass;
	uint32_t coeff_ptr[DCAM_COEFF_CNT];
};

struct dcam_path_desc {
	enum camera_path_id id;
	struct camera_size input_size;
	struct camera_rect input_rect;
	struct camera_size output_size;
	struct camera_frame ion_buffer[DCAM_FRM_QUEUE_LENGTH];
	struct cam_buf_queue buf_queue;/*todo link*/
	struct cam_frm_queue frame_queue;/*done link*/
	struct camera_frame reserved_frame;
	struct camera_endian_sel data_endian;
	struct dcam_path_valid valid_param;
	uint32_t frame_base_id;
	uint32_t output_frame_count;
	uint32_t output_format;
	uint32_t is_loose;
	uint32_t src_sel;
	uint32_t valid;
	enum path_status status;
	uint32_t is_update;
	uint32_t need_wait;
	uint32_t ion_buf_cnt;
	uint32_t buf_num; /*buf num share with isp*/
	void *private_data;
	uint32_t assoc_idx;
	uint32_t need_downsizer;
	struct cam_4in1_addr addr_4in1[DCAM_FRM_QUEUE_LENGTH];
	struct camera_size out_size_latest;
	struct cam_buf_queue coeff_queue;
	struct zoom_info_t zoom_info;
};

struct dcam_3dnr_me {
	int mv_x;
	int mv_y;
	uint32_t mv_ready_cnt;
	uint32_t bin_frame_cnt;
	uint32_t full_frame_cnt;
	struct camera_frame bin_frame;
	struct camera_frame full_frame;
};

struct dcam_fast_me_param {
	uint32_t nr3_channel_sel;
	uint32_t nr3_project_mode;
	uint32_t nr3_ping_pang_en;
	uint32_t nr3_bypass;
	uint32_t roi_start_x;
	uint32_t roi_start_y;
	uint32_t roi_width;
	uint32_t roi_height;
	uint32_t cap_in_size_w;
	uint32_t cap_in_size_h;
};

struct dcam_module {
	enum dcam_id id;
	struct dcam_cap_desc dcam_cap;
	struct dcam_fetch_desc dcam_fetch;
	struct dcam_path_desc full_path;
	struct dcam_path_desc bin_path;
	struct cam_statis_module statis_module_info;
	struct dcam_3dnr_me fast_me;
	struct dcam_fast_me_param me_param;
	uint32_t need_nr3;
	uint32_t need_4in1;
	uint32_t state;
	uint32_t frame_id;
	uint32_t cap_4in1;
	uint32_t is_high_fps;
	uint32_t high_fps_skip_num;
	uint32_t high_fps_cnt;
	uint32_t time_index;
	struct camera_time time[DCAM_FRM_QUEUE_LENGTH];
	struct dual_sync_info dual_info[DCAM_FRM_QUEUE_LENGTH];
};

struct dcam_group {
	uint32_t dual_cam;
	struct dcam_module *dcam[DCAM_MAX_COUNT];
};

struct crop_param_t {
	unsigned char bypass;
	uint32_t start_x;
	uint32_t start_y;
	uint32_t end_x;
	uint32_t end_y;
};

struct  crop_info_t {
	uint32_t crop_startx;
	uint32_t crop_starty;
	uint32_t crop_width;
	uint32_t crop_height;
};

struct rawsizer_param_t {
	unsigned char bypass;
	unsigned char crop_en;
	uint32_t crop_start_x;
	uint32_t crop_start_y;
	uint32_t crop_width;
	uint32_t crop_height;
	uint32_t output_width;
	uint32_t output_height;
	uint32_t is_zoom_min;
};


int sprd_dcam_drv_dev_init(enum dcam_id idx);
int sprd_dcam_drv_dev_deinit(enum dcam_id idx);
int sprd_dcam_drv_path_unmap(enum dcam_id idx);
void sprd_dcam_drv_path_clear(enum dcam_id idx);
int sprd_dcam_drv_module_en(enum dcam_id idx);
int sprd_dcam_drv_module_dis(enum dcam_id idx);
int sprd_dcam_drv_start(enum dcam_id idx);
int sprd_dcam_drv_stop(enum dcam_id idx, int is_irq);
void sprd_dcam_drv_irq_mask_en(enum dcam_id idx);
void sprd_dcam_drv_irq_mask_dis(enum dcam_id idx);
int sprd_dcam_drv_reset(enum dcam_id idx, int is_irq);
int sprd_dcam_drv_path_pause(enum dcam_id idx, uint32_t channel_id);
int sprd_dcam_drv_path_resume(enum dcam_id idx, uint32_t channel_id);
int sprd_dcam_drv_init(struct platform_device *p_dev);
void sprd_dcam_drv_deinit(void);
int sprd_dcam_drv_dt_parse(struct platform_device *pdev, uint32_t *dcam_count);
void sprd_dcam_drv_glb_reg_awr(enum dcam_id idx, unsigned long addr,
			uint32_t val, uint32_t reg_id);
void sprd_dcam_drv_glb_reg_owr(enum dcam_id idx, unsigned long addr,
			uint32_t val, uint32_t reg_id);
void sprd_dcam_drv_glb_reg_mwr(enum dcam_id idx, unsigned long addr,
			uint32_t mask, uint32_t val,
			uint32_t reg_id);
void sprd_dcam_drv_force_copy(enum dcam_id idx,
	enum camera_copy_id copy_id);
void sprd_dcam_drv_auto_copy(enum dcam_id idx, enum camera_copy_id copy_id);
struct dcam_group *sprd_dcam_drv_group_get(void);
struct dcam_module *sprd_dcam_drv_module_get(enum dcam_id idx);
struct dcam_cap_desc *sprd_dcam_drv_cap_get(enum dcam_id idx);
int sprd_dcam_cap_cfg_set(enum dcam_id idx, enum dcam_cfg_id id, void *param);
struct dcam_fetch_desc *sprd_dcam_drv_fetch_get(enum dcam_id idx);
int sprd_dcam_fetch_cfg_set(enum dcam_id idx,
	enum dcam_cfg_id id, void *param);

struct dcam_path_desc *sprd_dcam_drv_full_path_get(enum dcam_id idx);
int sprd_dcam_full_path_init(enum dcam_id idx);
int sprd_dcam_full_path_map(struct cam_buf_info *buf_info);
int sprd_dcam_full_path_unmap(enum dcam_id idx);
void sprd_dcam_full_path_clear(enum dcam_id idx);
int sprd_dcam_full_path_deinit(enum dcam_id idx);
int sprd_dcam_full_path_cfg_set(enum dcam_id idx,
	enum dcam_cfg_id id, void *param);
int sprd_dcam_full_path_start(enum dcam_id idx);
void sprd_dcam_full_path_quickstop(enum dcam_id idx);
int sprd_dcam_full_path_next_frm_set(enum dcam_id idx);
int sprd_dcam_full_path_buf_reset(enum dcam_id idx);

struct dcam_path_desc *sprd_dcam_drv_bin_path_get(enum dcam_id idx);
int sprd_dcam_bin_path_init(enum dcam_id idx);
int sprd_dcam_bin_path_map(struct cam_buf_info *buf_info);
int sprd_dcam_bin_path_unmap(enum dcam_id idx);
void sprd_dcam_bin_path_clear(enum dcam_id idx);
int sprd_dcam_bin_path_deinit(enum dcam_id idx);
int sprd_dcam_bin_path_cfg_set(enum dcam_id idx,
	enum dcam_cfg_id id, void *param);
int sprd_dcam_bin_path_start(enum dcam_id idx);
int sprd_dcam_bin_path_next_frm_set(enum dcam_id idx);
int sprd_dcam_bin_path_scaler_cfg(enum dcam_id idx);
int sprd_dcam_raw_sizer_coeff_gen(
		uint16_t src_width, uint16_t src_height,
		uint16_t dst_width, uint16_t dst_height,
		uint32_t *coeff_buf, uint32_t is_zoom_min);


int sprd_dcam_drv_3dnr_me_set(uint32_t idx, void *size);
int sprd_dcam_drv_3dnr_fast_me_info_get(enum dcam_id idx,
	uint32_t need_nr3, struct camera_size *size);
struct dcam_fast_me_param *sprd_dcam_drv_3dnr_me_param_get(enum dcam_id idx);
int sprd_dcam_drv_chip_id_get(void);
void sprd_dcam_drv_reg_trace(enum dcam_id idx);
void sprd_dcam_drv_4in1_info_get(uint32_t need_4in1);
int sprd_dcam_drv_zoom_param_update(struct dcam_zoom_param zoom_param,
			uint32_t is_zoom_min,
			enum dcam_id idx);
void sprd_dcam_drv_update_crop_param(struct crop_param_t *o_crop_param,
	struct zoom_info_t *io_zoom_info, int zoom_ratio);
void sprd_dcam_drv_update_rawsizer_param(
		struct rawsizer_param_t *o_rawsizer_param,
		struct zoom_info_t *io_zoom_info);
void sprd_dcam_drv_init_online_pipe(struct zoom_info_t *o_zoom_info,
	int sensor_width, int sensor_height,
	int output_width, int output_height);
#endif /* _DCAM_DRV_H_ */
