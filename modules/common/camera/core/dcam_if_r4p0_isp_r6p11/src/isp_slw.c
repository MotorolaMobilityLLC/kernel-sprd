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

#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include "isp_int.h"
#include "isp_buf.h"
#include "isp_statis_buf.h"
#include "isp_path.h"
#include "isp_slice.h"
#include "isp_slw.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_SLW: %d " fmt, __LINE__

int slowmotion_frame_enqueue(struct isp_slw_queue *queue,
			     struct isp_slw_info *slw)
{
	if (DCAM_ADDR_INVALID(queue) || DCAM_ADDR_INVALID(slw)) {
		pr_err("fail to get valid parm %p, %p\n", queue, slw);
		return -1;
	}

	if (queue->valid_cnt >= ISP_SLW_BUF_NUM) {
		pr_info("slw q over flow\n");
		return -1;
	}

	memcpy(&queue->slw_array[queue->valid_cnt], slw,
	       sizeof(struct isp_slw_info));
	queue->valid_cnt++;
	pr_debug("slw en queue, %d\n", queue->valid_cnt);

	return 0;
}

int slowmotion_frame_dequeue(struct isp_slw_queue *queue,
			     struct isp_slw_info *slw)
{
	uint32_t i = 0;

	if (DCAM_ADDR_INVALID(queue) || DCAM_ADDR_INVALID(slw)) {
		pr_err("fail to get valid parm %p, %p\n", queue, slw);
		return -1;
	}

	if (queue->valid_cnt == 0) {
		DCAM_TRACE("slw q under flow\n");
		return -1;
	}

	memcpy(slw, &queue->slw_array[0], sizeof(struct isp_slw_info));
	queue->valid_cnt--;
	for (i = 0; i < queue->valid_cnt; i++) {
		memcpy(&queue->slw_array[i], &queue->slw_array[i + 1],
		       sizeof(struct isp_slw_info));
	}
	pr_info("slw de queue, %d\n", queue->valid_cnt);

	return 0;
}

void slowmotion_frame_queue_clear(struct isp_slw_queue *queue)
{
	if (ISP_ADDR_INVALID(queue)) {
		pr_err("fail to get valid heap %p\n", queue);
		return;
	}

	memset((void *)queue, 0, sizeof(struct isp_slw_queue));
}

void isp_slw_queue_clear(struct isp_slw_queue *p_queue,
			 struct camera_frame *res_frame)
{
	/*
	 * Remove it because slow motion using CFG mode now. Notice
	 * that current stack frame in this function is exceed kernel
	 * default limit since isp_slw_info contains large frame queue.
	 */
#if 0
	enum isp_slw_rtn rtn = ISP_RTN_SLW_SUCCESS;
	struct isp_slw_info p_from_queue;
	struct camera_frame frame;

	if (!p_queue)
		pr_err("fail to get Input param is NULL\n");

	while (!slowmotion_frame_dequeue(p_queue, &p_from_queue)) {
		if (p_from_queue.is_reserved == 0) {
			rtn = isp_frame_dequeue(&p_from_queue.slw_queue,
				&frame);
			if (rtn == 0 && frame.pfinfo.mfd[0] !=
					res_frame->pfinfo.mfd[0])
				pfiommu_free_addr(&frame.pfinfo);
		}
		isp_frm_queue_clear(&p_from_queue.slw_queue);
	}
#endif
}

void isp_slw_clear(void *handle)
{
	struct isp_pipe_dev *dev = NULL;
	struct isp_slw_queue *p_queue = NULL;
	struct isp_fmcu_slw_desc *fmcu_slw = NULL;
	struct isp_fmcu_slw_info *slw_handle = NULL;
	struct isp_slw_info *p_reserved = NULL;
	struct camera_frame *res_frame = NULL;

	if (!handle) {
		pr_err("fail to get Input,dev ptr is NULL\n");
		return;
	}

	dev = (struct isp_pipe_dev *)handle;
	fmcu_slw = &dev->fmcu_slw;
	slw_handle = fmcu_slw->slw_handle;
	fmcu_slw->slw_flags = ISP_NORMAL_VIDEO;
	fmcu_slw->status = ISP_ST_STOP;
	res_frame = &dev->module_info.path_reserved_frame[ISP_SCL_VID];

	p_queue = &slw_handle->empty_queue;
	isp_slw_queue_clear(p_queue, res_frame);
	slowmotion_frame_queue_clear(p_queue);

	p_queue = &slw_handle->embed_queue;
	isp_slw_queue_clear(p_queue, res_frame);
	slowmotion_frame_queue_clear(p_queue);

	p_queue = &slw_handle->insert_queue;
	isp_slw_queue_clear(p_queue, res_frame);
	slowmotion_frame_queue_clear(p_queue);

	p_reserved = &slw_handle->slw_reserved;
	isp_frm_queue_clear(&p_reserved->slw_queue);

}

