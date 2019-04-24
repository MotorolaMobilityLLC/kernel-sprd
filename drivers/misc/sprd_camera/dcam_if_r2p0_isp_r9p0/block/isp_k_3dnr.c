/*
 * Copyright (C) 2017-2018 Spreadtrum Communications Inc.
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
#include <video/sprd_mm.h>
#include "isp_buf.h"
#include "isp_drv.h"
#include "isp_3dnr_drv.h"
#include "isp_slice.h"
#include "isp_reg.h"

static struct isp_3dnr_const_param g_3dnr_param_pre = {
	0, 1,
	5, 3, 3, 255, 255, 255,
	0, 255, 0, 255,
	20, 20, 20, 20, 20, 20, 20, 20, 20,
	15, 15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15, 15,
	63, 63, 63, 63, 63, 63, 63, 63, 63,
	63, 63, 63, 63, 63, 63, 63, 63, 63,
	63, 63, 63, 63, 63, 63, 63, 63, 63,
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
	{180, 180, 180, 180}, {180, 180, 180, 180}, {180, 180, 180, 180},
	{31, 37, 48, 63}, {31, 37, 48, 63}, {1, 2, 2, 3}, {1, 2, 2, 3},
	814, 931, 1047
};

static struct isp_3dnr_const_param g_3dnr_param_cap = {
	0, 1,
	5, 3, 3, 255, 255, 255,
	0, 255, 0, 255,
	30, 30, 30, 30, 30, 30, 30, 30, 30,
	30, 30, 30, 30, 30, 30, 30, 30, 30,
	30, 30, 30, 30, 30, 30, 30, 30, 30,
	63, 63, 63, 63, 63, 63, 63, 63, 63,
	63, 63, 63, 63, 63, 63, 63, 63, 63,
	63, 63, 63, 63, 63, 63, 63, 63, 63,
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
	{180, 180, 180, 180}, {180, 180, 180, 180}, {180, 180, 180, 180},
	{31, 37, 48, 63}, {31, 37, 48, 63}, {1, 2, 2, 3}, {1, 2, 2, 3},
	814, 931, 1047
};

static unsigned int g_frame_param[4][3] = {
	{128, 128, 128},
	{154, 154, 154},
	{154, 180, 180},
	{180, 180, 180},
};

void isp_3dnr_default_param(uint32_t idx)
{
	enum isp_id id = 0;

	id = ISP_GET_ISP_ID(idx);
	ISP_REG_MWR(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM0, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_YUV_3DNR_CONTROL0, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_STORE_LITE_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_YUV_3DNR_MEM_CTRL_PRE_PARAM0, BIT_0, 1);
	ISP_HREG_MWR(id, ISP_COMMON_SCL_PATH_SEL, BIT_8, 0 << 8);
}

void isp_3dnr_config_param(uint32_t idx, enum isp_operate_type type_id,
	struct isp_nr3_param *param)
{
	uint32_t val = 0;
	uint32_t blend_cnt = 0;
	enum isp_id id = 0;
	struct isp_mem_ctrl *mem_ctrl = NULL;
	struct isp_3dnr_store *nr3_store = NULL;
	struct isp_3dnr_crop *crop = NULL;
	struct isp_3dnr_const_param *blend_ptr = NULL;

	if (!param) {
		pr_err("fail to 3dnr_config_reg parm NULL\n");
		return;
	}

	id = ISP_GET_ISP_ID(idx);
	/*config memctl*/
	mem_ctrl = &param->mem_ctrl;
	ISP_REG_MWR(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM0,
		BIT_0, mem_ctrl->bypass);
	val = ((mem_ctrl->nr3_done_mode & 0x1) << 1)
		| ((mem_ctrl->back_toddr_en & 0x1) << 6)
		| ((mem_ctrl->chk_sum_clr_en & 0x1) << 9)
		| ((mem_ctrl->data_toyuv_en & 0x1) << 12)
		| ((mem_ctrl->roi_mode & 0x1) << 14)
		| ((mem_ctrl->retain_num & 0x7F) << 16)
		| ((mem_ctrl->ref_pic_flag & 0x1) << 23)
		| ((mem_ctrl->ft_max_len_sel & 0x1) << 28);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM0, val);
	val = ((mem_ctrl->first_line_mode & 0x1))
		| ((mem_ctrl->last_line_mode & 0x1) << 1);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_LINE_MODE, val);
	val = (mem_ctrl->start_row & 0x1FFF) |
		((mem_ctrl->start_col & 0x1FFF) << 16);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM1, val);
	val = ((mem_ctrl->global_img_height & 0x1FFF) << 16)
		| (mem_ctrl->global_img_width & 0x1FFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM2, val);
	val = (mem_ctrl->img_width & 0xFFF)
		| ((mem_ctrl->img_height & 0xFFF) << 16);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM3, val);
	val = (mem_ctrl->ft_y_width & 0xFFF)
		| ((mem_ctrl->ft_y_height & 0xFFF) << 16);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM4, val);
	val = (mem_ctrl->ft_uv_width & 0xFFF)
		| ((mem_ctrl->ft_uv_height & 0xFFF) << 16);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM5, val);
	val = (mem_ctrl->mv_y & 0xFF) |
			((mem_ctrl->mv_x & 0xFF) << 8);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM7, val);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_FT_CUR_LUMA_ADDR,
		mem_ctrl->ft_luma_addr);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_FT_CUR_CHROMA_ADDR,
		mem_ctrl->ft_chroma_addr);
	val = mem_ctrl->img_width & 0xFFFF;
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_FT_CTRL_PITCH, val);
	val = ((mem_ctrl->blend_y_en_start_col & 0xFFF) << 16)
		| (mem_ctrl->blend_y_en_start_row & 0xFFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM8, val);
	val = ((mem_ctrl->blend_y_en_end_col & 0xFFF) << 16)
		| (mem_ctrl->blend_y_en_end_row & 0xFFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM9, val);
	val = ((mem_ctrl->blend_uv_en_start_col & 0xFFF) << 16)
		| (mem_ctrl->blend_uv_en_start_row & 0xFFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM10, val);
	val = ((mem_ctrl->blend_uv_en_end_col & 0xFFF) << 16)
		|(mem_ctrl->blend_uv_en_end_row & 0xFFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM11, val);
	val = ((mem_ctrl->ft_hblank_num & 0xFFFF) << 16)
		| ((mem_ctrl->pipe_hblank_num & 0xFF) << 8)
		| (mem_ctrl->pipe_flush_line_num & 0xFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM12, val);
	val = ((mem_ctrl->pipe_nfull_num & 0x7FF) << 16)
		| (mem_ctrl->ft_fifo_nfull_num & 0xFFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_PARAM13, val);

	/*config store*/
	nr3_store = &param->nr3_store;
	val = (nr3_store->st_bypass & 0x1)
		| ((nr3_store->st_max_len_sel & 0x1) << 1)
		| ((nr3_store->shadow_clr_sel & 0x1) << 3)
		| ((nr3_store->chk_sum_clr_en & 0x1) << 4);
	ISP_REG_WR(idx, ISP_STORE_LITE_PARAM, val);
	val = (nr3_store->img_width & 0xFFFF)
		| ((nr3_store->img_height & 0xFFFF) << 16);
	ISP_REG_WR(idx, ISP_STORE_LITE_SIZE, val);
	val = nr3_store->st_luma_addr;
	ISP_REG_WR(idx, ISP_STORE_LITE_ADDR0, val);
	val = nr3_store->st_chroma_addr;
	ISP_REG_WR(idx, ISP_STORE_LITE_ADDR1, val);
	val = nr3_store->st_pitch & 0xFFFF;
	ISP_REG_WR(idx, ISP_STORE_LITE_PITCH, val);
	ISP_REG_MWR(idx, ISP_STORE_LITE_SHADOW_CLR,
		BIT_0, nr3_store->shadow_clr);

	/*config crop*/
	crop = &param->crop;
	ISP_REG_MWR(idx, ISP_YUV_3DNR_MEM_CTRL_PRE_PARAM0, BIT_0,
		crop->crop_bypass);
	val = (crop->src_width & 0xFFFF) | ((crop->src_height & 0xFFFF) << 16);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_PRE_PARAM1, val);
	val = (crop->dst_width & 0xFFFF) | ((crop->dst_height & 0xFFFF) << 16);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_PRE_PARAM2, val);
	val = (crop->start_y & 0xFFFF) | ((crop->start_x & 0xFFFF) << 16);
	ISP_REG_WR(idx, ISP_YUV_3DNR_MEM_CTRL_PRE_PARAM3, val);

	ISP_HREG_MWR(id, ISP_COMMON_SCL_PATH_SEL, BIT_8, 0x1 << 8);
	/*config variational blending*/
	ISP_REG_MWR(idx, ISP_YUV_3DNR_CONTROL0, BIT_0, 0);
	ISP_REG_MWR(idx, ISP_YUV_3DNR_CONTROL0, BIT_1, 0 << 1);
	ISP_REG_MWR(idx, ISP_YUV_3DNR_CONTROL0, BIT_2, 1 << 2);

	blend_cnt = param->blending_cnt;
	if (blend_cnt > 3)
		blend_cnt = 3;

	if (type_id != ISP_OPERATE_CAP) {
		blend_ptr = &g_3dnr_param_pre;
		val = (blend_ptr->y_pixel_noise_threshold & 0xFF)
		| ((blend_ptr->v_pixel_src_weight[blend_cnt] & 0xFF) << 8)
		| ((blend_ptr->u_pixel_src_weight[blend_cnt] & 0xFF) << 16)
		| ((blend_ptr->y_pixel_src_weight[blend_cnt] & 0xFF) << 24);
	} else {
		blend_ptr = &g_3dnr_param_cap;
		val = (blend_ptr->y_pixel_noise_threshold & 0xFF)
			| ((g_frame_param[blend_cnt][2] & 0xFF) << 8)
			| ((g_frame_param[blend_cnt][1] & 0xFF) << 16)
			| ((g_frame_param[blend_cnt][0] & 0xFF) << 24);
	}
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG1, val);

	blend_ptr->r1_circle = (unsigned int)(7 * mem_ctrl->img_width / 20);
	blend_ptr->r2_circle = (unsigned int)(2 * mem_ctrl->img_width / 5);
	blend_ptr->r3_circle = (unsigned int)(9 * mem_ctrl->img_width / 20);

	val = ((blend_ptr->v_divisor_factor[0] & 0x7) << 28)
		| ((blend_ptr->v_divisor_factor[1] & 0x7) << 24)
		| ((blend_ptr->v_divisor_factor[2] & 0x7) << 20)
		| ((blend_ptr->v_divisor_factor[3] & 0x7) << 16)
		| (blend_ptr->r1_circle & 0xFFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG23, val);
	val = ((blend_ptr->r2_circle & 0xFFF) << 16)
		| (blend_ptr->r3_circle & 0xFFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG24, val);
}

