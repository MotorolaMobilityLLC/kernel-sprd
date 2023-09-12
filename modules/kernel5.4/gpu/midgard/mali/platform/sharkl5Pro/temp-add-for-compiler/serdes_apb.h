/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * This file is dual-licensed: you can use it either under the terms
 * of the GPL or the X11 license, at your option. Note that this dual
 * licensing only applies to this file, and not this project as a
 * whole.
 *
 * updated at 2017-10-12 09:49:25
 *
 */


#ifndef SERDES_APB_H
#define SERDES_APB_H



#define REG_SERDES_APB_FUNC_EN          (0x0000)
#define REG_SERDES_APB_CH_EN            (0x0004)
#define REG_SERDES_APB_FUNNEL_EN        (0x0008)
#define REG_SERDES_APB_FUNNEL_OVERFLOW  (0x000C)
#define REG_SERDES_APB_FSM_CUT_OFF_LEN  (0x0010)

/* REG_SERDES_APB_FUNC_EN */

#define BIT_SERDES_APB_FUNC_EN_FUNC_EN                  BIT(0)

/* REG_SERDES_APB_CH_EN */

#define BIT_SERDES_APB_CH_EN_CH_EN(x)                   (((x) & 0xFFFF))

/* REG_SERDES_APB_FUNNEL_EN */

#define BIT_SERDES_APB_FUNNEL_EN_FUNNEL_EN              BIT(0)

/* REG_SERDES_APB_FUNNEL_OVERFLOW */

#define BIT_SERDES_APB_FUNNEL_OVERFLOW_FUNNEL_OVERFLOW  BIT(0)

/* REG_SERDES_APB_FSM_CUT_OFF_LEN */

#define BIT_SERDES_APB_FSM_CUT_OFF_LEN_FSM_CUT_OFF(x)   (((x) & 0xFFFF))


#endif /* SERDES_APB_H */

