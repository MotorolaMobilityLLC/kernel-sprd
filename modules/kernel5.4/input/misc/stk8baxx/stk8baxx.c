/*
 *  stk8baxx.c - Linux kernel modules for sensortek  stk8ba50 / stk8ba50-R /
 *  stk8ba53 accelerometer
 *
 *  Copyright (C) 2012~2016 Lex Hsieh / Sensortek <lex_hsieh@sensortek.com.tw>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include <linux/pm_runtime.h>
#include <linux/fs.h>
#include <linux/module.h>
#include  <asm/uaccess.h>
#include <linux/limits.h>
#include <linux/timer.h>
#include <linux/math64.h>
#include <linux/uaccess.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#define STK_ACC_DRIVER_VERSION	"3.8.1"
#define STK_SELFTEST_VERSION		"1.1"
/*------------------User-defined settings-------------------------*/
#define CONFIG_SENSORS_STK8BA53

#define STK_ACC_POLLING_MODE	1	/*choose polling or interrupt mode */
#define STK_LOWPASS
#define STK_FIR_LEN	2	/* 1~32 */
#define STK_TUNE
#define  STK_DEBUG_CALI
#define STK_CHECK_CODE
#define STK_QUALCOMM_MANUAL_CALI

#ifdef STK_ALLWINNER_PLATFORM
#include "stk8baxx.h"
#include <mach/sys_config.h>
#include <asm/atomic.h>
#include <linux/init.h>
#include <linux/init-input.h>
#else
#include "stk8baxx.h"
#endif
#ifdef STK_INTEL_ACPI_PLATFORM
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#endif

#ifdef STK_QUALCOMM_PLATFORM
#include <linux/regulator/consumer.h>
#include <linux/sensors.h>
/* POWER SUPPLY VOLTAGE RANGE */
#define STK8BAXX_VDD_MIN_UV	2400000
#define STK8BAXX_VDD_MAX_UV	3400000
#define STK8BAXX_VIO_MIN_UV	1700000
#define STK8BAXX_VIO_MAX_UV	3400000
#endif /* #ifdef STK_QUALCOMM_PLATFORM */

#ifdef STK8BAXX_PERMISSION_THREAD
#include <linux/fcntl.h>
#include <linux/syscalls.h>
#endif

/*------------------Miscellaneous settings-------------------------*/
#if (!STK_ACC_POLLING_MODE)
#define ADDITIONAL_GPIO_CFG 1
#endif

#define STK8BAXX_I2C_NAME	"stk8baxx"
#define ACC_IDEVICE_NAME		"accelerometer"

#define STK8BAXX_INIT_ODR		0xA
#define  STK8BAXX_SPTIME_NO		3
const static int STK8BAXX_SAMPLE_TIME[STK8BAXX_SPTIME_NO] = { 32000, 16000, 8000 };
#define  STK8BAXX_SPTIME_BASE	0x9

#define STK8BAXX_RNG_2G			0x3
#define STK8BAXX_RNG_4G			0x5
#define STK8BAXX_RNG_8G			0x8
#define STK8BAXX_RNG_16G		0xC

#ifdef CONFIG_SENSORS_STK8BA53
#define STK_DEF_DYNAMIC_RANGE	STK8BAXX_RNG_2G

#if (STK_DEF_DYNAMIC_RANGE == STK8BAXX_RNG_4G)
#define STK_LSB_1G			512
#elif (STK_DEF_DYNAMIC_RANGE == STK8BAXX_RNG_2G)
#define STK_LSB_1G			1024
#elif (STK_DEF_DYNAMIC_RANGE == STK8BAXX_RNG_8G)
#define STK_LSB_1G			256
#elif (STK_DEF_DYNAMIC_RANGE == STK8BAXX_RNG_16G)
#define STK_LSB_1G			128
#endif

#define STK_ZG_COUNT		(STK_LSB_1G / 128)
#define STK_TUNE_XYOFFSET	(STK_LSB_1G * 3 / 10)
#define STK_TUNE_ZOFFSET	(STK_LSB_1G * 3 / 5)
#define STK_TUNE_NOISE		(STK_LSB_1G / 10)
#else
#define STK_DEF_DYNAMIC_RANGE	STK8BAXX_RNG_2G

#if (STK_DEF_DYNAMIC_RANGE == STK8BAXX_RNG_2G)
#define STK_LSB_1G			256
#elif (STK_DEF_DYNAMIC_RANGE == STK8BAXX_RNG_4G)
#define STK_LSB_1G			128
#elif (STK_DEF_DYNAMIC_RANGE == STK8BAXX_RNG_8G)
#define STK_LSB_1G			64
#elif (STK_DEF_DYNAMIC_RANGE == STK8BAXX_RNG_16G)
#define STK_LSB_1G			32
#endif
#define STK_ZG_COUNT		(STK_LSB_1G / 128 + 1)
#define STK_TUNE_XYOFFSET	(STK_LSB_1G * 3 / 10)
#define STK_TUNE_ZOFFSET	(STK_LSB_1G * 3 / 5)
#define STK_TUNE_NOISE		(STK_LSB_1G / 10)
#endif

#define STK_OFFSET_REG_LSB_1G	128

#define STK_TUNE_NUM			125
#define STK_TUNE_DELAY			30

#define STK_EVENT_SINCE_EN_LIMIT_DEF	(3)

#define STK8BA50_ID		0x09
#define STK8BA50R_ID		0x86
#define STK8BA53_ID		0x87

#define STK_SELFTEST_STAT_DRIVER_ERR	(-3)
#define STK_SELFTEST_STAT_RUNNING	(-2)
#define STK_SELFTEST_STAT_NA			(-1)
#define STK_SELFTEST_STAT_NO_ERROR	(0)
#define STK_SELFTEST_STAT_FAIL_X		(1)
#define STK_SELFTEST_STAT_FAIL_Y		(2)
#define STK_SELFTEST_STAT_FAIL_Z		(4)

#define STK_SELFTEST_SAMPLE_NO		(100)

/* Set +-2g mode for all chips under selftest */
#ifdef CONFIG_SENSORS_STK8BA53
#define STK_SELFTEST_LSB_1G		1024
#else
#define STK_SELFTEST_LSB_1G		256
#endif
#define STK_SELFTEST_SATU		(STK_SELFTEST_LSB_1G * 1990 / 1000)

#define STK_SELFTEST_OFFSET_X		(STK_SELFTEST_LSB_1G * 6 / 10)
#define STK_SELFTEST_OFFSET_Y		(STK_SELFTEST_LSB_1G * 6 / 10)
#define STK_SELFTEST_OFFSET_Z		(STK_SELFTEST_LSB_1G * 6 / 10)

#define STK_SELFTEST_NOISE_X			(STK_SELFTEST_LSB_1G / 10)
#define STK_SELFTEST_NOISE_Y			(STK_SELFTEST_LSB_1G / 10)
#define STK_SELFTEST_NOISE_Z			(STK_SELFTEST_LSB_1G / 10)

#define STK_QCOM_CALI_SAMPLE_NO		(10)
/*------------------Calibration prameters-------------------------*/
#define STK_SAMPLE_NO				10
#define STK_ACC_CALI_VER0			0x18
#define STK_ACC_CALI_VER1			0x03
#define STK_ACC_CALI_END				'\0'
#define STK_ACC_CALI_FILE				"/data/misc/sensors/stkacccali.conf"
#define STK_ACC_CALI_FILE_SDCARD		"/sdcard/.stkacccali.conf"
#define STK_ACC_CALI_FILE_SIZE		25

#define STK_K_SUCCESS_QCOM			0x05
#define STK_K_SUCCESS_TUNE			0x04
#define STK_K_SUCCESS_FT2			0x03
#define STK_K_SUCCESS_FT1			0x02
#define STK_K_SUCCESS_FILE			0x01
#define STK_K_NO_CALI				0xFF
#define STK_K_RUNNING				0xFE
#define STK_K_FAIL_LRG_DIFF			0xFD
#define STK_K_FAIL_OPEN_FILE			0xFC
#define STK_K_FAIL_W_FILE				0xFB
#define STK_K_FAIL_R_BACK				0xFA
#define STK_K_FAIL_R_BACK_COMP		0xF9
#define STK_K_FAIL_I2C				0xF8
#define STK_K_FAIL_K_PARA				0xF7
#define STK_K_FAIL_OUT_RG			0xF6
#define STK_K_FAIL_ENG_I2C			0xF5
#define STK_K_FAIL_FT1_USD			0xF4
#define STK_K_FAIL_FT2_USD			0xF3
#define STK_K_FAIL_WRITE_NOFST		0xF2
#define STK_K_FAIL_OTP_5T				0xF1
#define STK_K_FAIL_PLACEMENT			0xF0

/*------------------stk8baxx registers-------------------------*/
#define		STK8BAXX_XOUT1			0x02
#define		STK8BAXX_XOUT2			0x03
#define		STK8BAXX_YOUT1			0x04
#define		STK8BAXX_YOUT2			0x05
#define		STK8BAXX_ZOUT1			0x06
#define		STK8BAXX_ZOUT2			0x07
#define		STK8BAXX_INTSTS1		0x09
#define		STK8BAXX_INTSTS2		0x0A
#define		STK8BAXX_EVENTINFO1	0x0B
#define		STK8BAXX_EVENTINFO2	0x0C
#define		STK8BAXX_RANGESEL		0x0F
#define		STK8BAXX_BWSEL			0x10
#define		STK8BAXX_POWMODE		0x11
#define		STK8BAXX_DATASETUP		0x13
#define		STK8BAXX_SWRST			0x14
#define		STK8BAXX_INTEN1			0x16
#define		STK8BAXX_INTEN2			0x17
#define		STK8BAXX_INTMAP1		0x19
#define		STK8BAXX_INTMAP2		0x1A
#define		STK8BAXX_INTMAP3		0x1B
#define		STK8BAXX_DATASRC		0x1E
#define		STK8BAXX_INTCFG1		0x20
#define		STK8BAXX_INTCFG2		0x21
#define		STK8BAXX_LGDLY			0x22
#define		STK8BAXX_LGTHD			0x23
#define		STK8BAXX_HLGCFG		0x24
#define		STK8BAXX_HGDLY			0x25
#define		STK8BAXX_HGTHD			0x26
#define		STK8BAXX_SLOPEDLY		0x27
#define		STK8BAXX_SLOPETHD		0x28
#define		STK8BAXX_TAPTIME		0x2A
#define		STK8BAXX_TAPCFG		0x2B
#define		STK8BAXX_ORIENTCFG		0x2C
#define		STK8BAXX_ORIENTTHETA	0x2D
#define		STK8BAXX_FLATTHETA		0x2E
#define		STK8BAXX_FLATHOLD		0x2F
#define		STK8BAXX_SLFTST			0x32
#define		STK8BAXX_INTFCFG		0x34
#define		STK8BAXX_OFSTCOMP1	0x36
#define		STK8BAXX_OFSTCOMP2	0x37
#define		STK8BAXX_OFSTFILTX		0x38
#define		STK8BAXX_OFSTFILTY		0x39
#define		STK8BAXX_OFSTFILTZ		0x3A
#define		STK8BAXX_OFSTUNFILTX	0x3B
#define		STK8BAXX_OFSTUNFILTY	0x3C
#define		STK8BAXX_OFSTUNFILTZ	0x3D

/*	ZOUT1 register	*/
#define STK8BAXX_O_NEW			0x01

/*	SWRST register	*/
#define		STK8BAXX_SWRST_VAL		0xB6

/*	STK8BAXX_POWMODE register	*/
#define STK8BAXX_MD_SUSPEND	0x80
#define STK8BAXX_MD_NORMAL		0x00
#define STK8BAXX_MD_SLP_MASK	0x1E

/*	RANGESEL register	*/
#define STK8BAXX_RANGE_MASK	0x0F

/* OFSTCOMP1 register*/
#define STK8BAXX_OF_CAL_DRY_MASK	0x10
#define CAL_AXIS_X_EN					0x20
#define CAL_AXIS_Y_EN					0x40
#define CAL_AXIS_Z_EN					0x60
#define CAL_OFST_RST					0x80

/* OFSTCOMP2 register*/
#define CAL_TG_X0_Y0_ZPOS1		0x20
#define CAL_TG_X0_Y0_ZNEG1		0x40

#ifdef CONFIG_SPRD_PH_INFO
/*set gsensor info*/
extern char SPRD_GsensorInfo[];
static char gsensor_Info[100] = "ST Stk8baxx 3-axis Accelerometer";
#endif
/*------------------Data structure-------------------------*/
struct stk8baxx_acc {
	union {
		struct {
			s16 x;
			s16 y;
			s16 z;
		};
		s16 acc[3];
	};
};

#if defined(STK_LOWPASS)
#define MAX_FIR_LEN 32
struct data_filter {
	s16 raw[MAX_FIR_LEN][3];
	int sum[3];
	int num;
	int idx;
};
#endif

struct stk8baxx_data {
#ifdef STK_QUALCOMM_PLATFORM
	struct sensors_classdev cdev;
	struct regulator *vdd;
	struct regulator *vio;
	bool power_enabled;
#endif				/* #ifdef STK_QUALCOMM_PLATFORM */
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct mutex write_lock;
	int irq;
	int int_pin;
	struct stk8baxx_acc acc_xyz;
	atomic_t enabled;
	bool first_enable;
	struct work_struct stk_work;
	struct hrtimer acc_timer;
	ktime_t poll_delay;
	struct workqueue_struct *stk_mems_work_queue;
	unsigned char stk8baxx_placement;
	atomic_t cali_status;
	atomic_t recv_reg;
	bool re_enable;
#if defined(STK_LOWPASS)
	atomic_t firlength;
	atomic_t fir_en;
	struct data_filter fir;
#endif
	int event_since_en;
	int event_since_en_limit;
	u8 stk_tune_offset_record[3];
#ifdef STK_TUNE
	int stk_tune_offset[3];
	int stk_tune_sum[3];
	int stk_tune_max[3];
	int stk_tune_min[3];
	int stk_tune_index;
	int stk_tune_done;
	s64 stk_tune_square_sum[3];
	u32 variance[3];
#endif
	int pid;
	struct timespec old_timespec;
	struct timespec new_timespec;
	int selftest_status;
#ifdef STK_QUALCOMM_MANUAL_CALI
	char calibrate_buf[64];
#endif
};

#ifdef STK_QUALCOMM_PLATFORM
static struct sensors_classdev sensors_cdev = {
	.name = "stk8baxx-accel",
	.vendor = "Sensortek",
	.version = 1,
	.handle = SENSOR_TYPE_ACCELEROMETER,
	.type = SENSOR_TYPE_ACCELEROMETER,
#ifdef CONFIG_SENSORS_STK8BA53
#if (STK_DEF_DYNAMIC_RANGE == STK8BAXX_RNG_4G)
	.max_range = "39.24",
	.resolution = "0.019",
#elif (STK_DEF_DYNAMIC_RANGE == STK8BAXX_RNG_2G)
	.max_range = "19.62",
	.resolution = "0.01",
#elif (STK_DEF_DYNAMIC_RANGE == STK8BAXX_RNG_8G)
	.max_range = "78.48",
	.resolution = "0.038",
#elif (STK_DEF_DYNAMIC_RANGE == STK8BAXX_RNG_16G)
	.max_range = "156.96",
	.resolution = "0.077",
#endif
#else
#if (STK_DEF_DYNAMIC_RANGE == STK8BAXX_RNG_2G)
	.max_range = "19.62",
	.resolution = "0.038",
#elif (STK_DEF_DYNAMIC_RANGE == STK8BAXX_RNG_4G)
	.max_range = "39.24",
	.resolution = "0.077",
#elif (STK_DEF_DYNAMIC_RANGE == STK8BAXX_RNG_8G)
	.max_range = "78.48",
	.resolution = "0.153",
#elif (STK_DEF_DYNAMIC_RANGE == STK8BAXX_RNG_16G)
	.max_range = "156.96",
	.resolution = "0.307",
#endif
#endif
	.sensor_power = "0.138f",
	.min_delay = 8000,	/* microsecond */
	.max_delay = 32000,	/* microsecond */
	.flags = 0,		/* Should be '1' if the sensor is a wake up sensor. set it to '0' otherwise. */
	.enabled = 0,
	.delay_msec = 16,	/* millisecond */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
	.sensors_set_latency = NULL,
	.sensors_enable_wakeup = NULL,
#ifdef STK_QUALCOMM_MANUAL_CALI
	.sensors_write_cal_params = NULL,
	.sensors_calibrate = NULL,
#endif /* #ifdef STK_QUALCOMM_MANUAL_CALI */
};
#endif /* #ifdef STK_QUALCOMM_PLATFORM */

/*	direction settings	*/
static const int coordinate_trans[8][3][3] = {
	/* x_after, y_after, z_after */
	{ {1, 0, 0}, {0, 1, 0}, {0, 0, 1} },
	{ {0, -1, 0}, {1, 0, 0}, {0, 0, 1} },
	{ {-1, 0, 0}, {0, -1, 0}, {0, 0, 1} },
	{ {0, 1, 0}, {-1, 0, 0}, {0, 0, 1} },
	{ {0, 1, 0}, {1, 0, 0}, {0, 0, -1} },
	{ {-1, 0, 0}, {0, 1, 0}, {0, 0, -1} },
	{ {0, -1, 0}, {-1, 0, 0}, {0, 0, -1} },
	{ {1, 0, 0}, {0, -1, 0}, {0, 0, -1} },

};

/*------------------Function prototype-------------------------*/
static int32_t stk8baxx_get_file_content(char *r_buf, int8_t buf_size);
static int stk8baxx_get_enable(struct stk8baxx_data *stk, char *gState);
static int stk8baxx_set_enable(struct stk8baxx_data *stk, char en);
static int stk8baxx_set_offset(struct stk8baxx_data *stk, u8 offset[],
			       int no_endis);
