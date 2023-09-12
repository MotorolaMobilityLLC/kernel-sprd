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

#ifndef _DCAM_INT_H_
#define _DCAM_INT_H_

#include <linux/bitops.h>
#include <linux/device.h>

#include "cam_types.h"

/* interrupt bits for DCAM0 or DCAM1 */
enum {
	DCAM_SENSOR_SOF         = 0,
	DCAM_SENSOR_EOF         = 1,
	DCAM_CAP_SOF            = 2,
	DCAM_CAP_EOF            = 3,

	DCAM_DCAM_OVF           = 4,
	DCAM_CAP_LINE_ERR       = 5,//DCAM_PREVIEW_SOF
	DCAM_CAP_FRM_ERR        = 6,//DCAM_AFL_LAST_SOF
	DCAM_FETCH_SOF_INT      = 7,

	DCAM_ISP_ENABLE_PULSE   = 8,
	DCAM_PREVIEW_SOF        = 9,//DCAM_BPC_MEM_ERR
	DCAM_PDAF_SOF           = 10,
	DCAM_AEM_SOF            = 11,

	DCAM_HIST_SOF           = 12,//DCAM_FULL_PATH_END
	DCAM_AFL_SOF            = 13,//DCAM_BIN_PATH_END
	DCAM_AFM_SOF            = 14,
	DCAM_LSCM_SOF           = 15,
	DCAM_FULL_PATH_TX_DONE  = 16,//DCAM_FULL_PATH_TX_DONE
	DCAM_PREV_PATH_TX_DONE  = 17,//DCAM_PREV_PATH_TX_DONE
	DCAM_PDAF_PATH_TX_DONE  = 18,//DCAM_PDAF_PATH_TX_DONE
	DCAM_VCH2_PATH_TX_DONE  = 19,//DCAM_VCH2_PATH_TX_DONE

	DCAM_VCH3_PATH_TX_DONE  = 20,//DCAM_VCH3_PATH_TX_DONE
	DCAM_AEM_TX_DONE        = 21,//DCAM_AEM_TX_DONE
	DCAM_HIST_TX_DONE       = 22,//DCAM_HIST_TX_DONE
	DCAM_AFL_TX_DONE        = 23,//DCAM_AFL_TX_DONE

	DCAM_BPC_MAP_DONE       = 24,//DCAM_BPC_MAP_DONE
	DCAM_BPC_POS_DONE       = 25,//DCAM_BPC_POS_DONE
	DCAM_AFM_INTREQ0        = 26,//DCAM_AFM_INTREQ0
	DCAM_AFM_INTREQ1        = 27,//DCAM_AFM_INTREQ1

	DCAM_NR3_TX_DONE        = 28,//DCAM_NR3_TX_DONE
	DCAM_LSCM_TX_DONE       = 29,
	DCAM_GTM_TX_DONE        = 30,
	DCAM_MMU_INT            = 31,//DCAM_MMU_INT

	DCAM_IRQ_NUMBER         = 32,//DCAM_IRQ_NUMBER
};

/* interrupt bits for DCAM2 */
enum {
	DCAM2_SENSOR_SOF        = 0,
	DCAM2_SENSOR_EOF        = 1,
	/* reserved */
	/* reserved */

	DCAM2_DCAM_OVF          = 4,
	/* reserved */
	/* reserved */
	/* reserved */

	/* reserved */
	/* reserved */
	DCAM2_CAP_LINE_ERR      = 10,
	DCAM2_CAP_FRM_ERR       = 11,

	DCAM2_FULL_PATH0_END    = 12,
	DCAM2_FULL_PATH1_END    = 13,
	/* reserved */
	/* reserved */

	/* reserved */
	/* reserved */
	DCAM2_FULL_PATH_TX_DONE = 18,
	/* reserved */

	/* reserved */
	/* reserved */
	/* reserved */
	/* reserved */

	DCAM2_MMU_INT           = 24,
	/* reserved */
	/* reserved */
	/* reserved */

	/* reserved */
	/* reserved */
	/* reserved */
	/* reserved */
};

