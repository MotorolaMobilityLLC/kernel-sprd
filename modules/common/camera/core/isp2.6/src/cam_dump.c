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

#include "cam_dump.h"

static void camdump_write_image_to_file(uint8_t *buffer,
	ssize_t size, const char *file)
{
	ssize_t result = 0, total = 0, writ = 0;
	struct file *wfp;

	wfp = cam_filp_open(file, O_CREAT | O_RDWR | O_APPEND, 0666);
	if (IS_ERR_OR_NULL(wfp)) {
		pr_err("fail to open file %s\n", file);
		return;
	}
	pr_debug("write image buf=%p, size=%d\n", buffer, (uint32_t)size);
	do {
		writ = (BYTE_PER_ONCE < size) ? BYTE_PER_ONCE : size;
		result = cam_kernel_write(wfp, buffer, writ, &wfp->f_pos);
		pr_debug("write result: %d, size: %d, pos: %d\n",
		(uint32_t)result,  (uint32_t)size, (uint32_t)wfp->f_pos);

		if (result > 0) {
			size -= result;
			buffer += result;
		}
		total += result;
	} while ((result > 0) && (size > 0));
	filp_close(wfp, NULL);
	pr_debug("write image done, total=%d\n", (uint32_t)total);
}

static int camdump_compress_frame_dump(struct cam_dump_ctx *dump_base, struct camera_frame *pframe, uint8_t* file_name, uint32_t* offset)
{
	struct cam_dump_fbc_header fbc_hdr = {0};
	unsigned long addr = 0;
	uint8_t tmp_str[20] = { '\0' };
	uint8_t superblock_layout = 5, file_message = 2;
	uint8_t tiled = 0, yuv_transform = 0, block_split = 0, left_crop = 0, top_crop = 0;
	uint8_t ncomponents[2];
	sprintf(tmp_str, "_compress");
	strcat(file_name, tmp_str);
	if (dump_base->pack_bits == CAM_RAW_HALF14 || dump_base->pack_bits == CAM_RAW_HALF10)
		strcat(file_name, ".raw");
	else
		strcat(file_name, ".mipi_raw");
	/* The fbd tool need to configure fbc_hdr.The fbc_hdr setting from Algorithm team. */
	ncomponents[0] = 1;
	ncomponents[1] = 2;
	fbc_hdr.yuv_mirror_en = 0;
	fbc_hdr.yuv_format = DUMP_AFBC_Y_UV;
	fbc_hdr.endian = 0;
	fbc_hdr.bits = dump_base->dcam_out_bits;
	fbc_hdr.img_h = pframe->height;
	fbc_hdr.img_w = pframe->width;
	fbc_hdr.img_h_pad = (pframe->height + AFBC_PADDING_H_YUV420_scaler - 1) / AFBC_PADDING_H_YUV420_scaler * AFBC_PADDING_H_YUV420_scaler;
	fbc_hdr.img_w_pad = (pframe->width  + AFBC_PADDING_W_YUV420_scaler - 1) / AFBC_PADDING_W_YUV420_scaler * AFBC_PADDING_W_YUV420_scaler;
	fbc_hdr.fbc_buffer_size = pframe->fbc_info.buffer_size;
	fbc_hdr.fbc_hdr_buffer[0] = 'A';
	fbc_hdr.fbc_hdr_buffer[1] = 'F';
	fbc_hdr.fbc_hdr_buffer[2] = 'B';
	fbc_hdr.fbc_hdr_buffer[3] = 'C';
	fbc_hdr.fbc_hdr_buffer[4] = AFBC_FILEHEADER_SIZE;
	fbc_hdr.fbc_hdr_buffer[5] = 0x00;
	fbc_hdr.fbc_hdr_buffer[6] = AFBC_VERSION;
	fbc_hdr.fbc_hdr_buffer[7] = 0x00;
	memcpy(&fbc_hdr.fbc_hdr_buffer[8], &(fbc_hdr.fbc_buffer_size), sizeof(fbc_hdr.fbc_buffer_size));
	fbc_hdr.fbc_hdr_buffer[12] = ncomponents[0] + ncomponents[1];
	fbc_hdr.fbc_hdr_buffer[13] = superblock_layout;
	fbc_hdr.fbc_hdr_buffer[14] = yuv_transform;
	fbc_hdr.fbc_hdr_buffer[15] = block_split;
	fbc_hdr.fbc_hdr_buffer[16] = dump_base->dcam_out_bits & 0xff;
	fbc_hdr.fbc_hdr_buffer[17] = dump_base->dcam_out_bits & 0xff;
	fbc_hdr.fbc_hdr_buffer[18] = dump_base->dcam_out_bits & 0xff;
	fbc_hdr.fbc_hdr_buffer[19] = 0x00;
	fbc_hdr.fbc_hdr_buffer[20] = pframe->fbc_info.tile_col & 0xff;
	fbc_hdr.fbc_hdr_buffer[21] = (pframe->fbc_info.tile_col >> 8) & 0xff;
	fbc_hdr.fbc_hdr_buffer[22] = pframe->fbc_info.tile_row & 0xff;
	fbc_hdr.fbc_hdr_buffer[23] = (pframe->fbc_info.tile_row >> 8) & 0xff;
	fbc_hdr.fbc_hdr_buffer[24] = fbc_hdr.img_w_pad & 0xff;
	fbc_hdr.fbc_hdr_buffer[25] = (fbc_hdr.img_w_pad >> 8) & 0xff;
	fbc_hdr.fbc_hdr_buffer[26] = fbc_hdr.img_h_pad & 0xff;
	fbc_hdr.fbc_hdr_buffer[27] = (fbc_hdr.img_h_pad >> 8) & 0xff;
	fbc_hdr.fbc_hdr_buffer[28] = left_crop;
	fbc_hdr.fbc_hdr_buffer[29] = top_crop;
	fbc_hdr.fbc_hdr_buffer[30] = tiled;
	fbc_hdr.fbc_hdr_buffer[31] = file_message;
	camdump_write_image_to_file((char *)&fbc_hdr, sizeof(fbc_hdr), file_name);
	addr = pframe->buf.addr_k[0] + *offset;
	*offset += pframe->fbc_info.buffer_size;
	camdump_write_image_to_file((char *)pframe->buf.addr_k[0], pframe->fbc_info.buffer_size, file_name);

	return 0;
}

