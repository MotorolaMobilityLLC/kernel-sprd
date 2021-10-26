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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <net/sock.h>
#include <linux/compat.h>
#include <linux/notifier.h>
#include "fpsensor_spi.h"
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#endif
//#include <linux/cdc_com.h>


#define FPSENSOR_SPI_VERSION              "fpsensor_spi_tee_spreadtrum_v1.22.2"
#define FP_NOTIFY                         1
#define FPSENSOR_INPUT                    0
#define REMOVE_FPSENSOR_DEV               1
#define SLEEP_WAKEUP_HIGH_LEV             0   //sc9832 must open this
#define FP_NOTIFY_ADF                     0  

#if FP_NOTIFY
#if FP_NOTIFY_ADF
#include <video/adf_notifier.h>
#define FP_NOTIFY_ON    DRM_MODE_DPMS_ON
#define FP_NOTIFY_OFF   DRM_MODE_DPMS_OFF
#else
#define FP_NOTIFY_ON    FB_BLANK_UNBLANK
#define FP_NOTIFY_OFF   FB_BLANK_POWERDOWN
#endif
#endif


/* debug log setting */
#define MODNAME "[fpCoreDriver][SN=%d]"
#undef pr_fmt
#define pr_fmt(fmt)  "%s:%d:" fmt, __func__, __LINE__

#define FP_LOG_DEBUG_LEVEL   3
#define FP_LOG_INFO_LEVEL    2
#define FP_LOG_ERROR_LEVEL   1

#define FP_LOG_LEVEL         3

#if (FP_LOG_LEVEL >= FP_LOG_DEBUG_LEVEL)
#define fpsensor_debug(fmt, args...)     pr_debug(MODNAME"[DBG] "fmt, g_cmd_sn, ##args)
#else
#define fpsensor_debug(fmt, args...)
#endif
#if (FP_LOG_LEVEL >= FP_LOG_INFO_LEVEL)
#define fpsensor_info(fmt, args...)      pr_info(MODNAME"[INFO] "fmt, g_cmd_sn, ##args)
#else
#define fpsensor_info(fmt, args...)
#endif
#if (FP_LOG_LEVEL >= FP_LOG_ERROR_LEVEL)
#define fpsensor_error(fmt, args...)     pr_err(MODNAME"[ERR] "fmt, g_cmd_sn, ##args)
#else
#define fpsensor_error(fmt, args...)
#endif


/*platform device name*/
#define FPSENSOR_DEV_NAME       "fpsensor"
/*device name after register in charater*/
#define FPSENSOR_CLASS_NAME     "fpsensor"
#define FPSENSOR_DEV_MAJOR      0 // 255
#define N_SPI_MINORS            32    /* ... up to 256 */
#define FPSENSOR_NR_DEVS        1
#define FPSENSOR_INPUT_NAME     "fpsensor_keys"


/***********************input *************************/
#ifndef FPSENSOR_INPUT_HOME_KEY
#define FPSENSOR_INPUT_HOME_KEY     KEY_HOMEPAGE /* KEY_HOME */
#define FPSENSOR_INPUT_MENU_KEY     KEY_MENU
#define FPSENSOR_INPUT_BACK_KEY     KEY_BACK
#define FPSENSOR_INPUT_FF_KEY       KEY_POWER
#define FPSENSOR_INPUT_CAMERA_KEY   KEY_CAMERA
#define FPSENSOR_INPUT_OTHER_KEY    KEY_VOLUMEDOWN  /* temporary key value for capture use */
#endif

#define FPSENSOR_NAV_UP_KEY     19  /*KEY_UP*/
#define FPSENSOR_NAV_DOWN_KEY   20  /*KEY_DOWN*/
#define FPSENSOR_NAV_LEFT_KEY   21  /*KEY_LEFT*/
#define FPSENSOR_NAV_RIGHT_KEY  22  /*KEY_RIGHT*/
#define FPSENSOR_NAV_TAP_KEY    23


/*************************************************************/
/* global variables                         */
static fpsensor_data_t *g_fpsensor = NULL;
uint32_t g_cmd_sn = 0;
static chipone_chip_info fpsensor_chip_info = {0,0,"unknow"};

int32_t  CHIPONE_FP_IC=0;

/* -------------------------------------------------------------------- */
/* fingerprint chip hardware configuration                                  */
/* -------------------------------------------------------------------- */
static void fpsensor_gpio_free(fpsensor_data_t *fpsensor)
{
    struct device *dev = &fpsensor->platform_device->dev;

    if (fpsensor->irq_gpio != 0) {
        devm_gpio_free(dev, fpsensor->irq_gpio);
        fpsensor->irq_gpio = 0;
    }
    if (fpsensor->reset_gpio != 0) {
        devm_gpio_free(dev, fpsensor->reset_gpio);
        fpsensor->reset_gpio = 0;
    }
}

