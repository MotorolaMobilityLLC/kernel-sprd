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

#ifndef __CAM_DEBUGGER_H__
#define __CAM_DEBUGGER_H__

#include "dcam_interface.h"
#include "cam_dump.h"
/* compression override commands */
enum {
	CH_PRE = 0,
	CH_CAP = 1,
	CH_VID = 2,
	CH_MAX = 3,

	FBC_DCAM = 0,
	FBC_3DNR = 1,
	FBC_ISP = 2,
	FBC_MAX = 3,
};

enum fbc_crl_type {
	DEBUG_FBC_CRL_BIN = 0x1,
	DEBUG_FBC_CRL_FULL = 0x2,
	DEBUG_FBC_CRL_RAW = 0x4,
	DEBUG_FBC_CRL_MAX,
};

/* compression override setting */
struct compression_override {
	uint32_t enable;
	uint32_t override[CH_MAX][FBC_MAX];
};

/*
 * replace input/output image by using reserved buffer...
 * currently only for FULL and BIN path
 */

struct cam_debug_bypass {
	uint32_t idx;
	struct cam_hw_info *hw;
};

struct camera_debugger {
	struct compression_override compression[CAM_ID_MAX];
	struct cam_hw_info *hw;
};

int cam_debugger_init(struct camera_debugger *debugger);
int cam_debugger_deinit(void);

#endif
