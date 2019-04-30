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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include "../sprd_sensor_drv.h"
#include "sensor_power.h"

static int sensor_hi544_poweron(uint32_t *fd_handle,
					struct sensor_power *dev0,
					struct sensor_power *dev1,
					struct sensor_power *dev2)
{
	int ret = 0;
	unsigned int saved_clk = 1;

	sprd_sensor_set_pd_level(SPRD_SENSOR_SUB_ID_E, 0);
	sprd_sensor_set_rst_level(SPRD_SENSOR_SUB_ID_E, 0);

	sprd_sensor_set_pd_level(SPRD_SENSOR_MAIN_ID_E, 0);
	sprd_sensor_set_rst_level(SPRD_SENSOR_MAIN_ID_E, 0);
	udelay(10);
	sprd_sensor_set_voltage(SPRD_SENSOR_MAIN_ID_E,
					SPRD_SENSOR_VDD_2800MV,
					SENSOR_REGULATOR_CAMMOT_ID_E);
	sprd_sensor_set_voltage(SPRD_SENSOR_MAIN_ID_E,
					SPRD_SENSOR_VDD_1800MV,
					SENSOR_REGULATOR_VDDIO_E);
	udelay(10);
	sprd_sensor_set_voltage(SPRD_SENSOR_MAIN_ID_E,
					SPRD_SENSOR_VDD_2800MV,
					SENSOR_REGULATOR_CAMAVDD_ID_E);
	mdelay(6);
	sprd_sensor_set_pd_level(SPRD_SENSOR_MAIN_ID_E, 1);
	mdelay(2);
	sprd_sensor_set_mclk(&saved_clk, 24, SENSOR_ID);
	mdelay(30);
	sprd_sensor_set_rst_level(SPRD_SENSOR_MAIN_ID_E, 1);
	udelay(2);
	pr_info("hi544_poweron OK\n");

	return ret;
}

static int sensor_hi544_poweroff(uint32_t *fd_handle,
						struct sensor_power *dev0,
						struct sensor_power *dev1,
						struct sensor_power *dev2)
{
	int ret = 0;
	unsigned int saved_clk = 1;

	sprd_sensor_set_rst_level(SPRD_SENSOR_MAIN_ID_E, 0);
	mdelay(10);
	sprd_sensor_set_mclk(&saved_clk, 0, SENSOR_ID);
	udelay(1);
	sprd_sensor_set_pd_level(SPRD_SENSOR_MAIN_ID_E, 0);
	mdelay(6);
	sprd_sensor_set_voltage(SPRD_SENSOR_MAIN_ID_E,
					SPRD_SENSOR_VDD_CLOSED,
					SENSOR_REGULATOR_CAMAVDD_ID_E);
	mdelay(6);
	sprd_sensor_set_voltage(SPRD_SENSOR_MAIN_ID_E,
					SPRD_SENSOR_VDD_CLOSED,
					SENSOR_REGULATOR_VDDIO_E);
	udelay(1);
	sprd_sensor_set_voltage(SPRD_SENSOR_MAIN_ID_E,
					SPRD_SENSOR_VDD_CLOSED,
					SENSOR_REGULATOR_CAMMOT_ID_E);
	pr_info("hi544_poweroff OK\n");

	return ret;
}


static int sensor_hi255_poweron(uint32_t *fd_handle,
						struct sensor_power *dev0,
						struct sensor_power *dev1,
						struct sensor_power *dev2)
{
	int ret = 0;
	unsigned int saved_clk = 1;

	sprd_sensor_set_pd_level(SPRD_SENSOR_MAIN_ID_E, 0);
	sprd_sensor_set_rst_level(SPRD_SENSOR_MAIN_ID_E, 0);

	sprd_sensor_set_pd_level(SPRD_SENSOR_SUB_ID_E, 0);
	sprd_sensor_set_rst_level(SPRD_SENSOR_SUB_ID_E, 0);
	udelay(1);

	sprd_sensor_set_voltage(SPRD_SENSOR_SUB_ID_E,
					SPRD_SENSOR_VDD_1800MV,
					SENSOR_REGULATOR_VDDIO_E);
	udelay(6);
	sprd_sensor_set_voltage(SPRD_SENSOR_SUB_ID_E,
					SPRD_SENSOR_VDD_2800MV,
					SENSOR_REGULATOR_CAMAVDD_ID_E);
	udelay(6);
	sprd_sensor_set_pd_level(SPRD_SENSOR_SUB_ID_E, 1);
	mdelay(2);
	sprd_sensor_set_mclk(&saved_clk, 24, SENSOR_ID);
	mdelay(31);
	sprd_sensor_set_rst_level(SPRD_SENSOR_SUB_ID_E, 1);
	udelay(2);

	pr_info("hi255_poweron OK\n");

	return ret;
}

static int sensor_hi255_poweroff(uint32_t *fd_handle,
						struct sensor_power *dev0,
						struct sensor_power *dev1,
						struct sensor_power *dev2)
{
	int ret = 0;
	unsigned int saved_clk = 1;

	sprd_sensor_set_rst_level(SPRD_SENSOR_SUB_ID_E, 0);
	udelay(2);
	sprd_sensor_set_mclk(&saved_clk, 0, SENSOR_ID);
	udelay(1);
	sprd_sensor_set_pd_level(SPRD_SENSOR_SUB_ID_E, 0);
	mdelay(6);
	sprd_sensor_set_voltage(SPRD_SENSOR_SUB_ID_E,
					SPRD_SENSOR_VDD_CLOSED,
					SENSOR_REGULATOR_CAMAVDD_ID_E);
	sprd_sensor_set_voltage(SPRD_SENSOR_SUB_ID_E,
					SPRD_SENSOR_VDD_CLOSED,
					SENSOR_REGULATOR_VDDIO_E);
	udelay(1);

	pr_info("hi255_poweroff OK\n");
	return ret;
}

int sensor_power_on(uint32_t *fd_handle,
					uint32_t sensor_id,
					struct sensor_power *dev0,
					struct sensor_power *dev1,
					struct sensor_power *dev2)
{
	int ret = 0;

	if (sensor_id == SPRD_SENSOR_MAIN_ID_E)
		ret = sensor_hi544_poweron(fd_handle, dev0, dev1, dev2);
	else if (sensor_id == SPRD_SENSOR_SUB_ID_E)
		ret = sensor_hi255_poweron(fd_handle, dev0, dev1, dev2);
	return ret;
}

int sensor_power_off(uint32_t *fd_handle,
					uint32_t sensor_id,
					struct sensor_power *dev0,
					struct sensor_power *dev1,
					struct sensor_power *dev2)
{
	int ret = 0;

	if (sensor_id == SPRD_SENSOR_MAIN_ID_E)
		ret = sensor_hi544_poweroff(fd_handle, dev0, dev1, dev2);
	else if (sensor_id == SPRD_SENSOR_SUB_ID_E)
		ret = sensor_hi255_poweroff(fd_handle, dev0, dev1, dev2);

	return ret;
}