static int stk8baxx_set_cali_scale_ofst(struct stk8baxx_data *stk, int acc[3]);
static int stk8baxx_set_delay(struct stk8baxx_data *stk, uint32_t sdelay_ns);
#ifndef STK_CALI_NO_FILE
static int stk8baxx_store_in_file(u8 offset[], u8 status, u32 variance[]);
#endif
#ifdef STK_QUALCOMM_PLATFORM
static int stk8baxx_power_on(struct stk8baxx_data *stk, bool on);
#endif /* #ifdef STK_QUALCOMM_PLATFORM */
/*------------------Global variables-------------------------*/
static struct stk8baxx_data *stk8baxx_data_ptr;

static struct stk8baxx_platform_data stk8baxx_plat_data = {
	.direction = 1,
	.interrupt_pin = 115,
};

/*------------------Main functions-------------------------*/

static int stk_i2c_rx(char *rxData, int length)
{
	uint8_t retry;
#ifdef STK_ROCKCHIP_PLATFORM
	int scl_clk_rate = 100 * 1000;
#endif
	int ret;

	struct i2c_msg msgs[] = {
		{
		 .addr = stk8baxx_data_ptr->client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = rxData,
#ifdef STK_ROCKCHIP_PLATFORM
		 .scl_rate = scl_clk_rate,
#endif
		 },
		{
		 .addr = stk8baxx_data_ptr->client->addr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = rxData,
#ifdef STK_ROCKCHIP_PLATFORM
		 .scl_rate = scl_clk_rate,
#endif
		 },
	};

	for (retry = 0; retry <= 3; retry++) {
		mutex_lock(&stk8baxx_data_ptr->write_lock);
		ret = i2c_transfer(stk8baxx_data_ptr->client->adapter, msgs, 2);
		mutex_unlock(&stk8baxx_data_ptr->write_lock);
		if (ret > 0)
			break;
		else
			mdelay(10);
	}

	if (retry > 3) {
		dev_err(&stk8baxx_data_ptr->client->dev,
			"stk8baxx i2c_transfer error, retry over 3\n");
		return -EIO;
	}
	return 0;
}

static int stk_i2c_tx(char *txData, int length)
{
	int tx_retry;
	int result;
	char buffer;
	int overall_retry;
#ifdef STK_ROCKCHIP_PLATFORM
	int scl_clk_rate = 100 * 1000;
#endif
	int ret;

	struct i2c_msg msg[] = {
		{
		 .addr = stk8baxx_data_ptr->client->addr,
		 .flags = 0,
		 .len = length,
		 .buf = txData,
#ifdef STK_ROCKCHIP_PLATFORM
		 .scl_rate = scl_clk_rate,
#endif
		 },
	};

	for (overall_retry = 0; overall_retry < 3; overall_retry++) {
		for (tx_retry = 0; tx_retry <= 3; tx_retry++) {
			mutex_lock(&stk8baxx_data_ptr->write_lock);
			ret =
			    i2c_transfer(stk8baxx_data_ptr->client->adapter,
					 msg, 1);
			mutex_unlock(&stk8baxx_data_ptr->write_lock);
			if (ret > 0)
				break;
			else
				mdelay(10);
		}

		if (tx_retry > 3) {
			dev_err(&stk8baxx_data_ptr->client->dev,
				"stk8baxx i2c_transfer error, tx_retry over 3\n");
			return -EIO;
		}

		if (txData[0] == STK8BAXX_OFSTCOMP1
		    || txData[0] == STK8BAXX_SWRST)
			return 0;

		buffer = txData[0];
		result = stk_i2c_rx(&buffer, 1);
		if (result < 0) {
			dev_err(&stk8baxx_data_ptr->client->dev,
				"stk8baxx i2c_transfer fail to read back\n");
			return result;
		}

		if (buffer == txData[1])
			return 0;
	}
	dev_err(&stk8baxx_data_ptr->client->dev,
		"stk8baxx i2c_transfer read back error,w=0x%x,r=0x%x\n",
		txData[1], buffer);
	return -EIO;
}

static s32 stk8baxx_smbus_write_byte_data(u8 command, u8 value)
{
	int result;
	char buffer[2] = "";

	buffer[0] = command;
	buffer[1] = value;
	result = stk_i2c_tx(buffer, 2);
	return result;
}

static s32 stk8baxx_smbus_read_byte_data(u8 command)
{
	int result;
	char buffer = command;
	result = stk_i2c_rx(&buffer, 1);
	return (result < 0) ? result : buffer;
}

static s32 stk8axxx_smbus_read_i2c_block_data(u8 command, u8 length,
					      u8 *values)
{
	int result;
	char buffer[16] = "";

	buffer[0] = command;
	result = stk_i2c_rx(buffer, length);
	if (result < 0)
		return result;
	memcpy(values, buffer, length);
	return length;
}

#ifdef ALLWINNER_PLATFOMR
/* Addresses to scan */
static union {
	unsigned short dirty_addr_buf[2];
	const unsigned short normal_i2c[2];
} u_i2c_addr = { {
0x00},};

static __u32 twi_id;
/**
	* gsensor_fetch_sysconfig_para - get config info from sysconfig.fex file.
	* return value:
	*                    = 0; success;
	*                    < 0; err
	*/
static int gsensor_fetch_sysconfig_para(void)
{
	int ret = -1;
	int device_used = -1;
	__u32 twi_addr = 0;
	char name[I2C_NAME_SIZE];
	script_parser_value_type_t type = SCIRPT_PARSER_VALUE_TYPE_STRING;

	printk("========%s===================\n", __func__);

	ret = script_parser_fetch("gsensor_para", "gsensor_used", &device_used, 1);

	if (SCRIPT_PARSER_OK != ret) {
		pr_err("%s: script_parser_fetch err.ret = %d. \n", __func__,
		       ret);
		goto script_parser_fetch_err;
	}
	if (1 == device_used) {
		if (SCRIPT_PARSER_OK !=
		    script_parser_fetch_ex("gsensor_para", "gsensor_name",
					   (int *)(&name), &type,
					   sizeof(name) / sizeof(int))) {
			pr_err("%s: line: %d script_parser_fetch err. \n",
			       __func__, __LINE__);
			goto script_parser_fetch_err;
		}
		if (strcmp("stk8ba", name)) {
			pr_err("%s: name %s does not match SENSOR_NAME=%s. \n",
			       __func__, name, SENSOR_NAME);
			pr_err(SENSOR_NAME);
			return ret;
		}
		if (SCRIPT_PARSER_OK !=
		    script_parser_fetch("gsensor_para", "gsensor_twi_addr",
					&twi_addr,
					sizeof(twi_addr) / sizeof(__u32))) {
			pr_err("%s: line: %d: script_parser_fetch err. \n",
			       name, __LINE__);
			goto script_parser_fetch_err;
		}
		u_i2c_addr.dirty_addr_buf[0] = twi_addr;
		u_i2c_addr.dirty_addr_buf[1] = I2C_CLIENT_END;
		printk
		    ("%s: after: gsensor_twi_addr is 0x%x, dirty_addr_buf: 0x%hx. dirty_addr_buf[1]: 0x%hx \n",
		     __func__, twi_addr, u_i2c_addr.dirty_addr_buf[0],
		     u_i2c_addr.dirty_addr_buf[1]);

		if (SCRIPT_PARSER_OK !=
		    script_parser_fetch("gsensor_para", "gsensor_twi_id",
					&twi_id, 1)) {
			pr_err("%s: script_parser_fetch err. \n", name);
			goto script_parser_fetch_err;
		}
		printk("%s: twi_id is %d. \n", __func__, twi_id);

		ret = 0;

	} else {
		pr_err("%s: gsensor_unused. \n", __func__);
		ret = -1;
	}

	return ret;

script_parser_fetch_err:
	pr_notice("=========script_parser_fetch_err============\n");
	return ret;

}

/**
	* gsensor_detect - Device detection callback for automatic device creation
	* return value:
	*                    = 0; success;
	*                    < 0; err
	*/
static int gsensor_detect(struct i2c_client *client,
			  struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	if (twi_id == adapter->nr) {
		pr_info("%s: Detected chip %s at adapter %d, address 0x%02x\n",
			__func__, SENSOR_NAME, i2c_adapter_id(adapter),
			client->addr);

		strlcpy(info->type, SENSOR_NAME, I2C_NAME_SIZE);
		return 0;
	} else {
		return -ENODEV;
	}
}
#endif

static int stk8baxx_sw_reset(struct stk8baxx_data *stk)
{
	int result;

	result =
	    stk8baxx_smbus_write_byte_data(STK8BAXX_SWRST, STK8BAXX_SWRST_VAL);
	if (result < 0)
		dev_err(&stk->client->dev,
			"%s:issue sw reset to 0x%x, result=%d\n", __func__,
			stk->client->addr, result);

	usleep_range(1000, 2000);

	return 0;
}

static int stk8baxx_check_id(struct stk8baxx_data *stk)
{
	int result;

	result = stk8baxx_smbus_read_byte_data(STK8BAXX_LGDLY);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s: failed to read acc data, error=%d\n", __func__,
			result);
		return result;
	}

	if (result == STK8BA50_ID) {
		dev_info(&stk->client->dev, "%s: chip is stk8ba50\n", __func__);
		stk->pid = STK8BA50_ID;
	} else {
		result = stk8baxx_smbus_read_byte_data(0x0);
		if (result < 0) {
			dev_err(&stk->client->dev,
				"%s: failed to read acc data, error=%d\n",
				__func__, result);
			return result;
		}

		dev_info(&stk->client->dev, "%s: 0x0=0x%x\n", __func__, result);
		if (result == STK8BA50R_ID) {
			dev_info(&stk->client->dev, "%s: chip is stk8ba50-R\n",
				 __func__);
			stk->pid = STK8BA50R_ID;
		} else {
			dev_info(&stk->client->dev, "%s: chip is stk8ba53\n",
				 __func__);
			stk->pid = STK8BA53_ID;
		}
	}

#ifdef CONFIG_SENSORS_STK8BA53
	if (stk->pid != STK8BA53_ID) {
		dev_err(&stk->client->dev,
			"%s: stk8ba53 is not attached, pid=0x%x\n", __func__,
			stk->pid);
		return -ENODEV;
	}
#else
	if (stk->pid == STK8BA53_ID) {
		dev_err(&stk->client->dev,
			"%s: stk8ba50/stk8ba50-R is not attached, pid=0x%x\n",
			__func__, stk->pid);
		return -ENODEV;
	}
#endif
	return result;
}

static void stk8baxx_parameters_boot_init(struct stk8baxx_data *stk)
{
	int aa;

	stk->selftest_status = STK_SELFTEST_STAT_NA;
	atomic_set(&stk->enabled, 0);
	atomic_set(&stk->cali_status, STK_K_NO_CALI);
	for (aa = 0; aa < 3; aa++) {
		stk->stk_tune_offset_record[aa] = 0;
	}
#ifdef STK_TUNE
	for (aa = 0; aa < 3; aa++) {
		stk->stk_tune_offset[aa] = 0;
		stk->stk_tune_sum[aa] = 0;
		stk->stk_tune_max[aa] = 0;
		stk->stk_tune_min[aa] = 0;
		stk->stk_tune_square_sum[aa] = 0LL;
		stk->variance[aa] = 0;
	}
	stk->stk_tune_done = 0;
	stk->stk_tune_index = 0;
#endif
}

static int stk8baxx_reg_init(struct stk8baxx_data *stk,
			     struct i2c_client *client)
{
	int result;

#ifdef CONFIG_SENSORS_STK8BA53
	dev_info(&stk->client->dev, "%s: Initialize stk8ba53\n", __func__);
#else
	dev_info(&stk->client->dev, "%s: Initialize stk8ba50/stk8ba50-r\n",
		 __func__);
#endif

	/*      sw reset        */
	result = stk8baxx_sw_reset(stk);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to stk8baxx_sw_reset, error=%d\n", __func__,
			result);
		return result;
	}

	result =
	    stk8baxx_smbus_write_byte_data(STK8BAXX_POWMODE,
					   STK8BAXX_MD_NORMAL);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_POWMODE, result);
		return result;
	}

	result = stk8baxx_check_id(stk);
	if (result < 0) {
		dev_err(&stk->client->dev, "%s:failed to check id, error=%d\n",
			__func__, result);
		return result;
	}
#if (!STK_ACC_POLLING_MODE)
	/* map new data int to int1     */
	result = stk8baxx_smbus_write_byte_data(STK8BAXX_INTMAP2, 0x01);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_INTMAP2, result);
		return result;
	}
	/*      enable new data int     */
	result = stk8baxx_smbus_write_byte_data(STK8BAXX_INTEN2, 0x10);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_INTEN2, result);
		return result;
	}
	/*      non-latch int   */
	result = stk8baxx_smbus_write_byte_data(STK8BAXX_INTCFG2, 0x00);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_INTCFG2, result);
		return result;
	}
	/*      filtered data source for new data int   */
	result = stk8baxx_smbus_write_byte_data(STK8BAXX_DATASRC, 0x00);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_DATASRC, result);
		return result;
	}
	/*      int1, push-pull, active high    */
	result = stk8baxx_smbus_write_byte_data(STK8BAXX_INTCFG1, 0x01);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_INTCFG1, result);
		return result;
	}
#endif

	/*      According to STK_DEF_DYNAMIC_RANGE      */
	result =
	    stk8baxx_smbus_write_byte_data(STK8BAXX_RANGESEL,
					   STK_DEF_DYNAMIC_RANGE);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_RANGESEL, result);
		return result;
	}

	/*      ODR = 62 Hz     */
	result =
	    stk8baxx_smbus_write_byte_data(STK8BAXX_BWSEL, STK8BAXX_INIT_ODR);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_BWSEL, result);
		return result;
	}

	/*      i2c watchdog enable, 1 ms timer perios  */
	result = stk8baxx_smbus_write_byte_data(STK8BAXX_INTFCFG, 0x04);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_INTFCFG, result);
		return result;
	}

	result =
	    stk8baxx_smbus_write_byte_data(STK8BAXX_POWMODE,
					   STK8BAXX_MD_SUSPEND);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_POWMODE, result);
		return result;
	}

	atomic_set(&stk->recv_reg, 0);
#ifdef STK_LOWPASS
	memset(&stk->fir, 0x00, sizeof(stk->fir));
	atomic_set(&stk->firlength, STK_FIR_LEN);
	atomic_set(&stk->fir_en, 1);
#endif

	stk->event_since_en_limit = STK_EVENT_SINCE_EN_LIMIT_DEF;
	stk->first_enable = true;

	return 0;
}

#ifdef STK_LOWPASS
static void stk8baxx_low_pass(struct stk8baxx_data *stk,
			      struct stk8baxx_acc *acc_lp)
{
	int idx, firlength = atomic_read(&stk->firlength);
#ifdef STK_ZG_FILTER
	s16 zero_fir = 0;
#endif

	if (atomic_read(&stk->fir_en)) {
		if (stk->fir.num < firlength) {
			stk->fir.raw[stk->fir.num][0] = acc_lp->x;
			stk->fir.raw[stk->fir.num][1] = acc_lp->y;
			stk->fir.raw[stk->fir.num][2] = acc_lp->z;
			stk->fir.sum[0] += acc_lp->x;
			stk->fir.sum[1] += acc_lp->y;
			stk->fir.sum[2] += acc_lp->z;
			stk->fir.num++;
			stk->fir.idx++;
		} else {
			idx = stk->fir.idx % firlength;
			stk->fir.sum[0] -= stk->fir.raw[idx][0];
			stk->fir.sum[1] -= stk->fir.raw[idx][1];
			stk->fir.sum[2] -= stk->fir.raw[idx][2];
			stk->fir.raw[idx][0] = acc_lp->x;
			stk->fir.raw[idx][1] = acc_lp->y;
			stk->fir.raw[idx][2] = acc_lp->z;
			stk->fir.sum[0] += acc_lp->x;
			stk->fir.sum[1] += acc_lp->y;
			stk->fir.sum[2] += acc_lp->z;
			stk->fir.idx++;
#ifdef STK_ZG_FILTER
			if (abs(stk->fir.sum[0] / firlength) <= STK_ZG_COUNT) {
				acc_lp->x =
				    (stk->fir.sum[0] * zero_fir) / firlength;
			} else {
				acc_lp->x = stk->fir.sum[0] / firlength;
			}
			if (abs(stk->fir.sum[1] / firlength) <= STK_ZG_COUNT) {
				acc_lp->y =
				    (stk->fir.sum[1] * zero_fir) / firlength;
			} else {
				acc_lp->y = stk->fir.sum[1] / firlength;
			}
			if (abs(stk->fir.sum[2] / firlength) <= STK_ZG_COUNT) {
				acc_lp->z =
				    (stk->fir.sum[2] * zero_fir) / firlength;
			} else {
				acc_lp->z = stk->fir.sum[2] / firlength;
			}
#else
			acc_lp->x = stk->fir.sum[0] / firlength;
			acc_lp->y = stk->fir.sum[1] / firlength;
			acc_lp->z = stk->fir.sum[2] / firlength;
#endif
#ifdef STK_DEBUG_PRINT
			/*
			   dev_info(&stk->client->dev,"add [%2d] [%5d %5d %5d] => [%5d %5d %5d] : [%5d %5d %5d]\n", idx,
			   stk->fir.raw[idx][0], stk->fir.raw[idx][1], stk->fir.raw[idx][2],
			   stk->fir.sum[0], stk->fir.sum[1], stk->fir.sum[2],
			   acc_lp->x, acc_lp->y, acc_lp->z);
			 */
#endif
		}
	}
}
#endif

