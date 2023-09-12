/**
 * Copyright (C) 2019-2022 UNISOC (Shanghai) Technologies Co.,Ltd.
 */

/*
 * Internal XRP structures definition.
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
 * This file has been modified by UNISOC to add faceid, memory, iommu related define
 * to realize real device driver.
 */
#ifndef XRP_INTERNAL_H
#define XRP_INTERNAL_H

#include <linux/completion.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include "sprd_vdsp_mem_core.h"
#include "sprd_vdsp_mem_core_priv.h"
#include "sprd_vdsp_mem_xvp_init.h"
#include "xrp_library_loader.h"
#include "vdsp_dvfs.h"
#include "vdsp_log.h"

struct device;
struct firmware;
struct xrp_hw_ops;

struct firmware_origin {
	size_t size;
	u8 *data;
};

struct xrp_comm {
	struct mutex lock;
	void __iomem *comm;
	struct completion completion;
	u32 priority;
};

struct faceid_mem_addr {
#ifdef MYL5
	struct xvp_buf *ion_fd_weights_p;
	struct xvp_buf *ion_fd_weights_r;
	struct xvp_buf *ion_fd_weights_o;
	struct xvp_buf *ion_fp_weights;
	struct xvp_buf *ion_flv_weights;
	struct xvp_buf *ion_fo_weights;
	struct xvp_buf *ion_fd_mem_pool;
#endif
#ifdef MYN6
	struct xvp_buf *ion_fa_weights;
	struct xvp_buf *ion_fp_weights;
	struct xvp_buf *ion_foc_weights;
	struct xvp_buf *ion_fd_mem_pool;
#endif
};

struct xvp {
	struct device *dev;
	const char *firmware_name;
	const struct firmware *firmware;

	struct miscdevice miscdev;
	const struct xrp_hw_ops *hw_ops;
	void *hw_arg;
	unsigned n_queues;

	u32 *queue_priority;
	struct xrp_comm *queue;
	struct xrp_comm **queue_ordered;
	atomic_t reboot_cycle;
	atomic_t reboot_cycle_complete;

	bool host_irq_mode;
	bool off;
	int nodeid;
	uint32_t open_count;

	struct vdsp_log_state *log_state;
	struct xrp_load_lib_info load_lib;
	struct vdsp_dvfs_info dvfs_info;
	struct hlist_head xrp_known_files[1 << 10];
	struct mutex xrp_known_files_lock;
	// iommu
	struct sprd_vdsp_iommus *iommus;
	// mem manage
	struct xvp_mem_dev *mem_dev;
	struct mem_ctx *drv_mem_ctx;	//dup mem_dev->xvp_mem_ctx
	// xvp_buf
	struct xvp_buf *ipc_buf;
	struct xvp_buf *log_buf;
	struct xvp_buf *fw_buf;

	//faceid
	bool secmode;		/*used for faceID */
	bool tee_con;		/*the status of connect TEE */
	int irq_status;
	uint32_t cur_opentype;
	struct faceid_mem_addr faceid_pool;
	const struct firmware *faceid_fw;
	const struct firmware *firmware2_sign;	/*faceid sign fw */
	const struct firmware *firmware_coeff;
	struct xvp_buf *faceid_com_buf;
	struct xvp_buf *faceid_fw_buf;
	struct xvp_buf *faceid_fws_buf;
#ifdef MYN6
	struct xvp_buf *faceid_img_buf;
	unsigned long faceid_addr_offset;
#endif
};
#endif
