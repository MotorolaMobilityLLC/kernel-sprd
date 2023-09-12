/**
 * Copyright (C) 2019-2022 UNISOC Technologies Co.,Ltd.
 */

/*
 * XRP: Linux device driver for Xtensa Remote Processing
 *
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Alternatively you can use and distribute this file under the terms of
 * the GNU General Public License version 2 or later.
 */
/*
 * This file has been modified by UNISOC to expanding communication module to
 * realize vdsp device communication and add memory,dvfs, faceid contol define.
 */

#ifndef __XVP_MAIN_H__
#define __XVP_MAIN_H__

#include <linux/types.h>
#include "xrp_kernel_defs.h"
#include "xrp_kernel_dsp_interface.h"
#include "vdsp_dvfs.h"

struct xvp_file {
	struct xvp *xvp;
	struct list_head load_lib_list;
	struct vdsp_powerhint powerhint;
	struct mutex lock;
	uint32_t working;
	struct list_head buf_list;
	struct mutex xvpfile_buf_list_lock;
};

struct xrp_known_file {
	void *filp;
	struct hlist_node node;
};

struct xrp_request {
	struct xrp_ioctl_queue ioctl_queue;
	size_t n_buffers;
	struct xvp_buf *in_buf;	// in buf
	struct xvp_buf *out_buf;	// out buf
	struct xvp_buf *dsp_buf;	// buf list
	int *id_dsp_pool;
	union {
		u8 in_data[XRP_DSP_CMD_INLINE_DATA_SIZE];
	};
	union {
		u8 out_data[XRP_DSP_CMD_INLINE_DATA_SIZE];
	};
	union {
		struct xrp_dsp_buffer buffer_data[XRP_DSP_CMD_INLINE_BUFFER_COUNT];
	};
	u8 nsid[XRP_DSP_CMD_NAMESPACE_ID_SIZE];
};

#endif