static void isp_3dnr_config_fast_me(enum isp_id idx,
	struct isp_3dnr_fast_me *fast_me)
{
	uint32_t val = 0;

	if (fast_me == NULL) {
		pr_err("fail to 3ndr fast_me_reg param NULL\n");
		return;
	}

	val = ((fast_me->nr3_channel_sel & 0x3) << 2)
		|(fast_me->nr3_project_mode & 0x3);
	DCAM_REG_MWR(idx, DCAM_NR3_FAST_ME_PARAM, 0xF, val << 4);
}

static void isp_3dnr_config_blend(uint32_t idx,
	struct isp_3dnr_const_param *blend)
{
	unsigned int val;

	if (blend == NULL) {
		pr_err("fail to 3ndr config_blend_reg param NULL\n");
		return;
	}

	val = ((blend->u_pixel_noise_threshold & 0xFF) << 24)
		| ((blend->v_pixel_noise_threshold & 0xFF) << 16)
		| ((blend->y_pixel_noise_weight & 0xFF) << 8)
		| (blend->u_pixel_noise_weight & 0xFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG2, val);

	val = ((blend->v_pixel_noise_weight & 0xFF) << 24) |
		((blend->threshold_radial_variation_u_range_min & 0xFF)
		<< 16)
		| ((blend->threshold_radial_variation_u_range_max & 0xFF)
		<< 8)
		| (blend->threshold_radial_variation_v_range_min & 0xFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG3, val);

