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
#include <linux/i2c.h>

#include "flash_drv.h"
#include <linux/usb/charger.h>
#include <linux/mfd/sc2703/registers.h>
#include "sc2703s_reg.h"
#include <linux/delay.h>
#include <linux/sprd_battery_common.h>
#define FLASH_GPIO_MAX 3

/* Structure Definitions */

struct flash_driver_data {
	struct regmap *reg_map;
	spinlock_t slock;
	int gpio_tab[SPRD_FLASH_NUM_MAX][FLASH_GPIO_MAX];
	u32 torch_led_index;
	void *priv;
	struct device *dev;
	struct usb_charger *usb_charger;
	struct notifier_block chg_usb_nb;
	struct regmap *charger_reg_map;
};
static struct flash_driver_data *sc2703_drv_data;

enum flash_led_type {
	LED_FLASH = 0,
	LED_TORCH,
};
/* Static Variables Definitions */

static const char *const flash_gpio_names[SPRD_FLASH_NUM_MAX] = {
	"flash-chip-en-gpios", /*gpio-89 connect to hw-en*/
	"flash-torch-en-gpios", /*gpio-87*/
	"flash-en-gpios", /*gpio-31 connect to strobe*/
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

static void sc2703s_init(struct flash_driver_data *drv_data)
{
	int ret;

	/* Make sure the timer disable bit is cleared */
	ret = regmap_update_bits(drv_data->reg_map,
			SC2703_FLASH_FD_CONFIG3,
			SC2703_FLASH_TIMEOUT_MASK,
			0x0B << SC2703_FLASH_TIMEOUT_SHIFT);

	ret = regmap_write(drv_data->reg_map,
			SC2703_FLASH_FD_CONFIG5, 0x04);

	ret = regmap_write(drv_data->reg_map,
			SC2703_FLASH_FD_CONFIG6, 0x02);

	ret = regmap_write(drv_data->reg_map,
			SC2703_FLASH_FD_CONFIG7, 0x04);

	ret = regmap_write(drv_data->reg_map,
			SC2703_FLASH_FD_CONFIG8, 0x02);

}

#define BITSINDEX(b, o)  ((b) * 16 + (o))

static void sprd2703_flash_cal(void *drvd)
{
}

void sc2703_charge_set_buck_mode(void *drvd)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (drv_data->charger_reg_map) {
		regmap_write(drv_data->charger_reg_map, 0xc0, 0xa1);
		regmap_write(drv_data->charger_reg_map, 0x85, 0x40);
		regmap_write(drv_data->charger_reg_map, 0xfc, 0x00);
		regmap_write(drv_data->charger_reg_map, 0xc0, 0x00);
	}
}

void sc2703_torch_mode_ac_charge_switch(void *drvd, int enable)
{
	union power_supply_propval val;

	if (enable == 1) {
		val.intval = 5000000;
		sprdpsy_set_property("ac",
			POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
		mdelay(100);
	} else {
		val.intval = 0;
		sprdpsy_set_property("ac",
				POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
	}
}

static int sc2703_flash_led_enable(void *drvd,
			int idx, int enable)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (enable)
		regmap_write(drv_data->reg_map, SC2703_FLASH_EVENT, 0xff);

	if (SPRD_FLASH_LED0 & idx) {
		regmap_update_bits(drv_data->reg_map,
			SC2703_FLASH_DRIVER_ACTIVE,
			SC2703_LED1_EN_MASK,
			enable << SC2703_LED1_EN_SHIFT);

		regmap_update_bits(drv_data->reg_map,
			SC2703_FLASH_DRIVER_ACTIVE,
			SC2703_LED1_DRIVER_EN_MASK,
			enable  << SC2703_LED1_DRIVER_EN_SHIFT);
	}
	if (SPRD_FLASH_LED1 & idx) {
		regmap_update_bits(drv_data->reg_map,
			SC2703_FLASH_DRIVER_ACTIVE,
			SC2703_LED2_EN_MASK,
			enable << SC2703_LED2_EN_SHIFT);

		regmap_update_bits(drv_data->reg_map,
			SC2703_FLASH_DRIVER_ACTIVE,
			SC2703_LED2_DRIVER_EN_MASK,
			enable  << SC2703_LED2_DRIVER_EN_SHIFT);

	}

	return 0;
}

static int sc2703_flash_set_mode(void *drvd, enum flash_led_type mode)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	regmap_update_bits(drv_data->reg_map,
		SC2703_FLASH_DRIVER_ACTIVE,
		SC2703_LED_MODE_MASK,
		mode << SC2703_LED_MODE_SHIFT);

	return 0;
}

/* API Function Implementation */

static int sprd_flash_sc2703s_open_torch(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	idx = drv_data->torch_led_index;
	sc2703_charge_set_buck_mode(drvd);
	sc2703_torch_mode_ac_charge_switch(drvd, 1);
	sc2703_flash_set_mode(drvd, LED_TORCH);
	sc2703_flash_led_enable(drvd, idx, 1);

	return 0;
}

static int sprd_flash_sc2703s_close_torch(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	idx = drv_data->torch_led_index;
	sc2703_flash_led_enable(drvd, idx, 0);
	sc2703_torch_mode_ac_charge_switch(drvd, 0);

	return 0;
}

static int sprd_flash_sc2703s_open_preflash(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	sc2703_flash_set_mode(drvd, LED_TORCH);
	sc2703_flash_led_enable(drvd, idx, 1);

	return 0;
}

