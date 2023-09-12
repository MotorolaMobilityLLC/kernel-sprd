/*
 * Copyright (C) 2021-2022 UNISOC Communications Inc.
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
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#include "sprd_img.h"
#include "flash_drv.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "flash_aw36515: %d %d %s : " fmt, current->pid, __LINE__, __func__

#define FLASH_IC_DRIVER_NAME       "flash_aw36515"
/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */
#define I2C_SLAVEADDR	0x63

struct flash_ic_cfg {
	unsigned int lvfm_enable;
	unsigned int torch_level;
	unsigned int preflash_level;
	unsigned int highlight_level;
	unsigned int cfg_factor;
};


#define FLASH_IC_GPIO_MAX 4

struct flash_driver_data {
	struct i2c_client *i2c_info;
	struct mutex i2c_lock;
	int gpio_tab[FLASH_IC_GPIO_MAX];
	void *priv;
	struct flash_ic_cfg flash_cfg;
	u32 torch_led_index;
};
/* Static Variables Definitions */

static const char *const flash_ic_gpio_names[FLASH_IC_GPIO_MAX] = {
	"flash-chip-en-gpios",
	"flash-torch-en-gpios",
	"flash-en-gpios",
	"flash-sync-gpios",
};

static int flash_ic_driver_reg_write(struct i2c_client *i2c, u8 reg, u8 value)
{
	int ret;

	pr_debug("flash ic reg write %x %x\n", reg, value);

	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	return ret;
}

#if 0
static int flash_ic_driver_reg_read(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	int ret;

	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret < 0) {
		pr_info("%s:%s reg(0x%x), ret(%d)\n",
			FLASH_IC_DRIVER_NAME, __func__, reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}
#endif

static int sprd_flash_ic_init(void *drvd)
{
	int ret = 0;
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	if (drv_data->i2c_info) {
		ret = flash_ic_driver_reg_write(drv_data->i2c_info, 0x01, 0x00);
		ret = flash_ic_driver_reg_write(drv_data->i2c_info, 0x08, 0x1f);
		pr_info("flash ic init ret %d %d\n", ret, __LINE__);
	}
	return 0;
}

static int sprd_flash_ic_open_torch(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	pr_info("torch_led_index:%d, idx:%d\n", drv_data->torch_led_index, idx);
	idx = drv_data->torch_led_index;

	if (drv_data->i2c_info) {
		flash_ic_driver_reg_write(drv_data->i2c_info, 0x01, 0x0a);
	}

	return 0;
}

static int sprd_flash_ic_close_torch(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	pr_info("torch_led_index:%d, idx:%d\n", drv_data->torch_led_index, idx);
	idx = drv_data->torch_led_index;

	if (drv_data->i2c_info) {
		flash_ic_driver_reg_write(drv_data->i2c_info, 0x01, 0x00);
	}

	return 0;
}

static int sprd_flash_ic_open_preflash(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	pr_info("torch_led_index:%d, idx:%d\n", drv_data->torch_led_index, idx);
	idx = drv_data->torch_led_index;

	if (drv_data->i2c_info) {
		flash_ic_driver_reg_write(drv_data->i2c_info, 0x01, 0x0a);
	}

	return 0;
}

static int sprd_flash_ic_close_preflash(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	pr_info("torch_led_index:%d, idx:%d\n", drv_data->torch_led_index, idx);
	idx = drv_data->torch_led_index;

	if (drv_data->i2c_info) {
		flash_ic_driver_reg_write(drv_data->i2c_info, 0x01, 0x00);
	}

	return 0;
}

static int sprd_flash_ic_open_highlight(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	pr_info("torch_led_index:%d, idx:%d\n", drv_data->torch_led_index, idx);
	idx = drv_data->torch_led_index;

	if (drv_data->i2c_info) {
		flash_ic_driver_reg_write(drv_data->i2c_info, 0x01, 0x0E);
	}

	return 0;
}

static int sprd_flash_ic_close_highlight(void *drvd, uint8_t idx)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	pr_info("torch_led_index:%d, idx:%d\n", drv_data->torch_led_index, idx);
	idx = drv_data->torch_led_index;

	if (drv_data->i2c_info) {
		flash_ic_driver_reg_write(drv_data->i2c_info, 0x01, 0x00);
	}

	return 0;
}

static int sprd_flash_ic_cfg_value_torch(void *drvd, uint8_t idx,
					  struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	pr_info("element->index:%d", element->index);
	pr_info("torch_led_index:%d, idx:%d\n", drv_data->torch_led_index, idx);
	idx = drv_data->torch_led_index;

	if (drv_data->i2c_info) {
		flash_ic_driver_reg_write(drv_data->i2c_info, 0x05, element->index * 8);
	}

	return 0;
}

static int sprd_flash_ic_cfg_value_preflash(void *drvd, uint8_t idx,
					  struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	pr_info("element->index:%d", element->index);
	pr_info("torch_led_index:%d, idx:%d\n", drv_data->torch_led_index, idx);
	idx = drv_data->torch_led_index;

	if (drv_data->i2c_info) {
		flash_ic_driver_reg_write(drv_data->i2c_info, 0x05, element->index * 8);
	}

	return 0;
}