static void fpsensor_irq_free(fpsensor_data_t *fpsensor_dev)
{
    if (fpsensor_dev->irq) {
        fpsensor_error("entry");
        free_irq(fpsensor_dev->irq, fpsensor_dev);
        fpsensor_dev->irq = 0;
        fpsensor_dev->irq_enabled = 0;
    }
}

static void fpsensor_irq_gpio_cfg(fpsensor_data_t *fpsensor)
{
    int error = 0;

    error = gpio_direction_input(fpsensor->irq_gpio);
    if (error) {
        fpsensor_error("setup fpsensor irq gpio for input failed!error[%d]\n", error);
        return ;
    }

    fpsensor->irq = gpio_to_irq(fpsensor->irq_gpio);
    fpsensor_debug("fpsensor irq number[%d]\n", fpsensor->irq);
    if (fpsensor->irq <= 0) {
        fpsensor_error("fpsensor irq gpio to irq failed!\n");
        return ;
    }

    return;
}

static int fpsensor_request_named_gpio(fpsensor_data_t *fpsensor_dev, const char *label, int *gpio)
{
    struct device *dev = &fpsensor_dev->platform_device->dev;
    struct device_node *np = dev->of_node;
    int ret = of_get_named_gpio(np, label, 0);

    if (ret < 0) {
        fpsensor_error("failed to get '%s'\n", label);
        return ret;
    }
    *gpio = ret;
    ret = devm_gpio_request(dev, *gpio, label);
    if (ret) {
        fpsensor_error("failed to request gpio %d\n", *gpio);
        return ret;
    }

    fpsensor_error("%s %d\n", label, *gpio);
    return ret;
}

/* delay us after reset */
static void fpsensor_hw_reset(int delay)
{
    fpsensor_debug("entry\n");
    gpio_set_value(g_fpsensor->reset_gpio, 1);

    udelay(100);
    gpio_set_value(g_fpsensor->reset_gpio, 0);

    udelay(1000);
    gpio_set_value(g_fpsensor->reset_gpio, 1);

    if (delay) {
        udelay(delay);
    }
    fpsensor_debug("exit\n");
    return;
}

static int fpsensor_get_gpio_dts_info(fpsensor_data_t *fpsensor)
{
    int ret = 0;

    fpsensor_debug("entry\n");
    // get interrupt gpio resource
    ret = fpsensor_request_named_gpio(fpsensor, "fpsensor,eint-gpio", &fpsensor->irq_gpio);
    if (ret) {
        fpsensor_error("Failed to request irq GPIO. ret = %d\n", ret);
        return -1;
    }

    // get reest gpio resourece
    ret = fpsensor_request_named_gpio(fpsensor, "fpsensor,reset-gpio", &fpsensor->reset_gpio);
    if (ret) {
        fpsensor_error("Failed to request reset GPIO. ret = %d\n", ret);
        return -1;
    }
    // set reset direction output
    gpio_direction_output(fpsensor->reset_gpio, 1);
    fpsensor_hw_reset(1250);

    return ret;
}

static void setRcvIRQ(int val)
{
    fpsensor_data_t *fpsensor_dev = g_fpsensor;
    fpsensor_dev->RcvIRQ = val;
}

/*static void fpsensor_hw_power_enable(u8 onoff)
{
    (void) onoff;
}*/

static void fpsensor_enable_irq(fpsensor_data_t *fpsensor_dev)
{
    fpsensor_debug("entry\n");
    setRcvIRQ(0);
    /* Request that the interrupt should be wakeable */
    if (fpsensor_dev->irq_enabled == 0) {
        enable_irq(fpsensor_dev->irq);
        fpsensor_dev->irq_enabled = 1;
        fpsensor_debug("fpsensor_enable_irq\n");
    }
    fpsensor_debug("exit\n");
    return;
}

static void fpsensor_disable_irq(fpsensor_data_t *fpsensor_dev)
{
    fpsensor_debug("entry\n");

    if (0 == fpsensor_dev->device_available) {
        fpsensor_error("devices not available\n");
    } else {
        if (0 == fpsensor_dev->irq_enabled) {
            fpsensor_error("irq already disabled\n");
        } else if (fpsensor_dev->irq) {
            disable_irq_nosync(fpsensor_dev->irq);
            fpsensor_dev->irq_enabled = 0;
            fpsensor_debug("disable interrupt!\n");
        }
    }
    setRcvIRQ(0);
    fpsensor_debug("exit\n");
    return;
}

