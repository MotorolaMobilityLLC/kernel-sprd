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
#include <linux/msm_drm_notify.h>
#endif
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#if defined(CONFIG_TPD_SENSORHUB)
#include <linux/shub_api.h>
#endif

#include"ats_nt36xxx.h"

#if NVT_TOUCH_ESD_PROTECT
#include <linux/jiffies.h>
#endif /* #if NVT_TOUCH_ESD_PROTECT */

struct nvt_ts_data *ts;

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
	Novatek touchscreen i2c read function.

return:
	Executive outcomes. 2---succeed. -5---I/O error
*******************************************************/
int32_t  nvt_ctp_i2c_read(struct i2c_client *client, uint16_t address, uint8_t *buf, uint16_t len)
{
	struct i2c_msg msgs[2];
	int32_t ret = -1;
	int32_t retries = 0;

	mutex_lock(&ts->xbuf_lock);

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = address;
	msgs[0].len   = 1;
	msgs[0].buf   = &buf[0];

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = address;
	msgs[1].len   = len - 1;
	msgs[1].buf   = ts->xbuf;

	while (retries < 3) {//binhua 5
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)	break;
		retries++;
	}

	if (unlikely(retries == 3)) {//binhua 5
		NVT_ERR("error, ret=%d\n", ret);
		ret = -EIO;
	}

	memcpy(buf + 1, ts->xbuf, len - 1);

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen i2c write function.

return:
	Executive outcomes. 1---succeed. -5---I/O error
*******************************************************/
int32_t nvt_ctp_i2c_write(struct i2c_client *client, uint16_t address, uint8_t *buf, uint16_t len)
{
	struct i2c_msg msg;
	int32_t ret = -1;
	int32_t retries = 0;

	mutex_lock(&ts->xbuf_lock);

	msg.flags = !I2C_M_RD;
	msg.addr  = address;
	msg.len   = len;
	memcpy(ts->xbuf, buf, len);
	msg.buf  = ts->xbuf;

	while (retries < 3) {//binhua 5
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)	break;
		retries++;
	}

	if (unlikely(retries == 3)) {// 5
		NVT_ERR("error, ret=%d\n", ret);
		ret = -EIO;
	}

	mutex_unlock(&ts->xbuf_lock);

	return ret;
}

static int8_t nvt_customizeCmd(uint8_t u8Cmd)
{
        uint8_t buf[8] = {0};
        uint8_t retry = 0;
        int8_t ret = 0;

        mutex_lock(&ts->lock);
	if (nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN)==0) {
		NVT_LOG("++ Cmd=0x%02X\n",u8Cmd);

	        for (retry = 0; retry < 20; retry++) {
	                //---set xdata index to EVENT BUF ADDR---
	                buf[0] = 0xFF;
	                buf[1] = (ts->mmap->EVENT_BUF_ADDR >> 16) & 0xFF;
	                buf[2] = (ts->mmap->EVENT_BUF_ADDR >> 8) & 0xFF;
	                nvt_ctp_i2c_write(ts->client, I2C_FW_Address, buf, 3);

	                //---switch HOST_CMD---
	                buf[0] = EVENT_MAP_HOST_CMD;
	                buf[1] = u8Cmd;
	                nvt_ctp_i2c_write(ts->client, I2C_FW_Address, buf, 2);

	                //msleep(35);

	                buf[0] = EVENT_MAP_HOST_CMD;
	                buf[1] = 0xFF;
	                nvt_ctp_i2c_read(ts->client, I2C_FW_Address, buf, 2);

	                if (buf[1] == 0x00)
	                        break;
	        }

	        if (unlikely(retry == 20)) {
	                NVT_ERR("customizeCmd 0x%02X failed, buf[1]=0x%02X\n", u8Cmd, buf[1]);
	                ret = -1;
	        }
	}

	mutex_unlock(&ts->lock);

        NVT_LOG("--\n");

        return ret;
}

