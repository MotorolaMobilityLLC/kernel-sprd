#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include "ats_core.h"

#define TS_I2C_MAX_RETRY 3
#define TS_I2C_RETRY_MSLEEP 10
#define TS_DEFAULT_SLAVE_REG_WIDTH 1
#define TS_MAX_SLAVE_REG_WIDTH 2
#define TS_I2C_MAX_BUF_LEN 250

struct i2c_client *g_i2c_client;
DEFINE_MUTEX(i2c_mutex);

/*
 * send msgs to adapter, retry must not be negative
 */
static inline int ts_send_retry(
	struct i2c_adapter *adapter, struct i2c_msg *msgs, int num, int retry)
{
	int tx_len, re = retry;

	if (!adapter || !msgs || num < 0 || retry < 0)
		return 0;

	while (re--) {
		mutex_lock(&i2c_mutex);
		//tx_len = i2c_transfer(adapter, msgs, num);
		if (num == 1) {
			tx_len = i2c_transfer(adapter, msgs, num);
		} else {
			tx_len = i2c_transfer(adapter, msgs, 1);
			tx_len += i2c_transfer(adapter, &msgs[1], 1);
		}
		mutex_unlock(&i2c_mutex);

		if (tx_len == num)
			return tx_len;

		TS_WARN("I2C failed %d times", retry - re);
		msleep(TS_I2C_RETRY_MSLEEP);
	}

	TS_ERR("I2C failed over limits");
	return tx_len;
}

static inline int ts_assign_reg(
	unsigned char *buf, unsigned short reg, int width)
{
	if (width == 1) {
		buf[0] = reg & 0xFF;
	} else {
		buf[0] = (reg >> 8) & 0xFF;
		buf[1] = reg & 0xFF;
	}

	return width;
}

static int ts_i2c_simple_read(unsigned char *data, unsigned short length)
{
	int rx_len;
	struct i2c_client *i2c = g_i2c_client;
	struct i2c_msg msgs[] = {
		{
			.flags = I2C_M_RD,
			.buf = data,
			.len = length,
		},
	};

	if (!i2c) {
		TS_ERR("I2C dev not ready");
		return -EIO;
	}

	msgs[0].addr = i2c->addr;

	mutex_lock(&i2c_mutex);
	rx_len = i2c_transfer(i2c->adapter, msgs, ARRAY_SIZE(msgs));
	mutex_unlock(&i2c_mutex);

	return rx_len == ARRAY_SIZE(msgs) ? length : -EIO;
}

static int ts_i2c_simple_write(unsigned char *data, unsigned short length)
{
	int tx_len;
	struct i2c_client *i2c = g_i2c_client;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
			.buf = data,
			.len = length,
		},
	};

	if (!i2c) {
		TS_ERR("I2C dev not ready");
		return -EIO;
	}

	msgs[0].addr = i2c->addr;

	mutex_lock(&i2c_mutex);
	tx_len = i2c_transfer(i2c->adapter, msgs, ARRAY_SIZE(msgs));
	mutex_unlock(&i2c_mutex);

	return tx_len == ARRAY_SIZE(msgs) ? length : -EIO;
}

static int ts_i2c_simple_read_reg(
	unsigned short reg, unsigned char *data, unsigned short length)
{
	int tx;
	unsigned char addr_buf[TS_MAX_SLAVE_REG_WIDTH];
	struct i2c_client *i2c = g_i2c_client;
	struct ts_bus_access *bus;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
			.buf = addr_buf,
		},
		{
			.flags = I2C_M_RD,
			.buf = data,
			.len = length,
		},
	};

	if (!i2c) {
		TS_ERR("I2C dev not ready");
		return -EIO;
	}

	bus = (struct ts_bus_access *)i2c_get_clientdata(i2c);
	if (bus->reg_width != 1 && bus->reg_width != 2) {
		TS_ERR("bad reg width: %u", bus->reg_width);
		return -EIO;
	}

	msgs[0].addr = i2c->addr;
	msgs[0].len = ts_assign_reg(addr_buf, reg, bus->reg_width);
	msgs[1].addr = i2c->addr;

	tx = ts_send_retry(i2c->adapter, msgs, ARRAY_SIZE(msgs), 1);

	return tx == ARRAY_SIZE(msgs) ? length : tx;
}

