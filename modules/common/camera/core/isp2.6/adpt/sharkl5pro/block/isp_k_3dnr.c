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
#include  "cam_trusty.h"

#include "isp_hw.h"
#include "isp_reg.h"
#include "cam_block.h"
#include "isp_3dnr.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "3DNR: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static void isp_3dnr_config_mem_ctrl(uint32_t idx,
		struct isp_3dnr_mem_ctrl *mem_ctrl, bool  nr3sec_eb)
{
	unsigned int val;

	if ((g_isp_bypass[idx] >> _EISP_NR3) & 1)
		mem_ctrl->bypass = 1;

	val = ((mem_ctrl->nr3_done_mode & 0x1) << 1) |
		((mem_ctrl->nr3_ft_path_sel & 0x1) << 2) |
		((mem_ctrl->back_toddr_en & 0x1) << 6) |
		((mem_ctrl->chk_sum_clr_en & 0x1) << 9) |
		((mem_ctrl->data_toyuv_en & 0x1) << 12) |
		((mem_ctrl->roi_mode & 0x1) << 14) |
		((mem_ctrl->retain_num & 0x7F) << 16) |
		((mem_ctrl->ref_pic_flag & 0x1) << 23) |
		((mem_ctrl->ft_max_len_sel & 0x1) << 28) |
		(mem_ctrl->bypass & 0x1);
	ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_PARAM0, val);

	val = ((mem_ctrl->last_line_mode & 0x1) << 1) |
		((mem_ctrl->first_line_mode & 0x1));
	ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_LINE_MODE, val);

	val = ((mem_ctrl->start_col & 0x1FFF) << 16) |
		(mem_ctrl->start_row & 0x1FFF);
	ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_PARAM1, val);

	val = ((mem_ctrl->global_img_height & 0x1FFF) << 16) |
		(mem_ctrl->global_img_width & 0x1FFF);
	ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_PARAM2, val);

	val = ((mem_ctrl->img_height & 0x1FFF) << 16) |
		(mem_ctrl->img_width & 0x1FFF);
	ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_PARAM3, val);

	val = ((mem_ctrl->ft_y_height & 0x1FFF) << 16) |
		(mem_ctrl->ft_y_width & 0x1FFF);
	ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_PARAM4, val);

	val = (mem_ctrl->ft_uv_width & 0x1FFF)
		| ((mem_ctrl->ft_uv_height & 0x1FFF) << 16);
	ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_PARAM5, val);

	val = ((mem_ctrl->mv_x & 0xFF) << 8) |
		(mem_ctrl->mv_y & 0xFF);
	ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_PARAM7, val);


	if (!nr3sec_eb) {
		ISP_REG_WR(idx,
			ISP_3DNR_MEM_CTRL_FT_CUR_LUMA_ADDR,
			mem_ctrl->ft_luma_addr);

		ISP_REG_WR(idx,
			ISP_3DNR_MEM_CTRL_FT_CUR_CHROMA_ADDR,
			mem_ctrl->ft_chroma_addr);

		val = mem_ctrl->img_width & 0xFFFF;
		ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_FT_CTRL_PITCH, val);
	} else {
		val = mem_ctrl->img_width & 0xFFFF;
		cam_trusty_isp_3dnr_fetch_set(mem_ctrl->ft_chroma_addr,
			mem_ctrl->ft_luma_addr, val);
	}

	val = ((mem_ctrl->blend_y_en_start_col & 0x1FFF) << 16) |
		(mem_ctrl->blend_y_en_start_row & 0x1FFF);
	ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_PARAM8, val);

	val = ((mem_ctrl->blend_y_en_end_col & 0x1FFF) << 16) |
		(mem_ctrl->blend_y_en_end_row & 0x1FFF);
	ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_PARAM9, val);

	val = ((mem_ctrl->blend_uv_en_start_col & 0x1FFF) << 16) |
		(mem_ctrl->blend_uv_en_start_row & 0x1FFF);
	ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_PARAM10, val);

	val = ((mem_ctrl->blend_uv_en_end_col & 0x1FFF) << 16) |
		(mem_ctrl->blend_uv_en_end_row & 0x1FFF);
	ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_PARAM11, val);

	val = ((mem_ctrl->ft_hblank_num & 0xFFFF) << 16) |
		((mem_ctrl->pipe_hblank_num & 0xFF) << 8) |
		(mem_ctrl->pipe_flush_line_num & 0xFF);
	ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_PARAM12, val);

	val = ((mem_ctrl->pipe_nfull_num & 0x7FF) << 16) |
		(mem_ctrl->ft_fifo_nfull_num & 0xFFF);
	ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_PARAM13, val);
}

