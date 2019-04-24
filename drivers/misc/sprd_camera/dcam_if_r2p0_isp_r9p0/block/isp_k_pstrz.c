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
#define pr_fmt(fmt) "PSTRZ: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_posterize_block
	(struct isp_io_param *param, uint32_t idx)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int i = 0;
	struct isp_dev_posterize_info pstrz_info;

	memset(&pstrz_info, 0x00, sizeof(pstrz_info));

	ret = copy_from_user((void *)&pstrz_info,
		param->property_param, sizeof(pstrz_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	ISP_REG_MWR(idx, ISP_PSTRZ_PARA, BIT_0, pstrz_info.bypass);
	if (pstrz_info.bypass)
		return 0;

	for (i = 0; i < POSTERIZE_NUM - 1; i++) {
		val = ((pstrz_info.posterize_r_data[i+1] & 0xFF)
			| (pstrz_info.posterize_r_data[i] & 0xFF) << 8);
		ISP_REG_WR(idx, (pstrz_info.pstrz_buf_sel ?
			ISP_PSTRZ_R_BUF1_CH0 : ISP_PSTRZ_R_BUF0_CH0) + i * 4,
			val);
	}
	for (i = 0; i < POSTERIZE_NUM - 1; i++) {
		val = ((pstrz_info.posterize_g_data[i+1] & 0xFF)
			| (pstrz_info.posterize_g_data[i] & 0xFF) << 8);
		ISP_REG_WR(idx, (pstrz_info.pstrz_buf_sel ?
			ISP_PSTRZ_G_BUF1_CH0 : ISP_PSTRZ_G_BUF0_CH0) + i * 4,
			val);
	}
	for (i = 0; i < POSTERIZE_NUM - 1; i++) {
		val = ((pstrz_info.posterize_b_data[i+1] & 0xFF)
			| (pstrz_info.posterize_b_data[i] & 0xFF) << 8);
		ISP_REG_WR(idx, (pstrz_info.pstrz_buf_sel ?
			ISP_PSTRZ_B_BUF1_CH0 : ISP_PSTRZ_B_BUF0_CH0) + i * 4,
			val);
	}

	ISP_REG_MWR(idx, ISP_PSTRZ_PARA,
		BIT_3|BIT_2, (pstrz_info.pstrz_buf_sel) << 3
		| (pstrz_info.sample_en << 2));

	return ret;
}

int isp_k_cfg_pstrz(struct isp_io_param *param,
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
	case ISP_PRO_POSTERIZE_BLOCK:
		ret = isp_k_posterize_block(param, com_idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