	val = ((blend->threshold_radial_variation_v_range_max & 0xFF) << 24)
		| ((blend->y_threshold_polyline_0 & 0xFF) << 16)
		| ((blend->y_threshold_polyline_1 & 0xFF) << 8)
		| (blend->y_threshold_polyline_2 & 0xFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG4, val);

	val = ((blend->y_threshold_polyline_3 & 0xFF) << 24)
		| ((blend->y_threshold_polyline_4 & 0xFF) << 16)
		| ((blend->y_threshold_polyline_5 & 0xFF) << 8)
		| (blend->y_threshold_polyline_6 & 0xFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG5, val);

	val = ((blend->y_threshold_polyline_7 & 0xFF) << 24)
		| ((blend->y_threshold_polyline_8 & 0xFF) << 16)
		| ((blend->u_threshold_polyline_0 & 0xFF) << 8)
		| (blend->u_threshold_polyline_1 & 0xFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG6, val);

	val = ((blend->u_threshold_polyline_2 & 0xFF) << 24)
		| ((blend->u_threshold_polyline_3 & 0xFF) << 16)
		| ((blend->u_threshold_polyline_4 & 0xFF) << 8)
		| (blend->u_threshold_polyline_5 & 0xFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG7, val);

	val = ((blend->u_threshold_polyline_6 & 0xFF) << 24)
		| ((blend->u_threshold_polyline_7 & 0xFF) << 16)
		| ((blend->u_threshold_polyline_8 & 0xFF) << 8)
		| (blend->v_threshold_polyline_0 & 0xFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG8, val);

