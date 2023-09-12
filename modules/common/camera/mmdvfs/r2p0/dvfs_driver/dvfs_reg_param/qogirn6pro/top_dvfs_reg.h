/*
 * Copyright (C) 2021-2022 UNISOC Communications Inc.
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

#ifndef __SHARKL5PRO_TOP_DVFS_REG_H____
#define __SHARKL5PRO_TOP_DVFS_REG_H____


/* Some defs, in case these are not defined elsewhere */
#ifndef SCI_IOMAP
#define SCI_IOMAP(_b_) ((unsigned long)(_b_))
#endif

#ifndef SCI_ADDR
#define SCI_ADDR(_b_, _o_) ((unsigned long)((_b_) + (_o_)))
#endif

#define REGS_MM_DVFS_AHB_START         SCI_IOMAP(0x30014000)
#define REGS_MM_DVFS_AHB_END           SCI_IOMAP(0x30014A70)

#define REGS_MM_TOP_DVFS_START         SCI_IOMAP(0x64940000)
#define REGS_MM_TOP_DVFS_END           SCI_IOMAP(0x64940ff0)

#define REGS_MM_AHB_REG_START          SCI_IOMAP(0x30000000)
#define REGS_MM_AHB_REG_END            SCI_IOMAP(0x300000f0)

//#define REGS_MM_POWER_REG_START        SCI_IOMAP(0x327e0024) //not find
//#define REGS_MM_POWER_REG_END          SCI_IOMAP(0x327e0028) //not find

//#define REGS_MM_POWER_ON_REG_START     SCI_IOMAP(0x327d0000) //not find
//#define REGS_MM_POWER_ON_REG_END       SCI_IOMAP(0x327d0004) //not find

#ifndef REGS_MM_DVFS_AHB_BASE
#define REGS_MM_DVFS_AHB_BASE     g_mmreg_map.mmdvfs_ahb_regbase
#endif

#ifndef REGS_MM_TOP_DVFS_BASE
#define REGS_MM_TOP_DVFS_BASE     g_mmreg_map.mmdvfs_top_regbase
#endif

#ifndef REGS_MM_AHB_BASE
#define REGS_MM_AHB_BASE          g_mmreg_map.mm_ahb_regbase
#endif

#ifndef REGS_MM_POWER_BASE
#define REGS_MM_POWER_BASE        g_mmreg_map.mm_power_regbase
#endif

#ifndef REGS_MM_POWER_ON_BASE
#define REGS_MM_POWER_ON_BASE     g_mmreg_map.mm_power_on_regbase
#endif

/* power*/
/*#define REG_TOP_PMU_AUTO_SHUTDOWN_EN  SCI_ADDR(0X327e0024, 0x0000UL)*/

/* registers definitions for controller REGS_MM_AHB, 0x30000000 */
#define REG_MM_AHB_AHB_EB              0x0000UL
#define BIT_DVFS_EN_EB                (BIT(3))
#define BIT_MM_CKG_EB                 (BIT(1))
#define BIT_MM_JPG_EB                 (BIT(0))

#define REG_MM_ISP_BLK_EB          0x0004UL
#define BIT_MM_FD_EB                  (BIT(3))
#define BIT_MM_DEPTH_EB               (BIT(2))
#define BIT_MM_CPP_EB                 (BIT(1))
#define BIT_MM_ISP_EB                 (BIT(0))

//#define REG_MM_DCAM_BLK_EB         0x000CUL
//#define BIT_MM_DCAM_IF_LITE_EB        (BIT(2))//需要修改
//#define BIT_MM_DCAM_IF_EB             (BIT(2))//需要修改

#define SHIFT_BIT_MM_CKG_EB    (1)
#define SHIFT_BIT_MM_JPG_EB    (0)

#define SHIFT_BIT_MM_FD_EB     (3)
#define SHIFT_BIT_MM_DEPTH_EB  (2)
#define SHIFT_BIT_MM_CPP_EB    (1)
#define SHIFT_BIT_MM_ISP_EB    (0)

//#define SHIFT_BIT_MM_DCAM_IF_LITE_EB (1)//需要修改
//#define SHIFT_BIT_MM_DCAM_IF_EB      (0)//需要修改


#define REG_MM_IP_BUSY0               0x0020UL
#define BIT_MM_DEPTH_BUSY         (BIT(6))
#define BIT_MM_FD_BUSY            (BIT(5))
#define BIT_MM_DCAM_IF_LITE_BUSY  (BIT(4))
#define BIT_MM_DCAM_IF_BUSY       (BIT(3))
#define BIT_MM_ISP_BUSY           (BIT(2))
#define BIT_MM_JPG_BUSY           (BIT(1))
#define BIT_MM_CPP_BUSY           (BIT(0))

#define SHIFT_BIT_MM_DEP_BUSY           (6)
#define SHIFT_BIT_MM_FD_BUSY            (5)
#define SHIFT_BIT_MM_DCAM_IF_LITE_BUSY  (4)
#define SHIFT_BIT_MM_DCAM_IF_BUSY       (3)
#define SHIFT_BIT_MM_ISP_BUSY           (2)
#define SHIFT_BIT_MM_JPG_BUSY           (1)
#define SHIFT_BIT_MM_CPP_BUSY           (0)

#define REG_TOP_DVFS_APB_DCDC_MM_FIX_VOLTAGE_CTRL   0x0354UL
#define REG_TOP_DVFS_APB_SUBSYS_SW_DVFS_EN_CFG      0x0D84UL
#define REG_TOP_DVFS_APB_DCDC_MM_SW_DVFS_CTRL       0x039CUL

/* debug */
#define REG_TOP_DVFS_APB_DCDC_MM_DVFS_STATE_DBG0         0x03F8UL
#define REG_TOP_DVFS_APB_DCDC_MM_DVFS_STATE_DBG1         0x03FCUL
#define REG_TOP_DVFS_APB_DCDC_MM_DVFS_STATE_DBG2         0x0400UL

/*REG_TOP_DVFS_APB_DCDC_MM_FIX_VOLTAGE_CTRL, 0x64940354 */
#define BIT_DCDC_MM_FIX_VOLTAGE_EN                      (BIT(4))
/*REG_TOP_DVFS_APB_SUBSYS_SW_DVFS_EN_CFG, 0x64940D84 */
#define BIT_MM_SYS_SW_DVFS_EN                           (BIT(7))
/*REG_TOP_DVFS_APB_DCDC_MM_SW_DVFS_CTRL, 0x6494039C */
#define BIT_DCDC_MM_SW_TUNE_EN                          (BIT(0))

#endif /* __SHARKL6PRO_TOP_DVFS_REG_H____ */
