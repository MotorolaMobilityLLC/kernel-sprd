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

#ifndef _ISP_SLW_HEADER_
#define _ISP_SLW_HEADER_

#include "isp_drv.h"

#define ISP_SLW_FRM_NUM			4
#define ISP_SLW_BUF_NUM			4
#define FMCU_ALIGN			8

enum isp_slw_rtn {
	ISP_RTN_SLW_SUCCESS = 0,
	ISP_RTN_SLW_DQ_ERR,
	ISP_RTN_SLW_EQ_ERR,
	ISP_RTN_SLW_FRM_ERR,
};

struct isp_int_info {
	uint32_t sdw_done_skip_en;
	uint32_t sdw_done_int_cnt_num;
	uint32_t sdw_done_skip_cnt_num;
	uint32_t sdw_done_skip_cnt_clr;
	uint32_t vid_done_skip_en;
	uint32_t vid_done_int_cnt_num;
	uint32_t vid_done_skip_cnt_num;
	uint32_t vid_done_skip_cnt_clr;
};

struct isp_fmcu_cmd_frm {
	uint32_t yaddr_frm;
	uint32_t yaddr_reg;
	uint32_t uaddr_frm;
	uint32_t uaddr_reg;
	uint32_t vaddr_frm;
	uint32_t vaddr_reg;
	uint32_t shadow_frm;
	uint32_t shadow_reg;
};

struct isp_slw_cmd {
	struct isp_fmcu_cmd_frm cmd_frm[ISP_SLW_FRM_NUM];
};

struct isp_slw_info {
	unsigned long *fmcu_addr_vir;
	phys_addr_t fmcu_addr_phy;
	uint32_t fmcu_num;
	uint32_t is_reserved;
	struct isp_frm_queue slw_queue;
};

struct isp_slw_queue {
	struct isp_slw_info slw_array[ISP_SLW_BUF_NUM];
	uint32_t valid_cnt;
};

struct isp_fmcu_slw_info {
	struct isp_slw_queue empty_queue;
	struct isp_slw_queue embed_queue;
	struct isp_slw_queue insert_queue;
	struct isp_slw_info slw_reserved;
	struct isp_int_info sdw_done_info;
};

int slowmotion_frame_enqueue(struct isp_slw_queue *queue,
			      struct isp_slw_info *slw);
int slowmotion_frame_dequeue(struct isp_slw_queue *queue,
			      struct isp_slw_info *slw);
int set_isp_fmcu_cmd_reg(enum isp_scl_id path_id, void *isp_handle);
int set_isp_fmcu_slw_cmd(void *isp_handle,
		enum isp_scl_id path_id, uint32_t buf_reserved);
int set_fmcu_slw_cfg(void *handle);
int get_slw_status(void *isp_handle);
void isp_slw_clear(void *handle);
int isp_slw_flags_init(void *isp_handle, struct isp_path_info *info);
int isp_fmcu_slw_start(enum isp_scl_id path_id, void *isp_handle);
int isp_fmcu_slw_init(void **fmcu_handler);
int isp_fmcu_slw_deinit(void *fmcu_handler);
#endif
