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

#include <linux/compat.h>
#ifdef CAM_IOCTL_LAYER

static int camioctl_time_get(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct sprd_img_time utime;
	timespec ts;

	memset(&ts, 0, sizeof(timespec));
	ktime_get_ts(&ts);
	utime.sec = (uint32_t)ts.tv_sec;
	utime.usec = (uint32_t)(ts.tv_nsec / NSEC_PER_USEC);
	pr_debug("get_time %d.%06d\n", utime.sec, utime.usec);

	ret = copy_to_user((void __user *)arg, &utime,
		sizeof(struct sprd_img_time));
	if (unlikely(ret)) {
		pr_err("fail to put user info, ret %d\n", ret);
		return -EFAULT;
	}

	return 0;
}

static int camioctl_flash_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct sprd_img_set_flash set_param;

	ret = copy_from_user((void *)&set_param,
		(void __user *)arg,
		sizeof(struct sprd_img_set_flash));
	if (ret) {
		pr_err("fail to get user info\n");
		ret = -EFAULT;
		goto exit;
	}

	module->flash_info.led0_ctrl = set_param.led0_ctrl;
	module->flash_info.led1_ctrl = set_param.led1_ctrl;
	module->flash_info.led0_status = set_param.led0_status;
	module->flash_info.led1_status = set_param.led1_status;
	module->flash_info.set_param = set_param;
	pr_info("led_ctrl %d %d led_status %d %d\n", set_param.led0_ctrl, set_param.led1_ctrl,
		set_param.led0_status, set_param.led1_status);

exit:
	return ret;
}

static int camioctl_flash_cfg(struct camera_module *module, unsigned long arg)
{
	int ret = 0;
	struct sprd_flash_cfg_param cfg_parm;

	ret = copy_from_user((void *) &cfg_parm,
		(void __user *)arg,
		sizeof(struct sprd_flash_cfg_param));
	if (ret) {
		pr_err("fail to get user info\n");
		ret = -EFAULT;
		goto exit;
	}

	ret = module->flash_core_handle->flash_core_ops->cfg_flash(module->flash_core_handle,
		(void *)&cfg_parm);

exit:
	return ret;
}

static int camioctl_flash_get(struct camera_module *module, unsigned long arg)
{
	int ret = 0;
	struct sprd_flash_capacity flash_info = {0};

	ret = module->flash_core_handle->flash_core_ops->get_flash(module->flash_core_handle,
		(void *)&flash_info);

	ret = copy_to_user((void __user *)arg, (void *)&flash_info,
		sizeof(struct sprd_flash_capacity));

	return ret;
}

static int camioctl_iommu_status_get(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	unsigned int iommu_enable;
	struct device	*dcam_dev;

	dcam_dev = &module->grp->pdev->dev;
	if (cam_buf_iommu_status_get(CAM_IOMMUDEV_DCAM) == 0)
		iommu_enable = 1;
	else
		iommu_enable = 0;
	module->iommu_enable = iommu_enable;

	ret = copy_to_user((void __user *)arg, &iommu_enable,
		sizeof(unsigned char));

	if (unlikely(ret)) {
		pr_err("fail to copy to user, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}
	pr_info("iommu_enable:%d\n", iommu_enable);
exit:
	return ret;
}

static int camioctl_statis_buf_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct channel_context *ch = NULL;
	struct isp_statis_buf_input statis_buf = {0};
	struct cam_hw_info *hw = module->dcam_dev_handle->hw;
	struct dcam_sw_context *dcam_sw_ctx = NULL;

	ret = copy_from_user((void *)&statis_buf, (void *)arg, sizeof(struct isp_statis_buf_input));
	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

	if ((statis_buf.type == STATIS_INIT)
		&& (atomic_read(&module->state) != CAM_IDLE)
		&& (atomic_read(&module->state) != CAM_CFG_CH)) {
		pr_err("fail to get init statis buf state: %d\n", atomic_read(&module->state));
		ret = -EFAULT;
		goto exit;
	}

	if ((statis_buf.type != STATIS_INIT) &&
		(statis_buf.type < STATIS_TYPE_MAX) &&
		(atomic_read(&module->state) != CAM_RUNNING)) {
		pr_warn("warning: should not configure statis buf for state %d\n",
			atomic_read(&module->state));
		goto exit;
	}

	if (module->aux_dcam_dev && module->dcam_dev_handle->sw_ctx[module->offline_cxt_id].rps)
		dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];
	else
		dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];

	switch (statis_buf.type) {
		case STATIS_INIT:
			ret = module->dcam_dev_handle->dcam_pipe_ops->ioctl(dcam_sw_ctx, DCAM_IOCTL_CFG_STATIS_BUF, &statis_buf);

			if (hw->ip_isp->frbg_hist_support || hw->ip_isp->rgb_gtm_support) {
				ch = &module->channel[CAM_CH_PRE];
				if (!ch->enable && module->simulator)
					ch = &module->channel[CAM_CH_CAP];

				if (ch->enable) {
					ret = module->isp_dev_handle->isp_ops->ioctl(module->isp_dev_handle,
						ch->isp_ctx_id,
						ISP_IOCTL_CFG_STATIS_BUF,
						&statis_buf);
				}
			}
			break;
		case STATIS_AEM:
		case STATIS_AFM:
		case STATIS_AFL:
		case STATIS_HIST:
		case STATIS_PDAF:
		case STATIS_EBD:
		case STATIS_LSCM:
			pr_debug("cam%d type %d mfd %d\n", module->idx, statis_buf.type, statis_buf.mfd);
			if (module->paused) {
				pr_info("cam%d paused, type %d ignored\n", module->idx, statis_buf.type);
				goto exit;
			}
			ret = module->dcam_dev_handle->dcam_pipe_ops->ioctl(dcam_sw_ctx, DCAM_IOCTL_CFG_STATIS_BUF, &statis_buf);
			break;
		case STATIS_GTMHIST:
			if (module->paused) {
				pr_debug("cam%d paused, type %d ignored\n", module->idx, statis_buf.type);
				goto exit;
			}

			if (hw->ip_isp->rgb_gtm_support > 0) {
				/*gtm in isp*/
				ch = &module->channel[CAM_CH_PRE];
				if (!ch->enable) {
					pr_debug("prev channel disable, can not set gtm buf\n");
					break;
				}

				ret = module->isp_dev_handle->isp_ops->ioctl(module->isp_dev_handle,
					ch->isp_ctx_id, ISP_IOCTL_CFG_STATIS_BUF, &statis_buf);
			} else {
				/*gtm in dcam*/
				pr_debug("dcam gtm statis buf cfg\n");
				ret = module->dcam_dev_handle->dcam_pipe_ops->ioctl(dcam_sw_ctx, DCAM_IOCTL_CFG_STATIS_BUF, &statis_buf);
			}
			break;
		case STATIS_HIST2:
			if (hw->ip_isp->frbg_hist_support) {
				ch = &module->channel[CAM_CH_PRE];
				if (!ch->enable && module->simulator)
					ch = &module->channel[CAM_CH_CAP];

				if (ch->enable) {
					ret = module->isp_dev_handle->isp_ops->ioctl(module->isp_dev_handle,
					ch->isp_ctx_id,
					ISP_IOCTL_CFG_STATIS_BUF,
					&statis_buf);
				}
			} else
				ret = module->dcam_dev_handle->dcam_pipe_ops->ioctl(dcam_sw_ctx, DCAM_IOCTL_CFG_STATIS_BUF, &statis_buf);
			break;
		case STATIS_DBG_INIT:
		case STATIS_PARAM:
			ret = camcore_param_buffer_cfg(module, &statis_buf);
			break;
		default:
			break;
	}

exit:
	return ret;
}

static int camioctl_param_cfg(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	int for_capture = 0, for_fdr = 0;
	int32_t isp_ctx_id = 0;
	u32 property_param = 0;
	struct channel_context *channel = NULL;
	struct isp_io_param param = {0};
	struct compat_isp_io_param __user *uparam = NULL;
	struct dcam_pipe_dev *dev = NULL;
	struct dcam_sw_context *dcam_aux_ctx = NULL;
	uint32_t is_rps = 0;

	if (module->compat_flag) {
		uparam = (struct compat_isp_io_param __user *)arg;
		ret |= get_user(param.scene_id, &uparam->scene_id);
		ret |= get_user(param.sub_block, &uparam->sub_block);
		ret |= get_user(param.property, &uparam->property);
		ret |= get_user(property_param, &uparam->property_param);
		param.property_param = compat_ptr(property_param);
	} else {
		ret = copy_from_user((void *)&param, (void *)arg, sizeof(struct isp_io_param));
	}

	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

	if (atomic_read(&module->state) == CAM_INIT || param.property_param == NULL
		|| module->dcam_dev_handle == NULL || module->isp_dev_handle == NULL) {
		pr_err("fail to get user param. state %d\n", atomic_read(&module->state));
		ret = -EFAULT;
		goto exit;
	}

	dev = (struct dcam_pipe_dev *)module->dcam_dev_handle;
	dcam_aux_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];

	if (dev && dev->sw_ctx[module->cur_sw_ctx_id].raw_fetch_count > 0) {
		pr_warn("warning: raw fetch in progress!\n");
		return 0;
	}
	if (module->aux_dcam_dev && module->dcam_dev_handle->sw_ctx[module->offline_cxt_id].rps)
		is_rps= 1;

	if ((param.scene_id == PM_SCENE_FDRL) ||
		(param.scene_id == PM_SCENE_FDRH) ||
		(param.scene_id == PM_SCENE_FDR_PRE) ||
		(param.scene_id == PM_SCENE_FDR_DRC) ||
		(param.scene_id == PM_SCENE_OFFLINE_CAP) ||
		(param.scene_id == PM_SCENE_OFFLINE_BPC) ||
		(param.scene_id == PM_SCENE_SFNR))
		for_fdr = 1;
	for_capture = (param.scene_id == PM_SCENE_CAP ? 1 : 0) | for_fdr;

	if (for_capture && (module->channel[CAM_CH_CAP].enable == 0)) {
		pr_warn("warning: ch scene_id[%d] ch_cap en[%d] ch_pre en[%d]\n",
		param.scene_id,
		module->channel[CAM_CH_CAP].enable,
		module->channel[CAM_CH_PRE].enable);

		return 0;
	}

	if (atomic_read(&module->state) == CAM_STREAM_OFF)
		return 0;

	if ((param.scene_id == PM_SCENE_FDRL) ||
		(param.scene_id == PM_SCENE_FDRH) ||
		(param.scene_id == PM_SCENE_FDR_PRE) ||
		(param.scene_id == PM_SCENE_OFFLINE_BPC) ||
		(param.scene_id == PM_SCENE_OFFLINE_CAP) ||
		(param.scene_id == PM_SCENE_FDR_DRC) ||
		(param.scene_id == PM_SCENE_SFNR)) {
		pr_info("cam%d, cfg FDR scene %d,  blk 0x%x, aux_dcam %pm  dcam swctx %d\n",
			module->idx, param.scene_id, param.sub_block,
			module->aux_dcam_dev, module->offline_cxt_id);
	}

	if ((param.sub_block & DCAM_ISP_BLOCK_MASK) == DCAM_BLOCK_BASE) {
		if (dev && dev->hw->ip_isp->rgb_gtm_support && (param.sub_block == DCAM_BLOCK_GTM)) {
			/*only for l6*/
			if (param.scene_id == PM_SCENE_PRE)
				channel = &module->channel[CAM_CH_PRE];
			else
				channel = &module->channel[CAM_CH_CAP];

			param.sub_block = ISP_BLOCK_RGB_GTM;

			pr_debug("Config gtm block chn en%d, ctx_id %d\n", channel->enable, channel->isp_ctx_id);
			if (channel->enable && (channel->isp_ctx_id >= 0)) {
				if (param.property == DCAM_PRO_GTM_BYPASS) {
					/* gtm bypass sync with frm in dcam ctx */
					if ((for_capture && (module->aux_dcam_dev != NULL)) || is_rps)
						pr_warn("not support offline ctx now\n");
					else
						ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_blk_param(
							&module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id], &param);
				} else {
					isp_ctx_id = channel->isp_ctx_id;
					ret = module->isp_dev_handle->isp_ops->cfg_blk_param(module->isp_dev_handle,
						isp_ctx_id, &param);
				}
				if (ret)
					pr_err("fail to set block %d, property %d\n", param.sub_block, param.property);
			}
			goto exit;
		}
		if (for_capture && (module->aux_dcam_dev == NULL))
			pr_debug("Config DCAM param for capture. Maybe raw proc\n");

		if (((for_capture && (module->aux_dcam_dev != NULL)) || is_rps) && (!module->cam_uinfo.virtualsensor)) {
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_blk_param(
				&module->dcam_dev_handle->sw_ctx[module->offline_cxt_id], &param);
		} else {
			if (module->paused) {
				pr_info("cam%d paused, block %x\n", module->idx, param.sub_block);
				return 0;
			}
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_blk_param(
				&module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id], &param);
		}
	} else {
		if (param.scene_id == PM_SCENE_PRE)
			channel = &module->channel[CAM_CH_PRE];
		else
			channel = &module->channel[CAM_CH_CAP];

		if (channel->enable && channel->isp_ctx_id >= 0) {
			isp_ctx_id = channel->isp_ctx_id;
			if (param.scene_id == PM_SCENE_FDRL) {
				isp_ctx_id = channel->isp_fdrl_ctx;
				if (module->cam_uinfo.raw_alg_type != RAW_ALG_FDR_V1)
					return 0;
			} else if (param.scene_id == PM_SCENE_FDRH || param.scene_id == PM_SCENE_FDR_DRC ||
				param.scene_id == PM_SCENE_OFFLINE_CAP || param.scene_id == PM_SCENE_SFNR)
				isp_ctx_id = channel->isp_fdrh_ctx;
			if (((module->grp->hw_info->prj_id == QOGIRN6pro || module->grp->hw_info->prj_id == QOGIRN6L)
				&& (param.sub_block == ISP_BLOCK_GAMMA || param.sub_block == ISP_BLOCK_CMC ||
				param.sub_block == ISP_BLOCK_CFA || param.sub_block == ISP_BLOCK_NLM ||
				param.sub_block == ISP_BLOCK_CCE || param.sub_block == ISP_BLOCK_HIST2)) ||
				(param.sub_block == ISP_BLOCK_RGB_LTM && param.property == ISP_PRO_RGB_LTM_BYPASS)) {
				if ((for_capture && (module->aux_dcam_dev != NULL)) || is_rps) {
					ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_blk_param(
						&module->dcam_dev_handle->sw_ctx[module->offline_cxt_id], &param);
				} else {
					if (module->paused) {
						pr_warn("warning: cam%d paused, block %x\n", module->idx, param.sub_block);
						return 0;
					}
					ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_blk_param(
						&module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id], &param);
					if (ret)
						pr_err("fail to set block %d\n", param.sub_block);
				}

			} else
				ret = module->isp_dev_handle->isp_ops->cfg_blk_param(module->isp_dev_handle,
					isp_ctx_id, &param);
		}
	}

exit:
	return ret;
}

static int camioctl_function_mode_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct sprd_img_function_mode __user *uparam;
	struct cam_hw_info *hw = NULL;

	uparam = (struct sprd_img_function_mode __user *)arg;
	hw = module->grp->hw_info;
	ret |= get_user(module->cam_uinfo.is_4in1, &uparam->need_4in1);
	ret |= get_user(module->cam_uinfo.is_dual, &uparam->dual_cam);
	ret |= get_user(module->cam_uinfo.is_raw_alg, &uparam->is_raw_alg);
	ret |= get_user(module->cam_uinfo.param_frame_sync, &uparam->param_frame_sync);
	ret |= get_user(module->cam_uinfo.raw_alg_type, &uparam->raw_alg_type);
	ret |= get_user(module->cam_uinfo.fdr_cap_pre_num, &uparam->fdr_preview_captured_num);
	ret |= get_user(module->cam_uinfo.zoom_conflict_with_ltm, &uparam->zoom_conflict_with_ltm);
	ret |= get_user(module->cam_uinfo.need_dcam_raw, &uparam->need_dcam_raw);
	ret |= get_user(module->cam_uinfo.virtualsensor, &uparam->virtualsensor);
	ret |= get_user(module->master_flag, &uparam->master_flag);

	module->cam_uinfo.is_rgb_ltm = hw->ip_isp->rgb_ltm_support;
	module->cam_uinfo.is_yuv_ltm = hw->ip_isp->yuv_ltm_support;
	module->cam_uinfo.is_rgb_gtm = hw->ip_isp->rgb_gtm_support;
	if (g_pyr_dec_offline_bypass || module->channel[CAM_CH_CAP].ch_uinfo.is_high_fps
		|| module->cam_uinfo.sn_size.w > DCAM_64M_WIDTH)
		module->cam_uinfo.is_pyr_dec = 0;
	else
		module->cam_uinfo.is_pyr_dec = hw->ip_isp->pyr_dec_support;

	pr_info("4in1:[%d], rgb_ltm[%d], yuv_ltm[%d], gtm[%d], dual[%d], dec %d, raw_alg_type:%d, zoom_conflict_with_ltm %d, %d. dcam_raw %d, param_frame_sync:%d,virtualsensor %d, master_flag:%d\n",
		module->cam_uinfo.is_4in1,module->cam_uinfo.is_rgb_ltm,
		module->cam_uinfo.is_yuv_ltm, module->cam_uinfo.is_rgb_gtm,
		module->cam_uinfo.is_dual, module->cam_uinfo.is_pyr_dec,
		module->cam_uinfo.raw_alg_type,
		module->cam_uinfo.zoom_conflict_with_ltm,
		module->cam_uinfo.is_raw_alg,
		module->cam_uinfo.need_dcam_raw,
		module->cam_uinfo.param_frame_sync,
		module->cam_uinfo.virtualsensor,
		module->master_flag);

	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

exit:
	return ret;
}

static int camioctl_mode_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;

	ret = copy_from_user(&module->cam_uinfo.capture_mode,
		(void __user *)arg,
		sizeof(uint32_t));

	pr_info("mode %d\n", module->cam_uinfo.capture_mode);
	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

exit:
	return ret;
}

static int camioctl_cam_security_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	bool sec_ret = 0;
	bool vaor_bp_en = 0;
	bool ca_conn = 0;
	struct sprd_cam_sec_cfg uparam;

	ret = copy_from_user(&uparam,
		(void __user *)arg,
		sizeof(struct sprd_cam_sec_cfg));

	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

	if (!module->isp_dev_handle) {
		pr_err("fail to isp dev_hanlde is NULL\n");
		ret = -EFAULT;
		goto exit;
	}

	if (atomic_read(&module->state) == CAM_INIT) {
		pr_err("cam%d error state: %d\n", module->idx,
			atomic_read(&module->state));
		return -EFAULT;
	}

	pr_info("camca : conn = %d, security mode %d,  u camsec_mode=%d, u work_mode=%d\n",
		module->grp->ca_conn,
		module->grp->camsec_cfg.camsec_mode,
		uparam.camsec_mode, uparam.work_mode);

	if (uparam.camsec_mode == SEC_UNABLE) {
		vaor_bp_en = false;
	}  else {
		if (!module->grp->ca_conn) {
			ca_conn = cam_trusty_connect();
			if (!ca_conn) {
				pr_err("fail to init cam_trusty_connect\n");
				ret = -EINVAL;
				goto exit;
			}
			module->grp->ca_conn = ca_conn;
		}

		sec_ret = cam_trusty_security_set(&uparam, CAM_TRUSTY_ENTER);
		if (!sec_ret) {
			ret = -EINVAL;
			pr_err("fail to init cam security set\n");
			goto exit;
		}
		vaor_bp_en = true;
	}

	module->grp->camsec_cfg.work_mode = uparam.work_mode;
	module->grp->camsec_cfg.camsec_mode = uparam.camsec_mode;
	if (atomic_read(&module->isp_dev_handle->pd_clk_rdy) > 0)
		ret = sprd_iommu_set_cam_bypass(vaor_bp_en);

