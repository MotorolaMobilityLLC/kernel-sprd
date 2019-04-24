/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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

#include <linux/uaccess.h>
#include <video/sprd_mm.h>

#include "isp_drv.h"

static int isp_k_grgb_imbalance_block(struct isp_io_param *param,
	enum isp_id idx)
{
	int ret = 0;
	struct isp_dev_grgb_imbalance_info grgb_imbalance_info;
	unsigned int val = 0;

	memset(&grgb_imbalance_info, 0x00, sizeof(grgb_imbalance_info));
	ret = copy_from_user((void *)&grgb_imbalance_info,
		param->property_param, sizeof(grgb_imbalance_info));
	if (ret != 0) {
		pr_err("isp_k_grgb_imablance_block: copy_from_user error, ret = 0x%x\n",
			(unsigned int)ret);
		return -1;
	}

	ISP_REG_MWR(idx, ISP_GC2_CTRL, BIT_0, grgb_imbalance_info.bypass);
	if (grgb_imbalance_info.bypass)
		return 0;

	val = (0xff & grgb_imbalance_info.gc2_slash_edge_thr0)
		| ((0xff & grgb_imbalance_info.gc2_hv_edge_thr0)<<8)
		| ((0xff & grgb_imbalance_info.gc2_S_baohedu02)<<16)
		| ((0xff & grgb_imbalance_info.gc2_S_baohedu01)<<24);
	ISP_REG_WR(idx, ISP_GC2_PARA1, val);

	val = (0x3ff & grgb_imbalance_info.gc2_slash_flat_thr0)
		| ((0x3ff & grgb_imbalance_info.gc2_hv_flat_thr0) << 10);
	ISP_REG_MWR(idx, ISP_GC2_PARA2, 0xfffff, val);

	val = (0x3ff & grgb_imbalance_info.gc2_flag3_frez)
		| ((0x3ff & grgb_imbalance_info.gc2_flag3_lum) << 10) |
		((0x3ff & grgb_imbalance_info.gc2_flag3_grid) << 20);
	ISP_REG_MWR(idx, ISP_GC2_PARA3, 0x3fffffff, val);

	val = (0xffff & grgb_imbalance_info.gc2_lumth2)
		| ((0xffff & grgb_imbalance_info.gc2_lumth1) << 16);
	ISP_REG_WR(idx, ISP_GC2_PARA4, val);

	val = (0x7ff & grgb_imbalance_info.gc2_lum0_flag4_r)
		| ((0x7ff & grgb_imbalance_info.gc2_lum0_flag2_r) << 11) |
		(0x3ff & grgb_imbalance_info.gc2_flag12_frezthr) << 22;
	ISP_REG_WR(idx, ISP_GC2_PARA5, val);

	val = (0x7ff & grgb_imbalance_info.gc2_lum0_flag0_r)
		| ((0x7ff & grgb_imbalance_info.gc2_lum0_flag0_rs) << 11);
	ISP_REG_MWR(idx, ISP_GC2_PARA6, 0x3fffff, val);

	val = (0x7ff & grgb_imbalance_info.gc2_lum1_flag2_r)
		| ((0x7ff & grgb_imbalance_info.gc2_lum0_flag1_r) << 11);
	ISP_REG_MWR(idx, ISP_GC2_PARA7, 0x3fffff, val);

	val = (0x7ff & grgb_imbalance_info.gc2_lum1_flag0_rs)
		| ((0x7ff & grgb_imbalance_info.gc2_lum1_flag4_r) << 11);
	ISP_REG_MWR(idx, ISP_GC2_PARA8, 0x3fffff, val);

	val = (0x7ff & grgb_imbalance_info.gc2_lum1_flag1_r)
		| ((0x7ff & grgb_imbalance_info.gc2_lum1_flag0_r)) << 11;
	ISP_REG_MWR(idx, ISP_GC2_PARA9, 0x3fffff, val);

