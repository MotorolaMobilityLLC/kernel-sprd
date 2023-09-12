/**
 * The device control driver for Sunwave's fingerprint sensor.
 *
 * Copyright (C) 2016 Sunwave Corporation. <http://www.sunwavecorp.com>
 * Copyright (C) 2016 Langson L. <mailto: liangzh@sunwavecorp.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
**/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/uaccess.h>

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <linux/gpio.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

#include "sf_ctl.h"

#define SUNWAVE_RST_PIN 136
#define SUNWAVE_IRQ_PIN 142

u8 suspend_flag;

struct sf_fp_platform_data {
	int irq_gpio_number;
	int reset_gpio_number;
	int pwr_gpio_number;
	const char *vdd_name;
};

struct sf_fp_platform_data *pdata;

struct platform_device *sf_pdev;

#define MODULE_NAME "sf_ctl"
#define xprintk(level, fmt, args...) printk(level MODULE_NAME": "fmt, ##args)

#if ANDROID_WAKELOCK
#include <linux/pm_wakeup.h>
static struct wakeup_source *wake;
#endif

static int sf_ctl_init_irq(void);
static int sf_ctl_init_gpio_pins(struct device *dev);
static int sf_ctl_deinit_gpio_pins(void);
static int sf_ctl_init_input(void);
static int sunwave_set_irq_type(unsigned long type);
/**
 * Define the driver version string.
 * There is NO need to modify 'rXXXX_yyyymmdd',
 * it should be updated automatically
 * by the building script (see the 'Driver-revision' section in 'build.sh').
 */
#define SF_DRV_VERSION "v1.2.1-20170707"

struct sf_ctl_device {
	struct miscdevice miscdev;
	int irq_num;
	struct work_struct work_queue;
	struct input_dev *input;
#if ANDROID_WAKELOCK
	struct wakeup_source wakelock;
#endif
};

static int sf_ctl_device_power(bool on)
{
	int err = 0;
#if SUPPLY_POWER_BY_REGULATOR
	struct regulator *reg_vdd;

	pdata->vdd_name = "vddsdio";
	reg_vdd = regulator_get(NULL, pdata->vdd_name);

	if (IS_ERR(reg_vdd)) {
		err = PTR_ERR(reg_vdd);
		xprintk(KERN_ERR, "Regulator get failed vdd err=%d\n", err);
		return err;
	}

	err = regulator_set_voltage(reg_vdd, 2800000, 2800000);

	if (err) {
		xprintk(KERN_ERR, "regulator_set_voltage(%d) failed!\n", err);
		goto reg_vdd_put;
	}

	if (on)
		regulator_enable(reg_vdd);
	else
		/*regulator_disable(reg_vdd)*/;

	return err;
reg_vdd_put:
	regulator_put(reg_vdd);
#elif SUPPLY_POWER_BY_GPIO
	xprintk(KERN_DEBUG, "%s(..) enter.\n", __func__);
	if (pdata->pwr_gpio_number == 0) {
		xprintk(KERN_ERR, "pdata->pwr_gpio_number is not get.\n");
		return -1;
	}
	gpio_set_value(pdata->pwr_gpio_number, 1);
	msleep(20);
	gpio_set_value(pdata->pwr_gpio_number, 0);
	msleep(20);
	gpio_set_value(pdata->pwr_gpio_number, 1);
#endif

	return err;
}

static int sf_spi_clock_enable(bool on)
{
	int err = 0;
	return err;
}

static int sf_ctl_device_reset(void)
{
	int err = 0;

	gpio_set_value(pdata->reset_gpio_number, 1);
	msleep(20);
	gpio_set_value(pdata->reset_gpio_number, 0);
	msleep(20);
	gpio_set_value(pdata->reset_gpio_number, 1);
	return err;
}

static void sf_ctl_device_event(struct work_struct *ws)
{
	struct sf_ctl_device *sf_ctl_dev =
		container_of(ws, struct sf_ctl_device, work_queue);
	char *uevent_env[2] = { "SPI_STATE=finger", NULL };

	xprintk(KERN_DEBUG, "%s(..) enter.\n", __func__);
	kobject_uevent_env(&sf_ctl_dev->miscdev.this_device->kobj,
				KOBJ_CHANGE, uevent_env);
}