exit:
	return ret;
}

static int camioctl_sensor_if_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct sprd_img_sensor_if *dst;

	dst = &module->cam_uinfo.sensor_if;

	ret = copy_from_user(dst,
		(void __user *)arg,
		sizeof(struct sprd_img_sensor_if));
	pr_info("sensor_if %d %x %x, %d.....mipi %d %d %d %d\n",
		dst->if_type, dst->img_fmt, dst->img_ptn, dst->frm_deci,
		dst->if_spec.mipi.use_href, dst->if_spec.mipi.bits_per_pxl,
		dst->if_spec.mipi.is_loose, dst->if_spec.mipi.lane_num);

	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

exit:
	return ret;
}

static int camioctl_sensor_size_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct sprd_img_size *dst;

	dst = &module->cam_uinfo.sn_size;

	ret = copy_from_user(dst, (void __user *)arg, sizeof(struct sprd_img_size));

	pr_info("cam%d, sensor_size %d %d\n", module->idx, dst->w, dst->h);
	module->cam_uinfo.dcam_slice_mode = dst->w > DCAM_24M_WIDTH ? CAM_OFFLINE_SLICE_HW : 0;
	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

exit:
	return ret;
}

static int camioctl_sensor_trim_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct sprd_img_rect *dst;

	dst = &module->cam_uinfo.sn_rect;

	ret = copy_from_user(dst,
		(void __user *)arg,
		sizeof(struct sprd_img_rect));
	pr_info("sensor_trim %d %d %d %d\n", dst->x, dst->y, dst->w, dst->h);
	/* make sure MIPI CAP size is 4 pixel aligned */
	if (unlikely(ret || (dst->w | dst->h) & (DCAM_MIPI_CAP_ALIGN - 1))) {
		pr_err("fail to copy from user, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

exit:
	return ret;
}

static int camioctl_sensor_max_size_set(
		struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct sprd_img_size *dst;

	dst = &module->cam_uinfo.sn_max_size;

	ret = copy_from_user(dst,
		(void __user *)arg,
		sizeof(struct sprd_img_size));
	pr_info("sensor_max_size %d %d\n", dst->w, dst->h);

	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

exit:
	return ret;
}

static int camioctl_cap_skip_num_set(	struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	uint32_t *dst;

	dst = &module->cam_uinfo.capture_skip;

	ret = copy_from_user(dst,
		(void __user *)arg,
		sizeof(uint32_t));
	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		return -EFAULT;
	}

	pr_debug("set cap skip frame %d\n", *dst);
	return 0;
}

static int camioctl_output_size_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	uint32_t scene_mode;
	uint32_t cap_type;
	uint32_t sn_fmt, dst_fmt;
	struct channel_context *channel = NULL;
	struct camera_uchannel *dst;
	struct sprd_img_parm __user *uparam;

	module->last_channel_id = CAM_CH_MAX;
	uparam = (struct sprd_img_parm __user *)arg;

	ret |= get_user(scene_mode, &uparam->scene_mode);
	ret |= get_user(cap_type, &uparam->need_isp_tool);
	ret |= get_user(sn_fmt, &uparam->sn_fmt);
	ret |= get_user(dst_fmt, &uparam->pixel_fmt);

	pr_info("cam%d, scene %d  cap_type %d, fmt %x %x\n",
		module->idx, scene_mode, cap_type, sn_fmt, dst_fmt);
	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		goto exit;
	}

	if ((dst_fmt == IMG_PIX_FMT_GREY) && (cap_type != CAM_CAP_RAW_FULL) && (scene_mode != DCAM_SCENE_MODE_SENSOR_RAW))
		module->raw_callback = 1;
	else
		module->raw_callback = 0;
	pr_debug("raw_callback=%d\n", module->raw_callback);

	if(scene_mode == DCAM_SCENE_MODE_HARDWARE_SIMULATION)
		module->simulator = 1;
	pr_info("cam%d, simulator=%d\n", module->idx, module->simulator);

	if ((scene_mode == DCAM_SCENE_MODE_HARDWARE_SIMULATION) &&
		(module->channel[CAM_CH_RAW].enable == 0)) {
		channel = &module->channel[CAM_CH_RAW];
		channel->enable = 1;
	} else if ((scene_mode == DCAM_SCENE_MODE_SENSOR_RAW) &&
		(module->channel[CAM_CH_DCAM_VCH].enable == 0)) {
		channel = &module->channel[CAM_CH_DCAM_VCH];
		channel->enable = 1;
	} else if (((cap_type == CAM_CAP_RAW_FULL) || (dst_fmt == sn_fmt)) &&
		(module->channel[CAM_CH_RAW].enable == 0)) {
		channel = &module->channel[CAM_CH_RAW];
		channel->enable = 1;
	} else if ((scene_mode == DCAM_SCENE_MODE_PREVIEW) &&
		(module->channel[CAM_CH_PRE].enable == 0)) {
		channel = &module->channel[CAM_CH_PRE];
		channel->enable = 1;
	} else if (((scene_mode == DCAM_SCENE_MODE_RECORDING) ||
			(scene_mode == DCAM_SCENE_MODE_CAPTURE_CALLBACK)) &&
			(module->channel[CAM_CH_VID].enable == 0)) {
		channel = &module->channel[CAM_CH_VID];
		channel->enable = 1;
	} else if ((scene_mode == DCAM_SCENE_MODE_CAPTURE) &&
		(module->channel[CAM_CH_CAP].enable == 0)) {
		channel = &module->channel[CAM_CH_CAP];
		channel->enable = 1;
	} else if ((scene_mode == DCAM_SCENE_MODE_CAPTURE_THUMB) &&
		(module->channel[CAM_CH_CAP_THM].enable == 0)) {
		channel = &module->channel[CAM_CH_CAP_THM];
		channel->enable = 1;
	} else if ((scene_mode == DCAM_SCENE_MODE_VIRTUAL) &&
		(module->channel[CAM_CH_VIRTUAL].enable == 0)) {
		channel = &module->channel[CAM_CH_VIRTUAL];
		channel->enable = 1;
	} else if (module->channel[CAM_CH_PRE].enable == 0) {
		channel = &module->channel[CAM_CH_PRE];
		channel->enable = 1;
	} else if (module->channel[CAM_CH_CAP].enable == 0) {
		channel = &module->channel[CAM_CH_CAP];
		channel->enable = 1;
	}

	if (channel == NULL) {
		pr_err("fail to get valid channel\n");
		ret = -EINVAL;
		goto exit;
	}

	module->last_channel_id = channel->ch_id;
	channel->dcam_path_id = -1;
	channel->aux_dcam_path_id = -1;
	channel->isp_ctx_id = -1;
	channel->isp_path_id = -1;
	channel->isp_fdrh_path = -1;
	channel->isp_fdrl_path = -1;
	channel->isp_fdrh_ctx = -1;
	channel->isp_fdrl_ctx = -1;
	channel->slave_isp_ctx_id = -1;
	channel->slave_isp_path_id = -1;

	dst = &channel->ch_uinfo;
	dst->dcam_raw_fmt = -1;
	dst->sensor_raw_fmt = -1;
	dst->sn_fmt = sn_fmt;
	dst->dst_fmt = dst_fmt;
	ret |= get_user(dst->is_high_fps, &uparam->is_high_fps);
	ret |= get_user(dst->high_fps_skip_num, &uparam->high_fps_skip_num);

	if (!dst->is_high_fps)
		dst->high_fps_skip_num = 0;

	if (dst->high_fps_skip_num == 1) {
		pr_err("fail to get valid high fps %u\n", dst->high_fps_skip_num);
		ret = -EINVAL;
	}
	ret |= copy_from_user(&dst->zoom_ratio_base,
		&uparam->base_rect, sizeof(struct sprd_img_rect));
	ret |= copy_from_user(&dst->src_crop,
			&uparam->crop_rect, sizeof(struct sprd_img_rect));
	ret |= copy_from_user(&dst->dst_size,
			&uparam->dst_size, sizeof(struct sprd_img_size));
	ret |= get_user(dst->slave_img_en, &uparam->aux_img.enable);
	ret |= get_user(dst->slave_img_fmt, &uparam->aux_img.pixel_fmt);
	ret |= copy_from_user(&dst->slave_img_size,
		&uparam->aux_img.dst_size, sizeof(struct sprd_img_size));
	ret |= get_user(dst->frame_sync_close, &uparam->reserved[3]);

	// TODO get this from HAL
	dst->is_compressed = 0;
	dst->scene = scene_mode;
	if (cap_type == CAM_CAP_RAW_FULL && dst->is_high_fps)
		dst->is_high_fps = 0;

	if (channel->ch_id == CAM_CH_VIRTUAL) {
		ret |= get_user(dst->vir_channel[0].dump_isp_out_fmt, &uparam->vir_ch_info[0].dump_isp_out_fmt);
		ret |= copy_from_user(&dst->vir_channel[0].dst_size,
			&uparam->vir_ch_info[0].dst_size, sizeof(struct sprd_img_size));
		ret |= get_user(dst->vir_channel[1].dump_isp_out_fmt, &uparam->vir_ch_info[1].dump_isp_out_fmt);
		ret |= copy_from_user(&dst->vir_channel[1].dst_size,
			&uparam->vir_ch_info[1].dst_size, sizeof(struct sprd_img_size));
		pr_debug("cam_virtual_pre_channel: dst %d %d\n",dst->vir_channel[0].dst_size.w,dst->vir_channel[0].dst_size.h);
		pr_debug("cam_virtual_cap_channel: dst %d %d\n",dst->vir_channel[1].dst_size.w,dst->vir_channel[1].dst_size.h);
	}

	/* for AF zoom_ratio cal*/
	camcore_crop_size_align(module, &dst->zoom_ratio_base);

	pr_info("cam_channel: ch_id %d high fps %u %u. aux %d %d %d %d\n",
		channel->ch_id, dst->is_high_fps, dst->high_fps_skip_num,
		dst->slave_img_en, dst->slave_img_fmt,
		dst->slave_img_size.w, dst->slave_img_size.h);
	pr_info("cam_channel: zoom ratio base %d %d %d %d dst %d %d\n",
		dst->zoom_ratio_base.x,dst->zoom_ratio_base.y,
		dst->zoom_ratio_base.w,dst->zoom_ratio_base.h,
		dst->dst_size.w, dst->dst_size.h);

exit:
	return ret;
}

static int camioctl_ch_id_get(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;

	if ((atomic_read(&module->state) != CAM_IDLE) &&
		(atomic_read(&module->state) != CAM_CFG_CH)) {
		pr_err("fail to get correct state, state %d\n",
			atomic_read(&module->state));
		return -EFAULT;
	}

	if (module->last_channel_id >= CAM_CH_MAX) {
		ret = -EINVAL;
		goto exit;
	}
	pr_info("cam_channel: get ch id: %d\n", module->last_channel_id);

	ret = copy_to_user((void __user *)arg, &module->last_channel_id,
		sizeof(uint32_t));
	if (unlikely(ret))
		pr_err("fail to copy to user. ret %d\n", ret);

exit:
	/* todo: error handling. */
	atomic_set(&module->state, CAM_CFG_CH);
	return ret;
}

static int camioctl_dcam_path_size(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct sprd_dcam_path_size param;

	ret = copy_from_user(
		&param, (void __user *)arg,
		sizeof(struct sprd_dcam_path_size));
	param.dcam_out_w = param.dcam_in_w;
	param.dcam_out_h = param.dcam_in_h;

	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

	if (module->simulator)
		camcore_channels_set(module, &param);

	if (atomic_read(&module->timeout_flag) == 1)
		pr_info("cam%d, in %d  %d. pre %d %d, vid %d, %d, out %d %d\n",
			module->idx, param.dcam_in_w, param.dcam_in_h,
			param.pre_dst_w, param.pre_dst_h,
			param.vid_dst_w, param.vid_dst_h,
			param.dcam_out_w, param.dcam_out_h);

	ret = copy_to_user((void __user *)arg, &param,
		sizeof(struct sprd_dcam_path_size));

exit:
	return ret;
}

static int camioctl_shrink_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	uint32_t channel_id;
	struct sprd_img_parm __user *uparam;

	if (atomic_read(&module->state) != CAM_CFG_CH) {
		pr_debug("skip\n");
		return ret;
	}

	uparam = (struct sprd_img_parm __user *)arg;

	ret = get_user(channel_id, &uparam->channel_id);
	if (ret == 0 && channel_id < CAM_CH_MAX) {
		ret = copy_from_user(
			&module->channel[channel_id].ch_uinfo.regular_desc,
			(void __user *)&uparam->regular_desc,
			sizeof(struct dcam_regular_desc));
	}

	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

exit:
	return ret;
}

 static int camioctl_ebd_control(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct sprd_ebd_control ebd_tmp;
	uint32_t channel_id;
	struct sprd_img_parm __user *uparam;

	if (atomic_read(&module->state) != CAM_CFG_CH) {
		pr_debug("skip\n");
		return ret;
	}

	uparam = (struct sprd_img_parm __user *)arg;
	ret = get_user(channel_id, &uparam->channel_id);
	if (ret || (channel_id != CAM_CH_PRE))
		return 0;

	ret = copy_from_user(&ebd_tmp, &uparam->ebd_ctrl,
		sizeof(struct sprd_ebd_control));
	if (unlikely(ret)) {
		pr_err("fail to copy pdaf param from user, ret %d\n", ret);
		return -EFAULT;
	}

	pr_info("MODE: %d, VC:%d, DT:%d\n", ebd_tmp.mode,
		ebd_tmp.image_vc, ebd_tmp.image_dt);

	ret = module->dcam_dev_handle->dcam_pipe_ops->ioctl(&module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id],
		DCAM_IOCTL_CFG_EBD, &ebd_tmp);

	return ret;
}

static int camioctl_zoom_mode_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;

	ret = copy_from_user(&module->is_smooth_zoom,
		(void __user *)arg,
		sizeof(uint32_t));

	pr_info("is_smooth_zoom %d\n", module->is_smooth_zoom);
	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

exit:
	return ret;
}

static int camioctl_crop_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0, zoom = 0;
	uint32_t channel_id;
	struct img_size max;
	struct channel_context *ch, *ch_vid;
	struct sprd_img_rect *crop;
	struct sprd_img_rect *total_crop;
	struct camera_frame *first = NULL;
	struct camera_frame *zoom_param = NULL;
	struct sprd_img_parm __user *uparam;

	if ((atomic_read(&module->state) != CAM_CFG_CH) &&
		(atomic_read(&module->state) != CAM_RUNNING)) {
		pr_warn("warning: module state: %d\n", atomic_read(&module->state));
		return 0;
	}

	uparam = (struct sprd_img_parm __user *)arg;

	ret = get_user(channel_id, &uparam->channel_id);
	if (ret || (channel_id >= CAM_CH_MAX)) {
		pr_err("fail to set crop, ret %d, ch %d\n", ret, channel_id);
		ret = -EINVAL;
		goto exit;
	}

	ch = &module->channel[channel_id];
	ch_vid = &module->channel[CAM_CH_VID];
	if (!ch->enable) {
		pr_err("fail to set crop, ret %d, ch %d\n", ret, channel_id);
		ret = -EINVAL;
		goto exit;
	}

	/* CAM_RUNNING: for zoom update
	 * only crop rect size can be re-configured during zoom
	 * and it is forbidden during capture.
	 */
	if (atomic_read(&module->state) == CAM_RUNNING) {
		if ((module->cap_status == CAM_CAPTURE_START) &&
			module->channel[CAM_CH_CAP].enable &&
			(module->channel[CAM_CH_CAP].type_3dnr != CAM_3DNR_OFF)) {
			pr_err("fail to zoom during 3DNR capture\n");
			goto exit;
		}
		zoom_param = cam_queue_empty_frame_get();
		crop = &zoom_param->zoom_crop;
		total_crop = &zoom_param->total_zoom_crop;
		zoom = 1;
	} else {
		crop = &ch->ch_uinfo.src_crop;
		total_crop = &ch->ch_uinfo.total_src_crop;
	}

	ret = copy_from_user(crop,
		(void __user *)&uparam->crop_rect,
		sizeof(struct sprd_img_rect));
	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		goto exit;
	}

	ret = copy_from_user(total_crop,
		(void __user *)&uparam->total_rect,
		sizeof(struct sprd_img_rect));
	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		goto exit;
	}

	max.w = module->cam_uinfo.sn_rect.w;
	max.h = module->cam_uinfo.sn_rect.h;

	pr_info("cam%d 4in1[%d],set ch%d crop %d %d %d %d.\n",
		module->idx, module->cam_uinfo.is_4in1, channel_id,
		crop->x, crop->y, crop->w, crop->h);
	/* 4in1 prev, enable 4in1 binning OR size > 24M, size/2 */
	if ((module->cam_uinfo.is_4in1 || module->cam_uinfo.dcam_slice_mode) &&
		((channel_id == CAM_CH_PRE) || (channel_id == CAM_CH_VID))) {
		crop->x >>= 1;
		crop->y >>= 1;
		crop->w >>= 1;
		crop->h >>= 1;
		max.w >>= 1;
		max.h >>= 1;
	}
	camcore_crop_size_align(module, crop);

	if (zoom) {
		if (cam_queue_enqueue(&ch->zoom_coeff_queue, &zoom_param->list)) {
			/* if zoom queue overflow, discard first one node in queue*/
			pr_warn("warning: ch %d zoom q overflow\n", channel_id);
			first = cam_queue_dequeue(&ch->zoom_coeff_queue,
				struct camera_frame, list);
			if (first) {
				cam_queue_empty_frame_put(first);
			}
			cam_queue_enqueue(&ch->zoom_coeff_queue, &zoom_param->list);
		}
		zoom_param = NULL;

		if (ch_vid->enable && channel_id == CAM_CH_PRE) {
			pr_debug("ch_vid->enable %d channel_id %d\n", ch_vid->enable, channel_id);
			goto exit;
		}
		complete(&module->zoom_thrd.thread_com);
	}
exit:
	if (zoom_param) {
		cam_queue_empty_frame_put(zoom_param);
	}
	return ret;
}

