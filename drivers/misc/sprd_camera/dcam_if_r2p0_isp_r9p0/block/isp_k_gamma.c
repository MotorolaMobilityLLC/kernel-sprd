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
#define pr_fmt(fmt) "GAMMA: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define NON_3DNR_TEST
#define ISP_FRGB_GAMC_BUF0             0
#define ISP_FRGB_GAMC_BUF1             1

int isp_k_pingpang_frgb_gamc(struct gamc_curve_info *nodes,
		struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int i = 0, j = 0;
	int gamma_node_r = 0;
	int gamma_node_g = 0;
	int gamma_node_b = 0;
	unsigned long r_buf_addr = 0;
	unsigned long g_buf_addr = 0;
	unsigned long b_buf_addr = 0;
	unsigned short *p_nodes_r = NULL;
	unsigned short *p_nodes_g = NULL;
	unsigned short *p_nodes_b = NULL;
	uint32_t work_mode = 0;
	uint32_t scene_id = 0;

	work_mode = ISP_GET_MODE_ID(idx);
	scene_id = ISP_GET_SCENE_ID(idx);

	if (work_mode == ISP_CFG_MODE) {
		r_buf_addr = ISP_FGAMMA_R_BUF0_CH0;
		g_buf_addr = ISP_FGAMMA_G_BUF0_CH0;
		b_buf_addr = ISP_FGAMMA_B_BUF0_CH0;
		isp_k_param->full_gamma_buf_id_prv =
			ISP_FRGB_GAMC_BUF0;
	} else {
		if (scene_id == ISP_SCENE_PRE) {
			if (isp_k_param->full_gamma_buf_id_prv ==
				ISP_FRGB_GAMC_BUF0) {
				r_buf_addr = ISP_FGAMMA_R_BUF1_CH0;
				g_buf_addr = ISP_FGAMMA_G_BUF1_CH0;
				b_buf_addr = ISP_FGAMMA_B_BUF1_CH0;
				isp_k_param->full_gamma_buf_id_prv =
					ISP_FRGB_GAMC_BUF1;
			} else {
				r_buf_addr = ISP_FGAMMA_R_BUF0_CH0;
				g_buf_addr = ISP_FGAMMA_G_BUF0_CH0;
				b_buf_addr = ISP_FGAMMA_B_BUF0_CH0;
				isp_k_param->full_gamma_buf_id_prv =
					ISP_FRGB_GAMC_BUF0;
			}
		} else {
			if (isp_k_param->full_gamma_buf_id_cap ==
				ISP_FRGB_GAMC_BUF0) {
				r_buf_addr = ISP_FGAMMA_R_BUF1_CH0;
				g_buf_addr = ISP_FGAMMA_G_BUF1_CH0;
				b_buf_addr = ISP_FGAMMA_B_BUF1_CH0;
				isp_k_param->full_gamma_buf_id_cap =
					ISP_FRGB_GAMC_BUF1;
			} else {
				r_buf_addr = ISP_FGAMMA_R_BUF0_CH0;
				g_buf_addr = ISP_FGAMMA_G_BUF0_CH0;
				b_buf_addr = ISP_FGAMMA_B_BUF0_CH0;
				isp_k_param->full_gamma_buf_id_cap =
					ISP_FRGB_GAMC_BUF0;
			}
		}
	}

	p_nodes_r = nodes->nodes_r;
	p_nodes_g = nodes->nodes_g;
	p_nodes_b = nodes->nodes_b;

	for (i = 0, j = 0; i < (ISP_PINGPANG_FRGB_GAMC_NUM - 1); i++, j += 4) {
		gamma_node_r = (((p_nodes_r[i] & 0xFF) << 8)
				| (p_nodes_r[i + 1] & 0xFF)) & 0xFFFF;
		gamma_node_g = (((p_nodes_g[i] & 0xFF) << 8)
				| (p_nodes_g[i + 1] & 0xFF)) & 0xFFFF;
		gamma_node_b = (((p_nodes_b[i] & 0xFF) << 8)
				| (p_nodes_b[i + 1] & 0xFF)) & 0xFFFF;

		ISP_REG_WR(idx, r_buf_addr + j, gamma_node_r);
		ISP_REG_WR(idx, g_buf_addr + j, gamma_node_g);
		ISP_REG_WR(idx, b_buf_addr + j, gamma_node_b);
	}

	if (work_mode == ISP_CFG_MODE) {
		val = isp_k_param->full_gamma_buf_id_prv;
	} else {
		if (scene_id == ISP_SCENE_PRE)
			val = isp_k_param->full_gamma_buf_id_prv;
		else
			val = isp_k_param->full_gamma_buf_id_cap;
	}

	ISP_REG_MWR(idx, ISP_GAMMA_PARAM, BIT_1, val << 1);

	return ret;
}

static int isp_k_gamma_block(struct isp_io_param *param,
			struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_gamma_info *gamma_info_ptr = NULL;

	gamma_info_ptr = (struct isp_dev_gamma_info *)
				isp_k_param->full_gamma_buf_addr;

	ret = copy_from_user((void *)gamma_info_ptr,
		param->property_param, sizeof(struct isp_dev_gamma_info));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		return -1;
	}
	ISP_REG_MWR(idx, ISP_GAMMA_PARAM, BIT_0,
			gamma_info_ptr->bypass);

	if (gamma_info_ptr->bypass)
		return 0;

	ret = isp_k_pingpang_frgb_gamc(&gamma_info_ptr->gamc_nodes,
					isp_k_param, idx);
	if (ret != 0) {
		pr_err("fail to get pingpang frgb gamc, ret = %d\n",
			ret);
		return -1;
	}

	return ret;
}

int isp_k_cfg_gamma(struct isp_io_param *param,
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
	case ISP_PRO_GAMMA_BLOCK:
		ret = isp_k_gamma_block(param, isp_k_param, com_idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n",
			param->property);
		break;
	}

	return ret;
}