static int camdump_one_frame_dump(struct cam_dump_ctx *dump_base, struct camera_frame *pframe)
{
	ssize_t size = 0;
	uint8_t file_name[256] = { '\0' };
	uint8_t file_name1[256] = { '\0' };
	uint8_t tmp_str[20] = { '\0' };
	uint32_t width = 0,offset = 0;
	unsigned long  addr = 0, addr1 = 0;
	enum cam_ch_id ch_id = 0;

	if(dump_base == NULL || pframe == NULL)
		return 0;

	if (cam_buf_kmap(&pframe->buf)) {
		pr_err("fail to kmap dump buf\n");
		return -EFAULT;
	}

	strcat(file_name, CAMERA_DUMP_PATH);
	if (dump_base->ch_id == CAM_CH_PRE)
		strcat(file_name, "pre_");
	else if(dump_base->ch_id == CAM_CH_RAW)
		strcat(file_name, "raw_");
	else
		strcat(file_name, "cap_");

	if (dump_base->dcam_out_fmt == DCAM_STORE_RAW_BASE)
		strcat(file_name, "raw_");
	else if (dump_base->dcam_out_fmt == DCAM_STORE_FRGB)
		strcat(file_name, "rgb_");
	else
		strcat(file_name, "yuv_");

	sprintf(tmp_str, "%d.", (uint32_t)dump_base->cur_dump_ts.tv_sec);
	strcat(file_name, tmp_str);
	sprintf(tmp_str, "%06d", (uint32_t)(dump_base->cur_dump_ts.tv_nsec / NSEC_PER_USEC));
	strcat(file_name, tmp_str);

	if (!pframe->sw_slice_num) {
		sprintf(tmp_str, "_w%d", pframe->width);
		strcat(file_name, tmp_str);
		sprintf(tmp_str, "_h%d", pframe->height);
		strcat(file_name, tmp_str);
		width = pframe->width;
	} else {
		sprintf(tmp_str, "_no%d", pframe->sw_slice_no);
		strcat(file_name, tmp_str);
		sprintf(tmp_str, "_w%d", pframe->slice_trim.size_x);
		strcat(file_name, tmp_str);
		sprintf(tmp_str, "_h%d", pframe->slice_trim.size_y);
		strcat(file_name, tmp_str);
		width = pframe->slice_trim.size_x;
	}

	sprintf(tmp_str, "_No%d", pframe->fid);
	strcat(file_name, tmp_str);

	if (pframe->is_compressed) {
		camdump_compress_frame_dump(dump_base, pframe, file_name, &offset);
	} else {
		if (dump_base->dcam_out_fmt == DCAM_STORE_RAW_BASE) {
			size = cal_sprd_raw_pitch(width, dump_base->pack_bits) * pframe->height;
			if (dump_base->pack_bits == CAM_RAW_HALF14 || dump_base->pack_bits == CAM_RAW_HALF10)
				strcat(file_name, ".raw");
			else
				strcat(file_name, ".mipi_raw");
			camdump_write_image_to_file((char *)pframe->buf.addr_k[0], size, file_name);
		} else if (dump_base->dcam_out_fmt == DCAM_STORE_FRGB) {
			size = pframe->height * pframe->width;
			strcat(file_name, ".rgb");
			camdump_write_image_to_file((char *)pframe->buf.addr_k[0], size, file_name);
		} else {
			if (dump_base->dcam_out_bits == DCAM_STORE_8_BIT) {
				strcat(file_name, "_8bit");
			} else {
				strcat(file_name, "_10bit");
				if (dump_base->is_pack)
					strcat(file_name, "_mipi");
			}
			strcat(file_name1, file_name);

			if (dump_base->dcam_out_fmt == DCAM_STORE_YUV420) {
				strcat(file_name, "_yuv420.y");
				strcat(file_name1, "_yuv420.uv");
			} else {
				strcat(file_name, "_yvu420.y");
				strcat(file_name1, "_yvu420.uv");
			}

			size = cal_sprd_yuv_pitch(pframe->width, dump_base->dcam_out_bits, dump_base->is_pack) * pframe->height;
			addr = pframe->buf.addr_k[0] + offset;
			addr1 = addr + size;
			offset += (size * 3 / 2);
			camdump_write_image_to_file((char *)addr, size, file_name);
			size = size / 2;
			camdump_write_image_to_file((char *)addr1, size, file_name1);
		}
	}

	cam_buf_kunmap(&pframe->buf);
	pr_debug("dump for ch %d, size %d, kaddr %p, file %s\n", ch_id,
		(int)size, (void *)pframe->buf.addr_k[0], file_name);

	return 0;
}

