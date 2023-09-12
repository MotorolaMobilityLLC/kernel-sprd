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
#include "dcam_path.h"
#include "cam_types.h"
#include "dcam_core.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "AEM: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

enum {
	_UPDATE_WIN = BIT(2),
};

int dcam_k_aem_bypass(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;

	if (param == NULL)
		return -EPERM;

	idx = param->idx;
	DCAM_REG_MWR(idx, DCAM_AEM_FRM_CTRL0, BIT_0,
		param->aem.bypass & BIT_0);

	return ret;
}

int dcam_k_aem_mode(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	uint32_t mode = 0;

	if (param == NULL)
		return -1;

	idx = param->idx;
	mode = param->aem.mode;
	DCAM_REG_MWR(idx, DCAM_AEM_FRM_CTRL0, BIT_2, mode << 2);

	/* mode: 0 - single; 1 - multi mode */
	if (mode == AEM_MODE_SINGLE)
		/* trigger aem_sgl_start. */
		DCAM_REG_MWR(idx, DCAM_AEM_FRM_CTRL1, BIT_0, 0x1);
	else
		/* trigger multi frame works after skip_num */
		DCAM_REG_MWR(idx, DCAM_AEM_FRM_CTRL0, BIT_3, (0x1 << 3));

	return ret;
}

int dcam_k_aem_win(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	uint32_t val = 0;
	struct dcam_dev_aem_win *p = NULL;

	if (param == NULL)
		return -1;
	/* update ? */
	if (!(param->aem.update & _UPDATE_WIN))
		return 0;
	param->aem.update &= (~(_UPDATE_WIN));

	idx = param->idx;
	p = &(param->aem.win_info);
	val = ((p->offset_y & 0x1FFF) << 16) |
			(p->offset_x & 0x1FFF);
	DCAM_REG_WR(idx, DCAM_AEM_OFFSET, val);

	val = ((p->blk_height & 0xFF) << 8) |
			(p->blk_width & 0xFF);
	DCAM_REG_WR(idx, DCAM_AEM_BLK_SIZE, val);

	val = ((p->blk_num_y & 0xFF) << 8) |
			(p->blk_num_x & 0xFF);
	DCAM_REG_WR(idx, DCAM_AEM_BLK_NUM, val);

	return ret;
}

int dcam_k_aem_skip_num(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	uint32_t val = 0;

	if (param == NULL)
		return -1;

	idx = param->idx;

	pr_info("DCAM%u AEM set skip_num %u\n", idx, param->aem.skip_num);

	val = (param->aem.skip_num & 0xF) << 4;
	DCAM_REG_MWR(idx, DCAM_AEM_FRM_CTRL0, 0xF0, val);

	/* It is better to set aem_skip_num_clr when new skip_num is set. */
	DCAM_REG_MWR(idx, DCAM_AEM_FRM_CTRL1, BIT_1, 1 << 1);
	/* hw skip, no need software skip again
	 * dcam_path_skip_num_set(param->dev,
	 *	DCAM_PATH_AEM, param->aem.skip_num);
	 */

	return ret;
}

int dcam_k_aem_rgb_thr(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	uint32_t val = 0;
	struct dcam_dev_aem_thr *p = NULL;

	if (param == NULL)
		return -1;

	idx = param->idx;

	p = &(param->aem.aem_info);
	val = ((p->aem_r_thr.low_thr & 0x3FF) << 16) |
		(p->aem_r_thr.high_thr & 0x3FF);
	DCAM_REG_WR(idx, DCAM_AEM_RED_THR, val);

	val = ((p->aem_b_thr.low_thr & 0x3FF) << 16) |
		(p->aem_b_thr.high_thr & 0x3FF);
	DCAM_REG_WR(idx, DCAM_AEM_BLUE_THR, val);

	val = ((p->aem_g_thr.low_thr & 0x3FF) << 16) |
		(p->aem_g_thr.high_thr & 0x3FF);
	DCAM_REG_WR(idx, DCAM_AEM_GREEN_THR, val);

	return ret;
}

int dcam_k_cfg_aem(struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	void *pcpy;
	int size;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	FUNC_DCAM_PARAM sub_func = NULL;

	switch (param->property) {
	case DCAM_PRO_AEM_BYPASS:
		pcpy = (void *)&(p->aem.bypass);
		size = sizeof(p->aem.bypass);
		sub_func = dcam_k_aem_bypass;
		break;
	case DCAM_PRO_AEM_MODE:
		pcpy = (void *)&(p->aem.mode);
		size = sizeof(p->aem.mode);
		sub_func = dcam_k_aem_mode;
		break;
	case DCAM_PRO_AEM_WIN:
		pcpy = (void *)&(p->aem.win_info);
		size = sizeof(p->aem.win_info);
		p->aem.update |= _UPDATE_WIN;
		sub_func = dcam_k_aem_win;
		break;
	case DCAM_PRO_AEM_SKIPNUM:
		pcpy = (void *)&(p->aem.skip_num);
		size = sizeof(p->aem.skip_num);
		sub_func = dcam_k_aem_skip_num;
		break;
	case DCAM_PRO_AEM_RGB_THR:
		pcpy = (void *)&(p->aem.aem_info);
		size = sizeof(p->aem.aem_info);
		sub_func = dcam_k_aem_rgb_thr;
		break;
	default:
		pr_err("fail to support property %d\n", param->property);
		return -EINVAL;
	}

	dcam_sw_ctx = (struct dcam_sw_context *)p->dev;
	if (!p->offline &&
		(atomic_read(&dcam_sw_ctx->state) == STATE_RUNNING) &&
		(param->property == DCAM_PRO_AEM_WIN)) {
		unsigned long flags = 0;
		struct dcam_dev_aem_win cur;
		struct dcam_path_desc *path = &dcam_sw_ctx->path[DCAM_PATH_AEM];

		ret = copy_from_user(&cur, param->property_param, size);
		if (ret) {
			pr_err("fail to copy from user ret=0x%x\n", (unsigned int)ret);
			return -EPERM;
		}

		pr_debug("dcam%d re-config aem win (%d %d %d %d %d %d)\n",
			dcam_sw_ctx->hw_ctx_id, cur.offset_x, cur.offset_y,
			cur.blk_num_x, cur.blk_num_y,
			cur.blk_width, cur.blk_height);

		spin_lock_irqsave(&path->size_lock, flags);
		p->aem.win_info = cur;
		p->aem.update |= _UPDATE_WIN;
		spin_unlock_irqrestore(&path->size_lock, flags);

		return ret;
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
