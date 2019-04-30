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

#ifdef FEATRUE_DCAM_IOCTRL

typedef int(*dcam_io_fun) (struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg);

struct dcam_io_ctrl_fun {
	uint32_t cmd;
	dcam_io_fun io_ctrl;
};

struct cam_io_ctrl_descr {
	uint32_t ioctl_val;
	char *ioctl_str;
};

static const struct cam_io_ctrl_descr cam_ioctl_desc[] = {
	{SPRD_IMG_IO_SET_MODE,            "SPRD_IMG_IO_SET_MODE"},
	{SPRD_IMG_IO_SET_CAP_SKIP_NUM,    "SPRD_IMG_IO_SET_CAP_SKIP_NUM"},
	{SPRD_IMG_IO_SET_SENSOR_SIZE,     "SPRD_IMG_IO_SET_SENSOR_SIZE"},
	{SPRD_IMG_IO_SET_SENSOR_TRIM,     "SPRD_IMG_IO_SET_SENSOR_TRIM"},
	{SPRD_IMG_IO_SET_FRM_ID_BASE,     "SPRD_IMG_IO_SET_FRM_ID_BASE"},
	{SPRD_IMG_IO_SET_CROP,            "SPRD_IMG_IO_SET_CROP"},
	{SPRD_IMG_IO_SET_FLASH,           "SPRD_IMG_IO_SET_FLASH"},
	{SPRD_IMG_IO_SET_OUTPUT_SIZE,     "SPRD_IMG_IO_SET_OUTPUT_SIZE"},
	{SPRD_IMG_IO_SET_ZOOM_MODE,       "SPRD_IMG_IO_SET_ZOOM_MODE"},
	{SPRD_IMG_IO_SET_SENSOR_IF,       "SPRD_IMG_IO_SET_SENSOR_IF"},
	{SPRD_IMG_IO_SET_FRAME_ADDR,      "SPRD_IMG_IO_SET_FRAME_ADDR"},
	{SPRD_IMG_IO_PATH_FRM_DECI,       "SPRD_IMG_IO_PATH_FRM_DECI"},
	{SPRD_IMG_IO_PATH_PAUSE,          "SPRD_IMG_IO_PATH_PAUSE"},
	{SPRD_IMG_IO_PATH_RESUME,         "SPRD_IMG_IO_PATH_RESUME"},
	{SPRD_IMG_IO_STREAM_ON,           "SPRD_IMG_IO_STREAM_ON"},
	{SPRD_IMG_IO_STREAM_OFF,          "SPRD_IMG_IO_STREAM_OFF"},
	{SPRD_IMG_IO_GET_FMT,             "SPRD_IMG_IO_GET_FMT"},
	{SPRD_IMG_IO_GET_CH_ID,           "SPRD_IMG_IO_GET_CH_ID"},
	{SPRD_IMG_IO_GET_TIME,            "SPRD_IMG_IO_GET_TIME"},
	{SPRD_IMG_IO_CHECK_FMT,           "SPRD_IMG_IO_CHECK_FMT"},
	{SPRD_IMG_IO_SET_SHRINK,          "SPRD_IMG_IO_SET_SHRINK"},
	{SPRD_IMG_IO_SET_FREQ_FLAG,       "SPRD_IMG_IO_SET_FREQ_FLAG"},
	{SPRD_IMG_IO_CFG_FLASH,           "SPRD_IMG_IO_CFG_FLASH"},
	{SPRD_IMG_IO_PDAF_CONTROL,        "SPRD_IMG_IO_PDAF_CONTROL"},
	{SPRD_IMG_IO_GET_IOMMU_STATUS,    "SPRD_IMG_IO_GET_IOMMU_STATUS"},
	{SPRD_IMG_IO_DISABLE_MODE,        "SPRD_IMG_IO_DISABLE_MODE"},
	{SPRD_IMG_IO_ENABLE_MODE,         "SPRD_IMG_IO_ENABLE_MODE"},
	{SPRD_IMG_IO_START_CAPTURE,       "SPRD_IMG_IO_START_CAPTURE"},
	{SPRD_IMG_IO_STOP_CAPTURE,        "SPRD_IMG_IO_STOP_CAPTURE"},
	{SPRD_IMG_IO_SET_PATH_SKIP_NUM,   "SPRD_IMG_IO_SET_PATH_SKIP_NUM"},
	{SPRD_IMG_IO_SBS_MODE,            "SPRD_IMG_IO_SBS_MODE"},
	{SPRD_IMG_IO_DCAM_PATH_SIZE,      "SPRD_IMG_IO_DCAM_PATH_SIZE"},
	{SPRD_IMG_IO_SET_SENSOR_MAX_SIZE, "SPRD_IMG_IO_SET_SENSOR_MAX_SIZE"},
	{SPRD_ISP_IO_CAPABILITY,          "SPRD_ISP_IO_CAPABILITY"},
	{SPRD_ISP_IO_IRQ,                 "SPRD_ISP_IO_IRQ"},
	{SPRD_ISP_IO_READ,                "SPRD_ISP_IO_READ"},
	{SPRD_ISP_IO_WRITE,               "SPRD_ISP_IO_WRITE"},
	{SPRD_ISP_IO_RST,                 "SPRD_ISP_IO_RST"},
	{SPRD_ISP_IO_STOP,                "SPRD_ISP_IO_STOP"},
	{SPRD_ISP_IO_INT,                 "SPRD_ISP_IO_INT"},
	{SPRD_ISP_IO_SET_STATIS_BUF,      "SPRD_ISP_IO_SET_STATIS_BUF"},
	{SPRD_ISP_IO_CFG_PARAM,           "SPRD_ISP_IO_CFG_PARAM"},
	{SPRD_ISP_REG_READ,               "SPRD_ISP_REG_READ"},
	{SPRD_ISP_IO_POST_3DNR,           "SPRD_ISP_IO_POST_3DNR"},
	{SPRD_STATIS_IO_CFG_PARAM,        "SPRD_STATIS_IO_CFG_PARAM"},
	{SPRD_ISP_IO_RAW_CAP,             "SPRD_ISP_IO_RAW_CAP"},
	{SPRD_IMG_IO_GET_DCAM_RES,        "SPRD_IMG_IO_GET_DCAM_RES"},
	{SPRD_IMG_IO_PUT_DCAM_RES,        "SPRD_IMG_IO_PUT_DCAM_RES"},
	{SPRD_ISP_IO_SET_PULSE_LINE,      "SPRD_ISP_IO_SET_PULSE_LINE"},
	{SPRD_ISP_IO_CFG_START,           "SPRD_ISP_IO_CFG_START"},
	{SPRD_ISP_IO_POST_YNR,            "SPRD_ISP_IO_POST_YNR"},
	{SPRD_ISP_IO_SET_NEXT_VCM_POS,    "SPRD_ISP_IO_SET_NEXT_VCM_POS"},
	{SPRD_ISP_IO_SET_VCM_LOG,         "SPRD_ISP_IO_SET_VCM_LOG"},
	{SPRD_IMG_IO_SET_3DNR,            "SPRD_IMG_IO_SET_3DNR"},
	{SPRD_IMG_IO_SET_FUNCTION_MODE,   "SPRD_IMG_IO_SET_FUNCTION_MODE"},
	{SPRD_ISP_IO_MASK_3A,             "SPRD_ISP_IO_MASK_3A"},
	{SPRD_IMG_IO_GET_FLASH_INFO,      "SPRD_IMG_IO_GET_FLASH_INFO"},
	{SPRD_IMG_IO_MAP_IOVA,            "SPRD_IMG_IO_MAP_IOVA"},
	{SPRD_IMG_IO_UNMAP_IOVA,          "SPRD_IMG_IO_UNMAP_IOVA"},
	{SPRD_IMG_IO_GET_SG,              "SPRD_IMG_IO_GET_SG"},
	{SPRD_ISP_IO_UPDATE_PARAM_START,  "SPRD_ISP_IO_UPDATE_PARAM_START"},
	{SPRD_ISP_IO_UPDATE_PARAM_END,    "SPRD_ISP_IO_UPDATE_PARAM_END"},
	{SPRD_ISP_IO_REG_ISP_ISR,         "SPRD_ISP_IO_REG_ISP_ISR"},
	{SPRD_IMG_IO_SET_4IN1_ADDR,       "SPRD_IMG_IO_SET_4IN1_ADDR"},
	{SPRD_IMG_IO_4IN1_POST_PROC,      "SPRD_IMG_IO_4IN1_POST_PROC"},
};

static int sprd_camioctl_path_capability_get(
	struct cam_path_capability *capacity)
{
	if (capacity == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}

	capacity->count = 5;
	capacity->support_3dnr_mode = SPRD_3DNR_HW;
	capacity->support_4in1 = 1;

	capacity->path_info[CAMERA_FULL_PATH].line_buf = 0;
	capacity->path_info[CAMERA_FULL_PATH].support_yuv = 0;
	capacity->path_info[CAMERA_FULL_PATH].support_raw = 1;
	capacity->path_info[CAMERA_FULL_PATH].support_jpeg = 0;
	capacity->path_info[CAMERA_FULL_PATH].support_scaling = 0;
	capacity->path_info[CAMERA_FULL_PATH].support_trim = 1;
	capacity->path_info[CAMERA_FULL_PATH].is_scaleing_path = 0;

	capacity->path_info[CAMERA_BIN_PATH].line_buf = 0;
	capacity->path_info[CAMERA_BIN_PATH].support_yuv = 0;
	capacity->path_info[CAMERA_BIN_PATH].support_raw = 1;
	capacity->path_info[CAMERA_BIN_PATH].support_jpeg = 0;
	capacity->path_info[CAMERA_BIN_PATH].support_scaling = 0;
	capacity->path_info[CAMERA_BIN_PATH].support_trim = 1;
	capacity->path_info[CAMERA_BIN_PATH].is_scaleing_path = 0;

	capacity->path_info[CAMERA_PRE_PATH].line_buf =
		ISP_PATH1_LINE_BUF_LENGTH;
	capacity->path_info[CAMERA_PRE_PATH].support_yuv = 1;
	capacity->path_info[CAMERA_PRE_PATH].support_raw = 0;
	capacity->path_info[CAMERA_PRE_PATH].support_jpeg = 0;
	capacity->path_info[CAMERA_PRE_PATH].support_scaling = 1;
	capacity->path_info[CAMERA_PRE_PATH].support_trim = 1;
	capacity->path_info[CAMERA_PRE_PATH].is_scaleing_path = 0;

	capacity->path_info[CAMERA_VID_PATH].line_buf =
		ISP_PATH2_LINE_BUF_LENGTH;
	capacity->path_info[CAMERA_VID_PATH].support_yuv = 1;
	capacity->path_info[CAMERA_VID_PATH].support_raw = 0;
	capacity->path_info[CAMERA_VID_PATH].support_jpeg = 0;
	capacity->path_info[CAMERA_VID_PATH].support_scaling = 1;
	capacity->path_info[CAMERA_VID_PATH].support_trim = 1;
	capacity->path_info[CAMERA_VID_PATH].is_scaleing_path = 0;

	capacity->path_info[CAMERA_CAP_PATH].line_buf =
		ISP_PATH3_LINE_BUF_LENGTH;
	capacity->path_info[CAMERA_CAP_PATH].support_yuv = 1;
	capacity->path_info[CAMERA_CAP_PATH].support_raw = 0;
	capacity->path_info[CAMERA_CAP_PATH].support_jpeg = 0;
	capacity->path_info[CAMERA_CAP_PATH].support_scaling = 1;
	capacity->path_info[CAMERA_CAP_PATH].support_trim = 1;
	capacity->path_info[CAMERA_CAP_PATH].is_scaleing_path = 0;

	return 0;
}

static int sprd_camioctl_cam_path_clear(struct camera_dev *dev)
{
	uint32_t i = 0;
	struct camera_context *ctx = &dev->cam_ctx;
	struct camera_path_spec *path;

	if (ctx == NULL) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	for (i = 0; i < CAMERA_MAX_PATH; i++) {
		path = &ctx->cam_path[i];
		if (path == NULL) {
			pr_err("fail to init path %d\n", i);
			return -EINVAL;
		}
		sprd_cam_queue_buf_clear(&path->buf_queue);
		path->assoc_idx = 0;
		path->is_work = 0;
		path->status = PATH_IDLE;
	}
	ctx->need_isp_tool = 0;

	return 0;
}

static int sprd_camioctl_dev_get(struct camera_file *pcamerafile,
	struct camera_dev **ppdev, struct camera_context **ppinfo)
{
	int ret = 0, idx = 0;
	struct camera_group *group = NULL;

	if (!pcamerafile) {
		ret = -EFAULT;
		pr_err("fail to get camerafile\n");
		goto exit;
	}
	group = pcamerafile->grp;
	if (!group) {
		ret = -EFAULT;
		pr_err("fail to get group\n");
		goto exit;
	}

	idx = pcamerafile->idx;
	if (unlikely(idx < 0 || idx >= CAMERA_MAX_COUNT)) {
		pr_err("fail to get valid cam idx=%d\n", idx);
		ret = -EFAULT;
		goto exit;
	}

	if (group->dev_inited & (1 << (int)idx)) {
		*ppdev = group->dev[idx];
		if (!(*ppdev)) {
			ret = -EFAULT;
			pr_err("fail to get cam dev[%d]\n", idx);
			goto exit;
		}
		*ppinfo = &(*ppdev)->cam_ctx;
	} else {
		ret = -EFAULT;
		pr_err("fail to get cam dev[%d] and info\n", idx);
		goto exit;
	}

exit:
	return ret;
}

static int sprd_camioctl_path_idx_get(uint32_t channel_id)
{
	int path_index = 0;

	switch (channel_id) {
	case CAMERA_PRE_PATH:
		path_index = ISP_PATH_IDX_PRE;
		break;
	case CAMERA_VID_PATH:
		path_index = ISP_PATH_IDX_VID;
		break;
	case CAMERA_CAP_PATH:
		path_index = ISP_PATH_IDX_CAP;
		break;
	default:
		path_index = ISP_PATH_IDX_ALL;
		pr_err("fail to get path index, channel %d\n", channel_id);
	}

	return path_index;
}

static int sprd_camioctl_tx_sof(struct camera_frame *frame, void *param)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_dev *dev = (struct camera_dev *)param;
	struct camera_node node;

	if (param == NULL || dev == NULL ||
		atomic_read(&dev->stream_on) == 0) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	memset((void *)&node, 0, sizeof(node));
	node.irq_flag = IMG_TX_DONE;
	node.irq_type = CAMERA_IRQ_DONE;
	node.irq_property = IRQ_DCAM_SOF;
	node.frame_id = frame->frame_id;
	node.time = frame->time;
	node.dual_info = frame->dual_info;

	ret = sprd_cam_queue_buf_write(&dev->queue, &node);
	if (ret) {
		pr_err("fail to write to queue\n");
		return ret;
	}
	complete(&dev->irq_com);

	return ret;
}

static int sprd_camioctl_tx_error(struct camera_frame *frame, void *param)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_dev *dev = (struct camera_dev *)param;
	struct camera_node node;

	if (param == NULL || dev == NULL ||
		atomic_read(&dev->stream_on) == 0) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	memset((void *)&node, 0, sizeof(node));
	atomic_set(&dev->run_flag, 1);
	node.irq_flag = IMG_TX_ERR;
	node.irq_type = CAMERA_IRQ_IMG;
	node.irq_property = IRQ_MAX_DONE;
	ret = sprd_cam_queue_buf_write(&dev->queue, &node);
	if (ret) {
		pr_err("fail to write to queue\n");
		return ret;
	}
	complete(&dev->irq_com);
	pr_info("tx error\n");

	return ret;
}

static int sprd_camioctl_tx_done(struct camera_frame *frame, void *param)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_dev *dev = (struct camera_dev *)param;
	struct camera_path_spec *path;
	struct camera_node node;

	if (frame == NULL || dev == NULL || param == NULL ||
		atomic_read(&dev->stream_on) == 0) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	atomic_set(&dev->run_flag, 1);

	memset((void *)&node, 0, sizeof(node));

	if (frame->irq_type == CAMERA_IRQ_IMG
		|| frame->irq_type == CAMERA_IRQ_4IN1_DONE
		|| frame->irq_type == CAMERA_IRQ_PATH_SOF) {
		path = &dev->cam_ctx.cam_path[frame->type];

		if (path->status == PATH_IDLE) {
			pr_info("CAM: path id %d idle\n", frame->type);
			return ret;
		}

		node.irq_flag = IMG_TX_DONE;
		node.irq_type = frame->irq_type;
		node.irq_property = IRQ_MAX_DONE;
		node.f_type = frame->type;
		node.index = frame->fid;
		node.height = frame->height;
		node.yaddr = frame->yaddr;
		node.uaddr = frame->uaddr;
		node.vaddr = frame->vaddr;
		node.yaddr_vir = frame->yaddr_vir;
		node.uaddr_vir = frame->uaddr_vir;
		node.vaddr_vir = frame->vaddr_vir;
		node.frame_id = frame->frame_id;
		if (dev->zoom_ratio)
			node.zoom_ratio = frame->zoom_ratio;
		else
			node.zoom_ratio = ZOOM_RATIO_DEFAULT;
		memcpy(node.mfd, frame->buf_info.mfd,
			sizeof(frame->buf_info.mfd));
	} else if (frame->irq_type == CAMERA_IRQ_STATIS) {
		node.irq_flag = IMG_TX_DONE;
		node.irq_type = frame->irq_type;
		node.irq_property = frame->irq_property;
		node.phy_addr = frame->phy_addr;
		node.vir_addr = frame->vir_addr;
		node.kaddr[0] = frame->kaddr[0];
		node.kaddr[1] = frame->kaddr[1];
		node.addr_offset = frame->addr_offset;
		node.buf_size = frame->buf_size;
		node.frame_id = frame->frame_id;
		node.dual_info = frame->dual_info;
		if (dev->zoom_ratio)
			node.zoom_ratio = dev->zoom_ratio;
		else
			node.zoom_ratio = ZOOM_RATIO_DEFAULT;
		memcpy(node.mfd, frame->buf_info.mfd,
			sizeof(frame->buf_info.mfd));
	} else if (frame->irq_type == CAMERA_IRQ_DONE) {
		node.irq_flag = IMG_TX_DONE;
		node.irq_type = frame->irq_type;
		node.irq_property = frame->irq_property;
		node.frame_id = frame->frame_id;
		node.dual_info = frame->dual_info;
	} else if (frame->irq_type == CAMERA_IRQ_3DNR_DONE) {
		node.irq_flag = IMG_TX_DONE;
		node.irq_type = frame->irq_type;
		node.irq_property = frame->irq_property;
		node.frame_id = frame->frame_id;
	} else {
		pr_err("fail to get valid irq_type %d\n", frame->irq_type);
		return -EINVAL;
	}

	if (frame->type == CAMERA_CAP_PATH) {
		if (dev->cam_ctx.need_4in1 || dev->raw_cap) {
			frame->time.boot_time = ktime_get_boottime();
			sprd_cam_com_timestamp(&frame->time.timeval);
		}
		pr_debug("cam %d frame %d: time %ld.%ld, %lld\n",
			dev->idx, frame->frame_id,
			frame->time.timeval.tv_sec,
			frame->time.timeval.tv_usec,
			frame->time.boot_time.tv64);
	}
	node.time = frame->time;

	ret = sprd_cam_queue_buf_write(&dev->queue, &node);
	if (ret) {
		pr_err("fail to write to queue\n");
		return ret;
	}

	CAM_TRACE("sprd_img %d %d %p\n", dev->idx, frame->type, dev);
	if (node.irq_property == IRQ_RAW_CAP_DONE)
		pr_info("raw capture tx done flag %d type %d fd = 0x%x\n",
			node.irq_flag, node.irq_type, frame->buf_info.mfd[0]);

	complete(&dev->irq_com);

	return ret;
}

static int sprd_camioctl_tx_stop(void *param, uint32_t sensor_id)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_dev *dev = (struct camera_dev *)param;
	struct camera_node node;

	memset((void *)&node, 0, sizeof(node));
	node.irq_flag = IMG_TX_STOP;
	ret = sprd_cam_queue_buf_write(&dev->queue, &node);
	if (ret) {
		pr_err("fail to write to queue\n");
		return ret;
	}
	complete(&dev->irq_com);
	pr_info("tx stop %d\n", dev->irq_com.done);

	return ret;
}

static void sprd_camioctl_timer_callback(unsigned long data)
{
	int ret = 0;
	struct camera_dev *dev = (struct camera_dev *)data;
	struct camera_node node;

	memset((void *)&node, 0, sizeof(node));
	if (data == 0 || dev == NULL ||
		atomic_read(&dev->stream_on) == 0) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	if (atomic_read(&dev->run_flag) == 0) {
		pr_info("cam%d timeout.\n", dev->idx);
		node.irq_flag = IMG_TIMEOUT;
		node.irq_type = CAMERA_IRQ_IMG;
		node.irq_property = IRQ_MAX_DONE;
		node.invalid_flag = 0;
		ret = sprd_cam_queue_buf_write(&dev->queue, &node);
		if (ret)
			pr_err("fail to write queue error\n");

		complete(&dev->irq_com);
	}
}

static void sprd_camioctl_timer_init(struct timer_list *cam_timer,
			unsigned long data)
{
	setup_timer(cam_timer, sprd_camioctl_timer_callback, data);
}

static int sprd_camioctl_timer_start(struct timer_list *cam_timer,
			uint32_t time_val)
{
	int ret = 0;

	CAM_TRACE("starting timer %ld\n", jiffies);
	ret = mod_timer(cam_timer, jiffies + msecs_to_jiffies(time_val));
	if (ret)
		pr_err("fail to start in mod_timer %d\n", ret);

	return ret;
}

static int sprd_camioctl_timer_stop(struct timer_list *cam_timer)
{
	CAM_TRACE("stop timer\n");
	del_timer_sync(cam_timer);

	return 0;
}

