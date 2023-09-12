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
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/pwm.h>
#include <linux/clk.h>
#include <linux/pm_wakeup.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio/consumer.h>

#include "sprd_img.h"
#include "flash_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "FLASH_OCP8135B: %d %d %s : " fmt, current->pid, __LINE__, __func__

#define FLASH_IC_DRIVER_NAME       "sprd_ocp8135b"
#define MAX_INDEX                  9

static int gpio_enf;
static struct pinctrl *p = NULL;

struct flash_driver_data {
	struct pwm_device *pwm_chip0;
	struct pwm_device *pwm_chip1;
	int torch_led_index;
	struct wakeup_source *pd_wake_lock;
};

static int sprd_pin_set_gpio(struct pwm_device *pwm)
{
    struct pinctrl_state *pinctrl_state;
    char *s = "gpio_32";
    int ret;

    pr_info("this is flash ocp8135b gpio pinctrl -----\n");

    /*set function to gpio*/

    pinctrl_state = pinctrl_lookup_state(p, s);

    pr_info("ocp8135b find dts define gpio");
    ret =  pinctrl_select_state(p, pinctrl_state);
    pr_info("pinctrl_select_state -----\n");
    return 0;
}

static int pull_up_gpio(bool enable)
{
    struct gpio_desc *desc;
    int ret1, gpio_num;
    /*拉高gpio32， gpio_num=  base+ 32*/  /*cat d/gpio看下9230的base是多少*/
    gpio_num = 96;
    desc = gpio_to_desc(gpio_num);
    ret1 = gpio_request(gpio_num, "flash-labels");      //使能GPIO
    ret1 = gpio_direction_output(gpio_num, 1);      //配置为输出
    gpiod_set_value(desc,enable);         //设置gpio高电平状态
    return ret1;
}

static int sprd_pin_set_pwm(struct pwm_device *pwm)
{
    struct pinctrl_state *pinctrl_state;
    char *s = "rfctl_32";
    int ret;

    pr_info("ocp8135b flash gpio to pwma pinctrl -----\n");

    /*set function to pwma*/

    pinctrl_state = pinctrl_lookup_state(p, s);
    pr_info("ocp8135b find dts define");

    ret =  pinctrl_select_state(p, pinctrl_state);
    pr_info("pinctrl_select_state -----\n");
    return 0;
}

int pwm_set_config( struct pwm_device *pwm, int duty_cycle)
{
	int duty_ns, period_ns;
	struct pwm_state state;

	pwm_get_state(pwm, &state);
	period_ns = state.period;
	duty_ns = duty_cycle * period_ns / 100;
	state.duty_cycle = duty_ns;

	if (duty_ns > 0) {
		state.enabled= true ;
	} else{
		state.enabled= false ;
	}
	pwm_apply_state(pwm, &state);

	pr_info("pwm_set_config,period_ns = %d,duty_cycle = %d",state.period,state.duty_cycle);	


	return 0;
}

static int sprd_flash_ic_open_torch(void *drvd, uint8_t idx)
{
	struct pwm_device *pwm;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (IS_ERR(drv_data))
		return -EFAULT;

	if (IS_ERR(drv_data->pwm_chip0))
		return -EFAULT;

	pwm = drv_data->pwm_chip0;
	if (IS_ERR(pwm)) {
		return -EFAULT;
	}
	idx |= 1;
	if (SPRD_FLASH_LED0 & idx){
		pr_info("open  torch :pwm , torch_led_index:%d",drv_data->torch_led_index);

		sprd_pin_set_gpio(pwm);
		pull_up_gpio(1);
		mdelay(5);
		pull_up_gpio(0);
		sprd_pin_set_pwm(pwm);
		pwm_set_config( pwm, 40);
	}
	__pm_stay_awake(drv_data->pd_wake_lock);
	return 0;
}