static int camdump_pdaf_frame_dump(struct cam_dump_ctx *dump_base, struct camera_frame *pframe)
{
	ssize_t size = 0;
	uint8_t file_name[256] = { '\0' };
	uint8_t file_name1[256] = { '\0' };
	uint8_t tmp_str[20] = { '\0' };
	unsigned long addr = 0, addr1 = 0;

	if(dump_base == NULL || pframe == NULL)
		return 0;

	/* Just add pdaf type3 dump now */
	if (dump_base->pdaf_type != DCAM_PDAF_TYPE3)
		return 0;
	strcat(file_name, CAMERA_DUMP_PATH);
	strcat(file_name, "pdaf");
	sprintf(tmp_str, "%d.", (uint32_t)dump_base->cur_dump_ts.tv_sec);
	strcat(file_name, tmp_str);
	sprintf(tmp_str, "%06d", (uint32_t)(dump_base->cur_dump_ts.tv_nsec / NSEC_PER_USEC));
	strcat(file_name, tmp_str);
	sprintf(tmp_str, "_No%d", pframe->fid);
	strcat(file_name, tmp_str);

	strcat(file_name1, file_name);
	strcat(file_name, "_left.yuv");
	strcat(file_name1, "_right.yuv");

	size = pframe->buf.size[0] / 2;;
	addr = pframe->buf.addr_k[0];
	addr1 = addr + size;
	camdump_write_image_to_file((char *)addr, size, file_name);
	camdump_write_image_to_file((char *)addr1, size, file_name1);

	pr_debug("dump for pdaf, size %d, kaddr %p, file %s\n",
		(int)size, (void *)pframe->buf.addr_k[0], file_name);

	return 0;
}

