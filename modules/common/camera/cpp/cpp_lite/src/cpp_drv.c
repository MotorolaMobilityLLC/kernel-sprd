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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <sprd_mm.h>
#include "cam_kernel_adapt.h"
#include <linux/dma-mapping.h>
#include <uapi/linux/dma-buf.h>
#include "cam_types.h"
#include "cpp_drv.h"
#include "cpp_hw.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "CPP_DRV: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

atomic_t cpp_dma_cnt;

static int cppdrv_put_sg_table(struct cpp_iommu_info *pfinfo)
{
	int ret = 0;

	if (!pfinfo) {
		pr_err("fail to get buffer info ptr\n");
		return -EFAULT;
	}

	pr_debug("enter.\n");
	if (pfinfo->mfd[0] <= 0) {
		pr_err("fail to get valid buffer\n");
		return -EFAULT;
	}

	if (!IS_ERR_OR_NULL(pfinfo->dmabuf_p)) {
		if (!IS_ERR_OR_NULL(pfinfo->dmabuf_p->file) &&
			virt_addr_valid(pfinfo->dmabuf_p->file))
			dma_buf_put(pfinfo->dmabuf_p);
		pfinfo->dmabuf_p = NULL;
		if (atomic_read(&cpp_dma_cnt))
			atomic_dec(&cpp_dma_cnt);
	}
	pfinfo->buf = NULL;
	return ret;
}

static int cppdrv_get_sg_table(struct cpp_iommu_info *pfinfo)
{
	int ret = 0;
	if (!pfinfo) {
		pr_err("fail to get buffer info ptr\n");
		return -EFAULT;
	}

	pr_debug("enter.\n");
	if (pfinfo->mfd[0] <= 0) {
		pr_err("fail to get valid buffer\n");
		return -EFAULT;
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	ret = sprd_dmabuf_get_sysbuffer(pfinfo->mfd[0], NULL,
		&pfinfo->buf, &pfinfo->size);
#else
	ret = sprd_ion_get_buffer(pfinfo->mfd[0], NULL,
		&pfinfo->buf, &pfinfo->size);
#endif
	if (ret) {
		pr_err("fail to get sg table\n");
		return -EFAULT;
	}
	if (pfinfo->dmabuf_p == NULL) {
		pfinfo->dmabuf_p = dma_buf_get(pfinfo->mfd[0]);
		if (IS_ERR_OR_NULL(pfinfo->dmabuf_p)) {
			pr_err("fail to get dma buf %p\n", pfinfo->dmabuf_p);
			goto failed;
		}
		if (atomic_read(&cpp_dma_cnt))
			atomic_inc(&cpp_dma_cnt);
		pr_debug("dmabuf %p\n", pfinfo->dmabuf_p);
	}
	return 0;
failed:
	cppdrv_put_sg_table(pfinfo);
	return -EINVAL;
}

static int cppdrv_get_addr(struct cpp_iommu_info *pfinfo)
{
	int i = 0, ret = 0;
	struct sprd_iommu_map_data iommu_data;

	memset(&iommu_data, 0x00, sizeof(iommu_data));

	if (!pfinfo) {
		pr_err("fail to get buffer info ptr\n");
		return -EFAULT;
	}

	if (sprd_iommu_attach_device(pfinfo->dev) == 0) {
		if (pfinfo->dmabuf_p == NULL) {
			pfinfo->dmabuf_p = dma_buf_get(pfinfo->mfd[0]);
			if (IS_ERR_OR_NULL(pfinfo->dmabuf_p)) {
				pr_err("fail to get dma buf %p\n", pfinfo->dmabuf_p);
				ret = -EINVAL;
				goto dmabuf_get_failed;
			}
		}
		pr_debug("mfd %d, dmabuf %p\n", pfinfo->mfd[0], pfinfo->dmabuf_p);

		if (dma_set_mask(pfinfo->dev, DMA_BIT_MASK(64))) {
			dev_warn(pfinfo->dev, "mydev: No suitable DMA available\n");
			goto ignore_this_device;
		}
		pfinfo->attachment = dma_buf_attach(pfinfo->dmabuf_p, pfinfo->dev);
		if (IS_ERR_OR_NULL(pfinfo->attachment)) {
			pr_err("failed to attach dmabuf %px\n", (void *)pfinfo->dmabuf_p);
			ret = -EINVAL;
			goto attach_failed;
		}
		pfinfo->table = dma_buf_map_attachment(pfinfo->attachment, DMA_BIDIRECTIONAL);
		if (IS_ERR_OR_NULL(pfinfo->table)) {
			pr_err("failed to map attachment %px\n", (void *)pfinfo->attachment);
			ret = -EINVAL;
			goto map_attachment_failed;
		}

		memset(&iommu_data, 0x00, sizeof(iommu_data));
		iommu_data.buf = pfinfo->buf;
		iommu_data.iova_size = pfinfo->size;
		iommu_data.ch_type = SPRD_IOMMU_FM_CH_RW;

		ret = sprd_iommu_map(pfinfo->dev, &iommu_data);
		if (ret) {
			pr_err("fail to get iommu kaddr\n");
			return -EFAULT;
		}

		for (i = 0; i < 2; i++)
			pfinfo->iova[i] = iommu_data.iova_addr
					+ pfinfo->offset[i];
	} else {
		for (i = 0; i < 2; i++) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
			ret = sprd_dmabuf_get_phys_addr(-1, pfinfo->dmabuf_p,
					&pfinfo->iova[i],
					&pfinfo->size);
#else
			ret = sprd_ion_get_phys_addr(-1, pfinfo->dmabuf_p,
					&pfinfo->iova[i],
					&pfinfo->size);
#endif
			if (ret) {
				pr_err("fail to get iommu phy addr\n");
				pr_err("index:%d mfd:0x%x\n",
					i, pfinfo->mfd[0]);
				return -EFAULT;
			}
			pfinfo->iova[i] += pfinfo->offset[i];
		}
	}

	return 0;

map_attachment_failed:
	if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) {
		if (!IS_ERR_OR_NULL(pfinfo->attachment))
			dma_buf_detach(pfinfo->dmabuf_p, pfinfo->attachment);
	}
attach_failed:
ignore_this_device:
	if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) {
		if (!IS_ERR_OR_NULL(pfinfo->dmabuf_p)) {
			dma_buf_put(pfinfo->dmabuf_p);
			pfinfo->dmabuf_p = NULL;
		}
	}
dmabuf_get_failed:
	return ret;
}

static int cppdrv_free_addr(struct cpp_iommu_info *pfinfo)
{
	int ret = 0;
	struct sprd_iommu_unmap_data iommu_data;

	memset(&iommu_data, 0x00, sizeof(iommu_data));

	if (!pfinfo) {
		pr_err("fail to get buffer info ptr\n");
		return -EFAULT;
	}

	if (sprd_iommu_attach_device(pfinfo->dev) == 0) {
		iommu_data.iova_addr = pfinfo->iova[0]
				- pfinfo->offset[0];
		iommu_data.iova_size = pfinfo->size;
		iommu_data.ch_type = SPRD_IOMMU_FM_CH_RW;
		iommu_data.buf = NULL;

		ret = sprd_iommu_unmap(pfinfo->dev, &iommu_data);
		if (ret) {
			pr_err("failed to free iommu\n");
			return -EFAULT;
		}
		if (!IS_ERR_OR_NULL(pfinfo->table))
			dma_buf_unmap_attachment(pfinfo->attachment, pfinfo->table, DMA_BIDIRECTIONAL);
		if (!IS_ERR_OR_NULL(pfinfo->attachment))
			dma_buf_detach(pfinfo->dmabuf_p, pfinfo->attachment);
		cppdrv_put_sg_table(pfinfo);
	}
	return 0;
}

static int cppdrv_get_slice_support(struct cpp_pipe_dev *cppif)
{
	struct cpp_hw_info *hw = NULL;
	int support = 0;

	if (!cppif) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	hw = cppif->hw_info;
	if (!hw) {
		pr_err("fail to get valid hw %p\n", hw);
		return -EINVAL;
	}
	support = hw->cpp_hw_ioctl(CPP_HW_CFG_SLICE_SUPPORT,hw);
	return support;
}

static int cppdrv_get_bp_support(struct cpp_pipe_dev *cppif)
{
	struct cpp_hw_info *hw = NULL;
	int support = 0;

	if (!cppif) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	hw = cppif->hw_info;
	if (!hw) {
		pr_err("fail to get valid hw %p\n", hw);
		return -EINVAL;
	}
	support = hw->cpp_hw_ioctl(CPP_HW_CFG_BP_SUPPORT,hw);
	return support;
}

static int cppdrv_get_zoomup_support(struct cpp_pipe_dev *cppif)
{
	struct cpp_hw_info *hw = NULL;
	int support = 0;

	if (!cppif) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	hw = cppif->hw_info;
	if (!hw) {
		pr_err("fail to get valid hw %p\n", hw);
		return -EINVAL;
	}
	support = hw->cpp_hw_ioctl(CPP_HW_CFG_ZOOMUP_SUPPORT,hw);
	return support;
}

static int cppdrv_get_support_info(void *arg1, void *arg2)
{
	struct cpp_hw_ip_info *ip_cpp = NULL;
	struct cpp_pipe_dev *cppif = NULL;

	if (!arg1) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	cppif = (struct cpp_pipe_dev *)arg1;
	ip_cpp = cppif->hw_info->ip_cpp;
	if (!ip_cpp) {
		pr_err("fail to get valid ip_cpp %p\n", ip_cpp);
		return -EINVAL;
	}

	ip_cpp->bp_support = cppdrv_get_bp_support(cppif);
	ip_cpp->slice_support = cppdrv_get_slice_support(cppif);
	ip_cpp->zoom_up_support = cppdrv_get_zoomup_support(cppif);

	return 0;
}

