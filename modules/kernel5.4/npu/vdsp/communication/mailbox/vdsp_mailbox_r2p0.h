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

#ifndef VDSP_MAILBOX_R2P0_H
#define VDSP_MAILBOX_R2P0_H

/* reg offset define */
#define MBOX_ID			0x00
#define MBOX_MSG_L		0x04
#define MBOX_MSG_H		0x08
#define MBOX_TRI		0x0c
#define MBOX_FIFO_RST		0x10
#define MBOX_IRQ_STS		0x18
#define MBOX_IRQ_MSK		0x1c
#define MBOX_LOCK			0x20
#define MBOX_FIFO_DEPTH		0x24

#define MBOX_FIFO_INBOX_STS_1		0x14
#define MBOX_FIFO_OUTBOX_STS_1		0x14
#define MBOX_FIFO_INBOX_STS_2		0x24
#define MBOX_FIFO_OUTBOX_STS_2		0x28

/*common config offset*/
#define MBOX_PRIOR_LOW			0x00
#define MBOX_PRIOR_HIGH			0x04
#define MBOX_VERSION			0x08

/*outbox 0x10 fifo ctrl 1
write 1 to outbox+0x10 reset fifo, rd/wr prt and flush fifo */
#define IRQ_MODE_BIT			BIT(16)
#define FIFO_RESET_CLR_IRQ_BIT	BIT(1)
#define FIFO_RESET_BIT			BIT(0)

/*outbox 0x14 fifo sts 1*/
#define FIFO_RD_PTR_BIT		GENMASK(31, 24)
#define FIFO_WR_PTR_BIT		GENMASK(23, 16)
#define FIFO_RECEIVE_FLAG	BIT(4)
#define FIFO_FULL_FLAG		BIT(2)
#define FIFO_EMPTY_FLAG		BIT(1)
#define FIFO_NOT_EMPTY_FLAG	BIT(0)

/*outbox 0x18 irq out*/
#define FIFO_ADDR_OVERFLOW_BIT	BIT(31)
#define CALC_BASE_ADDR_BIT		BIT(30)
#define OUTBOX_CLR_IRQ_BIT		BIT(16)
#define FIFO_CLR_WR_IRQ_BIT		GENMASK(15, 0)

/*outbox + 0x1C mask
	set 1: not receive irq
	set 0: can receive irq
*/
#define FIFO_IRQ_WR_BITMASK		GENMASK(31, 16)
#define FIFO_RECEIVE_BITMASK		BIT(4)
#define FIFO_FULL_BITMASK			BIT(2)
#define FIFO_EMPTY_BITMASK			BIT(1)
#define FIFO_NOT_EMPTY_BITMASK		BIT(0)

/*outbox + 0x20 user lock*/
#define USER_LOCK_BIT		BIT(0)

/*outbox + 0x24 fifo depth */
#define FIFO_DEPTH_BIT		GENMASK(7, 0)

/*outbox + 0x28 fifo sts 2*/
#define FIFO_WR_FLAG		GENMASK(15, 0)

/*inbox + 0x10 fifo sts clr in*/
#define IN_OUTBOX_SEND_CLR_BIT		GENMASK(31, 16)
#define IN_OUTBOX_OVERFLOW_CLR_BIT	GENMASK(15, 0)
#define IN_OUTBOX_SEND_CLR_SHIFT		16
#define IN_OUTBOX_OVERFLOW_CLR_SHIFT	0

/*inbox + 0x14 fifo sts 1*/
#define IN_OUTBOX_RECEIVING_FLAG_BIT	GENMASK(31, 16)
#define IN_OUTBOX_SEND_FLAG_BIT	GENMASK(15, 0)
#define IN_OUTBOX_RECEIVING_FLAG_SHIFT	16
#define IN_OUTBOX_SEND_FLAG_SHIFT		0

/*inbox + 0x18 IRQ*/
#define IN_FIFO_ADDR_OVERFLOW_BIT	BIT(31)
#define IN_CALC_BASE_ADDR_BIT		BIT(30)
#define IN_INBOX_CLR_IRQ			BIT(0)