static inline int camdump_should_dump(int mode, int path)
{
	return (mode == DUMP_PATH_BOTH)
		|| (mode == DUMP_PATH_BIN && path == CAM_CH_PRE)
		|| (mode == DUMP_PATH_FULL && path == CAM_CH_CAP)
		|| (mode == DUMP_ISP_PYR_REC)
		|| (mode == DUMP_ISP_PYR_DEC)
		|| (mode == DUMP_DCAM_PDAF)
		|| (mode == DUMP_DCAM_OFFLINE)
		|| (mode == DUMP_PATH_RAW_BIN && (path == CAM_CH_PRE || path == CAM_CH_RAW));
}

static int camdump_param_cfg(void *handle, uint32_t cmd, void *param)
{
	struct cam_dump_ctx *dump_base = NULL;
	uint32_t ret = 0;

	if(!handle || !param) {
		pr_err("fail to get valid dump param %p cmd%d\n", handle, cmd);
		return -EFAULT;
	}

	dump_base = (struct cam_dump_ctx *)handle;
	switch (cmd) {
	case DUMP_CFG_OUT_FMT:
		dump_base->dcam_out_fmt = *(uint32_t *)param;
		break;
	case DUMP_CFG_IS_PACK:
		dump_base->is_pack = *(uint32_t *)param;
		break;
	case DUMP_CFG_PACK_BITS:
		dump_base->pack_bits = *(uint32_t *)param;
		break;
	case DUMP_CFG_OUT_BITS:
		dump_base->dcam_out_bits = *(uint32_t *)param;
		break;
	case DUMP_CFG_PYR_LAYER_NUM:
		dump_base->pyr_layer_num = *(uint32_t *)param;
		break;
	case DUMP_CFG_PYR_START_LAYER:
		dump_base->start_layer = *(uint32_t *)param;
		break;
	case DUMP_CFG_PDAF_TYPE:
		dump_base->pdaf_type = *(uint32_t *)param;
		break;
	default:
		pr_err("fail to get known cmd: %d\n", cmd);
		ret = -EFAULT;
		break;
	}
	return ret;
}

static int camdump_enqueue(struct cam_dump_ctx *dump_base, struct camera_frame *pframe)
{
	uint8_t ret = 0;

	if (!dump_base || !pframe) {
		pr_err("fail to dump enqueue handle\n");
		return -EFAULT;
	}
	if (!camdump_should_dump(g_dbg_dump.dump_en, pframe->channel_id))
		return -EFAULT;
	ret = cam_queue_enqueue(&dump_base->dump_queue, &pframe->list);
	if (ret == 0)
		complete(&dump_base->dump_com);
	return ret;
}

