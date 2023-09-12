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

#ifdef FEATRUE_DCAM_IOCTRL
typedef int(*dcam_io_fun) (struct camera_file *camerafile,
			   unsigned long arg, uint32_t cmd);

struct dcam_io_ctrl_fun {
	uint32_t cmd;
	dcam_io_fun io_ctrl;
};

bool has_dual_cap_started;

static int dcamio_set_mode(struct camera_file *camerafile, unsigned long arg,
			   uint32_t cmd)

{
	int ret = 0;
	uint32_t mode;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;
	enum dcam_id idx = camerafile->idx;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&mode, (void __user *) arg,
			     sizeof(uint32_t));
	if (ret) {
		pr_err("fail to get user info, SET_MODE\n");
		ret = -EFAULT;
		goto exit;
	}
	info->capture_mode = mode;
	DCAM_TRACE("DCAM%d: capture mode %d\n", idx,
		   dev->dcam_cxt.capture_mode);

exit:
	return ret;
}

static int dcamio_set_cap_skip_num(struct camera_file *camerafile,
				   unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	uint32_t skip_num;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;
	enum dcam_id idx = camerafile->idx;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&skip_num, (void __user *) arg,
			     sizeof(uint32_t));
	if (ret) {
		pr_err("fail to get user info\n");
		ret = -EFAULT;
		goto exit;
	}
	info->skip_number = skip_num;
	DCAM_TRACE("%d: cap skip number %d\n", idx,
		   dev->dcam_cxt.skip_number);

exit:
	return ret;
}

static int dcamio_set_sensor_size(struct camera_file *camerafile,
				  unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	struct sprd_img_size size;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;
	enum dcam_id idx = camerafile->idx;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&size, (void __user *)arg,
			     sizeof(struct sprd_img_size));
	if (unlikely(ret)) {
		pr_err("fail to get user info, SET_SENSOR_SIZE\n");
		ret = -EFAULT;
		goto exit;
	}
	info->cap_in_size.w = size.w;
	info->cap_in_size.h = size.h;
	DCAM_TRACE("DCAM%d: sensor size %d %d\n", idx,
		   info->cap_in_size.w, info->cap_in_size.h);

exit:
	return ret;
}

static int dcamio_set_sensor_trim(struct camera_file *camerafile,
				  unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	struct sprd_img_rect rect;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&rect, (void __user *)arg,
			     sizeof(struct sprd_img_rect));
	if (unlikely(ret)) {
		pr_err("fail to get user info, SET_SENSOR_TRIM\n");
		ret = -EFAULT;
		goto exit;
	}
	info->cap_in_rect.x = rect.x;
	info->cap_in_rect.y = rect.y;
	info->cap_in_rect.w = rect.w;
	info->cap_in_rect.h = rect.h;

	info->cap_out_size.w = info->cap_in_rect.w;
	info->cap_out_size.h = info->cap_in_rect.h;
	DCAM_TRACE("%d: sensor trim x y w h %d %d %d %d\n",
		   camerafile->idx,
		   info->cap_in_rect.x,
		   info->cap_in_rect.y,
		   info->cap_in_rect.w,
		   info->cap_in_rect.h);

exit:
	return ret;
}

static int dcamio_set_frame_id_base(struct camera_file *camerafile,
				    unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	struct sprd_img_parm parm;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&parm, (void __user *)arg,
			     sizeof(struct sprd_img_parm));
	if (unlikely(ret)) {
		pr_err("fail to get user info, SET_FRM_ID_BASE\n");
		ret = -EFAULT;
		goto exit;
	}

	switch (parm.channel_id) {
	case CAMERA_RAW_PATH:
	case CAMERA_PRE_PATH:
	case CAMERA_VID_PATH:
	case CAMERA_CAP_PATH:
	case CAMERA_PDAF_PATH:
	case CAMERA_AEM_PATH:
		info->dcam_path[parm.channel_id].frm_id_base =
			parm.frame_base_id;
		break;
	default:
		pr_info("%d: wrong channel ID, %d\n",
			camerafile->idx,
			parm.channel_id);
		goto exit;
	}
	/* set every frame */
	DCAM_TRACE_INT("%d: channel %d, base id 0x%x\n",
		       camerafile->idx,
		       parm.channel_id,
		       parm.frame_base_id);

exit:
	return ret;
}

static int dcamio_set_crop(struct camera_file *camerafile,
			   unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	struct sprd_img_parm parm;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&parm, (void __user *)arg,
			     sizeof(struct sprd_img_parm));
	DCAM_TRACE("crop %u %u %u %u\n", parm.crop_rect.x,
		parm.crop_rect.y, parm.crop_rect.w, parm.crop_rect.h);
	if (unlikely(ret)) {
		pr_err("fail to get user info, SET_CROP\n");
		ret = -EFAULT;
		goto exit;
	}
	ret = sprd_img_set_crop(camerafile, &parm);
exit:
	return ret;
}

static int dcamio_set_flash(struct camera_file *camerafile,
			    unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	uint32_t led0_ctrl;
	uint32_t led1_ctrl;
	uint32_t led0_status;
	uint32_t led1_status;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&info->set_flash,
			     (void __user *)arg,
			     sizeof(struct sprd_img_set_flash));
	if (ret) {
		pr_err("fail to get user info\n");
		ret = -EFAULT;
		goto exit;
	}

	led0_ctrl = info->set_flash.led0_ctrl;
	led1_ctrl = info->set_flash.led1_ctrl;
	led0_status = info->set_flash.led0_status;
	led1_status = info->set_flash.led1_status;

#if 0 /* should defer to EoF */
	if ((led0_ctrl &&
	     (led0_status == FLASH_CLOSE_AFTER_OPEN ||
	      led0_status == FLASH_CLOSE ||
	      led0_status == FLASH_CLOSE_AFTER_AUTOFOCUS)) ||
	    (led1_ctrl &&
	     (led1_status == FLASH_CLOSE_AFTER_OPEN ||
	      led1_status == FLASH_CLOSE ||
	      led1_status == FLASH_CLOSE_AFTER_AUTOFOCUS))) {
		complete(&dev->flash_thread_com);
	}
#endif

	DCAM_TRACE("led0_ctrl %d led0_status %d\n",
		   info->set_flash.led0_ctrl,
		   info->set_flash.led0_status);
	DCAM_TRACE("led1_ctrl %d led1_status %d\n",
		   info->set_flash.led1_ctrl,
		   info->set_flash.led1_status);

