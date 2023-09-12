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

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/spinlock.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <sprd_mm.h>
#include "isp_reg.h"
#include "isp_int.h"
#include "isp_core.h"
#include "cam_scaler.h"
#include "cam_scaler_ex.h"
#include "isp_path.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_DRV: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

uint32_t s_isp_irq_no[ISP_LOGICAL_COUNT];
unsigned long s_isp_regbase[ISP_MAX_COUNT];
unsigned long isp_phys_base[ISP_MAX_COUNT];
unsigned long s_isp_mmubase;

static int ispdrv_path_scaler_get(struct isp_path_uinfo *in_ptr,
	struct isp_hw_path_scaler *path)
{
	int ret = 0;
	uint32_t is_yuv422 = 0, scale2yuv420 = 0;
	struct yuv_scaler_info *scaler = NULL;

	if (!in_ptr || !path) {
		pr_err("fail to get valid input ptr %p, %p\n", in_ptr, path);
		return -EFAULT;
	}

	scaler = &path->scaler;
	if (in_ptr->out_fmt == IMG_PIX_FMT_UYVY)
		path->uv_sync_v = 1;
	else
		path->uv_sync_v = 0;
	if (in_ptr->out_fmt == IMG_PIX_FMT_FULL_RGB)
		path->path_sel = 2;
	else
		path->path_sel = 0;
	path->frm_deci = 0;
	path->dst = in_ptr->dst;
	path->out_trim.start_x = 0;
	path->out_trim.start_y = 0;
	path->out_trim.size_x = in_ptr->dst.w;
	path->out_trim.size_y = in_ptr->dst.h;
	path->regular_info.regular_mode = in_ptr->regular_mode;
	ret = isp_path_scaler_param_calc(&path->in_trim, &path->dst,
		&path->scaler, &path->deci);
	if (ret) {
		pr_err("fail to calc scaler param.\n");
		return ret;
	}

	if (in_ptr->out_fmt == IMG_PIX_FMT_YUV422P)
		is_yuv422 = 1;

	if (((scaler->scaler_ver_factor_in == scaler->scaler_ver_factor_out)
		&& (scaler->scaler_factor_in == scaler->scaler_factor_out)
		&& (is_yuv422 || in_ptr->scaler_bypass_ctrl))
		|| in_ptr->out_fmt == IMG_PIX_FMT_FULL_RGB) {
		scaler->scaler_bypass = 1;
	} else {
		scaler->scaler_bypass = 0;

		if (in_ptr->scaler_coeff_ex) {
			/*0:yuv422 to 422 ;1:yuv422 to 420 2:yuv420 to 420*/
			scaler->work_mode = 2;
			ret = cam_scaler_coeff_calc_ex(scaler);
		}  else {
			scale2yuv420 = is_yuv422 ? 0 : 1;
			ret = cam_scaler_coeff_calc(scaler, scale2yuv420);
		}

		if (ret) {
			pr_err("fail to calc scaler coeff.\n");
			return ret;
		}
	}
	scaler->odata_mode = is_yuv422 ? 0x00 : 0x01;

	return ret;
}

enum isp_fetch_format isp_drv_fetch_format_get(struct isp_uinfo *pipe_src)
{
	enum isp_fetch_format format = ISP_FETCH_FORMAT_MAX;
	switch (pipe_src->in_fmt) {
	case IMG_PIX_FMT_GREY:
		format = ISP_FETCH_CSI2_RAW10;
		if (pipe_src->pack_bits == ISP_RAW_HALF14 || pipe_src->pack_bits == ISP_RAW_HALF10)
			format = ISP_FETCH_RAW10;
		break;
	case IMG_PIX_FMT_UYVY:
		format = ISP_FETCH_UYVY;
		break;
	case IMG_PIX_FMT_YUV422P:
		format = ISP_FETCH_YUV422_3FRAME;
		break;
	case IMG_PIX_FMT_NV12:
		if (pipe_src->data_in_bits == DCAM_STORE_10_BIT) {
			format = ISP_FETCH_YUV420_2FRAME_10;
			if (pipe_src->is_pack)
				format = ISP_FETCH_YUV420_2FRAME_MIPI;
		} else if (pipe_src->data_in_bits == DCAM_STORE_8_BIT)
			format = ISP_FETCH_YUV420_2FRAME;
		else
			pr_err("fail to get support data_bits:%d\n", pipe_src->data_in_bits);
		break;
	case IMG_PIX_FMT_NV21:
		if (pipe_src->data_in_bits == DCAM_STORE_10_BIT) {
			format = ISP_FETCH_YVU420_2FRAME_10;
			if (pipe_src->is_pack)
				format = ISP_FETCH_YVU420_2FRAME_MIPI;
		} else if (pipe_src->data_in_bits == DCAM_STORE_8_BIT)
			format = ISP_FETCH_YVU420_2FRAME;
		else
			pr_err("fail to get support data_bits:%d\n", pipe_src->data_in_bits);
		break;
	case IMG_PIX_FMT_FULL_RGB:
		format = ISP_FETCH_FULL_RGB10;
		break;
	default:
		format = ISP_FETCH_FORMAT_MAX;
		pr_err("fail to get support format 0x%x\n", pipe_src->in_fmt);
		break;
	}
	return format;
}

enum isp_fetch_format isp_drv_fetch_pyr_format_get(struct isp_uinfo *pipe_src)
{
	enum isp_fetch_format format = ISP_FETCH_FORMAT_MAX;
	switch (pipe_src->in_fmt) {
	case IMG_PIX_FMT_GREY:
		format = ISP_FETCH_CSI2_RAW10;
		if (pipe_src->pyr_data_in_bits == ISP_RAW_HALF14 || pipe_src->pyr_data_in_bits == ISP_RAW_HALF10)
			format = ISP_FETCH_RAW10;
		break;
	case IMG_PIX_FMT_UYVY:
		format = ISP_FETCH_UYVY;
		break;
	case IMG_PIX_FMT_YUV422P:
		format = ISP_FETCH_YUV422_3FRAME;
		break;
	case IMG_PIX_FMT_NV12:
		if (pipe_src->pyr_data_in_bits == DCAM_STORE_10_BIT) {
			format = ISP_FETCH_YUV420_2FRAME_10;
			if (pipe_src->pyr_is_pack)
				format = ISP_FETCH_YUV420_2FRAME_MIPI;
		} else if (pipe_src->pyr_data_in_bits == DCAM_STORE_8_BIT)
			format = ISP_FETCH_YUV420_2FRAME;
		else
			pr_err("fail to get support data_bits:%d\n", pipe_src->pyr_data_in_bits);
		break;
	case IMG_PIX_FMT_NV21:
		if (pipe_src->pyr_data_in_bits == DCAM_STORE_10_BIT) {
			format = ISP_FETCH_YVU420_2FRAME_10;
			if (pipe_src->pyr_is_pack)
				format = ISP_FETCH_YVU420_2FRAME_MIPI;
		} else if (pipe_src->pyr_data_in_bits == DCAM_STORE_8_BIT)
			format = ISP_FETCH_YVU420_2FRAME;
		else
			pr_err("fail to get support data_bits:%d\n", pipe_src->pyr_data_in_bits);
		break;
	case IMG_PIX_FMT_FULL_RGB:
		format = ISP_FETCH_FULL_RGB10;
		break;
	default:
		format = ISP_FETCH_FORMAT_MAX;
		pr_err("fail to get support format 0x%x\n", pipe_src->in_fmt);
		break;
	}
	return format;
}

