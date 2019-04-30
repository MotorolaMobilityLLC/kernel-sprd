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
#include <linux/usb/charger.h>
#include <linux/mfd/sc2705/registers.h>
#include "sc2705s_reg.h"
#include <linux/delay.h>
#define FLASH_GPIO_MAX 3

/* Structure Definitions */

struct flash_driver_data {
	struct regmap *reg_map;
	spinlock_t slock;
	int gpio_tab[SPRD_FLASH_NUM_MAX][FLASH_GPIO_MAX];
	void *priv;
	struct device *dev;
	struct usb_charger *usb_charger;
	struct notifier_block chg_usb_nb;
};
static struct flash_driver_data *sc2705_drv_data;

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

static void sc2705s_init(struct flash_driver_data *drv_data)
{
	/* flash ctrl */
}

#define BITSINDEX(b, o)  ((b) * 16 + (o))

static void sprd2705_flash_cal(void *drvd)
{
}

/* API Function Implementation */

static int sprd_flash_sc2705s_open_torch(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	return 0;
}

static int sprd_flash_sc2705s_close_torch(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	return 0;
}

static int sprd_flash_sc2705s_open_preflash(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	return 0;
}

static int sprd_flash_sc2705s_close_preflash(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	return 0;
}

static int sprd_flash_sc2705s_open_highlight(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	return 0;
}

static int sprd_flash_sc2705s_close_highlight(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	return 0;
}

static int sprd_flash_sc2705s_cfg_value_preflash(void *drvd, uint8_t idx,
					  struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	return 0;
}

static int sprd_flash_sc2705s_cfg_value_highlight(void *drvd, uint8_t idx,
					   struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	return 0;
}

static const struct sprd_flash_driver_ops flash_sc2705s_ops = {
	.open_torch = sprd_flash_sc2705s_open_torch,
	.close_torch = sprd_flash_sc2705s_close_torch,
	.open_preflash = sprd_flash_sc2705s_open_preflash,
	.close_preflash = sprd_flash_sc2705s_close_preflash,
	.open_highlight = sprd_flash_sc2705s_open_highlight,
	.close_highlight = sprd_flash_sc2705s_close_highlight,
	.cfg_value_preflash = sprd_flash_sc2705s_cfg_value_preflash,
	.cfg_value_highlight = sprd_flash_sc2705s_cfg_value_highlight,
};

static const struct of_device_id sc2705_flash_of_match[] = {
	{ .compatible = "sprd,sc2705-flash", .data = &flash_sc2705s_ops },
	{},
};

static int sprd_flash_sc2705s_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct flash_driver_data *drv_data;

	if (IS_ERR(pdev))
		return -EINVAL;

	drv_data = devm_kzalloc(&pdev->dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	drv_data->dev = &pdev->dev;
	platform_set_drvdata(pdev, drv_data);
	sc2705_drv_data = drv_data;

	drv_data->reg_map = dev_get_regmap(pdev->dev.parent, NULL);

#if 0
	ret = sprd_flash_register(&flash_sc2705s_ops, drv_data,
	SPRD_FLASH_FRONT);
#endif
	if (ret < 0)
		goto exit;

	spin_lock_init(&drv_data->slock);

	sc2705s_init(drv_data);

	sprd2705_flash_cal(drv_data);

exit:

	return ret;
}

static int sprd_flash_sc2705s_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver sprd_flash_sc2705s_drvier = {
	.probe = sprd_flash_sc2705s_probe,
	.remove = sprd_flash_sc2705s_remove,
	.driver = {
		.name = "sc2705-flash",
		.of_match_table = of_match_ptr(sc2705_flash_of_match),
	},
};

module_platform_driver(sprd_flash_sc2705s_drvier);
