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
#define pr_fmt(fmt) "GAMMA: %d %d %s : "\
		fmt, current->pid, __LINE__, __func__

#define	ISP_FRGB_GAMC_BUF0				  0
#define	ISP_FRGB_GAMC_BUF1				  1

static int isp_k_pingpang_frgb_gamc(struct gamc_curve_info *nodes,
		struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;
	unsigned int val = 0;
	unsigned int i, j;
	int gamma_node_r = 0;
	int gamma_node_g = 0;
	int gamma_node_b = 0;
	unsigned long r_buf_addr;
	unsigned long g_buf_addr;
	unsigned long b_buf_addr;
	struct coordinate_xy *p_nodes_r = NULL;
	struct coordinate_xy *p_nodes_g = NULL;
	struct coordinate_xy *p_nodes_b = NULL;

	if (ISP_GET_MID(idx) == ISP_AP_MODE) {
		if (isp_k_param->full_gamma_buf_id
				== ISP_FRGB_GAMC_BUF0) {
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
	} else {
	/*
	 * In CFG MODE, we don't need to select parameter buffer.
	 * Just let HW to pick the correct buffer by itself.
	 * DO NOT use buffer 1 or it will cause capture frame
	 * all black problem.
	 *
	 */
		r_buf_addr = ISP_FGAMMA_R_BUF0_CH0;
		g_buf_addr = ISP_FGAMMA_G_BUF0_CH0;
		b_buf_addr = ISP_FGAMMA_B_BUF0_CH0;
		isp_k_param->full_gamma_buf_id = ISP_FRGB_GAMC_BUF0;
	}

	p_nodes_r = nodes->nodes_r;
	p_nodes_g = nodes->nodes_g;
	p_nodes_b = nodes->nodes_b;

	for (i = 0, j = 0; i < (ISP_PINGPANG_FRGB_GAMC_NUM - 1); i++, j += 4) {
		gamma_node_r = (((p_nodes_r[i].node_y & 0xFF) << 8)
				| (p_nodes_r[i + 1].node_y & 0xFF)) & 0xFFFF;
		gamma_node_g = (((p_nodes_g[i].node_y & 0xFF) << 8)
				| (p_nodes_g[i + 1].node_y & 0xFF)) & 0xFFFF;
		gamma_node_b = (((p_nodes_b[i].node_y & 0xFF) << 8)
				| (p_nodes_b[i + 1].node_y & 0xFF)) & 0xFFFF;

		ISP_REG_WR(idx, r_buf_addr + j, gamma_node_r);
		ISP_REG_WR(idx, g_buf_addr + j, gamma_node_g);
		ISP_REG_WR(idx, b_buf_addr + j, gamma_node_b);
	}

	val = (isp_k_param->full_gamma_buf_id & 0x1) << 1;
	ISP_REG_MWR(idx, ISP_GAMMA_PARAM, 0x00000002, val);

	return ret;
}

static int isp_k_gamma_block(struct isp_io_param *param,
			struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;
	struct isp_dev_gamma_info *gamma_info_ptr = NULL;

	gamma_info_ptr = (struct isp_dev_gamma_info *)
				isp_k_param->full_gamma_buf_addr;

	ret = copy_from_user((void *)gamma_info_ptr,
		param->property_param, sizeof(struct isp_dev_gamma_info));
	if (ret != 0) {
		pr_err("fail to copy param from user, ret = %d\n", ret);
		return -EPERM;
	}
	ISP_REG_MWR(idx, ISP_GAMMA_PARAM, BIT_0,
				gamma_info_ptr->bypass);

	if (gamma_info_ptr->bypass) {
		isp_dbg_s_ori_byp(SBLK_BYPASS, full_gama, idx);
		return 0;
	}

	ret = isp_k_pingpang_frgb_gamc(&gamma_info_ptr->gamc_nodes,
					isp_k_param, idx);
	if (ret != 0) {
		pr_err("fail to in pingpang, ret = %d\n", ret);
		return ret;
	}

	return ret;
}

int isp_k_cfg_gamma(struct isp_io_param *param,
		struct isp_k_block *isp_k_param, enum isp_id idx)
{
	int ret = 0;

	if (!param) {
		pr_err("fail to get null param.\n");
		return -EPERM;
	}

	if (param->property_param == NULL) {
		pr_err("fail to get null property param.\n");
		return -EPERM;
	}

	switch (param->property) {
	case ISP_PRO_GAMMA_BLOCK:
		ret = isp_k_gamma_block(param, isp_k_param, idx);
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}

