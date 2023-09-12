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
#include <sprd_mm.h>
#include "isp_hw.h"
#include "isp_reg.h"
#include "cam_types.h"
#include "cam_block.h"
#include "isp_core.h"
#include "cam_queue.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "YNR: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int isp_k_ynr_block(struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	uint32_t val = 0;
	struct isp_dev_ynr_info_v2 *ynr = NULL;
	if (isp_k_param->ynr_info_v2_base.isupdate == 0)
		return ret;

	ynr = &isp_k_param->ynr_info_v2_base;
	isp_k_param->ynr_info_v2_base.isupdate = 0;

	if (g_isp_bypass[idx] & (1 << _EISP_YNR))
		ynr->bypass = 1;

	memcpy(&isp_k_param->ynr_info_v2, ynr, sizeof(struct isp_dev_ynr_info_v2));
	if (ynr->bypass)
		return 0;

	val = (ynr->l3_addback_enable << 9) |
		(ynr->l2_addback_enable << 8) |
		(ynr->l1_addback_enable << 7) |
		(ynr->l0_addback_enable << 6) |
		(ynr->l3_blf_en << 5) |
		(ynr->sal_enable << 4) |
		(ynr->l3_wv_nr_enable << 3) |
		(ynr->l2_wv_nr_enable << 2) |
		(ynr->l1_wv_nr_enable << 1);
	ISP_REG_WR(idx, ISP_YNR_CONTRL0, val);

	val = (ynr->blf_range_index << 28) |
		(ynr->blf_dist_weight2 << 24) |
		(ynr->blf_dist_weight1 << 20) |
		(ynr->blf_dist_weight0 << 16) |
		((ynr->blf_range_s4 & 0xf) << 12) |
		((ynr->blf_range_s3 & 0xf) << 8) |
		((ynr->blf_range_s2 & 0xf) << 4) |
		((ynr->blf_range_s1 & 0xf) << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG0, val);

	val = (ynr->coef_model << 31) |
		(ynr->blf_range_s0_high << 20) |
		(ynr->blf_range_s0_mid << 18) |
		(ynr->blf_range_s0_low << 16) |
		(ynr->lum_thresh1 << 8) |
		(ynr->lum_thresh0 << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG1, val);

	val = (ynr->l1_wv_ratio2_low << 24) |
		(ynr->l1_wv_ratio1_low << 16) |
		(ynr->l1_soft_offset_low << 8) |
		(ynr->l1_wv_thr1_low << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG2, val);

	val = (ynr->l1_wv_ratio_d2_low << 24) |
		(ynr->l1_wv_ratio_d1_low << 16) |
		(ynr->l1_soft_offset_d_low << 8) |
		(ynr->l1_wv_thr_d1_low << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG3, val);

	val = (ynr->l1_wv_ratio2_mid << 24) |
		(ynr->l1_wv_ratio1_mid << 16) |
		(ynr->l1_soft_offset_mid << 8) |
		(ynr->l1_wv_thr1_mid << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG4, val);

	val = (ynr->l1_wv_ratio_d2_mid << 24) |
		(ynr->l1_wv_ratio_d1_mid << 16) |
		(ynr->l1_soft_offset_d_mid << 8) |
		(ynr->l1_wv_thr_d1_mid << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG5, val);

	val = (ynr->l1_wv_ratio2_high << 24) |
		(ynr->l1_wv_ratio1_high << 16) |
		(ynr->l1_soft_offset_high << 8) |
		(ynr->l1_wv_thr1_high << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG6, val);

	val = (ynr->l1_wv_ratio_d2_high << 24) |
		(ynr->l1_wv_ratio_d1_high << 16) |
		(ynr->l1_soft_offset_d_high << 8) |
		(ynr->l1_wv_thr_d1_high << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG7, val);

	val = (ynr->l2_wv_ratio2_low << 24) |
		(ynr->l2_wv_ratio1_low << 16) |
		(ynr->l2_soft_offset_low << 8) |
		(ynr->l2_wv_thr1_low << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG8, val);

	val = (ynr->l2_wv_ratio_d2_low << 24) |
		(ynr->l2_wv_ratio_d1_low << 16) |
		(ynr->l2_soft_offset_d_low << 8) |
		(ynr->l2_wv_thr_d1_low << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG9, val);

	val = (ynr->l2_wv_ratio2_mid << 24) |
		(ynr->l2_wv_ratio1_mid << 16) |
		(ynr->l2_soft_offset_mid << 8) |
		(ynr->l2_wv_thr1_mid << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG10, val);

	val = (ynr->l2_wv_ratio_d2_mid << 24) |
		(ynr->l2_wv_ratio_d1_mid << 16) |
		(ynr->l2_soft_offset_d_mid << 8) |
		(ynr->l2_wv_thr_d1_mid << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG11, val);

	val = (ynr->l2_wv_ratio2_high << 24) |
		(ynr->l2_wv_ratio1_high << 16) |
		(ynr->l2_soft_offset_high << 8) |
		(ynr->l2_wv_thr1_high << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG12, val);

	val = (ynr->l2_wv_ratio_d2_high << 24) |
		(ynr->l2_wv_ratio_d1_high << 16) |
		(ynr->l2_soft_offset_d_high << 8) |
		(ynr->l2_wv_thr_d1_high << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG13, val);

	val = (ynr->l3_wv_ratio2_low << 24) |
		(ynr->l3_wv_ratio1_low << 16) |
		(ynr->l3_soft_offset_low << 8) |
		(ynr->l3_wv_thr1_low << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG14, val);

	val = (ynr->l3_wv_ratio_d2_low << 24) |
		(ynr->l3_wv_ratio_d1_low << 16) |
		(ynr->l3_soft_offset_d_low << 8) |
		(ynr->l3_wv_thr_d1_low << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG15, val);

	val = (ynr->l3_wv_ratio2_mid << 24) |
		(ynr->l3_wv_ratio1_mid << 16) |
		(ynr->l3_soft_offset_mid << 8) |
		(ynr->l3_wv_thr1_mid << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG16, val);

	val = (ynr->l3_wv_ratio_d2_mid << 24) |
		(ynr->l3_wv_ratio_d1_mid << 16) |
		(ynr->l3_soft_offset_d_mid << 8) |
		(ynr->l3_wv_thr_d1_mid << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG17, val);

	val = (ynr->l3_wv_ratio2_high << 24) |
		(ynr->l3_wv_ratio1_high << 16) |
		(ynr->l3_soft_offset_high << 8) |
		(ynr->l3_wv_thr1_high << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG18, val);

	val = (ynr->l3_wv_ratio_d2_high << 24) |
		(ynr->l3_wv_ratio_d1_high << 16) |
		(ynr->l3_soft_offset_d_high << 8) |
		(ynr->l3_wv_thr_d1_high << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG19, val);

	val = (ynr->l3_wv_thr2_high << 24) |
		(ynr->l3_wv_thr2_mid  << 21) |
		(ynr->l3_wv_thr2_low << 18) |
		(ynr->l2_wv_thr2_high << 15) |
		(ynr->l2_wv_thr2_mid << 12) |
		(ynr->l2_wv_thr2_low << 9) |
		(ynr->l1_wv_thr2_high << 6) |
		(ynr->l1_wv_thr2_mid << 3) |
		(ynr->l1_wv_thr2_low << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG20, val);

	val = (ynr->l3_wv_thr_d2_high << 24) |
		(ynr->l3_wv_thr_d2_mid << 21) |
		(ynr->l3_wv_thr_d2_low << 18) |
		(ynr->l2_wv_thr_d2_high << 15) |
		(ynr->l2_wv_thr_d2_mid << 12) |
		(ynr->l2_wv_thr_d2_low << 9) |
		(ynr->l1_wv_thr_d2_high << 6) |
		(ynr->l1_wv_thr_d2_mid << 3) |
		(ynr->l1_wv_thr_d2_low << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG21, val);

	val = (ynr->l1_addback_ratio << 24) |
		(ynr->l1_addback_clip << 16) |
		(ynr->l0_addback_ratio << 8) |
		(ynr->l0_addback_clip << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG22, val);

	val = (ynr->l3_addback_ratio << 24) |
		(ynr->l3_addback_clip << 16) |
		(ynr->l2_addback_ratio << 8) |
		(ynr->l2_addback_clip << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG23, val);

	val = (ynr->lut_thresh3 << 24) |
		(ynr->lut_thresh2 << 16) |
		(ynr->lut_thresh1 << 8) |
		(ynr->lut_thresh0 << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG24, val);

	val = (ynr->lut_thresh6 << 16) |
		(ynr->lut_thresh5 << 8) |
		(ynr->lut_thresh4 << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG25, val);

	val = (ynr->sal_offset3 << 24) |
		(ynr->sal_offset2 << 16) |
		(ynr->sal_offset1 << 8) |
		(ynr->sal_offset0 << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG26, val);

	val = (ynr->sal_offset7 << 24) |
		(ynr->sal_offset6 << 16) |
		(ynr->sal_offset5 << 8) |
		(ynr->sal_offset4 << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG27, val);

	val = (ynr->sal_nr_str3 << 24) |
		(ynr->sal_nr_str2 << 16) |
		(ynr->sal_nr_str1 << 8) |
		(ynr->sal_nr_str0 << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG28, val);

	val = (ynr->sal_nr_str7 << 24) |
		(ynr->sal_nr_str6 << 16) |
		(ynr->sal_nr_str5 << 8) |
		(ynr->sal_nr_str4 << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG29, val);

	val = (ynr->dis_interval << 16) | (ynr->radius << 0);
	ISP_REG_WR(idx, ISP_YNR_CFG32, val);

	val = (ynr->center_y << 16) | ynr->center_x;
	ISP_REG_WR(idx, ISP_YNR_CFG31, val);

	return ret;
}

