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

#include "isp_3dnr.h"
#include "alg_nr3_calc.h"
#include "isp_core.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_3DNR: %d %d %s : "fmt, current->pid, __LINE__, __func__


static int isp3dnr_calc_img_pitch(enum isp_fetch_format fmt, uint32_t w)
{
	int pitch = 0;

	switch (fmt) {
	case ISP_FETCH_RAW10:
	case ISP_FETCH_CSI2_RAW10:
		pitch = w;
		break;
	case ISP_FETCH_YUV420_2FRAME_MIPI:
	case ISP_FETCH_YVU420_2FRAME_MIPI:
	case ISP_FETCH_YUV420_2FRAME:
	case ISP_FETCH_YVU420_2FRAME:
		pitch = w * 2;
		break;
	default:
		pr_err("fail to get isp fetch format:%d, pitch:%d\n", fmt, pitch);
	}

	return pitch;
}

static int alg_nr3_memctrl_base_on_mv_update_ver0(struct isp_3dnr_ctx_desc *ctx)
{
	struct isp_3dnr_mem_ctrl *mem_ctrl = &ctx->mem_ctrl;

	if (ctx->mv.mv_x < 0) {
		if (ctx->mv.mv_x & 0x1) {
			mem_ctrl->ft_y_width = ctx->width + ctx->mv.mv_x + 1;
			mem_ctrl->ft_uv_width = ctx->width + ctx->mv.mv_x - 1;
			mem_ctrl->ft_chroma_addr = mem_ctrl->ft_chroma_addr + 2;
		} else {
			mem_ctrl->ft_y_width = ctx->width + ctx->mv.mv_x;
			mem_ctrl->ft_uv_width = ctx->width + ctx->mv.mv_x;
		}
	} else if (ctx->mv.mv_x > 0) {
		if (ctx->mv.mv_x & 0x1) {
			mem_ctrl->ft_y_width =
				ctx->width - ctx->mv.mv_x + 1;
			mem_ctrl->ft_uv_width =
				ctx->width - ctx->mv.mv_x + 1;
			mem_ctrl->ft_luma_addr =
				mem_ctrl->ft_luma_addr + ctx->mv.mv_x;
			mem_ctrl->ft_chroma_addr =
				mem_ctrl->ft_chroma_addr + ctx->mv.mv_x - 1;
		} else {
			mem_ctrl->ft_y_width = ctx->width - ctx->mv.mv_x;
			mem_ctrl->ft_uv_width = ctx->width - ctx->mv.mv_x;
			mem_ctrl->ft_luma_addr =
				mem_ctrl->ft_luma_addr + ctx->mv.mv_x;
			mem_ctrl->ft_chroma_addr =
				mem_ctrl->ft_chroma_addr + ctx->mv.mv_x;
		}
	}
	if (ctx->mv.mv_y < 0) {
		if (ctx->mv.mv_y & 0x1) {
			mem_ctrl->last_line_mode = 0;
			mem_ctrl->ft_uv_height = ctx->height / 2 + ctx->mv.mv_y / 2;
		} else {
			mem_ctrl->last_line_mode = 1;
			mem_ctrl->ft_uv_height = ctx->height / 2 + ctx->mv.mv_y / 2 + 1;
		}
		mem_ctrl->first_line_mode = 0;
		mem_ctrl->ft_y_height = ctx->height + ctx->mv.mv_y;
	} else if (ctx->mv.mv_y > 0) {
		if ((ctx->mv.mv_y) & 0x1) {
			/*temp modify first_line_mode =0*/
			mem_ctrl->first_line_mode = 0;
			mem_ctrl->last_line_mode = 0;
			mem_ctrl->ft_y_height = ctx->height - ctx->mv.mv_y;
			mem_ctrl->ft_uv_height = ctx->height / 2 - (ctx->mv.mv_y / 2);

			mem_ctrl->ft_luma_addr = mem_ctrl->ft_luma_addr
				+ mem_ctrl->ft_pitch * ctx->mv.mv_y;
			mem_ctrl->ft_chroma_addr = mem_ctrl->ft_chroma_addr
				+ mem_ctrl->ft_pitch * (ctx->mv.mv_y / 2);
		} else {
			mem_ctrl->ft_y_height = ctx->height - ctx->mv.mv_y;
			mem_ctrl->ft_uv_height = ctx->height / 2 - (ctx->mv.mv_y / 2);
			mem_ctrl->ft_luma_addr = mem_ctrl->ft_luma_addr
				+ mem_ctrl->ft_pitch * ctx->mv.mv_y;
			mem_ctrl->ft_chroma_addr = mem_ctrl->ft_chroma_addr
				+ mem_ctrl->ft_pitch * (ctx->mv.mv_y / 2);
		}
	}

	pr_debug("3DNR ft_luma=0x%lx,ft_chroma=0x%lx, mv_x=%d,mv_y=%d\n",
		mem_ctrl->ft_luma_addr,
		mem_ctrl->ft_chroma_addr,
		ctx->mv.mv_x,
		ctx->mv.mv_y);
	pr_debug("3DNR ft_y_h=%d, ft_uv_h=%d, ft_y_w=%d, ft_uv_w=%d\n",
		mem_ctrl->ft_y_height,
		mem_ctrl->ft_uv_height,
		mem_ctrl->ft_y_width,
		mem_ctrl->ft_uv_width);

	return 0;
}