static int sprd_flash_sc2703s_close_preflash(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	sc2703_flash_led_enable(drvd, idx, 0);

	return 0;
}

static int sprd_flash_sc2703s_open_highlight(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	sc2703_flash_set_mode(drvd, LED_FLASH);
	sc2703_flash_led_enable(drvd, idx, 1);

	return 0;
}

static int sprd_flash_sc2703s_close_highlight(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	sc2703_flash_led_enable(drvd, idx, 0);

	return 0;
}

static int sprd_flash_sc2703s_cfg_value_preflash(void *drvd, uint8_t idx,
					  struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	regmap_write(drv_data->reg_map,
			SC2703_FLASH_FD_CONFIG5 + idx/2 * 2, element->index);

	return 0;
}

static int sprd_flash_sc2703s_cfg_value_highlight(void *drvd, uint8_t idx,
					   struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	regmap_write(drv_data->reg_map,
			SC2703_FLASH_FD_CONFIG6 + idx/2 * 2, element->index);

	return 0;
}

static int sprd_flash_sc2703s_cfg_value_torch(void *drvd, uint8_t idx,
					   struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	idx = drv_data->torch_led_index;
	regmap_write(drv_data->reg_map,
			SC2703_FLASH_FD_CONFIG5 + idx/2 * 2, element->index);

	return 0;
}

static const struct sprd_flash_driver_ops flash_sc2703s_ops = {
	.open_torch = sprd_flash_sc2703s_open_torch,
	.close_torch = sprd_flash_sc2703s_close_torch,
	.open_preflash = sprd_flash_sc2703s_open_preflash,
	.close_preflash = sprd_flash_sc2703s_close_preflash,
	.open_highlight = sprd_flash_sc2703s_open_highlight,
	.close_highlight = sprd_flash_sc2703s_close_highlight,
	.cfg_value_preflash = sprd_flash_sc2703s_cfg_value_preflash,
	.cfg_value_highlight = sprd_flash_sc2703s_cfg_value_highlight,
	.cfg_value_torch = sprd_flash_sc2703s_cfg_value_torch,
};

static const struct i2c_device_id sc2703_flash_i2c_id[] = {
	{"sc2703-flash", 0},
	{},
};

static const struct of_device_id sc2703_flash_of_match[] = {
	{ .compatible = "sprd,sc2703-flash", .data = &flash_sc2703s_ops },
	{},
};

static bool sc2703_flash_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SC2703_FLASH_EVENT:
	case SC2703_FLASH_STATUS:
	case SC2703_FLASH_DRIVER_ACTIVE:
	case SC2703_FLASH_DRIVER_STATUS:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config sc2703_flash_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = SC2703_FLASH_FD_FAULT_CONFIG,
	.volatile_reg = sc2703_flash_volatile_reg,
};

static int sprd_flash_sc2703s_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	int ret = 0;
	struct flash_driver_data *drv_data;
	struct device_node *np;
	struct platform_device *pdev;

	pr_err("%s\n", __func__);

	drv_data = devm_kzalloc(&i2c->dev, sizeof(struct flash_driver_data),
							GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	drv_data->dev = &i2c->dev;
	sc2703_drv_data = drv_data;

	drv_data->reg_map = devm_regmap_init_i2c(i2c, &sc2703_flash_regmap);
	if (IS_ERR(drv_data->reg_map)) {
		ret = PTR_ERR(drv_data->reg_map);
		dev_err(drv_data->dev,
			"Failed to allocate register map: %d\n", ret);
		return ret;
	}

	np = of_parse_phandle(drv_data->dev->of_node, "sc2703,charger", 0);
	if (IS_ERR_OR_NULL(np))
		pr_err("failed to get sc2703,charger node\n");
	else {
		pdev = of_find_device_by_node(np);
		if (IS_ERR_OR_NULL(pdev)) {
			pr_err("failed to get sc2703,charger pdev\n");
			of_node_put(np);
		} else {
			drv_data->charger_reg_map =
					dev_get_regmap(pdev->dev.parent, NULL);
			if (IS_ERR(drv_data->charger_reg_map)) {
				pr_err("failed to regmap for sc2703,charger\n");
				drv_data->charger_reg_map = NULL;
			}
		}
	}

	ret = of_property_read_u32(drv_data->dev->of_node,
				"torch-led-idx", &drv_data->torch_led_index);
	if (ret)
		drv_data->torch_led_index = SPRD_FLASH_LED0;

	i2c_set_clientdata(i2c, drv_data);
#if 0
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
#endif
	ret = sprd_flash_register(&flash_sc2703s_ops, drv_data,
	SPRD_FLASH_REAR);

	if (ret < 0)
		goto exit;

	spin_lock_init(&drv_data->slock);

	sc2703s_init(drv_data);

	sprd2703_flash_cal(drv_data);

	pr_err("%s\n", __func__);

exit:

	return ret;
}

static int sprd_flash_sc2703s_remove(struct i2c_client *client)
{
	return 0;
}

static struct i2c_driver sprd_flash_sc2703s_drvier = {
	.driver = {
		.name = "sc2703-flash",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(sc2703_flash_of_match),
#endif
	},
	.probe = sprd_flash_sc2703s_probe,
	.remove = sprd_flash_sc2703s_remove,
	.id_table = sc2703_flash_i2c_id,
};

module_i2c_driver(sprd_flash_sc2703s_drvier);
