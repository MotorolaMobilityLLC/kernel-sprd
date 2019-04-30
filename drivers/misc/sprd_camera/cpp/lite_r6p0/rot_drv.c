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
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <uapi/video/sprd_cpp.h>

#include "cpp_common.h"
#include "cpp_reg.h"
#include "rot_drv.h"

#define ROT_ADDR_ALIGN 0x07

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ROT_DRV: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

enum {
	ROT_ONE_BYTE = 0,
	ROT_TWO_BYTES,
	ROT_FOUR_BYTES,
	ROT_BYTE_MAX
};

enum {
	ROT_UV420 = 0,
	ROT_UV422,
	ROT_DATA_FORMAT_MAX
};

static int sprd_rotdrv_rot_format_get(struct sprd_cpp_rot_cfg_parm *parm)
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
	default:
		pr_info("fail to get invalid format\n");
		break;
	}

	return fmt;
}

int sprd_rot_drv_parm_check(struct sprd_cpp_rot_cfg_parm *parm)
{
	if (!parm) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}
#ifdef ROT_DRV_DEBUG
	pr_info("format %d angle %d w %d h %d\n",
		parm->format, parm->angle, parm->size.w, parm->size.h);
	pr_info("src mfd %u y:u:v 0x%x 0x%x 0x%x\n", parm->src_addr.mfd[0],
		parm->src_addr.y, parm->src_addr.u, parm->src_addr.v);
	pr_info("dst mfd %u y:u:v 0x%x 0x%x 0x%x\n", parm->dst_addr.mfd[0],
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

int sprd_rot_drv_is_end(struct sprd_cpp_rot_cfg_parm *parm)
{
	int ret = 1;

	if (!parm) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

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

int sprd_rot_drv_y_parm_set(struct sprd_cpp_rot_cfg_parm *parm,
			struct rot_drv_private *p)
{
	int ret = 0;

	if (!parm || !p) {
		pr_err("fail to get valid input ptr\n");
		return -EINVAL;
	}

	memcpy((void *)&p->cfg_parm, (void *)parm,
		sizeof(struct sprd_cpp_rot_cfg_parm));
	memcpy(p->iommu_src.mfd, parm->src_addr.mfd,
		sizeof(parm->src_addr.mfd));
	memcpy(p->iommu_dst.mfd, parm->dst_addr.mfd,
		sizeof(parm->dst_addr.mfd));

	ret = sprd_cpp_core_get_sg_table(&p->iommu_src);
	if (ret) {
		pr_err("fail to get cpp sg table\n");
		return -1;
	}
	p->iommu_src.offset[0] = p->cfg_parm.src_addr.y;
	p->iommu_src.offset[1] = p->cfg_parm.src_addr.u;
	ret = sprd_cpp_core_get_addr(&p->iommu_src);
	if (ret) {
		pr_err("fail to get src addr\n");
		return -EFAULT;
	}

	ret = sprd_cpp_core_get_sg_table(&p->iommu_dst);
	if (ret) {
		pr_err("fail to get cpp sg table\n");
		sprd_cpp_core_free_addr(&p->iommu_src);
		return -1;
	}
	p->iommu_dst.offset[0] = p->cfg_parm.dst_addr.y;
	p->iommu_dst.offset[1] = p->cfg_parm.dst_addr.u;
	ret = sprd_cpp_core_get_addr(&p->iommu_dst);
	if (ret) {
		pr_err("fail to get src addr\n");
		sprd_cpp_core_free_addr(&p->iommu_src);
		return -EFAULT;
	}
	p->rot_src_addr = p->iommu_src.iova[0];
	p->rot_dst_addr = p->iommu_dst.iova[0];
	p->uv_mode = ROT_UV420;
	p->rot_fmt = sprd_rotdrv_rot_format_get(parm);
	p->rot_size.w = parm->size.w;
	p->rot_size.h = parm->size.h;
	p->rot_mode = parm->angle;
	p->rot_endian = 0x5;

	return ret;
}

void sprd_rot_drv_uv_parm_set(struct rot_drv_private *p)
{
	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return;
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
}

static void sprd_rotdrv_dev_enable(struct rot_drv_private *p)
{
	unsigned long flags = 0;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_OWR(CPP_PATH_EB, CPP_ROT_EB_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
}

static void sprd_rotdrv_dev_disable(struct rot_drv_private *p)
{
	unsigned long flags = 0;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_AWR(CPP_PATH_EB, ~CPP_ROT_EB_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
}

static void sprd_rotdrv_dev_start(struct rot_drv_private *p)
{
	unsigned long flags = 0;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_OWR(CPP_PATH_START, CPP_ROT_START_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
}

static void sprd_rotdrv_dev_stop(struct rot_drv_private *p)
{
	unsigned long flags = 0;

	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_AWR(CPP_PATH_START, ~CPP_ROT_START_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
}

void sprd_rot_drv_start(struct rot_drv_private *p)
{
	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	sprd_rotdrv_dev_stop(p);
	sprd_rotdrv_dev_enable(p);
	CPP_REG_WR(CPP_ROTATION_SRC_ADDR, p->rot_src_addr);
	CPP_REG_WR(CPP_ROTATION_DES_ADDR, p->rot_dst_addr);

	pr_info("rot:src addr:0x%x, dst addr:0x%x\n",
			p->rot_src_addr, p->rot_dst_addr);

	CPP_REG_WR(CPP_ROTATION_OFFSET_START, 0x0);
	CPP_REG_WR(CPP_ROTATION_IMG_SIZE,
		((p->rot_size.w & 0xFFFF) | ((p->rot_size.h & 0xFFFF) << 16)));
	CPP_REG_WR(CPP_ROTATION_SRC_PITCH, (p->rot_size.w & 0xFFFF));

	CPP_REG_AWR(CPP_ROTATION_PATH_CFG, (~CPP_ROT_UV_MODE_BIT));
	CPP_REG_OWR(CPP_ROTATION_PATH_CFG, ((p->uv_mode & 0x1) << 4));

	CPP_REG_AWR(CPP_ROTATION_PATH_CFG, (~CPP_ROT_MODE_MASK));
	CPP_REG_OWR(CPP_ROTATION_PATH_CFG, ((p->rot_mode & 0x3) << 2));

	CPP_REG_AWR(CPP_ROTATION_PATH_CFG, (~CPP_ROT_PIXEL_FORMAT_BIT));
	CPP_REG_OWR(CPP_ROTATION_PATH_CFG, ((p->rot_fmt & 0x3) << 0));

	CPP_REG_AWR(CPP_AXIM_CHN_SET, (~CPP_PATH1_ENDIAN));
	CPP_REG_WR(CPP_AXIM_CHN_SET, (CPP_PATH1_ENDIAN & (p->rot_endian << 8)));

	CPP_REG_MWR(CPP_AXIM_CHN_SET, CPP_AXIM_CHN_SET_QOS_MASK, (0x1 << 24));
	CPP_REG_MWR(CPP_MMU_PT_UPDATE_QOS, CPP_MMU_PT_UPDATE_QOS_MASK, 0x1);

	sprd_rotdrv_dev_start(p);
}

void sprd_rot_drv_stop(struct rot_drv_private *p)
{
	if (!p) {
		pr_err("fail to get valid input ptr\n");
		return;
	}

	sprd_rotdrv_dev_stop(p);
	udelay(1);
	sprd_rotdrv_dev_disable(p);
	sprd_cpp_core_free_addr(&p->iommu_src);
	sprd_cpp_core_free_addr(&p->iommu_dst);
}