void isp_3dnr_config_blend(uint32_t idx,
		struct isp_3dnr_blend_info *blend)
{
	unsigned int val;

	if (blend == NULL) {
		pr_err("fail to 3ndr config_blend_reg param NULL\n");
		return;
	}

	if (blend->isupdate == 0)
		return;
	blend->isupdate = 0;
	ISP_REG_MWR(idx, ISP_3DNR_BLEND_CONTROL0, BIT_0, blend->bypass);
	ISP_REG_MWR(idx, ISP_3DNR_BLEND_CONTROL0, BIT_1, blend->fusion_mode << 1);
	ISP_REG_MWR(idx, ISP_3DNR_BLEND_CONTROL0, BIT_2, blend->filter_switch << 2);

	val = ((blend->y_pixel_src_weight[0] & 0xFF) << 24) |
		((blend->u_pixel_src_weight[0] & 0xFF) << 16) |
		((blend->v_pixel_src_weight[0] & 0xFF) << 8) |
		(blend->y_pixel_noise_threshold & 0xFF);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG1, val);

	val = ((blend->u_pixel_noise_threshold & 0xFF) << 24) |
		((blend->v_pixel_noise_threshold & 0xFF) << 16) |
		((blend->y_pixel_noise_weight & 0xFF) << 8) |
		(blend->u_pixel_noise_weight & 0xFF);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG2, val);

	val = ((blend->v_pixel_noise_weight & 0xFF) << 24) |
		((blend->threshold_radial_variation_u_range_min & 0xFF) << 16) |
		((blend->threshold_radial_variation_u_range_max & 0xFF) << 8) |
		(blend->threshold_radial_variation_v_range_min & 0xFF);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG3, val);

	val = ((blend->threshold_radial_variation_v_range_max & 0xFF) << 24) |
		((blend->y_threshold_polyline_0 & 0xFF) << 16) |
		((blend->y_threshold_polyline_1 & 0xFF) << 8) |
		(blend->y_threshold_polyline_2 & 0xFF);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG4, val);

	val = ((blend->y_threshold_polyline_3 & 0xFF) << 24) |
		((blend->y_threshold_polyline_4 & 0xFF) << 16) |
		((blend->y_threshold_polyline_5 & 0xFF) << 8) |
		(blend->y_threshold_polyline_6 & 0xFF);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG5, val);

	val = ((blend->y_threshold_polyline_7 & 0xFF) << 24) |
		((blend->y_threshold_polyline_8 & 0xFF) << 16) |
		((blend->u_threshold_polyline_0 & 0xFF) << 8) |
		(blend->u_threshold_polyline_1 & 0xFF);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG6, val);

	val = ((blend->u_threshold_polyline_2 & 0xFF) << 24) |
		((blend->u_threshold_polyline_3 & 0xFF) << 16) |
		((blend->u_threshold_polyline_4 & 0xFF) << 8) |
		(blend->u_threshold_polyline_5 & 0xFF);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG7, val);

	val = ((blend->u_threshold_polyline_6 & 0xFF) << 24) |
		((blend->u_threshold_polyline_7 & 0xFF) << 16) |
		((blend->u_threshold_polyline_8 & 0xFF) << 8) |
		(blend->v_threshold_polyline_0 & 0xFF);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG8, val);

	val = ((blend->v_threshold_polyline_1 & 0xFF) << 24) |
		((blend->v_threshold_polyline_2 & 0xFF) << 16) |
		((blend->v_threshold_polyline_3 & 0xFF) << 8) |
		(blend->v_threshold_polyline_4 & 0xFF);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG9, val);

	val = ((blend->v_threshold_polyline_5 & 0xFF) << 24) |
		((blend->v_threshold_polyline_6 & 0xFF) << 16) |
		((blend->v_threshold_polyline_7 & 0xFF) << 8) |
		(blend->v_threshold_polyline_8 & 0xFF);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG10, val);

	val = ((blend->y_intensity_gain_polyline_0 & 0x7F) << 24) |
		((blend->y_intensity_gain_polyline_1 & 0x7F) << 16) |
		((blend->y_intensity_gain_polyline_2 & 0x7F) << 8) |
		(blend->y_intensity_gain_polyline_3 & 0x7F);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG11, val);

	val = ((blend->y_intensity_gain_polyline_4 & 0x7F) << 24) |
		((blend->y_intensity_gain_polyline_5 & 0x7F) << 16) |
		((blend->y_intensity_gain_polyline_6 & 0x7F) << 8) |
		(blend->y_intensity_gain_polyline_7 & 0x7F);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG12, val);

	val = ((blend->y_intensity_gain_polyline_8 & 0x7F) << 24) |
		((blend->u_intensity_gain_polyline_0 & 0x7F) << 16) |
		((blend->u_intensity_gain_polyline_1 & 0x7F) << 8) |
		(blend->u_intensity_gain_polyline_2 & 0x7F);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG13, val);

	val = ((blend->u_intensity_gain_polyline_3 & 0x7F) << 24) |
		((blend->u_intensity_gain_polyline_4 & 0x7F) << 16) |
		((blend->u_intensity_gain_polyline_5 & 0x7F) << 8) |
		(blend->u_intensity_gain_polyline_6 & 0x7F);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG14, val);

	val = ((blend->u_intensity_gain_polyline_7 & 0x7F) << 24) |
		((blend->u_intensity_gain_polyline_8 & 0x7F) << 16) |
		((blend->v_intensity_gain_polyline_0 & 0x7F) << 8) |
		(blend->v_intensity_gain_polyline_1 & 0x7F);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG15, val);

	val = ((blend->v_intensity_gain_polyline_2 & 0x7F) << 24) |
		((blend->v_intensity_gain_polyline_3 & 0x7F) << 16) |
		((blend->v_intensity_gain_polyline_4 & 0x7F) << 8) |
		(blend->v_intensity_gain_polyline_5 & 0x7F);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG16, val);

	val = ((blend->v_intensity_gain_polyline_6 & 0x7F) << 24) |
		((blend->v_intensity_gain_polyline_7 & 0x7F) << 16) |
		((blend->v_intensity_gain_polyline_8 & 0x7F) << 8) |
		(blend->gradient_weight_polyline_0 & 0x7F);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG17, val);

	val = ((blend->gradient_weight_polyline_1 & 0x7F) << 24) |
		((blend->gradient_weight_polyline_2 & 0x7F) << 16) |
		((blend->gradient_weight_polyline_3 & 0x7F) << 8) |
		(blend->gradient_weight_polyline_4 & 0x7F);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG18, val);

	val = ((blend->gradient_weight_polyline_5 & 0x7F) << 24) |
		((blend->gradient_weight_polyline_6 & 0x7F) << 16) |
		((blend->gradient_weight_polyline_7 & 0x7F) << 8) |
		(blend->gradient_weight_polyline_8 & 0x7F);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG19, val);

	val = ((blend->gradient_weight_polyline_9 & 0x7F) << 24) |
		((blend->gradient_weight_polyline_10 & 0x7F) << 16) |
		((blend->u_threshold_factor0 & 0x7F) << 8) |
		(blend->u_threshold_factor1 & 0x7F);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG20, val);

	val = ((blend->u_threshold_factor2 & 0x7F) << 24) |
		((blend->u_threshold_factor3 & 0x7F) << 16) |
		((blend->v_threshold_factor0 & 0x7F) << 8) |
		(blend->v_threshold_factor1 & 0x7F);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG21, val);

	val = ((blend->v_threshold_factor2 & 0x7F) << 24) |
		((blend->v_threshold_factor3 & 0x7F) << 16) |
		((blend->u_divisor_factor0 & 0x7) << 12) |
		((blend->u_divisor_factor1 & 0x7) << 8) |
		((blend->u_divisor_factor2 & 0x7) << 4) |
		(blend->u_divisor_factor3 & 0x7);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG22, val);

	val = ((blend->v_divisor_factor0 & 0x7) << 28) |
		((blend->v_divisor_factor1 & 0x7) << 24) |
		((blend->v_divisor_factor2 & 0x7) << 20) |
		((blend->v_divisor_factor3 & 0x7) << 16) |
		(blend->r1_circle & 0xFFF);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG23, val);

	val = ((blend->r2_circle & 0xFFF) << 16) |
		(blend->r3_circle & 0xFFF);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG24, val);
}