static irqreturn_t sf_ctl_device_irq(int irq, void *dev_id)
{
	struct sf_ctl_device *sf_ctl_dev = (struct sf_ctl_device *)dev_id;

	disable_irq_nosync(irq);
	xprintk(KERN_DEBUG, "%s(irq = %d, ..) toggled.\n", __func__, irq);
	schedule_work(&sf_ctl_dev->work_queue);
#if ANDROID_WAKELOCK
	__pm_wakeup_event(wake, msecs_to_jiffies(5000));
#endif
	sunwave_set_irq_type(IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND |
				IRQF_ONESHOT);
	enable_irq(irq);
	return IRQ_HANDLED;
}

static int sf_ctl_report_key_event(struct input_dev *input,
					struct sf_key_event_t *kevent)
{
	int err = 0;
	unsigned int key_code = KEY_UNKNOWN;

	xprintk(KERN_DEBUG, "%s(..) enter.\n", __func__);

	switch (kevent->key) {
	case SF_KEY_HOME:
		key_code = KEY_HOME;
		break;

	case SF_KEY_MENU:
		key_code = KEY_MENU;
		break;

	case SF_KEY_BACK:
		key_code = KEY_BACK;
		break;

	case SF_KEY_F11:
		key_code = KEY_F11;
		break;

	case SF_KEY_ENTER:
		key_code = KEY_ENTER;
		break;

	case SF_KEY_UP:
		key_code = KEY_UP;
		break;

	case SF_KEY_LEFT:
		key_code = KEY_LEFT;
		break;

	case SF_KEY_RIGHT:
		key_code = KEY_RIGHT;
		break;

	case SF_KEY_DOWN:
		key_code = KEY_DOWN;
		break;

	case SF_KEY_WAKEUP:
		key_code = KEY_WAKEUP;
		break;

	default:
		break;
	}

	xprintk(KERN_DEBUG, "%s(..) enter.\n", __func__);
	input_report_key(input, key_code, kevent->value);
	input_sync(input);
	xprintk(KERN_DEBUG, "%s(..) leave.\n", __func__);
	return err;
}

static const char *sf_ctl_get_version(void)
{
	static char version[SF_DRV_VERSION_LEN] = { '\0', };

	strncpy(version, SF_DRV_VERSION, SF_DRV_VERSION_LEN);
	version[SF_DRV_VERSION_LEN - 1] = '\0';
	return (const char *)version;
}

/*struct file_operations fields.*/
static long sf_ctl_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = (struct miscdevice *)filp->private_data;
	struct sf_ctl_device *sf_ctl_dev =
		container_of(dev, struct sf_ctl_device, miscdev);
	int err = 0;
	struct sf_key_event_t kevent;

	xprintk(KERN_DEBUG, "%s(cmd = 0x%08x, ..)\n", __func__, cmd);

	switch (cmd) {
	case SF_IOC_INIT_DRIVER:{
#if MULTI_HAL_COMPATIBLE
			sf_ctl_init_gpio_pins(&sf_pdev->dev);
#endif
			break;
		}

	case SF_IOC_DEINIT_DRIVER:{
#if MULTI_HAL_COMPATIBLE
			sf_ctl_deinit_gpio_pins();
#endif
			break;
		}

	case SF_IOC_RESET_DEVICE:{
			sf_ctl_device_reset();
			break;
		}

	case SF_IOC_ENABLE_IRQ:{
			/*TODO:*/
			break;
		}

	case SF_IOC_DISABLE_IRQ:{
			/*TODO:*/
			break;
		}

	case SF_IOC_REQUEST_IRQ:{
#if MULTI_HAL_COMPATIBLE
			sf_ctl_init_irq();
#endif
			break;
		}

	case SF_IOC_ENABLE_SPI_CLK:{
			sf_spi_clock_enable(true);
			break;
		}

	case SF_IOC_DISABLE_SPI_CLK:{
			sf_spi_clock_enable(false);
			break;
		}

	case SF_IOC_ENABLE_POWER:{
#if MULTI_HAL_COMPATIBLE
			sf_ctl_device_power(true);
#endif
			break;
		}

	case SF_IOC_DISABLE_POWER:{
#if MULTI_HAL_COMPATIBLE
			sf_ctl_device_power(false);
#endif
			break;
		}

	case SF_IOC_REPORT_KEY_EVENT:{
			if (copy_from_user
				(&kevent, (struct sf_key_event_t *) arg,
				sizeof(struct sf_key_event_t))) {
				xprintk(KERN_ERR,
					"copy_from_user(..) failed.\n");
				err = (-EFAULT);
				break;
			}

			err =
				sf_ctl_report_key_event(sf_ctl_dev->input, &kevent);
			break;
		}

	case SF_IOC_SYNC_CONFIG:{
			/*TODO:*/
			break;
		}

	case SF_IOC_GET_VERSION:{
			if (copy_to_user
				((void *)arg, sf_ctl_get_version(),
				SF_DRV_VERSION_LEN)) {
				xprintk(KERN_ERR, "copy_to_user(..) failed.\n");
				err = (-EFAULT);
				break;
			}

			break;
		}

	default:
		err = (-EINVAL);
		break;
	}

	return err;
}

