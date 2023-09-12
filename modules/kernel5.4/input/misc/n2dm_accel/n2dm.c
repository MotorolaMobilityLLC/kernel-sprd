/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
 *
 * File Name          : n2dm_acc.c
 * Authors            : MSH - Motion Mems BU - Application Team
 *		      : Carmine Iascone (carmine.iascone@st.com)
 *		      : Matteo Dameno (matteo.dameno@st.com)
 * Version            : V.1.0.5
 * Date               : 16/08/2010
 * Description        : N2DM accelerometer sensor API
 *
 *******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
 * PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
 * AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
 * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
 * CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
 * INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * THIS SOFTWARE IS SPECIFICALLY DESIGNED FOR EXCLUSIVE USE WITH ST PARTS.
 *
 ******************************************************************************
 Revision 1.0.0 05/11/09
 First Release
 Revision 1.0.3 22/01/2010
  Linux K&R Compliant Release;
 Revision 1.0.5 16/08/2010
 modified _get_acceleration_data function
 modified _update_odr function
 manages 2 interrupts

 ******************************************************************************/

#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/workqueue.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "n2dm.h"

#define	INTERRUPT_MANAGEMENT 1

#define	G_MAX		16000	/** Maximum polled-device-reported g value */

/*
#define	SHIFT_ADJ_2G		4
#define	SHIFT_ADJ_4G		3
#define	SHIFT_ADJ_8G		2
#define	SHIFT_ADJ_16G		1
*/
#if 0
#define SENSITIVITY_2G		0.976//1	/**	mg/LSB	*/
#define SENSITIVITY_4G		1.953//	2	/**	mg/LSB	*/
#define SENSITIVITY_8G		3.906//	4	/**	mg/LSB	*/
#define SENSITIVITY_16G		11.718//12	/**	mg/LSB	*/
#else
#define SENSITIVITY_2G		1	/**	mg/LSB	*/
#define SENSITIVITY_4G		2	/**	mg/LSB	*/
#define SENSITIVITY_8G		4	/**	mg/LSB	*/
#define SENSITIVITY_16G		12	/**	mg/LSB	*/
#endif

#define	HIGH_RESOLUTION		0x08

#define	AXISDATA_REG		0x28
#define WHOAMI_N2DM_ACC	0x33	/*      Expctd content for WAI  */

/*	CONTROL REGISTERS	*/
#define WHO_AM_I		0x0F	/*      WhoAmI register         */
#define	TEMP_CFG_REG		0x1F	/*      temper sens control reg */
/* ctrl 1: ODR3 ODR2 ODR ODR0 LPen Zenable Yenable Zenable */
#define	CTRL_REG1		0x20	/*      control reg 1           */
#define	CTRL_REG2		0x21	/*      control reg 2           */
#define	CTRL_REG3		0x22	/*      control reg 3           */
#define	CTRL_REG4		0x23	/*      control reg 4           */
#define	CTRL_REG5		0x24	/*      control reg 5           */
#define	CTRL_REG6		0x25	/*      control reg 6           */

#define	FIFO_CTRL_REG		0x2E	/*      FiFo control reg        */

#define	INT_CFG1		0x30	/*      interrupt 1 config      */
#define	INT_SRC1		0x31	/*      interrupt 1 source      */
#define	INT_THS1		0x32	/*      interrupt 1 threshold   */
#define	INT_DUR1		0x33	/*      interrupt 1 duration    */

#define	INT_CFG2		0x34	/*      interrupt 2 config      */
#define	INT_SRC2		0x35	/*      interrupt 2 source      */
#define	INT_THS2		0x36	/*      interrupt 2 threshold   */
#define	INT_DUR2		0x37	/*      interrupt 2 duration    */

#define	TT_CFG			0x38	/*      tap config              */
#define	TT_SRC			0x39	/*      tap source              */
#define	TT_THS			0x3A	/*      tap threshold           */
#define	TT_LIM			0x3B	/*      tap time limit          */
#define	TT_TLAT			0x3C	/*      tap time latency        */
#define	TT_TW			0x3D	/*      tap time window         */
/*	end CONTROL REGISTRES	*/

#define ENABLE_HIGH_RESOLUTION	1

#define N2DM_ACC_PM_OFF		0x00
#define N2DM_ACC_ENABLE_ALL_AXES	0x57

#define PMODE_MASK			0x08
#define ODR_MASK			0XF0

#define ODR1		0x10	/* 1Hz output data rate */
#define ODR10		0x20	/* 10Hz output data rate */
#define ODR25		0x30	/* 25Hz output data rate */
#define ODR50		0x40	/* 50Hz output data rate */
#define ODR100		0x50	/* 100Hz output data rate */
#define ODR200		0x60	/* 200Hz output data rate */
#define ODR400		0x70	/* 400Hz output data rate */
#define ODR1250		0x90	/* 1250Hz output data rate */

#define	IA			0x40
#define	ZH			0x20
#define	ZL			0x10
#define	YH			0x08
#define	YL			0x04
#define	XH			0x02
#define	XL			0x01
/* */
/* CTRL REG BITS*/
#define	CTRL_REG3_I1_AOI1	0x40
#define	CTRL_REG6_I2_TAPEN	0x80
#define	CTRL_REG6_HLACTIVE	0x02
/* */

/* TAP_SOURCE_REG BIT */
#define	DTAP			0x20
#define	STAP			0x10
#define	SIGNTAP			0x08
#define	ZTAP			0x04
#define	YTAP			0x02
#define	XTAZ			0x01

#define	FUZZ			32
#define	FLAT			32
#define	I2C_RETRY_DELAY		5
#define	I2C_RETRIES		5
#define	I2C_AUTO_INCREMENT	0x80

/* RESUME STATE INDICES */
#define	RES_CTRL_REG1		0
#define	RES_CTRL_REG2		1
#define	RES_CTRL_REG3		2
#define	RES_CTRL_REG4		3
#define	RES_CTRL_REG5		4
#define	RES_CTRL_REG6		5

#define	RES_INT_CFG1		6
#define	RES_INT_THS1		7
#define	RES_INT_DUR1		8
#define	RES_INT_CFG2		9
#define	RES_INT_THS2		10
#define	RES_INT_DUR2		11

#define	RES_TT_CFG		12
#define	RES_TT_THS		13
#define	RES_TT_LIM		14
#define	RES_TT_TLAT		15
#define	RES_TT_TW		16

#define	RES_TEMP_CFG_REG	17
#define	RES_REFERENCE_REG	18
#define	RES_FIFO_CTRL_REG	19

#define	RESUME_ENTRIES		20
#define DEVICE_INFO             "ST, N2DM"
#define DEVICE_INFO_LEN         32
/* end RESUME STATE INDICES */

#define GSENSOR_GINT1_GPI 0
#define GSENSOR_GINT2_GPI 1