static int alg_nr3_memctrl_base_on_mv_update_ver1(struct isp_3dnr_ctx_desc *ctx)
{
	struct isp_3dnr_mem_ctrl *mem_ctrl = &ctx->mem_ctrl;
	uint32_t global_img_width = 0, global_img_height = 0, ft_pitch = 0;
	struct ImageRegion_Info *image_region_info = NULL;
	int Y_start_x = 0, Y_end_x = 0, Y_start_y = 0, Y_end_y = 0;
	int UV_start_x = 0, UV_end_x = 0, UV_start_y = 0, UV_end_y = 0;

	image_region_info = ctx->image_region_info;
	image_region_info->mv_x = ctx->mv.mv_x;
	image_region_info->mv_y = ctx->mv.mv_y;
	image_region_info->region_end_row = ctx->height - 1;
	image_region_info->region_end_col = ctx->width - 1;
	image_region_info->region_width = image_region_info->region_end_col -
		image_region_info->region_start_col + 1;
	image_region_info->region_height= image_region_info->region_end_row -
		image_region_info->region_start_row + 1;

	global_img_width = ctx->width;
	global_img_height = ctx->height;
	ft_pitch = mem_ctrl->ft_pitch;

	nr3d_fetch_ref_image_position(image_region_info, global_img_width,
		global_img_height);//fetch position in full size image according to MV.

	Y_start_x = image_region_info->Y_start_x;
	Y_end_x = image_region_info->Y_end_x;
	Y_start_y = image_region_info->Y_start_y;
	Y_end_y = image_region_info->Y_end_y;

	if (image_region_info->mv_x < 0) {
		UV_start_x = 0;
	} else if (image_region_info->mv_x % 2 == 0) {
		UV_start_x = image_region_info->mv_x >> 1;
	} else {
		UV_start_x = image_region_info->mv_x >> 1;
	}
	UV_end_x = image_region_info->UV_end_x;

	if (image_region_info->mv_y < 0 && image_region_info->mv_y % 2 != 0) {
		UV_start_y = 1;//image_region_info->mv_y;
	} else if (image_region_info->mv_y < 0 && image_region_info->mv_y % 2 == 0) {
		UV_start_y = 0;
	} else {
		UV_start_y = image_region_info->mv_y >> 1;
	}
	UV_end_y = image_region_info->UV_end_y;

	if (image_region_info->mv_x < 0 && image_region_info->mv_x % 2 != 0)
		UV_start_x = image_region_info->UV_start_x + 1;

	if (image_region_info->mv_x > 0 && image_region_info->mv_x & 0x1)
		Y_start_x -= 1;

	mem_ctrl->ft_y_height = Y_end_y - Y_start_y + 1;
	mem_ctrl->ft_uv_height = UV_end_y - UV_start_y + 1;
	mem_ctrl->ft_y_width = (Y_end_x - Y_start_x + 2) / 2 * 2;
	mem_ctrl->ft_uv_width = (UV_end_x - UV_start_x + 1) * 2;

	if (mem_ctrl->yuv_8bits_flag == 0) {
		mem_ctrl->ft_luma_addr += Y_start_y * ft_pitch + Y_start_x * 2;
		mem_ctrl->ft_chroma_addr += (UV_start_y * ft_pitch) + UV_start_x * 2 * 2;
	} else {
		mem_ctrl->ft_luma_addr += Y_start_y * ft_pitch + Y_start_x;
		mem_ctrl->ft_chroma_addr += (UV_start_y * ft_pitch) + UV_start_x * 2;
	}

	pr_debug("3DNR ft_luma=0x%lx,ft_chroma=0x%lx, mv_x=%d,mv_y=%d\n",
		mem_ctrl->ft_luma_addr,
		mem_ctrl->ft_chroma_addr,
		ctx->mv.mv_x,
		ctx->mv.mv_y);
	pr_debug("3DNR ft_y_h=%d, ft_uv_h=%d, ft_y_w=%d, ft_uv_w=%d\n",
		mem_ctrl->ft_y_height,
		mem_ctrl->ft_uv_height,
		mem_ctrl->ft_y_width,
		mem_ctrl->ft_uv_width);

	return 0;
}

static int alg_nr3_memctrl_base_on_mv_update(struct isp_3dnr_ctx_desc *ctx)
{
	if (!ctx) {
		pr_err("fail to get valid in ptr\n");
		return 0;
	}

	switch (ctx->nr3_mv_version) {
		case ALG_NR3_MV_VER_0:
			alg_nr3_memctrl_base_on_mv_update_ver0(ctx);
			break;
		case ALG_NR3_MV_VER_1:
			alg_nr3_memctrl_base_on_mv_update_ver1(ctx);
			break;
		default:
			pr_err("fail to get invalid version %d\n", ctx->nr3_mv_version);
			break;
	}
	return 0;
}

static int isp3dnr_store_config_gen(struct isp_3dnr_ctx_desc *ctx)
{
	int ret = 0;
	struct isp_3dnr_store *store = NULL;

	if (!ctx) {
		pr_err("fail to get valid 3dnr store parameter\n");
		return -EINVAL;
	}

	store = &ctx->nr3_store;

	store->img_width = ctx->width;
	store->img_height = ctx->height;

	store->chk_sum_clr_en = 1;
	store->shadow_clr_sel = 1;
	store->st_max_len_sel = 1;
	store->shadow_clr = 1;
	store->last_frm_en = 3;
	store->flip_en = 0;
	store->mono_en = 0;
	store->endian = 0;
	store->mirror_en = 0;
	store->speed_2x = 1;
	store->store_res = 1;

	if (ctx->blending_cnt % 2 != 1) {
		store->st_luma_addr = ctx->buf_info[0]->iova[0];
		store->st_chroma_addr = ctx->buf_info[0]->iova[0] +
			(store->st_pitch * ctx->height);
	} else {
		store->st_luma_addr = ctx->buf_info[1]->iova[0];
		store->st_chroma_addr = ctx->buf_info[1]->iova[0] +
			(store->st_pitch * ctx->height);
	}

	pr_debug("3DNR nr3store st_luma=0x%lx, st_chroma=0x%lx\n",
		store->st_luma_addr, store->st_chroma_addr);
	pr_debug("3DNR nr3store w=%d,h=%d,frame_w=%d,frame_h=%d\n",
		store->img_width,
		store->img_height,
		ctx->width,
		ctx->height);

	return ret;
}