static void isp_3dnr_config_store(uint32_t idx,
				  struct isp_3dnr_store *nr3_store)
{
	unsigned int val;

	if ((g_isp_bypass[idx] >> _EISP_NR3) & 1)
		nr3_store->st_bypass = 1;
	val = ((nr3_store->chk_sum_clr_en & 0x1) << 4) |
		((nr3_store->shadow_clr_sel & 0x1) << 3) |
		((nr3_store->st_max_len_sel & 0x1) << 1) |
		(nr3_store->st_bypass & 0x1);
	ISP_REG_WR(idx, ISP_3DNR_STORE_PARAM, val);

	val = ((nr3_store->img_height & 0xFFFF) << 16) |
		(nr3_store->img_width & 0xFFFF);
	ISP_REG_WR(idx, ISP_3DNR_STORE_SIZE, val);

	val = nr3_store->st_luma_addr;
	ISP_REG_WR(idx, ISP_3DNR_STORE_LUMA_ADDR, val);

	val = nr3_store->st_chroma_addr;
	ISP_REG_WR(idx, ISP_3DNR_STORE_CHROMA_ADDR, val);

	val = nr3_store->st_pitch & 0xFFFF;
	ISP_REG_WR(idx, ISP_3DNR_STORE_PITCH, val);

	ISP_REG_MWR(idx, ISP_3DNR_STORE_SHADOW_CLR,
		BIT_0, nr3_store->shadow_clr);
}

