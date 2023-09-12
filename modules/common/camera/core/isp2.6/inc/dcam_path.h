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

#ifndef _DCAM_PATH_H_
#define _DCAM_PATH_H_

#include "dcam_core.h"

#define DCAM_PATH_WMAX_ROC1                     6000
#define DCAM_PATH_HMAX_ROC1                     4000

const char *dcam_path_name_get(enum dcam_path_id path_id);

int dcam_path_base_cfg(void *dcam_ctx_handle,
				struct dcam_path_desc *path,
				void *param);

int dcam_path_size_cfg(void *dcam_ctx_handle,
				struct dcam_path_desc *path,
				void *param);
int dcam_path_skip_num_set(void *dcam_ctx_handle,
				int path_id, uint32_t skip_num);

/* / TODO: refine this*/
int dcam_path_store_frm_set(void *dcam_ctx_handle, struct dcam_path_desc *path);
int dcam_path_fmcu_slw_queue_set(struct dcam_sw_context *sw_ctx);
#endif