static int ts_i2c_simple_write_reg(
	unsigned short reg,	unsigned char *data, unsigned short length)
{
	int tx;
	unsigned char *tx_buf;
	struct i2c_client *i2c;
	struct ts_bus_access *bus;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
		},
	};

	i2c = g_i2c_client;
	if (!i2c) {
		TS_ERR("I2C dev not ready");
		return -EIO;
	}

	bus = (struct ts_bus_access *)i2c_get_clientdata(i2c);
	if (bus->reg_width != 1 && bus->reg_width != 2) {
		TS_ERR("bad reg width: %u", bus->reg_width);
		return -EIO;
	}

	tx_buf = devm_kzalloc(&i2c->dev, length + bus->reg_width, GFP_KERNEL);
	if (IS_ERR_OR_NULL(tx_buf)) {
		TS_ERR("failed to allocate memory");
		return -ENOMEM;
	}

	msgs[0].addr = i2c->addr;
	msgs[0].buf = tx_buf;
	msgs[0].len = length + ts_assign_reg(tx_buf, reg, bus->reg_width);
	memcpy(tx_buf + bus->reg_width, data, length);

	tx = ts_send_retry(i2c->adapter, msgs, ARRAY_SIZE(msgs), 1);

	return tx == ARRAY_SIZE(msgs) ? length : tx;
}

static int ts_i2c_read(
	unsigned short reg, unsigned char *data, unsigned short length)
{
	int tx;
	unsigned short index = 0, tx_length, max_length;
	unsigned char addr_buf[TS_I2C_MAX_BUF_LEN];
	struct ts_bus_access *bus;
	struct i2c_client *i2c = g_i2c_client;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
			.buf = addr_buf,
		},
		{
			.flags = I2C_M_RD,
		}
	};

	if (!i2c) {
		TS_ERR("I2C dev not ready");
		return -EIO;
	}

	bus = (struct ts_bus_access *)i2c_get_clientdata(i2c);
	if (bus->reg_width != 1 && bus->reg_width != 2) {
		TS_ERR("bad reg width: %u", bus->reg_width);
		return -EIO;
	}

	max_length = TS_I2C_MAX_BUF_LEN - bus->reg_width;
	msgs[0].addr = i2c->addr;
	msgs[0].len = bus->reg_width;
	msgs[1].addr = i2c->addr;

	while (index < length) {
		tx_length = length - index;
		if (tx_length > max_length)
			tx_length = max_length;

		ts_assign_reg(addr_buf, reg, bus->reg_width);
		msgs[1].len = tx_length;
		msgs[1].buf = data;

		tx = ts_send_retry(i2c->adapter,
			msgs, ARRAY_SIZE(msgs), TS_I2C_MAX_RETRY);
		if (tx != ARRAY_SIZE(msgs)) {
			TS_ERR("I2C read error: %d", tx);
			return tx;
		}

		index += tx_length;
		reg += tx_length;
		data += tx_length;
	}

	return length;
}

