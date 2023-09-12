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
#include "cam_queue.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "HSV: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int isp_k_hsv_block(struct isp_k_block *isp_k_param, uint32_t idx)
{
	int i = 0, j = 0, ret = 0;
	uint32_t val = 0, addr = 0;
	struct isp_dev_hsv_info_v3 *hsv_info = NULL;

	if (isp_k_param->hsv_info3.isupdate == 0)
		return ret;

	hsv_info = &isp_k_param->hsv_info3;
	isp_k_param->hsv_info3.isupdate = 0;

	if (g_isp_bypass[idx] & (1 << _EISP_HSV))
		hsv_info->hsv_bypass = 1;

	ISP_REG_MWR(idx, ISP_HSV_PARAM, BIT_0, hsv_info->hsv_bypass);
	if (hsv_info->hsv_bypass) {
		pr_debug("idx %d, hsv_bypass!\n", idx);
		return 0;
	}

	hsv_info->buf_param.hsv_buf_sel = 0;
	hsv_info->hsv_delta_value_en = 1;

	val = (((hsv_info->buf_param.hsv_buf_sel & 0x1) << 1) |
		((hsv_info->hsv_delta_value_en & 0x1) << 2));
	ISP_REG_MWR(idx, ISP_HSV_PARAM , 0x6, val);

	val = (hsv_info->hsv_hue_thr[0][0] & 0x1F) |
		((hsv_info->hsv_hue_thr[0][1] & 0x1F) << 5) |
		((hsv_info->hsv_hue_thr[1][0] & 0x1F) << 10) |
		((hsv_info->hsv_hue_thr[1][1] & 0x1F) << 15) |
		((hsv_info->hsv_hue_thr[2][0] & 0x1F) << 20) |
		((hsv_info->hsv_hue_thr[2][1] & 0x1F) << 25);
	ISP_REG_MWR(idx, ISP_HSV_CFG0, 0x3FFFFFFF, val);

	hsv_info->hsv_param[0].hsv_curve_param.start_a = 0;
	hsv_info->hsv_param[0].hsv_curve_param.end_a = 0;
	hsv_info->hsv_param[0].hsv_curve_param.start_b = 1023;
	val = (hsv_info->hsv_param[0].hsv_curve_param.start_a & 0x3FF) |
		((hsv_info->hsv_param[0].hsv_curve_param.end_a & 0x3FF) << 10) |
		((hsv_info->hsv_param[0].hsv_curve_param.start_b & 0x3FF) << 20);
	ISP_REG_MWR(idx, ISP_HSV_CFG1, 0x3FFFFFFF, val);

	hsv_info->hsv_param[0].hsv_curve_param.end_b = 1023;
	hsv_info->hsv_param[1].hsv_curve_param.start_a = 0;
	hsv_info->hsv_param[1].hsv_curve_param.end_a = 0;
	val = (hsv_info->hsv_param[0].hsv_curve_param.end_b & 0x3FF) |
		((hsv_info->hsv_param[1].hsv_curve_param.start_a & 0x3FF) << 10) |
		((hsv_info->hsv_param[1].hsv_curve_param.end_a & 0x3FF) << 20);
	ISP_REG_MWR(idx, ISP_HSV_CFG2, 0x3FFFFFFF, val);

	hsv_info->hsv_param[1].hsv_curve_param.start_b = 1023;
	hsv_info->hsv_param[1].hsv_curve_param.end_b = 1023;
	hsv_info->hsv_param[2].hsv_curve_param.start_a = 0;
	val = (hsv_info->hsv_param[1].hsv_curve_param.start_b & 0x3FF) |
		((hsv_info->hsv_param[1].hsv_curve_param.end_b & 0x3FF) << 10) |
		((hsv_info->hsv_param[2].hsv_curve_param.start_a & 0x3FF) << 20);
	ISP_REG_MWR(idx, ISP_HSV_CFG3, 0x3FFFFFFF, val);

	hsv_info->hsv_param[2].hsv_curve_param.end_a = 0;
	hsv_info->hsv_param[2].hsv_curve_param.start_b = 1023;
	hsv_info->hsv_param[2].hsv_curve_param.end_b = 1023;
	val = (hsv_info->hsv_param[2].hsv_curve_param.end_a & 0x3FF) |
		((hsv_info->hsv_param[2].hsv_curve_param.start_b & 0x3FF) << 10) |
		((hsv_info->hsv_param[2].hsv_curve_param.end_b & 0x3FF) << 20);
	ISP_REG_MWR(idx, ISP_HSV_CFG4, 0x3FFFFFFF, val);

	hsv_info->hsv_param[3].hsv_curve_param.start_a = 0;
	hsv_info->hsv_param[3].hsv_curve_param.end_a = 0;
	hsv_info->hsv_param[3].hsv_curve_param.start_b = 1023;
	val = (hsv_info->hsv_param[3].hsv_curve_param.start_a & 0x3FF) |
		((hsv_info->hsv_param[3].hsv_curve_param.end_a & 0x3FF) << 10) |
		((hsv_info->hsv_param[3].hsv_curve_param.start_b & 0x3FF) << 20);
	ISP_REG_MWR(idx, ISP_HSV_CFG5, 0x3FFFFFFF, val);

	hsv_info->hsv_param[3].hsv_curve_param.end_b = 1023;
	val = (hsv_info->hsv_param[3].hsv_curve_param.end_b & 0x3FF) |
		((hsv_info->y_blending_factor & 0x7FF) << 16);
	ISP_REG_MWR(idx, ISP_HSV_CFG6, 0x7FF03FF, val);

	for(i = 0; i < 12; i++) {
		val = (hsv_info->hsv_1d_hue_lut[2*i] & 0x1FF) |
			((hsv_info->hsv_1d_hue_lut[2*i+1] & 0x1FF) << 9) |
			((hsv_info->hsv_1d_sat_lut[i] & 0x1FFF) << 18);
		ISP_REG_MWR(idx, ISP_HSV_CFG7+i*4, 0x7FFFFFFF, val);
	}

	val = (hsv_info->hsv_1d_hue_lut[24] & 0x1FF) |
		((hsv_info->hsv_1d_sat_lut[12] & 0x1FFF) << 16);
	ISP_REG_MWR(idx, ISP_HSV_CFG19, 0x1FFF01FF,  val);
	val = (hsv_info->hsv_1d_sat_lut[13] & 0x1FFF) |
		((hsv_info->hsv_1d_sat_lut[14] & 0x1FFF) << 16);
	ISP_REG_MWR(idx, ISP_HSV_CFG20, 0x1FFF1FFF, val);
	val = (hsv_info->hsv_1d_sat_lut[15] & 0x1FFF) |
		((hsv_info->hsv_1d_sat_lut[16] & 0x1FFF) << 16);
	ISP_REG_MWR(idx, ISP_HSV_CFG21, 0x1FFF1FFF, val);

	for (i = 0; i < 25; i++) {
		for (j = 0; j < 17; j++) {
			val = 0;
			if (((i + 1) % 2 == 1) && ((j + 1) % 2 == 1)) {
				addr = ((i >> 1) * 9 + (j >> 1)) * 4;
				val = (hsv_info->buf_param.hsv_2d_hue_lut_reg[i][j] & 0x1FF)
					| ((hsv_info->buf_param.hsv_2d_sat_lut[i][j] & 0x1FFF) << 9);
				ISP_REG_WR(idx, ISP_HSV_A_BUF0_CH0 + addr, val);
			} else if (((i + 1) % 2 == 1) && ((j + 1) % 2 != 1)) {
				addr = ((i >> 1) * 8 + (j >> 1)) * 4;
				val = (hsv_info->buf_param.hsv_2d_hue_lut_reg[i][j] & 0x1FF)
					| ((hsv_info->buf_param.hsv_2d_sat_lut[i][j] & 0x1FFF) << 9);
				ISP_REG_WR(idx, ISP_HSV_B_BUF0_CH0 +addr, val);
			} else if (((i + 1) % 2 != 1) && ((j + 1) % 2 == 1)) {
				addr = ((i >> 1) * 9 + (j >> 1)) * 4;
				val = (hsv_info->buf_param.hsv_2d_hue_lut_reg[i][j] & 0x1FF)
					| ((hsv_info->buf_param.hsv_2d_sat_lut[i][j] & 0x1FFF) << 9);
				ISP_REG_WR(idx, ISP_HSV_C_BUF0_CH0 +addr, val);
			} else if (((i + 1) % 2 != 1) && ((j + 1) % 2 != 1)) {
				addr = ((i >> 1) * 8 + (j >> 1)) * 4;
				val = (hsv_info->buf_param.hsv_2d_hue_lut_reg[i][j] & 0x1FF)
					| ((hsv_info->buf_param.hsv_2d_sat_lut[i][j] & 0x1FFF) << 9);
				ISP_REG_WR(idx, ISP_HSV_D_BUF0_CH0 +addr, val);
			}
		}
	}

	return ret;
}

int isp_k_cfg_hsv(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_hsv_info_v3 *hsv_info = NULL;

	hsv_info = &isp_k_param->hsv_info3;

	switch (param->property) {
	case ISP_PRO_HSV_BLOCK:
		ret = copy_from_user((void *)hsv_info, param->property_param, sizeof(struct isp_dev_hsv_info_v3));
		if (ret != 0) {
			pr_err("fail to copy from user, ret = %d\n", ret);
			return ret;
		}
		isp_k_param->hsv_info3.isupdate = 1;
		break;
	default:
		pr_err("fail to idx %d, support cmd id = %d\n", idx, param->property);
		break;
	}

	return ret;
}

int isp_k_cpy_hsv(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if (isp_k_param->hsv_info3.isupdate == 1) {
		memcpy(&param_block->hsv_info3, &isp_k_param->hsv_info3, sizeof(struct isp_dev_hsv_info_v3));
		isp_k_param->hsv_info3.isupdate = 0;
		param_block->hsv_info3.isupdate = 1;
	}

	return ret;
}