static int camdump_pyr_frame_store(struct cam_dump_ctx *dump_base, struct camera_frame *pframe,uint32_t i,
	uint32_t align_w, uint32_t align_h,ssize_t size )
{
	uint8_t file_name[256] = { '\0' };
	uint8_t file_name1[256] = { '\0' };
	uint8_t tmp_str[20] = { '\0' };
	unsigned long addr = 0, addr1 = 0;
	uint32_t offset = 0;

	if (cam_buf_kmap(&pframe->buf)) {
		pr_err("fail to kmap dump buf\n");
		return -EFAULT;
	}

	strcat(file_name, CAMERA_DUMP_PATH);
	if (g_dbg_dump.dump_en == DUMP_ISP_PYR_REC)
		strcat(file_name, "pyr_rec_");
	else if (g_dbg_dump.dump_en == DUMP_ISP_PYR_DEC)
			strcat(file_name, "isp_dec_");
		strcat(file_name, "layer_");
		sprintf(tmp_str, "%d.", i);
		strcat(file_name, tmp_str);
		sprintf(tmp_str, "%d.", (uint32_t)dump_base->cur_dump_ts.tv_sec);
		strcat(file_name, tmp_str);
		sprintf(tmp_str, "%06d", (uint32_t)(dump_base->cur_dump_ts.tv_nsec / NSEC_PER_USEC));
		strcat(file_name, tmp_str);
	if (i == 0) {
		sprintf(tmp_str, "_w%d", pframe->width);
		strcat(file_name, tmp_str);
		sprintf(tmp_str, "_h%d", pframe->height);
		strcat(file_name, tmp_str);
	} else {
		sprintf(tmp_str, "_w%d", align_w);
		strcat(file_name, tmp_str);
		sprintf(tmp_str, "_h%d", align_h);
		strcat(file_name, tmp_str);
	}
		sprintf(tmp_str, "_No%d", pframe->fid);
		strcat(file_name, tmp_str);

	if (dump_base->dcam_out_bits == DCAM_STORE_8_BIT) {
		strcat(file_name, "_8bit");
	} else {
		strcat(file_name, "_10bit");
		if (dump_base->is_pack)
			strcat(file_name, "_mipi");
	}

	if (pframe->is_compressed && i == 0) {
		camdump_compress_frame_dump(dump_base, pframe, file_name, &offset);
		return 0;
	}

	strcat(file_name1, file_name);
	if (dump_base->dcam_out_fmt == DCAM_STORE_YUV420) {
		strcat(file_name, "_yuv420.y");
		strcat(file_name1, "_yuv420.uv");
	} else {
		strcat(file_name, "_yvu420.y");
		strcat(file_name1, "_yvu420.uv");
	}

	addr = pframe->buf.addr_k[0] + offset;
	addr1 = addr + size;
	if (i == 0 && pframe->is_compressed && ((g_dbg_dump.dump_en == DUMP_PATH_BOTH)
		|| (g_dbg_dump.dump_en == DUMP_PATH_BIN)))
		offset += pframe->fbc_info.buffer_size;
	else
		offset += (size * 3 / 2);
	camdump_write_image_to_file((char *)addr, size, file_name);
	pr_debug("dump for ch %d, size %d, kaddr %p, file %s\n", dump_base->ch_id,
		(int)size, (void *)addr, file_name);
	size = size / 2;
	camdump_write_image_to_file((char *)addr1, size, file_name1);
	pr_debug("dump for ch %d, size %d, kaddr %p, file %s\n", dump_base->ch_id,
		(int)size, (void *)addr1, file_name1);
	cam_buf_kunmap(&pframe->buf);
	return 0;
}

