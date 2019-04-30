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

#include <linux/pagemap.h>
#include <linux/sprd_iommu.h>

#include "isp_drv.h"
#include "isp_block.h"
#include "isp_buf.h"
#include "isp_3dnr_drv.h"
#include "cam_common.h"
#include "isp_slice.h"

static int sprd_isp3dnr_memctrl_param_get(struct isp_nr3_param *param,
	struct camera_frame *frame)
{
	struct isp_mem_ctrl *mem_ctrl = NULL;
	struct cam_buf_info *buf_info = NULL;
	enum isp_drv_rtn ret = ISP_RTN_SUCCESS;

	if (!param || !frame) {
		pr_err("fail to 3ndr get memctrl parm NULL\n");
		return -ISP_RTN_PARA_ERR;
	}

	mem_ctrl = &param->mem_ctrl;
	buf_info = &param->buf_info;
	/*configuration param0*/
	if (!param->blending_cnt) {
		mem_ctrl->ref_pic_flag = 0;
		param->blending_cnt = 0;
	} else {
		pr_debug("3DNR ref_pic_flag nonzero\n");
		mem_ctrl->ref_pic_flag = 1;
	}
	mem_ctrl->ft_max_len_sel = 1;
	mem_ctrl->retain_num = 0;
	mem_ctrl->roi_mode = 0;
	mem_ctrl->data_toyuv_en = 1;
	mem_ctrl->chk_sum_clr_en = 1;
	mem_ctrl->back_toddr_en = 1;
	mem_ctrl->nr3_done_mode = 0;
	mem_ctrl->bypass = 0;
	mem_ctrl->start_col = 0;
	mem_ctrl->start_row = 0;
	/*configuration param2*/
	mem_ctrl->global_img_width = frame->width;
	mem_ctrl->global_img_height = frame->height;
	/*configuration param3*/
	mem_ctrl->img_width = frame->width;
	mem_ctrl->img_height = frame->height;
	pr_debug("3DNR img_width=%d, img_height=%d\n",
		mem_ctrl->img_width,
		mem_ctrl->img_height);
	/*configuration param4/5*/
	mem_ctrl->ft_y_width = frame->width;
	mem_ctrl->ft_y_height = frame->height;
	mem_ctrl->ft_uv_width = frame->width;
	mem_ctrl->ft_uv_height = frame->height / 2;
	/*if have ref pic, configuration param7*/
	if (mem_ctrl->ref_pic_flag) {
		mem_ctrl->mv_x = frame->mv.mv_x;
		mem_ctrl->mv_y = frame->mv.mv_y;
	}
	/*configuration ref frame pitch*/
	mem_ctrl->ft_pitch = frame->width;

	if (param->blending_cnt % 2 == 1) {
		mem_ctrl->ft_luma_addr = buf_info->iova[0];
		mem_ctrl->ft_chroma_addr = buf_info->iova[0]
			+ (param->prev_size.width * param->prev_size.height);
	} else {
		mem_ctrl->ft_luma_addr = buf_info->iova[1];
		mem_ctrl->ft_chroma_addr = buf_info->iova[1]
			+ (param->prev_size.width * param->prev_size.height);
	}
	mem_ctrl->first_line_mode = 0;
	mem_ctrl->last_line_mode = 0;

