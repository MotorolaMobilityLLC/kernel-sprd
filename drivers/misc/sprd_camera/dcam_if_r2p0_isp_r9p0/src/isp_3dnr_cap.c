/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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

#include <video/sprd_mm.h>

#include "isp_3dnr_cap.h"
#include "isp_drv.h"
#include "isp_3dnr_drv.h"
#include "isp_slice.h"

static void sprd_isp3dnr_memctrl_cfg(struct isp_mem_ctrl *mem_ctrl,
	void *data)
{
	struct camera_frame *frame = NULL;

	if (!mem_ctrl || !data) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	frame = (struct camera_frame *)data;

	mem_ctrl->bypass = 0;
	mem_ctrl->mv_x = frame->mv.mv_x;
	mem_ctrl->mv_y = frame->mv.mv_y;
	mem_ctrl->ref_pic_flag = 0;
	mem_ctrl->nr3_done_mode = 0;
	mem_ctrl->back_toddr_en = 1;
	mem_ctrl->chk_sum_clr_en = 1;
	mem_ctrl->data_toyuv_en = 1;
	mem_ctrl->roi_mode = 0;
	mem_ctrl->retain_num = 0;
	mem_ctrl->ft_max_len_sel = 1;
	mem_ctrl->start_row = 0;
	mem_ctrl->start_col = 0;
	mem_ctrl->global_img_width = frame->width;
	mem_ctrl->global_img_height = frame->height;
	mem_ctrl->img_width = frame->width;
	mem_ctrl->img_height = frame->height;
	mem_ctrl->ft_y_width = frame->width;
	mem_ctrl->ft_y_height = frame->height;
	mem_ctrl->ft_uv_width = frame->width;
	mem_ctrl->ft_uv_height = frame->height / 2;
	mem_ctrl->ft_pitch = frame->width;
	mem_ctrl->first_line_mode = 0;
	mem_ctrl->last_line_mode = 0;
	/*configuration param 8~11*/
	mem_ctrl->blend_y_en_start_row = 0;
	mem_ctrl->blend_y_en_start_col = 0;
	mem_ctrl->blend_y_en_end_row = frame->height - 1;
	mem_ctrl->blend_y_en_end_col = frame->width - 1;
	mem_ctrl->blend_uv_en_start_row = 0;
	mem_ctrl->blend_uv_en_start_col = 0;
	mem_ctrl->blend_uv_en_end_row = frame->height / 2 - 1;
	mem_ctrl->blend_uv_en_end_col = frame->width - 1;
	/*configuration param 12*/
	mem_ctrl->ft_hblank_num = 32;
	mem_ctrl->pipe_hblank_num = 60;
	mem_ctrl->pipe_flush_line_num = 17;
	/*configuration param 13*/
	mem_ctrl->pipe_nfull_num = 100;
	mem_ctrl->ft_fifo_nfull_num = 2648;
}

static void sprd_isp3dnr_store_cfg(struct isp_3dnr_store *store, void *data)
{
	struct camera_frame *frame = NULL;

	if (!store || !data) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	frame = (struct camera_frame *)data;

	store->st_bypass = 0;
	store->st_pitch = frame->width;
	store->img_width = frame->width;
	store->img_height = frame->height;
	store->chk_sum_clr_en = 1;
	store->shadow_clr_sel = 1;
	store->st_max_len_sel = 1;
	store->shadow_clr = 1;
}

static void sprd_isp3dnr_crop_cfg(struct isp_3dnr_crop *crop, void *data)
{
	struct camera_frame *frame = NULL;

	if (!crop || !data) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	frame = (struct camera_frame *)data;

	crop->crop_bypass = 0;
	crop->src_width = frame->width;
	crop->src_height = frame->height;
	crop->dst_width = frame->width;
	crop->dst_height = frame->height;
	crop->start_x = 0;
	crop->start_y = 0;
}

int sprd_isp_3dnr_cap_frame_proc(void *isp_handle, void *data)
{
	enum isp_drv_rtn ret = ISP_RTN_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct camera_frame *frame = NULL;
	struct isp_nr3_param *nr3_info = NULL;
	struct isp_module *module = NULL;
	struct isp_path_desc *path_cap = NULL;

	if (!isp_handle || !data) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	frame = (struct camera_frame *)data;
	module = &dev->module_info;
	path_cap = &module->isp_path[ISP_SCL_CAP];
	nr3_info = &path_cap->nr3_param;

	nr3_info->fetch_format = ISP_FETCH_YUV420_2FRAME;
	nr3_info->store_format = ISP_STORE_YUV420_2FRAME;

	if (nr3_info->need_3dnr) {
		sprd_isp3dnr_memctrl_cfg(&nr3_info->mem_ctrl, data);
		sprd_isp3dnr_store_cfg(&nr3_info->nr3_store, data);
		sprd_isp3dnr_crop_cfg(&nr3_info->crop, data);
		isp_3dnr_config_param(module->com_idx,
			ISP_OPERATE_CAP, nr3_info);

		if (nr3_info->blending_cnt < ISP_3DNR_BLEND_NUM)
			nr3_info->blending_cnt++;
		else
			nr3_info->blending_cnt = 0;
	} else {
		isp_3dnr_default_param(module->com_idx);
	}

	return ret;
}

int sprd_isp_3dnr_blending_calc(struct nr3_slice *in,
	struct nr3_slice_for_blending *out)
{
	uint32_t end_row = 0, end_col = 0, ft_pitch = 0;
	int mv_x = 0, mv_y = 0;
	uint32_t global_img_width = 0, global_img_height = 0;

	if (!in || !out) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	end_row = in->end_row;
	end_col = in->end_col;
	ft_pitch = in->cur_frame_width;
	mv_x = in->mv_x;
	mv_y = in->mv_y;
	global_img_width = in->cur_frame_width;
	global_img_height = in->cur_frame_height;