static void isp_3dnr_config_fbd_fetch(uint32_t idx,
		struct isp_3dnr_fbd_fetch *nr3_fbd_fetch)
{
	unsigned int val;

	val = nr3_fbd_fetch->fbdc_cr_ch0123_val0;
	ISP_REG_WR(idx, ISP_FBD_NR3_PARAM0, val);

	val = nr3_fbd_fetch->fbdc_cr_ch0123_val1;
	ISP_REG_WR(idx, ISP_FBD_NR3_PARAM1, val);

	val = (nr3_fbd_fetch->fbdc_cr_uv_val1 & 0xFF) |
		((nr3_fbd_fetch->fbdc_cr_uv_val0 & 0xFF)<<8)|
		((nr3_fbd_fetch->fbdc_cr_y_val1 & 0xFF)<<16) |
		((nr3_fbd_fetch->fbdc_cr_y_val0 & 0xFF)<<24);
	ISP_REG_WR(idx, ISP_FBD_NR3_PARAM2, val);

	val = (nr3_fbd_fetch->y_tiles_num_pitch & 0xFF) |
		(nr3_fbd_fetch->y_tile_addr_init_x256 & 0xFFFFFF00);
	ISP_REG_WR(idx, ISP_FBD_NR3_T_ADDR_Y, val);

