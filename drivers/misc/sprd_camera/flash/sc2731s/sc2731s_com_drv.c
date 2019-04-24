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
#include "sc2731s_reg.h"

#define FLASH_GPIO_MAX 2

/* Structure Definitions */

struct flash_driver_data {
	struct regmap *reg_map;
	spinlock_t slock;
	int gpio_tab[FLASH_GPIO_MAX];
	bool pmic_flash_exchange;
	void *priv;
	u32 torch_led_index;
};

/* Static Variables Definitions */

static const char *const flash_gpio_names[FLASH_GPIO_MAX] = {
	"flash0-gpios",
	"flash1-gpios",
};

static void sc2731s_init(struct flash_driver_data *drv_data)
{
	/* safe time */
	regmap_update_bits(drv_data->reg_map,
		SW_SAFE_TIME, FLASH0_CTRL_SAFE_TIME, 1);
	regmap_update_bits(drv_data->reg_map,
		SW_SAFE_TIME, FLASH1_CTRL_SAFE_TIME, 1 << 2);

	/* flash0 */
	regmap_update_bits(drv_data->reg_map,
		FLASH0_MODE_TIME, FLASH0_CTRL_PRE_TIME, 4);
	regmap_update_bits(drv_data->reg_map,
		FLASH0_MODE_TIME, FLASH0_CTRL_REAL_TIME, 20 << 5);
	regmap_update_bits(drv_data->reg_map,
		FLASH0_MODE_TIME, FLASH0_CTRL_TORCH_TIME, 5 << 10);

	/* flash1 */
	regmap_update_bits(drv_data->reg_map,
		FLASH1_MODE_TIME, FLASH1_CTRL_PRE_TIME, 6);
	regmap_update_bits(drv_data->reg_map,
		FLASH1_MODE_TIME, FLASH1_CTRL_REAL_TIME, 30 << 5);
	regmap_update_bits(drv_data->reg_map,
		FLASH1_MODE_TIME, FLASH1_CTRL_TORCH_TIME, 19 << 10);
}

#define BITSINDEX(b, o)  ((b) * 16 + (o))

static void sprd2731_flash_cal(void *drvd)
{
#ifdef CONFIG_OTP_SPRD_PMIC_EFUSE
	unsigned int cal;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!sprd_pmic_efuse_bits_read(0, 1)) {
		pr_err("sprd2731:have no efuse data\n");
		return;
	}

	/* block13,bit[5:0], flash2 cal */
	cal = sprd_pmic_efuse_bits_read(BITSINDEX(13, 0), 6);

	pr_info("sprd2731:cal1 = 0x%x", cal);
	regmap_update_bits(drv_data->reg_map,
		RG_BST_RESERVED, 0x3f << 2, cal << 2);

	/* block15[12:7] flash1 */
	cal = sprd_pmic_efuse_bits_read(BITSINDEX(15, 7), 6);
	pr_info("sprd2731:cal2 = 0x%x", cal);
	regmap_update_bits(drv_data->reg_map,
		RG_FLASH_IBTRIM, 0x3f, cal);
#endif
}

/* API Function Implementation */

static int sprd_flash_sc2731s0_open_torch(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map,
			SW_LED_EN, TORCH0_CTRL_EN, TORCH0_CTRL_EN);

	return 0;
}

static int sprd_flash_sc2731s0_close_torch(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map,
			SW_LED_EN, TORCH0_CTRL_EN,
			~(unsigned int)TORCH0_CTRL_EN);

	return 0;
}

static int sprd_flash_sc2731s0_open_preflash(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map,
			SW_LED_EN, FLASH0_CTRL_EN, FLASH0_CTRL_EN);

	return 0;
}

static int sprd_flash_sc2731s0_close_preflash(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map,
			SW_LED_EN, FLASH0_CTRL_EN,
			~(unsigned int)FLASH0_CTRL_EN);

	return 0;
}

static int sprd_flash_sc2731s0_open_highlight(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[0];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int sprd_flash_sc2731s0_close_highlight(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[0];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int sprd_flash_sc2731s0_cfg_value_preflash(void *drvd, uint8_t idx,
					  struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map, FLASH0_MODE_TIME,
			FLASH0_CTRL_PRE_TIME, element->index);

	return 0;
}

static int sprd_flash_sc2731s0_cfg_value_highlight(void *drvd, uint8_t idx,
					   struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map, FLASH0_MODE_TIME,
			FLASH0_CTRL_REAL_TIME, element->index << 5);

	return 0;
}

static const struct sprd_flash_driver_ops flash_sc2731s0_ops = {
	.open_torch = sprd_flash_sc2731s0_open_torch,
	.close_torch = sprd_flash_sc2731s0_close_torch,
	.open_preflash = sprd_flash_sc2731s0_open_preflash,
	.close_preflash = sprd_flash_sc2731s0_close_preflash,
	.open_highlight = sprd_flash_sc2731s0_open_highlight,
	.close_highlight = sprd_flash_sc2731s0_close_highlight,
	.cfg_value_preflash = sprd_flash_sc2731s0_cfg_value_preflash,
	.cfg_value_highlight = sprd_flash_sc2731s0_cfg_value_highlight,
};