static int sprd_flash_ic_cfg_value_highlight(void *drvd, uint8_t idx,
					   struct sprd_flash_element *element)
{
	struct flash_driver_data *drv_data = (struct flash_driver_data *)drvd;

	if (!drv_data)
		return -EFAULT;

	pr_info("element->index:%d", element->index);
	pr_info("torch_led_index:%d, idx:%d\n", drv_data->torch_led_index, idx);
	idx = drv_data->torch_led_index;

	if (drv_data->i2c_info) {
		flash_ic_driver_reg_write(drv_data->i2c_info, 0x03, element->index * 8);
	}

	return 0;
}

static const struct of_device_id sprd_flash_ic_of_match_table[] = {
	{.compatible = "sprd,flash-aw36515"},
	{/* MUST end with empty struct */},
};

static const struct i2c_device_id sprd_flash_ic_ids[] = {
	{}
};
static const struct sprd_flash_driver_ops flash_ic_ops = {
	.open_torch = sprd_flash_ic_open_torch,
	.close_torch = sprd_flash_ic_close_torch,
	.open_preflash = sprd_flash_ic_open_preflash,
	.close_preflash = sprd_flash_ic_close_preflash,
	.open_highlight = sprd_flash_ic_open_highlight,
	.close_highlight = sprd_flash_ic_close_highlight,
	.cfg_value_torch = sprd_flash_ic_cfg_value_torch,
	.cfg_value_preflash = sprd_flash_ic_cfg_value_preflash,
	.cfg_value_highlight = sprd_flash_ic_cfg_value_highlight,

};

static int sprd_flash_ic_driver_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret = 0;
	struct device *dev = &client->dev;
	struct flash_driver_data *pdata = NULL;
	unsigned int j;

	if (!dev->of_node) {
		pr_err("no device node %s", __func__);
		return -ENODEV;
	}
	pr_info("flash-ic-driver probe\n");
	ret = of_property_read_u32(dev->of_node, "sprd,flash-ic", &j);
	if (ret || j != 36515)
		return -ENODEV;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;
	client->dev.platform_data = (void *)pdata;
	pdata->i2c_info = client;
	mutex_init(&pdata->i2c_lock);

	//pdata->i2c_info->addr = I2C_SLAVEADDR;
	pr_info("I2C_SLAVEADDR %x", pdata->i2c_info->addr);

	ret = of_property_read_u32(dev->of_node,
				"torch-led-idx", &pdata->torch_led_index);
	if (ret)
		pdata->torch_led_index = SPRD_FLASH_LED0;

	ret = sprd_flash_ic_init(pdata);

	ret = sprd_flash_register(&flash_ic_ops, pdata, SPRD_FLASH_REAR);

	return ret;
}
static int sprd_flash_ic_driver_remove(struct i2c_client *client)
{
	struct flash_driver_data *pdata = NULL;
	pr_info("flash-ic-driver remove");
	pdata = (struct flash_driver_data *)client->dev.platform_data;
	if (pdata)
		devm_kfree(&client->dev, pdata);

	pdata = NULL;
	client->dev.platform_data = NULL;

	return 0;
}

static void sprd_flash_ic_driver_shutdown(struct i2c_client *client)
{
	struct flash_driver_data *drv_data = NULL;

	drv_data = (struct flash_driver_data *)client->dev.platform_data;
	if (!drv_data)
		return;

	pr_info("flash-ic-driver shutdown, idx:%d", drv_data->torch_led_index);

	if (drv_data->i2c_info) {
                flash_ic_driver_reg_write(drv_data->i2c_info, 0x01, 0x00);
	}

        return;
}

static struct i2c_driver sprd_flash_ic_driver = {
	.driver = {
		.of_match_table = of_match_ptr(sprd_flash_ic_of_match_table),
		.name = FLASH_IC_DRIVER_NAME,
		},
	.probe = sprd_flash_ic_driver_probe,
	.remove = sprd_flash_ic_driver_remove,
	.id_table = sprd_flash_ic_ids,
	.shutdown = sprd_flash_ic_driver_shutdown,
};

static int sprd_flash_ic_register_driver(void)
{
	int ret = 0;

	ret = i2c_add_driver(&sprd_flash_ic_driver);
	pr_info("register sprd_flash_ic_driver:%d\n", ret);

	return ret;
}

static void sprd_flash_ic_unregister_driver(void)
{
	i2c_del_driver(&sprd_flash_ic_driver);
}

static int sprd_flash_ic_driver_init(void)
{
	int ret = 0;

	ret = sprd_flash_ic_register_driver();
	return ret;
}

static void sprd_flash_ic_driver_deinit(void)
{
	sprd_flash_ic_unregister_driver();
}

module_init(sprd_flash_ic_driver_init);
module_exit(sprd_flash_ic_driver_deinit);
MODULE_DESCRIPTION("Sprd aw36515 Flash Driver");
MODULE_LICENSE("GPL");