static int sprd_camioctl_full_tx_done(struct camera_frame *frame, void *param)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_dev *dev = (struct camera_dev *)param;
	struct camera_path_spec *path =
		&dev->cam_ctx.cam_path[CAMERA_FULL_PATH];
	struct camera_group *group = (struct camera_group *)dev->grp;

	if (dev->cam_ctx.need_isp_tool
		|| path->assoc_idx == 0) {
		if (dev->cam_ctx.need_4in1) {
			frame->irq_type = CAMERA_IRQ_4IN1_DONE;
			sprd_cam_buf_addr_unmap(&frame->buf_info);
		} else if (path->assoc_idx == 0)
			sprd_cam_buf_addr_unmap(&frame->buf_info);
		sprd_camioctl_tx_done(frame, param);
		return 0;
	}

	if (path->assoc_idx != 0) {
		if (dev->idx == DCAM_ID_2
			|| (path->out_fmt == DCAM_YUV420
				&& ((path->assoc_idx & 1 << CAMERA_PRE_PATH)
				|| (path->assoc_idx & 1 << CAMERA_VID_PATH))))
			sprd_isp_path_offline_frame_set(
				dev->isp_dev_handle,
				CAMERA_BIN_PATH, frame);
		else
			sprd_isp_path_offline_frame_set(
				dev->isp_dev_handle,
				CAMERA_FULL_PATH, frame);
	} else {
		sprd_cam_ioctl_addr_write_back(&dev->isp_dev_handle,
			CAMERA_FULL_PATH, frame);
	}

	if (group->dump_dcamraw) {
		if (dev->cam_ctx.need_4in1)
			return ret;
		if (dev->cap_flag) {
			sprd_cam_debug_dump
				(dev->grp->dump_work, frame);
		}
	}

	return ret;
}

static int sprd_camioctl_bin_tx_done(struct camera_frame *frame, void *param)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_dev *dev = (struct camera_dev *)param;
	struct camera_path_spec *path =
		&dev->cam_ctx.cam_path[CAMERA_BIN_PATH];
	struct dcam_module *dcam_module = NULL;
	struct camera_group *group = (struct camera_group *)dev->grp;

	if (dev->cam_ctx.need_4in1 && dev->cam_ctx.need_isp_tool != 1) {
		dcam_module = sprd_dcam_drv_module_get(DCAM_ID_1);
		if (dcam_module->cap_4in1) {
			if (group->dump_dcamraw) {
				frame->type = CAMERA_FULL_PATH;
				sprd_cam_debug_dump(
					dev->grp->dump_work, frame);
			}
			sprd_isp_path_4in1_raw_proc_start(dev->isp_dev_handle,
				frame);
			dcam_module->cap_4in1 = 0;
			return ret;
		}
	}

	if (group->dump_dcamraw) {
		if (dev->cap_flag) {
			sprd_cam_debug_dump
				(dev->grp->dump_work, frame);
		}
	}

	if (dev->raw_cap && dev->raw_phase == 0) {
		if (group->dump_dcamraw) {
			sprd_cam_debug_dump
				(dev->grp->dump_work, frame);
		}
		sprd_isp_path_raw_proc_start(dev->isp_dev_handle,
			CAMERA_BIN_PATH, frame);
		dev->raw_phase = 1;
	} else if (path->assoc_idx != 0) {
		if ((atomic_read(&dev->stream_on) == 1)) {
			sprd_isp_path_offline_frame_set(dev->isp_dev_handle,
				CAMERA_BIN_PATH, frame);
		} else {
			sprd_cam_ioctl_addr_write_back(&dev->isp_dev_handle,
				CAMERA_BIN_PATH, frame);
		}
	} else {
		sprd_cam_ioctl_addr_write_back(&dev->isp_dev_handle,
			CAMERA_BIN_PATH, frame);
	}

	return ret;
}

static int sprd_camioctl_dcam_reg_isr(struct camera_dev *param)
{
	sprd_dcam_int_reg_isr(param->idx, DCAM_CAP_SOF,
		sprd_camioctl_tx_sof, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_CAP_FRM_ERR,
		sprd_camioctl_tx_error, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_CAP_LINE_ERR,
		sprd_camioctl_tx_error, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_DCAM_OVF,
		sprd_camioctl_tx_error, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_SN_EOF,
		sprd_cam_flash_start, param->flash_task);
	sprd_dcam_int_reg_isr(param->idx, DCAM_FULL_PATH_TX_DONE,
		sprd_camioctl_full_tx_done, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_BIN_PATH_TX_DONE,
		sprd_camioctl_bin_tx_done, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_AEM_PATH_TX_DONE,
		sprd_camioctl_tx_done, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_AFL_TX_DONE,
		sprd_camioctl_tx_done, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_AFM_INTREQ1,
		sprd_camioctl_tx_done, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_PDAF_PATH_TX_DONE,
		sprd_camioctl_tx_done, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_VCH2_PATH_TX_DONE,
		sprd_camioctl_tx_done, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_VCH3_PATH_TX_DONE,
		sprd_camioctl_tx_done, param);

	return 0;
}

static int sprd_camioctl_dcam_unreg_isr(struct camera_dev *param)
{
	sprd_dcam_int_reg_isr(param->idx, DCAM_CAP_SOF, NULL, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_CAP_FRM_ERR, NULL, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_CAP_LINE_ERR, NULL, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_DCAM_OVF, NULL, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_SN_EOF, NULL, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_FULL_PATH_TX_DONE, NULL, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_BIN_PATH_TX_DONE, NULL, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_AEM_PATH_TX_DONE, NULL, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_AFL_TX_DONE, NULL, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_AFM_INTREQ1, NULL, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_PDAF_PATH_TX_DONE, NULL, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_VCH2_PATH_TX_DONE, NULL, param);
	sprd_dcam_int_reg_isr(param->idx, DCAM_VCH3_PATH_TX_DONE, NULL, param);

	return 0;
}

static int sprd_camioctl_isp_reg_isr(struct camera_dev *param)
{
	if (!param) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	sprd_isp_drv_reg_isr(param->isp_id, ISP_PATH_PRE_DONE,
		sprd_camioctl_tx_done, param);
	sprd_isp_drv_reg_isr(param->isp_id, ISP_PATH_VID_DONE,
		sprd_camioctl_tx_done, param);
	sprd_isp_drv_reg_isr(param->isp_id, ISP_PATH_CAP_DONE,
		sprd_camioctl_tx_done, param);
	sprd_isp_drv_reg_isr(param->isp_id, ISP_DCAM_SOF,
		sprd_camioctl_tx_done, param);
	sprd_isp_drv_reg_isr(param->isp_id, ISP_HIST_DONE,
		sprd_camioctl_tx_done, param);
	sprd_isp_drv_reg_isr(param->isp_id, ISP_PATH_SOF,
		sprd_camioctl_tx_done, param);

	return 0;
}

static int sprd_camioctl_isp_unreg_isr(struct camera_dev *param)
{
	if (!param) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	sprd_isp_drv_reg_isr(param->isp_id, ISP_PATH_PRE_DONE,
			NULL, param);
	sprd_isp_drv_reg_isr(param->isp_id, ISP_PATH_VID_DONE,
			NULL, param);
	sprd_isp_drv_reg_isr(param->isp_id, ISP_PATH_CAP_DONE,
			NULL, param);
	sprd_isp_drv_reg_isr(param->isp_id, ISP_DCAM_SOF,
			NULL, param);
	sprd_isp_drv_reg_isr(param->isp_id, ISP_HIST_DONE,
			NULL, param);
	sprd_isp_drv_reg_isr(param->isp_id, ISP_PATH_SOF,
			NULL, param);
	return 0;
}

static int sprd_camioctl_res_get(struct camera_group *group,
		enum dcam_id dcam_id, struct sprd_img_res *res)
{
	int ret = 0;
	uint32_t dcam_res_flag = 0;

	if (dcam_id == DCAM_ID_0) {
		dcam_res_flag = DCAM_RES_DCAM0_CAP |
				DCAM_RES_DCAM0_PATH;
		if (0 == (dcam_res_flag
			& group->dcam_res_used)) {
			res->flag = dcam_res_flag;
			group->dcam_res_used |= dcam_res_flag;
		} else {
			pr_err("fail to get  valid dcam for rear sensor!\n");
			ret = -1;
			goto exit;
		}
	} else if (dcam_id == DCAM_ID_1) {
		dcam_res_flag = DCAM_RES_DCAM1_CAP |
				DCAM_RES_DCAM1_PATH;
		if (0 == (dcam_res_flag &
			group->dcam_res_used)) {
			res->flag = dcam_res_flag;
			group->dcam_res_used |= dcam_res_flag;
		} else {
			pr_err("fail to get valid dcam for front sensor!\n");
			ret = -1;
			goto exit;
		}
	} else if (dcam_id == DCAM_ID_2) {
		dcam_res_flag = DCAM_RES_DCAM2_CAP |
				DCAM_RES_DCAM2_PATH;
		if (0 == (dcam_res_flag &
			group->dcam_res_used)) {
			res->flag = dcam_res_flag;
			group->dcam_res_used |= dcam_res_flag;
		} else {
			pr_err("fail to get valid dcam for ir sensor!\n");
			ret = -1;
			goto exit;
		}
	}

exit:
	if (ret)
		res->flag = 0;
	return ret;
}

static int sprd_camioctl_res_put(struct camera_group *group,
			enum dcam_id dcam_id, struct sprd_img_res *res)
{
	int ret = 0;
	uint32_t dcam_res_flag = 0;

	if (dcam_id == DCAM_ID_0) {
		dcam_res_flag = DCAM_RES_DCAM0_CAP |
				DCAM_RES_DCAM0_PATH;
		if (dcam_res_flag ==
			(res->flag &
			group->dcam_res_used)) {
			group->dcam_res_used &= ~dcam_res_flag;
		} else if (DCAM_RES_DCAM0_CAP ==
			(res->flag & group->dcam_res_used)) {
			group->dcam_res_used &= ~DCAM_RES_DCAM0_CAP;
			goto exit;
		} else {
			pr_err("fail to put dcam0 for rear sensor!\n");
			ret = -1;
			goto exit;
		}
	} else if (dcam_id == DCAM_ID_1) {
		dcam_res_flag = DCAM_RES_DCAM1_CAP |
				DCAM_RES_DCAM1_PATH;
		if (dcam_res_flag ==
			(res->flag & group->dcam_res_used)) {
			group->dcam_res_used &= ~dcam_res_flag;
		} else if (DCAM_RES_DCAM1_CAP ==
			(res->flag &
			group->dcam_res_used)) {
			group->dcam_res_used &= ~DCAM_RES_DCAM1_CAP;
			goto exit;
		} else {
			pr_err("fail to put dcam1 for front sensor\n");
			ret = -1;
			goto exit;
		}
	} else if (dcam_id == DCAM_ID_2) {
		dcam_res_flag = DCAM_RES_DCAM2_CAP |
				DCAM_RES_DCAM2_PATH;
		if (dcam_res_flag ==
			(res->flag & group->dcam_res_used)) {
			group->dcam_res_used &= ~dcam_res_flag;
		} else if (DCAM_RES_DCAM2_CAP ==
			(res->flag &
			group->dcam_res_used)) {
			group->dcam_res_used &= ~DCAM_RES_DCAM2_CAP;
			goto exit;
		} else {
			pr_err("fail to put dcam1 for front sensor\n");
			ret = -1;
			goto exit;
		}
	}

exit:
	if (ret)
		res->flag = 0;
	return ret;
}

static int sprd_camioctl_sensor_if_set(struct camera_dev *dev,
				struct sprd_img_sensor_if *sensor_if)
{
	int ret = 0;
	struct camera_context *ctx = NULL;

	if (!sensor_if) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		return ret;
	}

	if (sensor_if->res[0] != IF_OPEN)
		return ret;

	ctx = &dev->cam_ctx;
	ctx->sn_mode = sensor_if->img_fmt;
	ctx->img_ptn = sensor_if->img_ptn;
	ctx->frm_deci = sensor_if->frm_deci;

	CAM_TRACE("cam%d: mode %d deci %d\n",
		dev->idx, ctx->sn_mode, ctx->frm_deci);

	ctx->sync_pol.need_href = sensor_if->if_spec.mipi.use_href;
	ctx->is_loose = sensor_if->if_spec.mipi.is_loose;
	ctx->data_bits = sensor_if->if_spec.mipi.bits_per_pxl;
	ctx->lane_num = sensor_if->if_spec.mipi.lane_num;
	ctx->bps_per_lane = sensor_if->if_spec.mipi.pclk;

	return ret;
}

static int sprd_camioctl_get_path_id(struct camera_get_path_id *path_id,
	uint32_t *channel_id, uint32_t scene_mode)
{
	int ret = DCAM_RTN_SUCCESS;

	if (path_id == NULL || channel_id == NULL)
		return -1;

	pr_info("DCAM: fourcc 0x%x input %d %d output %d %d\n",
		path_id->fourcc,
		path_id->input_size.w, path_id->input_size.h,
		path_id->output_size.w, path_id->output_size.h);
	pr_info("DCAM: input_trim %d %d %d %d need_isp %d\n",
		path_id->input_trim.x, path_id->input_trim.y,
		path_id->input_trim.w, path_id->input_trim.h,
		path_id->need_isp);
	pr_info("DCAM: is_path_work %d %d %d %d %d\n",
		path_id->is_path_work[CAMERA_FULL_PATH],
		path_id->is_path_work[CAMERA_BIN_PATH],
		path_id->is_path_work[CAMERA_PRE_PATH],
		path_id->is_path_work[CAMERA_VID_PATH],
		path_id->is_path_work[CAMERA_CAP_PATH]);
	pr_info("DCAM: scene_mode %d, need_isp_tool %d\n", scene_mode,
		path_id->need_isp_tool);

	if (path_id->need_isp_tool)
		*channel_id = CAMERA_FULL_PATH;
	else if (path_id->fourcc == IMG_PIX_FMT_GREY &&
		 !path_id->is_path_work[CAMERA_FULL_PATH])
		*channel_id = CAMERA_FULL_PATH;
	else if ((path_id->sn_fmt == IMG_PIX_FMT_NV12 ||
		path_id->sn_fmt == IMG_PIX_FMT_NV21) &&
		!path_id->is_path_work[CAMERA_FULL_PATH] &&
		!path_id->is_path_work[CAMERA_BIN_PATH])
		*channel_id = CAMERA_FULL_PATH;
	else if (scene_mode == DCAM_SCENE_MODE_PREVIEW) {
		if (path_id->output_size.w > ISP_PATH2_LINE_BUF_LENGTH &&
			path_id->output_size.w <= ISP_PATH3_LINE_BUF_LENGTH &&
			!path_id->is_path_work[CAMERA_CAP_PATH])
			*channel_id = CAMERA_CAP_PATH;
		else if (path_id->output_size.w > ISP_PATH1_LINE_BUF_LENGTH &&
			path_id->output_size.w <= ISP_PATH2_LINE_BUF_LENGTH &&
			!path_id->is_path_work[CAMERA_VID_PATH])
			*channel_id = CAMERA_VID_PATH;
		else
			*channel_id = CAMERA_PRE_PATH;
	} else if (scene_mode == DCAM_SCENE_MODE_RECORDING)
		*channel_id = CAMERA_VID_PATH;
	else if (scene_mode == DCAM_SCENE_MODE_CAPTURE)
		*channel_id = CAMERA_CAP_PATH;
	else if (scene_mode == DCAM_SCENE_MODE_CAPTURE_CALLBACK) {
		if (path_id->output_size.w <= ISP_PATH2_LINE_BUF_LENGTH
			&& !path_id->is_path_work[CAMERA_VID_PATH])
			*channel_id = CAMERA_VID_PATH;
		else
			*channel_id = CAMERA_CAP_PATH;
	} else {
		*channel_id = CAMERA_FULL_PATH;
		pr_err("fail to select path:error\n");
	}
	pr_info("path id %d\n", *channel_id);

	return ret;
}


static int sprd_camioctl_free_channel_get(struct camera_dev *dev,
	uint32_t *channel_id, uint32_t scene_mode)
{
	int ret = 0;
	struct camera_path_spec *full_path = NULL;
	struct camera_path_spec *bin_path = NULL;
	struct camera_path_spec *path_1 = NULL;
	struct camera_path_spec *path_2 = NULL;
	struct camera_path_spec *path_3 = NULL;
	struct camera_get_path_id path_id;

	if (!dev) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		return ret;
	}

	if (!channel_id) {
		ret = -EFAULT;
		pr_err("fail to get valid param:channel id\n");
		return ret;
	}

	full_path = &dev->cam_ctx.cam_path[CAMERA_FULL_PATH];
	bin_path = &dev->cam_ctx.cam_path[CAMERA_BIN_PATH];
	path_1 = &dev->cam_ctx.cam_path[CAMERA_PRE_PATH];
	path_2 = &dev->cam_ctx.cam_path[CAMERA_VID_PATH];
	path_3 = &dev->cam_ctx.cam_path[CAMERA_CAP_PATH];

	memset((void *)&path_id, 0x00, sizeof(path_id));
	path_id.input_size.w = dev->cam_ctx.cap_in_rect.w;
	path_id.input_size.h = dev->cam_ctx.cap_in_rect.h;
	path_id.output_size.w = dev->cam_ctx.dst_size.w;
	path_id.output_size.h = dev->cam_ctx.dst_size.h;
	path_id.fourcc = dev->cam_ctx.pxl_fmt;
	path_id.sn_fmt = dev->cam_ctx.sn_fmt;
	path_id.need_isp_tool = dev->cam_ctx.need_isp_tool;
	path_id.need_isp = dev->cam_ctx.need_isp;
	path_id.input_trim.x = dev->cam_ctx.path_input_rect.x;
	path_id.input_trim.y = dev->cam_ctx.path_input_rect.y;
	path_id.input_trim.w = dev->cam_ctx.path_input_rect.w;
	path_id.input_trim.h = dev->cam_ctx.path_input_rect.h;
	CAM_TRACE("get parm, path work %d %d %d %d %d\n",
		full_path->is_work, bin_path->is_work, path_1->is_work,
		path_2->is_work, path_3->is_work);
	path_id.is_path_work[CAMERA_FULL_PATH] = full_path->is_work;
	path_id.is_path_work[CAMERA_BIN_PATH] = bin_path->is_work;
	path_id.is_path_work[CAMERA_PRE_PATH] = path_1->is_work;
	path_id.is_path_work[CAMERA_VID_PATH] = path_2->is_work;
	path_id.is_path_work[CAMERA_CAP_PATH] = path_3->is_work;
	ret = sprd_camioctl_get_path_id(&path_id, channel_id, scene_mode);

	return ret;
}

static int sprd_camioctl_crop_set(struct camera_dev *dev,
		struct sprd_img_parm *p)
{
	int ret = 0;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_rect *input_rect = NULL;
	struct camera_size *input_size = NULL;

	if (!dev) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		goto exit;
	}

	if (!p) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		goto exit;
	}

	idx = dev->idx;
	if (p->crop_rect.x + p->crop_rect.w > dev->cam_ctx.cap_out_size.w ||
		p->crop_rect.y + p->crop_rect.h > dev->cam_ctx.cap_out_size.h) {
		ret = -EINVAL;
		goto exit;
	}

	CAM_TRACE("cam%d: set crop, window %d %d %d %d\n", idx,
		p->crop_rect.x, p->crop_rect.y,
		p->crop_rect.w, p->crop_rect.h);

	switch (p->channel_id) {
	case CAMERA_FULL_PATH:
	case CAMERA_BIN_PATH:
	case CAMERA_PRE_PATH:
	case CAMERA_VID_PATH:
	case CAMERA_CAP_PATH:
	case CAMERA_PDAF_PATH:
		input_size = &dev->cam_ctx.cam_path[p->channel_id].in_size;
		input_rect = &dev->cam_ctx.cam_path[p->channel_id].in_rect;
		break;
	default:
		pr_err("fail to get cam%d right channel id %d\n",
			idx, p->channel_id);
		ret = -EINVAL;
		goto exit;
	}

	input_size->w = dev->cam_ctx.cap_out_size.w;
	input_size->h = dev->cam_ctx.cap_out_size.h;
	input_rect->x = p->crop_rect.x;
	input_rect->y = p->crop_rect.y;
	input_rect->w = p->crop_rect.w;
	input_rect->h = p->crop_rect.h;
	pr_debug("path %d, rect %d %d %d %d, size %d %d\n",
		p->channel_id, input_rect->x, input_rect->y,
		input_rect->w, input_rect->h, input_size->w,
		input_size->h);

exit:
	return ret;
}

static int sprd_camioctl_size_convert(struct camera_path_spec *path,
				struct camera_path_spec *path_bin)
{
	int ret = 0;

	if (!path || !path_bin) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		return ret;
	}

	if (path_bin->need_downsizer)
		goto exit;

	if (path->in_size.w > ISP_PATH1_LINE_BUF_LENGTH) {
		path->in_size.w >>= 1;
		path->in_size.h >>= 1;
		path->in_rect.w >>= 1;
		path->in_rect.w = CAM_ALIGNTO(path->in_rect.w);
		if (path->in_rect.w *
			CAMERA_SC_COEFF_UP_MAX <
			path->out_size.w) {
			pr_info("more than max in_size{%d %d}, in_rect{%d %d %d %d}\n",
					path->in_size.w, path->in_size.h,
					path->in_rect.x, path->in_rect.y,
					path->in_rect.w, path->in_rect.h);
			path->in_rect.w = path->out_size.w / 4;
			path->in_rect.w = CAM_ALIGNTO(path->in_rect.w);
		}
		path->in_rect.h >>= 1;
		path->in_rect.h = CAM_ALIGNTO(path->in_rect.h);
		if (path->in_rect.h *
			CAMERA_SC_COEFF_UP_MAX <
			path->out_size.h) {
			pr_info("more than max in_size{%d %d}, in_rect{%d %d %d %d}\n",
					path->in_size.w, path->in_size.h,
					path->in_rect.x, path->in_rect.y,
					path->in_rect.w, path->in_rect.h);
			path->in_rect.h = path->out_size.h / 4;
			path->in_rect.h = CAM_ALIGNTO(path->in_rect.h);
		}
		path->in_rect.x =
			(path->in_size.w - path->in_rect.w) >> 1;
		path->in_rect.x = CAM_ALIGNTO(path->in_rect.x);
		path->in_rect.y =
			(path->in_size.h - path->in_rect.h) >> 1;
		path->in_rect.y = CAM_ALIGNTO(path->in_rect.y);
	}

