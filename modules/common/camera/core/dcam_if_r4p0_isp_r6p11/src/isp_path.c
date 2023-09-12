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
#include "isp_path.h"
#include "isp_buf.h"
#include "isp_statis_buf.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_PATH: %d: %d %s: "\
	fmt, current->pid, __LINE__, __func__

#define ISP_PATH_ALIGN_SIZE		2
#define ISP_ALIGNTO(size)	((size) & ~(ISP_PATH_ALIGN_SIZE - 1))
#define  MAX(_x, _y) (((_x) > (_y)) ? (_x) : (_y))
#define  MIN(_x, _y) (((_x) < (_y)) ? (_x) : (_y))

#define ISP_ALIGN(x, n)  ((~(n-1) & 0xFFFFFFFF) & ((x) + (n-1)))
#define ISP_ALIGN_4BYTE(x)      ISP_ALIGN(x, 4)

static int set_offline_scl_size(struct isp_path_desc *path,
				struct isp_offline_desc *off_desc)
{
	if (!path || !off_desc) {
		pr_err("fail to get valid param, NULL\n");
		return -EFAULT;
	}

	/* src size */
	path->src.w = path->in_size.w;
	path->src.h = path->in_size.h;
	/* trim0 size */
	path->trim0_info.start_x = path->in_rect.x;
	path->trim0_info.start_y = path->in_rect.y;
	path->trim0_info.size_x = path->in_rect.w;
	path->trim0_info.size_y = path->in_rect.h;
	/* dst size */
	path->dst.w = path->out_size.w;
	path->dst.h = path->out_size.h;
	/* trim1 size */
	path->trim1_info.start_x = 0;
	path->trim1_info.start_y = 0;
	path->trim1_info.size_x = path->dst.w;
	path->trim1_info.size_y = path->dst.h;
	path->path_sel = 1;

	return 0;
}

static int isp_path_offline_cowork_cap(struct isp_path_desc *pre,
				       struct isp_path_desc *vid,
				       struct isp_path_desc *cap,
				       struct isp_offline_desc *off_desc)
{
	int rtn = 0;
	struct isp_path_desc *max_path = NULL;
	uint32_t min_recty = 0;
	uint32_t tmph = 0;

	if (!pre || !vid || !cap || !off_desc) {
		pr_err("fail to get valid param, NULL\n");
		return -EFAULT;
	}

	if (pre->in_rect.w > vid->in_rect.w)
		max_path = pre;
	else
		max_path = vid;

	if (max_path->in_rect.w < cap->in_rect.w)
		max_path = cap;

	if (pre->in_rect.y < vid->in_rect.y)
		min_recty = pre->in_rect.y;
	else
		min_recty = vid->in_rect.y;

	if (min_recty > cap->in_rect.y)
		min_recty = cap->in_rect.y;

	off_desc->src.w = max_path->in_size.w;
	off_desc->src.h = max_path->in_size.h;
	off_desc->border.left_border = max_path->in_rect.x;
	off_desc->border.up_border = min_recty;
	off_desc->border.right_border = off_desc->src.w -
		max_path->in_rect.w - off_desc->border.left_border;
	tmph = max_path->in_rect.w * max_path->in_size.h /
		max_path->in_size.w;
	tmph = ISP_ALIGNTO(tmph);
	off_desc->border.down_border = off_desc->src.h - tmph -
		off_desc->border.up_border;
	off_desc->dst.w = max_path->in_rect.w;
	off_desc->dst.h = tmph;

	rtn = set_offline_scl_size(pre, off_desc);
	if (rtn) {
		pr_err("fail to set pre_path scl size\n");
		return rtn;
	}

	rtn = set_offline_scl_size(vid, off_desc);
	if (rtn) {
		pr_err("fail to set vid_path scl size\n");
		return rtn;
	}

	rtn = set_offline_scl_size(cap, off_desc);
	if (rtn) {
		pr_err("fail to set cap_path scl size\n");
		return rtn;
	}

	off_desc->valid = 1;

	return rtn;
}

static int isp_path_offline_cowork(struct isp_path_desc *pre,
				   struct isp_path_desc *vid,
				   struct isp_offline_desc *off_desc)
{
	int rtn = 0;
	struct isp_path_desc *max_path = NULL;
	struct isp_path_desc *min_path = NULL;
	uint32_t tmph = 0;

	if (!pre || !vid || !off_desc) {
		pr_err("fail to get valid param, NULL\n");
		return -EFAULT;
	}

	if (pre->in_rect.w > vid->in_rect.w) {
		max_path = pre;
		min_path = vid;
	} else {
		max_path = vid;
		min_path = pre;
	}

	off_desc->src.w = max_path->in_size.w;
	off_desc->src.h = max_path->in_size.h;
	off_desc->border.left_border = max_path->in_rect.x;
	off_desc->border.up_border = max_path->in_rect.y > min_path->in_rect.y ?
		min_path->in_rect.y : max_path->in_rect.y;
	off_desc->border.right_border = off_desc->src.w -
		max_path->in_rect.w - off_desc->border.left_border;
	tmph = max_path->in_rect.w * max_path->in_size.h /
		max_path->in_size.w;
	tmph = ISP_ALIGNTO(tmph);
	off_desc->border.down_border = off_desc->src.h - tmph -
		off_desc->border.up_border;
	off_desc->dst.w = max_path->in_rect.w;
	off_desc->dst.h = tmph;

	rtn = set_offline_scl_size(max_path, off_desc);
	if (rtn) {
		pr_err("fail to set max_path scl size\n");
		return rtn;
	}

	rtn = set_offline_scl_size(min_path, off_desc);
	if (rtn) {
		pr_err("fail to set min_path scl size\n");
		return rtn;
	}

	off_desc->valid = 1;

	return rtn;
}

#if 0
static int set_online_scl_size(struct isp_path_desc *path,
	struct isp_path_desc *scl0)
{
	if (!path || !scl0) {
		pr_err("fail to get valid param, NULL\n");
		return -EFAULT;
	}

	/* src size */
	path->src.w = scl0->trim1_info.size_x;
	path->src.h = scl0->trim1_info.size_y;
	/* trim0 size */
	path->trim0_info.size_x = path->in_rect.w;
	path->trim0_info.size_x = path->trim0_info.size_x * scl0->dst.w /
		scl0->trim0_info.size_x;
	path->trim0_info.size_x = ISP_ALIGNTO(path->trim0_info.size_x);
	path->trim0_info.size_x = MIN(path->trim0_info.size_x, path->src.w);
	path->trim0_info.size_y = path->in_rect.h;
	path->trim0_info.size_y = path->trim0_info.size_y * scl0->dst.h /
		scl0->trim0_info.size_y;
	path->trim0_info.size_y = ISP_ALIGNTO(path->trim0_info.size_y);
	path->trim0_info.size_y = MIN(path->trim0_info.size_y, path->src.h);
	path->trim0_info.start_x = abs(path->src.w -
		path->trim0_info.size_x) / 2;
	path->trim0_info.start_x = path->trim0_info.start_x * scl0->dst.w /
		scl0->trim0_info.size_x;
	path->trim0_info.start_x = ISP_ALIGNTO(path->trim0_info.start_x);
	path->trim0_info.start_y = abs(path->src.h -
		path->trim0_info.size_y) / 2;
	path->trim0_info.start_y = path->trim0_info.start_y * scl0->dst.h /
		scl0->trim0_info.size_y;
	path->trim0_info.start_y = ISP_ALIGNTO(path->trim0_info.start_y);

