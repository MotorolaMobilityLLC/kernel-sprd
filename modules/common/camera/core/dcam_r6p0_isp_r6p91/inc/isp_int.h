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

/* int 0*/
enum isp_irq0_id {
	ISP_INT_ISP_ALL_DONE = 0,
	ISP_INT_SHADOW_DONE = 1,
	ISP_INT_STORE_DONE = 2,
	ISP_INT_ISP_STRAT = 3,
	ISP_INT_DCAM_BUF_FULL = 4,
	ISP_INT_STORE_BUF_FULL = 5,
	ISP_INT_ISP2DCAM_AFIFO_FULL = 6,
	ISP_INT_LSC_LOAD = 7,
	ISP_INT_AEM_START = 8,
	ISP_INT_AEM_DONE = 9,
	ISP_INT_AFM_RGB_START = 14,
	ISP_INT_AFM_RGB_DONE = 15,
	ISP_INT_BINNING_DONE = 18,
	ISP_INT_BINNING_START = 19,
	ISP_INT_AFL_START = 20,
	ISP_INT_AFL_DONE = 21,
	ISP_INT_DCAM_SOF = 22,
	ISP_INT_DCAM_EOF = 23,
	ISP_INT_HIST_START = 24,
	ISP_INT_HIST_DONE = 25,
	ISP_INT_HIST2_START = 26,
	ISP_INT_HIST2_DONE = 27,
	ISP_INT_HIST2_WIN0_DONE = 28,
	ISP_INT_HIST2_WIN1_DONE = 29,
	ISP_INT_HIST2_WIN2_DONE = 30,
	ISP_INT_HIST2_WIN3_DONE = 31,
};

/* int 1*/
enum isp_irq1_id {
	ISP_INT_DISPATCH_BUF_FULL = 25,
	ISP_INT_AWBM_ERR = 26,
	ISP_INT_BINNING_ERR1 = 27,
	ISP_INT_BINNING_ERR0 = 28,
	ISP_INT_BPC_ERR2 = 29,
	ISP_INT_BPC_ERR1 = 30,
	ISP_INT_BPC_ERR0 = 31,
};

/* int 2*/
enum isp_irq2_id {
	ISP_INT_AFM_RGB_WIN0 = 0,
	ISP_INT_AFM_RGB_WIN1 = 1,
	ISP_INT_AFM_RGB_WIN2 = 2,
	ISP_INT_AFM_RGB_WIN3 = 3,
	ISP_INT_AFM_RGB_WIN4 = 4,
	ISP_INT_AFM_RGB_WIN5 = 5,
	ISP_INT_AFM_RGB_WIN6 = 6,
	ISP_INT_AFM_RGB_WIN7 = 7,
	ISP_INT_AFM_RGB_WIN8 = 8,
	ISP_INT_AFM_RGB_WIN9 = 9,
	ISP_INT_AFL_NEW_DDR_START = 25,
	ISP_INT_AFL_NEW_DDR_DONE = 26,
	ISP_INT_AFL_NEW_SRAM_START = 27,
	ISP_INT_AFL_NEW_SRAM_DONE = 28,
};

/* int 3*/
enum isp_irq3_id {
	ISP_INT_YUV_DONE = 0,
	ISP_INT_YUV_POSTCDN_DONE = 1,
	ISP_INT_YUV_CDN_DONE = 2,
	ISP_INT_YUV_EE_DONE = 3,
	ISP_INT_YUV_NLM_DONE = 4,
	ISP_INT_YUV_PRFUV_DONE = 5,
	ISP_INT_YUV_PRFY_DONE = 6,
	ISP_INT_YUV_START = 7,
	ISP_INT_FRGB_CCE_DONE = 8,
	ISP_INT_FRGB_CCE_START = 9,
	ISP_INT_FRGB_DONE = 10,
	ISP_INT_FRGB_START = 11,
	ISP_INT_FRGB_CFCE_DONE = 12,
	ISP_INT_FRGB_CFCE_START = 13,
	ISP_INT_RRGB_DONE = 14,
	ISP_INT_RRGB_BDN_DONE = 15,
	ISP_INT_RRGB_PWD_DONE = 16,
	ISP_INT_RRGB_BPC_DONE = 17,
	ISP_INT_RRGB_LENS_DONE = 18,
	ISP_INT_RRGB_NLM_DONE = 19,
	ISP_INT_RRGB_START = 20,
};

enum isp_mmu_irq {
	RAW_OUT_OF_RNGE_R = 0,
	RAW_OUT_OF_RNGE_W = 1,
	RAW_UNSECURE_R = 2,
	RAW_UNSECURE_W = 3,
	RAW_INVALID_PAGE_R = 4,
	RAW_INVALID_PAGE_W = 5,
	RAW_INVALID_ID_R = 6,
	RAW_INVALID_ID_W = 7,
};

int isp_irq_callback(enum isp_irq_id irq_id, isp_isr_func user_func,
			void *user_data);
int isp_irq_request(struct device *p_dev, unsigned int irq,
	struct isp_pipe_dev *ispdev);
int isp_irq_free(unsigned int irq, struct isp_pipe_dev *ispdev);
int isp_set_next_statis_buf(struct isp_statis_module *module,
			    enum isp_3a_block_id block_index);

#endif