static int ispdrv_fetch_normal_get(void *cfg_in, void *cfg_out,
		struct camera_frame *frame)
{
	int ret = 0;
	uint32_t trim_offset[3] = { 0 };
	struct img_size *src = NULL;
	struct img_trim *intrim = NULL;
	struct isp_hw_fetch_info *fetch = NULL;
	struct isp_uinfo *pipe_src = NULL;
	uint32_t mipi_word_num_start[16] = {
		0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5};
	uint32_t mipi_word_num_end[16] = {
		0, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5};
	if (!cfg_in || !cfg_out || !frame) {
		pr_err("fail to get valid input ptr, %p, %p\n", cfg_in, cfg_out);
		return -EFAULT;
	}

	pipe_src = (struct isp_uinfo *)cfg_in;
	fetch = (struct isp_hw_fetch_info *)cfg_out;

	src = &pipe_src->src;
	intrim = &pipe_src->crop;
	fetch->src = *src;
	fetch->in_trim = *intrim;
	fetch->fetch_fmt = isp_drv_fetch_format_get(pipe_src);
	fetch->fetch_pyr_fmt = isp_drv_fetch_pyr_format_get(pipe_src);
	fetch->is_pack = pipe_src->is_pack;
	fetch->data_bits = pipe_src->data_in_bits;
	fetch->bayer_pattern = pipe_src->bayer_pattern;
	if (pipe_src->in_fmt == IMG_PIX_FMT_GREY)
		fetch->dispatch_color = 0;
	else if (pipe_src->in_fmt == IMG_PIX_FMT_FULL_RGB)
		fetch->dispatch_color = 1;
	else
		fetch->dispatch_color = 2;
	fetch->fetch_path_sel = pipe_src->fetch_path_sel;
	fetch->pack_bits = pipe_src->pack_bits;
	fetch->addr.addr_ch0 = frame->buf.iova[0];

	switch (fetch->fetch_fmt) {
	case ISP_FETCH_YUV422_3FRAME:
		fetch->pitch.pitch_ch0 = src->w;
		fetch->pitch.pitch_ch1 = src->w / 2;
		fetch->pitch.pitch_ch2 = src->w / 2;
		trim_offset[0] = intrim->start_y * fetch->pitch.pitch_ch0 + intrim->start_x;
		trim_offset[1] = intrim->start_y * fetch->pitch.pitch_ch1 + intrim->start_x / 2;
		trim_offset[2] = intrim->start_y * fetch->pitch.pitch_ch2 + intrim->start_x / 2;
		fetch->addr.addr_ch1 = fetch->addr.addr_ch0 + fetch->pitch.pitch_ch0 * fetch->src.h;
		fetch->addr.addr_ch2 = fetch->addr.addr_ch1 + fetch->pitch.pitch_ch1 * fetch->src.h;
		break;
	case ISP_FETCH_YUYV:
	case ISP_FETCH_UYVY:
	case ISP_FETCH_YVYU:
	case ISP_FETCH_VYUY:
		fetch->pitch.pitch_ch0 = src->w * 2;
		trim_offset[0] = intrim->start_y * fetch->pitch.pitch_ch0 + intrim->start_x * 2;
		break;
	case ISP_FETCH_RAW10:
		fetch->pitch.pitch_ch0 = cal_sprd_raw_pitch(src->w, pipe_src->pack_bits);
		trim_offset[0] = intrim->start_y * fetch->pitch.pitch_ch0 + intrim->start_x * 2;
		break;
	case ISP_FETCH_YUV422_2FRAME:
	case ISP_FETCH_YVU422_2FRAME:
		fetch->pitch.pitch_ch0 = src->w;
		fetch->pitch.pitch_ch1 = src->w;
		trim_offset[0] = intrim->start_y * fetch->pitch.pitch_ch0 + intrim->start_x;
		trim_offset[1] = intrim->start_y * fetch->pitch.pitch_ch1 + intrim->start_x;
		fetch->addr.addr_ch1 = fetch->addr.addr_ch0 + fetch->pitch.pitch_ch0 * fetch->src.h;
		break;
	case ISP_FETCH_YUV420_2FRAME:
	case ISP_FETCH_YVU420_2FRAME:
		fetch->pitch.pitch_ch0 = src->w;
		fetch->pitch.pitch_ch1 = src->w;
		trim_offset[0] = intrim->start_y * fetch->pitch.pitch_ch0 + intrim->start_x;
		trim_offset[1] = intrim->start_y * fetch->pitch.pitch_ch1 / 2 + intrim->start_x;
		fetch->addr.addr_ch1 = fetch->addr.addr_ch0 + fetch->pitch.pitch_ch0 * fetch->src.h;
		pr_debug("y_addr: %x, pitch:: %x\n", fetch->addr.addr_ch0, fetch->pitch.pitch_ch0);
		break;
	case ISP_FETCH_YUV420_2FRAME_10:
	case ISP_FETCH_YVU420_2FRAME_10:
		fetch->pitch.pitch_ch0 = (src->w * 16 + 127) / 128 * 128 / 8;
		fetch->pitch.pitch_ch1 = (src->w * 16 + 127) / 128 * 128 / 8;
		trim_offset[0] = intrim->start_y * fetch->pitch.pitch_ch0 + intrim->start_x * 2;
		trim_offset[1] = intrim->start_y * fetch->pitch.pitch_ch1 / 2 + intrim->start_x * 2;
		break;
	case ISP_FETCH_YUV420_2FRAME_MIPI:
	case ISP_FETCH_YVU420_2FRAME_MIPI:
	{
		uint32_t start_col = intrim->start_x;
		uint32_t start_row = intrim->start_y;
		uint32_t end_col =  intrim->start_x + intrim->size_x - 1;
		fetch->pitch.pitch_ch0 = (src->w * 10 + 127) / 128 * 128 / 8;
		fetch->pitch.pitch_ch1 = (src->w * 10 + 127) / 128 * 128 / 8;
		fetch->mipi_byte_rel_pos = intrim->start_x & 0xf;
		fetch->mipi_word_num = ((end_col + 1) >> 4) * 5
			+ mipi_word_num_end[(end_col + 1) & 0xF]
			- ((start_col + 1) >> 4) * 5
			- mipi_word_num_start[(start_col + 1) & 0xF] + 1;
		fetch->mipi_byte_rel_pos_uv = fetch->mipi_byte_rel_pos;
		fetch->mipi_word_num_uv = fetch->mipi_word_num;
		trim_offset[0] = start_row * fetch->pitch.pitch_ch0 + (start_col >> 2) * 5 +
			(start_col & 0x3);
		trim_offset[1] = (start_row >> 1) * fetch->pitch.pitch_ch1 + (start_col >> 2) * 5 +
			(start_col & 0x3);
		fetch->addr.addr_ch1 = fetch->addr.addr_ch0 + fetch->pitch.pitch_ch0 * fetch->src.h;
		break;
	}
	case ISP_FETCH_FULL_RGB10:
		fetch->pitch.pitch_ch0 = src->w * 8;
		trim_offset[0] = intrim->start_y * fetch->pitch.pitch_ch0 + intrim->start_x * 8;
		break;
	case ISP_FETCH_CSI2_RAW10:
	{
		uint32_t mipi_byte_info = 0;
		uint32_t mipi_word_info = 0;
		uint32_t start_col = intrim->start_x;
		uint32_t start_row = intrim->start_y;
		uint32_t end_col =  intrim->start_x + intrim->size_x - 1;
		mipi_byte_info = start_col & 0xF;
		mipi_word_info = ((end_col + 1) >> 4) * 5
			+ mipi_word_num_end[(end_col + 1) & 0xF]
			- ((start_col + 1) >> 4) * 5
			- mipi_word_num_start[(start_col + 1) & 0xF] + 1;
		fetch->mipi_byte_rel_pos = mipi_byte_info;
		fetch->mipi_word_num = mipi_word_info;
		fetch->pitch.pitch_ch0 = cal_sprd_raw_pitch(src->w, 0);
		/* same as slice starts */
		trim_offset[0] = start_row * fetch->pitch.pitch_ch0 + (start_col >> 2) * 5 + (start_col & 0x3);
		if (!pipe_src->fetch_path_sel)
			pr_debug("fetch pitch %d, offset %ld, rel_pos %d, wordn %d\n",
				 fetch->pitch.pitch_ch0, trim_offset[0],
				 mipi_byte_info, mipi_word_info);
		break;
	}
	default:
		pr_err("fail to get fetch format: %d\n", fetch->fetch_fmt);
		break;
	}

