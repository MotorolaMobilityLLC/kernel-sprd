/*
 * Copyright (C) 2010 - 2018 Novatek, Inc.
 *
 * $Revision: 47247 $
 * $Date: 2019-07-10 10:41:36 +0800 (Wed, 10 Jul 2019) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include "gesture.h"

//#include <linux/wakelock.h>

#if defined(CONFIG_ADF)
#include <linux/notifier.h>
#include <video/adf_notifier.h>
#endif

#if defined(CONFIG_FB)
#ifdef CONFIG_DRM_MSM
//#include <linux/msm_drm_notify.h>
#endif
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#if defined(CONFIG_TPD_SENSORHUB)
#include <linux/shub_api.h>
#endif

#include"ats_nt36xxx_spi.h"

#if NVT_TOUCH_ESD_PROTECT
#include <linux/jiffies.h>
#endif /* #if NVT_TOUCH_ESD_PROTECT */

struct nvt_ts_data *ts;
int g_fw_control=0;
uint32_t ENG_RST_ADDR  = 0x7FFF80;
//uint32_t SWRST_N8_ADDR = 0; //read from dtsi
//uint32_t SPI_RD_FAST_ADDR = 0;	//read from dtsi

#define 	POINT_DATA_LEN 65
#define	CMD_ENTER_COMMON_USB_PLUGOUT 	0x51
#define	CMD_ENTER_COMMON_USB_PLUGIN		0x53
#define GESTURE_C          12
#define GESTURE_W          13
#define GESTURE_V          14
#define GESTURE_DOUBLECLICK    15
#define GESTURE_Z          16
#define GESTURE_M          17
#define GESTURE_O          18
#define GESTURE_E          19
#define GESTURE_S          20
#define GESTURE_UP        21
#define GESTURE_DOWN      22
#define GESTURE_LEFT      23
#define GESTURE_RIGHT     24
/* customized gesture id */
#define DATA_PROTOCOL           30

/* function page definition */
#define FUNCPAGE_GESTURE         1

struct msg_gesture_map  nt36xxx_gestures_maps[] = {
	{0,	GESTURE_LEFT,   GESTURE_LF, 	0xD1,	0},
	{0,	GESTURE_UP,     GESTURE_up,		0xD1,	2},
	{0,	GESTURE_RIGHT,	GESTURE_RT,	    0xD1,	1},
	{0,	GESTURE_DOWN,	GESTURE_down,	0xD1,	3},
	{0,	GESTURE_DOUBLECLICK,    GESTURE_DC,	    0xD1,	4},
	{0,	GESTURE_W,	GESTURE_w,		0xD2,	1},
	{0,	GESTURE_O,	GESTURE_o,		0xD2,	0},
	{0,	GESTURE_M,	GESTURE_m,		0xD2,	2},
	{0,	GESTURE_E,	GESTURE_e,		0xD2,	3},
	{0,	GESTURE_C,	GESTURE_c,		0xD2,	4},
	{0,	GESTURE_Z,	GESTURE_z,		0xD5,	1},
	{0,	GESTURE_S,	GESTURE_s,		0xD5,	6},
	{0,	GESTURE_V,	GESTURE_v,		0xD6,	4},
};
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

	memcpy(ts->xbuf, buf, len + DUMMY_BYTES);
	switch (rw) {
		case NVTREAD:
			t.tx_buf = ts->xbuf;
			t.rx_buf = ts->rbuf;
			t.len    = (len + DUMMY_BYTES);
			break;

		case NVTWRITE:
			t.tx_buf = ts->xbuf;
			break;
	}

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	TS_ERR("spi_read_write nt36xxx_spi\n");
	return spi_sync(client, &m);
	//return 0;
}

