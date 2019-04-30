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

#include <linux/interrupt.h>

#include "isp_int.h"
#include "isp_buf.h"
#include "isp_path.h"
#include "isp_block.h"
#include "cam_common.h"

#define ION
#ifdef ION
#include "ion.h"
#include "ion_priv.h"
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ISP_INT: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static isp_isr_func p_user_func[ISP_ID_MAX][ISP_IMG_MAX];
static void *p_user_data[ISP_ID_MAX][ISP_IMG_MAX];
static spinlock_t isp_irq0_lock[ISP_MAX_COUNT];
static spinlock_t isp_irq1_lock[ISP_MAX_COUNT];

static const uint32_t isp_irq_p0[] = {
	ISP_INT_ISP_ALL_DONE,
	ISP_INT_SHADOW_DONE,
	ISP_INT_STORE_DONE_PRE,
	ISP_INT_STORE_DONE_VID,
	ISP_INT_NR3_ALL_DONE,
	ISP_INT_NR3_SHADOW_DONE,
	ISP_INT_FMCU_CONFIG_DONE,
	ISP_INT_FMCU_CMD_ERROR,
	ISP_INT_HIST_DONE,
};

static const uint32_t isp_irq_p1[] = {
	ISP_INT_ISP_ALL_DONE,
	ISP_INT_SHADOW_DONE,
	ISP_INT_STORE_DONE_PRE,
	ISP_INT_STORE_DONE_VID,
	ISP_INT_NR3_ALL_DONE,
	ISP_INT_NR3_SHADOW_DONE,
	ISP_INT_FMCU_CONFIG_DONE,
	ISP_INT_FMCU_CMD_ERROR,
	ISP_INT_HIST_DONE,
};

static const uint32_t isp_irq_c0[] = {
	ISP_INT_ISP_ALL_DONE,
	ISP_INT_SHADOW_DONE,
	ISP_INT_STORE_DONE_PRE,
	ISP_INT_NR3_ALL_DONE,
	ISP_INT_NR3_SHADOW_DONE,
	ISP_INT_FMCU_CONFIG_DONE,
	ISP_INT_FMCU_CMD_ERROR,
};

static const uint32_t isp_irq_c1[] = {
	ISP_INT_ISP_ALL_DONE,
	ISP_INT_SHADOW_DONE,
	ISP_INT_STORE_DONE_PRE,
	ISP_INT_NR3_ALL_DONE,
	ISP_INT_NR3_SHADOW_DONE,
	ISP_INT_FMCU_CONFIG_DONE,
	ISP_INT_FMCU_CMD_ERROR,
};

static void sprd_ispint_iommu_reg_trace(enum isp_id id)
{
	unsigned long addr = 0;

	for (addr = ISP_MMU_INT_EN;
		addr <= ISP_MMU_SECURITY_EN ; addr += 16) {
		pr_info(" [ ISP_MMU ] 0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(id, ISP_MMU_INT_BASE + addr),
			ISP_REG_RD(id, ISP_MMU_INT_BASE + addr + 4),
			ISP_REG_RD(id, ISP_MMU_INT_BASE + addr + 8),
			ISP_REG_RD(id, ISP_MMU_INT_BASE + addr + 12));
	}
	pr_info("\n");
	pr_info("\n");

	for (addr = ISP_3DNR_START;
		addr <= ISP_3DNR_END ; addr += 16) {
		pr_info(" [ 3DNR ] 0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(id, ISP_3DNR_BASE + addr),
			ISP_REG_RD(id, ISP_3DNR_BASE + addr + 4),
			ISP_REG_RD(id, ISP_3DNR_BASE + addr + 8),
			ISP_REG_RD(id, ISP_3DNR_BASE + addr + 12));
	}
	pr_info("\n");
	pr_info("\n");

	for (addr = ISP_3DNR_BLEND_START;
		addr <= ISP_3DNR_BLEND_END ; addr += 16) {
		pr_info(" [ 3DNR_BLEND ] 0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(id, ISP_3DNR_BLEND_BASE + addr),
			ISP_REG_RD(id, ISP_3DNR_BLEND_BASE + addr + 4),
			ISP_REG_RD(id, ISP_3DNR_BLEND_BASE + addr + 8),
			ISP_REG_RD(id, ISP_3DNR_BLEND_BASE + addr + 12));
	}
	pr_info("\n");
	pr_info("\n");

	for (addr = ISP_3DNR_STORE_START;
		addr <= ISP_3DNR_STORE_END ; addr += 16) {
		pr_info(" [ 3DNR_STORE ] 0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(id, ISP_3DNR_STORE_BASE + addr),
			ISP_REG_RD(id, ISP_3DNR_STORE_BASE + addr + 4),
			ISP_REG_RD(id, ISP_3DNR_STORE_BASE + addr + 8),
			ISP_REG_RD(id, ISP_3DNR_STORE_BASE + addr + 12));
	}
	pr_info("\n");
	pr_info("\n");

	for (addr = ISP_3DNR_CROP_START;
		addr <= ISP_3DNR_CROP_END ; addr += 16) {
		pr_info(" [ 3DNR_CROP ] 0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(id, ISP_3DNR_CROP_BASE + addr),
			ISP_REG_RD(id, ISP_3DNR_CROP_BASE + addr + 4),
			ISP_REG_RD(id, ISP_3DNR_CROP_BASE + addr + 8),
			ISP_REG_RD(id, ISP_3DNR_CROP_BASE + addr + 12));
	}
	pr_info("\n");
	pr_info("\n");

	for (addr = 0x10;
		addr <= ISP_FETCH_END ; addr += 16) {
		pr_info(" [ FETCH ] 0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(id, ISP_FETCH_BASE + addr),
			ISP_REG_RD(id, ISP_FETCH_BASE + addr + 4),
			ISP_REG_RD(id, ISP_FETCH_BASE + addr + 8),
			ISP_REG_RD(id, ISP_FETCH_BASE + addr + 12));
	}
	pr_info("\n");
	pr_info("\n");

	for (addr = ISP_STORE_PARAM;
		addr <= ISP_STORE_SHADOW_CLR ; addr += 16) {
		pr_info(" [ VIDEO ] 0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(id, ISP_STORE_VID_BASE + addr),
			ISP_REG_RD(id, ISP_STORE_VID_BASE + addr + 4),
			ISP_REG_RD(id, ISP_STORE_VID_BASE + addr + 8),
			ISP_REG_RD(id, ISP_STORE_VID_BASE + addr + 12));
	}
	pr_info("\n");
	pr_info("\n");

	for (addr = ISP_STORE_PARAM;
		addr <= ISP_STORE_SHADOW_CLR ; addr += 16) {
		pr_info(" [ PREV/CAP ] 0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(id, ISP_STORE_PRE_CAP_BASE + addr),
			ISP_REG_RD(id, ISP_STORE_PRE_CAP_BASE + addr + 4),
			ISP_REG_RD(id, ISP_STORE_PRE_CAP_BASE + addr + 8),
			ISP_REG_RD(id, ISP_STORE_PRE_CAP_BASE + addr + 12));
	}
}