static int isp3dnr_crop_config_gen(struct isp_3dnr_ctx_desc *ctx)
{
	int ret = 0;

	if (!ctx) {
		pr_err("fail to get valid 3dnr crop parameter\n");
		return -EINVAL;
	}

	ctx->crop.crop_bypass = 1;
	ctx->crop.src_width = ctx->width;
	ctx->crop.src_height = ctx->height;
	ctx->crop.dst_width = ctx->width;
	ctx->crop.dst_height = ctx->height;
	ctx->crop.start_x = 0;
	ctx->crop.start_y = 0;

	return ret;
}

static int isp3dnr_memctrl_config_gen(struct isp_3dnr_ctx_desc *ctx)
{
	int ret = 0;
	struct isp_3dnr_mem_ctrl *mem_ctrl = NULL;

	if (!ctx) {
		pr_err("fail to get valid 3dnr mem ctrl parameter\n");
		return -EINVAL;
	}

	mem_ctrl = &ctx->mem_ctrl;

	/* configuration param0 */
	if (!ctx->blending_cnt) {
		mem_ctrl->ref_pic_flag = 0;
		/* ctx->blending_cnt = 0; */
	} else {
		pr_debug("3DNR ref_pic_flag nonzero\n");
		mem_ctrl->ref_pic_flag = 1;
	}

	mem_ctrl->ft_max_len_sel = 1;
	mem_ctrl->retain_num = 0;
	mem_ctrl->roi_mode = 0;
	mem_ctrl->data_toyuv_en = 1;
	mem_ctrl->chk_sum_clr_en = 1;
	mem_ctrl->slice_info = 0;
	mem_ctrl->back_toddr_en = 1;
	mem_ctrl->nr3_done_mode = 0;
	mem_ctrl->start_col = 0;
	mem_ctrl->start_row = 0;
	if (mem_ctrl->bypass)
		mem_ctrl->nr3_ft_path_sel = 1;

	/* configuration param2 */
	mem_ctrl->global_img_width = ctx->width;
	mem_ctrl->global_img_height = ctx->height;

	/* configuration param3 */
	mem_ctrl->img_width = ctx->width;
	mem_ctrl->img_height = ctx->height;
	pr_debug("3DNR img_width=%d, img_height=%d, ft_pitch:%d\n",
		mem_ctrl->img_width, mem_ctrl->img_height, mem_ctrl->ft_pitch);

	/* configuration param4/5 */
	mem_ctrl->ft_y_width = ctx->width;
	mem_ctrl->ft_y_height = ctx->height;
	mem_ctrl->ft_uv_width = ctx->width;
	mem_ctrl->ft_uv_height = ctx->height / 2;

	mem_ctrl->mv_x = ctx->mv.mv_x;
	mem_ctrl->mv_y = ctx->mv.mv_y;

	if (ctx->blending_cnt % 2 == 1) {
		mem_ctrl->ft_luma_addr = ctx->buf_info[0]->iova[0];
		mem_ctrl->ft_chroma_addr = ctx->buf_info[0]->iova[0] +
			(mem_ctrl->ft_pitch * ctx->height);
	} else {
		mem_ctrl->ft_luma_addr = ctx->buf_info[1]->iova[0];
		mem_ctrl->ft_chroma_addr = ctx->buf_info[1]->iova[0] +
			(mem_ctrl->ft_pitch * ctx->height);
	}
	mem_ctrl->frame_addr.addr_ch0 = mem_ctrl->ft_luma_addr;
	mem_ctrl->frame_addr.addr_ch1 = mem_ctrl->ft_chroma_addr;

	mem_ctrl->first_line_mode = 0;
	mem_ctrl->last_line_mode = 0;

	if (ctx->type == NR3_FUNC_PRE || ctx->type == NR3_FUNC_CAP)
		alg_nr3_memctrl_base_on_mv_update(ctx);

	/*configuration param 8~11*/
	mem_ctrl->blend_y_en_start_row = 0;
	mem_ctrl->blend_y_en_start_col = 0;
	mem_ctrl->blend_y_en_end_row = ctx->height - 1;
	mem_ctrl->blend_y_en_end_col = ctx->width - 1;
	mem_ctrl->blend_uv_en_start_row = 0;
	mem_ctrl->blend_uv_en_start_col = 0;
	mem_ctrl->blend_uv_en_end_row = ctx->height / 2 - 1;
	mem_ctrl->blend_uv_en_end_col = ctx->width - 1;

	/*configuration param 12*/
	mem_ctrl->ft_hblank_num = 32;
	mem_ctrl->pipe_hblank_num = 60;
	mem_ctrl->pipe_flush_line_num = 17;

	/*configuration param 13*/
	mem_ctrl->pipe_nfull_num = 100;
	mem_ctrl->ft_fifo_nfull_num = 2648;

	return ret;
}

