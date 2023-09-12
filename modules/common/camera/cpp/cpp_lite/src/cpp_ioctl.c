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

#ifdef CPP_IOCTL

typedef int (*cpp_io_func)(struct cpp_device *dev, unsigned long arg);

struct cpp_io_ctl_func {
	unsigned int cmd;
	cpp_io_func io_ctrl;
};

struct cpp_io_ctrl_descr {
	uint32_t ioctl_val;
	char *ioctl_str;
};

static int cppcore_ioctl_open_rot(struct cpp_device *dev,
	unsigned long arg)
{
	int ret = 0;

	if (arg)
		ret = copy_to_user((int *)arg, &ret,
		sizeof(ret));
	if (ret) {
		pr_err("fail to open rot drv");
		ret = -EFAULT;
		goto rot_open_exit;
	}

rot_open_exit:

	return ret;
}

static int cppcore_ioctl_start_rot(struct cpp_device *dev,
	unsigned long arg)
{
	int ret = 0;
	int timeleft = 0;
	struct cpp_pipe_dev *cppif = NULL;
	struct cpp_hw_soc_info *soc_cpp = NULL;
	struct rotif_device *rotif = NULL;
	struct sprd_cpp_rot_cfg_parm rot_parm;

	cppif = dev->cppif;
	soc_cpp= dev->hw_info->soc_cpp;
	if (!cppif) {
		pr_err("fail to get invalid cppif!\n");
		ret = -EINVAL;
		goto rot_start_exit;
	}
	rotif = cppif->rotif;
	if (!rotif) {
		pr_err("fail to get invalid rotif!\n");
		ret = -EINVAL;
		goto rot_start_exit;
	}
	mutex_lock(&rotif->rot_mutex);
	memset(&rot_parm, 0x00, sizeof(rot_parm));
	ret = copy_from_user(&rot_parm,
				(void __user *)arg, sizeof(rot_parm));
	if (unlikely(ret)) {
		pr_err("fail to get rot param form user, ret %d\n", ret);
		mutex_unlock(&rotif->rot_mutex);
		ret = -EFAULT;
		goto rot_start_exit;
	}

	ret = cppif->cppdrv_ops->ioctl(CPP_DRV_ROT_PARM_CHECK, &rot_parm, NULL);
	if (ret) {
		pr_err("fail to check rot parm\n");
		mutex_unlock(&rotif->rot_mutex);
		ret = -EFAULT;
		goto rot_start_exit;
	}
	rotif->drv_priv.iommu_src.dev = &dev->pdev->dev;
	rotif->drv_priv.iommu_dst.dev = &dev->pdev->dev;

	ret = cppif->cppdrv_ops->ioctl(CPP_DRV_ROT_Y_PARM_SET, &rot_parm, cppif);
	if (ret) {
		pr_err("fail to set rot y parm\n");
		mutex_unlock(&rotif->rot_mutex);
		ret = -EFAULT;
		goto rot_start_exit;
	}
	ret = cppif->cppdrv_ops->ioctl(CPP_DRV_ROT_START, cppif, NULL);
	if (ret) {
		pr_err("fail to start rot\n");
		mutex_unlock(&rotif->rot_mutex);
		ret = -EFAULT;
		goto rot_start_exit;
	}
	ret = cppif->cppdrv_ops->ioctl(CPP_DRV_ROT_REG_TRACE, cppif, NULL);
	if (ret) {
		pr_err("fail to trace rot reg\n");
		mutex_unlock(&rotif->rot_mutex);
		ret = -EFAULT;
		goto rot_start_exit;
	}
	if (!cppif->cppdrv_ops->ioctl(CPP_DRV_ROT_END, &rot_parm, NULL)) {
		timeleft = wait_for_completion_timeout(&rotif->done_com,
			msecs_to_jiffies(ROT_TIMEOUT));
		if (timeleft == 0) {
			ret = cppif->cppdrv_ops->ioctl(CPP_DRV_ROT_REG_TRACE, cppif, NULL);
			if (ret) {
				pr_err("fail to trace rot reg\n");
				mutex_unlock(&rotif->rot_mutex);
				ret = -EFAULT;
				goto rot_start_exit;
			}
			pr_err("fail to wait for rot path y int\n");
			ret = cppif->cppdrv_ops->ioctl(CPP_DRV_ROT_STOP, cppif, NULL);
			if (ret) {
				pr_err("fail to stop rot\n");
				mutex_unlock(&rotif->rot_mutex);
				ret = -EFAULT;
				goto rot_start_exit;
			}
			ret = cppif->cppdrv_ops->ioctl(CPP_DRV_ROT_RESET, dev->hw_info, soc_cpp);
			if (ret) {
				pr_err("fail to reset rot\n");
				mutex_unlock(&rotif->rot_mutex);
				ret = -EFAULT;
				goto rot_start_exit;
			}
			mutex_unlock(&rotif->rot_mutex);
			ret = -EBUSY;
			goto rot_start_exit;
		}
		ret = cppif->cppdrv_ops->ioctl(CPP_DRV_ROT_UV_PARM_SET, cppif, NULL);
		if (ret) {
			pr_err("fail to  set rot v parm\n");
			mutex_unlock(&rotif->rot_mutex);
			ret = -EFAULT;
			goto rot_start_exit;
		}
		ret = cppif->cppdrv_ops->ioctl(CPP_DRV_ROT_START, cppif, NULL);
		if (ret) {
			pr_err("fail to  start rot\n");
			mutex_unlock(&rotif->rot_mutex);
			ret = -EFAULT;
			goto rot_start_exit;
		}
	}

	timeleft = wait_for_completion_timeout(&rotif->done_com,
		msecs_to_jiffies(ROT_TIMEOUT));
	if (timeleft == 0) {
		ret = cppif->cppdrv_ops->ioctl(CPP_DRV_ROT_REG_TRACE, cppif, NULL);
		if (ret) {
			pr_err("fail to trace rot reg\n");
			mutex_unlock(&rotif->rot_mutex);
			ret = -EFAULT;
			goto rot_start_exit;
		}
		ret = cppif->cppdrv_ops->ioctl(CPP_DRV_ROT_STOP, cppif, NULL);
		if (ret) {
			pr_err("fail to stop rot\n");
			mutex_unlock(&rotif->rot_mutex);
			ret = -EFAULT;
			goto rot_start_exit;
		}
		pr_err("fail to wait for rot path uv int\n");
		cppif->cppdrv_ops->ioctl(CPP_DRV_ROT_RESET, dev->hw_info, soc_cpp);
		if (ret) {
			pr_err("fail to reset rot\n");
			mutex_unlock(&rotif->rot_mutex);
			ret = -EFAULT;
			goto rot_start_exit;
		}
		mutex_unlock(&rotif->rot_mutex);
		ret = -EBUSY;
		goto rot_start_exit;
	}

	ret = cppif->cppdrv_ops->ioctl(CPP_DRV_ROT_STOP, cppif, NULL);
	if (ret) {
		pr_err("fail to stop rot\n");
		mutex_unlock(&rotif->rot_mutex);
		ret = -EFAULT;
		goto rot_start_exit;
	}
	cppif->cppdrv_ops->ioctl(CPP_DRV_ROT_RESET, dev->hw_info, soc_cpp);
	if (ret) {
		pr_err("fail to reset rot\n");
		mutex_unlock(&rotif->rot_mutex);
		ret = -EFAULT;
		goto rot_start_exit;
	}
	mutex_unlock(&rotif->rot_mutex);

rot_start_exit:
	CPP_TRACE("cpp rotation ret %d\n", ret);

	return ret;
}

