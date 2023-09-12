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

#include <linux/slab.h>
#include <linux/uaccess.h>

#include "sprd_mm.h"
#include "sprd_isp_hw.h"
#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_AEM: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

static int isp_k_raw_aem_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int  val = 0;
	struct isp_dev_raw_aem_info aem_info;

	memset(&aem_info, 0x00, sizeof(aem_info));
	ret = copy_from_user((void *)&aem_info, param->property_param,
			     sizeof(struct isp_dev_raw_aem_info));

	if (ret != 0) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_AEM_PARAM, BIT_0, aem_info.bypass);
	if (aem_info.bypass) {
		ISP_REG_MWR(idx, ISP_AEM_CFG_READY, BIT_0, 1);
		isp_dbg_s_ori_byp(SBLK_BYPASS, raw_aem, idx);
		return 0;
	}

	/*aem work at continue frame mode*/
	ISP_REG_MWR(idx, ISP_AEM_PARAM, BIT_1, (0x1 << 1));

	val = (aem_info.skip_num & 0xF) << 4;
	ISP_REG_MWR(idx, ISP_AEM_PARAM, 0xF0, val);

	val = ((aem_info.offset.y & 0xFFFF) << 16) |
		(aem_info.offset.x & 0xFFFF);
	ISP_REG_WR(idx, ISP_AEM_OFFSET, val);

	val = ((aem_info.blk_size.height & 0x1FF) << 9) |
		(aem_info.blk_size.width & 0x1FF);
	ISP_REG_MWR(idx, ISP_AEM_BLK_SIZE, 0x3FFFF, val);

	val = ((aem_info.slice_size.height & 0xFFFF) << 16) |
		(aem_info.slice_size.width & 0xFFFF);
	ISP_REG_WR(idx, ISP_AEM_SLICE_SIZE, val);

	ISP_REG_WR(idx, ISP_AEM_DDR_WR_NUM, aem_info.ddr_wr_num);/*0x2000/8*/

	ISP_REG_MWR(idx, ISP_AEM_SKIP_NUM_CLR, BIT_0, 1);

	ISP_REG_MWR(idx, ISP_AEM_CFG_READY, BIT_0, 1);

	return ret;
}

static int isp_k_raw_aem_bypass(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int  bypass = 0;

	ret = copy_from_user((void *)&bypass, param->property_param,
			     sizeof(bypass));
	if (ret != 0) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_AEM_PARAM, BIT_0, bypass);
	ISP_REG_WR(idx, ISP_AEM_DDR_WR_NUM, 0x400);/*0x2000/8*/
	ISP_REG_MWR(idx, ISP_AEM_SKIP_NUM_CLR, BIT_0, 1);

	ISP_REG_MWR(idx, ISP_AEM_CFG_READY, BIT_0, 1);

	if (bypass)
		isp_dbg_s_ori_byp(SBLK_BYPASS, raw_aem, idx);

	return ret;
}

static int32_t isp_k_raw_aem_slice_size(struct isp_io_param *param,
					enum isp_id idx)
{
	int32_t ret = 0;
	struct isp_img_size size;
	uint32_t val = 0;

	ret = copy_from_user((void *)&size, param->property_param,
				sizeof(struct isp_img_size));
	if (ret != 0) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -1;
	}

	val = ((size.height & 0xFFFF) << 16) | (size.width & 0xFFFF);
	ISP_REG_WR(idx, ISP_AEM_SLICE_SIZE, val);
	ISP_REG_MWR(idx, ISP_AEM_CFG_READY, BIT_0, 1);

	return ret;
}

int isp_k_cfg_raw_aem(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param ptr.\n");
		return -EINVAL;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param.\n");
		return -EINVAL;
	}

	switch (param->property) {
	case ISP_PRO_RAW_AEM_BLOCK:
		ret = isp_k_raw_aem_block(param, idx);
		break;
	case ISP_PRO_RAW_AEM_BYPASS:
		ret = isp_k_raw_aem_bypass(param, idx);
		break;
	case ISP_PRO_RAW_AEM_SLICE_SIZE:
		ret = isp_k_raw_aem_slice_size(param, idx);
		break;

	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
