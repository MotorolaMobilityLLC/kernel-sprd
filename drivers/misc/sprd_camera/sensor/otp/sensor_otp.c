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
#include "sensor_otp.h"

static const struct sensor_otp_tab sensor_main_otp_table[] = {
	{0x230, &imx230_otp_func_tab},
	{0x3103, &s5k3p3sm_otp_func_tab},
	{0x3108, &s5k3p8sm_otp_func_tab},
	{0x30c8, &s5k3l8xxm3_otp_func_tab},
	{END_TABLE, NULL},
};

int sensor_reloadinfo_thread(void *data)
{
	int ret = SENSOR_FAIL;
	uint32_t i = 0;
	struct sensor_otp_ioctl_func_tab *func_ptr = NULL;
	struct _sensor_otp_param_tag *param_ptr = data;

	if (param_ptr == NULL)
		return -1;

	/*whait for platform driver*/
	msleep(1000);
	sprd_cam_pw_on();
	sprd_cam_domain_eb();

	while (sensor_main_otp_table[i].sensor_id != END_TABLE) {
		func_ptr = sensor_main_otp_table[i].sensor_otp_func;
		i++;
		if (func_ptr == NULL)
			continue;
		if (func_ptr->power_on == NULL
			|| NULL == func_ptr->set_i2c
			|| NULL == func_ptr->identify
			|| NULL == func_ptr->sensor_reloadinfo)
			continue;

		ret = func_ptr->power_on(1);
		ret = func_ptr->set_i2c();
		ret = func_ptr->identify();
		if (ret == SENSOR_SUCCESS) {
			ret = func_ptr->sensor_reloadinfo(param_ptr);
			ret = func_ptr->power_on(0);
			break;
		}
		ret = func_ptr->power_on(0);

	}

	sprd_cam_domain_disable();
	sprd_cam_pw_off();
	return ret;
}
