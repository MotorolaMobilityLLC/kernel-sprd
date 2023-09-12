/******************** (C) COPYRIGHT 2020 Unisoc Communications Inc. ************
*
* File Name     : lis2dh.h
* Authors       : Drive and Tools Technical Resources Department-Sensor_SH Team
*                   : Tianmin.Yang (tianmin.yang@unisoc.com)
*
* Version       : V.0.0.1
* Date          : 2020/Sep/04
* Description   : Header file of LIS2DH accelerometer driver code 
*
********************************************************************************
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
* OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
* PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
* AS A RESULT, Unisoc SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
*******************************************************************************/
/*******************************************************************************
*Version History.
*
* Revision 0.0.1: 2020/Sep/04
* first revision
*
*******************************************************************************/
#ifndef __LIS2DH_H__
#define __LIS2DH_H__

#include <linux/ioctl.h>
#include <linux/i2c.h>

#define DEF_VAL                          0
#define SEC                              1000

#define LOW_POWER_MODE                   1
#define NORMAL_MODE                      2
#define HIGH_RESOLUTION_MODE             3
#define POWER_DOWN_MODE                  4

struct lis2dh_data {
         struct i2c_client *lis2dh_client;
         u8 *name;
         u8 chip_id;
         u8 fs_range;
         u8 operating_mode;
         int odr;
         int poll_interval;
         struct lis2dh_acc {
                 int x;
                 int y;
                 int z;
         } value;
         atomic_t enable;
         struct mutex lock;
         int64_t timestamp;
         ktime_t ktime;
         struct input_dev *input_dev;
         struct device *dev;

         struct work_struct report_data_work;
         struct workqueue_struct *acc_workqueue;
         struct hrtimer hr_timer;
         int on_before_suspend;
 };

struct odr_param {
         int odr;
         int interval_ms;
} odr_parame_table[] = {
         {1, (SEC/1)},       /* 1000ms */
         {10, (SEC/10)},     /* 100ms */
         {25, (SEC/25)},     /* 40ms */
         {50, (SEC/50)},     /* 20ms */
         {100, (SEC/100)},   /* 10ms */
         {200, (SEC/200)},   /* 5ms */
         {400, (SEC/400)}    /* 2ms */
};

struct reg_param {
         int label;           /*label = DEF_VAL when it is no need to use*/
         u8 reg_addr;
         u8 reg_value;
         u8 mask;
};

struct reg_param chip_id = {
         DEF_VAL, 0x0f, 0x33, 0xff};

struct reg_param range_arry[] = {
         {2, 0x23, 0x00, 0x30},      /*2G*/
         {4, 0x23, 0x10, 0x30},      /*4G*/
         {8, 0x23, 0x20, 0x30},      /*8G*/
         {16, 0x23, 0x30, 0x30}       /*16G*/
};

struct reg_param odr_array[] = {
         {1, 0x20, 0x10, 0xf0},      /*1Hz*/
         {10, 0x20, 0x20, 0xf0},      /*10Hz*/
         {25, 0x20, 0x30, 0xf0},      /*25Hz*/
         {50, 0x20, 0x40, 0xf0},      /*50Hz*/
         {100, 0x20, 0x50, 0xf0},      /*100Hz*/
         {200, 0x20, 0x60, 0xf0},      /*200Hz*/
         {400, 0x20, 0x70, 0xf0}       /*400Hz*/
};

struct reg_param mode_array[] = {
         {LOW_POWER_MODE, 0x20, 0x08, 0x08},
         {LOW_POWER_MODE, 0x23, 0x00, 0x08},
         {NORMAL_MODE, 0x20, 0x00, 0x08},
         {NORMAL_MODE, 0x23, 0x00, 0x08},
         {HIGH_RESOLUTION_MODE, 0x20, 0x00, 0x08},
         {HIGH_RESOLUTION_MODE, 0x23, 0x08, 0x08},
         {POWER_DOWN_MODE, 0x20, 0x00, 0xf0}
};

struct reg_param init_array[] = {
         {DEF_VAL, 0x20, 0x07, 0x07},                    /*enable xyz*/
         {DEF_VAL, 0x23, 0x00, 0x80},                    /*continuos update*/
         {DEF_VAL, 0x23, 0x00, 0x06},                    /*self test disabled*/
         {HIGH_RESOLUTION_MODE, 0x20, 0x00, 0x08},      /*mode*/
         {HIGH_RESOLUTION_MODE, 0x23, 0x08, 0x08},      /*mode*/
         {4, 0x23, 0x10, 0x30},                         /*range 4G*/
         {50, 0x20, 0x40, 0xf0}                         /*odr  50HZ*/
};
#endif