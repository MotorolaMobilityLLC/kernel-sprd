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

#ifndef _SENSOR_OTP_H_
#define _SENSOR_OTP_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/fs.h>
#include <video/sprd_sensor_k.h>
#include <linux/vmalloc.h>
#include "../sprd_sensor_drv.h"
#include "cam_pw_domain.h"

#define END_TABLE     0xFFFFFFFF
#define SENSOR_SUCCESS      0
#define SENSOR_FAIL             1
#define cmr_bzero(b, len)   memset((b), '\0', (len))
#define NUMBER_OF_ARRAY(a)                            ARRAY_SIZE(a)

struct isp_data_t {
	uint32_t size;
	void *data_ptr;
};

struct sensor_otp_ioctl_func_tab {
	uint32_t (*power_on)(uint32_t power_on);
	uint32_t (*set_i2c)(void);
	uint32_t (*identify)(void);
	uint32_t (*sensor_reloadinfo)(struct _sensor_otp_param_tag *arg);
};

struct sensor_otp_tab {
	uint32_t sensor_id;
	struct sensor_otp_ioctl_func_tab *sensor_otp_func;
};

int sensor_reloadinfo_thread(void *data);
extern struct sensor_otp_ioctl_func_tab imx230_otp_func_tab;
extern struct sensor_otp_ioctl_func_tab s5k3p3sm_otp_func_tab;
extern struct sensor_otp_ioctl_func_tab s5k3p8sm_otp_func_tab;
extern struct sensor_otp_ioctl_func_tab s5k3l8xxm3_otp_func_tab;

#endif