	/* dst size */
	path->dst.w = path->out_size.w;
	path->dst.h = path->out_size.h;
	/* trim1 size */
	path->trim1_info.start_x = 0;
	path->trim1_info.start_y = 0;
	path->trim1_info.size_x = path->dst.w;
	path->trim1_info.size_y = path->dst.h;
	path->path_sel = 0;

	return 0;
}

static int isp_path_online_cowork(struct isp_path_desc *pre,
	struct isp_path_desc *vid, struct isp_path_desc *scl0)
{
	int rtn = 0;
	struct isp_path_desc *max_path = NULL;
	struct isp_path_desc *min_path = NULL;
	int scl0_max_width = ISP_SCL0_MAX_WIDTH;

	if (!pre || !vid || !scl0) {
		pr_err("fail to get valid param, NULL\n");
		return -EFAULT;
	}

	if (pre->in_rect.w > vid->in_rect.w) {
		max_path = pre;
		min_path = vid;
	} else {
		max_path = vid;
		min_path = pre;
	}

	/* scl0_max_width change here based on application scene
	 * eg: MAX & Default:with 2304; 3DNR with 720P; path cowork
	 * with 1080P; EIS on with 2304;
	 */

	if (scl0_max_width > max_path->in_size.w)
		scl0_max_width = max_path->in_size.w;

	scl0->src.w = max_path->in_size.w;
	scl0->src.h = max_path->in_size.h;
	scl0->dst.w = scl0_max_width;
	scl0->dst.h = max_path->out_size.h * scl0->dst.w /
		max_path->out_size.w;
	scl0->dst.h = ISP_ALIGNTO(scl0->dst.h);
	scl0->dst.h = MIN(scl0->dst.h, scl0->src.h);

	if (max_path->in_rect.w > ISP_SCL0_MAX_WIDTH) {
		scl0->trim0_info.start_x =
			max_path->in_rect.x > min_path->in_rect.x
			? min_path->in_rect.x : max_path->in_rect.x;
		scl0->trim0_info.start_y =
			max_path->in_rect.y > min_path->in_rect.y
			? min_path->in_rect.y : max_path->in_rect.y;
		scl0->trim0_info.size_x = scl0->src.w -
			scl0->trim0_info.start_x * 2;
		scl0->trim0_info.size_x =
			ISP_ALIGNTO(scl0->trim0_info.size_x);
		scl0->trim0_info.size_x = MIN(scl0->trim0_info.size_x,
			scl0->src.w);
		scl0->trim0_info.size_x = MAX(scl0->trim0_info.size_x,
			scl0->dst.w);
		scl0->trim0_info.size_y = scl0->src.h -
			scl0->trim0_info.start_y * 2;
		scl0->trim0_info.size_y =
			ISP_ALIGNTO(scl0->trim0_info.size_y);
		scl0->trim0_info.size_y = MIN(scl0->trim0_info.size_y,
			scl0->src.h);
		scl0->trim0_info.size_y = MAX(scl0->trim0_info.size_y,
			scl0->dst.h);
	} else {
		scl0->trim0_info.size_x = scl0_max_width;
		scl0->trim0_info.size_y = max_path->out_size.h *
			scl0_max_width / max_path->out_size.w;
		scl0->trim0_info.size_y =
			ISP_ALIGNTO(scl0->trim0_info.size_y);
		scl0->trim0_info.size_y = MIN(scl0->trim0_info.size_y,
			scl0->src.h);
		scl0->trim0_info.size_y = MAX(scl0->trim0_info.size_y,
			scl0->dst.h);
		scl0->trim0_info.start_x = abs(scl0->src.w -
			scl0->trim0_info.size_x) / 2;
		scl0->trim0_info.start_x =
			ISP_ALIGNTO(scl0->trim0_info.start_x);
		scl0->trim0_info.start_y = abs(scl0->src.h -
			scl0->trim0_info.size_y) / 2;
		scl0->trim0_info.start_y =
			ISP_ALIGNTO(scl0->trim0_info.start_y);
	}

	scl0->trim1_info.start_x = 0;
	scl0->trim1_info.start_y = 0;
	scl0->trim1_info.size_x = scl0->dst.w;
	scl0->trim1_info.size_y = scl0->dst.h;

	rtn = set_online_scl_size(max_path, scl0);
	if (rtn) {
		pr_err("fail to set max_path scl size\n");
		return rtn;
	}

	rtn = set_online_scl_size(min_path, scl0);
	if (rtn) {
		pr_err("fail to set min_path scl size\n");
		return rtn;
	}

	scl0->valid = 1;

	return rtn;
}
#endif

static int isp_path_offline(struct isp_path_desc *path,
			    struct isp_offline_desc *off_desc)
{
	int rtn = 0;

	if (!path || !off_desc) {
		pr_err("fail to get valid param, NULL\n");
		return -EFAULT;
	}

	off_desc->src.w = path->in_size.w;
	off_desc->src.h = path->in_size.h;
	off_desc->border.left_border = path->in_rect.x;
	off_desc->border.up_border = path->in_rect.y;
	off_desc->border.right_border = path->in_size.w - path->in_rect.w -
		off_desc->border.left_border;
	off_desc->border.down_border = path->in_size.h - path->in_rect.h -
		off_desc->border.up_border;
	off_desc->dst.w = path->in_rect.w;
	off_desc->dst.h = path->in_rect.h;
	path->src.w = path->in_size.w;
	path->src.h = path->in_size.h;
	path->trim0_info.start_x = path->in_rect.x;
	path->trim0_info.start_y = path->in_rect.y;
	path->trim0_info.size_x = path->in_rect.w;
	path->trim0_info.size_y = path->in_rect.h;
	path->dst.w = path->out_size.w;
	path->dst.h = path->out_size.h;
	path->trim1_info.start_x = 0;
	path->trim1_info.start_y = 0;
	path->trim1_info.size_x = path->dst.w;
	path->trim1_info.size_y = path->dst.h;
	path->path_sel = 1;
	off_desc->valid = 1;

	if (path->trim0_info.size_x > path->src.w ||
		path->trim0_info.size_y > path->src.h) {
		pr_err("fail to get valid size of trim0, %d %d src %d %d\n",
			path->trim0_info.size_x, path->trim0_info.size_y,
			path->src.w, path->src.h);
		return -EINVAL;
	}

	pr_info("path offline src %d %d dst %d %d trim0 %d %d %d %d trim1 %d %d %d %d\n",
		path->src.w, path->src.h,
		path->dst.w, path->dst.h,
		path->trim0_info.start_x,
		path->trim0_info.start_y,
		path->trim0_info.size_x,
		path->trim0_info.size_y,
		path->trim1_info.start_x,
		path->trim1_info.start_y,
		path->trim1_info.size_x,
		path->trim1_info.size_y);

	return rtn;
}

#if 0
static int isp_path_online(struct isp_path_desc *path,
	struct isp_path_desc *scl0)
{
	int rtn = 0;
	int scl0_max_width = ISP_SCL0_MAX_WIDTH;

	if (!path || !scl0) {
		pr_err("fail to get valid param, NULL");
		return -EFAULT;
	}

	/* scl0_max_width change here based on application scene
	 * eg: MAX & Default:with 2304; 3DNR with 720P; path cowork
	 * with 1080P; EIS on with 2304;
	 */

