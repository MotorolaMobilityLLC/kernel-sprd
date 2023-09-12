/*****************************************************************************
*
* Copyright (c) 2013 mCube, Inc.  All rights reserved.
*
* This source is subject to the mCube Software License.
* This software is protected by Copyright and the information and source code
* contained herein is confidential. The software including the source code
* may not be copied and the information contained herein may not be used or
* disclosed except with the written permission of mCube Inc.
*
* All other rights reserved.
*
* This code and information are provided "as is" without warranty of any
* kind, either expressed or implied, including but not limited to the
* implied warranties of merchantability and/or fitness for a
* particular purpose.
*
* The following software/firmware and/or related documentation ("mCube Software")
* have been modified by mCube Inc. All revisions are subject to any receiver's
* applicable license agreements with mCube Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
* GNU General Public License for more details.
*
*
*****************************************************************************/
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <uapi/linux/sched/types.h>
/*#define GSENSOR_INTERRUPT_MODE*/
#include <mc34xx.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
/*#define CONFIG_ZYT_GSENSOR_COMPATIBLE*/
#define CONFIG_ZYT_SUPPORT
extern int CDC_Gsensor_Device_Id(int);
#define MCUBE_USE_WAIT_QUEUE
/* register address define*/
#define MC34XX_REG_XOUT								0x00
#define MC34XX_REG_YOUT								0x01
#define MC34XX_REG_ZOUT								0x02
#define MC34XX_REG_INTERRUPT_ENABLE					0x06
#define MC34XX_REG_MODE_FEATURE						0x07
#define MC34XX_REG_SAMPLE_RATE						0x08
#define MC34XX_REG_XOUT_EX_L						0x0D
#define MC34XX_REG_XOUT_EX_H						0x0E
#define MC34XX_REG_YOUT_EX_L						0x0F
#define MC34XX_REG_YOUT_EX_H						0x10
#define MC34XX_REG_ZOUT_EX_L						0x11
#define MC34XX_REG_ZOUT_EX_H						0x12
#define MC34XX_REG_CHIPID							0x18
#define MC34XX_REG_LPF_RANGE_RES					0x20
#define MC34XX_REG_MCLK_POLARITY					0x2A
#define MC34XX_REG_PRODUCT_CODE_H					0x34
#define MC34XX_REG_PRODUCT_CODE_L					0x3B
#ifndef MERAK
#define MERAK			/*MENSA = MC3413, 3433 */
#define MC34X3_REG_TILT_STATUS						0x03
#define MC34X3_REG_SAMPLE_RATE_STATUS				0x04
#define MC34X3_REG_SLEEP_COUNT						0x05
#define MC34X3_REG_TAP_DETECTION_ENABLE				0x09
#define MC34X3_REG_TAP_DWELL_REJECT					0x0A
#define MC34X3_REG_DROP_CONTROL						0x0B
#define MC34X3_REG_SHAKE_DEBOUNCE					0x0C
#define MC34X3_REG_SDM_X							0x14
#define MC34X3_REG_MISC								0x17
#define MC34X3_REG_SHAKE_THRESHOLD					0x2B
#define MC34X3_REG_UD_Z_TH							0x2C
#define MC34X3_REG_UD_X_TH							0x2D
#define MC34X3_REG_RL_Z_TH							0x2E
#define MC34X3_REG_RL_Y_TH							0x2F
#define MC34X3_REG_FB_Z_TH							0x30
#define MC34X3_REG_DROP_THRESHOLD					0x31
#define MC34X3_REG_TAP_THRESHOLD					0x32
#define MC34X3_REG_PRODUCT_CODE						0x3B
#endif
#ifndef MENSA
#define MENSA			/*MENSA = MC3416, 3436 */
#define MC34X6_REG_STATUS_1							0x03
#define MC34X6_REG_INTR_STAT_1						0x04
#define MC34X6_REG_DEVICE_STATUS					0x05
#define MC34X6_REG_MOTION_CTRL						0x09
#define MC34X6_REG_STATUS_2							0x13
#define MC34X6_REG_INTR_STAT_2						0x14
#define MC34X6_REG_SDM_X							0x15
#define MC34X6_REG_RANGE_CONTROL					0x20
#define MC34X6_REG_MISC								0x2C
#define MC34X6_REG_TF_THRESHOLD_LSB					0x40
#define MC34X6_REG_TF_THRESHOLD_MSB					0x41
#define MC34X6_REG_TF_DB							0x42
#define MC34X6_REG_AM_THRESHOLD_LSB					0x43
#define MC34X6_REG_AM_THRESHOLD_MSB					0x44
#define MC34X6_REG_AM_DB							0x45
#define MC34X6_REG_SHK_THRESHOLD_LSB				0x46
#define MC34X6_REG_SHK_THRESHOLD_MSB				0x47
#define MC34X6_REG_PK_P2P_DUR_THRESHOLD_LSB			0x48
#define MC34X6_REG_PK_P2P_DUR_THRESHOLD_MSB			0x49
#define MC34X6_REG_TIMER_CTRL						0x4A
#endif
/* mode*/
#define MC34XX_MODE_AUTO			0
#define MC34XX_MODE_WAKE			1
#define MC34XX_MODE_SNIFF			2
#define MC34XX_MODE_STANDBY			3
/* range*/
#define MC34XX_RANGE_2G				0
#define MC34XX_RANGE_4G				1
#define MC34XX_RANGE_8G_10BIT		2
#define MC34XX_RANGE_8G_14BIT		3
#define MC34XX_RESOLUTION_LOW		1
#define MC34XX_RESOLUTION_HIGH		2
/* bandwidth*/
#define MC34XX_BW_512HZ				0
#define MC34XX_BW_256HZ				1
#define MC34XX_BW_128HZ				2
#define MC34XX_BW_64HZ				3
#define MC34XX_BW_32HZ				4
#define MC34XX_BW_16HZ				5
#define MC34XX_BW_8HZ				6
/* initial value*/
#define MC34XX_RANGE_SET			MC34XX_RANGE_8G_14BIT	/* +/-8g, 14bit */
#define MC34XX_BW_SET				MC34XX_BW_256HZ /*MC34XX_BW_128HZ // 128HZ */
#define MC34XX_MAX_DELAY			200
#define ABSMIN_8G					(-8 * 1024)
#define ABSMAX_8G					(8 * 1024)
#define ABSMIN_2G					(-2 * 64)
#define ABSMAX_2G					(2 * 64)
/* 1g constant value*/
#define GRAVITY_1G_VALUE			1024
/* product code*/
#define MC34XX_PCODE_3413	  0x10
#define MC34XX_PCODE_3433	  0x60
#define MC34XX_PCODE_3416	  0x20
#define MC34XX_PCODE_3436	  0x21
#define MC34XX_IOC_MAGIC				77	/*0x1D */
#define MC34XX_ACC_IOCTL_SET_DELAY		_IOW(MC34XX_IOC_MAGIC, 0, int)
#define MC34XX_ACC_IOCTL_GET_DELAY		_IOR(MC34XX_IOC_MAGIC, 1, int)
#define MC34XX_ACC_IOCTL_SET_ENABLE		_IOW(MC34XX_IOC_MAGIC, 2, int)
#define MC34XX_ACC_IOCTL_GET_ENABLE		_IOR(MC34XX_IOC_MAGIC, 3, int)
#define MC34XX_ACC_IOCTL_CALIBRATION		_IOW(MC34XX_IOC_MAGIC, 4, int)
#if defined(GSENSOR_INTERRUPT_MODE)
#define MC34XX_INT 139
static struct workqueue_struct *mc34xx_wq;
#endif
/* macro define*/
#define delay_to_jiffies(d) ((d) ? msecs_to_jiffies(d) : 1)
#define actual_delay(d)		(jiffies_to_msecs(delay_to_jiffies(d)))
/* data declare*/
enum mc34xx_orientation {
	MC34XX_TOP_LEFT_DOWN = 0,
	MC34XX_TOP_RIGHT_DOWN,
	MC34XX_TOP_RIGHT_UP,
	MC34XX_TOP_LEFT_UP,
	MC34XX_BOTTOM_LEFT_DOWN,
	MC34XX_BOTTOM_RIGHT_DOWN,
	MC34XX_BOTTOM_RIGHT_UP,
	MC34XX_BOTTOM_LEFT_UP
};
enum mc34xx_axis {
	MC34XX_AXIS_X = 0,
	MC34XX_AXIS_Y,
	MC34XX_AXIS_Z,
	MC34XX_AXIS_NUM
};
struct mc34xxacc {
	signed short x;
	signed short y;
	signed short z;
};
struct mc34xx_hwmsen_convert {
	signed char sign[3];
	unsigned char map[3];
};
struct mc34xx_data {
	atomic_t enable;	/* attribute value */
	unsigned int on_before_suspend;
	atomic_t delay;		/* attribute value */
	atomic_t position;	/* attribute value */
	atomic_t threshold;	/* attribute value */
	struct mc34xxacc value;
	struct mutex value_mutex;
	struct mutex enable_mutex;
	struct i2c_client *client;
	struct input_dev *input;
	struct delayed_work work;
	unsigned char product_code;
	unsigned char resolution;
#ifdef MCUBE_USE_WAIT_QUEUE
	struct hrtimer hrtimer;
#endif
};
/******************************
*** CONFIGURATIONS
******************************/
/*#define MCUBE_DOT_CALIBRATION*/
/* #define MCUBE_FUNC_DEBUG */
/*#define I2C_BUS_NUM_STATIC_ALLOC*/
#define MCUBE_I2C_BURST_LIMIT_2_BYTES
/*#define SUPPORT_VPROXIMITY_SENSOR*/
#define MC34XX_DEV_NAME		  MC34XX_ACC_I2C_NAME
#define MC34XX_DEV_VERSION	  "2.0.3"
#define MC34XX_INPUT_NAME	  "accelerometer"
#define MC34XX_I2C_ADDR		  MC34XX_ACC_I2C_ADDR
#define IS_MERAK()	((0xC0 == s_bHWID) || (0x40 == s_bHWID) || (0x20 == s_bHWID))
#define IS_MENSA()	(0xA0 == s_bHWID)
#if defined(ZCFG_GSENSOR_MC34XX_DIRECTION)
static unsigned char mc34xx_current_placement = ZCFG_GSENSOR_MC34XX_DIRECTION;	/* current soldered placement */
#else
static unsigned char mc34xx_current_placement = MC34XX_BOTTOM_RIGHT_UP;	/*MC34XX_BOTTOM_LEFT_UP; MC34XX_TOP_LEFT_UP;  current soldered placement */
#endif
#ifdef MCUBE_FUNC_DEBUG
#define MC_PRINT(x...)		  printk(x)
#define MC_ERR_PRINT(x...)	  printk(x)
#else
#define MC_PRINT(x...)
#define MC_ERR_PRINT(x...)
#endif
static char chip_info[20];
/* Transformation matrix for chip mounting position*/
static const struct mc34xx_hwmsen_convert mc34xx_cvt[] = {
	{ {1, 1, 1}, {MC34XX_AXIS_X, MC34XX_AXIS_Y, MC34XX_AXIS_Z} },	/* 0: top		, left-down */
	{ { -1, 1, 1}, {MC34XX_AXIS_Y, MC34XX_AXIS_X, MC34XX_AXIS_Z} },	/* 1: top		, right-down */
	{ { -1, -1, 1}, {MC34XX_AXIS_X, MC34XX_AXIS_Y, MC34XX_AXIS_Z} },	/* 2: top		, right-up */
	{ {1, -1, 1}, {MC34XX_AXIS_Y, MC34XX_AXIS_X, MC34XX_AXIS_Z} },	/* 3: top		, left-up */
	{ { -1, 1, -1}, {MC34XX_AXIS_X, MC34XX_AXIS_Y, MC34XX_AXIS_Z} },	/* 4: bottom, left-down */
	{ {1, 1, -1}, {MC34XX_AXIS_Y, MC34XX_AXIS_X, MC34XX_AXIS_Z} },	/* 5: bottom, right-down */
	{ {1, -1, -1}, {MC34XX_AXIS_X, MC34XX_AXIS_Y, MC34XX_AXIS_Z} },	/* 6: bottom, right-up */
	{ { -1, -1, -1}, {MC34XX_AXIS_Y, MC34XX_AXIS_X, MC34XX_AXIS_Z} },	/* 7: bottom, left-up */
};