static int camioctl_fmt_get(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct sprd_img_get_fmt fmt_desc;
	struct camera_format *fmt = NULL;

	ret = copy_from_user(&fmt_desc, (void __user *)arg,
		sizeof(struct sprd_img_get_fmt));
	if (unlikely(ret)) {
		pr_err("fail to copy from user ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}
	if (unlikely(fmt_desc.index >= ARRAY_SIZE(output_img_fmt))) {
		pr_err("fail to get valid index > arrar size\n");
		ret = -EINVAL;
		goto exit;
	}

	fmt = &output_img_fmt[fmt_desc.index];
	fmt_desc.fmt = fmt->fourcc;

	ret = copy_to_user((void __user *)arg,
		&fmt_desc,
		sizeof(struct sprd_img_get_fmt));
	if (unlikely(ret)) {
		pr_err("fail to put user info, GET_FMT, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}
exit:
	return ret;
}

static int camioctl_fmt_check(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	enum cam_ch_id channel_id;
	struct sprd_img_format img_format;
	struct channel_context *channel;

	if (atomic_read(&module->state) != CAM_CFG_CH) {
		pr_debug("skip\n");
		return ret;
	}

	pr_debug("check fmt\n");
	ret = copy_from_user(&img_format,
		(void __user *)arg,
		sizeof(struct sprd_img_format));
	if (ret) {
		pr_err("fail to get img_format\n");
		return -EFAULT;
	}

	if ((atomic_read(&module->state) != CAM_CFG_CH) &&
		(atomic_read(&module->state) != CAM_RUNNING)) {
		pr_err("fail to get module state: %d\n", atomic_read(&module->state));
		return -EFAULT;
	}

	channel_id = (enum cam_ch_id)img_format.channel_id;
	channel = &module->channel[channel_id];

	if (atomic_read(&module->state) == CAM_CFG_CH) {
		/* to do: check & set channel format / param before stream on */
		pr_info("chk_fmt ch %d\n", channel_id);

		ret = camcore_channel_init(module, channel);
		if (ret) {
			/* todo: error handling. */
			pr_err("fail to init channel %d\n", channel->ch_id);
			goto exit;
		}
	}

	img_format.need_binning = 0;
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

/*
 * SPRD_IMG_IO_PATH_FRM_DECI
 *
 * Set frame deci factor for each channel, which controls the number of dropped
 * frames in ISP output path. It's typically used in slow motion scene. There're
 * two situations in slow motion: preview and recording. HAL will set related
 * parameters according to the table below:
 *
 * ===================================================================================
 * |     scene     |  channel  |  is_high_fps  |  high_fps_skip_num  |  deci_factor  |
 * |---------------|-----------|---------------|---------------------|---------------|
 * |    normal     |  preview  |       0       |          0          |       0       |
 * |    preview    |           |               |                     |               |
 * |---------------|-----------|---------------|---------------------|---------------|
 * |  slow motion  |  preview  |       1       |          4          |       3       |
 * |    preview    |           |               |                     |               |
 * |---------------|-----------|---------------|---------------------|---------------|
 * |               |  preview  |       1       |          4          |       3       |
 * |  slow motion  |           |               |                     |               |
 * |   recording   |-----------|---------------|---------------------|---------------|
 * |               |   video   |       1       |          4          |       0       |
 * |               |           |               |                     |               |
 * ===================================================================================
 *
 * Here, is_high_fps means sensor is running at a high frame rate, thus DCAM
 * slow motion function should be enabled. And deci_factor controls how many
 * frames will be dropped by ISP path before DONE interrupt generated. The
 * high_fps_skip_num is responsible for keeping SOF interrupt running at 30
 * frame rate.
 */
static int camioctl_frm_deci_set(struct camera_module *module,
		unsigned long arg)
{
	struct sprd_img_parm __user *uparam = NULL;
	struct channel_context *ch = NULL;
	uint32_t deci_factor = 0, channel_id = 0;
	int ret = 0;

	if ((atomic_read(&module->state) != CAM_CFG_CH) &&
		(atomic_read(&module->state) != CAM_RUNNING)) {
		pr_warn("warning: only for state CFG_CH or RUNNING\n");
		return 0;
	}

	uparam = (struct sprd_img_parm __user *)arg;
	ret |= get_user(channel_id, &uparam->channel_id);
	ret |= get_user(deci_factor, &uparam->deci);
	if (ret) {
		pr_err("fail to get from user. ret %d\n", ret);
		return -EFAULT;
	}

	if ((channel_id >= CAM_CH_MAX) ||
		(module->channel[channel_id].enable == 0)) {
		pr_err("fail to get valid channel id %d\n", channel_id);
		return -EPERM;
	}

	ch = &module->channel[channel_id];
	ch->ch_uinfo.deci_factor = deci_factor;

	return ret;
}

static int camioctl_frame_addr_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	uint32_t sec = 0;
	uint32_t i = 0, cmd = ISP_PATH_CFG_OUTPUT_BUF;
	uint32_t dump_type[IMG_PATH_BUFFER_COUNT] = {0};
	struct sprd_img_parm __user *uparam;
	struct channel_context *ch = NULL;
	struct channel_context *ch_prv = NULL;
	struct camera_frame *pframe = NULL;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	struct dcam_sw_context *dcam_sw_aux_ctx = NULL;
	struct cam_hw_info *hw = NULL;
	struct secondary_buffer_info secondary_buf_info = {0};
	uint32_t channel_id = 0, buffer_count = 0, is_reserved_buf = 0, pixel_fmt = 0;

	if ((atomic_read(&module->state) != CAM_CFG_CH) &&
		(atomic_read(&module->state) != CAM_RUNNING)) {
		pr_warn("warning: only for state CFG_CH or RUNNING\n");
		return 0;
	}

	uparam = (struct sprd_img_parm __user *)arg;

	ret |= get_user(channel_id, &uparam->channel_id);
	ret |= get_user(buffer_count, &uparam->buffer_count);
	ret |= get_user(is_reserved_buf, &uparam->is_reserved_buf);
	ret |= get_user(pixel_fmt, &uparam->pixel_fmt);

	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		return -EFAULT;
	}

	if ((channel_id >= CAM_CH_MAX) || (buffer_count == 0) ||
		(module->channel[channel_id].enable == 0)) {
		pr_err("fail to get valid channel id %d. buf cnt %d\n",
			channel_id, buffer_count);
		return -EFAULT;
	}

	pr_debug("ch %d, buffer_count %d\n", channel_id, buffer_count);

	ch_prv = &module->channel[CAM_CH_PRE];
	hw = module->grp->hw_info;
	ch = &module->channel[channel_id];
	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	dcam_sw_aux_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];

	for (i = 0; i < buffer_count; i++) {
		pframe = cam_queue_empty_frame_get();
		pframe->buf.type = CAM_BUF_USER;
		pframe->channel_id = ch->ch_id;
		pframe->img_fmt = ch->ch_uinfo.dst_fmt;
		ret |= get_user(pframe->buf.mfd[0], &uparam->fd_array[i]);
		ret |= get_user(pframe->buf.offset[0], &uparam->frame_addr_array[i].y);
		ret |= get_user(pframe->buf.offset[1], &uparam->frame_addr_array[i].u);
		ret |= get_user(pframe->buf.offset[2], &uparam->frame_addr_array[i].v);
		ret |= get_user(pframe->user_fid, &uparam->user_fid);
		ret |= get_user(pframe->bpc_raw_flag, &uparam->fdr2_bpc_flag);
		ret |= get_user(pframe->buf.addr_vir[0], &uparam->frame_addr_vir_array[i].y);
		ret |= get_user(pframe->buf.addr_vir[1], &uparam->frame_addr_vir_array[i].u);
		ret |= get_user(pframe->buf.addr_vir[2], &uparam->frame_addr_vir_array[i].v);
		ret |= get_user(dump_type[i], &uparam->vir_ch_info[0].fd_array[i]);
		ret |= get_user(sec, &uparam->img_statis_info.sec);
		ret |= get_user(secondary_buf_info.frame_addr_array[i].y, &uparam->frame_addr_array[i].y);
		ret |= get_user(secondary_buf_info.frame_addr_array[i].u, &uparam->frame_addr_array[i].u);
		ret |= get_user(secondary_buf_info.frame_addr_array[i].v, &uparam->frame_addr_array[i].v);
		ret |= get_user(secondary_buf_info.fd_array[i], &uparam->fd_array[i]);

		if (unlikely(ret)) {
			pr_err("fail to copy from user, ret %d\n", ret);
			ret = -EFAULT;
			break;
		}

		ret = camcore_faceid_secbuf(sec, &pframe->buf);
		if (unlikely(ret))
			break;

		pr_debug("cam%d ch %d, mfd 0x%x, off 0x%x 0x%x 0x%x, reserved %d user_fid[%d]\n",
			module->idx, pframe->channel_id, pframe->buf.mfd[0],
			pframe->buf.offset[0], pframe->buf.offset[1],
			pframe->buf.offset[2], is_reserved_buf,
			pframe->user_fid);

		ret = cam_buf_ionbuf_get(&pframe->buf);
		if (ret) {
			cam_queue_empty_frame_put(pframe);
			ret = -EFAULT;
			break;
		}

		if (channel_id == CAM_CH_CAP)
			pr_info("ch %d, mfd 0x%x, off 0x%x 0x%x 0x%x, size 0x%x, reserved %d, buffer_cnt %d, bpc flag:%d\n",
				pframe->channel_id, pframe->buf.mfd[0],
				pframe->buf.offset[0],pframe->buf.offset[1], pframe->buf.offset[2],
				(uint32_t)pframe->buf.size[0], is_reserved_buf, buffer_count, pframe->bpc_raw_flag);

		if (ch->isp_path_id >= 0 && pixel_fmt != IMG_PIX_FMT_GREY) {
			cmd = ISP_PATH_CFG_OUTPUT_BUF;
			if (is_reserved_buf) {
				ch->reserved_buf_fd = pframe->buf.mfd[0];
				pframe->is_reserved = 1;
				ch->res_frame = pframe;
				pr_debug("ch %d, mfd 0x%x, off 0x%x 0x%x 0x%x, size %d user_fid[%d]\n",
					pframe->channel_id, pframe->buf.mfd[0],
					pframe->buf.offset[0], pframe->buf.offset[1],
					pframe->buf.offset[2], (uint32_t)pframe->buf.size[0],
					pframe->user_fid);
				continue;
			}
			if (channel_id == CAM_CH_VIRTUAL) {
				if (dump_type[i] == DUMP_CH_PRE) {
					pframe->height = ch->ch_uinfo.vir_channel[0].dst_size.h;
					ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle, cmd,
						ch->isp_ctx_id, ch->isp_path_id, pframe);
				}
				else if (dump_type[i] == DUMP_CH_CAP) {
					pframe->height = ch->ch_uinfo.vir_channel[1].dst_size.h;
					ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle, cmd,
						ch->slave_isp_ctx_id, ch->isp_path_id, pframe);
				}
			}
			else
				ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle, cmd,
					ch->isp_ctx_id, ch->isp_path_id, pframe);
		} else {
			cmd = DCAM_PATH_CFG_OUTPUT_BUF;
			if (is_reserved_buf) {
				ch->reserved_buf_fd = pframe->buf.mfd[0];
				pframe->is_reserved = 1;
				ch->res_frame = pframe;
				pr_debug("ch %d, mfd 0x%x, off 0x%x 0x%x 0x%x, size %d user_fid[%d]\n",
					pframe->channel_id, pframe->buf.mfd[0],
					pframe->buf.offset[0], pframe->buf.offset[1],
					pframe->buf.offset[2], (uint32_t)pframe->buf.size[0],
					pframe->user_fid);
				continue;
			} else if ((ch->ch_id == CAM_CH_CAP) && pixel_fmt == IMG_PIX_FMT_GREY) {
				pframe->img_fmt = pixel_fmt;
				cmd = DCAM_PATH_CFG_OUTPUT_ALTER_BUF;
			}

			if (pframe->bpc_raw_flag)
				dcam_sw_ctx = dcam_sw_aux_ctx;

			if (module->cam_uinfo.raw_alg_type == RAW_ALG_AI_SFNR && !module->cam_uinfo.param_frame_sync && (ch->ch_id != CAM_CH_DCAM_VCH))
				ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
					cmd, DCAM_PATH_RAW, pframe);
			else
				ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
					cmd, ch->dcam_path_id, pframe);

			/* 4in1_raw_capture, maybe need two image once */
			if (ch->second_path_enable) {
				ch->pack_bits = ch->ch_uinfo.dcam_raw_fmt;
				pframe = camcore_secondary_buf_get(&secondary_buf_info, ch, i);
				if (!pframe) {
					ret = -EFAULT;
					break;
				}
				ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
					cmd, ch->second_path_id, pframe);
			}
		}

		if (ret) {
			pr_err("fail to set output buffer for ch%d.\n", ch->ch_id);
			cam_buf_ionbuf_put(&pframe->buf);
			cam_queue_empty_frame_put(pframe);
			ret = -EFAULT;
			break;
		}
	}

	return ret;
}

static int camioctl_frame_id_base_set(	struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	uint32_t channel_id, frame_id_base;
	struct channel_context *ch;
	struct sprd_img_parm __user *uparam;

	if ((atomic_read(&module->state) != CAM_CFG_CH) &&
		(atomic_read(&module->state) != CAM_RUNNING)) {
		pr_warn("warning: only for state CFG_CH or RUNNING\n");
		return 0;
	}

	uparam = (struct sprd_img_parm __user *)arg;
	ret = get_user(channel_id, &uparam->channel_id);
	ret |= get_user(frame_id_base, &uparam->frame_base_id);
	if (ret) {
		pr_err("fail to get from user. ret %d\n", ret);
		return -EFAULT;
	}
	if ((channel_id >= CAM_CH_MAX) ||
		(module->channel[channel_id].enable == 0)) {
		pr_err("fail to get valid channel id %d\n", channel_id);
		return -EFAULT;
	}

	ch = &module->channel[channel_id];
	ch->frm_base_id = frame_id_base;

	return ret;
}

static int camioctl_stream_off(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	uint32_t i, j;
	uint32_t raw_cap = 0, running = 0;
	int32_t dcam_path_id;
	struct channel_context *ch = NULL;
	struct channel_context *ch_prv = NULL;
	int isp_ctx_id[CAM_CH_MAX] = { -1 };
	int dcam_ctx_id[CAM_CH_MAX] = { -1 };
	struct cam_hw_info *hw = NULL;
	struct dcam_pipe_dev *dev = NULL;
	struct dcam_sw_context *sw_ctx = NULL;

	if ((atomic_read(&module->state) != CAM_RUNNING) &&
		(atomic_read(&module->state) != CAM_CFG_CH)) {
		pr_info("cam%d state: %d\n", module->idx,
			atomic_read(&module->state));
		return -EFAULT;
	}

	dev = (struct dcam_pipe_dev *)module->dcam_dev_handle;
	sw_ctx = &dev->sw_ctx[module->cur_sw_ctx_id];

	if (atomic_read(&module->state) == CAM_RUNNING)
		running = 1;

	pr_info("cam %d stream off. state: %d module->cur_sw_ctx_id: %d\n",
		module->idx, atomic_read(&module->state), module->cur_sw_ctx_id);

	ch = &module->channel[CAM_CH_CAP];
	if (ch) {
		mutex_lock(&module->buf_lock[ch->ch_id]);
		if (ch->enable && ch->alloc_start) {
			wait_for_completion(&ch->alloc_com);
			pr_debug("alloc buffer done.\n");
			ch->alloc_start = 0;
		}
		mutex_unlock(&module->buf_lock[ch->ch_id]);
	}

	atomic_set(&module->state, CAM_STREAM_OFF);
	module->cap_status = CAM_CAPTURE_STOP;
	module->dcam_cap_status = DCAM_CAPTURE_STOP;

	hw = module->grp->hw_info;
	/* stop raw dump */
	if (module->dump_thrd.thread_task)
		camcore_dumpraw_deinit(module);

	/* stop mes */
	if (module->mes_thrd.thread_task)
		camcore_mes_deinit(module);

	camcore_timer_stop(&module->cam_timer);

	if (running) {
		ret = module->dcam_dev_handle->dcam_pipe_ops->stop(sw_ctx, DCAM_STOP);
		if (ret != 0)
			pr_err("fail to stop dcam%d ret:%d\n", sw_ctx->hw_ctx_id, ret);
	}

	if (sw_ctx->slw_type == DCAM_SLW_FMCU) {
		ret = hw->dcam_ioctl(hw, DCAM_HW_CFG_INIT_AXI, &sw_ctx->hw_ctx_id);
		if (ret)
			pr_err("fail to reset fmcu dcam%d ret:%d\n", sw_ctx->hw_ctx_id, ret);
		sw_ctx->hw_ctx->fmcu->ops->buf_unmap(sw_ctx->hw_ctx->fmcu);
	}

	if (module->cam_uinfo.is_4in1)
		camcore_4in1_aux_deinit(module);
	if (module->cam_uinfo.dcam_slice_mode && module->aux_dcam_dev)
		camcore_bigsize_aux_deinit(module);

	ch_prv = &module->channel[CAM_CH_PRE];

	mutex_lock(&module->zoom_lock);
	for (i = 0; i < CAM_CH_MAX; i++) {
		ch = &module->channel[i];
		isp_ctx_id[i] = -1;
		dcam_ctx_id[i] = -1;
		if (!ch->enable)
			continue;
		if (ch->ch_id == CAM_CH_RAW) {
			raw_cap = 1;
			if (module->cam_uinfo.is_4in1)
				camcore_4in1_secondary_path_deinit(module, ch);
		}
		if (ch->ch_id == CAM_CH_CAP) {
			mutex_lock(&module->fdr_lock);
			if (module->fdr_init)
				camcore_fdr_context_deinit(module, ch);
			if (module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V2) {
				struct dcam_path_cfg_param ch_desc;
				memset(&ch_desc, 0, sizeof(ch_desc));
				ch_desc.raw_fmt= module->channel[CAM_CH_CAP].ch_uinfo.dcam_raw_fmt;
				ch_desc.is_raw = 0;
				ch_desc.endian.y_endian = ENDIAN_LITTLE;
				ch_desc.bayer_pattern = module->cam_uinfo.sensor_if.img_ptn;
				module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
						DCAM_PATH_CFG_BASE,
						module->channel[CAM_CH_CAP].dcam_path_id, &ch_desc);
				module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
						DCAM_PATH_CLR_OUTPUT_ALTER_BUF,
						module->channel[CAM_CH_CAP].dcam_path_id, &ch_desc);
			}
			mutex_unlock(&module->fdr_lock);
		}

		pr_info("clear ch %d, dcam path %d, isp path 0x%x\n",
			ch->ch_id,
			ch->dcam_path_id,
			ch->isp_path_id);
		/* prv & vid use same dcam bin path, no need to put it twice */
		if (ch->ch_id == CAM_CH_VID && ch_prv->enable)
			dcam_path_id = -1;
		else {
			dcam_path_id = ch->dcam_path_id;
			dcam_ctx_id[i] = ch->dcam_ctx_id;
		}
		if (dcam_path_id >= 0) {
			module->dcam_dev_handle->dcam_pipe_ops->put_path(sw_ctx, ch->dcam_path_id);
			if (module->cam_uinfo.raw_alg_type == RAW_ALG_AI_SFNR)
				module->dcam_dev_handle->dcam_pipe_ops->put_path(sw_ctx, DCAM_PATH_RAW);
		}
	}

	for (i = 0; i < CAM_CH_MAX; i++) {
		ch = &module->channel[i];
		if (!ch->enable)
			continue;
		if (ch->ch_id == CAM_CH_RAW) {
			if (g_dbg_dump.dump_en == DUMP_PATH_RAW_BIN)
				cam_queue_clear(&ch->share_buf_queue, struct camera_frame, list);
			continue;
		}
#ifdef DCAM_BRINGUP
		if (dcam_ctx_id[i] != -1)
			module->dcam_dev_handle->dcam_pipe_ops->put_context(module->dcam_dev_handle,
				dcam_ctx_id[i])
#endif
		cam_queue_clear(&ch->zoom_coeff_queue, struct camera_frame, list);

		if ((ch->ch_id == CAM_CH_PRE) || (ch->ch_id == CAM_CH_CAP)) {
			isp_ctx_id[i] = ch->isp_ctx_id;
			if (isp_ctx_id[i] != -1) {
				uint32_t thread_proc_stop = 1;
				module->isp_dev_handle->isp_ops->ioctl(module->isp_dev_handle,
					isp_ctx_id[i], ISP_IOCTL_CFG_THREAD_PROC_STOP, &thread_proc_stop);
				module->isp_dev_handle->isp_ops->put_context(module->isp_dev_handle,
					isp_ctx_id[i]);
			}
			mutex_lock(&module->buf_lock[ch->ch_id]);
			if (ch->alloc_start) {
				wait_for_completion(&ch->alloc_com);
				pr_debug("alloc buffer done.\n");
				ch->alloc_start = 0;
			}
			mutex_unlock(&module->buf_lock[ch->ch_id]);
			if (ch->isp_updata) {
				struct isp_offline_param *cur, *prev;

				cur = (struct isp_offline_param *)ch->isp_updata;
				ch->isp_updata = NULL;
				while (cur) {
					prev = (struct isp_offline_param *)cur->prev;
					pr_info("free %p\n", cur);
					kfree(cur);
					cur = prev;
				}
			}
			if (ch->ch_id == CAM_CH_CAP && module->grp->is_mul_buf_share) {
				struct camera_frame *pframe = NULL;
				do {
					pframe = cam_queue_dequeue(&ch->share_buf_queue,
						struct camera_frame, list);
					if (pframe == NULL)
						break;
					camcore_share_buf_cfg(SHARE_BUF_SET_CB, pframe, module);
				} while (pframe);
			}
			cam_queue_clear(&ch->share_buf_queue, struct camera_frame, list);

			for (j = 0; j < ISP_NR3_BUF_NUM; j++) {
				if (ch->nr3_bufs[j]) {
					camcore_k_frame_put(ch->nr3_bufs[j]);
					ch->nr3_bufs[j] = NULL;
				}
			}

			if (module->cam_uinfo.is_rgb_ltm) {
				for (j = 0; j < ISP_LTM_BUF_NUM; j++) {
					if (ch->ltm_bufs[LTM_RGB][j]) {
						if (ch->ch_id == CAM_CH_PRE)
							camcore_k_frame_put(ch->ltm_bufs[LTM_RGB][j]);
						ch->ltm_bufs[LTM_RGB][j] = NULL;
					}
				}
			}

			if (module->cam_uinfo.is_yuv_ltm) {
				for (j = 0; j < ISP_LTM_BUF_NUM; j++) {
					if (ch->ltm_bufs[LTM_YUV][j]) {
						if (ch->ch_id == CAM_CH_PRE)
							camcore_k_frame_put(ch->ltm_bufs[LTM_YUV][j]);
						ch->ltm_bufs[LTM_YUV][j] = NULL;
					}
				}
			}

			if (ch->postproc_buf) {
				camcore_k_frame_put(ch->postproc_buf);
				ch->postproc_buf = NULL;
			}

			if (ch->ch_id == CAM_CH_CAP && module->grp->is_mul_buf_share) {
				if (ch->pyr_rec_buf) {
					cam_queue_empty_frame_put(ch->pyr_rec_buf);
					ch->pyr_rec_buf = NULL;
				}
				if (ch->pyr_dec_buf) {
					cam_queue_empty_frame_put(ch->pyr_dec_buf);
					ch->pyr_dec_buf = NULL;
				}
				if (ch->pyr_rec_buf_alg) {
					cam_queue_empty_frame_put(ch->pyr_rec_buf_alg);
					ch->pyr_rec_buf_alg = NULL;
				}
				if (ch->pyr_dec_buf_alg) {
					cam_queue_empty_frame_put(ch->pyr_dec_buf_alg);
					ch->pyr_dec_buf_alg = NULL;
				}
			} else {
				if ((module->cam_uinfo.is_pyr_rec && ch->ch_id != CAM_CH_CAP)
					|| (module->cam_uinfo.is_pyr_dec && ch->ch_id == CAM_CH_CAP)) {
					if (ch->pyr_rec_buf) {
						camcore_k_frame_put(ch->pyr_rec_buf);
						ch->pyr_rec_buf = NULL;
					}
					if (ch->pyr_rec_buf_alg && ch->ch_id == CAM_CH_CAP) {
						ret = cam_queue_empty_frame_put(ch->pyr_rec_buf_alg);
						ch->pyr_rec_buf_alg = NULL;
					}
				}

				if (module->cam_uinfo.is_pyr_dec && ch->ch_id == CAM_CH_CAP) {
					if (ch->pyr_dec_buf) {
						camcore_k_frame_put(ch->pyr_dec_buf);
						ch->pyr_dec_buf = NULL;
					}
					if (ch->pyr_dec_buf_alg) {
						ret = cam_queue_empty_frame_put(ch->pyr_dec_buf_alg);
						ch->pyr_dec_buf_alg = NULL;
					}
				}
			}
		}
	}

	if (module->dcam_dev_handle) {
		ret = module->dcam_dev_handle->dcam_pipe_ops->ioctl(sw_ctx, DCAM_IOCTL_DEINIT_STATIS_Q, NULL);
		if (ret != 0)
			pr_err("fail to deinit statis q %d\n", ret);
		ret = module->dcam_dev_handle->dcam_pipe_ops->ioctl(sw_ctx, DCAM_IOCTL_PUT_RESERV_STATSBUF, NULL);
	}

	for (i = 0; i < CAM_CH_MAX; i++) {
		ch = &module->channel[i];
		memset(ch, 0, sizeof(struct channel_context));
		ch->ch_id = i;
		ch->dcam_path_id = -1;
		ch->aux_dcam_path_id = -1;
		ch->isp_path_id = -1;
		ch->isp_ctx_id = -1;
		ch->ch_uinfo.dcam_raw_fmt = -1;
		ch->ch_uinfo.sensor_raw_fmt = -1;
		init_completion(&ch->alloc_com);
	}
	mutex_unlock(&module->zoom_lock);

	if (running) {
		if (atomic_dec_return(&module->grp->runner_nr) == 0)
			module->isp_dev_handle->isp_ops->reset(module->isp_dev_handle, hw);

		/* wait for read thread take all events in frm_queue,
		 * frm_queue max len is CAM_FRAME_Q_LEN
		 * then we loop for this counter.
		 * if read thread has exited unexpectedly,
		 * queue_clear() will clear frm_queue as well
		 */
		j = CAM_FRAME_Q_LEN;
		while (j--) {
			i = cam_queue_cnt_get(&module->frm_queue);
			if (i == 0)
				break;
			pr_info("camera%d wait for read %d %d\n", module->idx, i, j);
			msleep(20);
		}

		if (module->dual_frame) {
			cam_queue_enqueue(&module->zsl_fifo_queue,
				&module->dual_frame->list);
			module->dual_frame = NULL;
		}
		cam_queue_clear(&module->zsl_fifo_queue, struct camera_frame, list);
		cam_queue_clear(&module->remosaic_queue, struct camera_frame, list);
		/* default 0, hal set 1 when needed */
		module->auto_3dnr = 0;
	}

	camcore_param_buffer_uncfg(module);

	if(hw->csi_connect_type == DCAM_BIND_DYNAMIC && sw_ctx->csi_connect_stat == DCAM_CSI_RESUME) {
		/* csi disconnect(DCAM_HW_DISCONECT_CSI) is already done in the function of
		csi_controller_disable(csi_driver). No need to do again */
		sw_ctx->csi_connect_stat = DCAM_CSI_IDLE;
	}

	dcam_core_context_unbind(sw_ctx);

	atomic_set(&module->state, CAM_IDLE);
	if (raw_cap)
		complete(&module->streamoff_com);

	ret = cam_buf_mdbg_check();
	pr_info("cam %d stream off done.\n", module->idx);

	return ret;
}

