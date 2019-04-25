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

#include <linux/delay.h>
#include <video/sprd_mm.h>

#include "dcam_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "dcam_full: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int sprd_dcam_full_path_init(enum dcam_id idx)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc *full_path = sprd_dcam_drv_full_path_get(idx);

	memset(full_path, 0x00, sizeof(*full_path));
	full_path->id = CAMERA_FULL_PATH;
	rtn = sprd_cam_queue_buf_init(&full_path->buf_queue,
		sizeof(struct camera_frame),
		DCAM_BUF_QUEUE_LENGTH, "full path buf_queue");
	rtn |= sprd_cam_queue_frm_init(&full_path->frame_queue,
		sizeof(struct camera_frame),
		DCAM_FRM_QUEUE_LENGTH + 1, "full path frm_queue");
	return rtn;
}

int sprd_dcam_full_path_deinit(enum dcam_id idx)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc *full_path = sprd_dcam_drv_full_path_get(idx);

	if (DCAM_ADDR_INVALID(full_path)) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	sprd_cam_queue_frm_deinit(&full_path->frame_queue);
	sprd_cam_queue_buf_deinit(&full_path->buf_queue);
	memset(full_path, 0x00, sizeof(*full_path));

	return rtn;
}

int sprd_dcam_full_path_map(struct cam_buf_info *buf_info)
{
	int ret = 0;

	if (!buf_info) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	ret = sprd_cam_buf_sg_table_get(buf_info);
	if (ret) {
		pr_err("fail to get buf sg table\n");
		return ret;
	}
	ret = sprd_cam_buf_addr_map(buf_info);
	if (ret) {
		pr_err("fail to map buf addr\n");
		return ret;
	}

	return ret;
}

int sprd_dcam_full_path_unmap(enum dcam_id idx)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc *full_path = sprd_dcam_drv_full_path_get(idx);
	struct camera_frame *frame = NULL;
	struct camera_frame *res_frame = NULL;
	uint32_t i = 0;

	if (DCAM_ADDR_INVALID(full_path)) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	if (full_path->valid) {
		if (full_path->buf_num) {
			for (i = 0; i < full_path->ion_buf_cnt; i++) {
				frame = &full_path->ion_buffer[i];
				sprd_cam_buf_addr_unmap(&frame->buf_info);
			}
		} else {
			struct camera_frame frm;

			while (!sprd_cam_queue_frm_dequeue(
				&full_path->frame_queue, &frm)) {
				sprd_cam_buf_addr_unmap(&frm.buf_info);
				memset(&frm, 0x00, sizeof(frm));
			}
			while (!sprd_cam_queue_buf_read(
				&full_path->buf_queue, &frm)) {
				sprd_cam_buf_addr_unmap(&frm.buf_info);
				memset(&frm, 0x00, sizeof(frm));
			}
		}

		full_path->ion_buf_cnt = 0;
		res_frame = &full_path->reserved_frame;
		if (sprd_cam_buf_is_valid(&res_frame->buf_info))
			sprd_cam_buf_addr_unmap(&res_frame->buf_info);
		memset((void *)res_frame, 0x00, sizeof(*res_frame));
	}

	return rtn;
}

void sprd_dcam_full_path_clear(enum dcam_id idx)
{
	struct dcam_path_desc *full_path = sprd_dcam_drv_full_path_get(idx);

	sprd_cam_queue_buf_clear(&full_path->buf_queue);
	sprd_cam_queue_frm_clear(&full_path->frame_queue);
	full_path->valid = 0;
	full_path->status = DCAM_ST_STOP;
	full_path->output_frame_count = 0;
	full_path->ion_buf_cnt = 0;
}

