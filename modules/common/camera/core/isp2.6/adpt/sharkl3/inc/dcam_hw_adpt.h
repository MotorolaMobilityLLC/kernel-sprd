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

#define DCAM_64M_WIDTH                          9216
#define DCAM_24M_WIDTH                          5664
#define DCAM_PATH_WMAX                          5000
#define DCAM_PATH_HMAX                          4000
#define RAW_OVERLAP_UP                          62
#define RAW_OVERLAP_DOWN                        82
#define RAW_OVERLAP_LEFT                        122
#define RAW_OVERLAP_RIGHT                       142
#define DCAM_SW_SLICE_HEIGHT_MAX                4096
#define DCAM_HW_SLICE_WIDTH_MAX                 4096
#define DCAM_RDS_OUT_LIMIT                      2048

#define DCAM_FBC_TILE_WIDTH                     64
#define DCAM_FBC_TILE_HEIGHT                    4
#define FBC_TILE_ADDR_ALIGN                     256
#define FBC_HEADER_REDUNDANT                    64
#define ISP_FBD_MAX_WIDTH                       0xFFFFFFFF
#define DCAM_SCALE_DOWN_MAX                     4
#define DCAM_SCALER_MAX_WIDTH                   0xFFFFFFFF
#define DCAM_FRAME_TIMESTAMP_COUNT              0x40
#define DCAM_OVERLAP                            0

/*
 *DCAM_CONTROL register bit map id
 * for force_cpy/auto_cpy control
 */
enum dcam_ctrl_id {
	DCAM_CTRL_CAP = (1 << 0),
	DCAM_CTRL_RDS = (1 << 1),
	DCAM_CTRL_FULL = (1 << 2),
	DCAM_CTRL_BIN = (1 << 3),
	DCAM_CTRL_AEM = (1 << 4),
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
	DCAM_HW_CONTEXT_2,
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
	uint32_t mod16_len[16] = {0, 8, 8, 8, 8, 12, 12, 12,
				12, 16, 16, 16, 16, 20, 20, 20};

	if (pack_bits == DCAM_RAW_PACK_10)
		return ((w >> 4) * 20 + (mod16_len[w & 0xf]));
	else
		return w*2;
}

static inline uint32_t dcam_if_cal_compressed_size(struct dcam_compress_cal_para *para)
{
	pr_debug("sharkl3 not support fbc\n");
	return 0;
}

static inline void dcam_if_cal_compressed_addr(struct dcam_compress_cal_para *para)
{
	pr_debug("sharkl3 not support fbc\n");
}
#endif