static irqreturn_t fpsensor_irq(int irq, void *handle)
{
    fpsensor_data_t *fpsensor_dev = (fpsensor_data_t *)handle;

#if SLEEP_WAKEUP_HIGH_LEV
    int irqf = 0;
    irqf = IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_NO_SUSPEND;
    if (fpsensor_dev->suspend_flag == 1) {
        irq_set_irq_type(fpsensor_dev->irq, irqf);
        fpsensor_dev->suspend_flag = 0;
    }
#endif

    /* Make sure 'wakeup_enabled' is updated before using it
    ** since this is interrupt context (other thread...) */
    smp_rmb();
#if FPSENSOR_WAKEUP_SOURCE
	__pm_wakeup_event(&fpsensor_dev->ttw_wl, msecs_to_jiffies(1000));
#else	
    wake_lock_timeout(&fpsensor_dev->ttw_wl, msecs_to_jiffies(1000));
#endif
    setRcvIRQ(1);
    wake_up_interruptible(&fpsensor_dev->wq_irq_return);

    return IRQ_HANDLED;
}

// release and cleanup fpsensor char device
static void fpsensor_dev_cleanup(fpsensor_data_t *fpsensor)
{
    fpsensor_debug("entry\n");
#if FPSENSOR_INPUT
    if (fpsensor->input != NULL) {
        input_unregister_device(fpsensor->input);
        fpsensor->input = NULL;
    }
#endif
    cdev_del(&fpsensor->cdev);
    unregister_chrdev_region(fpsensor->devno, FPSENSOR_NR_DEVS);
    device_destroy(fpsensor->class, fpsensor->devno);
    class_destroy(fpsensor->class);
    fpsensor_debug("exit\n");
}

#if FPSENSOR_INPUT
static int fpsensor_input_init(fpsensor_data_t *fpsensor_dev)
{
    int status = 0;

    fpsensor_info("entry\n");
    if (!fpsensor_dev) {
        fpsensor_error("fpsensor device is nullptr.\n");
        status = -EINVAL;
        goto out;
    }

    fpsensor_dev->input = input_allocate_device();
    if (fpsensor_dev->input == NULL) {
        fpsensor_error("Failed to allocate input device.\n");
        status = -ENOMEM;
        goto out;
    }
    __set_bit(EV_KEY, fpsensor_dev->input->evbit);
    __set_bit(FPSENSOR_INPUT_HOME_KEY, fpsensor_dev->input->keybit);

    __set_bit(FPSENSOR_INPUT_MENU_KEY, fpsensor_dev->input->keybit);
    __set_bit(FPSENSOR_INPUT_BACK_KEY, fpsensor_dev->input->keybit);
    __set_bit(FPSENSOR_INPUT_FF_KEY, fpsensor_dev->input->keybit);

    __set_bit(FPSENSOR_NAV_TAP_KEY, fpsensor_dev->input->keybit);
    __set_bit(FPSENSOR_NAV_UP_KEY, fpsensor_dev->input->keybit);
    __set_bit(FPSENSOR_NAV_DOWN_KEY, fpsensor_dev->input->keybit);
    __set_bit(FPSENSOR_NAV_RIGHT_KEY, fpsensor_dev->input->keybit);
    __set_bit(FPSENSOR_NAV_LEFT_KEY, fpsensor_dev->input->keybit);
    __set_bit(FPSENSOR_INPUT_CAMERA_KEY, fpsensor_dev->input->keybit);
    fpsensor_dev->input->name = FPSENSOR_INPUT_NAME;
    if (input_register_device(fpsensor_dev->input)) {
        fpsensor_error("Failed to register input device.\n");
        status = -ENODEV;
        goto err1;
    }
    goto out;

err1:
    input_free_device(fpsensor_dev->input);
    fpsensor_dev->input = NULL;

out:
    fpsensor_debug("exit\n");
    return status;
}
#endif

static int fpsensor_enable_power(void)
{
    int ret = 0;
    fpsensor_data_t *fp_dev = g_fpsensor ;
    struct regulator *fp_reg = NULL;
    struct device dev = fp_dev->platform_device->dev;
 
    fpsensor_debug("get regulator from dts \n");

    fp_reg = regulator_get(&dev, "vcc_fp");

    if (IS_ERR(fp_reg)) {
        fpsensor_error("get regulator failed %d \n", IS_ERR(fp_reg));
        return IS_ERR(fp_reg);
    }

    ret = regulator_set_voltage(fp_reg, 2800000, 2800000);
    if (ret) {
        fpsensor_debug("regulator_set_voltage(%d)\n", ret);
        goto err;
    }
    ret = regulator_enable(fp_reg);
    if (ret) {
        fpsensor_error("regulator enable failed(%d)\n", ret);
        goto err;
    }
    return 0;

err:
    regulator_put(fp_reg);
    return ret;
}