	val = (0x7ff & grgb_imbalance_info.gc2_lum2_flag4_r)
		| ((0x7ff & grgb_imbalance_info.gc2_lum2_flag2_r) << 11);
	ISP_REG_MWR(idx, ISP_GC2_PARA10, 0x3fffff, val);

	val = (0x7ff & grgb_imbalance_info.gc2_lum2_flag0_r)
		| ((0x7ff & grgb_imbalance_info.gc2_lum2_flag0_rs) << 11);
	ISP_REG_MWR(idx, ISP_GC2_PARA11, 0x3fffff, val);

	val = (0x3ff & grgb_imbalance_info.gc2_diff0)
		| ((0x7ff & grgb_imbalance_info.gc2_lum2_flag1_r) << 10);
	ISP_REG_MWR(idx, ISP_GC2_PARA12, 0x1fffff, val);

	val = (0xffff & grgb_imbalance_info.gc2_faceRmax)
		| ((0xffff & grgb_imbalance_info.gc2_faceRmin) << 16);
	ISP_REG_WR(idx, ISP_GC2_PARA13, val);

	val = (0xffff & grgb_imbalance_info.gc2_faceBmax)
		| ((0xffff & grgb_imbalance_info.gc2_faceBmin) << 16);
	ISP_REG_WR(idx, ISP_GC2_PARA14, val);

	val = (0xffff & grgb_imbalance_info.gc2_faceGmax)
		| ((0xffff & grgb_imbalance_info.gc2_faceGmin) << 16);
	ISP_REG_WR(idx, ISP_GC2_PARA15, val);

	val = (0xff & grgb_imbalance_info.gc2_hv_edge_thr1)
		| ((0xff & grgb_imbalance_info.gc2_hv_edge_thr2) << 8)
		| ((0xff & grgb_imbalance_info.gc2_slash_edge_thr1) << 16)
		| ((grgb_imbalance_info.gc2_slash_edge_thr2) << 24);
	ISP_REG_WR(idx, ISP_GC2_PARA16, val);

	val = (0x3ff & grgb_imbalance_info.gc2_hv_flat_thr1)
		| ((0x3ff & grgb_imbalance_info.gc2_hv_flat_thr2) << 16);
	ISP_REG_MWR(idx, ISP_GC2_PARA17, 0x3ff03ff, val);

	val = (0x3ff & grgb_imbalance_info.gc2_slash_flat_thr1)
		| ((0x3ff & grgb_imbalance_info.gc2_slash_flat_thr2)<<16);
	ISP_REG_MWR(idx, ISP_GC2_PARA18, 0x3ff03ff, val);

	val = (0xff & grgb_imbalance_info.gc2_S_baohedu11)
		| ((0xff & grgb_imbalance_info.gc2_S_baohedu21) << 8)
		| ((0xff & grgb_imbalance_info.gc2_S_baohedu12) << 16)
		| ((0xff & grgb_imbalance_info.gc2_S_baohedu22) << 24);
	ISP_REG_WR(idx, ISP_GC2_PARA19, val);

	val = (0x7ff & grgb_imbalance_info.gc2_lum0_flag3_r)
		| ((0x7ff & grgb_imbalance_info.gc2_lum1_flag3_r) << 16);
	ISP_REG_MWR(idx, ISP_GC2_PARA20, 0x7ff07ff, val);

	val = (0x7ff & grgb_imbalance_info.gc2_lum2_flag3_r)
		| ((0x3ff & grgb_imbalance_info.gc2_sat_lumth) << 16);
	ISP_REG_MWR(idx, ISP_GC2_PARA21, 0x3ff07ff, val);

	val = (0x3ff & grgb_imbalance_info.gc2_diff1)
		| ((0x3ff & grgb_imbalance_info.gc2_diff2) << 16);
	ISP_REG_MWR(idx, ISP_GC2_PARA22, 0x3ff03ff, val);

	val = (0x3ff & grgb_imbalance_info.gc2_ff_wt0)
		| ((0x3ff & grgb_imbalance_info.gc2_ff_wt1) << 16);
	ISP_REG_MWR(idx, ISP_GC2_PARA23, 0x3ff03ff, val);

