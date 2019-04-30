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
#define pr_fmt(fmt) "dcam_fetch: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int sprd_dcam_fetch_cfg_set(enum dcam_id idx, enum dcam_cfg_id id, void *param)
{
	enum dcam_drv_rtn rtn = DCAM_RTN_SUCCESS;
	struct dcam_fetch_desc *fetch_desc = sprd_dcam_drv_fetch_get(idx);
	struct camera_addr *p_addr = NULL;
	int time_out = 5000;

	if (DCAM_ADDR_INVALID(fetch_desc)) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	switch (id) {
	case DCAM_FETCH_DATA_PACKET:
		{
			uint32_t is_loose = *(uint32_t *)param;

			DCAM_AXIM_MWR(REG_DCAM_IMG_FETCH_CTRL,
				BIT(1), is_loose << 1);
			fetch_desc->is_loose = is_loose;
			break;
		}
	case DCAM_FETCH_DATA_ENDIAN:
		{
			uint32_t endian = *(uint32_t *)param;

			DCAM_AXIM_MWR(REG_DCAM_IMG_FETCH_CTRL,
				0x03 << 2, endian << 2);
			break;
		}
	case DCAM_FETCH_INPUT_RECT:
		{
			struct camera_rect *rect = (struct camera_rect *)param;
			uint32_t tmp = 0;

			if (rect->x > DCAM_CAP_FRAME_WIDTH_MAX ||
				rect->y > DCAM_CAP_FRAME_HEIGHT_MAX ||
				rect->w > DCAM_CAP_FRAME_WIDTH_MAX ||
				rect->h > DCAM_CAP_FRAME_HEIGHT_MAX) {
				rtn = DCAM_RTN_CAP_FRAME_SIZE_ERR;
				return -rtn;
			}
			tmp = (rect->h << 16) | rect->w;
			DCAM_AXIM_WR(REG_DCAM_IMG_FETCH_SIZE, tmp);
			fetch_desc->input_rect = *rect;

			tmp = sprd_cam_com_raw_pitch_calc(fetch_desc->is_loose,
				rect->w) / 4;
			DCAM_AXIM_WR(REG_DCAM_IMG_FETCH_X, rect->x);
			DCAM_AXIM_MWR(REG_DCAM_IMG_FETCH_X,
					0x0fff << 16, tmp << 16);
			break;
		}
	case DCAM_FETCH_INPUT_ADDR:
		p_addr = (struct camera_addr *)param;

		if (p_addr->buf_info.type == CAM_BUF_USER_TYPE
			&& DCAM_YUV_ADDR_INVALID(p_addr->yaddr,
					p_addr->uaddr,
					p_addr->vaddr) &&
					p_addr->mfd_y == 0) {
			pr_err("fail to get valid addr!\n");
			rtn = DCAM_RTN_PATH_ADDR_ERR;
		} else if (p_addr->buf_info.type != CAM_BUF_USER_TYPE
			&& (p_addr->buf_info.client[0] == NULL
			|| p_addr->buf_info.handle[0] == NULL)) {
			pr_err("fail to get valid addr!\n");
			rtn = DCAM_RTN_PATH_ADDR_ERR;
		} else {
			struct camera_frame *frame = &fetch_desc->frame;
			uint32_t addr = 0;

			memset((void *)frame, 0x00, sizeof(*frame));
			frame->yaddr = p_addr->yaddr;
			frame->uaddr = p_addr->uaddr;
			frame->vaddr = p_addr->vaddr;
			frame->yaddr_vir = p_addr->yaddr_vir;
			frame->uaddr_vir = p_addr->uaddr_vir;
			frame->vaddr_vir = p_addr->vaddr_vir;

			frame->buf_info = p_addr->buf_info;
			frame->buf_info.mfd[0] = p_addr->mfd_y;
			frame->buf_info.mfd[1] = p_addr->mfd_u;
			frame->buf_info.mfd[2] = p_addr->mfd_v;
			frame->buf_info.offset[0] = p_addr->yaddr;
			frame->buf_info.offset[1] = p_addr->uaddr;
			frame->buf_info.offset[2] = p_addr->vaddr;

			/*may need update iommu here*/
			rtn = sprd_cam_buf_sg_table_get(&frame->buf_info);
			if (rtn) {
				pr_err("fail to cfg input addr!\n");
				rtn = DCAM_RTN_PATH_ADDR_ERR;
				break;
			}
			rtn = sprd_cam_buf_addr_map(&frame->buf_info);
			if (rtn) {
				pr_err("fail to cfg input addr!\n");
				rtn = DCAM_RTN_PATH_ADDR_ERR;
				break;
			}
			addr = frame->buf_info.iova[0] + frame->yaddr;
			DCAM_AXIM_WR(REG_DCAM_IMG_FETCH_RADDR,
					addr);
			DCAM_TRACE("y=0x%x mfd=0x%x\n",
				p_addr->yaddr, frame->buf_info.mfd[0]);
			DCAM_TRACE("fetch addr 0x%x\n",
				(uint32_t)addr);
		}
		break;
	case DCAM_FETCH_START:
	{
		uint32_t start = *(uint32_t *)param;

		if (start) {
			DCAM_AXIM_MWR(REG_DCAM_IMG_FETCH_CTRL,
				BIT(16), 0 << 16);
			DCAM_REG_MWR(idx, DCAM_MIPI_CAP_CFG, BIT(0), BIT(0));
			DCAM_AXIM_MWR(REG_DCAM_IMG_FETCH_CTRL,
				0x0F << 12, 0x0F << 12);
			DCAM_AXIM_MWR(REG_DCAM_IMG_FETCH_CTRL,
				0xFF << 4, 0xFF << 4);
			DCAM_AXIM_WR(REG_DCAM_IMG_FETCH_START, BIT(0));
		} else {
			DCAM_AXIM_MWR(REG_DCAM_IMG_FETCH_CTRL,
				BIT(16), 1 << 16);
			udelay(1000);

			while (time_out) {
				rtn = DCAM_AXIM_RD(REG_DCAM_IMG_FETCH_CTRL)
					& BIT(17);
				if (!rtn)
					break;
				time_out--;
			}
			if (!time_out)
				pr_info("fail to stop fetch timeout\n");
		}
		break;
	}
	default:
		rtn = DCAM_RTN_IO_ID_ERR;
		break;
	}

	return -rtn;
}