static int fpsensor_disable_power(void)
{
    int ret = 0;
    ret = regulator_disable(g_fpsensor->vdd_reg);
    if (ret) {
        fpsensor_error("regulator disable failed(%d)\n", ret);
        goto err;
    }else{
        fpsensor_debug("regulator disable success\n");
        return 0;
    }
    regulator_put(g_fpsensor->vdd_reg);
err:
    regulator_put(g_fpsensor->vdd_reg);
    return ret;
}

static long fpsensor_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    fpsensor_data_t *fpsensor_dev = NULL;
    int retval = 0;
    unsigned int val = 0;
    int irqf;
#if FPSENSOR_INPUT
    struct fpsensor_key fpsensor_key;
    uint32_t key_event;
#endif
    fpsensor_debug("[rickon]: fpsensor ioctl cmd : 0x%x\n", cmd);
    fpsensor_dev = (fpsensor_data_t *)filp->private_data;
    fpsensor_dev->cancel = 0 ;
    switch (cmd) {
    case FPSENSOR_IOC_INIT:
        fpsensor_info("fpsensor init started======\n");
        retval = fpsensor_get_gpio_dts_info(fpsensor_dev);
        if (retval) {
            break;
        }
        fpsensor_irq_gpio_cfg(fpsensor_dev);
        //regist irq
        irqf = IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_NO_SUSPEND;
        retval = devm_request_threaded_irq(&g_fpsensor->platform_device->dev, g_fpsensor->irq, fpsensor_irq,
                                           NULL, irqf, dev_name(&g_fpsensor->platform_device->dev), g_fpsensor);
        //retval = request_irq(g_fpsensor->irq, fpsensor_irq,irqf, "fpsnesor_int",NULL);                            
        if (retval == 0) {
            fpsensor_info("irq thread reqquest success!\n");
        } else {
            fpsensor_error("irq thread request failed , retval =%d \n", retval);
            break;
        }
        enable_irq_wake(g_fpsensor->irq);
        fpsensor_dev->device_available = 1;
        // fix Unbalanced enable for IRQ, disable irq at first
        fpsensor_dev->irq_enabled = 1;
        fpsensor_disable_irq(fpsensor_dev);
        fpsensor_info("fpsensor init finished======\n");
        break;

    case FPSENSOR_IOC_EXIT:
        fpsensor_disable_irq(fpsensor_dev);
        fpsensor_dev->device_available = 0;
        fpsensor_irq_free(fpsensor_dev);
        fpsensor_gpio_free(fpsensor_dev);
        fpsensor_info("fpsensor exit finished======\n");
        break;

    case FPSENSOR_IOC_RESET:
        fpsensor_info("chip reset command\n");
        fpsensor_hw_reset(1250);
        break;

    case FPSENSOR_IOC_ENABLE_IRQ:
        fpsensor_info("chip enable IRQ command\n");
        fpsensor_enable_irq(fpsensor_dev);
        break;

    case FPSENSOR_IOC_DISABLE_IRQ:
        fpsensor_info("chip disable IRQ command\n");
        fpsensor_disable_irq(fpsensor_dev);
        break;
    case FPSENSOR_IOC_GET_INT_VAL:
        val = gpio_get_value(fpsensor_dev->irq_gpio);
        if (copy_to_user((void __user *)arg, (void *)&val, sizeof(uint32_t))) {
            fpsensor_error("Failed to copy data to user\n");
            retval = -EFAULT;
            break;
        }
        retval = 0;
        break;
    case FPSENSOR_IOC_ENABLE_SPI_CLK:
        fpsensor_debug("ENABLE_SPI_CLK ======\n");
        break;
    case FPSENSOR_IOC_DISABLE_SPI_CLK:
        fpsensor_debug("DISABLE_SPI_CLK ======\n");
        break;
    case FPSENSOR_IOC_ENABLE_POWER:
        fpsensor_info("FPSENSOR_IOC_ENABLE_POWER ======\n");
        //fpsensor_hw_power_enable(1);
        fpsensor_enable_power();
        break;
    case FPSENSOR_IOC_DISABLE_POWER:
        fpsensor_info("FPSENSOR_IOC_DISABLE_POWER ======\n");
        //fpsensor_hw_power_enable(0);
        break;
    case FPSENSOR_IOC_INIT_INPUT_DEV:
        fpsensor_debug("FPSENSOR_IOC_INIT_INPUT_DEV ======\n");
#if FPSENSOR_INPUT
        retval = fpsensor_input_init(fpsensor_dev);
        if (retval) {
            fpsensor_error("fpsensor init fail\n");
        }
#endif
        break;
#if FPSENSOR_INPUT
    case FPSENSOR_IOC_INPUT_KEY_EVENT:
        if (copy_from_user(&fpsensor_key, (struct fpsensor_key *)arg, sizeof(struct fpsensor_key))) {
            fpsensor_error("Failed to copy input key event from user to kernel\n");
            retval = -EFAULT;
            break;
        }
        if (FPSENSOR_KEY_HOME == fpsensor_key.key) {
            key_event = FPSENSOR_INPUT_HOME_KEY;
        } else if (FPSENSOR_KEY_POWER == fpsensor_key.key) {
            key_event = FPSENSOR_INPUT_FF_KEY;
        } else if (FPSENSOR_KEY_CAPTURE == fpsensor_key.key) {
            key_event = FPSENSOR_INPUT_CAMERA_KEY;
        } else {
            /* add special key define */
            key_event = FPSENSOR_INPUT_OTHER_KEY;
        }
        fpsensor_debug("received key event[%d], key=%d, value=%d\n",
                       key_event, fpsensor_key.key, fpsensor_key.value);
        if ((FPSENSOR_KEY_POWER == fpsensor_key.key || FPSENSOR_KEY_CAPTURE == fpsensor_key.key)
            && (fpsensor_key.value == 1)) {
            input_report_key(fpsensor_dev->input, key_event, 1);
            input_sync(fpsensor_dev->input);
            input_report_key(fpsensor_dev->input, key_event, 0);
            input_sync(fpsensor_dev->input);
        } else if (FPSENSOR_KEY_UP == fpsensor_key.key) {
            input_report_key(fpsensor_dev->input, FPSENSOR_NAV_UP_KEY, 1);
            input_sync(fpsensor_dev->input);
            input_report_key(fpsensor_dev->input, FPSENSOR_NAV_UP_KEY, 0);
            input_sync(fpsensor_dev->input);
        } else if (FPSENSOR_KEY_DOWN == fpsensor_key.key) {
            input_report_key(fpsensor_dev->input, FPSENSOR_NAV_DOWN_KEY, 1);
            input_sync(fpsensor_dev->input);
            input_report_key(fpsensor_dev->input, FPSENSOR_NAV_DOWN_KEY, 0);
            input_sync(fpsensor_dev->input);
        } else if (FPSENSOR_KEY_RIGHT == fpsensor_key.key) {
            input_report_key(fpsensor_dev->input, FPSENSOR_NAV_RIGHT_KEY, 1);
            input_sync(fpsensor_dev->input);
            input_report_key(fpsensor_dev->input, FPSENSOR_NAV_RIGHT_KEY, 0);
            input_sync(fpsensor_dev->input);
        } else if (FPSENSOR_KEY_LEFT == fpsensor_key.key) {
            input_report_key(fpsensor_dev->input, FPSENSOR_NAV_LEFT_KEY, 1);
            input_sync(fpsensor_dev->input);
            input_report_key(fpsensor_dev->input, FPSENSOR_NAV_LEFT_KEY, 0);
            input_sync(fpsensor_dev->input);
        } else  if (FPSENSOR_KEY_TAP == fpsensor_key.key) {
            input_report_key(fpsensor_dev->input, FPSENSOR_NAV_TAP_KEY, 1);
            input_sync(fpsensor_dev->input);
            input_report_key(fpsensor_dev->input, FPSENSOR_NAV_TAP_KEY, 0);
            input_sync(fpsensor_dev->input);
        } else if ((FPSENSOR_KEY_POWER != fpsensor_key.key) && (FPSENSOR_KEY_CAPTURE != fpsensor_key.key)) {
            input_report_key(fpsensor_dev->input, key_event, fpsensor_key.value);
            input_sync(fpsensor_dev->input);
        }
        break;
#endif
    case FPSENSOR_IOC_REMOVE:
        fpsensor_disable_irq(fpsensor_dev);
        fpsensor_dev->device_available = 0;
        fpsensor_irq_free(fpsensor_dev);
        fpsensor_gpio_free(fpsensor_dev);
#if REMOVE_FPSENSOR_DEV
        fpsensor_dev_cleanup(fpsensor_dev);
#endif
#if FP_NOTIFY
      #if FP_NOTIFY_ADF 
	  adf_unregister_client(&fpsensor_dev->notifier);
	  #else
        fb_unregister_client(&fpsensor_dev->notifier);
	  #endif
#endif
        //fpsensor_hw_power_enable(0);
        //wake_lock_destroy(&fpsensor_dev->ttw_wl);
     #if FPSENSOR_WAKEUP_SOURCE	
	  wakeup_source_trash(&fpsensor_dev->ttw_wl);
      #else
      wake_lock_destroy(&fpsensor_dev->ttw_wl);
       #endif
        fpsensor_disable_power();
        fpsensor_info("remove finished\n");
        break;
    case FPSENSOR_IOC_CANCEL_WAIT:
        fpsensor_info("FPSENSOR CANCEL WAIT\n");
        wake_up_interruptible(&fpsensor_dev->wq_irq_return);
        fpsensor_dev->cancel = 1;
        break;
#if FP_NOTIFY
    case FPSENSOR_IOC_GET_FP_STATUS :
        val = fpsensor_dev->fb_status;
        fpsensor_debug("FPSENSOR_IOC_GET_FP_STATUS  %d \n", fpsensor_dev->fb_status);
        if (copy_to_user((void __user *)arg, (void *)&val, sizeof(uint32_t))) {
            fpsensor_error("Failed to copy data to user\n");
            retval = -EFAULT;
            break;
        }
        retval = 0;
        break;
#endif
    case FPSENSOR_IOC_ENABLE_REPORT_BLANKON:
        if (copy_from_user(&val, (void __user *)arg, sizeof(uint32_t))) {
            retval = -EFAULT;
            break;
        }
        fpsensor_dev->enable_report_blankon = val;
        fpsensor_info("%s: FPSENSOR_IOC_ENABLE_REPORT_BLANKON: %d\n", __func__, val);
        break;
    case FPSENSOR_IOC_UPDATE_DRIVER_SN:
        if (copy_from_user(&g_cmd_sn, (void __user *)arg, sizeof(uint32_t))) {
            fpsensor_error("Failed to copy g_cmd_sn from user to kernel\n");
            retval = -EFAULT;
            break;
        }
        break;
	     case FPSENSOR_IOC_GET_CHIP_INFO:
        if (copy_from_user(&fpsensor_chip_info,
			(chipone_chip_info *)arg, sizeof(fpsensor_chip_info))) {
            fpsensor_error("fpsensor Failed to get fpsensor_chip_info from user\n");
            retval = -EFAULT;
            break;
	}
        fpsensor_error("fpsensor get chip info:hardware_id == %x, sensor_id ==%d, version info == %s \n",
                        fpsensor_chip_info.hardware_id,fpsensor_chip_info.sensor_id,fpsensor_chip_info.ver_name);
		CHIPONE_FP_IC=fpsensor_chip_info.hardware_id;
		#ifdef __WW6_CDC_COM__
			check_fpsensor_chip();
		#endif
        break;
    default:
        fpsensor_error("fpsensor doesn't support this command(0x%x)\n", cmd);
        break;
    }

    return retval;
}