exit:
	return ret;
}

static int dcamio_set_output_size(struct camera_file *camerafile,
				  unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	struct sprd_img_parm parm;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;
	enum dcam_id idx = DCAM_ID_0;
	struct camera_group *group = NULL;
	enum isp_work_mode mid;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}

	group = camerafile->grp;
	idx = camerafile->idx;

	ret = copy_from_user(&parm, (void __user *)arg,
			     sizeof(struct sprd_img_parm));
	DCAM_TRACE("%d:dst_size:%u %u\n", idx, parm.dst_size.w,
			parm.dst_size.h);
	if (unlikely(ret)) {
		pr_err("fail to get user info, SET_OUTPUT_SIZE\n");
		ret = -EFAULT;
		goto exit;
	}
	info->dst_size.w = parm.dst_size.w;
	info->dst_size.h = parm.dst_size.h;
	info->pxl_fmt = parm.pixel_fmt;
	info->need_isp_tool = parm.need_isp_tool;
	info->raw_callback = parm.raw_callback;
	info->need_isp = parm.need_isp;
	info->rt_refocus = parm.rt_refocus;
	info->path_input_rect.x = parm.crop_rect.x;
	info->path_input_rect.y = parm.crop_rect.y;
	info->path_input_rect.w = parm.crop_rect.w;
	info->path_input_rect.h = parm.crop_rect.h;
	info->scene_mode = parm.scene_mode;
	info->is_slow_motion = parm.slowmotion;
	pr_debug("SPRD_IMG_IO_SET_OUTPUT_SIZE, scene_mode %d, slowmotion %d\n",
		 parm.scene_mode, parm.slowmotion);

	if (info->pxl_fmt == IMG_PIX_FMT_GREY && info->need_isp_tool != 1)
		info->raw_callback = 1;
	else
		info->raw_callback = 0;

	if (info->scene_mode == DCAM_SCENE_MODE_PREVIEW ||
	    info->scene_mode == DCAM_SCENE_MODE_RECORDING ||
	    info->scene_mode == DCAM_SCENE_MODE_CAPTURE_CALLBACK) {
		info->uframe_sync = 1;
		pr_info("uframe_sync: %d\n", info->uframe_sync);
	}
	/*
	 * TODO:  Need to switch slow motion mid back to AP mode till
	 * the preview screen split issue resolved.
	 */
#if 0
	mid = (parm.slowmotion & 0x01) ? ISP_AP_MODE : ISP_CFG_MODE;
#endif
	mid = ISP_CFG_MODE;
	pr_info("mid:%d, dcam_id/isp_id %d\n", mid, idx);

	/*
	 * Composite three ids into one com_idx, using ISP_SCENE_PRE as
	 * the default scene id.
	 */
	ISP_SET_MID((((struct isp_pipe_dev *)
		     (group->dev[idx]->isp_dev_handle))->com_idx), mid);
exit:
	return ret;
}

static int dcamio_statis_cfg_param(struct camera_file *camerafile,
				   unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = sprd_statistics_k_ioctl(dev->isp_dev_handle,
				info, cmd, arg, camerafile->idx);
	if (ret)
		pr_err("fail to sprd_statistics_k_ioctl\n");

exit:
	return ret;
}

static int dcamio_isp_k_ioctl_raw(struct camera_file *camerafile,
				  unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}

	mutex_lock(&dev->dcam_mutex);
	if (unlikely(atomic_read(&dev->stream_on) == 0)) {
		ret = sprd_img_queue_init(&dev->queue);
		if (unlikely(ret != 0)) {
			pr_err("fail to init queue\n");
			mutex_unlock(&dev->dcam_mutex);
			return ret;
		}
		ret = sprd_img_pulse_queue_init(
			&dev->dcam_cxt.pulse_info.vcm_queue);
		if (unlikely(ret != 0)) {
			pr_err("fail to init pulse queue\n");
			mutex_unlock(&dev->dcam_mutex);
			return ret;
		}
		ret = sprd_img_isp_reg_isr(dev);
		if (unlikely(ret)) {
			pr_err("fail to register isp isr\n");
			mutex_unlock(&dev->dcam_mutex);
			return ret;
		}
		dev->is_simulation_mode = 1;
		atomic_set(&dev->stream_on, 1);
	}
	mutex_unlock(&dev->dcam_mutex);

	ret = sprd_isp_k_ioctl(dev->isp_dev_handle, cmd, arg);
	if (ret) {
		pr_err("fail to sprd_img_k_ioctl\n");
		goto exit;
	}

exit:
	return ret;
}

static int dcamio_get_dcam_res(struct camera_file *camerafile,
			       unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	int idx = DCAM_ID_0;
	struct sprd_img_res res = {0};
	struct camera_group *group = NULL;

	if (!camerafile) {
		ret = -EFAULT;
		pr_err("fail to get camerafile\n");
		goto exit;
	}

	group = camerafile->grp;
	if (!group) {
		ret = -EFAULT;
		pr_err("fail to get group\n");
		goto exit;
	}

	ret = copy_from_user(&res, (void __user *)arg,
			     sizeof(struct sprd_img_res));
	if (ret) {
		pr_err("fail to copy_from_user\n");
		ret = -EFAULT;
		goto exit;
	}

	ret = sprd_img_get_res(group, &res);
	if (ret) {
		pr_err("fail to get res\n");
		goto exit;
	}

	idx = sprd_sensor_find_dcam_id(res.sensor_id);
	if (idx < 0) {
		pr_err("fail to find id\n");
		ret = -1;
		goto exit;
	}
	if (group->mode_inited & (1 << (int)idx)) {
		pr_info("dcam%d has been enabled!\n", idx);
		goto exit;
	}
	ret = sprd_dcam_module_en(idx);
	if (unlikely(ret != 0)) {
		pr_err("fail to enable dcam module %d\n", idx);
		goto exit;
	}

	if (group->mode_inited > 0) {
		pr_info("idx %d, enabling dual cam, mode_inited 0x%x\n",
				(int)idx, group->mode_inited);
		is_dual_cam = 1;
	}
	ret = sprd_isp_module_en(group->dev[idx]->isp_dev_handle,
		(enum isp_id)idx);
	if (unlikely(ret != 0)) {
		pr_err("fail to enable isp module %d\n", idx);
		goto exit;
	}
	camerafile->idx = idx;
	group->mode_inited |= 1 << idx;
	DCAM_TRACE("dcam dev[%d], inited %d res_used %d\n",
		idx, group->dev_inited,
		group->dcam_res_used);
	DCAM_TRACE("w=%u,h=%u,sensor_id=%u,flag=0x%x\n", res.width,
		res.height, res.sensor_id, res.flag);
	ret = copy_to_user((void __user *)arg, &res,
			   sizeof(struct sprd_img_res));
	if (ret) {
		pr_err("fail to copy_to_user\n");
		ret = -EFAULT;
	}

exit:
	return ret;
}

