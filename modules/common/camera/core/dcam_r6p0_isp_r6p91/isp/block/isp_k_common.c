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
#include "isp_block.h"

/*support select afl_old and afl_new*/
static int isp_k_common_afl_set(struct isp_io_param *param)
{
	int ret = 0;
	enum isp_dev_afl_sel afl_sel = 0;

	ret = copy_from_user((void *)&afl_sel,
		param->property_param, sizeof(afl_sel));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}
	if (afl_sel == ISP_DEV_ANTI_FLICKER)
		ISP_REG_MWR(ISP_COMMON_3A_CTRL0, BIT_1, 0<<1);
	else
		ISP_REG_MWR(ISP_COMMON_3A_CTRL0, BIT_1, 1<<1);

	return 0;
}

enum isp_dev_afl_sel isp_k_common_afl_get(void)
{
	uint32_t val;

	val = ISP_REG_RD(ISP_COMMON_3A_CTRL0);
	if (val & BIT_1)
		return ISP_DEV_ANTI_FLICKER_NEW;
	else
		return ISP_DEV_ANTI_FLICKER;
}

static int32_t isp_k_common_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_dev_common_info_v1 common_info;

	memset(&common_info, 0x00, sizeof(common_info));

	ret = copy_from_user((void *)&common_info,
		param->property_param, sizeof(common_info));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	/*fetch and store source*/
	ISP_REG_MWR(ISP_COMMON_CTRL_CH0, 3,
		common_info.fetch_sel_0);
	ISP_REG_MWR(ISP_COMMON_CTRL_CH0, 3 << 16,
		common_info.store_sel_0 << 16);

	ISP_REG_MWR(ISP_COMMON_SPACE_SEL, 3,
		common_info.fetch_color_format);
	ISP_REG_MWR(ISP_COMMON_SPACE_SEL, 3 << 2,
		common_info.store_color_format << 2);

	ISP_REG_MWR(ISP_COMMON_3A_CTRL0, 1,
		common_info.y_afm_pos_0);
	ISP_REG_MWR(ISP_COMMON_3A_CTRL0, 3 << 4,
		common_info.y_aem_pos_0 << 4);

	ISP_REG_WR(ISP_COMMON_LBUF_OFFSET0,
		((common_info.lbuf_off.cfae_lbuf_offset & 0xFFFF) << 16)
		| (common_info.lbuf_off.comm_lbuf_offset & 0xFFFF));
	ISP_REG_WR(ISP_COMMON_LBUF_OFFSET1,
		((common_info.lbuf_off.ydly_lbuf_offset & 0xFFFF) << 16)
		| (common_info.lbuf_off.awbm_lbuf_offset & 0xFFFF));

	return ret;
}

static int32_t isp_k_common_version(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t version = 0;

	version = ISP_REG_RD(ISP_COMMON_VERSION);

	ret = copy_to_user(param->property_param,
		(void *)&version, sizeof(version));
	if (ret != 0) {
		ret = -1;
		pr_info("copy_to_user error, ret = 0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_common_status0(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t status = 0;

	status = ISP_REG_RD(ISP_COMMON_STATUS0);

	ret = copy_to_user(param->property_param,
		(void *)&status, sizeof(status));
	if (ret != 0) {
		ret = -1;
		pr_info("copy_to_user error, ret = 0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_common_status1(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t status = 0;

	status = ISP_REG_RD(ISP_COMMON_STATUS1);

	ret = copy_to_user(param->property_param,
		(void *)&status, sizeof(status));
	if (ret != 0) {
		ret = -1;
		pr_info("copy_to_user error, ret = 0x%x\n", (uint32_t)ret);
	}

	return ret;
}

static int32_t isp_k_common_channel0_fetch_sel
						(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t fetch_sel = 0;

	ret = copy_from_user((void *)&fetch_sel,
		param->property_param, sizeof(fetch_sel));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_COMMON_CTRL_CH0, 3, fetch_sel);

	return ret;
}

static int32_t isp_k_common_channel0_store_sel
						(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t store_sel = 0;

	ret = copy_from_user((void *)&store_sel,
		param->property_param, sizeof(store_sel));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_COMMON_CTRL_CH0, 3 << 16, store_sel  << 16);

	return ret;
}

static int32_t isp_k_common_fetch_color_space_sel
						(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t color_space = 0;

	ret = copy_from_user((void *)&color_space,
		param->property_param, sizeof(color_space));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_COMMON_SPACE_SEL, 3, color_space);

	return ret;
}

static int32_t isp_k_common_store_color_space_sel
					(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t color_space = 0;

	ret = copy_from_user((void *)&color_space,
		param->property_param, sizeof(color_space));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_COMMON_SPACE_SEL, 3  << 2, color_space << 2);

	return ret;
}

static int32_t isp_k_common_channel0_y_afm_pos
						(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t position = 0;

	ret = copy_from_user((void *)&position,
		param->property_param, sizeof(position));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_COMMON_3A_CTRL0, 3, position);

	return ret;
}

