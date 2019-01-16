/*
 * Copyright (C) 2017-2018 Spreadtrum Communications Inc.
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
	unsigned int val = 0;
	struct isp_dev_ynr_info ynr;
	unsigned int i = 0;
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

	val = ((ynr.lowlux_bypass & 0x1) << 1)
		| ((ynr.nr_enable & 0x1) << 2)
		| ((ynr.l_blf_en[2] & 0x1) << 3)
		| ((ynr.l_blf_en[1] & 0x1) << 4)
		| ((ynr.l_blf_en[0] & 0x1) << 5);
	ISP_REG_MWR(idx, ISP_YNR_CONTRL0, 0x3E, val);

	val = (ynr.flat_th[1] & 0xFF)
		| ((ynr.flat_th[0] & 0xFF) << 8)
		| ((ynr.txt_th & 0xFF) << 16)
		| ((ynr.edge_th & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_YNR_CFG0, val);

	val = (ynr.flat_th[5] & 0xFF)
		| ((ynr.flat_th[4] & 0xFF) << 8)
		| ((ynr.flat_th[3] & 0xFF) << 16)
		| ((ynr.flat_th[2] & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_YNR_CFG1, val);

	val = (ynr.lut_th[2] & 0xFF)
		| ((ynr.lut_th[1] & 0xFF) << 8)
		| ((ynr.lut_th[0] & 0xFF) << 16)
		| ((ynr.flat_th[6] & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_YNR_CFG2, val);

	val = (ynr.lut_th[6] & 0xFF)
		| ((ynr.lut_th[5] & 0xFF) << 8)
		| ((ynr.lut_th[4] & 0xFF) << 16)
		| ((ynr.lut_th[3] & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_YNR_CFG3, val);

	for (i = 0; i < 2; i++) {
		val = (ynr.addback[i * 4 + 3] & 0xFF)
		| ((ynr.addback[i * 4 + 2] & 0xFF) << 8)
		| ((ynr.addback[i * 4 + 1] & 0xFF) << 16)
		| ((ynr.addback[i * 4] & 0xFF) << 24);
		ISP_REG_WR(idx, ISP_YNR_CFG4 + 4*i, val);
	}

	val = (ynr.sub_th[2] & 0x7F)
		| ((ynr.sub_th[1] & 0x7F) << 8)
		| ((ynr.sub_th[0] & 0x7F) << 16)
		| ((ynr.addback[8] & 0xFF) << 24);
	ISP_REG_WR(idx, ISP_YNR_CFG6, val);

	val = (ynr.sub_th[6] & 0x7F)
		| ((ynr.sub_th[5] & 0x7F) << 8)
		| ((ynr.sub_th[4] & 0x7F) << 16)
		| ((ynr.sub_th[3] & 0x7F) << 24);
	ISP_REG_WR(idx, ISP_YNR_CFG7, val);

	val = (ynr.l_euroweight[0][2] & 0xF)
		| ((ynr.l_euroweight[0][1] & 0xF) << 4)
		| ((ynr.l_euroweight[0][0] & 0xF) << 8)
		| ((ynr.sub_th[8] & 0x7F) << 16)
		| ((ynr.sub_th[7] & 0x7F) << 24);
	ISP_REG_WR(idx, ISP_YNR_CFG8, val);

	val = (ynr.l_euroweight[2][2] & 0xF)
		| ((ynr.l_euroweight[2][1] & 0xF) << 4)
		| ((ynr.l_euroweight[2][0] & 0xF) << 8)
		| ((ynr.l_euroweight[1][2] & 0xF) << 12)
		| ((ynr.l_euroweight[1][1] & 0xF) << 16)
		| ((ynr.l_euroweight[1][0] & 0xF) << 20);
	ISP_REG_WR(idx, ISP_YNR_CFG9, val);

	val = (ynr.l0_lut_th1 & 0x7)
		| ((ynr.l0_lut_th0 & 0x7) << 4)
		| ((ynr.l1_txt_th1 & 0x7) << 8)
		| ((ynr.l1_txt_th0 & 0x7) << 12)
		| ((ynr.l_wf_index[2] & 0xF) << 16)
		| ((ynr.l_wf_index[1] & 0xF) << 20)
		| ((ynr.l_wf_index[0] & 0xF) << 24);
	ISP_REG_WR(idx, ISP_YNR_CFG10, val);

	for (i = 0; i < 6; i++) {
		val = (ynr.wlt_th[i * 4 + 3] & 0x7F)
		| ((ynr.wlt_th[i * 4 + 2] & 0x7F) << 8)
		| ((ynr.wlt_th[i * 4 + 1] & 0x7F) << 16)
		| ((ynr.wlt_th[i * 4] & 0x7F) << 24);
		ISP_REG_WR(idx, ISP_YNR_WLT0 + i * 4, val);
	}

	for (i = 0; i < 8; i++) {
		val = (ynr.freq_ratio[i * 3 + 2] & 0x3FF)
		| ((ynr.freq_ratio[i * 3 + 1] & 0x3FF) << 10)
		| ((ynr.freq_ratio[i * 3] & 0x3FF) << 20);
		ISP_REG_WR(idx, ISP_YNR_FRATIO0 + i * 4, val);
	}

	if (scene_id == ISP_SCENE_PRE) {
		ISP_REG_WR(idx, ISP_YNR_CFG11, 0);
		val = (ynr.center.y & 0xFFFF)
			| ((ynr.center.x & 0xFFFF) << 16);
		ISP_REG_WR(idx, ISP_YNR_CFG12, val);
	} else {
		isp_k_param->ynr_center_x = ynr.center.x;
		isp_k_param->ynr_center_y = ynr.center.y;
	}
	val = (ynr.dist_interval & 0xFFFF)
		| ((ynr.radius & 0xFFFF) << 16);
	ISP_REG_WR(idx, ISP_YNR_CFG13, val);

	for (i = 0; i < 2; i++) {
		val = (ynr.sal_nr_str[i * 4 + 3] & 0x7F)
		| ((ynr.sal_nr_str[i * 4 + 2] & 0x7F) << 8)
		| ((ynr.sal_nr_str[i * 4 + 1] & 0x7F) << 16)
		| ((ynr.sal_nr_str[i * 4] & 0x7F) << 24);
		ISP_REG_WR(idx, ISP_YNR_CFG14 + 4*i, val);
	}

	for (i = 0; i < 2; i++) {
		val = (ynr.sal_offset[i * 4 + 3] & 0xFF)
		| ((ynr.sal_offset[i * 4 + 2] & 0xFF) << 8)
		| ((ynr.sal_offset[i * 4 + 1] & 0xFF) << 16)
		| ((ynr.sal_offset[i * 4] & 0xFF) << 24);
		ISP_REG_WR(idx, ISP_YNR_CFG16 + 4 * i, val);
	}

	return ret;
}

static int isp_k_ynr_slice(struct isp_io_param *param, uint32_t idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct img_offset img = {0};

	ret = copy_from_user((void *)&img,
		param->property_param, sizeof(img));
	if (ret != 0) {
		pr_err("fail to copy_from_user, ret = 0x%x\n",
			(unsigned int)ret);
		return -1;
	}

	val = (img.y & 0xFFFF) | ((img.x & 0xFFFF) << 16);
	ISP_REG_WR(idx, ISP_YNR_CFG11, val);

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