static int isp3dnr_fbd_fetch_config_gen(struct isp_3dnr_ctx_desc *ctx)
{
	int ret = 0;
	int mv_x = 0, mv_y = 0;
	uint32_t pad_width = 0, pad_height = 0;
	uint32_t cur_width = 0, cur_height = 0;
	struct compressed_addr out_addr = {0};
	struct dcam_compress_cal_para cal_fbc = {0};
	struct isp_3dnr_fbd_fetch *fbd_fetch = NULL;

	if (!ctx) {
		pr_err("invalid parameter, fbd fetch\n");
		return -EINVAL;
	}

	fbd_fetch = &ctx->nr3_fbd_fetch;
	cur_width = ctx->mem_ctrl.img_width;
	cur_height = ctx->mem_ctrl.img_height;

	/*This is for N6pro*/
	fbd_fetch->hblank_en = 0;
	fbd_fetch->start_3dnr_afbd = 1;
	fbd_fetch->slice_start_pxl_xpt = 0;
	fbd_fetch->slice_start_pxl_ypt = 0;
	fbd_fetch->hblank_num = 0x8000;
	fbd_fetch->chk_sum_auto_clr = 1;
	fbd_fetch->slice_width= cur_width;
	fbd_fetch->slice_height = cur_height;
	fbd_fetch->dout_req_signal_type = 1;
	fbd_fetch->data_bits = ctx->mem_ctrl.yuv_8bits_flag ? 8 : 10;
	fbd_fetch->tile_num_pitch = (cur_width + ISP_FBD_TILE_WIDTH - 1) / ISP_FBD_TILE_WIDTH;

	if (fbd_fetch->data_bits == 10)
		fbd_fetch->afbc_mode = 7;
	else
		fbd_fetch->afbc_mode = 5;

	cal_fbc.fbc_info = &ctx->fbc_info;
	cal_fbc.fmt = DCAM_STORE_YVU420;
	cal_fbc.out = &fbd_fetch->hw_addr;
	cal_fbc.data_bits = fbd_fetch->data_bits;
	cal_fbc.width = ctx->mem_ctrl.img_width;
	cal_fbc.height = ctx->mem_ctrl.img_height;

	if (cur_width % FBD_NR3_Y_PAD_WIDTH != 0 ||
		cur_height % FBD_NR3_Y_PAD_HEIGHT != 0) {
		pad_width = (cur_width + FBD_NR3_Y_PAD_WIDTH - 1) /
			FBD_NR3_Y_PAD_WIDTH * FBD_NR3_Y_PAD_WIDTH;
		pad_height = (cur_height + FBD_NR3_Y_PAD_HEIGHT - 1) /
			FBD_NR3_Y_PAD_HEIGHT * FBD_NR3_Y_PAD_HEIGHT;
	}

	fbd_fetch->y_tiles_num_in_hor = pad_width / FBD_NR3_Y_WIDTH;
	fbd_fetch->y_tiles_num_in_ver = pad_height / FBD_NR3_Y_HEIGHT;
	fbd_fetch->c_tiles_num_in_hor = fbd_fetch->y_tiles_num_in_hor;
	fbd_fetch->c_tiles_num_in_ver = fbd_fetch->y_tiles_num_in_ver / 2;

	if (ctx->blending_cnt % 2 == 1) {
		isp_3dnr_cal_compressed_addr(cur_width, cur_height,
			ctx->buf_info[0]->iova[0], &out_addr);
		fbd_fetch->y_header_addr_init = out_addr.addr1;
		fbd_fetch->y_tile_addr_init_x256 = out_addr.addr1;
		fbd_fetch->c_header_addr_init = out_addr.addr2;
		fbd_fetch->c_tile_addr_init_x256 = out_addr.addr2;
		/*This is for N6pro*/
		cal_fbc.in = ctx->buf_info[0]->iova[0];
		dcam_if_cal_compressed_addr(&cal_fbc);
		fbd_fetch->frame_header_base_addr = fbd_fetch->hw_addr.addr0;
		fbd_fetch->slice_start_header_addr = fbd_fetch->frame_header_base_addr +
			(((unsigned long)fbd_fetch->slice_start_pxl_ypt / ISP_FBD_TILE_HEIGHT) * fbd_fetch->tile_num_pitch +
			(unsigned long)fbd_fetch->slice_start_pxl_xpt / ISP_FBD_TILE_WIDTH) * 16;
	} else {
		isp_3dnr_cal_compressed_addr(cur_width, cur_height,
			ctx->buf_info[1]->iova[0], &out_addr);
		fbd_fetch->y_header_addr_init   = out_addr.addr1;
		fbd_fetch->y_tile_addr_init_x256 = out_addr.addr1;
		fbd_fetch->c_header_addr_init = out_addr.addr2;
		fbd_fetch->c_tile_addr_init_x256 = out_addr.addr2;
		/*This is for N6pro*/
		cal_fbc.in = ctx->buf_info[1]->iova[0];
		dcam_if_cal_compressed_addr(&cal_fbc);
		fbd_fetch->frame_header_base_addr = fbd_fetch->hw_addr.addr0;
		fbd_fetch->slice_start_header_addr = fbd_fetch->frame_header_base_addr +
			(((unsigned long)fbd_fetch->slice_start_pxl_ypt / ISP_FBD_TILE_HEIGHT) * fbd_fetch->tile_num_pitch +
			(unsigned long)fbd_fetch->slice_start_pxl_xpt / ISP_FBD_TILE_WIDTH) * 16;
	}

	fbd_fetch->y_tiles_num_pitch = pad_width / FBD_NR3_Y_WIDTH;
	fbd_fetch->c_tiles_num_pitch = fbd_fetch->y_tiles_num_pitch;

	fbd_fetch->y_pixel_size_in_hor = cur_width;
	fbd_fetch->y_pixel_size_in_ver = cur_height;
	fbd_fetch->c_pixel_size_in_hor = cur_width;
	fbd_fetch->c_pixel_size_in_ver = cur_height / 2;
	fbd_fetch->y_pixel_start_in_hor = 0;
	fbd_fetch->y_pixel_start_in_ver = 0;
	fbd_fetch->c_pixel_start_in_hor = 0;
	fbd_fetch->c_pixel_start_in_ver = 0;

	fbd_fetch->fbdc_cr_ch0123_val0 = 0;
	fbd_fetch->fbdc_cr_ch0123_val1 = 0x1000000;
	fbd_fetch->fbdc_cr_y_val0 = 0;
	fbd_fetch->fbdc_cr_y_val1 = 0xff;
	fbd_fetch->fbdc_cr_uv_val0 = 0;
	fbd_fetch->fbdc_cr_uv_val1 = 0;

	mv_x = ctx->mem_ctrl.mv_x;
	mv_y = ctx->mem_ctrl.mv_y;

	fbd_fetch->y_tiles_num_in_hor =
		(ctx->mem_ctrl.ft_y_width + FBD_NR3_Y_WIDTH - 1) / FBD_NR3_Y_WIDTH;
	fbd_fetch->y_tiles_num_in_ver =
		(ctx->mem_ctrl.ft_y_height + FBD_NR3_Y_HEIGHT - 1) / FBD_NR3_Y_HEIGHT;
	fbd_fetch->c_tiles_num_in_hor =
		(ctx->mem_ctrl.ft_uv_width + FBD_NR3_Y_WIDTH - 1) / FBD_NR3_Y_WIDTH;
	fbd_fetch->c_tiles_num_in_ver =
		(ctx->mem_ctrl.ft_uv_height + FBD_NR3_Y_HEIGHT - 1) / FBD_NR3_Y_HEIGHT;
	fbd_fetch->y_pixel_size_in_hor = ctx->mem_ctrl.ft_y_width;
	fbd_fetch->c_pixel_size_in_hor = ctx->mem_ctrl.ft_uv_width;
	fbd_fetch->y_pixel_size_in_ver = ctx->mem_ctrl.ft_y_height;
	fbd_fetch->c_pixel_size_in_ver = ctx->mem_ctrl.ft_uv_height;

	if (mv_x < 0) {
		if ((mv_x) & 0x1) {
			fbd_fetch->y_pixel_start_in_hor = 0;
			fbd_fetch->c_pixel_start_in_hor = 2;
		} else {
			fbd_fetch->y_pixel_start_in_hor = 0;
			fbd_fetch->c_pixel_start_in_hor = 0;
		}
	} else if (mv_x > 0) {
		if ((mv_x) & 0x1) {
			fbd_fetch->y_tile_addr_init_x256 =
				fbd_fetch->y_tile_addr_init_x256 + mv_x / FBD_NR3_Y_WIDTH;
			fbd_fetch->c_tile_addr_init_x256 =
				fbd_fetch->c_tile_addr_init_x256 + (mv_x - 1) / FBD_NR3_Y_WIDTH;
			fbd_fetch->y_pixel_start_in_hor = mv_x;
			fbd_fetch->c_pixel_start_in_hor = mv_x - 1;
		} else {
			fbd_fetch->y_tile_addr_init_x256
				= fbd_fetch->y_tile_addr_init_x256 + mv_x / FBD_NR3_Y_WIDTH;
			fbd_fetch->c_tile_addr_init_x256
				= fbd_fetch->c_tile_addr_init_x256 + mv_x / FBD_NR3_Y_WIDTH;
			fbd_fetch->y_pixel_start_in_hor = mv_x;
			fbd_fetch->c_pixel_start_in_hor = mv_x;
		}
	}

	if (mv_y < 0) {
		fbd_fetch->y_pixel_start_in_ver = 0;
		fbd_fetch->c_pixel_start_in_ver = 0;
	} else if (mv_y > 0) {
		fbd_fetch->y_pixel_start_in_ver = (mv_y) & 0x1;
		fbd_fetch->c_pixel_start_in_ver = (mv_y / 2) & 0x1;
		fbd_fetch->y_tile_addr_init_x256 =
			((fbd_fetch->y_tile_addr_init_x256 >> 8) +
			fbd_fetch->y_tiles_num_pitch * (mv_y / 2)) << 8;
		fbd_fetch->c_tile_addr_init_x256 =
			((fbd_fetch->c_tile_addr_init_x256 >> 8) +
			fbd_fetch->y_tiles_num_pitch * (mv_y / 4)) << 8;
		fbd_fetch->y_header_addr_init =
			fbd_fetch->y_header_addr_init -
			(fbd_fetch->y_tiles_num_pitch * (mv_y / 2)) / 2;
		fbd_fetch->c_header_addr_init =
			fbd_fetch->c_header_addr_init -
			(fbd_fetch->y_tiles_num_pitch * (mv_y / 4)) / 2;
	}

	pr_debug("3dnr mv_x 0x%x, mv_y 0x%x\n", mv_x, mv_y);

	pr_debug("3dnr fbd y_header_addr_init 0x%x, c_header_addr_init 0x%x\n",
		fbd_fetch->y_header_addr_init, fbd_fetch->c_header_addr_init);

	pr_debug("3dnr fbd y_tile_addr_init_x256 0x%x, c_tile_addr_init_x256 0x%x\n",
		fbd_fetch->y_tile_addr_init_x256, fbd_fetch->c_tile_addr_init_x256);

	pr_debug("3dnr fbd y_pixel_start_in_hor 0x%x, y_pixel_start_in_ver 0x%x\n",
		fbd_fetch->y_pixel_start_in_hor, fbd_fetch->y_pixel_start_in_ver);

	pr_debug("3dnr fbd c_pixel_start_in_hor 0x%x, c_pixel_start_in_ver 0x%x\n",
		fbd_fetch->c_pixel_start_in_hor, fbd_fetch->c_pixel_start_in_ver);

	return ret;
}