static int cppcore_ioctl_open_scale(struct cpp_device *dev,
	unsigned long arg)
{
	int ret = 0;
	int cpp_dimension = 0;
	struct sprd_cpp_size s_sc_cap;

	memset(&s_sc_cap, 0x00, sizeof(s_sc_cap));
	ret = dev->cppif->cppdrv_ops->ioctl(CPP_DRV_SCL_MAX_SIZE_GET, &s_sc_cap.w, &s_sc_cap.h);
	if (ret) {
		pr_err("fail to get scl max size\n");
		ret = -EFAULT;
		goto open_scal_exit;
	}
	cpp_dimension = (s_sc_cap.h << 16) | s_sc_cap.w;
	if (arg)
		ret = copy_to_user((int *)arg, &cpp_dimension,
		sizeof(cpp_dimension));
	if (ret) {
		pr_err("fail to get max size form user");
		ret = -EFAULT;
		goto open_scal_exit;
	}

open_scal_exit:

	return ret;
}

static int cppcore_ioctl_scale_slice_param_adapt(struct cpp_hw_info *cpp_hw,
	struct sprd_cpp_scale_cfg_parm *sc_parm)
{
	int i = 0;
	int j = 0;
	int ret = 0;
	slice_drv_param_t *slice_param = NULL;
	slice_drv_deci_param_t *deci_param = NULL;
	slice_drv_scaler_path_param_t *scaler_path_param = NULL;
	slice_drv_bypass_path_param_t *bypass_path_param = NULL;
	slice_drv_output_t *output = NULL;
	struct sprd_cpp_scale_slice_parm *slice_param_1 = NULL;
	struct sprd_cpp_scale_deci *deci_param_1 = NULL;
	struct sprd_cpp_path0_scaler_path_parm *scaler_path_param_1 = NULL;
	struct sprd_cpp_path0_bypass_path_parm *bypass_path_param_1 = NULL;
	struct sprd_cpp_slice_output *output_1 = NULL;