static int sprd_ispint_err_pre_proc(uint32_t idx)
{
	unsigned long addr = 0;

	pr_info("fmcu cmd error Register list\n");
	for (addr = 0x0988; addr <= 0x0998 ; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(idx, addr),
			ISP_REG_RD(idx, addr + 4),
			ISP_REG_RD(idx, addr + 8),
			ISP_REG_RD(idx, addr + 12));
	}
	pr_info("C0 interrupt status\n");
	for (addr = 0x0e00; addr <= 0x0e3c ; addr += 16) {
		pr_info("0x%lx: 0x%x 0x%x 0x%x 0x%x\n",
			addr,
			ISP_REG_RD(idx, addr),
			ISP_REG_RD(idx, addr + 4),
			ISP_REG_RD(idx, addr + 8),
			ISP_REG_RD(idx, addr + 12));
	}

	return 0;
}

static void sprd_ispint_path_done(enum isp_id idx, enum isp_scl_id path_id,
	void *isp_handle)
{
	int  ret = 0;
	void *data;
	isp_isr_func user_func;
	enum isp_irq_id img_id = ISP_PATH_PRE_DONE;
	struct camera_frame frame;
	struct isp_path_desc *path;
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;

	if (!isp_handle) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	if (path_id >= ISP_SCL_MAX) {
		pr_err("fail to get valid img_id %d\n.", path_id);
		return;
	}

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->module_info;
	path = &module->isp_path[path_id];
	if (path->valid == 0) {
		pr_err("fail to get isp%d valid path%d\n", idx, path_id);
		return;
	}

	if (path_id == ISP_SCL_PRE)
		img_id = ISP_PATH_PRE_DONE;
	else if (path_id == ISP_SCL_VID)
		img_id = ISP_PATH_VID_DONE;
	else if (path_id == ISP_SCL_CAP)
		img_id = ISP_PATH_CAP_DONE;
	else {
		pr_err("fail to get valid isp path id %d.\n", path_id);
		return;
	}
	user_func = p_user_func[idx][img_id];
	data = p_user_data[idx][img_id];

	ret = sprd_cam_queue_frm_dequeue(&path->frame_queue, &frame);
	if (ret) {
		pr_err("fail to dequeue frame queue.\n");
		return;
	}

	if (frame.buf_info.dev == NULL)
		pr_err("fail to done ISP%d dev %p\n",
			idx, frame.buf_info.dev);
	sprd_cam_buf_addr_unmap(&frame.buf_info);

	if (!sprd_cam_buf_is_equal(&frame.buf_info,
		&path->path_reserved_frame.buf_info)) {
		frame.width = path->dst.w;
		frame.height = path->dst.h;
		frame.irq_type = CAMERA_IRQ_IMG;
		pr_debug("ISP%d: path%d frame %p\n",
			idx, path_id, &frame);
		if (user_func)
			user_func(&frame, data);
	} else {
		pr_debug("isp%d: path %d use reserved 0x%x 0x%x\n",
			idx, path_id, frame.buf_info.mfd[0],
			frame.buf_info.offset[0]);
		module->path_reserved_frame[path_id].buf_info.iova[0] = 0;
		module->path_reserved_frame[path_id].buf_info.iova[1] = 0;
	}
	pr_debug("isp%d path%d done.\n", idx, img_id);
}