	if (scl0_max_width > path->in_size.w)
		scl0_max_width = path->in_size.w;

	scl0->src.w = path->in_size.w;
	scl0->src.h = path->in_size.h;
	scl0->dst.w = scl0_max_width;
	scl0->dst.h = path->out_size.h * scl0->dst.w /
		path->out_size.w;
	scl0->dst.h = ISP_ALIGNTO(scl0->dst.h);
	scl0->dst.h = MIN(scl0->dst.h, scl0->src.h);
	scl0->trim1_info.start_x = 0;
	scl0->trim1_info.start_y = 0;
	scl0->trim1_info.size_x = scl0->dst.w;
	scl0->trim1_info.size_y = scl0->dst.h;

	path->src.w = scl0->trim1_info.size_x;
	path->src.h = scl0->trim1_info.size_y;

	if (path->in_rect.w > scl0_max_width) {
		scl0->trim0_info.start_x = path->in_rect.x;
		scl0->trim0_info.start_y = path->in_rect.y;
		scl0->trim0_info.size_x = path->in_rect.w;
		scl0->trim0_info.size_y = path->in_rect.h;
		scl0->trim0_info.size_y = MAX(scl0->trim0_info.size_y,
			scl0->dst.h);
		path->trim0_info.start_x = 0;
		path->trim0_info.start_y = 0;
		path->trim0_info.size_x = path->src.w;
		path->trim0_info.size_y = path->src.h;
	} else {
		scl0->trim0_info.size_x = scl0_max_width;
		scl0->trim0_info.size_y = path->out_size.h *
			scl0_max_width / path->out_size.w;
		scl0->trim0_info.size_y =
			ISP_ALIGNTO(scl0->trim0_info.size_y);
		scl0->trim0_info.size_y = MIN(scl0->trim0_info.size_y,
			scl0->src.h);
		scl0->trim0_info.size_y = MAX(scl0->trim0_info.size_y,
			scl0->dst.h);
		scl0->trim0_info.start_x = abs(scl0->src.w -
			scl0->trim0_info.size_x) / 2;
		scl0->trim0_info.start_x =
			ISP_ALIGNTO(scl0->trim0_info.start_x);
		scl0->trim0_info.start_y = abs(scl0->src.h -
			scl0->trim0_info.size_y) / 2;
		scl0->trim0_info.start_y =
			ISP_ALIGNTO(scl0->trim0_info.start_y);
		path->trim0_info.size_x = path->in_rect.w;
		path->trim0_info.size_y = MIN(path->src.h,
			path->in_rect.h);
		path->trim0_info.start_x = abs(path->src.w -
			path->trim0_info.size_x) / 2;
		path->trim0_info.start_x =
			ISP_ALIGNTO(path->trim0_info.start_x);
		path->trim0_info.start_y = abs(path->src.h -
			path->trim0_info.size_y) / 2;
		path->trim0_info.start_y =
			ISP_ALIGNTO(path->trim0_info.start_y);
	}

	path->dst.w = path->out_size.w;
	path->dst.h = path->out_size.h;
	path->trim1_info.start_x = 0;
	path->trim1_info.start_y = 0;
	path->trim1_info.size_x = path->dst.w;
	path->trim1_info.size_y = path->dst.h;
	path->path_sel = 0;
	scl0->uv_sync_v = 1;
	scl0->valid = 1;

	return rtn;
}
#endif

static enum isp_store_format isp_store_format(enum dcam_fmt in_format)
{
	enum isp_store_format format = ISP_STORE_FORMAT_MAX;

	switch (in_format) {
	case DCAM_YUV422:
		format = ISP_STORE_YUV422_2FRAME;
		break;
	case DCAM_YUV420:
		format = ISP_STORE_YVU420_2FRAME;
		break;
	case DCAM_YVU420:
		format = ISP_STORE_YUV420_2FRAME;
		break;
	case DCAM_YUV420_3FRAME:
		format = ISP_STORE_YUV420_3FRAME;
		break;
	case DCAM_RAWRGB:
		format = ISP_STORE_RAW10;
		break;
	case DCAM_RGB888:
		format = ISP_STORE_FULL_RGB8;
		break;
	default:
		format = ISP_STORE_FORMAT_MAX;
		pr_info("error, format not support!");
		break;
	}
	return format;
}

static void get_store_pitch(struct slice_pitch *pitch_ptr,
			    enum isp_store_format format, uint32_t width)
{
	switch (format) {
	case ISP_STORE_YUV422_3FRAME:
	case ISP_STORE_YUV420_3FRAME:
		pitch_ptr->chn0 = width;
		pitch_ptr->chn1 = width >> 1;
		pitch_ptr->chn2 = width >> 1;
		break;
	case ISP_STORE_YUV422_2FRAME:
	case ISP_STORE_YVU422_2FRAME:
	case ISP_STORE_YUV420_2FRAME:
	case ISP_STORE_YVU420_2FRAME:
		pitch_ptr->chn0 = width;
		pitch_ptr->chn1 = width;
		break;
	case ISP_STORE_UYVY:
		pitch_ptr->chn0 = width << 1;
		break;
	case ISP_STORE_RAW10:
		pitch_ptr->chn0 = width << 1;
		break;
	case ISP_STORE_FULL_RGB8:
		pitch_ptr->chn0 = width * 4;
		break;
	default:
		break;
	}
}

static int isp_get_store_param(struct isp_path_desc *path)
{
	struct isp_store_info *store_info = NULL;

	if (!path) {
		pr_err("fail to get valid param, NULL\n");
		return -EFAULT;
	}

	store_info = &path->store_info;
	store_info->bypass = 0;
	store_info->endian = path->data_endian.uv_endian;
	store_info->speed_2x = 1;
	store_info->mirror_en = 0;
	if (path->input_format == ISP_FETCH_YVU420_2FRAME)
		store_info->color_format = isp_store_format(DCAM_YVU420);
	else
		store_info->color_format = isp_store_format(path->output_format);

	store_info->max_len_sel = 0;
	store_info->shadow_clr_sel = 1;
	store_info->shadow_clr = 1;
	store_info->store_res = 1;
	store_info->rd_ctrl = 0;

	store_info->size.w = path->trim1_info.size_x;
	store_info->size.h = path->trim1_info.size_y;

	store_info->border.up_border = 0;
	store_info->border.down_border = 0;
	store_info->border.left_border = 0;
	store_info->border.right_border = 0;

	get_store_pitch((void *)&store_info->pitch,
			store_info->color_format, store_info->size.w);

	return 0;
}

static int isp_path_store_cfg(struct isp_path_desc *pre,
			      struct isp_path_desc *vid,
			      struct isp_path_desc *cap,
			      struct isp_offline_desc *off_desc)
{
	int rtn = 0;

	if (!pre || !vid || !cap || !off_desc) {
		pr_err("fail to get valid param, NULL\n");
		return -EFAULT;
	}

	if (pre->valid) {
		rtn = isp_get_store_param(pre);
		if (rtn) {
			pr_err("fail to get pre store param\n");
			return rtn;
		}
	}

	if (vid->valid) {
		rtn = isp_get_store_param(vid);
		if (rtn) {
			pr_err("fail to get vid store param\n");
			return rtn;
		}
	}

