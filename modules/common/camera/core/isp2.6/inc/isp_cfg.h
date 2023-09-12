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

#ifndef _ISP_CFG_H_
#define _ISP_CFG_H_

#include <linux/types.h>
#include <linux/platform_device.h>

#include "cam_buf.h"
#include "isp_interface.h"

enum cfg_buf_id {
	CFG_BUF_SW = 0,
	CFG_BUF_HW,
	CFG_BUF_NUM
};

/* The max offset of isp REG is 0x39FFC,
 * so the buffer size we alloc must bigger than 0x39FFC
 */

#define ISP_REG_SIZE                     0x40000UL
#define ISP_CFG_BUF_SIZE                 (ISP_REG_SIZE * 1)
#define ISP_CFG_BUF_SIZE_SW_ALL          (ISP_CFG_BUF_SIZE * ISP_CONTEXT_SW_NUM)
#define ALIGN_PADDING_SIZE               (ISP_REG_SIZE)
#define ISP_CFG_BUF_SIZE_HW_PADDING      \
	(ISP_REG_SIZE * ISP_CONTEXT_HW_NUM + ALIGN_PADDING_SIZE)

#define ISP_CFG_MAP_MAX                  100

/* hw_addr for H/W; sw_addr for kernel */
struct regfile_buf_info {
	/*
	 * virtual address of cmd buf for software to
	 * set registers
	 */
	void *sw_addr;

	/*
	 * phys or iova of cmd buf for CFG HW to get
	 * registers
	 */
	unsigned long hw_addr;

	/*
	 * 1:dirty; 0:clean;
	 * used to identify if the cmd buf
	 * is dirty or not. If dirty, need to
	 * flush this cmd_buf into HW regs.
	 */
	uint32_t dirty;
};

struct isp_dev_cfg_info {
	uint32_t bypass;
	uint32_t tm_bypass;
	uint32_t sdw_mode;
	uint32_t num_of_mod;
	uint32_t *isp_cfg_map;
	uint32_t cfg_main_sel;
	uint32_t bp_pre0_pixel_rdy;
	uint32_t bp_pre1_pixel_rdy;
	uint32_t bp_cap0_pixel_rdy;
	uint32_t bp_cap1_pixel_rdy;
	/* 0: cfg trigger start. 1: fmcu trigger start */
	uint32_t pre0_cmd_ready_mode;
	uint32_t pre1_cmd_ready_mode;
	uint32_t cap0_cmd_ready_mode;
	uint32_t cap1_cmd_ready_mode;
	uint32_t tm_set_number;
	uint32_t cap0_th;
	uint32_t cap1_th;
};

struct isp_cfg_buf {
	struct regfile_buf_info reg_buf[CFG_BUF_NUM];
	enum cfg_buf_id cur_buf_id;
};

struct isp_cfg_ops;

struct isp_cfg_ctx_desc {
	struct isp_cfg_buf cfg_buf[ISP_CONTEXT_SW_NUM];
	struct camera_buf ion_pool;
	struct camera_buf ion_pool_cached;
	atomic_t user_cnt;
	atomic_t map_cnt;
	struct isp_cfg_ops *ops;
	struct cam_hw_ops *hw_ops;
	struct cam_hw_info *hw;
};

struct isp_cfg_ops {
	int (*ctx_init)(struct isp_cfg_ctx_desc *cfg_ctx);
	int (*ctx_deinit)(struct isp_cfg_ctx_desc *cfg_ctx);
	int (*ctx_reset)(struct isp_cfg_ctx_desc *cfg_ctx,
			enum isp_context_id ctx_id);
	int (*hw_init)(struct isp_cfg_ctx_desc *cfg_ctx);
	int (*hw_cfg)(struct isp_cfg_ctx_desc *cfg_ctx,
			enum isp_context_id sw_ctx_id,
			enum isp_context_hw_id hw_ctx_id,
			uint32_t fmcu_enable);
	int (*hw_start)(struct isp_cfg_ctx_desc *cfg_ctx,
			enum isp_context_hw_id ctx_id);
};

struct isp_cfg_ctx_desc *isp_cfg_ctx_desc_get(void);
int isp_cfg_ctx_desc_put(struct isp_cfg_ctx_desc *param);
int isp_cfg_ctx_reg_buf_debug_show(void *param);

#endif/* _ISP_CFG_H_ */
