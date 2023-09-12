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
#include "sprd_isp_r6p91.h"
#include "isp_reg.h"
#include "isp_block.h"

#define ISP_RAWAFM_TIMEOUT msecs_to_jiffies(1000)

/*TODO: replace sem to com*/
/*#define DEBUG*/

static int32_t isp_k_raw_afm_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t i = 0;
	int max_num = 0;
	unsigned long  addr = 0;
	struct isp_dev_rgb_afm_info rafm_info;

	memset(&rafm_info, 0x00, sizeof(rafm_info));

	ret = copy_from_user((void *)&rafm_info,
		param->property_param, sizeof(rafm_info));
	if (ret != 0) {
		pr_info("error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_0, rafm_info.bypass);
	if (rafm_info.bypass)
		return 0;

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_1, rafm_info.mode << 1);

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, (BIT_2 | BIT_3 | BIT_4 | BIT_5),
		(rafm_info.skip_num << 2));

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_6,
		rafm_info.skip_num_clear << 6);

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_7,
		rafm_info.spsmd_rtgbot_enable << 7);

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_8,
		rafm_info.spsmd_diagonal_enable << 8);

	max_num = ISP_AFM_WIN_NUM;
	addr = ISP_RGB_AFM_WIN_RANGE0S;

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_9,
		rafm_info.spsmd_square_en << 9);

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_10,
		1 << 10);

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_11,
		0 << 11);

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_12,
		rafm_info.overflow_protect_en << 12);

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_13,
		rafm_info.subfilter.average << 13);

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_14,
		rafm_info.subfilter.median << 14);

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_15,
		rafm_info.spsmd_touch_mode << 15);

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, (BIT_18)
		| (BIT_17) | (BIT_16),
		rafm_info.shift.shift_spsmd << 16);

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, (BIT_21) | (BIT_20) | (BIT_19),
		rafm_info.shift.shift_sobel9 << 19);

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, (BIT_27) | (BIT_26) | (BIT_25),
		rafm_info.shift.shift_sobel5 << 25);

	val = ((rafm_info.thrd.sobel5_thr_max_red << 16) & 0x3FFF0000)
		| (rafm_info.thrd.sobel5_thr_min_red & 0x0FFF);
	ISP_REG_WR(ISP_RGB_AFM_SOBEL5_THR_R, val);

	val = ((rafm_info.thrd.sobel5_thr_max_green << 16) & 0x3FFF0000)
		| (rafm_info.thrd.sobel5_thr_min_green & 0x0FFF);
	ISP_REG_WR(ISP_RGB_AFM_SOBEL5_THR_G, val);

	val = ((rafm_info.thrd.sobel5_thr_max_blue << 16) & 0x3FFF0000)
		| (rafm_info.thrd.sobel5_thr_min_blue & 0x0FFF);
	ISP_REG_WR(ISP_RGB_AFM_SOBEL5_THR_B, val);

	val = ((rafm_info.thrd.sobel9_thr_max_red << 16) & 0x3FFF0000)
		| (rafm_info.thrd.sobel9_thr_min_red & 0x0FFF);
	ISP_REG_WR(ISP_RGB_AFM_SOBEL9_THR_R, val);

	val = ((rafm_info.thrd.sobel9_thr_max_green << 16) & 0x3FFF0000)
		| (rafm_info.thrd.sobel9_thr_min_green & 0x0FFF);
	ISP_REG_WR(ISP_RGB_AFM_SOBEL9_THR_G, val);

	val = ((rafm_info.thrd.sobel9_thr_max_blue << 16) & 0x3FFF0000)
		| (rafm_info.thrd.sobel9_thr_min_blue & 0x0FFF);
	ISP_REG_WR(ISP_RGB_AFM_SOBEL9_THR_B, val);

	val = ((rafm_info.thrd.spsmd_thr_max_red << 12) & 0x7FFFF000)
		| (rafm_info.thrd.spsmd_thr_min_red & 0x0FFF);
	ISP_REG_WR(ISP_RGB_AFM_SPSMD_THR_R, val);

	val = ((rafm_info.thrd.spsmd_thr_max_green << 12) & 0x7FFFF000)
		| (rafm_info.thrd.spsmd_thr_min_green & 0x0FFF);
	ISP_REG_WR(ISP_RGB_AFM_SPSMD_THR_G, val);

	val = ((rafm_info.thrd.spsmd_thr_max_blue << 12) & 0x7FFFF000)
		| (rafm_info.thrd.spsmd_thr_min_blue & 0x0FFF);
	ISP_REG_WR(ISP_RGB_AFM_SPSMD_THR_B, val);


	for (i = 0; i < max_num; i++) {
		val = ((rafm_info.coord[i].start_y << 16) & 0xFFFF0000)
				| (rafm_info.coord[i].start_x & 0xFFFF);
		ISP_REG_WR((addr + 8 * i), val);

		val = ((rafm_info.coord[i].end_y << 16) & 0xFFFF0000)
			| (rafm_info.coord[i].end_x & 0xFFFF);
		ISP_REG_WR((addr + 8 * i + 4), val);
	}

	return ret;
}

