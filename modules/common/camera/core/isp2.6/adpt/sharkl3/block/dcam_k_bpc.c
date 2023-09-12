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
	uint32_t idx = param->idx;
	int i = 0;
	uint32_t val = 0;
	struct dcam_dev_bpc_info_l3 *p;

	p = &(param->bpc.bpc_param.bpc_info_l3);
	/* debugfs bpc not bypass then write*/
	if (g_dcam_bypass[idx] & (1 << _E_BPC))
		p->bpc_bypass = 1;

	if (param->dcam_slice_mode == CAM_OFFLINE_SLICE_SW) {
		p->bpc_bypass = 1;
		p->bpc_mode_en_gc = 0;
		p->bpc_mode_en = 0;
	}

	val = ((p->bpc_mode_en_gc & 0x1) << 2) |
		((p->bpc_mode_en & 0x1) << 1) |
		(p->bpc_gc_cg_dis & 0x1);
	DCAM_REG_MWR(idx, ISP_BPC_GC_CFG, 0x7, val);
	if (p->bpc_mode_en == 0 && p->bpc_mode_en_gc == 0)
		return 0;

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
	DCAM_REG_MWR(idx, ISP_BPC_PARAM, 0xF, val);
	if (p->bpc_bypass)
		return 0;

	DCAM_REG_MWR(idx, ISP_BPC_PARAM, BIT_4 | BIT_5, p->bpc_mode << 4);
	DCAM_REG_MWR(idx, ISP_BPC_PARAM, BIT_6,
			p->pos_out_continue_mode << 6);
	DCAM_REG_MWR(idx, ISP_BPC_PARAM, BIT_7, p->is_mono_sensor << 7);
	DCAM_REG_MWR(idx, ISP_BPC_PARAM, BIT_9 |
			BIT_8, p->edge_hv_mode << 8);
	DCAM_REG_MWR(idx, ISP_BPC_PARAM, BIT_11 |
			BIT_10, p->edge_rd_mode << 10);

	DCAM_REG_MWR(idx, ISP_BPC_PARAM,
			0xF000, p->pos_out_skip_num << 12);
	DCAM_REG_MWR(idx, ISP_BPC_PARAM, 0xFF0000,
			p->rd_retain_num << 16);
	DCAM_REG_MWR(idx, ISP_BPC_PARAM, BIT_24, p->rd_max_len_sel << 24);
	DCAM_REG_MWR(idx, ISP_BPC_PARAM, BIT_25, p->wr_max_len_sel << 25);
	DCAM_REG_MWR(idx, ISP_BPC_PARAM, BIT_31, p->bpc_blk_mode << 31);

	for (i = 0; i < 4; i++) {
		val = ((p->double_badpixel_th[i] & 0x3FF) << 20) |
			((p->three_badpixel_th[i] & 0x3FF) << 10) |
			((p->four_badpixel_th[i] & 0x3FF));
		DCAM_REG_WR(idx, ISP_BPC_BAD_PIXEL_TH0 + i * 4, val);
	}

	val = ((p->shift[0] & 0xF) << 28) |
			((p->shift[1] & 0xF) << 24) |
			((p->shift[2] & 0xF) << 20) |
			((p->flat_th & 0x3FF) << 10) |
			((p->texture_th & 0x3FF));
	DCAM_REG_WR(idx, ISP_BPC_FLAT_TH, val);

	val = ((p->edge_ratio_rd & 0x1FF) << 16) |
			((p->edge_ratio_hv & 0x1FF));
	DCAM_REG_WR(idx, ISP_BPC_EDGE_RATIO, val);

	val = ((p->low_coeff & 0x7) << 24) |
			((p->high_coeff & 0x7) << 16) |
			((p->low_offset & 0xFF) << 8) |
			((p->high_offset & 0xFF));
	DCAM_REG_WR(idx, ISP_BPC_BAD_PIXEL_PARAM, val);

	val = ((p->max_coeff & 0x1F) << 16) |
			((p->min_coeff & 0x1F));

	DCAM_REG_WR(idx, ISP_BPC_BAD_PIXEL_COEFF, val);

	for (i = 0; i < 8; i++) {
		val = ((p->lut_level[i] & 0x3FF) << 20) |
				((p->slope_k[i] & 0x3FF) << 10) |
				(p->intercept_b[i] & 0x3FF);
		DCAM_REG_WR(idx, ISP_BPC_LUTWORD0 + i * 4, val);
	}

	DCAM_REG_MWR(idx, ISP_BPC_MAP_CTRL,
			BIT_0, p->bad_map_hw_fifo_clr_en);
	DCAM_REG_WR(idx, ISP_BPC_MAP_CTRL1, p->bad_pixel_num);

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
		dst_ptr = (void *)&p->bpc.bpc_param.bpc_info_l3;
		dst_size = sizeof(struct dcam_dev_bpc_info_l3);
		sub_func = dcam_k_bpc_block;
		break;
	}
	default:
		pr_err("fail to support property %d\n", param->property);
		ret = -EINVAL;
		return ret;
	}

	if (p->offline == 0) {
		if (copy_from_user(dst_ptr, param->property_param, dst_size)) {
			pr_err("fail to copy from user\n");
			ret = -1;
			goto exit;
		}

		if (p->idx == DCAM_HW_CONTEXT_MAX)
			return 0;

		if (sub_func)
			ret = sub_func(p);
	} else {
		mutex_lock(&p->param_lock);
		if (copy_from_user(dst_ptr, param->property_param, dst_size)) {
			pr_err("fail to copy from user\n");
			ret = -1;
		}
		mutex_unlock(&p->param_lock);
	}
exit:
	return ret;
}