static int cppdrv_rot_format_get(struct sprd_cpp_rot_cfg_parm *parm)
{
	unsigned int fmt = ROT_ONE_BYTE;

	if (!parm) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}

	switch (parm->format) {
	case ROT_YUV422:
	case ROT_YUV420:
		fmt = ROT_ONE_BYTE;
		break;
	case ROT_RGB888:
		fmt = ROT_FOUR_BYTES;
		break;
	default:
		pr_err("fail to get invalid format\n");
		break;
	}

	return fmt;
}


static int cppdrv_rot_parm_check(void *arg1, void *arg2)
{
	struct sprd_cpp_rot_cfg_parm *parm = NULL;

	if (!arg1) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	parm = (struct sprd_cpp_rot_cfg_parm *)arg1;
#ifdef ROT_DRV_DEBUG
	CPP_TRACE("format %d angle %d w %d h %d\n",
		parm->format, parm->angle, parm->size.w, parm->size.h);
	CPP_TRACE("src mfd %u y:u:v 0x%x 0x%x 0x%x\n", parm->src_addr.mfd[0],
		parm->src_addr.y, parm->src_addr.u, parm->src_addr.v);
	CPP_TRACE("dst mfd %u y:u:v 0x%x 0x%x 0x%x\n", parm->dst_addr.mfd[0],
		parm->dst_addr.y, parm->dst_addr.u, parm->dst_addr.v);
#endif
	if ((parm->src_addr.y & ROT_ADDR_ALIGN) ||
		(parm->src_addr.u & ROT_ADDR_ALIGN) ||
		(parm->src_addr.v & ROT_ADDR_ALIGN) ||
		(parm->dst_addr.y & ROT_ADDR_ALIGN) ||
		(parm->dst_addr.u & ROT_ADDR_ALIGN) ||
		(parm->dst_addr.v & ROT_ADDR_ALIGN)) {
		pr_err("fail to get aligned addr\n");
		return -EINVAL;
	}

	if (parm->format != ROT_YUV422 && parm->format != ROT_YUV420 &&
		parm->format != ROT_RGB888) {
		pr_err("fail to get invalid image format %d\n", parm->format);
		return -EINVAL;
	}

	if (parm->angle > ROT_MIRROR) {
		pr_err("fail to get invalid rotation angle %d\n", parm->angle);
		return -EINVAL;
	}

	if ((parm->size.w % 8 != 0) || (parm->size.h % 4 != 0)) {
		pr_err("fail to get alogned width and height:%u, %u",
				parm->size.w, parm->size.h);
		return -EINVAL;
	}

	return 0;
}

static int cppdrv_rot_is_end(void *arg1, void *arg2)
{
	int ret = 1;
	struct sprd_cpp_rot_cfg_parm *parm = NULL;

	if (!arg1) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	parm = (struct sprd_cpp_rot_cfg_parm *)arg1;
	switch (parm->format) {
	case ROT_YUV422:
	case ROT_YUV420:
		ret = 0;
		break;
	case ROT_RGB888:
		ret = 1;
		break;
	default:
		pr_err("fail to get valid format\n");
		break;
	}

	return ret;
}

static int cppdrv_rot_y_parm_set(void *arg1, void *arg2)
{
	int ret = 0;
	struct rot_drv_private *p = NULL;
	struct sprd_cpp_rot_cfg_parm *parm = NULL;
	struct cpp_pipe_dev *cppif = NULL;

	if (!arg1 || !arg2) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	parm = (struct sprd_cpp_rot_cfg_parm *)arg1;
	cppif = (struct cpp_pipe_dev *)arg2;
	p = &(cppif->rotif->drv_priv);
	if (!p) {
		pr_err("fail to get valid rot_drv_private\n");
		return -EINVAL;
	}
	memcpy((void *)&p->cfg_parm, (void *)parm,
		sizeof(struct sprd_cpp_rot_cfg_parm));
	memcpy(p->iommu_src.mfd, parm->src_addr.mfd,
		sizeof(parm->src_addr.mfd));
	memcpy(p->iommu_dst.mfd, parm->dst_addr.mfd,
		sizeof(parm->dst_addr.mfd));

	ret = cppdrv_get_sg_table(&p->iommu_src);
	if (ret) {
		pr_err("fail to get cpp sg table\n");
		return -1;
	}
	p->iommu_src.offset[0] = p->cfg_parm.src_addr.y;
	p->iommu_src.offset[1] = p->cfg_parm.src_addr.u;
	ret = cppdrv_get_addr(&p->iommu_src);
	if (ret) {
		pr_err("fail to get src addr\n");
		return -EFAULT;
	}

	if (p->iommu_src.attachment && p->iommu_src.table)
		ret = dma_buf_end_cpu_access(p->iommu_src.dmabuf_p, DMA_BIDIRECTIONAL);

	ret = cppdrv_get_sg_table(&p->iommu_dst);
	if (ret) {
		pr_err("fail to get cpp sg table\n");
		cppdrv_free_addr(&p->iommu_src);
		return -1;
	}
	p->iommu_dst.offset[0] = p->cfg_parm.dst_addr.y;
	p->iommu_dst.offset[1] = p->cfg_parm.dst_addr.u;
	ret = cppdrv_get_addr(&p->iommu_dst);
	if (ret) {
		pr_err("fail to get src addr\n");
		cppdrv_free_addr(&p->iommu_src);
		return -EFAULT;
	}
	p->rot_src_addr = p->iommu_src.iova[0];
	p->rot_dst_addr = p->iommu_dst.iova[0];
	p->uv_mode = ROT_UV420;
	p->rot_fmt = cppdrv_rot_format_get(parm);
	p->rot_size.w = parm->size.w;
	p->rot_size.h = parm->size.h;
	p->rot_mode = parm->angle;
	p->rot_endian = 0x5;

	return ret;
}

static int cppdrv_rot_uv_parm_set(void *arg1, void *arg2)
{
	struct rot_drv_private *p = NULL;
	struct cpp_pipe_dev *cppif= NULL;

	if (!arg1) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}
	cppif = (struct cpp_pipe_dev *)arg1;
	p= &(cppif->rotif->drv_priv);
	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}
	p->rot_src_addr = p->iommu_src.iova[1];
	p->rot_dst_addr = p->iommu_dst.iova[1];
	p->rot_size.w >>= 0x01;
	p->rot_fmt = ROT_TWO_BYTES;

	if (p->cfg_parm.format == ROT_YUV422)
		p->uv_mode = ROT_UV422;
	else if (p->cfg_parm.format == ROT_YUV420) {
		p->uv_mode = ROT_UV420;
		p->rot_size.h >>= 0x01;
	}
	return 0;
}

static int cppdrv_rot_start(void *arg1, void *arg2)
{
	struct rot_drv_private *p = NULL;
	struct cpp_pipe_dev *cppif= NULL;
	struct cpp_hw_info *hw = NULL;
	int ret = 0;

	if (!arg1) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	cppif = (struct cpp_pipe_dev *)arg1;
	p = &(cppif->rotif->drv_priv);
	hw = cppif->hw_info;
	if (!p || !hw) {
		pr_err("fail to get valid drv_private %p, hw %p\n", p, hw);
		return -EINVAL;
	}

	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_ROT_STOP,p);
	if (ret) {
		pr_err("fail to stop rot\n");
		return -EINVAL;
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_ROT_EB,p);
	if (ret) {
		pr_err("fail to eb rot\n");
		return -EINVAL;
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_ROT_PARM_SET,p);
	if (ret) {
		pr_err("fail to set rot parm\n");
		return -EINVAL;
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_ROT_START,p);
	if (ret) {
		pr_err("fail to start rot\n");
		return -EINVAL;
	}
	return 0;
}

static int cppdrv_rot_stop(void *arg1, void *arg2)
{
	struct rot_drv_private *p = NULL;
	struct cpp_pipe_dev *cppif= NULL;
	struct cpp_hw_info *hw = NULL;
	int ret = 0;

	if (!arg1) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}
	cppif = (struct cpp_pipe_dev *)arg1;
	p = &(cppif->rotif->drv_priv);
	hw = cppif->hw_info;
	if (!p || !hw) {
		pr_err("fail to get valid rot_drv_private %p, hw %p\n", p, hw);
		return -EINVAL;
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_ROT_STOP,p);
	if (ret) {
		pr_err("fail to stop rot\n");
		return -EINVAL;
	}
	udelay(1);
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_ROT_DISABLE,p);
	if (ret) {
		pr_err("fail to disable rot\n");
		return -EINVAL;
	}
	cppdrv_free_addr(&p->iommu_src);
	cppdrv_free_addr(&p->iommu_dst);
	return 0;
}

static int cppdrv_scale_max_size_get(void *arg1, void *arg2)
{
	unsigned int *max_width = 0;
	unsigned int *max_height = 0;
	if (!arg1 ||!arg2 ) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}
	max_width = (unsigned int *)arg1;
	max_height = (unsigned int *)arg2;
	*max_width = SCALE_FRAME_WIDTH_MAX;
	*max_height = SCALE_FRAME_HEIGHT_MAX;
	return 0;
}

static int cppdrv_scale_sl_stop(void *arg1, void *arg2)
{
	struct scale_drv_private *p = NULL;
	struct cpp_pipe_dev *cppif= NULL;
	struct cpp_hw_info *hw = NULL;
	int ret = 0;

	if (!arg1) {
		pr_err("fail to get valid input ptr\n");
		return -1 ;
	}
	cppif = (struct cpp_pipe_dev *)arg1;
	p = &(cppif->scif->drv_priv);
	hw = cppif->hw_info;
	if (!p || !hw) {
		pr_err("fail to get valid drv_private %p, hw %p\n", p, hw);
		return -EINVAL;
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_STOP,p);
	if (ret) {
		pr_err("fail to stop scl\n");
		return -EINVAL;
	}
	return 0;
}

