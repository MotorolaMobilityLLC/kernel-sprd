/*
 *
 * Copyright (C) 2019 Unisoc Communications Inc.
 *
 *
 *
 * This software is licensed under the terms of the GNU General Public
 *
 * License version 2, as published by the Free Software Foundation, and
 *
 * may be copied, distributed, and modified under those terms.
 *
 *
 *
 * This program is distributed in the hope that it will be useful,
 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include "../sprd_sensor_drv.h"
#include "sensor_power.h"

static int sensor_s5k4h5yc_poweron(uint32_t *fd_handle,
					struct sensor_power *dev0,
					struct sensor_power *dev1,
					struct sensor_power *dev2)
{
	int ret = 0;
	unsigned int saved_clk = 1;

	sprd_sensor_set_rst_level(SPRD_SENSOR_SUB_ID_E, 0);
	sprd_sensor_set_rst_level(SPRD_SENSOR_MAIN_ID_E, 0);
	udelay(100);

	sprd_sensor_set_voltage(SPRD_SENSOR_MAIN_ID_E,
					SPRD_SENSOR_VDD_2800MV,
					SENSOR_REGULATOR_CAMMOT_ID_E);
	udelay(10);
	sprd_sensor_set_voltage(SPRD_SENSOR_MAIN_ID_E,
					SPRD_SENSOR_VDD_1200MV,
					SENSOR_REGULATOR_CAMDVDD_ID_E);
	udelay(10);
	sprd_sensor_set_voltage(SPRD_SENSOR_MAIN_ID_E,
					SPRD_SENSOR_VDD_2800MV,
					SENSOR_REGULATOR_CAMAVDD_ID_E);
	udelay(10);
	sprd_sensor_set_voltage(SPRD_SENSOR_MAIN_ID_E,
					SPRD_SENSOR_VDD_1800MV,
					SENSOR_REGULATOR_VDDIO_E);
	mdelay(2);
	sprd_sensor_set_rst_level(SPRD_SENSOR_MAIN_ID_E, 1);
	udelay(750);
	sprd_sensor_set_mclk(&saved_clk, 24, SENSOR_ID);
	mdelay(2);

	pr_info("s5k4h5yc_poweron OK\n");
	return ret;
}

static int sensor_s5k4h5yc_poweroff(uint32_t *fd_handle,
					struct sensor_power *dev0,
					struct sensor_power *dev1,
					struct sensor_power *dev2)
{
	int ret = 0;
	unsigned int saved_clk = 1;

	sprd_sensor_set_rst_level(SPRD_SENSOR_SUB_ID_E, 0);

	sprd_sensor_set_voltage(SPRD_SENSOR_MAIN_ID_E,
						SPRD_SENSOR_VDD_CLOSED,
						SENSOR_REGULATOR_CAMMOT_ID_E);
	udelay(10);
	sprd_sensor_set_mclk(&saved_clk, 0, SENSOR_ID);
	mdelay(6);
	sprd_sensor_set_rst_level(SPRD_SENSOR_MAIN_ID_E, 0);
	mdelay(2);
	sprd_sensor_set_voltage(SPRD_SENSOR_MAIN_ID_E,
						SPRD_SENSOR_VDD_CLOSED,
						SENSOR_REGULATOR_CAMDVDD_ID_E);
	mdelay(2);
	sprd_sensor_set_voltage(SPRD_SENSOR_MAIN_ID_E,
						SPRD_SENSOR_VDD_CLOSED,
						SENSOR_REGULATOR_CAMAVDD_ID_E);
	mdelay(2);
	sprd_sensor_set_voltage(SPRD_SENSOR_MAIN_ID_E,
						SPRD_SENSOR_VDD_CLOSED,
						SENSOR_REGULATOR_VDDIO_E);
	mdelay(6);

	pr_info("s5k4h5yc_poweroff OK\n");
	return ret;

}

static int sensor_s5k5e3yx_poweron(uint32_t *fd_handle,
					struct sensor_power *dev0,
					struct sensor_power *dev1,
					struct sensor_power *dev2)
{

	int ret = 0;

	unsigned int saved_clk = 1;

	sprd_sensor_set_rst_level(SPRD_SENSOR_MAIN_ID_E, 0);
	sprd_sensor_set_rst_level(SPRD_SENSOR_SUB_ID_E, 0);

	sprd_sensor_set_voltage(SPRD_SENSOR_SUB_ID_E,
					SPRD_SENSOR_VDD_1200MV,
					SENSOR_REGULATOR_CAMDVDD_ID_E);
	sprd_sensor_set_voltage(SPRD_SENSOR_SUB_ID_E,
					SPRD_SENSOR_VDD_2800MV,
					SENSOR_REGULATOR_CAMAVDD_ID_E);
	sprd_sensor_set_voltage(SPRD_SENSOR_SUB_ID_E,
					SPRD_SENSOR_VDD_1800MV,
					SENSOR_REGULATOR_VDDIO_E);
	udelay(50);
	sprd_sensor_set_rst_level(SPRD_SENSOR_SUB_ID_E, 1);
	udelay(250);
	sprd_sensor_set_mclk(&saved_clk, 24, SENSOR_ID);
	udelay(200);

	pr_info("s5k5e3yx poweron OK\n");
	return ret;
}

static int sensor_s5k5e3yx_poweroff(uint32_t *fd_handle,
					struct sensor_power *dev0,
					struct sensor_power *dev1,
					struct sensor_power *dev2)
{
	int ret = 0;
	unsigned int saved_clk = 1;

	sprd_sensor_set_mclk(&saved_clk, 0, SENSOR_ID);
	sprd_sensor_set_rst_level(SPRD_SENSOR_SUB_ID_E, 0);
	sprd_sensor_set_voltage(SPRD_SENSOR_SUB_ID_E,
					SPRD_SENSOR_VDD_CLOSED,
					SENSOR_REGULATOR_CAMDVDD_ID_E);
	sprd_sensor_set_voltage(SPRD_SENSOR_SUB_ID_E,
					SPRD_SENSOR_VDD_CLOSED,
					SENSOR_REGULATOR_CAMAVDD_ID_E);
	sprd_sensor_set_voltage(SPRD_SENSOR_SUB_ID_E,
					SPRD_SENSOR_VDD_CLOSED,
					SENSOR_REGULATOR_VDDIO_E);

	pr_info("s5k5e3yx poweroff OK\n");
	return ret;
}

int sensor_power_on(uint32_t *fd_handle, uint32_t sensor_id,
			struct sensor_power *dev0, struct sensor_power *dev1,
			struct sensor_power *dev2)
{

	int ret = 0;

	if (sensor_id == SPRD_SENSOR_MAIN_ID_E)
		ret = sensor_s5k4h5yc_poweron(fd_handle, dev0, dev1, dev2);
	else if (sensor_id == SPRD_SENSOR_SUB_ID_E)
		ret = sensor_s5k5e3yx_poweron(fd_handle, dev0, dev1, dev2);

	return ret;

}

int sensor_power_off(uint32_t *fd_handle, uint32_t sensor_id,
			struct sensor_power *dev0, struct sensor_power *dev1,
			struct sensor_power *dev2)
{

	int ret = 0;

	if (sensor_id == SPRD_SENSOR_MAIN_ID_E)
		ret = sensor_s5k4h5yc_poweroff(fd_handle, dev0, dev1, dev2);
	else if (sensor_id == SPRD_SENSOR_SUB_ID_E)
		ret = sensor_s5k5e3yx_poweroff(fd_handle, dev0, dev1, dev2);

	return ret;

}
