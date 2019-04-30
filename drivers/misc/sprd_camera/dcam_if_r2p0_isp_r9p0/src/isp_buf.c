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

#include "isp_buf.h"
#include <linux/sprd_ion.h>
#include <linux/sprd_iommu.h>

#include "isp_3dnr_drv.h"

#define ION
#ifdef ION
#include "ion.h"
#include "ion_priv.h"
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_BUF: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static void sprd_ispbuf_cfg_buf_int(struct isp_cfg_ctx_desc *cfg_ctx,
			void *phy_addr, void *vir_addr)
{
	int scene_id = 0, num = 0;
	unsigned long base = 0, offset = 0;
	struct isp_cfg_buf *ion_buf = NULL;

	for (scene_id = 0; scene_id < ISP_SCENE_MAX; scene_id++) {
		ion_buf = &cfg_ctx->cfg_buf[scene_id];
		base = scene_id * ISP_CFG_BUF_SIZE;
		for (num = 0; num < ISP_CFG_BUF_NUM; num++) {
			offset = num * ISP_REG_SIZE;
			ion_buf->cmd_buf[num].phy_addr =
				(void *)(((unsigned long)phy_addr)
				+ base + offset);
			ion_buf->cmd_buf[num].vir_addr =
				(void *)(((unsigned long)vir_addr)
				+ base + offset);
		}
	}
}

int sprd_isp_buf_cfg_iommu_map(struct isp_cfg_ctx_desc *cfg_ctx)
{
	int rtn = ISP_RTN_SUCCESS;
	void *phy_addr = NULL;
	void *vir_addr = NULL;

	rtn = sprd_cam_buf_addr_map(&cfg_ctx->buf_info);
	if (rtn) {
		pr_err("fail to ctx_buf_iommu_map\n");
		return rtn;
	}
	phy_addr = (void *)ALIGN((unsigned long)cfg_ctx->buf_info.iova[0],
		ISP_REG_SIZE);
	vir_addr =  (unsigned long) cfg_ctx->buf_info.kaddr[0] +
		(phy_addr - (unsigned long)cfg_ctx->buf_info.iova[0]);
	sprd_ispbuf_cfg_buf_int(cfg_ctx, phy_addr, vir_addr);
	return rtn;
}

void sprd_isp_buf_frm_clear(struct isp_pipe_dev *dev,
	enum isp_path_index path_index)
{
	int ret = 0;
	struct camera_frame frame;
	struct camera_frame *res_frame = NULL;
	struct isp_path_desc *path;
	struct isp_module *module = NULL;

	if (!dev)
		return;

	module = &dev->module_info;
	if (ISP_PATH_IDX_PRE & path_index) {
		path = &module->isp_path[ISP_SCL_PRE];
		while (!sprd_cam_queue_frm_dequeue(
			&path->frame_queue, &frame)) {
			sprd_cam_buf_addr_unmap(&frame.buf_info);
			memset(&frame, 0x00, sizeof(frame));
		}

		sprd_cam_queue_frm_clear(&path->frame_queue);
		ret = sprd_cam_queue_buf_clear(&path->buf_queue);
		if (unlikely(ret != 0))
			pr_err("fail to clear queue\n");
		ret = sprd_cam_queue_buf_clear(&path->coeff_queue);
		if (unlikely(ret != 0))
			pr_err("fail to clear zoom queue\n");

		res_frame = &module->path_reserved_frame[ISP_SCL_PRE];
		if (res_frame->buf_info.mfd[0] != 0
				&& res_frame->buf_info.iova[0])
			sprd_cam_buf_addr_unmap(&res_frame->buf_info);

		memset((void *)res_frame, 0x00, sizeof(*res_frame));

	}

	if (ISP_PATH_IDX_VID & path_index) {
		path = &module->isp_path[ISP_SCL_VID];
		while (!sprd_cam_queue_frm_dequeue(
			&path->frame_queue, &frame)) {
			sprd_cam_buf_addr_unmap(&frame.buf_info);
			memset(&frame, 0x00, sizeof(frame));
		}

		sprd_cam_queue_frm_clear(&path->frame_queue);
		ret = sprd_cam_queue_buf_clear(&path->buf_queue);
		if (unlikely(ret != 0))
			pr_err("fail to clear queue\n");
		ret = sprd_cam_queue_buf_clear(&path->coeff_queue);
		if (unlikely(ret != 0))
			pr_err("fail to clear zoom queue\n");

		res_frame = &module->path_reserved_frame[ISP_SCL_VID];
		if (res_frame->buf_info.mfd[0] != 0
				&& res_frame->buf_info.iova[0])
			sprd_cam_buf_addr_unmap(&res_frame->buf_info);

		memset((void *)res_frame, 0x00, sizeof(*res_frame));
	}

	if (ISP_PATH_IDX_CAP & path_index) {
		path = &module->isp_path[ISP_SCL_CAP];
		while (!sprd_cam_queue_frm_dequeue(
			&path->frame_queue, &frame)) {
			sprd_cam_buf_addr_unmap(&frame.buf_info);
			memset(&frame, 0x00, sizeof(frame));
		}

		sprd_cam_queue_frm_clear(&path->frame_queue);
		ret = sprd_cam_queue_buf_clear(&path->buf_queue);
		if (unlikely(ret != 0))
			pr_err("fail to clear queue\n");
		ret = sprd_cam_queue_buf_clear(&path->coeff_queue);
		if (unlikely(ret != 0))
			pr_err("fail to clear zoom queue\n");
		res_frame = &module->path_reserved_frame[ISP_SCL_CAP];
		if (res_frame->buf_info.mfd[0] != 0
				&& res_frame->buf_info.iova[0])
			sprd_cam_buf_addr_unmap(&res_frame->buf_info);

		memset((void *)res_frame, 0x00, sizeof(*res_frame));
	}
}