	if (cap->valid) {
		struct isp_module *module = NULL;
		struct isp_pipe_dev *dev = NULL;

		module = container_of(cap,
				struct isp_module, isp_path[ISP_SCL_CAP]);
		dev = container_of(module, struct isp_pipe_dev, module_info);

		rtn = isp_get_store_param(cap);
		cap->store_info.shadow_clr_sel = 1;
		if (rtn) {
			pr_err("fail to get cap store param\n");
			return rtn;
		}

		if (dev->is_3dnr_path_cfg) {
			rtn = isp_get_store_param(vid);
			if (rtn) {
				pr_err("fail to get 3dnr store param\n");
				return rtn;
			}
			vid->store_info.shadow_clr_sel = 1;
		}
	}
#if 0
	if (off_desc->valid) {
		rtn = isp_get_storecce_param(off_desc);
		if (rtn) {
			pr_err("fail to get store cce param\n");
			return rtn;
		}
	}
#endif
	return 0;
}

int isp_start_pre_proc(struct isp_path_desc *pre,
		       struct isp_path_desc *vid,
		       struct isp_path_desc *cap,
		       struct isp_offline_desc *off_desc)
{
	int rtn = 0;

	if (!pre || !vid || !cap || !off_desc) {
		pr_err("fail to get valid param, NULL\n");
		return -EFAULT;
	}
	if (pre->valid) {
		rtn = isp_path_offline(pre, off_desc);
		if (rtn) {
			pr_err("fail to config Preview offline\n");
			return rtn;
		}
	}

	if (vid->valid) {
		rtn = isp_path_offline(vid, off_desc);
		if (rtn) {
			pr_err("fail to config Video offline\n");
			return rtn;
		}
	}

	if (cap->valid) {
		rtn = isp_path_offline(cap, off_desc);
		if (rtn) {
			pr_err("fail to config Cap offline\n");
			return rtn;
		}

		if (pre->valid && vid->valid &&
		    pre->path_mode == ISP_PRE_OFFLINE &&
		    vid->path_mode == ISP_VID_OFFLINE) {
			rtn = isp_path_offline_cowork_cap(pre, vid, cap,
							  off_desc);
			if (rtn) {
				pr_err("fail to config Offline cowork\n");
				return rtn;
			}
		}
	} else if (pre->valid && vid->valid &&
		   pre->path_mode == ISP_PRE_OFFLINE &&
		   vid->path_mode == ISP_VID_OFFLINE) {
		rtn = isp_path_offline_cowork(pre, vid, off_desc);

		if (rtn) {
			pr_err("fail to config Preview&Video offline cowork\n");
			return rtn;
		}
	}

	rtn = isp_path_store_cfg(pre, vid, cap, off_desc);
	if (rtn) {
		pr_err("fail to get store param\n");
		return rtn;
	}

	return rtn;
}

static int isp_set_store(uint32_t idx, void *input_info, uint32_t addr)
{
	uint32_t val = 0;
	struct isp_store_info *store_info = (struct isp_store_info *)input_info;

	ISP_REG_MWR(idx, addr + ISP_STORE_PARAM,
			BIT_0, store_info->bypass);
	if (store_info->bypass)
		return 0;

	ISP_REG_MWR(idx, addr + ISP_STORE_PARAM,
		BIT_1, (store_info->max_len_sel << 1));

	ISP_REG_MWR(idx, addr + ISP_STORE_PARAM,
		BIT_2, (store_info->speed_2x << 2));

	ISP_REG_MWR(idx, addr + ISP_STORE_PARAM,
		BIT_3, (store_info->mirror_en << 3));

	ISP_REG_MWR(idx, addr + ISP_STORE_PARAM,
		0xF0, (store_info->color_format << 4));

	ISP_REG_MWR(idx, addr + ISP_STORE_PARAM,
		0x300, (store_info->endian << 8));

	val = ((store_info->size.h & 0xFFFF) << 16) |
		   (store_info->size.w & 0xFFFF);
	ISP_REG_WR(idx, addr + ISP_STORE_SLICE_SIZE, val);

	val = ((store_info->border.right_border & 0xFF) << 24) |
		((store_info->border.left_border & 0xFF) << 16) |
		((store_info->border.down_border & 0xFF) << 8) |
		(store_info->border.up_border & 0xFF);
	ISP_REG_WR(idx, addr+ISP_STORE_BORDER, val);
	ISP_REG_MWR(idx, addr+ISP_STORE_Y_PITCH,
				0xFFFF, store_info->pitch.chn0);
	ISP_REG_MWR(idx, addr+ISP_STORE_U_PITCH,
				0xFFFF, store_info->pitch.chn1);
	ISP_REG_MWR(idx, addr+ISP_STORE_V_PITCH,
				0xFFFF, store_info->pitch.chn2);

	pr_debug("set_store size %d %d border %d %d %d %d\n",
		store_info->size.w, store_info->size.h,
		store_info->border.left_border,
		store_info->border.up_border,
		store_info->border.right_border,
		store_info->border.down_border);

	ISP_REG_MWR(idx, addr + ISP_STORE_READ_CTRL,
		0x3, store_info->rd_ctrl);
	ISP_REG_MWR(idx, addr + ISP_STORE_READ_CTRL,
		0xFFFFFFFC, store_info->store_res << 2);

	ISP_REG_MWR(idx, addr + ISP_STORE_SHADOW_CLR_SEL,
		BIT_1, store_info->shadow_clr_sel << 1);
#if 0
	ISP_REG_MWR(idx, addr + ISP_STORE_SHADOW_CLR,
		BIT_0, store_info->shadow_clr);

#endif
	return 0;
}

static void isp_sc_print_coeff(uint32_t idx,
			       unsigned long h_coeff_addr,
			       unsigned long v_chroma_coeff_addr,
			       unsigned long v_coeff_addr)
{
#ifdef SCALE_DRV_DEBUG
	int i = 0;
	int j = 0;
	unsigned long addr = 0;

	pr_info("SCAL: h_coeff_addr\n");
	for (i = 0; i < 16; i++) {
		pr_info("0x%lx: ", h_coeff_addr + 4 * (4 * i));
		for (j = 0; j < 4; j++)
			pr_info("0x%x ",
			ISP_HREG_RD(idx, h_coeff_addr + 4 * (4 * i + j)));
		pr_info("\n");
	}

	pr_info("SCAL: v_chroma_coeff_addr\n");
	for (i = 0; i < 32; i++) {
		pr_info("0x%lx: ", v_chroma_coeff_addr + 4 * (4 * i));
		for (j = 0; j < 4; j++)
			pr_info("0x%x ",
				ISP_HREG_RD(idx,
				       v_chroma_coeff_addr + 4 * (4 * i + j)));
		pr_info("\n");
	}

	pr_info("SCAL: v_coeff_addr\n");
	for (addr = v_coeff_addr; addr <= (v_coeff_addr + 0x6FC); addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_HREG_RD(idx, addr),
			ISP_HREG_RD(idx, addr + 4),
			ISP_HREG_RD(idx, addr + 8),
			ISP_HREG_RD(idx, addr + 12));
	}
#endif
}

