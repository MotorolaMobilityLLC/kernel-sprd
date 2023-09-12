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

#ifndef _ROT_DRV_H_
#define _ROT_DRV_H_

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/dma-buf.h>
#include <linux/scatterlist.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>

#include "cam_iommu.h"
#include "rot_reg.h"

/*#define ROTATE_DEBUG*/

#ifdef ROTATE_DEBUG
	#define ROTATE_TRACE printk
#else
	#define ROTATE_TRACE pr_debug
#endif


typedef void (*rot_isr_func)(void *rot_k_private);

struct rot_param_tag {
	struct rot_size_tag img_size;
	uint32_t format;
	uint32_t angle;
	struct rot_addr_tag src_addr;
	struct rot_addr_tag dst_addr;
	uint32_t s_addr;
	uint32_t d_addr;
	uint32_t pixel_format;
	uint32_t uv_mode;
	int is_end;
	uint32_t src_endian;
	uint32_t dst_endian;

	struct rot_cfg_tag rot_cfg_parm;
	struct pfiommu_info rot_iommu_src;
	struct pfiommu_info rot_iommu_dst;
};

struct rot_drv_private {
	struct rot_param_tag cfg;
	rot_isr_func user_isr_func;
	spinlock_t rot_drv_lock;
	void *rot_fd;/*rot file*/
};

int rot_k_module_en(struct device_node *dn);
int rot_k_module_dis(struct device_node *dn);
int rot_k_isr_reg(rot_isr_func user_func, struct rot_drv_private *drv_private);
int rot_k_is_end(struct rot_param_tag *s);
int rot_k_set_UV_param(struct rot_cfg_tag *param_ptr,
					struct rot_param_tag *s);
void rot_k_register_cfg(struct rot_param_tag *s);
void rot_k_close(void);
int rot_k_io_cfg(struct rot_cfg_tag *param_ptr, struct rot_param_tag *s);
int sprd_rot_drv_init(struct platform_device *pdev);
#endif