static long fpsensor_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    return fpsensor_ioctl(filp, cmd, (unsigned long)(arg));
}

static unsigned int fpsensor_poll(struct file *filp, struct poll_table_struct *wait)
{
    unsigned int ret = 0;
    fpsensor_info("support poll opertion in version\n");
    ret |= POLLIN;
    poll_wait(filp, &g_fpsensor->wq_irq_return, wait);
    if (g_fpsensor->cancel == 1) {
        fpsensor_error("cancle\n");
        ret =  POLLERR;
        g_fpsensor->cancel = 0;
        return ret;
    }
    if (g_fpsensor->RcvIRQ) {
        if (g_fpsensor->RcvIRQ == 2) {
            fpsensor_debug("get fp on notify\n");
            ret |= POLLHUP;
        } else {
            fpsensor_error("******get irq\n");
            ret |= POLLRDNORM;
        }
    } else {
        ret = 0;
        fpsensor_debug("fpsensor_poll return 0\n");
    }
    return ret;
}

static int fpsensor_open(struct inode *inode, struct file *filp)
{
    fpsensor_data_t *fpsensor_dev;

    fpsensor_debug("entry\n");
    fpsensor_dev = container_of(inode->i_cdev, fpsensor_data_t, cdev);
    fpsensor_dev->users++;
    fpsensor_dev->device_available = 1;
    filp->private_data = fpsensor_dev;
    fpsensor_debug("exit\n");
    return 0;
}