int isp_set_sc_coeff_info(uint32_t idx, uint32_t addr, uint32_t *coeff_buf)
{
	int i = 0;
	int rtn = 0;
	uint32_t h_coeff_addr = 0;
	uint32_t v_coeff_addr = 0;
	uint32_t v_chroma_coeff_addr = 0;
	uint32_t *h_coeff = NULL;
	uint32_t *v_coeff = NULL;
	uint32_t *v_chroma_coeff = NULL;
	uint32_t reg_val = 0;

	if (coeff_buf == NULL) {
		pr_info("zero pointer!");
		return -1;
	}

	h_coeff = coeff_buf;
	v_coeff = coeff_buf + (ISP_SC_COEFF_COEF_SIZE / 4);
	v_chroma_coeff = v_coeff + (ISP_SC_COEFF_COEF_SIZE / 4);

	h_coeff_addr = addr + ISP_SCALER_LUMA_HCOEFF;
	v_coeff_addr = addr + ISP_SCALER_LUMA_VCOEFF;
	v_chroma_coeff_addr = addr + ISP_SCALER_CHROMA_VCOEFF;

	for (i = 0; i < ISP_SC_COEFF_H_NUM; i++) {
		ISP_REG_WR(idx, h_coeff_addr, *h_coeff);
		h_coeff_addr += 4;
		h_coeff++;
	}

	for (i = 0; i < ISP_SC_COEFF_V_NUM; i++) {
		ISP_REG_WR(idx, v_coeff_addr, *v_coeff);
		v_coeff_addr += 4;
		v_coeff++;
	}

	for (i = 0; i < ISP_SC_COEFF_V_CHROMA_NUM; i++) {
		ISP_REG_WR(idx, v_chroma_coeff_addr, *v_chroma_coeff);
		v_chroma_coeff_addr += 4;
		v_chroma_coeff++;
	}

	h_coeff_addr = addr + ISP_SCALER_LUMA_HCOEFF;
	v_coeff_addr = addr + ISP_SCALER_LUMA_VCOEFF;
	v_chroma_coeff_addr = addr + ISP_SCALER_CHROMA_VCOEFF;
	isp_sc_print_coeff(idx,
		h_coeff_addr, v_chroma_coeff_addr, v_coeff_addr);
	/* In AP mode, b29 = 0, after write coeff, b30 ^= 1 */
	if (ISP_GET_MID(idx) == ISP_AP_MODE) {
		reg_val = (ISP_HREG_RD(idx, addr + ISP_SCALER_CFG));
		ISP_REG_MWR(idx, addr + ISP_SCALER_CFG, BIT(30),
					(~reg_val) & BIT(30));
	}
	pr_debug("isp_set_sc_coeff_info\n");

	return rtn;
}


static void isp_set_shrink_info(void *input_info, uint32_t idx,
				uint32_t addr_base)
{
	unsigned long addr = 0;
	uint32_t reg_val = 0;
	struct isp_regular_info *regular_info = NULL;

	if (!input_info) {
		pr_err("fail to get valid param, NULL\n");
		return;
	}

	regular_info = (struct isp_regular_info *)input_info;
	addr = ISP_SCALER_CFG + addr_base;
	ISP_REG_MWR(idx, addr, (BIT_25|BIT_26),
		regular_info->regular_mode << 25);

	addr = ISP_SCALER_SHRINK_CFG + addr_base;
	reg_val = ((regular_info->shrink_uv_dn_th & 0xFF) << 24) |
		((regular_info->shrink_uv_up_th & 0xFF) << 16);
	reg_val |= ((regular_info->shrink_y_dn_th  & 0xFF) << 8) |
		((regular_info->shrink_y_up_th & 0xFF));
	ISP_REG_WR(idx, addr, reg_val);

	addr = ISP_SCALER_EFFECT_CFG + addr_base;
	reg_val = ((regular_info->effect_v_th & 0xFF) << 16) |
		((regular_info->effect_u_th & 0xFF) << 8);
	reg_val |= (regular_info->effect_y_th & 0xFF);
	ISP_REG_WR(idx, addr, reg_val);

	addr = ISP_SCALER_REGULAR_CFG + addr_base;
	reg_val = ((regular_info->shrink_c_range & 0xF) << 24) |
		((regular_info->shrink_c_offset & 0x1F) << 16);
	reg_val |= ((regular_info->shrink_y_range & 0xF) << 8) |
		(regular_info->shrink_y_offset & 0x1F);
	ISP_REG_WR(idx, addr, reg_val);
}

static void isp_set_scaler_info(void *input_info, uint32_t idx,
				uint32_t addr_base)
{
	uint32_t reg_val;

	struct isp_scaler_info *scalerInfo =
		 (struct isp_scaler_info *)input_info;

	ISP_REG_MWR(idx, addr_base+ISP_SCALER_CFG, BIT_20,
			scalerInfo->scaler_bypass << 20);
	ISP_REG_MWR(idx, addr_base+ISP_SCALER_CFG, 0xF0000,
			scalerInfo->scaler_y_ver_tap << 16);
	ISP_REG_MWR(idx, addr_base+ISP_SCALER_CFG, 0xF800,
			scalerInfo->scaler_uv_ver_tap << 11);

	reg_val = ((scalerInfo->scaler_ip_int & 0xF) << 16) |
			(scalerInfo->scaler_ip_rmd & 0x3FFF);
	ISP_REG_MWR(idx, addr_base+ISP_SCALER_IP, 0xF3FFF, reg_val);
	reg_val = ((scalerInfo->scaler_cip_int & 0xF) << 16) |
			(scalerInfo->scaler_cip_rmd & 0x3FFF);
	ISP_REG_MWR(idx, addr_base+ISP_SCALER_CIP, 0xF3FFF, reg_val);
	reg_val = ((scalerInfo->scaler_factor_in & 0x3FFF) << 16) |
			(scalerInfo->scaler_factor_out & 0x3FFF);
	ISP_REG_MWR(idx, addr_base+ISP_SCALER_FACTOR, 0x3FFF3FFF, reg_val);

	reg_val = ((scalerInfo->scaler_ver_ip_int & 0xF) << 16) |
			 (scalerInfo->scaler_ver_ip_rmd & 0x3FFF);
	ISP_REG_MWR(idx, addr_base+ISP_SCALER_VER_IP, 0xF3FFF, reg_val);
	reg_val = ((scalerInfo->scaler_ver_cip_int & 0xF) << 16) |
			 (scalerInfo->scaler_ver_cip_rmd & 0x3FFF);
	ISP_REG_MWR(idx, addr_base+ISP_SCALER_VER_CIP, 0xF3FFF, reg_val);
	reg_val = ((scalerInfo->scaler_ver_factor_in & 0x3FFF) << 16) |
			 (scalerInfo->scaler_ver_factor_out & 0x3FFF);
	ISP_REG_MWR(idx, addr_base+ISP_SCALER_VER_FACTOR, 0x3FFF3FFF, reg_val);

	pr_debug("set_scale_info in %d %d out %d %d\n",
		scalerInfo->scaler_factor_in,
		scalerInfo->scaler_ver_factor_in,
		scalerInfo->scaler_factor_out,
		scalerInfo->scaler_ver_factor_out);

	isp_set_sc_coeff_info(idx, addr_base, scalerInfo->coeff_buf);

}

static void isp_set_deci_info(void *input_info, uint32_t idx, uint32_t addr_base)
{
	struct isp_deci_info *deciInfo = (struct isp_deci_info *)input_info;

	ISP_REG_MWR(idx, addr_base+ISP_SCALER_CFG, BIT_2,
		deciInfo->deci_x_eb << 2);
	ISP_REG_MWR(idx, addr_base+ISP_SCALER_CFG, (BIT_0 | BIT_1),
		deciInfo->deci_x);
	ISP_REG_MWR(idx, addr_base+ISP_SCALER_CFG, BIT_5,
		deciInfo->deci_y_eb << 5);
	ISP_REG_MWR(idx, addr_base+ISP_SCALER_CFG, (BIT_3 | BIT_4),
		deciInfo->deci_y << 3);
}

