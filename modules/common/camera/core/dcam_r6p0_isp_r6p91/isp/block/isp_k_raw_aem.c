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

#define ISP_RAWAEM_TIMEOUT msecs_to_jiffies(1000)

static int32_t isp_k_raw_aem_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_raw_aem_info aem_info;

	memset(&aem_info, 0x00, sizeof(aem_info));

	ret = copy_from_user((void *)&aem_info,
		param->property_param, sizeof(aem_info));
	if (ret != 0) {
		pr_info("copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_AEM_PARAM, BIT_0, aem_info.bypass);

	val = (aem_info.shift & 0x1F) << 18;
	if (val != 0)
		pr_info("isp_k_raw_aem_block: shift error, 0x%x\n", val);
	ISP_REG_MWR(ISP_AEM_BLK_SIZE, 0x7C0000, 0);

	ISP_REG_MWR(ISP_AEM_PARAM, BIT_1, aem_info.mode << 1);

	return ret;
}

static int32_t isp_k_raw_aem_bypass(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t bypass = 0;

	ret = copy_from_user((void *)&bypass,
		param->property_param, sizeof(bypass));
	if (ret != 0) {
		pr_info("copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_AEM_PARAM, BIT_0, bypass);

	return ret;
}

static int32_t isp_k_raw_aem_mode(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t mode = 0;

	ret = copy_from_user((void *)&mode,
		param->property_param, sizeof(mode));
	if (ret != 0) {
		pr_info("copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_AEM_PARAM, BIT_1, mode << 1);

	return ret;
}

static int32_t isp_k_raw_aem_skip_num(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t skip_num;

	ret = copy_from_user((void *)&skip_num,
		param->property_param, sizeof(skip_num));
	if (ret != 0) {
		pr_info("copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (skip_num & 0xF) << 4;
	ISP_REG_MWR(ISP_AEM_PARAM, 0xF0, val);

	return ret;
}

static int32_t isp_k_raw_aem_shift(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t shift = 0;
	uint32_t val = 0;

	ret = copy_from_user((void *)&shift,
		param->property_param, sizeof(shift));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = (shift & 0x1F) << 18;
	if (val != 0)
		pr_info("isp_k_raw_aem_shift: shift error, 0x%x\n", val);
	ISP_REG_MWR(ISP_AEM_BLK_SIZE, 0x7C0000, 0);

	return ret;
}

static int32_t isp_k_raw_aem_offset(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct img_offset offset;

	ret = copy_from_user((void *)&offset,
		param->property_param, sizeof(struct img_offset));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((offset.y & 0xFFFF) << 16)
		| (offset.x & 0xFFFF);
	ISP_REG_WR(ISP_AEM_OFFSET, val);

	return ret;
}

static int32_t isp_k_raw_aem_blk_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_img_size size;
	uint32_t val = 0;

	ret = copy_from_user((void *)&size,
		param->property_param, sizeof(struct isp_img_size));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0x1FF) << 9)
		| (size.width & 0x1FF);
	ISP_REG_MWR(ISP_AEM_BLK_SIZE, 0x3FFFF, val);

	return ret;
}

static int32_t isp_k_raw_aem_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_img_size size;
	uint32_t val = 0;

	ret = copy_from_user((void *)&size,
		param->property_param, sizeof(struct isp_img_size));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height & 0xFFFF) << 16)
		| (size.width & 0xFFFF);
	ISP_REG_WR(ISP_AEM_SLICE_SIZE, val);
	/*need to fixed*/
	ISP_REG_WR(ISP_AEM_SLICE_DDR_PARAM, 0x800);

	return ret;
}

int32_t isp_k_cfg_raw_aem(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_raw_aem: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_raw_aem: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_RAW_AEM_BLOCK:
		ret = isp_k_raw_aem_block(param);
		break;
	case ISP_PRO_RAW_AEM_BYPASS:
		ret = isp_k_raw_aem_bypass(param);
		break;
	case ISP_PRO_RAW_AEM_MODE:
		ret = isp_k_raw_aem_mode(param);
		break;
	case ISP_PRO_RAW_AEM_SKIP_NUM:
		ret = isp_k_raw_aem_skip_num(param);
		break;
	case ISP_PRO_RAW_AEM_SHIFT:
		ret = isp_k_raw_aem_shift(param);
		break;
	case ISP_PRO_RAW_AEM_OFFSET:
		ret = isp_k_raw_aem_offset(param);
		break;
	case ISP_PRO_RAW_AEM_BLK_SIZE:
		ret = isp_k_raw_aem_blk_size(param);
		break;
	case ISP_PRO_RAW_AEM_SLICE_SIZE:
		ret = isp_k_raw_aem_slice_size(param);
		break;
	default:
		pr_info("fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