static void sprd_ispint_dcam_frame_free(enum isp_id idx, void *isp_handle)
{
	int ret = 0;
	struct isp_pipe_dev *dev = (struct isp_pipe_dev *)isp_handle;
	struct isp_module *module = NULL;
	struct camera_frame *pframe = NULL;
	struct camera_frame frame;

	module = &dev->module_info;

	if (sprd_cam_queue_frm_firstnode_get(&module->bin_frm_queue,
		(void **)&pframe) != 0) {
		pr_err("fail to get bin frm frame\n");
		return;
	}

	if (atomic_dec_return(&pframe->usr_cnt) != 0)
		return;

	/*write this buffer back to DCAM */
	if (sprd_cam_queue_frm_dequeue(&module->bin_frm_queue, &frame)) {
		pr_err("fail to dequeue bin frame\n");
		return;
	}

	if (dev->sn_mode == DCAM_CAP_MODE_YUV
		&& module->isp_path[ISP_SCL_CAP].valid) {
		sprd_cam_buf_addr_unmap(&frame.buf_info);
		sprd_isp_path_offline_frame_set(
			isp_handle,
			CAMERA_FULL_PATH, &frame);
		return;
	}

	sprd_cam_buf_addr_unmap(&frame.buf_info);

	if (sprd_cam_queue_frm_enqueue(&module->bin_zsl_queue, &frame)) {
		pr_err("fail to enqueue bin zsl frame\n");
		return;
	}

	if (module->bin_zsl_queue.valid_cnt > 1) {
		if (sprd_cam_queue_frm_dequeue(
			&module->bin_zsl_queue, &frame)) {
			pr_err("fail to dequeue bin zsl queue\n");
			return;
		}
		ret = sprd_cam_ioctl_addr_write_back(dev->isp_handle_addr,
			CAMERA_BIN_PATH, &frame);
	}

	if (ret)
		pr_err("fail to set frame back to dcam.\n");
}

static void sprd_ispint_store_pre_done(enum isp_id idx, void *isp_handle)
{
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->module_info;

	if (module->isp_path[ISP_SCL_PRE].valid) {
		sprd_ispint_path_done(idx, ISP_SCL_PRE, dev);
		sprd_ispint_dcam_frame_free(idx, isp_handle);
	}
}

static void sprd_ispint_store_vid_done(enum isp_id idx, void *isp_handle)
{
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->module_info;

	if (module->isp_path[ISP_SCL_VID].valid) {
		sprd_ispint_path_done(idx, ISP_SCL_VID, dev);
		sprd_ispint_dcam_frame_free(idx, isp_handle);
	}
}

static void sprd_ispint_store_cap_done(enum isp_id idx, void *isp_handle)
{
#ifdef ISP_DRV_DEBUG
	pr_info("isp%d ispint_store_cap_done.\n", idx);
#endif
}

static int sprd_ispint_hist_statistic_get(enum isp_id idx,
	struct cam_statis_buf *node, struct ion_buffer *ionbuffer)
{
	int ret = 0;
	int i = 0;
	int max_item = ISP_HIST_ITEMS;
	unsigned long HIST_BUF = ISP_HIST_BUF0_CH0;
	uint64_t *hist_statis = NULL;
	uint32_t isp_core_pmu_en = 0;
#ifdef CONFIG_64BIT
	hist_statis =
		(uint64_t *)(((unsigned long)node->kaddr[0]) |
		((uint64_t)(node->kaddr[1] << 32)));
#else
	hist_statis = (uint64_t *)(node->kaddr[0]);
#endif
	if (IS_ERR_OR_NULL(hist_statis)) {
		ret = -1;
		pr_err("fail to alloc memory\n");
		return ret;
	}
	/*Close the CLK gate dynamic switch of ISP block,*/
	/*to get the value of hist correctly.*/
	isp_core_pmu_en = ISP_HREG_RD(idx, ISP_PMU_EN);
	ISP_HREG_WR(idx, ISP_PMU_EN, 0xffff0000);