static int camdump_pyr_frame_dump(struct cam_dump_ctx *dump_base, struct camera_frame *pframe)
{
	ssize_t size = 0;
	uint32_t i = 0;
	uint32_t align_w = 0, align_h = 0;
	uint32_t layer_loop = 0;

	if(dump_base == NULL || pframe == NULL)
		return 0;

	align_w = isp_rec_layer0_width(pframe->width, dump_base->pyr_layer_num);
	align_h = isp_rec_layer0_heigh(pframe->height, dump_base->pyr_layer_num);

	if (g_dbg_dump.dump_en == DUMP_ISP_PYR_REC)
		layer_loop = dump_base->pyr_layer_num;
	else
		layer_loop = dump_base->pyr_layer_num + 1;
	for (i = dump_base->start_layer; i < layer_loop; i++) {
		if (i == 0) {
			size = cal_sprd_yuv_pitch(pframe->width, dump_base->dcam_out_bits, dump_base->is_pack);
			size = size * pframe->height;
		} else {
			align_w = align_w /2;
			align_h = align_h /2;
			size = cal_sprd_yuv_pitch(align_w, dump_base->dcam_out_bits, dump_base->is_pack);
			size = size * align_h;
		}

		camdump_pyr_frame_store(dump_base, pframe,i,align_w,align_h,size);
	}

	return 0;
}

static int camdump_normal_frame_dump(struct cam_dump_ctx *dump_base, struct camera_frame *pframe)
{
	if (pframe->need_pyr_rec == 1 && pframe->channel_id == CAM_CH_PRE)
		camdump_pyr_frame_dump(dump_base, pframe);
	else
		camdump_one_frame_dump(dump_base, pframe);
	return 0;
}

int camdump_start(struct cam_thread_info* thrd_info, struct cam_dump_ctx *dump_base, uint32_t dcam_idx)
{
	struct cam_dbg_dump *dbg = &g_dbg_dump;
	unsigned long flag = 0;
	spin_lock_irqsave(&dbg->dump_lock, flag);
	if (!(dbg->dump_ongoing & (1 << dcam_idx))) {
		dump_base->dump_cfg = camdump_param_cfg;
		switch (g_dbg_dump.dump_en) {
		case DUMP_PATH_BOTH:
		case DUMP_PATH_FULL:
		case DUMP_PATH_BIN:
		case DUMP_PATH_RAW_BIN:
			dump_base->dump_enqueue = camdump_enqueue;
			dump_base->dump_file = camdump_normal_frame_dump;
			break;
		case DUMP_ISP_PYR_REC:
			dump_base->dump_enqueue = camdump_enqueue;
			dump_base->dump_file = camdump_pyr_frame_dump;
			break;
		case DUMP_ISP_PYR_DEC:
			dump_base->dump_enqueue = camdump_enqueue;
			dump_base->dump_file = camdump_pyr_frame_dump;
			break;
		case DUMP_DCAM_PDAF:
			dump_base->dump_enqueue = camdump_enqueue;
			dump_base->dump_file = camdump_pdaf_frame_dump;
			break;
		case DUMP_DCAM_OFFLINE:
			dump_base->dump_enqueue = camdump_enqueue;
			dump_base->dump_file = camdump_one_frame_dump;
			break;
		default:
			dump_base->dump_enqueue = NULL;
			dump_base->dump_file = NULL;
			pr_debug("dump_en:%d",g_dbg_dump.dump_en);
		}
		complete(&thrd_info->thread_com);
		dbg->dump_count = 99;
		pr_debug("trigger sdump capture raw mode %d\n", g_dbg_dump.dump_en);
	}
	spin_unlock_irqrestore(&dbg->dump_lock, flag);
	return 0;
}

int camdump_stop(struct cam_dump_ctx *dump_base)
{
	unsigned long flag = 0;
	spin_lock_irqsave(&g_dbg_dump.dump_lock, flag);
	dump_base->dump_count = 0;
	dump_base->dump_cfg = NULL;
	dump_base->dump_enqueue = NULL;
	dump_base->dump_file = NULL;
	spin_unlock_irqrestore(&g_dbg_dump.dump_lock, flag);
	complete(&dump_base->dump_com);
	return 0;
}