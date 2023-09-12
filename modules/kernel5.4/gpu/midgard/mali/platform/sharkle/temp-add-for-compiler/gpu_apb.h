/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * This file is dual-licensed: you can use it either under the terms
 * of the GPL or the X11 license, at your option. Note that this dual
 * licensing only applies to this file, and not this project as a
 * whole.
 *
 * updated at 2017-06-12 20:13:17
 *
 */


#ifndef GPU_APB_H
#define GPU_APB_H



#define REG_GPU_APB_APB_RST                            (0x0000)
#define REG_GPU_APB_APB_CLK_CTRL                       (0x0004)
#define REG_GPU_APB_APB_BARRIER_CTRL                   (0x0008)
#define REG_GPU_APB_GPU_NIC400_GPU2PUB_SLV_LP_CFG      (0x000C)
#define REG_GPU_APB_GPU_LPC_NUM_CFG                    (0x0010)
#define REG_GPU_APB_GPU_NIC400_QOS                     (0x0014)
#define REG_GPU_APB_GPU_CGM_FDIV_NUM                   (0x0018)
#define REG_GPU_APB_GPU_CGM_FDIV_DENOM                 (0x001C)

/* REG_GPU_APB_APB_RST */

#define BIT_GPU_APB_GPU_SOFT_RST                 BIT(0)

/* REG_GPU_APB_APB_CLK_CTRL */

#define BIT_GPU_APB_CLK_GPU_DIV(x)               (((x) & 0x7) << 4)
#define BIT_GPU_APB_CLK_GPU_SEL(x)               (((x) & 0x7))

/* REG_GPU_APB_APB_BARRIER_CTRL */

#define BIT_GPU_APB_GPU_BARRIER_DISABLE_EN       BIT(0)

/* REG_GPU_APB_GPU_NIC400_GPU2PUB_SLV_LP_CFG */

#define BIT_GPU_APB_NIC400_GPU2PUB_SLV_LP_EB     BIT(1)
#define BIT_GPU_APB_NIC400_GPU2PUB_SLV_LP_FORCE  BIT(0)

/* REG_GPU_APB_GPU_LPC_NUM_CFG */

#define BIT_GPU_APB_GPU_SYS_LP_NUM(x)            (((x) & 0xFFFF))

/* REG_GPU_APB_GPU_NIC400_QOS */

#define BIT_GPU_APB_GPU_QOS_SEL                  BIT(16)
#define BIT_GPU_APB_AWQOS_THRESHOLD_GPU(x)       (((x) & 0xF) << 12)
#define BIT_GPU_APB_ARQOS_THRESHOLD_GPU(x)       (((x) & 0xF) << 8)
#define BIT_GPU_APB_AWQOS_GPU(x)                 (((x) & 0xF) << 4)
#define BIT_GPU_APB_ARQOS_GPU(x)                 (((x) & 0xF))

/* REG_GPU_APB_GPU_CGM_FDIV_NUM */

#define BIT_GPU_APB_CGM_GPU_FDIV_NUM(x)          (((x) & 0xF))

/* REG_GPU_APB_GPU_CGM_FDIV_DENOM */

#define BIT_GPU_APB_CGM_GPU_FDIV_DENOM(x)        (((x) & 0xF))


#endif /* GPU_APB_H */