	val = ((blend->v_threshold_polyline_1 & 0xFF) << 24)
		| ((blend->v_threshold_polyline_2 & 0xFF) << 16)
		| ((blend->v_threshold_polyline_3 & 0xFF) << 8)
		| (blend->v_threshold_polyline_4 & 0xFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG9, val);

	val = ((blend->v_threshold_polyline_5 & 0xFF) << 24)
		| ((blend->v_threshold_polyline_6 & 0xFF) << 16)
		| ((blend->v_threshold_polyline_7 & 0xFF) << 8)
		| (blend->v_threshold_polyline_8 & 0xFF);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG10, val);

	val = ((blend->y_intensity_gain_polyline_0 & 0x7F) << 24)
		| ((blend->y_intensity_gain_polyline_1 & 0x7F) << 16)
		| ((blend->y_intensity_gain_polyline_2 & 0x7F) << 8)
		| (blend->y_intensity_gain_polyline_3 & 0x7F);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG11, val);

	val = ((blend->y_intensity_gain_polyline_4 & 0x7F) << 24)
		| ((blend->y_intensity_gain_polyline_5 & 0x7F) << 16)
		| ((blend->y_intensity_gain_polyline_6 & 0x7F) << 8)
		| (blend->y_intensity_gain_polyline_7 & 0x7F);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG12, val);

	val = ((blend->y_intensity_gain_polyline_8 & 0x7F) << 24)
		| ((blend->u_intensity_gain_polyline_0 & 0x7F) << 16)
		| ((blend->u_intensity_gain_polyline_1 & 0x7F) << 8)
		| (blend->u_intensity_gain_polyline_2 & 0x7F);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG13, val);

	val = ((blend->u_intensity_gain_polyline_3 & 0x7F) << 24)
		| ((blend->u_intensity_gain_polyline_4 & 0x7F) << 16)
		| ((blend->u_intensity_gain_polyline_5 & 0x7F) << 8)
		| (blend->u_intensity_gain_polyline_6 & 0x7F);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG14, val);

	val = ((blend->u_intensity_gain_polyline_7 & 0x7F) << 24)
		| ((blend->u_intensity_gain_polyline_8 & 0x7F) << 16)
		| ((blend->v_intensity_gain_polyline_0 & 0x7F) << 8)
		| (blend->v_intensity_gain_polyline_1 & 0x7F);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG15, val);

	val = ((blend->v_intensity_gain_polyline_2 & 0x7F) << 24)
		| ((blend->v_intensity_gain_polyline_3 & 0x7F) << 16)
		| ((blend->v_intensity_gain_polyline_4 & 0x7F) << 8)
		| (blend->v_intensity_gain_polyline_5 & 0x7F);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG16, val);

	val = ((blend->v_intensity_gain_polyline_6 & 0x7F) << 24)
		| ((blend->v_intensity_gain_polyline_7 & 0x7F) << 16)
		| ((blend->v_intensity_gain_polyline_8 & 0x7F) << 8)
		| (blend->gradient_weight_polyline_0 & 0x7F);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG17, val);

	val = ((blend->gradient_weight_polyline_1 & 0x7F) << 24)
		| ((blend->gradient_weight_polyline_2 & 0x7F) << 16)
		| ((blend->gradient_weight_polyline_3 & 0x7F) << 8)
		| (blend->gradient_weight_polyline_4 & 0x7F);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG18, val);

	val = ((blend->gradient_weight_polyline_5 & 0x7F) << 24)
		| ((blend->gradient_weight_polyline_6 & 0x7F) << 16)
		| ((blend->gradient_weight_polyline_7 & 0x7F) << 8)
		| (blend->gradient_weight_polyline_8 & 0x7F);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG19, val);

	val = ((blend->gradient_weight_polyline_9 & 0x7F) << 24)
		| ((blend->gradient_weight_polyline_10 & 0x7F) << 16)
		| ((blend->u_threshold_factor[0] & 0x7F) << 8)
		| (blend->u_threshold_factor[1] & 0x7F);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG20, val);

	val = ((blend->u_threshold_factor[2] & 0x7F) << 24)
		| ((blend->u_threshold_factor[3] & 0x7F) << 16)
		| ((blend->v_threshold_factor[0] & 0x7F) << 8)
		| (blend->v_threshold_factor[1] & 0x7F);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG21, val);

	val = ((blend->v_threshold_factor[2] & 0x7F) << 24)
		| ((blend->v_threshold_factor[3] & 0x7F) << 16)
		| ((blend->u_divisor_factor[0] & 0x7) << 12)
		| ((blend->u_divisor_factor[1] & 0x7) << 8)
		| ((blend->u_divisor_factor[2] & 0x7) << 4)
		| (blend->u_divisor_factor[3] & 0x7);
	ISP_REG_WR(idx, ISP_YUV_3DNR_CFG22, val);
}

