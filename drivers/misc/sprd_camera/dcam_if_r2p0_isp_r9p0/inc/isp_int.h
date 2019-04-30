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

#ifndef _ISP_INT_HEADER_
#define _ISP_INT_HEADER_

#include <linux/platform_device.h>

#include "isp_drv.h"

extern struct isp_ch_irq s_isp_irq[ISP_MAX_COUNT];
extern struct isp_group s_isp_group;
enum isp_contex {
	ISP_CONTEX_P0 = 0,
	ISP_CONTEX_P1,
	ISP_CONTEX_C0,
	ISP_CONTEX_C1,
	ISP_CONTEX_MAX,
};

enum isp_irq0_id {
	ISP_INT_ISP_ALL_DONE,
	ISP_INT_SHADOW_DONE,
	ISP_INT_DISPATCH_DONE,
	ISP_INT_STORE_DONE_OUT,
	ISP_INT_STORE_DONE_PRE,
	ISP_INT_STORE_DONE_VID,
	ISP_INT_STORE_DONE_VID_SKIP,
	ISP_INT_NR3_ALL_DONE,
	ISP_INT_NR3_SHADOW_DONE,
	ISP_INT_NULL9,
	ISP_INT_NULL10,
	ISP_INT_FMCU_LOAD_DONE,
	ISP_INT_FMCU_CONFIG_DONE,
	ISP_INT_FMCU_SHADOW_DONE,
	ISP_INT_FMCU_CMD_X,
	ISP_INT_FMCU_TIMEOUT,
	ISP_INT_FMCU_CMD_ERROR,
	ISP_INT_FMCU_STOP_DONE,
	ISP_INT_RESERVED,
	ISP_INT_RESERVED1,
	ISP_INT_RESERVED2,
	ISP_INT_HIST_DONE,
	ISP_INT_NUMBER0,
};

enum isp_mmu_irq {
	RAW_OUT_OF_RNGE_R,
	RAW_OUT_OF_RNGE_W,
	RAW_UNSECURE_R,
	RAW_UNSECURE_W,
	RAW_INVALID_PAGE_R,
	RAW_INVALID_PAGE_W,
	RAW_INVALID_ID_R,
	RAW_INVALID_ID_W,
	MMU_INT_NUMBER3,
};

#define ISP_INT_LINE_MASK_P0 \
	((1 << ISP_INT_SHADOW_DONE) | \
	(1 << ISP_INT_STORE_DONE_PRE) | \
	(1 << ISP_INT_STORE_DONE_VID) | \
	(1 << ISP_INT_ISP_ALL_DONE) | \
	(1 << ISP_INT_FMCU_CONFIG_DONE) | \
	(1 << ISP_INT_HIST_DONE))

#define ISP_INT_LINE_MASK_P1 \
	((1 << ISP_INT_SHADOW_DONE) | \
	(1 << ISP_INT_STORE_DONE_PRE) | \
	(1 << ISP_INT_STORE_DONE_VID) | \
	(1 << ISP_INT_ISP_ALL_DONE) | \
	(1 << ISP_INT_FMCU_CONFIG_DONE) | \
	(1 << ISP_INT_HIST_DONE))

#define ISP_INT_LINE_MASK_C0 \
	((1 << ISP_INT_SHADOW_DONE) | \
	(1 << ISP_INT_STORE_DONE_PRE) | \
	(1 << ISP_INT_ISP_ALL_DONE) | \
	(1 << ISP_INT_FMCU_CONFIG_DONE))

#define ISP_INT_LINE_MASK_C1 \
	((1 << ISP_INT_SHADOW_DONE) | \
	(1 << ISP_INT_STORE_DONE_PRE) | \
	(1 << ISP_INT_ISP_ALL_DONE)  | \
	(1 << ISP_INT_FMCU_CONFIG_DONE))

#define ISP_INT_LINE_MASK_MMU \
	((1 << RAW_OUT_OF_RNGE_R) | \
	(1 << RAW_OUT_OF_RNGE_W) | \
	(1 << RAW_UNSECURE_R) | \
	(1 << RAW_UNSECURE_W) | \
	(1 << RAW_INVALID_PAGE_R) | \
	(1 << RAW_INVALID_PAGE_W) | \
	(1 << RAW_INVALID_ID_R) | \
	(1 << RAW_INVALID_ID_W))

#define ISP_IRQ_ERR_MASK_P0 \
		((1 << ISP_INT_FMCU_CMD_ERROR))

#define ISP_IRQ_ERR_MASK_P1 \
		((1 << ISP_INT_FMCU_CMD_ERROR))

#define ISP_IRQ_ERR_MASK_C0 \
		((1 << ISP_INT_FMCU_CMD_ERROR))

#define ISP_IRQ_ERR_MASK_C1 \
		((1 << ISP_INT_FMCU_CMD_ERROR))

int sprd_isp_int_irq_callback(enum isp_id id,
	enum isp_irq_id irq_id, isp_isr_func user_func, void *user_data);
int sprd_isp_int_irq_request(struct device *p_dev,
	struct isp_ch_irq *irq, struct isp_pipe_dev *ispdev);
int sprd_isp_int_irq_free(struct isp_ch_irq *irq,
	struct isp_pipe_dev *ispdev);
void sprd_isp_int_path_sof(enum isp_id idx, enum isp_path_index,
	void *isp_handle);

#endif