exit:
	pr_debug("in_size{%d %d}, in_rect{%d %d %d %d},out_size{%d,%d}\n",
		path->in_size.w, path->in_size.h,
		path->in_rect.x, path->in_rect.y,
		path->in_rect.w, path->in_rect.h,
		path->out_size.w, path->out_size.h);

	return ret;
}

static int sprd_camioctl_video_update(struct camera_dev *dev,
		uint32_t channel_id)
{
	int ret = DCAM_RTN_SUCCESS;
	struct camera_path_spec *path = NULL;
	struct camera_path_spec *path_bin = NULL;
	struct camera_path_spec *path_pre = NULL;
	struct camera_path_spec *path_vid = NULL;
	enum isp_path_index path_index;
	uint32_t zoom_ratio = ZOOM_RATIO_DEFAULT;
	struct dcam_zoom_param zoom_param;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_size out_size;
	struct crop_param_t crop_param;
	struct rawsizer_param_t rawsizer_parm;

	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}

	mutex_lock(&dev->cam_mutex);
	memset(&zoom_param, 0x00, sizeof(zoom_param));
	memset(&crop_param, 0x00, sizeof(crop_param));
	memset(&rawsizer_parm, 0x00, sizeof(rawsizer_parm));
	idx = dev->idx;
	path = &dev->cam_ctx.cam_path[channel_id];
	path_bin = &dev->cam_ctx.cam_path[CAMERA_BIN_PATH];
	path_index = sprd_camioctl_path_idx_get(channel_id);

	if (dev->cam_ctx.need_downsizer) {
		path_pre = &dev->cam_ctx.cam_path[CAMERA_PRE_PATH];
		path_vid = &dev->cam_ctx.cam_path[CAMERA_VID_PATH];
		if (path_pre->in_size.w > ISP_PATH1_LINE_BUF_LENGTH)
			path_bin->zoom_info.zoom_mode = ZOOM_BINNING;
		else
			path_bin->zoom_info.zoom_mode = ZOOM_RAWSIZER;
		if (channel_id == CAMERA_PRE_PATH
			|| channel_id == CAMERA_VID_PATH) {
			if (dev->cam_ctx.need_4in1) {
				path->in_size = path_bin->in_size;
				path->in_rect.w >>= 1;
				if (path_pre->in_size.w >
					ISP_PATH1_LINE_BUF_LENGTH * 2)
					path_bin->zoom_info.zoom_mode
						= ZOOM_BINNING;
				else
					path_bin->zoom_info.zoom_mode
						= ZOOM_RAWSIZER;
			}
			if (path->in_rect.w) {
				zoom_ratio = path->in_size.w * 1000
						/ path->in_rect.w;
				dev->zoom_ratio = zoom_ratio;
			}
			if (path_vid->is_work)
				out_size = (path_pre->out_size.w *
					path_pre->out_size.h)
					>= (path_vid->out_size.w *
					path_vid->out_size.h) ?
					path_pre->out_size:path_vid->out_size;
			else
				out_size = path_pre->out_size;
			sprd_dcam_drv_init_online_pipe(
				&path_bin->zoom_info,
				path->in_size.w, path->in_size.h,
				out_size.w, out_size.h);
			sprd_dcam_drv_update_crop_param(
				&crop_param,
				&path_bin->zoom_info, (int)dev->zoom_ratio);
			sprd_dcam_drv_update_rawsizer_param(
				&rawsizer_parm,
				&path_bin->zoom_info);

			if (dev->cam_ctx.need_4in1) {
				zoom_param.in_size.w = path_bin->in_size.w;
				zoom_param.in_size.h = path_bin->in_size.h;
			} else {
				zoom_param.in_size.w = path_pre->in_size.w;
				zoom_param.in_size.h = path_pre->in_size.h;
			}
			zoom_param.in_rect.x = path_bin->zoom_info.crop_startx;
			zoom_param.in_rect.y = path_bin->zoom_info.crop_starty;
			zoom_param.in_rect.w = path_bin->zoom_info.crop_width;
			zoom_param.in_rect.h = path_bin->zoom_info.crop_height;
			zoom_param.out_size.w =
				rawsizer_parm.output_width;
			zoom_param.out_size.h =
				rawsizer_parm.output_height;
			zoom_param.bin_crop_bypass = crop_param.bypass;

			ret = sprd_dcam_drv_zoom_param_update(zoom_param,
				rawsizer_parm.is_zoom_min,
				idx);
			if (ret) {
				pr_err("fail to write zoom param\n");
				mutex_unlock(&dev->cam_mutex);
				return -ret;
			}
			ret = sprd_dcam_bin_path_cfg_set(idx,
				DCAM_PATH_ZOOM_INFO,
				&path_bin->zoom_info);
			if (unlikely(ret)) {
				pr_err("fail to cfg bin_path zoom_info\n");
				mutex_unlock(&dev->cam_mutex);
				return -ret;
				}
			}
	} else {
		if (channel_id == CAMERA_PRE_PATH
			|| channel_id == CAMERA_VID_PATH)
			sprd_camioctl_size_convert(path, path_bin);

		pr_debug("in_size{%d %d}, in_rect{%d %d %d %d}\n",
			path->in_size.w, path->in_size.h, path->in_rect.x,
			path->in_rect.y, path->in_rect.w, path->in_rect.h);
		pr_debug("out_size{%d %d}\n",
			path->out_size.w, path->out_size.h);
	}
	ret = sprd_isp_drv_zoom_param_update(dev->isp_dev_handle,
					path_index,
					&path->in_size,
					&path->in_rect,
					&path->out_size);
	if (path->in_rect.w) {
		zoom_ratio = path->in_size.w * 1000 / path->in_rect.w;
		dev->zoom_ratio = zoom_ratio;
	}

	mutex_unlock(&dev->cam_mutex);

	if (ret)
		pr_err("fail to update video 0x%x\n", ret);

	return ret;
}

static struct camera_format *sprd_camioctl_format_get(uint32_t fourcc)
{
	uint32_t i = 0;
	struct camera_format *fmt;

	for (i = 0; i < ARRAY_SIZE(dcam_img_fmt); i++) {
		fmt = &dcam_img_fmt[i];
		if (fmt->fourcc == fourcc)
			break;
	}

	if (unlikely(i == ARRAY_SIZE(dcam_img_fmt))) {
		pr_err("fail to get valid image format\n");
		return NULL;
	}

	return &dcam_img_fmt[i];
}

static int sprd_camioctl_full_path_cap_check(uint32_t fourcc,
	struct sprd_img_format *f, struct camera_context *ctx)
{
	struct camera_path_spec *full_path = &ctx->cam_path[CAMERA_FULL_PATH];

	full_path->frm_type = f->channel_id;
	full_path->is_work = 0;

	switch (fourcc) {
	case IMG_PIX_FMT_GREY:
		full_path->out_fmt = DCAM_RAWRGB;
		full_path->pixel_depth = ctx->is_loose ? 16 : 10;
		full_path->end_sel.y_endian = DCAM_ENDIAN_LITTLE;
		break;
	case IMG_PIX_FMT_NV21:
		full_path->fourcc = fourcc;
		full_path->out_fmt = DCAM_YVU420;
		full_path->pixel_depth = 8;
		full_path->end_sel.y_endian = DCAM_ENDIAN_LITTLE;
		break;
	case IMG_PIX_FMT_NV12:
		full_path->fourcc = fourcc;
		full_path->out_fmt = DCAM_YUV420;
		full_path->pixel_depth = 8;
		full_path->end_sel.y_endian = DCAM_ENDIAN_LITTLE;
		break;
	default:
		pr_err("fail to get valid image format for full_path 0x%x\n",
			fourcc);
		return -EINVAL;
	}
	full_path->fourcc = fourcc;

	CAM_TRACE("check format for full_path: out_fmt=%d, is_loose=%d\n",
		full_path->out_fmt, ctx->is_loose);
	full_path->out_size.w = f->width;
	full_path->out_size.h = f->height;
	full_path->src_sel = 0;
	full_path->is_work = 1;

	return 0;
}

static int sprd_camioctl_bin_path_cap_check(uint32_t fourcc,
	struct sprd_img_format *f, struct camera_context *ctx)
{
	struct camera_path_spec *bin_path = &ctx->cam_path[CAMERA_BIN_PATH];

	bin_path->frm_type = f->channel_id;
	bin_path->is_work = 0;

	switch (fourcc) {
	case IMG_PIX_FMT_GREY:
		bin_path->out_fmt = DCAM_RAWRGB;
		bin_path->pixel_depth = ctx->is_loose ? 16 : 10;
		bin_path->end_sel.y_endian = DCAM_ENDIAN_BIG;
		break;
	default:
		pr_err("fail to get valid image format for bin_path 0x%x\n",
			fourcc);
		return -EINVAL;
	}
	bin_path->fourcc = fourcc;

	CAM_TRACE("check format for bin_path: out_fmt=%d, is_loose=%d\n",
		bin_path->out_fmt, ctx->is_loose);
	bin_path->out_size.w = f->width;
	bin_path->out_size.h = f->height;

	bin_path->is_work = 1;

	return 0;
}

static int sprd_camioctl_path_pdaf_cap_check(uint32_t fourcc,
	struct sprd_img_format *f, struct camera_context *ctx)
{
	struct camera_path_spec *path_pdaf = &ctx->cam_path[CAMERA_PDAF_PATH];

	path_pdaf->frm_type = f->channel_id;
	path_pdaf->is_work = 0;
	/* need add pdaf path necessary info here*/
	path_pdaf->is_work = 1;

	return 0;
}

static int sprd_camioctl_scaling_check(struct sprd_img_format *f,
	struct camera_context *ctx, struct camera_path_spec *path,
	uint32_t line_buf_size)
{
	uint32_t maxw = 0, maxh = 0, tempw = 0, temph = 0;

	tempw = path->in_rect.w;
	temph = path->in_rect.h;

	if (tempw == f->width && temph == f->height) {
		pr_debug("Do not need scale\n");
		return 0;
	}

	switch (ctx->sn_mode) {
	case DCAM_CAP_MODE_RAWRGB:
		maxw = f->width * CAMERA_SC_COEFF_DOWN_MAX;
		maxw = maxw * (1 << CAMERA_PATH_DECI_FAC_MAX);
		maxh = f->height * CAMERA_SC_COEFF_DOWN_MAX;
		maxh = maxh * (1 << CAMERA_PATH_DECI_FAC_MAX);
		if (unlikely(tempw > maxw || temph > maxh)) {
			pr_err("fail to scale:out of scaling capbility\n");
			return -EINVAL;
		}

		if (unlikely(f->width > line_buf_size))
			pr_err("fail to scale:out of scaling capbility\n");

		maxw = tempw * CAMERA_SC_COEFF_UP_MAX;
		maxh = temph * CAMERA_SC_COEFF_UP_MAX;
		if (unlikely(f->width > maxw || f->height > maxh)) {
			pr_err("fail to scale:out of scaling capbility\n");
			return -EINVAL;
		}
		break;

	default:
		break;
	}

	return 0;
}

static void sprd_camioctl_endian_sel(uint32_t fourcc,
	struct camera_path_spec *path)
{
	if (fourcc == IMG_PIX_FMT_YUV422P ||
		fourcc == IMG_PIX_FMT_RGB565 ||
		fourcc == IMG_PIX_FMT_RGB565X) {
		if (fourcc == IMG_PIX_FMT_YUV422P) {
			path->out_fmt = DCAM_YUV422;
		} else {
			path->out_fmt = DCAM_RGB565;
			if (fourcc == IMG_PIX_FMT_RGB565)
				path->end_sel.y_endian = DCAM_ENDIAN_HALFBIG;
			else
				path->end_sel.y_endian = DCAM_ENDIAN_BIG;
		}
	} else {
		if (fourcc == IMG_PIX_FMT_YUV420 ||
			fourcc == IMG_PIX_FMT_YVU420) {
			path->out_fmt = DCAM_YUV420_3FRAME;
		} else {
			if (fourcc == IMG_PIX_FMT_NV12) {
				path->out_fmt = DCAM_YVU420;
				path->end_sel.uv_endian = DCAM_ENDIAN_LITTLE;
			} else {
				path->out_fmt = DCAM_YUV420;
				path->end_sel.uv_endian = DCAM_ENDIAN_LITTLE;
			}
		}
	}
}

static int sprd_camioctl_path_cap_check(uint32_t fourcc,
	struct sprd_img_format *f, struct camera_context *ctx,
	enum camera_path_id path_id)
{
	int ret = 0;
	uint32_t tempw = 0, temph = 0, line_buf_size = 0;
	struct camera_path_spec *path;

	CAM_TRACE("check format for path%d\n", path_id);

	switch (path_id) {
	case CAMERA_PRE_PATH:
		line_buf_size = ISP_PATH1_LINE_BUF_LENGTH;
		break;
	case CAMERA_VID_PATH:
		line_buf_size = ISP_PATH2_LINE_BUF_LENGTH;
		break;
	case CAMERA_CAP_PATH:
		line_buf_size = ISP_PATH3_LINE_BUF_LENGTH;
		break;
	default:
		return -EINVAL;
	}

	path = &ctx->cam_path[path_id];
	path->is_work = 0;
	path->frm_type = f->channel_id;
	path->src_sel = f->need_isp;
	path->end_sel.y_endian = DCAM_ENDIAN_LITTLE;
	path->end_sel.uv_endian = DCAM_ENDIAN_LITTLE;
	path->pixel_depth = 0;
	path->img_deci.x_factor = 0;
	path->img_deci.y_factor = 0;
	tempw = path->in_rect.w;
	temph = path->in_rect.h;
	ctx->img_deci.x_factor = 0;
	f->need_binning = 0;
	/* app should fill in this field(fmt.pix.priv) to set the base index
	 * of frame buffer, and lately this field will return the flag
	 * whether ISP is needed for this work path
	 */
	switch (fourcc) {
	case IMG_PIX_FMT_GREY:
	case IMG_PIX_FMT_JPEG:
	case IMG_PIX_FMT_YUYV:
	case IMG_PIX_FMT_YVYU:
	case IMG_PIX_FMT_UYVY:
	case IMG_PIX_FMT_VYUY:
		if (unlikely(f->width != tempw || f->height != temph)) {
			/* need scaling or triming */
			pr_err("fail to scale, src %d %d, dst %d %d\n",
				tempw, temph, f->width, f->height);
			return -EINVAL;
		}

		if (fourcc == IMG_PIX_FMT_GREY) {
			if (unlikely(ctx->sn_mode != DCAM_CAP_MODE_RAWRGB)) {
				/* the output of sensor is not RawRGB
				 * which is needed by app
				 */
				pr_err("fail to get RawRGB sensor\n");
				return -EINVAL;
			}

			path->out_fmt = DCAM_RAWRGB;
			path->end_sel.y_endian = DCAM_ENDIAN_BIG;
		} else if (fourcc == IMG_PIX_FMT_JPEG) {
			if (unlikely(ctx->sn_mode != DCAM_CAP_MODE_JPEG)) {
				/* the output of sensor is not JPEG
				 * which is needed by app
				 */
				pr_err("fail to get JPEG sensor\n");
				return -EINVAL;
			}
			path->out_fmt = DCAM_JPEG;
		}
		break;
	case IMG_PIX_FMT_YUV422P:
	case IMG_PIX_FMT_YUV420:
	case IMG_PIX_FMT_YVU420:
	case IMG_PIX_FMT_NV12:
	case IMG_PIX_FMT_NV21:
	case IMG_PIX_FMT_RGB565:
	case IMG_PIX_FMT_RGB565X:
		if (ctx->sn_mode == DCAM_CAP_MODE_RAWRGB) {
			if (path->src_sel) {
				/* check scaling */
				ret = sprd_camioctl_scaling_check(f, ctx, path,
							line_buf_size);
				if (ret)
					return ret;
			} else {
				/* no isp, only RawRGB data can be sampled */
				pr_err("fail to get valid format 0x%x\n",
					fourcc);
				return -EINVAL;
			}
		} else if (ctx->sn_mode == DCAM_CAP_MODE_YUV) {
			pr_debug("YUV sensor mode 0x%x\n", ctx->sn_mode);
		} else {
			pr_err("fail to get valid sensor mode 0x%x\n",
				ctx->sn_mode);
			return -EINVAL;
		}

		sprd_camioctl_endian_sel(fourcc, path);
		break;
	default:
		pr_err("fail to get valid image format for path %d 0x%x\n",
			path_id, fourcc);
		return -EINVAL;
	}

	path->fourcc = fourcc;
	path->out_size.w = f->width;
	path->out_size.h = f->height;
	path->is_work = 1;

	return 0;
}

static int sprd_camioctl_frame_addr_set(struct camera_file *camerafile,
	struct camera_dev *dev, struct sprd_img_parm *p)
{
	int ret = 0;
	uint32_t i = 0;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_path_spec *path = NULL;
	enum isp_path_index path_index = ISP_PATH_IDX_0;

	if (!p) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		goto exit;
	}

	idx = camerafile->idx;

	switch (p->channel_id) {
	case CAMERA_FULL_PATH:
	case CAMERA_BIN_PATH:
	case CAMERA_PRE_PATH:
	case CAMERA_VID_PATH:
	case CAMERA_CAP_PATH:
	case CAMERA_PDAF_PATH:
		path = &dev->cam_ctx.cam_path[p->channel_id];
		break;
	default:
		pr_err("fail to get valid channel %d\n", p->channel_id);
		return -EINVAL;
	}

	CAM_TRACE("set path%d frame addr,status %d reserved_buf %d\n",
		p->channel_id, path->status, p->is_reserved_buf);

	if (unlikely(p->fd_array[0] == 0)) {
		pr_err("fail to get chn %d fd\n", p->channel_id);
		ret = -EINVAL;
		goto exit;
	}

	if (p->is_reserved_buf == 1) {
		path->frm_reserved_addr.yaddr = p->frame_addr_array[0].y;
		path->frm_reserved_addr.uaddr = p->frame_addr_array[0].u;
		path->frm_reserved_addr.vaddr = p->frame_addr_array[0].v;
		path->frm_reserved_addr.yaddr_vir =
			p->frame_addr_vir_array[0].y;
		path->frm_reserved_addr.uaddr_vir =
			p->frame_addr_vir_array[0].u;
		path->frm_reserved_addr.vaddr_vir =
			p->frame_addr_vir_array[0].v;
		path->frm_reserved_addr.mfd_y = p->fd_array[0];
		path->frm_reserved_addr.mfd_u = p->fd_array[0];
		path->frm_reserved_addr.mfd_v = p->fd_array[0];
		path->frm_reserved_addr.buf_info.dev =
			&camerafile->grp->pdev->dev;
		path->frm_reserved_addr.buf_info.type = CAM_BUF_USER_TYPE;
	} else {
		struct camera_addr frame_addr = {0};

		if (atomic_read(&dev->stream_on) == 1 &&
			path->status == PATH_RUN) {

			for (i = 0; i < p->buffer_count; i++) {
				if (p->fd_array[0] == 0) {
					pr_err("fail to get valid fd\n");
					ret = -EINVAL;
					goto exit;
				}
				frame_addr.yaddr = p->frame_addr_array[i].y;
				frame_addr.uaddr = p->frame_addr_array[i].u;
				frame_addr.vaddr = p->frame_addr_array[i].v;
				frame_addr.yaddr_vir =
						p->frame_addr_vir_array[i].y;
				frame_addr.uaddr_vir =
						p->frame_addr_vir_array[i].u;
				frame_addr.vaddr_vir =
						p->frame_addr_vir_array[i].v;
				frame_addr.mfd_y = p->fd_array[i];
				frame_addr.mfd_u = p->fd_array[i];
				frame_addr.mfd_v = p->fd_array[i];
				frame_addr.buf_info.dev =
					&camerafile->grp->pdev->dev;

				if (p->channel_id == CAMERA_FULL_PATH) {
					ret = sprd_dcam_full_path_cfg_set(idx,
						DCAM_PATH_OUTPUT_ADDR,
						&frame_addr);
					if (unlikely(ret)) {
						pr_err("fail to full addr\n");
						goto exit;
					}
				} else if (p->channel_id == CAMERA_BIN_PATH) {
					ret = sprd_dcam_bin_path_cfg_set(idx,
						DCAM_PATH_OUTPUT_ADDR,
						&frame_addr);
					if (unlikely(ret)) {
						pr_err("fail to bin addr\n");
						goto exit;
					}
				} else {
					path_index =
						sprd_camioctl_path_idx_get(
							p->channel_id);
					ret = sprd_isp_drv_path_cfg_set(
						dev->isp_dev_handle,
						path_index,
						ISP_PATH_OUTPUT_ADDR,
						&frame_addr);
					if (unlikely(ret)) {
						pr_err("fail to cfg isp\n");
						goto exit;
					}
				}
			}
		} else {

			for (i = 0; i < p->buffer_count; i++) {
				if (unlikely(p->fd_array[0] == 0)) {
					pr_err("fail to get valid fd\n");
					ret = -EINVAL;
					goto exit;
				}
				frame_addr.yaddr =
					p->frame_addr_array[i].y;
				frame_addr.uaddr =
					p->frame_addr_array[i].u;
				frame_addr.vaddr =
					p->frame_addr_array[i].v;
				frame_addr.yaddr_vir =
					p->frame_addr_vir_array[i].y;
				frame_addr.uaddr_vir =
					p->frame_addr_vir_array[i].u;
				frame_addr.vaddr_vir =
					p->frame_addr_vir_array[i].v;
				frame_addr.mfd_y = p->fd_array[i];
				frame_addr.mfd_u = p->fd_array[i];
				frame_addr.mfd_v = p->fd_array[i];
				frame_addr.buf_info.dev =
					&camerafile->grp->pdev->dev;

				ret = sprd_cam_queue_buf_write(
					&path->buf_queue,
					&frame_addr);
				pr_debug("y=0x%x u=0x%x mfd=0x%x 0x%x\n",
					frame_addr.yaddr, frame_addr.uaddr,
					frame_addr.mfd_y, frame_addr.mfd_u);
			}
		}
	}