static int camioctl_stream_on(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0, loop = 0;
	uint32_t online = 1;
	uint32_t i = 0, line_w = 0, isp_ctx_id = 0, isp_path_id = 0;
	uint32_t uframe_sync = 0, live_ch_count = 0, non_zsl_cap = 0;
	uint32_t timer = 0;
	struct channel_context *ch = NULL;
	struct channel_context *ch_pre = NULL, *ch_vid = NULL, *ch_raw = NULL;
	struct cam_hw_info *hw = module->grp->hw_info;
	struct dcam_pipe_dev *dev = (struct dcam_pipe_dev *)module->dcam_dev_handle;
	struct dcam_sw_context *sw_ctx = &dev->sw_ctx[module->cur_sw_ctx_id];
	struct cam_hw_lbuf_share camarg;

	ret = copy_from_user(&online, (void __user *)arg, sizeof(uint32_t));
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	if (atomic_read(&module->state) != CAM_CFG_CH) {
		pr_info("cam%d error state: %d\n", module->idx,
			atomic_read(&module->state));
		return -EFAULT;
	}

	atomic_set(&module->state, CAM_STREAM_ON);
	module->flash_skip_fid = 0;
	module->is_flash_status = 0;
	module->simu_fid = 0;

	loop = 0;
	do {
		ret = dcam_core_context_bind(sw_ctx, hw->csi_connect_type, module->dcam_idx);
		if (!ret) {
			if (sw_ctx->hw_ctx_id >= DCAM_HW_CONTEXT_MAX)
				pr_err("fail to get hw_ctx_id\n");
			break;
		}
		pr_info_ratelimited("ctx %d wait for hw. loop %d\n", sw_ctx->hw_ctx_id, loop);
		usleep_range(600, 800);
	} while (loop++ < 5000);

	if (sw_ctx->hw_ctx_id == DCAM_HW_CONTEXT_MAX) {
		atomic_set(&module->state, CAM_CFG_CH);
		pr_err("fail to connect. dcam %d sw_ctx_id %d\n", sw_ctx->hw_ctx_id, sw_ctx->sw_ctx_id);
		return -1;
	}

	online = !!online;
	sw_ctx->param_frame_sync = module->cam_uinfo.param_frame_sync;
	sw_ctx->raw_alg_type = module->cam_uinfo.raw_alg_type;
	pr_info("cam%d stream on online %d, sw_ctx_id = %d\n", module->idx, online, sw_ctx->sw_ctx_id);
	if (!online)
		goto cfg_ch_done;

	if (g_dbg_dump.dump_en == DUMP_PATH_RAW_BIN)
		camcore_full_raw_switch(module);
	/* settle down compression policy here */
	camcore_compression_cal(module);

	if (g_pyr_dec_online_bypass || (module->cam_uinfo.sensor_if.img_fmt == DCAM_CAP_MODE_YUV))
		module->cam_uinfo.is_pyr_rec = 0;
	else
		module->cam_uinfo.is_pyr_rec = hw->ip_dcam[sw_ctx->hw_ctx_id]->pyramid_support;
	pr_debug("cam%d sw_id %d is_pyr_rec %d\n", module->idx,
			sw_ctx->sw_ctx_id, module->cam_uinfo.is_pyr_rec);

	ret = camcore_channels_size_init(module);
	if (module->zoom_solution == ZOOM_DEFAULT)
		camcore_channel_size_binning_cal(module, 1);
	else if (module->zoom_solution == ZOOM_BINNING2 ||
		module->zoom_solution == ZOOM_BINNING4)
		camcore_channel_size_binning_cal(module, 0);
	else if (module->zoom_solution == ZOOM_SCALER)
		camcore_channel_size_binning_cal(module, ZOOM_SCALER);
	else
		camcore_channel_size_rds_cal(module);

	camcore_compression_config(module);
	ch_pre = &module->channel[CAM_CH_PRE];
	if (ch_pre->enable)
		camcore_channel_size_config(module, ch_pre);

	ch_vid = &module->channel[CAM_CH_VID];
	if (ch_vid->enable && !ch_pre->enable)
		camcore_channel_size_config(module, ch_vid);

	ch = &module->channel[CAM_CH_CAP];
	if (ch->enable) {
		camcore_channel_size_config(module, ch);
		/* alloc dcam1 memory and cfg out buf */
		if (module->cam_uinfo.dcam_slice_mode && !module->cam_uinfo.is_4in1)
			camcore_channel_bigsize_config(module, ch);
	}

	ch_raw = &module->channel[CAM_CH_RAW];
	if (ch_raw->enable)
		camcore_channel_size_config(module, ch_raw);

	ch = &module->channel[CAM_CH_CAP_THM];
	if (ch->enable)
		camcore_channel_size_config(module, ch);
cfg_ch_done:

	/* line buffer share mode setting
	 * Precondition: dcam0, dcam1 size not conflict
	 */
	line_w = module->cam_uinfo.sn_rect.w;
	if (module->cam_uinfo.is_4in1)
		line_w /= 2;
	camarg.idx = sw_ctx->hw_ctx_id;
	camarg.width = line_w;
	camarg.offline_flag = 0;
	pr_debug("dcam %d pdaf type%d\n", sw_ctx->hw_ctx_id, sw_ctx->ctx[DCAM_CXT_0].blk_pm.pdaf.pdaf_type);
	if (hw->ip_dcam[sw_ctx->hw_ctx_id]->lbuf_share_support)
		ret = hw->dcam_ioctl(hw, DCAM_HW_CFG_LBUF_SHARE_SET, &camarg);

	if (ret) {
		pr_err("fail to set line buf share\n");
		ret = -EFAULT;
		goto exit;
	}

	ret = dev->dcam_pipe_ops->ioctl(sw_ctx, DCAM_IOCTL_INIT_STATIS_Q, NULL);

	camcore_resframe_set(module);
	for (i = 0; i < CAM_CH_MAX; i++) {
		ch = &module->channel[i];
		if (!ch->enable || (ch->ch_id == CAM_CH_RAW) || (ch->ch_id == CAM_CH_VIRTUAL) || (ch->ch_id == CAM_CH_DCAM_VCH))
			continue;

		live_ch_count++;

		uframe_sync = ch->ch_id != CAM_CH_CAP;
		if (ch->ch_uinfo.frame_sync_close)
			uframe_sync = 0;
		ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
			ISP_PATH_CFG_PATH_UFRAME_SYNC,
			ch->isp_ctx_id,
			ch->isp_path_id,
			&uframe_sync);
		ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
			ISP_PATH_CFG_CTX_UFRAME_SYNC,
			ch->isp_ctx_id,
			ch->isp_path_id,
			&uframe_sync);

		cam_queue_init(&ch->zoom_coeff_queue,
			(module->is_smooth_zoom ? CAM_ZOOM_COEFF_Q_LEN : 1),
			camcore_empty_frame_put);

		if (i == CAM_CH_PRE || i == CAM_CH_VID || (i == CAM_CH_CAP && !module->channel[CAM_CH_PRE].enable)) {
			ret = camcore_buffer_path_cfg(module, i);
			if (ret) {
				pr_err("fail to cfg path buffer\n");
				goto exit;
			}
		}
		ret = camcore_buffer_ltm_cfg(module, i);
		if (ret) {
			pr_err("fail to cfg ltm buffer\n");
			goto exit;
		}
	}

	/* TODO: WORKAROUND for BBAT/factory_test/mini_camera, remove later */
	if (live_ch_count == 1) {
		pr_info("disable all uframe_sync feature\n");

		uframe_sync = 0;
		for (i = 0; i < CAM_CH_MAX; i++) {
			ch = &module->channel[i];
			if (!ch->enable)
				continue;

			isp_ctx_id = ch->isp_ctx_id;
			isp_path_id = ch->isp_path_id;

			module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
				ISP_PATH_CFG_PATH_UFRAME_SYNC,
				isp_ctx_id, isp_path_id,
				&uframe_sync);
		}
	}

	cam_queue_init(&module->zsl_fifo_queue,
		CAM_SHARED_BUF_NUM, camcore_k_frame_put);
	/* no need release buffer, only release camera_frame */
	cam_queue_init(&module->remosaic_queue,
		CAM_IRQ_Q_LEN, camcore_camera_frame_release);

	camcore_cap_info_set(module);

	module->dual_frame = NULL;

	if (module->channel[CAM_CH_CAP].enable &&
		module->channel[CAM_CH_CAP].dcam_path_id == DCAM_PATH_FULL)
			dev->dcam_pipe_ops->cfg_path(sw_ctx, DCAM_PATH_CFG_STATE,
				DCAM_PATH_FULL, &module->path_state);

	if (!module->cam_uinfo.virtualsensor) {
		ret = hw->dcam_ioctl(hw, DCAM_HW_CFG_RESET, &sw_ctx->hw_ctx_id);
		if (ret)
			pr_err("fail to reset dcam%d\n", sw_ctx->hw_ctx_id);
	}

	if(hw->csi_connect_type == DCAM_BIND_DYNAMIC && sw_ctx->csi_connect_stat != DCAM_CSI_RESUME) {
		struct dcam_switch_param csi_switch;
		csi_switch.csi_id = module->dcam_idx;
		csi_switch.dcam_id= sw_ctx->hw_ctx_id;
		pr_info("csi_switch.csi_id = %d, csi_switch.dcam_id = %d\n", csi_switch.csi_id, csi_switch.dcam_id);
		/* switch connect */
		hw->dcam_ioctl(hw, DCAM_HW_FORCE_EN_CSI, &csi_switch);
		sw_ctx->csi_connect_stat = DCAM_CSI_RESUME;
	}

	if ((module->channel[CAM_CH_PRE].enable == 0) && (module->channel[CAM_CH_CAP].enable == 1))
		non_zsl_cap = 1;
	dev->dcam_pipe_ops->ioctl(sw_ctx, DCAM_IOCTL_RECFG_PARAM, &non_zsl_cap);

	if (sw_ctx->slw_type == DCAM_SLW_FMCU)
		sw_ctx->hw_ctx->fmcu->ops->buf_map(sw_ctx->hw_ctx->fmcu);

	sw_ctx->raw_callback = module->raw_callback;
	if (!module->cam_uinfo.virtualsensor) {
		ret = dev->dcam_pipe_ops->start(sw_ctx, online);
		if (ret < 0) {
			pr_err("fail to start dcam dev, ret %d\n", ret);
			goto exit;
		}

		if (module->aux_dcam_dev) {
			struct dcam_sw_context *aux_sw_ctx = &dev->sw_ctx[module->offline_cxt_id];
			ret = dev->dcam_pipe_ops->start(aux_sw_ctx, 0);
			if (ret < 0) {
				pr_err("fail to start aux_dcam dev, ret %d\n", ret);
				goto exit;
			}
		}
	}

	atomic_set(&module->timeout_flag, 1);

	if (module->cam_uinfo.is_longexp)
		timer = CAMERA_LONGEXP_TIMEOUT;
	else
		timer = CAMERA_TIMEOUT;
	ret = camcore_timer_start(&module->cam_timer, timer);

	atomic_set(&module->state, CAM_RUNNING);

	if (module->mes_thrd.thread_task)
		camcore_mes_init(module);

	if (module->dump_thrd.thread_task)
		camcore_dumpraw_init(module);

	atomic_inc(&module->grp->runner_nr);

	if (module->cam_uinfo.virtualsensor) {
		init_completion(&sw_ctx->frm_done);
		complete(&sw_ctx->frm_done);
		init_completion(&sw_ctx->slice_done);
		complete(&sw_ctx->slice_done);
		ch = &module->channel[CAM_CH_CAP];
		if (ch->enable)
			atomic_set(&sw_ctx->virtualsensor_cap_en, 1);
		else
			atomic_set(&sw_ctx->virtualsensor_cap_en, 0);
		module->dcam_dev_handle->dcam_pipe_ops->ioctl(&module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id], DCAM_IOCTL_CREAT_INT_THREAD, NULL);
	}
	pr_info("stream on done module->dcam_idx = %d.\n", module->dcam_idx);
	cam_buf_mdbg_check();
	return 0;

exit:
	atomic_set(&module->state, CAM_CFG_CH);
	pr_info("stream on failed\n");

	/* call stream_off to clear buffers/path */
	camioctl_stream_off(module, 0L);

	return ret;
}

