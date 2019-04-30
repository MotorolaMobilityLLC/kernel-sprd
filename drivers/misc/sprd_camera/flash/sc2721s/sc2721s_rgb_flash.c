/*
 * Copyright (C) 2019 Unisoc Communications Inc.
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
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/vmalloc.h>
#include <linux/sprd_otp.h>

#include "flash_drv.h"
#include "sc2721s_reg.h"

/* #define FLASH_RGB_MAX 3 */

/* Structure Definitions */

#define SC2721S_BLTC_CTRL			0x0000
#define SC2721S_BLTC_R_PRESCL			0x0004
#define SC2721S_BLTC_G_PRESCL			0x0014
#define SC2721S_BLTC_B_PRESCL			0x0024
#define SC2721S_BLTC_DUTY_OFFSET		0x0004

#define SC2721S_R_RUN	(1 << 0)
#define SC2721S_R_TYPE	(1 << 1)
#define SC2721S_G_RUN	(1 << 4)
#define SC2721S_G_TYPE	(1 << 5)
#define SC2721S_B_RUN	(1 << 8)
#define SC2721S_B_TYPE	(1 << 9)

#define SC2721S_PWM_MOD_COUNTER 0xFF

struct flash_driver_data {
	spinlock_t slock;
	void *priv;
	unsigned int value;
	unsigned long bltc_addr;
	unsigned long sprd_bltc_base_addr;
	u32 torch_led_index;
};

enum sprd_rgb_leds_type {
	SPRD_LED_TYPE_R = 0,
	SPRD_LED_TYPE_G,
	SPRD_LED_TYPE_B,
	SPRD_LED_TYPE_TOTAL
};

static struct regmap *sprd_rgb_flash_handle;

static void sprd_sc2721s_leds_bltc_rgb_set_brightness(
				struct flash_driver_data *drv_data)
{
	unsigned long brightness = drv_data->value;
	unsigned long pwm_duty;

	pwm_duty = brightness;
	if (pwm_duty > SC2721S_PWM_MOD_COUNTER)
		pwm_duty = SC2721S_PWM_MOD_COUNTER;
	regmap_update_bits(sprd_rgb_flash_handle, drv_data->bltc_addr, 0xffff,
			(pwm_duty << 8) | SC2721S_PWM_MOD_COUNTER);
}

static void sprd_sc2721s_bltc_rgb_init(struct flash_driver_data *drv_data)
{
	/*ARM_MODULE_EN-enable pclk */
	regmap_update_bits(sprd_rgb_flash_handle,
				ANA_REG_GLB_MODULE_EN0,
				BIT_ANA_BLTC_EN, BIT_ANA_BLTC_EN);
	/*RTC_CLK_EN-enable rtc */
	regmap_update_bits(sprd_rgb_flash_handle,
				ANA_REG_GLB_RTC_CLK_EN0,
				BIT_RTC_BLTC_EN, BIT_RTC_BLTC_EN);
	/*SW POWERDOWN DISABLE */
	regmap_update_bits(sprd_rgb_flash_handle,
				ANA_REG_GLB_RGB_CTRL, BIT_RGB_PD_SW,
				~(unsigned int)(BIT_RGB_PD_SW));
	/*CURRENT CONTROL DEFAULT */
	regmap_update_bits(sprd_rgb_flash_handle,
				ANA_REG_GLB_RGB_CTRL,
				BITS_RGB_V(0x1f), ~BITS_RGB_V(0x1f));
}

static void sprd_sc2721s_leds_bltc_rgb_enable(
				struct flash_driver_data *drv_data)
{
	sprd_sc2721s_bltc_rgb_init(drv_data);

	regmap_update_bits(sprd_rgb_flash_handle,
			drv_data->sprd_bltc_base_addr + SC2721S_BLTC_CTRL,
			SC2721S_R_RUN | SC2721S_R_TYPE,
				SC2721S_R_RUN | SC2721S_R_TYPE);
	drv_data->bltc_addr = drv_data->sprd_bltc_base_addr
			+ SC2721S_BLTC_R_PRESCL + SC2721S_BLTC_DUTY_OFFSET;
	sprd_sc2721s_leds_bltc_rgb_set_brightness(drv_data);