	if (ionbuffer && (ionbuffer->kmap_cnt > 0)) {
		for (i = 0; i < max_item; i++) {
			hist_statis[i] = ISP_HREG_RD(idx, HIST_BUF + i * 4);
			pr_debug("ISP%d hist %ld statis[%d] %lu\n",
				 idx, HIST_BUF, i,
				 (unsigned long)hist_statis[i]);
		}
	} else {
		ret = -1;
		pr_err("fail to access hist memory ionbuffer %p\n", ionbuffer);
		return ret;
	}
	ISP_HREG_WR(idx, ISP_PMU_EN, isp_core_pmu_en);

	return ret;
}

static void sprd_ispint_hist_done(enum isp_id idx, void *isp_handle)
{
	enum isp_drv_rtn rtn = ISP_RTN_SUCCESS;
	void *data;
	isp_isr_func user_func;
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;
	struct cam_frm_queue *statis_heap = NULL;
	struct cam_statis_buf node;
	struct camera_frame frame_info;
	struct cam_statis_module *statis_module = NULL;
	enum isp_irq_id img_id = ISP_PATH_PRE_DONE;
	struct timeval tv;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->module_info;
	statis_module = &module->statis_module_info;
	statis_heap = &statis_module->hist_statis_frm_queue;

	memset(&node, 0x00, sizeof(node));
	memset(&frame_info, 0x00, sizeof(frame_info));

	img_id = ISP_HIST_DONE;
	user_func = p_user_func[idx][img_id];
	data = p_user_data[idx][img_id];

	/*dequeue the statis buf from a array*/
	rtn = sprd_cam_queue_frm_dequeue(statis_heap, &node);
	if (rtn)
		return;

	rtn = sprd_ispint_hist_statistic_get(idx, &node,
			statis_module->img_statis_buf.buf_info.buf[0]);
	if (rtn) {
		pr_err("fail to read hist statistic info\n");
		return;
	}
	frame_info.buf_size = node.buf_size;
	memcpy(frame_info.buf_info.mfd, node.buf_info.mfd,
		sizeof(node.buf_info.mfd));
	frame_info.phy_addr = node.phy_addr;
	frame_info.vir_addr = node.vir_addr;
	frame_info.kaddr[0] = node.kaddr[0];
	frame_info.kaddr[1] = node.kaddr[1];
	frame_info.addr_offset = node.addr_offset;
	frame_info.frame_id = node.frame_id;
	frame_info.irq_type = CAMERA_IRQ_STATIS;
	frame_info.irq_property = IRQ_HIST_STATIS;
	sprd_cam_com_timestamp(&tv);
	frame_info.timestamp = tv.tv_sec * 1000000000LL
		+ tv.tv_usec * 1000;
	pr_debug("frame_info.frame_id %u timestamp %lu\n",
		frame_info.frame_id, frame_info.timestamp);

	if (user_func)
		user_func(&frame_info, data);

}

