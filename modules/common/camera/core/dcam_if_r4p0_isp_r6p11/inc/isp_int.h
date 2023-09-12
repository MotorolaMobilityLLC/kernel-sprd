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

#ifndef _ISP_INT_HEADER_
#define _ISP_INT_HEADER_

#include <linux/platform_device.h>

#include "isp_drv.h"
#include "isp_cfg.h"

#define NUM_MOST_RECENT_STATUS	100
#define INT_NUM_PER_SET 32 /* 32bit per int reg set */

extern int report_isp_err(void *param);

struct irq_reg {
	uint32_t irq_status;
	uint32_t offset[3];
	uint32_t bits_idx_max;
	uint32_t msk_bmap[2];
	int *order;
	int pivot;
	uint32_t recent[NUM_MOST_RECENT_STATUS];
};

enum _INT_REG_OFF_NUM {
	INT_REG_OFF_EN = 0,
	INT_REG_OFF_CLR,
	INT_REG_OFF_INT,
};

enum isp_int_set {
	INT_REG_SET_0,
	INT_REG_SET_1,
	INT_REG_SET_2,
	INT_REG_SET_3,
	INT_REG_SETS
};

/* preview & capture int */
enum isp_irq0_id {
	ISP_INT_ISP_ALL_DONE,
	ISP_INT_SHADOW_DONE,
	ISP_INT_DISPATCH_SLICE_DONE,
	ISP_INT_ISP_START,
	ISP_INT_FETCH_RAW_DONE,
	ISP_INT_NULL0_5,
	ISP_INT_NULL0_6,
	ISP_INT_LENS_LOAD,
	ISP_INT_AEM_START,
	ISP_INT_AEM_SHADOW_DONE,
	ISP_INT_AEM_DONE,
	ISP_INT_AEM_ERROR,
	ISP_INT_AFL_START,
	ISP_INT_AFL_SHADOW_DONE,
	ISP_INT_AFL_DONE,
	ISP_INT_AFL_ERROR,
	ISP_INT_BINNING_START,
	ISP_INT_BINNING_SHADOW_DONE,
	ISP_INT_BINNING_DONE,
	ISP_INT_BINNING_ERROR,
	ISP_INT_NULL0_20,
	ISP_INT_NULL0_21,
	ISP_INT_NULL0_22,
	ISP_INT_NULL0_23,
	ISP_INT_NULL0_24,
	ISP_INT_NULL0_25,
	ISP_INT_NULL0_26,
	ISP_INT_NULL0_27,
	ISP_INT_DCAM_SOF,
	ISP_INT_DCAM_EOF,
	ISP_INT_STORE_DONE,
	ISP_INT_AFL_LAST_SOF,
	ISP_INT_NUMBER0,
};

enum isp_irq1_id {
	ISP_INT_STORE_DONE_OUT,
	ISP_INT_SHADOW_DONE_OUT,
	ISP_INT_STORE_DONE_CAP_PRE,
	ISP_INT_SHADOW_DONE_CAP_PRE,
	ISP_INT_STORE_DONE_VID,
	ISP_INT_SHADOW_DONE_VID,
	ISP_INT_NULL1_6,
	ISP_INT_NULL1_7,
	ISP_INT_NULL1_8,
	ISP_INT_NULL1_9,
	ISP_INT_NULL1_10,
	ISP_INT_NULL1_11,
	ISP_INT_NULL1_12,
	ISP_INT_NULL1_13,
	ISP_INT_NULL1_14,
	ISP_INT_NULL1_15,
	ISP_INT_NULL1_16,
	ISP_INT_NULL1_17,
	ISP_INT_HIST2_SHADOW_DONE,
	ISP_INT_HIST_SHADOW_DONE,
	ISP_INT_HIST_START,
	ISP_INT_HIST_DONE,
	ISP_INT_HIST2_START,
	ISP_INT_HIST2_DONE,
	ISP_INT_BPC_STORE_MEM_DONE,
	ISP_INT_BPC_STORE_ERROR,
	ISP_INT_BPC_ERR2,
	ISP_INT_BPC_ERR1,
	ISP_INT_BPC_ERR0,
	ISP_INT_BPC_STORE_NOT_EMPTY_ERROR,
	ISP_INT_CFG_ERR,
	ISP_INT_NULL1_31,
	ISP_INT_NUMBER1,
};