/*******************************************************
Description:
	Novatek touchscreen spi read function.

return:
	Executive outcomes. 2---succeed. -5---I/O error
*******************************************************/
int32_t nvt_spi_read(struct spi_device *client, uint8_t *buf, uint16_t len)
{
	int32_t ret = -1;
	int32_t retries = 0;

	mutex_lock(&ts->xbuf_lock);

	buf[0] = SPI_READ_MASK(buf[0]);

	while (retries < 5) {
		ret = spi_read_write(client, buf, len, NVTREAD);
		if (ret == 0) break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		NVT_ERR("read error, ret=%d\n", ret);
		ret = -EIO;
	} else {
		memcpy((buf+1), (ts->rbuf+2), (len-1));
	}

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen spi write function.

return:
	Executive outcomes. 1---succeed. -5---I/O error
*******************************************************/
int32_t nvt_spi_write(struct spi_device *client, uint8_t *buf, uint16_t len)
{
	int32_t ret = -1;
	int32_t retries = 0;

	mutex_lock(&ts->xbuf_lock);

	buf[0] = SPI_WRITE_MASK(buf[0]);

	while (retries < 5) {
		ret = spi_read_write(client, buf, len, NVTWRITE);
		if (ret == 0)	break;
		retries++;
	}

	if (unlikely(retries == 5)) {
		NVT_ERR("error, ret=%d\n", ret);
		ret = -EIO;
	}

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen set index/page/addr address.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_set_page(uint32_t addr)
{
	uint8_t buf[4] = {0};

	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 15) & 0xFF;
	buf[2] = (addr >> 7) & 0xFF;

	return nvt_spi_write(ts->client, buf, 3);
}

/*******************************************************
Description:
	Novatek touchscreen write data to specify address.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_write_addr(uint32_t addr, uint8_t data)
{
	int32_t ret = 0;
	uint8_t buf[4] = {0};

	//---set xdata index---
	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 15) & 0xFF;
	buf[2] = (addr >> 7) & 0xFF;
	ret = nvt_spi_write(ts->client, buf, 3);
	if (ret) {
		NVT_ERR("set page 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	//---write data to index---
	buf[0] = addr & (0x7F);
	buf[1] = data;
	ret = nvt_spi_write(ts->client, buf, 2);
	if (ret) {
		NVT_ERR("write data to 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen enable hw bld crc function.

return:
	N/A.
*******************************************************/
void nvt_bld_crc_enable(void)
{
	uint8_t buf[4] = {0};

	//---set xdata index to BLD_CRC_EN_ADDR---
	nvt_set_page(ts->mmap->BLD_CRC_EN_ADDR);

	//---read data from index---
	buf[0] = ts->mmap->BLD_CRC_EN_ADDR & (0x7F);
	buf[1] = 0xFF;
	nvt_spi_read(ts->client, buf, 2);

	//---write data to index---
	buf[0] = ts->mmap->BLD_CRC_EN_ADDR & (0x7F);
	buf[1] = buf[1] | (0x01 << 7);
	nvt_spi_write(ts->client, buf, 2);
}

/*******************************************************
Description:
	Novatek touchscreen clear status & enable fw crc function.

return:
	N/A.
*******************************************************/
void nvt_fw_crc_enable(void)
{
	uint8_t buf[4] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	//---clear fw reset status---
	buf[0] = EVENT_MAP_RESET_COMPLETE & (0x7F);
	buf[1] = 0x00;
	nvt_spi_write(ts->client, buf, 2);

	//---enable fw crc---
	buf[0] = EVENT_MAP_HOST_CMD & (0x7F);
	buf[1] = 0xAE;	//enable fw crc command
	nvt_spi_write(ts->client, buf, 2);
}

/*******************************************************
Description:
	Novatek touchscreen set boot ready function.

return:
	N/A.
*******************************************************/
void nvt_boot_ready(void)
{
	//---write BOOT_RDY status cmds---
	nvt_write_addr(ts->mmap->BOOT_RDY_ADDR, 1);

	mdelay(5);

	if (!ts->hw_crc) {
		//---write BOOT_RDY status cmds---
		nvt_write_addr(ts->mmap->BOOT_RDY_ADDR, 0);

		//---write POR_CD cmds---
		nvt_write_addr(ts->mmap->POR_CD_ADDR, 0xA0);
	}
}

/*******************************************************
Description:
	Novatek touchscreen eng reset cmd
    function.

return:
	n.a.
*******************************************************/
void nvt_eng_reset(void)
{
	//---eng reset cmds to ENG_RST_ADDR---
	nvt_write_addr(ENG_RST_ADDR, 0x5A);

	mdelay(1);	//wait tMCU_Idle2TP_REX_Hi after TP_RST
}

/*******************************************************
Description:
	Novatek touchscreen reset MCU
    function.

return:
	n.a.
*******************************************************/
void nvt_sw_reset(void)
{
	//---software reset cmds to SWRST_N8_ADDR---
	nvt_write_addr(ts->swrst_n8_addr, 0x55);

	msleep(10);
}

/*******************************************************
Description:
	Novatek touchscreen reset MCU then into idle mode
    function.

return:
	n.a.
*******************************************************/
void nvt_sw_reset_idle(void)
{
	//---MCU idle cmds to SWRST_N8_ADDR---
	nvt_write_addr(ts->swrst_n8_addr, 0xAA);

	msleep(15);
}

/*******************************************************
Description:
	Novatek touchscreen reset MCU (boot) function.

return:
	n.a.
*******************************************************/
void nvt_bootloader_reset(void)
{
	//---reset cmds to SWRST_N8_ADDR---
	nvt_write_addr(ts->swrst_n8_addr, 0x69);

	mdelay(5);	//wait tBRST2FR after Bootload RST

	if (ts->spi_rd_fast_addr) {
		/* disable SPI_RD_FAST */
		nvt_write_addr(ts->spi_rd_fast_addr, 0x00);
	}
}

/*******************************************************
Description:
	Novatek touchscreen clear FW status function.

return:
	Executive outcomes. 0---succeed. -1---fail.
*******************************************************/
int32_t nvt_clear_fw_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 20;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---clear fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		nvt_spi_write(ts->client, buf, 2);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0xFF;
		nvt_spi_read(ts->client, buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen check FW status function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
int32_t nvt_check_fw_status(void)
{
	uint8_t buf[8] = {0};
	int32_t i = 0;
	const int32_t retry = 50;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		nvt_spi_read(ts->client, buf, 2);

		if ((buf[1] & 0xF0) == 0xA0)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen check FW reset state function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;
	int32_t retry = 0;
	int32_t retry_max = (check_reset_state == RESET_STATE_INIT) ? 10 : 50;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_RESET_COMPLETE);

	while (1) {
		//---read reset state---
		buf[0] = EVENT_MAP_RESET_COMPLETE;
		buf[1] = 0x00;
		nvt_spi_read(ts->client, buf, 6);

		if ((buf[1] >= check_reset_state) && (buf[1] <= RESET_STATE_MAX)) {
			ret = 0;
			break;
		}

		retry++;
		if(unlikely(retry > retry_max)) {
			NVT_ERR("error, retry=%d, buf[1]=0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
				retry, buf[1], buf[2], buf[3], buf[4], buf[5]);
			ret = -1;
			break;
		}

		usleep_range(10000, 10000);
	}

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen get novatek project id information
	function.

return:
	Executive outcomes. 0---success. -1---fail.
*******************************************************/
int32_t nvt_read_pid(void)
{
	uint8_t buf[4] = {0};
	int32_t ret = 0;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_PROJECTID);

	//---read project id---
	buf[0] = EVENT_MAP_PROJECTID;
	buf[1] = 0x00;
	buf[2] = 0x00;
	nvt_spi_read(ts->client, buf, 3);

	ts->nvt_pid = (buf[2] << 8) + buf[1];

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	NVT_LOG("PID=%04X\n", ts->nvt_pid);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen get firmware related information
	function.

return:
	Executive outcomes. 0---success. -1---fail.
*******************************************************/
int32_t nvt_get_fw_info(void)
{
	uint8_t buf[64] = {0};
	uint32_t retry_count = 0;
	int32_t ret = 0;

info_retry:
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);

	//---read fw info---
	buf[0] = EVENT_MAP_FWINFO;
	nvt_spi_read(ts->client, buf, 17);
	ts->fw_ver = buf[1];
	ts->x_num = buf[3];
	ts->y_num = buf[4];
	//ts->abs_x_max = (uint16_t)((buf[5] << 8) | buf[6]);
	//ts->abs_y_max = (uint16_t)((buf[7] << 8) | buf[8]);
	ts->max_button_num = buf[11];

	//---clear x_num, y_num if fw info is broken---
	if ((buf[1] + buf[2]) != 0xFF) {
		NVT_ERR("FW info is broken! fw_ver=0x%02X, ~fw_ver=0x%02X\n", buf[1], buf[2]);
		ts->fw_ver = 0;
		ts->x_num = 18;
		ts->y_num = 32;
		//ts->abs_x_max = TOUCH_DEFAULT_MAX_WIDTH;
		//ts->abs_y_max = TOUCH_DEFAULT_MAX_HEIGHT;
		ts->max_button_num = TOUCH_KEY_NUM;

		if(retry_count < 3) {
			retry_count++;
			NVT_ERR("retry_count=%d\n", retry_count);
			goto info_retry;
		} else {
			NVT_ERR("Set default fw_ver=%d, x_num=%d, y_num=%d, "
					"abs_x_max=%d, abs_y_max=%d, max_button_num=%d!\n",
					ts->fw_ver, ts->x_num, ts->y_num,
					ts->abs_x_max, ts->abs_y_max, ts->max_button_num);
			ret = -1;
		}
	} else {
		ret = 0;
	}

	NVT_LOG("fw_ver = 0x%02X, fw_type = 0x%02X\n", ts->fw_ver, buf[14]);

	//---Get Novatek PID---
	nvt_read_pid();

	return ret;
}

/*******************************************************
  Create Device Node (Proc Entry)
*******************************************************/
#if NVT_TOUCH_PROC
static struct proc_dir_entry *NVT_proc_entry;
#define DEVICE_NAME	"NVTSPI"

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI read function.

return:
	Executive outcomes. 2---succeed. -5,-14---failed.
*******************************************************/
static ssize_t nvt_flash_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
	uint8_t *str = NULL;
	int32_t ret = 0;
	int32_t retries = 0;
	int8_t spi_wr = 0;
	uint8_t *buf;

	if ((count > NVT_TRANSFER_LEN + 3) || (count < 3)) {
		NVT_ERR("invalid transfer len!\n");
		return -EFAULT;
	}

	/* allocate buffer for spi transfer */
	str = (uint8_t *)kzalloc((count), GFP_KERNEL);
	if(str == NULL) {
		NVT_ERR("kzalloc for buf failed!\n");
		ret = -ENOMEM;
		goto kzalloc_failed;
	}

	buf = (uint8_t *)kzalloc((count), GFP_KERNEL | GFP_DMA);
	if(buf == NULL) {
		NVT_ERR("kzalloc for buf failed!\n");
		ret = -ENOMEM;
		kfree(str);
		str = NULL;
		goto kzalloc_failed;
	}

	if (copy_from_user(str, buff, count)) {
		NVT_ERR("copy from user error\n");
		ret = -EFAULT;
		goto out;
	}

#if NVT_TOUCH_ESD_PROTECT
	/*
	 * stop esd check work to avoid case that 0x77 report righ after here to enable esd check again
	 * finally lead to trigger esd recovery bootloader reset
	 */
	cancel_delayed_work_sync(&nvt_esd_check_work);
	nvt_esd_check_enable(false);
#endif /* #if NVT_TOUCH_ESD_PROTECT */

	spi_wr = str[0] >> 7;
	memcpy(buf, str+2, ((str[0] & 0x7F) << 8) | str[1]);

	if (spi_wr == NVTWRITE) {	//SPI write
		while (retries < 20) {
			ret = nvt_spi_write(ts->client, buf, ((str[0] & 0x7F) << 8) | str[1]);
			if (!ret)
				break;
			else
				NVT_ERR("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}
	} else if (spi_wr == NVTREAD) {	//SPI read
		while (retries < 20) {
			ret = nvt_spi_read(ts->client, buf, ((str[0] & 0x7F) << 8) | str[1]);
			if (!ret)
				break;
			else
				NVT_ERR("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		memcpy(str+2, buf, ((str[0] & 0x7F) << 8) | str[1]);
		// copy buff to user if spi transfer
		if (retries < 20) {
			if (copy_to_user(buff, str, count)) {
				ret = -EFAULT;
				goto out;
			}
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("error, ret = %d\n", ret);
			ret = -EIO;
			goto out;
		}
	} else {
		NVT_ERR("Call error, str[0]=%d\n", str[0]);
		ret = -EFAULT;
		goto out;
	}

out:
	kfree(str);
    kfree(buf);
kzalloc_failed:
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI open function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
static int32_t nvt_flash_open(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev;

	dev = kmalloc(sizeof(struct nvt_flash_data), GFP_KERNEL);
	if (dev == NULL) {
		NVT_ERR("Failed to allocate memory for nvt flash data\n");
		return -ENOMEM;
	}

	rwlock_init(&dev->lock);
	file->private_data = dev;

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI close function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_flash_close(struct inode *inode, struct file *file)
{
	struct nvt_flash_data *dev = file->private_data;

	if (dev)
		kfree(dev);

	return 0;
}

static const struct file_operations nvt_flash_fops = {
	.owner = THIS_MODULE,
	.open = nvt_flash_open,
	.release = nvt_flash_close,
	.read = nvt_flash_read,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI initial function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
static int32_t nvt_flash_proc_init(void)
{
	NVT_proc_entry = proc_create(DEVICE_NAME, 0444, NULL,&nvt_flash_fops);
	if (NVT_proc_entry == NULL) {
		NVT_ERR("Failed!\n");
		return -ENOMEM;
	} else {
		NVT_LOG("Succeeded!\n");
	}

	//NVT_LOG("============================================================\n");
	//NVT_LOG("Create /proc/%s\n", DEVICE_NAME);
	//NVT_LOG("============================================================\n");

	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTSPI deinitial function.

return:
	n.a.
*******************************************************/
static void nvt_flash_proc_deinit(void)
{
	if (NVT_proc_entry != NULL) {
		remove_proc_entry(DEVICE_NAME, NULL);
		NVT_proc_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", DEVICE_NAME);
	}
}
#endif

static int8_t nvt_customizeCmd(uint8_t u8Cmd)
{
        uint8_t buf[8] = {0};
        uint8_t retry = 0;
        int8_t ret = 0;

	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)==0) {
		NVT_LOG("++ Cmd=0x%02X\n",u8Cmd);

	        for (retry = 0; retry < 20; retry++) {
	                //---set xdata index to EVENT BUF ADDR---
			nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	                //---switch HOST_CMD---
	                buf[0] = EVENT_MAP_HOST_CMD;
	                buf[1] = u8Cmd;
			nvt_spi_write(ts->client, buf, 2);

	                //msleep(35);

	                buf[0] = EVENT_MAP_HOST_CMD;
	                buf[1] = 0xFF;
	                nvt_spi_read(ts->client, buf, 2);

	                if (buf[1] == 0x00)
	                        break;
	        }

	        if (unlikely(retry == 20)) {
	                NVT_ERR("customizeCmd 0x%02X failed, buf[1]=0x%02X\n", u8Cmd, buf[1]);
	                ret = -1;
	        }
	}
        NVT_LOG("--\n");

        return ret;
}

void nvt_customizeCmd_ext(uint8_t u8Cmd){

	uint8_t Cmd = u8Cmd;
	nvt_customizeCmd(Cmd);
}

static uint8_t nvt_fw_recovery(uint8_t *point_data)
{
	uint8_t i = 0;
	uint8_t detected = true;

	/* check pattern */
	for (i=1 ; i<7 ; i++) {
		if (point_data[i] != 0x77) {
			detected = false;
			break;
		}
	}

	return detected;
}

#if NVT_TOUCH_ESD_PROTECT
void nvt_esd_check_enable(uint8_t enable)
{
	/* update interrupt timer */
	irq_timer = jiffies;
	/* clear esd_retry counter, if protect function is enabled */
	esd_retry = enable ? 0 : esd_retry;
	/* enable/disable esd check flag */
	esd_check = enable;
}

static void nvt_esd_check_func(struct work_struct *work)
{
	unsigned int timer = jiffies_to_msecs(jiffies - irq_timer);

	//NVT_LOG("esd_check = %d (retry %d)\n", esd_check, esd_retry);	//DEBUG

	if ((timer > NVT_TOUCH_ESD_CHECK_PERIOD) && esd_check) {
		mutex_lock(&ts->lock);
		NVT_ERR("do ESD recovery, timer = %d, retry = %d\n", timer, esd_retry);
		/* do esd recovery, reload fw */
#if BOOT_UPDATE_FIRMWARE
		nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME);
#endif
		mutex_unlock(&ts->lock);
		/* update interrupt timer */
		irq_timer = jiffies;
		/* update esd_retry counter */
		esd_retry++;
	}

	queue_delayed_work(nvt_esd_check_wq, &nvt_esd_check_work,
			msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
}
#endif /* #if NVT_TOUCH_ESD_PROTECT */

#if NVT_TOUCH_WDT_RECOVERY
static uint8_t recovery_cnt = 0;
static uint8_t nvt_wdt_fw_recovery(uint8_t *point_data)
{
   uint32_t recovery_cnt_max = 10;
   uint8_t recovery_enable = false;
   uint8_t i = 0;

   recovery_cnt++;

   /* check pattern */
   for (i=1 ; i<7 ; i++) {
       if ((point_data[i] != 0xFD) && (point_data[i] != 0xFE)) {
           recovery_cnt = 0;
           break;
       }
   }

   if (recovery_cnt > recovery_cnt_max){
       recovery_enable = true;
       recovery_cnt = 0;
   }

   return recovery_enable;
}
static void nvt_read_fw_history(uint32_t fw_history_addr){
    uint8_t i = 0;
    uint8_t buf[65];
    char str[128];
    if (fw_history_addr == 0)
        return;
    nvt_set_page(fw_history_addr);
    buf[0] = (uint8_t) (fw_history_addr & 0x7F);
    nvt_spi_read(ts->client, buf, 64);   //read 64bytes history
    //print all data
    NVT_LOG("fw history 0x%x: \n", fw_history_addr);
    for (i = 0; i < 4; i++) {
        snprintf(str, sizeof(str), "%2x %2x %2x %2x %2x %2x %2x %2x    %2x %2x %2x %2x %2x %2x %2x %2x\n",
            buf[1+i*16], buf[2+i*16], buf[3+i*16], buf[4+i*16],
            buf[5+i*16], buf[6+i*16], buf[7+i*16], buf[8+i*16],
            buf[9+i*16], buf[10+i*16], buf[11+i*16], buf[12+i*16],
            buf[13+i*16], buf[14+i*16], buf[15+i*16], buf[16+i*16]);
        NVT_LOG("%s", str);
    }
}

#endif	/* #if NVT_TOUCH_WDT_RECOVERY */

#if POINT_DATA_CHECKSUM
static int32_t nvt_ts_point_data_checksum(uint8_t *buf, uint8_t length)
{
   uint8_t checksum = 0;
   int32_t i = 0;

   // Generate checksum
   for (i = 0; i < length - 1; i++) {
       checksum += buf[i + 1];
   }
   checksum = (~checksum + 1);

   // Compare ckecksum and dump fail data
   if (checksum != buf[length]) {
       NVT_ERR("i2c/spi packet checksum not match. (point_data[%d]=0x%02X, checksum=0x%02X)\n",
               length, buf[length], checksum);

       for (i = 0; i < 10; i++) {
           NVT_LOG("%02X %02X %02X %02X %02X %02X\n",
                   buf[1 + i*6], buf[2 + i*6], buf[3 + i*6], buf[4 + i*6], buf[5 + i*6], buf[6 + i*6]);
       }

       NVT_LOG("%02X %02X %02X %02X %02X\n", buf[61], buf[62], buf[63], buf[64], buf[65]);

       return -1;
   }

   return 0;
}
#endif /* POINT_DATA_CHECKSUM */

/*******************************************************
Description:
	Novatek touchscreen check chip version trim function.

return:
	Executive outcomes. 0---NVT IC. -1---not NVT IC.
*******************************************************/
int nvt_ts_check_chip_ver_trim(void)
{
	uint8_t buf[8] = {0};
	int32_t retry = 0;
	int32_t list = 0;
	int32_t i = 0;
	int32_t found_nvt_chip = 0;
	int32_t ret = -1;
	static int32_t init_first_is = 1;

	if(init_first_is==1){
		ts = kmalloc(sizeof(struct nvt_ts_data), GFP_KERNEL);
		if (ts == NULL) {
			NVT_ERR("failed to allocated memory for nvt ts data\n");
			return -1;
		}
		ts->xbuf = (uint8_t *)kzalloc((NVT_TRANSFER_LEN+1+DUMMY_BYTES), GFP_KERNEL);
		if(ts->xbuf == NULL) {
			NVT_ERR("kzalloc for xbuf failed!\n");
			if (ts) {
				kfree(ts);
				ts = NULL;
			}
			return -ENOMEM;
		}

		//ts->client->addr = I2C_HW_Address;
		ts->client = g_client;
		ts->max_touch_num = g_board_b->max_touch_num;
		ts->abs_x_max =  g_board_b->panel_width;
		ts->abs_y_max =  g_board_b->panel_height;
		ts->swrst_n8_addr = g_board_b->swrst_n8_addr;
		ts->spi_rd_fast_addr = g_board_b->spi_rd_fast_addr;

		mutex_init(&ts->lock);
		mutex_init(&ts->xbuf_lock);
		//---eng reset before TP_RESX high
		nvt_eng_reset();
		NVT_ERR("lxl nvt_ts_check_chip_ver_trim 4!\n");
		init_first_is = 0;
	}

	//---Check for 5 times---
	for (retry = 5; retry > 0; retry--) {

		nvt_bootloader_reset();

		//---set xdata index to 0x1F600---
		nvt_set_page(0x1F64E);

		buf[0] = 0x4E;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		buf[6] = 0x00;
		nvt_spi_read(ts->client, buf, 7);
//		NVT_LOG("buf[1]=0x%02X, buf[2]=0x%02X, buf[3]=0x%02X, buf[4]=0x%02X, buf[5]=0x%02X, buf[6]=0x%02X\n",
//			buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);

		// compare read chip id on supported list
		for (list = 0; list < (sizeof(trim_id_table) / sizeof(struct nvt_ts_trim_id_table)); list++) {
			found_nvt_chip = 0;

			memset(ts->chipName, 0x00, sizeof(ts->chipName));
			// compare each byte
			for (i = 0; i < NVT_ID_BYTE_MAX; i++) {

				sprintf(ts->chipName, "NT%02x", buf[i + 1]);
				if (trim_id_table[list].mask[i]) {
					if (buf[i + 1] != trim_id_table[list].id[i])
						break;
				}
			}

			if (i == NVT_ID_BYTE_MAX) {

				found_nvt_chip = 1;

				if(buf[1] > 0)
				{
					sprintf(ts->chipName, "NT%x%02x%02x%X", buf[6], buf[5], buf[4], buf[1]);
				}
				else
				{
					sprintf(ts->chipName, "NT%x%02x%02x", buf[6], buf[5], buf[4]);
				}
			}

			if (found_nvt_chip) {
				NVT_LOG("This is NVT touch IC\n");
				NVT_LOG("This is NVT %s\n", ts->chipName);
				ts->mmap = trim_id_table[list].mmap;
				ts->carrier_system = trim_id_table[list].hwinfo->carrier_system;
				ts->hw_crc = trim_id_table[list].hwinfo->hw_crc;
				ret = 1;
				goto out;
			} else {
				ts->mmap = NULL;
				ret = -1;
			}
		}

		msleep(10);
	}

out:
	return ret;
}

struct nt36xxx_controller {
	struct ts_controller controller;
	unsigned char a3;
	unsigned char a8;
	bool single_transfer_only;
};

#define to_nt36xxx_controller(ptr) \
	container_of(ptr, struct nt36xxx_controller, controller)

static const unsigned short nt36xxx_addrs[] = { 0x62};

static  unsigned char nt36xxx_gesture_readdata_c(struct ts_controller *c)
{
        int32_t ret = -1;
        uint8_t gesture_id = 0;
        uint8_t point_data[POINT_DATA_LEN + 2] = {0};
        unsigned char value = -1 ;
        uint8_t func_type = 0;
        uint8_t func_id = 0;
        uint8_t buf[3]={0};
        mutex_lock(&ts->lock);
        /* read all bytes once! */
        ret = nvt_spi_read(ts->client, point_data, POINT_DATA_LEN + 1);
        if (ret < 0) {
            NVT_ERR("ctp i2c read failed.(%d)\n", ret);
            goto XFER_ERROR;
        }

#if NVT_TOUCH_WDT_RECOVERY
            /* ESD protect by WDT */
            if (nvt_wdt_fw_recovery(point_data)) {
                NVT_ERR("Recover for fw reset, %02X\n", point_data[1]);
                if (point_data[1] == 0xFE) {
                    nvt_sw_reset_idle();
                }
                nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT0);
                nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT1);
                nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME);
                buf[0] = EVENT_MAP_HOST_CMD;
                buf[1] = 0x13;
                nvt_spi_write(ts->client, buf, 2);
                goto XFER_ERROR;
            }
#endif /* #if NVT_TOUCH_WDT_RECOVERY */
        gesture_id = (uint8_t)(point_data[1] >> 3);
        func_type = point_data[2];
        func_id = point_data[3];

        /* support fw specifal data protocol */
        if ((gesture_id == DATA_PROTOCOL) && (func_type == FUNCPAGE_GESTURE)) {
            gesture_id = func_id;
        } else if (gesture_id > DATA_PROTOCOL) {
            NVT_ERR("gesture_id %d is invalid, func_type=%d, func_id=%d\n", gesture_id, func_type, func_id);
            goto XFER_ERROR;
        }
        value = gesture_id ;

    XFER_ERROR:
         mutex_unlock(&ts->lock);
	return value;
}


int nt36xxx_gesture_config(struct ts_controller * c , int onoff, struct msg_gesture_map *map)
{
	if (map->enabled == onoff)
		return 0;
	map->enabled = onoff;
	return 0;
}

static void nt36xxx_gesture_init_c(struct ts_controller *c)
{
    return ;
}

static int nt36xxx_gesture_exit_c(struct ts_controller *c)
{
	return 0;
}

static int nt36xxx_gesture_suspend_c(struct ts_controller *c)
{
    uint8_t buf[4] = {0};
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x13;
    nvt_spi_write(ts->client, buf, 2);
    printk("[TS] nt36xxx_gesture_suspend_c\n");
    mdelay(50);
	return 0;
 }

static int nt36xxx_gesture_resume_c(struct ts_controller *c)
{
    if(1 != Boot_Update_Firmware(NULL))
		NVT_LOG("nt36xxx_spi upgrade firmware failed !!!");
	return 0;
}

static void nt36xxx_ps_reset(void){

	int32_t ret=-1;
	uint8_t point_data[POINT_DATA_LEN + 1] = {0};

	nvt_customizeCmd(0xB5);

	/* read all bytes once! */
	ret = nvt_spi_read(ts->client, point_data, POINT_DATA_LEN);
	if (ret < 0) {
		NVT_ERR("ctp spi read failed.(%d)\n", ret);
	}
	printk("[TS] nt36xxx reset ps mode\n");
}

static void nt36xxx_custom_initialization(void){

	int8_t ret = 0;
	//ts->client->addr = I2C_HW_Address;

	//---eng reset before TP_RESX high
	nvt_eng_reset();

#if NVT_TOUCH_PROC
	ret = nvt_flash_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt flash proc init failed. ret=%d\n", ret);
		goto err_flash_proc_init_failed;
	}
#endif
#if NVT_TOUCH_EXT_PROC
	ret = nvt_extra_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt extra proc init failed. ret=%d\n", ret);
		goto err_extra_proc_init_failed;
	}
#endif

#if NVT_TOUCH_MP
	ret = nvt_mp_proc_init();
	if (ret != 0) {
		NVT_ERR("nvt mp proc init failed. ret=%d\n", ret);
		goto err_mp_proc_init_failed;
	}
#endif

#if 1//defined( CONFIG_TOUCHSCREEN_ADAPTIVE_ICNL9911X_SPI)
//Í¬Ê±Ê¹ÓÃÕâ¸öTPÊ±£¬ÓÉÓÚICNL9911CÐèÒªÊ¹ÓÃµ½¸´Î»Òý½Å£¬¶ønt36525b²»ÐèÒª£¬µ«ÊÇ¸´Î»Á¬½Óµ½ÆÁICÁË£¬¶øÆÁÒ»¶ËÓÐ1.8VÊä³ö£¬CPUÎª0£¬ËùÒÔ¿ª»úºóÒªÉèÖÃÎªCPUÊä³öÎª¸ß¼´¿É£¬·ñÔò»áÓÐÂ©µçÏÖÏó
	gpio_direction_output(g_pdata->board->rst_gpio, 1);
#endif

	return;
	//nvt_mp_proc_deinit();
#if NVT_TOUCH_MP
err_mp_proc_init_failed:
#endif
#if NVT_TOUCH_EXT_PROC
	nvt_extra_proc_deinit();
err_extra_proc_init_failed:
#endif
#if NVT_TOUCH_PROC
	nvt_flash_proc_deinit();
err_flash_proc_init_failed:
#endif
	ret=0;
}

static enum ts_result nt36xxx_handle_event(
	struct ts_controller *c, enum ts_event event, void *data)
{
	int8_t ret = 0;
	uint8_t buf[4] = {0};
	struct device_node *pn = NULL;
	//enum ts_event ret = TSRESULT_EVENT_HANDLED;
	struct nt36xxx_controller *ftc = to_nt36xxx_controller(c);

	switch (event) {
	case TSEVENT_POWER_ON:
		if (data) {
			pn = (struct device_node *)data;
			if (!of_property_read_u8(pn, "a8", &ftc->a8))
				TS_DBG("parse a8 value: 0x%02X", ftc->a8);
			ftc->single_transfer_only = !!of_get_property(pn, "single-transfer-only", NULL);
			if (ftc->single_transfer_only)
				TS_DBG("single transfer only");
		}
		break;
	case TSEVENT_SUSPEND:
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x11;
		nvt_spi_write(ts->client, buf, 2);
		break;
	case TSEVENT_RESUME:
		break;
	case TSEVENT_NOISE_HIGH:
		ret = nvt_customizeCmd(CMD_ENTER_COMMON_USB_PLUGIN);
		if (ret < 0){
		    NVT_LOG("noise high mode 0x%02X cmd failed.\n", CMD_ENTER_COMMON_USB_PLUGIN);
		}
		break;
	case TSEVENT_NOISE_NORMAL:
		ret = nvt_customizeCmd(CMD_ENTER_COMMON_USB_PLUGOUT);
		if (ret < 0){
		    NVT_LOG("noise low mode 0x%02X cmd failed.\n", CMD_ENTER_COMMON_USB_PLUGOUT);
		}		break;
	default:
		break;
	}

	return TSRESULT_EVENT_HANDLED;
}

/* read chip id and module id to match controller */
static enum ts_result nt36xxx_match(struct ts_controller *c)
{
	int32_t ret = -1;

         //ts->client->addr=c->addrs[0];

	ret = nvt_ts_check_chip_ver_trim();

	return (ret) ? TSRESULT_NOT_MATCHED : TSRESULT_FULLY_MATCHED;
}

static const struct ts_register_info nt36xxx_registers[] = {
	DECLARE_REGISTER(TSREG_CHIP_ID, 0xA3),
/*	DECLARE_REGISTER(TSREG_MOD_ID, REG_MODULE_ID),
	DECLARE_REGISTER(TSREG_FW_VER, REG_FIRMWARE_VERSION),
	DECLARE_REGISTER("frequency", REG_SCANNING_FREQ),
	DECLARE_REGISTER("charger_indicator", REG_CHARGER_INDICATOR),
*/
};

static int nt36xxx_fetch(struct ts_controller *c, struct ts_point *points)
{
	int32_t ret = -1;
	uint32_t i = 0, j = 0;
	uint8_t input_id = 0;
	uint32_t input_p = 0;
	uint32_t position = 0;
	uint32_t input_w = 0;
	uint8_t point_data[POINT_DATA_LEN + 2] = {0};

	mutex_lock(&ts->lock);
	/* read all bytes once! */
	ret = nvt_spi_read(ts->client, point_data, POINT_DATA_LEN + 1);
	if (ret < 0) {
		NVT_ERR("ctp i2c read failed.(%d)\n", ret);
		goto XFER_ERROR;
	}

#if NVT_TOUCH_WDT_RECOVERY
	/* ESD protect by WDT */
	if (nvt_wdt_fw_recovery(point_data)) {
		NVT_ERR("Recover for fw reset, %02X\n", point_data[1]);
		if (point_data[1] == 0xFE) {
			nvt_sw_reset_idle();
		}
		nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT0);
		nvt_read_fw_history(ts->mmap->MMAP_HISTORY_EVENT1);
		nvt_update_firmware(BOOT_UPDATE_FIRMWARE_NAME);
		goto XFER_ERROR;
	}
#endif /* #if NVT_TOUCH_WDT_RECOVERY */

	if (nvt_fw_recovery(point_data)) {
#if NVT_TOUCH_ESD_PROTECT
		nvt_esd_check_enable(true);
#endif /* #if NVT_TOUCH_ESD_PROTECT */
		goto XFER_ERROR;
	}

#if WAKEUP_GESTURE
	if (bTouchIsAwake == 0) {
		input_id = (uint8_t)(point_data[1] >> 3);
		nvt_ts_wakeup_gesture_report(input_id, point_data);
		mutex_unlock(&ts->lock);
		return IRQ_HANDLED;
	}
#endif

	if(g_pdata->board->ps_status || g_pdata->board->sensorhub_status){
		if(g_pdata->tpd_prox_active){

			if (((point_data[1]==0xF0) && (point_data[2]==0x02))&&((point_data[3] != 0x01) && (point_data[3] != 0x02))){
				nvt_customizeCmd(0xB5);
				printk("[TS] reset ps mode point_data[3]=0x%x\n", point_data[3]);
			}

			printk_ratelimited("[TS][NVT-ts] dial:%d, 0x%x, 0x%x, 0x%x\n",
				g_pdata->tpd_prox_active, point_data[1], point_data[2], point_data[3] );
			if((point_data[1]==0xF0) && (point_data[2]==0x02)){

				if (0x01 == point_data[3]){//near
					c->pdata->ps_buf = 0xc0;
				}
				else if (0x02 == point_data[3]){//far-away
					c->pdata->ps_buf = 0xe0;
				}
				else{
					NVT_LOG("point_data[3] data error /ps failed!!\n");
				}
			}
		}
	}

	for (i = 0; i < ts->max_touch_num; i++) {
		position = 1 + 6 * i;
		input_id = (uint8_t)(point_data[position + 0] >> 3);
		if ((input_id == 0) || (input_id > ts->max_touch_num)){
			points[i].pressed = 0;
			points[i].slot = i;
			continue;
		}

		if (((point_data[position] & 0x07) == 0x01) || ((point_data[position] & 0x07) == 0x02)){//finger down (enter & moving)

			points[j].x = (uint32_t)(point_data[position + 1] << 4) + (uint32_t) (point_data[position + 3] >> 4);
			points[j].y = (uint32_t)(point_data[position + 2] << 4) + (uint32_t) (point_data[position + 3] & 0x0F);
			//if ((points[j].x < 0) || (points[j].y < 0))
			//	continue;
			if ((points[j].x > ts->abs_x_max) || (points[j].y > ts->abs_y_max))
				continue;
			input_w = (uint32_t)(point_data[position + 4]);
			if (input_w == 0)
				input_w = 1;
			if (i < 2) {
				input_p = (uint32_t)(point_data[position + 5]) + (uint32_t)(point_data[i + 63] << 8);
				if (input_p > TOUCH_FORCE_NUM)
					input_p = TOUCH_FORCE_NUM;
			} else {
				input_p = (uint32_t)(point_data[position + 5]);
			}
			if (input_p == 0)
				input_p = 1;

			points[j].pressed = 1;
			points[j].slot = (input_id-1)<<4>>4;//MT_TOOL_FINGER
			points[j].pressure = (unsigned short)input_p & 0xFFFF;//ABS_MT_PRESSURE
			points[j].touch_major = (unsigned short)input_w & 0xFFFF;//ABS_MT_TOUCH_MAJOR
			//printk("[[TS]NVT-ts]:X:Y:%d %d", points[j].x, points[j].y);

			j++;
		}
	}
	c->pdata->touch_point = (unsigned short)j;

	if(j==0){
		j=ts->max_touch_num;
	}
XFER_ERROR:
	mutex_unlock(&ts->lock);

	return j;
}