int sprd_dcam_full_path_cfg_set(enum dcam_id idx,
	enum dcam_cfg_id id, void *param)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc *full_path = sprd_dcam_drv_full_path_get(idx);
	struct camera_addr *p_addr;

	if (DCAM_ADDR_INVALID(full_path)) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	if ((unsigned long)(param) == 0) {
		pr_err("fail to get valid input ptr\n");
		return -DCAM_RTN_PARA_ERR;
	}

	switch (id) {
	case DCAM_PATH_BUF_NUM:
	{
		uint32_t *src = (uint32_t *)param;

		full_path->buf_num = *src;
		break;
	}
	case DCAM_PATH_SRC_SEL:
	{
		uint32_t *src = (uint32_t *)param;

		full_path->src_sel = *src;
		full_path->valid_param.src_sel = 1;
		break;
	}
	case DCAM_PATH_INPUT_SIZE:
	{
		struct camera_size *size = (struct camera_size *)param;

		memcpy((void *)&full_path->input_size, (void *)size,
			sizeof(struct camera_size));
		break;
	}
	case DCAM_PATH_INPUT_RECT:
	{
		struct camera_rect *rect = (struct camera_rect *)param;

		memcpy((void *)&full_path->input_rect, (void *)rect,
			sizeof(struct camera_rect));
		break;
	}
	case DCAM_PATH_OUTPUT_FORMAT:
	{
		uint32_t *fmt = (uint32_t *)param;

		full_path->output_format = *fmt;
		full_path->valid_param.output_format = 1;
		break;
	}
	case DCAM_PATH_OUTPUT_LOOSE:
	{
		uint32_t *fmt = (uint32_t *)param;

		full_path->is_loose = *fmt;
		break;
	}
	case DCAM_PATH_OUTPUT_ADDR:
		p_addr = (struct camera_addr *)param;

		if (p_addr->buf_info.type == CAM_BUF_USER_TYPE
			&& DCAM_YUV_ADDR_INVALID(p_addr->yaddr,
					p_addr->uaddr,
					p_addr->vaddr) &&
					p_addr->mfd_y == 0) {
			pr_err("fail to get valid addr!\n");
			rtn = DCAM_RTN_PATH_ADDR_ERR;
		} else if (p_addr->buf_info.type != CAM_BUF_USER_TYPE
			&& (p_addr->buf_info.dmabuf_p[0] == NULL
			|| p_addr->buf_info.buf[0] == NULL)) {
			pr_err("fail to get valid addr!\n");
			rtn = DCAM_RTN_PATH_ADDR_ERR;
		} else {
			struct camera_frame frame;

			memset((void *)&frame, 0x00, sizeof(frame));
			frame.yaddr = p_addr->yaddr;
			frame.uaddr = p_addr->uaddr;
			frame.vaddr = p_addr->vaddr;
			frame.yaddr_vir = p_addr->yaddr_vir;
			frame.uaddr_vir = p_addr->uaddr_vir;
			frame.vaddr_vir = p_addr->vaddr_vir;

			frame.type = CAMERA_FULL_PATH;
			frame.fid = full_path->frame_base_id;

			frame.buf_info = p_addr->buf_info;
			frame.buf_info.idx = idx;
			frame.buf_info.dev = &s_dcam_pdev->dev;
			frame.buf_info.mfd[0] = p_addr->mfd_y;
			frame.buf_info.mfd[1] = p_addr->mfd_u;
			frame.buf_info.mfd[2] = p_addr->mfd_v;
			frame.buf_info.offset[0] = p_addr->yaddr;
			frame.buf_info.offset[1] = p_addr->uaddr;
			frame.buf_info.offset[2] = p_addr->vaddr;
			frame.buf_info.num = 1;

			if (!(p_addr->buf_info.state
				& CAM_BUF_STATE_MAPPING_DCAM)) {
				rtn = sprd_dcam_full_path_map(&frame.buf_info);
				if (rtn) {
					pr_err("fail to map dcam full path!\n");
					rtn = DCAM_RTN_PATH_ADDR_ERR;
					break;
				}
			}

			if (!sprd_cam_queue_buf_write(&full_path->buf_queue,
						&frame))
				full_path->output_frame_count++;
			if (full_path->ion_buf_cnt < full_path->buf_num) {
				memcpy(&full_path->ion_buffer
					[full_path->ion_buf_cnt], &frame,
					sizeof(struct camera_frame));
				full_path->addr_4in1
					[full_path->ion_buf_cnt].mfd =
					frame.buf_info.mfd[0];
				full_path->addr_4in1
					[full_path->ion_buf_cnt].iova =
					frame.buf_info.iova[0];
				full_path->ion_buf_cnt++;
			}

			DCAM_TRACE("y=0x%x u=0x%x v=0x%x mfd=0x%x 0x%x\n",
				p_addr->yaddr, p_addr->uaddr,
				p_addr->vaddr, frame.buf_info.mfd[0],
				frame.buf_info.mfd[1]);
		}
		break;
	case DCAM_PATH_OUTPUT_RESERVED_ADDR:
		p_addr = (struct camera_addr *)param;

		if (p_addr->buf_info.type == CAM_BUF_USER_TYPE
			&& DCAM_YUV_ADDR_INVALID(p_addr->yaddr,
					p_addr->uaddr,
					p_addr->vaddr) &&
					p_addr->mfd_y == 0) {
			pr_err("fail to get valid addr!\n");
			rtn = DCAM_RTN_PATH_ADDR_ERR;
		} else if (p_addr->buf_info.type != CAM_BUF_USER_TYPE
			&& (p_addr->buf_info.dmabuf_p[0] == NULL
			|| p_addr->buf_info.buf[0] == NULL)) {
			pr_err("fail to get valid addr!\n");
			rtn = DCAM_RTN_PATH_ADDR_ERR;
		} else {
			struct camera_frame *frame = NULL;

			frame = &full_path->reserved_frame;

			memset((void *)frame, 0x00, sizeof(*frame));
			frame->yaddr = p_addr->yaddr;
			frame->uaddr = p_addr->uaddr;
			frame->vaddr = p_addr->vaddr;
			frame->yaddr_vir = p_addr->yaddr_vir;
			frame->uaddr_vir = p_addr->uaddr_vir;
			frame->vaddr_vir = p_addr->vaddr_vir;

			frame->buf_info = p_addr->buf_info;
			frame->buf_info.idx = idx;
			frame->buf_info.dev = &s_dcam_pdev->dev;
			frame->buf_info.mfd[0] = p_addr->mfd_y;
			frame->buf_info.mfd[1] = p_addr->mfd_u;
			frame->buf_info.mfd[2] = p_addr->mfd_v;
			frame->buf_info.num = 1;

			/*may need update iommu here*/
			rtn = sprd_cam_buf_sg_table_get(&frame->buf_info);
			if (rtn) {
				pr_err("fail to cfg reserved output addr!\n");
				rtn = DCAM_RTN_PATH_ADDR_ERR;
				break;
			}
			rtn = sprd_cam_buf_addr_map(&frame->buf_info);

			DCAM_TRACE("y=0x%x u=0x%x v=0x%x mfd=0x%x 0x%x\n",
				p_addr->yaddr, p_addr->uaddr,
				p_addr->vaddr, p_addr->mfd_y,
				p_addr->mfd_u);
		}
		break;
	case DCAM_PATH_DATA_ENDIAN:
	{
		struct camera_endian_sel *endian
			= (struct camera_endian_sel *)param;

		if (endian->y_endian >= DCAM_ENDIAN_MAX) {
			rtn = DCAM_RTN_PATH_ENDIAN_ERR;
		} else {
			full_path->data_endian.y_endian = endian->y_endian;
			full_path->valid_param.data_endian = 1;
		}
		break;
	}
	case DCAM_PATH_ENABLE:
		full_path->valid = *(uint32_t *)param;
		break;
	case DCAM_PATH_ASSOC:
		full_path->assoc_idx = *(uint32_t *)param;
		break;
	default:
		pr_err("fail to cfg dcam path\n");
		break;
	}

	return -rtn;
}

