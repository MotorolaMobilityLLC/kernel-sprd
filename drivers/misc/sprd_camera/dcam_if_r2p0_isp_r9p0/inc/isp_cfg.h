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

#ifndef _ISP_CFG_HEADER_
#define _ISP_CFG_HEADER_

#include <linux/types.h>
#include <linux/sprd_ion.h>

#include "cam_buf.h"

/* definition for CFG module */
enum cfg_context_id {
	CFG_CONTEXT_P0,
	CFG_CONTEXT_C0,
	CFG_CONTEXT_P1,
	CFG_CONTEXT_C1,
	CFG_CONTEXT_NUM,
};

/* The max offset of isp REG is 0x39FFC,
 * so the buffer size we alloc must bigger than 0x39FFC
 */
#define ISP_CFG_ON            1
#define ISP_REG_SIZE          0x40000UL
#define ISP_CFG_BUF_NUM       3
#define ISP_CFG_BUF_SIZE      (ISP_REG_SIZE * ISP_CFG_BUF_NUM)
#define ISP_CFG_BUF_SIZE_ALL  (ISP_CFG_BUF_SIZE * ISP_SCENE_MAX \
		 + ISP_REG_SIZE)
#define ALIGN_PADDING_SIZE    (ISP_REG_SIZE)

enum cfg_buf_id {
	CFG_BUF_SHADOW = 0,
	CFG_BUF_WORK_PING,
	CFG_BUF_WORK_PONG,
	CFG_BUF_NUM,
};

struct isp_cfg_buf_info {
	void *vir_addr;
	void *phy_addr;
	uint32_t flag;
};

struct isp_cfg_buf {
	struct isp_cfg_buf_info cmd_buf[ISP_CFG_BUF_NUM];
	enum cfg_buf_id cur_buf_id;
};

struct isp_cfg_ctx_desc {
	uint32_t cur_isp_id;
	uint32_t cur_scene_id;
	uint32_t cur_work_mode;
	struct isp_cfg_buf cfg_buf[2];/*work pre,cap/vid*/
	uint32_t temp_cfg_buf[2][ISP_REG_SIZE/4]; /*shadow before start*/
	struct cam_buf_info buf_info;
	spinlock_t lock;
	atomic_t cfg_map_lock;
};

int sprd_isp_cfg_ctx_init(struct isp_cfg_ctx_desc *cfg_ctx, uint32_t idx);
int sprd_isp_cfg_ctx_deinit(struct isp_cfg_ctx_desc *cfg_ctx);
int sprd_isp_cfg_buf_reset(struct isp_cfg_ctx_desc *cfg_ctx);
int sprd_isp_cfg_buf_update(struct isp_cfg_ctx_desc *cfg_ctx);
int sprd_isp_cfg_block_config(struct isp_cfg_ctx_desc *cfg_ctx);
void sprd_isp_cfg_isp_start(struct isp_cfg_ctx_desc *cfg_ctx);
int sprd_isp_cfg_map_init(struct isp_cfg_ctx_desc *cfg_ctx);
int sprd_isp_cfg_ctx_buf_init(struct isp_cfg_ctx_desc *cfg_ctx, uint32_t flag);

#endif /* _ISP_CFG_HEADER_ */