static int isp3dnr_fbc_store_config_gen(struct isp_3dnr_ctx_desc *ctx)
{
	int ret = 0;
	uint32_t tile_hor = 0, tile_ver = 0;
	uint32_t cur_width = 0, cur_height = 0;
	uint32_t pad_width = 0, pad_height = 0;
	struct compressed_addr out_addr = {0};
	struct isp_3dnr_fbc_store *fbc_store = NULL;
	struct dcam_compress_cal_para cal_fbc = {0};

	if (!ctx) {
		pr_err("invalid parameter, fbc store\n");
		return -EINVAL;
	}

	cur_width = ctx->mem_ctrl.img_width;
	cur_height = ctx->mem_ctrl.img_height;
	fbc_store = &ctx->nr3_fbc_store;
	fbc_store->size_in_hor = cur_width;
	fbc_store->size_in_ver = cur_height;
	fbc_store->color_format = 4;
	pad_width = cur_width;
	pad_height = cur_height;

	/*This is for N6pro*/
	cal_fbc.fmt = 5;
	cal_fbc.height = cur_height;
	cal_fbc.width = cur_width;
	cal_fbc.fbc_info = &ctx->fbc_info;
	cal_fbc.out = &fbc_store->hw_addr;
	fbc_store->c_nearly_full_level = 0x2;
	fbc_store->y_nearly_full_level = 0x2;
	cal_fbc.data_bits = ctx->mem_ctrl.yuv_8bits_flag ? 8 : 10;

	if (cal_fbc.data_bits == 10)
		fbc_store->afbc_mode = 7;
	else
		fbc_store->afbc_mode = 5;

	if ((cur_width % FBC_NR3_Y_PAD_WIDTH) != 0 ||
		(cur_height % FBC_NR3_Y_PAD_HEIGHT) != 0) {
		pad_width = (cur_width + FBC_NR3_Y_PAD_WIDTH - 1) /
			FBC_NR3_Y_PAD_WIDTH * FBC_NR3_Y_PAD_WIDTH;
		pad_height = (cur_height + FBC_NR3_Y_PAD_HEIGHT - 1) /
			FBC_NR3_Y_PAD_HEIGHT * FBC_NR3_Y_PAD_HEIGHT;
	}

	tile_hor = pad_width / FBC_NR3_Y_WIDTH;
	tile_ver = pad_height / FBC_NR3_Y_HEIGHT;

	fbc_store->tile_number = tile_hor * tile_ver + tile_hor * tile_ver / 2;
	fbc_store->tile_number_pitch = pad_width / FBD_NR3_Y_WIDTH;
	fbc_store->fbc_constant_yuv = 0xff0000ff;
	fbc_store->later_bits = 15;
	fbc_store->slice_mode_en = 0;
	fbc_store->bypass = 0;

	if (ctx->blending_cnt % 2 != 1) {
		isp_3dnr_cal_compressed_addr(cur_width, cur_height,
			ctx->buf_info[0]->iova[0], &out_addr);
		fbc_store->y_header_addr_init = out_addr.addr1;
		fbc_store->y_tile_addr_init_x256 = out_addr.addr1;
		fbc_store->c_header_addr_init = out_addr.addr2;
		fbc_store->c_tile_addr_init_x256 = out_addr.addr2;
		/*This is for N6pro*/
		cal_fbc.in = ctx->buf_info[0]->iova[0];
		dcam_if_cal_compressed_addr(&cal_fbc);
		fbc_store->tile_number_pitch = cal_fbc.fbc_info->tile_col;
		fbc_store->slice_header_base_addr = fbc_store->hw_addr.addr0;
		fbc_store->slice_payload_base_addr = fbc_store->hw_addr.addr1;
		fbc_store->slice_payload_offset_addr_init = fbc_store->hw_addr.addr1 - fbc_store->hw_addr.addr0;
	} else {
		isp_3dnr_cal_compressed_addr(cur_width, cur_height,
			ctx->buf_info[1]->iova[0], &out_addr);
		fbc_store->y_header_addr_init = out_addr.addr1;
		fbc_store->y_tile_addr_init_x256 = out_addr.addr1;
		fbc_store->c_header_addr_init = out_addr.addr2;
		fbc_store->c_tile_addr_init_x256 = out_addr.addr2;
		/*This is for N6pro*/
		cal_fbc.in = ctx->buf_info[1]->iova[0];
		dcam_if_cal_compressed_addr(&cal_fbc);
		fbc_store->tile_number_pitch = cal_fbc.fbc_info->tile_col;
		fbc_store->slice_header_base_addr = fbc_store->hw_addr.addr0;
		fbc_store->slice_payload_base_addr = fbc_store->hw_addr.addr1;
		fbc_store->slice_payload_offset_addr_init = fbc_store->hw_addr.addr1 - fbc_store->hw_addr.addr0;
	}

	pr_debug("3dnr fbc tile_number %d tile number_pitch %d\n",
		fbc_store->tile_number, fbc_store->tile_number_pitch);

	return ret;
}