static int isp_set_fetch(uint32_t idx, struct isp_path_desc *path)
{
	uint32_t start_col	 = 0;
	uint32_t end_col = 0;
	uint32_t mipi_byte_rel_pos = 0;
	uint32_t mipi_word_num = 0;
	uint32_t mipi_word_num_start[16] = {0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3,
						4, 4, 4, 5, 5};
	uint32_t mipi_word_num_end[16] = {0, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4,
						4, 4, 5, 5, 5};
	uint32_t val = 0;
	uint32_t width = path->in_size.w;
	uint32_t height = path->in_size.h;
	uint32_t pitch = 0;
	enum isp_fetch_format clr_fmt = path->input_format;

	switch (clr_fmt) {
	case ISP_FETCH_CSI2_RAW_10:
		pitch = ISP_ALIGN_4BYTE(((width * 5) >> 2));
		end_col	= start_col + width - 1;
		mipi_byte_rel_pos = start_col & 0x0f;
		mipi_word_num = ((((end_col + 1) >> 4) * 5 +
				mipi_word_num_end[(end_col + 1) & 0x0f]) -
				(((start_col + 1) >> 4) * 5 +
				mipi_word_num_start[(start_col + 1) & 0x0f])+1);
		pr_debug("pitch 0x%x, mipi_word_num 0x%x\n", pitch,
			mipi_word_num);
		break;

	case ISP_FETCH_YUV420_2FRAME:
	case ISP_FETCH_YVU420_2FRAME:
		pitch = width;
		ISP_HREG_MWR(idx, ISP_COMMON_SPACE_SEL, 0x3, 2);
		break;
	default:
		pr_err("fail to get color fmt %d path id 0x%x\n", clr_fmt, idx);
		return -1;
	}

	ISP_REG_MWR(idx, ISP_FETCH_PARAM0, 0xf0, (clr_fmt << 4));

	val = (((height & 0xFFFF) << 16) | (width & 0xFFFF));
	pr_debug("idx %d, fetch size 0x%x\n", idx, val);
	ISP_REG_WR(idx, ISP_FETCH_MEM_SLICE_SIZE, val);
	ISP_REG_WR(idx, ISP_DISPATCH_CH0_SIZE, val); /* Same as fetch */
	ISP_REG_WR(idx, ISP_FETCH_Y_PITCH, pitch);
	ISP_REG_WR(idx, ISP_FETCH_U_PITCH, pitch);

	val = ((mipi_byte_rel_pos & 0xF) << 16) | (mipi_word_num & 0xFFFF);
	ISP_REG_WR(idx, ISP_FETCH_MIPI_PARAM, val);

	/* overwrite this dispatch parameter using default value */
	ISP_REG_WR(idx, ISP_DISPATCH_HW_CTRL_CH0, 0x80004);

	ISP_HREG_WR(idx, ISP_AXI_ISOLATION, 0);
	ISP_REG_WR(idx, ISP_YDELAY_STEP, 0x144);

	return 0;
}

static bool check_ufid(struct camera_frame *frame, void *data)
{
	uint32_t target_fid;

	if (!frame || !data)
		return false;

	target_fid = *(uint32_t *)data;

	return (frame->user_fid == CAMERA_RESERVE_FRAME_NUM ||
		frame->user_fid == target_fid);
}

static int get_ufid_across_all_path(struct isp_pipe_dev *dev)
{
	int ret = 0;
	struct isp_path_desc *path = NULL;
	struct camera_frame *frame = NULL;
	uint32_t target_fid = CAMERA_RESERVE_FRAME_NUM;
	uint32_t path_id = 0;
	struct isp_module *module = NULL;

	if (!dev) {
		pr_err("fail to check isp dev ptr\n");
		return -EINVAL;
	}

	module = &dev->module_info;
	frame = kzalloc(sizeof(struct camera_frame), GFP_KERNEL);
	if (!frame)
		return -ENOMEM;

	for (path_id = ISP_SCL_PRE; path_id < ISP_SCL_MAX; path_id++) {
		path = &module->isp_path[path_id];
		if (!path->uframe_sync ||
		    isp_buf_queue_peek(&path->buf_queue, frame))
			continue;
		else {
			target_fid = min(target_fid, frame->user_fid);
			pr_debug("path%d user_fid %u\n",
				path_id, frame->user_fid);
			ret = target_fid;
		}
	}

	kfree(frame);
	pr_debug("target_fid %u\n", target_fid);
	return ret;
}

void isp_path_set_scl(uint32_t idx, struct isp_path_desc *path, uint32_t addr)
{
	uint32_t reg_val = 0;
	uint32_t frm_deci;

	if (!path) {
		pr_err("fail to get valid param, NULL\n");
		return;
	}

	/*CFG path_eb*/
	ISP_REG_MWR(idx, addr+ISP_SCALER_CFG, BIT_31, 1 << 31);
	ISP_REG_MWR(idx, addr+ISP_SCALER_CFG, BIT_9, ~path->valid << 9);

	/*CFG output format*/
	if (path->store_info.color_format == ISP_STORE_UYVY)
		path->uv_sync_v = 1;
	else
		path->uv_sync_v = 0;

	ISP_REG_MWR(idx, addr+ISP_SCALER_CFG, BIT_10, path->uv_sync_v << 10);

	/*CFG frame deci*/
	frm_deci = path->frm_deci;
	if ((int)frm_deci >= 0 && frm_deci <= 3) {
		pr_debug("path frame_deci: %d\n", frm_deci);
		ISP_REG_MWR(idx, addr+ISP_SCALER_CFG, (BIT_23 | BIT_24),
			    frm_deci << 23);
	} else
		pr_info("invalid frame_deci %d\n", frm_deci);

	/*CFG output format*/
	if (path->output_format == DCAM_YUV422)
		path->odata_mode = 0x00;
	else if (path->output_format == DCAM_YUV420 ||
		path->output_format == DCAM_YUV420_3FRAME)
		path->odata_mode = 0x01;
	else
		pr_info("invalid output format %d\n", path->output_format);

	ISP_REG_MWR(idx, addr+ISP_SCALER_CFG, BIT_6 | BIT_7,
			path->odata_mode << 6);

	ISP_REG_MWR(idx, addr+ISP_SCALER_CFG, BIT(29), 0);
	ISP_REG_MWR(idx, addr+ISP_SCALER_DEBUG, BIT_0, 1);
	ISP_REG_MWR(idx, addr+ISP_SCALER_HBLANK, 0xFF, 0x40);
	ISP_REG_MWR(idx, addr+ISP_SCALER_HBLANK, 0xFF00, 0x40 << 8);
	ISP_REG_MWR(idx, addr+ISP_SCALER_FRAME_CNT_CLR, BIT_0, 0);

	reg_val = (0x0 << 8) | (0xFF);
	ISP_REG_WR(idx, addr+ISP_SCALER_RES, reg_val);

	/*CFG deci */
	isp_set_deci_info((void *)&path->deci_info, idx, addr);

	/*src size*/
	reg_val = ((path->src.h & 0x3FFF) << 16) | (path->src.w & 0x3FFF);
	ISP_REG_WR(idx, addr+ISP_SCALER_SRC_SIZE, reg_val);
	pr_debug("scl input size w %d, h %d\n", path->src.w, path->src.h);

	/* trim0 */
	reg_val = ((path->trim0_info.start_y & 0x3FFF) << 16) |
			(path->trim0_info.start_x & 0x3FFF);
	ISP_REG_WR(idx, addr+ISP_SCALER_TRIM0_START, reg_val);
	/* trim0 size must <= src_size */
	if (path->trim0_info.size_x > path->src.w ||
	    path->trim0_info.size_y > path->src.h) {
		pr_warn("trim0 size > src size, used src size\n");
		path->trim0_info.size_x = path->src.w;
		path->trim0_info.size_y = path->src.h;
	}
	reg_val = ((path->trim0_info.size_y & 0x3FFF) << 16) |
			(path->trim0_info.size_x & 0x3FFF);
	ISP_REG_WR(idx, addr+ISP_SCALER_TRIM0_SIZE, reg_val);

	/* trim1 */
	reg_val = ((path->trim1_info.start_y & 0x3FFF) << 16) |
			(path->trim1_info.start_x & 0x3FFF);
	ISP_REG_WR(idx, addr+ISP_SCALER_TRIM1_START, reg_val);
	reg_val = ((path->trim1_info.size_y & 0x3FFF) << 16) |
			(path->trim1_info.size_x & 0x3FFF);
	ISP_REG_WR(idx, addr+ISP_SCALER_TRIM1_SIZE, reg_val);

	/* des size */
	reg_val = ((path->dst.h & 0x3FFF) << 16) | (path->dst.w & 0x3FFF);
	ISP_REG_WR(idx, addr+ISP_SCALER_DES_SIZE, reg_val);

	pr_debug("scl base addr %x\n", addr);
	pr_debug("set_scl src %d %d dst %d %d t0 %d %d %d %d t1 %d %d %d %d\n",
		path->src.w, path->src.h,
		path->dst.w, path->dst.h,
		path->trim0_info.start_x,
		path->trim0_info.start_y,
		path->trim0_info.size_x,
		path->trim0_info.size_y,
		path->trim1_info.start_x,
		path->trim1_info.start_y,
		path->trim1_info.size_x,
		path->trim1_info.size_y);
	/* scaler info*/
	isp_set_scaler_info((void *)&path->scaler_info, idx, addr);

	/* scaler_vid shrink */
	if (addr == ISP_SCALER_VID_BASE)
		isp_set_shrink_info((void *)&path->regular_info, idx, addr);
}