#ifdef STK_TUNE
static void stk8baxx_reset_para(struct stk8baxx_data *stk)
{
	int ii;
	for (ii = 0; ii < 3; ii++) {
		stk->stk_tune_sum[ii] = 0;
		stk->stk_tune_square_sum[ii] = 0LL;
		stk->stk_tune_min[ii] = 4096;
		stk->stk_tune_max[ii] = -4096;
		stk->variance[ii] = 0;
	}
	return;
}

static void stk8baxx_tune(struct stk8baxx_data *stk,
			  struct stk8baxx_acc *acc_xyz)
{
	int ii;
	u8 offset[3];
	s16 acc[3];
	s64 s64_temp;
	const s64 var_enlarge_scale = 64;

	if (stk->stk_tune_done != 0)
		return;

	acc[0] = acc_xyz->x;
	acc[1] = acc_xyz->y;
	acc[2] = acc_xyz->z;

	if (stk->event_since_en >= STK_TUNE_DELAY) {
		if ((abs(acc[0]) <= STK_TUNE_XYOFFSET)
		    && (abs(acc[1]) <= STK_TUNE_XYOFFSET)
		    && (abs(abs(acc[2]) - STK_LSB_1G) <= STK_TUNE_ZOFFSET))
			stk->stk_tune_index++;
		else
			stk->stk_tune_index = 0;

		if (stk->stk_tune_index == 0) {
			stk8baxx_reset_para(stk);
		} else {
			for (ii = 0; ii < 3; ii++) {
				stk->stk_tune_sum[ii] += acc[ii];
				stk->stk_tune_square_sum[ii] +=
				    acc[ii] * acc[ii];
				if (acc[ii] > stk->stk_tune_max[ii])
					stk->stk_tune_max[ii] = acc[ii];
				if (acc[ii] < stk->stk_tune_min[ii])
					stk->stk_tune_min[ii] = acc[ii];
			}
		}

		if (stk->stk_tune_index == STK_TUNE_NUM) {
			for (ii = 0; ii < 3; ii++) {
				if ((stk->stk_tune_max[ii] -
				     stk->stk_tune_min[ii]) > STK_TUNE_NOISE) {
					stk->stk_tune_index = 0;
					stk8baxx_reset_para(stk);
					return;
				}
			}

			for (ii = 0; ii < 3; ii++) {
				if (stk->stk_tune_sum[ii] >= 0) {
					stk->stk_tune_offset[ii] =
					    (int)(div64_long
						  (stk->stk_tune_sum[ii] +
						   STK_TUNE_NUM / 2,
						   STK_TUNE_NUM));
				} else {
					stk->stk_tune_offset[ii] =
					    (int)(div64_long
						  (stk->stk_tune_sum[ii] -
						   STK_TUNE_NUM / 2,
						   STK_TUNE_NUM));
				}
			}

			if (acc[2] > 0)
				stk->stk_tune_offset[2] =
				    stk->stk_tune_offset[2] - STK_LSB_1G;
			else
				stk->stk_tune_offset[2] =
				    stk->stk_tune_offset[2] - (-STK_LSB_1G);
			stk8baxx_set_cali_scale_ofst(stk, stk->stk_tune_offset);

			for (ii = 0; ii < 3; ii++) {
				offset[ii] = (u8) (-stk->stk_tune_offset[ii]);
				stk->stk_tune_offset_record[ii] = offset[ii];
				stk->stk_tune_square_sum[ii] *=
				    var_enlarge_scale * var_enlarge_scale;
				s64_temp =
				    stk->stk_tune_sum[ii] * var_enlarge_scale;
				stk->stk_tune_square_sum[ii] =
				    div64_long(stk->stk_tune_square_sum[ii],
					       STK_TUNE_NUM - 1);
				s64_temp = s64_temp * s64_temp;
				s64_temp = div64_long(s64_temp, STK_TUNE_NUM);
				s64_temp =
				    div64_long(s64_temp, STK_TUNE_NUM - 1);
				stk->variance[ii] =
				    (u32) (stk->stk_tune_square_sum[ii] -
					   s64_temp);
			}
			stk8baxx_set_offset(stk, offset, 0);
#ifndef STK_CALI_NO_FILE
			stk8baxx_store_in_file(offset, STK_K_SUCCESS_TUNE,
					       stk->variance);
#endif
			stk->stk_tune_done = 1;
			atomic_set(&stk->cali_status, STK_K_SUCCESS_TUNE);
			stk->event_since_en = 0;
			dev_info(&stk->client->dev, "%s:TUNE done, %d,%d,%d\n",
				 __func__, offset[0], offset[1], offset[2]);
			dev_info(&stk->client->dev,
				 "%s:TUNE done, var=%u,%u,%u\n", __func__,
				 stk->variance[0], stk->variance[1],
				 stk->variance[2]);
		}
	}

	return;
}
#endif

#define CHECK_CODE_SIZE  (STK_LSB_1G + 1)
static int stkcheckcode[CHECK_CODE_SIZE][CHECK_CODE_SIZE];

static int stk8baxx_get_code(int a0, int a1)
{
	int a = 1, i = 0, j = 0, b = 0, d = 0, n = 0, dd = 0;
	int c = STK_LSB_1G * STK_LSB_1G - a0 * a0 - a1 * a1;
	if (c < 0)
		return 0;
	a = a + c;
	if (a != i) {
		if (a < i)
			a = i - a;
		for (i = 0; i * i < a; i++)
			j = (i + 1) * 10;
		i = (i - 1) * 10;
		d = 100;
		a = a * d;
		while (d > 10) {
			b = (i + j) >> 1;
			if (b * b > a)
				j = b;
			else
				i = b;
			d = a - i * i;
			if (dd == d)
				n++;
			else
				n = 0;
			if (n >= 3)
				break;
			dd = d;
		}
		if ((i % 10) >= 5)
			a = (i / 10) + 1;
		else
			a = (i / 10);
	}
	return a;
}

static void stk8baxx_check_init(void)
{
	int i, j;

	for (i = 0; i < CHECK_CODE_SIZE; i++)
		for (j = 0; j < CHECK_CODE_SIZE; j++)
			stkcheckcode[i][j] = stk8baxx_get_code(i, j);
}

static int stk8baxx_check_code(s16 acc[])
{
	int a, b;
	if (acc[0] > 0)
		a = acc[0];
	else
		a = -acc[0];
	if (acc[1] > 0)
		b = acc[1];
	else
		b = -acc[1];
	if (a >= CHECK_CODE_SIZE || b >= CHECK_CODE_SIZE)
		acc[2] = 0;
	else
		acc[2] = (s16) stkcheckcode[a][b];
	return 0;
}

static int stk8baxx_check_reading(struct stk8baxx_data *stk, s16 acc[],
				  bool clear)
{
	static int check_result;
	static int event_no;
#ifdef CONFIG_SENSORS_STK8BA53
	const int max_value = 2047;
	const int min_value = -2048;
#else
	const int max_value = 511;
	const int min_value = -512;
#endif

	if (event_no > 20)
		return 0;

	if (acc[0] == max_value || acc[0] == min_value || acc[1] == max_value
	    || acc[1] == min_value || acc[2] == max_value
	    || acc[2] == min_value) {
		dev_info(&stk->client->dev, "%s: acc:%o,%o,%o\n", __func__,
			 acc[0], acc[1], acc[2]);
		check_result++;
	}

	if (clear) {
		if (check_result >= 3) {
			if (acc[0] != max_value && acc[0] != min_value
			    && acc[1] != max_value && acc[1] != min_value)
				stk->event_since_en_limit =
				    (STK_EVENT_SINCE_EN_LIMIT_DEF + 6);
			else
				stk->event_since_en_limit = 10000;
			dev_info(&stk->client->dev, "%s: incorrect reading\n",
				 __func__);
			return 1;
		}
		check_result = 0;
	}
	event_no++;
	return 0;
}

static void stk8baxx_sign_conv(struct stk8baxx_data *stk, s16 raw_acc_data[],
			       u8 acc_reg_data[])
{
#ifdef CONFIG_SENSORS_STK8BA53
	raw_acc_data[0] = acc_reg_data[1] << 8 | acc_reg_data[0];
	raw_acc_data[0] >>= 4;
	raw_acc_data[1] = acc_reg_data[3] << 8 | acc_reg_data[2];
	raw_acc_data[1] >>= 4;
	raw_acc_data[2] = acc_reg_data[5] << 8 | acc_reg_data[4];
	raw_acc_data[2] >>= 4;
#else
	raw_acc_data[0] = acc_reg_data[1] << 8 | acc_reg_data[0];
	raw_acc_data[0] >>= 6;
	raw_acc_data[1] = acc_reg_data[3] << 8 | acc_reg_data[2];
	raw_acc_data[1] >>= 6;
	raw_acc_data[2] = acc_reg_data[5] << 8 | acc_reg_data[4];
	raw_acc_data[2] >>= 6;
#endif
}

static int stk8baxx_read_sensor_data(struct stk8baxx_data *stk)
{
	int result;
	u8 acc_reg[6];
	int ii;
	struct stk8baxx_acc acc;
	int placement_no = (int)stk->stk8baxx_placement;
	s16 raw_acc[3];
	int k_status = atomic_read(&stk->cali_status);

	stk->new_timespec = ktime_to_timespec(ktime_get_boottime());

	acc.x = acc.y = acc.z = 0;
	result = stk8axxx_smbus_read_i2c_block_data(STK8BAXX_XOUT1, 6, acc_reg);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s: failed to read acc data, error=%d\n", __func__,
			result);
		return result;
	}

	stk8baxx_sign_conv(stk, raw_acc, acc_reg);
	if (stk->event_since_en == (STK_EVENT_SINCE_EN_LIMIT_DEF + 1)
	    || stk->event_since_en == (STK_EVENT_SINCE_EN_LIMIT_DEF + 2))
		stk8baxx_check_reading(stk, raw_acc, false);
	else if (stk->event_since_en == (STK_EVENT_SINCE_EN_LIMIT_DEF + 3))
		stk8baxx_check_reading(stk, raw_acc, true);
	else if (stk->event_since_en_limit ==
		 (STK_EVENT_SINCE_EN_LIMIT_DEF + 6))
		stk8baxx_check_code(raw_acc);

#ifdef STK_DEBUG_PRINT

#endif

	if (k_status == STK_K_RUNNING
	    || stk->selftest_status == STK_SELFTEST_STAT_RUNNING) {

		stk->acc_xyz.x = raw_acc[0];
		stk->acc_xyz.y = raw_acc[1];
		stk->acc_xyz.z = raw_acc[2];

		return 0;
	}

	acc.x = raw_acc[0];
	acc.y = raw_acc[1];
	acc.z = raw_acc[2];

#ifdef STK_TUNE
	if ((k_status & 0xF0) != 0)
		stk8baxx_tune(stk, &acc);
#endif

	acc.x = 0;
	acc.y = 0;
	acc.z = 0;
	for (ii = 0; ii < 3; ii++) {
		acc.x += raw_acc[ii] * coordinate_trans[placement_no][0][ii];
		acc.y += raw_acc[ii] * coordinate_trans[placement_no][1][ii];
		acc.z += raw_acc[ii] * coordinate_trans[placement_no][2][ii];
	}

#ifdef STK_LOWPASS
	stk8baxx_low_pass(stk, &acc);
#endif

	stk->acc_xyz.x = acc.x;
	stk->acc_xyz.y = acc.y;
	stk->acc_xyz.z = acc.z;
#ifdef STK_DEBUG_PRINT
#endif
	return 0;
}

static int stk8baxx_report_value(struct stk8baxx_data *stk)
{
	u64 old_time_ns =
	    stk->old_timespec.tv_sec * 1000000000ULL +
	    stk->old_timespec.tv_nsec;
	u64 new_time_ns =
	    stk->new_timespec.tv_sec * 1000000000ULL +
	    stk->new_timespec.tv_nsec;
	struct timespec mid_timespec;

	if (stk->event_since_en < 1200)
		stk->event_since_en++;

	if (stk->event_since_en < stk->event_since_en_limit)
		return 0;

	if (((new_time_ns - old_time_ns) >
	     (ktime_to_ms(stk->poll_delay) * 1800000LL))
	    && (old_time_ns != 0)) {
		mid_timespec = ns_to_timespec((new_time_ns + old_time_ns) >> 1);
#if 1
		input_report_abs(stk->input_dev, ABS_X, (stk->acc_xyz.y)/4);
		input_report_abs(stk->input_dev, ABS_Y, -(stk->acc_xyz.x)/4);
		input_report_abs(stk->input_dev, ABS_Z, (stk->acc_xyz.z)/4);
#else
		input_report_rel(stk->input_dev, REL_X, stk->acc_xyz.x);
		input_report_rel(stk->input_dev, REL_Y, stk->acc_xyz.y);
		input_report_rel(stk->input_dev, REL_Z, stk->acc_xyz.z);
		input_report_rel(stk->input_dev, REL_DIAL, mid_timespec.tv_sec);
		input_report_rel(stk->input_dev, REL_MISC, mid_timespec.tv_nsec);
#endif /* #ifdef STK_QUALCOMM_PLATFORM */
		input_sync(stk->input_dev);
	}
#if 1
	input_report_abs(stk->input_dev, ABS_X, (stk->acc_xyz.y)/4);
	input_report_abs(stk->input_dev, ABS_Y, -(stk->acc_xyz.x)/4);
	input_report_abs(stk->input_dev, ABS_Z, (stk->acc_xyz.z)/4);
#else
	input_report_rel(stk->input_dev, REL_X, stk->acc_xyz.x);
	input_report_rel(stk->input_dev, REL_Y, stk->acc_xyz.y);
	input_report_rel(stk->input_dev, REL_Z, stk->acc_xyz.z);
	input_report_rel(stk->input_dev, REL_DIAL, stk->new_timespec.tv_sec);
	input_report_rel(stk->input_dev, REL_MISC, stk->new_timespec.tv_nsec);
#endif /* #ifdef STK_QUALCOMM_PLATFORM */
	input_sync(stk->input_dev);
	stk->old_timespec.tv_sec = stk->new_timespec.tv_sec;
	stk->old_timespec.tv_nsec = stk->new_timespec.tv_nsec;

	return 0;
}

static int stk8baxx_enter_active(struct stk8baxx_data *stk)
{
	int result;
	result =
	    stk8baxx_smbus_write_byte_data(STK8BAXX_POWMODE,
					   STK8BAXX_MD_NORMAL);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_POWMODE, result);
		return result;
	}
	return 0;
}

static int stk8baxx_enter_suspend(struct stk8baxx_data *stk)
{
	int result;
	result =
	    stk8baxx_smbus_write_byte_data(STK8BAXX_POWMODE,
					   STK8BAXX_MD_SUSPEND);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_POWMODE, result);
		return result;
	}
	return 0;
}

#ifdef STK_HOLD_ODR
static int stk8baxx_set_delay(struct stk8baxx_data *stk, uint32_t sdelay_ns)
{
	dev_info(&stk->client->dev, "%s: ODR is fixed\n", __func__);
	return 0;
}
#else
static int stk8baxx_set_delay(struct stk8baxx_data *stk, uint32_t sdelay_ns)
{
	unsigned char sr_no;
	int result;
	uint32_t sdelay_us = sdelay_ns / 1000;
	char en;

	for (sr_no = 0; sr_no < STK8BAXX_SPTIME_NO; sr_no++) {
		if (sdelay_us >= STK8BAXX_SAMPLE_TIME[sr_no])
			break;
	}

	if (sr_no == STK8BAXX_SPTIME_NO) {
		sdelay_ns = STK8BAXX_SAMPLE_TIME[STK8BAXX_SPTIME_NO - 1] * 1000;
		sr_no--;
	}
	sr_no += STK8BAXX_SPTIME_BASE;
#ifdef STK_DEBUG_PRINT
	dev_info(&stk->client->dev, "%s:sdelay_us=%u, sr_no=0x%x\n", __func__,
		 sdelay_ns / 1000, sr_no);
#endif

	result = stk8baxx_get_enable(stk, &en);
	if (result < 0)
		dev_err(&stk->client->dev,
			"%s: stk8baxx_get_enable failed, error=%d\n", __func__,
			result);

	if (en == 0)
		stk8baxx_enter_active(stk);

	result = stk8baxx_smbus_write_byte_data(STK8BAXX_BWSEL, sr_no);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_BWSEL, result);
		return result;
	}
	if (en == 0)
		stk8baxx_enter_suspend(stk);

	stk->poll_delay = ns_to_ktime(sdelay_ns);

#if defined(STK_LOWPASS)
	stk->fir.num = 0;
	stk->fir.idx = 0;
	stk->fir.sum[0] = 0;
	stk->fir.sum[1] = 0;
	stk->fir.sum[2] = 0;
#endif

	return 0;
}
#endif

static s64 stk8baxx_get_delay(struct stk8baxx_data *stk)
{
	int result, sample_time;
	s64 delay_us;
	char en;

	result = stk8baxx_get_enable(stk, &en);
	if (result < 0)
		dev_err(&stk->client->dev,
			"%s: stk8baxx_get_enable failed, error=%d\n", __func__,
			result);

	if (en == 0)
		stk8baxx_enter_active(stk);
	sample_time = stk8baxx_smbus_read_byte_data(STK8BAXX_BWSEL);
	if (sample_time < 0) {
		dev_err(&stk->client->dev,
			"%s: failed to read reg 0x%x, error=%d\n", __func__,
			STK8BAXX_BWSEL, sample_time);
		return sample_time;
	}
	if (en == 0)
		stk8baxx_enter_suspend(stk);
	delay_us = STK8BAXX_SAMPLE_TIME[sample_time - STK8BAXX_SPTIME_BASE];
	dev_info(&stk->client->dev, "%s: delay =%lld us\n", __func__, delay_us);
	return (delay_us * NSEC_PER_USEC);
}