static int32_t isp_k_raw_afm_frame_range(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size  frame_size = {0, 0};

	ret = copy_from_user((void *)&frame_size,
		param->property_param, sizeof(frame_size));
	if (ret != 0) {
		pr_info("error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (frame_size.height & 0xFFFF)
		| ((frame_size.width & 0xFFFF) << 16);
	ISP_REG_WR(ISP_RGB_AFM_FRAME_RANGE, val);
	return ret;
}

int32_t isp_k_raw_afm_statistic_r6p9(char *afm_buf)
{
	int i = 0, j = 0;
	unsigned long addr_sobel5;
	unsigned long addr_sobel9;
	unsigned long addr_spsmd;
	unsigned long addr_high_bit;
	int max_item = ISP_AFM_WIN_NUM;
	unsigned int buf_sel = ISP_REG_RD(ISP_RGB_AFM_STATUS0);
	struct isp_raw_afm_statistic_r6p9 item;

	if (buf_sel & BIT_28) {
		addr_sobel5 = ISP_RGB_AFM_ROI_0_SOBEL5X5_R_1;
		addr_sobel9 = ISP_RGB_AFM_ROI_0_SOBEL9X9_R_1;
		addr_spsmd = ISP_RGB_AFM_ROI_0_SPSMD_R_1;
		addr_high_bit = ISP_RGB_AFM_HIGH_BIT_ROI_01_R_1;
		ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_11,
				0 << 11);
		pr_debug("afm statics buffer1\n");
	} else {
		addr_sobel5 = ISP_RGB_AFM_ROI_0_SOBEL5X5_R_0;
		addr_sobel9 = ISP_RGB_AFM_ROI_0_SOBEL9X9_R_0;
		addr_spsmd = ISP_RGB_AFM_ROI_0_SPSMD_R_0;
		addr_high_bit = ISP_RGB_AFM_HIGH_BIT_ROI_01_R_0;
		ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_11,
				1 << 11);
		pr_debug("afm statics buffer0\n");
	}

	memset(&item, 0, sizeof(struct isp_raw_afm_statistic_r6p9));
	for (i = 0; i < max_item; i++) {
		item.val[j] = ISP_REG_RD(addr_sobel5);
		item.val[j + 1] = ISP_REG_RD(addr_sobel5 + 4);
		item.val[j + 2] = ISP_REG_RD(addr_sobel5 + 8);

		item.val[max_item * 3 + j]
			= ISP_REG_RD(addr_sobel9);
		item.val[max_item * 3 + j + 1]
			= ISP_REG_RD(addr_sobel9 + 4);
		item.val[max_item * 3 + j + 2]
			= ISP_REG_RD(addr_sobel9 + 8);

		item.val[max_item * 3 * 2 + j]
			= ISP_REG_RD(addr_spsmd);
		item.val[max_item * 3 * 2 + j + 1]
			= ISP_REG_RD(addr_spsmd + 4);
		item.val[max_item * 3 * 2 + j + 2]
			= ISP_REG_RD(addr_spsmd + 8);

		j += 3;
		addr_sobel5 += 12;
		addr_sobel9 += 12;
		addr_spsmd += 12;
	}

	j = 0;
	addr_high_bit = ISP_RGB_AFM_HIGH_BIT_ROI_01_R_0;
	for (i = 0; i < 3; i++) {
		item.val[max_item * 3 * 3 + j]
			= ISP_REG_RD(addr_high_bit);
		item.val[max_item * 3 * 3 + j + 1]
			= ISP_REG_RD(addr_high_bit + 4);
		item.val[max_item * 3 * 3 + j + 2]
			= ISP_REG_RD(addr_high_bit + 8);
		item.val[max_item * 3 * 3 + j + 3]
			= ISP_REG_RD(addr_high_bit + 12);
		item.val[max_item * 3 * 3 + j + 4]
			= ISP_REG_RD(addr_high_bit + 16);
		j += 5;
		addr_high_bit += 20;
	}

	memcpy(afm_buf, (void *)&item, sizeof(item));

	return 0;
}

