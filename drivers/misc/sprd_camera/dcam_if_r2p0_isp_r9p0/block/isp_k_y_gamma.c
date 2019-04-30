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
#define pr_fmt(fmt) "YGAMMA: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define ISP_YUV_YGAMMA_BUF0      0
#define ISP_YUV_YGAMMA_BUF1      1

static int isp_k_pingpang_yuv_ygamma(struct coordinate_xy *nodes,
		struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	unsigned int i = 0, j = 0;
	unsigned long ybuf_addr = 0;
	struct coordinate_xy *p_nodes = NULL;

	if (!nodes) {
		ret = -1;
		pr_err("fail to get nodes\n");
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

	for (i = 0; i < ISP_PINGPANG_YUV_YGAMMA_NUM - 1; i++) {
		ISP_REG_WR(idx, ybuf_addr + j,
				p_nodes[i].node_y & 0xff);
		j += 4;
	}

	if (isp_k_param->yuv_ygamma_buf_id)
		ISP_REG_OWR(idx, ISP_YGAMMA_PARAM, BIT_1);
	else
		ISP_REG_MWR(idx, ISP_YGAMMA_PARAM, BIT_1, 0);

	return ret;
}

static int isp_k_ygamma_block(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_ygamma_info ygamma_info;

	memset(&ygamma_info, 0x00, sizeof(ygamma_info));
	ret = copy_from_user((void *)&ygamma_info,
		param->property_param, sizeof(ygamma_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}

	ISP_REG_MWR(idx, ISP_YGAMMA_PARAM, BIT_0, ygamma_info.bypass);
	if (ygamma_info.bypass)
		return 0;

	ret = isp_k_pingpang_yuv_ygamma(ygamma_info.nodes, isp_k_param, idx);
	if (ret != 0) {
		pr_err("fail to pingpang yuv ygamma, ret = %d\n",
			ret);
		return -1;
	}

	return ret;
}

int isp_k_cfg_ygamma(struct isp_io_param *param,
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
	case ISP_PRO_YGAMMA_BLOCK:
		ret = isp_k_ygamma_block(param, isp_k_param, com_idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