void nvt_customizeCmd_ext(uint8_t u8Cmd){

	uint8_t Cmd = u8Cmd;
	nvt_customizeCmd(Cmd);
}
/*******************************************************
Description:
	Novatek touchscreen set index/page/addr address.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_set_page(uint16_t i2c_addr, uint32_t addr)
{
	uint8_t buf[4] = {0};

	buf[0] = 0xFF;	//set index/page/addr command
	buf[1] = (addr >> 16) & 0xFF;
	buf[2] = (addr >> 8) & 0xFF;

	return nvt_ctp_i2c_write(ts->client, i2c_addr, buf, 3);
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
	uint8_t buf[4]={0};

	//---write i2c cmds to reset idle---
	buf[0]=0x00;
	buf[1]=0xA5;
	nvt_ctp_i2c_write(ts->client, I2C_HW_Address, buf, 2);

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
	uint8_t buf[8] = {0};

	NVT_LOG("start\n");

	//---write i2c cmds to reset---
	buf[0] = 0x00;
	buf[1] = 0x69;
	nvt_ctp_i2c_write(ts->client, I2C_HW_Address, buf, 2);

	// need 35ms delay after bootloader reset
	msleep(35);

	NVT_LOG("end\n");
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
		nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---clear fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		nvt_ctp_i2c_write(ts->client, I2C_FW_Address, buf, 2);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0xFF;
		nvt_ctp_i2c_read(ts->client, I2C_FW_Address, buf, 2);

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
		nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		nvt_ctp_i2c_read(ts->client, I2C_FW_Address, buf, 2);

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

	while (1) {
		usleep_range(10000, 10000);

		//---read reset state---
		buf[0] = EVENT_MAP_RESET_COMPLETE;
		buf[1] = 0x00;
		nvt_ctp_i2c_read(ts->client, I2C_FW_Address, buf, 6);

		if ((buf[1] >= check_reset_state) && (buf[1] <= RESET_STATE_MAX)) {
			ret = 0;
			break;
		}

		retry++;
		if(unlikely(retry > 100)) {
			NVT_ERR("error, retry=%d, buf[1]=0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
				retry, buf[1], buf[2], buf[3], buf[4], buf[5]);
			ret = -1;
			break;
		}
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
	uint8_t buf[3] = {0};
	int32_t ret = 0;

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_PROJECTID);

	//---read project id---
	buf[0] = EVENT_MAP_PROJECTID;
	buf[1] = 0x00;
	buf[2] = 0x00;
	nvt_ctp_i2c_read(ts->client, I2C_FW_Address, buf, 3);

	ts->nvt_pid = (buf[2] << 8) + buf[1];

	NVT_LOG("PID=%04X\n", ts->nvt_pid);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen check and stop crc reboot loop.

return:
	n.a.
*******************************************************/
void nvt_stop_crc_reboot(void)
{
	uint8_t buf[8] = {0};
	int32_t retry = 0;

	//read dummy buffer to check CRC fail reboot is happening or not

	//---change I2C index to prevent geting 0xFF, but not 0xFC---
	nvt_set_page(I2C_BLDR_Address, 0x1F64E);

	//---read to check if buf is 0xFC which means IC is in CRC reboot ---
	buf[0] = 0x4E;
	nvt_ctp_i2c_read(ts->client, I2C_BLDR_Address, buf, 4);

	if ((buf[1] == 0xFC) ||
		((buf[1] == 0xFF) && (buf[2] == 0xFF) && (buf[3] == 0xFF))) {

		//IC is in CRC fail reboot loop, needs to be stopped!
		for (retry = 5; retry > 0; retry--) {

			//---write i2c cmds to reset idle : 1st---
			buf[0]=0x00;
			buf[1]=0xA5;
			nvt_ctp_i2c_write(ts->client, I2C_HW_Address, buf, 2);

			//---write i2c cmds to reset idle : 2rd---
			buf[0]=0x00;
			buf[1]=0xA5;
			nvt_ctp_i2c_write(ts->client, I2C_HW_Address, buf, 2);
			msleep(1);

			//---clear CRC_ERR_FLAG---
			nvt_set_page(I2C_BLDR_Address, 0x3F135);

			buf[0] = 0x35;
			buf[1] = 0xA5;
			nvt_ctp_i2c_write(ts->client, I2C_BLDR_Address, buf, 2);

			//---check CRC_ERR_FLAG---
			nvt_set_page(I2C_BLDR_Address, 0x3F135);

			buf[0] = 0x35;
			buf[1] = 0x00;
			nvt_ctp_i2c_read(ts->client, I2C_BLDR_Address, buf, 2);

			if (buf[1] == 0xA5)
				break;
		}
		if (retry == 0)
			NVT_ERR("CRC auto reboot is not able to be stopped! buf[1]=0x%02X\n", buf[1]);
	}

	return;
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
	nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);

	//---read fw info---
	buf[0] = EVENT_MAP_FWINFO;
	nvt_ctp_i2c_read(ts->client, I2C_FW_Address, buf, 17);
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
			NVT_ERR("Set default fw_ver=%d, x_num=%d, y_num=%d,max_button_num=%d!\n",
					ts->fw_ver, ts->x_num, ts->y_num,ts->max_button_num);
			ret = -1;
		}
	} else {
		ret = 0;
	}

	//---Get Novatek PID---
	nvt_read_pid();

	return ret;
}

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

		g_client->addr = I2C_HW_Address;
		ts->client = g_client;
		ts->max_touch_num = g_board_b->max_touch_num;
		ts->abs_x_max =  g_board_b->panel_width;
		ts->abs_y_max =  g_board_b->panel_height;

		mutex_init(&ts->lock);
		mutex_init(&ts->xbuf_lock);
		init_first_is = 0;
	}

	nvt_bootloader_reset(); // NOT in retry loop

	//---Check for 5 times---
	for (retry = 5; retry > 0; retry--) {
		nvt_sw_reset_idle();

		buf[0] = 0x00;
		buf[1] = 0x35;
		nvt_ctp_i2c_write(ts->client, I2C_HW_Address, buf, 2);
		msleep(10);

		nvt_set_page(I2C_BLDR_Address, 0x1F64E);

		buf[0] = 0x4E;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
		buf[4] = 0x00;
		buf[5] = 0x00;
		buf[6] = 0x00;
		nvt_ctp_i2c_read(ts->client, I2C_BLDR_Address, buf, 7);
		//NVT_LOG("buf[1]=0x%02X, buf[2]=0x%02X, buf[3]=0x%02X, buf[4]=0x%02X, buf[5]=0x%02X, buf[6]=0x%02X\n",
		//	buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);

		//---Stop CRC check to prevent IC auto reboot---
		if ((buf[1] == 0xFC) ||
			((buf[1] == 0xFF) && (buf[2] == 0xFF) && (buf[3] == 0xFF))) {
			nvt_stop_crc_reboot();
			continue;
		}

		// compare read chip id on supported list
		for (list = 0; list < (sizeof(trim_id_table) / sizeof(struct nvt_ts_trim_id_table)); list++) {
			found_nvt_chip = 0;

			memset(ts->chipName, 0x00, sizeof(ts->chipName));
			// compare each byte
			for (i = 0; i < NVT_ID_BYTE_MAX; i++) {
				if (trim_id_table[list].mask[i]) {

					sprintf(ts->chipName, "NT%02x", buf[i + 1]);
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

/*******************************************************
  Create Device Node (Proc Entry)
*******************************************************/
#if NVT_TOUCH_PROC
static struct proc_dir_entry *NVT_proc_entry;
#define DEVICE_NAME	"NVTflash"

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTflash read function.

return:
	Executive outcomes. 2---succeed. -5,-14---failed.
*******************************************************/
static ssize_t nvt_flash_read(struct file *file, char __user *buff, size_t count, loff_t *offp)
{
	uint8_t str[68] = {0};
	int32_t ret = -1;
	int32_t retries = 0;
	int8_t i2c_wr = 0;

	if (count > sizeof(str)) {
		NVT_ERR("error count=%zu\n", count);
		return -EFAULT;
	}

	if (copy_from_user(str, buff, count)) {
		NVT_ERR("copy from user error\n");
		return -EFAULT;
	}

	i2c_wr = str[0] >> 7;

	if (i2c_wr == 0) {	//I2C write
		while (retries < 20) {
			ret = nvt_ctp_i2c_write(ts->client, (str[0] & 0x7F), &str[2], str[1]);
			if (ret == 1)
				break;
			else
				NVT_ERR("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("error, ret = %d\n", ret);
			return -EIO;
		}

		return ret;
	} else if (i2c_wr == 1) {	//I2C read
		while (retries < 20) {
			ret = nvt_ctp_i2c_read(ts->client, (str[0] & 0x7F), &str[2], str[1]);
			if (ret == 2)
				break;
			else
				NVT_ERR("error, retries=%d, ret=%d\n", retries, ret);

			retries++;
		}

		// copy buff to user if i2c transfer
		if (retries < 20) {
			if (copy_to_user(buff, str, count))
				return -EFAULT;
		}

		if (unlikely(retries == 20)) {
			NVT_ERR("error, ret = %d\n", ret);
			return -EIO;
		}

		return ret;
	} else {
		NVT_ERR("Call error, str[0]=%d\n", str[0]);
		return -EFAULT;
	}
}

/*******************************************************
Description:
	Novatek touchscreen /proc/NVTflash open function.

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
	Novatek touchscreen /proc/NVTflash close function.

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
	Novatek touchscreen /proc/NVTflash initial function.

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
	Novatek touchscreen /proc/NVTflash deinitial function.

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

static  unsigned char nt36xxx_gesture_readdata_c(struct ts_controller *c)
{
        int32_t ret = -1;
        uint8_t gesture_id = 0;
        uint8_t point_data[POINT_DATA_LEN + 1] = {0};
        unsigned char value = -1 ;
        uint8_t func_type = 0;
        uint8_t func_id = 0;
        mutex_lock(&ts->lock);
        /* read all bytes once! */
        ret = nvt_ctp_i2c_read(ts->client, I2C_FW_Address, point_data, POINT_DATA_LEN + 1);
        if (ret < 0) {
            NVT_ERR("ctp i2c read failed.(%d)\n", ret);
            goto XFER_ERROR;
        }
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
	nvt_ctp_i2c_write(ts->client, I2C_FW_Address, buf, 2);
    printk("[TS] nt36xxx_gesture_suspend_c\n");
    mdelay(50);
	return 0;	
 }

static int nt36xxx_gesture_resume_c(struct ts_controller *c)
{

	return 0;
}


static void nt36xxx_ps_reset(void){

	int32_t ret=-1;
	uint8_t point_data[POINT_DATA_LEN + 1] = {0};

	nvt_customizeCmd(0xB5);

	/* read all bytes once! */
	ret = nvt_ctp_i2c_read(ts->client, I2C_FW_Address, point_data, POINT_DATA_LEN + 1);
	if (ret < 0) {
		NVT_ERR("ctp i2c read failed.(%d)\n", ret);
	}
	printk("[TS] nt36xxx reset ps mode\n");
}

static void nt36xxx_custom_initialization(void){

	int8_t ret = 0;
	g_client->addr = I2C_HW_Address;

	nvt_bootloader_reset();
	nvt_check_fw_reset_state(RESET_STATE_INIT);
	nvt_get_fw_info();

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
		nvt_ctp_i2c_write(ts->client, I2C_FW_Address, buf, 2);	
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
		}
		break;
	default:
		break;
	}

	return TSRESULT_EVENT_HANDLED;
}

/* read chip id and module id to match controller */
static enum ts_result nt36xxx_match(struct ts_controller *c)
{
	int32_t ret = -1;

         g_client->addr=c->addrs[0];

	ret = nvt_ts_check_chip_ver_trim();

	return (ret) ? TSRESULT_NOT_MATCHED : TSRESULT_FULLY_MATCHED;
}

static const struct ts_virtualkey_info nt36xxx_virtualkeys[] = {
	DECLARE_VIRTUALKEY(120, 1500, 60, 45, KEY_BACK),
	DECLARE_VIRTUALKEY(360, 1500, 60, 45, KEY_HOMEPAGE),
	DECLARE_VIRTUALKEY(600, 1500, 60, 45, KEY_APPSELECT),
};

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
	int32_t i = 0, j = 0;
	uint8_t input_id = 0;
	uint32_t input_p = 0;
	uint32_t position = 0;
	uint32_t input_w = 0;
	uint8_t point_data[POINT_DATA_LEN + 1] = {0};

	mutex_lock(&ts->lock);
	/* read all bytes once! */
	ret = nvt_ctp_i2c_read(ts->client, I2C_FW_Address, point_data, POINT_DATA_LEN + 1);
	if (ret < 0) {
		NVT_ERR("ctp i2c read failed.(%d)\n", ret);
		goto XFER_ERROR;
	}

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
			if ((points[j].x < 0) || (points[j].y < 0))
				continue;
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

			points[j].pressed = !((point_data[position]&0x07) & 0x40);
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
int  nt36xxx_upgrade_status(struct ts_controller *c)
{
	return Boot_Update_Firmware();
}

static int nt36xxx_ps_resume(struct ts_data *pdata){

	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)||ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if (pdata->tpd_prox_active && (pdata->tpm_status == TPM_DESUSPEND)) {

			nvt_customizeCmd_ext(0xB5);
			pdata->tpd_prox_old_state = 0x0f;

			printk("[TS] ps_resume ps is on, so return !!!\n");
			return 0;
		}
	}

	ts_reset_controller_ex(pdata, true);

	if (ts_get_mode_ext(pdata, TSMODE_PS_STATUS)||ts_get_mode_ext(pdata, TSMODE_SENSORHUB_STATUS)){
		if(pdata->tps_status == TPS_DEON && pdata->tpd_prox_active){

			nvt_customizeCmd_ext(0xB5);
			pdata->tps_status = TPS_ON ;
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

static struct nt36xxx_controller nt36xxx = {
	.controller = {
		.name = "NT36xxx",
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
		.virtualkey_count = ARRAY_SIZE(nt36xxx_virtualkeys),
		.virtualkeys = nt36xxx_virtualkeys,
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
	ts_register_controller(&nt36xxx.controller);
	return 0;
}

void nt36xxx_exit(void)
{       
	ts_unregister_controller(&nt36xxx.controller);
}