	val = nr3_fbd_fetch->y_header_addr_init;
	ISP_REG_WR(idx, ISP_FBD_NR3_H_ADDR_Y, val);

	val = (nr3_fbd_fetch->c_tiles_num_pitch & 0xFF) |
		(nr3_fbd_fetch->c_tile_addr_init_x256 & 0xFFFFFF00);
	ISP_REG_WR(idx, ISP_FBD_NR3_T_ADDR_C, val);

	val = nr3_fbd_fetch->c_header_addr_init;
	ISP_REG_WR(idx, ISP_FBD_NR3_H_ADDR_C, val);

	val = (nr3_fbd_fetch->y_pixel_size_in_ver & 0x1FFF) |
		((nr3_fbd_fetch->y_pixel_size_in_hor & 0x1FFF) << 16);
	ISP_REG_WR(idx, ISP_FBD_NR3_SIZE_Y, val);

	val = (nr3_fbd_fetch->c_pixel_size_in_ver & 0x1FFF) |
		((nr3_fbd_fetch->c_pixel_size_in_hor & 0x1FFF) << 16);
	ISP_REG_MWR(idx, ISP_FBD_NR3_SIZE_C, 0x1FFF1FFF, val);

	val = (nr3_fbd_fetch->y_pixel_start_in_ver & 1) |
		((nr3_fbd_fetch->y_pixel_start_in_hor & 0xFF) << 16);
	ISP_REG_MWR(idx, ISP_FBD_NR3_START_Y, 0xFF0001, val);

	val = (nr3_fbd_fetch->c_pixel_start_in_ver & 1) |
		((nr3_fbd_fetch->c_pixel_start_in_hor & 0xFF) << 16);
	ISP_REG_MWR(idx, ISP_FBD_NR3_START_C, 0xFF0001, val);

	val = (nr3_fbd_fetch->y_tiles_num_in_ver & 0x1FFF) |
		((nr3_fbd_fetch->y_tiles_num_in_hor & 0x1F) << 16);
	ISP_REG_MWR(idx, ISP_FBD_NR3_TILE_SIZE_Y, 0x1F1FFF, val);

	val = (nr3_fbd_fetch->c_tiles_num_in_ver & 0x1FFF) |
		((nr3_fbd_fetch->c_tiles_num_in_hor & 0x1F) << 16);
	ISP_REG_MWR(idx, ISP_FBD_NR3_TILE_SIZE_C, 0x1F1FFF, val);

	val = 0;
	ISP_REG_MWR(idx, ISP_FBD_NR3_SLICE_TILE_PARAM, 0x10001, val);

	val = 0xFF << 16;
	ISP_REG_MWR(idx, ISP_FBD_NR3_READ_SPECIAL, 0xFF0000, val);
}

static void isp_3dnr_config_fbc_store(uint32_t idx,
		struct isp_3dnr_fbc_store *nr3_fbc_store)
{
	unsigned int val;

	if ((g_isp_bypass[idx] >> _EISP_NR3) & 1)
		nr3_fbc_store->bypass = 1;

	val = (nr3_fbc_store->bypass & 0x1) |
		((nr3_fbc_store->slice_mode_en & 0x1) << 1);
	ISP_REG_WR(idx, ISP_FBC_3DNR_STORE_PARAM, val);

	if (nr3_fbc_store->bypass)
		return;

	val = (nr3_fbc_store->size_in_hor & 0x1FFF) |
		((nr3_fbc_store->size_in_ver & 0x1FFF) << 16);
	ISP_REG_WR(idx, ISP_FBC_3DNR_STORE_SLICE_SIZE, val);

	val = nr3_fbc_store->y_tile_addr_init_x256;
	ISP_REG_WR(idx, ISP_FBC_3DNR_STORE_SLICE_YADDR, val);

	val = nr3_fbc_store->c_tile_addr_init_x256;
	ISP_REG_WR(idx, ISP_FBC_3DNR_STORE_SLICE_CADDR, val);

