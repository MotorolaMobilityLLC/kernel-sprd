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

#ifndef VDSP_MAILBOX_DRV_H
#define VDSP_MAILBOX_DRV_H

#include <linux/interrupt.h>
typedef irqreturn_t (*mbox_handle)(void *ptr, void *private);

#define SEND_FIFO_LEN 64
#define MBOX_INVALID_CORE	0xff

enum xrp_irq_mode {
	XRP_IRQ_NONE,
	XRP_IRQ_LEVEL,
	XRP_IRQ_EDGE,
	XRP_IRQ_EDGE_SW,
	XRP_IRQ_MAX,
};

struct mbox_dts_cfg_tag {
	struct regmap *gpr;
	u32 enable_reg;
	u32 mask_bit;
	struct resource inboxres;
	struct resource outboxres;
	struct resource commonres;
	u32 inbox_irq;
	u32 outbox_irq;
	u32 outbox_sensor_irq;
	u32 sensor_core;
	u32 core_cnt;
	u32 version;
};

struct mbox_chn_tag {
	mbox_handle mbox_smsg_handler;
	unsigned long max_irq_proc_time;
	unsigned long max_recv_flag_cnt;
	void *mbox_priv_data;
};

struct mbox_operations_tag {
	int (*cfg_init) (struct mbox_dts_cfg_tag *, u8 *);
	int (*phy_register_irq_handle) (u8, mbox_handle, void *);
	int (*phy_unregister_irq_handle) (u8);
	irqreturn_t(*src_irqhandle) (int, void *);
	irqreturn_t(*recv_irqhandle) (int, void *);
	irqreturn_t(*sensor_recv_irqhandle) (int, void *);
	int (*phy_send) (u8, u64);
	void (*process_bak_msg) (void);
	u32(*phy_core_fifo_full) (int);
	void (*phy_just_sent) (u8, u64);
	bool(*outbox_has_irq) (void);
	int (*enable) (void *ctx);
	int (*disable) (void *ctx);
};

struct vdsp_mbox_ops;
struct vdsp_mbox_ctx_desc {
	struct regmap *mm_ahb;
	struct mbox_dts_cfg_tag mbox_cfg;
	/* how IRQ is used to notify the device of incoming data */
	enum xrp_irq_mode irq_mode;
	struct vdsp_mbox_ops *ops;
	spinlock_t mbox_spinlock;
	struct mutex mbox_lock;
	uint32_t mbox_active;
};

struct vdsp_mbox_ops {
	int (*ctx_init) (struct vdsp_mbox_ctx_desc *ctx);
	int (*ctx_deinit) (struct vdsp_mbox_ctx_desc *ctx);
	irqreturn_t(*irq_handler) (int irq, void *arg);
	int (*irq_register) (u8 idx, mbox_handle handler, void *arg);
	int (*irq_unregister) (u8 idx);
	int (*irq_send) (u8 idx, u64 msg);
	int (*irq_clear) (int idx);
	int (*mbox_init) (void);
};

struct vdsp_mbox_ctx_desc *get_vdsp_mbox_ctx_desc(void);
#endif /* SPRD_MAILBOX_DRV_H */
