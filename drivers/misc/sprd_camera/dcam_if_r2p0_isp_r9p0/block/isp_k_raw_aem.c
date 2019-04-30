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
#define pr_fmt(fmt) "AEM: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_raw_aem_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	struct isp_dev_raw_aem_info aem_info;

	memset(&aem_info, 0x00, sizeof(aem_info));
	ret = copy_from_user((void *)&aem_info, param->property_param,
				sizeof(struct isp_dev_raw_aem_info));

	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	return ret;
}

static int isp_k_raw_aem_bypass(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param,
			sizeof(bypass));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	DCAM_REG_MWR(idx, DCAM_AEM_FRM_CTRL0, BIT_0, bypass);

	return ret;
}

static int32_t isp_k_raw_aem_offset(struct isp_io_param *param,
					enum isp_id idx)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct img_offset offset;

	ret = copy_from_user((void *)&offset,
		param->property_param, sizeof(struct img_offset));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	val = ((offset.y & 0x1FFF) << 16) | (offset.x & 0x1FFF);
	DCAM_REG_WR(idx, DCAM_AEM_OFFSET, val);

	return ret;
}

static int isp_k_raw_aem_mode(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int mode = 0;

	ret = copy_from_user((void *)&mode,
			param->property_param, sizeof(mode));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	DCAM_REG_MWR(idx, DCAM_AEM_FRM_CTRL0, BIT_2, mode << 2);

	if (mode)
		DCAM_REG_MWR(idx, DCAM_AEM_FRM_CTRL0, BIT_3, (0x1 << 3));
	else
		DCAM_REG_MWR(idx, DCAM_AEM_FRM_CTRL1, BIT_0, 0x1);

	return ret;
}

static int isp_k_raw_aem_blk_size(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	struct isp_img_size size;
	unsigned int val = 0;

	ret = copy_from_user((void *)&size,
		param->property_param, sizeof(struct isp_img_size));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return ret;
	}

	val = ((size.height & 0xFF) << 8) | (size.width & 0xFF);
	DCAM_REG_MWR(idx, DCAM_AEM_BLK_SIZE, 0xFFFF, val);

	return ret;
}

static int isp_k_raw_aem_skip_num(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int skip_num = 0;

	ret = copy_from_user((void *)&skip_num,
			param->property_param, sizeof(skip_num));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	val = (skip_num & 0xF) << 4;
	DCAM_REG_MWR(idx, DCAM_AEM_FRM_CTRL0, 0xF0, val);

	return ret;
}

static int isp_k_raw_aem_blk_num(struct isp_io_param *param,
	enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct img_offset blk_num;

	ret = copy_from_user((void *)&blk_num,
		param->property_param, sizeof(struct img_offset));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	val = ((blk_num.y & 0xFF) << 8) | (blk_num.x & 0xFF);
	DCAM_REG_WR(idx, DCAM_AEM_BLK_NUM, val);

	return ret;
}

static int isp_k_raw_aem_rgb_thr(struct isp_io_param *param,
	enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	struct awbc_rgb rgb_thr[2];

	ret = copy_from_user((void *)&rgb_thr,
		param->property_param, sizeof(rgb_thr));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	val = (((rgb_thr[1].r & 0x3FF) << 16) |
		(rgb_thr[0].r & 0x3FF));/*0: h_thr 1: l_thr*/
	DCAM_REG_WR(idx, DCAM_AEM_RED_THR, val);

	val = (((rgb_thr[1].b & 0x3FF) << 16) |
		(rgb_thr[0].b & 0x3FF));/*0: h_thr 1: l_thr*/
	DCAM_REG_WR(idx, DCAM_AEM_BLUE_THR, val);

	val = (((rgb_thr[1].g & 0x3FF) << 16) |
		(rgb_thr[0].g & 0x3FF));/*0: h_thr 1: l_thr*/
	DCAM_REG_WR(idx, DCAM_AEM_GREEN_THR, val);

	return ret;
}

static int isp_k_raw_aem_skip_num_clr(struct isp_io_param *param,
	enum isp_id idx)
{
	int ret = 0;
	unsigned int is_clear = 0;

	ret = copy_from_user((void *)&is_clear,
		param->property_param, sizeof(is_clear));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	DCAM_REG_MWR(idx, DCAM_AEM_FRM_CTRL1, BIT_1,
		is_clear << 1);

	return ret;
}

int isp_k_cfg_raw_aem(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, uint32_t com_idx)
{
	int ret = 0;
	enum isp_id idx = ISP_ID_0;

	if (!param) {
		pr_err("fail to get param\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -1;
	}

	idx = ISP_GET_ISP_ID(com_idx);
	switch (param->property) {
	case ISP_PRO_RAW_AEM_BLOCK:
		ret = isp_k_raw_aem_block(param, idx);
		break;
	case ISP_PRO_RAW_AEM_BYPASS:
		ret = isp_k_raw_aem_bypass(param, idx);
		break;
	case ISP_PRO_RAW_AEM_MODE:
		ret = isp_k_raw_aem_mode(param, idx);
		break;
	case ISP_PRO_RAW_AEM_OFFSET:
		ret = isp_k_raw_aem_offset(param, idx);
		break;
	case ISP_PRO_RAW_AEM_BLK_SIZE:
		ret = isp_k_raw_aem_blk_size(param, idx);
		break;
	case ISP_PRO_RAW_AEM_SKIP_NUM:
		ret = isp_k_raw_aem_skip_num(param, idx);
		break;
	case ISP_PRO_RAW_AEM_BLK_NUM:
		ret = isp_k_raw_aem_blk_num(param, idx);
		break;
	case ISP_PRO_RAW_AEM_RGB_THR:
		ret = isp_k_raw_aem_rgb_thr(param, idx);
		break;
	case ISP_PRO_RAW_AEM_SKIP_NUM_CLR:
		ret = isp_k_raw_aem_skip_num_clr(param, idx);
		break;

	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