static void sprd_ispint_fmcu_config_done(enum isp_id idx, void *isp_handle)
{
	int ret = 0;
	void *data;
	isp_isr_func user_func;
	uint32_t dual_capture_state = 0;
	enum dcam_id id = ISP_ID_0;
	enum isp_scl_id path_id = ISP_SCL_CAP;
	struct isp_pipe_dev *dev = NULL;
	struct isp_pipe_dev *dev_dual = NULL;
	struct isp_path_desc *path_pre = NULL;
	struct isp_path_desc *path_vid = NULL;
	struct isp_path_desc *path_cap = NULL;
	struct isp_fmcu_slice_desc *fmcu_slice = NULL;
	struct camera_frame frame;
	struct isp_module *module = NULL;
	struct isp_nr3_param *nr3_info = NULL;
	struct sprd_img_capture_param capture_param;

	if (!isp_handle) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	memset(&frame, 0x00, sizeof(frame));
	memset(&capture_param, 0x00, sizeof(capture_param));

	id = idx;
	dev = (struct isp_pipe_dev *)isp_handle;

	fmcu_slice = &dev->fmcu_slice;
	module = &dev->module_info;
	path_pre = &module->isp_path[ISP_SCL_PRE];
	path_vid = &module->isp_path[ISP_SCL_VID];
	path_cap = &module->isp_path[ISP_SCL_CAP];
	nr3_info = &path_cap->nr3_param;

	if (path_cap->valid) {
		if (!nr3_info->need_3dnr || (nr3_info->need_3dnr
			&& nr3_info->cur_cap_frame == ISP_3DNR_NUM)) {
			user_func = p_user_func[idx][ISP_PATH_CAP_DONE];
			data = p_user_data[idx][ISP_PATH_CAP_DONE];
			ret = sprd_cam_queue_frm_dequeue(
				&path_cap->frame_queue, &frame);
			if (ret) {
				pr_debug("fail to dequeue isp%d cap frame\n",
					idx);
				return;
			}
			sprd_cam_buf_addr_unmap(&frame.buf_info);
			if (!sprd_cam_buf_is_equal(&frame.buf_info,
				&path_cap->path_reserved_frame.buf_info)) {
				frame.width = path_cap->dst.w;
				frame.height = path_cap->dst.h;
				frame.irq_type = CAMERA_IRQ_IMG;
				if (dev->is_raw_capture == 1) {
					frame.irq_type = CAMERA_IRQ_DONE;
					frame.irq_property = IRQ_RAW_CAP_DONE;
				}

				if (user_func)
					(*user_func)(&frame, data);
			} else {
				pr_info("isp%d: use reserved cap\n", idx);
				module->path_reserved_frame
					[path_id].buf_info.iova[0] = 0;
				module->path_reserved_frame
					[path_id].buf_info.iova[1] = 0;
			}
		}
		sprd_cam_buf_addr_unmap(
			&dev->offline_frame[ISP_SCENE_CAP].buf_info);
		if (dev->is_raw_capture || dev->need_4in1)
			ret = sprd_cam_ioctl_addr_write_back(
				dev->isp_handle_addr, CAMERA_BIN_PATH,
				&dev->offline_frame[ISP_SCENE_CAP]);
		else
			ret = sprd_cam_ioctl_addr_write_back(
				dev->isp_handle_addr, CAMERA_FULL_PATH,
				&dev->offline_frame[ISP_SCENE_CAP]);
		if (ret)
			pr_err("fail to set back capture buf\n");
	}

	if (dev->is_raw_capture == 1) {
		path_cap->valid = 0;
		path_cap->status = ISP_ST_STOP;
		dev->is_raw_capture = 0;
		s_isp_group.dual_cap_sts = 0;
		sprd_isp_drv_stop(dev, 1);
		return;
	}

	if (dev->cap_flag == DCAM_CAPTURE_START_HDR
		|| dev->cap_flag == DCAM_CAPTURE_START_3DNR) {
		s_isp_group.dual_cap_sts = 0;
		capture_param.type = DCAM_CAPTURE_START;
		if (sprd_isp_drv_cap_start(isp_handle, capture_param, 1)) {
			pr_err("fail to start slice capture\n");
			return;
		}
	} else {
		while (!sprd_cam_queue_frm_dequeue(
			&module->full_zsl_queue, &frame)) {
			sprd_cam_ioctl_addr_write_back(dev->isp_handle_addr,
				CAMERA_FULL_PATH, &frame);
		}

		dev->isp_offline_state = ISP_ST_START;
		if (s_isp_group.dual_cam) {
			dev_dual = (struct isp_pipe_dev *)
				s_isp_group.isp_dev[id ^ 1];

			dual_capture_state = dev_dual->fmcu_slice.capture_state;
			fmcu_slice->capture_state = ISP_ST_STOP;
			if (++s_isp_group.dual_cap_cnt == 1) {
				pr_info("start another cap and buf in dual cam!\n");
				if (s_isp_group.first_need_wait) {
					pr_info("post dual first frame done.\n");
					s_isp_group.dual_cap_sts = 0;
					capture_param.type = DCAM_CAPTURE_START;
					if (dual_capture_state != ISP_ST_STOP
						&& sprd_isp_drv_cap_start(
						dev_dual, capture_param, 1)) {
						pr_err("fail to start slice capture\n");
						return;
					}
				}
				s_isp_group.first_need_wait = 0;
			} else if (s_isp_group.dual_cap_cnt == 2) {
				sprd_dcam_full_path_buf_reset(DCAM_ID_0);
				sprd_cam_queue_frm_clear(&module
					->full_zsl_queue);
				sprd_dcam_full_path_next_frm_set(DCAM_ID_0);

				sprd_dcam_full_path_buf_reset(DCAM_ID_1);
				sprd_cam_queue_frm_clear(&dev_dual
					->module_info.full_zsl_queue);
				sprd_dcam_full_path_next_frm_set(DCAM_ID_1);

				dev->frame_id = 0;
				dev_dual->frame_id = 0;
				s_isp_group.dual_fullpath_stop = 0;
				s_isp_group.dual_cap_cnt = 0;
				s_isp_group.dual_sel_cnt = 0;
				s_isp_group.dual_cap_total++;
				module->isp_state &= ~ISP_ZSL_QUEUE_LOCK;
				dev_dual->module_info.isp_state &=
					~ISP_ZSL_QUEUE_LOCK;

				sprd_dcam_drv_path_resume(DCAM_ID_0,
					CAMERA_FULL_PATH);
				sprd_dcam_drv_path_resume(DCAM_ID_1,
					CAMERA_FULL_PATH);
				pr_info("dual capture finish!total %d\n",
					s_isp_group.dual_cap_total);
			}
		} else {
			sprd_dcam_full_path_buf_reset(id);
			sprd_cam_queue_frm_clear(&module->full_zsl_queue);
			sprd_dcam_full_path_next_frm_set(id);
			sprd_dcam_drv_path_resume(id, CAMERA_FULL_PATH);
		}
		s_isp_group.dual_cap_sts = 0;
		if (dev->need_4in1) {
			module->capture_4in1_state = ISP_ST_STOP;
			fmcu_slice->capture_state = ISP_ST_STOP;
		}
	}
	pr_info("isp%d end\n", idx);
}

