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

#ifndef _SPRD_SENSOR_CORE_H_
#define _SPRD_SENSOR_CORE_H_

#include <linux/of_device.h>
#include <video/sprd_sensor_k.h>

#define SPRD_SENSOR_VDD_1000MV_VAL	1000000
#define SPRD_SENSOR_VDD_1200MV_VAL	1200000
#define SPRD_SENSOR_VDD_1300MV_VAL	1300000
#define SPRD_SENSOR_VDD_1500MV_VAL	1500000
#define SPRD_SENSOR_VDD_1800MV_VAL	1800000
#define SPRD_SENSOR_VDD_2000MV_VAL	2000000
#define SPRD_SENSOR_VDD_2500MV_VAL	2500000
#define SPRD_SENSOR_VDD_2800MV_VAL	2800000
#define SPRD_SENSOR_VDD_3000MV_VAL	3000000
#define SPRD_SENSOR_VDD_3300MV_VAL	3300000
#define SPRD_SENSOR_VDD_3800MV_VAL	3800000

enum sprd_sensor_vdd_e {
	SPRD_SENSOR_VDD_3800MV = 0,
	SPRD_SENSOR_VDD_3300MV,
	SPRD_SENSOR_VDD_3000MV,
	SPRD_SENSOR_VDD_2800MV,
	SPRD_SENSOR_VDD_2500MV,
	SPRD_SENSOR_VDD_2000MV,
	SPRD_SENSOR_VDD_1800MV,
	SPRD_SENSOR_VDD_1500MV,
	SPRD_SENSOR_VDD_1300MV,
	SPRD_SENSOR_VDD_1200MV,
	SPRD_SENSOR_VDD_1000MV,
	SPRD_SENSOR_VDD_CLOSED,
	SPRD_SENSOR_VDD_UNUSED
};

enum SPRD_SENSOR_REGULATOR_ID_E {
	SENSOR_REGULATOR_VDDIO_E,
	SENSOR_REGULATOR_CAMAVDD_ID_E,
	SENSOR_REGULATOR_CAMDVDD_ID_E,
	SENSOR_REGULATOR_CAMMOT_ID_E,
	SENSOR_REGULATOR_ID_MAX
};

enum SPRD_SENSOR_ID_E {
	SPRD_SENSOR_MAIN_ID_E,
	SPRD_SENSOR_SUB_ID_E,
	SPRD_SENSOR_MAIN2_ID_E,
	SPRD_SENSOR_SUB2_ID_E,
	SPRD_SENSOR_MAIN3_ID_E,
	SPRD_SENSOR_SUB3_ID_E,
	SPRD_SENSOR_ID_MAX
};

enum SPRD_SENSOR_INTERFACE_TYPE_E {
	SPRD_SENSOR_INTERFACE_CCIR_E,
	SPRD_SENSOR_INTERFACE_MIPI_E
};

enum SPRD_SENSOR_INTERFACE_OP_ID {
	SPRD_SENSOR_INTERFACE_OPEN = 0,
	SPRD_SENSOR_INTERFACE_CLOSE
};

enum SPRD_SENSOR_GPIO_TAG_E {
	SPRD_SENSOR_RESET_GPIO_TAG_E,
	SPRD_SENSOR_PWN_GPIO_TAG_E,
	SPRD_SENSOR_FLASH_GPIO_TAG_E,
	SPRD_SENSOR_MIPI_SWITCH_EN_GPIO_TAG_E,
	SPRD_SENSOR_MIPI_SWITCH_MODE_GPIO_TAG_E,
	SPRD_SENSOR_SWITCH_MODE_GPIO_TAG_E,
	SPRD_SENSOR_CAM_ID_GPIO_TAG_E,
	SPRD_SENSOR_IOVDD_GPIO_TAG_E,
	SPRD_SENSOR_AVDD_GPIO_TAG_E,
	SPRD_SENSOR_DVDD_GPIO_TAG_E,
	SPRD_SENSOR_MOT_GPIO_TAG_E,
	SPRD_SENSOR_GPIO_TAG_MAX
};

enum SPRD_SENSOR_MIPI_STATE_E {
	SPRD_SENSOR_MIPI_STATE_OFF_E,
	SPRD_SENSOR_MIPI_STATE_ON_E
};

struct sprd_sensor_mem_tag {
	void *buf_ptr;
	unsigned int size;
};

struct sprd_sensor_core_module_tag {
	atomic_t total_users;
	unsigned int padding;
	struct mutex sensor_id_lock;
	struct wakeup_source ws;
};

struct sprd_sensor_file_tag {
	unsigned int sensor_mclk;
	unsigned int phy_id;
	unsigned int if_type;
	struct sprd_sensor_core_module_tag *mod_data;
	enum SPRD_SENSOR_ID_E sensor_id;
	enum SPRD_SENSOR_MIPI_STATE_E mipi_state;
};

int sprd_sensor_malloc(struct sprd_sensor_mem_tag *mem_ptr, unsigned int size);
void sprd_sensor_free(struct sprd_sensor_mem_tag *mem_ptr);

#endif
