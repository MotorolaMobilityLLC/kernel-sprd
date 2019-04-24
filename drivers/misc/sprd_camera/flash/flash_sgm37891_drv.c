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
#include <linux/kernel.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "flash_drv.h"


#define FLASH_DRIVER_NAME "flash-sgm37891"
#define FLASH_GPIO_MAX 3

enum {
	GPIO_FLASH_1W,
	GPIO_FLASH_TORCH_EN,
	GPIO_FLASH_EN,
};

struct flash_driver_data {
	int gpio_tab[FLASH_GPIO_MAX];
	u32 torch_led_index;
};

static const char *const flash_gpio_names[FLASH_GPIO_MAX] = {
	"flash-1w-gpios", /* chip enable and level control */
	"flash-torch-en-gpios", /* for torch mode */
	"flash-en-gpios", /* for flash high light mode */
};


static int sprd_flash_sgm37891_init(void *drvd)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	gpio_id = drv_data->gpio_tab[GPIO_FLASH_1W];
	if (gpio_is_valid(gpio_id)) {
		ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
		if (ret)
			goto exit;
		udelay(300);
	}
exit:
	return ret;
}

static int sprd_flash_sgm37891_deinit(void *drvd)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	gpio_id = drv_data->gpio_tab[GPIO_FLASH_1W];
	if (gpio_is_valid(gpio_id)) {
		ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
		if (ret)
			goto exit;
	}
exit:
	return ret;
}

static int sprd_flash_sgm37891_open_torch(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_EN];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
			udelay(20);
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_TORCH_EN];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
			if (ret)
				goto exit;
		}
	}
exit:
	return ret;
}

static int sprd_flash_sgm37891_close_torch(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_TORCH_EN];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}

		gpio_id = drv_data->gpio_tab[GPIO_FLASH_1W];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
	}
exit:
	return ret;
}

static int sprd_flash_sgm37891_open_preflash(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	int gpio_torch_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_EN];
		gpio_torch_id = drv_data->gpio_tab[GPIO_FLASH_TORCH_EN];
		if (gpio_is_valid(gpio_id) && gpio_is_valid(gpio_torch_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
			ret = gpio_direction_output(gpio_torch_id,
						SPRD_FLASH_ON);
			udelay(20);
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
	}
exit:
	return ret;
}

static int sprd_flash_sgm37891_close_preflash(void *drvd, uint8_t idx)
{
	int ret = 0;

	return ret;
}

static int sprd_flash_sgm37891_open_highlight(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_EN];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
			if (ret)
				goto exit;
		}
	}
exit:
	return ret;
}

static int sprd_flash_sgm37891_close_highlight(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_EN];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
	}
exit:
	return ret;
}

static int sprd_flash_sgm37891_cfg(uint8_t idx, int gpio_id, uint16_t level)
{
	int i, ret = 0;

	ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
	if (ret)
		goto exit;
	udelay(45);

	/* for level > 5, output current should be same as that of level 0 */
	if (level > 5)
		level = 0;

	for (i = 0; i < level; i++) {
		ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
		if (ret)
			goto exit;
		udelay(45);
		ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
		if (ret)
			goto exit;
		udelay(45);
	}
	ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
	if (ret)
		goto exit;
	udelay(800);
exit:
	return ret;
}

static int sprd_flash_sgm37891_cfg_value_torch(void *drvd, uint8_t idx,
				struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;
	int gpio_id = 0;
	int ret = 0;

	if (!drv_data)
		return -EFAULT;

	idx = drv_data->torch_led_index;
	ret = sprd_flash_sgm37891_init(drv_data);
	element->index = 5;
	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_1W];
		if (gpio_is_valid(gpio_id))
			ret = sprd_flash_sgm37891_cfg(idx, gpio_id,
						element->index);
	}
	return ret;
}

static int sprd_flash_sgm37891_cfg_value_preflash(void *drvd, uint8_t idx,
				struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;
	int ret = 0;
	int gpio_id = 0;

	if (!drv_data)
		return -EFAULT;

	ret = sprd_flash_sgm37891_init(drv_data);
	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_1W];
		if (gpio_is_valid(gpio_id))
			ret = sprd_flash_sgm37891_cfg(idx, gpio_id,
						element->index);
	}
	return ret;
}

