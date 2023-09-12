#include <linux/delay.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include "ats_core.h"

uint8_t *spi_txbuf;
static uint8_t spi_rxbuf[128]={0};//spi_rxbuf[1025]={0};
struct mutex spi_mutex;
struct spi_device *g_spi_client;

//---SPI READ/WRITE---
#define SPI_WRITE_MASK(a)	(a | 0x80)
#define SPI_READ_MASK(a)	(a & 0x7F)

#define DUMMY_BYTES (1)
#define SPI_TRANSFER_LEN	(128)//(63*1024) binhua notice heer
#define TS_DEFAULT_SLAVE_REG_WIDTH 1

typedef enum {
	NVTWRITE = 0,
	NVTREAD  = 1
} NVT_SPI_RW;

static int ts_spi_write(
	unsigned short reg,	unsigned char *buf, unsigned short len);
static int ts_spi_read(
	unsigned short reg,	unsigned char *buf, unsigned short len);
/*******************************************************
Description:
	Novatek touchscreen spi read/write core function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static inline int32_t spi_read_write(struct spi_device *client, uint8_t *buf, size_t len , NVT_SPI_RW rw)
{
	struct spi_message m;
	struct spi_transfer t = {
		.len    = len,
	};

	memcpy(spi_txbuf, buf, len + DUMMY_BYTES);

	switch (rw) {
		case NVTREAD:
			t.tx_buf = spi_txbuf;
			t.rx_buf = spi_rxbuf;
			t.len    = (len + DUMMY_BYTES);
			break;

		case NVTWRITE:
			t.tx_buf = spi_txbuf;
			break;
	}

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	TS_ERR("spi_read_write ats_spi\n");
	return spi_sync(client, &m);
	//return 0;
}

static int ts_spi_simple_read(unsigned char *buf, unsigned short len)
{
	int32_t ret = -1;
	int32_t retries = 0;
	struct spi_device *client = g_spi_client;

	mutex_lock(&spi_mutex);

	buf[0] = SPI_READ_MASK(buf[0]);

	while (retries < 5) {
		ret = spi_read_write(client, buf, len, NVTREAD);
		if (ret == 0) break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		TS_ERR("read error, ret=%d\n", ret);
		ret = -EIO;
	} else {
		memcpy((buf+1), (spi_rxbuf+2), (len-1));
	}

	mutex_unlock(&spi_mutex);

	return len;
}

static int ts_spi_simple_write(unsigned char *buf, unsigned short len)
{
	int32_t ret = -1;
	int32_t retries = 0;
	struct spi_device *client = g_spi_client;

	mutex_lock(&spi_mutex);

	buf[0] = SPI_WRITE_MASK(buf[0]);

	while (retries < 5) {
		ret = spi_read_write(client, buf, len, NVTWRITE);
		if (ret == 0)	break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		TS_ERR("error, ret=%d\n", ret);
		ret = -EIO;
	}

	mutex_unlock(&spi_mutex);

	return len;
}

static int ts_spi_read_fw(
	unsigned short reg, unsigned char *data, unsigned short length)
{

	return length;
}

static int ts_spi_write_fw(
	unsigned short reg,	unsigned char *data, unsigned short length)
{

	return length;
}

static int ts_spi_read(
	unsigned short reg,	unsigned char *buf, unsigned short len)
{
	int32_t ret = -1;
	int32_t retries = 0;
	struct spi_device *client = g_spi_client;

	//mutex_lock(&spi_mutex);

	buf[0] = SPI_READ_MASK(buf[0]);

	while (retries < 5) {
		ret = spi_read_write(client, buf, len, NVTREAD);
		if (ret == 0) break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		TS_ERR("read error, ret=%d\n", ret);
		ret = -EIO;
	} else {
		memcpy((buf+1), (spi_rxbuf+2), (len-1));
	}

	//mutex_unlock(&spi_mutex);

	return len;
}

static int ts_spi_write(
	unsigned short reg,	unsigned char *buf, unsigned short len)
{
	int32_t ret = -1;
	int32_t retries = 0;
	struct spi_device *client = g_spi_client;

	//mutex_lock(&spi_mutex);

	buf[0] = SPI_WRITE_MASK(buf[0]);

	while (retries < 5) {
		ret = spi_read_write(client, buf, len, NVTWRITE);
		if (ret == 0)	break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		TS_ERR("error, ret=%d\n", ret);
		ret = -EIO;
	}

	//mutex_unlock(&spi_mutex);

	return len;
}

static int ts_spi_read_addr(
	unsigned short addr,	unsigned char *data, unsigned short len)
{
	int32_t ret = 0;
	unsigned char buf[4] = {0};
	//struct spi_device *client = g_spi_client;

	mutex_lock(&spi_mutex);
	//---set xdata index---
	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 15) & 0xFF;
	buf[2] = (addr >> 7) & 0xFF;
	ret = ts_spi_write(0x01, buf, 3);
	if (!ret) {
		TS_ERR("set page 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	//---write data to index---
	buf[0] = addr & (0x7F);
	buf[1] = *data;
	ret = ts_spi_read(0x01, buf, 2);
	if (!ret) {
		TS_ERR("write data to 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	mutex_unlock(&spi_mutex);

	return ret;
}

static int ts_spi_write_addr(
	unsigned short addr,	unsigned char *data, unsigned short len)
{
	int32_t ret = 0;
	unsigned char buf[4] = {0};
	//struct spi_device *client = g_spi_client;

	mutex_lock(&spi_mutex);
	//---set xdata index---
	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 15) & 0xFF;
	buf[2] = (addr >> 7) & 0xFF;
	ret = ts_spi_write(0x01, buf, 3);
	if (!ret) {
		TS_ERR("set page 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	//---write data to index---
	buf[0] = addr & (0x7F);
	buf[1] = *data;
	ret = ts_spi_write(0x01, buf, 2);
	if (!ret) {
		TS_ERR("write data to 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	mutex_unlock(&spi_mutex);

	return ret;
}
static struct ts_bus_access ts_spi_bus_access = {

	.simple_read = ts_spi_simple_read,
	.simple_write = ts_spi_simple_write,
	.read = ts_spi_read_addr,
	.write = ts_spi_write_addr,
	.read_fw = ts_spi_read_fw,
	.write_fw = ts_spi_write_fw,
	.simple_read_reg = ts_spi_read_addr,
	.simple_write_reg = ts_spi_write_addr,
	.reg_width = TS_DEFAULT_SLAVE_REG_WIDTH,
	.bus_type = TSBUS_SPI,
};

static int ts_spi_probe(struct spi_device *client)
{
	int32_t ret = 0;

	spi_txbuf = (uint8_t *)kzalloc((SPI_TRANSFER_LEN+1), GFP_KERNEL);
	if(spi_txbuf == NULL) {
		TS_ERR("kzalloc for spi_xbuf failed!\n");
		return -ENOMEM;
	}
	mutex_init(&spi_mutex);

	spi_set_drvdata(client, &ts_spi_bus_access);

	//---prepare for spi parameter---
	if (client->master->flags & SPI_MASTER_HALF_DUPLEX) {
		TS_ERR("Full duplex not supported by master\n");
		ret = -EIO;
		return -ENODEV;
	}

	client->bits_per_word = 8;
	client->mode = SPI_MODE_0;
	ret = spi_setup(client);
	if (ret < 0) {
		TS_ERR("Failed to perform SPI setup\n");
		return -ENODEV;
	}

	ts_register_bus_dev(&client->dev);
	g_spi_client = client;

	TS_INFO("SPI device probe OK");

	return 0;
}

static int ts_spi_remove(struct spi_device *client)
{
	ts_unregister_bus_dev();
	spi_set_drvdata(client, NULL);
	g_spi_client = NULL;

	return 0;
}

static const struct spi_device_id ts_spi_ids[] = {
	{ ATS_SPI_DEV, 0 },
	{ }
};

static const struct of_device_id ts_spi_matchs[] = {
	{ .compatible = ATS_COMPATIBLE, },
	{ }
};

static const struct dev_pm_ops irq_resume_early = {
	.suspend_late = ts_suspend_late,
	.resume_early = ts_resume_early,
};

static struct spi_driver ts_spi_driver = {
	.probe	= ts_spi_probe,
	.remove	= ts_spi_remove,
	.id_table	= ts_spi_ids,
	.driver	= {
		.name  = ATS_SPI_DEV,
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = ts_spi_matchs,
		.pm = &irq_resume_early,
	},
};

int ts_spi_init(void)
{
	int32_t ret = 0;
	//---add spi driver---
	ret = spi_register_driver(&ts_spi_driver);
	if (ret) {
		TS_ERR("failed to add spi driver");
	}

	return ret;
}

void ts_spi_exit(void)
{
	spi_unregister_driver(&ts_spi_driver);
}
