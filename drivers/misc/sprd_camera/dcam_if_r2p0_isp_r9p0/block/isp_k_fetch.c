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

#include "isp_buf.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "FETCH: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_fetch_block(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_fetch_info fetch_info;

	memset(&fetch_info, 0x00, sizeof(fetch_info));
	ret = copy_from_user((void *)&fetch_info,
		param->property_param, sizeof(fetch_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	ISP_REG_MWR(idx, ISP_FETCH_PARAM0, BIT_0, fetch_info.bypass);
	if (fetch_info.bypass)
		return 0;

	ISP_REG_MWR(idx, ISP_FETCH_PARAM0, 0xF << 4,
		fetch_info.color_format << 4);

	ISP_REG_WR(idx, ISP_FETCH_Y_PITCH,
			(fetch_info.pitch[0] & 0xFFFF));

	ISP_REG_WR(idx, ISP_FETCH_U_PITCH,
			(fetch_info.pitch[1] & 0xFFFF));

	ISP_REG_WR(idx, ISP_FETCH_V_PITCH,
			(fetch_info.pitch[2] & 0xFFFF));

	return ret;
}

static int isp_k_fetch_start(struct isp_io_param *param, uint32_t idx)
{
	int ret = 0;
	unsigned int start = 0;
	unsigned int addr = 0;

	switch (param->sub_block) {
	case ISP_BLOCK_FETCH:
		addr = ISP_FETCH_START;
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->sub_block);
		return -1;
	}

	ret = copy_from_user((void *)&start,
		param->property_param, sizeof(unsigned int));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	pr_info("fetch raw addr %x start %d\n", addr, start);
	ISP_REG_WR(idx, addr, start);

	return ret;
}

static int isp_k_fetch_slice_size(struct isp_io_param *param,
					uint32_t idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct isp_dev_fetch_slice_info slice_info = {0};

	ret = copy_from_user((void *)&slice_info,
		param->property_param, sizeof(slice_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	val = ((slice_info.height & 0xFFFF) << 16)
		| (slice_info.width & 0xFFFF);
	ISP_REG_WR(idx, ISP_FETCH_MEM_SLICE_SIZE, val);

	ISP_REG_WR(idx, ISP_FETCH_SLICE_Y_ADDR, slice_info.addr[0]);

	ISP_REG_WR(idx, ISP_FETCH_SLICE_U_ADDR, slice_info.addr[1]);

	ISP_REG_WR(idx, ISP_FETCH_SLICE_V_ADDR, slice_info.addr[2]);

	val = (slice_info.mipi_word & 0xFFFF)
		| ((slice_info.mipi_byte & 0xF) << 16);
	ISP_REG_MWR(idx, ISP_FETCH_MIPI_PARAM, 0xFFFFF, val);

	return ret;
}

static int isp_k_fetch_transaddr(struct isp_io_param *param,
			struct isp_k_block *isp_k_param)
{
	int ret = 0;

	return ret;
}

int isp_k_cfg_fetch(struct isp_io_param *param,
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
	case ISP_PRO_FETCH_BLOCK:
		ret = isp_k_fetch_block(param, isp_k_param, com_idx);
		break;
	case ISP_PRO_FETCH_START:
		ret = isp_k_fetch_start(param, com_idx);
		break;
	case ISP_PRO_FETCH_SLICE_SIZE:
		ret = isp_k_fetch_slice_size(param, com_idx);
		break;
	case ISP_PRO_FETCH_TRANSADDR:
		ret = isp_k_fetch_transaddr(param, isp_k_param);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