	val = nr3_fbc_store->tile_number_pitch & 0xFFFF;
	ISP_REG_MWR(idx, ISP_FBC_3DNR_STORE_TILE_PITCH, 0xFFFF, val);

	val = nr3_fbc_store->y_header_addr_init;
	ISP_REG_WR(idx, ISP_FBC_3DNR_STORE_SLICE_YHEADER, val);

	val = nr3_fbc_store->c_header_addr_init;
	ISP_REG_WR(idx, ISP_FBC_3DNR_STORE_SLICE_CHEADER, val);

	val = nr3_fbc_store->fbc_constant_yuv;
	ISP_REG_WR(idx, ISP_FBC_3DNR_STORE_CONSTANT, val);

	val = nr3_fbc_store->later_bits & 0xFFFF;
	ISP_REG_MWR(idx, ISP_FBC_3DNR_STORE_BITS, 0xFFFF, val);

	val = nr3_fbc_store->tile_number & 0xFFFFF;
	ISP_REG_MWR(idx, ISP_FBC_3DNR_STORE_TILE_NUM, 0xFFFFF, val);

	val = 0x20002 & 0xFF00FF;
	ISP_REG_MWR(idx, ISP_FBC_3DNR_STORE_NFULL_LEVEL, 0xFF00FF, val);
}

static void isp_3dnr_config_crop(uint32_t idx,
		struct isp_3dnr_crop *crop)
{
	unsigned int val;

	if ((g_isp_bypass[idx] >> _EISP_NR3) & 1)
		crop->crop_bypass = 1;
	ISP_REG_MWR(idx, ISP_3DNR_MEM_CTRL_PRE_PARAM0,
		BIT_0, crop->crop_bypass);

	val = ((crop->src_height & 0xFFFF) << 16) |
		(crop->src_width & 0xFFFF);
	ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_PRE_PARAM1, val);

	val = ((crop->dst_height & 0xFFFF) << 16) |
		(crop->dst_width & 0xFFFF);
	ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_PRE_PARAM2, val);

	val = ((crop->start_x & 0xFFFF) << 16) |
		(crop->start_y & 0xFFFF);
	ISP_REG_WR(idx, ISP_3DNR_MEM_CTRL_PRE_PARAM3, val);
}

static int isp_k_3dnr_block(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;
	struct isp_dev_3dnr_info *pnr3;

	pnr3 = &isp_k_param->nr3_info_base;

	ret = copy_from_user((void *)pnr3,
			param->property_param,
			sizeof(struct isp_dev_3dnr_info));
	if (ret != 0) {
		pr_err("fail to 3dnr copy from user, ret = %d\n", ret);
		return -EPERM;
	}

	pnr3->blend.isupdate = 1;

	memcpy(&isp_k_param->nr3d_info, pnr3, sizeof(struct isp_dev_3dnr_info));

	return ret;
}

/*
 * global function
 */
void isp_3dnr_bypass_config(uint32_t idx)
{
	ISP_REG_MWR(idx, ISP_3DNR_MEM_CTRL_PARAM0, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_3DNR_BLEND_CONTROL0, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_3DNR_STORE_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_FBC_3DNR_STORE_PARAM, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_3DNR_MEM_CTRL_PRE_PARAM0, BIT_0, 1);
	ISP_REG_MWR(idx, ISP_COMMON_SCL_PATH_SEL, BIT_8, 0 << 8);
}

#ifdef _NR3_DATA_TO_YUV_
static unsigned long irq_base[ISP_CONTEXT_MAX] = {
	ISP_P0_INT_BASE,
	ISP_C0_INT_BASE,
	ISP_P1_INT_BASE,
	ISP_C1_INT_BASE
};
#endif /* _NR3_DATA_TO_YUV_ */

