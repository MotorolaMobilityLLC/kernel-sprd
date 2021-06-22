/*
 * platform indepent driver interface
 * Copyright (C) 2016 Goodix
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/timer.h>
#include <linux/err.h>

#include "gf_spi.h"

#if defined(USE_SPI_BUS)
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#elif defined(USE_PLATFORM_BUS)
#include <linux/platform_device.h>
#endif

int gf_parse_dts(struct gf_dev *gf_dev)
{
	int rc = 0;
	struct device *dev = &gf_dev->spi->dev;
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "sprd,fingerprint-fpc1520");
	gf_dev->reset_gpio = of_get_named_gpio(node, "fpsensor,reset-gpio", 0);
	pr_debug("value of gf_dev->reset_gpio is %d!\n", gf_dev->reset_gpio);
	if (gf_dev->reset_gpio < 0) {
		pr_err("falied to get reset gpio!\n");
		return gf_dev->reset_gpio;
	}

	rc = devm_gpio_request(dev, gf_dev->reset_gpio, "goodix_reset");
	pr_info("value of rc for request gf_dev->reset_gpio is %d!\n", rc);
	if (rc) {
	    pr_err("failed to request reset gpio, rc = %d\n", rc);
            gpio_free(gf_dev->reset_gpio);
            rc = devm_gpio_request(dev, gf_dev->reset_gpio, "goodix_reset");
            pr_err("value of rc for request gf_dev->reset_gpio is %d!\n", rc);
	}

	gpio_direction_output(gf_dev->reset_gpio, 1);

	rc = of_get_named_gpio(node, "fpsensor,eint-gpio", 0);
	if (rc < 0) {
		pr_err("failed to get fpsensor,eint-gpio\n");
                return rc;
	}
	gf_dev->irq_gpio = rc;
	pr_err("value of gf_dev->irq_gpio is %d!\n", gf_dev->irq_gpio);

	rc = devm_gpio_request(dev, gf_dev->irq_gpio, "goodix_irq");
	pr_info("value of rc for request gf_dev->irq_gpio is %d!\n", rc);
	if (rc) {
		pr_err("failed to request irq gpio, rc = %d\n", rc);
                gpio_free(gf_dev->irq_gpio);
                rc = devm_gpio_request(dev, gf_dev->irq_gpio, "goodix_irq");
                pr_err("value of rc for request gf_dev->irq_gpio is %d!\n", rc);
	}

	gpio_direction_input(gf_dev->irq_gpio);

	return rc;

}

void gf_cleanup(struct gf_dev *gf_dev)
{
	struct device *dev = &gf_dev->spi->dev;

	pr_info("[info] %s\n", __func__);

	if (gpio_is_valid(gf_dev->irq_gpio)) {
		devm_gpio_free(dev, gf_dev->irq_gpio);
		pr_info("remove irq_gpio success\n");
	}
	if (gpio_is_valid(gf_dev->reset_gpio)) {
		devm_gpio_free(dev, gf_dev->reset_gpio);
		pr_info("remove reset_gpio success\n");
	}
}

int gf_power_on(struct gf_dev *gf_dev)
{
	int rc = 0;

	if (!gf_dev->power_enabled) {
		gf_dev->power_enabled = 1;
		/* TODO: add your power control here */
	}

	return rc;
}

int gf_power_off(struct gf_dev *gf_dev)
{
	int rc = 0;

	if (gf_dev->power_enabled) {
		gf_dev->power_enabled = 0;
		/* TODO: add your power control here */
	}

	return rc;
}

int gf_hw_reset(struct gf_dev *gf_dev, unsigned int delay_ms)
{
	if (!gf_dev) {
		pr_info("Input buff is NULL.\n");
		return -ENODEV;
	}
	gpio_direction_output(gf_dev->reset_gpio, 1);
	gpio_set_value(gf_dev->reset_gpio, 0);
	mdelay(3);
	gpio_set_value(gf_dev->reset_gpio, 1);
	mdelay(delay_ms);
	return 0;
}

int gf_irq_num(struct gf_dev *gf_dev)
{
	if (!gf_dev) {
		pr_info("Input buff is NULL.\n");
		return -ENODEV;
	} else {
		return gpio_to_irq(gf_dev->irq_gpio);
	}
}

