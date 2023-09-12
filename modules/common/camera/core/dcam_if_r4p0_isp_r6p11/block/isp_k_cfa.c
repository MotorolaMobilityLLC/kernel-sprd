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

#include <linux/uaccess.h>

#include "sprd_mm.h"
#include "sprd_isp_hw.h"
#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CFA: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_cfa_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_dev_cfa_info cfa_info;

	memset(&cfa_info, 0x00, sizeof(cfa_info));

	ret = copy_from_user((void *)&cfa_info, param->property_param,
			sizeof(cfa_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_CFAE_NEW_CFG0, BIT_0, cfa_info.bypass);
	if (cfa_info.bypass)
		return 0;

	val = (cfa_info.grid_thr & 0xFFFF) |
			((cfa_info.min_grid_new & 0x1FFF) << 16);
	ISP_REG_WR(idx, ISP_CFAE_INTP_CFG0, val);

	val = ((cfa_info.weight_control_bypass & 0x1) << 31) |
			((cfa_info.uni_dir_intplt_thr_new & 0xFFF) << 12) |
			((cfa_info.strong_edge_thr & 0xFF) << 4) |
			((cfa_info.grid_gain_new & 0xF));
	ISP_REG_WR(idx, ISP_CFAE_INTP_CFG1, val);

	val = (cfa_info.cdcr_adj_factor & 0x3F) |
			((cfa_info.smooth_area_thr & 0x1FFFF) << 8);
	ISP_REG_WR(idx, ISP_CFAE_INTP_CFG2, val);

	val = ((cfa_info.grid_dir_weight_t2 & 0x1F) << 20) |
			((cfa_info.grid_dir_weight_t1 & 0x1F) << 12) |
			(cfa_info.readblue_high_sat_thr & 0x3FF);
	ISP_REG_WR(idx, ISP_CFAE_INTP_CFG3, val);

	val = (cfa_info.round_diff_03_thr & 0xFFF) |
			((cfa_info.low_lux_03_thr & 0x3FF) << 16);
	ISP_REG_WR(idx, ISP_CFAE_INTP_CFG4, val);

	val = (cfa_info.round_diff_12_thr & 0xFFF) |
			((cfa_info.low_lux_12_thr & 0x3FF) << 16);
	ISP_REG_WR(idx, ISP_CFAE_INTP_CFG5, val);

	ISP_REG_MWR(idx, ISP_CFAE_NEW_CFG0, BIT_2, cfa_info.css_bypass << 2);

	val = (cfa_info.css_weak_edge_thr & 0x1FFF) |
			((cfa_info.css_edge_thr & 0x1FFF) << 16);
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG0, val);

	val = (cfa_info.css_texture1_thr & 0x1FFF) |
			((cfa_info.css_texture2_thr & 0x1FFF) << 16);
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG1, val);

	val = ((cfa_info.css_gray_thr & 0x3FF) << 20) |
			((cfa_info.css_uv_diff_thr & 0x3FF) << 10) |
			(cfa_info.css_uv_val_thr & 0x3FF);
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG2, val);

	val = ((cfa_info.css_green_weak_edge_thr & 0x3FF) << 20) |
			((cfa_info.css_green_edge_thr & 0x3FF) << 10) |
			(cfa_info.css_pix_similar_thr & 0x3FF);
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG3, val);

	val = ((cfa_info.css_green_flat_thr & 0x3FF) << 20) |
			((cfa_info.css_green_tex2_thr & 0x3FF) << 10) |
			(cfa_info.css_green_tex1_thr & 0x3FF);
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG4, val);

	val = ((cfa_info.css_text1_corr_ratio_r & 0x1FF) << 18) |
			((cfa_info.css_edge_corr_ratio_b & 0x1FF) << 9) |
			(cfa_info.css_edge_corr_ratio_r & 0x1FF);
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG5, val);

	val = ((cfa_info.css_text2_corr_ratio_b & 0x1FF) << 18) |
			((cfa_info.css_text2_corr_ratio_r & 0x1FF) << 9) |
			(cfa_info.css_text1_corr_ratio_b & 0x1FF);
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG6, val);

	val = ((cfa_info.css_wedge_corr_ratio_r & 0x1FF) << 18) |
			((cfa_info.css_flat_corr_ratio_b & 0x1FF) << 9) |
			(cfa_info.css_flat_corr_ratio_r & 0x1FF);
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG7, val);

	val = ((cfa_info.css_alpha_for_tex2 & 0x1F) << 17) |
			(cfa_info.css_wedge_corr_ratio_b & 0x1FF);
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG8, val);

	val = ((cfa_info.css_skin_u_top[0] & 0x3FF) << 20) |
			((cfa_info.css_skin_u_down[0] & 0x3FF) << 10) |
			(cfa_info.css_skin_v_top[0] & 0x3FF);
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG9, val);

	val = ((cfa_info.css_skin_v_down[0] & 0x3FF) << 20) |
			((cfa_info.css_skin_u_top[1] & 0x3FF) << 10) |
			(cfa_info.css_skin_u_down[1] & 0x3FF);
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG10, val);

	val = ((cfa_info.css_skin_v_top[1] & 0x3FF) << 10) |
			(cfa_info.css_skin_v_down[1] & 0x3FF);
	ISP_REG_WR(idx, ISP_CFAE_CSS_CFG11, val);

	pr_debug("ISP_CFAE_GBUF_CFG 0x%x\n", cfa_info.gbuf_addr_max);
	ISP_REG_WR(idx, ISP_CFAE_GBUF_CFG, cfa_info.gbuf_addr_max);

	return ret;
}

int isp_k_cfg_cfa(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -EPERM;
	}

	switch (param->property) {
	case ISP_PRO_CFA_BLOCK:
		ret = isp_k_cfa_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}