void isp_3dnr_config_param(struct isp_3dnr_ctx_desc *ctx)
{
	struct isp_3dnr_mem_ctrl *mem_ctrl = NULL;
	struct isp_3dnr_store *nr3_store = NULL;
	struct isp_3dnr_fbd_fetch *nr3_fbd_fetch = NULL;
	struct isp_3dnr_fbc_store *nr3_fbc_store = NULL;
	struct isp_3dnr_blend_info *blend = NULL;
	struct isp_3dnr_crop *crop = NULL;
	uint32_t blend_cnt = 0, idx = 0;
	unsigned int val;

	if (!ctx) {
		pr_err("fail to 3dnr_config_reg parm NULL\n");
		return;
	}

	idx = ctx->ctx_id;
	blend = &ctx->isp_block->nr3_info_base.blend;
	mem_ctrl = &ctx->mem_ctrl;
	isp_3dnr_config_mem_ctrl(idx, mem_ctrl, ctx->nr3_sec_mode);

	nr3_store = &ctx->nr3_store;
	isp_3dnr_config_store(idx, nr3_store);

	nr3_fbc_store = &ctx->nr3_fbc_store;
	isp_3dnr_config_fbc_store(idx, nr3_fbc_store);

	nr3_fbd_fetch = &ctx->nr3_fbd_fetch;
	isp_3dnr_config_fbd_fetch(idx, nr3_fbd_fetch);

	crop = &ctx->crop;
	isp_3dnr_config_crop(idx, crop);

	ISP_REG_MWR(idx, ISP_3DNR_BLEND_CONTROL0, BIT_0, blend->bypass);
	ISP_REG_MWR(idx, ISP_COMMON_SCL_PATH_SEL, BIT_8, 0x1 << 8);

	blend_cnt = ctx->blending_cnt;
	if (blend_cnt > 3)
		blend_cnt = 3;

	val = ((blend->y_pixel_src_weight[blend_cnt] & 0xFF) << 24) |
		((blend->u_pixel_src_weight[blend_cnt] & 0xFF) << 16) |
		((blend->v_pixel_src_weight[blend_cnt] & 0xFF) << 8) |
		(blend->y_pixel_noise_threshold & 0xFF);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG1, val);


#ifdef _NR3_DATA_TO_YUV_
	if (mem_ctrl->data_toyuv_en) {
		uint32_t val = 0;

		val = ISP_HREG_RD(ISP_P0_INT_BASE + ISP_INT_STATUS);
		pr_debug("vall 0x%x\n", val);
		val |= (BIT_1);
		pr_debug("val after 0x%x\n", val);
		ISP_HREG_MWR(irq_base[idx] + ISP_INT_ALL_DONE_CTRL, 0x1F, val);

		ISP_REG_MWR(idx, ISP_STORE_PRE_CAP_BASE + ISP_STORE_PARAM,
			BIT_0, 0);
		ISP_REG_MWR(idx, ISP_STORE_VID_BASE + ISP_STORE_PARAM,
			BIT_0, 1);
		ISP_REG_MWR(idx, ISP_STORE_THUMB_BASE + ISP_STORE_PARAM,
			BIT_0, 1);
	} else {
		uint32_t val = 0;

		val = ISP_HREG_RD(ISP_P0_INT_BASE+ISP_INT_STATUS);
		pr_debug("val 0x%x\n", val);
		val &= (~BIT_1);
		pr_debug("val after 0x%x\n", val);
		ISP_HREG_MWR(irq_base[idx] + ISP_INT_ALL_DONE_CTRL, 0x1F, val);

		ISP_REG_MWR(idx, ISP_STORE_PRE_CAP_BASE + ISP_STORE_PARAM,
			BIT_0, 1);
		ISP_REG_MWR(idx, ISP_STORE_VID_BASE + ISP_STORE_PARAM,
			BIT_0, 1);
		ISP_REG_MWR(idx, ISP_STORE_THUMB_BASE + ISP_STORE_PARAM,
			BIT_0, 1);
	}
#endif/* _NR3_DATA_TO_YUV_ */
}

