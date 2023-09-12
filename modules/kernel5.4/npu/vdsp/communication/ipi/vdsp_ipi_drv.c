/*
* SPDX-FileCopyrightText: 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd
* SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
*
* Copyright 2019-2022 Unisoc (Shanghai) Technologies Co., Ltd.
* Licensed under the Unisoc General Software License, version 1.0 (the License);
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
* Software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* See the Unisoc General Software License, version 1.0 for more details.
*/

#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/delay.h>
#include "vdsp_ipi_drv.h"
#include "vdsp_hw.h"
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: ipi %d: %d %s:" \
	fmt, current->pid, __LINE__, __func__

static irqreturn_t irq_handler(int irq, void *arg);
static int vdsp_ipi_reg_irq_handle(int idx, irq_handler_t handler, void *arg);
static int vdsp_ipi_unreg_irq_handle(int idx);
static int vdsp_ipi_send_irq(int idx);
static int vdsp_ipi_ctx_init(struct vdsp_ipi_ctx_desc *ctx);
static int vdsp_ipi_ctx_deinit(struct vdsp_ipi_ctx_desc *ctx);

struct vdsp_ipi_ops ipi_ops = {
	.ctx_init = vdsp_ipi_ctx_init,
	.ctx_deinit = vdsp_ipi_ctx_deinit,
	.irq_handler = irq_handler,
	.irq_register = vdsp_ipi_reg_irq_handle,
	.irq_unregister = vdsp_ipi_unreg_irq_handle,
	.irq_send = vdsp_ipi_send_irq,
};

static struct vdsp_ipi_ctx_desc s_ipi_desc = {
	.ops = &ipi_ops,
};

static irq_handler_t ipi_isr_handler[IPI_IDX_MAX] = { };

static void *ipi_isr_param[IPI_IDX_MAX] = {
	[IPI_IDX_0] = NULL,
	[IPI_IDX_1] = NULL,
	[IPI_IDX_2] = NULL,
	[IPI_IDX_3] = NULL,
};

static int vdsp_ipi_reg_irq_handle(int idx, irq_handler_t handler, void *param)
{
	pr_debug("vdsp register handler[%d] = 0x%p, param = 0x%p\n", idx, handler, param);
	ipi_isr_handler[idx] = handler;
	ipi_isr_param[idx] = param;

	return 0;
}

static int vdsp_ipi_unreg_irq_handle(int idx)
{
	ipi_isr_handler[idx] = NULL;
	ipi_isr_param[idx] = NULL;

	return 0;
}

static int vdsp_ipi_send_irq(int idx)
{
	pr_debug("device_irq:%x, ipi_addr 0x%#lx\n", idx, (unsigned long)s_ipi_desc.ipi_addr);

	switch (s_ipi_desc.irq_mode) {
	case XRP_IRQ_EDGE_SW:
		break;
	case XRP_IRQ_EDGE:
		/* fallthrough */
	case XRP_IRQ_LEVEL:
		wmb();
		if (s_ipi_desc.ipi_addr) {
			IPI_HREG_OWR(s_ipi_desc.ipi_addr, 0x1 << idx);
		}

		break;
	default:
		break;
	}

	return 0;
}

static irqreturn_t irq_handler(int irq, void *arg)
{
	irqreturn_t ret = IRQ_NONE;
	int irq_val = 0;
	int irq_mask = 0;
	int i = 0;
	unsigned long flags;
	struct vdsp_hw *hw = (struct vdsp_hw *)arg;
	struct vdsp_ipi_ctx_desc *ctx = hw->vdsp_ipi_desc;

	spin_lock_irqsave(&ctx->ipi_spinlock, flags);
	/*if irq active is 0 ,the ipi may be disabled ,so return here */
	if (ctx->ipi_active == 0) {
		spin_unlock_irqrestore(&ctx->ipi_spinlock, flags);
		pr_warn("irq_handler ipi_active is 0 return\n");
		return IRQ_HANDLED;
	}
	irq_val = IPI_HREG_RD(s_ipi_desc.ipi_addr) & 0xF;
	irq_mask = irq_val;

	/* clear the interrupt */
	IPI_HREG_OWR((s_ipi_desc.ipi_addr + 8), irq_val & 0xF);
	spin_unlock_irqrestore(&ctx->ipi_spinlock, flags);
	/* dispatch the interrupt */
	for (i = 0; i < IPI_IDX_MAX; i++) {
		if (irq_mask & (1 << i)) {
			if (ipi_isr_handler[i])
				ret = ipi_isr_handler[i](irq, ipi_isr_param[i]);
		}
		irq_val &= ~(1 << i);
		if (!irq_val)
			break;
	}

	return ret;
}

static int vdsp_ipi_ctx_init(struct vdsp_ipi_ctx_desc *ctx)
{
	unsigned long flags;

	vdsp_regmap_update_bits(ctx->base_addr, 0x0, (1 << 6), (1 << 6), RT_NO_SET_CLR);
	udelay(1);
	vdsp_regmap_update_bits(ctx->base_addr, REG_VDSP_INT_CTL, 0x1ffff, 0xd85f, RT_NO_SET_CLR);
	IPI_HREG_OWR((ctx->ipi_addr + 8), 0xFF);
	spin_lock_init(&ctx->ipi_spinlock);
	spin_lock_irqsave(&ctx->ipi_spinlock, flags);
	ctx->ipi_active = 1;
	spin_unlock_irqrestore(&ctx->ipi_spinlock, flags);
	return 0;
}

static int vdsp_ipi_ctx_deinit(struct vdsp_ipi_ctx_desc *ctx)
{
	unsigned long flags;

	spin_lock_irqsave(&ctx->ipi_spinlock, flags);
	ctx->ipi_active = 0;
	IPI_HREG_OWR((ctx->ipi_addr + 8), 0xFF);

	vdsp_regmap_update_bits(ctx->base_addr, 0x0, (1 << 6), 0, RT_NO_SET_CLR);
	vdsp_regmap_update_bits(ctx->base_addr, REG_VDSP_INT_CTL, 0x1ffff, 0x1ffff, RT_NO_SET_CLR);
	spin_unlock_irqrestore(&ctx->ipi_spinlock, flags);
	return 0;
}

struct vdsp_ipi_ctx_desc *get_vdsp_ipi_ctx_desc(void)
{
	return &s_ipi_desc;
}

EXPORT_SYMBOL_GPL(get_vdsp_ipi_ctx_desc);