static int cppdrv_scale_enable(void *arg1, void *arg2)
{
	struct scale_drv_private *p = NULL;
	struct cpp_pipe_dev *cppif= NULL;
	struct cpp_hw_info *hw = NULL;
	int ret = 0;

	if (!arg1) {
		pr_err("fail to get valid input ptr\n");
		return -1 ;
	}
	cppif = (struct cpp_pipe_dev *)arg1;
	p = &(cppif->scif->drv_priv);
	hw = cppif->hw_info;
	if (!p || !hw) {
		pr_err("fail to get valid drv_private %p, hw %p\n", p, hw);
		return -EINVAL;
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_EB,p);
	if (ret) {
		pr_err("fail to eb scl\n");
		return -EINVAL;
	}

	return 0;
}

static int cppdrv_scale_addr_set(
	struct cpp_pipe_dev *cppif,
	struct sprd_cpp_scale_cfg_parm *cfg_parm)
{
	int ret = 0;
	int bp_support = 0;
	struct scale_drv_private *p = NULL;
	struct cpp_hw_info *hw = NULL;

	if (!cppif || !cfg_parm) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
	p = &(cppif->scif->drv_priv);
	hw = cppif->hw_info;
	if (!p || !hw) {
		pr_err("fail to get valid drv_private %p, hw %p\n", p, hw);
		return -EINVAL;
	}

	if (cfg_parm->input_addr.mfd[0] == 0 ||
		cfg_parm->output_addr.mfd[0] == 0) {
		pr_err("fail to get valid in or out mfd\n");
		return -1;
	}
	bp_support = cppdrv_get_bp_support(cppif);
	if (1 == bp_support){
		if ((p->bp_en == 1) &&
			(cfg_parm->bp_output_addr.mfd[0] == 0)) {
			pr_err("fail to get valid bypass mfd\n");
			return -1;
		}
	}

	if (cfg_parm->slice_param.output.slice_count == cfg_parm->slice_param_1.output.slice_count) {
		memcpy(p->iommu_src.mfd, cfg_parm->input_addr.mfd,
			sizeof(cfg_parm->input_addr.mfd));
		memcpy(p->iommu_dst.mfd, cfg_parm->output_addr.mfd,
			sizeof(cfg_parm->output_addr.mfd));
		if (1 == bp_support){
			if (p->bp_en == 1)
				memcpy(p->iommu_dst_bp.mfd, cfg_parm->bp_output_addr.mfd,
					sizeof(cfg_parm->bp_output_addr.mfd));
		}
		ret = cppdrv_get_sg_table(&p->iommu_src);
		if (ret) {
			pr_err("fail to get cpp src sg table\n");
			return -1;
		}
		p->iommu_src.offset[0] = cfg_parm->input_addr.y;
		p->iommu_src.offset[1] = cfg_parm->input_addr.u;
		p->iommu_src.offset[2] = cfg_parm->input_addr.v;
		ret = cppdrv_get_addr(&p->iommu_src);
		if (ret) {
			pr_err("fail to get cpp src addr\n");
			return -1;
		}

		if (p->iommu_src.attachment && p->iommu_src.table)
			ret = dma_buf_end_cpu_access(p->iommu_src.dmabuf_p, DMA_BIDIRECTIONAL);

		ret = cppdrv_get_sg_table(&p->iommu_dst);
		if (ret) {
			pr_err("fail to get cpp dst sg table\n");
			cppdrv_free_addr(&p->iommu_src);
			return ret;
		}
		p->iommu_dst.offset[0] = cfg_parm->output_addr.y;
		p->iommu_dst.offset[1] = cfg_parm->output_addr.u;
		p->iommu_dst.offset[2] = cfg_parm->output_addr.v;
		ret = cppdrv_get_addr(&p->iommu_dst);
		if (ret) {
			pr_err("fail to get cpp dst addr\n");
			cppdrv_free_addr(&p->iommu_src);
			return ret;
		}
		if (1 == bp_support){
			if (p->bp_en == 1) {
				ret = cppdrv_get_sg_table(&p->iommu_dst_bp);
				if (ret) {
					pr_err("fail to get cpp dst sg table\n");
					cppdrv_free_addr(&p->iommu_src);
					cppdrv_free_addr(&p->iommu_dst);
					return ret;
				}
				p->iommu_dst_bp.offset[0] = cfg_parm->bp_output_addr.y;
				p->iommu_dst_bp.offset[1] = cfg_parm->bp_output_addr.u;
				p->iommu_dst_bp.offset[2] = cfg_parm->bp_output_addr.v;
				ret = cppdrv_get_addr(&p->iommu_dst_bp);
				if (ret) {
					pr_err("fail to get cpp dst addr\n");
					cppdrv_free_addr(&p->iommu_src);
					cppdrv_free_addr(&p->iommu_dst);
					return ret;
				}
			}
		}
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_ADDR_SET,p);
	if (ret) {
		pr_err("fail to add scl addr\n");
		return -EINVAL;
	}

	return ret;
}

static int cppdrv_scale_coeff_set(struct cpp_pipe_dev *cppif,
		struct sprd_cpp_scale_cfg_parm *sc_cfg)
{
	struct sprd_cpp_scaler_coef *drv_scaler_coef = NULL;
	int i = 0;
	int j = 0;
	int ret = 0;
	unsigned long reg_addr_offset = 0;
	unsigned int sc_slice_in_height_y = 0;
	unsigned int sc_slice_out_height_y = 0;
	unsigned int sc_slice_out_height_uv = 0;
	unsigned int sc_slice_in_height_uv = 0;
	int clk_switch_flag = 0;
	int vcoef_reorder[144];
	int tmp = 0;
	int luma_hcoeff[32];
	int chroma_hcoeff[16];
	int vcoeff[144];
	int luma_hor[64] __aligned(16);
	int chroma_hor[32] __aligned(16);
	struct scale_drv_private *p = NULL;
	struct cpp_hw_info *hw = NULL;

	if (!cppif || !sc_cfg) {
		pr_err("fail to get valid input scale_cfg_parm\n");
		return -1;
	}
	p = &(cppif->scif->drv_priv);
	hw = cppif->hw_info;
	if (!p || !hw) {
		pr_err("fail to get valid drv_private %p, hw %p\n", p, hw);
		return -EINVAL;
	}

	drv_scaler_coef = &sc_cfg->slice_param_1.output.scaler_path_coef;

	memset(&luma_hcoeff, 0, sizeof(luma_hcoeff));
	memset(&chroma_hcoeff, 0, sizeof(chroma_hcoeff));
	memset(&vcoeff, 0, sizeof(vcoeff));
	memset(&vcoef_reorder, 0, sizeof(vcoef_reorder));

	clk_switch_flag = 1;
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_CLK_SWITCH,&clk_switch_flag);
	if (ret) {
		pr_err("fail to switch scl clk\n");
		return -EINVAL;
	}

	/*  handle luma her coeff */
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++)
			luma_hor[i * 8 + j] = drv_scaler_coef->y_hor_coef[i][j];
	}
	for (i = 0; i < 32; i++)
		luma_hcoeff[i] =
			((luma_hor[2 * i] & 0x000001ff) << 9) |
			(luma_hor[2 * i + 1] & 0x000001ff);

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 4; j++) {
			reg_addr_offset = 4 * (4 * i + j);
			p->coeff_addr_offset = reg_addr_offset;
			p->coeff_arg = luma_hcoeff[4 * i + 3 - j];
			ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_LUMA_HCOEFF_SET,p);
			if (ret) {
				pr_err("fail to set luma hcoeff\n");
				return -EINVAL;
			}
		}
	}
	/* handle chroma her coeff */
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 4; j++)
			chroma_hor[i * 4 + j] =
				drv_scaler_coef->c_hor_coef[i][j];
	}
	for (i = 0; i < 16; i++) {
		chroma_hcoeff[i] = ((chroma_hor[2 * i] & 0x000001ff) << 9) |
			(chroma_hor[2 * i + 1] & 0x000001ff);
	}
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 2; j++) {
			reg_addr_offset = 4 * (2 * i + j);
			p->coeff_addr_offset = reg_addr_offset;
			p->coeff_arg = chroma_hcoeff[2 * i + 1 - j];
			ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_CHRIMA_HCOEF_SET,p);
			if (ret) {
				pr_err("fail to set chrima hcoeff\n");
				return -EINVAL;
			}
		}
	}

	/* handle ver coeff */
	sc_slice_in_height_y = p->sc_slice_in_size.h;
	sc_slice_out_height_y = p->sc_slice_out_size.h;

	if (p->input_fmt == 0)
		sc_slice_in_height_uv = p->sc_slice_in_size.h >> 1;
	else if (p->input_fmt == 2)
		sc_slice_in_height_uv = p->sc_slice_in_size.h;

	if (p->sc_output_fmt == 0)
		sc_slice_out_height_uv = p->sc_slice_out_size.h >> 1;
	else if (p->sc_output_fmt == 2)
		sc_slice_out_height_uv = p->sc_slice_out_size.h;

	for (i = 0; i < 9; i++) {
		for (j = 0; j < 16; j++) {
			tmp = drv_scaler_coef->c_ver_coef[i][j];
			vcoeff[i * 16 + j] = ((tmp & 0x000001ff) << 9) |
			(drv_scaler_coef->y_ver_coef[i][j] & 0x000001ff);
		}
	}
	for (i = 0; i < 8; i++) {
		if (sc_slice_out_height_y * 2 > sc_slice_in_height_y) {
			for (j = 0; j < 4; j++)
				vcoef_reorder[i * 4 + j] =
					vcoeff[i * 16 + j] & 0x000001ff;
		} else {
			for (j = 0; j < 16; j++)
				vcoef_reorder[i * 16 + j] =
					vcoeff[i * 16 + j] & 0x000001ff;
		}
	}
	for (i = 0; i < 8; i++) {
		if (sc_slice_out_height_uv * 2 > sc_slice_in_height_uv) {
			for (j = 0; j < 4; j++) {
				vcoef_reorder[i * 4 + j] |=
					(vcoeff[i * 16 + j] & 0x0003fe00);
			}
		} else {
			for (j = 0; j < 16; j++)
				vcoef_reorder[i * 16 + j] |=
					(vcoeff[i * 16 + j] & 0x0003fe00);
		}
	}
	for (i = 0; i < 16; i++)
		vcoef_reorder[128 + i] = vcoeff[128 + i];

	for (i = 0; i < 132; i++) {
		reg_addr_offset = 4 * i;
		p->coeff_addr_offset = reg_addr_offset;
		p->coeff_arg= vcoef_reorder[i];
		ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_VCOEF_SET,p);
		if (ret) {
				pr_err("fail to set vcoeff\n");
				return -EINVAL;
			}
	}

	clk_switch_flag = 0;
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_CLK_SWITCH,&clk_switch_flag);
	if (ret) {
		pr_err("fail to switch scl clk\n");
		return -EINVAL;
	}
	return 0;
}