static int sprd_flash_sc2731s1_open_torch(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map,
			SW_LED_EN, TORCH1_CTRL_EN, TORCH1_CTRL_EN);

	return 0;
}

static int sprd_flash_sc2731s1_close_torch(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map,
			SW_LED_EN, TORCH1_CTRL_EN,
			~(unsigned int)TORCH1_CTRL_EN);

	return 0;
}

static int sprd_flash_sc2731s1_open_preflash(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map,
			SW_LED_EN, FLASH1_CTRL_EN, FLASH1_CTRL_EN);

	return 0;
}

static int sprd_flash_sc2731s1_close_preflash(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map,
			SW_LED_EN, FLASH1_CTRL_EN,
			~(unsigned int)FLASH1_CTRL_EN);

	return 0;
}

static int sprd_flash_sc2731s1_open_highlight(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[1];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int sprd_flash_sc2731s1_close_highlight(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[1];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int sprd_flash_sc2731s1_cfg_value_preflash(void *drvd, uint8_t idx,
					  struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map, FLASH1_MODE_TIME,
			FLASH1_CTRL_PRE_TIME, element->index);

	return 0;
}

static int sprd_flash_sc2731s1_cfg_value_highlight(void *drvd, uint8_t idx,
					   struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map, FLASH1_MODE_TIME,
			FLASH1_CTRL_REAL_TIME, element->index << 5);

	return 0;
}

static const struct sprd_flash_driver_ops flash_sc2731s1_ops = {
	.open_torch = sprd_flash_sc2731s1_open_torch,
	.close_torch = sprd_flash_sc2731s1_close_torch,
	.open_preflash = sprd_flash_sc2731s1_open_preflash,
	.close_preflash = sprd_flash_sc2731s1_close_preflash,
	.open_highlight = sprd_flash_sc2731s1_open_highlight,
	.close_highlight = sprd_flash_sc2731s1_close_highlight,
	.cfg_value_preflash = sprd_flash_sc2731s1_cfg_value_preflash,
	.cfg_value_highlight = sprd_flash_sc2731s1_cfg_value_highlight,
};

static const struct of_device_id sc2371_flash_of_match[] = {
	{ .compatible = "sprd,flash-sc2731s-com" },
	{},
};

static int sprd_flash_sc2731s_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct flash_driver_data *drv_data;
	int i = 0;

	if (IS_ERR(pdev))
		return -EINVAL;

	drv_data = devm_kzalloc(&pdev->dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	drv_data->reg_map = dev_get_regmap(pdev->dev.parent, NULL);
	if (IS_ERR(drv_data->reg_map)) {
		pr_err("failed to regmap for flash\n");
		ret = PTR_ERR(drv_data->reg_map);
		goto exit;
	}

	if (of_get_property(np, "pmic_flash_exchange", NULL))
		drv_data->pmic_flash_exchange = true;
	else
		drv_data->pmic_flash_exchange = false;

	ret = of_property_read_u32(np,
				"torch-led-idx", &drv_data->torch_led_index);
	if (ret)
		drv_data->torch_led_index = SPRD_FLASH_LED0;

	for (i = 0; i < FLASH_GPIO_MAX; i++) {
		drv_data->gpio_tab[i] = of_get_named_gpio(
			pdev->dev.of_node, flash_gpio_names[i], 0);
		if (gpio_is_valid(drv_data->gpio_tab[i])) {
			ret = devm_gpio_request(&pdev->dev,
				drv_data->gpio_tab[i], flash_gpio_names[i]);
			if (ret)
				goto exit;
		}
	}

	if (!drv_data->pmic_flash_exchange) {
		ret = sprd_flash_register(&flash_sc2731s0_ops,
			drv_data, SPRD_FLASH_FRONT);
		ret = sprd_flash_register(&flash_sc2731s1_ops,
			drv_data, SPRD_FLASH_REAR);
	} else {
		ret = sprd_flash_register(&flash_sc2731s1_ops,
			drv_data, SPRD_FLASH_FRONT);
		ret = sprd_flash_register(&flash_sc2731s0_ops,
			drv_data, SPRD_FLASH_REAR);
	}

	if (ret < 0)
		goto exit;

	spin_lock_init(&drv_data->slock);
	sc2731s_init(drv_data);
	sprd2731_flash_cal(drv_data);

exit:
	return ret;
}

static int sprd_flash_sc2731s_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver sprd_flash_sc2731s_drvier = {
	.probe = sprd_flash_sc2731s_probe,
	.remove = sprd_flash_sc2731s_remove,
	.driver = {
		.name = "flash-sc2731s-com",
		.of_match_table = of_match_ptr(sc2371_flash_of_match),
	},
};

static int __init sc2731s_flash_init(void)
{
	return platform_driver_register(&sprd_flash_sc2731s_drvier);
}

static void __exit sc2731s_flash_exit(void)
{
	platform_driver_unregister(&sprd_flash_sc2731s_drvier);
}

late_initcall(sc2731s_flash_init);
module_exit(sc2731s_flash_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xiaotong.lu@spreadtrum.com");
