#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#define LOG_TAG "LCM_BIAS"
#define LCM_LOGI(fmt, args...)  pr_err("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)


/*****************************************************************************
 * Define
 *****************************************************************************/

#define I2C_ID_NAME "OCP2131"

/*****************************************************************************
 * GLobal Variable
 *****************************************************************************/

static struct i2c_client *OCP2131_i2c_client;

/*****************************************************************************
 * Function Prototype
 *****************************************************************************/
static int OCP2131_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int OCP2131_remove(struct i2c_client *client);
/*****************************************************************************
 * Data Structure
 *****************************************************************************/

struct OCP2131_dev {
	struct i2c_client *client;

};

static const struct i2c_device_id OCP2131_id[] = {
	{I2C_ID_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, OCP2131_id);

static struct of_device_id OCP2131_dt_match[] = {
	{ .compatible = "sprd,i2c_lcd_bias" },
	{ },
};

static struct i2c_driver OCP2131_iic_driver = {
	.id_table = OCP2131_id,
	.probe = OCP2131_probe,
	.remove = OCP2131_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "OCP2131",
		.of_match_table = of_match_ptr(OCP2131_dt_match),
	},
};

/*****************************************************************************
 * Function
 *****************************************************************************/
static int OCP2131_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	LCM_LOGI("OCP2131_iic_probe\n");
	LCM_LOGI("TPS: info==>name=%s addr=0x%x\n", client->name, client->addr);
	OCP2131_i2c_client = client;
	return 0;
}

static int OCP2131_remove(struct i2c_client *client)
{
	LCM_LOGI("OCP2131_remove\n");
	OCP2131_i2c_client = NULL;
	i2c_unregister_device(client);
	return 0;
}


int OCP2131_write_bytes(unsigned char addr, unsigned char value)
{
	int ret = 0;
	struct i2c_client *client = OCP2131_i2c_client;
	char write_data[2] = { 0 };
	write_data[0] = addr;
	write_data[1] = value;

	LCM_LOGI("OCP2131 write enter !!\n");

	if(OCP2131_i2c_client == NULL)
		return 0;

	ret = i2c_master_send(client, write_data, 2);
	if (ret < 0)
		LCM_LOGI("OCP2131 write data fail !!\n");
	return ret;
}
EXPORT_SYMBOL(OCP2131_write_bytes);

static int __init OCP2131_iic_init(void)
{
	int ret = 0;

	LCM_LOGI("OCP2131_iic_init\n");
	ret = i2c_add_driver(&OCP2131_iic_driver);
	if (ret < 0)
	{
		LCM_LOGI("OCP2131 i2c driver add fail !!\n");
		return ret ;
	}

	LCM_LOGI("OCP2131_iic_init success\n");
	return 0;
}

static void __exit OCP2131_iic_exit(void)
{
	LCM_LOGI("OCP2131_iic_exit\n");
	i2c_del_driver(&OCP2131_iic_driver);
}

module_init(OCP2131_iic_init);
module_exit(OCP2131_iic_exit);
MODULE_DESCRIPTION("SPRD OCP2131 I2C Driver");
MODULE_AUTHOR("TS");
MODULE_LICENSE("GPL");