	if (!cpp_hw || !sc_parm) {
		pr_err("fail to get invalid input param!\n");
		ret = -EINVAL;
		return ret;
	}
	if(cpp_hw->prj_id == CPP_R5P0 || cpp_hw->prj_id == CPP_R4P0 ||
		cpp_hw->prj_id == CPP_R3P0) {
		slice_param_1 = &sc_parm->slice_param_1;
		deci_param_1 = &sc_parm->slice_param_1.deci_param;
		scaler_path_param_1 = &sc_parm->slice_param_1.scaler_path_param;
		bypass_path_param_1 = &sc_parm->slice_param_1.bypass_path_param;
		output_1 = &sc_parm->slice_param_1.output;
		slice_param = &sc_parm->slice_param;
		deci_param = &sc_parm->slice_param.deci_param;
		scaler_path_param = &sc_parm->slice_param.scaler_path_param;
		bypass_path_param = &sc_parm->slice_param.bypass_path_param;
		output = &sc_parm->slice_param.output;
		slice_param_1->img_w = slice_param->img_w;
		slice_param_1->img_h = slice_param->img_h;
		slice_param_1->img_format = slice_param->img_format;
		slice_param_1->crop_en = slice_param->crop_en;
		slice_param_1->crop_start_x = slice_param->crop_start_x;
		slice_param_1->crop_start_y = slice_param->crop_start_y;
		slice_param_1->crop_width = slice_param->crop_width;
		slice_param_1->crop_height = slice_param->crop_height;
		slice_param_1->slice_w = slice_param->slice_w;
		deci_param_1->hor = deci_param->deci_x;
		deci_param_1->ver = deci_param->deci_y;
		scaler_path_param_1->trim_eb = scaler_path_param->trim_eb;
		scaler_path_param_1->trim_start_x = scaler_path_param->trim_start_x;
		scaler_path_param_1->trim_start_y = scaler_path_param->trim_start_y;
		scaler_path_param_1->trim_size_x = scaler_path_param->trim_size_x;
		scaler_path_param_1->trim_size_y = scaler_path_param->trim_size_y;
		scaler_path_param_1->scaler_en = scaler_path_param->scaler_en;
		scaler_path_param_1->scaler_init_phase_hor = scaler_path_param->scaler_init_phase_hor;
		scaler_path_param_1->scaler_des_size_x = scaler_path_param->scaler_des_size_x;
		scaler_path_param_1->scaler_des_size_y = scaler_path_param->scaler_des_size_y;
		scaler_path_param_1->scaler_des_pitch = scaler_path_param->scaler_des_pitch;
		scaler_path_param_1->scaler_output_format = scaler_path_param->scaler_output_format;
		bypass_path_param_1->bypass_eb = bypass_path_param->enable;
		bypass_path_param_1->trim_eb = bypass_path_param->trim_eb;
		bypass_path_param_1->trim_start_x = bypass_path_param->trim_size_x;
		bypass_path_param_1->trim_start_y = bypass_path_param->trim_size_y;
		bypass_path_param_1->trim_size_x = bypass_path_param->trim_size_x;
		bypass_path_param_1->trim_size_y = bypass_path_param->trim_size_y;
		bypass_path_param_1->bp_des_pitch = bypass_path_param->bp_des_pitch;
		output_1->slice_count = output->slice_count;
		for (i = 0;  i < 8; i++){
			for(j = 0; j < 8; j++){
				output_1->scaler_path_coef.y_hor_coef[i][j] =
					output->scaler_path_coef.y_hor_coef[i][j];
				output_1->scaler_path_coef.c_hor_coef[i][j] =
					output->scaler_path_coef.c_hor_coef[i][j];
			}
		}
		for (i = 0;  i < 9; i++){
			for(j = 0; j < 16; j++){
				output_1->scaler_path_coef.y_ver_coef[i][j] =
					output->scaler_path_coef.y_ver_coef[i][j];
				output_1->scaler_path_coef.c_ver_coef[i][j] =
					output->scaler_path_coef.c_ver_coef[i][j];
			}
		}
		for (i = 0; i < CPP_MAX_SLICE_NUM; i++) {
			output_1->hw_slice_param[i].path0_src_pitch = output->hw_slice_param[i].Path0_src_pitch;
			output_1->hw_slice_param[i].path0_src_offset_x = output->hw_slice_param[i].Path0_src_offset_x;
			output_1->hw_slice_param[i].path0_src_offset_y = output->hw_slice_param[i].Path0_src_offset_y;
			output_1->hw_slice_param[i].path0_src_width = output->hw_slice_param[i].Path0_src_width;
			output_1->hw_slice_param[i].path0_src_height = output->hw_slice_param[i].Path0_src_height;
			output_1->hw_slice_param[i].deci_param.hor = output->hw_slice_param[i].hor_deci;
			output_1->hw_slice_param[i].deci_param.ver = output->hw_slice_param[i].ver_deci;
			output_1->hw_slice_param[i].input_format = output->hw_slice_param[i].Input_format;
			output_1->hw_slice_param[i].sc_in_trim_src.h = output->hw_slice_param[i].Sc_in_trim_src_height;
			output_1->hw_slice_param[i].sc_in_trim_src.w = output->hw_slice_param[i].Sc_in_trim_src_width;
			output_1->hw_slice_param[i].sc_in_trim.h = output->hw_slice_param[i].Sc_in_trim_height;
			output_1->hw_slice_param[i].sc_in_trim.w = output->hw_slice_param[i].Sc_in_trim_width;
			output_1->hw_slice_param[i].sc_in_trim.x = output->hw_slice_param[i].Sc_in_trim_offset_x;
			output_1->hw_slice_param[i].sc_in_trim.y = output->hw_slice_param[i].Sc_in_trim_offset_y;
			output_1->hw_slice_param[i].sc_slice_in_width = output->hw_slice_param[i].Sc_slice_in_width;
			output_1->hw_slice_param[i].sc_slice_in_height = output->hw_slice_param[i].Sc_slice_in_height;
			output_1->hw_slice_param[i].sc_slice_out_width = output->hw_slice_param[i].Sc_full_out_width;
			output_1->hw_slice_param[i].sc_slice_out_height = output->hw_slice_param[i].Sc_slice_out_height;
			output_1->hw_slice_param[i].sc_full_in_width = output->hw_slice_param[i].Sc_full_in_width;
			output_1->hw_slice_param[i].sc_full_in_height = output->hw_slice_param[i].Sc_full_in_height;
			output_1->hw_slice_param[i].sc_full_out_width = output->hw_slice_param[i].Sc_full_out_width;
			output_1->hw_slice_param[i].sc_full_out_height = output->hw_slice_param[i].Sc_full_out_height;
			output_1->hw_slice_param[i].y_hor_ini_phase_int = output->hw_slice_param[i].y_hor_ini_phase_int;
			output_1->hw_slice_param[i].y_hor_ini_phase_frac = output->hw_slice_param[i].y_hor_ini_phase_frac;
			output_1->hw_slice_param[i].uv_hor_ini_phase_int = output->hw_slice_param[i].uv_hor_ini_phase_int;
			output_1->hw_slice_param[i].uv_hor_ini_phase_frac = output->hw_slice_param[i].uv_hor_ini_phase_frac;
			output_1->hw_slice_param[i].y_ver_ini_phase_int = output->hw_slice_param[i].y_ver_ini_phase_int;
			output_1->hw_slice_param[i].y_ver_ini_phase_frac = output->hw_slice_param[i].y_ver_ini_phase_frac;
			output_1->hw_slice_param[i].uv_ver_ini_phase_int = output->hw_slice_param[i].uv_ver_ini_phase_int;
			output_1->hw_slice_param[i].uv_ver_ini_phase_frac = output->hw_slice_param[i].uv_ver_ini_phase_frac;
			output_1->hw_slice_param[i].y_ver_tap = output->hw_slice_param[i].y_ver_tap;
			output_1->hw_slice_param[i].uv_ver_tap = output->hw_slice_param[i].uv_ver_tap;
			output_1->hw_slice_param[i].sc_out_trim_src.h = output->hw_slice_param[i].Sc_out_trim_src_height;
			output_1->hw_slice_param[i].sc_out_trim_src.w = output->hw_slice_param[i].Sc_out_trim_src_width;
			output_1->hw_slice_param[i].sc_out_trim.h = output->hw_slice_param[i].Sc_out_trim_height;
			output_1->hw_slice_param[i].sc_out_trim.w = output->hw_slice_param[i].Sc_out_trim_src_width;
			output_1->hw_slice_param[i].sc_out_trim.x = output->hw_slice_param[i].Sc_out_trim_offset_x;
			output_1->hw_slice_param[i].sc_out_trim.y = output->hw_slice_param[i].Sc_in_trim_offset_y;
			output_1->hw_slice_param[i].path0_sc_des_pitch = output->hw_slice_param[i].Path0_sc_des_pitch;
			output_1->hw_slice_param[i].path0_sc_des_offset_x = output->hw_slice_param[i].Path0_sc_des_offset_x;
			output_1->hw_slice_param[i].path0_sc_des_offset_y = output->hw_slice_param[i].Path0_sc_des_offset_y;
			output_1->hw_slice_param[i].path0_sc_des_width = output->hw_slice_param[i].Path0_sc_des_width;
			output_1->hw_slice_param[i].path0_sc_des_height = output->hw_slice_param[i].Path0_sc_des_height;
			output_1->hw_slice_param[i].path0_sc_output_format = output->hw_slice_param[i].Path0_sc_output_format;
			output_1->hw_slice_param[i].path0_bypass_path_en = output->hw_slice_param[i].path0_bypass_path_en;
			output_1->hw_slice_param[i].bypass_trim_src.h = output->hw_slice_param[i].bypass_trim_src_height;
			output_1->hw_slice_param[i].bypass_trim_src.w = output->hw_slice_param[i].bypass_trim_src_width;
			output_1->hw_slice_param[i].bypass_trim.h = output->hw_slice_param[i].bypass_trim_height;
			output_1->hw_slice_param[i].bypass_trim.w = output->hw_slice_param[i].bypass_trim_width;
			output_1->hw_slice_param[i].bypass_trim.x = output->hw_slice_param[i].bypass_trim_offset_x;
			output_1->hw_slice_param[i].bypass_trim.y = output->hw_slice_param[i].bypass_trim_offset_y;
			output_1->hw_slice_param[i].path0_bypass_des_pitch = output->hw_slice_param[i].Path0_bypass_des_pitch;
			output_1->hw_slice_param[i].path0_bypass_des_offset_x = output->hw_slice_param[i].Path0_bypass_des_offset_x;
			output_1->hw_slice_param[i].path0_bypass_des_offset_y = output->hw_slice_param[i].Path0_bypass_des_offset_y;
			output_1->hw_slice_param[i].path0_bypass_des_width = output->hw_slice_param[i].Path0_bypass_des_width;
			output_1->hw_slice_param[i].path0_bypass_des_height = output->hw_slice_param[i].Path0_bypass_des_height;
			output_1->hw_slice_param[i].path0_bypass_output_format = output->hw_slice_param[i].Path0_bypass_output_format;
		}
	} else {
		return ret;
	}
	return ret;
}