static int ts_i2c_write(
	unsigned short reg,	unsigned char *data, unsigned short length)
{
	int tx;
	unsigned short index = 0, tx_length, max_length;
	unsigned char buf[TS_I2C_MAX_BUF_LEN];
	struct ts_bus_access *bus;
	struct i2c_client *i2c = g_i2c_client;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
			.buf = buf,
		},
	};

	if (!i2c) {
		TS_ERR("I2C is not ready, please wait a moment.");
		return -EIO;
	}

	bus = (struct ts_bus_access *)i2c_get_clientdata(i2c);
	if (bus->reg_width != 1 && bus->reg_width != 2) {
		TS_ERR("bad reg width: %u", bus->reg_width);
		return -EIO;
	}

	max_length = TS_I2C_MAX_BUF_LEN - bus->reg_width;
	msgs[0].addr = i2c->addr;

	while (index < length) {
		tx_length = length - index;
		if (tx_length > max_length)
			tx_length = max_length;

		ts_assign_reg(buf, reg, bus->reg_width);
		memcpy(buf + bus->reg_width, data + index, tx_length);
		msgs[0].len = tx_length + bus->reg_width;

		tx = ts_send_retry(i2c->adapter,
			msgs, ARRAY_SIZE(msgs), TS_I2C_MAX_RETRY);
		if (tx != ARRAY_SIZE(msgs)) {
			TS_ERR("I2C read error: %d", tx);
			return tx;
		}

		index += tx_length;
		reg += tx_length;
	}
	return length;
}

static int ts_i2c_read_fw(
	unsigned short reg, unsigned char *data, unsigned short length)
{
	int tx;
	int j;
	unsigned short index = 0, tx_length, max_length;
	unsigned char addr_buf[TS_I2C_MAX_BUF_LEN];
	struct ts_bus_access *bus;
	struct i2c_client *i2c = g_i2c_client;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
			.buf = addr_buf,
		},
		{
			.flags = I2C_M_RD,
		}
	};

	if (!i2c) {
		TS_ERR("I2C dev not ready");
		return -EIO;
	}

	bus = (struct ts_bus_access *)i2c_get_clientdata(i2c);

	bus->reg_width = 2;
		if(length <= 4) {
		for(j = 0; j < length; j++) {
			printk("entry:ts_i2c_read_fw:%x\n",data[j]);
		}
	}
	if (bus->reg_width != 1 && bus->reg_width != 2) {
		TS_ERR("bad reg width: %u", bus->reg_width);
		return -EIO;
	}

	max_length = TS_I2C_MAX_BUF_LEN - bus->reg_width;
	msgs[0].addr = i2c->addr;
	msgs[0].len = bus->reg_width;
	msgs[1].addr = i2c->addr;

	while (index < length) {
		tx_length = length - index;
		if (tx_length > max_length)
			tx_length = max_length;

		ts_assign_reg(addr_buf, reg, bus->reg_width);
		msgs[1].len = tx_length;
		msgs[1].buf = data;

		tx = ts_send_retry(i2c->adapter,
			msgs, ARRAY_SIZE(msgs), TS_I2C_MAX_RETRY);
		if (tx != ARRAY_SIZE(msgs)) {
			TS_ERR("I2C read error: %d", tx);
			return tx;
		}

		index += tx_length;
		reg += tx_length;
		data += tx_length;
	}

	bus->reg_width = 1;

	return length;
}

static int ts_i2c_write_fw(
	unsigned short reg,	unsigned char *data, unsigned short length)
{
	int tx;
	int j;
	unsigned short index = 0, tx_length, max_length;
	unsigned char buf[TS_I2C_MAX_BUF_LEN];
	struct ts_bus_access *bus;
	struct i2c_client *i2c = g_i2c_client;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
			.buf = buf,
		},
	};

	if (!i2c) {
		TS_ERR("I2C is not ready, please wait a moment.");
		return -EIO;
	}

	bus = (struct ts_bus_access *)i2c_get_clientdata(i2c);

	bus->reg_width = 2;

	if(length <= 4) {
		for(j = 0; j < length; j++) {
			printk("entry:ts_i2c_write_fw:%x\n",data[j]);
		}
	}

	if (bus->reg_width != 1 && bus->reg_width != 2) {
		TS_ERR("bad reg width: %u", bus->reg_width);
		return -EIO;
	}
	printk("entry:ts_i2c_write111:%d\n",bus->reg_width);
	max_length = TS_I2C_MAX_BUF_LEN - bus->reg_width;
	msgs[0].addr = i2c->addr;

	while (index < length) {
		tx_length = length - index;
		if (tx_length > max_length)
			tx_length = max_length;

		ts_assign_reg(buf, reg, bus->reg_width);
		memcpy(buf + bus->reg_width, data + index, tx_length);
		msgs[0].len = tx_length + bus->reg_width;
    		printk("entry:ts_i2c_write222:%d\n",bus->reg_width);
		tx = ts_send_retry(i2c->adapter,
			msgs, ARRAY_SIZE(msgs), TS_I2C_MAX_RETRY);
		if (tx != ARRAY_SIZE(msgs)) {
			TS_ERR("I2C write error: %d", tx);
			return tx;
		}

		index += tx_length;
		reg += tx_length;
	}

	bus->reg_width = 1;
	return length;
}