void isp_path_set(struct isp_module *module,
		  struct isp_path_desc *path,
		  enum isp_path_index path_index)
{
	uint32_t scl_addr = 0, store_addr = 0;
	uint32_t idx = 0;
	enum isp_scene_id sid = ISP_SCENE_PRE;
	struct isp_pipe_dev *dev = NULL;

	if (!module || !path) {
		pr_err("fail to get valid param, NULL\n");
		return;
	}

	dev = container_of(module, struct isp_pipe_dev, module_info);
	if (ISP_PATH_IDX_PRE & path_index) {
		sid = ISP_SCENE_PRE;
		scl_addr = ISP_SCALER_PRE_CAP_BASE;
		store_addr = ISP_STORE_PRE_CAP_BASE;
	} else if (ISP_PATH_IDX_VID & path_index) {
		if (dev->is_3dnr)
			sid = ISP_SCENE_CAP;
		else
			sid = ISP_SCENE_PRE;
		scl_addr = ISP_SCALER_VID_BASE;
		store_addr = ISP_STORE_VID_BASE;
	} else if (ISP_PATH_IDX_CAP & path_index) {
		sid = ISP_SCENE_CAP;
		scl_addr = ISP_SCALER_PRE_CAP_BASE;
		store_addr = ISP_STORE_PRE_CAP_BASE;
	} else {
		pr_err("fail to get valid path index\n");
		return;
	}

	idx = dev->com_idx;
	ISP_SET_SID(idx, sid);
	pr_debug("path index %d, idx 0x%x\n", path_index, idx);
	isp_path_set_scl(idx, path, scl_addr);
	isp_set_fetch(idx, path);
	isp_set_store(idx, (void *)&path->store_info, store_addr);
}

int isp_get_scl_index(uint32_t channel_id)
{
	enum isp_scl_id  path_index;

	switch (channel_id) {
	case ISP_PATH_IDX_PRE:
		path_index = ISP_SCL_PRE;
		break;
	case ISP_PATH_IDX_VID:
		path_index = ISP_SCL_VID;
		break;
	case ISP_PATH_IDX_CAP:
		path_index = ISP_SCL_CAP;
		break;
	default:
		path_index = ISP_SCL_MAX;
		pr_info("fail to get path index, channel %d\n", channel_id);
	}
	return path_index;
}

int isp_path_set_next_frm(struct isp_module *module,
			  enum isp_path_index path_index,
			  struct slice_addr *addr)
{
	enum isp_drv_rtn rtn = ISP_RTN_PATH_ADDR_ERR;
	struct camera_frame *reserved_frame = NULL;
	struct camera_frame frame;
	struct isp_path_desc *path = NULL;
	struct isp_frm_queue *p_heap = NULL;
	struct isp_buf_queue *p_buf_queue = NULL;
	unsigned long yuv_reg[3] = {0};
	uint32_t yuv_addr[3] = {0};
	int use_reserve_frame = 0;
	enum isp_id iid = ISP_ID_0;
	enum isp_scene_id sid = ISP_SCENE_PRE;
	uint32_t idx = 0;
	struct isp_pipe_dev *dev = NULL;
	uint32_t iova0, iova2;
	uint32_t frm_q_len;
	uint32_t target_fid = 0;
	uint32_t ret = 0;

	if (module == NULL) {
		pr_err("fail to get moudule, It's NULL\n");
		return -EFAULT;
	}

	dev = container_of(module, struct isp_pipe_dev, module_info);

#define _ISP_SET_STORE_ADDR_REG(base, reg)	\
	do {	\
		reg[0] = base+ISP_STORE_SLICE_Y_ADDR;	\
		reg[1] = base+ISP_STORE_SLICE_U_ADDR;	\
		reg[2] = base+ISP_STORE_SLICE_V_ADDR;	\
	} while (0)

	if (path_index == ISP_PATH_IDX_PRE) {
		sid = ISP_SCENE_PRE;
		reserved_frame = &module->path_reserved_frame[ISP_SCL_PRE];
		path = &module->isp_path[ISP_SCL_PRE];
		_ISP_SET_STORE_ADDR_REG(ISP_STORE_PRE_CAP_BASE, yuv_reg);
		p_heap = &path->frame_queue;
		p_buf_queue = &path->buf_queue;
	} else if (path_index == ISP_PATH_IDX_VID) {
		sid = ISP_SCENE_PRE;
		reserved_frame = &module->path_reserved_frame[ISP_SCL_VID];
		path = &module->isp_path[ISP_SCL_VID];
		_ISP_SET_STORE_ADDR_REG(ISP_STORE_VID_BASE, yuv_reg);
		p_heap = &path->frame_queue;
		p_buf_queue = &path->buf_queue;
	} else if (path_index == ISP_PATH_IDX_CAP) {
		sid = ISP_SCENE_CAP;
		reserved_frame = &module->path_reserved_frame[ISP_SCL_CAP];
		path = &module->isp_path[ISP_SCL_CAP];
		_ISP_SET_STORE_ADDR_REG(ISP_STORE_PRE_CAP_BASE, yuv_reg);
		p_heap = &path->frame_queue;
		p_buf_queue = &path->buf_queue;
	} else {
		pr_err("fail to get valid path index 0x%x\n", path_index);
		return -EFAULT;
	}

