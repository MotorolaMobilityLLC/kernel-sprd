/* SPDX-License-Identifier: GPL-2.0
 *
 * UNISOC APCPU POWER DEBUG driver
 *
 * Copyright (C) 2020 Unisoc, Inc.
 */

#ifndef __SPRD_DEBUGLOG_H__
#define __SPRD_DEBUGLOG_H__

#include <linux/types.h>

#define WAKEUP_NAME_LEN		(64)
#define INVALID_SUB_NUM		(-1)

enum wakeup_type {
	WAKEUP_GPIO = 1,
	WAKEUP_RTC,
	WAKEUP_MODEM,
	WAKEUP_SENSORHUB,
	WAKEUP_WIFI,
	WAKEUP_EIC_DBNC,
	WAKEUP_EIC_LATCH,
	WAKEUP_EIC_ASYNC,
	WAKEUP_EIC_SYNC,
	WAKEUP_PMIC_EIC,
};

struct wakeup_info {
	char name[WAKEUP_NAME_LEN];
	int type;
	int gpio;
	int source;
	int reason;
	int protocol;
	int version;
	int port;
};

int wakeup_info_register(int gic_num, int sub_num,
				int (*get)(void *info, void *data), void *data);
int wakeup_info_unregister(int gic_num, int sub_num);

#endif /* __SPRD_DEBUGLOG_H__ */