static int32_t isp_k_common_channel0_y_aem_pos
						(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t position = 0;

	ret = copy_from_user((void *)&position,
		param->property_param, sizeof(position));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_COMMON_3A_CTRL0, 3 << 4, position  << 4);

	return ret;
}

static int32_t isp_k_common_lbuf_offset(struct isp_io_param *param)
{
	int32_t ret = 0;
	struct isp_common_lbuf_param_v1 lbuf_off;

	memset(&lbuf_off, 0x00, sizeof(lbuf_off));
	ret = copy_from_user((void *)&lbuf_off,
		param->property_param, sizeof(lbuf_off));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_WR(ISP_COMMON_LBUF_OFFSET0,
		((lbuf_off.cfae_lbuf_offset & 0xFFFF) << 16)
		| (lbuf_off.comm_lbuf_offset & 0xFFFF));
	ISP_REG_WR(ISP_COMMON_LBUF_OFFSET1,
		((lbuf_off.ydly_lbuf_offset & 0xFFFF) << 16)
		| (lbuf_off.awbm_lbuf_offset & 0xFFFF));

	return ret;
}

static int32_t isp_k_all_shadow_ctrl(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t auto_shadow = 0;

	ret = copy_from_user((void *)&auto_shadow,
		param->property_param, sizeof(auto_shadow));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_SHADOW_CTRL_CH0, 1 << 16, auto_shadow  << 16);

	return ret;
}

static int32_t isp_k_awbm_shadow_ctrl(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t shadow_done = 0;

	ret = copy_from_user((void *)&shadow_done,
		param->property_param, sizeof(shadow_done));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_SHADOW_CTRL_CH0, 1 << 17, shadow_done  << 17);

	return ret;
}

void isp_k_ae_shadow_ctrl(void)
{
	ISP_REG_MWR(ISP_SHADOW_CTRL_CH0, 1 << 18, 1 << 18);
}

void isp_k_af_shadow_ctrl(void)
{
	ISP_REG_MWR(ISP_SHADOW_CTRL_CH0, 1 << 19, 1 << 19);
}

void isp_k_afl_shadow_ctrl(void)
{
	ISP_REG_MWR(ISP_SHADOW_CTRL_CH0, 1 << 20, 1 << 20);
}

static int32_t isp_k_comm_shadow_ctrl(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t shadow_done = 0;

	ret = copy_from_user((void *)&shadow_done,
		param->property_param, sizeof(shadow_done));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_SHADOW_CTRL_CH0, 1 << 21, shadow_done  << 21);

	return ret;
}

static int32_t isp_k_3a_shadow_ctrl(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t enable = 0;

	ret = copy_from_user((void *)&enable,
		param->property_param, sizeof(enable));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_COMMON_RESERVED0, 1 << 2, enable  << 2);

	return ret;
}

int32_t isp_k_cfg_common(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_common: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_common: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_COMMON_BLOCK:
		ret = isp_k_common_block(param);
		break;
	case ISP_PRO_COMMON_VERSION:
		ret = isp_k_common_version(param);
		break;
	case ISP_PRO_COMMON_STATUS0:
		ret = isp_k_common_status0(param);
		break;
	case ISP_PRO_COMMON_STATUS1:
		ret = isp_k_common_status1(param);
		break;
	case ISP_PRO_COMMON_CH0_FETCH_SEL:
		ret = isp_k_common_channel0_fetch_sel(param);
		break;
	case ISP_PRO_COMMON_CH0_STORE_SEL:
		ret = isp_k_common_channel0_store_sel(param);
		break;
	case ISP_PRO_COMMON_FETCH_COLOR_SPACE_SEL:
		ret = isp_k_common_fetch_color_space_sel(param);
		break;
	case ISP_PRO_COMMON_STORE_COLOR_SPACE_SEL:
		ret = isp_k_common_store_color_space_sel(param);
		break;
	case ISP_PRO_COMMON_CH0_AEM2_POS:
		ret = isp_k_common_channel0_y_aem_pos(param);
		break;
	case ISP_PRO_COMMON_CH0_Y_AFM_POS:
		ret = isp_k_common_channel0_y_afm_pos(param);
		break;
	case ISP_PRO_COMMON_LBUF_OFFSET:
		ret = isp_k_common_lbuf_offset(param);
		break;
	case ISP_PRO_COMMON_SHADOW_ALL_CTRL:
		ret = isp_k_all_shadow_ctrl(param);
		break;
	case ISP_PRO_COMMON_AWBM_SHADOW:
		ret = isp_k_awbm_shadow_ctrl(param);
		break;
	case ISP_PRO_COMMON_AFL_SEL:
		ret = isp_k_common_afl_set(param);
		break;
	case ISP_PRO_COMMON_COMM_SHADOW:
		ret = isp_k_comm_shadow_ctrl(param);
		break;
	case ISP_PRO_COMMON_3A_SINGLE_FRAME_CTRL:
		ret = isp_k_3a_shadow_ctrl(param);
		break;
	default:
		pr_info("not supported.\n");
		break;
	}

	return ret;
}
