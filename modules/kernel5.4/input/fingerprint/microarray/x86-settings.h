 /*
 * Copyright (C) 2012-2022 Microarray Co., Ltd.
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

#ifndef __X86_SETTINGS_H_
#define __X86_SETTINGS_H_

#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include "madev.h"
#include <linux/spi/spi.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>

/*macro settings*/
#define MA_DRV_NAME			 "madev"
#define MA_DTS_NAME			"microarray,afs121"
#define MA_EINT_DTS_NAME		"microarray,afs121"
#define MA_INT_PIN_LABEL	"finger_int_pin"

/*call madev function*/
extern int mas_plat_probe(struct platform_device *pdev);
extern int mas_plat_remove(struct platform_device *pdev);


/* the interface called by madev*/
void mas_select_transfer(struct spi_device *spi, int len);
int mas_finger_get_gpio_info(struct platform_device *pdev);
int mas_finger_set_gpio_info(int cmd);
void mas_enable_spi_clock(struct spi_device *spi);
void mas_disable_spi_clock(struct spi_device *spi);
unsigned int mas_get_irq(struct platform_device *pdev);
int mas_get_platform(void);
int mas_remove_platform(void);
void ma_spi_change(struct spi_device *spi, unsigned int speed, int flag);
int mas_get_interrupt_gpio(unsigned int index);
int mas_switch_power(unsigned int on_off);
int mas_set_enable_gpio(struct platform_device *pdev);
#endif
