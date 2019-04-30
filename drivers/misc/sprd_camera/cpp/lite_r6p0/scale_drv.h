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

#ifndef _SCALE_DRV_H_
#define _SCALE_DRV_H_

#include <uapi/video/sprd_cpp.h>
#include "cpp_core.h"

struct scale_drv_private {
	unsigned int input_fmt;
	unsigned int input_endian;
	unsigned int input_uv_endian;
	unsigned int output_endian;
	unsigned int output_uv_endian;
	unsigned int rch_burst_gap;
	unsigned int wch_burst_gap;
	unsigned int src_pitch;
	struct sprd_cpp_rect src_rect;
	unsigned int ver_deci;
	unsigned int hor_deci;

	struct sprd_cpp_size sc_intrim_src_size;
	struct sprd_cpp_rect sc_intrim_rect;
	struct sprd_cpp_size sc_slice_in_size;
	struct sprd_cpp_size sc_slice_out_size;
	struct sprd_cpp_size sc_full_in_size;
	struct sprd_cpp_size sc_full_out_size;
	struct sprd_cpp_size sc_out_trim_src_size;
	struct sprd_cpp_rect sc_outtrim_rect;
	unsigned int y_hor_ini_phase_int;
	unsigned int y_hor_ini_phase_frac;
	unsigned int uv_hor_ini_phase_int;
	unsigned int uv_hor_ini_phase_frac;
	unsigned int y_ver_ini_phase_int;
	unsigned int y_ver_ini_phase_frac;
	unsigned int uv_ver_ini_phase_int;
	unsigned int uv_ver_ini_phase_frac;
	unsigned int y_ver_tap;
	unsigned int uv_ver_tap;
	unsigned int sc_des_pitch;
	struct sprd_cpp_rect sc_des_rect;
	unsigned int sc_output_fmt;

	unsigned int bp_en;
	struct sprd_cpp_size bp_trim_src_size;
	struct sprd_cpp_rect bp_trim_rect;
	unsigned int bp_des_pitch;
	struct sprd_cpp_rect bp_des_rect;

	spinlock_t *hw_lock;
	void *priv;
	struct cpp_iommu_info iommu_src;
	struct cpp_iommu_info iommu_dst;
	struct cpp_iommu_info iommu_dst_bp;
	void __iomem *io_base;
	struct platform_device *pdev;
};

void sprd_scale_drv_max_size_get(unsigned int *max_width,
		unsigned int *max_height);

void sprd_scale_drv_start(struct scale_drv_private *p);
void sprd_scale_drv_stop(struct scale_drv_private *p);
int sprd_scale_drv_capability_get(
		struct sprd_cpp_scale_capability *scale_param);
void sprd_scale_drv_dev_enable(struct scale_drv_private *p);
int sprd_scale_drv_slice_param_set(struct scale_drv_private *p,
		struct sprd_cpp_scale_cfg_parm *sc_cfg,
		struct sprd_cpp_hw_slice_parm *convert_param);
void sprd_scale_drv_dev_start(struct scale_drv_private *p);
void sprd_scale_drv_dev_stop(struct scale_drv_private *p);

#endif