int set_pingpang_reg(uint32_t idx, struct isp_slw_info *p_from_embed)
{
	enum isp_slw_rtn rtn = ISP_RTN_SLW_SUCCESS;
	uint32_t addr = 0;

	if ((!p_from_embed) || (!p_from_embed->fmcu_addr_vir))
		return -EINVAL;

	/* add set fmcu addr&num here*/
#ifdef CONFIG_64BIT
	__flush_dcache_area(p_from_embed->fmcu_addr_vir, PAGE_SIZE);
#else
	flush_kernel_vmap_range(p_from_embed->fmcu_addr_vir, PAGE_SIZE);
#endif

	addr = ISP_FMCU_DDR_ADR;
	ISP_REG_WR(idx, addr, p_from_embed->fmcu_addr_phy);
	addr = ISP_FMCU_CTRL;
	ISP_REG_MWR(idx, addr, 0xFFFF0000, p_from_embed->fmcu_num << 16);

	return rtn;
}

int set_isp_fmcu_int_reg(void *isp_handle)
{
	uint32_t idx;
	enum isp_slw_rtn rtn = ISP_RTN_SLW_SUCCESS;
	uint32_t reg = 0;
	struct isp_pipe_dev *dev = NULL;
	struct isp_fmcu_slw_desc *fmcu_slw = NULL;
	struct isp_fmcu_slw_info *slw_handle = NULL;
	struct isp_int_info *sdw_done_info = NULL;

	if (!isp_handle) {
		pr_err("fail to get Input param ptr is NULL\n");
		return -ENODEV;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	idx = dev->com_idx;
	fmcu_slw = &dev->fmcu_slw;
	slw_handle = fmcu_slw->slw_handle;
	sdw_done_info = &slw_handle->sdw_done_info;

	reg = ISP_INT_SKIP_CTRL;
	ISP_REG_MWR(idx, reg, BIT_17,
		    sdw_done_info->sdw_done_skip_cnt_clr << 17);
	ISP_REG_MWR(idx, reg, 0XF000000,
		    sdw_done_info->sdw_done_int_cnt_num << 24);
	ISP_REG_MWR(idx, reg, 0xF00000,
		    sdw_done_info->sdw_done_skip_cnt_num << 20);
	ISP_REG_MWR(idx, reg, BIT_16,
		    sdw_done_info->sdw_done_skip_en << 16);

	reg = ISP_INT_SKIP_CTRL1;
	ISP_REG_MWR(idx, reg, BIT_1,
		    sdw_done_info->vid_done_skip_cnt_clr << 1);
	ISP_REG_MWR(idx, reg, 0XF00,
		    sdw_done_info->vid_done_int_cnt_num << 8);
	ISP_REG_MWR(idx, reg, 0xF0,
		    sdw_done_info->vid_done_skip_cnt_num << 4);
	ISP_REG_MWR(idx, reg, BIT_0,
		    sdw_done_info->vid_done_skip_en);

	return rtn;
}

void set_isp_fmcu_start_reg(uint32_t idx)
{
	ISP_REG_MWR(idx, ISP_FMCU_START, BIT_0, 1);
}

int set_isp_fmcu_cmd_reg(enum isp_scl_id path_id, void *isp_handle)
{
	/*
	 * Remove it because slow motion using CFG mode now. Notice
	 * that current stack frame in this function is exceed kernel
	 * default limit since isp_slw_info contains large frame queue.
	 */
#if 0
	enum isp_slw_rtn rtn = ISP_RTN_SLW_SUCCESS;
	uint32_t idx;
	struct isp_slw_info p_from_embed;
	struct isp_pipe_dev *dev = NULL;
	struct isp_slw_queue *p_insert_queue = NULL;
	struct isp_slw_queue *p_embed_queue = NULL;
	struct isp_fmcu_slw_desc *fmcu_slw = NULL;
	struct isp_fmcu_slw_info *slw_handle = NULL;

	if (!isp_handle) {
		pr_err("fail to get Input param ptr is NULL\n");
		return -ENODEV;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	idx = dev->com_idx;
	fmcu_slw = &dev->fmcu_slw;
	slw_handle = fmcu_slw->slw_handle;

	if (path_id != ISP_SCL_VID) {
		pr_err("fail to get valid pathid %s rnt %d\n", __func__, rtn);
		return -EFAULT;
	}

	p_embed_queue = &slw_handle->embed_queue;
	rtn = slowmotion_frame_dequeue(p_embed_queue, &p_from_embed);
	if (rtn) {
		p_from_embed = slw_handle->slw_reserved;
		if (p_from_embed.is_reserved == 0) {
			pr_err("fail to dequeue %s rtn %d\n", __func__, rtn);
			return rtn;
		}
	}
	rtn = set_pingpang_reg(idx, &p_from_embed);
	if (rtn) {
		pr_err("fail to set reg %s rnt%d\n", __func__, rtn);
		return rtn;
	}

	p_insert_queue = &slw_handle->insert_queue;
	rtn = slowmotion_frame_enqueue(p_insert_queue, &p_from_embed);
	if (rtn) {
		pr_err("fail to enque %s rtn %d\n", __func__, rtn);
		return rtn;
	}

	return rtn;
#else
	return ISP_RTN_SLW_SUCCESS;
#endif
}

int isp_slw_buf_error(struct isp_slw_queue *p_empty_queue,
		      struct isp_slw_info *p_from_empty,
		      struct isp_buf_queue *p_buf_queue)
{
	enum isp_slw_rtn rtn = ISP_RTN_SLW_SUCCESS;
	struct camera_frame frame;
	struct isp_frm_queue *p_frm_queue = NULL;

	if (!p_empty_queue || !p_from_empty || !p_buf_queue) {
		pr_err("fail to get Input param ptr is NULL\n");
		return -EINVAL;
	}

	p_frm_queue = &p_from_empty->slw_queue;
	while (!isp_frame_dequeue(p_frm_queue, &frame))
		isp_buf_queue_write(p_buf_queue, &frame);

	slowmotion_frame_enqueue(p_empty_queue, p_from_empty);

	return rtn;
}

void set_fmcu_cmd(enum isp_id iid, uint32_t index, struct camera_frame *frame,
		  uint32_t u_offset, struct isp_slw_info *p_from_empty)
{
	struct isp_slw_cmd *slw_cmd = NULL;
	uint32_t num = 0, addr = 0, cmd = 0;
	uint32_t shadow_done_cmd[] = {0x10, 0x13};

	if (!p_from_empty) {
		pr_err("fail to get Input param ptr is NULL\n");
		return;
	}

	slw_cmd = (struct isp_slw_cmd *)p_from_empty->fmcu_addr_vir;

	addr = ISP_GET_REG(iid, ISP_STORE_VID_BASE+ISP_STORE_SLICE_Y_ADDR);
	cmd = frame->pfinfo.iova[0];
	slw_cmd->cmd_frm[index].yaddr_frm = cmd;
	slw_cmd->cmd_frm[index].yaddr_reg = addr;
	num++;

	addr = ISP_GET_REG(iid, ISP_STORE_VID_BASE+ISP_STORE_SLICE_U_ADDR);
	cmd = frame->pfinfo.iova[1] + u_offset;
	slw_cmd->cmd_frm[index].uaddr_frm = cmd;
	slw_cmd->cmd_frm[index].uaddr_reg = addr;
	num++;

	addr = ISP_GET_REG(iid, ISP_STORE_VID_BASE+ISP_STORE_SLICE_V_ADDR);
	cmd = frame->pfinfo.iova[2];
	slw_cmd->cmd_frm[index].vaddr_frm = cmd;
	slw_cmd->cmd_frm[index].vaddr_reg = addr;
	num++;

	addr = ISP_GET_REG(iid, ISP_FMCU_CMD);
	cmd = shadow_done_cmd[0];
	slw_cmd->cmd_frm[index].shadow_frm = cmd;
	slw_cmd->cmd_frm[index].shadow_reg = addr;
	num++;

	p_from_empty->fmcu_num += num;

}

int fmcu_slw_cmd(void *isp_handle,
	enum isp_scl_id path_id, uint32_t buf_reserved)
{
	/*
	 * Remove it because slow motion using CFG mode now. Notice
	 * that current stack frame in this function is exceed kernel
	 * default limit since isp_slw_info contains large frame queue.
	 */
#if 0
	enum isp_slw_rtn rtn = ISP_RTN_SLW_SUCCESS;
	enum isp_id iid;
	uint32_t i = 0, u_offset = 0;
	struct isp_slw_info p_from_empty;
	struct camera_frame frame;
	struct isp_pipe_dev *dev = NULL;
	struct isp_path_desc *path = NULL;
	struct isp_module *module = NULL;
	struct isp_frm_queue *p_frm_queue = NULL;
	struct isp_buf_queue *p_buf_queue = NULL;
	struct isp_slw_queue *p_empty_queue = NULL;
	struct isp_slw_queue *p_embed_queue = NULL;
	struct isp_fmcu_slw_desc *fmcu_slw = NULL;
	struct isp_fmcu_slw_info *slw_handle = NULL;

	if (!isp_handle) {
		pr_err("fail to get Input param ptr is NULL\n");
		return -ENODEV;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	iid = ISP_GET_IID(dev->com_idx);
	module = &dev->module_info;
	path = &module->isp_path[path_id];
	p_buf_queue = &path->buf_queue;

	fmcu_slw = &dev->fmcu_slw;
	slw_handle = fmcu_slw->slw_handle;
	u_offset = path->dst.h * path->dst.w;
	p_empty_queue = &slw_handle->empty_queue;
	p_embed_queue = &slw_handle->embed_queue;

	if (unlikely(buf_reserved)) {
		memcpy(&frame, &module->path_reserved_frame[path_id],
				sizeof(struct camera_frame));
		memcpy(&p_from_empty, &slw_handle->slw_reserved,
				sizeof(struct isp_slw_info));
		if (frame.pfinfo.mfd[0] == 0)
			return -ISP_RTN_SLW_FRM_ERR;
	} else {
		if (slowmotion_frame_dequeue(p_empty_queue,
			&p_from_empty) != 0) {
			pr_err("fail to deque %s rtn %d", __func__, rtn);
			return -ISP_RTN_SLW_DQ_ERR;
		}
	}

	p_frm_queue = &p_from_empty.slw_queue;
	for (i = 0; i < ISP_SLW_FRM_NUM; i++) {
		if (buf_reserved == 0) {
			if (isp_buf_queue_read(p_buf_queue, &frame) != 0) {
				pr_err("fail to read que,no free buf");
				isp_slw_buf_error(p_empty_queue,
				&p_from_empty, p_buf_queue);
				return -ISP_RTN_SLW_FRM_ERR;
			}
			if (frame.pfinfo.mfd[0] == 0) {
				i--;
				continue;
			}
		}
		if (frame.pfinfo.dev == NULL)
			pr_info("ISP%d next dev NULL %p\n",
						iid, frame.pfinfo.dev);
		if (pfiommu_get_addr(&frame.pfinfo) != 0) {
			pr_err("fail to get frame iommu address!\n");
			return ISP_RTN_PATH_ADDR_ERR;
		}
		set_fmcu_cmd(iid, i, &frame, u_offset, &p_from_empty);

		if (isp_frame_enqueue(p_frm_queue, &frame) != 0) {
			pr_err("fail to enque %s rtn %d", __func__, rtn);
			return -ISP_RTN_SLW_EQ_ERR;
		}
	}

	if (buf_reserved == 0) {
		p_from_empty.is_reserved = 0;
		if (slowmotion_frame_enqueue(p_embed_queue,
						&p_from_empty) != 0) {
			pr_err("fail to enque %s rtn %d", __func__, rtn);
			return -1;
		}
	} else {
		slw_handle->slw_reserved.is_reserved = 1;
		slw_handle->slw_reserved.fmcu_num = p_from_empty.fmcu_num;
	}

	return rtn;
#else
	return ISP_RTN_SLW_SUCCESS;
#endif

}

int isp_fmcu_slw_int_init(void *isp_handle)
{
	enum isp_slw_rtn rtn = ISP_RTN_SLW_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_int_info *int_info_ptr = NULL;
	struct isp_fmcu_slw_desc *fmcu_slw = NULL;
	struct isp_fmcu_slw_info *slw_hanle = NULL;

	if (!isp_handle) {
		pr_err("fail to get valid param, NULL\n");
		return -ENODEV;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	fmcu_slw = &dev->fmcu_slw;
	slw_hanle = fmcu_slw->slw_handle;
	int_info_ptr = &slw_hanle->sdw_done_info;

	int_info_ptr->sdw_done_int_cnt_num = 0;
	int_info_ptr->sdw_done_skip_cnt_clr = 1;
	int_info_ptr->sdw_done_skip_cnt_num = 4;
	int_info_ptr->sdw_done_skip_en = 1;

	int_info_ptr->vid_done_int_cnt_num = 3;
	int_info_ptr->vid_done_skip_cnt_clr = 1;
	int_info_ptr->vid_done_skip_cnt_num = 4;
	int_info_ptr->vid_done_skip_en = 1;

	return rtn;
}

int isp_fmcu_slw_queue_init(void *isp_handle)
{
	/*
	 * Remove it because slow motion using CFG mode now. Notice
	 * that current stack frame in this function is exceed kernel
	 * default limit since isp_slw_info contains large frame queue.
	 */
#if 0
	enum isp_slw_rtn rtn = ISP_RTN_SLW_SUCCESS;
	uint32_t i;
	struct isp_slw_info p_from_empty;
	struct isp_pipe_dev *dev = NULL;
	struct isp_slw_cmd *fmcu_addr = NULL;
	struct isp_slw_info *p_reserved = NULL;
	struct isp_fmcu_slw_desc *fmcu_slw = NULL;
	struct isp_fmcu_slw_info *slw_hanle = NULL;
	struct isp_slw_queue *p_empty_queue = NULL;

	if (!isp_handle) {
		pr_err("fail to get valid param, NULL\n");
		return -ENODEV;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	fmcu_slw = &dev->fmcu_slw;
	slw_hanle = fmcu_slw->slw_handle;
	p_empty_queue = &slw_hanle->empty_queue;
	p_reserved = &slw_hanle->slw_reserved;
	if (!fmcu_slw->fmcu_addr_vir) {
		pr_err("fail to get fmcu_addr_vir\n");
		return -EFAULT;
	}
	fmcu_addr = (struct isp_slw_cmd *)fmcu_slw->fmcu_addr_vir;
	fmcu_addr = (struct isp_slw_cmd *)((~(FMCU_ALIGN - 1))
		& ((unsigned long)fmcu_addr + (FMCU_ALIGN - 1)));
	memset(&p_from_empty, 0, sizeof(struct isp_slw_info));
	slowmotion_frame_queue_clear(p_empty_queue);
	for (i = 0; i < ISP_SLW_BUF_NUM ; i++) {
		memset(&p_from_empty, 0, sizeof(struct isp_slw_info));
		p_from_empty.fmcu_addr_vir = (unsigned long *)fmcu_addr;
		p_from_empty.fmcu_addr_phy = virt_to_phys(fmcu_addr);
		p_from_empty.fmcu_num = 0;
		p_from_empty.is_reserved = 0;
		isp_frm_queue_clear(&p_from_empty.slw_queue);
		rtn = slowmotion_frame_enqueue(p_empty_queue, &p_from_empty);
		if (rtn) {
			pr_err("fail to enque %s rtn %d", __func__, rtn);
			return -ISP_RTN_SLW_EQ_ERR;
		}
		fmcu_addr++;
		fmcu_addr = (struct isp_slw_cmd *)((~(FMCU_ALIGN - 1))
			& ((unsigned long)fmcu_addr + (FMCU_ALIGN - 1)));

	}
	p_reserved->fmcu_addr_vir = (unsigned long *)fmcu_addr;
	p_reserved->fmcu_addr_phy = virt_to_phys(fmcu_addr);
	p_reserved->fmcu_num = 0;
	p_reserved->is_reserved = 0;
	isp_frm_queue_clear(&p_reserved->slw_queue);

	return rtn;
#else
	return ISP_RTN_SLW_SUCCESS;
#endif
}

int isp_slw_flags_init(void *isp_handle, struct isp_path_info *info)
{
	int ret = 0;
	struct isp_pipe_dev *dev = NULL;

	if (!isp_handle || !info) {
		pr_err("fail to get valid param, NULL\n");
		return -ENODEV;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	if (info->is_slow_motion == ISP_NORMAL_VIDEO) {
		dev->fmcu_slw.slw_flags = ISP_NORMAL_VIDEO;
		dev->fmcu_slw.vid_num = 0;
	} else if (info->is_slow_motion == ISP_SLW_VIDEO) {
		dev->fmcu_slw.slw_flags = ISP_SLW_VIDEO;
		if (!info->fmcu_addr_vir) {
			pr_err("fail to get fmcu_addr_vir\n");
			return -EFAULT;
		}
		dev->fmcu_slw.fmcu_addr_vir = info->fmcu_addr_vir;
	}

	return ret;
}

int get_slw_status(void *isp_handle)
{
	struct isp_pipe_dev *dev = NULL;
	uint32_t slw_flags;

	if (!isp_handle) {
		pr_err("fail to get valid param, NULL\n");
		return -ENODEV;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	slw_flags = dev->fmcu_slw.status;

	return slw_flags;
}

int set_isp_fmcu_slw_cmd(void *isp_handle, enum isp_scl_id path_id,
			 uint32_t buf_reserved)
{
	enum isp_slw_rtn rtn = ISP_RTN_SLW_SUCCESS;
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;
	struct isp_path_desc *path = NULL;
	struct isp_buf_queue *p_buf_queue = NULL;

	if (!isp_handle) {
		pr_err("fail to get valid param, NULL\n");
		return -ENODEV;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->module_info;
	path = &module->isp_path[path_id];
	p_buf_queue = &path->buf_queue;

	if (buf_reserved) {
		rtn = fmcu_slw_cmd(isp_handle, path_id, buf_reserved);
	} else {
		while (p_buf_queue->valid_cnt > 0) {
			if (fmcu_slw_cmd(isp_handle, path_id,
					 buf_reserved) != 0) {
				pr_info("no enough queue or buffer\n ");
				break;
			}
		}
	}

	return rtn;
}

int isp_fmcu_slw_start(enum isp_scl_id path_id, void *isp_handle)
{
	uint32_t idx;
	enum isp_slw_rtn rtn = ISP_RTN_SLW_SUCCESS;
	struct isp_pipe_dev *dev = NULL;

	if (!isp_handle) {
		pr_err("fail to get valid param, NULL\n");
		return -ENODEV;
	}

	dev = (struct  isp_pipe_dev *)isp_handle;
	idx = dev->com_idx;

	rtn = set_isp_fmcu_int_reg(isp_handle);
	if (rtn) {
		pr_err("fail to init fmcu reg %s rtn %d", __func__, rtn);
		return rtn;
	}

	rtn = set_isp_fmcu_cmd_reg(path_id, isp_handle);
	if (rtn) {
		pr_err("fail to set fmcu reg %s rnt %d", __func__, rtn);
		return rtn;
	}
	set_isp_fmcu_start_reg(idx);
	dev->fmcu_slw.status = ISP_ST_START;

	return rtn;
}

int set_fmcu_slw_cfg(void *handle)
{
	enum isp_slw_rtn rtn = ISP_RTN_SLW_SUCCESS;
	uint32_t buf_reserved;
	struct isp_pipe_dev *dev = NULL;

	if (!handle) {
		pr_err("fail to get Input param ptr is NULL\n");
		return -ENODEV;
	}

	dev = (struct isp_pipe_dev *)handle;
	dev->fmcu_slw.status = ISP_ST_STOP;

	rtn = isp_fmcu_slw_int_init(handle);
	if (rtn) {
		pr_err("fail to init fmcu %s rnt %d", __func__, rtn);
		return rtn;
	}

	rtn = isp_fmcu_slw_queue_init(handle);
	if (rtn) {
		pr_err("fail to init queue %s rtn %d", __func__, rtn);
		return rtn;
	}

	buf_reserved = 1;
	rtn = set_isp_fmcu_slw_cmd(handle, ISP_SCL_VID, buf_reserved);
	if (rtn) {
		pr_err("fail to set fmcu cmd %s rtn %d", __func__, rtn);
		return rtn;
	}

	buf_reserved = 0;
	rtn = set_isp_fmcu_slw_cmd(handle, ISP_SCL_VID, buf_reserved);
	if (rtn) {
		pr_err("fail to set %s rtn %d", __func__, rtn);
		return rtn;
	}

	return rtn;
}

int isp_fmcu_slw_init(void **fmcu_handler)
{
	int rtn = 0;
	struct isp_fmcu_slw_info *cxt = NULL;

	if (!fmcu_handler) {
		pr_err("fail to get Input param ptr is NULL\n");
		rtn = -1;
		goto exit;
	}

	*fmcu_handler = NULL;
	cxt = vzalloc(sizeof(struct isp_fmcu_slw_info));
	*fmcu_handler = (void *)cxt;

exit:
	return rtn;
}

int isp_fmcu_slw_deinit(void *fmcu_handler)
{
	int rtn = 0;
	struct isp_fmcu_slw_info *cxt = NULL;

	if (!fmcu_handler) {
		pr_err("fail to get Input param ptr is NULL\n");
		return -1;
	}

	cxt = (struct isp_fmcu_slw_info *)fmcu_handler;
	if (cxt != NULL)
		vfree(cxt);

	return rtn;
}