static int isp3dnr_config_gen(struct isp_3dnr_ctx_desc *ctx)
{
	int ret = 0;

	if (!ctx) {
		pr_err("fail to get valid 3ndr context\n");
		return -EINVAL;
	}

	ret = isp3dnr_memctrl_config_gen(ctx);
	if (ret) {
		pr_err("fail to generate mem ctrl configuration\n");
		return ret;
	}

	ret = isp3dnr_store_config_gen(ctx);
	if (ret) {
		pr_err("fail to generate store configuration\n");
		return ret;
	}

	if (!ctx->nr3_fbc_store.bypass) {
		ret = isp3dnr_fbc_store_config_gen(ctx);
		if (ret) {
			pr_err("fail to generate fbc store configuration\n");
			return ret;
		}
	}

	if (!ctx->nr3_fbd_fetch.bypass) {
		ret = isp3dnr_fbd_fetch_config_gen(ctx);
		if (ret) {
			pr_err("fail to generate fbd fetch configuration\n");
			return ret;
		}
	}

	ret = isp3dnr_crop_config_gen(ctx);
	if (ret) {
		pr_err("fail to generate crop configuration\n");
		return ret;
	}

	ctx->blending_cnt++;

	return ret;
}

static int isp3dnr_conversion_mv(struct isp_3dnr_ctx_desc *nr3_ctx)
{
	int ret = 0;
	struct alg_nr3_mv_cfg cfg_in = {0};

	if (!nr3_ctx) {
		pr_err("fail to get valid nr3_ctx\n");
		return -EINVAL;
	}

	cfg_in.mv_version = nr3_ctx->nr3_mv_version;
	cfg_in.mv_x = nr3_ctx->mvinfo->mv_x;
	cfg_in.mv_y = nr3_ctx->mvinfo->mv_y;
	cfg_in.mode_projection = nr3_ctx->mvinfo->project_mode;
	cfg_in.sub_me_bypass = nr3_ctx->mvinfo->sub_me_bypass;
	cfg_in.iw = nr3_ctx->mvinfo->src_width;
	cfg_in.ow = nr3_ctx->width;
	cfg_in.ih = nr3_ctx->mvinfo->src_height;
	cfg_in.oh = nr3_ctx->height;
	nr3_mv_convert_ver(&cfg_in);
	nr3_ctx->mv.mv_x = cfg_in.o_mv_x;
	nr3_ctx->mv.mv_y = cfg_in.o_mv_y;

	pr_debug("3DNR conv_mv in_x =%d, in_y =%d, out_x=%d, out_y=%d\n",
		nr3_ctx->mvinfo->mv_x, nr3_ctx->mvinfo->mv_y,
		nr3_ctx->mv.mv_x, nr3_ctx->mv.mv_y);

	return ret;
}