static int cppcore_ioctl_start_scale(struct cpp_device *dev,
	unsigned long arg)
{
	int i = 0;
	int timeleft = 0;
	int ret = 0;
	struct cpp_pipe_dev *cppif = NULL;
	struct sprd_cpp_scale_cfg_parm *sc_parm = NULL;
	struct scif_device *scif = NULL;
	struct cpp_hw_soc_info *soc_cpp = NULL;

	soc_cpp = dev->hw_info->soc_cpp;
	cppif = dev->cppif;
	if (!cppif) {
		pr_err("fail to get invalid cppif!\n");
		ret = -EINVAL;
		goto start_scal_exit;
	}
	scif = cppif->scif;
	if (!scif) {
		pr_err("fail to get valid scif!\n");
		ret = -EFAULT;
		goto start_scal_exit;
	}

	sc_parm = kzalloc(sizeof(struct sprd_cpp_scale_cfg_parm),
			GFP_KERNEL);
	if (sc_parm == NULL) {
		ret = -EFAULT;
		goto start_scal_exit;
	}

	mutex_lock(&scif->sc_mutex);

	ret = copy_from_user(sc_parm, (void __user *)arg,
			sizeof(struct sprd_cpp_scale_cfg_parm));
	if (ret) {
		pr_err("fail to get parm form user\n");
		mutex_unlock(&scif->sc_mutex);
		ret = -EFAULT;
		goto start_scal_exit;
	}
	ret = cppif->cppdrv_ops->ioctl(CPP_DRV_SCL_SUPPORT_INFO_GET, cppif, NULL);
	if (ret) {
		pr_err("fail to get scl support info\n");
		mutex_unlock(&scif->sc_mutex);
		ret = -EFAULT;
		goto start_scal_exit;
	}
	scif->drv_priv.iommu_src.dev = &dev->pdev->dev;
	scif->drv_priv.iommu_dst.dev = &dev->pdev->dev;
	if (cppif->hw_info->ip_cpp->bp_support ==1)
		scif->drv_priv.iommu_dst_bp.dev = &dev->pdev->dev;
	ret = cppcore_ioctl_scale_slice_param_adapt(dev->hw_info, sc_parm);
	if (ret) {
		pr_err("fail to get parm form user\n");
		mutex_unlock(&scif->sc_mutex);
		ret = -EFAULT;
		goto start_scal_exit;
	}

	ret = cppif->cppdrv_ops->ioctl(CPP_DRV_SCL_SL_STOP, cppif, NULL);
	if (ret) {
		pr_err("fail to stop scl\n");
		mutex_unlock(&scif->sc_mutex);
		ret = -EFAULT;
		goto start_scal_exit;
	}
	ret = cppif->cppdrv_ops->ioctl(CPP_DRV_SCL_EB, cppif, NULL);
	if (ret) {
		pr_err("fail to eb scl\n");
		mutex_unlock(&scif->sc_mutex);
		ret = -EFAULT;
		goto start_scal_exit;
	}
	sc_parm->slice_param.output.slice_count = sc_parm->slice_param_1.output.slice_count;
	do {
		if (cppif->hw_info->ip_cpp->slice_support==1) {
			ret =cppif->cppdrv_ops->ioctl(CPP_DRV_SCL_CFG_PARAM_SET, cppif, sc_parm);
			if (ret) {
				pr_err("fail to set scl param\n");
				mutex_unlock(&scif->sc_mutex);
				ret = -EFAULT;
				goto start_scal_exit;
			}
			ret = cppif->cppdrv_ops->ioctl(CPP_DRV_SCL_SLICE_PARAM_SET, cppif,
				&sc_parm->slice_param_1.output.hw_slice_param[i]);
			if (ret) {
				pr_err("fail to set scl slice param\n");
				mutex_unlock(&scif->sc_mutex);
				ret = -EFAULT;
				goto start_scal_exit;
			}
			ret =cppif->cppdrv_ops->ioctl(CPP_DRV_SCL_SLICE_PARAM_CHECK, &scif->drv_priv, NULL);
			if (ret) {
				pr_err("fail to check slice param\n");
				mutex_unlock(&scif->sc_mutex);
				ret = -EFAULT;
				goto start_scal_exit;
			}
		} else if (cppif->hw_info->ip_cpp->slice_support==0){
			ret = cppif->cppdrv_ops->ioctl(CPP_DRV_SCL_PARAM_CHECK, &scif->drv_priv, NULL);
			if (ret) {
				pr_err("fail to check scl param\n");
				mutex_unlock(&scif->sc_mutex);
				ret = -EFAULT;
				goto start_scal_exit;
			}
		} else {
			pr_err("fail to get slice support info\n");
			mutex_unlock(&scif->sc_mutex);
			ret = -EINVAL;
			goto start_scal_exit;
		}

		ret = cppif->cppdrv_ops->ioctl(CPP_DRV_SCL_REG_SET, cppif, sc_parm);
		if (ret) {
			pr_err("fail to set slice param\n");
			mutex_unlock(&scif->sc_mutex);
			ret = -EINVAL;
			goto start_scal_exit;
		}
		CPP_TRACE("Start scale drv\n");
		ret = cppif->cppdrv_ops->ioctl(CPP_DRV_SCL_START, cppif, NULL);
		if (ret) {
			pr_err("fail to start scl\n");
			mutex_unlock(&scif->sc_mutex);
			ret = -EFAULT;
			goto start_scal_exit;
		}
		timeleft = wait_for_completion_timeout(&scif->done_com,
				msecs_to_jiffies(SCALE_TIMEOUT));
		if (timeleft == 0) {
			ret = cppif->cppdrv_ops->ioctl(CPP_DRV_SCL_REG_TRACE, cppif, NULL);
			if (ret) {
				pr_err("fail to trace scl reg\n");
				mutex_unlock(&scif->sc_mutex);
				ret = -EFAULT;
				goto start_scal_exit;
			}
			ret = cppif->cppdrv_ops->ioctl(CPP_DRV_SCL_STOP, cppif, NULL);
			if (ret) {
				pr_err("fail to stop scl\n");
				mutex_unlock(&scif->sc_mutex);
				ret = -EFAULT;
				goto start_scal_exit;
			}
			ret = cppif->cppdrv_ops->ioctl(CPP_DRV_SCL_RESET, dev->hw_info, soc_cpp);
			if (ret) {
				pr_err("fail to reset scl\n");
				mutex_unlock(&scif->sc_mutex);
				ret = -EFAULT;
				goto start_scal_exit;
			}
			mutex_unlock(&scif->sc_mutex);
			pr_err("fail to get scaling done com\n");
			ret = -EBUSY;
			goto start_scal_exit;
		}
		i++;
	} while (--sc_parm->slice_param_1.output.slice_count);
	ret =cppif->cppdrv_ops->ioctl(CPP_DRV_SCL_STOP, cppif, NULL);
	if (ret) {
		pr_err("fail to stop scl\n");
		mutex_unlock(&scif->sc_mutex);
		ret = -EFAULT;
		goto start_scal_exit;
	}
	ret =cppif->cppdrv_ops->ioctl(CPP_DRV_SCL_RESET, dev->hw_info, soc_cpp);
	if (ret) {
		pr_err("fail to reset scl\n");
		mutex_unlock(&scif->sc_mutex);
		ret = -EFAULT;
		goto start_scal_exit;
	}
	mutex_unlock(&scif->sc_mutex);
	CPP_TRACE("cpp scale over\n");

start_scal_exit:
	if (sc_parm != NULL)
		kfree(sc_parm);

	return ret;
}

