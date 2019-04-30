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
#ifndef _SC2731S_FLASH_REG_H_
#define _SC2731S_FLASH_REG_H_

#define SW_SAFE_TIME            0x0240
#define SW_LED_EN               0x0248
#define FLASH0_MODE_TIME        0x024c
#define FLASH1_MODE_TIME        0x0250
#define RG_FLASH_IBTRIM			0x0254
#define RG_BST_RESERVED			0x0264

#define RG_BST_CFG1			0x025c
#define RG_BST_V      (BIT(7)|BIT(8)|BIT(9)|BIT(10))
#define RG_BST_V_CAL      (BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6))

#define FLASH_IRQ_INT           0x0010
#define FLASH_IRQ_EN            0x0014
#define FLASH_IRQ_CLR           0x0018
#define FLASH_IRQ_BIT_MASK      (BIT(5)|BIT(4))

#define FLASH_VAL_ON            1
#define FLASH_VAL_OFF           0

#define FLASH0_CTRL_EN          BIT(0)
#define FLASH1_CTRL_EN          BIT(1)
#define TORCH0_CTRL_EN          BIT(2)
#define TORCH1_CTRL_EN          BIT(3)

/* 1.2s */
#define FLASH0_SAFE_TIME        1
#define FLASH0_CTRL_SAFE_TIME   (BIT(0)|BIT(1))
#define FLASH1_CTRL_SAFE_TIME   (BIT(2)|BIT(3))

/* min:16, step:48 (mA) */
#define FLASH0_CTRL_PRE_TIME    (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4))
/* min: 16, step:48 (mA) */
#define FLASH0_CTRL_REAL_TIME   (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9))
/* min: 16, step:48 (mA) */
#define FLASH0_CTRL_TORCH_TIME  (BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14))

/* min:30, step:30 (mA) */
#define FLASH1_CTRL_PRE_TIME    (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4))
/* min: 30, step:30 (mA) */
#define FLASH1_CTRL_REAL_TIME   (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9))
/* min: 10, step:10 (mA) */
#define FLASH1_CTRL_TORCH_TIME  (BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14))

#endif
