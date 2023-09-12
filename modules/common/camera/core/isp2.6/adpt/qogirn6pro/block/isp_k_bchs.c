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
#define pr_fmt(fmt) "BCHS: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int isp_k_bchs_block(struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_bchs_info *bchs_info = NULL;

	if (isp_k_param->bchs_info.isupdate == 0)
		return ret;

	bchs_info = &isp_k_param->bchs_info;
	isp_k_param->bchs_info.isupdate = 0;

	if (g_isp_bypass[idx] & (1 << _EISP_BCHS))
		bchs_info->bchs_bypass = 1;
	if (bchs_info->bchs_bypass)
		return 0;

	ISP_REG_MWR(idx, ISP_BCHS_PARAM, 0x1e,
		(bchs_info->cnta_en << 4) |
		(bchs_info->brta_en << 3) |
		(bchs_info->hua_en << 2) |
		(bchs_info->csa_en << 1));

	ISP_REG_MWR(idx, ISP_CSA_FACTOR, 0xffffffff,
		(bchs_info->csa_factor_u << 8) |
		(bchs_info->csa_factor_v << 0));

	ISP_REG_MWR(idx, ISP_HUA_FACTOR, 0xffffffff,
		((bchs_info->hua_cos_value & 0x1ff) << 16) |
		((bchs_info->hua_sina_value & 0x1ff) << 0));

	ISP_REG_MWR(idx, ISP_BRTA_FACTOR, 0xffffffff,
		(bchs_info->brta_factor << 0));

	ISP_REG_MWR(idx, ISP_CNTA_FACTOR, 0xffffffff,
		(bchs_info->cnta_factor << 0));

	return ret;
}

static int isp_k_brightness_block(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	uint32_t bchs_bypass = 0;
	uint32_t bright_en;
	struct isp_dev_brightness_info brightness_info;

	ret = copy_from_user((void *)&brightness_info,
			param->property_param,
			sizeof(brightness_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	bright_en = !brightness_info.bypass;
	isp_k_param->bchs_info.brta_en = bright_en;
	ISP_REG_MWR(idx, ISP_BCHS_PARAM, BIT_3, bright_en << 3);
	if (bright_en == 0) {
		pr_debug("brightness is bypass.");
		return 0;
	}
	ISP_REG_MWR(idx, ISP_BCHS_PARAM, BIT_0, bchs_bypass);
	ISP_REG_WR(idx, ISP_BRTA_FACTOR, brightness_info.factor);

	isp_k_param->bchs_info.bchs_bypass = 0;
	isp_k_param->bchs_info.brta_factor = brightness_info.factor;

	return ret;
}

static int isp_k_contrast_block(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	uint32_t bchs_bypass = 0;
	uint32_t ctra_en;
	struct isp_dev_contrast_info contrast_info;

	ret = copy_from_user((void *)&contrast_info,
			param->property_param,
			sizeof(contrast_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ctra_en = !contrast_info.bypass;
	isp_k_param->bchs_info.cnta_en = ctra_en;
	ISP_REG_MWR(idx, ISP_BCHS_PARAM, BIT_4, ctra_en << 4);
	if (ctra_en == 0) {
		pr_debug("contrast is bypass.");
		return 0;
	}
	ISP_REG_MWR(idx, ISP_BCHS_PARAM, BIT_0, bchs_bypass);
	ISP_REG_WR(idx, ISP_CNTA_FACTOR, contrast_info.factor);

	isp_k_param->bchs_info.bchs_bypass = 0;
	isp_k_param->bchs_info.cnta_factor = contrast_info.factor;

	return ret;
}

static int isp_k_satuation_block(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	uint32_t bchs_bypass = 0;
	uint32_t csa_en;
	struct isp_dev_csa_info csa_info;

	ret = copy_from_user((void *)&csa_info,
			param->property_param,
			sizeof(csa_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	csa_en = !csa_info.bypass;
	isp_k_param->bchs_info.csa_en = csa_en;
	ISP_REG_MWR(idx, ISP_BCHS_PARAM, BIT_1, csa_en << 1);
	if (csa_en == 0) {
		pr_debug("satucation is bypass.");
		return 0;
	}
	ISP_REG_MWR(idx, ISP_BCHS_PARAM, BIT_0, bchs_bypass);
	ISP_REG_MWR(idx, ISP_CSA_FACTOR, 0xffff,
		(csa_info.csa_factor_u << 8) | csa_info.csa_factor_v);

	isp_k_param->bchs_info.bchs_bypass = 0;
	isp_k_param->bchs_info.csa_factor_u = csa_info.csa_factor_u;
	isp_k_param->bchs_info.csa_factor_v = csa_info.csa_factor_v;

	return ret;
}

static int isp_k_hue_block(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	uint32_t bchs_bypass = 0;
	uint32_t hue_en;
	struct isp_dev_hue_info hue_info;

	ret = copy_from_user((void *)&hue_info,
			param->property_param,
			sizeof(hue_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	hue_en = !hue_info.bypass;
	isp_k_param->bchs_info.hua_en = hue_en;
	ISP_REG_MWR(idx, ISP_BCHS_PARAM, BIT_2, hue_en << 2);
	if (hue_en == 0) {
		pr_debug("satucation is bypass.");
		return 0;
	}
	ISP_REG_MWR(idx, ISP_BCHS_PARAM, BIT_0, bchs_bypass);
	ISP_REG_MWR(idx, ISP_HUA_FACTOR, 0x1ff01ff,
		((hue_info.hua_cos_value & 0x1ff) << 16) |
		((hue_info.hua_sin_value & 0x1ff) << 0));

	isp_k_param->bchs_info.bchs_bypass = 0;
	isp_k_param->bchs_info.hua_cos_value = hue_info.hua_cos_value;
	isp_k_param->bchs_info.hua_sina_value = hue_info.hua_sin_value;

	return ret;
}

int isp_k_cfg_bchs(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_bchs_info *bchs_info = NULL;

	bchs_info = &isp_k_param->bchs_info;

	switch (param->property) {
	case ISP_PRO_BCHS_BLOCK:
		ret = copy_from_user((void *)bchs_info, param->property_param, sizeof(struct isp_dev_bchs_info));
		if (ret != 0) {
			pr_err("fail to copy from user, ret = %d\n", ret);
			return -1;
		}
		isp_k_param->bchs_info.isupdate = 1;
		break;
	case ISP_PRO_BCHS_BRIGHT:
		ret = isp_k_brightness_block(param, isp_k_param, idx);
		break;
	case ISP_PRO_BCHS_CONTRAST:
		ret = isp_k_contrast_block(param, isp_k_param, idx);
		break;
	case ISP_PRO_BCHS_SATUATION:
		ret = isp_k_satuation_block(param, isp_k_param, idx);
		break;
	case ISP_PRO_BCHS_HUE:
		ret = isp_k_hue_block(param, isp_k_param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
				param->property);
		break;
	}

	return ret;
}

int isp_k_cpy_bchs(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if (isp_k_param->bchs_info.isupdate == 1) {
		memcpy(&param_block->bchs_info, &isp_k_param->bchs_info, sizeof(struct isp_dev_bchs_info));
		isp_k_param->bchs_info.isupdate = 0;
		param_block->bchs_info.isupdate = 1;
	}

	return ret;
}
