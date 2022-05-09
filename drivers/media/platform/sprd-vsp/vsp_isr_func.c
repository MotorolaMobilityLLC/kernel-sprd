// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc UMS512 VSP driver
 *
 * Copyright (C) 2019 Unisoc, Inc.
 */

#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/sprd_iommu.h>
#include <linux/uaccess.h>
#include "vsp_common.h"

int vsp_handle_interrupt(struct vsp_dev_t *vsp_hw_dev, int *status,
			void __iomem *sprd_vsp_base,
			void __iomem *vsp_glb_reg_base)
{
	int i;
	int int_status;
	int mmu_status;

	int_status = readl_relaxed(vsp_glb_reg_base + VSP_INT_RAW_OFF);

	if (vsp_hw_dev->version >= SHARKL3) {
		mmu_status = readl_relaxed(sprd_vsp_base + VSP_MMU_INT_RAW_OFF);
		*status |= int_status | (mmu_status << 16);
	} else {
		*status = int_status;
		mmu_status = (int_status >> 10) & 0xff;
		int_status = *status & 0x3ff;
	}

	if (((int_status & 0x1fff) == 0) &&
		((mmu_status & 0xff) == 0)) {
		pr_info("%s vsp IRQ_NONE int_status 0x%x 0x%x",
			__func__, int_status, mmu_status);
		return IRQ_NONE;
	}

	if (int_status & BSM_BUF_OVF_ERR)
		pr_err("vsp_bsm_overflow");

	if (int_status & VLD_ERR)
		pr_err("vsp_vld_err");

	if (int_status & TIMEOUT_ERR)
		pr_err("vsp_timeout");

	if (mmu_status & MMU_RD_WR_ERR) {
		pr_err("vsp iommu addr: 0x%x\n",
		       readl_relaxed(sprd_vsp_base + VSP_MMU_INT_RAW_OFF));

		for (i = 0x18; i <= 0x2c; i += 4)
			pr_info("addr 0x%x is 0x%x\n", i,
				readl_relaxed(sprd_vsp_base + i));

		for (i = 0x4c; i <= 0x58; i += 4)
			pr_info("addr 0x%x is 0x%x\n", i,
				readl_relaxed(sprd_vsp_base + i));
		WARN_ON_ONCE(1);
	}

	/* clear VSP accelerator interrupt bit */
	vsp_clr_interrupt_mask(vsp_hw_dev, sprd_vsp_base, vsp_glb_reg_base);

	return IRQ_HANDLED;
}

void vsp_clr_interrupt_mask(struct vsp_dev_t *vsp_hw_dev,
	void __iomem *sprd_vsp_base, void __iomem *vsp_glb_reg_base)
{
	int cmd = 0;
	int vsp_int_mask = 0;
	int mmu_int_mask = 0;

	if (vsp_hw_dev->version < SHARKL3) {
		/* PIKE2, SHARKLE or the chip before them */
		/*set the interrupt mask 0 */
		cmd = readl_relaxed(sprd_vsp_base + ARM_INT_MASK_OFF);
		cmd &= ~0x4;
		writel_relaxed(cmd, sprd_vsp_base + ARM_INT_MASK_OFF);
		writel_relaxed(BIT(2), sprd_vsp_base + ARM_INT_CLR_OFF);

		/* clear vsp int */
		vsp_int_mask = 0x3ffff;
	} else {
		/* Sharkl3, Sharkl5, Roc1 */
		vsp_int_mask = 0x1fff;
		mmu_int_mask = 0xff;
	}

	/* set the interrupt mask 0 */
	writel_relaxed(0, vsp_glb_reg_base + VSP_INT_MASK_OFF);
	if (vsp_hw_dev->version >= SHARKL3)
		writel_relaxed(0, sprd_vsp_base + VSP_MMU_INT_MASK_OFF);

	/* clear vsp int */
	writel_relaxed(vsp_int_mask, vsp_glb_reg_base + VSP_INT_CLR_OFF);
	if (vsp_hw_dev->version >= SHARKL3)
		writel_relaxed(mmu_int_mask,
			       sprd_vsp_base + VSP_MMU_INT_CLR_OFF);
}
