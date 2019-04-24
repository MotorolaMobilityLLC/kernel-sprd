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
#include <linux/usb/charger.h>
#include "sc2731s_reg.h"
#include <linux/delay.h>
#include <linux/mfd/sc2705/registers.h>
#include <linux/sprd_battery_common.h>

#define FLASH_GPIO_MAX 3

#define VIN_ATTACHED      (SC2705_S_ADP_DET_MASK)
#define SC2705_FLASH_LP_2731_SHIFT	4
#define SC2705_FLASH_LP_2731_MASK		BIT(4)

/* Structure Definitions */

struct flash_driver_data {
	struct regmap *reg_map;
	spinlock_t slock;
	int gpio_tab[SPRD_FLASH_NUM_MAX][FLASH_GPIO_MAX];
	void *priv;
	struct device *dev;
	struct usb_charger *usb_charger;
	struct notifier_block chg_usb_nb;
	bool flash_torch_enable;
	bool usb_plugin;
	uint8_t idx;
	struct regmap *ldo_reg_map;
	u32 torch_led_index;
};

static struct flash_driver_data *sc2731_drv_data;

enum flash_led_type {
	LED_FLASH = 0,
	LED_TORCH,
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
			   FLASH1_MODE_TIME, FLASH1_CTRL_TORCH_TIME, 6 << 10);


#if 0
	sci_adi_clr(ANA_INTC_BASE + 0x08, (1 << 15));

	regmap_update_bits(drv_data->reg_map,
			   FLASH_IRQ_EN,
			   FLASH_IRQ_BIT_MASK, FLASH_IRQ_BIT_MASK);
#endif
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

static void sprd_flash_sc2731s_2705_ldo_cfg(int type, int plug, int enable)
{
	unsigned int read_val;
	int ret = 0;
	u8 mask = 0, shift = 0;
	u8 is_vbus_charge = 0;
	union power_supply_propval val;

	/* Also need to check if VIN <= 6V */
	if (plug && type == LED_TORCH) {
		mask = SC2705_TORCH_CHG_MASK;
		shift = SC2705_TORCH_CHG_SHIFT;
		is_vbus_charge = 1;
		if (enable == 1) {
			val.intval = 5000000;
			sprdpsy_set_property("ac",
				POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
			mdelay(500);
			regmap_read(sc2731_drv_data->ldo_reg_map,
				SC2705_EVENT_F, &read_val);
			pr_info("sc2705 event_f reg write 0x%x\n", read_val);
			if (read_val & SC2705_E_TORCH_CHG_OV_MASK) {
				regmap_update_bits(sc2731_drv_data->ldo_reg_map,
					SC2705_EVENT_F, 0xff, 0xff);
				regmap_read(sc2731_drv_data->ldo_reg_map,
					SC2705_EVENT_F, &read_val);
				if (read_val & SC2705_E_TORCH_CHG_OV_MASK) {
					val.intval = 0;
					sprdpsy_set_property("ac",
					POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
					return;
				}
			}
		}
	} else {
		mask = SC2705_FLASH_EN_MASK;
		shift = SC2705_FLASH_EN_SHIFT;
	}
	pr_info("sc2705 reg write 0x%x 0x%x\n", enable, mask);
	ret = regmap_update_bits(sc2731_drv_data->ldo_reg_map,
		SC2705_DCDC_CTRL_A, mask, ((enable > 0) ? 1:0) << shift);
	ret = regmap_update_bits(sc2731_drv_data->ldo_reg_map,
		SC2705_DCDC_CTRL_F, SC2705_FLASH_LP_2731_MASK,
		type << SC2705_FLASH_LP_2731_SHIFT);

	if (ret)
		goto err_i2c;

	if (enable)
		mdelay(10);

err_i2c:
	if (is_vbus_charge == 1) {
		if (enable == 0) {
			val.intval = 0;
			sprdpsy_set_property("ac",
					POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
		}
	}
	pr_info("%s ret : %d\n", __func__, ret);
}

static void sprd_flash_sc2731s_torch_2705_ldo_out(int enable)
{
	unsigned int read_val;
	int ret = 0;

	if ((sc2731_drv_data == NULL) || (sc2731_drv_data->ldo_reg_map == NULL))
		return;

	if (enable == 0 && sc2731_drv_data->flash_torch_enable == false)
		return;

	ret = regmap_read(sc2731_drv_data->ldo_reg_map,
				SC2705_STATUS_A, &read_val);
	if (ret)
		goto err_i2c;

	sprd_flash_sc2731s_2705_ldo_cfg(LED_TORCH,
					read_val & VIN_ATTACHED, enable);

err_i2c:
	pr_info("%s ret : %d\n", __func__, ret);
}

static void sprd_flash_sc2731s_usb_plugin_ldo_set(bool plugin, int enable)
{
	if ((sc2731_drv_data == NULL) || (sc2731_drv_data->ldo_reg_map == NULL))
		return;

	sprd_flash_sc2731s_2705_ldo_cfg(LED_TORCH, (int)plugin, enable);
	pr_info("%s ret\n", __func__);
}

static void sprd_flash_sc2731s_flash_2705_ldo_out(int enable)
{
	if ((sc2731_drv_data == NULL) || (sc2731_drv_data->ldo_reg_map == NULL))
		return;

	sprd_flash_sc2731s_2705_ldo_cfg(LED_FLASH, 0, enable);
	pr_info("%s ret\n", __func__);
}

/* API Function Implementation */

static int sprd_flash_sc2731s_open_torch(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	idx = drv_data->torch_led_index;
	sprd_flash_sc2731s_torch_2705_ldo_out(1);
	sc2731_drv_data->flash_torch_enable = true;
	sc2731_drv_data->idx = idx;

	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map,
			SW_LED_EN, TORCH0_CTRL_EN, TORCH0_CTRL_EN);

	if (SPRD_FLASH_LED1 & idx)
		regmap_update_bits(drv_data->reg_map,
			SW_LED_EN, TORCH1_CTRL_EN, TORCH1_CTRL_EN);

	return 0;
}

static int sprd_flash_sc2731s_close_torch(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map,
				   SW_LED_EN,
				   TORCH0_CTRL_EN,
				   ~(unsigned int)TORCH0_CTRL_EN);

	if (SPRD_FLASH_LED1 & idx)
		regmap_update_bits(drv_data->reg_map,
				   SW_LED_EN,
				   TORCH1_CTRL_EN,
				   ~(unsigned int)TORCH1_CTRL_EN);

	sprd_flash_sc2731s_torch_2705_ldo_out(0);
	sc2731_drv_data->flash_torch_enable = false;
	sc2731_drv_data->idx = idx;

	return 0;
}

static int sprd_flash_sc2731s_open_preflash(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	sprd_flash_sc2731s_flash_2705_ldo_out(1);

	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map,
			SW_LED_EN, FLASH0_CTRL_EN, FLASH0_CTRL_EN);

	if (SPRD_FLASH_LED1 & idx)
		regmap_update_bits(drv_data->reg_map,
			SW_LED_EN, FLASH1_CTRL_EN, FLASH1_CTRL_EN);

	return 0;
}

