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

#ifndef _DCAM_FMCU_H_
#define _DCAM_FMCU_H_

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>

#include "cam_buf.h"

#define DCAM_FMCU_CMDQ_SIZE              0x1000
#define DCAM_FMCU_PUSH(fmcu, addr, cmd)  fmcu->ops->push_cmdq(fmcu, addr, cmd)

enum dcam_fmcu_id {
	DCAM_FMCU_0,
	DCAM_FMCU_NUM
};

enum dcam_fmcu_buf_id {
	DCAM_FMCU_PING,
	DCAM_FMCU_PANG,
	DCAM_FMCU_BUF_MAX
};

enum {
	DCAM_FMCU_IS_NEED = (1 << 0),
	DCAM_FMCU_IS_MUST = (1 << 1),
};

enum dcam_fmcu_cmd {
	DCAM0_POF3_DISPATCH = 0x10,
	DCAM0_PREVIEW_DONE,
	DCAM0_CAPTURE_DONE,
	DCAM0_RAW_PATH_DONE,
	DCAM0_CAP_SOF,
	DCAM0_DATA2PATH_POF,
	DCAM1_POF3_DISPATCH,
	DCAM1_PREVIEW_DONE,
	DCAM1_CAPTURE_DONE,
	DCAM1_RAW_PATH_DONE,
	DCAM1_CAP_SOF,
	DCAM1_DATA2PATH_POF,
	FETCH_BUSY,
	DCAM_BUSY,
	DCAM_FMCU_CMD_MAX
};

struct dcam_fmcu_ops;
struct dcam_fmcu_ctx_desc {
	enum dcam_fmcu_id fid;
	enum dcam_fmcu_buf_id cur_buf_id;
	struct camera_buf ion_pool[DCAM_FMCU_BUF_MAX];
	uint32_t *cmd_buf[DCAM_FMCU_BUF_MAX];
	unsigned long hw_addr[DCAM_FMCU_BUF_MAX];
	size_t cmdq_size;
	size_t cmdq_pos[DCAM_FMCU_BUF_MAX];
	spinlock_t lock;
	atomic_t  user_cnt;
	struct list_head list;
	struct dcam_fmcu_ops *ops;
	struct cam_hw_info *hw;
	uint32_t hw_ctx_id;
};

struct dcam_fmcu_ops {
	int (*ctx_init)(struct dcam_fmcu_ctx_desc *fmcu_ctx);
	int (*ctx_deinit)(struct dcam_fmcu_ctx_desc *fmcu_ctx);
	int (*ctx_reset)(struct dcam_fmcu_ctx_desc *fmcu_ctx);
	int (*push_cmdq)(struct dcam_fmcu_ctx_desc *fmcu_ctx,
					uint32_t addr, uint32_t cmd);
	int (*hw_start)(struct dcam_fmcu_ctx_desc *fmcu_ctx);
	int (*cmd_ready)(struct dcam_fmcu_ctx_desc *fmcu_ctx);
	int (*buf_map)(void *handle);
	int (*buf_unmap)(void *handle);
};

struct dcam_fmcu_ctx_desc *dcam_fmcu_ctx_desc_get(void *arg, uint32_t index);
int dcam_fmcu_ctx_desc_put(struct dcam_fmcu_ctx_desc *fmcu);

#endif/* _DCAM_FMCU_H_ */