static int fpsensor_release(struct inode *inode, struct file *filp)
{
    fpsensor_data_t *fpsensor_dev;
    int status = 0;

    fpsensor_error("entry\n");
    fpsensor_dev = filp->private_data;
    filp->private_data = NULL;

    /*last close??*/
    fpsensor_dev->users--;
    if (fpsensor_dev->users <= 0) {
        fpsensor_info("disble_irq. irq = %d\n", fpsensor_dev->irq);
        fpsensor_disable_irq(fpsensor_dev);
    }

    fpsensor_dev->device_available = 0;
    fpsensor_debug("exit\n");
    return status;
}

static ssize_t fpsensor_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    fpsensor_error("Not support read opertion in TEE version\n");
    return -EFAULT;
}

static ssize_t fpsensor_write(struct file *filp, const char __user *buf, size_t count,
                              loff_t *f_pos)
{
    fpsensor_error("Not support write opertion in TEE version\n");
    return -EFAULT;
}

static const struct file_operations fpsensor_fops = {
    .owner          = THIS_MODULE,
    .write          = fpsensor_write,
    .read           = fpsensor_read,
    .unlocked_ioctl = fpsensor_ioctl,
    .compat_ioctl   = fpsensor_compat_ioctl,
    .open           = fpsensor_open,
    .release        = fpsensor_release,
    .poll           = fpsensor_poll,

};