	val = (0x3ff & grgb_imbalance_info.gc2_ff_wt2)
		| ((0x3ff & grgb_imbalance_info.gc2_ff_wt3) << 16);
	ISP_REG_MWR(idx, ISP_GC2_PARA24, 0x3ff03ff, val);

	val = (0xff & grgb_imbalance_info.gc2_ff_wr0)
		| ((0xff & grgb_imbalance_info.gc2_ff_wr1) << 8)
		| ((0xff & grgb_imbalance_info.gc2_ff_wr2) << 16)
		| ((0xff & grgb_imbalance_info.gc2_ff_wr3) << 24);
	ISP_REG_WR(idx, ISP_GC2_PARA25, val);

	val = (0xff & grgb_imbalance_info.gc2_ff_wr4);
	ISP_REG_WR(idx, ISP_GC2_PARA26, val);

	val = grgb_imbalance_info.gc2_radial_1d_en << 1;
	ISP_REG_MWR(idx, ISP_GC2_CTRL, BIT_1, val);

	val = (0xff & grgb_imbalance_info.gc2_ff_wr4)
		| ((0xff & grgb_imbalance_info.gc2_radial_1d_coef_r0) << 8)
		| ((0xff & grgb_imbalance_info.gc2_radial_1d_coef_r1) << 16)
		| ((0xff & grgb_imbalance_info.gc2_radial_1d_coef_r2) << 24);
	ISP_REG_WR(idx, ISP_GC2_PARA26, val);

	val = (0xff & grgb_imbalance_info.gc2_radial_1d_coef_r3)
		| ((0xff & grgb_imbalance_info.gc2_radial_1d_coef_r4) << 8)
		| ((0x7ff & grgb_imbalance_info.gc2_radial_1d_protect_ratio_max)
		<< 16);
	ISP_REG_MWR(idx, ISP_GC2_PARA27, 0x7ffffff, val);

	val = (0xffff & grgb_imbalance_info.gc2_radial_1d_center_y)
		| ((0xffff & grgb_imbalance_info.gc2_radial_1d_center_x) << 16);
	ISP_REG_WR(idx, ISP_GC2_PARA28, val);

	val = 0xffff & grgb_imbalance_info.gc2_radial_1D_radius_threshold;
	ISP_REG_MWR(idx, ISP_GC2_PARA29, 0xffff, val);

	val = 0;
	ISP_REG_WR(idx, ISP_GC2_PARA30, val);

	return ret;
}

static int isp_k_grgb_imbalance_slice_param
	(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	struct isp_dev_grgb_imbalance_slice_info grgb_imbalance_slice;
	unsigned int val = 0;

	memset(&grgb_imbalance_slice, 0x00, sizeof(grgb_imbalance_slice));
	ret = copy_from_user((void *)&grgb_imbalance_slice,
		param->property_param, sizeof(grgb_imbalance_slice));
	if (ret != 0) {
		pr_err("isp_k_grgb_imablance_slice: copy_from_user error, ret = 0x%x\n",
			(unsigned int)ret);
		return -1;
	}

	val = ((grgb_imbalance_slice.gc2_global_y_start & 0xFFFF) << 16)
		| (grgb_imbalance_slice.gc2_global_x_start & 0xFFFF);
	ISP_REG_WR(idx, ISP_GC2_PARA30, val);

	return ret;

}
int isp_k_cfg_grgb_imbalance(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param\n");
		return -1;
	}

	if (!param->property_param) {
		pr_err("fail to get param\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_GRGB_IMBALANCE_BLOCK:
		ret = isp_k_grgb_imbalance_block(param, idx);
		break;
	case ISP_PRO_GRGB_IMBALANCE_SLICE_PARAM:
		ret = isp_k_grgb_imbalance_slice_param(param, idx);
		break;
	default:
		pr_err("fail to check cmd id:%d\n", param->property);
		break;
	}

	return ret;
}