	idx = dev->com_idx;
	ISP_SET_SID(idx, sid);
	iid = ISP_GET_IID(idx);
	pr_debug("iid%d path index %d, idx 0x%x, uframe_sync %d\n",
		iid, path_index, idx, path->uframe_sync);

#if 1 /* TODO: this is incorrect, need to change it */
	if (get_off_frm_q_len(&module->off_desc, &frm_q_len))
		return -EFAULT;

	if (p_heap->valid_cnt >= frm_q_len) {
		rtn = ISP_RTN_PATH_ADDR_ERR;
		pr_err("fail to write frame_queue, queue will overflow, idx:0x%x\n",
		       idx);
		goto overflow;
	}
#endif

	memset(&frame, 0, sizeof(frame));

	if (path->uframe_sync) {
		target_fid = get_ufid_across_all_path(dev);
		pr_debug("path 0x%x, target frame id %u\n", path_index,
			 target_fid);
		rtn = isp_buf_queue_read_if(&path->buf_queue, check_ufid,
					    (void *)&target_fid, &frame);
		if(!rtn)
			path->output_frame_count--;
		pr_debug("path 0x%x, get target frame id %u, rtn %d\n",
			path_index, frame.user_fid, rtn);
	} else {
		if (isp_buf_queue_read(p_buf_queue, &frame) == 0 &&
		    frame.pfinfo.mfd[0] != 0)
			path->output_frame_count--;
		else
			use_reserve_frame = 1;
	}


	if ((path->uframe_sync &&
	     (target_fid == CAMERA_RESERVE_FRAME_NUM || rtn != 0)) ||
	    (use_reserve_frame == 0 &&
	     frame.pfinfo.mfd[0] == reserved_frame->pfinfo.mfd[0]))
		use_reserve_frame = 1;

	if (use_reserve_frame) {
		pr_debug("buf queue have %d, mfd %u\n",
			 p_buf_queue->valid_cnt, frame.pfinfo.mfd[0]);
		rtn = pfiommu_get_addr(&reserved_frame->pfinfo);
		if (rtn) {
			pr_err("ISP%d: fail to get reserved buffer addr\n",
			       iid);
			rtn = ISP_RTN_PATH_ADDR_ERR;
			goto iommu_err;
		}
		pr_debug("ISP%d: No freed frame id %d path_index %d\n",
			 iid, frame.fid, path_index);
		if (reserved_frame->pfinfo.mfd[0] == 0) {
			pr_err("fail to cfg reserve buf, ISP%d\n", iid);
			return -EFAULT;
		}
		memcpy(&frame, reserved_frame, sizeof(struct camera_frame));
	} else {
		if (frame.pfinfo.dev == NULL)
			pr_info("ISP%d next dev NULL %p\n", iid,
				frame.pfinfo.dev);
		if (pfiommu_get_addr(&frame.pfinfo)) {
			pr_err("ISP%d: fail to get frame iova\n", iid);
			rtn = ISP_RTN_PATH_ADDR_ERR;
			goto iommu_err;
		}
	}

	frame.uaddr = path->out_size.w * path->out_size.h;
	yuv_addr[0] = frame.pfinfo.iova[0] + frame.yaddr;
	yuv_addr[1] = frame.pfinfo.iova[0] + frame.uaddr;
	yuv_addr[2] = frame.pfinfo.iova[2];

	if (isp_frame_enqueue(p_heap, &frame) == 0)
		pr_debug("success to enq frame buf, idx 0x%x, fid:%d, valid_cnt:%d\n",
			 idx, frame.fid, p_heap->valid_cnt);
	else {
		rtn = ISP_RTN_PATH_FRAME_LOCKED;
		pr_err("fail to enq frame buf, idx 0x%x\n", idx);
		goto queue_err;
	}

	ISP_REG_WR(idx, yuv_reg[0], yuv_addr[0]);
	if (path->output_format < DCAM_YUV400) {
		ISP_REG_WR(idx, yuv_reg[1], yuv_addr[1]);
		if (path->output_format == DCAM_YUV420_3FRAME)
			ISP_REG_WR(idx, yuv_reg[2], yuv_addr[2]);
	}

	if (addr != NULL) {
		addr->chn0 = yuv_addr[0];
		addr->chn1 = yuv_addr[1];
		addr->chn2 = yuv_addr[2];
	}

	if ((path_index == ISP_PATH_IDX_CAP) && dev->is_3dnr) {
		struct isp_path_desc *path_vid = &module->isp_path[ISP_SCL_VID];

		pr_debug("cap out w %d h %d, 3dnr out w %d h %d\n",
			path->out_size.w, path->out_size.h,
			path_vid->out_size.w, path_vid->out_size.h);

		_ISP_SET_STORE_ADDR_REG(ISP_STORE_VID_BASE, yuv_reg);
		iova0 = frame.pfinfo.iova[0] +
			path->out_size.w *
			path->out_size.h * 3/2;
		iova2 = frame.pfinfo.iova[2] +
			path->out_size.w *
			path->out_size.h * 3/2;

		frame.uaddr = path_vid->out_size.w * path_vid->out_size.h;
		yuv_addr[0] = iova0 + frame.yaddr;
		yuv_addr[1] = iova0 + frame.uaddr;
		yuv_addr[2] = frame.pfinfo.iova[2];
		ISP_REG_WR(idx, yuv_reg[0], yuv_addr[0]);
		if (path->output_format < DCAM_YUV400) {
			ISP_REG_WR(idx, yuv_reg[1], yuv_addr[1]);
			if (path->output_format == DCAM_YUV420_3FRAME)
				ISP_REG_WR(idx, yuv_reg[2], yuv_addr[2]);
		}
	}
#undef _ISP_SET_STORE_ADDR_REG

	return 0;

queue_err:
	if (use_reserve_frame)
		return -rtn;
	if (path_index == ISP_PATH_IDX_PRE) {
		ret = pfiommu_free_addr_with_id(&frame.pfinfo, ISP_IOMMU_CH_AW,
					  AW_ID_STORE_PRE_CAP_YUV);
	} else if (path_index == ISP_PATH_IDX_VID) {
		ret = pfiommu_free_addr_with_id(&frame.pfinfo, ISP_IOMMU_CH_AW,
					  AW_ID_STORE_VID_YUV);
	} else if (path_index == ISP_PATH_IDX_CAP) {
		ret = pfiommu_free_addr_with_id(&frame.pfinfo, ISP_IOMMU_CH_AW,
					  AW_ID_STORE_PRE_CAP_YUV);
	}
	if (ret)
		pr_err("fail to free frame, isp path 0x%x\n", path_index);

	memset(frame.pfinfo.iova, 0, sizeof(frame.pfinfo.iova));

iommu_err:
	if (!use_reserve_frame) {
		frame.pfinfo.offset[0] = 0;
		frame.pfinfo.offset[1] = 0;
		frame.pfinfo.offset[2] = 0;
		if (!isp_buf_queue_write(p_buf_queue, &frame))
			path->output_frame_count++;
	}

overflow:
	return -rtn;
}
