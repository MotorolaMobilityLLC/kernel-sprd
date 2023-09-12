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

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>

#include "cpp_reg.h"
#include "cpp_block.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "ROT_DRV: %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

int cpp_k_rot_dev_enable(void *arg)
{
	struct rot_drv_private *p = NULL;
	unsigned long flags = 0;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct rot_drv_private *) arg;

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_OWR(CPP_PATH_EB, CPP_ROT_EB_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
	return 0;
}

int cpp_k_rot_dev_disable(void *arg)
{
	struct rot_drv_private *p = NULL;
	unsigned long flags = 0;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct rot_drv_private *) arg;

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_AWR(CPP_PATH_EB, ~CPP_ROT_EB_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
	return 0;
}

int cpp_k_rot_dev_start(void *arg)
{
	struct rot_drv_private *p = NULL;
	unsigned long flags = 0;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct rot_drv_private *) arg;

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_OWR(CPP_PATH_START, CPP_ROT_START_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
	return 0;
}

int cpp_k_rot_dev_stop(void *arg)
{
	struct rot_drv_private *p = NULL;
	unsigned long flags = 0;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct rot_drv_private *) arg;

	spin_lock_irqsave(p->hw_lock, flags);
	CPP_REG_AWR(CPP_PATH_START, ~CPP_ROT_START_BIT);
	spin_unlock_irqrestore(p->hw_lock, flags);
	return 0;
}

int cpp_k_rot_parm_set(void *arg)
{
	struct rot_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid input ptr\n");
		return -EFAULT;
	}
	p = (struct rot_drv_private *) arg;

	CPP_REG_WR(CPP_ROTATION_SRC_ADDR, p->rot_src_addr);
	CPP_REG_WR(CPP_ROTATION_DES_ADDR, p->rot_dst_addr);

	CPP_TRACE("rot:src addr:0x%x, dst addr:0x%x\n",
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
	return 0;

}

int cpp_k_rot_reg_trace(void *arg)
{
#ifdef ROT_DRV_DEBUG
	unsigned long addr = 0;
	struct rot_drv_private *p = NULL;

	if (!arg) {
		pr_err("fail to get valid arg\n");
		return -EFAULT;
	}
	p= (struct rot_drv_private *) arg;

	CPP_TRACE("CPP:Rotation Register list");
	for (addr = CPP_ROTATION_SRC_ADDR; addr <= CPP_ROTATION_PATH_CFG;
		addr += 16) {
		CPP_TRACE("0x%lx: 0x%8x 0x%8x 0x%8x 0x%8x\n", addr,
			CPP_REG_RD(addr), CPP_REG_RD(addr + 4),
			CPP_REG_RD(addr + 8), CPP_REG_RD(addr + 12));
	}
#endif
	return 0;
}
