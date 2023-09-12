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

#ifndef _ISP_CCTX_HEADER_
#define _ISP_CCTX_HEADER_

#include <linux/types.h>
#include <linux/sprd_ion.h>
#include "isp_drv.h"

/* definition for CFG module */
enum cfg_context_id {
	CFG_CONTEXT_P0,
	CFG_CONTEXT_P1,
	CFG_CONTEXT_C0,
	CFG_CONTEXT_C1,
	CFG_CONTEXT_NUM,
};

/* definitions of cmd buf id for each context */
enum cmdbuf_id {
	CMD_BUF_SHADOW = 0,
	CMD_BUF_WORK,
	CMD_BUF_NUM,
};

#define IS_CONTEXT_VALID(id)	\
	(((id) < ISP_SCENE_NUM) && ((id) >= ISP_SCENE_PRE))

#define ISP_CCTX_CMD_BUF_NUM	2  /* shadow-work */
#define ISP_CCTX_CMD_BUF_SIZE  (ISP_REG_SIZE * ISP_CCTX_CMD_BUF_NUM)
#define ISP_CCTX_ALL_CMD_BUF_SIZE  (ISP_CCTX_CMD_BUF_SIZE * ISP_SCENE_NUM)
#define ALIGN_PADDING_SIZE (ISP_REG_SIZE)
#define ISP_CCTX_ALL_CMD_BUF_ALIGN_SIZE \
	(ISP_CCTX_ALL_CMD_BUF_SIZE + ALIGN_PADDING_SIZE)

/*
 * data structures and interface for context handling
 */

/* hw_addr for H/W; sw_addr for kernel */
struct cmd_buf_info {
	/*
	 * virtual address of cmd buf for software to
	 * set registers
	 */
	void *sw_addr;

	/*
	 * phys or iova of cmd buf for CFG HW to get
	 * registers
	 */
	void *hw_addr;

	/*
	 * 1:dirty; 0:clean;
	 * used to identify if the cmd buf
	 * is dirty or not. If dirty, need to
	 * flush this cmd_buf into HW regs.
	 */
	uint32_t dirty;
};

struct isp_cctx_ion_buf {

	/*
	 * shadow-work buf for cfg cmd
	 */
	struct cmd_buf_info cmd_buf[CMD_BUF_NUM];

	/*
	 * current cmd buf id
	 */
	enum cmdbuf_id cur_buf_id;
};


typedef int (*isp_cctx_func_cfg_map_init)(struct isp_pipe_dev *);
typedef int (*isp_cctx_func_ctx_init)(struct isp_cctx_desc *, enum isp_id);
typedef int (*isp_cctx_func_ctx_deinit)(struct isp_cctx_desc *, enum isp_id);
typedef int (*isp_cctx_func_hw)(struct isp_cctx_desc *, enum isp_id,
				enum isp_scene_id);
typedef void (*isp_cctx_func_rst_page_buf)(struct isp_cctx_desc *, enum isp_id);
typedef void (*isp_cctx_func_buf_ready)(struct isp_cctx_desc *, enum isp_id,
					enum isp_scene_id);

struct isp_cctx_intf {
	isp_cctx_func_ctx_init		init_cctx;
	isp_cctx_func_hw		init_hw;
	isp_cctx_func_ctx_deinit	deinit_cctx;
	isp_cctx_func_cfg_map_init	init_cfg_map;
	isp_cctx_func_rst_page_buf	rst_page_buf;
	isp_cctx_func_buf_ready		buf_ready;
};

/*
 * 1. We only allocate one single ION buffer for ISP command buffers.
 * 2. One buffer design is aimed for continuous iova and kernel virtual
 *    address access, but maybe not for physical address.
 * 3. Separate the single buffer for each buffer of each context.
 */
struct isp_cctx_desc {
	const char *owner;
	struct isp_cctx_intf *intf; /* pointer to cfg ctx interface*/
	struct ion_client *client;
	struct ion_handle *handle;
	struct isp_cctx_ion_buf ion_buf[ISP_SCENE_NUM];
	struct isp_buf_info ion_pool;
	int ofst_align;
	spinlock_t lock;
};

struct isp_cctx_desc *isp_cctx_get_desc(enum isp_id);

#endif /* _ISP_CCTX_HEADER_ */
