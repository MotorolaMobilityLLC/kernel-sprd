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

#ifndef _ISP_INT_H_
#define _ISP_INT_H_

#include <linux/platform_device.h>

#include "cam_types.h"

enum isp_irq_id {
	ISP_INT_ISP_ALL_DONE,
	ISP_INT_SHADOW_DONE,
	ISP_INT_DISPATCH_DONE,
	ISP_INT_NULL3,

	ISP_INT_STORE_DONE_PRE,
	ISP_INT_STORE_DONE_VID,
	ISP_INT_STORE_DONE_VID_SKIP,
	ISP_INT_NR3_ALL_DONE,

	ISP_INT_NR3_SHADOW_DONE,
	ISP_INT_STORE_DONE_THUMBNAIL,
	ISP_INT_LTMHISTS_DONE,
	ISP_INT_FMCU_LOAD_DONE,

	ISP_INT_FMCU_CONFIG_DONE,
	ISP_INT_FMCU_SHADOW_DONE,
	ISP_INT_FMCU_CMD_X,
	ISP_INT_FMCU_TIMEOUT,

	ISP_INT_FMCU_CMD_ERROR,
	ISP_INT_FMCU_STOP_DONE,
	ISP_INT_NULL18,
	ISP_INT_FBD_FETCH_ERR,

	ISP_INT_NR3_FBD_ERR,
	ISP_INT_FBC_ERR,
	ISP_INT_RGB_LTMHISTS_DONE,
	ISP_INT_AXI_TIMEOUT,

	ISP_INT_REC_ST_DONE,
	ISP_INT_REC_ST_SHADOW_DONE,
	ISP_INT_REC_DISPATCH_DONE,
	ISP_INT_REC_TRANS_DONE,

	ISP_INT_REC_FIFO_ERR,
	ISP_INT_REC_ALL_DONE,
	ISP_INT_CFG_ERR,
	ISP_INT_MMU_ERR,
};
#define ISP_INT_FMCU_STORE_DONE              ISP_INT_FMCU_CONFIG_DONE

#define ISP_INT_LINE_MASK_ERR                \
	((1 << ISP_INT_FMCU_TIMEOUT) |       \
	(1 << ISP_INT_FMCU_CMD_ERROR) |      \
	(1 << ISP_INT_FBD_FETCH_ERR) |       \
	(1 << ISP_INT_NR3_FBD_ERR) |         \
	(1 << ISP_INT_AXI_TIMEOUT) |         \
	(1 << ISP_INT_REC_FIFO_ERR) |         \
	(1 << ISP_INT_FBC_ERR))

#define ISP_INT_LINE_MASK_MMU                (1 << ISP_INT_MMU_ERR)

#define ISP_INT_LINE_MASK                    \
	((1 << ISP_INT_ISP_ALL_DONE) |       \
	(1 << ISP_INT_SHADOW_DONE) |         \
	(1 << ISP_INT_DISPATCH_DONE) |       \
	(1 << ISP_INT_STORE_DONE_PRE) |      \
	(1 << ISP_INT_STORE_DONE_VID) |      \
	(1 << ISP_INT_STORE_DONE_VID_SKIP) | \
	(1 << ISP_INT_NR3_ALL_DONE) |        \
	(1 << ISP_INT_NR3_SHADOW_DONE) |     \
	(1 << ISP_INT_STORE_DONE_THUMBNAIL) |\
	(1 << ISP_INT_LTMHISTS_DONE) |   \
	(1 << ISP_INT_FMCU_LOAD_DONE) |      \
	(1 << ISP_INT_FMCU_CMD_X) |          \
	(1 << ISP_INT_FMCU_SHADOW_DONE) |    \
	(1 << ISP_INT_FMCU_STORE_DONE) |     \
	(1 << ISP_INT_RGB_LTMHISTS_DONE))

int isp_int_isp_irq_cnt_reset(int ctx_id);
int isp_int_isp_irq_cnt_trace(int ctx_id);

int isp_int_isp_irq_sw_cnt_reset(int ctx_id);
int isp_int_isp_irq_sw_cnt_trace(int ctx_id);

int isp_int_irq_request(struct device *p_dev,
	uint32_t *irq_no, void *isp_handle);
int isp_int_irq_free(struct device *p_dev, void *isp_handle);
#endif
