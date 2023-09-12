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

#ifndef _DCAM_HW_ADPT_H_
#define _DCAM_HW_ADPT_H_

#include "cam_types.h"

#define DCAM_64M_WIDTH                 9280
#define DCAM_24M_WIDTH                 5664
#define DCAM_16M_WIDTH                 4672
#define DCAM_13M_WIDTH                 4160
#define DCAM_8M_WIDTH                  3264
#define DCAM_RDS_OUT_LIMIT             2160
#define DCAM_OVERLAP                   64

#define DCAM_PATH_WMAX                 8048
#define DCAM_PATH_HMAX                 6036
#define RAW_OVERLAP_UP                 58
#define RAW_OVERLAP_DOWN               78
#define RAW_OVERLAP_LEFT               118
#define RAW_OVERLAP_RIGHT              138
#define DCAM_SW_SLICE_HEIGHT_MAX       8192
#define DCAM_HW_SLICE_WIDTH_MAX        8192
#define CAM_FACEID_SEC
#define DCAM_OFFSET_RANGE              0x3E103E0

#define DCAM_SCALE_DOWN_MAX            10
#define DCAM_SCALER_MAX_WIDTH          3840
#define DCAM_FRAME_TIMESTAMP_COUNT     0x100

/*Total lbuf DCAM0+DCAM1: 5184+5184 */
#define DCAM_TOTAL_LBUF                10368

/*
 * dcam_if fbc capability limit
 * modification to these values may cause some function in isp_slice.c not
 * work, check @ispslice_slice_fbd_raw_cfg and all other symbol references for details
 */

#define DCAM_FBC_TILE_WIDTH            32
#define DCAM_FBC_TILE_HEIGHT           8
#define FBC_PAYLOAD_YUV10_BYTE_UNIT    512
#define FBC_PAYLOAD_YUV8_BYTE_UNIT     384
#define FBC_PAYLOAD_RAW10_BYTE_UNIT    640
#define FBC_HEADER_BYTE_UNIT           16
#define FBC_TILE_HEAD_SIZE_ALIGN       1024
#define FBC_STORE_ADDR_ALIGN           16
#define FBC_LOWBIT_PITCH_ALIGN         16

#define FBC_HEADER_REDUNDANT           0
#define FBC_TILE_ADDR_ALIGN            1
#define ISP_FBD_MAX_WIDTH              0xFFFFFFFF

enum dcam_ctrl_id {
	DCAM_CTRL_CAP = (1 << 0),
	DCAM_CTRL_COEF = (1 << 1),
	DCAM_CTRL_RDS = (1 << 2),
	DCAM_CTRL_FULL = (1 << 3),
	DCAM_CTRL_BIN = (1 << 4),
	DCAM_CTRL_PDAF = (1 << 5),
	DCAM_CTRL_VCH2 = (1 << 6),
	DCAM_CTRL_VCH3 = (1 << 7),
};
#define DCAM_CTRL_ALL  0xff

enum raw_pitch_format {
	RAW_PACK10 = 0x00,
	RAW_HALF10 = 0x01,
	RAW_HALF14 = 0x02,
	RAW_8 = 0x03,
	RAW_FORMAT_MAX
};

enum dcam_hw_context_id {
	DCAM_HW_CONTEXT_0 = 0,
	DCAM_HW_CONTEXT_1,
	DCAM_HW_CONTEXT_MAX,
};

enum dcam_id {
	DCAM_ID_0 = 0,
	DCAM_ID_1,
	DCAM_ID_2,
	DCAM_ID_3,
	DCAM_ID_MAX,
};

enum csi_id {
	CSI_ID_0 = 0,
	CSI_ID_1,
	CSI_ID_2,
	CSI_ID_3,
	CSI_ID_4,
	CSI_ID_5,
	CSI_ID_MAX,
};

static inline uint32_t cal_sprd_raw_pitch(uint32_t w, uint32_t pack_bits)
{
	if(pack_bits == DCAM_RAW_PACK_10)
		w = (w * 10 + 127) / 128 * 128 / 8;
	else
		w = (w * 16 + 127) / 128 * 128 / 8;

	return w;
}