exit:
	return ret;
}

static int sprd_camioctl_bin_size_get(struct camera_path_spec *pre,
	struct camera_path_spec *vid, struct camera_path_spec *bin,
	struct camera_path_spec *full)
{
	int ret = 0;
	int zoom_ratio = 1;
	struct crop_param_t crop_param;
	struct rawsizer_param_t rawsizer_parm;

	memset(&crop_param, 0x00, sizeof(crop_param));
	memset(&rawsizer_parm, 0x00, sizeof(rawsizer_parm));
	if (!pre || !vid || !bin || !full) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		return ret;
	}

	if (pre->is_work) {
		if (!bin->is_work) {
			bin->in_size = pre->in_size;
			bin->in_rect.w = pre->in_rect.w;
			bin->in_rect.h = pre->in_rect.h;
			bin->out_size = pre->out_size;
			if (bin->need_downsizer) {
				if (pre->in_size.w > ISP_PATH1_LINE_BUF_LENGTH)
					bin->zoom_info.zoom_mode = ZOOM_BINNING;
				else
					bin->zoom_info.zoom_mode
						= ZOOM_RAWSIZER;
				sprd_dcam_drv_init_online_pipe(&bin->zoom_info,
					pre->in_size.w,
					pre->in_size.h,
					pre->out_size.w,
					pre->out_size.h);
				zoom_ratio = pre->in_size.w * 1000
						/ pre->in_rect.w;
				pr_debug("in_size.w = %d, in_rect.w = %d, ratio = %d\n",
					pre->in_size.w,
					pre->in_rect.w,
					zoom_ratio);
				sprd_dcam_drv_update_crop_param(
					&crop_param,
					&bin->zoom_info, zoom_ratio);
				sprd_dcam_drv_update_rawsizer_param(
					&rawsizer_parm,
					&bin->zoom_info);
				bin->in_size.w = pre->in_size.w;
				bin->in_size.h = pre->in_size.h;
				bin->in_rect.x =
					bin->zoom_info.crop_startx;
				bin->in_rect.y =
					bin->zoom_info.crop_starty;
				bin->in_rect.w =
					bin->zoom_info.crop_width;
				bin->in_rect.h =
					bin->zoom_info.crop_height;
				bin->out_size.w =
					rawsizer_parm.output_width;
				bin->out_size.h =
					rawsizer_parm.output_height;
				sprd_dcam_drv_init_online_pipe(&bin->zoom_info,
					pre->in_size.w,
					pre->in_size.h,
					pre->out_size.w,
					pre->out_size.h);
				sprd_dcam_drv_update_crop_param(
					&crop_param,
					&bin->zoom_info, 1000);
				sprd_dcam_drv_update_rawsizer_param(
					&rawsizer_parm,
					&bin->zoom_info);
				bin->max_out_size.w =
					rawsizer_parm.output_width;
				bin->max_out_size.h =
					rawsizer_parm.output_height;
			} else {
				sprd_camioctl_size_convert(pre, bin);
				bin->in_rect.x = 0;
				bin->in_rect.y = 0;
				bin->in_rect.w = bin->in_size.w;
				bin->in_rect.h = bin->in_size.h;
				bin->out_size = pre->in_size;
			}
			bin->is_work = 1;
			bin->assoc_idx = 1 << CAMERA_PRE_PATH;
			bin->out_fmt = DCAM_RAWRGB;
			pre->assoc_idx = 1 << CAMERA_BIN_PATH;
			if (vid->is_work)
				sprd_camioctl_size_convert(vid, bin);
		}
	}

	if (vid->is_work) {
		if (full->is_work && vid->in_size.w < CAMERA_PFC_OPT_WIDTH
			&& vid->in_size.h < CAMERA_PFC_OPT_HEIGHT) {
			full->assoc_idx |= 1 << CAMERA_VID_PATH;
			vid->assoc_idx = 1 << CAMERA_FULL_PATH;
		} else if (!bin->is_work) {
			bin->in_size = vid->in_size;
			bin->in_rect.w = vid->in_rect.w;
			bin->in_rect.h = vid->in_rect.h;
			bin->out_size = vid->out_size;
			if (bin->need_downsizer) {
				sprd_dcam_drv_init_online_pipe(&bin->zoom_info,
					vid->in_size.w,
					vid->in_size.h,
					vid->out_size.w,
					vid->out_size.h);
				zoom_ratio = vid->in_size.w * 1000
						/ vid->in_rect.w;
				pr_debug("in_size.w = %d, in_rect.w = %d, ratio = %d\n",
					vid->in_size.w,
					vid->in_rect.w,
					zoom_ratio);
				sprd_dcam_drv_update_crop_param(
					&crop_param,
					&bin->zoom_info, zoom_ratio);
				sprd_dcam_drv_update_rawsizer_param(
					&rawsizer_parm,
					&bin->zoom_info);
				bin->in_size.w = vid->in_size.w;
				bin->in_size.h = vid->in_size.h;
				bin->in_rect.x =
					bin->zoom_info.crop_startx;
				bin->in_rect.y =
					bin->zoom_info.crop_starty;
				bin->in_rect.w =
					bin->zoom_info.crop_width;
				bin->in_rect.h =
					bin->zoom_info.crop_height;
				bin->out_size.w =
					rawsizer_parm.output_width;
				bin->out_size.h =
					rawsizer_parm.output_height;
				sprd_dcam_drv_init_online_pipe(&bin->zoom_info,
					vid->in_size.w,
					vid->in_size.h,
					vid->out_size.w,
					vid->out_size.h);
				sprd_dcam_drv_update_crop_param(
					&crop_param,
					&bin->zoom_info, 1000);
				sprd_dcam_drv_update_rawsizer_param(
					&rawsizer_parm,
					&bin->zoom_info);
				bin->max_out_size.w =
					rawsizer_parm.output_width;
				bin->max_out_size.h =
					rawsizer_parm.output_height;
			} else {
				sprd_camioctl_size_convert(vid, bin);
				bin->in_rect.x = 0;
				bin->in_rect.y = 0;
				bin->in_rect.w = bin->in_size.w;
				bin->in_rect.h = bin->in_size.h;
				bin->out_size = vid->in_size;
			}
			bin->is_work = 1;
			bin->assoc_idx |= 1 << CAMERA_VID_PATH;
			bin->out_fmt = DCAM_RAWRGB;
			vid->assoc_idx = 1 << CAMERA_BIN_PATH;
		} else {
			bin->assoc_idx |= 1 << CAMERA_VID_PATH;
			vid->assoc_idx = 1 << CAMERA_BIN_PATH;
		}
	}

	return ret;
}

static int sprd_camioctl_camera_raw_pipeline_cfg(struct camera_file *camerafile)
{
	int ret = 0;
	struct camera_context *ctx = NULL;
	struct camera_dev *dev = NULL;
	struct camera_dev *dev1 = NULL;

	struct camera_path_spec *path_pre = NULL;
	struct camera_path_spec *path_vid = NULL;
	struct camera_path_spec *path_cap = NULL;
	struct camera_path_spec *path_full = NULL;
	struct camera_path_spec *path_bin = NULL;
	struct camera_path_spec *path = NULL;

	sprd_camioctl_dev_get(camerafile, &dev, &ctx);
	if (!ctx) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		return ret;
	}

	dev1 = camerafile->grp->dev[DCAM_ID_1];
	path_pre = &ctx->cam_path[CAMERA_PRE_PATH];
	path_vid = &ctx->cam_path[CAMERA_VID_PATH];
	path_cap = &ctx->cam_path[CAMERA_CAP_PATH];
	path_full = &ctx->cam_path[CAMERA_FULL_PATH];
	path_bin = &ctx->cam_path[CAMERA_BIN_PATH];

	if (path_cap->is_work) {
		if (!path_full->is_work) {
			path_full->is_work = 1;
			path_full->in_rect.x = 0;
			path_full->in_rect.y = 0;
			path_full->in_rect.w = path_cap->in_size.w;
			path_full->in_rect.h = path_cap->in_size.h;
			path_full->out_size = path_cap->in_size;
			if (ctx->need_4in1)
				path_full->src_sel = 0;
			else
				path_full->src_sel = 1;
			path_full->assoc_idx = 1 << CAMERA_CAP_PATH;
			path_full->out_fmt = DCAM_RAWRGB;
			path_cap->assoc_idx = 1 << CAMERA_FULL_PATH;
			path_full->frm_reserved_addr =
				path_cap->frm_reserved_addr;
			path_full->in_size = path_cap->in_size;
		}
	}

	if (camerafile->idx != DCAM_ID_2) {
		path_bin->need_downsizer = ctx->need_downsizer;
		ret = sprd_camioctl_bin_size_get(path_pre, path_vid,
			path_bin, path_full);
		if (unlikely(ret)) {
			pr_err("fail to get bin_path size %d\n", ret);
			return ret;
		}
	} else if (path_pre->is_work) {
		path_full->is_work = 1;
		path_full->in_size = path_pre->in_size;
		path_full->in_rect.x = 0;
		path_full->in_rect.y = 0;
		path_full->in_rect.w = path_pre->in_size.w;
		path_full->in_rect.h = path_pre->in_size.h;
		path_full->out_size = path_pre->in_size;
		path_full->src_sel = 0;
		path_full->assoc_idx = 1 << CAMERA_PRE_PATH;
		path_full->out_fmt = DCAM_RAWRGB;
		path_pre->assoc_idx = 1 << CAMERA_FULL_PATH;
	}

	if (ctx->need_4in1 && path_bin->is_work) {
		path_bin->in_size.w = path_bin->in_size.w >> 1;
		path_bin->in_size.h = path_bin->in_size.h >> 1;
		path_bin->in_rect.w = path_bin->in_rect.w >> 1;
		path_bin->in_rect.h = path_bin->in_rect.h >> 1;
		path_bin->in_rect.x = (path_bin->in_size.w -
			path_bin->in_rect.w) >> 1;
		path_bin->in_rect.x = CAM_ALIGNTO(path_bin->in_rect.x);
		path_bin->in_rect.y = (path_bin->in_size.h -
			path_bin->in_rect.h) >> 1;
		path_bin->in_rect.y = CAM_ALIGNTO(path_bin->in_rect.y);
		if (path_bin->need_downsizer) {
			if (path_bin->in_size.w > ISP_PATH1_LINE_BUF_LENGTH)
				path_bin->zoom_info.zoom_mode = ZOOM_BINNING;
			else
				path_bin->zoom_info.zoom_mode = ZOOM_RAWSIZER;
		}
	}

	if (path_full->is_work) {
		path = path_full;
		if (ctx->need_4in1 && ctx->need_isp_tool != 1) {
			struct camera_addr cam_addr = {0};
			struct camera_size size_tmp = {0};
			size_t size;
			uint32_t param = 0;

			if (ctx->need_isp_tool)
				size_tmp = path_full->out_size;
			else
				size_tmp = path_cap->in_size;

			path = &dev1->cam_ctx.cam_path[CAMERA_BIN_PATH];
			path->buf_num = 1;

			size = sprd_cam_com_raw_pitch_calc(0, size_tmp.w)
				* size_tmp.h;
			sprd_cam_buf_alloc(&cam_addr.buf_info,
				DCAM_ID_1,
				&s_dcam_pdev->dev, size, 1,
				CAM_BUF_SWAP_TYPE);
			sprd_dcam_bin_path_clear(DCAM_ID_1);
			param = 1;
			ret = sprd_dcam_bin_path_cfg_set(DCAM_ID_1,
				DCAM_PATH_ENABLE, &param);
			if (unlikely(ret)) {
				pr_err("fail to enable bin_path\n");
				return ret;
			}
			ret = sprd_dcam_bin_path_cfg_set(DCAM_ID_1,
				DCAM_PATH_BUF_NUM, &path->buf_num);
			if (unlikely(ret)) {
				pr_err("fail to cfg bin_path buf num\n");
				return ret;
			}
			ret = sprd_dcam_bin_path_cfg_set(DCAM_ID_1,
				DCAM_PATH_OUTPUT_ADDR, &cam_addr);
			if (unlikely(ret)) {
				pr_err("fail to cfg bin_path addr %d\n", ret);
				return ret;
			}
		} else if (path_full->assoc_idx != 0
			&& !ctx->need_4in1) {
			struct camera_addr cam_addr = {0};
			size_t size;
			int i;

			size = sprd_cam_com_raw_pitch_calc(0, path->out_size.w)
				* path->out_size.h;
			path->buf_num = DCAM_FRM_QUEUE_LENGTH;
			for (i = 0; i < path->buf_num; i++) {
				sprd_cam_buf_alloc(&cam_addr.buf_info,
					camerafile->idx,
					&s_dcam_pdev->dev, size, 1,
					CAM_BUF_SWAP_TYPE);
				sprd_cam_queue_buf_write(&path->buf_queue,
					&cam_addr);
			}
			sprd_cam_buf_alloc(&cam_addr.buf_info,
				camerafile->idx,
				&s_dcam_pdev->dev, size, 1,
				CAM_BUF_SWAP_TYPE);
			path->frm_reserved_addr = cam_addr;
		} else if (camerafile->idx != DCAM_ID_2) {
			/*if it is raw capture*/
			path->buf_num = 1;
		}
	}

	if (path_bin->is_work && path_bin->assoc_idx != 0) {
		struct camera_addr cam_addr = {0};
		size_t size;
		int i;

		pr_debug("bin path max out w = %d, h = %d\n",
			path_bin->max_out_size.w, path_bin->max_out_size.h);
		path = path_bin;
		if (!ctx->need_downsizer)
			size = sprd_cam_com_raw_pitch_calc(0, path->out_size.w)
					* path->out_size.h;
		else
			size = sprd_cam_com_raw_pitch_calc(0,
				path->max_out_size.w) * path->max_out_size.h;
		path->buf_num = DCAM_FRM_QUEUE_LENGTH;
		for (i = 0; i < DCAM_FRM_QUEUE_LENGTH; i++) {
			sprd_cam_buf_alloc(&cam_addr.buf_info,
				camerafile->idx, &s_dcam_pdev->dev,
				size, 1, CAM_BUF_SWAP_TYPE);
			sprd_cam_queue_buf_write(&path->buf_queue,
				&cam_addr);
		}
		sprd_cam_buf_alloc(&cam_addr.buf_info, camerafile->idx,
			&s_dcam_pdev->dev, size, 1,
			CAM_BUF_SWAP_TYPE);
		path->frm_reserved_addr = cam_addr;
	}

	if ((path_bin->is_work && path_bin->assoc_idx != 0)
		|| (path_full->is_work && path_full->assoc_idx != 0))
		dev->isp_work = 1;

	pr_debug("path is_work: %d %d %d %d %d\n",
		path_full->is_work, path_bin->is_work,
		path_pre->is_work, path_cap->is_work, path_vid->is_work);
	pr_debug("path assoc_idx: 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		path_full->assoc_idx, path_bin->assoc_idx,
		path_pre->assoc_idx, path_cap->assoc_idx,
		path_vid->assoc_idx);

	return ret;
}

static int sprd_camioctl_camera_yuv_pipeline_cfg(struct camera_file *camerafile)
{
	int ret = 0;
	struct camera_context *ctx = NULL;
	struct camera_dev *dev = NULL;

	struct camera_path_spec *path_pre = NULL;
	struct camera_path_spec *path_vid = NULL;
	struct camera_path_spec *path_cap = NULL;
	struct camera_path_spec *path_full = NULL;
	struct camera_path_spec *path = NULL;

	sprd_camioctl_dev_get(camerafile, &dev, &ctx);
	if (!ctx) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		return ret;
	}

	path_pre = &ctx->cam_path[CAMERA_PRE_PATH];
	path_vid = &ctx->cam_path[CAMERA_VID_PATH];
	path_cap = &ctx->cam_path[CAMERA_CAP_PATH];
	path_full = &ctx->cam_path[CAMERA_FULL_PATH];

	if (!path_full->is_work) {
		path_full->is_work = 1;
		path_full->src_sel = 0;
		path_full->out_fmt = DCAM_YUV420;
		path_full->in_rect.x = 0;
		path_full->in_rect.y = 0;
	}

	if (path_pre->is_work) {
		path_full->in_rect.w = path_pre->in_size.w;
		path_full->in_rect.h = path_pre->in_size.h;
		path_full->out_size = path_pre->in_size;
		path_full->assoc_idx = 1 << CAMERA_PRE_PATH;
		path_pre->assoc_idx = 1 << CAMERA_FULL_PATH;
		path_full->in_size = path_pre->in_size;
	}
	if (path_vid->is_work) {
		path_full->in_rect.w = path_vid->in_size.w;
		path_full->in_rect.h = path_vid->in_size.h;
		path_full->out_size = path_vid->in_size;
		path_full->assoc_idx |= 1 << CAMERA_VID_PATH;
		path_vid->assoc_idx = 1 << CAMERA_FULL_PATH;
		path_full->in_size = path_vid->in_size;
	}
	if (path_cap->is_work) {
		path_full->in_rect.w = path_cap->in_size.w;
		path_full->in_rect.h = path_cap->in_size.h;
		path_full->out_size = path_cap->in_size;
		path_full->assoc_idx |= 1 << CAMERA_CAP_PATH;
		path_cap->assoc_idx = 1 << CAMERA_FULL_PATH;
		path_full->in_size = path_cap->in_size;
	}

	if (path_full->is_work) {
		path = path_full;
		if (path_full->assoc_idx != 0) {
			struct camera_addr cam_addr = {0};
			size_t size;
			int i;

			size = path->out_size.w * path->out_size.h * 3 / 2;
			cam_addr.uaddr = path->out_size.w * path->out_size.h;
			path->buf_num = DCAM_FRM_QUEUE_LENGTH;
			for (i = 0; i < path->buf_num; i++) {
				sprd_cam_buf_alloc(&cam_addr.buf_info,
					camerafile->idx,
					&s_dcam_pdev->dev, size, 1,
					CAM_BUF_SWAP_TYPE);
				sprd_cam_queue_buf_write(&path->buf_queue,
					&cam_addr);
			}
			sprd_cam_buf_alloc(&cam_addr.buf_info,
				camerafile->idx,
				&s_dcam_pdev->dev, size, 1,
				CAM_BUF_SWAP_TYPE);
			path->frm_reserved_addr = cam_addr;
		} else {
			/*if it is raw capture*/
			path->buf_num = 1;
		}
	}

	if (path_full->is_work && path_full->assoc_idx != 0)
		dev->isp_work = 1;

	return ret;
}

static int sprd_camioctl_camera_pipeline_cfg(struct camera_file *camerafile)
{
	int ret = 0;
	struct camera_context *ctx = NULL;
	struct camera_dev *dev = NULL;

	sprd_camioctl_dev_get(camerafile, &dev, &ctx);
	if (!ctx) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		return ret;
	}

	sprd_dcam_drv_path_clear(camerafile->idx);
	sprd_isp_drv_path_clear(dev->isp_dev_handle);

	if (ctx->sn_mode == DCAM_CAP_MODE_YUV)
		sprd_camioctl_camera_yuv_pipeline_cfg(camerafile);
	else if (ctx->sn_mode == DCAM_CAP_MODE_RAWRGB)
		sprd_camioctl_camera_raw_pipeline_cfg(camerafile);

	return ret;
}

static int sprd_camioctl_dcam_cap_cfg(struct camera_context *ctx,
		enum dcam_id idx)
{
	int ret = DCAM_RTN_SUCCESS;

	if (ctx == NULL) {
		pr_err("fail to get valid input ptr\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = sprd_dcam_cap_cfg_set(idx, DCAM_CAP_SENSOR_MODE,
		&ctx->sn_mode);
	if (unlikely(ret)) {
		pr_err("fail to cfg dcam cap sensor mode\n");
		goto exit;
	}

	ret = sprd_dcam_cap_cfg_set(idx, DCAM_CAP_SAMPLE_MODE,
		&ctx->capture_mode);
	if (unlikely(ret)) {
		pr_err("fail to cfg dcam cap sample mode\n");
		goto exit;
	}

	ret = sprd_dcam_cap_cfg_set(idx, DCAM_CAP_HREF_SEL,
		&ctx->sync_pol);
	if (unlikely(ret)) {
		pr_err("fail to cfg dcam cap sync pol\n");
		goto exit;
	}

	ret = sprd_dcam_cap_cfg_set(idx, DCAM_CAP_DATA_BITS,
		&ctx->data_bits);
	if (unlikely(ret)) {
		pr_err("fail to cfg dcam cap data bits\n");
		goto exit;
	}

	ret = sprd_dcam_cap_cfg_set(idx, DCAM_CAP_DATA_PATTERN,
				&ctx->img_ptn);
	if (unlikely(ret)) {
		pr_err("fail to cfg dcam cap data bits\n");
		goto exit;
	}

	ret = sprd_dcam_cap_cfg_set(idx, DCAM_CAP_FRM_DECI, &ctx->frm_deci);
	if (unlikely(ret)) {
		pr_err("fail to cfg dcam cap frame deci\n");
		goto exit;
	}

	ret = sprd_dcam_cap_cfg_set(idx, DCAM_CAP_INPUT_RECT,
		&ctx->cap_in_rect);
	if (unlikely(ret)) {
		pr_err("fail to cfg dcam cap input rect\n");
		goto exit;
	}

	ret = sprd_dcam_cap_cfg_set(idx, DCAM_CAP_FRM_COUNT_CLR, NULL);
	if (unlikely(ret)) {
		pr_err("fail to cfg dcam cap frame count clear\n");
		goto exit;
	}

	ret = sprd_dcam_cap_cfg_set(idx, DCAM_CAP_PRE_SKIP_CNT,
		&ctx->skip_number);
	if (unlikely(ret)) {
		pr_err("fail to cfg dcam cap pre skip count\n");
		goto exit;
	}

	ret = sprd_dcam_cap_cfg_set(idx, DCAM_CAP_IMAGE_XY_DECI,
		&ctx->img_deci);
	if (unlikely(ret)) {
		pr_err("fail to cfg dcam cap image xy deci\n");
		goto exit;
	}

	if (ctx->need_4in1) {
		ret = sprd_dcam_cap_cfg_set(idx, DCAM_CAP_4IN1_BYPASS, NULL);
		if (unlikely(ret)) {
			pr_err("fail to cfg dcam cap 4in1 bypass\n");
			goto exit;
		}
	}

	ret = sprd_dcam_cap_cfg_set(idx, DCAM_CAP_DUAL_MODE,
		&ctx->dual_cam);
	if (unlikely(ret)) {
		pr_err("fail to cfg dcam dual mode\n");
		goto exit;
	}

exit:
	return ret;
}

static int sprd_camioctl_full_path_cfg(struct camera_path_spec *full_path,
	enum dcam_id idx)
{
	int ret = 0;
	uint32_t param = 0;
	struct camera_addr cur_node = {0};