static void sprd_ispint_p_all_done(enum isp_id idx, void *isp_handle)
{
	struct isp_pipe_dev *dev = (struct isp_pipe_dev *)isp_handle;
	struct isp_module *module = NULL;
	struct camera_frame *pframe = NULL;

	module = &dev->module_info;

	if (sprd_cam_queue_frm_firstnode_get(&module->bin_frm_queue,
		(void **)&pframe) != 0) {
		pr_err("fail to get bin frm frame\n");
		return;
	}
	pr_debug("isp all done. isp_id:%d usr_cnt %d\n", idx,
		atomic_read(&pframe->usr_cnt));

	if (module->isp_path[ISP_SCL_PRE].valid
		&& module->isp_path[ISP_SCL_VID].valid) {
		if (atomic_read(&pframe->usr_cnt) > 1) {
			sprd_ispint_store_pre_done(idx, isp_handle);
			sprd_ispint_store_vid_done(idx, isp_handle);
		} else
			sprd_ispint_store_vid_done(idx, isp_handle);
	} else if (module->isp_path[ISP_SCL_PRE].valid) {
		sprd_ispint_store_pre_done(idx, isp_handle);
	} else if (module->isp_path[ISP_SCL_VID].valid) {
		sprd_ispint_store_vid_done(idx, isp_handle);
	}
}

static void sprd_ispint_shadow_done(enum isp_id idx, void *isp_handle)
{
	struct isp_pipe_dev *dev = NULL;
	struct isp_module *module = NULL;

	dev = (struct isp_pipe_dev *)isp_handle;
	module = &dev->module_info;

	if (module->isp_path[ISP_SCL_PRE].valid) {
		atomic_set(&dev->shadow_done, 0);
		if (dev->pre_flag > 0) {
			dev->pre_flag--;
			complete(&dev->offline_thread_com);
		}
	}
	pr_debug("isp%d ispint_shadow_done\n", idx);
}

static void sprd_ispint_c_all_done(enum isp_id idx, void *isp_handle)
{
#ifdef ISP_DRV_DEBUG
	pr_info("isp all done. isp_id:%d\n", idx);
#endif
}

static isp_isr isp_isr_list[ISP_CONTEX_MAX][32] = {
	[ISP_CONTEX_P0][ISP_INT_ISP_ALL_DONE] = sprd_ispint_p_all_done,
	[ISP_CONTEX_P0][ISP_INT_SHADOW_DONE] = sprd_ispint_shadow_done,
	[ISP_CONTEX_P0][ISP_INT_STORE_DONE_VID] = NULL,
	[ISP_CONTEX_P0][ISP_INT_STORE_DONE_PRE] = NULL,
	[ISP_CONTEX_P0][ISP_INT_FMCU_CONFIG_DONE] = NULL,
	[ISP_CONTEX_P0][ISP_INT_HIST_DONE] = sprd_ispint_hist_done,

	[ISP_CONTEX_P1][ISP_INT_ISP_ALL_DONE] = sprd_ispint_p_all_done,
	[ISP_CONTEX_P1][ISP_INT_SHADOW_DONE] = sprd_ispint_shadow_done,
	[ISP_CONTEX_P1][ISP_INT_STORE_DONE_VID] = NULL,
	[ISP_CONTEX_P1][ISP_INT_STORE_DONE_PRE] = NULL,
	[ISP_CONTEX_P1][ISP_INT_FMCU_CONFIG_DONE] = NULL,
	[ISP_CONTEX_P1][ISP_INT_HIST_DONE] = sprd_ispint_hist_done,

	[ISP_CONTEX_C0][ISP_INT_ISP_ALL_DONE] = sprd_ispint_c_all_done,
	[ISP_CONTEX_C0][ISP_INT_SHADOW_DONE] = NULL,
	[ISP_CONTEX_C0][ISP_INT_STORE_DONE_PRE] = sprd_ispint_store_cap_done,
	[ISP_CONTEX_C0][ISP_INT_FMCU_CONFIG_DONE] =
		sprd_ispint_fmcu_config_done,

	[ISP_CONTEX_C1][ISP_INT_ISP_ALL_DONE] = sprd_ispint_c_all_done,
	[ISP_CONTEX_C1][ISP_INT_SHADOW_DONE] = NULL,
	[ISP_CONTEX_C1][ISP_INT_STORE_DONE_PRE] = sprd_ispint_store_cap_done,
	[ISP_CONTEX_C1][ISP_INT_FMCU_CONFIG_DONE] =
		sprd_ispint_fmcu_config_done,
};

