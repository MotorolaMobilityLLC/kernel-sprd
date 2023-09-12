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
#include <linux/vmalloc.h>
#include "isp_block.h"

#define ISP_FRGB_GAMC_BUF0                0
#define ISP_FRGB_GAMC_BUF1                1

static int32_t isp_k_pingpang_frgb_gamc(struct gamc_curve_info *curve_info,
		struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;
	uint32_t i, j;
	uint32_t val = 0;
	int32_t gamma_node = 0;
	unsigned long r_buf_addr;
	unsigned long g_buf_addr;
	unsigned long b_buf_addr;
	struct coordinate_xy *p_nodes_r = NULL,
		*p_nodes_g = NULL, *p_nodes_b = NULL;

	if (!curve_info) {
		ret = -1;
		pr_info("isp_k_pingpang_frgb_gamc: node is null error .\n");
		return ret;
	}

	p_nodes_r = curve_info->nodes_r;
	p_nodes_g = curve_info->nodes_g;
	p_nodes_b = curve_info->nodes_b;

	if (isp_k_param->full_gamma_buf_id == ISP_FRGB_GAMC_BUF0) {
		r_buf_addr = ISP_FGAMMA_R_BUF1_CH0;
		g_buf_addr = ISP_FGAMMA_G_BUF1_CH0;
		b_buf_addr = ISP_FGAMMA_B_BUF1_CH0;
		isp_k_param->full_gamma_buf_id = ISP_FRGB_GAMC_BUF1;
	} else {
		r_buf_addr = ISP_FGAMMA_R_BUF0_CH0;
		g_buf_addr = ISP_FGAMMA_G_BUF0_CH0;
		b_buf_addr = ISP_FGAMMA_B_BUF0_CH0;
		isp_k_param->full_gamma_buf_id = ISP_FRGB_GAMC_BUF0;
	}

	for (i = 0, j = 0; i < (ISP_PINGPANG_FRGB_GAMC_NUM - 1);
						i++, j += 4) {
		gamma_node = (((p_nodes_r[i].node_y & 0xFF) << 8)
			| (p_nodes_r[i + 1].node_y & 0xFF)) & 0xFFFF;
		ISP_REG_WR(r_buf_addr + j, gamma_node);
		gamma_node = (((p_nodes_g[i].node_y & 0xFF) << 8)
			| (p_nodes_g[i + 1].node_y & 0xFF)) & 0xFFFF;
		ISP_REG_WR(g_buf_addr + j, gamma_node);
		gamma_node = (((p_nodes_b[i].node_y & 0xFF) << 8)
			| (p_nodes_b[i + 1].node_y & 0xFF)) & 0xFFFF;
		ISP_REG_WR(b_buf_addr + j, gamma_node);
	}

	val = ((isp_k_param->full_gamma_buf_id & 0x1) << 1)
		| ((isp_k_param->full_gamma_buf_id & 0x1) << 2)
		| ((isp_k_param->full_gamma_buf_id & 0x1) << 3);
	ISP_REG_MWR(ISP_GAMMA_PARAM, 0x0000000E, val);

	return ret;
}

static int32_t isp_k_gamma_block(struct isp_io_param *param,
		struct isp_k_block *isp_k_param)
{
	struct isp_dev_gamma_info *gamma_info_ptr = NULL;
	int32_t ret = 0;

	gamma_info_ptr = (struct isp_dev_gamma_info *)
				isp_k_param->full_gamma_buf_addr;

	ret = copy_from_user((void *)gamma_info_ptr,
		param->property_param, sizeof(struct isp_dev_gamma_info));
	if (ret != 0) {
		pr_info("copy error,ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	ret = isp_k_pingpang_frgb_gamc(&gamma_info_ptr->gamc_nodes,
				isp_k_param);
	if (ret != 0) {
		pr_info("pingpang error,ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_GAMMA_PARAM, BIT_0, gamma_info_ptr->bypass);

	return ret;

}

int32_t isp_k_cfg_gamma(struct isp_io_param *param,
		struct isp_k_block *isp_k_param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_gamma: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_gamma: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_GAMMA_BLOCK:
		ret = isp_k_gamma_block(param, isp_k_param);
		break;
	default:
		pr_info("fail cmd id:%d,not supported.\n", param->property);
		break;
	}

	return ret;
}