static int cppcore_ioctl_open_dma(struct cpp_device *dev,
	unsigned long arg)
{
	int ret = 0;

	if (arg)
		ret = copy_to_user((int *)arg, &ret, sizeof(ret));
	if (ret) {
		pr_err("fail to open dma drv");
		ret = -EFAULT;
		goto dma_open_exit;
	}

dma_open_exit:

	return ret;
}

static int cppcore_ioctl_start_dma(struct cpp_device *dev,
	unsigned long arg)
{
	int ret = 0;
	int timeleft = 0;
	struct cpp_pipe_dev *cppif = NULL;
	struct cpp_hw_soc_info *soc_cpp = NULL;
	struct dmaif_device *dmaif = NULL;
	struct sprd_cpp_dma_cfg_parm dma_parm;
	struct dma_drv_private *p = NULL;
	unsigned long i = 0;

	cppif = dev->cppif;
	soc_cpp = dev->hw_info->soc_cpp;

	if (!cppif) {
		pr_err("fail to get invalid cppif!!\n");
		ret = -EINVAL;
		goto dma_exit;
	}
	dmaif = cppif->dmaif;
	if (!dmaif) {
		pr_err("fail to get invalid dmaif!\n");
		ret = -EINVAL;
		goto dma_exit;
	}
	p = &(cppif->dmaif->drv_priv);
	mutex_lock(&dmaif->dma_mutex);
	memset(&dma_parm, 0x00, sizeof(dma_parm));
	ret = copy_from_user(&dma_parm, (void __user *)arg, sizeof(dma_parm));
	if (unlikely(ret)) {
		pr_err("fail to get dma param form user, ret %d\n", ret);
		ret = -EFAULT;
		goto dma_exit;
	}

	dmaif->drv_priv.iommu_src.dev = &dev->pdev->dev;
	dmaif->drv_priv.iommu_dst.dev = &dev->pdev->dev;

	ret = cppif->cppdrv_ops->ioctl(CPP_DRV_DMA_STOP, &dma_parm, cppif);
	if (ret) {
		pr_err("fail to stop dma\n");
		ret = -EFAULT;
		goto dma_exit;
	}

	ret = cppif->cppdrv_ops->ioctl(CPP_DRV_DMA_EB, &dma_parm, cppif);
	if (ret) {
		pr_err("fail to en dma\n");
		ret = -EFAULT;
		goto dma_exit;
	}

	ret = cppif->cppdrv_ops->ioctl(CPP_DRV_DMA_SET_PARM, &dma_parm, cppif);
	if (ret) {
		pr_err("fail to set dma parm\n");
		ret = -EFAULT;
		goto dma_exit;
	}

	for (i = 0; i < dma_parm.loop_num; i++) {
		ret = cppif->cppdrv_ops->ioctl(CPP_DRV_DMA_START, &dma_parm, cppif);
		if (ret) {
			pr_err("fail to start dma\n");
			ret = -EFAULT;
			goto dma_exit;
		}

		timeleft = wait_for_completion_timeout(&dmaif->done_com,
					msecs_to_jiffies(DMA_TIMEOUT));
		if (timeleft == 0) {
			ret = cppif->cppdrv_ops->ioctl(CPP_DRV_DMA_REG_TRACE, cppif, NULL);
			ret = -EBUSY;
			goto dma_exit;
		}
		p->dma_src_addr += dma_parm.total_num;
		p->dma_dst_addr += dma_parm.total_num;
	}

	if (dma_parm.rest_num != 0) {
		p->cfg_parm.total_num = dma_parm.rest_num;
		ret = cppif->cppdrv_ops->ioctl(CPP_DRV_DMA_START, &dma_parm, cppif);
		if (ret) {
			pr_err("fail to start dma\n");
			ret = -EFAULT;
			goto dma_exit;
		}

		timeleft = wait_for_completion_timeout(&dmaif->done_com,
					msecs_to_jiffies(DMA_TIMEOUT));
		if (timeleft == 0) {
			ret = cppif->cppdrv_ops->ioctl(CPP_DRV_DMA_REG_TRACE, cppif, NULL);
			ret = -EBUSY;
			goto dma_exit;
		}
	}

	ret = cppif->cppdrv_ops->ioctl(CPP_DRV_DMA_STOP, &dma_parm, cppif);
	if (ret) {
		pr_err("fail to stop dma\n");
		ret = -EFAULT;
		goto dma_exit;
	}
	cppif->cppdrv_ops->ioctl(CPP_DRV_DMA_RESET, dev->hw_info, soc_cpp);
	if (ret) {
		pr_err("fail to reset dma\n");
		ret = -EFAULT;
		goto dma_exit;
	}
	mutex_unlock(&dmaif->dma_mutex);

dma_exit:

	CPP_TRACE("cpp dma ret %d\n", ret);
	return ret;
}