int sprd_dcam_full_path_next_frm_set(enum dcam_id idx)
{
	int use_reserve_frame = 0;
	unsigned long addr[2];
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct camera_frame frame;
	struct camera_frame *reserved_frame = NULL;
	struct dcam_path_desc *full_path = sprd_dcam_drv_full_path_get(idx);
	struct dcam_module *module = sprd_dcam_drv_module_get(idx);
	struct cam_frm_queue *p_heap = NULL;

	if (DCAM_ADDR_INVALID(full_path) || DCAM_ADDR_INVALID(module)) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	if (module->slice_info[0].dcam_cap.slice_en && DCAM_ID_1 == idx)
		return rtn;

	memset((void *)&frame, 0x00, sizeof(frame));
	reserved_frame = &full_path->reserved_frame;
	p_heap = &full_path->frame_queue;

	if (sprd_cam_queue_buf_read(&full_path->buf_queue, &frame) == 0 &&
		sprd_cam_buf_is_valid(&frame.buf_info)) {
		full_path->output_frame_count--;
	} else {
		pr_info("DCAM%d: No free frame id %d\n",
			idx, module->frame_id);
		if (!sprd_cam_buf_is_valid(&reserved_frame->buf_info)) {
			pr_info("DCAM%d: No need to cfg frame buffer\n", idx);
			return -1;
		}
		memcpy(&frame, reserved_frame, sizeof(struct camera_frame));
		use_reserve_frame = 1;
	}

	if (frame.buf_info.dev == NULL)
		pr_info("DCAM%d next dev NULL %p\n", idx, frame.buf_info.dev);
	if (rtn) {
		pr_err("fail to get path addr\n");
		return rtn;
	}
	addr[0] = frame.buf_info.iova[0] + frame.yaddr;
	addr[1] = frame.buf_info.iova[0] + frame.uaddr;

	DCAM_REG_WR(idx, DCAM_FULL_BASE_WADDR, addr[0]);
	if (module->slice_info[0].dcam_cap.slice_en && DCAM_ID_0 == idx)
		DCAM_REG_WR(DCAM_ID_1, DCAM_FULL_BASE_WADDR, addr[0] +
			module->slice_info[0].full_path.output_size.w * 10 / 8);
	if (full_path->output_format != DCAM_RAWRGB)
		DCAM_REG_WR(idx, DCAM_BIN_BASE_WADDR0, addr[1]);

	frame.frame_id = module->frame_id;

	pr_info("DCAM%d: frame id %d reserved %d iova[0]=0x%x mfd=0x%x 0x%x\n",
		idx, frame.frame_id, use_reserve_frame, (int)addr[0],
		frame.buf_info.mfd[0], frame.yaddr);

	if (sprd_cam_queue_frm_enqueue(p_heap, &frame) == 0)
		DCAM_TRACE("success to enq frame buf\n");
	else
		rtn = DCAM_RTN_PATH_FRAME_LOCKED;

	return -rtn;
}

