#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/wait.h>
#include "lcm_i2c.h"

#define LCM_I2C_ID_NAME "I2C_LCD_BIAS"

static const struct of_device_id _lcm_i2c_of_match[] = {
	{
		.compatible = "lcd,i2c_lcd_bias",
	},
};

static const struct i2c_device_id _lcm_i2c_id[] = {
	{LCM_I2C_ID_NAME, 0},
	{}
};

static struct i2c_client *_lcm_i2c_client;
static int _lcm_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id);
static int _lcm_i2c_remove(struct i2c_client *client);

static struct i2c_driver _lcm_i2c_driver = {
	.id_table = _lcm_i2c_id,
	.probe = _lcm_i2c_probe,
	.remove = _lcm_i2c_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = LCM_I2C_ID_NAME,
		.of_match_table = _lcm_i2c_of_match,
	},

};
static int _lcm_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	pr_debug("[LCM][I2C] _lcm_i2c_probe\n");
	pr_debug("[LCM][I2C] info==>name=%s addr=0x%x\n",
			client->name, client->addr);
	_lcm_i2c_client = client;
	return 0;
}


static int _lcm_i2c_remove(struct i2c_client *client)
{
	pr_debug("[LCM][I2C] _lcm_i2c_remove\n");
	_lcm_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}


static int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = _lcm_i2c_client;
	char write_data[2] = { 0 };

	if (client == NULL) {
		pr_err("firefly ERROR!! _lcm_i2c_client is null\n");
		pr_err("firefly ERROR!! please register i2c client\n");
		return 0;
	}

	write_data[0] = addr;
	write_data[1] = value;
	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		pr_err("firefly ERROR!! _lcm_i2c write data fail !!\n");

	return ret;
}

int lcm_set_bias_volatage(unsigned char addr, unsigned char value)
{
	int ret = 0;
	ret = _lcm_i2c_write_bytes(addr, value);
	return ret;
}

static int __init _lcm_i2c_init(void)
{
	pr_debug("[LCM][I2C] _lcm_i2c_init\n");
	i2c_add_driver(&_lcm_i2c_driver);
	pr_debug("[LCM][I2C] _lcm_i2c_init success\n");

	return 0;
}


static void __exit _lcm_i2c_exit(void)
{
	pr_debug("[LCM][I2C] _lcm_i2c_exit\n");
	i2c_del_driver(&_lcm_i2c_driver);
}

module_init(_lcm_i2c_init);
module_exit(_lcm_i2c_exit);

MODULE_AUTHOR("Open");
MODULE_DESCRIPTION("LCM I2C Driver");
MODULE_LICENSE("GPL");