static int stk8baxx_set_offset(struct stk8baxx_data *stk, u8 offset[],
			       int no_endis)
{
	int result;
	char en = 1;

	if (!no_endis) {
		result = stk8baxx_get_enable(stk, &en);
		if (result < 0) {
			dev_err(&stk->client->dev,
				"%s: stk8baxx_get_enable failed, error=%d\n",
				__func__, result);
		}

		if (!en)
			stk8baxx_enter_active(stk);
	}

	result = stk8baxx_smbus_write_byte_data(STK8BAXX_OFSTFILTX, offset[0]);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_OFSTFILTX, result);
		return result;
	}
	result = stk8baxx_smbus_write_byte_data(STK8BAXX_OFSTFILTY, offset[1]);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_OFSTFILTX, result);
		return result;
	}
	result = stk8baxx_smbus_write_byte_data(STK8BAXX_OFSTFILTZ, offset[2]);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_OFSTFILTX, result);
		return result;
	}

	if (!en && !no_endis)
		stk8baxx_enter_suspend(stk);
	return 0;
}

static int stk8baxx_get_offset(struct stk8baxx_data *stk, u8 offset[])
{
	int result;
	char en;

	result = stk8baxx_get_enable(stk, &en);
	if (result < 0)
		dev_err(&stk->client->dev,
			"%s: stk8baxx_get_enable failed, error=%d\n", __func__,
			result);
	if (en == 0)
		stk8baxx_enter_active(stk);

	result =
	    stk8axxx_smbus_read_i2c_block_data(STK8BAXX_OFSTFILTX, 3, offset);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_OFSTFILTX, result);
		return result;
	}

	if (en == 0)
		stk8baxx_enter_suspend(stk);
	return 0;
}

static int stk8baxx_set_range(struct stk8baxx_data *stk, char srange)
{
	int result;
	s8 write_buffer = 0;
	char en;

	if (srange > STK8BAXX_RANGE_MASK) {
		dev_err(&stk->client->dev, "%s:range=0x%x, out of range\n",
			__func__, srange);
		return -EINVAL;
	}
#ifdef STK_DEBUG_PRINT
	dev_info(&stk->client->dev, "%s:range=0x%x\n", __func__, srange);
#endif
	switch (srange) {
	case STK8BAXX_RNG_2G:
		write_buffer = STK8BAXX_RNG_2G;
		break;
	case STK8BAXX_RNG_4G:
		write_buffer = STK8BAXX_RNG_4G;
		break;
	case STK8BAXX_RNG_8G:
		write_buffer = STK8BAXX_RNG_8G;
		break;
	case STK8BAXX_RNG_16G:
		write_buffer = STK8BAXX_RNG_16G;
		break;
	default:
		write_buffer = STK8BAXX_RNG_2G;
		dev_err(&stk->client->dev,
			"%s: unknown range, set as STK8BAXX_RNG_2G\n",
			__func__);
	}

	result = stk8baxx_get_enable(stk, &en);
	if (result < 0)
		dev_err(&stk->client->dev,
			"%s: stk8baxx_get_enable failed, error=%d\n", __func__,
			result);

	if (en == 0)
		stk8baxx_enter_active(stk);
	result =
	    stk8baxx_smbus_write_byte_data(STK8BAXX_RANGESEL, write_buffer);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_RANGESEL, result);
		return result;
	}
	if (en == 0)
		stk8baxx_enter_suspend(stk);

	return 0;
}

static int stk8baxx_get_range(struct stk8baxx_data *stk, char *grange)
{
	int result;
	char en;

	result = stk8baxx_get_enable(stk, &en);
	if (result < 0)
		dev_err(&stk->client->dev,
			"%s: stk8baxx_get_enable failed, error=%d\n", __func__,
			result);

	if (en == 0)
		stk8baxx_enter_active(stk);

	result = stk8baxx_smbus_read_byte_data(STK8BAXX_RANGESEL);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s: failed to read reg 0x%x, error=%d\n", __func__,
			STK8BAXX_RANGESEL, result);
		return result;
	}

	if (en == 0)
		stk8baxx_enter_suspend(stk);

	switch (result) {
	case STK8BAXX_RNG_2G:
		*grange = 2;
		break;
	case STK8BAXX_RNG_4G:
		*grange = 4;
		break;
	case STK8BAXX_RNG_8G:
		*grange = 8;
		break;
	case STK8BAXX_RNG_16G:
		*grange = 16;
		break;
	}

	return 0;
}

static int stk8baxx_get_cali(struct stk8baxx_data *stk)
{
	char stk_file[STK_ACC_CALI_FILE_SIZE];
	int aa;

	dev_info(&stk->client->dev,
		 "%s: stk->stk_tune_done=%d, stk->stk_tune_index=%d, stk->stk_tune_offset=%d,%d,%d\n",
		 __func__, stk->stk_tune_done, stk->stk_tune_index,
		 stk->stk_tune_offset_record[0], stk->stk_tune_offset_record[1],
		 stk->stk_tune_offset_record[2]);

	if ((stk8baxx_get_file_content(stk_file, STK_ACC_CALI_FILE_SIZE)) == 0) {
		if (stk_file[0] == STK_ACC_CALI_VER0
		    && stk_file[1] == STK_ACC_CALI_VER1
		    && stk_file[STK_ACC_CALI_FILE_SIZE - 1] ==
		    STK_ACC_CALI_END) {
			atomic_set(&stk->cali_status, (int)stk_file[8]);
			dev_info(&stk->client->dev,
				 "%s: offset:%d,%d,%d, mode=0x%x\n", __func__,
				 stk_file[3], stk_file[5], stk_file[7],
				 stk_file[8]);
			dev_info(&stk->client->dev, "%s: variance=%u,%u,%u\n",
				 __func__,
				 (stk_file[9] << 24 | stk_file[10] << 16 |
				  stk_file[11] << 8 | stk_file[12]),
				 (stk_file[13] << 24 | stk_file[14] << 16 |
				  stk_file[15] << 8 | stk_file[16]),
				 (stk_file[17] << 24 | stk_file[18] << 16 |
				  stk_file[19] << 8 | stk_file[20]));
		} else {
			dev_err(&stk->client->dev,
				"%s: cali version number error!\n", __func__);
			for (aa = 0; aa < STK_ACC_CALI_FILE_SIZE; aa++)
				dev_info(&stk->client->dev, "%s:buf[%d]=%x\n",
					 __func__, aa, stk_file[aa]);
		}
	}
	return 0;
}

static int stk8baxx_verify_cali(struct stk8baxx_data *stk, uint32_t delay_ms)
{
	unsigned char axis, state;
	int acc_ave[3] = { 0, 0, 0 };
	const unsigned char verify_sample_no = 3;
	const unsigned char verify_diff = STK_LSB_1G / 10;
	int ret = 0;

	msleep(delay_ms);
	for (state = 0; state < verify_sample_no; state++) {
		msleep(delay_ms);
		stk8baxx_read_sensor_data(stk);
		acc_ave[0] += stk->acc_xyz.x;
		acc_ave[1] += stk->acc_xyz.y;
		acc_ave[2] += stk->acc_xyz.z;
#ifdef STK_DEBUG_CALI
		dev_info(&stk->client->dev, "%s: acc=%d,%d,%d\n", __func__,
			 stk->acc_xyz.x, stk->acc_xyz.y, stk->acc_xyz.z);
#endif
	}

	for (axis = 0; axis < 3; axis++)
		acc_ave[axis] /= verify_sample_no;

	if (acc_ave[2] > 0)
		acc_ave[2] -= STK_LSB_1G;
	else
		acc_ave[2] += STK_LSB_1G;

	if (abs(acc_ave[0]) > verify_diff || abs(acc_ave[1]) > verify_diff
	    || abs(acc_ave[2]) > verify_diff) {
		dev_info(&stk->client->dev, "%s:Check data x:%d, y:%d, z:%d\n",
			 __func__, acc_ave[0], acc_ave[1], acc_ave[2]);
		dev_err(&stk->client->dev, "%s:Check Fail, Calibration Fail\n",
			__func__);
		ret = -STK_K_FAIL_LRG_DIFF;
	}
#ifdef STK_DEBUG_CALI
	else {
		dev_info(&stk->client->dev, "%s:Check data pass\n", __func__);
	}
#endif

	return ret;
}

static int stk8baxx_set_cali_scale_ofst(struct stk8baxx_data *stk, int acc[3])
{
	int result;
	int xyz_sensitivity = 256;
	int axis;

	result = stk8baxx_smbus_read_byte_data(STK8BAXX_RANGESEL);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s: failed to read acc data, error=%d\n", __func__,
			result);
		return result;
	}

	result &= STK8BAXX_RANGE_MASK;
	dev_info(&stk->client->dev, "%s: range=0x%x\n", __func__, result);
#ifdef CONFIG_SENSORS_STK8BA53
	switch (result) {
	case STK8BAXX_RNG_2G:
		xyz_sensitivity = 1024;
		break;
	case STK8BAXX_RNG_4G:
		xyz_sensitivity = 512;
		break;
	case STK8BAXX_RNG_8G:
		xyz_sensitivity = 256;
		break;
	case STK8BAXX_RNG_16G:
		xyz_sensitivity = 128;
		break;
	default:
		xyz_sensitivity = 512;
	}
#else
	switch (result) {
	case STK8BAXX_RNG_2G:
		xyz_sensitivity = 256;
		break;
	case STK8BAXX_RNG_4G:
		xyz_sensitivity = 128;
		break;
	case STK8BAXX_RNG_8G:
		xyz_sensitivity = 64;
		break;
	case STK8BAXX_RNG_16G:
		xyz_sensitivity = 32;
		break;
	default:
		xyz_sensitivity = 256;
	}
#endif
	for (axis = 0; axis < 3; axis++) {
		if (acc[axis] > 0) {
			acc[axis] =
			    (acc[axis] * STK_OFFSET_REG_LSB_1G +
			     xyz_sensitivity / 2)
			    / xyz_sensitivity;
		} else {
			acc[axis] =
			    (acc[axis] * STK_OFFSET_REG_LSB_1G -
			     xyz_sensitivity / 2)
			    / xyz_sensitivity;
		}
	}
	return 0;
}

static int stk8baxx_set_cali_do(struct stk8baxx_data *stk,
				unsigned int delay_ms)
{
	int sample_no, axis;
	int acc_ave[3] = { 0, 0, 0 };
	u8 offset[3];
	u8 offset_in_reg[3];
	int result;

	msleep(delay_ms * STK_EVENT_SINCE_EN_LIMIT_DEF);
	for (sample_no = 0; sample_no < STK_SAMPLE_NO; sample_no++) {
		msleep(delay_ms);
		stk8baxx_read_sensor_data(stk);
		acc_ave[0] += stk->acc_xyz.x;
		acc_ave[1] += stk->acc_xyz.y;
		acc_ave[2] += stk->acc_xyz.z;
#ifdef STK_DEBUG_CALI
		dev_info(&stk->client->dev, "%s: acc=%d,%d,%d\n", __func__,
			 stk->acc_xyz.x, stk->acc_xyz.y, stk->acc_xyz.z);
#endif
	}

	for (axis = 0; axis < 3; axis++) {
		if (acc_ave[axis] >= 0)
			acc_ave[axis] =
			    (acc_ave[axis] + STK_SAMPLE_NO / 2) / STK_SAMPLE_NO;
		else
			acc_ave[axis] =
			    (acc_ave[axis] - STK_SAMPLE_NO / 2) / STK_SAMPLE_NO;
	}

	if (acc_ave[2] > 0)
		acc_ave[2] -= STK_LSB_1G;
	else
		acc_ave[2] += STK_LSB_1G;

	stk8baxx_set_cali_scale_ofst(stk, acc_ave);

	for (axis = 0; axis < 3; axis++) {
		offset[axis] = -acc_ave[axis];
	}
	dev_info(&stk->client->dev, "%s: New offset for reg:%d,%d,%d\n",
		 __func__, offset[0], offset[1], offset[2]);

	stk8baxx_set_offset(stk, offset, 1);
	result =
	    stk8axxx_smbus_read_i2c_block_data(STK8BAXX_OFSTFILTX, 3,
					       offset_in_reg);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_OFSTFILTX, result);
		return result;
	}

	for (axis = 0; axis < 3; axis++) {
		if (offset[axis] != offset_in_reg[axis]) {
			dev_err(&stk->client->dev,
				"%s: set offset to register fail!, offset[%d]=%d,offset_in_reg[%d]=%d\n",
				__func__, axis, offset[axis], axis,
				offset_in_reg[axis]);
			atomic_set(&stk->cali_status, STK_K_FAIL_WRITE_NOFST);
			return -STK_K_FAIL_WRITE_NOFST;
		}
	}

	result = stk8baxx_verify_cali(stk, delay_ms);
	if (result) {
		dev_err(&stk->client->dev,
			"%s: calibration check fail, result=0x%x\n", __func__,
			result);
		atomic_set(&stk->cali_status, -result);
		return result;
	}
#ifndef STK_CALI_NO_FILE
	result = stk8baxx_store_in_file(offset, STK_K_SUCCESS_FILE, NULL);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to stk8baxx_store_in_file, error=%d\n",
			__func__, result);
		atomic_set(&stk->cali_status, STK_K_FAIL_W_FILE);
		return result;
	}
#endif
	atomic_set(&stk->cali_status, STK_K_SUCCESS_FILE);
#ifdef STK_TUNE
	stk->stk_tune_offset_record[0] = 0;
	stk->stk_tune_offset_record[1] = 0;
	stk->stk_tune_offset_record[2] = 0;
	stk->stk_tune_done = 1;
#endif
	return 0;
}

static int stk8baxx_set_cali(struct stk8baxx_data *stk, char sstate)
{
	int result;
	char enabled;
	s64 org_delay;
	uint32_t real_delay_ms;

	atomic_set(&stk->cali_status, STK_K_RUNNING);
	stk8baxx_get_enable(stk, &enabled);
	org_delay = stk8baxx_get_delay(stk);

	result = stk8baxx_set_delay(stk, 8000000);	/*      150 Hz ODR */
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to stk8baxx_set_delay, error=%d\n", __func__,
			result);
		atomic_set(&stk->cali_status, STK_K_FAIL_I2C);
		goto k_exit;
	}
	real_delay_ms = stk8baxx_get_delay(stk);
	real_delay_ms /= NSEC_PER_MSEC;
#ifdef STK_DEBUG_CALI
	dev_info(&stk->client->dev, "%s: real_delay_ms =%d ms\n", __func__,
		 real_delay_ms);
#endif

	if (enabled)
		stk8baxx_set_enable(stk, 0);

	result =
	    stk8baxx_smbus_write_byte_data(STK8BAXX_POWMODE,
					   STK8BAXX_MD_NORMAL);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_POWMODE, result);
		atomic_set(&stk->cali_status, STK_K_FAIL_I2C);
		goto k_exit;
	}
	result =
	    stk8baxx_smbus_write_byte_data(STK8BAXX_OFSTCOMP1, CAL_OFST_RST);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_OFSTCOMP1, result);
		atomic_set(&stk->cali_status, STK_K_FAIL_I2C);
		goto k_exit;
	}
	result = stk8baxx_set_cali_do(stk, (unsigned int)real_delay_ms);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to stk8baxx_set_cali_do, error=%d\n",
			__func__, result);
		atomic_set(&stk->cali_status, -result);
		goto k_exit;
	}

	if (enabled) {
		stk8baxx_set_enable(stk, 1);
	} else {
		result =
		    stk8baxx_smbus_write_byte_data(STK8BAXX_POWMODE,
						   STK8BAXX_MD_SUSPEND);
		if (result < 0) {
			dev_err(&stk->client->dev,
				"%s:failed to write reg 0x%x, error=%d\n",
				__func__, STK8BAXX_POWMODE, result);
			atomic_set(&stk->cali_status, STK_K_FAIL_I2C);
			goto k_exit;
		}
	}

	stk8baxx_set_delay(stk, org_delay);
	dev_info(&stk->client->dev, "%s: successful calibration\n", __func__);
	return 0;

k_exit:
	if (enabled)
		stk8baxx_set_enable(stk, 1);
	else
		stk8baxx_smbus_write_byte_data(STK8BAXX_POWMODE,
					       STK8BAXX_MD_SUSPEND);
	stk8baxx_set_delay(stk, org_delay);
	return result;
}

