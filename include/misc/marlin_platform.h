/*
 * Copyright (C) 2016 Spreadtrum Communications Inc.
 *
 * Filename : marlin.h
 * Abstract : This file is a implementation for driver of marlin2
 *
 * Authors	: yufeng.yang
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
#ifndef __MARLIN_H__
#define __MARLIN_H__

#include <linux/types.h>

#define FALSE								(0)
#define TRUE								(1)

typedef int (*marlin_reset_callback) (void *para);
extern marlin_reset_callback marlin_reset_func;
extern void *marlin_callback_para;
enum marlin_sub_sys {
	MARLIN_BLUETOOTH = 0,
	MARLIN_FM,
	MARLIN_WIFI,
	MARLIN_WIFI_FLUSH,
	MARLIN_SDIO_TX,
	MARLIN_SDIO_RX,
	MARLIN_MDBG,
	MARLIN_GNSS,
	WCN_AUTO,	/* fist GPS, then btwififm */
	MARLIN_ALL,
};

void marlin_power_off(enum marlin_sub_sys subsys);
int marlin_get_power(enum marlin_sub_sys subsys);
int marlin_set_wakeup(enum marlin_sub_sys subsys);
int marlin_set_sleep(enum marlin_sub_sys subsys, bool enable);
int marlin_reset_reg(void);
int start_marlin(u32 subsys);
int stop_marlin(u32 subsys);
int open_power_ctl(void);
bool marlin_get_download_status(void);
void marlin_chip_en(bool enable, bool reset);
int marlin_get_module_status(void);
int marlin_get_module_status_changed(void);
int wcn_get_module_status_changed(void);
void wcn_set_module_status_changed(bool status);
int marlin_reset_register_notify(void *callback_func, void *para);
int marlin_reset_unregister_notify(void);
int is_first_power_on(enum marlin_sub_sys subsys);
const char *strno(int subsys);
#endif