static int sprd_flash_ic_close_torch(void *drvd, uint8_t idx)
{
	struct pwm_device *pwm;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (IS_ERR(drv_data))
		return -EFAULT;


	if (IS_ERR(drv_data->pwm_chip0))
		return -EFAULT;


	pwm = drv_data->pwm_chip0;
	if (IS_ERR(pwm)) {
		return -EFAULT;
	}
	idx |= 1;
	if (SPRD_FLASH_LED0 & idx){
		pr_info("close torch :pwm");
		pwm_set_config(pwm, 0);
	}

	__pm_relax(drv_data->pd_wake_lock);
	return 0;
}


static int sprd_flash_ic_open_preflash(void *drvd, uint8_t idx)
{
	struct pwm_device *pwm;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (IS_ERR(drv_data))
		return -EFAULT;
	if (IS_ERR(drv_data->pwm_chip0))
		return -EFAULT;

	pwm = drv_data->pwm_chip0;
	if (IS_ERR(pwm)) {
	
	return -EFAULT;
	}	
	if (SPRD_FLASH_LED0 & idx){
		pr_info("open  preflash :pwm , torch_led_index:%d",drv_data->torch_led_index);	
		sprd_pin_set_gpio(pwm);
		pull_up_gpio(1);
		mdelay(5);
		pull_up_gpio(0);
		sprd_pin_set_pwm(pwm);
		pwm_set_config( pwm, drv_data->torch_led_index);
	}
	return 0;
}

static int sprd_flash_ic_close_preflash(void *drvd, uint8_t idx)
{

	struct pwm_device *pwm;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (IS_ERR(drv_data))
		return -EFAULT;
	if (IS_ERR(drv_data->pwm_chip0))
		return -EFAULT;

	pwm = drv_data->pwm_chip0;
        if (IS_ERR(pwm)) {

	return -EFAULT;
	}
	if (SPRD_FLASH_LED0 & idx){
		pr_info("close preflash :pwm");
		pwm_set_config(pwm, 0);
	}

	return 0;
}

static int sprd_flash_ic_open_highlight(void *drvd, uint8_t idx)
{
	struct pwm_device *pwm;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;
	if (IS_ERR(drv_data))
		return -EFAULT;


	if (IS_ERR(drv_data->pwm_chip0))
		return -EFAULT;


	pwm = drv_data->pwm_chip0;
	if (IS_ERR(pwm)) {
		return -EFAULT;
	}
	if (SPRD_FLASH_LED0 & idx){
		pr_info("open  highlight :pwm , torch_led_index:%d",drv_data->torch_led_index);
		pwm_set_config(pwm, drv_data->torch_led_index);

		gpio_direction_output(gpio_enf, SPRD_FLASH_ON);
		mdelay(400);
		gpio_direction_output(gpio_enf, SPRD_FLASH_OFF);

		pwm_set_config(pwm, 0);
	}

	return 0;
}

static int sprd_flash_ic_close_highlight(void *drvd, uint8_t idx)
{
	struct pwm_device *pwm;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (IS_ERR(drv_data))
		return -EFAULT;


	if (IS_ERR(drv_data->pwm_chip0))
		return -EFAULT;


	pwm = drv_data->pwm_chip0;
        if (IS_ERR(pwm)) {
        return -EFAULT;
	}
	if (SPRD_FLASH_LED0 & idx){
		pr_info("close highlight :pwm");
        gpio_direction_output(gpio_enf, SPRD_FLASH_OFF);
		pwm_set_config(pwm, 0);
	}
	return 0;
}

static int sprd_flash_ic_cfg_value_torch(void *drvd, uint8_t idx,
					  struct sprd_flash_element *element)
{
/*
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;
	if (SPRD_FLASH_LED0 & idx){
		drv_data->torch_led_index = (int) (5 * element->index);
		pr_info("cfg_value  torch :pwm , element:%d,torch_led_index:%d",element->index,drv_data->torch_led_index);
	}
*/
	return 0;

}

