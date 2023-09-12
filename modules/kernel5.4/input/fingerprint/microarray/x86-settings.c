 /*
 * Copyright (C) 2012-2022 Microarray Co., Ltd.
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

#include "x86-settings.h"

static int ret;
static unsigned int int_gpio;
static struct device_node *node;
static int en_gpio;


/**
*  the platform struct start,for getting the platform device to set gpio state
*/

static struct of_device_id sof_match[] = {
	  { .compatible = MA_DTS_NAME, },		/*this name is used for matching the dts device for settings the gpios*/
	  { },
};
MODULE_DEVICE_TABLE(of, sof_match);

static struct platform_driver spdrv = {
	  .probe   = mas_plat_probe,
	  .remove  = mas_plat_remove,
	  .driver = {
		   .name  = MA_DRV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = sof_match,
	  }
};

void mas_select_transfer(struct spi_device *spi, int len)
{
	printk("mas_select_transfer start");
}

/*
 *  set spi speed, often we must check whether the setting is efficient
 */

void ma_spi_change(struct spi_device *spi, unsigned int  speed, int flag)
{
	printk("ma_spi_change start");
}

int mas_get_platform(void)
{
	int ret;
	MALOGD("start");
	ret = platform_driver_register(&spdrv);
	if (ret) {
		printk("MAFP mas_get_platform  spi_register_driver ret = %d", ret);
	}
	MALOGD("end");
	return ret;
}

int mas_remove_platform(void)
{
	return 0;
}

int mas_finger_get_gpio_info(struct platform_device *pdev)
{
	node = of_find_compatible_node(NULL, NULL, MA_DTS_NAME);
	return 0;
}



int mas_finger_set_spi(int cmd)
{
	return 0;
}

int mas_finger_set_power(int cmd)
{
	return 0;
}

/*
 * this is a demo function,if the power on-off switch by other way
 * modify it as the right way
 * on_off 1 on  0 off
 */
int mas_switch_power(unsigned int on_off)
{
	return 0;
}

int mas_finger_set_eint(int cmd)
{
	return 0;
}


int mas_finger_set_gpio_info(int cmd)
{
	return ret;
}

void mas_enable_spi_clock(struct spi_device *spi)
{
/*  mt_spi_enable_clk(spi_master_get_devdata(spi->master));*/
}

void mas_disable_spi_clock(struct spi_device *spi)
{
/*  mt_spi_disable_clk(spi_master_get_devdata(spi->master));*/
}

unsigned int mas_get_irq(struct platform_device *pdev)
{
	int status  = 0;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	int_gpio = of_get_named_gpio(np, "fpint-gpios", 0);
	status = gpio_request(int_gpio, "microarray_eint");
	if (status < 0) {
		MALOGE("gpio request error! ");
		return 0;
	}
	status = gpio_direction_input(int_gpio);
        if (status) {
                MALOGE("gpio_direction_input failed\n");
                return 0;
       }
	status = gpio_to_irq(int_gpio);
	if (status < 0) {
		MALOGE("gpio_to_irq failed\n");
		return 0;
	}
	return status;
}

/*
 * this function used for check the interrupt gpio state
 * @index 0 gpio level 1 gpio mode, often use 0
 * @return 0 gpio low 1 gpio high if index = 1,the return is the gpio mode
 * under 0 the of_property_read_u32_index return errno,check the dts as below:
 * last but not least use this function must checkt the label on dts file,
 * after is an example:
 * ma_finger: ma_finger{
 *   compatible = "mediatek,afs120x";
 *   finger_int_pin = <100 0>;
 * }
 */
int mas_get_interrupt_gpio(unsigned int index)
{
	int ret;

	ret = gpio_get_value(int_gpio);
	pr_debug("MAFP guq int %d\n", ret);
	return ret;
}

int mas_set_enable_gpio(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	en_gpio = of_get_named_gpio(np, "fpen-gpios", 0);
	if (en_gpio < 0) {
		MALOGD("en gpio not exist");
		return 0;
	 }
	ret = gpio_request(en_gpio, "microarray_en");
	if (ret < 0) {
		MALOGE("en gpio request error! ");
		return ret;
	 }
	ret = gpio_direction_output(en_gpio, 1);
	if (ret < 0) {
		MALOGE("en gpio set output error! ");
		gpio_free(en_gpio);
		return ret;
	}
	return 0;
}