/*
 * error bits
 * same in DCAM0/1/2
 */
#define DCAMINT_ALL_ERROR                         \
	(BIT(DCAM_DCAM_OVF) |                     \
	BIT(DCAM_CAP_LINE_ERR) |                  \
	BIT(DCAM_CAP_FRM_ERR) |                   \
	BIT(DCAM_MMU_INT))

/*
 * fatal error bits
 */
#define DCAMINT_FATAL_ERROR                       \
	(BIT(DCAM_DCAM_OVF) |                     \
	BIT(DCAM_CAP_LINE_ERR) |                  \
	BIT(DCAM_CAP_FRM_ERR))

/*
 * SOF bits
 * some bits is reserved in DCAM2
 */
#define DCAMINT_ALL_SOF                           \
	(BIT(DCAM_SENSOR_SOF) |                   \
	BIT(DCAM_CAP_SOF) |                       \
	BIT(DCAM_PREVIEW_SOF))

/*
 * TX DONE bits
 * some bits is reserved in DCAM2
 * NOTE: BIT(24) is mmu_int in DCAM2
 */
#define DCAMINT_ALL_TX_DONE                       \
	(BIT(DCAM_FULL_PATH_TX_DONE) |            \
	BIT(DCAM_PREV_PATH_TX_DONE) |             \
	BIT(DCAM_PDAF_PATH_TX_DONE) |             \
	BIT(DCAM_VCH2_PATH_TX_DONE) |             \
	BIT(DCAM_VCH3_PATH_TX_DONE) |             \
	BIT(DCAM_AEM_TX_DONE) |                   \
	BIT(DCAM_HIST_TX_DONE) |                  \
	BIT(DCAM_AFL_TX_DONE) |                   \
	BIT(DCAM_BPC_MAP_DONE) |                  \
	BIT(DCAM_BPC_POS_DONE) |                  \
	BIT(DCAM_AFM_INTREQ0) |                   \
	BIT(DCAM_AFM_INTREQ1) |                   \
	BIT(DCAM_LSCM_TX_DONE) |                  \
	BIT(DCAM_NR3_TX_DONE) |                   \
	BIT(DCAM_GTM_TX_DONE))

/*
 * all currently useful bits on irq line
 */
#define DCAMINT_IRQ_LINE_MASK                     \
	(DCAMINT_ALL_ERROR | DCAMINT_ALL_TX_DONE |\
	BIT(DCAM_CAP_SOF) |                       \
	BIT(DCAM_SENSOR_SOF) |                    \
	BIT(DCAM_SENSOR_EOF) |                    \
	BIT(DCAM_PREVIEW_SOF))

/* enabled interrupt source in normal scene */
#define DCAMINT_IRQ_LINE_EN_NORMAL                \
	(DCAMINT_ALL_ERROR | DCAMINT_ALL_TX_DONE |\
	BIT(DCAM_CAP_SOF) |                       \
	BIT(DCAM_SENSOR_EOF))

/*
 * enabled interrupt source in slow motion scene
 *
 * Note: this one is deprecated as we have a design defect in current DCAM IP.
 * The address written into slow motion register in DCAM_PREVIEW_SOF cannot be
 * applied by hardware because DCAM will only shadow registers at the first one
 * of four frames in slow motion mode. In order to make DCAM_PREVIEW_SOF work,
 * software has to set auto copy at the last DCAM_CAP_SOF of four frames. So we
 * just use DCAM_CAP_SOF to do all the work.
 */
#define DCAMINT_IRQ_LINE_EN_SLM                   \
	(DCAMINT_ALL_ERROR | DCAMINT_ALL_TX_DONE |\
	BIT(DCAM_PREVIEW_SOF))

int dcam_int_irq_request(struct device *pdev, int irq, void *param);
void dcam_int_irq_free(struct device *pdev, void *param);

void dcam_int_tracker_reset(uint32_t idx);
void dcam_int_tracker_dump(uint32_t idx);
int dcamint_interruption_proc(void *dcam_handle);

#endif