	/*porting from isp fw code */
	if (frame->mv.mv_x < 0) {
		if (frame->mv.mv_x & 0x1) {
			mem_ctrl->ft_y_width =
				frame->width + frame->mv.mv_x + 1;
			mem_ctrl->ft_uv_width =
				frame->width + frame->mv.mv_x - 1;
			mem_ctrl->ft_luma_addr = mem_ctrl->ft_luma_addr;
			mem_ctrl->ft_chroma_addr = mem_ctrl->ft_chroma_addr + 2;
		} else {
			mem_ctrl->ft_y_width =
				frame->width + frame->mv.mv_x;
			mem_ctrl->ft_uv_width =
				frame->width + frame->mv.mv_x;
			mem_ctrl->ft_luma_addr = mem_ctrl->ft_luma_addr;
			mem_ctrl->ft_chroma_addr = mem_ctrl->ft_chroma_addr;
		}
	} else if (frame->mv.mv_x > 0) {
		if (frame->mv.mv_x & 0x1) {
			mem_ctrl->ft_y_width =
				frame->width - frame->mv.mv_x + 1;
			mem_ctrl->ft_uv_width =
				frame->width - frame->mv.mv_x + 1;
			mem_ctrl->ft_luma_addr =
				mem_ctrl->ft_luma_addr + frame->mv.mv_x;
			mem_ctrl->ft_chroma_addr =
				mem_ctrl->ft_chroma_addr + frame->mv.mv_x - 1;
		} else {
			mem_ctrl->ft_y_width =
				frame->width - frame->mv.mv_x;
			mem_ctrl->ft_uv_width =
				frame->width - frame->mv.mv_x;
			mem_ctrl->ft_luma_addr =
				mem_ctrl->ft_luma_addr + frame->mv.mv_x;
			mem_ctrl->ft_chroma_addr =
				mem_ctrl->ft_chroma_addr + frame->mv.mv_x;
		}
	}
	if (frame->mv.mv_y < 0) {
		if (frame->mv.mv_y & 0x1) {
			mem_ctrl->last_line_mode = 0;
			mem_ctrl->ft_uv_height =
				frame->height / 2 + frame->mv.mv_y / 2;
		} else {
			mem_ctrl->last_line_mode = 1;
			mem_ctrl->ft_uv_height =
				frame->height / 2 + frame->mv.mv_y / 2 + 1;
		}
		mem_ctrl->first_line_mode = 0;
		mem_ctrl->ft_y_height =
			frame->height + frame->mv.mv_y;
		mem_ctrl->ft_luma_addr = mem_ctrl->ft_luma_addr;
		mem_ctrl->ft_chroma_addr = mem_ctrl->ft_chroma_addr;
	} else if (frame->mv.mv_y > 0) {
		if ((frame->mv.mv_y) & 0x1) {
			/*temp modify first_line_mode =0*/
			mem_ctrl->first_line_mode = 0;
			mem_ctrl->last_line_mode = 0;
			mem_ctrl->ft_y_height =
				frame->height - frame->mv.mv_y;
			mem_ctrl->ft_uv_height =
				frame->height / 2 - (frame->mv.mv_y / 2);

			mem_ctrl->ft_luma_addr =
				mem_ctrl->ft_luma_addr
				+ mem_ctrl->ft_pitch * frame->mv.mv_y;
			mem_ctrl->ft_chroma_addr =
				mem_ctrl->ft_chroma_addr
				+ mem_ctrl->ft_pitch * (frame->mv.mv_y / 2);
		} else {
			mem_ctrl->ft_y_height =
				frame->height - frame->mv.mv_y;
			mem_ctrl->ft_uv_height =
				frame->height / 2 - (frame->mv.mv_y / 2);
			mem_ctrl->ft_luma_addr =
				mem_ctrl->ft_luma_addr
				+ mem_ctrl->ft_pitch * frame->mv.mv_y;
			mem_ctrl->ft_chroma_addr =
				mem_ctrl->ft_chroma_addr
				+ mem_ctrl->ft_pitch * (frame->mv.mv_y / 2);
		}
	}
	pr_debug("3DNR ft_luma=0x%lx,ft_chroma=0x%lx, mv_x=%d,mv_y=%d\n",
		mem_ctrl->ft_luma_addr,
		mem_ctrl->ft_chroma_addr,
		frame->mv.mv_x,
		frame->mv.mv_y);
	pr_debug("3DNR ft_y_h=%d, ft_uv_h=%d, ft_y_w=%d, ft_uv_w=%d\n",
		mem_ctrl->ft_y_height,
		mem_ctrl->ft_uv_height,
		mem_ctrl->ft_y_width,
		mem_ctrl->ft_uv_width);
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

