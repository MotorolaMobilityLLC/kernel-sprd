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
#define pr_fmt(fmt) "GRGB: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__


static int dcam_k_grgb_block(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = param->idx;
	int i = 0;
	uint32_t val = 0;
	struct isp_dev_grgb_info *p;

	p = &(param->grgb.grgb_info);
	/* debugfs bpc not bypass then write*/
	if (g_dcam_bypass[idx] & (1 << _E_GRGB))
		p->bypass = 1;

	/*valid when bpc_mode_en_gc=1*/
	DCAM_REG_MWR(idx, DCAM_GRGB_CTRL, BIT_0, p->bypass);
	if (p->bypass)
		return 0;

	val = ((p->slash_edge_thr & 0x7F) << 24) |
		(p->check_sum_clr & 0x01) << 23 |
		((p->hv_edge_thr & 0x7F) << 16) |
		((p->diff_thd & 0x3FF) << 1);
	DCAM_REG_MWR(idx, DCAM_GRGB_CTRL, 0x7FFF07FE, val);

	val = ((p->gb_ratio & 0x1F) << 26) |
		((p->hv_flat_thr & 0x3FF) << 16) |
		((p->gr_ratio & 0x1F) << 10) |
		(p->slash_flat_thr & 0x3FF);
	DCAM_REG_WR(idx, DCAM_GRGB_CFG0, val);

	for (i = 0; i < 3; i++) {
		val = ((p->lum.curve_t[i][0] & 0x3FF) << 20) |
			((p->lum.curve_t[i][1] & 0x3FF) << 10) |
			(p->lum.curve_t[i][2] & 0x3FF);
		DCAM_REG_WR(idx, DCAM_GRGB_LUM_FLAT_T + i * 8, val);

		val = ((p->lum.curve_t[i][3] & 0x3FF) << 15) |
			((p->lum.curve_r[i][0] & 0x1F) << 10) |
			((p->lum.curve_r[i][1] & 0x1F) << 5) |
			(p->lum.curve_r[i][2] & 0x1F);
		DCAM_REG_WR(idx, DCAM_GRGB_LUM_FLAT_R + i * 8, val);
	}

	for (i = 0; i < 3; i++) {
		val = ((p->frez.curve_t[i][0] & 0x3FF) << 20) |
			((p->frez.curve_t[i][1] & 0x3FF) << 10) |
			(p->frez.curve_t[i][2] & 0x3FF);
		DCAM_REG_WR(idx, DCAM_GRGB_FREZ_FLAT_T + i * 8, val);

		val = ((p->frez.curve_t[i][3] & 0x3FF) << 15) |
			((p->frez.curve_r[i][0] & 0x1F) << 10) |
			((p->frez.curve_r[i][1] & 0x1F) << 5) |
			(p->frez.curve_r[i][2] & 0x1F);
		DCAM_REG_WR(idx, DCAM_GRGB_FREZ_FLAT_R + i * 8, val);
	}

	return ret;
}

int dcam_k_cfg_grgb(struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	void *dst_ptr;
	ssize_t dst_size;
	FUNC_DCAM_PARAM sub_func = NULL;

	switch (param->property) {
	case ISP_PRO_GRGB_BLOCK:
		dst_ptr = (void *)&(p->grgb.grgb_info);
		dst_size = sizeof(struct isp_dev_grgb_info);
		sub_func = dcam_k_grgb_block;
		break;
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
