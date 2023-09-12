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
#include "isp_reg.h"
#include "dcam_reg.h"
#include "cam_types.h"
#include "cam_block.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "GAMMA: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int dcam_k_gamma_block(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	uint32_t val = 0;
	uint32_t i = 0;
	struct isp_dev_gamma_info_v1 *p;

	if (param == NULL)
		return -EPERM;

	idx = param->idx;
	p = &(param->gamma_info_v1.gamma_info);

	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;
	if (g_dcam_bypass[idx] & (1 << _E_GAMMA))
		p->bypass = 1;

	/* only cfg mode and buf0 is selected. */
	DCAM_REG_MWR(idx, DCAM_BUF_CTRL, BIT_4, BIT_4);
	DCAM_REG_MWR(idx, DCAM_FGAMMA10_PARAM, BIT_0, p->bypass);
	if (p->bypass)
		return 0;

	DCAM_REG_MWR(idx, DCAM_BUF_CTRL, 0x300, 0 << 8);
	for (i = 0; i < ISP_FRGB_GAMMA_PT_NUM_V1 - 1; i++) {
		val = ((p->gain_r[i] & 0x3FF) << 10) | (p->gain_r[i + 1] & 0x3FF);
		DCAM_REG_WR(idx, DCAM_FGAMMA10_TABLE + i * 4, val);
	}

	DCAM_REG_MWR(idx, DCAM_BUF_CTRL, 0x300, 1 << 8);
	for (i = 0; i < ISP_FRGB_GAMMA_PT_NUM_V1 - 1; i++) {
		val = ((p->gain_g[i] & 0x3FF) << 10) | (p->gain_g[i + 1] & 0x3FF);
		DCAM_REG_WR(idx, DCAM_FGAMMA10_TABLE + i * 4, val);
	}

	DCAM_REG_MWR(idx, DCAM_BUF_CTRL, 0x300, 2 << 8);
	for (i = 0; i < ISP_FRGB_GAMMA_PT_NUM_V1 - 1; i++) {
		val = ((p->gain_b[i] & 0x3FF) << 10) | (p->gain_b[i + 1] & 0x3FF);
		DCAM_REG_WR(idx, DCAM_FGAMMA10_TABLE + i * 4, val);
	}

	val = DCAM_REG_RD(idx, DCAM_BUF_CTRL);
	val = val & BIT_20;
	DCAM_REG_MWR(idx, DCAM_BUF_CTRL, BIT_20, ~val);

	return ret;
}

int dcam_k_cfg_gamma(struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;

	switch (param->property) {
	case ISP_PRO_GAMMA_BLOCK:
		/* online mode not need mutex, response faster
		 * Offline need mutex to protect param
		 */
		if (p->offline == 0) {
			ret = copy_from_user((void *)&(p->gamma_info_v1.gamma_info),
				param->property_param,
				sizeof(p->gamma_info_v1.gamma_info));
			if (ret) {
				pr_err("fail to copy from user ret=0x%x\n", (unsigned int)ret);
				return -EPERM;
			}
			if (p->idx == DCAM_HW_CONTEXT_MAX || param->scene_id == PM_SCENE_CAP)
				return 0;
			if (g_dcam_bypass[p->idx] & (1 << _E_GAMMA))
				p->gamma_info_v1.gamma_info.bypass = 1;
			ret = dcam_k_gamma_block(p);
		} else {
			mutex_lock(&p->param_lock);
			ret = copy_from_user((void *)&(p->gamma_info_v1.gamma_info),
				param->property_param,
				sizeof(p->gamma_info_v1.gamma_info));
			if (ret) {
				mutex_unlock(&p->param_lock);
				pr_err("fail to copy from user ret=0x%x\n", (unsigned int)ret);
				return -EPERM;
			}
			mutex_unlock(&p->param_lock);
		}
		break;
	default:
		pr_err("fail to support cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