static struct i2c_client *mc34xx_client;
static GSENSOR_VECTOR3D mc34xx_gain = { 0 };

#ifdef I2C_BUS_NUM_STATIC_ALLOC
#define I2C_STATIC_BUS_NUM		 0
static struct i2c_board_info mc34xx_i2c_boardinfo = {
	I2C_BOARD_INFO(MC34XX_DEV_NAME, MC34XX_I2C_ADDR)
};
#endif
static unsigned char s_bResolution;
static unsigned char s_bPCODE;
static unsigned char s_bHWID;
static unsigned char s_bMPOL;
static unsigned short mc34xx_i2c_auto_probe_addr[] = { 0x4C, 0x6C, 0x4E, 0x6D, 0x6E, 0x6F };
#ifdef MCUBE_USE_WAIT_QUEUE
static struct task_struct *thread;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static int gsensor_flag;
#endif
#ifdef MCUBE_DOT_CALIBRATION
#define CALIB_PATH			"/data/data/com.mcube.acc/files/mcube-calib.txt"
#define DATA_PATH			"/sdcard/mcube-register-map.txt"
/*static char file_path[128] = "/data/data/com.mcube.acc/files/mcube-calib.txt";*/
/*static char file_path[128] = "/productinfo/mcube-calib.txt";*/
/*static char factory_path[128] ="/data/data/com.mcube.acc/files/fac-calib.txt";*/
/*static GSENSOR_VECTOR3D mc34xx_gain;*/
static SENSOR_DATA gain_data = { 0 }, offset_data = {
	0
};
static unsigned char offset_buf[9] = { 0 };

static struct file *fd_file;
static int load_cali_cnt = 30;
static bool IsRbmMode;
static mm_segment_t oldfs;
static SENSOR_DATA mc34xx_cali_data = { 0 };

static void mc34xx_read_cali_file(struct mc34xx_data *mc34xx);
static int mc34xx_read_true_data(struct mc34xx_data *mc34xx,
								 struct mc34xxacc *acc);
#endif
static inline int mc34xx_smbus_read_byte(struct i2c_client *client,
		unsigned char reg_addr,
		unsigned char *data)
{
	signed int dummy = 0;
	dummy = i2c_smbus_read_byte_data(client, reg_addr);

	if (dummy < 0)
		return dummy;

	*data = dummy & 0x000000ff;
	return 0;
}

static inline int mc34xx_smbus_write_byte(struct i2c_client *client,
		unsigned char reg_addr,
		unsigned char *data)
{
	signed int dummy = 0;
	dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);

	if (dummy < 0)
		return dummy;

	return 0;
}

static inline int mc34xx_smbus_read_block(struct i2c_client *client,
		unsigned char reg_addr,
		unsigned char *data,
		unsigned char len)
{
	signed int dummy = 0;
#ifdef MCUBE_I2C_BURST_LIMIT_2_BYTES

	for (; len > 1; (len -= 2)) {
		dummy =
			i2c_smbus_read_i2c_block_data(client, reg_addr, 2, data);

		if (dummy < 0)
			return dummy;

		reg_addr += 2;
		data += 2;
	}

	if (0 == len)
		return 0;

#endif
	dummy = i2c_smbus_read_i2c_block_data(client, reg_addr, len, data);

	if (dummy < 0)
		return dummy;

	return 0;
}

static inline int mc34xx_smbus_write_block(struct i2c_client *client,
		unsigned char reg_addr,
		unsigned char *data,
		unsigned char len)
{
	signed int dummy = 0;
#ifdef MCUBE_I2C_BURST_LIMIT_2_BYTES

	for (; len > 1; (len -= 2)) {
		dummy =
			i2c_smbus_write_i2c_block_data(client, reg_addr, 2, data);

		if (dummy < 0)
			return dummy;

		reg_addr += 2;
		data += 2;
	}

	if (0 == len)
		return 0;

#endif
	dummy = i2c_smbus_write_i2c_block_data(client, reg_addr, len, data);

	if (dummy < 0)
		return dummy;

	return 0;
}

static bool mc34xx_validate_pcode(unsigned char *pbPCode, unsigned char *pbHwID)
{
	MC_PRINT("[%s] raw data pbHwID=%02X,pbPCode=%02X\n", __func__, *pbHwID,
			 *pbPCode);
	*pbPCode = *pbPCode & 0xF1;
	*pbHwID = *pbHwID & 0xF0;

	if (0xA0 == *pbHwID) {
		if ((MC34XX_PCODE_3416 == *pbPCode)
			|| (MC34XX_PCODE_3436 == *pbPCode))
			return true;
	} else if ((0xC0 == *pbHwID) || (0x40 == *pbHwID) || (0x20 == *pbHwID)) {
		if ((MC34XX_PCODE_3413 == *pbPCode)
			|| (MC34XX_PCODE_3433 == *pbPCode))
			return true;
	}

	return false;
}

static int mc34xx_detect_pcode(struct i2c_client *client)
{
	int ret = 0;
	unsigned char product_code = 0;
	unsigned char hardware_id = 0;
	int _nProbeAddrCount =
		(sizeof(mc34xx_i2c_auto_probe_addr) /
		 sizeof(mc34xx_i2c_auto_probe_addr[0]));
	int _nCount = 0;
	MC_PRINT("%s: entered\n", __func__);

	for (_nCount = 0; _nCount < _nProbeAddrCount; _nCount++) {
		client->addr = mc34xx_i2c_auto_probe_addr[_nCount];
		ret =
			mc34xx_smbus_read_byte(client, MC34XX_REG_PRODUCT_CODE_L,
								   &product_code);

		if (ret) {
			MC_PRINT("%s: read error: %x\n", __func__, ret);
			continue;
		}

		ret =
			mc34xx_smbus_read_byte(client, MC34XX_REG_CHIPID,
								   &hardware_id);

		if (ret) {
			MC_PRINT("%s: read error: %x\n", __func__, ret);
			continue;
		}

		if (true == mc34xx_validate_pcode(&product_code, &hardware_id)) {
			s_bPCODE = product_code;
			s_bHWID = hardware_id;
			return true;
		}
	}

	return -1;
}

static int mc34xx_set_mode(struct i2c_client *client, unsigned char mode)
{
	int comres = 0;
	unsigned char data = 0;

	if (4 > mode) {
		if (MC34XX_MODE_WAKE == mode) {
			data = 0xc1;	/*Interrupt pin INTN is active high		 and  Interrupt pin INTN is push-pull. No external pull-up resistor should be installed. */
		} else if (MC34XX_MODE_STANDBY == mode) {
			data = 0xc3;
		}

		comres +=
			mc34xx_smbus_write_byte(client, MC34XX_REG_MODE_FEATURE,
									&data);
	} else {
		comres = -1;
	}

	return comres;
}

static int mc34xx_get_mode(struct i2c_client *client, unsigned char *mode)
{
	int comres = 0;
	unsigned char data = 0;
	comres = mc34xx_smbus_read_byte(client, MC34XX_REG_MODE_FEATURE, &data);
	*mode = data & 0x03;
	return comres;
}

static int mc34xx_get_range(struct i2c_client *client, unsigned char *range)
{
	int comres = 0;
	unsigned char data = 0;
	comres =
		mc34xx_smbus_read_byte(client, MC34XX_REG_LPF_RANGE_RES, &data);
	*range = ((data >> 2) & 0x03);
	return comres;
}

static int mc34xx_get_bandwidth(struct i2c_client *client, unsigned char *BW)
{
	int comres = 0;
	unsigned char data = 0;
	comres =
		mc34xx_smbus_read_byte(client, MC34XX_REG_LPF_RANGE_RES, &data);
	*BW = ((data >> 4) & 0x07);
	return comres;
}

/*****************************************
*** MC34XX_SetResolution
*****************************************/
static void MC34XX_SetResolution(void)
{
	MC_PRINT("[%s]\n", __func__);

	switch (s_bPCODE) {
	case MC34XX_PCODE_3433:
	case MC34XX_PCODE_3436:
		s_bResolution = MC34XX_RESOLUTION_LOW;
		break;

	case MC34XX_PCODE_3413:
	case MC34XX_PCODE_3416:
		s_bResolution = MC34XX_RESOLUTION_HIGH;
		break;

	default:
		MC_ERR_PRINT("ERR: no resolution assigned!\n");
		break;
	}

	if (MC34XX_RESOLUTION_HIGH == s_bResolution) {
		MC_PRINT("[%s] s_bResolution: %d(MC34XX_RESOLUTION_HIGH)\n",
				 __func__, s_bResolution);
	} else if (MC34XX_RESOLUTION_LOW == s_bResolution) {
		MC_PRINT("[%s] s_bResolution: %d(MC34XX_RESOLUTION_LOW)\n",
				 __func__, s_bResolution);
	}
}

/*****************************************
*** MC34XX_SetSampleRate
*****************************************/
static void MC34XX_SetSampleRate(struct i2c_client *mc34xx_client)
{
	unsigned char _baDataBuf[2] = { 0 }, _baData2Buf[2] = {
		0
	};
	MC_PRINT("[%s]\n", __func__);
	_baDataBuf[0] = MC34XX_REG_SAMPLE_RATE;
	_baDataBuf[1] = 0x00;

	if (IS_MERAK()) {
		mc34xx_smbus_read_block(mc34xx_client, MC34XX_REG_MCLK_POLARITY,
								_baData2Buf, 1);
		MC_PRINT("[%s MERAK] REG(0x2A) = 0x%02X\n", __func__,
				 _baData2Buf[0]);
		_baData2Buf[0] = (_baData2Buf[0] & 0xC0);
		mc34xx_smbus_read_block(mc34xx_client, MC34XX_REG_SAMPLE_RATE,
								_baDataBuf, 1);

		switch (_baData2Buf[0]) {
		case 0x00:
			_baDataBuf[0] = ((_baDataBuf[0] & 0xF0) | 0x00);	/*CLK =2.56MHz,ODR=128Hz */
			break;

		case 0x40:
			_baDataBuf[0] = ((_baDataBuf[0] & 0xF0) | 0x08);	/*CLK =1.28MHz,ODR=128Hz */
			break;

		case 0x80:
			_baDataBuf[0] = ((_baDataBuf[0] & 0xF0) | 0x09);	/*CLK =640KHz,ODR=128Hz */
			break;

		case 0xC0:
			_baDataBuf[0] = ((_baDataBuf[0] & 0xF0) | 0x0A);	/*CLK =320KHz,ODR=128Hz */
			break;

		default:
			MC_ERR_PRINT
			("[%s] no chance to get here... check code!\n",
			 __func__);
			break;
		}

		MC_PRINT("[%s MERAK] REG(0x08) = 0x%02X\n", __func__,
				 _baDataBuf[0]);
	} else if (IS_MENSA()) {
		mc34xx_smbus_read_block(mc34xx_client, MC34XX_REG_SAMPLE_RATE,
								_baDataBuf, 1);
		_baDataBuf[0] = ((_baDataBuf[0] & 0xF8) | 0x04);	/*CLK =2.56MHz,OSR=64,ODR=2048Hz */
		MC_PRINT("[%s MENSA] REG(0x08) = 0x%02X\n", __func__,
				 _baDataBuf[0]);
	} else
		_baDataBuf[0] = 0x00;

	mc34xx_smbus_write_block(mc34xx_client, MC34XX_REG_SAMPLE_RATE,
							 _baDataBuf, 1);
}