static int dcamio_put_dcam_res(struct camera_file *camerafile,
			       unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	int idx = DCAM_ID_0;
	struct sprd_img_res res = {0};
	struct camera_group *group = NULL;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	if (!camerafile) {
		ret = -EFAULT;
		pr_err("fail to get camerafile\n");
		goto exit;
	}

	group = camerafile->grp;
	if (!group) {
		ret = -EFAULT;
		pr_err("fail to get group\n");
		goto exit;
	}

	ret = copy_from_user(&res, (void __user *)arg,
			     sizeof(struct sprd_img_res));
	if (ret) {
		pr_err("fail to copy_from_user\n");
		ret = -EFAULT;
		goto exit;
	}

	idx = sprd_sensor_find_dcam_id(res.sensor_id);
	if (unlikely(idx < 0)) {
		pr_err("fail to find id\n");
		ret = -1;
		goto exit;
	}
	pr_info("dev_inited:%d, idx: %d\n", group->dev_inited, idx);
	if (!(group->mode_inited & (1 << (int)idx))) {
		pr_info("dcam%d has been already disabled!\n", idx);
		goto exit;
	}

	/*
	 * Disable modules includes power down. Can't stream off after that.
	 * Sometimes there is no stream off ioctl coming. Do it here before
	 * disabling modules.
	 */
	ret = sprd_camera_stream_off(group, idx);
	if (unlikely(ret != 0))
			pr_err("SPRD_IMG%d: fail to stream off\n", idx);

	sprd_img_get_dcam_dev(camerafile, &dev, &info);

	if (info)
		info->is_3dnr = 0;
	if (dev->dcam_cxt.need_isp_tool) {
		ret = sprd_isp_stop_pipeline(dev->isp_dev_handle);
		if (ret)
			pr_err("fail to stop isp pipeline\n");
	}

	ret = sprd_dcam_module_dis(idx);
	if (unlikely(ret != 0)) {
		pr_err("SPRD_IMG%d: fail to disable dcam module\n",
			idx);
		goto exit;
	}
	ret = sprd_isp_module_dis(group->dev[idx]->isp_dev_handle,
		(enum isp_id)idx);
	if (unlikely(ret != 0)) {
		pr_err("SPRD_IMG%d: fail to disable isp module\n",
			idx);
		goto exit;
	}
	group->mode_inited &= ~(1<<idx);
	if (is_dual_cam) {
		pr_info("disabling dual cam mode, mode_inited 0x%x\n",
				group->mode_inited);
		if (group->mode_inited == 0) {
			is_dual_cam = 0;
		}
	}

	ret = sprd_img_put_res(group, &res);
	if (ret) {
		pr_err("fail to put res\n");
		goto exit;
	}
	ret = copy_to_user((void __user *)arg, &res,
			   sizeof(struct sprd_img_res));
	if (ret) {
		pr_err("fail to copy_to_user\n");
		ret = -EFAULT;
		goto exit;
	}

	DCAM_TRACE("w=%u,h=%u,sensor_id=%u,flag=0x%x\n", res.width,
		res.height, res.sensor_id, res.flag);

exit:
	return ret;
}

static int dcamio_set_pulse_line(struct camera_file *camerafile,
				 unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&info->pulse_info.pulse_line,
			     (void __user *)arg,
			     sizeof(uint32_t));
	if (ret) {
		pr_err("fail to get user info\n");
		ret = -EFAULT;
		goto exit;
	}

	if (atomic_read(&dev->stream_on) == 1) {
		ret = set_dcam_cap_cfg(camerafile->idx,
				       DCAM_CAP_FRM_PULSE_LINE,
				       &info->pulse_info.pulse_line);
		if (ret)
			pr_err("fail to set_dcam_cap_cfg\n");
	}

exit:
	return ret;
}

static int dcamio_set_next_vcm_pos(struct camera_file *camerafile,
				   unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;
	struct sprd_img_vcm_param vcm_info;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}

	mutex_lock(&info->pulse_info.pulse_mutex);
	ret = copy_from_user(&vcm_info,
			     (void __user *)arg,
			     sizeof(vcm_info));
	if (ret) {
		pr_err("fail to get user info\n");
		ret = -EFAULT;
		goto exit;
	}

	sprd_img_pulse_write(&info->pulse_info.vcm_queue, &vcm_info);
exit:
	mutex_unlock(&info->pulse_info.pulse_mutex);
	return ret;
}

static int dcamio_set_vcm_log(struct camera_file *camerafile, unsigned long arg,
			      uint32_t cmd)
{
	int ret = 0;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&info->pulse_info.enable_debug_info,
			     (void __user *)arg,
			     sizeof(uint32_t));
	if (ret) {
		pr_err("fail to get user info\n");
		ret = -EFAULT;
	}

exit:
	return ret;
}

static int dcamio_set_3dnr(struct camera_file *camerafile,
			   unsigned long arg,
			   uint32_t cmd)
{
	int ret = 0;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;
	struct sprd_img_3dnr_param param = {0};

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}

	ret = copy_from_user(&param, (void __user *)arg, sizeof(param));
	if (unlikely(ret)) {
		pr_err("fail to get user info, SPRD_IMG_IO_SET_3DNR\n");
		ret = -EFAULT;
		goto exit;
	}

	if (param.is_3dnr == 1) {
		pr_info("enabling 3dnr, width %d, height %d\n",
				param.w, param.h);
		info->small_size.w = param.w;
		info->small_size.h = param.h;
	}
	info->is_3dnr = param.is_3dnr;
exit:
	return ret;
}

static int dcamio_set_zoom_mode(struct camera_file *camerafile,
				unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	uint32_t zoom;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&zoom, (void __user *) arg,
			     sizeof(uint32_t));
	if (ret) {
		pr_err("fail to get user info\n");
		ret = -EFAULT;
		goto exit;
	}
	info->is_smooth_zoom = zoom;
	DCAM_TRACE("%d: set zoom mode %d\n",
		   camerafile->idx,
		   info->is_smooth_zoom);

exit:
	return ret;
}