	fetch->addr_hw.addr_ch0 = fetch->addr.addr_ch0 + trim_offset[0];
	fetch->addr_hw.addr_ch1 = fetch->addr.addr_ch1 + trim_offset[1];
	fetch->addr_hw.addr_ch2 = fetch->addr.addr_ch2 + trim_offset[2];
	pr_debug("fetch fmt %d, y_addr: %x, u_addr: %x\n", fetch->fetch_fmt, fetch->addr_hw.addr_ch0, fetch->addr_hw.addr_ch1);

	return ret;
}

static enum isp_store_format ispdrv_afbc_store_format_get(uint32_t forcc)
{
	enum isp_store_format format = ISP_STORE_FORMAT_MAX;

	switch (forcc) {
	case IMG_PIX_FMT_NV12:
		format = ISP_STORE_YUV420_2FRAME;
		break;
	case IMG_PIX_FMT_NV21:
		format = ISP_STORE_YVU420_2FRAME;
		break;
	default:
		format = ISP_STORE_FORMAT_MAX;
		pr_err("fail to get support afbc format 0x%x\n", forcc);
		break;
	}
	return format;
}

static int ispdrv_fbd_raw_get(void *cfg_in, void *cfg_out,
		struct camera_frame *frame)
{
	int32_t tile_col = 0, tile_row = 0;
	int32_t crop_start_x = 0, crop_start_y = 0,
		crop_width = 0, crop_height = 0;
	int32_t left_offset_tiles_num = 0,
		up_offset_tiles_num = 0;
	int32_t img_width = 0;
	int32_t end_x = 0, end_y = 0,
		left_tiles_num = 0, right_tiles_num = 0,
		middle_tiles_num = 0, left_size,right_size = 0;
	int32_t up_tiles_num = 0, down_tiles_num = 0,
		vertical_middle_tiles_num = 0,
		up_size = 0, down_size = 0;
	int32_t tiles_num_pitch = 0;
	struct isp_uinfo *pipe_src = NULL;
	struct isp_fbd_raw_info *fbd_raw = NULL;
	struct dcam_compress_cal_para cal_fbc;

	if (!cfg_in || !cfg_out || !frame) {
		pr_err("fail to get valid input ptr, %p, %p\n", cfg_in, cfg_out);
		return -EFAULT;
	}

	pipe_src = (struct isp_uinfo *)cfg_in;
	fbd_raw = (struct isp_fbd_raw_info *)cfg_out;
	img_width = pipe_src->src.w;

	if (pipe_src->fetch_path_sel == 0 || pipe_src->fetch_path_sel == 2)
		return 0;

	/*Bug 1024606 sw workaround
	fetch fbd crop isp timeout when crop.start_y % 4 == 2 */
	if((pipe_src->fetch_path_sel == 1) && (pipe_src->crop.start_y % 4 == 2)) {
		pipe_src->crop.start_y -= 2;
		pipe_src->crop.size_y += 2;
		pr_info("fbd start_y: %d, size_y: %d",
			pipe_src->crop.start_y, pipe_src->crop.size_y);
	}

	fbd_raw->width = pipe_src->src.w;
	fbd_raw->height = pipe_src->src.h;
	fbd_raw->size = pipe_src->src;
	fbd_raw->trim = pipe_src->crop;
	fbd_raw->fetch_fbd_bypass = 0;
	fbd_raw->fetch_fbd_4bit_bypass = pipe_src->fetch_fbd_4bit_bypass;
	pr_debug("fbd raw info: width %d, height %d, trim: x %d, y %d, w %d, h %d\n",
		fbd_raw->size.w, fbd_raw->size.h,
		fbd_raw->trim.start_x, fbd_raw->trim.start_y,
		fbd_raw->trim.size_x, fbd_raw->trim.size_y);

	tile_col = (fbd_raw->width + ISP_FBD_TILE_WIDTH - 1) / ISP_FBD_TILE_WIDTH;
	tile_col = (tile_col + 2 - 1) / 2 * 2;
	tile_row =(fbd_raw->height + ISP_FBD_TILE_HEIGHT - 1)/ ISP_FBD_TILE_HEIGHT;

	fbd_raw->tiles_num_pitch = tile_col;
	fbd_raw->low_bit_pitch = fbd_raw->tiles_num_pitch * ISP_FBD_TILE_WIDTH / 2;
	if(fbd_raw->fetch_fbd_4bit_bypass == 0)
		fbd_raw->low_4bit_pitch = fbd_raw->tiles_num_pitch * ISP_FBD_TILE_WIDTH;

	if (fbd_raw->trim.size_x != fbd_raw->size.w ||
		fbd_raw->trim.size_y != fbd_raw->size.h) {
		tiles_num_pitch = fbd_raw->tiles_num_pitch;
		fbd_raw->width = fbd_raw->trim.size_x;
		fbd_raw->height = fbd_raw->trim.size_y;

		crop_start_x = fbd_raw->trim.start_x;
		crop_start_y = fbd_raw->trim.start_y;
		crop_width = fbd_raw->trim.size_x;
		crop_height = fbd_raw->trim.size_y;

		left_offset_tiles_num = crop_start_x / ISP_FBD_TILE_WIDTH;
		up_offset_tiles_num = crop_start_y / ISP_FBD_TILE_HEIGHT;
		end_x = crop_start_x + crop_width - 1;
		end_y = crop_start_y + crop_height - 1;
		if (crop_start_x %  ISP_FBD_TILE_WIDTH == 0) {
			left_tiles_num = 0;
			left_size = 0;
		} else {
			left_tiles_num = 1;
			left_size = ISP_FBD_TILE_WIDTH - crop_start_x %  ISP_FBD_TILE_WIDTH;
		}
		if ((end_x + 1) % ISP_FBD_TILE_WIDTH == 0)
			right_tiles_num = 0;
		else
			right_tiles_num = 1;
		right_size = (end_x + 1) % ISP_FBD_TILE_WIDTH;
		middle_tiles_num = (crop_width - left_size - right_size) / ISP_FBD_TILE_WIDTH;

		if (crop_start_y % ISP_FBD_TILE_HEIGHT == 0) {
			up_tiles_num = 0;
			up_size = 0;
		} else {
			up_tiles_num = 1;
			up_size = ISP_FBD_TILE_HEIGHT - crop_start_y % ISP_FBD_TILE_HEIGHT;
		}
		if ((end_y + 1) % ISP_FBD_TILE_HEIGHT == 0)
			down_tiles_num = 0;
		else
			down_tiles_num = 1;
		down_size = (end_y + 1) % ISP_FBD_TILE_HEIGHT;
		vertical_middle_tiles_num = (crop_height - up_size - down_size) / ISP_FBD_TILE_HEIGHT;
		fbd_raw->pixel_start_in_hor = crop_start_x % ISP_FBD_TILE_WIDTH;
		fbd_raw->pixel_start_in_ver = crop_start_y % ISP_FBD_TILE_HEIGHT;
		fbd_raw->tiles_num_in_ver = up_tiles_num + down_tiles_num + vertical_middle_tiles_num;
		fbd_raw->tiles_num_in_hor = left_tiles_num + right_tiles_num + middle_tiles_num;
		fbd_raw->tiles_start_odd = left_offset_tiles_num % 2;
		fbd_raw->header_addr_offset =
			(left_offset_tiles_num + up_offset_tiles_num * tiles_num_pitch) / 2;
		fbd_raw->tile_addr_offset_x256 =
			(left_offset_tiles_num + up_offset_tiles_num * tiles_num_pitch) * ISP_FBD_BASE_ALIGN;
		fbd_raw->low_bit_addr_offset = (crop_start_x + crop_start_y * img_width) /2;
		if(fbd_raw->fetch_fbd_4bit_bypass == 0)
			fbd_raw->low_4bit_addr_offset = (crop_start_x + crop_start_y * img_width);
		pr_debug("addr offset:header 0x%x, 8bit 0x%x, 2bit 0x%x, 4bit 0x%x\n",
			fbd_raw->header_addr_offset, fbd_raw->tile_addr_offset_x256,
			fbd_raw->low_bit_addr_offset, fbd_raw->low_4bit_addr_offset);
	} else {
		fbd_raw->pixel_start_in_hor = 0;
		fbd_raw->pixel_start_in_ver = 0;
		fbd_raw->tiles_num_in_hor = tile_col;
		fbd_raw->tiles_num_in_ver = tile_row;
		fbd_raw->tiles_start_odd = 0;
		fbd_raw->header_addr_offset = 0;
		fbd_raw->tile_addr_offset_x256 = 0;
		fbd_raw->low_bit_addr_offset = 0;
		if(fbd_raw->fetch_fbd_4bit_bypass == 0)
			fbd_raw->low_4bit_addr_offset = 0;
	}