int isp_k_cfg_ynr(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_ynr_info_v2 *ynr = NULL;

	ynr = &isp_k_param->ynr_info_v2_base;

	switch (param->property) {
	case ISP_PRO_YNR_BLOCK:
		ret = copy_from_user((void *)ynr, param->property_param, sizeof(struct isp_dev_ynr_info_v2));
		if (ret != 0) {
			pr_err("fail to copy from user, ret = %d\n", ret);
			return ret;
		}

		isp_k_param->ynr_info_v2_base.isupdate = 1;
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}

int isp_k_update_ynr(void *handle)
{
	int ret = 0;
	uint32_t val = 0, center_x = 0, center_y = 0;
	uint32_t radius = 0, radius_limit = 0, dis_interval = 0;
	uint32_t max_radius = 0, max_radius_limit = 0;
	uint32_t idx = 0, new_width = 0, old_width = 0;
	uint32_t new_height = 0, old_height = 0;
	struct isp_dev_ynr_info_v2 *ynr_info = NULL, *pdst = NULL;
	struct isp_sw_context *pctx = NULL;

	if (!handle) {
		pr_err("fail to get invalid in ptr\n");
		return -EFAULT;
	}

	pctx = (struct isp_sw_context *)handle;
	idx = pctx->ctx_id;
	new_width = pctx->isp_using_param->blkparam_info.new_width;
	new_height = pctx->isp_using_param->blkparam_info.new_height;
	old_width = pctx->isp_using_param->blkparam_info.old_width;
	old_height = pctx->isp_using_param->blkparam_info.old_height;

	pdst = &pctx->isp_using_param->ynr_info_v2;
	ynr_info = &pctx->isp_using_param->ynr_info_v2_base;
	if (ynr_info->bypass)
		return 0;

	center_x = new_width >> 1;
	center_y = new_height >> 1;
	val = (center_y << 16) | center_x;
	ISP_REG_WR(idx, ISP_YNR_CFG31, val);

	pdst->center_x = center_x;
	pdst->center_y = center_y;

	radius = ynr_info->radius * new_width / old_width;
	radius_limit = new_height * ynr_info->radius_factor / ynr_info->radius_base;
	radius = (radius < radius_limit) ? radius : radius_limit;

	max_radius = ynr_info->max_radius * new_width / old_width;
	max_radius_limit = (new_height + new_width) *
		ynr_info->max_radius_factor / ynr_info->radius_base;
	max_radius = (max_radius < max_radius_limit) ? max_radius : max_radius_limit;
	dis_interval = ((max_radius - radius) >> 2);

	val = (dis_interval << 16) | radius;
	ISP_REG_WR(idx, ISP_YNR_CFG32, val);

	val = (new_height << 16) | new_width;
	ISP_REG_WR(idx, ISP_YNR_CFG33, val);

	pdst->dis_interval = dis_interval;
	pdst->radius = radius;

	pr_debug("cen %d %d,  orig radius %d %d %d %d, base %d, new %d %d %d\n",
		center_x, center_y, ynr_info->radius, ynr_info->max_radius,
		ynr_info->radius_factor, ynr_info->max_radius_factor,
		ynr_info->radius_base, radius, max_radius, dis_interval);

	return ret;
}

int isp_k_cpy_ynr(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if (isp_k_param->ynr_info_v2_base.isupdate == 1) {
		memcpy(&param_block->ynr_info_v2_base, &isp_k_param->ynr_info_v2_base, sizeof(struct isp_dev_ynr_info_v2));
		isp_k_param->ynr_info_v2_base.isupdate = 0;
		param_block->ynr_info_v2_base.isupdate = 1;
	}

	return ret;
}
