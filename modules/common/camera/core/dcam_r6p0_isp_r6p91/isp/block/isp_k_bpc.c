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

static int32_t isp_k_bpc_block(struct isp_io_param *param)
{
	int32_t ret = 0;
	uint32_t val = 0;
	uint32_t i = 0;
	struct isp_dev_bpc_info bpc_info;

	memset(&bpc_info, 0x00, sizeof(bpc_info));

	ret = copy_from_user((void *)&bpc_info,
		param->property_param, sizeof(bpc_info));
	if (ret != 0) {
		pr_info("copy error, ret=0x%x\n", (uint32_t)ret);
		return -1;
	}

	ISP_REG_MWR(ISP_BPC_PARAM, BIT_0, bpc_info.bypass);

	ISP_REG_MWR(ISP_BPC_PARAM, BIT_1, bpc_info.pvd_bypass << 1);

	ISP_REG_MWR(ISP_BPC_PARAM, BIT_4
		| BIT_5, bpc_info.bpc_mode << 4);

	ISP_REG_MWR(ISP_BPC_PARAM, BIT_10 | BIT_9 | BIT_8,
		bpc_info.mask_mode << 8);

	val = ((bpc_info.kmax & 0x7) << 20)
		| ((bpc_info.kmin & 0x7) << 16);
	ISP_REG_MWR(ISP_BPC_PARAM, 0x770000, val);

	ISP_REG_MWR(ISP_BPC_NEW_OLD_SEL, BIT_0, bpc_info.new_old_sel);

	ISP_REG_MWR(ISP_BPC_NEW_OLD_SEL, BIT_1, bpc_info.map_done_sel << 1);

	val = ((bpc_info.safe_factor & 0x1F) << 4)
		| (bpc_info.flat_factor & 0x7);
	ISP_REG_WR(ISP_BPC_FACTOR, val);

	val = ((bpc_info.dead_coeff & 0x7) << 4)
		| (bpc_info.spike_coeff & 0x7);
	ISP_REG_WR(ISP_BPC_COEFF, val);

	for (i = 0; i < 8; i++) {
		val = ((bpc_info.lut_level[i] & 0x3FF) << 20)
			| ((bpc_info.slope_k[i] & 0x3FF) << 10)
			| (bpc_info.intercept_b[i] & 0x3FF);
		ISP_REG_WR(ISP_BPC_LUTWORD0 + i * 4, val);
	}

	ISP_REG_MWR(ISP_BPC_CFG, BIT_10 | BIT_9 | BIT_8, bpc_info.delta << 8);

	ISP_REG_MWR(ISP_BPC_CFG, BIT_7, bpc_info.bpc_map_fifo_clr << 7);

	ISP_REG_MWR(ISP_BPC_CFG, BIT_6 | BIT_5 | BIT_4, bpc_info.ktimes << 4);

	ISP_REG_MWR(ISP_BPC_CFG, 0x7, bpc_info.cntr_threshold);

	ISP_REG_MWR(ISP_BPC_CFG, BIT_3, bpc_info.bad_map_hw_fifo_clr_en << 3);

	ISP_REG_WR(ISP_BPC_MAP_ADDR, bpc_info.bpc_map_addr_new);

	ISP_REG_MWR(ISP_BPC_CFG, 0xFFFF0000, bpc_info.bad_pixel_num << 16);

	return ret;
}

int32_t isp_k_cfg_bpc(struct isp_io_param *param)
{
	int32_t ret = 0;

	if (!param) {
		pr_info("isp_k_cfg_bpc: param is null error.\n");
		return -1;
	}

	if (param->property_param == NULL) {
		pr_info("isp_k_cfg_bpc: property_param is null error.\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_BPC_BLOCK:
		ret = isp_k_bpc_block(param);
		break;
	default:
		pr_info("fail cmd id:%d, not supported.\n", param->property);
		break;
	}

	return ret;
}
