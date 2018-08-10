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

/* begin: address map on gnss side, operate by SDIO BUS */
/* set(s)/clear(c) */
#define GNSS_SET_OFFSET                 0x1000
#define GNSS_CLEAR_OFFSET               0x2000

#define GNSS_APB_BASE              0x40bc8000
#define REG_GNSS_APB_MCU_AP_RST        (GNSS_APB_BASE + 0x0280) /* s/c */
#define BIT_GNSS_APB_MCU_AP_RST_SOFT    (1<<0)    /* bit0 */

#define GNSS_CALI_ADDRESS 0x40aaff4c
#define GNSS_CALI_DONE_FLAG 0x1314520
#define GNSS_CALI_DATA_SIZE 0x14
/* end: address map on gnss side */
int gnss_backup_cali(void);

#endif