/* firmware upgrade procedure */
/*
static enum ts_result nt36xxx_upgrade_firmware(struct ts_controller *c,
	const unsigned char *data, size_t size, bool force)
{
  	return 1;
}*/
//static enum ts_result tlsc6x_upgrade_firmware(struct ts_controller *c,
//	const unsigned char *data, size_t size, bool force)
//static enum ts_result tlsc6x_upgrade_firmware(void)
 uint32_t lcd_moden_from_uboot=0;
int  nt36xxx_upgrade_init(void)
{
	int ret=-1, i=0;
	uint8_t *tpd_vendor_id={0};
	int nvt_vendor_id=0;
	uint8_t nvt_vendor_name[16]={0};
	u32 *tpd_firmware_update={0};

	ts->update_node = ts->client->dev.of_node;
	ts->update_node = of_get_child_by_name(ts->update_node, "ats_nt36xxx");
	ret = of_property_read_u32(ts->update_node, "tp_vendor_num", &ts->vendor_nums);
	if (ret < 0)
		NVT_ERR("read tp_vendor_num fail. ret=%x\n", ret);

	tpd_vendor_id = kmalloc(ts->vendor_nums, GFP_KERNEL);
	if(tpd_vendor_id == NULL){
		NVT_ERR("tpd_vendor_id kmalloc is not found\n");
		return -1;
	}

	tpd_firmware_update = kmalloc(ts->vendor_nums*sizeof(u32), GFP_KERNEL);
	if(tpd_firmware_update == NULL){
		NVT_ERR("tpd_firmware kmalloc is not found\n");
		kfree(tpd_vendor_id);
		return -1;
	}

	ret = of_property_read_u32_array(ts->update_node, "tp_upgrade_fw", tpd_firmware_update, ts->vendor_nums);
	if (ret < 0)
		NVT_ERR("read tp_upgrade_fw fail. ret=%x\n", ret);
	ret = of_property_read_u8_array(ts->update_node, "tp_vendor_id", tpd_vendor_id, ts->vendor_nums);
	if (ret < 0)
		NVT_ERR("read tp_vendor_id fail. ret=%x\n", ret);

	for(i=0; i<ts->vendor_nums; i++){
		NVT_LOG("[tpd] tp_vendor_id[%d] = 0x%x\n",  i, tpd_vendor_id[i]);
		NVT_LOG("[tpd] tp_upgrade_switch[%d] = %d\n",  i, tpd_firmware_update[i]);
	}

	if(lcd_moden_from_uboot==0)
		nvt_vendor_id = 0x01;
	else if(lcd_moden_from_uboot==1)
		nvt_vendor_id = 0x02;
	else
		nvt_vendor_id = 0x01;

	NVT_LOG("[tpd] ili_vendor_id=0x%x \n", nvt_vendor_id);

	for(i=0; i <  ts->vendor_nums; i++ ){
		if (tpd_vendor_id[i] ==nvt_vendor_id){

			ts->vendor_num = i;
			g_pdata->firmware_update_switch = tpd_firmware_update[i];

			sprintf(nvt_vendor_name, "tp_vendor_name%d", i);
			ret = of_property_read_string(ts->update_node, nvt_vendor_name, (char const **)&g_pdata->vendor_string);
			if (ret < 0)
				NVT_ERR("read tp_vendor_name fail. ret=%x\n", ret);

			break;
		}
	}

	ret = Boot_Update_Firmware(NULL);

	ret = nvt_get_fw_info();
	if (ret < 0)
		NVT_ERR("nvt_get_fw_info fail. ret=%x\n", ret);
	memset(g_pdata->firmwork_version, 0x00, sizeof(g_pdata->firmwork_version));
	sprintf(g_pdata->firmwork_version, "0x%02x", ts->fw_ver);

	memset(g_pdata->chip_name, 0x00, sizeof(g_pdata->chip_name));
	memcpy(g_pdata->chip_name, ts->chipName, strlen(ts->chipName));
	kfree(tpd_vendor_id);
	kfree(tpd_firmware_update);

	return ret;
}