static int isp_k_3dnr_block(struct isp_io_param *param, uint32_t idx)
{
	int ret = 0;
	struct isp_3dnr_tuning_param tuning_param;
	struct isp_3dnr_fast_me fast_me = {0};
	struct isp_3dnr_const_param *blend_para = NULL;
	enum isp_id id = ISP_ID_0;

	ret = copy_from_user((void *)&tuning_param,
		param->property_param, sizeof(struct isp_3dnr_tuning_param));
	if (ret != 0) {
		pr_err("fail to 3dnr copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	if (param->property == ISP_PRO_3DNR_UPDATE_PRE_PARAM)
		blend_para = &g_3dnr_param_pre;
	else
		blend_para = &g_3dnr_param_cap;

	memcpy(blend_para, &tuning_param.blend_param,
		sizeof(struct isp_3dnr_const_param));
	memcpy(&fast_me, &tuning_param.fast_me,
		sizeof(struct isp_3dnr_fast_me));

	id = ISP_GET_ISP_ID(idx);
	isp_3dnr_config_fast_me(id, &fast_me);

	isp_3dnr_config_blend(idx, blend_para);

	return ret;
}

int isp_k_cfg_3dnr(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;

	if (!param || !param->property_param) {
		pr_err("fail to chech param\n");
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_3DNR_UPDATE_CAP_PARAM:
	case ISP_PRO_3DNR_UPDATE_PRE_PARAM:
		ret = isp_k_3dnr_block(param, idx);
		break;
	default:
		pr_err("fail to 3dnr cmd id = %d\n", param->property);
		break;
	}

	return ret;
}
