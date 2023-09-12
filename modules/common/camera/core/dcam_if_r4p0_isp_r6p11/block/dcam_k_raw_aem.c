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
#include <linux/delay.h>

#include "sprd_mm.h"
#include "dcam_block.h"
#include "dcam_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "DCAM_AEM: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

#define DCAM_LSC_TIME_OUT_MAX 1000

static int dcam_k_2d_check_condition(enum dcam_id idx)
{
	int ret = 0;
	unsigned int time_out_cnt = 0;
	unsigned int reg_value = 0;

	while ((0x00000000 ==
		(reg_value = DCAM_REG_RD(
		idx, DCAM0_LENS_LOAD_DONE) & BIT_0)) &&
		(time_out_cnt < DCAM_LSC_TIME_OUT_MAX)) {
		udelay(1);
		time_out_cnt++;
	}

	if (time_out_cnt >= DCAM_LSC_TIME_OUT_MAX) {
		DCAM_REG_MWR(idx, DCAM0_AEM_PARA, BIT_0, 1);
		ret = -EPERM;
		pr_err("fail to load lsc table time out.\n");
	} else if (reg_value == 0x00000001) {
		sprd_dcam_glb_reg_mwr(idx, DCAM0_AEM_SKIP_NUM_CLR,
			BIT_1, 0x00000002, DCAM_REG_MAX);
		pr_debug("lsc table load success.\n");
	}
	return ret;
}

static void dcam_aem_mode_setting(enum dcam_id idx, unsigned int mode)
{
	unsigned int mode_start = 1;

	sprd_dcam_glb_reg_mwr(idx, DCAM0_AEM_PARA,
		BIT_1,
		mode << 1,
		DCAM_REG_MAX);

	if (mode == 0) {  /* single frame mode */
		sprd_dcam_glb_reg_mwr(idx, DCAM0_LENS_LOAD_ENABLE,
			BIT_1,
			mode_start << 1,
			DCAM_REG_MAX);
	} else { /* multi frame mode*/
		sprd_dcam_glb_reg_mwr(idx, DCAM0_LENS_LOAD_ENABLE,
			BIT_2,
			mode_start << 2,
			DCAM_REG_MAX);
	}
}

int dcam_k_raw_aem_block(struct isp_io_param *param, enum dcam_id idx)
{
	int ret = 0;
	unsigned int  val = 0;
	struct sprd_aem_info aem_info;

	memset(&aem_info, 0x00, sizeof(aem_info));
	ret = copy_from_user((void *)&aem_info, param->property_param,
			     sizeof(struct sprd_aem_info));

	if (unlikely(ret != 0)) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EPERM;
	}

	val = (aem_info.red_thr.low << 16) |
		(aem_info.red_thr.high);
	sprd_dcam_glb_reg_mwr(idx, DCAM0_AEM_RED_THR,
			0x03FF03FF,
			val,
			DCAM_REG_MAX);

	val = (aem_info.blue_thr.low << 16) |
		(aem_info.blue_thr.high);
	sprd_dcam_glb_reg_mwr(idx, DCAM0_AEM_BLUE_THR,
			0x03FF03FF,
			val,
			DCAM_REG_MAX);

	val = (aem_info.green_thr.low << 16) |
		(aem_info.green_thr.high);
	sprd_dcam_glb_reg_mwr(idx, DCAM0_AEM_GREEN_THR,
			0x03FF03FF,
			val,
			DCAM_REG_MAX);

	DCAM_REG_MWR(idx, DCAM0_AEM_SKIP_NUM_CLR, BIT_0, 1);
	DCAM_REG_MWR(idx, DCAM0_AEM_CFG_READY, BIT_0, 1);

	dcam_k_2d_check_condition(idx);

	return ret;
}

int32_t dcam_k_raw_aem_offset(struct isp_io_param *param, enum dcam_id idx)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct sprd_aem_offset offset;

	ret = copy_from_user((void *)&offset,
		param->property_param, sizeof(struct sprd_aem_offset));
	if (unlikely(ret != 0)) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EINVAL;
	}

	val = (offset.y << 16) | (offset.x);
	sprd_dcam_glb_reg_mwr(idx, DCAM0_AEM_OFFSET,
		0x1FFF1FFF,
		val,
		DCAM_REG_MAX);
	DCAM_REG_MWR(idx, DCAM0_AEM_CFG_READY, BIT_0, 1);

	return ret;
}

