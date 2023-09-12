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
#define pr_fmt(fmt) "AWBC: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int dcam_k_awbc_block(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	uint32_t val = 0;
	struct dcam_dev_awbc_info *p = NULL;

	if (param == NULL)
		return -EPERM;

	idx = param->idx;
	p = &(param->awbc.awbc_info);
	DCAM_REG_MWR(idx, ISP_AWBC_PARAM, BIT_0,
			p->awbc_bypass & 1);
	if (p->awbc_bypass)
		return 0;

	val = ((p->gain.b & 0x3FFF) << 16) |
		(p->gain.r & 0x3FFF);
	DCAM_REG_WR(idx, ISP_AWBC_GAIN0, val);

	val = ((p->gain.gb & 0x3FFF) << 16) |
		(p->gain.gr & 0x3FFF);
	DCAM_REG_WR(idx, ISP_AWBC_GAIN1, val);

	val = ((p->thrd.b & 0x3FF) << 20) |
		((p->thrd.gr & 0x3FF) << 10) |
		(p->thrd.r & 0x3FF);
	DCAM_REG_WR(idx, ISP_AWBC_THRD, val);

	val = ((p->gain_offset.b & 0x7FF) << 16) |
		(p->gain_offset.r & 0x7FF);
	DCAM_REG_WR(idx, ISP_AWBC_OFFSET0, val);

	val = ((p->gain_offset.gb & 0x7FF) << 16) |
		(p->gain_offset.gr & 0x7FF);
	DCAM_REG_WR(idx, ISP_AWBC_OFFSET1, val);

	return ret;
}

int dcam_k_awbc_gain(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	uint32_t val = 0;
	struct img_rgb_info *p = NULL;

	if (param == NULL)
		return -EPERM;

	idx = param->idx;
	p = &(param->awbc.awbc_info.gain);
	val = ((p->b & 0x3FFF) << 16) | (p->r & 0x3FFF);
	DCAM_REG_WR(idx, ISP_AWBC_GAIN0, val);

	val = ((p->gb & 0x3FFF) << 16) | (p->gr & 0x3FFF);
	DCAM_REG_WR(idx, ISP_AWBC_GAIN1, val);

	return ret;
}

int dcam_k_awbc_bypass(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	uint32_t bypass = 0;

	if (param == NULL)
		return -EPERM;

	idx = param->idx;
	bypass = param->awbc.awbc_info.awbc_bypass;
	DCAM_REG_MWR(idx, ISP_AWBC_PARAM, BIT_0, bypass & 1);

	return ret;
}

int dcam_k_cfg_awbc(struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	void *pcpy;
	int size;
	FUNC_DCAM_PARAM sub_func = NULL;

	switch (param->property) {
	case DCAM_PRO_AWBC_BLOCK:
		pcpy = (void *)&(p->awbc.awbc_info);
		size = sizeof(p->awbc.awbc_info);
		sub_func = dcam_k_awbc_block;
		break;
	case DCAM_PRO_AWBC_GAIN:
		pcpy = (void *)&(p->awbc.awbc_info.gain);
		size = sizeof(p->awbc.awbc_info.gain);
		sub_func = dcam_k_awbc_gain;
		break;
	case DCAM_PRO_AWBC_BYPASS:
		pcpy = (void *)&(p->awbc.awbc_info.awbc_bypass);
		size = sizeof(p->awbc.awbc_info.awbc_bypass);
		sub_func = dcam_k_awbc_bypass;
		break;
	default:
		pr_err("fail to support property %d\n",
			param->property);
		return -EINVAL;
	}

	if (p->offline == 0) {
		ret = copy_from_user(pcpy, param->property_param, size);
		if (ret) {
			pr_err("fail to copy from user ret=0x%x\n",
				(unsigned int)ret);
			return -EPERM;
		}

		if (p->idx == DCAM_HW_CONTEXT_MAX || (g_dcam_bypass[p->idx] & (1 << _E_AWBC)))
			return 0;

		ret = sub_func(p);
		return ret;
	}

	/* offline just save parameters in structure */
	mutex_lock(&p->param_lock);
	ret = copy_from_user(pcpy, param->property_param, size);
	if (ret) {
		mutex_unlock(&p->param_lock);
		pr_err("fail to copy from user ret=0x%x\n",
			(unsigned int)ret);
		return -EPERM;
	}
	mutex_unlock(&p->param_lock);

	return ret;
}
