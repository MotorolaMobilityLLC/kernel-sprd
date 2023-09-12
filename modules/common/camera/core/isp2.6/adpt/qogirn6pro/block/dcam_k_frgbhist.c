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
#include "dcam_path.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "FRGB_HIST: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

enum {
	_UPDATE_ROI = BIT(0),
};

int dcam_k_frgbhist_block(struct dcam_dev_param *param)
{
	struct dcam_sw_context *sw_ctx = NULL;
	int ret = 0;
	uint32_t idx = 0;
	uint32_t val = 0;
	struct isp_dev_hist2_info *p = NULL;

	if (param == NULL)
		return -1;

	idx = param->idx;
	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;
	p = &(param->hist_roi.hist_roi_info);

	pr_debug("dcam%d, frgb hist roi bypass:%d (%d %d %d %d)\n", idx,
		p->bypass,
		p->hist_roi.start_x, p->hist_roi.start_y,
		p->hist_roi.end_x, p->hist_roi.end_y);

	DCAM_REG_MWR(idx, DCAM_HIST_ROI_CTRL0, BIT_0, p->bypass);
	if (p->bypass)
		return 0;

	/*
	 * use hardware slow motion feature
	 * TODO: handle skip_num not equal to slowmotion_count - 1
	 */
	sw_ctx = param->dev;
	if (sw_ctx && p->skip_num > 0 && sw_ctx->slowmotion_count) {
		pr_debug("DCAM%u frgb hist ignore skip_num %u, slowmotion_count %u\n",
			sw_ctx->hw_ctx_id, p->skip_num, sw_ctx->slowmotion_count);
		p->skip_num = 0;
	}

	val = (0 << 12) | ((p->skip_num & 0xff) << 4) | (1 << 2) | ((p->mode & 1) << 1);
	DCAM_REG_MWR(idx, DCAM_HIST_ROI_CTRL0, 0x1ffe, val);

	DCAM_REG_MWR(idx, DCAM_HIST_ROI_CTRL1, BIT_1, 1);

	dcam_path_skip_num_set(param->dev, DCAM_PATH_FRGB_HIST, p->skip_num);
	pr_debug("skip num %d\n", p->skip_num);

	DCAM_REG_MWR(idx, DCAM_HIST_ROI_START, 0x1fff1fff,
			((p->hist_roi.start_y & 0x1fff) << 16) |
			(p->hist_roi.start_x & 0x1fff));

	p->hist_roi.end_y &= ~1;
	p->hist_roi.end_x &= ~1;

	DCAM_REG_MWR(idx, DCAM_HIST_ROI_END, 0x1fff1fff,
			((p->hist_roi.end_y & 0x1fff) << 16) |
			(p->hist_roi.end_x & 0x1fff));

	return ret;

}

int dcam_k_frgbhist_roi(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	struct isp_dev_hist2_info *p = NULL;

	if (param == NULL)
		return -1;
	idx = param->idx;
	if (idx >= DCAM_HW_CONTEXT_MAX)
		return 0;

	/* update ? */
	if (!(param->hist_roi.update & _UPDATE_ROI))
		return 0;
	param->hist_roi.update &= (~(_UPDATE_ROI));
	p = &(param->hist_roi.hist_roi_info);
	DCAM_REG_MWR(idx, DCAM_HIST_ROI_CTRL0, BIT_0, p->bypass);
	if (p->bypass)
		return 0;

	pr_debug("dcam%d, frgb hist roi (%d %d %d %d)\n", idx,
		p->hist_roi.start_x, p->hist_roi.start_y,
		p->hist_roi.end_x, p->hist_roi.end_y);

	DCAM_REG_MWR(idx, DCAM_HIST_ROI_START, 0x1fff1fff,
			((p->hist_roi.start_y & 0x1fff) << 16) |
			(p->hist_roi.start_x & 0x1fff));


	DCAM_REG_MWR(idx, DCAM_HIST_ROI_END, 0x1fff1fff,
			((p->hist_roi.end_y & 0x1fff) << 16) |
			(p->hist_roi.end_x & 0x1fff));

	return ret;
}

int dcam_k_cfg_frgbhist(struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;
	struct dcam_sw_context *sw_ctx = NULL;
	unsigned long flags = 0;
	struct isp_dev_hist2_info cur;
	struct dcam_path_desc *path = NULL;

	switch (param->property) {
	case ISP_PRO_HIST2_BLOCK:
		ret = copy_from_user((void *)&cur,
				param->property_param,
				sizeof(struct isp_dev_hist2_info));
		if (ret) {
			pr_err("fail to copy from user, ret=0x%x\n",
				(unsigned int)ret);
			return -EPERM;
		}

		sw_ctx = (struct dcam_sw_context *)p->dev;
		path = &sw_ctx->path[DCAM_PATH_FRGB_HIST];

		spin_lock_irqsave(&path->size_lock, flags);
		p->hist_roi.hist_roi_info = cur;
		p->hist_roi.update |= _UPDATE_ROI;
		spin_unlock_irqrestore(&path->size_lock, flags);

		pr_debug("dcam%d, w:%d h:%d\n", p->idx, cur.hist_roi.end_x ,cur.hist_roi.end_y);

		if (p->idx == DCAM_HW_CONTEXT_MAX)
			return 0;
		if (g_dcam_bypass[p->idx] & (1 << _E_RGBHIST))
			p->hist_roi.hist_roi_info.bypass = 1;
		if (atomic_read(&sw_ctx->state) != STATE_RUNNING) {
			ret = dcam_k_frgbhist_block(p);
			if (ret)
				pr_err("fail to config frgb hist reg\n");
			pr_debug("dcam%d config frgb hist bypass %d, win (%d %d %d %d)\n",
				p->idx, cur.bypass,
				cur.hist_roi.start_x, cur.hist_roi.start_y,
				cur.hist_roi.end_x, cur.hist_roi.end_y);
		} else {
			pr_debug("dcam%d re-config frgb hist bypass %d, win (%d %d %d %d)\n",
				p->idx, cur.bypass,
				cur.hist_roi.start_x, cur.hist_roi.start_y,
				cur.hist_roi.end_x, cur.hist_roi.end_y);
		}
		break;

	default:
		pr_err("fail to support property %d\n",
			param->property);
		ret = -EINVAL;
		break;
	}

	return ret;
}

