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

#include "sprd_mm.h"
#include "sprd_isp_hw.h"
#include "isp_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "YGAMMA: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

#define ISP_YUV_YGAMMA_BUF0      0
#define ISP_YUV_YGAMMA_BUF1      1

static int isp_k_pingpang_yuv_ygamma(struct coordinate_xy *nodes,
		struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;
	unsigned int i = 0, j = 0;
	unsigned long ybuf_addr = 0;

	if (ISP_GET_MID(idx) == ISP_AP_MODE) {

		if (isp_k_param->yuv_ygamma_buf_id == ISP_YUV_YGAMMA_BUF0) {
			ybuf_addr = ISP_YGAMMA_BUF1_CH0;
			isp_k_param->yuv_ygamma_buf_id = ISP_YUV_YGAMMA_BUF1;
		} else {
			ybuf_addr = ISP_YGAMMA_BUF0_CH0;
			isp_k_param->yuv_ygamma_buf_id = ISP_YUV_YGAMMA_BUF0;
		}
	} else {
	/*
	 * In CFG MODE, we don't need to select parameter buffer.
	 * Just let HW to pick the correct buffer by itself.
	 * DO NOT use buffer 1 or it will cause capture frame
	 * all black problem.
	 *
	 */
		ybuf_addr = ISP_YGAMMA_BUF0_CH0;
		isp_k_param->yuv_ygamma_buf_id = ISP_YUV_YGAMMA_BUF0;
	}

	for (i = 0, j = 0; i < ISP_PINGPANG_YUV_YGAMMA_NUM; i++, j += 4)
		ISP_REG_WR(idx, ybuf_addr + j, nodes[i].node_y & 0xff);

	if (isp_k_param->yuv_ygamma_buf_id)
		ISP_REG_OWR(idx, ISP_YGAMMA_PARAM, BIT_1);
	else
		ISP_REG_MWR(idx, ISP_YGAMMA_PARAM, BIT_1, 0);

	return ret;
}

static int isp_k_ygamma_block(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;
	struct isp_dev_ygamma_info ygamma_info;

	memset(&ygamma_info, 0x00, sizeof(ygamma_info));

	ret = copy_from_user((void *)&ygamma_info,
		param->property_param, sizeof(ygamma_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_YGAMMA_PARAM, BIT_0, ygamma_info.bypass);
	if (ygamma_info.bypass) {
		isp_dbg_s_ori_byp(SBLK_BYPASS, yuv_gama, idx);
		pr_debug("bypass.\n");
		return 0;
	}

	return isp_k_pingpang_yuv_ygamma(ygamma_info.nodes, isp_k_param, idx);
}

int isp_k_cfg_ygamma(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param.\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property param.\n");
		return -EPERM;
	}

	if (!isp_k_param) {
		pr_err("fail to get isp_k_param.\n");
		return -EPERM;
	}

	switch (param->property) {
	case ISP_PRO_YGAMMA_BLOCK:
		ret = isp_k_ygamma_block(param, isp_k_param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