static int cppcore_ioctl_get_scale_cap(struct cpp_device *dev,
	unsigned long arg)
{
	int ret = 0;
	struct sprd_cpp_scale_capability sc_cap_param;

	memset(&sc_cap_param, 0x00, sizeof(sc_cap_param));

	if (!arg) {
		pr_err("%s: param is null error.\n", __func__);
		ret = -EFAULT;
		goto get_cap_exit;
	}

	ret = copy_from_user(&sc_cap_param, (void __user *)arg,
			sizeof(sc_cap_param));
	if (ret != 0) {
		pr_err("fail to copy from user, ret = %d\n", ret);
		ret = -EFAULT;
		goto get_cap_exit;
	}
	ret = dev->cppif->cppdrv_ops->ioctl(CPP_DRV_SCL_CAPABILITY_GET, dev->cppif, &sc_cap_param);
	if (ret != 0)
		sc_cap_param.is_supported = 0;
	else
		sc_cap_param.is_supported = 1;

	ret = copy_to_user((void  __user *)arg,
			&sc_cap_param, sizeof(sc_cap_param));
	if (ret != 0) {
		pr_err("fail to copy TO user, ret = %d\n", ret);
		ret = -EFAULT;
		goto get_cap_exit;
	}
	CPP_TRACE("cpp scale_capability done ret = %d\n", ret);

get_cap_exit:
	return ret;
}