static int cppdrv_scale_set_regs(void *arg1, void *arg2)
{
	int ret = 0;
	int slice_support = 0;
	struct scale_drv_private *p = NULL;
	struct cpp_pipe_dev *cppif = NULL;
	struct sprd_cpp_scale_cfg_parm *sc_cfg = NULL;
	struct cpp_hw_info *hw = NULL;

	if (!arg1 || !arg2) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}
	cppif = (struct cpp_pipe_dev *)arg1;
	sc_cfg = (struct sprd_cpp_scale_cfg_parm *)arg2;
	p = &(cppif->scif->drv_priv);
	hw = cppif->hw_info;
	if (!p || !hw) {
		pr_err("fail to get valid drv_private %p, hw %p\n", p, hw);
		return -EINVAL;
	}

  	memcpy((void *)&p->cfg_parm, (void *)sc_cfg,
  		sizeof(struct sprd_cpp_scale_cfg_parm));

	slice_support = cppdrv_get_slice_support(cppif);
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_IN_FORMAT_SET,p);
	if (ret) {
		pr_err("fail to get valid input format\n");
		goto exit;
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_OUT_FORMAT_SET,p);
	if (ret) {
		pr_err("fail to get valid output format\n");
		goto exit;
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_IN_ENDIAN_SET,p);
	if (ret) {
		pr_err("fail to get valid input endian\n");
		goto exit;
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_OUT_ENDIAN_SET,p);
	if (ret) {
		pr_err("fail to get valid output endian\n");
		goto exit;
	}
	if (1 == slice_support) {
		ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_BURST_GAP_SET,p);
		if (ret) {
			pr_err("fail to set burst gap\n");
			goto exit;
		}

		ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_BPEN_SET,p);
		if (ret) {
			pr_err("fail to set bp en\n");
			goto exit;
		}
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_SRC_PITCH_SET,p);
	if (ret) {
		pr_err("fail to set valid src pitch\n");
		goto exit;
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_DES_PITCH_SET,p);
	if (ret) {
		pr_err("fail to set valid des pitch\n");
		goto exit;
	}

	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_DECI_SET,p);
	if (ret) {
		pr_err("fail to set valid deci\n");
		goto exit;
	}

	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_IN_RECT_SET,p);
	if (ret) {
		pr_err("fail to set valid input rect\n");
		goto exit;
	}

	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_OUT_RECT_SET,p);
	if (ret) {
		pr_err("fail to set output rect\n");
		goto exit;
	}
	if (1 == slice_support) {
		ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_OFFSET_SIZE_SET,p);
		if (ret) {
			pr_err("fail to set offset size\n");
			goto exit;
		}
	}
	ret = cppdrv_scale_addr_set(cppif, sc_cfg);
	if (ret) {
		pr_err("fail to get valid output addr\n");
		goto exit;
	}
	if (1 == slice_support) {
		cppdrv_scale_coeff_set(cppif, sc_cfg);
		ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_INI_PHASE_SET,p);
		if (ret) {
			pr_err("fail to get valid ini phase\n");
			goto exit;
		}
		ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_TAP_SET,p);
		if (ret) {
			pr_err("fail to get valid tap value\n");
			goto exit;
		}
	}

exit:
	return ret;
}

static int cppdrv_scale_param_check(void *arg1, void *arg2)
{
	struct scale_drv_private *p = NULL;
	struct sprd_cpp_scale_cfg_parm *cfg_parm = NULL;
	int ret = 0;

	if (!arg1) {
		pr_err("fail to get Input ptr\n");
		return -EINVAL;
	}
	p = (struct scale_drv_private *)arg1;
	cfg_parm = &p->cfg_parm;

	if (cfg_parm->input_size.w > SCALE_FRAME_WIDTH_MAX ||
		cfg_parm->input_size.h > SCALE_FRAME_HEIGHT_MAX ||
		cfg_parm->output_size.w > SCALE_FRAME_OUT_WIDTH_MAX ||
		cfg_parm->output_size.h > SCALE_FRAME_HEIGHT_MAX) {
		pr_err("fail to get valid input or output size:%d %d %d %d\n",
			cfg_parm->input_size.w, cfg_parm->input_size.h,
			cfg_parm->output_size.w, cfg_parm->output_size.h);
		ret = -1;
		goto exit;
	} else if (cfg_parm->input_size.w < cfg_parm->input_rect.w +
		cfg_parm->input_rect.x ||
		cfg_parm->input_size.h < cfg_parm->input_rect.h +
		cfg_parm->input_rect.y) {
		pr_err("fail to get valid input size %d %d %d %d %d %d\n",
			cfg_parm->input_size.w, cfg_parm->input_size.h,
			cfg_parm->input_rect.x, cfg_parm->input_rect.y,
			cfg_parm->input_rect.w, cfg_parm->input_rect.h);
		ret = -1;
		goto exit;
	} else {
		if (cfg_parm->output_size.w % 8 != 0) {
			pr_err("fail to get dst width 8 byte align: %d\n",
					cfg_parm->input_rect.x);
			ret = -1;
			goto exit;
		}
		if (cfg_parm->output_format == SCALE_YUV420)
			if (cfg_parm->output_size.h % 2 != 0) {
			pr_err("fail to get dst height 2 byte align: %d\n",
					cfg_parm->input_rect.x);
			ret = -1;
			goto exit;
			}
		if (cfg_parm->input_size.w % 8 != 0) {
			pr_err("fail to get src scale pitch size %d\n",
				cfg_parm->input_size.w);
			ret = -1;
			goto exit;
		}
		if (cfg_parm->input_format == SCALE_YUV420) {
			if (cfg_parm->input_rect.h % 2 != 0) {
				cfg_parm->input_rect.h =
					ALIGNED_DOWN_2(cfg_parm->input_rect.h);
				pr_info("adjust src height align 2: %d\n",
					cfg_parm->input_rect.y);
			}
			if (cfg_parm->input_rect.y % 2 != 0) {
				cfg_parm->input_rect.y =
					ALIGNED_DOWN_2(cfg_parm->input_rect.y);
				pr_info("adjust src offset y align 2: %d\n",
					cfg_parm->input_rect.y);
			}
		}
		if (cfg_parm->input_rect.w % 4 != 0) {
			cfg_parm->input_rect.w =
				ALIGNED_DOWN_4(cfg_parm->input_rect.w);
			pr_info("adjust src width align 4: %d\n",
					cfg_parm->input_rect.y);
		}
		if (cfg_parm->input_rect.x % 2 != 0) {
			cfg_parm->input_rect.x =
				ALIGNED_DOWN_2(cfg_parm->input_rect.x);
			pr_info("adjust src offset x align 2: %d\n",
					cfg_parm->input_rect.x);
		}
	}
exit:
	return ret;
}