static int camioctl_stream_pause(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	uint32_t i;
	int32_t dcam_path_id;
	struct channel_context *ch = NULL;
	struct dcam_pipe_dev *dev = NULL;
	struct channel_context *ch_prv = NULL;
	struct dcam_sw_context *sw_ctx = NULL;

	if (atomic_read(&module->state) != CAM_RUNNING) {
		pr_info("cam%d state: %d\n", module->idx,
			atomic_read(&module->state));
		return -EFAULT;
	}

	if (module->dcam_dev_handle == NULL) {
		pr_err("fail to cam%d dcam_dev_handle is NULL\n", module->idx);
		return -EFAULT;
	}


	dev = module->dcam_dev_handle;
	sw_ctx = &dev->sw_ctx[module->cur_sw_ctx_id];
	pr_info("cam%d stream pause\n", module->idx);

	module->paused = 1;
	module->dcam_dev_handle->dcam_pipe_ops->stop(sw_ctx, DCAM_PAUSE_ONLINE);

	ch_prv = &module->channel[CAM_CH_PRE];
	for (i = 0; i < CAM_CH_MAX; i++) {
		ch = &module->channel[i];
		if (!ch->enable)
			continue;

		pr_info("clear ch %d, dcam path %d, isp path 0x%x\n",
				ch->ch_id,
				ch->dcam_path_id,
				ch->isp_path_id);
		/* prv & vid use same dcam bin path, no need to put it twice */
		if (ch->ch_id == CAM_CH_VID && ch_prv->enable)
			dcam_path_id = -1;
		else
			dcam_path_id = ch->dcam_path_id;
		if (dcam_path_id >= 0) {
			pr_info("put dcam path %d\n", ch->dcam_path_id);
			module->dcam_dev_handle->dcam_pipe_ops->put_path(sw_ctx,
					ch->dcam_path_id);
		}
	}

	for (i = 0; i < CAM_CH_MAX; i++) {
		ch = &module->channel[i];
		if (!ch->enable || (ch->ch_id == CAM_CH_RAW))
			continue;

		if ((ch->ch_id == CAM_CH_PRE) && (ch->isp_updata)) {
			struct isp_offline_param *cur, *prev;

			cur = (struct isp_offline_param *)ch->isp_updata;
			ch->isp_updata = NULL;
			while (cur) {
				prev = (struct isp_offline_param *)cur->prev;
				pr_info("free %p\n", cur);
				kfree(cur);
				cur = prev;
			}
		}
	}

	ret = module->dcam_dev_handle->dcam_pipe_ops->ioctl(sw_ctx,
			DCAM_IOCTL_DEINIT_STATIS_Q, NULL);
	if (ret)
		pr_err("fail to deinit statis q %d\n", ret);

	for (i = 0; i < CAM_CH_MAX; i++) {
		ch = &module->channel[i];
		ch->dcam_path_id = -1;
	}

	pr_info("cam %d stream pause done.\n", module->idx);

	return ret;
}

static int camioctl_stream_resume(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	uint32_t i, online = 1;
	struct channel_context *ch = NULL;
	struct channel_context *ch_pre = NULL, *ch_vid = NULL;
	struct dcam_pipe_dev *dev = NULL;
	struct dcam_sw_context *sw_ctx = NULL;
	struct cam_hw_info *hw = NULL;

	if (atomic_read(&module->state) != CAM_RUNNING) {
		pr_info("cam%d error state: %d\n", module->idx,
			atomic_read(&module->state));
		return -EFAULT;
	}

	pr_info("cam%d stream resume enter\n", module->idx);

	hw = module->grp->hw_info;
	dev = module->dcam_dev_handle;
	sw_ctx = &dev->sw_ctx[module->cur_sw_ctx_id];

	mutex_lock(&module->fdr_lock);
	if (module->fdr_init || module->fdr_done) {
		pr_err("cam%d fdr not deinit, %d %d\n", module->idx,
			module->fdr_init, module->fdr_done);
	}
	mutex_unlock(&module->fdr_lock);

	for (i = 0; i < CAM_CH_MAX; i++) {
		ch = &module->channel[i];
		if (!ch->enable)
			continue;
		camcore_channel_init(module, ch);
	}

	if (module->zoom_solution == ZOOM_DEFAULT)
		camcore_channel_size_binning_cal(module, 1);
	else if (module->zoom_solution == ZOOM_BINNING2 ||
		module->zoom_solution == ZOOM_BINNING4)
		camcore_channel_size_binning_cal(module, 0);
	else if (module->zoom_solution == ZOOM_SCALER)
		camcore_channel_size_binning_cal(module, ZOOM_SCALER);
	else
		camcore_channel_size_rds_cal(module);

	camcore_compression_config(module);

	ch_pre = &module->channel[CAM_CH_PRE];
	if (ch_pre->enable)
		camcore_channel_size_config(module, ch_pre);

	ch_vid = &module->channel[CAM_CH_VID];
	if (ch_vid->enable && !ch_pre->enable)
		camcore_channel_size_config(module, ch_vid);

	ch = &module->channel[CAM_CH_CAP];
	if (ch->enable)
		camcore_channel_size_config(module, ch);

	ret = dev->dcam_pipe_ops->ioctl(sw_ctx, DCAM_IOCTL_RECFG_PARAM, NULL);
	ret = dev->dcam_pipe_ops->ioctl(sw_ctx, DCAM_IOCTL_INIT_STATIS_Q, NULL);

	for (i = 0; i < CAM_CH_MAX; i++) {
		ch = &module->channel[i];
		if (!ch->enable)
			continue;

		if (ch->ch_id == CAM_CH_PRE || ch->ch_id == CAM_CH_CAP) {
			/* set shared frame for dcam output */
			while (1) {
				struct camera_frame *pframe = NULL;

				pframe = cam_queue_dequeue(&ch->share_buf_queue,
					struct camera_frame, list);
				if (pframe == NULL)
					break;
				ret = dev->dcam_pipe_ops->cfg_path(sw_ctx,
						DCAM_PATH_CFG_OUTPUT_BUF,
						ch->dcam_path_id, pframe);
				if (ret) {
					pr_err("fail to config dcam output buffer\n");
					cam_queue_enqueue(&ch->share_buf_queue, &pframe->list);
					ret = -EINVAL;
					goto exit;
				}
			}
		}
	}

	module->paused = 0;
	camcore_cap_info_set(module);

	ret = dev->dcam_pipe_ops->start(sw_ctx, online);
	if (ret < 0) {
		pr_err("fail to start dcam dev, ret %d\n", ret);
		goto exit;
	}

	pr_info("cam%d stream resume done\n", module->idx);
	return 0;

exit:
	pr_info("cam%d stream resume failed\n", module->idx);
	return ret;
}

static int camioctl_cam_res_get(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	int dcam_idx, retry = 1;
	uint32_t rps_info = 0;
	struct sprd_img_res res = {0};
	struct camera_group *grp = module->grp;
	void *dcam = NULL;
	void *isp = NULL;
	struct cam_thread_info *thrd = NULL;

	ret = copy_from_user(&res, (void __user *)arg,
		sizeof(struct sprd_img_res));
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	if (atomic_read(&module->state) != CAM_INIT) {
		pr_err("fail to get cam%d state: %d\n",
			module->idx, atomic_read(&module->state));
		return -EFAULT;
	}

	/* create capture thread */
	thrd = &module->cap_thrd;
	sprintf(thrd->thread_name, "cam%d_capture", module->idx);
	ret = camcore_thread_create(module, thrd, camcore_capture_proc);
	if (ret)
		goto stop_thrd;

	/* create zoom thread */
	thrd = &module->zoom_thrd;
	sprintf(thrd->thread_name, "cam%d_zoom", module->idx);
	ret = camcore_thread_create(module, thrd, camcore_zoom_proc);
	if (ret)
		goto stop_thrd;

	/* create buf thread */
	thrd = &module->buf_thrd;
	sprintf(thrd->thread_name, "cam%d_alloc_buf", module->idx);
	ret = camcore_thread_create(module, thrd, camcore_buffers_alloc);
	if (ret)
		goto stop_thrd;

	/* create dcam offline thread */
	thrd = &module->dcam_offline_proc_thrd;
	sprintf(thrd->thread_name, "cam%d_offline_proc", module->idx);
	ret = camcore_thread_create(module, thrd, camcore_offline_proc);
	if (ret)
		goto stop_thrd;

	/* create raw mes thread */
	thrd = &module->mes_thrd;
	sprintf(thrd->thread_name, "cam%d_mes_proc", module->idx);
	ret = camcore_thread_create(module, thrd, camcore_mes_proc);
	if (ret)
		goto stop_thrd;

	if (g_dbg_dump.dump_en) {
		/* create dump thread */
		thrd = &module->dump_thrd;
		sprintf(thrd->thread_name, "cam%d_dumpraw", module->idx);
		ret = camcore_thread_create(module, thrd, camcore_dumpraw_proc);
		if (ret)
			goto stop_thrd;
	}

	pr_info("cam%d get res, flag %d, sensor %d\n",
		module->idx, res.flag, res.sensor_id);

	dcam_idx = -1;
	rps_info =  !!res.flag;
	module->simulator = rps_info;
	if (unlikely(rps_info)) {
		if (res.sensor_id >= DCAM_ID_MAX) {
			/* invalid sensor id means no senor and any DCAM is OK */
			/* camera offline simulator should set to it */
			dcam_idx = DCAM_ID_0;
			retry = 1;
		} else {
			dcam_idx = res.sensor_id;
		}
	} else if (res.sensor_id < SPRD_SENSOR_ID_MAX) {
		/* get a preferred dcam dev */
		dcam_idx = sprd_sensor_find_dcam_id(res.sensor_id);
		pr_debug("get csi id %d\n", dcam_idx);
	}

check:
	if (!is_csi_id(dcam_idx) || (dcam_idx < 0)) {
		pr_err("fail to get dcam id for sensor: %d\n", res.sensor_id);
		return -EFAULT;
	}

	dcam = module->dcam_dev_handle;
	if (dcam == NULL) {
		dcam = dcam_core_pipe_dev_get(grp->hw_info);
		if (IS_ERR_OR_NULL(dcam)) {
			if (retry) {
				dcam_idx++;
				goto check;
			}
			pr_err("fail to get dcam%d\n", dcam_idx);
			ret = -EINVAL;
			goto no_dcam;
		}
		module->dcam_dev_handle = dcam;
		module->dcam_idx = dcam_idx;
	}

	ret = module->dcam_dev_handle->dcam_pipe_ops->open(dcam);
	if (ret) {
		ret = -EINVAL;
		goto dcam_fail;
	}

	ret = module->dcam_dev_handle->dcam_pipe_ops->get_context(module->dcam_dev_handle);
	if (ret < 0) {
		pr_err("fail to get dcam context for cam%d\n", module->idx);
		goto dcam_cb_fail;
	} else {
		module->cur_sw_ctx_id = ret;
		module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id].offline = 0;
	}

	module->dcam_dev_handle->dcam_pipe_ops->set_callback(module->dcam_dev_handle,
		module->cur_sw_ctx_id, camcore_dcam_callback, module);
	module->dcam_dev_handle->dcam_pipe_ops->share_buf_set_cb(module->dcam_dev_handle,
		module->cur_sw_ctx_id, camcore_share_buf_cfg, module);

	/*offline context*/
	ret = module->dcam_dev_handle->dcam_pipe_ops->get_context(module->dcam_dev_handle);
	if (ret < 0) {
		pr_err("fail to get dcam offine context for cam%d\n", module->idx);
		goto dcam_cb_fail;
	} else {
		module->offline_cxt_id = ret;
		module->dcam_dev_handle->sw_ctx[module->offline_cxt_id].offline = 1;
		module->dcam_dev_handle->dcam_pipe_ops->ioctl(&module->dcam_dev_handle->sw_ctx[module->offline_cxt_id], DCAM_IOCTL_CREAT_INT_THREAD, NULL);
		if (module->cam_uinfo.raw_alg_type && module->cam_uinfo.param_frame_sync) {
			ret = camcore_dcam_pmbuf_init(&module->dcam_dev_handle->sw_ctx[module->offline_cxt_id]);
			if (ret)
				pr_err("fail to alloc dcam pm buffer, use ctx to recieve pm.\n");
		}
	}

	module->dcam_dev_handle->dcam_pipe_ops->set_callback(module->dcam_dev_handle,
		module->offline_cxt_id, camcore_dcam_callback, module);

	pr_info("camca get camera res camsec mode %d.\n",
		module->grp->camsec_cfg.camsec_mode);

	if (module->grp->camsec_cfg.camsec_mode != SEC_UNABLE) {
		bool sec_eb = true;

		ret = module->dcam_dev_handle->dcam_pipe_ops->ioctl(&module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id],
			DCAM_IOCTL_CFG_SEC, &sec_eb);
		if (ret) {
			pr_err("fail to set cam sec %d.\n", module->grp->camsec_cfg.camsec_mode);
			goto dcam_cb_fail;
		}
	}

	if (rps_info)
		module->dcam_dev_handle->dcam_pipe_ops->ioctl(&module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id],
			DCAM_IOCTL_CFG_RPS, &rps_info);

	isp = module->isp_dev_handle;
	if (isp == NULL) {
		isp = isp_core_pipe_dev_get();
		if (IS_ERR_OR_NULL(isp)) {
			pr_err("fail to get isp\n");
			module->isp_dev_handle = NULL;
			ret = -EINVAL;
			goto no_isp;
		}
		module->isp_dev_handle = isp;
	}

	ret = module->isp_dev_handle->isp_ops->ioctl(module->isp_dev_handle, 0,
		ISP_IOCTL_CFG_SEC, &module->grp->camsec_cfg.camsec_mode);

	if (ret) {
		pr_err("fail to set isp sec %d.\n", module->grp->camsec_cfg.camsec_mode);
		goto wq_fail;
	}

	ret = module->isp_dev_handle->isp_ops->open(isp, grp->hw_info);
	if (ret) {
		pr_err("fail to enable isp module.\n");
		ret = -EINVAL;
		goto isp_fail;
	}


	module->attach_sensor_id = res.sensor_id;

	if (dcam_idx == DCAM_ID_0)
		res.flag = DCAM_RES_DCAM0_CAP | DCAM_RES_DCAM0_PATH;
	else if (dcam_idx == DCAM_ID_1)
		res.flag = DCAM_RES_DCAM1_CAP | DCAM_RES_DCAM1_PATH;
	else
		res.flag = DCAM_RES_DCAM2_CAP | DCAM_RES_DCAM2_PATH;

	pr_info("sensor %d w %u h %u, cam %d, csi_idx %d, sw ctx%d, offline ctx%d, res.flag %x\n",
		res.sensor_id, res.width, res.height, module->idx, module->dcam_idx, module->cur_sw_ctx_id, module->offline_cxt_id, res.flag);

	ret = copy_to_user((void __user *)arg, &res,
		sizeof(struct sprd_img_res));
	if (ret) {
		pr_err("fail to copy_to_user\n");
		ret = -EFAULT;
		goto copy_fail;
	}
	atomic_set(&module->state, CAM_IDLE);
	pr_info("cam%d get res done\n", module->idx);
	return 0;

copy_fail:
wq_fail:
	module->isp_dev_handle->isp_ops->close(isp);

isp_fail:
	isp_core_pipe_dev_put(isp);
	module->isp_dev_handle = NULL;

no_isp:
dcam_cb_fail:
	module->dcam_dev_handle->dcam_pipe_ops->close(dcam);

dcam_fail:
	dcam_core_pipe_dev_put(dcam);
	module->dcam_dev_handle = NULL;
no_dcam:
	pr_err("fail to get camera res for sensor: %d\n", res.sensor_id);

stop_thrd:
	camcore_thread_stop(&module->cap_thrd);
	camcore_thread_stop(&module->zoom_thrd);
	camcore_thread_stop(&module->buf_thrd);
	camcore_thread_stop(&module->dump_thrd);
	camcore_thread_stop(&module->dcam_offline_proc_thrd);
	camcore_thread_stop(&module->mes_thrd);

	return ret;
}


static int camioctl_cam_res_put(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	uint32_t idx;
	struct sprd_img_res res = {0};

	ret = copy_from_user(&res, (void __user *)arg,
		sizeof(struct sprd_img_res));
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	pr_info("cam%d put res state: %d\n", module->idx,
		atomic_read(&module->state));

	ret = camioctl_stream_off(module, arg);

	if (atomic_read(&module->state) != CAM_IDLE) {
		pr_info("cam%d error state: %d\n", module->idx,
			atomic_read(&module->state));
		return -EFAULT;
	}

	idx = module->idx;

	if (module->attach_sensor_id != res.sensor_id) {
		pr_warn("warning: mismatch sensor id: %d, %d for cam %d\n",
			module->attach_sensor_id, res.sensor_id,
			module->idx);
	}

	module->attach_sensor_id = -1;

	camcore_thread_stop(&module->dcam_offline_proc_thrd);
	camcore_thread_stop(&module->cap_thrd);
	camcore_thread_stop(&module->zoom_thrd);
	camcore_thread_stop(&module->buf_thrd);
	camcore_thread_stop(&module->dump_thrd);
	camcore_thread_stop(&module->mes_thrd);

	if (module->dcam_dev_handle) {
		module->dcam_dev_handle->dcam_pipe_ops->put_context(module->dcam_dev_handle,
			module->cur_sw_ctx_id);
		module->dcam_dev_handle->dcam_pipe_ops->put_context(module->dcam_dev_handle,
			module->offline_cxt_id);
		module->dcam_dev_handle->dcam_pipe_ops->close(module->dcam_dev_handle);
		dcam_core_pipe_dev_put(module->dcam_dev_handle);
		module->dcam_dev_handle = NULL;
	}
	if (module->isp_dev_handle) {
		module->isp_dev_handle->isp_ops->close(module->isp_dev_handle);
		isp_core_pipe_dev_put(module->isp_dev_handle);
		module->isp_dev_handle = NULL;
	}

	atomic_set(&module->state, CAM_INIT);

	pr_debug("sensor %d w %u h %u, cam [%d]\n",
		res.sensor_id, res.width, res.height, module->idx);

	pr_info("put camera res for sensor %d res %x done.",
			res.sensor_id, res.flag);
	ret = copy_to_user((void __user *)arg, &res,
			sizeof(struct sprd_img_res));
	if (ret)
		pr_err("fail to copy_to_user!\n");

	return ret;
}