int isp_k_update_3dnr(uint32_t idx,
	struct isp_k_block *isp_k_param,
	uint32_t new_width, uint32_t old_width,
	uint32_t new_height, uint32_t old_height)
{
	unsigned int val;
	uint32_t r1_circle, r1_circle_limit;
	uint32_t r2_circle, r2_circle_limit;
	uint32_t r3_circle, r3_circle_limit;
	struct isp_dev_3dnr_info *pnr3, *pdst;

	pdst = &isp_k_param->nr3d_info;
	pnr3 = &isp_k_param->nr3_info_base;

	r1_circle = pnr3->blend.r1_circle * new_width / old_width;
	r1_circle_limit = (new_width / 2);
	r1_circle_limit *= pnr3->blend.r1_circle_factor;
	r1_circle_limit /= pnr3->blend.r_circle_base;
	r1_circle = (r1_circle < r1_circle_limit) ? r1_circle : r1_circle_limit;

	r2_circle = pnr3->blend.r2_circle * new_width / old_width;
	r2_circle_limit = (new_width / 2);
	r2_circle_limit *= pnr3->blend.r2_circle_factor;
	r2_circle_limit /= pnr3->blend.r_circle_base;
	r2_circle = (r2_circle < r2_circle_limit) ? r2_circle : r2_circle_limit;

	r3_circle = pnr3->blend.r3_circle * new_width / old_width;
	r3_circle_limit = (new_width / 2);
	r3_circle_limit *= pnr3->blend.r3_circle_factor;
	r3_circle_limit /= pnr3->blend.r_circle_base;
	r3_circle = (r3_circle < r3_circle_limit) ? r3_circle : r3_circle_limit;

	val = ((pnr3->blend.v_divisor_factor0 & 0x7) << 28) |
		((pnr3->blend.v_divisor_factor1 & 0x7) << 24) |
		((pnr3->blend.v_divisor_factor2 & 0x7) << 20) |
		((pnr3->blend.v_divisor_factor3 & 0x7) << 16) |
		(r1_circle & 0xFFF);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG23, val);

	val = ((r2_circle & 0xFFF) << 16) |
		(r3_circle & 0xFFF);
	ISP_REG_WR(idx, ISP_3DNR_BLEND_CFG24, val);

	pdst->blend.r1_circle = r1_circle;
	pdst->blend.r2_circle = r2_circle;
	pdst->blend.r3_circle = r3_circle;

	pr_debug("orig %d %d %d, factor %d %d %d, base %d, new %d %d %d\n",
		pnr3->blend.r1_circle, pnr3->blend.r2_circle, pnr3->blend.r3_circle,
		pnr3->blend.r1_circle_factor, pnr3->blend.r2_circle_factor,
		pnr3->blend.r3_circle_factor, pnr3->blend.r_circle_base,
		r1_circle, r2_circle, r3_circle);
	return 0;
}

int isp_k_cfg_3dnr(struct isp_io_param *param,
	struct isp_k_block *isp_k_param, uint32_t idx)
{
	int ret = 0;

	if (!param || !param->property_param) {
		pr_err("fail to get valid param error %p\n", param);
		return -1;
	}

	switch (param->property) {
	case ISP_PRO_3DNR_BLOCK:
		ret = isp_k_3dnr_block(param, isp_k_param, idx);
		break;
	default:
		pr_err("fail to 3dnr cmd id = %d\n", param->property);
		break;
	}

	return ret;
}

int isp_k_cpy_3dnr(struct isp_k_block *param_block, struct isp_k_block *isp_k_param)
{
	int ret = 0;
	if (isp_k_param->nr3_info_base.blend.isupdate == 1) {
		memcpy(&param_block->nr3_info_base, &isp_k_param->nr3_info_base, sizeof(struct isp_dev_3dnr_info));
		memcpy(&param_block->nr3d_info, &isp_k_param->nr3d_info, sizeof(struct isp_dev_3dnr_info));
		isp_k_param->nr3_info_base.blend.isupdate = 0;
		param_block->nr3_info_base.blend.isupdate = 1;
	}

	return ret;
}