static int sprd_flash_sc2731s_close_preflash(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map,
				   SW_LED_EN,
				   FLASH0_CTRL_EN,
				   ~(unsigned int)FLASH0_CTRL_EN);

	if (SPRD_FLASH_LED1 & idx)
		regmap_update_bits(drv_data->reg_map,
				   SW_LED_EN,
				   FLASH1_CTRL_EN,
				   ~(unsigned int)FLASH1_CTRL_EN);

	sprd_flash_sc2731s_flash_2705_ldo_out(0);

	return 0;
}

static int sprd_flash_sc2731s_open_highlight(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	sprd_flash_sc2731s_flash_2705_ldo_out(1);

	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[0][0];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
			if (ret)
				goto exit;
		}
	}

	if (SPRD_FLASH_LED1 & idx) {
		gpio_id = drv_data->gpio_tab[1][0];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
			if (ret)
				goto exit;
		}
	}

exit:
	return ret;
}

static int sprd_flash_sc2731s_close_highlight(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[0][0];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
	}

	if (SPRD_FLASH_LED1 & idx) {
		gpio_id = drv_data->gpio_tab[1][0];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
	}

exit:
	sprd_flash_sc2731s_flash_2705_ldo_out(0);
	return ret;
}

static int sprd_flash_sc2731s_cfg_value_preflash(void *drvd, uint8_t idx,
					  struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map, FLASH0_MODE_TIME,
				   FLASH0_CTRL_PRE_TIME, element->index);


	if (SPRD_FLASH_LED1 & idx)
		regmap_update_bits(drv_data->reg_map, FLASH1_MODE_TIME,
				   FLASH1_CTRL_PRE_TIME, element->index);

	return 0;
}

static int sprd_flash_sc2731s_cfg_value_highlight(void *drvd, uint8_t idx,
					   struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map, FLASH0_MODE_TIME,
			FLASH0_CTRL_REAL_TIME, element->index << 5);

	if (SPRD_FLASH_LED1 & idx)
		regmap_update_bits(drv_data->reg_map, FLASH1_MODE_TIME,
			FLASH1_CTRL_REAL_TIME, element->index << 5);
	return 0;
}

