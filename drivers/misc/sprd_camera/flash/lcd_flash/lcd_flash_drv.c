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
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/backlight.h>
#include "sprd_dispc.h"

#include "flash_drv.h"
#include "lcd_flash_reg.h"
#define FLASH_GPIO_MAX 3

/* Structure Definitions */
struct flash_driver_data {
	struct regmap *reg_map;
	spinlock_t slock;
	int gpio_tab[SPRD_FLASH_NUM_MAX][FLASH_GPIO_MAX];
	void *priv;
	unsigned int  lcd_reg_base;
	int is_highlight;
	struct sprd_dispc *dispc;
	struct backlight_device *backlight;
	int last_brightness;
};

/* Static Variables Definitions */

static const char *const flash_gpio_names[SPRD_FLASH_NUM_MAX] = {
	"flash0-gpios",
	"flash1-gpios",
	"flash2-gpios",
};

/* Internal Function Implementation */
#if 0
static irqreturn_t flash_interrupt_handler(int irq, void *priv)
{
	int ret = 0;
	unsigned int status;
	unsigned long flag;
	irqreturn_t irq_ret = 0;
	struct flash_driver_data *drv_data;

	if (!priv)
		return IRQ_NONE;

	drv_data = (struct flash_driver_data *)priv;

	spin_lock_irqsave(&drv_data->slock, flag);

	ret = regmap_read(drv_data->reg_map, FLASH_IRQ_INT, &status);
	if (ret) {
		spin_unlock_irqrestore(&drv_data->slock, flag);
		return IRQ_NONE;
	}

	status &= FLASH_IRQ_BIT_MASK;
	pr_info("irq status 0x%x\n", status);

	regmap_update_bits(drv_data->reg_map,
			   FLASH_IRQ_CLR,
			   FLASH_IRQ_BIT_MASK, FLASH_IRQ_BIT_MASK);

	if (status)
		irq_ret = IRQ_HANDLED;
	else
		irq_ret = IRQ_NONE;

	spin_unlock_irqrestore(&drv_data->slock, flag);

	return irq_ret;
}
#endif

static void sprd_flash_lcd_bk_init(struct flash_driver_data *drv_data)
{
	/* flash ctrl */
}

#define BITSINDEX(b, o)  ((b) * 16 + (o))

static void sprd_flash_cal(void *drvd)
{
}

/* API Function Implementation */

static int sprd_flash_lcd_open_torch(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;


	return 0;
}

static int sprd_flash_lcd_close_torch(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;


	return 0;
}

static int sprd_flash_lcd_open_preflash(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (drv_data->dispc) {
		sprd_dispc_bgcolor(drv_data->dispc, 0xffffff);
		sprd_dispc_disable_timeout(drv_data->dispc, 4000);
	}

	if (drv_data->backlight) {
		drv_data->last_brightness =
			drv_data->backlight->props.brightness;
		drv_data->backlight->props.brightness = 0xff;
		backlight_update_status(drv_data->backlight);
	}

	return 0;
}

static int sprd_flash_lcd_close_preflash(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	return 0;
}

static int sprd_flash_lcd_open_highlight(void *drvd, uint8_t idx)
{
	int ret = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	return ret;
}

static int sprd_flash_lcd_close_highlight(void *drvd, uint8_t idx)
{
	int ret = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (drv_data->is_highlight) {
		if (drv_data->backlight) {
			drv_data->backlight->props.brightness =
					drv_data->last_brightness;
			backlight_update_status(drv_data->backlight);
		}

		if (drv_data->dispc)
			sprd_dispc_refresh_restore(drv_data->dispc);

		drv_data->is_highlight = 0;
	}

	return ret;
}

static int sprd_flash_lcd_cfg_value_preflash(void *drvd, uint8_t idx,
					  struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	drv_data->is_highlight = 0;

	return 0;
}

static int sprd_flash_lcd_cfg_value_highlight(void *drvd, uint8_t idx,
					   struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	drv_data->is_highlight = 1;

	return 0;
}

static const struct sprd_flash_driver_ops flash_lcd_ops = {
	.open_torch = sprd_flash_lcd_open_torch,
	.close_torch = sprd_flash_lcd_close_torch,
	.open_preflash = sprd_flash_lcd_open_preflash,
	.close_preflash = sprd_flash_lcd_close_preflash,
	.open_highlight = sprd_flash_lcd_open_highlight,
	.close_highlight = sprd_flash_lcd_close_highlight,
	.cfg_value_preflash = sprd_flash_lcd_cfg_value_preflash,
	.cfg_value_highlight = sprd_flash_lcd_cfg_value_highlight,
};

static const struct of_device_id lcd_flash_of_match[] = {
	{ .compatible = "sprd,lcd-flash", .data = &flash_lcd_ops },
	{},
};

static int sprd_flash_lcd_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np;
	struct flash_driver_data *drv_data;

	if (IS_ERR(pdev))
		return -EINVAL;

	drv_data = devm_kzalloc(&pdev->dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	pdev->dev.platform_data = (void *)drv_data;

	drv_data->reg_map = dev_get_regmap(pdev->dev.parent, NULL);
	if (IS_ERR(drv_data->reg_map)) {
		pr_err("failed to regmap for flash\n");
		ret = PTR_ERR(drv_data->reg_map);
		goto exit;
	}

	np = of_parse_phandle(pdev->dev.of_node, "sprd,backlight", 0);

	if (np) {
		drv_data->backlight = of_find_backlight_by_node(np);

		if (!drv_data->backlight) {
			pr_err("failed to get backlight pdev\n");
			of_node_put(np);
			ret = -ENOENT;
			goto exit;
		}
	}

	np = of_parse_phandle(pdev->dev.of_node, "sprd,dispc", 0);

	if (np) {
		drv_data->dispc = platform_get_drvdata(
				of_find_device_by_node(np));

		if (!drv_data->dispc) {
			pr_err("failed to get dispc pdev\n");
			of_node_put(np);
			ret = -ENOENT;
			goto exit;
		}
	}

	ret = sprd_flash_register(&flash_lcd_ops, drv_data,
							SPRD_FLASH_FRONT);
	if (ret < 0)
		goto exit;
	spin_lock_init(&drv_data->slock);

	sprd_flash_lcd_bk_init(drv_data);

	sprd_flash_cal(drv_data);


exit:

	return ret;
}

static int sprd_flash_lcd_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver sprd_flash_lcd_drvier = {
	.probe = sprd_flash_lcd_probe,
	.remove = sprd_flash_lcd_remove,
	.driver = {
		.name = "lcd-flash",
		.of_match_table = of_match_ptr(lcd_flash_of_match),
	},
};

static int __init sprd_flash_lcd_init(void)
{
	return platform_driver_register(&sprd_flash_lcd_drvier);
}

static void __exit sprd_flash_lcd_exit(void)
{
	platform_driver_unregister(&sprd_flash_lcd_drvier);
}

late_initcall(sprd_flash_lcd_init);

module_exit(sprd_flash_lcd_exit);
