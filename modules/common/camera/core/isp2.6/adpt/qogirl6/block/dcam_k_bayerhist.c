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
#include "dcam_core.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "BAYER_HIST: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

enum {
	_UPDATE_ROI = BIT(0),
};

int dcam_k_bayerhist_block(struct dcam_dev_param *param)
{
	struct dcam_sw_context *sw_ctx = NULL;
	int ret = 0;
	uint32_t idx = 0;
	struct dcam_dev_hist_info *p;

	if (param == NULL)
		return -1;

	idx = param->idx;
	p = &(param->hist.bayerHist_info);
	DCAM_REG_MWR(idx, DCAM_HIST_FRM_CTRL0, BIT_0, p->hist_bypass);
	if (p->hist_bypass)
		return 0;

	/*
	 * use hardware slow motion feature
	 * TODO: handle skip_num not equal to slowmotion_count - 1
	 */
	sw_ctx = param->dev;
	if (p->hist_skip_num > 0 && sw_ctx->slowmotion_count) {
		pr_debug("DCAM%u HIST ignore skip_num %u, slowmotion_count %u\n",
			sw_ctx->hw_ctx_id, p->hist_skip_num, sw_ctx->slowmotion_count);
		p->hist_skip_num = 0;
	}

	DCAM_REG_MWR(idx, DCAM_HIST_FRM_CTRL0, 0xfc,
			((p->hist_skip_num & 0xf) << 4) |
			(p->hist_mul_enable << 3) |
			(p->hist_mode_sel << 2));

	DCAM_REG_MWR(idx, DCAM_BAYER_HIST_START, 0xffffffff,
			((p->bayer_hist_sty & 0x1fff) << 16) |
			(p->bayer_hist_stx & 0x1fff));

	DCAM_REG_MWR(idx, DCAM_BAYER_HIST_END, 0xffffffff,
			((p->bayer_hist_endy & 0x1fff) << 16) |
			(p->bayer_hist_endx & 0x1fff));

	DCAM_REG_MWR(idx, DCAM_HIST_FRM_CTRL1, 0x7,
			(p->hist_initial_clear << 2) |
			(p->hist_skip_num_clr << 1) |
			(p->hist_sgl_start << 0));
	return ret;
}

int dcam_k_bayerhist_roi(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	struct dcam_dev_hist_info *p = NULL;

	if (param == NULL)
		return -1;
	idx = param->idx;
	if(idx >= DCAM_HW_CONTEXT_MAX)
		return 0;

	if (!(param->hist.update & _UPDATE_ROI))
		return 0;
	param->hist.update &= (~(_UPDATE_ROI));
	p = &(param->hist.bayerHist_info);
	DCAM_REG_MWR(idx, DCAM_HIST_FRM_CTRL0, BIT_0, p->hist_bypass);
	if (p->hist_bypass)
		return 0;

	pr_debug("dcam%d, roi (%d %d %d %d)\n", idx,
		p->bayer_hist_stx, p->bayer_hist_sty,
		p->bayer_hist_endx, p->bayer_hist_endy);

	DCAM_REG_MWR(idx, DCAM_BAYER_HIST_START, 0xffffffff,
			((p->bayer_hist_sty & 0x1fff) << 16) |
			(p->bayer_hist_stx & 0x1fff));

	DCAM_REG_MWR(idx, DCAM_BAYER_HIST_END, 0xffffffff,
			((p->bayer_hist_endy & 0x1fff) << 16) |
			(p->bayer_hist_endx & 0x1fff));

	return ret;
}

int dcam_k_bayerhist_bypass(struct dcam_dev_param *p)
{
	int ret = 0;
	uint32_t idx = p->idx;
	uint32_t bypass = 0;

	bypass = p->hist.bayerHist_info.hist_bypass;
	DCAM_REG_MWR(idx, DCAM_HIST_FRM_CTRL0, BIT_0, bypass);

	return ret;
}

int dcam_k_cfg_bayerhist(struct isp_io_param *param,
			struct dcam_dev_param *p)
{
	int ret = 0;

	switch (param->property) {
	case DCAM_PRO_BAYERHIST_BYPASS:
		ret = copy_from_user((void *)&(p->hist.bayerHist_info.hist_bypass),
				param->property_param,
				sizeof(p->hist.bayerHist_info.hist_bypass));
		if (ret) {
			pr_err("fail to copy from user, ret=0x%x\n",
				(unsigned int)ret);
			return -EPERM;
		}
		dcam_k_bayerhist_bypass(p);
		break;
	case DCAM_PRO_BAYERHIST_BLOCK: {
		struct dcam_sw_context *sw_ctx = NULL;
		unsigned long flags = 0;
		struct dcam_dev_hist_info cur;
		struct dcam_path_desc *path;

		ret = copy_from_user((void *)&cur,
				param->property_param,
				sizeof(struct dcam_dev_hist_info));
		if (ret) {
			pr_err("fail to copy from user, ret=0x%x\n",
				(unsigned int)ret);
			return -EPERM;
		}

		sw_ctx = (struct dcam_sw_context *)p->dev;
		path = &sw_ctx->path[DCAM_PATH_HIST];

		spin_lock_irqsave(&path->size_lock, flags);
		p->hist.bayerHist_info = cur;
		p->hist.update |= _UPDATE_ROI;
		spin_unlock_irqrestore(&path->size_lock, flags);
		if (p->idx == DCAM_HW_CONTEXT_MAX)
			return 0;
		if (atomic_read(&sw_ctx->state) != STATE_RUNNING) {
			ret = dcam_k_bayerhist_block(p);
			pr_debug("dcam%d config hist %d, win (%d %d %d %d)\n",
				p->idx, cur.hist_bypass,
				cur.bayer_hist_stx, cur.bayer_hist_sty,
				cur.bayer_hist_endx, cur.bayer_hist_endy);
		} else {
			pr_debug("dcam%d re-config hist %d, win (%d %d %d %d)\n",
				p->idx, cur.hist_bypass,
				cur.bayer_hist_stx, cur.bayer_hist_sty,
				cur.bayer_hist_endx, cur.bayer_hist_endy);
		}
		break;
	}
	default:
		pr_err("fail to support property %d\n",
			param->property);
		ret = -EINVAL;
		break;
	}

	return ret;
}