	return ret;
}

static int sprd_isp3dnr_store_param_get(struct isp_nr3_param *param,
	struct camera_frame *frame)
{
	struct isp_3dnr_store *nr3_store = NULL;
	struct cam_buf_info *buf_info = NULL;
	enum isp_drv_rtn ret = ISP_RTN_SUCCESS;

	if (!param || !frame) {
		pr_err("fail to 3ndr nr3store parm NULL\n");
		return -ISP_RTN_PARA_ERR;
	}

	nr3_store = &param->nr3_store;
	buf_info = &param->buf_info;
	nr3_store->chk_sum_clr_en = 1;
	nr3_store->shadow_clr_sel = 1;
	nr3_store->st_max_len_sel = 1;
	nr3_store->st_bypass = 0;
	nr3_store->img_width = frame->width;
	nr3_store->img_height = frame->height;

	if (param->blending_cnt % 2 != 1) {
		nr3_store->st_luma_addr = buf_info->iova[0];
		nr3_store->st_chroma_addr = buf_info->iova[0]
			+ (param->prev_size.width * param->prev_size.height);
	} else {
		nr3_store->st_luma_addr =  buf_info->iova[1];
		nr3_store->st_chroma_addr = buf_info->iova[1]
			+ (param->prev_size.width * param->prev_size.height);
	}

	pr_debug("3DNR nr3store st_luma=0x%lx, st_chroma=0x%lx\n",
		nr3_store->st_luma_addr, nr3_store->st_chroma_addr);
	pr_debug("3DNR nr3store w=%d,h=%d,frame_w=%d,frame_h=%d\n",
		nr3_store->img_width,
		nr3_store->img_height,
		frame->width,
		frame->height);
	nr3_store->st_pitch = frame->width;
	nr3_store->shadow_clr = 1;

	return ret;
}

static int sprd_isp3dnr_crop_param_get(struct isp_nr3_param *param,
	struct camera_frame *frame)
{
	enum isp_drv_rtn ret = ISP_RTN_SUCCESS;

	if (!param || !frame) {
		pr_err("fail to 3ndr get_crop_param NULL\n");
		return -ISP_RTN_PARA_ERR;
	}

	param->crop.crop_bypass = 1;
	return ret;
}

static int sprd_isp3dnr_mv_conversion(int mv_x, int mv_y,
	uint32_t mode_projection,
	int input_width, int output_width,
	int input_height, int output_height,
	int *out_mv_x, int *out_mv_y)
{
	if (mode_projection == 1) {
		if (mv_x > 0)
			*out_mv_x = (mv_x * output_width + (input_width >> 1))
						/ input_width;
		else
			*out_mv_x = (mv_x * output_width - (input_width >> 1))
						/ input_width;

		if (mv_y > 0)
			*out_mv_y = (mv_y * output_height + (input_height >> 1))
						/ input_height;
		else
			*out_mv_y = (mv_y * output_height - (input_height >> 1))
						/ input_height;
	} else {
		if (mv_x > 0)
			*out_mv_x = (mv_x * output_width + input_width)
						/ (input_width * 2);
		else
			*out_mv_x = (mv_x * output_width - input_width)
						/ (input_width * 2);

		if (mv_y > 0)
			*out_mv_y = (mv_y * output_height + input_height)
						/ (input_height * 2);
		else
			*out_mv_y = (mv_y * output_height - input_height)
						/ (input_height * 2);
	}

	return 0;
}