#ifdef CONFIG_COMPAT
static long sf_ctl_compat_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	return sf_ctl_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int sf_ctl_open(struct inode *inode, struct file *filp)
{
	xprintk(KERN_DEBUG, "%s(..) enter.\n", __func__);
	return 0;
}

static int sf_ctl_release(struct inode *inode, struct file *filp)
{
	xprintk(KERN_DEBUG, "%s(..) enter.\n", __func__);
	return 0;
}

static const struct file_operations sf_ctl_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = sf_ctl_ioctl,
	.open = sf_ctl_open,
	.release = sf_ctl_release,
#ifdef CONFIG_COMPAT
	.compat_ioctl = sf_ctl_compat_ioctl,
#endif
};

static struct sf_ctl_device sf_ctl_dev = {
	.miscdev = {
			.minor = MISC_DYNAMIC_MINOR,
			.name = "sunwave_fp",
			.fops = &sf_ctl_fops,
			}, 0,
};

#ifdef CONFIG_OF
static const struct of_device_id sf_of_match[] = {
	{.compatible = "sunwave,fingerprint",},
	{}
};
MODULE_DEVICE_TABLE(of, sf_of_match);
#endif

static int sf_probe(struct platform_device *pdev)
{
	int err = 0;

	sf_pdev = pdev;
	xprintk(KERN_ERR, "sunwave %s enter\n", __func__);

#if ANDROID_WAKELOCK
	//wakeup_source_init(&sf_ctl_dev.wakelock, "sunwave_wl");
	wake = wakeup_source_register(NULL, "sunwave_wl");
	if (!wake) {
		return -ENOMEM;
	}
#endif

#if MULTI_HAL_COMPATIBLE
	xprintk(KERN_INFO, "=== Do not sf_ctl_init_gpio_pins !!! ===\n");
	xprintk(KERN_INFO, "=== Do not sf_ctl_init_irq !!! ===\n");
#else
	/* Initialize the GPIO pins. */
	err = sf_ctl_init_gpio_pins(&pdev->dev);

	if (err) {
		xprintk(KERN_ERR, "sf_ctl_init_gpio_pins failed with %d.\n",
			err);
		return err;
	}

	err = sf_ctl_device_power(true);
	sf_ctl_device_reset();

	/* Initialize the interrupt callback. */
	err = sf_ctl_init_irq();

	if (err) {
		xprintk(KERN_ERR, "sf_ctl_init_irq failed with %d.\n", err);
		return err;
	}
#endif

	/* Initialize the input subsystem. */
	err = sf_ctl_init_input();

	if (err) {
		xprintk(KERN_ERR, "sf_ctl_init_input failed with %d.\n", err);
		free_irq(sf_ctl_dev.irq_num, (void *)&sf_ctl_dev);
		return err;
	}

	/* Register as a miscellaneous device. */
	err = misc_register(&sf_ctl_dev.miscdev);

	if (err) {
		xprintk(KERN_ERR, "misc_register(..) = %d.\n", err);
		input_unregister_device(sf_ctl_dev.input);
		free_irq(sf_ctl_dev.irq_num, (void *)&sf_ctl_dev);
		return err;
	}

	INIT_WORK(&sf_ctl_dev.work_queue, sf_ctl_device_event);
	xprintk(KERN_ERR, "%s leave\n", __func__);
	return err;
}