	cal_fbc.compress_4bit_bypass = frame->compress_4bit_bypass;
	cal_fbc.data_bits = pipe_src->data_in_bits;
	cal_fbc.fbc_info = &frame->fbc_info;
	cal_fbc.in = frame->buf.iova[0];
	cal_fbc.fmt = DCAM_STORE_RAW_BASE;
	cal_fbc.height = fbd_raw->height;
	cal_fbc.width = fbd_raw->width;
	cal_fbc.out = &fbd_raw->hw_addr;

	dcam_if_cal_compressed_addr(&cal_fbc);
	/* store start address for slice use */
	fbd_raw->header_addr_init = fbd_raw->hw_addr.addr1;
	fbd_raw->tile_addr_init_x256 = fbd_raw->hw_addr.addr1;
	fbd_raw->low_bit_addr_init = fbd_raw->hw_addr.addr2;
	if (fbd_raw->fetch_fbd_4bit_bypass == 0)
		fbd_raw->low_4bit_addr_init = fbd_raw->hw_addr.addr3;

	pr_debug("fetch_fbd: %u 0x%x 0x%x, 0x%x, size %u %u\n",
		 frame->fid, fbd_raw->hw_addr.addr0,
		 fbd_raw->hw_addr.addr1, fbd_raw->hw_addr.addr2,
		pipe_src->src.w, pipe_src->src.h);

	return 0;
}

static int ispdrv_fbd_yuv_get(void *cfg_in, void *cfg_out,
		struct camera_frame *frame)
{
	int32_t tile_col = 0, tile_row = 0;
	struct isp_fbd_yuv_info *fbd_yuv = NULL;
	struct isp_uinfo *pipe_src = NULL;
	struct dcam_compress_cal_para cal_fbc = {0};

	if (!cfg_in || !cfg_out || !frame) {
		pr_err("fail to get valid input ptr, %p, %p\n", cfg_in, cfg_out);
		return -EFAULT;
	}

	fbd_yuv = (struct isp_fbd_yuv_info *)cfg_out;
	pipe_src = (struct isp_uinfo *)cfg_in;

	if (pipe_src->fetch_path_sel == 0)
		return 0;

	fbd_yuv->fetch_fbd_bypass = 0;
	fbd_yuv->slice_size.w = pipe_src->src.w;
	fbd_yuv->slice_size.h = pipe_src->src.h;
	fbd_yuv->trim = pipe_src->crop;
	tile_col = (fbd_yuv->slice_size.w + ISP_FBD_TILE_WIDTH - 1) / ISP_FBD_TILE_WIDTH;
	tile_row =(fbd_yuv->slice_size.h + ISP_FBD_TILE_HEIGHT - 1) / ISP_FBD_TILE_HEIGHT;

	fbd_yuv->tile_num_pitch = tile_col;
	fbd_yuv->slice_start_pxl_xpt = 0;
	fbd_yuv->slice_start_pxl_ypt = 0;

	cal_fbc.compress_4bit_bypass = frame->compress_4bit_bypass;
	cal_fbc.data_bits = pipe_src->data_in_bits;
	cal_fbc.fbc_info = &frame->fbc_info;
	cal_fbc.in = frame->buf.iova[0];
	if (pipe_src->in_fmt == IMG_PIX_FMT_NV21)
		cal_fbc.fmt = DCAM_STORE_YVU420;
	else if (pipe_src->in_fmt == IMG_PIX_FMT_NV12)
		cal_fbc.fmt = DCAM_STORE_YUV420;
	cal_fbc.height = fbd_yuv->slice_size.h;
	cal_fbc.width = fbd_yuv->slice_size.w;
	cal_fbc.out = &fbd_yuv->hw_addr;
	dcam_if_cal_compressed_addr(&cal_fbc);
	fbd_yuv->buffer_size = cal_fbc.fbc_info->buffer_size;

	/* store start address for slice use */
	fbd_yuv->frame_header_base_addr = fbd_yuv->hw_addr.addr0;
	fbd_yuv->slice_start_header_addr = fbd_yuv->frame_header_base_addr +
			((fbd_yuv->slice_start_pxl_ypt / ISP_FBD_TILE_HEIGHT) * fbd_yuv->tile_num_pitch +
			fbd_yuv->slice_start_pxl_xpt / ISP_FBD_TILE_WIDTH) * 16;
	fbd_yuv->data_bits = cal_fbc.data_bits;

	pr_debug("iova:%x, fetch_fbd: %u 0x%x 0x%x, 0x%x, size %u %u, channel_id:%d, tile_col:%d\n",
		 frame->buf.iova[0], frame->fid, fbd_yuv->hw_addr.addr0,
		 fbd_yuv->hw_addr.addr1, fbd_yuv->hw_addr.addr2,
		pipe_src->src.w, pipe_src->src.h, frame->channel_id, fbd_yuv->tile_num_pitch);

	return 0;
}

static enum isp_store_format ispdrv_store_format_get(struct isp_path_uinfo *in_ptr)
{
	enum isp_store_format format = ISP_STORE_FORMAT_MAX;

	if (!in_ptr) {
		pr_err("fail to get valid input ptr is NULL\n");
		return format;
	}

	switch (in_ptr->out_fmt) {
	case IMG_PIX_FMT_UYVY:
		format = ISP_STORE_UYVY;
		break;
	case IMG_PIX_FMT_YUV422P:
		format = ISP_STORE_YUV422_3FRAME;
		break;
	case IMG_PIX_FMT_NV12:
		format = ISP_STORE_YUV420_2FRAME;
		break;
	case IMG_PIX_FMT_NV21:
		if (in_ptr->data_in_bits == ISP_FRAME_10_BIT)
			format = ISP_STORE_YVU420_2FRAME;
		else
			format = ISP_STORE_YUV420_2FRAME_MIPI;
		break;
	case IMG_PIX_FMT_YUV420:
		format = ISP_STORE_YUV420_3FRAME;
		break;
	case IMG_PIX_FMT_FULL_RGB:
		format = ISP_STORE_FULL_RGB;
		break;
	default:
		format = ISP_STORE_FORMAT_MAX;
		pr_err("fail to get support format 0x%x\n", in_ptr->out_fmt);
		break;
	}