static const struct cpp_io_ctrl_descr cpp_ioctl_desc[] = {
	{SPRD_CPP_IO_OPEN_ROT,         "SPRD_CPP_IO_OPEN_ROT"},
	{SPRD_CPP_IO_CLOSE_ROT,        "SPRD_CPP_IO_CLOSE_ROT"},
	{SPRD_CPP_IO_START_ROT,        "SPRD_CPP_IO_START_ROT"},
	{SPRD_CPP_IO_OPEN_SCALE,       "SPRD_CPP_IO_OPEN_SCALE"},
	{SPRD_CPP_IO_START_SCALE,      "SPRD_CPP_IO_START_SCALE"},
	{SPRD_CPP_IO_STOP_SCALE,       "SPRD_CPP_IO_STOP_SCALE"},
	{SPRD_CPP_IO_OPEN_DMA,         "SPRD_CPP_IO_OPEN_DMA"},
	{SPRD_CPP_IO_START_DMA,        "SPRD_CPP_IO_START_DMA"},
	{SPRD_CPP_IO_SCALE_CAPABILITY, "SPRD_CPP_IO_SCALE_CAPABILITY"},
};

static struct cpp_io_ctl_func s_cpp_io_ctrl_fun_tab[] = {
	{SPRD_CPP_IO_OPEN_ROT,         cppcore_ioctl_open_rot},
	{SPRD_CPP_IO_CLOSE_ROT,        NULL},
	{SPRD_CPP_IO_START_ROT,        cppcore_ioctl_start_rot},
	{SPRD_CPP_IO_OPEN_SCALE,       cppcore_ioctl_open_scale},
	{SPRD_CPP_IO_START_SCALE,      cppcore_ioctl_start_scale},
	{SPRD_CPP_IO_STOP_SCALE,       NULL},
	{SPRD_CPP_IO_OPEN_DMA,         cppcore_ioctl_open_dma},
	{SPRD_CPP_IO_START_DMA,        cppcore_ioctl_start_dma},
	{SPRD_CPP_IO_SCALE_CAPABILITY, cppcore_ioctl_get_scale_cap},
};