// create and register a char device for fpsensor
static int fpsensor_dev_setup(fpsensor_data_t *fpsensor)
{
    int ret = 0;
    dev_t dev_no = 0;
    struct device *dev = NULL;
    int fpsensor_dev_major = FPSENSOR_DEV_MAJOR;
    int fpsensor_dev_minor = 0;

    fpsensor_debug("entry\n");

    if (fpsensor_dev_major > 0) {
        dev_no = MKDEV(fpsensor_dev_major, fpsensor_dev_minor);
        ret = register_chrdev_region(dev_no, FPSENSOR_NR_DEVS, FPSENSOR_DEV_NAME);
    } else {
        ret = alloc_chrdev_region(&dev_no, fpsensor_dev_minor, FPSENSOR_NR_DEVS, FPSENSOR_DEV_NAME);
        fpsensor_dev_major = MAJOR(dev_no);
        fpsensor_dev_minor = MINOR(dev_no);
        fpsensor_info("fpsensor device major is %d, minor is %d\n", fpsensor_dev_major, fpsensor_dev_minor);
    }

    if (ret < 0) {
        fpsensor_error("can not get device major number %d\n", fpsensor_dev_major);
        goto out;
    }

    cdev_init(&fpsensor->cdev, &fpsensor_fops);
    fpsensor->cdev.owner = THIS_MODULE;
    fpsensor->cdev.ops   = &fpsensor_fops;
    fpsensor->devno      = dev_no;
    ret = cdev_add(&fpsensor->cdev, dev_no, FPSENSOR_NR_DEVS);
    if (ret) {
        fpsensor_error("add char dev for fpsensor failed\n");
        goto release_region;
    }

    fpsensor->class = class_create(THIS_MODULE, FPSENSOR_CLASS_NAME);
    if (IS_ERR(fpsensor->class)) {
        fpsensor_error("create fpsensor class failed\n");
        ret = PTR_ERR(fpsensor->class);
        goto release_cdev;
    }

    dev = device_create(fpsensor->class, &fpsensor->platform_device->dev, dev_no, fpsensor, FPSENSOR_DEV_NAME);
    if (IS_ERR(dev)) {
        fpsensor_error("create device for fpsensor failed\n");
        ret = PTR_ERR(dev);
        goto release_class;
    }
    fpsensor_debug("exit\n");
    return ret;

release_class:
    class_destroy(fpsensor->class);
    fpsensor->class = NULL;
release_cdev:
    cdev_del(&fpsensor->cdev);
release_region:
    unregister_chrdev_region(dev_no, FPSENSOR_NR_DEVS);
out:
    fpsensor_debug("exit\n");
    return ret;
}

#if FP_NOTIFY
static int fpsensor_fb_notifier_callback(struct notifier_block* self, unsigned long event, void* data)
{
    int retval = 0;
    static char screen_status[64] = { '\0' };
    struct fb_event* evdata = data;
    unsigned int blank;
    fpsensor_data_t *fpsensor_dev = g_fpsensor;

    fpsensor_debug("enter.  event : 0x%lx\n", event);
#if FP_NOTIFY_ADF
    if (event != ADF_EVENT_BLANK) {
        return NOTIFY_DONE;
    }
#else
    if (event != FB_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */) {
        return 0;
    }
#endif

    blank = *(int*)evdata->data;
    fpsensor_debug("enter, blank=0x%x\n", blank);

    switch (blank) {
    case FP_NOTIFY_ON:
    
        fpsensor_info("lcd on notify\n");
        sprintf(screen_status, "SCREEN_STATUS=%s", "ON");
        fpsensor_dev->fb_status = 1;
        if( fpsensor_dev->enable_report_blankon) {
            fpsensor_dev->RcvIRQ = 2;
            wake_up_interruptible(&fpsensor_dev->wq_irq_return);
        }
        break;
    case FP_NOTIFY_OFF:
        fpsensor_info("lcd off notify\n");
        sprintf(screen_status, "SCREEN_STATUS=%s", "OFF");
        fpsensor_dev->fb_status = 0;
        break;

    default:
        fpsensor_info("other notifier, ignore\n");
        break;
    }

    fpsensor_debug("%s leave.\n", screen_status);
    return retval;
}
#endif

