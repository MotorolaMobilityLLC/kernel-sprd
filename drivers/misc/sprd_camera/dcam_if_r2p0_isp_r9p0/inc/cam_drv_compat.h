/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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

#include "isp_drv.h"

#define ISP_COMPAT_SUPPORT
#ifdef ISP_COMPAT_SUPPORT

/*#define COMPAT_ISP_DEBUG*/
#ifdef COMPAT_ISP_DEBUG
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "compat_isp_trace: %s " fmt, __func__

#endif

struct compat_isp_addr {
	compat_ulong_t chn0;
	compat_ulong_t chn1;
	compat_ulong_t chn2;
};

struct compat_isp_statis_buf_input {/* TODO */
	uint32_t buf_size;
	uint32_t dcam_stat_buf_size;
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
	uint32_t reserved[4];
};

struct compat_isp_dev_block_addr {/* TODO */
	struct compat_isp_addr img_vir;
	struct compat_isp_addr img_offset;
	unsigned int img_fd;
};

struct compat_isp_raw_proc_info {/* TODO */
	struct isp_img_size in_size;
	struct isp_img_size out_size;
	struct compat_isp_addr img_vir;
	struct compat_isp_addr img_offset;
	unsigned int img_fd;
	unsigned int padding;
};

struct compat_isp_io_param {/* TODO */
	unsigned int isp_id;
	unsigned int scene_id;
	unsigned int sub_block;
	unsigned int property;
	compat_caddr_t property_param;
};

struct compat_sprd_isp_capability {
	uint32_t isp_id;
	uint32_t index;
	compat_caddr_t property_param;
};

struct compat_isp_dev_fetch_info_v1 {
	unsigned int bypass;
	unsigned int subtract;
	unsigned int color_format;
	unsigned int start_isp;
	struct isp_img_size size;
	struct isp_addr_fs addr;
	struct isp_pitch_fs pitch;
	unsigned int mipi_word_num;
	unsigned int mipi_byte_rel_pos;
	unsigned int no_line_dly_ctrl;
	unsigned int req_cnt_num;
	unsigned int line_dly_num;
	struct compat_isp_dev_block_addr fetch_addr;
};

#define COMPAT_SPRD_ISP_IO_CAPABILITY \
	_IOR(SPRD_IMG_IO_MAGIC, 33, struct compat_sprd_isp_capability)

#define COMPAT_SPRD_ISP_IO_SET_STATIS_BUF \
	_IOW(SPRD_IMG_IO_MAGIC, 40, struct compat_isp_statis_buf_input)

#define COMPAT_SPRD_ISP_IO_CFG_PARAM \
	_IOWR(SPRD_IMG_IO_MAGIC, 41, struct compat_isp_io_param)

#define COMPAT_SPRD_ISP_IO_RAW_CAP \
	_IOR(SPRD_IMG_IO_MAGIC, 45, struct compat_isp_raw_proc_info)

extern long sprd_cam_drv_compat_ioctl(
				struct file *file,
				unsigned int cmd,
				unsigned long param);

#endif /* ISP_COMPAT_SUPPORT */
