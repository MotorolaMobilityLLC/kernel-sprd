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

#ifndef _SPRD_VDSP_H
#define _SPRD_VDSP_H

#include <linux/ioctl.h>
#include <linux/compat.h>

#define SPRD_DSP_CMD_INLINE_DATA_SIZE	16
#define SPRD_DSP_CMD_INLINE_BUFFER_COUNT	1
#define SPRD_DSP_CMD_NAMESPACE_ID_SIZE	16

enum sprd_dsp_cmd_flag {
	SPRD_DSP_CMD_FLAG_REQUEST_VALID = 1,  /* request valid cmd */
	SPRD_DSP_CMD_FLAG_RESPONSE_VALID =2,  /* response valid cmd */
	SPRD_DSP_CMD_FLAG_REQUEST_NSID = 4,   /* request nsid cmd, with REQUEST_VALID */
	SPRD_DSP_CMD_FLAG_RESPONSE_DELIVERY_FAILED = 8, /*/ response fail cmd */
};

struct sprd_dsp_buffer {
	uint32_t flags;
	uint32_t size;    /* buffer size */
	int32_t fd;   /* buffer addr */
	uint32_t addr;
};

struct sprd_dsp_cmd
{
	enum sprd_dsp_cmd_flag flag; /* flag 0 */
	uint32_t in_data_size;   /* in data size */
	int32_t in_data_fd;
	uint32_t out_data_size;  /* out data size */
	int32_t out_data_fd;
	uint32_t buffer_size; /* buffer size */
	union {
	/* if in_data_size > sizeof in_data, remap it then send to vdsp directly*/
		uint32_t in_data_addr;
		uint8_t in_data[SPRD_DSP_CMD_INLINE_DATA_SIZE];
	};
	union {
		uint32_t out_data_addr;
		uint8_t out_data[SPRD_DSP_CMD_INLINE_DATA_SIZE];
	};
	union {
		uint32_t buffer_addr; /*same as in_data_size*/
		struct sprd_dsp_buffer buffer_data[SPRD_DSP_CMD_INLINE_BUFFER_COUNT];
		uint8_t buffer_alignment[SPRD_DSP_CMD_INLINE_DATA_SIZE];
	}; 
	char nsid[SPRD_DSP_CMD_NAMESPACE_ID_SIZE]; /* algo id, ex. "hdr"*/
	uint32_t priority; /* 0 is normal, 1 is high priority */
};

#define VDSP_IO_MAGIC               'R'
#define SPRD_VDSP_IO_VERSION     	    _IO(VDSP_IO_MAGIC, 1)
#define SPRD_VDSP_IO_LOAD_FW             _IO(VDSP_IO_MAGIC,2)
#define SPRD_VDSP_IO_SET_MSG \
	_IOW(VDSP_IO_MAGIC, 3, struct sprd_dsp_cmd)
#define SPRD_VDSP_IO_FAST_STOP \
	_IOW(VDSP_IO_MAGIC, 4, struct sprd_dsp_cmd)


#endif
