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

#define DCAM_64M_WIDTH                 9216
#define DCAM_48M_WIDTH                 8048
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

#define DCAM_FBC_TILE_WIDTH            64
#define DCAM_FBC_TILE_HEIGHT           4
#define FBC_TILE_ADDR_ALIGN            256
#define FBC_HEADER_REDUNDANT           64
#define ISP_FBD_MAX_WIDTH              0xFFFFFFFF
#define DCAM_SCALE_DOWN_MAX            4
#define DCAM_SCALER_MAX_WIDTH          0xFFFFFFFF
#define DCAM_FRAME_TIMESTAMP_COUNT     0x100


/*
 *DCAM_CONTROL register bit map id
 * for force_cpy/auto_cpy control
 */
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

/*
 * Supported dcam_if index. Number 0&1 for dcam_if and 2 for dcam_if_lite.
 */

enum dcam_id {
	DCAM_ID_0 = 0,
	DCAM_ID_1,
	DCAM_ID_2,
	DCAM_ID_MAX,
};

enum csi_id {
	CSI_ID_0 = 0,
	CSI_ID_1,
	CSI_ID_2,
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
	pr_debug("qogirl6 not support fbc\n");
	return 0;
}

static inline void dcam_if_cal_compressed_addr(struct dcam_compress_cal_para *para)
{
	pr_debug("qogirl6 not support fbc\n");
}
#endif
