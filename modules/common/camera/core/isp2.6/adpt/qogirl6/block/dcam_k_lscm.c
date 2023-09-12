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
#include "dcam_core.h"
#include "dcam_path.h"
#include "cam_types.h"
#include "cam_block.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "LSCM: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__


int dcam_k_lscm_bypass(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = param->idx;

	DCAM_REG_MWR(idx, DCAM_LSCM_FRM_CTRL0, BIT_0,
		param->lscm.bypass & BIT_0);

	return ret;
}

int dcam_k_lscm_monitor(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = param->idx;
	uint32_t mode = 0;
	uint32_t val = 0;

	DCAM_REG_MWR(idx, DCAM_LSCM_FRM_CTRL0, BIT_0, 0);

	mode = param->lscm.mode;
	DCAM_REG_MWR(idx, DCAM_LSCM_FRM_CTRL0, BIT_2, mode << 2);

	/* mode: 0 - single; 1 - multi mode */
	if (mode == 0)
		/* trigger lscm_sgl_start. */
		DCAM_REG_MWR(idx, DCAM_LSCM_FRM_CTRL1, BIT_0, 0x1);
	else
		/* trigger multi frame works after skip_num */
		DCAM_REG_MWR(idx, DCAM_LSCM_FRM_CTRL0, BIT_3, (0x1 << 3));

	val = (param->lscm.skip_num & 0xF) << 4;
	DCAM_REG_MWR(idx, DCAM_LSCM_FRM_CTRL0, 0xF0, val);

	/* It is better to set lscm_skip_num_clr when new skip_num is set. */
	DCAM_REG_MWR(idx, DCAM_LSCM_FRM_CTRL1, BIT_1, 1 << 1);

	val = ((param->lscm.offset_y & 0x1FFF) << 16) |
			(param->lscm.offset_x & 0x1FFF);
	DCAM_REG_WR(idx, DCAM_LSCM_OFFSET, val);

	val = ((param->lscm.blk_height & 0xFF) << 8) |
			(param->lscm.blk_width & 0xFF);
	DCAM_REG_WR(idx, DCAM_LSCM_BLK_SIZE, val);

	val = ((param->lscm.blk_num_y & 0xFF) << 8) |
			(param->lscm.blk_num_x & 0xFF);
	DCAM_REG_WR(idx, DCAM_LSCM_BLK_NUM, val);

	return ret;
}

int dcam_k_cfg_lscm(struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	void *pcpy;
	int size;
	FUNC_DCAM_PARAM sub_func = NULL;

	switch (param->property) {
	case DCAM_PRO_LSCM_BYPASS:
		pcpy = (void *)&(p->lscm.bypass);
		size = sizeof(p->lscm.bypass);
		sub_func = dcam_k_lscm_bypass;
		break;
	case DCAM_PRO_LSC_MONITOR:
		pcpy = (void *)&(p->lscm);
		size = sizeof(p->lscm);
		sub_func = dcam_k_lscm_monitor;
		break;
	default:
		pr_err("fail to support property %d\n",
			param->property);
		return -EINVAL;
	}

	if (p->offline == 0) {
		ret = copy_from_user(pcpy, param->property_param, size);
		if (ret) {
			pr_err("fail to copy, ret=0x%x\n", (unsigned int)ret);
			return -EPERM;
		}
		if (p->idx == DCAM_HW_CONTEXT_MAX)
			return 0;
		ret = sub_func(p);
	} else {
		mutex_lock(&p->param_lock);
		ret = copy_from_user(pcpy, param->property_param, size);
		if (ret) {
			mutex_unlock(&p->param_lock);
			pr_err("fail to copy, ret=0x%x\n", (unsigned int)ret);
			return -EPERM;
		}
		mutex_unlock(&p->param_lock);
	}

	return ret;
}
