/*
 * Copyright (C) 2015-2016 Spreadtrum Communications Inc.
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
#ifndef _FLASH_DRV_H_
#define _FLASH_DRV_H_

#include <video/sprd_img.h>

#define SPRD_FLASH_NUM_MAX      3
#define SPRD_FLASH_ON           1
#define SPRD_FLASH_OFF          0

enum {
	SPRD_FLASH_REAR = 0,
	SPRD_FLASH_FRONT = 1,
	SPRD_FLASH_MAX,
};

enum {
	SPRD_FLASH_LED0 = 0x01,
	SPRD_FLASH_LED1 = 0x02,
	SPRD_FLASH_LED2 = 0x04,
	SPRD_FLASH_LED_ALL = 0x0f
};

struct sprd_flash_driver_ops {
	int (*open_torch)(void *drvd, uint8_t idx);
	int (*close_torch)(void *drvd, uint8_t idx);
	int (*open_preflash)(void *drvd, uint8_t idx);
	int (*close_preflash)(void *drvd, uint8_t idx);
	int (*open_highlight)(void *drvd, uint8_t idx);
	int (*close_highlight)(void *drvd, uint8_t idx);
	int (*cfg_value_torch)(void *drvd, uint8_t idx,
				  struct sprd_flash_element *element);
	int (*cfg_value_preflash)(void *drvd, uint8_t idx,
				  struct sprd_flash_element *element);
	int (*cfg_value_highlight)(void *drvd, uint8_t idx,
				   struct sprd_flash_element *element);
	int (*get_flash_info)(void *drvd, uint8_t idx,
				   struct sprd_flash_capacity *info);
};

int sprd_flash_ctrl(struct sprd_img_set_flash *set_flash);
int sprd_flash_cfg(struct sprd_flash_cfg_param *parm);
int sprd_flash_register(const struct sprd_flash_driver_ops *ops,
			void *drvd,
			uint8_t flash_idx);
int sprd_flash_get_info(uint8_t flash_idx, uint8_t led_idx,
			struct sprd_flash_capacity *flash_capacity);
#endif
