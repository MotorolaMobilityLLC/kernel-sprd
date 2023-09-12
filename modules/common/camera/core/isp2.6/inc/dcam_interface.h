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

#ifndef _DCAM_INTERFACE_H_
#define _DCAM_INTERFACE_H_

#include <linux/platform_device.h>

#include "cam_hw.h"
#include "cam_types.h"

/* 4-pixel align for MIPI CAP input from CSI */
#define DCAM_MIPI_CAP_ALIGN             4

/* 2-pixel align for RDS output size */
#define DCAM_RDS_OUT_ALIGN              2
/* 16-pixel align for debug convenience */
#define DCAM_OUTPUT_DEBUG_ALIGN         16

/* align size for full/bin crop, use 4 for zzhdr sensor, 2 for normal sensor */
#define DCAM_CROP_SIZE_ALIGN            4

/* dcam dec pyramid layer num */
#define DCAM_PYR_DEC_LAYER_NUM          4

/*
 * Quick function to check is @idx valid.
 */
#define is_dcam_id(idx)                 ((idx) < DCAM_ID_MAX)
#define is_csi_id(idx)                  ((idx) < CSI_ID_MAX)

enum dcam_pdaf_type {
	DCAM_PDAF_DUAL = 0,
	DCAM_PDAF_TYPE1 = 1,
	DCAM_PDAF_TYPE2 = 2,
	DCAM_PDAF_TYPE3 = 3,
};

enum dcam_sw_context_id {
	DCAM_SW_CONTEXT_0 = 0,
	DCAM_SW_CONTEXT_1,
	DCAM_SW_CONTEXT_2,
	DCAM_SW_CONTEXT_3,
	DCAM_SW_CONTEXT_4,
	DCAM_SW_CONTEXT_5,
	DCAM_SW_CONTEXT_6,
	DCAM_SW_CONTEXT_MAX,
};

enum dcam_sw_ctx_type {
	DCAM_ONLINE_CTX = 0,
	DCAM_OFFLINE_CTX,
};

struct statis_path_buf_info {
	enum dcam_path_id path_id;
	size_t buf_size;
	size_t buf_cnt;
	uint32_t buf_type;
};

/*
 * Quick function to check is @id valid.
 */
#define is_path_id(id) ((id) >= DCAM_PATH_FULL && (id) < DCAM_PATH_MAX)

enum dcam_path_cfg_cmd {
	DCAM_PATH_CFG_CTX_BASE = 0,
	DCAM_PATH_CFG_BASE = 0,
	DCAM_PATH_CFG_OUTPUT_BUF,
	DCAM_PATH_CFG_OUTPUT_ALTER_BUF,
	DCAM_PATH_CLR_OUTPUT_ALTER_BUF,
	DCAM_PATH_CFG_OUTPUT_RESERVED_BUF,
	DCAM_PATH_CFG_SIZE,
	DCAM_PATH_CFG_FULL_SOURCE,/* 4in1 select full path source */
	DCAM_PATH_CFG_SHUTOFF,
	DCAM_PATH_CFG_STATE,
	DCAM_PATH_CLR_OUTPUT_SHARE_BUF,
};


static inline uint32_t dcam_if_cal_pyramid_size(uint32_t w, uint32_t h,
		uint32_t out_bits, uint32_t is_pack, uint32_t start_layer, uint32_t layer_num)
{
	uint32_t align = 1, i = 0, size = 0, pitch = 0;
	uint32_t w_align = PYR_DEC_WIDTH_ALIGN;
	uint32_t h_align = PYR_DEC_HEIGHT_ALIGN;

	for (i = start_layer; i <= layer_num; i++) {
		w_align *= 2;
		h_align *= 2;
	}
	for (i = 0; i < start_layer; i++)
		align = align * 2;

	w = ALIGN(w, w_align);
	h = ALIGN(h, h_align);
	for (i = start_layer; i <= layer_num; i++) {
		pitch = w / align;
		if (out_bits != DCAM_STORE_8_BIT) {
			if(is_pack)
				pitch = (pitch * 10 + 127) / 128 * 128 / 8;
			else
				pitch = (pitch * 16 + 127) / 128 * 128 / 8;
		}
		size = size + pitch * (h / align) * 3 / 2;
		align = align * 2;
	}

	return size;
}

enum dcam_ioctrl_cmd {
	DCAM_IOCTL_CFG_CAP,
	DCAM_IOCTL_CFG_STATIS_BUF,
	DCAM_IOCTL_PUT_RESERV_STATSBUF,
	DCAM_IOCTL_RECFG_PARAM,
	DCAM_IOCTL_INIT_STATIS_Q,
	DCAM_IOCTL_DEINIT_STATIS_Q,
	DCAM_IOCTL_CFG_EBD,
	DCAM_IOCTL_CFG_SEC,
	DCAM_IOCTL_CFG_FBC,
	DCAM_IOCTL_CFG_RPS, /* raw proc scene */
	DCAM_IOCTL_CFG_REPLACER,
	DCAM_IOCTL_GET_PATH_RECT,
	DCAM_IOCTL_CFG_STATIS_BUF_SKIP,
	DCAM_IOCTL_CFG_GTM_UPDATE,
	DCAM_IOCTL_CFG_PYR_DEC_EN,
	DCAM_IOCTL_CREAT_INT_THREAD,
	DCAM_IOCTL_CFG_MAX,
};

/*
 *DCAM_STOP: normal stop;
 *DCAM_PAUSE_ONLINE: online paused; pause for fdr use same dcam for following offline process
 *DCAM_PAUSE_OFFLINE: offline paused; after fdr use same dcam for offline process and prepare for online resume
 */
