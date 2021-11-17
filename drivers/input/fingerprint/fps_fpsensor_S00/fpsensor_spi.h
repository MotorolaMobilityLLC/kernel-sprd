/*
 * Copyright (C) 2012-2022 fpsensor Technology (Beijing) Co., Ltd.
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

#ifndef __FPSENSOR_SPI_H
#define __FPSENSOR_SPI_H

#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/regulator/consumer.h>

/*********************** wakelock setting **********************/
#ifdef CONFIG_PM_WAKELOCKS
#define FPSENSOR_WAKEUP_SOURCE  1
#else
#define FPSENSOR_WAKEUP_SOURCE  0
// others
#endif

#if FPSENSOR_WAKEUP_SOURCE
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

/**********************IO Magic**********************/
#define FPSENSOR_IOC_MAGIC      0xf0

#define CONFIG_OF               1


typedef enum fpsensor_key_event {
    FPSENSOR_KEY_NONE = 0,
    FPSENSOR_KEY_HOME,
    FPSENSOR_KEY_POWER,
    FPSENSOR_KEY_MENU,
    FPSENSOR_KEY_BACK,
    FPSENSOR_KEY_CAPTURE,
    FPSENSOR_KEY_UP,
    FPSENSOR_KEY_DOWN,
    FPSENSOR_KEY_RIGHT,
    FPSENSOR_KEY_LEFT,
    FPSENSOR_KEY_TAP,
    FPSENSOR_KEY_HEAVY
} fpsensor_key_event_t;

struct fpsensor_key {
    enum fpsensor_key_event key;
    uint32_t value;   /* key down = 1, key up = 0 */
};

/* define commands */
#define FPSENSOR_IOC_INIT                       _IOWR(FPSENSOR_IOC_MAGIC,0,uint32_t)
#define FPSENSOR_IOC_EXIT                       _IOWR(FPSENSOR_IOC_MAGIC,1,uint32_t)
#define FPSENSOR_IOC_RESET                      _IOWR(FPSENSOR_IOC_MAGIC,2,uint32_t)
#define FPSENSOR_IOC_ENABLE_IRQ                 _IOWR(FPSENSOR_IOC_MAGIC,3,uint32_t)
#define FPSENSOR_IOC_DISABLE_IRQ                _IOWR(FPSENSOR_IOC_MAGIC,4,uint32_t)
#define FPSENSOR_IOC_GET_INT_VAL                _IOWR(FPSENSOR_IOC_MAGIC,5,uint32_t)
#define FPSENSOR_IOC_DISABLE_SPI_CLK            _IOWR(FPSENSOR_IOC_MAGIC,6,uint32_t)
#define FPSENSOR_IOC_ENABLE_SPI_CLK             _IOWR(FPSENSOR_IOC_MAGIC,7,uint32_t)
#define FPSENSOR_IOC_ENABLE_POWER               _IOWR(FPSENSOR_IOC_MAGIC,8,uint32_t)
#define FPSENSOR_IOC_DISABLE_POWER              _IOWR(FPSENSOR_IOC_MAGIC,9,uint32_t)
#define FPSENSOR_IOC_INPUT_KEY_EVENT            _IOWR(FPSENSOR_IOC_MAGIC, 10, struct fpsensor_key)
/* fp sensor has change to sleep mode while screen off */
#define FPSENSOR_IOC_ENTER_SLEEP_MODE           _IOWR(FPSENSOR_IOC_MAGIC,11,uint32_t)
#define FPSENSOR_IOC_REMOVE                     _IOWR(FPSENSOR_IOC_MAGIC,12,uint32_t)
#define FPSENSOR_IOC_CANCEL_WAIT                _IOWR(FPSENSOR_IOC_MAGIC,13,uint32_t)
#define FPSENSOR_IOC_GET_FP_STATUS              _IOWR(FPSENSOR_IOC_MAGIC,19,uint32_t)
#define FPSENSOR_IOC_INIT_INPUT_DEV             _IOWR(FPSENSOR_IOC_MAGIC, 20, uint32_t)
#define FPSENSOR_IOC_ENABLE_REPORT_BLANKON      _IOWR(FPSENSOR_IOC_MAGIC,21,uint32_t)
#define FPSENSOR_IOC_UPDATE_DRIVER_SN           _IOWR(FPSENSOR_IOC_MAGIC,22,uint32_t)
#define FPSENSOR_IOC_GET_CHIP_INFO              _IOWR(FPSENSOR_IOC_MAGIC,25,unsigned int*)
typedef struct {
    dev_t devno;
    struct class *class;
    struct device *device;
    struct cdev cdev;
    struct platform_device *platform_device;
    struct input_dev *input;
    unsigned int users;
    u8 device_available;    /* changed during fingerprint chip sleep and wakeup phase */
    u8 irq_enabled;
    volatile unsigned int RcvIRQ;
    int irq;
    int irq_gpio;
    int reset_gpio;
    int power_gpio;
    #if FPSENSOR_WAKEUP_SOURCE
    struct wakeup_source ttw_wl;
     #else	
    struct wake_lock ttw_wl;
     #endif
    wait_queue_head_t wq_irq_return;
    int cancel;
    u8 suspend_flag;
    struct notifier_block notifier;
    u8 fb_status;
    int enable_report_blankon;
    struct regulator *vdd_reg;
} fpsensor_data_t;

typedef struct 
{
    int32_t hardware_id;
    int32_t sensor_id;
    char ver_name[64];
}chipone_chip_info;
#endif    /* __FPSENSOR_SPI_TEE_H */
