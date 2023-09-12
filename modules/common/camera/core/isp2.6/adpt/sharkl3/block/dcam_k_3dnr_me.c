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

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <sprd_mm.h>

#include "isp_hw.h"
#include "dcam_core.h"
#include "dcam_reg.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "3DNR: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

#define DCAM_3DNR_ROI_SIZE_ALIGN 16u
#define DCAM_3DNR_ROI_LINE_CUT 32u

struct roi_size {
	uint32_t roi_width;
	uint32_t roi_height;
};

/* [DCAM_ID] [project_Mode] */
struct roi_size roi_max_size_info[2][2] = {
	{
		{2304, 1728},
		{4608, 3456}
	},
	{
		{2128, 1600},
		{4256, 3200}
	}
};

/* input: rect: bin path crop size, include start point(x,y), and size(w,h)
 */
void dcam_k_3dnr_set_roi(struct isp_img_rect rect,
	uint32_t project_mode, uint32_t idx)
{
	uint32_t roi_w_max, roi_h_max;
	uint32_t roi_w, roi_h, roi_x = 0, roi_y = 0;
	uint32_t pmode = 0;

	/* get max roi size
	 * max roi size should be half of normal value if project_mode is off
	 */
	pmode = DCAM_REG_RD(idx, NR3_FAST_ME_PARAM0) & 0x3;
	if (pmode == 1)
		project_mode = 1;

	roi_w_max = roi_max_size_info[idx][project_mode].roi_width;
	roi_h_max = roi_max_size_info[idx][project_mode].roi_height;

	/* get roi and align to 16 pixels */
	roi_w = ALIGN_DOWN(min(roi_w_max, rect.w), DCAM_3DNR_ROI_SIZE_ALIGN);
	roi_h = ALIGN_DOWN(min(roi_h_max, rect.h), DCAM_3DNR_ROI_SIZE_ALIGN);

	/* get offset */
	roi_x = (rect.w - roi_w) >> 1;
	roi_y = (rect.h - roi_h) >> 1;
	roi_x = ALIGN_DOWN(roi_x + rect.x, 2);
	roi_y = ALIGN_DOWN(roi_y + rect.y, 2);

	/* leave 32 lines to make sure BIN DONE comes earlier than NR3 DONE */
	roi_h = max(roi_h, DCAM_3DNR_ROI_LINE_CUT) - DCAM_3DNR_ROI_LINE_CUT;

	/* almost done! */
	DCAM_REG_WR(idx, NR3_FAST_ME_ROI_PARAM0, roi_x << 16 | roi_y);
	DCAM_REG_WR(idx, NR3_FAST_ME_ROI_PARAM1, roi_w << 16 | roi_h);

	pr_info("DCAM%u 3DNR ROI %u %u %u %u\n",
		idx, roi_x, roi_y, roi_w, roi_h);
}

int dcam_k_3dnr_me(struct dcam_dev_param *param)
{
	int ret = 0;
	uint32_t idx = 0;
	struct dcam_sw_context *dev = NULL;
	struct dcam_dev_3dnr_me *p = NULL;
	struct dcam_path_desc *path;
	struct isp_img_rect rect;
	uint32_t val = 0;

	if (param == NULL)
		return -EPERM;

	idx = param->idx;
	dev = (struct dcam_sw_context *)param->dev;

	/* debugfs bypass nr3 */
	if (g_dcam_bypass[idx] & (1 << _E_NR3))
		return 0;

	p = &param->nr3.nr3_me;
	DCAM_REG_MWR(idx, NR3_FAST_ME_PARAM,
			BIT(0), (p->bypass & 0x1));
	if (p->bypass)
		return 0;
	val = ((p->nr3_channel_sel & 0x3) << 2)
		|(p->nr3_project_mode & 0x3);
	DCAM_REG_MWR(idx, NR3_FAST_ME_PARAM0, 0xF, val);
	/* update ROI according to project_mode */
	path = &dev->path[DCAM_PATH_3DNR];
	rect.x = path->in_trim.start_x;
	rect.y = path->in_trim.start_y;
	rect.w = path->in_trim.size_x;
	rect.h = path->in_trim.size_y;
	if ((rect.x + rect.w) <= dev->cap_info.cap_size.size_x &&
		(rect.y + rect.h) <= dev->cap_info.cap_size.size_y)
		dcam_k_3dnr_set_roi(rect, p->nr3_project_mode, idx);

	return ret;
}

int dcam_k_cfg_3dnr_me(struct isp_io_param *param, struct dcam_dev_param *p)
{
	int ret = 0;

	switch (param->property) {
	case DCAM_PRO_3DNR_ME:
		if (p->offline == 0) {
			ret = copy_from_user((void *)&(p->nr3.nr3_me),
					param->property_param,
					sizeof(p->nr3.nr3_me));
			if (ret) {
				pr_err("fail to copy, ret=0x%x\n", (unsigned int)ret);
				return -EPERM;
			}
			if (p->idx == DCAM_HW_CONTEXT_MAX)
				return 0;

			ret = dcam_k_3dnr_me(p);
		} else {
			mutex_lock(&p->param_lock);
			ret = copy_from_user((void *)&(p->nr3.nr3_me),
					param->property_param,
					sizeof(p->nr3.nr3_me));
			if (ret) {
				mutex_unlock(&p->param_lock);
				pr_err("fail to copy, ret=0x%x\n", (unsigned int)ret);
				return -EPERM;
			}
			mutex_unlock(&p->param_lock);
		}

		break;
	default:
		pr_err("fail to support property %d\n", param->property);
		ret = -EINVAL;
		break;
	}

	return ret;
}
