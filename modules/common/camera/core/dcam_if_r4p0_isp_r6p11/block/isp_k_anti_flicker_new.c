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
#include <asm/cacheflush.h>

#include "sprd_mm.h"
#include "sprd_isp_hw.h"
#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "AFL NEW: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_afl_new_bypass(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param,
			sizeof(bypass));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}
	ISP_REG_MWR(idx, ISP_ANTI_FLICKER_NEW_PARAM0, BIT_0, bypass);
	ISP_REG_MWR(idx, ISP_ANTI_FLICKER_NEW_CFG_READY, BIT_0, 1);
	if (bypass)
		isp_dbg_s_ori_byp(SBLK_BYPASS, yuv_afl, idx);

	return ret;
}

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
	if (data_long%2 == 0)
		data_long = data_long/2;
	else
		data_long = data_long/2+1;
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
	data_long = data_long/2;
	/*statistic on the 64bit data for every periods*/
	data_long = data_long * afl_frm_num;

	return data_long;
}

static int isp_k_anti_flicker_new_block(struct isp_io_param *param,
					enum isp_id idx)
{
	int ret = 0;
	unsigned int  afl_glb_total_num;
	unsigned int  afl_region_total_num;
	struct isp_dev_anti_flicker_new_info afl_info;

	memset(&afl_info, 0x00, sizeof(afl_info));
	ret = copy_from_user((void *)&afl_info, param->property_param,
			sizeof(afl_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	/* enable new anti flicker moudle */
	ISP_REG_MWR(idx, ISP_ANTI_FLICKER_NEW_PARAM0, BIT_0, afl_info.bypass);
	if (afl_info.bypass) {
		ISP_REG_WR(idx, ISP_ANTI_FLICKER_NEW_CFG_READY, BIT_0);
		isp_dbg_s_ori_byp(SBLK_BYPASS, yuv_afl, idx);
		return 0;
	}

	ISP_HREG_MWR(idx, ISP_COMMON_BIN_AFL_CTRL, BIT_1, 1 << 1);

	ISP_REG_MWR(idx, ISP_ANTI_FLICKER_NEW_PARAM0,
				BIT_1, afl_info.mode << 1);
	ISP_REG_MWR(idx, ISP_ANTI_FLICKER_NEW_PARAM0, 0xF0,
				afl_info.skip_frame_num << 4);

	ISP_REG_MWR(idx, ISP_ANTI_FLICKER_NEW_PARAM1,
				0xFFFFFF, afl_info.afl_stepx);

	ISP_REG_MWR(idx, ISP_ANTI_FLICKER_NEW_PARAM2,
				0xFFFFFF, afl_info.afl_stepy);
	ISP_REG_MWR(idx, ISP_ANTI_FLICKER_NEW_PARAM2,
				0xFF << 24, afl_info.frame_num << 24);

	ISP_REG_MWR(idx, ISP_ANTI_FLICKER_NEW_COL_POS,
				0xFFFF, afl_info.start_col);
	ISP_REG_MWR(idx, ISP_ANTI_FLICKER_NEW_COL_POS,
				0xFFFF << 16, afl_info.end_col << 16);

	ISP_REG_WR(idx, ISP_ANTI_FLICKER_NEW_REGION0,
				(afl_info.step_x_region & 0xFFFFFF));
	ISP_REG_WR(idx, ISP_ANTI_FLICKER_NEW_REGION1,
				(afl_info.step_y_region & 0xFFFFFF));

	ISP_REG_MWR(idx, ISP_ANTI_FLICKER_NEW_REGION2, 0xFFFF,
				afl_info.step_x_start_region);
	ISP_REG_MWR(idx, ISP_ANTI_FLICKER_NEW_REGION2, 0xFFFF << 16,
				afl_info.step_x_end_region << 16);

	afl_glb_total_num = get_afl_data_num(afl_info.afl_stepy,
				afl_info.frame_num, afl_info.img_size.height);

	ISP_REG_MWR(idx, ISP_ANTI_FLICKER_NEW_SUM1,
				0xFFFFF, afl_glb_total_num);

	afl_region_total_num = get_afl_region_data_num(afl_info.step_y_region,
				afl_info.frame_num, afl_info.img_size.height);
	ISP_REG_MWR(idx, ISP_ANTI_FLICKER_NEW_SUM2,
				0xFFFFF, afl_region_total_num);

	ISP_REG_MWR(idx, ISP_ANTI_FLICKER_NEW_CFG_READY, BIT_0, 1);

	return ret;
}


int isp_k_cfg_anti_flicker_new(struct isp_io_param *param,
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