static void stk8baxx_load_cali(struct stk8baxx_data *stk)
{
#ifdef STK_CALI_NO_FILE
	if (stk->stk_tune_offset_record[0] != 0
	    || stk->stk_tune_offset_record[1] != 0
	    || stk->stk_tune_offset_record[2] != 0) {
		stk8baxx_set_offset(stk, stk->stk_tune_offset_record, 0);
		stk->stk_tune_done = 1;
		atomic_set(&stk->cali_status, STK_K_SUCCESS_TUNE);
		dev_info(&stk->client->dev, "%s: set offset:%d,%d,%d\n",
			 __func__, stk->stk_tune_offset_record[0],
			 stk->stk_tune_offset_record[1],
			 stk->stk_tune_offset_record[2]);
	}
#else
	char stk_file[STK_ACC_CALI_FILE_SIZE];
	u8 offset[3], mode;
	int aa;

	if ((stk8baxx_get_file_content(stk_file, STK_ACC_CALI_FILE_SIZE)) == 0) {
		if (stk_file[0] == STK_ACC_CALI_VER0
		    && stk_file[1] == STK_ACC_CALI_VER1
		    && stk_file[STK_ACC_CALI_FILE_SIZE - 1] ==
		    STK_ACC_CALI_END) {
			offset[0] = stk_file[3];
			offset[1] = stk_file[5];
			offset[2] = stk_file[7];
			mode = stk_file[8];
			stk8baxx_set_offset(stk, offset, 0);
			atomic_set(&stk->cali_status, mode);
			dev_info(&stk->client->dev,
				 "%s: set offset:%d,%d,%d, mode=%d\n", __func__,
				 offset[0], offset[1], offset[2], mode);
			dev_info(&stk->client->dev, "%s: variance=%u,%u,%u\n",
				 __func__,
				 (stk_file[9] << 24 | stk_file[10] << 16 |
				  stk_file[11] << 8 | stk_file[12]),
				 (stk_file[13] << 24 | stk_file[14] << 16 |
				  stk_file[15] << 8 | stk_file[16]),
				 (stk_file[17] << 24 | stk_file[18] << 16 |
				  stk_file[19] << 8 | stk_file[20]));
#ifdef STK_TUNE
			stk->stk_tune_offset_record[0] = offset[0];
			stk->stk_tune_offset_record[1] = offset[1];
			stk->stk_tune_offset_record[2] = offset[2];
#endif
		} else {
			dev_err(&stk->client->dev,
				"%s: cali version number error!\n", __func__);
			for (aa = 0; aa < STK_ACC_CALI_FILE_SIZE; aa++)
				dev_info(&stk->client->dev, "%s:buf[%d]=%x\n",
					 __func__, aa, stk_file[aa]);
		}
	}
#ifdef STK_TUNE
	else if (stk->stk_tune_offset_record[0] != 0
		 || stk->stk_tune_offset_record[1] != 0
		 || stk->stk_tune_offset_record[2] != 0) {
		stk8baxx_set_offset(stk, stk->stk_tune_offset_record, 0);
		stk->stk_tune_done = 1;
		atomic_set(&stk->cali_status, STK_K_SUCCESS_TUNE);
		dev_info(&stk->client->dev, "%s: set offset:%d,%d,%d\n",
			 __func__, stk->stk_tune_offset_record[0],
			 stk->stk_tune_offset_record[1],
			 stk->stk_tune_offset_record[2]);
	}
#endif
	else {
		offset[0] = offset[1] = offset[2] = 0;
		stk8baxx_store_in_file(offset, STK_K_NO_CALI, NULL);
		atomic_set(&stk->cali_status, STK_K_NO_CALI);
	}
	dev_info(&stk->client->dev, "%s: cali_status=0x%x\n", __func__,
		 atomic_read(&stk->cali_status));
#endif
}

static int stk8baxx_test_offset_noise(struct stk8baxx_data *stk)
{
	int result;
	uint32_t real_delay_ms;
	unsigned int sample_no;
	int acc_ave[3] = { 0, 0, 0 };
	int acc_max[3] = { INT_MIN, INT_MIN, INT_MIN };
	int acc_min[3] = { INT_MAX, INT_MAX, INT_MAX };
	int noise[3] = { 0, 0, 0 };
	int axis;
	int err_code = 0;

	result = stk8baxx_sw_reset(stk);
	if (result != 0) {
		dev_err(&stk->client->dev, "stk8baxx set reset error\n");
		return result;
	}

	result = stk8baxx_enter_active(stk);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to stk8baxx_enter_active, error=%d\n",
			__func__, result);
		return result;
	}

	result = stk8baxx_set_delay(stk, 8000000);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to stk8baxx_set_delay, error=%d\n", __func__,
			result);
		return result;
	}

	result = stk8baxx_set_range(stk, STK8BAXX_RNG_2G);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to stk8baxx_set_range, error=%d\n", __func__,
			result);
		return result;
	}

	real_delay_ms = stk8baxx_get_delay(stk);
	real_delay_ms /= NSEC_PER_MSEC;

	result = stk8baxx_enter_active(stk);
	if (result < 0) {
		dev_info(&stk->client->dev, "set power mode failed!\n");
		return result;
	}

	msleep(real_delay_ms * (STK_EVENT_SINCE_EN_LIMIT_DEF + 3));

	for (sample_no = 0; sample_no < STK_SELFTEST_SAMPLE_NO; sample_no++) {
		msleep(real_delay_ms);
		stk8baxx_read_sensor_data(stk);
		dev_info(&stk->client->dev, "%s: acc=%d,%d,%d\n", __func__,
			 stk->acc_xyz.x, stk->acc_xyz.y, stk->acc_xyz.z);

		for (axis = 0; axis < 3; axis++) {
			acc_ave[axis] += stk->acc_xyz.acc[axis];

			if (stk->acc_xyz.acc[axis] > acc_max[axis])
				acc_max[axis] = stk->acc_xyz.acc[axis];

			if (stk->acc_xyz.acc[axis] < acc_min[axis])
				acc_min[axis] = stk->acc_xyz.acc[axis];
		}
	}

	for (axis = 0; axis < 3; axis++) {
		if (acc_ave[axis] >= 0)
			acc_ave[axis] =
			    (acc_ave[axis] +
			     STK_SELFTEST_SAMPLE_NO / 2) /
			    STK_SELFTEST_SAMPLE_NO;
		else
			acc_ave[axis] =
			    (acc_ave[axis] -
			     STK_SELFTEST_SAMPLE_NO / 2) /
			    STK_SELFTEST_SAMPLE_NO;

		noise[axis] = acc_max[axis] - acc_min[axis];
	}

	dev_info(&stk->client->dev, "%s: placement_no =%d\n", __func__,
		 stk->stk8baxx_placement);
	dev_info(&stk->client->dev, "%s: acc_ave=%d,%d,%d,noise=%d,%d,%d\n",
		 __func__, acc_ave[0], acc_ave[1], acc_ave[2], noise[0],
		 noise[1], noise[2]);

	if (acc_ave[0] == 0 && acc_ave[1] == 0 && acc_ave[2] == 0) {
		dev_err(&stk->client->dev, "NO_OUTPUT\n");
		err_code |=
		    (STK_SELFTEST_STAT_FAIL_X | STK_SELFTEST_STAT_FAIL_Y |
		     STK_SELFTEST_STAT_FAIL_Z);
	}

	if (abs(acc_ave[0]) >= STK_SELFTEST_SATU) {
		dev_err(&stk->client->dev, "X_OFFSET_SATURATION, %d\n",
			(int)abs(acc_ave[0]));
		err_code |= STK_SELFTEST_STAT_FAIL_X;
	}

	if (abs(acc_ave[1]) >= STK_SELFTEST_SATU) {
		dev_err(&stk->client->dev, "Y_OFFSET_SATURATION, %d\n",
			(int)abs(acc_ave[1]));
		err_code |= STK_SELFTEST_STAT_FAIL_Y;
	}

	if (abs(acc_ave[2]) >= STK_SELFTEST_SATU) {
		dev_err(&stk->client->dev, "Z_OFFSET_SATURATION, %d\n",
			(int)abs(acc_ave[2]));
		err_code |= STK_SELFTEST_STAT_FAIL_Z;
	}

	if (noise[0] == 0) {
		dev_err(&stk->client->dev, "X_INVALID\n");
		err_code |= STK_SELFTEST_STAT_FAIL_X;
	}

	if (noise[1] == 0) {
		dev_err(&stk->client->dev, "Y_INVALID\n");
		err_code |= STK_SELFTEST_STAT_FAIL_Y;
	}

	if (noise[2] == 0) {
		dev_err(&stk->client->dev, "Z_INVALID\n");
		err_code |= STK_SELFTEST_STAT_FAIL_Z;
	}

	if (abs(acc_ave[0]) >= STK_SELFTEST_OFFSET_X) {
		dev_err(&stk->client->dev, "X_OFFSET_OUT_OF_SPEC, %d\n",
			(int)abs(acc_ave[0]));
		err_code |= STK_SELFTEST_STAT_FAIL_X;
	}

	if (abs(acc_ave[1]) >= STK_SELFTEST_OFFSET_Y) {
		dev_err(&stk->client->dev, "Y_OFFSET_OUT_OF_SPEC, %d\n",
			(int)abs(acc_ave[1]));
		err_code |= STK_SELFTEST_STAT_FAIL_Y;
	}

	if (acc_ave[2] > 0)
		acc_ave[2] -= STK_SELFTEST_LSB_1G;
	else
		acc_ave[2] += STK_SELFTEST_LSB_1G;

	if (abs(acc_ave[2]) >= STK_SELFTEST_OFFSET_Z) {
		dev_err(&stk->client->dev, "Z_OFFSET_OUT_OF_SPEC, %d\n",
			(int)abs(acc_ave[2]));
		err_code |= STK_SELFTEST_STAT_FAIL_Z;
	}

	if (noise[0] >= STK_SELFTEST_NOISE_X) {
		dev_err(&stk->client->dev, "X_NOISE_FAIL, %d\n", noise[0]);
		err_code |= STK_SELFTEST_STAT_FAIL_X;
	}

	if (noise[1] >= STK_SELFTEST_NOISE_Y) {
		dev_err(&stk->client->dev, "Y_NOISE_FAIL, %d\n", noise[1]);
		err_code |= STK_SELFTEST_STAT_FAIL_Y;
	}

	if (noise[2] >= STK_SELFTEST_NOISE_Z) {
		dev_err(&stk->client->dev, "Z_NOISE_FAIL, %d\n", noise[2]);
		err_code |= STK_SELFTEST_STAT_FAIL_Z;
	}

	stk->selftest_status = err_code;

	return 0;
}

static int stk8baxx_set_enable(struct stk8baxx_data *stk, char en)
{
	s8 result;
	s8 write_buffer = 0;
	int new_enabled = (en) ? 1 : 0;
	int k_status = atomic_read(&stk->cali_status);

#ifdef STK_QUALCOMM_PLATFORM
	result = stk8baxx_power_on(stk, true);
	if (result) {
		dev_err(&stk->client->dev, "%s: power_on failed, err=%d\n",
			__func__, result);
		goto error_enable;
	}
#endif

	if (stk->first_enable && k_status != STK_K_RUNNING) {
		stk->first_enable = false;
		stk8baxx_load_cali(stk);
	}

	if (new_enabled == atomic_read(&stk->enabled))
		return 0;
	dev_info(&stk->client->dev, "%s:%x\n", __func__, en);

	if (en)
		write_buffer = STK8BAXX_MD_NORMAL;
	else
		write_buffer = STK8BAXX_MD_SUSPEND;

	result = stk8baxx_smbus_write_byte_data(STK8BAXX_POWMODE, write_buffer);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_POWMODE, result);
		goto error_enable;
	}

	if (en) {
		stk->event_since_en = 0;
		stk->old_timespec.tv_sec = 0L;
		stk->old_timespec.tv_nsec = 0L;
		stk->new_timespec.tv_sec = 0L;
		stk->new_timespec.tv_nsec = 0L;
#ifdef STK_TUNE
		if ((k_status & 0xF0) != 0 && stk->stk_tune_done == 0) {
			stk->stk_tune_index = 0;
			stk8baxx_reset_para(stk);
		}
#endif
#if STK_ACC_POLLING_MODE
		hrtimer_start(&stk->acc_timer, stk->poll_delay,
			      HRTIMER_MODE_REL);
#else
		enable_irq((unsigned int)stk->irq);
#endif
	} else {
#if STK_ACC_POLLING_MODE
		hrtimer_cancel(&stk->acc_timer);
		cancel_work_sync(&stk->stk_work);
#else
		disable_irq((unsigned int)stk->irq);
#endif
	}

	atomic_set(&stk->enabled, new_enabled);
	return 0;

error_enable:
#ifdef STK_QUALCOMM_PLATFORM
	stk8baxx_power_on(stk, false);
#endif
	return result;
}

static int stk8baxx_get_enable(struct stk8baxx_data *stk, char *gState)
{
	*gState = atomic_read(&stk->enabled);
	return 0;
}

static int stk8baxx_show_all_reg(struct stk8baxx_data *stk, char *show_buffer)
{
	u8 buffer[16] = "";
	int aa, bb, no;
	int len = 0;
	char en;
	int result;

	result = stk8baxx_get_enable(stk, &en);
	if (result < 0)
		dev_err(&stk->client->dev,
			"%s: stk8baxx_get_enable failed, error=%d\n", __func__,
			result);

	if (en == 0) {
		result = stk8baxx_enter_active(stk);
		if (result < 0)
			dev_err(&stk->client->dev,
				"%s: stk8baxx_set_enable failed, error=%d\n",
				__func__, result);
	}

	for (bb = 0; bb < 4; bb++) {
		result =
		    stk8axxx_smbus_read_i2c_block_data(bb * 0x10, 16, buffer);
		if (result < 0) {
			dev_err(&stk->client->dev, "%s:failed\n", __func__);
			return result;
		}

		for (aa = 0; aa < 16; aa++) {
			no = bb * 0x10 + aa;
			dev_info(&stk->client->dev, "stk reg[0x%x]=0x%x\n", no,
				 buffer[aa]);

			if (show_buffer != NULL)
				len +=
				    snprintf(show_buffer + len, PAGE_SIZE - len,
					     "0x%02x,", buffer[aa]);
		}
	}

	if (show_buffer != NULL)
		len += snprintf(show_buffer + len, PAGE_SIZE - len, "\n");

	if (en == 0)
		stk8baxx_enter_suspend(stk);

	return len;
}

#ifdef STK8BAXX_PERMISSION_THREAD
SYSCALL_DEFINE3(fchmodat, int, dfd, const char __user *, filename, mode_t,
		mode);
static struct task_struct *STKPermissionThread;

static int stk8baxx_permis_thread(void *data)
{
	int ret = 0;
	int retry = 0;
	mm_segment_t fs = get_fs();
	set_fs(KERNEL_DS);
	msleep(20000);
	do {
		msleep(5000);
		ret =
		    sys_fchmodat(AT_FDCWD,
				 "/sys/class/input/input0/driver/cali", 0666);
		ret =
		    sys_fchmodat(AT_FDCWD,
				 "/sys/class/input/input1/driver/cali", 0666);
		ret =
		    sys_fchmodat(AT_FDCWD,
				 "/sys/class/input/input2/driver/cali", 0666);
		ret =
		    sys_fchmodat(AT_FDCWD,
				 "/sys/class/input/input3/driver/cali", 0666);
		ret =
		    sys_fchmodat(AT_FDCWD,
				 "/sys/class/input/input4/driver/cali", 0666);
		ret =
		    sys_fchmodat(AT_FDCWD, "/sys/class/input/input0/cali",
				 0666);
		ret =
		    sys_fchmodat(AT_FDCWD, "/sys/class/input/input1/cali",
				 0666);
		ret =
		    sys_fchmodat(AT_FDCWD, "/sys/class/input/input2/cali",
				 0666);
		ret =
		    sys_fchmodat(AT_FDCWD, "/sys/class/input/input3/cali",
				 0666);
		ret =
		    sys_fchmodat(AT_FDCWD, "/sys/class/input/input4/cali",
				 0666);
		ret = sys_chmod(STK_ACC_CALI_FILE, 0666);
		ret = sys_fchmodat(AT_FDCWD, STK_ACC_CALI_FILE, 0666);
		ret = sys_chmod(STK_ACC_CALI_FILE_SDCARD, 0666);
		ret = sys_fchmodat(AT_FDCWD, STK_ACC_CALI_FILE_SDCARD, 0666);
		if (retry++ > 20)
			break;
	} while (ret == -ENOENT);
	set_fs(fs);
	dev_info(&stk->client->dev, "%s exit, retry=%d\n", __func__, retry);
	return 0;
}
#endif /*      #ifdef STK8BAXX_PERMISSION_THREAD       */

static int32_t stk8baxx_get_file_content(char *r_buf, int8_t buf_size)
{
	struct file *cali_file;
	mm_segment_t fs;
	ssize_t ret;
	int err;

	cali_file = filp_open(STK_ACC_CALI_FILE, O_RDONLY, 0);
	if (IS_ERR(cali_file)) {
		err = PTR_ERR(cali_file);
		printk(KERN_ERR "%s: filp_open error, no offset file!err=%d\n",
		       __func__, err);
		return -ENOENT;
	} else {
		fs = get_fs();
		set_fs(get_ds());
		ret =
		    cali_file->f_op->read(cali_file, r_buf,
					  STK_ACC_CALI_FILE_SIZE,
					  &cali_file->f_pos);
		if (ret < 0) {
			printk(KERN_ERR "%s: read error, ret=%d\n", __func__,
			       (int)ret);
			filp_close(cali_file, NULL);
			return -EIO;
		}
		set_fs(fs);
	}
	filp_close(cali_file, NULL);
	return 0;
}

#ifndef STK_CALI_NO_FILE
static int stk8baxx_write_file(int mode, char write_buf[])
{
	struct file *cali_file;
	char r_buf[STK_ACC_CALI_FILE_SIZE] = { 0 };
	mm_segment_t fs;
	ssize_t ret;
	int8_t i;
	int err;

	if (mode == 0)
		cali_file =
		    filp_open(STK_ACC_CALI_FILE, O_CREAT | O_RDWR, 0666);
	else
		cali_file =
		    filp_open(STK_ACC_CALI_FILE_SDCARD, O_CREAT | O_RDWR, 0666);

	if (IS_ERR(cali_file)) {
		err = PTR_ERR(cali_file);
		if (mode == 0)
			printk(KERN_ERR "%s: filp_open error!err=%d,path=%s\n",
			       __func__, err, STK_ACC_CALI_FILE);
		else
			printk(KERN_ERR "%s: filp_open error!err=%d,path=%s\n",
			       __func__, err, STK_ACC_CALI_FILE_SDCARD);
		return -STK_K_FAIL_OPEN_FILE;
	} else {
		fs = get_fs();
		set_fs(get_ds());

		ret =
		    cali_file->f_op->write(cali_file, write_buf,
					   STK_ACC_CALI_FILE_SIZE,
					   &cali_file->f_pos);
		if (ret != STK_ACC_CALI_FILE_SIZE) {
			printk(KERN_ERR "%s: write error!\n", __func__);
			filp_close(cali_file, NULL);
			return -STK_K_FAIL_W_FILE;
		}
		cali_file->f_pos = 0x00;
		ret =
		    cali_file->f_op->read(cali_file, r_buf,
					  STK_ACC_CALI_FILE_SIZE,
					  &cali_file->f_pos);
		if (ret < 0) {
			printk(KERN_ERR "%s: read error!\n", __func__);
			filp_close(cali_file, NULL);
			return -STK_K_FAIL_R_BACK;

		}
		set_fs(fs);

		for (i = 0; i < STK_ACC_CALI_FILE_SIZE; i++) {
			if (r_buf[i] != write_buf[i]) {
				printk(KERN_ERR
				       "%s: read back error, r_buf[%x](0x%x) != write_buf[%x](0x%x)\n",
				       __func__, i, r_buf[i], i, write_buf[i]);
				filp_close(cali_file, NULL);
				return -STK_K_FAIL_R_BACK_COMP;
			}
		}
	}
	filp_close(cali_file, NULL);

#ifdef STK_PERMISSION_THREAD
	fs = get_fs();
	set_fs(KERNEL_DS);
	if (mode == 0) {
		ret = sys_chmod(STK_ACC_CALI_FILE, 0666);
		ret = sys_fchmodat(AT_FDCWD, STK_ACC_CALI_FILE, 0666);
	} else {
		ret = sys_chmod(STK_ACC_CALI_FILE_SDCARD, 0666);
		ret = sys_fchmodat(AT_FDCWD, STK_ACC_CALI_FILE_SDCARD, 0666);
	}
	set_fs(fs);
#endif
	return 0;
}

