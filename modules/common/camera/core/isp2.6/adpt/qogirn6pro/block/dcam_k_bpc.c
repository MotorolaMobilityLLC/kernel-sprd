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
#include "isp_hw.h"

#include "dcam_reg.h"
#include "dcam_interface.h"
#include "cam_block.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "BPC: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int dcam_k_bpc_block(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx;
	int i = 0;
	uint32_t val = 0;
	struct dcam_dev_bpc_info_v1 *p;

	idx = param->idx;
	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;
	p = &(param->bpc_n6pro.bpc_param_n6pro.bpc_info);

	/* debugfs bpc not bypass then write*/
	if (g_dcam_bypass[idx] & (1 << _E_BPC))
		p->bpc_bypass = 1;

	/* following bit can be 0 only if bpc_bypss is 0 */
	if (p->bpc_bypass == 1)
		p->bpc_double_bypass = 1;
	if (p->bpc_double_bypass == 1)
		p->bpc_three_bypass = 1;
	if (p->bpc_three_bypass == 1)
		p->bpc_four_bypass = 1;

	val = (p->bpc_bypass & 0x1) |
		((p->bpc_double_bypass & 0x1) << 1) |
		((p->bpc_three_bypass & 0x1) << 2) |
		((p->bpc_four_bypass & 0x1) << 3);
	DCAM_REG_MWR(idx, DCAM_BPC_PARAM, 0xF, val);
	val = DCAM_REG_RD(idx, DCAM_BPC_PARAM);
	if (p->bpc_bypass)
		return 0;

	val = ((p->bpc_mode & 0x3) << 4) |
		((p->bpc_is_mono_sensor & 0x1) << 6) |
		((p->bpc_pos_out_en & 0x1) << 16);
	DCAM_REG_MWR(idx, DCAM_BPC_PARAM, 0x10070, val);

	val = p->bad_pixel_num;
	DCAM_REG_WR(idx, DCAM_BPC_MAP_CTRL, val);

	for (i = 0; i < 4; i++) {
		val = (p->bpc_bad_pixel_th[i] & 0x3FF);
		DCAM_REG_WR(idx, DCAM_BPC_BAD_PIXEL_TH0 + i * 4, val);
	}

	val = (p->bpc_ig_th & 0x3FF) |
		((p->bpc_flat_th & 0x3FF) << 10) |
		((p->bpc_shift[2] & 0xF) << 20) |
		((p->bpc_shift[1] & 0xF) << 24) |
		((p->bpc_shift[0] & 0xF) << 28);
	DCAM_REG_WR(idx, DCAM_BPC_FLAT_TH, val);

	val = (p->bpc_edgeratio_hv & 0x1FF) |
		((p->bpc_edgeratio_rd & 0x1FF) << 16);
	DCAM_REG_WR(idx, DCAM_BPC_EDGE_RATIO0, val);

	val = (p->bpc_edgeratio_g & 0x1FF) |
		((p->bpc_edgeratio_dirc & 0x1FF) << 16);
	DCAM_REG_WR(idx, DCAM_BPC_EDGE_RATIO1, val);

	val = (p->bpc_difflimit & 0x3FF) |
		((p->bpc_diffcoeff_limit & 0x1F) << 16) |
		((p->bpc_diffcoeff_detect & 0x1F) << 24);
	DCAM_REG_WR(idx, DCAM_BPC_BAD_PIXEL_PARAM, val);

	val = (p->bpc_lowcoeff & 0x3F) |
		((p->bpc_lowoffset & 0xFF) << 8) |
		((p->bpc_highcoeff & 0x3F) << 16) |
		((p->bpc_highoffset & 0xFF) << 24);
	DCAM_REG_WR(idx, DCAM_BPC_GDIF_TH, val);

	for (i = 0; i < 8; i++) {
		val = ((p->bpc_lut_level[i] & 0x3FF) << 20) |
			((p->bpc_slope_k[i] & 0x3FF) << 10) |
			(p->bpc_intercept_b[i] & 0x3FF);
		DCAM_REG_WR(idx, DCAM_BPC_LUTWORD0 + i * 4, val);
	}

	val = p->bpc_map_addr & 0xFFFFFFF0;
	DCAM_REG_WR(idx, DCAM_BPC_MAP_ADDR, val);

	val = p->bpc_bad_pixel_pos_out_addr & 0xFFFFFFF0;
	DCAM_REG_WR(idx, DCAM_BPC_OUT_ADDR, val);

	return ret;
}

int dcam_k_bpc_ppi_param(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = param->idx;
	uint32_t val = 0;
	struct dcam_bpc_ppi_info *p;

	p = &(param->bpc.bpc_ppi_info);

	if (!p->bpc_ppi_en)
		return 0;

	val = p->bpc_ppi_start_row
		| p->bpc_ppi_end_row << 16;
	DCAM_REG_WR(idx, DCAM_BPC_PPI_RANG, val);

	val = p->bpc_ppi_start_col
		| p->bpc_ppi_end_col << 16;
	DCAM_REG_WR(idx, DCAM_BPC_PPI_RANG1, val);

	val = (p->bpc_ppi_en & 1) << 7;
	DCAM_REG_MWR(idx, DCAM_BPC_PARAM, BIT_7, val);

	return ret;
}


int dcam_k_cfg_bpc(struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	void *dst_ptr;
	ssize_t dst_size;
	FUNC_DCAM_PARAM sub_func = NULL;

	switch (param->property) {
	case DCAM_PRO_BPC_BLOCK:
	{
		dst_ptr = (void *)&p->bpc_n6pro.bpc_param_n6pro.bpc_info;
		dst_size = sizeof(struct dcam_dev_bpc_info_v1);
		sub_func = dcam_k_bpc_block;
		break;
	}
	case DCAM_PRO_BPC_PPI_PARAM:
	{
		dst_ptr = (void *)&p->bpc.bpc_ppi_info;
		dst_size = sizeof(struct dcam_bpc_ppi_info);
		sub_func = dcam_k_bpc_ppi_param;
		break;
	}
	default:
		pr_err("fail to support property %d\n",
			param->property);
		ret = -EINVAL;
		return ret;
	}

	if (p->offline == 0) {
		ret = copy_from_user(dst_ptr,
				param->property_param,
				dst_size);
		if (ret) {
			pr_err("fail to copy from user, ret = %d\n", ret);
			goto exit;
		}
		if (p->idx == DCAM_HW_CONTEXT_MAX)
			return 0;
		if (sub_func)
			ret = sub_func(p);
	} else {
		mutex_lock(&p->param_lock);
		ret = copy_from_user(dst_ptr,
				param->property_param,
				dst_size);
		if (ret)
			pr_err("fail to copy from user, ret = %d\n", ret);

		mutex_unlock(&p->param_lock);
	}
exit:
	return ret;
}
