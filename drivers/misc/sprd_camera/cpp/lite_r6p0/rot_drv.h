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

#ifndef _ROT_DRV_H_
#define _ROT_DRV_H_

#include "cpp_core.h"

struct rot_drv_private {
	struct sprd_cpp_rot_cfg_parm cfg_parm;
	spinlock_t *hw_lock;
	unsigned int rot_fmt;
	unsigned int uv_mode;
	unsigned int rot_mode;
	unsigned int rot_src_addr;
	unsigned int rot_dst_addr;
	struct sprd_cpp_size rot_size;
	unsigned int rot_endian;

	struct cpp_iommu_info iommu_src;
	struct cpp_iommu_info iommu_dst;
	void __iomem *io_base;

	void *priv;
};

int sprd_rot_drv_parm_check(struct sprd_cpp_rot_cfg_parm *parm);
int sprd_rot_drv_is_end(struct sprd_cpp_rot_cfg_parm *parm);
int sprd_rot_drv_y_parm_set(
	struct sprd_cpp_rot_cfg_parm *parm, struct rot_drv_private *p);
void sprd_rot_drv_uv_parm_set(struct rot_drv_private *p);
void sprd_rot_drv_start(struct rot_drv_private *p);
void sprd_rot_drv_stop(struct rot_drv_private *p);

#endif