	return format;
}

static int ispdrv_store_afbc_get(struct isp_path_uinfo *in_ptr,
	struct isp_hw_afbc_path *afbc)
{
	uint32_t w_tile_num = 0, h_tile_num = 0;
	uint32_t pad_width = 0, pad_height = 0;
	uint32_t header_size = 0, tile_data_size = 0;
	uint32_t header_addr = 0;
	struct isp_afbc_store_info *store_info = NULL;

	if (!in_ptr || !afbc) {
		pr_err("fail to get valid input ptr %p\n", in_ptr, afbc);
		return -EFAULT;
	}

	store_info = &afbc->afbc_store;
	store_info->bypass = 0;
	store_info->endian = in_ptr->data_endian.uv_endian;
	store_info->mirror_en = 0;
	store_info->color_format = ispdrv_afbc_store_format_get(in_ptr->out_fmt);
	store_info->tile_number_pitch = 0;
	store_info->header_offset = 0x0;
	store_info->border.up_border = 0;
	store_info->border.down_border = 0;
	store_info->border.left_border = 0;
	store_info->border.right_border = 0;
	store_info->size.w = in_ptr->dst.w;
	store_info->size.h = in_ptr->dst.h;

	pad_width = (store_info->size.w + AFBC_PADDING_W_YUV420 - 1)
		/ AFBC_PADDING_W_YUV420 * AFBC_PADDING_W_YUV420;
	pad_height = (store_info->size.h + AFBC_PADDING_H_YUV420 - 1)
		/ AFBC_PADDING_H_YUV420 * AFBC_PADDING_H_YUV420;

	w_tile_num = pad_width / AFBC_PADDING_W_YUV420;
	h_tile_num = pad_height / AFBC_PADDING_H_YUV420;

	header_size = w_tile_num * h_tile_num * AFBC_HEADER_SIZE;
	tile_data_size = w_tile_num * h_tile_num * AFBC_PAYLOAD_SIZE;
	header_addr = store_info->yheader;

	store_info->header_offset = (header_size + 1024 - 1) / 1024 * 1024;
	store_info->yheader = header_addr;
	store_info->yaddr = store_info->yheader + store_info->header_offset;
	store_info->tile_number_pitch = w_tile_num;

	pr_debug("afbc w_tile_num = %d, h_tile_num = %d\n", w_tile_num, h_tile_num);
	pr_debug("afbc header_offset = %x, yheader = %x, yaddr = %x\n",
		store_info->header_offset, store_info->yheader, store_info->yaddr);

	return 0;
}

static int ispdrv_store_normal_get(struct isp_path_uinfo *in_ptr,
	struct isp_hw_path_store *store_info)
{
	int ret = 0;
	struct isp_store_info *store = NULL;

	if (!in_ptr || !store_info) {
		pr_err("fail to get valid input ptr %p\n", in_ptr, store_info);
		return -EFAULT;
	}

	store = &store_info->store;
	store->color_fmt = ispdrv_store_format_get(in_ptr);
	store->bypass = 0;
	store->endian = in_ptr->data_endian.uv_endian;
	if (store->color_fmt == ISP_STORE_FULL_RGB)
		store->speed_2x = 0;
	else
		store->speed_2x = 1;

	if (in_ptr->data_in_bits == ISP_FRAME_10_BIT)
		store->need_bwd = 1;
	else
		store->need_bwd = 0;

	store->mirror_en = 0;
	store->max_len_sel = 0;
	store->shadow_clr_sel = 1;
	store->shadow_clr = 1;
	store->store_res = 1;
	store->rd_ctrl = 0;
	store->size.w = in_ptr->dst.w;
	store->size.h = in_ptr->dst.h;
	switch (store->color_fmt) {
	case ISP_STORE_UYVY:
		store->pitch.pitch_ch0 = store->size.w * 2;
		store->total_size = store->size.w * store->size.h * 2;
		break;
	case ISP_STORE_YUV422_2FRAME:
	case ISP_STORE_YVU422_2FRAME:
		store->pitch.pitch_ch0 = store->size.w;
		store->pitch.pitch_ch1 = store->size.w;
		store->total_size = store->size.w * store->size.h * 2;
		break;
	case ISP_STORE_YUV420_2FRAME:
	case ISP_STORE_YVU420_2FRAME:
		store->pitch.pitch_ch0 = store->size.w;
		store->pitch.pitch_ch1 = store->size.w;
		store->total_size = store->size.w * store->size.h * 3 / 2;
		break;
	case ISP_STORE_YUV420_2FRAME_10:
	case ISP_STORE_YVU420_2FRAME_10:
		store->pitch.pitch_ch0 = store->size.w * 2;
		store->pitch.pitch_ch1 = store->size.w * 2;
		store->total_size = store->size.w * store->size.h * 3 / 2;
		break;
	case ISP_STORE_YUV420_2FRAME_MIPI:
	case ISP_STORE_YVU420_2FRAME_MIPI:
		store->pitch.pitch_ch0 = store->size.w * 10 / 8;
		store->pitch.pitch_ch1 = store->size.w * 10 / 8 ;
		store->total_size = store->size.w * store->size.h * 3 / 2;
		break;
	case ISP_STORE_YUV422_3FRAME:
		store->pitch.pitch_ch0 = store->size.w;
		store->pitch.pitch_ch1 = store->size.w / 2;
		store->pitch.pitch_ch2 = store->size.w / 2;
		store->total_size = store->size.w * store->size.h * 2;
		break;
	case ISP_STORE_YUV420_3FRAME:
		store->pitch.pitch_ch0 = store->size.w;
		store->pitch.pitch_ch1 = store->size.w / 2;
		store->pitch.pitch_ch2 = store->size.w / 2;
		store->total_size = store->size.w * store->size.h * 3 / 2;
		break;
	case ISP_STORE_FULL_RGB:
		store->pitch.pitch_ch0 = store->size.w * 8;
		break;
	default:
		pr_err("fail to get support store fmt: %d\n", store->color_fmt);
		store->pitch.pitch_ch0 = 0;
		store->pitch.pitch_ch1 = 0;
		store->pitch.pitch_ch2 = 0;
		break;
	}

	return ret;
}

static uint32_t ispdrv_deci_factor_cal(uint32_t deci)
{
	/* 0: 1/2, 1: 1/4, 2: 1/8, 3: 1/16*/
	if (deci == 16)
		return 3;
	else if (deci == 8)
		return 2;
	else if (deci == 4)
		return 1;
	else
		return 0;
}

static uint32_t ispdrv_deci_cal(uint32_t src, uint32_t dst)
{
	uint32_t deci = 1;

	if (src <= dst * 4)
		deci = 1;
	else if (src <= dst * 8)
		deci = 2;
	else if (src <= dst * 16)
		deci = 4;
	else if (src <= dst * 32)
		deci = 8;
	else if (src <= dst * 64)
		deci = 16;
	else
		deci = 0;
	return deci;
}

static int ispdrv_trim_deci_info_cal(uint32_t src, uint32_t dst,
	uint32_t *trim, uint32_t *deci)
{
	uint32_t tmp;

	tmp = ispdrv_deci_cal(src, dst);
	if (tmp == 0)
		return -EINVAL;

	if ((src % (2 * tmp)) == 0) {
		*trim = src;
		*deci = tmp;
	} else {
		*trim = (src / (2 * tmp) * (2 * tmp));
		*deci = ispdrv_deci_cal(*trim, dst);
	}
	return 0;
}