static int camioctl_capture_start(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct sprd_img_capture_param param;
	struct cam_hw_info *hw = NULL;
	struct cam_hw_gtm_ltm_dis dis;
	ktime_t start_time = 0;
	uint32_t isp_idx = 0;
	struct channel_context *ch = NULL;
	struct dcam_sw_context *sw_ctx = NULL;
	struct dcam_sw_context *sw_aux_ctx = NULL;
	struct dcam_hw_path_restart re_patharg;
	struct camera_frame *pframe = NULL;
	uint32_t is_post_multi = 0;

	ret = copy_from_user(&param, (void __user *)arg,
			sizeof(struct sprd_img_capture_param));
	if (ret) {
		pr_err("fail to copy user info\n");
		return -EFAULT;
	}

	if (atomic_read(&module->state) != CAM_RUNNING) {
		pr_info("cam%d state: %d\n", module->idx,
			atomic_read(&module->state));
		return ret;
	}
	hw = module->grp->hw_info;
	start_time = ktime_get_boottime();
	module->capture_times = start_time;
	ch = &module->channel[CAM_CH_CAP];
	if(ch) {
		mutex_lock(&module->buf_lock[ch->ch_id]);
		if (ch->alloc_start) {
			ret = wait_for_completion_interruptible(&ch->alloc_com);
			if (ret != 0)
				pr_err("fail to config channel/path param work %d\n", ret);
			pr_debug("alloc buffer done.\n");
			ch->alloc_start = 0;
		}
		mutex_unlock(&module->buf_lock[ch->ch_id]);
	}

	sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	sw_aux_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];
	module->capture_scene = param.cap_scene;
	isp_idx = module->channel[CAM_CH_CAP].isp_ctx_id;
	module->isp_dev_handle->isp_ops->ioctl(module->isp_dev_handle, ch->isp_ctx_id,
		ISP_IOCTL_CFG_POST_CNT, &param.cap_cnt);
	module->isp_dev_handle->isp_ops->clear_stream_ctrl(module->isp_dev_handle, ch->isp_ctx_id);
	if (module->capture_scene == CAPTURE_FDR
		|| module->capture_scene == CAPTURE_HW3DNR
		|| module->capture_scene == CAPTURE_FLASH
		|| module->capture_scene == CAPTURE_RAWALG
		|| module->capture_scene == CAPTURE_AI_SFNR) {
		dis.dcam_idx = sw_ctx->hw_ctx_id;
		dis.isp_idx = isp_idx;
		if (!hw->ip_isp->rgb_gtm_support) {
			hw->dcam_ioctl(hw, DCAM_HW_CFG_GTM_LTM_DIS, &dis);
		}

		if (module->capture_scene == CAPTURE_FDR) {
			if (module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V2) {
				module->dcam_dev_handle->sw_ctx[module->offline_cxt_id].is_raw_alg = 1;
			} else if (module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V1) {
				struct dcam_path_cfg_param ch_desc;
				/* reconfig full path to raw */
				memset(&ch_desc, 0, sizeof(ch_desc));
				ch_desc.raw_fmt= DCAM_RAW_14;
				ch_desc.is_raw = 1;
				ch_desc.endian.y_endian = ENDIAN_LITTLE;
				sw_ctx->is_raw_alg = 1;
				ch_desc.bayer_pattern = module->cam_uinfo.sensor_if.img_ptn;
				ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
					DCAM_PATH_CFG_BASE,
					module->channel[CAM_CH_CAP].dcam_path_id, &ch_desc);

				if (atomic_read(&sw_ctx->path[DCAM_PATH_RAW].is_shutoff) == 1 &&
					module->channel[CAM_CH_CAP].dcam_path_id == DCAM_PATH_RAW) {
					uint32_t shutoff = 0;
					re_patharg.idx = sw_ctx->hw_ctx_id;
					re_patharg.path_id = DCAM_PATH_RAW;
					hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_RESTART, &re_patharg);
					module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx, DCAM_PATH_CFG_SHUTOFF,
						DCAM_PATH_RAW, &shutoff);
				}
			}
		}  else if (module->capture_scene == CAPTURE_AI_SFNR) {
				uint32_t shutoff = 0;
				struct dcam_hw_path_restart re_patharg;
				struct dcam_hw_path_stop patharg;

				sw_ctx->is_raw_alg = 1;
				sw_aux_ctx->is_raw_alg = 1;

				shutoff = 1;
				patharg.path_id = DCAM_PATH_FULL;
				patharg.idx = sw_ctx->hw_ctx_id;
				patharg.raw_alg_type = module->cam_uinfo.raw_alg_type;
				hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_STOP, &patharg);
				module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
					DCAM_PATH_CFG_SHUTOFF, patharg.path_id, &shutoff);

				shutoff = 0;
				re_patharg.path_id = DCAM_PATH_RAW;
				re_patharg.idx = sw_ctx->hw_ctx_id;
				sw_ctx->path[re_patharg.path_id].size_update = 1;
				re_patharg.raw_alg_type = module->cam_uinfo.raw_alg_type;
				hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_RESTART, &re_patharg);
				module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
					DCAM_PATH_CFG_SHUTOFF, re_patharg.path_id, &shutoff);
		} else
			pr_debug("other cap scene:%d.\n", module->capture_scene);
	}

	/* recognize the capture scene */
	if (param.type == DCAM_CAPTURE_START_FROM_NEXT_SOF) {
		is_post_multi = 1;
		module->isp_dev_handle->isp_ops->ioctl(module->isp_dev_handle, ch->isp_ctx_id,
			ISP_IOCTL_CFG_POST_MULTI_SCENE, &is_post_multi);
		module->dcam_cap_status = DCAM_CAPTURE_START_FROM_NEXT_SOF;
		atomic_set(&module->capture_frames_dcam, param.cap_cnt);
		atomic_set(&module->cap_total_frames, param.cap_cnt);
		if ((module->capture_scene == CAPTURE_SW3DNR ||
			module->capture_scene == CAPTURE_FDR ||
			module->capture_scene == CAPTURE_RAWALG ||
			module->capture_scene == CAPTURE_AI_SFNR ||
			module->capture_scene == CAPTURE_HDR) &&
			(param.timestamp != 0))
			module->capture_times = param.timestamp;
		else
			module->capture_times = start_time;
	} else if (param.type == DCAM_CAPTURE_START_WITH_TIMESTAMP) {
		module->dcam_cap_status = DCAM_CAPTURE_START_WITH_TIMESTAMP;
		/* dual camera need 1 frame */
		atomic_set(&module->capture_frames_dcam, param.cap_cnt);
		module->capture_times = param.timestamp;
	} else if (module->cam_uinfo.is_4in1) {
		/* not report when setting */
		atomic_set(&module->capture_frames_dcam, 0);
		if (param.type == DCAM_CAPTURE_START_4IN1_LOWLUX) {
			/* 4in1: low lux mode, report until stop capture
			 * raw capture then not change source of full
			 * maybe need 1 frame
			 */
			if (module->last_channel_id == CAM_CH_RAW) {
				atomic_set(&module->capture_frames_dcam, -1);
				module->lowlux_4in1 = 0;
			} else {
				module->lowlux_4in1 = 1;
				module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
					DCAM_PATH_CFG_FULL_SOURCE,
					DCAM_PATH_FULL,
					&module->lowlux_4in1);
			}
		} else {
			/* 4in1: report 1 frame for remosaic */
			module->lowlux_4in1 = 0;
			atomic_set(&module->capture_frames_dcam, CAP_NUM_COMMON);
		}
		camcore_4in1_channel_size_config(module, module->lowlux_4in1);
	} else if (param.type == DCAM_CAPTURE_START) {
		module->capture_times = param.timestamp;
		module->dcam_cap_status = DCAM_CAPTURE_START;
		atomic_set(&module->capture_frames_dcam, CAP_NUM_COMMON);
		if (module->capture_scene == CAPTURE_HDR) {
			while (1) {
				pframe = cam_queue_dequeue(&module->channel[CAM_CH_CAP].share_buf_queue, struct camera_frame, list);
				if (!pframe)
					break;
				module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx, DCAM_PATH_CFG_OUTPUT_BUF, module->channel[CAM_CH_CAP].dcam_path_id, pframe);
			}
		}
	} else {
		atomic_set(&module->capture_frames_dcam, -1);
	}

	if (param.type != DCAM_CAPTURE_STOP) {
		module->cap_status = CAM_CAPTURE_START;
		if (module->dcam_cap_status != DCAM_CAPTURE_START_FROM_NEXT_SOF)
			module->isp_dev_handle->isp_ops->ioctl(module->isp_dev_handle, module->channel[CAM_CH_CAP].isp_ctx_id,
				ISP_IOCTL_CFG_CTX_CAP_CNT, NULL);
	}

	/* alway trigger dump for capture */
	if (module->dump_thrd.thread_task && module->dcam_dev_handle)
		camdump_start(&module->dump_thrd, &module->dump_base, module->dcam_idx);

	pr_info("cam %d start capture type %d, scene %d, cnt %d, time %lld, capture num:%d\n",
		module->idx, param.type, module->capture_scene, param.cap_cnt,
		module->capture_times, module->capture_frames_dcam);
	return ret;
}

static int camioctl_capture_stop(struct camera_module *module,
		unsigned long arg)
{
	struct channel_context *channel = NULL;
	struct cam_hw_info *hw = NULL;
	struct cam_hw_gtm_ltm_eb eb;
	uint32_t isp_idx = 0;
	struct dcam_sw_context *sw_ctx = NULL;
	struct dcam_sw_context *sw_aux_ctx = NULL;
	uint32_t is_post_multi = 0;

	if (module->cap_status == CAM_CAPTURE_STOP) {
		pr_info("cam%d alreay capture stopped\n", module->idx);
		return 0;
	}

	if (module->dcam_cap_status != DCAM_CAPTURE_START_FROM_NEXT_SOF)
		module->isp_dev_handle->isp_ops->ioctl(module->isp_dev_handle, module->channel[CAM_CH_CAP].isp_ctx_id,
			ISP_IOCTL_CFG_CTX_CAP_CNT, NULL);

	sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	sw_aux_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];
	channel = &module->channel[CAM_CH_CAP];
	module->cap_status = CAM_CAPTURE_STOP;
	module->dcam_cap_status = DCAM_CAPTURE_STOP;
	module->raw_cap_fetch_fmt = DCAM_RAW_MAX;
	atomic_set(&module->cap_flag, 0);

	pr_info("cam %d stop capture.\n", module->idx);
	hw = module->grp->hw_info;
	isp_idx = channel->isp_ctx_id;
	module->isp_dev_handle->isp_ops->ioctl(module->isp_dev_handle, module->channel[CAM_CH_CAP].isp_ctx_id,
		ISP_IOCTL_CFG_POST_MULTI_SCENE, &is_post_multi);
	if (module->capture_scene == CAPTURE_FDR
		|| module->capture_scene == CAPTURE_HW3DNR
		|| module->capture_scene == CAPTURE_FLASH
		|| module->capture_scene == CAPTURE_AI_SFNR) {
		struct dcam_path_cfg_param ch_desc;
		memset(&ch_desc, 0, sizeof(ch_desc));

		eb.dcam_idx = sw_ctx->hw_ctx_id;
		eb.isp_idx = isp_idx;
		eb.dcam_param = &sw_ctx->ctx[DCAM_CXT_0].blk_pm;
		if (!hw->ip_isp->rgb_gtm_support)
			module->dcam_dev_handle->hw->dcam_ioctl(hw, DCAM_HW_CFG_GTM_LTM_EB, &eb);

		if (module->capture_scene == CAPTURE_FDR) {
			if (module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V1) {
				if (module->channel[CAM_CH_CAP].dcam_path_id == DCAM_PATH_FULL) {
					ch_desc.raw_fmt= module->channel[CAM_CH_CAP].ch_uinfo.dcam_raw_fmt;
					ch_desc.is_raw = 0;
					ch_desc.endian.y_endian = ENDIAN_LITTLE;
					ch_desc.bayer_pattern = module->cam_uinfo.sensor_if.img_ptn;
					module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
							DCAM_PATH_CFG_BASE,
							channel->dcam_path_id, &ch_desc);

					module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
							DCAM_PATH_CLR_OUTPUT_ALTER_BUF,
							channel->dcam_path_id, &ch_desc);
				}
			}
		} else if (module->capture_scene == CAPTURE_AI_SFNR) {
			if (hw->ip_dcam[0]->dcam_raw_path_id != DCAM_PATH_RAW) {
				channel->zsl_buffer_num = module->cam_uinfo.zsl_num;
				sw_ctx->is_raw_alg = 0;
				ch_desc.raw_fmt = DCAM_RAW_14;
				ch_desc.is_raw = 0;
				ch_desc.endian.y_endian = ENDIAN_LITTLE;
				ch_desc.bayer_pattern = module->cam_uinfo.sensor_if.img_ptn;
				ch_desc.input_trim.start_x = module->cam_uinfo.sn_rect.x;
				ch_desc.input_trim.start_y = module->cam_uinfo.sn_rect.y;
				ch_desc.input_trim.size_x = module->cam_uinfo.sn_rect.w;
				ch_desc.input_trim.size_y = module->cam_uinfo.sn_rect.h;
				module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
					DCAM_PATH_CFG_BASE, channel->dcam_path_id, &ch_desc);
			} else {
				uint32_t shutoff = 0;
				struct dcam_hw_path_restart re_patharg;

				shutoff = 0;
				sw_ctx->is_raw_alg= 0;
				sw_aux_ctx->is_raw_alg = 0;
				re_patharg.path_id = DCAM_PATH_FULL;
				re_patharg.idx = sw_ctx->hw_ctx_id;
				re_patharg.raw_alg_type = module->cam_uinfo.raw_alg_type;
				hw->dcam_ioctl(hw, DCAM_HW_CFG_PATH_RESTART, &re_patharg);
				module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
					DCAM_PATH_CFG_SHUTOFF, re_patharg.path_id, &shutoff);
			}
		} else
			pr_debug("other cap scene:%d.\n", module->capture_scene);
	}

	/* Handling special case in which stop_capture comes before start_capture.
	 * In this case before assigning NULL to  module->dual_frame, we should check if it points
	 * to some valid frame, if yes then add that frame to dcam path queue to avoid Memroy leak.
	 */
	if (module->cam_uinfo.is_dual && module->dual_frame) {
		channel = &module->channel[module->dual_frame->channel_id];
		module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
				DCAM_PATH_CFG_OUTPUT_BUF,
				channel->dcam_path_id, module->dual_frame);
		pr_info("stop capture comes before start capture, dcam_path_Id = %d\n", channel->dcam_path_id);
		module->dual_frame = NULL;
	}
	/* stop dump for capture */
	if (module->dump_thrd.thread_task && module->dump_base.in_dump) {
		camdump_stop(&module->dump_base);
	}
	/* 4in1 lowlux deinit */
	if (module->cam_uinfo.is_4in1 && module->lowlux_4in1) {
		module->lowlux_4in1 = 0;
		module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
				DCAM_PATH_CFG_FULL_SOURCE,
				DCAM_PATH_FULL,
				&module->lowlux_4in1);
	}
	module->capture_scene = CAPTURE_COMMON;
	module->is_flash_status = 0;

	return 0;
}

static int camioctl_raw_proc(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	int error_state = 0;
	struct isp_raw_proc_info proc_info;
	uint32_t rps_info;
	struct dcam_sw_context *dcam_sw_ctx = NULL;

	ret = copy_from_user(&proc_info, (void __user *)arg,
				sizeof(struct isp_raw_proc_info));
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	if (!module->dcam_dev_handle || !module->isp_dev_handle) {
		pr_err("fail to init hw resource.\n");
		return -EFAULT;
	}

	if (module->cam_uinfo.is_4in1)
		proc_info.scene = RAW_PROC_SCENE_RAWCAP;

	if (proc_info.scene == RAW_PROC_SCENE_HWSIM_NEW) {
		ret = camcore_raw_post_proc_new(module, &proc_info);
		return ret;
	}

	if (proc_info.scene == RAW_PROC_SCENE_HWSIM) {
		rps_info = 1;
		pr_info("hwsim\n");
		if (module->cam_uinfo.virtualsensor) {
			dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
			ret = module->dcam_dev_handle->dcam_pipe_ops->ioctl(&module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id],
					DCAM_IOCTL_CFG_RPS, &rps_info);
			dcam_sw_ctx->virtualsensor = 1;

		} else {
			dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];
			ret = module->dcam_dev_handle->dcam_pipe_ops->ioctl(&module->dcam_dev_handle->sw_ctx[module->offline_cxt_id],
					DCAM_IOCTL_CFG_RPS, &rps_info);
			dcam_sw_ctx->virtualsensor = 0;
		}
		if (ret != 0) {
			pr_err("fail to config rps %d\n", ret);
			return -EFAULT;
		}
	}

	if ((proc_info.cmd == RAW_PROC_PRE) && (proc_info.scene == RAW_PROC_SCENE_RAWCAP)) {
		mutex_unlock(&module->lock);
		/* raw proc must wait for camera stream off and hw is idle */
		ret = wait_for_completion_interruptible(
				&module->streamoff_com);
		mutex_lock(&module->lock);
		if (ret != 0) {
			pr_err("fail to wait streamoff com %d\n", ret);
			return -EFAULT;
		}
	}

	if (proc_info.scene == RAW_PROC_SCENE_RAWCAP) {
		error_state = ((proc_info.cmd == RAW_PROC_PRE) &&
			(atomic_read(&module->state) != CAM_IDLE));
		error_state |= ((proc_info.cmd == RAW_PROC_POST) &&
			(atomic_read(&module->state) != CAM_CFG_CH));
		if (error_state) {
			pr_info("cam%d rawproc %d error state: %d\n",
					module->idx, proc_info.cmd,
					atomic_read(&module->state));
			return -EFAULT;
		}
	} else {
		pr_info("state[%d]\n", atomic_read(&module->state));
	}

	if (proc_info.cmd == RAW_PROC_PRE) {
		ret = camcore_raw_pre_proc(module, &proc_info);
	} else if (proc_info.cmd == RAW_PROC_POST) {
		ret = camcore_raw_post_proc(module, &proc_info);
	} else if (proc_info.cmd == RAW_PROC_DONE) {
		ret = camcore_raw_proc_done(module);
	} else if (proc_info.cmd == VIRTUAL_SENSOR_PROC) {
		ret = camcore_virtual_sensor_proc(module, &proc_info);
	} else {
		pr_err("fail to get correct cmd %d\n", proc_info.cmd);
		ret = -EINVAL;
	}

	return ret;
}

