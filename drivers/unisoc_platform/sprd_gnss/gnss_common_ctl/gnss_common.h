/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * Filename : gnss_common.h
 * Abstract : This file is a implementation for driver of gnss:
 *
 * Authors  : zhaohui.chen
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __GNSS_COMMON_H__
#define __GNSS_COMMON_H__

#define TRUE    (1)
#define FALSE    (0)

/* gnss defined locally begin*/
/* Note: should keep with wcn bsp defined orginally */

/* include/misc/wcn_integrate_platform.h */
enum {
	WCN_PLATFORM_TYPE_SHARKLE,
	WCN_PLATFORM_TYPE_PIKE2,
	WCN_PLATFORM_TYPE_SHARKL3,
	WCN_PLATFORM_TYPE_QOGIRL6,
	WCN_PLATFORM_TYPE,
};
enum wcn_gnss_sub_sys {
	WCN_GNSS = 16,
	WCN_GNSS_BD,
	WCN_GNSS_GAL,
	WCN_GNSS_ALL,
};
/* gnss defined locally end*/

/* wcn bsp API begin */
int wcn_write_data_to_phy_addr(phys_addr_t phy_addr, void *src_data, u32 size);
int wcn_read_data_from_phy_addr(phys_addr_t phy_addr, void *tar_data, u32 size);

u32 wcn_platform_chip_type(void);

phys_addr_t wcn_get_gnss_base_addr(void);
/* wcn bsp API end */

#define GNSS_STATUS_OFFSET                 (0x0014F004)
#define GNSS_STATUS_SIZE                   (4)
#define GNSS_QOGIRL6_STATUS_OFFSET        (0x001ffc34) /* Qogirl6 */

#ifndef CONFIG_WCN_INTEG
/* begin: address map on gnss side, operate by SDIO BUS */
/* set(s)/clear(c) */

#define GNSS_INDIRECT_OP_REG		0x40b20000

#define GNSS_CALI_DONE_FLAG 0x1314520

/*  GNSS assert workaround */
#define GNSS_BOOTSTATUS_SIZE     0x4
#define GNSS_BOOTSTATUS_MAGIC    0x12345678
/* end: address map on gnss side */
#endif

/* begin: PMIC configuration for marlin3lite */
/* sharkl5 sharkl6 */
#define SC2730_PIN_REG_BASE     0x0480
#define PTEST0			0x0
#define PTEST0_MASK		(BIT(4) | BIT(5))
#define PTEST0_sel(x)		(((x)&0x3)<<4)

#define REGS_ANA_APB_BASE	0x1800
#define XTL_WAIT_CTRL0		0x378
#define BIT_XTL_EN		BIT(8)

#define TSEN_CTRL0		0x334
#define BIT_TSEN_CLK_SRC_SEL	BIT(4)
#define BIT_TSEN_ADCLDO_EN	BIT(15)

#define TSEN_CTRL1		 0x338
#define BIT_TSEN_CLK_EN		BIT(7)
#define BIT_TSEN_SDADC_EN	BIT(11)
#define BIT_TSEN_UGBUF_EN	BIT(14)

#define TSEN_CTRL2		0x33c
#define TSEN_CTRL3		0x340
#define BIT_TSEN_EN		BIT(0)
#define BIT_TSEN_SEL_EN		BIT(3)
#define BIT_TSEN_TIME_SEL_MASK  (BIT(4) | BIT(5))
#define BIT_TSEN_TIME_sel(x)    (((x)&0x3)<<4)

#define TSEN_CTRL4		0x344
#define TSEN_CTRL5		0x348
#define CLK32KLESS_CTRL0	0x368
#define M26_TSX_32KLESS		0x8010

enum{
	TSEN_EXT,
	TSEN_INT,
};
/* end: PMIC configuration for marlin3lite */

/* begin: Qogirl6 tcxo */
#define XTLBUF3_REL_CFG_ADDR    0x640209e0
#define XTLBUF3_REL_CFG_OFFSET  0x9e0
#define WCN_XTL_CTRL_ADDR       0x64020fe4
#define WCN_XTL_CTRL_OFFSET     0xfe4
/* end: Qogirl6 tcxo */

bool gnss_delay_ctl(void);

#ifdef GNSS_SINGLE_MODULE
int gnss_pnt_ctl_init(void);
int gnss_gdb_init(void);
void gnss_pnt_ctl_cleanup(void);
void gnss_gdb_exit(void);
#endif

#endif