static int stk8baxx_store_in_file(u8 offset[], u8 status, u32 variance[])
{
	int err;
	char file_buf[STK_ACC_CALI_FILE_SIZE];

	memset(file_buf, 0, sizeof(file_buf));

	file_buf[0] = STK_ACC_CALI_VER0;
	file_buf[1] = STK_ACC_CALI_VER1;
	file_buf[3] = offset[0];
	file_buf[5] = offset[1];
	file_buf[7] = offset[2];
	file_buf[8] = status;

	if (variance != NULL) {
		file_buf[9] = ((variance[0] >> 24) & 0xFF);
		file_buf[10] = ((variance[0] >> 16) & 0xFF);
		file_buf[11] = ((variance[0] >> 8) & 0xFF);
		file_buf[12] = (variance[0] & 0xFF);
		file_buf[13] = ((variance[1] >> 24) & 0xFF);
		file_buf[14] = ((variance[1] >> 16) & 0xFF);
		file_buf[15] = ((variance[1] >> 8) & 0xFF);
		file_buf[16] = (variance[1] & 0xFF);
		file_buf[17] = ((variance[2] >> 24) & 0xFF);
		file_buf[18] = ((variance[2] >> 16) & 0xFF);
		file_buf[19] = ((variance[2] >> 8) & 0xFF);
		file_buf[20] = (variance[2] & 0xFF);
	}
	file_buf[STK_ACC_CALI_FILE_SIZE - 2] = '\0';
	file_buf[STK_ACC_CALI_FILE_SIZE - 1] = STK_ACC_CALI_END;
	stk8baxx_write_file(1, file_buf);
	err = stk8baxx_write_file(0, file_buf);
	if (err == 0)
		printk(KERN_INFO "%s successfully\n", __func__);
	return err;
}
#endif

static ssize_t stk8baxx_enable_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{

	struct stk8baxx_data *stk = stk8baxx_data_ptr;
	char en;
	stk8baxx_get_enable(stk, &en);
	return scnprintf(buf, PAGE_SIZE, "%d\n", en);
}

static ssize_t stk8baxx_enable_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct stk8baxx_data *stk = stk8baxx_data_ptr;
	unsigned long data;
	int error;
	error = kstrtoul(buf, 10, &data);
	if (error) {
		dev_err(&stk->client->dev, "%s: kstrtoul failed, error=%d\n",
			__func__, error);
		return error;
	}

	if ((data == 0) || (data == 1)) {
		stk8baxx_set_enable(stk, data);
	} else {
		dev_err(&stk->client->dev, "%s: invalud argument, data=%ld\n",
			__func__, data);
	}

	return count;
}

static ssize_t stk8baxx_value_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct stk8baxx_data *stk = dev_get_drvdata(dev);
	int ddata[3];

	dev_info(&stk->client->dev, "driver version:%s\n",
		 STK_ACC_DRIVER_VERSION);
	stk8baxx_read_sensor_data(stk);
	ddata[0] = stk->acc_xyz.x;
	ddata[1] = stk->acc_xyz.y;
	ddata[2] = stk->acc_xyz.z;
	return scnprintf(buf, PAGE_SIZE, "%d %d %d\n", ddata[0], ddata[1],
			 ddata[2]);
}

static ssize_t stk8baxx_delay_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct stk8baxx_data *stk = stk8baxx_data_ptr;
	return scnprintf(buf, PAGE_SIZE, "%lld\n",
			 (long long)stk8baxx_get_delay(stk));
}

static ssize_t stk8baxx_delay_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct stk8baxx_data *stk = stk8baxx_data_ptr;
	unsigned long data;
	int error;

	error = kstrtoul(buf, 10, &data);
	if (error) {
		dev_err(&stk->client->dev, "%s: kstrtoul failed, error=%d\n",
			__func__, error);
		return error;
	}
	stk8baxx_set_delay(stk, data);
	return count;
}

static ssize_t stk8baxx_cali_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct stk8baxx_data *stk = dev_get_drvdata(dev);
	int status = atomic_read(&stk->cali_status);

	if (status != STK_K_RUNNING)
		stk8baxx_get_cali(stk);
	return scnprintf(buf, PAGE_SIZE, "%02x\n",
			 atomic_read(&stk->cali_status));
}

static ssize_t stk8baxx_cali_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct stk8baxx_data *stk = dev_get_drvdata(dev);

	if (sysfs_streq(buf, "1"))
		stk8baxx_set_cali(stk, 1);
	else {
		dev_err(&stk->client->dev, "%s, invalid value %d\n", __func__,
			*buf);
		return -EINVAL;
	}

	return count;
}

static ssize_t stk8baxx_offset_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct stk8baxx_data *stk = dev_get_drvdata(dev);
	u8 offset[3] = { 0 };

	stk8baxx_get_offset(stk, offset);
	dev_info(&stk->client->dev, "%s: offset = 0x%x, 0x%x, 0x%x\n", __func__,
		 offset[0], offset[1], offset[2]);
	dev_info(&stk->client->dev, "%s: var = %d, %d, %d\n", __func__,
		 stk->variance[0], stk->variance[1], stk->variance[2]);
	return scnprintf(buf, PAGE_SIZE, "%x %x %x %x %x %x\n", offset[0],
			 offset[1], offset[2], stk->variance[0],
			 stk->variance[1], stk->variance[2]);
}

static ssize_t stk8baxx_offset_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct stk8baxx_data *stk = dev_get_drvdata(dev);
	u8 offset[3] = { 0 };
	int error, i;
	int r_offset[3];
	char *token[10];

	for (i = 0; i < 3; i++)
		token[i] = strsep((char **)&buf, " ");

	error = kstrtoul(token[0], 16, (unsigned long *)&(r_offset[0]));

	if (error < 0) {
		dev_err(&stk->client->dev, "%s:kstrtoul failed, error=%d\n",
			__func__, error);
		return error;
	}

	error = kstrtoul(token[1], 16, (unsigned long *)&(r_offset[1]));
	if (error < 0) {
		dev_err(&stk->client->dev, "%s:kstrtoul failed, error=%d\n",
			__func__, error);
		return error;
	}
	error = kstrtoul(token[2], 16, (unsigned long *)&(r_offset[2]));
	if (error < 0) {
		dev_err(&stk->client->dev, "%s:kstrtoul failed, error=%d\n",
			__func__, error);
		return error;
	}
	dev_info(&stk->client->dev, "%s: offset = 0x%x, 0x%x, 0x%x\n", __func__,
		 r_offset[0], r_offset[1], r_offset[2]);

	for (i = 0; i < 3; i++) {
		offset[i] = (u8) r_offset[i];
#ifdef STK_CALI_NO_FILE
		stk->stk_tune_offset_record[i] = r_offset[i];
#endif
	}
	stk8baxx_set_offset(stk, offset, 0);

	return count;
}

static ssize_t stk8baxx_send_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t stk8baxx_send_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct stk8baxx_data *stk = dev_get_drvdata(dev);
	int error;
	int addr, cmd, i;
	char *token[10];
	int result;
	char en;

	for (i = 0; i < 2; i++)
		token[i] = strsep((char **)&buf, " ");
	error = kstrtoul(token[0], 16, (unsigned long *)&(addr));
	if (error < 0) {
		dev_err(&stk->client->dev, "%s:kstrtoul failed, error=%d\n",
			__func__, error);
		return error;
	}
	error = kstrtoul(token[1], 16, (unsigned long *)&(cmd));
	if (error < 0) {
		dev_err(&stk->client->dev, "%s:kstrtoul failed, error=%d\n",
			__func__, error);
		return error;
	}
	dev_info(&stk->client->dev, "%s: write reg 0x%x=0x%x\n", __func__, addr,
		 cmd);

	result = stk8baxx_get_enable(stk, &en);
	if (result < 0)
		dev_err(&stk->client->dev,
			"%s: stk8baxx_get_enable failed, error=%d\n", __func__,
			result);

	if (en == 0) {
		stk8baxx_enter_active(stk);
		usleep_range(2000, 5000);	/* for eng reg */
	}

	error = stk8baxx_smbus_write_byte_data((u8) addr, (u8) cmd);
	if (error < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			(u8) addr, error);
		return error;
	}

	if (en == 0)
		stk8baxx_enter_suspend(stk);
	return count;
}

static ssize_t stk8baxx_recv_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct stk8baxx_data *stk = dev_get_drvdata(dev);
	return scnprintf(buf, PAGE_SIZE, "0x%x\n", atomic_read(&stk->recv_reg));
}

static ssize_t stk8baxx_recv_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct stk8baxx_data *stk = dev_get_drvdata(dev);
	unsigned long data;
	int result;
	char en;

	result = kstrtoul(buf, 16, &data);
	if (result) {
		dev_err(&stk->client->dev, "%s: kstrtoul failed, result=%d\n",
			__func__, result);
		return result;
	}

	result = stk8baxx_get_enable(stk, &en);
	if (result < 0)
		dev_err(&stk->client->dev,
			"%s: stk8baxx_get_enable failed, error=%d\n", __func__,
			result);

	if (en == 0) {
		stk8baxx_enter_active(stk);
		usleep_range(2000, 5000);	/* for eng reg */
	}

	result = stk8baxx_smbus_read_byte_data((u8) data);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s: failed to read reg 0x%x, result=%d\n", __func__,
			(int)data, result);
		return result;
	}
	atomic_set(&stk->recv_reg, result);
	dev_info(&stk->client->dev, "%s: reg 0x%x=0x%x\n", __func__,
		 (unsigned int)data, result);

	if (en == 0)
		stk8baxx_enter_suspend(stk);

	return count;
}

static ssize_t stk8baxx_firlen_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
#ifdef STK_LOWPASS
	struct stk8baxx_data *stk = dev_get_drvdata(dev);
	int len = atomic_read(&stk->firlength);

	if (atomic_read(&stk->firlength)) {
		dev_info(&stk->client->dev, "len = %2d, idx = %2d\n",
			 stk->fir.num, stk->fir.idx);
		dev_info(&stk->client->dev, "sum = [%5d %5d %5d]\n",
			 stk->fir.sum[0], stk->fir.sum[1], stk->fir.sum[2]);
		dev_info(&stk->client->dev, "avg = [%5d %5d %5d]\n",
			 stk->fir.sum[0] / len, stk->fir.sum[1] / len,
			 stk->fir.sum[2] / len);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&stk->firlength));
#else
	return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}

static ssize_t stk8baxx_firlen_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct stk8baxx_data *stk = dev_get_drvdata(dev);
#ifdef STK_LOWPASS
	int error;
	unsigned long data;

	error = kstrtoul(buf, 10, &data);
	if (error) {
		dev_err(&stk->client->dev, "%s: kstrtoul failed, error=%d\n",
			__func__, error);
		return error;
	}

	if (data > MAX_FIR_LEN) {
		dev_err(&stk->client->dev,
			"%s: firlen exceed maximum filter length\n", __func__);
	} else if (data < 1) {
		atomic_set(&stk->firlength, 1);
		atomic_set(&stk->fir_en, 0);
		memset(&stk->fir, 0x00, sizeof(stk->fir));
	} else {
		atomic_set(&stk->firlength, data);
		memset(&stk->fir, 0x00, sizeof(stk->fir));
		atomic_set(&stk->fir_en, 1);
	}
#else
	dev_err(&stk->client->dev, "%s: firlen is not supported\n", __func__);
#endif
	return count;
}

static ssize_t stk8baxx_allreg_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct stk8baxx_data *stk = dev_get_drvdata(dev);
	int result;

	result = stk8baxx_show_all_reg(stk, buf);
	if (result < 0)
		return result;

	return (ssize_t) result;
}

static ssize_t stk8baxx_chipinfo_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct stk8baxx_data *stk = dev_get_drvdata(dev);

	if (stk->pid == STK8BA50_ID)
		return snprintf(buf, PAGE_SIZE, "stk8ba50\n");
	else if (stk->pid == STK8BA50R_ID)
		return snprintf(buf, PAGE_SIZE, "stk8ba50r\n");
	else if (stk->pid == STK8BA53_ID)
		return snprintf(buf, PAGE_SIZE, "stk8ba53\n");

	return snprintf(buf, PAGE_SIZE, "unknown\n");
}

static ssize_t stk8baxx_selftest_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct stk8baxx_data *stk = dev_get_drvdata(dev);

	dev_info(&stk->client->dev, ":version=%s\n", STK_SELFTEST_VERSION);

	return scnprintf(buf, PAGE_SIZE, "%02X\n", stk->selftest_status);
}

static ssize_t stk8baxx_selftest_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct stk8baxx_data *stk = dev_get_drvdata(dev);
	unsigned long data;
	int result;
	char en;

	result = kstrtoul(buf, 16, &data);
	if (result) {
		dev_err(&stk->client->dev, "%s: kstrtoul failed, result=%d\n",
			__func__, result);
		return result;
	}

	dev_info(&stk->client->dev, "%s: version=%s\n", __func__,
		 STK_SELFTEST_VERSION);
	stk->selftest_status = STK_SELFTEST_STAT_RUNNING;

	result = stk8baxx_get_enable(stk, &en);
	if (result < 0)
		dev_err(&stk->client->dev,
			"%s: stk8baxx_get_enable failed, error=%d\n", __func__,
			result);

	if (en == 0)
		stk8baxx_enter_active(stk);

	result = stk8baxx_check_id(stk);
	if (result < 0) {
		dev_err(&stk->client->dev, "%s:failed to check id, error=%d\n",
			__func__, result);
		stk->selftest_status = STK_SELFTEST_STAT_DRIVER_ERR;
		return result;
	}

	result = stk8baxx_show_all_reg(stk, NULL);
	if (result < 0) {
		stk->selftest_status = STK_SELFTEST_STAT_DRIVER_ERR;
		return result;
	}

	if (data & 0x0001) {
		result = stk8baxx_test_offset_noise(stk);
		if (result < 0) {
			stk->selftest_status = STK_SELFTEST_STAT_DRIVER_ERR;
			dev_err(&stk->client->dev,
				"%s:failed to stk8baxx_test_offset_noise, error=%d\n",
				__func__, result);
			return result;
		}
	}
	dev_info(&stk->client->dev, "%s: selftest_status=%d\n", __func__,
		 stk->selftest_status);

	result = stk8baxx_reg_init(stk, stk->client);
	if (result) {
		dev_err(&stk->client->dev,
			"%s:stk8baxx initialization failed\n", __func__);
		return result;
	}

	if (en == 1)
		stk8baxx_enter_active(stk);

	return count;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP, stk8baxx_enable_show,
		   stk8baxx_enable_store);
static DEVICE_ATTR(value, S_IRUGO, stk8baxx_value_show, NULL);
static DEVICE_ATTR(delay, S_IRUGO | S_IWUSR | S_IWGRP, stk8baxx_delay_show,
		   stk8baxx_delay_store);
static DEVICE_ATTR(cali, S_IRUGO | S_IWUSR | S_IWGRP, stk8baxx_cali_show,
		   stk8baxx_cali_store);
static DEVICE_ATTR(offset, S_IRUGO | S_IWUSR | S_IWGRP, stk8baxx_offset_show,
		   stk8baxx_offset_store);
static DEVICE_ATTR(send, S_IRUGO | S_IWUSR | S_IWGRP, stk8baxx_send_show,
		   stk8baxx_send_store);
static DEVICE_ATTR(recv, S_IRUGO | S_IWUSR | S_IWGRP, stk8baxx_recv_show,
		   stk8baxx_recv_store);
static DEVICE_ATTR(firlen, S_IRUGO | S_IWUSR | S_IWGRP, stk8baxx_firlen_show,
		   stk8baxx_firlen_store);
static DEVICE_ATTR(allreg, S_IRUGO, stk8baxx_allreg_show, NULL);
static DEVICE_ATTR(chipinfo, S_IRUGO, stk8baxx_chipinfo_show, NULL);
static DEVICE_ATTR(selftest, S_IRUGO | S_IWUSR | S_IWGRP,
		   stk8baxx_selftest_show, stk8baxx_selftest_store);

static DEVICE_ATTR(gsensor, 0664, stk8baxx_enable_show, stk8baxx_enable_store);
static DEVICE_ATTR(delay_acc, 0664, stk8baxx_delay_show, stk8baxx_delay_store);

static struct attribute *stk8baxx_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_value.attr,
	&dev_attr_delay.attr,
	&dev_attr_cali.attr,
	&dev_attr_offset.attr,
	&dev_attr_send.attr,
	&dev_attr_recv.attr,
	&dev_attr_firlen.attr,
	&dev_attr_allreg.attr,
	&dev_attr_chipinfo.attr,
	&dev_attr_selftest.attr,
	NULL
};