int32_t dcam_k_raw_aem_shift(struct isp_io_param *param, enum dcam_id idx)
{
	int32_t ret = 0;
	struct sprd_aem_avgshf shift;

	ret = copy_from_user((void *)&shift,
			param->property_param, sizeof(struct sprd_aem_avgshf));
	if (unlikely(ret != 0)) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EINVAL;
	}

	sprd_dcam_glb_reg_mwr(idx, DCAM0_AEM_BLK_SIZE,
		0x003F0000,
		(shift.aem_m_avgshf << 20) |
		(shift.aem_l_avgshf << 18) |
		(shift.aem_h_avgshf << 16),
		DCAM_REG_MAX);
	DCAM_REG_MWR(idx, DCAM0_AEM_CFG_READY, BIT_0, 1);

	return ret;
}

int dcam_k_raw_aem_mode(struct isp_io_param *param, enum dcam_id idx)
{
	int ret = 0;
	unsigned int  mode = 0;

	ret = copy_from_user((void *)&mode,
			param->property_param, sizeof(mode));
	if (unlikely(ret != 0)) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EINVAL;
	}

	dcam_aem_mode_setting(idx, mode);
	DCAM_REG_MWR(idx, DCAM0_AEM_CFG_READY, BIT_0, 1);

	return ret;
}

int dcam_k_raw_aem_blk_size(struct isp_io_param *param, enum dcam_id idx)
{
	int ret = 0;
	struct sprd_aem_blk_size size;
	unsigned int val = 0;

	ret = copy_from_user((void *)&size,
		param->property_param, sizeof(struct sprd_aem_blk_size));
	if (unlikely(ret != 0)) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return ret;
	}

	val = (size.height << 8) | (size.width);
	sprd_dcam_glb_reg_mwr(idx, DCAM0_AEM_BLK_SIZE,
			0x0000FFFF,
			val,
			DCAM_REG_MAX);
	DCAM_REG_MWR(idx, DCAM0_AEM_CFG_READY, BIT_0, 1);

	return ret;
}

int dcam_k_raw_aem_skip_num(struct isp_io_param *param, enum dcam_id idx)
{
	int ret = 0;
	unsigned int  skip_num = 0;

	ret = copy_from_user((void *)&skip_num,
			param->property_param, sizeof(skip_num));
	if (unlikely(ret != 0)) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EINVAL;
	}

	sprd_dcam_glb_reg_mwr(idx, DCAM0_AEM_PARA,
		  BIT(4) | BIT(5) | BIT(6) | BIT(7),
	      skip_num << 4 &
		  (BIT(4) | BIT(5) | BIT(6) | BIT(7)),
	      DCAM_REG_MAX);

	DCAM_REG_MWR(idx, DCAM0_AEM_CFG_READY, BIT_0, 1);

	return ret;
}

int dcam_k_cfg_raw_aem(struct isp_io_param *param, enum dcam_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param.\n");
		return -EINVAL;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param.\n");
		return -EINVAL;
	}

	switch (param->property) {
	case DCAM_PRO_RAW_AEM_BLOCK:
		ret = dcam_k_raw_aem_block(param, idx);
		break;
	case DCAM_PRO_RAW_AEM_MODE:
		ret = dcam_k_raw_aem_mode(param, idx);
		break;
	case DCAM_PRO_RAW_AEM_OFFSET:
		ret = dcam_k_raw_aem_offset(param, idx);
		break;
	case DCAM_PRO_RAW_AEM_SHIFT:
		ret = dcam_k_raw_aem_shift(param, idx);
		break;
	case DCAM_PRO_RAW_AEM_BLK_SIZE:
		ret = dcam_k_raw_aem_blk_size(param, idx);
		break;
	case DCAM_PRO_RAW_AEM_SKIP_NUM:
		ret = dcam_k_raw_aem_skip_num(param, idx);
		break;

	default:
		pr_err("fail to supported cmr is = %d\n", param->property);
		break;
	}

	return ret;
}