static int ispdrv_thumb_scaler_get(struct isp_path_uinfo *in_ptr,
	struct isp_hw_thumbscaler_info *scalerInfo)
{
	int ret = 0;
	uint32_t deci_w = 0;
	uint32_t deci_h = 0;
	uint32_t trim_w, trim_h, temp_w, temp_h;
	uint32_t offset, shift, is_yuv422 = 0;
	struct img_size src, dst;
	uint32_t align_size = 0;

	if (!in_ptr || !scalerInfo) {
		pr_err("fail to get valid input ptr %p\n", in_ptr, scalerInfo);
		return -EFAULT;
	}

	scalerInfo->scaler_bypass = 0;
	scalerInfo->frame_deci = 0;
	/* y factor & deci */
	src.w = in_ptr->in_trim.size_x;
	src.h = in_ptr->in_trim.size_y;
	dst = in_ptr->dst;
	ret = ispdrv_trim_deci_info_cal(src.w, dst.w, &temp_w, &deci_w);
	ret |= ispdrv_trim_deci_info_cal(src.h, dst.h, &temp_h, &deci_h);
	if (deci_w == 0 || deci_h == 0)
		return -EINVAL;
	if (ret) {
		pr_err("fail to set thumbscaler ydeci. src %d %d, dst %d %d\n",
					src.w, src.h, dst.w, dst.h);
		return ret;
	}

	scalerInfo->y_deci.deci_x = deci_w;
	scalerInfo->y_deci.deci_y = deci_h;
	if (deci_w > 1)
		scalerInfo->y_deci.deci_x_eb = 1;
	else
		scalerInfo->y_deci.deci_x_eb = 0;
	if (deci_h > 1)
		scalerInfo->y_deci.deci_y_eb = 1;
	else
		scalerInfo->y_deci.deci_y_eb = 0;
	align_size = deci_w * ISP_PIXEL_ALIGN_WIDTH;
	trim_w = (temp_w) & ~(align_size - 1);
	align_size = deci_h * ISP_PIXEL_ALIGN_HEIGHT;
	trim_h = (temp_h) & ~(align_size - 1);
	scalerInfo->y_factor_in.w = trim_w / deci_w;
	scalerInfo->y_factor_in.h = trim_h / deci_h;
	scalerInfo->y_factor_out = in_ptr->dst;

	if (in_ptr->out_fmt == IMG_PIX_FMT_YUV422P)
		is_yuv422 = 1;

	/* uv factor & deci, input: yuv422(isp pipeline format) */
	shift = is_yuv422 ? 0 : 1;
	scalerInfo->uv_deci.deci_x = deci_w;
	scalerInfo->uv_deci.deci_y = deci_h;
	if (deci_w > 1)
		scalerInfo->uv_deci.deci_x_eb = 1;
	else
		scalerInfo->uv_deci.deci_x_eb = 0;
	if (deci_h > 1)
		scalerInfo->uv_deci.deci_y_eb = 1;
	else
		scalerInfo->uv_deci.deci_y_eb = 0;
	trim_w >>= 1;
	scalerInfo->uv_factor_in.w = trim_w / deci_w;
	scalerInfo->uv_factor_in.h = trim_h / deci_h;
	scalerInfo->uv_factor_out.w = dst.w / 2;
	scalerInfo->uv_factor_out.h = dst.h >> shift;

	scalerInfo->src0.w = in_ptr->in_trim.size_x;
	scalerInfo->src0.h = in_ptr->in_trim.size_y;

	/* y trim */
	trim_w = scalerInfo->y_factor_in.w * scalerInfo->y_deci.deci_x;
	offset = (in_ptr->in_trim.size_x - trim_w) / 2;
	scalerInfo->y_trim.start_x = in_ptr->in_trim.start_x + offset;
	scalerInfo->y_trim.size_x = trim_w;

	trim_h = scalerInfo->y_factor_in.h * scalerInfo->y_deci.deci_y;
	offset = (in_ptr->in_trim.size_y - trim_h) / 2;
	scalerInfo->y_trim.start_y = in_ptr->in_trim.start_y + offset;
	scalerInfo->y_trim.size_y = trim_h;

	scalerInfo->y_src_after_deci = scalerInfo->y_factor_in;
	scalerInfo->y_dst_after_scaler = scalerInfo->y_factor_out;

	/* uv trim */
	trim_w = scalerInfo->uv_factor_in.w * scalerInfo->uv_deci.deci_x;
	offset = (in_ptr->in_trim.size_x / 2 - trim_w) / 2;
	scalerInfo->uv_trim.start_x = in_ptr->in_trim.start_x / 2 + offset;
	scalerInfo->uv_trim.size_x = trim_w;

	trim_h = scalerInfo->uv_factor_in.h * scalerInfo->uv_deci.deci_y;
	offset = (in_ptr->in_trim.size_y - trim_h) / 2;
	scalerInfo->uv_trim.start_y = in_ptr->in_trim.start_y + offset;
	scalerInfo->uv_trim.size_y = trim_h;

	scalerInfo->uv_src_after_deci = scalerInfo->uv_factor_in;
	scalerInfo->uv_dst_after_scaler = scalerInfo->uv_factor_out;
	scalerInfo->odata_mode = is_yuv422 ? 0x00 : 0x01;

	scalerInfo->y_deci.deci_x = ispdrv_deci_factor_cal(scalerInfo->y_deci.deci_x);
	scalerInfo->y_deci.deci_y = ispdrv_deci_factor_cal(scalerInfo->y_deci.deci_y);
	scalerInfo->uv_deci.deci_x = ispdrv_deci_factor_cal(scalerInfo->uv_deci.deci_x);
	scalerInfo->uv_deci.deci_y = ispdrv_deci_factor_cal(scalerInfo->uv_deci.deci_y);

	/* N6pro thumbscaler calculation regulation */
	if (scalerInfo->thumbscl_cal_version == 1) {
		scalerInfo->y_init_phase.w = scalerInfo->y_dst_after_scaler.w / 2;
		scalerInfo->y_init_phase.h = scalerInfo->y_dst_after_scaler.h / 2;
		scalerInfo->uv_src_after_deci.w = scalerInfo->y_src_after_deci.w / 2;
		scalerInfo->uv_src_after_deci.h = scalerInfo->y_src_after_deci.h;
		scalerInfo->uv_dst_after_scaler.w = scalerInfo->y_dst_after_scaler.w / 2;
		scalerInfo->uv_dst_after_scaler.h = scalerInfo->y_dst_after_scaler.h / 2;
		scalerInfo->uv_trim.size_x = scalerInfo->y_trim.size_x / 2;
		scalerInfo->uv_trim.size_y = scalerInfo->y_trim.size_y / 2;
		scalerInfo->uv_init_phase.w = scalerInfo->uv_dst_after_scaler.w / 2;
		scalerInfo->uv_init_phase.h = scalerInfo->uv_dst_after_scaler.h / 2;
		scalerInfo->uv_factor_in.w = scalerInfo->y_factor_in.w / 2;
		scalerInfo->uv_factor_in.h = scalerInfo->y_factor_in.h / 2;
		scalerInfo->uv_factor_out.w = scalerInfo->y_factor_out.w / 2;
		scalerInfo->uv_factor_out.h = scalerInfo->y_factor_out.h / 2;
	}