static struct attribute_group stk8baxx_attribute_group = {
	.name = "driver",
	.attrs = stk8baxx_attributes,
};

static int stk8baxx_open(struct inode *inode, struct file *file)
{
	int ret;
	ret = nonseekable_open(inode, file);
	if (ret < 0)
		return ret;
	file->private_data = stk8baxx_data_ptr;
	return 0;
}

static int stk8baxx_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long stk8baxx_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int retval = 0;
	char state = 0;
	char rwbuf[10] = "";
	uint32_t delay_ns;
	char char3_buffer[3];
	int result;
	int int3_buffer[3];
	struct stk8baxx_data *stk = file->private_data;
	char en;

	result = stk8baxx_get_enable(stk, &en);
	if (result < 0)
		dev_err(&stk->client->dev,
			"%s: stk8baxx_get_enable failed, error=%d\n", __func__,
			result);

	if (en == 0 && cmd != STK_IOCTL_SET_ENABLE)
		stk8baxx_enter_active(stk);

	switch (cmd) {
	case STK_IOCTL_SET_OFFSET:
		if (copy_from_user(&char3_buffer, argp, sizeof(char3_buffer))) {
			retval = -EFAULT;
			goto ioctl_err;
		}
		break;
	case STK_IOCTL_SET_DELAY:
		if (copy_from_user(&delay_ns, argp, sizeof(uint32_t))) {
			retval = -EFAULT;
			goto ioctl_err;
		}
		break;
	case STK_IOCTL_WRITE:
	case STK_IOCTL_READ:
		if (copy_from_user(&char3_buffer, argp, sizeof(char3_buffer))) {
			retval = -EFAULT;
			goto ioctl_err;
		}
		break;
	case STK_IOCTL_SET_ENABLE:
	case STK_IOCTL_SET_RANGE:
	case STK_IOCTL_SET_CALI:
		if (copy_from_user(&state, argp, sizeof(char))) {
			retval = -EFAULT;
			goto ioctl_err;
		}
		break;
	default:
		break;
	}

	switch (cmd) {
	case STK_IOCTL_WRITE:
		if (rwbuf[0] < 2) {
			retval = -EINVAL;
			goto ioctl_err;
		}
		result = stk8baxx_smbus_write_byte_data(rwbuf[1], rwbuf[2]);	/*force write one byte */
		if (result < 0) {
			dev_err(&stk->client->dev,
				"%s:failed to write reg 0x%x, error=%d\n",
				__func__, rwbuf[1], result);
			retval = result;
			goto ioctl_err;
		}
		break;
	case STK_IOCTL_SET_OFFSET:
		stk8baxx_set_offset(stk, char3_buffer, 0);
		break;
	case STK_IOCTL_SET_DELAY:
		stk8baxx_set_delay(stk, delay_ns);
		break;
	case STK_IOCTL_READ:
		if (char3_buffer[0] < 1 || char3_buffer[0] > 10) {
			retval = -EINVAL;
			goto ioctl_err;
		}
		rwbuf[0] = char3_buffer[0];
		result =
		    stk8axxx_smbus_read_i2c_block_data(char3_buffer[1],
						       char3_buffer[0],
						       &rwbuf[1]);
		if (result < 0) {
			retval = result;
			goto ioctl_err;
		}
		break;
	case STK_IOCTL_GET_DELAY:
		delay_ns = stk8baxx_get_delay(stk);
		break;
	case STK_IOCTL_GET_OFFSET:
		stk8baxx_get_offset(stk, char3_buffer);
		break;
	case STK_IOCTL_GET_ACCELERATION:
	case STK_IOCTL_GET_COOR_XYZ:
		stk8baxx_read_sensor_data(stk);
		int3_buffer[0] = stk->acc_xyz.x;
		int3_buffer[1] = stk->acc_xyz.y;
		int3_buffer[2] = stk->acc_xyz.z;
		break;
	case STK_IOCTL_SET_ENABLE:
		stk8baxx_set_enable(stk, state);
		break;
	case STK_IOCTL_GET_ENABLE:
		stk8baxx_get_enable(stk, &state);
		break;
	case STK_IOCTL_SET_RANGE:
		stk8baxx_set_range(stk, state);
		break;
	case STK_IOCTL_GET_RANGE:
		stk8baxx_get_range(stk, &state);
		break;
	case STK_IOCTL_SET_CALI:
		stk8baxx_set_cali(stk, state);
		break;
	case STK_IOCTL_GET_CHIP_ID:
		{
#define DEVICE_INFO_LEN         32
#define DEVICE_INFO             "STK,stk8baxx"
			u8 devid = 0;
			u8 devinfo[DEVICE_INFO_LEN] = { 0 };
			devid = stk8baxx_check_id(stk);
			if (devid < 0) {
				printk("%s, error read register WHO_AM_I\n",
				       __func__);
				return -EAGAIN;
			}
			sprintf(devinfo, "%s, %#x", DEVICE_INFO, devid);
			if (copy_to_user(argp, devinfo, sizeof(devinfo))) {
				printk
				    ("%s error in copy_to_user(IOCTL_GET_CHIP_ID)\n",
				     __func__);
				return -EINVAL;
			}
		}
		break;
	default:
		retval = -ENOTTY;
		break;
	}
	if (en == 0 && cmd != STK_IOCTL_SET_ENABLE)
		stk8baxx_enter_suspend(stk);

	switch (cmd) {
	case STK_IOCTL_GET_ACCELERATION:
	case STK_IOCTL_GET_COOR_XYZ:
		if (copy_to_user(argp, &int3_buffer, sizeof(int3_buffer)))
			return -EFAULT;
		break;
	case STK_IOCTL_READ:
		if (copy_to_user(argp, &rwbuf, sizeof(rwbuf)))
			return -EFAULT;
		break;
	case STK_IOCTL_GET_DELAY:
		if (copy_to_user(argp, &delay_ns, sizeof(delay_ns)))
			return -EFAULT;
		break;
	case STK_IOCTL_GET_OFFSET:
		if (copy_to_user(argp, &char3_buffer, sizeof(char3_buffer)))
			return -EFAULT;
		break;
	case STK_IOCTL_GET_RANGE:
	case STK_IOCTL_GET_ENABLE:
		if (copy_to_user(argp, &state, sizeof(char)))
			return -EFAULT;
		break;
	default:
		break;
	}

	return retval;

ioctl_err:
	if (en == 0)
		stk8baxx_enter_suspend(stk);
	return retval;
}

static struct file_operations stk_fops = {
	.owner = THIS_MODULE,
	.open = stk8baxx_open,
	.release = stk8baxx_release,
	.unlocked_ioctl = stk8baxx_ioctl,
};

static struct miscdevice stk_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "stk8baxx",
	.fops = &stk_fops,
};

static void stk_mems_wq_function(struct work_struct *work)
{
	struct stk8baxx_data *stk =
	    container_of(work, struct stk8baxx_data, stk_work);
	stk8baxx_read_sensor_data(stk);
	stk8baxx_report_value(stk);
#if (!STK_ACC_POLLING_MODE)
	enable_irq(stk->irq);
#endif
}

#if STK_ACC_POLLING_MODE
static enum hrtimer_restart stk_acc_timer_func(struct hrtimer *timer)
{
	struct stk8baxx_data *stk =
	    container_of(timer, struct stk8baxx_data, acc_timer);
	queue_work(stk->stk_mems_work_queue, &stk->stk_work);
	hrtimer_forward_now(&stk->acc_timer, stk->poll_delay);
	return HRTIMER_RESTART;
}

#else

static irqreturn_t stk_mems_irq_fun(int irq, void *data)
{
	struct stk8baxx_data *stk = data;
	disable_irq_nosync(stk->irq);
	queue_work(stk->stk_mems_work_queue, &stk->stk_work);
	return IRQ_HANDLED;
}

static int stk8baxx_irq_setup(struct i2c_client *client,
			      struct stk8baxx_data *stk_dat, int int_pin)
{
	int error;
	int irq = -1;
#if ADDITIONAL_GPIO_CFG
	if (gpio_request(int_pin, "EINT")) {
		dev_err(&client->dev, "%s:gpio_request() failed\n", __func__);
		return -1;
	}
	gpio_direction_input(int_pin);

	irq = gpio_to_irq(int_pin);
	if (irq < 0) {
		dev_err(&client->dev, "%s:gpio_to_irq() failed\n", __func__);
		return -1;
	}
	client->irq = irq;
	stk_dat->irq = irq;
#endif
	dev_info(&client->dev, "%s: irq #=%d, int pin=%d\n", __func__, irq,
		 int_pin);

	error =
	    request_any_context_irq(client->irq, stk_mems_irq_fun,
				    IRQF_TRIGGER_FALLING, "stk8baxx", stk_dat);
	if (error < 0) {
		dev_err(&client->dev,
			"%s: request_threaded_irq(%d) failed for (%d)\n",
			__func__, client->irq, error);
		return -1;
	}

	disable_irq(irq);
	return irq;
}

#endif

#ifdef STK_QUALCOMM_PLATFORM
static int stk8baxx_enable_set(struct sensors_classdev *sensors_cdev,
			       unsigned int enabled)
{
	struct stk8baxx_data *stk = container_of(sensors_cdev,
						 struct stk8baxx_data, cdev);
	struct input_dev *input_dev = stk->input_dev;

	mutex_lock(&input_dev->mutex);

	if (enabled == 0) {
		stk8baxx_set_enable(stk, 0);
	} else if (enabled == 1) {
		stk8baxx_set_enable(stk, 1);
	} else {
		dev_err(&stk->client->dev,
			"Invalid value of input, input=%d\n", enabled);
		mutex_unlock(&input_dev->mutex);
		return -EINVAL;
	}

	mutex_unlock(&input_dev->mutex);

	return 0;
}

static int stk8baxx_poll_delay_set(struct sensors_classdev *sensors_cdev,
				   unsigned int delay_msec)
{
	struct stk8baxx_data *stk = container_of(sensors_cdev,
						 struct stk8baxx_data, cdev);
	struct input_dev *input_dev = stk->input_dev;

	mutex_lock(&input_dev->mutex);
	stk8baxx_set_delay(stk, delay_msec * 1000000UL);
	mutex_unlock(&input_dev->mutex);

	return 0;
}

#ifdef STK_QUALCOMM_MANUAL_CALI

static int stk8baxx_do_calibration(struct sensors_classdev *sensors_cdev,
				   int axis, int apply_now)
{
	struct stk8baxx_data *stk = container_of(sensors_cdev,
						 struct stk8baxx_data, cdev);
	int sum_x, sum_y, sum_z;
	int i;
	int xyz[3] = { 0 };
	int status = atomic_read(&stk->cali_status);
	int result;
	uint32_t real_delay_ms;
	u8 file_offset[3] = { 0 };
	int offset[3] = { 0 };
	u8 offset_u8[3] = { 0 };

	if (status == STK_K_RUNNING)
		return 0;

	atomic_set(&stk->cali_status, STK_K_RUNNING);

	if (atomic_read(&stk->enabled) == 0)
		stk8baxx_set_enable(stk, 1);

	result =
	    stk8baxx_smbus_write_byte_data(STK8BAXX_OFSTCOMP1, CAL_OFST_RST);
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to write reg 0x%x, error=%d\n", __func__,
			STK8BAXX_OFSTCOMP1, result);
		return result;
	}

	result = stk8baxx_set_delay(stk, 8000000);	/*      150 Hz ODR */
	if (result < 0) {
		dev_err(&stk->client->dev,
			"%s:failed to stk8baxx_set_delay, error=%d\n", __func__,
			result);
		return result;
	}

	real_delay_ms = stk8baxx_get_delay(stk);
	real_delay_ms /= NSEC_PER_MSEC;

	msleep(real_delay_ms * STK_EVENT_SINCE_EN_LIMIT_DEF);

	offset[0] = offset[1] = offset[2] = 0;
	sum_x = sum_y = sum_z = 0;

	for (i = 0; i < STK_QCOM_CALI_SAMPLE_NO; i++) {
		msleep(real_delay_ms);
		stk8baxx_read_sensor_data(stk);
		sum_x += stk->acc_xyz.x;
		sum_y += stk->acc_xyz.y;
		sum_z += stk->acc_xyz.z;
	}

	sum_x = sum_x / STK_QCOM_CALI_SAMPLE_NO;
	sum_y = sum_y / STK_QCOM_CALI_SAMPLE_NO;
	sum_z = sum_z / STK_QCOM_CALI_SAMPLE_NO;

	if (sum_z > 0)
		sum_z = sum_z - STK_LSB_1G;
	else
		sum_z = sum_z - (-STK_LSB_1G);

	offset[0] = sum_x;
	offset[1] = sum_y;
	offset[2] = sum_z;

	stk8baxx_set_cali_scale_ofst(stk, offset);

	dev_info(&stk->client->dev, "%s: offset after scale=%d,%d,%d\n",
		 __func__, offset[0], offset[1], offset[2]);

	switch (axis) {
	case AXIS_X:
		xyz[0] = offset[0];
		xyz[1] = 0;
		xyz[2] = 0;
		break;
	case AXIS_Y:
		xyz[0] = 0;
		xyz[1] = offset[1];
		xyz[2] = 0;
		break;
	case AXIS_Z:
		xyz[0] = 0;
		xyz[1] = 0;
		xyz[2] = offset[2];
		break;
	case AXIS_XYZ:
		xyz[0] = offset[0];
		xyz[1] = offset[1];
		xyz[2] = offset[2];
		break;
	}

	memset(stk->calibrate_buf, 0, sizeof(stk->calibrate_buf));
	snprintf(stk->calibrate_buf, sizeof(stk->calibrate_buf), "%d,%d,%d",
		 xyz[0], xyz[1], xyz[2]);
	sensors_cdev->params = stk->calibrate_buf;

	if (apply_now) {
		if (offset[0] > 0)
			offset_u8[0] = (u8) offset[0];
		else
			offset_u8[0] = (u8) (offset[0] + 0x100);

		if (offset[1] > 0)
			offset_u8[1] = (u8) offset[1];
		else
			offset_u8[1] = (u8) (offset[1] + 0x100);

		if (offset[2] > 0)
			offset_u8[2] = (u8) offset[2];
		else
			offset_u8[2] = (u8) (offset[2] + 0x100);

		stk8baxx_set_offset(stk, offset_u8, 1);
		stk8baxx_store_in_file(file_offset, STK_K_SUCCESS_QCOM, NULL);
		atomic_set(&stk->cali_status, STK_K_SUCCESS_QCOM);
	}

	return 0;
}

static int stk8baxx_write_cal_params(struct sensors_classdev *sensors_cdev,
				     struct cal_result_t *cal_result)
{
	struct stk8baxx_data *stk = container_of(sensors_cdev,
						 struct stk8baxx_data, cdev);
	u8 file_offset[3] = { 0, 0, 0 };
	u8 offset[3];
	u8 offset_u8[3] = { 0 };

	if (offset[0] > 0)
		offset_u8[0] = (u8) offset[0];
	else
		offset_u8[0] = (u8) (offset[0] + 0x100);

	if (offset[1] > 0)
		offset_u8[1] = (u8) offset[1];
	else
		offset_u8[1] = (u8) (offset[1] + 0x100);

	if (offset[2] > 0)
		offset_u8[2] = (u8) offset[2];
	else
		offset_u8[2] = (u8) (offset[2] + 0x100);

	stk8baxx_set_offset(stk, offset, 0);
	stk8baxx_store_in_file(file_offset, STK_K_SUCCESS_QCOM, NULL);
	atomic_set(&stk->cali_status, STK_K_SUCCESS_QCOM);

	dev_err(&stk->client->dev,
		"%s: read accel calibrate bias %d,%d,%d\n",
		__func__, offset[0], offset[1], offset[2]);

	return 0;
}
#endif

