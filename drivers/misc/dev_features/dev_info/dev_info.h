/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DEV_INFO_H__
#define __DEV_INFO_H__

#ifdef CONFIG_T_PRODUCT_INFO

#include <linux/types.h>

#define LOG_DEVINFO_TAG  "[PRODUCT_DEV_INFO]:" 
#define __FUN(f)   printk(KERN_ERR LOG_DEVINFO_TAG "~~~~~~~~~~~~ %s:%d ~~~~~~~~~\n", __func__,__LINE__)
#define klog(fmt, args...)    printk(KERN_ERR LOG_DEVINFO_TAG fmt, ##args)

#define show_content_len (128)


#define FULL_PRODUCT_DEVICE_CB(id, cb, args) \
    do { \
        full_product_device_info(id, NULL, cb, args); \
    } \
    while(0)

	
#define FULL_PRODUCT_DEVICE_INFO(id, info) \
    do { \
        full_product_device_info(id, info, NULL, NULL); \
    } \
    while(0)


#define FULL_PRODUCT_DEVICE_INFO_CAMERA( \
        invokeSocketIdx, \
        sensor_id, \
        sensor_name, \
        SensorFullWidth, \
        SensorFullHeight) \
do { \
    \
    if (sensor_id == 0) { \
        FULL_PRODUCT_DEVICE_CB(ID_SUB_CAM, get_camera_info, "sub"); \
    } \
    else { \
        FULL_PRODUCT_DEVICE_CB(ID_MAIN_CAM, get_camera_info, "main"); \
    } \
    sprintf(g_cam_info_struct[sensor_id].name, "%s", sensor_name); \
    g_cam_info_struct[sensor_id].id = invokeSocketIdx; \
    g_cam_info_struct[sensor_id].w = SensorFullWidth; \
    g_cam_info_struct[sensor_id].h = SensorFullHeight; \
} \
while(0)


typedef int (*FuncPtr)(char* buf, void *args);
	
typedef struct product_dev_info {
    char show[show_content_len];
    FuncPtr cb;
    void *args;
} product_info_arr;


enum product_dev_info_attribute {
    ID_LCD = 0,
    ID_TP = 1,
    ID_GYRO = 2,
    ID_GSENSOR = 3,
    ID_PSENSOR = 4,
    ID_MSENSOR = 5,
    ID_SUB_CAM = 6,
    ID_MAIN_CAM = 7,
    ID_FINGERPRINT = 8,
    ID_SECBOOT = 9, 
    ID_BL_LOCK_STATUS = 10, 
    ID_NFC = 11,
    ID_HALL = 12,
    ID_FLASH = 13,
// add new..
    
    ID_MAX 
};

///////////////////////////////////////////////////////////////////////////////////////////
int full_product_device_info(int id, const char *info, FuncPtr cb, void *args);
extern int get_mmc_chip_info(char *buf, void *arg0);
///////////////////////////////////////////////////////////////////////////////////////////
#endif
#endif /* __FP_DRV_H */
