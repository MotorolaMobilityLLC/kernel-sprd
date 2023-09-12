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
#include "isp_block.h"


#define ISP_YUV_YGAMMA_BUF0      0
#define ISP_YUV_YGAMMA_BUF1      1

static int32_t isp_k_pingpang_yuv_ygamma(struct coordinate_xy *nodes,
		struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;
	uint32_t i = 0, j = 0;
	unsigned long ybuf_addr = 0;
	struct coordinate_xy *p_nodes = NULL;

	if (!nodes) {
		ret = -1;
		pr_info("isp_k_pingpang_yuv_ygamma: node is null error .\n");
		return ret;
	}
	p_nodes = nodes;

	if (isp_k_param->yuv_ygamma_buf_id == ISP_YUV_YGAMMA_BUF0) {
		ybuf_addr = ISP_YGAMMA_BUF1_CH0;
		isp_k_param->yuv_ygamma_buf_id = ISP_YUV_YGAMMA_BUF1;
	} else {
		ybuf_addr = ISP_YGAMMA_BUF0_CH0;
		isp_k_param->yuv_ygamma_buf_id = ISP_YUV_YGAMMA_BUF0;
	}

	for (i = 0, j = 0; i < ISP_PINGPANG_YUV_YGAMMA_NUM; i++, j += 4)
		ISP_REG_WR(ybuf_addr + j, p_nodes[i].node_y & 0xff);

	if (isp_k_param->yuv_ygamma_buf_id)
		ISP_REG_OWR(ISP_YGAMMA_PARAM, BIT_1);
	else
		ISP_REG_MWR(ISP_YGAMMA_PARAM, BIT_1, 0);

	return ret;
}

static int32_t isp_k_ygamma_block(struct isp_io_param *param,
	struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;
	struct isp_dev_ygamma_info ygamma_info;

	ret = copy_from_user((void *)&ygamma_info,
		param->property_param, sizeof(ygamma_info));
	if (ret != 0) {
		pr_info("copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	if (!ygamma_info.bypass) {
		ret = isp_k_pingpang_yuv_ygamma(ygamma_info.nodes, isp_k_param);
		if (ret != 0) {
			pr_info("pingpang error, ret=0x%x\n", (uint32_t)ret);
			return -1;
		}
	}
	ISP_REG_MWR(ISP_YGAMMA_PARAM, BIT_0, ygamma_info.bypass);

	return ret;
}

int32_t isp_k_cfg_ygamma(struct isp_io_param *param,
	struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_ygamma: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_ygamma: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_YGAMMA_BLOCK:
	case ISP_PRO_CONTRAST_BLOCK:
		ret = isp_k_ygamma_block(param, isp_k_param);
		break;
	default:
		pr_info("fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