static int isp3dnr_pipe_proc(void *handle, void *param, uint32_t mode)
{
	int ret = 0;
	uint32_t curblend_bypass = 0;
	struct isp_3dnr_ctx_desc *nr3_ctx = NULL;
	struct nr3_me_data *nr3_me = NULL;

	if (!handle || !param) {
		pr_debug("fail to get valid input ptr\n");
		return -EFAULT;
	}

	nr3_ctx = (struct isp_3dnr_ctx_desc *)handle;
	nr3_me = (struct nr3_me_data *)param;

	if ((g_isp_bypass[nr3_ctx->ctx_id] >> _EISP_NR3) & 1)
		mode = NR3_FUNC_OFF;

	switch (mode) {
	case MODE_3DNR_PRE:
		nr3_ctx->type = NR3_FUNC_PRE;

		if (nr3_me->valid) {
			nr3_ctx->mv.mv_x = nr3_me->mv_x;
			nr3_ctx->mv.mv_y = nr3_me->mv_y;
			nr3_ctx->mvinfo = nr3_me;
			isp3dnr_conversion_mv(nr3_ctx);
		} else {
			pr_err("fail to get binning path mv, set default 0\n");
			nr3_ctx->mv.mv_x = 0;
			nr3_ctx->mv.mv_y = 0;
			nr3_ctx->mvinfo = NULL;
		}

		/*determine wheather the first frame after hdr*/
		if (nr3_ctx->nr3_mv_version == ALG_NR3_MV_VER_0)
			curblend_bypass = nr3_ctx->isp_block->nr3_info_base.blend.bypass;
		else
			curblend_bypass = nr3_ctx->isp_block->nr3_info_base_v1.blend.bypass;
		if (nr3_ctx->preblend_bypass == 1 && curblend_bypass == 0) {
			nr3_ctx->blending_cnt = 0;
			nr3_ctx->mv.mv_x = 0;
			nr3_ctx->mv.mv_y = 0;
			pr_debug("the first frame after hdr, set 0\n");
		}
		nr3_ctx->preblend_bypass = curblend_bypass;

		isp3dnr_config_gen(nr3_ctx);
		isp_3dnr_config_param(nr3_ctx);

		pr_debug("3DNR_PRE path mv[%d, %d]!\n.",
			nr3_ctx->mv.mv_x, nr3_ctx->mv.mv_y);
		break;
	case MODE_3DNR_CAP:
		nr3_ctx->type = NR3_FUNC_CAP;
		if (nr3_ctx->mode == MODE_3DNR_OFF)
			return 0;

		if (nr3_me->valid) {
			nr3_ctx->mv.mv_x = nr3_me->mv_x;
			nr3_ctx->mv.mv_y = nr3_me->mv_y;
			nr3_ctx->mvinfo = nr3_me;
			if (nr3_ctx->mvinfo->src_width != nr3_ctx->width ||
				nr3_ctx->mvinfo->src_height != nr3_ctx->height) {
				isp3dnr_conversion_mv(nr3_ctx);
			}
		} else {
			pr_err("fail to get full path mv, set default 0\n");
			nr3_ctx->mv.mv_x = 0;
			nr3_ctx->mv.mv_y = 0;
		}

		isp3dnr_config_gen(nr3_ctx);
		isp_3dnr_config_param(nr3_ctx);

		pr_debug("3DNR_CAP path mv[%d, %d]!\n.",
			nr3_ctx->mv.mv_x, nr3_ctx->mv.mv_y);
		break;
	default:
		/* default: bypass 3dnr */
		nr3_ctx->type = NR3_FUNC_OFF;
		isp_3dnr_bypass_config(nr3_ctx->ctx_id);
		pr_debug("ispcore_offline_frame_start default\n");
		break;
	}

	return ret;
}

static int isp3dnr_cfg_param(void *handle,
		enum isp_3dnr_cfg_cmd cmd, void *param)
{
	int ret = 0;
	uint32_t i = 0;
	uint32_t nr3_compress_eb = 0;
	struct img_trim *crop = NULL;
	struct camera_frame *pframe = NULL;
	struct isp_3dnr_ctx_desc *nr3_ctx = NULL;
	struct isp_hw_fetch_info *fetch_info = NULL;

