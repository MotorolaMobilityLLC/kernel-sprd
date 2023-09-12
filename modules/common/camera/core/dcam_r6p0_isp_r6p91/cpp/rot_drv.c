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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include "sprd_rot_k.h"
#ifndef CONFIG_64BIT
/* #include <soc/sprd/hardware.h> */
#endif
//#include <soc/sprd/sci.h>
#include "img_rot.h"
#include "rot_drv.h"
#include "dcam_drv.h"
/**/
#include <sprd_mm.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <dt-bindings/soc/sprd,pike2-mask.h>
#include <dt-bindings/soc/sprd,pike2-regs.h>


#define ALGIN_FOUR 0x03

static struct platform_device *s_rot_pdev;
static struct regmap *cam_ahb_gpr;

int rot_k_module_en(struct device_node *dn)
{
	int ret = 0;

	ret = dcam_module_en(dn);

	if (ret)
		pr_err("dcam_module_en, failed  %d\n", ret);


	return ret;
}

int rot_k_module_dis(struct device_node *dn)
{
	int ret = 0;

	ret = dcam_module_dis(dn);

	if (ret)
		pr_err("rot_k_module_dis, failed  %d\n", ret);

	return ret;
}

/* pike2 rot_k_ahb_reset */
static void rot_k_ahb_reset(void)
{
	regmap_update_bits(cam_ahb_gpr,
				REG_MM_AHB_AHB_RST,
				MASK_MM_AHB_DCAM_ROT_SOFT_RST,
				MASK_MM_AHB_DCAM_ROT_SOFT_RST);
	regmap_update_bits(cam_ahb_gpr,
				REG_MM_AHB_AHB_RST,
				MASK_MM_AHB_DCAM_ROT_SOFT_RST,
				~(unsigned int)MASK_MM_AHB_DCAM_ROT_SOFT_RST);
}


static void rot_k_set_src_addr(uint32_t src_addr)
{
	REG_WR(REG_ROTATION_SRC_ADDR, src_addr);
}

static void rot_k_set_dst_addr(uint32_t dst_addr)
{
	REG_WR(REG_ROTATION_DST_ADDR, dst_addr);
}

static void rot_k_set_img_size(struct rot_size_tag *size)
{
	REG_WR(REG_ROTATION_OFFSET_START, 0x00000000);
	REG_WR(REG_ROTATION_IMG_SIZE,
		((size->w & 0x1FFF) | ((size->h & 0x1FFF) << 16)));
	REG_WR(REG_ROTATION_ORIGWIDTH, (size->w & 0x1FFF));
}

static void rot_k_set_endian(uint32_t src_end,
	uint32_t dst_end)
{
	dcam_glb_reg_awr(REG_ROTATION_ENDIAN_SEL,
		(~(ROT_RD_ENDIAN_MASK | ROT_WR_ENDIAN_MASK)), DCAM_ENDIAN_REG);

	dcam_glb_reg_owr(REG_ROTATION_ENDIAN_SEL,
		(ROT_AXI_RD_WORD_ENDIAN_BIT |
		ROT_AXI_WR_WORD_ENDIAN_BIT |
		(src_end << 16) | (dst_end << 14)), DCAM_ENDIAN_REG);
}

static void rot_k_set_pixel_mode(uint32_t pixel_format)
{
	REG_AWR(REG_ROTATION_PATH_CFG, (~ROT_PIXEL_FORMAT_BIT));
	REG_OWR(REG_ROTATION_PATH_CFG, ((pixel_format & 0x1) << 1));
}

static void rot_k_set_UV_mode(uint32_t uv_mode)
{
	REG_AWR(REG_ROTATION_PATH_CFG, (~ROT_UV_MODE_BIT));
	REG_OWR(REG_ROTATION_PATH_CFG, ((uv_mode & 0x1) << 4));
}

static void rot_k_set_dir(uint32_t angle)
{
	REG_AWR(REG_ROTATION_PATH_CFG, (~ROT_MODE_MASK));
	REG_OWR(REG_ROTATION_PATH_CFG, ((angle & 0x3) << 2));
}

static void rot_k_enable(void)
{
	REG_OWR(REG_ROTATION_PATH_CFG, ROT_EB_BIT);
}

static void rot_k_disable(void)
{
	REG_AWR(REG_ROTATION_PATH_CFG, (~ROT_EB_BIT));
}

static void rot_k_start(void)
{
	REG_OWR(REG_ROTATION_PATH_CFG, ROT_START_BIT);
}


int rot_k_isr(struct camera_frame *dcam_frm, void *u_data)
{
	unsigned long flag;
	rot_isr_func user_isr_func;
	struct rot_drv_private *private = (struct rot_drv_private *)u_data;

	(void)dcam_frm;

	if (!private)
		goto isr_exit;


	spin_lock_irqsave(&private->rot_drv_lock, flag);
	user_isr_func = private->user_isr_func;
	if (user_isr_func)
		(*user_isr_func)(private->rot_fd);

	spin_unlock_irqrestore(&private->rot_drv_lock, flag);

isr_exit:
	return 0;
}

