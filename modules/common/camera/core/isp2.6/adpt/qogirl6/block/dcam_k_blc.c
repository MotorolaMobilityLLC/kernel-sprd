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
#define pr_fmt(fmt) "BLC: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__


int dcam_k_blc_block(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	unsigned int val = 0;
	struct dcam_dev_blc_info *p;

	if (param == NULL)
		return -EPERM;

	idx = param->idx;
	p = &(param->blc.blc_info);
	DCAM_REG_MWR(idx, DCAM_BLC_PARA_R_B, BIT_31,
		(p->bypass) << 31);
	if (p->bypass)
		return 0;

	val = ((p->b & 0x3FFF) << 16) | (p->r & 0x3FFF);
	DCAM_REG_WR(idx, DCAM_BLC_PARA_R_B, val);

	val = ((p->gb & 0x3FFF) << 16) | (p->gr & 0x3FFF);
	DCAM_REG_WR(idx, DCAM_BLC_PARA_G, val);

	return ret;
}

int dcam_k_cfg_blc(struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;

	switch (param->property) {
	case DCAM_PRO_BLC_BLOCK:
		/* online mode not need mutex, response faster
		 * Offline need mutex to protect param
		 */
		if (p->offline == 0) {
			ret = copy_from_user((void *)&(p->blc.blc_info),
				param->property_param,
				sizeof(p->blc.blc_info));
			if (ret) {
				pr_err("fail to copy from user ret=0x%x\n",
					(unsigned int)ret);
				return -EPERM;
			}
			if (p->idx == DCAM_HW_CONTEXT_MAX || (g_dcam_bypass[p->idx] & (1 << _E_BLC)))
				return 0;
			ret = dcam_k_blc_block(p);
		} else {
			mutex_lock(&p->param_lock);
			ret = copy_from_user((void *)&(p->blc.blc_info),
				param->property_param,
				sizeof(p->blc.blc_info));
			if (ret) {
				mutex_unlock(&p->param_lock);
				pr_err("fail to copy from user ret=0x%x\n",
					(unsigned int)ret);
				return -EPERM;
			}
			mutex_unlock(&p->param_lock);
		}
		break;
	default:
		pr_err("fail to support cmd id:%d\n",
			param->property);
		break;
	}

	return ret;
}