static int fpsensor_probe(struct platform_device *pdev)
{
    int status = 0;
    fpsensor_data_t *fpsensor_dev = NULL;

    fpsensor_debug("entry\n");
    /* Allocate driver data */
    fpsensor_dev = kzalloc(sizeof(*fpsensor_dev), GFP_KERNEL);
    if (!fpsensor_dev) {
        status = -ENOMEM;
        fpsensor_error("Failed to alloc memory for fpsensor device.\n");
        goto out;
    }

    /* Initialize the driver data */
    g_fpsensor = fpsensor_dev;
    fpsensor_dev->platform_device   = pdev ;
    fpsensor_dev->device_available  = 0;
    fpsensor_dev->users             = 0;
    fpsensor_dev->irq               = 0;
    fpsensor_dev->power_gpio        = 0;
    fpsensor_dev->reset_gpio        = 0;
    fpsensor_dev->irq_gpio          = 0;
    fpsensor_dev->irq_enabled       = 0;
    fpsensor_dev->suspend_flag      = 0;
    fpsensor_dev->input             = NULL;
    fpsensor_dev->fb_status = 1;
    /* setup a char device for fpsensor */
    status = fpsensor_dev_setup(fpsensor_dev);
    if (status) {
        fpsensor_error("fpsensor setup char device failed, %d", status);
        goto release_drv_data;
    }

    init_waitqueue_head(&fpsensor_dev->wq_irq_return);
#if FPSENSOR_WAKEUP_SOURCE
	wakeup_source_init(&g_fpsensor->ttw_wl , "fpsensor_ttw_wl");
#else	
    wake_lock_init(&g_fpsensor->ttw_wl, WAKE_LOCK_SUSPEND, "fpsensor_ttw_wl");
#endif
    fpsensor_dev->device_available = 1;
    //fpsensor_hw_power_enable(1);
    udelay(1000);
#if FP_NOTIFY
    fpsensor_dev->notifier.notifier_call = fpsensor_fb_notifier_callback;
#if FP_NOTIFY_ADF
    status = adf_register_client(&fpsensor_dev->notifier);
    if (status < 0) {
        fpsensor_error("failed to register adf notifier");
    } else {
        fpsensor_debug("succeed in registering adf notifier");
    }
#else
    fb_register_client(&fpsensor_dev->notifier);
#endif
#endif

    fpsensor_info("finished, driver version: %s\n", FPSENSOR_SPI_VERSION);
    goto out;

release_drv_data:
    if (fpsensor_dev != NULL) {
        kfree(fpsensor_dev);
        fpsensor_dev = NULL;
        g_fpsensor = NULL;
    }
out:
    fpsensor_debug("exit\n");
    return status;
}

static int fpsensor_remove(struct platform_device *pdev)
{
    fpsensor_data_t *fpsensor_dev = g_fpsensor;
    fpsensor_error("entry\n");
    if (fpsensor_dev != NULL) {
        kfree(fpsensor_dev);
        fpsensor_dev = NULL;
    }
    g_fpsensor = NULL;
	
    fpsensor_debug("exit\n");
    return 0;
}

#ifdef CONFIG_PM
static int fpsensor_suspend(struct platform_device *pdev, pm_message_t state)
{
#if SLEEP_WAKEUP_HIGH_LEV
    int irqf = 0;
    irqf = IRQF_TRIGGER_HIGH | IRQF_ONESHOT | IRQF_NO_SUSPEND;
    irq_set_irq_type(g_fpsensor->irq, irqf);
    g_fpsensor->suspend_flag = 1;
#endif
    return 0;
}

static int fpsensor_resume(struct platform_device *pdev)
{
    return 0;
}
#endif

/*-------------------------------------------------------------------------*/
static struct of_device_id fpsensor_of_match[] = {
    { .compatible = "sprd,fingerprint-fpsensor", },
    {}
};
MODULE_DEVICE_TABLE(of, fpsensor_of_match);
static struct platform_driver fpsensor_driver = {
    .driver = {
        .name = FPSENSOR_DEV_NAME,
        .owner = THIS_MODULE,
        .of_match_table = fpsensor_of_match,
    },
    .probe = fpsensor_probe,
    .remove = fpsensor_remove,
#ifdef CONFIG_PM
    .suspend = fpsensor_suspend,
    .resume = fpsensor_resume,
#endif
};
module_platform_driver(fpsensor_driver);

MODULE_AUTHOR("xhli");
MODULE_DESCRIPTION(" Fingerprint chip TEE driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:fpsensor-drivers");