enum dcam_stop_cmd {
	DCAM_STOP,
	DCAM_PAUSE_ONLINE,
	DCAM_PAUSE_OFFLINE,
	DCAM_RECOVERY,
	DCAM_DEV_ERR,
};

struct dcam_cap_cfg {
	uint32_t sensor_if;/* MIPI CSI-2 */
	uint32_t format;/* input color format */
	uint32_t mode;/* single or multi mode. */
	uint32_t data_bits;
	uint32_t pattern;/* bayer mode for rgb, yuv pattern for yuv */
	uint32_t href;
	uint32_t frm_deci;
	uint32_t frm_skip;
	uint32_t x_factor;
	uint32_t y_factor;
	uint32_t is_4in1;
	uint32_t dcam_slice_mode;
	uint32_t is_cphy;
	struct img_trim cap_size;
};

struct dcam_path_cfg_param {
	uint32_t slowmotion_count;
	uint32_t enable_3dnr;
	uint32_t is_raw;
	uint32_t raw_src;
	uint32_t raw_fmt;
	uint32_t bayer_pattern;
	uint32_t dcam_out_fmt;
	uint32_t dcam_out_bits;
	uint32_t is_4in1;
	uint32_t frm_deci;
	uint32_t frm_skip;
	uint32_t force_rds;
	uint32_t raw_cap;
	uint32_t is_pack;
	uint32_t pyr_data_bits;
	uint32_t pyr_is_pack;
	void *priv_size_data;
	struct img_endian endian;
	struct img_size input_size;
	struct img_trim input_trim;
	struct img_trim total_input_trim;
	struct img_size output_size;
	struct sprd_img_rect zoom_ratio_base;
};

enum dcam_start_ctrl {
	DCAM_START_CTRL_EN,
	DCAM_START_CTRL_DIS,
	DCAM_START_CTRL_MAX,
};

enum dcam_callback_ctrl {
	DCAM_CALLBACK_CTRL_USER,
	DCAM_CALLBACK_CTRL_ISP,
	DCAM_CALLBACK_CTRL_MAX,
};

struct dcam_data_ctrl_info {
	enum dcam_start_ctrl start_ctrl;
	enum dcam_callback_ctrl callback_ctrl;
	enum raw_alg_types raw_alg_type;
	uint32_t in_format;
	uint32_t out_format;
	uint32_t need_raw_path;
};

/*
 * supported operations for dcam_if device
 *
 * @open:         initialize software and hardware resource for dcam_if
 * @close:        uninitialize resource for dcam_if
 * @start:        configure MIPI parameters and enable capture, parameters
 *                must be updated by ioctl() before this call
 * @stop:         // TODO: fill this
 * @get_path:
 * @put_path:
 * @cfg_path:
 * @ioctl:
 * @proc_frame:
 * @set_callback:
 * @update_clk:
 *
 */
struct dcam_pipe_ops {
	int (*open)(void *handle);
	int (*close)(void *handle);
	int (*reset)(void *handle);
	int (*start)(void *handle, int online);
	int (*stop)(void *handle, enum dcam_stop_cmd pause);
	int (*get_path)(void *handle, int path_id);
	int (*put_path)(void *handle, int path_id);
	int (*cfg_path)(void *dcam_handle, enum dcam_path_cfg_cmd cfg_cmd, int path_id, void *param);
	int (*ioctl)(void *handle, enum dcam_ioctrl_cmd cmd, void *arg);
	int (*cfg_blk_param)(void *handle, void *param);
	int (*set_callback)(void *handle, int ctx_id, dcam_dev_callback cb, void *priv_data);
	int (*update_clk)(void *handle, void *arg);
	int (*get_context)(void *dcam_handle);
	int (*put_context)(void *dcam_handle, int ctx_id);
	int (*get_datactrl)(void *handle, void *in, void *out);
	int (*share_buf_set_cb)(void *handle, int ctx_id, share_buf_get_cb cb, void *priv_data);
};

/*
 * A nr3_me_data object carries motion vector and settings. Downstream module
 * who peforms noice reduction operation uses these information to calculate
 * correct motion vector for target image size.
 *
 * *****Note: target image should not be cropped*****
 *
 * @valid:         valid bit
 * @sub_me_bypass: sub_me_bypass bit, this has sth to do with mv calculation
 * @project_mode:  project_mode bit, 0 for step, 1 for interlace
 * @mv_x:          motion vector in x direction
 * @mv_y:          motion vector in y direction
 * @src_width:     source image width
 * @src_height:    source image height
 */
struct nr3_me_data {
	uint32_t valid:1;
	uint32_t sub_me_bypass:1;
	uint32_t project_mode:1;
	s8 mv_x;
	s8 mv_y;
	uint32_t src_width;
	uint32_t src_height;
	uint32_t full_path_mv_ready;
	uint32_t full_path_cnt;
	uint32_t bin_path_mv_ready;
	uint32_t bin_path_cnt;
};

/*
 * Retrieve a dcam_if device for the hardware. A dcam_if device is a wrapper
 * with supported operations defined in dcam_pipe_ops.
 */
void *dcam_core_pipe_dev_get(struct cam_hw_info *hw);
/*
 * Release a dcam_if device after capture finished.
 */
int dcam_core_pipe_dev_put(void *dcam_handle);

/*
 * Retrieve hardware info from dt.
 */
int dcam_drv_hw_init(void *arg);
int dcam_drv_hw_deinit(void *arg);
int dcam_drv_dt_parse(struct platform_device *pdev,
			struct cam_hw_info *hw_info,
			uint32_t *dcam_count);

#endif/* _DCAM_INTERFACE_H_ */
