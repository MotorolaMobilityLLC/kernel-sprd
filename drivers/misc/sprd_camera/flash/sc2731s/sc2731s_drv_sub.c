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
#include "sc2731s_reg.h"

#define FLASH_GPIO_MAX 3

/* Structure Definitions */

struct flash_driver_data {
	struct regmap *reg_map;
	spinlock_t slock;
	int gpio_tab[SPRD_FLASH_NUM_MAX][FLASH_GPIO_MAX];
	void *priv;
	u32 torch_led_index;
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
			   FLASH1_MODE_TIME, FLASH1_CTRL_TORCH_TIME, 19 << 10);

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

/* API Function Implementation */

static int sprd_flash_sc2731s_open_torch(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	idx = drv_data->torch_led_index;
	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map,
			SW_LED_EN, TORCH1_CTRL_EN, TORCH1_CTRL_EN);

	/*
	 * if (SPRD_FLASH_LED1 & idx)
	 *	regmap_update_bits(drv_data->reg_map,
	 *		SW_LED_EN, TORCH1_CTRL_EN, TORCH1_CTRL_EN);
	 */

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
				   TORCH1_CTRL_EN,
				   ~(unsigned int)TORCH1_CTRL_EN);

	/*
	 *if (SPRD_FLASH_LED1 & idx)
	 *	regmap_update_bits(drv_data->reg_map,
	 *			   SW_LED_EN,
	 *			   TORCH1_CTRL_EN,
	 *			   ~(unsigned int)TORCH1_CTRL_EN);
	 */

	return 0;
}

static int sprd_flash_sc2731s_open_preflash(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map,
			SW_LED_EN, FLASH1_CTRL_EN, FLASH1_CTRL_EN);

	/*
	 *if (SPRD_FLASH_LED1 & idx)
	 *	regmap_update_bits(drv_data->reg_map,
	 *		SW_LED_EN, FLASH1_CTRL_EN, FLASH1_CTRL_EN);
	 */

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
				   FLASH1_CTRL_EN,
				   ~(unsigned int)FLASH1_CTRL_EN);

	/*
	 *if (SPRD_FLASH_LED1 & idx)
	 *	regmap_update_bits(drv_data->reg_map,
	 *			   SW_LED_EN,
	 *			   FLASH1_CTRL_EN,
	 *			   ~(unsigned int)FLASH1_CTRL_EN);
	 */

	return 0;
}

static int sprd_flash_sc2731s_open_highlight(void *drvd, uint8_t idx)
{
	int ret = 0;
	int gpio_id = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx) {
		gpio_id = drv_data->gpio_tab[1][0];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
			if (ret)
				goto exit;
		}
	}

	/*
	 *if (SPRD_FLASH_LED1 & idx) {
	 *	gpio_id = drv_data->gpio_tab[1][0];
	 *	if (gpio_is_valid(gpio_id)) {
	 *		ret = gpio_direction_output(gpio_id, SPRD_FLASH_ON);
	 *		if (ret)
	 *			goto exit;
	 *	}
	 *}
	 */

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
		gpio_id = drv_data->gpio_tab[1][0];
		if (gpio_is_valid(gpio_id)) {
			ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
			if (ret)
				goto exit;
		}
	}

	/*
	 *if (SPRD_FLASH_LED1 & idx) {
	 *	gpio_id = drv_data->gpio_tab[1][0];
	 *	if (gpio_is_valid(gpio_id)) {
	 *		ret = gpio_direction_output(gpio_id, SPRD_FLASH_OFF);
	 *		if (ret)
	 *			goto exit;
	 *	}
	 *}
	 */

exit:
	return ret;
}

static int sprd_flash_sc2731s_cfg_value_preflash(void *drvd, uint8_t idx,
					  struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map, FLASH1_MODE_TIME,
				   FLASH1_CTRL_PRE_TIME, element->index);


	/*
	 *if (SPRD_FLASH_LED1 & idx)
	 *	regmap_update_bits(drv_data->reg_map, FLASH1_MODE_TIME,
	 *			   FLASH1_CTRL_PRE_TIME, element->index);
	 */

	return 0;
}

static int sprd_flash_sc2731s_cfg_value_highlight(void *drvd, uint8_t idx,
					   struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (SPRD_FLASH_LED0 & idx)
		regmap_update_bits(drv_data->reg_map, FLASH1_MODE_TIME,
			FLASH1_CTRL_REAL_TIME, element->index << 5);

	/*
	 *if (SPRD_FLASH_LED1 & idx)
	 *	regmap_update_bits(drv_data->reg_map, FLASH1_MODE_TIME,
	 *		FLASH1_CTRL_REAL_TIME, element->index << 5);
	 */

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
};

static const struct of_device_id sc2371_flash_of_match[] = {
	{ .compatible = "sprd,flash-sc2731sub", .data = &flash_sc2731s_ops },
	{},
};

static int sprd_flash_sc2731s_probe(struct platform_device *pdev)
{
	int ret = 0;
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

	ret = sprd_flash_register(of_id->data, drv_data, SPRD_FLASH_FRONT);
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

exit:

	return ret;
}

static int sprd_flash_sc2731s_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver sprd_flash_sc2731sub_drvier = {
	.probe = sprd_flash_sc2731s_probe,
	.remove = sprd_flash_sc2731s_remove,
	.driver = {
		.name = "flash-sc2731sub",
		.of_match_table = of_match_ptr(sc2371_flash_of_match),
	},
};

module_platform_driver(sprd_flash_sc2731sub_drvier);