	regmap_update_bits(sprd_rgb_flash_handle,
			drv_data->sprd_bltc_base_addr + SC2721S_BLTC_CTRL,
			SC2721S_G_RUN | SC2721S_G_TYPE,
				SC2721S_G_RUN | SC2721S_G_TYPE);
	drv_data->bltc_addr = drv_data->sprd_bltc_base_addr
			+ SC2721S_BLTC_G_PRESCL + SC2721S_BLTC_DUTY_OFFSET;
	sprd_sc2721s_leds_bltc_rgb_set_brightness(drv_data);

	regmap_update_bits(sprd_rgb_flash_handle,
			drv_data->sprd_bltc_base_addr + SC2721S_BLTC_CTRL,
			SC2721S_B_RUN | SC2721S_B_TYPE,
				SC2721S_B_RUN | SC2721S_B_TYPE);
	drv_data->bltc_addr = drv_data->sprd_bltc_base_addr
			+ SC2721S_BLTC_B_PRESCL + SC2721S_BLTC_DUTY_OFFSET;
	sprd_sc2721s_leds_bltc_rgb_set_brightness(drv_data);

	pr_info("%s\n", __func__);
}

static void sprd_sc2721s_leds_bltc_rgb_disable(
				struct flash_driver_data *drv_data)
{
	regmap_update_bits(sprd_rgb_flash_handle,
			drv_data->sprd_bltc_base_addr + SC2721S_BLTC_CTRL,
			SC2721S_R_RUN, ~SC2721S_R_RUN);
	regmap_update_bits(sprd_rgb_flash_handle,
			drv_data->sprd_bltc_base_addr + SC2721S_BLTC_CTRL,
			SC2721S_G_RUN, ~SC2721S_G_RUN);
	regmap_update_bits(sprd_rgb_flash_handle,
			drv_data->sprd_bltc_base_addr + SC2721S_BLTC_CTRL,
			SC2721S_B_RUN, ~SC2721S_B_RUN);
	pr_info("sprd_leds_bltc_rgb_disable\n");
}

/* API Function Implementation */
static int sprd_flash_rgb_open_torch(void *drvd, uint8_t idx)
{
	int ret = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx)
		sprd_sc2721s_leds_bltc_rgb_enable(drv_data);

	if (SPRD_FLASH_LED1 & idx)
		sprd_sc2721s_leds_bltc_rgb_enable(drv_data);

	return ret;
}

static int sprd_flash_rgb_close_torch(void *drvd, uint8_t idx)
{
	int ret = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx)
		sprd_sc2721s_leds_bltc_rgb_disable(drv_data);

	if (SPRD_FLASH_LED1 & idx)
		sprd_sc2721s_leds_bltc_rgb_disable(drv_data);

	return ret;
}

static int sprd_flash_rgb_open_preflash(void *drvd, uint8_t idx)
{
	int ret = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		sprd_sc2721s_leds_bltc_rgb_enable(drv_data);

	if (SPRD_FLASH_LED1 & idx)
		sprd_sc2721s_leds_bltc_rgb_enable(drv_data);

	return ret;
}

static int sprd_flash_rgb_close_preflash(void *drvd, uint8_t idx)
{
	int ret = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		sprd_sc2721s_leds_bltc_rgb_disable(drv_data);

	if (SPRD_FLASH_LED1 & idx)
		sprd_sc2721s_leds_bltc_rgb_disable(drv_data);

	return ret;
}

static int sprd_flash_rgb_open_highlight(void *drvd, uint8_t idx)
{
	int ret = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		sprd_sc2721s_leds_bltc_rgb_enable(drv_data);

	if (SPRD_FLASH_LED1 & idx)
		sprd_sc2721s_leds_bltc_rgb_enable(drv_data);

	return ret;
}