static cpp_io_func cppcore_ioctl_get_fun(uint32_t cmd)
{
	cpp_io_func io_ctrl = NULL;
	int total_num = 0;
	int i = 0;

	total_num = sizeof(s_cpp_io_ctrl_fun_tab) /
		sizeof(struct cpp_io_ctl_func);
	for (i = 0; i < total_num; i++) {
		if (cmd == s_cpp_io_ctrl_fun_tab[i].cmd) {
			io_ctrl = s_cpp_io_ctrl_fun_tab[i].io_ctrl;
			break;
		}
	}

	return io_ctrl;
}

static uint32_t cppcore_ioctl_get_val(uint32_t cmd)
{
	uint32_t nr = _IOC_NR(cmd);
	uint32_t i = 0;

	for (i = 0; i < ARRAY_SIZE(cpp_ioctl_desc); i++) {
		if (nr == _IOC_NR(cpp_ioctl_desc[i].ioctl_val))
			return cpp_ioctl_desc[i].ioctl_val;
	}

	return -1;
}

static char *cppcore_ioctl_get_str(uint32_t cmd)
{
	uint32_t nr = _IOC_NR(cmd);
	uint32_t i = 0;

	for (i = 0; i < ARRAY_SIZE(cpp_ioctl_desc); i++) {
		if (nr == _IOC_NR(cpp_ioctl_desc[i].ioctl_val))
			return (char *)cpp_ioctl_desc[i].ioctl_str;
	}

	return "NULL";
}
#endif