	pr_debug("deciY %d %d, Yfactor (%d %d) => (%d %d) ytrim (%d %d %d %d)\n",
		scalerInfo->y_deci.deci_x, scalerInfo->y_deci.deci_y,
		scalerInfo->y_factor_in.w, scalerInfo->y_factor_in.h,
		scalerInfo->y_factor_out.w, scalerInfo->y_factor_out.h,
		scalerInfo->y_trim.start_x, scalerInfo->y_trim.start_y,
		scalerInfo->y_trim.size_x, scalerInfo->y_trim.size_y);
	pr_debug("deciU %d %d, Ufactor (%d %d) => (%d %d), Utrim (%d %d %d %d)\n",
		scalerInfo->uv_deci.deci_x, scalerInfo->uv_deci.deci_y,
		scalerInfo->uv_factor_in.w, scalerInfo->uv_factor_in.h,
		scalerInfo->uv_factor_out.w, scalerInfo->uv_factor_out.h,
		scalerInfo->uv_trim.start_x, scalerInfo->uv_trim.start_y,
		scalerInfo->uv_trim.size_x, scalerInfo->uv_trim.size_y);

	pr_debug("my frameY: %d %d %d %d\n",
		scalerInfo->y_src_after_deci.w, scalerInfo->y_src_after_deci.h,
		scalerInfo->y_dst_after_scaler.w,
		scalerInfo->y_dst_after_scaler.h);
	pr_debug("my frameU: %d %d %d %d\n",
		scalerInfo->uv_src_after_deci.w,
		scalerInfo->uv_src_after_deci.h,
		scalerInfo->uv_dst_after_scaler.w,
		scalerInfo->uv_dst_after_scaler.h);

	pr_debug("init_phase: Y(%d %d), UV(%d %d)\n",
		scalerInfo->y_init_phase.w, scalerInfo->y_init_phase.h,
		scalerInfo->uv_init_phase.w, scalerInfo->uv_init_phase.h);

	return ret;
}

int isp_drv_pipeinfo_get(void *arg, void *frame)
{
	int ret = 0;
	uint32_t i = 0;
	struct isp_sw_context *ctx = NULL;
	struct isp_pipe_info *pipe_in = NULL;
	struct isp_uinfo *pipe_src = NULL;
	struct isp_path_uinfo *path_info = NULL;
	struct camera_frame *pframe = NULL;

	if (!arg || !frame) {
		pr_err("fail to get valid arg %p frame %p\n", arg, frame);
		return -EFAULT;
	}

	ctx = (struct isp_sw_context *)arg;
	pframe = (struct camera_frame *)frame;
	pipe_src = &ctx->pipe_src;
	pipe_in = &ctx->pipe_info;

	pipe_in->fetch.ctx_id = ctx->ctx_id;
	pipe_in->fetch.sec_mode = ctx->dev->sec_mode;
	pipe_src->fetch_path_sel = pframe->is_compressed;
	ret = ispdrv_fetch_normal_get(pipe_src, &pipe_in->fetch, pframe);
	if (ret) {
		pr_err("fail to get pipe fetch info\n");
		return -EFAULT;
	}

	pipe_in->fetch_fbd.ctx_id = ctx->ctx_id;
	pipe_in->fetch_fbd_yuv.ctx_id = ctx->ctx_id;

	if (ctx->dev->isp_hw->ip_isp->fbd_raw_support) {
		ret = ispdrv_fbd_raw_get(pipe_src, &pipe_in->fetch_fbd, pframe);
		pipe_in->fetch_fbd_yuv.fetch_fbd_bypass = 1;
	} else if (ctx->dev->isp_hw->ip_isp->fbd_yuv_support) {
		ret = ispdrv_fbd_yuv_get(pipe_src, &pipe_in->fetch_fbd_yuv, pframe);
		pipe_in->fetch_fbd.fetch_fbd_bypass = 1;
	}
	if (ret) {
		pr_err("fail to get pipe fetch fbd info\n");
		return -EFAULT;
	}

	for (i = 0; i < ISP_SPATH_NUM; i++) {
		path_info = &pipe_src->path_info[i];
		if (atomic_read(&ctx->isp_path[i].user_cnt) < 1)
			continue;
		pipe_in->store[i].ctx_id = ctx->ctx_id;
		pipe_in->store[i].spath_id = i;
		ret = ispdrv_store_normal_get(path_info, &pipe_in->store[i]);
		if (ret) {
			pr_err("fail to get pipe store normal info\n");
			return -EFAULT;
		}

		if (i < AFBC_PATH_NUM) {
			pipe_in->afbc[i].ctx_id = ctx->ctx_id;
			pipe_in->afbc[i].spath_id = i;
			ispdrv_store_afbc_get(path_info, &pipe_in->afbc[i]);
			/* store normal should bypass when afbc store work */
			if (path_info->store_fbc)
				pipe_in->store[i].store.bypass = 1;
			else
				pipe_in->afbc[i].afbc_store.bypass = 1;
		}

		if (i != ISP_SPATH_FD) {
			pipe_in->scaler[i].ctx_id = ctx->ctx_id;
			pipe_in->scaler[i].spath_id = i;
			pipe_in->scaler[i].src.w = pipe_src->crop.size_x;
			pipe_in->scaler[i].src.h = pipe_src->crop.size_y;
			if (pframe->data_src_dec) {
				pipe_in->scaler[i].in_trim.size_x = pipe_src->crop.size_x;
				pipe_in->scaler[i].in_trim.size_y = pipe_src->crop.size_y;
			} else {
				pipe_in->scaler[i].in_trim = path_info->in_trim;
			}
			path_info->scaler_coeff_ex = ctx->hw->ip_isp->scaler_coeff_ex;
			path_info->scaler_bypass_ctrl = ctx->hw->ip_isp->scaler_bypass_ctrl;
			ret = ispdrv_path_scaler_get(path_info, &pipe_in->scaler[i]);
			if (ret) {
				pr_err("fail to get pipe path scaler info\n");
				return -EFAULT;
			}
		} else {
			pipe_in->thumb_scaler.idx = ctx->ctx_id;
			if (ctx->dev->isp_hw->ip_isp->thumb_scaler_cal_version)
				pipe_in->thumb_scaler.thumbscl_cal_version = 1;
			ret = ispdrv_thumb_scaler_get(path_info, &pipe_in->thumb_scaler);
			if (ret) {
				pr_err("fail to get pipe thumb scaler info\n");
				return -EFAULT;
			}
		}
	}

	return ret;
}