/*****************************************
*** MC34XX_LowPassFilter
*****************************************/
static void MC34XX_LowPassFilter(struct i2c_client *mc34xx_client)
{
	unsigned char _baDataBuf[2] = { 0 };
	int res = 0;

	if (IS_MENSA()) {
		res =
			mc34xx_smbus_read_block(mc34xx_client,
									MC34XX_REG_LPF_RANGE_RES,
									_baDataBuf, 1);
		MC_PRINT("[%s MENSA] Read REG(0x20) = 0x%02X\n", __func__,
				 _baDataBuf[0]);
		_baDataBuf[0] = ((_baDataBuf[0] & 0xF0) | 0x09);	/* 128Hz @ 2048 Hz ODR */
		res =
			mc34xx_smbus_write_block(mc34xx_client,
									 MC34XX_REG_LPF_RANGE_RES,
									 _baDataBuf, 1);

		if (res < 0)
			MC_PRINT("[%s] MENSA LPF failed.\n", __func__);
		else
			MC_PRINT
			("[%s] Write REG(0x20) = 0x%02X,MENSA LPF enable success.\n",
			 __func__, _baDataBuf[0]);
	} else if (IS_MERAK()) {
		MC_PRINT("[%s] MERAK not support LPF.\n", __func__);
	}
}

/*****************************************
*** MC34XX_ConfigResRange
*****************************************/
static void MC34XX_ConfigResRange(struct i2c_client *mc34xx_client)
{
	unsigned char _baDataBuf[2] = { 0 };
	int res = 0;
	mc34xx_smbus_read_block(mc34xx_client, MC34XX_REG_LPF_RANGE_RES,
							_baDataBuf, 1);
	MC_PRINT("[%s] Read REG(0x20) = 0x%02X\n", __func__, _baDataBuf[0]);

	if (IS_MERAK()) {
		if (MC34XX_RESOLUTION_LOW == s_bResolution)
			_baDataBuf[0] = ((_baDataBuf[0] & 0x88) | 0x02);	/*8bit,(+2g~-2g) */
		else
			_baDataBuf[0] = ((_baDataBuf[0] & 0x88) | 0x25);	/*14bit,(+8g~-8g) */
	} else if (IS_MENSA()) {
		if (MC34XX_RESOLUTION_LOW == s_bResolution)
			_baDataBuf[0] = ((_baDataBuf[0] & 0x0F) | 0x80);	/*8bit,(+2g~-2g) */
		else
			_baDataBuf[0] = ((_baDataBuf[0] & 0x0F) | 0x20);	/*16bit,(+8g~-8g) */
	}

	res =
		mc34xx_smbus_write_block(mc34xx_client, MC34XX_REG_LPF_RANGE_RES,
								 _baDataBuf, 1);

	if (res < 0)
		MC_ERR_PRINT("MC34XX_ConfigResRange fail\n");

	MC_PRINT("[%s] Write REG(0x20)resolution and range set 0x%02X\n",
			 __func__, _baDataBuf[0]);
}

/*****************************************
*** MC34XX_SetGain
*****************************************/
static void MC34XX_SetGain(void)
{
	if (IS_MERAK()) {
		if (MC34XX_RESOLUTION_LOW == s_bResolution) {
			mc34xx_gain.x = mc34xx_gain.y = mc34xx_gain.z = (1 << 8) / (1 << 2);	/*8bit,(+2g~-2g)-->64; */
		} else if (MC34XX_RESOLUTION_HIGH == s_bResolution) {
			mc34xx_gain.x = mc34xx_gain.y = mc34xx_gain.z = (1 << 14) / (1 << 4);	/*14bit,(+8g~-8g)-->1024 */
		}
	} else if (IS_MENSA()) {
		if (MC34XX_RESOLUTION_LOW == s_bResolution) {
			mc34xx_gain.x = mc34xx_gain.y = mc34xx_gain.z = (1 << 8) / (1 << 2);	/*8bit,(+2g~-2g)-->64; */
		} else if (MC34XX_RESOLUTION_HIGH == s_bResolution) {
			mc34xx_gain.x = mc34xx_gain.y = mc34xx_gain.z = (1 << 16) / (1 << 4);	/*16bit,(+8g~-8g)-->4096 */
		}
	}

	MC_PRINT("[%s] gain: %d / %d / %d\n", __func__, mc34xx_gain.x,
			 mc34xx_gain.y, mc34xx_gain.z);
}

static int mc34xx_init(struct mc34xx_data *mc34xx)
{
	unsigned char _baDataBuf[2] = { 0 };
	_baDataBuf[0] = 0x43;
	mc34xx_smbus_read_block(mc34xx->client, MC34XX_REG_MODE_FEATURE,
							_baDataBuf, 1);
	MC34XX_SetResolution();
	MC34XX_SetSampleRate(mc34xx->client);
	MC34XX_LowPassFilter(mc34xx->client);
	MC34XX_ConfigResRange(mc34xx->client);
	MC34XX_SetGain();

	if (IS_MENSA()) {
		_baDataBuf[0] = 0x04;
		mc34xx_smbus_write_block(mc34xx->client, MC34X6_REG_MOTION_CTRL,
								 _baDataBuf, 1);
	}

	if (IS_MERAK()) {
		_baDataBuf[0] = 0x00;
		mc34xx_smbus_write_block(mc34xx->client,
								 MC34XX_REG_INTERRUPT_ENABLE,
								 _baDataBuf, 1);
	} else if (IS_MENSA()) {
		_baDataBuf[0] = 0x00;
		mc34xx_smbus_write_block(mc34xx->client,
								 MC34XX_REG_INTERRUPT_ENABLE,
								 _baDataBuf, 1);
	}

	mc34xx_smbus_read_block(mc34xx->client, MC34XX_REG_MCLK_POLARITY,
							_baDataBuf, 1);
	s_bMPOL = (_baDataBuf[0] & 0x03);
	MC_PRINT("[%s] init ok.\n", __func__);
	return true;
}

static int mc34xx_read_accel_xyz(struct mc34xx_data *mc34xx,
								 struct mc34xxacc *acc, int orient)
{
	int comres = -1;
	unsigned char data[6] = { 0 };
	/*s16 tmp = 0; */
	s16 raw[3] = { 0 };
	const struct mc34xx_hwmsen_convert *pCvt = NULL;

	if (MC34XX_RESOLUTION_HIGH == s_bResolution) {
		comres =
			mc34xx_smbus_read_block(mc34xx->client,
									MC34XX_REG_XOUT_EX_L, data, 6);
		raw[0] = (s16)(data[0] + (data[1] << 8));
		raw[1] = (s16)(data[2] + (data[3] << 8));
		raw[2] = (s16)(data[4] + (data[5] << 8));
	} else {
		comres =
			mc34xx_smbus_read_block(mc34xx->client, MC34XX_REG_XOUT,
									data, 3);
		raw[0] = (s8) data[0];
		raw[1] = (s8) data[1];
		raw[2] = (s8) data[2];
	}

	if (comres) {
		MC_ERR_PRINT("%s: i2c error!\n", __func__);
		return comres;
	}

#ifdef MCUBE_FUNC_DEBUG
	printk("%s: %d, %d, %d\n", __func__, raw[0], raw[1], raw[2]);
#endif
	raw[0] = raw[0] * GRAVITY_1G_VALUE / mc34xx_gain.x;
	raw[1] = raw[1] * GRAVITY_1G_VALUE / mc34xx_gain.y;
	raw[2] = raw[2] * GRAVITY_1G_VALUE / mc34xx_gain.z;

	if (s_bMPOL & 0x01)
		raw[0] = -raw[0];

	if (s_bMPOL & 0x02)
		raw[0] = -raw[0];

	pCvt = &mc34xx_cvt[orient];
	acc->x = pCvt->sign[MC34XX_AXIS_X] * raw[pCvt->map[MC34XX_AXIS_X]];
	acc->y = pCvt->sign[MC34XX_AXIS_Y] * raw[pCvt->map[MC34XX_AXIS_Y]];
	acc->z = pCvt->sign[MC34XX_AXIS_Z] * raw[pCvt->map[MC34XX_AXIS_Z]];
	return comres;
}

#ifndef GSENSOR_INTERRUPT_MODE
static void mc34xx_work_func(struct work_struct *work)
{
	/*static s16 temp_acc = 0; */
	static struct mc34xxacc acc = { 0 };
#ifdef MCUBE_USE_WAIT_QUEUE
	struct mc34xx_data *mc34xx = i2c_get_clientdata(mc34xx_client);
#else
	struct mc34xx_data *mc34xx =
		container_of((struct delayed_work *)work, struct mc34xx_data, work);
	unsigned long delay = msecs_to_jiffies(atomic_read(&mc34xx->delay));
#endif
#if 0				/*def MCUBE_DOT_CALIBRATION */
	mc34xx_read_cali_file(mc34xx);
	mc34xx_read_true_data(mc34xx, &acc);
#endif
	mc34xx_read_accel_xyz(mc34xx, &acc, mc34xx_current_placement);
	/* printk( "%s: %d, %d, %d\n",__func__, acc.x, acc.y, acc.z); */
	input_report_abs(mc34xx->input, ABS_X, acc.y);
	input_report_abs(mc34xx->input, ABS_Y, -acc.x);
	input_report_abs(mc34xx->input, ABS_Z, acc.z);
	input_sync(mc34xx->input);
	mutex_lock(&mc34xx->value_mutex);
	mc34xx->value = acc;
	mutex_unlock(&mc34xx->value_mutex);
#ifdef MCUBE_USE_WAIT_QUEUE
	/*schedule_delayed_work(&mc34xx->work, delay); */
#else
	schedule_delayed_work(&mc34xx->work, delay);
#endif
}
#endif
static int mc34xx_input_init(struct mc34xx_data *mc34xx)
{
	struct input_dev *dev = NULL;
	int err = 0;
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called input init\n", __func__);
#endif
	dev = input_allocate_device();

	if (!dev) {
		pr_err("%s: can't allocate device!\n", __func__);
		return -ENOMEM;
	}

	dev->name = MC34XX_INPUT_NAME;
	dev->id.bustype = BUS_I2C;
	input_set_capability(dev, EV_ABS, ABS_MISC);

	if (MC34XX_RESOLUTION_HIGH == s_bResolution) {
		input_set_abs_params(dev, ABS_X, ABSMIN_8G, ABSMAX_8G, 0, 0);
		input_set_abs_params(dev, ABS_Y, ABSMIN_8G, ABSMAX_8G, 0, 0);
		input_set_abs_params(dev, ABS_Z, ABSMIN_8G, ABSMAX_8G, 0, 0);
	} else {
		input_set_abs_params(dev, ABS_X, ABSMIN_2G, ABSMAX_2G, 0, 0);
		input_set_abs_params(dev, ABS_Y, ABSMIN_2G, ABSMAX_2G, 0, 0);
		input_set_abs_params(dev, ABS_Z, ABSMIN_2G, ABSMAX_2G, 0, 0);
	}

	input_set_drvdata(dev, mc34xx);
	err = input_register_device(dev);

	if (err < 0) {
		pr_err("%s: can't register device!\n", __func__);
		input_free_device(dev);
		return err;
	}

	mc34xx->input = dev;
	return 0;
}

