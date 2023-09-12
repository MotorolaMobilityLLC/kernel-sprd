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

#ifndef __MADEV_H_
#define __MADEV_H_

#define X86     0
#define MA_DRV_VERSION      (0x00004006)

/*[KERN_DEBUG|KERN_EMERG] usually,debug level used for the release version */
#define MALOGD_LEVEL    KERN_DEBUG

#define MA_CHR_FILE_NAME    "madev0"  /*do not neeed modify usually */
#define MA_CHR_DEV_NAME     "madev"   /*do not neeed modify usually */

#define MA_EINT_NAME        "sprd,finger_print-eint"


/*key define   just modify the KEY_FN_* for different platform*/
#define FINGERPRINT_SWIPE_UP        KEY_FN_F1
#define FINGERPRINT_SWIPE_DOWN      KEY_FN_F2
#define FINGERPRINT_SWIPE_LEFT      KEY_FN_F3
#define FINGERPRINT_SWIPE_RIGHT     KEY_FN_F4
#define FINGERPRINT_TAP             KEY_FN_F5
#define FINGERPRINT_DTAP            KEY_FN_F6
#define FINGERPRINT_LONGPRESS       KEY_FN_F7

/*settings macro end*/
#include <linux/poll.h>
#include <linux/notifier.h>
#include <linux/fb.h>
/*this two head file for the screen on/off test*/
#include <asm/ioctl.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/ktime.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
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
#include <linux/ioctl.h>
#include <linux/mm.h>

#include "x86-settings.h"

/*fprint_spi struct use to save the value*/
struct fprint_spi {
		u8 do_what;             /*工作内容*/
		u8 f_wake;              /*唤醒标志*/
		int value;

		volatile u8 f_irq;      /*中断标志*/
		volatile u8 u1_flag;    /*reserve for ours thread interrupt*/
		volatile u8 u2_flag;    /*reserve for ours thread interrupt*/
		volatile u8 f_repo;     /*上报开关  */
		spinlock_t spi_lock;
		struct spi_device *spi;
		struct list_head dev_entry;
		struct spi_message msg;
		struct spi_transfer xfer;
		struct input_dev *input;
		struct work_struct work;
		struct workqueue_struct *workq;
#ifdef CONFIG_HAS_EARLYSUSPEND
		struct early_suspend suspend;
#endif
		struct wakeup_source wl;
};

struct fprint_dev {
		dev_t idd;
		int major;
		int minor;
		struct cdev *chd;
		struct class *cls;
		struct device *dev;
};


/*function define extern the settings.h function */
extern void mas_select_transfer(struct spi_device *spi, int len);
extern int mas_finger_get_gpio_info(struct platform_device *pdev);
extern int mas_finger_set_gpio_info(int cmd);
extern void mas_enable_spi_clock(struct spi_device *spi);
extern void mas_disable_spi_clock(struct spi_device *spi);
extern unsigned int mas_get_irq(struct platform_device *pdev);
extern int mas_get_platform(void);
extern int mas_remove_platform(void);
extern void ma_spi_change(struct spi_device *spi, unsigned int speed, int flag);
extern int mas_get_interrupt_gpio(unsigned int index);
extern int mas_switch_power(unsigned int on_off);


/*use for the log print*/
#define MALOG_TAG "MAFP_"
#define MALOGE(x) printk(KERN_ERR "%s%s: error! %s is failed, ret = %d\n",\
		MALOG_TAG, __func__, x, ret);
#define MALOGF(x) printk(MALOGD_LEVEL "%s%s: debug! %s!\n",\
		MALOG_TAG, __func__, x);
#define MALOGD(x) MALOGF(x)
#define MALOGW(x) printk(KERN_WARNING "%s%s: warning! %s's ret = %d\n",\
		MALOG_TAG, __func__, x, ret);