static irqreturn_t sprd_ispint_isr_root(int irq, void *priv)
{
	uint32_t irq_line[ISP_CONTEX_MAX] = {0};
	uint32_t irq_numbers[ISP_CONTEX_MAX] = {0};
	uint32_t j = 0, k = 0, vect = 0;
	unsigned long flag = 0;
	unsigned long base_addr = 0;
	enum isp_id id = ISP_ID_0;
	struct isp_pipe_dev *isp_handle = NULL;
	unsigned int mmu_irq_line = 0;

	isp_handle = (struct isp_pipe_dev *)priv;
	irq_numbers[0] = ARRAY_SIZE(isp_irq_p0);
	irq_numbers[1] = ARRAY_SIZE(isp_irq_p1);
	irq_numbers[2] = ARRAY_SIZE(isp_irq_c0);
	irq_numbers[3] = ARRAY_SIZE(isp_irq_c1);

	id = ISP_GET_ISP_ID(isp_handle->com_idx);

	base_addr = s_isp_regbase[id];

	if (irq == s_isp_irq[ISP_ID_0].irq0) {
		if (id != ISP_ID_0) {
			pr_err("fail to match isp_handle and irq\n");
			return IRQ_NONE;
		}
		irq_line[0] = REG_RD(base_addr +
			ISP_P0_INT_BASE + ISP_INT_INT0) & ISP_INT_LINE_MASK_P0;
		irq_line[2] = REG_RD(base_addr +
			ISP_C0_INT_BASE + ISP_INT_INT0) & ISP_INT_LINE_MASK_C0;
	} else if (irq == s_isp_irq[ISP_ID_0].irq1) {
		if (id != ISP_ID_1) {
			pr_err("fail to match isp_handle and irq\n");
			return IRQ_NONE;
		}
		irq_line[1] = REG_RD(base_addr +
			ISP_P1_INT_BASE + ISP_INT_INT0) & ISP_INT_LINE_MASK_P1;
		irq_line[3] = REG_RD(base_addr +
			ISP_C1_INT_BASE + ISP_INT_INT0) & ISP_INT_LINE_MASK_C1;
	} else {
		pr_info("isp IRQ %d, %d %d\n", irq, s_isp_irq[ISP_ID_0].irq0,
			s_isp_irq[id].irq1);
		return IRQ_HANDLED;
	}
	pr_debug("isp IRQ %d, %p, %d  %d\n", irq, priv, s_isp_irq[id].irq0,
		s_isp_irq[id].irq1);

	if (unlikely(irq_line[0] == 0 && irq_line[1] == 0
		&& irq_line[2] == 0 && irq_line[3] == 0))
		return IRQ_NONE;
	pr_debug("isp IRQ is %d, INTP0:0x%x, INTP1:0x%x, INTC0:0x%x, INTC1:0x%x\n",
		irq, irq_line[0], irq_line[1], irq_line[2], irq_line[3]);

	/*clear the interrupt*/
	if (irq == s_isp_irq[ISP_ID_0].irq0) {
		REG_WR(base_addr + ISP_P0_INT_BASE + ISP_INT_CLR0, irq_line[0]);
		REG_WR(base_addr + ISP_C0_INT_BASE + ISP_INT_CLR0, irq_line[2]);
	} else {
		REG_WR(base_addr + ISP_P1_INT_BASE + ISP_INT_CLR0, irq_line[1]);
		REG_WR(base_addr + ISP_C1_INT_BASE + ISP_INT_CLR0, irq_line[3]);
	}

	if (unlikely(ISP_IRQ_ERR_MASK_P0 & irq_line[0])
		|| unlikely(ISP_IRQ_ERR_MASK_P1 & irq_line[1])
		|| unlikely(ISP_IRQ_ERR_MASK_C0 & irq_line[2])
		|| unlikely(ISP_IRQ_ERR_MASK_C1 & irq_line[3])) {
		pr_err("fail to get isp%d IRQ P0:0x%x P1:0x%x C0:0x%x C1:0x%x\n",
			id, irq_line[0], irq_line[1], irq_line[2], irq_line[3]);
		/*handle the error here*/
		if (sprd_ispint_err_pre_proc(isp_handle->com_idx))
			return IRQ_HANDLED;
	}

	mmu_irq_line = REG_RD(base_addr +
				ISP_MMU_INT_BASE + ISP_MMU_INT_MASKED_STS);
	if (unlikely(ISP_INT_LINE_MASK_MMU & mmu_irq_line)) {
		pr_err("fail to run iommu, INT:0x%x\n", mmu_irq_line);
		pr_err("fail to get isp%d IRQ P0:0x%x P1:0x%x C0:0x%x C1:0x%x\n",
			id, irq_line[0], irq_line[1], irq_line[2], irq_line[3]);
		sprd_ispint_iommu_reg_trace(id);
		REG_WR(base_addr +
			ISP_MMU_INT_BASE + ISP_MMU_INT_CLR, mmu_irq_line);
	}

	/*spin_lock_irqsave protect the isr_func*/
	spin_lock_irqsave(&isp_mod_lock, flag);
	for (j = 0; j < ISP_CONTEX_MAX; j++) {
		if (id == ISP_ID_0 && (j != ISP_CONTEX_P0
			&& j != ISP_CONTEX_C0))
			continue;
		if (id == ISP_ID_1 && (j != ISP_CONTEX_P1
			&& j != ISP_CONTEX_C1))
			continue;
		for (k = 0; k < irq_numbers[j]; k++) {
			vect = 0;
			if (j == ISP_CONTEX_P0)
				vect = isp_irq_p0[k];
			else if (j == ISP_CONTEX_P1)
				vect = isp_irq_p1[k];
			else if (j == ISP_CONTEX_C0)
				vect = isp_irq_c0[k];
			else if (j == ISP_CONTEX_C1)
				vect = isp_irq_c1[k];
			else
				continue;
			if (irq_line[j] & (1 << (uint32_t)vect)) {
				if (isp_isr_list[j][vect])
					isp_isr_list[j][vect](id,
						isp_handle);
			}
			irq_line[j] &= ~(1 << (uint32_t)vect);
			if (!irq_line[j])
				break;
		}
	}

	/*spin_unlock_irqrestore*/
	spin_unlock_irqrestore(&isp_mod_lock, flag);

	return IRQ_HANDLED;
}

