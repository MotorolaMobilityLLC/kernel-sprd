/*
 * Copyright (C) 2019 Unisoc Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
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

static int sprd_cppcore_ioctl_open_rot(struct cpp_device *dev,
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

static int sprd_cppcore_ioctl_start_rot(struct cpp_device *dev,
	unsigned long arg)
{
	int ret = 0;
	int timeleft = 0;
	struct rotif_device *rotif = NULL;
	struct sprd_cpp_rot_cfg_parm rot_parm;

	rotif = dev->rotif;
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

	ret = sprd_rot_drv_parm_check(&rot_parm);
	if (ret) {
		pr_err("fail to check rot parm\n");
		mutex_unlock(&rotif->rot_mutex);
		ret = -EFAULT;
		goto rot_start_exit;
	}
	rotif->drv_priv.iommu_src.dev = &dev->pdev->dev;
	rotif->drv_priv.iommu_dst.dev = &dev->pdev->dev;

	ret = sprd_rot_drv_y_parm_set(&rot_parm,
			&rotif->drv_priv);
	if (ret) {
		pr_err("fail to set rot y parm\n");
		mutex_unlock(&rotif->rot_mutex);
		ret = -EFAULT;
		goto rot_start_exit;
	}
	sprd_rot_drv_start(&rotif->drv_priv);
	sprd_cppcore_rot_reg_trace(&rotif->drv_priv);
	if (!sprd_rot_drv_is_end(&rot_parm)) {
		timeleft = wait_for_completion_timeout(&rotif->done_com,
			msecs_to_jiffies(ROT_TIMEOUT));
		if (timeleft == 0) {
			sprd_cppcore_rot_reg_trace(&rotif->drv_priv);
			pr_err("fail to wait for rot path y int\n");
			sprd_rot_drv_stop(&rotif->drv_priv);
			sprd_cppcore_rot_reset(dev);
			mutex_unlock(&rotif->rot_mutex);
			ret = -EBUSY;
			goto rot_start_exit;
		}
		sprd_rot_drv_uv_parm_set(&rotif->drv_priv);
		sprd_rot_drv_start(&rotif->drv_priv);
	}

	timeleft = wait_for_completion_timeout(&rotif->done_com,
		msecs_to_jiffies(ROT_TIMEOUT));
	if (timeleft == 0) {
		sprd_cppcore_rot_reg_trace(&rotif->drv_priv);
		sprd_rot_drv_stop(&rotif->drv_priv);
		pr_err("fail to wait for rot path uv int\n");
		sprd_cppcore_rot_reset(dev);
		mutex_unlock(&rotif->rot_mutex);
		ret = -EBUSY;
		goto rot_start_exit;
	}

	sprd_rot_drv_stop(&rotif->drv_priv);
	sprd_cppcore_rot_reset(dev);
	mutex_unlock(&rotif->rot_mutex);

rot_start_exit:
	CPP_TRACE("cpp rotation ret %d\n", ret);

	return ret;
}

static int sprd_cppcore_ioctl_open_scale(struct cpp_device *dev,
	unsigned long arg)
{
	int ret = 0;
	int cpp_dimension = 0;
	struct sprd_cpp_size s_sc_cap;

	memset(&s_sc_cap, 0x00, sizeof(s_sc_cap));
	sprd_scale_drv_max_size_get(&s_sc_cap.w, &s_sc_cap.h);
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

static int sprd_cppcore_ioctl_start_scale(struct cpp_device *dev,
	unsigned long arg)
{
	int i = 0;
	int timeleft = 0;
	int ret = 0;
	struct sprd_cpp_scale_cfg_parm *sc_parm = NULL;
	struct scif_device *scif = NULL;

	scif = dev->scif;
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
	scif->drv_priv.iommu_src.dev = &dev->pdev->dev;
	scif->drv_priv.iommu_dst.dev = &dev->pdev->dev;
	scif->drv_priv.iommu_dst_bp.dev = &dev->pdev->dev;

	sprd_scale_drv_dev_stop(&scif->drv_priv);
	sprd_scale_drv_dev_enable(&scif->drv_priv);
	do {
		ret = sprd_scale_drv_slice_param_set(&scif->drv_priv, sc_parm,
			&sc_parm->slice_param.output.hw_slice_param[i]);
		if (ret) {
			pr_err("fail to set slice param\n");
			mutex_unlock(&scif->sc_mutex);
			ret = -EINVAL;
			goto start_scal_exit;
		}
		CPP_TRACE("Start scale drv\n");
		sprd_scale_drv_start(&scif->drv_priv);
		timeleft = wait_for_completion_timeout(&scif->done_com,
				msecs_to_jiffies(SCALE_TIMEOUT));
		if (timeleft == 0) {
			sprd_cppcore_sc_reg_trace(&scif->drv_priv);
			sprd_scale_drv_stop(&scif->drv_priv);
			sprd_cppcore_scale_reset(dev);
			mutex_unlock(&scif->sc_mutex);
			pr_err("fail to get scaling done com\n");
			ret = -EBUSY;
			goto start_scal_exit;
		}
		i++;
	} while (--sc_parm->slice_param.output.slice_count);
	sprd_scale_drv_stop(&scif->drv_priv);
	sprd_cppcore_scale_reset(dev);
	mutex_unlock(&scif->sc_mutex);
	CPP_TRACE("cpp scale over\n");

start_scal_exit:
	if (sc_parm != NULL)
		kfree(sc_parm);

	return ret;
}

static int sprd_cppcore_ioctl_start_dma(struct cpp_device *dev,
	unsigned long arg)
{
	int ret = 0;
	int timeleft = 0;
	struct dmaif_device *dmaif = NULL;
	struct sprd_cpp_dma_cfg_parm dma_parm;

	memset(&dma_parm, 0x00, sizeof(dma_parm));

	dmaif = dev->dmaif;
	if (!dmaif) {
		pr_err("fail to get valid dmaif!\n");
		ret = -EFAULT;
		goto start_dma_exit;
	}

	mutex_lock(&dmaif->dma_mutex);

	ret = copy_from_user(&dma_parm, (void __user *)arg, sizeof(dma_parm));
	if (ret) {
		pr_err("fail to get param form user\n");
		mutex_unlock(&dmaif->dma_mutex);
		ret = -EFAULT;
		goto start_dma_exit;
	}

	dmaif->drv_priv.iommu_src.dev = &dev->pdev->dev;
	dmaif->drv_priv.iommu_dst.dev = &dev->pdev->dev;

	ret = sprd_dma_drv_start(&dma_parm, &dmaif->drv_priv);
	if (ret) {
		pr_err("fail to start dma\n");
		mutex_unlock(&dmaif->dma_mutex);
		ret = -EFAULT;
		goto start_dma_exit;
	}

	timeleft = wait_for_completion_timeout(&dmaif->done_com,
			msecs_to_jiffies(DMA_TIMEOUT));
	if (timeleft == 0) {
		sprd_cppcore_dma_reg_trace(&dmaif->drv_priv);
		sprd_dma_drv_stop(&dmaif->drv_priv);
		sprd_cppcore_dma_reset(dev);
		mutex_unlock(&dmaif->dma_mutex);
		pr_err("failed to get dma done com\n");
		ret = -EBUSY;
		goto start_dma_exit;
	}

	sprd_dma_drv_stop(&dmaif->drv_priv);
	sprd_cppcore_dma_reset(dev);
	mutex_unlock(&dmaif->dma_mutex);
	CPP_TRACE("cpp dma over\n");

start_dma_exit:

	return ret;
}

static int sprd_cppcore_ioctl_get_scale_cap(struct cpp_device *dev,
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
	ret = sprd_scale_drv_capability_get(&sc_cap_param);
	if (ret != 0)
		sc_cap_param.is_supported = 0;

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
	{SPRD_CPP_IO_OPEN_ROT, "SPRD_CPP_IO_OPEN_ROT"},
	{SPRD_CPP_IO_CLOSE_ROT, "SPRD_CPP_IO_CLOSE_ROT"},
	{SPRD_CPP_IO_START_ROT, "SPRD_CPP_IO_START_ROT"},
	{SPRD_CPP_IO_OPEN_SCALE, "SPRD_CPP_IO_OPEN_SCALE"},
	{SPRD_CPP_IO_START_SCALE, "SPRD_CPP_IO_START_SCALE"},
	{SPRD_CPP_IO_STOP_SCALE, "SPRD_CPP_IO_STOP_SCALE"},
	{SPRD_CPP_IO_OPEN_DMA, "SPRD_CPP_IO_OPEN_DMA"},
	{SPRD_CPP_IO_START_DMA, "SPRD_CPP_IO_START_DMA"},
	{SPRD_CPP_IO_SCALE_CAPABILITY, "SPRD_CPP_IO_SCALE_CAPABILITY"},
};

static struct cpp_io_ctl_func s_cpp_io_ctrl_fun_tab[] = {
	{SPRD_CPP_IO_OPEN_ROT, sprd_cppcore_ioctl_open_rot},
	{SPRD_CPP_IO_CLOSE_ROT, NULL},
	{SPRD_CPP_IO_START_ROT, sprd_cppcore_ioctl_start_rot},
	{SPRD_CPP_IO_OPEN_SCALE, sprd_cppcore_ioctl_open_scale},
	{SPRD_CPP_IO_START_SCALE, sprd_cppcore_ioctl_start_scale},
	{SPRD_CPP_IO_STOP_SCALE, NULL},
	{SPRD_CPP_IO_OPEN_DMA, NULL},
	{SPRD_CPP_IO_START_DMA, sprd_cppcore_ioctl_start_dma},
	{SPRD_CPP_IO_SCALE_CAPABILITY, sprd_cppcore_ioctl_get_scale_cap},
};

static cpp_io_func sprd_cppcore_ioctl_get_fun(uint32_t cmd)
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

static uint32_t sprd_cppcore_ioctl_get_val(uint32_t cmd)
{
	uint32_t nr = _IOC_NR(cmd);
	uint32_t i = 0;

	for (i = 0; i < ARRAY_SIZE(cpp_ioctl_desc); i++) {
		if (nr == _IOC_NR(cpp_ioctl_desc[i].ioctl_val))
			return cpp_ioctl_desc[i].ioctl_val;
	}

	return -1;
}

static char *sprd_cppcore_ioctl_get_str(uint32_t cmd)
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