static int sprd_flash_sc2731s_cfg_value_torch(void *drvd, uint8_t idx,
					   struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map, FLASH0_MODE_TIME,
			FLASH0_CTRL_TORCH_TIME, element->index << 10);

	if (SPRD_FLASH_LED1 & idx)
		regmap_update_bits(drv_data->reg_map, FLASH1_MODE_TIME,
			FLASH1_CTRL_TORCH_TIME, element->index << 10);
	return 0;
}

static int sprd_flash_sc2731s_usb_open_torch(uint8_t idx)
{
	sprd_flash_sc2731s_usb_plugin_ldo_set(sc2731_drv_data->usb_plugin, 1);
	sc2731_drv_data->flash_torch_enable = true;
	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(sc2731_drv_data->reg_map,
			SW_LED_EN, TORCH0_CTRL_EN, TORCH0_CTRL_EN);

	if (SPRD_FLASH_LED1 & idx)
		regmap_update_bits(sc2731_drv_data->reg_map,
			SW_LED_EN, TORCH1_CTRL_EN, TORCH1_CTRL_EN);

	return 0;
}

static int sprd_flash_sc2731s_usb_close_torch(uint8_t idx)
{
	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(sc2731_drv_data->reg_map,
				   SW_LED_EN,
				   TORCH0_CTRL_EN,
				   ~(unsigned int)TORCH0_CTRL_EN);

	if (SPRD_FLASH_LED1 & idx)
		regmap_update_bits(sc2731_drv_data->reg_map,
				   SW_LED_EN,
				   TORCH1_CTRL_EN,
				   ~(unsigned int)TORCH1_CTRL_EN);
	sprd_flash_sc2731s_usb_plugin_ldo_set(!sc2731_drv_data->usb_plugin, 0);
	sc2731_drv_data->flash_torch_enable = false;
	return 0;
}

static int sprd_flash_sc2731s_usb_plug_event(struct notifier_block *this,
				  unsigned long limit, void *ptr)
{
	bool isActive = sc2731_drv_data->flash_torch_enable;

	if (limit != 0) {
		sc2731_drv_data->usb_plugin = true;
		if (isActive) {
			sprd_flash_sc2731s_usb_close_torch(
					sc2731_drv_data->idx);
			sprd_flash_sc2731s_usb_open_torch(sc2731_drv_data->idx);
		}
	} else {
		sc2731_drv_data->usb_plugin = false;
		if (isActive) {
			sprd_flash_sc2731s_usb_close_torch(
					sc2731_drv_data->idx);
			sprd_flash_sc2731s_usb_open_torch(sc2731_drv_data->idx);
		}
	}
	return 0;
}

static const struct sprd_flash_driver_ops flash_sc2731s_ops = {
	.open_torch = sprd_flash_sc2731s_open_torch,
	.close_torch = sprd_flash_sc2731s_close_torch,
	.open_preflash = sprd_flash_sc2731s_open_preflash,
	.close_preflash = sprd_flash_sc2731s_close_preflash,
	.open_highlight = sprd_flash_sc2731s_open_highlight,
	.close_highlight = sprd_flash_sc2731s_close_highlight,
	.cfg_value_preflash = sprd_flash_sc2731s_cfg_value_preflash,
	.cfg_value_highlight = sprd_flash_sc2731s_cfg_value_highlight,
	.cfg_value_torch = sprd_flash_sc2731s_cfg_value_torch,
};

static const struct of_device_id sc2371_flash_of_match[] = {
	{ .compatible = "sprd,flash-sc2731", .data = &flash_sc2731s_ops },
	{},
};