static int sf_remove(struct platform_device *pdev)
{
	if (sf_ctl_dev.input)
		input_unregister_device(sf_ctl_dev.input);

	if (sf_ctl_dev.irq_num >= 0)
		free_irq(sf_ctl_dev.irq_num, (void *)&sf_ctl_dev);

	misc_deregister(&sf_ctl_dev.miscdev);
#if ANDROID_WAKELOCK
	//wakeup_source_trash(&sf_ctl_dev.wakelock);
	wakeup_source_unregister(wake);
#endif
	return 0;
}

/***************add lanh 2017-06-10 start*********/
static int sunwave_set_irq_type(unsigned long type)
{
	irq_set_irq_type(sf_ctl_dev.irq_num,
			 type | IRQF_NO_SUSPEND | IRQF_ONESHOT);
	return 0;
}

#ifdef CONFIG_PM
static int sf_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	printk("'%s' enter", __func__);
	sunwave_set_irq_type(IRQF_TRIGGER_HIGH);
	suspend_flag = 1;
	return 0;
}

static int sf_resume(struct platform_device *pdev)
{
	printk("'%s' enter", __func__);
	sunwave_set_irq_type(IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND |
				IRQF_ONESHOT);
	suspend_flag = 0;
	return 0;
}
#endif

static struct platform_driver sf_driver = {
	.driver = {
			.name = "sunwave_fp",
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = sf_of_match,
#endif
			},
	.probe = sf_probe,
	.remove = sf_remove,

#ifdef CONFIG_PM
	.suspend = sf_suspend,
	.resume = sf_resume,
#endif
};

static int sf_ctl_init_gpio_pins(struct device *dev)
{
	int err = 0;
#ifdef CONFIG_OF
	struct device_node *of_node = dev->of_node;

	if (of_node == NULL) {
		xprintk(KERN_ERR, "fail to get of_node\n");
		return -1;
	}

	pdata->reset_gpio_number =
		of_get_named_gpio(of_node, "sunwave,reset-gpio", 0);

	if (pdata->reset_gpio_number < 0) {
		xprintk(KERN_ERR, "fail to get reset_gpio_number\n");
		return err;
	}

	pdata->irq_gpio_number =
		of_get_named_gpio(of_node, "sunwave,irq-gpio", 0);

	if (pdata->irq_gpio_number < 0) {
		xprintk(KERN_ERR, "fail to get irq_gpio_number\n");
		return err;
	}

	xprintk(KERN_INFO, "[sunwave] %s [irq=%d];[rst=%d]\n", __func__,
		pdata->irq_gpio_number, pdata->reset_gpio_number);
#else
	pdata->irq_gpio_number = SUNWAVE_IRQ_PIN;
	pdata->reset_gpio_number = SUNWAVE_RST_PIN;
#endif
	xprintk(KERN_INFO, "%s [irq=%d];[rst=%d]\n", __func__,
		pdata->irq_gpio_number, pdata->reset_gpio_number);
	err = gpio_request(pdata->reset_gpio_number, "sunwave_rst");

	if (err) {
		xprintk(KERN_ERR, "sunwave failed to request sunwave_rst\n");
		return err;
	}

	gpio_direction_output(pdata->reset_gpio_number, 1);
	err = gpio_request(pdata->irq_gpio_number, "sunwave_irq");

	if (err) {
		xprintk(KERN_ERR,
			"sunwave failed to request GPX1_3 for pdev irq detect\n");
		return err;
	}

	err = gpio_direction_input(pdata->irq_gpio_number);

	if (err) {
		xprintk(KERN_ERR, "sunwave failed to set  GPX1_3 to input\n");
		return err;
	}

	return err;
}