static int cppdrv_scale_slice_param_check(void *arg1, void *arg2)
{
	struct scale_drv_private *p = NULL;
	if (!arg1) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}
	p = (struct scale_drv_private *)arg1;
	if (p->src_pitch > SCALE_FRAME_WIDTH_MAX ||
		MOD(p->src_pitch, 8) != 0 ||
		CMP(p->src_pitch, p->src_rect.w, p->src_rect.x) ||
		OSIDE(p->src_pitch,
			SCALE_FRAME_WIDTH_MIN, SCALE_FRAME_WIDTH_MAX)) {
		pr_err("fail to checkpitch %d\n", p->src_pitch);
		return -1;
	}
	if (MOD(p->sc_des_pitch, 8) != 0 ||
		OSIDE(p->sc_des_pitch, SCALE_FRAME_WIDTH_MIN,
			SCALE_FRAME_HEIGHT_MAX) ||
		CMP(p->sc_des_pitch, p->sc_des_rect.w, p->sc_des_rect.x)) {
		pr_err("fail to check sc des pitch %d\n", p->sc_des_pitch);
		return -1;
	}
	if ((p->bp_en == 1) && (MOD(p->bp_des_pitch, 8) != 0 ||
		OSIDE(p->bp_des_pitch, BP_TRIM_SIZE_MIN, BP_TRIM_SIZE_MAX) ||
		CMP(p->bp_des_pitch, p->bp_des_rect.w, p->bp_des_rect.x))) {
		pr_err("fail to check bp des pitch %d\n", p->bp_des_pitch);
		return -1;
	}
	if (OSIDE(p->src_rect.w, SCALE_FRAME_WIDTH_MIN,
		SCALE_FRAME_WIDTH_MAX) ||
		OSIDE(p->src_rect.h,
			SCALE_FRAME_HEIGHT_MIN, SCALE_FRAME_HEIGHT_MAX)) {
		pr_err("fail to check src_rec.h %d\n", p->src_rect.h);
		return -1;
	}
	if (p->src_rect.w % (2 << p->hor_deci) != 0) {
		pr_err("fail to check src_rect.w %d\n", p->src_rect.w);
		return -1;
	}
	if ((p->input_fmt == SCALE_YUV420) &&
		(p->src_rect.h % (2 <<  p->ver_deci) != 0)) {
		pr_err("fail to check src_rect.h %d\n", p->src_rect.h);
		return -1;
	}
	if ((p->input_fmt == SCALE_YUV422) &&
		(p->src_rect.h % (1 << p->ver_deci) != 0)) {
		pr_err("fail to check src_rect.h %d\n", p->src_rect.h);
		return -1;
	}
	if (MOD2(p->src_rect.x) != 0) {
		pr_err("fail to check src offset x %d\n", p->src_rect.x);
		return -1;
	}
	if (((p->input_fmt == SCALE_YUV420) && MOD2(p->src_rect.y) != 0) ||
		((p->input_fmt == SCALE_YUV422) &&
		MOD(p->src_rect.y, 1) != 0)) {
		pr_err("fail to check src offset y %d\n", p->src_rect.y);
		return -1;
	}
	if ((MOD2(p->sc_des_rect.w) != 0) ||
		OSIDE(p->sc_des_rect.w, SCALE_WIDTH_MIN,
			SCALE_SLICE_OUT_WIDTH_MAX) ||
		OSIDE(p->sc_des_rect.h, SCALE_HEIGHT_MIN,
			SCALE_FRAME_WIDTH_MAX)) {
		pr_err("fail to check des width or height w:%d h:%d\n",
			p->sc_des_rect.w, p->sc_des_rect.h);
		return -1;
	}
	if (((p->sc_output_fmt == SCALE_YUV420) &&
		MOD2(p->sc_des_rect.h) != 0) ||
		((p->input_fmt == SCALE_YUV422) &&
		MOD(p->sc_des_rect.h, 1) != 0)) {
		pr_err("fail to check sc_des height or align h:%d,format:%d\n",
			p->sc_des_rect.h, p->input_fmt);
		return -1;
	}
	if (MOD2(p->sc_des_rect.x) != 0) {
		pr_err("fail to check sc_des align %d\n", p->sc_des_rect.x);
		return -1;
	}
	if (((p->sc_output_fmt == SCALE_YUV420) &&
		MOD2(p->sc_des_rect.y) != 0) ||
		((p->sc_output_fmt == SCALE_YUV422) &&
		MOD(p->sc_des_rect.y, 1) != 0)) {
		pr_err("fail to check sc_des align h:%d,format:%d\n",
			p->sc_des_rect.y, p->input_fmt);
		return -1;
	}
	if ((p->bp_en == 1) && (MOD2(p->bp_des_rect.w) != 0 ||
		OSIDE(p->bp_des_rect.w, BP_TRIM_SIZE_MIN,
			SCALE_FRAME_HEIGHT_MAX) ||
		OSIDE(p->bp_des_rect.h, BP_TRIM_SIZE_MIN,
			SCALE_FRAME_HEIGHT_MAX))) {
		pr_err("fail to check bp_des width or height %d %d\n",
			p->bp_des_rect.w, p->bp_des_rect.h);
		return -1;
	}
	if ((p->bp_en == 1) && (((p->input_fmt == SCALE_YUV420) &&
		MOD2(p->bp_des_rect.h) != 0) ||
		((p->input_fmt == SCALE_YUV422) &&
		MOD(p->bp_des_rect.h, 1) != 0))) {
		pr_err("fail to check bp_des height align h:%d,format:%d\n",
			p->bp_des_rect.h, p->input_fmt);
		return -1;
	}
	if ((p->bp_en == 1) && (MOD2(p->bp_des_rect.x) != 0||
		(p->bp_des_rect.x > SCALE_FRAME_HEIGHT_MAX)||
		(p->bp_des_rect.y > SCALE_FRAME_HEIGHT_MAX)) ){
		pr_err("fail to check bp_des offset x:%d,y:%d\n",
			p->bp_des_rect.x, p->bp_des_rect.y);
		return -1;
	}
	if ((p->bp_en == 1) && (((p->input_fmt == SCALE_YUV420) &&
		MOD2(p->bp_des_rect.y) != 0) ||
		((p->input_fmt == SCALE_YUV422) &&
		MOD(p->bp_des_rect.y, 1) != 0))) {
		pr_err("fail to check bp output offset_y align h:%d,format:%d\n",
			p->bp_des_rect.y, p->input_fmt);
		return -1;
	}
	if (MOD2(p->sc_intrim_rect.w) != 0 ||
		p->sc_intrim_rect.w < SCALE_WIDTH_MIN ||
		p->sc_intrim_rect.h < SCALE_HEIGHT_MIN) {
		pr_err("fail to check sc in trim width:%d,height:%d\n",
			p->sc_intrim_rect.w, p->sc_intrim_rect.h);
		return -1;
	}
	if (CMP((p->src_rect.w >> p->hor_deci), p->sc_intrim_rect.w,
		p->sc_intrim_rect.x) ||
		CMP((p->src_rect.h >> p->ver_deci), p->sc_intrim_rect.h,
		p->sc_intrim_rect.y)) {
		pr_err("fail to check sc_trim in size.\n");
		pr_err("[src.w:%d >= intrim.w:%d + intrim.x:%d]\n",
			p->src_rect.w >> p->hor_deci,
			p->sc_intrim_rect.w, p->sc_intrim_rect.x);
		pr_err("[src.h:%d >= intrim.h:%d + intrim.y:%d]\n",
			p->src_rect.h >> p->ver_deci,
			p->sc_intrim_rect.h, p->sc_intrim_rect.y);
		return -1;
	}
	if (((p->input_fmt == SCALE_YUV420) &&
		MOD2(p->sc_intrim_rect.h) != 0) ||
		((p->input_fmt == SCALE_YUV422) &&
		MOD(p->sc_intrim_rect.h, 1) != 0)) {
		pr_err("fail to check sc in trim align h:%d format:%d\n",
			p->sc_intrim_rect.h, p->input_fmt);
		return -1;
	}
	if (MOD2(p->sc_intrim_rect.x) != 0) {
		pr_err("fail to check sc in trim offset_x:%d\n",
			p->sc_intrim_rect.x);
		return -1;
	}
	if (((p->input_fmt == SCALE_YUV420) &&
		MOD2(p->sc_intrim_rect.y) != 0) ||
		((p->input_fmt == SCALE_YUV422) &&
		MOD(p->sc_intrim_rect.y, 1) != 0)) {
		pr_err("fail to check sc in trim align offset_y:%d format:%d\n",
			p->sc_intrim_rect.y, p->input_fmt);
		return -1;
	}
	if (MOD2(p->sc_slice_in_size.w) != 0) {
		pr_err("fail to check slice in width:%d\n",
			p->sc_slice_in_size.w);
		return -1;
	}
	if (((p->input_fmt == SCALE_YUV420) &&
		MOD2(p->sc_slice_in_size.h) != 0) ||
		((p->input_fmt == SCALE_YUV422) &&
		MOD(p->sc_slice_in_size.h, 1) != 0)) {
		pr_err("fail to check slice in height:%d format:%d\n",
			p->sc_slice_in_size.h, p->input_fmt);
		return -1;
	}
	if (MOD2(p->sc_slice_out_size.w) != 0) {
		pr_err("fail to check slice out width:%d\n",
			p->sc_slice_out_size.w);
		return -1;
	}
	if (((p->input_fmt == SCALE_YUV420) &&
		MOD2(p->sc_slice_out_size.h) != 0) ||
		((p->input_fmt == SCALE_YUV422) &&
		MOD(p->sc_slice_out_size.h, 1) != 0)) {
		pr_err("fail to check slice out height:%d format:%d\n",
			p->sc_slice_out_size.h, p->input_fmt);
		return -1;
	}
	if (MOD2(p->sc_outtrim_rect.w) != 0) {
		pr_err("fail to check out trim width:%d\n",
			p->sc_outtrim_rect.w);
		return -1;
	}
	if (((p->sc_output_fmt == SCALE_YUV420) &&
		MOD2(p->sc_outtrim_rect.h) != 0) ||
		((p->sc_output_fmt == SCALE_YUV422) &&
		MOD(p->sc_outtrim_rect.h, 1) != 0)) {
		pr_err("fail to check sc out trim h:%d align,format:%d\n",
			p->sc_outtrim_rect.h, p->sc_output_fmt);
		return -1;
	}
	if (MOD2(p->sc_intrim_rect.x) != 0) {
		pr_err("fail to check sc in trim offset_x:%d align\n",
			p->sc_intrim_rect.x);
		return -1;
	}
	if (((p->sc_output_fmt == SCALE_YUV420) &&
		MOD2(p->sc_outtrim_rect.y) != 0) ||
		((p->sc_output_fmt == SCALE_YUV422) &&
		MOD(p->sc_outtrim_rect.y, 1) != 0)) {
		pr_err("fail to check sc out trim offset_y:%d format:%d\n",
			p->sc_outtrim_rect.y, p->sc_output_fmt);
		return -1;
	}
	if ((p->bp_en == 1) && MOD2(p->bp_trim_rect.w) != 0) {
		pr_err("fail to check bp trim width:%d align\n",
			p->bp_trim_rect.w);
		return -1;
	}
	if ((p->bp_en == 1) && (((p->input_fmt == SCALE_YUV420) &&
		MOD2(p->bp_trim_rect.h) != 0) ||
		((p->input_fmt == SCALE_YUV422) &&
		MOD(p->bp_trim_rect.h, 1) != 0))) {
		pr_err("fail to check bp trim height:%d format:%d\n",
			p->bp_trim_rect.h, p->input_fmt);
		return -1;
	}
	if ((p->bp_en == 1) && MOD2(p->bp_trim_rect.x) != 0) {
		pr_err("fail to check bp_trim align x:%d\n",
			p->bp_trim_rect.x);
		return -1;
	}
	if ((p->bp_en == 1) &&( ((p->input_fmt == SCALE_YUV420) &&
		MOD2(p->bp_trim_rect.y) != 0) ||
		((p->input_fmt == SCALE_YUV422) &&
		MOD(p->bp_trim_rect.y, 1) != 0))) {
		pr_err("fail to check bp_trim align y:%d format:%d\n",
			p->bp_trim_rect.y, p->input_fmt);
		return -1;
	}
	return 0;
}