static int dcamio_set_sensor_if(struct camera_file *camerafile,
				unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	struct sprd_img_sensor_if sensor;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&sensor,
			     (void __user *)arg,
			     sizeof(struct sprd_img_sensor_if));
	if (unlikely(ret)) {
		pr_err("fail to get user info, SET_SENSOR_IF\n");
		ret = -EFAULT;
		goto exit;
	}
	DCAM_TRACE("%d: sensor:type0x%x,fmt0x%x,deci%d\n",
		   camerafile->idx,
		   sensor.if_type,
		   sensor.img_fmt,
		   sensor.frm_deci);

	ret = sprd_img_set_sensor_if(camerafile, &sensor);

exit:
	return ret;
}

static int dcamio_set_frame_addr(struct camera_file *camerafile,
				 unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	struct sprd_img_parm parm;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&parm, (void __user *)arg,
			     sizeof(struct sprd_img_parm));
	if (unlikely(ret)) {
		pr_err("fail to get user info. SET_FRAME_ADDR\n");
		ret = -EFAULT;
		goto exit;
	}
	ret = sprd_img_set_frame_addr(camerafile, &parm);

exit:
	return ret;
}

static int dcamio_set_path_frame_deci(struct camera_file *camerafile,
				      unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	struct sprd_img_parm parm;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;
	struct camera_path_spec *path = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&parm, (void __user *)arg,
			     sizeof(struct sprd_img_parm));
	if (unlikely(ret)) {
		pr_err("fail to get user info, PATH_FRM_DECI\n");
		ret = -EFAULT;
		goto exit;
	}
	DCAM_TRACE("isp path channel id: %d frame deci: %d\n",
		   parm.channel_id, parm.deci);
	path = &info->dcam_path[parm.channel_id];
	path->path_frm_deci = parm.deci; /* deci on raw_path or isp path */

exit:
	return ret;
}

static int dcamio_stream_on(struct camera_file *camerafile, unsigned long arg,
			    uint32_t cmd)
{
	int ret = 0;
	struct camera_dev *dev = NULL;
	struct isp_pipe_dev *isp_dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		return ret;
	}

	if (unlikely((dev->idx == 1) && (is_dual_cam_dore != 0))) {
		if (!(is_dual_cam_dore & BIT(5))) {
			is_dual_cam_dore |= BIT(8);
			ret = wait_for_completion_interruptible(
				&camerafile->grp->dualcam_recovery_com);
		}
		usleep_range(33000*3, 34000*3);
	}

	isp_dev = (struct isp_pipe_dev *)dev->isp_dev_handle;
	if (unlikely(!IS_ERR_OR_NULL(isp_dev)))
		isp_dev->is_hdr = info->is_hdr;

	ret = sprd_camera_stream_on(camerafile);
	if (ret)
		pr_err("fail to stream on\n");

	if (unlikely(is_dual_cam_dore != 0)) {
		if (dev->idx == 0) {
			is_dual_cam_dore |= BIT(5);
			if ((is_dual_cam_dore & BIT(8))) {
				complete(
				&camerafile->grp->dualcam_recovery_com);
			}
		} else {
			if (dual_cam_cap_sta == 0)
				is_dual_cam_dore = 0;
		}
	}

	return ret;
}

int dcamio_stream_off(struct camera_file *camerafile, unsigned long arg,
			     uint32_t cmd)
{
	int ret = 0;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;
	struct camera_group *group = NULL;
	enum dcam_id idx = DCAM_ID_0;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}

	group = camerafile->grp;
	idx = camerafile->idx;

	if (unlikely(atomic_read(&dev->stream_on) == 0)) {
		pr_info("camdev%d already stream off\n", idx);
		goto exit;
	}

	ret = sprd_camera_stream_off(group, idx);
	if (ret) {
		pr_err("fail to get stream off\n");
		goto exit;
	}

	info->is_3dnr = 0;
	if (dev->dcam_cxt.need_isp_tool) {
		ret = sprd_isp_stop_pipeline(dev->isp_dev_handle);
		if (ret)
			pr_err("fail to stop isp pipeline\n");
	}

exit:
	return ret;
}