static int sprd_flash_rgb_close_highlight(void *drvd, uint8_t idx)
{
	int ret = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		sprd_sc2721s_leds_bltc_rgb_disable(drv_data);

	if (SPRD_FLASH_LED1 & idx)
		sprd_sc2721s_leds_bltc_rgb_disable(drv_data);

	return ret;
}

static int sprd_flash_rgb_cfg_value_preflash(void *drvd, uint8_t idx,
					  struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		sprd_sc2721s_leds_bltc_rgb_enable(drv_data);

	if (SPRD_FLASH_LED1 & idx)
		sprd_sc2721s_leds_bltc_rgb_enable(drv_data);

	return 0;
}

static int sprd_flash_rgb_cfg_value_highlight(void *drvd, uint8_t idx,
					   struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		sprd_sc2721s_leds_bltc_rgb_enable(drv_data);

	if (SPRD_FLASH_LED1 & idx)
		sprd_sc2721s_leds_bltc_rgb_enable(drv_data);

	return 0;
}

static int sprd_flash_rgb_cfg_value_torch(void *drvd, uint8_t idx,
					   struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx)
		sprd_sc2721s_leds_bltc_rgb_enable(drv_data);

	if (SPRD_FLASH_LED1 & idx)
		sprd_sc2721s_leds_bltc_rgb_enable(drv_data);

	return 0;
}

static const struct sprd_flash_driver_ops flash_rgb_ops = {
	.open_torch = sprd_flash_rgb_open_torch,
	.close_torch = sprd_flash_rgb_close_torch,
	.open_preflash = sprd_flash_rgb_open_preflash,
	.close_preflash = sprd_flash_rgb_close_preflash,
	.open_highlight = sprd_flash_rgb_open_highlight,
	.close_highlight = sprd_flash_rgb_close_highlight,
	.cfg_value_preflash = sprd_flash_rgb_cfg_value_preflash,
	.cfg_value_highlight = sprd_flash_rgb_cfg_value_highlight,
	.cfg_value_torch = sprd_flash_rgb_cfg_value_torch,
};

static const struct of_device_id sprd_flash_rgb_of_match[] = {
	{ .compatible = "sprd,flash-front-rgb", },
	{},
};
static int sprd_flash_rgb_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct flash_driver_data *drv_data = NULL;
	struct device_node *node = pdev->dev.of_node;
	unsigned int pmic_base_addr;

	if (IS_ERR(pdev))
		return -EINVAL;

	drv_data = devm_kzalloc(&pdev->dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	pdev->dev.platform_data = (void *)drv_data;

	sprd_rgb_flash_handle = dev_get_regmap(pdev->dev.parent, NULL);
	if (!sprd_rgb_flash_handle)
		panic("%s :NULL rgb flash parent property for vibrator!",
					__func__);

	ret = of_property_read_u32(node, "reg", &pmic_base_addr);
	if (ret) {
		pr_info("reg read dts error!\n");
		goto exit;
	} else {
		drv_data->sprd_bltc_base_addr = pmic_base_addr;
		drv_data->value = SC2721S_PWM_MOD_COUNTER;
		regmap_update_bits(sprd_rgb_flash_handle,
				ANA_REG_GLB_SOFT_RST0,
				BIT_BLTC_SOFT_RST,
				~(unsigned int)(BIT_BLTC_SOFT_RST));
	}

	ret = of_property_read_u32(node,
				"torch-led-idx", &drv_data->torch_led_index);
	if (ret)
		drv_data->torch_led_index = SPRD_FLASH_LED0;

	ret = sprd_flash_register(&flash_rgb_ops, drv_data,
					SPRD_FLASH_FRONT);
	if (ret < 0)
		goto exit;
exit:
	return ret;
}

static int sprd_flash_rgb_remove(struct platform_device *pdev)
{
	return 0;
}
static struct platform_driver sprd_flash_rgb_driver = {
	.probe = sprd_flash_rgb_probe,
	.remove = sprd_flash_rgb_remove,
	.driver = {
		.name = "flash-front-rgb",
		.of_match_table = of_match_ptr(sprd_flash_rgb_of_match),
	},
};

module_platform_driver(sprd_flash_rgb_driver);