int isp_drv_dt_parse(struct device_node *dn,
		struct cam_hw_info *hw_info,
		uint32_t *isp_count)
{
	int i = 0;
	uint32_t count = 0;
	void __iomem *reg_base;
	struct cam_hw_soc_info *soc_isp = NULL;
	struct cam_hw_ip_info *ip_isp = NULL;
	struct device_node *isp_node = NULL;
	struct device_node *qos_node = NULL;
	struct device_node *iommu_node = NULL;
	struct resource res = {0};
	uint32_t args[2];

	/* todo: should update according to SharkL5/ROC1 dts
	 * or set value for required variables with hard-code
	 * for quick bringup
	 */

	if (!dn || !hw_info) {
		pr_err("fail to get dn %p hw info %p\n", dn, hw_info);
		return -EINVAL;
	}

	pr_info("isp dev device node %s, full name %s\n",
		dn->name, dn->full_name);
	isp_node = of_parse_phandle(dn, "sprd,isp", 0);
	if (isp_node == NULL) {
		pr_err("fail to parse the property of sprd,isp\n");
		return -EFAULT;
	}

	soc_isp = hw_info->soc_isp;
	ip_isp = hw_info->ip_isp;
	pr_info("after isp dev device node %s, full name %s\n",
		isp_node->name, isp_node->full_name);
	soc_isp->pdev = of_find_device_by_node(isp_node);
	if (soc_isp->pdev == NULL) {
		pr_err("fail to get isp pdev\n");
		return -EFAULT;
	}
	pr_info("sprd s_isp_pdev name %s\n", soc_isp->pdev->name);

	if (of_device_is_compatible(isp_node, "sprd,isp")) {
		if (of_property_read_u32_index(isp_node,
			"sprd,isp-count", 0, &count)) {
			pr_err("fail to parse the property of sprd,isp-count\n");
			return -EINVAL;
		}

		if (count > 1) {
			pr_err("fail to count isp number: %d", count);
			return -EINVAL;
		}
		*isp_count = count;

		/* read clk from dts */
		if (hw_info->cam_ioctl(hw_info, CAM_HW_GET_ISP_DTS_CLK, isp_node)) {
			pr_err("fail to get clk\n");
			return -EFAULT;
		}

		iommu_node = of_parse_phandle(isp_node, "iommus", 0);
		if (iommu_node) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
			reg_base = of_iomap(iommu_node, 0);
#else
			reg_base = of_iomap(iommu_node, 1);
#endif
			if (!reg_base)
				pr_err("fail to map ISP IOMMU base\n");
			else
				s_isp_mmubase = (unsigned long)reg_base;
		}
		pr_info("ISP IOMMU Base  0x%lx\n", s_isp_mmubase);

		/* qos dt parse */
		qos_node = of_parse_phandle(isp_node, "isp_qos", 0);
		if (qos_node) {
			uint8_t val;

			if (of_property_read_u8(qos_node, "awqos-high", &val)) {
				pr_warn("warning: isp awqos-high reading fail.\n");
				val = 7;
			}
			soc_isp->awqos_high = (uint32_t)val;
			if (of_property_read_u8(qos_node, "awqos-low", &val)) {
				pr_warn("warning: isp awqos-low reading fail.\n");
				val = 6;
			}
			soc_isp->awqos_low = (uint32_t)val;
			if (of_property_read_u8(qos_node, "arqos-high", &val)) {
				pr_warn("warning: isp arqos-high reading fail.\n");
				val = 7;
			}
			soc_isp->arqos_high = (uint32_t)val;
			if (of_property_read_u8(qos_node, "arqos-low", &val)) {
				pr_warn("warning: isp arqos-low reading fail.\n");
				val = 6;
			}
			soc_isp->arqos_low = (uint32_t)val;
			pr_info("get isp qos node. r: %d %d w: %d %d\n",
				soc_isp->arqos_high, soc_isp->arqos_low,
				soc_isp->awqos_high, soc_isp->awqos_low);
		} else {
			soc_isp->awqos_high = 7;
			soc_isp->awqos_low = 6;
			soc_isp->arqos_high = 7;
			soc_isp->arqos_low = 6;
		}

		soc_isp->cam_ahb_gpr = syscon_regmap_lookup_by_phandle(isp_node,
			"sprd,cam-ahb-syscon");
		if (IS_ERR_OR_NULL(soc_isp->cam_ahb_gpr)) {
			pr_err("fail to get sprd,cam-ahb-syscon");
			return PTR_ERR(soc_isp->cam_ahb_gpr);
		}

		if (!cam_syscon_get_args_by_name(isp_node, "reset",
			ARRAY_SIZE(args), args)) {
			ip_isp->syscon.rst = args[0];
			ip_isp->syscon.rst_mask = args[1];
		} else {
			pr_err("fail to get isp reset syscon\n");
			return -EINVAL;
		}

		if (!cam_syscon_get_args_by_name(isp_node, "isp_ahb_reset",
			ARRAY_SIZE(args), args))
			ip_isp->syscon.rst_ahb_mask = args[1];

		if (!cam_syscon_get_args_by_name(isp_node, "isp_vau_reset",
			ARRAY_SIZE(args), args))
			ip_isp->syscon.rst_vau_mask = args[1];

		if (!cam_syscon_get_args_by_name(isp_node, "sys_h2p_db_soft_rst",
			ARRAY_SIZE(args), args)) {
			ip_isp->syscon.sys_soft_rst = args[0];
			ip_isp->syscon.sys_h2p_db_soft_rst = args[1];
		}

		if (of_address_to_resource(isp_node, i, &res))
			pr_err("fail to get isp phys addr\n");

		ip_isp->phy_base = (unsigned long)res.start;
		isp_phys_base[0] = ip_isp->phy_base;
		pr_info("isp phys reg base is %lx\n", isp_phys_base[0]);
		reg_base = of_iomap(isp_node, i);
		if (!reg_base) {
			pr_err("fail to get isp reg_base %d\n", i);
			return -ENXIO;
		}

		ip_isp->reg_base = (unsigned long)reg_base;
		s_isp_regbase[0] = ip_isp->reg_base;

		for (i = 0; i < ISP_LOGICAL_COUNT; i++) {
			s_isp_irq_no[i] = irq_of_parse_and_map(isp_node, i);
			if (s_isp_irq_no[i] <= 0) {
				pr_err("fail to get isp irq %d\n", i);
				return -EFAULT;
			}

			pr_info("ISP%d dts OK! regbase %lx, irq %d\n", i,
				s_isp_regbase[0], s_isp_irq_no[i]);
		}
		ip_isp->dec_irq_no = s_isp_irq_no[ISP_LOGICAL_COUNT - 1];
	} else {
		pr_err("fail to match isp device node\n");
		return -EINVAL;
	}

	return 0;
}

int isp_drv_hw_init(void *arg)
{
	int ret = 0;
	uint32_t reset_flag = 0;
	struct isp_pipe_dev *dev = NULL;
	struct cam_hw_info *hw = NULL;
	struct isp_hw_default_param default_para;

	if (!arg) {
		pr_err("fail to get valid arg\n");
		return -EFAULT;
	}

	dev = (struct isp_pipe_dev *)arg;
	hw = dev->isp_hw;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	ret = sprd_cam_pw_on();
	if (ret)
		goto exit;
	ret = sprd_cam_domain_eb();
	if (ret)
		goto power_eb_fail;
#else
	if (ret)
		goto exit;
#endif

	ret = hw->isp_ioctl(hw, ISP_HW_CFG_ENABLE_CLK, NULL);
	if (ret)
		goto clk_fail;

	reset_flag = ISP_RESET_AFTER_POWER_ON;
	ret = hw->isp_ioctl(hw, ISP_HW_CFG_RESET, &reset_flag);
	if (ret)
		goto reset_fail;

	ret = isp_int_irq_request(&hw->pdev->dev, s_isp_irq_no, arg);
	if (ret)
		goto reset_fail;

	default_para.type = ISP_HW_PARA;
	hw->isp_ioctl(hw, ISP_HW_CFG_DEFAULT_PARA_SET, &default_para);

	sprd_iommu_restore(&hw->soc_isp->pdev->dev);
	return 0;

reset_fail:
	hw->isp_ioctl(hw, ISP_HW_CFG_DISABLE_CLK, NULL);
clk_fail:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	sprd_cam_domain_disable();
power_eb_fail:
	sprd_cam_pw_off();
#endif
exit:
	return ret;
}

int isp_drv_hw_deinit(void *arg)
{
	int ret = 0;
	uint32_t reset_flag = 0;
	struct isp_pipe_dev *dev = NULL;
	struct cam_hw_info *hw = NULL;

	if (!arg) {
		pr_err("fail to get valid arg\n");
		return -EFAULT;
	}

	dev = (struct isp_pipe_dev *)arg;
	hw = dev->isp_hw;

	reset_flag = ISP_RESET_BEFORE_POWER_OFF;
	ret = hw->isp_ioctl(hw, ISP_HW_CFG_RESET, &reset_flag);
	if (ret)
		pr_err("fail to reset isp\n");
	ret = isp_int_irq_free(&hw->pdev->dev, arg);
	if (ret)
		pr_err("fail to free isp irq\n");
	ret = hw->isp_ioctl(hw, ISP_HW_CFG_DISABLE_CLK, NULL);
	if (ret)
		pr_err("fail to disable isp clk\n");

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	sprd_cam_domain_disable();
	sprd_cam_pw_off();
#endif
	return ret;
}