static int camioctl_fdr_post(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0, i = 0;
	uint32_t shutoff = 0;
	uint32_t scene_mode = 0, user_fid = 0, index = 0, reserved[4] = {0};
	int32_t isp_path_id = 0, isp_ctx_id = 0;
	struct sprd_img_size dst_size = {0};
	struct dcam_pipe_dev *dcam = NULL;
	struct channel_context *ch = NULL;
	struct camera_frame *pframe = NULL, *pfrm[3] = {NULL, NULL, NULL};
	struct camera_frame *pfrm_dcam = NULL, *pfrm_isp = NULL;
	struct dcam_data_ctrl_info dcam_ctrl = {0};
	struct cam_data_ctrl_in ctrl_in = {0};
	struct isp_data_ctrl_cfg *fdr_ctrl = NULL;
	struct dcam_path_cfg_param ch_desc = {0};
	struct dcam_hw_path_stop patharg = {0};
	struct dcam_hw_path_restart re_patharg = {0};
	struct dcam_sw_context *dcam_sw_aux_ctx = NULL;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	struct isp_sw_context *isp_sw_ctx = NULL;
	struct sprd_img_parm __user *uparam;

	uparam = (struct sprd_img_parm __user *)arg;
	ret |= get_user(scene_mode, &uparam->scene_mode);
	ret |= get_user(user_fid, &uparam->user_fid);
	ret |= get_user(index, &uparam->index);
	ret |= get_user(reserved[0], &uparam->reserved[0]);
	ret |= get_user(reserved[1], &uparam->reserved[1]);
	ret |= get_user(reserved[2], &uparam->reserved[2]);
	ret |= get_user(reserved[3], &uparam->reserved[3]);
	ret |= get_user(dst_size.w, &uparam->dst_size.w);
	ret |= get_user(dst_size.h, &uparam->dst_size.h);

	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	mutex_lock(&module->fdr_lock);
	if (module->fdr_init == 0) {
		mutex_unlock(&module->fdr_lock);
		pr_debug("deinit_fdr_context may be called for stream off\n");
		return 0;
	}

	memset(&ch_desc, 0, sizeof(ch_desc));
	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	if (module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V1)
		module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id].is_raw_alg = 0;

	ch = &module->channel[CAM_CH_CAP];
	for (i = 0; i < 3; i++) {
		pframe = cam_queue_empty_frame_get();
		pframe->endian = ENDIAN_LITTLE;
		pframe->pattern = module->cam_uinfo.sensor_if.img_ptn;
		pframe->width = ch->ch_uinfo.src_size.w;
		pframe->height = ch->ch_uinfo.src_size.h;
		pframe->user_fid = user_fid;
		pframe->fid = index;
		pframe->sensor_time.tv_sec = reserved[0];
		pframe->sensor_time.tv_usec = reserved[1];
		pframe->boot_sensor_time = reserved[3];
		pframe->boot_sensor_time = (pframe->boot_sensor_time << 32) | reserved[2];
		pframe->buf.type = CAM_BUF_USER;
		ret |= get_user(pframe->buf.mfd[0],&uparam->fd_array[i]);
		ret |= get_user(pframe->buf.offset[0],&uparam->frame_addr_array[i].y);
		ret |= get_user(pframe->buf.offset[1],&uparam->frame_addr_array[i].u);
		ret |= get_user(pframe->buf.offset[2],&uparam->frame_addr_array[i].v);
		ret |= get_user(pframe->buf.addr_vir[0],&uparam->frame_addr_vir_array[i].y);
		ret |= get_user(pframe->buf.addr_vir[1],&uparam->frame_addr_vir_array[i].u);
		ret |= get_user(pframe->buf.addr_vir[2],&uparam->frame_addr_vir_array[i].v);
		if (ret) {
			pr_err("fail to copy_from_user\n");
			return -EFAULT;
		}
		pframe->channel_id = ch->ch_id;
		if (scene_mode == FDR_POST_DRC || scene_mode == FDR_POST_HIGH) {
			pframe->irq_property = CAM_FRAME_FDRH;
			isp_path_id = ch->isp_fdrh_path;
			isp_ctx_id = ch->isp_fdrh_ctx;
			fdr_ctrl = &ch->isp_scene_ctrl.fdrh_ctrl;
			ctrl_in.scene_type = CAM_SCENE_CTRL_FDR_H;
			isp_sw_ctx = module->isp_dev_handle->sw_ctx[ch->isp_ctx_id];
		} else {
			pframe->irq_property = CAM_FRAME_FDRL;
			isp_path_id = ch->isp_fdrl_path;
			isp_ctx_id = ch->isp_fdrl_ctx;
			fdr_ctrl = &ch->isp_scene_ctrl.fdrl_ctrl;
			ctrl_in.scene_type = CAM_SCENE_CTRL_FDR_L;
			if (module->cam_uinfo.raw_alg_type == RAW_ALG_MFNR) {
				pframe->irq_property = CAM_FRAME_RAW_PROC;
				ch_desc.raw_fmt = DCAM_RAW_14;
			}
		}

		ret = cam_buf_ionbuf_get(&pframe->buf);
		if (ret) {
			cam_queue_empty_frame_put(pframe);
			goto exit;
		}
		pfrm[i] = pframe;
	}
	dcam = module->aux_dcam_dev;
	if (dcam == NULL) {
		cam_buf_ionbuf_put(&pfrm[0]->buf);
		cam_queue_empty_frame_put(pfrm[0]);
		goto isp_proc;
	}

	dcam_sw_aux_ctx = &dcam->sw_ctx[module->offline_cxt_id];
	dcam_sw_aux_ctx->raw_alg_type = module->cam_uinfo.raw_alg_type;
	ret = module->dcam_dev_handle->dcam_pipe_ops->get_datactrl(dcam_sw_aux_ctx,
			&ctrl_in, &dcam_ctrl);
	if (ret) {
		pr_err("fail to get dcam data ctrl.\n");
		goto exit;
	}

	if (dcam_ctrl.start_ctrl == DCAM_START_CTRL_DIS)
		goto isp_proc;
	dcam_sw_aux_ctx->pack_bits = DCAM_RAW_14;
	dcam_sw_aux_ctx->fetch.fmt = dcam_ctrl.in_format;

	if (dcam_ctrl.need_raw_path) {
		shutoff = 0;
		patharg.path_id = ch->aux_raw_path_id;
		module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_aux_ctx,
			DCAM_PATH_CFG_SHUTOFF, patharg.path_id, &shutoff);

		shutoff = 1;
		re_patharg.path_id = ch->aux_dcam_path_id;
		module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_aux_ctx,
			DCAM_PATH_CFG_SHUTOFF, re_patharg.path_id, &shutoff);
	} else {
		shutoff = 1;
		patharg.path_id = ch->aux_raw_path_id;
		module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_aux_ctx,
			DCAM_PATH_CFG_SHUTOFF, patharg.path_id, &shutoff);

		shutoff = 0;
		re_patharg.path_id = ch->aux_dcam_path_id;
		module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_aux_ctx,
			DCAM_PATH_CFG_SHUTOFF, re_patharg.path_id, &shutoff);
	}

	if (!dcam_ctrl.need_raw_path) {
		ch_desc.bayer_pattern = module->cam_uinfo.sensor_if.img_ptn;
		ch_desc.endian.y_endian = ENDIAN_LITTLE;
		ch_desc.is_4in1 = module->cam_uinfo.is_4in1;
		ch_desc.dcam_out_fmt = dcam_ctrl.out_format;
		ch_desc.dcam_out_bits = DCAM_STORE_10_BIT;
		ch_desc.pyr_data_bits = DCAM_STORE_10_BIT;
		ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(&dcam->sw_ctx[module->offline_cxt_id],
			DCAM_PATH_CFG_BASE, ch->aux_dcam_path_id, &ch_desc);
		if (ret) {
			pr_err("fail to cfg dcam base cfg.\n");
			goto exit;
		}
	}

	if (dcam_ctrl.callback_ctrl == DCAM_CALLBACK_CTRL_ISP && dcam_ctrl.in_format != DCAM_STORE_FRGB) {
		pfrm_dcam = pfrm[1];
		pfrm_isp = pfrm[2];
		if (module->cam_uinfo.raw_alg_type == RAW_ALG_FDR_V1)
			pfrm_isp = pfrm[2];
	} else {
		if (module->cam_uinfo.raw_alg_type == RAW_ALG_MFNR) {
			pfrm_dcam = pfrm[1];
			pfrm_isp = pfrm[2];
		} else {
			pfrm_dcam = pfrm[2];
			pfrm_isp = pfrm[1];
		}
	}

	if (scene_mode == FDR_POST_HIGH) {
		ch_desc.input_size.w = ch->ch_uinfo.src_size.w;
		ch_desc.input_size.h = ch->ch_uinfo.src_size.h;
		ch_desc.input_trim.size_x = ch->ch_uinfo.src_size.w;
		ch_desc.input_trim.size_y = ch->ch_uinfo.src_size.h;
		if (module->cam_uinfo.raw_alg_type != RAW_ALG_FDR_V1) {
			ch_desc.input_trim.start_x = ch->trim_dcam.start_x;
			ch_desc.input_trim.start_y = ch->trim_dcam.start_y;
			ch_desc.input_trim.size_x = ch->trim_dcam.size_x;
			ch_desc.input_trim.size_y = ch->trim_dcam.size_y;
		}
		ch_desc.output_size.w = ch_desc.input_trim.size_x;
		ch_desc.output_size.h = ch_desc.input_trim.size_y;
		ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_aux_ctx,
			DCAM_PATH_CFG_SIZE, ch->aux_dcam_path_id, &ch_desc);
	}

	if (dcam_ctrl.need_raw_path)
		ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(&dcam->sw_ctx[module->offline_cxt_id],
			DCAM_PATH_CFG_OUTPUT_BUF, ch->aux_raw_path_id, pfrm_dcam);
	else {
		pfrm_dcam->need_pyr_dec = 1;
		pfrm_dcam->pyr_status = OFFLINE_DEC_ON;
		ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(&dcam->sw_ctx[module->offline_cxt_id],
			DCAM_PATH_CFG_OUTPUT_BUF, ch->aux_dcam_path_id, pfrm_dcam);
	}
	if (ret) {
		pr_err("fail to cfg dcam out buffer.\n");
		goto exit;
	}

	if (dcam_ctrl.callback_ctrl == DCAM_CALLBACK_CTRL_USER) {
		cam_buf_ionbuf_put(&pfrm_isp->buf);
		cam_queue_empty_frame_put(pfrm_isp);
	} else {
		struct isp_path_base_desc path_desc;
		memset(&path_desc, 0, sizeof(path_desc));
		path_desc.out_fmt = fdr_ctrl->out_format;
		path_desc.endian.y_endian = ENDIAN_LITTLE;
		path_desc.endian.uv_endian = ENDIAN_LITTLE;
		path_desc.output_size.w = fdr_ctrl->dst.w;
		path_desc.output_size.h = fdr_ctrl->dst.h;
		path_desc.data_bits = DCAM_STORE_10_BIT;
		pfrm_isp->width = fdr_ctrl->dst.w;
		pfrm_isp->height = fdr_ctrl->dst.h;
		pr_debug("w:%d, h:%d.\n", dst_size.w, dst_size.h);
		if (dst_size.w && dst_size.h) {
			path_desc.output_size.w = dst_size.w;
			path_desc.output_size.h = dst_size.h;
			pfrm_isp->width = dst_size.w;
			pfrm_isp->height = dst_size.h;
		}
		ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
			ISP_PATH_CFG_PATH_BASE,
			isp_ctx_id, isp_path_id, &path_desc);
		ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
			ISP_PATH_CFG_OUTPUT_BUF, isp_ctx_id, isp_path_id, pfrm_isp);
		if (ret) {
			pr_err("fail to cfg isp out buffer.\n");
			goto exit;
		}
	}

	ret = camcore_frame_start_proc(module, pfrm[0]);
	pr_info("scene %d, frm fd (%d 0x%x), (%d 0x%x), (%d 0x%x)\n", scene_mode,
		pfrm[0]->buf.mfd[0], pfrm[0]->buf.offset[0],
		pfrm[1]->buf.mfd[0], pfrm[1]->buf.offset[0],
		pfrm[2]->buf.mfd[0], pfrm[2]->buf.offset[0], ctrl_in.scene_type);

	mutex_unlock(&module->fdr_lock);
	return 0;

isp_proc:
	pfrm[1]->width = fdr_ctrl->dst.w;
	pfrm[1]->height = fdr_ctrl->dst.h;
	ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
			ISP_PATH_CFG_OUTPUT_BUF,
			isp_ctx_id, isp_path_id, pfrm[1]);
	if (ret) {
		pr_err("fail to cfg isp out buffer.\n");
		goto exit;
	}

	pframe = pfrm[0];
	pr_info("fdr %d , isp path 0x%x ctx_id 0x%x\n", pframe->irq_property, isp_path_id, isp_ctx_id);
	ret = module->isp_dev_handle->isp_ops->proc_frame(module->isp_dev_handle, pframe,
		isp_ctx_id);

	pr_info("scene %d, frm fd (%d 0x%x), (%d 0x%x), (%d 0x%x)\n", scene_mode,
		pfrm[0]->buf.mfd[0], pfrm[0]->buf.offset[0],
		pfrm[1]->buf.mfd[0], pfrm[1]->buf.offset[0],
		pfrm[2]->buf.mfd[0], pfrm[2]->buf.offset[0]);

	if (pfrm[2]->buf.mfd[0] == 0)
		cam_queue_empty_frame_put(pfrm[2]);
	mutex_unlock(&module->fdr_lock);
	return 0;

exit:
	for (i = 0; i < 3; i++) {
		if (pfrm[i]) {
			cam_buf_ionbuf_put(&pfrm[i]->buf);
			cam_queue_empty_frame_put(pfrm[i]);
		}
	}
	mutex_unlock(&module->fdr_lock);
	return ret;
}

/* set addr for 4in1 raw which need remosaic
 */
static int camioctl_4in1_raw_addr_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	enum dcam_id idx;
	struct sprd_img_parm __user *uparam;
	struct channel_context *ch;
	struct camera_frame *pframe;
	struct dcam_sw_context *dcam_sw_ctx = NULL;
	uint32_t channel_id, buffer_count, is_reserved_buf;
	int i, j;

	uparam = (struct sprd_img_parm __user *)arg;
	ret |= get_user(channel_id, &uparam->channel_id);
	ret |= get_user(buffer_count, &uparam->buffer_count);
	ret |= get_user(is_reserved_buf, &uparam->is_reserved_buf);
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}
	idx = module->idx;
	pr_debug("idx %d, channel_id %d, enable %d\n", idx,
		channel_id, module->channel[channel_id].enable);
	if ((channel_id >= CAM_CH_MAX) ||
		(buffer_count == 0) ||
		(module->channel[channel_id].enable == 0)) {
		pr_err("fail to get valid channel id %d. buf cnt %d\n",
			channel_id, buffer_count);
		return -EFAULT;
	}
	pr_info("ch %d, buffer_count %d\n", channel_id,
		buffer_count);

	ch = &module->channel[channel_id];
	dcam_sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];

	for (i = 0; i < buffer_count; i++) {
		pframe = cam_queue_empty_frame_get();
		pframe->buf.type = CAM_BUF_USER;
		ret |= get_user(pframe->buf.mfd[0],&uparam->fd_array[i]);
		ret |= get_user(pframe->buf.addr_vir[0],&uparam->frame_addr_vir_array[i].y);
		ret |= get_user(pframe->buf.addr_vir[1],&uparam->frame_addr_vir_array[i].u);
		ret |= get_user(pframe->buf.addr_vir[2],&uparam->frame_addr_vir_array[i].v);
		pframe->channel_id = ch->ch_id;
		pframe->width = ch->ch_uinfo.src_size.w;
		pframe->height = ch->ch_uinfo.src_size.h;
		pr_debug("mfd %d, reserved %d\n", pframe->buf.mfd[0],
			is_reserved_buf);
		ret = cam_buf_ionbuf_get(&pframe->buf);
		if (ret) {
			pr_err("fail to get ion buffer\n");
			cam_queue_empty_frame_put(pframe);
			ret = -EFAULT;
			break;
		}

		if (is_reserved_buf) {
			ch->reserved_buf_fd = pframe->buf.mfd[0];
			pframe->is_reserved = 1;
			ch->res_frame = pframe;
			for (j = 0; j < 3; j++)
				pframe->buf.size[j] = cal_sprd_raw_pitch(ch->ch_uinfo.src_size.w, ch->ch_uinfo.dcam_raw_fmt)
					* ch->ch_uinfo.src_size.h;
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
				DCAM_PATH_CFG_OUTPUT_RESERVED_BUF,
				ch->dcam_path_id, pframe);
		} else {
			ret = module->dcam_dev_handle->dcam_pipe_ops->cfg_path(dcam_sw_ctx,
				DCAM_PATH_CFG_OUTPUT_BUF,
				ch->dcam_path_id, pframe);
		}

		if (ret) {
			pr_err("fail to set output buffer for ch%d.\n",
				ch->ch_id);
			cam_buf_ionbuf_put(&pframe->buf);
			cam_queue_empty_frame_put(pframe);
			ret = -EFAULT;
			break;
		}
	}
	pr_info("exit, ret = %d\n", ret);

	return ret;
}

/* buffer set to kernel after remosaic
 */
static int camioctl_4in1_post_proc(struct camera_module *module,
		unsigned long arg)
{
	struct sprd_img_parm __user *uparam;
	int ret = 0;
	int iommu_enable;
	struct channel_context *channel;
	struct camera_frame *pframe = NULL;
	uint32_t index, reserved1, reserved2, fd_array;
	int i;
	ktime_t sensor_time;

	if ((atomic_read(&module->state) != CAM_RUNNING)) {
		pr_info("cam%d state: %d\n", module->idx,
				atomic_read(&module->state));
		return -EFAULT;
	}

	uparam = (struct sprd_img_parm __user *)arg;
	ret |= get_user(reserved1, &uparam->reserved[1]);
	ret |= get_user(reserved2, &uparam->reserved[2]);
	ret |= get_user(fd_array, &uparam->fd_array[0]);
	ret |= get_user(index, &uparam->index);
	if (ret) {
		pr_err("fail to copy_from_user\n");
		return -EFAULT;
	}

	pr_info("E\n");
	/* about image data address
	 * fetch: from HAL, in param
	 * dcam out: alloc by kernel, share buf
	 */
	channel = &module->channel[CAM_CH_CAP];
	iommu_enable = module->iommu_enable;
	/* timestamp, reserved	([2]<<32)|[1]
	 * Attention: Only send back one time, maybe need some
	 * change when hal use another time
	 */
	sensor_time = reserved2;
	sensor_time <<= 32;
	sensor_time |= reserved1;
	/* get frame: 1:check id;2:check time;3:get first,4:get new */
	i = cam_queue_cnt_get(&module->remosaic_queue);
	while (i--) {
		pframe = cam_queue_dequeue(&module->remosaic_queue,
			struct camera_frame, list);
		/* check frame id */
		if (pframe == NULL)
			break;
		if (pframe->fid == index)
			break;
		if (pframe->boot_sensor_time == sensor_time)
			break;
		cam_queue_enqueue(&module->remosaic_queue, &pframe->list);
		pframe = NULL;
	}
	if (pframe == NULL) {
		pr_info("Can't find frame in the queue, get new one\n");
		pframe = cam_queue_empty_frame_get();
		pframe->boot_sensor_time = sensor_time;
		pframe->sensor_time = cam_ktime_to_timeval(ktime_sub(
			pframe->boot_sensor_time, ktime_sub(
			ktime_get_boottime(), ktime_get())));
		pframe->fid = index;
	}
	pframe->endian = ENDIAN_LITTLE;
	pframe->pattern = module->cam_uinfo.sensor_if.img_ptn;
	pframe->width = channel->ch_uinfo.src_size.w;
	pframe->height = channel->ch_uinfo.src_size.h;
	pframe->buf.type = CAM_BUF_USER;
	pframe->buf.mfd[0] = fd_array;
	ret |= get_user(pframe->buf.addr_vir[0],&uparam->frame_addr_vir_array[0].y);
	ret |= get_user(pframe->buf.addr_vir[1],&uparam->frame_addr_vir_array[0].u);
	ret |= get_user(pframe->buf.addr_vir[2],&uparam->frame_addr_vir_array[0].v);
	pframe->channel_id = channel->ch_id;

	ret = cam_buf_ionbuf_get(&pframe->buf);
	if (ret) {
		cam_queue_empty_frame_put(pframe);
		return -EFAULT;
	}
	pr_info("frame[%d] fd %d addr_vir[0]=0x%lx iova[0]=0x%lx\n",
		pframe->fid, pframe->buf.mfd[0], pframe->buf.addr_vir[0],
		pframe->buf.iova[0]);

	ret = camcore_frame_start_proc(module, pframe);
	if (ret)
		pr_err("fail to start dcam for raw proc\n");

	return ret;
}

/* set which channel use hw 3dnr
 * if auto 3dnr, will be set when previewing
 */
static int camioctl_3dnr_mode_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct sprd_img_3dnr_mode parm;
	struct channel_context *ch;
	uint32_t ch_id;

	if (!module) {
		pr_err("fail to get valid param\n");
		return -EINVAL;
	}

	memset((void *)&parm, 0, sizeof(parm));
	ret = copy_from_user(&parm, (void __user *)arg,
		sizeof(parm));
	if (ret) {
		pr_err("fail to get user info ret %d\n", ret);
		return -EFAULT;
	}
	ch_id = parm.channel_id;
	if (ch_id >= CAM_CH_MAX) {
		pr_err("fail to get channel id %d\n", ch_id);
		return -EFAULT;
	}
	ch = &module->channel[ch_id];
	pr_info("ch_id %d, need_3dnr %d\n", ch_id, parm.need_3dnr);

	/* dynamic set when auto 3dnr */
	if (module->auto_3dnr) {
		if (ch->uinfo_3dnr == parm.need_3dnr)
			return ret;

		ch->uinfo_3dnr = parm.need_3dnr;
		camcore_capture_3dnr_set(module, ch);

	} else {
		ch->uinfo_3dnr = parm.need_3dnr;
	}

	return ret;
}

/* set auto_3dnr enable bit to drv
 * 190614: hal set 1 if need, but not set 0(default 0)
 */
static int camioctl_auto_3dnr_mode_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct sprd_img_auto_3dnr_mode parm;

	if (!module) {
		pr_err("fail to get valid param\n");
		return -EINVAL;
	}

	memset((void *)&parm, 0, sizeof(parm));
	ret = copy_from_user(&parm, (void __user *)arg,
				sizeof(parm));
	if (ret) {
		pr_err("fail to get user info ret %d\n", ret);
		return -EFAULT;
	}
	module->auto_3dnr = parm.auto_3dnr_enable;
	pr_info("auto_3dnr %d\n", module->auto_3dnr);

	return ret;
}

static int camioctl_dewarp_otp_set(struct camera_module *module, unsigned long arg)
{
	int ret = 0;

	if (!module) {
		pr_err("fail to get valid param\n");
		return -EINVAL;
	}

	/* wait dewarp sensor opt data format */
	return ret;

}

