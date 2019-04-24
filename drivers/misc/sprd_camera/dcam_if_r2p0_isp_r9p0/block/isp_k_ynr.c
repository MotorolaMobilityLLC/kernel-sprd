/*
 * Copyright (C) 2018-2019 Spreadtrum Communications Inc.
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

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "YNR: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_ynr_block(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_ynr_info ynr;
	uint32_t scene_id = 0;

	memset(&ynr, 0x00, sizeof(ynr));

	scene_id = ISP_GET_SCENE_ID(idx);

	ret = copy_from_user((void *)&ynr,
		param->property_param, sizeof(ynr));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	ISP_REG_MWR(idx, ISP_YNR_CONTRL0, BIT_0, ynr.bypass);
	if (ynr.bypass)
		return 0;
	ISP_REG_MWR(idx, ISP_YNR_CONTRL0, 0xffffffff,
		(ynr.l3_addback_enable << 9) |
		(ynr.l2_addback_enable << 8) |
		(ynr.l1_addback_enable << 7) |
		(ynr.l0_addback_enable << 6) |
		(ynr.l3_blf_en << 5) |
		(ynr.sal_enable << 4) |
		(ynr.l3_wv_nr_enable << 3) |
		(ynr.l2_wv_nr_enable << 2) |
		(ynr.l1_wv_nr_enable << 1));
	ISP_REG_MWR(idx, ISP_YNR_CFG0, 0xffffffff,
		(ynr.blf_range_index << 28) |
		(ynr.blf_dist_weight2 << 24) |
		(ynr.blf_dist_weight1 << 20) |
		(ynr.blf_dist_weight0 << 16) |
		((ynr.blf_range_s4 & 0xf) << 12) |
		((ynr.blf_range_s3 & 0xf) << 8) |
		((ynr.blf_range_s2 & 0xf) << 4) |
		((ynr.blf_range_s1 & 0xf) << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG1, 0xffffffff,
		(ynr.coef_model << 31) |
		(ynr.blf_range_s0_high << 20) |
		(ynr.blf_range_s0_mid << 18) |
		(ynr.blf_range_s0_low << 16) |
		(ynr.lum_thresh1 << 8) |
		(ynr.lum_thresh0 << 0));


	ISP_REG_MWR(idx, ISP_YNR_CFG2, 0xffffffff,
		(ynr.l1_wv_ratio2_low << 24) |
		(ynr.l1_wv_ratio1_low << 16) |
		(ynr.l1_soft_offset_low << 8) |
		(ynr.l1_wv_thr1_low << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG3, 0xffffffff,
		(ynr.l1_wv_ratio_d2_low << 24) |
		(ynr.l1_wv_ratio_d1_low << 16) |
		(ynr.l1_soft_offset_d_low << 8) |
		(ynr.l1_wv_thr_d1_low << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG4, 0xffffffff,
		(ynr.l1_wv_ratio2_mid << 24) |
		(ynr.l1_wv_ratio1_mid << 16) |
		(ynr.l1_soft_offset_mid << 8) |
		(ynr.l1_wv_thr1_mid << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG5, 0xffffffff,
		(ynr.l1_wv_ratio_d2_mid << 24) |
		(ynr.l1_wv_ratio_d1_mid << 16) |
		(ynr.l1_soft_offset_d_mid << 8) |
		(ynr.l1_wv_thr_d1_mid << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG6, 0xffffffff,
		(ynr.l1_wv_ratio2_high << 24) |
		(ynr.l1_wv_ratio1_high << 16) |
		(ynr.l1_soft_offset_high << 8) |
		(ynr.l1_wv_thr1_high << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG7, 0xffffffff,
		(ynr.l1_wv_ratio_d2_high << 24) |
		(ynr.l1_wv_ratio_d1_high << 16) |
		(ynr.l1_soft_offset_d_high << 8) |
		(ynr.l1_wv_thr_d1_high << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG8, 0xffffffff,
		(ynr.l2_wv_ratio2_low << 24) |
		(ynr.l2_wv_ratio1_low << 16) |
		(ynr.l2_soft_offset_low << 8) |
		(ynr.l2_wv_thr1_low << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG9, 0xffffffff,
		(ynr.l2_wv_ratio_d2_low << 24) |
		(ynr.l2_wv_ratio_d1_low << 16) |
		(ynr.l2_soft_offset_d_low << 8) |
		(ynr.l2_wv_thr_d1_low << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG10, 0xffffffff,
		(ynr.l2_wv_ratio2_mid << 24) |
		(ynr.l2_wv_ratio1_mid << 16) |
		(ynr.l2_soft_offset_mid << 8) |
		(ynr.l2_wv_thr1_mid << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG11, 0xffffffff,
		(ynr.l2_wv_ratio_d2_mid << 24) |
		(ynr.l2_wv_ratio_d1_mid << 16) |
		(ynr.l2_soft_offset_d_mid << 8) |
		(ynr.l2_wv_thr_d1_mid << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG12, 0xffffffff,
		(ynr.l2_wv_ratio2_high << 24) |
		(ynr.l2_wv_ratio1_high << 16) |
		(ynr.l2_soft_offset_high << 8) |
		(ynr.l2_wv_thr1_high << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG13, 0xffffffff,
		(ynr.l2_wv_ratio_d2_high << 24) |
		(ynr.l2_wv_ratio_d1_high << 16) |
		(ynr.l2_soft_offset_d_high << 8) |
		(ynr.l2_wv_thr_d1_high << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG14, 0xffffffff,
		(ynr.l3_wv_ratio2_low << 24) |
		(ynr.l3_wv_ratio1_low << 16) |
		(ynr.l3_soft_offset_low << 8) |
		(ynr.l3_wv_thr1_low << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG15, 0xffffffff,
		(ynr.l3_wv_ratio_d2_low << 24) |
		(ynr.l3_wv_ratio_d1_low << 16) |
		(ynr.l3_soft_offset_d_low << 8) |
		(ynr.l3_wv_thr_d1_low << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG16, 0xffffffff,
		(ynr.l3_wv_ratio2_mid << 24) |
		(ynr.l3_wv_ratio1_mid << 16) |
		(ynr.l3_soft_offset_mid << 8) |
		(ynr.l3_wv_thr1_mid << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG17, 0xffffffff,
		(ynr.l3_wv_ratio_d2_mid << 24) |
		(ynr.l3_wv_ratio_d1_mid << 16) |
		(ynr.l3_soft_offset_d_mid << 8) |
		(ynr.l3_wv_thr_d1_mid << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG18, 0xffffffff,
		(ynr.l3_wv_ratio2_high << 24) |
		(ynr.l3_wv_ratio1_high << 16) |
		(ynr.l3_soft_offset_high << 8) |
		(ynr.l3_wv_thr1_high << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG19, 0xffffffff,
		(ynr.l3_wv_ratio_d2_high << 24) |
		(ynr.l3_wv_ratio_d1_high << 16) |
		(ynr.l3_soft_offset_d_high << 8) |
		(ynr.l3_wv_thr_d1_high << 0));
	ISP_REG_MWR(idx, ISP_YNR_CFG20, 0xffffffff,
		(ynr.l3_wv_thr2_high << 24) |
		(ynr.l3_wv_thr2_mid << 21) |
		(ynr.l3_wv_thr2_low << 18) |
		(ynr.l2_wv_thr2_high << 15) |
		(ynr.l2_wv_thr2_mid << 12) |
		(ynr.l2_wv_thr2_low << 9) |
		(ynr.l1_wv_thr2_high << 6) |
		(ynr.l1_wv_thr2_mid << 3) |
		(ynr.l1_wv_thr2_low << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG21, 0xffffffff,
		(ynr.l3_wv_thr_d2_high << 24) |
		(ynr.l3_wv_thr_d2_mid << 21) |
		(ynr.l3_wv_thr_d2_low << 18) |
		(ynr.l2_wv_thr_d2_high << 15) |
		(ynr.l2_wv_thr_d2_mid << 12) |
		(ynr.l2_wv_thr_d2_low << 9) |
		(ynr.l1_wv_thr_d2_high << 6) |
		(ynr.l1_wv_thr_d2_mid << 3) |
		(ynr.l1_wv_thr_d2_low << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG22, 0xffffffff,
		(ynr.l1_addback_ratio << 24) |
		(ynr.l1_addback_clip << 16) |
		(ynr.l0_addback_ratio << 8) |
		(ynr.l0_addback_clip << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG23, 0xffffffff,
		(ynr.l3_addback_ratio << 24) |
		(ynr.l3_addback_clip << 16) |
		(ynr.l2_addback_ratio << 8) |
		(ynr.l2_addback_clip << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG24, 0xffffffff,
		(ynr.lut_thresh3 << 24) |
		(ynr.lut_thresh2 << 16) |
		(ynr.lut_thresh1 << 8) |
		(ynr.lut_thresh0 << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG25, 0xffffffff,
		(ynr.lut_thresh6 << 16) |
		(ynr.lut_thresh5 << 8) |
		(ynr.lut_thresh4 << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG26, 0xffffffff,
		(ynr.sal_offset3 << 24) |
		(ynr.sal_offset2 << 16) |
		(ynr.sal_offset1 << 8) |
		(ynr.sal_offset0 << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG27, 0xffffffff,
		(ynr.sal_offset7 << 24) |
		(ynr.sal_offset6 << 16) |
		(ynr.sal_offset5 << 8) |
		(ynr.sal_offset4 << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG28, 0xffffffff,
		(ynr.sal_nr_str3 << 24) |
		(ynr.sal_nr_str2 << 16) |
		(ynr.sal_nr_str1 << 8) |
		(ynr.sal_nr_str0 << 0)
		);

	ISP_REG_MWR(idx, ISP_YNR_CFG29, 0xffffffff,
		(ynr.sal_nr_str7 << 24) |
		(ynr.sal_nr_str6 << 16) |
		(ynr.sal_nr_str5 << 8) |
		(ynr.sal_nr_str4 << 0));

	ISP_REG_MWR(idx, ISP_YNR_CFG32, 0xffffffff,
		(ynr.dis_interval << 16) |
		(ynr.radius << 0));

	return ret;
}

static int isp_k_ynr_slice(struct isp_io_param *param, uint32_t idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct img_signed_offset img;

	ret = copy_from_user((void *)&img, param->property_param, sizeof(img));

	if (ret != 0) {
		pr_err("fail to copy_from_user, ret = 0x%x\n",
			(unsigned int)ret);
		return -1;
	}

	val = (img.offset_col & 0xFFFF) | ((img.offset_row & 0xFFFF) << 16);
	ISP_REG_WR(idx, ISP_YNR_CFG31, val);

	val = (img.slice_width & 0xFFFF) | ((img.slice_height & 0xFFFF) << 16);
	ISP_REG_WR(idx, ISP_YNR_CFG33, val);

	return ret;
}

int isp_k_cfg_ynr(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t com_idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_YNR_BLOCK:
		ret = isp_k_ynr_block(param, isp_k_param, com_idx);
		break;
	case ISP_PRO_YNR_SLICE:
		ret = isp_k_ynr_slice(param, com_idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