int sprd_isp_3dnr_param_get(struct isp_nr3_param *param,
	void *frame)
{
	enum isp_drv_rtn ret = ISP_RTN_SUCCESS;

	if (!param || !frame) {
		ret = -ISP_RTN_PARA_ERR;
		pr_err("fail to 3ndr get_preview_parm NULL\n");
		return ret;
	}

	ret = sprd_isp3dnr_memctrl_param_get(param, frame);
	if (ret) {
		pr_err("fail to 3ndr get memctrl\n");
		return ret;
	}

	ret = sprd_isp3dnr_store_param_get(param, frame);
	if (ret) {
		pr_err("fail to 3ndr get nr3store\n");
		return ret;
	}

	ret = sprd_isp3dnr_crop_param_get(param, frame);
	if (ret) {
		pr_err("fail to 3ndr get nr3store\n");
		return ret;
	}

	param->blending_cnt++;
	return ret;
}

int sprd_isp_3dnr_conversion_mv(struct isp_nr3_param *nr3_param,
	void *pdata)
{
	int input_x = 0;
	int input_y = 0;
	int output_x = 0;
	int output_y = 0;
	int input_width = 0;
	int input_height = 0;
	int output_width = 0;
	int output_height = 0;
	struct camera_frame *frame = NULL;
	enum isp_drv_rtn ret = ISP_RTN_SUCCESS;

	if (!nr3_param || !pdata) {
		pr_err("fail to 3ndr calculate 3dnr me NULL\n");
		return -ISP_RTN_PARA_ERR;
	}

	frame = (struct camera_frame *)pdata;

	input_x = frame->mv.mv_x;
	input_y = frame->mv.mv_y;
	input_width = nr3_param->me_conv.input_width;
	input_height = nr3_param->me_conv.input_height;
	output_width = frame->width;
	output_height = frame->height;
	ret = sprd_isp3dnr_mv_conversion(input_x,
			input_y,
			nr3_param->me_conv.project_mode,
			input_width,
			output_width,
			input_height,
			output_height,
			&output_x,
			&output_y);

	frame->mv.mv_x = output_x;
	frame->mv.mv_y = output_y;
	pr_debug("3DNR conv_mv in_x =%d, in_y =%d, out_x=%d, out_y=%d\n",
		input_x, input_y, output_x, output_y);

	return ret;
}

int sprd_isp_3dnr_release(void *isp_handle)
{
	struct isp_pipe_dev *dev = NULL;
	struct cam_buf_info *buf = NULL;
	struct isp_path_desc *path_pre = NULL;
	struct isp_path_desc *path_vid = NULL;
	struct isp_path_desc *path_cap = NULL;
	struct isp_module *module = NULL;
	enum isp_drv_rtn ret = ISP_RTN_SUCCESS;

	if (isp_handle == NULL) {
		pr_err("fail to 3dnr_module_release param err\n");
		return -ISP_RTN_PARA_ERR;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->module_info;
	path_pre = &module->isp_path[ISP_SCL_PRE];
	path_vid = &module->isp_path[ISP_SCL_VID];
	path_cap = &module->isp_path[ISP_SCL_CAP];

	if ((path_pre->valid && path_pre->nr3_param.need_3dnr)
		|| (path_vid->valid && path_vid->nr3_param.need_3dnr)) {
		buf = (struct cam_buf_info *)&path_pre->nr3_param.buf_info;
		ret = sprd_cam_buf_addr_unmap(buf);
		if (ret) {
			pr_err("fail to 3ndr prev unmap buf\n");
			return ret;
		}
	}
	if (path_cap->valid && path_cap->nr3_param.need_3dnr) {
		buf = (struct cam_buf_info *)&path_cap->nr3_param.buf_info;
		ret = sprd_cam_buf_addr_unmap(buf);
		if (ret) {
			pr_err("fail to 3ndr cap unmap buf\n");
			return ret;
		}
	}

	memset((void *)&path_pre->nr3_param, 0x00, sizeof(path_pre->nr3_param));
	memset((void *)&path_vid->nr3_param, 0x00, sizeof(path_vid->nr3_param));
	memset((void *)&path_cap->nr3_param, 0x00, sizeof(path_cap->nr3_param));

	return ret;
}