#define MA_IOC_MAGIC	'M'
/*user for the ioctl,new version*/
#define MA_IOC_DELK     _IO(MA_IOC_MAGIC,  1)
/*dealy lock*/
#define MA_IOC_SLEP     _IO(MA_IOC_MAGIC,  2)
/*sleep, remove the process out of the runqueue*/
#define MA_IOC_WKUP     _IO(MA_IOC_MAGIC,  3)
/*wake up, sechdule the process into the runqueeue*/
#define MA_IOC_ENCK     _IO(MA_IOC_MAGIC,  4)
/*only use in tee while the spi clock is not enable*/
#define MA_IOC_DICK     _IO(MA_IOC_MAGIC,  5)
/*disable spi clock*/
#define MA_IOC_EINT     _IO(MA_IOC_MAGIC,  6)
/*enable irq*/
#define MA_IOC_DINT     _IO(MA_IOC_MAGIC,  7)
/*disable irq*/
#define MA_IOC_TPDW     _IO(MA_IOC_MAGIC,  8)
/*tap DOWN*/
#define MA_IOC_TPUP     _IO(MA_IOC_MAGIC,  9)
/*tap UP*/
#define MA_IOC_SGTP     _IO(MA_IOC_MAGIC,  11)
/*single tap*/
#define MA_IOC_DBTP     _IO(MA_IOC_MAGIC,  12)
/*double tap*/
#define MA_IOC_LGTP     _IO(MA_IOC_MAGIC,  13)
/*log tap*/

#define MA_IOC_VTIM     _IOR(MA_IOC_MAGIC,  14, unsigned char)
/*version time	not use*/
#define MA_IOC_CNUM     _IOR(MA_IOC_MAGIC,  15, unsigned char)
/*cover num	not use*/
#define MA_IOC_SNUM     _IOR(MA_IOC_MAGIC,  16, unsigned char)
/*sensor type   not use*/

#define MA_IOC_UKRP     _IOW(MA_IOC_MAGIC,  17, unsigned char)
/*user define the report key*/
#define MA_IOC_NAVW     _IO(MA_IOC_MAGIC,   18)
/*navigation   up*/
#define MA_IOC_NAVA     _IO(MA_IOC_MAGIC,   19)
/*navigation   left*/
#define MA_IOC_NAVS     _IO(MA_IOC_MAGIC,   20)
/*navigation   down*/
#define MA_IOC_NAVD     _IO(MA_IOC_MAGIC,   21)
/*navigation   right*/
/*the navigation cmd was referencing from e-game*/
#define MA_IOC_EIRQ     _IO(MA_IOC_MAGIC,   31)
/*request irq*/
#define MA_IOC_DIRQ     _IO(MA_IOC_MAGIC,   32)
/*free irq*/
#define MA_IOC_SPAR     _IOW(MA_IOC_MAGIC,   33, unsigned int)
/*register write reserve*/
#define MA_IOC_GPAR     _IOR(MA_IOC_MAGIC,   34, unsigned int)
/*register read reserve*/
/*get device version*/
#define MA_IOC_GVER     _IOR(MA_IOC_MAGIC,   35, unsigned int)
/*the version mapping in the u32 is the final  4+4+8,as ********
******* ****(major verson number) ****(minor version number)
*******(revised version number), the front 16 byte is reserved.
*/
#define MA_IOC_PWOF    _IO(MA_IOC_MAGIC,    36)
#define MA_IOC_PWON    _IO(MA_IOC_MAGIC,    37)
/*set spi speed*/
#define MA_IOC_STSP    _IOW(MA_IOC_MAGIC,   38, unsigned int)
#define MA_IOC_FD_WAIT_CMD     _IO(MA_IOC_MAGIC, 39)
/*use for fingerprintd to wait the factory apk call*/
#define MA_IOC_TEST_WAKE_FD    _IO(MA_IOC_MAGIC, 40)
/*use factory apk wake up fingerprintd*/
#define MA_IOC_TEST_WAIT_FD_RET     _IOR(MA_IOC_MAGIC, 41, unsigned int)
/*use factory to sleep and wait the fingerprintd complete to send back the result*/
#define MA_IOC_FD_WAKE_TEST_RET     _IOW(MA_IOC_MAGIC, 42, unsigned int)
/*fingerprintd send the result and wake up the factory apk*/
#define MA_IOC_GET_SCREEN       _IOR(MA_IOC_MAGIC, 43, unsigned int)
/*get the system's screen */
#define MA_IOC_GET_INT_STATE    _IOR(MA_IOC_MAGIC, 44, unsigned int)
/*get the int pin state high or low*/
#define MA_IOC_GET_FACTORY_FLAG    _IOR(MA_IOC_MAGIC, 53, unsigned int)
#define MA_IOC_SET_FACTORY_FLAG    _IOW(MA_IOC_MAGIC, 54, unsigned int)


#endif /* __MADEV_H_ */

