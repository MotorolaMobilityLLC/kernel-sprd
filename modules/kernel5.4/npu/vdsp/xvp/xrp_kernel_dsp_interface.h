/**
 * Copyright (C) 2019-2022 UNISOC (Shanghai) Technologies Co.,Ltd.
 */

/*
 * Data structures and constants for generic XRP interface between
 * linux and DSP
 *
 * Copyright (c) 2015 - 2019 Cadence Design Systems, Inc.
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
 * realize vdsp device communication.
 */

#ifndef _XRP_KERNEL_DSP_INTERFACE_H
#define _XRP_KERNEL_DSP_INTERFACE_H

#ifndef XRP_DSP_COMM_BASE_MAGIC
#define XRP_DSP_COMM_BASE_MAGIC		0x20161006
#endif

enum {
	XRP_DSP_SYNC_IDLE = 0,
	XRP_DSP_SYNC_HOST_TO_DSP = 0x1,
	XRP_DSP_SYNC_DSP_TO_HOST = 0x3,
	XRP_DSP_SYNC_START = 0x101,
	XRP_DSP_SYNC_DSP_READY_V1 = 0x203,
	XRP_DSP_SYNC_DSP_READY_V2 = 0x303,
};

enum {
	XRP_DSP_SYNC_TYPE_ACCEPT = 0x80000000,
	XRP_DSP_SYNC_TYPE_MASK = 0x00ffffff,

	XRP_DSP_SYNC_TYPE_LAST = 0,
	XRP_DSP_SYNC_TYPE_HW_SPEC_DATA = 1,
	XRP_DSP_SYNC_TYPE_HW_QUEUES = 2,
	XRP_DSP_SYNC_TYPE_LOG = 3,
};

struct xrp_dsp_tlv {
	__u32 type;
	__u32 length;
	__u32 value[0];
};

struct xrp_dsp_sync_v1 {
	__u32 sync;
	__u32 hw_sync_data[0];
};

struct xrp_dsp_sync_v2 {
	__u32 sync;
	__u32 reserved[3];
	struct xrp_dsp_tlv hw_sync_data[0];
};

enum {
	XRP_DSP_BUFFER_FLAG_READ = 0x1,
	XRP_DSP_BUFFER_FLAG_WRITE = 0x2,
};

struct xrp_dsp_buffer {
	/*
	 * When submitted to DSP: types of access allowed
	 * When returned to host: actual access performed
	 */
	__u32 flags;
	__u32 size;
	__u32 addr;
	int fd;
};

enum {
	XRP_DSP_CMD_FLAG_REQUEST_VALID = 0x00000001,
	XRP_DSP_CMD_FLAG_RESPONSE_VALID = 0x00000002,
	XRP_DSP_CMD_FLAG_REQUEST_NSID = 0x00000004,
	XRP_DSP_CMD_FLAG_RESPONSE_DELIVERY_FAIL = 0x00000008,
};

#define XRP_DSP_CMD_INLINE_DATA_SIZE 16
#define XRP_DSP_CMD_INLINE_BUFFER_COUNT 1
#define XRP_DSP_CMD_NAMESPACE_ID_SIZE 32
#define XRP_DSP_CMD_STRIDE 128

struct xrp_dsp_cmd {
	__u32 flags;
	__u32 in_data_size;
	int in_data_fd;
	__u32 out_data_size;
	int out_data_fd;
	__u32 buffer_size;
	union {
		__u32 in_data_addr;
		__u8 in_data[XRP_DSP_CMD_INLINE_DATA_SIZE];
	};
	union {
		__u32 out_data_addr;
		__u8 out_data[XRP_DSP_CMD_INLINE_DATA_SIZE];
	};
	union {
		__u32 buffer_addr;
		struct xrp_dsp_buffer buffer_data[XRP_DSP_CMD_INLINE_BUFFER_COUNT];
		__u8 buffer_alignment[XRP_DSP_CMD_INLINE_DATA_SIZE];
	};
	__u8 nsid[XRP_DSP_CMD_NAMESPACE_ID_SIZE];
};

#endif
