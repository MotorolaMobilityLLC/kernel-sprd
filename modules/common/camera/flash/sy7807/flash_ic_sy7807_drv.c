/*
 * Copyright (C) 2021-2022 UNISOC Communications Inc.
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

#include "sprd_img.h"
#include "flash_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "FLASH_SY7807: %d %d %s : " fmt, current->pid, __LINE__, __func__

#define FLASH_IC_DRIVER_NAME       "sprd_sy7807"
#define FLASH_PWM_NAME             "flash_sy7807"
#define TORCH_PWM_NAME             "torch_sy7807"

/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */

#define PWM_CLK_PARENT	"clk_parent"
#define PWM_CLK		"clk_pwm"
#define PWM_DUTY	0x8
#define PWM_REGS_SHIFT	5
#define NUM_PWM		4


struct flash_driver_data {
	struct pwm_device *pwm_chip0;
	struct pwm_device *pwm_chip1;
	int torch_led_index;
};


int pwm_set_config( struct pwm_device *pwm, int duty_cycle)
{	int res;
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

	pr_info("pwm_set_config:chip->ops->config 00000");	


	return res;
}

static int sprd_flash_ic_open_torch(void *drvd, uint8_t idx)
{
	struct pwm_device *pwm;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (IS_ERR(drv_data))
		return -EFAULT;

	if (IS_ERR(drv_data->pwm_chip1))
		return -EFAULT;

	pwm = drv_data->pwm_chip1;
	if (IS_ERR(pwm)) {
		return -EFAULT;
	}
	idx |= 1;
	if (SPRD_FLASH_LED0 & idx){
		pr_info("open  torch :pwm , torch_led_index:%d",drv_data->torch_led_index);
		pwm_set_config( pwm, 40);
	}
	return 0;
}

static int sprd_flash_ic_close_torch(void *drvd, uint8_t idx)
{
	struct pwm_device *pwm;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (IS_ERR(drv_data))
		return -EFAULT;


	if (IS_ERR(drv_data->pwm_chip1))
		return -EFAULT;


	pwm = drv_data->pwm_chip1;
	if (IS_ERR(pwm)) {
		return -EFAULT;
	}
	idx |= 1;
	if (SPRD_FLASH_LED0 & idx){
		pr_info("close torch :pwm");
		pwm_set_config(pwm, 0);
	}

	return 0;
}


static int sprd_flash_ic_open_preflash(void *drvd, uint8_t idx)
{
	struct pwm_device *pwm;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (IS_ERR(drv_data))
		return -EFAULT;
	if (IS_ERR(drv_data->pwm_chip1))
		return -EFAULT;

	pwm = drv_data->pwm_chip0;
	if (IS_ERR(pwm)) {
	
	return -EFAULT;
	}	
	if (SPRD_FLASH_LED0 & idx){
		pr_info("open  preflash :pwm , torch_led_index:%d",drv_data->torch_led_index);	
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
	if (IS_ERR(drv_data->pwm_chip1))
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
		pwm_set_config(pwm, drv_data->torch_led_index);
		pr_info("open  highlight :pwm , torch_led_index:%d",drv_data->torch_led_index);
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
		pwm_set_config(pwm, 0);
	}
	return 0;
}

static int sprd_flash_ic_cfg_value_torch(void *drvd, uint8_t idx,
					  struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;
	if (SPRD_FLASH_LED0 & idx){
		drv_data->torch_led_index = (int) (5 * element->index);
		pr_info("cfg_value  torch :pwm , element:%d,torch_led_index:%d",element->index,drv_data->torch_led_index);
	}
	return 0;

}

static int sprd_flash_ic_cfg_value_preflash(void *drvd, uint8_t idx,
					  struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;
	if (SPRD_FLASH_LED0 & idx){
		drv_data->torch_led_index = (int) (1.7 * element->index);
		pr_info("cfg_value  preflash :pwm , element:%d,torch_led_index:%d",element->index,drv_data->torch_led_index);
	}
	return 0;
}

static int sprd_flash_ic_cfg_value_highlight(void *drvd, uint8_t idx,
					   struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (SPRD_FLASH_LED0 & idx){
		drv_data->torch_led_index = (int) (3.4 * element->index);
		pr_info("cfg_value  highlight :pwm , element:%d,torch_led_index:%d",element->index,drv_data->torch_led_index);
	}

	return 0;
}

static const struct of_device_id sprd_flash_ic_of_match_table[] = {
	{.compatible = "sprd,pwm-sy7807"},
	{/* MUST end with empty struct */},
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
	int ret;

	pr_info("sy7808-sprd_flash_ic_driver_probe     E");
	if (IS_ERR(pdev))
		return -EINVAL;
	
	//pr_err("---LYS--sy7808-sprd_flash_ic_driver_probe     E");
	drv_data = devm_kzalloc(&pdev->dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	platform_set_drvdata(pdev, drv_data);
#if 0
    np = of_parse_phandle(pdev->dev.of_node, "pwm", 0);
	if (IS_ERR_OR_NULL(np)) {
      dev_err(&pdev->dev, "get pwm device failed\n");
	  goto exit;
	}
	
	drv_data->pwm_chip = (struct sprd_pwm_chip *)of_find_device_by_node(np);
#endif

	drv_data->pwm_chip0 = devm_pwm_get(&pdev->dev,"pwm0");
	if (IS_ERR(drv_data->pwm_chip0)) {
      dev_err(&pdev->dev, "get pwm device0 failed\n");
	  goto exit;
	}

	drv_data->pwm_chip1 = devm_pwm_get(&pdev->dev,"pwm1");
	if (IS_ERR(drv_data->pwm_chip1)) {
      dev_err(&pdev->dev, "get pwm device1 failed\n");
	  goto exit;
	}

	ret = sprd_flash_register(&flash_ic_ops, drv_data, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "register flash driver failed\n");
		goto exit;
	}
	
exit:
	pr_err("sy7808-sprd_flash_ic_driver_probe       X");
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

MODULE_DESCRIPTION("Sprd sy7807 Flash Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Victor <hisense.com>");