enum isp_irq2_id {
	ISP_INT_AFM_RGB_Win0,
	ISP_INT_AFM_RGB_Win1,
	ISP_INT_AFM_RGB_Win2,
	ISP_INT_AFM_RGB_Win3,
	ISP_INT_AFM_RGB_Win4,
	ISP_INT_AFM_RGB_Win5,
	ISP_INT_AFM_RGB_Win6,
	ISP_INT_AFM_RGB_Win7,
	ISP_INT_AFM_RGB_Win8,
	ISP_INT_AFM_RGB_Win9,
	ISP_INT_AFM_RGB_START,
	ISP_INT_AFM_RGB_DONE,
	ISP_INT_AFM_RGB_SHADOW_DONE,
	ISP_INT_NULL2_13,
	ISP_INT_NULL2_14,
	ISP_INT_NULL2_15,
	ISP_INT_NULL2_16,
	ISP_INT_NULL2_17,
	ISP_INT_NULL2_18,
	ISP_INT_NULL2_19,
	ISP_INT_NULL2_20,
	ISP_INT_FMCU_LOAD_DONE,
	ISP_INT_FMCU_CONFIG_DONE,
	ISP_INT_FMCU_SHADOW_DONE,
	ISP_INT_FMCU_CMD_X,
	ISP_INT_FMCU_TIMEOUT,
	ISP_INT_FMCU_CMD_ERROR,
	ISP_INT_FMCU_STOP_DONE,
	ISP_INT_NULL2_28,
	ISP_INT_NULL2_29,
	ISP_INT_NULL2_30,
	ISP_INT_NULL2_31,
	ISP_INT_NUMBER2,
};

enum isp_irq3_id {
	ISP_INT_YUV_DONE,
	ISP_INT_YUV_START,
	ISP_INT_FRGB_DONE,
	ISP_INT_FRGB_START,
	ISP_INT_RRGB_DONE,
	ISP_INT_RRGB_START,
	ISP_INT_NUMBER3,
};

#define ISP_IRQ_PRE_ERR_MASK0 \
	( \
	(1 << ISP_INT_AFL_ERROR) | \
	(1 << ISP_INT_BINNING_ERROR) | \
	0)
#define ISP_MOD_PRE_EN0_MASK \
	( \
	(1 << ISP_INT_ISP_ALL_DONE) | \
	(1 << ISP_INT_AFL_START) | \
	(1 << ISP_INT_AFL_DONE) | \
	(1 << ISP_INT_BINNING_START) | \
	(1 << ISP_INT_BINNING_DONE) | \
	ISP_IRQ_PRE_ERR_MASK0)

#define ISP_IRQ_PRE_ERR_MASK1 \
	( \
	(1 << ISP_INT_BPC_STORE_ERROR) | \
	(1 << ISP_INT_BPC_ERR2) | \
	(1 << ISP_INT_BPC_ERR1) | \
	(1 << ISP_INT_BPC_ERR0) | \
	(1 << ISP_INT_BPC_STORE_NOT_EMPTY_ERROR) | \
	(1 << ISP_INT_CFG_ERR) | \
	0)
#define ISP_MOD_PRE_EN1_MASK \
	( \
	(1 << ISP_INT_STORE_DONE_CAP_PRE) | \
	(1 << ISP_INT_SHADOW_DONE_CAP_PRE) | \
	(1 << ISP_INT_STORE_DONE_VID) | \
	(1 << ISP_INT_SHADOW_DONE_VID) | \
	(1 << ISP_INT_HIST_START) | \
	(1 << ISP_INT_HIST_DONE) | \
	ISP_IRQ_PRE_ERR_MASK1)

#define ISP_IRQ_PRE_ERR_MASK2 \
	( \
	0)
#define ISP_MOD_PRE_EN2_MASK \
	( \
	(1 << ISP_INT_AFM_RGB_START) | \
	(1 << ISP_INT_AFM_RGB_DONE) | \
	ISP_IRQ_PRE_ERR_MASK2)

#define ISP_IRQ_PRE_ERR_MASK3 \
	( \
	0)