/*inbox + 0x1C irq mask
	set 1: not receive irq
	set 0: can receive irq
*/
#define IN_SEND_BITMASK		BIT(2)
#define IN_OVERFLOW_BITMASK	BIT(1)
#define IN_BLOCK_BITMASK	BIT(0)

/*inbox 0x24 fifo sts 2*/
#define IN_OUTBOX_OVERFLOW_FLAG		GENMASK(31, 16)
#define IN_OUTBOX_BLOCK_FLAG		GENMASK(15, 0)
#define IN_OUTBOX_OVERFLOW_FLAG_SHIFT	16
#define IN_OUTBOX_BLOCK_FLAG_SHIFT		0

#define MBOX_UNLOCK_KEY 0x5a5a5a5a

/*mm ahb global set*/
#define MM_AHB_MBOX_EB		BIT(2)

/*mailbox trigger*/
#define TRIGGER			BIT(0)

/*mailbox inbox base is 0x30020000 in dts*/
#define MBOX_REGBASE			0x30020000

#define MBOX_RANGE				0x4000
#define MBOX_VDSPAP_CORE_OFFSET	0x0000
#define MBOX_VDSPAP_INBOX_OFFSET	(MBOX_VDSPAP_CORE_OFFSET)
#define MBOX_VDSPAP_OUTBOX_OFFSET	(MBOX_VDSPAP_CORE_OFFSET + MBOX_RANGE)
#define MBOX_VDSPAP_COMMON_OFFSET	(MBOX_VDSPAP_CORE_OFFSET + MBOX_RANGE * 2)
#define MBOX_VDSP_INBOX_BASE	(MBOX_REGBASE + MBOX_VDSPAP_INBOX_OFFSET)
#define MBOX_VDSP_OUTBOX_BASE	(MBOX_REGBASE + MBOX_VDSPAP_OUTBOX_OFFSET)
#define MBOX_VDSP_COMMON_BASE	(MBOX_REGBASE + MBOX_VDSPAP_COMMON_OFFSET)

/*set in dts*/
#define MBOX_MAX_CORE_CNT	0x3
#define MBOX_MAX_CORE_MASK	GENMASK(7,0)
#define MAX_SMSG_BAK		64

#define MBOX_V2_INBOX_FIFO_SIZE		0x1
#define MBOX_V2_OUTBOX_FIFO_SIZE	0x80	/*max 0x9F */

#define MBOX_V2_READ_PT_SHIFT	24
#define MBOX_V2_WRITE_PT_SHIFT	16

/*mailbox range*/
#define MBOX_V2_INBOX_CORE_SIZE		0x1000
#define MBOX_V2_OUTBOX_CORE_SIZE	0x1000

#define MBOX_V2_INBOX_IRQ_MASK	(IN_SEND_BITMASK)
#define MBOX_V2_OUTBOX_IRQ_MASK	(~((u32)FIFO_NOT_EMPTY_BITMASK))

#define MBOX_GET_FIFO_RD_PTR(val) ((val & FIFO_RD_PTR_BIT) >> (MBOX_V2_READ_PT_SHIFT))
#define MBOX_GET_FIFO_WR_PTR(val) ((val & FIFO_WR_PTR_BIT) >> (MBOX_V2_WRITE_PT_SHIFT))

struct mbox_device_tag {
	u32 version;
	u32 max_cnt;
	const struct mbox_operations_tag *fops;
};

struct mbox_cfg_tag {
	u32 inbox_irq;
	u32 inbox_base;
	u32 inbox_range;
	u32 inbox_fifo_size;
	u32 inbox_irq_mask;

	u32 outbox_irq;
	u32 outbox_sensor_irq;
	u32 sensor_core;

	u32 outbox_base;
	u32 outbox_range;
	u32 outbox_fifo_size;
	u32 outbox_irq_mask;

	u32 rd_bit;
	u32 rd_mask;
	u32 wr_bit;
	u32 wr_mask;

	u32 enable_reg;
	u32 mask_bit;

	u32 core_cnt;
	u32 version;

	u32 prior_low;
	u32 prior_high;
};

struct mbox_fifo_data_tag {
	u8 core_id;
	u64 msg;
};

void mbox_get_phy_device(struct mbox_device_tag **mbox_dev);

#endif /* #ifndef VDSP_MAILBOX_R2P0_H */