static int cppdrv_scale_cfg_param_set(void *arg1, void *arg2)
{
	int ret = 0;
	struct scale_drv_private *p = NULL;
	struct cpp_pipe_dev *cppif = NULL;
	struct sprd_cpp_scale_cfg_parm *sc_cfg  = NULL;

	if (!arg1 || !arg2 ) {
		pr_err("fail to get valid input ptr\n");
		ret = -1;
		return ret;
	}
	cppif = (struct cpp_pipe_dev *)arg1;
	sc_cfg = (struct sprd_cpp_scale_cfg_parm *)arg2;
	p = &(cppif->scif->drv_priv);
	if (!p) {
		pr_err("fail to get valid scale_drv_private\n");
		ret = -1;
		return ret;
	}

	p->input_endian = sc_cfg->input_endian.y_endian;
	p->input_uv_endian = sc_cfg->input_endian.uv_endian;
	p->output_endian = sc_cfg->output_endian.y_endian;
	p->output_uv_endian = sc_cfg->output_endian.uv_endian;
	p->rch_burst_gap = 0;
	p->wch_burst_gap = 0;
	return ret;
}

static int cppdrv_scale_slice_param_set(void *arg1, void *arg2)
{
	int ret = 0;

	struct scale_drv_private *p = NULL;
	struct cpp_pipe_dev *cppif = NULL;
	struct sprd_cpp_hw_slice_parm *convert_param = NULL;

	if (!arg1 || !arg2) {
		pr_err("fail to get valid input ptr\n");
		ret = -1;
		return ret;
	}
	cppif = (struct cpp_pipe_dev *)arg1;
	convert_param = (struct sprd_cpp_hw_slice_parm *)arg2;
	p = &(cppif->scif->drv_priv);
	if (!p) {
		pr_err("fail to get valid scale_drv_private\n");
		ret = -1;
		return ret;
	}

	p->src_pitch = convert_param->path0_src_pitch;
	p->src_rect.x = convert_param->path0_src_offset_x;
	p->src_rect.y = convert_param->path0_src_offset_y;
	p->src_rect.w = convert_param->path0_src_width;
	p->src_rect.h = convert_param->path0_src_height;
	p->ver_deci = convert_param->deci_param.ver;
	p->hor_deci = convert_param->deci_param.hor;
	p->input_fmt = convert_param->input_format;
	p->sc_intrim_src_size.w = convert_param->sc_in_trim_src.w;
	p->sc_intrim_src_size.h = convert_param->sc_in_trim_src.h;
	p->sc_intrim_rect.x = convert_param->sc_in_trim.x;
	p->sc_intrim_rect.y = convert_param->sc_in_trim.y;
	p->sc_intrim_rect.w = convert_param->sc_in_trim.w;
	p->sc_intrim_rect.h = convert_param->sc_in_trim.h;
	p->sc_slice_in_size.w = convert_param->sc_slice_in_width;
	p->sc_slice_in_size.h = convert_param->sc_slice_in_height;
	p->sc_slice_out_size.w = convert_param->sc_slice_out_width;
	p->sc_slice_out_size.h = convert_param->sc_slice_out_height;
	p->sc_full_in_size.w = convert_param->sc_full_in_width;
	p->sc_full_in_size.h = convert_param->sc_full_in_height;
	p->sc_full_out_size.w = convert_param->sc_full_out_width;
	p->sc_full_out_size.h = convert_param->sc_full_out_height;
	p->y_hor_ini_phase_int = convert_param->y_hor_ini_phase_int;
	p->y_hor_ini_phase_frac = convert_param->y_hor_ini_phase_frac;
	p->y_ver_ini_phase_int = convert_param->y_ver_ini_phase_int;
	p->y_ver_ini_phase_frac = convert_param->y_ver_ini_phase_frac;
	p->uv_hor_ini_phase_int = convert_param->uv_hor_ini_phase_int;
	p->uv_hor_ini_phase_frac = convert_param->uv_hor_ini_phase_frac;
	p->uv_ver_ini_phase_int = convert_param->uv_ver_ini_phase_int;
	p->uv_ver_ini_phase_frac = convert_param->uv_ver_ini_phase_frac;
	p->y_ver_tap = convert_param->y_ver_tap;
	p->uv_ver_tap = convert_param->uv_ver_tap;
	p->sc_out_trim_src_size.w = convert_param->sc_out_trim_src.w;
	p->sc_out_trim_src_size.h = convert_param->sc_out_trim_src.h;
	p->sc_outtrim_rect.x = convert_param->sc_out_trim.x;
	p->sc_outtrim_rect.y = convert_param->sc_out_trim.y;
	p->sc_outtrim_rect.w = convert_param->sc_out_trim.w;
	p->sc_outtrim_rect.h = convert_param->sc_out_trim.h;
	p->sc_des_pitch = convert_param->path0_sc_des_pitch;
	p->sc_des_rect.w = convert_param->path0_sc_des_width;
	p->sc_des_rect.h = convert_param->path0_sc_des_height;
	p->sc_des_rect.x = convert_param->path0_sc_des_offset_x;
	p->sc_des_rect.y = convert_param->path0_sc_des_offset_y;
	p->sc_output_fmt = convert_param->path0_sc_output_format;
	p->bp_en = convert_param->path0_bypass_path_en;
	p->bp_trim_src_size.w = convert_param->bypass_trim_src.w;
	p->bp_trim_src_size.h = convert_param->bypass_trim_src.h;
	p->bp_trim_rect.x = convert_param->bypass_trim.x;
	p->bp_trim_rect.y = convert_param->bypass_trim.y;
	p->bp_trim_rect.w = convert_param->bypass_trim.w;
	p->bp_trim_rect.h = convert_param->bypass_trim.h;
	p->bp_des_rect.x = convert_param->path0_bypass_des_offset_x;
	p->bp_des_rect.y = convert_param->path0_bypass_des_offset_y;
	p->bp_des_rect.w = convert_param->path0_bypass_des_width;
	p->bp_des_rect.h = convert_param->path0_bypass_des_height;
	p->bp_des_pitch = convert_param->path0_bypass_des_pitch;

	return ret;
}

static int cppdrv_scale_start(void *arg1, void *arg2)
{
	struct scale_drv_private *p = NULL;
	struct cpp_pipe_dev *cppif = NULL;
	struct cpp_hw_info *hw = NULL;
	int ret = 0;

	if (!arg1) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}
	cppif = (struct cpp_pipe_dev *)arg1;
	p = &(cppif->scif->drv_priv);
	hw = cppif->hw_info;
	if (!p || !hw) {
		pr_err("fail to get valid drv_private %p, hw %p\n", p, hw);
		return -EINVAL;
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_START,p);
	if (ret) {
		pr_err("fail to start scl\n");
		return -EINVAL;
	}
	return 0;
}

static int cppdrv_scale_stop(void *arg1, void *arg2)
{
	struct scale_drv_private *p = NULL;
	struct cpp_pipe_dev *cppif = NULL;
	struct cpp_hw_info *hw = NULL;
	int bp_support = 0;
	int ret = 0;

	if (!arg1) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}
	cppif = (struct cpp_pipe_dev *)arg1;
	p = &(cppif->scif->drv_priv);
	hw = cppif->hw_info;
	if (!p || !hw) {
		pr_err("fail to get valid drv_private %p, hw %p\n", p, hw);
		return -EINVAL;
	}
	bp_support = cppdrv_get_bp_support(cppif);
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_STOP,p);
	if (ret) {
		pr_err("fail to stop scl\n");
		return -EINVAL;
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCL_DISABLE,p);
	if (ret) {
		pr_err("fail to disable scl\n");
		return -EINVAL;
	}
	cppdrv_free_addr(&p->iommu_src);
	cppdrv_free_addr(&p->iommu_dst);
	if (1 == bp_support) {
		if (p->bp_en == 1)
			cppdrv_free_addr(&p->iommu_dst_bp);
	}
	return 0;
}

static int cppdrv_dma_enable(void *arg1, void *arg2)
{
	struct dma_drv_private *p = NULL;
	struct cpp_pipe_dev *cppif= NULL;
	struct cpp_hw_info *hw = NULL;
	int ret = 0;

	if (!arg2) {
		pr_err("fail to get valid input ptr\n");
		return -1 ;
	}
	cppif = (struct cpp_pipe_dev *)arg2;
	p = &(cppif->dmaif->drv_priv);
	hw = cppif->hw_info;
	if (!p || !hw) {
		pr_err("fail to get valid drv_private %p, hw %p\n", p, hw);
		return -EINVAL;
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_DMA_EB, p);
	if (ret) {
		pr_err("fail to eb dma\n");
		return -EINVAL;
	}

	return 0;
}

