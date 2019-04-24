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
#define pr_fmt(fmt) "LTM_MAP: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_ltm_map_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	uint32_t val = 0;
	uint32_t  base = 0;
	struct isp_dev_ltm_map_block_info ltm_map_info = {0};

	ret = copy_from_user((void *)&ltm_map_info,
		param->property_param, sizeof(ltm_map_info));
	if (ret != 0) {
		pr_err("fail to check param ret = 0x%x\n", (unsigned int)ret);
		return -1;
	}

	switch (param->sub_block) {
	case ISP_BLOCK_LTM_RGB_MAP:
		base = ISP_LTM_MAP_RGB_BASE;
		break;
	case ISP_BLOCK_LTM_YUV_MAP:
		base = ISP_LTM_MAP_YUV_BASE;
		break;
	default:
		pr_err("fail to check cmd %d.\n", param->sub_block);
		return -1;
	}

	ISP_REG_MWR(idx, base + ISP_LTM_MAP_PARAM0, BIT_0, ltm_map_info.bypass);
	if (ltm_map_info.bypass)
		return 0;
	val = ((ltm_map_info.burst8_en & 0x1) << 1) |
		((ltm_map_info.fetch_wait_en & 0x1) << 3) |
		((ltm_map_info.fetch_wait_line & 0x1) << 4);
	ISP_REG_MWR(idx, base + ISP_LTM_MAP_PARAM0, 0x1A, val);
	val = (ltm_map_info.tile_width & 0x3FF) |
		((ltm_map_info.tile_height & 0x3FF) << 12);
	ISP_REG_MWR(idx, base + ISP_LTM_MAP_PARAM1, 0x3FF3FF, val);
	val = ltm_map_info.tile_size_pro;
	ISP_REG_WR(idx, base + ISP_LTM_MAP_PARAM2, val);
	val = (ltm_map_info.hist_pitch & 0x7) << 24;
	ISP_REG_WR(idx, base + ISP_LTM_MAP_PARAM5, val);

	return ret;
}

static int isp_k_ltm_map_slice(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int base = 0;
	struct isp_dev_ltm_map_slice_info slice_info = {0};

	ret = copy_from_user((void *)&slice_info,
		param->property_param, sizeof(slice_info));
	if (ret != 0) {
		pr_err("fail to check param, ret = 0x%x\n", (unsigned int)ret);
		return -1;
	}

	switch (param->sub_block) {
	case ISP_BLOCK_LTM_RGB_MAP:
		base = ISP_LTM_MAP_RGB_BASE;
		break;
	case ISP_BLOCK_LTM_YUV_MAP:
		base = ISP_LTM_MAP_YUV_BASE;
		break;
	default:
		pr_err("fail to check cmd %d\n", param->sub_block);
		return -1;
	}

	val = ((slice_info.tile_num_x & 0x7) << 24)
		| ((slice_info.tile_num_y & 0x7) << 28);
	ISP_REG_MWR(idx, base + ISP_LTM_MAP_PARAM1, 0x77000000, val);
	val = (slice_info.tile_start_x & 0x7FF)
		| ((slice_info.tile_left_flag & 0x1) << 11)
		| ((slice_info.tile_start_y & 0x7FF) << 12)
		| ((slice_info.tile_right_flag & 0x1) << 23);
	ISP_REG_WR(idx, base + ISP_LTM_MAP_PARAM3, val);
	val = slice_info.mem_init_addr;
	ISP_REG_WR(idx, base + ISP_LTM_MAP_PARAM4, val);

	return ret;
}

int isp_k_cfg_ltm_map(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to check param\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_err("fail to check  param\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_LTM_MAP_BLOCK:
		ret = isp_k_ltm_map_block(param, idx);
		break;
	case ISP_PRO_LTM_MAP_SLICE:
		ret = isp_k_ltm_map_slice(param, idx);
		break;
	default:
		pr_err("fail to check cmd, cmd %d not supported\n",
			param->property);
		break;
	}

	return ret;
}
