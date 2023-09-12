/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This file is dual-licensed: you can use it either under the terms
 * of the GPL or the X11 license, at your option. Note that this dual
 * licensing only applies to this file, and not this project as a
 * whole.
 *
 */

#ifndef _SERDES_APB_REG_H
#define _SERDES_APB_REG_H

#define REG_SERDES_APB_FUNC_EN             0x0000
#define REG_SERDES_APB_CH_EN               0x0004
#define REG_SERDES_APB_FUNNEL_EN	   0x0008
#define REG_SERDES_APB_FSM_CUT_OFF_LEN     0x0010
#define REG_SERDES_APB_CH0_MAX_LEN         0x0020
#define REG_SERDES_APB_CH1_MAX_LEN         0x0024
#define REG_SERDES_APB_CH2_MAX_LEN         0x0028
#define REG_SERDES_APB_CH3_MAX_LEN         0x002C
#define REG_SERDES_APB_CH4_MAX_LEN         0x0030
#define REG_SERDES_APB_CH5_MAX_LEN         0x0034
#define REG_SERDES_APB_CH6_MAX_LEN         0x0038
#define REG_SERDES_APB_CH7_MAX_LEN         0x003C
#define REG_SERDES_APB_CH8_MAX_LEN         0x0040
#define REG_SERDES_APB_CH9_MAX_LEN         0x0044
#define REG_SERDES_APB_CH10_MAX_LEN        0x0048
#define REG_SERDES_APB_CH11_MAX_LEN        0x004C
#define REG_SERDES_APB_CH12_MAX_LEN        0x0050
#define REG_SERDES_APB_CH13_MAX_LEN        0x0054
#define REG_SERDES_APB_CH14_MAX_LEN        0x0058
#define REG_SERDES_APB_CH15_MAX_LEN        0x005C
#define REG_SERDES_APB_CH3_LA_SAMPLE_RATE  0x008C

/* REG_SERDES_APB_FUNC_EN */

#define BIT_SERDES_APB_FUNC_EN_FUNC_EN                BIT(0)

/* REG_SERDES_APB_CH_EN */

#define BIT_SERDES_APB_CH_EN_CH_EN(x)                 (((x) & 0xFFFF))

/* REG_SERDES_APB_FUNNEL_EN */

#define BIT_SERDES_APB_FUNNEL_EN                      BIT(0)

/* REG_SERDES_APB_FSM_CUT_OFF_LEN */

#define BIT_SERDES_APB_FSM_CUT_OFF_LEN_FSM_CUT_OFF(x) (((x) & 0xFFFF))

/* REG_SERDES_APB_CH0_MAX_LEN */

#define BIT_SERDES_APB_CH0_MAX_LEN_CH0_MAX_LEN(x)    (((x) & 0xFFFF))

/* REG_SERDES_APB_CH1_MAX_LEN */

#define BIT_SERDES_APB_CH1_MAX_LEN_CH1_MAX_LEN(x)   (((x) & 0xFFFF))

/* REG_SERDES_APB_CH2_MAX_LEN */

#define BIT_SERDES_APB_CH2_MAX_LEN_CH2_MAX_LEN(x)   (((x) & 0xFFFF))

/* REG_SERDES_APB_CH3_MAX_LEN */

#define BIT_SERDES_APB_CH3_MAX_LEN_CH3_MAX_LEN(x)  (((x) & 0xFFFF))

/* REG_SERDES_APB_CH4_MAX_LEN */

#define BIT_SERDES_APB_CH4_MAX_LEN_CH4_MAX_LEN(x)  (((x) & 0xFFFF))

/* REG_SERDES_APB_CH5_MAX_LEN */

#define BIT_SERDES_APB_CH5_MAX_LEN_CH5_MAX_LEN(x) (((x) & 0xFFFF))

/* REG_SERDES_APB_CH6_MAX_LEN */

#define BIT_SERDES_APB_CH6_MAX_LEN_CH6_MAX_LEN(x) (((x) & 0xFFFF))

/* REG_SERDES_APB_CH7_MAX_LEN */

#define BIT_SERDES_APB_CH7_MAX_LEN_CH7_MAX_LEN(x) (((x) & 0xFFFF))

/* REG_SERDES_APB_CH8_MAX_LEN */

#define BIT_SERDES_APB_CH8_MAX_LEN_CH8_MAX_LEN(x) (((x) & 0xFFFF))

/* REG_SERDES_APB_CH9_MAX_LEN */

#define BIT_SERDES_APB_CH9_MAX_LEN_CH9_MAX_LEN(x) (((x) & 0xFFFF))

/* REG_SERDES_APB_CH10_MAX_LEN */

#define BIT_SERDES_APB_CH10_MAX_LEN_CH10_MAX_LEN(x) (((x) & 0xFFFF))

/* REG_SERDES_APB_CH11_MAX_LEN */

#define BIT_SERDES_APB_CH11_MAX_LEN_CH11_MAX_LEN(x) (((x) & 0xFFFF))

/* REG_SERDES_APB_CH12_MAX_LEN */

#define BIT_SERDES_APB_CH12_MAX_LEN_CH12_MAX_LEN(x) (((x) & 0xFFFF))

/* REG_SERDES_APB_CH13_MAX_LEN */

#define BIT_SERDES_APB_CH13_MAX_LEN_CH13_MAX_LEN(x) (((x) & 0xFFFF))

/* REG_SERDES_APB_CH14_MAX_LEN */

#define BIT_SERDES_APB_CH14_MAX_LEN_CH14_MAX_LEN(x) (((x) & 0xFFFF))

/* REG_SERDES_APB_CH15_MAX_LEN */

#define BIT_SERDES_APB_CH15_MAX_LEN_CH15_MAX_LEN(x) (((x) & 0xFFFF))

/* REG_SERDES_APB_CH3_LA_SAMPLE_RATE */

#define BIT_SERDES_APB_CH3_LA_SAMPLE_RATE_CH3_LA_SAMPLE_RATE(x) (((x) & 0xF))

#endif