static int sprd_flash_sc2731s_probe(struct platform_device *pdev)
{
	int ret = 0;
	int res = 0;
#if 0
	int irq;
#endif
	struct device_node *np;
	struct platform_device *pdev_regmap;
	struct flash_driver_data *drv_data;
	const struct of_device_id *of_id;
	int i = 0;
	int j = 0;

	if (IS_ERR(pdev))
		return -EINVAL;

	drv_data = devm_kzalloc(&pdev->dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	drv_data->dev = &pdev->dev;
	platform_set_drvdata(pdev, drv_data);
	sc2731_drv_data = drv_data;

	np = of_parse_phandle(pdev->dev.of_node, "sprd,pmic-flash", 0);
	if (IS_ERR_OR_NULL(np)) {
		pr_err("failed to get pmic flash node\n");
		ret = -ENOENT;
		goto exit;
	}

	pdev_regmap = of_find_device_by_node(np);
	if (IS_ERR_OR_NULL(pdev_regmap)) {
		pr_err("failed to get pmic flash pdev\n");
		of_node_put(np);
		ret = -ENOENT;
		goto exit;
	}

	drv_data->reg_map = dev_get_regmap(pdev_regmap->dev.parent, NULL);
	if (IS_ERR(drv_data->reg_map)) {
		pr_err("failed to regmap for flash\n");
		ret = PTR_ERR(drv_data->reg_map);
		goto exit;
	}

	of_id = of_match_node(sc2371_flash_of_match, pdev->dev.of_node);
	if (IS_ERR_OR_NULL(of_id)) {
		pr_err("failed to find matched id for pmic flash\n");
		ret = -ENOENT;
		goto exit;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				"torch-led-idx", &drv_data->torch_led_index);
	if (ret)
		drv_data->torch_led_index = SPRD_FLASH_LED0;

	for (i = 0; i < SPRD_FLASH_NUM_MAX; i++) {
		for (j = 0; j < FLASH_GPIO_MAX; j++) {
			drv_data->gpio_tab[i][j] =
				of_get_named_gpio(pdev->dev.of_node,
						  flash_gpio_names[i], j);
			if (gpio_is_valid(drv_data->gpio_tab[i][j])) {
				ret = devm_gpio_request(&pdev->dev,
						drv_data->gpio_tab[i][j],
						flash_gpio_names[i]);
				if (ret)
					goto exit;
			}
		}
	}

	ret = sprd_flash_register(of_id->data, drv_data, SPRD_FLASH_REAR);
	if (ret < 0)
		goto exit;
#if 0
	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		pr_err("failed to get irq\n");
		ret = -ENXIO;
		goto exit;
	}

	ret = devm_request_irq(&pdev->dev,
			       irq,
			       flash_interrupt_handler,
			       IRQF_SHARED, "sprd_flash", (void *)drv_data);
	if (ret < 0) {
		pr_err("failed to request irq %d\n", ret);
		goto exit;
	}
#endif
	spin_lock_init(&drv_data->slock);

	sc2731s_init(drv_data);

	sprd2731_flash_cal(drv_data);

	np = of_parse_phandle(pdev->dev.of_node, "sprd,2705-ldo", 0);
	if (IS_ERR_OR_NULL(np)) {
		pr_err("failed to get sprd,2705-ldo node\n");
		res = -ENOENT;
		goto exit;
	}

	pdev_regmap = of_find_device_by_node(np);
	if (IS_ERR_OR_NULL(pdev_regmap)) {
		pr_err("failed to get sprd,2705-ldo pdev\n");
		of_node_put(np);
		res = -ENOENT;
		goto exit;
	}

	drv_data->ldo_reg_map = dev_get_regmap(pdev_regmap->dev.parent, NULL);
	if (IS_ERR(drv_data->ldo_reg_map)) {
		pr_err("failed to regmap for sprd,2705-ldo\n");
		res = PTR_ERR(drv_data->ldo_reg_map);
		goto exit;
	}

	pr_info("register_usb_notifier\n");
	drv_data->usb_charger =
		usb_charger_find_by_name("usb-charger.0");
	if (IS_ERR(drv_data->usb_charger)) {
		res = -EPROBE_DEFER;
		dev_err(&pdev->dev,
			"Failed to find USB gadget: %d\n", ret);
		goto exit;
	}

	drv_data->chg_usb_nb.notifier_call = sprd_flash_sc2731s_usb_plug_event;

	res = usb_charger_register_notify(drv_data->usb_charger,
					  &drv_data->chg_usb_nb);
	if (res != 0) {
		dev_err(&pdev->dev,
			"Failed to register notifier: %d\n", ret);
		goto exit;
	}

	regmap_update_bits(drv_data->reg_map, RG_BST_CFG1,
			RG_BST_V|RG_BST_V_CAL,
			~(unsigned int)(RG_BST_V|RG_BST_V_CAL));

exit:

	return ret;
}

static int sprd_flash_sc2731s_remove(struct platform_device *pdev)
{
	struct flash_driver_data *drv_data = platform_get_drvdata(pdev);

	if (drv_data != NULL)
		usb_charger_unregister_notify(drv_data->usb_charger,
				      &drv_data->chg_usb_nb);

	return 0;
}

static struct platform_driver sprd_flash_sc2731s_drvier = {
	.probe = sprd_flash_sc2731s_probe,
	.remove = sprd_flash_sc2731s_remove,
	.driver = {
		.name = "flash-sc2731s",
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