#define ISP_MOD_PRE_EN3_MASK \
	( \
	ISP_IRQ_PRE_ERR_MASK3)

#define ISP_IRQ_CAP_ERR_MASK0 \
	( \
	0)
#define ISP_MOD_CAP_EN0_MASK \
	( \
	ISP_IRQ_CAP_ERR_MASK0)

#define ISP_IRQ_CAP_ERR_MASK1 \
	( \
	(1 << ISP_INT_BPC_STORE_ERROR) | \
	(1 << ISP_INT_BPC_ERR2) | \
	(1 << ISP_INT_BPC_ERR1) | \
	(1 << ISP_INT_BPC_ERR0) | \
	(1 << ISP_INT_BPC_STORE_NOT_EMPTY_ERROR) | \
	(1 << ISP_INT_CFG_ERR) | \
	0)
#define ISP_MOD_CAP_EN1_MASK \
	( \
	ISP_IRQ_CAP_ERR_MASK1)

#define ISP_IRQ_CAP_ERR_MASK2 \
	( \
	(1 << ISP_INT_FMCU_CMD_ERROR) | \
	0)
#define ISP_MOD_CAP_EN2_MASK \
	( \
	(1 << ISP_INT_FMCU_LOAD_DONE) | \
	(1 << ISP_INT_FMCU_CONFIG_DONE) | \
	(1 << ISP_INT_FMCU_CMD_X) | \
	(1 << ISP_INT_FMCU_TIMEOUT) | \
	ISP_IRQ_CAP_ERR_MASK2)

#define ISP_IRQ_CAP_ERR_MASK3 \
	( \
	0)
#define ISP_MOD_CAP_EN3_MASK \
	( \
	ISP_IRQ_CAP_ERR_MASK3)

#define ISP_3A_INT0_MASK \
	( \
	(1 << ISP_INT_AEM_START) | \
	(1 << ISP_INT_AEM_SHADOW_DONE) | \
	(1 << ISP_INT_AEM_DONE) | \
	(1 << ISP_INT_AEM_ERROR) | \
	(1 << ISP_INT_AFL_START) | \
	(1 << ISP_INT_AFL_SHADOW_DONE) | \
	(1 << ISP_INT_AFL_DONE) | \
	(1 << ISP_INT_AFL_ERROR) | \
	(1 << ISP_INT_BINNING_START) | \
	(1 << ISP_INT_BINNING_SHADOW_DONE) | \
	(1 << ISP_INT_BINNING_DONE) | \
	(1 << ISP_INT_BINNING_ERROR) | \
	0)

#define ISP_AF_INT_MASK \
	( \
	(1 << ISP_INT_AFM_RGB_Win0) | \
	(1 << ISP_INT_AFM_RGB_Win1) | \
	(1 << ISP_INT_AFM_RGB_Win2) | \
	(1 << ISP_INT_AFM_RGB_Win3) | \
	(1 << ISP_INT_AFM_RGB_Win4) | \
	(1 << ISP_INT_AFM_RGB_Win5) | \
	(1 << ISP_INT_AFM_RGB_Win6) | \
	(1 << ISP_INT_AFM_RGB_Win7) | \
	(1 << ISP_INT_AFM_RGB_Win8) | \
	(1 << ISP_INT_AFM_RGB_Win9) | \
	(1 << ISP_INT_AFM_RGB_START) | \
	(1 << ISP_INT_AFM_RGB_DONE) | \
	(1 << ISP_INT_AFM_RGB_SHADOW_DONE) | \
	0)

extern struct irq_reg irq_sets[INT_REG_SETS];
extern struct isp_ch_irq s_isp_irq[ISP_MAX_COUNT];

int isp_irq_callback(enum isp_id iid, enum isp_irq_id irq_id,
	isp_isr_func user_func, void *user_data);
int isp_irq_request(struct device *p_dev, struct isp_ch_irq *irq,
	struct isp_pipe_dev *ispdev);
int isp_irq_free(struct isp_ch_irq *irq, struct isp_pipe_dev *ispdev);
int isp_set_next_statis_buf(uint32_t com_idx,
				struct isp_statis_module *module,
			    enum isp_3a_block_id block_index);
void isp_irq_ctrl(struct isp_pipe_dev *dev, bool enable);
#endif