int sprd_isp_int_irq_request(struct device *p_dev, struct isp_ch_irq *irq,
	struct isp_pipe_dev *ispdev)
{
	int ret = 0;
	enum isp_id id = 0;

	if (!p_dev || !irq || !ispdev) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	id = ISP_GET_ISP_ID(ispdev->com_idx);
	isp_irq0_lock[id] = __SPIN_LOCK_UNLOCKED(&isp_irq0_lock[id]);
	isp_irq1_lock[id] = __SPIN_LOCK_UNLOCKED(&isp_irq1_lock[id]);

	if (id == ISP_ID_0) {
		ret = request_irq(irq->irq0, sprd_ispint_isr_root,
			IRQF_SHARED, "ISP0", (void *)ispdev);
		if (ret) {
			pr_err("fail to install IRQ irq0 %d\n", ret);
			goto exit;
		}
	} else if (id == ISP_ID_1) {
		ret = request_irq(irq->irq1, sprd_ispint_isr_root,
			IRQF_SHARED, "ISP1", (void *)ispdev);
		if (ret) {
			pr_err("fail to install IRQ irq1 %d\n", ret);
			goto exit;
		}
	} else {
		pr_err("fail to get right isp id %d %p\n", id, ispdev);
		return -EFAULT;
	}

exit:
	return ret;
}

int sprd_isp_int_irq_free(struct isp_ch_irq *irq,
	struct isp_pipe_dev *ispdev)
{
	int ret = 0;
	enum isp_id id = 0;

	if (!irq || !ispdev) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	id = ISP_GET_ISP_ID(ispdev->com_idx);
	isp_irq0_lock[id] = __SPIN_LOCK_UNLOCKED(&isp_irq0_lock[id]);
	isp_irq1_lock[id] = __SPIN_LOCK_UNLOCKED(&isp_irq1_lock[id]);

	if (id == ISP_ID_0)
		free_irq(irq->irq0, (void *)ispdev);
	else if (id == ISP_ID_1)
		free_irq(irq->irq1, (void *)ispdev);
	else {
		pr_err("fail to get right isp id %d %p\n", id, ispdev);
		return -EFAULT;
	}

	return ret;
}

void sprd_isp_int_path_sof(enum isp_id idx, enum isp_path_index path_idx,
	void *isp_handle)
{
	void *data;
	isp_isr_func user_func;
	struct camera_frame frame_info;

	memset(&frame_info, 0x00, sizeof(frame_info));

	user_func = p_user_func[idx][ISP_PATH_SOF];
	data = p_user_data[idx][ISP_PATH_SOF];
	frame_info.irq_type = CAMERA_IRQ_PATH_SOF;

	switch (path_idx) {

	case ISP_PATH_IDX_PRE:
		frame_info.type = CAMERA_PRE_PATH;
		break;
	case ISP_PATH_IDX_CAP:
		frame_info.type = CAMERA_CAP_PATH;
		break;
	case ISP_PATH_IDX_VID:
		frame_info.type = CAMERA_VID_PATH;
		break;
	default:
		pr_err("fail to get valid path\n");
		return;
	}

	if (user_func)
		user_func(&frame_info, data);
}

int sprd_isp_int_irq_callback(enum isp_id id, enum isp_irq_id irq_id,
	isp_isr_func user_func, void *user_data)
{
	unsigned long flag = 0;

	if (!user_data) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	spin_lock_irqsave(&isp_mod_lock, flag);
	p_user_func[id][irq_id] = user_func;
	p_user_data[id][irq_id] = user_data;
	spin_unlock_irqrestore(&isp_mod_lock, flag);

	return 0;
}