static int stk8baxx_power_on(struct stk8baxx_data *stk, bool on)
{
	int rc = 0;

	if (!on && stk->power_enabled) {
		rc = regulator_disable(stk->vdd);
		if (rc) {
			dev_err(&stk->client->dev,
				"Regulator vdd disable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_disable(stk->vio);
		if (rc) {
			dev_err(&stk->client->dev,
				"Regulator vio disable failed rc=%d\n", rc);
			rc = regulator_enable(stk->vdd);
			if (rc) {
				dev_err(&stk->client->dev,
					"Regulator vdd enable failed rc=%d\n",
					rc);
			}
		}
		stk->power_enabled = false;
	} else if (on && !stk->power_enabled) {
		rc = regulator_enable(stk->vdd);
		if (rc) {
			dev_err(&stk->client->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_enable(stk->vio);
		if (rc) {
			dev_err(&stk->client->dev,
				"Regulator vio enable failed rc=%d\n", rc);
			regulator_disable(stk->vdd);
		}
		stk->power_enabled = true;
	} else {
		dev_warn(&stk->client->dev,
			 "Power on=%d. enabled=%d\n", on, stk->power_enabled);
	}

	return rc;
}

static int stk8baxx_power_init(struct stk8baxx_data *stk, bool on)
{
	int rc;

	if (!on) {
		if (regulator_count_voltages(stk->vdd) > 0)
			regulator_set_voltage(stk->vdd, 0, STK8BAXX_VDD_MAX_UV);

		regulator_put(stk->vdd);

		if (regulator_count_voltages(stk->vio) > 0)
			regulator_set_voltage(stk->vio, 0, STK8BAXX_VIO_MAX_UV);

		regulator_put(stk->vio);
	} else {
		stk->vdd = regulator_get(&stk->client->dev, "vdd");
		if (IS_ERR(stk->vdd)) {
			rc = PTR_ERR(stk->vdd);
			dev_err(&stk->client->dev,
				"Regulator get failed vdd rc=%d\n", rc);
			return rc;
		}

		if (regulator_count_voltages(stk->vdd) > 0) {
			rc = regulator_set_voltage(stk->vdd,
						   STK8BAXX_VDD_MIN_UV,
						   STK8BAXX_VDD_MAX_UV);
			if (rc) {
				dev_err(&stk->client->dev,
					"Regulator set failed vdd rc=%d\n", rc);
				goto reg_vdd_put;
			}
		}

		stk->vio = regulator_get(&stk->client->dev, "vio");
		if (IS_ERR(stk->vio)) {
			rc = PTR_ERR(stk->vio);
			dev_err(&stk->client->dev,
				"Regulator get failed vio rc=%d\n", rc);
			goto reg_vdd_set;
		}

		if (regulator_count_voltages(stk->vio) > 0) {
			rc = regulator_set_voltage(stk->vio,
						   STK8BAXX_VIO_MIN_UV,
						   STK8BAXX_VIO_MAX_UV);
			if (rc) {
				dev_err(&stk->client->dev,
					"Regulator set failed vio rc=%d\n", rc);
				goto reg_vio_put;
			}
		}
	}

	return 0;

reg_vio_put:
	regulator_put(stk->vio);
reg_vdd_set:
	if (regulator_count_voltages(stk->vdd) > 0)
		regulator_set_voltage(stk->vdd, 0, STK8BAXX_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(stk->vdd);
	return rc;
}
#endif

static int stk8baxx_input_setup(struct stk8baxx_data *stk)
{
	int error;

	stk->input_dev = input_allocate_device();
	if (!stk->input_dev) {
		dev_err(&stk->client->dev, "%s:input_allocate_device failed\n",
			__func__);
		return -ENOMEM;
	}
	stk->input_dev->name = ACC_IDEVICE_NAME;
	stk->input_dev->id.bustype = BUS_I2C;

#if 1
	input_set_capability(stk->input_dev, EV_ABS, ABS_X);
	input_set_capability(stk->input_dev, EV_ABS, ABS_Y);
	input_set_capability(stk->input_dev, EV_ABS, ABS_Z);
#else
	input_set_capability(stk->input_dev, EV_REL, REL_X);
	input_set_capability(stk->input_dev, EV_REL, REL_Y);
	input_set_capability(stk->input_dev, EV_REL, REL_Z);
	input_set_capability(stk->input_dev, EV_REL, REL_DIAL);
	input_set_capability(stk->input_dev, EV_REL, REL_MISC);
#endif /* #ifdef STK_QUALCOMM_PLATFORM */

	error = input_register_device(stk->input_dev);
	if (error) {
		dev_err(&stk->client->dev,
			"%s:Unable to register input device: %s\n", __func__,
			stk->input_dev->name);
		return error;
	}
	input_set_drvdata(stk->input_dev, stk);
	return 0;
}

#ifdef CONFIG_OF
static int stk8baxx_parse_dt(struct device *dev,
			     struct stk8baxx_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	u32 temp_val;
	uint32_t int_flags;

	pdata->interrupt_pin = of_get_named_gpio_flags(np, "stk8baxx,irq-gpio",
						       0, &int_flags);
	if (pdata->interrupt_pin < 0) {
		dev_err(dev, "Unable to read irq-gpio\n");
	}

	rc = of_property_read_u32(np, "stk,direction", &temp_val);
	if (!rc)
		pdata->direction = temp_val;
	else {
		dev_err(dev, "Unable to read stk,direction\n");
		return rc;
	}

	return 0;
}
#else
static int stk8baxx_parse_dt(struct device *dev,
			     struct stk8baxx_platform_data *pdata)
{
	return -ENODEV;
}
#endif /* !CONFIG_OF */

static int stk8baxx_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	int error = 0;
	struct stk8baxx_data *stk;
	struct stk8baxx_platform_data *stk_platdata;
	struct class *gsensor_class;
	struct device *gsensor_cmd_dev;

	dev_info(&client->dev, "stk8baxx_probe: driver version:%s\n",
		 STK_ACC_DRIVER_VERSION);

	if (!i2c_check_functionality
	    (client->adapter,
	     I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_I2C_BLOCK)) {
		error = i2c_get_functionality(client->adapter);
		dev_err(&client->dev,
			"%s:i2c_check_functionality error, functionality=0x%x\n",
			__func__, error);
		error = -ENODEV;
		goto exit_i2c_check_functionality_error;
	}

	stk = kzalloc(sizeof(struct stk8baxx_data), GFP_KERNEL);
	if (!stk) {
		dev_err(&client->dev, "%s:memory allocation error\n", __func__);
		error = -ENOMEM;
		goto exit_kzalloc_error;
	}

	stk->client = client;
	i2c_set_clientdata(client, stk);

	if (client->dev.of_node) {
		dev_info(&stk->client->dev, "%s: probe with device tree\n",
			 __func__);
		stk_platdata =
		    devm_kzalloc(&client->dev,
				 sizeof(struct stk8baxx_platform_data),
				 GFP_KERNEL);
		if (!stk_platdata) {
			dev_err(&stk->client->dev,
				"Failed to allocate memory\n");
			goto exit_kzalloc_error;
		}

		error = stk8baxx_parse_dt(&client->dev, stk_platdata);
		if (error) {
			dev_err(&stk->client->dev,
				"%s: stk8baxx_parse_dt ret=%d\n", __func__,
				error);
			goto exit_kzalloc_error;
		}
	} else {
		if (client->dev.platform_data != NULL) {
			dev_info(&stk->client->dev,
				 "%s: probe with platform data\n", __func__);
			stk_platdata = client->dev.platform_data;
		} else {
			dev_info(&stk->client->dev,
				 "%s: probe with platform data in driver\n",
				 __func__);
			stk_platdata = &stk8baxx_plat_data;
		}
	}
	if (!stk_platdata) {
		dev_err(&stk->client->dev, "%s: no platform data!\n", __func__);
		goto exit_kzalloc_error;
	}
	stk->int_pin = stk_platdata->interrupt_pin;
	stk->stk8baxx_placement = stk_platdata->direction;
	stk8baxx_data_ptr = stk;

	mutex_init(&stk->write_lock);

#ifdef STK_QUALCOMM_PLATFORM
	stk->power_enabled = false;
	error = stk8baxx_power_init(stk, true);
	if (error) {
		dev_err(&stk->client->dev, "%s:power_init failed\n", __func__);
		goto exit_stk_power_init_error;
	}

	error = stk8baxx_power_on(stk, true);
	if (error) {
		dev_err(&stk->client->dev, "%s:power_on failed\n", __func__);
		goto exit_stk_power_on_error;
	}
#endif

	stk->stk_mems_work_queue = create_singlethread_workqueue("stk_mems_wq");
	if (stk->stk_mems_work_queue) {
		INIT_WORK(&stk->stk_work, stk_mems_wq_function);
	} else {
		dev_err(&stk->client->dev,
			"%s:create_singlethread_workqueue error\n", __func__);
		error = -EPERM;
		goto exit_create_workqueue_error;
	}
#if (!STK_ACC_POLLING_MODE)
	error = stk8baxx_irq_setup(client, stk, stk_platdata->interrupt_pin);
	if (!error)
		goto exit_irq_setup_error;
#else
	hrtimer_init(&stk->acc_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	stk->poll_delay =
	    ns_to_ktime(STK8BAXX_SAMPLE_TIME
			[STK8BAXX_INIT_ODR -
			 STK8BAXX_SPTIME_BASE] * NSEC_PER_USEC);
	stk->acc_timer.function = stk_acc_timer_func;
#endif

	stk8baxx_parameters_boot_init(stk);
	error = stk8baxx_reg_init(stk, client);
	if (error) {
		dev_err(&stk->client->dev,
			"%s:stk8baxx initialization failed\n", __func__);
		goto exit_stk_init_error;
	}
	stk->re_enable = false;
#ifdef STK_CHECK_CODE
	stk8baxx_check_init();
#endif

	error = stk8baxx_input_setup(stk);
	if (error) {
		dev_err(&stk->client->dev, "%s: stk8baxx_input_setup failed\n",
			__func__);
		goto exit_stk_init_error;
	}

	error =
	    sysfs_create_group(&stk->input_dev->dev.kobj,
			       &stk8baxx_attribute_group);
	if (error) {
		dev_err(&stk->client->dev, "%s: sysfs_create_group failed\n",
			__func__);
		goto exit_sysfs_create_group_error;
	}

	error = misc_register(&stk_device);
	if (error) {
		dev_err(&stk->client->dev, "%s: misc_register failed\n",
			__func__);
		goto exit_misc_device_register_error;
	}
#ifdef STK_QUALCOMM_PLATFORM
	stk->cdev = sensors_cdev;
	stk->cdev.sensors_enable = stk8baxx_enable_set;
	stk->cdev.sensors_poll_delay = stk8baxx_poll_delay_set;
#ifdef STK_QUALCOMM_MANUAL_CALI
	stk->cdev.sensors_calibrate = stk8baxx_do_calibration;
	stk->cdev.sensors_write_cal_params = stk8baxx_write_cal_params;
#endif /* #ifdef STK_QUALCOMM_MANUAL_CALI */
	stk->cdev.min_delay = STK8BAXX_SAMPLE_TIME[STK8BAXX_SPTIME_NO - 1];
	stk->cdev.max_delay = STK8BAXX_SAMPLE_TIME[0];
	stk->cdev.delay_msec =
	    STK8BAXX_SAMPLE_TIME[STK8BAXX_INIT_ODR -
				 STK8BAXX_SPTIME_BASE] / 1000;
	stk->cdev.version = 380;
	dev_info(&stk->client->dev, "%s: register qualcomm sensor classdev\n",
		 __func__);
	error = sensors_classdev_register(&stk->input_dev->dev, &stk->cdev);
	if (error) {
		dev_err(&stk->client->dev,
			"%s:class device create failed: %d\n", __func__, error);
		goto exit_misc_device_register_error;
	}
	stk8baxx_power_on(stk, false);
#endif /* #ifdef STK_QUALCOMM_PLATFORM */
#if 1
	gsensor_class = class_create(THIS_MODULE, "xr-gsensor");
	if (IS_ERR(gsensor_class))
		printk("Failed to create class(xr-gsensor)!\n");
	gsensor_cmd_dev = device_create(gsensor_class, NULL, 0, NULL, "device");
	if (IS_ERR(gsensor_cmd_dev))
		printk("Failed to create device(gsensor_cmd_dev)!\n");
	if (device_create_file(gsensor_cmd_dev, &dev_attr_gsensor) < 0) {
		printk("Failed to create device file(%s)!\n",
		       dev_attr_gsensor.attr.name);
	}
	if (device_create_file(gsensor_cmd_dev, &dev_attr_delay_acc) < 0) {
		printk("Failed to create device file(%s)!\n",
		       dev_attr_delay_acc.attr.name);
	}
#endif
#ifdef CONFIG_SPRD_PH_INFO
	memset((void *)SPRD_GsensorInfo, 0, 100);
	memcpy(SPRD_GsensorInfo, gsensor_Info, 100);
#endif
	dev_info(&stk->client->dev, "%s successfully\n", __func__);
	return 0;

exit_misc_device_register_error:
	sysfs_remove_group(&stk->input_dev->dev.kobj,
			   &stk8baxx_attribute_group);
exit_sysfs_create_group_error:
	input_unregister_device(stk->input_dev);
exit_stk_init_error:
#if (!STK_ACC_POLLING_MODE)
	free_irq(client->irq, stk);
#if ADDITIONAL_GPIO_CFG
exit_irq_setup_error:
	gpio_free(stk->int_pin);
	cancel_work_sync(&stk->stk_work);
#endif
#else
	hrtimer_try_to_cancel(&stk->acc_timer);
#endif
	destroy_workqueue(stk->stk_mems_work_queue);
exit_create_workqueue_error:

	mutex_destroy(&stk->write_lock);
#ifdef STK_QUALCOMM_PLATFORM
	stk8baxx_power_on(stk, false);
exit_stk_power_on_error:
	stk8baxx_power_init(stk, false);
exit_stk_power_init_error:
#endif
	kfree(stk);
	stk = NULL;
exit_kzalloc_error:
exit_i2c_check_functionality_error:
	return error;
}

static int stk8baxx_remove(struct i2c_client *client)
{
	struct stk8baxx_data *stk = i2c_get_clientdata(client);
#ifdef STK_QUALCOMM_PLATFORM
	sensors_classdev_unregister(&stk->cdev);
#endif
	misc_deregister(&stk_device);
	sysfs_remove_group(&stk->input_dev->dev.kobj,
			   &stk8baxx_attribute_group);
	input_unregister_device(stk->input_dev);
#if (!STK_ACC_POLLING_MODE)
	free_irq(client->irq, stk);
#if ADDITIONAL_GPIO_CFG
	gpio_free(stk->int_pin);
#endif
#else
	hrtimer_try_to_cancel(&stk->acc_timer);
#endif
	cancel_work_sync(&stk->stk_work);
	if (stk->stk_mems_work_queue)
		destroy_workqueue(stk->stk_mems_work_queue);

	mutex_destroy(&stk->write_lock);
#ifdef STK_QUALCOMM_PLATFORM
	stk8baxx_power_on(stk, false);
	stk8baxx_power_init(stk, false);
#endif
	kfree(stk);
	stk = NULL;
	return 0;
}

#if (defined(STK_INTEL_ACPI_PLATFORM) || defined(STK_ALLWINNER_PLATFORM))
int stk8baxx_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, "STK8BAXX");
	return 0;
}
#endif

#if 0
static int stk8baxx_suspend(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct stk8baxx_data *stk = i2c_get_clientdata(client);

	if (atomic_read(&stk->enabled)) {

		stk8baxx_set_enable(stk, 0);
		stk->re_enable = true;
	}
	return 0;
}

static int stk8baxx_resume(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct stk8baxx_data *stk = i2c_get_clientdata(client);
#ifdef STK_RESUME_RE_INIT
	int error;

	if (atomic_read(&stk->enabled))
		stk->re_enable = true;

	error = stk8baxx_reg_init(stk, client);
	if (error) {
		dev_err(&stk->client->dev,
			"%s:stk8baxx initialization failed\n", __func__);
	}
	stk->first_enable = true;
#endif

	if (stk->re_enable) {
		stk->re_enable = false;

		stk8baxx_set_enable(stk, 1);

	}
	return 0;
}

static const struct dev_pm_ops stk8baxx_pm_ops = {
	.suspend = stk8baxx_suspend,
	.resume = stk8baxx_resume,
};

#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_OF
static struct of_device_id stk8baxx_match_table[] = {

	{.compatible = "stk,stk8baxx",},
	{},
};
#endif

#ifdef CONFIG_ACPI
static const struct acpi_device_id stk8baxx_acpi_id[] = {
	{"STK8BA50", 0},
	{"STK8BAXX", 0},
	{}
};

MODULE_DEVICE_TABLE(acpi, stk8baxx_acpi_id);
#endif

unsigned short stk8baxx_add_list[] = { 0x18 };

static const struct i2c_device_id stk8bxx_i2c_id[] = {
	{STK8BAXX_I2C_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, stk8bxx_i2c_id);

static struct i2c_driver stk8baxx_i2c_driver = {
#if (defined(STK_ALLWINNER_PLATFORM) || defined(STK_INTEL_ACPI_PLATFORM))
	.class = I2C_CLASS_HWMON,
#endif
	.probe = stk8baxx_probe,
	.remove = stk8baxx_remove,
	.id_table = stk8bxx_i2c_id,
	.driver = {
		   .name = STK8BAXX_I2C_NAME,
#ifdef CONFIG_PM_SLEEP

#endif
#ifdef CONFIG_ACPI
		   .acpi_match_table = ACPI_PTR(stk8baxx_acpi_id),
#endif
#ifdef CONFIG_OF
		   .of_match_table = stk8baxx_match_table,
#endif

		   },
#ifdef STK_INTEL_ACPI_PLATFORM
	.detect = stk8baxx_detect,
	.address_list = stk8baxx_add_list,
#endif
#ifdef STK_ALLWINNER_PLATFORM
	.detect = stk8baxx_detect,
	.address_list = u_i2c_addr.normal_i2c,
#endif
};

#ifdef STK_INTEL_ACPI_PLATFORM
module_i2c_driver(stk8baxx_i2c_driver);
#else

static int __init stk8baxx_init(void)
{
	int ret = -1;

	printk("%s: start\n", __func__);
#ifdef STK_ALLWINNER_PLATFORM

	if (gsensor_fetch_sysconfig_para()) {
		printk("%s: err.\n", __func__);
		return -1;
	}

	printk
	    ("%s: after fetch_sysconfig_para:  normal_i2c: 0x%hx. normal_i2c[1]: 0x%hx \n",
	     __func__, u_i2c_addr.normal_i2c[0], u_i2c_addr.normal_i2c[1]);

	stk8baxx_i2c_driver.detect = gsensor_detect;
#endif

	ret = i2c_add_driver(&stk8baxx_i2c_driver);
#ifdef STK8BAXX_PERMISSION_THREAD
	STKPermissionThread =
	    kthread_run(stk8baxx_permis_thread, "stk", "Permissionthread");
	if (IS_ERR(STKPermissionThread))
		STKPermissionThread = NULL;
#endif
	return ret;
}

static void __exit stk8baxx_exit(void)
{
	i2c_del_driver(&stk8baxx_i2c_driver);
#ifdef STK8BAXX_PERMISSION_THREAD
	if (STKPermissionThread)
		STKPermissionThread = NULL;
#endif
}

module_init(stk8baxx_init);
module_exit(stk8baxx_exit);
#endif

MODULE_AUTHOR("Lex Hsieh, Sensortek");
MODULE_DESCRIPTION("stk8baxx 3-Axis accelerometer driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(STK_ACC_DRIVER_VERSION);