static int dcamio_get_fmt(struct camera_file *camerafile, unsigned long arg,
			  uint32_t cmd)
{
	int ret = 0;
	struct sprd_img_get_fmt fmt_desc;
	struct camera_format *fmt = NULL;

	ret = copy_from_user(&fmt_desc, (void __user *)arg,
			     sizeof(struct sprd_img_get_fmt));
	if (unlikely(ret)) {
		pr_err("fail to get user info, GET_FMT, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}
	if (unlikely(fmt_desc.index >= ARRAY_SIZE(dcam_img_fmt))) {
		pr_err("fail to get valid index > arrar size\n");
		ret = -EINVAL;
		goto exit;
	}

	DCAM_TRACE("fmt, index%d,fmt:0x%x\n",
		fmt_desc.index, fmt_desc.fmt);
	fmt = &dcam_img_fmt[fmt_desc.index];
	fmt_desc.fmt = fmt->fourcc;

	ret = copy_to_user((void __user *)arg, &fmt_desc,
			   sizeof(struct sprd_img_get_fmt));
	if (unlikely(ret)) {
		pr_err("fail to put user info, GET_FMT, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}
exit:
	return ret;
}

static int dcamio_get_ch_id(struct camera_file *camerafile, unsigned long arg,
                            uint32_t cmd)
{
	int ret = 0;
	uint32_t channel_id;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	sprd_img_get_free_channel(camerafile, &channel_id, info->scene_mode);
	DCAM_TRACE("free channel%u,scene%u\n",
		channel_id, info->scene_mode);
	ret = copy_to_user((void __user *) arg, &channel_id, sizeof(uint32_t));
	if (unlikely(ret)) {
		pr_err("fail to put user info, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

	if (channel_id < CAMERA_MAX_PATH) {
		/* set dst_size to path.isp_out_size
		 * This IOCTL(GET_CH_ID) must after IOCTRL(OUTPUT_SIZE)
		 * Don't know channel_id when OUTPUT_SIZE
		 */
		dev->dcam_cxt.dcam_path[channel_id].isp_out_size =
			dev->dcam_cxt.dst_size;
		DCAM_TRACE("path[%d].isp_out_size: %d %d\n",
			channel_id, dev->dcam_cxt.dst_size.w,
			dev->dcam_cxt.dst_size.h);
	}

exit:
	return ret;
}

static int dcamio_get_time(struct camera_file *camerafile, unsigned long arg,
                           uint32_t cmd)
{
	int ret = 0;
	struct timeval time;
	struct sprd_img_time utime;

	DCAM_TRACE("get time\n");
	img_get_timestamp(&time);
	utime.sec = time.tv_sec;
	utime.usec = time.tv_usec;
	ret = copy_to_user((void __user *)arg, &utime,
			   sizeof(struct sprd_img_time));
	if (unlikely(ret)) {
		pr_err("fail to put user info, ret %d\n", ret);
		return -EFAULT;
	}

	return ret;
}

static int dcamio_check_fmt(struct camera_file *camerafile, unsigned long arg,
			    uint32_t cmd)
{
	int ret = 0;
	struct sprd_img_format img_format;

	ret = copy_from_user(&img_format, (void __user *)arg,
			     sizeof(struct sprd_img_format));
	if (unlikely(ret)) {
		pr_err("fail to get user info,CHECK_FMT\n");
		ret = -EFAULT;
		goto exit;
	}
	DCAM_TRACE("check fmt,chid%d, fource0x%x\n",
		img_format.channel_id, img_format.fourcc);
	ret = sprd_img_check_fmt(camerafile, &img_format);
	if (unlikely(ret)) {
		pr_err("fail to check_fmt\n");
		goto exit;
	}

	ret = copy_to_user((void __user *)arg,
			   &img_format,
			   sizeof(struct sprd_img_format));
	if (unlikely(ret)) {
		pr_err("fail to put user info, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}
exit:
	return ret;
}

static int dcamio_set_shrink(struct camera_file *camerafile, unsigned long arg,
			     uint32_t cmd)
{
	int ret = 0;
	struct sprd_img_parm parm;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;
	struct camera_path_spec *path = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&parm, (void __user *)arg,
			     sizeof(struct sprd_img_parm));
	if (unlikely(ret)) {
		pr_err("fail to get user info, SET_SHRINK\n");
		ret = -EFAULT;
		goto exit;
	}
	path = &info->dcam_path[parm.channel_id];
	path->regular_desc = parm.regular_desc;
	DCAM_TRACE("channel %d, regular mode %d\n",
		   parm.channel_id, path->regular_desc.regular_mode);

exit:
	return ret;
}

static int dcamio_cfg_flash(struct camera_file *camerafile, unsigned long arg,
			    uint32_t cmd)
{
	int ret = 0;
	struct sprd_flash_cfg_param cfg_parm;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&cfg_parm,
			     (void __user *)arg,
			     sizeof(struct sprd_flash_cfg_param));
	if (ret) {
		ret = -EFAULT;
		goto exit;
	}

	ret = sprd_flash_cfg(&cfg_parm);
	if (ret) {
		pr_err("fail to sprd_flash_cfg\n");
		goto exit;
	}
	DCAM_TRACE("config flash, ret %d\n", ret);

exit:
	return ret;
}

static int dcamio_pdaf_ctrl(struct camera_file *camerafile, unsigned long arg,
			    uint32_t cmd)
{
	int ret = 0;
	struct sprd_img_parm parm;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;
	struct camera_path_spec *path = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&parm, (void __user *)arg,
			     sizeof(struct sprd_img_parm));
	if (unlikely(ret)) {
		pr_err("fail to get user info, PDAF_CONTROL\n");
		ret = -EFAULT;
		goto exit;
	}
	path = &info->dcam_path[CAMERA_PDAF_PATH];
	memcpy(&(path->pdaf_ctrl), &(parm.pdaf_ctrl),
		sizeof(struct sprd_pdaf_control));
	if (path->pdaf_ctrl.mode != 0)
		path->is_work = 1; /*can be changed */
	DCAM_TRACE("channel %d, pdaf mode %d type %d\n",
		   parm.channel_id, path->pdaf_ctrl.mode,
		   path->pdaf_ctrl.phase_data_type);
exit:
	return ret;
}

static int dcamio_set_statis_buf(struct camera_file *camerafile,
				 unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	struct isp_statis_buf_input parm_inptr;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		return ret;
	}

	mutex_lock(&dev->dcam_mutex);
	ret = copy_from_user(&parm_inptr,
			     (const void __user *)arg,
			     sizeof(struct isp_statis_buf_input));
	if (ret != 0) {
		pr_err("fail to copy_from_user\n");
		ret = -EFAULT;
		goto exit;
	}
	if (parm_inptr.buf_flag == STATIS_BUF_FLAG_INIT) {
		ret = sprd_isp_cfg_statis_buf(dev->isp_dev_handle,
					      &dev->statis_module_info,
					      &parm_inptr);
		dev->init_inptr = parm_inptr;
		if (parm_inptr.statis_valid & ISP_STATIS_VALID_RAW)
			info->is_raw_rt = 1;
		else
			info->is_raw_rt = 0;
	} else
		ret = sprd_isp_set_statis_addr(dev->isp_dev_handle,
					       &dev->statis_module_info,
					       &parm_inptr);
	if (ret)
		pr_err("fail to set buffer\n");

exit:
	mutex_unlock(&dev->dcam_mutex);
	return ret;
}

static int dcamio_get_iommu_status(struct camera_file *camerafile,
				   unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	uint32_t iommu_enable;
	struct camera_group *group = NULL;

	if (!camerafile) {
		ret = -EFAULT;
		pr_err("fail to get camerafile\n");
		goto exit;
	}

	group = camerafile->grp;
	if (!group) {
		ret = -EFAULT;
		pr_err("fail to get group\n");
		goto exit;
	}

	ret = copy_from_user(&iommu_enable, (void __user *)arg,
			     sizeof(unsigned char));
	if (ret) {
		pr_err("fail to get copy_from_user\n");
		ret = -EFAULT;
		goto exit;
	}
	if (sprd_iommu_attach_device(&group->pdev->dev) == 0)
		iommu_enable = 1;
	else
		iommu_enable = 0;

	ret = copy_to_user((void __user *)arg, &iommu_enable,
			   sizeof(unsigned char));
	if (unlikely(ret)) {
		pr_err("fail to put user info, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}
	DCAM_TRACE("iommu_enable:%d\n", iommu_enable);
exit:
	return ret;
}

static int dcamio_start_capture(struct camera_file *camerafile,
				unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	uint32_t cap_flag = 0;
	struct sprd_img_capture_param capture_param;
	struct camera_dev *dev = NULL, *dev_other = NULL;
	struct camera_info *info = NULL;
	struct camera_file t_camerafile;
	struct isp_pipe_dev *isp_dev = NULL, *isp_dev_other = NULL;
	int idx = 0, idx_other = 0;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit_nolock;
	}

	if (unlikely(atomic_read(&dev->stream_on) == 0)) {
		pr_info("idx:%d stream not on, skip captrue req.\n",
				dev->idx);
		goto exit_nolock;
	}

	ret = copy_from_user(&capture_param, (void __user *) arg,
			     sizeof(capture_param));
	if (ret) {
		pr_err("fail to get user info\n");
		ret = -EFAULT;
		goto exit_nolock;
	}

	cap_flag = capture_param.type;
	isp_dev = dev->isp_dev_handle;
	isp_dev->frm_cnt_cap = capture_param.cap_cnt;

	/* In practice, start captures may run parallelly, with no lock. */
	if (is_dual_cam)
		mutex_lock(&camerafile->grp->camera_dualcam_mutex);

	if (dev->cap_flag != DCAM_CAPTURE_STOP) {
		pr_warn("capture status is not DCAM_CAPTURE_STOP\n");
		goto exit;
	}

	dev->cap_flag = DCAM_CAPTURE_START;
	if (dev->dcam_cxt.need_isp_tool)
		cap_flag = DCAM_CAPTURE_NONE;

	pr_info("dcam%d, start capture, cap_flag %d\n",
			dev->idx, cap_flag);

	if (isp_dev->is_yuv_sn) {
		/* enable capture for yuv sensor */
		isp_dev->cap_on = 1;
		goto exit;
	}

	if (!is_dual_cam) {
		int i;
#define DEQUEUE_TIMEOUT	(DCAM_TIMEOUT * 1000) /* us */
#define DEQUEUE_TIMEOUT_MARGIN	(5 * 1000) /* us */
#define DEQUEUE_TIMES	10
#define DEQUEUE_INTERVAL	(DEQUEUE_TIMEOUT / DEQUEUE_TIMES)
		for (i = 0; i < DEQUEUE_TIMES; i++) {
			ret = sprd_isp_start_pipeline_full(dev->isp_dev_handle,
				cap_flag);
			if (ret != -ENOMEM)
				break;
			usleep_range(DEQUEUE_INTERVAL,
				DEQUEUE_INTERVAL +
					DEQUEUE_TIMEOUT_MARGIN);
		}
		if (ret) {
			pr_err("fail to start offline\n");
			goto exit;
		}
	} else {
		if (g_isp_dev_parray[0] == isp_dev) {
			isp_dev_other = g_isp_dev_parray[1];
			idx = 0;
			idx_other = 1;
		} else {
			isp_dev_other = g_isp_dev_parray[0];
			idx = 1;
			idx_other = 0;
		}
		t_camerafile = *camerafile;
		t_camerafile.idx = idx_other;
		ret = sprd_img_get_dcam_dev(&t_camerafile, &dev_other,
			&info);
		if (ret) {
			pr_err("fail to get dcam dev\n");
			goto exit;
		}

		/*ducal camera always start capture with both cameras*/
		if (likely(is_dual_cam && (!is_dual_cam_dore)))
			dual_cam_cap_sta |= 0x3;

		if (dev_other->cap_flag == DCAM_CAPTURE_START) {
			struct isp_pipe_dev *isp_dev_first;
			struct isp_pipe_dev *isp_dev_second;
			int idx_first, idx_second;

			sprd_isp_wait_for_buffers((void *)isp_dev);
			sprd_isp_wait_for_buffers((void *)isp_dev_other);

			/* streaming first, start capture first */
			if (isp_dev->delta_full) {
				isp_dev_first = isp_dev_other;
				isp_dev_second = isp_dev;
				idx_first = idx_other;
				idx_second = idx;
			} else {
				isp_dev_first = isp_dev;
				isp_dev_second = isp_dev_other;
				idx_first = idx;
				idx_second = idx_other;
			}

			has_dual_cap_started = true;

			if (unlikely(is_dual_cam_dore)) {
				/* disable dcam path to reduce bandwidth */
				dcam_path_pause(idx_second);

				ret = sprd_isp_start_pipeline_full(
					isp_dev_second, cap_flag);
				if (ret) {
					pr_err("fail to start offline\n");
					goto exit;
				}

				/* disable dcam path to reduce bandwidth */
				dcam_path_pause(idx_first);

				ret = sprd_isp_start_pipeline_full(
					isp_dev_first, cap_flag);
				if (ret) {
					pr_err("fail to start offline\n");
					goto exit;
				}

				is_dual_cam_dore |= BIT(13);
				if (is_dual_cam_dore & BIT(15))
					complete(
					&camerafile->grp->dualcam_recovery_com);
			} else {
				/* disable dcam path to reduce bandwidth */
				dcam_path_pause(idx_first);

				ret = sprd_isp_start_pipeline_full(
					isp_dev_first, cap_flag);
				if (ret) {
					pr_err("fail to start offline\n");
					goto exit;
				}

				/* disable dcam path to reduce bandwidth */
				dcam_path_pause(idx_second);

				ret = sprd_isp_start_pipeline_full(
					isp_dev_second, cap_flag);
				if (ret) {
					pr_err("fail to start offline\n");
					goto exit;
				}
			}
		} else
			pr_info("postpone start capture...\n");
	}

exit:
	if (is_dual_cam)
		mutex_unlock(&camerafile->grp->camera_dualcam_mutex);
exit_nolock:
	pr_info("dcam%d, start capture -\n", dev->idx);
	return ret;
}

static int dcamio_stop_capture(struct camera_file *camerafile,
			       unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	int idx_other = 0;
	struct camera_dev *dev = NULL;
	struct camera_dev *dev_other = NULL;
	struct isp_pipe_dev *isp_dev = NULL;
	struct camera_file t_camerafile;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}

	if (unlikely(is_dual_cam_dore & BIT(10)))
		if (!(is_dual_cam_dore & BIT(13))) {
			is_dual_cam_dore |= BIT(15);
			pr_info("waiting for recovery complete\n");
			ret = wait_for_completion_interruptible(
				&camerafile->grp->dualcam_recovery_com);
			pr_info("continue\n");
		}

	if (is_dual_cam)
		has_dual_cap_started = false;

	if (likely(is_dual_cam && (!is_dual_cam_dore)))
		dual_cam_cap_sta &= (~(BIT(dev->idx)));

	if (dev->cap_flag != DCAM_CAPTURE_STOP) {
		pr_info("dcam%d, stop capture, flag %d\n",
				dev->idx, dev->cap_flag);
		ret = sprd_isp_stop_pipeline(dev->isp_dev_handle);
		if (ret) {
			pr_err("fail to stop offline\n");
			goto exit;
		}
		dev->cap_flag = DCAM_CAPTURE_STOP;
	} else if (unlikely(is_dual_cam_dore & BIT(5)))
		dual_cam_cap_sta &= (~(BIT(dev->idx)));

	if (is_dual_cam) {
		isp_dev = dev->isp_dev_handle;
		idx_other = (g_isp_dev_parray[0] == isp_dev) ? 1 : 0;
		t_camerafile = *camerafile;
		t_camerafile.idx = idx_other;
		ret = sprd_img_get_dcam_dev(&t_camerafile, &dev_other,
			&info);
		if (ret) {
			pr_err("fail to get dcam dev\n");
			goto exit;
		}

		dcam_path_resume(dev_other->idx);

		/*Adjust recovery flow*/
		if (unlikely(is_dual_cam_dore & BIT(5))) {
			if (!(dual_cam_cap_sta & BIT(idx_other))) {
				if (dev_other->cap_flag != DCAM_CAPTURE_STOP) {
					pr_info("dcam%d, stop capture, flag %d\n",
						dev_other->idx,
						dev_other->cap_flag);
					ret = sprd_isp_stop_pipeline(
						dev_other->isp_dev_handle);
					if (ret) {
						pr_err("fail to stop offline\n");
						goto exit;
					}
					dev_other->cap_flag = DCAM_CAPTURE_STOP;
				}

				dcam_path_resume(dev->idx);
			}
			dual_cam_cap_sta = 0;
			is_dual_cam_dore = 0;
		}
		/*Adjust recovery flow*/
		if (unlikely((dev->idx == 1) && (is_dual_cam_dore != 0))) {
			if (!(is_dual_cam_dore & BIT(9))) {
				is_dual_cam_dore |= BIT(12);
				ret = wait_for_completion_interruptible(
					&camerafile->grp->dualcam_recovery_com);
			}
			is_dual_cam_dore |= BIT(10);
		}
		/*Adjust recovery flow*/
		if (unlikely((dev->idx == 0) && (is_dual_cam_dore != 0))) {
			is_dual_cam_dore |= BIT(9);
			if ((is_dual_cam_dore & BIT(12))) {
				complete(
				&camerafile->grp->dualcam_recovery_com);
			}
		}

	}

	pr_info("dcam%d, stop capture -\n", dev->idx);

exit:
	return ret;
}

static int dcamio_set_path_skip_num(struct camera_file *camerafile,
				    unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	struct sprd_img_parm parm;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&parm,
			     (void __user *)arg,
			     sizeof(struct sprd_img_parm));
	if (ret) {
		pr_err("fail to get user info\n");
		ret = -EFAULT;
		goto exit;
	}
	info->skip_number = parm.skip_num;
	DCAM_TRACE("path %d skip number %d\n", parm.channel_id,
		   parm.skip_num);

exit:
	return ret;
}

static int dcamio_out_path_size(struct camera_file *camerafile,
				unsigned long arg, uint32_t cmd)
{
	uint32_t ratio = 0;
	int ret = 0;
	struct camera_size size;
	struct camera_size tmp;
	struct camera_info *info;
	struct camera_dev *dev;
	struct sprd_dcam_path_size parm;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		return -EFAULT;
	}
	ret = copy_from_user(&parm, (void __user *)arg,
			sizeof(struct sprd_dcam_path_size));
	if (ret)
		return -EFAULT;

	if ((!parm.dcam_in_w) || (!parm.dcam_in_h)) {
		pr_err("fail to bin ratio, dcam in size %d %d\n",
			parm.dcam_in_w, parm.dcam_in_h);
		return -EFAULT;
	}
	DCAM_TRACE("dcam_out_size: in:%d %d,pre:%d %d,vid:%d %d\n",
		parm.dcam_in_w, parm.dcam_in_h, parm.pre_dst_w,
		parm.pre_dst_h,	parm.vid_dst_w, parm.vid_dst_h);
	/* pre, vid cal binning ratio */
	if (parm.vid_dst_w >= parm.pre_dst_w &&
		parm.vid_dst_h >= parm.pre_dst_h) {
		tmp.w = parm.vid_dst_w;
		tmp.h = parm.vid_dst_h;
	} else {
		tmp.w = parm.pre_dst_w;
		tmp.h = parm.pre_dst_h;
	}
	if ((!tmp.w) || (!tmp.h)) {
		pr_err("fail to bin ratio, dst size %d %d\n",
			tmp.w, tmp.h);
		return -EFAULT;
	}

	size.w = parm.dcam_in_w;
	size.h = parm.dcam_in_h;
	ratio = cal_bin_ratio(size, tmp);
	/* prev vid use the same bin ratio */
	info->dcam_path[CAMERA_PRE_PATH].bin_ratio = ratio;
	info->dcam_path[CAMERA_VID_PATH].bin_ratio = ratio;
	/* calculate dcam out size */
	align_for_bin(&size, ratio);
	parm.dcam_out_w = size.w >> ratio;
	parm.dcam_out_h = size.h >> ratio;
	ret = copy_to_user((void __user *)arg, &parm,
			sizeof(struct sprd_dcam_path_size));
	if (unlikely(ret))
		return -EFAULT;
	pr_debug("dcam out size:%d %d, bin:%d\n", parm.dcam_out_w,
		parm.dcam_out_h, ratio);

	return 0;
}

static int dcamio_set_sensor_max_size(struct camera_file *camerafile,
				      unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	struct sprd_img_size size;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&size, (void __user *)arg,
			     sizeof(struct sprd_img_size));
	if (ret || !size.w || !size.h) {
		pr_err("fail to get right user info\n");
		ret = -EFAULT;
		goto exit;
	}
	info->sn_max_size.w = size.w;
	info->sn_max_size.h = size.h;
	DCAM_TRACE("%d: sensor max size %d %d\n",
		   camerafile->idx,
		   info->sn_max_size.w,
		   info->sn_max_size.h);

exit:
	return ret;
}

static int dcamio_isp_k_ioctl(struct camera_file *camerafile, unsigned long arg,
			      uint32_t cmd)
{
	int ret = 0;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = sprd_isp_k_ioctl(dev->isp_dev_handle, cmd, arg);
	if (ret) {
		pr_err("fail to sprd_img_k_ioctl\n");
		goto exit;
	}

exit:
	return ret;
}

static int dcamio_core_ebd_ctrl(struct camera_file *camerafile,
				unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	struct sprd_img_parm parm;
	struct camera_dev *dev = NULL;
	struct camera_info *info = NULL;
	struct camera_path_spec *path = NULL;

	ret = sprd_img_get_dcam_dev(camerafile, &dev, &info);
	if (ret) {
		pr_err("fail to get dcam dev\n");
		goto exit;
	}
	ret = copy_from_user(&parm, (void __user *)arg,
			     sizeof(struct sprd_img_parm));
	if (unlikely(ret)) {
		pr_err("fail to get user info\n");
		ret = -EFAULT;
		goto exit;
	}
	path = &info->dcam_path[CAMERA_EBD_PATH];
	memcpy(&(path->ebd_ctrl), &(parm.ebd_ctrl),
		sizeof(path->ebd_ctrl));
	if (path->ebd_ctrl.mode != 0)
		path->is_work = 1;
	else
		path->is_work = 0;

exit:
	return ret;
}

static int camioctl_key_set(struct camera_file *camerafile,
                                unsigned long arg, uint32_t cmd)
{
	int ret = 0;
	uint32_t param = 0;
	ret = copy_from_user(&param, (void __user *)arg, sizeof(uint32_t));
	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		return -EFAULT;
	}
	if (param == CAM_IOCTL_PRIVATE_KEY) {
		camerafile->private_key = 1;
	}

	pr_info("cam%d get ioctrl permission %d\n", camerafile->idx, camerafile->private_key);
	return 0;
}