static int sf_ctl_deinit_gpio_pins(void)
{
	if (pdata) {
		gpio_free(pdata->reset_gpio_number);
		gpio_free(pdata->irq_gpio_number);
	}

	return 0;
}

static int sf_ctl_init_irq(void)
{
	int err = 0;

	sf_ctl_dev.irq_num = gpio_to_irq(pdata->irq_gpio_number);
	xprintk(KERN_INFO, "irq number is %d.\n", sf_ctl_dev.irq_num);
	/* Register interrupt callback. */
	err = request_irq(sf_ctl_dev.irq_num, sf_ctl_device_irq,
				IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND | IRQF_ONESHOT,
				"sf-irq", (void *)&sf_ctl_dev);

	if (err)
		xprintk(KERN_ERR, "request_irq(..) = %d.\n", err);

	enable_irq_wake(sf_ctl_dev.irq_num);
	return err;
}

static int sf_ctl_init_input(void)
{
	int err = 0;

	xprintk(KERN_DEBUG, "%s(..) enter.\n", __func__);
	sf_ctl_dev.input = input_allocate_device();

	if (!sf_ctl_dev.input) {
		xprintk(KERN_ERR, "input_allocate_device(..) failed.\n");
		return (-ENOMEM);
	}

	sf_ctl_dev.input->name = "sf-keys";
	__set_bit(EV_KEY, sf_ctl_dev.input->evbit);
	__set_bit(KEY_HOME, sf_ctl_dev.input->keybit);
	__set_bit(KEY_MENU, sf_ctl_dev.input->keybit);
	__set_bit(KEY_BACK, sf_ctl_dev.input->keybit);
	__set_bit(KEY_F11, sf_ctl_dev.input->keybit);
	__set_bit(KEY_ENTER, sf_ctl_dev.input->evbit);
	__set_bit(KEY_UP, sf_ctl_dev.input->keybit);
	__set_bit(KEY_LEFT, sf_ctl_dev.input->keybit);
	__set_bit(KEY_RIGHT, sf_ctl_dev.input->keybit);
	__set_bit(KEY_DOWN, sf_ctl_dev.input->keybit);
	__set_bit(KEY_WAKEUP, sf_ctl_dev.input->keybit);
	err = input_register_device(sf_ctl_dev.input);

	if (err) {
		xprintk(KERN_ERR, "input_register_device(..) = %d.\n", err);
		input_free_device(sf_ctl_dev.input);
		sf_ctl_dev.input = NULL;
		return (-ENODEV);
	}

	xprintk(KERN_DEBUG, "%s(..) leave.\n", __func__);
	return err;
}

static int __init sf_ctl_driver_init(void)
{
	int ret = 0;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);

	if (!pdata) {
		xprintk(KERN_ERR, "sf_ctl_driver_init malloc failed with.\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	ret = platform_driver_register(&sf_driver);

	if (ret < 0) {
		xprintk(KERN_INFO, "%s, Failed to register platform driver.\n",
			__func__);
		ret = -EINVAL;
		goto err_driver;
	}

	xprintk(KERN_INFO, "'%s' platform register success", __func__);
	xprintk(KERN_INFO,
		"sunwave fingerprint device control driver registered.\n");
	xprintk(KERN_INFO, "driver version: '%s'.\n", sf_ctl_get_version());
	return 0;

err_driver:
	kfree(pdata);
err_alloc:
	return ret;
}

static void __exit sf_ctl_driver_exit(void)
{
	platform_driver_unregister(&sf_driver);
	xprintk(KERN_INFO,
		"sunwave fingerprint device control driver released.\n");
}

module_init(sf_ctl_driver_init);
module_exit(sf_ctl_driver_exit);

MODULE_DESCRIPTION("The device control driver for Sunwave's fingerprint sensor.");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Langson L. <liangzh@sunwavecorp.com>");
