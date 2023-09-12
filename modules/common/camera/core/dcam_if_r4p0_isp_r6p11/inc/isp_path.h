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

#ifndef _ISP_PATH_HEADER_
#define _ISP_PATH_HEADER_

#include "sprd_mm.h"
#include "isp_drv.h"
#include "isp_reg.h"
#include "isp_slice.h"

#define CAMERA_RESERVE_FRAME_NUM	0xffffffff

int isp_start_pre_proc(struct isp_path_desc *pre,
			struct isp_path_desc *vid,
			struct isp_path_desc *cap,
		       struct isp_offline_desc *off_desc);
int isp_set_sc_coeff_info(uint32_t idx, uint32_t addr,
	uint32_t *coeff_buf);
void isp_path_set(struct isp_module *module,
		  struct isp_path_desc *path, enum isp_path_index path_index);
void isp_path_set_scl(uint32_t idx, struct isp_path_desc *path,
	uint32_t addr);
int isp_get_scl_index(uint32_t channel_id);
int isp_path_set_next_frm(struct isp_module *module,
	enum isp_path_index path_index, struct slice_addr *addr);
#endif
