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

static int32_t isp_k_fetch_block(struct isp_io_param *param,
		struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;
	struct isp_dev_fetch_info_v1 fetch_info;
	struct isp_addr block_addr = {0};
	struct pfiommu_info pfinfo;

	memset(&fetch_info, 0x00, sizeof(fetch_info));

	ret = copy_from_user((void *)&fetch_info,
		param->property_param, sizeof(fetch_info));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (fetch_info.subtract)
		ISP_REG_OWR(ISP_FETCH_PARAM, BIT_1);
	else
		ISP_REG_MWR(ISP_FETCH_PARAM, BIT_1, 0);
	memset(&pfinfo, 0, sizeof(struct pfiommu_info));
	block_addr = fetch_info.addr;
	pfinfo.dev = &isp_k_param->isp_pdev->dev;
	pfinfo.mfd[0] = block_addr.img_fd;
	pfinfo.mfd[1] = 0;
	pfinfo.mfd[2] = 0;
	if (pfinfo.mfd[0] != 0) {
		/*mapping iommu buffer*/
		ret = pfiommu_get_sg_table(&pfinfo);
		if (ret) {
			pr_err("map iommu fetch buffer failed.\n");
			return -EFAULT;
		}

		ret = pfiommu_get_addr(&pfinfo);
		if (ret) {
			pr_err("map iommu fetch get addr failed.\n");
			return -EFAULT;
		}

		fetch_info.addr.chn0 = pfinfo.iova[0] + block_addr.chn0;
		fetch_info.addr.chn1 = pfinfo.iova[0] + block_addr.chn1;
		fetch_info.addr.chn2 = pfinfo.iova[0] + block_addr.chn2;
	}

	pr_debug("fetch: y_pitch: %d, word_num: %d yaddr :%x",
		fetch_info.pitch.chn0, fetch_info.mipi_word_num,
		fetch_info.addr.chn0);

	isp_k_param->fetch_pfinfo.iova[0] =
		pfinfo.iova[0];
	isp_k_param->fetch_raw_phys_addr = pfinfo.iova[0]
		+ block_addr.chn1;
	isp_k_param->fetch_pfinfo.size[0] = pfinfo.size[0];
	isp_k_param->fetch_pfinfo.dev = &isp_k_param->isp_pdev->dev;

	ISP_REG_MWR(ISP_FETCH_PARAM, 0xF0, (fetch_info.color_format << 4));


	ISP_REG_WR(ISP_FETCH_SLICE_Y_ADDR, fetch_info.addr.chn0);
	ISP_REG_WR(ISP_FETCH_SLICE_Y_PITCH, fetch_info.pitch.chn0);
	ISP_REG_WR(ISP_FETCH_SLICE_U_ADDR, fetch_info.addr.chn1);
	ISP_REG_WR(ISP_FETCH_SLICE_U_PITCH, fetch_info.pitch.chn1);
	ISP_REG_WR(ISP_FETCH_SLICE_V_ADDR, fetch_info.addr.chn2);
	ISP_REG_WR(ISP_FETCH_SLICE_V_PITCH, fetch_info.pitch.chn2);
	ISP_REG_WR(ISP_FETCH_MIPI_WORD_INFO,
		(fetch_info.mipi_word_num & 0xFFFF));
	ISP_REG_WR(ISP_FETCH_MIPI_BYTE_INFO,
		(fetch_info.mipi_byte_rel_pos & 0x0F));

	if (fetch_info.bypass)
		ISP_REG_OWR(ISP_FETCH_PARAM, BIT_0);
	else
		ISP_REG_MWR(ISP_FETCH_PARAM, BIT_0, 0);

	return ret;
}

static int32_t isp_k_fetch_slice_size(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t size = 0;
	struct isp_img_size slice_size = {0, 0};

	ret = copy_from_user((void *)&slice_size,
		param->property_param, sizeof(struct isp_img_size));
	if (ret != 0) {
		pr_info("read copy_from_user error, ret = 0x%x\n",
				(uint32_t)ret);
		return -1;
	}

	size = ((slice_size.height & 0xFFFF) << 16)
		| (slice_size.width & 0xFFFF);

	ISP_REG_WR(ISP_FETCH_SLICE_SIZE, size);

	return ret;
}

static int32_t isp_k_fetch_start_isp(struct isp_io_param *param,
		struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;
	uint32_t fetch_start = 0;

	/* Only in raw capture this function will be called*/
	isp_k_param->is_raw_capture = 1;
	ret = copy_from_user((void *)&fetch_start,
		param->property_param, sizeof(uint32_t));
	if (ret != 0) {
		pr_info("copy_from_user error, ret = 0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (fetch_start) {
		ISP_REG_OWR(ISP_RGB_AFM_PARAM, BIT_0);
		ISP_REG_OWR(ISP_ANTI_FLICKER_NEW_PARAM0, BIT_0);
		ISP_REG_OWR(ISP_BINNING_PARAM, BIT_0);
		ISP_REG_OWR(ISP_HIST_PARAM, BIT_0);
		ISP_REG_OWR(ISP_FETCH_START, BIT_0);
	}
	else
		ISP_REG_MWR(ISP_FETCH_START, BIT_0, 0);
	pr_debug("fetch started\n");
	return ret;
}

int32_t isp_k_cfg_fetch(struct isp_io_param *param,
		struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_fetch: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_fetch: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_FETCH_BLOCK:
		ret = isp_k_fetch_block(param, isp_k_param);
		break;
	case ISP_PRO_FETCH_SLICE_SIZE:
		ret = isp_k_fetch_slice_size(param);
		break;
	case ISP_PRO_FETCH_START_ISP:
		ret = isp_k_fetch_start_isp(param, isp_k_param);
		break;
	default:
		pr_info("not supported, property is %x.\n", param->property);
		ret = -1;
		break;
	}

	return ret;
}
