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

#ifndef _DCAM_INT_HEADER_
#define _DCAM_INT_HEADER_

#include <linux/platform_device.h>

#include "dcam_drv.h"

extern int s_dcam_irq[DCAM_MAX_COUNT];/*dts parsed INTC irq no*/
extern spinlock_t dcam_lock[DCAM_MAX_COUNT];

#define DCAM_IRQ_ERR_MASK \
	((1 << DCAM_DCAM_OVF) | (1 << DCAM_CAP_LINE_ERR) | \
	(1 << DCAM_CAP_FRM_ERR) | (1 << DCAM_BPC_MEM_ERR))

#define DCAM_IRQ_LINE_MASK \
	((1 << DCAM_FULL_PATH_TX_DONE) | (1 << DCAM_BIN_PATH_TX_DONE) | \
	(1 << DCAM_AEM_PATH_TX_DONE) | (1 << DCAM_PDAF_PATH_TX_DONE) | \
	(1 << DCAM_VCH2_PATH_TX_DONE) | (1 << DCAM_VCH3_PATH_TX_DONE) | \
	(1 << DCAM_BPC_POS_DONE) | (1 << DCAM_AFM_INTREQ1) | \
	(1 << DCAM_AFL_TX_DONE) | (1 << DCAM_NR3_TX_DONE) | \
	(1 << DCAM_CAP_SOF) | (1 << DCAM_SN_SOF) | (1 << DCAM_SN_EOF) | \
	DCAM_IRQ_ERR_MASK | DCAM_IRQ_ERR_MMU)

#define DCAM_IRQ_ERR_MMU \
	((1 << DCAM_MMU_INT))

enum dcam_irq_id {
	DCAM_SN_SOF = 0,
	DCAM_SN_EOF,
	DCAM_CAP_SOF,/*DCAM2 RESERVED*/
	DCAM_CAP_EOF,/*DCAM2 RESERVED*/
	DCAM_DCAM_OVF,
	DCAM_PREVIEW_SOF,/*DCAM2 RESERVED*/
	DCAM_ISP_ENABLE_PULSE,/*DCAM2 RESERVED*/
	DCAM_FETCH_SOF_INT,/*DCAM2 RESERVED*/
	DCAM_AFL_LAST_SOF,/*DCAM2 RESERVED*/
	DCAM_BPC_MEM_ERR,/*DCAM2 RESERVED*/
	DCAM_CAP_LINE_ERR,
	DCAM_CAP_FRM_ERR,
	DCAM_FULL_PATH_END,/*DCAM2 RESERVED*/
	DCAM_BIN_PATH_END,/*DCAM2 RESERVED*/
	DCAM_AEM_PATH_END,/*DCAM2 RESERVED*/
	DCAM_PDAF_PATH_END,/*DCAM1 VCH_END, DCAM2 RESERVED*/
	DCAM_VCH2_PATH_END,/*DCAM1 RESERVED, DCAM2 RESERVED*/
	DCAM_VCH3_PATH_END,/*DCAM1 RESERVED, DCAM2 RESERVED*/
	DCAM_FULL_PATH_TX_DONE,
	DCAM_BIN_PATH_TX_DONE,/*DCAM2 RESERVED*/
	DCAM_AEM_PATH_TX_DONE,/*DCAM2 RESERVED*/
	DCAM_PDAF_PATH_TX_DONE,/*DCAM1 VCH_TX_DONE, DCAM2 RESERVED*/
	DCAM_VCH2_PATH_TX_DONE,/*DCAM1 RESERVED, DCAM2 RESERVED*/
	DCAM_VCH3_PATH_TX_DONE,/*DCAM1 RESERVED, DCAM2 RESERVED*/
	DCAM_BPC_MAP_DONE,/*DCAM2 DCAM_MMU_INT*/
	DCAM_BPC_POS_DONE,/*DCAM2 RESERVED*/
	DCAM_AFM_INTREQ0,/*DCAM2 RESERVED*/
	DCAM_AFM_INTREQ1,/*DCAM2 RESERVED*/
	DCAM_AFL_TX_DONE,/*DCAM2 RESERVED*/
	DCAM_NR3_TX_DONE,/*DCAM2 RESERVED*/
	DCAM_RESERVED,/*DCAM2 RESERVED*/
	DCAM_MMU_INT,/*DCAM2 RESERVED*/
	DCAM_IRQ_NUMBER,
};

typedef int (*dcam_isr_func) (struct camera_frame *frame, void *u_data);

int sprd_dcam_int_irq_request(enum dcam_id idx, void *param);
void sprd_dcam_int_irq_free(enum dcam_id id, void *param);
int sprd_dcam_int_reg_isr(enum dcam_id idx, enum dcam_irq_id id,
	dcam_isr_func user_func, void *u_data);

#endif