static struct ts_bus_access ts_i2c_bus_access = {
	.simple_read = ts_i2c_simple_read,
	.simple_write = ts_i2c_simple_write,
	.read = ts_i2c_read,
	.write = ts_i2c_write,
	.read_fw = ts_i2c_read_fw,
	.write_fw = ts_i2c_write_fw,
	.simple_read_reg = ts_i2c_simple_read_reg,
	.simple_write_reg = ts_i2c_simple_write_reg,
	.reg_width = TS_DEFAULT_SLAVE_REG_WIDTH,
	.bus_type = TSBUS_I2C,
};

static int ts_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		TS_ERR("I2C check functionality fail!");
		return -ENODEV;
	}

	ts_i2c_bus_access.client_addr = client->addr;
	i2c_set_clientdata(client, &ts_i2c_bus_access);
	ts_register_bus_dev(&client->dev);
	g_i2c_client = client;

	TS_INFO("I2C device probe OK");
	return 0;
}

static int ts_i2c_remove(struct i2c_client *client)
{
	ts_unregister_bus_dev();
	i2c_set_clientdata(client, NULL);
	g_i2c_client = NULL;

	return 0;
}

static const struct i2c_device_id ts_i2c_ids[] = {
	{ ATS_I2C_DEV, 0 },
	{ }
};

static const struct dev_pm_ops irq_resume_early = {
	.suspend_late = ts_suspend_late,
	.resume_early = ts_resume_early,
};

static const struct of_device_id ts_i2c_matchs[] = {
	{ .compatible = "adaptive-touchscreen", },//ATS_COMPATIBLE, },
	{ }
};

static struct i2c_driver ts_i2c_driver = {
	.probe		= ts_i2c_probe,
	.remove		= ts_i2c_remove,
	.id_table	= ts_i2c_ids,
	.driver	= {
		.name	= ATS_I2C_DEV,
		.owner	= THIS_MODULE,
		.of_match_table = ts_i2c_matchs,
		.pm = &irq_resume_early,
	},
};

int ts_i2c_init(struct device_node *pn, unsigned short *addrs)
{
	struct i2c_adapter *adap = NULL;
	struct i2c_board_info i2c_info;
	struct i2c_client *client = NULL;

	TS_DBG("init i2c in %sadaptive mode", addrs ? "" : "non-");

	if (addrs != NULL) {
		/* works in adaptive mode */
		adap = of_find_i2c_adapter_by_node(pn);
		if (adap == NULL) {
			TS_ERR("no adapter found on this i2c!");
			return -ENODEV;
		}

		memset(&i2c_info, 0, sizeof(struct i2c_board_info));
		strlcpy(i2c_info.type, ATS_I2C_DEV, I2C_NAME_SIZE);
		client = i2c_new_probed_device(adap, &i2c_info, addrs, NULL);
		if (client == NULL) {
			TS_ERR("i2c instantiating device failed");
			put_device(&adap->dev);
			return -ENODEV;
		}

		put_device(&adap->dev);
	}

	return i2c_add_driver(&ts_i2c_driver);
}

void ts_i2c_exit(void)
{
	i2c_del_driver(&ts_i2c_driver);
}

MODULE_AUTHOR("joseph.cai@spreadtrum.com");
MODULE_DESCRIPTION("Spreadtrum touchscreen i2c driver");
MODULE_LICENSE("GPL");