	if (full_path == NULL) {
		pr_err("fail to get valid input ptr\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = sprd_dcam_full_path_cfg_set(idx, DCAM_PATH_BUF_NUM,
		&full_path->buf_num);
	if (unlikely(ret)) {
		pr_err("fail to cfg full_path buf num\n");
		goto exit;
	}

	ret = sprd_dcam_full_path_cfg_set(idx, DCAM_PATH_SRC_SEL,
		&full_path->src_sel);
	if (unlikely(ret)) {
		pr_err("fail to cfg full_path src\n");
		goto exit;
	}

	ret = sprd_dcam_full_path_cfg_set(idx, DCAM_PATH_INPUT_SIZE,
		&full_path->in_size);
	if (unlikely(ret)) {
		pr_err("fail to cfg full_path input rect\n");
		goto exit;
	}

	ret = sprd_dcam_full_path_cfg_set(idx, DCAM_PATH_INPUT_RECT,
		&full_path->in_rect);
	if (unlikely(ret)) {
		pr_err("fail to cfg full_path input rect\n");
		goto exit;
	}

	ret = sprd_dcam_full_path_cfg_set(idx, DCAM_PATH_OUTPUT_FORMAT,
		&full_path->out_fmt);
	if (unlikely(ret)) {
		pr_err("fail to cfg full_path format\n");
		goto exit;
	}

	param = full_path->pixel_depth > 10 ? 1 : 0;
	ret = sprd_dcam_full_path_cfg_set(idx, DCAM_PATH_OUTPUT_LOOSE,
		&param);
	if (unlikely(ret)) {
		pr_err("fail to cfg full_path format\n");
		goto exit;
	}

	ret = sprd_dcam_full_path_cfg_set(idx, DCAM_PATH_DATA_ENDIAN,
		&full_path->end_sel);
	if (unlikely(ret)) {
		pr_err("fail to cfg full_path data endian\n");
		goto exit;
	}

	while (sprd_cam_queue_buf_read(&full_path->buf_queue,
		&cur_node) == 0) {
		ret = sprd_dcam_full_path_cfg_set(idx, DCAM_PATH_OUTPUT_ADDR,
			&cur_node);
		if (unlikely(ret)) {
			pr_err("fail to cfg full_path output addr\n");
			goto exit;
		}
	}

	ret = sprd_dcam_full_path_cfg_set(idx,
		DCAM_PATH_OUTPUT_RESERVED_ADDR,
		&full_path->frm_reserved_addr);
	if (unlikely(ret)) {
		pr_err("fail to cfg full_path output reserved addr\n");
		goto exit;
	}

	ret = sprd_dcam_bin_path_cfg_set(idx,
		DCAM_PATH_ASSOC,
		&full_path->assoc_idx);

	param = 1;
	ret = sprd_dcam_full_path_cfg_set(idx, DCAM_PATH_ENABLE, &param);
	if (unlikely(ret)) {
		pr_err("fail to enable full_path\n");
		goto exit;
	}

exit:
	return ret;
}

static int sprd_camioctl_bin_path_cfg(struct camera_path_spec *bin_path,
	enum dcam_id idx)
{
	int ret = 0;
	uint32_t param = 0;
	struct camera_addr cur_node = {0};

	if (bin_path == NULL) {
		pr_err("fail to get valid input ptr\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = sprd_dcam_bin_path_cfg_set(idx, DCAM_PATH_BUF_NUM,
		&bin_path->buf_num);
	if (unlikely(ret)) {
		pr_err("fail to cfg bin_path buf num\n");
		goto exit;
	}

	ret = sprd_dcam_bin_path_cfg_set(idx, DCAM_PATH_INPUT_SIZE,
		&bin_path->in_size);
	if (unlikely(ret)) {
		pr_err("fail to cfg bin_path input rect\n");
		goto exit;
	}

	ret = sprd_dcam_bin_path_cfg_set(idx, DCAM_PATH_INPUT_RECT,
		&bin_path->in_rect);
	if (unlikely(ret)) {
		pr_err("fail to cfg bin_path input rect\n");
		goto exit;
	}

	ret = sprd_dcam_bin_path_cfg_set(idx, DCAM_PATH_OUTPUT_FORMAT,
		&bin_path->out_fmt);
	if (unlikely(ret)) {
		pr_err("fail to cfg bin_path format\n");
		goto exit;
	}

	param = bin_path->pixel_depth > 10 ? 1 : 0;
	ret = sprd_dcam_bin_path_cfg_set(idx, DCAM_PATH_OUTPUT_LOOSE,
		&param);
	if (unlikely(ret)) {
		pr_err("fail to cfg bin_path format\n");
		goto exit;
	}

	ret = sprd_dcam_bin_path_cfg_set(idx, DCAM_PATH_DATA_ENDIAN,
		&bin_path->end_sel);
	if (unlikely(ret)) {
		pr_err("fail to cfg bin_path data endian\n");
		goto exit;
	}

	ret = sprd_dcam_bin_path_cfg_set(idx, DCAM_PATH_OUTPUT_SIZE,
		&bin_path->out_size);
	if (unlikely(ret)) {
		pr_err("fail to cfg bin_path size\n");
		goto exit;
	}

	while (sprd_cam_queue_buf_read(&bin_path->buf_queue, &cur_node) == 0) {

		ret = sprd_dcam_bin_path_cfg_set(idx, DCAM_PATH_OUTPUT_ADDR,
			&cur_node);
		if (unlikely(ret)) {
			pr_err("fail to cfg bin_path output addr %d\n", ret);
			goto exit;
		}
	}

	if (!(bin_path->assoc_idx & (1 << CAMERA_CAP_PATH))) {
		ret = sprd_dcam_bin_path_cfg_set(idx,
			DCAM_PATH_OUTPUT_RESERVED_ADDR,
			&bin_path->frm_reserved_addr);
		if (unlikely(ret)) {
			pr_err("fail to cfg bin_path output reserved addr\n");
			goto exit;
		}
	}

	ret = sprd_dcam_bin_path_cfg_set(idx,
		DCAM_PATH_ASSOC,
		&bin_path->assoc_idx);

	param = 1;
	ret = sprd_dcam_bin_path_cfg_set(idx, DCAM_PATH_ENABLE, &param);
	if (unlikely(ret)) {
		pr_err("fail to enable bin_path\n");
		goto exit;
	}

	ret = sprd_dcam_bin_path_cfg_set(idx, DCAM_PATH_NEED_DOWNSIZER,
		&bin_path->need_downsizer);
	if (unlikely(ret)) {
		pr_err("fail to cfg bin_path need downsizer flag\n");
		goto exit;
	}

	ret = sprd_dcam_bin_path_cfg_set(idx, DCAM_PATH_ZOOM_INFO,
		&bin_path->zoom_info);
	if (unlikely(ret)) {
		pr_err("fail to cfg bin_path zoom_info flag\n");
		goto exit;
	}
exit:
	return ret;
}

static int sprd_camioctl_dcam_cfg(struct camera_dev *dev)
{
	int ret = 0;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_context *ctx = NULL;
	struct camera_path_spec *full_path = NULL;
	struct camera_path_spec *bin_path = NULL;
	struct dcam_module *dcam_module = NULL;
	struct camera_size size = {0};

	ctx = &dev->cam_ctx;
	idx = dev->idx;
	full_path = &ctx->cam_path[CAMERA_FULL_PATH];
	bin_path = &ctx->cam_path[CAMERA_BIN_PATH];
	dcam_module = sprd_dcam_drv_module_get(dev->idx);

	ret = sprd_camioctl_dcam_reg_isr(dev);
	if (unlikely(ret)) {
		pr_err("fail to register dcam isr\n");
		goto exit;
	}

	/* config cap sub-module */
	ret = sprd_camioctl_dcam_cap_cfg(ctx, idx);
	if (unlikely(ret)) {
		pr_err("fail to config cap\n");
		goto exit;
	}

	/* config dcam_if full_path */
	if (full_path->is_work) {
		ret = sprd_camioctl_full_path_cfg(full_path, idx);
		if (unlikely(ret)) {
			pr_err("fail to config full_path cap\n");
			goto exit;
		}
		full_path->status = PATH_RUN;
	}

	/* config dcam_if bin_path */
	if (bin_path->is_work) {
		ret = sprd_camioctl_bin_path_cfg(bin_path, idx);
		if (unlikely(ret)) {
			pr_err("fail to config bin_path cap\n");
			goto exit;
		}
		bin_path->status = PATH_RUN;
	}
	/* for dcam 3dnr_me */
	if (ctx->need_3dnr) {
		size.w = ctx->cap_in_size.w;
		size.h = ctx->cap_in_size.h;
		ret = sprd_dcam_drv_3dnr_fast_me_info_get(idx, ctx->need_3dnr,
			&size);
		if (unlikely(ret)) {
			pr_err("fail to cfg_nr3_fast_me info\n");
			goto exit;
		}
	}

	if (dev->init_inptr.statis_valid) {
		ret = sprd_cam_statistic_buf_cfg(
				&dcam_module->statis_module_info,
				&dev->init_inptr);
		if (ret != 0) {
			pr_err("fail to cfg statis!\n");
			goto exit;
		}
	}

exit:
	return ret;
}

static int sprd_camioctl_isp_path_buf_cfg(void *handle,
	enum isp_config_param cfg_id, enum isp_path_index path_index,
	struct cam_buf_queue *queue)
{
	int ret = 0;
	struct camera_addr cur_node;

	if (!queue || !handle) {
		pr_err("fail to get valid input ptr\n");
		ret = -EINVAL;
		return ret;

	}

	while (sprd_cam_queue_buf_read(queue, &cur_node) == 0)
		ret = sprd_isp_drv_path_cfg_set(handle, path_index,
			cfg_id, &cur_node);

	return ret;
}

static int sprd_camioctl_isp_path_block_cfg(struct camera_path_spec *path,
	void **handle, enum isp_path_index path_index)
{
	int ret = 0;
	uint32_t param = 0;
	struct isp_endian_sel endian;
	struct isp_regular_info regular_info;
	struct camera_size me_conv_size = {0};
	struct camera_size sns_size = {0};
	struct camera_dev *dev = NULL;

	memset(&endian, 0x00, sizeof(endian));

	if (!path || !handle) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	dev = container_of(handle, struct camera_dev, isp_dev_handle);

	if (!dev) {
		pr_err("fail to get valid dev\n");
		return -EINVAL;
	}

	ret = sprd_isp_drv_path_cfg_set(*handle, path_index,
		ISP_PATH_INPUT_SIZE, &path->in_size);
	if (unlikely(ret)) {
		pr_err("fail to cfg path %d input size\n", path_index);
		goto exit;
	}

	ret = sprd_isp_drv_path_cfg_set(*handle, path_index,
		ISP_PATH_INPUT_RECT, &path->in_rect);
	if (unlikely(ret)) {
		pr_err("fail to cfg path %d input rect\n", path_index);
		goto exit;
	}

	ret = sprd_isp_drv_path_cfg_set(*handle, path_index,
		ISP_PATH_OUTPUT_SIZE, &path->out_size);
	if (unlikely(ret)) {
		pr_err("fail to cfg path %d output size\n", path_index);
		goto exit;
	}

	ret = sprd_isp_drv_path_cfg_set(*handle, path_index,
		ISP_PATH_OUTPUT_FORMAT, &path->out_fmt);
	if (unlikely(ret)) {
		pr_err("fail to cfg path %d output format\n", path_index);
		goto exit;
	}

	ret = sprd_camioctl_isp_path_buf_cfg(*handle, ISP_PATH_OUTPUT_ADDR,
		path_index, &path->buf_queue);
	if (unlikely(ret)) {
		pr_err("fail to cfg path %d output addr\n", path_index);
		goto exit;
	}

	ret = sprd_isp_drv_path_cfg_set(*handle, path_index,
		ISP_PATH_OUTPUT_RESERVED_ADDR,
		&path->frm_reserved_addr);
	if (unlikely(ret)) {
		pr_err("fail to cfg path %d output reserved addr\n",
			path_index);
		goto exit;
	}

	ret = sprd_isp_drv_path_cfg_set(*handle, path_index,
		ISP_PATH_FRM_DECI, &path->path_frm_deci);
	if (unlikely(ret)) {
		pr_err("fail to cfg path %d frame deci\n", path_index);
		goto exit;
	}

	ret = sprd_isp_drv_path_cfg_set(*handle, path_index,
		ISP_PATH_SKIP_NUM, &path->skip_num);
	if (unlikely(ret)) {
		pr_err("fail to cfg path %d skip num\n", path_index);
		goto exit;
	}

	memset(&regular_info, 0, sizeof(regular_info));
	regular_info.regular_mode = path->regular_desc.regular_mode;
	ret = sprd_isp_drv_path_cfg_set(*handle, path_index,
		ISP_PATH_SHRINK, &regular_info);
	if (unlikely(ret)) {
		pr_err("fail to cfg path %d shrink\n", path_index);
		goto exit;
	}

	param = 1;
	ret = sprd_isp_drv_path_cfg_set(*handle, path_index,
		ISP_PATH_ENABLE, &param);
	if (unlikely(ret)) {
		pr_err("fail to enable path %d\n", path_index);
		goto exit;
	}

	endian.y_endian = path->end_sel.y_endian;
	endian.uv_endian = path->end_sel.uv_endian;
	ret = sprd_isp_drv_path_cfg_set(*handle, path_index,
		ISP_PATH_DATA_ENDIAN, &endian);
	if (unlikely(ret)) {
		pr_err("fail to cfg path %d data endian\n", path_index);
		goto exit;
	}

	param = dev->cam_ctx.need_3dnr;
	ret = sprd_isp_drv_path_cfg_set(*handle, path_index,
		ISP_NR3_ENABLE, &param);
	if (unlikely(ret)) {
		pr_err("fail to cfg path %d need_3dnr\n", path_index);
		goto exit;
	}

	me_conv_size.w = dev->cam_ctx.cap_in_size.w;
	me_conv_size.h = dev->cam_ctx.cap_in_size.h;
	ret = sprd_isp_drv_path_cfg_set(*handle, path_index,
			ISP_NR3_ME_CONV_SIZE, &me_conv_size);
	if (unlikely(ret)) {
		pr_err("fail to cfg path %d me_conv size\n", path_index);
		goto exit;
	}

	sns_size.w = dev->cam_ctx.sn_max_size.w;
	sns_size.h = dev->cam_ctx.sn_max_size.h;
	ret = sprd_isp_drv_path_cfg_set(*handle, path_index,
		ISP_SNS_MAX_SIZE, &sns_size);
	if (unlikely(ret)) {
		pr_err("fail to cfg path %d cap_size\n", path_index);
		goto exit;
	}

	ret = sprd_isp_drv_path_cfg_set(*handle, path_index,
		ISP_PATH_ASSOC, &path->assoc_idx);
	if (unlikely(ret)) {
		pr_err("fail to cfg path %d cap_size\n", path_index);
		goto exit;
	}

exit:
	return ret;
}

static int sprd_camioctl_isp_path_cfg(struct camera_dev *dev)
{
	int ret = 0, path_not_work = 0;
	struct camera_context *ctx = NULL;
	struct camera_path_spec *path_pre = NULL;
	struct camera_path_spec *path_vid = NULL;
	struct camera_path_spec *path_cap = NULL;
	struct isp_pipe_dev *isp_dev = NULL;

	ctx = &dev->cam_ctx;
	path_pre = &ctx->cam_path[CAMERA_PRE_PATH];
	path_vid = &ctx->cam_path[CAMERA_VID_PATH];
	path_cap = &ctx->cam_path[CAMERA_CAP_PATH];
	isp_dev = (struct isp_pipe_dev *)dev->isp_dev_handle;

	isp_dev->sn_mode = ctx->sn_mode;
	isp_dev->module_info.need_downsizer = ctx->need_downsizer;
	pr_info("need_downsizer = %d\n",
		isp_dev->module_info.need_downsizer);
	ret = sprd_camioctl_isp_reg_isr(dev);
	if (unlikely(ret)) {
		pr_err("fail to register isp isr\n");
		goto exit;
	}
	ret = sprd_cam_statistic_queue_init(
		&isp_dev->module_info.statis_module_info,
		dev->idx, ISP_DEV_STATIS);
	if (ret) {
		pr_err("fail to init isp statis queue\n");
		return ret;
	}

	do {
		/* config isp pre path */
		if (path_pre->is_work) {
			ret = sprd_camioctl_isp_path_block_cfg(path_pre,
				&dev->isp_dev_handle, ISP_PATH_IDX_PRE);
			if (unlikely(ret)) {
				pr_err("fail to config path_pre\n");
				break;
			}
			path_pre->status = PATH_RUN;
		} else {
			ret = sprd_isp_drv_path_cfg_set(dev->isp_dev_handle,
				ISP_PATH_IDX_PRE, ISP_PATH_ENABLE,
				&path_not_work);
			if (unlikely(ret)) {
				pr_err("fail to config isp path pre\n");
				break;
			}
		}

		/* config isp vid path*/
		if (path_vid->is_work) {
			ret = sprd_camioctl_isp_path_block_cfg(path_vid,
				&dev->isp_dev_handle, ISP_PATH_IDX_VID);
			if (unlikely(ret)) {
				pr_err("fail to config path_vid\n");
				break;
			}
			path_vid->status = PATH_RUN;
		} else {
			ret = sprd_isp_drv_path_cfg_set(dev->isp_dev_handle,
				ISP_PATH_IDX_VID, ISP_PATH_ENABLE,
				&path_not_work);
			if (unlikely(ret)) {
				pr_err("fail to config isp path vid\n");
				break;
			}
		}

		/* config isp cap path*/
		if (path_cap->is_work) {
			ret = sprd_camioctl_isp_path_block_cfg(path_cap,
				&dev->isp_dev_handle, ISP_PATH_IDX_CAP);
			if (unlikely(ret)) {
				pr_err("fail to config path_cap\n");
				break;
			}
			path_cap->status = PATH_RUN;
		} else {
			ret = sprd_isp_drv_path_cfg_set(dev->isp_dev_handle,
				ISP_PATH_IDX_CAP, ISP_PATH_ENABLE,
				&path_not_work);
			if (unlikely(ret)) {
				pr_err("fail to config isp path cap\n");
				break;
			}
		}

		ret = sprd_isp_drv_path_cfg_set(dev->isp_dev_handle,
			ISP_PATH_IDX_PRE, ISP_DUAL_CAM_EN,
			&ctx->dual_cam);
		if (unlikely(ret)) {
			pr_err("fail to config isp path pre\n");
			break;
		}

		if (dev->init_inptr.statis_valid) {
			ret = sprd_cam_statistic_buf_cfg(
				&isp_dev->module_info.statis_module_info,
				&dev->init_inptr);
			if (ret != 0) {
				pr_err("fail to cfg statis!\n");
				break;
			}
		}
	} while (0);

exit:
	return ret;
}

static int sprd_camioctl_dcam_fetch_start(enum dcam_id idx,
	enum dcam_id fetch_idx, struct camera_group *group)
{
	int ret = 0;
	uint32_t i = 0;
	uint32_t param = 0;
	size_t image_size;
	struct camera_dev *dev = group->dev[idx];
	struct camera_rect rect;
	struct camera_addr cam_addr = {0};
	struct camera_path_spec *path;
	struct camera_path_spec *isp_path;
	struct isp_img_size size;
	uint32_t tmp = 0;
	uint32_t addr = 0;
	struct dcam_module *dcam_module = NULL;
	struct dcam_path_desc *full_path = sprd_dcam_drv_full_path_get(idx);

	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		ret = -EINVAL;
		return ret;
	}

	pr_info("dcam%d => dcam%d\n", idx, fetch_idx);
	size = dev->fetch_info.size;
	path = &group->dev[fetch_idx]->cam_ctx.cam_path[CAMERA_BIN_PATH];
	isp_path = &dev->cam_ctx.cam_path[CAMERA_CAP_PATH];
	if (!dev->raw_cap)
		goto config_fetch;

	/*enable dcam 1*/
	if (idx != fetch_idx ||
		!(group->mode_inited & (1 << fetch_idx))) {

		ret = sprd_dcam_drv_module_en(fetch_idx);
		if (unlikely(ret != 0)) {
			pr_err("fail to enable dcam module %d\n", fetch_idx);
			goto exit;
		}
		pr_info("dcam%d has been enabled!\n", fetch_idx);
	}
	ret = sprd_dcam_int_reg_isr(fetch_idx, DCAM_BIN_PATH_TX_DONE,
		sprd_camioctl_bin_tx_done, dev);
	if (unlikely(ret)) {
		pr_err("fail to register dcam isr\n");
		goto exit;
	}
	group->fetch_inited |= 1 << fetch_idx;