	if (in->slice_num == 1) {
		if (mv_x < 0) {
			if ((mv_x) & 0x1) {
				out->ft_y_width = global_img_width + mv_x + 1;
				out->ft_uv_width = global_img_width + mv_x - 1;
				out->src_chr_addr += 2;
			} else {
				out->ft_y_width = global_img_width + mv_x;
				out->ft_uv_width = global_img_width + mv_x;
			}
		} else if (mv_x > 0) {
			if ((mv_x) & 0x1) {
				out->ft_y_width = global_img_width - mv_x + 1;
				out->ft_uv_width = global_img_width - mv_x + 1;
				out->src_lum_addr += mv_x;
				out->src_chr_addr += mv_x - 1;
			} else {
				out->ft_y_width = global_img_width - mv_x;
				out->ft_uv_width = global_img_width - mv_x;
				out->src_lum_addr += mv_x;
				out->src_chr_addr += mv_x;
			}
		}
		if (mv_y < 0) {
			if ((mv_y) & 0x1) {
				out->last_line_mode = 0;
				out->ft_uv_height =
					global_img_height / 2 + mv_y / 2;
			} else {
				out->last_line_mode = 1;
				out->ft_uv_height =
					global_img_height / 2 + mv_y / 2 + 1;
			}
			out->first_line_mode = 0;
			out->ft_y_height = global_img_height + mv_y;
		} else if (mv_y > 0) {
			if ((mv_y) & 0x1) {
				out->first_line_mode = 1;
				out->last_line_mode = 0;
				out->ft_y_height = global_img_height - mv_y;
				out->ft_uv_height =
					global_img_height / 2 - (mv_y / 2);
				out->src_lum_addr += ft_pitch * mv_y;
				out->src_chr_addr += ft_pitch * (mv_y / 2);
			} else {
				out->ft_y_height = global_img_height - mv_y;
				out->ft_uv_height =
					global_img_height / 2 - (mv_y / 2);
				out->src_lum_addr += ft_pitch*mv_y;
				out->src_chr_addr += ft_pitch * (mv_y / 2);
			}
		}
	} else {
		if (out->start_col == 0) {
			if (mv_x < 0) {
				if ((mv_x) & 0x1) {
					out->src_chr_addr =
						out->src_chr_addr + 2;
				}
			} else if (mv_x > 0) {
				if ((mv_x) & 0x1) {
					out->src_lum_addr =
						out->src_lum_addr + mv_x;
					out->src_chr_addr =
						out->src_chr_addr + mv_x - 1;
				} else {
					out->src_lum_addr =
						out->src_lum_addr + mv_x;
					out->src_chr_addr =
						out->src_chr_addr + mv_x;
				}
			}
		} else {
			if ((mv_x < 0) && ((mv_x) & 0x1)) {
				out->src_lum_addr =
					out->src_lum_addr + mv_x;
				out->src_chr_addr =
					out->src_chr_addr + (mv_x / 2) * 2;
			} else if ((mv_x > 0) && ((mv_x) & 0x1)) {
				out->src_lum_addr =
					out->src_lum_addr + mv_x;
				out->src_chr_addr =
					out->src_chr_addr + mv_x - 1;
			} else {
				out->src_lum_addr = out->src_lum_addr + mv_x;
				out->src_chr_addr = out->src_chr_addr + mv_x;
			}
		}
		if (out->start_col == 0) {
			if (mv_x < 0) {
				if ((mv_x) & 0x1) {
					out->ft_y_width =
						out->ft_y_width + mv_x + 1;
					out->ft_uv_width =
						out->ft_uv_width + mv_x - 1;
				} else {
					out->ft_y_width =
						out->ft_y_width + mv_x;
					out->ft_uv_width =
						out->ft_uv_width + mv_x;
				}
			}
		}
		if ((global_img_width - 1) == end_col) {
			if (mv_x > 0) {
				if ((mv_x) & 0x1) {
					out->ft_y_width =
						out->ft_y_width - mv_x + 1;
					out->ft_uv_width =
						out->ft_uv_width - mv_x + 1;
				} else {
					out->ft_y_width =
						out->ft_y_width - mv_x;
					out->ft_uv_width =
						out->ft_uv_width - mv_x;
				}
			}
		}
		if (out->start_row == 0) {
			if (mv_y < 0) {
				out->ft_y_height = out->ft_y_height + mv_y;
				out->ft_uv_height =
					out->ft_uv_height + (mv_y / 2);
			}
		}
		if ((global_img_height - 1) == end_row) {
			if (mv_y > 0) {
				out->ft_y_height = out->ft_y_height - mv_y;
				out->ft_uv_height =
					out->ft_uv_height - (mv_y / 2);
			}
		}
		if ((out->start_row == 0) && mv_y < 0) {
			out->src_lum_addr = out->src_lum_addr;
			out->src_chr_addr = out->src_chr_addr;
		} else {
			out->src_lum_addr = out->src_lum_addr
				+ ft_pitch * mv_y;
			out->src_chr_addr = out->src_chr_addr
				+ ft_pitch * (mv_y / 2);
		}
		if ((out->start_row != 0 || mv_y > 0)
			&& ((mv_y) & 0x1)) {
			out->first_line_mode = 1;
			out->ft_uv_height = out->ft_uv_height + 1;
			if (mv_y < 0) {
				out->src_chr_addr = out->src_chr_addr
					- ft_pitch;
			}
		}
		if (((global_img_height - 1) != end_row || mv_y < 0)
			&& ((mv_y) & 0x1) == 0) {
			out->last_line_mode = 1;
			out->ft_uv_height = out->ft_uv_height + 1;
		}
	}

	return 0;
}
