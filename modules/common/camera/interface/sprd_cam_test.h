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

#ifndef _SPRD_CAM_TEST_H_
#define _SPRD_CAM_TEST_H_
#define DRV_PATH_NUM 2

#include "sprd_img.h"

#define ISP_HW_MAX_COUNT (4)

typedef enum ispt_fmt {
	ISPT_FMT_UYVY = 0,
	ISPT_FMT_YUV422_2FRAME,
	ISPT_FMT_YUV420_2FRAME,
	ISPT_FMT_YVU420_2FRAME,
	ISPT_FMT_YVU422_2FRAME,
	ISPT_FMT_YUV422_3FRAME,
	ISPT_FMT_YUV420_3FRAME,
} ispt_fmt_id;

enum camt_cmd {
	CAMT_CMD_INIT = 0,
	CAMT_CMD_START,
	CAMT_CMD_STOP,
	CAMT_CMD_DEINIT,
	CAMT_CMD_MAX
};

enum camt_chip {
	CAMT_CHIP_DCAM0 = 0,
	CAMT_CHIP_DCAM1,
	CAMT_CHIP_DCAM2,
	CAMT_CHIP_DCAM_LITE0,
	CAMT_CHIP_DCAM_LITE1,
	CAMT_CHIP_ISP0,
	CAMT_CHIP_ISP1,
	CAMT_CHIP_MAX
};

enum camt_dcam_path_id {
	CAMT_DCAM_PATH_FULL = 0,
	CAMT_DCAM_PATH_BIN,
	CAMT_DCAM_PATH_PDAF,
	CAMT_DCAM_PATH_VCH2,
	CAMT_DCAM_PATH_VCH3,
	CAMT_DCAM_PATH_AEM,
	CAMT_DCAM_PATH_AFM,
	CAMT_DCAM_PATH_AFL,
	CAMT_DCAM_PATH_HIST,
	CAMT_DCAM_PATH_3DNR,
	CAMT_DCAM_PATH_BPC,
	CAMT_DCAM_PATH_LSCM,
	CAMT_DCAM_PATH_MAX,
};

struct camt_isp_info {
	int set;

	unsigned int isp_id;
	unsigned int isp_cid;

	struct sprd_img_size input_size;
	struct sprd_img_rect crop_rect;
	struct sprd_img_size output_size;

	/* buffer desc */
	int inbuf_fd;
	int outbuf_fd;
};

struct img_store_addr {
	unsigned long addr_ch0;
	unsigned long addr_ch1;
	unsigned long addr_ch2;
};

struct camt_info {
	enum camt_cmd cmd;
	enum camt_chip chip;
	unsigned int path_id[DRV_PATH_NUM];
	/* 0: online; 1: offline */
	unsigned int test_mode;
	struct camt_isp_info isp_info[ISP_HW_MAX_COUNT];

	/* channel desc  */
	unsigned int in_fmt;  /* forcc */
	unsigned int out_fmt;  /* forcc */
	uint32_t bayer_mode;

	struct sprd_img_size input_size;
	struct sprd_img_rect crop_rect;
	struct sprd_img_size output_size;

	unsigned int is_4in1;
	unsigned int deci;
	unsigned int skip_num;
	unsigned int is_loose;
	unsigned int pitch;
	struct sprd_img_endian endian;

	/* buffer desc */
	int inbuf_fd;
	int outbuf_fd[DRV_PATH_NUM];
};

#endif