	image_size = sprd_cam_com_raw_pitch_calc(0, size.width) * size.height;
	memset((void *)&cam_addr, 0, sizeof(cam_addr));
	sprd_cam_buf_alloc(&cam_addr.buf_info, idx, &s_dcam_pdev->dev,
		image_size, 1, CAM_BUF_SWAP_TYPE);
	sprd_cam_queue_buf_write(&path->buf_queue,
		&cam_addr);
	sprd_dcam_bin_path_clear(fetch_idx);

config_fetch:
	/*config fetch*/
	path->is_work = 1;
	path->status = PATH_RUN;
	path->assoc_idx = 1 << CAMERA_CAP_PATH;
	path->in_size.w = size.width;
	path->in_size.h = size.height;
	path->in_rect.x = 0;
	path->in_rect.y = 0;
	path->in_rect.w = size.width;
	path->in_rect.h = size.height;
	path->out_size = path->in_size;
	path->out_fmt = DCAM_RAWRGB;
	path->pixel_depth = 10;
	path->buf_num = 1;
	isp_path->assoc_idx = 1 << CAMERA_BIN_PATH;
	ret = sprd_isp_drv_path_cfg_set(dev->isp_dev_handle,
		ISP_PATH_IDX_CAP, ISP_PATH_ASSOC, &isp_path->assoc_idx);
	if (unlikely(ret)) {
		pr_err("fail to config isp path\n");
		goto exit;
	}

	ret = sprd_camioctl_bin_path_cfg(path, fetch_idx);
	if (unlikely(ret)) {
		pr_err("fail to config bin path\n");
		goto exit;
	}

	rect.x = 0;
	rect.y = 0;
	rect.w = size.width;
	rect.h = size.height;

	/*full_path->src_sel == DCAM_PATH_FROM_CAP*/
	DCAM_REG_MWR(fetch_idx, DCAM_MIPI_CAP_CFG,
		BIT_16 | BIT_17, dev->cam_ctx.img_ptn << 16);
	DCAM_REG_WR(fetch_idx, DCAM_MIPI_CAP_START, 0);
	tmp = (size.width - 1);
	tmp |= (size.height - 1) << 16;
	DCAM_REG_WR(fetch_idx, DCAM_MIPI_CAP_END, tmp);

	sprd_dcam_bin_path_start(fetch_idx);
	if (dev->raw_cap) {
		dcam_module = sprd_dcam_drv_module_get(fetch_idx);
		sprd_cam_statistic_queue_clear(
			&dcam_module->statis_module_info);
		sprd_cam_statistic_buf_cfg(
			&dcam_module->statis_module_info, &dev->init_inptr);
		sprd_cam_statistic_buf_set(
			&dcam_module->statis_module_info);
	} else {
		DCAM_REG_MWR(fetch_idx, ISP_AEM_PARAM, BIT_0, 1);
		DCAM_REG_MWR(fetch_idx, ISP_RAW_AFM_FRAM_CTRL, BIT_0, 1);
		DCAM_REG_MWR(fetch_idx, ISP_ANTI_FLICKER_FRAM_CTRL, BIT_0, 1);
		DCAM_REG_MWR(fetch_idx, DCAM_NR3_PARA1, BIT_0, 1);
	}
	sprd_dcam_drv_irq_mask_en(fetch_idx);

	sprd_dcam_drv_force_copy(fetch_idx, ALL_COPY);

	/*config fetch*/
	param = 0;
	ret = sprd_dcam_fetch_cfg_set(fetch_idx,
		DCAM_FETCH_DATA_PACKET, &param);
	if (unlikely(ret)) {
		pr_err("fail to enable full_path\n");
		goto exit;
	}

	ret = sprd_dcam_fetch_cfg_set(fetch_idx,
		DCAM_FETCH_INPUT_RECT, &rect);
	if (unlikely(ret)) {
		pr_err("fail to cfg full_path input rect\n");
		goto exit;
	}

	param = dev->fetch_info.dcam_fetch_endian;
	ret = sprd_dcam_fetch_cfg_set(fetch_idx,
		DCAM_FETCH_DATA_ENDIAN, &param);
	if (unlikely(ret)) {
		pr_err("fail to cfg full_path data endian\n");
		goto exit;
	}

	memset((void *)&cam_addr, 0, sizeof(cam_addr));
	cam_addr.buf_info.dev = &s_dcam_pdev->dev;
	cam_addr.buf_info.type = CAM_BUF_USER_TYPE;
	cam_addr.buf_info.num = 1;
	cam_addr.mfd_y = dev->fetch_info.fetch_addr.img_fd;
	cam_addr.yaddr = dev->fetch_info.fetch_addr.offset.x;
	if (!dev->raw_cap) {
		for (i = 0; i < full_path->buf_num; i++) {
			if (full_path->addr_4in1[i].mfd == cam_addr.mfd_y) {
				addr = full_path->addr_4in1[i].iova;
				break;
			}
		}
		if (addr == 0) {
			pr_err("fail to find mfd %d\n", cam_addr.mfd_y);
			return -EFAULT;
		}
		addr += cam_addr.yaddr;
		DCAM_AXIM_WR(REG_DCAM_IMG_FETCH_RADDR, addr);
	} else {
		ret = sprd_dcam_fetch_cfg_set(fetch_idx,
			DCAM_FETCH_INPUT_ADDR, &cam_addr);
		if (unlikely(ret)) {
			pr_err("fail to cfg dcam fetch source addr\n");
			goto exit;
		}
	}

	/*start dcam1 and isp fetch flow*/
	param = 1;
	ret = sprd_dcam_fetch_cfg_set(fetch_idx,
		DCAM_FETCH_START, &param);

	pr_debug("size mfd offset: 0x%x 0x%x, %dx%d, fetch id %d\n",
		dev->fetch_info.fetch_addr.img_fd,
		dev->fetch_info.fetch_addr.offset.x,
		size.width, size.height, fetch_idx);

exit:
	return ret;
}

static int sprd_camioctl_dcam_fetch_stop(enum dcam_id idx,
	enum dcam_id fetch_idx, struct camera_group *group)
{
	int ret = 0;
	uint32_t param = 0;
	struct camera_dev *dev = group->dev[idx];
	struct camera_path_spec *path;
	struct dcam_module *dcam_module = NULL;

	if (!dev) {
		pr_err("fail to get valid input ptr\n");
		ret = -EINVAL;
		return ret;
	}

	ret = sprd_dcam_fetch_cfg_set(fetch_idx,
		DCAM_FETCH_START, &param);

	path = &group->dev[fetch_idx]->cam_ctx.cam_path[CAMERA_BIN_PATH];

	dcam_module = sprd_dcam_drv_module_get(fetch_idx);
	if (dev->raw_cap) {
		sprd_cam_buf_addr_unmap(
			&dcam_module->dcam_fetch.frame.buf_info);
		sprd_cam_statistic_unmap(
			&dcam_module->statis_module_info.img_statis_buf);
	}

	sprd_dcam_bin_path_unmap(fetch_idx);
	path->is_work = 0;
	path->status = PATH_IDLE;
	path->assoc_idx = 0;

	sprd_dcam_drv_irq_mask_dis(fetch_idx);
	if (idx != fetch_idx ||
		!(group->mode_inited & group->fetch_inited)) {
		ret = sprd_dcam_drv_module_dis(fetch_idx);
		if (unlikely(ret != 0)) {
			pr_err("fail to disable dcam%d module\n", idx);
			ret = -EFAULT;
		}
	}
	ret = sprd_dcam_int_reg_isr(fetch_idx, DCAM_BIN_PATH_TX_DONE,
		NULL, dev);
	if (unlikely(ret)) {
		pr_err("fail to register dcam isr\n");
		return ret;
	}
	group->fetch_inited &= ~(1 << fetch_idx);
	pr_info("dcam%d => dcam%d fetch stop end!\n", idx, fetch_idx);

	return ret;
}

static int sprd_camioctl_dcam_out_size(struct camera_dev *dev,
	enum dcam_id idx, unsigned long arg)
{
	int ret = 0;
	struct sprd_dcam_path_size parm;

	CAM_TRACE("%d: dcam out size\n", idx);
	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&parm, (void __user *)arg,
		sizeof(struct sprd_dcam_path_size));
	if (ret) {
		pr_err("fail to get user info ret %d\n", ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}

	if ((!parm.dcam_in_w) || (!parm.dcam_in_h)) {
		pr_err("fail to get dcam in size %d %d\n",
			parm.dcam_in_w, parm.dcam_in_h);
		mutex_unlock(&dev->cam_mutex);
		return -EFAULT;
	}

	parm.dcam_out_w = parm.dcam_in_w;
	parm.dcam_out_h = parm.dcam_in_h;

	ret = copy_to_user((void __user *)arg, &parm,
		sizeof(struct sprd_dcam_path_size));
	if (ret) {
		pr_err("fail to copy to user, ret = %d\n", ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}

	mutex_unlock(&dev->cam_mutex);

	return ret;
}

static int sprd_camioctl_io_cap_mode_set(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	uint32_t mode = 0;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_context *ctx = NULL;

	idx = camerafile->idx;
	ctx = &dev->cam_ctx;

	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&mode, (void __user *) arg,
			sizeof(uint32_t));
	if (ret) {
		pr_err("fail to get user info\n");
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}
	ctx->capture_mode = mode;
	mutex_unlock(&dev->cam_mutex);
	CAM_TRACE("%d: capture mode %d\n", idx,
		dev->cam_ctx.capture_mode);

	return ret;
}

static int sprd_camioctl_io_cap_skip_num_set(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	uint32_t skip_num = 0;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_context *ctx = NULL;

	idx = camerafile->idx;
	ctx = &dev->cam_ctx;

	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&skip_num, (void __user *)arg,
			sizeof(uint32_t));
	if (ret) {
		pr_err("fail to get user info\n");
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}
	ctx->skip_number = skip_num;
	mutex_unlock(&dev->cam_mutex);
	CAM_TRACE("%d: cap skip number %d\n", idx,
		dev->cam_ctx.skip_number);

	return ret;
}

static int sprd_camioctl_io_sensor_size_set(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_size size;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_context *ctx = NULL;

	idx = camerafile->idx;
	ctx = &dev->cam_ctx;

	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&size, (void __user *)arg,
				sizeof(struct sprd_img_size));
	if (ret) {
		pr_err("fail to get user info %d\n", ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}
	ctx->cap_in_size.w = size.w;
	ctx->cap_in_size.h = size.h;
	mutex_unlock(&dev->cam_mutex);
	CAM_TRACE("%d: sensor size %d %d\n", idx,
			ctx->cap_in_size.w, ctx->cap_in_size.h);

	return ret;
}

static int sprd_camioctl_io_sensor_trim_set(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_rect rect;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_context *ctx = NULL;

	idx = camerafile->idx;
	ctx = &dev->cam_ctx;

	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&rect, (void __user *)arg,
				sizeof(struct sprd_img_rect));
	if (ret) {
		pr_err("fail to get user info %d\n", ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}
	ctx->cap_in_rect.x = rect.x;
	ctx->cap_in_rect.y = rect.y;
	ctx->cap_in_rect.w = rect.w;
	ctx->cap_in_rect.h = rect.h;
	ctx->cap_out_size.w = ctx->cap_in_rect.w;
	ctx->cap_out_size.h = ctx->cap_in_rect.h;

	mutex_unlock(&dev->cam_mutex);
	CAM_TRACE("%d: sensor trim x y w h %d %d %d %d\n", idx,
			ctx->cap_in_rect.x,
			ctx->cap_in_rect.y,
			ctx->cap_in_rect.w,
			ctx->cap_in_rect.h);

	return ret;
}

static int sprd_camioctl_io_frm_id_base_set(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_parm parm;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_context *ctx = NULL;

	idx = camerafile->idx;
	ctx = &dev->cam_ctx;

	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&parm, (void __user *)arg,
				sizeof(struct sprd_img_parm));
	if (ret) {
		pr_err("fail to get user info ret %d\n", ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}

	switch (parm.channel_id) {
	case CAMERA_FULL_PATH:
	case CAMERA_BIN_PATH:
	case CAMERA_PRE_PATH:
	case CAMERA_VID_PATH:
	case CAMERA_CAP_PATH:
	case CAMERA_PDAF_PATH:
		ctx->cam_path[parm.channel_id].frm_id_base =
			parm.frame_base_id;
		break;
	default:
		pr_err("fail to get valid channel ID, %d\n",
			parm.channel_id);
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}
	mutex_unlock(&dev->cam_mutex);
	CAM_TRACE("%d: channel %d, base id 0x%x\n",
			idx, parm.channel_id, parm.frame_base_id);

	return ret;
}

static int sprd_camioctl_io_crop_set(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_parm parm;

	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&parm, (void __user *)arg,
				sizeof(struct sprd_img_parm));
	if (ret) {
		pr_err("fail to get user info ret %d\n", ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}

	ret = sprd_camioctl_crop_set(dev, &parm);
	mutex_unlock(&dev->cam_mutex);

	return ret;
}

static int sprd_camioctl_io_flash_set(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	uint32_t led0_ctrl = 0, led1_ctrl = 0;
	uint32_t led0_status = 0, led1_status = 0;
	struct flash_led_task *flash_task = NULL;

	flash_task = dev->flash_task;
	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&flash_task->set_flash,
			(void __user *)arg,
			sizeof(struct sprd_img_set_flash));
	if (ret) {
		pr_err("fail to get user info\n");
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}

	led0_ctrl = flash_task->set_flash.led0_ctrl;
	led1_ctrl = flash_task->set_flash.led1_ctrl;
	led0_status = flash_task->set_flash.led0_status;
	led1_status = flash_task->set_flash.led1_status;

	mutex_unlock(&dev->cam_mutex);

#if 0   /* should defer to EoF */
	if ((led0_ctrl &&
		(led0_status == FLASH_CLOSE_AFTER_OPEN ||
		led0_status == FLASH_CLOSE ||
		led0_status == FLASH_CLOSE_AFTER_AUTOFOCUS)) ||
		(led1_ctrl &&
		(led1_status == FLASH_CLOSE_AFTER_OPEN ||
		led1_status == FLASH_CLOSE ||
		led1_status == FLASH_CLOSE_AFTER_AUTOFOCUS))) {
		complete(&flash_task->flash_thread_com);
	}
#endif

	CAM_TRACE("led0_ctrl %d led0_status %d\n",
		flash_task->set_flash.led0_ctrl,
		flash_task->set_flash.led0_status);
	CAM_TRACE("led1_ctrl %d led1_status %d\n",
		flash_task->set_flash.led1_ctrl,
		flash_task->set_flash.led1_status);

	return ret;
}

static int sprd_camioctl_io_output_size_set(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_parm parm;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_context *ctx = NULL;
	struct dcam_module *module = NULL;

	idx = camerafile->idx;
	ctx = &dev->cam_ctx;

	CAM_TRACE("%d: set output size\n", idx);
	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&parm, (void __user *)arg,
			sizeof(struct sprd_img_parm));
	if (ret) {
		pr_err("fail to get user info ret %d\n", ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}

	ctx->dst_size.w = parm.dst_size.w;
	ctx->dst_size.h = parm.dst_size.h;
	ctx->pxl_fmt = parm.pixel_fmt;
	ctx->sn_fmt = parm.sn_fmt;
	ctx->need_isp_tool = parm.need_isp_tool;
	ctx->need_isp = parm.need_isp;
	ctx->path_input_rect.x = parm.crop_rect.x;
	ctx->path_input_rect.y = parm.crop_rect.y;
	ctx->path_input_rect.w = parm.crop_rect.w;
	ctx->path_input_rect.h = parm.crop_rect.h;
	ctx->scene_mode = parm.scene_mode;
	ctx->is_high_fps = parm.is_high_fps;
	ctx->high_fps_skip_num = parm.high_fps_skip_num;

	if (parm.slowmotion)
		ctx->is_slow_motion = parm.slowmotion;

	module = sprd_dcam_drv_module_get(idx);
	module->is_high_fps = ctx->is_high_fps;
	module->high_fps_skip_num = ctx->high_fps_skip_num;

	mutex_unlock(&dev->cam_mutex);

	return ret;
}

static int sprd_camioctl_io_sensor_if_set(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_sensor_if sensor;
	enum dcam_id idx = DCAM_ID_0;

	idx = camerafile->idx;

	CAM_TRACE("%d: set sensor if\n", idx);
	ret = copy_from_user(&sensor,
			(void __user *)arg,
			sizeof(struct sprd_img_sensor_if));
	if (unlikely(ret)) {
		pr_err("fail to copy form user%d\n", ret);
		ret = -EFAULT;
		return ret;
	}
	mutex_lock(&dev->cam_mutex);
	ret = sprd_camioctl_sensor_if_set(dev, &sensor);
	mutex_unlock(&dev->cam_mutex);

	return ret;
}

static int sprd_camioctl_io_frame_addr_set(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_parm parm;
	enum dcam_id idx = DCAM_ID_0;

	idx = camerafile->idx;

	CAM_TRACE("%d: set frame addr\n", idx);
	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&parm, (void __user *)arg,
				sizeof(struct sprd_img_parm));
	if (ret) {
		pr_err("fail to get user info ret %d\n", ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}

	ret = sprd_camioctl_frame_addr_set(camerafile, dev, &parm);
	mutex_unlock(&dev->cam_mutex);

	return ret;
}

static int sprd_camioctl_io_path_frm_deci(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_parm parm;
	struct camera_path_spec *path = NULL;
	struct camera_context *ctx = NULL;

	ctx = &dev->cam_ctx;

	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&parm, (void __user *)arg,
				sizeof(struct sprd_img_parm));
	if (ret) {
		pr_err("fail to get user info ret %d\n", ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}
	path = &ctx->cam_path[parm.channel_id];
	path->path_frm_deci = parm.deci;
	mutex_unlock(&dev->cam_mutex);

	return ret;
}

static int sprd_camioctl_io_stream_on(struct camera_file *camerafile,
	struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct camera_group *group = NULL;
	enum dcam_id idx = DCAM_ID_0;
	struct flash_led_task *flash_task = NULL;

	group = camerafile->grp;
	idx = camerafile->idx;
	flash_task = dev->flash_task;

	mutex_lock(&dev->cam_mutex);

	if (dev->init_inptr.statis_valid) {
		group->cam_ion_client[idx] =
			sprd_ion_client_get(dev->init_inptr.dev_fd);
		if (!group->cam_ion_client[idx])
			pr_err("fail to get ion client fd 0x%lx\n",
				dev->init_inptr.dev_fd);
	}

	ret = sprd_cam_queue_buf_clear(&dev->queue);
	if (unlikely(ret != 0)) {
		mutex_unlock(&dev->cam_mutex);
		pr_err("fail to clear queue\n");
		goto exit;
	}
	ret = sprd_camioctl_camera_pipeline_cfg(camerafile);
	if (unlikely(ret)) {
		pr_err("fail to config isp path mode\n");
		mutex_unlock(&dev->cam_mutex);
		goto exit;
	}

	ret = sprd_camioctl_dcam_cfg(dev);
	if (unlikely(ret)) {
		mutex_unlock(&dev->cam_mutex);
		pr_err("fail to config dcam param\n");
		goto exit;
	}

	if (dev->isp_work) {
		ret = sprd_camioctl_isp_path_cfg(dev);
		if (unlikely(ret)) {
			mutex_unlock(&dev->cam_mutex);
			pr_err("fail to config isp path\n");
			goto exit;
		}
		ret = sprd_isp_drv_start(dev->isp_dev_handle);
		if (unlikely(ret)) {
			mutex_unlock(&dev->cam_mutex);
			pr_err("fail to start isp path\n");
			goto exit;
		}
		atomic_inc(&group->isp_run_count);
	}

	flash_task->frame_skipped = 0;
	flash_task->skip_number = dev->cam_ctx.skip_number;
	if ((flash_task->set_flash.led0_ctrl &&
		flash_task->set_flash.led0_status == FLASH_HIGH_LIGHT) ||
		(flash_task->set_flash.led1_ctrl &&
		flash_task->set_flash.led1_status == FLASH_HIGH_LIGHT)) {
		if (dev->cam_ctx.skip_number == 0)
			sprd_cam_flash_start(NULL, flash_task);
	}

	ret = sprd_dcam_drv_start(idx);
	if (unlikely(ret)) {
		pr_err("fail to start dcam\n");
		ret = sprd_camioctl_isp_unreg_isr(dev);
		mutex_unlock(&dev->cam_mutex);
		goto exit;
	} else {
		atomic_set(&dev->run_flag, 0);
		sprd_camioctl_timer_start(&dev->cam_timer, DCAM_TIMEOUT);
	}

	atomic_set(&dev->stream_on, 1);
	atomic_inc(&group->dcam_run_count);
	mutex_unlock(&dev->cam_mutex);
	pr_info("Camera stream on success! dev %p group %p\n", dev, group);

exit:
	return ret;
}