int sprd_dcam_full_path_start(enum dcam_id idx)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc *full_path = sprd_dcam_drv_full_path_get(idx);
	uint32_t reg_val = 0;
	unsigned long addr = 0;
	struct camera_rect *rect = NULL;
	int path_eb = BIT_0;

	if (DCAM_ADDR_INVALID(full_path)) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	if (full_path->valid) {
		if (full_path->valid_param.data_endian) {
			sprd_dcam_drv_glb_reg_mwr(idx, DCAM_PATH_ENDIAN,
				BIT_1 | BIT_0,
				full_path->data_endian.y_endian << 0,
				DCAM_ENDIAN_REG);
			DCAM_TRACE("data_endian y=0x%x\n",
				full_path->data_endian.y_endian);
		}

		if (full_path->valid_param.src_sel) {
			DCAM_REG_MWR(idx, DCAM_FULL_CFG,
				BIT_4, full_path->src_sel << 4);
		}

		if (full_path->input_size.w != full_path->input_rect.w
			|| full_path->input_size.h != full_path->input_rect.h) {
			pr_info("set crop_eb\n");

			DCAM_REG_MWR(idx, DCAM_FULL_CFG, BIT_1, BIT_1);

			addr = DCAM_FULL_CROP_START;
			rect = &full_path->input_rect;
			reg_val = (rect->x & 0x1FFF) |
				((rect->y & 0x1FFF) << 16);
			DCAM_REG_WR(idx, addr, reg_val);

			addr = DCAM_FULL_CROP_SIZE;
			reg_val = ((rect->x + rect->w - 1) & 0x1FFF) |
				(((rect->y + rect->h - 1) & 0x1FFF) << 16);
			DCAM_REG_WR(idx, addr, reg_val);
			full_path->output_size.w = rect->w;
			full_path->output_size.h = rect->h;
		} else {
			full_path->output_size.w = full_path->input_size.w;
			full_path->output_size.h = full_path->input_size.h;
		}

		if (full_path->output_format == DCAM_RAWRGB)
			DCAM_REG_MWR(idx, DCAM_FULL_CFG,
				BIT_3 | BIT_2, full_path->is_loose << 2);

		rtn = sprd_dcam_full_path_next_frm_set(idx);
		if (rtn) {
			pr_err("fail to set next frame\n");
			return -(rtn);
		}
		pr_info("output_size.w = %d, pitch = %d\n",
			full_path->output_size.w, full_path->pitch);
		DCAM_REG_MWR(idx, DCAM_FULL_CFG,
			0x3ff << 20,
			(full_path->pitch & 0x3ff) << 20);
		DCAM_REG_MWR(idx, DCAM_FULL_CFG, BIT_0, path_eb);
		sprd_dcam_drv_force_copy(idx, FULL_COPY);

		full_path->need_wait = 0;
		full_path->status = DCAM_ST_START;

		if (full_path->output_format == DCAM_YUV420) {
			DCAM_REG_MWR(idx, DCAM_CAM_BIN_CFG, BIT_0, 0x1);
			sprd_dcam_drv_force_copy(idx, BIN_COPY);
		}
	}

	return rtn;
}

