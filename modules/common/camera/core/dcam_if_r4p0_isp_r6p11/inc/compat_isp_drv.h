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

#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "sprd_isp_hw.h"
#include "isp_drv.h"

#define ISP_COMPAT_SUPPORT
#ifdef ISP_COMPAT_SUPPORT

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "compat_isp_trace: %s " fmt, __func__

#define COMPAT_SPRD_ISP_IO_CAPABILITY \
	_IOR(SPRD_IMG_IO_MAGIC, 33, struct compat_sprd_isp_capability)
#define COMPAT_SPRD_ISP_IO_SET_STATIS_BUF \
	_IOW(SPRD_IMG_IO_MAGIC, 40, struct compat_isp_statis_buf_input)
#define COMPAT_SPRD_ISP_IO_CFG_PARAM \
	_IOWR(SPRD_IMG_IO_MAGIC, 41, struct compat_isp_io_param)
#define COMPAT_SPRD_STATIS_IO_CFG_PARAM \
	_IOW(SPRD_IMG_IO_MAGIC, 44, struct compat_isp_io_param)
#define COMPAT_SPRD_ISP_IO_RAW_CAP \
	_IOR(SPRD_IMG_IO_MAGIC, 45, struct compat_isp_raw_proc_info)

struct compat_isp_addr {
	compat_ulong_t chn0;
	compat_ulong_t chn1;
	compat_ulong_t chn2;
};

struct compat_isp_statis_buf_input { /* TODO */
	uint32_t buf_size;
	uint32_t buf_num;
	compat_ulong_t phy_addr;
	compat_ulong_t vir_addr;
	compat_ulong_t addr_offset;
	uint32_t kaddr[2];
	compat_ulong_t mfd;
	compat_ulong_t dev_fd;
	uint32_t buf_property;
	uint32_t buf_flag;
	uint32_t statis_valid;
	uint32_t is_statis_buf_reserved;
	uint32_t reserved[4];
};

struct compat_isp_dev_block_addr { /* TODO */
	struct compat_isp_addr img_vir;
	struct compat_isp_addr img_offset;
	uint32_t img_fd;
};

struct compat_isp_raw_proc_info {  /* TODO */
	struct isp_img_size in_size;
	struct isp_img_size out_size;
	struct compat_isp_addr img_vir;
	struct compat_isp_addr img_offset;
	uint32_t img_fd;
	uint32_t sensor_id;
};

struct compat_isp_io_param {  /* TODO */
	uint32_t isp_id;
	uint32_t scene_id;
	uint32_t sub_block;
	uint32_t property;
	compat_caddr_t property_param;
};

struct compat_sprd_isp_capability {
	uint32_t isp_id;
	uint32_t index;
	compat_caddr_t property_param;
};

struct compat_isp_dev_fetch_info_v1 {
	uint32_t bypass;
	uint32_t subtract;
	uint32_t color_format;
	uint32_t start_isp;
	struct isp_img_size size;
	struct isp_addr_fs addr;
	struct isp_pitch_fs pitch;
	uint32_t mipi_word_num;
	uint32_t mipi_byte_rel_pos;
	uint32_t no_line_dly_ctrl;
	uint32_t req_cnt_num;
	uint32_t line_dly_num;
	struct compat_isp_dev_block_addr fetch_addr;
};

extern long compat_sprd_img_k_ioctl(
		struct file *file,
		uint32_t cmd,
		unsigned long param);
#endif /* ISP_COMPAT_SUPPORT */
