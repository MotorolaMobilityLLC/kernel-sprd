/*
* Copyright (C) 2019 Unisoc Communications Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/

#ifndef _SPRD_SENSOR_DRV_H_
#define _SPRD_SENSOR_DRV_H_

#include <linux/delay.h>
#include <linux/of_device.h>
#include "sprd_sensor_core.h"

#define SPRD_SENSOR_MINOR		MISC_DYNAMIC_MINOR
#define SPRD_SENSOR_DEVICE_NAME		"sprd_sensor"
#define SPRD_SENSOR_MAIN_DRIVER_NAME	"sensor_main"
#define SPRD_SENSOR_SUB_DRIVER_NAME	"sensor_sub"
#define SPRD_SENSOR_MAIN2_DRIVER_NAME	"sensor_main2"
#define SPRD_SENSOR_SUB2_DRIVER_NAME	"sensor_sub2"
#define SPRD_SENSOR_MAIN3_DRIVER_NAME	"sensor_main3"
#define SPRD_SENSOR_SUB3_DRIVER_NAME	"sensor_sub3"

#define SPRD_SENSOR_MCLK_VALUE		1000000

#define SPRD_SENSOR_I2C_OP_TRY_NUM		3
#define SPRD_SENSOR_CMD_BITS_8			1
#define SPRD_SENSOR_CMD_BITS_16			2
#define SPRD_SENSOR_I2C_VAL_8BIT		0x00
#define SPRD_SENSOR_I2C_VAL_16BIT		0x01
#define SPRD_SENSOR_I2C_REG_8BIT		(0x00 << 1)
#define SPRD_SENSOR_I2C_REG_16BIT		(0x01 << 1)
#define SPRD_SENSOR_I2C_CUSTOM			(0x01 << 2)
#define SPRD_SENSOR_LOW_EIGHT_BIT		0xff
#define SPRD_SENSOR_WRITE_DELAY			0xffff
#define SPRD_SENSOR_I2C_WRITE_SUCCESS_CNT	1
#define SPRD_SENSOR_I2C_READ_SUCCESS_CNT	2
#define SPRD_SENSOR_I2C_BUST_NB			7

#define SLEEP_MS msleep
#define SPRD_SENSOR_MAX_MCLK 96

struct sensor_mclk_tag {
	uint32_t clock;
	char *src_name;
};

struct sprd_sensor_dev_info_tag {
	struct mutex sync_lock;
	struct mutex set_voltage_lock;
	atomic_t users;
	struct clk *sensor_clk;
	struct clk *sensor_clk_default;
	unsigned long sensor_clk_default_rate;
	struct clk *ccir_eb;
	struct clk *sensor_eb;
	struct regulator *regulator_supply[SENSOR_REGULATOR_ID_MAX];
	struct i2c_client *i2c_info;
	struct i2c_client *vcm_i2c_client;
	enum SPRD_SENSOR_GPIO_TAG_E gpio_tab[SPRD_SENSOR_GPIO_TAG_MAX];
	/* unsigned int vcm_gpio_i2c_flag;//TODO need to check */
	unsigned int power_on_count[SENSOR_REGULATOR_ID_MAX];
	int sensor_id;
	int attch_dcam_id;
	int mclk_freq;
	struct device_node *dev_node;
};

void sprd_sensor_sync_lock(int sensor_id);
void sprd_sensor_sync_unlock(int sensor_id);
int sprd_sensor_register_driver(void);
int sprd_sensor_set_voltage_by_gpio(int sensor_id, unsigned int val, int type);
int sprd_sensor_set_voltage(int sensor_id, unsigned int val, int type);
int sprd_sensor_set_mclk(unsigned int *saved_clk, unsigned int set_mclk,
			 int sensor_id);
int sprd_sensor_set_pd_level(int sensor_id, int power_level);
int sprd_sensor_reset(int sensor_id, unsigned int level, unsigned int width);
int sprd_sensor_set_rst_level(int sensor_id, uint32_t rst_level);
int sprd_sensor_set_mipi_level(int sensor_id, uint32_t plus_level);
int sprd_sensor_set_i2c_addr(int sensor_id, uint16_t i2c_addr);
int sprd_sensor_set_i2c_clk(int sensor_id, uint32_t clock);
int sprd_sensor_read_reg(int sensor_id, struct sensor_reg_bits_tag *pReg);
int sprd_sensor_write_reg(int sensor_id, struct sensor_reg_bits_tag *pReg);
int sprd_sensor_burst_write_init(struct sensor_reg_tag *p_reg_table,
				 int sensor_id, uint32_t init_table_size,
				 uint32_t reg_bits);
int sprd_sensor_write_regtab(struct sensor_reg_tab_tag *p_reg_table,
			     int sensor_id);
int sprd_sensor_k_write_regtab(struct sensor_reg_tab_tag *p_reg_table,
			       int sensor_id);
int sprd_sensor_write_i2c(struct sensor_i2c_tag *i2c_tab, int sensor_id);
int sprd_sensor_read_i2c(struct sensor_i2c_tag *i2c_tab, int sensor_id);
int sprd_sensor_write_muti_i2c(struct sensor_muti_aec_i2c_tag *muti_aec_i2c);
void sprd_sensor_unregister_driver(void);
int sprd_sensor_find_dcam_id(int sensor_id);

#endif