static int cppdrv_dma_param_set(void *arg1, void *arg2)
{
	int ret = 0;
	struct dma_drv_private *p = NULL;
	struct sprd_cpp_dma_cfg_parm *parm = NULL;
	struct cpp_pipe_dev *cppif = NULL;

	if (!arg1 || !arg2) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	parm = (struct sprd_cpp_dma_cfg_parm *)arg1;
	cppif = (struct cpp_pipe_dev *)arg2;
	p = &(cppif->dmaif->drv_priv);
	if (!p) {
		pr_err("fail to get valid dma_drv_private\n");
		return -EINVAL;
	}
	memcpy((void *)&p->cfg_parm, (void *)parm,
		sizeof(struct sprd_cpp_dma_cfg_parm));
	memcpy(p->iommu_src.mfd, parm->input_addr.mfd,
		sizeof(parm->input_addr.mfd));
	memcpy(p->iommu_dst.mfd, parm->output_addr.mfd,
		sizeof(parm->output_addr.mfd));

	ret = cppdrv_get_sg_table(&p->iommu_src);
	if (ret) {
		pr_err("fail to get cpp sg table\n");
		return -1;
	}
	p->iommu_src.offset[0] = p->cfg_parm.input_addr.y;
	p->iommu_src.offset[1] = p->cfg_parm.input_addr.u;
	ret = cppdrv_get_addr(&p->iommu_src);
	if (ret) {
		pr_err("fail to get src addr\n");
		return -EFAULT;
	}

	ret = cppdrv_get_sg_table(&p->iommu_dst);
	if (ret) {
		pr_err("fail to get cpp sg table\n");
		cppdrv_free_addr(&p->iommu_src);
		return -1;
	}
	p->iommu_dst.offset[0] = p->cfg_parm.output_addr.y;
	p->iommu_dst.offset[1] = p->cfg_parm.output_addr.u;
	ret = cppdrv_get_addr(&p->iommu_dst);
	if (ret) {
		pr_err("fail to get src addr\n");
		cppdrv_free_addr(&p->iommu_src);
		return -EFAULT;
	}
	p->dma_src_addr = p->iommu_src.iova[0];
	p->dma_dst_addr = p->iommu_dst.iova[0];
	pr_debug("src fd 0x%x, addr 0x%x, dst fd 0x%x, addr 0x%x\n",
		parm->input_addr.mfd[0], p->dma_src_addr,
		parm->output_addr.mfd[0], p->dma_dst_addr);

	return ret;

}

static int cppdrv_dma_start(void *arg1, void *arg2)

{
	struct dma_drv_private *p = NULL;
	struct cpp_pipe_dev *cppif= NULL;
	struct cpp_hw_info *hw = NULL;
	int ret = 0;

	if (!arg2) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	cppif = (struct cpp_pipe_dev *)arg2;
	p = &(cppif->dmaif->drv_priv);
	hw = cppif->hw_info;

	if (!p || !hw) {
		pr_err("fail to get valid drv_private %p, hw %p\n", p, hw);
		return -EINVAL;
	}

	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_DMA_PARM, p);
	if (ret) {
		pr_err("fail to cfg dma\n");
		return -EINVAL;
	}

	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_DMA_START,p);
	if (ret) {
		pr_err("fail to start dma\n");
		return -EINVAL;
	}

	return 0;

}

static int cppdrv_dma_stop(void *arg1, void *arg2)
{
	struct dma_drv_private *p = NULL;
	struct cpp_pipe_dev *cppif= NULL;
	struct cpp_hw_info *hw = NULL;
	int ret = 0;

	if (!arg2) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}
	cppif = (struct cpp_pipe_dev *)arg2;
	p = &(cppif->dmaif->drv_priv);
	hw = cppif->hw_info;
	if (!p || !hw) {
		pr_err("fail to get valid dma_drv_private %p, hw %p\n", p, hw);
		return -EINVAL;
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_DMA_STOP,p);
	if (ret) {
		pr_err("fail to stop dma\n");
		return -EINVAL;
	}
	udelay(1);
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_DMA_DISABLE,p);
	if (ret) {
		pr_err("fail to disable dma\n");
		return -EINVAL;
	}
	cppdrv_free_addr(&p->iommu_src);
	cppdrv_free_addr(&p->iommu_dst);
	return ret;
}

static int cppdrv_scale_capability_get(void *arg1, void *arg2)
{
	struct sprd_cpp_scale_capability *scale_param = NULL;
	struct cpp_pipe_dev *cppif = NULL;
	int zoom_up_support;

	if (!arg1 || !arg2) {
		pr_err("fail to get valid input ptr\n");
		return -1;
	}
	scale_param = (struct sprd_cpp_scale_capability *)arg2;
	cppif = (struct cpp_pipe_dev *)arg1;

	if ((scale_param->src_format != SCALE_YUV420 &&
		scale_param->src_format != SCALE_YUV422) ||
		(scale_param->dst_format != SCALE_YUV420 &&
		scale_param->dst_format != SCALE_YUV422)) {
		pr_debug("get invalid format src %d dst %d\n",
			scale_param->src_format, scale_param->dst_format);
		return -1;
	}

	zoom_up_support = cppdrv_get_zoomup_support(cppif);
	if (0 == zoom_up_support){
		if (scale_param->dst_size.w > scale_param->src_size.w ||
			scale_param->dst_size.h > scale_param->src_size.h) {
			pr_err("fail to upscale src.w:%d h:%d dst w:%d h:%d\n",
				scale_param->src_size.w, scale_param->src_size.h,
				scale_param->dst_size.w, scale_param->dst_size.h);
			return -1;
		}
	}

	if (scale_param->src_size.w > SCALE_FRAME_WIDTH_MAX ||
		scale_param->src_size.h > SCALE_FRAME_HEIGHT_MAX ||
		scale_param->src_size.w < SCALE_FRAME_WIDTH_MIN ||
		scale_param->src_size.h < SCALE_FRAME_HEIGHT_MIN ||
		scale_param->dst_size.w < SCALE_FRAME_WIDTH_MIN ||
		scale_param->dst_size.h < SCALE_FRAME_HEIGHT_MIN ||
		scale_param->dst_size.w > SCALE_FRAME_OUT_WIDTH_MAX ||
		scale_param->dst_size.h > SCALE_FRAME_HEIGHT_MAX) {
		return -1;
	}

	if (MOD(scale_param->src_size.w, CPP_SRC_W_ALIGN) != 0)
		return -1;

	if (MOD(scale_param->dst_size.w, CPP_DST_W_ALIGN) != 0)
		return -1;

	if ((scale_param->dst_format == SCALE_YUV420)
		&& (MOD(scale_param->dst_size.h, CPP_DST_YUV420_H_ALIGN) != 0))
		return -1;

	if ((scale_param->dst_format == SCALE_YUV422)
		&& (MOD(scale_param->dst_size.h, CPP_DST_YUV422_H_ALIGN) != 0))
		return -1;

	scale_param->is_supported = 1;

	return 0;
}

static int cppdrv_sc_reg_trace(void *arg1, void *arg2)
{
	int ret = 0;
	struct scale_drv_private *p = NULL;
	struct cpp_pipe_dev *cppif = NULL;
	struct cpp_hw_info *hw = NULL;

	if (!arg1) {
		pr_err("fail to get valid input ptr\n");
		ret = -1;
		return ret;
	}
	cppif = (struct cpp_pipe_dev *)arg1;
	p = &(cppif->scif->drv_priv);
	hw = cppif->hw_info;
	if (!p || !hw) {
		pr_err("fail to get valid drv_private %p, hw %p\n", p, hw);
		ret = -EINVAL;
		return ret;
	}

	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SC_REG_TRACE,p);
	if (ret) {
		pr_err("fail to trace scl\n");
		return -EINVAL;
	}

	return ret;
}

static int cppdrv_rot_reg_trace(void *arg1, void *arg2)
{
	int ret = 0;
	struct rot_drv_private *p = NULL;
	struct cpp_pipe_dev *cppif = NULL;
	struct cpp_hw_info *hw = NULL;

	if (!arg1) {
		pr_err("fail to get valid input ptr\n");
		ret = -1;
		return ret;
	}
	cppif = (struct cpp_pipe_dev *)arg1;
	p = &(cppif->rotif->drv_priv);
	hw = cppif->hw_info;
	if (!p || !hw) {
		pr_err("fail to get valid drv_private %p, hw %p\n", p, hw);
		ret = -EINVAL;
		return ret;
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_ROT_REG_TRACE,p);
	if (ret) {
		pr_err("fail to trace rot\n");
		return -EINVAL;
	}
	return ret;
}

static int cppdrv_qos_set(void *arg1, void *arg2)
{
	int ret = 0;
	struct cpp_hw_soc_info *soc_cpp = NULL;
	struct cpp_hw_info *hw = NULL;

	if (!arg1 || !arg2) {
		pr_err("fail to get valid input ptr arg1 %p, arg2 %p\n", arg1, arg2);
		ret = -1;
		return ret;
	}
	hw = (struct cpp_hw_info *)arg1;
	soc_cpp = (struct cpp_hw_soc_info *)arg2;

	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_QOS_SET, soc_cpp);
	if (ret) {
		pr_err("fail to set qos\n");
		return -EINVAL;
	}
	return ret;
}

static int cppdrv_mmu_set(void *arg1, void *arg2)
{
	int ret = 0;
	struct cpp_hw_soc_info *soc_cpp = NULL;
	struct cpp_hw_info *hw = NULL;

	if (!arg1) {
		pr_err("fail to get valid input ptr\n");
		ret = -1;
		return ret;
	}
	hw = (struct cpp_hw_info *)arg1;
	soc_cpp = (struct cpp_hw_soc_info *)arg2;

	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_MMU_SET, soc_cpp);
	if (ret) {
		pr_err("fail to set mmu\n");
		return -EINVAL;
	}
	return ret;
}