static int32_t isp_k_raw_afm_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass,
		param->property_param, sizeof(bypass));
	if (ret != 0) {
		pr_info("ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (bypass)
		ISP_REG_OWR(ISP_RGB_AFM_PARAM, BIT_0);
	else
		ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_0, 0);

	return ret;
}

static int32_t isp_k_raw_afm_mode(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t mode = 0;

	ret = copy_from_user((void *)&mode,
		param->property_param, sizeof(mode));
	if (ret != 0) {
		pr_info("ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (mode)
		ISP_REG_OWR(ISP_RGB_AFM_PARAM, BIT_1);
	else
		ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_1, 0);

	return ret;
}

static int32_t isp_k_raw_afm_skip_num(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t skip_num = 0;

	ret = copy_from_user((void *)&skip_num,
		param->property_param, sizeof(skip_num));
	if (ret != 0) {
		pr_info("error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, (BIT_2 | BIT_3 | BIT_4 | BIT_5),
		(skip_num << 2));

	return ret;
}

static int32_t isp_k_raw_afm_skip_num_clr(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t is_clear = 0;

	ret = copy_from_user((void *)&is_clear,
		param->property_param, sizeof(is_clear));
	if (ret != 0) {
		pr_info("error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (is_clear)
		ISP_REG_OWR(ISP_RGB_AFM_PARAM, BIT_6);
	else
		ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_6, 0);

	return ret;
}

static int32_t isp_k_raw_afm_spsmd_rtgbot_enable
					(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t is_enable = 0;

	ret = copy_from_user((void *)&is_enable,
		param->property_param, sizeof(is_enable));
	if (ret != 0) {
		pr_info("error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (is_enable)
		ISP_REG_OWR(ISP_RGB_AFM_PARAM, BIT_7);
	else
		ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_7, 0);

	return ret;
}

static int32_t isp_k_raw_afm_spsmd_diagonal_enable
					(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t is_enable = 0;

	ret = copy_from_user((void *)&is_enable,
		param->property_param, sizeof(is_enable));
	if (ret != 0) {
		pr_info("error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (is_enable)
		ISP_REG_OWR(ISP_RGB_AFM_PARAM, BIT_8);
	else
		ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_8, 0);

	return ret;
}

static int32_t isp_k_raw_afm_spsmd_square_en
						(struct isp_io_param *param)
{
	int32_t ret = 0;

	uint32_t val = 0;

	ret = copy_from_user((void *)&val,
		param->property_param, sizeof(val));
	if (ret != 0) {
		pr_info("error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}
	if (val)
		ISP_REG_OWR(ISP_RGB_AFM_PARAM, BIT_9);
	else
		ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_9, 0);

	return ret;
}

static int32_t isp_k_raw_afm_overflow_protect(struct isp_io_param *param)
{
	int32_t ret = 0;

	uint32_t val = 0;

	ret = copy_from_user((void *)&val,
		param->property_param, sizeof(val));
	if (ret != 0) {
		pr_info("error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (val)
		ISP_REG_OWR(ISP_RGB_AFM_PARAM, BIT_12);
	else
		ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_12, 0);

	return ret;
}

static int32_t isp_k_raw_afm_subfilter(struct isp_io_param *param)
{
	int32_t ret = 0;

	struct afm_subfilter subfilter;

	ret = copy_from_user((void *)&subfilter,
		param->property_param, sizeof(subfilter));
	if (ret != 0) {
		pr_info("error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (subfilter.average)
		ISP_REG_OWR(ISP_RGB_AFM_PARAM, BIT_13);
	else
		ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_13, 0);

	if (subfilter.median)
		ISP_REG_OWR(ISP_RGB_AFM_PARAM, BIT_14);
	else
		ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_14, 0);

	return ret;
}

static int32_t isp_k_raw_afm_spsmd_touch_mode
	(struct isp_io_param *param)
{
	int32_t ret = 0;

	uint32_t val = 0;

	ret = copy_from_user((void *)&val,
		param->property_param, sizeof(val));
	if (ret != 0) {
		pr_info("error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (val)
		ISP_REG_OWR(ISP_RGB_AFM_PARAM, BIT_15);
	else
		ISP_REG_MWR(ISP_RGB_AFM_PARAM, BIT_15, 0);

	return ret;
}

static int32_t isp_k_raw_afm_shift(struct isp_io_param *param)
{
	int32_t ret = 0;

	struct afm_shift shift;

	ret = copy_from_user((void *)&shift,
		param->property_param, sizeof(shift));
	if (ret != 0) {
		pr_info("error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_RGB_AFM_PARAM, (BIT_18 | BIT_17 | BIT_16),
		shift.shift_spsmd << 16);
	ISP_REG_MWR(ISP_RGB_AFM_PARAM, (BIT_21 | BIT_20 | BIT_19),
		shift.shift_sobel9 << 19);
	ISP_REG_MWR(ISP_RGB_AFM_PARAM, (BIT_27 | BIT_26 | BIT_25),
		shift.shift_sobel5 << 25);

	return ret;
}

static int32_t isp_k_raw_afm_threshold_rgb(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (1/*soc_is_scx9832a_v0() || soc_is_scx30g3_v0()*/) {
		uint32_t val = 0;
		struct afm_thrd_rgb thrd_rgb;

		ret = copy_from_user((void *)&thrd_rgb,
			param->property_param, sizeof(thrd_rgb));
		if (ret != 0) {
			pr_info("rror, ret = 0x%x\n", (uint32_t)ret);
			return -1;
		}

		val = ((thrd_rgb.sobel5_thr_max_red << 16) & 0x3FFF0000)
				| (thrd_rgb.sobel5_thr_min_red & 0x0FFF);
		ISP_REG_WR(ISP_RGB_AFM_SOBEL5_THR_R, val);

		val = ((thrd_rgb.sobel5_thr_max_green << 16) & 0x3FFF0000)
				| (thrd_rgb.sobel5_thr_min_green & 0x0FFF);
		ISP_REG_WR(ISP_RGB_AFM_SOBEL5_THR_G, val);

		val = ((thrd_rgb.sobel5_thr_max_blue << 16) & 0x3FFF0000)
				| (thrd_rgb.sobel5_thr_min_blue & 0x0FFF);
		ISP_REG_WR(ISP_RGB_AFM_SOBEL5_THR_B, val);

		val = ((thrd_rgb.sobel9_thr_max_red << 16) & 0x3FFF0000)
				| (thrd_rgb.sobel9_thr_min_red & 0x0FFF);
		ISP_REG_WR(ISP_RGB_AFM_SOBEL9_THR_R, val);

		val = ((thrd_rgb.sobel9_thr_max_green << 16) & 0x3FFF0000)
				| (thrd_rgb.sobel9_thr_min_green & 0x0FFF);
		ISP_REG_WR(ISP_RGB_AFM_SOBEL9_THR_G, val);

		val = ((thrd_rgb.sobel9_thr_max_blue << 16) & 0x3FFF0000)
				| (thrd_rgb.sobel9_thr_min_blue & 0x0FFF);
		ISP_REG_WR(ISP_RGB_AFM_SOBEL9_THR_B, val);

		val = ((thrd_rgb.spsmd_thr_max_red << 12) & 0x7FFFF000)
				| (thrd_rgb.spsmd_thr_min_red & 0x0FFF);
		ISP_REG_WR(ISP_RGB_AFM_SPSMD_THR_R, val);

		val = ((thrd_rgb.spsmd_thr_max_green << 12) & 0x7FFFF000)
				| (thrd_rgb.spsmd_thr_min_green & 0x0FFF);
		ISP_REG_WR(ISP_RGB_AFM_SPSMD_THR_G, val);

		val = ((thrd_rgb.spsmd_thr_max_blue << 12) & 0x7FFFF000)
				| (thrd_rgb.spsmd_thr_min_blue & 0x0FFF);
		ISP_REG_WR(ISP_RGB_AFM_SPSMD_THR_B, val);
	}

	return ret;
}

static int32_t isp_k_raw_afm_win(struct isp_io_param *param)
{
	int32_t ret = 0;
	int i = 0;
	uint32_t val = 0;
	int max_num = 0;
	unsigned long addr = 0;
	struct isp_coord coord[ISP_AFM_WIN_NUM];

	memset(coord, 0, sizeof(coord));
	ret = copy_from_user((void *)&coord,
		param->property_param, sizeof(coord));
	if (ret != 0) {
		pr_info("isp_k_raw_afm_win: error, ret = 0x%x\n",
						(uint32_t)ret);
		return -1;
	}

	max_num = ISP_AFM_WIN_NUM;
	addr = ISP_RGB_AFM_WIN_RANGE0S;

	for (i = 0; i < max_num; i++) {
		val = ((coord[i].start_y << 16) & 0xFFFF0000)
				| (coord[i].start_x & 0xFFFF);
		ISP_REG_WR((addr + 8 * i), val);

		val = ((coord[i].end_y << 16) & 0xFFFF0000)
			| (coord[i].end_x & 0xFFFF);
		ISP_REG_WR((addr + 8 * i + 4), val);
	}

	return ret;
}

static int32_t isp_k_raw_afm_win_num(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t num = 0;

	num = ISP_AFM_WIN_NUM;

	ret = copy_to_user(param->property_param, (void *)&num, sizeof(num));
	if (ret != 0) {
		ret = -1;
		pr_info("copy_to_user error, ret = 0x%x\n", (uint32_t)ret);
	}

	return ret;
}

int32_t isp_k_cfg_rgb_afm(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_afm: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_afm: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_RGB_AFM_BLOCK:
		ret = isp_k_raw_afm_block(param);
		break;
	case ISP_PRO_RGB_AFM_FRAME_SIZE:
		ret = isp_k_raw_afm_frame_range(param);
		break;
	case ISP_PRO_RGB_AFM_BYPASS:
		ret = isp_k_raw_afm_bypass(param);
		break;
	case ISP_PRO_RGB_AFM_MODE:
		ret = isp_k_raw_afm_mode(param);
		break;
	case ISP_PRO_RGB_AFM_SKIP_NUM:
		ret = isp_k_raw_afm_skip_num(param);
		break;
	case ISP_PRO_RGB_AFM_SKIP_NUM_CLR:
		ret = isp_k_raw_afm_skip_num_clr(param);
		break;
	case ISP_PRO_RGB_AFM_SPSMD_RTGBOT_ENABLE:
		ret = isp_k_raw_afm_spsmd_rtgbot_enable(param);
		break;
	case ISP_PRO_RGB_AFM_SPSMD_DIAGONAL_ENABLE:
		ret = isp_k_raw_afm_spsmd_diagonal_enable(param);
		break;
	case ISP_PRO_RGB_AFM_SPSMD_SQUARE_ENABLE:
		ret = isp_k_raw_afm_spsmd_square_en(param);
		break;
	case ISP_PRO_RGB_AFM_OVERFLOW_PROTECT:
		ret = isp_k_raw_afm_overflow_protect(param);
		break;
	case ISP_PRO_RGB_AFM_SUBFILTER:
		ret = isp_k_raw_afm_subfilter(param);
		break;
	case ISP_PRO_RGB_AFM_SPSMD_TOUCH_MODE:
		ret = isp_k_raw_afm_spsmd_touch_mode(param);
		break;
	case ISP_PRO_RGB_AFM_SHFIT:
		ret = isp_k_raw_afm_shift(param);
		break;
	case ISP_PRO_RGB_AFM_THRESHOLD_RGB:
		ret = isp_k_raw_afm_threshold_rgb(param);
		break;
	case ISP_PRO_RGB_AFM_WIN:
		ret = isp_k_raw_afm_win(param);
		break;
	case ISP_PRO_RGB_AFM_WIN_NUM:
		ret = isp_k_raw_afm_win_num(param);
		break;
	default:
		pr_info("isp_k_cfg_afm: fail cmd id:%d, not supported.\n",
			param->property);
		break;
	}

	return ret;
}
