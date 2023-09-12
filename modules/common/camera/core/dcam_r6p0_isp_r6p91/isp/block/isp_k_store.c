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

static int32_t isp_k_store_block(struct isp_io_param *param,
		struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_dev_store_info_v1 store_info;
	struct isp_addr block_addr = {0};
	struct pfiommu_info pfinfo;

	memset(&store_info, 0x00, sizeof(store_info));

	ret = copy_from_user((void *)&store_info,
		param->property_param, sizeof(store_info));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	memset(&pfinfo, 0, sizeof(struct pfiommu_info));
	block_addr = store_info.addr;
	pfinfo.dev = &isp_k_param->isp_pdev->dev;
	pfinfo.mfd[0] = block_addr.img_fd;
	pfinfo.mfd[1] = 0;
	pfinfo.mfd[2] = 0;
	if (pfinfo.mfd[0] != 0) {
		/*mapping iommu buffer*/
		ret = pfiommu_get_sg_table(&pfinfo);
		if (ret) {
			pr_err("map iommu store buffer failed.\n");
			return -EFAULT;
		}

		ret = pfiommu_get_addr(&pfinfo);
		if (ret) {
			pr_err("map iommu store get addr failed.\n");
			return -EFAULT;
		}

		store_info.addr.chn0 = pfinfo.iova[0] + block_addr.chn0;
		store_info.addr.chn1 = pfinfo.iova[0] + block_addr.chn1;
		store_info.addr.chn2 = pfinfo.iova[0] + block_addr.chn2;
	}

	val = ((store_info.border.right_border & 0xFF) << 24)
		| ((store_info.border.left_border & 0xFF) << 16)
		| ((store_info.border.down_border & 0xFF) << 8)
		| ((store_info.border.up_border & 0xFF));
	ISP_REG_WR(ISP_STORE_BORDER, val);

	if (store_info.subtract)
		ISP_REG_OWR(ISP_STORE_PARAM, BIT_1);
	else
		ISP_REG_MWR(ISP_STORE_PARAM, BIT_1, 0);

	ISP_REG_MWR(ISP_STORE_PARAM, 0xF0, (store_info.color_format << 4));

	ISP_REG_WR(ISP_STORE_Y_ADDR, store_info.addr.chn0);
	ISP_REG_WR(ISP_STORE_Y_PITCH, store_info.pitch.chn0);
	ISP_REG_WR(ISP_STORE_U_ADDR, store_info.addr.chn1);
	ISP_REG_WR(ISP_STORE_U_PITCH, store_info.pitch.chn1);
	ISP_REG_WR(ISP_STORE_V_ADDR, store_info.addr.chn2);
	ISP_REG_WR(ISP_STORE_V_PITCH, store_info.pitch.chn2);

	pr_debug("store: mfd: %d\n", pfinfo.mfd[0]);

	isp_k_param->store_pfinfo.iova[0] =
		pfinfo.iova[0];
	isp_k_param->store_pfinfo.size[0] = pfinfo.size[0];
	isp_k_param->store_pfinfo.dev = &isp_k_param->isp_pdev->dev;

	if (store_info.bypass)
		ISP_REG_OWR(ISP_STORE_PARAM, BIT_0);
	else
		ISP_REG_MWR(ISP_STORE_PARAM, BIT_0, 0);

	return ret;
}

static int32_t isp_k_store_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	struct isp_img_size size = {0, 0};

	ret = copy_from_user((void *)&size,
		param->property_param, sizeof(size));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	val = ((size.height&0xFFFF) << 16)
		| (size.width&0xFFFF);
	ISP_REG_WR(ISP_STORE_SLICE_SIZE, val);

	return ret;
}

int32_t isp_k_cfg_store(struct isp_io_param *param,
		struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_store: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_store: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_STORE_BLOCK:
		ret = isp_k_store_block(param, isp_k_param);
		break;
	case ISP_PRO_STORE_SLICE_SIZE:
		ret = isp_k_store_slice_size(param);
		break;
	default:
		pr_info("fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
