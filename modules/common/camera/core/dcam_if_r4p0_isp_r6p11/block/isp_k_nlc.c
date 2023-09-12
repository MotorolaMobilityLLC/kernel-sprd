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
#define pr_fmt(fmt) "NLC: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static int isp_k_nlc_block(struct isp_io_param *param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int i = 0;
	struct isp_dev_nlc_info nlc_info;

	memset(&nlc_info, 0x00, sizeof(nlc_info));
	ret = copy_from_user((void *)&nlc_info, param->property_param,
				sizeof(nlc_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	ISP_REG_MWR(idx, ISP_NLC_PARA, BIT_0, nlc_info.bypass);
	if (nlc_info.bypass) {
		pr_debug("nlc bypass.\n");
		isp_dbg_s_ori_byp(SBLK_BYPASS, raw_nlc, idx);
		return 0;
	}

	for (i = 0; i < 9; i++) {
		val = ((nlc_info.node.r_node[i*3] & 0x3FF) << 20)
			| ((nlc_info.node.r_node[i*3+1] & 0x3FF) << 10)
			| (nlc_info.node.r_node[i*3+2] & 0x3FF);
		ISP_REG_WR(idx, ISP_NLC_PARA_R0 + i * 4, val);

		val = ((nlc_info.node.g_node[i*3] & 0x3FF) << 20)
			| ((nlc_info.node.g_node[i*3+1] & 0x3FF) << 10)
			| (nlc_info.node.g_node[i*3+2] & 0x3FF);
		ISP_REG_WR(idx, ISP_NLC_PARA_G0 + i * 4, val);

		val = ((nlc_info.node.b_node[i*3] & 0x3FF) << 20)
			| ((nlc_info.node.b_node[i*3+1] & 0x3FF) << 10)
			| (nlc_info.node.b_node[i*3+2] & 0x3FF);
		ISP_REG_WR(idx, ISP_NLC_PARA_B0 + i * 4, val);

		val = ((nlc_info.node.l_node[i*3] & 0x3FF) << 20)
			| ((nlc_info.node.l_node[i*3+1] & 0x3FF) << 10)
			| (nlc_info.node.l_node[i*3+2] & 0x3FF);
		ISP_REG_WR(idx, ISP_NLC_PARA_L0 + i * 4, val);
	}

	val = ((nlc_info.node.r_node[27] & 0x3FF) << 20)
		| ((nlc_info.node.r_node[28] & 0x3FF) << 10);
	ISP_REG_WR(idx, ISP_NLC_PARA_R9, val);

	val = ((nlc_info.node.g_node[27] & 0x3FF) << 20)
		| ((nlc_info.node.g_node[28] & 0x3FF) << 10);
	ISP_REG_WR(idx, ISP_NLC_PARA_G9, val);

	val = ((nlc_info.node.b_node[27] & 0x3FF) << 20)
		| ((nlc_info.node.b_node[28] & 0x3FF) << 10);
	ISP_REG_WR(idx, ISP_NLC_PARA_B9, val);

	return ret;
}

int isp_k_cfg_nlc(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get param\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get property_param\n");
		return -EPERM;
	}

	switch (param->property) {
	case ISP_PRO_NLC_BLOCK:
		ret = isp_k_nlc_block(param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
