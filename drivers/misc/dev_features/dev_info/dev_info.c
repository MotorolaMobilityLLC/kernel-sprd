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

#ifdef CONFIG_T_PRODUCT_INFO

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>             
#include <linux/mm.h>
#include "dev_info.h"
#include <linux/module.h>
//#include <linux/dev_info.h>

/* /sys/devices/platform/$PRODUCT_DEVICE_INFO */
#define PRODUCT_DEVICE_INFO    "product-device-info"

///////////////////////////////////////////////////////////////////
static int dev_info_probe(struct platform_device *pdev);
static int dev_info_remove(struct platform_device *pdev);
static ssize_t store_product_dev_info(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t show_product_dev_info(struct device *dev, struct device_attribute *attr, char *buf);
///////////////////////////////////////////////////////////////////
static product_info_arr prod_dev_array[ID_MAX];
static product_info_arr *pi_p = prod_dev_array;

static struct platform_driver dev_info_driver = {
	.probe = dev_info_probe,
	.remove = dev_info_remove,
	.driver = {
		   .name = PRODUCT_DEVICE_INFO,
	},
};

static struct platform_device dev_info_device = {
    .name = PRODUCT_DEVICE_INFO,
    .id = -1,
};

#define PRODUCT_DEV_INFO_ATTR(_name)                         \
{                                       \
	.attr = { .name = #_name, .mode = S_IRUGO | S_IWUSR | S_IWGRP,},  \
	.show = show_product_dev_info,                  \
	.store = store_product_dev_info,                              \
}

// emmc     /sys/block/mmcblk0/device/chipinfo
// battery  /sys/class/power_supply/battery/voltage_now

static struct device_attribute product_dev_attr_array[] = {
    PRODUCT_DEV_INFO_ATTR(info_lcd),
    PRODUCT_DEV_INFO_ATTR(info_tp),
    PRODUCT_DEV_INFO_ATTR(info_gyro),
    PRODUCT_DEV_INFO_ATTR(info_gsensor),
    PRODUCT_DEV_INFO_ATTR(info_psensor),
    PRODUCT_DEV_INFO_ATTR(info_msensor),
    PRODUCT_DEV_INFO_ATTR(info_sub_camera),
    PRODUCT_DEV_INFO_ATTR(info_main_camera),
    PRODUCT_DEV_INFO_ATTR(info_fingerprint),
    PRODUCT_DEV_INFO_ATTR(info_secboot),
    PRODUCT_DEV_INFO_ATTR(info_bl_lock_status),
    PRODUCT_DEV_INFO_ATTR(info_nfc),
    PRODUCT_DEV_INFO_ATTR(info_hall),
    PRODUCT_DEV_INFO_ATTR(info_flash),
// add new ...
    
};
int get_mmc_chip_info(char *buf, void *arg0)
{										
    struct mmc_card *card = (struct mmc_card *)arg0;
    char tempID[64] = "";
    char vendorName[16] = "";
    char romsize[8] = "";
    char ramsize[8] = "";
    struct sysinfo si;
    si_meminfo(&si);
    if(si.totalram > 1572864 )				   // 6G = 1572864 	(256 *1024)*6
   		strcpy(ramsize , "8G");
    else if(si.totalram > 1048576)			  // 4G = 786432 	(256 *1024)*4
    		strcpy(ramsize , "6G");
    else if(si.totalram > 786432)			 // 3G = 786432 	(256 *1024)*3
    		strcpy(ramsize , "4G");
    else if(si.totalram > 524288)			// 2G = 524288 	(256 *1024)*2
    		strcpy(ramsize , "3G");
    else if(si.totalram > 262144)               // 1G = 262144		(256 *1024)     4K page size
    		strcpy(ramsize , "2G");
    else if(si.totalram > 131072)               // 512M = 131072		(256 *1024/2)   4K page size
    		strcpy(ramsize , "1G");
    else
    		strcpy(ramsize , "512M");
    
    if(card->ext_csd.sectors > 134217728)  		
		strcpy(romsize , "128G");
    else if(card->ext_csd.sectors > 67108864)  	// 67108864 = 32G *1024*1024*1024 /512            512 page	
		strcpy(romsize , "64G");
    else if(card->ext_csd.sectors > 33554432)  // 33554432 = 16G *1024*1024*1024 /512            512 page
		strcpy(romsize , "32G");
    else if(card->ext_csd.sectors > 16777216)  // 16777216 = 8G *1024*1024*1024 /512            512 page
		strcpy(romsize , "16G");
    else if(card->ext_csd.sectors > 8388608)  // 8388608 = 4G *1024*1024*1024 /512            512 page
		strcpy(romsize , "8G");
    else
		strcpy(romsize , "4G");	
	
    memset(tempID, 0, sizeof(tempID));
    sprintf(tempID, "%08x ", card->raw_cid[0]);
	klog("FlashID is %s, totalram= %ld, emmc_capacity =%d\n",tempID, si.totalram, card->ext_csd.sectors);

    if(strncasecmp((const char *)tempID, "90", 2) == 0)           // 90 is OEMid for Hynix 
   		strcpy(vendorName , "Hynix");
    else if(strncasecmp((const char *)tempID, "15", 2) == 0)		// 15 is OEMid for Samsung 
   		strcpy(vendorName , "Samsung");
    else if(strncasecmp((const char *)tempID, "45", 2) == 0)		// 45 is OEMid for Sandisk 
   		strcpy(vendorName , "Sandisk");
    else if(strncasecmp((const char *)tempID, "70", 2) == 0)		// 70 is OEMid for Kingston 
   		strcpy(vendorName , "Kingston");
    else if(strncasecmp((const char *)tempID, "88", 2) == 0)		// 88 is OEMid for Foresee 
   		strcpy(vendorName , "Foresee");
    else if(strncasecmp((const char *)tempID, "d6", 2) == 0)
   		strcpy(vendorName , "Foresee");
    else if(strncasecmp((const char *)tempID, "f4", 2) == 0)		// f4 is OEMid for Biwin 
   		strcpy(vendorName , "Biwin");
    else if(strncasecmp((const char *)tempID, "13", 2) == 0)		// 13 is OEMid for Micron 
   		strcpy(vendorName , "Micron");
    else
		strcpy(vendorName , "Unknown");

    memset(tempID, 0, sizeof(tempID));
    sprintf(tempID,"%s_%s+%s,prv:%02x,life:%02x,%02x",vendorName,romsize,ramsize,card->cid.prv,
           card->ext_csd.device_life_time_est_typ_a,card->ext_csd.device_life_time_est_typ_b);
   
    return sprintf(buf,"%s", tempID);					
}
///////////////////////////////////////////////////////////////////////////////////////////
/*
* int full_product_device_info(int id, const char *info, int (*cb)(char* buf, void *args), void *args);
*/
int full_product_device_info(int id, const char *info, FuncPtr cb, void *args) {   
    klog("%s: - [%d, %s, %pf]\n", __func__, id, info, cb); 

    if (id >= 0 &&  id < ID_MAX ) {
        memset(pi_p[id].show, 0, show_content_len);
        if (cb != NULL && pi_p[id].cb == NULL) {
            pi_p[id].cb = cb;
            pi_p[id].args = args; 
        }
        else if (info != NULL) {
            strcpy(pi_p[id].show, info);
            pi_p[id].cb = NULL;
            pi_p[id].args = NULL;
        }
        return 0;
    }
    return -1;
}
EXPORT_SYMBOL(full_product_device_info);

static ssize_t store_product_dev_info(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    return count;
}

static ssize_t show_product_dev_info(struct device *dev, struct device_attribute *attr, char *buf) {
    int i = 0;
    char *show = NULL;
    const ptrdiff_t x = (attr - product_dev_attr_array);
   
    if (x >= ID_MAX) {
        BUG_ON(1);
    }

    show = pi_p[x].show;
    if (pi_p[x].cb != NULL) {
        pi_p[x].cb(show, pi_p[x].args);
    }

    klog("%s: - offset(%d): %s\n", __func__, (int)x, show);
    if (strlen(show) > 0) {
        i = sprintf(buf, "%s ", show);
    }
    else {
        klog("%s - offset(%d): NULL!\n", __func__, (int)x);
    }

    return i;
}

static int dev_info_probe(struct platform_device *pdev) {	
    int i, rc;

    __FUN(); 
    for (i = 0; i < ARRAY_SIZE(product_dev_attr_array); i++) {
        rc = device_create_file(&pdev->dev, &product_dev_attr_array[i]);
        if (rc) {
            klog( "%s, create_attrs_failed:%d,%d\n", __func__, i, rc);
        }
    }

    return 0;
}

static int dev_info_remove(struct platform_device *pdev) {    
    int i;
	
     __FUN();
    for (i = 0; i < ARRAY_SIZE(product_dev_attr_array); i++) {
        device_remove_file(&pdev->dev, &product_dev_attr_array[i]);
    }
    return 0;
}

static int __init dev_info_drv_init(void) {
    __FUN();

    if (platform_device_register(&dev_info_device) != 0) {
        klog( "device_register fail!.\n");
        return -1;
    
    }
	
    if (platform_driver_register(&dev_info_driver) != 0) {
        klog( "driver_register fail!.\n");
        return -1;
    }
	
    return 0;
}

static void __init dev_info_drv_exit(void) {
	__FUN();
	platform_driver_unregister(&dev_info_driver);
	platform_device_unregister(&dev_info_device);
}

///////////////////////////////////////////////////////////////////
late_initcall(dev_info_drv_init);
module_exit(dev_info_drv_exit);

#endif