static int cppdrv_module_reset(void *arg1, void *arg2)
{
	int ret = 0;
	struct cpp_hw_soc_info *soc_cpp = NULL;
	struct cpp_hw_info *hw = NULL;

	if (!arg1 || !arg2) {
		pr_err("fail to get valid input ptr\n");
		ret = -1;
		return ret;
	}
	hw = (struct cpp_hw_info *)arg1;
	soc_cpp = (struct cpp_hw_soc_info *)arg2;

	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_MODULE_RESET, soc_cpp);
	if (ret) {
		pr_err("fail to reset cpp\n");
		return -EINVAL;
	}
	return ret;
}

static int cppdrv_rot_reset(void *arg1, void *arg2)
{
	int ret = 0;
	struct cpp_hw_soc_info *soc_cpp = NULL;
	struct cpp_hw_info *hw = NULL;

	if (!arg1 || !arg2) {
		pr_err("fail to get valid input ptr\n");
		ret = -1;
		return ret;
	}
	hw = (struct cpp_hw_info *)arg1;
	soc_cpp = (struct cpp_hw_soc_info *)arg2;

	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_ROT_RESET,soc_cpp);
	if (ret) {
		pr_err("fail to reset rot\n");
		return -EINVAL;
	}
	return ret;
}

static int cppdrv_scale_reset(void *arg1, void *arg2)
{
	int ret = 0;
	struct cpp_hw_soc_info *soc_cpp = NULL;
	struct cpp_hw_info *hw = NULL;

	if (!arg1 || !arg2) {
		pr_err("fail to get valid input ptr\n");
		ret = -1;
		return ret;
	}
	hw = (struct cpp_hw_info *)arg1;
	soc_cpp = (struct cpp_hw_soc_info *)arg2;

	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_SCALE_RESET, soc_cpp);
	if (ret) {
		pr_err("fail to reset cl\n");
		return -EINVAL;
	}
	return ret;
}

static int cppdrv_dma_reset(void *arg1, void *arg2)

{
	int ret = 0;
	struct cpp_hw_soc_info *soc_cpp = NULL;
	struct cpp_hw_info *hw = NULL;

	if (!arg1 || !arg2) {
		pr_err("fail to get valid input ptr\n");
		ret = -1;
		return ret;
	}
	hw = (struct cpp_hw_info *)arg1;
	soc_cpp = (struct cpp_hw_soc_info *)arg2;

	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_DMA_RESET, soc_cpp);
	if (ret) {
		pr_err("fail to reset dma\n");
		return -EINVAL;
	}
	return ret;
}

static int cppdrv_clk_eb(void *arg1, void *arg2)
{
	int ret = 0;
	struct cpp_hw_soc_info *soc_cpp = NULL;
	struct cpp_hw_info *hw = NULL;

	if (!arg1 || !arg2) {
		pr_err("fail to get valid input ptr\n");
		ret = -1;
		return ret;
	}
	hw = (struct cpp_hw_info *)arg1;
	soc_cpp = (struct cpp_hw_soc_info *)arg2;

	if (!hw) {
		pr_err("fail to get valid  hw %p\n", hw);
		ret = -EINVAL;
		return ret;
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_CLK_EB, soc_cpp);
	if (ret) {
		pr_err("fail to eb clk\n");
		return -EINVAL;
	}
	return ret;
}

static int cppdrv_clk_disable(void *arg1, void *arg2)
{
	int ret = 0;
	struct cpp_hw_soc_info *soc_cpp = NULL;
	struct cpp_hw_info *hw = NULL;

	if (!arg1 || !arg2) {
		pr_err("fail to get valid input ptr\n");
		ret = -1;
		return ret;
	}
	hw = (struct cpp_hw_info *)arg1;
	soc_cpp = (struct cpp_hw_soc_info *)arg2;

	if (!hw) {
		pr_err("fail to get valid  hw %p\n", hw);
		ret = -EINVAL;
		return ret;
	}
	ret = hw->cpp_hw_ioctl(CPP_HW_CFG_CLK_DIS, soc_cpp);
	if (ret) {
		pr_err("fail to dis clk\n");
		return -EINVAL;
	}
	return ret;
}

static struct cppdrv_ioctrl cppdrv_ioctl_fun_tab[] = {
	{CPP_DRV_SCL_SUPPORT_INFO_GET,    cppdrv_get_support_info},
	{CPP_DRV_SCL_MAX_SIZE_GET,        cppdrv_scale_max_size_get},
	{CPP_DRV_SCL_START,               cppdrv_scale_start},
	{CPP_DRV_SCL_STOP,                cppdrv_scale_stop},
	{CPP_DRV_SCL_CAPABILITY_GET,      cppdrv_scale_capability_get},
	{CPP_DRV_SCL_EB,                  cppdrv_scale_enable},
	{CPP_DRV_SCL_CFG_PARAM_SET,       cppdrv_scale_cfg_param_set},
	{CPP_DRV_SCL_REG_SET,             cppdrv_scale_set_regs},
	{CPP_DRV_SCL_SLICE_PARAM_CHECK,   cppdrv_scale_slice_param_check},
	{CPP_DRV_SCL_SLICE_PARAM_SET,     cppdrv_scale_slice_param_set},
	{CPP_DRV_SCL_PARAM_CHECK,         cppdrv_scale_param_check},
	{CPP_DRV_SCL_SL_STOP,             cppdrv_scale_sl_stop},
	{CPP_DRV_SCL_REG_TRACE,           cppdrv_sc_reg_trace},
	{CPP_DRV_ROT_PARM_CHECK,          cppdrv_rot_parm_check},
	{CPP_DRV_ROT_END,                 cppdrv_rot_is_end},
	{CPP_DRV_ROT_Y_PARM_SET,          cppdrv_rot_y_parm_set},
	{CPP_DRV_ROT_UV_PARM_SET,         cppdrv_rot_uv_parm_set},
	{CPP_DRV_ROT_START,               cppdrv_rot_start},
	{CPP_DRV_ROT_STOP,                cppdrv_rot_stop},
	{CPP_DRV_ROT_REG_TRACE,           cppdrv_rot_reg_trace},
	{CPP_DRV_QOS_SET,                 cppdrv_qos_set},
	{CPP_DRV_MMU_SET,                 cppdrv_mmu_set},
	{CPP_DRV_MODULE_RESET,            cppdrv_module_reset},
	{CPP_DRV_ROT_RESET,               cppdrv_rot_reset},
	{CPP_DRV_SCL_RESET,               cppdrv_scale_reset},
	{CPP_DRV_CLK_EB,                  cppdrv_clk_eb},
	{CPP_DRV_CLK_DIS,                 cppdrv_clk_disable},
	{CPP_DRV_DMA_SET_PARM,            cppdrv_dma_param_set},
	{CPP_DRV_DMA_EB,                  cppdrv_dma_enable},
	{CPP_DRV_DMA_STOP,                cppdrv_dma_stop},
	{CPP_DRV_DMA_START,               cppdrv_dma_start},
	{CPP_DRV_DMA_RESET,               cppdrv_dma_reset}
};

static cppdrv_ioctl_fun cppdrv_ioctl_fun_get(
	enum cppdrv_cfg_cmd cmd)
{
	cppdrv_ioctl_fun hw_ctrl = NULL;
	uint32_t total_num = 0;
	uint32_t i = 0;

	total_num = sizeof(cppdrv_ioctl_fun_tab) / sizeof(struct cppdrv_ioctrl);
	for (i = 0; i < total_num; i++) {
		if (cmd == cppdrv_ioctl_fun_tab[i].cmd) {
			hw_ctrl = cppdrv_ioctl_fun_tab[i].hw_ctrl;
			break;
		}
	}

	return hw_ctrl;
}

static int cppdrv_ioctl(enum cppdrv_cfg_cmd cmd, void *arg1, void *arg2)
{
	int ret = 0;
	cppdrv_ioctl_fun hw_ctrl = NULL;

	hw_ctrl = cppdrv_ioctl_fun_get(cmd);
	if (hw_ctrl != NULL)
		ret = hw_ctrl(arg1, arg2);
	else
		pr_err("cppdrv_core_ctrl_fun is null, cmd %d", cmd);

	return ret;
}

/*
 * Operations for this cpp_pipe_dev.
 */
static struct cppdrv_ops cppdrv_ops = {
	.ioctl = cppdrv_ioctl,
};

static int cppdrv_ops_get(struct cpp_pipe_dev *dev)
{
	if (unlikely(!dev)) {
		pr_err("fail to get valid param \n");
		return 0;
	}

	dev->cppdrv_ops = &cppdrv_ops;

	return 0;
}

static int cppdrv_init(void *arg)
{
	int ret = 0;
	struct cpp_pipe_dev *dev;

	if (!arg) {
		pr_err("fail to get valid input ptr  %p\n",
			arg);
		ret= -EFAULT;
	}
	dev = (struct cpp_pipe_dev *)arg;
	cppdrv_ops_get(dev);
	return ret;
}

int cpp_drv_get_cpp_res(struct cpp_pipe_dev *cppif, struct cpp_hw_info *hw)
{
	int ret = 0;
	if (!cppif || !hw ) {
		pr_err("fail to get valid input dev %p, hw  %p\n",
			cppif, hw);
		ret= -EFAULT;
	} else {
	    cppif->hw_info = hw;
	    spin_lock_init(&cppif->slock);
	    cppif->pdev= hw->pdev;
	    cppif->irq = hw->ip_cpp->irq;
	    cppif->io_base = hw->ip_cpp->io_base;
	    atomic_set(&cpp_dma_cnt, 0);
	    ret = cppdrv_init(cppif);
	}
   return ret;
}