#ifdef CONFIG_SPRD_PH_INFO
/*set gsensor info*/
extern	char	SPRD_GsensorInfo[];
static char	gsensor_Info[100] = "ST N2DM 3-axis Accelerometer";
#endif

struct {
	unsigned int cutoff_ms;
	unsigned int mask;
} n2dm_acc_odr_table[] = {
	{1, ODR1250},
	{3, ODR400},
	{5, ODR200},
	{10, ODR100},
	{20, ODR50},
	{40, ODR25},
	{100, ODR10},
	{1000, ODR1},
};

struct n2dm_acc_data {
	struct i2c_client *client;
	struct n2dm_acc_platform_data *pdata;

	struct mutex lock;
	struct delayed_work input_work;

	struct input_dev *input_dev;

	int hw_initialized;
	/* hw_working=-1 means not tested yet */
	int hw_working;
	atomic_t enabled;
	int on_before_suspend;

	u8 sensitivity;

	u8 resume_state[RESUME_ENTRIES];

	int irq1;
	struct work_struct irq1_work;
	struct workqueue_struct *irq1_work_queue;
	int irq2;
	struct work_struct irq2_work;
	struct workqueue_struct *irq2_work_queue;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

/*
 * Because misc devices can not carry a pointer from driver register to
 * open, we keep this global.  This limits the driver to a single instance.
 */
struct n2dm_acc_data *n2dm_acc_misc_data;
struct i2c_client *n2dm_i2c_client;

static int n2dm_acc_i2c_read(struct n2dm_acc_data *acc, u8 *buf, int len)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
			.addr = acc->client->addr,
			.flags = acc->client->flags & I2C_M_TEN,
			.len = 1,
			.buf = buf,},
		{
			.addr = acc->client->addr,
			.flags = (acc->client->flags & I2C_M_TEN) | I2C_M_RD,
			.len = len,
			.buf = buf,},
	};

	do {
		err = i2c_transfer(acc->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		dev_err(&acc->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int n2dm_acc_i2c_write(struct n2dm_acc_data *acc, u8 *buf, int len)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = { {.addr = acc->client->addr,
		.flags = acc->client->flags & I2C_M_TEN,
		.len = len + 1,
		.buf = buf,},
	};
	do {
		err = i2c_transfer(acc->client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&acc->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int n2dm_acc_hw_init(struct n2dm_acc_data *acc)
{
	int err = -1;
	u8 buf[7];

	N2DM_FUN();
	buf[0] = WHO_AM_I;
	err = n2dm_acc_i2c_read(acc, buf, 1);
	if (err < 0)
		goto error_firstread;
	else
		acc->hw_working = 1;
	if (buf[0] != WHOAMI_N2DM_ACC) {
		err = -1;	/* choose the right coded error */
		goto error_unknown_device;
	}

	buf[0] = CTRL_REG1;
	buf[1] = acc->resume_state[RES_CTRL_REG1];
	err = n2dm_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto error1;

	buf[0] = TEMP_CFG_REG;
	buf[1] = acc->resume_state[RES_TEMP_CFG_REG];
	err = n2dm_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto error1;

	buf[0] = FIFO_CTRL_REG;
	buf[1] = acc->resume_state[RES_FIFO_CTRL_REG];
	err = n2dm_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto error1;

	buf[0] = (I2C_AUTO_INCREMENT | TT_THS);
	buf[1] = acc->resume_state[RES_TT_THS];
	buf[2] = acc->resume_state[RES_TT_LIM];
	buf[3] = acc->resume_state[RES_TT_TLAT];
	buf[4] = acc->resume_state[RES_TT_TW];
	err = n2dm_acc_i2c_write(acc, buf, 4);
	if (err < 0)
		goto error1;
	buf[0] = TT_CFG;
	buf[1] = acc->resume_state[RES_TT_CFG];
	err = n2dm_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto error1;

	buf[0] = (I2C_AUTO_INCREMENT | INT_THS1);
	buf[1] = acc->resume_state[RES_INT_THS1];
	buf[2] = acc->resume_state[RES_INT_DUR1];
	err = n2dm_acc_i2c_write(acc, buf, 2);
	if (err < 0)
		goto error1;
	buf[0] = INT_CFG1;
	buf[1] = acc->resume_state[RES_INT_CFG1];
	err = n2dm_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto error1;

	buf[0] = (I2C_AUTO_INCREMENT | INT_THS2);
	buf[1] = acc->resume_state[RES_INT_THS2];
	buf[2] = acc->resume_state[RES_INT_DUR2];
	err = n2dm_acc_i2c_write(acc, buf, 2);
	if (err < 0)
		goto error1;
	buf[0] = INT_CFG2;
	buf[1] = acc->resume_state[RES_INT_CFG2];
	err = n2dm_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto error1;

	buf[0] = (I2C_AUTO_INCREMENT | CTRL_REG2);
	buf[1] = acc->resume_state[RES_CTRL_REG2];
	buf[2] = acc->resume_state[RES_CTRL_REG3];
	buf[3] = acc->resume_state[RES_CTRL_REG4];
	buf[4] = acc->resume_state[RES_CTRL_REG5];
	buf[5] = acc->resume_state[RES_CTRL_REG6];
	err = n2dm_acc_i2c_write(acc, buf, 5);
	if (err < 0)
		goto error1;

	acc->hw_initialized = 1;
	return 0;

error_firstread:
	acc->hw_working = 0;
	dev_warn(&acc->client->dev,
		 "Error reading WHO_AM_I: is device available/working?\n");
	goto error1;
error_unknown_device:
	dev_err(&acc->client->dev,
		"device unknown. Expected: 0x%x, Replies: 0x%x\n",
		WHOAMI_N2DM_ACC, buf[0]);
error1:
	acc->hw_initialized = 0;
	dev_err(&acc->client->dev, "hw init error 0x%x,0x%x: %d\n", buf[0],
			buf[1], err);
	return err;
}

static void n2dm_acc_device_power_off(struct n2dm_acc_data *acc)
{
	int err;
	u8 buf[2] = { CTRL_REG1, N2DM_ACC_PM_OFF };

	err = n2dm_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		dev_err(&acc->client->dev, "soft power off failed: %d\n", err);

	if (acc->pdata->power_off) {
		if (acc->irq1 != 0)
			disable_irq_nosync(acc->irq1);
		if (acc->irq2 != 0)
			disable_irq_nosync(acc->irq2);
		acc->pdata->power_off();
		acc->hw_initialized = 0;
	}
	if (acc->hw_initialized) {
		if (acc->irq1 != 0)
			disable_irq_nosync(acc->irq1);
		if (acc->irq2 != 0)
			disable_irq_nosync(acc->irq2);
		acc->hw_initialized = 0;
	}
}

static int n2dm_acc_device_power_on(struct n2dm_acc_data *acc)
{
	int err = -1;

	if (acc->pdata->power_on) {
		err = acc->pdata->power_on();
		if (err < 0) {
			dev_err(&acc->client->dev,
					"power_on failed: %d\n", err);
			return err;
		}
	}

	if (!acc->hw_initialized) {
		err = n2dm_acc_hw_init(acc);
		if (acc->hw_working == 1 && err < 0) {
			n2dm_acc_device_power_off(acc);
			return err;
		}
	}
	return 0;
}

static irqreturn_t n2dm_acc_isr1(int irq, void *dev)
{
	struct n2dm_acc_data *acc = dev;

	disable_irq_nosync(irq);
	queue_work(acc->irq1_work_queue, &acc->irq1_work);

	return IRQ_HANDLED;
}

static irqreturn_t n2dm_acc_isr2(int irq, void *dev)
{
	struct n2dm_acc_data *acc = dev;

	disable_irq_nosync(irq);
	queue_work(acc->irq2_work_queue, &acc->irq2_work);

	return IRQ_HANDLED;
}

static void n2dm_acc_irq1_work_func(struct work_struct *work)
{
	N2DM_FUN();
}

static void n2dm_acc_irq2_work_func(struct work_struct *work)
{
	N2DM_FUN();
}

int n2dm_acc_update_g_range(struct n2dm_acc_data *acc, u8 new_g_range)
{
	int err;

	u8 sensitivity;
	u8 buf[2];
	u8 updated_val;
	u8 init_val;
	u8 new_val;
	u8 mask = N2DM_ACC_FS_MASK | HIGH_RESOLUTION;

	switch (new_g_range) {
	case N2DM_ACC_G_2G:
		sensitivity = SENSITIVITY_2G;
		break;
	case N2DM_ACC_G_4G:
		sensitivity = SENSITIVITY_4G;
		break;
	case N2DM_ACC_G_8G:
		sensitivity = SENSITIVITY_8G;
		break;
	case N2DM_ACC_G_16G:
		sensitivity = SENSITIVITY_16G;
		break;
	default:
		dev_err(&acc->client->dev, "invalid g range requested: %u\n",
				new_g_range);
		return -EINVAL;
	}

	if (atomic_read(&acc->enabled)) {
		/* Set configuration register 4, which contains g range setting
		 *  NOTE: this is a straight overwrite because this driver does
		 *  not use any of the other configuration bits in this
		 *  register.  Should this become untrue, we will have to read
		 *  out the value and only change the relevant bits --XX----
		 *  (marked by X)
		 */
		buf[0] = CTRL_REG4;
		err = n2dm_acc_i2c_read(acc, buf, 1);
		if (err < 0)
			goto error;
		init_val = buf[0];
		acc->resume_state[RES_CTRL_REG4] = init_val;
		new_val = new_g_range | HIGH_RESOLUTION;
		updated_val = ((mask & new_val) | ((~mask) & init_val));
		buf[1] = updated_val;
		buf[0] = CTRL_REG4;
		err = n2dm_acc_i2c_write(acc, buf, 1);
		if (err < 0)
			goto error;
		acc->resume_state[RES_CTRL_REG4] = updated_val;
		acc->sensitivity = sensitivity;
		N2DM_LOG("sensitivity %d g-range %d\n",
			 sensitivity, new_g_range);
	}

	return 0;
error:
	dev_err(&acc->client->dev, "update g range failed 0x%x,0x%x: %d\n",
			buf[0], buf[1], err);

	return err;
}

int n2dm_acc_update_odr(struct n2dm_acc_data *acc, int poll_interval_ms)
{
	int err = -1;
	int i;
	u8 config[2];

	/* Convert the poll interval into an output data rate configuration
	 *  that is as low as possible.  The ordering of these checks must be
	 *  maintained due to the cascading cut off values - poll intervals are
	 *  checked from shortest to longest.  At each check, if the next lower
	 *  ODR cannot support the current poll interval, we stop searching
	 */
	for (i = ARRAY_SIZE(n2dm_acc_odr_table) - 1; i >= 0; i--) {
		if (n2dm_acc_odr_table[i].cutoff_ms <= poll_interval_ms)
			break;
	}
	config[1] = n2dm_acc_odr_table[i].mask;

	config[1] |= N2DM_ACC_ENABLE_ALL_AXES;

	/* If device is currently enabled, we need to write new
	 *  configuration out to it
	 */
	if (atomic_read(&acc->enabled)) {
		config[0] = CTRL_REG1;
		err = n2dm_acc_i2c_write(acc, config, 1);
		if (err < 0)
			goto error;
		acc->resume_state[RES_CTRL_REG1] = config[1];
	}

	return 0;

error:
	dev_err(&acc->client->dev, "update odr failed 0x%x,0x%x: %d\n",
			config[0], config[1], err);

	return err;
}

static int n2dm_acc_register_write(struct n2dm_acc_data *acc, u8 *buf,
		u8 reg_address, u8 new_value)
{
	int err = -1;

	if (atomic_read(&acc->enabled)) {
		/* Sets configuration register at reg_address
		 *  NOTE: this is a straight overwrite
		 */
		buf[0] = reg_address;
		buf[1] = new_value;
		err = n2dm_acc_i2c_write(acc, buf, 1);
		if (err < 0)
			return err;
	}
	return err;
}

static int n2dm_acc_register_read(struct n2dm_acc_data *acc, u8 *buf,
		u8 reg_address)
{

	int err = -1;

	buf[0] = (reg_address);
	err = n2dm_acc_i2c_read(acc, buf, 1);
	return err;
}

static int n2dm_acc_register_update(struct n2dm_acc_data *acc, u8 *buf,
		u8 reg_address, u8 mask,
		u8 new_bit_values)
{
	int err = -1;
	u8 init_val;
	u8 updated_val;

	err = n2dm_acc_register_read(acc, buf, reg_address);
	if (!(err < 0)) {
		init_val = buf[1];
		updated_val = ((mask & new_bit_values) | ((~mask) & init_val));
		err = n2dm_acc_register_write(acc, buf, reg_address,
				updated_val);
	}
	return err;
}

static int n2dm_acc_get_acceleration_data(struct n2dm_acc_data *acc,
		int *xyz)
{
	int err = -1;
	/* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	u8 acc_data[6];
	/* x,y,z hardware data */
	s16 hw_d[3] = { 0 };

	acc_data[0] = (I2C_AUTO_INCREMENT | AXISDATA_REG);
	err = n2dm_acc_i2c_read(acc, acc_data, 6);

	if (err < 0) {
		N2DM_ERR("%s I2C read error %d\n", N2DM_ACC_I2C_NAME,
		      err);
		return err;
	}

	hw_d[0] = (((s16) ((acc_data[1] << 8) | acc_data[0])) >> 4);
	hw_d[1] = (((s16) ((acc_data[3] << 8) | acc_data[2])) >> 4);
	hw_d[2] = (((s16) ((acc_data[5] << 8) | acc_data[4])) >> 4);
#if 0
	hw_d[0] = hw_d[0] * 976/1000;//acc->sensitivity;
	hw_d[1] = hw_d[1] * 976/1000;//acc->sensitivity;
	hw_d[2] = hw_d[2] * 976/1000;acc->sensitivity;
#else
	hw_d[0] = hw_d[0] * acc->sensitivity;
	hw_d[1] = hw_d[1] * acc->sensitivity;
	hw_d[2] = hw_d[2] * acc->sensitivity;
#endif

	xyz[0] = ((acc->pdata->negate_x) ? (-hw_d[acc->pdata->axis_map_x])
			: (hw_d[acc->pdata->axis_map_x]));
	xyz[1] = ((acc->pdata->negate_y) ? (-hw_d[acc->pdata->axis_map_y])
			: (hw_d[acc->pdata->axis_map_y]));
	xyz[2] = ((acc->pdata->negate_z) ? (-hw_d[acc->pdata->axis_map_z])
			: (hw_d[acc->pdata->axis_map_z]));

	return err;
}

static void n2dm_acc_report_values(struct n2dm_acc_data *acc, int *xyz)
{
	input_report_abs(acc->input_dev, ABS_X, xyz[0]);
	input_report_abs(acc->input_dev, ABS_Y, xyz[1]);
	input_report_abs(acc->input_dev, ABS_Z, xyz[2]);
	input_sync(acc->input_dev);
}

static int n2dm_acc_enable(struct n2dm_acc_data *acc)
{
	int err;

	N2DM_FUN();
	if (!atomic_cmpxchg(&acc->enabled, 0, 1)) {
		err = n2dm_acc_device_power_on(acc);
		if (err < 0) {
			atomic_set(&acc->enabled, 0);
			return err;
		}

		if (acc->hw_initialized) {
			if (acc->irq1 != 0)
				enable_irq(acc->irq1);
			if (acc->irq2 != 0)
				enable_irq(acc->irq2);
		}

		schedule_delayed_work(&acc->input_work,
				msecs_to_jiffies(acc->pdata->
					poll_interval));
	}
	return 0;
}

static int n2dm_acc_disable(struct n2dm_acc_data *acc)
{
	N2DM_FUN();
	if (atomic_cmpxchg(&acc->enabled, 1, 0)) {
		cancel_delayed_work_sync(&acc->input_work);
		n2dm_acc_device_power_off(acc);
	}

	return 0;
}

static int n2dm_acc_misc_open(struct inode *inode, struct file *file)
{
	int err;

	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;

	file->private_data = n2dm_acc_misc_data;

	return 0;
}

static long n2dm_acc_misc_ioctl_handler(struct file *file, unsigned int cmd,
		unsigned long arg, int compat_mode)
{
	void __user *argp = (void __user *)arg;
	u8 buf[4];
	u8 mask;
	u8 reg_address;
	u8 bit_values;
	int err;
	int interval;
	int xyz[3] = { 0 };
	struct n2dm_acc_data *acc = file->private_data;

	N2DM_LOG("cmd %d,compat_mode=%d\n", _IOC_NR(cmd), compat_mode);
	switch (cmd) {
	case N2DM_ACC_IOCTL_GET_DELAY:
		interval = acc->pdata->poll_interval;
		if (copy_to_user(argp, &interval, sizeof(interval)))
			return -EFAULT;
		break;

	case N2DM_ACC_IOCTL_SET_DELAY:
		if (copy_from_user(&interval, argp, sizeof(interval)))
			return -EFAULT;
		if (interval < 0 || interval > 1000)
			return -EINVAL;

		acc->pdata->poll_interval = max(interval,
						acc->pdata->min_interval);
		err = n2dm_acc_update_odr(acc, 2);
		/* TODO: if update fails poll is still set */
		if (err < 0)
			return err;
		break;

	case N2DM_ACC_IOCTL_SET_ENABLE:
		if (copy_from_user(&interval, argp, sizeof(interval)))
			return -EFAULT;
		if (interval > 1)
			return -EINVAL;
		if (interval)
			err = n2dm_acc_enable(acc);
		else
			err = n2dm_acc_disable(acc);
		return err;
		break;

	case N2DM_ACC_IOCTL_GET_ENABLE:
		interval = atomic_read(&acc->enabled);
		if (copy_to_user(argp, &interval, sizeof(interval)))
			return -EINVAL;
		break;

	case N2DM_ACC_IOCTL_SET_G_RANGE:
		if (copy_from_user(buf, argp, 1))
			return -EFAULT;
		bit_values = buf[0];
		err = n2dm_acc_update_g_range(acc, bit_values);
		if (err < 0)
			return err;
		break;

#ifdef INTERRUPT_MANAGEMENT
	case N2DM_ACC_IOCTL_SET_CTRL_REG3:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = CTRL_REG3;
		mask = buf[1];
		bit_values = buf[0];
		err = n2dm_acc_register_update(acc, (u8 *) arg, reg_address,
				mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_CTRL_REG3] = ((mask & bit_values) |
				(~mask & acc->
				 resume_state
				 [RES_CTRL_REG3]));
		break;

	case N2DM_ACC_IOCTL_SET_CTRL_REG6:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = CTRL_REG6;
		mask = buf[1];
		bit_values = buf[0];
		err = n2dm_acc_register_update(acc, (u8 *) arg, reg_address,
				mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_CTRL_REG6] = ((mask & bit_values) |
				(~mask & acc->
				 resume_state
				 [RES_CTRL_REG6]));
		break;

	case N2DM_ACC_IOCTL_SET_DURATION1:
		if (copy_from_user(buf, argp, 1))
			return -EFAULT;
		reg_address = INT_DUR1;
		mask = 0x7F;
		bit_values = buf[0];
		err = n2dm_acc_register_update(acc, (u8 *) arg, reg_address,
				mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_INT_DUR1] = ((mask & bit_values) |
				(~mask & acc->
				 resume_state
				 [RES_INT_DUR1]));
		break;

	case N2DM_ACC_IOCTL_SET_THRESHOLD1:
		if (copy_from_user(buf, argp, 1))
			return -EFAULT;
		reg_address = INT_THS1;
		mask = 0x7F;
		bit_values = buf[0];
		err = n2dm_acc_register_update(acc, (u8 *) arg, reg_address,
				mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_INT_THS1] = ((mask & bit_values) |
				(~mask & acc->
				 resume_state
				 [RES_INT_THS1]));
		break;

	case N2DM_ACC_IOCTL_SET_CONFIG1:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = INT_CFG1;
		mask = buf[1];
		bit_values = buf[0];
		err = n2dm_acc_register_update(acc, (u8 *) arg, reg_address,
				mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_INT_CFG1] = ((mask & bit_values) |
				(~mask & acc->
				 resume_state
				 [RES_INT_CFG1]));
		break;

	case N2DM_ACC_IOCTL_GET_SOURCE1:
		err = n2dm_acc_register_read(acc, buf, INT_SRC1);
		if (err < 0)
			return err;

		if (copy_to_user(argp, buf, 1))
			return -EINVAL;
		break;

	case N2DM_ACC_IOCTL_SET_DURATION2:
		if (copy_from_user(buf, argp, 1))
			return -EFAULT;
		reg_address = INT_DUR2;
		mask = 0x7F;
		bit_values = buf[0];
		err = n2dm_acc_register_update(acc, (u8 *) arg, reg_address,
				mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_INT_DUR2] = ((mask & bit_values) |
				(~mask & acc->
				 resume_state
				 [RES_INT_DUR2]));
		break;

	case N2DM_ACC_IOCTL_SET_THRESHOLD2:
		if (copy_from_user(buf, argp, 1))
			return -EFAULT;
		reg_address = INT_THS2;
		mask = 0x7F;
		bit_values = buf[0];
		err = n2dm_acc_register_update(acc, (u8 *) arg, reg_address,
				mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_INT_THS2] = ((mask & bit_values) |
				(~mask & acc->
				 resume_state
				 [RES_INT_THS2]));
		break;

	case N2DM_ACC_IOCTL_SET_CONFIG2:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = INT_CFG2;
		mask = buf[1];
		bit_values = buf[0];
		err = n2dm_acc_register_update(acc, (u8 *) arg, reg_address,
				mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_INT_CFG2] = ((mask & bit_values) |
				(~mask & acc->
				 resume_state
				 [RES_INT_CFG2]));
		break;

	case N2DM_ACC_IOCTL_GET_SOURCE2:
		err = n2dm_acc_register_read(acc, buf, INT_SRC2);
		if (err < 0)
			return err;

		if (copy_to_user(argp, buf, 1))
			return -EINVAL;
		break;

	case N2DM_ACC_IOCTL_GET_TAP_SOURCE:
		err = n2dm_acc_register_read(acc, buf, TT_SRC);
		if (err < 0)
			return err;

		if (copy_to_user(argp, buf, 1)) {
			pr_err("%s: %s error in copy_to_user\n",
					N2DM_ACC_DEV_NAME, __func__);
			return -EINVAL;
		}
		break;
	case N2DM_ACC_IOCTL_GET_COOR_XYZ:
		err = n2dm_acc_get_acceleration_data(acc, xyz);
		if (err < 0)
			return err;

		if (copy_to_user(argp, xyz, sizeof(xyz))) {
			pr_err(" %s %d error in copy_to_user\n",
					__func__, __LINE__);
			return -EINVAL;
		}
		break;
	case N2DM_ACC_IOCTL_SET_TAP_CFG:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = TT_CFG;
		mask = buf[1];
		bit_values = buf[0];
		err = n2dm_acc_register_update(acc, (u8 *) arg, reg_address,
				mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_TT_CFG] = ((mask & bit_values) |
				(~mask & acc->
				 resume_state[RES_TT_CFG]));
		break;

	case N2DM_ACC_IOCTL_SET_TAP_TLIM:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = TT_LIM;
		mask = buf[1];
		bit_values = buf[0];
		err = n2dm_acc_register_update(acc, (u8 *) arg, reg_address,
				mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_TT_LIM] = ((mask & bit_values) |
				(~mask & acc->
				 resume_state[RES_TT_LIM]));
		break;

	case N2DM_ACC_IOCTL_SET_TAP_THS:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = TT_THS;
		mask = buf[1];
		bit_values = buf[0];
		err = n2dm_acc_register_update(acc, (u8 *) arg, reg_address,
				mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_TT_THS] = ((mask & bit_values) |
				(~mask & acc->
				 resume_state[RES_TT_THS]));
		break;

	case N2DM_ACC_IOCTL_SET_TAP_TLAT:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = TT_TLAT;
		mask = buf[1];
		bit_values = buf[0];
		err = n2dm_acc_register_update(acc, (u8 *) arg, reg_address,
				mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_TT_TLAT] = ((mask & bit_values) |
				(~mask & acc->
				 resume_state[RES_TT_TLAT]));
		break;

	case N2DM_ACC_IOCTL_SET_TAP_TW:
		if (copy_from_user(buf, argp, 2))
			return -EFAULT;
		reg_address = TT_TW;
		mask = buf[1];
		bit_values = buf[0];
		err = n2dm_acc_register_update(acc, (u8 *) arg, reg_address,
				mask, bit_values);
		if (err < 0)
			return err;
		acc->resume_state[RES_TT_TW] = ((mask & bit_values) |
				(~mask & acc->
				 resume_state[RES_TT_TW]));
		break;

#endif /* INTERRUPT_MANAGEMENT */
	case N2DM_ACC_IOCTL_GET_CHIP_ID:
		{
			u8 devid = 0;
			u8 devinfo[DEVICE_INFO_LEN] = {0};

			err = n2dm_acc_register_read(acc, &devid, WHO_AM_I);
			if (err < 0) {
				N2DM_ERR("error read register WHO_AM_I\n");
				return -EAGAIN;
			}
			sprintf(devinfo, "%s, %#x", DEVICE_INFO, devid);

			if (copy_to_user(argp, devinfo, sizeof(devinfo))) {
				N2DM_ERR("error in copy_to_user(IOCTL_GET_CHIP_ID)\n");
				return -EINVAL;
			}
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static long n2dm_acc_misc_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	return n2dm_acc_misc_ioctl_handler(file, cmd, arg, 0);
}

#ifdef CONFIG_COMPAT
static long n2dm_acc_misc_ioctl_compat(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	return n2dm_acc_misc_ioctl_handler(file, cmd, (unsigned long )compat_ptr(arg), 1);
}
#endif
static const struct file_operations n2dm_acc_misc_fops = {
	.owner = THIS_MODULE,
	.open = n2dm_acc_misc_open,
	.unlocked_ioctl = n2dm_acc_misc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = n2dm_acc_misc_ioctl_compat,
#endif
};

static struct miscdevice n2dm_acc_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = N2DM_ACC_DEV_NAME,
	.fops = &n2dm_acc_misc_fops,
};
#if 1 //xr_gsensor
static ssize_t show_n2dm_delay(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct n2dm_acc_data *acc = n2dm_acc_misc_data;
	int interval = acc->pdata->poll_interval;

	return sprintf(buf, "n2dm %d\n", interval);
}

static ssize_t store_n2dm_delay(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct n2dm_acc_data *acc = n2dm_acc_misc_data;
	int interval = 0;

	if (kstrtoint(buf, 10, &interval) < 0
	    || interval < 0 || interval > 1000)
		return -EINVAL;

	acc->pdata->poll_interval = max(interval, acc->pdata->min_interval);
	n2dm_acc_update_odr(acc, acc->pdata->poll_interval);

	return size;
}

static DEVICE_ATTR(delay_acc, 0644, show_n2dm_delay, store_n2dm_delay);

static ssize_t show_n2dm_enable(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int enabled;
	struct n2dm_acc_data *acc = n2dm_acc_misc_data;

	enabled	= atomic_read(&acc->enabled);
	return sprintf(buf, "n2dm %d\n", enabled);
}

static ssize_t store_n2dm_enable(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	bool value;
	struct n2dm_acc_data *acc = n2dm_acc_misc_data;

	if (strtobool(buf, &value))
		return -EINVAL;
	if (value)
		n2dm_acc_enable(acc);
	else
		n2dm_acc_disable(acc);
	return size;
}

static DEVICE_ATTR(gsensor, 0644, show_n2dm_enable, store_n2dm_enable);
#endif
static void n2dm_acc_input_work_func(struct work_struct *work)
{
	struct n2dm_acc_data *acc;

	int xyz[3] = { 0 };
	int err;

	acc = container_of((struct delayed_work *)work,
			struct n2dm_acc_data, input_work);

	mutex_lock(&acc->lock);
	err = n2dm_acc_get_acceleration_data(acc, xyz);
	if (err < 0)
		dev_err(&acc->client->dev, "get_acceleration_data failed\n");
	else
		n2dm_acc_report_values(acc, xyz);

	schedule_delayed_work(&acc->input_work,
			msecs_to_jiffies(acc->pdata->poll_interval));
	mutex_unlock(&acc->lock);
}

#ifdef N2DM_OPEN_ENABLE
int n2dm_acc_input_open(struct input_dev *input)
{
	struct n2dm_acc_data *acc = input_get_drvdata(input);

	return n2dm_acc_enable(acc);
}

void n2dm_acc_input_close(struct input_dev *dev)
{
	struct n2dm_acc_data *acc = input_get_drvdata(dev);

	n2dm_acc_disable(acc);
}
#endif

static int n2dm_acc_validate_pdata(struct n2dm_acc_data *acc)
{
	acc->pdata->poll_interval = max(acc->pdata->poll_interval,
			acc->pdata->min_interval);

	if (acc->pdata->axis_map_x > 2 || acc->pdata->axis_map_y > 2
			|| acc->pdata->axis_map_z > 2) {
		dev_err(&acc->client->dev,
			"invalid axis_map value x:%u y:%u z%u\n",
			acc->pdata->axis_map_x,
			acc->pdata->axis_map_y,
			acc->pdata->axis_map_z);
		return -EINVAL;
	}

	/* Only allow 0 and 1 for negation boolean flag */
	if (acc->pdata->negate_x > 1 || acc->pdata->negate_y > 1
			|| acc->pdata->negate_z > 1) {
		dev_err(&acc->client->dev,
			"invalid negate value x:%u y:%u z:%u\n",
			acc->pdata->negate_x,
			acc->pdata->negate_y,
			acc->pdata->negate_z);
		return -EINVAL;
	}

	/* Enforce minimum polling interval */
	if (acc->pdata->poll_interval < acc->pdata->min_interval) {
		dev_err(&acc->client->dev, "minimum poll interval violated\n");
		return -EINVAL;
	}

	return 0;
}

static int n2dm_acc_input_init(struct n2dm_acc_data *acc)
{
	int err;
	/* Polling rx data when the interrupt is not used.*/
	if (1 /*acc->irq1 == 0 && acc->irq1 == 0 */ ) {
		INIT_DELAYED_WORK(&acc->input_work, n2dm_acc_input_work_func);
	}

	acc->input_dev = input_allocate_device();
	if (!acc->input_dev) {
		err = -ENOMEM;
		dev_err(&acc->client->dev, "input device allocate failed\n");
		goto err0;
	}
#ifdef N2DM_ACC_OPEN_ENABLE
	acc->input_dev->open = n2dm_acc_input_open;
	acc->input_dev->close = n2dm_acc_input_close;
#endif

	input_set_drvdata(acc->input_dev, acc);

	set_bit(EV_ABS, acc->input_dev->evbit);
	/*      next is used for interruptA sources data if the case */
	set_bit(ABS_MISC, acc->input_dev->absbit);
	/*      next is used for interruptB sources data if the case */
	set_bit(ABS_WHEEL, acc->input_dev->absbit);

	input_set_abs_params(acc->input_dev, ABS_X, -G_MAX, G_MAX, 0, 0);
	input_set_abs_params(acc->input_dev, ABS_Y, -G_MAX, G_MAX, 0, 0);
	input_set_abs_params(acc->input_dev, ABS_Z, -G_MAX, G_MAX, 0, 0);
	/*      next is used for interruptA sources data if the case */
	input_set_abs_params(acc->input_dev, ABS_MISC, INT_MIN, INT_MAX, 0, 0);
	/*      next is used for interruptB sources data if the case */
	input_set_abs_params(acc->input_dev, ABS_WHEEL, INT_MIN, INT_MAX, 0, 0);

	acc->input_dev->name = "accelerometer";

	err = input_register_device(acc->input_dev);
	if (err) {
		dev_err(&acc->client->dev,
			"unable to register input polled device %s\n",
			acc->input_dev->name);
		goto err1;
	}

	return 0;

err1:
	input_free_device(acc->input_dev);
err0:
	return err;
}

static void n2dm_acc_input_cleanup(struct n2dm_acc_data *acc)
{
	input_unregister_device(acc->input_dev);
	input_free_device(acc->input_dev);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void n2dm_early_suspend(struct early_suspend *es);
static void n2dm_early_resume(struct early_suspend *es);
#endif

#ifdef CONFIG_OF
static struct n2dm_acc_platform_data *n2dm_acc_parse_dt(struct device *dev)
{
	struct n2dm_acc_platform_data *pdata;
	struct device_node *np = dev->of_node;
	int ret;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	ret = of_property_read_u32(np, "poll_interval", &pdata->poll_interval);
	if (ret) {
		dev_err(dev, "fail to get poll_interval\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "min_interval", &pdata->min_interval);
	if (ret) {
		dev_err(dev, "fail to get min_interval\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "g_range", &pdata->g_range);
	if (ret) {
		dev_err(dev, "fail to get g_range\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "axis_map_x", &pdata->axis_map_x);
	if (ret) {
		dev_err(dev, "fail to get axis_map_x\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "axis_map_y", &pdata->axis_map_y);
	if (ret) {
		dev_err(dev, "fail to get axis_map_y\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "axis_map_z", &pdata->axis_map_z);
	if (ret) {
		dev_err(dev, "fail to get axis_map_z\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "negate_x", &pdata->negate_x);
	if (ret) {
		dev_err(dev, "fail to get negate_x\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "negate_y", &pdata->negate_y);
	if (ret) {
		dev_err(dev, "fail to get negate_y\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "negate_z", &pdata->negate_z);
	if (ret) {
		dev_err(dev, "fail to get negate_z\n");
		goto fail;
	}
	return pdata;
fail:
	kfree(pdata);
	return NULL;
}
#endif
static int n2dm_acc_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{

	struct n2dm_acc_data *acc;
	struct n2dm_acc_platform_data *pdata = client->dev.platform_data;

	int err = -1;
	int tempvalue;
	struct class *gsensor_class;
	struct device *gsensor_cmd_dev;
	struct device_node *np;
	printk("sprd-gsensor: -- %s -- start !\n",__func__);
	//printk("%s: probe start.\n", N2DM_ACC_DEV_NAME);

#ifdef CONFIG_OF
	np = client->dev.of_node;
	if (np && !pdata){
		pdata = n2dm_acc_parse_dt(&client->dev);
		if (pdata) {
			client->dev.platform_data = pdata;
		}
		if (!pdata) {
			err = -ENOMEM;
			goto exit_alloc_platform_data_failed;
		}
	}
#endif
	/*
	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}
	*/
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE |
				I2C_FUNC_SMBUS_BYTE_DATA |
				I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(&client->dev, "client not smb-i2c capable:2\n");
		err = -EIO;
		goto exit_check_functionality_failed;
	}

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_I2C_BLOCK)) {
		dev_err(&client->dev, "client not smb-i2c capable:3\n");
		err = -EIO;
		goto exit_check_functionality_failed;
	}
	/*
	 * OK. From now, we presume we have a valid client. We now create the
	 * client structure, even though we cannot fill it completely yet.
	 */

	acc = kzalloc(sizeof(struct n2dm_acc_data), GFP_KERNEL);
	if (acc == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
			"failed to allocate memory for module data: %d\n", err);
		goto exit_alloc_data_failed;
	}

	mutex_init(&acc->lock);
	mutex_lock(&acc->lock);

	acc->client = client;
	n2dm_i2c_client = client;
	i2c_set_clientdata(client, acc);

	acc->irq1 = 0; /* gpio_to_irq(GSENSOR_GINT1_GPI); */
	acc->irq2 = 0; /* gpio_to_irq(GSENSOR_GINT2_GPI); */

	if (acc->irq1 != 0) {
		err = request_irq(acc->irq1, n2dm_acc_isr1,
				  IRQF_TRIGGER_RISING, "n2dm_acc_irq1", acc);
		if (err < 0) {
			dev_err(&client->dev, "request irq1 failed: %d\n", err);
			goto err_mutexunlockfreedata;
		}
		disable_irq_nosync(acc->irq1);

		INIT_WORK(&acc->irq1_work, n2dm_acc_irq1_work_func);
		acc->irq1_work_queue =
			create_singlethread_workqueue("n2dm_acc_wq1");
		if (!acc->irq1_work_queue) {
			err = -ENOMEM;
			dev_err(&client->dev, "cannot create work queue1: %d\n",
					err);
			goto err_free_irq1;
		}
	}

	if (acc->irq2 != 0) {
		err = request_irq(acc->irq2, n2dm_acc_isr2,
				  IRQF_TRIGGER_RISING, "n2dm_acc_irq2", acc);
		if (err < 0) {
			dev_err(&client->dev, "request irq2 failed: %d\n", err);
			goto err_destoyworkqueue1;
		}
		disable_irq_nosync(acc->irq2);

		/* Create workqueue for IRQ.*/

		INIT_WORK(&acc->irq2_work, n2dm_acc_irq2_work_func);
		acc->irq2_work_queue =
			create_singlethread_workqueue("n2dm_acc_wq2");
		if (!acc->irq2_work_queue) {
			err = -ENOMEM;
			dev_err(&client->dev, "cannot create work queue2: %d\n",
					err);
			goto err_free_irq2;
		}
	}

	acc->pdata = kmalloc(sizeof(*acc->pdata), GFP_KERNEL);
	if (acc->pdata == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
			"failed to allocate memory for pdata: %d\n", err);
		goto err_destoyworkqueue2;
	}

	memcpy(acc->pdata, client->dev.platform_data, sizeof(*acc->pdata));

	err = n2dm_acc_validate_pdata(acc);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto exit_kfree_pdata;
	}

	err = n2dm_acc_device_power_on(acc);
	if (err < 0) {
		dev_err(&client->dev, "power on failed: %d\n", err);
		goto err2;
	}

	if (i2c_smbus_read_byte(client) < 0) {
		pr_err("i2c_smbus_read_byte error!!\n");
		goto err_destoyworkqueue2;
	}
	/* read chip id */
	tempvalue = i2c_smbus_read_word_data(client, WHO_AM_I);
	if ((tempvalue & 0x00FF) != WHOAMI_N2DM_ACC) {
		acc->client = NULL;
		N2DM_ERR("I2C driver not registered! Device unknown 0x%x\n",
			 tempvalue);
		goto err_destoyworkqueue2;
	}

	i2c_set_clientdata(client, acc);

	if (acc->pdata->init) {
		err = acc->pdata->init();
		if (err < 0) {
			dev_err(&client->dev, "init failed: %d\n", err);
			goto err2;
		}
	}

	memset(acc->resume_state, 0, ARRAY_SIZE(acc->resume_state));

	acc->resume_state[RES_CTRL_REG1] = N2DM_ACC_ENABLE_ALL_AXES;
	acc->resume_state[RES_CTRL_REG2] = 0x00;
	acc->resume_state[RES_CTRL_REG3] = 0x00;
	acc->resume_state[RES_CTRL_REG4] = 0x00;
	acc->resume_state[RES_CTRL_REG5] = 0x00;
	acc->resume_state[RES_CTRL_REG6] = 0x00;

	acc->resume_state[RES_TEMP_CFG_REG] = 0x00;
	acc->resume_state[RES_FIFO_CTRL_REG] = 0x00;
	acc->resume_state[RES_INT_CFG1] = 0x00;
	acc->resume_state[RES_INT_THS1] = 0x00;
	acc->resume_state[RES_INT_DUR1] = 0x00;
	acc->resume_state[RES_INT_CFG2] = 0x00;
	acc->resume_state[RES_INT_THS2] = 0x00;
	acc->resume_state[RES_INT_DUR2] = 0x00;

	acc->resume_state[RES_TT_CFG] = 0x00;
	acc->resume_state[RES_TT_THS] = 0x00;
	acc->resume_state[RES_TT_LIM] = 0x00;
	acc->resume_state[RES_TT_TLAT] = 0x00;
	acc->resume_state[RES_TT_TW] = 0x00;


	atomic_set(&acc->enabled, 1);

	err = n2dm_acc_update_g_range(acc, acc->pdata->g_range);
	if (err < 0) {
		dev_err(&client->dev, "update_g_range failed\n");
		goto err_power_off;
	}

	err = n2dm_acc_update_odr(acc, acc->pdata->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto err_power_off;
	}

	err = n2dm_acc_input_init(acc);
	if (err < 0) {
		dev_err(&client->dev, "input init failed\n");
		goto err_power_off;
	}
	n2dm_acc_misc_data = acc;

	err = misc_register(&n2dm_acc_misc_device);
	if (err < 0) {
		dev_err(&client->dev,
			"misc N2DM_ACC_DEV_NAME register failed\n");
		goto err_input_cleanup;
	}
#if 1 // xr_gsensor
	gsensor_class = class_create(THIS_MODULE, "xr-gsensor");
	if (IS_ERR(gsensor_class))
		N2DM_ERR("Failed to create class(xr-gsensor)!\n");
	gsensor_cmd_dev = device_create(gsensor_class, NULL, 0, NULL, "device");
	if (IS_ERR(gsensor_cmd_dev))
		N2DM_ERR("Failed to create device(gsensor_cmd_dev)!\n");
	/* /sys/class/xr-gsensor/device/gsensor */
	if (device_create_file(gsensor_cmd_dev, &dev_attr_gsensor) < 0)
		N2DM_ERR("Failed to create device file(%s)!\n",
			 dev_attr_gsensor.attr.name);
	/* /sys/class/xr-gsensor/device/delay_acc */
	if (device_create_file(gsensor_cmd_dev, &dev_attr_delay_acc) < 0)
		N2DM_ERR("Failed to create device file(%s)!\n",
			 dev_attr_delay_acc.attr.name);
#endif

	n2dm_acc_device_power_off(acc);

	/* As default, do not report information */
	atomic_set(&acc->enabled, 0);
/*
#ifdef CONFIG_HAS_EARLYSUSPEND
	acc->early_suspend.suspend = n2dm_early_suspend;
	acc->early_suspend.resume = n2dm_early_resume;
	acc->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	register_early_suspend(&acc->early_suspend);
#endif
*/
	mutex_unlock(&acc->lock);
	dev_info(&client->dev, "###%s###\n", __func__);
	N2DM_LOG("success!\n");

#ifdef CONFIG_SPRD_PH_INFO
	memset((void *)SPRD_GsensorInfo, 0, 100);
	memcpy(SPRD_GsensorInfo, gsensor_Info, 100);
#endif

	return 0;

err_input_cleanup:
	n2dm_acc_input_cleanup(acc);
err_power_off:
	n2dm_acc_device_power_off(acc);
err2:
	if (acc->pdata->exit)
		acc->pdata->exit();
exit_kfree_pdata:
	kfree(acc->pdata);
err_destoyworkqueue2:
	if (acc->irq2_work_queue)
		destroy_workqueue(acc->irq2_work_queue);
err_free_irq2:
	if (acc->irq2) {
		free_irq(acc->irq2, acc);
		gpio_free(GSENSOR_GINT2_GPI);
	}
err_destoyworkqueue1:
	if (acc->irq1_work_queue)
		destroy_workqueue(acc->irq1_work_queue);
err_free_irq1:
	if (acc->irq1) {
		free_irq(acc->irq1, acc);
		gpio_free(GSENSOR_GINT1_GPI);
	}
err_mutexunlockfreedata:
	i2c_set_clientdata(client, NULL);
	mutex_unlock(&acc->lock);
	kfree(acc);
	n2dm_acc_misc_data = NULL;
exit_alloc_data_failed:
exit_check_functionality_failed:
	pr_err("%s: Driver Init failed\n", N2DM_ACC_DEV_NAME);
exit_alloc_platform_data_failed:
	return err;
}

static int  n2dm_acc_remove(struct i2c_client *client)
{
	/* TODO: revisit ordering here once _probe order is finalized */
	struct n2dm_acc_data *acc = i2c_get_clientdata(client);

	if (acc != NULL) {
		if (acc->irq1) {
			free_irq(acc->irq1, acc);
			gpio_free(GSENSOR_GINT1_GPI);
		}
		if (acc->irq2) {
			free_irq(acc->irq2, acc);
			gpio_free(GSENSOR_GINT2_GPI);
		}

		if (acc->irq1_work_queue)
			destroy_workqueue(acc->irq1_work_queue);
		if (acc->irq2_work_queue)
			destroy_workqueue(acc->irq2_work_queue);
		misc_deregister(&n2dm_acc_misc_device);
		n2dm_acc_input_cleanup(acc);
		n2dm_acc_device_power_off(acc);
		if (acc->pdata->exit)
			acc->pdata->exit();
		kfree(acc->pdata);
		kfree(acc);
	}

	return 0;
}
#if 0
static int n2dm_acc_resume(struct i2c_client *client)
{
	struct n2dm_acc_data *acc = i2c_get_clientdata(client);

	if (acc != NULL && acc->on_before_suspend)
		return n2dm_acc_enable(acc);

	return 0;
}

static int n2dm_acc_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct n2dm_acc_data *acc = i2c_get_clientdata(client);

	if (acc != NULL) {
		acc->on_before_suspend = atomic_read(&acc->enabled);
		return n2dm_acc_disable(acc);
	}
	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND

static void n2dm_early_suspend(struct early_suspend *es)
{
	n2dm_acc_suspend(n2dm_i2c_client, (pm_message_t) {
			   .event = 0});
}

static void n2dm_early_resume(struct early_suspend *es)
{
	n2dm_acc_resume(n2dm_i2c_client);
}

#endif /* CONFIG_HAS_EARLYSUSPEND */

static const struct i2c_device_id n2dm_acc_id[]
= { {N2DM_ACC_DEV_NAME, 0}, {}, };

MODULE_DEVICE_TABLE(i2c, n2dm_acc_id);

static const struct of_device_id n2dm_acc_of_match[] = {
	{ .compatible = "ST,n2dm_acc", },
	{ }
};
MODULE_DEVICE_TABLE(of, n2dm_acc_of_match);
static struct i2c_driver n2dm_acc_driver = {
	.driver = {
		.name = N2DM_ACC_I2C_NAME,
		.of_match_table = n2dm_acc_of_match,
	},
	.probe = n2dm_acc_probe,
	.remove = n2dm_acc_remove,
//	.resume = n2dm_acc_resume,
//	.suspend = n2dm_acc_suspend,
	.id_table = n2dm_acc_id,
};

static int __init n2dm_acc_init(void)
{
	return i2c_add_driver(&n2dm_acc_driver);
}

static void __exit n2dm_acc_exit(void)
{
	i2c_del_driver(&n2dm_acc_driver);
}

module_init(n2dm_acc_init);
module_exit(n2dm_acc_exit);

MODULE_DESCRIPTION("n2dm accelerometer misc driver");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");
