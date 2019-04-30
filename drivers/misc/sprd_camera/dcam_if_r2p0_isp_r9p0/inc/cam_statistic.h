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

#ifndef _CAM_STATISTIC_H_
#define _CAM_STATISTIC_H_

#include "cam_buf.h"
#include "cam_queue.h"
#include "cam_common.h"
#include "sprd_isp_hw.h"

/*all statitics queue node definitions*/
struct cam_statis_buf {
	uint32_t buf_size;
	int buf_property;
	unsigned long phy_addr;
	unsigned long vir_addr;
	unsigned long addr_offset;
	unsigned long kaddr[2];
	unsigned long mfd;
	uint32_t frame_id;
	struct cam_buf_info buf_info;
};

/*all statitics queues attached with dcam/isp*/
struct cam_statis_module {
	struct cam_frm_queue aem_statis_frm_queue;
	struct cam_frm_queue afl_statis_frm_queue;
	struct cam_frm_queue afm_statis_frm_queue;
	struct cam_frm_queue pdaf_statis_frm_queue;
	struct cam_frm_queue ebd_statis_frm_queue;
	struct cam_frm_queue hist_statis_frm_queue;
	struct cam_statis_buf aem_buf_reserved;
	struct cam_statis_buf afl_buf_reserved;
	struct cam_statis_buf afm_buf_reserved;
	struct cam_statis_buf nr3_buf_reserved;
	struct cam_statis_buf pdaf_buf_reserved;
	struct cam_statis_buf ebd_buf_reserved;
	struct cam_buf_queue aem_statis_queue;
	struct cam_buf_queue afl_statis_queue;
	struct cam_buf_queue afm_statis_queue;
	struct cam_buf_queue pdaf_statis_queue;
	struct cam_buf_queue ebd_statis_queue;
	struct cam_buf_queue hist_statis_queue;
	struct cam_statis_buf img_statis_buf;
	uint32_t idx;
	uint32_t module_flag;
	uint32_t statis_valid;
};

enum cam_statis_dev {
	DCAM_DEV_STATIS = 0,
	ISP_DEV_STATIS
};

int sprd_cam_statistic_queue_init(struct cam_statis_module *module,
	uint32_t idx, int module_flag);
void sprd_cam_statistic_queue_clear(struct cam_statis_module *module);
int sprd_cam_statistic_map(struct cam_statis_buf *statis_buf);
void sprd_cam_statistic_unmap(struct cam_statis_buf *statis_buf);
void sprd_cam_statistic_queue_deinit(struct cam_statis_module *module);
int sprd_cam_statistic_buf_cfg(struct cam_statis_module *module,
	struct isp_statis_buf_input *parm);
int sprd_cam_statistic_addr_set(struct cam_statis_module *module,
	struct isp_statis_buf_input *parm);
int sprd_cam_statistic_buf_set(struct cam_statis_module *module);
int sprd_cam_statistic_next_buf_set(struct cam_statis_module *module,
	enum isp_3a_block_id block_index,
	uint32_t frame_id);

#endif