static int sprd_camioctl_io_stream_off(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_group *group = NULL;

	group = camerafile->grp;
	idx = dev->idx;

	mutex_lock(&dev->cam_mutex);

	if (unlikely(atomic_read(&dev->stream_on) == 0)) {
		sprd_camioctl_cam_path_clear(dev);
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}
	do {
		ret = sprd_camioctl_timer_stop(&dev->cam_timer);
		if (unlikely(ret)) {
			pr_err("fail to stop timer\n");
			break;
		}

		if (dev->cam_ctx.need_4in1 &&
			dev->cam_ctx.need_isp_tool != 1) {
			ret = sprd_camioctl_dcam_fetch_stop(
				dev->idx, DCAM_ID_1, group);
			if (unlikely(ret != 0)) {
				pr_err("fail to stop dcam fetch\n");
				ret = -EFAULT;
			}
			group->fetch_inited &= ~(1 << DCAM_ID_1);
		}

		ret = sprd_dcam_drv_stop(idx, 0);
		if (unlikely(ret)) {
			pr_err("fail to stop dcam\n");
			break;
		}
		ret = sprd_camioctl_dcam_unreg_isr(dev);
		if (unlikely(ret)) {
			pr_err("fail to unregister isr\n");
			break;
		}

		if (dev->isp_work) {
			ret = sprd_isp_drv_stop(dev->isp_dev_handle, 0);
			if (unlikely(ret)) {
				pr_err("fail to stop isp\n");
				break;
			}

			ret = sprd_camioctl_isp_unreg_isr(dev);
			if (unlikely(ret)) {
				pr_err("fail to unregister isp isr\n");
				break;
			}
			sprd_isp_drv_path_clear(dev->isp_dev_handle);
		}
		ret = sprd_dcam_drv_path_unmap(idx);
		if (unlikely(ret)) {
			pr_err("fail to unmap dcam drv path\n");
			break;
		}
		sprd_dcam_drv_path_clear(idx);

		sprd_camioctl_cam_path_clear(dev);

		atomic_set(&dev->stream_on, 0);
		sprd_cam_buf_sg_table_put(idx);
		if (dev->cam_ctx.need_4in1 &&
			dev->cam_ctx.need_isp_tool != 1)
			sprd_cam_buf_sg_table_put(DCAM_ID_1);

		if (dev->init_inptr.statis_valid) {
			if (group->cam_ion_client[idx]) {
				sprd_ion_client_put(
					group->cam_ion_client[idx]);
				group->cam_ion_client[idx] = NULL;
			}
		}

		if (atomic_dec_return(&group->dcam_run_count) == 0)
			sprd_iommu_restore(&s_dcam_pdev->dev);
		if (dev->isp_work && atomic_dec_return(
			&group->isp_run_count) == 0)
			sprd_iommu_restore(&s_isp_pdev->dev);
	} while (0);

	dev->cap_flag = DCAM_CAPTURE_STOP;

	mutex_unlock(&dev->cam_mutex);
	pr_info("Camera stream off success!\n");

	return ret;
}

static int sprd_camioctl_io_fmt_get(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct camera_format *fmt = NULL;
	struct sprd_img_get_fmt fmt_desc;

	CAM_TRACE("get fmt\n");
	ret = copy_from_user(&fmt_desc,
				(void __user *)arg,
				sizeof(struct sprd_img_get_fmt));
	if (ret) {
		ret = -EFAULT;
		pr_err("fail to copy from user\n");
		return ret;
	}
	if (unlikely(fmt_desc.index >= ARRAY_SIZE(dcam_img_fmt)))
		return -EINVAL;

	fmt = &dcam_img_fmt[fmt_desc.index];
	fmt_desc.fmt = fmt->fourcc;

	ret = copy_to_user((void __user *)arg,
			&fmt_desc,
			sizeof(struct sprd_img_get_fmt));
	if (ret) {
		ret = -EFAULT;
		pr_err("fail to copy to user\n");
		return ret;
	}

	return ret;
}

static int sprd_camioctl_io_ch_id_get(struct camera_file *camerafile,
		struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	uint32_t channel_id = 0;
	struct camera_context *ctx = NULL;

	ctx = &dev->cam_ctx;

	CAM_TRACE("get free channel\n");
	sprd_camioctl_free_channel_get(dev, &channel_id, ctx->scene_mode);
	ret = copy_to_user((void __user *)arg, &channel_id,
			sizeof(uint32_t));
	if (ret) {
		pr_err("fail to copy to user, ret = %d\n", ret);
		ret = -EFAULT;
		return ret;
	}

	return ret;
}

static int sprd_camioctl_io_time_get(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_time utime;
	struct timeval time;

	CAM_TRACE("get time\n");
	sprd_cam_com_timestamp(&time);
	utime.sec = time.tv_sec;
	utime.usec = time.tv_usec;
	ret = copy_to_user((void __user *)arg, &utime,
				sizeof(struct sprd_img_time));
	if (ret) {
		pr_err("fail to get time info %d\n", ret);
		ret = -EFAULT;
	}

	return ret;
}

static int sprd_camioctl_io_fmt_check(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	uint32_t channel_id = 0;
	struct camera_format *fmt;
	struct sprd_img_format img_format;

	CAM_TRACE("check fmt\n");
	ret = copy_from_user(&img_format,
				(void __user *)arg,
				sizeof(struct sprd_img_format));
	if (ret) {
		ret = -EFAULT;
		pr_err("fail to get img_format\n");
		goto exit;
	}

	if (!dev) {
		ret = -EFAULT;
		pr_err("fail to get valid camerafile:NULL\n");
		goto exit;
	}

	dev->use_path = img_format.buffer_cfg_isp;

	fmt = sprd_camioctl_format_get(img_format.fourcc);
	if (unlikely(!fmt)) {
		ret = -EFAULT;
		pr_err("fail to get valid fourcc format (0x%08x)\n",
			img_format.fourcc);
		goto exit;
	}

	if (img_format.channel_id == CAMERA_FULL_PATH) {
		mutex_lock(&dev->cam_mutex);
		ret = sprd_camioctl_full_path_cap_check(fmt->fourcc,
					&img_format,
					&dev->cam_ctx);
		mutex_unlock(&dev->cam_mutex);
		channel_id = CAMERA_FULL_PATH;
	} else if (img_format.channel_id == CAMERA_BIN_PATH) {
		mutex_lock(&dev->cam_mutex);
		ret = sprd_camioctl_bin_path_cap_check(fmt->fourcc,
					&img_format,
					&dev->cam_ctx);
		mutex_unlock(&dev->cam_mutex);
		channel_id = CAMERA_BIN_PATH;
	} else if (img_format.channel_id == CAMERA_PDAF_PATH) {
		mutex_lock(&dev->cam_mutex);
		ret = sprd_camioctl_path_pdaf_cap_check(fmt->fourcc,
					&img_format,
					&dev->cam_ctx);
		mutex_unlock(&dev->cam_mutex);
		channel_id = CAMERA_PDAF_PATH;
	} else {
		mutex_lock(&dev->cam_mutex);
		ret = sprd_camioctl_path_cap_check(fmt->fourcc, &img_format,
			&dev->cam_ctx, img_format.channel_id);
		mutex_unlock(&dev->cam_mutex);
		channel_id = img_format.channel_id;
	}

	if (channel_id < CAMERA_PDAF_PATH) {
		img_format.endian.y_endian =
			dev->cam_ctx.cam_path[channel_id].end_sel.y_endian;
		img_format.endian.uv_endian =
			dev->cam_ctx.cam_path[channel_id].end_sel.uv_endian;
	}

	if (ret == 0) {
		if (atomic_read(&dev->stream_on) != 0) {
			if (channel_id == CAMERA_PRE_PATH
				|| channel_id == CAMERA_VID_PATH
				|| channel_id == CAMERA_CAP_PATH) {
				ret = sprd_camioctl_video_update(
					dev, channel_id);
			}
		}
	}

	ret = copy_to_user((void __user *)arg,
			&img_format,
			sizeof(struct sprd_img_format));
	if (ret) {
		ret = -EFAULT;
		pr_err("fail to copy to user\n");
		goto exit;
	}

exit:
	return ret;
}

static int sprd_camioctl_io_shrink_set(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_parm parm;
	struct camera_path_spec *path = NULL;
	struct camera_context *ctx = NULL;

	ctx = &dev->cam_ctx;

	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&parm, (void __user *)arg,
				sizeof(struct sprd_img_parm));
	if (ret) {
		pr_err("fail to get user info ret %d\n", ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}
	path = &ctx->cam_path[parm.channel_id];
	path->regular_desc = parm.regular_desc;
	mutex_unlock(&dev->cam_mutex);
	CAM_TRACE("channel %d, regular mode %d\n",
		parm.channel_id, path->regular_desc.regular_mode);

	return ret;
}

static int sprd_camioctl_io_flash_cfg(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_flash_cfg_param cfg_parm;

	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&cfg_parm,
				(void __user *)arg,
				sizeof(struct sprd_flash_cfg_param));
	if (ret) {
		pr_err("fail to copy from user %d\n", ret);
		mutex_unlock(&dev->cam_mutex);
		ret = -EFAULT;
		return ret;
	}
	ret = sprd_flash_cfg(&cfg_parm);
	mutex_unlock(&dev->cam_mutex);
	CAM_TRACE("config flash, ret %d\n", ret);

	return ret;
}

static int sprd_camioctl_io_pdaf_control(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_parm parm;
	struct camera_path_spec *path = NULL;
	struct camera_context *ctx = NULL;

	ctx = &dev->cam_ctx;

	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&parm, (void __user *)arg,
				sizeof(struct sprd_img_parm));
	if (ret) {
		pr_err("fail to get user info ret %d\n", ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}
	path = &ctx->cam_path[parm.channel_id];
	path->pdaf_ctrl.mode = parm.pdaf_ctrl.mode;
	path->pdaf_ctrl.phase_data_type =
		parm.pdaf_ctrl.phase_data_type;
	mutex_unlock(&dev->cam_mutex);
	CAM_TRACE("channel %d, pdaf mode %d type %d\n",
		parm.channel_id, path->pdaf_ctrl.mode,
		path->pdaf_ctrl.phase_data_type);

	return ret;
}

static int sprd_camioctl_io_ebd_control(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_parm parm;
	struct camera_path_spec *path = NULL;
	struct camera_context *ctx = NULL;

	ctx = &dev->cam_ctx;

	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&parm, (void __user *)arg,
				sizeof(struct sprd_img_parm));
	if (ret) {
		pr_err("fail to get user info ret %d\n", ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}
	path = &ctx->cam_path[parm.channel_id];
	path->ebd_ctrl.mode = parm.ebd_ctrl.mode;
	path->ebd_ctrl.image_vc = parm.ebd_ctrl.image_vc;
	path->ebd_ctrl.image_dt = parm.ebd_ctrl.image_dt;
	mutex_unlock(&dev->cam_mutex);

	return ret;
}

static int sprd_camioctl_io_iommu_status_get(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	uint32_t iommu_enable = 0;
	struct camera_group *group = NULL;

	group = camerafile->grp;

	ret = copy_from_user(&iommu_enable, (void __user *)arg,
			sizeof(unsigned char));
	if (ret) {
		pr_err("fail to copy from user\n");
		ret = -EFAULT;
		return ret;
	}

	if (sprd_iommu_attach_device(&group->pdev->dev) == 0)
		iommu_enable = 1;
	else
		iommu_enable = 0;

	ret = copy_to_user((void __user *)arg, &iommu_enable,
			sizeof(unsigned char));
	if (ret) {
		pr_err("fail to copy to user, ret = %d\n", ret);
		ret = -EFAULT;
		return ret;
	}

	return ret;
}

static int sprd_camioctl_io_capture_start(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	enum dcam_id idx = DCAM_ID_0;
	struct sprd_img_capture_param capture_param;

	idx = camerafile->idx;

	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&capture_param, (void __user *)arg,
			sizeof(struct sprd_img_capture_param));
	if (ret) {
		pr_err("fail to get user info\n");
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		goto exit;
	}
	if (unlikely(atomic_read(&dev->stream_on) == 0)) {
		mutex_unlock(&dev->cam_mutex);
		goto exit;
	}

	pr_info("cam%d capture start: cap_flag = %d\n", idx,
		dev->cap_flag);

	if (dev->cam_ctx.need_4in1 && dev->cam_ctx.need_isp_tool != 1) {
		ret = sprd_isp_path_4in1_cap_start(dev->isp_dev_handle,
			sprd_camioctl_tx_done, dev, capture_param.type);
		if (ret)
			pr_err("fail to cap 4in1 raw data\n");
		dev->cap_flag = DCAM_CAPTURE_START;
		mutex_unlock(&dev->cam_mutex);
		goto exit;
	}

	if (dev->cap_flag == DCAM_CAPTURE_STOP &&
			dev->cam_ctx.need_isp_tool != 1) {
		ret = sprd_isp_drv_cap_start(dev->isp_dev_handle,
			capture_param, 0);
		if (ret) {
			mutex_unlock(&dev->cam_mutex);
			pr_err("fail to start offline\n");
			goto exit;
		}
		dev->cap_flag = DCAM_CAPTURE_START;
	}
	mutex_unlock(&dev->cam_mutex);

exit:
	return ret;
}

static int sprd_camioctl_io_capture_stop(struct camera_file *camerafile,
				struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	enum dcam_id idx = DCAM_ID_0;

	idx = camerafile->idx;

	mutex_lock(&dev->cam_mutex);
	pr_info("cam%d capture stop\n", idx);
	if (dev->cam_ctx.need_4in1) {
		dev->cap_flag = DCAM_CAPTURE_STOP;
		ret = sprd_isp_path_4in1_cap_stop(dev->isp_dev_handle);
		if (ret) {
			pr_err("fail to stop 4in1 raw\n");
			mutex_unlock(&dev->cam_mutex);
			goto exit;
		}
		mutex_unlock(&dev->cam_mutex);
		goto exit;
	}

	if (dev->cap_flag == DCAM_CAPTURE_START) {
		dev->cap_flag = DCAM_CAPTURE_STOP;
		ret = sprd_isp_drv_fmcu_slice_stop(dev->isp_dev_handle);
		if (ret) {
			mutex_unlock(&dev->cam_mutex);
			pr_err("fail to stop offline\n");
			goto exit;
		}
	}
	mutex_unlock(&dev->cam_mutex);

exit:
	return ret;
}

static int sprd_camioctl_io_path_skip_num_set(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_parm parm;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_context *ctx = NULL;

	idx = camerafile->idx;
	ctx = &dev->cam_ctx;

	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&parm, (void __user *)arg,
				sizeof(struct sprd_img_parm));
	if (ret) {
		pr_err("fail to copy from user %d\n", ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}

	switch (parm.channel_id) {
	case CAMERA_FULL_PATH:
	case CAMERA_BIN_PATH:
	case CAMERA_PRE_PATH:
	case CAMERA_VID_PATH:
	case CAMERA_CAP_PATH:
	case CAMERA_PDAF_PATH:
		ctx->cam_path[parm.channel_id].skip_num =
			parm.skip_num;
		break;
	default:
		pr_err("fail to get valid channel ID, %d\n",
			parm.channel_id);
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}
	mutex_unlock(&dev->cam_mutex);
	CAM_TRACE("%d: channel %d, skip_num %d\n",
			idx, parm.channel_id, parm.skip_num);

	return ret;
}

static int sprd_camioctl_io_dcam_path_size(struct camera_file *camerafile,
	struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	enum dcam_id idx = DCAM_ID_0;

	idx = camerafile->idx;
	ret = sprd_camioctl_dcam_out_size(dev, idx, arg);
	if (ret) {
		pr_err("fail to get dcam out size\n");
		return ret;
	}

	return ret;
}

static int sprd_camioctl_io_sensor_max_size_set(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_size size;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_context *ctx = NULL;

	idx = camerafile->idx;
	ctx = &dev->cam_ctx;

	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&size, (void __user *)arg,
				sizeof(struct sprd_img_size));
	if (ret || !size.w || !size.h) {
		pr_err("fail to get user info ret %d\n", ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}
	ctx->sn_max_size.w = size.w;
	ctx->sn_max_size.h = size.h;
	mutex_unlock(&dev->cam_mutex);
	CAM_TRACE("%d: sensor max size %d %d\n", idx,
			ctx->sn_max_size.w, ctx->sn_max_size.h);

	return ret;
}

static int sprd_camioctl_io_capability(struct camera_file *camerafile,
		struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;

	mutex_lock(&dev->cam_mutex);
	ret = isp_capability((void *)arg);
	mutex_unlock(&dev->cam_mutex);

	return ret;
}

static int sprd_camioctl_io_rst(struct camera_file *camerafile,
	struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	enum dcam_id idx = DCAM_ID_0;

	idx = camerafile->idx;

	sprd_dcam_full_path_quickstop(idx);
	sprd_dcam_drv_reset(idx, 0);

	return ret;
}

static int sprd_camioctl_io_statis_buf_set(struct camera_file *camerafile,
	struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct isp_statis_buf_input parm_inptr;
	struct cam_statis_module *statis_module = NULL;
	struct dcam_module *dcam_module = NULL;
	struct isp_pipe_dev *isp_dev = NULL;

	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&parm_inptr,
				(void __user *)arg,
				sizeof(struct isp_statis_buf_input));
	if (ret != 0) {
		pr_err("fail to copy_from_user\n");
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}

	if (parm_inptr.buf_flag == STATIS_BUF_FLAG_INIT)
		dev->init_inptr = parm_inptr;
	else {
		if (atomic_read(&dev->stream_on) == 1) {
			isp_dev = (struct isp_pipe_dev *)dev->isp_dev_handle;
			dcam_module = sprd_dcam_drv_module_get(dev->idx);

			if (parm_inptr.buf_property < ISP_DCAM_BLOCK_MAX)
				statis_module = &dcam_module
					->statis_module_info;
			else
				statis_module = &isp_dev->module_info
					.statis_module_info;
			ret = sprd_cam_statistic_addr_set(
				statis_module, &parm_inptr);
		}
	}
	mutex_unlock(&dev->cam_mutex);

	return ret;
}

static int sprd_camioctl_io_cfg_param(struct camera_file *camerafile,
		struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_group *group = NULL;
	struct isp_io_param param;
	struct isp_pipe_dev *isp_dev = NULL;
	struct isp_k_block *isp_k_param = NULL;

	idx = camerafile->idx;
	group = camerafile->grp;

	mutex_lock(&dev->cam_mutex);

	ret = copy_from_user((void *)&param,
		(void __user *)arg, sizeof(param));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n",
			ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		goto exit;
	}
	if (param.sub_block == ISP_BLOCK_FETCH) {
		if (param.property ==
				ISP_PRO_FETCH_RAW_BLOCK) {
			struct isp_dev_fetch_info *fetch_info =
			&dev->fetch_info;

			ret = copy_from_user((void *)fetch_info,
			param.property_param,
			sizeof(struct isp_dev_fetch_info));
			if (ret != 0)
				pr_err("fail to copy user\n");
		} else if (dev->raw_cap == 1
			&& param.property ==
				ISP_PRO_FETCH_START) {
			ret = sprd_camioctl_dcam_fetch_start(idx,
				idx, group);
			mutex_unlock(&dev->cam_mutex);
			goto exit;
		}
	}

	if (!dev->isp_dev_handle) {
		ret = -EFAULT;
		pr_err("fail to get cam%d valid input ptr\n", idx);
		mutex_unlock(&dev->cam_mutex);
		goto exit;
	}
	isp_dev = (struct isp_pipe_dev *)dev->isp_dev_handle;
	isp_k_param = &isp_dev->isp_k_param;
	if (!isp_k_param) {
		ret = -EFAULT;
		pr_err("fail to get isp_private.\n");
		mutex_unlock(&dev->cam_mutex);
		goto exit;
	}
	ret = sprd_isp_cfg_param((void *)arg, isp_k_param, isp_dev);
	mutex_unlock(&dev->cam_mutex);

exit:
	return ret;
}

static int sprd_camioctl_io_raw_cap(struct camera_file *camerafile,
	struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	enum dcam_id idx = DCAM_ID_0;
	struct isp_pipe_dev *isp_dev = NULL;
	struct isp_raw_proc_info raw_cap;
	struct camera_group *group;

	idx = camerafile->idx;
	group = dev->grp;

	mutex_lock(&dev->cam_mutex);
	pr_info("raw_cap, idx: %d, %d\n",
		idx, atomic_read(&dev->stream_on));
	if (unlikely(atomic_read(&dev->stream_on) == 0)) {
		ret = sprd_cam_queue_buf_clear(&dev->queue);
		if (unlikely(ret != 0)) {
			mutex_unlock(&dev->cam_mutex);
			pr_err("fail to init queue\n");
			goto exit;
		}
		ret = sprd_camioctl_dcam_reg_isr(dev);
		if (unlikely(ret)) {
			pr_err("fail to register dcam isr\n");
			mutex_unlock(&dev->cam_mutex);
			goto exit;
		}
		ret = sprd_camioctl_isp_reg_isr(dev);
		if (unlikely(ret)) {
			pr_err("fail to register isp isr\n");
			mutex_unlock(&dev->cam_mutex);
			goto exit;
		}
		atomic_set(&dev->stream_on, 1);
	}
	dev->raw_cap = 1;
	dev->raw_phase = 0;

	if (!dev->isp_dev_handle) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		mutex_unlock(&dev->cam_mutex);
		goto exit;
	}

	isp_dev = (struct isp_pipe_dev *)dev->isp_dev_handle;
	memset((void *)&raw_cap, 0x00, sizeof(raw_cap));
	ret = copy_from_user(&raw_cap,
			(void __user *)arg,
			sizeof(struct isp_raw_proc_info));
	if (ret != 0) {
		pr_err("fail to copy_from_user\n");
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		goto exit;
	}
	wait_for_completion(&group->fetch_com);
	ret = sprd_isp_drv_raw_cap_proc(isp_dev, &raw_cap);
	mutex_unlock(&dev->cam_mutex);

exit:
	return ret;
}