static int sprd_flash_ic_cfg_value_preflash(void *drvd, uint8_t idx,
					  struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;
	if (SPRD_FLASH_LED0 & idx){
		if(element->index > MAX_INDEX)
			element->index = MAX_INDEX;
		drv_data->torch_led_index = (int) (10 * (element->index + 1));
		pr_info("cfg_value  preflash :pwm , element:%d,torch_led_index:%d",element->index,drv_data->torch_led_index);
	}
	return 0;
}

static int sprd_flash_ic_cfg_value_highlight(void *drvd, uint8_t idx,
					   struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (SPRD_FLASH_LED0 & idx){
		if(element->index > MAX_INDEX)
			element->index = MAX_INDEX;
		drv_data->torch_led_index = (int) (10 * (element->index + 1));
		pr_info("cfg_value  highlight :pwm , element:%d,highlight_led_index:%d",element->index,drv_data->torch_led_index);
	}

	return 0;
}

static const struct of_device_id sprd_flash_ic_of_match_table[] = {
	{.compatible = "sprd,pwm-ocp8135b"},
};


static const struct sprd_flash_driver_ops flash_ic_ops = {
	.open_torch = sprd_flash_ic_open_torch,
	.close_torch = sprd_flash_ic_close_torch,
	.open_preflash = sprd_flash_ic_open_preflash,
	.close_preflash = sprd_flash_ic_close_preflash,
	.open_highlight = sprd_flash_ic_open_highlight,
	.close_highlight = sprd_flash_ic_close_highlight,
	.cfg_value_torch = sprd_flash_ic_cfg_value_torch,
	.cfg_value_preflash = sprd_flash_ic_cfg_value_preflash,
	.cfg_value_highlight = sprd_flash_ic_cfg_value_highlight,

};


static int sprd_flash_ic_driver_probe(struct platform_device *pdev)
{
	struct flash_driver_data *drv_data ;
	int ret = 0;

	pr_info("ocp8135b-sprd_flash_ic_driver_probe     E");
	if (IS_ERR(pdev))
		return -EINVAL;

	drv_data = devm_kzalloc(&pdev->dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	platform_set_drvdata(pdev, drv_data);

	p = devm_pinctrl_get(&pdev->dev);
	if (p == NULL) {
		pr_err("[DTS]pinctrl parsing failed\n");
		goto exit;
	}

	drv_data->pwm_chip0 = devm_pwm_get(&pdev->dev,"pwm0");
	if (IS_ERR(drv_data->pwm_chip0)) {
      dev_err(&pdev->dev, "get pwm device0 failed\n");
	  goto exit;
	}

	gpio_enf = of_get_named_gpio(pdev->dev.of_node,
				    "flash-enf-gpios", 0);
	if (gpio_is_valid(gpio_enf)) {
		ret = devm_gpio_request(&pdev->dev,
					gpio_enf, "flash-enf-gpios");
		if (ret) {
			pr_err("flash gpio err\n");
			goto exit;
		}

		ret = gpio_direction_output(gpio_enf, SPRD_FLASH_OFF);
		if (ret) {
			pr_err("flash gpio output err\n");
			goto exit;
		}
	}

	ret = sprd_flash_register(&flash_ic_ops, drv_data, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "register flash driver failed\n");
		goto exit;
	}

    drv_data->pd_wake_lock = wakeup_source_create("flash_pwm");
    wakeup_source_add(drv_data->pd_wake_lock);

	//wakeup_source_init(&drv_data->pd_wake_lock, "flash_pwm");

exit:
	pr_err("ocp8135b-sprd_flash_ic_driver_probe       X");
	return ret;

}
static int sprd_flash_ic_driver_remove(struct platform_device *pdev)
{
	return 0;
}


static struct platform_driver sprd_flash_ic_driver = {

	.driver = {
		.name = FLASH_IC_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = sprd_flash_ic_of_match_table,
	},
	.probe = sprd_flash_ic_driver_probe,
	.remove = sprd_flash_ic_driver_remove,
};


module_platform_driver(sprd_flash_ic_driver);

MODULE_DESCRIPTION("Sprd ocp8135b Flash Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Victor <hisense.com>");


