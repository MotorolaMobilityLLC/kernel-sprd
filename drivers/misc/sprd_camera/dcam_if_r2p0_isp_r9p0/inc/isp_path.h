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

#ifndef _ISP_PATH_HEADER_
#define _ISP_PATH_HEADER_

#include <video/sprd_mm.h>

#include "isp_drv.h"
#include "isp_slice.h"

extern struct isp_group s_isp_group;

int sprd_isp_path_pre_proc_start(struct isp_path_desc *pre,
		struct isp_path_desc *vid,
		struct isp_path_desc *cap);
void sprd_isp_path_pathset(struct isp_module *module,
		struct isp_path_desc *path, enum isp_path_index path_index);
int sprd_isp_path_next_frm_set(struct isp_module *module,
	enum isp_path_index path_index, struct camera_frame *dcam_frame);
void sprd_isp_path_offline_frame_set(struct isp_pipe_dev *dev,
	enum camera_path_id path_index, struct camera_frame *frame);
void sprd_isp_path_raw_proc_start(struct isp_pipe_dev *dev,
	enum camera_path_id path_index, struct camera_frame *frame);
int sprd_isp_path_param_cfg(struct isp_path_desc *path);
int sprd_isp_path_4in1_cap_start(struct isp_pipe_dev *dev, isp_isr_func func,
	void *data, uint32_t cap_type);
int sprd_isp_path_4in1_cap_stop(struct isp_pipe_dev *dev);
void sprd_isp_path_4in1_raw_proc_start(struct isp_pipe_dev *dev,
	struct camera_frame *frame);
int sprd_isp_path_4in1_scaler_update(void *isp_handle);

#endif