void sprd_dcam_full_path_quickstop(enum dcam_id idx)
{
	uint32_t ret = 0;
	int time_out = 5000;
	struct dcam_path_desc *full_path = sprd_dcam_drv_full_path_get(idx);

	if (DCAM_ADDR_INVALID(full_path)) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	if (full_path->valid == 0) {
		pr_info("DCAM%d: path is not valid\n", idx);
		return;
	}

	sprd_dcam_drv_glb_reg_owr(idx, DCAM_PATH_STOP, BIT_0, DCAM_CONTROL_REG);
	DCAM_REG_MWR(idx, DCAM_FULL_CFG, BIT_0, 0);
	sprd_dcam_drv_force_copy(idx, FULL_COPY);
	udelay(1000);

	/* wait for AHB path busy cleared */
	while (time_out) {
		ret = DCAM_REG_RD(idx, DCAM_PATH_BUSY) & BIT_0;
		if (!ret)
			break;
		time_out--;
	}
	if (!time_out)
		pr_info("DCAM%d: stop path time out\n", idx);

	full_path->status = DCAM_ST_STOP;
	DCAM_TRACE("path stop end!\n");
}

int sprd_dcam_full_path_buf_reset(enum dcam_id idx)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_path_desc *full_path = sprd_dcam_drv_full_path_get(idx);
	struct camera_frame *frame = NULL;
	uint32_t i = 0;

	if (DCAM_ADDR_INVALID(full_path)) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	sprd_cam_queue_buf_clear(&full_path->buf_queue);
	sprd_cam_queue_frm_clear(&full_path->frame_queue);

	for (i = 0; i < full_path->buf_num; i++) {
		frame = &full_path->ion_buffer[i];
		rtn = sprd_cam_queue_buf_write(&full_path->buf_queue, frame);
		if (rtn)
			pr_err("fail to write buffer queue\n");
	}

	return rtn;
}
