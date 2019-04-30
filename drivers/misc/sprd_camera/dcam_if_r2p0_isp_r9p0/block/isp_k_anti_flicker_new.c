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
#include <asm/cacheflush.h>
#include <video/sprd_mm.h>

#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "AFL NEW: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static unsigned int get_afl_data_num(unsigned int afl_stepy,
		unsigned int afl_frm_num, unsigned int ImgHeight)
{
	unsigned int data_long = 0;
	unsigned int v_counter = 0;
	unsigned int v_factor = afl_stepy;
	unsigned int row = 0;

	for (row = 0; row < ImgHeight; row++) {
		if (row == ((v_counter + v_counter_interval) >> 20)) {
			data_long++;
			v_counter += v_factor;
		}
	}
	/*statistics on the 64bit data num for one frame*/
	if (data_long % 2 == 0)
		data_long = data_long / 2;
	else
		data_long = data_long / 2 + 1;
	/*statistics on the 64bit data num for one frame*/
	data_long = data_long * afl_frm_num;

	return data_long;

}

static int get_afl_region_data_num(unsigned int step_y_region,
		unsigned int afl_frm_num, unsigned int ImgHeight)
{
	unsigned int data_long = 0;
	unsigned int v_counter = 0;
	unsigned int v_factor = step_y_region;
	unsigned int row = 0;

	for (row = 0; row < ImgHeight; row++) {
		if (row == ((v_counter + v_counter_interval) >> 20)) {
			data_long += 4;
			v_counter += v_factor;
		}
	}
	/*add 8,32bit flatness data for one frame*/
	data_long = data_long + 8;
	/*statistics on the 64bit data num for one frame*/
	data_long = data_long / 2;
	/*statistic on the 64bit data for every periods*/
	data_long = data_long * afl_frm_num;

	return data_long;
}

static int isp_k_anti_flicker_new_block(struct isp_io_param *param,
		enum isp_id idx)
{
	int ret = 0, val = 0;
	unsigned int afl_glb_total_num = 0;
	unsigned int afl_region_total_num = 0;
	struct isp_dev_anti_flicker_new_info afl_info;

	memset(&afl_info, 0x00, sizeof(afl_info));
	ret = copy_from_user((void *)&afl_info, param->property_param,
			sizeof(afl_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	DCAM_REG_MWR(idx, ISP_AFL_FRM_CTRL0, BIT_0, afl_info.bypass);
	DCAM_REG_MWR(idx, ISP_AFL_PARAM0, BIT_1, afl_info.bypass << 1);
	if (afl_info.bypass)
		return 0;

	DCAM_REG_MWR(idx, DCAM_CONTROL, BIT_13, 0x2000);

	val = ((afl_info.bayer2y_chanel & 0x3) << 6) |
		((afl_info.bayer2y_mode & 0x3) << 4);
	DCAM_REG_MWR(idx, ISP_AFL_PARAM0, 0xF0, val);

	DCAM_REG_MWR(idx, ISP_AFL_FRM_CTRL0, 0xF << 4,
		(afl_info.skip_frame_num & 0xF) << 4);

	DCAM_REG_MWR(idx, ISP_AFL_FRM_CTRL0, BIT_2, afl_info.mode << 2);
	if (afl_info.mode)
		DCAM_REG_MWR(idx, ISP_AFL_FRM_CTRL0, BIT_3, (0x1 << 3));
	else
		DCAM_REG_MWR(idx, ISP_AFL_FRM_CTRL1, BIT_0, 0x1);

	DCAM_REG_WR(idx, ISP_AFL_PARAM1,
		(afl_info.afl_stepx & 0xFFFFFF));
	DCAM_REG_WR(idx, ISP_AFL_PARAM2,
		(afl_info.afl_stepy & 0xFFFFFF));

	DCAM_REG_MWR(idx, ISP_AFL_COL_POS,
		0x1FFF, afl_info.start_col);
	DCAM_REG_MWR(idx, ISP_AFL_COL_POS,
		0x1FFF << 16, afl_info.end_col << 16);

	DCAM_REG_WR(idx, ISP_AFL_REGION0,
		afl_info.step_x_region & 0xFFFFFF);
	DCAM_REG_WR(idx, ISP_AFL_REGION1,
		afl_info.step_y_region & 0XFFFFFF);

	DCAM_REG_MWR(idx, ISP_AFL_REGION2,
		0x1FFF, afl_info.step_x_start_region);
	DCAM_REG_MWR(idx, ISP_AFL_REGION2,
		0x1FFF << 16, afl_info.step_x_end_region << 16);

	afl_glb_total_num = get_afl_data_num(afl_info.afl_stepy,
		afl_info.frame_num, afl_info.img_size.height);

	DCAM_REG_WR(idx, ISP_AFL_SUM1,
		afl_glb_total_num & 0xFFFFF);

	afl_region_total_num =
		get_afl_region_data_num(afl_info.step_y_region,
		afl_info.frame_num, afl_info.img_size.height);

	DCAM_REG_WR(idx, ISP_AFL_SUM2,
		afl_region_total_num & 0xFFFFF);

	DCAM_REG_MWR(idx, ISP_AFL_FRM_CTRL0,
		0xFF0000, (afl_info.frame_num & 0xFF) << 16);

	return ret;
}

static int isp_k_afl_new_bypass(struct isp_io_param *param,
		enum isp_id idx)
{
	int ret = 0;
	unsigned int bypass = 0;

	ret = copy_from_user((void *)&bypass,
		param->property_param, sizeof(bypass));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	DCAM_REG_MWR(idx, ISP_AFL_FRM_CTRL0,
		BIT_0, bypass);

	return ret;
}

int isp_k_cfg_anti_flicker_new(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t com_idx)
{
	int ret = 0;
	enum isp_id idx = ISP_ID_0;

	if (!param) {
		pr_err("fail to get param\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -1;
	}

	idx = ISP_GET_ISP_ID(com_idx);
	switch (param->property) {
	case ISP_PRO_ANTI_FLICKER_NEW_BLOCK:
		ret = isp_k_anti_flicker_new_block(param, idx);
		break;
	case ISP_PRO_ANTI_FLICKER_NEW_BYPASS:
		ret = isp_k_afl_new_bypass(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
