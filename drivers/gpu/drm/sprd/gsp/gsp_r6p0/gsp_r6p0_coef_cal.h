/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _GSP_R6P0_COEF_GENERATE_H_
#define _GSP_R6P0_COEF_GENERATE_H_

#include "gsp_r6p0_core.h"
#include "../gsp_debug.h"

#define LIST_SET_ENTRY_KEY(pEntry, i_w, i_h, o_w, o_h, h_t, v_t)\
{\
	pEntry->in_w = i_w;\
	pEntry->in_h = i_h;\
	pEntry->out_w = o_w;\
	pEntry->out_h = o_h;\
	pEntry->hor_tap = h_t;\
	pEntry->ver_tap = v_t;\
}

enum scale_kernel_type {
	GSP_SCL_TYPE_SINC,
	GSP_SCL_TYPE_BI_CUBIC,
	GSP_SCL_TYPE_MAX,
};

enum scale_win_type {
	GSP_SCL_WIN_RECT,
	GSP_SCL_WIN_SINC,
	GSP_SCL_WIN_MAX,
};

#define INTERPOLATION_STEP 128

/* log2(MAX_PHASE) */
#define FIX_POINT		4
#define MAX_PHASE		16
#define MAX_TAP			8
#define MAX_COEF_LEN	(MAX_PHASE * MAX_TAP + 1)

struct GSC_MEM_POOL {
	ulong begin_addr;
	ulong total_size;
	ulong used_size;
};

u32 *gsp_r6p0_gen_block_scaler_coef(struct gsp_r6p0_core *core,
						 u32 i_w,
						 u32 i_h,
						 u32 o_w,
						 u32 o_h,
						 u32 hor_tap,
						 u32 ver_tap);

#endif
