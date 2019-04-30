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

#include <linux/uaccess.h>
#include <video/sprd_mm.h>

#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "HSV: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define ISP_FRGB_HSV_BUF0  0
#define ISP_FRGB_HSV_BUF1  1

static int isp_pingpang_frgb_hsv(struct isp_dev_hsv_info *hsv_info,
			struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	unsigned int i;
	unsigned int val = 0;
	unsigned long dst_addr = 0;
	struct isp_hsv_region_info region_info;
	unsigned long data_ptr = 0;
	int32_t buf_size = 0;
	uint32_t buf_id = 0;
	uint32_t work_mode = 0;
	uint32_t scene_id = 0;

	memset(&region_info, 0x00, sizeof(region_info));
	if (!hsv_info)
		return -1;

	work_mode = ISP_GET_MODE_ID(idx);
	scene_id = ISP_GET_SCENE_ID(idx);

	if (work_mode == ISP_CFG_MODE) {
		dst_addr = ISP_BASE_ADDR(idx) +
			ISP_HSV_BUF0_CH0;
		isp_k_param->hsv_buf_id_prv =
			ISP_FRGB_HSV_BUF0;
		ISP_REG_MWR(idx, ISP_HSV_PARAM, BIT_1,
			isp_k_param->hsv_buf_id_prv << 1);
		buf_id = isp_k_param->hsv_buf_id_prv;
		region_info =
			hsv_info->region_info[buf_id];
	} else {
		if (scene_id == ISP_SCENE_PRE) {
			if (isp_k_param->hsv_buf_id_prv ==
				ISP_FRGB_HSV_BUF1) {
				dst_addr = ISP_BASE_ADDR(idx) +
					ISP_HSV_BUF0_CH0;
				isp_k_param->hsv_buf_id_prv =
					ISP_FRGB_HSV_BUF0;
			} else {
				dst_addr = ISP_BASE_ADDR(idx) +
					ISP_HSV_BUF1_CH0;
				isp_k_param->hsv_buf_id_prv =
					ISP_FRGB_HSV_BUF1;
			}
			ISP_REG_MWR(idx, ISP_HSV_PARAM, BIT_1,
				isp_k_param->hsv_buf_id_prv << 1);
			buf_id = isp_k_param->hsv_buf_id_prv;
			region_info =
				hsv_info->region_info[buf_id];
		} else {
			if (isp_k_param->hsv_buf_id_cap ==
				ISP_FRGB_HSV_BUF1) {
				dst_addr = ISP_BASE_ADDR(idx) +
					ISP_HSV_BUF0_CH0;
				isp_k_param->hsv_buf_id_cap =
					ISP_FRGB_HSV_BUF0;
			} else {
				dst_addr = ISP_BASE_ADDR(idx) +
					ISP_HSV_BUF1_CH0;
				isp_k_param->hsv_buf_id_cap =
					ISP_FRGB_HSV_BUF1;
			}
			ISP_REG_MWR(idx, ISP_HSV_PARAM, BIT_1,
				isp_k_param->hsv_buf_id_cap << 1);
			buf_id = isp_k_param->hsv_buf_id_cap;
			region_info =
				hsv_info->region_info[buf_id];
		}
	}

	for (i = 0; i < 5; i++) {
		val = ((region_info.hrange_left[i] & 0x1FF) << 23) |
				((region_info.s_curve[i][1] & 0x7FF) << 11) |
				(region_info.s_curve[i][0] & 0x7FF);
		ISP_REG_WR(idx, ISP_HSV_CFG0 + i * 12, val);

		val = ((region_info.hrange_right[i] & 0x1FF) << 23) |
				((region_info.s_curve[i][3] & 0x7FF) << 11) |
				(region_info.s_curve[i][2] & 0x7FF);
		ISP_REG_WR(idx, ISP_HSV_CFG1 + i * 12, val);

		val = ((region_info.v_curve[i][3] & 0xFF) << 24) |
				((region_info.v_curve[i][2] & 0xFF) << 16) |
				((region_info.v_curve[i][1] & 0xFF) << 8) |
				(region_info.v_curve[i][0] & 0xFF);
		ISP_REG_WR(idx, ISP_HSV_CFG2 + i*12, val);
	}

	val = ((region_info.r_s[1][0] << 20) |
			(region_info.r_s[0][0] << 9) |
			(region_info.r_v[0][0]));
	ISP_REG_WR(idx, ISP_HSV_CFG15, val);

	val = ((region_info.r_s[3][0] << 20) |
			(region_info.r_s[2][0] << 9) |
			(region_info.r_v[1][0]));
	ISP_REG_WR(idx, ISP_HSV_CFG16, val);

	val = ((region_info.r_v[3][0] << 20) |
			(region_info.r_s[4][0] << 9) |
			(region_info.r_v[2][0]));
	ISP_REG_WR(idx, ISP_HSV_CFG17, val);

	val = ((region_info.r_s[1][1] << 20) |
			(region_info.r_s[0][1] << 9) |
			(region_info.r_v[0][1]));
	ISP_REG_WR(idx, ISP_HSV_CFG18, val);

	val = ((region_info.r_s[3][1] << 20) |
			(region_info.r_s[2][1] << 9) |
			(region_info.r_v[1][1]));
	ISP_REG_WR(idx, ISP_HSV_CFG19, val);

	val = ((region_info.r_v[3][1] << 20) |
			(region_info.r_s[4][1] << 9) |
			(region_info.r_v[2][1]));
	ISP_REG_WR(idx, ISP_HSV_CFG20, val);

	val = ((region_info.r_v[4][1] << 9) |
			(region_info.r_v[4][0]));
	ISP_REG_WR(idx, ISP_HSV_CFG21, val);


#ifdef CONFIG_64BIT
	data_ptr = ((unsigned long)hsv_info->data_ptr[1] << 32)
				| hsv_info->data_ptr[0];
#else
	data_ptr = hsv_info->data_ptr[0];
#endif
	buf_size = ISP_HSV_ITEM * 4;
	if (data_ptr == 0 || hsv_info->size > buf_size) {
		pr_err("fail to get param or buf_size: data_ptr == %ld, buf_size = %d\n",
			data_ptr, hsv_info->size);
		return -1;
	}
	ret = copy_from_user((void *)dst_addr,
		(void __user *)data_ptr, hsv_info->size);
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	return ret;
}

static int isp_k_hsv_block(struct isp_io_param *param,
			struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_hsv_info hsv_info;

	memset(&hsv_info, 0x00, sizeof(hsv_info));
	ret = copy_from_user((void *)&hsv_info,
		param->property_param, sizeof(struct isp_dev_hsv_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}
	ISP_REG_MWR(idx, ISP_HSV_PARAM, BIT_0, hsv_info.bypass);
	if (hsv_info.bypass)
		return 0;
	isp_pingpang_frgb_hsv(&hsv_info, isp_k_param, idx);

	return ret;
}

int isp_k_cfg_hsv(struct isp_io_param *param,
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
	case ISP_PRO_HSV_BLOCK:
		ret = isp_k_hsv_block(param, isp_k_param, com_idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