int  nt36xxx_upgrade_status(struct ts_controller *c){

	int ret=-1;
	static int is_init=1;

	if(is_init){
		is_init=0;
		ret = nt36xxx_upgrade_init();
	}
	else{
		ret = Boot_Update_Firmware(NULL);
	}

	return ret;
}

static int nt36xxx_ps_resume(struct ts_data *pdata){

	if(1 != Boot_Update_Firmware(NULL))
		NVT_LOG("nt36xxx_spi upgrade firmware failed !!!");

	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)||ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if (pdata->tpd_prox_active && (pdata->tpm_status == TPM_DESUSPEND)) {

			nvt_customizeCmd_ext(0xB5);
			pdata->tpd_prox_old_state = 0x0f;

			printk("[TS] ps_resume ps is on, so return !!!\n");
			return 0;
		}
	}

	ts_reset_controller_ex(pdata, true);

	/*å¨TSå¤äºä¼ç æ¨¡å¼ä¸ï¼å¦ææå¯¹è¿è¡psæå¼æä½ï¼å¨è¿å¥å¤éåï¼éè¦å¨æ­¤å¤å¯¹åºä»¶ä¸åps         ä½¿è½åæ°*/
	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)||ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if(pdata->tps_status == TPS_DEON && pdata->tpd_prox_active){

			nvt_customizeCmd_ext(0xB5);
			pdata->tps_status = TPS_ON;
		}
	}

	return 1;
}