	if (!handle || !param) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	nr3_ctx = (struct isp_3dnr_ctx_desc *)handle;
	switch (cmd) {
	case ISP_3DNR_CFG_BUF:
		pframe = (struct camera_frame *)param;
		ret = cam_buf_iommu_map(&pframe->buf, CAM_IOMMUDEV_ISP);
		if (ret) {
			pr_err("fail to map isp 3dnr iommu buf.\n");
			ret = -EINVAL;
			goto exit;
		}
		for (i = 0; i < ISP_NR3_BUF_NUM; i++) {
			if (nr3_ctx->buf_info[i] == NULL) {
				nr3_ctx->buf_info[i] = &pframe->buf;
				pr_debug("3DNR CFGB[%d][0x%p] = 0x%lx\n",
					i, pframe, nr3_ctx->buf_info[i]->iova[0]);
				break;
			} else {
				pr_debug("3DNR CFGB[%d][0x%p][0x%p] failed\n",
					i, pframe, nr3_ctx->buf_info[i]);
			}
		}
		if (i == 2) {
			pr_err("fail to set isp nr3 buffers.\n");
			cam_buf_iommu_unmap(&pframe->buf);
			goto exit;
		}
		break;
	case ISP_3DNR_CFG_MODE:
		nr3_ctx->mode = *(uint32_t *)param;
		pr_debug("3DNR mode %d\n", nr3_ctx->mode);
		break;
	case ISP_3DNR_CFG_BLEND_CNT:
		nr3_ctx->blending_cnt = *(uint32_t *)param;
		pr_debug("3DNR blending cnt %d\n", nr3_ctx->blending_cnt);
		break;
	case ISP_3DNR_CFG_FBC_FBD_INFO:
		nr3_compress_eb = *(uint32_t *)param;
		if (nr3_compress_eb) {
			nr3_ctx->mem_ctrl.bypass = 1;
			nr3_ctx->nr3_store.st_bypass = 1;
			nr3_ctx->nr3_fbd_fetch.bypass = 0;
			nr3_ctx->nr3_fbc_store.bypass = 0;
		} else {
			nr3_ctx->mem_ctrl.bypass = 0;
			nr3_ctx->nr3_store.st_bypass = 0;
			nr3_ctx->nr3_fbc_store.bypass = 1;
			nr3_ctx->nr3_fbd_fetch.bypass = 1;
		}
		break;
	case ISP_3DNR_CFG_SIZE_INFO:
		crop = (struct img_trim *)param;
		/* Check Zoom or not */
		if ((crop->size_x != nr3_ctx->width) || (crop->size_y != nr3_ctx->height)) {
			nr3_ctx->blending_cnt = 0;
			pr_debug("3DNR %d size changed, reset blend cnt\n", nr3_ctx->ctx_id);
		}
		nr3_ctx->width = crop->size_x;
		nr3_ctx->height = crop->size_y;
		break;
	case ISP_3DNR_CFG_BLEND_INFO:
		nr3_ctx->isp_block = (struct isp_k_block *)param;
		break;
	case ISP_3DNR_CFG_MEMCTL_STORE_INFO:
		fetch_info = (struct isp_hw_fetch_info *)param;
		nr3_ctx->mem_ctrl.yuv_8bits_flag = (fetch_info->data_bits == 8) ? 1 : 0;
		nr3_ctx->nr3_sec_mode = fetch_info->sec_mode;
		nr3_ctx->mem_ctrl.ft_pitch = isp3dnr_calc_img_pitch(fetch_info->fetch_fmt, nr3_ctx->width);
		nr3_ctx->nr3_store.st_pitch = nr3_ctx->mem_ctrl.ft_pitch;
		nr3_ctx->nr3_store.y_pitch_mem = nr3_ctx->mem_ctrl.ft_pitch;
		nr3_ctx->nr3_store.u_pitch_mem = nr3_ctx->mem_ctrl.ft_pitch;
		nr3_ctx->nr3_store.color_format = fetch_info->fetch_fmt;
		nr3_ctx->nr3_store.mipi_en = 0;
		nr3_ctx->nr3_store.data_10b = (fetch_info->data_bits == 8) ? 0 : 1;
		break;
	case ISP_3DNR_CFG_MV_VERSION:
		nr3_ctx->nr3_mv_version = *(uint32_t *)param;
		break;
	default:
		pr_err("fail to get known cmd: %d\n", cmd);
		ret = -EFAULT;
		break;
	}

exit:
	return ret;
}

void *isp_3dnr_ctx_get(uint32_t idx)
{
	struct isp_3dnr_ctx_desc *nr3_ctx = NULL;

	nr3_ctx = vzalloc(sizeof(struct isp_3dnr_ctx_desc));
	if (!nr3_ctx)
		return NULL;

	nr3_ctx->ctx_id = idx;
	nr3_ctx->ops.cfg_param = isp3dnr_cfg_param;
	nr3_ctx->ops.pipe_proc = isp3dnr_pipe_proc;

	return nr3_ctx;
}

void isp_3dnr_ctx_put(void *nr3_handle)
{
	struct isp_3dnr_ctx_desc *nr3_ctx = NULL;
	struct camera_buf *buf_info = NULL;
	uint32_t i = 0;

	if (!nr3_handle) {
		pr_err("fail to get valid nr3 handle\n");
		return;
	}

	nr3_ctx = (struct isp_3dnr_ctx_desc *)nr3_handle;
	for (i = 0; i < ISP_NR3_BUF_NUM; i++) {
		buf_info = nr3_ctx->buf_info[i];
		if (buf_info && buf_info->mapping_state & CAM_BUF_MAPPING_DEV) {
			cam_buf_iommu_unmap(buf_info);
			buf_info = NULL;
		}
	}

	if (nr3_ctx)
		vfree(nr3_ctx);
	nr3_ctx = NULL;
}