static inline uint32_t dcam_if_cal_compressed_size(struct dcam_compress_cal_para *para)
{
	uint32_t payload_size = 0, lowbits_size = 0;
	int32_t tile_col = 0, tile_row = 0, header_size = 0, payload_unit = 0;

	if (para->fmt & DCAM_STORE_RAW_BASE)
		payload_unit = FBC_PAYLOAD_RAW10_BYTE_UNIT;
	else {
		if (para->data_bits == DCAM_STORE_10_BIT)
			payload_unit = FBC_PAYLOAD_YUV10_BYTE_UNIT;
		else
			payload_unit = FBC_PAYLOAD_YUV8_BYTE_UNIT;
	}
	tile_col = (para->width + DCAM_FBC_TILE_WIDTH - 1) / DCAM_FBC_TILE_WIDTH;
	if (para->fmt & DCAM_STORE_RAW_BASE)
		tile_row = (para->height + DCAM_FBC_TILE_HEIGHT * 2 - 1) / DCAM_FBC_TILE_HEIGHT / 2;
	else
		tile_row = (para->height + DCAM_FBC_TILE_HEIGHT - 1) / DCAM_FBC_TILE_HEIGHT;

	/* header addr 16byte align, size 1024byte align*/
	header_size = tile_col * tile_row * FBC_HEADER_BYTE_UNIT;
	header_size = ALIGN(header_size, FBC_TILE_HEAD_SIZE_ALIGN);

	/*payload/lowbit addr 16byte align, lowbit addr = payload addr + payload size, so payload size 16byte align*/
	payload_size= tile_col * tile_row * payload_unit;
	payload_size = ALIGN(payload_size, FBC_STORE_ADDR_ALIGN);

	if ((!para->compress_4bit_bypass) && (para->fmt & DCAM_STORE_RAW_BASE))
		lowbits_size = payload_size >> 1;

	return header_size + payload_size + lowbits_size;
}

static inline void dcam_if_cal_compressed_addr(struct dcam_compress_cal_para *para)
{
	uint32_t payload_size = 0, lowbits_size = 0;
	int32_t tile_col = 0, tile_row = 0, header_size = 0, payload_unit = 0;
	struct dcam_compress_info *fbc_info = para->fbc_info;

	if (unlikely(!para|| !para->out))
		return;

	if (para->fmt & DCAM_STORE_RAW_BASE)
		payload_unit = FBC_PAYLOAD_RAW10_BYTE_UNIT;
	else {
		if (para->data_bits == DCAM_STORE_10_BIT)
			payload_unit = FBC_PAYLOAD_YUV10_BYTE_UNIT;
		else
			payload_unit = FBC_PAYLOAD_YUV8_BYTE_UNIT;
	}
	tile_col = (para->width + DCAM_FBC_TILE_WIDTH - 1) / DCAM_FBC_TILE_WIDTH;
	if (para->fmt & DCAM_STORE_RAW_BASE)
		tile_row = (para->height + DCAM_FBC_TILE_HEIGHT * 2 - 1) / DCAM_FBC_TILE_HEIGHT / 2;
	else
		tile_row = (para->height + DCAM_FBC_TILE_HEIGHT - 1) / DCAM_FBC_TILE_HEIGHT;

	/* header addr 16byte align, size 1024byte align*/
	header_size = tile_col * tile_row * FBC_HEADER_BYTE_UNIT;
	header_size = ALIGN(header_size, FBC_TILE_HEAD_SIZE_ALIGN);

	/*payload/lowbit addr 16byte align, lowbit addr = payload addr + payload size, so payload size 16byte align*/
	payload_size= tile_col * tile_row * payload_unit;
	payload_size = ALIGN(payload_size, FBC_STORE_ADDR_ALIGN);

	if ((!para->compress_4bit_bypass) && (para->fmt & DCAM_STORE_RAW_BASE))
		lowbits_size = payload_size >> 1;

	para->out->addr0 = ALIGN(para->in, FBC_STORE_ADDR_ALIGN);
	para->out->addr1 = ALIGN(para->out->addr0 + header_size, FBC_STORE_ADDR_ALIGN);

	if (!para->compress_4bit_bypass) {
		para->out->addr2 = ALIGN(para->out->addr1+ payload_size, FBC_STORE_ADDR_ALIGN);
	}

	if (fbc_info) {
		fbc_info->tile_col = tile_col;
		fbc_info->tile_row = tile_row;
		fbc_info->is_compress = 1;
		fbc_info->tile_row_lowbit = (para->width / 2 + FBC_LOWBIT_PITCH_ALIGN - 1) /
						FBC_LOWBIT_PITCH_ALIGN * FBC_LOWBIT_PITCH_ALIGN;
		fbc_info->header_size = header_size;
		fbc_info->payload_size = payload_size;
		fbc_info->lowbits_size = lowbits_size;
		fbc_info->buffer_size = header_size + payload_size + lowbits_size;
	}
}

#endif