static void mc34xx_input_deinit(struct mc34xx_data *mc34xx)
{
	struct input_dev *dev = mc34xx->input;
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	input_unregister_device(dev);
	input_free_device(dev);
}

static ssize_t mc34xx_register_store(struct device *dev,
									 struct device_attribute *attr,
									 const char *buf, size_t count)
{
	int address = 0, value = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc34xx_data *mc34xx = i2c_get_clientdata(client);
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	sscanf(buf, "%d %d", &address, &value);

	if (0 != mc34xx_smbus_write_byte(mc34xx->client, (unsigned char)address,
									 (unsigned char *)&value))
		return -EINVAL;

	return count;
}

static ssize_t mc34xx_register_show(struct device *dev,
									struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc34xx_data *mc34xx = i2c_get_clientdata(client);
	size_t count = 0;
	u8 reg[0x40] = { 0 };
	int i = 0;
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif

	for (i = 0; i < 0x40; i++) {
		mc34xx_smbus_read_byte(mc34xx->client, i, reg + i);
		count += sprintf(&buf[count], "0x%x: %d\n", i, reg[i]);
	}

	return count;
}

static ssize_t mc34xx_range_show(struct device *dev,
								 struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc34xx_data *mc34xx = i2c_get_clientdata(client);
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif

	if (mc34xx_get_range(mc34xx->client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t mc34xx_bandwidth_show(struct device *dev,
									 struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc34xx_data *mc34xx = i2c_get_clientdata(client);
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif

	if (mc34xx_get_bandwidth(mc34xx->client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t mc34xx_position_show(struct device *dev,
									struct device_attribute *attr, char *buf)
{
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	return sprintf(buf, "%d\n", mc34xx_current_placement);
}

static ssize_t mc34xx_position_store(struct device *dev,
									 struct device_attribute *attr,
									 const char *buf, size_t count)
{
	unsigned long position = 0;
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	position = simple_strtoul(buf, NULL, 10);

	if ((position >= 0) && (position <= 7)) {
		mc34xx_current_placement = position;
	}

	return count;
}

static ssize_t mc34xx_mode_show(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc34xx_data *mc34xx = i2c_get_clientdata(client);
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif

	if (mc34xx_get_mode(mc34xx->client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t mc34xx_mode_store(struct device *dev,
								 struct device_attribute *attr,
								 const char *buf, size_t count)
{
	unsigned long data = 0;
	int error = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct mc34xx_data *mc34xx = i2c_get_clientdata(client);
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	error = kstrtoul(buf, 10, &data);

	if (error)
		return error;

	if (mc34xx_set_mode(mc34xx->client, (unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t mc34xx_value_show(struct device *dev,
								 struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct mc34xx_data *mc34xx = input_get_drvdata(input);
	struct mc34xxacc acc_value = { 0 };
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	mutex_lock(&mc34xx->value_mutex);
	acc_value = mc34xx->value;
	mutex_unlock(&mc34xx->value_mutex);
	return sprintf(buf, "%d %d %d\n", acc_value.x, acc_value.y,
				   acc_value.z);
}

static ssize_t mc34xx_delay_show(struct device *dev,
								 struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc34xx_data *mc34xx = i2c_get_clientdata(client);
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	return sprintf(buf, "%d\n", atomic_read(&mc34xx->delay));
}

static ssize_t mc34xx_version_show(struct device *dev,
								   struct device_attribute *attr, char *buf)
{
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	return sprintf(buf, "%s\n", MC34XX_DEV_VERSION);
}

static ssize_t mc34xx_product_code_show(struct device *dev,
										struct device_attribute *attr,
										char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc34xx_data *mc34xx = i2c_get_clientdata(client);
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	return sprintf(buf, "0x%02X\n", mc34xx->product_code);
}

static int mc34xx_get_enable(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc34xx_data *mc34xx = i2c_get_clientdata(client);
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	return atomic_read(&mc34xx->enable);
}

static void mc34xx_set_enable(struct device *dev, int enable)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc34xx_data *mc34xx = i2c_get_clientdata(client);
	int pre_enable = atomic_read(&mc34xx->enable);
	/*unsigned char data = 0; */
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called set enable %d\n", __func__, enable);
#endif
	mutex_lock(&mc34xx->enable_mutex);

	if (enable) {
		if (pre_enable == 0) {
			mc34xx_set_mode(mc34xx->client, MC34XX_MODE_WAKE);
#ifdef MCUBE_USE_WAIT_QUEUE
			hrtimer_start(&mc34xx->hrtimer,
						  ktime_set(atomic_read(&mc34xx->delay) /
									1000,
									(atomic_read(&mc34xx->delay) %
									 1000) * 1000000),
						  HRTIMER_MODE_REL);
#else
			schedule_delayed_work(&mc34xx->work,
								  msecs_to_jiffies(atomic_read
												   (&mc34xx->
													delay)));
#endif
			atomic_set(&mc34xx->enable, 1);
		}
	} else {
		if (pre_enable == 1) {
#ifdef MCUBE_USE_WAIT_QUEUE
			hrtimer_cancel(&mc34xx->hrtimer);
			mc34xx_set_mode(mc34xx->client, MC34XX_MODE_STANDBY);
#else
			mc34xx_set_mode(mc34xx->client, MC34XX_MODE_STANDBY);
			cancel_delayed_work_sync(&mc34xx->work);
#endif
			atomic_set(&mc34xx->enable, 0);
		}
	}

	mutex_unlock(&mc34xx->enable_mutex);
}

static ssize_t mc34xx_enable_show(struct device *dev,
								  struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc34xx_data *mc34xx = i2c_get_clientdata(client);
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	return sprintf(buf, "%d\n", atomic_read(&mc34xx->enable));
}

static ssize_t mc34xx_enable_store(struct device *dev,
								   struct device_attribute *attr,
								   const char *buf, size_t count)
{
	unsigned int enable = simple_strtoul(buf, NULL, 10);
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called %d\n", __func__, enable);
#endif

	if (enable)
		mc34xx_set_enable(dev, 1);
	else
		mc34xx_set_enable(dev, 0);

	return count;
}

static int mc34xx_get_delay(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc34xx_data *mc32x0 = i2c_get_clientdata(client);
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	return atomic_read(&mc32x0->delay);
}

/*
if (copy_from_user(&temp, argp, sizeof(temp)))
{
MC_ERR_PRINT("%s: set delay copy error!\n", __func__);
return -EFAULT;
}
if (temp < 0 || temp > 1000)
{
MC_ERR_PRINT("%s: set delay over limit!\n", __func__);
return -EINVAL;
}
mc34xx_set_delay(&(mc34xx->client->dev),temp);
*/
static void mc34xx_set_delay(struct device *dev, int delay)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc34xx_data *mc34xx = i2c_get_clientdata(client);
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called %d\n", __func__, delay);
#endif
	atomic_set(&mc34xx->delay, delay);
#ifndef MCUBE_USE_WAIT_QUEUE
	mutex_lock(&mc34xx->enable_mutex);

	if (mc34xx_get_enable(dev)) {
		cancel_delayed_work_sync(&mc34xx->work);
		schedule_delayed_work(&mc34xx->work,
							  delay_to_jiffies(delay) + 1);
	}

	mutex_unlock(&mc34xx->enable_mutex);
#endif
}

static ssize_t mc34xx_delay_store(struct device *dev,
								  struct device_attribute *attr,
								  const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mc34xx_data *mc34xx = i2c_get_clientdata(client);
	unsigned long delay = simple_strtoul(buf, NULL, 10);
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif

	if (delay > MC34XX_MAX_DELAY)
		delay = MC34XX_MAX_DELAY;

	atomic_set(&mc34xx->delay, delay);
	return count;
}

static ssize_t mc34xx_chip_info_show(struct device *dev,
									 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s", chip_info);
}

static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO, mc34xx_enable_show,
				   mc34xx_enable_store);
static DEVICE_ATTR(delay, S_IWUSR | S_IRUGO, mc34xx_delay_show,
				   mc34xx_delay_store);
static DEVICE_ATTR(chip_info, 0440, mc34xx_chip_info_show, NULL);
static DEVICE_ATTR(value, S_IRUGO, mc34xx_value_show, NULL);
static DEVICE_ATTR(range, S_IWUSR | S_IRUGO, mc34xx_range_show, NULL);
static DEVICE_ATTR(bandwidth, S_IWUSR | S_IRUGO, mc34xx_bandwidth_show, NULL);
static DEVICE_ATTR(position, S_IWUSR | S_IRUGO, mc34xx_position_show,
				   mc34xx_position_store);
static DEVICE_ATTR(mode, S_IWUSR | S_IRUGO, mc34xx_mode_show,
				   mc34xx_mode_store);
static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO, mc34xx_register_show,
				   mc34xx_register_store);
static DEVICE_ATTR(product_code, S_IRUGO, mc34xx_product_code_show, NULL);
static DEVICE_ATTR(version, S_IRUGO, mc34xx_version_show, NULL);
static struct attribute *mc34xx_attributes[] = {
	&dev_attr_range.attr,
	&dev_attr_bandwidth.attr,
	&dev_attr_position.attr,
	&dev_attr_mode.attr,
	&dev_attr_value.attr,
	&dev_attr_delay.attr,
	&dev_attr_enable.attr,
	&dev_attr_chip_info.attr,
	&dev_attr_reg.attr,
	&dev_attr_product_code.attr,
	&dev_attr_version.attr,
	NULL
};

static struct attribute_group mc34xx_attribute_group = {
	.attrs = mc34xx_attributes
};

#ifdef MCUBE_DOT_CALIBRATION
static struct file *openFile(char *path, int flag, int mode)
{
	struct file *fp = NULL;
	fp = filp_open(path, flag, mode);

	if (IS_ERR(fp) || !fp->f_op) {
		MC_ERR_PRINT("%s: open return NULL!\n", __func__);
		return NULL;
	} else {
		return fp;
	}
}

static int readFile(struct file *fp, char *buf, int readlen)
{
	if (fp->f_op && fp->f_op->read)
		return fp->f_op->read(fp, buf, readlen, &fp->f_pos);
	else
		return -1;
}

static int writeFile(struct file *fp, char *buf, int writelen)
{
	if (fp->f_op && fp->f_op->write)
		return fp->f_op->write(fp, buf, writelen, &fp->f_pos);
	else
		return -1;
}

static int closeFile(struct file *fp)
{
	filp_close(fp, NULL);
	return 0;
}

static void initKernelEnv(void)
{
	oldfs = get_fs();
	set_fs(KERNEL_DS);
}

static int mc34xx_read_rbm_xyz(struct mc34xx_data *mc34xx,
							   struct mc34xxacc *acc, int orient)
{
	int comres = -1;
	unsigned char data[6] = { 0 };
	/*s16 tmp = 0; */
	s16 raw[3] = { 0 };
	const struct mc34xx_hwmsen_convert *pCvt = NULL;

	if ((0 == gain_data.x) || (0 == gain_data.y) || (0 == gain_data.z)) {
		acc->x = 0;
		acc->y = 0;
		acc->z = 0;
		return 0;
	}

	comres =
		mc34xx_smbus_read_block(mc34xx->client, MC34XX_REG_XOUT_EX_L, data,
								6);

	if (comres) {
		MC_ERR_PRINT("%s: i2c error!\n", __func__);
		return comres;
	}

	raw[0] = (s16)(data[0] + (data[1] << 8));
	raw[1] = (s16)(data[2] + (data[3] << 8));
	raw[2] = (s16)(data[4] + (data[5] << 8));
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called: %d, %d, %d\n", __func__, raw[0], raw[1], raw[2]);
#endif
	raw[0] = (raw[0] + offset_data.x / 2) * mc34xx_gain.x / gain_data.x;
	raw[1] = (raw[1] + offset_data.y / 2) * mc34xx_gain.y / gain_data.y;
	raw[2] = (raw[2] + offset_data.z / 2) * mc34xx_gain.z / gain_data.z;
	raw[0] = raw[0] * GRAVITY_1G_VALUE / mc34xx_gain.x;
	raw[1] = raw[1] * GRAVITY_1G_VALUE / mc34xx_gain.y;
	raw[2] = raw[2] * GRAVITY_1G_VALUE / mc34xx_gain.z;
	pCvt = &mc34xx_cvt[orient];
	acc->x = pCvt->sign[MC34XX_AXIS_X] * raw[pCvt->map[MC34XX_AXIS_X]];
	acc->y = pCvt->sign[MC34XX_AXIS_Y] * raw[pCvt->map[MC34XX_AXIS_Y]];
	acc->z = pCvt->sign[MC34XX_AXIS_Z] * raw[pCvt->map[MC34XX_AXIS_Z]];
	return comres;
}

static int mc34xx_read_true_data(struct mc34xx_data *mc34xx,
								 struct mc34xxacc *acc)
{
	int err = 0;
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called: %d\n", __func__, IsRbmMode);
#endif

	if (true == IsRbmMode) {
		err =
			mc34xx_read_rbm_xyz(mc34xx, acc, mc34xx_current_placement);
	} else {
		err =
			mc34xx_read_accel_xyz(mc34xx, acc,
								  mc34xx_current_placement);
	}

	if (err) {
		MC_ERR_PRINT("%s: read error!\n", __func__);
		return err;
	}

	return err;
}

#ifdef SUPPORT_VPROXIMITY_SENSOR
static int mc34xx_read_psensor_data(struct mc34xx_data *mc34xx, char *buf)
{
	int err = 0;
	struct mc34xxacc acc = { 0 };
	signed long psensor_data[3] = { 0 };
	int pre_enable = atomic_read(&mc34xx->enable);
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif

	if (!buf) {
		printk("%s: invalid buffer pointer!\n", __func__);
		return -EINVAL;
	}

	if (pre_enable == 0) {
		mc34xx_set_mode(mc34xx->client, MC34XX_MODE_WAKE);
		atomic_set(&mc34xx->enable, 1);
	}

	err = mc34xx_read_true_data(mc34xx, &acc);

	if (err) {
		printk("%s: read error!\n", __func__);
		return err;
	}

	psensor_data[0] = acc.x * 10;
	psensor_data[1] = acc.y * 10;
	psensor_data[2] = acc.z * 10;
	sprintf(buf, "%04x %04x %04x", psensor_data[0], psensor_data[1],
			psensor_data[2]);
	return err;
}
#endif
static int mc34xx_read_raw_data(struct mc34xx_data *mc34xx, char *buf)
{
	int err = 0;
	struct mc34xxacc acc = { 0 };
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif

	if (!buf) {
		MC_ERR_PRINT("%s: invalid buffer pointer!\n", __func__);
		return -EINVAL;
	}

	err = mc34xx_read_true_data(mc34xx, &acc);

	if (err) {
		MC_ERR_PRINT("%s: read error!\n", __func__);
		return err;
	}

	acc.z -= GRAVITY_1G_VALUE;
	sprintf(buf, "%04x %04x %04x", acc.x, acc.y, acc.z);
	return err;
}

static int mc34xx_read_rbm_data(struct mc34xx_data *mc34xx, char *buf)
{
	int err = 0;
	struct mc34xxacc acc = { 0 };
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif

	if (!buf) {
		MC_ERR_PRINT("%s: invalid buffer pointer!\n", __func__);
		return -EINVAL;
	}

	err = mc34xx_read_rbm_xyz(mc34xx, &acc, mc34xx_current_placement);

	if (err)
		MC_ERR_PRINT("%s: read error!\n", __func__);

	sprintf(buf, "%04x %04x %04x", acc.x, acc.y, acc.z);
	return err;
}

static void mc34xx_get_offset_gain(u8 *buf, SENSOR_DATA *pOffset,
								   SENSOR_DATA *pGain)
{
	s16 tmp = 0;
	u8 bMsbFilter = 0x3F;
	s16 wSignBitMask = 0x2000;
	s16 wSignPaddingBits = 0xC000;
	/* get x,y,z offset */
	tmp = ((buf[1] & bMsbFilter) << 8) + buf[0];

	if (tmp & wSignBitMask)
		tmp |= wSignPaddingBits;

	pOffset->x = tmp;
	tmp = ((buf[3] & bMsbFilter) << 8) + buf[2];

	if (tmp & wSignBitMask)
		tmp |= wSignPaddingBits;

	pOffset->y = tmp;
	tmp = ((buf[5] & bMsbFilter) << 8) + buf[4];

	if (tmp & wSignBitMask)
		tmp |= wSignPaddingBits;

	pOffset->z = tmp;
	/* get x,y,z gain */
	pGain->x = ((buf[1] >> 7) << 8) + buf[6];
	pGain->y = ((buf[3] >> 7) << 8) + buf[7];
	pGain->z = ((buf[5] >> 7) << 8) + buf[8];
}

static int mc34xx_write_calibration(struct mc34xx_data *mc34xx,
									const SENSOR_DATA *pSensorData)
{
	int err = 0;
	u8 buf[9] = { 0 };
	/*int tmp = 0; */
	int raw[3] = { 0 };
	SENSOR_DATA offset = { 0 }, gain = {
		0
	};
	struct mc34xxacc acc = { 0 };
	const struct mc34xx_hwmsen_convert *pCvt = NULL;
	u8 bMsbFilter = 0x3F;
	s32 dwRangePosLimit = 0x1FFF;
	s32 dwRangeNegLimit = -0x2000;
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called: %d, %d, %d\n", __func__, pSensorData->x,
		   pSensorData->y, pSensorData->z);
#endif
	err = mc34xx_smbus_read_block(mc34xx->client, 0x21, buf, 9);

	if (err) {
		MC_ERR_PRINT("%s: read error!\n", __func__);
		return err;
	}

	acc.x = pSensorData->x;
	acc.y = pSensorData->y;
	acc.z = pSensorData->z;
	/* get raw from dat */
	pCvt = &mc34xx_cvt[mc34xx_current_placement];
	raw[pCvt->map[MC34XX_AXIS_X]] = pCvt->sign[MC34XX_AXIS_X] * acc.x;
	raw[pCvt->map[MC34XX_AXIS_Y]] = pCvt->sign[MC34XX_AXIS_Y] * acc.y;
	raw[pCvt->map[MC34XX_AXIS_Z]] = pCvt->sign[MC34XX_AXIS_Z] * acc.z;
	raw[MC34XX_AXIS_X] =
		raw[MC34XX_AXIS_X] * mc34xx_gain.x / GRAVITY_1G_VALUE;
	raw[MC34XX_AXIS_Y] =
		raw[MC34XX_AXIS_Y] * mc34xx_gain.y / GRAVITY_1G_VALUE;
	raw[MC34XX_AXIS_Z] =
		raw[MC34XX_AXIS_Z] * mc34xx_gain.z / GRAVITY_1G_VALUE;
	MC_PRINT("mc34xx_write_calibration gain: %d, %d, %d\n", mc34xx_gain.x,
			 mc34xx_gain.y, mc34xx_gain.z);
	/* get offset and gain */
	mc34xx_get_offset_gain(buf, &offset, &gain);
	MC_PRINT("mc34xx_write_calibration og: %x, %x, %x, %x, %x, %x\n",
			 offset.x, offset.y, offset.z, gain.x, gain.y, gain.z);
	/* prepare new offset */
	offset.x =
		offset.x +
		16 * raw[MC34XX_AXIS_X] * 256 * 128 / 3 / mc34xx_gain.x / (40 +
				gain.x);
	offset.y =
		offset.y +
		16 * raw[MC34XX_AXIS_Y] * 256 * 128 / 3 / mc34xx_gain.y / (40 +
				gain.y);
	offset.z =
		offset.z +
		16 * raw[MC34XX_AXIS_Z] * 256 * 128 / 3 / mc34xx_gain.z / (40 +
				gain.z);

	/*add for over range */
	if (offset.x > dwRangePosLimit) {
		offset.x = dwRangePosLimit;
	} else if (offset.x < dwRangeNegLimit) {
		offset.x = dwRangeNegLimit;
	}

	if (offset.y > dwRangePosLimit) {
		offset.y = dwRangePosLimit;
	} else if (offset.y < dwRangeNegLimit) {
		offset.y = dwRangeNegLimit;
	}

	if (offset.z > dwRangePosLimit) {
		offset.z = dwRangePosLimit;
	} else if (offset.z < dwRangeNegLimit) {
		offset.z = dwRangeNegLimit;
	}

	/* write offset registers */
	err = mc34xx_set_mode(mc34xx->client, MC34XX_MODE_STANDBY);
	buf[0] = offset.x & 0xff;
	buf[1] = ((offset.x >> 8) & bMsbFilter) | (gain.x & 0x0100 ? 0x80 : 0);
	buf[2] = offset.y & 0xff;
	buf[3] = ((offset.y >> 8) & bMsbFilter) | (gain.y & 0x0100 ? 0x80 : 0);
	buf[4] = offset.z & 0xff;
	buf[5] = ((offset.z >> 8) & bMsbFilter) | (gain.z & 0x0100 ? 0x80 : 0);
	err += mc34xx_smbus_write_block(mc34xx->client, 0x21, buf, 6);
	err += mc34xx_set_mode(mc34xx->client, MC34XX_MODE_WAKE);

	if (err)
		MC_ERR_PRINT("%s: write error!\n", __func__);

	/* save offset and gain of DOT format for later use */
	offset_data.x = offset.x;
	offset_data.y = offset.y;
	offset_data.z = offset.z;
	gain_data.x = 256 * 8 * 128 / 3 / (40 + gain.x);
	gain_data.y = 256 * 8 * 128 / 3 / (40 + gain.y);
	gain_data.z = 256 * 8 * 128 / 3 / (40 + gain.z);
	msleep(50);
	return err;
}

static int mc34xx_reset_calibration(struct mc34xx_data *mc34xx)
{
	int err = 0;
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	err = mc34xx_set_mode(mc34xx->client, MC34XX_MODE_STANDBY);
	err += mc34xx_smbus_write_block(mc34xx->client, 0x21, offset_buf, 6);
	err += mc34xx_set_mode(mc34xx->client, MC34XX_MODE_WAKE);

	if (err)
		MC_ERR_PRINT("%s: write error!\n", __func__);

	/* save offset and gain of DOT format for later use */
	mc34xx_get_offset_gain(offset_buf, &offset_data, &gain_data);
	gain_data.x = 256 * 8 * 128 / 3 / (40 + gain_data.x);
	gain_data.y = 256 * 8 * 128 / 3 / (40 + gain_data.y);
	gain_data.z = 256 * 8 * 128 / 3 / (40 + gain_data.z);
	return err;
}

static int mc34xx_soft_reset(struct i2c_client *client)
{
	int err = 0;
	u8 tmp = 0;
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	tmp = 0x6d;
	err = mc34xx_smbus_write_byte(client, 0x1B, &tmp);
	tmp = 0x43;
	err += mc34xx_smbus_write_byte(client, 0x1B, &tmp);
	msleep(5);
	tmp = 0x43;
	err += mc34xx_smbus_write_byte(client, 0x07, &tmp);
	tmp = 0x80;
	err += mc34xx_smbus_write_byte(client, 0x1C, &tmp);
	tmp = 0x80;
	err += mc34xx_smbus_write_byte(client, 0x17, &tmp);
	msleep(5);
	tmp = 0x00;
	err += mc34xx_smbus_write_byte(client, 0x1C, &tmp);
	tmp = 0x00;
	err += mc34xx_smbus_write_byte(client, 0x17, &tmp);
	msleep(5);
	memset(offset_buf, 0, sizeof(offset_buf));
	err += mc34xx_smbus_read_block(client, 0x21, &offset_buf[0], 9);

	if (err) {
		MC_ERR_PRINT("%s: read error!\n", __func__);
		return err;
	}

	/* save offset and gain of DOT format for later use */
	mc34xx_get_offset_gain(offset_buf, &offset_data, &gain_data);
	/*printk( "mc34xx_soft_reset: %x, %x, %x, %x, %x, %x, %x, %x, %x\n", offset_buf[0], offset_buf[1], offset_buf[2], */
	/*		offset_buf[3], offset_buf[4], offset_buf[5], offset_buf[6], offset_buf[7], offset_buf[8]); */
	/*printk( "mc34xx_soft_reset1: %x, %x, %x, %x, %x, %x */
	gain_data.x = 256 * 8 * 128 / 3 / (40 + gain_data.x);
	gain_data.y = 256 * 8 * 128 / 3 / (40 + gain_data.y);
	gain_data.z = 256 * 8 * 128 / 3 / (40 + gain_data.z);
	/* initial calibration data */
	mc34xx_cali_data.x = 0;
	mc34xx_cali_data.y = 0;
	mc34xx_cali_data.z = 0;
	return err;
}

static void mc34xx_read_calibration(struct mc34xx_data *mc34xx,
									SENSOR_DATA *pSensorData)
{
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	pSensorData->x = mc34xx_cali_data.x;
	pSensorData->y = mc34xx_cali_data.y;
	pSensorData->z = mc34xx_cali_data.z;
}

static int mc34xx_rbm_mode(struct mc34xx_data *mc34xx, bool enable)
{
	int rc = 0;
	unsigned char data = 0;
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called: %d\n", __func__, enable);
#endif
	return rc;
	rc = mc34xx_set_mode(mc34xx->client, MC34XX_MODE_STANDBY);
	rc += mc34xx_smbus_read_byte(mc34xx->client, 0x04, &data);

	if (0x00 == (data & 0x40)) {
		data = 0x6D;
		rc += mc34xx_smbus_write_byte(mc34xx->client, 0x1B, &data);
		data = 0x43;
		rc += mc34xx_smbus_write_byte(mc34xx->client, 0x1B, &data);
	}

	if (true == enable) {
		data = 0;
		rc += mc34xx_smbus_write_byte(mc34xx->client, 0x3B, &data);
		data = 0x02;
		rc += mc34xx_smbus_write_byte(mc34xx->client, 0x14, &data);
		IsRbmMode = 1;
	} else {
		data = 0;
		rc += mc34xx_smbus_write_byte(mc34xx->client, 0x14, &data);
		data = s_bPCODE;
		rc += mc34xx_smbus_write_byte(mc34xx->client, 0x3B, &data);
		IsRbmMode = 0;
	}

	rc += mc34xx_smbus_read_byte(mc34xx->client, 0x04, &data);

	if (data & 0x40) {
		data = 0x6D;
		rc += mc34xx_smbus_write_byte(mc34xx->client, 0x1B, &data);
		data = 0x43;
		rc += mc34xx_smbus_write_byte(mc34xx->client, 0x1B, &data);
	}

	rc += mc34xx_set_mode(mc34xx->client, MC34XX_MODE_WAKE);
	msleep(220);
	return rc;
}

static void mc34xx_read_cali_file(struct mc34xx_data *mc34xx)
{
	SENSOR_DATA cali_data = { 0 };
	int err = 0;
	char buf[64] = { 0 };
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif

	if (load_cali_cnt > 0) {
		load_cali_cnt--;
	} else {
		return;
	}

	initKernelEnv();
	fd_file = openFile(CALIB_PATH, O_RDONLY, 0);

	if (fd_file == NULL) {
		MC_ERR_PRINT("%s: fail to open!\n", __func__);
		cali_data.x = 0;
		cali_data.y = 0;
		cali_data.z = 0;
		return;
	} else {
		memset(buf, 0, sizeof(buf));
		err = readFile(fd_file, buf, 64);
		if (err <= 0) {
			MC_ERR_PRINT("%s: read file error %d!\n", __func__,
						 err);
		} else {
			MC_PRINT("%s: %s\n", __func__, buf);
		}

		set_fs(oldfs);
		closeFile(fd_file);
		sscanf(buf, "%d %d %d", &cali_data.x, &cali_data.y,
			   &cali_data.z);
		mc34xx_write_calibration(mc34xx, &cali_data);
		load_cali_cnt = 0;
		return;
	}
}

static int mc34xx_write_log_data(const unsigned char data[64])
{
#define _WRT_LOG_DATA_BUFFER_SIZE	 (66 * 50)
	s16 rbm_data[3] = { 0 }, raw_data[3] = {
		0
	};
	int err = 0;
	char *_pszBuffer = NULL;
	int n = 0, i = 0;
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	initKernelEnv();
	fd_file = openFile(DATA_PATH, O_RDWR | O_CREAT, 0);

	if (fd_file == NULL) {
		MC_ERR_PRINT("%s: can't create file!\n", __func__);
		return -1;
	} else {
		rbm_data[0] = (s16)((data[0x0d]) | (data[0x0e] << 8));
		rbm_data[1] = (s16)((data[0x0f]) | (data[0x10] << 8));
		rbm_data[2] = (s16)((data[0x11]) | (data[0x12] << 8));
		raw_data[0] =
			(rbm_data[0] +
			 offset_data.x / 2) * mc34xx_gain.x / gain_data.x;
		raw_data[1] =
			(rbm_data[1] +
			 offset_data.y / 2) * mc34xx_gain.y / gain_data.y;
		raw_data[2] =
			(rbm_data[2] +
			 offset_data.z / 2) * mc34xx_gain.z / gain_data.z;
		_pszBuffer = kzalloc(_WRT_LOG_DATA_BUFFER_SIZE, GFP_KERNEL);

		if (NULL == _pszBuffer) {
			MC_ERR_PRINT
			("%s: fail to allocate memory for buffer!\n",
			 __func__);
			return -1;
		}

		memset(_pszBuffer, 0, _WRT_LOG_DATA_BUFFER_SIZE);
		n += sprintf(_pszBuffer + n,
					 "G-sensor RAW X = %d	 Y = %d	 Z = %d\n",
					 raw_data[0], raw_data[1], raw_data[2]);
		n += sprintf(_pszBuffer + n,
					 "G-sensor RBM X = %d	 Y = %d	 Z = %d\n",
					 rbm_data[0], rbm_data[1], rbm_data[2]);

		for (i = 0; i < 64; i++) {
			n += sprintf(_pszBuffer + n,
						 "mCube register map Register[%x] = 0x%x\n",
						 i, data[i]);
		}

		msleep(50);
		err = writeFile(fd_file, _pszBuffer, n);
		if (err <= 0) {
			MC_ERR_PRINT("%s: write file error %d!\n", __func__,
						 err);
		}

		kfree(_pszBuffer);
		set_fs(oldfs);
		closeFile(fd_file);
	}

	return 0;
}

static int mc34xx_read_reg_map(struct mc34xx_data *mc34xx)
{
	u8 data[64] = { 0 };
	int err = 0;
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	err = mc34xx_smbus_read_block(mc34xx->client, 0, data, 64);

	if (err) {
		MC_ERR_PRINT("%s: read reg fail!\n", __func__);
		return err;
	}

	msleep(50);
	mc34xx_write_log_data(data);
	msleep(50);
	return err;
}
#endif
/*static int mc34xx_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)*/
static long mc34xx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	void __user *argp = (void __user *)arg;
	struct mc34xx_data *mc34xx = file->private_data;
	int temp = 0;
#ifdef MCUBE_DOT_CALIBRATION
	char strbuf[256] = { 0 };
	SENSOR_DATA sensor_data = { 0 };
#endif
#ifdef MCUBE_FUNC_DEBUG
	printk("%s: %x\n", __func__, cmd);
#endif

	if (_IOC_DIR(cmd) & _IOC_READ)
		err =
			!access_ok(VERIFY_WRITE, (void __user *)arg,
					   _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =
			!access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) {
		MC_ERR_PRINT("%s: access isn't ok!\n", __func__);
		return -EFAULT;
	}

	if (NULL == mc34xx_client) {
		MC_ERR_PRINT("%s: i2c client isn't exist!\n", __func__);
		return -EFAULT;
	}

	switch (cmd) {
	case MC34XX_ACC_IOCTL_SET_DELAY:
		if (copy_from_user(&temp, argp, sizeof(temp))) {
			MC_ERR_PRINT("%s: set delay copy error!\n", __func__);
			return -EFAULT;
		}

		if (temp < 0 || temp > 1000) {
			MC_ERR_PRINT("%s: set delay over limit!\n", __func__);
			return -EINVAL;
		}

		mc34xx_set_delay(&(mc34xx->client->dev), temp);
		break;

	case MC34XX_ACC_IOCTL_GET_DELAY:
		temp = mc34xx_get_delay(&(mc34xx->client->dev));

		if (copy_to_user(argp, &temp, sizeof(temp))) {
			MC_ERR_PRINT("%s: get delay copy error!\n", __func__);
			return -EFAULT;
		}

		break;

	case MC34XX_ACC_IOCTL_SET_ENABLE:
		if (copy_from_user(&temp, argp, sizeof(temp))) {
			MC_ERR_PRINT("%s: set enable copy error!\n", __func__);
			return -EFAULT;
		}

		if (1 == temp)
			mc34xx_set_enable(&(mc34xx->client->dev), 1);
		else if (0 == temp)
			mc34xx_set_enable(&(mc34xx->client->dev), 0);
		else {
			MC_ERR_PRINT("%s: set enable over limit!\n", __func__);
			return -EINVAL;
		}

		break;

	case MC34XX_ACC_IOCTL_GET_ENABLE:
		temp = mc34xx_get_enable(&(mc34xx->client->dev));

		if (copy_to_user(argp, &temp, sizeof(temp))) {
			MC_ERR_PRINT("%s: get enable copy error!\n", __func__);
			return -EINVAL;
		}

		break;

	case MC34XX_ACC_IOCTL_CALIBRATION:
		MC_ERR_PRINT("%s: don't handle the command!\n", __func__);
		return -EINVAL;
#ifdef MCUBE_DOT_CALIBRATION

	case GSENSOR_IOCTL_READ_SENSORDATA:
#ifdef SUPPORT_VPROXIMITY_SENSOR
		MC_PRINT("%s: GSENSOR_IOCTL_READ_SENSORDATA\n", __func__);
		mc34xx_read_psensor_data(mc34xx, strbuf);

		if (copy_to_user(argp, strbuf, strlen(strbuf) + 1)) {
			printk("%s: read rawdata fail to copy!\n", __func__);
			return -EFAULT;
		}

		break;
#endif

	case GSENSOR_IOCTL_READ_RAW_DATA:
		MC_PRINT("%s: GSENSOR_IOCTL_READ_SENSORDATA\n", __func__);
		mc34xx_read_raw_data(mc34xx, strbuf);

		if (copy_to_user(argp, strbuf, strlen(strbuf) + 1)) {
			MC_ERR_PRINT("%s: read rawdata fail to copy!\n",
						 __func__);
			return -EFAULT;
		}

		break;

	case GSENSOR_MCUBE_IOCTL_SET_CALI:
		MC_PRINT("%s: GSENSOR_MCUBE_IOCTL_SET_CALI\n", __func__);

		if (copy_from_user(&sensor_data, argp, sizeof(sensor_data))) {
			MC_ERR_PRINT("%s: set cali fail to copy!\n", __func__);
			return -EFAULT;
		} else {
			mutex_lock(&mc34xx->enable_mutex);
			err = mc34xx_write_calibration(mc34xx, &sensor_data);
			mutex_unlock(&mc34xx->enable_mutex);
		}

		break;

	case GSENSOR_IOCTL_CLR_CALI:
		MC_PRINT("%s: GSENSOR_IOCTL_CLR_CALI\n", __func__);
		mutex_lock(&mc34xx->enable_mutex);
		err = mc34xx_reset_calibration(mc34xx);
		mutex_unlock(&mc34xx->enable_mutex);
		break;

	case GSENSOR_IOCTL_GET_CALI:
		MC_PRINT("%s: GSENSOR_IOCTL_GET_CALI\n", __func__);
		mc34xx_read_calibration(mc34xx, &sensor_data);

		if (copy_to_user(argp, &sensor_data, sizeof(sensor_data))) {
			MC_ERR_PRINT("%s: get cali fail to copy!\n", __func__);
			err = -EFAULT;
			return err;
		}

		break;

	case GSENSOR_IOCTL_SET_CALI_MODE:
		MC_PRINT("%s: GSENSOR_IOCTL_SET_CALI_MODE\n", __func__);
		break;

	case GSENSOR_MCUBE_IOCTL_READ_RBM_DATA:
		MC_PRINT("%s: GSENSOR_MCUBE_IOCTL_READ_RBM_DATA\n", __func__);
		mc34xx_read_rbm_data(mc34xx, strbuf);

		if (copy_to_user(argp, &strbuf, strlen(strbuf) + 1)) {
			MC_ERR_PRINT("%s: read rawdata fail to copy!\n",
						 __func__);
			return -EFAULT;
		}

		break;

	case GSENSOR_MCUBE_IOCTL_SET_RBM_MODE:
		MC_PRINT("%s: GSENSOR_MCUBE_IOCTL_SET_RBM_MODE\n", __func__);
		mutex_lock(&mc34xx->enable_mutex);
		err = mc34xx_rbm_mode(mc34xx, 1);
		mutex_unlock(&mc34xx->enable_mutex);
		break;

	case GSENSOR_MCUBE_IOCTL_CLEAR_RBM_MODE:
		MC_PRINT("%s: GSENSOR_MCUBE_IOCTL_CLEAR_RBM_MODE\n", __func__);
		mutex_lock(&mc34xx->enable_mutex);
		err = mc34xx_rbm_mode(mc34xx, 0);
		mutex_unlock(&mc34xx->enable_mutex);
		break;

	case GSENSOR_MCUBE_IOCTL_REGISTER_MAP:
		MC_PRINT("%s: GSENSOR_MCUBE_IOCTL_REGISTER_MAP\n", __func__);
		err = mc34xx_read_reg_map(mc34xx);
		break;

	case GSENSOR_MCUBE_IOCTL_READ_PRODUCT_ID:
		MC_PRINT("%s: GSENSOR_MCUBE_IOCTL_READ_PRODUCT_ID\n", __func__);
		temp = mc34xx_detect_pcode(mc34xx->client);

		if (temp > 0)
			temp = 0;
		else
			temp = -1;

		if (copy_to_user(argp, &temp, sizeof(temp))) {
			MC_ERR_PRINT("%s: read pcode fail to copy!\n",
						 __func__);
			return -EFAULT;
		}

		break;
#endif

	default:
		MC_ERR_PRINT("%s: can't recognize the cmd!\n", __func__);
		return 0;
	}

	return 0;
}

static ssize_t mc34xx_read(struct file *file, char __user *buf, size_t count,
						   loff_t *offset)
{
	int ret = 0;
	struct mc34xxacc acc = { 0 };
	struct mc34xx_data *mc34xx = NULL;
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif

	if (NULL == mc34xx_client) {
		MC_ERR_PRINT("%s: I2C driver not install!", __func__);
		return -1;
	}

	mc34xx = i2c_get_clientdata(mc34xx_client);
#ifdef MCUBE_DOT_CALIBRATION
	mc34xx_read_cali_file(mc34xx);
	mc34xx_read_true_data(mc34xx, &acc);
#else
	mc34xx_read_accel_xyz(mc34xx, &acc, mc34xx_current_placement);
#endif
	mutex_lock(&mc34xx->value_mutex);
	mc34xx->value = acc;
	mutex_unlock(&mc34xx->value_mutex);
	ret = copy_to_user(buf, &acc, sizeof(acc));

	if (ret) {
		MC_ERR_PRINT("%s: fail to copy_to_user: %d\n", __func__, ret);
		return 0;
	}

	return sizeof(acc);
}

static ssize_t mc34xx_write(struct file *file, const char __user *buf,
							size_t count, loff_t *offset)
{
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	return 0;
}

static int mc34xx_open(struct inode *inode, struct file *file)
{
	int err = 0;
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called open\n", __func__);
#endif
	err = nonseekable_open(inode, file);

	if (err < 0) {
		MC_ERR_PRINT("%s: open fail!\n", __func__);
		return err;
	}

	file->private_data = i2c_get_clientdata(mc34xx_client);
	return 0;
}

static int mc34xx_close(struct inode *inode, struct file *file)
{
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called close\n", __func__);
#endif
	return 0;
}

static const struct file_operations mc34xx_fops = {
	.owner = THIS_MODULE,
	.read = mc34xx_read,
	.write = mc34xx_write,
	.open = mc34xx_open,
	.release = mc34xx_close,
	.unlocked_ioctl = mc34xx_ioctl,
};

#ifdef CONFIG_ZYT_GSENSOR_COMPATIBLE
static int check_gsensor_chip(void)
{
	CDC_Gsensor_Device_Id(0x306D);
	return 0;
}

static int remove_gsensor_chip(void)
{
	CDC_Gsensor_Device_Id(0xFFFF);
	return 0;
}
#endif
static int mc34xx_remove(struct i2c_client *client)
{
	struct mc34xx_data *mc34xx = i2c_get_clientdata(client);
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	mc34xx_set_enable(&client->dev, 0);
	sysfs_remove_group(&mc34xx->input->dev.kobj, &mc34xx_attribute_group);
	mc34xx_input_deinit(mc34xx);
#ifdef CONFIG_ZYT_GSENSOR_COMPATIBLE
	remove_gsensor_chip();
#endif
	kfree(mc34xx);
	return 0;
}

static int mc34xx_i2c_remove(struct i2c_client *client)
{
	struct mc34xx_data *mc34xx = i2c_get_clientdata(client);
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	return mc34xx_remove(mc34xx->client);
}

static struct miscdevice mc34xx_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MC34XX_DEV_NAME,
	.fops = &mc34xx_fops,
};

#ifdef MCUBE_USE_WAIT_QUEUE
static int mc34xx_acc_event_handler(void *unused)
{
	struct sched_param param = {.sched_priority = 5 };
	sched_setscheduler(current, SCHED_RR, &param);

	do {
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event_interruptible(waiter, (0 != gsensor_flag));
		gsensor_flag = 0;
		set_current_state(TASK_RUNNING);
		mc34xx_work_func(NULL);
	} while (!kthread_should_stop());

	return 0;
}

static enum hrtimer_restart mc34xx_timer_expire(struct hrtimer *hrt)
{
	struct mc34xx_data *mc34xx = i2c_get_clientdata(mc34xx_client);
	hrtimer_forward_now(&mc34xx->hrtimer,
						ktime_set(atomic_read(&mc34xx->delay) / 1000,
								  (atomic_read(&mc34xx->delay) % 1000) *
								  1000000));
	/*		printk("%s %d \n",__FUNCTION__,atomic_read(&mc34xx->delay)); */
	/*		#if MCUBE_USE_WAIT_QUEUE */
	gsensor_flag = 1;
	wake_up_interruptible(&waiter);
	/*		#endif */
	return HRTIMER_RESTART;
}
#endif
#if defined(GSENSOR_INTERRUPT_MODE)
static void mc34xx_interrupt_work_func(struct work_struct *work)
{
	/*unsigned int hall_keycode; */
	/*unsigned int gpio_state; */
	static s16 temp_acc;
	struct mc34xx_data *mc34xx = NULL;
	static struct mc34xxacc acc = { 0 };
	unsigned char interrupt_state = 0, mc34xx_freq = 0;
	mc34xx = container_of(work, struct mc34xx_data, work);
	mc34xx_read_accel_xyz(mc34xx, &acc, mc34xx_current_placement);

	if (temp_acc == acc.z) {
		acc.z++;
	}

	temp_acc = acc.z;
#ifdef MCUBE_FUNC_DEBUG
	printk("%s: %d, %d, %d\n", __func__, acc.x, acc.y, acc.z);
#endif
	input_report_abs(mc34xx->input, ABS_X, -acc.y);
	input_report_abs(mc34xx->input, ABS_Y, -acc.x);
	input_report_abs(mc34xx->input, ABS_Z, acc.z);
	input_sync(mc34xx->input);
	mutex_lock(&mc34xx->value_mutex);
	mc34xx->value = acc;
	mutex_unlock(&mc34xx->value_mutex);
	enable_irq(mc34xx->irq);
	mc34xx_smbus_read_byte(mc34xx->client, MC34XX_REG_SAMPLE_RATE,
						   &mc34xx_freq);
	printk("MC34XX_SAMPR_REG================================%d\n",
		   mc34xx_freq);

	if (IS_MERAK()) {
		mc34xx_smbus_read_byte(mc34xx->client, MC34X3_REG_TILT_STATUS,
							   &interrupt_state);
	} else if (IS_MENSA()) {
		mc34xx_smbus_read_byte(mc34xx->client, MC34X6_REG_INTR_STAT_1,
							   &interrupt_state);
	}
}

static irqreturn_t mc34xx_irq_handler(int irq, void *dev_id)
{
	struct mc34xx_data *ts = (struct mc34xx_data *)dev_id;
	printk("mc34xx_irq_handler\n");
	disable_irq_nosync(ts->irq);
	queue_work(mc34xx_wq, &ts->work);
	return IRQ_HANDLED;
}
#endif
static int mc34xx_i2c_probe(struct i2c_client *client,
							const struct i2c_device_id *id)
{
	struct mc34xx_data *mc34xx = NULL;
	unsigned char product_code = 0;
	int err = 0;
#ifdef MCUBE_FUNC_DEBUG
	printk("%s caikai called  probe start\n", __func__);
#endif
#ifdef CONFIG_ZYT_GSENSOR_COMPATIBLE

	if (CDC_Gsensor_Device_Id(0) != 0) {
		printk("GSensor(0x%x)Exist!", CDC_Gsensor_Device_Id(0));
		return -ENODEV;
	}

#endif
	err = mc34xx_detect_pcode(client);

	if (err < 0) {
		MC_ERR_PRINT("%s: isn't mcube g-sensor!\n", __func__);
		return -ENODEV;
	}

#ifdef MCUBE_DOT_CALIBRATION
	mc34xx_soft_reset(client);
	err = mc34xx_detect_pcode(client);

	if (err < 0) {
		MC_ERR_PRINT("%s: can't confirm mcube g-sensor!\n", __func__);
		return -ENODEV;
	}

#else
	mc34xx_set_mode(client, MC34XX_MODE_STANDBY);
#endif
	product_code = (unsigned char)s_bPCODE;
	sprintf(chip_info, "%s", "MC34XX");
	/* setup private data */
	mc34xx = kzalloc(sizeof(struct mc34xx_data), GFP_KERNEL);

	if (!mc34xx) {
		MC_ERR_PRINT("%s: can't allocate memory for mc34xx_data!\n",
					 __func__);
		err = -ENOMEM;
		return err;
	}

	mc34xx->product_code = product_code;
	mutex_init(&mc34xx->enable_mutex);
	mutex_init(&mc34xx->value_mutex);

	/* setup i2c client */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		MC_ERR_PRINT("%s: i2c function not support!\n", __func__);
		kfree(mc34xx);
		err = -ENODEV;
		return err;
	}

	i2c_set_clientdata(client, mc34xx);
	mc34xx->client = client;
	mc34xx_client = client;
#ifdef CONFIG_ZYT_GSENSOR_COMPATIBLE

	if (check_gsensor_chip() < 0) {
		err = -ENODEV;
		printk("Fail to find mc34xx sensor.\n");
		goto exit_check_chip_failed;
	}

#endif
	/*#ifdef CONFIG_HAS_EARLYSUSPEND
	   mc34xx->early_suspend.suspend = mc34xx_early_suspend;
	   mc34xx->early_suspend.resume	 = mc34xx_early_resume;
	   mc34xx->early_suspend.level	  = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	   register_early_suspend(&mc34xx->early_suspend);
	   #endif */
	dev_info(&client->dev, "%s found\n", id->name);
	atomic_set(&mc34xx->position, mc34xx_current_placement);
	atomic_set(&mc34xx->delay, MC34XX_MAX_DELAY);
	mc34xx_init(mc34xx);
#ifdef MCUBE_USE_WAIT_QUEUE
	thread = kthread_run(mc34xx_acc_event_handler, 0, "mc34xx-thread");

	if (IS_ERR(thread)) {
		err = PTR_ERR(thread);
		printk("failed to create kernel thread:%d\n", err);
	}

	hrtimer_init(&mc34xx->hrtimer, CLOCK_BOOTTIME, HRTIMER_MODE_REL);
	mc34xx->hrtimer.function = mc34xx_timer_expire;
#else
	/* setup driver interfaces */
	INIT_DELAYED_WORK(&mc34xx->work, mc34xx_work_func);
#endif

	mc34xx->on_before_suspend = 0;

	err = mc34xx_input_init(mc34xx);

	if (err < 0) {
		MC_ERR_PRINT("%s: input init fail!\n", __func__);
		kfree(mc34xx);
		return err;
	}

#if defined(GSENSOR_INTERRUPT_MODE)
	err = gpio_request(MC34XX_INT, "MC34XX");

	if (err) {
		printk("gpio pin request fail (%d)\n", err);
		return err;
	} else {
		gpio_direction_input(MC34XX_INT);
		printk("mc34xx gpio pin request successful");
		/*get irq */
		mc34xx->irq = gpio_to_irq(MC34XX_INT);
		printk("mc34xx IRQ1 number is %d\n", mc34xx->irq);
	}

	err = request_irq(mc34xx->irq,
					  mc34xx_irq_handler,
					  IRQF_TRIGGER_HIGH, "mc34xx", mc34xx);

	if (err < 0) {
		MC_ERR_PRINT("mc34xx request irq failed: %d\n", err);
		return err;
	}

	disable_irq_nosync(mc34xx->irq);
	INIT_WORK(&mc34xx->work, mc34xx_interrupt_work_func);
	mc34xx_wq = create_singlethread_workqueue("mc34xx_wq");

	if (!mc34xx_wq) {
		printk("Creat mc34xx_wq workqueue failed.");
		err = -ENOMEM;
		return err;
	}

	enable_irq(mc34xx->irq);
	printk("Creat mc34xx_wq workqueue successful.");
#endif
	err =
		sysfs_create_group(&mc34xx->input->dev.kobj,
						   &mc34xx_attribute_group);

	if (err < 0) {
		MC_ERR_PRINT("%s: create group fail!\n", __func__);
		mc34xx_input_deinit(mc34xx);
		kfree(mc34xx);
		return err;
	}

	kobject_uevent(&mc34xx->input->dev.kobj, KOBJ_ADD);
	err = misc_register(&mc34xx_device);

	if (err) {
		MC_ERR_PRINT("%s: create register fail!\n", __func__);
		sysfs_remove_group(&mc34xx->input->dev.kobj,
						   &mc34xx_attribute_group);
		mc34xx_input_deinit(mc34xx);
		kfree(mc34xx);
		return err;
	}

	MC_ERR_PRINT("%s: successful!\n", __func__);
	return 0;
#ifdef CONFIG_ZYT_GSENSOR_COMPATIBLE
exit_check_chip_failed:
#endif
	kfree(mc34xx);
}

static int mc34xx_acc_suspend(struct i2c_client *client, pm_message_t mesg)
{
    struct mc34xx_data *mc34xx = i2c_get_clientdata(client);

    if (mc34xx != NULL) {
	mc34xx->on_before_suspend = atomic_read(&mc34xx->enable);
	printk(" -- on_before_suspend= %d-- !\n", mc34xx->on_before_suspend);
	if (mc34xx->on_before_suspend)
	    mc34xx_set_enable(&client->dev, 0);
    }
	printk("--exit--\n");
    return 0;
}

static int mc34xx_acc_resume(struct i2c_client *client)
{
    struct mc34xx_data *mc34xx = i2c_get_clientdata(client);

    if (mc34xx != NULL) {
	printk(" -- on_before_suspend= %d-- !\n", mc34xx->on_before_suspend);
	if (mc34xx->on_before_suspend)
	    mc34xx_set_enable(&client->dev, 1);
    }
    return 0;
}

static int mc34xx_pm_suspend(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    pm_message_t mesg = {0};

    mc34xx_acc_suspend(client, mesg);
    return 0;
}

static int mc34xx_pm_resume(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);

    mc34xx_acc_resume(client);
    return 0;
}

static const struct dev_pm_ops mc34xx_pm_ops = {
    .suspend = mc34xx_pm_suspend,
    .resume = mc34xx_pm_resume,
};

static const struct i2c_device_id mc34xx_id[] = {
	{MC34XX_DEV_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, mc34xx_id);
#ifdef CONFIG_OF
static struct of_device_id mc34xx_of_match_table[] = {
	{.compatible = "mCube,mc3xxx",},
	{},
};

MODULE_DEVICE_TABLE(of, mc34xx_of_match_table);
#endif
static struct i2c_driver mc34xx_driver = {
	.driver = {
		.name = MC34XX_DEV_NAME,
#ifdef CONFIG_OF
		.of_match_table = mc34xx_of_match_table,
#endif
		.owner = THIS_MODULE,
		.pm = &mc34xx_pm_ops,
	},
	.probe = mc34xx_i2c_probe,
	/*		.remove	  = __devexit_p(mc34xx_i2c_remove), */
	.remove = mc34xx_i2c_remove,
	/*.suspend		= mc34xx_i2c_suspend, */
	/*.resume		= mc34xx_i2c_resume, */
	.id_table = mc34xx_id,
};

#ifdef I2C_BUS_NUM_STATIC_ALLOC
int i2c_static_add_device(struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = NULL;
	struct i2c_client *client = NULL;
	int err = 0;
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	adapter = i2c_get_adapter(I2C_STATIC_BUS_NUM);

	if (!adapter) {
		MC_ERR_PRINT("%s: can't get i2c adapter!\n", __func__);
		err = -ENODEV;
		return err;
	}

	client = i2c_new_device(adapter, info);

	if (!client) {
		MC_ERR_PRINT("%s: can't add i2c device at 0x%x!\n", __func__,
					 (unsigned int)info->addr);
		err = -ENODEV;
		return err;
	}

	i2c_put_adapter(adapter);
	return 0;
}
#endif /* I2C_BUS_NUM_STATIC_ALLOC */
static int __init mc34xx_i2c_init(void)
{
#ifdef I2C_BUS_NUM_STATIC_ALLOC
	int ret = 0;
#endif
#ifdef MCUBE_FUNC_DEBUG
	printk("%s caikai called init\n", __func__);
#endif
#ifdef I2C_BUS_NUM_STATIC_ALLOC
	ret = i2c_static_add_device(&mc34xx_i2c_boardinfo);

	if (ret < 0) {
		MC_ERR_PRINT("%s: add i2c device error %d\n", __func__, ret);
		return ret;
	}

#endif
	return i2c_add_driver(&mc34xx_driver);
}

static void __exit mc34xx_i2c_exit(void)
{
#ifdef MCUBE_FUNC_DEBUG
	printk("%s called\n", __func__);
#endif
	i2c_del_driver(&mc34xx_driver);
#ifdef I2C_BUS_NUM_STATIC_ALLOC
	i2c_unregister_device(mc34xx_client);
#endif
}

module_init(mc34xx_i2c_init);
module_exit(mc34xx_i2c_exit);
MODULE_DESCRIPTION("mc34xx accelerometer driver");
MODULE_AUTHOR("mCube-inc");
MODULE_LICENSE("GPL");
MODULE_VERSION(MC34XX_DEV_VERSION);