static int sprd_camioctl_io_dcam_res_get(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_res res = {0};
	enum dcam_id idx = DCAM_ID_0;
	struct camera_group *group = NULL;

	group = camerafile->grp;

	ret = copy_from_user(&res, (void __user *)arg,
				sizeof(struct sprd_img_res));
	if (ret) {
		ret = -EFAULT;
		pr_err("fail to copy from user!\n");
		goto exit;
	}
	idx = sprd_sensor_find_dcam_id(res.sensor_id);
	if (idx == -1) {
		ret = -EFAULT;
		pr_err("fail to find attach dcam id!\n");
		goto exit;
	}
	ret = sprd_camioctl_res_get(group, idx, &res);
	if (ret) {
		ret = -EFAULT;
		pr_err("fail to get res!\n");
		goto exit;
	}

	if (group->mode_inited & (1 << (int)idx)) {
		pr_info("cam%d has been enabled!\n", idx);
		/*break;*/
	}
	ret = sprd_dcam_drv_module_en(idx);
	if (unlikely(ret != 0)) {
		pr_err("fail to enable dcam module %d\n", idx);
		goto exit;
	}

	dev = group->dev[idx];
	if (dev->isp_id < ISP_ID_MAX) {
		ret = sprd_isp_drv_module_en(dev->isp_dev_handle);
		if (unlikely(ret != 0)) {
			pr_err("fail to enable isp module %d\n", idx);
			ret = -EFAULT;
			goto exit;
		}
	}
	camerafile->idx = idx;
	group->mode_inited |= 1 << idx;

	ret = copy_to_user((void __user *)arg, &res,
				sizeof(struct sprd_img_res));
	if (ret) {
		ret = -EFAULT;
		pr_err("fail to copy to user\n");
		goto exit;
	}

exit:
	return ret;
}

static int sprd_camioctl_io_dcam_res_put(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_res res = {0};
	enum dcam_id idx = DCAM_ID_0;
	struct camera_group *group = NULL;

	idx = camerafile->idx;
	group = camerafile->grp;

	ret = copy_from_user(&res, (void __user *)arg,
				sizeof(struct sprd_img_res));
	if (ret) {
		ret = -EFAULT;
		pr_err("fail to copy_from_user!\n");
		goto exit;
	}

	if (idx == -1) {
		ret = -EFAULT;
		pr_err("fail to find attach dcam id!\n");
		goto exit;
	}
	if (!(group->mode_inited & (1 << (int)idx))) {
		pr_info("dcam%d has been already disabled!\n", idx);
		/*break;*/
	}

	sprd_camioctl_io_stream_off(camerafile, dev, arg);

	ret = sprd_dcam_drv_module_dis(idx);
	if (unlikely(ret != 0)) {
		pr_err("fail to disable dcam%d module\n", idx);
		ret = -EFAULT;
	}

	if (dev->isp_id < ISP_ID_MAX) {
		ret = sprd_isp_drv_module_dis(group->dev[idx]->isp_dev_handle);
		if (unlikely(ret != 0)) {
			pr_err("fail to disable SPRD_IMG%d isp module\n",
				idx);
			ret = -EFAULT;
		}
	}

	group->mode_inited &= ~(1<<idx);

	ret = sprd_camioctl_res_put(group, idx, &res);
	if (ret) {
		pr_err("fail to put res %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}
	ret = copy_to_user((void __user *)arg, &res,
			sizeof(struct sprd_img_res));
	if (ret) {
		ret = -EFAULT;
		pr_err("fail to copy_to_user!\n");
		goto exit;
	}

exit:
	return ret;
}

static int sprd_camioctl_io_cfg_start(struct camera_file *camerafile,
		struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct camera_group *group = NULL;
	struct isp_pipe_dev *isp_dev = NULL;
	struct isp_k_block *isp_k_param = NULL;

	mutex_lock(&dev->cam_mutex);
	if (!dev->isp_dev_handle) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		mutex_unlock(&dev->cam_mutex);
		goto exit;
	}
	group = camerafile->grp;
	isp_dev = (struct isp_pipe_dev *)dev->isp_dev_handle;
	isp_k_param = &isp_dev->isp_k_param;
	if (!isp_k_param) {
		ret = -EFAULT;
		pr_err("fail to get isp_private.\n");
		mutex_unlock(&dev->cam_mutex);
		goto exit;
	}

	if (dev->cam_ctx.need_4in1 && dev->cam_ctx.need_isp_tool != 1) {
		ret = sprd_dcam_drv_module_en(DCAM_ID_1);
		if (unlikely(ret != 0)) {
			pr_err("fail to enable dcam1\n");
			mutex_unlock(&dev->cam_mutex);
			goto exit;
		}
		ret = sprd_dcam_int_reg_isr(DCAM_ID_1, DCAM_BIN_PATH_TX_DONE,
			sprd_camioctl_bin_tx_done, group->dev[DCAM_ID_0]);
		if (unlikely(ret)) {
			pr_err("fail to register dcam isr\n");
			mutex_unlock(&dev->cam_mutex);
			goto exit;
		}
		group->fetch_inited |= 1 << DCAM_ID_1;
	}

	isp_k_param->lsc_2d_weight_en = 0;
	isp_k_param->param_update_flag = 0;
	isp_k_param->lsc_load_buf_id_prv = 0;
	isp_k_param->lsc_load_buf_id_cap = 0;
	isp_k_param->lsc_load_param_init = 0;
	isp_k_param->isp_status = 0;
	mutex_unlock(&dev->cam_mutex);

exit:
	return ret;
}

static int sprd_camioctl_io_function_mode_set(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_function_mode parm;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_context *ctx = NULL;

	idx = camerafile->idx;
	ctx = &dev->cam_ctx;

	if (!dev || !ctx) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		return ret;
	}

	CAM_TRACE("%d: set function mode\n", idx);
	memset((void *)&parm, 0, sizeof(parm));
	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&parm, (void __user *)arg,
		sizeof(struct sprd_img_function_mode));
	if (ret) {
		pr_err("fail to get user info ret %d\n", ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}

	ctx->need_4in1 = parm.need_4in1;
	ctx->need_3dnr = parm.need_3dnr;
	ctx->dual_cam = parm.dual_cam;
	if ((ctx->need_4in1) || (ctx->sn_mode == DCAM_CAP_MODE_YUV)
		|| MAX(ctx->cap_in_size.w, ctx->cap_in_size.h) <=
		ISP_PATH1_LINE_BUF_LENGTH)
		ctx->need_downsizer = 0;
	else
		ctx->need_downsizer = 1;

	ret = sprd_isp_drv_path_cfg_set(dev->isp_dev_handle,
		ISP_PATH_IDX_PRE, ISP_PATH_SUPPORT_4IN1,
		&ctx->need_4in1);
	if (unlikely(ret))
		pr_err("fail to cfg 4in1 support %d\n", ctx->need_4in1);

	pr_info("Set function mode 3dnr %d, 4in1 %d dual_cam %d\n",
		ctx->need_3dnr, ctx->need_4in1, ctx->dual_cam);
	mutex_unlock(&dev->cam_mutex);

	return ret;
}

static int sprd_camioctl_io_update_param_start(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct isp_pipe_dev *isp_dev = NULL;
	struct isp_k_block *isp_k_param = NULL;

	mutex_lock(&dev->cam_mutex);

	if (!dev->isp_dev_handle) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}
	isp_dev = (struct isp_pipe_dev *)dev->isp_dev_handle;
	isp_k_param = &isp_dev->isp_k_param;
	if (!isp_k_param) {
		ret = -EFAULT;
		pr_err("fail to get isp_private.\n");
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}
	isp_k_param->param_update_flag++;
	mutex_unlock(&dev->cam_mutex);

	return ret;
}

static int sprd_camioctl_io_update_param_end(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct isp_pipe_dev *isp_dev = NULL;
	struct isp_k_block *isp_k_param = NULL;

	mutex_lock(&dev->cam_mutex);

	if (!dev->isp_dev_handle) {
		pr_err("fail to get valid input ptr\n");
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}
	isp_dev = (struct isp_pipe_dev *)dev->isp_dev_handle;
	isp_k_param = &isp_dev->isp_k_param;
	if (!isp_k_param) {
		ret = -EFAULT;
		pr_err("fail to get isp_private.\n");
		mutex_unlock(&dev->cam_mutex);
		return ret;
	}
	isp_k_param->param_update_flag--;
	mutex_unlock(&dev->cam_mutex);

	return ret;
}

static int sprd_camioctl_io_4in1_raw_addr_set(struct camera_file *camerafile,
			struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	uint32_t i = 0;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_path_spec *path = NULL;
	struct sprd_img_parm parm;
	struct camera_addr frame_addr;

	if (!dev || !camerafile) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		goto exit;
	}

	memset((void *)&parm, 0, sizeof(parm));
	memset((void *)&frame_addr, 0, sizeof(frame_addr));
	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&parm, (void __user *)arg,
				sizeof(struct sprd_img_parm));
	if (ret) {
		pr_err("fail to get user info ret %d\n", ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		goto exit;
	}

	idx = camerafile->idx;
	path = &dev->cam_ctx.cam_path[CAMERA_FULL_PATH];
	path->buf_num = parm.buffer_count;

	pr_debug("4in1 addr %x %d\n", parm.fd_array[0], parm.buffer_count);
	if (atomic_read(&dev->stream_on) == 1 &&
		path->status == PATH_RUN) {

		for (i = 0; i < parm.buffer_count; i++) {
			if (parm.fd_array[i] == 0) {
				pr_err("fail to get 4in1 raw fd\n");
				ret = -EINVAL;
				mutex_unlock(&dev->cam_mutex);
				goto exit;
			}
			frame_addr.yaddr = parm.frame_addr_array[i].y;
			frame_addr.uaddr = parm.frame_addr_array[i].u;
			frame_addr.vaddr = parm.frame_addr_array[i].v;
			frame_addr.yaddr_vir = parm.frame_addr_vir_array[i].y;
			frame_addr.uaddr_vir = parm.frame_addr_vir_array[i].u;
			frame_addr.vaddr_vir = parm.frame_addr_vir_array[i].v;
			frame_addr.mfd_y = parm.fd_array[i];
			frame_addr.mfd_u = parm.fd_array[i];
			frame_addr.mfd_v = parm.fd_array[i];
			frame_addr.buf_info.dev = &camerafile->grp->pdev->dev;

			ret = sprd_dcam_full_path_cfg_set(idx,
				DCAM_PATH_OUTPUT_ADDR,
				&frame_addr);
			if (unlikely(ret)) {
				pr_err("fail to cfg full_path 4in1 raw addr\n");
				mutex_unlock(&dev->cam_mutex);
				goto exit;
			}
		}
	} else {
		for (i = 0; i < parm.buffer_count; i++) {
			if (unlikely(parm.fd_array[i] == 0)) {
				pr_err("fail to get 4in1 raw fd\n");
				ret = -EINVAL;
				mutex_unlock(&dev->cam_mutex);
				goto exit;
			}
			frame_addr.yaddr = parm.frame_addr_array[i].y;
			frame_addr.uaddr = parm.frame_addr_array[i].u;
			frame_addr.vaddr = parm.frame_addr_array[i].v;
			frame_addr.yaddr_vir = parm.frame_addr_vir_array[i].y;
			frame_addr.uaddr_vir = parm.frame_addr_vir_array[i].u;
			frame_addr.vaddr_vir = parm.frame_addr_vir_array[i].v;
			frame_addr.mfd_y = parm.fd_array[i];
			frame_addr.mfd_u = parm.fd_array[i];
			frame_addr.mfd_v = parm.fd_array[i];
			frame_addr.buf_info.dev = &camerafile->grp->pdev->dev;

			ret = sprd_cam_queue_buf_write(
				&path->buf_queue,
				&frame_addr);
			pr_debug("y=0x%x viry=0x%x mfd=0x%x 0x%x\n",
				frame_addr.yaddr, frame_addr.yaddr_vir,
				frame_addr.mfd_y, frame_addr.mfd_u);
		}
	}
	mutex_unlock(&dev->cam_mutex);

exit:
	return ret;
}

static int sprd_camioctl_io_4in1_post_proc(struct camera_file *camerafile,
	struct camera_dev *dev, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_parm parm;
	struct isp_dev_fetch_info *fetch_info = NULL;
	struct camera_path_spec *path = NULL;
	struct camera_group *group = NULL;
	enum dcam_id idx = DCAM_ID_0;

	if (!camerafile || !dev) {
		ret = -EFAULT;
		pr_err("fail to get valid input ptr\n");
		goto exit;
	}

	memset((void *)&parm, 0, sizeof(parm));
	mutex_lock(&dev->cam_mutex);
	ret = copy_from_user(&parm, (void __user *)arg,
				sizeof(struct sprd_img_parm));
	if (ret) {
		pr_err("fail to get user info ret %d\n", ret);
		ret = -EFAULT;
		mutex_unlock(&dev->cam_mutex);
		goto exit;
	}

	group = camerafile->grp;
	idx = camerafile->idx;

	path = &dev->cam_ctx.cam_path[CAMERA_CAP_PATH];
	fetch_info = &dev->fetch_info;
	fetch_info->fetch_addr.img_fd = parm.fd_array[0];
	fetch_info->fetch_addr.offset.x = 0;
	fetch_info->size.width = path->in_size.w;
	fetch_info->size.height = path->in_size.h;
	sprd_dcam_drv_4in1_info_get(dev->cam_ctx.need_4in1);

	pr_debug("4in1 post mfd %x w %d h %d\n", parm.fd_array[0],
		fetch_info->size.width, fetch_info->size.height);
	ret = sprd_camioctl_dcam_fetch_start(idx, DCAM_ID_1, group);
	if (ret) {
		mutex_unlock(&dev->cam_mutex);
		pr_err("fail to start fetch\n");
		goto exit;
	}
	mutex_unlock(&dev->cam_mutex);

exit:
	return ret;
}

static struct dcam_io_ctrl_fun s_cam_io_ctrl_fun_tab[] = {
	{SPRD_IMG_IO_SET_MODE,            sprd_camioctl_io_cap_mode_set},
	{SPRD_IMG_IO_SET_CAP_SKIP_NUM,    sprd_camioctl_io_cap_skip_num_set},
	{SPRD_IMG_IO_SET_SENSOR_SIZE,     sprd_camioctl_io_sensor_size_set},
	{SPRD_IMG_IO_SET_SENSOR_TRIM,     sprd_camioctl_io_sensor_trim_set},
	{SPRD_IMG_IO_SET_FRM_ID_BASE,     sprd_camioctl_io_frm_id_base_set},
	{SPRD_IMG_IO_SET_CROP,            sprd_camioctl_io_crop_set},
	{SPRD_IMG_IO_SET_FLASH,           sprd_camioctl_io_flash_set},
	{SPRD_IMG_IO_SET_OUTPUT_SIZE,     sprd_camioctl_io_output_size_set},
	{SPRD_IMG_IO_SET_ZOOM_MODE,       NULL},
	{SPRD_IMG_IO_SET_SENSOR_IF,       sprd_camioctl_io_sensor_if_set},
	{SPRD_IMG_IO_SET_FRAME_ADDR,      sprd_camioctl_io_frame_addr_set},
	{SPRD_IMG_IO_PATH_FRM_DECI,       sprd_camioctl_io_path_frm_deci},
	{SPRD_IMG_IO_PATH_PAUSE,          NULL},
	{SPRD_IMG_IO_PATH_RESUME,         NULL},
	{SPRD_IMG_IO_STREAM_ON,           sprd_camioctl_io_stream_on},
	{SPRD_IMG_IO_STREAM_OFF,          sprd_camioctl_io_stream_off},
	{SPRD_IMG_IO_GET_FMT,             sprd_camioctl_io_fmt_get},
	{SPRD_IMG_IO_GET_CH_ID,           sprd_camioctl_io_ch_id_get},
	{SPRD_IMG_IO_GET_TIME,            sprd_camioctl_io_time_get},
	{SPRD_IMG_IO_CHECK_FMT,           sprd_camioctl_io_fmt_check},
	{SPRD_IMG_IO_SET_SHRINK,          sprd_camioctl_io_shrink_set},
	{SPRD_IMG_IO_CFG_FLASH,           sprd_camioctl_io_flash_cfg},
	{SPRD_IMG_IO_PDAF_CONTROL,        sprd_camioctl_io_pdaf_control},
	{SPRD_IMG_IO_EBD_CONTROL,         sprd_camioctl_io_ebd_control},
	{SPRD_IMG_IO_GET_IOMMU_STATUS,    sprd_camioctl_io_iommu_status_get},
	{SPRD_IMG_IO_START_CAPTURE,       sprd_camioctl_io_capture_start},
	{SPRD_IMG_IO_STOP_CAPTURE,        sprd_camioctl_io_capture_stop},
	{SPRD_IMG_IO_SET_PATH_SKIP_NUM,   sprd_camioctl_io_path_skip_num_set},
	{SPRD_IMG_IO_SBS_MODE,            NULL},
	{SPRD_IMG_IO_DCAM_PATH_SIZE,      sprd_camioctl_io_dcam_path_size},
	{SPRD_IMG_IO_SET_SENSOR_MAX_SIZE, sprd_camioctl_io_sensor_max_size_set},
	{SPRD_ISP_IO_CAPABILITY,          sprd_camioctl_io_capability},
	{SPRD_ISP_IO_RST,                 sprd_camioctl_io_rst},
	{SPRD_ISP_IO_SET_STATIS_BUF,      sprd_camioctl_io_statis_buf_set},
	{SPRD_ISP_IO_CFG_PARAM,           sprd_camioctl_io_cfg_param},
	{SPRD_ISP_IO_RAW_CAP,             sprd_camioctl_io_raw_cap},
	{SPRD_IMG_IO_GET_DCAM_RES,        sprd_camioctl_io_dcam_res_get},
	{SPRD_IMG_IO_PUT_DCAM_RES,        sprd_camioctl_io_dcam_res_put},
	{SPRD_ISP_IO_CFG_START,           sprd_camioctl_io_cfg_start},
	{SPRD_IMG_IO_SET_FUNCTION_MODE,   sprd_camioctl_io_function_mode_set},
	{SPRD_ISP_IO_UPDATE_PARAM_START,  sprd_camioctl_io_update_param_start},
	{SPRD_ISP_IO_UPDATE_PARAM_END,    sprd_camioctl_io_update_param_end},
	{SPRD_IMG_IO_SET_4IN1_ADDR,       sprd_camioctl_io_4in1_raw_addr_set},
	{SPRD_IMG_IO_4IN1_POST_PROC,      sprd_camioctl_io_4in1_post_proc}
};

static dcam_io_fun sprd_camioctl_get_fun(uint32_t cmd)
{
	dcam_io_fun io_ctrl = NULL;
	int total_num = 0;
	int i = 0;

	total_num = sizeof(s_cam_io_ctrl_fun_tab) /
		sizeof(struct dcam_io_ctrl_fun);
	for (i = 0; i < total_num; i++) {
		if (cmd == s_cam_io_ctrl_fun_tab[i].cmd) {
			io_ctrl = s_cam_io_ctrl_fun_tab[i].io_ctrl;
			break;
		}
	}

	return io_ctrl;
}

static uint32_t sprd_camioctl_get_val(uint32_t cmd)
{
	uint32_t nr = _IOC_NR(cmd);
	uint32_t i = 0;

	for (i = 0; i < ARRAY_SIZE(cam_ioctl_desc); i++) {
		if (nr == _IOC_NR(cam_ioctl_desc[i].ioctl_val))
			return cam_ioctl_desc[i].ioctl_val;
	}
	return -1;
}

static char *sprd_camioctl_get_str(uint32_t cmd)
{
	uint32_t nr = _IOC_NR(cmd);
	uint32_t i = 0;

	for (i = 0; i < ARRAY_SIZE(cam_ioctl_desc); i++) {
		if (nr == _IOC_NR(cam_ioctl_desc[i].ioctl_val))
			return (char *)cam_ioctl_desc[i].ioctl_str;
	}
	return "NULL";
}

int sprd_cam_ioctl_addr_write_back(void **isp_dev_handle,
	enum camera_path_id path_idx, struct camera_frame *frame_addr)
{
	int ret = 0;
	struct camera_path_spec *path_full = NULL;
	struct dcam_path_desc *path_4in1 = NULL;
	struct camera_addr cam_addr = {0};
	struct camera_dev *dev = container_of(isp_dev_handle,
				struct camera_dev, isp_dev_handle);

	if (frame_addr == NULL) {
		CAM_TRACE("fail to get valid input ptr\n");
		return -1;
	}

	if (atomic_read(&dev->stream_on) == 0) {
		CAM_TRACE("it has been stream off\n");
		return -EPERM;
	}

	path_full = &dev->cam_ctx.cam_path[CAMERA_FULL_PATH];
	if (dev->cam_ctx.need_4in1)
		path_4in1 = sprd_dcam_drv_bin_path_get(DCAM_ID_1);

	cam_addr.mfd_y = frame_addr->buf_info.mfd[0];
	cam_addr.yaddr_vir = frame_addr->yaddr_vir;
	cam_addr.uaddr = frame_addr->uaddr;
	cam_addr.buf_info = frame_addr->buf_info;

	if (dev->cam_ctx.sn_mode == DCAM_CAP_MODE_YUV
		|| dev->idx == DCAM_ID_2
		|| (path_idx == CAMERA_FULL_PATH
		&& frame_addr->width == path_full->out_size.w
		&& frame_addr->height == path_full->out_size.h)) {

		ret = sprd_dcam_full_path_cfg_set(dev->idx,
			DCAM_PATH_OUTPUT_ADDR,
			&cam_addr);
		if (unlikely(ret)) {
			pr_err("fail to cfg full_path output addr\n");
			return ret;
		}
	} else if (path_idx == CAMERA_BIN_PATH) {
		if (path_4in1 && frame_addr->width == path_4in1->output_size.w
			&& frame_addr->height == path_4in1->output_size.h) {
			frame_addr->buf_info.dev = &s_dcam_pdev->dev;
			sprd_cam_queue_buf_write(
				&path_4in1->buf_queue, frame_addr);
		} else {
			ret = sprd_dcam_bin_path_cfg_set(dev->idx,
				DCAM_PATH_OUTPUT_ADDR,
				&cam_addr);
			if (unlikely(ret)) {
				pr_err("fail to cfg bin_path output addr\n");
				return ret;
			}
		}
	} else {
		pr_err("fail to take back frame, %d %dx%d\n", path_idx,
			frame_addr->width, frame_addr->height);
	}
	CAM_TRACE("%d: success to set frame addr\n", dev->idx);

	return ret;
}

#endif