int rot_k_isr_reg(rot_isr_func user_func, struct rot_drv_private *drv_private)
{
	int rtn = 0;
	unsigned long flag;

	if (user_func) {
		if (!drv_private) {
			rtn = -EFAULT;
			goto reg_exit;
		}

		spin_lock_irqsave(&drv_private->rot_drv_lock, flag);
		drv_private->user_isr_func = user_func;
		spin_unlock_irqrestore(&drv_private->rot_drv_lock, flag);

		dcam_reg_isr(DCAM_ROT_DONE, rot_k_isr, drv_private);
	} else {
		dcam_reg_isr(DCAM_ROT_DONE, NULL, NULL);
	}

reg_exit:
	return rtn;
}

int rot_k_is_end(struct rot_param_tag *s)
{
	return s->is_end;
}

static uint32_t rot_k_get_end_mode(struct rot_param_tag *s)
{
	uint32_t ret = 1;

	switch (s->format) {
	case ROT_YUV422:
	case ROT_YUV420:
		ret = 0;
		break;
	case ROT_RGB565:
		ret = 1;
		break;
	default:
		ret = 1;
		break;
	}
	return ret;
}

static uint32_t rot_k_get_pixel_format(struct rot_param_tag *s)
{
	uint32_t ret = ROT_ONE_BYTE;

	switch (s->format) {
	case ROT_YUV422:
	case ROT_YUV420:
		ret = ROT_ONE_BYTE;
		break;
	case ROT_RGB565:
		ret = ROT_TWO_BYTES;
		break;
	default:
		ret = ROT_ONE_BYTE;
		break;
	}

	return ret;
}

static int rot_k_set_y_param(struct rot_cfg_tag *param_ptr,
	struct rot_param_tag *s)
{
	int ret = 0;

	if (!param_ptr || !s) {
		pr_err("rot y param err, param %p, s %p\n",
			param_ptr, s);
		ret = -EFAULT;
		goto param_exit;
	}

	/* add iommu fd */
	memcpy((void *)&s->rot_cfg_parm, (void *)param_ptr,
		sizeof(struct rot_cfg_tag));
	s->rot_iommu_src.dev = &s_rot_pdev->dev;
	s->rot_iommu_dst.dev = &s_rot_pdev->dev;
	memcpy(s->rot_iommu_src.mfd, param_ptr->src_addr.mfd,
		3 * sizeof(unsigned int));
	memcpy(s->rot_iommu_dst.mfd, param_ptr->dst_addr.mfd,
		3 * sizeof(unsigned int));
	pr_err("src_fd=0x%x,rot_dst_fd=0x%x.\n",
		s->rot_iommu_src.mfd[0],
		s->rot_iommu_dst.mfd[0]);

	ret = pfiommu_get_sg_table(&s->rot_iommu_src);
	if (ret) {
		pr_err("rot src sg table failed, fd 0x%x\n",
			s->rot_iommu_src.mfd[0]);
		ret = DCAM_RTN_PATH_ADDR_ERR;
		goto param_exit;
	}
	s->rot_iommu_src.offset[0] = 0;
	s->rot_iommu_src.offset[1] = 0;
	ret = pfiommu_get_addr(&s->rot_iommu_src);
	if (ret) {
		pr_err("rot src get addr fail, 0x%x\n",
			s->rot_iommu_src.mfd[0]);
		ret = DCAM_RTN_PATH_ADDR_ERR;
		goto param_exit;
	}

	ret = pfiommu_get_sg_table(&s->rot_iommu_dst);
	if (ret) {
		pr_err("rot dst sg table failed, fd 0x%x\n",
			s->rot_iommu_dst.mfd[0]);
		ret = DCAM_RTN_PATH_ADDR_ERR;
		goto param_exit;
	}
	s->rot_iommu_dst.offset[0] = 0;
	s->rot_iommu_dst.offset[1] = 0;
	ret = pfiommu_get_addr(&s->rot_iommu_dst);
	if (ret) {
		pr_err("rot dst get addr fail, fd 0x%x\n",
			s->rot_iommu_dst.mfd[0]);
		ret = DCAM_RTN_PATH_ADDR_ERR;
		goto param_exit;
	}
	if (s->rot_iommu_src.iova[0] == 0x0 ||
			s->rot_iommu_dst.iova[0] == 0x0) {
		pr_err("error addr y 0x%lx uv 0x%lx.\n",
					s->rot_iommu_src.iova[0],
					s->rot_iommu_dst.iova[0]);
		ret = DCAM_RTN_PATH_NO_MEM;
		goto param_exit;
	}
	s->s_addr = s->rot_iommu_src.iova[0] + param_ptr->src_addr.y_addr;
	s->d_addr = s->rot_iommu_dst.iova[0] + param_ptr->dst_addr.y_addr;
	s->format = param_ptr->format;
	s->img_size.w = param_ptr->img_size.w;
	s->img_size.h = param_ptr->img_size.h;
	s->angle = param_ptr->angle;
	s->pixel_format = rot_k_get_pixel_format(s);
	s->is_end = rot_k_get_end_mode(s);
	s->uv_mode = ROT_NORMAL;
	s->src_endian = 1;/*param_ptr->src_addr;*/
	s->dst_endian = 1;/*param_ptr->dst_endian;*/

param_exit:
	return ret;
}