static int camioctl_capability_get(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct sprd_img_size size = {0};

	if (!module) {
		pr_err("fail to get valid param\n");
		ret = -EINVAL;
		goto exit;
	}

	size.w = ISP_WIDTH_MAX;
	size.h = ISP_HEIGHT_MAX;
	ret = copy_to_user((void __user *)arg, &size,
		sizeof(struct sprd_img_size));
	if (unlikely(ret)) {
		pr_err("fail to get capability, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

exit:
	return ret;
}

static int camioctl_scaler_capability_get(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	uint32_t isp_scaler_up_max = ISP_SCALER_UP_MAX;

	if (!module) {
		pr_err("fail to get valid param\n");
		ret = -EINVAL;
		goto exit;
	}

	ret = copy_to_user((void __user *)arg, &isp_scaler_up_max, sizeof(uint32_t));
	if (unlikely(ret)) {
		pr_err("fail to get SCALER capability, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}
	pr_debug("isp_scaler_up_max: %d\n", isp_scaler_up_max);

exit:
	return ret;
}

static int camioctl_dewarp_hw_capability_get(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct camera_group *grp = NULL;
	struct cam_hw_info *hw = NULL;
	uint32_t isp_dewarp_hw_support = 0;

	if (!module) {
		pr_err("fail to get valid param\n");
		ret = -EINVAL;
		goto exit;
	}
	grp = module->grp;
	hw = grp->hw_info;
	isp_dewarp_hw_support = hw->ip_isp->dewarp_support;
	ret = copy_to_user((void __user *)arg, &isp_dewarp_hw_support, sizeof(uint32_t));
	if (unlikely(ret)) {
		pr_err("fail to get dewarp hw capability, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}
	pr_debug("isp_dewarp_hw_support: %d\n", isp_dewarp_hw_support);

exit:
	return ret;
}

static int camioctl_path_rect_get(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct sprd_img_path_rect parm;

	if (!module) {
		pr_err("fail to get valid param\n");
		return -EINVAL;
	}

	memset((void *)&parm, 0, sizeof(parm));
	ret = copy_from_user(&parm, (void __user *)arg,
		sizeof(struct sprd_img_path_rect));
	if (ret) {
		pr_err("fail to get user info ret %d\n", ret);
		return -EFAULT;
	}

	ret = module->dcam_dev_handle->dcam_pipe_ops->ioctl(&module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id],
		DCAM_IOCTL_GET_PATH_RECT, &parm);
	pr_debug("TRIM rect info x %d y %d w %d h %d\n",
		parm.trim_valid_rect.x, parm.trim_valid_rect.y,
		parm.trim_valid_rect.w, parm.trim_valid_rect.h);
	pr_debug("AE rect info x %d y %d w %d h %d\n",
		parm.ae_valid_rect.x, parm.ae_valid_rect.y,
		parm.ae_valid_rect.w, parm.ae_valid_rect.h);
	pr_debug("AF rect info x %d y %d w %d h %d\n",
		parm.af_valid_rect.x, parm.af_valid_rect.y,
		parm.af_valid_rect.w, parm.af_valid_rect.h);

	ret = copy_to_user((void __user *)arg, &parm,
		sizeof(struct sprd_img_path_rect));
	if (ret) {
		ret = -EFAULT;
		pr_err("fail to copy to user\n");
		return ret;
	}

	return ret;
}

static int camioctl_csi_switch(struct camera_module *module, unsigned long arg)
{
	int ret = 0;
	uint32_t csi_connect = 0;
	uint32_t switch_mode = CAM_CSI_NORMAL_SWITCH;
	struct dcam_sw_context *sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];

	ret = copy_from_user(&csi_connect, (void __user *)arg, sizeof(uint32_t));
	if (unlikely(ret)) {
		pr_err("fail to get capability, ret %d\n", ret);
		return -EFAULT;
	}

	switch (csi_connect) {
		case 0:
			sw_ctx->csi_connect_stat = DCAM_CSI_PAUSE;
			ret = camcore_csi_switch_disconnect(module, switch_mode);
			break;
		case 1:
			sw_ctx->csi_connect_stat = DCAM_CSI_RESUME;
			ret = camcore_csi_switch_connect(module, switch_mode);
			break;
		default:
			pr_err("fail to get vailed csi_connect: %d\n", csi_connect);
			break;
	}

	return ret;
}

static int camioctl_path_pause(struct camera_module *module,
	unsigned long arg)
{
	int ret = 0;
	uint32_t dcam_path_state = DCAM_PATH_PAUSE;
	struct camera_frame *pframe = NULL;
	struct dcam_sw_context *sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];
	struct dcam_path_desc *path = NULL;

	pr_debug("pause cam%d with csi_id %d ,sw_ctx_id = %d\n",
			module->idx, module->dcam_idx, sw_ctx->sw_ctx_id);

	module->path_state = dcam_path_state;

	if (atomic_read(&module->state) == CAM_RUNNING) {
		mutex_lock(&module->lock);
		if (module->channel[CAM_CH_CAP].enable) {
			ret = module->isp_dev_handle->isp_ops->clear_blk_param_q(module->isp_dev_handle, module->channel[CAM_CH_CAP].isp_ctx_id);
			if (ret)
				pr_err("fail to recycle cam%d  blk param node\n", module->idx);
			if (module->cam_uinfo.need_share_buf) {
				module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
					DCAM_PATH_CFG_STATE, DCAM_PATH_FULL, &dcam_path_state);
				module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
					DCAM_PATH_CLR_OUTPUT_SHARE_BUF,
					DCAM_PATH_FULL, &dcam_path_state);
				do {
					pframe = cam_queue_dequeue(&module->channel[CAM_CH_CAP].share_buf_queue,
						struct camera_frame, list);
					if (pframe == NULL)
						break;
					camcore_share_buf_cfg(SHARE_BUF_SET_CB, pframe, module);
				} while (pframe);
			} else {
				path = &sw_ctx->path[module->channel[CAM_CH_CAP].dcam_path_id];
				do {
					pframe = cam_queue_dequeue(&module->channel[CAM_CH_CAP].share_buf_queue,
						struct camera_frame, list);
					if (pframe == NULL)
						break;
					module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
						DCAM_PATH_CFG_OUTPUT_BUF, path->path_id, pframe);
				} while (pframe);
			}
		}
		mutex_unlock(&module->lock);
	}

	return ret;
}

static int camioctl_path_resume(struct camera_module *module,
	unsigned long arg)
{
	int ret = 0;
	uint32_t dcam_path_state = DCAM_PATH_RESUME;
	struct dcam_sw_context *sw_ctx = &module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id];

	pr_debug("resume cam%d with csi_id %d , sw_ctx_id = %d\n",
			module->idx, module->dcam_idx, sw_ctx->sw_ctx_id);

	module->path_state = dcam_path_state;

	if (atomic_read(&module->state) == CAM_RUNNING) {
		mutex_lock(&module->lock);
		if (module->channel[CAM_CH_CAP].enable)
			module->dcam_dev_handle->dcam_pipe_ops->cfg_path(sw_ctx,
				DCAM_PATH_CFG_STATE,DCAM_PATH_FULL, &dcam_path_state);
		mutex_unlock(&module->lock);
	}

	return ret;
}

static int camioctl_cam_test(struct camera_module *module, unsigned long arg)
{
	int ret = 0;
	struct cam_hw_info *hw = NULL;

	hw = module->grp->hw_info;
	ret = camt_test(hw, arg);

	return ret;
}

static int camioctl_longexp_mode_set(struct camera_module *module, unsigned long arg)
{
	int ret = 0;
	struct sprd_img_longexp_mode param = {0};

	if (!module) {
		pr_err("fail to get input ptr %p\n", module);
		return -EINVAL;
	}

	ret = copy_from_user(&param, (void __user *)arg, sizeof(param));
	if (ret) {
		pr_err("fail to get longexp mode %d\n", ret);
		return -EFAULT;
	}

	module->cam_uinfo.is_longexp = param.need_longexp;
	pr_debug("is_longexp %d\n", param.need_longexp);

	return ret;
}

static int camioctl_mul_max_sensor_size_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct sprd_img_size *dst = NULL;
	struct camera_group *grp = NULL;

	grp = module->grp;
	dst = &grp->mul_sn_max_size;

	ret = copy_from_user(dst, (void __user *)arg, sizeof(struct sprd_img_size));

	pr_debug("mul sensor_size %d %d\n", dst->w, dst->h);
	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

exit:
	return ret;
}

static int camioctl_cap_zsl_info_set(struct camera_module *module,
		unsigned long arg)
{
	int ret = 0;
	struct sprd_cap_zsl_param param = {0};

	ret = copy_from_user(&param, (void __user *)arg, sizeof(struct sprd_cap_zsl_param));
	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		ret = -EFAULT;
		goto exit;
	}

	pr_debug("cam %d, zsl num %d zsl skip num %d is share buf %d\n", module->idx, param.zsl_num,
		param.zsk_skip_num, param.need_share_buf);

	module->cam_uinfo.zsl_num = param.zsl_num;
	module->cam_uinfo.zsk_skip_num = param.zsk_skip_num;
	module->cam_uinfo.need_share_buf = param.need_share_buf;

exit:
	return ret;
}

static int camioctl_dcam_raw_fmt_set(struct camera_module *module,unsigned long arg)
{
	int ret = 0;
	struct sprd_dcam_raw_fmt param;
	struct cam_hw_info *hw = NULL;
	struct channel_context *channel = NULL;
	uint32_t dcam_raw_update = 0, sensor_raw_update = 0, dcam_out_update = 0;
	uint32_t i = 0;

	ret = copy_from_user(&param, (void __user *)arg, sizeof(struct sprd_dcam_raw_fmt));
	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		return -EFAULT;
	}

	hw = module->grp->hw_info;
	channel = &module->channel[param.ch_id];
	if (param.ch_id == CAM_CH_DCAM_VCH){
		channel->ch_uinfo.sensor_raw_fmt = param.sensor_raw_fmt;
		return 0;
	}

	if (param.ch_id > CAM_CH_CAP && param.ch_id != CAM_CH_VIRTUAL) {
		pr_err("fail to check param, ch%d\n", param.ch_id);
		return -EFAULT;
	}
	if ((param.dcam_raw_fmt >= DCAM_RAW_MAX) && (param.sensor_raw_fmt >= DCAM_RAW_MAX)) {
		pr_err("fail to check param, sensor raw fmt %d, dcam raw fmt %d, ch%d\n",
			param.sensor_raw_fmt, param.dcam_raw_fmt, param.ch_id);
		return -EFAULT;
	}

	if (channel->enable == 0) {
		pr_err("ch%d not enable\n", param.ch_id);
		return -EFAULT;
	}

	for (i = 0; i < DCAM_RAW_MAX; i++) {
		if (hw->ip_dcam[0]->raw_fmt_support[i] == DCAM_RAW_MAX)
			break;
		if (param.dcam_raw_fmt == hw->ip_dcam[0]->raw_fmt_support[i]) {
			channel->ch_uinfo.dcam_raw_fmt = param.dcam_raw_fmt;
			dcam_raw_update = 1;
		}
	}

	for (i = 0; i < DCAM_RAW_MAX; i++) {
		if (hw->ip_dcam[0]->raw_fmt_support[i] == DCAM_RAW_MAX)
			break;
		if (param.sensor_raw_fmt == hw->ip_dcam[0]->raw_fmt_support[i]) {
			channel->ch_uinfo.sensor_raw_fmt = param.sensor_raw_fmt;
			module->raw_cap_fetch_fmt = param.sensor_raw_fmt;
			sensor_raw_update = 1;
		}
	}

	for (i = 0; i < DCAM_STORE_BIT_MAX; i++) {
		if (hw->ip_dcam[0]->dcam_output_support[i] == DCAM_STORE_BIT_MAX)
			break;
		if (param.dcam_output_bit == hw->ip_dcam[0]->dcam_output_support[i]) {
			channel->ch_uinfo.dcam_output_bit = param.dcam_output_bit;
			dcam_out_update = 1;
		}
	}

	if (dcam_raw_update)
		pr_info("cam%d, ch%d set dcam raw fmt %d\n", module->idx, param.ch_id, channel->ch_uinfo.dcam_raw_fmt);
	if (sensor_raw_update)
		pr_info("cam%d, ch%d set sensor raw fmt %d\n", module->idx, param.ch_id, channel->ch_uinfo.sensor_raw_fmt);
	if (dcam_out_update)
		pr_info("cam%d, ch%d set dcam_output_bit %d\n", module->idx, param.ch_id, channel->ch_uinfo.dcam_output_bit);
 	if ((!dcam_raw_update) && (!sensor_raw_update)) {
		pr_err("fail to support raw fmt, not support, %d %d\n", param.sensor_raw_fmt, param.dcam_raw_fmt);
		return -EFAULT;
	} else
		return 0;

}

static int camioctl_key_set(struct camera_module *module, unsigned long arg)
{
	int ret = 0;
	uint32_t param = 0;
	ret = copy_from_user(&param, (void __user *)arg, sizeof(uint32_t));
	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		return -EFAULT;
	}
	if (param == CAM_IOCTL_PRIVATE_KEY)
		module->private_key = 1;
	pr_info("cam%d get ioctl permission %d\n", module->idx, module->private_key);
	return 0;
}

static int camioctl_960fps_param_set(struct camera_module *module, unsigned long arg)
{
	struct sprd_slowmotion_960fps_param param;
	struct channel_context *channel = NULL;
	int ret = 0;
	int isp_ctx_id = 0;
	int isp_path_id = 0;
	ret = copy_from_user(&param, (void __user *)arg, sizeof(struct sprd_slowmotion_960fps_param));
	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		return -EFAULT;
	}

	if (param.ch_id > CAM_CH_CAP && param.ch_id != CAM_CH_VIRTUAL) {
		pr_err("fail to check param, ch%d\n", param.ch_id);
		return -EFAULT;
	}
	channel = &module->channel[param.ch_id];
	if (channel->enable == 0) {
		pr_err("ch%d not enable\n", param.ch_id);
		return -EFAULT;
	}
	isp_ctx_id = channel->isp_ctx_id;
	isp_path_id = ISP_SPATH_VID;
	channel->ch_uinfo.frame_num = param.stage_a_frm_num;
	channel->ch_uinfo.frame_num1 = param.stage_b_frm_num;
	channel->ch_uinfo.high_fps_skip_num1= param.stage_a_valid_frm_num;
	if (channel->ch_uinfo.is_high_fps && (isp_ctx_id >= 0)) {
		struct isp_ctx_base_desc slw_desc;
		memset(&slw_desc, 0, sizeof(struct isp_ctx_base_desc));
		slw_desc.slowmotion_stage_a_num = channel->ch_uinfo.frame_num;
		slw_desc.slowmotion_stage_a_valid_num = channel->ch_uinfo.high_fps_skip_num1;
		slw_desc.slowmotion_stage_b_num = channel->ch_uinfo.frame_num1;
		ret = module->isp_dev_handle->isp_ops->cfg_path(module->isp_dev_handle,
			ISP_PATH_CFG_PATH_SLW, isp_ctx_id, isp_path_id, &slw_desc);
	}
	pr_info("cam%d ch%d, (%d/8 %d num), (8/8 %d num)\n",
		module->idx, param.ch_id, channel->ch_uinfo.high_fps_skip_num1,
		channel->ch_uinfo.frame_num, channel->ch_uinfo.frame_num1);
	return 0;
}

static int camioctl_cfg_param_start_end(struct camera_module *module, unsigned long arg)
{
	int ret = 0;
	uint32_t for_fdr = 0, for_capture = 0;
	struct sprd_cfg_param_status param;
	struct isp_sw_context *pctx = NULL;
	struct dcam_sw_context *sw_ctx = NULL;
	struct channel_context *channel = NULL;
	struct camera_frame *param_frame = NULL;

	if (atomic_read(&module->state) != CAM_CFG_CH && atomic_read(&module->state) != CAM_STREAM_ON &&atomic_read(&module->state) != CAM_RUNNING)
		return ret;

	ret = copy_from_user(&param, (void __user *)arg, sizeof(struct sprd_cfg_param_status));
	if (unlikely(ret)) {
		pr_err("fail to copy from user, ret %d\n", ret);
		return -EFAULT;
	}

	if ((param.scene_id == PM_SCENE_FDRL) ||
		(param.scene_id == PM_SCENE_FDRH) ||
		(param.scene_id == PM_SCENE_FDR_PRE) ||
		(param.scene_id == PM_SCENE_FDR_DRC))
		for_fdr = 1;
	for_capture = (param.scene_id == PM_SCENE_CAP ? 1 : 0) | for_fdr;

	if (for_capture && (module->channel[CAM_CH_CAP].enable == 0)) {
		pr_warn("warn: cam%d cap channel not enable\n");
		return 0;
	}

	if (param.status == 0)
		pr_debug("cam%d start param, fid %d, scene %d\n", module->idx, param.frame_id, param.scene_id);
	else
		pr_debug("cam%d end param, fid %d, scene %d\n", module->idx, param.frame_id, param.scene_id);
	if (param.status == 0) {
		if (module->isp_dev_handle->cfg_handle) {
			if (param.scene_id == PM_SCENE_PRE) {
				channel = &module->channel[CAM_CH_PRE];
				if (channel->enable == 0)
					return 0;
				pctx = module->isp_dev_handle->sw_ctx[channel->isp_ctx_id];
				param_frame = cam_queue_empty_blk_param_get(&pctx->param_share_queue);
				if (param_frame) {
					pctx->isp_receive_param = param_frame;
				} else {
					pr_err("fail to recive param, cam%d scene %d fid %d\n", module->idx, param.scene_id, param.frame_id);
					return -EFAULT;
				}
			}
		}

		if (module->cam_uinfo.is_raw_alg && param.scene_id == PM_SCENE_OFFLINE_CAP && module->cam_uinfo.param_frame_sync) {
			sw_ctx = &module->dcam_dev_handle->sw_ctx[module->offline_cxt_id];
			param_frame = cam_queue_dequeue(&sw_ctx->blk_param_queue, struct camera_frame, list);
			if (param_frame) {
				pr_info("frame id:%d,%p.\n", param.frame_id, param_frame->pm);
				param_frame->fid = param.frame_id;
				sw_ctx->pm = param_frame->pm;
				cam_queue_enqueue(&sw_ctx->blk_param_queue, &param_frame->list);
			} else
				pr_debug("no frame in dcam blk empty queue:%d.\n", sw_ctx->blk_param_queue.cnt);
		}
	}

	if (param.status) {
		if (module->isp_dev_handle->cfg_handle) {
			if (param.scene_id == PM_SCENE_PRE) {
				channel = &module->channel[CAM_CH_PRE];
				if (channel->enable == 0)
					return 0;
				pctx = module->isp_dev_handle->sw_ctx[channel->isp_ctx_id];
				param_frame = pctx->isp_receive_param;
				if (!param_frame)
					return 0;
				/*give up current param and no longer receive new param after csi disconnect*/
				if ((module->dcam_dev_handle->sw_ctx[module->cur_sw_ctx_id].hw_ctx_id == DCAM_HW_CONTEXT_MAX) && param.frame_id > 0) {
					ret = cam_queue_recycle_blk_param(&pctx->param_share_queue, param_frame);
					pctx->isp_receive_param = NULL;
					return ret;
				}
				if (param.update)
					param_frame->blkparam_info.update = 1;
				else
					param_frame->blkparam_info.update = 0;
				param_frame->fid = param.frame_id;
				ret = cam_queue_enqueue(&pctx->param_buf_queue, &param_frame->list);
				pctx->isp_receive_param = NULL;
			} else {
				channel = &module->channel[CAM_CH_CAP];
				if (channel->enable == 0)
					return 0;
				pctx = module->isp_dev_handle->sw_ctx[channel->isp_ctx_id];
				param_frame = cam_queue_empty_blk_param_get(&pctx->param_share_queue);
				if (!param_frame) {
					mutex_lock(&pctx->blkpm_q_lock);
					param_frame = cam_queue_dequeue(&pctx->param_buf_queue, struct camera_frame, list);
					mutex_unlock(&pctx->blkpm_q_lock);
					if (param_frame)
						pr_debug("isp%d deq param node %d\n", channel->isp_ctx_id, param_frame->fid);
				}

				if (param_frame) {
					memcpy(param_frame->blkparam_info.param_block, &pctx->isp_k_param, sizeof(struct isp_k_block));
					if (param.update)
						param_frame->blkparam_info.update = 1;
					else
						param_frame->blkparam_info.update = 0;
					param_frame->fid = param.frame_id;
					ret = cam_queue_enqueue(&pctx->param_buf_queue, &param_frame->list);
				} else
					pr_warn("isp%d cap lose param %d\n", pctx->ctx_id, param.frame_id);
			}
		}
	}
	return ret;
}
#endif
