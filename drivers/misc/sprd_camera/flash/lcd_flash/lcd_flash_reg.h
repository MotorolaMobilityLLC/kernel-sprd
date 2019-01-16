/*
 * Copyright (C) 2015-2016 Spreadtrum Communications Inc.
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
#ifndef _RGB_FLASH_REG_H_
#define _RGB_FLASH_REG_H_
#include <linux/mfd/sprd/pmic_glb_reg.h>

#define FLASH_CTRL_REG            ANA_REG_GLB_FLASH_CTRL
#define FLASH_PON  BIT(15)
#define FLASH_V_HW_EN  BIT(6)
#define FLASH_V_HW_STEP (BIT(5)|BIT(4))
#define FLASH_V_SW (BIT(3)|BIT(2)|BIT(1)|BIT(0))


#define FLASH_V_HW_STEP_CYCLE0 0
#define FLASH_V_HW_STEP_CYCLE1 (BIT(4))
#define FLASH_V_HW_STEP_CYCLE2 (BIT(5))
#define FLASH_V_HW_STEP_CYCLE3 (BIT(5)|BIT(4))

#define FLASH_V_SW_SETP0 0
#define FLASH_V_SW_SETP1 (BIT(0))
#define FLASH_V_SW_SETP2 (BIT(1))
#define FLASH_V_SW_SETP3 (BIT(0)|BIT(1))
#define FLASH_V_SW_SETP4 (BIT(2))
#define FLASH_V_SW_SETP5 (BIT(2)|BIT(0))
#define FLASH_V_SW_SETP6 (BIT(2)|BIT(1))
#define FLASH_V_SW_SETP7 (BIT(2)|BIT(1)|BIT(0))
#define FLASH_V_SW_SETP8 (BIT(3))
#define FLASH_V_SW_SETP9 (BIT(3)|BIT(0))
#define FLASH_V_SW_SETP10 (BIT(3)|BIT(1))
#define FLASH_V_SW_SETP11 (BIT(3)|BIT(1)|BIT(0))
#define FLASH_V_SW_SETP12 (BIT(3)|BIT(2))
#define FLASH_V_SW_SETP13 (BIT(3)|BIT(2)|BIT(0))
#define FLASH_V_SW_SETP14 (BIT(3)|BIT(2)|BIT(1))
#define FLASH_V_SW_SETP15 (BIT(3)|BIT(2)|BIT(1)|BIT(0))

#endif