int rot_k_set_UV_param(struct rot_cfg_tag *param_ptr, struct rot_param_tag *s)
{
	struct rot_cfg_tag *p = param_ptr;

	s->s_addr = s->rot_iommu_src.iova[0] + p->src_addr.u_addr;
	s->d_addr = s->rot_iommu_dst.iova[0] + p->dst_addr.u_addr;
	s->img_size.w = p->img_size.w / 2;
	s->img_size.h = p->img_size.h;
	s->pixel_format = ROT_TWO_BYTES;
	if (s->format == ROT_YUV422) {
		s->uv_mode = ROT_UV422;
	} else if (s->format == ROT_YUV420) {
		s->uv_mode = ROT_NORMAL;
		s->img_size.h = p->img_size.h / 2;
	}
	return 0;
}

void rot_k_register_cfg(struct rot_param_tag *s)
{
	rot_k_ahb_reset();
	dcam_irq_set(ROTATION_IRQ_LINE_MASK);
	rot_k_set_src_addr(s->s_addr);
	rot_k_set_dst_addr(s->d_addr);
	rot_k_set_img_size(&(s->img_size));
	rot_k_set_pixel_mode(s->pixel_format);
	rot_k_set_UV_mode(s->uv_mode);
	rot_k_set_dir(s->angle);
	rot_k_set_endian(s->src_endian, s->dst_endian);
	rot_k_enable();
	rot_k_start();
	ROTATE_TRACE("rot_k_register_cfg.\n");
}

void rot_k_close(void)
{
	dcam_irq_clear(ROTATION_IRQ_LINE_MASK);
	rot_k_disable();
}

static int rot_k_check_param(struct rot_cfg_tag *param_ptr)
{
	if (param_ptr == NULL) {
		pr_err("Rotation: the param ptr is null\n");
		return -1;
	}

	if ((param_ptr->src_addr.y_addr & ALGIN_FOUR)
		|| (param_ptr->src_addr.u_addr & ALGIN_FOUR)
		|| (param_ptr->src_addr.v_addr & ALGIN_FOUR)
		|| (param_ptr->dst_addr.y_addr & ALGIN_FOUR)
		|| (param_ptr->dst_addr.u_addr & ALGIN_FOUR)
		|| (param_ptr->dst_addr.v_addr & ALGIN_FOUR)) {
		pr_err("Rotation: the addr not algin\n");
		return -1;
	}

	if (!(param_ptr->format == ROT_YUV422
		|| param_ptr->format == ROT_YUV420
		|| param_ptr->format == ROT_RGB565)) {
		pr_err("Rotation: data for err : %d\n", param_ptr->format);
		return -1;
	}

	if (param_ptr->angle > ROT_MIRROR) {
		pr_err("Rotation: data angle err : %d\n", param_ptr->angle);
		return -1;
	}
#if 0
	if (param_ptr->src_endian >= ROT_ENDIAN_MAX ||
		param_ptr->dst_endian >= ROT_ENDIAN_MAX) {
		ROTATE_TRACE("Rotation: endian err : %d %d.\n",
			param_ptr->src_endian,
			param_ptr->dst_endian);
		return -1;
	}
#endif
	return 0;
}

int rot_k_io_cfg(struct rot_cfg_tag *param_ptr, struct rot_param_tag *s)
{
	int ret = 0;
	struct rot_cfg_tag *p = param_ptr;

	ROTATE_TRACE("rot_k_io_cfg start\n");
	ROTATE_TRACE("w=%d, h=%d\n", p->img_size.w, p->img_size.h);
	ROTATE_TRACE("format=%d, angle=%d\n", p->format, p->angle);
	ROTATE_TRACE("s.y=%x, s.u=%x, s.v=%x\n", p->src_addr.y_addr,
		p->src_addr.u_addr, p->src_addr.v_addr);
	ROTATE_TRACE("d.y=%x, d.u=%x, d.v=%x\n", p->dst_addr.y_addr,
		p->dst_addr.u_addr, p->dst_addr.v_addr);

	ret = rot_k_check_param(param_ptr);

	if (ret == 0)
		ret = rot_k_set_y_param(param_ptr, s);

	return ret;
}

int sprd_rot_drv_init(struct platform_device *pdev)
{

	s_rot_pdev = pdev;
	cam_ahb_gpr = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						      "sprd,cam-ahb-syscon");

	return PTR_ERR_OR_ZERO(cam_ahb_gpr);
}