static int sprd_flash_sgm37891_cfg_value_highlight(void *drvd, uint8_t idx,
				struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;
	int ret = 0;
	int gpio_id = 0;

	if (!drv_data)
		return -EFAULT;

	ret = sprd_flash_sgm37891_init(drv_data);
	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[GPIO_FLASH_1W];
		if (gpio_is_valid(gpio_id))
			ret = sprd_flash_sgm37891_cfg(idx, gpio_id,
						element->index);
	}
	return ret;
}

static const struct of_device_id sgm37891_flash_of_match_table[] = {
	{.compatible = "sprd,flash-sgm37891"},
};

static const struct sprd_flash_driver_ops flash_gpio_ops = {
	.open_torch = sprd_flash_sgm37891_open_torch,
	.close_torch = sprd_flash_sgm37891_close_torch,
	.open_preflash = sprd_flash_sgm37891_open_preflash,
	.close_preflash = sprd_flash_sgm37891_close_preflash,
	.open_highlight = sprd_flash_sgm37891_open_highlight,
	.close_highlight = sprd_flash_sgm37891_close_highlight,
	.cfg_value_torch = sprd_flash_sgm37891_cfg_value_torch,
	.cfg_value_preflash = sprd_flash_sgm37891_cfg_value_preflash,
	.cfg_value_highlight = sprd_flash_sgm37891_cfg_value_highlight,
};

static int sprd_flash_sgm37891_probe(struct platform_device *pdev)
{
	int ret = 0;
	u32 gpio_node = 0;
	struct flash_driver_data *drv_data = NULL;
	int gpio[FLASH_GPIO_MAX];
	int j;

	if (IS_ERR_OR_NULL(pdev))
		return -EINVAL;

	if (!pdev->dev.of_node) {
		pr_err("no device node %s", __func__);
		return -ENODEV;
	}
	pr_info("flash-sgm37891 probe\n");

	ret = of_property_read_u32(pdev->dev.of_node,
						"flash-ic", &gpio_node);
	if (ret || gpio_node != 37891) {
		pr_err("no gpio flash\n");
		return -ENODEV;
	}

	drv_data = devm_kzalloc(&pdev->dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	pdev->dev.platform_data = (void *)drv_data;

	ret = of_property_read_u32(pdev->dev.of_node,
				"torch-led-idx", &drv_data->torch_led_index);
	if (ret)
		drv_data->torch_led_index = SPRD_FLASH_LED0;

	for (j = 0; j < FLASH_GPIO_MAX; j++) {
		gpio[j] = of_get_named_gpio(pdev->dev.of_node,
						flash_gpio_names[j], 0);
		if (gpio_is_valid(gpio[j])) {
			ret = devm_gpio_request(&pdev->dev,
						gpio[j],
						flash_gpio_names[j]);

			if (ret) {
				pr_err("flash gpio err\n");
				goto exit;
			}

			ret = gpio_direction_output(gpio[j], SPRD_FLASH_OFF);

			if (ret) {
				pr_err("flash gpio output err\n");
				goto exit;
			}
		}
	}

	memcpy((void *)drv_data->gpio_tab, (void *)gpio, sizeof(gpio));

	ret = sprd_flash_sgm37891_init(drv_data);
	if (ret)
		goto exit;
	ret = sprd_flash_register(&flash_gpio_ops, drv_data, SPRD_FLASH_REAR);
	if (ret)
		goto exit;
exit:
	return ret;
}

static int sprd_flash_sgm37891_remove(struct platform_device *pdev)
{
	int ret = 0;

	ret = sprd_flash_sgm37891_deinit(pdev->dev.platform_data);

	if (ret)
		pr_err("flash deinit err\n");

	return ret;
}

static struct platform_driver sprd_flash_sgm37891_driver = {
	.probe = sprd_flash_sgm37891_probe,
	.remove = sprd_flash_sgm37891_remove,
	.driver = {
		.name = FLASH_DRIVER_NAME,
		.of_match_table = of_match_ptr(sgm37891_flash_of_match_table),
	},
};

module_platform_driver(sprd_flash_sgm37891_driver);