static int nt36xxx_ps_suspend(struct ts_data *pdata){

	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)||ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if (pdata->tpd_prox_active) {

			ts_clear_points_ext(pdata);

			printk("[TS] ps_suspend:ps is on, so return!!!\n");
			return 0;
		}
	}

	return 1;
}

static void nt36xxx_proximity_switch(bool onoff){

	if(onoff)//proximity on
		nvt_customizeCmd_ext(0xB5);
	else//proximity off
		nvt_customizeCmd_ext(0xB6);
}

static void nt36xxx_ps_irq_handler(struct ts_data *pdata){

	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)){
		if(((pdata->ps_buf== 0xc0) || (pdata->ps_buf== 0xe0)) && (pdata->tpd_prox_old_state != pdata->ps_buf)){
			pdata->tpd_prox_old_state = pdata->ps_buf;
		}
	}

	if(ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if(pdata->tpd_prox_old_state != pdata->ps_buf){
			pdata->tpd_prox_old_state = pdata->ps_buf;
		}
	}
}

static struct nt36xxx_controller nt36xxx_spi = {
	.controller = {
		.name = "NT36XXX",
		.vendor = "nt36xxx",
		.incell = 1,
		.config = TSCONF_ADDR_WIDTH_BYTE
			| TSCONF_POWER_ON_RESET
			| TSCONF_RESET_LEVEL_LOW
			| TSCONF_REPORT_MODE_IRQ
			| TSCONF_IRQ_TRIG_EDGE_FALLING
			| TSCONF_REPORT_TYPE_3,
		.addr_count = ARRAY_SIZE(nt36xxx_addrs),
		.addrs = nt36xxx_addrs,
		//.virtualkey_count = NULL,
		//.virtualkeys = NULL,
		.register_count = ARRAY_SIZE(nt36xxx_registers),
		.registers = nt36xxx_registers,
		.panel_width = 720,
		.panel_height = 1600,
		.reset_keep_ms = 20,
		.reset_delay_ms = 30,
		.parser = {
		},
        .gesture_reg = {0},
        .msg_gestures_maps = nt36xxx_gestures_maps,
        .msg_gestures_count = ARRAY_SIZE(nt36xxx_gestures_maps),
		.ps_reset = nt36xxx_ps_reset,
		.custom_initialization = nt36xxx_custom_initialization,
		.match = nt36xxx_match,
		.fetch_points = nt36xxx_fetch,
		.handle_event = nt36xxx_handle_event,
		.upgrade_firmware = NULL,//nt36xxx_upgrade_firmware,
		.upgrade_status = nt36xxx_upgrade_status,
		.gesture_readdata = NULL,
		.gesture_init = NULL,
		.gesture_exit = NULL,
		.gesture_suspend = NULL,
		.gesture_resume = NULL,
		.gesture_readdata = nt36xxx_gesture_readdata_c,
        .gesture_config = nt36xxx_gesture_config,
        .gesture_init = nt36xxx_gesture_init_c,
        .gesture_exit = nt36xxx_gesture_exit_c,
        .gesture_suspend = nt36xxx_gesture_suspend_c,
        .gesture_resume = nt36xxx_gesture_resume_c,
		.ps_resume = nt36xxx_ps_resume,
		.ps_suspend = nt36xxx_ps_suspend,
		.proximity_switch = nt36xxx_proximity_switch,
		.ps_irq_handler = nt36xxx_ps_irq_handler,

	},
	.a3 = 0x54,
	.a8 = 0x87,
	.single_transfer_only = false,
};


int nt36xxx_init(void)
{
	ts_register_controller(&nt36xxx_spi.controller);
	return 0;
}

void nt36xxx_exit(void)
{
	ts_unregister_controller(&nt36xxx_spi.controller);
}
