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

#ifndef _CAM_DUMP_H_
#define _CAM_DUMP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>

#include "cam_queue.h"
#include "cam_types.h"
#include "dcam_core.h"

#define CAMERA_DUMP_PATH "/data/ylog/"
/* will create thread in user to read raw buffer*/
#define BYTE_PER_ONCE                   4096
#define AFBC_PADDING_W_YUV420_scaler    32
#define AFBC_PADDING_H_YUV420_scaler    8
#define AFBC_VERSION                    5
#define AFBC_FILEHEADER_SIZE            32
#define DUMP_Q_LEN                      50
#define DUMP_RAW_BUF_NUM                10
#define DCAM_MID_BUF                    20

enum dump_mode {
	DUMP_DISABLE = 0,
	DUMP_PATH_BOTH,
	DUMP_PATH_FULL,
	DUMP_PATH_BIN,
	DUMP_ISP_PYR_REC,
	DUMP_ISP_PYR_DEC,
	DUMP_DCAM_PDAF,
	DUMP_PATH_RAW_BIN,
	DUMP_DCAM_OFFLINE,
	DUMP_CAM_MAX
};

enum dump_cfg {
	DUMP_CFG_OUT_FMT = 0,
	DUMP_CFG_IS_PACK,
	DUMP_CFG_PACK_BITS,
	DUMP_CFG_OUT_BITS,
	DUMP_CFG_PYR_LAYER_NUM,
	DUMP_CFG_PYR_START_LAYER,
	DUMP_CFG_PDAF_TYPE,
	DUMP_CFG_MAX
};

enum dump_afbc_format {
	DUMP_AFBC_Y_UV = 1,
	DUMP_AFBC_Y_VU = 2,
	DUMP_AFBC_FORMAT_MAX
};

struct cam_dump_fbc_header {
    uint8_t yuv_mirror_en;
    uint8_t yuv_format;
    uint8_t endian;
    uint8_t bits;
    int32_t img_h_pad;
    int32_t img_w_pad;
    int32_t img_h;
    int32_t img_w;
    int32_t fbc_buffer_size;
    uint8_t fbc_hdr_buffer[32];
};

/* for raw picture dump */
struct cam_dbg_dump {
	uint32_t dump_en;/* see enumerations above */
	uint32_t dump_count;
	uint32_t dump_ongoing;
	spinlock_t dump_lock;
	struct completion *dump_start[6];
};
extern struct cam_dbg_dump g_dbg_dump;

struct cam_mes_ctx {
	struct camera_queue mes_queue;
	struct completion mes_com;
};

struct cam_dump_ctx {
	struct camera_queue dump_queue;
	struct completion dump_com;
	timespec cur_dump_ts;
	uint32_t in_dump;
	uint32_t dump_count;
	enum cam_ch_id ch_id;
	uint32_t is_pack;
	uint32_t dcam_out_bits;
	uint32_t pack_bits;
	uint32_t dcam_out_fmt;
	int (*dump_cfg)(void *handle, uint32_t cmd, void *param);
	int (*dump_enqueue)(struct cam_dump_ctx *dump_base, struct camera_frame *pframe);
	int (*dump_file)(struct cam_dump_ctx *dump_base, struct camera_frame *pframe);

	/*pyr*/
	uint32_t pyr_layer_num;
	uint32_t start_layer;
	uint32_t is_pyr_rec;
	uint32_t is_pyr_dec;

	/* PDAF */
	uint32_t pdaf_type;

	/*one*/
	struct dcam_compress_info fbc_info;
};

int camdump_start(struct cam_thread_info* thrd_info, struct cam_dump_ctx *dump_ctx, uint32_t dcam_idx);
int camdump_stop(struct cam_dump_ctx *dump_base);
#ifdef __cplusplus
}
#endif

#endif