static struct dcam_io_ctrl_fun s_cam_io_ctrl_fun_tab[] = {
	{SPRD_IMG_IO_SET_MODE,			dcamio_set_mode},
	{SPRD_IMG_IO_SET_CAP_SKIP_NUM,		dcamio_set_cap_skip_num},
	{SPRD_IMG_IO_SET_SENSOR_SIZE,		dcamio_set_sensor_size},
	{SPRD_IMG_IO_SET_SENSOR_TRIM,		dcamio_set_sensor_trim},
	{SPRD_IMG_IO_SET_FRM_ID_BASE,		dcamio_set_frame_id_base},
	{SPRD_IMG_IO_SET_CROP,			dcamio_set_crop},
	{SPRD_IMG_IO_SET_FLASH,			dcamio_set_flash},
	{SPRD_IMG_IO_SET_OUTPUT_SIZE,		dcamio_set_output_size},
	{SPRD_IMG_IO_SET_ZOOM_MODE,		dcamio_set_zoom_mode},
	{SPRD_IMG_IO_SET_SENSOR_IF,		dcamio_set_sensor_if},
	{SPRD_IMG_IO_SET_FRAME_ADDR,		dcamio_set_frame_addr},
	{SPRD_IMG_IO_PATH_FRM_DECI,		dcamio_set_path_frame_deci},
	{SPRD_IMG_IO_STREAM_ON,			dcamio_stream_on},
	{SPRD_IMG_IO_STREAM_OFF,		dcamio_stream_off},
	{SPRD_IMG_IO_GET_FMT,			dcamio_get_fmt},
	{SPRD_IMG_IO_GET_CH_ID,			dcamio_get_ch_id},
	{SPRD_IMG_IO_GET_TIME,			dcamio_get_time},
	{SPRD_IMG_IO_CHECK_FMT,			dcamio_check_fmt},
	{SPRD_IMG_IO_SET_SHRINK,		dcamio_set_shrink},
	{SPRD_IMG_IO_CFG_FLASH,			dcamio_cfg_flash},
	{SPRD_IMG_IO_PDAF_CONTROL,		dcamio_pdaf_ctrl},
	{SPRD_IMG_IO_GET_IOMMU_STATUS,		dcamio_get_iommu_status},
	{SPRD_IMG_IO_START_CAPTURE,		dcamio_start_capture},
	{SPRD_IMG_IO_STOP_CAPTURE,		dcamio_stop_capture},
	{SPRD_IMG_IO_SET_PATH_SKIP_NUM,		dcamio_set_path_skip_num},
	{SPRD_IMG_IO_DCAM_PATH_SIZE,		dcamio_out_path_size},
	{SPRD_IMG_IO_SET_SENSOR_MAX_SIZE,	dcamio_set_sensor_max_size},
	{SPRD_ISP_IO_CAPABILITY,		dcamio_isp_k_ioctl},
	{SPRD_ISP_IO_IRQ,			dcamio_isp_k_ioctl},
	{SPRD_ISP_IO_READ,			dcamio_isp_k_ioctl},
	{SPRD_ISP_IO_WRITE,			dcamio_isp_k_ioctl},
	{SPRD_ISP_IO_RST,			dcamio_isp_k_ioctl},
	{SPRD_ISP_IO_STOP,			dcamio_isp_k_ioctl},
	{SPRD_ISP_IO_INT,			dcamio_isp_k_ioctl},
	{SPRD_ISP_IO_SET_STATIS_BUF,		dcamio_set_statis_buf},
	{SPRD_ISP_IO_CFG_PARAM,			dcamio_isp_k_ioctl},
	{SPRD_ISP_REG_READ,			dcamio_isp_k_ioctl},
	{SPRD_STATIS_IO_CFG_PARAM,		dcamio_statis_cfg_param},
	{SPRD_ISP_IO_RAW_CAP,			dcamio_isp_k_ioctl_raw},
	{SPRD_IMG_IO_GET_DCAM_RES,		dcamio_get_dcam_res},
	{SPRD_IMG_IO_PUT_DCAM_RES,		dcamio_put_dcam_res},
	{SPRD_ISP_IO_SET_PULSE_LINE,		dcamio_set_pulse_line},
	{SPRD_ISP_IO_SET_NEXT_VCM_POS,		dcamio_set_next_vcm_pos},
	{SPRD_ISP_IO_SET_VCM_LOG,		dcamio_set_vcm_log},
	{SPRD_IMG_IO_SET_3DNR,			dcamio_set_3dnr},
	{SPRD_ISP_IO_MASK_3A,			dcamio_isp_k_ioctl},
	{SPRD_IMG_IO_EBD_CONTROL,		dcamio_core_ebd_ctrl},
	{SPRD_IMG_IO_SET_KEY,              	camioctl_key_set},
};

static dcam_io_fun dcam_ioctl_get_fun(uint32_t cmd)
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

#endif
