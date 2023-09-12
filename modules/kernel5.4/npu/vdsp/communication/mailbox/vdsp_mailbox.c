/*
* SPDX-FileCopyrightText: 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd
* SPDX-License-Identifier: LicenseRef-Unisoc-General-1.0
*
* Copyright 2021-2022 Unisoc (Shanghai) Technologies Co., Ltd.
* Licensed under the Unisoc General Software License, version 1.0 (the License);
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* https://www.unisoc.com/en_us/license/UNISOC_GENERAL_LICENSE_V1.0-EN_US
* Software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OF ANY KIND, either express or implied.
* See the Unisoc General Software License, version 1.0 for more details.
*/

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>
#include "vdsp_mailbox_drv.h"
#include "vdsp_mailbox_r2p0.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vdsp: mbox %d %d %s : "\
	fmt, current->pid, __LINE__, __func__

static struct mbox_dts_cfg_tag mbox_dts;
static struct mbox_device_tag *mbox_ops;
static u8 g_mbox_inited;

static spinlock_t mbox_lock;

static int vdsp_mbox_register_irq_handle(u8 target_id, mbox_handle handler,
	void *data)
{
	unsigned long flags;
	int ret;

	pr_debug("target_id =%d\n", target_id);
	if (!g_mbox_inited) {
		pr_err("[error] mbox not inited\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&mbox_lock, flags);
	ret = mbox_ops->fops->phy_register_irq_handle(target_id, handler, data);
	spin_unlock_irqrestore(&mbox_lock, flags);
	if (ret) {
		pr_err("[error]mbox register irq fail, ret:%d\n", ret);
		return ret;
	}

	/* must do it, Ap may be have already revieved some msgs */
	mbox_ops->fops->process_bak_msg();
	return ret;
}

static int vdsp_mbox_unregister_irq_handle(u8 target_id)
{
	int ret;

	if (!g_mbox_inited) {
		pr_err("[error] mbox not inited\n");
		return -EINVAL;
	}
	spin_lock(&mbox_lock);
	ret = mbox_ops->fops->phy_unregister_irq_handle(target_id);
	spin_unlock(&mbox_lock);

	return ret;
}

static irqreturn_t recv_irq_handler(int irq, void *dev)
{
	return mbox_ops->fops->recv_irqhandle(irq, dev);
}

static int vdsp_mbox_send_irq(u8 core_id, uint64_t msg)
{
	int ret;
	unsigned long flag;

	spin_lock_irqsave(&mbox_lock, flag);
	ret = mbox_ops->fops->phy_send(core_id, msg);
	spin_unlock_irqrestore(&mbox_lock, flag);
	return ret;
}

static int mbox_parse_dts(void)
{
	int ret;
	struct device_node *np;

	/* parse mbox dts: inbox base/outbox base/core cnt/version */
	np = of_find_compatible_node(NULL, NULL, "sprd,vdsp-mailbox");
	if (!np) {
		pr_err("[error] dts:can't find compatible node!\n");
		return -EINVAL;
	}
	of_address_to_resource(np, 0, &mbox_dts.inboxres);
	of_address_to_resource(np, 1, &mbox_dts.outboxres);
	ret = of_property_read_u32(np, "sprd,vdsp-core-cnt", &mbox_dts.core_cnt);
	if (ret) {
		pr_err("[error]dts: fail get core_cnt\n");
		return -EINVAL;
	}
	ret = of_property_read_u32(np, "sprd,vdsp-version", &mbox_dts.version);
	if (ret) {
		pr_err("[error]dts: fail get version\n");
		return -EINVAL;
	}

	return 0;
}

static int vdsp_mbox_init(void)
{
	int ret;

	ret = mbox_parse_dts();
	if (ret != 0) {
		return -EINVAL;
	}
	mbox_get_phy_device(&mbox_ops);
	spin_lock_init(&mbox_lock);
	ret = mbox_ops->fops->cfg_init(&mbox_dts, &g_mbox_inited);
	if (ret != 0) {
		return -EINVAL;
	}
	return 0;
}

static int vdsp_mbox_enable(struct vdsp_mbox_ctx_desc *ctx)
{
	return mbox_ops->fops->enable(ctx);
}

static int vdsp_mbox_disable(struct vdsp_mbox_ctx_desc *ctx)
{
	return mbox_ops->fops->disable(ctx);
}

struct vdsp_mbox_ops vdsp_mbox_ops = {
	.ctx_init = vdsp_mbox_enable,
	.ctx_deinit = vdsp_mbox_disable,
	.irq_handler = recv_irq_handler,
	.irq_register = vdsp_mbox_register_irq_handle,
	.irq_unregister = vdsp_mbox_unregister_irq_handle,
	.irq_send = vdsp_mbox_send_irq,
	.mbox_init = vdsp_mbox_init,
};

static struct vdsp_mbox_ctx_desc s_mbox_desc = {
	.ops = &vdsp_mbox_ops,
};

struct vdsp_mbox_ctx_desc *get_vdsp_mbox_ctx_desc(void)
{
	return &s_mbox_desc;
}


